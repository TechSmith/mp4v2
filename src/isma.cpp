/*
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is MPEG4IP.
 *
 * The Initial Developer of the Original Code is Cisco Systems Inc.
 * Portions created by Cisco Systems Inc. are
 * Copyright (C) Cisco Systems Inc. 2001 - 2004.  All Rights Reserved.
 *
 * 3GPP features implementation is based on 3GPP's TS26.234-v5.60,
 * and was contributed by Ximpo Group Ltd.
 *
 * Portions created by Ximpo Group Ltd. are
 * Copyright (C) Ximpo Group Ltd. 2003, 2004.  All Rights Reserved.
 *
 * Contributor(s):
 *      Dave Mackie       dmackie@cisco.com
 *              Alix Marchandise-Franquet alix@cisco.com
 *              Ximpo Group Ltd.                mp4v2@ximpo.com
 */

#include "src/impl.h"

namespace mp4v2 {
namespace impl {

///////////////////////////////////////////////////////////////////////////////

static const uint8_t BifsV2Config[3] = {
    0x00, 0x00, 0x60 // IsCommandStream = 1, PixelMetric = 1
};

void MP4File::MakeIsmaCompliant(bool addIsmaComplianceSdp)
{
    if( !IsWriteMode() ) {
        throw new EXCEPTION("operation not permitted in read mode");
    }

    if (m_useIsma) {
        // already done
        return;
    }

    // find first audio and/or video tracks

    MP4TrackId audioTrackId = MP4_INVALID_TRACK_ID;
    try {
        audioTrackId = FindTrackId(0, MP4_AUDIO_TRACK_TYPE);
    }
    catch ( Exception *x ) {
        log.errorf(*x);
        delete x;
    }
    MP4TrackId videoTrackId = MP4_INVALID_TRACK_ID;
    try {
        videoTrackId = FindTrackId(0, MP4_VIDEO_TRACK_TYPE);
    }
    catch (Exception* x) {
        log.errorf(*x);
        delete x;
    }
    if (audioTrackId == MP4_INVALID_TRACK_ID &&
            videoTrackId == MP4_INVALID_TRACK_ID) return;

    const char *audio_media_data_name, *video_media_data_name;
    uint8_t videoProfile = 0xff;
    if (audioTrackId != MP4_INVALID_TRACK_ID) {
        audio_media_data_name = MP4GetTrackMediaDataName(this, audioTrackId);
        if (!(ATOMID(audio_media_data_name) == ATOMID("mp4a") ||
                ATOMID(audio_media_data_name) == ATOMID("enca"))) {
            log.errorf("%s: \"%s\": can't make ISMA compliant when file contains an %s track", 
                       __FUNCTION__, GetFilename().c_str(), audio_media_data_name);
            return;
        }
    }
    //
    // Note - might have to check for avc1 here...
    if (videoTrackId != MP4_INVALID_TRACK_ID) {
        video_media_data_name = MP4GetTrackMediaDataName(this, videoTrackId);
        if (!(ATOMID(video_media_data_name) == ATOMID("mp4v") ||
                ATOMID(video_media_data_name) == ATOMID("encv"))) {
            log.errorf("%s: \"%s\": can't make ISMA compliant when file contains an %s track", __FUNCTION__, 
                       GetFilename().c_str(), video_media_data_name);
            return;
        }
        MP4LogLevel verb = log.verbosity;
        log.setVerbosity(MP4_LOG_NONE);
        videoProfile = MP4GetVideoProfileLevel(this, videoTrackId);
        log.setVerbosity(verb);
    }

    m_useIsma = true;

    uint64_t fileMsDuration = 0;
    fileMsDuration =
        ConvertFromMovieDuration(GetDuration(), MP4_MSECS_TIME_SCALE);

    // delete any existing OD track
    if (m_odTrackId != MP4_INVALID_TRACK_ID) {
        DeleteTrack(m_odTrackId);
    }

    if (m_pRootAtom->FindAtom("moov.iods") == NULL) {
        (void)AddChildAtom("moov", "iods");
    }
    (void)AddODTrack();
    SetODProfileLevel(0xFF);

    if (audioTrackId != MP4_INVALID_TRACK_ID) {
        AddTrackToOd(audioTrackId);
        MP4SetAudioProfileLevel(this, 0xf);
    }
    if (videoTrackId != MP4_INVALID_TRACK_ID) {
        AddTrackToOd(videoTrackId);
        MP4SetVideoProfileLevel(this, videoProfile);
    }

    // delete any existing scene track
    MP4TrackId sceneTrackId = MP4_INVALID_TRACK_ID;
    try {
        sceneTrackId = FindTrackId(0, MP4_SCENE_TRACK_TYPE);
    }
    catch (Exception *x) {
        log.errorf(*x);
        delete x;
    }
    if (sceneTrackId != MP4_INVALID_TRACK_ID) {
        DeleteTrack(sceneTrackId);
    }

    // add scene track
    sceneTrackId = AddSceneTrack();
    SetSceneProfileLevel(0xFF);
    SetGraphicsProfileLevel(0xFF);
    SetTrackIntegerProperty(sceneTrackId,
                            "mdia.minf.stbl.stsd.mp4s.esds.decConfigDescr.objectTypeId",
                            MP4SystemsV2ObjectType);

    SetTrackESConfiguration(sceneTrackId,
                            BifsV2Config, sizeof(BifsV2Config));

    uint8_t* pBytes = NULL;
    uint64_t numBytes = 0;

    // write OD Update Command
    CreateIsmaODUpdateCommandFromFileForFile(
        m_odTrackId,
        audioTrackId,
        videoTrackId,
        &pBytes,
        &numBytes);

    WriteSample(m_odTrackId, pBytes, numBytes, fileMsDuration);

    MP4Free(pBytes);
    pBytes = NULL;

    // write BIFS Scene Replace Command
    CreateIsmaSceneCommand(
        MP4_IS_VALID_TRACK_ID(audioTrackId),
        MP4_IS_VALID_TRACK_ID(videoTrackId),
        &pBytes,
        &numBytes);

    WriteSample(sceneTrackId, pBytes, numBytes, fileMsDuration);

    MP4Free(pBytes);
    pBytes = NULL;

    // add session level sdp
    CreateIsmaIodFromFile(
        m_odTrackId,
        sceneTrackId,
        audioTrackId,
        videoTrackId,
        &pBytes,
        &numBytes);

    char* iodBase64 = MP4ToBase64(pBytes, numBytes);

    uint32_t sdpBufLen = (uint32_t)strlen(iodBase64) + 256;
    uint32_t used;
    char* sdpBuf = (char*)MP4Calloc(sdpBufLen);

    if (addIsmaComplianceSdp) {
        strncpy(sdpBuf, "a=isma-compliance:1,1.0,1\015\012", sdpBufLen);
    }

    used = (uint32_t)strlen(sdpBuf);
    sdpBufLen -= used;
    snprintf(&sdpBuf[used], sdpBufLen,
             "a=mpeg4-iod: \042data:application/mpeg4-iod;base64,%s\042\015\012",
             iodBase64);

    SetSessionSdp(sdpBuf);

    log.verbose1f("\"%s\": IOD SDP = %s", GetFilename().c_str(), sdpBuf);

    MP4Free(iodBase64);
    iodBase64 = NULL;
    MP4Free(pBytes);
    pBytes = NULL;
    MP4Free(sdpBuf);
    sdpBuf = NULL;
}

static void CloneIntegerProperty(
    MP4Descriptor* pDest,
    MP4DescriptorProperty* pSrc,
    const char* name)
{
    MP4IntegerProperty* pGetProperty;
    MP4IntegerProperty* pSetProperty;

    if (!pSrc->FindProperty(name, (MP4Property**)&pGetProperty)) return;
    if (!pDest->FindProperty(name, (MP4Property**)&pSetProperty)) return;
    pSetProperty->SetValue(pGetProperty->GetValue());
}

void MP4File::CreateIsmaIodFromFile(
    MP4TrackId odTrackId,
    MP4TrackId sceneTrackId,
    MP4TrackId audioTrackId,
    MP4TrackId videoTrackId,
    uint8_t** ppBytes,
    uint64_t* pNumBytes)
{
    MP4Atom* pIodsAtom = FindAtom("moov.iods");
    ASSERT(pIodsAtom);
    MP4DescriptorProperty* pSrcIod =
        (MP4DescriptorProperty*)pIodsAtom->GetProperty(2);

    MP4Descriptor* pIod = new MP4IODescriptor(*pIodsAtom);
    pIod->SetTag(MP4IODescrTag);
    pIod->Generate();

    CloneIntegerProperty(pIod, pSrcIod, "objectDescriptorId");
    CloneIntegerProperty(pIod, pSrcIod, "ODProfileLevelId");
    CloneIntegerProperty(pIod, pSrcIod, "sceneProfileLevelId");
    CloneIntegerProperty(pIod, pSrcIod, "audioProfileLevelId");
    CloneIntegerProperty(pIod, pSrcIod, "visualProfileLevelId");
    CloneIntegerProperty(pIod, pSrcIod, "graphicsProfileLevelId");

    // mutate esIds from MP4ESIDIncDescrTag to MP4ESDescrTag
    MP4DescriptorProperty* pEsProperty;
    if (!pIod->FindProperty("esIds", (MP4Property**)&pEsProperty)) return;
    pEsProperty->SetTags(MP4ESDescrTag);

    MP4IntegerProperty* pSetProperty;
    MP4IntegerProperty* pSceneESID;
    MP4IntegerProperty* pOdESID;

    // OD
    MP4Descriptor* pOdEsd =
        pEsProperty->AddDescriptor(MP4ESDescrTag);
    pOdEsd->Generate();

    if (!pOdEsd->FindProperty("ESID", (MP4Property**)&pOdESID)) return;

    // we set the OD ESID to a non-zero unique value
    pOdESID->SetValue(m_odTrackId);

    if (pOdEsd->FindProperty("URLFlag",
                             (MP4Property**)&pSetProperty))
        pSetProperty->SetValue(1);

    uint8_t* pBytes;
    uint64_t numBytes;

    CreateIsmaODUpdateCommandFromFileForStream(
        audioTrackId,
        videoTrackId,
        &pBytes,
        &numBytes);

    log.hexDump(0, MP4_LOG_VERBOSE1, pBytes, numBytes, "\"%s\": OD data",
                GetFilename().c_str() );

    char* odCmdBase64 = MP4ToBase64(pBytes, numBytes);

    uint32_t urlBufLen = (uint32_t)strlen(odCmdBase64) + 64;
    char* urlBuf = (char*)MP4Malloc(urlBufLen);

    snprintf(urlBuf, urlBufLen,
             "data:application/mpeg4-od-au;base64,%s",
             odCmdBase64);

    MP4StringProperty* pUrlProperty;
    if (pOdEsd->FindProperty("URL",
                             (MP4Property**)&pUrlProperty))
        pUrlProperty->SetValue(urlBuf);

    log.verbose1f("\"%s\": OD data URL = \042%s\042", GetFilename().c_str(),
                  urlBuf);

    MP4Free(odCmdBase64);
    odCmdBase64 = NULL;
    MP4Free(pBytes);
    pBytes = NULL;
    MP4Free(urlBuf);
    urlBuf = NULL;

    MP4DescriptorProperty* pSrcDcd = NULL;

    // HACK temporarily point to scene decoder config
    (void)FindProperty(MakeTrackName(odTrackId,
                                     "mdia.minf.stbl.stsd.mp4s.esds.decConfigDescr"),
                       (MP4Property**)&pSrcDcd);
    ASSERT(pSrcDcd);
    MP4Property* pOrgOdEsdProperty =
        pOdEsd->GetProperty(8);
    pOdEsd->SetProperty(8, pSrcDcd);

    // bufferSizeDB needs to be set appropriately
    MP4BitfieldProperty* pBufferSizeProperty = NULL;
    if (pOdEsd->FindProperty("decConfigDescr.bufferSizeDB",
                             (MP4Property**)&pBufferSizeProperty)) {
        ASSERT(pBufferSizeProperty);
        pBufferSizeProperty->SetValue(numBytes);
    }

    // SL config needs to change from 2 (file) to 1 (null)
    if (pOdEsd->FindProperty("slConfigDescr.predefined",
                             (MP4Property**)&pSetProperty))
        pSetProperty->SetValue(1);


    // Scene
    MP4Descriptor* pSceneEsd =
        pEsProperty->AddDescriptor(MP4ESDescrTag);
    pSceneEsd->Generate();

    if (pSceneEsd->FindProperty("ESID",
                                (MP4Property**)&pSceneESID)) {
        // we set the Scene ESID to a non-zero unique value
        pSceneESID->SetValue(sceneTrackId);
    }

    if (pSceneEsd->FindProperty("URLFlag",
                                (MP4Property**)&pSetProperty))
        pSetProperty->SetValue(1);

    CreateIsmaSceneCommand(
        MP4_IS_VALID_TRACK_ID(audioTrackId),
        MP4_IS_VALID_TRACK_ID(videoTrackId),
        &pBytes,
        &numBytes);

    log.hexDump(0, MP4_LOG_VERBOSE1, pBytes, numBytes, "\"%s\": Scene data",
                GetFilename().c_str() );

    char *sceneCmdBase64 = MP4ToBase64(pBytes, numBytes);

    urlBuf = (char*)MP4Malloc(strlen(sceneCmdBase64) + 64);
    snprintf(urlBuf, strlen(sceneCmdBase64) + 64,
             "data:application/mpeg4-bifs-au;base64,%s",
             sceneCmdBase64);

    if (pSceneEsd->FindProperty("URL",
                                (MP4Property**)&pUrlProperty))
        pUrlProperty->SetValue(urlBuf);

    log.verbose1f("\"%s\": Scene data URL = \042%s\042",
                  GetFilename().c_str(), urlBuf);

    MP4Free(sceneCmdBase64);
    sceneCmdBase64 = NULL;
    MP4Free(urlBuf);
    urlBuf = NULL;
    MP4Free(pBytes);
    pBytes = NULL;

    // HACK temporarily point to scene decoder config
    ASSERT(FindProperty(MakeTrackName(sceneTrackId,
                                      "mdia.minf.stbl.stsd.mp4s.esds.decConfigDescr"),
                        (MP4Property**)&pSrcDcd));
    ASSERT(pSrcDcd);
    MP4Property* pOrgSceneEsdProperty =
        pSceneEsd->GetProperty(8);
    pSceneEsd->SetProperty(8, pSrcDcd);

    // bufferSizeDB needs to be set
    pBufferSizeProperty = NULL;
    if (pSceneEsd->FindProperty("decConfigDescr.bufferSizeDB",
                                (MP4Property**)&pBufferSizeProperty)) {
        ASSERT(pBufferSizeProperty);
        pBufferSizeProperty->SetValue(numBytes);
    }

    // SL config needs to change from 2 (file) to 1 (null)
    if (pSceneEsd->FindProperty("slConfigDescr.predefined",
                                (MP4Property**)&pSetProperty))
        pSetProperty->SetValue(1);


    // finally get the whole thing written to a memory
    pIod->WriteToMemory(*this, ppBytes, pNumBytes);


    // now carefully replace esd properties before destroying
    pOdEsd->SetProperty(8, pOrgOdEsdProperty);
    pSceneEsd->SetProperty(8, pOrgSceneEsdProperty);
    pSceneESID->SetValue(0); // restore 0 value
    pOdESID->SetValue(0);

    delete pIod;

    log.hexDump(0, MP4_LOG_VERBOSE1, *ppBytes, *pNumBytes, "\"%s\": IOD data",
                GetFilename().c_str() );
}

void MP4File::CreateIsmaIodFromParams(
    uint8_t videoProfile,
    uint32_t videoBitrate,
    uint8_t* videoConfig,
    uint32_t videoConfigLength,
    uint8_t audioProfile,
    uint32_t audioBitrate,
    uint8_t* audioConfig,
    uint32_t audioConfigLength,
    uint8_t** ppIodBytes,
    uint64_t* pIodNumBytes)
{
    MP4IntegerProperty* pInt;
    uint8_t* pBytes = NULL;
    uint64_t numBytes;

    // Descriptor constructors need a parent atom.  In this
    // case we don't have one so create a dummy one.  This
    // is OK as we only build the descriptor and then dump
    // its contents to a buffer
    MP4Atom     dummyParent(*this);

    // Create the IOD
    MP4Descriptor* pIod = new MP4IODescriptor(dummyParent);
    pIod->SetTag(MP4IODescrTag);
    pIod->Generate();

    // Set audio and video profileLevels
    if (pIod->FindProperty("audioProfileLevelId",
                           (MP4Property**)&pInt))
        pInt->SetValue(audioProfile);

    if (pIod->FindProperty("visualProfileLevelId",
                           (MP4Property**)&pInt))
        pInt->SetValue(videoProfile);

    // Mutate esIds from MP4ESIDIncDescrTag to MP4ESDescrTag
    MP4DescriptorProperty* pEsProperty;
    if (!pIod->FindProperty("esIds", (MP4Property**)&pEsProperty)) return;
    pEsProperty->SetTags(MP4ESDescrTag);

    // Add ES Descriptors

    // Scene
    CreateIsmaSceneCommand(
        (audioProfile != 0xFF),
        (videoProfile != 0xFF),
        &pBytes,
        &numBytes);

    log.hexDump(0, MP4_LOG_VERBOSE1, pBytes, numBytes, "\"%s\": Scene data",
                GetFilename().c_str() );

    char* sceneCmdBase64 = MP4ToBase64(pBytes, numBytes);

    char* urlBuf =
        (char*)MP4Malloc(strlen(sceneCmdBase64) + 64);
    snprintf(urlBuf, strlen(sceneCmdBase64) + 64,
             "data:application/mpeg4-bifs-au;base64,%s",
             sceneCmdBase64);

    log.verbose1f("\"%s\": Scene data URL = \042%s\042", GetFilename().c_str(),
                  urlBuf);

    /* MP4Descriptor* pSceneEsd = */
    CreateESD(
        pEsProperty,
        201,                // esid
        MP4SystemsV2ObjectType,
        MP4SceneDescriptionStreamType,
        numBytes,           // bufferSize
        numBytes * 8,       // bitrate
        BifsV2Config,
        sizeof(BifsV2Config),
        urlBuf);
    MP4Free(urlBuf);
    urlBuf = NULL;

    MP4Free(sceneCmdBase64);
    sceneCmdBase64 = NULL;
    MP4Free(pBytes);
    pBytes = NULL;

    // OD

    // Video
    MP4DescriptorProperty* pVideoEsdProperty =
        new MP4DescriptorProperty(dummyParent);
    pVideoEsdProperty->SetTags(MP4ESDescrTag);

    /* MP4Descriptor* pVideoEsd = */
    CreateESD(
        pVideoEsdProperty,
        20,                 // esid
        MP4_MPEG4_VIDEO_TYPE,
        MP4VisualStreamType,
        videoBitrate / 8,   // bufferSize
        videoBitrate,
        videoConfig,
        videoConfigLength,
        NULL);

    // Audio
    MP4DescriptorProperty* pAudioEsdProperty =
        new MP4DescriptorProperty(dummyParent);
    pAudioEsdProperty->SetTags(MP4ESDescrTag);

    /* MP4Descriptor* pAudioEsd = */
    CreateESD(
        pAudioEsdProperty,
        10,                 // esid
        MP4_MPEG4_AUDIO_TYPE,
        MP4AudioStreamType,
        audioBitrate / 8,   // bufferSize
        audioBitrate,
        audioConfig,
        audioConfigLength,
        NULL);

    CreateIsmaODUpdateCommandForStream(
        pAudioEsdProperty,
        pVideoEsdProperty,
        &pBytes,
        &numBytes);

    // cleanup temporary descriptor properties
    delete pAudioEsdProperty;
    delete pVideoEsdProperty;

    log.hexDump(0, MP4_LOG_VERBOSE1,pBytes, numBytes,"\"%s\": OD data = %" PRIu64 " bytes",
                GetFilename().c_str(), numBytes);

    char* odCmdBase64 = MP4ToBase64(pBytes, numBytes);

    urlBuf = (char*)MP4Malloc(strlen(odCmdBase64) + 64);
    if (urlBuf != NULL) {
        snprintf(urlBuf, strlen(odCmdBase64) + 64,
                 "data:application/mpeg4-od-au;base64,%s",
                 odCmdBase64);

        log.verbose1f("\"%s\": OD data URL = \042%s\042", GetFilename().c_str(), urlBuf);

        /* MP4Descriptor* pOdEsd = */
        CreateESD(
            pEsProperty,
            101,
            MP4SystemsV1ObjectType,
            MP4ObjectDescriptionStreamType,
            numBytes,       // bufferSize
            numBytes * 8,   // bitrate
            NULL,           // config
            0,              // configLength
            urlBuf);

        MP4Free(urlBuf);
        urlBuf = NULL;
    }
    MP4Free(odCmdBase64);
    odCmdBase64 = NULL;
    MP4Free(pBytes);
    pBytes = NULL;

    // finally get the whole thing written to a memory
    pIod->WriteToMemory(*this, ppIodBytes, pIodNumBytes);

    delete pIod;

    log.hexDump(0, MP4_LOG_VERBOSE1, *ppIodBytes, *pIodNumBytes,"\"%s\": IOD data",
                GetFilename().c_str() );
}

void MP4File::CreateESD(
    MP4DescriptorProperty* pEsProperty,
    uint32_t esid,
    uint8_t objectType,
    uint8_t streamType,
    uint32_t bufferSize,
    uint32_t bitrate,
    const uint8_t* pConfig,
    uint32_t configLength,
    char* url)
{
    MP4IntegerProperty* pInt;
    MP4StringProperty* pString;
    MP4BytesProperty* pBytes;
    MP4BitfieldProperty* pBits;

    MP4Descriptor* pEsd =
        pEsProperty->AddDescriptor(MP4ESDescrTag);
    pEsd->Generate();

    if (pEsd->FindProperty("ESID",
                           (MP4Property**)&pInt))
        pInt->SetValue(esid);

    if (pEsd->FindProperty("decConfigDescr.objectTypeId",
                           (MP4Property**)&pInt))
        pInt->SetValue(objectType);

    if (pEsd->FindProperty("decConfigDescr.streamType",
                           (MP4Property**)&pInt))
        pInt->SetValue(streamType);

    if (pEsd->FindProperty("decConfigDescr.bufferSizeDB",
                           (MP4Property**)&pInt))
        pInt->SetValue(bufferSize);

    if (pEsd->FindProperty("decConfigDescr.maxBitrate",
                           (MP4Property**)&pInt))
        pInt->SetValue(bitrate);

    if (pEsd->FindProperty("decConfigDescr.avgBitrate",
                           (MP4Property**)&pInt))
        pInt->SetValue(bitrate);

    MP4DescriptorProperty* pConfigDescrProperty;
    if (pEsd->FindProperty("decConfigDescr.decSpecificInfo",
                           (MP4Property**)&pConfigDescrProperty)) {

        MP4Descriptor* pConfigDescr =
            pConfigDescrProperty->AddDescriptor(MP4DecSpecificDescrTag);
        pConfigDescr->Generate();

        if (pConfigDescrProperty->FindProperty("decSpecificInfo[0].info",
                                               (MP4Property**)&pBytes))
            pBytes->SetValue(pConfig, configLength);
    }

    if (pEsd->FindProperty("slConfigDescr.predefined",
                           (MP4Property**)&pInt))
        // changed 12/5/02 from plugfest to value 0
        pInt->SetValue(0);

    if (pEsd->FindProperty("slConfig.useAccessUnitEndFlag",
                           (MP4Property **)&pBits))
        pBits->SetValue(1);

    if (url) {
        if (pEsd->FindProperty("URLFlag",
                               (MP4Property**)&pInt))
            pInt->SetValue(1);

        if (pEsd->FindProperty("URL",
                               (MP4Property**)&pString))
            pString->SetValue(url);
    }

    //return pEsd;
}

void MP4File::CreateIsmaODUpdateCommandFromFileForFile(
    MP4TrackId odTrackId,
    MP4TrackId audioTrackId,
    MP4TrackId videoTrackId,
    uint8_t** ppBytes,
    uint64_t* pNumBytes)
{
    // Descriptor constructors need a parent atom.  In this
    // case we don't have one so create a dummy one.  This
    // is OK as we only build the descriptor and then dump
    // its contents to a buffer
    MP4Atom     dummyParent(*this);
    MP4Descriptor* pCommand = CreateODCommand(dummyParent, MP4ODUpdateODCommandTag);
    pCommand->Generate();

    for (uint8_t i = 0; i < 2; i++) {
        MP4TrackId trackId;
        uint16_t odId;

        if (i == 0) {
            trackId = audioTrackId;
            odId = 10;
        } else {
            trackId = videoTrackId;
            odId = 20;
        }

        if (trackId == MP4_INVALID_TRACK_ID) {
            continue;
        }

        MP4DescriptorProperty* pOdDescrProperty =
            (MP4DescriptorProperty*)(pCommand->GetProperty(0));

        pOdDescrProperty->SetTags(MP4FileODescrTag);

        MP4Descriptor* pOd =
            pOdDescrProperty->AddDescriptor(MP4FileODescrTag);

        pOd->Generate();

        MP4BitfieldProperty* pOdIdProperty = NULL;
        if (pOd->FindProperty("objectDescriptorId",
                              (MP4Property**)&pOdIdProperty))
            pOdIdProperty->SetValue(odId);

        MP4DescriptorProperty* pEsIdsDescriptorProperty = NULL;
        ASSERT(pOd->FindProperty("esIds",
                                 (MP4Property**)&pEsIdsDescriptorProperty));
        ASSERT(pEsIdsDescriptorProperty);

        pEsIdsDescriptorProperty->SetTags(MP4ESIDRefDescrTag);

        MP4Descriptor *pRefDescriptor =
            pEsIdsDescriptorProperty->AddDescriptor(MP4ESIDRefDescrTag);
        pRefDescriptor->Generate();

        MP4Integer16Property* pRefIndexProperty = NULL;
        ASSERT(pRefDescriptor->FindProperty("refIndex",
                                            (MP4Property**)&pRefIndexProperty));
        ASSERT(pRefIndexProperty);

        uint32_t mpodIndex = FindTrackReference(
                                 MakeTrackName(odTrackId, "tref.mpod"), trackId);
        ASSERT(mpodIndex != 0);

        pRefIndexProperty->SetValue(mpodIndex);
    }

    pCommand->WriteToMemory(*this, ppBytes, pNumBytes);

    delete pCommand;
}

void MP4File::CreateIsmaODUpdateCommandFromFileForStream(
    MP4TrackId audioTrackId,
    MP4TrackId videoTrackId,
    uint8_t** ppBytes,
    uint64_t* pNumBytes)
{
    MP4DescriptorProperty* pAudioEsd = NULL;
    MP4Integer8Property* pAudioSLConfigPredef = NULL;
    MP4BitfieldProperty* pAudioAccessUnitEndFlag = NULL;
    int oldAudioUnitEndFlagValue = 0;
    MP4DescriptorProperty* pVideoEsd = NULL;
    MP4Integer8Property* pVideoSLConfigPredef = NULL;
    MP4BitfieldProperty* pVideoAccessUnitEndFlag = NULL;
    int oldVideoUnitEndFlagValue = 0;
    MP4IntegerProperty* pAudioEsdId = NULL;
    MP4IntegerProperty* pVideoEsdId = NULL;

    if (audioTrackId != MP4_INVALID_TRACK_ID) {
        // changed mp4a to * to handle enca case
        MP4Atom* pEsdsAtom =
            FindAtom(MakeTrackName(audioTrackId,
                                   "mdia.minf.stbl.stsd.*.esds"));
        ASSERT(pEsdsAtom);

        pAudioEsd = (MP4DescriptorProperty*)(pEsdsAtom->GetProperty(2));
        // ESID is 0 for file, stream needs to be non-ze
        ASSERT(pAudioEsd->FindProperty("ESID",
                                       (MP4Property**)&pAudioEsdId));

        ASSERT(pAudioEsdId);
        pAudioEsdId->SetValue(audioTrackId);

        // SL config needs to change from 2 (file) to 1 (null)
        if (pAudioEsd->FindProperty("slConfigDescr.predefined",
                                    (MP4Property**)&pAudioSLConfigPredef)) {
            ASSERT(pAudioSLConfigPredef);
            pAudioSLConfigPredef->SetValue(0);
        }

        if (pAudioEsd->FindProperty("slConfigDescr.useAccessUnitEndFlag",
                                    (MP4Property **)&pAudioAccessUnitEndFlag)) {
            oldAudioUnitEndFlagValue =
                pAudioAccessUnitEndFlag->GetValue();
            pAudioAccessUnitEndFlag->SetValue(1);
        }
    }

    if (videoTrackId != MP4_INVALID_TRACK_ID) {
        // changed mp4v to * to handle encv case
        MP4Atom* pEsdsAtom =
            FindAtom(MakeTrackName(videoTrackId,
                                   "mdia.minf.stbl.stsd.*.esds"));
        ASSERT(pEsdsAtom);

        pVideoEsd = (MP4DescriptorProperty*)(pEsdsAtom->GetProperty(2));
        ASSERT(pVideoEsd->FindProperty("ESID",
                                       (MP4Property**)&pVideoEsdId));

        ASSERT(pVideoEsdId);
        pVideoEsdId->SetValue(videoTrackId);

        // SL config needs to change from 2 (file) to 1 (null)
        ASSERT(pVideoEsd->FindProperty("slConfigDescr.predefined",
                                       (MP4Property **)&pVideoSLConfigPredef));
        ASSERT(pVideoSLConfigPredef);
        pVideoSLConfigPredef->SetValue(0);

        if (pVideoEsd->FindProperty("slConfigDescr.useAccessUnitEndFlag",
                                    (MP4Property **)&pVideoAccessUnitEndFlag)) {
            oldVideoUnitEndFlagValue =
                pVideoAccessUnitEndFlag->GetValue();
            pVideoAccessUnitEndFlag->SetValue(1);
        }
    }

    CreateIsmaODUpdateCommandForStream(
        pAudioEsd, pVideoEsd, ppBytes, pNumBytes);
    log.hexDump(0, MP4_LOG_VERBOSE1, *ppBytes, *pNumBytes,
                "\"%s\": After CreateImsaODUpdateCommandForStream len %" PRIu64,
                GetFilename().c_str(), *pNumBytes);

    // return SL config values to 2 (file)
    // return ESID values to 0
    if (pAudioSLConfigPredef) {
        pAudioSLConfigPredef->SetValue(2);
    }
    if (pAudioEsdId) {
        pAudioEsdId->SetValue(0);
    }
    if (pAudioAccessUnitEndFlag) {
        pAudioAccessUnitEndFlag->SetValue(oldAudioUnitEndFlagValue );
    }
    if (pVideoEsdId) {
        pVideoEsdId->SetValue(0);
    }
    if (pVideoSLConfigPredef) {
        pVideoSLConfigPredef->SetValue(2);
    }
    if (pVideoAccessUnitEndFlag) {
        pVideoAccessUnitEndFlag->SetValue(oldVideoUnitEndFlagValue );
    }
}

void MP4File::CreateIsmaODUpdateCommandForStream(
    MP4DescriptorProperty* pAudioEsdProperty,
    MP4DescriptorProperty* pVideoEsdProperty,
    uint8_t** ppBytes,
    uint64_t* pNumBytes)
{
    MP4Descriptor* pAudioOd = NULL;
    MP4Descriptor* pVideoOd = NULL;

    // Descriptor constructors need a parent atom.  In this
    // case we don't have one so create a dummy one.  This
    // is OK as we only build the descriptor and then dump
    // its contents to a buffer
    MP4Atom     dummyParent(*this);
    MP4Descriptor* pCommand =
        CreateODCommand(dummyParent, MP4ODUpdateODCommandTag);
    pCommand->Generate();

    for (uint8_t i = 0; i < 2; i++) {
        uint16_t odId;
        MP4DescriptorProperty* pEsdProperty = NULL;

        if (i == 0) {
            odId = 10;
            pEsdProperty = pAudioEsdProperty;
        } else {
            odId = 20;
            pEsdProperty = pVideoEsdProperty;
        }

        if (pEsdProperty == NULL) {
            continue;
        }

        MP4DescriptorProperty* pOdDescrProperty =
            (MP4DescriptorProperty*)(pCommand->GetProperty(0));

        pOdDescrProperty->SetTags(MP4ODescrTag);

        MP4Descriptor* pOd =
            pOdDescrProperty->AddDescriptor(MP4ODescrTag);
        pOd->Generate();

        if (i == 0) {
            pAudioOd = pOd;
        } else {
            pVideoOd = pOd;
        }

        MP4BitfieldProperty* pOdIdProperty = NULL;
        if (pOd->FindProperty("objectDescriptorId",
                              (MP4Property**)&pOdIdProperty)) {
            pOdIdProperty->SetValue(odId);
        }

        delete (MP4DescriptorProperty*)pOd->GetProperty(4);
        pOd->SetProperty(4, pEsdProperty);
    }

    // serialize OD command
    pCommand->WriteToMemory(*this, ppBytes, pNumBytes);

    // detach from esd descriptor params
    if (pAudioOd) {
        pAudioOd->SetProperty(4, NULL);
    }
    if (pVideoOd) {
        pVideoOd->SetProperty(4, NULL);
    }

    // then destroy
    delete pCommand;
}

void MP4File::CreateIsmaSceneCommand(
    bool hasAudio,
    bool hasVideo,
    uint8_t** ppBytes,
    uint64_t* pNumBytes)
{
    // from ISMA 1.0 Tech Spec Appendix E
    static const uint8_t bifsAudioOnly[] = {
        0xC0, 0x10, 0x12,
        0x81, 0x30, 0x2A, 0x05, 0x6D, 0xC0
    };
    static const uint8_t bifsVideoOnly[] = {
        0xC0, 0x10, 0x12,
        0x61, 0x04,
        0x1F, 0xC0, 0x00, 0x00,
        0x1F, 0xC0, 0x00, 0x00,
        0x44, 0x28, 0x22, 0x82, 0x9F, 0x80
    };
    static const uint8_t bifsAudioVideo[] = {
        0xC0, 0x10, 0x12,
        0x81, 0x30, 0x2A, 0x05, 0x6D, 0x26,
        0x10, 0x41, 0xFC, 0x00, 0x00, 0x01, 0xFC, 0x00, 0x00,
        0x04, 0x42, 0x82, 0x28, 0x29, 0xF8
    };

    if (hasAudio && hasVideo) {
        *pNumBytes = sizeof(bifsAudioVideo);
        *ppBytes = (uint8_t*)MP4Malloc(*pNumBytes);
        memcpy(*ppBytes, bifsAudioVideo, sizeof(bifsAudioVideo));

    } else if (hasAudio) {
        *pNumBytes = sizeof(bifsAudioOnly);
        *ppBytes = (uint8_t*)MP4Malloc(*pNumBytes);
        memcpy(*ppBytes, bifsAudioOnly, sizeof(bifsAudioOnly));

    } else if (hasVideo) {
        *pNumBytes = sizeof(bifsVideoOnly);
        *ppBytes = (uint8_t*)MP4Malloc(*pNumBytes);
        memcpy(*ppBytes, bifsVideoOnly, sizeof(bifsVideoOnly));
    } else {
        *pNumBytes = 0;
        *ppBytes = NULL;
    }
}

///////////////////////////////////////////////////////////////////////////////

}
} // namespace mp4v2::impl

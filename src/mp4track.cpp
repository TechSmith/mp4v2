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
 *      Dave Mackie         dmackie@cisco.com
 *      Alix Marchandise-Franquet   alix@cisco.com
 *              Ximpo Group Ltd.                mp4v2@ximpo.com
 */

#include "src/impl.h"

namespace mp4v2 { namespace impl {

///////////////////////////////////////////////////////////////////////////////

#define AMR_UNINITIALIZED -1
#define AMR_TRUE 0
#define AMR_FALSE 1

MP4Track::MP4Track(MP4File& file, MP4Atom& trakAtom)
    : m_File(file)
    , m_trakAtom(trakAtom)
{
    m_lastStsdIndex = 0;
    m_lastSampleFile = NULL;

    m_cachedReadSampleId = MP4_INVALID_SAMPLE_ID;
    m_pCachedReadSample = NULL;
    m_cachedReadSampleSize = 0;

    m_writeSampleId = 1;
    m_fixedSampleDuration = 0;
    m_pChunkBuffer = NULL;
    m_chunkBufferSize = 0;
    m_sizeOfDataInChunkBuffer = 0;
    m_chunkSamples = 0;
    m_chunkDuration = 0;

    // m_bytesPerSample should be set to 1, except for the
    // quicktime audio constant bit rate samples, which have non-1 values
    m_bytesPerSample = 1;
    m_samplesPerChunk = 0;
    m_durationPerChunk = 0;
    m_isAmr = AMR_UNINITIALIZED;
    m_curMode = 0;

    m_cachedSttsSid = MP4_INVALID_SAMPLE_ID;
    m_cachedCttsSid = MP4_INVALID_SAMPLE_ID;

    bool success = true;

    MP4Integer32Property* pTrackIdProperty;
    success &= m_trakAtom.FindProperty(
                   "trak.tkhd.trackId",
                   (MP4Property**)&pTrackIdProperty);
    if (success) {
        m_trackId = pTrackIdProperty->GetValue();
    }

    success &= m_trakAtom.FindProperty(
                   "trak.mdia.mdhd.timeScale",
                   (MP4Property**)&m_pTimeScaleProperty);
    if (success) {
        // default chunking is 1 second of samples
        m_durationPerChunk = m_pTimeScaleProperty->GetValue();
    }

    success &= m_trakAtom.FindProperty(
                   "trak.tkhd.duration",
                   (MP4Property**)&m_pTrackDurationProperty);

    success &= m_trakAtom.FindProperty(
                   "trak.mdia.mdhd.duration",
                   (MP4Property**)&m_pMediaDurationProperty);

    success &= m_trakAtom.FindProperty(
                   "trak.tkhd.modificationTime",
                   (MP4Property**)&m_pTrackModificationProperty);

    success &= m_trakAtom.FindProperty(
                   "trak.mdia.mdhd.modificationTime",
                   (MP4Property**)&m_pMediaModificationProperty);

    success &= m_trakAtom.FindProperty(
                   "trak.mdia.hdlr.handlerType",
                   (MP4Property**)&m_pTypeProperty);

    // get handles on sample size information


    m_pStszFixedSampleSizeProperty = NULL;
    bool have_stsz =
        m_trakAtom.FindProperty("trak.mdia.minf.stbl.stsz.sampleSize",
                                  (MP4Property**)&m_pStszFixedSampleSizeProperty);

    if (have_stsz) {
        success &= m_trakAtom.FindProperty(
                       "trak.mdia.minf.stbl.stsz.sampleCount",
                       (MP4Property**)&m_pStszSampleCountProperty);

        success &= m_trakAtom.FindProperty(
                       "trak.mdia.minf.stbl.stsz.entries.entrySize",
                       (MP4Property**)&m_pStszSampleSizeProperty);
        m_stsz_sample_bits = 32;
    } else {
        success &= m_trakAtom.FindProperty(
                       "trak.mdia.minf.stbl.stz2.sampleCount",
                       (MP4Property**)&m_pStszSampleCountProperty);
        success &= m_trakAtom.FindProperty(
                       "trak.mdia.minf.stbl.stz2.entries.entrySize",
                       (MP4Property**)&m_pStszSampleSizeProperty);
        MP4Integer8Property *stz2_field_size;
        if (m_trakAtom.FindProperty(
                    "trak.mdia.minf.stbl.stz2.fieldSize",
                    (MP4Property **)&stz2_field_size)) {
            m_stsz_sample_bits = stz2_field_size->GetValue();
            m_have_stz2_4bit_sample = false;
        } else success = false;
    }

    // get handles on information needed to map sample id's to file offsets

    success &= m_trakAtom.FindProperty(
                   "trak.mdia.minf.stbl.stsc.entryCount",
                   (MP4Property**)&m_pStscCountProperty);

    success &= m_trakAtom.FindProperty(
                   "trak.mdia.minf.stbl.stsc.entries.firstChunk",
                   (MP4Property**)&m_pStscFirstChunkProperty);

    success &= m_trakAtom.FindProperty(
                   "trak.mdia.minf.stbl.stsc.entries.samplesPerChunk",
                   (MP4Property**)&m_pStscSamplesPerChunkProperty);

    success &= m_trakAtom.FindProperty(
                   "trak.mdia.minf.stbl.stsc.entries.sampleDescriptionIndex",
                   (MP4Property**)&m_pStscSampleDescrIndexProperty);

    success &= m_trakAtom.FindProperty(
                   "trak.mdia.minf.stbl.stsc.entries.firstSample",
                   (MP4Property**)&m_pStscFirstSampleProperty);

    bool haveStco = m_trakAtom.FindProperty(
                        "trak.mdia.minf.stbl.stco.entryCount",
                        (MP4Property**)&m_pChunkCountProperty);

    if (haveStco) {
        success &= m_trakAtom.FindProperty(
                       "trak.mdia.minf.stbl.stco.entries.chunkOffset",
                       (MP4Property**)&m_pChunkOffsetProperty);
    } else {
        success &= m_trakAtom.FindProperty(
                       "trak.mdia.minf.stbl.co64.entryCount",
                       (MP4Property**)&m_pChunkCountProperty);

        success &= m_trakAtom.FindProperty(
                       "trak.mdia.minf.stbl.co64.entries.chunkOffset",
                       (MP4Property**)&m_pChunkOffsetProperty);
    }

    // get handles on sample timing info

    success &= m_trakAtom.FindProperty(
                   "trak.mdia.minf.stbl.stts.entryCount",
                   (MP4Property**)&m_pSttsCountProperty);

    success &= m_trakAtom.FindProperty(
                   "trak.mdia.minf.stbl.stts.entries.sampleCount",
                   (MP4Property**)&m_pSttsSampleCountProperty);

    success &= m_trakAtom.FindProperty(
                   "trak.mdia.minf.stbl.stts.entries.sampleDelta",
                   (MP4Property**)&m_pSttsSampleDeltaProperty);

    // get handles on rendering offset info if it exists

    m_pCttsCountProperty = NULL;
    m_pCttsSampleCountProperty = NULL;
    m_pCttsSampleOffsetProperty = NULL;

    bool haveCtts = m_trakAtom.FindProperty(
                        "trak.mdia.minf.stbl.ctts.entryCount",
                        (MP4Property**)&m_pCttsCountProperty);

    if (haveCtts) {
        success &= m_trakAtom.FindProperty(
                       "trak.mdia.minf.stbl.ctts.entries.sampleCount",
                       (MP4Property**)&m_pCttsSampleCountProperty);

        success &= m_trakAtom.FindProperty(
                       "trak.mdia.minf.stbl.ctts.entries.sampleOffset",
                       (MP4Property**)&m_pCttsSampleOffsetProperty);
    }

    // get handles on sync sample info if it exists

    m_pStssCountProperty = NULL;
    m_pStssSampleProperty = NULL;

    bool haveStss = m_trakAtom.FindProperty(
                        "trak.mdia.minf.stbl.stss.entryCount",
                        (MP4Property**)&m_pStssCountProperty);

    if (haveStss) {
        success &= m_trakAtom.FindProperty(
                       "trak.mdia.minf.stbl.stss.entries.sampleNumber",
                       (MP4Property**)&m_pStssSampleProperty);
    }

    // edit list
    (void)InitEditListProperties();

    // was everything found?
    if (!success) {
        throw new Exception("invalid track", __FILE__, __LINE__, __FUNCTION__ );
    }
    CalculateBytesPerSample();

    // update sdtp log from sdtp atom
    MP4SdtpAtom* sdtp = (MP4SdtpAtom*)m_trakAtom.FindAtom( "trak.mdia.minf.stbl.sdtp" );
    if( sdtp ) {
        uint8_t* buffer;
        uint32_t bufsize;
        sdtp->data.GetValue( &buffer, &bufsize );
        m_sdtpLog.assign( (char*)buffer, bufsize );
        free( buffer );
    }
}

MP4Track::~MP4Track()
{
    MP4Free(m_pCachedReadSample);
    m_pCachedReadSample = NULL;
    MP4Free(m_pChunkBuffer);
    m_pChunkBuffer = NULL;
}

const char* MP4Track::GetType()
{
    return m_pTypeProperty->GetValue();
}

void MP4Track::SetType(const char* type)
{
    m_pTypeProperty->SetValue(MP4NormalizeTrackType(type));
}

void MP4Track::ReadSample(
    MP4SampleId   sampleId,
    uint8_t**     ppBytes,
    uint32_t*     pNumBytes,
    MP4Timestamp* pStartTime,
    MP4Duration*  pDuration,
    MP4Duration*  pRenderingOffset,
    bool*         pIsSyncSample,
    bool*         hasDependencyFlags, 
    uint32_t*     dependencyFlags )
{
    if( sampleId == MP4_INVALID_SAMPLE_ID )
        throw new Exception( "sample id can't be zero", __FILE__, __LINE__, __FUNCTION__ );

    if( hasDependencyFlags )
        *hasDependencyFlags = !m_sdtpLog.empty();

    if( dependencyFlags ) {
        if( m_sdtpLog.empty() ) {
            *dependencyFlags = 0;
        }
        else {
            if( sampleId > m_sdtpLog.size() )
                throw new Exception( "sample id > sdtp logsize", __FILE__, __LINE__, __FUNCTION__ );
            *dependencyFlags = m_sdtpLog[sampleId-1]; // sampleId is 1-based
        }
    }

    // handle unusual case of wanting to read a sample
    // that is still sitting in the write chunk buffer
    if (m_pChunkBuffer && sampleId >= m_writeSampleId - m_chunkSamples) {
        WriteChunkBuffer();
    }

    File* fin = GetSampleFile( sampleId );
    if( fin == (File*)-1 )
        throw new Exception( "sample is located in an inaccessible file", __FILE__, __LINE__, __FUNCTION__ );

    uint64_t fileOffset = GetSampleFileOffset(sampleId);

    uint32_t sampleSize = GetSampleSize(sampleId);
    if (*ppBytes != NULL && *pNumBytes < sampleSize) {
        throw new Exception("sample buffer is too small",
                            __FILE__, __LINE__, __FUNCTION__ );
    }
    *pNumBytes = sampleSize;

    log.verbose3f("\"%s\": ReadSample: track %u id %u offset 0x%" PRIx64 " size %u (0x%x)",
                  GetFile().GetFilename().c_str(), m_trackId, sampleId, fileOffset, *pNumBytes, *pNumBytes);

    bool bufferMalloc = false;
    if (*ppBytes == NULL) {
        *ppBytes = (uint8_t*)MP4Malloc(*pNumBytes);
        bufferMalloc = true;
    }

    uint64_t oldPos = m_File.GetPosition( fin ); // only used in mode == 'w'
    try {
        m_File.SetPosition( fileOffset, fin );
        m_File.ReadBytes( *ppBytes, *pNumBytes, fin );

        if (pStartTime || pDuration) {
            GetSampleTimes(sampleId, pStartTime, pDuration);

            log.verbose3f("\"%s\": ReadSample:  start %" PRIu64 " duration %" PRId64,
                          GetFile().GetFilename().c_str(), (pStartTime ? *pStartTime : 0),
                          (pDuration ? *pDuration : 0));
        }
        if (pRenderingOffset) {
            *pRenderingOffset = GetSampleRenderingOffset(sampleId);

            log.verbose3f("\"%s\": ReadSample:  renderingOffset %" PRId64,
                          GetFile().GetFilename().c_str(), *pRenderingOffset);
        }
        if (pIsSyncSample) {
            *pIsSyncSample = IsSyncSample(sampleId);

            log.verbose3f("\"%s\": ReadSample:  isSyncSample %u",
                          GetFile().GetFilename().c_str(), *pIsSyncSample);
        }
    }

    catch (Exception* x) {
        if( bufferMalloc ) {
            MP4Free( *ppBytes );
            *ppBytes = NULL;
        }

        if( m_File.IsWriteMode() )
            m_File.SetPosition( oldPos, fin );

        throw x;
    }

    if( m_File.IsWriteMode() )
        m_File.SetPosition( oldPos, fin );
}

void MP4Track::ReadSampleFragment(
    MP4SampleId sampleId,
    uint32_t sampleOffset,
    uint16_t sampleLength,
    uint8_t* pDest)
{
    if (sampleId == MP4_INVALID_SAMPLE_ID) {
        throw new Exception("invalid sample id",
                            __FILE__, __LINE__, __FUNCTION__ );
    }

    if (sampleId != m_cachedReadSampleId) {
        MP4Free(m_pCachedReadSample);
        m_pCachedReadSample = NULL;
        m_cachedReadSampleSize = 0;
        m_cachedReadSampleId = MP4_INVALID_SAMPLE_ID;

        ReadSample(
            sampleId,
            &m_pCachedReadSample,
            &m_cachedReadSampleSize);

        m_cachedReadSampleId = sampleId;
    }

    if (sampleOffset + sampleLength > m_cachedReadSampleSize) {
        throw new Exception("offset and/or length are too large",
                            __FILE__, __LINE__, __FUNCTION__ );
    }

    memcpy(pDest, &m_pCachedReadSample[sampleOffset], sampleLength);
}

void MP4Track::WriteSample(
    const uint8_t* pBytes,
    uint32_t       numBytes,
    MP4Duration    duration,
    MP4Duration    renderingOffset,
    bool           isSyncSample )
{
    uint8_t curMode = 0;

    log.verbose3f("\"%s\": WriteSample: track %u id %u size %u (0x%x) ",
                  GetFile().GetFilename().c_str(),
                  m_trackId, m_writeSampleId, numBytes, numBytes);

    if (pBytes == NULL && numBytes > 0) {
        throw new Exception("no sample data", __FILE__, __LINE__, __FUNCTION__ );
    }

    if (m_isAmr == AMR_UNINITIALIZED ) {
        // figure out if this is an AMR audio track
        if (m_trakAtom.FindAtom("trak.mdia.minf.stbl.stsd.samr") ||
                m_trakAtom.FindAtom("trak.mdia.minf.stbl.stsd.sawb")) {
            m_isAmr = AMR_TRUE;
            m_curMode = (pBytes[0] >> 3) & 0x000F;
        } else {
            m_isAmr = AMR_FALSE;
        }
    }

    if (m_isAmr == AMR_TRUE) {
        curMode = (pBytes[0] >> 3) &0x000F; // The mode is in the first byte
    }

    if (duration == MP4_INVALID_DURATION) {
        duration = GetFixedSampleDuration();
    }

    log.verbose3f("\"%s\": duration %" PRIu64, GetFile().GetFilename().c_str(), 
                  duration);

    if ((m_isAmr == AMR_TRUE) &&
            (m_curMode != curMode)) {
        WriteChunkBuffer();
        m_curMode = curMode;
    }

    // append sample bytes to chunk buffer
    if( m_sizeOfDataInChunkBuffer + numBytes > m_chunkBufferSize ) {
        m_pChunkBuffer = (uint8_t*)MP4Realloc(m_pChunkBuffer, m_chunkBufferSize + numBytes);
        if (m_pChunkBuffer == NULL) 
            return;	
        
        m_chunkBufferSize += numBytes;
    }

    memcpy(&m_pChunkBuffer[m_sizeOfDataInChunkBuffer], pBytes, numBytes);
    m_sizeOfDataInChunkBuffer += numBytes;
    m_chunkSamples++;
    m_chunkDuration += duration;

    UpdateSampleSizes(m_writeSampleId, numBytes);

    UpdateSampleTimes(duration);

    UpdateRenderingOffsets(m_writeSampleId, renderingOffset);

    UpdateSyncSamples(m_writeSampleId, isSyncSample);

    if (IsChunkFull(m_writeSampleId)) {
        WriteChunkBuffer();
        m_curMode = curMode;
    }

    UpdateDurations(duration);

    UpdateModificationTimes();

    m_writeSampleId++;
}

void MP4Track::WriteSampleDependency(
    const uint8_t* pBytes,
    uint32_t       numBytes,
    MP4Duration    duration,
    MP4Duration    renderingOffset,
    bool           isSyncSample,
    uint32_t       dependencyFlags )
{
    m_sdtpLog.push_back( dependencyFlags ); // record dependency flags for processing at finish
    WriteSample( pBytes, numBytes, duration, renderingOffset, isSyncSample );
}

void MP4Track::WriteChunkBuffer()
{
    if (m_sizeOfDataInChunkBuffer == 0) {
        return;
    }

    uint64_t chunkOffset = m_File.GetPosition();

    // write chunk buffer
    m_File.WriteBytes(m_pChunkBuffer, m_sizeOfDataInChunkBuffer);

    log.verbose3f("\"%s\": WriteChunk: track %u offset 0x%" PRIx64 " size %u (0x%x) numSamples %u",
                  GetFile().GetFilename().c_str(), 
                  m_trackId, chunkOffset, m_sizeOfDataInChunkBuffer,
                  m_sizeOfDataInChunkBuffer, m_chunkSamples);

    UpdateSampleToChunk(m_writeSampleId,
                        m_pChunkCountProperty->GetValue() + 1,
                        m_chunkSamples);

    UpdateChunkOffsets(chunkOffset);

    // note: we do not free our chunk buffer; we reuse it, expanding as needed.
    // It gets zapped when this class goes out of scope
    m_sizeOfDataInChunkBuffer = 0;
    m_chunkSamples = 0;
    m_chunkDuration = 0;
}

void MP4Track::FinishWrite(uint32_t options)
{
    FinishSdtp();

    // write out any remaining samples in chunk buffer
    WriteChunkBuffer();

    if (m_pStszFixedSampleSizeProperty == NULL &&
            m_stsz_sample_bits == 4) {
        if (m_have_stz2_4bit_sample) {
            ((MP4Integer8Property *)m_pStszSampleSizeProperty)->AddValue(m_stz2_4bit_sample_value);
            m_pStszSampleSizeProperty->IncrementValue();
        }
    }

    // record buffer size and bitrates
    MP4BitfieldProperty* pBufferSizeProperty;

    if (m_trakAtom.FindProperty(
                "trak.mdia.minf.stbl.stsd.*.esds.decConfigDescr.bufferSizeDB",
                (MP4Property**)&pBufferSizeProperty)) {
        pBufferSizeProperty->SetValue(GetMaxSampleSize());
    }

	// don't overwrite bitrate if it was requested in the Close call
    if( !(options & MP4_CLOSE_DO_NOT_COMPUTE_BITRATE)) {
        MP4Integer32Property* pBitrateProperty;

        if (m_trakAtom.FindProperty(
                    "trak.mdia.minf.stbl.stsd.*.esds.decConfigDescr.maxBitrate",
                    (MP4Property**)&pBitrateProperty)) {
            pBitrateProperty->SetValue(GetMaxBitrate());
        }

        if (m_trakAtom.FindProperty(
                    "trak.mdia.minf.stbl.stsd.*.esds.decConfigDescr.avgBitrate",
                    (MP4Property**)&pBitrateProperty)) {
            pBitrateProperty->SetValue(GetAvgBitrate());
        }
    }

    // cleaup trak.udta
    MP4BytesProperty* nameProperty = NULL;
    m_trakAtom.FindProperty("trak.udta.name.value", (MP4Property**) &nameProperty);
    if( nameProperty != NULL && nameProperty->GetValueSize() == 0 ){
        // Zero length name value--delete name, and then udta if no child atoms
        MP4Atom* name = m_trakAtom.FindChildAtom("udta.name");
        if( name ) {
            MP4Atom* udta = name->GetParentAtom();
            udta->DeleteChildAtom( name );
            delete name;

            if( udta->GetNumberOfChildAtoms() == 0 ) {
                udta->GetParentAtom()->DeleteChildAtom( udta );
                delete udta;
            }
        }
    }
}

// Process sdtp log and add sdtp atom.
//
// Testing (subjective) showed a marked improvement with QuickTime
// player on Mac OS X when scrubbing. Best results were obtained
// from encodings using low number of bframes. It's expected sdtp may help
// other QT-based players.
//
void MP4Track::FinishSdtp()
{
    // bail if log is empty -- indicates dependency information was not written
    if( m_sdtpLog.empty() )
        return;

    MP4SdtpAtom* sdtp = (MP4SdtpAtom*)m_trakAtom.FindAtom( "trak.mdia.minf.stbl.sdtp" );
    if( !sdtp )
        sdtp = (MP4SdtpAtom*)AddAtom( "trak.mdia.minf.stbl", "sdtp" );
    sdtp->data.SetValue( (const uint8_t*)m_sdtpLog.data(), (uint32_t)m_sdtpLog.size() );

    // add avc1 compatibility indicator if not present
    MP4FtypAtom* ftyp = (MP4FtypAtom*)m_File.FindAtom( "ftyp" );
    if( ftyp ) {
        bool found = false;
        const uint32_t max = ftyp->compatibleBrands.GetCount();
        for( uint32_t i = 0; i < max; i++ ) {
            if( !strcmp( ftyp->compatibleBrands.GetValue( i ), "avc1" )) {
                found = true;
                break;
            }
        }

        if( !found )
            ftyp->compatibleBrands.AddValue( "avc1" );
    }
}

bool MP4Track::IsChunkFull(MP4SampleId sampleId)
{
    if (m_samplesPerChunk) {
        return m_chunkSamples >= m_samplesPerChunk;
    }

    ASSERT(m_durationPerChunk);
    return m_chunkDuration >= m_durationPerChunk;
}

uint32_t MP4Track::GetNumberOfSamples()
{
    return m_pStszSampleCountProperty->GetValue();
}

uint32_t MP4Track::GetSampleSize(MP4SampleId sampleId)
{
    if (m_pStszFixedSampleSizeProperty != NULL) {
        uint32_t fixedSampleSize =
            m_pStszFixedSampleSizeProperty->GetValue();

        if (fixedSampleSize != 0) {
            return fixedSampleSize * m_bytesPerSample;
        }
    }
    // will have to check for 4 bit sample size here
    if (m_stsz_sample_bits == 4) {
        uint8_t value = m_pStszSampleSizeProperty->GetValue((sampleId - 1) / 2);
        if ((sampleId - 1) / 2 == 0) {
            value >>= 4;
        } else value &= 0xf;
        return m_bytesPerSample * value;
    }
    return m_bytesPerSample *
           m_pStszSampleSizeProperty->GetValue(sampleId - 1);
}

uint32_t MP4Track::GetMaxSampleSize()
{
    if (m_pStszFixedSampleSizeProperty != NULL) {
        uint32_t fixedSampleSize =
            m_pStszFixedSampleSizeProperty->GetValue();

        if (fixedSampleSize != 0) {
            return fixedSampleSize * m_bytesPerSample;
        }
    }

    uint32_t maxSampleSize = 0;
    uint32_t numSamples = m_pStszSampleSizeProperty->GetCount();
    for (MP4SampleId sid = 1; sid <= numSamples; sid++) {
        uint32_t sampleSize =
            m_pStszSampleSizeProperty->GetValue(sid - 1);
        if (sampleSize > maxSampleSize) {
            maxSampleSize = sampleSize;
        }
    }
    return maxSampleSize * m_bytesPerSample;
}

uint64_t MP4Track::GetTotalOfSampleSizes()
{
    uint64_t retval;
    if (m_pStszFixedSampleSizeProperty != NULL) {
        uint32_t fixedSampleSize =
            m_pStszFixedSampleSizeProperty->GetValue();

        // if fixed sample size, just need to multiply by number of samples
        if (fixedSampleSize != 0) {
            retval = m_bytesPerSample;
            retval *= fixedSampleSize;
            retval *= GetNumberOfSamples();
            return retval;
        }
    }

    // else non-fixed sample size, sum them
    uint64_t totalSampleSizes = 0;
    uint32_t numSamples = m_pStszSampleSizeProperty->GetCount();
    for (MP4SampleId sid = 1; sid <= numSamples; sid++) {
        uint32_t sampleSize =
            m_pStszSampleSizeProperty->GetValue(sid - 1);
        totalSampleSizes += sampleSize;
    }
    return totalSampleSizes * m_bytesPerSample;
}

void MP4Track::SampleSizePropertyAddValue (uint32_t size)
{
    // this has to deal with different sample size values
    switch (m_pStszSampleSizeProperty->GetType()) {
    case Integer32Property:
        ((MP4Integer32Property *)m_pStszSampleSizeProperty)->AddValue(size);
        break;
    case Integer16Property:
        ((MP4Integer16Property *)m_pStszSampleSizeProperty)->AddValue(size);
        break;
    case Integer8Property:
        if (m_stsz_sample_bits == 4) {
            if (m_have_stz2_4bit_sample == false) {
                m_have_stz2_4bit_sample = true;
                m_stz2_4bit_sample_value = size << 4;
                return;
            } else {
                m_have_stz2_4bit_sample = false;
                size &= 0xf;
                size |= m_stz2_4bit_sample_value;
            }
        }
        ((MP4Integer8Property *)m_pStszSampleSizeProperty)->AddValue(size);
        break;
    default:
        break;
    }


    //  m_pStszSampleSizeProperty->IncrementValue();
}

void MP4Track::UpdateSampleSizes(MP4SampleId sampleId, uint32_t numBytes)
{
    if (m_bytesPerSample > 1) {
        if ((numBytes % m_bytesPerSample) != 0) {
            // error
            log.errorf("%s: \"%s\": numBytes %u not divisible by bytesPerSample %u sampleId %u",
                       __FUNCTION__, GetFile().GetFilename().c_str(),
                       numBytes, m_bytesPerSample, sampleId);
        }
        numBytes /= m_bytesPerSample;
    }
    // for first sample
    // wmay - if we are adding, we want to make sure that
    // we don't inadvertently set up the fixed size again.
    // so, we check the number of samples
    if (sampleId == 1 && GetNumberOfSamples() == 0) {
        if (m_pStszFixedSampleSizeProperty == NULL ||
                numBytes == 0) {
            // special case of first sample is zero bytes in length
            // leave m_pStszFixedSampleSizeProperty at 0
            // start recording variable sample sizes
            if (m_pStszFixedSampleSizeProperty != NULL)
                m_pStszFixedSampleSizeProperty->SetValue(0);
            SampleSizePropertyAddValue(0);
        } else {
            // presume sample size is fixed
            m_pStszFixedSampleSizeProperty->SetValue(numBytes);
        }
    } else { // sampleId > 1

        uint32_t fixedSampleSize = 0;
        if (m_pStszFixedSampleSizeProperty != NULL) {
            fixedSampleSize = m_pStszFixedSampleSizeProperty->GetValue();
        }

        // if we don't have a fixed size, or the current sample size
        // doesn't match our sample size, we need to write the current
        // sample size into the table
        if (fixedSampleSize == 0 || numBytes != fixedSampleSize) {
            if (fixedSampleSize != 0) {
                // fixed size was set; we need to clear fixed sample size
                m_pStszFixedSampleSizeProperty->SetValue(0);

                // and create sizes for all previous samples
                // use GetNumberOfSamples due to needing the total number
                // not just the appended part of the file
                uint32_t samples = GetNumberOfSamples();
                for (MP4SampleId sid = 1; sid <= samples; sid++) {
                    SampleSizePropertyAddValue(fixedSampleSize);
                }
            }
            // add size value for this sample
            SampleSizePropertyAddValue(numBytes);
        }
    }
    // either way, we increment the number of samples.
    m_pStszSampleCountProperty->IncrementValue();
}

uint32_t MP4Track::GetAvgBitrate()
{
    if (GetDuration() == 0) {
        return 0;
    }

    double calc = double(GetTotalOfSampleSizes());
    // this is a bit better - we use the whole duration
    calc *= 8.0;
    calc *= GetTimeScale();
    calc /= double(GetDuration());
    // we might want to think about rounding to the next 100 or 1000
    return (uint32_t) ceil(calc);
}

uint32_t MP4Track::GetMaxBitrate()
{
    uint32_t timeScale = GetTimeScale();
    MP4SampleId numSamples = GetNumberOfSamples();
    uint32_t maxBytesPerSec = 0;
    uint32_t bytesThisSec = 0;
    MP4Timestamp thisSecStart = 0;
    MP4Timestamp lastSampleTime = 0;
    uint32_t lastSampleSize = 0;

    MP4SampleId thisSecStartSid = 1;
    for (MP4SampleId sid = 1; sid <= numSamples; sid++) {
        uint32_t sampleSize;
        MP4Timestamp sampleTime;

        sampleSize = GetSampleSize(sid);
        GetSampleTimes(sid, &sampleTime, NULL);

        if (sampleTime < thisSecStart + timeScale) {
            bytesThisSec += sampleSize;
            lastSampleSize = sampleSize;
            lastSampleTime = sampleTime;
        } else {
            // we've already written the last sample and sampleSize.
            // this means that we've probably overflowed the last second
            // calculate the time we've overflowed
            MP4Duration overflow_dur =
                (thisSecStart + timeScale) - lastSampleTime;
            // calculate the duration of the last sample
            MP4Duration lastSampleDur = sampleTime - lastSampleTime;
            // now, calculate the number of bytes we overflowed.  Round up.
            if( lastSampleDur > 0 ) {
                uint32_t overflow_bytes = 0;
                overflow_bytes = ((lastSampleSize * overflow_dur) + (lastSampleDur - 1)) / lastSampleDur;

                if (bytesThisSec - overflow_bytes > maxBytesPerSec) {
                    maxBytesPerSec = bytesThisSec - overflow_bytes;
                }
            }

            // now adjust the values for this sample.  Remove the bytes
            // from the first sample in this time frame
            lastSampleTime = sampleTime;
            lastSampleSize = sampleSize;
            bytesThisSec += sampleSize;
            bytesThisSec -= GetSampleSize(thisSecStartSid);
            thisSecStartSid++;
            GetSampleTimes(thisSecStartSid, &thisSecStart, NULL);
        }
    }

    return maxBytesPerSec * 8;
}

uint32_t MP4Track::GetSampleStscIndex(MP4SampleId sampleId)
{
    uint32_t stscIndex;
    uint32_t numStscs = m_pStscCountProperty->GetValue();

    if (numStscs == 0) {
        throw new Exception("No data chunks exist", __FILE__, __LINE__, __FUNCTION__ );
    }

    for (stscIndex = 0; stscIndex < numStscs; stscIndex++) {
        if (sampleId < m_pStscFirstSampleProperty->GetValue(stscIndex)) {
            ASSERT(stscIndex != 0);
            stscIndex -= 1;
            break;
        }
    }
    if (stscIndex == numStscs) {
        ASSERT(stscIndex != 0);
        stscIndex -= 1;
    }

    return stscIndex;
}

File* MP4Track::GetSampleFile( MP4SampleId sampleId )
{
    uint32_t stscIndex = GetSampleStscIndex( sampleId );
    uint32_t stsdIndex = m_pStscSampleDescrIndexProperty->GetValue( stscIndex );

    // check if the answer will be the same as last time
    if( m_lastStsdIndex && stsdIndex == m_lastStsdIndex )
        return m_lastSampleFile;

    MP4Atom* pStsdAtom = m_trakAtom.FindAtom( "trak.mdia.minf.stbl.stsd" );
    ASSERT( pStsdAtom );

    MP4Atom* pStsdEntryAtom = pStsdAtom->GetChildAtom( stsdIndex - 1 );
    ASSERT( pStsdEntryAtom );

    MP4Integer16Property* pDrefIndexProperty = NULL;
    if( !pStsdEntryAtom->FindProperty( "*.dataReferenceIndex", (MP4Property**)&pDrefIndexProperty ) ||
        pDrefIndexProperty == NULL )
    {
        throw new Exception( "invalid stsd entry", __FILE__, __LINE__, __FUNCTION__ );
    }

    uint32_t drefIndex = pDrefIndexProperty->GetValue();

    MP4Atom* pDrefAtom = m_trakAtom.FindAtom( "trak.mdia.minf.dinf.dref" );
    ASSERT(pDrefAtom);

    MP4Atom* pUrlAtom = pDrefAtom->GetChildAtom( drefIndex - 1 );
    ASSERT( pUrlAtom );

    File* file;

    // make sure this is actually a url atom (somtimes it's "cios", like in iTunes videos)
    if( strcmp(pUrlAtom->GetType(), "url ") ||
        pUrlAtom->GetFlags() & 1 ) {
        file = NULL; // self-contained
    }
    else {
        MP4StringProperty* pLocationProperty = NULL;
        ASSERT( pUrlAtom->FindProperty( "*.location", (MP4Property**)&pLocationProperty) );
        ASSERT( pLocationProperty );

        const char* url = pLocationProperty->GetValue();

        log.verbose3f("\"%s\": dref url = %s", GetFile().GetFilename().c_str(), 
                      url);

        file = (File*)-1;

        // attempt to open url if it's a file url
        // currently this is the only thing we understand
        if( !strncmp( url, "file:", 5 )) {
            const char* fileName = url + 5;

            if( !strncmp(fileName, "//", 2 ))
                fileName = strchr( fileName + 2, '/' );

            if( fileName ) {
                file = new File( fileName, File::MODE_READ );
                if( !file->open() ) {
                    delete file;
                    file = (File*)-1;
                }
            }
        }
    }

    if( m_lastSampleFile )
        m_lastSampleFile->close();

    // cache the answer
    m_lastStsdIndex = stsdIndex;
    m_lastSampleFile = file;

    return file;
}

uint64_t MP4Track::GetSampleFileOffset(MP4SampleId sampleId)
{
    uint32_t stscIndex =
        GetSampleStscIndex(sampleId);

    // firstChunk is the chunk index of the first chunk with
    // samplesPerChunk samples in the chunk.  There may be multiples -
    // ie: several chunks with the same number of samples per chunk.
    uint32_t firstChunk =
        m_pStscFirstChunkProperty->GetValue(stscIndex);

    MP4SampleId firstSample =
        m_pStscFirstSampleProperty->GetValue(stscIndex);

    uint32_t samplesPerChunk =
        m_pStscSamplesPerChunkProperty->GetValue(stscIndex);

    // chunkId tells which is the absolute chunk number that this sample
    // is stored in.
    MP4ChunkId chunkId = firstChunk +
                         ((sampleId - firstSample) / samplesPerChunk);

    // chunkOffset is the file offset (absolute) for the start of the chunk
    uint64_t chunkOffset = m_pChunkOffsetProperty->GetValue(chunkId - 1);

    MP4SampleId firstSampleInChunk =
        sampleId - ((sampleId - firstSample) % samplesPerChunk);

    // need cumulative samples sizes from firstSample to sampleId - 1
    uint32_t sampleOffset = 0;
    for (MP4SampleId i = firstSampleInChunk; i < sampleId; i++) {
        sampleOffset += GetSampleSize(i);
    }

    return chunkOffset + sampleOffset;
}

void MP4Track::UpdateSampleToChunk(MP4SampleId sampleId,
                                   MP4ChunkId chunkId, uint32_t samplesPerChunk)
{
    uint32_t numStsc = m_pStscCountProperty->GetValue();

    // if samplesPerChunk == samplesPerChunk of last entry
    if (numStsc && samplesPerChunk ==
            m_pStscSamplesPerChunkProperty->GetValue(numStsc-1)) {

        // nothing to do

    } else {
        // add stsc entry
        m_pStscFirstChunkProperty->AddValue(chunkId);
        m_pStscSamplesPerChunkProperty->AddValue(samplesPerChunk);
        m_pStscSampleDescrIndexProperty->AddValue(1);
        m_pStscFirstSampleProperty->AddValue(sampleId - samplesPerChunk + 1);

        m_pStscCountProperty->IncrementValue();
    }
}

void MP4Track::UpdateChunkOffsets(uint64_t chunkOffset)
{
    if (m_pChunkOffsetProperty->GetType() == Integer32Property) {
        ((MP4Integer32Property*)m_pChunkOffsetProperty)->AddValue(chunkOffset);
    } else {
        ((MP4Integer64Property*)m_pChunkOffsetProperty)->AddValue(chunkOffset);
    }
    m_pChunkCountProperty->IncrementValue();
}

MP4Duration MP4Track::GetFixedSampleDuration()
{
    uint32_t numStts = m_pSttsCountProperty->GetValue();

    if (numStts == 0) {
        return m_fixedSampleDuration;
    }
    if (numStts != 1) {
        return MP4_INVALID_DURATION;    // sample duration is not fixed
    }
    return m_pSttsSampleDeltaProperty->GetValue(0);
}

void MP4Track::SetFixedSampleDuration(MP4Duration duration)
{
    uint32_t numStts = m_pSttsCountProperty->GetValue();

    // setting this is only allowed before samples have been written
    if (numStts != 0) {
        return;
    }
    m_fixedSampleDuration = duration;
    return;
}

void MP4Track::GetSampleTimes(MP4SampleId sampleId,
                              MP4Timestamp* pStartTime, MP4Duration* pDuration)
{
    uint32_t numStts = m_pSttsCountProperty->GetValue();
    MP4SampleId sid;
    MP4Duration elapsed;


    if (m_cachedSttsSid != MP4_INVALID_SAMPLE_ID && sampleId >= m_cachedSttsSid) {
        sid   = m_cachedSttsSid;
        elapsed   = m_cachedSttsElapsed;
    } else {
        m_cachedSttsIndex = 0;
        sid   = 1;
        elapsed   = 0;
    }

    for (uint32_t sttsIndex = m_cachedSttsIndex; sttsIndex < numStts; sttsIndex++) {
        uint64_t sampleCount =
            m_pSttsSampleCountProperty->GetValue(sttsIndex);
        uint32_t sampleDelta =
            m_pSttsSampleDeltaProperty->GetValue(sttsIndex);

        if (sampleId <= sid + sampleCount - 1) {
            if (pStartTime) {
                *pStartTime = (sampleId - sid);
                *pStartTime *= sampleDelta;
                *pStartTime += elapsed;
            }
            if (pDuration) {
                *pDuration = sampleDelta;
            }

            m_cachedSttsIndex = sttsIndex;
            m_cachedSttsSid = sid;
            m_cachedSttsElapsed = elapsed;

            return;
        }
        sid += sampleCount;
        elapsed += sampleCount * sampleDelta;
    }

    throw new Exception("sample id out of range",
                        __FILE__, __LINE__, __FUNCTION__ );
}

MP4SampleId MP4Track::GetSampleIdFromTime(
    MP4Timestamp when,
    bool wantSyncSample)
{
    uint32_t numStts = m_pSttsCountProperty->GetValue();
    MP4SampleId sid = 1;
    MP4Duration elapsed = 0;

    for (uint32_t sttsIndex = 0; sttsIndex < numStts; sttsIndex++) {
        uint64_t sampleCount =
            m_pSttsSampleCountProperty->GetValue(sttsIndex);
        uint32_t sampleDelta =
            m_pSttsSampleDeltaProperty->GetValue(sttsIndex);

        if (sampleDelta == 0 && sttsIndex < numStts - 1) {
            log.warningf("%s: \"%s\": Zero sample duration, stts entry %u",
                         __FUNCTION__, GetFile().GetFilename().c_str(), sttsIndex);
        }

        MP4Duration d = when - elapsed;

        if (d <= sampleCount * sampleDelta) {
            MP4SampleId sampleId = sid;
            if (sampleDelta) {
                sampleId += (d / sampleDelta);
            }

            if (wantSyncSample) {
                return GetNextSyncSample(sampleId);
            }
            return sampleId;
        }

        sid += sampleCount;
        elapsed += sampleCount * sampleDelta;
    }

    throw new Exception("time out of range",
                        __FILE__, __LINE__, __FUNCTION__);

    return 0; // satisfy MS compiler
}

void MP4Track::UpdateSampleTimes(MP4Duration duration)
{
    uint32_t numStts = m_pSttsCountProperty->GetValue();

    // if duration == duration of last entry
    if (numStts
            && duration == m_pSttsSampleDeltaProperty->GetValue(numStts-1)) {
        // increment last entry sampleCount
        m_pSttsSampleCountProperty->IncrementValue(1, numStts-1);

    } else {
        // add stts entry, sampleCount = 1, sampleDuration = duration
        m_pSttsSampleCountProperty->AddValue(1);
        m_pSttsSampleDeltaProperty->AddValue(duration);
        m_pSttsCountProperty->IncrementValue();;
    }
}

uint32_t MP4Track::GetSampleCttsIndex(MP4SampleId sampleId,
                                      MP4SampleId* pFirstSampleId)
{
    uint32_t numCtts = m_pCttsCountProperty->GetValue();
    MP4SampleId sid;

    if (m_cachedCttsSid != MP4_INVALID_SAMPLE_ID && sampleId >= m_cachedCttsSid) {
        sid   = m_cachedCttsSid;
    } else {
        m_cachedCttsIndex = 0;
        sid = 1;
    }

    for (uint32_t cttsIndex = m_cachedCttsIndex; cttsIndex < numCtts; cttsIndex++) {
        uint32_t sampleCount =
            m_pCttsSampleCountProperty->GetValue(cttsIndex);
        
        if (sampleId <= sid + sampleCount - 1) {
            if (pFirstSampleId) {
                *pFirstSampleId = sid;
            }

            m_cachedCttsIndex = cttsIndex;
            m_cachedCttsSid = sid;

            return cttsIndex;
        }
        sid += sampleCount;
    }

    throw new Exception("sample id out of range",
                        __FILE__, __LINE__, __FUNCTION__ );
    return 0; // satisfy MS compiler
}

MP4Duration MP4Track::GetSampleRenderingOffset(MP4SampleId sampleId)
{
    if (m_pCttsCountProperty == NULL) {
        return 0;
    }
    if (m_pCttsCountProperty->GetValue() == 0) {
        return 0;
    }

    uint32_t cttsIndex = GetSampleCttsIndex(sampleId);

    return m_pCttsSampleOffsetProperty->GetValue(cttsIndex);
}

void MP4Track::UpdateRenderingOffsets(MP4SampleId sampleId,
                                      MP4Duration renderingOffset)
{
    // if ctts atom doesn't exist
    if (m_pCttsCountProperty == NULL) {

        // no rendering offset, so nothing to do
        if (renderingOffset == 0) {
            return;
        }

        // else create a ctts atom
        MP4Atom* pCttsAtom = AddAtom("trak.mdia.minf.stbl", "ctts");

        // and get handles on the properties
        ASSERT(pCttsAtom->FindProperty(
                   "ctts.entryCount",
                   (MP4Property**)&m_pCttsCountProperty));

        ASSERT(pCttsAtom->FindProperty(
                   "ctts.entries.sampleCount",
                   (MP4Property**)&m_pCttsSampleCountProperty));

        ASSERT(pCttsAtom->FindProperty(
                   "ctts.entries.sampleOffset",
                   (MP4Property**)&m_pCttsSampleOffsetProperty));

        // if this is not the first sample
        if (sampleId > 1) {
            // add a ctts entry for all previous samples
            // with rendering offset equal to zero
            m_pCttsSampleCountProperty->AddValue(sampleId - 1);
            m_pCttsSampleOffsetProperty->AddValue(0);
            m_pCttsCountProperty->IncrementValue();;
        }
    }

    // ctts atom exists (now)

    uint32_t numCtts = m_pCttsCountProperty->GetValue();

    // if renderingOffset == renderingOffset of last entry
    if (numCtts && renderingOffset
            == m_pCttsSampleOffsetProperty->GetValue(numCtts-1)) {

        // increment last entry sampleCount
        m_pCttsSampleCountProperty->IncrementValue(1, numCtts-1);

    } else {
        // add ctts entry, sampleCount = 1, sampleOffset = renderingOffset
        m_pCttsSampleCountProperty->AddValue(1);
        m_pCttsSampleOffsetProperty->AddValue(renderingOffset);
        m_pCttsCountProperty->IncrementValue();
    }
}

void MP4Track::SetSampleRenderingOffset(MP4SampleId sampleId,
                                        MP4Duration renderingOffset)
{
    // check if any ctts entries exist
    if (m_pCttsCountProperty == NULL
            || m_pCttsCountProperty->GetValue() == 0) {
        // if not then Update routine can be used
        // to create a ctts entry for samples before this one
        // and a ctts entry for this sample
        UpdateRenderingOffsets(sampleId, renderingOffset);

        // but we also need a ctts entry
        // for all samples after this one
        uint32_t afterSamples = GetNumberOfSamples() - sampleId;

        if (afterSamples) {
            m_pCttsSampleCountProperty->AddValue(afterSamples);
            m_pCttsSampleOffsetProperty->AddValue(0);
            m_pCttsCountProperty->IncrementValue();;
        }

        return;
    }

    MP4SampleId firstSampleId;
    uint32_t cttsIndex = GetSampleCttsIndex(sampleId, &firstSampleId);

    // do nothing in the degenerate case
    if (renderingOffset ==
            m_pCttsSampleOffsetProperty->GetValue(cttsIndex)) {
        return;
    }

    uint32_t sampleCount =
        m_pCttsSampleCountProperty->GetValue(cttsIndex);

    // if this sample has it's own ctts entry
    if (sampleCount == 1) {
        // then just set the value,
        // note we don't attempt to collapse entries
        m_pCttsSampleOffsetProperty->SetValue(renderingOffset, cttsIndex);
        return;
    }

    MP4SampleId lastSampleId = firstSampleId + sampleCount - 1;

    // else we share this entry with other samples
    // we need to insert our own entry
    if (sampleId == firstSampleId) {
        // our sample is the first one
        m_pCttsSampleCountProperty->
        InsertValue(1, cttsIndex);
        m_pCttsSampleOffsetProperty->
        InsertValue(renderingOffset, cttsIndex);

        m_pCttsSampleCountProperty->
        SetValue(sampleCount - 1, cttsIndex + 1);

        m_pCttsCountProperty->IncrementValue();

    } else if (sampleId == lastSampleId) {
        // our sample is the last one
        m_pCttsSampleCountProperty->
        InsertValue(1, cttsIndex + 1);
        m_pCttsSampleOffsetProperty->
        InsertValue(renderingOffset, cttsIndex + 1);

        m_pCttsSampleCountProperty->
        SetValue(sampleCount - 1, cttsIndex);

        m_pCttsCountProperty->IncrementValue();

    } else {
        // our sample is in the middle, UGH!

        // insert our new entry
        m_pCttsSampleCountProperty->
        InsertValue(1, cttsIndex + 1);
        m_pCttsSampleOffsetProperty->
        InsertValue(renderingOffset, cttsIndex + 1);

        // adjust count of previous entry
        m_pCttsSampleCountProperty->
        SetValue(sampleId - firstSampleId, cttsIndex);

        // insert new entry for those samples beyond our sample
        m_pCttsSampleCountProperty->
        InsertValue(lastSampleId - sampleId, cttsIndex + 2);
        uint32_t oldRenderingOffset =
            m_pCttsSampleOffsetProperty->GetValue(cttsIndex);
        m_pCttsSampleOffsetProperty->
        InsertValue(oldRenderingOffset, cttsIndex + 2);

        m_pCttsCountProperty->IncrementValue(2);
    }
}

bool MP4Track::IsSyncSample(MP4SampleId sampleId)
{
    if (m_pStssCountProperty == NULL) {
        return true;
    }

    uint32_t numStss = m_pStssCountProperty->GetValue();
    uint32_t stssLIndex = 0;
    uint32_t stssRIndex = numStss - 1;

    while (stssRIndex >= stssLIndex) {
        uint32_t stssIndex = (stssRIndex + stssLIndex) >> 1;
        MP4SampleId syncSampleId =
            m_pStssSampleProperty->GetValue(stssIndex);

        if (sampleId == syncSampleId) {
            return true;
        }

        if (sampleId > syncSampleId) {
            stssLIndex = stssIndex + 1;
        } else {
            stssRIndex = stssIndex - 1;
        }
    }

    return false;
}

// N.B. "next" is inclusive of this sample id
MP4SampleId MP4Track::GetNextSyncSample(MP4SampleId sampleId)
{
    if (m_pStssCountProperty == NULL) {
        return sampleId;
    }

    uint32_t numStss = m_pStssCountProperty->GetValue();

    for (uint32_t stssIndex = 0; stssIndex < numStss; stssIndex++) {
        MP4SampleId syncSampleId =
            m_pStssSampleProperty->GetValue(stssIndex);

        if (sampleId > syncSampleId) {
            continue;
        }
        return syncSampleId;
    }

    // LATER check stsh for alternate sample

    return MP4_INVALID_SAMPLE_ID;
}

void MP4Track::UpdateSyncSamples(MP4SampleId sampleId, bool isSyncSample)
{
    if (isSyncSample) {
        // if stss atom exists, add entry
        if (m_pStssCountProperty) {
            m_pStssSampleProperty->AddValue(sampleId);
            m_pStssCountProperty->IncrementValue();
        } // else nothing to do (yet)

    } else { // !isSyncSample
        // if stss atom doesn't exist, create one
        if (m_pStssCountProperty == NULL) {

            MP4Atom* pStssAtom = AddAtom("trak.mdia.minf.stbl", "stss");

            ASSERT(pStssAtom->FindProperty(
                       "stss.entryCount",
                       (MP4Property**)&m_pStssCountProperty));

            ASSERT(pStssAtom->FindProperty(
                       "stss.entries.sampleNumber",
                       (MP4Property**)&m_pStssSampleProperty));

            // set values for all samples that came before this one
            uint32_t samples = GetNumberOfSamples();
            for (MP4SampleId sid = 1; sid < samples; sid++) {
                m_pStssSampleProperty->AddValue(sid);
                m_pStssCountProperty->IncrementValue();
            }
        } // else nothing to do
    }
}

MP4Atom* MP4Track::AddAtom(const char* parentName, const char* childName)
{
    MP4Atom* pParentAtom = m_trakAtom.FindAtom(parentName);
    ASSERT(pParentAtom);

    MP4Atom* pChildAtom = MP4Atom::CreateAtom(m_File, pParentAtom, childName);

    pParentAtom->AddChildAtom(pChildAtom);

    pChildAtom->Generate();

    return pChildAtom;
}

uint64_t MP4Track::GetDuration()
{
    return m_pMediaDurationProperty->GetValue();
}

uint32_t MP4Track::GetTimeScale()
{
    return m_pTimeScaleProperty->GetValue();
}

void MP4Track::UpdateDurations(MP4Duration duration)
{
    // update media, track, and movie durations
    m_pMediaDurationProperty->SetValue(
        m_pMediaDurationProperty->GetValue() + duration);

    MP4Duration movieDuration = ToMovieDuration(
        m_pMediaDurationProperty->GetValue());
    m_pTrackDurationProperty->SetValue(movieDuration);

    m_File.UpdateDuration(m_pTrackDurationProperty->GetValue());
}

MP4Duration MP4Track::ToMovieDuration(MP4Duration trackDuration)
{
    return (trackDuration * m_File.GetTimeScale())
           / m_pTimeScaleProperty->GetValue();
}

void MP4Track::UpdateModificationTimes()
{
    // update media and track modification times
    MP4Timestamp now = MP4GetAbsTimestamp();
    m_pMediaModificationProperty->SetValue(now);
    m_pTrackModificationProperty->SetValue(now);
}

uint32_t MP4Track::GetNumberOfChunks()
{
    return m_pChunkOffsetProperty->GetCount();
}

uint32_t MP4Track::GetChunkStscIndex(MP4ChunkId chunkId)
{
    uint32_t stscIndex;
    uint32_t numStscs = m_pStscCountProperty->GetValue();

    ASSERT(chunkId);
    ASSERT(numStscs > 0);

    for (stscIndex = 0; stscIndex < numStscs; stscIndex++) {
        if (chunkId < m_pStscFirstChunkProperty->GetValue(stscIndex)) {
            ASSERT(stscIndex != 0);
            break;
        }
    }
    return stscIndex - 1;
}

MP4Timestamp MP4Track::GetChunkTime(MP4ChunkId chunkId)
{
    uint32_t stscIndex = GetChunkStscIndex(chunkId);

    MP4ChunkId firstChunkId =
        m_pStscFirstChunkProperty->GetValue(stscIndex);

    MP4SampleId firstSample =
        m_pStscFirstSampleProperty->GetValue(stscIndex);

    uint32_t samplesPerChunk =
        m_pStscSamplesPerChunkProperty->GetValue(stscIndex);

    MP4SampleId firstSampleInChunk =
        firstSample + ((chunkId - firstChunkId) * samplesPerChunk);

    MP4Timestamp chunkTime;

    GetSampleTimes(firstSampleInChunk, &chunkTime, NULL);

    return chunkTime;
}

uint32_t MP4Track::GetChunkSize(MP4ChunkId chunkId)
{
    uint32_t stscIndex = GetChunkStscIndex(chunkId);

    MP4ChunkId firstChunkId =
        m_pStscFirstChunkProperty->GetValue(stscIndex);

    MP4SampleId firstSample =
        m_pStscFirstSampleProperty->GetValue(stscIndex);

    uint32_t samplesPerChunk =
        m_pStscSamplesPerChunkProperty->GetValue(stscIndex);

    MP4SampleId firstSampleInChunk =
        firstSample + ((chunkId - firstChunkId) * samplesPerChunk);

    // need cumulative sizes of samples in chunk
    uint32_t chunkSize = 0;
    for (uint32_t i = 0; i < samplesPerChunk; i++) {
        chunkSize += GetSampleSize(firstSampleInChunk + i);
    }

    return chunkSize;
}

void MP4Track::ReadChunk(MP4ChunkId chunkId,
                         uint8_t** ppChunk, uint32_t* pChunkSize)
{
    ASSERT(chunkId);
    ASSERT(ppChunk);
    ASSERT(pChunkSize);

    uint64_t chunkOffset =
        m_pChunkOffsetProperty->GetValue(chunkId - 1);

    *pChunkSize = GetChunkSize(chunkId);
    *ppChunk = (uint8_t*)MP4Malloc(*pChunkSize);

    log.verbose3f("\"%s\": ReadChunk: track %u id %u offset 0x%" PRIx64 " size %u (0x%x)",
                  GetFile().GetFilename().c_str(),
                  m_trackId, chunkId, chunkOffset, *pChunkSize, *pChunkSize);

    uint64_t oldPos = m_File.GetPosition(); // only used in mode == 'w'
    try {
        m_File.SetPosition( chunkOffset );
        m_File.ReadBytes( *ppChunk, *pChunkSize );
    }
    catch( Exception* x ) {
        MP4Free( *ppChunk );
        *ppChunk = NULL;

        if( m_File.IsWriteMode() )
            m_File.SetPosition( oldPos );

        throw x;
    }

    if( m_File.IsWriteMode() )
        m_File.SetPosition( oldPos );
}

void MP4Track::RewriteChunk(MP4ChunkId chunkId,
                            uint8_t* pChunk, uint32_t chunkSize)
{
    uint64_t chunkOffset = m_File.GetPosition();

    m_File.WriteBytes(pChunk, chunkSize);

    m_pChunkOffsetProperty->SetValue(chunkOffset, chunkId - 1);

    log.verbose3f("\"%s\": RewriteChunk: track %u id %u offset 0x%" PRIx64 " size %u (0x%x)",
                  GetFile().GetFilename().c_str(),
                  m_trackId, chunkId, chunkOffset, chunkSize, chunkSize);
}

// map track type name aliases to official names


bool MP4Track::InitEditListProperties()
{
    m_pElstCountProperty = NULL;
    m_pElstMediaTimeProperty = NULL;
    m_pElstDurationProperty = NULL;
    m_pElstRateProperty = NULL;
    m_pElstReservedProperty = NULL;

    MP4Atom* pElstAtom =
        m_trakAtom.FindAtom("trak.edts.elst");

    if (!pElstAtom) {
        return false;
    }

    (void)pElstAtom->FindProperty(
        "elst.entryCount",
        (MP4Property**)&m_pElstCountProperty);
    (void)pElstAtom->FindProperty(
        "elst.entries.mediaTime",
        (MP4Property**)&m_pElstMediaTimeProperty);
    (void)pElstAtom->FindProperty(
        "elst.entries.segmentDuration",
        (MP4Property**)&m_pElstDurationProperty);
    (void)pElstAtom->FindProperty(
        "elst.entries.mediaRate",
        (MP4Property**)&m_pElstRateProperty);

    (void)pElstAtom->FindProperty(
        "elst.entries.reserved",
        (MP4Property**)&m_pElstReservedProperty);

    return m_pElstCountProperty
           && m_pElstMediaTimeProperty
           && m_pElstDurationProperty
           && m_pElstRateProperty
           && m_pElstReservedProperty;
}

MP4EditId MP4Track::AddEdit(MP4EditId editId)
{
    if (!m_pElstCountProperty) {
        (void)m_File.AddDescendantAtoms(&m_trakAtom, "edts.elst");
        if (InitEditListProperties() == false) return MP4_INVALID_EDIT_ID;
    }

    if (editId == MP4_INVALID_EDIT_ID) {
        editId = m_pElstCountProperty->GetValue() + 1;
    }

    m_pElstMediaTimeProperty->InsertValue(0, editId - 1);
    m_pElstDurationProperty->InsertValue(0, editId - 1);
    m_pElstRateProperty->InsertValue(1, editId - 1);
    m_pElstReservedProperty->InsertValue(0, editId - 1);

    m_pElstCountProperty->IncrementValue();

    return editId;
}

void MP4Track::DeleteEdit(MP4EditId editId)
{
    if (editId == MP4_INVALID_EDIT_ID) {
        throw new Exception("edit id can't be zero",
                            __FILE__, __LINE__, __FUNCTION__ );
    }

    if (!m_pElstCountProperty
            || m_pElstCountProperty->GetValue() == 0) {
        throw new Exception("no edits exist",
                            __FILE__, __LINE__, __FUNCTION__ );
    }

    m_pElstMediaTimeProperty->DeleteValue(editId - 1);
    m_pElstDurationProperty->DeleteValue(editId - 1);
    m_pElstRateProperty->DeleteValue(editId - 1);
    m_pElstReservedProperty->DeleteValue(editId - 1);

    m_pElstCountProperty->IncrementValue(-1);

    // clean up if last edit is deleted
    if (m_pElstCountProperty->GetValue() == 0) {
        m_pElstCountProperty = NULL;
        m_pElstMediaTimeProperty = NULL;
        m_pElstDurationProperty = NULL;
        m_pElstRateProperty = NULL;
        m_pElstReservedProperty = NULL;

        m_trakAtom.DeleteChildAtom(
            m_trakAtom.FindAtom("trak.edts"));
    }
}

MP4Timestamp MP4Track::GetEditStart(
    MP4EditId editId)
{
    if (editId == MP4_INVALID_EDIT_ID) {
        return MP4_INVALID_TIMESTAMP;
    } else if (editId == 1) {
        return 0;
    }
    return (MP4Timestamp)GetEditTotalDuration(editId - 1);
}

MP4Duration MP4Track::GetEditTotalDuration(
    MP4EditId editId)
{
    uint32_t numEdits = 0;

    if (m_pElstCountProperty) {
        numEdits = m_pElstCountProperty->GetValue();
    }

    if (editId == MP4_INVALID_EDIT_ID) {
        editId = numEdits;
    }

    if (numEdits == 0 || editId > numEdits) {
        return MP4_INVALID_DURATION;
    }

    MP4Duration totalDuration = 0;

    for (MP4EditId eid = 1; eid <= editId; eid++) {
        totalDuration +=
            m_pElstDurationProperty->GetValue(eid - 1);
    }

    return totalDuration;
}

MP4SampleId MP4Track::GetSampleIdFromEditTime(
    MP4Timestamp editWhen,
    MP4Timestamp* pStartTime,
    MP4Duration* pDuration)
{
    MP4SampleId sampleId = MP4_INVALID_SAMPLE_ID;
    uint32_t numEdits = 0;

    if (m_pElstCountProperty) {
        numEdits = m_pElstCountProperty->GetValue();
    }

    if (numEdits) {
        MP4Duration editElapsedDuration = 0;

        for (MP4EditId editId = 1; editId <= numEdits; editId++) {
            // remember edit segment's start time (in edit timeline)
            MP4Timestamp editStartTime =
                (MP4Timestamp)editElapsedDuration;

            // accumulate edit segment's duration
            editElapsedDuration +=
                m_pElstDurationProperty->GetValue(editId - 1);

            // calculate difference between the specified edit time
            // and the end of this edit segment
            if (editElapsedDuration - editWhen <= 0) {
                // the specified time has not yet been reached
                continue;
            }

            // 'editWhen' is within this edit segment

            // calculate the specified edit time
            // relative to just this edit segment
            MP4Duration editOffset =
                editWhen - editStartTime;

            // calculate the media (track) time that corresponds
            // to the specified edit time based on the edit list
            MP4Timestamp mediaWhen =
                m_pElstMediaTimeProperty->GetValue(editId - 1)
                + editOffset;

            // lookup the sample id for the media time
            sampleId = GetSampleIdFromTime(mediaWhen, false);

            // lookup the sample's media start time and duration
            MP4Timestamp sampleStartTime;
            MP4Duration sampleDuration;

            GetSampleTimes(sampleId, &sampleStartTime, &sampleDuration);

            // calculate the difference if any between when the sample
            // would naturally start and when it starts in the edit timeline
            MP4Duration sampleStartOffset =
                mediaWhen - sampleStartTime;

            // calculate the start time for the sample in the edit time line
            MP4Timestamp editSampleStartTime =
                editWhen - min(editOffset, sampleStartOffset);

            MP4Duration editSampleDuration = 0;

            // calculate how long this sample lasts in the edit list timeline
            if (m_pElstRateProperty->GetValue(editId - 1) == 0) {
                // edit segment is a "dwell"
                // so sample duration is that of the edit segment
                editSampleDuration =
                    m_pElstDurationProperty->GetValue(editId - 1);

            } else {
                // begin with the natural sample duration
                editSampleDuration = sampleDuration;

                // now shorten that if the edit segment starts
                // after the sample would naturally start
                if (editOffset < sampleStartOffset) {
                    editSampleDuration -= sampleStartOffset - editOffset;
                }

                // now shorten that if the edit segment ends
                // before the sample would naturally end
                if (editElapsedDuration
                        < editSampleStartTime + sampleDuration) {
                    editSampleDuration -= (editSampleStartTime + sampleDuration)
                                          - editElapsedDuration;
                }
            }

            if (pStartTime) {
                *pStartTime = editSampleStartTime;
            }

            if (pDuration) {
                *pDuration = editSampleDuration;
            }

            log.verbose2f("\"%s\": GetSampleIdFromEditTime: when %" PRIu64 " "
                          "sampleId %u start %" PRIu64 " duration %" PRId64,
                          GetFile().GetFilename().c_str(),
                          editWhen, sampleId,
                          editSampleStartTime, editSampleDuration);

            return sampleId;
        }

        throw new Exception("time out of range",
                            __FILE__, __LINE__, __FUNCTION__ );

    } else { // no edit list
        sampleId = GetSampleIdFromTime(editWhen, false);

        if (pStartTime || pDuration) {
            GetSampleTimes(sampleId, pStartTime, pDuration);
        }
    }

    return sampleId;
}

void MP4Track::CalculateBytesPerSample ()
{
    MP4Atom *pMedia = m_trakAtom.FindAtom("trak.mdia.minf.stbl.stsd");
    MP4Atom *pMediaData;
    const char *media_data_name;
    if (pMedia == NULL) return;

    if (pMedia->GetNumberOfChildAtoms() != 1) return;

    pMediaData = pMedia->GetChildAtom(0);
    media_data_name = pMediaData->GetType();
    if ((ATOMID(media_data_name) == ATOMID("twos")) ||
            (ATOMID(media_data_name) == ATOMID("sowt"))) {
        MP4IntegerProperty *chan, *sampleSize;
        chan = (MP4IntegerProperty *)pMediaData->GetProperty(4);
        sampleSize = (MP4IntegerProperty *)pMediaData->GetProperty(5);
        m_bytesPerSample = chan->GetValue() * (sampleSize->GetValue() / 8);
    }
}

MP4Duration MP4Track::GetDurationPerChunk()
{
    return m_durationPerChunk;
}

void MP4Track::SetDurationPerChunk( MP4Duration duration )
{
    m_durationPerChunk = duration;
}

///////////////////////////////////////////////////////////////////////////////

}} // namespace mp4v2::impl

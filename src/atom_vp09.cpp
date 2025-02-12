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
 * Copyright (C) Cisco Systems Inc. 2004.  All Rights Reserved.
 *
 * Contributor(s):
 */

#include "src/impl.h"

namespace mp4v2 {
namespace impl {

///////////////////////////////////////////////////////////////////////////////

MP4Vp09Atom::MP4Vp09Atom(MP4File &file)
        : MP4Atom(file, "vp09")
{
    AddReserved(*this, "reserved1", 6);
    AddProperty(new MP4Integer16Property(*this, "dataReferenceIndex"));
    AddReserved(*this, "reserved2", 16); // version, revision level, vendor, temporal quality, spatial quality
    AddProperty(new MP4Integer16Property(*this, "width"));
    AddProperty(new MP4Integer16Property(*this, "height"));
    AddReserved(*this, "reserved3", 14); // horiz and vert resolution, datasize and framecount

    MP4StringProperty* pProp = new MP4StringProperty(*this, "compressorName");
    pProp->SetFixedLength(32);
    pProp->SetCountedFormat(true);
    pProp->SetValue("vp09 Coding");
    AddProperty(pProp);

    AddReserved(*this, "reserved4", 4); // depth and color table ID

    ExpectChildAtom("vpcC", Required, OnlyOne);
    ExpectChildAtom("btrt", Optional, OnlyOne);
    ExpectChildAtom("colr", Optional, OnlyOne);
    ExpectChildAtom("pasp", Optional, OnlyOne);
}

void MP4Vp09Atom::Generate()
{
    MP4Atom::Generate();

    ((MP4Integer16Property*)m_pProperties[1])->SetValue(1);

    // property reserved3 has non-zero fixed values
    static uint8_t reserved3[14] = {
        0x00, 0x48, 0x00, 0x00,
        0x00, 0x48, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00,
        0x00, 0x01,
    };
    m_pProperties[5]->SetReadOnly(false);
    ((MP4BytesProperty*)m_pProperties[5])->SetValue(reserved3, sizeof(reserved3));
    m_pProperties[5]->SetReadOnly(true);

    // property reserved4 has non-zero fixed values
    static uint8_t reserved4[4] = {
        0x00, 0x18, 0xFF, 0xFF,
    };
    m_pProperties[7]->SetReadOnly(false);
    ((MP4BytesProperty*)m_pProperties[7])->SetValue(reserved4, sizeof(reserved4));
    m_pProperties[7]->SetReadOnly(true);
}

///////////////////////////////////////////////////////////////////////////////

}
} // namespace mp4v2::impl

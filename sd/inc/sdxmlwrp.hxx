/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * This file incorporates work covered by the following license notice:
 *
 *   Licensed to the Apache Software Foundation (ASF) under one or more
 *   contributor license agreements. See the NOTICE file distributed
 *   with this work for additional information regarding copyright
 *   ownership. The ASF licenses this file to you under the Apache
 *   License, Version 2.0 (the "License"); you may not use this file
 *   except in compliance with the License. You may obtain a copy of
 *   the License at http://www.apache.org/licenses/LICENSE-2.0 .
 */
#ifndef INCLUDED_SD_INC_SDXMLWRP_HXX
#define INCLUDED_SD_INC_SDXMLWRP_HXX

#include <comphelper/fileformat.h>
#include <tools/solar.h>
#include "sdfilter.hxx"

class ErrCode;

// SdXMLFilter
enum SdXMLFilterMode
{
    SDXMLMODE_Normal,   ///< standard load and save of the complete document
    SDXMLMODE_Organizer ///< only for import, only the styles are loaded
};

class SdXMLFilter : public SdFilter
{
public:
    SdXMLFilter(
        SfxMedium& rMedium,
        ::sd::DrawDocShell& rDocShell,
        SdXMLFilterMode eFilterMode = SDXMLMODE_Normal,
        sal_uLong nStoreVer = SOFFICE_FILEFORMAT_8 );
    virtual ~SdXMLFilter() override;

    bool        Import( ErrCode& nError );
    bool        Export() override;

private:
    SdXMLFilterMode meFilterMode;
    sal_uLong mnStoreVer;
};

#endif // INCLUDED_SD_INC_SDXMLWRP_HXX

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */

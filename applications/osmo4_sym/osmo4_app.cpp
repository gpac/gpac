/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Copyright (c) ENST 2006-200X
 *				Authors: Jean Le Feuvre 
 *					All rights reserved
 *
 *  This file is part of GPAC / Symbian GUI player
 *
 *  GPAC is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU Lesser General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your option)
 *  any later version.
 *   
 *  GPAC is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Lesser General Public License for more details.
 *   
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; see the file COPYING.  If not, write to
 *  the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA. 
 *
 */


// INCLUDE FILES
#include "osmo4_doc.h"
#include "osmo4_app.h"

// ============================ MEMBER FUNCTIONS ===============================

// UID for the application;
// this should correspond to the uid defined in the mmp file
const TUid KUidOsmo4App = { 0xf01f9075 };

// -----------------------------------------------------------------------------
// COsmo4Application::CreateDocumentL()
// Creates CApaDocument object
// -----------------------------------------------------------------------------
//
CApaDocument* COsmo4Application::CreateDocumentL()
    {
    // Create an Osmo4 document, and return a pointer to it
    return (static_cast<CApaDocument*>
                    ( COsmo4Document::NewL( *this ) ) );
    }

// -----------------------------------------------------------------------------
// COsmo4Application::AppDllUid()
// Returns application UID
// -----------------------------------------------------------------------------
//
TUid COsmo4Application::AppDllUid() const
    {
    // Return the UID for the Osmo4 application
    return KUidOsmo4App;
    }

// End of File


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
#include "osmo4_AppUi.h"
#include "osmo4_doc.h"

// ============================ MEMBER FUNCTIONS ===============================

// -----------------------------------------------------------------------------
// Cosmo4Document::NewL()
// Two-phased constructor.
// -----------------------------------------------------------------------------
//
COsmo4Document* COsmo4Document::NewL( CEikApplication&
                                                          aApp )
    {
    COsmo4Document* self = NewLC( aApp );
    CleanupStack::Pop( self );
    return self;
    }

// -----------------------------------------------------------------------------
// COsmo4Document::NewLC()
// Two-phased constructor.
// -----------------------------------------------------------------------------
//
COsmo4Document* COsmo4Document::NewLC( CEikApplication&
                                                           aApp )
    {
    COsmo4Document* self =
        new ( ELeave ) COsmo4Document( aApp );

    CleanupStack::PushL( self );
    self->ConstructL();
    return self;
    }

// -----------------------------------------------------------------------------
// COsmo4Document::ConstructL()
// Symbian 2nd phase constructor can leave.
// -----------------------------------------------------------------------------
//
void COsmo4Document::ConstructL()
    {
    // No implementation required
    }

// -----------------------------------------------------------------------------
// COsmo4Document::COsmo4Document()
// C++ default constructor can NOT contain any code, that might leave.
// -----------------------------------------------------------------------------
//
COsmo4Document::COsmo4Document( CEikApplication& aApp )
    : CAknDocument( aApp )
    {
    // No implementation required
    }

// ---------------------------------------------------------------------------
// COsmo4Document::~COsmo4Document()
// Destructor.
// ---------------------------------------------------------------------------
//
COsmo4Document::~COsmo4Document()
    {
    // No implementation required
    }

// ---------------------------------------------------------------------------
// COsmo4Document::CreateAppUiL()
// Constructs CreateAppUi.
// ---------------------------------------------------------------------------
//
CEikAppUi* COsmo4Document::CreateAppUiL()
    {
    // Create the application user interface, and return a pointer to it;
    // the framework takes ownership of this object
    return ( static_cast <CEikAppUi*> ( new ( ELeave )
                                        COsmo4AppUi ) );
    }

// End of File


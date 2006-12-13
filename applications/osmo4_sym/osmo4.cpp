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
#include <eikstart.h>
#include "osmo4_app.h"


EXPORT_C CApaApplication* NewApplication()
	{
	return new COsmo4Application;
	}

#ifdef EKA2

// EXE point
GLDEF_C TInt E32Main() {
    return EikStart::RunApplication( NewApplication );
}

#else

// DLL entry point
GLDEF_C TInt E32Dll(TDllReason /*aReason*/) {
    return KErrNone;
}

#endif

/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre 
 *			Copyright (c) Telecom ParisTech 2012
 *					All rights reserved
 *
 *  This file is part of GPAC
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

#ifndef _GF_VERSION_H

/*!
 *	\brief GPAC Version
 *	\hideinitializer
 *
 *	Macro giving GPAC version expressed as a printable string
*/
/* KEEP SPACE SEPARATORS FOR MAKE / GREP (SEE MAIN MAKEFILE & CONFIGURE & CO)
 * NO SPACE in GPAC_VERSION / GPAC_FULL_VERSION for proper install
 * SONAME versions must be digits (not strings)
 */
#define GPAC_VERSION          "0.5.1-DEV"
#define GPAC_VERSION_MAJOR 3
#define GPAC_VERSION_MINOR 0
#define GPAC_VERSION_MICRO 0

#include <gpac/revision.h>
#define GPAC_FULL_VERSION       GPAC_VERSION "-rev" GPAC_SVN_REVISION


#endif //_GF_VERSION_H


/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2012-2023
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
\file "gpac/version.h"
\brief GPAC version.
\addtogroup cst_grp

@{
*/

/*!
\brief GPAC Version
\hideinitializer
*/
/* KEEP SPACE SEPARATORS FOR MAKE / GREP (SEE MAIN MAKEFILE & CONFIGURE & CO)
 * NO SPACE in GPAC_VERSION / GPAC_FULL_VERSION for proper install
 * SONAME versions must be digits (not strings)
 */
/*! Macro giving GPAC version name expressed as a printable string*/
#define GPAC_VERSION          "2.3-DEV"

// WARNING: when bumping, reflect the changes in share/python/libgpac.py !!
/*! ABI Major number of libgpac */
#define GPAC_VERSION_MAJOR 12
/*! ABI Minor number of libgpac */
#define GPAC_VERSION_MINOR 12
/*! ABI Micro number of libgpac */
#define GPAC_VERSION_MICRO 0

/*! gets GPAC full version including GIT revision
\return GPAC full version
*/
const char *gf_gpac_version();

/*! gets GPAC copyright
\return GPAC copyright
*/
const char *gf_gpac_copyright();

/*! gets GPAC copyright + citations DOI
\return GPAC copyright
*/
const char *gf_gpac_copyright_cite();

/*!gets libgpac ABI major version
\return major number of libgpac ABI version*/
u32 gf_gpac_abi_major();

/*!gets libgpac ABI minor version
\return minor number of libgpac ABI version*/
u32 gf_gpac_abi_minor();

/*!gets libgpac ABI major version
\return micro number of libgpac ABI version*/
u32 gf_gpac_abi_micro();

/*! @} */

#endif //_GF_VERSION_H

/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2000-2012
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

#ifndef _GF_ISO_639_H
#define _GF_ISO_639_H

#ifdef __cplusplus
extern "C" {
#endif


/*!
 *	\file <gpac/iso639.h>
 *	\brief Language codes helper tools.
 */

/*!
 *	\addtogroup lang_grp Languages
 *	\ingroup utils_grp
 *	\brief Language codes helper tools
 *
 *	This section documents the language codes used in GPAC, based in ISO 639 or RFC 5646.
 *	@{
 */

#include <gpac/setup.h>

/*!
 *	Gets number of supported language codes
 *	\return the number of supported language codes
*/
u32 gf_lang_get_count();
/*!
 *	Finds language by name or code
 *	\param lang_or_rfc_5646_code the langauage name, ISO 639 code or RFC 5646 code
 *	\return the index of the language, or -1 if not supported
*/
s32 gf_lang_find(const char *lang_or_rfc_5646_code);

/*!
 *	Gets the langauge name for the given index
 *	\param lang_idx the langauge 0-based IDX
 *	\return the name of the language
*/
const char *gf_lang_get_name(u32 lang_idx);

/*!
 *	Gets the 2 character code for the given index
 *	\param lang_idx the langauge 0-based IDX
 *	\return the 2 character code of the language
*/
const char *gf_lang_get_2cc(u32 lang_idx);

/*!
 *	Gets the 3 character code for the given index
 *	\param lang_idx the langauge 0-based IDX
 *	\return the 3 character code of the language
*/
const char *gf_lang_get_3cc(u32 lang_idx);

/*! @} */

#ifdef __cplusplus
}
#endif

#endif

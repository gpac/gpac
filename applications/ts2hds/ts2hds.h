/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Author: Romain Bouqueau
 *			Copyright (c) Romain Bouqueau 2012-
 *				All rights reserved
 *
 *          Note: this development was kindly sponsorized by Vizion'R (http://vizionr.com)
 *
 *  This file is part of GPAC / TS to HDS (ts2hds) application
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

#include <gpac/media_tools.h>

//f4m
typedef struct __tag_adobe_stream AdobeStream;
typedef struct __tag_adobe_multirate AdobeMultirate;
AdobeMultirate *adobe_alloc_multirate_manifest(char *id);
void adobe_free_multirate_manifest(AdobeMultirate *am);
GF_Err adobe_gen_multirate_manifest(AdobeMultirate* am, char *bootstrap, size_t bootstrap_size);

//context
typedef struct
{
	u64 curr_time;
	u32 segnum;
	char *bootstrap;
	size_t bootstrap_size;
	AdobeMultirate *multirate_manifest;
} AdobeHDSCtx;

//f4v
GF_Err adobize_segment(GF_ISOFile *isom_file, AdobeHDSCtx *ctx);

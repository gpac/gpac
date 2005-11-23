/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean le Feuvre
 *				Copyright (c) 2005-200X ENST
 *					All rights reserved
 *
 *  This file is part of GPAC / LASeR codec sub-project
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


#ifndef _GF_LASER_DEV_H_
#define _GF_LASER_DEV_H_

#include <gpac/laser.h>

/*per_stream config support*/
typedef struct 
{
	GF_LASERConfig cfg;
	u16 ESID;
} LASeRStreamInfo;

typedef struct
{
	/*colors can be encoded on up to 16 bits per comp*/
	u16 r, g, b;
} LSRCol;

struct __tag_laser_codec
{
	GF_BitStream *bs;
	GF_SceneGraph *sg;

	FILE *trace;
	Bool force_usename;

	/*all attached streams*/
	GF_List *streamInfo;

	LASeRStreamInfo *info;
	Fixed res_factor/*2^-coord_res*/;
	/*duplicated from config*/
	u8 scale_bits;
	u8 coord_bits;
	u16 time_resolution;
	u16 color_scale;

	LSRCol *col_table;
	u32 nb_cols;
	/*computed dynamically*/
	u32 colorIndexBits;
	GF_List *font_table;
	u32 fontIndexBits;

	/*decoder only*/
	Double (*GetSceneTime)(void *cbk);
	void *cbk;
};

#endif


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

	u32 privateData_id_index, privateTag_index;

	/*decoder only*/
	Double (*GetSceneTime)(void *cbk);
	void *cbk;

	/*sameElement coding*/
	SVGgElement *prev_g;
	SVGlineElement *prev_line;
	SVGpathElement *prev_path;
	SVGpolygonElement *prev_polygon;
	SVGrectElement *prev_rect;
	SVGtextElement *prev_text;
	SVGuseElement *prev_use;
};

/*returns laser anim coding type based on field index, -1 if field cannot be animated*/
s32 gf_lsr_field_to_anim_type(GF_Node *n, u32 fieldIndex);

/*returns laser attributeName coding type based on field index, -1 if field cannot be animated*/
s32 gf_lsr_field_to_attrib_type(GF_Node *n, u32 fieldIndex);


/*start of RARE properties*/
#define RARE_AUDIO_LEVEL		1
#define RARE_COLOR				2
#define RARE_COLOR_RENDERING	3
#define RARE_DISPLAY			4
#define RARE_DISPLAY_ALIGN		5
#define RARE_FILL_OPACITY		6
#define RARE_FILL_RULE			7
#define RARE_IMAGE_RENDERING	8
#define RARE_LINE_INCREMENT		9
#define RARE_POINTER_EVENTS		10
#define RARE_SHAPE_RENDERING	11
#define RARE_SOLID_COLOR		12
#define RARE_SOLID_OPACITY		13
#define RARE_STOP_COLOR			14
#define RARE_STOP_OPACITY		15
#define RARE_STROKE_DASHARRAY	16
#define RARE_STROKE_DASHOFFSET	17
#define RARE_STROKE_LINECAP		18
#define RARE_STROKE_LINEJOIN	19
#define RARE_STROKE_MITERLIMIT	20
#define RARE_STROKE_OPACITY		21
#define RARE_STROKE_WIDTH		22
#define RARE_TEXT_ANCHOR		23
#define RARE_TEXT_RENDERING		24
#define RARE_VIEWPORT_FILL		25
#define RARE_VIEWPORT_FILL_OPACITY	26
#define RARE_VECTOR_EFFECT		27
#define RARE_VISIBILITY			28
#define RARE_FONT_FAMILY		51
#define RARE_FONT_SIZE			52
#define RARE_FONT_STYLE			53
#define RARE_FONT_WEIGHT		54
/*end of RARE properties*/
/*conditional processing*/
#define RARE_REQUIREDEXTENSIONS	29
#define RARE_REQUIREDFEATURES	30
#define RARE_REQUIREDFORMATS	31
#define RARE_SYSTEMLANGUAGE		32
/*XML*/
#define RARE_XML_BASE			33
#define RARE_XML_LANG			34
#define RARE_XML_SPACE			35
/*focus*/
#define RARE_FOCUSNEXT			36
#define RARE_FOCUSNORTH			37
#define RARE_FOCUSNORTHEAST		38
#define RARE_FOCUSNORTHWEST		39
#define RARE_FOCUSPREV			40
#define RARE_FOCUSSOUTH			41
#define RARE_FOCUSSOUTHEAST		42
#define RARE_FOCUSSOUTHWEST		43
#define RARE_FOCUSWEST			44
#define RARE_FOCUSABLE			45
#define RARE_FOCUSEAST			46
/*href*/
#define RARE_HREF_TITLE			55
#define RARE_HREF_TYPE			56
#define RARE_HREF_ROLE			57
#define RARE_HREF_ARCROLE		58
#define RARE_HREF_ACTUATE		59
#define RARE_HREF_SHOW			60
/*timing*/
#define RARE_END				61
#define RARE_MAX				62
#define RARE_MIN				63

#define RARE_TRANSFORM			47
#define RARE_LSR_ROTATION		48
#define RARE_LSR_SCALE			49
#define RARE_LSR_TRANSLATION	50


#endif


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
	GF_Err last_error;

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
	GF_Node *current_root;

	/*0: normal playback, store script content
	  1: memory decoding of scene, decompress script into commands
	*/
	Bool memory_dec;

	GF_List *defered_hrefs;
	GF_List *defered_anims;
	GF_List *defered_listeners;

	char *cache_dir, *service_name;
	GF_List *unresolved_commands;
};


/*returns laser attributeName coding type based on field index, -1 if field cannot be animated*/
s32 gf_lsr_field_to_attrib_type(GF_Node *n, u32 fieldIndex);


/*start of RARE properties*/
#define RARE_CLASS					0
#define RARE_AUDIO_LEVEL			1
#define RARE_COLOR					2
#define RARE_COLOR_RENDERING		3
#define RARE_DISPLAY				4
#define RARE_DISPLAY_ALIGN			5
#define RARE_FILL_OPACITY			6
#define RARE_FILL_RULE				7
#define RARE_IMAGE_RENDERING		8
#define RARE_LINE_INCREMENT			9
#define RARE_POINTER_EVENTS			10
#define RARE_SHAPE_RENDERING		11
#define RARE_SOLID_COLOR			12
#define RARE_SOLID_OPACITY			13
#define RARE_STOP_COLOR				14
#define RARE_STOP_OPACITY			15
#define RARE_STROKE_DASHARRAY		16
#define RARE_STROKE_DASHOFFSET		17
#define RARE_STROKE_LINECAP			18
#define RARE_STROKE_LINEJOIN		19
#define RARE_STROKE_MITERLIMIT		20
#define RARE_STROKE_OPACITY			21
#define RARE_STROKE_WIDTH			22
#define RARE_TEXT_ANCHOR			23
#define RARE_TEXT_RENDERING			24
#define RARE_VIEWPORT_FILL			25
#define RARE_VIEWPORT_FILL_OPACITY	26
#define RARE_VECTOR_EFFECT			27
#define RARE_VISIBILITY				28
#define RARE_FONT_VARIANT			50
#define RARE_FONT_FAMILY			51
#define RARE_FONT_SIZE				52
#define RARE_FONT_STYLE				53
#define RARE_FONT_WEIGHT			54
/*end of RARE properties*/
/*conditional processing*/
#define RARE_REQUIREDEXTENSIONS		29
#define RARE_REQUIREDFEATURES		30
#define RARE_REQUIREDFORMATS		31
#define RARE_SYSTEMLANGUAGE			32
/*XML*/
#define RARE_XML_BASE				33
#define RARE_XML_LANG				34
#define RARE_XML_SPACE				35
/*focus*/
#define RARE_FOCUSNEXT				36
#define RARE_FOCUSNORTH				37
#define RARE_FOCUSNORTHEAST			38
#define RARE_FOCUSNORTHWEST			39
#define RARE_FOCUSPREV				40
#define RARE_FOCUSSOUTH				41
#define RARE_FOCUSSOUTHEAST			42
#define RARE_FOCUSSOUTHWEST			43
#define RARE_FOCUSWEST				44
#define RARE_FOCUSABLE				45
#define RARE_FOCUSEAST				46
/*href*/
#define RARE_HREF_TITLE				55
#define RARE_HREF_TYPE				56
#define RARE_HREF_ROLE				57
#define RARE_HREF_ARCROLE			58
#define RARE_HREF_ACTUATE			59
#define RARE_HREF_SHOW				60
/*timing*/
#define RARE_END					61
#define RARE_MAX					62
#define RARE_MIN					63
/*transform*/
#define RARE_TRANSFORM				47



#define LSR_UPDATE_TYPE_SCALE			78
#define LSR_UPDATE_TYPE_ROTATE			75
#define LSR_UPDATE_TYPE_TRANSFORM		105
#define LSR_UPDATE_TYPE_TRANSLATION		107
#define LSR_UPDATE_TYPE_TEXT_CONTENT	104


#define LSR_UPDATE_ADD				0
#define LSR_UPDATE_CLEAN			1
#define LSR_UPDATE_DELETE			2
#define LSR_UPDATE_INSERT			3
#define LSR_UPDATE_NEW_SCENE		4
#define LSR_UPDATE_REFRESH_SCENE	5
#define LSR_UPDATE_REPLACE			6
#define LSR_UPDATE_RESTORE			7
#define LSR_UPDATE_SAVE				8
#define LSR_UPDATE_SEND_EVENT		9
#define LSR_UPDATE_EXTEND			10
#define LSR_UPDATE_TEXT_CONTENT		11


/*Code point Path code*/
enum
{
	LSR_PATH_COM_C = 0,
	LSR_PATH_COM_H,
	LSR_PATH_COM_L,
	LSR_PATH_COM_M, 
	LSR_PATH_COM_Q, 
	LSR_PATH_COM_S, 
	LSR_PATH_COM_T, 
	LSR_PATH_COM_V, 
	LSR_PATH_COM_Z, 
	LSR_PATH_COM_c, 
	LSR_PATH_COM_h, 
	LSR_PATH_COM_l, 
	LSR_PATH_COM_m, 
	LSR_PATH_COM_q, 
	LSR_PATH_COM_s, 
	LSR_PATH_COM_t,
	LSR_PATH_COM_v,
	LSR_PATH_COM_z
};

enum
{
	LSR_SCENE_CONTENT_MODEL_a = 0,
	LSR_SCENE_CONTENT_MODEL_animate,
	LSR_SCENE_CONTENT_MODEL_animateColor,
	LSR_SCENE_CONTENT_MODEL_animateMotion,
	LSR_SCENE_CONTENT_MODEL_animateTransform,
	LSR_SCENE_CONTENT_MODEL_audio,
	LSR_SCENE_CONTENT_MODEL_circle,
	LSR_SCENE_CONTENT_MODEL_defs,
	LSR_SCENE_CONTENT_MODEL_desc,
	LSR_SCENE_CONTENT_MODEL_ellipse,
	LSR_SCENE_CONTENT_MODEL_foreignObject,
	LSR_SCENE_CONTENT_MODEL_g,
	LSR_SCENE_CONTENT_MODEL_image,
	LSR_SCENE_CONTENT_MODEL_line,
	LSR_SCENE_CONTENT_MODEL_linearGradient,
	LSR_SCENE_CONTENT_MODEL_metadata,
	LSR_SCENE_CONTENT_MODEL_mpath,
	LSR_SCENE_CONTENT_MODEL_path,
	LSR_SCENE_CONTENT_MODEL_polygon,
	LSR_SCENE_CONTENT_MODEL_polyline,
	LSR_SCENE_CONTENT_MODEL_radialGradient,
	LSR_SCENE_CONTENT_MODEL_rect,
	LSR_SCENE_CONTENT_MODEL_sameg,
	LSR_SCENE_CONTENT_MODEL_sameline,
	LSR_SCENE_CONTENT_MODEL_samepath,
	LSR_SCENE_CONTENT_MODEL_samepathfill,
	LSR_SCENE_CONTENT_MODEL_samepolygon,
	LSR_SCENE_CONTENT_MODEL_samepolygonfill,
	LSR_SCENE_CONTENT_MODEL_samepolygonstroke,
	LSR_SCENE_CONTENT_MODEL_samepolyline,
	LSR_SCENE_CONTENT_MODEL_samepolylinefill,
	LSR_SCENE_CONTENT_MODEL_samepolylinestroke,
	LSR_SCENE_CONTENT_MODEL_samerect,
	LSR_SCENE_CONTENT_MODEL_samerectfill,
	LSR_SCENE_CONTENT_MODEL_sametext,
	LSR_SCENE_CONTENT_MODEL_sametextfill,
	LSR_SCENE_CONTENT_MODEL_sameuse,
	LSR_SCENE_CONTENT_MODEL_script,
	LSR_SCENE_CONTENT_MODEL_set,
	LSR_SCENE_CONTENT_MODEL_stop,
	LSR_SCENE_CONTENT_MODEL_switch,
	LSR_SCENE_CONTENT_MODEL_text,
	LSR_SCENE_CONTENT_MODEL_title,
	LSR_SCENE_CONTENT_MODEL_tspan,
	LSR_SCENE_CONTENT_MODEL_use,
	LSR_SCENE_CONTENT_MODEL_video,
	LSR_SCENE_CONTENT_MODEL_listener,
	LSR_SCENE_CONTENT_MODEL_conditional,
	LSR_SCENE_CONTENT_MODEL_cursorManager,
	LSR_SCENE_CONTENT_MODEL_element_any,
	LSR_SCENE_CONTENT_MODEL_privateContainer,
	LSR_SCENE_CONTENT_MODEL_rectClip,
	LSR_SCENE_CONTENT_MODEL_selector,
	LSR_SCENE_CONTENT_MODEL_simpleLayout,
	LSR_SCENE_CONTENT_MODEL_textContent,
	LSR_SCENE_CONTENT_MODEL_extension,
};

enum
{
	LSR_UPDATE_CONTENT_MODEL_a = 0,
	LSR_UPDATE_CONTENT_MODEL_animate,
	LSR_UPDATE_CONTENT_MODEL_animateColor,
	LSR_UPDATE_CONTENT_MODEL_animateMotion,
	LSR_UPDATE_CONTENT_MODEL_animateTransform,
	LSR_UPDATE_CONTENT_MODEL_audio,
	LSR_UPDATE_CONTENT_MODEL_circle,
	LSR_UPDATE_CONTENT_MODEL_defs,
	LSR_UPDATE_CONTENT_MODEL_desc,
	LSR_UPDATE_CONTENT_MODEL_ellipse,
	LSR_UPDATE_CONTENT_MODEL_foreignObject,
	LSR_UPDATE_CONTENT_MODEL_g,
	LSR_UPDATE_CONTENT_MODEL_image,
	LSR_UPDATE_CONTENT_MODEL_line,
	LSR_UPDATE_CONTENT_MODEL_linearGradient,
	LSR_UPDATE_CONTENT_MODEL_metadata,
	LSR_UPDATE_CONTENT_MODEL_mpath,
	LSR_UPDATE_CONTENT_MODEL_path,
	LSR_UPDATE_CONTENT_MODEL_polygon,
	LSR_UPDATE_CONTENT_MODEL_polyline,
	LSR_UPDATE_CONTENT_MODEL_radialGradient,
	LSR_UPDATE_CONTENT_MODEL_rect,
	LSR_UPDATE_CONTENT_MODEL_script,
	LSR_UPDATE_CONTENT_MODEL_set,
	LSR_UPDATE_CONTENT_MODEL_stop,
	LSR_UPDATE_CONTENT_MODEL_svg,
	LSR_UPDATE_CONTENT_MODEL_switch,
	LSR_UPDATE_CONTENT_MODEL_text,
	LSR_UPDATE_CONTENT_MODEL_title,
	LSR_UPDATE_CONTENT_MODEL_tspan,
	LSR_UPDATE_CONTENT_MODEL_use,
	LSR_UPDATE_CONTENT_MODEL_video,
	LSR_UPDATE_CONTENT_MODEL_listener,
};

enum
{
	LSR_UPDATE_CONTENT_MODEL2_conditional = 0,
	LSR_UPDATE_CONTENT_MODEL2_cursorManager,
	LSR_UPDATE_CONTENT_MODEL2_extend,
	LSR_UPDATE_CONTENT_MODEL2_private,
	LSR_UPDATE_CONTENT_MODEL2_rectClip,
	LSR_UPDATE_CONTENT_MODEL2_simpleLayout,
	LSR_UPDATE_CONTENT_MODEL2_selector,
};
#endif


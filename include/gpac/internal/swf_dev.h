/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Copyright (c) Jean Le Feuvre 2000-2005
 *					All rights reserved
 *
 *  This file is part of GPAC / Scene Management sub-project
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

#ifndef _GF_SWF_DEV_H_
#define _GF_SWF_DEV_H_

#include <gpac/scene_manager.h>
#include <gpac/color.h>


#define SWF_COLOR_SCALE				(1/256.0f)
#define SWF_TWIP_SCALE				(1/20.0f)
#define SWF_TEXT_SCALE				(1/1024.0f)


typedef struct SWFReader SWFReader;
typedef struct SWFSound SWFSound;
typedef struct SWFText SWFText;
typedef struct SWFEditText SWFEditText;
typedef struct SWF_Button SWF_Button;
typedef struct SWFShape SWFShape;
typedef struct SWFFont SWFFont;


struct SWFReader
{
	GF_SceneLoader *load;
	FILE *input;

	char *localPath;
	/*file header*/
	u8 sig[3];
	u8 version;
	u32 length;
	u32 frame_rate;
	u32 frame_count;
	Fixed width, height;

	Bool compressed, has_interact;
	
	u32 flags;
	u32 max_depth;

	GF_List *display_list;

	/*all fonts*/
	GF_List *fonts;
	/*all simple appearances (no texture)*/
	GF_List *apps;

	/*all sounds*/
	GF_List *sounds;

	/*the one and only sound stream for current timeline*/
	SWFSound *sound_stream;

	/*bit reader*/
	GF_BitStream *bs;
	
	/*current tag*/
	u32 tag, size;

	/*current BIFS stream*/
	GF_StreamContext *bifs_es;
	GF_AUContext *bifs_au;

	GF_StreamContext *bifs_dict_es;
	GF_AUContext *bifs_dict_au;

	/*for sound insert*/
	GF_Node *root;

	/*current OD AU*/
	GF_StreamContext *od_es;
	GF_AUContext *od_au;

	GF_Node *cur_shape;

	u32 current_frame;
	GF_Err ioerr;	

	/*when creating sprites: 
		1- all BIFS AUs in sprites are random access
		2- depth is ignored in Sprites
	*/
	u32 current_sprite_id;

	/*the parser can decide to remove nearly aligned pppoints in lineTo sequences*/
	/*flatten limit - 0 means no flattening*/
	Fixed flat_limit;
	/*number of points removed*/
	u32 flatten_points;

	u16 prev_od_id, prev_es_id;

	u8 *jpeg_hdr;
	u32 jpeg_hdr_size;


	/*callback functions for translator*/
	GF_Err (*set_backcol)(SWFReader *read, u32 xrgb);
	GF_Err (*show_frame)(SWFReader *read);

	/*checks if display list is large enough - returns 1 if yes, 0 otherwise (and allocate space)*/
	Bool (*allocate_depth)(SWFReader *read, u32 depth); 
	GF_Err (*place_obj)(SWFReader *read, u32 depth, u32 ID, u32 type, GF_Matrix2D *mat, GF_ColorMatrix *cmat);
	GF_Err (*remove_obj)(SWFReader *read, u32 depth);

	GF_Err (*define_shape)(SWFReader *read, SWFShape *shape, SWFFont *parent_font, Bool last_sub_shape);
	GF_Err (*define_sprite)(SWFReader *read, u32 nb_frames);
	GF_Err (*define_button)(SWFReader *read, SWF_Button *button);
	GF_Err (*define_text)(SWFReader *read, SWFText *text);
	GF_Err (*define_edit_text)(SWFReader *read, SWFEditText *text);

	GF_Err (*setup_image)(SWFReader *read, u32 ID, char *fileName);
	GF_Err (*setup_sound)(SWFReader *read, SWFSound *snd);
	GF_Err (*start_sound)(SWFReader *read, SWFSound *snd, Bool stop);

	void (*finalize)(SWFReader *read);

};


void swf_report(SWFReader *read, GF_Err e, char *format, ...);
SWFFont *swf_find_font(SWFReader *read, u32 fontID);
GF_Err swf_parse_sprite(SWFReader *read);


GF_Err swf_to_bifs_init(SWFReader *read);



typedef struct
{
	Fixed x, y;
	Fixed w, h;
} SWFRec;

typedef struct
{
	/*0: not defined, otherwise index of shape*/
	u32 nbType;
	/*0: moveTo, 1: lineTo, 2: quad curveTo*/
	u32 *types;
	SFVec2f *pts;
	u32 nbPts;
} SWFPath;

enum
{
	GF_SWF_IS_MOVE = 1,
	GF_SWF_IS_REPLACE = 1<<1,
	GF_SWF_HAD_MATRIX = 1<<2,
	GF_SWF_HAD_COLORMATRIX = 1<<3,
};

typedef struct
{
	u32 type;
	u32 solid_col;
	u32 nbGrad;
	u32 *grad_col;
	u8 *grad_ratio;
	GF_Matrix2D mat;
	u32 img_id;
	Fixed width;

	SWFPath *path;
} SWFShapeRec;

struct SWFShape 
{
	GF_List *fill_left, *fill_right, *lines;
	u32 ID;
	SWFRec rc;
};

/*SWF font object*/
struct SWFFont
{
	u32 fontID;
	u32 nbGlyphs;
	GF_List *glyphs;

	/*the following may all be overridden by a DefineFontInfo*/

	/*index -> glyph code*/
	u16 *glyph_codes;
	/*index -> glyph advance*/
	s16 *glyph_adv;

	/*font flags (SWF 3.0)*/
	Bool has_layout;
	Bool has_shiftJIS;
	Bool is_unicode, is_ansi;
	Bool is_bold, is_italic;
	s16 ascent, descent, leading;

	/*font familly*/
	char *fontName;
};

/*chunk of text with the same aspect (font, col)*/
typedef struct
{
	u32 fontID;
	u32 col;
	/*font size scaling (so that glyph coords * fontHeight is in TWIPs) */
	Fixed fontHeight;
	/*origin point in local metrics*/
	Fixed orig_x, orig_y;

	u32 nbGlyphs;
	u32 *indexes;
	Fixed *dx;
} SWFGlyphRec;

struct SWFText
{
	u32 ID;
	GF_Matrix2D mat;
	GF_List *text;
};

struct SWFEditText 
{
	u32 ID;
	char *init_value;
	Bool word_wrap, multiline, password, read_only, auto_size, no_select, html, outlines, has_layout, border;
	u32 color;
	Fixed max_length, font_height;
	u32 fontID;

	u32 align;
	Fixed left, right, indent, leading;
};


enum
{
	SWF_SND_UNCOMP = 0,
	SWF_SND_ADPCM,
	SWF_SND_MP3
};

struct SWFSound
{
	u32 ID;
	u8 format;
	/*0: 5.5k - 1: 11k - 2: 22k - 3: 44k*/
	u8 sound_rate;
	u8 bits_per_sample;
	Bool stereo;
	u16 sample_count;
	u32 frame_delay_ms;

	/*IO*/
	FILE *output;
	char *szFileName;

	/*set when sound is setup (OD inserted)*/
	Bool is_setup;
};

typedef struct 
{
	Bool hitTest, down, over, up;
	u32 character_id;
	u16 depth;
	GF_Matrix2D mx;
	GF_ColorMatrix cmx;
} SWF_ButtonRecord;


struct SWF_Button 
{
	u32 count;
	SWF_ButtonRecord buttons[40];
	u32 ID;
};


enum
{
	SWF_END = 0,
	SWF_SHOWFRAME = 1,
	SWF_DEFINESHAPE = 2,
	SWF_FREECHARACTER = 3,
	SWF_PLACEOBJECT = 4,
	SWF_REMOVEOBJECT = 5,
	SWF_DEFINEBITS = 6,
	SWF_DEFINEBITSJPEG = 6,
	SWF_DEFINEBUTTON = 7,
	SWF_JPEGTABLES = 8,
	SWF_SETBACKGROUNDCOLOR = 9,
	SWF_DEFINEFONT = 10,
	SWF_DEFINETEXT = 11,
	SWF_DOACTION = 12,
	SWF_DEFINEFONTINFO = 13,
	SWF_DEFINESOUND = 14,
	SWF_STARTSOUND = 15,
	SWF_DEFINEBUTTONSOUND = 17,
	SWF_SOUNDSTREAMHEAD = 18,
	SWF_SOUNDSTREAMBLOCK = 19,
	SWF_DEFINEBITSLOSSLESS = 20,
	SWF_DEFINEBITSJPEG2 = 21,
	SWF_DEFINESHAPE2 = 22,
	SWF_DEFINEBUTTONCXFORM = 23,
	SWF_PROTECT = 24,
	SWF_PLACEOBJECT2 = 26,
	SWF_REMOVEOBJECT2 = 28,
	SWF_DEFINESHAPE3 = 32,
	SWF_DEFINETEXT2 = 33,
	SWF_DEFINEBUTTON2 = 34,
	SWF_DEFINEBITSJPEG3 = 35,
	SWF_DEFINEBITSLOSSLESS2 = 36,
	SWF_DEFINEEDITTEXT = 37,
	SWF_DEFINEMOVIE = 38,
	SWF_DEFINESPRITE = 39,
	SWF_NAMECHARACTER = 40,
	SWF_SERIALNUMBER = 41,
	SWF_GENERATORTEXT = 42,
	SWF_FRAMELABEL = 43,
	SWF_SOUNDSTREAMHEAD2 = 45,
	SWF_DEFINEMORPHSHAPE = 46,
	SWF_DEFINEFONT2 = 48,
	SWF_TEMPLATECOMMAND = 49,
	SWF_GENERATOR3 = 51,
	SWF_EXTERNALFONT = 52,
	SWF_EXPORTASSETS = 56,
	SWF_IMPORTASSETS	= 57,
	SWF_ENABLEDEBUGGER = 58,
	SWF_MX0 = 59,
	SWF_MX1 = 60,
	SWF_MX2 = 61,
	SWF_MX3 = 62,
	SWF_MX4 = 63,
	SWF_REFLEX = 777
};


#endif /*_GF_SWF_DEV_H_*/

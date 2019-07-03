/*
 *          GPAC - Multimedia Framework C SDK
 *
 *          Authors: Cyril Concolato
 *          Copyright (c) Telecom ParisTech 2000-2012
 *                  All rights reserved
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
#include <gpac/utf.h>
#include <gpac/xml.h>
#include <gpac/internal/swf_dev.h>
#include <gpac/internal/scenegraph_dev.h>
#include <gpac/internal/isomedia_dev.h>
#include <gpac/internal/media_dev.h>
#include <gpac/constants.h>

#ifndef GPAC_DISABLE_VVG

#ifndef GPAC_DISABLE_SWF_IMPORT

#define SWF_TEXT_SCALE              (1/1024.0f)

typedef struct _swf_svg_sample
{
	u64 start;
	u64 end;
	char *data;
} GF_SWF_SVG_Sample;

static void swf_svg_print(SWFReader *read, const char *format, ...) {
	char line[2000];
	u32 line_length;
	u32 new_size;
	va_list args;

	/* print the line */
	va_start(args, format);
	vsnprintf(line, 2000, format, args);
	va_end(args);
	/* add the line to the buffer */
	line_length = (u32)strlen(line);
	new_size = read->svg_data_size+line_length;
	read->svg_data = (char *)gf_realloc(read->svg_data, new_size+1);
	if (read->print_frame_header) {
		/* write at the beginning of the buffer */
		memmove(read->svg_data+read->frame_header_offset+line_length, read->svg_data+read->frame_header_offset, (read->svg_data_size-read->frame_header_offset)+1);
		memcpy(read->svg_data+read->frame_header_offset, line, line_length);
		read->frame_header_offset += line_length;
	} else {
		strcpy(read->svg_data+read->svg_data_size, line);
	}
	read->svg_data_size = new_size;
}

static void swf_svg_print_color(SWFReader *read, u32 ARGB)
{
	SFColor val;
	val.red = INT2FIX((ARGB>>16)&0xFF) / 255*100;
	val.green = INT2FIX((ARGB>>8)&0xFF) / 255*100;
	val.blue = INT2FIX((ARGB)&0xFF) / 255*100;
	swf_svg_print(read, "rgb(%g%%,%g%%,%g%%)", FIX2FLT(val.red), FIX2FLT(val.green), FIX2FLT(val.blue));
}

static void swf_svg_print_alpha(SWFReader *read, u32 ARGB)
{
	Fixed alpha;
	alpha = INT2FIX((ARGB>>24)&0xFF)/255;
	swf_svg_print(read, "%g", FIX2FLT(alpha));
}

static void swg_svg_print_shape_record_to_fill_stroke(SWFReader *read, SWFShapeRec *srec, Bool is_fill)
{
	/*get regular appearance reuse*/
	if (is_fill) {
		switch (srec->type) {
		/*solid/alpha fill*/
		case 0x00:
			swf_svg_print(read, "fill=\"");
			swf_svg_print_color(read, srec->solid_col);
			swf_svg_print(read, "\" ");
			swf_svg_print(read, "fill-opacity=\"");
			swf_svg_print_alpha(read, srec->solid_col);
			swf_svg_print(read, "\" ");
			break;
		case 0x10:
		case 0x12:
		//if (read->flags & GF_SM_SWF_NO_GRADIENT) {
		//	u32 col = srec->grad_col[srec->nbGrad/2];
		//	col |= 0xFF000000;
		//	n->appearance = s2b_get_appearance(read, (GF_Node *) n, col, 0, 0);
		//} else {
		//	n->appearance = s2b_get_gradient(read, (GF_Node *) n, shape, srec);
		//}
		//break;
		case 0x40:
		case 0x41:
		case 0x42:
		case 0x43:
		//n->appearance = s2b_get_bitmap(read, (GF_Node *) n, shape, srec);
		//break;
		default:
			swf_report(read, GF_NOT_SUPPORTED, "fill_style %x not supported", srec->type);
			break;
		}
	} else {
		swf_svg_print(read, "fill=\"none\" ");
		swf_svg_print(read, "stroke=\"");
		swf_svg_print_color(read, srec->solid_col);
		swf_svg_print(read, "\" ");
		swf_svg_print(read, "stroke-opacity=\"");
		swf_svg_print_alpha(read, srec->solid_col);
		swf_svg_print(read, "\" ");
		swf_svg_print(read, "stroke-width=\"%g\" ", FIX2FLT(srec->width));
	}
}

static void swf_svg_print_shape_record_to_path_d(SWFReader *read, SWFShapeRec *srec)
{
	u32     pt_idx;
	u32     i;

	pt_idx = 0;
	for (i=0; i<srec->path->nbType; i++) {
		switch (srec->path->types[i]) {
		/*moveTo*/
		case 0:
			swf_svg_print(read, "M%g,%g", FIX2FLT(srec->path->pts[pt_idx].x), FIX2FLT(srec->path->pts[pt_idx].y));
			pt_idx++;
			break;
		/*lineTo*/
		case 1:
			swf_svg_print(read, "L%g,%g", FIX2FLT(srec->path->pts[pt_idx].x), FIX2FLT(srec->path->pts[pt_idx].y));
			pt_idx++;
			break;
		/*curveTo*/
		case 2:
			swf_svg_print(read, "Q%g,%g", FIX2FLT(srec->path->pts[pt_idx].x), FIX2FLT(srec->path->pts[pt_idx].y));
			pt_idx++;
			swf_svg_print(read, ",%g,%g", FIX2FLT(srec->path->pts[pt_idx].x), FIX2FLT(srec->path->pts[pt_idx].y));
			pt_idx++;
			break;
		}
	}
}

static void swf_svg_print_matrix(SWFReader *read, GF_Matrix2D *mat)
{
	if (!gf_mx2d_is_identity(*mat))
	{
		GF_Point2D  scale;
		GF_Point2D  translate;
		Fixed       rotate;
		if( gf_mx2d_decompose(mat, &scale, &rotate, &translate))
		{
			swf_svg_print(read, "transform=\"");
			if (translate.x != 0 || translate.y != 0)
			{
				swf_svg_print(read, "translate(%g, %g) ", FIX2FLT(translate.x), FIX2FLT(translate.y));
			}
			if (rotate != 0)
			{
				swf_svg_print(read, "rotate(%g) ", FIX2FLT(rotate));
			}
			if (scale.x != FIX_ONE || scale.y != FIX_ONE)
			{
				swf_svg_print(read, "scale(%g, %g) ", FIX2FLT(scale.x), FIX2FLT(scale.y));
			}
			swf_svg_print(read, "\" ");
		}
		else
		{
			swf_svg_print(read, "transform=\"matrix(%g,%g,%g,%g,%g,%g)\" ", FIX2FLT(mat->m[0]), FIX2FLT(mat->m[3]), FIX2FLT(mat->m[1]), FIX2FLT(mat->m[4]), FIX2FLT(mat->m[2]), FIX2FLT(mat->m[5]) );
		}
	}
}

/*translates Flash to SVG shapes*/
static GF_Err swf_svg_define_shape(SWFReader *read, SWFShape *shape, SWFFont *parent_font, Bool last_sub_shape)
{
	u32 i;
	SWFShapeRec *srec;

	if (parent_font && (read->flags & GF_SM_SWF_NO_FONT))
	{
		return GF_OK;
	}

	if (!read->svg_shape_started)
	{
		swf_svg_print(read, "<defs>\n");
		if (!parent_font)
		{
			swf_svg_print(read, "<g id=\"S%d\" >\n", shape->ID);
		}
		else
		{
			char    szGlyphId[256];
			sprintf(szGlyphId, "Font%d_Glyph%d", parent_font->fontID, gf_list_count(parent_font->glyphs));
			swf_svg_print(read, "<g id=\"%s\" >\n", szGlyphId);
			gf_list_add(parent_font->glyphs, szGlyphId);
		}
	}
	read->empty_frame = GF_FALSE;
	read->svg_shape_started = GF_TRUE;

	i=0;
	while ((srec = (SWFShapeRec*)gf_list_enum(shape->fill_left, &i))) {
		swf_svg_print(read, "<path d=\"");
		swf_svg_print_shape_record_to_path_d(read, srec);
		swf_svg_print(read, "\" ");
		swg_svg_print_shape_record_to_fill_stroke(read, srec, 1);
		swf_svg_print(read, "/>\n");
	}
	i=0;
	while ((srec = (SWFShapeRec*)gf_list_enum(shape->lines, &i))) {
		swf_svg_print(read, "<path d=\"");
		swf_svg_print_shape_record_to_path_d(read, srec);
		swf_svg_print(read, "\" ");
		swg_svg_print_shape_record_to_fill_stroke(read, srec, 0);
		swf_svg_print(read, "/>\n");
	}

	if (last_sub_shape)
	{
		read->svg_shape_started = GF_FALSE;
		swf_svg_print(read, "</g>\n");
		swf_svg_print(read, "</defs>\n");
	}
	return GF_OK;
}

static GF_Err swf_svg_define_text(SWFReader *read, SWFText *text)
{
	Bool            use_text;
	u32             i;
	u32             j;
	SWFGlyphRec     *gr;
	SWFFont         *ft;

	use_text = (read->flags & GF_SM_SWF_NO_FONT) ? 1 : 0;

	swf_svg_print(read, "<defs>\n");
	swf_svg_print(read, "<g id=\"S%d\" ", text->ID);
	swf_svg_print_matrix(read, &text->mat);
	swf_svg_print(read, ">\n");

	i=0;
	while ((gr = (SWFGlyphRec*)gf_list_enum(text->text, &i)))
	{
		ft = NULL;
		if (use_text) {
			ft = swf_find_font(read, gr->fontID);
			if (!ft->glyph_codes) {
				use_text = 0;
				swf_report(read, GF_BAD_PARAM, "Font glyphs are not defined, cannot reference extern font - Forcing glyph embedding");
			}
		}
		if (use_text) {
			/*restore back the font height in pixels (it's currently in SWF glyph design units)*/
			swf_svg_print(read, "<text ");
			swf_svg_print(read, "x=\"%g \" ", FIX2FLT(gr->orig_x));
			swf_svg_print(read, "y=\"%g \" ", FIX2FLT(gr->orig_y));
			swf_svg_print(read, "font-size=\"%d\" ", (u32)(gr->fontSize * SWF_TWIP_SCALE));
			if (ft->fontName)
			{
				swf_svg_print(read, "font-family=\"%s\" ", ft->fontName);
			}
			if (ft->is_italic)
			{
				swf_svg_print(read, "font-style=\"italic\" ");
			}
			if (ft->is_bold)
			{
				swf_svg_print(read, "font-weight=\"bold\" ");
			}
			swf_svg_print(read, ">");
			/*convert to UTF-8*/
			{
				size_t _len;
				u16     *str_w;
				u16     *widestr;
				char    *str;

				str_w = (u16*)gf_malloc(sizeof(u16) * (gr->nbGlyphs+1));
				for (j=0; j<gr->nbGlyphs; j++)
				{
					str_w[j] = ft->glyph_codes[gr->indexes[j]];
				}
				str_w[j] = 0;
				str = (char*)gf_malloc(sizeof(char) * (gr->nbGlyphs+2));
				widestr = str_w;
				_len = gf_utf8_wcstombs(str, sizeof(u8) * (gr->nbGlyphs+1), (const unsigned short **) &widestr);
				if (_len != (size_t) -1) {
					str[(u32) _len] = 0;
					swf_svg_print(read, "%s", str);
				}
			}
			swf_svg_print(read, "</text>\n");
		}
		else
		{
			/*convert glyphs*/
			Fixed       dx;
			swf_svg_print(read, "<g tranform=\"scale(1,-1) ");
			swf_svg_print(read, "translate(%g, %g)\" >\n", FIX2FLT(gr->orig_x), FIX2FLT(gr->orig_y));

			dx = 0;
			for (j=0; j<gr->nbGlyphs; j++)
			{
				swf_svg_print(read, "<use xlink:href=\"#Font%d_Glyph%d\" transform=\"translate(%g)\" />\n", gr->fontID, gr->indexes[j], FIX2FLT(gf_divfix(dx, FLT2FIX(gr->fontSize * SWF_TEXT_SCALE))));
				dx += gr->dx[j];
			}
			swf_svg_print(read, "</g>\n");
		}
	}
	read->empty_frame = GF_FALSE;
	swf_svg_print(read, "</g>\n");
	swf_svg_print(read, "</defs>\n");
	return GF_OK;
}

static GF_Err swf_svg_define_edit_text(SWFReader *read, SWFEditText *text)
{
	return GF_OK;
}

#if 0
/*called upon end of sprite or clip*/
static void swf_svg_end_of_clip(SWFReader *read)
{
}
#endif

static Bool swf_svg_allocate_depth(SWFReader *read, u32 depth)
{
	return GF_FALSE;
}

static GF_Err swf_svg_define_sprite(SWFReader *read, u32 nb_frames)
{
	return GF_OK;
}

static GF_Err swf_svg_setup_sound(SWFReader *read, SWFSound *snd, Bool soundstream_first_block)
{
	return GF_OK;
}

static GF_Err swf_svg_setup_image(SWFReader *read, u32 ID, char *fileName)
{
	swf_svg_print(read, "<defs>\n");
	swf_svg_print(read, "<image id=\"S%d\" xlink:href=\"\"/>", ID, fileName);
	swf_svg_print(read, "</defs>\n");
	return GF_OK;
}

static GF_Err swf_svg_set_backcol(SWFReader *read, u32 xrgb)
{

	//rgb.red = INT2FIX((xrgb>>16) & 0xFF) / 255;
	//rgb.green = INT2FIX((xrgb>>8) & 0xFF) / 255;
	//rgb.blue = INT2FIX((xrgb) & 0xFF) / 255;
	return GF_OK;
}

static GF_Err swf_svg_start_sound(SWFReader *read, SWFSound *snd, Bool stop)
{
	return GF_OK;
}

static GF_Err swf_svg_place_obj(SWFReader *read, u32 depth, u32 ID, u32 prev_id, u32 type, GF_Matrix2D *mat, GF_ColorMatrix *cmat, GF_Matrix2D *prev_mat, GF_ColorMatrix *prev_cmat)
{
	read->empty_frame = GF_FALSE;
	return GF_OK;
}

static GF_Err swf_svg_remove_obj(SWFReader *read, u32 depth, u32 ID)
{
	read->empty_frame = GF_FALSE;
	return GF_OK;
}

static GF_Err swf_svg_show_frame(SWFReader *read)
{
	u32     i;
	u32     len;
	GF_List *sdl = gf_list_new(); // sorted display list

	/* sorting the display list because SVG/CSS z-index is not well supported */
	while (gf_list_count(read->display_list))
	{
		Bool        inserted = GF_FALSE;
		DispShape   *s;

		s = (DispShape *)gf_list_get(read->display_list, 0);
		gf_list_rem(read->display_list, 0);

		for (i = 0; i < gf_list_count(sdl); i++)
		{
			DispShape *s2 = (DispShape *)gf_list_get(sdl, i);
			if (s->depth < s2->depth)
			{
				gf_list_insert(sdl, s, i);
				inserted = GF_TRUE;
				break;
			}
		}
		if (!inserted)
		{
			gf_list_add(sdl, s);
		}
	}
	gf_list_del(read->display_list);
	read->display_list = sdl;

	/* dumping the display list */
	len = gf_list_count(read->display_list);
	for (i=0; i<len; i++)
	{
		DispShape   *s;
		s = (DispShape *)gf_list_get(read->display_list, i);
		swf_svg_print(read, "<use xlink:href=\"#S%d\" ", s->char_id);
		//swf_svg_print(read, "z-index=\"%d\" ", s->depth);
		swf_svg_print_matrix(read, &s->mat);
		swf_svg_print(read, "/>\n");
		read->empty_frame = GF_FALSE;
	}
	if (!read->empty_frame) {
		read->print_frame_header = GF_TRUE;
		read->frame_header_offset = 0;
		swf_svg_print(read, "<g display=\"none\">\n");
		swf_svg_print(read, "<animate id=\"frame%d_anim\" attributeName=\"display\" to=\"inline\" ", read->current_frame);
		swf_svg_print(read, "begin=\"%g\" ", 1.0*(read->current_frame)/read->frame_rate);
		if (read->current_frame+1 < read->frame_count) {
			swf_svg_print(read, "end=\"frame%d_anim.begin\" fill=\"remove\" ", (read->current_frame+1));
		} else {
			swf_svg_print(read, "fill=\"freeze\" ");
		}
		swf_svg_print(read, "/>\n");
		read->print_frame_header = GF_FALSE;

		swf_svg_print(read, "</g>\n");
	}
	read->add_sample(read->user, read->svg_data, read->svg_data_size, read->current_frame*1000/read->frame_rate, (read->current_frame == 0));
	gf_free(read->svg_data);
	read->svg_data = NULL;
	read->svg_data_size = 0;

	read->empty_frame = GF_TRUE;
	return GF_OK;
}

static void swf_svg_finalize(SWFReader *read)
{
	swf_svg_print(read, "</svg>\n");
	read->add_header(read->user, read->svg_data, read->svg_data_size, GF_FALSE);
	gf_free(read->svg_data);
	read->svg_data = NULL;
	read->svg_data_size = 0;
}

static GF_Err swf_svg_define_button(SWFReader *read, SWF_Button *btn)
{
	return GF_OK;
}

static Bool swf_svg_action(SWFReader *read, SWFAction *act)
{
	return GF_TRUE;
}

GF_Err swf_to_svg_init(SWFReader *read, u32 swf_flags, Float swf_flatten_angle)
{
	if (!read->user) return GF_BAD_PARAM;

	/*init callbacks*/
	read->show_frame = swf_svg_show_frame;
	read->allocate_depth = swf_svg_allocate_depth;
	read->place_obj = swf_svg_place_obj;
	read->remove_obj = swf_svg_remove_obj;
	read->define_shape = swf_svg_define_shape;
	read->define_sprite = swf_svg_define_sprite;
	read->set_backcol = swf_svg_set_backcol;
	read->define_button = swf_svg_define_button;
	read->define_text = swf_svg_define_text;
	read->define_edit_text = swf_svg_define_edit_text;
	read->setup_sound = swf_svg_setup_sound;
	read->start_sound = swf_svg_start_sound;
	read->setup_image = swf_svg_setup_image;
	read->action = swf_svg_action;
	read->finalize = swf_svg_finalize;

	read->flags = swf_flags;
	read->flat_limit = FLT2FIX(swf_flatten_angle);

	read->print_stream_header = GF_TRUE;
	swf_svg_print(read, "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n");
	swf_svg_print(read, "<svg xmlns=\"http://www.w3.org/2000/svg\" ");
	swf_svg_print(read, "xmlns:xlink=\"http://www.w3.org/1999/xlink\" ");
	swf_svg_print(read, "width=\"100%%\" ");
	swf_svg_print(read, "height=\"100%%\" ");
	swf_svg_print(read, "viewBox=\"0 0 %d %d\" ", FIX2INT(read->width), FIX2INT(read->height));
	swf_svg_print(read, "viewport-fill=\"rgb(255,255,255)\" ");
	swf_svg_print(read, ">\n");
	read->print_stream_header = GF_FALSE;

	/* update sample description */
	read->add_header(read->user, read->svg_data, read->svg_data_size, GF_TRUE);
	gf_free(read->svg_data);
	read->svg_data = NULL;
	read->svg_data_size = 0;

	return GF_OK;
}

GF_Err swf_svg_write_text_sample(void *user, const u8 *data, u32 length, u64 timestamp, Bool isRap)
{
	FILE *svgFile = (FILE *)user;
	u32  lengthWritten;

	lengthWritten = (u32)fwrite(data, 1, length, svgFile);
	if (length != lengthWritten) {
		return GF_BAD_PARAM;
	} else {
		return GF_OK;
	}
}

GF_Err swf_svg_write_text_header(void *user, const u8 *data, u32 length, Bool isHeader)
{
	FILE *svgFile = (FILE *)user;
	u32  lengthWritten;

	lengthWritten = (u32)fwrite(data, 1, length, svgFile);
	if (length != lengthWritten) {
		return GF_BAD_PARAM;
	} else {
		return GF_OK;
	}
}
#endif /*GPAC_DISABLE_SWF_IMPORT*/

#endif /*GPAC_DISABLE_SVG*/

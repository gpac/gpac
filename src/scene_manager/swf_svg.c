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

#ifndef GPAC_DISABLE_VRML

#ifndef GPAC_DISABLE_SWF_IMPORT

#define SWF_TEXT_SCALE              (1/1024.0f)

typedef struct
{
    u32 btn_id;
    u32 sprite_up_id;
} s2sBtnRec;

static void swf_svg_print_color(SWFReader *read, u32 ARGB)
{
    SFColor val;
    val.red = INT2FIX((ARGB>>16)&0xFF) / 255*100;
    val.green = INT2FIX((ARGB>>8)&0xFF) / 255*100;
    val.blue = INT2FIX((ARGB)&0xFF) / 255*100;
    fprintf(read->svg_output, "rgb(%f%%,%f%%,%f%%)", FIX2FLT(val.red), FIX2FLT(val.green), FIX2FLT(val.blue));
}

static void swf_svg_print_alpha(SWFReader *read, u32 ARGB)
{
    Fixed alpha;
    alpha = INT2FIX((ARGB>>24)&0xFF)/255;
    fprintf(read->svg_output, "%f", FIX2FLT(alpha));
}

static void swg_svg_print_shape_record_to_fill_stroke(SWFReader *read, SWFShapeRec *srec, Bool is_fill)
{
	/*get regular appearance reuse*/
	if (is_fill) {
		switch (srec->type) {
		/*solid/alpha fill*/
		case 0x00:
            fprintf(read->svg_output, "fill=\"");
            swf_svg_print_color(read, srec->solid_col);
            fprintf(read->svg_output, "\" ");
            fprintf(read->svg_output, "fill-opacity=\"");
            swf_svg_print_alpha(read, srec->solid_col);
            fprintf(read->svg_output, "\" ");
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
        fprintf(read->svg_output, "fill=\"none\" ");
        fprintf(read->svg_output, "stroke=\"");
        swf_svg_print_color(read, srec->solid_col);
        fprintf(read->svg_output, "\" ");
        fprintf(read->svg_output, "stroke-opacity=\"");
        swf_svg_print_alpha(read, srec->solid_col);
        fprintf(read->svg_output, "\" ");
        fprintf(read->svg_output, "stroke-width=\"%f\" ", FIX2FLT(srec->width));
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
            fprintf(read->svg_output, "M%f,%f", FIX2FLT(srec->path->pts[pt_idx].x), FIX2FLT(srec->path->pts[pt_idx].y));
            pt_idx++;
            break;
        /*lineTo*/
        case 1:
            fprintf(read->svg_output, "L%f,%f", FIX2FLT(srec->path->pts[pt_idx].x), FIX2FLT(srec->path->pts[pt_idx].y));
            pt_idx++;
            break;
        /*curveTo*/
        case 2:
            fprintf(read->svg_output, "Q%f,%f", FIX2FLT(srec->path->pts[pt_idx].x), FIX2FLT(srec->path->pts[pt_idx].y));
            pt_idx++;
            fprintf(read->svg_output, ",%f,%f", FIX2FLT(srec->path->pts[pt_idx].x), FIX2FLT(srec->path->pts[pt_idx].y));
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
            fprintf(read->svg_output, "transform=\"");
            if (translate.x != 0 || translate.y != 0)
            {
                fprintf(read->svg_output, "translate(%f, %f) ", FIX2FLT(translate.x), FIX2FLT(translate.y));
            }
            if (rotate != 0)
            {
                fprintf(read->svg_output, "rotate(%f) ", FIX2FLT(rotate));
            }
            if (scale.x != FIX_ONE || scale.y != FIX_ONE)
            {
                fprintf(read->svg_output, "scale(%f, %f) ", FIX2FLT(scale.x), FIX2FLT(scale.y));
            }
            fprintf(read->svg_output, "\" ");
        } 
        else 
        {
            fprintf(read->svg_output, "transform=\"matrix(%f,%f,%f,%f,%f,%f)\" ", FIX2FLT(mat->m[0]), FIX2FLT(mat->m[3]), FIX2FLT(mat->m[1]), FIX2FLT(mat->m[4]), FIX2FLT(mat->m[2]), FIX2FLT(mat->m[5]) );
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

    if (!read->cur_shape) 
    {
        fprintf(read->svg_output, "<defs>\n");
        if (!parent_font)
        {
            fprintf(read->svg_output, "<g id=\"S%d\" >\n", shape->ID);
        }
        else
        {
            char    szGlyphId[256];
            sprintf(szGlyphId, "Font%d_Glyph%d", parent_font->fontID, gf_list_count(parent_font->glyphs));
            fprintf(read->svg_output, "<g id=\"%s\" >\n", szGlyphId);
            gf_list_add(parent_font->glyphs, szGlyphId);
        }
    }
    read->cur_shape = (GF_Node *)"shape";

    i=0;
    while ((srec = (SWFShapeRec*)gf_list_enum(shape->fill_left, &i))) {
        fprintf(read->svg_output, "<path d=\"");
        swf_svg_print_shape_record_to_path_d(read, srec);
        fprintf(read->svg_output, "\" ");
        swg_svg_print_shape_record_to_fill_stroke(read, srec, 1);
        fprintf(read->svg_output, "/>\n");
    }
    i=0;
    while ((srec = (SWFShapeRec*)gf_list_enum(shape->lines, &i))) {
        fprintf(read->svg_output, "<path d=\"");
        swf_svg_print_shape_record_to_path_d(read, srec);
        fprintf(read->svg_output, "\" ");
        swg_svg_print_shape_record_to_fill_stroke(read, srec, 0);
        fprintf(read->svg_output, "/>\n");
    }

    if (last_sub_shape) 
    {
        read->cur_shape = NULL;
        fprintf(read->svg_output, "</g>\n");
        fprintf(read->svg_output, "</defs>\n");
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

    fprintf(read->svg_output, "<defs>\n");
    fprintf(read->svg_output, "<g id=\"S%d\" ", text->ID);
    swf_svg_print_matrix(read, &text->mat);
    fprintf(read->svg_output, ">\n");

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
            fprintf(read->svg_output, "<text ");
            fprintf(read->svg_output, "x=\"%f \" ", FIX2FLT(gr->orig_x));
            fprintf(read->svg_output, "y=\"%f \" ", FIX2FLT(gr->orig_y));
            fprintf(read->svg_output, "font-size=\"%d\" ", (u32)(gr->fontSize * SWF_TWIP_SCALE));
            if (ft->fontName)
            {
                fprintf(read->svg_output, "font-family=\"%s\" ", ft->fontName);
            }
            if (ft->is_italic) 
            {
                fprintf(read->svg_output, "font-style=\"italic\" ");
            }
            if (ft->is_bold) 
            {
                fprintf(read->svg_output, "font-weight=\"bold\" ");
            }
            fprintf(read->svg_output, ">");
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
                    fprintf(read->svg_output, "%s", str);
                }
            }
            fprintf(read->svg_output, "</text>\n");
        }
        else
        {
            /*convert glyphs*/
            Fixed       dx;
            fprintf(read->svg_output, "<g tranform=\"scale(1,-1) ");
            fprintf(read->svg_output, "translate(%f, %f)\" >\n", FIX2FLT(gr->orig_x), FIX2FLT(gr->orig_y));

            dx = 0;
            for (j=0; j<gr->nbGlyphs; j++) 
            {
                fprintf(read->svg_output, "<use xlink:href=\"#Font%d_Glyph%d\" transform=\"translate(%f)\" />\n", gr->fontID, gr->indexes[j], FIX2FLT(gf_divfix(dx, FLT2FIX(gr->fontSize * SWF_TEXT_SCALE))));
                dx += gr->dx[j];
            }
            fprintf(read->svg_output, "</g>\n");
        }
    }
    fprintf(read->svg_output, "</g>\n");
    fprintf(read->svg_output, "</defs>\n");
    return GF_OK;
}

static GF_Err swf_svg_define_edit_text(SWFReader *read, SWFEditText *text)
{
    //char styles[1024];
    //char *ptr;
    //Bool use_layout;
    //M_Layout *layout = NULL;
    //M_Shape *txt;
    //M_Text *t;
    //M_FontStyle *f;
    //M_Transform2D *tr;

    //tr = (M_Transform2D *) s2s_new_node(read, TAG_MPEG4_Transform2D);
    //tr->scale.y = -FIX_ONE;

    //use_layout = 0;
    //if (text->align==3) use_layout = 1;
    //else if (text->multiline) use_layout = 1;

    //if (use_layout) {
    //  layout = (M_Layout *) s2s_new_node(read, TAG_MPEG4_Layout);
    //  tr->translation.x = read->width/2;
    //  tr->translation.y = read->height/2;
    //}

    //t = (M_Text *) s2s_new_node(read, TAG_MPEG4_Text);
    //f = (M_FontStyle *) s2s_new_node(read, TAG_MPEG4_FontStyle);
    //t->fontStyle = (GF_Node *) f;
    //gf_node_register(t->fontStyle, (GF_Node *) t);

    ///*restore back the font height in pixels (it's currently in SWF glyph design units)*/
    //f->size = text->font_height;
    //f->spacing = text->font_height + text->leading;

    //gf_sg_vrml_mf_reset(&f->justify, GF_SG_VRML_MFSTRING);
    //gf_sg_vrml_mf_append(&f->justify, GF_SG_VRML_MFSTRING, (void**)&ptr);
    //switch (text->align) {
    //case 0:
    //  ((SFString*)ptr)->buffer = gf_strdup("BEGIN"); 
    //  break;
    //case 1:
    //  ((SFString*)ptr)->buffer = gf_strdup("END"); 
    //  break;
    //case 3:
    //  ((SFString*)ptr)->buffer = gf_strdup("JUSTIFY"); 
    //  break;
    //default:
    //  ((SFString*)ptr)->buffer = gf_strdup("MIDDLE"); 
    //  break;
    //}

    //strcpy(styles, "");
    //if (!text->read_only) strcat(styles, "EDITABLE");
    //if (text->password) strcat(styles, "PASSWORD");
    //
    //if (f->style.buffer) gf_free(f->style.buffer);
    //f->style.buffer = gf_strdup(styles);

    //if (text->init_value) {
    //  gf_sg_vrml_mf_reset(&t->string, GF_SG_VRML_MFSTRING);
    //  gf_sg_vrml_mf_append(&t->string, GF_SG_VRML_MFSTRING, (void**)&ptr);

    //  if (text->html) {
    //      GF_SAXParser *xml;
    //      SWFFlatText flat;
    //      flat.final = 0;
    //      flat.len = 0;
    //      xml = gf_xml_sax_new(swf_nstart, swf_nend, swf_ntext, &flat);
    //      gf_xml_sax_init(xml, NULL);
    //      gf_xml_sax_parse(xml, text->init_value);
    //      gf_xml_sax_del(xml);

    //      if (flat.final) {
    //          ((SFString*)ptr)->buffer = gf_strdup(flat.final);
    //          gf_free(flat.final);
    //      }
    //  } else {
    //      ((SFString*)ptr)->buffer = gf_strdup(text->init_value);
    //  }
    //}


    //txt = (M_Shape *) s2s_new_node(read, TAG_MPEG4_Shape);
    //txt->appearance = s2s_get_appearance(read, (GF_Node *) txt, text->color, 0, 0);               
    //txt->geometry = (GF_Node *) t;
    //gf_node_register(txt->geometry, (GF_Node *) txt);

    //if (layout) {     
    //  gf_sg_vrml_mf_reset(&layout->justify, GF_SG_VRML_MFSTRING);
    //  gf_sg_vrml_mf_append(&layout->justify, GF_SG_VRML_MFSTRING, NULL);
    //  switch (text->align) {
    //  case 0:
    //      layout->justify.vals[0] = gf_strdup("BEGIN"); 
    //      break;
    //  case 1:
    //      layout->justify.vals[0] = gf_strdup("END"); 
    //      break;
    //  case 3:
    //      layout->justify.vals[0] = gf_strdup("JUSTIFY"); 
    //      break;
    //  default:
    //      layout->justify.vals[0] = gf_strdup("MIDDLE"); 
    //      break;
    //  }
    //  if (text->multiline || text->word_wrap) layout->wrap = 1;

    //  gf_node_insert_child((GF_Node *) layout, (GF_Node *)txt, -1);
    //  gf_node_register((GF_Node *) txt, (GF_Node *) layout);

    //  gf_node_insert_child((GF_Node *) tr, (GF_Node *)layout, -1);
    //  gf_node_register((GF_Node *) layout, (GF_Node *) tr);
    //} else {
    //  gf_node_insert_child((GF_Node *) tr, (GF_Node *)txt, -1);
    //  gf_node_register((GF_Node *) txt, (GF_Node *) tr);
    //} 
    //if (tr) {
    //  char szDEF[1024];
    //  u32 ID;
    //  sprintf(szDEF, "Text%d", text->ID);
    //  read->load->ctx->max_node_id++;
    //  ID = read->load->ctx->max_node_id;
    //  gf_node_set_id((GF_Node*)tr, ID, szDEF);
    //  s2s_insert_symbol(read, (GF_Node*)tr);
    //}
    return GF_OK;
}

#if 0
/*called upon end of sprite or clip*/
static void swf_svg_end_of_clip(SWFReader *read)
{
    //char szDEF[1024];
    //u32 i;
    //GF_AUContext *au;
    //GF_Command *com;
    //GF_CommandField *f;
    //GF_Node *empty;
    //
    //return;

    //empty = gf_sg_find_node_by_name(read->load->scene_graph, "Shape0");

    //au = gf_list_get(read->bifs_es->AUs, 0);
    //for (i=0; i<read->max_depth; i++) {
    //  /*and write command*/
    //  com = gf_sg_command_new(read->load->scene_graph, GF_SG_INDEXED_REPLACE);
    //  sprintf(szDEF, "CLIP%d_DL", read->current_sprite_id);
    //  com->node = gf_sg_find_node_by_name(read->load->scene_graph, szDEF);

    //  gf_node_register(com->node, NULL);
    //  f = gf_sg_command_field_new(com);
    //  f->field_ptr = &f->new_node;
    //  f->fieldType = GF_SG_VRML_SFNODE;
    //  f->pos = i;
    //  f->fieldIndex = 2;  /*children index*/
    //  f->new_node = empty;
    //  gf_node_register(f->new_node, com->node);

    //  gf_list_insert(au->commands, com, i);
    //}
}
#endif

static Bool swf_svg_allocate_depth(SWFReader *read, u32 depth)
{
    //char szDEF[100];
    //GF_Node *disp, *empty;
    //if (read->max_depth > depth) return 1;

    //sprintf(szDEF, "CLIP%d_DL", read->current_sprite_id);
    //disp = gf_sg_find_node_by_name(read->load->scene_graph, szDEF);

    //empty = gf_sg_find_node_by_name(read->load->scene_graph, "Shape0");
    //while (read->max_depth<=depth) {
    //  gf_node_insert_child(disp, empty, -1);
    //  gf_node_register(empty, disp);
    //  read->max_depth++;
    //}
    return 0;
}

static GF_Err swf_svg_define_sprite(SWFReader *read, u32 nb_frames)
{
    //GF_Err e;
    //GF_ObjectDescriptor *od;
    //GF_ESD *esd;
    //u32 ID;
    //GF_Node *n, *par;
    //GF_FieldInfo info;
    //char szDEF[100];
    //GF_StreamContext *prev_sc;
    //GF_AUContext *prev_au;

    ///*init OD*/
    //e = swf_init_od(read, 0);
    //if (e) return e;

    ///*create animationStream object*/
    //od = (GF_ObjectDescriptor *) gf_odf_desc_new(GF_ODF_OD_TAG);
    //if (!od) return GF_OUT_OF_MEM;

    //od->objectDescriptorID = swf_get_od_id(read);
    //esd = (GF_ESD *) gf_odf_desc_esd_new(0);
    //if (!esd) return GF_OUT_OF_MEM;
    //esd->ESID = swf_get_es_id(read);
    ///*sprite runs on its own timeline*/
    //esd->OCRESID = esd->ESID;
    ///*always depends on main scene*/
    //esd->dependsOnESID = 1;
    //esd->decoderConfig->streamType = GF_STREAM_SCENE;
    //esd->decoderConfig->objectTypeIndication = 1;
    //esd->slConfig->timestampResolution = read->bifs_es->timeScale;
    //gf_odf_desc_del((GF_Descriptor *) esd->decoderConfig->decoderSpecificInfo);
    //esd->decoderConfig->decoderSpecificInfo = NULL;
    //gf_list_add(od->ESDescriptors, esd);

    ///*by default insert OD at begining*/
    //e = swf_insert_od(read, 0, od);
    //if (e) {
    //  gf_odf_desc_del((GF_Descriptor *) od);
    //  return e;
    //}

    ///*create AS for sprite - all AS are created in initial scene replace*/
    //n = s2s_new_node(read, TAG_MPEG4_AnimationStream);
    //gf_node_insert_child(read->root, n, 0);
    //gf_node_register(n, read->root);
    ///*assign URL*/
    //gf_node_get_field_by_name(n, "url", &info);
    //gf_sg_vrml_mf_alloc(info.far_ptr, info.fieldType, 1);
    //((MFURL*)info.far_ptr)->vals[0].OD_ID = od->objectDescriptorID;
    //((M_AnimationStream *)n)->startTime = 0;

    //n = s2s_new_node(read, TAG_MPEG4_MediaControl);
    //sprintf(szDEF, "CLIP%d_CTRL", read->current_sprite_id);
    //read->load->ctx->max_node_id++;
    //ID = read->load->ctx->max_node_id;
    //gf_node_set_id(n, ID, szDEF);

    //gf_node_insert_child(read->root, n, 0);
    //gf_node_register(n, read->root);
    ///*assign URL*/
    //gf_node_get_field_by_name(n, "url", &info);
    //gf_sg_vrml_mf_alloc(info.far_ptr, info.fieldType, 1);
    //((MFURL*)info.far_ptr)->vals[0].OD_ID = od->objectDescriptorID;
    ///*inactive by default (until inserted)*/
    //((M_MediaControl *)n)->mediaSpeed = 0;
    //((M_MediaControl *)n)->loop = 1;

    ///*create sprite grouping node*/
    //n = s2s_new_node(read, TAG_MPEG4_Group);
    //sprintf(szDEF, "CLIP%d_DL", read->current_sprite_id);

    //read->load->ctx->max_node_id++;
    //ID = read->load->ctx->max_node_id;
    //gf_node_set_id(n, ID, szDEF);
    //par = gf_sg_find_node_by_name(read->load->scene_graph, "DICTIONARY");
    //assert(par);
    //gf_node_list_add_child(&((M_Switch *)par)->choice, n);
    //gf_node_register(n, par);
    //par = gf_sg_find_node_by_name(read->load->scene_graph, "Shape0");
    //gf_node_insert_child(n, par, -1);
    //gf_node_register(par, n);

    ///*store BIFS context*/
    //prev_sc = read->bifs_es;
    //prev_au = read->bifs_au;
    ///*create new BIFS stream*/
    //read->bifs_es = gf_sm_stream_new(read->load->ctx, esd->ESID, GF_STREAM_SCENE, 1);
    //read->bifs_es->timeScale = prev_sc->timeScale;
    //read->bifs_es->imp_exp_time = prev_sc->imp_exp_time + prev_au->timing;

    ///*create first AU*/
    //read->bifs_au = gf_sm_stream_au_new(read->bifs_es, 0, 0, 1);

    //e = swf_parse_sprite(read);
    //if (e) return e;

    //swf_svg_end_of_clip(read);

    ///*restore BIFS context*/
    //read->bifs_es = prev_sc;
    //read->bifs_au = prev_au;

    return GF_OK;
}

static GF_Err swf_svg_setup_sound(SWFReader *read, SWFSound *snd, Bool soundstream_first_block)
{
//  GF_Err e;
//  GF_ObjectDescriptor *od;
//  GF_ESD *esd;
//  GF_MuxInfo *mux;
//  GF_Node *n, *par;
//  GF_FieldInfo info;
//  u32 ID;
//  char szDEF[100];
//
//  /*soundstream header, only declare the associated MediaControl node for later actions*/
//  if (!snd->ID && !soundstream_first_block) {
//      n = s2s_new_node(read, TAG_MPEG4_MediaControl);
//      sprintf(szDEF, "CLIP%d_SND", read->current_sprite_id);
//      read->load->ctx->max_node_id++;
//      ID = read->load->ctx->max_node_id;
//      gf_node_set_id(n, ID, szDEF);
//
//      gf_node_insert_child(read->root, n, 0);
//      gf_node_register(n, read->root);
//      return GF_OK;
//  }
//
//  e = swf_init_od(read, 0);
//  if (e) return e;
//
//  /*create audio object*/
//  od = (GF_ObjectDescriptor *) gf_odf_desc_new(GF_ODF_OD_TAG);
//  if (!od) return GF_OUT_OF_MEM;
//  od->objectDescriptorID = swf_get_od_id(read);
//  esd = (GF_ESD *) gf_odf_desc_new(GF_ODF_ESD_TAG);
//  if (!esd) return GF_OUT_OF_MEM;
//  esd->ESID = swf_get_es_id(read);
//  if (snd->ID) {
//      /*sound runs on its own timeline*/
//      esd->OCRESID = esd->ESID;
//  } else {
//      /*soundstream runs on movie/sprite timeline*/
//      esd->OCRESID = read->bifs_es->ESID;
//      esd->OCRESID = esd->ESID;
//  }
//  gf_list_add(od->ESDescriptors, esd);
//
//  /*setup mux info*/
//  mux = (GF_MuxInfo*)gf_odf_desc_new(GF_ODF_MUXINFO_TAG);
//  mux->file_name = gf_strdup(snd->szFileName);
////    mux->startTime = snd->frame_delay_ms;
//  mux->startTime = 0;
//  /*MP3 in, destroy file once done*/
//  if (snd->format==2) mux->delete_file = 1;
//  gf_list_add(esd->extensionDescriptors, mux);
//
//
//  /*by default insert OD at begining*/
//  e = swf_insert_od(read, 0, od);
//  if (e) {
//      gf_odf_desc_del((GF_Descriptor *) od);
//      return e;
//  }
//  /*create sound & audio clip*/
//  n = s2s_new_node(read, TAG_MPEG4_Sound2D);
//  gf_node_insert_child(read->root, n, 0);
//  gf_node_register(n, read->root);
//  par = n;
//  n = s2s_new_node(read, TAG_MPEG4_AudioClip);
//  ((M_Sound2D *)par)->source = n;
//  gf_node_register(n, par);
//  /*assign URL*/
//  gf_node_get_field_by_name(n, "url", &info);
//  gf_sg_vrml_mf_alloc(info.far_ptr, info.fieldType, 1);
//  ((MFURL *)info.far_ptr)->vals[0].OD_ID = od->objectDescriptorID;
//
//  ((M_AudioClip*)n)->startTime = -1.0;
//
//  /*regular sound: set an ID to do play/stop*/
//  if (snd->ID) {
//      sprintf(szDEF, "Sound%d", snd->ID);
//      read->load->ctx->max_node_id++;
//      ID = read->load->ctx->max_node_id;
//      gf_node_set_id(n, ID, szDEF);
//  }
//  /*soundStream - add a MediaControl*/
//  else {
//      /*if sprite always have the media active but controled by its mediaControl*/
//      if (read->current_sprite_id) {
//          ((M_AudioClip*)n)->startTime = 0;
//      } 
//      /*otherwise start the media at the first soundstream block*/
//      else {
//          ((M_AudioClip*)n)->startTime = snd->frame_delay_ms/1000.0;
//          ((M_AudioClip*)n)->startTime = 0;
//      }
//
//      sprintf(szDEF, "CLIP%d_SND", read->current_sprite_id);
//      n = gf_sg_find_node_by_name(read->load->scene_graph, szDEF);
//
//      /*assign URL*/
//      gf_node_get_field_by_name(n, "url", &info);
//      gf_sg_vrml_mf_alloc(info.far_ptr, info.fieldType, 1);
//      ((MFURL*)info.far_ptr)->vals[0].OD_ID = od->objectDescriptorID;
//      ((M_MediaControl *)n)->loop = 0;
//
//      /*inactive by default (until inserted)*/
//      if (read->current_sprite_id) {
//          ((M_MediaControl *)n)->mediaSpeed = 0;
//      } else {
//          ((M_MediaControl *)n)->mediaSpeed = FIX_ONE;
//      }
//  }
    return GF_OK;
}

static GF_Err swf_svg_setup_image(SWFReader *read, u32 ID, char *fileName)
{

    //GF_Err e;
    //GF_ObjectDescriptor *od;
    //GF_ESD *esd;
    //GF_MuxInfo *mux;
    //GF_Node *n, *par;
    //GF_FieldInfo info;
    //char szDEF[100];
    //
    //e = swf_init_od(read, 0);
    //if (e) return e;

    ///*create visual object*/
    //od = (GF_ObjectDescriptor *) gf_odf_desc_new(GF_ODF_OD_TAG);
    //if (!od) return GF_OUT_OF_MEM;
    //od->objectDescriptorID = swf_get_od_id(read);
    //esd = (GF_ESD *) gf_odf_desc_new(GF_ODF_ESD_TAG);
    //if (!esd) return GF_OUT_OF_MEM;
    //esd->ESID = swf_get_es_id(read);
    //esd->OCRESID = esd->ESID;
    //gf_list_add(od->ESDescriptors, esd);

    ///*setup mux info*/
    //mux = (GF_MuxInfo*)gf_odf_desc_new(GF_ODF_MUXINFO_TAG);

    //mux->file_name = gf_strdup(fileName);
    ///*destroy file once done*/
    ////mux->delete_file = 1;
    //gf_list_add(esd->extensionDescriptors, mux);


    ///*by default insert OD at begining*/
    //e = swf_insert_od(read, 0, od);
    //if (e) {
    //  gf_odf_desc_del((GF_Descriptor *) od);
    //  return e;
    //}
    ///*create appearance clip*/
    //par = s2s_new_node(read, TAG_MPEG4_Shape);
    //s2s_insert_symbol(read, par);
    //n = s2s_new_node(read, TAG_MPEG4_Appearance);
    //((M_Shape *)par)->appearance = n;
    //gf_node_register(n, par);

    //par = n;
    //n = s2s_new_node(read, TAG_MPEG4_ImageTexture);
    //((M_Appearance *)par)->texture = n;
    //gf_node_register(n, par);

    //sprintf(szDEF, "Bitmap%d", ID);
    //read->load->ctx->max_node_id++;
    //ID = read->load->ctx->max_node_id;
    //gf_node_set_id(n, ID, szDEF);

    ///*assign URL*/
    //gf_node_get_field_by_name(n, "url", &info);
    //gf_sg_vrml_mf_alloc(info.far_ptr, info.fieldType, 1);
    //((MFURL *)info.far_ptr)->vals[0].OD_ID = od->objectDescriptorID;

    return GF_OK;
}

static GF_Err swf_svg_set_backcol(SWFReader *read, u32 xrgb)
{
    //SFColor rgb;
    //GF_Node *bck = gf_sg_find_node_by_name(read->load->scene_graph, "BACKGROUND");

    //rgb.red = INT2FIX((xrgb>>16) & 0xFF) / 255;
    //rgb.green = INT2FIX((xrgb>>8) & 0xFF) / 255;
    //rgb.blue = INT2FIX((xrgb) & 0xFF) / 255;
    //s2s_set_field(read, read->bifs_au->commands, bck, "backColor", -1, GF_SG_VRML_SFCOLOR, &rgb, 0);
    return GF_OK;
}

static GF_Err swf_svg_start_sound(SWFReader *read, SWFSound *snd, Bool stop)
{
    //GF_Node *sound2D;
    //SFTime t = 0;
    //char szDEF[100];

    //sprintf(szDEF, "Sound%d", snd->ID);
    //sound2D = gf_sg_find_node_by_name(read->load->scene_graph, szDEF);
    ///*check flags*/
    //if (sound2D)
    //  s2s_set_field(read, read->bifs_au->commands, sound2D, stop ? "stopTime" : "startTime", -1, GF_SG_VRML_SFTIME, &t, 0);

    return GF_OK;
}

static GF_Err swf_svg_place_obj(SWFReader *read, u32 depth, u32 ID, u32 prev_id, u32 type, GF_Matrix2D *mat, GF_ColorMatrix *cmat, GF_Matrix2D *prev_mat, GF_ColorMatrix *prev_cmat)
{
    //fprintf(read->svg_output, "<use xlink:href=\"#S%d\" z-index=\"%d\" ", ID, depth);
    //if (mat) {
    //    swf_svg_print_matrix(read, mat);
    //}
    //fprintf(read->svg_output, "/>\n");

    //GF_Command *com;
    //GF_CommandField *f;
    //GF_Node *obj, *par;
    //char szDEF[100];
    //Bool is_sprite;

    //obj = s2s_get_node(read, ID);
    //is_sprite = 0;
    //if (!obj) {
    //  sprintf(szDEF, "CLIP%d_DL", ID);
    //  obj = gf_sg_find_node_by_name(read->load->scene_graph, szDEF);
    //  if (obj) is_sprite = 1;
    //}
    //if (!obj) return GF_BAD_PARAM;

    ///*then add cmat/mat and node*/
    //par = s2s_wrap_node(read, obj, mat, cmat);

    ///*and write command*/
    //com = gf_sg_command_new(read->load->scene_graph, GF_SG_INDEXED_REPLACE);
    //sprintf(szDEF, "CLIP%d_DL", read->current_sprite_id);
    //com->node = gf_sg_find_node_by_name(read->load->scene_graph, szDEF);
    //gf_node_register(com->node, NULL);
    //f = gf_sg_command_field_new(com);
    //f->field_ptr = &f->new_node;
    //f->fieldType = GF_SG_VRML_SFNODE;
    //f->pos = depth;
    //f->fieldIndex = 2;    /*children index*/
    //f->new_node = par;
    //gf_node_register(f->new_node, com->node);
    //gf_list_add(read->bifs_au->commands, com);

    //if (ID==prev_id) return GF_OK;

    //strcpy(szDEF, gf_node_get_name(obj));
    ///*when inserting a button, trigger a pause*/
    //if (!strnicmp(szDEF, "Button", 6)) {
    //  u32 i, count;
    //  s2s_control_sprite(read, read->bifs_au->commands, read->current_sprite_id, 1, 0, 0, 1);

    //  count = gf_list_count(read->buttons);
    //  for (i=0; i<count; i++) {
    //      s2sBtnRec *btnrec = gf_list_get(read->buttons, i);
    //      if (btnrec->btn_id==ID) {
    //          s2s_control_sprite(read, read->bifs_au->commands, btnrec->sprite_up_id, 0, 0, 0, 1);
    //      }
    //  }
    //}
    ///*starts anim*/
    //else if (is_sprite) {
    //  s2s_control_sprite(read, read->bifs_au->commands, ID, 0, 1, 0, 0);
    //  if (prev_id) {
    //      s2s_control_sprite(read, read->bifs_au->commands, prev_id, 1, 0, 0, 0);
    //  }
    //}
    return GF_OK;
}

static GF_Err swf_svg_remove_obj(SWFReader *read, u32 depth, u32 ID)
{
    return GF_OK;
}

static GF_Err swf_svg_show_frame(SWFReader *read)
{
    u32     i;
    u32     len;
    GF_List *sdl = gf_list_new(); // sorted display list

    /* sorting the display list */
    while (gf_list_count(read->display_list))
    {
        Bool        inserted = 0;
        DispShape   *s;

        s = (DispShape *)gf_list_get(read->display_list, 0);
        gf_list_rem(read->display_list, 0);
        
        for (i = 0; i < gf_list_count(sdl); i++)
        {
            DispShape *s2 = (DispShape *)gf_list_get(sdl, i);
            if (s->depth < s2->depth) 
            {
                gf_list_insert(sdl, s, i);
                inserted = 1;
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
        fprintf(read->svg_output, "<use xlink:href=\"#S%d\" z-index=\"%d\" ", s->char_id, s->depth);
        swf_svg_print_matrix(read, &s->mat);
        fprintf(read->svg_output, "/>\n");
    }
    fprintf(read->svg_output, "</g>\n");

    fprintf(read->svg_output, "<g id=\"frame%d\" display=\"none\">\n",read->current_frame+1);
    fprintf(read->svg_output, "<animate attributeName=\"display\" to=\"inline\" begin=\"%f\" end=\"%f\" fill=\"%s\" restart=\"never\"/>\n", 
        1.0*(read->current_frame+1)/read->frame_rate, 1.0*(read->current_frame+2)/read->frame_rate,
        (((read->current_frame+1) <= (read->frame_count-1)) ? "remove" : "freeze"));
    return GF_OK;
}

static void swf_svg_finalize(SWFReader *read)
{
    //u32 i, count;

    //swf_svg_end_of_clip(read);    

    //while (gf_list_count(read->buttons)) {
    //  s2sBtnRec *btnrec = gf_list_get(read->buttons, 0);
    //  gf_list_rem(read->buttons, 0);
    //  gf_free(btnrec);
    //}

    //count = gf_list_count(read->fonts);
    //for (i=0;i<count; i++) {
    //  SWFFont *ft = (SWFFont *)gf_list_get(read->fonts, i);
    //  while (gf_list_count(ft->glyphs)) {
    //      GF_Node *gl = (GF_Node *)gf_list_get(ft->glyphs, 0);
    //      gf_list_rem(ft->glyphs, 0);
    //      gf_node_unregister(gl, NULL);
    //  }
    //}
    fprintf(read->svg_output, "</g>\n");
    fprintf(read->svg_output, "</svg>\n");
    fclose(read->svg_output);
}

static GF_Err swf_svg_define_button(SWFReader *read, SWF_Button *btn)
{
    //char szName[1024];
    //M_Switch *button;
    //SWF_ButtonRecord *br;
    //GF_Node *btn_root, *n, *btn_ts;
    //u32 i, ID, pos;

    //if (!btn) {
    //  read->btn = NULL;
    //  read->btn_over = read->btn_not_over = read->btn_active = read->btn_not_active = NULL;
    //  return GF_OK;
    //}

    //read->btn = btn;

    //btn_root = s2s_new_node(read, TAG_MPEG4_Transform2D);
    //sprintf(szName, "Button%d", btn->ID);
    //read->load->ctx->max_node_id++;
    //ID = read->load->ctx->max_node_id;
    //gf_node_set_id((GF_Node *)btn_root, ID, szName);

    //n = s2s_button_add_child(read, btn_root, TAG_MPEG4_ColorTransform, NULL, -1);
    //((M_ColorTransform*)n)->maa = ((M_ColorTransform*)n)->mab = ((M_ColorTransform*)n)->mar = ((M_ColorTransform*)n)->mag = ((M_ColorTransform*)n)->ta = 0; 

    ///*locate hit buttons and add them to the color transform*/
    //for (i=0; i<btn->count; i++) {
    //  GF_Node *character;
    //  br = &btn->buttons[i];
    //  if (!br->hitTest) continue;
    //  character = s2s_get_node(read, br->character_id);
    //  if (!character) {
    //      sprintf(szName, "CLIP%d_DL", br->character_id);
    //      character = gf_sg_find_node_by_name(read->load->scene_graph, szName);
    //  }
    //  if (character) {
    //      gf_node_list_add_child(&((GF_ParentNode*)n)->children, character);
    //      gf_node_register(character, (GF_Node *)n);
    //  }
    //}
    ///*add touch sensor to the color transform*/
    //sprintf(szName, "BTN%d_TS", read->btn->ID);
    //btn_ts = s2s_button_add_child(read, n, TAG_MPEG4_TouchSensor, szName, -1);

    //s2s_insert_symbol(read, (GF_Node *)btn_root);

    ///*isActive handler*/
    //sprintf(szName, "BTN%d_CA", read->btn->ID);
    //n = s2s_button_add_child(read, btn_root, TAG_MPEG4_Conditional, szName, -1);
    //read->btn_active = ((M_Conditional*)n)->buffer.commandList;
    //s2s_button_add_route(read, btn_ts, 4, n, 0);

    ///*!isActive handler*/
    //sprintf(szName, "BTN%d_CNA", read->btn->ID);
    //n = s2s_button_add_child(read, btn_root, TAG_MPEG4_Conditional, szName, -1);
    //read->btn_not_active = ((M_Conditional*)n)->buffer.commandList;
    //s2s_button_add_route(read, btn_ts, 4, n, 1);

    ///*isOver handler*/
    //sprintf(szName, "BTN%d_CO", read->btn->ID);
    //n = s2s_button_add_child(read, btn_root, TAG_MPEG4_Conditional, szName, -1);
    //read->btn_over = ((M_Conditional*)n)->buffer.commandList;
    //s2s_button_add_route(read, btn_ts, 5, n, 0);

    ///*!isOver handler*/
    //sprintf(szName, "BTN%d_CNO", read->btn->ID);
    //n = s2s_button_add_child(read, btn_root, TAG_MPEG4_Conditional, szName, -1);
    //read->btn_not_over = ((M_Conditional*)n)->buffer.commandList;
    //s2s_button_add_route(read, btn_ts, 5, n, 1);

    ///*by default show first character*/
    //pos = 0;
    //for (i=0; i<btn->count; i++) {
    //  GF_Node *sprite_ctrl = NULL;
    //  GF_Node *character;
    //  br = &btn->buttons[i];
    //  if (!br->up && !br->down && !br->over) continue;

    //  character = s2s_get_node(read, br->character_id);

    //  if (!character) {
    //      sprintf(szName, "CLIP%d_DL", br->character_id);
    //      character = gf_sg_find_node_by_name(read->load->scene_graph, szName);
    //      if (character) {
    //          sprintf(szName, "CLIP%d_CTRL", br->character_id);
    //          sprite_ctrl = gf_sg_find_node_by_name(read->load->scene_graph, szName);
    //      }
    //  }
    //  if (character) {
    //      SFInt32 choice = 0;
    //      GF_Node *n = s2s_wrap_node(read, character, &br->mx, &br->cmx);

    //      sprintf(szName, "BTN%d_R%d", btn->ID, i+1);
    //      button = (M_Switch *) s2s_button_add_child(read, btn_root, TAG_MPEG4_Switch, szName, pos);
    //      pos++;

    //      gf_node_list_add_child(&button->choice, n);
    //      gf_node_register(n, (GF_Node *)button);
    //      /*initial state*/
    //      if (br->up) {
    //          button->whichChoice = 0;
    //          /*register this button for sprite start upon place_obj*/
    //          if (sprite_ctrl) {
    //              s2sBtnRec *btnrec;
    //              if (!read->buttons) read->buttons = gf_list_new();
    //              btnrec = gf_malloc(sizeof(s2sBtnRec));
    //              btnrec->btn_id = btn->ID;
    //              btnrec->sprite_up_id = br->character_id;
    //              gf_list_add(read->buttons, btnrec);
    //          }

    //      } else {
    //          button->whichChoice = -1;
    //      }

    //      choice = br->up ? 0 : -1;
    //      s2s_set_field(read, read->btn_not_over, (GF_Node *)button, "whichChoice", -1, GF_SG_VRML_SFINT32, &choice, 0);
    //      /*start or stop sprite if button is up or not*/
    //      if (sprite_ctrl) {
    //          s2s_control_sprite(read, read->btn_not_over, br->character_id, choice, 1, 0, 0);
    //      }

    //      choice = br->down ? 0 : -1;
    //      s2s_set_field(read, read->btn_active, (GF_Node *)button, "whichChoice", -1, GF_SG_VRML_SFINT32, &choice, 0);
    //      if (sprite_ctrl && !br->over) {
    //          s2s_control_sprite(read, read->btn_active, br->character_id, choice, 1, 0, 0);
    //      }

    //      choice = br->over ? 0 : -1;
    //      s2s_set_field(read, read->btn_not_active, (GF_Node *)button, "whichChoice", -1, GF_SG_VRML_SFINT32, &choice, 0);
    //      s2s_set_field(read, read->btn_over, (GF_Node *)button, "whichChoice", -1, GF_SG_VRML_SFINT32, &choice, 0);
    //      if (sprite_ctrl) {
    //          s2s_control_sprite(read, read->btn_over, br->character_id, choice, 1, 0, 0);
    //          if (!br->down)
    //              s2s_control_sprite(read, read->btn_not_active, br->character_id, choice, 1, 0, 0);
    //      }
    //  }
    //}

    return GF_OK;
}

Bool swf_svg_action(SWFReader *read, SWFAction *act)
{
//  GF_List *dst;
//  MFURL url;
//  SFURL sfurl;
//  Bool bval;
//  GF_Node *n;
//  Double time;
//
//  dst = read->bifs_au->commands;
//  if (read->btn) {
//      if (act->button_mask & GF_SWF_COND_OVERUP_TO_OVERDOWN) dst = read->btn_active;
//      else if (act->button_mask & GF_SWF_COND_IDLE_TO_OVERUP) dst = read->btn_over;
//      else if (act->button_mask & GF_SWF_COND_OVERUP_TO_IDLE) dst = read->btn_not_over;
//      else dst = read->btn_not_active;
//  }
//
//  switch (act->type) {
//  case GF_SWF_AS3_WAIT_FOR_FRAME:
//      /*while correct, this is not optimal, we set the wait-frame upon GOTO frame*/
////        read->wait_frame = act->frame_number;
//      break;
//  case GF_SWF_AS3_GOTO_FRAME:
//      if (act->frame_number>read->current_frame)
//          read->wait_frame = act->frame_number;
//
//      time = act->frame_number ? act->frame_number +1: 0;
//      time /= read->frame_rate;
//      s2s_control_sprite(read, dst, read->current_sprite_id, 0, 1, time, 0);
//      break;
//  case GF_SWF_AS3_GET_URL:
//      n = gf_sg_find_node_by_name(read->load->scene_graph, "MOVIE_URL");
//      sfurl.OD_ID = 0; sfurl.url = act->url;
//      url.count = 1; url.vals = &sfurl;
//      s2s_set_field(read, dst, n, "url", -1, GF_SG_VRML_MFURL, &url, 0);
//      s2s_set_field(read, dst, n, "parameter", -1, GF_SG_VRML_MFSTRING, &url, 0);
//      bval = 1;
//      s2s_set_field(read, dst, n, "activate", -1, GF_SG_VRML_SFBOOL, &bval, 0);
//      break;
//  case GF_SWF_AS3_PLAY:
//      s2s_control_sprite(read, dst, read->current_sprite_id, 0, 1, -1, 0);
//      break;
//  case GF_SWF_AS3_STOP:
//      s2s_control_sprite(read, dst, read->current_sprite_id, 1, 0, 0, 0);
//      break;
//  default:
//      return 0;
//  }
//
    return 1;
}

GF_Err swf_to_svg_init(SWFReader *read)
{
    char szFileName[GF_MAX_PATH];
    sprintf(szFileName, "%s.svg", read->load->fileName);
    /*init callbacks*/
    read->svg_output = gf_f64_open(szFileName, "wt");
    fprintf(read->svg_output, "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n");
    fprintf(read->svg_output, "<svg xmlns=\"http://www.w3.org/2000/svg\" ");
    fprintf(read->svg_output, "xmlns:xlink=\"http://www.w3.org/1999/xlink\" ");
    fprintf(read->svg_output, "width=\"100%%\" ");
    fprintf(read->svg_output, "height=\"100%%\" ");
    fprintf(read->svg_output, "viewBox=\"0 0 %d %d\" ", FIX2INT(read->width), FIX2INT(read->height));
    fprintf(read->svg_output, "viewport-fill=\"rgb(255,255,255)\" ");
    fprintf(read->svg_output, ">\n");
    fprintf(read->svg_output, "<g id=\"frame%d\" display=\"none\">\n",read->current_frame);
    fprintf(read->svg_output, "<animate attributeName=\"display\" to=\"inline\" begin=\"%f\" end=\"%f\" fill=\"remove\" restart=\"never\"/>\n",
        1.0*(read->current_frame)/read->frame_rate, 1.0*(read->current_frame+1)/read->frame_rate);
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
    return GF_OK;
}

#endif /*GPAC_DISABLE_SWF_IMPORT*/

#else
GF_Err swf_to_svg_init(SWFReader *read)
{
    return GF_NOT_SUPPORTED;
}

#endif /*GPAC_DISABLE_VRML*/

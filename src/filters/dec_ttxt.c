/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2000-2022
 *					All rights reserved
 *
 *  This file is part of GPAC / 3GPP/MPEG4 text renderer filter
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

#include <gpac/filters.h>
#include <gpac/constants.h>

#include <gpac/internal/isomedia_dev.h>
#include <gpac/utf.h>
#include <gpac/nodes_mpeg4.h>
#include <gpac/internal/compositor_dev.h>


#if !defined(GPAC_DISABLE_VRML) && !defined(GPAC_DISABLE_ISOM)


/*
	this decoder is simply a scene decoder generating its own scene graph based on input data,
	this scene graph is then used as an extra graph by the renderer, and manipulated by the decoder
	for any time animation handling.
	Translation from text to MPEG-4 scene graph:
		* all modifiers (styles, hilight, etc) are unrolled into chunks forming a unique, linear
	sequence of text data (startChar, endChar) with associated styles & modifs
		* chunks are mapped to classic MPEG-4/VRML text
		* these chunks are then gathered in a Form node (supported by 2D and 3D renderers), with
	text truncation at each newline char.
		* the Form then performs all alignment of the chunks

	It could be possible to use Layout instead of form, BUT layout cannot handle new lines at the time being...

	Currently supported for 3GP text streams:
		* text box positioning & filling, dynamic text box
		* text color
		* proper alignment (H and V) with horizontal text. Vertical text may not be properly layed out (not fully tested)
		* style Records (font, size, fontstyles, and colors change) - any nb per sample supported
		* hilighting (static only) with color or reverse video - any nb per sample supported
		* hypertext links - any nb per sample supported
		* blinking - any nb per sample supported
		* complete scrolling: in, out, in+out, up, down, right and left directions. All other
		modifiers are supported when scrolling
		* scroll delay

	It does NOT support:
		* dynamic hilighting (karaoke)
		* wrap

	The decoder only accepts complete timed text units TTU(1). In band reconfig (TTU(5) is not supported,
	nor fragmented TTUs (2, 3, 4).
	UTF16 support should work but MP4Box does not support it at encoding time.
*/

typedef struct
{
	//opts
	Bool texture, outline;
	u32 txtw, txth;

	GF_FilterPid *ipid, *opid;

	GF_ObjectManager *odm;
	GF_Scene *scene;
	u32 dsi_crc;
	Bool is_tx3g;
	Bool is_playing, graph_registered, is_eos;
	u32 timescale;
	s64 delay;

	GF_TextConfig *cfg;
	GF_BitStream *bs_r;

	GF_SceneGraph *scenegraph;

	/*avoid searching the graph for things we know...*/
	M_Transform2D *tr_track, *tr_box, *tr_scroll;
	M_Material2D *mat_track, *mat_box;
	M_Layer2D *dlist;
	M_Rectangle *rec_box, *rec_track;

	M_TimeSensor *ts_blink, *ts_scroll;
	M_ScalarInterpolator *process_blink, *process_scroll;
	GF_Route *time_route;
	GF_List *blink_nodes;
	u32 scroll_type, scroll_mode;
	Fixed scroll_time, scroll_delay;
	Bool timer_active;

	Bool simple_text;
	u8 *static_text;
	u32 txt_static_alloc;
	u64 sample_end;
} GF_TTXTDec;


static void ttd_set_blink_fraction(GF_Node *node, GF_Route *route);
static void ttd_set_scroll_fraction(GF_Node *node, GF_Route *route);
static void ttd_reset_display(GF_TTXTDec *ctx);
static void ttd_setup_scene(GF_TTXTDec *ctx);
static void ttd_toggle_display(GF_TTXTDec *ctx);

/*the WORST thing about 3GP in MPEG4 is positioning of the text track...*/
static void ttd_update_size_info(GF_TTXTDec *ctx)
{
	u32 w, h;
	Bool has_size;
	s32 offset, thw, thh, vw, vh;

	has_size = gf_sg_get_scene_size_info(ctx->scene->graph, &w, &h);
	/*no size info is given in main scene, override by associated video size if any, or by text track size*/
	if (!has_size) {
		if (ctx->cfg->has_vid_info && ctx->cfg->video_width && ctx->cfg->video_height) {
			gf_sg_set_scene_size_info(ctx->scenegraph, ctx->cfg->video_width, ctx->cfg->video_height, GF_TRUE);
		} else if (ctx->cfg->text_width && ctx->cfg->text_height) {
			gf_sg_set_scene_size_info(ctx->scenegraph, ctx->cfg->text_width, ctx->cfg->text_height, GF_TRUE);
		} else {
			gf_sg_set_scene_size_info(ctx->scenegraph, ctx->txtw, ctx->txth, GF_TRUE);
		}
		gf_sg_get_scene_size_info(ctx->scenegraph, &w, &h);
		if (!w || !h) return;
		gf_scene_force_size(ctx->scene, w, h);
	}

	if (!w || !h) return;
	/*apply*/
	gf_sg_set_scene_size_info(ctx->scenegraph, w, h, GF_TRUE);
	/*make sure the scene size is big enough to contain the text track after positioning. RESULTS ARE UNDEFINED
	if offsets are negative: since MPEG-4 uses centered coord system, we must assume video is aligned to top-left*/
	if (ctx->cfg->has_vid_info) {
		Bool set_size = GF_FALSE;
		vw = ctx->cfg->horiz_offset;
		if (vw<0) vw = 0;
		vh = ctx->cfg->vert_offset;
		if (vh<0) vh = 0;
		if (ctx->cfg->text_width + (u32) vw > w) {
			w = ctx->cfg->text_width+vw;
			set_size = GF_TRUE;
		}
		if (ctx->cfg->text_height + (u32) vh > h) {
			h = ctx->cfg->text_height+vh;
			set_size = GF_TRUE;
		}
		if (set_size) {
			gf_sg_set_scene_size_info(ctx->scenegraph, w, h, GF_TRUE);
			gf_scene_force_size(ctx->scene, w, h);
		}
	} else {
		/*otherwise override (mainly used for SRT & TTXT file direct loading*/
		ctx->cfg->text_width = w;
		ctx->cfg->text_height = h;

		u32 i=0;
		GF_TextSampleDescriptor *td;
		while ((td = gf_list_enum(ctx->cfg->sample_descriptions, &i))) {
			td->default_pos.left = 0;
			td->default_pos.top = 0;
			td->default_pos.right = w;
			td->default_pos.bottom = h;
		}
	}

	/*ok override video size with main scene size*/
	ctx->cfg->video_width = w;
	ctx->cfg->video_height = h;

	vw = (s32) w;
	vh = (s32) h;
	thw = ctx->cfg->text_width / 2;
	thh = ctx->cfg->text_height / 2;
	/*check translation, we must not get out of scene size - not supported in GPAC*/
	offset = ctx->cfg->horiz_offset - vw/2 + thw;
	/*safety checks ?
	if (offset + thw < - vw/2) offset = - vw/2 + thw;
	else if (offset - thw > vw/2) offset = vw/2 - thw;
	*/
	ctx->tr_track->translation.x = INT2FIX(offset);

	offset = vh/2 - ctx->cfg->vert_offset - thh;
	/*safety checks ?
	if (offset + thh > vh/2) offset = vh/2 - thh;
	else if (offset - thh < -vh/2) offset = -vh/2 + thh;
	*/
	ctx->tr_track->translation.y = INT2FIX(offset);

	gf_node_changed((GF_Node *)ctx->tr_track, NULL);
}

static GFINLINE void ttd_add_child(GF_Node *n1, GF_Node *par)
{
	gf_node_list_add_child( & ((GF_ParentNode *)par)->children, n1);
	gf_node_register(n1, par);
}


static GFINLINE GF_Node *ttd_create_node(GF_TTXTDec *ttd, u32 tag, const char *def_name)
{
	GF_Node *n = gf_node_new(ttd->scenegraph, tag);
	if (n) {
		if (def_name) gf_node_set_id(n, gf_sg_get_next_available_node_id(ttd->scenegraph), def_name);
		gf_node_init(n);
	}
	return n;
}

static GF_Err ttd_configure_pid(GF_Filter *filter, GF_FilterPid *pid, Bool is_remove)
{
	GF_TTXTDec *ctx = gf_filter_get_udta(filter);
	GF_Err e;
	u32 st, codecid, dsi_crc;
	const GF_PropertyValue *p, *dsi;

	if (is_remove) {
		if (ctx->opid) {
			gf_filter_pid_remove(ctx->opid);
			ctx->opid = NULL;
		}
		ctx->ipid = NULL;
		return GF_OK;
	}
	//TODO: we need to cleanup cap checking upon reconfigure
	if (ctx->ipid && !gf_filter_pid_check_caps(pid)) return GF_NOT_SUPPORTED;
	assert(!ctx->ipid || (ctx->ipid == pid));

	st = codecid = 0;
	p = gf_filter_pid_get_property(pid, GF_PROP_PID_STREAM_TYPE);
	if (p) st = p->value.uint;
	p = gf_filter_pid_get_property(pid, GF_PROP_PID_CODECID);
	if (p) codecid = p->value.uint;

	if (st != GF_STREAM_TEXT) return GF_NOT_SUPPORTED;

	ctx->timescale = gf_filter_pid_get_timescale(pid);

	ctx->simple_text = GF_FALSE;
	switch (codecid) {
	case GF_CODECID_SUBS_TEXT:
	case GF_CODECID_SIMPLE_TEXT:
		ctx->simple_text = GF_TRUE;
		break;
	}

	ctx->ipid = pid;
	if (!ctx->opid) {
		ctx->opid = gf_filter_pid_new(filter);
	}

	//copy properties at init or reconfig
	gf_filter_pid_copy_properties(ctx->opid, ctx->ipid);
	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_STREAM_TYPE, &PROP_UINT(GF_STREAM_TEXT));
	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_CODECID, &PROP_UINT(GF_CODECID_RAW));

	if (ctx->simple_text) {
		if (!ctx->cfg) ctx->cfg = (GF_TextConfig *) gf_odf_desc_new(GF_ODF_TEXT_CFG_TAG);
		if (!gf_list_count(ctx->cfg->sample_descriptions)) {
			GF_TextSampleDescriptor *txtc;
			GF_SAFEALLOC(txtc, GF_TextSampleDescriptor);
			gf_list_add(ctx->cfg->sample_descriptions, txtc);
			txtc->sample_index = 1;
			/*
			txtc->font_count = 1;
			txtc->fonts[0].fontID = 1;
			txtc->fonts[0].fontName = gf_strdup(ctx->fontname ? ctx->fontname : "Serif");
			txtc->default_style.fontID = 1;
			txtc->default_style.font_size = ctx->fontsize;
			*/
			txtc->back_color = 0x00000000;	/*transparent*/
			txtc->default_style.text_color = 0xFFFFFFFF;	/*white*/
			txtc->default_style.style_flags = 0;
			txtc->horiz_justif = 1; /*center of scene*/
			txtc->vert_justif = (s8) -1;	/*bottom of scene*/
		}
	} else {
		Bool needs_init = GF_TRUE;
		//check dsi is ready
		dsi = gf_filter_pid_get_property(pid, GF_PROP_PID_DECODER_CONFIG);
		if (!dsi) return GF_OK;

		dsi_crc = gf_crc_32(dsi->value.data.ptr, dsi->value.data.size);
		if (dsi_crc == ctx->dsi_crc) return GF_OK;
		ctx->dsi_crc = dsi_crc;

		if (ctx->cfg) {
			needs_init = GF_FALSE;
			gf_odf_desc_del((GF_Descriptor *) ctx->cfg);
		}
		ctx->cfg = (GF_TextConfig *) gf_odf_desc_new(GF_ODF_TEXT_CFG_TAG);
		if (codecid == GF_CODECID_TEXT_MPEG4) {
			e = gf_odf_get_text_config(dsi->value.data.ptr, dsi->value.data.size, codecid, ctx->cfg);
			if (e) {
				gf_odf_desc_del((GF_Descriptor *) ctx->cfg);
				ctx->cfg = NULL;
				return e;
			}
			ctx->is_tx3g = GF_FALSE;
		} else if (codecid == GF_CODECID_TX3G) {
			GF_TextSampleDescriptor * sd = gf_odf_tx3g_read(dsi->value.data.ptr, dsi->value.data.size);
			if (!sd) {
				gf_odf_desc_del((GF_Descriptor *) ctx->cfg);
				ctx->cfg = NULL;
				return GF_NON_COMPLIANT_BITSTREAM;
			}
			if (!sd->default_style.text_color)
				sd->default_style.text_color = 0xFFFFFFFF;

			gf_list_add(ctx->cfg->sample_descriptions, sd);
			ctx->is_tx3g = GF_TRUE;
		}

		if (needs_init && ctx->odm && ctx->scene) {
			ttd_setup_scene(ctx);
			ttd_toggle_display(ctx);
		}
	}

	p = gf_filter_pid_get_property(pid, GF_PROP_PID_TIMESCALE);
	if (p && !ctx->cfg->timescale) ctx->cfg->timescale = p->value.uint;

	p = gf_filter_pid_get_property(pid, GF_PROP_PID_DELAY);
	ctx->delay = p ? p->value.longsint : 0;
	return GF_OK;
}

static void ttd_setup_scene(GF_TTXTDec *ctx)
{
	GF_Node *root, *n1, *n2;
	if (ctx->scenegraph) return;

	ctx->scenegraph = gf_sg_new_subscene(ctx->scene->graph);

	root = ttd_create_node(ctx, TAG_MPEG4_OrderedGroup, NULL);
	gf_sg_set_root_node(ctx->scenegraph, root);
	gf_node_register(root, NULL);
	/*root transform*/
	ctx->tr_track = (M_Transform2D *)ttd_create_node(ctx, TAG_MPEG4_Transform2D, NULL);
	ttd_add_child((GF_Node *) ctx->tr_track, root);

	ttd_update_size_info(ctx);

	/*txt track background*/
	n1 = ttd_create_node(ctx, TAG_MPEG4_Shape, NULL);
	ttd_add_child(n1, (GF_Node *) ctx->tr_track);
	((M_Shape *)n1)->appearance = ttd_create_node(ctx, TAG_MPEG4_Appearance, NULL);
	gf_node_register(((M_Shape *)n1)->appearance, n1);
	ctx->mat_track = (M_Material2D *) ttd_create_node(ctx, TAG_MPEG4_Material2D, NULL);
	ctx->mat_track->filled = 1;
	ctx->mat_track->transparency = 1;
	((M_Appearance *) ((M_Shape *)n1)->appearance)->material = (GF_Node *) ctx->mat_track;
	gf_node_register((GF_Node *) ctx->mat_track, ((M_Shape *)n1)->appearance);
	n2 = ttd_create_node(ctx, TAG_MPEG4_Rectangle, NULL);
	((M_Rectangle *)n2)->size.x = 0;
	((M_Rectangle *)n2)->size.y = 0;
	((M_Shape *)n1)->geometry = n2;
	gf_node_register(n2, n1);
	ctx->rec_track = (M_Rectangle *)n2;

	/*txt box background*/
	ctx->tr_box = (M_Transform2D *) ttd_create_node(ctx, TAG_MPEG4_Transform2D, NULL);
	ttd_add_child((GF_Node*) ctx->tr_box, (GF_Node*)ctx->tr_track);
	n1 = ttd_create_node(ctx, TAG_MPEG4_Shape, NULL);
	ttd_add_child(n1, (GF_Node*)ctx->tr_box);
	((M_Shape *)n1)->appearance = ttd_create_node(ctx, TAG_MPEG4_Appearance, NULL);
	gf_node_register(((M_Shape *)n1)->appearance, n1);
	ctx->mat_box = (M_Material2D *) ttd_create_node(ctx, TAG_MPEG4_Material2D, NULL);
	ctx->mat_box->filled = 1;
	ctx->mat_box->transparency = 1;
	((M_Appearance *) ((M_Shape *)n1)->appearance)->material = (GF_Node *)ctx->mat_box;
	gf_node_register((GF_Node *)ctx->mat_box, ((M_Shape *)n1)->appearance);
	ctx->rec_box = (M_Rectangle *) ttd_create_node(ctx, TAG_MPEG4_Rectangle, NULL);
	ctx->rec_box->size.x = 0;
	ctx->rec_box->size.y = 0;
	((M_Shape *)n1)->geometry = (GF_Node *) ctx->rec_box;
	gf_node_register((GF_Node *) ctx->rec_box, n1);

	ctx->dlist = (M_Layer2D *) ttd_create_node(ctx, TAG_MPEG4_Layer2D, NULL);
	ctx->dlist->size.x = ctx->cfg->text_width;
	ctx->dlist->size.y = ctx->cfg->text_height;
	ttd_add_child((GF_Node *)ctx->dlist, (GF_Node *)ctx->tr_box);

	ctx->blink_nodes = gf_list_new();
	ctx->ts_blink = (M_TimeSensor *) ttd_create_node(ctx, TAG_MPEG4_TimeSensor, "TimerBlink");
	ctx->ts_blink->cycleInterval = 0.25;
	ctx->ts_blink->startTime = 0.0;
	ctx->ts_blink->loop = 1;
	ctx->process_blink = (M_ScalarInterpolator *) ttd_create_node(ctx, TAG_MPEG4_ScalarInterpolator, NULL);
	/*override set_fraction*/
	ctx->process_blink->on_set_fraction = ttd_set_blink_fraction;
	gf_node_set_private((GF_Node *) ctx->process_blink, ctx);
	/*route from fraction_changed to set_fraction*/
	gf_sg_route_new(ctx->scenegraph, (GF_Node *) ctx->ts_blink, 6, (GF_Node *) ctx->process_blink, 0);

	ctx->ts_scroll = (M_TimeSensor *) ttd_create_node(ctx, TAG_MPEG4_TimeSensor, "TimerScroll");
	ctx->ts_scroll->cycleInterval = 0;
	ctx->ts_scroll->startTime = -1;
	ctx->ts_scroll->loop = 0;
	ctx->process_scroll = (M_ScalarInterpolator *) ttd_create_node(ctx, TAG_MPEG4_ScalarInterpolator, NULL);
	/*override set_fraction*/
	ctx->process_scroll->on_set_fraction = ttd_set_scroll_fraction;
	gf_node_set_private((GF_Node *) ctx->process_scroll, ctx);
	/*route from fraction_changed to set_fraction*/
	gf_sg_route_new(ctx->scenegraph, (GF_Node *) ctx->ts_scroll, 6, (GF_Node *) ctx->process_scroll, 0);

	gf_node_register((GF_Node *) ctx->ts_blink, NULL);
	gf_node_register((GF_Node *) ctx->process_blink, NULL);
	gf_node_register((GF_Node *) ctx->ts_scroll, NULL);
	gf_node_register((GF_Node *) ctx->process_scroll, NULL);

}

static void ttd_reset_scene(GF_TTXTDec *ctx)
{
	if (!ctx->scenegraph) return;

	gf_scene_register_extra_graph(ctx->scene, ctx->scenegraph, GF_TRUE);

	gf_node_unregister((GF_Node *) ctx->ts_blink, NULL);
	gf_node_unregister((GF_Node *) ctx->process_blink, NULL);
	gf_node_unregister((GF_Node *) ctx->ts_scroll, NULL);
	gf_node_unregister((GF_Node *) ctx->process_scroll, NULL);

	gf_sg_del(ctx->scenegraph);
	ctx->scenegraph = NULL;
	gf_list_del(ctx->blink_nodes);
}

static void ttd_set_blink_fraction(GF_Node *node, GF_Route *route)
{
	M_Material2D *m;
	u32 i;
	GF_TTXTDec *ctx = (GF_TTXTDec *)gf_node_get_private(node);

	Bool blink_on = GF_TRUE;
	if (ctx->process_blink->set_fraction>FIX_ONE/2) blink_on = GF_FALSE;
	i=0;
	while ((m = (M_Material2D*)gf_list_enum(ctx->blink_nodes, &i))) {
		if (m->filled != blink_on) {
			m->filled = blink_on;
			gf_node_changed((GF_Node *) m, NULL);
		}
	}
}

static void ttd_set_scroll_fraction(GF_Node *node, GF_Route *route)
{
	Fixed frac;
	GF_TTXTDec *ctx = (GF_TTXTDec *)gf_node_get_private(node);
	frac = ctx->process_scroll->set_fraction;
	if (frac==FIX_ONE) ctx->timer_active = GF_FALSE;
	if (!ctx->tr_scroll) return;

	switch (ctx->scroll_type - 1) {
	case GF_TXT_SCROLL_CREDITS:
	case GF_TXT_SCROLL_DOWN:
		ctx->tr_scroll->translation.x = 0;
		if (ctx->scroll_mode & GF_TXT_SCROLL_IN) {
			if (frac>ctx->scroll_time) {
				ctx->scroll_mode &= ~GF_TXT_SCROLL_IN;
				ctx->tr_scroll->translation.y = 0;
			} else {
				ctx->tr_scroll->translation.y = gf_muldiv(ctx->dlist->size.y, frac, ctx->scroll_time) - ctx->dlist->size.y;
			}
		} else if (ctx->scroll_mode & GF_TXT_SCROLL_OUT) {
			if (frac < FIX_ONE - ctx->scroll_time) return;
			frac -= FIX_ONE - ctx->scroll_time;
			if (ctx->scroll_type - 1 == GF_TXT_SCROLL_DOWN) {
				ctx->tr_scroll->translation.y = gf_muldiv(ctx->dlist->size.y, frac, ctx->scroll_time);
			} else {
				ctx->tr_scroll->translation.y = gf_muldiv(ctx->dlist->size.y, frac, ctx->scroll_time);
			}
		}
		if (ctx->scroll_type - 1 == GF_TXT_SCROLL_DOWN) ctx->tr_scroll->translation.y *= -1;
		break;
	case GF_TXT_SCROLL_MARQUEE:
	case GF_TXT_SCROLL_RIGHT:
		ctx->tr_scroll->translation.y = 0;
		if (ctx->scroll_mode & GF_TXT_SCROLL_IN) {
			if (! (ctx->scroll_mode & GF_TXT_SCROLL_OUT)) {
				if (frac<ctx->scroll_delay) return;
				frac-=ctx->scroll_delay;
			}
			if (frac>ctx->scroll_time) {
				ctx->scroll_mode &= ~GF_TXT_SCROLL_IN;
				ctx->tr_scroll->translation.x = 0;
			} else {
				ctx->tr_scroll->translation.x = gf_muldiv(ctx->dlist->size.x, frac, ctx->scroll_time) - ctx->dlist->size.x;
			}
		} else if (ctx->scroll_mode & GF_TXT_SCROLL_OUT) {
			if (frac < FIX_ONE - ctx->scroll_time) return;
			frac -= FIX_ONE - ctx->scroll_time;
			ctx->tr_scroll->translation.x = gf_muldiv(ctx->dlist->size.x, frac, ctx->scroll_time);
		}
		if (ctx->scroll_type - 1 == GF_TXT_SCROLL_MARQUEE) ctx->tr_scroll->translation.x *= -1;
		break;
	default:
		break;
	}
	gf_node_changed((GF_Node *)ctx->tr_scroll, NULL);
}

static void ttd_reset_display(GF_TTXTDec *ctx)
{
	gf_list_reset(ctx->blink_nodes);
	gf_node_unregister_children((GF_Node*)ctx->dlist, ctx->dlist->children);
	ctx->dlist->children = NULL;
	gf_node_changed((GF_Node *) ctx->dlist, NULL);
	ctx->tr_scroll = NULL;
}

static char *ttd_find_font(GF_TextSampleDescriptor *tsd, u32 ID)
{
	u32 i;
	for (i=0; i<tsd->font_count; i++) {
		if (tsd->fonts[i].fontID==ID) return tsd->fonts[i].fontName;
	}
	return "SERIF";
}

static void ttd_add_item(M_Form *form)
{
	s32 *new_gr;
	gf_sg_vrml_mf_append(&form->groups, GF_SG_VRML_MFINT32, (void **) &new_gr);
	(*new_gr) = gf_node_list_get_count(form->children);
	gf_sg_vrml_mf_append(&form->groups, GF_SG_VRML_MFINT32, (void **) &new_gr);
	(*new_gr) = -1;
	/*store line info*/
	gf_sg_vrml_mf_append(&form->groupsIndex, GF_SG_VRML_MFINT32, (void **) &new_gr);
	(*new_gr) = gf_node_list_get_count(form->children);
}

static void ttd_add_line(M_Form *form)
{
	s32 *new_gr;
	gf_sg_vrml_mf_append(&form->groupsIndex, GF_SG_VRML_MFINT32, (void **) &new_gr);
	(*new_gr) = -1;
}

typedef struct
{
	u32 start_char, end_char;
	GF_StyleRecord *srec;
	Bool is_hilight;
	u32 hilight_col;	/*0 means RV*/
	GF_TextHyperTextBox *hlink;
	Bool has_blink;
	/*karaoke not done yet*/
	/*text wrapping is not supported - we will need to move to Layout (rather than form), and modify
	layout to handle new lines and proper scrolling*/
} TTDTextChunk;

static void ttd_new_text_chunk(GF_TTXTDec *ctx, GF_TextSampleDescriptor *tsd, M_Form *form, u16 *utf16_txt, TTDTextChunk *tc)
{
	GF_Node *txt_model, *n2, *txt_material;
	M_Text *text;
	M_FontStyle *fs;
	char *fontName;
	char szStyle[1024];
	u32 fontSize, styleFlags, color, i, start_char;

	if (!tc->srec) {
		fontName = ttd_find_font(tsd, tsd->default_style.fontID);
		fontSize = tsd->default_style.font_size;
		styleFlags = tsd->default_style.style_flags;
		color = tsd->default_style.text_color;
		if (!color && !tsd->back_color)
			color = 0xFFFFFFFF;

		u32 def_font_size = ctx->scene->compositor->subfs;
		if (!def_font_size) {
			if (ctx->cfg->text_height > 2000) def_font_size = 80;
			else if (ctx->cfg->text_height > 700) def_font_size = 40;
			else def_font_size = 20;
		}

		if (!fontSize || (def_font_size>fontSize)) {
			fontSize = def_font_size;
		}
	} else {
		fontName = ttd_find_font(tsd, tc->srec->fontID);
		fontSize = tc->srec->font_size;
		styleFlags = tc->srec->style_flags;
		color = tc->srec->text_color;
	}

	/*create base model for text node. It will then be cloned for each text item*/
	txt_model = ttd_create_node(ctx, TAG_MPEG4_Shape, NULL);
	gf_node_register(txt_model, NULL);
	n2 = ttd_create_node(ctx, TAG_MPEG4_Appearance, NULL);
	((M_Shape *)txt_model)->appearance = n2;
	gf_node_register(n2, txt_model);
	txt_material = ttd_create_node(ctx, TAG_MPEG4_Material2D, NULL);
	((M_Appearance *)n2)->material = txt_material;
	gf_node_register(txt_material, n2);

	((M_Material2D *)txt_material)->filled = 1;
	((M_Material2D *)txt_material)->transparency = FIX_ONE - INT2FIX((color>>24) & 0xFF) / 255;
	((M_Material2D *)txt_material)->emissiveColor.red = INT2FIX((color>>16) & 0xFF) / 255;
	((M_Material2D *)txt_material)->emissiveColor.green = INT2FIX((color>>8) & 0xFF) / 255;
	((M_Material2D *)txt_material)->emissiveColor.blue = INT2FIX((color) & 0xFF) / 255;
	/*force 0 lineWidth if blinking (the stupid MPEG-4 default values once again..)*/
	if (tc->has_blink) {
		((M_Material2D *)txt_material)->lineProps = ttd_create_node(ctx, TAG_MPEG4_LineProperties, NULL);
		((M_LineProperties *)((M_Material2D *)txt_material)->lineProps)->width = 0;
		gf_node_register(((M_Material2D *)txt_material)->lineProps, txt_material);
	}

	n2 = ttd_create_node(ctx, TAG_MPEG4_Text, NULL);
	((M_Shape *)txt_model)->geometry = n2;
	gf_node_register(n2, txt_model);
	text = (M_Text *) n2;
	fs = (M_FontStyle *) ttd_create_node(ctx, TAG_MPEG4_FontStyle, NULL);
	gf_free(fs->family.vals[0]);

	/*translate default fonts to MPEG-4/VRML names*/
	if (!stricmp(fontName, "Serif")) fs->family.vals[0] = gf_strdup("SERIF");
	else if (!stricmp(fontName, "Sans-Serif")) fs->family.vals[0] = gf_strdup("SANS");
	else if (!stricmp(fontName, "Monospace")) fs->family.vals[0] = gf_strdup("TYPEWRITER");
	else fs->family.vals[0] = gf_strdup(fontName);

	fs->size = INT2FIX(fontSize);
	gf_free(fs->style.buffer);
	strcpy(szStyle, "");
	if (styleFlags & GF_TXT_STYLE_BOLD) {
		if (styleFlags & GF_TXT_STYLE_ITALIC) strcpy(szStyle, "BOLDITALIC");
		else strcpy(szStyle, "BOLD");
	} else if (styleFlags & GF_TXT_STYLE_ITALIC) strcat(szStyle, "ITALIC");
	if (!strlen(szStyle)) strcpy(szStyle, "PLAIN");
	/*also underline for URLs*/
	if ((styleFlags & GF_TXT_STYLE_UNDERLINED) || (tc->hlink && tc->hlink->URL)) strcat(szStyle, " UNDERLINED");
	if (styleFlags & GF_TXT_STYLE_STRIKETHROUGH) strcat(szStyle, " STRIKETHROUGH");

	if (tc->is_hilight) {
		if (tc->hilight_col) {
			char szTxt[50];
			sprintf(szTxt, " HIGHLIGHT#%x", tc->hilight_col);
			strcat(szStyle, szTxt);
		} else {
			strcat(szStyle, " HIGHLIGHT#RV");
		}
	}
	/*a better way would be to draw the entire text box in a composite texture & bitmap but we can't really rely
	on text box size (in MP4Box, it actually defaults to the entire video area) and drawing a too large texture
	& bitmap could slow down rendering*/
	if (ctx->texture) strcat(szStyle, " TEXTURED");
	if (ctx->outline) strcat(szStyle, " OUTLINED");

	fs->style.buffer = gf_strdup(szStyle);
	fs->horizontal = (tsd->displayFlags & GF_TXT_VERTICAL) ? 0 : 1;
	text->fontStyle = (GF_Node *) fs;
	gf_node_register((GF_Node *)fs, (GF_Node *)text);
	gf_sg_vrml_mf_reset(&text->string, GF_SG_VRML_MFSTRING);

	if (tc->hlink && tc->hlink->URL) {
		SFURL *s;
		M_Anchor *anc = (M_Anchor *) ttd_create_node(ctx, TAG_MPEG4_Anchor, NULL);
		gf_sg_vrml_mf_append(&anc->url, GF_SG_VRML_MFURL, (void **) &s);
		s->OD_ID = 0;
		s->url = gf_strdup(tc->hlink->URL);
		if (tc->hlink->URL_hint) anc->description.buffer = gf_strdup(tc->hlink->URL_hint);
		gf_node_list_add_child(& anc->children, txt_model);
		gf_node_register(txt_model, (GF_Node *)anc);
		gf_node_unregister(txt_model, NULL);
		txt_model = (GF_Node *)anc;
		gf_node_register((GF_Node *)anc, NULL);
	}

	start_char = tc->start_char;
	for (i=tc->start_char; i<tc->end_char; i++) {
		Bool new_line = GF_FALSE;
		if (utf16_txt[i] == '\r') continue;
		if ((utf16_txt[i] == '\n') || (utf16_txt[i] == '\r') || (utf16_txt[i] == 0x85) || (utf16_txt[i] == 0x2028) || (utf16_txt[i] == 0x2029))
			new_line = GF_TRUE;

		if (new_line || (i+1==tc->end_char) ) {
			SFString *st;

			if (i+1==tc->end_char) i++;

			if (i!=start_char) {
				char szLine[5000];
				u32 len;
				s16 wsChunk[5000], *sp;

				/*splitting lines, duplicate node*/

				n2 = gf_node_clone(ctx->scenegraph, txt_model, NULL, "", GF_TRUE);
				if (tc->hlink && tc->hlink->URL) {
					GF_Node *t = ((M_Anchor *)n2)->children->node;
					text = (M_Text *) ((M_Shape *)t)->geometry;
					txt_material = ((M_Appearance *) ((M_Shape *)t)->appearance)->material;
				} else {
					text = (M_Text *) ((M_Shape *)n2)->geometry;
					txt_material = ((M_Appearance *) ((M_Shape *)n2)->appearance)->material;
				}
				gf_sg_vrml_mf_reset(&text->string, GF_SG_VRML_MFSTRING);
				gf_node_list_add_child( &form->children, n2);
				gf_node_register(n2, (GF_Node *) form);
				ttd_add_item(form);
				/*clone node always register by default*/
				gf_node_unregister(n2, NULL);

				if (tc->has_blink && txt_material) gf_list_add(ctx->blink_nodes, txt_material);


				memcpy(wsChunk, &utf16_txt[start_char], sizeof(s16)*(i-start_char));
				wsChunk[i-start_char] = 0;
				sp = &wsChunk[0];
				len = gf_utf8_wcstombs(szLine, 5000, (const unsigned short **) &sp);
				if (len == GF_UTF8_FAIL) len = 0;
				szLine[len] = 0;
				if (len && (szLine[len-1]=='\r'))
					szLine[len-1] = 0;

				gf_sg_vrml_mf_append(&text->string, GF_SG_VRML_MFSTRING, (void **) &st);
				st->buffer = gf_strdup(szLine);
			}
			start_char = i+1;
			if (new_line) {
				ttd_add_line(form);
				if ((utf16_txt[i]=='\r') && (utf16_txt[i+1]=='\n')) i++;
			}
		}
	}
	gf_node_unregister(txt_model, NULL);
	return;
}


/*mod can be any of TextHighlight, TextKaraoke, TextHyperText, TextBlink*/
static void ttd_split_chunks(GF_TextSample *txt, u32 nb_chars, GF_List *chunks, GF_Box *mod)
{
	TTDTextChunk *tc;
	u32 start_char, end_char;
	u32 i;
	switch (mod->type) {
	/*these 3 can be safelly typecasted to the same struct for start/end char*/
	case GF_ISOM_BOX_TYPE_HLIT:
	case GF_ISOM_BOX_TYPE_HREF:
	case GF_ISOM_BOX_TYPE_BLNK:
		start_char = ((GF_TextHighlightBox *)mod)->startcharoffset;
		end_char = ((GF_TextHighlightBox *)mod)->endcharoffset;
		break;
	case GF_ISOM_BOX_TYPE_KROK:
	default:
		return;
	}

	if (end_char>nb_chars) end_char = nb_chars;

	i=0;
	while ((tc = (TTDTextChunk *)gf_list_enum(chunks, &i))) {
		if (tc->end_char<=start_char) continue;
		/*need to split chunk at begin*/
		if (tc->start_char<start_char) {
			TTDTextChunk *tc2;
			tc2 = (TTDTextChunk *) gf_malloc(sizeof(TTDTextChunk));
			memcpy(tc2, tc, sizeof(TTDTextChunk));
			tc2->start_char = start_char;
			tc2->end_char = tc->end_char;
			tc->end_char = start_char;
			gf_list_insert(chunks, tc2, i+1);
			i++;
			tc = tc2;
		}
		/*need to split chunks at end*/
		if (tc->end_char>end_char) {
			TTDTextChunk *tc2;
			tc2 = (TTDTextChunk *) gf_malloc(sizeof(TTDTextChunk));
			memcpy(tc2, tc, sizeof(TTDTextChunk));
			tc2->start_char = tc->start_char;
			tc2->end_char = end_char;
			tc->start_char = end_char;
			gf_list_insert(chunks, tc2, i);
			i++;
			tc = tc2;
		}
		/*assign mod*/
		switch (mod->type) {
		case GF_ISOM_BOX_TYPE_HLIT:
			tc->is_hilight = GF_TRUE;
			if (txt->highlight_color) tc->hilight_col = txt->highlight_color->hil_color;
			break;
		case GF_ISOM_BOX_TYPE_HREF:
			tc->hlink = (GF_TextHyperTextBox *) mod;
			break;
		case GF_ISOM_BOX_TYPE_BLNK:
			tc->has_blink = GF_TRUE;
			break;
		}
		/*done*/
		if (tc->end_char==end_char) return;
	}
}


static void ttd_apply_sample(GF_TTXTDec *ctx, GF_TextSample *txt, u32 sample_desc_index, Bool is_utf_16, u32 sample_duration)
{
	u32 i, nb_lines, start_idx, count;
	s32 *id, thw, thh, tw, th, offset;
	Bool vertical;
	MFInt32 idx;
	SFString *s;
	GF_BoxRecord br;
	M_Material2D *n;
	M_Form *form;
	u16 utf16_text[5000];
	u32 char_offset, char_count;
	GF_List *chunks;
	TTDTextChunk *tc;
	GF_Box *a;
	GF_TextSampleDescriptor *td = NULL;

	/*stop timer sensor*/
	if (gf_list_count(ctx->blink_nodes)) {
		ctx->ts_blink->stopTime = gf_node_get_scene_time((GF_Node *) ctx->ts_blink);
		gf_node_changed((GF_Node *) ctx->ts_blink, NULL);
	}
	ctx->ts_scroll->stopTime = gf_node_get_scene_time((GF_Node *) ctx->ts_scroll);
	gf_node_changed((GF_Node *) ctx->ts_scroll, NULL);
	/*flush routes to avoid getting the set_fraction of the scroll sensor deactivation*/
	gf_sg_activate_routes(ctx->scene->graph);

	ttd_reset_display(ctx);
	if (!sample_desc_index || !txt || !txt->len) return;

	if (ctx->is_tx3g) {
		td = (GF_TextSampleDescriptor *)gf_list_get(ctx->cfg->sample_descriptions, 0);
	} else {
		i=0;
		while ((td = (GF_TextSampleDescriptor *)gf_list_enum(ctx->cfg->sample_descriptions, &i))) {
			if (td->sample_index==sample_desc_index) break;
			td = NULL;
		}
	}
	if (!td) return;


	vertical = (td->displayFlags & GF_TXT_VERTICAL) ? GF_TRUE : GF_FALSE;

	/*set back color*/
	/*do we fill the text box or the entire text track region*/
	if (td->displayFlags & GF_TXT_FILL_REGION) {
		ctx->mat_box->transparency = FIX_ONE;
		n = ctx->mat_track;
	} else {
		ctx->mat_track->transparency = FIX_ONE;
		n = ctx->mat_box;
	}

	n->transparency = FIX_ONE - INT2FIX((td->back_color>>24) & 0xFF) / 255;
	n->emissiveColor.red = INT2FIX((td->back_color>>16) & 0xFF) / 255;
	n->emissiveColor.green = INT2FIX((td->back_color>>8) & 0xFF) / 255;
	n->emissiveColor.blue = INT2FIX((td->back_color) & 0xFF) / 255;
	gf_node_changed((GF_Node *) n, NULL);

	if (txt->box) {
		br = txt->box->box;
	} else {
		br = td->default_pos;
	}
	if (!br.right || !br.bottom) {
		br.top = br.left = 0;
		br.right = ctx->cfg->text_width;
		br.bottom = ctx->cfg->text_height;
	}

	thw = br.right - br.left;
	thh = br.bottom - br.top;
	if (!thw || !thh) {
		br.top = br.left = 0;
		thw = ctx->cfg->text_width;
		thh = ctx->cfg->text_height;
	}

	ctx->dlist->size.x = INT2FIX(thw);
	ctx->dlist->size.y = INT2FIX(thh);

	/*disable backgrounds if not used*/
	if (ctx->mat_track->transparency<FIX_ONE) {
		if (ctx->rec_track->size.x != ctx->cfg->text_width) {
			ctx->rec_track->size.x = ctx->cfg->text_width;
			ctx->rec_track->size.y = ctx->cfg->text_height;
			gf_node_changed((GF_Node *) ctx->rec_track, NULL);
		}
	} else if (ctx->rec_track->size.x) {
		ctx->rec_track->size.x = ctx->rec_track->size.y = 0;
		gf_node_changed((GF_Node *) ctx->rec_box, NULL);
	}

	if (ctx->mat_box->transparency<FIX_ONE) {
		if (ctx->rec_box->size.x != ctx->dlist->size.x) {
			ctx->rec_box->size.x = ctx->dlist->size.x;
			ctx->rec_box->size.y = ctx->dlist->size.y;
			gf_node_changed((GF_Node *) ctx->rec_box, NULL);
		}
	} else if (ctx->rec_box->size.x) {
		ctx->rec_box->size.x = ctx->rec_box->size.y = 0;
		gf_node_changed((GF_Node *) ctx->rec_box, NULL);
	}

	form = (M_Form *) ttd_create_node(ctx, TAG_MPEG4_Form, NULL);
	form->size.x = INT2FIX(thw);
	form->size.y = INT2FIX(thh);

	thw /= 2;
	thh /= 2;
	tw = ctx->cfg->text_width;
	th = ctx->cfg->text_height;

	/*check translation, we must not get out of scene size - not supported in GPAC*/
	offset = br.left - tw/2 + thw;
	if (offset + thw < - tw/2) offset = - tw/2 + thw;
	else if (offset - thw > tw/2) offset = tw/2 - thw;
	offset += ctx->scene->compositor->subtx;
	ctx->tr_box->translation.x = INT2FIX(offset);

	offset = th/2 - br.top - thh;
	if (offset + thh > th/2) offset = th/2 - thh;
	else if (offset - thh < -th/2) offset = -th/2 + thh;
	offset += ctx->scene->compositor->subty;
	ctx->tr_box->translation.y = INT2FIX(offset);

	gf_node_dirty_set((GF_Node *)ctx->tr_box, 0, GF_TRUE);


	if (ctx->scroll_type) {
		ctx->ts_scroll->stopTime = gf_node_get_scene_time((GF_Node *) ctx->ts_scroll);
		gf_node_changed((GF_Node *) ctx->ts_scroll, NULL);
	}
	ctx->scroll_mode = 0;
	if (td->displayFlags & GF_TXT_SCROLL_IN) ctx->scroll_mode |= GF_TXT_SCROLL_IN;
	if (td->displayFlags & GF_TXT_SCROLL_OUT) ctx->scroll_mode |= GF_TXT_SCROLL_OUT;

	ctx->scroll_type = 0;
	if (ctx->scroll_mode) {
		ctx->scroll_type = (td->displayFlags & GF_TXT_SCROLL_DIRECTION)>>7;
		ctx->scroll_type ++;
	}
	/*no sample duration, cannot determine scroll rate, so just show*/
	if (!sample_duration) ctx->scroll_type = 0;
	/*no scroll*/
	if (!ctx->scroll_mode) ctx->scroll_type = 0;

	if (ctx->scroll_type) {
		ctx->tr_scroll = (M_Transform2D *) ttd_create_node(ctx, TAG_MPEG4_Transform2D, NULL);
		gf_node_list_add_child( &ctx->dlist->children, (GF_Node*)ctx->tr_scroll);
		gf_node_register((GF_Node *) ctx->tr_scroll, (GF_Node *) ctx->dlist);
		gf_node_list_add_child( &ctx->tr_scroll->children, (GF_Node*)form);
		gf_node_register((GF_Node *) form, (GF_Node *) ctx->tr_scroll);
		ctx->tr_scroll->translation.x = ctx->tr_scroll->translation.y = (ctx->scroll_mode & GF_TXT_SCROLL_IN) ? -INT2FIX(1000) : 0;
		/*if no delay, text is in motion for the duration of the sample*/
		ctx->scroll_time = FIX_ONE;
		ctx->scroll_delay = 0;

		if (txt->scroll_delay) {
			ctx->scroll_delay = gf_divfix(INT2FIX(txt->scroll_delay->scroll_delay), INT2FIX(sample_duration));
			if (ctx->scroll_delay>FIX_ONE) ctx->scroll_delay = FIX_ONE;
			ctx->scroll_time = (FIX_ONE - ctx->scroll_delay);
		}
		/*if both scroll (in and out), use same scroll duration for both*/
		if ((ctx->scroll_mode & GF_TXT_SCROLL_IN) && (ctx->scroll_mode & GF_TXT_SCROLL_OUT)) ctx->scroll_time /= 2;

	} else {
		gf_node_list_add_child( &ctx->dlist->children, (GF_Node*)form);
		gf_node_register((GF_Node *) form, (GF_Node *) ctx->dlist);
		ctx->tr_scroll = NULL;
	}

	if (is_utf_16) {
		memcpy((char *) utf16_text, txt->text, sizeof(char) * txt->len);
		((char *) utf16_text)[txt->len] = 0;
		((char *) utf16_text)[txt->len+1] = 0;
		char_count = txt->len / 2;
	} else {
		char *p = txt->text;
		char_count = gf_utf8_mbstowcs(utf16_text, 2500, (const char **) &p);
		if (char_count == GF_UTF8_FAIL) char_count = 0;
	}

	chunks = gf_list_new();
	/*flatten all modifiers*/
	if (!txt->styles || !txt->styles->entry_count) {
		GF_SAFEALLOC(tc, TTDTextChunk);
		if (!tc) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_CODEC, ("[TimedText] Failed to allocate text chunk\n"));
		} else {
			tc->end_char = char_count;
			gf_list_add(chunks, tc);
		}
	} else {
		GF_StyleRecord *srec = NULL;
		char_offset = 0;
		for (i=0; i<txt->styles->entry_count; i++) {
			srec = &txt->styles->styles[i];
			if (srec->startCharOffset==srec->endCharOffset) continue;
			/*handle not continuous modifiers*/
			if (char_offset < srec->startCharOffset) {
				GF_SAFEALLOC(tc, TTDTextChunk);
				if (!tc) {
					GF_LOG(GF_LOG_ERROR, GF_LOG_CODEC, ("[TimedText] Failed to allocate text chunk\n"));
				} else {
					tc->start_char = char_offset;
					tc->end_char = srec->startCharOffset;
					gf_list_add(chunks, tc);
				}
			}
			GF_SAFEALLOC(tc, TTDTextChunk);
			if (!tc) {
				GF_LOG(GF_LOG_ERROR, GF_LOG_CODEC, ("[TimedText] Failed to allocate text chunk\n"));
			} else {
				tc->start_char = srec->startCharOffset;
				tc->end_char = srec->endCharOffset;
				tc->srec = srec;
				gf_list_add(chunks, tc);
			}
			char_offset = srec->endCharOffset;
		}

		if (srec->endCharOffset<char_count) {
			GF_SAFEALLOC(tc, TTDTextChunk);
			if (!tc) {
				GF_LOG(GF_LOG_ERROR, GF_LOG_CODEC, ("[TimedText] Failed to allocate text chunk\n"));
			} else {
				tc->start_char = char_offset;
				tc->end_char = char_count;
				gf_list_add(chunks, tc);
			}
		}
	}
	/*apply all other modifiers*/
	i=0;
	while ((a = (GF_Box*)gf_list_enum(txt->others, &i))) {
		ttd_split_chunks(txt, char_count, chunks, a);
	}

	while (gf_list_count(chunks)) {
		tc = (TTDTextChunk*)gf_list_get(chunks, 0);
		gf_list_rem(chunks, 0);
		ttd_new_text_chunk(ctx, td, form, utf16_text, tc);
		gf_free(tc);
	}
	gf_list_del(chunks);

	if (form->groupsIndex.count && (form->groupsIndex.vals[form->groupsIndex.count-1] != -1))
		ttd_add_line(form);

	/*rewrite form groupIndex - group is fine (eg one child per group)*/
	idx.count = form->groupsIndex.count;
	idx.vals = form->groupsIndex.vals;
	form->groupsIndex.vals = NULL;
	form->groupsIndex.count = 0;

	nb_lines = 0;
	start_idx = 0;
	for (i=0; i<idx.count; i++) {
		if (idx.vals[i] == -1) {
			u32 j;

			/*only one item in line, no need for alignment, but still add a group (we could use the
			item as a group but that would complicate the alignment generation)*/
			if (start_idx==i-1) {
				gf_sg_vrml_mf_append(&form->groups, GF_SG_VRML_MFINT32, (void **) &id);
				(*id) = idx.vals[start_idx];
				gf_sg_vrml_mf_append(&form->groups, GF_SG_VRML_MFINT32, (void **) &id);
				(*id) = -1;
			} else {
				/*spread horizontal 0 pixels (eg align) all items in line*/
				gf_sg_vrml_mf_append(&form->constraints, GF_SG_VRML_MFSTRING, (void **) &s);
				s->buffer = gf_strdup(vertical ? "SV 0" : "SH 0");
				for (j=start_idx; j<i; j++) {
					gf_sg_vrml_mf_append(&form->groupsIndex, GF_SG_VRML_MFINT32, (void **) &id);
					(*id) = idx.vals[j];
					/*also add a group for the line, for final justif*/
					gf_sg_vrml_mf_append(&form->groups, GF_SG_VRML_MFINT32, (void **) &id);
					(*id) = idx.vals[j];
				}
				gf_sg_vrml_mf_append(&form->groupsIndex, GF_SG_VRML_MFINT32, (void **) &id);
				(*id) = -1;
				/*mark end of group*/
				gf_sg_vrml_mf_append(&form->groups, GF_SG_VRML_MFINT32, (void **) &id);
				(*id) = -1;
			}
			start_idx = i+1;
			nb_lines ++;
		}
	}
	gf_free(idx.vals);

	/*finally add constraints on lines*/
	start_idx = gf_node_list_get_count(form->children) + 1;
	/*horizontal alignment*/
	gf_sg_vrml_mf_append(&form->constraints, GF_SG_VRML_MFSTRING, (void **) &s);
	if (vertical) {
		switch (td->vert_justif) {
		case 1:
			s->buffer = gf_strdup("AV");
			break;/*center*/
		case -1:
			s->buffer = gf_strdup("AB");
			break;/*bottom*/
		default:
			s->buffer = gf_strdup("AT");
			break;/*top*/
		}
	} else {
		switch (td->horiz_justif) {
		case 1:
			s->buffer = gf_strdup("AH");
			break;/*center*/
		case -1:
			s->buffer = gf_strdup("AR");
			break;/*right*/
		default:
			s->buffer = gf_strdup("AL");
			break;/*left*/
		}
	}
	gf_sg_vrml_mf_append(&form->groupsIndex, GF_SG_VRML_MFINT32, (void **) &id);
	(*id) = 0;
	for (i=0; i<nb_lines; i++) {
		gf_sg_vrml_mf_append(&form->groupsIndex, GF_SG_VRML_MFINT32, (void **) &id);
		(*id) = i+start_idx;
	}
	gf_sg_vrml_mf_append(&form->groupsIndex, GF_SG_VRML_MFINT32, (void **) &id);
	(*id) = -1;


	/*vertical alignment: first align all items vertically, 0 pixel */
	gf_sg_vrml_mf_append(&form->constraints, GF_SG_VRML_MFSTRING, (void **) &s);
	s->buffer = gf_strdup(vertical ? "SH 0" : "SV 0");
	gf_sg_vrml_mf_append(&form->groupsIndex, GF_SG_VRML_MFINT32, (void **) &id);
	(*id) = 0;
	for (i=0; i<nb_lines; i++) {
		gf_sg_vrml_mf_append(&form->groupsIndex, GF_SG_VRML_MFINT32, (void **) &id);
		(*id) = i+start_idx;
	}
	gf_sg_vrml_mf_append(&form->groupsIndex, GF_SG_VRML_MFINT32, (void **) &id);
	(*id) = -1;

	/*define a group with every item drawn*/
	count = gf_node_list_get_count(form->children);
	for (i=0; i<count; i++) {
		gf_sg_vrml_mf_append(&form->groups, GF_SG_VRML_MFINT32, (void **) &id);
		(*id) = i+1;
	}
	gf_sg_vrml_mf_append(&form->groups, GF_SG_VRML_MFINT32, (void **) &id);
	(*id) = -1;

	gf_sg_vrml_mf_append(&form->constraints, GF_SG_VRML_MFSTRING, (void **) &s);
	if (vertical) {
		switch (td->horiz_justif) {
		case 1:
			s->buffer = gf_strdup("AH");
			break;/*center*/
		case -1:
			s->buffer = gf_strdup("AR");
			break;/*right*/
		default:
			s->buffer = gf_strdup("AL");
			break;/*left*/
		}
	} else {
		switch (td->vert_justif) {
		case 1:
			s->buffer = gf_strdup("AV");
			break;/*center*/
		case -1:
			s->buffer = gf_strdup("AB");
			break;/*bottom*/
		default:
			s->buffer = gf_strdup("AT");
			break;/*top*/
		}
	}
	gf_sg_vrml_mf_append(&form->groupsIndex, GF_SG_VRML_MFINT32, (void **) &id);
	(*id) = 0;
	gf_sg_vrml_mf_append(&form->groupsIndex, GF_SG_VRML_MFINT32, (void **) &id);
	(*id) = start_idx + nb_lines;
	gf_sg_vrml_mf_append(&form->groupsIndex, GF_SG_VRML_MFINT32, (void **) &id);
	(*id) = -1;


	gf_node_dirty_set((GF_Node *)form, 0, GF_TRUE);
	gf_node_changed((GF_Node *)form, NULL);
	gf_node_changed((GF_Node *) ctx->dlist, NULL);

	if (gf_list_count(ctx->blink_nodes)) {
		/*restart time sensor*/
		ctx->ts_blink->startTime = gf_node_get_scene_time((GF_Node *) ctx->ts_blink);
		gf_node_changed((GF_Node *) ctx->ts_blink, NULL);
	}

	ctx->timer_active = GF_TRUE;
	/*scroll timer also acts as AU timer*/
	ctx->ts_scroll->startTime = gf_node_get_scene_time((GF_Node *) ctx->ts_scroll);
	ctx->ts_scroll->stopTime = ctx->ts_scroll->startTime - 1.0;
	ctx->ts_scroll->cycleInterval = sample_duration;
	ctx->ts_scroll->cycleInterval /= ctx->cfg->timescale;
	ctx->ts_scroll->cycleInterval -= 0.1;
	gf_node_changed((GF_Node *) ctx->ts_scroll, NULL);
}

static void ttd_toggle_display(GF_TTXTDec *ctx)
{
	if (!ctx->scenegraph) return;

	if (ctx->is_playing) {
		if (!ctx->graph_registered) {
			ttd_reset_display(ctx);
			ttd_update_size_info(ctx);
			gf_scene_register_extra_graph(ctx->scene, ctx->scenegraph, GF_FALSE);
			ctx->graph_registered = GF_TRUE;
		}
	 } else {
		if (ctx->graph_registered) {
			gf_scene_register_extra_graph(ctx->scene, ctx->scenegraph, GF_TRUE);
			ctx->graph_registered = GF_FALSE;
		}
	}
}

static Bool ttd_process_event(GF_Filter *filter, const GF_FilterEvent *evt)
{
	GF_TTXTDec *ctx = gf_filter_get_udta(filter);

	//check for scene attach
	switch (evt->base.type) {
	case GF_FEVT_ATTACH_SCENE:
		break;
	case GF_FEVT_RESET_SCENE:
		if (ctx->opid != evt->attach_scene.on_pid) return GF_TRUE;
		ctx->is_playing = GF_FALSE;
		ttd_toggle_display(ctx);
		ttd_reset_scene(ctx);
		ctx->scene = NULL;
		return GF_TRUE;
	case GF_FEVT_PLAY:
		ctx->is_playing = GF_TRUE;
		ttd_toggle_display(ctx);
		return GF_FALSE;
	case GF_FEVT_STOP:
		ctx->is_playing = GF_FALSE;
		ttd_toggle_display(ctx);
		return GF_FALSE;
	default:
		return GF_FALSE;
	}
	if (ctx->opid != evt->attach_scene.on_pid) return GF_TRUE;

	ctx->odm = evt->attach_scene.object_manager;
	ctx->scene = ctx->odm->subscene ? ctx->odm->subscene : ctx->odm->parentscene;

	/*timedtext cannot be a root scene object*/
	if (ctx->odm->subscene) {
		ctx->odm = NULL;
		ctx->scene = NULL;
	 } else {
		if (ctx->cfg) {
			ttd_setup_scene(ctx);
			ttd_toggle_display(ctx);
		}
	 }
	 return GF_TRUE;
}

static GF_Err ttd_render_simple_text(GF_TTXTDec *ctx, const char *pck_data, u32 pck_size, u32 sample_duration)
{
	GF_TextSample static_txts;
	memset(&static_txts, 0, sizeof (GF_TextSample));
	if (ctx->txt_static_alloc < pck_size+1) {
		ctx->static_text = gf_realloc(ctx->static_text, pck_size+1);
		if (ctx->static_text) ctx->txt_static_alloc = pck_size+1;
		else return GF_OUT_OF_MEM;
	}
	memcpy(ctx->static_text, pck_data, pck_size);
	ctx->static_text[pck_size] = 0;
	static_txts.text = ctx->static_text;
	static_txts.len = pck_size;
	ttd_apply_sample(ctx, &static_txts, 1, GF_FALSE, sample_duration);
	return GF_OK;
}
static GF_Err ttd_process(GF_Filter *filter)
{
	const char *pck_data;
	u32 pck_size, obj_time;
	u64 cts;
	GF_FilterPacket *pck;
	GF_TTXTDec *ctx = gf_filter_get_udta(filter);

	if (!ctx->scene) {
		if (ctx->is_playing) {
			gf_filter_pid_set_eos(ctx->opid);
			return GF_EOS;
		}
		return GF_OK;
	}

	pck = gf_filter_pid_get_packet(ctx->ipid);
	if (!pck) {
		if (gf_filter_pid_is_eos(ctx->ipid)) {
			if (!ctx->is_eos) {
				gf_filter_pid_set_eos(ctx->opid);
				ctx->ts_blink->stopTime = gf_node_get_scene_time((GF_Node *) ctx->ts_blink);
				gf_node_changed((GF_Node *) ctx->ts_blink, NULL);
				ctx->ts_scroll->stopTime = gf_node_get_scene_time((GF_Node *) ctx->ts_scroll);
				gf_node_changed((GF_Node *) ctx->ts_scroll, NULL);
				ctx->is_eos = GF_TRUE;
			}
			return GF_EOS;
		}
		if (!ctx->sample_end)
			return GF_OK;
	}
	ctx->is_eos = GF_FALSE;

	//object clock shall be valid
	assert(ctx->odm->ck);
	if (pck) {
		s64 delay;
		cts = gf_filter_pck_get_cts( pck );

		delay = ctx->scene->compositor->subd;
		if (delay)
			delay = gf_timestamp_rescale_signed(delay, 1000, ctx->timescale);
		delay += ctx->delay;

		if (delay>=0) cts += delay;
		else if (cts > -delay) cts -= -delay;
		else cts = 0;
	} else {
		cts = ctx->sample_end;
	}
	if (ctx->sample_end && (ctx->sample_end<cts)) {
		cts = ctx->sample_end;
		pck = NULL;
	}

	gf_odm_check_buffering(ctx->odm, ctx->ipid);

	//we still process any frame before our clock time even when buffering
	obj_time = gf_clock_time(ctx->odm->ck);

	if (gf_timestamp_greater(cts, ctx->timescale, obj_time, 1000)) {
		Double ts_offset = (Double) cts;
		ts_offset /= ctx->timescale;

		gf_sc_sys_frame_pending(ctx->scene->compositor, ts_offset, obj_time, filter);
		return GF_OK;
	}

	if (!pck) {
		GF_Err e = ttd_render_simple_text(ctx, NULL, 0, 0);
		ctx->sample_end = 0;
		return e;
	}

	pck_data = gf_filter_pck_get_data(pck, &pck_size);

	if (ctx->simple_text) {
		u32 sample_duration = gf_filter_pck_get_duration(pck);
		GF_Err e = ttd_render_simple_text(ctx, pck_data, pck_size, sample_duration);
		gf_filter_pid_drop_packet(ctx->ipid);
		ctx->sample_end = sample_duration + cts;
		return e;
	}
	gf_bs_reassign_buffer(ctx->bs_r, pck_data, pck_size);

	while (gf_bs_available(ctx->bs_r)) {
		GF_TextSample *txt;
		Bool is_utf_16=0;
		u32 type, /*length, */sample_index, sample_duration;

		if (!ctx->is_tx3g) {
			is_utf_16 = (Bool)gf_bs_read_int(ctx->bs_r, 1);
			gf_bs_read_int(ctx->bs_r, 4);
			type = gf_bs_read_int(ctx->bs_r, 3);
			/*length = */gf_bs_read_u16(ctx->bs_r);

			/*currently only full text samples are supported*/
			if (type != 1) {
				return GF_NOT_SUPPORTED;
			}
			sample_index = gf_bs_read_u8(ctx->bs_r);
			/*duration*/
			sample_duration = gf_bs_read_u24(ctx->bs_r);
		} else {
			sample_index = 1;
			/*duration*/
			sample_duration = gf_filter_pck_get_duration(pck);
		}
		/*txt length is parsed with the sample*/
		txt = gf_isom_parse_text_sample(ctx->bs_r);
		if (!txt) return GF_NON_COMPLIANT_BITSTREAM;
		GF_LOG(GF_LOG_DEBUG, GF_LOG_CODEC, ("[TTXTDec] Applying new sample - duration %d text \"%s\"\n", sample_duration, txt->text ? txt->text : ""));
		ttd_apply_sample(ctx, txt, sample_index, is_utf_16, sample_duration);
		gf_isom_delete_text_sample(txt);

		/*since we support only TTU(1), no need to go on*/
		if (!ctx->is_tx3g) {
			break;
		} else {
			//tx3g mode, single sample per AU
			assert(gf_bs_available(ctx->bs_r)==0);
			break;
		}
	}
	gf_filter_pid_drop_packet(ctx->ipid);
	return GF_OK;
}

static GF_Err ttd_initialize(GF_Filter *filter)
{
	GF_TTXTDec *ctx = gf_filter_get_udta(filter);
	ctx->bs_r = gf_bs_new((char *) "", 1, GF_BITSTREAM_READ);
	return GF_OK;
}

void ttd_finalize(GF_Filter *filter)
{
	GF_TTXTDec *ctx = gf_filter_get_udta(filter);

	ttd_reset_scene(ctx);

	if (ctx->cfg) gf_odf_desc_del((GF_Descriptor *) ctx->cfg);
	gf_bs_del(ctx->bs_r);

	if (ctx->static_text) gf_free(ctx->static_text);
}


#define OFFS(_n)	#_n, offsetof(GF_TTXTDec, _n)
static const GF_FilterArgs TTXTDecArgs[] =
{
	{ OFFS(texture), "use texturing for output text", GF_PROP_BOOL, "false", NULL, GF_FS_ARG_HINT_ADVANCED},
	{ OFFS(outline), "draw text outline", GF_PROP_BOOL, "false", NULL, GF_FS_ARG_HINT_ADVANCED},
	{ OFFS(txtw), "default width in standalone rendering", GF_PROP_UINT, "400", NULL, 0},
	{ OFFS(txth), "default height in standalone rendering", GF_PROP_UINT, "200", NULL, 0},
	{0}
};

static const GF_FilterCapability TTXTDecCaps[] =
{
	CAP_UINT(GF_CAPS_INPUT,GF_PROP_PID_STREAM_TYPE, GF_STREAM_TEXT),
	CAP_BOOL(GF_CAPS_INPUT_EXCLUDED, GF_PROP_PID_UNFRAMED, GF_TRUE),
	CAP_UINT(GF_CAPS_INPUT,GF_PROP_PID_CODECID, GF_CODECID_TEXT_MPEG4),
	CAP_UINT(GF_CAPS_INPUT,GF_PROP_PID_CODECID, GF_CODECID_TX3G),
	CAP_UINT(GF_CAPS_INPUT,GF_PROP_PID_CODECID, GF_CODECID_SUBS_TEXT),
	CAP_UINT(GF_CAPS_INPUT,GF_PROP_PID_CODECID, GF_CODECID_SIMPLE_TEXT),
	CAP_UINT(GF_CAPS_OUTPUT, GF_PROP_PID_STREAM_TYPE, GF_STREAM_TEXT),
	CAP_UINT(GF_CAPS_OUTPUT, GF_PROP_PID_CODECID, GF_CODECID_RAW),
};

GF_FilterRegister TTXTDecRegister = {
	.name = "ttxtdec",
	GF_FS_SET_DESCRIPTION("TTXT/TX3G decoder")
	GF_FS_SET_HELP("This filter decodes TTXT/TX3G streams into a BIFS scene graph of the compositor filter.\n"
	"The TTXT documentation is available at https://wiki.gpac.io/TTXT-Format-Documentation\n")
	.private_size = sizeof(GF_TTXTDec),
	.flags = GF_FS_REG_MAIN_THREAD,
	.args = TTXTDecArgs,
	.priority = 1,
	SETCAPS(TTXTDecCaps),
	.initialize = ttd_initialize,
	.finalize = ttd_finalize,
	.process = ttd_process,
	.configure_pid = ttd_configure_pid,
	.process_event = ttd_process_event,
};

#endif

const GF_FilterRegister *ttxtdec_register(GF_FilterSession *session)
{
#if !defined(GPAC_DISABLE_VRML) && !defined(GPAC_DISABLE_ISOM)
	return &TTXTDecRegister;
#else
	return NULL;
#endif
}




/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2000-2012
 *					All rights reserved
 *
 *  This file is part of GPAC / 3GPP/MPEG4 timed text module
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

#include <gpac/internal/terminal_dev.h>
#include <gpac/internal/isomedia_dev.h>
#include <gpac/utf.h>
#include <gpac/constants.h>
#include <gpac/nodes_mpeg4.h>

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
	GF_Scene *inlineScene;
	GF_Terminal *app;
	u32 nb_streams;

	GF_TextConfig *cfg;

	GF_SceneGraph *sg;

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
	Bool is_active, use_texture, outline;
} TTDPriv;


static void ttd_set_blink_fraction(GF_Node *node, GF_Route *route);
static void ttd_set_scroll_fraction(GF_Node *node, GF_Route *route);
static void TTD_ResetDisplay(TTDPriv *priv);

/*the WORST thing about 3GP in MPEG4 is positioning of the text track...*/
static void TTD_UpdateSizeInfo(TTDPriv *priv)
{
	u32 w, h;
	Bool has_size;
	s32 offset, thw, thh, vw, vh;

	has_size = gf_sg_get_scene_size_info(priv->inlineScene->graph, &w, &h);
	/*no size info is given in main scene, override by associated video size if any, or by text track size*/
	if (!has_size) {
		if (priv->cfg->has_vid_info && priv->cfg->video_width && priv->cfg->video_height) {
			gf_sg_set_scene_size_info(priv->sg, priv->cfg->video_width, priv->cfg->video_height, 1);
		} else {
			gf_sg_set_scene_size_info(priv->sg, priv->cfg->text_width, priv->cfg->text_height, 1);
		}
		gf_sg_get_scene_size_info(priv->sg, &w, &h);
		if (!w || !h) return;
		gf_scene_force_size(priv->inlineScene, w, h);
	}

	if (!w || !h) return;
	/*apply*/
	gf_sg_set_scene_size_info(priv->sg, w, h, 1);
	/*make sure the scene size is big enough to contain the text track after positioning. RESULTS ARE UNDEFINED
	if offsets are negative: since MPEG-4 uses centered coord system, we must assume video is aligned to top-left*/
	if (priv->cfg->has_vid_info) {
		Bool set_size = 0;
		vw = priv->cfg->horiz_offset;
		if (vw<0) vw = 0;
		vh = priv->cfg->vert_offset;
		if (vh<0) vh = 0;
		if (priv->cfg->text_width + (u32) vw > w) {
			w = priv->cfg->text_width+vw;
			set_size = 1;
		}
		if (priv->cfg->text_height + (u32) vh > h) {
			h = priv->cfg->text_height+vh;
			set_size = 1;
		}
		if (set_size) {
			gf_sg_set_scene_size_info(priv->sg, w, h, 1);
			gf_scene_force_size(priv->inlineScene, w, h);
		}
	} else {
		/*otherwise override (mainly used for SRT & TTXT file direct loading*/
		priv->cfg->text_width = w;
		priv->cfg->text_height = h;
	}

	/*ok override video size with main scene size*/
	priv->cfg->video_width = w;
	priv->cfg->video_height = h;

	vw = (s32) w;
	vh = (s32) h;
	thw = priv->cfg->text_width / 2;
	thh = priv->cfg->text_height / 2;
	/*check translation, we must not get out of scene size - not supported in GPAC*/
	offset = priv->cfg->horiz_offset - vw/2 + thw;
	/*safety checks ?
	if (offset + thw < - vw/2) offset = - vw/2 + thw;
	else if (offset - thw > vw/2) offset = vw/2 - thw;
	*/
	priv->tr_track->translation.x = INT2FIX(offset);

	offset = vh/2 - priv->cfg->vert_offset - thh;
	/*safety checks ?
	if (offset + thh > vh/2) offset = vh/2 - thh;
	else if (offset - thh < -vh/2) offset = -vh/2 + thh;
	*/
	priv->tr_track->translation.y = INT2FIX(offset);

	gf_node_changed((GF_Node *)priv->tr_track, NULL);
}

static GF_Err TTD_GetCapabilities(GF_BaseDecoder *plug, GF_CodecCapability *capability)
{
	TTDPriv *priv = (TTDPriv *)plug->privateStack;
	switch (capability->CapCode) {
	case GF_CODEC_WIDTH:
		capability->cap.valueInt = priv->cfg->text_width;
		return GF_OK;
	case GF_CODEC_HEIGHT:
		capability->cap.valueInt = priv->cfg->text_height;
		return GF_OK;
	case GF_CODEC_MEDIA_NOT_OVER:
		capability->cap.valueInt = priv->is_active;
		return GF_OK;
	default:
		capability->cap.valueInt = 0;
		return GF_OK;
	}
}

static GF_Err TTD_SetCapabilities(GF_BaseDecoder *plug, const GF_CodecCapability capability)
{
	TTDPriv *priv = (TTDPriv *)plug->privateStack;
	if (capability.CapCode==GF_CODEC_SHOW_SCENE) {
		if (capability.cap.valueInt) {
			TTD_ResetDisplay(priv);
			TTD_UpdateSizeInfo(priv);
			gf_scene_register_extra_graph(priv->inlineScene, priv->sg, 0);
		} else {
			gf_scene_register_extra_graph(priv->inlineScene, priv->sg, 1);
		}
	}
	return GF_OK;
}

GF_Err TTD_AttachScene(GF_SceneDecoder *plug, GF_Scene *scene, Bool is_scene_decoder)
{
	TTDPriv *priv = (TTDPriv *)plug->privateStack;
	if (priv->nb_streams) return GF_BAD_PARAM;
	/*timedtext cannot be a root scene object*/
	if (is_scene_decoder) return GF_BAD_PARAM;
	priv->inlineScene = scene;
	priv->app = scene->root_od->term;
	return GF_OK;
}

GF_Err TTD_ReleaseScene(GF_SceneDecoder *plug)
{
	TTDPriv *priv = (TTDPriv *)plug->privateStack;
	if (priv->nb_streams) return GF_BAD_PARAM;
	return GF_OK;
}

static GFINLINE void add_child(GF_Node *n1, GF_Node *par)
{
	gf_node_list_add_child( & ((GF_ParentNode *)par)->children, n1);
	gf_node_register(n1, par);
}


static GFINLINE GF_Node *ttd_create_node(TTDPriv *ttd, u32 tag, const char *def_name)
{
	GF_Node *n = gf_node_new(ttd->sg, tag);
	if (n) {
		if (def_name) gf_node_set_id(n, gf_sg_get_next_available_node_id(ttd->sg), def_name);
		gf_node_init(n);
	}
	return n;
}

static GF_Err TTD_AttachStream(GF_BaseDecoder *plug, GF_ESD *esd)
{
	TTDPriv *priv = (TTDPriv *)plug->privateStack;
	GF_Err e;
	GF_Node *root, *n1, *n2;
	const char *opt;
	/*no scalable, no upstream*/
	if (priv->nb_streams || esd->decoderConfig->upstream) return GF_NOT_SUPPORTED;
	if (!esd->decoderConfig->decoderSpecificInfo || !esd->decoderConfig->decoderSpecificInfo->data) return GF_NON_COMPLIANT_BITSTREAM;

	priv->cfg = (GF_TextConfig *) gf_odf_desc_new(GF_ODF_TEXT_CFG_TAG);
	e = gf_odf_get_text_config(esd->decoderConfig->decoderSpecificInfo, (u8) esd->decoderConfig->objectTypeIndication, priv->cfg);
	if (e) {
		gf_odf_desc_del((GF_Descriptor *) priv->cfg);
		priv->cfg = NULL;
		return e;
	}
	priv->nb_streams++;
	if (!priv->cfg->timescale) priv->cfg->timescale = 1000;

	priv->sg = gf_sg_new_subscene(priv->inlineScene->graph);

	root = ttd_create_node(priv, TAG_MPEG4_OrderedGroup, NULL);
	gf_sg_set_root_node(priv->sg, root);
	gf_node_register(root, NULL);
	/*root transform*/
	priv->tr_track = (M_Transform2D *)ttd_create_node(priv, TAG_MPEG4_Transform2D, NULL);
	add_child((GF_Node *) priv->tr_track, root);

	TTD_UpdateSizeInfo(priv);

	/*txt track background*/
	n1 = ttd_create_node(priv, TAG_MPEG4_Shape, NULL);
	add_child(n1, (GF_Node *) priv->tr_track);
	((M_Shape *)n1)->appearance = ttd_create_node(priv, TAG_MPEG4_Appearance, NULL);
	gf_node_register(((M_Shape *)n1)->appearance, n1);
	priv->mat_track = (M_Material2D *) ttd_create_node(priv, TAG_MPEG4_Material2D, NULL);
	priv->mat_track->filled = 1;
	priv->mat_track->transparency = 1;
	((M_Appearance *) ((M_Shape *)n1)->appearance)->material = (GF_Node *) priv->mat_track;
	gf_node_register((GF_Node *) priv->mat_track, ((M_Shape *)n1)->appearance);
	n2 = ttd_create_node(priv, TAG_MPEG4_Rectangle, NULL);
	((M_Rectangle *)n2)->size.x = 0;
	((M_Rectangle *)n2)->size.y = 0;
	((M_Shape *)n1)->geometry = n2;
	gf_node_register(n2, n1);
	priv->rec_track = (M_Rectangle *)n2;

	/*txt box background*/
	priv->tr_box = (M_Transform2D *) ttd_create_node(priv, TAG_MPEG4_Transform2D, NULL);
	add_child((GF_Node*) priv->tr_box, (GF_Node*)priv->tr_track);
	n1 = ttd_create_node(priv, TAG_MPEG4_Shape, NULL);
	add_child(n1, (GF_Node*)priv->tr_box);
	((M_Shape *)n1)->appearance = ttd_create_node(priv, TAG_MPEG4_Appearance, NULL);
	gf_node_register(((M_Shape *)n1)->appearance, n1);
	priv->mat_box = (M_Material2D *) ttd_create_node(priv, TAG_MPEG4_Material2D, NULL);
	priv->mat_box->filled = 1;
	priv->mat_box->transparency = 1;
	((M_Appearance *) ((M_Shape *)n1)->appearance)->material = (GF_Node *)priv->mat_box;
	gf_node_register((GF_Node *)priv->mat_box, ((M_Shape *)n1)->appearance);
	priv->rec_box = (M_Rectangle *) ttd_create_node(priv, TAG_MPEG4_Rectangle, NULL);
	priv->rec_box->size.x = 0;
	priv->rec_box->size.y = 0;
	((M_Shape *)n1)->geometry = (GF_Node *) priv->rec_box;
	gf_node_register((GF_Node *) priv->rec_box, n1);

	priv->dlist = (M_Layer2D *) ttd_create_node(priv, TAG_MPEG4_Layer2D, NULL);
	priv->dlist->size.x = priv->cfg->text_width;
	priv->dlist->size.y = priv->cfg->text_height;
	add_child((GF_Node *)priv->dlist, (GF_Node *)priv->tr_box);

	priv->blink_nodes = gf_list_new();
	priv->ts_blink = (M_TimeSensor *) ttd_create_node(priv, TAG_MPEG4_TimeSensor, "TimerBlink");
	priv->ts_blink->cycleInterval = 0.25;
	priv->ts_blink->startTime = 0.0;
	priv->ts_blink->loop = 1;
	priv->process_blink = (M_ScalarInterpolator *) ttd_create_node(priv, TAG_MPEG4_ScalarInterpolator, NULL);
	/*override set_fraction*/
	priv->process_blink->on_set_fraction = ttd_set_blink_fraction;
	gf_node_set_private((GF_Node *) priv->process_blink, priv);
	/*route from fraction_changed to set_fraction*/
	gf_sg_route_new(priv->sg, (GF_Node *) priv->ts_blink, 6, (GF_Node *) priv->process_blink, 0);

	priv->ts_scroll = (M_TimeSensor *) ttd_create_node(priv, TAG_MPEG4_TimeSensor, "TimerScroll");
	priv->ts_scroll->cycleInterval = 0;
	priv->ts_scroll->startTime = -1;
	priv->ts_scroll->loop = 0;
	priv->process_scroll = (M_ScalarInterpolator *) ttd_create_node(priv, TAG_MPEG4_ScalarInterpolator, NULL);
	/*override set_fraction*/
	priv->process_scroll->on_set_fraction = ttd_set_scroll_fraction;
	gf_node_set_private((GF_Node *) priv->process_scroll, priv);
	/*route from fraction_changed to set_fraction*/
	gf_sg_route_new(priv->sg, (GF_Node *) priv->ts_scroll, 6, (GF_Node *) priv->process_scroll, 0);

	gf_node_register((GF_Node *) priv->ts_blink, NULL);
	gf_node_register((GF_Node *) priv->process_blink, NULL);
	gf_node_register((GF_Node *) priv->ts_scroll, NULL);
	gf_node_register((GF_Node *) priv->process_scroll, NULL);

	/*option setup*/
	opt = gf_modules_get_option((GF_BaseInterface *)plug, "StreamingText", "UseTexturing");
	priv->use_texture = (opt && !strcmp(opt, "yes")) ? 1 : 0;
	opt = gf_modules_get_option((GF_BaseInterface *)plug, "StreamingText", "OutlineText");
	priv->outline = (opt && !strcmp(opt, "yes")) ? 1 : 0;
	return e;
}

static GF_Err TTD_DetachStream(GF_BaseDecoder *plug, u16 ES_ID)
{
	TTDPriv *priv = (TTDPriv *)plug->privateStack;
	if (!priv->nb_streams) return GF_BAD_PARAM;

	gf_scene_register_extra_graph(priv->inlineScene, priv->sg, 1);

	gf_node_unregister((GF_Node *) priv->ts_blink, NULL);
	gf_node_unregister((GF_Node *) priv->process_blink, NULL);
	gf_node_unregister((GF_Node *) priv->ts_scroll, NULL);
	gf_node_unregister((GF_Node *) priv->process_scroll, NULL);

	gf_sg_del(priv->sg);
	priv->sg = NULL;
	if (priv->cfg) gf_odf_desc_del((GF_Descriptor *) priv->cfg);
	priv->cfg = NULL;
	priv->nb_streams = 0;
	gf_list_del(priv->blink_nodes);
	return GF_OK;
}

static void ttd_set_blink_fraction(GF_Node *node, GF_Route *route)
{
	M_Material2D *m;
	u32 i;
	TTDPriv *priv = (TTDPriv *)gf_node_get_private(node);

	Bool blink_on = 1;
	if (priv->process_blink->set_fraction>FIX_ONE/2) blink_on = 0;
	i=0;
	while ((m = (M_Material2D*)gf_list_enum(priv->blink_nodes, &i))) {
		if (m->filled != blink_on) {
			m->filled = blink_on;
			gf_node_changed((GF_Node *) m, NULL);
		}
	}
}

static void ttd_set_scroll_fraction(GF_Node *node, GF_Route *route)
{
	Fixed frac;
	TTDPriv *priv = (TTDPriv *)gf_node_get_private(node);
	frac = priv->process_scroll->set_fraction;
	if (frac==FIX_ONE) priv->is_active = 0;
	if (!priv->tr_scroll) return;

	switch (priv->scroll_type - 1) {
	case GF_TXT_SCROLL_CREDITS:
	case GF_TXT_SCROLL_DOWN:
		priv->tr_scroll->translation.x = 0;
		if (priv->scroll_mode & GF_TXT_SCROLL_IN) {
			if (frac>priv->scroll_time) {
				priv->scroll_mode &= ~GF_TXT_SCROLL_IN;
				priv->tr_scroll->translation.y = 0;
			} else {
				priv->tr_scroll->translation.y = gf_muldiv(priv->dlist->size.y, frac, priv->scroll_time) - priv->dlist->size.y;
			}
		} else if (priv->scroll_mode & GF_TXT_SCROLL_OUT) {
			if (frac < FIX_ONE - priv->scroll_time) return;
			frac -= FIX_ONE - priv->scroll_time;
			if (priv->scroll_type - 1 == GF_TXT_SCROLL_DOWN) {
				priv->tr_scroll->translation.y = gf_muldiv(priv->dlist->size.y, frac, priv->scroll_time);
			} else {
				priv->tr_scroll->translation.y = gf_muldiv(priv->dlist->size.y, frac, priv->scroll_time);
			}
		}
		if (priv->scroll_type - 1 == GF_TXT_SCROLL_DOWN) priv->tr_scroll->translation.y *= -1;
		break;
	case GF_TXT_SCROLL_MARQUEE:
	case GF_TXT_SCROLL_RIGHT:
		priv->tr_scroll->translation.y = 0;
		if (priv->scroll_mode & GF_TXT_SCROLL_IN) {
			if (! (priv->scroll_mode & GF_TXT_SCROLL_OUT)) {
				if (frac<priv->scroll_delay) return;
				frac-=priv->scroll_delay;
			}
			if (frac>priv->scroll_time) {
				priv->scroll_mode &= ~GF_TXT_SCROLL_IN;
				priv->tr_scroll->translation.x = 0;
			} else {
				priv->tr_scroll->translation.x = gf_muldiv(priv->dlist->size.x, frac, priv->scroll_time) - priv->dlist->size.x;
			}
		} else if (priv->scroll_mode & GF_TXT_SCROLL_OUT) {
			if (frac < FIX_ONE - priv->scroll_time) return;
			frac -= FIX_ONE - priv->scroll_time;
			priv->tr_scroll->translation.x = gf_muldiv(priv->dlist->size.x, frac, priv->scroll_time);
		}
		if (priv->scroll_type - 1 == GF_TXT_SCROLL_MARQUEE) priv->tr_scroll->translation.x *= -1;
		break;
	default:
		break;
	}
	gf_node_changed((GF_Node *)priv->tr_scroll, NULL);
}

static void TTD_ResetDisplay(TTDPriv *priv)
{
	gf_list_reset(priv->blink_nodes);
	gf_node_unregister_children((GF_Node*)priv->dlist, priv->dlist->children);
	priv->dlist->children = NULL;
	gf_node_changed((GF_Node *) priv->dlist, NULL);
	priv->tr_scroll = NULL;
}

char *TTD_FindFont(GF_TextSampleDescriptor *tsd, u32 ID)
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

static void TTD_NewTextChunk(TTDPriv *priv, GF_TextSampleDescriptor *tsd, M_Form *form, u16 *utf16_txt, TTDTextChunk *tc)
{
	GF_Node *txt_model, *n2, *txt_material;
	M_Text *text;
	M_FontStyle *fs;
	char *fontName;
	char szStyle[1024];
	u32 fontSize, styleFlags, color, i, start_char;

	if (!tc->srec) {
		fontName = TTD_FindFont(tsd, tsd->default_style.fontID);
		fontSize = tsd->default_style.font_size;
		styleFlags = tsd->default_style.style_flags;
		color = tsd->default_style.text_color;
	} else {
		fontName = TTD_FindFont(tsd, tc->srec->fontID);
		fontSize = tc->srec->font_size;
		styleFlags = tc->srec->style_flags;
		color = tc->srec->text_color;
	}

	/*create base model for text node. It will then be cloned for each text item*/
	txt_model = ttd_create_node(priv, TAG_MPEG4_Shape, NULL);
	gf_node_register(txt_model, NULL);
	n2 = ttd_create_node(priv, TAG_MPEG4_Appearance, NULL);
	((M_Shape *)txt_model)->appearance = n2;
	gf_node_register(n2, txt_model);
	txt_material = ttd_create_node(priv, TAG_MPEG4_Material2D, NULL);
	((M_Appearance *)n2)->material = txt_material;
	gf_node_register(txt_material, n2);

	((M_Material2D *)txt_material)->filled = 1;
	((M_Material2D *)txt_material)->transparency = FIX_ONE - INT2FIX((color>>24) & 0xFF) / 255;
	((M_Material2D *)txt_material)->emissiveColor.red = INT2FIX((color>>16) & 0xFF) / 255;
	((M_Material2D *)txt_material)->emissiveColor.green = INT2FIX((color>>8) & 0xFF) / 255;
	((M_Material2D *)txt_material)->emissiveColor.blue = INT2FIX((color) & 0xFF) / 255;
	/*force 0 lineWidth if blinking (the stupid MPEG-4 default values once again..)*/
	if (tc->has_blink) {
		((M_Material2D *)txt_material)->lineProps = ttd_create_node(priv, TAG_MPEG4_LineProperties, NULL);
		((M_LineProperties *)((M_Material2D *)txt_material)->lineProps)->width = 0;
		gf_node_register(((M_Material2D *)txt_material)->lineProps, txt_material);
	}

	n2 = ttd_create_node(priv, TAG_MPEG4_Text, NULL);
	((M_Shape *)txt_model)->geometry = n2;
	gf_node_register(n2, txt_model);
	text = (M_Text *) n2;
	fs = (M_FontStyle *) ttd_create_node(priv, TAG_MPEG4_FontStyle, NULL);
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
	if (priv->use_texture) strcat(szStyle, " TEXTURED");
	if (priv->outline) strcat(szStyle, " OUTLINED");

	fs->style.buffer = gf_strdup(szStyle);
	fs->horizontal = (tsd->displayFlags & GF_TXT_VERTICAL) ? 0 : 1;
	text->fontStyle = (GF_Node *) fs;
	gf_node_register((GF_Node *)fs, (GF_Node *)text);
	gf_sg_vrml_mf_reset(&text->string, GF_SG_VRML_MFSTRING);

	if (tc->hlink && tc->hlink->URL) {
		SFURL *s;
		M_Anchor *anc = (M_Anchor *) ttd_create_node(priv, TAG_MPEG4_Anchor, NULL);
		gf_sg_vrml_mf_append(&anc->url, GF_SG_VRML_MFURL, (void **) &s);
		s->OD_ID = 0;
		s->url = gf_strdup(tc->hlink->URL);
		if (tc->hlink->URL_hint) anc->description.buffer = gf_strdup(tc->hlink->URL_hint);
		gf_node_list_add_child(& anc->children, txt_model);
		gf_node_register(txt_model, (GF_Node *)anc);
		txt_model = (GF_Node *)anc;
		gf_node_register((GF_Node *)anc, NULL);
	}

	start_char = tc->start_char;
	for (i=tc->start_char; i<tc->end_char; i++) {
		Bool new_line = 0;
		if ((utf16_txt[i] == '\n') || (utf16_txt[i] == '\r') || (utf16_txt[i] == 0x85) || (utf16_txt[i] == 0x2028) || (utf16_txt[i] == 0x2029)) new_line = 1;

		if (new_line || (i+1==tc->end_char) ) {
			SFString *st;

			if (i+1==tc->end_char) i++;

			if (i!=start_char) {
				char szLine[5000];
				u32 len;
				s16 wsChunk[5000], *sp;

				/*spliting lines, duplicate node*/

				n2 = gf_node_clone(priv->sg, txt_model, NULL, "", 1);
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

				if (tc->has_blink && txt_material) gf_list_add(priv->blink_nodes, txt_material);


				memcpy(wsChunk, &utf16_txt[start_char], sizeof(s16)*(i-start_char));
				wsChunk[i-start_char] = 0;
				sp = &wsChunk[0];
				len = (u32) gf_utf8_wcstombs(szLine, 5000, (const unsigned short **) &sp);
				szLine[len] = 0;

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
void TTD_SplitChunks(GF_TextSample *txt, u32 nb_chars, GF_List *chunks, GF_Box *mod)
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
			tc->is_hilight = 1;
			if (txt->highlight_color) tc->hilight_col = txt->highlight_color->hil_color;
			break;
		case GF_ISOM_BOX_TYPE_HREF:
			tc->hlink = (GF_TextHyperTextBox *) mod;
			break;
		case GF_ISOM_BOX_TYPE_BLNK:
			tc->has_blink = 1;
			break;
		}
		/*done*/
		if (tc->end_char==end_char) return;
	}
}


static void TTD_ApplySample(TTDPriv *priv, GF_TextSample *txt, u32 sdi, Bool is_utf_16, u32 sample_duration)
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
	if (gf_list_count(priv->blink_nodes)) {
		priv->ts_blink->stopTime = gf_node_get_scene_time((GF_Node *) priv->ts_blink);
		gf_node_changed((GF_Node *) priv->ts_blink, NULL);
	}
	priv->ts_scroll->stopTime = gf_node_get_scene_time((GF_Node *) priv->ts_scroll);
	gf_node_changed((GF_Node *) priv->ts_scroll, NULL);
	/*flush routes to avoid getting the set_fraction of the scroll sensor deactivation*/
	gf_sg_activate_routes(priv->inlineScene->graph);

	TTD_ResetDisplay(priv);
	if (!sdi || !txt || !txt->len) return;

	i=0;
	while ((td = (GF_TextSampleDescriptor *)gf_list_enum(priv->cfg->sample_descriptions, &i))) {
		if (td->sample_index==sdi) break;
		td = NULL;
	}
	if (!td) return;


	vertical = (td->displayFlags & GF_TXT_VERTICAL) ? 1 : 0;

	/*set back color*/
	/*do we fill the text box or the entire text track region*/
	if (td->displayFlags & GF_TXT_FILL_REGION) {
		priv->mat_box->transparency = FIX_ONE;
		n = priv->mat_track;
	} else {
		priv->mat_track->transparency = FIX_ONE;
		n = priv->mat_box;
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
		br.right = priv->cfg->text_width;
		br.bottom = priv->cfg->text_height;
	}
	thw = br.right - br.left;
	thh = br.bottom - br.top;
	if (!thw || !thh) {
		br.top = br.left = 0;
		thw = priv->cfg->text_width;
		thh = priv->cfg->text_height;
	}

	priv->dlist->size.x = INT2FIX(thw);
	priv->dlist->size.y = INT2FIX(thh);

	/*disable backgrounds if not used*/
	if (priv->mat_track->transparency<FIX_ONE) {
		if (priv->rec_track->size.x != priv->cfg->text_width) {
			priv->rec_track->size.x = priv->cfg->text_width;
			priv->rec_track->size.y = priv->cfg->text_height;
			gf_node_changed((GF_Node *) priv->rec_track, NULL);
		}
	} else if (priv->rec_track->size.x) {
		priv->rec_track->size.x = priv->rec_track->size.y = 0;
		gf_node_changed((GF_Node *) priv->rec_box, NULL);
	}

	if (priv->mat_box->transparency<FIX_ONE) {
		if (priv->rec_box->size.x != priv->dlist->size.x) {
			priv->rec_box->size.x = priv->dlist->size.x;
			priv->rec_box->size.y = priv->dlist->size.y;
			gf_node_changed((GF_Node *) priv->rec_box, NULL);
		}
	} else if (priv->rec_box->size.x) {
		priv->rec_box->size.x = priv->rec_box->size.y = 0;
		gf_node_changed((GF_Node *) priv->rec_box, NULL);
	}

	form = (M_Form *) ttd_create_node(priv, TAG_MPEG4_Form, NULL);
	form->size.x = INT2FIX(thw);
	form->size.y = INT2FIX(thh);

	thw /= 2;
	thh /= 2;
	tw = priv->cfg->text_width;
	th = priv->cfg->text_height;

	/*check translation, we must not get out of scene size - not supported in GPAC*/
	offset = br.left - tw/2 + thw;
	if (offset + thw < - tw/2) offset = - tw/2 + thw;
	else if (offset - thw > tw/2) offset = tw/2 - thw;
	priv->tr_box->translation.x = INT2FIX(offset);

	offset = th/2 - br.top - thh;
	if (offset + thh > th/2) offset = th/2 - thh;
	else if (offset - thh < -th/2) offset = -th/2 + thh;
	priv->tr_box->translation.y = INT2FIX(offset);

	gf_node_dirty_set((GF_Node *)priv->tr_box, 0, 1);


	if (priv->scroll_type) {
		priv->ts_scroll->stopTime = gf_node_get_scene_time((GF_Node *) priv->ts_scroll);
		gf_node_changed((GF_Node *) priv->ts_scroll, NULL);
	}
	priv->scroll_mode = 0;
	if (td->displayFlags & GF_TXT_SCROLL_IN) priv->scroll_mode |= GF_TXT_SCROLL_IN;
	if (td->displayFlags & GF_TXT_SCROLL_OUT) priv->scroll_mode |= GF_TXT_SCROLL_OUT;

	priv->scroll_type = 0;
	if (priv->scroll_mode) {
		priv->scroll_type = (td->displayFlags & GF_TXT_SCROLL_DIRECTION)>>7;
		priv->scroll_type ++;
	}
	/*no sample duration, cannot determine scroll rate, so just show*/
	if (!sample_duration) priv->scroll_type = 0;
	/*no scroll*/
	if (!priv->scroll_mode) priv->scroll_type = 0;

	if (priv->scroll_type) {
		priv->tr_scroll = (M_Transform2D *) ttd_create_node(priv, TAG_MPEG4_Transform2D, NULL);
		gf_node_list_add_child( &priv->dlist->children, (GF_Node*)priv->tr_scroll);
		gf_node_register((GF_Node *) priv->tr_scroll, (GF_Node *) priv->dlist);
		gf_node_list_add_child( &priv->tr_scroll->children, (GF_Node*)form);
		gf_node_register((GF_Node *) form, (GF_Node *) priv->tr_scroll);
		priv->tr_scroll->translation.x = priv->tr_scroll->translation.y = (priv->scroll_mode & GF_TXT_SCROLL_IN) ? -INT2FIX(1000) : 0;
		/*if no delay, text is in motion for the duration of the sample*/
		priv->scroll_time = FIX_ONE;
		priv->scroll_delay = 0;

		if (txt->scroll_delay) {
			priv->scroll_delay = gf_divfix(INT2FIX(txt->scroll_delay->scroll_delay), INT2FIX(sample_duration));
			if (priv->scroll_delay>FIX_ONE) priv->scroll_delay = FIX_ONE;
			priv->scroll_time = (FIX_ONE - priv->scroll_delay);
		}
		/*if both scroll (in and out), use same scroll duration for both*/
		if ((priv->scroll_mode & GF_TXT_SCROLL_IN) && (priv->scroll_mode & GF_TXT_SCROLL_OUT)) priv->scroll_time /= 2;

	} else {
		gf_node_list_add_child( &priv->dlist->children, (GF_Node*)form);
		gf_node_register((GF_Node *) form, (GF_Node *) priv->dlist);
		priv->tr_scroll = NULL;
	}

	if (is_utf_16) {
		memcpy((char *) utf16_text, txt->text, sizeof(char) * txt->len);
		((char *) utf16_text)[txt->len] = 0;
		((char *) utf16_text)[txt->len+1] = 0;
		char_count = txt->len / 2;
	} else {
		char *p = txt->text;
		char_count = (u32) gf_utf8_mbstowcs(utf16_text, 2500, (const char **) &p);
	}

	chunks = gf_list_new();
	/*flatten all modifiers*/
	if (!txt->styles || !txt->styles->entry_count) {
		GF_SAFEALLOC(tc, TTDTextChunk);
		tc->end_char = char_count;
		gf_list_add(chunks, tc);
	} else {
		GF_StyleRecord *srec = NULL;
		char_offset = 0;
		for (i=0; i<txt->styles->entry_count; i++) {
			TTDTextChunk *tc;
			srec = &txt->styles->styles[i];
			if (srec->startCharOffset==srec->endCharOffset) continue;
			/*handle not continuous modifiers*/
			if (char_offset < srec->startCharOffset) {
				GF_SAFEALLOC(tc, TTDTextChunk);
				tc->start_char = char_offset;
				tc->end_char = srec->startCharOffset;
				gf_list_add(chunks, tc);
			}
			GF_SAFEALLOC(tc, TTDTextChunk);
			tc->start_char = srec->startCharOffset;
			tc->end_char = srec->endCharOffset;
			tc->srec = srec;
			gf_list_add(chunks, tc);
			char_offset = srec->endCharOffset;
		}

		if (srec->endCharOffset<char_count) {
			GF_SAFEALLOC(tc, TTDTextChunk);
			tc->start_char = char_offset;
			tc->end_char = char_count;
			gf_list_add(chunks, tc);
		}
	}
	/*apply all other modifiers*/
	i=0;
	while ((a = (GF_Box*)gf_list_enum(txt->others, &i))) {
		TTD_SplitChunks(txt, char_count, chunks, a);
	}

	while (gf_list_count(chunks)) {
		tc = (TTDTextChunk*)gf_list_get(chunks, 0);
		gf_list_rem(chunks, 0);
		TTD_NewTextChunk(priv, td, form, utf16_text, tc);
		gf_free(tc);
	}
	gf_list_del(chunks);

	if (form->groupsIndex.vals[form->groupsIndex.count-1] != -1)
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
			s32 *id;
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


	gf_node_dirty_set((GF_Node *)form, 0, 1);
	gf_node_changed((GF_Node *)form, NULL);
	gf_node_changed((GF_Node *) priv->dlist, NULL);

	if (gf_list_count(priv->blink_nodes)) {
		/*restart time sensor*/
		priv->ts_blink->startTime = gf_node_get_scene_time((GF_Node *) priv->ts_blink);
		gf_node_changed((GF_Node *) priv->ts_blink, NULL);
	}

	priv->is_active = 1;
	/*scroll timer also acts as AU timer*/
	priv->ts_scroll->startTime = gf_node_get_scene_time((GF_Node *) priv->ts_scroll);
	priv->ts_scroll->stopTime = priv->ts_scroll->startTime - 1.0;
	priv->ts_scroll->cycleInterval = sample_duration;
	priv->ts_scroll->cycleInterval /= priv->cfg->timescale;
	priv->ts_scroll->cycleInterval -= 0.1;
	gf_node_changed((GF_Node *) priv->ts_scroll, NULL);
}

static GF_Err TTD_ProcessData(GF_SceneDecoder*plug, const char *inBuffer, u32 inBufferLength,
                              u16 ES_ID, u32 AU_time, u32 mmlevel)
{
	GF_BitStream *bs;
	GF_Err e = GF_OK;
	TTDPriv *priv = (TTDPriv *)plug->privateStack;

	bs = gf_bs_new(inBuffer, inBufferLength, GF_BITSTREAM_READ);
	while (gf_bs_available(bs)) {
		GF_TextSample *txt;
		Bool is_utf_16;
		u32 type, length, sample_index, sample_duration;
		is_utf_16 = gf_bs_read_int(bs, 1);
		gf_bs_read_int(bs, 4);
		type = gf_bs_read_int(bs, 3);
		length = gf_bs_read_u16(bs);

		/*currently only full text samples are supported*/
		if (type != 1) {
			gf_bs_del(bs);
			return GF_NOT_SUPPORTED;
		}
		sample_index = gf_bs_read_u8(bs);
		/*duration*/
		sample_duration = gf_bs_read_u24(bs);
		length -= 8;
		/*txt length is parsed with the sample*/
		txt = gf_isom_parse_texte_sample(bs);
		TTD_ApplySample(priv, txt, sample_index, is_utf_16, sample_duration);
		gf_isom_delete_text_sample(txt);
		/*since we support only TTU(1), no need to go on*/
		break;
	}
	gf_bs_del(bs);
	return e;
}

static u32 TTD_CanHandleStream(GF_BaseDecoder *ifce, u32 StreamType, GF_ESD *esd, u8 PL)
{
	/*TTDPriv *priv = (TTDPriv *)ifce->privateStack;*/
	if (StreamType!=GF_STREAM_TEXT) return GF_CODEC_NOT_SUPPORTED;
	/*media type query*/
	if (!esd) return GF_CODEC_STREAM_TYPE_SUPPORTED;

	if (esd->decoderConfig->objectTypeIndication==0x08) return GF_CODEC_SUPPORTED;

	return GF_CODEC_NOT_SUPPORTED;
}


void DeleteTimedTextDec(GF_BaseDecoder *plug)
{
	TTDPriv *priv = (TTDPriv *)plug->privateStack;
	/*in case something went wrong*/
	if (priv->cfg) gf_odf_desc_del((GF_Descriptor *) priv->cfg);
	gf_free(priv);
	gf_free(plug);
}

GF_BaseDecoder *NewTimedTextDec()
{
	TTDPriv *priv;
	GF_SceneDecoder *tmp;

	GF_SAFEALLOC(tmp, GF_SceneDecoder);
	if (!tmp) return NULL;
	GF_SAFEALLOC(priv, TTDPriv);

	tmp->privateStack = priv;
	tmp->AttachStream = TTD_AttachStream;
	tmp->DetachStream = TTD_DetachStream;
	tmp->GetCapabilities = TTD_GetCapabilities;
	tmp->SetCapabilities = TTD_SetCapabilities;
	tmp->ProcessData = TTD_ProcessData;
	tmp->AttachScene = TTD_AttachScene;
	tmp->CanHandleStream = TTD_CanHandleStream;
	tmp->ReleaseScene = TTD_ReleaseScene;
	GF_REGISTER_MODULE_INTERFACE(tmp, GF_SCENE_DECODER_INTERFACE, "GPAC TimedText Decoder", "gpac distribution")
	return (GF_BaseDecoder *) tmp;
}

#if !defined(GPAC_DISABLE_ISOM_WRITE) && !defined(GPAC_DISABLE_MEDIA_IMPORT)
void DeleteTTReader(void *ifce);
void *NewTTReader();
#endif

GPAC_MODULE_EXPORT
GF_BaseInterface *LoadInterface(u32 InterfaceType)
{
	switch (InterfaceType) {
	case GF_SCENE_DECODER_INTERFACE:
		return (GF_BaseInterface *)NewTimedTextDec();
#if !defined(GPAC_DISABLE_ISOM_WRITE) && !defined(GPAC_DISABLE_MEDIA_IMPORT)
	case GF_NET_CLIENT_INTERFACE:
		return (GF_BaseInterface *)NewTTReader();
#endif
	default:
		return NULL;
	}
}

GPAC_MODULE_EXPORT
void ShutdownInterface(GF_BaseInterface *ifce)
{
	switch (ifce->InterfaceType) {
	case GF_SCENE_DECODER_INTERFACE:
		DeleteTimedTextDec((GF_BaseDecoder *)ifce);
		break;
#if !defined(GPAC_DISABLE_ISOM_WRITE) && !defined(GPAC_DISABLE_MEDIA_IMPORT)
	case GF_NET_CLIENT_INTERFACE:
		DeleteTTReader(ifce);
		break;
#endif
	}
}

#else

GPAC_MODULE_EXPORT
GF_BaseInterface *LoadInterface(u32 InterfaceType) {
	return NULL;
}
GPAC_MODULE_EXPORT
void ShutdownInterface(GF_BaseInterface *ifce) {}


#endif /*!defined(GPAC_DISABLE_VRML) && !defined(GPAC_DISABLE_ISOM)*/

GPAC_MODULE_EXPORT
const u32 *QueryInterfaces()
{
	static u32 si [] = {
#if !defined(GPAC_DISABLE_VRML) && !defined(GPAC_DISABLE_ISOM)
		GF_SCENE_DECODER_INTERFACE,
		GF_NET_CLIENT_INTERFACE,
#endif
		0
	};
	return si;
}

GPAC_MODULE_STATIC_DELARATION( timedtext )

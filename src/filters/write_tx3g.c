/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2022-2023
 *					All rights reserved
 *
 *  This file is part of GPAC / TX3G to SRT/VTT/TTML convert filter
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
#include <gpac/bitstream.h>
#include <gpac/internal/isomedia_dev.h>
#include <gpac/internal/media_dev.h>
#include <gpac/color.h>

#if !defined(GPAC_DISABLE_ISOM_DUMP) && !defined(GPAC_DISABLE_ISOM)

#define TTML_NAMESPACE "http://www.w3.org/ns/ttml"

typedef struct
{
	//opts
	Bool exporter, merge;

	//only one input pid declared
	GF_FilterPid *ipid;
	//only one output pid declared
	GF_FilterPid *opid;

	GF_BitStream *bs_r;
	u32 codecid;

	GF_Fraction64 duration;
	s64 delay;

	GF_TextConfig *cfg;
	u32 dsi_crc;
	Bool is_tx3g;

	u32 dump_type;
	u32 cur_frame;
} TX3GMxCtx;

static void tx3gmx_write_config(TX3GMxCtx *ctx);

GF_Err tx3gmx_configure_pid(GF_Filter *filter, GF_FilterPid *pid, Bool is_remove)
{
	GF_Err e;
	const GF_PropertyValue *p;
	TX3GMxCtx *ctx = gf_filter_get_udta(filter);

	if (is_remove) {
		ctx->ipid = NULL;
		if (ctx->opid) {
			gf_filter_pid_remove(ctx->opid);
			ctx->opid = NULL;
		}
		return GF_OK;
	}
	if (! gf_filter_pid_check_caps(pid))
		return GF_NOT_SUPPORTED;

	p = gf_filter_pid_get_property(pid, GF_PROP_PID_CODECID);
	if (!p) return GF_NOT_SUPPORTED;
	ctx->codecid = p->value.uint;

	//decoder config may be not ready yet
	p = gf_filter_pid_get_property(pid, GF_PROP_PID_DECODER_CONFIG);
	if (p) {
		u32 crc = gf_crc_32(p->value.data.ptr, p->value.data.size);
		if (crc == ctx->dsi_crc) return GF_OK;
		ctx->dsi_crc = crc;
	}


	if (!ctx->opid) {
		ctx->opid = gf_filter_pid_new(filter);
	}
	gf_filter_pid_copy_properties(ctx->opid, pid);

	u32 codec_id = 0;
	if (ctx->dump_type==3) codec_id = GF_CODECID_SUBS_XML;
	else if (ctx->dump_type==2) codec_id = GF_CODECID_WEBVTT;
	else if (ctx->dump_type==1) codec_id = GF_CODECID_SIMPLE_TEXT;
	else codec_id = GF_CODECID_TX3G;

	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_STREAM_TYPE, &PROP_UINT(GF_STREAM_TEXT) );
	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_CODECID, &PROP_UINT(codec_id) );
	if (ctx->dump_type<3)
		gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_UNFRAMED, &PROP_BOOL(GF_TRUE) );
	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_DECODER_CONFIG, NULL );

	if (ctx->dump_type==3) {
		gf_filter_pid_set_property_str(ctx->opid, "meta:xmlns", &PROP_STRING(TTML_NAMESPACE) );
	}

	ctx->ipid = pid;

	//decoder config may be not ready yet
	p = gf_filter_pid_get_property(pid, GF_PROP_PID_DECODER_CONFIG);
	if (!p) return GF_OK;

	if (ctx->cfg) gf_odf_desc_del((GF_Descriptor *) ctx->cfg);
	ctx->cfg = (GF_TextConfig *) gf_odf_desc_new(GF_ODF_TEXT_CFG_TAG);
	if (ctx->codecid == GF_CODECID_TEXT_MPEG4) {
		e = gf_odf_get_text_config(p->value.data.ptr, p->value.data.size, GF_CODECID_TEXT_MPEG4, ctx->cfg);
		if (e) {
			gf_odf_desc_del((GF_Descriptor *) ctx->cfg);
			ctx->cfg = NULL;
			return e;
		}
		ctx->is_tx3g = GF_FALSE;
	} else if (ctx->codecid == GF_CODECID_TX3G) {
		GF_TextSampleDescriptor * sd = gf_odf_tx3g_read(p->value.data.ptr, p->value.data.size);
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

	if (ctx->exporter) {
		GF_LOG(GF_LOG_INFO, GF_LOG_MEDIA, ("Exporting %s\n", gf_codecid_name(ctx->codecid)));
	}

	p = gf_filter_pid_get_property(pid, GF_PROP_PID_DURATION);
	if (p && (p->value.lfrac.num>0)) ctx->duration = p->value.lfrac;

	gf_filter_pid_set_framing_mode(pid, GF_TRUE);

	if (!ctx->dump_type)
		tx3gmx_write_config(ctx);


	p = gf_filter_pid_get_property(pid, GF_PROP_PID_DELAY);
	ctx->delay = p ? p->value.longsint : 0;
	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_DELAY, NULL);

	return GF_OK;
}

void dump_ttxt_header(FILE *dump, GF_Tx3gSampleEntryBox *txt_e, u32 def_width, u32 def_height);

static void tx3gmx_get_stsd(GF_TextSampleDescriptor *ent, GF_Tx3gSampleEntryBox *samp_ent, GF_FontTableBox *font_ent)
{
	memset(samp_ent, 0, sizeof(GF_Tx3gSampleEntryBox));
	samp_ent->type = GF_ISOM_BOX_TYPE_TX3G;
	samp_ent->displayFlags = ent->displayFlags;
	samp_ent->horizontal_justification = ent->horiz_justif;
	samp_ent->vertical_justification = ent->vert_justif;
	samp_ent->back_color = ent->back_color;
	samp_ent->default_box = ent->default_pos;
	samp_ent->default_style = ent->default_style;
	samp_ent->font_table = font_ent;
	font_ent->entry_count = ent->font_count;
	font_ent->fonts = ent->fonts;
}

static void tx3gmx_write_config(TX3GMxCtx *ctx)
{
	FILE *dump = gf_file_temp(NULL);
	GF_Tx3gSampleEntryBox samp_ent;
	GF_FontTableBox font_ent;
	const GF_PropertyValue *p;
	GF_TextSampleDescriptor *ent = gf_list_get(ctx->cfg->sample_descriptions, 0);

	tx3gmx_get_stsd(ent, &samp_ent, &font_ent);

	p = gf_filter_pid_get_property(ctx->ipid, GF_PROP_PID_WIDTH);
	u32 w = p ? p->value.uint : 0;
	p = gf_filter_pid_get_property(ctx->ipid, GF_PROP_PID_HEIGHT);
	u32 h = p ? p->value.uint : 0;


	gf_fprintf(dump, "<?xml version=\"1.0\" encoding=\"UTF-8\" ?>\n");
	gf_fprintf(dump, "<!-- GPAC 3GPP Text Stream -->\n");
	gf_fprintf(dump, "<TextStream version=\"1.1\">\n");
	if (w && h) {
		gf_fprintf(dump, "<TextStreamHeader width=\"%u\" height=\"%u\">\n", w, h);
	} else {
		gf_fprintf(dump, "<TextStreamHeader>\n");
	}
#ifndef GPAC_DISABLE_ISOM_DUMP
	dump_ttxt_header(dump, &samp_ent, w, h);
#endif
	gf_fprintf(dump, "</TextStreamHeader>\n");

	u32 size = (u32) gf_ftell(dump);
	u8 *dsi = gf_malloc(size);
	if (dsi) {
		gf_fseek(dump, 0, SEEK_SET);
		gf_fread(dsi, size, dump);
		gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_DECODER_CONFIG, &PROP_DATA_NO_COPY(dsi, size));
	}

	gf_fclose(dump);
}

//from libisomedia box_dump.c
void dump_ttxt_sample(FILE *dump, GF_TextSample *s_txt, u64 ts, u32 timescale, u32 di, Bool box_dump);
char *tx3g_format_time(u64 ts, u32 timescale, char *szDur, Bool is_srt);
GF_Err dump_ttxt_sample_srt(FILE *dump, GF_TextSample *txt, GF_Tx3gSampleEntryBox *txtd, Bool vtt_dump);

static GF_Err dump_ttxt_sample_ttml(TX3GMxCtx *ctx, FILE *dump, GF_TextSample *txt, GF_Tx3gSampleEntryBox *txtd, u64 start_ts, u64 end_ts)
{
	char szTime[100];
	u32 len, j, k;

	gf_fprintf(dump,
"<?xml version=\"1.0\" encoding=\"UTF-8\" ?>\n"
"<tt xmlns=\"http://www.w3.org/ns/ttml\" xmlns:ttm=\"http://www.w3.org/ns/ttml#metadata\" "
"xmlns:tts=\"http://www.w3.org/ns/ttml#styling\" xmlns:ttp=\"http://www.w3.org/ns/ttml#parameter\"");

	const GF_PropertyValue *p = gf_filter_pid_get_property(ctx->ipid, GF_PROP_PID_LANGUAGE);
	if (p && p->value.string) {
		gf_fprintf(dump, "  xml:lang=\"%s\"", p->value.string);
	}
	gf_fprintf(dump, ">\n");

	gf_fprintf(dump," <head>\n  <layout>\n   <region xml:id=\"Default\" tts:origin=\"5%% 5%%\" tts:extent=\"90%% 90%%\" tts:displayAlign=\"after\" tts:overflow=\"visible\"");

	switch (txtd->horizontal_justification) {
	case 1:
		gf_fprintf(dump, " tts:textAlign=\"center\"");
		break;/*center*/
	case -1:
		gf_fprintf(dump, " tts:textAlign=\"right\"");
		break;/*right*/
	default:
		gf_fprintf(dump, " tts:textAlign=\"left\"");
		break;/*left*/
	}
	if (txtd->default_style.font_size)
		gf_fprintf(dump, " tts:fontSize=\"%dpx\"", txtd->default_style.font_size);
	if (txtd->font_table && txtd->font_table->entry_count && txtd->font_table->fonts[0].fontName)
		gf_fprintf(dump, " tts:fontFamily=\"%s\"", txtd->font_table->fonts[0].fontName);

	gf_fprintf(dump, "/>\n  </layout>\n </head>\n <body>\n  <div>\n");

	tx3g_format_time(start_ts, 1000, szTime, GF_FALSE);
	gf_fprintf(dump, "  <p begin=\"%s\"", szTime);
	tx3g_format_time(end_ts, 1000, szTime, GF_FALSE);
	gf_fprintf(dump, " end=\"%s\"", szTime);

	if (!txt || !txt->len) {
		gf_fprintf(dump, "></p>\n");
		goto exit;
	}

	u32 styles, char_num, new_styles, color, new_color;
	u16 *utf16Line = gf_malloc(sizeof(u16) * 4*txt->len);
	if (!utf16Line) return GF_OUT_OF_MEM;
	Bool has_span = GF_FALSE;

	/*UTF16*/
	if ((txt->len>2) && ((unsigned char) txt->text[0] == (unsigned char) 0xFE) && ((unsigned char) txt->text[1] == (unsigned char) 0xFF)) {
		memcpy(utf16Line, txt->text+2, sizeof(char)*txt->len);
		( ((char *)utf16Line)[txt->len] ) = 0;
		len = txt->len;
	} else {
		u8 *str = (u8 *) (txt->text);
		len = gf_utf8_mbstowcs(utf16Line, 10000, (const char **) &str);
		if (len == GF_UTF8_FAIL) return GF_NON_COMPLIANT_BITSTREAM;
		utf16Line[len] = 0;
	}
	char_num = 0;
	styles = 0;
	new_styles = txtd->default_style.style_flags;
	color = new_color = txtd->default_style.text_color;
	Bool single_style = GF_FALSE;
	if (txt->styles && (txt->styles->entry_count==1)) {
		if ((txt->styles->styles[0].startCharOffset==0) && (txt->styles->styles[0].endCharOffset==len)) {
			single_style = GF_TRUE;
			new_styles = txt->styles->styles[0].style_flags;
			new_color = txt->styles->styles[0].text_color;
		}
	}
	if (single_style) {
		Bool needs_bold = (new_styles & GF_TXT_STYLE_BOLD);
		Bool needs_italic = (new_styles & GF_TXT_STYLE_ITALIC);
		Bool needs_underlined = (new_styles & GF_TXT_STYLE_UNDERLINED);
		Bool needs_strikethrough = (new_styles & GF_TXT_STYLE_STRIKETHROUGH);
		Bool needs_color = (new_color != txtd->default_style.text_color);
		if (needs_bold || needs_italic || needs_underlined || needs_strikethrough || needs_color) {
			if (needs_italic) gf_fprintf(dump, " tts:fontStyle=\"italic\"");
			if (needs_bold) gf_fprintf(dump, " tts:fontWeight=\"bold\"");
			if (needs_underlined) gf_fprintf(dump, " tts:textDecoration=\"underline\"");
			if (needs_strikethrough) gf_fprintf(dump, " tts:textDecoration=\"lineThrough\"");

			if (needs_color) gf_fprintf(dump, " tts:color=\"%s\"", gf_color_get_name(color));
		}
	}
	//end of <p>
	gf_fprintf(dump, ">");

	for (j=0; j<len; j++) {
		if (!single_style) {
			if (txt->styles) {
				new_styles = txtd->default_style.style_flags;
				new_color = txtd->default_style.text_color;
				for (k=0; k<txt->styles->entry_count; k++) {
					if (txt->styles->styles[k].startCharOffset>char_num) continue;
					if (txt->styles->styles[k].endCharOffset<char_num+1) continue;

					if (txt->styles->styles[k].style_flags & (GF_TXT_STYLE_ITALIC | GF_TXT_STYLE_BOLD | GF_TXT_STYLE_UNDERLINED | GF_TXT_STYLE_STRIKETHROUGH)) {
						new_styles = txt->styles->styles[k].style_flags;
						new_color = txt->styles->styles[k].text_color;
						break;
					}
				}
			}

			Bool needs_color=GF_FALSE;
			Bool close_span=GF_FALSE;
			Bool needs_span=GF_FALSE;

			if (new_color != color) {
				close_span = GF_TRUE;
				if (new_color != txtd->default_style.text_color) needs_color = GF_TRUE;
				color = new_color;
				//reset style to default
				styles = txtd->default_style.style_flags;
			}

			//check each style:
			//- if set but not previously set, needs a new span
			//- if not set but previously set, needs to close span
			if (new_styles != styles) {
				if ((new_styles & GF_TXT_STYLE_BOLD) && !(styles & GF_TXT_STYLE_BOLD)) needs_span = GF_TRUE;
				else if ((styles & GF_TXT_STYLE_BOLD) && !(new_styles & GF_TXT_STYLE_BOLD)) close_span = GF_TRUE;

				if ((new_styles & GF_TXT_STYLE_ITALIC) && !(styles & GF_TXT_STYLE_ITALIC)) needs_span = GF_TRUE;
				else if ((styles & GF_TXT_STYLE_ITALIC) && !(new_styles & GF_TXT_STYLE_ITALIC)) close_span = GF_TRUE;

				if ((new_styles & GF_TXT_STYLE_UNDERLINED) && !(styles & GF_TXT_STYLE_UNDERLINED)) needs_span = GF_TRUE;
				if ((styles & GF_TXT_STYLE_UNDERLINED) && !(new_styles & GF_TXT_STYLE_UNDERLINED)) close_span = GF_TRUE;

				if ((new_styles & GF_TXT_STYLE_STRIKETHROUGH) && !(styles & GF_TXT_STYLE_STRIKETHROUGH)) needs_span = GF_TRUE;
				else if ((styles & GF_TXT_STYLE_STRIKETHROUGH) && !(new_styles & GF_TXT_STYLE_STRIKETHROUGH)) close_span = GF_TRUE;

				styles = new_styles;
			}

			if (has_span && close_span) {
				gf_fprintf(dump, "</span>");
				has_span = GF_FALSE;
			}
			if (needs_span) {
				has_span = GF_TRUE;
				gf_fprintf(dump, "<span");
				if (styles & GF_TXT_STYLE_ITALIC) gf_fprintf(dump, " tts:fontStyle=\"italic\"");
				if (styles & GF_TXT_STYLE_BOLD) gf_fprintf(dump, " tts:fontWeight=\"bold\"");
				if (styles & GF_TXT_STYLE_UNDERLINED) gf_fprintf(dump, " tts:textDecoration=\"underline\"");
				if (styles & GF_TXT_STYLE_STRIKETHROUGH) gf_fprintf(dump, " tts:textDecoration=\"lineThrough\"");

				if (needs_color) gf_fprintf(dump, " tts:color=\"%s\"", gf_color_get_name(color));

				gf_fprintf(dump, ">");
			}
		}

		if (utf16Line[j]=='\r') {

		} else if (utf16Line[j]=='\n') {
			gf_fprintf(dump, "<br/>");
		} else {
			u32 sl;
			char szChar[30];
			s16 swT[2], *swz;
			swT[0] = utf16Line[j];
			swT[1] = 0;
			swz= (s16 *)swT;
			sl = gf_utf8_wcstombs(szChar, 30, (const unsigned short **) &swz);
			if (sl == GF_UTF8_FAIL) sl=0;
			szChar[sl]=0;
			gf_fprintf(dump, "%s", szChar);
		}
		char_num++;
	}

	if (has_span) {
		gf_fprintf(dump, "</span>");
	}
	gf_fprintf(dump, "</p>\n");
	gf_free(utf16Line);


exit:
	gf_fprintf(dump, "  </div>\n </body>\n</tt>");
	return GF_OK;
}

GF_Err tx3gmx_process(GF_Filter *filter)
{
	TX3GMxCtx *ctx = gf_filter_get_udta(filter);
	GF_FilterPacket *pck, *dst_pck;
	u8 *data, *output;
	u64 start_ts, end_ts, o_start_ts;
	u32 pck_size, timescale;
	FILE *dump=NULL;
	pck = gf_filter_pid_get_packet(ctx->ipid);
	if (!pck) {
		if (gf_filter_pid_is_eos(ctx->ipid)) {
			gf_filter_pid_set_eos(ctx->opid);
			return GF_EOS;
		}
		return GF_OK;
	}

	data = (char *) gf_filter_pck_get_data(pck, &pck_size);
	if (pck_size<=1) {
		gf_filter_pid_drop_packet(ctx->ipid);
		//we consider a 0 packet size not an error
		return pck_size ? GF_NON_COMPLIANT_BITSTREAM : GF_OK;;
	}
	if (ctx->dump_type && (pck_size<=2)) {
		gf_filter_pid_drop_packet(ctx->ipid);
		return GF_OK;
	}

	timescale = gf_filter_pck_get_timescale(pck);
	if (!timescale) timescale=1000;

	start_ts = gf_filter_pck_get_cts(pck);
	end_ts = start_ts + gf_filter_pck_get_duration(pck);

	if ((s64) start_ts > -ctx->delay) start_ts += ctx->delay;
	else start_ts = 0;
	o_start_ts = start_ts;
	if ((s64) end_ts > -ctx->delay) end_ts += ctx->delay;
	else end_ts = 0;

	start_ts = gf_timestamp_rescale(start_ts, timescale, 1000);
	end_ts = gf_timestamp_rescale(end_ts, timescale, 1000);

	gf_bs_reassign_buffer(ctx->bs_r, data, pck_size);

	while (gf_bs_available(ctx->bs_r)) {
		GF_TextSample *txt;
		//Bool is_utf_16=GF_FALSE;
		u32 type, /*length, */sample_index;

		if (!ctx->is_tx3g) {
			/*is_utf_16 = (Bool) */gf_bs_read_int(ctx->bs_r, 1);
			gf_bs_read_int(ctx->bs_r, 4);
			type = gf_bs_read_int(ctx->bs_r, 3);
			/*length = */gf_bs_read_u16(ctx->bs_r);

			/*currently only full text samples are supported*/
			if (type != 1) {
				return GF_NOT_SUPPORTED;
			}
			sample_index = gf_bs_read_u8(ctx->bs_r);
			/*duration*/
			//sample_duration = gf_bs_read_u24(ctx->bs_r);
		} else {
			sample_index = 1;
			/*duration*/
			//sample_duration = gf_filter_pck_get_duration(pck);
		}
		/*txt length is parsed with the sample*/
		txt = gf_isom_parse_text_sample(ctx->bs_r);
		if (!txt) return GF_NON_COMPLIANT_BITSTREAM;

		if (!dump)
			dump = gf_file_temp(NULL);

		if (ctx->dump_type) {
			ctx->cur_frame++;

			GF_TextSampleDescriptor *ent = gf_list_get(ctx->cfg->sample_descriptions, sample_index-1);
			if (ent) {
				GF_Tx3gSampleEntryBox txtd;
				GF_FontTableBox font_ent;

				tx3gmx_get_stsd(ent, &txtd, &font_ent);
				if (ctx->dump_type<3) {
					dump_ttxt_sample_srt(dump, txt, &txtd, (ctx->dump_type==2) ? GF_TRUE : GF_FALSE);
				} else {
					dump_ttxt_sample_ttml(ctx, dump, txt, &txtd, start_ts, end_ts);
				}
			}
		} else {
			dump_ttxt_sample(dump, txt, o_start_ts, gf_filter_pck_get_timescale(pck), sample_index, GF_FALSE);
		}

		gf_isom_delete_text_sample(txt);

		/*since we support only TTU(1), no need to go on*/
		if (!ctx->is_tx3g) {
			break;
		} else {
			//tx3g mode, single sample per AU
			break;
		}
	}

	if (dump && gf_ftell(dump)) {
		u32 size = (u32) gf_ftell(dump);
		dst_pck = gf_filter_pck_new_alloc(ctx->opid, size, &output);
		if (!dst_pck) return GF_OUT_OF_MEM;
		gf_fseek(dump, 0, SEEK_SET);
		gf_fread(output, size, dump);
		gf_filter_pck_merge_properties(pck, dst_pck);
		gf_filter_pck_set_byte_offset(dst_pck, GF_FILTER_NO_BO);
		gf_filter_pck_set_framing(dst_pck, GF_TRUE, GF_TRUE);
		gf_filter_pck_set_sap(dst_pck, GF_FILTER_SAP_1);
		gf_filter_pck_set_cts(dst_pck, o_start_ts);
		gf_filter_pck_set_dts(dst_pck, o_start_ts);

		gf_filter_pck_send(dst_pck);
	}
	if (dump) gf_fclose(dump);

	if (ctx->exporter) {
		u64 ts = gf_filter_pck_get_cts(pck);
		timescale = gf_filter_pck_get_timescale(pck);
		gf_set_progress("Exporting", ts*ctx->duration.den, ctx->duration.num*timescale);
	}

	gf_filter_pid_drop_packet(ctx->ipid);

	return GF_OK;
}

static GF_Err tx3gmx_initialize(GF_Filter *filter)
{
	TX3GMxCtx *ctx = gf_filter_get_udta(filter);
	ctx->bs_r = gf_bs_new((char *) "", 1, GF_BITSTREAM_READ);
	return GF_OK;
}

static void tx3gmx_finalize(GF_Filter *filter)
{
	TX3GMxCtx *ctx = gf_filter_get_udta(filter);
	if (ctx->cfg) gf_odf_desc_del((GF_Descriptor *) ctx->cfg);
	gf_bs_del(ctx->bs_r);
}

static const GF_FilterCapability TX3GMxCaps[] =
{
	CAP_UINT(GF_CAPS_INPUT,GF_PROP_PID_STREAM_TYPE, GF_STREAM_TEXT),
	CAP_UINT(GF_CAPS_INPUT,GF_PROP_PID_CODECID, GF_CODECID_TX3G),
	CAP_UINT(GF_CAPS_INPUT,GF_PROP_PID_CODECID, GF_CODECID_TEXT_MPEG4),
	CAP_BOOL(GF_CAPS_INPUT_EXCLUDED, GF_PROP_PID_UNFRAMED, GF_TRUE),
	CAP_UINT(GF_CAPS_OUTPUT, GF_PROP_PID_STREAM_TYPE, GF_STREAM_TEXT),
	CAP_UINT(GF_CAPS_OUTPUT, GF_PROP_PID_CODECID, GF_CODECID_TX3G),
	CAP_BOOL(GF_CAPS_OUTPUT, GF_PROP_PID_UNFRAMED, GF_TRUE),
	{0},
};


#define OFFS(_n)	#_n, offsetof(TX3GMxCtx, _n)
static const GF_FilterArgs TX3GMxArgs[] =
{
	{ OFFS(exporter), "compatibility with old exporter, displays export results", GF_PROP_BOOL, "false", NULL, GF_FS_ARG_HINT_ADVANCED},
	{0}
};

GF_FilterRegister TTXTMxRegister = {
	.name = "ufttxt",
	GF_FS_SET_DESCRIPTION("TX3G unframer")
	GF_FS_SET_HELP("This filter converts a single ISOBMFF TX3G stream to TTXT (xml format) unframed stream.")
	.private_size = sizeof(TX3GMxCtx),
	.args = TX3GMxArgs,
	.initialize = tx3gmx_initialize,
	.finalize = tx3gmx_finalize,
	SETCAPS(TX3GMxCaps),
	.configure_pid = tx3gmx_configure_pid,
	.process = tx3gmx_process
};

const GF_FilterRegister *ufttxt_register(GF_FilterSession *session)
{
	return &TTXTMxRegister;
}

static const GF_FilterCapability TX3G2SRTCaps[] =
{
	CAP_UINT(GF_CAPS_INPUT,GF_PROP_PID_STREAM_TYPE, GF_STREAM_TEXT),
	CAP_UINT(GF_CAPS_INPUT,GF_PROP_PID_CODECID, GF_CODECID_TX3G),
	CAP_UINT(GF_CAPS_INPUT,GF_PROP_PID_CODECID, GF_CODECID_TEXT_MPEG4),
	CAP_BOOL(GF_CAPS_INPUT_EXCLUDED, GF_PROP_PID_UNFRAMED, GF_TRUE),
	CAP_UINT(GF_CAPS_OUTPUT, GF_PROP_PID_STREAM_TYPE, GF_STREAM_TEXT),
	CAP_UINT(GF_CAPS_OUTPUT, GF_PROP_PID_CODECID, GF_CODECID_SIMPLE_TEXT),
	CAP_BOOL(GF_CAPS_OUTPUT, GF_PROP_PID_UNFRAMED, GF_TRUE),
	{0},
};

static GF_Err tx3g2srt_initialize(GF_Filter *filter)
{
	TX3GMxCtx *ctx = gf_filter_get_udta(filter);
	tx3gmx_initialize(filter);
	ctx->dump_type=1;
	return GF_OK;
}

GF_FilterRegister TX3G2SRTRegister = {
	.name = "tx3g2srt",
	GF_FS_SET_DESCRIPTION("TX3G to SRT")
	GF_FS_SET_HELP("This filter converts a single ISOBMFF TX3G stream to an SRT unframed stream.")
	.private_size = sizeof(TX3GMxCtx),
	.args = TX3GMxArgs,
	.initialize = tx3g2srt_initialize,
	.finalize = tx3gmx_finalize,
	SETCAPS(TX3G2SRTCaps),
	.configure_pid = tx3gmx_configure_pid,
	.process = tx3gmx_process
};

const GF_FilterRegister *tx3g2srt_register(GF_FilterSession *session)
{
	return &TX3G2SRTRegister;
}


static const GF_FilterCapability TX3G2VTTCaps[] =
{
	CAP_UINT(GF_CAPS_INPUT,GF_PROP_PID_STREAM_TYPE, GF_STREAM_TEXT),
	CAP_UINT(GF_CAPS_INPUT,GF_PROP_PID_CODECID, GF_CODECID_TX3G),
	CAP_UINT(GF_CAPS_INPUT,GF_PROP_PID_CODECID, GF_CODECID_TEXT_MPEG4),
	CAP_BOOL(GF_CAPS_INPUT_EXCLUDED, GF_PROP_PID_UNFRAMED, GF_TRUE),
	CAP_UINT(GF_CAPS_OUTPUT, GF_PROP_PID_STREAM_TYPE, GF_STREAM_TEXT),
	CAP_UINT(GF_CAPS_OUTPUT, GF_PROP_PID_CODECID, GF_CODECID_WEBVTT),
	CAP_BOOL(GF_CAPS_OUTPUT, GF_PROP_PID_UNFRAMED, GF_TRUE),
	{0},
};

static GF_Err tx3g2vtt_initialize(GF_Filter *filter)
{
	TX3GMxCtx *ctx = gf_filter_get_udta(filter);
	tx3gmx_initialize(filter);
	ctx->dump_type=2;
	return GF_OK;
}

GF_FilterRegister TTX2VTTRegister = {
	.name = "tx3g2vtt",
	GF_FS_SET_DESCRIPTION("TX3G to WebVTT")
	GF_FS_SET_HELP("This filter converts a single ISOBMFF TX3G stream to a WebVTT unframed stream.")
	.private_size = sizeof(TX3GMxCtx),
	.args = TX3GMxArgs,
	.initialize = tx3g2vtt_initialize,
	.finalize = tx3gmx_finalize,
	SETCAPS(TX3G2VTTCaps),
	.configure_pid = tx3gmx_configure_pid,
	.process = tx3gmx_process
};

const GF_FilterRegister *tx3g2vtt_register(GF_FilterSession *session)
{
	return &TTX2VTTRegister;
}


static const GF_FilterCapability TX3G2TTMLCaps[] =
{
	CAP_UINT(GF_CAPS_INPUT,GF_PROP_PID_STREAM_TYPE, GF_STREAM_TEXT),
	CAP_UINT(GF_CAPS_INPUT,GF_PROP_PID_CODECID, GF_CODECID_TX3G),
	CAP_UINT(GF_CAPS_INPUT,GF_PROP_PID_CODECID, GF_CODECID_TEXT_MPEG4),
	CAP_BOOL(GF_CAPS_INPUT_EXCLUDED, GF_PROP_PID_UNFRAMED, GF_TRUE),
	CAP_UINT(GF_CAPS_OUTPUT, GF_PROP_PID_STREAM_TYPE, GF_STREAM_TEXT),
	CAP_UINT(GF_CAPS_OUTPUT,GF_PROP_PID_CODECID, GF_CODECID_SUBS_XML),
	{0},
};

static GF_Err tx3g2ttml_initialize(GF_Filter *filter)
{
	TX3GMxCtx *ctx = gf_filter_get_udta(filter);
	tx3gmx_initialize(filter);
	ctx->dump_type=3;
	return GF_OK;
}

GF_FilterRegister TX3G2TTMLRegister = {
	.name = "tx3g2ttml",
	GF_FS_SET_DESCRIPTION("TX3G to TTML")
	GF_FS_SET_HELP("This filter converts ISOBMFF TX3G stream to a TTML stream.\n"
	"\n"
	"Each output TTML frame is a complete TTML document.")
	.private_size = sizeof(TX3GMxCtx),
	.args = NULL,
	.initialize = tx3g2ttml_initialize,
	.finalize = tx3gmx_finalize,
	SETCAPS(TX3G2TTMLCaps),
	.configure_pid = tx3gmx_configure_pid,
	.process = tx3gmx_process
};


const GF_FilterRegister *tx3g2ttml_register(GF_FilterSession *session)
{
	return &TX3G2TTMLRegister;
}

#else
const GF_FilterRegister *ufttxt_register(GF_FilterSession *session)
{
	return NULL;
}
const GF_FilterRegister *tx3g2srt_register(GF_FilterSession *session)
{
	return NULL;
}
const GF_FilterRegister *tx3g2vtt_register(GF_FilterSession *session)
{
	return NULL;
}
const GF_FilterRegister *tx3g2ttml_register(GF_FilterSession *session)
{
	return NULL;
}
#endif //#if !defined(GPAC_DISABLE_ISOM_DUMP) && !defined(GPAC_DISABLE_ISOM)



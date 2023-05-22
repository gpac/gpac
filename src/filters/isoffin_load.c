/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2000-2023
 *					All rights reserved
 *
 *  This file is part of GPAC / ISOBMFF reader filter
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

#include "isoffin.h"
#include <gpac/iso639.h>
#include <gpac/base_coding.h>
#include <gpac/media_tools.h>
#include <gpac/internal/isomedia_dev.h>

#ifndef GPAC_DISABLE_ISOM

static void isor_get_chapters(GF_ISOFile *file, GF_FilterPid *opid)
{
	u32 i, count;
	GF_PropertyValue p;
	GF_PropUIntList times;
	GF_PropStringList names;
	count = gf_isom_get_chapter_count(file, 0);
	if (count) {
		times.vals = gf_malloc(sizeof(u32)*count);
		names.vals = gf_malloc(sizeof(char *)*count);
		times.nb_items = names.nb_items = count;

		for (i=0; i<count; i++) {
			const char *name;
			u64 start;
			gf_isom_get_chapter(file, 0, i+1, &start, &name);
			times.vals[i] = (u32) start;
			names.vals[i] = gf_strdup(name);
		}
		p.type = GF_PROP_UINT_LIST;
		p.value.uint_list = times;
		gf_filter_pid_set_property(opid, GF_PROP_PID_CHAP_TIMES, &p);
		gf_free(times.vals);

		p.type = GF_PROP_STRING_LIST;
		p.value.string_list = names;
		gf_filter_pid_set_property(opid, GF_PROP_PID_CHAP_NAMES, &p);
		//no free for string lists
		return;
	}

	u32 chap_tk=0;
	count = gf_isom_get_track_count(file);
	for (i=0; i<count; i++) {
		u32 nb_ref = gf_isom_get_reference_count(file, i+1, GF_ISOM_REF_CHAP);
		if (nb_ref) {
			gf_isom_get_reference(file, i+1, GF_ISOM_REF_CHAP, 1, &chap_tk);
			break;
		}
	}
	if (chap_tk) {
		count = gf_isom_get_sample_count(file, chap_tk);
		if (!count) chap_tk=0;
	}
	if (!chap_tk) return;

	times.vals = gf_malloc(sizeof(u32)*count);
	names.vals = gf_malloc(sizeof(char *)*count);
	times.nb_items = names.nb_items = count;

	for (i=0; i<count; i++) {
		u32 di;
		GF_ISOSample *s = gf_isom_get_sample(file, chap_tk, i+1, &di);
		if (!s) continue;
		GF_BitStream *bs = gf_bs_new(s->data, s->dataLength, GF_BITSTREAM_READ);
		GF_TextSample *txt = gf_isom_parse_text_sample(bs);
		if (txt) {
			times.vals[i] = (u32) s->DTS;
			names.vals[i] = gf_strdup(txt->text);
			gf_isom_delete_text_sample(txt);
		}
		gf_bs_del(bs);
		gf_isom_sample_del(&s);
	}
	p.type = GF_PROP_UINT_LIST;
	p.value.uint_list = times;
	gf_filter_pid_set_property(opid, GF_PROP_PID_CHAP_TIMES, &p);
	gf_free(times.vals);

	p.type = GF_PROP_STRING_LIST;
	p.value.string_list = names;
	gf_filter_pid_set_property(opid, GF_PROP_PID_CHAP_NAMES, &p);

}

static void isor_export_ref(ISOMReader *read, ISOMChannel *ch, u32 rtype, char *rname)
{
	u32 nb_refs = gf_isom_get_reference_count(read->mov, ch->track, rtype);
	if (nb_refs) {
		u32 j;
		GF_PropertyValue prop;
		prop.type = GF_PROP_UINT_LIST;
		prop.value.uint_list.nb_items = nb_refs;
		prop.value.uint_list.vals = gf_malloc(sizeof(u32)*nb_refs);
		for (j=0; j<nb_refs; j++) {
			u32 ref_tk;
			gf_isom_get_reference(read->mov, ch->track, rtype, j+1, &ref_tk );
			prop.value.uint_list.vals[j] = gf_isom_get_track_id(read->mov, ref_tk);
		}
		gf_filter_pid_set_property_str(ch->pid, rname, &prop);
		gf_free(prop.value.uint_list.vals);
	}
}

static void isor_declare_track(ISOMReader *read, ISOMChannel *ch, u32 track, u32 stsd_idx, u32 streamtype, Bool use_iod)
{
	u32 w, h, sr, nb_ch, nb_bps, codec_id, depends_on_id, esid, avg_rate, max_rate, buffer_size, sample_count, max_size, base_track, audio_fmt, pix_fmt;
	GF_ESD *an_esd;
	const char *mime, *encoding, *stxtcfg, *namespace, *schemaloc, *mime_cfg;
#if !defined(GPAC_DISABLE_ISOM_WRITE)
	u8 *tk_template;
	u32 tk_template_size;
#endif
	GF_Language *lang_desc = NULL;
	Bool external_base=GF_FALSE;
	Bool has_scalable_layers = GF_FALSE;
	u8 *dsi = NULL, *enh_dsi = NULL;
	u32 dsi_size = 0, enh_dsi_size = 0;
	Double track_dur=0;
	u32 srd_id=0, srd_indep=0, srd_x=0, srd_y=0, srd_w=0, srd_h=0;
	u32 base_tile_track=0;
	Bool srd_full_frame=GF_FALSE;
	u32 mtype, m_subtype;
	GF_GenericSampleDescription *udesc = NULL;
	GF_Err e;
	u32 ocr_es_id;
	u32 meta_codec_id = 0;
	char *meta_codec_name = NULL;
	u32 meta_opaque=0;
	Bool first_config = GF_FALSE;


	depends_on_id = avg_rate = max_rate = buffer_size = 0;
	mime = encoding = stxtcfg = namespace = schemaloc = mime_cfg = NULL;

	if ( gf_isom_is_media_encrypted(read->mov, track, stsd_idx)) {
		gf_isom_get_original_format_type(read->mov, track, stsd_idx, &m_subtype);
	} else {
		m_subtype = gf_isom_get_media_subtype(read->mov, track, stsd_idx);
	}
	
	audio_fmt = 0;
	pix_fmt = 0;
	ocr_es_id = 0;
	an_esd = gf_media_map_esd(read->mov, track, stsd_idx);
	if (an_esd && an_esd->decoderConfig) {
		if (an_esd->decoderConfig->streamType==GF_STREAM_INTERACT) {
			gf_odf_desc_del((GF_Descriptor *)an_esd);
			return;
		}
		streamtype = an_esd->decoderConfig->streamType;
		if (an_esd->decoderConfig->objectTypeIndication < GF_CODECID_LAST_MPEG4_MAPPING) {
			codec_id = gf_codecid_from_oti(streamtype, an_esd->decoderConfig->objectTypeIndication);
		} else {
			codec_id = an_esd->decoderConfig->objectTypeIndication;
		}
		ocr_es_id = an_esd->OCRESID;
		depends_on_id = an_esd->dependsOnESID;
		lang_desc = an_esd->langDesc;
		an_esd->langDesc = NULL;
		esid = an_esd->ESID;

		if (an_esd->decoderConfig->decoderSpecificInfo && an_esd->decoderConfig->decoderSpecificInfo->data) {
			dsi = an_esd->decoderConfig->decoderSpecificInfo->data;
			dsi_size = an_esd->decoderConfig->decoderSpecificInfo->dataLength;
			an_esd->decoderConfig->decoderSpecificInfo->data = NULL;
			an_esd->decoderConfig->decoderSpecificInfo->dataLength = 0;
		}

		gf_odf_desc_del((GF_Descriptor *)an_esd);

#ifndef GPAC_DISABLE_AV_PARSERS
		if (dsi && (codec_id==GF_CODECID_AAC_MPEG4)) {
			GF_M4ADecSpecInfo acfg;
			gf_m4a_get_config(dsi, dsi_size, &acfg);
			if (acfg.base_object_type == GF_M4A_USAC)
				codec_id = GF_CODECID_USAC;
		}
#endif

	} else {
		u32 i, pcm_flags, pcm_size, pcm_ch;
		Double pcm_sr;
		Bool load_default = GF_FALSE;
		Bool is_float = GF_FALSE;

		if (an_esd)
			gf_odf_desc_del((GF_Descriptor *)an_esd);

		lang_desc = (GF_Language *) gf_odf_desc_new(GF_ODF_LANG_TAG);
		gf_isom_get_media_language(read->mov, track, &lang_desc->full_lang_code);
		esid = gf_isom_get_track_id(read->mov, track);

		if (!streamtype) streamtype = gf_codecid_type(m_subtype);
		codec_id = 0;

		switch (m_subtype) {
		case GF_ISOM_SUBTYPE_STXT:
		case GF_ISOM_SUBTYPE_METT:
		case GF_ISOM_SUBTYPE_SBTT:
		case GF_ISOM_MEDIA_SUBT:

			codec_id = GF_CODECID_SIMPLE_TEXT;
			gf_isom_stxt_get_description(read->mov, track, stsd_idx, &mime, &encoding, &stxtcfg);
			break;
		case GF_ISOM_SUBTYPE_STPP:
			codec_id = GF_CODECID_SUBS_XML;
			gf_isom_xml_subtitle_get_description(read->mov, track, stsd_idx, &namespace, &schemaloc, &mime);
			break;
		case GF_ISOM_SUBTYPE_METX:
			codec_id = GF_CODECID_META_XML;
			gf_isom_xml_subtitle_get_description(read->mov, track, stsd_idx, &namespace, &schemaloc, &mime);
			break;
		case GF_ISOM_SUBTYPE_WVTT:
			codec_id = GF_CODECID_WEBVTT;
#ifndef GPAC_DISABLE_VTT
			stxtcfg = gf_isom_get_webvtt_config(read->mov, track, stsd_idx);
#endif
			break;
		case GF_ISOM_SUBTYPE_MJP2:
			codec_id = GF_CODECID_J2K;
			gf_isom_get_jp2_config(read->mov, track, stsd_idx, &dsi, &dsi_size);
			break;
		case GF_ISOM_SUBTYPE_HVT1:
			codec_id = GF_CODECID_HEVC_TILES;
			gf_isom_get_reference(read->mov, track, GF_ISOM_REF_TBAS, 1, &base_tile_track);
			if (base_tile_track) {
				depends_on_id = gf_isom_get_track_id(read->mov, base_tile_track);
			}
			gf_isom_get_tile_info(read->mov, track, 1, NULL, &srd_id, &srd_indep, &srd_full_frame, &srd_x, &srd_y, &srd_w, &srd_h);
			break;
		case GF_ISOM_SUBTYPE_TEXT:
		case GF_ISOM_SUBTYPE_TX3G:
		{
			GF_TextSampleDescriptor *txtcfg = NULL;
			codec_id = GF_CODECID_TX3G;
			e = gf_isom_get_text_description(read->mov, track, stsd_idx, &txtcfg);
			if (e) {
				GF_LOG(GF_LOG_WARNING, GF_LOG_CONTAINER, ("[IsoMedia] Track %d unable to fetch TX3G config\n", track));
			}
			if (txtcfg) {
				gf_odf_tx3g_write(txtcfg, &dsi, &dsi_size);
				gf_odf_desc_del((GF_Descriptor *) txtcfg);
			}
		}
			break;

		case GF_ISOM_SUBTYPE_FLAC:
			codec_id = GF_CODECID_FLAC;
			gf_isom_flac_config_get(read->mov, track, stsd_idx, &dsi, &dsi_size);
			break;
		case GF_ISOM_SUBTYPE_OPUS:
			codec_id = GF_CODECID_OPUS;
			gf_isom_opus_config_get(read->mov, track, stsd_idx, &dsi, &dsi_size);
			break;

		case GF_QT_SUBTYPE_FL32:
		case GF_QT_SUBTYPE_FL64:
			is_float = GF_TRUE;
		case GF_QT_SUBTYPE_TWOS:
		case GF_QT_SUBTYPE_SOWT:
		case GF_QT_SUBTYPE_IN24:
		case GF_QT_SUBTYPE_IN32:
			if (gf_isom_get_pcm_config(read->mov, track, stsd_idx, &pcm_flags, &pcm_size) == GF_OK) {
				codec_id = GF_CODECID_RAW;
				if (pcm_size==16) audio_fmt = (pcm_flags&1) ? GF_AUDIO_FMT_S16 : GF_AUDIO_FMT_S16_BE;
				else if (pcm_size==24) audio_fmt = (pcm_flags&1) ? GF_AUDIO_FMT_S24 : GF_AUDIO_FMT_S24_BE;
				else if (!is_float && (pcm_size==32)) audio_fmt = (pcm_flags&1) ? GF_AUDIO_FMT_S32 : GF_AUDIO_FMT_S32_BE;
				else if (is_float && (pcm_size==32)) audio_fmt = (pcm_flags&1) ? GF_AUDIO_FMT_FLT : GF_AUDIO_FMT_FLT_BE;
				else if (is_float && (pcm_size==64)) audio_fmt = (pcm_flags&1) ? GF_AUDIO_FMT_DBL : GF_AUDIO_FMT_DBL_BE;
				else codec_id = 0;
			}
			break;
		case GF_ISOM_MEDIA_TIMECODE:
			codec_id = GF_CODECID_TMCD;
			streamtype = GF_STREAM_METADATA;
			break;

		case GF_QT_SUBTYPE_RAW:
			codec_id = GF_CODECID_RAW;
			if (streamtype==GF_STREAM_AUDIO)
 				audio_fmt = GF_AUDIO_FMT_U8;
			else
 				pix_fmt = GF_PIXEL_RGB;
			break;

		case GF_ISOM_SUBTYPE_IPCM:
			if (gf_isom_get_pcm_config(read->mov, track, stsd_idx, &pcm_flags, &pcm_size) == GF_OK) {
				codec_id = GF_CODECID_RAW;
				if (pcm_size==16) audio_fmt = (pcm_flags & 1) ? GF_AUDIO_FMT_S16 : GF_AUDIO_FMT_S16_BE;
				else if (pcm_size==24) audio_fmt = (pcm_flags & 1) ? GF_AUDIO_FMT_S24 : GF_AUDIO_FMT_S24_BE;
				else if (pcm_size==32) audio_fmt = (pcm_flags & 1) ? GF_AUDIO_FMT_S32 : GF_AUDIO_FMT_S32_BE;
			}
			break;
		case GF_ISOM_SUBTYPE_FPCM:
			if (gf_isom_get_pcm_config(read->mov, track, stsd_idx, &pcm_flags, &pcm_size) == GF_OK) {
				codec_id = GF_CODECID_RAW;
				if (pcm_size==64) audio_fmt = (pcm_flags & 1) ? GF_AUDIO_FMT_DBL : GF_AUDIO_FMT_DBL_BE;
				else if (pcm_size==32) audio_fmt = (pcm_flags & 1) ? GF_AUDIO_FMT_FLT : GF_AUDIO_FMT_FLT_BE;
				else codec_id = 0;
			}
			break;
		case GF_QT_SUBTYPE_LPCM:
			if (gf_isom_get_lpcm_config(read->mov, track, stsd_idx, &pcm_sr, &pcm_ch, &pcm_flags, &pcm_size) == GF_OK) {
				audio_fmt = 0;
				Bool is_be = (pcm_flags & (1<<1)) ? GF_TRUE : GF_FALSE;
				if (pcm_flags >> 4) {}
				//we don't support non-packed audio
				else if (!(pcm_flags & (1<<3))) {}
				else if (pcm_sr != (u32) gf_ceil(pcm_sr)) {}
				else {
					sr = (u32) gf_ceil(pcm_sr);
					nb_ch = pcm_ch;
					//float formats
					if (pcm_flags & 1) {
						if (pcm_size==64) audio_fmt = is_be ? GF_AUDIO_FMT_DBL_BE : GF_AUDIO_FMT_DBL;
						else if (pcm_size==32) audio_fmt = is_be ? GF_AUDIO_FMT_FLT_BE : GF_AUDIO_FMT_FLT;
					}
					//signed int formats
					else if (pcm_flags & 4) {
						if (pcm_size==16) audio_fmt = is_be ? GF_AUDIO_FMT_S16_BE : GF_AUDIO_FMT_S16;
						else if (pcm_size==24) audio_fmt = is_be ? GF_AUDIO_FMT_S24_BE : GF_AUDIO_FMT_S24;
						else if (pcm_size==32) audio_fmt = is_be ? GF_AUDIO_FMT_S32_BE : GF_AUDIO_FMT_S32;
					} else {
						if (pcm_size==8) audio_fmt = GF_AUDIO_FMT_U8;
					}
					if (audio_fmt) {
						codec_id = GF_CODECID_RAW;
						nb_bps = pcm_size;
					}
				}
			}
			break;

		case GF_ISOM_SUBTYPE_VVC1:
		case GF_ISOM_SUBTYPE_VVI1:
		{
			GF_VVCConfig *vvccfg = gf_isom_vvc_config_get(read->mov, track, stsd_idx);
			if (vvccfg) {
				gf_odf_vvc_cfg_write(vvccfg, &dsi, &dsi_size);
				gf_odf_vvc_cfg_del(vvccfg);
			}
			codec_id = GF_CODECID_VVC;
		}
			break;
		case GF_ISOM_SUBTYPE_VVS1:
			base_tile_track = 0;
			codec_id = gf_isom_get_track_group(read->mov, track, GF_4CC('a','l','t','e') );
			for (i=0; i<gf_isom_get_track_count(read->mov); i++) {
				u32 j, nb_refs = gf_isom_get_reference_count(read->mov, i+1, GF_ISOM_REF_SUBPIC);
				for (j=0; j<nb_refs; j++) {
					u32 ref_tk=0;
					gf_isom_get_reference_ID(read->mov, i+1, GF_ISOM_REF_SUBPIC, j+1, &ref_tk);
					if ((ref_tk == esid) || (ref_tk == codec_id)) {
						base_tile_track = i+1;
						depends_on_id = gf_isom_get_track_id(read->mov, base_tile_track);
						break;
					}
				}
				if (base_tile_track) break;
			}
			if (!base_tile_track) {
				GF_LOG(GF_LOG_WARNING, GF_LOG_CONTAINER, ("[IsoMedia] VVC subpicture track ID %d with no base VVC track referencing it, ignoring\n", esid));
				if (lang_desc) gf_odf_desc_del((GF_Descriptor *)lang_desc);
				if (dsi) gf_free(dsi);
				return;
			}
			codec_id = GF_CODECID_VVC_SUBPIC;
			gf_isom_get_tile_info(read->mov, track, 1, NULL, &srd_id, &srd_indep, &srd_full_frame, &srd_x, &srd_y, &srd_w, &srd_h);
			break;

		case GF_ISOM_SUBTYPE_AC3:
		case GF_ISOM_SUBTYPE_EC3:
		{
			GF_AC3Config *ac3cfg = gf_isom_ac3_config_get(read->mov, track, stsd_idx);
			codec_id = (m_subtype==GF_ISOM_SUBTYPE_AC3) ? GF_CODECID_AC3 : GF_CODECID_EAC3;
			if (ac3cfg) {
				gf_odf_ac3_cfg_write(ac3cfg, &dsi, &dsi_size);
				gf_free(ac3cfg);
			} else {
				GF_LOG(GF_LOG_WARNING, GF_LOG_CONTAINER, ("[IsoMedia] Track %d missing AC3/EC3 configuration !\n", track));
			}
		}
			break;

		case GF_ISOM_SUBTYPE_MLPA:
		{
			u32 fmt, prate;
			if (gf_isom_truehd_config_get(read->mov, track, stsd_idx, &fmt, &prate) == GF_OK) {
				GF_BitStream *bs = gf_bs_new(NULL, 0, GF_BITSTREAM_WRITE);
				gf_bs_write_u32(bs, fmt);
				gf_bs_write_int(bs, prate, 15);
				gf_bs_write_int(bs, 0, 1);
				gf_bs_write_u32(bs, 0);
				gf_bs_get_content(bs, &dsi, &dsi_size);
				gf_bs_del(bs);
				codec_id = GF_CODECID_TRUEHD;
			} else {
				GF_LOG(GF_LOG_WARNING, GF_LOG_CONTAINER, ("[IsoMedia] Track %d missing TrueHD configuration !\n", track));
			}
			break;
		}

		case GF_ISOM_SUBTYPE_DVB_SUBS:
			codec_id = GF_CODECID_DVB_SUBS;
			load_default = GF_TRUE;
			break;
		case GF_ISOM_SUBTYPE_DVB_TELETEXT:
			codec_id = GF_CODECID_DVB_TELETEXT;
			load_default = GF_TRUE;
			break;

		case GF_4CC('G','M','C','W'):
			codec_id = m_subtype;
			load_default = GF_TRUE;
			break;

		default:
			codec_id = gf_codec_id_from_isobmf(m_subtype);
			if (!codec_id || (codec_id==GF_CODECID_RAW)) {
				pix_fmt=0;
				if (streamtype==GF_STREAM_VISUAL) {
					pix_fmt = gf_pixel_fmt_from_qt_type(m_subtype);
					if (!pix_fmt && gf_pixel_fmt_probe(m_subtype, NULL))
						pix_fmt = m_subtype;
				}

 				if (pix_fmt) {
					codec_id = GF_CODECID_RAW;
					if (pix_fmt==GF_PIXEL_UNCV) {
						load_default = GF_TRUE;
						codec_id = GF_CODECID_RAW_UNCV;
					}
				} else {
					load_default = GF_TRUE;
				}
			}
			//load dsi in any other case
			else {
			//if ((codec_id==GF_CODECID_FFV1) || (codec_id==GF_CODECID_ALAC))
				load_default = GF_TRUE;
			}
			break;
		}

		if (load_default) {
			if (!codec_id) {
				GF_LOG(GF_LOG_WARNING, GF_LOG_CONTAINER, ("[IsoMedia] Track %d type %s not natively handled\n", track, gf_4cc_to_str(m_subtype) ));

				codec_id = m_subtype;
			}
			udesc = gf_isom_get_generic_sample_description(read->mov, track, stsd_idx);
			if (udesc) {
				if ((codec_id==GF_CODECID_FFV1) && (udesc->extension_buf_size>8)) {
					dsi = gf_malloc(udesc->extension_buf_size-8);
					if (dsi) memcpy(dsi, udesc->extension_buf+8, udesc->extension_buf_size-8);
					dsi_size = udesc->extension_buf_size - 8;
				} else if ((codec_id==GF_4CC('G','M','C','W')) && (udesc->extension_buf_size>=16)) {
					GF_BitStream *bs = gf_bs_new(udesc->extension_buf, udesc->extension_buf_size, GF_BITSTREAM_READ);
					if (udesc->ext_box_wrap == GF_4CC('G','M','C','C')) {
						codec_id = gf_bs_read_u32(bs);
						meta_codec_id = gf_bs_read_u32(bs);
						meta_codec_name = gf_bs_read_utf8(bs);
						meta_opaque = gf_bs_read_u32(bs);
						if (gf_bs_available(bs)) {
							u32 pos = (u32) gf_bs_get_position(bs);
							dsi = udesc->extension_buf+pos;
							dsi_size = udesc->extension_buf_size-pos;
						}
					}
					gf_bs_del(bs);
				} else {
					dsi = udesc->extension_buf;
					dsi_size = udesc->extension_buf_size;
					udesc->extension_buf = NULL;
					udesc->extension_buf_size = 0;
				}
			}
		}
	}
	if (!streamtype || !codec_id) {
		if (udesc) gf_free(udesc);
		GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[IsoMedia] Failed to %s pid for track %d, could not extract codec/streamtype info\n", ch ? "update" : "create", track));
		if (lang_desc) gf_odf_desc_del((GF_Descriptor *)lang_desc);
		if (dsi) gf_free(dsi);
		return;
	}

	mime_cfg = gf_isom_subtitle_get_mime(read->mov, track, stsd_idx);

	//first setup, creation of PID and channel
	if (!ch) {
		Bool use_sidx_dur = GF_FALSE;
		GF_FilterPid *pid;
		first_config = GF_TRUE;

		gf_isom_get_reference(read->mov, track, GF_ISOM_REF_BASE, 1, &base_track);

		if (base_track) {
			u32 base_subtype=0;
			if (read->smode==MP4DMX_SINGLE)
				depends_on_id = 0;

			switch (m_subtype) {
			case GF_ISOM_SUBTYPE_LHV1:
			case GF_ISOM_SUBTYPE_LHE1:
				base_subtype = gf_isom_get_media_subtype(read->mov, base_track, stsd_idx);
				switch (base_subtype) {
				case GF_ISOM_SUBTYPE_HVC1:
				case GF_ISOM_SUBTYPE_HEV1:
				case GF_ISOM_SUBTYPE_HVC2:
				case GF_ISOM_SUBTYPE_HEV2:
					break;
				default:
					external_base=GF_TRUE;
					break;
				}
			}
			if (external_base) {
				depends_on_id = gf_isom_get_track_id(read->mov, base_track);
				has_scalable_layers = GF_TRUE;
			} else {
				switch (gf_isom_get_hevc_lhvc_type(read->mov, track, stsd_idx)) {
				case GF_ISOM_HEVCTYPE_HEVC_LHVC:
				case GF_ISOM_HEVCTYPE_LHVC_ONLY:
					has_scalable_layers = GF_TRUE;
					break;
				//this is likely temporal sublayer of base
				case GF_ISOM_HEVCTYPE_HEVC_ONLY:
					has_scalable_layers = GF_FALSE;
					if (gf_isom_get_reference_count(read->mov, track, GF_ISOM_REF_SCAL)<=0) {
						depends_on_id = gf_isom_get_track_id(read->mov, base_track);
					}
					break;
				default:
					break;
				}
			}
		} else {
			switch (gf_isom_get_hevc_lhvc_type(read->mov, track, stsd_idx)) {
			case GF_ISOM_HEVCTYPE_HEVC_LHVC:
			case GF_ISOM_HEVCTYPE_LHVC_ONLY:
				has_scalable_layers = GF_TRUE;
				break;
			default:
				break;
			}

			if (!has_scalable_layers) {
				u32 i;
				GF_ISOTrackID track_id = gf_isom_get_track_id(read->mov, track);
				for (i=0; i<gf_isom_get_track_count(read->mov); i++) {
					if (gf_isom_get_reference_count(read->mov, i+1, GF_ISOM_REF_BASE)>=0) {
						GF_ISOTrackID tkid;
						gf_isom_get_reference_ID(read->mov, i+1, GF_ISOM_REF_BASE, 1, &tkid);
						if (tkid==track_id) {
							has_scalable_layers = GF_TRUE;
							break;
						}
					}
				}
			}
		}

		if (base_track && !ocr_es_id) {
			ocr_es_id = gf_isom_get_track_id(read->mov, base_track);
		}
		if (!ocr_es_id) ocr_es_id = esid;

		//OK declare PID
		pid = gf_filter_pid_new(read->filter);
		if (read->pid)
			gf_filter_pid_copy_properties(pid, read->pid);

		gf_filter_pid_set_property(pid, GF_PROP_PID_ID, &PROP_UINT(esid));
		gf_filter_pid_set_property(pid, GF_PROP_PID_CLOCK_ID, &PROP_UINT(ocr_es_id));
		if (depends_on_id && (depends_on_id != esid))
			gf_filter_pid_set_property(pid, GF_PROP_PID_DEPENDENCY_ID, &PROP_UINT(depends_on_id));

		if (gf_isom_get_track_count(read->mov)>1) {
			char szPName[1024];
			const char *szST = gf_stream_type_name(streamtype);
			sprintf(szPName, "%c%d", szST[0], esid);
			gf_filter_pid_set_name(pid, szPName);
		}

		//MPEG-4 systems present
		if (use_iod)
			gf_filter_pid_set_property(pid, GF_PROP_PID_ESID, &PROP_UINT(esid));

		if (gf_isom_is_track_in_root_od(read->mov, track) && !read->lightp) {
			switch (streamtype) {
			case GF_STREAM_SCENE:
			case GF_STREAM_OD:
				gf_filter_pid_set_property(pid, GF_PROP_PID_IN_IOD, &PROP_BOOL(GF_TRUE));
				break;
			}
		}

		gf_filter_pid_set_property(pid, GF_PROP_PID_STREAM_TYPE, &PROP_UINT(streamtype));
		gf_filter_pid_set_property(pid, GF_PROP_PID_TIMESCALE, &PROP_UINT( gf_isom_get_media_timescale(read->mov, track) ) );

		if (!gf_sys_is_test_mode())
			gf_filter_pid_set_property(pid, GF_PROP_PID_TRACK_NUM, &PROP_UINT(track) );

		//Dolby Vision - check for any video type
		GF_DOVIDecoderConfigurationRecord *dovi = gf_isom_dovi_config_get(read->mov, track, stsd_idx);
		if (dovi) {
			u8 *data = NULL;
			u32 size = 0;
			GF_BitStream *bs = gf_bs_new(NULL, 0, GF_BITSTREAM_WRITE);
			gf_odf_dovi_cfg_write_bs(dovi, bs);
			gf_bs_get_content(bs, &data, &size);
			gf_filter_pid_set_property(pid, GF_PROP_PID_DOLBY_VISION, &PROP_DATA_NO_COPY(data, size));
			gf_bs_del(bs);
			gf_odf_dovi_cfg_del(dovi);

			if (gf_isom_get_reference_count(read->mov, track, GF_4CC('v','d','e','p'))) {
				GF_ISOTrackID ref_id=0;
				gf_isom_get_reference_ID(read->mov, track, GF_4CC('v','d','e','p'), 1, &ref_id);
				if (ref_id) gf_filter_pid_set_property(pid, GF_PROP_PID_DEPENDENCY_ID, &PROP_UINT(ref_id));
			}
		}

		//create our channel
		ch = isor_create_channel(read, pid, track, 0, (codec_id==GF_CODECID_LHVC) ? GF_TRUE : GF_FALSE);

		if (lang_desc) {
			char *lang=NULL;
			gf_isom_get_media_language(read->mov, track, &lang);
			//s32 idx = gf_lang_find(lang);
			gf_filter_pid_set_property(pid, GF_PROP_PID_LANGUAGE, &PROP_STRING( lang ));
			if (lang) gf_free(lang);
			gf_odf_desc_del((GF_Descriptor *)lang_desc);
			lang_desc = NULL;
		}

		ch->streamType = streamtype;
//		ch->clock_id = ocr_es_id;

		if (!read->lightp) {
			if (has_scalable_layers)
				gf_filter_pid_set_property(pid, GF_PROP_PID_SCALABLE, &PROP_BOOL(GF_TRUE));

			if (gf_isom_get_reference_count(read->mov, track, GF_ISOM_REF_SABT)>0) {
				gf_filter_pid_set_property(pid, GF_PROP_PID_TILE_BASE, &PROP_BOOL(GF_TRUE));
			}
			else if (gf_isom_get_reference_count(read->mov, track, GF_ISOM_REF_SUBPIC)>0) {
				gf_filter_pid_set_property(pid, GF_PROP_PID_TILE_BASE, &PROP_BOOL(GF_TRUE));
			}

			if (srd_w && srd_h) {
				gf_filter_pid_set_property(pid, GF_PROP_PID_CROP_POS, &PROP_VEC2I_INT(srd_x, srd_y) );
				if (base_tile_track) {
					gf_isom_get_visual_info(read->mov, base_tile_track, stsd_idx, &w, &h);
					if (w && h) {
						gf_filter_pid_set_property(pid, GF_PROP_PID_ORIG_SIZE, &PROP_VEC2I_INT(w, h) );
					}
				}
			}


			if (codec_id !=GF_CODECID_LHVC)
				isor_export_ref(read, ch, GF_ISOM_REF_SCAL, "isom:scal");
			isor_export_ref(read, ch, GF_ISOM_REF_SABT, "isom:sabt");
			isor_export_ref(read, ch, GF_ISOM_REF_TBAS, "isom:tbas");
			isor_export_ref(read, ch, GF_ISOM_REF_SUBPIC, "isom:subp");
		}

		if (read->lightp) {
			ch->duration = gf_isom_get_track_duration_orig(read->mov, ch->track);
		} else {
			ch->duration = gf_isom_get_track_duration(read->mov, ch->track);
		}
		if (!ch->duration) {
			ch->duration = gf_isom_get_duration(read->mov);
		}
		sample_count = gf_isom_get_sample_count(read->mov, ch->track);

#ifndef GPAC_DISABLE_ISOM_FRAGMENTS
		if (read->frag_type && !read->input_loaded) {
			u32 ts;
			u64 dur;
			if (gf_isom_get_sidx_duration(read->mov, &dur, &ts)==GF_OK) {
				dur *= read->timescale;
				dur /= ts;
				ch->duration = dur;
				use_sidx_dur = GF_TRUE;
				sample_count = 0;
			}
		}
#endif

		if (!read->mem_load_mode || ch->duration) {
			//if no edit list (whether complex or simple TS offset) and no sidx, use media duration
			if (!ch->has_edit_list && !use_sidx_dur && !ch->ts_offset) {
				//no specific edit list type but edit present, use the duration in the edit
				if (gf_isom_get_edits_count(read->mov, ch->track)) {
					gf_filter_pid_set_property(pid, GF_PROP_PID_DURATION, &PROP_FRAC64_INT(ch->duration, read->timescale));
				} else {
					u64 dur = gf_isom_get_media_duration(read->mov, ch->track);
					gf_filter_pid_set_property(pid, GF_PROP_PID_DURATION, &PROP_FRAC64_INT(dur, ch->timescale));
				}
			}
			//otherwise trust track duration
			else {
				gf_filter_pid_set_property(pid, GF_PROP_PID_DURATION, &PROP_FRAC64_INT(ch->duration, read->timescale));
			}
			gf_filter_pid_set_property(pid, GF_PROP_PID_NB_FRAMES, &PROP_UINT(sample_count));
		}

		if (sample_count && (streamtype==GF_STREAM_VISUAL)) {
			u64 mdur = gf_isom_get_media_duration(read->mov, track);
			//if ts_offset is negative (skip), update media dur before computing fps
			if (!gf_sys_old_arch_compat()) {
				u32 sdur = gf_isom_get_avg_sample_delta(read->mov, ch->track);
				if (sdur) {
					mdur = sdur;
				} else {
					if (ch->ts_offset<0)
						mdur -= (u32) -ch->ts_offset;
					mdur /= sample_count;
				}
			} else {
				mdur /= sample_count;
			}
			gf_filter_pid_set_property(pid, GF_PROP_PID_FPS, &PROP_FRAC_INT(ch->timescale, (u32) mdur));
		}

		track_dur = (Double) (s64) ch->duration;
		track_dur /= read->timescale;
		//move channel duration in media timescale
		ch->duration = (u64) (track_dur * ch->timescale);


		//set stream subtype
		mtype = gf_isom_get_media_type(read->mov, track);
		gf_filter_pid_set_property(ch->pid, GF_PROP_PID_SUBTYPE, &PROP_4CC(mtype) );

		if (!read->mem_load_mode) {
			gf_filter_pid_set_property(ch->pid, GF_PROP_PID_MEDIA_DATA_SIZE, &PROP_LONGUINT(gf_isom_get_media_data_size(read->mov, track) ) );
		}
		//in no cache mode, depending on fetch speed we may have fetched a fragment or not, resulting in has_rap set
		//always for HAS_SYNC to false
		else if (gf_sys_is_test_mode() && !sample_count) {
			gf_filter_pid_set_property(pid, GF_PROP_PID_HAS_SYNC, &PROP_BOOL(GF_FALSE) );
		}

		if (read->lightp) goto props_done;

		w = gf_isom_get_constant_sample_size(read->mov, track);
		if (w)
			gf_filter_pid_set_property(ch->pid, GF_PROP_PID_FRAME_SIZE, &PROP_UINT(w));

		//mem mode, cannot read backwards
		if (read->mem_load_mode) {
			const GF_PropertyValue *p = gf_filter_pid_get_property(read->pid, GF_PROP_PID_PLAYBACK_MODE);
			if (!p)
				gf_filter_pid_set_property(pid, GF_PROP_PID_PLAYBACK_MODE, &PROP_UINT(GF_PLAYBACK_MODE_FASTFORWARD) );
		} else {
			gf_filter_pid_set_property(pid, GF_PROP_PID_PLAYBACK_MODE, &PROP_UINT(GF_PLAYBACK_MODE_REWIND) );
		}

		GF_PropertyValue brands;
		brands.type = GF_PROP_4CC_LIST;
		u32 major_brand=0;
		gf_isom_get_brand_info(read->mov, &major_brand, NULL, &brands.value.uint_list.nb_items);
		brands.value.uint_list.vals = (u32 *) gf_isom_get_brands(read->mov);
		gf_filter_pid_set_property(ch->pid, GF_PROP_PID_ISOM_BRANDS, &brands);
		gf_filter_pid_set_property(ch->pid, GF_PROP_PID_ISOM_MBRAND, &PROP_4CC(major_brand) );

		//we cannot expose average size/dur in mem mode with fragmented files (sample_count=0)
		if (sample_count) {
			max_size = gf_isom_get_max_sample_size(read->mov, ch->track);
			if (max_size) gf_filter_pid_set_property(pid, GF_PROP_PID_MAX_FRAME_SIZE, &PROP_UINT(max_size) );

			max_size = gf_isom_get_avg_sample_size(read->mov, ch->track);
			if (max_size) gf_filter_pid_set_property(pid, GF_PROP_PID_AVG_FRAME_SIZE, &PROP_UINT(max_size) );

			max_size = gf_isom_get_max_sample_delta(read->mov, ch->track);
			if (max_size) gf_filter_pid_set_property(pid, GF_PROP_PID_MAX_TS_DELTA, &PROP_UINT(max_size) );

			max_size = gf_isom_get_max_sample_cts_offset(read->mov, ch->track);
			if (max_size) gf_filter_pid_set_property(pid, GF_PROP_PID_MAX_CTS_OFFSET, &PROP_UINT(max_size) );

			max_size = gf_isom_get_constant_sample_duration(read->mov, ch->track);
			if (max_size) gf_filter_pid_set_property(pid, GF_PROP_PID_CONSTANT_DURATION, &PROP_UINT(max_size) );
		}


		u32 media_pl=0;
		if (streamtype==GF_STREAM_VISUAL) {
			media_pl = gf_isom_get_pl_indication(read->mov, GF_ISOM_PL_VISUAL);
		} else if (streamtype==GF_STREAM_AUDIO) {
			media_pl = gf_isom_get_pl_indication(read->mov, GF_ISOM_PL_AUDIO);
		}
		if (media_pl && (media_pl!=0xFF) ) gf_filter_pid_set_property(pid, GF_PROP_PID_PROFILE_LEVEL, &PROP_UINT(media_pl) );

#if !defined(GPAC_DISABLE_ISOM_WRITE)
		e = gf_isom_get_track_template(read->mov, ch->track, &tk_template, &tk_template_size);
		if (e == GF_OK) {
			gf_filter_pid_set_property(ch->pid, GF_PROP_PID_ISOM_TRACK_TEMPLATE, &PROP_DATA_NO_COPY(tk_template, tk_template_size) );
		} else {
			GF_LOG(GF_LOG_WARNING, GF_LOG_CONTAINER, ("[IsoMedia] Failed to serialize track box: %s\n", gf_error_to_string(e) ));
		}

		e = gf_isom_get_trex_template(read->mov, ch->track, &tk_template, &tk_template_size);
		if (e == GF_OK) {
			gf_filter_pid_set_property(ch->pid, GF_PROP_PID_ISOM_TREX_TEMPLATE, &PROP_DATA_NO_COPY(tk_template, tk_template_size) );
		}

		e = gf_isom_get_raw_user_data(read->mov, &tk_template, &tk_template_size);
		if (e==GF_OK) {
			if (tk_template_size)
				gf_filter_pid_set_property(ch->pid, GF_PROP_PID_ISOM_UDTA, &PROP_DATA_NO_COPY(tk_template, tk_template_size) );
		} else {
			GF_LOG(GF_LOG_WARNING, GF_LOG_CONTAINER, ("[IsoMedia] Failed to serialize moov UDTA box: %s\n", gf_error_to_string(e) ));
		}
#endif

		GF_Fraction64 moov_time;
		moov_time.num = gf_isom_get_duration(read->mov);
		moov_time.den = gf_isom_get_timescale(read->mov);
		gf_filter_pid_set_property(ch->pid, GF_PROP_PID_ISOM_MOVIE_TIME, &PROP_FRAC64(moov_time) );


		u32 i;
		s32 tx, ty;
		s16 l;
		gf_isom_get_track_layout_info(read->mov, ch->track, &w, &h, &tx, &ty, &l);
		if (w && h) {
			gf_filter_pid_set_property(ch->pid, GF_PROP_PID_WIDTH, &PROP_UINT(w) );
			gf_filter_pid_set_property(ch->pid, GF_PROP_PID_HEIGHT, &PROP_UINT(h) );
			gf_filter_pid_set_property(ch->pid, GF_PROP_PID_TRANS_X, &PROP_SINT(tx) );
			gf_filter_pid_set_property(ch->pid, GF_PROP_PID_TRANS_Y, &PROP_SINT(ty) );
			gf_filter_pid_set_property(ch->pid, GF_PROP_PID_ZORDER, &PROP_SINT(l) );
		}
		if (codec_id==GF_CODECID_TX3G) {
			u32 m_w = w;
			u32 m_h = h;
			for (i=0; i<gf_isom_get_track_count(read->mov); i++) {
				switch (gf_isom_get_media_type(read->mov, i+1)) {
				case GF_ISOM_MEDIA_SCENE:
				case GF_ISOM_MEDIA_VISUAL:
				case GF_ISOM_MEDIA_AUXV:
				case GF_ISOM_MEDIA_PICT:
					gf_isom_get_track_layout_info(read->mov, i+1, &w, &h, &tx, &ty, &l);
					if (w>m_w) m_w = w;
					if (h>m_h) m_h = h;
					break;
				default:
					break;
				}
			}
			gf_filter_pid_set_property(ch->pid, GF_PROP_PID_WIDTH_MAX, &PROP_UINT(m_w) );
			gf_filter_pid_set_property(ch->pid, GF_PROP_PID_HEIGHT_MAX, &PROP_UINT(m_h) );
			char *tx3g_config_sdp = NULL;
			for (i=0; i<gf_isom_get_sample_description_count(read->mov, ch->track); i++) {
				u8 *tx3g;
				u32 l1;
				u32 tx3g_len, len;
				e = gf_isom_text_get_encoded_tx3g(read->mov, ch->track, i+1, GF_RTP_TX3G_SIDX_OFFSET, &tx3g, &tx3g_len);
				if (e==GF_OK) {
					char buffer[2000];
					len = gf_base64_encode(tx3g, tx3g_len, buffer, 2000);
					gf_free(tx3g);
					buffer[len] = 0;

					l1 = tx3g_config_sdp ? (u32) strlen(tx3g_config_sdp) : 0;
					tx3g_config_sdp = gf_realloc(tx3g_config_sdp, len+3+l1);
					tx3g_config_sdp[l1] = 0;
					if (i) strcat(tx3g_config_sdp, ", ");
					strcat(tx3g_config_sdp, buffer);
				}
			}
			if (tx3g_config_sdp) {
				gf_filter_pid_set_property(ch->pid, GF_PROP_PID_DECODER_CONFIG_ENHANCEMENT, &PROP_STRING_NO_COPY(tx3g_config_sdp) );
			}
		}

		u32 idx=0;
		while (1) {
			u32 data_len, int_val2, flags;
			u64 int_val;
			const char *name;
			const u8 *data;
			GF_ISOiTunesTag itag;
			u32 itype = 0;
			s32 tag_idx;

			e = gf_isom_apple_enum_tag(read->mov, idx, &itag, &data, &data_len, &int_val, &int_val2, &flags);
			if (e) break;
			idx++;

			//do not expose tool
			if (!gf_sys_is_test_mode() && (itag == GF_ISOM_ITUNE_TOOL))
				continue;

			tag_idx = gf_itags_find_by_itag(itag);
			if (tag_idx>=0)
				itype = gf_itags_get_type(tag_idx);

			name = gf_itags_get_name(tag_idx);
			switch (itype) {
			case GF_ITAG_BOOL:
				gf_filter_pid_set_property_str(ch->pid, name, &PROP_BOOL((Bool) int_val ) );
				break;
			case GF_ITAG_INT8:
			case GF_ITAG_INT16:
			case GF_ITAG_INT32:
				gf_filter_pid_set_property_str(ch->pid, name, &PROP_UINT((u32) int_val ) );
				break;
			case GF_ITAG_INT64:
				gf_filter_pid_set_property_str(ch->pid, name, &PROP_LONGUINT(int_val) );
				break;
			case GF_ITAG_FRAC8:
			case GF_ITAG_FRAC6:
				gf_filter_pid_set_property_str(ch->pid, name, &PROP_FRAC_INT((s32) int_val, int_val2)  );
				break;
			case GF_ITAG_FILE:
				if (data && data_len)
					gf_filter_pid_set_property_str(ch->pid, name, &PROP_DATA((u8 *)data, data_len)  );
				break;
			default:
				if (data && data_len) {
					if (gf_utf8_is_legal(data, data_len))
						gf_filter_pid_set_property_str(ch->pid, name, &PROP_STRING(data) );
					else
						gf_filter_pid_set_property_str(ch->pid, name, &PROP_DATA((u8 *)data, data_len)  );
				}
				break;
			}
		}

		if (codec_id==GF_CODECID_TMCD) {
			u32 tmcd_flags=0, tmcd_fps_num=0, tmcd_fps_den=0, tmcd_fpt=0;
			gf_isom_get_tmcd_config(read->mov, track, stsd_idx, &tmcd_flags, &tmcd_fps_num, &tmcd_fps_den, &tmcd_fpt);
			gf_filter_pid_set_property_str(ch->pid, "tmcd:flags", &PROP_UINT(tmcd_flags) );
			gf_filter_pid_set_property_str(ch->pid, "tmcd:framerate", &PROP_FRAC_INT(tmcd_fps_num, tmcd_fps_den) );
			gf_filter_pid_set_property_str(ch->pid, "tmcd:frames_per_tick", &PROP_UINT(tmcd_fpt) );

		}

		if (gf_sys_old_arch_compat()) {
			Bool gf_isom_has_time_offset_table(GF_ISOFile *the_file, u32 trackNumber);
			if (gf_isom_has_time_offset_table(read->mov, ch->track))
				gf_filter_pid_set_property_str(ch->pid, "isom_force_ctts", &PROP_BOOL(GF_TRUE) );
		}

		if (!gf_sys_is_test_mode()) {
			u32 nb_udta, alt_grp=0;
			const char *hdlr = NULL;
			gf_isom_get_handler_name(read->mov, ch->track, &hdlr);
			if (hdlr)
				gf_filter_pid_set_property(ch->pid, GF_PROP_PID_ISOM_HANDLER, &PROP_STRING(hdlr));

			gf_filter_pid_set_property(ch->pid, GF_PROP_PID_ISOM_TRACK_FLAGS, &PROP_UINT( gf_isom_get_track_flags(read->mov, ch->track) ));

			gf_filter_pid_set_property(ch->pid, GF_PROP_PID_ISOM_TRACK_FLAGS, &PROP_UINT( gf_isom_get_track_flags(read->mov, ch->track) ));

			gf_isom_get_track_switch_group_count(read->mov, ch->track, &alt_grp, NULL);
			if (alt_grp)
				gf_filter_pid_set_property(ch->pid, GF_PROP_PID_ISOM_ALT_GROUP, &PROP_UINT( alt_grp ));


			if (streamtype==GF_STREAM_VISUAL) {
				GF_PropertyValue p;
				u32 vals[9];
				memset(vals, 0, sizeof(u32)*9);
				memset(&p, 0, sizeof(GF_PropertyValue));
				p.type = GF_PROP_SINT_LIST;
				p.value.uint_list.nb_items = 9;
				p.value.uint_list.vals = vals;
				gf_isom_get_track_matrix(read->mov, ch->track, vals);
				gf_filter_pid_set_property(ch->pid, GF_PROP_PID_ISOM_TRACK_MATRIX, &p);
			}


			nb_udta =  gf_isom_get_udta_count(read->mov, ch->track);
			if (nb_udta) {
				for (i=0; i<nb_udta; i++) {
					u32 j, type, nb_items;
					bin128 uuid;
					gf_isom_get_udta_type(read->mov, ch->track, i+1, &type, &uuid);
					nb_items = gf_isom_get_user_data_count(read->mov, ch->track, type, uuid);
					//we only export 4CC udta boxes
					if (!type) continue;

					for (j=0; j<nb_items; j++) {
						char szName[31];
						u8 *udta=NULL;
						u32 udta_size;
						gf_isom_get_user_data(read->mov, ch->track, type, uuid, j+1, &udta, &udta_size);
						if (!udta || !udta_size) continue;
						if (nb_items>1)
							snprintf(szName, 30, "udta_%s_%d", gf_4cc_to_str(type), j+1);
						else
							snprintf(szName, 30, "udta_%s", gf_4cc_to_str(type));
						szName[30]=0;
						if (gf_utf8_is_legal(udta, udta_size)) {
							gf_filter_pid_set_property_dyn(ch->pid, szName, &PROP_STRING_NO_COPY(udta));
						} else {
							gf_filter_pid_set_property_dyn(ch->pid, szName, &PROP_DATA_NO_COPY(udta, udta_size));
						}
					}
				}
			}

			//delcare track groups
			u32 idx=0;
			while (1) {
				char szTK[100];
				u32 track_group_type, track_group_id;
				if (!gf_isom_enum_track_group(read->mov, ch->track, &idx, &track_group_type, &track_group_id)) break;
				sprintf(szTK, "tkgp_%s", gf_4cc_to_str(track_group_type));
				gf_filter_pid_set_property_dyn(ch->pid, szTK, &PROP_SINT(track_group_id));
			}
		}

props_done:

		if (read->sigfrag) {
#ifndef GPAC_DISABLE_ISOM_FRAGMENTS
			u64 start, end;
			if (gf_isom_get_root_sidx_offsets(read->mov, &start, &end)) {
				if (end)
					gf_filter_pid_set_property(ch->pid, GF_PROP_PCK_SIDX_RANGE, &PROP_FRAC64_INT(start , end));
			}
#endif
			if (!read->frag_type) {
				gf_filter_pid_set_property_str(ch->pid, "nofrag", &PROP_BOOL(GF_TRUE));
			}
		}
	}

	//update decoder configs
	ch->check_avc_ps = ch->check_hevc_ps = ch->check_vvc_ps = 0;
	if (ch->avcc) gf_odf_avc_cfg_del(ch->avcc);
	ch->avcc = NULL;
	if (ch->hvcc) gf_odf_hevc_cfg_del(ch->hvcc);
	ch->hvcc = NULL;
	if (ch->vvcc) gf_odf_vvc_cfg_del(ch->vvcc);
	ch->vvcc = NULL;

	if (lang_desc) {
		gf_odf_desc_del((GF_Descriptor *)lang_desc);
		lang_desc = NULL;
	}
	
	if (read->smode != MP4DMX_SINGLE) {
		if ((codec_id==GF_CODECID_LHVC) || (codec_id==GF_CODECID_HEVC)) {
			Bool signal_lhv = (read->smode==MP4DMX_SPLIT) ? GF_TRUE : GF_FALSE;
			GF_HEVCConfig *hvcc = gf_isom_hevc_config_get(read->mov, track, stsd_idx);
			GF_HEVCConfig *lhcc = gf_isom_lhvc_config_get(read->mov, track, stsd_idx);

			if (hvcc || lhcc) {
				if (dsi) gf_free(dsi);
				dsi = NULL;
				//no base layer config
				if (!hvcc) signal_lhv = GF_TRUE;

				if (signal_lhv && lhcc) {
					if (hvcc) {
						hvcc->is_lhvc = GF_FALSE;
						gf_odf_hevc_cfg_write(hvcc, &dsi, &dsi_size);
					}
					lhcc->is_lhvc = GF_TRUE;
					gf_odf_hevc_cfg_write(lhcc, &enh_dsi, &enh_dsi_size);
					codec_id = GF_CODECID_LHVC;
				} else {
					if (hvcc) {
						hvcc->is_lhvc = GF_FALSE;
						gf_odf_hevc_cfg_write(hvcc, &dsi, &dsi_size);
					}
					if (lhcc) {
						lhcc->is_lhvc = GF_TRUE;
						gf_odf_hevc_cfg_write(lhcc, &enh_dsi, &enh_dsi_size);
					}
					codec_id = GF_CODECID_HEVC;
				}
			}
			if (hvcc) gf_odf_hevc_cfg_del(hvcc);
			if (lhcc) gf_odf_hevc_cfg_del(lhcc);
		}
		if ((codec_id==GF_CODECID_AVC) || (codec_id==GF_CODECID_SVC) || (codec_id==GF_CODECID_MVC)) {
			Bool is_mvc = GF_FALSE;
			Bool signal_svc = (read->smode==MP4DMX_SPLIT) ? GF_TRUE : GF_FALSE;
			GF_AVCConfig *avcc = gf_isom_avc_config_get(read->mov, track, stsd_idx);
			GF_AVCConfig *svcc = gf_isom_svc_config_get(read->mov, track, stsd_idx);
			if (!svcc) {
				svcc = gf_isom_mvc_config_get(read->mov, track, stsd_idx);
				is_mvc = GF_TRUE;
			}

			if (avcc || svcc) {
				if (dsi) gf_free(dsi);
				dsi = NULL;
				//no base layer config
				if (!avcc) signal_svc = GF_TRUE;

				if (signal_svc && svcc) {
					if (avcc) {
						gf_odf_avc_cfg_write(avcc, &dsi, &dsi_size);
					}
					gf_odf_avc_cfg_write(svcc, &enh_dsi, &enh_dsi_size);
					codec_id = is_mvc ? GF_CODECID_MVC : GF_CODECID_SVC;
				} else {
					if (avcc) {
						gf_odf_avc_cfg_write(avcc, &dsi, &dsi_size);
					}
					if (svcc) {
						gf_odf_avc_cfg_write(svcc, &enh_dsi, &enh_dsi_size);
					}
					codec_id = GF_CODECID_AVC;
				}
			}
			if (avcc) gf_odf_avc_cfg_del(avcc);
			if (svcc) gf_odf_avc_cfg_del(svcc);
		}

		if (!gf_sys_is_test_mode()) {
			u64 create_date, modif_date;
			gf_isom_get_creation_time(read->mov, &create_date, &modif_date);
			gf_filter_pid_set_property_str(ch->pid, "isom:creation_date", &PROP_LONGUINT(create_date));
			gf_filter_pid_set_property_str(ch->pid, "isom:modification_date", &PROP_LONGUINT(modif_date));
		}

		isor_get_chapters(read->mov, ch->pid);

		if (!gf_sys_is_test_mode()) {
			Bool has_roll=GF_FALSE;
			gf_isom_has_cenc_sample_group(read->mov, track, NULL, &has_roll);
			if (has_roll)
				gf_filter_pid_set_property(ch->pid, GF_PROP_PID_CENC_HAS_ROLL, &PROP_BOOL(GF_TRUE));
		}
	}

	//all stsd specific init/reconfig
	gf_filter_pid_set_property(ch->pid, GF_PROP_PID_CODECID, &PROP_UINT(codec_id));

	if (meta_codec_name || meta_codec_id) {
		if (meta_codec_id)
			gf_filter_pid_set_property(ch->pid, GF_PROP_PID_META_DEMUX_CODEC_ID, &PROP_UINT(meta_codec_id));

		if (meta_codec_name) {
			gf_filter_pid_set_property(ch->pid, GF_PROP_PID_META_DEMUX_CODEC_NAME, &PROP_STRING(meta_codec_name ) );
			gf_free(meta_codec_name);
		}
		if (meta_opaque)
			gf_filter_pid_set_property(ch->pid, GF_PROP_PID_META_DEMUX_OPAQUE, &PROP_UINT(meta_opaque));

		if (dsi) {
			ch->dsi_crc = gf_crc_32(dsi, dsi_size);
			gf_filter_pid_set_property(ch->pid, GF_PROP_PID_DECODER_CONFIG, &PROP_DATA(dsi, dsi_size)); //copy
			dsi = NULL; //do not free it
		}
		m_subtype = 0;
		if (udesc) {
			if (udesc->extension_buf) gf_free(udesc->extension_buf);
			gf_free(udesc);
			udesc = NULL;
		}
	}

	if (dsi) {
		ch->dsi_crc = gf_crc_32(dsi, dsi_size);
		//strip box header for these codecs
		if (codec_id==GF_CODECID_SMPTE_VC1) {
			gf_filter_pid_set_property(ch->pid, GF_PROP_PID_DECODER_CONFIG, &PROP_DATA(dsi+8, dsi_size-8));
			gf_free(dsi);
			dsi=NULL;
		} else {
			gf_filter_pid_set_property(ch->pid, GF_PROP_PID_DECODER_CONFIG, &PROP_DATA_NO_COPY(dsi, dsi_size));
		}
	}
	if (enh_dsi) {
		gf_filter_pid_set_property(ch->pid, GF_PROP_PID_DECODER_CONFIG_ENHANCEMENT, &PROP_DATA_NO_COPY(enh_dsi, enh_dsi_size));
	}
	if (audio_fmt) {
		gf_filter_pid_set_property(ch->pid, GF_PROP_PID_AUDIO_FORMAT, &PROP_UINT(audio_fmt));
		if (codec_id == GF_CODECID_RAW) {
			gf_isom_enable_raw_pack(read->mov, track, read->frame_size);
		}
	}
	if (pix_fmt) {
		gf_filter_pid_set_property(ch->pid, GF_PROP_PID_PIXFMT, &PROP_UINT(pix_fmt));
	}

	gf_filter_pid_set_property(ch->pid, GF_PROP_PID_CONFIG_IDX, &PROP_UINT(stsd_idx) );

	w = h = 0;
	gf_isom_get_visual_info(read->mov, track, stsd_idx, &w, &h);
	if (w && h) {
		GF_ISOM_Y3D_Info yt3d;
		u32 hspace, vspace;
		gf_filter_pid_set_property(ch->pid, GF_PROP_PID_WIDTH, &PROP_UINT(w));
		gf_filter_pid_set_property(ch->pid, GF_PROP_PID_HEIGHT, &PROP_UINT(h));

		gf_isom_get_pixel_aspect_ratio(read->mov, track, stsd_idx, &hspace, &vspace);
		if (hspace != vspace)
			gf_filter_pid_set_property(ch->pid, GF_PROP_PID_SAR, &PROP_FRAC_INT(hspace, vspace) );

		{
			const u8 *icc;
			u32 icc_size;
			u32 colour_type;
			u16 colour_primaries, transfer_characteristics, matrix_coefficients;
			Bool full_range_flag;
			if (GF_OK == gf_isom_get_color_info(read->mov, track, stsd_idx,
				&colour_type, &colour_primaries, &transfer_characteristics, &matrix_coefficients, &full_range_flag)) {
				gf_filter_pid_set_property(ch->pid, GF_PROP_PID_COLR_PRIMARIES, &PROP_UINT(colour_primaries));
				gf_filter_pid_set_property(ch->pid, GF_PROP_PID_COLR_TRANSFER, &PROP_UINT(transfer_characteristics));
				gf_filter_pid_set_property(ch->pid, GF_PROP_PID_COLR_MX, &PROP_UINT(matrix_coefficients));
				gf_filter_pid_set_property(ch->pid, GF_PROP_PID_COLR_RANGE, &PROP_BOOL(full_range_flag));
			}
			if (gf_isom_get_icc_profile(read->mov, track, stsd_idx, NULL, &icc, &icc_size)==GF_OK) {
				if (icc && icc_size)
					gf_filter_pid_set_property(ch->pid, GF_PROP_PID_ICC_PROFILE, &PROP_DATA((u8*)icc, icc_size) );
			}
		}

		e = gf_isom_get_y3d_info(ch->owner->mov, ch->track, stsd_idx, &yt3d);
		if (e==GF_OK) {
			if (yt3d.stereo_type) {
				gf_filter_pid_set_property(ch->pid, GF_PROP_PID_STEREO_TYPE, &PROP_UINT(yt3d.stereo_type));
			} else {
				gf_filter_pid_set_property(ch->pid, GF_PROP_PID_STEREO_TYPE, NULL);
			}

			if (yt3d.projection_type) {
				gf_filter_pid_set_property(ch->pid, GF_PROP_PID_PROJECTION_TYPE, &PROP_UINT(yt3d.projection_type));
				if (yt3d.projection_type==GF_PROJ360_CUBE_MAP) {
					gf_filter_pid_set_property(ch->pid, GF_PROP_PID_CUBE_MAP_PAD, yt3d.padding ? &PROP_UINT(yt3d.padding) : NULL);
				}
				else if (yt3d.projection_type==GF_PROJ360_EQR) {
					if (yt3d.top || yt3d.bottom || yt3d.left || yt3d.right)
						gf_filter_pid_set_property(ch->pid, GF_PROP_PID_EQR_CLAMP, &PROP_VEC4I_INT(yt3d.top, yt3d.bottom , yt3d.left , yt3d.right));
					else
						gf_filter_pid_set_property(ch->pid, GF_PROP_PID_EQR_CLAMP, NULL);
				}
			} else {
				gf_filter_pid_set_property(ch->pid, GF_PROP_PID_PROJECTION_TYPE, NULL);
			}
			if (yt3d.pose_present) {
				gf_filter_pid_set_property(ch->pid, GF_PROP_PID_VR_POSE, &PROP_VEC3I_INT(yt3d.yaw, yt3d.pitch, yt3d.roll) );
			} else {
				gf_filter_pid_set_property(ch->pid, GF_PROP_PID_VR_POSE, NULL);
			}
		}
	}
	if (m_subtype!=GF_QT_SUBTYPE_LPCM) {
		sr = nb_ch = nb_bps = 0;
		gf_isom_get_audio_info(read->mov,track, stsd_idx, &sr, &nb_ch, &nb_bps);
	}
	if (streamtype==GF_STREAM_AUDIO) {
		if (!sr) sr = gf_isom_get_media_timescale(read->mov, track);
	}
	//nb_ch may be set to 0 for "not applicable" (3D / object coding audio)
	if (sr) {
		u32 d1, d2;
		gf_filter_pid_set_property(ch->pid, GF_PROP_PID_SAMPLE_RATE, &PROP_UINT(sr));
		gf_filter_pid_set_property(ch->pid, GF_PROP_PID_NUM_CHANNELS, &PROP_UINT(nb_ch));

		//to remove once we deprecate master
		if (!gf_sys_old_arch_compat()) {
			GF_AudioChannelLayout layout;
			gf_filter_pid_set_property(ch->pid, GF_PROP_PID_AUDIO_BPS, &PROP_UINT(nb_bps));

			if (gf_isom_get_audio_layout(read->mov, track, stsd_idx, &layout)==GF_OK) {
				if (layout.definedLayout) {
					u64 lay = gf_audio_fmt_get_layout_from_cicp(layout.definedLayout);
					gf_filter_pid_set_property(ch->pid, GF_PROP_PID_CHANNEL_LAYOUT, &PROP_LONGUINT(lay));
				}

			}

		}

		if (first_config ) {
			d1 = gf_isom_get_sample_duration(read->mov, ch->track, 1);
			d2 = gf_isom_get_sample_duration(read->mov, ch->track, 2);
			if (d1 && d2 && (d1==d2)) {
				d1 *= sr;
				d1 /= ch->timescale;
				gf_filter_pid_set_property(ch->pid, GF_PROP_PID_SAMPLES_PER_FRAME, &PROP_UINT(d1));
			}
		}

		if ((codec_id==GF_CODECID_MPHA) || (codec_id==GF_CODECID_MHAS)) {
			u32 nb_profiles;
			const u8 *prof_compat = gf_isom_get_mpegh_compatible_profiles(read->mov, ch->track, stsd_idx, &nb_profiles);
			if (prof_compat) {
				u32 j;
				GF_PropertyValue prop;
				prop.type = GF_PROP_UINT_LIST;
				prop.value.uint_list.nb_items = nb_profiles;
				prop.value.uint_list.vals = gf_malloc(sizeof(u32)*nb_profiles);
				for (j=0; j<nb_profiles; j++)
					prop.value.uint_list.vals[j] = prof_compat[j];
				gf_filter_pid_set_property(ch->pid, GF_PROP_PID_MHA_COMPATIBLE_PROFILES, &prop);
				gf_free(prop.value.uint_list.vals);
			}
		}
	}


	gf_isom_get_bitrate(read->mov, ch->track, stsd_idx, &avg_rate, &max_rate, &buffer_size);

	if (!avg_rate) {
		if (first_config && ch->duration) {
			u64 avgrate = 8 * gf_isom_get_media_data_size(read->mov, ch->track);
			avgrate = (u64) (avgrate / track_dur);
			gf_filter_pid_set_property(ch->pid, GF_PROP_PID_BITRATE, &PROP_UINT((u32) avgrate));
		}
	} else {
		gf_filter_pid_set_property(ch->pid, GF_PROP_PID_BITRATE, &PROP_UINT((u32) avg_rate));
		gf_filter_pid_set_property(ch->pid, GF_PROP_PID_MAXRATE, &PROP_UINT((u32) max_rate));
		gf_filter_pid_set_property(ch->pid, GF_PROP_PID_DBSIZE, &PROP_UINT((u32) buffer_size));
	}

	if (mime) gf_filter_pid_set_property_str(ch->pid, "meta:mime", &PROP_STRING(mime) );
	if (encoding) gf_filter_pid_set_property_str(ch->pid, "meta:encoding", &PROP_STRING(encoding) );
	if (namespace) gf_filter_pid_set_property_str(ch->pid, "meta:xmlns", &PROP_STRING(namespace) );
	if (schemaloc) gf_filter_pid_set_property_str(ch->pid, "meta:schemaloc", &PROP_STRING(schemaloc) );
	if (mime_cfg) gf_filter_pid_set_property_str(ch->pid, "meta:mime", &PROP_STRING(mime_cfg) );
	else if ((m_subtype==GF_ISOM_SUBTYPE_STPP) && namespace && strstr(namespace, "ns/ttml")) {
		mime_cfg = "application/ttml+xml;codecs=im1t";
		if (gf_isom_sample_has_subsamples(read->mov, track, 0, 0) )
			mime_cfg = "application/ttml+xml;codecs=im1i";
		gf_filter_pid_set_property_str(ch->pid, "meta:mime", &PROP_STRING(mime_cfg) );
	}

	if (!gf_sys_is_test_mode() && (m_subtype==GF_ISOM_SUBTYPE_MPEG4))
		m_subtype = gf_isom_get_mpeg4_subtype(read->mov, ch->track, stsd_idx);

	if (m_subtype)
		gf_filter_pid_set_property(ch->pid, GF_PROP_PID_ISOM_SUBTYPE, &PROP_4CC(m_subtype) );

	if (stxtcfg) gf_filter_pid_set_property(ch->pid, GF_PROP_PID_DECODER_CONFIG, &PROP_DATA((char *)stxtcfg, (u32) strlen(stxtcfg) ));


#if !defined(GPAC_DISABLE_ISOM_WRITE)
	tk_template=NULL;
	tk_template_size=0;
	e = gf_isom_get_stsd_template(read->mov, ch->track, stsd_idx, &tk_template, &tk_template_size);
	if (e == GF_OK) {
		gf_filter_pid_set_property(ch->pid, GF_PROP_PID_ISOM_STSD_TEMPLATE, &PROP_DATA_NO_COPY(tk_template, tk_template_size) );

		if (!gf_sys_old_arch_compat()) {
			//if more than one sample desc, export all of them
			if (gf_isom_get_sample_description_count(read->mov, ch->track)>1) {
				tk_template=NULL;
				tk_template_size=0;
				e = gf_isom_get_stsd_template(read->mov, ch->track, 0, &tk_template, &tk_template_size);
				if (e == GF_OK) {
					gf_filter_pid_set_property(ch->pid, GF_PROP_PID_ISOM_STSD_ALL_TEMPLATES, &PROP_DATA_NO_COPY(tk_template, tk_template_size) );

					gf_filter_pid_set_property(ch->pid, GF_PROP_PID_ISOM_STSD_TEMPLATE_IDX, &PROP_UINT(stsd_idx) );
				}
			}
		}
	} else {
		GF_LOG(GF_LOG_WARNING, GF_LOG_CONTAINER, ("[IsoMedia] Failed to serialize stsd box: %s\n", gf_error_to_string(e) ));
	}
#endif

	if (codec_id == GF_CODECID_DIMS) {
		GF_DIMSDescription dims;
		memset(&dims, 0, sizeof(GF_DIMSDescription));

		gf_isom_get_dims_description(read->mov, ch->track, stsd_idx, &dims);
		gf_filter_pid_set_property_str(ch->pid, "dims:profile", &PROP_UINT(dims.profile));
		gf_filter_pid_set_property_str(ch->pid, "dims:level", &PROP_UINT(dims.level));
		gf_filter_pid_set_property_str(ch->pid, "dims:pathComponents", &PROP_UINT(dims.pathComponents));
		gf_filter_pid_set_property_str(ch->pid, "dims:fullRequestHost", &PROP_BOOL(dims.fullRequestHost));
		gf_filter_pid_set_property_str(ch->pid, "dims:streamType", &PROP_BOOL(dims.streamType));
		gf_filter_pid_set_property_str(ch->pid, "dims:redundant", &PROP_BOOL(dims.containsRedundant));
		if (dims.content_script_types)
			gf_filter_pid_set_property_str(ch->pid, "dims:scriptTypes", &PROP_STRING(dims.content_script_types));
		if (dims.textEncoding)
			gf_filter_pid_set_property_str(ch->pid, "meta:encoding", &PROP_STRING(dims.textEncoding));
		if (dims.contentEncoding)
			gf_filter_pid_set_property_str(ch->pid, "meta:content_encoding", &PROP_STRING(dims.contentEncoding));
		if (dims.xml_schema_loc)
			gf_filter_pid_set_property_str(ch->pid, "meta:schemaloc", &PROP_STRING(dims.xml_schema_loc));
		if (dims.mime_type)
			gf_filter_pid_set_property_str(ch->pid, "meta:mime", &PROP_STRING(dims.mime_type));
	}
	else if (codec_id==GF_CODECID_AVC)
		ch->check_avc_ps = (ch->owner->xps_check==MP4DMX_XPS_REMOVE) ? 1 : 0;
	else if (codec_id==GF_CODECID_HEVC)
		ch->check_hevc_ps = (ch->owner->xps_check==MP4DMX_XPS_REMOVE) ? 1 : 0;
	else if (codec_id==GF_CODECID_VVC)
		ch->check_vvc_ps = (ch->owner->xps_check==MP4DMX_XPS_REMOVE) ? 1 : 0;
	else if (codec_id==GF_CODECID_MHAS) {
		if (!dsi) {
			ch->check_mhas_pl = 1;
#ifndef GPAC_DISABLE_AV_PARSERS
			GF_ISOSample *samp = gf_isom_get_sample(ch->owner->mov, ch->track, 1, NULL);
			if (samp) {
				u64 ch_layout=0;
				s32 PL = gf_mpegh_get_mhas_pl(samp->data, samp->dataLength, &ch_layout);
				if (PL>0) {
					gf_filter_pid_set_property(ch->pid, GF_PROP_PID_PROFILE_LEVEL, &PROP_UINT((u32) PL));
					ch->check_mhas_pl = 0;
					if (ch_layout)
						gf_filter_pid_set_property(ch->pid, GF_PROP_PID_CHANNEL_LAYOUT, &PROP_LONGUINT(ch_layout));
				}
				gf_isom_sample_del(&samp);
			}
#endif
		}
	}

	if (udesc) {
		gf_filter_pid_set_property_str(ch->pid, "codec_vendor", &PROP_UINT(udesc->vendor_code));
		gf_filter_pid_set_property_str(ch->pid, "codec_version", &PROP_UINT(udesc->version));
		gf_filter_pid_set_property_str(ch->pid, "codec_revision", &PROP_UINT(udesc->revision));
		gf_filter_pid_set_property_str(ch->pid, "compressor_name", &PROP_STRING(udesc->compressor_name));
		if (udesc->temporal_quality)
			gf_filter_pid_set_property_str(ch->pid, "temporal_quality", &PROP_UINT(udesc->temporal_quality));

		if (udesc->spatial_quality)
			gf_filter_pid_set_property_str(ch->pid, "spatial_quality", &PROP_UINT(udesc->spatial_quality));

		if (udesc->h_res) {
			gf_filter_pid_set_property_str(ch->pid, "hres", &PROP_UINT(udesc->h_res));
			gf_filter_pid_set_property_str(ch->pid, "vres", &PROP_UINT(udesc->v_res));
		} else if (udesc->nb_channels) {
			gf_filter_pid_set_property(ch->pid, GF_PROP_PID_NUM_CHANNELS, &PROP_UINT(udesc->nb_channels));
			switch (udesc->bits_per_sample) {
			case 8:
				gf_filter_pid_set_property(ch->pid, GF_PROP_PID_AUDIO_FORMAT, &PROP_UINT(GF_AUDIO_FMT_U8));
				break;
			case 16:
				gf_filter_pid_set_property(ch->pid, GF_PROP_PID_AUDIO_FORMAT, &PROP_UINT(GF_AUDIO_FMT_S16));
				break;
			case 24:
				gf_filter_pid_set_property(ch->pid, GF_PROP_PID_AUDIO_FORMAT, &PROP_UINT(GF_AUDIO_FMT_S24));
				break;
			case 32:
				gf_filter_pid_set_property(ch->pid, GF_PROP_PID_AUDIO_FORMAT, &PROP_UINT(GF_AUDIO_FMT_S32));
				break;
			default:
				GF_LOG(GF_LOG_WARNING, GF_LOG_CONTAINER, ("[IsoMedia] Track %d unsupported audio bit depth %d\n", track, udesc->bits_per_sample ));
				break;
			}
		}
		if (udesc->depth)
			gf_filter_pid_set_property(ch->pid, GF_PROP_PID_BIT_DEPTH_Y, &PROP_UINT(udesc->depth));

		gf_free(udesc);
	}


	if (ch->check_avc_ps) {
		ch->avcc = gf_isom_avc_config_get(ch->owner->mov, ch->track, ch->last_sample_desc_index ? ch->last_sample_desc_index : 1);
	}
	else if (ch->check_hevc_ps) {
		ch->hvcc = gf_isom_hevc_config_get(ch->owner->mov, ch->track, ch->last_sample_desc_index ? ch->last_sample_desc_index : 1);
	}
	else if (ch->check_vvc_ps) {
		ch->vvcc = gf_isom_vvc_config_get(ch->owner->mov, ch->track, ch->last_sample_desc_index ? ch->last_sample_desc_index : 1);
	}

	if (streamtype==GF_STREAM_VISUAL) {
		u32 cwn, cwd, chn, chd, cxn, cxd, cyn, cyd;
		gf_isom_get_clean_aperture(ch->owner->mov, ch->track, ch->last_sample_desc_index ? ch->last_sample_desc_index : 1, &cwn, &cwd, &chn, &chd, &cxn, &cxd, &cyn, &cyd);

		if (cwd && chd && cxd && cyd) {
			gf_filter_pid_set_property(ch->pid, GF_PROP_PID_CLAP_W, &PROP_FRAC_INT(cwn, cwd) );
			gf_filter_pid_set_property(ch->pid, GF_PROP_PID_CLAP_H, &PROP_FRAC_INT(chn, chd) );
			gf_filter_pid_set_property(ch->pid, GF_PROP_PID_CLAP_X, &PROP_FRAC_INT(cxn, cxd) );
			gf_filter_pid_set_property(ch->pid, GF_PROP_PID_CLAP_Y, &PROP_FRAC_INT(cyn, cyd) );
		} else {
			gf_filter_pid_set_property(ch->pid, GF_PROP_PID_CLAP_W, NULL);
			gf_filter_pid_set_property(ch->pid, GF_PROP_PID_CLAP_H, NULL);
			gf_filter_pid_set_property(ch->pid, GF_PROP_PID_CLAP_X, NULL);;
			gf_filter_pid_set_property(ch->pid, GF_PROP_PID_CLAP_Y, NULL);
		}

		const GF_MasteringDisplayColourVolumeInfo *mdcv = gf_isom_get_mastering_display_colour_info(ch->owner->mov, ch->track,ch->last_sample_desc_index ? ch->last_sample_desc_index : 1);
		if (mdcv) {
			u8 *pdata;
			u32 psize, c;
			GF_BitStream *bs = gf_bs_new(NULL, 0, GF_BITSTREAM_WRITE);

			for(c=0;c<3;c++) {
				gf_bs_write_u16(bs, mdcv->display_primaries[c].x);
				gf_bs_write_u16(bs, mdcv->display_primaries[c].y);
			}
			gf_bs_write_u16(bs, mdcv->white_point_x);
			gf_bs_write_u16(bs, mdcv->white_point_y);
			gf_bs_write_u32(bs, mdcv->max_display_mastering_luminance);
			gf_bs_write_u32(bs, mdcv->min_display_mastering_luminance);
			gf_bs_get_content(bs, &pdata, &psize);
			gf_bs_del(bs);
			gf_filter_pid_set_property(ch->pid, GF_PROP_PID_MASTER_DISPLAY_COLOUR, &PROP_DATA_NO_COPY(pdata, psize));
		}

		const GF_ContentLightLevelInfo *clli = gf_isom_get_content_light_level_info(ch->owner->mov, ch->track,ch->last_sample_desc_index ? ch->last_sample_desc_index : 1);
		if (clli) {
			u8 *pdata;
			u32 psize;
			GF_BitStream *bs = gf_bs_new(NULL, 0, GF_BITSTREAM_WRITE);
			gf_bs_write_u16(bs, clli->max_content_light_level);
			gf_bs_write_u16(bs, clli->max_pic_average_light_level);
			gf_bs_get_content(bs, &pdata, &psize);
			gf_bs_del(bs);
			gf_filter_pid_set_property(ch->pid, GF_PROP_PID_CONTENT_LIGHT_LEVEL, &PROP_DATA_NO_COPY(pdata, psize));
		}
	}
}

void isor_update_channel_config(ISOMChannel *ch)
{
	isor_declare_track(ch->owner, ch, ch->track, ch->last_sample_desc_index, ch->streamType, GF_FALSE);
}

GF_Err isor_declare_objects(ISOMReader *read)
{
	const u8 *tag;
	u32 tlen;
	u32 i, count, j, track_id;
	Bool highest_stream;
	Bool single_media_found = GF_FALSE;
	Bool use_iod = GF_FALSE;
	Bool tk_found = GF_FALSE;
	GF_Err e;
	Bool isom_contains_video = GF_FALSE;
	GF_Descriptor *od = gf_isom_get_root_od(read->mov);
	if (od && gf_list_count(((GF_ObjectDescriptor*)od)->ESDescriptors)) {
		use_iod = GF_TRUE;
	}
	if (od) gf_odf_desc_del(od);

	/*TODO
	 check for alternate tracks
    */
	count = gf_isom_get_track_count(read->mov);
	for (i=0; i<count; i++) {
		u32 mtype, m_subtype, streamtype, stsd_idx;

		mtype = gf_isom_get_media_type(read->mov, i+1);

		if (read->tkid) {
			u32 for_id=0;
			if (sscanf(read->tkid, "%d", &for_id)) {
				u32 id = gf_isom_get_track_id(read->mov, i+1);
				if (id != for_id) continue;
			} else if (!strcmp(read->tkid, "audio")) {
				if (mtype!=GF_ISOM_MEDIA_AUDIO) continue;
			} else if (!strcmp(read->tkid, "video")) {
				if (mtype!=GF_ISOM_MEDIA_VISUAL) continue;
			} else if (!strcmp(read->tkid, "text")) {
				if ((mtype!=GF_ISOM_MEDIA_TEXT) && (mtype!=GF_ISOM_MEDIA_SUBT) && (mtype!=GF_ISOM_MEDIA_MPEG_SUBT)) continue;
			} else if (strlen(read->tkid)==4) {
				u32 t = GF_4CC(read->tkid[0], read->tkid[1], read->tkid[2], read->tkid[3]);
				if (mtype!=t) continue;
			} else {
				GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[IsoMedia] Bad format for tkid option %s, no match\n", read->tkid));
				return GF_BAD_PARAM;
			}
			tk_found = GF_TRUE;
		}

		switch (mtype) {
		case GF_ISOM_MEDIA_AUDIO:
			streamtype = GF_STREAM_AUDIO;
			break;
		case GF_ISOM_MEDIA_VISUAL:
		case GF_ISOM_MEDIA_AUXV:
		case GF_ISOM_MEDIA_PICT:
		case GF_ISOM_MEDIA_QTVR:
			streamtype = GF_STREAM_VISUAL;
			isom_contains_video = GF_TRUE;
			break;
		case GF_ISOM_MEDIA_TEXT:
		case GF_ISOM_MEDIA_SUBT:
		case GF_ISOM_MEDIA_SUBPIC:
		case GF_ISOM_MEDIA_MPEG_SUBT:
		case GF_ISOM_MEDIA_CLOSED_CAPTION:
			streamtype = GF_STREAM_TEXT;
			mtype = GF_ISOM_MEDIA_TEXT;
			break;
		case GF_ISOM_MEDIA_FLASH:
		case GF_ISOM_MEDIA_DIMS:
		case GF_ISOM_MEDIA_SCENE:
			streamtype = GF_STREAM_SCENE;
			break;
		case GF_ISOM_MEDIA_OD:
			streamtype = GF_STREAM_OD;
			break;
		case GF_ISOM_MEDIA_META:
		case GF_ISOM_MEDIA_TIMECODE:
			streamtype = GF_STREAM_METADATA;
			break;
		/*hint tracks are never exported*/
		case GF_ISOM_MEDIA_HINT:
			continue;
		default:
			if (!read->allt) {
				GF_LOG(GF_LOG_WARNING, GF_LOG_CONTAINER, ("[IsoMedia] Track %d type %s not supported, ignoring track - you may retry by specifying allt option\n", i+1, gf_4cc_to_str(mtype) ));
				continue;
			}
			streamtype = GF_STREAM_UNKNOWN;
			break;
		}

		if (!read->alltk && !read->tkid && !gf_isom_is_track_enabled(read->mov, i+1)) {
			if (count>1) {
				GF_LOG(GF_LOG_WARNING, GF_LOG_CONTAINER, ("[IsoMedia] Track %d is disabled, ignoring track - you may retry by specifying alltk option\n", i+1));
				continue;
			} else {
				GF_LOG(GF_LOG_WARNING, GF_LOG_CONTAINER, ("[IsoMedia] Track %d is disabled but single track in file, considering it enabled\n", i+1 ));
			}
		}

		stsd_idx = read->stsd ? read->stsd : 1;
		//some subtypes are not declared as readable objects
		m_subtype = gf_isom_get_media_subtype(read->mov, i+1, stsd_idx);
		switch (m_subtype) {
		case GF_ISOM_SUBTYPE_HVT1:
			if (read->smode == MP4DMX_SINGLE)
				continue;

			break;
		default:
			break;
		}

		/*we declare only the highest video track (i.e the track we play)*/
		highest_stream = GF_TRUE;
		track_id = gf_isom_get_track_id(read->mov, i+1);
		if (read->play_only_track_id && (read->play_only_track_id != track_id)) continue;

		if (read->play_only_first_media) {
			if (read->play_only_first_media != mtype) continue;
			if (single_media_found) continue;
			single_media_found = GF_TRUE;
		}

		for (j = 0; j < count; j++) {
			if (gf_isom_has_track_reference(read->mov, j+1, GF_ISOM_REF_SCAL, track_id) > 0) {
				highest_stream = GF_FALSE;
				break;
			}
			if (gf_isom_has_track_reference(read->mov, j+1, GF_ISOM_REF_BASE, track_id) > 0) {
				highest_stream = GF_FALSE;
				break;
			}
		}

		if ((read->smode==MP4DMX_SINGLE) && (gf_isom_get_media_type(read->mov, i+1) == GF_ISOM_MEDIA_VISUAL) && !highest_stream)
			continue;


		isor_declare_track(read, NULL, i+1, stsd_idx, streamtype, use_iod);

		if (read->tkid)
			break;
	}

	if (!read->tkid) {
		/*declare image items*/
		count = gf_isom_get_meta_item_count(read->mov, GF_TRUE, 0);
		for (i=0; i<count; i++) {
			if (! isor_declare_item_properties(read, NULL, i+1))
				continue;

			if (read->itt) break;
		}
	} else {
		if (!tk_found) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[IsoMedia] TrackID %s not found in file\n", read->tkid ));
			return GF_BAD_PARAM;
		}
	}
	if (! gf_list_count(read->channels)) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[IsoMedia] No suitable tracks in file\n"));
		return GF_NOT_SUPPORTED;
	}
	
	/*if cover art, declare a video pid*/
	if (gf_isom_apple_get_tag(read->mov, GF_ISOM_ITUNE_COVER_ART, &tag, &tlen)==GF_OK) {

		/*write cover data*/
		assert(!(tlen & 0x80000000));
		tlen &= 0x7FFFFFFF;

		if (read->expart && !isom_contains_video) {
			GF_FilterPid *cover_pid=NULL;
			e = gf_filter_pid_raw_new(read->filter, NULL, NULL, NULL, NULL, (char *) tag, tlen, GF_FALSE, &cover_pid);
			if (e) {
				GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[IsoMedia] error setting up video pid for cover art: %s\n", gf_error_to_string(e) ));
			}
			if (cover_pid) {
				u8 *out_buffer;
				GF_FilterPacket *dst_pck;
				gf_filter_pid_set_property(cover_pid, GF_PROP_PID_STREAM_TYPE, &PROP_UINT(GF_STREAM_FILE) );
				gf_filter_pid_set_name(cover_pid, "CoverArt");
				dst_pck = gf_filter_pck_new_alloc(cover_pid, tlen, &out_buffer);
				if (dst_pck) {
					gf_filter_pck_set_framing(dst_pck, GF_TRUE, GF_TRUE);
					memcpy(out_buffer, tag, tlen);
					gf_filter_pck_send(dst_pck);
				}
				gf_filter_pid_set_eos(cover_pid);
			}
		}
	}
	return GF_OK;
}

Bool isor_declare_item_properties(ISOMReader *read, ISOMChannel *ch, u32 item_idx)
{
	GF_ImageItemProperties props;
	GF_FilterPid *pid;
	GF_ESD *esd;
	u32 item_id=0;
	u32 scheme_type=0, scheme_version=0, item_type;
	const char *item_name, *item_mime_type, *item_encoding;

retry:

	if (item_idx>gf_isom_get_meta_item_count(read->mov, GF_TRUE, 0))
		return GF_FALSE;

	item_name = item_mime_type = item_encoding = NULL;
	gf_isom_get_meta_item_info(read->mov, GF_TRUE, 0, item_idx, &item_id, &item_type, &scheme_type, &scheme_version, NULL, NULL, NULL, &item_name, &item_mime_type, &item_encoding);

	if (!item_id) return GF_FALSE;
	if (item_type==GF_ISOM_ITEM_TYPE_AUXI) return GF_FALSE;
	if (read->play_only_track_id && (read->play_only_track_id!=item_id)) return GF_FALSE;

	gf_isom_get_meta_image_props(read->mov, GF_TRUE, 0, item_id, &props, NULL);

	//check we support the protection scheme
	switch (scheme_type) {
	case GF_ISOM_CBC_SCHEME:
	case GF_ISOM_CENS_SCHEME:
	case GF_ISOM_CBCS_SCHEME:
	case GF_ISOM_CENC_SCHEME:
	case GF_ISOM_PIFF_SCHEME:
	case 0:
		break;
	default:
		return GF_FALSE;
	}


	esd = gf_media_map_item_esd(read->mov, item_id);
	if (!esd) {
		//unsupported item, try next
		item_idx++;
		goto retry;
	}


	//OK declare PID
	if (!ch)
		pid = gf_filter_pid_new(read->filter);
	else {
		pid = ch->pid;
		ch->item_id = item_id;
	}


	//do not override PID ID if itt is set
	if (!ch) {
		if (read->pid)
			gf_filter_pid_copy_properties(pid, read->pid);
		gf_filter_pid_set_property(pid, GF_PROP_PID_ID, &PROP_UINT(esd->ESID));
	}

	if (read->itemid)
		gf_filter_pid_set_property(pid, GF_PROP_PID_ITEM_ID, &PROP_UINT(item_id));
		
	//TODO: no support for LHEVC images
	//gf_filter_pid_set_property(pid, GF_PROP_PID_DEPENDENCY_ID, &PROP_UINT(esd->dependsOnESID));

	gf_filter_pid_set_property(pid, GF_PROP_PID_STREAM_TYPE, &PROP_UINT(esd->decoderConfig->streamType));
	gf_filter_pid_set_property(pid, GF_PROP_PID_CODECID, &PROP_UINT(esd->decoderConfig->objectTypeIndication));
	gf_filter_pid_set_property(pid, GF_PROP_PID_TIMESCALE, &PROP_UINT(1000));
	if (esd->decoderConfig->decoderSpecificInfo && esd->decoderConfig->decoderSpecificInfo->data) {
		gf_filter_pid_set_property(pid, GF_PROP_PID_DECODER_CONFIG, &PROP_DATA_NO_COPY(esd->decoderConfig->decoderSpecificInfo->data, esd->decoderConfig->decoderSpecificInfo->dataLength));

		esd->decoderConfig->decoderSpecificInfo->data=NULL;
		esd->decoderConfig->decoderSpecificInfo->dataLength=0;
	}

	if (props.width && props.height) {
		gf_filter_pid_set_property(pid, GF_PROP_PID_WIDTH, &PROP_UINT(props.width));
		gf_filter_pid_set_property(pid, GF_PROP_PID_HEIGHT, &PROP_UINT(props.height));
	}

	gf_filter_pid_set_property(pid, GF_PROP_PID_HIDDEN, props.hidden ? &PROP_BOOL(GF_TRUE) : NULL);
	gf_filter_pid_set_property(pid, GF_PROP_PID_ALPHA, props.alpha ? &PROP_BOOL(GF_TRUE) : NULL);
	gf_filter_pid_set_property(pid, GF_PROP_PID_MIRROR, props.mirror ? &PROP_UINT(props.mirror) : NULL);
	gf_filter_pid_set_property(pid, GF_PROP_PID_ROTATE, props.alpha ? &PROP_UINT(props.angle) : NULL);

	if (props.clap_wden) {
		gf_filter_pid_set_property(pid, GF_PROP_PID_CLAP_W, &PROP_FRAC_INT(props.clap_wnum,props.clap_wden) );
		gf_filter_pid_set_property(pid, GF_PROP_PID_CLAP_H, &PROP_FRAC_INT(props.clap_hnum,props.clap_hden) );
		gf_filter_pid_set_property(pid, GF_PROP_PID_CLAP_X, &PROP_FRAC_INT(props.clap_honum,props.clap_hoden) );
		gf_filter_pid_set_property(pid, GF_PROP_PID_CLAP_Y, &PROP_FRAC_INT(props.clap_vonum,props.clap_voden) );
	} else {
		gf_filter_pid_set_property(pid, GF_PROP_PID_CLAP_W, NULL);
		gf_filter_pid_set_property(pid, GF_PROP_PID_CLAP_H, NULL);
		gf_filter_pid_set_property(pid, GF_PROP_PID_CLAP_X, NULL);;
		gf_filter_pid_set_property(pid, GF_PROP_PID_CLAP_Y, NULL);
	}

	if (gf_isom_get_meta_primary_item_id(read->mov, GF_TRUE, 0) == item_id) {
		gf_filter_pid_set_property(pid, GF_PROP_PID_PRIMARY_ITEM, &PROP_BOOL(GF_TRUE));
	} else {
		gf_filter_pid_set_property(pid, GF_PROP_PID_PRIMARY_ITEM, &PROP_BOOL(GF_FALSE));
	}

	if (!gf_sys_is_test_mode() && !read->itt)
		gf_filter_pid_set_property(pid, GF_PROP_PID_ITEM_NUM, &PROP_UINT(item_idx) );

	gf_filter_pid_set_property_str(pid, "meta:mime", item_mime_type ? &PROP_STRING(item_mime_type) : NULL );
	gf_filter_pid_set_property_str(pid, "meta:name", item_name ? &PROP_STRING(item_name) : NULL );
	gf_filter_pid_set_property_str(pid, "meta:encoding", item_encoding ? &PROP_STRING(item_encoding) : NULL );

	if ((item_type == GF_ISOM_SUBTYPE_UNCV) || (item_type == GF_ISOM_ITEM_TYPE_UNCI)) {
		gf_filter_pid_set_property(pid, GF_PROP_PID_PIXFMT, &PROP_UINT(GF_PIXEL_UNCV) );
		gf_filter_pid_set_property(pid, GF_PROP_PID_CODECID, &PROP_UINT(GF_CODECID_RAW_UNCV) );
	}


	//setup cenc
	if (scheme_type) {
		gf_filter_pid_set_property(pid, GF_PROP_PID_PROTECTION_SCHEME_TYPE, &PROP_4CC(scheme_type) );
		gf_filter_pid_set_property(pid, GF_PROP_PID_PROTECTION_SCHEME_VERSION, &PROP_UINT(scheme_version) );
		gf_filter_pid_set_property(pid, GF_PROP_PID_ENCRYPTED, &PROP_BOOL(GF_TRUE) );

		gf_filter_pid_set_property(pid, GF_PROP_PID_ORIG_STREAM_TYPE, &PROP_UINT(esd->decoderConfig->streamType));
		gf_filter_pid_set_property(pid, GF_PROP_PID_STREAM_TYPE, &PROP_UINT(GF_STREAM_ENCRYPTED) );
	}

	gf_odf_desc_del((GF_Descriptor *)esd);

	if (!ch) {
		ch = isor_create_channel(read, pid, 0, item_id, GF_FALSE);
		if (ch && scheme_type) {
			ch->is_cenc = 1;
			ch->is_encrypted = 1;

			isor_declare_pssh(ch);

		}
	}
	return GF_TRUE;
}

#endif /*GPAC_DISABLE_ISOM*/




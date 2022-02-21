/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2005-2022
 *					All rights reserved
 *
 *  This file is part of GPAC / AVI demuxer filter
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
#include <gpac/thread.h>
#include <gpac/list.h>
#include <gpac/bitstream.h>

#ifndef GPAC_DISABLE_AVILIB
#include <gpac/internal/avilib.h>
#include <gpac/media_tools.h>

typedef struct
{
	GF_FilterPid *opid;
	u32 stream_num;
	Bool in_use;
	u32 aud_frame, audio_bps, nb_channels, freq;
	u64 audio_ts, seek_to_ts;
	Bool audio_done, is_aac, playing, init_play_done;
} AVIAstream;


typedef struct
{
	//opts
	GF_Fraction fps;
	Bool importer;

	GF_FilterPid *ipid;

	GF_FilterPid *v_opid;
	Bool v_in_use;
	u32 nb_frames, cur_frame, nb_frame_sent;
	u32 dummy, nvops;
	const char *src_url;
	avi_t *avi;

	Bool use_file_fps;
	GF_Fraction64 duration;
	Bool v_playing;
	Bool v_init_play_done;

	GF_List *audios;
} GF_AVIDmxCtx;


static void avidmx_setup(GF_Filter *filter, GF_AVIDmxCtx *ctx)
{
	u32 sync_id = 0;
	u32 codecid = 0;
	Bool unframed;
	u32 i, count, pfmt=0;
	GF_Fraction64 dur;
	char *comp;

	if (ctx->use_file_fps) {
		Double fps = AVI_frame_rate(ctx->avi);
		gf_media_get_video_timing(fps, &ctx->fps.num, &ctx->fps.den);
	}

	dur.den = ctx->fps.num;
	dur.num = (u64) (ctx->fps.den * AVI_video_frames(ctx->avi));

	unframed = GF_TRUE;
	comp = AVI_video_compressor(ctx->avi);
	if (!comp) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[AVIDmx] Cannot retrieve video compressor name, ignoring video stream\n"));
	}
	/*these are/should be OK*/
	else if (!stricmp(comp, "DIVX") || !stricmp(comp, "DX50")	/*DivX*/
		|| !stricmp(comp, "XVID") /*XviD*/
		|| !stricmp(comp, "3iv2") /*3ivX*/
		|| !stricmp(comp, "fvfw") /*ffmpeg*/
		|| !stricmp(comp, "NDIG") /*nero*/
		|| !stricmp(comp, "MP4V") /*!! not tested*/
		|| !stricmp(comp, "M4CC") /*Divio - not tested*/
		|| !stricmp(comp, "PVMM") /*PacketVideo - not tested*/
		|| !stricmp(comp, "SEDG") /*Samsung - not tested*/
		|| !stricmp(comp, "RMP4") /*Sigma - not tested*/
		|| !stricmp(comp, "MP43") /*not tested*/
		|| !stricmp(comp, "FMP4") /*not tested*/
		|| !stricmp(comp, "VP6F") /*not tested*/
	) {
		codecid = GF_CODECID_MPEG4_PART2;
	} else if ( !stricmp(comp, "H264") /*not tested*/
		|| !stricmp(comp, "X264") /*not tested*/
	) {
		codecid = GF_CODECID_AVC;
	} else if ( !stricmp(comp, "avc1") ) {
		codecid = GF_CODECID_AVC;
		unframed = GF_FALSE;
	} else if (!stricmp(comp, "DIV3") || !stricmp(comp, "DIV4")) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[AVIDmx] Video format %s not compliant with MPEG-4 Visual - please recompress the file first\n", comp));
	} else if (!comp[0]) {
		codecid = GF_CODECID_RAW;
		pfmt = GF_PIXEL_BGR;
	} else {
		GF_LOG(GF_LOG_WARNING, GF_LOG_CONTAINER, ("[AVIDmx] Video format %s not natively supported, signaling as is\n", comp));
		codecid = gf_4cc_parse(comp);
		unframed = GF_FALSE;
	}

	ctx->v_in_use = GF_FALSE;
	if (codecid) {
		u32 w, h;
		if (!ctx->v_opid) {
			ctx->v_opid = gf_filter_pid_new(filter);
		}
		ctx->nb_frames = (u32) AVI_video_frames(ctx->avi);
		ctx->cur_frame = 0;
		sync_id = 1;
		ctx->v_in_use = GF_TRUE;

		gf_filter_pid_set_property(ctx->v_opid, GF_PROP_PID_STREAM_TYPE, &PROP_UINT(GF_STREAM_VISUAL) );
		gf_filter_pid_set_property(ctx->v_opid, GF_PROP_PID_CODECID, &PROP_UINT(codecid) );
		gf_filter_pid_set_property(ctx->v_opid, GF_PROP_PID_TIMESCALE, &PROP_UINT(ctx->fps.num) );

		gf_filter_pid_set_property(ctx->v_opid, GF_PROP_PID_ID, &PROP_UINT( sync_id) );
		gf_filter_pid_set_property(ctx->v_opid, GF_PROP_PID_CLOCK_ID, &PROP_UINT( sync_id ) );
		gf_filter_pid_set_property(ctx->v_opid, GF_PROP_PID_FPS, &PROP_FRAC( ctx->fps ) );
		w = AVI_video_width(ctx->avi);
		h = AVI_video_height(ctx->avi);
		gf_filter_pid_set_property(ctx->v_opid, GF_PROP_PID_WIDTH, &PROP_UINT( w ) );
		gf_filter_pid_set_property(ctx->v_opid, GF_PROP_PID_HEIGHT, &PROP_UINT( h ) );
		gf_filter_pid_set_property(ctx->v_opid, GF_PROP_PID_DURATION, &PROP_FRAC64( dur ) );

		gf_filter_pid_set_property(ctx->v_opid, GF_PROP_PID_PLAYBACK_MODE, &PROP_UINT(GF_PLAYBACK_MODE_FASTFORWARD ) );

		if (pfmt) {
			u32 stride=0;
			gf_pixel_get_size_info(pfmt, w, h, NULL, &stride, NULL, NULL, NULL);
			gf_filter_pid_set_property(ctx->v_opid, GF_PROP_PID_STRIDE, &PROP_UINT( stride ) );
			gf_filter_pid_set_property(ctx->v_opid, GF_PROP_PID_PIXFMT, &PROP_UINT( pfmt ) );
		} else if (unframed) {
			gf_filter_pid_set_property(ctx->v_opid, GF_PROP_PID_UNFRAMED, &PROP_BOOL(GF_TRUE) );
			gf_filter_pid_set_property_str(ctx->v_opid, "nocts", &PROP_BOOL( GF_TRUE ) );
		} else if (ctx->avi->extradata_size && ctx->avi->extradata) {
			gf_filter_pid_set_property(ctx->v_opid, GF_PROP_PID_DECODER_CONFIG, &PROP_DATA(ctx->avi->extradata, ctx->avi->extradata_size) );

		}
	}

	unframed = GF_FALSE;
	count = AVI_audio_tracks(ctx->avi);
	for (i=0; i<count; i++) {
		u32 a_fmt, afmt=0, nb_bits;
		AVI_set_audio_track(ctx->avi, i);

		codecid = 0;
		a_fmt = AVI_audio_format(ctx->avi);
		nb_bits = AVI_audio_bits(ctx->avi);
		switch (a_fmt) {
		case WAVE_FORMAT_PCM:
		case GF_4CC('P','C','M',' '):
			codecid = GF_CODECID_RAW;
			switch (nb_bits) {
			case 8:
				afmt = GF_AUDIO_FMT_U8;
				break;
			case 16:
				afmt = GF_AUDIO_FMT_S16;
				break;
			case 24:
				afmt = GF_AUDIO_FMT_S24;
				break;
			case 32:
				afmt = GF_AUDIO_FMT_S32;
				break;
			default:
				GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[AVIDmx] Audio bit depth %d not mapped, patch welcome\n", nb_bits));
				afmt = GF_AUDIO_FMT_S16;
				break;
			}
			break;
		case WAVE_FORMAT_ADPCM:
			codecid = GF_CODECID_ADPCM;
			break;
		case WAVE_FORMAT_IBM_CVSD:
			codecid = GF_CODECID_IBM_CVSD;
			break;
		case WAVE_FORMAT_ALAW:
			codecid = GF_CODECID_ALAW;
			break;
		case WAVE_FORMAT_MULAW:
			codecid = GF_CODECID_MULAW;
			break;
		case WAVE_FORMAT_OKI_ADPCM:
			codecid = GF_CODECID_OKI_ADPCM;
			break;
		case WAVE_FORMAT_DVI_ADPCM:
			codecid = GF_CODECID_DVI_ADPCM;
			break;
		case WAVE_FORMAT_DIGISTD:
			codecid = GF_CODECID_DIGISTD;
			break;
		case WAVE_FORMAT_YAMAHA_ADPCM:
			codecid = GF_CODECID_YAMAHA_ADPCM;
			break;
		case WAVE_FORMAT_DSP_TRUESPEECH:
			codecid = GF_CODECID_DSP_TRUESPEECH;
			break;
		case WAVE_FORMAT_GSM610:
			codecid = GF_CODECID_GSM610;
			break;
		case IBM_FORMAT_MULAW:
			codecid = GF_CODECID_IBM_MULAW;
			break;
		case IBM_FORMAT_ALAW:
			codecid = GF_CODECID_IBM_ALAW;
			break;
		case IBM_FORMAT_ADPCM:
			codecid = GF_CODECID_IBM_ADPCM;
			break;
		case WAVE_FORMAT_MP3:
			codecid = GF_CODECID_MPEG_AUDIO;
			unframed = GF_TRUE;
			break;
		case WAVE_FORMAT_AAC_ADTS:
			codecid = GF_CODECID_AAC_MPEG4;
			unframed = GF_TRUE;
			break;
		case WAVE_FORMAT_AAC:
			codecid = GF_CODECID_AAC_MPEG4;
			break;
		case WAVE_FORMAT_AC3:
			codecid = GF_CODECID_AC3;
			break;
		default:
			GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[AVIDmx] Audio format %d not supported, patch welcome\n", a_fmt));
			break;
		}

		if (codecid) {
			AVIAstream *st = NULL;
			u32 brate, j, c  = gf_list_count(ctx->audios);
			for (j=0; j<c; j++) {
				st = gf_list_get(ctx->audios, j);
				if (!st->in_use) break;
				st = NULL;
			}
			if (!st) {
				GF_SAFEALLOC(st, AVIAstream);
				if (!st) continue;
				st->opid = gf_filter_pid_new(filter);
				gf_list_add(ctx->audios, st);
			}
			st->in_use = GF_TRUE;
			st->stream_num = i;
			if (!sync_id) sync_id = 2 + st->stream_num;
			st->audio_done = GF_FALSE;

			if (codecid==GF_CODECID_MPEG_AUDIO) {
				u32 cid;
				char data[8];
				AVI_set_audio_track(ctx->avi, i);
				AVI_read_audio(ctx->avi, data, 8, (int*)&cid);
				u32 hdr = GF_4CC(data[0], data[1], data[2], data[3]);
				cid = gf_mp3_object_type_indication(hdr);
				AVI_set_audio_position(ctx->avi, 0);
				if (cid) codecid = cid;
			}

			gf_filter_pid_set_property(st->opid, GF_PROP_PID_STREAM_TYPE, &PROP_UINT(GF_STREAM_AUDIO) );
			gf_filter_pid_set_property(st->opid, GF_PROP_PID_CODECID, &PROP_UINT( codecid) );
			st->freq = AVI_audio_rate(ctx->avi);
			gf_filter_pid_set_property(st->opid, GF_PROP_PID_SAMPLE_RATE, &PROP_UINT( st->freq ) );
			st->nb_channels = AVI_audio_channels(ctx->avi);
			gf_filter_pid_set_property(st->opid, GF_PROP_PID_NUM_CHANNELS, &PROP_UINT( st->nb_channels ) );
			brate = AVI_audio_mp3rate(ctx->avi);
			//for mp3 and aac
			if (brate && (unframed || (codecid == GF_CODECID_AAC_MPEG4)))
				gf_filter_pid_set_property(st->opid, GF_PROP_PID_BITRATE, &PROP_UINT( brate ) );
			gf_filter_pid_set_property(st->opid, GF_PROP_PID_ID, &PROP_UINT( 2 + st->stream_num) );
			gf_filter_pid_set_property(st->opid, GF_PROP_PID_CLOCK_ID, &PROP_UINT( sync_id ) );
			gf_filter_pid_set_property(st->opid, GF_PROP_PID_DURATION, &PROP_FRAC64( dur ) );

			gf_filter_pid_set_property(st->opid, GF_PROP_PID_PLAYBACK_MODE, &PROP_UINT(GF_PLAYBACK_MODE_FASTFORWARD ) );
			st->audio_bps = 0;
			if (unframed) {
				gf_filter_pid_set_property(st->opid, GF_PROP_PID_UNFRAMED, &PROP_BOOL( GF_TRUE ) );
				gf_filter_pid_set_property(st->opid, GF_PROP_PID_TIMESCALE, &PROP_UINT(st->freq) );
			} else {
				if (afmt) {
					gf_filter_pid_set_property(st->opid, GF_PROP_PID_AUDIO_FORMAT, &PROP_UINT(afmt) );
				}
				st->audio_bps = AVI_audio_bits(ctx->avi);
				gf_filter_pid_set_property(st->opid, GF_PROP_PID_TIMESCALE, &PROP_UINT(st->freq) );

				if (codecid == GF_CODECID_AAC_MPEG4) {
#ifndef GPAC_DISABLE_AV_PARSERS
					GF_M4ADecSpecInfo acfg;
					u8 *dsi=NULL;
					u32 dsi_len=0;
					memset(&acfg, 0, sizeof(GF_M4ADecSpecInfo));
					acfg.base_object_type = GF_M4A_AAC_LC;
					acfg.base_sr = st->freq;
					acfg.nb_chan = st->nb_channels;
					acfg.sbr_object_type = 0;
					acfg.audioPL = gf_m4a_get_profile(&acfg);
					gf_m4a_write_config(&acfg, &dsi, &dsi_len);
					gf_filter_pid_set_property(st->opid, GF_PROP_PID_DECODER_CONFIG, &PROP_DATA_NO_COPY(dsi, dsi_len) );
#endif
					st->audio_bps = 0;
					st->is_aac = GF_TRUE;
				}
			}

		}
	}
}

GF_Err avidmx_configure_pid(GF_Filter *filter, GF_FilterPid *pid, Bool is_remove)
{
	const GF_PropertyValue *p;
	GF_AVIDmxCtx *ctx = gf_filter_get_udta(filter);

	if (is_remove) {
		ctx->ipid = NULL;
		if (ctx->v_opid) {
			gf_filter_pid_remove(ctx->v_opid);
			ctx->v_opid = NULL;
		}
		while (gf_list_count(ctx->audios) ) {
			AVIAstream *st = gf_list_pop_back(ctx->audios);
			if (st->opid)
				gf_filter_pid_remove(st->opid);
			gf_free(st);
		}
		return GF_OK;
	}
	if (! gf_filter_pid_check_caps(pid))
		return GF_NOT_SUPPORTED;

	if (!ctx->ipid) {
		GF_FilterEvent fevt;
		ctx->ipid = pid;

		//we work with full file only, send a play event on source to indicate that
		GF_FEVT_INIT(fevt, GF_FEVT_PLAY, pid);
		fevt.play.start_range = 0;
		fevt.base.on_pid = ctx->ipid;
		fevt.play.full_file_only = GF_TRUE;
		gf_filter_pid_send_event(ctx->ipid, &fevt);
	}

	p = gf_filter_pid_get_property(ctx->ipid, GF_PROP_PID_FILEPATH);
	if (!p) return GF_NOT_SUPPORTED;

	if (ctx->src_url && !strcmp(ctx->src_url, p->value.string)) return GF_OK;

	if (ctx->avi) {
		u32 i;
		AVI_close(ctx->avi);
		ctx->v_in_use = GF_FALSE;
		for (i=0; i<gf_list_count(ctx->audios); i++) {
			AVIAstream *st = gf_list_get(ctx->audios, i);
			st->in_use = GF_FALSE;
		}
	}
	ctx->avi = NULL;

	ctx->src_url = p->value.string;

	return GF_OK;
}

static Bool avidmx_process_event(GF_Filter *filter, const GF_FilterEvent *evt)
{
	GF_AVIDmxCtx *ctx = gf_filter_get_udta(filter);

	switch (evt->base.type) {
	case GF_FEVT_PLAY:
		gf_filter_post_process_task(filter);
		if (evt->base.on_pid==ctx->v_opid) {
			u32 frame_idx = 0;
			ctx->v_playing = GF_TRUE;
			if ((evt->play.start_range==0) && !ctx->v_init_play_done) {
				ctx->v_init_play_done = GF_TRUE;
				return GF_TRUE;
			}
			ctx->v_init_play_done = GF_TRUE;
			frame_idx = (u32) (ctx->avi->fps * evt->play.start_range);
			if (frame_idx) {
				AVI_set_video_position(ctx->avi, frame_idx);
			} else {
				AVI_seek_start(ctx->avi);
				gf_filter_post_process_task(filter);
			}
			ctx->cur_frame = frame_idx;
		} else {
			u32 i;
			for (i=0; i<gf_list_count(ctx->audios); i++) {
				AVIAstream *st = gf_list_get(ctx->audios, i);
				if (st->opid != evt->base.on_pid) continue;
				st->playing = GF_TRUE;
				if ((evt->play.start_range==0) && !st->init_play_done) {
					st->init_play_done = GF_TRUE;
					return GF_TRUE;
				}
				st->init_play_done = GF_TRUE;
				AVI_set_audio_position(ctx->avi, 0);
				st->seek_to_ts = (u32) (evt->play.start_range * st->freq);
			}
		}

		//cancel play event, we work with full file
		return GF_TRUE;

	case GF_FEVT_STOP:
		if (evt->base.on_pid==ctx->v_opid) {
			ctx->v_playing = GF_FALSE;
		} else {
			u32 i;
			for (i=0; i<gf_list_count(ctx->audios); i++) {
				AVIAstream *st = gf_list_get(ctx->audios, i);
				if (st->opid != evt->base.on_pid) continue;
				st->playing = GF_TRUE;
			}
		}
		//don't cancel event
		return GF_FALSE;

	case GF_FEVT_SET_SPEED:
		//cancel event
		return GF_TRUE;
	default:
		break;
	}
	//by default don't cancel event - to rework once we have downloading in place
	return GF_FALSE;
}

GF_Err avidmx_process(GF_Filter *filter)
{
	GF_AVIDmxCtx *ctx = gf_filter_get_udta(filter);
	u32 i, count, nb_done, nb_active=0;
	Bool start, end, video_done;

	if (!ctx->avi) {
		GF_FilterPacket *pck = gf_filter_pid_get_packet(ctx->ipid);
		if (!pck) {
			return GF_OK;
		}
		gf_filter_pck_get_framing(pck, &start, &end);
		gf_filter_pid_drop_packet(ctx->ipid);

		if (!end) {
			return GF_OK;
		}

		ctx->avi = AVI_open_input_file((char *)ctx->src_url, 1);
		if (!ctx->avi) {
			GF_Err e = GF_NON_COMPLIANT_BITSTREAM;
			if (! gf_file_exists(ctx->src_url)) e = GF_URL_ERROR;
			gf_filter_setup_failure(filter, e);
			return GF_NOT_SUPPORTED;
		}
		avidmx_setup(filter, ctx);
		return GF_OK;
	}
	video_done = GF_FALSE;
	if (ctx->v_in_use && ctx->v_playing && (ctx->cur_frame < ctx->nb_frames) && !gf_filter_pid_would_block(ctx->v_opid) ) {
		u32 key;
		u64 file_offset, cts;
		u8 *pck_data;
		s32 size = AVI_frame_size(ctx->avi, ctx->cur_frame);
		if (!size) {
			AVI_read_frame(ctx->avi, NULL, &key);
			ctx->dummy++;
		}
		//remove dummy frames
		else {
			file_offset = (u64) AVI_get_video_position(ctx->avi, ctx->cur_frame);
			cts = ctx->nb_frame_sent * ctx->fps.den;

			if (size > 4) {
				GF_FilterPacket *dst_pck = gf_filter_pck_new_alloc(ctx->v_opid, size, &pck_data);
				if (!dst_pck) return GF_OUT_OF_MEM;
				AVI_read_frame(ctx->avi, pck_data, &key);
				gf_filter_pck_set_byte_offset(dst_pck, file_offset);
				gf_filter_pck_set_cts(dst_pck, cts);
				gf_filter_pck_set_duration(dst_pck, ctx->fps.den);
				gf_filter_pck_set_framing(dst_pck, GF_TRUE, GF_TRUE);
				if (key) gf_filter_pck_set_sap(dst_pck, GF_FILTER_SAP_1);
				gf_filter_pck_send(dst_pck);
				ctx->nb_frame_sent++;
			} else {
				AVI_read_frame(ctx->avi, NULL, &key);
				ctx->nvops++;
			}
		}
		nb_active++;

		ctx->cur_frame++;
		if (ctx->cur_frame < ctx->nb_frames)
			gf_filter_post_process_task(filter);
		else
			video_done = GF_TRUE;
	} else if (!ctx->v_in_use) {
		video_done = GF_TRUE;
	} else if (ctx->cur_frame >= ctx->nb_frames) {
		video_done = GF_TRUE;
	}
	
	nb_done = 0;
	count = gf_list_count(ctx->audios);
	for (i=0; i<count; i++) {
		s32 size;
		AVIAstream *st = gf_list_get(ctx->audios, i);
		if (st->audio_done || !st->in_use) {
			nb_done++;
			continue;
		}
		if (!ctx->v_playing) video_done = GF_TRUE;
		if (!st->playing || gf_filter_pid_would_block(st->opid) )
			continue;
		AVI_set_audio_track(ctx->avi, st->stream_num);
		nb_active++;

		size = AVI_audio_size(ctx->avi, st->aud_frame);

		if (st->seek_to_ts && size) {
			if (st->seek_to_ts > st->audio_ts) {
				st->aud_frame ++;
				continue;
			}
			else
				st->seek_to_ts = 0;
		}

		if (size>0) {
			int continuous;
			u8 *pck_data;
			u64 file_offset;
			GF_FilterPacket *dst_pck = gf_filter_pck_new_alloc(st->opid, size, &pck_data);
			if (!dst_pck) return GF_OUT_OF_MEM;
			
			file_offset = gf_ftell(ctx->avi->fdes);
			AVI_read_audio(ctx->avi, pck_data, size, (int*)&continuous);

			if (st->audio_bps) {
				u32 nb_samples = (8*size) / (st->audio_bps * st->nb_channels);
				gf_filter_pck_set_cts(dst_pck, st->audio_ts);
				gf_filter_pck_set_sap(dst_pck, GF_FILTER_SAP_1);
				st->audio_ts += nb_samples;
			} else if (st->is_aac) {
				gf_filter_pck_set_cts(dst_pck, st->audio_ts);
				gf_filter_pck_set_sap(dst_pck, GF_FILTER_SAP_1);
				st->audio_ts += 1024;
			}

			if (continuous)
				gf_filter_pck_set_byte_offset(dst_pck, file_offset);

			gf_filter_pck_send(dst_pck);

			st->aud_frame ++;
		} else {
			st->audio_done = GF_TRUE;
			nb_done++;
		}
	}
	if (!nb_active)
		return GF_OK;
	if (video_done && (nb_done==count) ) {
		if (ctx->v_opid && ctx->v_in_use) gf_filter_pid_set_eos(ctx->v_opid);

		for (i=0; i<count;i++) {
			AVIAstream *st = gf_list_get(ctx->audios, i);
			gf_filter_pid_set_eos(st->opid);
		}
		return GF_EOS;
	}
	gf_filter_post_process_task(filter);
	return GF_OK;
}

GF_Err avidmx_initialize(GF_Filter *filter)
{
	GF_AVIDmxCtx *ctx = gf_filter_get_udta(filter);
	ctx->use_file_fps = ctx->fps.den ? GF_FALSE : GF_TRUE;
	ctx->audios = gf_list_new();
	return GF_OK;
}

void avidmx_finalize(GF_Filter *filter)
{
	GF_AVIDmxCtx *ctx = gf_filter_get_udta(filter);
	if (ctx->avi) AVI_close(ctx->avi);
	while (gf_list_count(ctx->audios)) {
		AVIAstream *st = gf_list_pop_back(ctx->audios);
		gf_free(st);
	}
	gf_list_del(ctx->audios);

	if (ctx->importer) {
		GF_LOG(GF_LOG_INFO, GF_LOG_MEDIA, ("AVI Removed Frames: 1 VFW delay frames - 296 N-VOPs\n", ctx->dummy, ctx->nvops));
	}

}

static const char * avidmx_probe_data(const u8 *data, u32 size, GF_FilterProbeScore *score)
{
	if (size<12) return NULL;
	if (strncmp(data, "RIFF", 4)) return NULL;
	if (strncmp(data+8, "AVI ", 4)) return NULL;
	*score = GF_FPROBE_SUPPORTED;
	return "video/avi";
}

static const GF_FilterCapability AVIDmxCaps[] =
{
	CAP_UINT(GF_CAPS_INPUT, GF_PROP_PID_STREAM_TYPE, GF_STREAM_FILE),
	CAP_STRING(GF_CAPS_INPUT, GF_PROP_PID_FILE_EXT, "avi"),
	CAP_STRING(GF_CAPS_INPUT, GF_PROP_PID_MIME, "video/avi|video/x-avi"),
	CAP_UINT(GF_CAPS_OUTPUT, GF_PROP_PID_STREAM_TYPE, GF_STREAM_AUDIO),
	CAP_UINT(GF_CAPS_OUTPUT, GF_PROP_PID_STREAM_TYPE, GF_STREAM_VISUAL),
};


#define OFFS(_n)	#_n, offsetof(GF_AVIDmxCtx, _n)
static const GF_FilterArgs AVIDmxArgs[] =
{
	{ OFFS(fps), "import frame rate, default is AVI one", GF_PROP_FRACTION, "1/0", NULL, 0},
	{ OFFS(importer), "compatibility with old importer, displays import results", GF_PROP_BOOL, "false", NULL, GF_FS_ARG_HINT_ADVANCED},
	{0}
};


GF_FilterRegister AVIDmxRegister = {
	.name = "avidmx",
	GF_FS_SET_DESCRIPTION("AVI demultiplexer")
	GF_FS_SET_HELP("This filter demultiplexes AVI files to produce media PIDs and frames.")
	.private_size = sizeof(GF_AVIDmxCtx),
	.initialize = avidmx_initialize,
	.finalize = avidmx_finalize,
	.args = AVIDmxArgs,
	SETCAPS(AVIDmxCaps),
	.configure_pid = avidmx_configure_pid,
	.process = avidmx_process,
	.process_event = avidmx_process_event,
	.probe_data = avidmx_probe_data,
};

#endif // GPAC_DISABLE_AVILIB

const GF_FilterRegister *avidmx_register(GF_FilterSession *session)
{
#ifndef GPAC_DISABLE_AVILIB
	return &AVIDmxRegister;
#else
	return NULL;
#endif
}


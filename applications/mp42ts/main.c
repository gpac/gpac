/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre, Cyril Concolato, Romain Bouqueau
 *			Copyright (c) Telecom ParisTech 2005-2012
 *					All rights reserved
 *
 *  This file is part of GPAC / mp4-to-ts (mp42ts) application
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

#include <gpac/media_tools.h>
#include <gpac/constants.h>
#include <gpac/base_coding.h>
#include <gpac/mpegts.h>

#ifndef GPAC_DISABLE_STREAMING
#include <gpac/internal/ietf_dev.h>
#endif

#ifndef GPAC_DISABLE_SENG
#include <gpac/scene_engine.h>
#endif

#ifndef GPAC_DISABLE_TTXT
#include <gpac/webvtt.h>
#endif


#ifdef GPAC_DISABLE_MPEG2TS_MUX
#error "Cannot compile MP42TS if GPAC is not built with MPEG2-TS Muxing support"
#endif

#define MP42TS_PRINT_TIME_MS 500 /*refresh printed info every CLOCK_REFRESH ms*/
#define MP42TS_VIDEO_FREQ 1000 /*meant to send AVC IDR only every CLOCK_REFRESH ms*/


s32 temi_id_1 = -1;
s32 temi_id_2 = -1;

u32 temi_url_insertion_delay = 1000;
u32 temi_offset = 0;
Bool temi_disable_loop = GF_FALSE;
FILE *logfile = NULL;

static void on_gpac_log(void *cbk, GF_LOG_Level ll, GF_LOG_Tool lm, const char *fmt, va_list list)
{
	FILE *logs = (FILE*)cbk;
	vfprintf(logs, fmt, list);
	fflush(logs);
}

static GFINLINE void usage()
{
	fprintf(stderr, "GPAC version " GPAC_FULL_VERSION "\n"
	        "GPAC Copyright (c) Telecom ParisTech 2000-2014\n"
	        "GPAC Configuration: " GPAC_CONFIGURATION "\n"
	        "Features: %s\n\n", gpac_features());
	fprintf(stderr, "mp42ts <inputs> <destinations> [options]\n"
	        "\n"
	        "Inputs:\n"
	        "-src filename[:OPTS]   specifies an input file used for a TS service\n"
	        "                        * currently only supports ISO files and SDP files\n"
	        "                        * can be used several times, once for each program\n"
	        "By default each source is a program in a TS. \n"
	        "Source options are colon-separated list of options, as follows:\n"
	        "ID=N                   specifies the program ID for this source.\n"
	        "             All sources with the same ID will be added to the same program\n"
	        "name=STR               program name, as used in DVB service description table\n"
	        "provider=STR           provider name, as used in DVB service description table\n"

	        "\n"
	        "-prog filename        same as -src filename\n"
	        "\n"
	        "Destinations:\n"
	        "Several destinations may be specified as follows, at least one is mandatory\n"
	        "-dst-udp UDP_address:port (multicast or unicast)\n"
	        "-dst-rtp RTP_address:port\n"
	        "-dst-file filename\n"
	        "The following parameters may be specified when -dst-file is used\n"
	        "-segment-dir dir       server local directory to store segments (ends with a '/')\n"
	        "-segment-duration dur  segment duration in seconds\n"
	        "-segment-manifest file m3u8 file basename\n"
	        "-segment-http-prefix p client address for accessing server segments\n"
	        "-segment-number n      number of segments to list in the manifest\n"
	        "\n"
	        "Basic options:\n"
	        "-rate R                specifies target rate in kbps of the multiplex (optional)\n"
	        "-real-time             specifies the muxer will work in real-time mode\n"
	        "                        * if not specified, the muxer will generate the TS as quickly as possible\n"
	        "                        * automatically set for SDP or BT input\n"
	        "-pcr-init V            sets initial value V for PCR - if not set, random value is used\n"
	        "-pcr-offset V          offsets all timestamps from PCR by V, in 90kHz. Default value is computed based on input media.\n"
	        "-psi-rate V            sets PSI refresh rate V in ms (default 100ms).\n"
	        "                        * If 0, PSI data is only send once at the beginning or before each IDR when -rap option is set.\n"
	        "                        * This should be set to 0 for DASH streams.\n"
	        "-time n                request the muxer to stop after n ms\n"
	        "-single-au             forces 1 PES = 1 AU (disabled by default)\n"
	        "-multi-au              forces 1 PES = N AU for all streams (disabled by default).\n"
	        "                        By default, audio streams pack N AUs in one PES but video and systems data use 1 AU per PES.\n"
	        "-rap                   forces RAP/IDR to be aligned with PES start for video streams (disabled by default)\n"
	        "                          in this mode, PAT, PMT and PCR will be inserted before the first TS packet of the RAP PES\n"
	        "-flush-rap             same as -rap but flushes all other streams (sends remaining PES packets) before inserting PAT/PMT\n"
	        "-nb-pack N             specifies to pack up to N TS packets together before sending on network or writing to file\n"
	        "-pcr-ms N              sets max interval in ms between 2 PCR. Default is 100 ms or at each PES header\n"
	        "-force-pcr-only        allows sending PCR-only packets to enforce the requested PCR rate - STILL EXPERIMENTAL.\n"
	        "-ttl N                 specifies Time-To-Live for multicast. Default is 1.\n"
	        "-ifce IPIFCE           specifies default IP interface to use. Default is IF_ANY.\n"
	        "-temi [URL]            Inserts TEMI time codes in adaptation field. URL is optional, and can be a number for external timeline IDs\n"
	        "-temi-delay DelayMS    Specifies delay between two TEMI url descriptors (default is 1000)\n"
	        "-temi-offset OffsetMS  Specifies an offset in ms to add to TEMI (by default TEMI starts at 0)\n"
	        "-temi-noloop           Do not restart the TEMI timeline at the end of the source\n"
	        "-temi2 ID              Inserts a secondary TEMI time codes in adaptation field of the audio PID if any. ID shall be set to the desired external timeline IDs\n"
	        "-insert-ntp            Inserts NTP timestamp in TEMI timeline descriptor\n"
	        "-sdt-rate MS           Gives the SDT carrousel rate in milliseconds. If 0 (default), SDT is not sent\n"
	        "\n"
	        "MPEG-4/T-DMB options:\n"
	        "-bifs-src filename          update file: must be either an .sdp or a .bt file\n"
	        "-audio url             may be mp3/udp or aac/http (shoutcast/icecast)\n"
	        "-video url             shall be a raw h264 frame\n"
	        "-mpeg4-carousel n      carousel period in ms\n"
	        "-mpeg4 or -4on2        forces usage of MPEG-4 signaling (IOD and SL Config)\n"
	        "-4over2                same as -4on2 and uses PMT to carry OD Updates\n"
	        "-bifs-pes              carries BIFS over PES instead of sections\n"
	        "-bifs-pes-ex           carries BIFS over PES without writing timestamps in SL\n"
	        "\n"
	        "Misc options\n"
#ifdef GPAC_MEMORY_TRACKING
            "-mem-track             enables memory tracker\n"
            "-mem-track-stack       enables memory tracker stack dumping\n"
#endif
	        "-logs                  set log tools and levels, formatted as a ':'-separated list of toolX[:toolZ]@levelX\n"
	        "-h or -help            print this screen\n"
	        "\n"
	       );
}


#define MAX_MUX_SRC_PROG	100
typedef struct
{

#ifndef GPAC_DISABLE_ISOM
	GF_ISOFile *mp4;
#endif

	u32 nb_streams, pcr_idx;
	GF_ESInterface streams[40];
	GF_Descriptor *iod;
#ifndef GPAC_DISABLE_SENG
	GF_SceneEngine *seng;
#endif
	GF_Thread *th;
	char *bifs_src_name;
	u32 rate;
	Bool repeat;
	u32 mpeg4_signaling;
	Bool audio_configured;
	u64 samples_done, samples_count;
	u32 nb_real_streams;
	Bool real_time;
	GF_List *od_updates;

	u32 max_sample_size;

	char program_name[20];
	char provider_name[20];
	u32 ID;
	Bool is_not_program_declaration;

	Double last_ntp;
} M2TSSource;

#ifndef GPAC_DISABLE_ISOM
typedef struct
{
	GF_ISOFile *mp4;
	u32 track, sample_number, sample_count;
	u32 mstype, mtype;
	GF_ISOSample *sample;
	/*refresh rate for images*/
	u32 image_repeat_ms, nb_repeat_last;
	void *dsi;
	u32 dsi_size;

	void *dsi_and_rap;
	Bool loop;
	Bool is_repeat;
	s64 ts_offset;
	M2TSSource *source;

	const char *temi_url;
	u32 last_temi_url, timeline_id;
	Bool insert_ntp;

} GF_ESIMP4;
#endif

typedef struct
{
	u32 carousel_period, ts_delta;
	u16 aggregate_on_stream;
	Bool adjust_carousel_time;
	Bool discard;
	Bool rap;
	Bool critical;
	Bool vers_inc;
} GF_ESIStream;

typedef struct
{
	u32 size;
	char *data;
} GF_SimpleDataDescriptor;

//TODO: find a clean way to save this data
#ifndef GPAC_DISABLE_PLAYER
static u32 audio_OD_stream_id = (u32)-1;
#endif

#define AUDIO_OD_ESID	100
#define AUDIO_DATA_ESID	101
#define VIDEO_DATA_ESID	105

/*output types*/
enum
{
	GF_MP42TS_FILE, /*open mpeg2ts file*/
	GF_MP42TS_UDP,  /*open udp socket*/
	GF_MP42TS_RTP,  /*open rtp socket*/
#ifndef GPAC_DISABLE_PLAYER
	GF_MP42TS_HTTP,	/*open http downloader*/
#endif
};

static u32 format_af_descriptor(char *af_data, u32 timeline_id, u64 timecode, u32 timescale, u64 ntp, const char *temi_url, u32 *last_url_time)
{
	u32 res;
	u32 len;
	u32 last_time;
	GF_BitStream *bs = gf_bs_new(af_data, 188, GF_BITSTREAM_WRITE);

	if (ntp) {
		last_time = 1000*(ntp>>32);
		last_time += 1000*(ntp&0xFFFFFFFF)/0xFFFFFFFF;
	} else {
		last_time = (u32) (1000*timecode/timescale);
	}
	if (temi_url && (!*last_url_time || (last_time - *last_url_time + 1 >= temi_url_insertion_delay)) ) {
		*last_url_time = last_time + 1;
		len = 0;
		gf_bs_write_int(bs,	GF_M2TS_AFDESC_LOCATION_DESCRIPTOR, 8);
		gf_bs_write_int(bs,	len, 8);

		gf_bs_write_int(bs,	0, 1); //force_reload
		gf_bs_write_int(bs,	0, 1); //is_announcement
		gf_bs_write_int(bs,	0, 1); //splicing_flag
		gf_bs_write_int(bs,	0, 1); //use_base_temi_url
		gf_bs_write_int(bs,	0xFF, 5); //reserved
		gf_bs_write_int(bs,	timeline_id, 7); //timeline_id

		if (strlen(temi_url)) {
			char *url = (char *)temi_url;
			if (!strnicmp(temi_url, "http://", 7)) {
				gf_bs_write_int(bs,	1, 8); //url_scheme
				url = (char *) temi_url + 7;
			} else if (!strnicmp(temi_url, "https://", 8)) {
				gf_bs_write_int(bs,	2, 8); //url_scheme
				url = (char *) temi_url + 8;
			} else {
				gf_bs_write_int(bs,	0, 8); //url_scheme
			}
			gf_bs_write_u8(bs, (u32) strlen(url)); //url_path_len
			gf_bs_write_data(bs, url, (u32) strlen(url) ); //url
			gf_bs_write_u8(bs, 0); //nb_addons
		}
		//rewrite len
		len = (u32) gf_bs_get_position(bs) - 2;
		af_data[1] = len;
	}

	if (timescale || ntp) {
		Bool use64 = (timecode > 0xFFFFFFFFUL) ? GF_TRUE : GF_FALSE;
		len = 3; //3 bytes flags

		if (timescale) len += 4 + (use64 ? 8 : 4);
		if (ntp) len += 8;

		//write timeline descriptor
		gf_bs_write_int(bs,	GF_M2TS_AFDESC_TIMELINE_DESCRIPTOR, 8);
		gf_bs_write_int(bs,	len, 8);

		gf_bs_write_int(bs,	timescale ? (use64 ? 2 : 1) : 0, 2); //has_timestamp
		gf_bs_write_int(bs,	ntp ? 1 : 0, 1); //has_ntp
		gf_bs_write_int(bs,	0, 1); //has_ptp
		gf_bs_write_int(bs,	0, 2); //has_timecode
		gf_bs_write_int(bs,	0, 1); //force_reload
		gf_bs_write_int(bs,	0, 1); //paused
		gf_bs_write_int(bs,	0, 1); //discontinuity
		gf_bs_write_int(bs,	0xFF, 7); //reserved
		gf_bs_write_int(bs,	timeline_id, 8); //timeline_id
		if (timescale) {
			gf_bs_write_u32(bs,	timescale); //timescale
			if (use64)
				gf_bs_write_u64(bs,	timecode); //timestamp
			else
				gf_bs_write_u32(bs,	(u32) timecode); //timestamp
		}
		if (ntp) {
			gf_bs_write_u64(bs,	ntp); //ntp
		}
	}
	res = (u32) gf_bs_get_position(bs);
	gf_bs_del(bs);
	return res;
}

#ifndef GPAC_DISABLE_ISOM

static GF_Err mp4_input_ctrl(GF_ESInterface *ifce, u32 act_type, void *param)
{
	char af_data[188];
	GF_ESIMP4 *priv = (GF_ESIMP4 *)ifce->input_udta;
	if (!priv) return GF_BAD_PARAM;

	switch (act_type) {
	case GF_ESI_INPUT_DATA_FLUSH:
	{
		GF_ESIPacket pck;
#ifndef GPAC_DISABLE_TTXT
		GF_List *cues = NULL;
#endif
		if (!priv->sample)
			priv->sample = gf_isom_get_sample(priv->mp4, priv->track, priv->sample_number+1, NULL);

		if (!priv->sample) {
			return GF_IO_ERR;
		}

		memset(&pck, 0, sizeof(GF_ESIPacket));

		pck.flags = GF_ESI_DATA_AU_START | GF_ESI_DATA_HAS_CTS;
		if (priv->sample->IsRAP) pck.flags |= GF_ESI_DATA_AU_RAP;
		pck.cts = priv->sample->DTS + priv->ts_offset;
		if (priv->is_repeat) pck.flags |= GF_ESI_DATA_REPEAT;

		if (priv->timeline_id) {
			u64 ntp=0;
			u64 tc = priv->sample->DTS + priv->sample->CTS_Offset;
			if (temi_disable_loop) {
				tc += priv->ts_offset;
			}

			if (temi_offset) {
				tc += ((u64) temi_offset) * ifce->timescale / 1000;
			}

			if (priv->insert_ntp) {
				u32 sec, frac;
				gf_net_get_ntp(&sec, &frac);
				ntp = sec;
				ntp <<= 32;
				ntp |= frac;
			}
			pck.mpeg2_af_descriptors_size = format_af_descriptor(af_data, priv->timeline_id - 1, tc, ifce->timescale, ntp, priv->temi_url, &priv->last_temi_url);
			pck.mpeg2_af_descriptors = af_data;
		}

		if (priv->nb_repeat_last) {
			pck.cts += priv->nb_repeat_last*ifce->timescale * priv->image_repeat_ms / 1000;
		}

		pck.dts = pck.cts;
		if (priv->sample->CTS_Offset) {
			pck.cts += priv->sample->CTS_Offset;
			pck.flags |= GF_ESI_DATA_HAS_DTS;
		}

		if (priv->sample->IsRAP && priv->dsi && priv->dsi_size) {
			pck.data = (char*)priv->dsi;
			pck.data_len = priv->dsi_size;
			ifce->output_ctrl(ifce, GF_ESI_OUTPUT_DATA_DISPATCH, &pck);
			pck.flags &= ~GF_ESI_DATA_AU_START;
		}

		pck.flags |= GF_ESI_DATA_AU_END;
		pck.data = priv->sample->data;
		pck.data_len = priv->sample->dataLength;
		pck.duration = gf_isom_get_sample_duration(priv->mp4, priv->track, priv->sample_number+1);
#ifndef GPAC_DISABLE_TTXT
		if (priv->mtype==GF_ISOM_MEDIA_TEXT && priv->mstype==GF_ISOM_SUBTYPE_WVTT) {
			u64             start;
			GF_WebVTTCue    *cue;
			GF_List *gf_webvtt_parse_iso_cues(GF_ISOSample *iso_sample, u64 start);
			start = (priv->sample->DTS * 1000) / ifce->timescale;
			cues = gf_webvtt_parse_iso_cues(priv->sample, start);
			if (gf_list_count(cues)>1) {
				GF_LOG(GF_LOG_DEBUG, GF_LOG_CONTAINER, ("[MPEG-2 TS Muxer] More than one cue in sample\n"));
			}
			cue = (GF_WebVTTCue *)gf_list_get(cues, 0);
			if (cue) {
				pck.data = cue->text;
				pck.data_len = (u32)strlen(cue->text)+1;
			} else {
				pck.data = NULL;
				pck.data_len = 0;
			}
		}
#endif
		ifce->output_ctrl(ifce, GF_ESI_OUTPUT_DATA_DISPATCH, &pck);
		GF_LOG(GF_LOG_DEBUG, GF_LOG_CONTAINER, ("[MPEG-2 TS Muxer] Track %d: sample %d CTS %d\n", priv->track, priv->sample_number+1, pck.cts));

#ifndef GPAC_DISABLE_VTT
		if (cues) {
			while (gf_list_count(cues)) {
				GF_WebVTTCue *cue = (GF_WebVTTCue *)gf_list_get(cues, 0);
				gf_list_rem(cues, 0);
				gf_webvtt_cue_del(cue);
			}
			gf_list_del(cues);
			cues = NULL;
		}
#endif
		gf_isom_sample_del(&priv->sample);
		priv->sample_number++;

		if (!priv->source->real_time && !priv->is_repeat) {
			priv->source->samples_done++;
			gf_set_progress("Converting to MPEG-2 TS", priv->source->samples_done, priv->source->samples_count);
		}

		if (priv->sample_number==priv->sample_count) {
			if (priv->loop) {
				Double scale;
				u64 duration;
				/*increment ts offset*/
				scale = gf_isom_get_media_timescale(priv->mp4, priv->track);
				scale /= gf_isom_get_timescale(priv->mp4);
				duration = (u64) (gf_isom_get_duration(priv->mp4) * scale);
				priv->ts_offset += duration;
				priv->sample_number = 0;
				priv->is_repeat = (priv->sample_count==1) ? GF_TRUE : GF_FALSE;
			}
			else if (priv->image_repeat_ms && priv->source->nb_real_streams) {
				priv->nb_repeat_last++;
				priv->sample_number--;
				priv->is_repeat = GF_TRUE;
			} else {
				if (!(ifce->caps & GF_ESI_STREAM_IS_OVER)) {
					ifce->caps |= GF_ESI_STREAM_IS_OVER;
					if (priv->sample_count>1) {
						assert(priv->source->nb_real_streams);
						priv->source->nb_real_streams--;
					}
				}
			}
		}
	}
	return GF_OK;

	case GF_ESI_INPUT_DESTROY:
		if (priv->dsi) gf_free(priv->dsi);
		if (ifce->decoder_config) {
			gf_free(ifce->decoder_config);
			ifce->decoder_config = NULL;
		}
		gf_free(priv);
		ifce->input_udta = NULL;
		return GF_OK;
	default:
		return GF_BAD_PARAM;
	}
}

static void fill_isom_es_ifce(M2TSSource *source, GF_ESInterface *ifce, GF_ISOFile *mp4, u32 track_num, u32 bifs_use_pes, Bool compute_max_size)
{
	GF_ESIMP4 *priv;
	char *_lan;
	GF_ESD *esd;
	u64 avg_rate, duration;
	s32 ref_count;
	s64 mediaOffset;

	GF_SAFEALLOC(priv, GF_ESIMP4);
	if (!priv) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_APP, ("Failed to allocate MP4 input handler\n"));
		return;
	}

	priv->mp4 = mp4;
	priv->track = track_num;
	priv->mtype = gf_isom_get_media_type(priv->mp4, priv->track);
	priv->mstype = gf_isom_get_media_subtype(priv->mp4, priv->track, 1);
	priv->loop = source->real_time ? GF_TRUE : GF_FALSE;
	priv->sample_count = gf_isom_get_sample_count(mp4, track_num);
	source->samples_count += priv->sample_count;
	if (priv->sample_count>1)
		source->nb_real_streams++;

	priv->source = source;
	memset(ifce, 0, sizeof(GF_ESInterface));
	ifce->stream_id = gf_isom_get_track_id(mp4, track_num);

	esd = gf_media_map_esd(mp4, track_num);

	if (esd) {
		ifce->stream_type = esd->decoderConfig->streamType;
		ifce->object_type_indication = esd->decoderConfig->objectTypeIndication;
		if (esd->decoderConfig->decoderSpecificInfo && esd->decoderConfig->decoderSpecificInfo->dataLength) {
			switch (esd->decoderConfig->objectTypeIndication) {
			case GPAC_OTI_AUDIO_AAC_MPEG4:
			case GPAC_OTI_AUDIO_AAC_MPEG2_MP:
			case GPAC_OTI_AUDIO_AAC_MPEG2_LCP:
			case GPAC_OTI_AUDIO_AAC_MPEG2_SSRP:
			case GPAC_OTI_VIDEO_MPEG4_PART2:
				ifce->decoder_config = (char *)gf_malloc(sizeof(char)*esd->decoderConfig->decoderSpecificInfo->dataLength);
				ifce->decoder_config_size = esd->decoderConfig->decoderSpecificInfo->dataLength;
				memcpy(ifce->decoder_config, esd->decoderConfig->decoderSpecificInfo->data, esd->decoderConfig->decoderSpecificInfo->dataLength);
				if (esd->decoderConfig->objectTypeIndication == GPAC_OTI_VIDEO_MPEG4_PART2) {
					priv->dsi = (char *)gf_malloc(sizeof(char)*esd->decoderConfig->decoderSpecificInfo->dataLength);
					priv->dsi_size = esd->decoderConfig->decoderSpecificInfo->dataLength;
					memcpy(priv->dsi, esd->decoderConfig->decoderSpecificInfo->data, esd->decoderConfig->decoderSpecificInfo->dataLength);
				}
				break;
			case GPAC_OTI_VIDEO_HEVC:
			case GPAC_OTI_VIDEO_LHVC:
			case GPAC_OTI_VIDEO_AVC:
			case GPAC_OTI_VIDEO_SVC:
				gf_isom_set_nalu_extract_mode(mp4, track_num, GF_ISOM_NALU_EXTRACT_LAYER_ONLY | GF_ISOM_NALU_EXTRACT_INBAND_PS_FLAG | GF_ISOM_NALU_EXTRACT_ANNEXB_FLAG | GF_ISOM_NALU_EXTRACT_VDRD_FLAG);
				break;
			case GPAC_OTI_SCENE_VTT_MP4:
				ifce->decoder_config = (char *)gf_malloc(sizeof(char)*esd->decoderConfig->decoderSpecificInfo->dataLength);
				ifce->decoder_config_size = esd->decoderConfig->decoderSpecificInfo->dataLength;
				memcpy(ifce->decoder_config, esd->decoderConfig->decoderSpecificInfo->data, esd->decoderConfig->decoderSpecificInfo->dataLength);
				break;
			}
		}
		gf_odf_desc_del((GF_Descriptor *)esd);
	}
	gf_isom_get_media_language(mp4, track_num, &_lan);
	if (!_lan || !strcmp(_lan, "und")) {
		ifce->lang = 0;
	} else {
		ifce->lang = GF_4CC(_lan[0],_lan[1],_lan[2],' ');
	}
	if (_lan) {
		gf_free(_lan);
	}

	ifce->timescale = gf_isom_get_media_timescale(mp4, track_num);
	ifce->duration = gf_isom_get_media_timescale(mp4, track_num);
	avg_rate = gf_isom_get_media_data_size(mp4, track_num);
	avg_rate *= ifce->timescale * 8;
	if (0!=(duration=gf_isom_get_media_duration(mp4, track_num)))
		avg_rate /= duration;

	if (gf_isom_has_time_offset(mp4, track_num)) ifce->caps |= GF_ESI_SIGNAL_DTS;

	ifce->bit_rate = (u32) avg_rate;
	ifce->duration = (Double) (s64) gf_isom_get_media_duration(mp4, track_num);
	ifce->duration /= ifce->timescale;

	GF_SAFEALLOC(ifce->sl_config, GF_SLConfig);
	if (!ifce->sl_config) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_APP, ("Failed to allocate interface SLConfig\n"));
		return;
	}
	
	ifce->sl_config->tag = GF_ODF_SLC_TAG;
	ifce->sl_config->useAccessUnitStartFlag = 1;
	ifce->sl_config->useAccessUnitEndFlag = 1;
	ifce->sl_config->useRandomAccessPointFlag = 1;
	ifce->sl_config->useTimestampsFlag = 1;
	ifce->sl_config->timestampLength = 33;
	ifce->sl_config->timestampResolution = ifce->timescale;

	/*test mode in which time stamps are 90khz and not coded but copied over from PES header*/
	if (bifs_use_pes==2) {
		ifce->sl_config->timestampLength = 0;
		ifce->sl_config->timestampResolution = 90000;
	}

#ifdef GPAC_DISABLE_ISOM_WRITE
	fprintf(stderr, "Warning: GPAC was compiled without ISOM Write support, can't set SL Config!\n");
#else
	gf_isom_set_extraction_slc(mp4, track_num, 1, ifce->sl_config);
#endif

	ifce->input_ctrl = mp4_input_ctrl;
	if (priv != ifce->input_udta) {
		if (ifce->input_udta)
			gf_free(ifce->input_udta);
		ifce->input_udta = priv;
	}


	if (! gf_isom_get_edit_list_type(mp4, track_num, &mediaOffset)) {
		priv->ts_offset = mediaOffset;
	}

	ref_count = gf_isom_get_reference_count(mp4, track_num, GF_ISOM_REF_SCAL);
	if (ref_count > 0) {
		gf_isom_get_reference_ID(mp4, track_num, GF_ISOM_REF_SCAL, (u32) ref_count, &ifce->depends_on_stream);
	}
	else {
		ifce->depends_on_stream = 0;
	}

	if (compute_max_size) {
		u32 i;
		for (i=0; i < priv->sample_count; i++) {
			u32 s = gf_isom_get_sample_size(mp4, track_num, i+1);
			if (s>source->max_sample_size) source->max_sample_size = s;
		}
	}

}

#endif //GPAC_DISABLE_ISOM


#ifndef GPAC_DISABLE_SENG
static GF_Err seng_input_ctrl(GF_ESInterface *ifce, u32 act_type, void *param)
{
	if (act_type==GF_ESI_INPUT_DESTROY) {
		//TODO: free my data
		if (ifce->input_udta)
			gf_free(ifce->input_udta);
		ifce->input_udta = NULL;
		return GF_OK;
	}

	return GF_OK;
}
#endif


#ifndef GPAC_DISABLE_STREAMING
typedef struct
{
	/*RTP channel*/
	GF_RTPChannel *rtp_ch;

	/*depacketizer*/
	GF_RTPDepacketizer *depacketizer;

	GF_ESIPacket pck;

	GF_ESInterface *ifce;

	Bool cat_dsi, is_264;
	void *dsi_and_rap;
	u32 avc_dsi_size;

	Bool use_carousel;
	u32 au_sn;

	s64 ts_offset;
	Bool rtcp_init;
	M2TSSource *source;

	u32 min_dts_inc;
	u64 prev_cts;
	u64 prev_dts;
} GF_ESIRTP;

static GF_Err rtp_input_ctrl(GF_ESInterface *ifce, u32 act_type, void *param)
{
	u32 size, PayloadStart;
	GF_Err e;
	GF_RTPHeader hdr;
	char buffer[8000];
	GF_ESIRTP *rtp = (GF_ESIRTP*)ifce->input_udta;

	if (!ifce->input_udta) return GF_BAD_PARAM;

	switch (act_type) {
	case GF_ESI_INPUT_DATA_FLUSH:
		/*flush rtcp channel*/
		while (1) {
			Bool has_sr = GF_FALSE;
			size = gf_rtp_read_rtcp(rtp->rtp_ch, buffer, 8000);
			if (!size) break;
			e = gf_rtp_decode_rtcp(rtp->rtp_ch, buffer, size, &has_sr);

			if (e == GF_EOS) ifce->caps |= GF_ESI_STREAM_IS_OVER;

			if (has_sr && !rtp->rtcp_init) {
				Double time = rtp->rtp_ch->last_SR_NTP_sec;
				time += ((Double)rtp->rtp_ch->last_SR_NTP_frac)/0xFFFFFFFF;
				if (!rtp->source->last_ntp) {
					rtp->source->last_ntp = time;
				}
				if (time >= rtp->source->last_ntp) {
					time -= rtp->source->last_ntp;
				} else {
					time = 0;
				}
				rtp->ts_offset = rtp->rtp_ch->last_SR_rtp_time;
				rtp->ts_offset -= (s64) (time * rtp->rtp_ch->TimeScale);
				rtp->rtcp_init = GF_TRUE;
			}
		}
		/*flush rtp channel*/
		while (1) {
			size = gf_rtp_read_rtp(rtp->rtp_ch, buffer, 8000);
			if (!size) break;
			e = gf_rtp_decode_rtp(rtp->rtp_ch, buffer, size, &hdr, &PayloadStart);
			if (e) return e;
			gf_rtp_depacketizer_process(rtp->depacketizer, &hdr, buffer + PayloadStart, size - PayloadStart);
		}
		return GF_OK;
	case GF_ESI_INPUT_DESTROY:
		gf_rtp_depacketizer_del(rtp->depacketizer);
		if (rtp->dsi_and_rap) gf_free(rtp->dsi_and_rap);
		gf_rtp_del(rtp->rtp_ch);
		gf_free(rtp);

		if (ifce->decoder_config) {
			gf_free(ifce->decoder_config);
			ifce->decoder_config = NULL;
		}
		ifce->input_udta = NULL;
		return GF_OK;
	}
	return GF_OK;
}

static void rtp_sl_packet_cbk(void *udta, char *payload, u32 size, GF_SLHeader *hdr, GF_Err e)
{
	GF_ESIRTP *rtp = (GF_ESIRTP*)udta;

	/*sync not found yet, cannot start (since we don't support PCR discontinuities yet ...)*/
	if (!rtp->rtcp_init) return;

	/*try to compute a DTS*/
	if (hdr->accessUnitStartFlag && !hdr->decodingTimeStampFlag) {
		if (!rtp->prev_cts) {
			rtp->prev_cts = rtp->prev_dts = hdr->compositionTimeStamp;
		}

		if (hdr->compositionTimeStamp > rtp->prev_cts) {
			u32 diff = (u32) (hdr->compositionTimeStamp - rtp->prev_cts);
			if (!rtp->min_dts_inc || (rtp->min_dts_inc > diff)) {
				rtp->min_dts_inc = diff;
				rtp->prev_dts = hdr->compositionTimeStamp - diff;
			}
		}
		hdr->decodingTimeStampFlag = 1;
		hdr->decodingTimeStamp = rtp->prev_dts + rtp->min_dts_inc;
		rtp->prev_dts += rtp->min_dts_inc;
		if (hdr->compositionTimeStamp < hdr->decodingTimeStamp) {
			hdr->decodingTimeStamp = hdr->compositionTimeStamp;
		}
	}

	rtp->pck.data = payload;
	rtp->pck.data_len = size;
	rtp->pck.dts = hdr->decodingTimeStamp + rtp->ts_offset;
	rtp->pck.cts = hdr->compositionTimeStamp + rtp->ts_offset;
	rtp->pck.flags = 0;
	if (hdr->compositionTimeStampFlag) rtp->pck.flags |= GF_ESI_DATA_HAS_CTS;
	if (hdr->decodingTimeStampFlag) rtp->pck.flags |= GF_ESI_DATA_HAS_DTS;
	if (hdr->randomAccessPointFlag) rtp->pck.flags |= GF_ESI_DATA_AU_RAP;
	if (hdr->accessUnitStartFlag) rtp->pck.flags |= GF_ESI_DATA_AU_START;
	if (hdr->accessUnitEndFlag) rtp->pck.flags |= GF_ESI_DATA_AU_END;

	if (rtp->use_carousel) {
		if ((hdr->AU_sequenceNumber==rtp->au_sn) && hdr->randomAccessPointFlag) rtp->pck.flags |= GF_ESI_DATA_REPEAT;
		rtp->au_sn = hdr->AU_sequenceNumber;
	}

	if (rtp->is_264) {
		if (!payload) return;

		/*send a NALU delim: copy over NAL ref idc*/
		if (hdr->accessUnitStartFlag) {
			char sc[6];
			sc[0] = sc[1] = sc[2] = 0;
			sc[3] = 1;
			sc[4] = (payload[4] & 0x60) | GF_AVC_NALU_ACCESS_UNIT;
			sc[5] = 0xF0 /*7 "all supported NALUs" (=111) + rbsp trailing (10000)*/;

			rtp->pck.data = sc;
			rtp->pck.data_len = 6;
			rtp->ifce->output_ctrl(rtp->ifce, GF_ESI_OUTPUT_DATA_DISPATCH, &rtp->pck);

			rtp->pck.flags &= ~GF_ESI_DATA_AU_START;

			/*since we don't inspect the RTP content, we can only concatenate SPS and PPS indicated in SDP*/
			if (hdr->randomAccessPointFlag && rtp->dsi_and_rap) {
				rtp->pck.data = (char*)rtp->dsi_and_rap;
				rtp->pck.data_len = rtp->avc_dsi_size;

				rtp->ifce->output_ctrl(rtp->ifce, GF_ESI_OUTPUT_DATA_DISPATCH, &rtp->pck);
			}

			rtp->pck.data = payload;
			rtp->pck.data_len = size;
		}

		rtp->ifce->output_ctrl(rtp->ifce, GF_ESI_OUTPUT_DATA_DISPATCH, &rtp->pck);
	} else {
		if (rtp->cat_dsi && hdr->randomAccessPointFlag && hdr->accessUnitStartFlag) {
			if (rtp->dsi_and_rap) gf_free(rtp->dsi_and_rap);
			rtp->pck.data_len = size + rtp->depacketizer->sl_map.configSize;
			rtp->dsi_and_rap = gf_malloc(sizeof(char)*(rtp->pck.data_len));
			memcpy(rtp->dsi_and_rap, rtp->depacketizer->sl_map.config, rtp->depacketizer->sl_map.configSize);
			memcpy((char *) rtp->dsi_and_rap + rtp->depacketizer->sl_map.configSize, payload, size);
			rtp->pck.data = (char*)rtp->dsi_and_rap;
		}
		rtp->ifce->output_ctrl(rtp->ifce, GF_ESI_OUTPUT_DATA_DISPATCH, &rtp->pck);
	}
}

static void fill_rtp_es_ifce(GF_ESInterface *ifce, GF_SDPMedia *media, GF_SDPInfo *sdp, M2TSSource *source)
{
	u32 i;
	GF_Err e;
	GF_X_Attribute*att;
	GF_ESIRTP *rtp;
	GF_RTPMap*map;
	GF_SDPConnection *conn;
	GF_RTSPTransport trans;

	/*check connection*/
	conn = sdp->c_connection;
	if (!conn) conn = (GF_SDPConnection*)gf_list_get(media->Connections, 0);

	/*check payload type*/
	map = (GF_RTPMap*)gf_list_get(media->RTPMaps, 0);
	GF_SAFEALLOC(rtp, GF_ESIRTP);
	if (!rtp) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_APP, ("Failed to allocate RTP input handler\n"));
		return;
	}

	memset(ifce, 0, sizeof(GF_ESInterface));
	rtp->rtp_ch = gf_rtp_new();
	i=0;
	while ((att = (GF_X_Attribute*)gf_list_enum(media->Attributes, &i))) {
		if (!stricmp(att->Name, "mpeg4-esid") && att->Value) ifce->stream_id = atoi(att->Value);
	}

	memset(&trans, 0, sizeof(GF_RTSPTransport));
	trans.Profile = media->Profile;
	trans.source = conn ? conn->host : sdp->o_address;
	trans.IsUnicast = gf_sk_is_multicast_address(trans.source) ? GF_FALSE : GF_TRUE;
	if (!trans.IsUnicast) {
		trans.port_first = media->PortNumber;
		trans.port_last = media->PortNumber + 1;
		trans.TTL = conn ? conn->TTL : 0;
	} else {
		trans.client_port_first = media->PortNumber;
		trans.client_port_last = media->PortNumber + 1;
	}

	if (gf_rtp_setup_transport(rtp->rtp_ch, &trans, NULL) != GF_OK) {
		gf_rtp_del(rtp->rtp_ch);
		fprintf(stderr, "Cannot initialize RTP transport\n");
		return;
	}
	/*setup depacketizer*/
	rtp->depacketizer = gf_rtp_depacketizer_new(media, rtp_sl_packet_cbk, rtp);
	if (!rtp->depacketizer) {
		gf_rtp_del(rtp->rtp_ch);
		fprintf(stderr, "Cannot create RTP depacketizer\n");
		return;
	}
	/*setup channel*/
	gf_rtp_setup_payload(rtp->rtp_ch, map);
	ifce->input_udta = rtp;
	ifce->input_ctrl = rtp_input_ctrl;
	rtp->ifce = ifce;
	rtp->source = source;

	ifce->object_type_indication = rtp->depacketizer->sl_map.ObjectTypeIndication;
	ifce->stream_type = rtp->depacketizer->sl_map.StreamType;
	ifce->timescale = gf_rtp_get_clockrate(rtp->rtp_ch);
	if (rtp->depacketizer->sl_map.config) {
		switch (ifce->object_type_indication) {
		case GPAC_OTI_VIDEO_MPEG4_PART2:
			rtp->cat_dsi = GF_TRUE;
			break;
		case GPAC_OTI_VIDEO_AVC:
		case GPAC_OTI_VIDEO_SVC:
			rtp->is_264 = GF_TRUE;
			rtp->depacketizer->flags |= GF_RTP_AVC_USE_ANNEX_B;
			{
#ifndef GPAC_DISABLE_AV_PARSERS
				GF_AVCConfig *avccfg = gf_odf_avc_cfg_read(rtp->depacketizer->sl_map.config, rtp->depacketizer->sl_map.configSize);
				if (avccfg) {
					GF_AVCConfigSlot *slc;
					u32 i;
					GF_BitStream *bs;

					bs = gf_bs_new(NULL, 0, GF_BITSTREAM_WRITE);
					for (i=0; i<gf_list_count(avccfg->sequenceParameterSets); i++) {
						slc = (GF_AVCConfigSlot*)gf_list_get(avccfg->sequenceParameterSets, i);
						gf_bs_write_u32(bs, 1);
						gf_bs_write_data(bs, slc->data, slc->size);
					}
					for (i=0; i<gf_list_count(avccfg->pictureParameterSets); i++) {
						slc = (GF_AVCConfigSlot*)gf_list_get(avccfg->pictureParameterSets, i);
						gf_bs_write_u32(bs, 1);
						gf_bs_write_data(bs, slc->data, slc->size);
					}
					gf_bs_get_content(bs, (char **) &rtp->dsi_and_rap, &rtp->avc_dsi_size);
					gf_bs_del(bs);
				}
				gf_odf_avc_cfg_del(avccfg);
#endif
			}
			break;
		case GPAC_OTI_AUDIO_AAC_MPEG4:
			ifce->decoder_config = (char*)gf_malloc(sizeof(char) * rtp->depacketizer->sl_map.configSize);
			ifce->decoder_config_size = rtp->depacketizer->sl_map.configSize;
			memcpy(ifce->decoder_config, rtp->depacketizer->sl_map.config, rtp->depacketizer->sl_map.configSize);
			break;
		}
	}
	if (rtp->depacketizer->sl_map.StreamStateIndication) {
		rtp->use_carousel = GF_TRUE;
		rtp->au_sn=0;
	}

	/*DTS signaling is only supported for MPEG-4 visual*/
	if (rtp->depacketizer->sl_map.DTSDeltaLength) ifce->caps |= GF_ESI_SIGNAL_DTS;

	gf_rtp_depacketizer_reset(rtp->depacketizer, GF_TRUE);
	e = gf_rtp_initialize(rtp->rtp_ch, 0x100000ul, GF_FALSE, 0, 10, 200, NULL);
	if (e!=GF_OK) {
		gf_rtp_del(rtp->rtp_ch);
		fprintf(stderr, "Cannot initialize RTP channel: %s\n", gf_error_to_string(e));
		return;
	}
	fprintf(stderr, "RTP interface initialized\n");
}
#endif /*GPAC_DISABLE_STREAMING*/

#ifndef GPAC_DISABLE_SENG
static GF_Err void_input_ctrl(GF_ESInterface *ifce, u32 act_type, void *param)
{
	return GF_OK;
}
#endif

/*AAC import features*/
#ifndef GPAC_DISABLE_PLAYER

void *audio_prog = NULL;
static void SampleCallBack(void *calling_object, u16 ESID, char *data, u32 size, u64 ts);
#define DONT_USE_TERMINAL_MODULE_API
#include "../../modules/aac_in/aac_in.c"
AACReader *aac_reader = NULL;
u64 audio_discontinuity_offset = 0;

/*create an OD codec and encode the descriptor*/
static GF_Err encode_audio_desc(GF_ESD *esd, GF_SimpleDataDescriptor *audio_desc)
{
	GF_Err e;
	GF_ODCodec *odc = gf_odf_codec_new();
	GF_ODUpdate *od_com = (GF_ODUpdate*)gf_odf_com_new(GF_ODF_OD_UPDATE_TAG);
	GF_ObjectDescriptor *od = (GF_ObjectDescriptor*)gf_odf_desc_new(GF_ODF_OD_TAG);
	assert( esd );
	assert( audio_desc );
	gf_list_add(od->ESDescriptors, esd);
	od->objectDescriptorID = AUDIO_DATA_ESID;
	gf_list_add(od_com->objectDescriptors, od);

	e = gf_odf_codec_add_com(odc, (GF_ODCom*)od_com);
	if (e) {
		fprintf(stderr, "Audio input error add the command to be encoded\n");
		return e;
	}
	e = gf_odf_codec_encode(odc, 0);
	if (e) {
		fprintf(stderr, "Audio input error encoding the descriptor\n");
		return e;
	}
	e = gf_odf_codec_get_au(odc, &audio_desc->data, &audio_desc->size);
	if (e) {
		fprintf(stderr, "Audio input error getting the descriptor\n");
		return e;
	}
	e = gf_odf_com_del((GF_ODCom**)&od_com);
	if (e) {
		fprintf(stderr, "Audio input error deleting the command\n");
		return e;
	}
	gf_odf_codec_del(odc);

	return GF_OK;
}

#endif


static void SampleCallBack(void *calling_object, u16 ESID, char *data, u32 size, u64 ts)
{
	u32 i;
	//fprintf(stderr, "update: ESID=%d - size=%d - ts="LLD"\n", ESID, size, ts);

	if (calling_object) {
		M2TSSource *source = (M2TSSource *)calling_object;

#ifndef GPAC_DISABLE_PLAYER
		if (ESID == AUDIO_DATA_ESID) {
			if (audio_OD_stream_id != (u32)-1) {
				/*this is the first time we get some audio data. Therefore we are sure we can retrieve the audio descriptor. Then we'll
				  send it by calling this callback recursively so that a player gets the audio descriptor before audio data.
				  Hack: the descriptor is carried thru the input_udta, you shall delete it*/
				GF_SimpleDataDescriptor *audio_desc = source->streams[audio_OD_stream_id].input_udta;
				if (audio_desc && !audio_desc->data) /*intended for HTTP/AAC: an empty descriptor was set (vs already filled for RTP/UDP MP3)*/
				{
					/*get the audio descriptor and encode it*/
					GF_ESD *esd = AAC_GetESD(aac_reader);
					assert(esd->slConfig->timestampResolution);
					esd->slConfig->useAccessUnitStartFlag = 1;
					esd->slConfig->useAccessUnitEndFlag = 1;
					esd->slConfig->useTimestampsFlag = 1;
					esd->slConfig->timestampLength = 33;
					/*audio stream, all samples are RAPs*/
					esd->slConfig->useRandomAccessPointFlag = 0;
					esd->slConfig->hasRandomAccessUnitsOnlyFlag = 1;
					for (i=0; i<source->nb_streams; i++) {
						if (source->streams[i].stream_id == AUDIO_DATA_ESID) {
							GF_Err e;
							source->streams[i].timescale = esd->slConfig->timestampResolution;
							e = gf_m2ts_program_stream_update_ts_scale(&source->streams[i], esd->slConfig->timestampResolution);
							if (e != GF_OK) {
								fprintf(stderr, "Failed updating TS program timescale\n");
							}
							else if (!source->streams[i].sl_config)
								source->streams[i].sl_config = (GF_SLConfig *)gf_odf_desc_new(GF_ODF_SLC_TAG);

							memcpy(source->streams[i].sl_config, esd->slConfig, sizeof(GF_SLConfig));
							break;
						}
					}
					esd->ESID = AUDIO_DATA_ESID;
					assert(audio_OD_stream_id != (u32)-1);
					encode_audio_desc(esd, audio_desc);

					/*build the ESI*/
					{
						/*audio OD descriptor: rap=1 and vers_inc=0*/
						GF_SAFEALLOC(source->streams[audio_OD_stream_id].input_udta, GF_ESIStream);
						if (!source->streams[audio_OD_stream_id].input_udta) {
							GF_LOG(GF_LOG_ERROR, GF_LOG_APP, ("Failed to allocate aac input handler\n"));
							return;
						}
						((GF_ESIStream*)source->streams[audio_OD_stream_id].input_udta)->rap = 1;

						/*we have the descriptor; now call this callback recursively so that a player gets the audio descriptor before audio data.*/
						source->repeat = 1;
						SampleCallBack(source, AUDIO_OD_ESID, audio_desc->data, audio_desc->size, 0/*gf_m2ts_get_sys_clock(muxer)*/);
						source->repeat = 0;

						/*clean*/
						gf_free(audio_desc->data);
						gf_free(audio_desc);
						gf_free(source->streams[audio_OD_stream_id].input_udta);
						source->streams[audio_OD_stream_id].input_udta = NULL;
					}
				}
			}
			/*update the timescale if needed*/
			else if (!source->audio_configured) {
				GF_ESD *esd = AAC_GetESD(aac_reader);
				assert(esd->slConfig->timestampResolution);
				for (i=0; i<source->nb_streams; i++) {
					if (source->streams[i].stream_id == AUDIO_DATA_ESID) {
						GF_Err e;
						source->streams[i].timescale = esd->slConfig->timestampResolution;
						source->streams[i].decoder_config = esd->decoderConfig->decoderSpecificInfo->data;
						source->streams[i].decoder_config_size = esd->decoderConfig->decoderSpecificInfo->dataLength;
						esd->decoderConfig->decoderSpecificInfo->data = NULL;
						esd->decoderConfig->decoderSpecificInfo->dataLength = 0;
						e = gf_m2ts_program_stream_update_ts_scale(&source->streams[i], esd->slConfig->timestampResolution);
						if (!e)
							source->audio_configured = 1;
						break;
					}
				}
				gf_odf_desc_del((GF_Descriptor *)esd);
			}

			/*overwrite timing as it is flushed to 0 on discontinuities*/
			ts += audio_discontinuity_offset;
		}
#endif
		i=0;
		while (i<source->nb_streams) {
			if (source->streams[i].output_ctrl==NULL) {
				fprintf(stderr, "MULTIPLEX NOT YET CREATED\n");
				return;
			}
			if (source->streams[i].stream_id == ESID) {
				GF_ESIStream *priv = (GF_ESIStream *)source->streams[i].input_udta;
				GF_ESIPacket pck;
				memset(&pck, 0, sizeof(GF_ESIPacket));
				pck.data = data;
				pck.data_len = size;
				pck.flags |= GF_ESI_DATA_HAS_CTS;
				pck.flags |= GF_ESI_DATA_HAS_DTS;
				pck.flags |= GF_ESI_DATA_AU_START;
				pck.flags |= GF_ESI_DATA_AU_END;
				if (ts) pck.cts = pck.dts = ts;

				if (priv->rap)
					pck.flags |= GF_ESI_DATA_AU_RAP;
				if (source->repeat || !priv->vers_inc) {
					pck.flags |= GF_ESI_DATA_REPEAT;
					fprintf(stderr, "RAP carousel from scene engine sent: ESID=%d - size=%d - ts="LLD"\n", ESID, size, ts);
				} else {
					if (ESID != AUDIO_DATA_ESID && ESID != VIDEO_DATA_ESID)	/*don't log A/V inputs*/
						fprintf(stderr, "Update from scene engine sent: ESID=%d - size=%d - ts="LLD"\n", ESID, size, ts);
				}
				source->streams[i].output_ctrl(&source->streams[i], GF_ESI_OUTPUT_DATA_DISPATCH, &pck);
				return;
			}
			i++;
		}
	}
	return;
}

//static gf_seng_callback * SampleCallBack = &mySampleCallBack;


static volatile Bool run = 1;

#ifndef GPAC_DISABLE_SENG
static GF_ESIStream * set_broadcast_params(M2TSSource *source, u16 esid, u32 period, u32 ts_delta, u16 aggregate_on_stream, Bool adjust_carousel_time, Bool force_rap, Bool aggregate_au, Bool discard_pending, Bool signal_rap, Bool signal_critical, Bool version_inc)
{
	u32 i=0;
	GF_ESIStream *priv=NULL;
	GF_ESInterface *esi=NULL;

	/*locate our stream*/
	if (esid) {
		while (i<source->nb_streams) {
			if (source->streams[i].stream_id == esid) {
				priv = (GF_ESIStream *)source->streams[i].input_udta;
				esi = &source->streams[i];
				break;
			}
			else {
				i++;
			}
		}
		/*TODO: stream not found*/
	}

	/*TODO - set/reset the ESID for the parsers*/
	if (!priv) return NULL;

	/*TODO - if discard is set, abort current carousel*/
	if (discard_pending) {
	}

	/*remember RAP flag*/
	priv->rap = signal_rap;
	priv->critical = signal_critical;
	priv->vers_inc = version_inc;

	priv->ts_delta = ts_delta;
	priv->adjust_carousel_time = adjust_carousel_time;

	/*change stream aggregation mode*/
	if ((aggregate_on_stream != (u16)-1) && (priv->aggregate_on_stream != aggregate_on_stream)) {
		gf_seng_enable_aggregation(source->seng, esid, aggregate_on_stream);
		priv->aggregate_on_stream = aggregate_on_stream;
	}
	/*change stream aggregation mode*/
	if (priv->aggregate_on_stream==esi->stream_id) {
		if (priv->aggregate_on_stream && (period!=(u32)-1) && (esi->repeat_rate != period)) {
			esi->repeat_rate = period;
		}
	} else {
		esi->repeat_rate = 0;
	}
	return priv;
}
#endif

#ifndef GPAC_DISABLE_SENG

static u32 seng_output(void *param)
{
	GF_Err e;
	u64 last_src_modif, mod_time;
	M2TSSource *source = (M2TSSource *)param;
	GF_SceneEngine *seng = source->seng;
#ifndef GPAC_DISABLE_PLAYER
	GF_SimpleDataDescriptor *audio_desc;
#endif
	Bool update_context=0;
	Bool force_rap, adjust_carousel_time, discard_pending, signal_rap, signal_critical, version_inc, aggregate_au;
	u32 period, ts_delta;
	u16 es_id, aggregate_on_stream;
	e = GF_OK;
	gf_sleep(2000); /*TODO: events instead? What are we waiting for?*/
	gf_seng_encode_context(seng, SampleCallBack);

	last_src_modif = source->bifs_src_name ? gf_file_modification_time(source->bifs_src_name) : 0;

	/*send the audio descriptor*/
#ifndef GPAC_DISABLE_PLAYER
	if (source->mpeg4_signaling==GF_M2TS_MPEG4_SIGNALING_FULL && audio_OD_stream_id!=(u32)-1) {
		audio_desc = source->streams[audio_OD_stream_id].input_udta;
		if (audio_desc && audio_desc->data) /*RTP/UDP + MP3 case*/
		{
			assert(audio_OD_stream_id != (u32)-1);
			assert(!aac_reader); /*incompatible with AAC*/
			source->repeat = 1;
			SampleCallBack(source, AUDIO_OD_ESID, audio_desc->data, audio_desc->size, 0/*gf_m2ts_get_sys_clock(muxer)*/);
			source->repeat = 0;
			gf_free(audio_desc->data);
			gf_free(audio_desc);
			source->streams[audio_OD_stream_id].input_udta = NULL;
		}
	}
#endif

	while (run) {
		if (!gf_prompt_has_input()) {
			if (source->bifs_src_name) {
				mod_time = gf_file_modification_time(source->bifs_src_name);
				if (mod_time != last_src_modif) {
					FILE *srcf;
					char flag_buf[201], *flag;
					fprintf(stderr, "Update file modified - processing\n");
					last_src_modif = mod_time;

					srcf = gf_fopen(source->bifs_src_name, "rt");
					if (!srcf) continue;

					/*checks if we have a broadcast config*/
					if (!fgets(flag_buf, 200, srcf))
						flag_buf[0] = '\0';
					gf_fclose(srcf);

					aggregate_au = force_rap = adjust_carousel_time = discard_pending = signal_rap = signal_critical = 0;
					version_inc = 1;
					period = -1;
					aggregate_on_stream = -1;
					ts_delta = 0;
					es_id = 0;

					/*find our keyword*/
					flag = strstr(flag_buf, "gpac_broadcast_config ");
					if (flag) {
						flag += strlen("gpac_broadcast_config ");
						/*move to next word*/
						while (flag && (flag[0]==' ')) flag++;

						while (1) {
							char *sep = strchr(flag, ' ');
							if (sep) sep[0] = 0;
							if (!strnicmp(flag, "esid=", 5)) {
								/*ESID on which the update is applied*/
								es_id = atoi(flag+5);
							} else if (!strnicmp(flag, "period=", 7)) {
								/*TODO: target period carousel for ESID ??? (ESID/carousel)*/
								period = atoi(flag+7);
							} else if (!strnicmp(flag, "ts=", 3)) {
								/*TODO: */
								ts_delta = atoi(flag+3);
							} else if (!strnicmp(flag, "carousel=", 9)) {
								/*TODO: why? => sends the update on carousel id specified by this argument*/
								aggregate_on_stream = atoi(flag+9);
							} else if (!strnicmp(flag, "restamp=", 8)) {
								/*CTS is updated when carouselled*/
								adjust_carousel_time = atoi(flag+8);
							} else if (!strnicmp(flag, "discard=", 8)) {
								/*when we receive several updates during a single carousel period, this attribute specifies whether the current update discard pending ones*/
								discard_pending = atoi(flag+8);
							} else if (!strnicmp(flag, "aggregate=", 10)) {
								/*Boolean*/
								aggregate_au = atoi(flag+10);
							} else if (!strnicmp(flag, "force_rap=", 10)) {
								/*TODO: */
								force_rap = atoi(flag+10);
							} else if (!strnicmp(flag, "rap=", 4)) {
								/*TODO: */
								signal_rap = atoi(flag+4);
							} else if (!strnicmp(flag, "critical=", 9)) {
								/*TODO: */
								signal_critical = atoi(flag+9);
							} else if (!strnicmp(flag, "vers_inc=", 9)) {
								/*Boolean to increment m2ts section version number*/
								version_inc = atoi(flag+9);
							}
							if (sep) {
								sep[0] = ' ';
								flag = sep+1;
							} else {
								break;
							}
						}

						set_broadcast_params(source, es_id, period, ts_delta, aggregate_on_stream, adjust_carousel_time, force_rap, aggregate_au, discard_pending, signal_rap, signal_critical, version_inc);
					}

					e = gf_seng_encode_from_file(seng, es_id, aggregate_au ? 0 : 1, source->bifs_src_name, SampleCallBack);
					if (e) {
						fprintf(stderr, "Processing command failed: %s\n", gf_error_to_string(e));
					} else
						gf_seng_aggregate_context(seng, 0);

					update_context=1;



				}
			}
			if (update_context) {
				source->repeat = 1;
				e = gf_seng_encode_context(seng, SampleCallBack);
				source->repeat = 0;
				update_context = 0;
			}

			gf_sleep(10);
		} else { /*gf_prompt_has_input()*/
			char c = gf_prompt_get_char();
			switch (c) {
			case 'u':
			{
				GF_Err e;
				char szCom[8192];
				fprintf(stderr, "Enter command to send:\n");
				fflush(stdin);
				szCom[0] = 0;
				if (1 > scanf("%[^\t\n]", szCom)) {
					fprintf(stderr, "No command has been properly entered, aborting.\n");
					break;
				}
				e = gf_seng_encode_from_string(seng, 0, 0, szCom, SampleCallBack);
				if (e) {
					fprintf(stderr, "Processing command failed: %s\n", gf_error_to_string(e));
				}
				update_context=1;
			}
			break;
			case 'p':
			{
				char rad[GF_MAX_PATH];
				fprintf(stderr, "Enter output file name - \"std\" for stderr: ");
				if (1 > scanf("%s", rad)) {
					fprintf(stderr, "No outfile name has been entered, aborting.\n");
					break;
				}
				e = gf_seng_save_context(seng, !strcmp(rad, "std") ? NULL : rad);
				fprintf(stderr, "Dump done (%s)\n", gf_error_to_string(e));
			}
			break;
			case 'q':
			{
				run = 0;
			}
			}
			e = GF_OK;
		}
	}


	return e ? 1 : 0;
}

void fill_seng_es_ifce(GF_ESInterface *ifce, u32 i, GF_SceneEngine *seng, u32 period)
{
	GF_Err e = GF_OK;
	u32 len;
	GF_ESIStream *stream;
	char *config_buffer = NULL;

	memset(ifce, 0, sizeof(GF_ESInterface));
	e = gf_seng_get_stream_config(seng, i, (u16*) &(ifce->stream_id), &config_buffer, &len, (u32*) &(ifce->stream_type), (u32*) &(ifce->object_type_indication), &(ifce->timescale));
	if (e) {
		fprintf(stderr, "Cannot set the stream config for stream %d to %d: %s\n", ifce->stream_id, period, gf_error_to_string(e));
	}

	ifce->repeat_rate = period;
	GF_SAFEALLOC(stream, GF_ESIStream);
	if (!stream) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_APP, ("Failed to allocate SENG input handler\n"));
		return;
	}

	stream->rap = 1;
	if (ifce->input_udta)
		gf_free(ifce->input_udta);
	ifce->input_udta = stream;

	//fprintf(stderr, "Caroussel period: %d\n", period);
//	e = gf_seng_set_carousel_time(seng, ifce->stream_id, period);
	if (e) {
		fprintf(stderr, "Cannot set carousel time on stream %d to %d: %s\n", ifce->stream_id, period, gf_error_to_string(e));
	}
	ifce->input_ctrl = seng_input_ctrl;

}
#endif

static Bool open_source(M2TSSource *source, char *src, u32 carousel_rate, u32 mpeg4_signaling, char *update, char *audio_input_ip, u16 audio_input_port, char *video_buffer, Bool force_real_time, u32 bifs_use_pes, const char *temi_url, Bool compute_max_size, Bool insert_ntp)
{
#ifndef GPAC_DISABLE_STREAMING
	GF_SDPInfo *sdp;
#endif

	memset(source, 0, sizeof(M2TSSource));
	source->mpeg4_signaling = mpeg4_signaling;

	/*open ISO file*/
#ifndef GPAC_DISABLE_ISOM
	if (gf_isom_probe_file(src)) {
		u32 i;
		u32 nb_tracks;
		Bool has_bifs_od = 0;
		Bool temi_assigned = 0;
		u32 first_audio = 0;
		u32 first_other = 0;
		s64 min_offset = 0;
		u32 min_offset_timescale = 0;
		source->mp4 = gf_isom_open(src, GF_ISOM_OPEN_READ, 0);
		source->nb_streams = 0;
		source->real_time = force_real_time;
		/*on MPEG-2 TS, carry 3GPP timed text as MPEG-4 Part17*/
		gf_isom_text_set_streaming_mode(source->mp4, 1);
		nb_tracks = gf_isom_get_track_count(source->mp4);

		for (i=0; i<nb_tracks; i++) {
			Bool check_deps = 0;
			if (gf_isom_get_media_type(source->mp4, i+1) == GF_ISOM_MEDIA_HINT)
				continue;

			fill_isom_es_ifce(source, &source->streams[i], source->mp4, i+1, bifs_use_pes, compute_max_size);
			if (min_offset > ((GF_ESIMP4 *)source->streams[i].input_udta)->ts_offset) {
				min_offset = ((GF_ESIMP4 *)source->streams[i].input_udta)->ts_offset;
				min_offset_timescale = source->streams[i].timescale;
			}

			switch(source->streams[i].stream_type) {
			case GF_STREAM_OD:
				has_bifs_od = 1;
				source->streams[i].repeat_rate = carousel_rate;
				break;
			case GF_STREAM_SCENE:
				has_bifs_od = 1;
				source->streams[i].repeat_rate = carousel_rate;
				break;
			case GF_STREAM_VISUAL:
				/*turn on image repeat*/
				switch (source->streams[i].object_type_indication) {
				case GPAC_OTI_IMAGE_JPEG:
				case GPAC_OTI_IMAGE_PNG:
					((GF_ESIMP4 *)source->streams[i].input_udta)->image_repeat_ms = carousel_rate;
					break;
				default:
					check_deps = 1;
					if (gf_isom_get_sample_count(source->mp4, i+1)>1) {
						/*get first visual stream as PCR*/
						if (!source->pcr_idx) {
							source->pcr_idx = i+1;
							if ((temi_id_1>=0) || (temi_id_2>=0)) {
								temi_assigned = GF_TRUE;
								((GF_ESIMP4 *)source->streams[i].input_udta)->timeline_id = (u32) ( (temi_id_1>=0) ? temi_id_1 + 1 : temi_id_2 + 1 );
								((GF_ESIMP4 *)source->streams[i].input_udta)->insert_ntp = insert_ntp;

								if (temi_url && (temi_id_1>=0))
									((GF_ESIMP4 *)source->streams[i].input_udta)->temi_url = temi_url;

								if (temi_id_1>=0) temi_id_1 = -1;
								else temi_id_2 = -1;
							}
						}
					}
					break;
				}
				break;
			case GF_STREAM_AUDIO:
				if (!first_audio) first_audio = i+1;
				check_deps = 1;
				break;
			default:
				/*log not supported stream type: %s*/
				break;
			}
			source->nb_streams++;
			if (gf_isom_get_sample_count(source->mp4, i+1)>1) first_other = i+1;

			if (check_deps) {
				u32 k;
				Bool found_dep = 0;
				for (k=0; k<nb_tracks; k++) {
					if (gf_isom_get_media_type(source->mp4, k+1) != GF_ISOM_MEDIA_OD)
						continue;

					/*this stream is not refered to by any OD, send as regular PES*/
					if (gf_isom_has_track_reference(source->mp4, k+1, GF_ISOM_REF_OD, gf_isom_get_track_id(source->mp4, i+1) )==1) {
						found_dep = 1;
						break;
					}
				}
				if (!found_dep) {
					source->streams[i].caps |= GF_ESI_STREAM_WITHOUT_MPEG4_SYSTEMS;
				}
			}
		}
		if (has_bifs_od && !source->mpeg4_signaling) source->mpeg4_signaling = GF_M2TS_MPEG4_SIGNALING_FULL;
		if ( !temi_assigned && first_audio && ((temi_id_1>=0) || (temi_id_2>=0) ) ) {
			((GF_ESIMP4 *)source->streams[first_audio-1].input_udta)->timeline_id = (u32) ( (temi_id_1>=0) ? temi_id_1 + 1 : temi_id_2 + 1 );
			((GF_ESIMP4 *)source->streams[first_audio-1].input_udta)->insert_ntp = insert_ntp;

			if (temi_url && (temi_id_1>=0) )
				((GF_ESIMP4 *)source->streams[first_audio-1].input_udta)->temi_url = temi_url;

			if (temi_id_1>=0) temi_id_1 = -1;
			else temi_id_2 = -1;
		}

		/*if no visual PCR found, use first audio*/
		if (!source->pcr_idx) source->pcr_idx = first_audio;
		if (!source->pcr_idx) source->pcr_idx = first_other;
		if (source->pcr_idx) {
			GF_ESIMP4 *priv;
			source->pcr_idx-=1;
			priv = source->streams[source->pcr_idx].input_udta;
			gf_isom_set_default_sync_track(source->mp4, priv->track);
		}

		if (min_offset < 0) {
			for (i=0; i<source->nb_streams; i++) {
				Double scale = source->streams[i].timescale;
				scale /= min_offset_timescale;
				((GF_ESIMP4 *)source->streams[i].input_udta)->ts_offset += (s64) (-min_offset * scale);
			}
		}

		source->iod = gf_isom_get_root_od(source->mp4);
		if (source->iod) {
			GF_ObjectDescriptor*iod = (GF_ObjectDescriptor*)source->iod;
			if (gf_list_count( ((GF_ObjectDescriptor*)source->iod)->ESDescriptors) == 0) {
				gf_odf_desc_del(source->iod);
				source->iod = NULL;
			} else {
				fprintf(stderr, "IOD found for program %s\n", src);

				/*if using 4over2, get rid of OD tracks*/
				if (source->mpeg4_signaling==GF_M2TS_MPEG4_SIGNALING_SCENE) {
					for (i=0; i<gf_list_count(iod->ESDescriptors); i++) {
						u32 track_num, k;
						GF_M2TSDescriptor *oddesc;
						GF_ISOSample *sample;
						GF_ESD *esd = gf_list_get(iod->ESDescriptors, i);
						if (esd->decoderConfig->streamType!=GF_STREAM_OD) continue;
						track_num = gf_isom_get_track_by_id(source->mp4, esd->ESID);
						if (gf_isom_get_sample_count(source->mp4, track_num)>1) continue;

						sample = gf_isom_get_sample(source->mp4, track_num, 1, NULL);
						if (sample->dataLength >= 255-2) {
							gf_isom_sample_del(&sample);
							continue;
						}
						/*rewrite ESD dependencies*/
						for (k=0; k<gf_list_count(iod->ESDescriptors); k++) {
							GF_ESD *dep_esd = gf_list_get(iod->ESDescriptors, k);
							if (dep_esd->dependsOnESID==esd->ESID) dep_esd->dependsOnESID = esd->dependsOnESID;
						}

						for (k=0; k<source->nb_streams; k++) {
							if (source->streams[k].stream_id==esd->ESID) {
								source->streams[k].stream_type = 0;
								break;
							}
						}

						if (!source->od_updates) source->od_updates = gf_list_new();
						GF_SAFEALLOC(oddesc, GF_M2TSDescriptor);
						oddesc->data_len = sample->dataLength;
						oddesc->data = sample->data;
						oddesc->tag = GF_M2TS_MPEG4_ODUPDATE_DESCRIPTOR;
						sample->data = NULL;
						gf_isom_sample_del(&sample);
						gf_list_add(source->od_updates, oddesc);

						gf_list_rem(iod->ESDescriptors, i);
						i--;
						gf_odf_desc_del((GF_Descriptor *) esd);
						source->samples_count--;
					}
				}

			}
		}
		return 1;
	}
#endif


#ifndef GPAC_DISABLE_STREAMING
	/*open SDP file*/
	if (strstr(src, ".sdp")) {
		GF_X_Attribute *att;
		char *sdp_buf;
		u32 sdp_size, i;
		GF_Err e;
		FILE *_sdp = gf_fopen(src, "rt");
		if (!_sdp) {
			fprintf(stderr, "Error opening %s - no such file\n", src);
			return 0;
		}
		gf_fseek(_sdp, 0, SEEK_END);
		sdp_size = (u32)gf_ftell(_sdp);
		gf_fseek(_sdp, 0, SEEK_SET);
		sdp_buf = (char*)gf_malloc(sizeof(char)*sdp_size);
		memset(sdp_buf, 0, sizeof(char)*sdp_size);
		sdp_size = (u32) fread(sdp_buf, 1, sdp_size, _sdp);
		gf_fclose(_sdp);

		sdp = gf_sdp_info_new();
		e = gf_sdp_info_parse(sdp, sdp_buf, sdp_size);
		gf_free(sdp_buf);
		if (e) {
			fprintf(stderr, "Error opening %s : %s\n", src, gf_error_to_string(e));
			gf_sdp_info_del(sdp);
			return 0;
		}

		i=0;
		while ((att = (GF_X_Attribute*)gf_list_enum(sdp->Attributes, &i))) {
			char buf[2000];
			u32 size;
			char *buf64;
			u32 size64;
			char *iod_str;
			if (strcmp(att->Name, "mpeg4-iod") ) continue;
			iod_str = att->Value + 1;
			if (strnicmp(iod_str, "data:application/mpeg4-iod;base64", strlen("data:application/mpeg4-iod;base64"))) continue;

			buf64 = strstr(iod_str, ",");
			if (!buf64) break;
			buf64 += 1;
			size64 = (u32) strlen(buf64) - 1;
			size = gf_base64_decode(buf64, size64, buf, 2000);

			gf_odf_desc_read(buf, size, &source->iod);
			break;
		}

		source->nb_streams = gf_list_count(sdp->media_desc);
		for (i=0; i<source->nb_streams; i++) {
			GF_SDPMedia *media = gf_list_get(sdp->media_desc, i);
			fill_rtp_es_ifce(&source->streams[i], media, sdp, source);
			switch(source->streams[i].stream_type) {
			case GF_STREAM_OD:
			case GF_STREAM_SCENE:
				source->mpeg4_signaling = GF_M2TS_MPEG4_SIGNALING_FULL;
				source->streams[i].repeat_rate = carousel_rate;
				break;
			}
			if (!source->pcr_idx && (source->streams[i].stream_type == GF_STREAM_VISUAL)) {
				source->pcr_idx = i+1;
			}
		}

		if (source->pcr_idx) source->pcr_idx-=1;
		gf_sdp_info_del(sdp);

		return 2;
	} else
#endif /*GPAC_DISABLE_STREAMING*/

#ifndef GPAC_DISABLE_SENG
		if (strstr(src, ".bt")) //open .bt file
		{
			u32 i;
			u32 load_type=0;
			source->seng = gf_seng_init(source, src, load_type, NULL, (load_type == GF_SM_LOAD_DIMS) ? 1 : 0);
			if (!source->seng) {
				fprintf(stderr, "Cannot create scene engine\n");
				exit(1);
			}
			else {
				fprintf(stderr, "Scene engine created.\n");
			}
			assert( source );
			assert( source->seng);
			source->iod = gf_seng_get_iod(source->seng);
			if (! source->iod) {
				fprintf(stderr, __FILE__": No IOD\n");
			}

			source->nb_streams = gf_seng_get_stream_count(source->seng);
			source->rate = carousel_rate;
			source->mpeg4_signaling = GF_M2TS_MPEG4_SIGNALING_FULL;

			for (i=0; i<source->nb_streams; i++) {
				fill_seng_es_ifce(&source->streams[i], i, source->seng, source->rate);
				//fprintf(stderr, "Fill interface\n");
				if (!source->pcr_idx && (source->streams[i].stream_type == GF_STREAM_AUDIO)) {
					source->pcr_idx = i+1;
				}
			}

#ifndef GPAC_DISABLE_PLAYER
			/*when an audio input is present, declare it and store OD + ESD_U*/
			if (audio_input_ip) {
				/*add the audio program*/
				source->pcr_idx = source->nb_streams;
				source->streams[source->nb_streams].stream_type = GF_STREAM_AUDIO;
				/*hack: http urls are not decomposed therefore audio_input_port remains null*/
				if (audio_input_port) {	/*UDP/RTP*/
					source->streams[source->nb_streams].object_type_indication = GPAC_OTI_AUDIO_MPEG1;
				} else { /*HTTP*/
					aac_reader->oti = source->streams[source->nb_streams].object_type_indication = GPAC_OTI_AUDIO_AAC_MPEG4;
				}
				source->streams[source->nb_streams].input_ctrl = void_input_ctrl;
				source->streams[source->nb_streams].stream_id = AUDIO_DATA_ESID;
				source->streams[source->nb_streams].timescale = 1000;

				GF_SAFEALLOC(source->streams[source->nb_streams].input_udta, GF_ESIStream);
				if (!source->streams[source->nb_streams].input_udta) {
					GF_LOG(GF_LOG_ERROR, GF_LOG_APP, ("Failed to allocate audio input handler\n"));
					return 0;
				}
				
				((GF_ESIStream*)source->streams[source->nb_streams].input_udta)->vers_inc = 1;	/*increment version number at every audio update*/
				assert( source );
				//assert( source->iod);
				if (source->iod && ((source->iod->tag!=GF_ODF_IOD_TAG) || (mpeg4_signaling != GF_M2TS_MPEG4_SIGNALING_SCENE))) {
					/*create the descriptor*/
					GF_ESD *esd;
					GF_SimpleDataDescriptor *audio_desc;
					GF_SAFEALLOC(audio_desc, GF_SimpleDataDescriptor);
					if (audio_input_port) {	/*UDP/RTP*/
						esd = gf_odf_desc_esd_new(0);
						esd->decoderConfig->streamType = source->streams[source->nb_streams].stream_type;
						esd->decoderConfig->objectTypeIndication = source->streams[source->nb_streams].object_type_indication;
					} else {				/*HTTP*/
						esd = AAC_GetESD(aac_reader);		/*in case of AAC, we have to wait the first ADTS chunk*/
					}
					assert( esd );
					esd->ESID = source->streams[source->nb_streams].stream_id;
					if (esd->slConfig->timestampResolution) /*in case of AAC, we have to wait the first ADTS chunk*/
						encode_audio_desc(esd, audio_desc);
					else
						gf_odf_desc_del((GF_Descriptor *)esd);

					/*find the audio OD stream and attach its descriptor*/
					for (i=0; i<source->nb_streams; i++) {
						if (source->streams[i].stream_id == AUDIO_OD_ESID) {
							if (source->streams[i].input_udta)
								gf_free(source->streams[i].input_udta);
							source->streams[i].input_udta = (void*)audio_desc;	/*Hack: the real input_udta type (for our SampleCallBack function) is GF_ESIStream*/
							audio_OD_stream_id = i;
							break;
						}
					}
					if (audio_OD_stream_id == (u32)-1) {
						fprintf(stderr, "Error: could not find an audio OD stream with ESID=100 in '%s'\n", src);
						return 0;
					}
				} else {
					source->mpeg4_signaling = GF_M2TS_MPEG4_SIGNALING_SCENE;
				}
				source->nb_streams++;
			}
#endif

			/*when an audio input is present, declare it and store OD + ESD_U*/
			if (video_buffer) {
				/*add the video program*/
				source->streams[source->nb_streams].stream_type = GF_STREAM_VISUAL;
				source->streams[source->nb_streams].object_type_indication = GPAC_OTI_VIDEO_AVC;
				source->streams[source->nb_streams].input_ctrl = void_input_ctrl;
				source->streams[source->nb_streams].stream_id = VIDEO_DATA_ESID;
				source->streams[source->nb_streams].timescale = 1000;

				GF_SAFEALLOC(source->streams[source->nb_streams].input_udta, GF_ESIStream);
				if (!source->streams[source->nb_streams].input_udta) {
					GF_LOG(GF_LOG_ERROR, GF_LOG_APP, ("Failed to allocate video input handler\n"));
					return 0;
				}
				((GF_ESIStream*)source->streams[source->nb_streams].input_udta)->vers_inc = 1;	/*increment version number at every video update*/
				assert(source);

				if (source->iod && ((source->iod->tag!=GF_ODF_IOD_TAG) || (mpeg4_signaling != GF_M2TS_MPEG4_SIGNALING_SCENE))) {
					assert(0); /*TODO*/
#if 0
					/*create the descriptor*/
					GF_ESD *esd;
					GF_SimpleDataDescriptor *video_desc;
					GF_SAFEALLOC(video_desc, GF_SimpleDataDescriptor);
					esd = gf_odf_desc_esd_new(0);
					esd->decoderConfig->streamType = source->streams[source->nb_streams].stream_type;
					esd->decoderConfig->objectTypeIndication = source->streams[source->nb_streams].object_type_indication;
					esd->ESID = source->streams[source->nb_streams].stream_id;

					/*find the audio OD stream and attach its descriptor*/
					for (i=0; i<source->nb_streams; i++) {
						if (source->streams[i].stream_id == 103/*TODO: VIDEO_OD_ESID*/) {
							if (source->streams[i].input_udta)
								gf_free(source->streams[i].input_udta);
							source->streams[i].input_udta = (void*)video_desc;
							audio_OD_stream_id = i;
							break;
						}
					}
					if (audio_OD_stream_id == (u32)-1) {
						fprintf(stderr, "Error: could not find an audio OD stream with ESID=100 in '%s'\n", src);
						return 0;
					}
#endif
				} else {
					assert (source->mpeg4_signaling == GF_M2TS_MPEG4_SIGNALING_SCENE);
				}

				source->nb_streams++;
			}

			if (!source->pcr_idx) source->pcr_idx=1;
			source->th = gf_th_new("Carousel");
			source->bifs_src_name = update;
			gf_th_run(source->th, seng_output, source);
			return 1;
		} else
#endif
		{
			FILE *f = gf_fopen(src, "rt");
			if (f) {
				gf_fclose(f);
				fprintf(stderr, "Error opening %s - not a supported input media, skipping.\n", src);
			} else {
				fprintf(stderr, "Error opening %s - no such file.\n", src);
			}
			return 0;
		}
}

#ifdef GPAC_MEMORY_TRACKING
GF_MemTrackerType mem_track = GF_MemTrackerNone;
#endif

/*macro to keep retro compatibility with '=' and spaces in parse_args*/
#define CHECK_PARAM(param) (!strnicmp(arg, param, strlen(param)) \
        && (   ((arg[strlen(param)] == '=') && (next_arg = arg+strlen(param)+1)) \
            || ((strlen(arg) == strlen(param)) && ++i && (i<argc) && (next_arg = argv[i]))))

/*parse MP42TS arguments*/
static GFINLINE GF_Err parse_args(int argc, char **argv, u32 *mux_rate, u32 *carrousel_rate, s64 *pcr_init_val, u32 *pcr_offset, u32 *psi_refresh_rate, GF_M2TS_PackMode *pes_packing_mode, u32 *bifs_use_pes,
                                  M2TSSource *sources, u32 *nb_sources, char **bifs_src_name,
                                  Bool *real_time, u32 *run_time, char **video_buffer, u32 *video_buffer_size,
                                  u32 *audio_input_type, char **audio_input_ip, u16 *audio_input_port,
                                  u32 *output_type, char **ts_out, char **udp_out, char **rtp_out, u16 *output_port,
                                  char** segment_dir, u32 *segment_duration, char **segment_manifest, u32 *segment_number, char **segment_http_prefix, u32 *split_rap, u32 *nb_pck_pack, u32 *pcr_ms, u32 *ttl, const char **ip_ifce, const char **temi_url, u32 *sdt_refresh_rate, Bool *enable_forced_pcr)
{
	Bool rate_found=0, mpeg4_carousel_found=0, time_found=0, src_found=0, dst_found=0, audio_input_found=0, video_input_found=0,
	     seg_dur_found=0, seg_dir_found=0, seg_manifest_found=0, seg_number_found=0, seg_http_found=0, real_time_found=0, insert_ntp=0;
	char *arg = NULL, *next_arg = NULL, *error_msg = "no argument found";
	u32 mpeg4_signaling = GF_M2TS_MPEG4_SIGNALING_NONE;
	Bool force_real_time = 0;
	s32 i;

	/*first pass: find audio - NO GPAC INIT MODIFICATION MUST OCCUR IN THIS PASS*/
	for (i=1; i<argc; i++) {
		arg = argv[i];
		if (!stricmp(arg, "-h") || strstr(arg, "-help")) {
			usage();
			return GF_EOS;
		}
		else if (CHECK_PARAM("-pcr-init")) {
			sscanf(next_arg, LLD, pcr_init_val);
		}
		else if (CHECK_PARAM("-pcr-offset")) {
			*pcr_offset = atoi(next_arg);
		}
		else if (CHECK_PARAM("-video")) {
			FILE *f;
			if (video_input_found) {
				error_msg = "multiple '-video' found";
				arg = NULL;
				goto error;
			}
			video_input_found = 1;
			f = gf_fopen(next_arg, "rb");
			if (!f) {
				error_msg = "video file not found: ";
				goto error;
			}
			gf_fseek(f, 0, SEEK_END);
			*video_buffer_size = (u32)gf_ftell(f);
			gf_fseek(f, 0, SEEK_SET);
			assert(*video_buffer_size);
			*video_buffer = (char*) gf_malloc(*video_buffer_size);
			{
				s32 read = (u32) fread(*video_buffer, sizeof(char), *video_buffer_size, f);
				if (read != *video_buffer_size)
					fprintf(stderr, "Error while reading video file, has readen %u chars instead of %u.\n", read, *video_buffer_size);
			}
			gf_fclose(f);
		} else if (CHECK_PARAM("-audio")) {
			if (audio_input_found) {
				error_msg = "multiple '-audio' found";
				arg = NULL;
				goto error;
			}
			audio_input_found = 1;
			if (!strnicmp(next_arg, "udp://", 6) || !strnicmp(next_arg, "rtp://", 6) || !strnicmp(next_arg, "http://", 7)) {
				char *sep;
				/*set audio input type*/
				if (!strnicmp(next_arg, "udp://", 6))
					*audio_input_type = GF_MP42TS_UDP;
				else if (!strnicmp(next_arg, "rtp://", 6))
					*audio_input_type = GF_MP42TS_RTP;
#ifndef GPAC_DISABLE_PLAYER
				else if (!strnicmp(next_arg, "http://", 7))
					*audio_input_type = GF_MP42TS_HTTP;
#endif
				/*http needs to get the complete URL*/
				switch(*audio_input_type) {
				case GF_MP42TS_UDP:
				case GF_MP42TS_RTP:
					sep = strchr(next_arg+6, ':');
					*real_time=1;
					if (sep) {
						*audio_input_port = atoi(sep+1);
						sep[0]=0;
						*audio_input_ip = gf_strdup(next_arg+6);
						sep[0]=':';
					} else {
						*audio_input_ip = gf_strdup(next_arg+6);
					}
					break;
#ifndef GPAC_DISABLE_PLAYER
				case GF_MP42TS_HTTP:
					/* No need to dup since it may come from argv */
					*audio_input_ip = next_arg;
					assert(audio_input_port != 0);
					break;
#endif
				default:
					assert(0);
				}
			}
		} else if (CHECK_PARAM("-psi-rate")) {
			*psi_refresh_rate = atoi(next_arg);
		} else if (!stricmp(arg, "-bifs-pes")) {
			*bifs_use_pes = 1;
		} else if (!stricmp(arg, "-bifs-pes-ex")) {
			*bifs_use_pes = 2;
		} else if (!stricmp(arg, "-mpeg4") || !stricmp(arg, "-4on2")) {
			mpeg4_signaling = GF_M2TS_MPEG4_SIGNALING_FULL;
		} else if (!stricmp(arg, "-4over2")) {
			mpeg4_signaling = GF_M2TS_MPEG4_SIGNALING_SCENE;
		} else if (!strcmp(arg, "-mem-track") || !strcmp(arg, "-mem-track-stack")) {
#ifdef GPAC_MEMORY_TRACKING
			gf_sys_close();
            mem_track = !strcmp(arg, "-mem-track-stack") ? GF_MemTrackerBackTrace : GF_MemTrackerSimple;
			gf_sys_init(mem_track);
			gf_log_set_tool_level(GF_LOG_MEMORY, GF_LOG_INFO);
#else
			fprintf(stderr, "WARNING - GPAC not compiled with Memory Tracker - ignoring \"%s\"\n", arg);
#endif
		} else if (CHECK_PARAM("-rate")) {
			if (rate_found) {
				error_msg = "multiple '-rate' found";
				arg = NULL;
				goto error;
			}
			rate_found = 1;
			*mux_rate = 1000*atoi(next_arg);
		} else if (CHECK_PARAM("-mpeg4-carousel")) {
			if (mpeg4_carousel_found) {
				error_msg = "multiple '-mpeg4-carousel' found";
				arg = NULL;
				goto error;
			}
			mpeg4_carousel_found = 1;
			*carrousel_rate = atoi(next_arg);
		} else if (!strnicmp(arg, "-real-time", 10)) {
			if (real_time_found) {
				goto error;
			}
			real_time_found = 1;
			*real_time = 1;
		} else if (CHECK_PARAM("-time")) {
			if (time_found) {
				error_msg = "multiple '-time' found";
				arg = NULL;
				goto error;
			}
			time_found = 1;
			*run_time = atoi(next_arg);
		} else if (!stricmp(arg, "-single-au")) {
			*pes_packing_mode = GF_M2TS_PACK_NONE;
		} else if (!stricmp(arg, "-multi-au")) {
			*pes_packing_mode = GF_M2TS_PACK_ALL;
		} else if (!stricmp(arg, "-rap")) {
			*split_rap = 1;
		} else if (!stricmp(arg, "-flush-rap")) {
			*split_rap = 2;
		} else if (!stricmp(arg, "-force-pcr-only")) {
			*enable_forced_pcr = GF_TRUE;
		} else if (CHECK_PARAM("-nb-pack")) {
			*nb_pck_pack = atoi(next_arg);
		} else if (CHECK_PARAM("-nb-pck")) {
			*nb_pck_pack = atoi(next_arg);
		} else if (CHECK_PARAM("-pcr-ms")) {
			*pcr_ms = atoi(next_arg);
		} else if (CHECK_PARAM("-ttl")) {
			*ttl = atoi(next_arg);
		} else if (CHECK_PARAM("-ifce")) {
			*ip_ifce = next_arg;
		} else if (CHECK_PARAM("-sdt-rate")) {
			*sdt_refresh_rate = atoi(next_arg);
		}
		else if (CHECK_PARAM("-logs")) {
			if (gf_log_set_tools_levels(next_arg) != GF_OK)
				return GF_BAD_PARAM;
		} else if (CHECK_PARAM("-lf")) {
			logfile = gf_fopen(next_arg, "wt");
			gf_log_set_callback(logfile, on_gpac_log);
		} else if (CHECK_PARAM("-segment-dir")) {
			if (seg_dir_found) {
				goto error;
			}
			seg_dir_found = 1;
			*segment_dir = next_arg;
			/* TODO: add the path separation char, if missing */
		} else if (CHECK_PARAM("-segment-duration")) {
			if (seg_dur_found) {
				goto error;
			}
			seg_dur_found = 1;
			*segment_duration = atoi(next_arg);
		} else if (CHECK_PARAM("-segment-manifest")) {
			if (seg_manifest_found) {
				goto error;
			}
			seg_manifest_found = 1;
			*segment_manifest = next_arg;
		} else if (CHECK_PARAM("-segment-http-prefix")) {
			if (seg_http_found) {
				goto error;
			}
			seg_http_found = 1;
			*segment_http_prefix = next_arg;
		} else if (CHECK_PARAM("-segment-number")) {
			if (seg_number_found) {
				goto error;
			}
			seg_number_found = 1;
			*segment_number = atoi(next_arg);
		}
		else if (CHECK_PARAM("-bifs-src")) {
			if (src_found) {
				error_msg = "multiple '-bifs-src' found";
				arg = NULL;
				goto error;
			}
			src_found = 1;
			*bifs_src_name = next_arg;
		} else if (CHECK_PARAM("-dst-file")) {
			dst_found = 1;
			*ts_out = gf_strdup(next_arg);
		} else if (CHECK_PARAM("-temi")) {
			if (next_arg[0]=='-') {
				*temi_url = NULL;
				i--;
				temi_id_1 = 150;
			} else {
				u32 temi_id = 0;
				if (sscanf(next_arg, "%d", &temi_id) == 1) {
					if (temi_id < 0x80 || temi_id>0xFF) {
						fprintf(stderr, "TEMI external timeline IDs shall be in the range [0x80, 0xFF], but %d was specified\n", temi_id);
						return GF_BAD_PARAM;
					}
				}
				if (!temi_id) {
					*temi_url = next_arg;
					if (strlen(next_arg) > 150) {
						fprintf(stderr, "URLs longer than 150 bytes are not currently supported\n");
						return GF_NOT_SUPPORTED;
					}
					temi_id_1 = 0;
				} else {
					temi_id_1 = temi_id;
					*temi_url = NULL;
				}
			}
		} else if (CHECK_PARAM("-temi2")) {
			u32 temi_id = 0;
			if (next_arg[0]=='-') {
				fprintf(stderr, "No ID for secondary external TEMI timeline specified\n");
				return GF_BAD_PARAM;
			}
			if (sscanf(next_arg, "%d", &temi_id) == 1) {
				if (temi_id < 0x80 || temi_id>0xFF) {
					fprintf(stderr, "TEMI external timeline IDs shall be in the range [0x80, 0xFF], but %d was specified\n", temi_id);
					return GF_BAD_PARAM;
				}
				temi_id_2 = temi_id;
			} else {
				fprintf(stderr, "No ID for secondary external TEMI timeline specified\n");
				return GF_BAD_PARAM;
			}
		} else if (CHECK_PARAM("-temi-delay")) {
			temi_url_insertion_delay = atoi(next_arg);
		} else if (CHECK_PARAM("-temi-offset")) {
			temi_offset = atoi(next_arg);
		} else if (!stricmp(arg, "-temi-noloop")) {
			temi_disable_loop = 1;
		} else if (!stricmp(arg, "-insert-ntp")) {
			insert_ntp = GF_TRUE;
		}
		else if (CHECK_PARAM("-dst-udp")) {
			char *sep = strchr(next_arg, ':');
			dst_found = 1;
			*real_time=1;
			if (sep) {
				*output_port = atoi(sep+1);
				sep[0]=0;
				*udp_out = gf_strdup(next_arg);
				sep[0]=':';
			} else {
				*udp_out = gf_strdup(next_arg);
			}
		}
		else if (CHECK_PARAM("-dst-rtp")) {
			char *sep = strchr(next_arg, ':');
			dst_found = 1;
			*real_time=1;
			if (sep) {
				*output_port = atoi(sep+1);
				sep[0]=0;
				*rtp_out = gf_strdup(next_arg);
				sep[0]=':';
			} else {
				*rtp_out = gf_strdup(next_arg);
			}
		} else if (CHECK_PARAM("-src")) { //second pass arguments
		} else if (CHECK_PARAM("-prog")) { //second pass arguments
		}
		else {
			error_msg = "unknown option";
			goto error;
		}
	}
	if (*real_time) force_real_time = 1;
	rate_found = 1;

	/*second pass: open sources*/
	for (i=1; i<argc; i++) {
		u32 res;
		char *src_args;
		arg = argv[i];
		if (arg[0] !='-') continue;

		if (! CHECK_PARAM("-src") && ! CHECK_PARAM("-prog") ) continue;

		src_args = strchr(next_arg, ':');
		if (src_args && (src_args[1]=='\\')) {
			src_args = strchr(src_args+2, ':');
		}
		if (src_args) {
			src_args[0] = 0;
			src_args = src_args + 1;
		}

		res = open_source(&sources[*nb_sources], next_arg, *carrousel_rate, mpeg4_signaling, *bifs_src_name, *audio_input_ip, *audio_input_port, *video_buffer, force_real_time, *bifs_use_pes, *temi_url, (*pcr_offset == (u32) -1) ? 1 : 0, insert_ntp);


		//we may have arguments
		while (src_args) {
			char *sep = strchr(src_args, ':');
			if (sep) sep[0] = 0;

			if (!strnicmp(src_args, "name=", 5)) {
				strncpy(sources[*nb_sources].program_name, src_args+5, 20);
			} else if (!strnicmp(src_args, "provider=", 9)) {
				strncpy(sources[*nb_sources].provider_name, src_args+9, 20);
			} else if (!strnicmp(src_args, "ID=", 3)) {
				u32 k;
				sources[*nb_sources].ID = atoi(src_args+3);

				for (k=0; k<*nb_sources; k++) {
					if (sources[k].ID == sources[*nb_sources].ID) {
						sources[*nb_sources].is_not_program_declaration = 1;
						if (sources[k].max_sample_size < sources[*nb_sources].max_sample_size)
							sources[k].max_sample_size = sources[*nb_sources].max_sample_size;

						break;
					}
				}
			}

			if (sep) {
				sep[0] = ':';
				src_args = sep+1;
			} else
				break;
		}

		if (res) {
			(*nb_sources)++;
			if (res==2) *real_time=1;
		}
	}
	/*syntax is correct; now testing the presence of mandatory arguments*/
	if (dst_found && *nb_sources && rate_found) {
		return GF_OK;
	} else {
		if (!dst_found)
			fprintf(stderr, "Error: Destination argument not found\n");
		if (! *nb_sources)
			fprintf(stderr, "Error: No Programs are available\n");
		usage();
		return GF_BAD_PARAM;
	}

error:
	if (!arg) {
		fprintf(stderr, "Error: %s\n", error_msg);
	} else {
		fprintf(stderr, "Error: %s \"%s\"\n", error_msg, arg);
	}
	return GF_BAD_PARAM;
}

static GF_Err write_manifest(char *manifest, char *segment_dir, u32 segment_duration, char *segment_prefix, char *http_prefix, u32 first_segment, u32 last_segment, Bool end)
{
	FILE *manifest_fp;
	u32 i;
	char manifest_tmp_name[GF_MAX_PATH];
	char manifest_name[GF_MAX_PATH];
	char *tmp_manifest = manifest_tmp_name;

	if (segment_dir) {
		sprintf(manifest_tmp_name, "%stmp.m3u8", segment_dir);
		sprintf(manifest_name, "%s%s", segment_dir, manifest);
	} else {
		sprintf(manifest_tmp_name, "tmp.m3u8");
		sprintf(manifest_name, "%s", manifest);
	}

	manifest_fp = gf_fopen(tmp_manifest, "w");
	if (!manifest_fp) {
		fprintf(stderr, "Could not create m3u8 manifest file (%s)\n", tmp_manifest);
		return GF_BAD_PARAM;
	}

	fprintf(manifest_fp, "#EXTM3U\n#EXT-X-TARGETDURATION:%u\n#EXT-X-MEDIA-SEQUENCE:%u\n", segment_duration, first_segment);

	for (i = first_segment; i <= last_segment; i++) {
		fprintf(manifest_fp, "#EXTINF:%u,\n%s%s_%u.ts\n", segment_duration, http_prefix, segment_prefix, i);
	}

	if (end) {
		fprintf(manifest_fp, "#EXT-X-ENDLIST\n");
	}
	gf_fclose(manifest_fp);

	if (!rename(tmp_manifest, manifest_name)) {
		return GF_OK;
	} else {
		if (remove(manifest_name)) {
			fprintf(stderr, "Error removing file %s\n", manifest_name);
			return GF_IO_ERR;
		} else if (rename(tmp_manifest, manifest_name)) {
			fprintf(stderr, "Could not rename temporary m3u8 manifest file (%s) into %s\n", tmp_manifest, manifest_name);
			return GF_IO_ERR;
		} else {
			return GF_OK;
		}
	}
}

int main(int argc, char **argv)
{
	/********************/
	/*   declarations   */
	/********************/
	const char *ts_pck;
	char *ts_pack_buffer = NULL;
	GF_Err e;
	u32 run_time;
	Bool real_time, is_stdout;
	s64 pcr_init_val = -1;
	u32 usec_till_next, ttl, split_rap, sdt_refresh_rate;
	GF_M2TS_PackMode pes_packing_mode;
	u32 i, j, mux_rate, nb_sources, cur_pid, carrousel_rate, last_print_time, last_video_time, bifs_use_pes, psi_refresh_rate, nb_pck_pack, nb_pck_in_pack, pcr_ms;
	char *ts_out = NULL, *udp_out = NULL, *rtp_out = NULL, *audio_input_ip = NULL;
	FILE *ts_output_file = NULL;
	GF_Socket *ts_output_udp_sk = NULL, *audio_input_udp_sk = NULL;
#ifndef GPAC_DISABLE_STREAMING
	GF_RTPChannel *ts_output_rtp = NULL;
	GF_RTSPTransport tr;
	GF_RTPHeader hdr;
#endif
	char *video_buffer;
	u32 video_buffer_size;
	u16 output_port = 0, audio_input_port = 0;
	u32 output_type, audio_input_type, pcr_offset;
	char *audio_input_buffer = NULL;
	u32 audio_input_buffer_length=65536;
	char *bifs_src_name;
	const char *insert_temi = 0;
	M2TSSource sources[MAX_MUX_SRC_PROG];
	u32 segment_duration, segment_index, segment_number;
	char segment_manifest_default[GF_MAX_PATH];
	char *segment_manifest, *segment_http_prefix, *segment_dir;
	char segment_prefix[GF_MAX_PATH];
	char segment_name[GF_MAX_PATH];
	const char *ip_ifce = NULL;
	GF_M2TS_Time prev_seg_time;
	GF_M2TS_Mux *muxer;
	Bool enable_forced_pcr = GF_FALSE;
	/*****************/
	/*   gpac init   */
	/*****************/
	gf_sys_init(GF_MemTrackerNone);
	gf_log_set_tool_level(GF_LOG_ALL, GF_LOG_WARNING);

	/***********************/
	/*   initialisations   */
	/***********************/
	real_time = 0;
	is_stdout = 0;
	ts_output_file = NULL;
	video_buffer = NULL;
	last_video_time = 0;
	audio_input_type = 0;
	sdt_refresh_rate = 0;
	ts_output_udp_sk = NULL;
	udp_out = NULL;
#ifndef GPAC_DISABLE_STREAMING
	ts_output_rtp = NULL;
	rtp_out = NULL;
#endif
	ts_out = NULL;
	bifs_src_name = NULL;
	nb_sources = 0;
	mux_rate = 0;
	run_time = 0;
	carrousel_rate = 500;
	output_port = 1234;
	segment_duration = 0;
	segment_number = 10; /* by default, we keep the 10 previous segments */
	segment_index = 0;
	segment_manifest = NULL;
	segment_http_prefix = NULL;
	segment_dir = NULL;
	prev_seg_time.sec = 0;
	prev_seg_time.nanosec = 0;
	video_buffer_size = 0;
	nb_pck_pack = 1;
	pcr_ms = 100;
#ifndef GPAC_DISABLE_PLAYER
	aac_reader = AAC_Reader_new();
#endif
	muxer = NULL;
	pes_packing_mode = GF_M2TS_PACK_AUDIO_ONLY;
	bifs_use_pes = 0;
	split_rap = 0;
	ttl = 1;
	psi_refresh_rate = GF_M2TS_PSI_DEFAULT_REFRESH_RATE;
	pcr_offset = (u32) -1;

	/***********************/
	/*   parse arguments   */
	/***********************/
	if (GF_OK != parse_args(argc, argv, &mux_rate, &carrousel_rate, &pcr_init_val, &pcr_offset, &psi_refresh_rate, &pes_packing_mode, &bifs_use_pes, sources, &nb_sources, &bifs_src_name,
	                        &real_time, &run_time, &video_buffer, &video_buffer_size,
	                        &audio_input_type, &audio_input_ip, &audio_input_port,
	                        &output_type, &ts_out, &udp_out, &rtp_out, &output_port,
	                        &segment_dir, &segment_duration, &segment_manifest, &segment_number, &segment_http_prefix, &split_rap, &nb_pck_pack, &pcr_ms, &ttl, &ip_ifce, &insert_temi, &sdt_refresh_rate, &enable_forced_pcr)) {
		goto exit;
	}

	if (run_time && !mux_rate) {
		fprintf(stderr, "Cannot specify TS run time for VBR multiplex - disabling run time\n");
		run_time = 0;
	}

	/***************************/
	/*   create mp42ts muxer   */
	/***************************/
	muxer = gf_m2ts_mux_new(mux_rate, psi_refresh_rate, real_time);
	if (!muxer) {
		fprintf(stderr, "Could not create the muxer. Aborting.\n");
		goto exit;
	}
	gf_m2ts_mux_use_single_au_pes_mode(muxer, pes_packing_mode);
	if (pcr_init_val>=0) gf_m2ts_mux_set_initial_pcr(muxer, (u64) pcr_init_val);
	gf_m2ts_mux_set_pcr_max_interval(muxer, pcr_ms);
	gf_m2ts_mux_enable_pcr_only_packets(muxer, enable_forced_pcr);


	if (ts_out != NULL) {
		if (segment_duration) {
			strcpy(segment_prefix, ts_out);
			if (segment_dir) {
				if (strchr("\\/", segment_name[strlen(segment_name)-1])) {
					sprintf(segment_name, "%s%s_%d.ts", segment_dir, segment_prefix, segment_index);
				} else {
					sprintf(segment_name, "%s/%s_%d.ts", segment_dir, segment_prefix, segment_index);
				}
			} else {
				sprintf(segment_name, "%s_%d.ts", segment_prefix, segment_index);
			}
			ts_out = gf_strdup(segment_name);
			if (!segment_manifest) {
				sprintf(segment_manifest_default, "%s.m3u8", segment_prefix);
				segment_manifest = segment_manifest_default;
			}
			//write_manifest(segment_manifest, segment_dir, segment_duration, segment_prefix, segment_http_prefix, segment_index, 0, 0);
		}
		if (!strcmp(ts_out, "stdout") || !strcmp(ts_out, "-") ) {
			ts_output_file = stdout;
			is_stdout = GF_TRUE;
		} else {
			ts_output_file = gf_fopen(ts_out, "wb");
			is_stdout = GF_FALSE;
		}
		if (!ts_output_file) {
			fprintf(stderr, "Error opening %s\n", ts_out);
			goto exit;
		}
	}
	if (udp_out != NULL) {
		ts_output_udp_sk = gf_sk_new(GF_SOCK_TYPE_UDP);
		if (gf_sk_is_multicast_address((char *)udp_out)) {
			e = gf_sk_setup_multicast(ts_output_udp_sk, (char *)udp_out, output_port, ttl, 0, (char *) ip_ifce);
		} else {
			e = gf_sk_bind(ts_output_udp_sk, ip_ifce, output_port, (char *)udp_out, output_port, GF_SOCK_REUSE_PORT);
		}
		if (e) {
			fprintf(stderr, "Error initializing UDP socket: %s\n", gf_error_to_string(e));
			goto exit;
		}
	}
#ifndef GPAC_DISABLE_STREAMING
	if (rtp_out != NULL) {
		ts_output_rtp = gf_rtp_new();
		gf_rtp_set_ports(ts_output_rtp, output_port);
		memset(&tr, 0, sizeof(GF_RTSPTransport));
		tr.IsUnicast = gf_sk_is_multicast_address((char *)rtp_out) ? 0 : 1;
		tr.Profile="RTP/AVP";
		tr.destination = (char *)rtp_out;
		tr.source = "0.0.0.0";
		tr.IsRecord = 0;
		tr.Append = 0;
		tr.SSRC = rand();
		tr.port_first = output_port;
		tr.port_last = output_port+1;
		if (tr.IsUnicast) {
			tr.client_port_first = output_port;
			tr.client_port_last = output_port+1;
		} else {
			tr.source = (char *)rtp_out;
			tr.TTL = ttl;
		}
		e = gf_rtp_setup_transport(ts_output_rtp, &tr, (char *)ts_out);
		if (e != GF_OK) {
			fprintf(stderr, "Cannot setup RTP transport info : %s\n", gf_error_to_string(e));
			goto exit;
		}
		e = gf_rtp_initialize(ts_output_rtp, 0, 1, 1500, 0, 0, (char *) ip_ifce);
		if (e != GF_OK) {
			fprintf(stderr, "Cannot initialize RTP sockets : %s\n", gf_error_to_string(e));
			goto exit;
		}
		memset(&hdr, 0, sizeof(GF_RTPHeader));
		hdr.Version = 2;
		hdr.PayloadType = 33;	/*MP2T*/
		hdr.SSRC = tr.SSRC;
		hdr.Marker = 0;
	}
#endif /*GPAC_DISABLE_STREAMING*/

	/************************************/
	/*   create streaming audio input   */
	/************************************/
	if (audio_input_ip)
		switch(audio_input_type) {
		case GF_MP42TS_UDP:
			audio_input_udp_sk = gf_sk_new(GF_SOCK_TYPE_UDP);
			if (gf_sk_is_multicast_address((char *)audio_input_ip)) {
				e = gf_sk_setup_multicast(audio_input_udp_sk, (char *)audio_input_ip, audio_input_port, 32, 0, NULL);
			} else {
				e = gf_sk_bind(audio_input_udp_sk, NULL, audio_input_port, (char *)audio_input_ip, audio_input_port, GF_SOCK_REUSE_PORT);
			}
			if (e) {
				fprintf(stderr, "Error initializing UDP socket for %s:%d : %s\n", audio_input_ip, audio_input_port, gf_error_to_string(e));
				goto exit;
			}
			gf_sk_set_buffer_size(audio_input_udp_sk, 0, GF_M2TS_UDP_BUFFER_SIZE);
			gf_sk_set_block_mode(audio_input_udp_sk, 0);

			/*allocate data buffer*/
			audio_input_buffer = (char*)gf_malloc(audio_input_buffer_length);
			assert(audio_input_buffer);
			break;
		case GF_MP42TS_RTP:
			/*TODO: not implemented*/
			assert(0);
			break;
#ifndef GPAC_DISABLE_PLAYER
		case GF_MP42TS_HTTP:
			audio_prog = (void*)&sources[nb_sources-1];
			aac_download_file(aac_reader, audio_input_ip);
			break;
#endif
		case GF_MP42TS_FILE:
			assert(0); /*audio live input is restricted to realtime/streaming*/
			break;
		default:
			assert(0);
		}

	if (!nb_sources) {
		fprintf(stderr, "No program to mux, quitting.\n");
	}

	for (i=0; i<nb_sources; i++) {
		if (!sources[i].ID) {
			for (j=i+1; j<nb_sources; j++) {
				if (sources[i].ID < sources[j].ID) sources[i].ID = sources[i].ID+1;
			}
			if (!sources[i].ID) sources[i].ID = 1;
		}
	}

	/****************************************/
	/*   declare all streams to the muxer   */
	/****************************************/
	cur_pid = 100;	/*PIDs start from 100*/
	for (i=0; i<nb_sources; i++) {
		GF_M2TS_Mux_Program *program;

		if (! sources[i].is_not_program_declaration) {
			u32 prog_pcr_offset = 0;
			if (pcr_offset==(u32)-1) {
				if (sources[i].max_sample_size && mux_rate) {
					Double r = sources[i].max_sample_size * 8;
					r *= 90000;
					r/= mux_rate;
					//add 10% of safety to cover TS signaling and other potential table update while sending the largest PES
					r *= 1.1;
					prog_pcr_offset = (u32) r;
				}
			} else {
				prog_pcr_offset = pcr_offset;
			}
			fprintf(stderr, "Setting up program ID %d - send rates: PSI %d ms PCR %d ms - PCR offset %d\n", sources[i].ID, psi_refresh_rate, pcr_ms, prog_pcr_offset);

			program = gf_m2ts_mux_program_add(muxer, sources[i].ID, cur_pid, psi_refresh_rate, prog_pcr_offset, sources[i].mpeg4_signaling);
			if (sources[i].mpeg4_signaling) program->iod = sources[i].iod;
			if (sources[i].od_updates) {
				program->loop_descriptors = sources[i].od_updates;
				sources[i].od_updates = NULL;
			}
		} else {
			program = gf_m2ts_mux_program_find(muxer, sources[i].ID);
		}
		if (!program) continue;

		for (j=0; j<sources[i].nb_streams; j++) {
			GF_M2TS_Mux_Stream *stream;
			Bool force_pes_mode = 0;
			/*likely an OD stream disabled*/
			if (!sources[i].streams[j].stream_type) continue;

			if (sources[i].streams[j].stream_type==GF_STREAM_SCENE) force_pes_mode = bifs_use_pes ? 1 : 0;

			stream = gf_m2ts_program_stream_add(program, &sources[i].streams[j], cur_pid+j+1, (sources[i].pcr_idx==j) ? 1 : 0, force_pes_mode);
			if (split_rap && (sources[i].streams[j].stream_type==GF_STREAM_VISUAL)) stream->start_pes_at_rap = 1;
		}

		cur_pid += sources[i].nb_streams;
		while (cur_pid % 10)
			cur_pid ++;

		if (sources[i].program_name[0] || sources[i].provider_name[0] ) gf_m2ts_mux_program_set_name(program, sources[i].program_name, sources[i].provider_name);
	}
	muxer->flush_pes_at_rap = (split_rap == 2) ? GF_TRUE : GF_FALSE;

	if (sdt_refresh_rate) {
		gf_m2ts_mux_enable_sdt(muxer, sdt_refresh_rate);
	}
	gf_m2ts_mux_update_config(muxer, 1);

	if (nb_pck_pack>1) {
		ts_pack_buffer = gf_malloc(sizeof(char) * 188 * nb_pck_pack);
	}

	/*****************/
	/*   main loop   */
	/*****************/
	last_print_time = gf_sys_clock();
	while (run) {
		u32 status;

		/*check for some audio input from the network*/
		if (audio_input_ip) {
			u32 read;
			switch (audio_input_type) {
			case GF_MP42TS_UDP:
			case GF_MP42TS_RTP:
				/*e =*/
				gf_sk_receive(audio_input_udp_sk, audio_input_buffer, audio_input_buffer_length, 0, &read);
				if (read) {
					SampleCallBack((void*)&sources[nb_sources-1], AUDIO_DATA_ESID, audio_input_buffer, read, gf_m2ts_get_sys_clock(muxer));
				}
				break;
#ifndef GPAC_DISABLE_PLAYER
			case GF_MP42TS_HTTP:
				/*nothing to do: AAC_OnLiveData is called automatically*/
				/*check we're still alive*/
				if (gf_dm_is_thread_dead(aac_reader->dnload)) {
					GF_ESD *esd;
					aac_download_file(aac_reader, audio_input_ip);
					esd = AAC_GetESD(aac_reader);
					if (!esd)
						break;
					assert(esd->slConfig->timestampResolution); /*if we don't have this value we won't be able to adjust the timestamps within the MPEG2-TS*/
					if (esd->slConfig->timestampResolution)
						audio_discontinuity_offset = gf_m2ts_get_sys_clock(muxer) * (u64)esd->slConfig->timestampResolution / 1000;
					gf_odf_desc_del((GF_Descriptor *)esd);
				}
				break;
#endif
			default:
				assert(0);
			}
		}

		/*flush all packets*/
		nb_pck_in_pack=0;
		while ((ts_pck = gf_m2ts_mux_process(muxer, &status, &usec_till_next)) != NULL) {

			if (ts_pack_buffer ) {
				memcpy(ts_pack_buffer + 188 * nb_pck_in_pack, ts_pck, 188);
				nb_pck_in_pack++;

				if (nb_pck_in_pack < nb_pck_pack)
					continue;

				ts_pck = (const char *) ts_pack_buffer;
			} else {
				nb_pck_in_pack = 1;
			}

call_flush:
			if (ts_output_file != NULL) {
				gf_fwrite(ts_pck, 1, 188 * nb_pck_in_pack, ts_output_file);
				if (segment_duration && (muxer->time.sec > prev_seg_time.sec + segment_duration)) {
					prev_seg_time = muxer->time;
					gf_fclose(ts_output_file);
					segment_index++;
					if (segment_dir) {
						if (strchr("\\/", segment_name[strlen(segment_name)-1])) {
							sprintf(segment_name, "%s%s_%d.ts", segment_dir, segment_prefix, segment_index);
						} else {
							sprintf(segment_name, "%s/%s_%d.ts", segment_dir, segment_prefix, segment_index);
						}
					} else {
						sprintf(segment_name, "%s_%d.ts", segment_prefix, segment_index);
					}
					ts_output_file = gf_fopen(segment_name, "wb");
					if (!ts_output_file) {
						fprintf(stderr, "Error opening %s\n", segment_name);
						goto exit;
					}
					/* delete the oldest segment */
					if (segment_number && ((s32) (segment_index - segment_number - 1) >= 0)) {
						char old_segment_name[GF_MAX_PATH];
						if (segment_dir) {
							if (strchr("\\/", segment_name[strlen(segment_name)-1])) {
								sprintf(old_segment_name, "%s%s_%d.ts", segment_dir, segment_prefix, segment_index - segment_number - 1);
							} else {
								sprintf(old_segment_name, "%s/%s_%d.ts", segment_dir, segment_prefix, segment_index - segment_number - 1);
							}
						} else {
							sprintf(old_segment_name, "%s_%d.ts", segment_prefix, segment_index - segment_number - 1);
						}
						gf_delete_file(old_segment_name);
					}
					write_manifest(segment_manifest, segment_dir, segment_duration, segment_prefix, segment_http_prefix,
//								   (segment_index >= segment_number/2 ? segment_index - segment_number/2 : 0), segment_index >1 ? segment_index-1 : 0, 0);
					               ( (segment_index > segment_number ) ? segment_index - segment_number : 0), segment_index >1 ? segment_index-1 : 0, 0);
				}
			}

			if (ts_output_udp_sk != NULL) {
				e = gf_sk_send(ts_output_udp_sk, (char*)ts_pck, 188 * nb_pck_in_pack);
				if (e) {
					fprintf(stderr, "Error %s sending UDP packet\n", gf_error_to_string(e));
				}
			}
#ifndef GPAC_DISABLE_STREAMING
			if (ts_output_rtp != NULL) {
				u32 ts;
				hdr.SequenceNumber++;
				/*muxer clock at 90k*/
				ts = muxer->time.sec*90000 + muxer->time.nanosec*9/100000;
				/*FIXME - better discontinuity check*/
				hdr.Marker = (ts < hdr.TimeStamp) ? 1 : 0;
				hdr.TimeStamp = ts;
				e = gf_rtp_send_packet(ts_output_rtp, &hdr, (char*)ts_pck, 188 * nb_pck_in_pack, 0);
				if (e) {
					fprintf(stderr, "Error %s sending RTP packet\n", gf_error_to_string(e));
				}
			}
#endif

			nb_pck_in_pack = 0;

			if (status>=GF_M2TS_STATE_PADDING) {
				break;
			}
		}
		if (nb_pck_in_pack) {
			ts_pck = (const char *) ts_pack_buffer;
			goto call_flush;
		}

		/*push video*/
		{
			u32 now=gf_sys_clock();
			if (now/MP42TS_VIDEO_FREQ != last_video_time/MP42TS_VIDEO_FREQ) {
				/*should use carrousel behaviour instead of being pushed manually*/
				if (video_buffer)
					SampleCallBack((void*)&sources[nb_sources-1], VIDEO_DATA_ESID, video_buffer, video_buffer_size, gf_m2ts_get_sys_clock(muxer)+1000/*try buffering due to VLC msg*/);
				last_video_time = now;
			}
		}

		if (real_time) {
			/*refresh every MP42TS_PRINT_TIME_MS ms*/
			u32 now=gf_sys_clock();
			if (now > last_print_time + MP42TS_PRINT_TIME_MS) {
				last_print_time = now;
				fprintf(stderr, "M2TS: time % 6d - TS time % 6d - bitrate % 8d\r", gf_m2ts_get_sys_clock(muxer), gf_m2ts_get_ts_clock(muxer), muxer->average_birate_kbps);

				if (gf_prompt_has_input()) {
					char c = gf_prompt_get_char();
					if (c=='q') break;
				}
			}
			if (status == GF_M2TS_STATE_IDLE) {
#if 0
				/*wait till next packet is ready to be sent*/
				if (usec_till_next>1000) {
					//fprintf(stderr, "%d usec till next packet\n", usec_till_next);
					gf_sleep(usec_till_next / 1000);
				}
#else
				//we don't have enough precision on usec counting and we end up eating one core on most machines, so let's just sleep
				//one second whenever we are idle - it's maybe too much but the muxer will catchup afterwards
				gf_sleep(1);
#endif
			}
		}


		if (run_time) {
			if (gf_m2ts_get_ts_clock(muxer) > run_time) {
				fprintf(stderr, "Stopping multiplex at %d ms (requested runtime %d ms)\n", gf_m2ts_get_ts_clock(muxer), run_time);
				break;
			}
		}
		if (status==GF_M2TS_STATE_EOS) {
			break;
		}
	}

	{
		u64 bits = muxer->tot_pck_sent*8*188;
		u64 dur_ms = gf_m2ts_get_ts_clock(muxer);
		if (!dur_ms) dur_ms = 1;
		fprintf(stderr, "Done muxing - %.02f sec - %sbitrate %d kbps "LLD" packets written\n", ((Double) dur_ms)/1000.0,mux_rate ? "" : "average ", (u32) (bits/dur_ms), muxer->tot_pck_sent);
		fprintf(stderr, " Padding: "LLD" packets (%g kbps) - "LLD" PES padded bytes (%g kbps)\n", muxer->tot_pad_sent, (Double) (muxer->tot_pad_sent*188*8.0/dur_ms) , muxer->tot_pes_pad_bytes, (Double) (muxer->tot_pes_pad_bytes*8.0/dur_ms) );
	}

exit:
	if (ts_pack_buffer) gf_free(ts_pack_buffer);
	run = 0;
	if (segment_duration) {
		write_manifest(segment_manifest, segment_dir, segment_duration, segment_prefix, segment_http_prefix, segment_index - segment_number, segment_index, 1);
	}
	if (ts_output_file && !is_stdout) gf_fclose(ts_output_file);
	if (ts_output_udp_sk) gf_sk_del(ts_output_udp_sk);
#ifndef GPAC_DISABLE_STREAMING
	if (ts_output_rtp) gf_rtp_del(ts_output_rtp);
#endif
	if (ts_out) gf_free(ts_out);
	if (audio_input_udp_sk) gf_sk_del(audio_input_udp_sk);
	if (audio_input_buffer) gf_free (audio_input_buffer);
	if (video_buffer) gf_free(video_buffer);
	if (udp_out) gf_free(udp_out);
#ifndef GPAC_DISABLE_STREAMING
	if (rtp_out) gf_free(rtp_out);
#endif
	if (muxer) gf_m2ts_mux_del(muxer);

	for (i=0; i<nb_sources; i++) {
		for (j=0; j<sources[i].nb_streams; j++) {
			if (sources[i].streams[j].input_ctrl) sources[i].streams[j].input_ctrl(&sources[i].streams[j], GF_ESI_INPUT_DESTROY, NULL);
			if (sources[i].streams[j].input_udta) {
				gf_free(sources[i].streams[j].input_udta);
			}
			if (sources[i].streams[j].decoder_config) {
				gf_free(sources[i].streams[j].decoder_config);
			}
			if (sources[i].streams[j].sl_config) {
				gf_free(sources[i].streams[j].sl_config);
			}
		}
		if (sources[i].iod) gf_odf_desc_del((GF_Descriptor*)sources[i].iod);
#ifndef GPAC_DISABLE_ISOM
		if (sources[i].mp4) gf_isom_close(sources[i].mp4);
#endif

#ifndef GPAC_DISABLE_SENG
		if (sources[i].seng) {
			gf_seng_terminate(sources[i].seng);
			sources[i].seng = NULL;
		}
#endif
		if (sources[i].th) gf_th_del(sources[i].th);
	}

#ifndef GPAC_DISABLE_PLAYER
	if (aac_reader) AAC_Reader_del(aac_reader);
#endif

	if (logfile) gf_fclose(logfile);
	gf_sys_close();

#ifdef GPAC_MEMORY_TRACKING
	if (mem_track && (gf_memory_size() || gf_file_handles_count() )) {
        gf_log_set_tool_level(GF_LOG_MEMORY, GF_LOG_INFO);
		gf_memory_print();
		return 2;
	}
#endif
	return 0;
}


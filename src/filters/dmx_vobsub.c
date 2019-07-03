/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2005-2017
 *					All rights reserved
 *
 *  This file is part of GPAC / NHNT demuxer filter
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

#ifndef GPAC_DISABLE_VOBSUB

#include <gpac/internal/vobsub.h>


typedef struct
{
	//opts
	Bool blankframe;

	GF_FilterPid *idx_pid, *sub_pid;
	GF_Filter *sub_filter;
	GF_List *opids;
	Bool first;

	u32 idx_file_crc;
	FILE *mdia;

	Double start_range;
	u64 first_dts;

	u32 nb_playing;
	GF_Fraction duration;
	Bool in_seek;

	Bool initial_play_done;
	Bool idx_parsed;

	u32 timescale;

	vobsub_file *vobsub;
} GF_VOBSubDmxCtx;


GF_Err vobsubdmx_configure_pid(GF_Filter *filter, GF_FilterPid *pid, Bool is_remove)
{
	u32 crc;
	const GF_PropertyValue *p;
	GF_VOBSubDmxCtx *ctx = gf_filter_get_udta(filter);

	p = gf_filter_pid_get_property(pid, GF_PROP_PID_URL);
	if (!p) {
		gf_filter_setup_failure(filter, GF_URL_ERROR);
		return GF_EOS;
	}

	if (is_remove) {
		u32 i, count;
		ctx->idx_pid = NULL;
		ctx->sub_pid = NULL;
		count = gf_filter_get_opid_count(filter);
		for (i=0; i<count; i++) {
			gf_filter_pid_remove( gf_filter_get_opid(filter, i) );
		}
		return GF_OK;
	}
	if (! gf_filter_pid_check_caps(pid))
		return GF_NOT_SUPPORTED;

	if (!ctx->idx_pid)
		ctx->idx_pid = pid;
	else if (ctx->idx_pid == pid)
		ctx->idx_pid = pid;
	else
		ctx->sub_pid = pid;

	gf_filter_pid_set_framing_mode(pid, GF_TRUE);

	if (ctx->idx_pid==pid) {
		GF_Err e;
		char sURL[GF_MAX_PATH], *ext;
		crc = gf_crc_32(p->value.string, (u32) strlen(p->value.string));
		if (ctx->idx_file_crc == crc) return GF_OK;
		ctx->idx_file_crc = crc;

		if (ctx->sub_filter) {
			gf_filter_remove_src(filter, ctx->sub_filter);
			ctx->sub_filter = NULL;
		}
		strcpy(sURL, p->value.string);
		ext = strrchr(sURL, '.');
		if (ext) ext[0] = 0;
		strcat(sURL, ".sub");

		ctx->sub_filter = gf_filter_connect_source(filter, sURL, NULL, &e);
		if (e) return e;
		if (ctx->mdia) gf_fclose(ctx->mdia);
		ctx->mdia = NULL;
		gf_filter_disable_probe(ctx->sub_filter);

		ctx->first = GF_TRUE;
	}

	return GF_OK;
}

static Bool vobsubdmx_process_event(GF_Filter *filter, const GF_FilterEvent *evt)
{
	GF_VOBSubDmxCtx *ctx = gf_filter_get_udta(filter);

	switch (evt->base.type) {
	case GF_FEVT_PLAY:
		if (ctx->nb_playing) return GF_TRUE;
		if (ctx->vobsub && (ctx->start_range != evt->play.start_range)) {
			u32 i;
			for (i=0; i<ctx->vobsub->num_langs; i++) {
				ctx->vobsub->langs[i].last_dts = 0;
				ctx->vobsub->langs[i].current = 0;
				ctx->vobsub->langs[i].is_seek = GF_TRUE;
			}
		}
		ctx->start_range = evt->play.start_range;
		ctx->nb_playing++;

		//cancel event
		return GF_TRUE;

	case GF_FEVT_STOP:
		ctx->nb_playing--;
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

GF_Err vobsubdmx_parse_idx(GF_Filter *filter, GF_VOBSubDmxCtx *ctx)
{
	FILE *file = NULL;
	int		  version;
	GF_Err err = GF_OK;
	const GF_PropertyValue *p;

	if (!ctx->idx_pid) return GF_SERVICE_ERROR;

	p = gf_filter_pid_get_property(ctx->idx_pid, GF_PROP_PID_FILEPATH);
	if (!p) {
		gf_filter_setup_failure(filter, GF_URL_ERROR);
		return GF_EOS;
	}

	file = gf_fopen(p->value.string, "rb");
	if (!file) {
		gf_filter_setup_failure(filter, GF_URL_ERROR);
		return GF_EOS;
	}

	GF_SAFEALLOC(ctx->vobsub, vobsub_file);
	if (!ctx->vobsub) {
		gf_fclose(file);
		gf_filter_setup_failure(filter, GF_URL_ERROR);
		return GF_EOS;
	}

	err = vobsub_read_idx(file, ctx->vobsub, &version);
	gf_fclose(file);
	if (err) {
		gf_filter_setup_failure(filter, GF_URL_ERROR);
		return GF_EOS;
	}
	if (!gf_filter_get_opid_count(filter) ) {
		u32 i;
		ctx->duration.num = 0;

		for (i=0; i<ctx->vobsub->num_langs; i++) {
			vobsub_pos *pos = (vobsub_pos*)gf_list_last(ctx->vobsub->langs[i].subpos);
			if (ctx->duration.num < pos->start*90)
				ctx->duration.num = (u32) (pos->start*90);
		}
		ctx->duration.den = 90000;

		for (i=0; i<ctx->vobsub->num_langs; i++) {
			GF_FilterPid *opid = gf_filter_pid_new(filter);

			//copy properties from idx pid
			gf_filter_pid_copy_properties(opid, ctx->idx_pid);

			gf_filter_pid_set_property(opid, GF_PROP_PID_ID, &PROP_UINT(i+1) );
			gf_filter_pid_set_property(opid, GF_PROP_PID_STREAM_TYPE, &PROP_UINT(GF_STREAM_TEXT) );
			gf_filter_pid_set_property(opid, GF_PROP_PID_CODECID, &PROP_UINT(GF_CODECID_SUBPIC) );
			gf_filter_pid_set_property(opid, GF_PROP_PID_TIMESCALE, &PROP_UINT(90000) );
			gf_filter_pid_set_property(opid, GF_PROP_PID_DECODER_CONFIG, &PROP_DATA((char*)&ctx->vobsub->palette[0][0], sizeof(ctx->vobsub->palette)) );


			gf_filter_pid_set_property(opid, GF_PROP_PID_WIDTH, &PROP_UINT(ctx->vobsub->width) );
			gf_filter_pid_set_property(opid, GF_PROP_PID_HEIGHT, &PROP_UINT(ctx->vobsub->height) );
			gf_filter_pid_set_property(opid, GF_PROP_PID_LANGUAGE, &PROP_STRING(ctx->vobsub->langs[i].name) );
			gf_filter_pid_set_property(opid, GF_PROP_PID_DURATION, &PROP_FRAC(ctx->duration) );

			gf_filter_pid_set_udta(opid, &ctx->vobsub->langs[i]);
		}
	}
	return GF_OK;
}

static GF_Err vobsubdmx_send_stream(GF_VOBSubDmxCtx *ctx, GF_FilterPid *pid)
{
	static const u8 null_subpic[] = { 0x00, 0x09, 0x00, 0x04, 0x00, 0x00, 0x00, 0x04, 0xFF };
	GF_List *subpic;
	vobsub_lang *vslang;
	u32 c, count;
	GF_FilterPacket *dst_pck;

	unsigned char buf[0x800];

	vslang = gf_filter_pid_get_udta(pid);
	subpic = vslang->subpos;

	count = gf_list_count(subpic);

	c = vslang->current;

	for (; c<count; c++) {
		u32		i, left, size, psize, dsize, hsize, duration;
		u8 *packet;
		vobsub_pos *pos = (vobsub_pos*)gf_list_get(subpic, c);

		if (vslang->is_seek) {
			if (pos->start*90 < ctx->start_range * 90000) {
				continue;
			}
			vslang->is_seek = GF_FALSE;
		}

		gf_fseek(ctx->mdia, pos->filepos, SEEK_SET);
		if (gf_ftell(ctx->mdia) != pos->filepos) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_PARSER, ("[VobSub] Could not seek in file\n"));
			return GF_IO_ERR;
		}

		if (!fread(buf, sizeof(buf), 1, ctx->mdia)) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_PARSER, ("[VobSub] Could not read from file\n"));
			return GF_IO_ERR;
		}

		if (*(u32*)&buf[0x00] != 0xba010000		   ||
		        *(u32*)&buf[0x0e] != 0xbd010000		   ||
		        !(buf[0x15] & 0x80)				   ||
		        (buf[0x17] & 0xf0) != 0x20			   ||
		        (buf[buf[0x16] + 0x17] & 0xe0) != 0x20)
		{
			GF_LOG(GF_LOG_ERROR, GF_LOG_PARSER, ("[VobSub] Corrupted data found in file (1)\n"));
			return GF_CORRUPTED_DATA;
		}

		psize = (buf[buf[0x16] + 0x18] << 8) + buf[buf[0x16] + 0x19];
		dsize = (buf[buf[0x16] + 0x1a] << 8) + buf[buf[0x16] + 0x1b];

		if (ctx->blankframe && !c && (pos->start>0)) {
			dst_pck = gf_filter_pck_new_alloc(pid, sizeof(null_subpic), &packet);
			if (!dst_pck) {
				GF_LOG(GF_LOG_ERROR, GF_LOG_PARSER, ("[VobSub] Memory allocation failed\n"));
				return GF_OUT_OF_MEM;
			}
			memcpy(packet, null_subpic, sizeof(null_subpic));
			gf_filter_pck_set_cts(dst_pck, 0);
			gf_filter_pck_set_sap(dst_pck, GF_FILTER_SAP_1);
			gf_filter_pck_set_duration(dst_pck, (u32) (pos->start * 90) );
			gf_filter_pck_send(dst_pck);
		}

		dst_pck = gf_filter_pck_new_alloc(pid, psize, &packet);
		if (!dst_pck) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_PARSER, ("[VobSub] Memory allocation failed\n"));
			return GF_OUT_OF_MEM;
		}

		for (i = 0, left = psize; i < psize; i += size, left -= size) {
			hsize = 0x18 + buf[0x16];
			size  = MIN(left, 0x800 - hsize);
			memcpy(packet + i, buf + hsize, size);

			if (size != left) {
				while (fread(buf, 1, sizeof(buf), ctx->mdia)) {
					if (buf[buf[0x16] + 0x17] == (vslang->idx | 0x20)) {
						break;
					}
				}
			}
		}

		if (i != psize || left > 0) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_PARSER, ("[VobSub] Corrupted data found in file (2)\n"));
			return GF_CORRUPTED_DATA;
		}

		if (vobsub_get_subpic_duration(packet, psize, dsize, &duration) != GF_OK) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_PARSER, ("[VobSub] Corrupted data found in file (3)\n"));
			return GF_CORRUPTED_DATA;
		}

		gf_filter_pck_set_cts(dst_pck, pos->start * 90);
		gf_filter_pck_set_sap(dst_pck, GF_FILTER_SAP_1);
		gf_filter_pck_set_duration(dst_pck, duration);

		if (vslang->last_dts && (vslang->last_dts >= pos->start * 90)) {
			GF_LOG(GF_LOG_WARNING, GF_LOG_PARSER, ("[VobSub] Out of order timestamps in vobsub file\n"));
		}
		gf_filter_pck_send(dst_pck);
		vslang->last_dts = pos->start * 90;

		vslang->current++;
		if (gf_filter_pid_would_block(pid)) return GF_OK;
	}
	return GF_EOS;
}

GF_Err vobsubdmx_process(GF_Filter *filter)
{
	GF_VOBSubDmxCtx *ctx = gf_filter_get_udta(filter);
	const GF_PropertyValue *p;
	GF_FilterPacket *pck;
	u32 pkt_size, i, count, nb_eos;
	Bool start, end;

	if (!ctx->idx_parsed) {
		GF_Err e;
		pck = gf_filter_pid_get_packet(ctx->idx_pid);
		if (!pck) return GF_OK;
		gf_filter_pck_get_framing(pck, &start, &end);
		//for now we only work with complete files
		assert(end);

		e = vobsubdmx_parse_idx(filter, ctx);
		ctx->idx_parsed = GF_TRUE;
		gf_filter_pid_drop_packet(ctx->idx_pid);
		if (e) {
			gf_filter_setup_failure(filter, e);
			return e;
		}
	}

	if (!ctx->nb_playing) return GF_OK;

	if (!ctx->mdia) {
		if (!ctx->sub_pid) return GF_OK;
		p = gf_filter_pid_get_property(ctx->sub_pid, GF_PROP_PID_FILEPATH);
		if (!p) {
			gf_filter_setup_failure(filter, GF_URL_ERROR);
			GF_LOG(GF_LOG_ERROR, GF_LOG_PARSER, ("[VobSub] Cannot open sub file\n"));
			return GF_EOS;
		}
		ctx->mdia = gf_fopen(p->value.string, "rb");
	}

	pck = gf_filter_pid_get_packet(ctx->sub_pid);
	if (!pck) {
		if (gf_filter_pid_is_eos(ctx->sub_pid)) return GF_EOS;
		return GF_OK;
	}

	/*data =*/ gf_filter_pck_get_data(pck, &pkt_size);
	gf_filter_pck_get_framing(pck, &start, &end);
	//for now we only work with complete files
	assert(end);

	nb_eos = 0;
	count = gf_filter_get_opid_count(filter);
	for (i=0; i<count; i++) {
		GF_FilterPid *opid = gf_filter_get_opid(filter, i);
		GF_Err e = vobsubdmx_send_stream(ctx, opid);
		if (e==GF_EOS) {
			nb_eos++;
			gf_filter_pid_set_eos(opid);
		}
	}

	if (nb_eos==count) {
		//only drop packet once we are done
		gf_filter_pid_drop_packet(ctx->sub_pid);
		return GF_EOS;
	}
	return GF_OK;
}

static void vobsubdmx_finalize(GF_Filter *filter)
{
	GF_VOBSubDmxCtx *ctx = gf_filter_get_udta(filter);
	if (ctx->vobsub) vobsub_free(ctx->vobsub);
	if (ctx->mdia) gf_fclose(ctx->mdia);
}

static const char * vobsubdmx_probe_data(const u8 *data, u32 size, GF_FilterProbeScore *score)
{
	if (!strncmp(data, "# VobSub", 8)) {
		*score = GF_FPROBE_SUPPORTED;
		return "text/vobsub";
	}
	return NULL;
}

#define OFFS(_n)	#_n, offsetof(GF_VOBSubDmxCtx, _n)
static const GF_FilterArgs GF_VOBSubDmxArgs[] =
{
	{ OFFS(blankframe), "force inserting a blank frame if first subpic is not at 0", GF_PROP_BOOL, "true", NULL, GF_FS_ARG_HINT_ADVANCED},
	{0}
};


static const GF_FilterCapability VOBSubDmxCaps[] =
{
	CAP_UINT(GF_CAPS_INPUT, GF_PROP_PID_STREAM_TYPE, GF_STREAM_FILE),
	CAP_STRING(GF_CAPS_INPUT, GF_PROP_PID_MIME, "text/vobsub"),
	CAP_UINT(GF_CAPS_OUTPUT_STATIC, GF_PROP_PID_STREAM_TYPE, GF_STREAM_TEXT),
	CAP_UINT(GF_CAPS_OUTPUT_STATIC, GF_PROP_PID_CODECID, GF_CODECID_SUBPIC),
	{0},
	CAP_UINT(GF_CAPS_INPUT, GF_PROP_PID_STREAM_TYPE, GF_STREAM_FILE),
	CAP_STRING(GF_CAPS_INPUT, GF_PROP_PID_FILE_EXT, "idx|sub"),
};


GF_FilterRegister VOBSubDmxRegister = {
	.name = "vobsubdmx",
	GF_FS_SET_DESCRIPTION("VobSub demuxer")
	GF_FS_SET_HELP("This filter demultiplexes VobSub files/data to produce media PIDs and frames.")
	.private_size = sizeof(GF_VOBSubDmxCtx),
	.max_extra_pids = 1,
	.args = GF_VOBSubDmxArgs,
	.finalize = vobsubdmx_finalize,
	SETCAPS(VOBSubDmxCaps),
	.configure_pid = vobsubdmx_configure_pid,
	.process = vobsubdmx_process,
	.probe_data = vobsubdmx_probe_data,
	.process_event = vobsubdmx_process_event
};

#endif

const GF_FilterRegister *vobsubdmx_register(GF_FilterSession *session)
{
#ifndef GPAC_DISABLE_VOBSUB
	return &VOBSubDmxRegister;
#else
	return NULL;
#endif

}


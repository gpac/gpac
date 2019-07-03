/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2018
 *					All rights reserved
 *
 *  This file is part of GPAC / RAW video (YUV,RGB) reframer filter
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

#include <gpac/avparse.h>
#include <gpac/constants.h>
#include <gpac/filters.h>

typedef struct
{
	//opts
	GF_PropVec2i size;
	GF_PixelFormat spfmt;
	GF_Fraction fps;
	Bool copy;

	//only one input pid declared
	GF_FilterPid *ipid;
	//only one output pid declared
	GF_FilterPid *opid;

	Bool file_loaded, is_playing, initial_play_done;
	u64 cts;
	u32 frame_size, nb_bytes_in_frame;
	u64 filepos, total_frames;
	GF_FilterPacket *out_pck;
	u8 *out_data;
	Bool reverse_play, done;
} GF_RawVidReframeCtx;




GF_Err rawvidreframe_configure_pid(GF_Filter *filter, GF_FilterPid *pid, Bool is_remove)
{
	const GF_PropertyValue *p;
	u32 stride, stride_uv;
	GF_RawVidReframeCtx *ctx = gf_filter_get_udta(filter);

	if (is_remove) {
		ctx->ipid = NULL;
		gf_filter_pid_remove(ctx->opid);
		return GF_OK;
	}
	if (! gf_filter_pid_check_caps(pid))
		return GF_NOT_SUPPORTED;

	ctx->ipid = pid;
	if (!ctx->spfmt) {
		p = gf_filter_pid_get_property(ctx->ipid, GF_PROP_PID_FILE_EXT);
		if (p && p->value.string) ctx->spfmt = gf_pixel_fmt_parse(p->value.string);
	}
	if (!ctx->spfmt) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_MEDIA, ("[RawVidReframe] Missing pixel format, cannot parse\n"));
		return GF_BAD_PARAM;
	}
	if (!ctx->size.x || !ctx->size.y) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_MEDIA, ("[RawVidReframe] Missing video frame size, cannot parse\n"));
		return GF_BAD_PARAM;
	}

	stride = stride_uv = 0;
	if (! gf_pixel_get_size_info(ctx->spfmt, ctx->size.x, ctx->size.y, &ctx->frame_size, &stride, &stride_uv, NULL, NULL)) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_MEDIA, ("[RawVidReframe] Failed to query pixel format size info\n"));
		return GF_BAD_PARAM;
	}

	if (!ctx->opid)
		ctx->opid = gf_filter_pid_new(filter);

	gf_filter_pid_copy_properties(ctx->opid, ctx->ipid);
	gf_filter_pid_set_framing_mode(ctx->ipid, GF_FALSE);
	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_STREAM_TYPE, &PROP_UINT(GF_STREAM_VISUAL));
	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_CODECID, &PROP_UINT(GF_CODECID_RAW));
	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_WIDTH, &PROP_UINT(ctx->size.x));
	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_HEIGHT, &PROP_UINT(ctx->size.y));
	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_PIXFMT, &PROP_UINT(ctx->spfmt));
	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_FPS, &PROP_FRAC(ctx->fps));
	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_TIMESCALE, &PROP_UINT(ctx->fps.num));
	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_STRIDE, &PROP_UINT(stride));
	if (stride_uv)
		gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_STRIDE_UV, &PROP_UINT(stride_uv));
	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_PLAYBACK_MODE, &PROP_UINT(GF_PLAYBACK_MODE_REWIND));
	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_CAN_DATAREF, &PROP_BOOL(GF_TRUE));


	p = gf_filter_pid_get_property(ctx->ipid, GF_PROP_PID_FILE_CACHED);
	if (p && p->value.boolean) ctx->file_loaded = GF_TRUE;

	p = gf_filter_pid_get_property(ctx->ipid, GF_PROP_PID_DOWN_SIZE);
	if (p && p->value.longuint) {
		u64 nb_frames = p->value.longuint;
		nb_frames /= ctx->frame_size;
		ctx->total_frames = nb_frames;
		nb_frames *= ctx->fps.den;
		gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_DURATION, &PROP_FRAC_INT((u32) nb_frames, ctx->fps.num));
	}

	if (!ctx->copy) {
		gf_filter_pid_set_property_str(ctx->opid, "BufferLength", &PROP_UINT(0));
	}
	return GF_OK;
}

static Bool rawvidreframe_process_event(GF_Filter *filter, const GF_FilterEvent *evt)
{
	u32 nb_frames;
	GF_FilterEvent fevt;
	GF_RawVidReframeCtx *ctx = gf_filter_get_udta(filter);

	switch (evt->base.type) {
	case GF_FEVT_PLAY:
		if (!ctx->is_playing) {
			ctx->is_playing = GF_TRUE;
			ctx->cts = 0;
		}
		ctx->done = GF_FALSE;
		if (evt->play.start_range>=0) {
			nb_frames = (u32) (evt->play.start_range * ctx->fps.num);
		} else {
			nb_frames = (u32) (ctx->cts / ctx->fps.den);
		}
		if (nb_frames>=ctx->total_frames)
			nb_frames = (u32) ctx->total_frames-1;

		ctx->cts = nb_frames * ctx->fps.den;
		ctx->filepos = nb_frames * ctx->frame_size;
		ctx->reverse_play =  (evt->play.speed<0) ? GF_TRUE : GF_FALSE;

		//post a seek even for the begining, to try to load frame by frame
		GF_FEVT_INIT(fevt, GF_FEVT_SOURCE_SEEK, ctx->ipid);
		fevt.seek.start_offset = ctx->filepos;
		fevt.seek.hint_block_size = ctx->frame_size;
		gf_filter_pid_send_event(ctx->ipid, &fevt);

		//cancel event
		return GF_TRUE;

	case GF_FEVT_STOP:
		//don't cancel event
		ctx->is_playing = GF_FALSE;
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

GF_Err rawvidreframe_process(GF_Filter *filter)
{
	GF_RawVidReframeCtx *ctx = gf_filter_get_udta(filter);
	GF_FilterPacket *pck;
	u64 byte_offset;
	char *data;
	u32 pck_size;

	if (ctx->done) return GF_EOS;

	if (!ctx->is_playing && ctx->opid) return GF_OK;

	pck = gf_filter_pid_get_packet(ctx->ipid);
	if (!pck) {
		if (gf_filter_pid_is_eos(ctx->ipid) && !ctx->reverse_play) {
			if (ctx->out_pck) {
				gf_filter_pck_send(ctx->out_pck);
				ctx->out_pck = NULL;
			}
			gf_filter_pid_set_eos(ctx->opid);
			return GF_EOS;
		}
		return GF_OK;
	}

	data = (char *) gf_filter_pck_get_data(pck, &pck_size);
	byte_offset = gf_filter_pck_get_byte_offset(pck);

	while (pck_size) {
		Bool use_ref = GF_FALSE;
		if (!ctx->out_pck) {
			assert(! ctx->nb_bytes_in_frame);
			if (!ctx->copy && (pck_size >= ctx->frame_size)) {
				ctx->out_pck = gf_filter_pck_new_ref(ctx->opid, data, ctx->frame_size, pck);
				use_ref = GF_TRUE;
			} else {
				ctx->out_pck = gf_filter_pck_new_alloc(ctx->opid, ctx->frame_size, &ctx->out_data);
			}
			if (!ctx->out_pck) return GF_OUT_OF_MEM;

			gf_filter_pck_set_cts(ctx->out_pck, ctx->cts);
			gf_filter_pck_set_sap(ctx->out_pck, GF_FILTER_SAP_1);
			gf_filter_pck_set_duration(ctx->out_pck, ctx->fps.den);
			gf_filter_pck_set_byte_offset(ctx->out_pck, byte_offset);
		}

		if (pck_size + ctx->nb_bytes_in_frame < ctx->frame_size) {
			memcpy(ctx->out_data + ctx->nb_bytes_in_frame, data, pck_size);
			ctx->nb_bytes_in_frame += pck_size;
			pck_size = 0;
		} else {
			u32 remain = ctx->frame_size - ctx->nb_bytes_in_frame;
			if (!use_ref) {
				memcpy(ctx->out_data + ctx->nb_bytes_in_frame, data, remain);
			}
			gf_filter_pck_send(ctx->out_pck);

			pck_size -= remain;
			data += remain;
			byte_offset += remain;
			ctx->out_pck = NULL;
			ctx->nb_bytes_in_frame = 0;

			//reverse playback, the remaining data is for the next frame, we want the previous one.
			//Trash packet and seek to previous frame
			if (ctx->reverse_play) {
				GF_FilterEvent fevt;
				if (!ctx->cts) {
					gf_filter_pid_set_eos(ctx->opid);
					ctx->done = GF_TRUE;
					return GF_EOS;
				}
				ctx->cts -= ctx->fps.den;
				ctx->filepos -= ctx->frame_size;
				gf_filter_pid_drop_packet(ctx->ipid);
				//post a seek, this will trash remaining packets in buffers
				GF_FEVT_INIT(fevt, GF_FEVT_SOURCE_SEEK, ctx->ipid);
				fevt.seek.start_offset = ctx->filepos;
				gf_filter_pid_send_event(ctx->ipid, &fevt);
				return GF_OK;
			}
			ctx->cts += ctx->fps.den;
		}
	}
	gf_filter_pid_drop_packet(ctx->ipid);
	return GF_OK;
}


static GF_FilterCapability RawVidReframeCaps[] =
{
	CAP_UINT(GF_CAPS_INPUT, GF_PROP_PID_STREAM_TYPE, GF_STREAM_FILE),
	CAP_STRING(GF_CAPS_INPUT, GF_PROP_PID_FILE_EXT, "yuv"),
	CAP_UINT(GF_CAPS_OUTPUT_STATIC, GF_PROP_PID_STREAM_TYPE, GF_STREAM_VISUAL),
	CAP_UINT(GF_CAPS_OUTPUT_STATIC, GF_PROP_PID_CODECID, GF_CODECID_RAW),
	CAP_UINT(GF_CAPS_OUTPUT_STATIC_EXCLUDED, GF_PROP_PID_UNFRAMED, GF_TRUE),
	{0},
	CAP_UINT(GF_CAPS_INPUT, GF_PROP_PID_STREAM_TYPE, GF_STREAM_FILE),
	CAP_STRING(GF_CAPS_INPUT, GF_PROP_PID_MIME, "video/x-yuv"),
};

#define OFFS(_n)	#_n, offsetof(GF_RawVidReframeCtx, _n)
static GF_FilterArgs RawVidReframeArgs[] =
{
	{ OFFS(size), "source video resolution", GF_PROP_VEC2I, "0x0", NULL, 0},
	{ OFFS(spfmt), "source pixel format. When not set, derived from file extension", GF_PROP_PIXFMT, "none", NULL, 0},
	{ OFFS(fps), "number of frames per second", GF_PROP_FRACTION, "25/1", NULL, 0},
	{ OFFS(copy), "copy source bytes into output frame. If not set, source bytes are referenced only", GF_PROP_BOOL, "false", NULL, 0},
	{0}
};


GF_FilterRegister RawVidReframeRegister = {
	.name = "rfrawvid",
	GF_FS_SET_DESCRIPTION("RAW video reframer")
	GF_FS_SET_HELP("This filter parses raw YUV and RGB files/data and outputs corresponding raw video PID and frames.")
	.private_size = sizeof(GF_RawVidReframeCtx),
	.args = RawVidReframeArgs,
	SETCAPS(RawVidReframeCaps),
	.configure_pid = rawvidreframe_configure_pid,
	.process = rawvidreframe_process,
	.process_event = rawvidreframe_process_event
};


const GF_FilterRegister *rawvidreframe_register(GF_FilterSession *session)
{
	RawVidReframeArgs[1].min_max_enum = gf_pixel_fmt_all_names();
	RawVidReframeCaps[1].val.value.string = (char *) gf_pixel_fmt_all_shortnames();
	return &RawVidReframeRegister;
}


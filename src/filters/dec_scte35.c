/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Romain Bouqueau
 *			Copyright (c) Motion Spell 2024
 *					All rights reserved
 *
 *  This file is part of GPAC / SCTE35 property decode filter
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
#include <gpac/internal/isomedia_dev.h>

typedef struct {
	GF_FilterPid *ipid;
	GF_FilterPid *opid;

	u32 last_event_id;

	// used to compute event duration
	u64 last_dts;

	// used to segment empty boxes
	GF_Fraction seg_dur;
	GF_FilterPacket *seg_emeb;
	Bool seg_setup;
	u64 orig_ts;
	u32 nb_forced;
} SCTE35DecCtx;

GF_Err scte35dec_configure_pid(GF_Filter *filter, GF_FilterPid *pid, Bool is_remove)
{
	SCTE35DecCtx *ctx = gf_filter_get_udta(filter);

	if (is_remove) {
		GF_FilterPid *out_pid = gf_filter_pid_get_udta(pid);
		if (out_pid == ctx->opid)
			ctx->opid = NULL;
		if (out_pid)
			gf_filter_pid_remove(out_pid);
		return GF_OK;
	}

	if (!gf_filter_pid_check_caps(pid))
		return GF_NOT_SUPPORTED;

	if (!ctx->opid) {
		ctx->opid = gf_filter_pid_new(filter);
		if (!ctx->opid) return GF_OUT_OF_MEM;
		gf_filter_pid_set_udta(pid, ctx->opid);
		ctx->last_event_id = gf_rand();
	}
	ctx->ipid = pid;
	gf_filter_pid_set_framing_mode(pid, GF_TRUE);

	//copy properties at init or reconfig
	gf_filter_pid_copy_properties(ctx->opid, pid);
	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_STREAM_TYPE, &PROP_UINT(GF_STREAM_METADATA) );
	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_CODECID, &PROP_UINT(GF_CODECID_SCTE35) );
	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_INTERLACED, &PROP_BOOL(GF_FALSE) );

	return GF_OK;
}

static Bool scte35dec_process_event(GF_Filter *filter, const GF_FilterEvent *evt)
{
	if (evt->base.type==GF_FEVT_ENCODE_HINTS) {
		SCTE35DecCtx *ctx = gf_filter_get_udta(filter);
		if (evt->encode_hints.intra_period.den && evt->encode_hints.intra_period.num) {
			ctx->seg_dur = evt->encode_hints.intra_period;
		}
		return GF_TRUE;
	}
	return GF_FALSE;
}

static void scte35dec_send_pck(SCTE35DecCtx *ctx, GF_FilterPacket *pck, u64 dts)
{
	if (ctx->last_dts) {
		gf_filter_pck_set_duration(pck, (u32)(dts - ctx->last_dts));
	}
	gf_filter_pck_set_dts(pck, ctx->last_dts);
	ctx->last_dts = dts;
	gf_filter_pck_set_framing(pck, GF_TRUE, GF_TRUE);
	gf_filter_pck_set_sap(pck, GF_FILTER_SAP_1);
	gf_filter_pck_send(pck);
}

static GF_Err scte35dec_flush_emeb(SCTE35DecCtx *ctx, u64 dts)
{
	if (!ctx->seg_dur.num)
		return GF_OK;

	if (ctx->seg_emeb) {
		scte35dec_send_pck(ctx, ctx->seg_emeb, dts);
		ctx->seg_emeb = NULL;
	}

	if (dts == UINT64_MAX)
		return GF_OK;

	GF_Err e;
	GF_Box *emeb = gf_isom_box_new(GF_ISOM_BOX_TYPE_EMEB);
	if (!emeb) {
		e = GF_OUT_OF_MEM;
		goto exit;
	}

	e = gf_isom_box_size((GF_Box*)emeb);
	if (e) goto exit;

	u8 *output = NULL;
	ctx->seg_emeb = gf_filter_pck_new_alloc(ctx->opid, emeb->size, &output);
	if (!ctx->seg_emeb) {
		e = GF_OUT_OF_MEM;
		goto exit;
	}
	gf_filter_pck_set_dts(ctx->seg_emeb, dts);

	GF_BitStream *bs = gf_bs_new(output, emeb->size, GF_BITSTREAM_WRITE);
	e = gf_isom_box_write((GF_Box*)emeb, bs);
	gf_bs_del(bs);
	if (e) goto exit;

	gf_isom_box_del(emeb);

	return GF_OK;

exit:
	gf_isom_box_del(emeb);
	if (ctx->seg_emeb) {
		gf_filter_pid_drop_packet(ctx->opid);
		ctx->seg_emeb = NULL;
	}
	return e;
}



static u64 scte35dec_parse_splice_time(GF_BitStream *bs)
{
	Bool time_specified_flag = gf_bs_read_int(bs, 1);
	if (time_specified_flag == 1) {
		/*reserved = */gf_bs_read_int(bs, 6);
		u64 pts_time = gf_bs_read_long_int(bs, 33);
		return pts_time;
	} else {
		/*reserved = */gf_bs_read_int(bs, 7);
		return 0;
	}
}

static void scte35dec_get_timing(const u8 *data, u32 size, u64 *pts, u32 *dur, u32 *splice_event_id)
{
	GF_BitStream *bs = gf_bs_new(data, size, GF_BITSTREAM_READ);
	
	/*u8 protocol_version = */gf_bs_read_u8(bs);
	Bool encrypted_packet = gf_bs_read_int(bs, 1);
	/*u8 encryption_algorithm = */gf_bs_read_int(bs, 6);
	u64 pts_adjustment = gf_bs_read_long_int(bs, 33);

	if (encrypted_packet)
		goto exit;

	/*u8 cw_index = */gf_bs_read_u8(bs);
	/*int tier = */gf_bs_read_int(bs, 12);

	int splice_command_length = gf_bs_read_int(bs, 12);
	if (splice_command_length > gf_bs_available(bs)) {
		GF_LOG(GF_LOG_WARNING, GF_LOG_CODEC, ("[Scte35Dec] bitstream too short (" LLU " bytes) while parsing splice command (%u bytes)\n", gf_bs_available(bs), splice_command_length));
		goto exit;
	}

	u8 splice_command_type = gf_bs_read_u8(bs);
	switch(splice_command_type) {
	case 0x05: //splice_insert()
		{
			*splice_event_id = gf_bs_read_u32(bs);
			Bool splice_event_cancel_indicator = gf_bs_read_int(bs, 1);
			/*reserved = */gf_bs_read_int(bs, 7);
			if (splice_event_cancel_indicator == 0) {
				/*Bool out_of_network_indicator = */gf_bs_read_int(bs, 1);
				Bool program_splice_flag = gf_bs_read_int(bs, 1);
				Bool duration_flag = gf_bs_read_int(bs, 1);
				Bool splice_immediate_flag = gf_bs_read_int(bs, 1);
				/*reserved = */gf_bs_read_int(bs, 4);

				if ((program_splice_flag == 1) && (splice_immediate_flag == 0)) {
					*pts = scte35dec_parse_splice_time(bs) + pts_adjustment;
				}

				if (program_splice_flag == 0) {
					u8 component_count = gf_bs_read_u8(bs);
					for (int i=0; i<component_count; i++) {
						/*u8 component_tag = */gf_bs_read_u8(bs);
						if (splice_immediate_flag == 0) {
							gf_assert(*pts == 0); // we've never encounter multi component streams
							*pts = scte35dec_parse_splice_time(bs) + pts_adjustment;
						}
					}
				}
				if (duration_flag == 1) {
					//break_duration()
					/*Bool auto_return = */gf_bs_read_int(bs, 1);
					/*reserved = */gf_bs_read_int(bs, 6);
					*dur = gf_bs_read_long_int(bs, 33);
				}

				// truncated parsing (so we only parse the first command...)
			}
		}
		break;
	case 0x06: //time_signal()
		*pts = scte35dec_parse_splice_time(bs) + pts_adjustment;
		break;
	}

exit:;
	gf_bs_del(bs);
}

GF_Err scte35dec_process(GF_Filter *filter)
{
	SCTE35DecCtx *ctx = gf_filter_get_udta(filter);

	GF_FilterPacket *pck = gf_filter_pid_get_packet(ctx->ipid);
	if (!pck) {
		if (gf_filter_pid_is_eos(ctx->ipid)) {
			scte35dec_flush_emeb(ctx, UINT64_MAX);
			gf_filter_pid_set_eos(ctx->opid);
			return GF_EOS;
		}
		return GF_OK;
	}
	u64 dts = gf_filter_pck_get_dts(pck);
	if (!ctx->last_dts)
		ctx->last_dts = dts;

	GF_FilterPacket *pck_dst = NULL;
	const GF_PropertyValue *emsg = gf_filter_pck_get_property_str(pck, "scte35");
	if (emsg && (emsg->type == GF_PROP_DATA) && emsg->value.data.ptr) {
		GF_Err e = scte35dec_flush_emeb(ctx, dts);
		if (e) return e;

		GF_EventMessageBox *emib = (GF_EventMessageBox *) gf_isom_box_new(GF_ISOM_BOX_TYPE_EMIB);
		if (!emib) return GF_OUT_OF_MEM;

		// Values according to SCTE 214-3 2015
		emib->timescale = gf_filter_pck_get_timescale(pck);
		u64 pts = 0;
		u32 dur = 0xFFFFFFFF;
		scte35dec_get_timing(emsg->value.data.ptr, emsg->value.data.size, &pts, &dur, &ctx->last_event_id); // only check in first splice command
		emib->presentation_time_delta = pts - dts;
		emib->event_duration = dur;
		emib->event_id = ctx->last_event_id++;
		emib->scheme_id_uri = gf_strdup("urn:scte:scte35:2013:bin");
		emib->value = gf_strdup("1001");
		emib->message_data_size = emsg->value.data.size;
		emib->message_data = emsg->value.data.ptr;

		e = gf_isom_box_size((GF_Box*)emib);
		if (e) {
			emib->message_data = NULL;
			gf_isom_box_del((GF_Box*)emib);
			return e;
		}

		u8 *output = NULL;
		pck_dst = gf_filter_pck_new_alloc(ctx->opid, emib->size, &output);
		if (!pck_dst) {
			emib->message_data = NULL;
			gf_isom_box_del((GF_Box*)emib);
			return GF_OUT_OF_MEM;
		}

		GF_BitStream *bs = gf_bs_new(output, emib->size, GF_BITSTREAM_WRITE);
		e = gf_isom_box_write((GF_Box*)emib, bs);
		gf_bs_del(bs);
		emib->message_data = NULL;
		gf_isom_box_del((GF_Box*)emib);
		if (e) return e;

		//TODO: compute duration when available in the SCTE35 payload
		//TODO: we need to segment emib and recompute the offsets too!
		scte35dec_send_pck(ctx, pck_dst, ctx->last_dts);
	}

	//check if we need to force an 'emeb' box
	if (ctx->seg_dur.den && ctx->seg_dur.num>0) {
		if (!ctx->seg_setup) {
			ctx->seg_setup = GF_TRUE;
			ctx->orig_ts = dts;
			scte35dec_flush_emeb(ctx, dts);
			ctx->nb_forced = 1;
		} else if (dts < ctx->orig_ts) {
			GF_LOG(GF_LOG_WARNING, GF_LOG_CODEC, ("[Scte35Dec] timestamps not increasing monotonuously, resetting segmentation state !\n"));
			ctx->orig_ts = dts;
			scte35dec_flush_emeb(ctx, dts);
			ctx->nb_forced = 1;
		} else {
			GF_Fraction64 ts_diff = { dts-ctx->orig_ts, gf_filter_pck_get_timescale(pck) };
			if (ts_diff.num * ctx->seg_dur.den >= ctx->nb_forced * ctx->seg_dur.num * ts_diff.den) {
				scte35dec_flush_emeb(ctx, dts);
				ctx->nb_forced++;
				GF_LOG(GF_LOG_INFO, GF_LOG_CODEC, ("[Scte35Dec] Forcing segment at DTS %d / 1000000\n", dts));
			}
		}
	}

	gf_filter_pid_drop_packet(ctx->ipid);

	return GF_OK;
}

static const GF_FilterCapability SCTE35DecCaps[] =
{
	CAP_UINT(GF_CAPS_INPUT, GF_PROP_PID_STREAM_TYPE, GF_STREAM_VISUAL),
	CAP_UINT(GF_CAPS_INPUT, GF_PROP_PID_STREAM_TYPE, GF_STREAM_AUDIO),
	CAP_BOOL(GF_CAPS_INPUT_EXCLUDED, GF_PROP_PID_UNFRAMED, GF_TRUE),
	CAP_UINT(GF_CAPS_INPUT_EXCLUDED, GF_PROP_PID_CODECID, GF_CODECID_NONE),
	CAP_BOOL(GF_CAPS_OUTPUT_STATIC_EXCLUDED, GF_PROP_PID_UNFRAMED, GF_TRUE),
};

GF_FilterRegister SCTE35DecRegister = {
	.name = "scte35dec",
	GF_FS_SET_DESCRIPTION("SCTE35 decoder")
	GF_FS_SET_HELP("This filter extracts SCTE-35 markers attached as properties to audio and video\n"
	               "packets as 23001-18 'emib' boxes. It also creates empty 'emeb' box in between\n"
	               "following segmentation as hinted by the graph.")
	.private_size = sizeof(SCTE35DecCtx),
	.flags = GF_FS_REG_EXPLICIT_ONLY,
	SETCAPS(SCTE35DecCaps),
	.process = scte35dec_process,
	.process_event = scte35dec_process_event,
	.configure_pid = scte35dec_configure_pid,
};

const GF_FilterRegister *scte35dec_register(GF_FilterSession *session)
{
	return &SCTE35DecRegister;
}

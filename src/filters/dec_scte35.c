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
#include <stdint.h> // UINT32_MAX

#define IS_SEGMENTED (ctx->segdur.den && ctx->segdur.num>0)
#define IS_PASSTHRU  (ctx->mode == 1)

typedef struct {
	u64 dts;
	GF_EventMessageBox *emib;
} Event;

GF_OPT_ENUM (SCTE35DecDataMode,
	PROP=0, // scte35 data is carried as a property
	RAW,    // packet data contains the scte35 payload (m2ts section)
	BOX,    // packet data contains the emib/emeb boxes
);

typedef struct {
	GF_FilterPid *ipid;
	GF_FilterPid *opid;

	SCTE35DecDataMode data_mode;
	u64 clock;

	// options
	u32 mode;
	GF_Fraction segdur;

	// override gf_filter_*() calls for testability
	GF_FilterPacket* (*pck_new_shared)(GF_FilterPid *pid, const u8 *data, u32 data_size, gf_fsess_packet_destructor destruct);
	GF_FilterPacket* (*pck_new_alloc)(GF_FilterPid *pid, u32 data_size, u8 **data);
	GF_Err (*pck_send)(GF_FilterPacket *pck);

	GF_List *ordered_events; // Event: events ordered by dispatch time
	u32 last_event_id;

	// used to compute immediate dispatch event duration
	u32 timescale;
	u32 last_pck_dur;
	s64 last_dispatched_dts;
	Bool last_dispatched_dts_init;

	// used to segment empty boxes
	u64 orig_ts;
	Bool seg_setup;
	u32 segnum;
	u8 emeb_box[8];
	GF_FilterPacket *dash_pck; // when called from the dasher
} SCTE35DecCtx;

static GF_Err scte35dec_initialize_internal(SCTE35DecCtx *ctx)
{
	ctx->ordered_events = gf_list_new();

	GF_Box *emeb = gf_isom_box_new(GF_ISOM_BOX_TYPE_EMEB);
	if (!emeb) return GF_OUT_OF_MEM;
	GF_Err e = gf_isom_box_size((GF_Box*)emeb);
	if (e) return e;
	GF_BitStream *bs = gf_bs_new(ctx->emeb_box, sizeof(ctx->emeb_box), GF_BITSTREAM_WRITE);
	e = gf_isom_box_write((GF_Box*)emeb, bs);
	gf_bs_del(bs);
	if (e) return e;
	gf_isom_box_del(emeb);

	return GF_OK;
}

static GF_Err scte35dec_initialize(GF_Filter *filter)
{
	SCTE35DecCtx *ctx = gf_filter_get_udta(filter);
	ctx->pck_new_shared = gf_filter_pck_new_shared;
	ctx->pck_new_alloc = gf_filter_pck_new_alloc;
	ctx->pck_send = gf_filter_pck_send;
	return scte35dec_initialize_internal(ctx);
}

static void scte35dec_finalize_internal(SCTE35DecCtx *ctx)
{
	for (u32 i=0; i<gf_list_count(ctx->ordered_events); i++) {
		Event *evt = gf_list_get(ctx->ordered_events, i);
		gf_isom_box_del((GF_Box*)evt->emib);
		gf_free(evt);
	}
	gf_list_del(ctx->ordered_events);
	if (ctx->dash_pck) {
		gf_filter_pck_unref(ctx->dash_pck);
		ctx->dash_pck = NULL;
	}
}

static void scte35dec_finalize(GF_Filter *filter)
{
	SCTE35DecCtx *ctx = gf_filter_get_udta(filter);
	scte35dec_finalize_internal(ctx);
}

static GF_Err scte35dec_configure_pid(GF_Filter *filter, GF_FilterPid *pid, Bool is_remove)
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
	if (IS_PASSTHRU) return GF_OK;

	const GF_PropertyValue *p = gf_filter_pid_get_property(ctx->ipid, GF_PROP_PID_CODECID);
	if (p) {
		if (p->value.uint == GF_CODECID_SCTE35)
			ctx->data_mode = RAW;
		else if (p->value.uint == GF_CODECID_EVTE)
			ctx->data_mode = BOX;
	}

	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_STREAM_TYPE, &PROP_UINT(GF_STREAM_METADATA) );
	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_CODECID,     &PROP_UINT(GF_CODECID_EVTE) );
	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_INTERLACED,  &PROP_BOOL(GF_FALSE) );

	return GF_OK;
}

static Bool scte35dec_process_event(GF_Filter *filter, const GF_FilterEvent *evt)
{
	if (evt->base.type==GF_FEVT_TRANSPORT_HINTS) {
		if (evt->transport_hints.flags & GF_TRANSPORT_HINTS_SAW_ENCODER) {
			// this is a pass-through event, ignore it
			return GF_FALSE;
		}

		SCTE35DecCtx *ctx = gf_filter_get_udta(filter);
		if (evt->transport_hints.seg_duration.den && evt->transport_hints.seg_duration.num) {
			ctx->segdur = evt->transport_hints.seg_duration;
		}

		//send the event upstream (in case any other filter is interested in it)
		GF_FilterEvent new_evt = *evt;
		new_evt.base.on_pid = ctx->ipid;
		new_evt.transport_hints.flags |= GF_TRANSPORT_HINTS_SAW_ENCODER;
		gf_filter_pid_send_event(ctx->ipid, &new_evt);
		return GF_TRUE;
	}
	return GF_FALSE;
}

static void scte35dec_send_pck(SCTE35DecCtx *ctx, GF_FilterPacket *pck, u64 dts, u32 dur)
{
	if (IS_SEGMENTED) {
		if (ctx->dash_pck) {
			gf_filter_pck_merge_properties(ctx->dash_pck, pck);
			gf_filter_pck_unref(ctx->dash_pck);
			ctx->dash_pck = NULL;
		} else {
			GF_LOG(GF_LOG_DEBUG, GF_LOG_CODEC, ("[Scte35Dec] Unaligned segment at dts="LLU". Ignore this message if not using the dasher.\n", dts));
		}
	}

	if (dur > 0) {
		gf_filter_pck_set_duration(pck, dur);
	}
	GF_LOG(GF_LOG_DEBUG, GF_LOG_CODEC, ("[Scte35Dec] Send dts="LLU" dur=%u\n", dts, dur));
	gf_filter_pck_set_dts(pck, dts);
	ctx->last_dispatched_dts = dts;
	ctx->last_pck_dur = dur;
	gf_filter_pck_set_framing(pck, GF_TRUE, GF_TRUE);
	gf_filter_pck_set_sap(pck, GF_FILTER_SAP_1);
	ctx->pck_send(pck);
	ctx->clock = ctx->last_dispatched_dts + ctx->last_pck_dur;
}

static GF_Err scte35dec_flush_emeb(SCTE35DecCtx *ctx, u64 dts, u32 dur)
{
	GF_FilterPacket *seg_emeb = ctx->pck_new_shared(ctx->opid, ctx->emeb_box, sizeof(ctx->emeb_box), NULL);
	if (!seg_emeb) return GF_OUT_OF_MEM;

	scte35dec_send_pck(ctx, seg_emeb, dts, dur != UINT32_MAX ? dur : (u32)(dts - ctx->last_dispatched_dts));

	return GF_OK;
}

static void scte35dec_schedule(SCTE35DecCtx *ctx, u64 dts, GF_EventMessageBox *emib)
{
	Event *evt_new;
	GF_SAFEALLOC(evt_new, Event);
	evt_new->dts = dts;
	evt_new->emib = emib;

	for (u32 i=0; i<gf_list_count(ctx->ordered_events); i++) {
		Event *evt_i = gf_list_get(ctx->ordered_events, i);
		if (evt_i->dts + evt_i->emib->presentation_time_delta > dts + emib->presentation_time_delta) {
			gf_list_insert(ctx->ordered_events, evt_new, i);
			return;
		}
	}
	gf_list_add(ctx->ordered_events, evt_new);
}

static u32 compute_emib_duration(u64 dts, u64 evt_dts, u32 max_dur, u32 evt_dur)
{
	gf_assert(dts <= evt_dts);
	if (dts < evt_dts) {
		return (u32) MIN(evt_dts - dts, max_dur);
	} else if (max_dur != UINT32_MAX && evt_dur > max_dur) {
		return max_dur;
	} else {
		return evt_dur;
	}
}

static GF_Err scte35dec_flush_emib(SCTE35DecCtx *ctx, u64 dts, u32 max_dur)
{
	gf_fatal_assert(gf_list_count(ctx->ordered_events) > 0);

	GF_Err e = GF_OK;
	Event *evt;
	while ( (evt = gf_list_pop_front(ctx->ordered_events)) ) {
		u32 evt_dur = evt->emib->event_duration == 0xFFFFFFFF ? 1 : evt->emib->event_duration;
		if (evt->dts + evt->emib->presentation_time_delta + evt->emib->event_duration >= dts) {
			u8 *output = NULL;
			GF_FilterPacket *pck_dst = ctx->pck_new_alloc(ctx->opid, (u32) evt->emib->size, &output);
			if (!pck_dst) {
				e = GF_OUT_OF_MEM;
				goto exit;
			}

			GF_BitStream *bs = gf_bs_new(output, evt->emib->size, GF_BITSTREAM_WRITE);
			e = gf_isom_box_write((GF_Box*)evt->emib, bs);
			gf_bs_del(bs);
			if (e) goto exit;

			u32 emib_dur = compute_emib_duration(dts, evt->dts+evt->emib->presentation_time_delta, max_dur, evt_dur);
			u64 emib_dts = IS_SEGMENTED ? evt->dts : dts;
			scte35dec_send_pck(ctx, pck_dst, emib_dts, emib_dur);

			evt->dts += emib_dur;
			dts += emib_dur;
			if (evt->emib->presentation_time_delta <= 0)
				if (evt->emib->event_duration != UINT32_MAX)
					evt->emib->event_duration -= emib_dur;
			evt->emib->presentation_time_delta -= emib_dur;
			if (evt->emib->presentation_time_delta < 0)
				evt->emib->presentation_time_delta = 0;
			if (max_dur != UINT32_MAX)
				max_dur -= emib_dur;
		}

		if (!IS_SEGMENTED ||
		    (evt->emib->presentation_time_delta <= 0 && (evt->emib->event_duration == UINT32_MAX || evt->emib->event_duration <= 0))) {
			// we're done with the event
			gf_isom_box_del((GF_Box*)evt->emib);
			gf_free(evt);
		} else {
			scte35dec_schedule(ctx, evt->dts, evt->emib);
			gf_free(evt);
			if (max_dur != UINT32_MAX && max_dur > 0)
				continue; // still time within time scope: re-schedule and continue to process
			else
				break;    // process later
		}
	}

exit:
	if (e) {
		gf_isom_box_del((GF_Box*)evt->emib);
		gf_free(evt);
	}
	return e;
}

static GF_Err scte35_insert_emeb_before_emib(SCTE35DecCtx *ctx, Event *first_evt, u64 timestamp, u64 dur)
{
	if (dur == UINT32_MAX) dur = first_evt->dts - timestamp;
	gf_assert(timestamp + dur >= first_evt->dts);
	GF_Err e = scte35dec_flush_emeb(ctx, timestamp, (u32) dur);
	ctx->clock = timestamp + dur;
	return e;
}

static GF_Err scte35dec_push_box(SCTE35DecCtx *ctx, const u64 ts, const u32 dur)
{
	if (gf_list_count(ctx->ordered_events) == 0)
		return scte35dec_flush_emeb(ctx, ts, dur);

	GF_Err e = GF_OK;
	Event *first_evt = gf_list_get(ctx->ordered_events, 0);
	u64 curr_ts = ts;
	u64 curr_dur = dur;
	if (IS_SEGMENTED) {
		u64 segdur = ctx->segdur.num * ctx->timescale / ctx->segdur.den;
		gf_assert(segdur == dur);
		// pre-signal events in each segment
		if (curr_ts < first_evt->dts) {
			u64 emeb_dur = MIN(first_evt->dts - curr_ts, segdur);
			e = scte35_insert_emeb_before_emib(ctx, first_evt, curr_ts, emeb_dur);
			if (e) return e;
			curr_dur -= emeb_dur;
			curr_ts += emeb_dur;
		}
	} else {
		// immediate dispatch: jump directly to event
		if (ts < first_evt->dts + first_evt->emib->presentation_time_delta) {
			e = scte35_insert_emeb_before_emib(ctx, first_evt, ts, first_evt->dts + first_evt->emib->presentation_time_delta - ts);
			if (e) return e;
			curr_ts = first_evt->dts + first_evt->emib->presentation_time_delta;
		}
	}

	e = scte35dec_flush_emib(ctx, curr_ts, (u32) curr_dur);
	if (e) return e;

	if (IS_SEGMENTED && ctx->clock < ts + dur) {
		// complete the segment with an empty box
		return scte35dec_flush_emeb(ctx, ctx->clock, (u32) (ts + dur - ctx->clock));
	}

	return GF_OK;
}

static void scte35dec_flush(SCTE35DecCtx *ctx)
{
	if (IS_PASSTHRU)
		return;
	if (!gf_list_count(ctx->ordered_events) && ctx->clock == ctx->last_dispatched_dts + ctx->last_pck_dur)
		return; //nothing to flush

	if (IS_SEGMENTED) {
		scte35dec_push_box(ctx, ctx->segnum * ctx->segdur.num * ctx->timescale / ctx->segdur.den, ctx->segdur.num * ctx->timescale / ctx->segdur.den);
		ctx->segnum++;
	} else {
		scte35dec_push_box(ctx, 0, UINT32_MAX);
	}
}

static GF_Err new_segment(SCTE35DecCtx *ctx)
{
	GF_LOG(GF_LOG_INFO, GF_LOG_CODEC, ("[Scte35Dec] New segment at DTS %d/%u (%lf). Flushing the previous one.\n",
		ctx->clock, ctx->timescale, (double)ctx->clock/ctx->timescale));
	u64 dts = ctx->orig_ts + ctx->segnum * ctx->segdur.num * ctx->timescale / ctx->segdur.den;
	if (ctx->segnum == 0) // first segment: adjust last_dispatched_dts to a previous fictive segment
		ctx->last_dispatched_dts = ctx->orig_ts - ctx->segdur.num * ctx->timescale / ctx->segdur.den;
	ctx->segnum++;
	ctx->clock = dts;
	return scte35dec_push_box(ctx, dts, (u32) ( ctx->segnum * ctx->segdur.num * ctx->timescale / ctx->segdur.den - (dts - ctx->orig_ts)) );
}

static u64 scte35dec_parse_splice_time(GF_BitStream *bs)
{
	Bool time_specified_flag = gf_bs_read_int(bs, 1);
	if (time_specified_flag == 1) {
		/*reserved = */gf_bs_read_int(bs, 6);
		return /*pts_time =*/ gf_bs_read_long_int(bs, 33);
	} else {
		/*reserved = */gf_bs_read_int(bs, 7);
		return 0;
	}
}

static void scte35dec_get_timing(const u8 *data, u32 size, u64 *pts, u64 *dur, u32 *splice_event_id, Bool *needs_idr)
{
	GF_BitStream *bs = gf_bs_new(data, size, GF_BITSTREAM_READ);

	// splice_info_section() : the full MPEG2-TS Section is in here
	u8 table_id = gf_bs_read_u8(bs);
	if (table_id != 0xFC) {
		GF_LOG(GF_LOG_WARNING, GF_LOG_CODEC, ("[Scte35Dec] Invalid splice_info_section() table_id. Abort parsing.\n"));
		goto exit;
	}
	/*Bool section_syntax_indicator = */gf_bs_read_int(bs, 1);
	/*Bool private_indicator = */gf_bs_read_int(bs, 1);
	/*u8 sap_type = */gf_bs_read_int(bs, 2);
	u32 section_length = gf_bs_read_int(bs, 12);
	if (section_length + 3 != gf_bs_get_size(bs)) {
		GF_LOG(GF_LOG_WARNING, GF_LOG_CODEC, ("[Scte35Dec] Invalid section length %d\n", section_length));
		goto exit;
	}
	
	/*u8 protocol_version = */gf_bs_read_u8(bs);
	Bool encrypted_packet = gf_bs_read_int(bs, 1);
	/*u8 encryption_algorithm = */gf_bs_read_int(bs, 6);
	u64 pts_adjustment = gf_bs_read_long_int(bs, 33);

	if (encrypted_packet) {
		GF_LOG(GF_LOG_WARNING, GF_LOG_CODEC, ("[Scte35Dec] Encrypted packet, not supported (pts_adjustment="LLU")\n", pts_adjustment));
		goto exit;
	}

	/*u8 cw_index = */gf_bs_read_u8(bs);
	/*u32 tier = */gf_bs_read_int(bs, 12);

	u32 splice_command_length = gf_bs_read_int(bs, 12);
	if (splice_command_length > gf_bs_available(bs)) {
		GF_LOG(GF_LOG_WARNING, GF_LOG_CODEC, ("[Scte35Dec] Bitstream too short (" LLU " bytes) while parsing splice command (%u bytes)\n",
			gf_bs_available(bs), splice_command_length));
		goto exit;
	}

	u8 splice_command_type = gf_bs_read_u8(bs);
	switch(splice_command_type) {
	case 0x05: //splice_insert()
		{
			u64 splice_time = 0;
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
					splice_time = scte35dec_parse_splice_time(bs);
					*pts = splice_time + pts_adjustment;
				}

				if (program_splice_flag == 0) {
					u32 i;
					u32 component_count = gf_bs_read_u8(bs);
					for (i=0; i<component_count; i++) {
						/*u8 component_tag = */gf_bs_read_u8(bs);
						if (splice_immediate_flag == 0) {
							gf_assert(*pts == 0); // we've never encounter multi component streams
							splice_time = scte35dec_parse_splice_time(bs);
							*pts = splice_time + pts_adjustment;
						}
					}
				}
				if (duration_flag == 1) {
					//break_duration()
					/*Bool auto_return = */gf_bs_read_int(bs, 1);
					/*reserved = */gf_bs_read_int(bs, 6);
					*dur = gf_bs_read_long_int(bs, 33);
				}

				*needs_idr = GF_TRUE;
				// truncated parsing: we make the assumption that there is only one command (which is the case from M2TS section sources)
			}

			GF_LOG(GF_LOG_INFO, GF_LOG_CODEC, ("[Scte35Dec] Found splice_insert() (*splice_event_id=%u, pts_adjustment="LLU", dur=%u, splice_time="LLU")\n",
				*splice_event_id, pts_adjustment, *dur, splice_time));
		}
		goto exit;
	case 0x06: //time_signal()
		{
			u64 splice_time = scte35dec_parse_splice_time(bs);
			*pts = splice_time + pts_adjustment;
			GF_LOG(GF_LOG_INFO, GF_LOG_CODEC, ("[Scte35Dec] Found time_signal() for PTS="LLU" (splice_time="LLU", pts_adjustment="LLU")\n",
				*pts, splice_time, pts_adjustment));
		}
		break;
	default:
		GF_LOG(GF_LOG_INFO, GF_LOG_CODEC, ("[Scte35Dec] Found splice_command_type=0x%02X length=%d pts_adjustment="LLU"\n",
			splice_command_type, splice_command_length, pts_adjustment));
		goto exit;
	}

	u16 descriptor_loop_length = gf_bs_read_u16(bs);
	u32 descriptor_start_pos = (u32) gf_bs_get_position(bs);
	while (gf_bs_get_position(bs) - descriptor_start_pos < descriptor_loop_length) {
		u8 splice_descriptor_tag = gf_bs_read_u8(bs);
		u8 descriptor_length = gf_bs_read_u8(bs);

		if (descriptor_length > gf_bs_available(bs)) {
			GF_LOG(GF_LOG_WARNING, GF_LOG_CODEC, ("[Scte35Dec] Bitstream too short while parsing descriptor (%u bytes)\n", descriptor_length));
			goto exit;
		}

		/*u32 identifier = */gf_bs_read_u32(bs);
		switch (splice_descriptor_tag) {
		case 0x02: //segmentation_descriptor()
			{
				/*u32 segmentation_event_id = */gf_bs_read_u32(bs);
				Bool segmentation_event_cancel_indicator = gf_bs_read_int(bs, 1);
				/*Bool segmentation_event_id_compliance_indicator = */gf_bs_read_int(bs, 1);
				/*reserved = */gf_bs_read_int(bs, 6);

				if (segmentation_event_cancel_indicator == 0) {
					Bool program_segmentation_flag = gf_bs_read_int(bs, 1);
					Bool segmentation_duration_flag = gf_bs_read_int(bs, 1);
					Bool delivery_not_restricted_flag = gf_bs_read_int(bs, 1);

					if (delivery_not_restricted_flag == 0) {
						/*u8 web_delivery_allowed_flag = */gf_bs_read_int(bs, 1);
						/*u8 no_regional_blackout_flag = */gf_bs_read_int(bs, 1);
						/*u8 archive_allowed_flag = */gf_bs_read_int(bs, 1);
						/*u8 device_restrictions = */gf_bs_read_int(bs, 2);
					} else {
						/*reserved = */gf_bs_read_int(bs, 5);
					}

					if (program_segmentation_flag == 0) { //deprecated
						u8 component_count = gf_bs_read_u8(bs);
						for (u8 i=0; i<component_count; i++)
							gf_bs_skip_bytes(bs, 48);
					}

					if (segmentation_duration_flag == 1) {
						/*u64 segmentation_duration = */gf_bs_read_long_int(bs, 40);
					}

					/*u8 segmentation_upid_type = */gf_bs_read_u8(bs);
					u8 segmentation_upid_length = gf_bs_read_u8(bs);
					gf_bs_skip_bytes(bs, segmentation_upid_length);
					u8 segmentation_type_id = gf_bs_read_u8(bs);
					/*u8 segment_num = */gf_bs_read_u8(bs);
					/*u8 segments_expected = */gf_bs_read_u8(bs);

					switch (segmentation_type_id)
					{
					case 0x10:
						*needs_idr = GF_TRUE;
						break;
					case 0x34:
						*needs_idr = GF_TRUE;
					case 0x30:
					case 0x32:
					case 0x36:
					case 0x38:
					case 0x3A:
					case 0x44:
					case 0x46:
						/*u8 sub_segment_num = */gf_bs_read_u8(bs);
						/*u8 sub_segments_expected = */gf_bs_read_u8(bs);
						break;
					default:
						break;
					}

					GF_LOG(GF_LOG_INFO, GF_LOG_CODEC, ("[Scte35Dec] Found segmentation_descriptor() segmentation_type_id=%u (needs_idr=%u)\n", segmentation_type_id, *needs_idr));
				}
			}
			break;
		default:
			gf_bs_skip_bytes(bs, MAX(0, descriptor_length - 4));
			break;
		}
	}

exit:
	gf_bs_del(bs);
}

static void scte35dec_process_timing(SCTE35DecCtx *ctx, u64 dts, u32 timescale, u32 dur)
{
	// handle internal clock, timescale and duration
	if (!ctx->last_dispatched_dts_init) {
		ctx->timescale = timescale;
		if (IS_SEGMENTED && ctx->segdur.num * ctx->timescale % ctx->segdur.den)
			GF_LOG(GF_LOG_WARNING, GF_LOG_CODEC, ("[Scte35Dec] timescale(%u) can't express segment duration(%u/%u).\n", timescale, ctx->segdur.num, ctx->segdur.den));
		ctx->last_dispatched_dts = dts - dur;
		ctx->last_pck_dur = dur;
		ctx->last_dispatched_dts_init = GF_TRUE;
		ctx->clock = dts;
	} else if (ctx->clock < dts && !IS_SEGMENTED &&
	           ctx->last_pck_dur && (ctx->last_pck_dur + ctx->last_dispatched_dts != dts)) {
		// drift control
		s64 drift = (s64) (ctx->last_pck_dur + ctx->last_dispatched_dts);
		drift -= (s64) dts;
		GF_LOG(ABS(drift) <= 2 ? GF_LOG_DEBUG : GF_LOG_WARNING, GF_LOG_CODEC, ("[Scte35Dec] Detected drift of "LLD" at dts="LLU", rectifying.\n", drift, dts));
		ctx->last_dispatched_dts += drift;
		dts += drift;
	} else if (!IS_SEGMENTED && dur != ctx->last_pck_dur) {
		// variable duration packets
		ctx->last_dispatched_dts = dts - dur;
	}

	if (IS_SEGMENTED) {
		// check if we moved forward by more than one segment (which may happen with sparse streams/no heartbeat/non-prop data_mode)
		while ((dts - ctx->clock) * ctx->segdur.den >= ctx->segdur.num * ctx->timescale) {
			ctx->segnum = 1 + (u32) (ctx->clock * ctx->segdur.den / (ctx->segdur.num * ctx->timescale) );
			u32 segdur = ctx->segdur.num * ctx->timescale / ctx->segdur.den;
			segdur = (u32) MIN(dts - ctx->clock * segdur, segdur);
			scte35dec_push_box(ctx, ctx->clock, segdur);
		}
	}

	ctx->clock = MAX(ctx->clock, dts);
}

static GF_Err scte35dec_process_emsg(SCTE35DecCtx *ctx, const u8 *data, u32 size, u64 dts)
{
	u64 pts = 0;
	u64 dur = (u64) -1;
	Bool needs_idr = GF_FALSE;
	// parsing is incomplete so we only check the first splice command ...
	scte35dec_get_timing(data, size, &pts, &dur, &ctx->last_event_id, &needs_idr);

	GF_EventMessageBox *emib = (GF_EventMessageBox *) gf_isom_box_new(GF_ISOM_BOX_TYPE_EMIB);
	if (!emib) return GF_OUT_OF_MEM;

	// set values according to SCTE 214-3 2015
	emib->presentation_time_delta = pts - dts;
	if (pts < ctx->clock && !IS_SEGMENTED)
		GF_LOG(GF_LOG_WARNING, GF_LOG_CODEC, ("[Scte35Dec] event overlap detected in immediate dispatch mode (not segmented)\n"));
	emib->event_duration = (u32) dur;
	GF_LOG(GF_LOG_DEBUG, GF_LOG_CODEC, ("[Scte35Dec] detected pts="LLU" (delta="LLU") dur=%u at dts="LLU"\n", pts, pts-dts, dur, dts));
	emib->event_id = ctx->last_event_id++;
	emib->scheme_id_uri = gf_strdup("urn:scte:scte35:2013:bin");
	emib->value = gf_strdup("1001");
	emib->message_data_size = size;
	emib->message_data = gf_malloc(emib->message_data_size);
	if (!emib->message_data) return GF_OUT_OF_MEM;
	memcpy(emib->message_data, data, emib->message_data_size);

	GF_Err e = gf_isom_box_size((GF_Box*)emib);
	if (e) {
		gf_isom_box_del((GF_Box*)emib);
		return e;
	}

	if (!IS_PASSTHRU || (IS_PASSTHRU && needs_idr))
		scte35dec_schedule(ctx, dts, emib);
	else
		gf_isom_box_del((GF_Box*)emib);

	return GF_OK;
}

static Bool scte35dec_is_splice_point(SCTE35DecCtx *ctx, u64 cts)
{
	Event *evt = gf_list_get(ctx->ordered_events, 0);
	if (!evt) return GF_FALSE;
	Bool is_splice = (evt->dts + evt->emib->presentation_time_delta == cts);
	if (is_splice) {
		Event *evt = gf_list_pop_front(ctx->ordered_events);
		gf_isom_box_del((GF_Box*)evt->emib);
		gf_free(evt);
	}
	return is_splice;
}

static GF_Err scte35dec_process_dispatch(SCTE35DecCtx *ctx, u64 dts, u32 dur)
{
	if (!IS_SEGMENTED) {
		// unsegmented: dispatch at each frame
		gf_assert(gf_list_count(ctx->ordered_events) <= 1);
		GF_Err e = scte35dec_push_box(ctx, dts, UINT32_MAX);
		gf_list_rem_last(ctx->ordered_events);
		return e;
	} else {
		if (!ctx->seg_setup) {
			ctx->seg_setup = GF_TRUE;
			ctx->orig_ts = ctx->clock;
			ctx->segnum = 0;
		}

		// segmented: we can only flush at the end of the segment
		if (ctx->clock < ctx->orig_ts) {
			GF_LOG(GF_LOG_WARNING, GF_LOG_CODEC, ("[Scte35Dec] timestamps not increasing monotonuously, resetting segmentation state !\n"));
			ctx->orig_ts = ctx->clock;
			ctx->segnum = 0;
		} else {
			GF_Fraction64 ts_diff = { ctx->clock - ctx->orig_ts, ctx->timescale };
			if ((s64) ((ts_diff.num + dur) * ctx->segdur.den) >= (s64) ( (ctx->segnum+1) * ctx->segdur.num * ts_diff.den))
				return new_segment(ctx);
		}
	}

	return GF_OK;
}

static GF_Err scte35dec_process_passthrough(SCTE35DecCtx *ctx, GF_FilterPacket *pck)
{
	GF_FilterPacket *dst_pck = gf_filter_pck_new_clone(ctx->opid, pck, NULL);
	if (!dst_pck)
		return GF_OUT_OF_MEM;

	u64 cts = gf_filter_pck_get_cts(pck);
	if (scte35dec_is_splice_point(ctx, cts)) {
		GF_LOG(GF_LOG_INFO, GF_LOG_CODEC, ("[Scte35Dec] Detected splice point at cts=" LLU " - adding cue start property\n", cts));
		gf_filter_pck_set_property(dst_pck, GF_PROP_PCK_CUE_START, &PROP_BOOL(GF_TRUE));
	}

	return ctx->pck_send(dst_pck);
}

static const u8 *scte35dec_pck_get_data(SCTE35DecCtx *ctx, GF_FilterPacket *pck, u32 *size)
{
	const u8 *data = NULL;

	if (ctx->data_mode != PROP) {
		data = gf_filter_pck_get_data(pck, size); // RAW data_mode

		if (ctx->data_mode == BOX) {
			GF_BitStream *bs = gf_bs_new(data, *size, GF_BITSTREAM_READ);

			// not RAW: reset
			data = NULL;
			*size = 0;

			// parse boxes
			while (gf_bs_available(bs) > 0) {
				GF_Box *a = NULL;
				GF_Err e = gf_isom_box_parse(&a, bs);
				if (e) {
					GF_LOG(GF_LOG_WARNING, GF_LOG_CODEC, ("[Scte35Dec] parsing data boxes failed\n"));
					break; //don't parse any further
				}
				if (a->type == GF_ISOM_BOX_TYPE_EMIB) {
					if (data && *size)
						GF_LOG(GF_LOG_WARNING, GF_LOG_CODEC, ("[Scte35Dec] detected two 'emib' box while parsing data boxes: not supported\n"));
					GF_EventMessageBox *emib = (GF_EventMessageBox*)a;
					data = emib->message_data;
					*size = emib->message_data_size;
					GF_LOG(GF_LOG_DEBUG, GF_LOG_CODEC, ("[Scte35Dec] detected 'emib' box (size=%u))\n", *size));
				}
				gf_isom_box_del(a);
			}
			gf_bs_del(bs);
		}
	} else {
		const GF_PropertyValue *emsg = gf_filter_pck_get_property_str(pck, "scte35");
		if (emsg && (emsg->type == GF_PROP_DATA) && emsg->value.data.ptr) {
			data = emsg->value.data.ptr;
			*size = emsg->value.data.size;
		}
	}

	return data;
}

static GF_Err scte35dec_process(GF_Filter *filter)
{
	SCTE35DecCtx *ctx = gf_filter_get_udta(filter);

	GF_FilterPacket *pck = gf_filter_pid_get_packet(ctx->ipid);
	if (!pck) {
		if (gf_filter_pid_is_eos(ctx->ipid)) {
			scte35dec_flush(ctx);
			gf_filter_pid_set_eos(ctx->opid);
			return GF_EOS;
		}
		return GF_OK;
	}

	u64 dts = gf_filter_pck_get_dts(pck);
	if (dts == GF_FILTER_NO_TS) {
		u32 size = 0;
		gf_filter_pck_get_data(pck, &size);
		if (!size) {
			const GF_PropertyValue *p = gf_filter_pck_get_property(pck, GF_PROP_PCK_EODS);
			if (p && p->value.boolean) {
				if (ctx->clock != ctx->last_dispatched_dts + ctx->last_pck_dur) {
					// TODO: we split without following the dasher's instructions
					GF_LOG(GF_LOG_ERROR, GF_LOG_CODEC, ("[Scte35Dec] segment boundary mismatch - contact the GPAC Team\n"));
				}
				gf_filter_pck_forward(pck, ctx->opid);
				gf_filter_pid_drop_packet(ctx->ipid);
				return GF_OK;
			}
		}

		dts = ctx->last_dispatched_dts + ctx->last_pck_dur;
		GF_LOG(GF_LOG_ERROR, GF_LOG_CODEC, ("[Scte35Dec] packet with no DTS. Inferring value "LLU".\n", dts));
	}
	u32 dur = gf_filter_pck_get_duration(pck);
	scte35dec_process_timing(ctx, dts, gf_filter_pck_get_timescale(pck), dur);

	u32 size = 0;
	const u8 *data = scte35dec_pck_get_data(ctx, pck, &size);
	if (data && size) {
		GF_Err e = scte35dec_process_emsg(ctx, data, size, dts);
		if (e) GF_LOG(GF_LOG_WARNING, GF_LOG_CODEC, ("[Scte35Dec] Detected error while processing 'emsg' at dts="LLU"\n", dts));
	}

	GF_Err e;
	if (IS_PASSTHRU) {
		e = scte35dec_process_passthrough(ctx, pck);
	} else {
		if (gf_filter_pck_get_property(pck, GF_PROP_PCK_FILENUM)) {
			//remember first pck of segment
			if (ctx->dash_pck)
				gf_filter_pck_unref(ctx->dash_pck);
			ctx->dash_pck = pck;
			gf_filter_pck_ref_props(&ctx->dash_pck);
		}

		e = scte35dec_process_dispatch(ctx, dts, dur);
	}

	gf_filter_pid_drop_packet(ctx->ipid);

	return e;
}

static const GF_FilterCapability SCTE35DecCaps[] =
{
	CAP_UINT(GF_CAPS_INPUT, GF_PROP_PID_STREAM_TYPE, GF_STREAM_AUDIO),
	CAP_UINT(GF_CAPS_INPUT, GF_PROP_PID_STREAM_TYPE, GF_STREAM_VISUAL),
	CAP_UINT(GF_CAPS_INPUT_EXCLUDED, GF_PROP_PID_CODECID, GF_CODECID_NONE),
	CAP_BOOL(GF_CAPS_INPUT_EXCLUDED, GF_PROP_PID_UNFRAMED, GF_TRUE),

	CAP_UINT(GF_CAPS_OUTPUT_STATIC, GF_PROP_PID_STREAM_TYPE, GF_STREAM_METADATA),
	CAP_UINT(GF_CAPS_OUTPUT_STATIC, GF_PROP_PID_CODECID, GF_CODECID_EVTE),

	{0},

	CAP_UINT(GF_CAPS_INPUT, GF_PROP_PID_STREAM_TYPE, GF_STREAM_METADATA),
	CAP_UINT(GF_CAPS_INPUT, GF_PROP_PID_CODECID, GF_CODECID_SCTE35),
	CAP_UINT(GF_CAPS_INPUT, GF_PROP_PID_CODECID, GF_CODECID_EVTE),
	CAP_BOOL(GF_CAPS_INPUT_EXCLUDED, GF_PROP_PID_UNFRAMED, GF_TRUE),

	CAP_BOOL(GF_CAPS_OUTPUT_STATIC_EXCLUDED, GF_PROP_PID_UNFRAMED, GF_TRUE),
	CAP_UINT(GF_CAPS_OUTPUT_EXCLUDED, GF_PROP_PID_STREAM_TYPE, GF_STREAM_FILE),
	CAP_UINT(GF_CAPS_OUTPUT_EXCLUDED, GF_PROP_PID_CODECID, GF_CODECID_NONE),
};

#define OFFS(_n)	#_n, offsetof(SCTE35DecCtx, _n)
static const GF_FilterArgs SCTE35DecArgs[] =
{
	{ OFFS(mode), "mode to operate in\n"
		"- 23001-18: extract SCTE-35 markers as emib/emeb boxes for Event Tracks\n"
		"- passthrough: pass-through mode adding cue start property on splice points", GF_PROP_UINT, "23001-18", "23001-18|passthrough", 0},
	{ OFFS(segdur), "segmentation duration in seconds. 0/0 flushes immediately for each input packet (beware of the bitrate overhead)", GF_PROP_FRACTION, "1/1", NULL, 0},
	{0}
};

GF_FilterRegister SCTE35DecRegister = {
	.name = "scte35dec",
	GF_FS_SET_DESCRIPTION("SCTE35 decoder")
	GF_FS_SET_HELP("This filter writes the SCTE-35 markers attached as properties to audio and video\n"
	               "packets or inside a dedicated stream, as 23001-18 'emib' boxes. It also creates\n"
				   "empty 'emeb' box in between following segmentation as hinted by the graph.")
	.private_size = sizeof(SCTE35DecCtx),
	.args = SCTE35DecArgs,
	.flags = GF_FS_REG_EXPLICIT_ONLY,
	SETCAPS(SCTE35DecCaps),
	.process = scte35dec_process,
	.process_event = scte35dec_process_event,
	.configure_pid = scte35dec_configure_pid,
	.initialize = scte35dec_initialize,
	.finalize = scte35dec_finalize,
	.hint_class_type = GF_FS_CLASS_DECODER
};

const GF_FilterRegister *scte35dec_register(GF_FilterSession *session)
{
	return &SCTE35DecRegister;
}

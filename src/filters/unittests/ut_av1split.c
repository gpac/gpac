#include "tests.h"
#include <gpac/filters.h>
#include <gpac/bitstream.h>
#include <gpac/internal/media_dev.h>

#if !defined(GPAC_DISABLE_AV_PARSERS) && !defined(GPAC_DISABLE_AV1MERGE) && !defined(GPAC_DISABLE_AV1SPLIT)

//128x64 reduced still-picture headers from aomenc, see av1merge
static const u8 av1split_test_sequence[] = { 0x0a, 0x06, 0x18, 0x19, 0x7f, 0xfe, 0xd8, 0x04 };
static const u8 av1split_test_header[] = { 0x13, 0xc0, 0x00, 0x00, 0x24, 0x00, 0x00, 0x00 };

typedef struct {
	GF_FilterPid *source[6], *sink[6], *middle_in, *middle_out;
	u32 cols, rows, phase, mode, received[6], sizes[6];
} AV1SplitTest;
static AV1SplitTest av1split_test;

static void av1split_test_leb128(GF_BitStream *bs, u32 value)
{
	do {
		u8 byte = value & 127;
		value >>= 7;
		gf_bs_write_u8(bs, byte | (value ? 128 : 0));
	} while (value);
}

static GF_Err av1split_test_source_init(GF_Filter *filter)
{
	u32 i, size;
	u8 *dsi;
	GF_AV1_OBUArrayEntry entry = {0};
	GF_AV1Config *cfg = gf_odf_av1_cfg_new();
	entry.obu = (u8 *) av1split_test_sequence;
	entry.obu_length = sizeof(av1split_test_sequence);
	entry.obu_type = OBU_SEQUENCE_HEADER;
	cfg->chroma_subsampling_x = cfg->chroma_subsampling_y = 1;
	gf_list_add(cfg->obu_array, &entry);
	gf_odf_av1_cfg_write(cfg, &dsi, &size);
	gf_list_reset(cfg->obu_array);
	gf_odf_av1_cfg_del(cfg);
	for (i=0; i<av1split_test.cols * av1split_test.rows; i++) {
		GF_FilterPid *pid = av1split_test.source[i] = gf_filter_pid_new(filter);
		gf_filter_pid_set_property(pid, GF_PROP_PID_STREAM_TYPE, &PROP_UINT(GF_STREAM_VISUAL));
		gf_filter_pid_set_property(pid, GF_PROP_PID_CODECID, &PROP_UINT(GF_CODECID_AV1));
		gf_filter_pid_set_property(pid, GF_PROP_PID_WIDTH, &PROP_UINT(128));
		gf_filter_pid_set_property(pid, GF_PROP_PID_HEIGHT, &PROP_UINT(64));
		gf_filter_pid_set_property(pid, GF_PROP_PID_TIMESCALE, &PROP_UINT(1000));
		gf_filter_pid_set_property(pid, GF_PROP_PID_TILE_ID, &PROP_UINT(i+1));
		gf_filter_pid_set_property(pid, GF_PROP_PID_DECODER_CONFIG, &PROP_DATA(dsi, size));
	}
	gf_free(dsi);
	gf_filter_post_process_task(filter);
	return GF_OK;
}

static GF_Err av1split_test_source_process(GF_Filter *filter)
{
	u32 i, j, count = av1split_test.cols * av1split_test.rows;
	if (av1split_test.phase == 2) {
		for (i=0; i<count; i++) gf_filter_pid_set_eos(av1split_test.source[i]);
		return GF_EOS;
	}
	for (i=0; i<count; i++) {
		u8 *data, *dst;
		u32 size;
		GF_BitStream *bs = gf_bs_new(NULL, 0, GF_BITSTREAM_WRITE);
		GF_FilterPacket *pck;
		gf_bs_write_data(bs, av1split_test_sequence, sizeof(av1split_test_sequence));
		gf_bs_write_u8(bs, 0x32);
		av1split_test_leb128(bs, sizeof(av1split_test_header) + av1split_test.sizes[i]);
		gf_bs_write_data(bs, av1split_test_header, sizeof(av1split_test_header));
		for (j=0; j<av1split_test.sizes[i]; j++) gf_bs_write_u8(bs, i+1+j+av1split_test.phase);
		gf_bs_get_content(bs, &data, &size);
		gf_bs_del(bs);
		pck = gf_filter_pck_new_alloc(av1split_test.source[i], size, &dst);
		memcpy(dst, data, size);
		gf_free(data);
		gf_filter_pck_set_framing(pck, GF_TRUE, GF_TRUE);
		gf_filter_pck_set_dts(pck, 1000*av1split_test.phase);
		gf_filter_pck_set_cts(pck, 1000*av1split_test.phase+10);
		gf_filter_pck_set_duration(pck, 1000);
		gf_filter_pck_set_sap(pck, GF_FILTER_SAP_1);
		gf_filter_pck_send(pck);
	}
	av1split_test.phase++;
	gf_filter_post_process_task(filter);
	return GF_OK;
}

static GF_Err av1split_test_middle_configure(GF_Filter *filter, GF_FilterPid *pid, Bool remove)
{
	if (remove) return GF_OK;
	av1split_test.middle_in = pid;
	if (!av1split_test.middle_out) av1split_test.middle_out = gf_filter_pid_new(filter);
	gf_filter_pid_copy_properties(av1split_test.middle_out, pid);
	if (av1split_test.mode == 6)
		gf_filter_pid_set_property(av1split_test.middle_out, GF_PROP_PID_DECODER_CONFIG, NULL);
	return GF_OK;
}

//Convert OBU_FRAME to a frame header followed by one tile-group OBU per tile.
//Malformed modes remove the last group, reverse group order, or overstate a size.
static GF_Err av1split_test_middle_process(GF_Filter *filter)
{
	GF_FilterPacket *pck = av1split_test.middle_in ? gf_filter_pid_get_packet(av1split_test.middle_in) : NULL;
	GF_BitStream *bs, *out;
	AV1State *s;
	AV1ParseOffsets offsets = {0};
	u32 size, pos, out_size;
	const u8 *data;
	u8 *result, *dst;
	if (!pck) {
		if (av1split_test.middle_in && gf_filter_pid_is_eos(av1split_test.middle_in)) {
			gf_filter_pid_set_eos(av1split_test.middle_out);
			return GF_EOS;
		}
		return GF_OK;
	}
	data = gf_filter_pck_get_data(pck, &size);
	bs = gf_bs_new(data, size, GF_BITSTREAM_READ);
	out = gf_bs_new(NULL, 0, GF_BITSTREAM_WRITE);
	GF_SAFEALLOC(s, AV1State);
	s->config = gf_odf_av1_cfg_new();
	s->parse_offsets = &offsets;
	while (gf_bs_available(bs)) {
		ObuType type;
		u64 len = 0;
		u32 header;
		pos = (u32) gf_bs_get_position(bs);
		GF_Err e = gf_av1_parse_obu(bs, &type, &len, &header, s);
		assert_equal(e, GF_OK, "%d");
		if (e) break;
		if (type != OBU_FRAME) {
			//Mode 5 exercises a decoder config without in-band sequence headers.
			if (av1split_test.mode != 5 || type != OBU_SEQUENCE_HEADER)
				gf_bs_write_data(out, data+pos, (u32) len);
			continue;
		}
		if (!av1split_test.mode || av1split_test.mode == 5) {
			gf_bs_write_data(out, data+pos, (u32) len);
			continue;
		}
		u32 bits = (u32) offsets.header_end - (pos+header)*8;
		gf_bs_write_u8(out, 0x1a);
		av1split_test_leb128(out, (bits+1+7)/8);
		gf_bs_seek(bs, pos+header);
		while (bits--) gf_bs_write_int(out, gf_bs_read_int(bs, 1), 1);
		gf_bs_write_int(out, 1, 1);
		gf_bs_align(out);
		u32 count = s->tileCols * s->tileRows, tile_bits = s->tileColsLog2+s->tileRowsLog2, i;
		for (i=0; i<count; i++) {
			u32 id = (av1split_test.mode == 3) ? count-i-1 : i;
			AV1Tile *t = &s->frame_state.tiles[id];
			if (av1split_test.mode == 2 && i+1 == count) break;
			gf_bs_write_u8(out, 0x22);
			av1split_test_leb128(out, 1+t->size + ((av1split_test.mode == 4 && i+1 == count) ? 100 : 0));
			gf_bs_write_int(out, 1, 1);
			gf_bs_write_int(out, id, tile_bits);
			gf_bs_write_int(out, id, tile_bits);
			gf_bs_align(out);
			gf_bs_write_data(out, data+pos+t->obu_start_offset, t->size);
		}
		gf_bs_seek(bs, pos+len);
	}
	gf_bs_get_content(out, &result, &out_size);
	GF_FilterPacket *p = gf_filter_pck_new_alloc(av1split_test.middle_out, out_size, &dst);
	memcpy(dst, result, out_size);
	gf_free(result);
	gf_filter_pck_merge_properties(pck, p);
	gf_filter_pck_send(p);
	gf_filter_pid_drop_packet(av1split_test.middle_in);
	gf_bs_del(bs);
	gf_bs_del(out);
	gf_odf_av1_cfg_del(s->config);
	gf_free(s);
	return GF_OK;
}

static GF_Err av1split_test_sink_configure(GF_Filter *filter, GF_FilterPid *pid, Bool remove)
{
	const GF_PropertyValue *p = gf_filter_pid_get_property(pid, GF_PROP_PID_TILE_ID);
	assert_true(p && p->value.uint && p->value.uint <= 6);
	if (!p || !p->value.uint || p->value.uint > 6) return GF_BAD_PARAM;
	u32 id = p->value.uint-1;
	if (remove) { av1split_test.sink[id] = NULL; return GF_OK; }
	av1split_test.sink[id] = pid;
	assert_equal(gf_filter_pid_get_property(pid, GF_PROP_PID_WIDTH)->value.uint, 128, "%u");
	assert_equal(gf_filter_pid_get_property(pid, GF_PROP_PID_HEIGHT)->value.uint, 64, "%u");
	p = gf_filter_pid_get_property(pid, GF_PROP_PID_CROP_POS);
	s32 x = 128*(id % av1split_test.cols);
	assert_equal(p->value.vec2i.x, x, "%d");
	assert_equal(p->value.vec2i.y, (s32) (64*(id / av1split_test.cols)), "%d");
	p = gf_filter_pid_get_property(pid, GF_PROP_PID_ORIG_SIZE);
	assert_equal(p->value.vec2i.x, (s32) (128*av1split_test.cols), "%d");
	assert_equal(p->value.vec2i.y, (s32) (64*av1split_test.rows), "%d");
	GF_FilterEvent evt;
	GF_FEVT_INIT(evt, GF_FEVT_PLAY, pid);
	gf_filter_pid_send_event(pid, &evt);
	return GF_OK;
}

static GF_Err av1split_test_sink_process(GF_Filter *filter)
{
	u32 id;
	for (id=0; id<av1split_test.cols*av1split_test.rows; id++) {
		GF_FilterPid *pid = av1split_test.sink[id];
		GF_FilterPacket *pck = pid ? gf_filter_pid_get_packet(pid) : NULL;
		if (!pck) continue;
		u32 size, i, j, frames = 0;
		const u8 *data = gf_filter_pck_get_data(pck, &size);
		const GF_PropertyValue *dsi = gf_filter_pid_get_property(pid, GF_PROP_PID_DECODER_CONFIG);
		AV1State *s;
		GF_SAFEALLOC(s, AV1State);
		s->config = gf_odf_av1_cfg_read(dsi->value.data.ptr, dsi->value.data.size);
		GF_BitStream *bs = gf_bs_new(data, size, GF_BITSTREAM_READ);
		for (i=0; i<gf_list_count(s->config->obu_array); i++) {
			GF_AV1_OBUArrayEntry *obu = gf_list_get(s->config->obu_array, i);
			ObuType type;
			u64 len = obu->obu_length;
			gf_bs_reassign_buffer(bs, obu->obu, (u32) len);
			assert_equal(gf_av1_parse_obu(bs, &type, &len, NULL, s), GF_OK, "%d");
		}
		assert_equal(s->sequence_width, 128, "%u");
		assert_equal(s->sequence_height, 64, "%u");
		gf_bs_reassign_buffer(bs, data, size);
		while (gf_bs_available(bs)) {
			ObuType type;
			u64 len = 0, pos = gf_bs_get_position(bs);
			GF_Err e = gf_av1_parse_obu(bs, &type, &len, NULL, s);
			assert_equal(e, GF_OK, "%d");
			if (e) break;
			if (type != OBU_FRAME && type != OBU_TILE_GROUP) continue;
			frames++;
			assert_equal(s->tileCols, 1, "%u");
			assert_equal(s->tileRows, 1, "%u");
			assert_equal(s->frame_state.nb_tiles_in_obu, 1, "%u");
			AV1Tile *t = &s->frame_state.tiles[0];
			assert_equal(t->size, av1split_test.sizes[id], "%u");
			assert_true(pos+t->obu_start_offset+t->size <= size);
			if (pos+t->obu_start_offset+t->size > size) break;
			for (j=0; j<t->size; j++) {
				if (data[pos+t->obu_start_offset+j] != (u8)(id+1+j+av1split_test.received[id])) break;
			}
			assert_equal(j, t->size, "%u");
		}
		assert_equal(frames, 1, "%u");
		assert_equal(gf_filter_pck_get_dts(pck), (u64)1000*av1split_test.received[id], "%llu");
		assert_equal(gf_filter_pck_get_cts(pck), (u64)1000*av1split_test.received[id]+10, "%llu");
		assert_equal(gf_filter_pck_get_duration(pck), 1000, "%u");
		assert_equal(gf_filter_pck_get_sap(pck), GF_FILTER_SAP_1, "%u");
		av1split_test.received[id]++;
		gf_filter_pid_drop_packet(pid);
		gf_bs_del(bs);
		gf_odf_av1_cfg_del(s->config);
		gf_free(s);
	}
	return GF_OK;
}

static const GF_FilterCapability av1split_test_source_caps[] = {
	CAP_UINT(GF_CAPS_OUTPUT, GF_PROP_PID_STREAM_TYPE, GF_STREAM_VISUAL),
	CAP_UINT(GF_CAPS_OUTPUT, GF_PROP_PID_CODECID, GF_CODECID_AV1)
};
static const GF_FilterCapability av1split_test_sink_caps[] = {
	CAP_UINT(GF_CAPS_INPUT, GF_PROP_PID_STREAM_TYPE, GF_STREAM_VISUAL),
	CAP_UINT(GF_CAPS_INPUT, GF_PROP_PID_CODECID, GF_CODECID_AV1)
};
static const GF_FilterCapability av1split_test_middle_caps[] = {
	CAP_UINT(GF_CAPS_INPUT_OUTPUT, GF_PROP_PID_STREAM_TYPE, GF_STREAM_VISUAL),
	CAP_UINT(GF_CAPS_INPUT_OUTPUT, GF_PROP_PID_CODECID, GF_CODECID_AV1)
};
static const GF_FilterRegister av1split_test_source = {
	.name = "av1split_ut_source", SETCAPS(av1split_test_source_caps),
	.initialize = av1split_test_source_init, .process = av1split_test_source_process
};
static const GF_FilterRegister av1split_test_middle = {
	.name = "av1split_ut_middle", SETCAPS(av1split_test_middle_caps),
	.configure_pid = av1split_test_middle_configure, .process = av1split_test_middle_process
};
static const GF_FilterRegister av1split_test_sink = {
	.name = "av1split_ut_sink", SETCAPS(av1split_test_sink_caps),
	.configure_pid = av1split_test_sink_configure, .process = av1split_test_sink_process
};

static void av1split_test_session(u32 cols, u32 rows, u32 mode)
{
	u32 i;
	GF_Err e;
	GF_Filter *source, *merge, *middle, *split, *sink;
	char args[64];
	GF_FilterSession *session = gf_fs_new(0, GF_FS_SCHEDULER_LOCK_FREE, 0, NULL);
	memset(&av1split_test, 0, sizeof(av1split_test));
	av1split_test.cols = cols;
	av1split_test.rows = rows;
	av1split_test.mode = mode;
	av1split_test.sizes[0] = 127;
	av1split_test.sizes[1] = 256;
	av1split_test.sizes[2] = 65537;
	av1split_test.sizes[3] = 16384;
	av1split_test.sizes[4] = 1;
	av1split_test.sizes[5] = 257;
	gf_fs_add_filter_register(session, &av1split_test_source);
	gf_fs_add_filter_register(session, &av1split_test_middle);
	gf_fs_add_filter_register(session, &av1split_test_sink);
	source = gf_fs_load_filter(session, "av1split_ut_source", &e);
	assert_equal(e, GF_OK, "%d");
	sprintf(args, "av1merge:cols=%u:rows=%u", cols, rows);
	merge = gf_fs_load_filter(session, args, &e);
	assert_equal(e, GF_OK, "%d");
	middle = gf_fs_load_filter(session, "av1split_ut_middle", &e);
	assert_equal(e, GF_OK, "%d");
	split = gf_fs_load_filter(session, "av1split", &e);
	assert_equal(e, GF_OK, "%d");
	if (!split) { gf_fs_del(session); return; }
	sink = gf_fs_load_filter(session, "av1split_ut_sink", &e);
	assert_equal(e, GF_OK, "%d");
	assert_equal(gf_filter_set_source(merge, source, NULL), GF_OK, "%d");
	assert_equal(gf_filter_set_source(middle, merge, NULL), GF_OK, "%d");
	assert_equal(gf_filter_set_source(split, middle, NULL), GF_OK, "%d");
	assert_equal(gf_filter_set_source(sink, split, NULL), GF_OK, "%d");
	gf_fs_run(session);
	Bool malformed = mode >= 2 && mode <= 4;
	assert_equal(gf_fs_get_last_connect_error(session), GF_OK, "%d");
	assert_equal(gf_fs_get_last_process_error(session), (malformed ? GF_NON_COMPLIANT_BITSTREAM : GF_OK), "%d");
	for (i=0; i<cols*rows; i++) assert_equal(av1split_test.received[i], (malformed ? 0 : 2), "%u");
	gf_fs_del(session);
}
#endif

unittest(av1split_roundtrip)
{
#if !defined(GPAC_DISABLE_AV_PARSERS) && !defined(GPAC_DISABLE_AV1MERGE) && !defined(GPAC_DISABLE_AV1SPLIT)
	av1split_test_session(1, 1, 0);
	av1split_test_session(2, 1, 0);
	av1split_test_session(1, 2, 0);
	av1split_test_session(3, 2, 0);
	av1split_test_session(3, 2, 1);
	av1split_test_session(2, 1, 5);
	av1split_test_session(2, 1, 6);
#endif
}

unittest(av1split_invalid_groups)
{
#if !defined(GPAC_DISABLE_AV_PARSERS) && !defined(GPAC_DISABLE_AV1MERGE) && !defined(GPAC_DISABLE_AV1SPLIT)
	av1split_test_session(2, 1, 2);
	av1split_test_session(2, 1, 3);
	av1split_test_session(2, 1, 4);
#endif
}

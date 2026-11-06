#include "tests.h"
#include <gpac/filters.h>
#include <gpac/bitstream.h>
#include <gpac/internal/media_dev.h>

#if !defined(GPAC_DISABLE_AV_PARSERS) && !defined(GPAC_DISABLE_AV1MERGE)

//a 128x64 reduced still-picture sequence and frame header generated from a red yuv420p frame:
// aomenc --cpu-used=8 --limit=1 --passes=1 --end-usage=q --cq-level=30 --obu
static const u8 av1merge_sequence[] = { 0x0a, 0x06, 0x18, 0x19, 0x7f, 0xfe, 0xd8, 0x04 };
static const u8 av1merge_header  [] = { 0x13, 0xc0, 0x00, 0x00, 0x24, 0x00, 0x00, 0x00 };

typedef struct {
	GF_FilterPid *pids[6], *sink_pid;
	u32 count, cols, rows, phase, received;
	u32 sizes[6];
	u32 mode; // 1: unequal duration, 2: offset timestamps, 3: separate tile groups
	          // 4/5: incomplete final unit on tile 1/2; 6/7: same mismatch followed by more packets
} AV1MergeTest;

static AV1MergeTest av1merge_test;

static void av1merge_test_write_leb128(GF_BitStream *bs, u32 value)
{
	do {
		u8 byte = value & 127;
		value >>= 7;
		gf_bs_write_u8(bs, byte | (value ? 128 : 0));
	} while (value);
}

static GF_Err av1merge_test_source_init(GF_Filter *filter)
{
	u32 i, dsi_size;
	u8 *dsi;
	GF_AV1_OBUArrayEntry entry = {0};
	GF_AV1Config *config = gf_odf_av1_cfg_new();
	entry.obu = (u8 *) av1merge_sequence;
	entry.obu_length = sizeof(av1merge_sequence);
	entry.obu_type = OBU_SEQUENCE_HEADER;
	config->seq_profile = 0;
	config->chroma_subsampling_x = config->chroma_subsampling_y = 1;
	gf_list_add(config->obu_array, &entry);
	gf_odf_av1_cfg_write(config, &dsi, &dsi_size);
	gf_list_reset(config->obu_array);
	gf_odf_av1_cfg_del(config);
	//connect in reverse TileID order to exercise grid ordering
	for (i = 0; i < av1merge_test.count; i++) {
		GF_FilterPid *pid = gf_filter_pid_new(filter);
		av1merge_test.pids[i] = pid;
		gf_filter_pid_set_property(pid, GF_PROP_PID_STREAM_TYPE, &PROP_UINT(GF_STREAM_VISUAL));
		gf_filter_pid_set_property(pid, GF_PROP_PID_CODECID, &PROP_UINT(GF_CODECID_AV1));
		gf_filter_pid_set_property(pid, GF_PROP_PID_WIDTH, &PROP_UINT(128));
		gf_filter_pid_set_property(pid, GF_PROP_PID_HEIGHT, &PROP_UINT(64));
		gf_filter_pid_set_property(pid, GF_PROP_PID_TIMESCALE, &PROP_UINT(1000 * (av1merge_test.count-i)));
		gf_filter_pid_set_property(pid, GF_PROP_PID_TILE_ID, &PROP_UINT(av1merge_test.count-i));
		gf_filter_pid_set_property(pid, GF_PROP_PID_DECODER_CONFIG, &PROP_DATA(dsi, dsi_size));
	}
	gf_free(dsi);
	gf_filter_post_process_task(filter);
	return GF_OK;
}

static GF_Err av1merge_test_source_process(GF_Filter *filter)
{
	u32 i, size, id, index, phase = av1merge_test.phase++;
	u32 end_phase = ((av1merge_test.mode == 6 || av1merge_test.mode == 7) ? 6 : 4) * av1merge_test.count;
	Bool extra_frame;
	u8 *data, *dst;
	GF_FilterPacket *pck;
	GF_BitStream *bs;
	//one input is fed every call but av1merge must retain earlier
	//packets until all tiles at this timestamp have arrived
	if (phase >= end_phase + 4) {
		for (i = 0; i < av1merge_test.count; i++) gf_filter_pid_set_eos(av1merge_test.pids[i]);
		return GF_EOS;
	}
	gf_filter_post_process_task(filter);
	//signal EOS separately from the last packets, exercising delayed EOS
	if (phase >= end_phase) return GF_OK;
	if (phase & 1) return GF_OK;
	index = (phase / 2) % av1merge_test.count;
	id = av1merge_test.count - index;
	if ((av1merge_test.mode == 1) && phase >= 2 * av1merge_test.count && !index) {
		gf_filter_pid_set_eos(av1merge_test.pids[index]);
		return GF_OK;
	}
	bs = gf_bs_new(NULL, 0, GF_BITSTREAM_WRITE);
	gf_bs_write_data(bs, av1merge_sequence, sizeof(av1merge_sequence));
	if (av1merge_test.mode == 3) {
		gf_bs_write_u8(bs, 0x1a); //OBU_FRAME_HEADER
		av1merge_test_write_leb128(bs, sizeof(av1merge_header) + 1);
		gf_bs_write_data(bs, av1merge_header, sizeof(av1merge_header));
		gf_bs_write_u8(bs, 0x80); //trailing bits
		gf_bs_write_u8(bs, 0x22); //OBU_TILE_GROUP
		av1merge_test_write_leb128(bs, av1merge_test.sizes[id-1]);
	} else {
		gf_bs_write_u8(bs, 0x32); //OBU_FRAME
		av1merge_test_write_leb128(bs, sizeof(av1merge_header) + av1merge_test.sizes[id-1]);
		gf_bs_write_data(bs, av1merge_header, sizeof(av1merge_header));
	}
	for (i = 0; i < av1merge_test.sizes[id-1]; i++) gf_bs_write_u8(bs, id + i);
	gf_bs_get_content(bs, &data, &size);
	gf_bs_del(bs);
	extra_frame = (phase >= 2 * av1merge_test.count && phase < 4 * av1merge_test.count)
		&& (((av1merge_test.mode == 4 || av1merge_test.mode == 6) && id == 2)
		|| ((av1merge_test.mode == 5 || av1merge_test.mode == 7) && id == 1));
	pck = gf_filter_pck_new_alloc(av1merge_test.pids[index], size + (extra_frame ? size - sizeof(av1merge_sequence) : 0), &dst);
	memcpy(dst, data, size);
	if (extra_frame) memcpy(dst + size, data + sizeof(av1merge_sequence), size - sizeof(av1merge_sequence));
	gf_free(data);
	gf_filter_pck_set_framing(pck, GF_TRUE, GF_TRUE);
	u64 dts = (phase / (2 * av1merge_test.count)) * 1000;
	if (av1merge_test.mode == 2 && id == 1) dts += 1000;
	gf_filter_pck_set_dts(pck, dts * id);
	gf_filter_pck_set_cts(pck, dts * id);
	gf_filter_pck_set_sap(pck, GF_FILTER_SAP_1);
	return gf_filter_pck_send(pck);
}

static GF_Err av1merge_test_sink_configure(GF_Filter *filter, GF_FilterPid *pid, Bool is_remove)
{
	if (!is_remove) {
		GF_FilterEvent evt;
		av1merge_test.sink_pid = pid;
		GF_FEVT_INIT(evt, GF_FEVT_PLAY, pid);
		gf_filter_pid_send_event(pid, &evt);
	}
	return GF_OK;
}

static GF_Err av1merge_test_sink_process(GF_Filter *filter)
{
	GF_FilterPacket *pck;
	AV1State *state;
	GF_BitStream *bs;
	u32 i, j, size, frames = 0;
	const u8 *data;
	if (!av1merge_test.sink_pid) return GF_OK;
	pck = gf_filter_pid_get_packet(av1merge_test.sink_pid);
	if (!pck) return gf_filter_pid_is_eos(av1merge_test.sink_pid) ? GF_EOS : GF_OK;
	assert_greater_equal(av1merge_test.phase, 2 * av1merge_test.count - 1, "%u");
	assert_equal(gf_filter_pid_get_property(av1merge_test.sink_pid, GF_PROP_PID_WIDTH)->value.uint, 128 * av1merge_test.cols, "%u");
	assert_equal(gf_filter_pid_get_property(av1merge_test.sink_pid, GF_PROP_PID_HEIGHT)->value.uint, 64 * av1merge_test.rows, "%u");
	data = gf_filter_pck_get_data(pck, &size);
	bs = gf_bs_new(data, size, GF_BITSTREAM_READ);
	GF_SAFEALLOC(state, AV1State);
	state->config = gf_odf_av1_cfg_new();
	while (gf_bs_available(bs)) {
		ObuType type;
		u64 obu_size = 0, pos = gf_bs_get_position(bs);
		GF_Err e = gf_av1_parse_obu(bs, &type, &obu_size, NULL, state);
		assert_equal(e, GF_OK, "%d");
		if (e) break;
		if (type != OBU_FRAME && type != OBU_TILE_GROUP) continue;
		frames++;
		assert_equal(state->width, 128 * av1merge_test.cols, "%u");
		assert_equal(state->height, 64 * av1merge_test.rows, "%u");
		assert_equal(state->tileCols, av1merge_test.cols, "%u");
		assert_equal(state->tileRows, av1merge_test.rows, "%u");
		assert_equal(state->frame_state.nb_tiles_in_obu, av1merge_test.count, "%u");
		for (i = 0; i < av1merge_test.count; i++) {
			AV1Tile *tile = &state->frame_state.tiles[i];
			assert_equal(tile->size, av1merge_test.sizes[i], "%u");
			assert_true(pos + tile->obu_start_offset + tile->size <= size);
			if (pos + tile->obu_start_offset + tile->size > size) break;
			for (j = 0; j < tile->size; j++) {
				if (data[pos + tile->obu_start_offset + j] != (u8)(i+1+j)) break;
			}
			assert_equal(j, tile->size, "%u");
		}
	}
	assert_equal(frames, 1, "%u");
	gf_odf_av1_cfg_del(state->config);
	gf_free(state);
	gf_bs_del(bs);
	av1merge_test.received++;
	gf_filter_pid_drop_packet(av1merge_test.sink_pid);
	return GF_OK;
}

static const GF_FilterCapability av1merge_test_source_caps[] = {
	CAP_UINT(GF_CAPS_OUTPUT, GF_PROP_PID_STREAM_TYPE, GF_STREAM_VISUAL),
	CAP_UINT(GF_CAPS_OUTPUT, GF_PROP_PID_CODECID, GF_CODECID_AV1)
};
static const GF_FilterCapability av1merge_test_sink_caps[] = {
	CAP_UINT(GF_CAPS_INPUT, GF_PROP_PID_STREAM_TYPE, GF_STREAM_VISUAL),
	CAP_UINT(GF_CAPS_INPUT, GF_PROP_PID_CODECID, GF_CODECID_AV1)
};
static const GF_FilterRegister av1merge_test_source = {
	.name = "av1merge_ut_source",
	SETCAPS(av1merge_test_source_caps),
	.initialize = av1merge_test_source_init,
	.process = av1merge_test_source_process
};
static const GF_FilterRegister av1merge_test_sink = {
	.name = "av1merge_ut_sink",
	SETCAPS(av1merge_test_sink_caps),
	.configure_pid = av1merge_test_sink_configure,
	.process = av1merge_test_sink_process
};

static void av1merge_test_session(const char *args, u32 cols, u32 rows, u32 mode, GF_Err expected_error)
{
	GF_Err e;
	GF_Filter *source, *merge, *sink;
	GF_FilterSession *session = gf_fs_new(0, GF_FS_SCHEDULER_LOCK_FREE, 0, NULL);
	memset(&av1merge_test, 0, sizeof(av1merge_test));
	av1merge_test.cols = cols;
	av1merge_test.rows = rows;
	av1merge_test.count = cols * rows;
	av1merge_test.mode = mode;
	av1merge_test.sizes[0] = 127;
	av1merge_test.sizes[1] = 256;
	av1merge_test.sizes[2] = 65537;
	av1merge_test.sizes[3] = 16384;
	av1merge_test.sizes[4] = 1;
	av1merge_test.sizes[5] = 257;
	gf_fs_add_filter_register(session, &av1merge_test_source);
	gf_fs_add_filter_register(session, &av1merge_test_sink);
	source = gf_fs_load_filter(session, "av1merge_ut_source", &e);
	assert_equal(e, GF_OK, "%d");
	merge = gf_fs_load_filter(session, args, &e);
	if (!merge) {
		assert_equal(e, expected_error, "%d");
		gf_fs_del(session);
		return;
	}
	assert_equal(e, GF_OK, "%d");
	sink = gf_fs_load_filter(session, "av1merge_ut_sink", &e);
	assert_equal(e, GF_OK, "%d");
	assert_equal(gf_filter_set_source(merge, source, NULL), GF_OK, "%d");
	assert_equal(gf_filter_set_source(sink, merge, NULL), GF_OK, "%d");
	gf_fs_run(session);
	assert_equal(gf_fs_get_last_connect_error(session), GF_OK, "%d");
	assert_equal(gf_fs_get_last_process_error(session), expected_error, "%d");
	assert_equal(av1merge_test.received, ((mode >= 4) ? 1 : (expected_error ? 0 : ((mode == 1 || mode == 2) ? 1 : 2))), "%u");
	gf_fs_del(session);
}
#endif

unittest(av1merge_layout)
{
#if !defined(GPAC_DISABLE_AV_PARSERS) && !defined(GPAC_DISABLE_AV1MERGE)
	av1merge_test_session("av1merge:rows=3", 2, 1, 0, GF_BAD_PARAM);
	av1merge_test_session("av1merge:cols=3", 2, 1, 0, GF_BAD_PARAM);
	av1merge_test_session("av1merge:rows=2:cols=2", 2, 1, 0, GF_BAD_PARAM);
	av1merge_test_session("av1merge:cols=65", 2, 1, 0, GF_BAD_PARAM);
#endif
}

unittest(av1merge_packets)
{
#if !defined(GPAC_DISABLE_AV_PARSERS) && !defined(GPAC_DISABLE_AV1MERGE)
	av1merge_test_session("av1merge", 1, 1, 0, GF_OK);
	av1merge_test_session("av1merge", 1, 2, 0, GF_OK);
	av1merge_test_session("av1merge:rows=1", 2, 1, 0, GF_OK);
	av1merge_test_session("av1merge:cols=3:rows=2", 3, 2, 0, GF_OK);
	av1merge_test_session("av1merge:cols=2", 2, 2, 1, GF_OK);
	av1merge_test_session("av1merge:cols=2", 2, 1, 2, GF_OK);
	av1merge_test_session("av1merge:cols=2", 2, 1, 3, GF_OK);
#endif
}


unittest(av1merge_incomplete_temporal_unit)
{
#if !defined(GPAC_DISABLE_AV_PARSERS) && !defined(GPAC_DISABLE_AV1MERGE)
	av1merge_test_session("av1merge:cols=2", 2, 1, 4, GF_OK);
	av1merge_test_session("av1merge:cols=2", 2, 1, 5, GF_OK);
	av1merge_test_session("av1merge:cols=2", 2, 1, 6, GF_NON_COMPLIANT_BITSTREAM);
	av1merge_test_session("av1merge:cols=2", 2, 1, 7, GF_NON_COMPLIANT_BITSTREAM);
#endif
}

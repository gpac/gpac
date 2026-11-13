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
	u32 widths[6], heights[6], out_width, out_height;
	u32 grid_widths[6], grid_heights[6], tile_index[6];
	Bool sparse, render;
	s32 pos_x[6], pos_y[6];
	u8 sequences[6][sizeof(av1merge_sequence)];
	u8 headers[6][16];
	u32 header_sizes[6];
	u32 mode; // 1: unequal duration, 2: offset timestamps, 3: separate tile groups
	          // 4/5: incomplete final unit on tile 1/2; 6/7: same mismatch followed by more packets
	          // 8: explicit render sizes; 9: mismatched coding configuration
} AV1MergeTest;

static AV1MergeTest av1merge_test;

enum {
	AV1MERGE_MISSING_ORIGIN = 10,
	AV1MERGE_NEGATIVE_ORIGIN,
	AV1MERGE_DUPLICATE_ORIGIN,
	AV1MERGE_HORIZONTAL_GAP,
	AV1MERGE_HORIZONTAL_OVERLAP,
	AV1MERGE_VERTICAL_GAP,
	AV1MERGE_SHIFTED_COLUMN,
	AV1MERGE_CHANGED_ORIGIN,
	AV1MERGE_IGNORED_TILE_ID,
	AV1MERGE_REMOVED_ORIGIN,
	AV1MERGE_OFFSET_ORIGIN,
	AV1MERGE_MISSING_CELL,
	AV1MERGE_UNALIGNED_ORIGIN,
	AV1MERGE_RELATIVE_ORIGIN,
	AV1MERGE_RELATIVE_OFFSET,
	AV1MERGE_RELATIVE_GAP,
	AV1MERGE_IMPLICIT,
	AV1MERGE_SRD,
	AV1MERGE_RELATIVE_SRD,
	AV1MERGE_IMPLICIT_SRD,
	AV1MERGE_CHANGED_SRD,
	AV1MERGE_PARTIAL_SRD,
	AV1MERGE_REMOVED_SRD,
	AV1MERGE_INVALID_SRD,
	AV1MERGE_INVALID_SRD_REF,
	AV1MERGE_OVERFLOW_SRD,
	AV1MERGE_RELATIVE_MIN,
	AV1MERGE_MISSING_FIRST,
	AV1MERGE_MISSING_INTERIOR,
	AV1MERGE_SPARSE_GROUPS,
	AV1MERGE_SPARSE_RENDER,
	AV1MERGE_GAP_RENDER,
	AV1MERGE_CHANGED_GAP,
	AV1MERGE_QP_MISMATCH,
	AV1MERGE_QP_GROUPS,
	AV1MERGE_QP_LATE_MISMATCH,
	AV1MERGE_QP_MATCHED_CHANGE,
	AV1MERGE_TRACK_MATRIX
};

static Bool av1merge_test_implicit(void)
{
	return av1merge_test.mode == AV1MERGE_IMPLICIT || av1merge_test.mode == AV1MERGE_IMPLICIT_SRD
		|| av1merge_test.mode == AV1MERGE_PARTIAL_SRD || av1merge_test.mode == AV1MERGE_REMOVED_SRD;
}

static void av1merge_test_sequence(u8 *sequence, u32 width, u32 height)
{
	GF_BitStream *src = gf_bs_new(av1merge_sequence, sizeof(av1merge_sequence), GF_BITSTREAM_READ);
	GF_BitStream *dst = gf_bs_new(NULL, 0, GF_BITSTREAM_WRITE);
	u8 *data;
	u32 size, width_bits, height_bits;
	//OBU header and size, then reduced still-picture fields through seq_level_idx.
	gf_bs_write_int(dst, gf_bs_read_int(src, 26), 26);
	width_bits = gf_bs_read_int(src, 4) + 1;
	height_bits = gf_bs_read_int(src, 4) + 1;
	gf_bs_write_int(dst, width_bits - 1, 4);
	gf_bs_write_int(dst, height_bits - 1, 4);
	gf_bs_read_int(src, width_bits);
	gf_bs_read_int(src, height_bits);
	gf_bs_write_int(dst, width - 1, width_bits);
	gf_bs_write_int(dst, height - 1, height_bits);
	for (u32 i = 34 + width_bits + height_bits; i < sizeof(av1merge_sequence) * 8; i++)
		gf_bs_write_int(dst, gf_bs_read_int(src, 1), 1);
	gf_bs_get_content(dst, &data, &size);
	assert_equal(size, (u32) sizeof(av1merge_sequence), "%u");
	memcpy(sequence, data, sizeof(av1merge_sequence));
	gf_free(data);
	gf_bs_del(src);
	gf_bs_del(dst);
}

static u32 av1merge_test_header(u8 *header, u32 width, u32 height, Bool render, s32 base_q_idx)
{
	const u8 frame[] = {0x32, 0x09, 0x13, 0xc0, 0x00, 0x00, 0x24, 0x00, 0x00, 0x00, 0x01};
	AV1State *state;
	AV1ParseOffsets offsets = {0};
	GF_BitStream *src = gf_bs_new(av1merge_sequence, sizeof(av1merge_sequence), GF_BITSTREAM_READ);
	GF_BitStream *dst = gf_bs_new(NULL, 0, GF_BITSTREAM_WRITE);
	ObuType type;
	u64 obu_size = 0;
	u32 size, i;
	u8 *data;
	GF_SAFEALLOC(state, AV1State);
	state->config = gf_odf_av1_cfg_new();
	state->parse_offsets = &offsets;
	assert_equal(gf_av1_parse_obu(src, &type, &obu_size, NULL, state), GF_OK, "%d");
	gf_bs_reassign_buffer(src, frame, sizeof(frame));
	assert_equal(gf_av1_parse_obu(src, &type, &obu_size, NULL, state), GF_OK, "%d");
	gf_bs_seek(src, 2);
	for (i = 16; i < offsets.render_size; i++)
		gf_bs_write_int(dst, gf_bs_read_int(src, 1), 1);
	gf_bs_read_int(src, 1);
	gf_bs_write_int(dst, render, 1);
	if (render) {
		gf_bs_write_int(dst, width * 2 - 1, 16);
		gf_bs_write_int(dst, height * 2 - 1, 16);
	}
	for (i = (u32) offsets.render_size + 1; i < offsets.tile_info; i++)
		gf_bs_write_int(dst, gf_bs_read_int(src, 1), 1);
	gf_bs_write_int(dst, 1, 1); //uniform spacing, one tile
	if (width > 64) gf_bs_write_int(dst, 0, 1);
	gf_bs_seek(src, offsets.tile_info_end / 8);
	if (offsets.tile_info_end % 8) gf_bs_read_int(src, offsets.tile_info_end % 8);
	u32 qp = gf_bs_read_int(src, 8);
	gf_bs_write_int(dst, base_q_idx < 0 ? qp : base_q_idx, 8);
	for (i = (u32) offsets.tile_info_end + 8; i < offsets.header_end; i++)
		gf_bs_write_int(dst, gf_bs_read_int(src, 1), 1);
	gf_bs_align(dst);
	gf_bs_get_content(dst, &data, &size);
	assert_less_equal(size, 16U, "%u");
	memcpy(header, data, size);
	gf_free(data);
	gf_bs_del(src);
	gf_bs_del(dst);
	gf_odf_av1_cfg_del(state->config);
	gf_free(state);
	return size;
}

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
	//Reverse explicit inputs; implicit equal-size inputs retain connection order.
	for (i = 0; i < av1merge_test.count; i++) {
		u32 id = av1merge_test_implicit() ? i : av1merge_test.count-i-1;
		av1merge_test_sequence(av1merge_test.sequences[id], av1merge_test.widths[id], av1merge_test.heights[id]);
		if (av1merge_test.mode == 9 && id == 1) av1merge_test.sequences[id][3] |= 0x40; //seq_level_idx_0
		s32 qp = -1;
		if (av1merge_test.mode >= AV1MERGE_QP_MISMATCH && av1merge_test.mode <= AV1MERGE_QP_MATCHED_CHANGE) {
			qp = 80;
			if (id == 1 && (av1merge_test.mode == AV1MERGE_QP_MISMATCH || av1merge_test.mode == AV1MERGE_QP_GROUPS))
				qp = 140;
		}
		av1merge_test.header_sizes[id] = av1merge_test_header(av1merge_test.headers[id], av1merge_test.widths[id], av1merge_test.heights[id], av1merge_test.render, qp);
		entry.obu = av1merge_test.sequences[id];
		gf_odf_av1_cfg_write(config, &dsi, &dsi_size);
		GF_FilterPid *pid = gf_filter_pid_new(filter);
		av1merge_test.pids[i] = pid;
		gf_filter_pid_set_property(pid, GF_PROP_PID_STREAM_TYPE, &PROP_UINT(GF_STREAM_VISUAL));
		gf_filter_pid_set_property(pid, GF_PROP_PID_CODECID, &PROP_UINT(GF_CODECID_AV1));
		gf_filter_pid_set_property(pid, GF_PROP_PID_WIDTH, &PROP_UINT(av1merge_test.widths[id]));
		gf_filter_pid_set_property(pid, GF_PROP_PID_HEIGHT, &PROP_UINT(av1merge_test.heights[id]));
		gf_filter_pid_set_property(pid, GF_PROP_PID_TIMESCALE, &PROP_UINT(1000 * (id+1)));
		if (!av1merge_test_implicit() && (av1merge_test.mode != AV1MERGE_MISSING_ORIGIN || id != av1merge_test.count-1))
			gf_filter_pid_set_property(pid, GF_PROP_PID_CROP_POS, &PROP_VEC2I_INT(av1merge_test.pos_x[id], av1merge_test.pos_y[id]));
		if (av1merge_test.mode == AV1MERGE_SRD || av1merge_test.mode == AV1MERGE_RELATIVE_SRD
			|| av1merge_test.mode == AV1MERGE_IMPLICIT_SRD || av1merge_test.mode == AV1MERGE_CHANGED_SRD
			|| av1merge_test.mode == AV1MERGE_REMOVED_SRD || av1merge_test.mode == AV1MERGE_INVALID_SRD
			|| av1merge_test.mode == AV1MERGE_INVALID_SRD_REF || av1merge_test.mode == AV1MERGE_OVERFLOW_SRD
			|| (av1merge_test.mode == AV1MERGE_PARTIAL_SRD && !id)) {
			s32 x = 64 * id, width = 64, ref_width = 512;
			if (av1merge_test.mode == AV1MERGE_INVALID_SRD) width = 0;
			if (av1merge_test.mode == AV1MERGE_INVALID_SRD_REF) ref_width = 0;
			if (av1merge_test.mode == AV1MERGE_OVERFLOW_SRD) {
				x = 2147483647;
				ref_width = 1;
			}
			gf_filter_pid_set_property(pid, GF_PROP_PID_SRD, &PROP_VEC4I_INT(x, 32, width, 32));
			gf_filter_pid_set_property(pid, GF_PROP_PID_SRD_REF, &PROP_VEC2I_INT(ref_width, 256));
		}
		if (av1merge_test.mode == AV1MERGE_IGNORED_TILE_ID)
			gf_filter_pid_set_property(pid, GF_PROP_PID_TILE_ID, &PROP_UINT(99));
		gf_filter_pid_set_property(pid, GF_PROP_PID_ORIG_SIZE, &PROP_VEC2I_INT(av1merge_test.out_width, av1merge_test.out_height));
		if (av1merge_test.mode == AV1MERGE_TRACK_MATRIX) {
			s32 matrix[9] = {0, 65536, 0, -65536, 0, 0, 64*65536, 64*65536, 0x40000000};
			GF_PropertyValue prop = {0};
			prop.type = GF_PROP_SINT_LIST;
			prop.value.sint_list.nb_items = 9;
			prop.value.sint_list.vals = matrix;
			gf_filter_pid_set_property(pid, GF_PROP_PID_ISOM_TRACK_MATRIX, &prop);
			gf_filter_pid_set_property(pid, GF_PROP_PID_TRANS_X, &PROP_SINT(64));
			gf_filter_pid_set_property(pid, GF_PROP_PID_TRANS_Y, &PROP_SINT(64));
		}
		gf_filter_pid_set_property_str(pid, "av1_split", &PROP_BOOL(GF_TRUE));
		gf_filter_pid_set_property(pid, GF_PROP_PID_DECODER_CONFIG, &PROP_DATA(dsi, dsi_size));
		gf_free(dsi);
	}
	gf_list_reset(config->obu_array);
	gf_odf_av1_cfg_del(config);
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
	if (phase == 2 * av1merge_test.count) {
		if (av1merge_test.mode == AV1MERGE_CHANGED_ORIGIN) {
			for (i = 0; i < av1merge_test.count; i++)
				gf_filter_pid_set_property(av1merge_test.pids[i], GF_PROP_PID_CROP_POS, &PROP_VEC2I_INT(0, 64 * (av1merge_test.count-i-1)));
		} else if (av1merge_test.mode == AV1MERGE_REMOVED_ORIGIN) {
			gf_filter_pid_set_property(av1merge_test.pids[0], GF_PROP_PID_CROP_POS, NULL);
		} else if (av1merge_test.mode == AV1MERGE_CHANGED_SRD) {
			for (i = 0; i < av1merge_test.count; i++)
				gf_filter_pid_set_property(av1merge_test.pids[i], GF_PROP_PID_SRD, &PROP_VEC4I_INT(64 * (av1merge_test.count-i-1), 64, 64, 32));
		} else if (av1merge_test.mode == AV1MERGE_REMOVED_SRD) {
			for (i = 0; i < av1merge_test.count; i++) {
				gf_filter_pid_set_property(av1merge_test.pids[i], GF_PROP_PID_SRD, NULL);
				gf_filter_pid_set_property(av1merge_test.pids[i], GF_PROP_PID_SRD_REF, NULL);
			}
		} else if (av1merge_test.mode == AV1MERGE_CHANGED_GAP) {
			for (i = 0; i < av1merge_test.count; i++)
				gf_filter_pid_set_property(av1merge_test.pids[i], GF_PROP_PID_CROP_POS, &PROP_VEC2I_INT(128 * (av1merge_test.count-i-1), 0));
		}
	}
	index = (phase / 2) % av1merge_test.count;
	id = av1merge_test_implicit() ? index+1 : av1merge_test.count - index;
	if ((av1merge_test.mode == 1) && phase >= 2 * av1merge_test.count && !index) {
		gf_filter_pid_set_eos(av1merge_test.pids[index]);
		return GF_OK;
	}
	if (phase >= 2 * av1merge_test.count
		&& (av1merge_test.mode == AV1MERGE_QP_LATE_MISMATCH || av1merge_test.mode == AV1MERGE_QP_MATCHED_CHANGE)) {
		s32 qp = av1merge_test.mode == AV1MERGE_QP_MATCHED_CHANGE || id == 2 ? 140 : 80;
		av1merge_test.header_sizes[id-1] = av1merge_test_header(av1merge_test.headers[id-1],
			av1merge_test.widths[id-1], av1merge_test.heights[id-1], av1merge_test.render, qp);
	}
	bs = gf_bs_new(NULL, 0, GF_BITSTREAM_WRITE);
	gf_bs_write_data(bs, av1merge_test.sequences[id-1], sizeof(av1merge_sequence));
	if (av1merge_test.mode == 3 || av1merge_test.mode == AV1MERGE_SPARSE_GROUPS || av1merge_test.mode == AV1MERGE_QP_GROUPS) {
		gf_bs_write_u8(bs, 0x1a); //OBU_FRAME_HEADER
		av1merge_test_write_leb128(bs, av1merge_test.header_sizes[id-1] + 1);
		gf_bs_write_data(bs, av1merge_test.headers[id-1], av1merge_test.header_sizes[id-1]);
		gf_bs_write_u8(bs, 0x80); //trailing bits
		gf_bs_write_u8(bs, 0x22); //OBU_TILE_GROUP
		av1merge_test_write_leb128(bs, av1merge_test.sizes[id-1]);
	} else {
		gf_bs_write_u8(bs, 0x32); //OBU_FRAME
		av1merge_test_write_leb128(bs, av1merge_test.header_sizes[id-1] + av1merge_test.sizes[id-1]);
		gf_bs_write_data(bs, av1merge_test.headers[id-1], av1merge_test.header_sizes[id-1]);
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
	u32 i, j, size, headers = 0, groups = 0, payloads = 0;
	u32 cols = av1merge_test.cols, rows = av1merge_test.rows;
	u32 out_width = av1merge_test.out_width, out_height = av1merge_test.out_height;
	Bool sparse = av1merge_test.sparse;
	const u8 *data;
	if (!av1merge_test.sink_pid) return GF_OK;
	pck = gf_filter_pid_get_packet(av1merge_test.sink_pid);
	if (!pck) return gf_filter_pid_is_eos(av1merge_test.sink_pid) ? GF_EOS : GF_OK;
	if (av1merge_test.mode == AV1MERGE_CHANGED_ORIGIN && av1merge_test.received) {
		cols = 1;
		rows = av1merge_test.count;
		out_width = 128;
		out_height = 64 * rows;
	} else if (av1merge_test.mode == AV1MERGE_CHANGED_GAP && av1merge_test.received) {
		cols = 2;
		out_width = 256;
		sparse = GF_FALSE;
	}
	assert_greater_equal(av1merge_test.phase, 2 * av1merge_test.count - 1, "%u");
	assert_equal(gf_filter_pid_get_property(av1merge_test.sink_pid, GF_PROP_PID_WIDTH)->value.uint, out_width, "%u");
	assert_equal(gf_filter_pid_get_property(av1merge_test.sink_pid, GF_PROP_PID_HEIGHT)->value.uint, out_height, "%u");
	assert_true(!gf_filter_pid_get_property(av1merge_test.sink_pid, GF_PROP_PID_CROP_POS));
	assert_true(!gf_filter_pid_get_property(av1merge_test.sink_pid, GF_PROP_PID_ORIG_SIZE));
	assert_true(!gf_filter_pid_get_property(av1merge_test.sink_pid, GF_PROP_PID_TILE_ID));
	assert_true(!gf_filter_pid_get_property_str(av1merge_test.sink_pid, "av1_split"));
	if (av1merge_test.mode == AV1MERGE_TRACK_MATRIX) {
		const s32 expected[9] = {0, 65536, 0, -65536, 0, 0, 0, 0, 0x40000000};
		const GF_PropertyValue *matrix = gf_filter_pid_get_property(av1merge_test.sink_pid, GF_PROP_PID_ISOM_TRACK_MATRIX);
		assert_true(!gf_filter_pid_get_property(av1merge_test.sink_pid, GF_PROP_PID_TRANS_X));
		assert_true(!gf_filter_pid_get_property(av1merge_test.sink_pid, GF_PROP_PID_TRANS_Y));
		assert_true(matrix != NULL);
		if (matrix) {
			assert_equal(matrix->value.sint_list.nb_items, 9U, "%u");
			if (matrix->value.sint_list.nb_items == 9)
				assert_equal_mem(matrix->value.sint_list.vals, expected, sizeof(expected));
		}
	}
	const GF_PropertyValue *map = gf_filter_pid_get_property(av1merge_test.sink_pid, GF_PROP_PID_SRD_MAP);
	Bool relative = av1merge_test.mode == AV1MERGE_RELATIVE_ORIGIN || av1merge_test.mode == AV1MERGE_RELATIVE_OFFSET
		|| av1merge_test.mode == AV1MERGE_RELATIVE_SRD || av1merge_test.mode == AV1MERGE_RELATIVE_MIN;
	Bool srd = av1merge_test.mode == AV1MERGE_SRD || av1merge_test.mode == AV1MERGE_RELATIVE_SRD
		|| av1merge_test.mode == AV1MERGE_IMPLICIT_SRD || av1merge_test.mode == AV1MERGE_CHANGED_SRD
		|| (av1merge_test.mode == AV1MERGE_REMOVED_SRD && !av1merge_test.received);
	if (srd || (!relative && !av1merge_test_implicit())) {
		assert_true(map != NULL);
		if (map) {
			assert_equal(map->value.uint_list.nb_items, 8 * av1merge_test.count, "%u");
			const GF_PropertyValue *ref = gf_filter_pid_get_property(av1merge_test.sink_pid, GF_PROP_PID_SRD_REF);
			assert_true(ref != NULL);
			if (ref) {
				assert_equal(ref->value.vec2i.x, (s32) out_width, "%d");
				assert_equal(ref->value.vec2i.y, (s32) out_height, "%d");
			}
			u32 x = 0, y = 0;
			for (i = 0; i < av1merge_test.count && map->value.uint_list.nb_items == 8 * av1merge_test.count; i++) {
				if (!(i % cols)) {
					x = 0;
					if (i) y += av1merge_test.heights[i-cols];
				}
				const u32 *rect = &map->value.uint_list.vals[8*i];
				u32 ox = (av1merge_test.mode == AV1MERGE_CHANGED_ORIGIN || av1merge_test.mode == AV1MERGE_CHANGED_GAP) && av1merge_test.received ? x : av1merge_test.pos_x[i];
				u32 oy = av1merge_test.mode == AV1MERGE_CHANGED_ORIGIN && av1merge_test.received ? y : av1merge_test.pos_y[i];
				const u32 source[] = {
					srd ? 64 * i * out_width / 512 : ox,
					srd ? (av1merge_test.mode == AV1MERGE_CHANGED_SRD && av1merge_test.received ? 64 : 32) * out_height / 256 : oy,
					srd ? 64 * out_width / 512 : av1merge_test.widths[i],
					srd ? 32 * out_height / 256 : av1merge_test.heights[i]
				};
				for (j = 0; j < 4; j++) assert_equal(rect[j], source[j], "%u");
				u32 dx = sparse ? av1merge_test.pos_x[i] : x;
				u32 dy = sparse ? av1merge_test.pos_y[i] : y;
				assert_equal(rect[4], dx, "%u");
				assert_equal(rect[5], dy, "%u");
				assert_equal(rect[6], av1merge_test.widths[i], "%u");
				assert_equal(rect[7], av1merge_test.heights[i], "%u");
				x += av1merge_test.widths[i];
			}
		}
	} else {
		assert_true(!map);
		assert_true(!gf_filter_pid_get_property(av1merge_test.sink_pid, GF_PROP_PID_SRD_REF));
	}
	assert_true(!gf_filter_pid_get_property(av1merge_test.sink_pid, GF_PROP_PID_SRD));
	data = gf_filter_pck_get_data(pck, &size);
	bs = gf_bs_new(data, size, GF_BITSTREAM_READ);
	GF_SAFEALLOC(state, AV1State);
	state->config = gf_odf_av1_cfg_new();
	AV1ParseOffsets offsets = {0};
	state->parse_offsets = &offsets;
	while (gf_bs_available(bs)) {
		ObuType type;
		u64 obu_size = 0, pos = gf_bs_get_position(bs);
		u32 hdr_size = 0;
		GF_Err e = gf_av1_parse_obu(bs, &type, &obu_size, &hdr_size, state);
		assert_equal(e, GF_OK, "%d");
		if (e) break;
		if (type != OBU_FRAME && type != OBU_FRAME_HEADER && type != OBU_TILE_GROUP) continue;
		assert_equal(state->width, out_width, "%u");
		assert_equal(state->height, out_height, "%u");
		assert_equal(state->tileCols, cols, "%u");
		assert_equal(state->tileRows, rows, "%u");
		if (type == OBU_FRAME || type == OBU_FRAME_HEADER) {
			headers++;
			if (sparse) assert_equal(type, OBU_FRAME_HEADER, "%d");
			u64 end = gf_bs_get_position(bs);
			gf_bs_seek(bs, offsets.render_size / 8);
			if (offsets.render_size % 8) gf_bs_read_int(bs, offsets.render_size % 8);
			assert_equal(gf_bs_read_int(bs, 1), (av1merge_test.render ? 1U : 0U), "%u");
			if (av1merge_test.render) {
				assert_equal(gf_bs_read_int(bs, 16) + 1, out_width * 2, "%u");
				assert_equal(gf_bs_read_int(bs, 16) + 1, out_height * 2, "%u");
			}
			if (cols > 1 || rows > 1) {
				u32 bits = state->tileColsLog2 + state->tileRowsLog2;
				u64 context = offsets.tile_info_end - bits - 2;
				gf_bs_seek(bs, context / 8);
				if (context % 8) gf_bs_read_int(bs, context % 8);
				assert_equal(gf_bs_read_int(bs, bits), av1merge_test.tile_index[0], "%u");
			}
			gf_bs_seek(bs, end);
		}
		u32 start = 0;
		for (i = 0; i < cols; i++) {
			assert_equal(offsets.col_start[i], start, "%u");
			u32 width = av1merge_test.grid_widths[i];
			if ((av1merge_test.mode == AV1MERGE_CHANGED_ORIGIN || av1merge_test.mode == AV1MERGE_CHANGED_GAP) && av1merge_test.received)
				width = 128;
			start += (width + 63) / 64;
		}
		assert_equal(offsets.col_start[i], start, "%u");
		start = 0;
		for (i = 0; i < rows; i++) {
			assert_equal(offsets.row_start[i], start, "%u");
			u32 height = av1merge_test.grid_heights[i];
			if (av1merge_test.mode == AV1MERGE_CHANGED_ORIGIN && av1merge_test.received) height = 64;
			start += (height + 63) / 64;
		}
		assert_equal(offsets.row_start[i], start, "%u");
		if (type == OBU_FRAME_HEADER) continue;
		groups++;
		u32 expected = sparse ? 1 : av1merge_test.count;
		while (sparse && payloads+expected < av1merge_test.count
			&& av1merge_test.tile_index[payloads+expected] == av1merge_test.tile_index[payloads]+expected)
			expected++;
		assert_equal(state->frame_state.nb_tiles_in_obu, expected, "%u");
		if (sparse) {
			u64 end = gf_bs_get_position(bs);
			u32 bits = state->tileColsLog2 + state->tileRowsLog2;
			gf_bs_seek(bs, pos + hdr_size);
			assert_equal(gf_bs_read_int(bs, 1), 1U, "%u");
			assert_equal(gf_bs_read_int(bs, bits), av1merge_test.tile_index[payloads], "%u");
			assert_equal(gf_bs_read_int(bs, bits), av1merge_test.tile_index[payloads+expected-1], "%u");
			gf_bs_seek(bs, end);
		}
		for (i = 0; i < state->frame_state.nb_tiles_in_obu && payloads+i < av1merge_test.count; i++) {
			AV1Tile *tile = &state->frame_state.tiles[i];
			assert_equal(tile->size, av1merge_test.sizes[payloads+i], "%u");
			assert_true(pos + tile->obu_start_offset + tile->size <= size);
			if (pos + tile->obu_start_offset + tile->size > size) break;
			for (j = 0; j < tile->size; j++) {
				if (data[pos + tile->obu_start_offset + j] != (u8)(payloads+i+1+j)) break;
			}
			assert_equal(j, tile->size, "%u");
		}
		payloads += state->frame_state.nb_tiles_in_obu;
	}
	assert_equal(headers, 1, "%u");
	assert_equal(payloads, av1merge_test.count, "%u");
	u32 expected_groups = 1;
	if (sparse) {
		for (i = 1; i < av1merge_test.count; i++)
			if (av1merge_test.tile_index[i] != av1merge_test.tile_index[i-1]+1) expected_groups++;
	}
	assert_equal(groups, expected_groups, "%u");
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

static void av1merge_test_geometry(const char *args, u32 cols, u32 rows, u32 mode, GF_Err expected_error, const u32 *widths, const u32 *heights)
{
	GF_Err e;
	GF_Filter *source, *merge, *sink;
	GF_FilterSession *session = gf_fs_new(0, GF_FS_SCHEDULER_LOCK_FREE, 0, NULL);
	memset(&av1merge_test, 0, sizeof(av1merge_test));
	av1merge_test.cols = cols;
	av1merge_test.rows = rows;
	av1merge_test.count = cols * rows;
	av1merge_test.mode = mode;
	av1merge_test.render = mode == 8 || mode == AV1MERGE_SPARSE_RENDER || mode == AV1MERGE_GAP_RENDER;
	for (u32 i = 0; i < av1merge_test.count; i++) {
		av1merge_test.widths[i] = widths ? widths[i] : 128;
		av1merge_test.heights[i] = heights ? heights[i] : 64;
		av1merge_test.tile_index[i] = i;
		if (i < cols) av1merge_test.grid_widths[i] = av1merge_test.widths[i];
		if (!(i % cols)) av1merge_test.grid_heights[i/cols] = av1merge_test.heights[i];
		if (i < cols) av1merge_test.out_width += av1merge_test.widths[i];
		if (!(i % cols)) av1merge_test.out_height += av1merge_test.heights[i];
	}
	for (u32 i = 0; i < av1merge_test.count; i++) {
		u32 col = i % cols, row = i / cols;
		for (u32 j = 0; j < col; j++) av1merge_test.pos_x[i] += av1merge_test.widths[j];
		for (u32 j = 0; j < row; j++) av1merge_test.pos_y[i] += av1merge_test.heights[j * cols];
		if ((mode == AV1MERGE_HORIZONTAL_GAP || mode == AV1MERGE_SPARSE_GROUPS
			|| mode == AV1MERGE_GAP_RENDER || mode == AV1MERGE_CHANGED_GAP) && col)
			av1merge_test.pos_x[i] += 64;
		if (mode == AV1MERGE_HORIZONTAL_OVERLAP && col) av1merge_test.pos_x[i] -= 64;
		if (mode == AV1MERGE_VERTICAL_GAP && row) av1merge_test.pos_y[i] += 64;
		if (mode == AV1MERGE_OFFSET_ORIGIN) {
			av1merge_test.pos_x[i] += 640;
			av1merge_test.pos_y[i] += 384;
		}
		if (mode == AV1MERGE_UNALIGNED_ORIGIN) av1merge_test.pos_x[i]++;
		if (mode == AV1MERGE_RELATIVE_ORIGIN || mode == AV1MERGE_RELATIVE_OFFSET
			|| mode == AV1MERGE_RELATIVE_GAP || mode == AV1MERGE_RELATIVE_SRD) {
			av1merge_test.pos_x[i] = -(s32) col;
			av1merge_test.pos_y[i] = -(s32) row;
			if (mode == AV1MERGE_RELATIVE_OFFSET) {
				av1merge_test.pos_x[i] -= 7;
				av1merge_test.pos_y[i] -= 9;
			}
			if (mode == AV1MERGE_RELATIVE_GAP && col) av1merge_test.pos_x[i]--;
		}
	}
	if (mode == AV1MERGE_NEGATIVE_ORIGIN) av1merge_test.pos_y[0] = -1;
	if (mode == AV1MERGE_RELATIVE_MIN) av1merge_test.pos_x[0] = av1merge_test.pos_y[0] = -2147483647-1;
	if (mode == AV1MERGE_DUPLICATE_ORIGIN) av1merge_test.pos_x[1] = av1merge_test.pos_y[1] = 0;
	if (mode == AV1MERGE_SHIFTED_COLUMN) av1merge_test.pos_x[cols]++;
	if (mode == AV1MERGE_MISSING_CELL) {
		av1merge_test.pos_x[2] = 0;
		av1merge_test.pos_y[2] = 64;
	}
	if (mode == AV1MERGE_HORIZONTAL_GAP || mode == AV1MERGE_SPARSE_GROUPS
		|| mode == AV1MERGE_GAP_RENDER || mode == AV1MERGE_CHANGED_GAP) {
		av1merge_test.sparse = GF_TRUE;
		av1merge_test.cols = 3;
		av1merge_test.grid_widths[1] = 64;
		av1merge_test.grid_widths[2] = av1merge_test.widths[1];
		av1merge_test.tile_index[1] = 2;
		av1merge_test.out_width += 64;
	} else if (mode == AV1MERGE_VERTICAL_GAP) {
		av1merge_test.sparse = GF_TRUE;
		av1merge_test.rows = 3;
		av1merge_test.grid_heights[1] = 64;
		av1merge_test.grid_heights[2] = av1merge_test.heights[1];
		av1merge_test.tile_index[1] = 2;
		av1merge_test.out_height += 64;
	} else if (mode == AV1MERGE_MISSING_CELL || mode == AV1MERGE_MISSING_FIRST) {
		av1merge_test.sparse = GF_TRUE;
		av1merge_test.cols = av1merge_test.rows = 2;
		av1merge_test.out_width = 256;
		av1merge_test.out_height = 128;
		av1merge_test.grid_heights[1] = 64;
		if (mode == AV1MERGE_MISSING_FIRST) {
			for (u32 i = 0; i < 3; i++) {
				av1merge_test.tile_index[i] = i+1;
				av1merge_test.pos_x[i] = ((i+1) % 2) * 128;
				av1merge_test.pos_y[i] = ((i+1) / 2) * 64;
			}
		}
	} else if (mode == AV1MERGE_MISSING_INTERIOR || mode == AV1MERGE_SPARSE_RENDER) {
		av1merge_test.sparse = GF_TRUE;
		av1merge_test.cols = 3;
		av1merge_test.rows = 2;
		av1merge_test.out_width = 384;
		av1merge_test.out_height = 128;
		av1merge_test.grid_heights[1] = 64;
		for (u32 i = 0; i < 5; i++) {
			av1merge_test.tile_index[i] = i == 4 ? 5 : i;
			av1merge_test.pos_x[i] = (av1merge_test.tile_index[i] % 3) * 128;
			av1merge_test.pos_y[i] = (av1merge_test.tile_index[i] / 3) * 64;
		}
	}
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
	assert_equal(av1merge_test.received, (((mode >= 4 && mode <= 7) || mode == AV1MERGE_REMOVED_ORIGIN
		|| mode == AV1MERGE_QP_LATE_MISMATCH) ? 1 : (expected_error ? 0 : ((mode == 1 || mode == 2) ? 1 : 2))), "%u");
	gf_fs_del(session);
}

static void av1merge_test_session(const char *args, u32 cols, u32 rows, u32 mode, GF_Err expected_error)
{
	av1merge_test_geometry(args, cols, rows, mode, expected_error, NULL, NULL);
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

unittest(av1merge_variable_sizes)
{
#if !defined(GPAC_DISABLE_AV_PARSERS) && !defined(GPAC_DISABLE_AV1MERGE)
	const u32 widths[] = {128, 96, 128, 96};
	const u32 heights[] = {64, 64, 48, 48};
	const u32 unequal_columns[] = {64, 128, 96, 64, 128, 96};
	const u32 unequal_rows[] = {64, 64, 64, 48, 48, 48};
	const u32 bad_column[] = {128, 96, 64, 96};
	const u32 bad_row[] = {64, 64, 48, 32};
	const u32 unaligned_column[] = {96, 128, 96, 128};
	const u32 unaligned_row[] = {48, 48, 64, 64};
	av1merge_test_geometry("av1merge:cols=2", 2, 1, 0, GF_OK, widths, NULL);
	av1merge_test_geometry("av1merge:cols=1", 1, 2, 0, GF_OK, NULL, (const u32[]){64, 48});
	av1merge_test_geometry("av1merge:cols=2", 2, 2, 0, GF_OK, widths, heights);
	av1merge_test_geometry("av1merge:cols=2", 2, 2, 3, GF_OK, widths, heights);
	av1merge_test_geometry("av1merge:cols=2", 2, 2, 8, GF_OK, widths, heights);
	av1merge_test_geometry("av1merge:cols=3", 3, 2, 0, GF_OK, unequal_columns, unequal_rows);
	av1merge_test_geometry("av1merge:cols=2", 2, 2, 0, GF_BAD_PARAM, bad_column, heights);
	av1merge_test_geometry("av1merge:cols=2", 2, 2, 0, GF_BAD_PARAM, widths, bad_row);
	av1merge_test_geometry("av1merge:cols=2", 2, 2, 0, GF_NOT_SUPPORTED, unaligned_column, heights);
	av1merge_test_geometry("av1merge:cols=2", 2, 2, 0, GF_NOT_SUPPORTED, widths, unaligned_row);
	av1merge_test_geometry("av1merge:cols=2", 2, 2, 9, GF_BAD_PARAM, widths, heights);
#endif
}

unittest(av1merge_qp)
{
#if !defined(GPAC_DISABLE_AV_PARSERS) && !defined(GPAC_DISABLE_AV1MERGE)
	av1merge_test_session("av1merge", 2, 1, AV1MERGE_QP_MISMATCH, GF_NON_COMPLIANT_BITSTREAM);
	av1merge_test_session("av1merge", 2, 1, AV1MERGE_QP_GROUPS, GF_NON_COMPLIANT_BITSTREAM);
	av1merge_test_session("av1merge", 2, 1, AV1MERGE_QP_LATE_MISMATCH, GF_NON_COMPLIANT_BITSTREAM);
	av1merge_test_session("av1merge", 2, 1, AV1MERGE_QP_MATCHED_CHANGE, GF_OK);
#endif
}

unittest(av1merge_track_matrix)
{
#if !defined(GPAC_DISABLE_AV_PARSERS) && !defined(GPAC_DISABLE_AV1MERGE)
	av1merge_test_session("av1merge", 2, 1, AV1MERGE_TRACK_MATRIX, GF_OK);
#endif
}

unittest(av1merge_crop_origin)
{
#if !defined(GPAC_DISABLE_AV_PARSERS) && !defined(GPAC_DISABLE_AV1MERGE)
	av1merge_test_session("av1merge", 2, 2, AV1MERGE_OFFSET_ORIGIN, GF_OK);
	av1merge_test_session("av1merge", 2, 1, AV1MERGE_IGNORED_TILE_ID, GF_OK);
	av1merge_test_session("av1merge", 2, 1, AV1MERGE_CHANGED_ORIGIN, GF_OK);
	av1merge_test_session("av1merge", 2, 1, AV1MERGE_REMOVED_ORIGIN, GF_BAD_PARAM);
	av1merge_test_session("av1merge", 2, 1, AV1MERGE_MISSING_ORIGIN, GF_BAD_PARAM);
	av1merge_test_session("av1merge", 2, 1, AV1MERGE_NEGATIVE_ORIGIN, GF_BAD_PARAM);
	av1merge_test_session("av1merge", 2, 1, AV1MERGE_DUPLICATE_ORIGIN, GF_BAD_PARAM);
	av1merge_test_session("av1merge", 2, 1, AV1MERGE_HORIZONTAL_OVERLAP, GF_BAD_PARAM);
	av1merge_test_session("av1merge", 2, 2, AV1MERGE_SHIFTED_COLUMN, GF_BAD_PARAM);
	av1merge_test_session("av1merge", 2, 1, AV1MERGE_UNALIGNED_ORIGIN, GF_NOT_SUPPORTED);
	av1merge_test_session("av1merge:cols=2", 1, 2, 0, GF_BAD_PARAM);
#endif
}

unittest(av1merge_sparse_grid)
{
#if !defined(GPAC_DISABLE_AV_PARSERS) && !defined(GPAC_DISABLE_AV1MERGE)
	av1merge_test_session("av1merge", 2, 1, AV1MERGE_HORIZONTAL_GAP, GF_OK);
	av1merge_test_session("av1merge", 1, 2, AV1MERGE_VERTICAL_GAP, GF_OK);
	av1merge_test_session("av1merge", 3, 1, AV1MERGE_MISSING_CELL, GF_OK);
	av1merge_test_session("av1merge", 3, 1, AV1MERGE_MISSING_FIRST, GF_OK);
	av1merge_test_session("av1merge", 5, 1, AV1MERGE_MISSING_INTERIOR, GF_OK);
	av1merge_test_session("av1merge", 2, 1, AV1MERGE_SPARSE_GROUPS, GF_OK);
	av1merge_test_session("av1merge", 5, 1, AV1MERGE_SPARSE_RENDER, GF_OK);
	av1merge_test_session("av1merge", 2, 1, AV1MERGE_GAP_RENDER, GF_NOT_SUPPORTED);
	av1merge_test_session("av1merge", 2, 1, AV1MERGE_CHANGED_GAP, GF_OK);
	av1merge_test_geometry("av1merge:cols=3", 2, 1, AV1MERGE_HORIZONTAL_GAP, GF_OK,
		(const u32[]){128, 96}, (const u32[]){48, 48});
	av1merge_test_session("av1merge:cols=2", 2, 1, AV1MERGE_HORIZONTAL_GAP, GF_BAD_PARAM);
#endif
}

unittest(av1merge_positioning)
{
#if !defined(GPAC_DISABLE_AV_PARSERS) && !defined(GPAC_DISABLE_AV1MERGE)
	const u32 widths[] = {128, 96, 128, 96};
	const u32 heights[] = {64, 64, 48, 48};
	av1merge_test_geometry("av1merge", 2, 2, AV1MERGE_RELATIVE_ORIGIN, GF_OK, widths, heights);
	av1merge_test_geometry("av1merge", 2, 2, AV1MERGE_RELATIVE_OFFSET, GF_OK, widths, heights);
	av1merge_test_session("av1merge", 2, 1, AV1MERGE_RELATIVE_GAP, GF_BAD_PARAM);
	av1merge_test_session("av1merge", 1, 1, AV1MERGE_IMPLICIT, GF_OK);
	av1merge_test_session("av1merge", 2, 2, AV1MERGE_IMPLICIT, GF_OK);
	av1merge_test_session("av1merge", 2, 3, AV1MERGE_IMPLICIT, GF_OK);
	av1merge_test_session("av1merge:cols=3", 3, 2, AV1MERGE_IMPLICIT, GF_OK);
	av1merge_test_geometry("av1merge", 2, 2, AV1MERGE_IMPLICIT, GF_OK, widths, heights);
	av1merge_test_geometry("av1merge:cols=1", 1, 2, AV1MERGE_IMPLICIT, GF_OK, NULL, (const u32[]){64, 48});
	av1merge_test_session("av1merge:cols=3", 2, 2, AV1MERGE_IMPLICIT, GF_BAD_PARAM);
	av1merge_test_geometry("av1merge", 2, 1, AV1MERGE_IMPLICIT, GF_BAD_PARAM, (const u32[]){64, 128}, (const u32[]){64, 48});
	av1merge_test_session("av1merge", 2, 1, AV1MERGE_SRD, GF_OK);
	av1merge_test_session("av1merge", 2, 2, AV1MERGE_RELATIVE_SRD, GF_OK);
	av1merge_test_session("av1merge", 2, 2, AV1MERGE_IMPLICIT_SRD, GF_OK);
	av1merge_test_session("av1merge", 2, 1, AV1MERGE_CHANGED_SRD, GF_OK);
	av1merge_test_session("av1merge", 2, 2, AV1MERGE_PARTIAL_SRD, GF_OK);
	av1merge_test_session("av1merge", 2, 2, AV1MERGE_REMOVED_SRD, GF_OK);
	av1merge_test_session("av1merge", 2, 1, AV1MERGE_INVALID_SRD, GF_BAD_PARAM);
	av1merge_test_session("av1merge", 2, 1, AV1MERGE_INVALID_SRD_REF, GF_BAD_PARAM);
	av1merge_test_session("av1merge", 2, 1, AV1MERGE_OVERFLOW_SRD, GF_BAD_PARAM);
	av1merge_test_session("av1merge", 1, 1, AV1MERGE_RELATIVE_MIN, GF_OK);
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

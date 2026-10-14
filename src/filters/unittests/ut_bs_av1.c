#include "tests.h"
#include <gpac/filters.h>
#include <gpac/internal/media_dev.h>

#if !defined(GPAC_DISABLE_AV_PARSERS) && !defined(GPAC_DISABLE_BSSPLIT) && !defined(GPAC_DISABLE_BSAGG)

#include "../bs_av1.h"

//frame payloads with ayer 3 before layers 1/2
//we also use temporal_id=7 (upper value)
//every frame starts with a temporal delimiter
static const u8 av1_layers_tu0[] = {
	0x12, 0, 0x2a, 5, 1, 0, 0, 0, 0,
	0x36, 0x00, 3, 0, 0, 0, 0x36, 0x18, 3, 0, 3, 0
};
static const u8 av1_layers_tu1[] = {
	0x12, 0, 0x36, 0x20, 3, 1, 0, 1, 0x36, 0x38, 3, 1, 3, 1
};
static const u8 av1_layers_tu2[] = {
	0x12, 0, 0x36, 0x00, 3, 2, 0, 0
};
static const u8 av1_layers_tu3[] = {
	0x12, 0, 0x36, 0xe0, 3, 3, 0, 7, 0x36, 0xe8, 3, 3, 1, 7,
	0x36, 0xf0, 3, 3, 2, 7, 0x36, 0xf8, 3, 3, 3, 7
};
static const u8 *av1_layers_tus[] =   {        av1_layers_tu0 ,        av1_layers_tu1 ,        av1_layers_tu2 ,        av1_layers_tu3  };
static const u32 av1_layers_sizes[] = { sizeof(av1_layers_tu0), sizeof(av1_layers_tu1), sizeof(av1_layers_tu2), sizeof(av1_layers_tu3) };
static const u8 av1_layers_config[] = {                  0x81 ,                      0 ,                 0x0c ,                     0  };

static struct {
	GF_FilterPid *source;
	u32 phase, received, split_pids, mode, invalid;
	u32 frames[4][4];
	Bool track_refs, sparse;
} av1_layers_test;

static GF_Err av1_layers_source_init(GF_Filter *filter)
{
	GF_FilterPid *pid = av1_layers_test.source = gf_filter_pid_new(filter);
	gf_filter_pid_set_property(pid, GF_PROP_PID_STREAM_TYPE, &PROP_UINT(GF_STREAM_VISUAL));
	gf_filter_pid_set_property(pid, GF_PROP_PID_CODECID, &PROP_UINT(GF_CODECID_AV1));
	gf_filter_pid_set_property(pid, GF_PROP_PID_ID, &PROP_UINT(1));
	gf_filter_pid_set_property(pid, GF_PROP_PID_TIMESCALE, &PROP_UINT(1000));
	gf_filter_pid_set_property(pid, GF_PROP_PID_DECODER_CONFIG, &PROP_DATA((u8 *) av1_layers_config, sizeof(av1_layers_config)));
	gf_filter_post_process_task(filter);
	return GF_OK;
}

static GF_Err av1_layers_source_process(GF_Filter *filter)
{
	u32 phase = av1_layers_test.phase;
	u8 *dst;
	GF_FilterPacket *pck;
	if ((phase == 4) || (av1_layers_test.invalid && phase)) {
		gf_filter_pid_set_eos(av1_layers_test.source);
		return GF_EOS;
	}
	if (phase == 2) {
		u8 config[4];
		memcpy(config, av1_layers_config, sizeof(config));
		config[1] = 1;
		gf_filter_pid_set_property(av1_layers_test.source, GF_PROP_PID_DECODER_CONFIG, &PROP_DATA(config, sizeof(config)));
	}
	pck = gf_filter_pck_new_alloc(av1_layers_test.source, av1_layers_sizes[phase], &dst);
	memcpy(dst, av1_layers_tus[phase], av1_layers_sizes[phase]);
	if (av1_layers_test.invalid) dst[1] = 127;
	gf_filter_pck_set_dts(pck, 100+10*phase);
	gf_filter_pck_set_cts(pck, 103+10*phase);
	gf_filter_pck_set_duration(pck, 10);
	gf_filter_pck_set_sap(pck, phase ? GF_FILTER_SAP_NONE : GF_FILTER_SAP_1);
	gf_filter_pck_set_framing(pck, GF_TRUE, GF_TRUE);
	gf_filter_pck_send(pck);
	av1_layers_test.phase++;
	gf_filter_post_process_task(filter);
	return GF_OK;
}

//inspect the actual split PIDs and then forward to the aggregator
//removing DependencyID emulates MP4 demuxing where isom:scal carries the dependency
static GF_Err av1_layers_tap_configure(GF_Filter *filter, GF_FilterPid *pid, Bool remove)
{
	GF_FilterPid *opid = gf_filter_pid_get_udta(pid);
	const GF_PropertyValue *cfg;
	if (remove) return GF_OK;
	if (!opid) {
		opid = gf_filter_pid_new(filter);
		gf_filter_pid_set_udta(pid, opid);
		av1_layers_test.split_pids++;
	}
	cfg = gf_filter_pid_get_property(pid, GF_PROP_PID_DECODER_CONFIG);
	assert_not_null(cfg);
	if (cfg) {
		assert_equal(cfg->value.data.size, (u32) sizeof(av1_layers_config), "%u");
		if (cfg->value.data.size == sizeof(av1_layers_config)) {
			assert_equal(cfg->value.data.ptr[0], av1_layers_config[0], "%u");
			assert_true(cfg->value.data.ptr[1] <= 1);
		}
	}
	gf_filter_pid_copy_properties(opid, pid);
	if (av1_layers_test.sparse && gf_filter_pid_get_property(pid, GF_PROP_PID_DEPENDENCY_ID))
		gf_filter_pid_set_property(opid, GF_PROP_PID_TIMESCALE, &PROP_UINT(2000));
	if (av1_layers_test.track_refs)
		gf_filter_pid_set_property(opid, GF_PROP_PID_DEPENDENCY_ID, NULL);
	return GF_OK;
}

static GF_Err av1_layers_tap_process(GF_Filter *filter)
{
	u32 i;
	for (i=0; i<gf_filter_get_ipid_count(filter); i++) {
		GF_FilterPid *pid = gf_filter_get_ipid(filter, i);
		GF_FilterPid *opid = gf_filter_pid_get_udta(pid);
		GF_FilterPacket *pck = gf_filter_pid_get_packet(pid);
		if (!pck) {
			if (gf_filter_pid_is_eos(pid)) gf_filter_pid_set_eos(opid);
			continue;
		}
		u32 size, pos = 0;
		const u8 *data = gf_filter_pck_get_data(pck, &size);
		const GF_PropertyValue *lid = gf_filter_pid_get_property_str(pid, "max_layer_id");
		const GF_PropertyValue *tid = gf_filter_pid_get_property_str(pid, "max_temporal_id");
		assert_not_null(lid);
		assert_not_null(tid);
		while (pos < size) {
			BSAV1OBU obu;
			GF_Err e = bs_av1_obu_inspect(data+pos, size-pos, &obu);
			assert_equal(e, GF_OK, "%d");
			if (e) break;
			if (obu.type == OBU_FRAME) {
				u32 phase = data[pos+3];
				assert_less(phase, 4u, "%u");
				if (phase < 4) av1_layers_test.frames[obu.spatial_id][phase]++;
				assert_equal(gf_filter_pck_get_dts(pck), (u64) (100+10*phase), LLU);
				assert_equal(gf_filter_pck_get_cts(pck), (u64) (103+10*phase), LLU);
				if (lid && av1_layers_test.mode != 2) assert_equal(lid->value.uint, obu.spatial_id, "%u");
				if (tid && av1_layers_test.mode == 1) assert_equal(tid->value.uint, obu.temporal_id+1, "%u");
			}
			pos += obu.size;
		}
		if (av1_layers_test.sparse) {
			//test absent heads, future packets and early EOS without the
			//splitter's gap markers, all with a different enhancement timescale
			if (size != 2) {
				u8 *dst;
				u32 scale = gf_filter_pid_get_timescale(opid) / 1000;
				GF_FilterPacket *copy = gf_filter_pck_new_alloc(opid, size, &dst);
				memcpy(dst, data, size);
				gf_filter_pck_merge_properties(pck, copy);
				gf_filter_pck_set_dts(copy, gf_filter_pck_get_dts(pck) * scale);
				gf_filter_pck_set_cts(copy, gf_filter_pck_get_cts(pck) * scale);
				gf_filter_pck_set_duration(copy, gf_filter_pck_get_duration(pck) * scale);
				gf_filter_pck_send(copy);
			}
		} else {
			gf_filter_pck_forward(pck, opid);
		}
		gf_filter_pid_drop_packet(pid);
	}
	return GF_OK;
}

static GF_Err av1_layers_sink_configure(GF_Filter *filter, GF_FilterPid *pid, Bool remove)
{
	GF_FilterEvent evt;
	if (remove) return GF_OK;
	assert_false(gf_filter_pid_get_property(pid, GF_PROP_PID_DEPENDENCY_ID));
	assert_false(gf_filter_pid_get_property_str(pid, "isom:scal"));
	GF_FEVT_INIT(evt, GF_FEVT_PLAY, pid);
	gf_filter_pid_send_event(pid, &evt);
	return GF_OK;
}

static GF_Err av1_layers_sink_process(GF_Filter *filter)
{
	u32 i;
	for (i=0; i<gf_filter_get_ipid_count(filter); i++) {
		GF_FilterPid *pid = gf_filter_get_ipid(filter, i);
		GF_FilterPacket *pck = gf_filter_pid_get_packet(pid);
		if (!pck) continue;
		u32 size, phase = av1_layers_test.received++;
		const u8 *data = gf_filter_pck_get_data(pck, &size);
		assert_less(phase, 4u, "%u");
		if (phase < 4) {
			const GF_PropertyValue *cfg = gf_filter_pid_get_property(pid, GF_PROP_PID_DECODER_CONFIG);
			assert_not_null(cfg);
			if (cfg) assert_equal(cfg->value.data.ptr[1], (phase < 2 ? 0 : 1), "%u");
			assert_equal(size, av1_layers_sizes[phase]-2, "%u");
			if (size == av1_layers_sizes[phase]-2)
				assert_equal_mem(data, av1_layers_tus[phase]+2, size);
			assert_equal(gf_filter_pck_get_dts(pck), (u64) (100+10*phase), LLU);
			assert_equal(gf_filter_pck_get_cts(pck), (u64) (103+10*phase), LLU);
			assert_equal(gf_filter_pck_get_duration(pck), 10u, "%u");
		}
		gf_filter_pid_drop_packet(pid);
	}
	return GF_OK;
}

static const GF_FilterCapability av1_layers_source_caps[] = {
	CAP_UINT(GF_CAPS_OUTPUT, GF_PROP_PID_STREAM_TYPE, GF_STREAM_VISUAL),
	CAP_UINT(GF_CAPS_OUTPUT, GF_PROP_PID_CODECID, GF_CODECID_AV1)
};
static const GF_FilterCapability av1_layers_sink_caps[] = {
	CAP_UINT(GF_CAPS_INPUT, GF_PROP_PID_STREAM_TYPE, GF_STREAM_VISUAL),
	CAP_UINT(GF_CAPS_INPUT, GF_PROP_PID_CODECID, GF_CODECID_AV1)
};
static const GF_FilterCapability av1_layers_tap_caps[] = {
	CAP_UINT(GF_CAPS_INPUT_OUTPUT, GF_PROP_PID_STREAM_TYPE, GF_STREAM_VISUAL),
	CAP_UINT(GF_CAPS_INPUT_OUTPUT, GF_PROP_PID_CODECID, GF_CODECID_AV1)
};
static const GF_FilterRegister av1_layers_source = {
	.name = "av1_layers_ut_source", SETCAPS(av1_layers_source_caps),
	.initialize = av1_layers_source_init, .process = av1_layers_source_process
};
static const GF_FilterRegister av1_layers_tap = {
	.name = "av1_layers_ut_tap", SETCAPS(av1_layers_tap_caps), .max_extra_pids = (u32)-1,
	.configure_pid = av1_layers_tap_configure, .process = av1_layers_tap_process
};
static const GF_FilterRegister av1_layers_sink = {
	.name = "av1_layers_ut_sink", SETCAPS(av1_layers_sink_caps), .max_extra_pids = (u32)-1,
	.configure_pid = av1_layers_sink_configure, .process = av1_layers_sink_process
};

static void av1_layers_session(u32 mode, Bool track_refs, u32 invalid, Bool sparse)
{
	GF_Err e;
	GF_Filter *source, *split, *tap, *agg, *sink;
	GF_FilterSession *session = gf_fs_new(0, GF_FS_SCHEDULER_LOCK_FREE, 0, NULL);
	memset(&av1_layers_test, 0, sizeof(av1_layers_test));
	av1_layers_test.mode = mode;
	av1_layers_test.track_refs = track_refs;
	av1_layers_test.invalid = invalid;
	av1_layers_test.sparse = sparse;
	gf_fs_add_filter_register(session, &av1_layers_source);
	gf_fs_add_filter_register(session, &av1_layers_tap);
	gf_fs_add_filter_register(session, &av1_layers_sink);
	source = gf_fs_load_filter(session, "av1_layers_ut_source", &e);
	assert_equal(e, GF_OK, "%d");
	agg = gf_fs_load_filter(session, "bsagg", &e);
	assert_equal(e, GF_OK, "%d");
	if (invalid != 2) {
		const char *args = mode == 1 ? "bssplit:sig_ltid:ltid=all" : mode == 2 ? "bssplit:sig_ltid:ltid=1" : "bssplit:sig_ltid";
		split = gf_fs_load_filter(session, args, &e);
		assert_equal(e, GF_OK, "%d");
		tap = gf_fs_load_filter(session, "av1_layers_ut_tap", &e);
		assert_equal(e, GF_OK, "%d");
		assert_equal(gf_filter_set_source(split, source, NULL), GF_OK, "%d");
		assert_equal(gf_filter_set_source(tap, split, NULL), GF_OK, "%d");
		assert_equal(gf_filter_set_source(agg, tap, NULL), GF_OK, "%d");
	} else {
		assert_equal(gf_filter_set_source(agg, source, NULL), GF_OK, "%d");
	}
	sink = gf_fs_load_filter(session, "av1_layers_ut_sink", &e);
	assert_equal(e, GF_OK, "%d");
	assert_equal(gf_filter_set_source(sink, agg, NULL), GF_OK, "%d");
	gf_fs_run(session);
	assert_equal(gf_fs_get_last_connect_error(session), GF_OK, "%d");
	assert_equal(gf_fs_get_last_process_error(session), (invalid ? GF_NON_COMPLIANT_BITSTREAM : GF_OK), "%d");
	assert_equal(av1_layers_test.received, (invalid ? 0u : 4u), "%u");
	if (!invalid) {
		u32 lid, phase;
		assert_equal(av1_layers_test.split_pids, (mode == 1 ? 8u : mode == 2 ? 2u : 4u), "%u");
		for (lid=0; lid<4; lid++) {
			for (phase=0; phase<4; phase++) {
				u32 expected = (!lid || phase == 3 || (lid == 3 && phase < 2)) ? 1 : 0;
				assert_equal(av1_layers_test.frames[lid][phase], expected, "%u");
			}
		}
	}
	gf_fs_del(session);
}
#endif

unittest(av1_layer_roundtrip)
{
#if !defined(GPAC_DISABLE_AV_PARSERS) && !defined(GPAC_DISABLE_BSSPLIT) && !defined(GPAC_DISABLE_BSAGG)
	av1_layers_session(0, GF_FALSE, 0, GF_FALSE);
	av1_layers_session(1, GF_FALSE, 0, GF_FALSE);
	av1_layers_session(2, GF_FALSE, 0, GF_FALSE);
	av1_layers_session(0, GF_TRUE,  0, GF_FALSE);
	av1_layers_session(1, GF_FALSE, 0, GF_TRUE);
#endif
}

unittest(av1_layer_invalid_packet)
{
#if !defined(GPAC_DISABLE_AV_PARSERS) && !defined(GPAC_DISABLE_BSSPLIT) && !defined(GPAC_DISABLE_BSAGG)
	av1_layers_session(0, GF_FALSE, 1, GF_FALSE);
	av1_layers_session(0, GF_FALSE, 2, GF_FALSE);
#endif
}

unittest(av1_layer_obu_framing)
{
#if !defined(GPAC_DISABLE_AV_PARSERS) && !defined(GPAC_DISABLE_BSSPLIT) && !defined(GPAC_DISABLE_BSAGG)
	BSAV1OBU obu;
	const u8 good[] = { 0x36, 0xe8, 0x81, 0, 0xaa };
	const u8 oversized[] = { 0x32, 127, 0 };
	const u8 no_size[] = { 0x30, 0 };
	const u8 unterminated[] = { 0x32, 128, 128, 128, 128, 128, 128, 128, 128 };
	u32 i;
	assert_equal(bs_av1_obu_inspect(good, sizeof(good), &obu), GF_OK, "%d");
	assert_equal(obu.spatial_id, 1u, "%u");
	assert_equal(obu.temporal_id, 7u, "%u");
	assert_equal(obu.size, (u32) sizeof(good), "%u");
	for (i=0; i<sizeof(good); i++)
		assert_equal(bs_av1_obu_inspect(good, i, &obu), GF_NON_COMPLIANT_BITSTREAM, "%d");
	assert_equal(bs_av1_obu_inspect(oversized, sizeof(oversized), &obu), GF_NON_COMPLIANT_BITSTREAM, "%d");
	assert_equal(bs_av1_obu_inspect(no_size, sizeof(no_size), &obu), GF_NON_COMPLIANT_BITSTREAM, "%d");
	assert_equal(bs_av1_obu_inspect(unterminated, sizeof(unterminated), &obu), GF_NON_COMPLIANT_BITSTREAM, "%d");
#endif
}

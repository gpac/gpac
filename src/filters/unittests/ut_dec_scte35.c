#include "tests.h"
#include "../dec_scte35.c"
#include "../../filter_core/filter_session.h" // GF_FilterPacket

static GF_FilterPacket* pck_new_alloc(GF_FilterPid *pid, u32 data_size, u8 **data)
{
	GF_FilterPacket *pck;
	GF_SAFEALLOC(pck, GF_FilterPacket);
	pck->data = gf_malloc(data_size);
	pck->pck = pck;
	pck->data_length = data_size;
	*data = pck->data;
	pck->filter_owns_mem = 0;
	return pck;
}

static GF_FilterPacket* pck_new_shared(GF_FilterPid *pid, const u8 *data, u32 data_size, gf_fsess_packet_destructor destruct)
{
	GF_FilterPacket *pck;
	GF_SAFEALLOC(pck, GF_FilterPacket);
	pck->pck = pck;
	pck->data = (char*)data;
	pck->data_length = data_size;
	pck->destructor = destruct;
	pck->filter_owns_mem = 1;
	return pck;
}

// ISOBMFF box sizes (used for identifying the eponym boxes)
#define EMIB_BOX_SIZE 105
#define EMEB_BOX_SIZE 8

static u8 scte35_payload[] = {
    0xfc, 0x00, 0x28, // m2ts section header
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xff,
    0xf0, 0x14, 0x05, 0x4f, 0xff, 0xff, 0xf2, 0x7f,
    0xef, 0xfe, 0x00, 0x00, 0xe8, 0xbf, 0xfe, 0x00,
    0x00, 0x8f, 0x1d, 0x00, 0x01, 0x00, 0x00, 0x00,
    0x0a, 0x00, 0x08, 0x43, 0x55, 0x45, 0x49, 0x00,
};

#define SCTE35_DTS 59583ULL
#define SCTE35_DUR 36637U
#define SCTE35_LAST_EVENT_ID 1342177266U

static u8 emeb_box[EMEB_BOX_SIZE] = {
    0x00, 0x00, 0x00, 0x08, 0x65, 0x6d, 0x65, 0x62
};

#define TIMESCALE 90000 // usual value
#define FPS 3           // allows to put event at the beginning, middle, and end

#define SEND_VIDEO(num_frames) { \
        for (int i=0; i<num_frames; ++i) { \
            scte35dec_process_timing(&ctx, dts, TIMESCALE, TIMESCALE/FPS); \
            scte35dec_process_dispatch(&ctx, dts, TIMESCALE/FPS); \
            dts += TIMESCALE/FPS; \
        } \
    }

#define SEND_EVENT() { \
        GF_PropertyValue emsg = { .type=GF_PROP_CONST_DATA, .value.data.ptr=scte35_payload, .value.data.size=sizeof(scte35_payload)}; \
        scte35dec_process_timing(&ctx, dts, TIMESCALE, SCTE35_DUR); \
        scte35dec_process_emsg(&ctx, emsg.value.data.ptr, emsg.value.data.size, dts); \
        if (ctx.mode != 1) scte35dec_process_dispatch(&ctx, dts, SCTE35_DUR); \
        dts = SCTE35_DTS + SCTE35_DUR; \
    }

#define UT_SCTE35_INIT(pck_send_fct) \
    SCTE35DecCtx ctx = {0}; \
    assert_equal(scte35dec_initialize_internal(&ctx), GF_OK, "%d"); \
    ctx.pck_new_shared = pck_new_shared;\
    ctx.pck_new_alloc = pck_new_alloc; \
    ctx.pck_send = pck_send_fct;

#define UT_SCTE35_PCK_SEND_FINALIZE() \
    if (!pck->filter_owns_mem) gf_free(pck->data); \
    gf_free(pck); \
    calls++; \
    return GF_OK;

/*************************************/

unittest(scte35dec_safety)
{
	const int remainder = TIMESCALE % FPS;
	assert_equal(remainder, 0, "%d");
}

/*************************************/

static GF_Err pck_send_no_event(GF_FilterPacket *pck)
{
	#define expected_calls 1
	static int calls = 0;
	static u64 expected_dts [expected_calls] = { 0 };
	static u32 expected_dur [expected_calls] = { TIMESCALE*2 };
	static u32 expected_size[expected_calls] = { EMEB_BOX_SIZE };

	if (pck == NULL) {
		// checks at termination
		assert_equal(calls, expected_calls, "%d");
		return GF_OK;
	}

	// dynamic checks
	assert_less(calls, expected_calls, "%d");
	assert_equal(gf_filter_pck_get_dts(pck), expected_dts[calls], LLU);
	assert_equal(gf_filter_pck_get_duration(pck), expected_dur[calls], "%u");

	u32 size = 0;
	const u8 *data = gf_filter_pck_get_data(pck, &size);
	assert_equal(size, expected_size[calls], "%u");
	assert_equal(size, EMEB_BOX_SIZE, "%u");
	assert_equal_mem(data, emeb_box, EMEB_BOX_SIZE);

	UT_SCTE35_PCK_SEND_FINALIZE();
	#undef expected_calls
}

unittest(scte35dec_no_event)
{
	UT_SCTE35_INIT(pck_send_no_event);
	u64 dts = 0;

	SEND_VIDEO(FPS*2); // send 2 seconds of heartbeat

	scte35dec_flush(&ctx);
	scte35dec_finalize_internal(&ctx);
	ctx.pck_send(NULL); // final checks
}

/*************************************/

static GF_Err pck_send_segmentation_no_event(GF_FilterPacket *pck)
{
	#define expected_calls 3
	static int calls = 0;
	static u64 expected_dts [expected_calls] = { 0, TIMESCALE, 2*TIMESCALE };
	static u32 expected_dur [expected_calls] = { TIMESCALE, TIMESCALE, TIMESCALE  };
	static u32 expected_size[expected_calls] = { EMEB_BOX_SIZE, EMEB_BOX_SIZE, EMEB_BOX_SIZE };

	if (pck == NULL) {
		// checks at termination
		assert_equal(calls, expected_calls, "%d");
		return GF_OK;
	}

	// dynamic checks
	assert_less(calls, expected_calls, "%d");
	assert_equal(gf_filter_pck_get_dts(pck), expected_dts[calls], LLU);
	assert_equal(gf_filter_pck_get_duration(pck), expected_dur[calls], "%u");

	u32 size = 0;
	const u8 *data = gf_filter_pck_get_data(pck, &size);
	assert_equal(size, expected_size[calls], "%u");
	assert_equal(size, EMEB_BOX_SIZE, "%u");
	assert_equal_mem(data, emeb_box, EMEB_BOX_SIZE);

	UT_SCTE35_PCK_SEND_FINALIZE();
	#undef expected_calls
}

unittest(scte35dec_segmentation_no_event)
{
	UT_SCTE35_INIT(pck_send_segmentation_no_event);
	ctx.sampdur = (GF_Fraction){1, 1};
	u64 dts = 0;

	SEND_VIDEO(FPS*3); // send 3 seconds of heartbeat

	scte35dec_flush(&ctx);
	scte35dec_finalize_internal(&ctx);
	ctx.pck_send(NULL); // final checks
}

/*************************************/

unittest(scte35dec_splice_point_with_idr)
{
	SCTE35DecCtx ctx = {0};
	assert_equal(scte35dec_initialize_internal(&ctx), GF_OK, "%d");
	ctx.mode = 1; // passthru
	u64 dts = 0;

	SEND_EVENT();
	assert_true(scte35dec_is_splice_point(&ctx, SCTE35_DTS));

	scte35dec_flush(&ctx);
	scte35dec_finalize_internal(&ctx);
}

/*************************************/

static GF_Err pck_send_simple(GF_FilterPacket *pck)
{
	#define expected_calls 3
	static int calls = 0;
	static u64 expected_dts [expected_calls] = {             0,    SCTE35_DTS, SCTE35_DTS+SCTE35_DUR };
	static u32 expected_dur [expected_calls] = {    SCTE35_DTS,    SCTE35_DUR, TIMESCALE };
	static u32 expected_size[expected_calls] = { EMEB_BOX_SIZE, EMIB_BOX_SIZE, EMEB_BOX_SIZE };
	static s64 expected_event_pts_delta[expected_calls] = { 0, 0, 0 };
	static u32 expected_event_duration [expected_calls] = { 0, SCTE35_DUR, 0 };
	static u32 expected_event_id       [expected_calls] = { 0, SCTE35_LAST_EVENT_ID, 0 };

	if (pck == NULL) {
		// checks at termination
		assert_equal(calls, expected_calls, "%d");
		return GF_OK;
	}

	// dynamic checks
	assert_less(calls, expected_calls, "%d");
	assert_equal(gf_filter_pck_get_dts(pck), expected_dts[calls], LLU);
	assert_equal(gf_filter_pck_get_duration(pck), expected_dur[calls], "%u");

	u32 size = 0;
	const u8 *data = gf_filter_pck_get_data(pck, &size);
	assert_equal(size, expected_size[calls], "%u");

	switch(size) {
		case EMEB_BOX_SIZE:
			assert_equal(EMEB_BOX_SIZE, size, "%u");
			assert_equal_mem(data, emeb_box, EMEB_BOX_SIZE);
			break;
		case EMIB_BOX_SIZE: {
			assert_less_equal((u32)sizeof(scte35_payload), size, "%u");
			assert_equal_mem(data + size - sizeof(scte35_payload), scte35_payload, sizeof(scte35_payload));

			GF_BitStream *bs = gf_bs_new(data, size, GF_BITSTREAM_READ);
			gf_bs_seek(bs, 16);
			assert_equal(gf_bs_read_u64(bs), expected_event_pts_delta[calls], LLU); //presentation_time_delta
			assert_equal(gf_bs_read_u32(bs), expected_event_duration[calls], "%u");  //event_duration
			assert_equal(gf_bs_read_u32(bs), expected_event_id[calls], "%u");        //event_id
			gf_bs_del(bs);
			break;
		}
		default: assert_true(0);
	}

	UT_SCTE35_PCK_SEND_FINALIZE();
	#undef expected_calls
}

unittest(scte35dec_simple)
{
	UT_SCTE35_INIT(pck_send_simple);
	u64 dts = 0;

	SEND_VIDEO(1);
	dts = SCTE35_DTS;
	SEND_EVENT();
	SEND_VIDEO(FPS);

	scte35dec_flush(&ctx);
	scte35dec_finalize_internal(&ctx);
	ctx.pck_send(NULL); // final checks
}

/*************************************/

static GF_Err pck_send_initial_delay(GF_FilterPacket *pck)
{
	#define expected_calls 5
	static int calls = 0;
	static u64 expected_dts [expected_calls] = { 0, TIMESCALE/FPS, TIMESCALE/FPS*3/2,
	                                             TIMESCALE/FPS*2, TIMESCALE/FPS*2+SCTE35_DUR-TIMESCALE/FPS/2 };
	static u32 expected_dur [expected_calls] = { TIMESCALE/FPS, TIMESCALE/FPS/2, TIMESCALE/FPS/2, SCTE35_DUR-TIMESCALE/FPS/2,
                                                 TIMESCALE/FPS*2-TIMESCALE/FPS/2-SCTE35_DUR };
	static u32 expected_size[expected_calls] = { EMEB_BOX_SIZE, EMEB_BOX_SIZE, EMIB_BOX_SIZE,
	                                             EMIB_BOX_SIZE, EMEB_BOX_SIZE };

	if (pck == NULL) {
		// checks at termination
		assert_equal(calls, expected_calls, "%d");
		return GF_OK;
	}

	// dynamic checks
	//printf("Debug: call %d, dts="LLU"/"LLU", dur=%u/%u\n", calls,
	//  gf_filter_pck_get_dts(pck), expected_dts[calls], gf_filter_pck_get_duration(pck), expected_dur[calls]);
	assert_less(calls, expected_calls, "%d");
	assert_equal(gf_filter_pck_get_dts(pck), expected_dts[calls], LLU);
	assert_equal(gf_filter_pck_get_duration(pck), expected_dur[calls], "%u");

	u32 size = 0;
	const u8 *data = gf_filter_pck_get_data(pck, &size);
	assert_equal(size, expected_size[calls], "%u");

	UT_SCTE35_PCK_SEND_FINALIZE();
	#undef expected_calls
}

unittest(scte35dec_initial_delay)
{
	UT_SCTE35_INIT(pck_send_initial_delay);
	ctx.sampdur = (GF_Fraction){1, FPS};
	u64 dts = 0;
	SEND_VIDEO(1);         // video (1 frame)
	dts=TIMESCALE/FPS*3/2; // introduce a delay before the first scte35 event
	SEND_EVENT();          // scte35 event scheduled at dts=59583 w/ dur=36637

	scte35dec_flush(&ctx);
	scte35dec_finalize_internal(&ctx);
	ctx.pck_send(NULL); // trigger final checks
}

/*************************************/

static GF_Err pck_send_segmentation_beginning(GF_FilterPacket *pck)
{
    #define expected_calls 3
	static int calls = 0;
	static u64 expected_dts [expected_calls] = {             0, SCTE35_DUR, TIMESCALE };
	static u32 expected_dur [expected_calls] = {    SCTE35_DUR, TIMESCALE-SCTE35_DUR, TIMESCALE };
	static u32 expected_size[expected_calls] = { EMIB_BOX_SIZE, EMEB_BOX_SIZE, EMEB_BOX_SIZE };
	static s64 expected_event_pts_delta[expected_calls] = { SCTE35_DTS, 0, 0 };
	static u32 expected_event_duration [expected_calls] = { SCTE35_DUR, 0, 0 };
	static u32 expected_event_id       [expected_calls] = { SCTE35_LAST_EVENT_ID, 0, 0 };

	if (pck == NULL) {
		// checks at termination
		assert_equal(calls, expected_calls, "%d");
		return GF_OK;
	}

	// dynamic checks
	assert_less(calls, expected_calls, "%d");
	assert_equal(gf_filter_pck_get_dts(pck), expected_dts[calls], LLU);
	assert_equal(gf_filter_pck_get_duration(pck), expected_dur[calls], "%u");

	u32 size = 0;
	const u8 *data = gf_filter_pck_get_data(pck, &size);
	assert_equal(size, expected_size[calls], "%u");

	switch(size) {
		case EMEB_BOX_SIZE:
			assert_equal(EMEB_BOX_SIZE, size, "%u");
			assert_equal_mem(data, emeb_box, EMEB_BOX_SIZE);
			break;
		case EMIB_BOX_SIZE: {
			assert_less_equal((u32)sizeof(scte35_payload), size, "%u");
			assert_equal_mem(data + size - sizeof(scte35_payload), scte35_payload, sizeof(scte35_payload));

			GF_BitStream *bs = gf_bs_new(data, size, GF_BITSTREAM_READ);
			gf_bs_seek(bs, 16);
			assert_equal(gf_bs_read_u64(bs), expected_event_pts_delta[calls], LLU); //presentation_time_delta
			assert_equal(gf_bs_read_u32(bs), expected_event_duration[calls], "%u");  //event_duration
			assert_equal(gf_bs_read_u32(bs), expected_event_id[calls], "%u");        //event_id
			gf_bs_del(bs);
			break;
		}
		default: assert_true(0);
	}

	UT_SCTE35_PCK_SEND_FINALIZE();
	#undef expected_calls
}

unittest(scte35dec_segmentation_beginning)
{
	UT_SCTE35_INIT(pck_send_segmentation_beginning);
	ctx.sampdur = (GF_Fraction){1, 1};
	u64 dts = 0;

	SEND_EVENT();  // scte35 event scheduled for dts=59583 with dur=36637
	SEND_VIDEO(1); // video (1 frame)

	scte35dec_flush(&ctx);
	scte35dec_finalize_internal(&ctx);
	ctx.pck_send(NULL); // trigger final checks
}

/*************************************/

static GF_Err pck_send_segmentation_end(GF_FilterPacket *pck)
{
	#define expected_calls 4
	static int calls = 0;
	//scte35 event at dts=30000 scheduled for dts=59583 with dur=36637, segment_dur=30000
	//emeb[0:30000] emib[30000:60000] emib[60000:66637] emeb[66637:90000]
	static u64 expected_dts [expected_calls] = {             0, TIMESCALE/FPS,          2*TIMESCALE/FPS, TIMESCALE/FPS+SCTE35_DUR };
	static u32 expected_dur [expected_calls] = { TIMESCALE/FPS, TIMESCALE/FPS, SCTE35_DUR-TIMESCALE/FPS, 2*TIMESCALE/FPS-SCTE35_DUR };
	static u32 expected_size[expected_calls] = { EMEB_BOX_SIZE, EMIB_BOX_SIZE, EMIB_BOX_SIZE, EMEB_BOX_SIZE };
	static s64 expected_event_pts_delta[expected_calls] = { 0, SCTE35_DTS-TIMESCALE/FPS, 0, 0 };
	static u32 expected_event_duration [expected_calls] = { 0, SCTE35_DUR, SCTE35_DUR-TIMESCALE/FPS, 0 };
	static u32 expected_event_id       [expected_calls] = { 0, SCTE35_LAST_EVENT_ID, SCTE35_LAST_EVENT_ID, 0 };

	if (pck == NULL) {
		// checks at termination
		assert_equal(calls, expected_calls, "%d");
		return GF_OK;
	}

	// dynamic checks
	//printf("Debug: call %d, dts="LLU"/"LLU", dur=%u/%u\n", calls,
	//  gf_filter_pck_get_dts(pck), expected_dts[calls], gf_filter_pck_get_duration(pck), expected_dur[calls]);
	assert_less(calls, expected_calls, "%d");
	assert_equal(gf_filter_pck_get_dts(pck), expected_dts[calls], LLU);
	assert_equal(gf_filter_pck_get_duration(pck), expected_dur[calls], "%u");

	u32 size = 0;
	const u8 *data = gf_filter_pck_get_data(pck, &size);
	assert_equal(size, expected_size[calls], "%u");

	switch(size) {
		case EMEB_BOX_SIZE:
			assert_equal(EMEB_BOX_SIZE, size, "%u");
			assert_equal_mem(data, emeb_box, EMEB_BOX_SIZE);
			break;
		case EMIB_BOX_SIZE: {
			assert_less_equal((u32)sizeof(scte35_payload), size, "%u");
			assert_equal_mem(data + size - sizeof(scte35_payload), scte35_payload, sizeof(scte35_payload));

			GF_BitStream *bs = gf_bs_new(data, size, GF_BITSTREAM_READ);
			gf_bs_seek(bs, 16);
			assert_equal(gf_bs_read_u64(bs), expected_event_pts_delta[calls], LLU); //presentation_time_delta
			assert_equal(gf_bs_read_u32(bs), expected_event_duration[calls], "%u");  //event_duration
			assert_equal(gf_bs_read_u32(bs), expected_event_id[calls], "%u");        //event_id
			gf_bs_del(bs);
			break;
		}
		default: assert_true(0);
	}

	UT_SCTE35_PCK_SEND_FINALIZE();
	#undef expected_calls
}

unittest(scte35dec_short_segmentation_end)
{
	UT_SCTE35_INIT(pck_send_segmentation_end);
	ctx.sampdur = (GF_Fraction){1, FPS};
	u64 dts = 0;

	SEND_VIDEO(1); // video (1 frame) 
	SEND_EVENT();  // scte35 event at "dts=1 frame" scheduled for dts=59583 with dur=36637

	scte35dec_flush(&ctx);
	scte35dec_flush(&ctx); // this test requires an additional segment
	scte35dec_finalize_internal(&ctx);
	ctx.pck_send(NULL); // trigger final checks
}

/*************************************/

/*
 * Direct scte35dec_get_timing tests for time_signal and 33-bit PTS wraparound.
 *
 * time_signal section layout (21 bytes):
 *   [0]     table_id = 0xFC
 *   [1-2]   section_syntax_indicator=0, private=0, sap=00, section_length=18
 *   [3]     protocol_version = 0
 *   [4-8]   encrypted=0, encryption_algo=0, pts_adjustment (33 bits)
 *   [9]     cw_index = 0
 *   [10-12] tier=0xFFF, splice_command_length=5
 *   [13]    splice_command_type = 0x06 (time_signal)
 *   [14-18] splice_time: time_specified=1, reserved=111111, pts_time (33 bits)
 *   [19-20] descriptor_loop_length = 0
 */

unittest(scte35dec_time_signal_basic)
{
	static u8 payload[] = {
		0xFC, 0x00, 0x12,
		0x00,
		0x00, 0x00, 0x00, 0x00, 0x00,
		0x00,
		0xFF, 0xF0, 0x05,
		0x06,
		0xFE, 0x00, 0x00, 0xE8, 0xBF,
		0x00, 0x00
	};

	u64 dts = 0, dur = 0;
	u32 splice_event_id = 0;
	Bool needs_idr = GF_FALSE;

	Bool ret = scte35dec_get_timing(payload, sizeof(payload), &dts, &dur, &splice_event_id, &needs_idr);
	assert_true(ret);
	assert_equal(dts, (u64)59583, LLU);
	assert_equal(dur, (u64)0, LLU);
	assert_equal(needs_idr, (Bool)GF_FALSE, "%d");
}

/*************************************/

unittest(scte35dec_time_signal_pts_wrap)
{
	/*pts_adjustment=5, splice_time=0x1FFFFFFFD (2^33 - 3)
	  sum = 0x200000002, masked to 33 bits = 2*/
	static u8 payload[] = {
		0xFC, 0x00, 0x12,
		0x00,
		0x00, 0x00, 0x00, 0x00, 0x05,
		0x00,
		0xFF, 0xF0, 0x05,
		0x06,
		0xFF, 0xFF, 0xFF, 0xFF, 0xFD,
		0x00, 0x00
	};

	u64 dts = 0, dur = 0;
	u32 splice_event_id = 0;
	Bool needs_idr = GF_FALSE;

	Bool ret = scte35dec_get_timing(payload, sizeof(payload), &dts, &dur, &splice_event_id, &needs_idr);
	assert_true(ret);
	assert_equal(dts, (u64)2, LLU);
	assert_equal(dur, (u64)0, LLU);
}

/*************************************/

unittest(scte35dec_splice_insert_pts_wrap)
{
	/*splice_insert with pts_adjustment=5, splice_time=0x1FFFFFFFD
	  expected PTS = (0x1FFFFFFFD + 5) & 0x1FFFFFFFF = 2
	  splice_event_id = 100, needs_idr = TRUE, no duration*/
	static u8 payload[] = {
		0xFC, 0x00, 0x1C,
		0x00,
		0x00, 0x00, 0x00, 0x00, 0x05,
		0x00,
		0xFF, 0xF0, 0x0F,
		0x05,
		0x00, 0x00, 0x00, 0x64,
		0x7F,
		0xCF,
		0xFF, 0xFF, 0xFF, 0xFF, 0xFD,
		0x00, 0x01,
		0x00,
		0x00,
		0x00, 0x00
	};

	u64 dts = 0, dur = 0;
	u32 splice_event_id = 0;
	Bool needs_idr = GF_FALSE;

	Bool ret = scte35dec_get_timing(payload, sizeof(payload), &dts, &dur, &splice_event_id, &needs_idr);
	assert_true(ret);
	assert_equal(dts, (u64)2, LLU);
	assert_equal(dur, (u64)0, LLU);
	assert_equal(splice_event_id, (u32)100, "%u");
	assert_equal(needs_idr, (Bool)GF_TRUE, "%d");
}

/*************************************/

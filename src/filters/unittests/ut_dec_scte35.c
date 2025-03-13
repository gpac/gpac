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

#define SCTE35_PTS 59583ULL
#define SCTE35_DUR 36637U
#define SCTE35_LAST_EVENT_ID 1342177266U

static u8 emeb_box[EMEB_BOX_SIZE] = {
    0x00, 0x00, 0x00, 0x08, 0x65, 0x6d, 0x65, 0x62
};

#define TIMESCALE 90000 // usual value
#define FPS 3           // allows to put event at the beginning, middle, and end

#define SEND_VIDEO(num_frames) { \
        for (int i=0; i<num_frames; ++i) { \
            scte35dec_process_timing(&ctx, pts, TIMESCALE, TIMESCALE/FPS); \
            scte35dec_process_dispatch(&ctx, pts, TIMESCALE/FPS); \
            pts += TIMESCALE/FPS; \
        } \
    }

#define SEND_EVENT() { \
        GF_PropertyValue emsg = { .type=GF_PROP_CONST_DATA, .value.data.ptr=scte35_payload, .value.data.size=sizeof(scte35_payload)}; \
        scte35dec_process_timing(&ctx, pts, TIMESCALE, SCTE35_DUR); \
        scte35dec_process_emsg(&ctx, emsg.value.data.ptr, emsg.value.data.size, pts); \
        if (!ctx.pass) scte35dec_process_dispatch(&ctx, pts, SCTE35_DUR); \
        pts = SCTE35_PTS + SCTE35_DUR; \
    }

#define UT_SCTE35_INIT(pck_send_fct) \
    SCTE35DecCtx ctx = {0}; \
    assert_equal(scte35dec_initialize_internal(&ctx), GF_OK); \
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
    assert_equal(TIMESCALE % FPS, 0);
}

/*************************************/

static GF_Err pck_send_no_event(GF_FilterPacket *pck)
{
    #define expected_calls 6
    static int calls = 0;
    static u64 expected_dts [expected_calls] = { 0, TIMESCALE/FPS, 2*TIMESCALE/FPS, 3*TIMESCALE/FPS, 4*TIMESCALE/FPS, 5*TIMESCALE/FPS };
    static u32 expected_dur [expected_calls] = { TIMESCALE/FPS, TIMESCALE/FPS, TIMESCALE/FPS, TIMESCALE/FPS, TIMESCALE/FPS, TIMESCALE/FPS };
    static u32 expected_size[expected_calls] = { EMEB_BOX_SIZE, EMEB_BOX_SIZE, EMEB_BOX_SIZE, EMEB_BOX_SIZE, EMEB_BOX_SIZE, EMEB_BOX_SIZE };

    if (pck == NULL) {
        // checks at termination
        assert_equal(calls, expected_calls);
        return GF_OK;
    }

    // dynamic checks
    assert_less(calls, expected_calls);
    assert_equal(gf_filter_pck_get_dts(pck), expected_dts[calls]);
    assert_equal(gf_filter_pck_get_duration(pck), expected_dur[calls]);

    u32 size = 0;
    const u8 *data = gf_filter_pck_get_data(pck, &size);
    assert_equal(size, expected_size[calls]);
    assert_equal(size, EMEB_BOX_SIZE);
    assert_equal_mem(data, emeb_box, EMEB_BOX_SIZE);

    UT_SCTE35_PCK_SEND_FINALIZE();
    #undef expected_calls
}

unittest(scte35dec_no_event)
{
    UT_SCTE35_INIT(pck_send_no_event);
    u64 pts = 0;

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
        assert_equal(calls, expected_calls);
        return GF_OK;
    }

    // dynamic checks
    assert_less(calls, expected_calls);
    assert_equal(gf_filter_pck_get_dts(pck), expected_dts[calls]);
    assert_equal(gf_filter_pck_get_duration(pck), expected_dur[calls]);

    u32 size = 0;
    const u8 *data = gf_filter_pck_get_data(pck, &size);
    assert_equal(size, expected_size[calls]);
    assert_equal(size, EMEB_BOX_SIZE);
    assert_equal_mem(data, emeb_box, EMEB_BOX_SIZE);

    UT_SCTE35_PCK_SEND_FINALIZE();
    #undef expected_calls
}

unittest(scte35dec_segmentation_no_event)
{
    UT_SCTE35_INIT(pck_send_segmentation_no_event);
    ctx.segdur = (GF_Fraction){1, 1};
    u64 pts = 0;

    SEND_VIDEO(FPS*3); // send 3 seconds of heartbeat

    scte35dec_flush(&ctx);
    scte35dec_finalize_internal(&ctx);
    ctx.pck_send(NULL); // final checks
}

/*************************************/

unittest(scte35dec_splice_point_with_idr)
{
    SCTE35DecCtx ctx = {0};
    assert_equal(scte35dec_initialize_internal(&ctx), GF_OK);
    ctx.pass = GF_TRUE;
    u64 pts = 0;

    SEND_EVENT();
    assert_true(scte35dec_is_splice_point(&ctx, SCTE35_PTS));

    scte35dec_flush(&ctx);
    scte35dec_finalize_internal(&ctx);
}

/*************************************/

static GF_Err pck_send_simple(GF_FilterPacket *pck)
{
    #define expected_calls 5
    static int calls = 0;
    static u64 expected_dts [expected_calls] = {             0,            TIMESCALE/FPS,    SCTE35_PTS, SCTE35_PTS+SCTE35_DUR, SCTE35_PTS+SCTE35_DUR+TIMESCALE/FPS };
    static u32 expected_dur [expected_calls] = { TIMESCALE/FPS, SCTE35_PTS-TIMESCALE/FPS,    SCTE35_DUR,         TIMESCALE/FPS, TIMESCALE/FPS };
    static u32 expected_size[expected_calls] = { EMEB_BOX_SIZE,            EMEB_BOX_SIZE, EMIB_BOX_SIZE,         EMEB_BOX_SIZE, EMEB_BOX_SIZE };
    static s64 expected_event_pts_delta[expected_calls] = { 0, 0, SCTE35_PTS-TIMESCALE/FPS, 0, 0 };
    static u32 expected_event_duration [expected_calls] = { 0, 0, SCTE35_DUR, 0, 0 };
    static u32 expected_event_id       [expected_calls] = { 0, 0, SCTE35_LAST_EVENT_ID, 0, 0 };

    if (pck == NULL) {
        // checks at termination
        assert_equal(calls, expected_calls);
        return GF_OK;
    }

    // dynamic checks
    assert_less(calls, expected_calls);
    assert_equal(gf_filter_pck_get_dts(pck), expected_dts[calls]);
    assert_equal(gf_filter_pck_get_duration(pck), expected_dur[calls]);

    u32 size = 0;
    const u8 *data = gf_filter_pck_get_data(pck, &size);
    assert_equal(size, expected_size[calls]);

    switch(size) {
        case EMEB_BOX_SIZE:
            assert_equal(EMEB_BOX_SIZE, size);
            assert_equal_mem(data, emeb_box, EMEB_BOX_SIZE);
            break;
        case EMIB_BOX_SIZE: {
            assert_less_equal(sizeof(scte35_payload), size);
            assert_equal_mem(data + size - sizeof(scte35_payload), scte35_payload, sizeof(scte35_payload));

            GF_BitStream *bs = gf_bs_new(data, size, GF_BITSTREAM_READ);
            gf_bs_seek(bs, 16);
            assert_equal(gf_bs_read_u64(bs), expected_event_pts_delta[calls]); //presentation_time_delta
            assert_equal(gf_bs_read_u32(bs), expected_event_duration[calls]);  //event_duration
            assert_equal(gf_bs_read_u32(bs), expected_event_id[calls]);        //event_id
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
    u64 pts = 0;

    SEND_VIDEO(1); // video (1 frame)
    SEND_EVENT();  // scte35 event at "pts=1 frame" scheduled for pts=59583 with dur=36637
    SEND_VIDEO(1); // video (1 frame)
    SEND_VIDEO(1); // video (1 frame)

    scte35dec_flush(&ctx);
    scte35dec_finalize_internal(&ctx);
    ctx.pck_send(NULL); // trigger final checks
}

/*************************************/

static GF_Err pck_send_segmentation_beginning(GF_FilterPacket *pck)
{
    #define expected_calls 4
    static int calls = 0;
    static u64 expected_dts [expected_calls] = {             0,           SCTE35_PTS,                       TIMESCALE, SCTE35_PTS+SCTE35_DUR};
    static u32 expected_dur [expected_calls] = {    SCTE35_PTS, TIMESCALE-SCTE35_PTS, SCTE35_PTS+SCTE35_DUR-TIMESCALE, 2*TIMESCALE-(SCTE35_PTS+SCTE35_DUR) };
    static u32 expected_size[expected_calls] = { EMIB_BOX_SIZE,        EMIB_BOX_SIZE,                   EMIB_BOX_SIZE, EMEB_BOX_SIZE };
    static s64 expected_event_pts_delta[expected_calls] = { SCTE35_PTS, 0, 0, 0 };
    static u32 expected_event_duration [expected_calls] = { SCTE35_DUR, SCTE35_DUR, SCTE35_PTS+SCTE35_DUR-TIMESCALE, 0 };
    static u32 expected_event_id       [expected_calls] = { SCTE35_LAST_EVENT_ID, SCTE35_LAST_EVENT_ID, SCTE35_LAST_EVENT_ID, 0 };

    if (pck == NULL) {
        // checks at termination
        assert_equal(calls, expected_calls);
        return GF_OK;
    }

    // dynamic checks
    assert_less(calls, expected_calls);
    assert_equal(gf_filter_pck_get_dts(pck), expected_dts[calls]);
    assert_equal(gf_filter_pck_get_duration(pck), expected_dur[calls]);

    u32 size = 0;
    const u8 *data = gf_filter_pck_get_data(pck, &size);
    assert_equal(size, expected_size[calls]);

    switch(size) {
        case EMEB_BOX_SIZE:
            assert_equal(EMEB_BOX_SIZE, size);
            assert_equal_mem(data, emeb_box, EMEB_BOX_SIZE);
            break;
        case EMIB_BOX_SIZE: {
            assert_less_equal(sizeof(scte35_payload), size);
            assert_equal_mem(data + size - sizeof(scte35_payload), scte35_payload, sizeof(scte35_payload));

            GF_BitStream *bs = gf_bs_new(data, size, GF_BITSTREAM_READ);
            gf_bs_seek(bs, 16);
            assert_equal(gf_bs_read_u64(bs), expected_event_pts_delta[calls]); //presentation_time_delta
            assert_equal(gf_bs_read_u32(bs), expected_event_duration[calls]);  //event_duration
            assert_equal(gf_bs_read_u32(bs), expected_event_id[calls]);        //event_id
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
    ctx.segdur = (GF_Fraction){1, 1};
    u64 pts = 0;

    SEND_EVENT();  // scte35 event scheduled for pts=59583 with dur=36637
    SEND_VIDEO(1); // video (1 frame)

    scte35dec_flush(&ctx);
    scte35dec_finalize_internal(&ctx);
    ctx.pck_send(NULL); // trigger final checks
}

/*************************************/

static GF_Err pck_send_segmentation_end(GF_FilterPacket *pck)
{
    #define expected_calls 6
    static int calls = 0;
    //scte35 event at pts=30000 scheduled for pts=59583 with dur=36637, segment_dur=30000
    //v[0:30000] emeb[30000:59583] emib[59583:60000] emib[60000:90000] emib[90000:96220] emeb[96220:120000]
    static u64 expected_dts [expected_calls] = {             0,            TIMESCALE/FPS,                 SCTE35_PTS,
                                                 2*TIMESCALE/FPS,                       3*TIMESCALE/FPS, SCTE35_PTS+SCTE35_DUR };
    static u32 expected_dur [expected_calls] = { TIMESCALE/FPS, SCTE35_PTS-TIMESCALE/FPS, 2*TIMESCALE/FPS-SCTE35_PTS,
                                                   TIMESCALE/FPS, SCTE35_PTS+SCTE35_DUR-3*TIMESCALE/FPS, 4*TIMESCALE/FPS-SCTE35_PTS-SCTE35_DUR };
    static u32 expected_size[expected_calls] = { EMEB_BOX_SIZE, EMIB_BOX_SIZE, EMIB_BOX_SIZE, EMIB_BOX_SIZE, EMIB_BOX_SIZE, EMEB_BOX_SIZE };
    static s64 expected_event_pts_delta[expected_calls] = { 0, SCTE35_PTS-TIMESCALE/FPS, 0, 0, 0, 0 };
    static u32 expected_event_duration [expected_calls] = { 0, SCTE35_DUR, SCTE35_DUR, SCTE35_DUR-(2*TIMESCALE/FPS-SCTE35_PTS), SCTE35_DUR-(3*TIMESCALE/FPS-SCTE35_PTS), 0 };
    static u32 expected_event_id       [expected_calls] = { SCTE35_LAST_EVENT_ID, SCTE35_LAST_EVENT_ID, SCTE35_LAST_EVENT_ID, SCTE35_LAST_EVENT_ID, SCTE35_LAST_EVENT_ID, SCTE35_LAST_EVENT_ID };

    if (pck == NULL) {
        // checks at termination
        assert_equal(calls, expected_calls);
        return GF_OK;
    }

    // dynamic checks
    assert_less(calls, expected_calls);
    assert_equal(gf_filter_pck_get_dts(pck), expected_dts[calls]);
    assert_equal(gf_filter_pck_get_duration(pck), expected_dur[calls]);

    u32 size = 0;
    const u8 *data = gf_filter_pck_get_data(pck, &size);
    assert_equal(size, expected_size[calls]);

    switch(size) {
        case EMEB_BOX_SIZE:
            assert_equal(EMEB_BOX_SIZE, size);
            assert_equal_mem(data, emeb_box, EMEB_BOX_SIZE);
            break;
        case EMIB_BOX_SIZE: {
            assert_less_equal(sizeof(scte35_payload), size);
            assert_equal_mem(data + size - sizeof(scte35_payload), scte35_payload, sizeof(scte35_payload));

            GF_BitStream *bs = gf_bs_new(data, size, GF_BITSTREAM_READ);
            gf_bs_seek(bs, 16);
            assert_equal(gf_bs_read_u64(bs), expected_event_pts_delta[calls]); //presentation_time_delta
            assert_equal(gf_bs_read_u32(bs), expected_event_duration[calls]);  //event_duration
            assert_equal(gf_bs_read_u32(bs), expected_event_id[calls]);        //event_id
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
    ctx.segdur = (GF_Fraction){1, FPS};
    u64 pts = 0;

    SEND_VIDEO(1); // video (1 frame) 
    SEND_EVENT();  // scte35 event at "pts=1 frame" scheduled for pts=59583 with dur=36637

    scte35dec_flush(&ctx);
    scte35dec_flush(&ctx); // this test requires an additional segment
    scte35dec_finalize_internal(&ctx);
    ctx.pck_send(NULL); // trigger final checks
}

/*************************************/

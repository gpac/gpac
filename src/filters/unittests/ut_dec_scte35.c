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
#define EMIB 105
#define EMEB 8

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

static u8 emeb_box[EMEB] = {
    0x00, 0x00, 0x00, 0x08, 0x65, 0x6d, 0x65, 0x62
};

#define TIMESCALE 90000 // usual value
#define FPS 3           // allows to put event at the beginning, midlle, and end

#define SEND_VIDEO(num_frames) { \
        for (int i=0; i<num_frames; ++i) { \
            scte35dec_process_timing(&ctx, pts, TIMESCALE, TIMESCALE/FPS); \
            scte35dec_process_dispatch(&ctx, pts); \
            pts += TIMESCALE/FPS; \
        } \
    }

#define SEND_EVENT(dur) { \
        GF_PropertyValue emsg = { .type=GF_PROP_CONST_DATA, .value.data.ptr=scte35_payload, .value.data.size=sizeof(scte35_payload)}; \
        scte35dec_process_timing(&ctx, pts, TIMESCALE, dur); \
        scte35dec_process_emsg(&ctx, &emsg, pts); \
        if (!ctx.pass) scte35dec_process_dispatch(&ctx, pts); \
        pts += dur; \
    }

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
    static u64 expected_dts[expected_calls] = { 0, TIMESCALE/FPS, 2*TIMESCALE/FPS, 3*TIMESCALE/FPS, 4*TIMESCALE/FPS, 5*TIMESCALE/FPS };
    static u32 expected_dur[expected_calls] = { 0, TIMESCALE/FPS, TIMESCALE/FPS, TIMESCALE/FPS, TIMESCALE/FPS, TIMESCALE/FPS };
    static u32 expected_size[expected_calls] = { EMEB, EMEB, EMEB, EMEB, EMEB, EMEB };

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
    assert_equal(EMEB, size);
    assert_equal_mem(data, emeb_box, EMEB);

    // update context
    if (!pck->filter_owns_mem) gf_free(pck->data);
    gf_free(pck);
    calls++;
    #undef expected_calls

    return GF_OK;
}

unittest(scte35dec_no_event)
{
    SCTE35DecCtx ctx = {0};
    assert_equal(scte35dec_initialize_internal(&ctx), GF_OK);
    ctx.pck_new_shared = pck_new_shared;
    ctx.pck_new_alloc = pck_new_alloc;
    ctx.pck_send = pck_send_no_event;

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
    static u64 expected_dts[expected_calls] = { 0, TIMESCALE, 2*TIMESCALE };
    static u32 expected_dur[expected_calls] = { TIMESCALE, TIMESCALE, TIMESCALE  };
    static u32 expected_size[expected_calls] = { EMEB, EMEB, EMEB };

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
    assert_equal(EMEB, size);
    assert_equal_mem(data, emeb_box, EMEB);

    // update context
    if (!pck->filter_owns_mem) gf_free(pck->data);
    gf_free(pck);
    calls++;
    #undef expected_calls

    return GF_OK;
}

unittest(scte35dec_segmentation_no_event)
{
    SCTE35DecCtx ctx = {0};
    assert_equal(scte35dec_initialize_internal(&ctx), GF_OK);
    ctx.pck_new_shared = pck_new_shared;
    ctx.pck_new_alloc = pck_new_alloc;
    ctx.pck_send = pck_send_segmentation_no_event;

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
    SEND_EVENT(SCTE35_DUR);
    assert_true(scte35dec_is_splice_point(&ctx, SCTE35_PTS));

    scte35dec_flush(&ctx);
    scte35dec_finalize_internal(&ctx);
}

/*************************************/

static GF_Err pck_send_simple(GF_FilterPacket *pck)
{
    #define expected_calls 4 //FIXME: if we put 5 then we have some drift control that triggers a final EMEB box
    static int calls = 0;
    static u64 expected_dts[expected_calls] = { 0, TIMESCALE/FPS, SCTE35_PTS, SCTE35_PTS+SCTE35_DUR };//, SCTE35_PTS+SCTE35_DUR+TIMESCALE/FPS };
    static u32 expected_dur[expected_calls] = { 0/*TIMESCALE/FPS*/, SCTE35_PTS/*-TIMESCALE/FPS*/, SCTE35_DUR, SCTE35_DUR/*FIXME: TIMESCALE/FPS*/ };//, TIMESCALE/FPS+TIMESCALE/FPS-SCTE35_DUR/*TIMESCALE/FPS*/};
    static u32 expected_size[expected_calls] = { EMEB, EMEB, EMIB, EMEB };//, EMEB };
    static s64 expected_event_pts_delta[expected_calls] = { 0, 0, SCTE35_PTS-TIMESCALE/FPS, 0 };//, 0 };
    static u32 expected_event_duration[expected_calls] = { 0, 0, SCTE35_DUR, 0 };//, 0 };
    static u32 expected_event_id[expected_calls] = { 0, 0, SCTE35_LAST_EVENT_ID, 0 };//, 0 };

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
        case EMEB:
            assert_equal(EMEB, size);
            assert_equal_mem(data, emeb_box, EMEB);
            break;
        case EMIB: {
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
        default:
            assert_true(0);
    }

    // update context
    if (!pck->filter_owns_mem) gf_free(pck->data);
    gf_free(pck);
    calls++;
    #undef expected_calls

    return GF_OK;
}

unittest(scte35dec_simple)
{
    SCTE35DecCtx ctx = {0};
    assert_equal(scte35dec_initialize_internal(&ctx), GF_OK);
    ctx.pck_new_shared = pck_new_shared;
    ctx.pck_new_alloc = pck_new_alloc;
    ctx.pck_send = pck_send_simple;

    u64 pts = 0;
    SEND_VIDEO(1);
    SEND_EVENT(SCTE35_PTS + SCTE35_DUR - pts);
    SEND_VIDEO(1);
    //SEND_VIDEO(1);

    scte35dec_flush(&ctx);
    scte35dec_finalize_internal(&ctx);

    ctx.pck_send(NULL); // trigger final checks
}

/*************************************/

static GF_Err pck_send_segmentation_beginning(GF_FilterPacket *pck)
{
    #define expected_calls 2
    static int calls = 0;
    static u64 expected_dts[expected_calls] = { 0, SCTE35_DUR/*FIXME: SCTE35_PTS or SCTE35_PTS+SCTE35_DUR!!!*/ };
    static u32 expected_dur[expected_calls] = { SCTE35_DUR, SCTE35_DUR/*FIXME: TIMESCALE/FPS*/ };
    static u32 expected_size[expected_calls] = { EMIB, EMEB };
    static s64 expected_event_pts_delta[expected_calls] = { SCTE35_PTS, 0 };
    static u32 expected_event_duration[expected_calls] = { SCTE35_DUR, 0 };
    static u32 expected_event_id[expected_calls] = { SCTE35_LAST_EVENT_ID, 0 };

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
        case EMEB:
            assert_equal(EMEB, size);
            assert_equal_mem(data, emeb_box, EMEB);
            break;
        case EMIB: {
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
        default:
            assert_true(0);
    }

    // update context
    if (!pck->filter_owns_mem) gf_free(pck->data);
    gf_free(pck);
    calls++;
    #undef expected_calls

    return GF_OK;
}

unittest(scte35dec_segmentation_beginning)
{
    SCTE35DecCtx ctx = {0};
    assert_equal(scte35dec_initialize_internal(&ctx), GF_OK);
    ctx.pck_new_shared = pck_new_shared;
    ctx.pck_new_alloc = pck_new_alloc;
    ctx.pck_send = pck_send_segmentation_beginning;

    ctx.segdur = (GF_Fraction){1, 1};

    u64 pts = 0;
    SEND_EVENT(SCTE35_DUR);
    SEND_VIDEO(1);

    scte35dec_flush(&ctx);
    scte35dec_finalize_internal(&ctx);

    ctx.pck_send(NULL); // trigger final checks
}

/*************************************/

static GF_Err pck_send_segmentation_end(GF_FilterPacket *pck)
{
    #define expected_calls 2 //FIXME: FPS
    static int calls = 0;
    static u64 expected_dts[expected_calls] = { 0, TIMESCALE/FPS/*SCTE35_PTS*/ };//, TIMESCALE/FPS+SCTE35_DUR };
    static u32 expected_dur[expected_calls] = { 5*TIMESCALE/FPS/*FIXME: TIMESCALE/FPS*/, SCTE35_DUR/*this is what the algorithm infers*/ }; //FIXME
    static u32 expected_size[expected_calls] = { EMEB, EMIB };//, EMEB };
    static s64 expected_event_pts_delta[expected_calls] = { 0, SCTE35_PTS-TIMESCALE/FPS };//, 0 };
    static u32 expected_event_duration[expected_calls] = { 0, SCTE35_DUR };//, 0 };
    static u32 expected_event_id[expected_calls] = { 0, SCTE35_LAST_EVENT_ID };//, 0 };

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
        case EMEB:
            assert_equal(EMEB, size);
            assert_equal_mem(data, emeb_box, EMEB);
            break;
        case EMIB: {
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
        default:
            assert_true(0);
    }

    // update context
    if (!pck->filter_owns_mem) gf_free(pck->data);
    gf_free(pck);
    calls++;
    #undef expected_calls

    return GF_OK;
}

unittest(scte35dec_segmentation_end)
{
    SCTE35DecCtx ctx = {0};
    assert_equal(scte35dec_initialize_internal(&ctx), GF_OK);
    ctx.pck_new_shared = pck_new_shared;
    ctx.pck_new_alloc = pck_new_alloc;
    ctx.pck_send = pck_send_segmentation_end;

    ctx.segdur = (GF_Fraction){1, 1};

    u64 pts = 0;
    SEND_VIDEO(1);
    SEND_EVENT(SCTE35_DUR);

    scte35dec_flush(&ctx);
    scte35dec_finalize_internal(&ctx);

    ctx.pck_send(NULL); // trigger final checks
}

/*************************************/

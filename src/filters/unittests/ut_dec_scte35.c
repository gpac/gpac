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
#define EMIB 109
#define EMEB 8

static u8 scte35_payload[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xff,
    0xf0, 0x14, 0x05, 0x4f, 0xff, 0xff, 0xf2, 0x7f,
    0xef, 0xff, 0x53, 0x35, 0xe8, 0xbf, 0xfe, 0x00,
    0x52, 0x6f, 0x1d, 0x00, 0x01, 0x00, 0x00, 0x00,
    0x0a, 0x00, 0x08, 0x43, 0x55, 0x45, 0x49, 0x00,
    0x00, 0x00, 0x18, 0x9f, 0x1d, 0x41, 0x47
};

static GF_Err pck_send(GF_FilterPacket *pck)
{
    #define expected_calls 1
    static int calls = 0;
    assert_less(calls, expected_calls);

    static u64 expected_dts[expected_calls] = { 0 };
    assert_equal(gf_filter_pck_get_dts(pck), expected_dts[calls]);

    static u32 expected_dur[expected_calls] = { 5402397 };
    assert_equal(gf_filter_pck_get_duration(pck), expected_dur[calls]);

    u32 size = 0;
    const u8 *data = gf_filter_pck_get_data(pck, &size);
    static u32 expected_size[expected_calls] = { EMIB };
    assert_equal(size, expected_size[calls]);
    assert_less_equal(sizeof(scte35_payload), size);
    assert_equal_mem(data + size - sizeof(scte35_payload), scte35_payload, sizeof(scte35_payload));

    if (!pck->filter_owns_mem) gf_free(pck->data);
    gf_free(pck);
    calls++;
    return GF_OK;
}

unittest(scte35dec_simple)
{
    SCTE35DecCtx ctx = {0};
    assert_equal(scte35dec_initialize_internal(&ctx), GF_OK);
    ctx.pck_new_shared = pck_new_shared;
	ctx.pck_new_alloc = pck_new_alloc;
    ctx.pck_send = pck_send;

    u64 dts = 0;
    u32 timescale = 90000, dur = 2250;
    scte35dec_process_timing(&ctx, dts, timescale, dur);
    GF_PropertyValue emsg = { .type=GF_PROP_CONST_DATA, .value.data.ptr=scte35_payload, .value.data.size=sizeof(scte35_payload)};
    scte35dec_process_emsg(&ctx, &emsg, dts, timescale);
    scte35dec_process_dispatch(&ctx, dts);

    assert_equal(scte35dec_flush(&ctx, 0, UINT32_MAX), GF_OK);
    scte35dec_finalize_internal(&ctx);
}

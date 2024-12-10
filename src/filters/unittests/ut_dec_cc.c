#include "tests.h"
#include "../dec_cc.c"
#include "../../filter_core/filter_session.h" // GF_FilterPacket


static GF_Err pck_truncate(GF_FilterPacket *pck, u32 size)
{
    pck->data_length = size;
    return GF_OK;
}

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


const char* txt = "GP\nA C \n1 2";

static void ccdec_test_template(int agg, Bool text_with_overlaps, GF_Err (*pck_send)(GF_FilterPacket *pck))
{
    CCDecCtx ctx = {0};
    ctx.agg = agg;

	ctx.pck_send = pck_send;
    ctx.pck_truncate = pck_truncate;
    ctx.pck_new_alloc = pck_new_alloc;

    for (size_t i=0; i<strlen(txt); ++i) {
        //we write to ctx.txtdata+ctx.txtlen like dec_cc.c does
        if (text_with_overlaps) {
            strncpy(ctx.txtdata+ctx.txtlen, txt, i+1);
            text_aggregate_and_post(&ctx, i+1, i);
        } else {
            assert_equal(ctx.txtlen, i);
            strncpy(ctx.txtdata+ctx.txtlen, txt+i, 1);
            text_aggregate_and_post(&ctx, 1, i);
        }
    }

    //termination calls
    ccdec_flush(&ctx);
    pck_send(NULL);
}


static GF_Err pck_send_default(GF_FilterPacket *pck)
{
    static int calls = 0;
    static const char* expected[] = { "G", "GP", "GP\n", "GP\nA", "GP\nA ", "GP\nA C", "GP\nA C ", "GP\nA C \n", "GP\nA C \n1", "GP\nA C \n1 ", "GP\nA C \n1 2" };
    
    if (!pck) {
        assert_equal(calls, sizeof(expected)/sizeof(const char*));
        return GF_OK;
    }
    
    u32 size = 0;
    const u8 *data = gf_filter_pck_get_data(pck, &size);
    assert_equal_str(data, expected[calls]);
    calls++;
    assert_equal(size, calls);
    return GF_OK;
}

unittest(ccdec_default)
{
    ccdec_test_template(0, GF_TRUE, pck_send_default);
}


static GF_Err pck_send_aggregation(GF_FilterPacket *pck)
{
    static int calls = 0;
    static const char* expected[] = { "GP\n", "GP\nA", "GP\nA C", "GP\nA C \n", "GP\nA C \n1", "GP\nA C \n1 2" };

    if (!pck) {
        assert_equal(calls, sizeof(expected)/sizeof(const char*));
        return GF_OK;
    }

    u32 size = 0;
    const u8 *data = gf_filter_pck_get_data(pck, &size);
    assert_equal_str(data, expected[calls]);
    calls++;
    return GF_OK;
}

unittest(ccdec_aggregation)
{
    ccdec_test_template(1, GF_FALSE, pck_send_aggregation);
}

unittest(ccdec_aggregation_overlaps)
{
    ccdec_test_template(1, GF_TRUE, pck_send_aggregation);
}


static GF_Err pck_send_several_entries(GF_FilterPacket *pck)
{
    static int calls = 0;
    static const char* expected[] = { "GPAC", "ROCKS" };
    
    if (!pck) {
        assert_equal(calls, sizeof(expected)/sizeof(const char*));
        return GF_OK;
    }
    
    u32 size = 0;
    const u8 *data = gf_filter_pck_get_data(pck, &size);
    assert_equal_str(data, expected[calls]);
    calls++;
    assert_equal(size, calls);
    return GF_OK;
}

unittest(ccdec_several_entries)
{
    CCDecCtx ctx = {0};
    ctx.agg = 1;

	ctx.pck_send = pck_send_several_entries;
    ctx.pck_truncate = pck_truncate;
    ctx.pck_new_alloc = pck_new_alloc;

    u64 ts = 0;
    strcpy(ctx.txtdata, "GPAC");
    text_aggregate_and_post(&ctx, strlen(ctx.txtdata), ts++);

    strcpy(ctx.txtdata, "ROCKS");
    ctx.txtlen = 0;
    text_aggregate_and_post(&ctx, strlen(ctx.txtdata), ts++);

    //termination calls
    ccdec_flush(&ctx);
    ctx.pck_send(NULL);
}

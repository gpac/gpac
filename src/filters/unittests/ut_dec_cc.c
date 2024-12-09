#include "tests.h"
#include "../dec_cc.c"
#include "../../filter_core/filter_session.h" // GF_FilterPacket

static GF_Err pck_truncate(GF_FilterPacket *pck, u32 size)
{
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


static GF_Err pck_send_default(GF_FilterPacket *pck)
{
    #define expected_calls 13
    static int calls = 0;
    static const char* expected[expected_calls] = { "G", "GP", "GP\n", "GP\nA", "GP\nA ", "GP\nA C", "GP\nA C ", "GP\nA C \n", "GP\nA C \n1", "GP\nA C \n1 ", "GP\nA C \n1 2", "GP\nA C \n1 2", "GP\nA C \n1 2" };
    
    if (!pck) {
        assert_equal(calls, expected_calls);
        return GF_OK;
    }
    
    u32 size = 0;
    const u8 *data = gf_filter_pck_get_data(pck, &size);
    assert_equal_str(data, expected[calls]);
    assert_equal(size, calls);
    calls++;
    return GF_OK;
}

unittest(ccdec_default)
{
    CCDecCtx ctx = {0};
	ctx.pck_send = pck_send_default;
    ctx.pck_truncate = pck_truncate;
    ctx.pck_new_alloc = pck_new_alloc;
    ctx.opid = gf_filter_pid_new(NULL);
    
    for (size_t i=1; i<strlen(txt); ++i) {
        strncpy(ctx.txtdata, txt, i);
        text_aggregate_and_post(&ctx, (u32)i, (u64)i);
    }
    
    //termination call
    pck_send_default(NULL);

    gf_filter_pid_del(ctx.opid);
}

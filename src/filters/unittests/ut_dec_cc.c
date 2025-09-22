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
#ifdef GPAC_HAS_LIBCAPTION
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
#endif // GPAC_HAS_LIBCAPTION
}


static GF_Err pck_send_default(GF_FilterPacket *pck)
{
	static int calls = 0;
	static const char* expected[] = { "G", "GP", "GP\n", "GP\nA", "GP\nA ", "GP\nA C", "GP\nA C ", "GP\nA C \n", "GP\nA C \n1", "GP\nA C \n1 ", "GP\nA C \n1 2" };
	const int num_expected = sizeof(expected)/sizeof(const char*);
	
	if (!pck) {
		assert_equal(calls, num_expected);
		return GF_OK;
	}
	if (calls >= num_expected)
		assert_true(0);
	
	u32 size = 0;
	const u8 *data = gf_filter_pck_get_data(pck, &size);
	assert_equal_str(data, expected[calls]);
	gf_free((u8*)data);
	gf_free(pck);
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
	const int num_expected = sizeof(expected)/sizeof(const char*);

	if (!pck) {
		assert_equal(calls, num_expected);
		calls = 0;
		return GF_OK;
	}
	if (calls >= num_expected)
		assert_true(0);

	u32 size = 0;
	const u8 *data = gf_filter_pck_get_data(pck, &size);
	assert_equal_str(data, expected[calls]);
	gf_free((u8*)data);
	gf_free(pck);
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
	const int num_expected = sizeof(expected)/sizeof(const char*);

	if (!pck) {
		assert_equal(calls, num_expected);
		return GF_OK;
	}
	if (calls >= num_expected)
		assert_true(0);
	
	u32 size = 0;
	const u8 *data = gf_filter_pck_get_data(pck, &size);
	assert_equal_str(data, expected[calls]);
	gf_free((u8*)data);
	gf_free(pck);
	calls++;
	return GF_OK;
}

unittest(ccdec_several_entries)
{
#ifdef GPAC_HAS_LIBCAPTION
	u64 ts = 0;
	CCDecCtx ctx = {0};
	ctx.agg = 1;

	ctx.pck_send = pck_send_several_entries;
	ctx.pck_truncate = pck_truncate;
	ctx.pck_new_alloc = pck_new_alloc;

	strcpy(ctx.txtdata+ctx.txtlen, "GPAC");
	text_aggregate_and_post(&ctx, strlen("GPAC"), ts++);

	strcpy(ctx.txtdata+ctx.txtlen, "ROCKS");
	text_aggregate_and_post(&ctx, strlen("ROCKS"), ts++);

	//termination calls
	ccdec_flush(&ctx);
	ctx.pck_send(NULL);
#endif // GPAC_HAS_LIBCAPTION
}

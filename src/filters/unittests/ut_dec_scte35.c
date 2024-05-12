#include "tests.h"
#include "../dec_scte35.c"

unittest(scte35dec_initialize)
{
    SCTE35DecCtx ctx = {0};
    assert_equal(scte35dec_initialize_internal(&ctx), GF_OK);

    //GF_Fraction intra_period = {1, 1};
    //ctx->segdur = intra_period;
}

#include "tests.h"

#ifndef GPAC_DISABLE_ROUTE
#include "ut_route_helpers.h"

/*
 * gf_route_dmx_reset_all() moved each object to the reservoir while walking s->objects
 * with gf_list_enum(), which skipped the object following each removed one: half of the
 * objects stayed and were not dispatched again when the same TSI/TOI came back.
 */
unittest(route_dmx_reset_all_removes_every_object)
{
	UTRouteCtx ctx;
	u32 toi;
	assert_true(ut_route_open(&ctx));
	if (!ctx.dmx) return;

	for (toi=1; toi<=4; toi++)
		ut_route_send_object(&ctx, toi, "old-segment");
	assert_equal(ctx.nb_segs, 4, "%u");

	gf_route_dmx_reset_all(ctx.dmx);
	assert_equal(gf_route_dmx_get_object_count(ctx.dmx, 1), 0, "%u");
	assert_equal(ctx.nb_deleted, 4, "%u");

	//the same objects received again after the reset are new objects
	ctx.nb_segs = 0;
	for (toi=1; toi<=4; toi++)
		ut_route_send_object(&ctx, toi, "new-segment");
	assert_equal(ctx.nb_segs, 4, "%u");

	ut_route_close(&ctx);
}

#endif

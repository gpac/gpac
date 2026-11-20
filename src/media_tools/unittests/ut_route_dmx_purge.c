#include "tests.h"

#ifndef GPAC_DISABLE_ROUTE
#include "ut_route_helpers.h"

/*
 * gf_route_dmx_purge_objects() moved objects to the reservoir while walking s->objects
 * with gf_list_enum(), which skipped the object following each removed one: every other
 * completed segment stayed and was not dispatched again on the next loop of the session.
 */
unittest(route_dmx_purge_objects_removes_every_object)
{
	UTRouteCtx ctx;
	u32 toi;
	assert_true(ut_route_open(&ctx));
	if (!ctx.dmx) return;

	for (toi=1; toi<=4; toi++)
		ut_route_send_object(&ctx, toi, "old-segment");
	assert_equal(ctx.nb_segs, 4, "%u");

	gf_route_dmx_purge_objects(ctx.dmx, 1);
	assert_equal(gf_route_dmx_get_object_count(ctx.dmx, 1), 0, "%u");
	assert_equal(ctx.nb_deleted, 4, "%u");

	//second loop of the session: the same segments are dispatched again
	ctx.nb_segs = 0;
	for (toi=1; toi<=4; toi++)
		ut_route_send_object(&ctx, toi, "new-segment");
	assert_equal(ctx.nb_segs, 4, "%u");

	ut_route_close(&ctx);
}

#endif

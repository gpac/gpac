#include "tests.h"

#ifndef GPAC_DISABLE_ROUTE
#include "ut_route_helpers.h"

/*
 * An object received without the TOL extension was delivered with its total size set to
 * the capacity of its payload buffer. Buffers are recycled from the reservoir, so the blob
 * carried the bytes of an earlier, larger object after the received data.
 */
unittest(route_dmx_object_without_tol_size)
{
	UTRouteCtx ctx;
	assert_true(ut_route_open(&ctx));
	if (!ctx.dmx) return;

	//a larger object, then removed: its buffer goes back to the reservoir
	ut_route_send_object(&ctx, 1, "OLD-OBJECT-0123456789-0123456789-0123456");
	gf_route_dmx_purge_objects(ctx.dmx, 1);

	//4 bytes, close flag, no TOL; delivered when the next object starts
	ut_route_send(&ctx, GF_TRUE, 10, 2, 0, 0, "new2", 4);
	ut_route_send_object(&ctx, 3, "new-3");

	assert_equal(ctx.nb_segs, 3, "%u");
	assert_equal(ctx.seg_toi[1], 2, "%u");
	assert_equal(ctx.seg_total_size[1], 4, "%u");
	assert_equal(ctx.seg_blob_size[1], 4, "%u");

	ut_route_close(&ctx);
}

#endif

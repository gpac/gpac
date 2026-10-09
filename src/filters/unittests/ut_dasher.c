#include "tests.h"
#include <gpac/filters.h>

Bool dasher_scte35_boundary_due(u64 packet_cts, u32 packet_timescale, u32 sap_type, u64 boundary_time);

unittest(dasher_scte35_boundary_uses_first_sap_at_or_after_splice)
{
	const u64 splice_time_90k = 6330602;

	/* A predictive packet after the cue cannot start an independently decodable
	 * HLS segment. */
	assert_false(dasher_scte35_boundary_due(6332400, 90000, GF_FILTER_SAP_NONE, splice_time_90k));

	/* A SAP before the requested splice point is still too early. */
	assert_false(dasher_scte35_boundary_due(6330000, 90000, GF_FILTER_SAP_1, splice_time_90k));

	/* Exact and slightly-later SAPs are valid boundaries. The latter models the
	 * real broadcast sample where the cue is about 20 ms before the IDR. */
	assert_true(dasher_scte35_boundary_due(6330602, 90000, GF_FILTER_SAP_1, splice_time_90k));
	assert_true(dasher_scte35_boundary_due(6332400, 90000, GF_FILTER_SAP_1, splice_time_90k));

	/* Comparison is timescale-safe. 70.36 s in a 1 kHz stream is after the
	 * 70.340022 s SCTE splice time. */
	assert_true(dasher_scte35_boundary_due(70360, 1000, GF_FILTER_SAP_1, splice_time_90k));
	assert_false(dasher_scte35_boundary_due(70300, 1000, GF_FILTER_SAP_1, splice_time_90k));
}

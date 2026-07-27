#include "tests.h"
#include <string.h>
#include <gpac/bitstream.h>
#include <gpac/internal/media_dev.h>

unittest(avc_pps_weighted_bipred_idc)
{
	AVCState avc;
	s32 pps_id;
	/* Minimal AVC PPS NAL, byte-aligned:
	 * - byte 0 (0x68): NAL header (nal_ref_idc=3, nal_unit_type=8/PPS)
	 * - byte 1 (0xCE) + byte 2 (0x78): PPS RBSP with all ue(v)/se(v) fields set
	 *   to 0 except weighted_bipred_idc, encoded as 1 (binary "01").
	 */
	u8 pps_nal[] = { 0x68, 0xCE, 0x78 };
	GF_BitStream *bs;

	memset(&avc, 0, sizeof(avc));
	/* fake a registered SPS so the PPS sps_id lookup doesn't reject the PPS */
	avc.sps[0].state = 1;

	bs = gf_bs_new(pps_nal, sizeof(pps_nal), GF_BITSTREAM_READ);
	pps_id = gf_avc_read_pps_bs(bs, &avc);
	gf_bs_del(bs);

	assert_true(pps_id == 0);
	assert_equal(avc.pps[0].weighted_bipred_idc, 1, "%u");
}

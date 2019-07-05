/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2017-2018
 *					All rights reserved
 *
 *  This file is part of GPAC / filters sub-project
 *
 *  GPAC is free software; you can redistribute it and/or modify
 *  it under the terfsess of the GNU Lesser General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your option)
 *  any later version.
 *
 *  GPAC is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; see the file COPYING.  If not, write to
 *  the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 */

#include "filter_session.h"
#include <gpac/network.h>


const GF_FilterRegister *ut_filter_register(GF_FilterSession *session);
const GF_FilterRegister *ut_source_register(GF_FilterSession *session);
const GF_FilterRegister *ut_sink_register(GF_FilterSession *session);
const GF_FilterRegister *ut_sink2_register(GF_FilterSession *session);
const GF_FilterRegister *ffdmx_register(GF_FilterSession *session);
const GF_FilterRegister *ffdec_register(GF_FilterSession *session);
const GF_FilterRegister *ffavin_register(GF_FilterSession *session);
const GF_FilterRegister *ffsws_register(GF_FilterSession *session);
const GF_FilterRegister *inspect_register(GF_FilterSession *session);
const GF_FilterRegister *probe_register(GF_FilterSession *session);
const GF_FilterRegister *compose_filter_register(GF_FilterSession *session);
const GF_FilterRegister *isoffin_register(GF_FilterSession *session);
const GF_FilterRegister *bifs_dec_register(GF_FilterSession *session);
const GF_FilterRegister *odf_dec_register(GF_FilterSession *session);
const GF_FilterRegister *filein_register(GF_FilterSession *session);
const GF_FilterRegister *ctxload_register(GF_FilterSession *session);
const GF_FilterRegister *httpin_register(GF_FilterSession *session);
const GF_FilterRegister *svgin_register(GF_FilterSession *session);
const GF_FilterRegister *img_reframe_register(GF_FilterSession *session);
const GF_FilterRegister *imgdec_register(GF_FilterSession *session);
const GF_FilterRegister *adts_dmx_register(GF_FilterSession *session);
const GF_FilterRegister *latm_dmx_register(GF_FilterSession *session);
const GF_FilterRegister *mp3_dmx_register(GF_FilterSession *session);
const GF_FilterRegister *faad_register(GF_FilterSession *session);
const GF_FilterRegister *maddec_register(GF_FilterSession *session);
const GF_FilterRegister *xviddec_register(GF_FilterSession *session);
const GF_FilterRegister *j2kdec_register(GF_FilterSession *session);
const GF_FilterRegister *ac3dmx_register(GF_FilterSession *session);
const GF_FilterRegister *a52dec_register(GF_FilterSession *session);
const GF_FilterRegister *amrdmx_register(GF_FilterSession *session);
const GF_FilterRegister *oggdmx_register(GF_FilterSession *session);
const GF_FilterRegister *vorbisdec_register(GF_FilterSession *session);
const GF_FilterRegister *theoradec_register(GF_FilterSession *session);
const GF_FilterRegister *m2tsdmx_register(GF_FilterSession *session);
const GF_FilterRegister *sockin_register(GF_FilterSession *session);
const GF_FilterRegister *dvblin_register(GF_FilterSession *session);
const GF_FilterRegister *vtbdec_register(GF_FilterSession *session);
const GF_FilterRegister *lsrdec_register(GF_FilterSession *session);
const GF_FilterRegister *safdmx_register(GF_FilterSession *session);
const GF_FilterRegister *osvcdec_register(GF_FilterSession *session);
#ifdef GPAC_OPENHEVC_STATIC
const GF_FilterRegister *ohevcdec_register(GF_FilterSession *session);
#endif
const GF_FilterRegister *dashdmx_register(GF_FilterSession *session);
const GF_FilterRegister *cenc_decrypt_register(GF_FilterSession *session);
const GF_FilterRegister *cenc_encrypt_register(GF_FilterSession *session);
const GF_FilterRegister *mp4_mux_register(GF_FilterSession *session);
const GF_FilterRegister *qcpdmx_register(GF_FilterSession *session);
const GF_FilterRegister *h263dmx_register(GF_FilterSession *session);
const GF_FilterRegister *mpgviddmx_register(GF_FilterSession *session);
const GF_FilterRegister *nhntdmx_register(GF_FilterSession *session);
const GF_FilterRegister *nhmldmx_register(GF_FilterSession *session);
const GF_FilterRegister *naludmx_register(GF_FilterSession *session);
const GF_FilterRegister *m2psdmx_register(GF_FilterSession *session);
const GF_FilterRegister *avidmx_register(GF_FilterSession *session);
const GF_FilterRegister *txtin_register(GF_FilterSession *session);
const GF_FilterRegister *ttxtdec_register(GF_FilterSession *session);
const GF_FilterRegister *vttdec_register(GF_FilterSession *session);
const GF_FilterRegister *rtpin_register(GF_FilterSession *session);
const GF_FilterRegister *fileout_register(GF_FilterSession *session);
const GF_FilterRegister *adtsmx_register(GF_FilterSession *session);
const GF_FilterRegister *latm_mx_register(GF_FilterSession *session);
const GF_FilterRegister *reframer_register(GF_FilterSession *session);
const GF_FilterRegister *writegen_register(GF_FilterSession *session);
const GF_FilterRegister *nalumx_register(GF_FilterSession *session);
const GF_FilterRegister *qcpmx_register(GF_FilterSession *session);
const GF_FilterRegister *vttmx_register(GF_FilterSession *session);
const GF_FilterRegister *nhntdump_register(GF_FilterSession *session);
const GF_FilterRegister *nhmldump_register(GF_FilterSession *session);
const GF_FilterRegister *vobsubdmx_register(GF_FilterSession *session);
const GF_FilterRegister *avimux_register(GF_FilterSession *session);
const GF_FilterRegister *aout_register(GF_FilterSession *session);
const GF_FilterRegister *m4vmx_register(GF_FilterSession *session);
const GF_FilterRegister *resample_register(GF_FilterSession *session);
#if !defined(GPAC_CONFIG_ANDROID)
const GF_FilterRegister *vout_register(GF_FilterSession *session);
#endif
const GF_FilterRegister *vcrop_register(GF_FilterSession *session);
const GF_FilterRegister *vflip_register(GF_FilterSession *session);
const GF_FilterRegister *rawvidreframe_register(GF_FilterSession *session);
const GF_FilterRegister *pcmreframe_register(GF_FilterSession *session);
const GF_FilterRegister *jpgenc_register(GF_FilterSession *session);
const GF_FilterRegister *pngenc_register(GF_FilterSession *session);
const GF_FilterRegister *ffenc_register(GF_FilterSession *session);
const GF_FilterRegister *rewind_register(GF_FilterSession *session);
const GF_FilterRegister *filelist_register(GF_FilterSession *session);
const GF_FilterRegister *tsmux_register(GF_FilterSession *session);
const GF_FilterRegister *dasher_register(GF_FilterSession *session);
const GF_FilterRegister *tileagg_register(GF_FilterSession *session);
#if !defined(GPAC_CONFIG_ANDROID)
const GF_FilterRegister *pipein_register(GF_FilterSession *session);
const GF_FilterRegister *pipeout_register(GF_FilterSession *session);
#endif
const GF_FilterRegister *gsfmx_register(GF_FilterSession *session);
const GF_FilterRegister *gsfdmx_register(GF_FilterSession *session);
const GF_FilterRegister *sockout_register(GF_FilterSession *session);
const GF_FilterRegister *av1dmx_register(GF_FilterSession *session);
const GF_FilterRegister *obumx_register(GF_FilterSession *session);
#if !defined(GPAC_CONFIG_IOS) && !defined(GPAC_CONFIG_ANDROID)
const GF_FilterRegister *nvdec_register(GF_FilterSession *session);
#endif
const GF_FilterRegister *atscin_register(GF_FilterSession *session);
const GF_FilterRegister *rtpout_register(GF_FilterSession *session);
const GF_FilterRegister *rtspout_register(GF_FilterSession *session);
const GF_FilterRegister *hevcsplit_register(GF_FilterSession *session);
const GF_FilterRegister *hevcmerge_register(GF_FilterSession *session);

#ifdef GPAC_HAVE_DTAPI
const GF_FilterRegister *dtout_register(GF_FilterSession *session)
#endif

#ifdef GPAC_CONFIG_ANDROID
const GF_FilterRegister *mcdec_register(GF_FilterSession *session);
#endif

const GF_FilterRegister *flac_dmx_register(GF_FilterSession *session);

void gf_fs_reg_all(GF_FilterSession *fsess, GF_FilterSession *a_sess)
{
	gf_fs_add_filter_register(fsess, inspect_register(a_sess) );
	gf_fs_add_filter_register(fsess, probe_register(a_sess) );
	gf_fs_add_filter_register(fsess, compose_filter_register(a_sess) );
	gf_fs_add_filter_register(fsess, isoffin_register(a_sess) );
	gf_fs_add_filter_register(fsess, bifs_dec_register(a_sess) );
	gf_fs_add_filter_register(fsess, odf_dec_register(a_sess) );
	gf_fs_add_filter_register(fsess, filein_register(a_sess) );
	gf_fs_add_filter_register(fsess, ctxload_register(a_sess) );
	gf_fs_add_filter_register(fsess, httpin_register(a_sess) );
	gf_fs_add_filter_register(fsess, svgin_register(a_sess) );
	gf_fs_add_filter_register(fsess, img_reframe_register(a_sess) );
	gf_fs_add_filter_register(fsess, imgdec_register(a_sess) );
	gf_fs_add_filter_register(fsess, adts_dmx_register(a_sess) );
	gf_fs_add_filter_register(fsess, latm_dmx_register(a_sess) );
	gf_fs_add_filter_register(fsess, mp3_dmx_register(a_sess) );
	gf_fs_add_filter_register(fsess, faad_register(a_sess) );
	gf_fs_add_filter_register(fsess, maddec_register(a_sess) );
	gf_fs_add_filter_register(fsess, xviddec_register(a_sess) );
	gf_fs_add_filter_register(fsess, j2kdec_register(a_sess) );
	gf_fs_add_filter_register(fsess, ac3dmx_register(a_sess) );
	gf_fs_add_filter_register(fsess, a52dec_register(a_sess) );
	gf_fs_add_filter_register(fsess, amrdmx_register(a_sess) );
	gf_fs_add_filter_register(fsess, oggdmx_register(a_sess) );
	gf_fs_add_filter_register(fsess, vorbisdec_register(a_sess) );
	gf_fs_add_filter_register(fsess, theoradec_register(a_sess) );
	gf_fs_add_filter_register(fsess, m2tsdmx_register(a_sess) );
	gf_fs_add_filter_register(fsess, sockin_register(a_sess) );
	gf_fs_add_filter_register(fsess, dvblin_register(a_sess) );
	gf_fs_add_filter_register(fsess, osvcdec_register(a_sess) );
	gf_fs_add_filter_register(fsess, vtbdec_register(a_sess) );

#ifdef GPAC_CONFIG_ANDROID
	gf_fs_add_filter_register(fsess, mcdec_register(a_sess) );
#endif

	gf_fs_add_filter_register(fsess, lsrdec_register(a_sess) );
	gf_fs_add_filter_register(fsess, safdmx_register(a_sess) );
#ifdef GPAC_OPENHEVC_STATIC
	gf_fs_add_filter_register(fsess, ohevcdec_register(a_sess) );
#endif
	gf_fs_add_filter_register(fsess, dashdmx_register(a_sess) );
	gf_fs_add_filter_register(fsess, cenc_decrypt_register(a_sess) );
	gf_fs_add_filter_register(fsess, cenc_encrypt_register(a_sess) );
	gf_fs_add_filter_register(fsess, mp4_mux_register(a_sess) );
	gf_fs_add_filter_register(fsess, qcpdmx_register(a_sess) );
	gf_fs_add_filter_register(fsess, h263dmx_register(a_sess) );
	gf_fs_add_filter_register(fsess, mpgviddmx_register(a_sess) );
	gf_fs_add_filter_register(fsess, nhntdmx_register(a_sess) );
	gf_fs_add_filter_register(fsess, nhmldmx_register(a_sess) );
	gf_fs_add_filter_register(fsess, naludmx_register(a_sess) );
	gf_fs_add_filter_register(fsess, m2psdmx_register(a_sess) );
	gf_fs_add_filter_register(fsess, avidmx_register(a_sess) );
	gf_fs_add_filter_register(fsess, txtin_register(a_sess) );
	gf_fs_add_filter_register(fsess, ttxtdec_register(a_sess) );
	gf_fs_add_filter_register(fsess, vttdec_register(a_sess) );
	gf_fs_add_filter_register(fsess, rtpin_register(a_sess) );
	gf_fs_add_filter_register(fsess, fileout_register(a_sess) );
	gf_fs_add_filter_register(fsess, latm_mx_register(a_sess) );
	gf_fs_add_filter_register(fsess, adtsmx_register(a_sess) );

	gf_fs_add_filter_register(fsess, reframer_register(a_sess) );
	gf_fs_add_filter_register(fsess, writegen_register(a_sess) );
	gf_fs_add_filter_register(fsess, nalumx_register(a_sess) );
	gf_fs_add_filter_register(fsess, qcpmx_register(a_sess) );
	gf_fs_add_filter_register(fsess, vttmx_register(a_sess) );
	gf_fs_add_filter_register(fsess, nhntdump_register(a_sess) );
	gf_fs_add_filter_register(fsess, nhmldump_register(a_sess) );
	gf_fs_add_filter_register(fsess, vobsubdmx_register(a_sess) );
	gf_fs_add_filter_register(fsess, avimux_register(a_sess) );
	gf_fs_add_filter_register(fsess, aout_register(a_sess) );
	gf_fs_add_filter_register(fsess, m4vmx_register(a_sess) );
	gf_fs_add_filter_register(fsess, resample_register(a_sess) );
#if !defined(GPAC_CONFIG_ANDROID)
	gf_fs_add_filter_register(fsess, vout_register(a_sess) );
#endif
	gf_fs_add_filter_register(fsess, vcrop_register(a_sess) );
	gf_fs_add_filter_register(fsess, vflip_register(a_sess) );
	gf_fs_add_filter_register(fsess, rawvidreframe_register(a_sess) );
	gf_fs_add_filter_register(fsess, pcmreframe_register(a_sess) );
	gf_fs_add_filter_register(fsess, jpgenc_register(a_sess) );
	gf_fs_add_filter_register(fsess, pngenc_register(a_sess) );
	gf_fs_add_filter_register(fsess, rewind_register(a_sess) );
	gf_fs_add_filter_register(fsess, filelist_register(a_sess) );
	gf_fs_add_filter_register(fsess, tsmux_register(a_sess) );
	gf_fs_add_filter_register(fsess, dasher_register(a_sess) );
	gf_fs_add_filter_register(fsess, tileagg_register(a_sess) );
#if !defined(GPAC_CONFIG_ANDROID)
	gf_fs_add_filter_register(fsess, pipein_register(a_sess) );
	gf_fs_add_filter_register(fsess, pipeout_register(a_sess) );
#endif
	gf_fs_add_filter_register(fsess, gsfmx_register(a_sess) );
	gf_fs_add_filter_register(fsess, gsfdmx_register(a_sess) );
	gf_fs_add_filter_register(fsess, sockout_register(a_sess) );
	gf_fs_add_filter_register(fsess, av1dmx_register(a_sess) );
	gf_fs_add_filter_register(fsess, obumx_register(a_sess) );
#if !defined(GPAC_CONFIG_IOS) && !defined(GPAC_CONFIG_ANDROID)
	gf_fs_add_filter_register(fsess, nvdec_register(a_sess));
#endif
	gf_fs_add_filter_register(fsess, atscin_register(a_sess));
	gf_fs_add_filter_register(fsess, rtpout_register(a_sess));
	gf_fs_add_filter_register(fsess, rtspout_register(a_sess));

	gf_fs_add_filter_register(fsess, hevcsplit_register(a_sess));
	gf_fs_add_filter_register(fsess, hevcmerge_register(a_sess));
	gf_fs_add_filter_register(fsess, flac_dmx_register(a_sess));


	gf_fs_add_filter_register(fsess, ffdmx_register(a_sess) );
	gf_fs_add_filter_register(fsess, ffdec_register(a_sess) );
	gf_fs_add_filter_register(fsess, ffavin_register(a_sess) );
	gf_fs_add_filter_register(fsess, ffsws_register(a_sess) );
	gf_fs_add_filter_register(fsess, ffenc_register(a_sess) );
}

GF_EXPORT
void gf_fs_register_test_filters(GF_FilterSession *fsess)
{
	gf_fs_add_filter_register(fsess, ut_source_register(NULL) );
	gf_fs_add_filter_register(fsess, ut_filter_register(NULL) );
	gf_fs_add_filter_register(fsess, ut_sink_register(NULL) );
	gf_fs_add_filter_register(fsess, ut_sink2_register(NULL) );
}



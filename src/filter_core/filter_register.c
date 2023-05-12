/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2017-2023
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
#include <gpac/module.h>


const GF_FilterRegister *ut_filter_register(GF_FilterSession *session);
const GF_FilterRegister *ut_source_register(GF_FilterSession *session);
const GF_FilterRegister *ut_sink_register(GF_FilterSession *session);
const GF_FilterRegister *ut_sink2_register(GF_FilterSession *session);

#define REG_DEC(__n) const GF_FilterRegister *__n##_register(GF_FilterSession *session);
REG_DEC(ffdmx)
REG_DEC(ffdmxpid)
REG_DEC(ffdec)
REG_DEC(ffenc)
REG_DEC(ffavin)
REG_DEC(ffsws)
REG_DEC(ffmx)
REG_DEC(ffavf)
REG_DEC(ffbsf)
REG_DEC(inspect)
REG_DEC(probe)
REG_DEC(compositor)
REG_DEC(mp4dmx)
REG_DEC(bifsdec)
REG_DEC(odfdec)
REG_DEC(fin)
REG_DEC(btplay)
REG_DEC(httpin)
REG_DEC(svgplay)
REG_DEC(rfimg)
REG_DEC(imgdec)
REG_DEC(rfadts)
REG_DEC(rflatm)
REG_DEC(rfmp3)
REG_DEC(faad)
REG_DEC(maddec)
REG_DEC(xviddec)
REG_DEC(j2kdec)
REG_DEC(rfac3)
REG_DEC(a52dec)
REG_DEC(rfamr)
REG_DEC(oggdmx)
REG_DEC(vorbisdec)
REG_DEC(theoradec)
REG_DEC(m2tsdmx)
REG_DEC(sockin)
REG_DEC(dvbin)
REG_DEC(vtbdec)
REG_DEC(lsrdec)
REG_DEC(safdmx)
REG_DEC(osvcdec)
#ifdef GPAC_OPENHEVC_STATIC
REG_DEC(ohevcdec)
#endif
REG_DEC(dashin)
REG_DEC(cdcrypt)
REG_DEC(cecrypt)
REG_DEC(mp4mx)
REG_DEC(rfqcp)
REG_DEC(rfh263)
REG_DEC(rfmpgvid)
REG_DEC(nhntr)
REG_DEC(nhmlr)
REG_DEC(rfnalu)
REG_DEC(m2psdmx)
REG_DEC(avidmx)
REG_DEC(txtin)
REG_DEC(ttxtdec)
REG_DEC(vttdec)
REG_DEC(ttmldec)
REG_DEC(rtpin)
REG_DEC(fout)
REG_DEC(ufadts)
REG_DEC(uflatm)
REG_DEC(reframer)
REG_DEC(writegen)
REG_DEC(ufnalu)
REG_DEC(writeqcp)
REG_DEC(ufvtt)
REG_DEC(nhntw)
REG_DEC(nhmlw)
REG_DEC(vobsubdmx)
REG_DEC(avimx)
REG_DEC(aout)
REG_DEC(ufm4v)
REG_DEC(ufvc1)
REG_DEC(resample)
#if !defined(GPAC_CONFIG_ANDROID)
REG_DEC(vout)
#endif
REG_DEC(vcrop)
REG_DEC(vflip)
REG_DEC(rfrawvid)
REG_DEC(rfpcm)
REG_DEC(jpgenc)
REG_DEC(pngenc)
REG_DEC(rewind)
REG_DEC(flist)
REG_DEC(m2tsmx)
REG_DEC(dasher)
REG_DEC(tileagg)
REG_DEC(tilesplit)

#if !defined(GPAC_CONFIG_ANDROID)
REG_DEC(pin)
REG_DEC(pout)
#endif
REG_DEC(gsfmx)
REG_DEC(gsfdmx)
REG_DEC(sockout)
REG_DEC(rfav1)
REG_DEC(ufobu)
#if !defined(GPAC_CONFIG_IOS) && !defined(GPAC_CONFIG_ANDROID)
REG_DEC(nvdec)
#endif
REG_DEC(routein)
REG_DEC(rtpout)
REG_DEC(rtspout)
REG_DEC(hevcsplit)
REG_DEC(hevcmerge)

REG_DEC(jsf)
REG_DEC(tssplit)
REG_DEC(httpout)
REG_DEC(uncvdec)

#if !defined(GPAC_CONFIG_IOS) && !defined(GPAC_CONFIG_ANDROID) && !defined(GPAC_HAVE_DTAPI) && !defined(WIN32) 
REG_DEC(dtout)
#endif

#if !defined(GPAC_CONFIG_IOS)
REG_DEC(mcdec)
#endif

#if defined(GPAC_CONFIG_EMSCRIPTEN)
REG_DEC(wcdec)
REG_DEC(wcenc)
REG_DEC(webgrab)
#endif


REG_DEC(rfflac)
REG_DEC(rfprores)
REG_DEC(bsrw)
REG_DEC(bssplit)
REG_DEC(bsagg)
REG_DEC(rfmhas)
REG_DEC(ufmhas)
REG_DEC(routeout)
REG_DEC(rftruehd)
REG_DEC(cryptin)
REG_DEC(cryptout)
REG_DEC(restamp)
REG_DEC(oggmx)
REG_DEC(vtt2tx3g)
REG_DEC(rfsrt)
REG_DEC(ufttxt)
REG_DEC(tx3g2srt)
REG_DEC(tx3g2vtt)
REG_DEC(tx3g2ttml)
REG_DEC(ttml2vtt)
REG_DEC(ttml2srt)
REG_DEC(unframer)
REG_DEC(writeuf)
REG_DEC(ghidmx)
REG_DEC(evgs)

typedef const GF_FilterRegister *(*filter_reg_fun)(GF_FilterSession *session);

typedef struct
{
	const char *name;
	filter_reg_fun fun;
} BuiltinReg;

#define REG_IT(__n) { #__n , __n##_register }

BuiltinReg BuiltinFilters [] = {
	REG_IT(inspect),
	REG_IT(probe),
	REG_IT(compositor),
	REG_IT(mp4dmx),
	REG_IT(bifsdec),
	REG_IT(odfdec),
	REG_IT(fin),
	REG_IT(btplay),
	REG_IT(httpin),
	REG_IT(svgplay),
	REG_IT(rfimg),
	REG_IT(imgdec),
	REG_IT(rfadts),
	REG_IT(rflatm),
	REG_IT(rfmp3),
	REG_IT(faad),
	REG_IT(maddec),
	REG_IT(xviddec),
	REG_IT(j2kdec),
	REG_IT(rfac3),
	REG_IT(a52dec),
	REG_IT(rfamr),
	REG_IT(oggdmx),
	REG_IT(vorbisdec),
	REG_IT(theoradec),
	REG_IT(m2tsdmx),
	REG_IT(sockin),
	REG_IT(dvbin),
	REG_IT(osvcdec),
	REG_IT(vtbdec),
#if !defined(GPAC_CONFIG_IOS)
	REG_IT(mcdec),
#endif
	REG_IT(lsrdec),
	REG_IT(safdmx),
#ifdef GPAC_OPENHEVC_STATIC
	REG_IT(ohevcdec),
#endif
	REG_IT(dashin),
	REG_IT(cdcrypt),
	REG_IT(cecrypt),
	REG_IT(mp4mx),
	REG_IT(rfqcp),
	REG_IT(rfh263),
	REG_IT(rfmpgvid),
	REG_IT(nhntr),
	REG_IT(nhmlr),
	REG_IT(rfnalu),
	REG_IT(m2psdmx),
	REG_IT(avidmx),
	REG_IT(txtin),
	REG_IT(ttxtdec),
	REG_IT(vttdec),
	REG_IT(ttmldec),
	REG_IT(rtpin),
	REG_IT(fout),
	REG_IT(uflatm),
	REG_IT(ufadts),
	REG_IT(ufmhas),
	REG_IT(reframer),
	REG_IT(writegen),
	REG_IT(ufnalu),
	REG_IT(writeqcp),
	REG_IT(ufvtt),
	REG_IT(nhntw),
	REG_IT(nhmlw),
	REG_IT(vobsubdmx),
	REG_IT(avimx),
	REG_IT(aout),
	REG_IT(ufm4v),
	REG_IT(ufvc1),
	REG_IT(resample),
#if !defined(GPAC_CONFIG_ANDROID)
	REG_IT(vout),
#endif
	REG_IT(vcrop),
	REG_IT(vflip),
	REG_IT(rfrawvid),
	REG_IT(rfpcm),
	REG_IT(jpgenc),
	REG_IT(pngenc),
	REG_IT(rewind),
	REG_IT(flist),
	REG_IT(m2tsmx),
	REG_IT(dasher),
	REG_IT(tileagg),
	REG_IT(tilesplit),
#if !defined(GPAC_CONFIG_ANDROID)
	REG_IT(pin),
	REG_IT(pout),
#endif
	REG_IT(gsfmx),
	REG_IT(gsfdmx),
	REG_IT(sockout),
	REG_IT(rfav1),
	REG_IT(ufobu),
#if !defined(GPAC_CONFIG_IOS) && !defined(GPAC_CONFIG_ANDROID)
	REG_IT(nvdec),
#endif
	REG_IT(routein),
	REG_IT(rtpout),
	REG_IT(rtspout),
	REG_IT(httpout),
	REG_IT(hevcsplit),
	REG_IT(hevcmerge),
	REG_IT(rfflac),
	REG_IT(rfmhas),
	REG_IT(rfprores),
	REG_IT(tssplit),
	REG_IT(bsrw),
	REG_IT(bssplit),
	REG_IT(bsagg),
	REG_IT(ufttxt),
	REG_IT(tx3g2srt),
	REG_IT(tx3g2vtt),
	REG_IT(tx3g2ttml),
	REG_IT(vtt2tx3g),
	REG_IT(rfsrt),
	REG_IT(ttml2vtt),
	REG_IT(ttml2srt),

	REG_IT(ffdmx),
	REG_IT(ffdmxpid),
	REG_IT(ffdec),
	REG_IT(ffavin),
	REG_IT(ffsws),
	REG_IT(ffenc),
	REG_IT(ffmx),
	REG_IT(ffavf),
	REG_IT(ffbsf),
	REG_IT(ffbsf),

	REG_IT(jsf),
	REG_IT(routeout),
	REG_IT(rftruehd),
	REG_IT(cryptin),
	REG_IT(cryptout),
	REG_IT(restamp),
	REG_IT(oggmx),
	REG_IT(unframer),
	REG_IT(writeuf),
	REG_IT(uncvdec),
	REG_IT(ghidmx),
	REG_IT(evgs),

#if defined(GPAC_CONFIG_EMSCRIPTEN)
	REG_IT(wcdec),
	REG_IT(wcenc),
	REG_IT(webgrab),
#endif
#if !defined(GPAC_CONFIG_IOS) && !defined(GPAC_CONFIG_ANDROID) && !defined(GPAC_HAVE_DTAPI) && !defined(WIN32)
	REG_IT(dtout),
#endif
};


void gf_fs_reg_all(GF_FilterSession *fsess, GF_FilterSession *a_sess)
{
	Bool is_whitelist = GF_FALSE;
	GF_PropertyValue _bl;
	u32 nb_blacklist=0;
	char **bls=NULL;

	if (fsess->blacklist) {
		const char *blacklist = fsess->blacklist;
		if (blacklist[0]=='-') {
			blacklist++;
			is_whitelist = GF_TRUE;
		}
		_bl = gf_props_parse_value(GF_PROP_STRING_LIST, "blacklist", blacklist, NULL, fsess->sep_list);
		nb_blacklist = _bl.value.string_list.nb_items;
		bls = _bl.value.string_list.vals;
	}

	u32 i, j, count = GF_ARRAY_LENGTH(BuiltinFilters);
	for (i=0; i<count; i++) {
		const char *reg_name = BuiltinFilters[i].name;

		if (nb_blacklist) {
			Bool match = GF_FALSE;
			for (j=0; j<nb_blacklist; j++) {
				if (!strcmp(bls[j], reg_name)) {
					match = GF_TRUE;
					break;
				}
			}
			if (is_whitelist) match = !match;
			if (match) continue;
		}

		const GF_FilterRegister *freg = BuiltinFilters[i].fun(a_sess);
		if (!freg) continue;
		assert( !strcmp(freg->name, BuiltinFilters[i].name));
		gf_fs_add_filter_register(fsess, freg);
	}

	//load external modules
	if (!gf_opts_get_bool("core", "no-dynf")) {
		count = gf_modules_count();
		for (i=0; i<count; i++) {
			GF_FilterRegister *freg = (GF_FilterRegister *) gf_modules_load_filter(i, a_sess);
			if (!freg) continue;

			if (nb_blacklist) {
				Bool match = GF_FALSE;
				for (j=0; j<nb_blacklist; j++) {
					if (!strcmp(bls[j], freg->name)) {
						match = GF_TRUE;
						break;
					}
				}
				if (is_whitelist) match = !match;
				if (match) {
					if (freg->register_free) freg->register_free(fsess, (GF_FilterRegister *)freg);
					continue;
				}
			}
			gf_fs_add_filter_register(fsess, freg);
		}
	}

	if (nb_blacklist) {
		gf_props_reset_single(&_bl);
	}
}

GF_EXPORT
void gf_fs_register_test_filters(GF_FilterSession *fsess)
{
	gf_fs_add_filter_register(fsess, ut_source_register(NULL) );
	gf_fs_add_filter_register(fsess, ut_filter_register(NULL) );
	gf_fs_add_filter_register(fsess, ut_sink_register(NULL) );
	gf_fs_add_filter_register(fsess, ut_sink2_register(NULL) );
}



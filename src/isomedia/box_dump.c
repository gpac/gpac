/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre 
 *			Copyright (c) Telecom ParisTech 2000-2012
 *					All rights reserved
 *
 *  This file is part of GPAC / ISO Media File Format sub-project
 *
 *  GPAC is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU Lesser General Public License as published by
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

#include <gpac/internal/isomedia_dev.h>
#include <gpac/utf.h>
#include <gpac/network.h>
#include <time.h>

#ifndef GPAC_DISABLE_ISOM_DUMP

static GF_Err apple_tag_dump(GF_Box *a, FILE * trace);

void NullBoxErr(FILE * trace)
{
	fprintf(trace, "<!--ERROR: NULL Box Found-->\n");
}

void BadTopBoxErr(GF_Box *a, FILE * trace)
{
	fprintf(trace, "<!--ERROR: Invalid Top-level Box Found (\"%s\")-->\n", gf_4cc_to_str(a->type));
}

static void DumpData(FILE *trace, char *data, u32 dataLength)
{
	u32 i;
	fprintf(trace, "data:application/octet-string,");
	for (i=0; i<dataLength; i++) {
		fprintf(trace, "%%");
		fprintf(trace, "%02X", (unsigned char) data[i]);
	}
}

static void DumpDataHex(FILE *trace, char *data, u32 dataLength)
{
	u32 i;
	fprintf(trace, "0x");
	for (i=0; i<dataLength; i++) {
		fprintf(trace, "%02X", (unsigned char) data[i]);
	}
}

GF_Err DumpBox(GF_Box *a, FILE * trace)
{
	if (a->size > 0xFFFFFFFF) {
		fprintf(trace, "<BoxInfo LargeSize=\""LLD"\" ", LLD_CAST a->size);
	} else {
		fprintf(trace, "<BoxInfo Size=\"%d\" ", (u32) a->size);
	}
	if (a->type == GF_ISOM_BOX_TYPE_UUID) {
		u32 i;
		fprintf(trace, "UUID=\"{");
		for (i=0; i<16; i++) {
			fprintf(trace, "%02X", (unsigned char) ((GF_UUIDBox*)a)->uuid[i]);
			if ((i<15) && (i%4)==3) fprintf(trace, "-");
		}
		fprintf(trace, "}\"/>\n");
	} else {
		fprintf(trace, "Type=\"%s\"/>\n", gf_4cc_to_str(a->type));
	}
	return GF_OK;
}

GF_Err gf_box_dump(void *ptr, FILE * trace)
{
	GF_Box *a = (GF_Box *) ptr;

	if (!a) {
		NullBoxErr(trace);
		return GF_OK;
	}

	switch (a->type) {
	case GF_ISOM_BOX_TYPE_REFT:
		return reftype_dump(a, trace);
	case GF_ISOM_BOX_TYPE_FREE:
	case GF_ISOM_BOX_TYPE_SKIP:
		return free_dump(a, trace);
	case GF_ISOM_BOX_TYPE_MDAT:
		return mdat_dump(a, trace);
	case GF_ISOM_BOX_TYPE_MOOV:
		return moov_dump(a, trace);
	case GF_ISOM_BOX_TYPE_MVHD:
		return mvhd_dump(a, trace);
	case GF_ISOM_BOX_TYPE_MDHD:
		return mdhd_dump(a, trace);
	case GF_ISOM_BOX_TYPE_VMHD:
		return vmhd_dump(a, trace);
	case GF_ISOM_BOX_TYPE_SMHD:
		return smhd_dump(a, trace);
	case GF_ISOM_BOX_TYPE_HMHD:
		return hmhd_dump(a, trace);
	//the same box is used for all MPEG4 systems streams
	case GF_ISOM_BOX_TYPE_ODHD:
	case GF_ISOM_BOX_TYPE_CRHD:
	case GF_ISOM_BOX_TYPE_SDHD:
	case GF_ISOM_BOX_TYPE_NMHD:
		return nmhd_dump(a, trace);
	case GF_ISOM_BOX_TYPE_STBL:
		return stbl_dump(a, trace);
	case GF_ISOM_BOX_TYPE_DINF:
		return dinf_dump(a, trace);
	case GF_ISOM_BOX_TYPE_URL:
		return url_dump(a, trace);
	case GF_ISOM_BOX_TYPE_URN:
		return urn_dump(a, trace);
	case GF_ISOM_BOX_TYPE_CPRT:
		return cprt_dump(a, trace);
	case GF_ISOM_BOX_TYPE_HDLR:
		return hdlr_dump(a, trace);
	case GF_ISOM_BOX_TYPE_IODS:
		return iods_dump(a, trace);
	case GF_ISOM_BOX_TYPE_TRAK:
		return trak_dump(a, trace);
	case GF_ISOM_BOX_TYPE_MP4S:
		return mp4s_dump(a, trace);
	case GF_ISOM_BOX_TYPE_MP4V:
		return mp4v_dump(a, trace);
	case GF_ISOM_BOX_TYPE_MP4A:
		return mp4a_dump(a, trace);
	case GF_ISOM_BOX_TYPE_GNRM:
		return gnrm_dump(a, trace);
	case GF_ISOM_BOX_TYPE_GNRV:
		return gnrv_dump(a, trace);
	case GF_ISOM_BOX_TYPE_GNRA:
		return gnra_dump(a, trace);
	case GF_ISOM_BOX_TYPE_EDTS:
		return edts_dump(a, trace);
	case GF_ISOM_BOX_TYPE_UDTA:
		return udta_dump(a, trace);
	case GF_ISOM_BOX_TYPE_DREF:
		return dref_dump(a, trace);
	case GF_ISOM_BOX_TYPE_STSD:
		return stsd_dump(a, trace);
	case GF_ISOM_BOX_TYPE_STTS:
		return stts_dump(a, trace);
	case GF_ISOM_BOX_TYPE_CTTS:
		return ctts_dump(a, trace);
	case GF_ISOM_BOX_TYPE_CSLG:
		return cslg_dump(a, trace);
	case GF_ISOM_BOX_TYPE_STSH:
		return stsh_dump(a, trace);
	case GF_ISOM_BOX_TYPE_ELST:
		return elst_dump(a, trace);
	case GF_ISOM_BOX_TYPE_STSC:
		return stsc_dump(a, trace);
	case GF_ISOM_BOX_TYPE_STZ2:
	case GF_ISOM_BOX_TYPE_STSZ:
		return stsz_dump(a, trace);
	case GF_ISOM_BOX_TYPE_STCO:
		return stco_dump(a, trace);
	case GF_ISOM_BOX_TYPE_STSS:
		return stss_dump(a, trace);
	case GF_ISOM_BOX_TYPE_STDP:
		return stdp_dump(a, trace);
	case GF_ISOM_BOX_TYPE_SDTP:
		return sdtp_dump(a, trace);
	case GF_ISOM_BOX_TYPE_CO64:
		return co64_dump(a, trace);
	case GF_ISOM_BOX_TYPE_ESDS:
		return esds_dump(a, trace);
	case GF_ISOM_BOX_TYPE_MINF:
		return minf_dump(a, trace);
	case GF_ISOM_BOX_TYPE_TKHD:
		return tkhd_dump(a, trace);
	case GF_ISOM_BOX_TYPE_TREF:
		return tref_dump(a, trace);
	case GF_ISOM_BOX_TYPE_MDIA:
		return mdia_dump(a, trace);
	case GF_ISOM_BOX_TYPE_CHPL:
		return chpl_dump(a, trace);
	case GF_ISOM_BOX_TYPE_PDIN:
		return dpin_dump(a, trace);
	case GF_ISOM_BOX_TYPE_SBGP: 
		return sbgp_dump(a, trace);
	case GF_ISOM_BOX_TYPE_SGPD: 
		return sgpd_dump(a, trace);
	case GF_ISOM_BOX_TYPE_SAIZ: 
		return saiz_dump(a, trace);
	case GF_ISOM_BOX_TYPE_SAIO: 
		return saio_dump(a, trace);

	case GF_ISOM_BOX_TYPE_RTP_STSD:
		return ghnt_dump(a, trace);
	case GF_ISOM_BOX_TYPE_RTPO:
		return rtpo_dump(a, trace);
	case GF_ISOM_BOX_TYPE_HNTI:
		return hnti_dump(a, trace);
	case GF_ISOM_BOX_TYPE_SDP:
		return sdp_dump(a, trace);
	case GF_ISOM_BOX_TYPE_HINF:
		return hinf_dump(a, trace);
	case GF_ISOM_BOX_TYPE_RELY:
		return rely_dump(a, trace);
	case GF_ISOM_BOX_TYPE_TIMS:
		return tims_dump(a, trace);
	case GF_ISOM_BOX_TYPE_TSRO:
		return tsro_dump(a, trace);
	case GF_ISOM_BOX_TYPE_SNRO:
		return snro_dump(a, trace);
	case GF_ISOM_BOX_TYPE_TRPY:
		return trpy_dump(a, trace);
	case GF_ISOM_BOX_TYPE_NUMP:
		return nump_dump(a, trace);
	case GF_ISOM_BOX_TYPE_TOTL:
		return totl_dump(a, trace);
	case GF_ISOM_BOX_TYPE_NPCK:
		return npck_dump(a, trace);
	case GF_ISOM_BOX_TYPE_TPYL:
		return tpyl_dump(a, trace);
	case GF_ISOM_BOX_TYPE_TPAY:
		return tpay_dump(a, trace);
	case GF_ISOM_BOX_TYPE_MAXR:
		return maxr_dump(a, trace);
	case GF_ISOM_BOX_TYPE_DMED:
		return dmed_dump(a, trace);
	case GF_ISOM_BOX_TYPE_DIMM:
		return dimm_dump(a, trace);
	case GF_ISOM_BOX_TYPE_DREP:
		return drep_dump(a, trace);
	case GF_ISOM_BOX_TYPE_TMIN:
		return tmin_dump(a, trace);
	case GF_ISOM_BOX_TYPE_TMAX:
		return tmax_dump(a, trace);
	case GF_ISOM_BOX_TYPE_PMAX:
		return pmax_dump(a, trace);
	case GF_ISOM_BOX_TYPE_DMAX:
		return dmax_dump(a, trace);
	case GF_ISOM_BOX_TYPE_PAYT:
		return payt_dump(a, trace);
	case GF_ISOM_BOX_TYPE_NAME:
		return name_dump(a, trace);

	case GF_ISOM_BOX_TYPE_FTYP: 
	case GF_ISOM_BOX_TYPE_STYP: 
        return ftyp_dump(a, trace);
	case GF_ISOM_BOX_TYPE_PADB:
		return padb_dump(a, trace);

#ifndef	GPAC_DISABLE_ISOM_FRAGMENTS
	case GF_ISOM_BOX_TYPE_MVEX:
		return mvex_dump(a, trace);
	case GF_ISOM_BOX_TYPE_MEHD:
		return mehd_dump(a, trace);
	case GF_ISOM_BOX_TYPE_TREX:
		return trex_dump(a, trace);
	case GF_ISOM_BOX_TYPE_MOOF:
		return moof_dump(a, trace);
	case GF_ISOM_BOX_TYPE_MFHD:
		return mfhd_dump(a, trace);
	case GF_ISOM_BOX_TYPE_TRAF:
		return traf_dump(a, trace);
	case GF_ISOM_BOX_TYPE_TFHD:
		return tfhd_dump(a, trace);
	case GF_ISOM_BOX_TYPE_TRUN:
		return trun_dump(a, trace);
	case GF_ISOM_BOX_TYPE_TFDT:
		return tfdt_dump(a, trace);		
#endif
	
	case GF_ISOM_BOX_TYPE_SUBS:
		return subs_dump(a, trace);
	case GF_ISOM_BOX_TYPE_RVCC:
		return rvcc_dump(a, trace);

	case GF_ISOM_BOX_TYPE_VOID:
		return void_dump(a, trace);
	case GF_ISOM_BOX_TYPE_STSF:
		return stsf_dump(a, trace);

	case GF_ISOM_SUBTYPE_3GP_AMR:
	case GF_ISOM_SUBTYPE_3GP_AMR_WB:
	case GF_ISOM_SUBTYPE_3GP_QCELP:
	case GF_ISOM_SUBTYPE_3GP_EVRC:
	case GF_ISOM_SUBTYPE_3GP_SMV:
		return gppa_dump(a, trace);
	case GF_ISOM_SUBTYPE_3GP_H263:
		return gppv_dump(a, trace);
	case GF_ISOM_BOX_TYPE_DAMR:
	case GF_ISOM_BOX_TYPE_DEVC:
	case GF_ISOM_BOX_TYPE_DQCP:
	case GF_ISOM_BOX_TYPE_DSMV:
	case GF_ISOM_BOX_TYPE_D263:
		return gppc_dump(a, trace);

	case GF_ISOM_BOX_TYPE_AVCC: 
	case GF_ISOM_BOX_TYPE_SVCC: 
		return avcc_dump(a, trace);
	case GF_ISOM_BOX_TYPE_HVCC: 
	case GF_ISOM_BOX_TYPE_SHCC: 
		return hvcc_dump(a, trace);
	case GF_ISOM_BOX_TYPE_BTRT:
		return btrt_dump(a, trace);
	case GF_ISOM_BOX_TYPE_M4DS:
		return m4ds_dump(a, trace);
	case GF_ISOM_BOX_TYPE_AVC1: 
	case GF_ISOM_BOX_TYPE_AVC2: 
	case GF_ISOM_BOX_TYPE_AVC3:
	case GF_ISOM_BOX_TYPE_AVC4:
	case GF_ISOM_BOX_TYPE_SVC1: 
	case GF_ISOM_BOX_TYPE_HVC1: 
	case GF_ISOM_BOX_TYPE_HEV1: 
	case GF_ISOM_BOX_TYPE_HVC2:
	case GF_ISOM_BOX_TYPE_HEV2:
	case GF_ISOM_BOX_TYPE_SHC1:
	case GF_ISOM_BOX_TYPE_SHV1:
		return mp4v_dump(a, trace);
	case GF_ISOM_BOX_TYPE_PASP:
		return pasp_dump(a, trace);

	case GF_ISOM_BOX_TYPE_FTAB:
		return ftab_dump(a, trace);
	case GF_ISOM_BOX_TYPE_TX3G: 
		return tx3g_dump(a, trace);
	case GF_ISOM_BOX_TYPE_TEXT:
		return text_dump(a, trace);
	case GF_ISOM_BOX_TYPE_STYL:
		return styl_dump(a, trace);
	case GF_ISOM_BOX_TYPE_HLIT:
		return hlit_dump(a, trace);
	case GF_ISOM_BOX_TYPE_HCLR:
		return hclr_dump(a, trace);
	case GF_ISOM_BOX_TYPE_KROK:
		return krok_dump(a, trace);
	case GF_ISOM_BOX_TYPE_DLAY:
		return dlay_dump(a, trace);
	case GF_ISOM_BOX_TYPE_HREF:
		return href_dump(a, trace);
	case GF_ISOM_BOX_TYPE_TBOX:
		return tbox_dump(a, trace);
	case GF_ISOM_BOX_TYPE_BLNK:
		return blnk_dump(a, trace);
	case GF_ISOM_BOX_TYPE_TWRP:
		return twrp_dump(a, trace);

	case GF_ISOM_BOX_TYPE_PSSH:
		return pssh_dump(a, trace);
	case GF_ISOM_BOX_TYPE_TENC:
		return tenc_dump(a, trace);

	/* ISMA 1.0 Encryption and Authentication V 1.0 */
	case GF_ISOM_BOX_TYPE_IKMS:
		return iKMS_dump(a, trace);
	case GF_ISOM_BOX_TYPE_ISFM:
		return iSFM_dump(a, trace);

	/*MPEG-21 extensions*/
	case GF_ISOM_BOX_TYPE_META:
		return meta_dump(a, trace);
	case GF_ISOM_BOX_TYPE_XML:
		return xml_dump(a, trace);
	case GF_ISOM_BOX_TYPE_BXML:
		return bxml_dump(a, trace);
	case GF_ISOM_BOX_TYPE_ILOC:
		return iloc_dump(a, trace);
	case GF_ISOM_BOX_TYPE_PITM:
		return pitm_dump(a, trace);
	case GF_ISOM_BOX_TYPE_IPRO:
		return ipro_dump(a, trace);
	case GF_ISOM_BOX_TYPE_INFE:
		return infe_dump(a, trace);
	case GF_ISOM_BOX_TYPE_IINF:
		return iinf_dump(a, trace);
	case GF_ISOM_BOX_TYPE_SINF:
		return sinf_dump(a, trace);
	case GF_ISOM_BOX_TYPE_FRMA:
		return frma_dump(a, trace);
	case GF_ISOM_BOX_TYPE_SCHM:
		return schm_dump(a, trace);
	case GF_ISOM_BOX_TYPE_SCHI:
		return schi_dump(a, trace);
	case GF_ISOM_BOX_TYPE_ENCA:
		return mp4a_dump(a, trace);
	case GF_ISOM_BOX_TYPE_ENCV:
		return mp4v_dump(a, trace);
	case GF_ISOM_BOX_TYPE_ENCS:
		return mp4s_dump(a, trace);
	case GF_ISOM_BOX_TYPE_PRFT:
		return prft_dump(a, trace);

	case GF_ISOM_BOX_TYPE_0xA9NAM:
	case GF_ISOM_BOX_TYPE_0xA9CMT:
	case GF_ISOM_BOX_TYPE_0xA9DAY:
	case GF_ISOM_BOX_TYPE_0xA9ART:
	case GF_ISOM_BOX_TYPE_0xA9TRK:
	case GF_ISOM_BOX_TYPE_0xA9ALB:
	case GF_ISOM_BOX_TYPE_0xA9COM:
	case GF_ISOM_BOX_TYPE_0xA9WRT:
	case GF_ISOM_BOX_TYPE_0xA9TOO:
	case GF_ISOM_BOX_TYPE_0xA9CPY:
	case GF_ISOM_BOX_TYPE_0xA9DES:
	case GF_ISOM_BOX_TYPE_0xA9GEN:
	case GF_ISOM_BOX_TYPE_0xA9GRP:
	case GF_ISOM_BOX_TYPE_GNRE:
	case GF_ISOM_BOX_TYPE_DISK:
	case GF_ISOM_BOX_TYPE_TRKN:
	case GF_ISOM_BOX_TYPE_TMPO:
	case GF_ISOM_BOX_TYPE_CPIL:
	case GF_ISOM_BOX_TYPE_COVR:
	case GF_ISOM_BOX_TYPE_iTunesSpecificInfo:
		return apple_tag_dump(a, trace);
#ifndef GPAC_DISABLE_ISOM_ADOBE
	/*Adobe extensions*/
	case GF_ISOM_BOX_TYPE_ABST:
		return abst_dump(a, trace);
	case GF_ISOM_BOX_TYPE_AFRA:
		return afra_dump(a, trace);
	case GF_ISOM_BOX_TYPE_ASRT:
		return asrt_dump(a, trace);
	case GF_ISOM_BOX_TYPE_AFRT:
		return afrt_dump(a, trace);
#endif
	/*Apple extensions*/
	case GF_ISOM_BOX_TYPE_ILST:
		return ilst_dump(a, trace);

	case GF_ISOM_BOX_TYPE_OHDR:
		return ohdr_dump(a, trace);
	case GF_ISOM_BOX_TYPE_GRPI:
		return grpi_dump(a, trace);
	case GF_ISOM_BOX_TYPE_MDRI:
		return mdri_dump(a, trace);
	case GF_ISOM_BOX_TYPE_ODTT:
		return odtt_dump(a, trace);
	case GF_ISOM_BOX_TYPE_ODRB:
		return odrb_dump(a, trace);
	case GF_ISOM_BOX_TYPE_ODKM:
		return odkm_dump(a, trace);
	case GF_ISOM_BOX_TYPE_ODAF:
		return iSFM_dump(a, trace);

	case GF_ISOM_BOX_TYPE_TSEL:
		return tsel_dump(a, trace);
	case GF_ISOM_BOX_TYPE_METX:
		return metx_dump(a, trace);
	case GF_ISOM_BOX_TYPE_METT:
		return metx_dump(a, trace);

	case GF_ISOM_BOX_TYPE_DIMS:
		return dims_dump(a, trace);
	case GF_ISOM_BOX_TYPE_DIMC:
		return dimC_dump(a, trace);
	case GF_ISOM_BOX_TYPE_DIST:
		return diST_dump(a, trace);

	case GF_ISOM_BOX_TYPE_AC3:
		return ac3_dump(a, trace);
	case GF_ISOM_BOX_TYPE_DAC3:
		return dac3_dump(a, trace);

	case GF_ISOM_BOX_TYPE_LSR1:
		return lsr1_dump(a, trace);
	case GF_ISOM_BOX_TYPE_LSRC:
		return lsrc_dump(a, trace);
	case GF_ISOM_BOX_TYPE_SIDX:
		return sidx_dump(a, trace);
	case GF_ISOM_BOX_TYPE_PCRB:
		return pcrb_dump(a, trace);

	case GF_ISOM_BOX_TYPE_SENC:
		return senc_dump(a, trace);

	case GF_ISOM_BOX_TYPE_UUID:
		switch ( ((GF_UnknownUUIDBox *)a)->internal_4cc) {
		case GF_ISOM_BOX_UUID_TENC: return piff_tenc_dump(a, trace);
		case GF_ISOM_BOX_UUID_PSEC: return piff_psec_dump(a, trace);
		case GF_ISOM_BOX_UUID_PSSH: return piff_pssh_dump(a, trace);
		case GF_ISOM_BOX_UUID_TFRF: 
		case GF_ISOM_BOX_UUID_TFXD: 
		default:
			return defa_dump(a, trace);
		}
#ifndef GPAC_DISABLE_TTXT
	case GF_ISOM_BOX_TYPE_STSE:
		return stse_dump(a, trace);

	case GF_ISOM_BOX_TYPE_STTC:
	case GF_ISOM_BOX_TYPE_VTTC:
	case GF_ISOM_BOX_TYPE_CTIM:
	case GF_ISOM_BOX_TYPE_IDEN:
	case GF_ISOM_BOX_TYPE_STTG:
	case GF_ISOM_BOX_TYPE_PAYL:
		return boxstring_dump(a, trace);
	case GF_ISOM_BOX_TYPE_VTCU:
		return vtcu_dump(a, trace);
	case GF_ISOM_BOX_TYPE_VTTE:
		return vtte_dump(a, trace);
	case GF_ISOM_BOX_TYPE_WVTT:
		return wvtt_dump(a, trace);
#endif
			
	default: 
		return defa_dump(a, trace);
	}
}

GF_Err gf_box_array_dump(GF_List *list, FILE * trace)
{
	u32 i;
	GF_Box *a;
	if (!list) return GF_OK;
	i=0;
	while ((a = (GF_Box *)gf_list_enum(list, &i))) {
		gf_box_dump(a, trace);
	}
	return GF_OK;
}

void gf_box_dump_done(char *name, GF_Box *ptr, FILE *trace)
{
	if (ptr && ptr->other_boxes) {
		gf_box_array_dump(ptr->other_boxes, trace);
	}
	if (name) 
		fprintf(trace, "</%s>\n", name);
}


GF_EXPORT
GF_Err gf_isom_dump(GF_ISOFile *mov, FILE * trace)
{
	u32 i;
	GF_Box *box;
	if (!mov || !trace) return GF_BAD_PARAM;

	fprintf(trace, "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n");
	fprintf(trace, "<!--MP4Box dump trace-->\n");

	fprintf(trace, "<IsoMediaFile Name=\"%s\">\n", mov->fileName);

	i=0;
	while ((box = (GF_Box *)gf_list_enum(mov->TopBoxes, &i))) {
		switch (box->type) {
		case GF_ISOM_BOX_TYPE_FTYP:
		case GF_ISOM_BOX_TYPE_MOOV:
		case GF_ISOM_BOX_TYPE_MDAT:
		case GF_ISOM_BOX_TYPE_FREE:
		case GF_ISOM_BOX_TYPE_META:
		case GF_ISOM_BOX_TYPE_SKIP:
#ifndef	GPAC_DISABLE_ISOM_FRAGMENTS
		case GF_ISOM_BOX_TYPE_MOOF:
		case GF_ISOM_BOX_TYPE_STYP:
		case GF_ISOM_BOX_TYPE_SIDX:
		case GF_ISOM_BOX_TYPE_PCRB:
#ifndef GPAC_DISABLE_ISOM_ADOBE
		/*Adobe specific*/
		case GF_ISOM_BOX_TYPE_AFRA:
		case GF_ISOM_BOX_TYPE_ABST:
#endif
#endif
		case GF_ISOM_BOX_TYPE_MFRA:
		case GF_ISOM_BOX_TYPE_PRFT:
			break;

		default:
			BadTopBoxErr(box, trace);
			break;
		}
		gf_box_dump(box, trace);
	}
	fprintf(trace, "</IsoMediaFile>\n");
	return GF_OK;
}



GF_Err gf_full_box_dump(GF_Box *a, FILE * trace)
{
	GF_FullBox *p;
	p = (GF_FullBox *)a;
	fprintf(trace, "<FullBoxInfo Version=\"%d\" Flags=\"0x%X\"/>\n", p->version, p->flags);
	return GF_OK;
}


GF_Err reftype_dump(GF_Box *a, FILE * trace)
{
	char *s;
	u32 i;
	GF_TrackReferenceTypeBox *p;

	p = (GF_TrackReferenceTypeBox *)a;
	p->type = p->reference_type;

	switch (a->type) {
	case GF_ISOM_BOX_TYPE_HINT: s = "Hint"; break;
	case GF_ISOM_BOX_TYPE_DPND: s = "Stream"; break;
	case GF_ISOM_BOX_TYPE_MPOD: s = "OD"; break;
	case GF_ISOM_BOX_TYPE_SYNC: s = "Sync"; break;
	case GF_ISOM_BOX_TYPE_CHAP: s = "Chapter"; break;
	default:
		s = (char *) gf_4cc_to_str(a->type);
		break;
	}
	fprintf(trace, "<%sTrackReferenceBox Tracks=\"", s);
	for (i=0; i<p->trackIDCount; i++) fprintf(trace, " %d", p->trackIDs[i]);
	fprintf(trace, "\">\n");
	DumpBox(a, trace);

	gf_box_dump_done(NULL, a, trace);
	fprintf(trace, "</%sTrackReferenceBox>\n", s);

	p->type = GF_ISOM_BOX_TYPE_REFT;
	return GF_OK;
}

GF_Err free_dump(GF_Box *a, FILE * trace)
{
	GF_FreeSpaceBox *p;

	p = (GF_FreeSpaceBox *)a;
	fprintf(trace, "<FreeSpaceBox size=\"%d\">\n", p->dataSize);
	DumpBox(a, trace);
	gf_box_dump_done("FreeSpaceBox", a, trace);
	return GF_OK;
}

GF_Err mdat_dump(GF_Box *a, FILE * trace)
{
	GF_MediaDataBox *p;

	p = (GF_MediaDataBox *)a;
	fprintf(trace, "<MediaDataBox dataSize=\""LLD"\">\n", LLD_CAST p->dataSize);
	DumpBox(a, trace);
	gf_box_dump_done("MediaDataBox", a, trace);
	return GF_OK;
}

GF_Err moov_dump(GF_Box *a, FILE * trace)
{
	GF_MovieBox *p;
	p = (GF_MovieBox *)a;
	fprintf(trace, "<MovieBox>\n");
	DumpBox(a, trace);

	if (p->iods) gf_box_dump(p->iods, trace);
	if (p->meta) gf_box_dump(p->meta, trace);
	gf_box_dump(p->mvhd, trace);

#ifndef	GPAC_DISABLE_ISOM_FRAGMENTS
	if (p->mvex) gf_box_dump(p->mvex, trace);
#endif

	gf_box_array_dump(p->trackList, trace);
	if (p->udta) gf_box_dump(p->udta, trace);
	gf_box_dump_done("MovieBox", a, trace);
	return GF_OK;
}

GF_Err mvhd_dump(GF_Box *a, FILE * trace)
{
	GF_MovieHeaderBox *p;

	p = (GF_MovieHeaderBox *) a;

	fprintf(trace, "<MovieHeaderBox ");
	fprintf(trace, "CreationTime=\""LLD"\" ", LLD_CAST p->creationTime);
	fprintf(trace, "ModificationTime=\""LLD"\" ", LLD_CAST p->modificationTime);
	fprintf(trace, "TimeScale=\"%d\" ", p->timeScale);
	fprintf(trace, "Duration=\""LLD"\" ", LLD_CAST p->duration);
	fprintf(trace, "NextTrackID=\"%d\">\n", p->nextTrackID);

	DumpBox(a, trace);
	gf_full_box_dump(a, trace);

	gf_box_dump_done("MovieHeaderBox", a, trace);
	return GF_OK;
}

GF_Err mdhd_dump(GF_Box *a, FILE * trace)
{
	GF_MediaHeaderBox *p;

	p = (GF_MediaHeaderBox *)a;
	fprintf(trace, "<MediaHeaderBox ");

	fprintf(trace, "CreationTime=\""LLD"\" ", LLD_CAST p->creationTime);
	fprintf(trace, "ModificationTime=\""LLD"\" ", LLD_CAST p->modificationTime);
	fprintf(trace, "TimeScale=\"%d\" ", p->timeScale);
	fprintf(trace, "Duration=\""LLD"\" ", LLD_CAST p->duration);
	fprintf(trace, "LanguageCode=\"%c%c%c\">\n", p->packedLanguage[0], p->packedLanguage[1], p->packedLanguage[2]);

	DumpBox(a, trace);
	gf_full_box_dump(a, trace);
	gf_box_dump_done("MediaHeaderBox", a, trace);
	return GF_OK;
}

GF_Err vmhd_dump(GF_Box *a, FILE * trace)
{
	fprintf(trace, "<VideoMediaHeaderBox>\n");
	DumpBox(a, trace);
	gf_full_box_dump(a, trace);
	gf_box_dump_done("VideoMediaHeaderBox", a, trace);
	return GF_OK;
}

GF_Err smhd_dump(GF_Box *a, FILE * trace)
{
	fprintf(trace, "<SoundMediaHeaderBox>\n");
	DumpBox(a, trace);
	gf_full_box_dump(a, trace);
	gf_box_dump_done("SoundMediaHeaderBox", a, trace);
	return GF_OK;
}

GF_Err hmhd_dump(GF_Box *a, FILE * trace)
{
	GF_HintMediaHeaderBox *p;

	p = (GF_HintMediaHeaderBox *)a;

	fprintf(trace, "<HintMediaHeaderBox ");

	fprintf(trace, "MaximumPDUSize=\"%d\" ", p->maxPDUSize);
	fprintf(trace, "AveragePDUSize=\"%d\" ", p->avgPDUSize);
	fprintf(trace, "MaxBitRate=\"%d\" ", p->maxBitrate);
	fprintf(trace, "AverageBitRate=\"%d\">\n", p->avgBitrate);

	DumpBox(a, trace);
	gf_full_box_dump(a, trace);
	gf_box_dump_done("HintMediaHeaderBox", a, trace);
	return GF_OK;
}

GF_Err nmhd_dump(GF_Box *a, FILE * trace)
{
	fprintf(trace, "<MPEGMediaHeaderBox>\n");
	DumpBox(a, trace);
	gf_full_box_dump(a, trace);
	gf_box_dump_done("MPEGMediaHeaderBox", a, trace);
	return GF_OK;
}

GF_Err stbl_dump(GF_Box *a, FILE * trace)
{
	GF_SampleTableBox *p;
	p = (GF_SampleTableBox *)a;
	fprintf(trace, "<SampleTableBox>\n");
	DumpBox(a, trace);

	gf_box_dump(p->SampleDescription, trace);
	gf_box_dump(p->TimeToSample, trace);
	if (p->CompositionOffset) gf_box_dump(p->CompositionOffset, trace);
	if (p->CompositionToDecode) gf_box_dump(p->CompositionToDecode, trace);
	if (p->SyncSample) gf_box_dump(p->SyncSample, trace);
	if (p->ShadowSync) gf_box_dump(p->ShadowSync, trace);
	gf_box_dump(p->SampleToChunk, trace);
	gf_box_dump(p->SampleSize, trace);
	gf_box_dump(p->ChunkOffset, trace);
	if (p->DegradationPriority) gf_box_dump(p->DegradationPriority, trace);
	if (p->SampleDep) gf_box_dump(p->SampleDep, trace);
	if (p->PaddingBits) gf_box_dump(p->PaddingBits, trace);
	if (p->SubSamples) gf_box_dump(p->SubSamples, trace);
	if (p->Fragments) gf_box_dump(p->Fragments, trace);
	if (p->sampleGroupsDescription) gf_box_array_dump(p->sampleGroupsDescription, trace);
	if (p->sampleGroups) gf_box_array_dump(p->sampleGroups, trace);
	if (p->sai_sizes) {
		u32 i;
		for (i = 0; i < gf_list_count(p->sai_sizes); i++) {
			GF_SampleAuxiliaryInfoSizeBox *saiz = (GF_SampleAuxiliaryInfoSizeBox *)gf_list_get(p->sai_sizes, i);
			gf_box_dump(saiz, trace);
		}
	}

	if (p->sai_offsets) {
		u32 i;
		for (i = 0; i < gf_list_count(p->sai_offsets); i++) {
			GF_SampleAuxiliaryInfoOffsetBox *saio = (GF_SampleAuxiliaryInfoOffsetBox *)gf_list_get(p->sai_offsets, i);
			gf_box_dump(saio, trace);
		}
	}
	
	gf_box_dump_done("SampleTableBox", a, trace);
	return GF_OK;
}

GF_Err dinf_dump(GF_Box *a, FILE * trace)
{
	GF_DataInformationBox *p;
	p = (GF_DataInformationBox *)a;
	fprintf(trace, "<DataInformationBox>");
	DumpBox(a, trace);
	gf_box_dump(p->dref, trace);
	gf_box_dump_done("DataInformationBox", a, trace);
	return GF_OK;
}

GF_Err url_dump(GF_Box *a, FILE * trace)
{
	GF_DataEntryURLBox *p;

	p = (GF_DataEntryURLBox *)a;
	fprintf(trace, "<URLDataEntryBox");
	if (p->location) {
		fprintf(trace, " URL=\"%s\">\n", p->location);
	} else {
		fprintf(trace, ">\n");
		if (! (p->flags & 1) ) {
			fprintf(trace, "<!--ERROR: No location indicated-->\n");
		} else {
			fprintf(trace, "<!--Data is contained in the movie file-->\n");
		}
	}
	DumpBox(a, trace);
	gf_full_box_dump(a, trace);
	gf_box_dump_done("URLDataEntryBox", a, trace);
	return GF_OK;
}

GF_Err urn_dump(GF_Box *a, FILE * trace)
{
	GF_DataEntryURNBox *p;

	p = (GF_DataEntryURNBox *)a;
	fprintf(trace, "<URNDataEntryBox");
	if (p->nameURN) fprintf(trace, " URN=\"%s\"", p->nameURN);
	if (p->location) fprintf(trace, " URL=\"%s\"", p->location);
	fprintf(trace, ">\n");

	DumpBox(a, trace);
	gf_full_box_dump(a, trace);
	gf_box_dump_done("URNDataEntryBox", a, trace);
	return GF_OK;
}

GF_Err cprt_dump(GF_Box *a, FILE * trace)
{
	GF_CopyrightBox *p;

	p = (GF_CopyrightBox *)a;
	fprintf(trace, "<CopyrightBox LanguageCode=\"%s\" CopyrightNotice=\"%s\">\n", p->packedLanguageCode, p->notice);
	DumpBox(a, trace);
	gf_full_box_dump(a, trace);
	gf_box_dump_done("CopyrightBox", a, trace);
	return GF_OK;
}


static char *format_duration(u64 dur, u32 timescale, char *szDur)
{
	u32 h, m, s, ms;
	dur = (u32) (( ((Double) (s64) dur)/timescale)*1000);
	h = (u32) (dur / 3600000);
	dur -= h*3600000;
	m = (u32) (dur / 60000);
	dur -= m*60000;
	s = (u32) (dur/1000);
	dur -= s*1000;
	ms = (u32) (dur);
	sprintf(szDur, "%02d:%02d:%02d.%03d", h, m, s, ms);
	return szDur;
}

GF_Err chpl_dump(GF_Box *a, FILE * trace)
{
	u32 i, count;
	char szDur[20];
	GF_ChapterListBox *p = (GF_ChapterListBox *)a;
	fprintf(trace, "<ChapterListBox>\n");
	DumpBox(a, trace);
	gf_full_box_dump(a, trace);

	count = gf_list_count(p->list);
	for (i=0; i<count; i++) {
		GF_ChapterEntry *ce = (GF_ChapterEntry *)gf_list_get(p->list, i);
		fprintf(trace, "<Chapter name=\"%s\" startTime=\"%s\" />\n", ce->name, format_duration(ce->start_time, 1000*10000, szDur));
	}

	gf_box_dump_done("ChapterListBox", a, trace);
	return GF_OK;
}

GF_Err dpin_dump(GF_Box *a, FILE * trace)
{
	u32 i;
	GF_ProgressiveDownloadBox *p = (GF_ProgressiveDownloadBox *)a;
	fprintf(trace, "<ProgressiveDownloadBox>\n");
	DumpBox(a, trace);
	gf_full_box_dump(a, trace);

	for (i=0; i<p->count; i++) {
		fprintf(trace, "<DownloadInfo rate=\"%d\" estimatedTime=\"%d\" />\n", p->rates[i], p->times[i]);
	}
	gf_box_dump_done("ProgressiveDownloadBox", a, trace);
	return GF_OK;
}

GF_Err hdlr_dump(GF_Box *a, FILE * trace)
{
	GF_HandlerBox *p = (GF_HandlerBox *)a;
	if (p->nameUTF8 && (u32) p->nameUTF8[0] == strlen(p->nameUTF8+1)) {
		fprintf(trace, "<HandlerBox Type=\"%s\" Name=\"%s\" ", gf_4cc_to_str(p->handlerType), p->nameUTF8+1);
	} else {
		fprintf(trace, "<HandlerBox Type=\"%s\" Name=\"%s\" ", gf_4cc_to_str(p->handlerType), p->nameUTF8);
	}
	fprintf(trace, "reserved1=\"%d\" reserved2=\"", p->reserved1);
	DumpData(trace, (char *) p->reserved2, 12);
	fprintf(trace, "\"");

	fprintf(trace, ">\n");
	DumpBox(a, trace);
	gf_full_box_dump(a, trace);
	gf_box_dump_done("HandlerBox", a, trace);
	return GF_OK;
}

GF_Err iods_dump(GF_Box *a, FILE * trace)
{
	GF_ObjectDescriptorBox *p;

	p = (GF_ObjectDescriptorBox *)a;
	fprintf(trace, "<ObjectDescriptorBox>\n");
	DumpBox(a, trace);
	gf_full_box_dump(a, trace);

	if (p->descriptor) {
#ifndef GPAC_DISABLE_OD_DUMP
		gf_odf_dump_desc(p->descriptor, trace, 1, 1);
#else
		fprintf(trace, "<!-- Object Descriptor Dumping disabled in this build of GPAC -->\n");
#endif
	} else {
		fprintf(trace, "<!--WARNING: Object Descriptor not present-->\n");
	}
	gf_box_dump_done("ObjectDescriptorBox", a, trace);
	return GF_OK;
}

GF_Err trak_dump(GF_Box *a, FILE * trace)
{
	GF_TrackBox *p;

	p = (GF_TrackBox *)a;
	fprintf(trace, "<TrackBox>\n");
	DumpBox(a, trace);
	if (p->Header) {
		gf_box_dump(p->Header, trace);
	} else {
		fprintf(trace, "<!--INVALID FILE: Missing Track Header-->\n");
	}
	if (p->References) gf_box_dump(p->References, trace);
	if (p->meta) gf_box_dump(p->meta, trace);
	if (p->editBox) gf_box_dump(p->editBox, trace);
	if (p->Media) gf_box_dump(p->Media, trace);
	if (p->udta) gf_box_dump(p->udta, trace);
	gf_box_dump_done("TrackBox", a, trace);
	return GF_OK;
}

GF_Err mp4s_dump(GF_Box *a, FILE * trace)
{
	GF_MPEGSampleEntryBox *p;

	p = (GF_MPEGSampleEntryBox *)a;
	fprintf(trace, "<MPEGSystemsSampleDescriptionBox DataReferenceIndex=\"%d\">\n", p->dataReferenceIndex);
	DumpBox(a, trace);
	if (p->esd) {
		gf_box_dump(p->esd, trace);
	} else {
		fprintf(trace, "<!--INVALID MP4 FILE: ESDBox not present in MPEG Sample Description or corrupted-->\n");
	}
	if (a->type == GF_ISOM_BOX_TYPE_ENCS) {
		gf_box_array_dump(p->protections, trace);
	}
	gf_box_dump_done("MPEGSystemsSampleDescriptionBox", a, trace);
	return GF_OK;
}


void base_visual_entry_dump(GF_VisualSampleEntryBox *p, FILE * trace)
{
	fprintf(trace, " DataReferenceIndex=\"%d\" Width=\"%d\" Height=\"%d\"", p->dataReferenceIndex, p->Width, p->Height);

	//dump reserved info
	fprintf(trace, " XDPI=\"%d\" YDPI=\"%d\" BitDepth=\"%d\"", p->horiz_res, p->vert_res, p->bit_depth);
	if (strlen((const char*)p->compressor_name) )
		fprintf(trace, " CompressorName=\"%s\"\n", p->compressor_name+1);

}

GF_Err mp4v_dump(GF_Box *a, FILE * trace)
{
	GF_MPEGVisualSampleEntryBox *p = (GF_MPEGVisualSampleEntryBox *)a;
	const char *name = p->avc_config ? "AVCSampleEntryBox" : "MPEGVisualSampleDescriptionBox";

	fprintf(trace, "<%s", name);
	base_visual_entry_dump((GF_VisualSampleEntryBox *)p, trace);
	fprintf(trace, ">\n");
	DumpBox(a, trace);

	if (p->esd) {
		gf_box_dump(p->esd, trace);
	} else {
		if (p->hevc_config) gf_box_dump(p->hevc_config, trace);
		if (p->avc_config) gf_box_dump(p->avc_config, trace);
		if (p->ipod_ext) gf_box_dump(p->ipod_ext, trace);
		if (p->descr) gf_box_dump(p->descr, trace);
		if (p->bitrate) gf_box_dump(p->bitrate, trace);
		if (p->svc_config) gf_box_dump(p->svc_config, trace);
		if (p->shvc_config) gf_box_dump(p->shvc_config, trace);
	}
	if (a->type == GF_ISOM_BOX_TYPE_ENCV) {
		gf_box_array_dump(p->protections, trace);
	}
	if (p->pasp) gf_box_dump(p->pasp, trace);
	if (p->rvcc) gf_box_dump(p->rvcc, trace);

	gf_box_dump_done(NULL, a, trace);
	fprintf(trace, "</%s>\n", name);
	return GF_OK;
}


void base_audio_entry_dump(GF_AudioSampleEntryBox *p, FILE * trace)
{
	fprintf(trace, " DataReferenceIndex=\"%d\" SampleRate=\"%d\"", p->dataReferenceIndex, p->samplerate_hi);
	fprintf(trace, " Channels=\"%d\" BitsPerSample=\"%d\"", p->channel_count, p->bitspersample);
}

GF_Err mp4a_dump(GF_Box *a, FILE * trace)
{
	GF_MPEGAudioSampleEntryBox *p;

	p = (GF_MPEGAudioSampleEntryBox *)a;
	fprintf(trace, "<MPEGAudioSampleDescriptionBox");
	base_audio_entry_dump((GF_AudioSampleEntryBox *)p, trace);
	fprintf(trace, ">\n");

	DumpBox(a, trace);
	if (p->esd) {
		gf_box_dump(p->esd, trace);
	} else {
		fprintf(trace, "<!--INVALID MP4 FILE: ESDBox not present in MPEG Sample Description or corrupted-->\n");
	}
	if (a->type == GF_ISOM_BOX_TYPE_ENCA) {
		gf_box_array_dump(p->protections, trace);
	}
	gf_box_dump_done("MPEGAudioSampleDescriptionBox", a, trace);
	return GF_OK;
}

GF_Err gnrm_dump(GF_Box *a, FILE * trace)
{
	GF_GenericSampleEntryBox *p = (GF_GenericSampleEntryBox *)a;
	fprintf(trace, "<SampleDescriptionBox DataReferenceIndex=\"%d\" ExtensionDataSize=\"%d\">\n", p->dataReferenceIndex, p->data_size);
	a->type = p->EntryType;
	DumpBox(a, trace);
	a->type = GF_ISOM_BOX_TYPE_GNRM;
	gf_box_dump_done("SampleDescriptionBox", a, trace);
	return GF_OK;
}

GF_Err gnrv_dump(GF_Box *a, FILE * trace)
{
	GF_GenericVisualSampleEntryBox *p = (GF_GenericVisualSampleEntryBox *)a;
	fprintf(trace, "<VisualSampleDescriptionBox DataReferenceIndex=\"%d\" Version=\"%d\" Revision=\"%d\" Vendor=\"%d\" TemporalQuality=\"%d\" SpacialQuality=\"%d\" Width=\"%d\" Height=\"%d\" HorizontalResolution=\"%d\" VerticalResolution=\"%d\" CompressorName=\"%s\" BitDepth=\"%d\">\n",
		p->dataReferenceIndex, p->version, p->revision, p->vendor, p->temporal_quality, p->spatial_quality, p->Width, p->Height, p->horiz_res, p->vert_res, p->compressor_name+1, p->bit_depth);
	a->type = p->EntryType;
	DumpBox(a, trace);
	a->type = GF_ISOM_BOX_TYPE_GNRV;
	gf_box_dump_done("VisualSampleDescriptionBox", a, trace);
	return GF_OK;
}

GF_Err gnra_dump(GF_Box *a, FILE * trace)
{
	GF_GenericAudioSampleEntryBox *p = (GF_GenericAudioSampleEntryBox *)a;
	fprintf(trace, "<AudioSampleDescriptionBox DataReferenceIndex=\"%d\" Version=\"%d\" Revision=\"%d\" Vendor=\"%d\" ChannelCount=\"%d\" BitsPerSample=\"%d\" Samplerate=\"%d\">\n",
		p->dataReferenceIndex, p->version, p->revision, p->vendor, p->channel_count, p->bitspersample, p->samplerate_hi);
	a->type = p->EntryType;
	DumpBox(a, trace);
	a->type = GF_ISOM_BOX_TYPE_GNRA;
	gf_box_dump_done("AudioSampleDescriptionBox", a, trace);
	return GF_OK;
}

GF_Err edts_dump(GF_Box *a, FILE * trace)
{
	GF_EditBox *p;

	p = (GF_EditBox *)a;
	fprintf(trace, "<EditBox>\n");
	DumpBox(a, trace);
	gf_box_dump(p->editList, trace);
	gf_box_dump_done("EditBox", a, trace);
	return GF_OK;
}

GF_Err udta_dump(GF_Box *a, FILE * trace)
{
	GF_UserDataBox *p;
	GF_UserDataMap *map;
	u32 i;

	p = (GF_UserDataBox *)a;
	fprintf(trace, "<UserDataBox>\n");
	DumpBox(a, trace);

	i=0;
	while ((map = (GF_UserDataMap *)gf_list_enum(p->recordList, &i))) {
		fprintf(trace, "<UDTARecord Type=\"%s\">\n", gf_4cc_to_str(map->boxType));
		gf_box_array_dump(map->other_boxes, trace);
		fprintf(trace, "</UDTARecord>\n");
	}
	gf_box_dump_done("UserDataBox", a, trace);
	return GF_OK;
}

GF_Err dref_dump(GF_Box *a, FILE * trace)
{
//	GF_DataReferenceBox *p = (GF_DataReferenceBox *)a;
	fprintf(trace, "<DataReferenceBox>\n");
	DumpBox(a, trace);
	gf_full_box_dump(a, trace);
	gf_box_dump_done("DataReferenceBox", a, trace);
	return GF_OK;
}

GF_Err stsd_dump(GF_Box *a, FILE * trace)
{
//	GF_SampleDescriptionBox *p = (GF_SampleDescriptionBox *)a;
	fprintf(trace, "<SampleDescriptionBox>\n");
	DumpBox(a, trace);
	gf_full_box_dump(a, trace);
	gf_box_dump_done("SampleDescriptionBox", a, trace);
	return GF_OK;
}

GF_Err stts_dump(GF_Box *a, FILE * trace)
{
	GF_TimeToSampleBox *p;
	u32 i, nb_samples;

	p = (GF_TimeToSampleBox *)a;
	fprintf(trace, "<TimeToSampleBox EntryCount=\"%d\">\n", p->nb_entries);
	DumpBox(a, trace);
	gf_full_box_dump(a, trace);

	nb_samples = 0;
	for (i=0; i<p->nb_entries; i++) {
		fprintf(trace, "<TimeToSampleEntry SampleDelta=\"%d\" SampleCount=\"%d\"/>\n", p->entries[i].sampleDelta, p->entries[i].sampleCount);
		nb_samples += p->entries[i].sampleCount;
	}
	fprintf(trace, "<!-- counted %d samples in STTS entries -->\n", nb_samples);
	gf_box_dump_done("TimeToSampleBox", a, trace);
	return GF_OK;
}

GF_Err ctts_dump(GF_Box *a, FILE * trace)
{
	GF_CompositionOffsetBox *p;
	u32 i, nb_samples;
	p = (GF_CompositionOffsetBox *)a;
	fprintf(trace, "<CompositionOffsetBox EntryCount=\"%d\">\n", p->nb_entries);
	DumpBox(a, trace);
	gf_full_box_dump(a, trace);

	nb_samples = 0;
	for (i=0; i<p->nb_entries;i++) {
		fprintf(trace, "<CompositionOffsetEntry CompositionOffset=\"%d\" SampleCount=\"%d\"/>\n", p->entries[i].decodingOffset, p->entries[i].sampleCount);
		nb_samples += p->entries[i].sampleCount;
	}
	fprintf(trace, "<!-- counted %d samples in CTTS entries -->\n", nb_samples);
	gf_box_dump_done("CompositionOffsetBox", a, trace);
	return GF_OK;
}

GF_Err cslg_dump(GF_Box *a, FILE * trace)
{
	GF_CompositionToDecodeBox *p;

	p = (GF_CompositionToDecodeBox *)a;
	fprintf(trace, "<CompositionToDecodeBox compositionToDTSShift=\"%d\" leastDecodeToDisplayDelta=\"%d\" compositionStartTime=\"%d\" compositionEndTime=\"%d\">\n", p->leastDecodeToDisplayDelta, p->greatestDecodeToDisplayDelta, p->compositionStartTime, p->compositionEndTime);
	DumpBox(a, trace);
	gf_full_box_dump(a, trace);
	gf_box_dump_done("CompositionToDecodeBox", a, trace);
	return GF_OK;
}

GF_Err stsh_dump(GF_Box *a, FILE * trace)
{
	GF_ShadowSyncBox *p;
	u32 i;
	GF_StshEntry *t;

	p = (GF_ShadowSyncBox *)a;
	fprintf(trace, "<SyncShadowBox EntryCount=\"%d\">\n", gf_list_count(p->entries));
	DumpBox(a, trace);
	gf_full_box_dump(a, trace);
	i=0;
	while ((t = (GF_StshEntry *)gf_list_enum(p->entries, &i))) {
		fprintf(trace, "<SyncShadowEntry ShadowedSample=\"%d\" SyncSample=\"%d\"/>\n", t->shadowedSampleNumber, t->syncSampleNumber);
	}
	gf_box_dump_done("SyncShadowBox", a, trace);
	return GF_OK;
}

GF_Err elst_dump(GF_Box *a, FILE * trace)
{
	GF_EditListBox *p;
	u32 i;
	GF_EdtsEntry *t;

	p = (GF_EditListBox *)a;
	fprintf(trace, "<EditListBox EntryCount=\"%d\">\n", gf_list_count(p->entryList));
	DumpBox(a, trace);
	gf_full_box_dump(a, trace);

	i=0;
	while ((t = (GF_EdtsEntry *)gf_list_enum(p->entryList, &i))) {
		fprintf(trace, "<EditListEntry Duration=\""LLD"\" MediaTime=\""LLD"\" MediaRate=\"%u\"/>\n", LLD_CAST t->segmentDuration, LLD_CAST t->mediaTime, t->mediaRate);
	}
	gf_box_dump_done("EditListBox", a, trace);
	return GF_OK;
}

GF_Err stsc_dump(GF_Box *a, FILE * trace)
{
	GF_SampleToChunkBox *p;
	u32 i, nb_samples;

	p = (GF_SampleToChunkBox *)a;
	fprintf(trace, "<SampleToChunkBox EntryCount=\"%d\">\n", p->nb_entries);
	DumpBox(a, trace);
	gf_full_box_dump(a, trace);

	nb_samples = 0;
	for (i=0; i<p->nb_entries; i++) {
		fprintf(trace, "<SampleToChunkEntry FirstChunk=\"%d\" SamplesPerChunk=\"%d\" SampleDescriptionIndex=\"%d\"/>\n", p->entries[i].firstChunk, p->entries[i].samplesPerChunk, p->entries[i].sampleDescriptionIndex);
		if (i+1<p->nb_entries) {
			nb_samples += (p->entries[i+1].firstChunk - p->entries[i].firstChunk) * p->entries[i].samplesPerChunk;
		} else {
			nb_samples += p->entries[i].samplesPerChunk;
		}
	}
	fprintf(trace, "<!-- counted %d samples in STSC entries (could be less than sample count) -->\n", nb_samples);
	gf_box_dump_done("SampleToChunkBox", a, trace);
	return GF_OK;
}

GF_Err stsz_dump(GF_Box *a, FILE * trace)
{
	GF_SampleSizeBox *p;
	u32 i;
	p = (GF_SampleSizeBox *)a;

	fprintf(trace, "<%sBox SampleCount=\"%d\"", (a->type == GF_ISOM_BOX_TYPE_STSZ) ? "SampleSize" : "CompactSampleSize", p->sampleCount);
	if (a->type == GF_ISOM_BOX_TYPE_STSZ) {
		if (p->sampleSize) {
			fprintf(trace, " ConstantSampleSize=\"%d\"", p->sampleSize);
		}
	} else {
		fprintf(trace, " SampleSizeBits=\"%d\"", p->sampleSize);
	}
	fprintf(trace, ">\n");

	DumpBox(a, trace);
	gf_full_box_dump(a, trace);

	if ((a->type != GF_ISOM_BOX_TYPE_STSZ) || !p->sampleSize) {
		if (!p->sizes) {
			fprintf(trace, "<!--WARNING: No Sample Size indications-->\n");
		} else {
			for (i=0; i<p->sampleCount; i++) {
				fprintf(trace, "<SampleSizeEntry Size=\"%d\"/>\n", p->sizes[i]);
			}
		}
	}
	gf_box_dump_done(NULL, a, trace);
	fprintf(trace, "</%sBox>\n", (a->type == GF_ISOM_BOX_TYPE_STSZ) ? "SampleSize" : "CompactSampleSize");
	return GF_OK;
}

GF_Err stco_dump(GF_Box *a, FILE * trace)
{
	GF_ChunkOffsetBox *p;
	u32 i;

	p = (GF_ChunkOffsetBox *)a;
	fprintf(trace, "<ChunkOffsetBox EntryCount=\"%d\">\n", p->nb_entries);
	DumpBox(a, trace);
	gf_full_box_dump(a, trace);

	if (!p->offsets) {
		fprintf(trace, "<!--Warning: No Chunk Offsets indications-->\n");
	} else {
		for (i=0; i<p->nb_entries; i++) {
			fprintf(trace, "<ChunkEntry offset=\"%d\"/>\n", p->offsets[i]);
		}
	}
	gf_box_dump_done("ChunkOffsetBox", a, trace);
	return GF_OK;
}

GF_Err stss_dump(GF_Box *a, FILE * trace)
{
	GF_SyncSampleBox *p;
	u32 i;

	p = (GF_SyncSampleBox *)a;
	fprintf(trace, "<SyncSampleBox EntryCount=\"%d\">\n", p->nb_entries);
	DumpBox(a, trace);
	gf_full_box_dump(a, trace);

	if (!p->sampleNumbers) {
		fprintf(trace, "<!--Warning: No Key Frames indications-->\n");
	} else {
		for (i=0; i<p->nb_entries; i++) {
			fprintf(trace, "<SyncSampleEntry sampleNumber=\"%d\"/>\n", p->sampleNumbers[i]);
		}
	}
	gf_box_dump_done("SyncSampleBox", a, trace);
	return GF_OK;
}

GF_Err stdp_dump(GF_Box *a, FILE * trace)
{
	GF_DegradationPriorityBox *p;
	u32 i;

	p = (GF_DegradationPriorityBox *)a;
	fprintf(trace, "<DegradationPriorityBox EntryCount=\"%d\">\n", p->nb_entries);
	DumpBox(a, trace);
	gf_full_box_dump(a, trace);

	if (!p->priorities) {
		fprintf(trace, "<!--Warning: No Degradation Priority indications-->\n");
	} else {
		for (i=0; i<p->nb_entries; i++) {
			fprintf(trace, "<DegradationPriorityEntry DegradationPriority=\"%d\"/>\n", p->priorities[i]);
		}
	}
	gf_box_dump_done("DegradationPriorityBox", a, trace);
	return GF_OK;
}

GF_Err sdtp_dump(GF_Box *a, FILE * trace)
{
	GF_SampleDependencyTypeBox *p;
	u32 i;

	p = (GF_SampleDependencyTypeBox*)a;
	fprintf(trace, "<SampleDependencyTypeBox SampleCount=\"%d\">\n", p->sampleCount);
	DumpBox(a, trace);
	gf_full_box_dump(a, trace);

	if (!p->sample_info) {
		fprintf(trace, "<!--Warning: No sample dependencies indications-->\n");
	} else {
		for (i=0; i<p->sampleCount; i++) {
			u8 flag = p->sample_info[i];
			fprintf(trace, "<SampleDependencyEntry ");
			switch ( (flag >> 4) & 3) {
			case 0: fprintf(trace, "dependsOnOther=\"unknown\" "); break;
			case 1: fprintf(trace, "dependsOnOther=\"yes\" "); break;
			case 2: fprintf(trace, "dependsOnOther=\"no\" "); break;
			case 3: fprintf(trace, "dependsOnOther=\"!! RESERVED !!\" "); break;
			}
			switch ( (flag >> 2) & 3) {
			case 0: fprintf(trace, "dependedOn=\"unknown\" "); break;
			case 1: fprintf(trace, "dependedOn=\"yes\" "); break;
			case 2: fprintf(trace, "dependedOn=\"no\" "); break;
			case 3: fprintf(trace, "dependedOn=\"!! RESERVED !!\" "); break;
			}
			switch ( flag & 3) {
			case 0: fprintf(trace, "hasRedundancy=\"unknown\" "); break;
			case 1: fprintf(trace, "hasRedundancy=\"yes\" "); break;
			case 2: fprintf(trace, "hasRedundancy=\"no\" "); break;
			case 3: fprintf(trace, "hasRedundancy=\"!! RESERVED !!\" "); break;
			}
			fprintf(trace, " />\n");
		}
	}
	gf_box_dump_done("SampleDependencyTypeBox", a, trace);
	return GF_OK;
}

GF_Err co64_dump(GF_Box *a, FILE * trace)
{
	GF_ChunkLargeOffsetBox *p;
	u32 i;

	p = (GF_ChunkLargeOffsetBox *)a;
	fprintf(trace, "<ChunkLargeOffsetBox EntryCount=\"%d\">\n", p->nb_entries);
	DumpBox(a, trace);
	gf_full_box_dump(a, trace);

	if (!p->offsets) {
		fprintf(trace, "<Warning: No Chunk Offsets indications/>\n");
	} else {
		for (i=0; i<p->nb_entries; i++)
			fprintf(trace, "<ChunkOffsetEntry offset=\""LLD"\"/>\n", LLD_CAST p->offsets[i]);
	}
	gf_box_dump_done("ChunkLargeOffsetBox", a, trace);
	return GF_OK;
}

GF_Err esds_dump(GF_Box *a, FILE * trace)
{
	GF_ESDBox *p;

	p = (GF_ESDBox *)a;
	fprintf(trace, "<MPEG4ESDescriptorBox>\n");
	DumpBox(a, trace);
	gf_full_box_dump(a, trace);

	if (p->desc) {
#ifndef GPAC_DISABLE_OD_DUMP
		gf_odf_dump_desc(p->desc, trace, 1, 1);
#else
		fprintf(trace, "<!-- Object Descriptor Dumping disabled in this build of GPAC -->\n");
#endif
	} else {
		fprintf(trace, "<!--INVALID MP4 FILE: ESD not present in MPEG Sample Description or corrupted-->\n");
	}
	gf_box_dump_done("MPEG4ESDescriptorBox", a, trace);
	return GF_OK;
}

GF_Err minf_dump(GF_Box *a, FILE * trace)
{
	GF_MediaInformationBox *p;

	p = (GF_MediaInformationBox *)a;
	fprintf(trace, "<MediaInformationBox>\n");
	DumpBox(a, trace);

	gf_box_dump(p->InfoHeader, trace);
	gf_box_dump(p->dataInformation, trace);
	gf_box_dump(p->sampleTable, trace);
	gf_box_dump_done("MediaInformationBox", a, trace);
	return GF_OK;
}

GF_Err tkhd_dump(GF_Box *a, FILE * trace)
{
	GF_TrackHeaderBox *p;
	p = (GF_TrackHeaderBox *)a;
	fprintf(trace, "<TrackHeaderBox ");

	fprintf(trace, "CreationTime=\""LLD"\" ModificationTime=\""LLD"\" TrackID=\"%u\" Duration=\""LLD"\"",
		LLD_CAST p->creationTime, LLD_CAST p->modificationTime, p->trackID, LLD_CAST p->duration);

	if (p->alternate_group) fprintf(trace, " AlternateGroupID=\"%d\"", p->alternate_group);
	if (p->volume) {
		fprintf(trace, " Volume=\"%.2f\"", (Float)p->volume / 256);
	} else if (p->width || p->height) {
		fprintf(trace, " Width=\"%.2f\" Height=\"%.2f\"", (Float)p->width / 65536, (Float)p->height / 65536);
		if (p->layer) fprintf(trace, " Layer=\"%d\"", p->layer);
	}
	fprintf(trace, ">\n");
	if (p->width || p->height) {
		fprintf(trace, "<Matrix m11=\"0x%.8x\" m12=\"0x%.8x\" m13=\"0x%.8x\" \
								m21=\"0x%.8x\" m22=\"0x%.8x\" m23=\"0x%.8x\" \
								m31=\"0x%.8x\" m32=\"0x%.8x\" m33=\"0x%.8x\"/>",
								p->matrix[0], p->matrix[1], p->matrix[2],
								p->matrix[3], p->matrix[4], p->matrix[5],
								p->matrix[6], p->matrix[7], p->matrix[8]);
	}
	DumpBox(a, trace);
	gf_full_box_dump(a, trace);

	gf_box_dump_done("TrackHeaderBox", a, trace);
	return GF_OK;
}

GF_Err tref_dump(GF_Box *a, FILE * trace)
{
//	GF_TrackReferenceBox *p = (GF_TrackReferenceBox *)a;
	fprintf(trace, "<TrackReferenceBox>\n");
	DumpBox(a, trace);
	gf_box_dump_done("TrackReferenceBox", a, trace);
	return GF_OK;
}

GF_Err mdia_dump(GF_Box *a, FILE * trace)
{
	GF_MediaBox *p = (GF_MediaBox *)a;
	fprintf(trace, "<MediaBox>\n");
	DumpBox(a, trace);
	gf_box_dump(p->mediaHeader, trace);
	gf_box_dump(p->handler, trace);
	gf_box_dump(p->information, trace);
	gf_box_dump_done("MediaBox", a, trace);
	return GF_OK;
}

GF_Err defa_dump(GF_Box *a, FILE * trace)
{
	fprintf(trace, "<UnknownBox>\n");
	DumpBox(a, trace);
	fprintf(trace, "</UnknownBox>\n");
	return GF_OK;
}

GF_Err void_dump(GF_Box *a, FILE * trace)
{
	fprintf(trace, "<VoidBox/>\n");
	return GF_OK;
}

GF_Err ftyp_dump(GF_Box *a, FILE * trace)
{
	GF_FileTypeBox *p;
	u32 i;

	p = (GF_FileTypeBox *)a;
    fprintf(trace, "<%s MajorBrand=\"%s\" MinorVersion=\"%d\">\n", (a->type == GF_ISOM_BOX_TYPE_FTYP ? "FileTypeBox" : "SegmentTypeBox"), gf_4cc_to_str(p->majorBrand), p->minorVersion);
	DumpBox(a, trace);

	for (i=0; i<p->altCount; i++) {
		fprintf(trace, "<BrandEntry AlternateBrand=\"%s\"/>\n", gf_4cc_to_str(p->altBrand[i]));
	}
	gf_box_dump_done(NULL, a, trace);
	fprintf(trace, "</%s>\n",(a->type == GF_ISOM_BOX_TYPE_FTYP ? "FileTypeBox" : "SegmentTypeBox"));
	return GF_OK;
}

GF_Err padb_dump(GF_Box *a, FILE * trace)
{
	GF_PaddingBitsBox *p;
	u32 i;

	p = (GF_PaddingBitsBox *)a;
	fprintf(trace, "<PaddingBitsBox EntryCount=\"%d\">\n", p->SampleCount);
	DumpBox(a, trace);
	for (i=0; i<p->SampleCount; i+=1) {
		fprintf(trace, "<PaddingBitsEntry PaddingBits=\"%d\"/>\n", p->padbits[i]);
	}
	gf_box_dump_done("PaddingBitsBox", a, trace);
	return GF_OK;
}

GF_Err stsf_dump(GF_Box *a, FILE * trace)
{
	GF_SampleFragmentBox *p;
	GF_StsfEntry *ent;
	u32 i, j, count;


	p = (GF_SampleFragmentBox *)a;
	count = gf_list_count(p->entryList);
	fprintf(trace, "<SampleFragmentBox EntryCount=\"%d\">\n", count);
	DumpBox(a, trace);

	for (i=0; i<count; i++) {
		ent = (GF_StsfEntry *)gf_list_get(p->entryList, i);
		fprintf(trace, "<SampleFragmentEntry SampleNumber=\"%d\" FragmentCount=\"%d\">\n", ent->SampleNumber, ent->fragmentCount);
		for (j=0;j<ent->fragmentCount;j++) fprintf(trace, "<FragmentSizeEntry size=\"%d\"/>\n", ent->fragmentSizes[j]);
		fprintf(trace, "</SampleFragmentEntry>\n");
	}

	gf_box_dump_done("SampleFragmentBox", a, trace);
	return GF_OK;
}


GF_Err gppa_dump(GF_Box *a, FILE * trace)
{
	char *szName;
	GF_3GPPAudioSampleEntryBox *p = (GF_3GPPAudioSampleEntryBox *)a;
	switch (p->type) {
	case GF_ISOM_SUBTYPE_3GP_AMR: szName = "AMRSampleDescription"; break;
	case GF_ISOM_SUBTYPE_3GP_AMR_WB: szName = "AMR_WB_SampleDescription"; break;
	case GF_ISOM_SUBTYPE_3GP_EVRC: szName = "EVRCSampleDescription"; break;
	case GF_ISOM_SUBTYPE_3GP_QCELP: szName = "QCELPSampleDescription"; break;
	case GF_ISOM_SUBTYPE_3GP_SMV: szName = "SMVSampleDescription"; break;
	default: szName = "3GPAudioSampleDescription"; break;
	}
	fprintf(trace, "<%sBox", szName);
	base_audio_entry_dump((GF_AudioSampleEntryBox *)p, trace);
	fprintf(trace, ">\n");
	DumpBox(a, trace);

	if (p->info) {
		gf_box_dump(p->info, trace);
	} else {
		fprintf(trace, "<!--INVALID 3GPP FILE: Config not present in Sample Description-->\n");
	}
	gf_box_dump_done(NULL, a, trace);
	fprintf(trace, "</%sBox>\n", szName);
	return GF_OK;
}

GF_Err gppv_dump(GF_Box *a, FILE * trace)
{
	char *szName;
	GF_3GPPVisualSampleEntryBox *p = (GF_3GPPVisualSampleEntryBox *)a;

	switch (p->type) {
	case GF_ISOM_SUBTYPE_3GP_H263: szName = "H263SampleDescription"; break;
	default: szName = "3GPVisualSampleDescription"; break;
	}
	fprintf(trace, "<%sBox", szName);
	base_visual_entry_dump((GF_VisualSampleEntryBox *)p, trace);
	fprintf(trace, ">\n");
	DumpBox(a, trace);
	if (p->info) {
		gf_box_dump(p->info, trace);
	} else {
		fprintf(trace, "<!--INVALID 3GPP FILE: Config not present in Sample Description-->\n");
	}
	gf_box_dump_done(NULL, a, trace);
	fprintf(trace, "</%sBox>\n", szName);
	return GF_OK;
}

GF_Err gppc_dump(GF_Box *a, FILE * trace)
{
	GF_3GPPConfigBox *p = (GF_3GPPConfigBox *)a;
	const char *name = gf_4cc_to_str(p->cfg.vendor);
	switch (p->cfg.type) {
	case GF_ISOM_SUBTYPE_3GP_AMR:
	case GF_ISOM_SUBTYPE_3GP_AMR_WB:
		fprintf(trace, "<AMRConfigurationBox Vendor=\"%s\" Version=\"%d\"", name, p->cfg.decoder_version);
		fprintf(trace, " FramesPerSample=\"%d\" SupportedModes=\"%x\" ModeRotating=\"%d\"", p->cfg.frames_per_sample, p->cfg.AMR_mode_set, p->cfg.AMR_mode_change_period);
		fprintf(trace, ">\n");
		DumpBox(a, trace);
		gf_box_dump_done("AMRConfigurationBox", a, trace);
		break;
	case GF_ISOM_SUBTYPE_3GP_EVRC:
		fprintf(trace, "<EVRCConfigurationBox Vendor=\"%s\" Version=\"%d\" FramesPerSample=\"%d\" >\n", name, p->cfg.decoder_version, p->cfg.frames_per_sample);
		DumpBox(a, trace);
		gf_box_dump_done("EVRCConfigurationBox", a, trace);
		break;
	case GF_ISOM_SUBTYPE_3GP_QCELP:
		fprintf(trace, "<QCELPConfigurationBox Vendor=\"%s\" Version=\"%d\" FramesPerSample=\"%d\" >\n", name, p->cfg.decoder_version, p->cfg.frames_per_sample);
		DumpBox(a, trace);
		gf_box_dump_done("QCELPConfigurationBox", a, trace);
		break;
	case GF_ISOM_SUBTYPE_3GP_SMV:
		fprintf(trace, "<SMVConfigurationBox Vendor=\"%s\" Version=\"%d\" FramesPerSample=\"%d\" >\n", name, p->cfg.decoder_version, p->cfg.frames_per_sample);
		DumpBox(a, trace);
		gf_box_dump_done("SMVConfigurationBox", a, trace);
		break;
	case GF_ISOM_SUBTYPE_3GP_H263:
		fprintf(trace, "<H263ConfigurationBox Vendor=\"%s\" Version=\"%d\"", name, p->cfg.decoder_version);
		fprintf(trace, " Profile=\"%d\" Level=\"%d\"", p->cfg.H263_profile, p->cfg.H263_level);
		fprintf(trace, ">\n");
		DumpBox(a, trace);
		gf_box_dump_done("H263ConfigurationBox", a, trace);
		break;
	default:
		break;
	}
	return GF_OK;
}


GF_Err avcc_dump(GF_Box *a, FILE * trace)
{
	u32 i, count;
	GF_AVCConfigurationBox *p = (GF_AVCConfigurationBox *) a;
	const char *name = (p->type==GF_ISOM_BOX_TYPE_SVCC) ? "SVC" : "AVC";
	fprintf(trace, "<%sConfigurationBox>\n", name);

	fprintf(trace, "<%sDecoderConfigurationRecord configurationVersion=\"%d\" AVCProfileIndication=\"%d\" profile_compatibility=\"%d\" AVCLevelIndication=\"%d\" nal_unit_size=\"%d\"",
					name, p->config->configurationVersion, p->config->AVCProfileIndication, p->config->profile_compatibility, p->config->AVCLevelIndication, p->config->nal_unit_size);

	if (p->type==GF_ISOM_BOX_TYPE_SVCC) 
		fprintf(trace, " complete_representation=\"%d\"", p->config->complete_representation);

	if (p->type==GF_ISOM_BOX_TYPE_AVCC) {
		switch (p->config->AVCProfileIndication) {
		case 100:
		case 110:
		case 122:
		case 144:
			fprintf(trace, " chroma_format=\"%d\" luma_bit_depth=\"%d\" chroma_bit_depth=\"%d\"", p->config->chroma_format, p->config->luma_bit_depth, p->config->chroma_bit_depth);
			break;
		}
	}

	fprintf(trace, ">\n");

	count = gf_list_count(p->config->sequenceParameterSets);
	for (i=0; i<count; i++) {
		GF_AVCConfigSlot *c = (GF_AVCConfigSlot *)gf_list_get(p->config->sequenceParameterSets, i);
		fprintf(trace, "<SequenceParameterSet size=\"%d\" content=\"", c->size);
		DumpData(trace, c->data, c->size);
		fprintf(trace, "\"/>\n");
	}
	count = gf_list_count(p->config->pictureParameterSets);
	for (i=0; i<count; i++) {
		GF_AVCConfigSlot *c = (GF_AVCConfigSlot *)gf_list_get(p->config->pictureParameterSets, i);
		fprintf(trace, "<PictureParameterSet size=\"%d\" content=\"", c->size);
		DumpData(trace, c->data, c->size);
		fprintf(trace, "\"/>\n");
	}

	if (p->config->sequenceParameterSetExtensions) {
		count = gf_list_count(p->config->sequenceParameterSetExtensions);
		for (i=0; i<count; i++) {
			GF_AVCConfigSlot *c = (GF_AVCConfigSlot *)gf_list_get(p->config->sequenceParameterSetExtensions, i);
			fprintf(trace, "<SequenceParameterSetExtensions size=\"%d\" content=\"", c->size);
			DumpData(trace, c->data, c->size);
			fprintf(trace, "\"/>\n");
		}
	}

	fprintf(trace, "</%sDecoderConfigurationRecord>\n", name);

	DumpBox(a, trace);
	gf_box_dump_done(NULL, a, trace);
	fprintf(trace, "</%sConfigurationBox>\n", name);
	return GF_OK;
}

GF_Err hvcc_dump(GF_Box *a, FILE * trace)
{
	u32 i, count;
	const char *name = (a->type==GF_ISOM_BOX_TYPE_HVCC) ? "HEVC" : "SHVC";
	GF_HEVCConfigurationBox *p = (GF_HEVCConfigurationBox *) a;

	fprintf(trace, "<%sConfigurationBox>\n", name);

	fprintf(trace, "<%sDecoderConfigurationRecord nal_unit_size=\"%d\" ", name, p->config->nal_unit_size);
	fprintf(trace, "configurationVersion=\"%d\" ", p->config->configurationVersion);
	fprintf(trace, "profile_space=\"%d\" ", p->config->profile_space);
	fprintf(trace, "tier_flag=\"%d\" ", p->config->tier_flag);
	fprintf(trace, "profile_idc=\"%d\" ", p->config->profile_idc);
	fprintf(trace, "general_profile_compatibility_flags=\"%d\" ", p->config->general_profile_compatibility_flags);
	fprintf(trace, "progressive_source_flag=\"%d\" ", p->config->progressive_source_flag);
	fprintf(trace, "interlaced_source_flag=\"%d\" ", p->config->interlaced_source_flag);
	fprintf(trace, "non_packed_constraint_flag=\"%d\" ", p->config->non_packed_constraint_flag);
	fprintf(trace, "frame_only_constraint_flag=\"%d\" ", p->config->frame_only_constraint_flag);
	fprintf(trace, "constraint_indicator_flags=\""LLD"\" ", p->config->constraint_indicator_flags);
	fprintf(trace, "level_idc=\"%d\" ", p->config->level_idc);
	fprintf(trace, "min_spatial_segmentation_idc=\"%d\" ", p->config->min_spatial_segmentation_idc);
	fprintf(trace, "parallelismType=\"%d\" ", p->config->parallelismType);

	fprintf(trace, "chroma_format=\"%d\" luma_bit_depth=\"%d\" chroma_bit_depth=\"%d\" avgFrameRate=\"%d\" constantFrameRate=\"%d\" numTemporalLayers=\"%d\" temporalIdNested=\"%d\"",
		p->config->chromaFormat, p->config->luma_bit_depth, p->config->chroma_bit_depth, p->config->avgFrameRate, p->config->constantFrameRate, p->config->numTemporalLayers, p->config->temporalIdNested);

	fprintf(trace, ">\n");

	count = gf_list_count(p->config->param_array);
	for (i=0; i<count; i++) {
		u32 nalucount, j;
		GF_HEVCParamArray *ar = gf_list_get(p->config->param_array, i);
		fprintf(trace, "<ParameterSetArray nalu_type=\"%d\" complete_set=\"%d\">\n", ar->type, ar->array_completeness);
		nalucount = gf_list_count(ar->nalus);
		for (j=0; j<nalucount; j++) {
			GF_AVCConfigSlot *c = (GF_AVCConfigSlot *)gf_list_get(ar->nalus, j);
			fprintf(trace, "<ParameterSet size=\"%d\" content=\"", c->size);
			DumpData(trace, c->data, c->size);
			fprintf(trace, "\"/>\n");
		}
		fprintf(trace, "</ParameterSetArray>\n");
	}

	fprintf(trace, "</%sDecoderConfigurationRecord>\n", name);

	DumpBox(a, trace);
	gf_box_dump_done(NULL, a, trace);
	fprintf(trace, "</%sConfigurationBox>\n", name);
	return GF_OK;
}

GF_Err m4ds_dump(GF_Box *a, FILE * trace)
{
	u32 i;
	GF_Descriptor *desc;
	GF_MPEG4ExtensionDescriptorsBox *p = (GF_MPEG4ExtensionDescriptorsBox *) a;
	fprintf(trace, "<MPEG4ExtensionDescriptorsBox>\n");

	i=0;
	while ((desc = (GF_Descriptor *)gf_list_enum(p->descriptors, &i))) {
#ifndef GPAC_DISABLE_OD_DUMP
		gf_odf_dump_desc(desc, trace, 1, 1);
#else
		fprintf(trace, "<!-- Object Descriptor Dumping disabled in this build of GPAC -->\n");
#endif
	}
	DumpBox(a, trace);
	gf_box_dump_done("MPEG4ExtensionDescriptorsBox", a, trace);
	return GF_OK;
}

GF_Err btrt_dump(GF_Box *a, FILE * trace)
{
	GF_MPEG4BitRateBox *p = (GF_MPEG4BitRateBox*)a;
	fprintf(trace, "<MPEG4BitRateBox BufferSizeDB=\"%d\" avgBitRate=\"%d\" maxBitRate=\"%d\">\n", p->bufferSizeDB, p->avgBitrate, p->maxBitrate);
	DumpBox(a, trace);
	gf_box_dump_done("MPEG4BitRateBox", a, trace);
	return GF_OK;
}

GF_Err ftab_dump(GF_Box *a, FILE * trace)
{
	u32 i;
	GF_FontTableBox *p = (GF_FontTableBox *)a;
	fprintf(trace, "<FontTableBox>\n");
	DumpBox(a, trace);
	for (i=0; i<p->entry_count; i++) {
		fprintf(trace, "<FontRecord ID=\"%d\" name=\"%s\"/>\n", p->fonts[i].fontID, p->fonts[i].fontName ? p->fonts[i].fontName : "NULL");
	}
	gf_box_dump_done("FontTableBox", a, trace);
	return GF_OK;
}

static void gpp_dump_rgba8(FILE * trace, char *name, u32 col)
{
	fprintf(trace, "%s=\"%x %x %x %x\"", name, (col>>16)&0xFF, (col>>8)&0xFF, (col)&0xFF, (col>>24)&0xFF);
}
static void gpp_dump_rgb16(FILE * trace, char *name, char col[6])
{
	fprintf(trace, "%s=\"%x %x %x\"", name, *((u16*)col), *((u16*)(col+1)), *((u16*)(col+2)));
}
static void gpp_dump_box(FILE * trace, GF_BoxRecord *rec)
{
	fprintf(trace, "<BoxRecord top=\"%d\" left=\"%d\" bottom=\"%d\" right=\"%d\"/>\n", rec->top, rec->left, rec->bottom, rec->right);
}
static void gpp_dump_style(FILE * trace, GF_StyleRecord *rec)
{
	fprintf(trace, "<StyleRecord startChar=\"%d\" endChar=\"%d\" fontID=\"%d\" styles=\"", rec->startCharOffset, rec->endCharOffset, rec->fontID);
	if (!rec->style_flags) {
		fprintf(trace, "Normal");
	} else {
		if (rec->style_flags & 1) fprintf(trace, "Bold ");
		if (rec->style_flags & 2) fprintf(trace, "Italic ");
		if (rec->style_flags & 4) fprintf(trace, "Underlined ");
	}
	fprintf(trace, "\" fontSize=\"%d\" ", rec->font_size);
	gpp_dump_rgba8(trace, "text-color", rec->text_color);
	fprintf(trace, "/>\n");
}

GF_Err tx3g_dump(GF_Box *a, FILE * trace)
{
	GF_Tx3gSampleEntryBox *p = (GF_Tx3gSampleEntryBox *)a;
	fprintf(trace, "<Tx3gSampleEntryBox dataReferenceIndex=\"%d\" displayFlags=\"%x\" horizontal-justification=\"%d\" vertical-justification=\"%d\" ",
			p->dataReferenceIndex, p->displayFlags, p->horizontal_justification, p->vertical_justification);

	gpp_dump_rgba8(trace, "background-color", p->back_color);
	fprintf(trace, ">\n");
	DumpBox(a, trace);

	fprintf(trace, "<DefaultBox>\n");
	gpp_dump_box(trace, &p->default_box);
	gf_box_dump_done("DefaultBox", a, trace);
	fprintf(trace, "<DefaultStyle>\n");
	gpp_dump_style(trace, &p->default_style);
	fprintf(trace, "</DefaultStyle>\n");
	gf_box_dump(p->font_table, trace);
	gf_box_dump_done("Tx3gSampleEntryBox", a, trace);
	return GF_OK;
}

GF_Err text_dump(GF_Box *a, FILE * trace)
{
	GF_TextSampleEntryBox *p = (GF_TextSampleEntryBox *)a;
	fprintf(trace, "<TextSampleEntryBox dataReferenceIndex=\"%d\" displayFlags=\"%x\" textJustification=\"%d\"  ",
			p->dataReferenceIndex, p->displayFlags, p->textJustification);
	if (p->textName)
		fprintf(trace, "textName=%s ", p->textName);
	gpp_dump_rgb16(trace, "background-color", p->background_color);
	gpp_dump_rgb16(trace, "foreground-color", p->foreground_color);
	fprintf(trace, ">\n");
	DumpBox(a, trace);

	fprintf(trace, "<DefaultBox>\n");
	gpp_dump_box(trace, &p->default_box);
	gf_box_dump_done("DefaultBox", a, trace);
	gf_box_dump_done("TextSampleEntryBox", a, trace);
	return GF_OK;
}

GF_Err styl_dump(GF_Box *a, FILE * trace)
{
	u32 i;
	GF_TextStyleBox*p = (GF_TextStyleBox*)a;
	fprintf(trace, "<TextStyleBox>\n");
	DumpBox(a, trace);
	for (i=0; i<p->entry_count; i++) gpp_dump_style(trace, &p->styles[i]);
	gf_box_dump_done("TextStyleBox", a, trace);
	return GF_OK;
}
GF_Err hlit_dump(GF_Box *a, FILE * trace)
{
	GF_TextHighlightBox*p = (GF_TextHighlightBox*)a;
	fprintf(trace, "<TextHighlightBox startcharoffset=\"%d\" endcharoffset=\"%d\">\n", p->startcharoffset, p->endcharoffset);
	DumpBox(a, trace);
	gf_box_dump_done("TextHighlightBox", a, trace);
	return GF_OK;
}
GF_Err hclr_dump(GF_Box *a, FILE * trace)
{
	GF_TextHighlightColorBox*p = (GF_TextHighlightColorBox*)a;
	fprintf(trace, "<TextHighlightColorBox ");
	gpp_dump_rgba8(trace, "highlight_color", p->hil_color);
	fprintf(trace, ">\n");
	DumpBox(a, trace);
	gf_box_dump_done("TextHighlightColorBox", a, trace);
	return GF_OK;
}

GF_Err krok_dump(GF_Box *a, FILE * trace)
{
	u32 i;
	GF_TextKaraokeBox*p = (GF_TextKaraokeBox*)a;
	fprintf(trace, "<TextKaraokeBox highlight_starttime=\"%d\">\n", p->highlight_starttime);
	DumpBox(a, trace);
	for (i=0; i<p->nb_entries; i++) {
		fprintf(trace, "<KaraokeRecord highlight_endtime=\"%d\" start_charoffset=\"%d\" end_charoffset=\"%d\"/>\n", p->records[i].highlight_endtime, p->records[i].start_charoffset, p->records[i].end_charoffset);
	}
	gf_box_dump_done("TextKaraokeBox", a, trace);
	return GF_OK;
}
GF_Err dlay_dump(GF_Box *a, FILE * trace)
{
	GF_TextScrollDelayBox*p = (GF_TextScrollDelayBox*)a;
	fprintf(trace, "<TextScrollDelayBox scroll_delay=\"%d\">\n", p->scroll_delay);
	DumpBox(a, trace);
	gf_box_dump_done("TextScrollDelayBox", a, trace);
	return GF_OK;
}
GF_Err href_dump(GF_Box *a, FILE * trace)
{
	GF_TextHyperTextBox*p = (GF_TextHyperTextBox*)a;
	fprintf(trace, "<TextHyperTextBox startcharoffset=\"%d\" startcharoffset=\"%d\" URL=\"%s\" altString=\"%s\">\n", p->startcharoffset, p->endcharoffset, p->URL ? p->URL : "NULL", p->URL_hint ? p->URL_hint : "NULL");
	DumpBox(a, trace);
	gf_box_dump_done("TextHyperTextBox", a, trace);
	return GF_OK;
}
GF_Err tbox_dump(GF_Box *a, FILE * trace)
{
	GF_TextBoxBox*p = (GF_TextBoxBox*)a;
	fprintf(trace, "<TextBoxBox>\n");
	gpp_dump_box(trace, &p->box);
	DumpBox(a, trace);
	gf_box_dump_done("TextBoxBox", a, trace);
	return GF_OK;
}
GF_Err blnk_dump(GF_Box *a, FILE * trace)
{
	GF_TextBlinkBox*p = (GF_TextBlinkBox*)a;
	fprintf(trace, "<TextBlinkBox start_charoffset=\"%d\" end_charoffset=\"%d\">\n", p->startcharoffset, p->endcharoffset);
	DumpBox(a, trace);
	gf_box_dump_done("TextBlinkBox", a, trace);
	return GF_OK;
}
GF_Err twrp_dump(GF_Box *a, FILE * trace)
{
	GF_TextWrapBox*p = (GF_TextWrapBox*)a;
	fprintf(trace, "<TextWrapBox wrap_flag=\"%s\">\n", p->wrap_flag ? ( (p->wrap_flag>1) ? "Reserved" : "Automatic" ) : "No Wrap");
	DumpBox(a, trace);
	gf_box_dump_done("TextWrapBox", a, trace);
	return GF_OK;
}


GF_Err meta_dump(GF_Box *a, FILE * trace)
{
	GF_MetaBox *p;
	p = (GF_MetaBox *)a;
	fprintf(trace, "<MetaBox>\n");
	DumpBox(a, trace);
	gf_full_box_dump(a, trace);

	if (p->handler) gf_box_dump(p->handler, trace);
	if (p->primary_resource) gf_box_dump(p->primary_resource, trace);
	if (p->file_locations) gf_box_dump(p->file_locations, trace);
	if (p->item_locations) gf_box_dump(p->item_locations, trace);
	if (p->protections) gf_box_dump(p->protections, trace);
	if (p->item_infos) gf_box_dump(p->item_infos, trace);
	if (p->IPMP_control) gf_box_dump(p->IPMP_control, trace);
	gf_box_dump_done("MetaBox", a, trace);
	return GF_OK;
}


GF_Err xml_dump(GF_Box *a, FILE * trace)
{
	GF_XMLBox *p = (GF_XMLBox *)a;
	fprintf(trace, "<XMLBox>\n");
	DumpBox(a, trace);
	gf_full_box_dump(a, trace);
	fprintf(trace, "<![CDATA[\n");
	gf_fwrite(p->xml, p->xml_length, 1, trace);
	fprintf(trace, "]]>\n");
	gf_box_dump_done("XMLBox", a, trace);
	return GF_OK;
}


GF_Err bxml_dump(GF_Box *a, FILE * trace)
{
	GF_BinaryXMLBox *p = (GF_BinaryXMLBox *)a;
	fprintf(trace, "<BinaryXMLBox binarySize=\"%d\">\n", p->data_length);
	DumpBox(a, trace);
	gf_full_box_dump(a, trace);
	gf_box_dump_done("BinaryXMLBox", a, trace);
	return GF_OK;
}


GF_Err pitm_dump(GF_Box *a, FILE * trace)
{
	GF_PrimaryItemBox *p = (GF_PrimaryItemBox *)a;
	fprintf(trace, "<PrimaryItemBox item_ID=\"%d\">\n", p->item_ID);
	DumpBox(a, trace);
	gf_full_box_dump(a, trace);
	gf_box_dump_done("PrimaryItemBox", a, trace);
	return GF_OK;
}

GF_Err ipro_dump(GF_Box *a, FILE * trace)
{
	GF_ItemProtectionBox *p = (GF_ItemProtectionBox *)a;
	fprintf(trace, "<ItemProtectionBox>\n");
	DumpBox(a, trace);
	gf_full_box_dump(a, trace);
	gf_box_array_dump(p->protection_information, trace);
	gf_box_dump_done("ItemProtectionBox", a, trace);
	return GF_OK;
}

GF_Err infe_dump(GF_Box *a, FILE * trace)
{
	GF_ItemInfoEntryBox *p = (GF_ItemInfoEntryBox *)a;
	fprintf(trace, "<ItemInfoEntryBox item_ID=\"%d\" item_protection_index=\"%d\" item_name=\"%s\" content_type=\"%s\" content_encoding=\"%s\">\n", p->item_ID, p->item_protection_index, p->item_name, p->content_type, p->content_encoding);
	DumpBox(a, trace);
	gf_full_box_dump(a, trace);
	gf_box_dump_done("ItemInfoEntryBox", a, trace);
	return GF_OK;
}

GF_Err iinf_dump(GF_Box *a, FILE * trace)
{
	GF_ItemInfoBox *p = (GF_ItemInfoBox *)a;
	fprintf(trace, "<ItemInfoBox>\n");
	DumpBox(a, trace);
	gf_full_box_dump(a, trace);
	gf_box_array_dump(p->item_infos, trace);
	gf_box_dump_done("ItemInfoBox", a, trace);
	return GF_OK;
}

GF_Err iloc_dump(GF_Box *a, FILE * trace)
{
	u32 i, j, count, count2;
	GF_ItemLocationBox *p = (GF_ItemLocationBox*)a;
	fprintf(trace, "<ItemLocationBox offset_size=\"%d\" length_size=\"%d\" base_offset_size=\"%d\">\n", p->offset_size, p->length_size, p->base_offset_size);
	DumpBox(a, trace);
	gf_full_box_dump(a, trace);
	count = gf_list_count(p->location_entries);
	for (i=0;i<count;i++) {
		GF_ItemLocationEntry *ie = (GF_ItemLocationEntry *)gf_list_get(p->location_entries, i);
		count2 = gf_list_count(ie->extent_entries);
		fprintf(trace, "<ItemLocationEntry item_ID=\"%d\" data_reference_index=\"%d\" base_offset=\""LLD"\" />\n", ie->item_ID, ie->data_reference_index, LLD_CAST ie->base_offset);
		for (j=0; j<count2; j++) {
			GF_ItemExtentEntry *iee = (GF_ItemExtentEntry *)gf_list_get(ie->extent_entries, j);
			fprintf(trace, "<ItemExtentEntry extent_offset=\""LLD"\" extent_length=\""LLD"\" />\n", LLD_CAST iee->extent_offset, LLD_CAST iee->extent_length);
		}
	}
	gf_box_dump_done("ItemLocationBox", a, trace);
	return GF_OK;
}


GF_Err hinf_dump(GF_Box *a, FILE * trace)
{
//	GF_HintInfoBox *p  = (GF_HintInfoBox *)a;
	fprintf(trace, "<HintInfoBox>\n");
	DumpBox(a, trace);
	gf_box_dump_done("HintInfoBox", a, trace);
	return GF_OK;
}

GF_Err trpy_dump(GF_Box *a, FILE * trace)
{
	GF_TRPYBox *p = (GF_TRPYBox *)a;
	fprintf(trace, "<LargeTotalRTPBytesBox RTPBytesSent=\""LLD"\">\n", LLD_CAST p->nbBytes);
	DumpBox(a, trace);
	gf_box_dump_done("LargeTotalRTPBytesBox", a, trace);
	return GF_OK;
}

GF_Err totl_dump(GF_Box *a, FILE * trace)
{
	GF_TOTLBox *p;

	p = (GF_TOTLBox *)a;
	fprintf(trace, "<TotalRTPBytesBox RTPBytesSent=\"%d\">\n", p->nbBytes);
	DumpBox(a, trace);
	gf_box_dump_done("TotalRTPBytesBox", a, trace);
	return GF_OK;
}

GF_Err nump_dump(GF_Box *a, FILE * trace)
{
	GF_NUMPBox *p;

	p = (GF_NUMPBox *)a;
	fprintf(trace, "<LargeTotalPacketBox PacketsSent=\""LLD"\">\n", LLD_CAST p->nbPackets);
	DumpBox(a, trace);
	gf_box_dump_done("LargeTotalPacketBox", a, trace);
	return GF_OK;
}

GF_Err npck_dump(GF_Box *a, FILE * trace)
{
	GF_NPCKBox *p;

	p = (GF_NPCKBox *)a;
	fprintf(trace, "<TotalPacketBox packetsSent=\"%d\">\n", p->nbPackets);
	DumpBox(a, trace);
	gf_box_dump_done("TotalPacketBox", a, trace);
	return GF_OK;
}

GF_Err tpyl_dump(GF_Box *a, FILE * trace)
{
	GF_NTYLBox *p;

	p = (GF_NTYLBox *)a;
	fprintf(trace, "<LargeTotalMediaBytesBox BytesSent=\""LLD"\">\n", LLD_CAST p->nbBytes);
	DumpBox(a, trace);
	gf_box_dump_done("LargeTotalMediaBytesBox", a, trace);
	return GF_OK;
}

GF_Err tpay_dump(GF_Box *a, FILE * trace)
{
	GF_TPAYBox *p;

	p = (GF_TPAYBox *)a;
	fprintf(trace, "<TotalMediaBytesBox BytesSent=\"%d\">\n", p->nbBytes);
	DumpBox(a, trace);
	gf_box_dump_done("TotalMediaBytesBox", a, trace);
	return GF_OK;
}

GF_Err maxr_dump(GF_Box *a, FILE * trace)
{
	GF_MAXRBox *p;
	p = (GF_MAXRBox *)a;
	fprintf(trace, "<MaxDataRateBox MaxDataRate=\"%d\" Granularity=\"%d\">\n", p->maxDataRate, p->granularity);
	DumpBox(a, trace);
	gf_box_dump_done("MaxDataRateBox", a, trace);
	return GF_OK;
}

GF_Err dmed_dump(GF_Box *a, FILE * trace)
{
	GF_DMEDBox *p;

	p = (GF_DMEDBox *)a;
	fprintf(trace, "<BytesFromMediaTrackBox BytesSent=\""LLD"\">\n", LLD_CAST p->nbBytes);
	DumpBox(a, trace);
	gf_box_dump_done("BytesFromMediaTrackBox", a, trace);
	return GF_OK;
}

GF_Err dimm_dump(GF_Box *a, FILE * trace)
{
	GF_DIMMBox *p;

	p = (GF_DIMMBox *)a;
	fprintf(trace, "<ImmediateDataBytesBox BytesSent=\""LLD"\">\n", LLD_CAST p->nbBytes);
	DumpBox(a, trace);
	gf_box_dump_done("ImmediateDataBytesBox", a, trace);
	return GF_OK;
}

GF_Err drep_dump(GF_Box *a, FILE * trace)
{
	GF_DREPBox *p;

	p = (GF_DREPBox *)a;
	fprintf(trace, "<RepeatedDataBytesBox RepeatedBytes=\""LLD"\">\n", LLD_CAST p->nbBytes);
	DumpBox(a, trace);
	gf_box_dump_done("RepeatedDataBytesBox", a, trace);
	return GF_OK;
}

GF_Err tmin_dump(GF_Box *a, FILE * trace)
{
	GF_TMINBox *p;

	p = (GF_TMINBox *)a;
	fprintf(trace, "<MinTransmissionTimeBox MinimumTransmitTime=\"%d\">\n", p->minTime);
	DumpBox(a, trace);
	gf_box_dump_done("MinTransmissionTimeBox", a, trace);
	return GF_OK;
}

GF_Err tmax_dump(GF_Box *a, FILE * trace)
{
	GF_TMAXBox *p;

	p = (GF_TMAXBox *)a;
	fprintf(trace, "<MaxTransmissionTimeBox MaximumTransmitTime=\"%d\">\n", p->maxTime);
	DumpBox(a, trace);
	gf_box_dump_done("MaxTransmissionTimeBox", a, trace);
	return GF_OK;
}

GF_Err pmax_dump(GF_Box *a, FILE * trace)
{
	GF_PMAXBox *p;

	p = (GF_PMAXBox *)a;
	fprintf(trace, "<MaxPacketSizeBox MaximumSize=\"%d\">\n", p->maxSize);
	DumpBox(a, trace);
	gf_box_dump_done("MaxPacketSizeBox", a, trace);
	return GF_OK;
}

GF_Err dmax_dump(GF_Box *a, FILE * trace)
{
	GF_DMAXBox *p;

	p = (GF_DMAXBox *)a;
	fprintf(trace, "<MaxPacketDurationBox MaximumDuration=\"%d\">\n", p->maxDur);
	DumpBox(a, trace);
	gf_box_dump_done("MaxPacketDurationBox", a, trace);
	return GF_OK;
}

GF_Err payt_dump(GF_Box *a, FILE * trace)
{
	GF_PAYTBox *p;

	p = (GF_PAYTBox *)a;
	fprintf(trace, "<PayloadTypeBox PayloadID=\"%d\" PayloadString=\"%s\">\n", p->payloadCode, p->payloadString);
	DumpBox(a, trace);
	gf_box_dump_done("PayloadTypeBox", a, trace);
	return GF_OK;
}

GF_Err name_dump(GF_Box *a, FILE * trace)
{
	GF_NameBox *p;

	p = (GF_NameBox *)a;
	fprintf(trace, "<NameBox Name=\"%s\">\n", p->string);
	DumpBox(a, trace);
	gf_box_dump_done("NameBox", a, trace);
	return GF_OK;
}

GF_Err rely_dump(GF_Box *a, FILE * trace)
{
	GF_RelyHintBox *p;

	p = (GF_RelyHintBox *)a;
	fprintf(trace, "<RelyTransmissionBox Prefered=\"%d\" required=\"%d\">\n", p->prefered, p->required);
	DumpBox(a, trace);
	gf_box_dump_done("RelyTransmissionBox", a, trace);
	return GF_OK;
}

GF_Err snro_dump(GF_Box *a, FILE * trace)
{
	GF_SeqOffHintEntryBox *p;

	p = (GF_SeqOffHintEntryBox *)a;
	fprintf(trace, "<PacketSequenceOffsetBox SeqNumOffset=\"%d\">\n", p->SeqOffset);
	DumpBox(a, trace);
	gf_box_dump_done("PacketSequenceOffsetBox", a, trace);
	return GF_OK;
}

GF_Err tims_dump(GF_Box *a, FILE * trace)
{
	GF_TSHintEntryBox *p;

	p = (GF_TSHintEntryBox *)a;
	fprintf(trace, "<RTPTimeScaleBox TimeScale=\"%d\">\n", p->timeScale);
	DumpBox(a, trace);
	gf_box_dump_done("RTPTimeScaleBox", a, trace);
	return GF_OK;
}

GF_Err tsro_dump(GF_Box *a, FILE * trace)
{
	GF_TimeOffHintEntryBox *p;

	p = (GF_TimeOffHintEntryBox *)a;
	fprintf(trace, "<TimeStampOffsetBox TimeStampOffset=\"%d\">\n", p->TimeOffset);
	DumpBox(a, trace);
	gf_box_dump_done("TimeStampOffsetBox", a, trace);
	return GF_OK;
}


GF_Err ghnt_dump(GF_Box *a, FILE * trace)
{
	GF_HintSampleEntryBox *p;

	p = (GF_HintSampleEntryBox *)a;
	fprintf(trace, "<GenericHintSampleEntryBox EntrySubType=\"%s\" DataReferenceIndex=\"%d\" HintTrackVersion=\"%d\" LastCompatibleVersion=\"%d\" MaxPacketSize=\"%d\">\n",
		gf_4cc_to_str(p->type), p->dataReferenceIndex, p->HintTrackVersion, p->LastCompatibleVersion, p->MaxPacketSize);

	DumpBox(a, trace);
	gf_box_array_dump(p->HintDataTable, trace);
	gf_box_dump_done("GenericHintSampleEntryBox", a, trace);
	return GF_OK;
}

GF_Err hnti_dump(GF_Box *a, FILE * trace)
{
	GF_HintTrackInfoBox *p;
	GF_Box *ptr;
	GF_RTPBox *rtp;
	u32 i;

	p = (GF_HintTrackInfoBox *)a;
	fprintf(trace, "<HintTrackInfoBox>\n");
	DumpBox(a, trace);

	i=0;
	while ((ptr = (GF_Box *)gf_list_enum(p->other_boxes, &i))) {
		if (ptr->type !=GF_ISOM_BOX_TYPE_RTP) {
			gf_box_dump(ptr, trace);
		} else {
			rtp = (GF_RTPBox *)ptr;
			fprintf(trace, "<RTPInfoBox subType=\"%s\">\n", gf_4cc_to_str(rtp->subType));
			fprintf(trace, "<!-- sdp text: %s -->\n", rtp->sdpText);
			gf_box_dump_done("RTPInfoBox", a, trace);
		}
	}
	gf_box_dump_done("HintTrackInfoBox", NULL, trace);
	return GF_OK;
}

GF_Err sdp_dump(GF_Box *a, FILE * trace)
{
	GF_SDPBox *p;

	p = (GF_SDPBox *)a;
	fprintf(trace, "<SDPBox>\n");
	DumpBox(a, trace);
	fprintf(trace, "<!-- sdp text: %s -->\n", p->sdpText);
	gf_box_dump_done("SDPBox", a, trace);
	return GF_OK;
}

GF_Err rtpo_dump(GF_Box *a, FILE * trace)
{
	GF_RTPOBox *p;

	p = (GF_RTPOBox *)a;
	fprintf(trace, "<RTPTimeOffsetBox PacketTimeOffset=\"%d\">\n", p->timeOffset);
	DumpBox(a, trace);
	gf_box_dump_done("RTPTimeOffsetBox", a, trace);
	return GF_OK;
}



#ifndef	GPAC_DISABLE_ISOM_FRAGMENTS

GF_Err mvex_dump(GF_Box *a, FILE * trace)
{
	GF_MovieExtendsBox *p;

	p = (GF_MovieExtendsBox *)a;
	fprintf(trace, "<MovieExtendsBox>\n");
	DumpBox(a, trace);
	if (p->mehd) gf_box_dump(p->mehd, trace);
	gf_box_array_dump(p->TrackExList, trace);
	gf_box_dump_done("MovieExtendsBox", a, trace);
	return GF_OK;
}

GF_Err mehd_dump(GF_Box *a, FILE * trace)
{
	GF_MovieExtendsHeaderBox *p = (GF_MovieExtendsHeaderBox*)a;
	fprintf(trace, "<MovieExtendsHeaderBox fragmentDuration=\""LLD"\" >\n", LLD_CAST p->fragment_duration);
	DumpBox(a, trace);
	gf_full_box_dump(a, trace);
	gf_box_dump_done("MovieExtendsHeaderBox", a, trace);
	return GF_OK;
}

void sample_flags_dump(const char *name, u32 sample_flags, FILE * trace)
{
	fprintf(trace, "<%s", name);
	fprintf(trace, " IsLeading=\"%d\"", GF_ISOM_GET_FRAG_LEAD(sample_flags) );
	fprintf(trace, " SampleDependsOn=\"%d\"", GF_ISOM_GET_FRAG_DEPENDS(sample_flags) );
	fprintf(trace, " SampleIsDependedOn=\"%d\"", GF_ISOM_GET_FRAG_DEPENDED(sample_flags) );
	fprintf(trace, " SampleHasRedundancy=\"%d\"", GF_ISOM_GET_FRAG_REDUNDANT(sample_flags) );
	fprintf(trace, " SamplePadding=\"%d\"", GF_ISOM_GET_FRAG_PAD(sample_flags) );
	fprintf(trace, " SampleSync=\"%d\"", GF_ISOM_GET_FRAG_SYNC(sample_flags));
	fprintf(trace, " SampleDegradationPriority=\"%d\"", GF_ISOM_GET_FRAG_DEG(sample_flags));
	fprintf(trace, "/>\n");
}

GF_Err trex_dump(GF_Box *a, FILE * trace)
{
	GF_TrackExtendsBox *p;

	p = (GF_TrackExtendsBox *)a;
	fprintf(trace, "<TrackExtendsBox TrackID=\"%d\"", p->trackID);

	fprintf(trace, " SampleDescriptionIndex=\"%d\" SampleDuration=\"%d\" SampleSize=\"%d\"", p->def_sample_desc_index, p->def_sample_duration, p->def_sample_size);
	fprintf(trace, ">\n");
	sample_flags_dump("DefaultSampleFlags", p->def_sample_flags, trace);
	DumpBox(a, trace);
	gf_full_box_dump(a, trace);
	gf_box_dump_done("TrackExtendsBox", a, trace);
	return GF_OK;
}

GF_Err moof_dump(GF_Box *a, FILE * trace)
{
	GF_MovieFragmentBox *p;

	p = (GF_MovieFragmentBox *)a;
	fprintf(trace, "<MovieFragmentBox TrackFragments=\"%d\">\n", gf_list_count(p->TrackList));
	DumpBox(a, trace);

	if (p->mfhd) gf_box_dump(p->mfhd, trace);
	gf_box_array_dump(p->TrackList, trace);
	gf_box_dump_done("MovieFragmentBox", a, trace);
	return GF_OK;
}

GF_Err mfhd_dump(GF_Box *a, FILE * trace)
{
	GF_MovieFragmentHeaderBox *p;

	p = (GF_MovieFragmentHeaderBox *)a;
	fprintf(trace, "<MovieFragmentHeaderBox FragmentSequenceNumber=\"%d\">\n", p->sequence_number);
	DumpBox(a, trace);
	gf_full_box_dump(a, trace);
	gf_box_dump_done("MovieFragmentHeaderBox", a, trace);
	return GF_OK;
}

GF_Err traf_dump(GF_Box *a, FILE * trace)
{
	GF_TrackFragmentBox *p;

	p = (GF_TrackFragmentBox *)a;
	fprintf(trace, "<TrackFragmentBox>\n");
	DumpBox(a, trace);
	if (p->tfhd) gf_box_dump(p->tfhd, trace);
	if (p->subs) gf_box_dump(p->subs, trace);
	if (p->sdtp) gf_box_dump(p->sdtp, trace);
	if (p->tfdt) gf_box_dump(p->tfdt, trace);
	if (p->sampleGroupsDescription) gf_box_array_dump(p->sampleGroupsDescription, trace);
	if (p->sampleGroups) gf_box_array_dump(p->sampleGroups, trace);
	gf_box_array_dump(p->TrackRuns, trace);
	if (p->sai_sizes) gf_box_array_dump(p->sai_sizes, trace);
	if (p->sai_offsets) gf_box_array_dump(p->sai_offsets, trace);
	if (p->piff_sample_encryption) gf_box_dump(p->piff_sample_encryption, trace);
	if (p->sample_encryption) gf_box_dump(p->sample_encryption, trace);
	gf_box_dump_done("TrackFragmentBox", a, trace);
	return GF_OK;
}

static void frag_dump_sample_flags(FILE * trace, u32 flags)
{
	fprintf(trace, " SamplePadding=\"%d\" Sync=\"%d\" DegradationPriority=\"%d\" IsLeading=\"%d\" DependsOn=\"%d\" IsDependedOn=\"%d\" HasRedundancy=\"%d\"",
		GF_ISOM_GET_FRAG_PAD(flags), GF_ISOM_GET_FRAG_SYNC(flags), GF_ISOM_GET_FRAG_DEG(flags), 
		GF_ISOM_GET_FRAG_LEAD(flags), GF_ISOM_GET_FRAG_DEPENDS(flags), GF_ISOM_GET_FRAG_DEPENDED(flags), GF_ISOM_GET_FRAG_REDUNDANT(flags));
}


GF_Err tfhd_dump(GF_Box *a, FILE * trace)
{
	GF_TrackFragmentHeaderBox *p;

	p = (GF_TrackFragmentHeaderBox *)a;
	fprintf(trace, "<TrackFragmentHeaderBox TrackID=\"%d\"", p->trackID);

	if (p->flags & GF_ISOM_TRAF_BASE_OFFSET) {
		fprintf(trace, " BaseDataOffset=\""LLU"\"", p->base_data_offset);
	} else {
		fprintf(trace, " BaseDataOffset=\"%s\"", (p->flags & GF_ISOM_MOOF_BASE_OFFSET) ? "moof" : "moof-or-previous-traf");
	}

	if (p->flags & GF_ISOM_TRAF_SAMPLE_DESC)
		fprintf(trace, " SampleDescriptionIndex=\"%d\"", p->sample_desc_index);
	if (p->flags & GF_ISOM_TRAF_SAMPLE_DUR)
		fprintf(trace, " SampleDuration=\"%d\"", p->def_sample_duration);
	if (p->flags & GF_ISOM_TRAF_SAMPLE_SIZE)
		fprintf(trace, " SampleSize=\"%d\"", p->def_sample_size);

	if (p->flags & GF_ISOM_TRAF_SAMPLE_FLAGS) {
		frag_dump_sample_flags(trace, p->def_sample_flags);
	}

	fprintf(trace, ">\n");

	DumpBox(a, trace);
	gf_full_box_dump(a, trace);
	gf_box_dump_done("TrackFragmentHeaderBox", a, trace);
	return GF_OK;
}

GF_Err trun_dump(GF_Box *a, FILE * trace)
{
	u32 i;
	GF_TrunEntry *ent;
	GF_TrackFragmentRunBox *p;

	p = (GF_TrackFragmentRunBox *)a;
	fprintf(trace, "<TrackRunBox SampleCount=\"%d\"", p->sample_count);

	if (p->flags & GF_ISOM_TRUN_DATA_OFFSET)
		fprintf(trace, " DataOffset=\"%d\"", p->data_offset);
	fprintf(trace, ">\n");

	if (p->flags & GF_ISOM_TRUN_FIRST_FLAG) {
		sample_flags_dump("FirstSampleFlags", p->first_sample_flags, trace);
	}
	DumpBox(a, trace);
	gf_full_box_dump(a, trace);

	if (p->flags & (GF_ISOM_TRUN_DURATION|GF_ISOM_TRUN_SIZE|GF_ISOM_TRUN_CTS_OFFSET|GF_ISOM_TRUN_FLAGS)) {
		i=0;
		while ((ent = (GF_TrunEntry *)gf_list_enum(p->entries, &i))) {

			fprintf(trace, "<TrackRunEntry");

			if (p->flags & GF_ISOM_TRUN_DURATION)
				fprintf(trace, " Duration=\"%d\"", ent->Duration);
			if (p->flags & GF_ISOM_TRUN_SIZE)
				fprintf(trace, " Size=\"%d\"", ent->size);
			if (p->flags & GF_ISOM_TRUN_CTS_OFFSET)
				fprintf(trace, " CTSOffset=\"%d\"", ent->CTS_Offset);

			if (p->flags & GF_ISOM_TRUN_FLAGS) {
				frag_dump_sample_flags(trace, ent->flags);
			}
			fprintf(trace, "/>\n");
		}
	} else {
		fprintf(trace, "<!-- all default values used -->\n");
	}
	gf_box_dump_done("TrackRunBox", a, trace);
	return GF_OK;
}

#endif

#ifndef GPAC_DISABLE_ISOM_HINTING

GF_Err DTE_Dump(GF_List *dte, FILE * trace)
{
	GF_GenericDTE *p;
	GF_ImmediateDTE *i_p;
	GF_SampleDTE *s_p;
	GF_StreamDescDTE *sd_p;
	u32 i, count;

	count = gf_list_count(dte);
	for (i=0; i<count; i++) {
		p = (GF_GenericDTE *)gf_list_get(dte, i);
		switch (p->source) {
		case 0:
			fprintf(trace, "<EmptyDataEntry/>\n");
			break;
		case 1:
			i_p = (GF_ImmediateDTE *) p;
			fprintf(trace, "<ImmediateDataEntry DataSize=\"%d\"/>\n", i_p->dataLength);
			break;
		case 2:
			s_p = (GF_SampleDTE *) p;
			fprintf(trace, "<SampleDataEntry DataSize=\"%d\" SampleOffset=\"%d\" SampleNumber=\"%d\" TrackReference=\"%d\"/>\n",
				s_p->dataLength, s_p->byteOffset, s_p->sampleNumber, s_p->trackRefIndex);
			break;
		case 3:
			sd_p = (GF_StreamDescDTE *) p;
			fprintf(trace, "<SampleDescriptionEntry DataSize=\"%d\" DescriptionOffset=\"%d\" StreamDescriptionindex=\"%d\" TrackReference=\"%d\"/>\n",
				sd_p->dataLength, sd_p->byteOffset, sd_p->streamDescIndex, sd_p->trackRefIndex);
			break;
		default:
			fprintf(trace, "<UnknownTableEntry/>\n");
			break;
		}
	}
	return GF_OK;
}


GF_EXPORT
GF_Err gf_isom_dump_hint_sample(GF_ISOFile *the_file, u32 trackNumber, u32 SampleNum, FILE * trace)
{
	GF_ISOSample *tmp;
	GF_HintSampleEntryBox *entry;
	u32 descIndex, count, count2, i;
	GF_Err e;
	GF_BitStream *bs;
	GF_HintSample *s;
	GF_TrackBox *trak;
	GF_RTPPacket *pck;

	trak = gf_isom_get_track_from_file(the_file, trackNumber);
	if (!trak || !IsHintTrack(trak)) return GF_BAD_PARAM;

	tmp = gf_isom_get_sample(the_file, trackNumber, SampleNum, &descIndex);
	if (!tmp) return GF_BAD_PARAM;

	e = Media_GetSampleDesc(trak->Media, descIndex, (GF_SampleEntryBox **) &entry, &count);
	if (e) {
		gf_isom_sample_del(&tmp);
		return e;
	}

	//check we can read the sample
	switch (entry->type) {
	case GF_ISOM_BOX_TYPE_RTP_STSD:
		break;
	default:
		gf_isom_sample_del(&tmp);
		return GF_NOT_SUPPORTED;
	}

	bs = gf_bs_new(tmp->data, tmp->dataLength, GF_BITSTREAM_READ);
	s = gf_isom_hint_sample_new(entry->type);
	gf_isom_hint_sample_read(s, bs, tmp->dataLength);
	gf_bs_del(bs);

	count = gf_list_count(s->packetTable);

	fprintf(trace, "<RTPHintSample SampleNumber=\"%d\" DecodingTime=\""LLD"\" RandomAccessPoint=\"%d\" PacketCount=\"%u\">\n", SampleNum, LLD_CAST tmp->DTS, tmp->IsRAP, count);

	for (i=0; i<count; i++) {
		pck = (GF_RTPPacket *)gf_list_get(s->packetTable, i);

		fprintf(trace, "<RTPHintPacket PacketNumber=\"%d\" P=\"%d\" X=\"%d\" M=\"%d\" PayloadType=\"%d\"",
			i+1,  pck->P_bit, pck->X_bit, pck->M_bit, pck->payloadType);

		fprintf(trace, " SequenceNumber=\"%d\" RepeatedPacket=\"%d\" DropablePacket=\"%d\" RelativeTransmissionTime=\"%d\" FullPacketSize=\"%d\">\n",
			pck->SequenceNumber, pck->R_bit, pck->B_bit, pck->relativeTransTime, gf_isom_hint_rtp_length(pck));


		//TLV is made of Boxes
		count2 = gf_list_count(pck->TLV);
		if (count2) {
			fprintf(trace, "<PrivateExtensionTable EntryCount=\"%d\">\n", count2);
			gf_box_array_dump(pck->TLV, trace);
			fprintf(trace, "</PrivateExtensionTable>\n");
		}
		//DTE is made of NON boxes
		count2 = gf_list_count(pck->DataTable);
		if (count2) {
			fprintf(trace, "<PacketDataTable EntryCount=\"%d\">\n", count2);
			DTE_Dump(pck->DataTable, trace);
			fprintf(trace, "</PacketDataTable>\n");
		}
		fprintf(trace, "</RTPHintPacket>\n");
	}

	fprintf(trace, "</RTPHintSample>\n");
	gf_isom_sample_del(&tmp);
	gf_isom_hint_sample_del(s);
	return GF_OK;
}

#endif /*GPAC_DISABLE_ISOM_HINTING*/

static void gpp_dump_box_nobox(FILE * trace, GF_BoxRecord *rec)
{
	fprintf(trace, "<TextBox top=\"%d\" left=\"%d\" bottom=\"%d\" right=\"%d\"/>\n", rec->top, rec->left, rec->bottom, rec->right);
}

static void gpp_print_char_offsets(FILE * trace, u32 start, u32 end, u32 *shift_offset, u32 so_count)
{
	u32 i;
	if (shift_offset) {
		for (i=0; i<so_count; i++) {
			if (start>shift_offset[i]) {
				start --;
				break;
			}
		}
		for (i=0; i<so_count; i++) {
			if (end>shift_offset[i]) {
				end --;
				break;
			}
		}
	}
	if (start || end) fprintf(trace, "fromChar=\"%d\" toChar=\"%d\" ", start, end);
}

static void gpp_dump_style_nobox(FILE * trace, GF_StyleRecord *rec, u32 *shift_offset, u32 so_count)
{
	fprintf(trace, "<Style ");
	if (rec->startCharOffset || rec->endCharOffset)
		gpp_print_char_offsets(trace, rec->startCharOffset, rec->endCharOffset, shift_offset, so_count);

	fprintf(trace, "styles=\"");
	if (!rec->style_flags) {
		fprintf(trace, "Normal");
	} else {
		if (rec->style_flags & 1) fprintf(trace, "Bold ");
		if (rec->style_flags & 2) fprintf(trace, "Italic ");
		if (rec->style_flags & 4) fprintf(trace, "Underlined ");
	}
	fprintf(trace, "\" fontID=\"%d\" fontSize=\"%d\" ", rec->fontID, rec->font_size);
	gpp_dump_rgba8(trace, "color", rec->text_color);
	fprintf(trace, "/>\n");
}

static char *ttd_format_time(u64 ts, u32 timescale, char *szDur, Bool is_srt)
{
	u32 h, m, s, ms;
	ts = (u32) (ts*1000 / timescale);
	h = (u32) (ts / 3600000);
	m = (u32) (ts/ 60000) - h*60;
	s = (u32) (ts/1000) - h*3600 - m*60;
	ms = (u32) (ts) - h*3600000 - m*60000 - s*1000;
	if (is_srt) {
		sprintf(szDur, "%02d:%02d:%02d,%03d", h, m, s, ms);
	} else {
		sprintf(szDur, "%02d:%02d:%02d.%03d", h, m, s, ms);
	}
	return szDur;
}

//#define DUMP_OLD_TEXT

static GF_Err gf_isom_dump_ttxt_track(GF_ISOFile *the_file, u32 track, FILE *dump)
{
	u32 i, j, count, di, nb_descs, shift_offset[20], so_count;
	u64 last_DTS;
	size_t len;
	GF_Box *a;
	Bool has_scroll;
	char szDur[100];
	GF_Tx3gSampleEntryBox *txt;

	GF_TrackBox *trak = gf_isom_get_track_from_file(the_file, track);
	if (!trak) return GF_BAD_PARAM;
	switch (trak->Media->handler->handlerType) {
	case GF_ISOM_MEDIA_TEXT:
	case GF_ISOM_MEDIA_SUBT:
		break;
	default:
		return GF_BAD_PARAM;
	}

	txt = (GF_Tx3gSampleEntryBox *)gf_list_get(trak->Media->information->sampleTable->SampleDescription->other_boxes, 0);
	switch (txt->type) {
	case GF_ISOM_BOX_TYPE_TX3G:
		break;
	case GF_ISOM_BOX_TYPE_TEXT:
	default:
		return GF_BAD_PARAM;
	}

	fprintf(dump, "<?xml version=\"1.0\" encoding=\"UTF-8\" ?>\n");
	fprintf(dump, "<!-- GPAC 3GPP Text Stream -->\n");

#ifdef DUMP_OLD_TEXT
	fprintf(dump, "<TextStream version=\"1.0\">\n");
#else
	fprintf(dump, "<TextStream version=\"1.1\">\n");
#endif
	fprintf(dump, "<TextStreamHeader width=\"%d\" height=\"%d\" layer=\"%d\" translation_x=\"%d\" translation_y=\"%d\">\n", trak->Header->width >> 16 , trak->Header->height >> 16, trak->Header->layer, trak->Header->matrix[6] >> 16, trak->Header->matrix[7] >> 16);

	nb_descs = gf_list_count(trak->Media->information->sampleTable->SampleDescription->other_boxes);
	for (i=0; i<nb_descs; i++) {
		GF_Tx3gSampleEntryBox *txt = (GF_Tx3gSampleEntryBox *)gf_list_get(trak->Media->information->sampleTable->SampleDescription->other_boxes, i);

		fprintf(dump, "<TextSampleDescription horizontalJustification=\"");
		switch (txt->horizontal_justification) {
		case 1: fprintf(dump, "center"); break;
		case -1: fprintf(dump, "right"); break;
		default: fprintf(dump, "left"); break;
		}
		fprintf(dump, "\" verticalJustification=\"");
		switch (txt->vertical_justification) {
		case 1: fprintf(dump, "center"); break;
		case -1: fprintf(dump, "bottom"); break;
		default: fprintf(dump, "top"); break;
		}
		fprintf(dump, "\" ");
		gpp_dump_rgba8(dump, "backColor", txt->back_color);
		fprintf(dump, " verticalText=\"%s\"", (txt->displayFlags & GF_TXT_VERTICAL) ? "yes" : "no");
		fprintf(dump, " fillTextRegion=\"%s\"", (txt->displayFlags & GF_TXT_FILL_REGION) ? "yes" : "no");
		fprintf(dump, " continuousKaraoke=\"%s\"", (txt->displayFlags & GF_TXT_KARAOKE) ? "yes" : "no");
		has_scroll = 0;
		if (txt->displayFlags & GF_TXT_SCROLL_IN) {
			has_scroll = 1;
			if (txt->displayFlags & GF_TXT_SCROLL_OUT) fprintf(dump, " scroll=\"InOut\"");
			else fprintf(dump, " scroll=\"In\"");
		} else if (txt->displayFlags & GF_TXT_SCROLL_OUT) {
			has_scroll = 1;
			fprintf(dump, " scroll=\"Out\"");
		} else {
			fprintf(dump, " scroll=\"None\"");
		}
		if (has_scroll) {
			u32 mode = (txt->displayFlags & GF_TXT_SCROLL_DIRECTION)>>7;
			switch (mode) {
			case GF_TXT_SCROLL_CREDITS: fprintf(dump, " scrollMode=\"Credits\""); break;
			case GF_TXT_SCROLL_MARQUEE: fprintf(dump, " scrollMode=\"Marquee\""); break;
			case GF_TXT_SCROLL_DOWN: fprintf(dump, " scrollMode=\"Down\""); break;
			case GF_TXT_SCROLL_RIGHT: fprintf(dump, " scrollMode=\"Right\""); break;
			default: fprintf(dump, " scrollMode=\"Unknown\""); break;
			}
		}
		fprintf(dump, ">\n");
		fprintf(dump, "<FontTable>\n");
		if (txt->font_table) {
			for (j=0; j<txt->font_table->entry_count; j++) {
				fprintf(dump, "<FontTableEntry fontName=\"%s\" fontID=\"%d\"/>\n", txt->font_table->fonts[j].fontName, txt->font_table->fonts[j].fontID);

			}
		}
		fprintf(dump, "</FontTable>\n");
		if ((txt->default_box.bottom == txt->default_box.top) || (txt->default_box.right == txt->default_box.left)) {
			txt->default_box.top = txt->default_box.left = 0;
			txt->default_box.right = trak->Header->width / 65536;
			txt->default_box.bottom = trak->Header->height / 65536;
		}
		gpp_dump_box_nobox(dump, &txt->default_box);
		gpp_dump_style_nobox(dump, &txt->default_style, NULL, 0);
		fprintf(dump, "</TextSampleDescription>\n");
	}
	fprintf(dump, "</TextStreamHeader>\n");

	last_DTS = 0;
	count = gf_isom_get_sample_count(the_file, track);
	for (i=0; i<count; i++) {
		GF_BitStream *bs;
		GF_TextSample *txt;
		GF_ISOSample *s = gf_isom_get_sample(the_file, track, i+1, &di);
		if (!s) continue;

		fprintf(dump, "<TextSample sampleTime=\"%s\"", ttd_format_time(s->DTS, trak->Media->mediaHeader->timeScale, szDur, 0));
		if (nb_descs>1) fprintf(dump, " sampleDescriptionIndex=\"%d\"", di);

		bs = gf_bs_new(s->data, s->dataLength, GF_BITSTREAM_READ);
		txt = gf_isom_parse_texte_sample(bs);
		gf_bs_del(bs);


		if (txt->highlight_color) {
			fprintf(dump, " ");
			gpp_dump_rgba8(dump, "highlightColor", txt->highlight_color->hil_color);
		}
		if (txt->scroll_delay) {
			Double delay = txt->scroll_delay->scroll_delay;
			delay /= trak->Media->mediaHeader->timeScale;
			fprintf(dump, " scrollDelay=\"%g\"", delay);
		}
		if (txt->wrap) fprintf(dump, " wrap=\"%s\"", (txt->wrap->wrap_flag==0x01) ? "Automatic" : "None");

		so_count = 0;

#ifndef DUMP_OLD_TEXT
		fprintf(dump, " xml:space=\"preserve\">");
#endif
		if (!txt->len) {
#ifdef DUMP_OLD_TEXT
			fprintf(dump, " text=\"\"");
#endif
			last_DTS = (u32) trak->Media->mediaHeader->duration;
		} else {
			unsigned short utf16Line[10000];
			last_DTS = s->DTS;
			/*UTF16*/
			if ((txt->len>2) && ((unsigned char) txt->text[0] == (unsigned char) 0xFE) && ((unsigned char) txt->text[1] == (unsigned char) 0xFF)) {
				/*copy 2 more chars because the lib always add 2 '0' at the end for UTF16 end of string*/
				memcpy((char *) utf16Line, txt->text+2, sizeof(char) * (txt->len));
				len = gf_utf8_wcslen((const u16*)utf16Line);
			} else {
				char *str;
				str = txt->text;
				len = gf_utf8_mbstowcs((u16*)utf16Line, 10000, (const char **) &str);
			}
			if (len != (size_t) -1) {
				utf16Line[len] = 0;
#ifdef DUMP_OLD_TEXT
				fprintf(dump, " text=\"\'");
#endif
				for (j=0; j<len; j++) {
					if ((utf16Line[j]=='\n') || (utf16Line[j]=='\r') || (utf16Line[j]==0x85) || (utf16Line[j]==0x2028) || (utf16Line[j]==0x2029) ) {
#ifndef DUMP_OLD_TEXT
						fprintf(dump, "\n");
#else
						fprintf(dump, "\'\'");
#endif
						if ((utf16Line[j]=='\r') && (utf16Line[j+1]=='\n')) {
							shift_offset[so_count] = j;
							so_count++;
							j++;
						}
					}
					else {
						switch (utf16Line[j]) {
						case '\'': fprintf(dump, "&apos;"); break;
						case '\"': fprintf(dump, "&quot;"); break;
						case '&': fprintf(dump, "&amp;"); break;
						case '>': fprintf(dump, "&gt;"); break;
						case '<': fprintf(dump, "&lt;"); break;
						default:
							if (utf16Line[j] < 128) {
								fprintf(dump, "%c", (u8) utf16Line[j]);
							} else {
								fprintf(dump, "&#%d;", utf16Line[j]);
							}
							break;
						}
					}
				}
#ifdef DUMP_OLD_TEXT
				fprintf(dump, "\'\"");
#endif
			} else {
#ifdef DUMP_OLD_TEXT
				fprintf(dump, "text=\"%s\"", txt->text);
#endif
			}
		}

#ifdef DUMP_OLD_TEXT
		fprintf(dump, ">\n");
#endif

		if (txt->box) gpp_dump_box_nobox(dump, &txt->box->box);
		if (txt->styles) {
			for (j=0; j<txt->styles->entry_count; j++) {
				gpp_dump_style_nobox(dump, &txt->styles->styles[j], shift_offset, so_count);
			}
		}
		j=0;
		while ((a = (GF_Box *)gf_list_enum(txt->others, &j))) {
			switch (a->type) {
			case GF_ISOM_BOX_TYPE_HLIT:
				fprintf(dump, "<Highlight ");
				gpp_print_char_offsets(dump, ((GF_TextHighlightBox *)a)->startcharoffset, ((GF_TextHighlightBox *)a)->endcharoffset, shift_offset, so_count);
				fprintf(dump, "/>\n");
				break;
			case GF_ISOM_BOX_TYPE_HREF:
			{
				GF_TextHyperTextBox *ht = (GF_TextHyperTextBox *)a;
				fprintf(dump, "<HyperLink ");
				gpp_print_char_offsets(dump, ht->startcharoffset, ht->endcharoffset, shift_offset, so_count);
				fprintf(dump, "URL=\"%s\" URLToolTip=\"%s\"/>\n", ht->URL ? ht->URL : "", ht->URL_hint ? ht->URL_hint : "");
			}
				break;
			case GF_ISOM_BOX_TYPE_BLNK:
				fprintf(dump, "<Blinking ");
				gpp_print_char_offsets(dump, ((GF_TextBlinkBox *)a)->startcharoffset, ((GF_TextBlinkBox *)a)->endcharoffset, shift_offset, so_count);
				fprintf(dump, "/>\n");
				break;
			case GF_ISOM_BOX_TYPE_KROK:
			{
				u32 k;
				Double t;
				GF_TextKaraokeBox *krok = (GF_TextKaraokeBox *)a;
				t = krok->highlight_starttime;
				t /= trak->Media->mediaHeader->timeScale;
				fprintf(dump, "<Karaoke startTime=\"%g\">\n", t);
				for (k=0; k<krok->nb_entries; k++) {
					t = krok->records[k].highlight_endtime;
					t /= trak->Media->mediaHeader->timeScale;
					fprintf(dump, "<KaraokeRange ");
					gpp_print_char_offsets(dump, krok->records[k].start_charoffset, krok->records[k].end_charoffset, shift_offset, so_count);
					fprintf(dump, "endTime=\"%g\"/>\n", t);
				}
				fprintf(dump, "</Karaoke>\n");
			}
				break;
			}
		}

		fprintf(dump, "</TextSample>\n");
		gf_isom_sample_del(&s);
		gf_isom_delete_text_sample(txt);
		gf_set_progress("TTXT Extract", i, count);
	}
	if (last_DTS < trak->Media->mediaHeader->duration) {
		fprintf(dump, "<TextSample sampleTime=\"%s\" text=\"\" />\n", ttd_format_time(trak->Media->mediaHeader->duration, trak->Media->mediaHeader->timeScale, szDur, 0));
	}

	fprintf(dump, "</TextStream>\n");
	if (count) gf_set_progress("TTXT Extract", count, count);
	return GF_OK;
}

static GF_Err gf_isom_dump_srt_track(GF_ISOFile *the_file, u32 track, FILE *dump)
{
	u32 i, j, k, count, di, len, ts, cur_frame;
	u64 start, end;
	GF_Tx3gSampleEntryBox *txtd;
	GF_BitStream *bs;
	char szDur[100];

	GF_TrackBox *trak = gf_isom_get_track_from_file(the_file, track);
	if (!trak) return GF_BAD_PARAM;
	switch (trak->Media->handler->handlerType) {
	case GF_ISOM_MEDIA_TEXT:
	case GF_ISOM_MEDIA_SUBT:
		break;
	default:
		return GF_BAD_PARAM;
	}

	ts = trak->Media->mediaHeader->timeScale;
	cur_frame = 0;
	start = end = 0;

	count = gf_isom_get_sample_count(the_file, track);
	for (i=0; i<count; i++) {
		GF_TextSample *txt;
		GF_ISOSample *s = gf_isom_get_sample(the_file, track, i+1, &di);
		if (!s) continue;

		start = s->DTS;
		if (s->dataLength==2) {
			gf_isom_sample_del(&s);
			continue;
		}
		if (i+1<count) {
			GF_ISOSample *next = gf_isom_get_sample_info(the_file, track, i+2, NULL, NULL);
			if (next) {
				end = next->DTS;
				gf_isom_sample_del(&next);
			}
		} else {
			end = gf_isom_get_media_duration(the_file, track) ;
		}
		cur_frame++;
		fprintf(dump, "%d\n", cur_frame);
		ttd_format_time(start, ts, szDur, 1);
		fprintf(dump, "%s --> ", szDur);
		ttd_format_time(end, ts, szDur, 1);
		fprintf(dump, "%s\n", szDur);

		bs = gf_bs_new(s->data, s->dataLength, GF_BITSTREAM_READ);
		txt = gf_isom_parse_texte_sample(bs);
		gf_bs_del(bs);

		txtd = (GF_Tx3gSampleEntryBox *)gf_list_get(trak->Media->information->sampleTable->SampleDescription->other_boxes, di-1);

		if (!txt->len) {
			fprintf(dump, "\n");
		}else {
			u32 styles, char_num, new_styles;
			u16 utf16Line[10000];

			/*UTF16*/
			if ((txt->len>2) && ((unsigned char) txt->text[0] == (unsigned char) 0xFE) && ((unsigned char) txt->text[1] == (unsigned char) 0xFF)) {
				memcpy(utf16Line, txt->text+2, sizeof(char)*txt->len);
				( ((char *)utf16Line)[txt->len] ) = 0;
				len = txt->len;
			} else {
				u8 *str = (u8 *) (txt->text);
				size_t res = gf_utf8_mbstowcs(utf16Line, 10000, (const char **) &str);
				if (res==(size_t)-1) return GF_NON_COMPLIANT_BITSTREAM;
				len = (u32) res;
				utf16Line[len] = 0;
			}
            char_num = 0;
            styles = 0;
            new_styles = txtd->default_style.style_flags;
            for (j=0; j<len; j++) {
                Bool is_new_line;

                if (txt->styles) {
                    new_styles = txtd->default_style.style_flags;
                    for (k=0; k<txt->styles->entry_count; k++) {
                        if (txt->styles->styles[k].startCharOffset>char_num) continue;
                        if (txt->styles->styles[k].endCharOffset<char_num+1) continue;

                        if (txt->styles->styles[k].style_flags & (GF_TXT_STYLE_ITALIC | GF_TXT_STYLE_BOLD | GF_TXT_STYLE_UNDERLINED)) {
                            new_styles = txt->styles->styles[k].style_flags;
                            break;
                        }
                    }
                }
                if (new_styles != styles) {
                    if ((new_styles & GF_TXT_STYLE_BOLD) && !(styles & GF_TXT_STYLE_BOLD)) fprintf(dump, "<b>");
                    if ((new_styles & GF_TXT_STYLE_ITALIC) && !(styles & GF_TXT_STYLE_ITALIC)) fprintf(dump, "<i>");
                    if ((new_styles & GF_TXT_STYLE_UNDERLINED) && !(styles & GF_TXT_STYLE_UNDERLINED)) fprintf(dump, "<u>");

                    if ((styles & GF_TXT_STYLE_BOLD) && !(new_styles & GF_TXT_STYLE_BOLD)) fprintf(dump, "</b>");
                    if ((styles & GF_TXT_STYLE_ITALIC) && !(new_styles & GF_TXT_STYLE_ITALIC)) fprintf(dump, "</i>");
                    if ((styles & GF_TXT_STYLE_UNDERLINED) && !(new_styles & GF_TXT_STYLE_UNDERLINED)) fprintf(dump, "</u>");

                    styles = new_styles;
                }

                /*not sure if styles must be reseted at line breaks in srt...*/
                is_new_line = 0;
                if ((utf16Line[j]=='\n') || (utf16Line[j]=='\r') ) {
                    if ((utf16Line[j]=='\r') && (utf16Line[j+1]=='\n')) j++;
                    fprintf(dump, "\n");
                    is_new_line = 1;
                }

                if (!is_new_line) {
                    size_t sl;
                    char szChar[30];
                    s16 swT[2], *swz;
                    swT[0] = utf16Line[j];
                    swT[1] = 0;
                    swz= (s16 *)swT;
                    sl = gf_utf8_wcstombs(szChar, 30, (const unsigned short **) &swz);
                    if (sl == (size_t)-1) sl=0;
					szChar[(u32) sl]=0;
                    fprintf(dump, "%s", szChar);
                }
                char_num++;
            }
            new_styles = 0;
            if (new_styles != styles) {
                if ((new_styles & GF_TXT_STYLE_BOLD) && !(styles & GF_TXT_STYLE_BOLD)) fprintf(dump, "<b>");
                if ((new_styles & GF_TXT_STYLE_ITALIC) && !(styles & GF_TXT_STYLE_ITALIC)) fprintf(dump, "<i>");
                if ((new_styles & GF_TXT_STYLE_UNDERLINED) && !(styles & GF_TXT_STYLE_UNDERLINED)) fprintf(dump, "<u>");

                if ((styles & GF_TXT_STYLE_BOLD) && !(new_styles & GF_TXT_STYLE_BOLD)) fprintf(dump, "</b>");
                if ((styles & GF_TXT_STYLE_ITALIC) && !(new_styles & GF_TXT_STYLE_ITALIC)) fprintf(dump, "</i>");
                if ((styles & GF_TXT_STYLE_UNDERLINED) && !(new_styles & GF_TXT_STYLE_UNDERLINED)) fprintf(dump, "</u>");

                styles = new_styles;
            }
            fprintf(dump, "\n");
		}
		gf_isom_sample_del(&s);
		gf_isom_delete_text_sample(txt);
		fprintf(dump, "\n");
		gf_set_progress("SRT Extract", i, count);
	}
	if (count) gf_set_progress("SRT Extract", i, count);
	return GF_OK;
}

static GF_Err gf_isom_dump_svg_track(GF_ISOFile *the_file, u32 track, FILE *dump)
{
	char nhmlFileName[1024];
	FILE *nhmlFile;
	u32 i, count, di, ts, cur_frame;
	u64 start, end;
	GF_BitStream *bs;

	GF_TrackBox *trak = gf_isom_get_track_from_file(the_file, track);
	if (!trak) return GF_BAD_PARAM;
	switch (trak->Media->handler->handlerType) {
	case GF_ISOM_MEDIA_TEXT:
	case GF_ISOM_MEDIA_SUBT:
		break;
	default:
		return GF_BAD_PARAM;
	}

	strcpy(nhmlFileName, the_file->fileName);
	strcat(nhmlFileName, ".nhml");
	nhmlFile = gf_f64_open(nhmlFileName, "wt");
	fprintf(nhmlFile, "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n");
	fprintf(nhmlFile, "<NHNTStream streamType=\"3\" objectTypeIndication=\"10\" timeScale=\"%d\" baseMediaFile=\"file.svg\" inRootOD=\"yes\">\n", trak->Media->mediaHeader->timeScale);
	fprintf(nhmlFile, "<NHNTSample isRAP=\"yes\" DTS=\"0\" xmlFrom=\"doc.start\" xmlTo=\"text_1.start\"/>\n");

	ts = trak->Media->mediaHeader->timeScale;
	cur_frame = 0;
	start = end = 0;

	fprintf(dump, "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n");
	fprintf(dump, "<svg version=\"1.2\" baseProfile=\"tiny\" xmlns=\"http://www.w3.org/2000/svg\" xmlns:xlink=\"http://www.w3.org/1999/xlink\" width=\"%d\" height=\"%d\" fill=\"black\">\n", trak->Header->width >> 16 , trak->Header->height >> 16);
	fprintf(dump, "<g transform=\"translate(%d, %d)\" text-anchor=\"middle\">\n", (trak->Header->width >> 16)/2 , (trak->Header->height >> 16)/2);

	count = gf_isom_get_sample_count(the_file, track);
	for (i=0; i<count; i++) {
		GF_TextSample *txt;
		GF_ISOSample *s = gf_isom_get_sample(the_file, track, i+1, &di);
		if (!s) continue;

		start = s->DTS;
		if (s->dataLength==2) {
			gf_isom_sample_del(&s);
			continue;
		}
		if (i+1<count) {
			GF_ISOSample *next = gf_isom_get_sample_info(the_file, track, i+2, NULL, NULL);
			if (next) {
				end = next->DTS;
				gf_isom_sample_del(&next);
			}
		}

		cur_frame++;
		bs = gf_bs_new(s->data, s->dataLength, GF_BITSTREAM_READ);
		txt = gf_isom_parse_texte_sample(bs);
		gf_bs_del(bs);

		if (!txt->len) continue;

		fprintf(dump, " <text id=\"text_%d\" display=\"none\">%s\n", cur_frame, txt->text);
		fprintf(dump, "  <set attributeName=\"display\" to=\"inline\" begin=\"%g\" end=\"%g\"/>\n", ((s64)start*1.0)/ts, ((s64)end*1.0)/ts);
		fprintf(dump, "  <discard begin=\"%g\"/>\n", ((s64)end*1.0)/ts);
		fprintf(dump, " </text>\n");
		gf_isom_sample_del(&s);
		gf_isom_delete_text_sample(txt);
		fprintf(dump, "\n");
		gf_set_progress("SRT Extract", i, count);

		if (i == count - 2) {
			fprintf(nhmlFile, "<NHNTSample isRAP=\"no\" DTS=\"%f\" xmlFrom=\"text_%d.start\" xmlTo=\"doc.end\"/>\n", ((s64)start*1.0), cur_frame);
		} else {
			fprintf(nhmlFile, "<NHNTSample isRAP=\"no\" DTS=\"%f\" xmlFrom=\"text_%d.start\" xmlTo=\"text_%d.start\"/>\n", ((s64)start*1.0), cur_frame, cur_frame+1);
		}

	}
	fprintf(dump, "</g>\n");
	fprintf(dump, "</svg>\n");

	fprintf(nhmlFile, "</NHNTStream>\n");
	fclose(nhmlFile);

	if (count) gf_set_progress("SRT Extract", i, count);
	return GF_OK;
}

GF_EXPORT
GF_Err gf_isom_text_dump(GF_ISOFile *the_file, u32 track, FILE *dump, u32 dump_type)
{
	switch (dump_type) {
	case 2:
		return gf_isom_dump_svg_track(the_file, track, dump);
	case 1:
		return gf_isom_dump_srt_track(the_file, track, dump);
	default:
		return gf_isom_dump_ttxt_track(the_file, track, dump);
	}
}


/* ISMA 1.0 Encryption and Authentication V 1.0  dump */
GF_Err sinf_dump(GF_Box *a, FILE * trace)
{
	GF_ProtectionInfoBox *p;
	p = (GF_ProtectionInfoBox *)a;
	fprintf(trace, "<ProtectionInfoBox>\n");
	DumpBox(a, trace);

	gf_box_dump(p->original_format, trace);
	gf_box_dump(p->scheme_type, trace);
	gf_box_dump(p->info, trace);
	gf_box_dump_done("ProtectionInfoBox", a, trace);
	return GF_OK;
}

GF_Err frma_dump(GF_Box *a, FILE * trace)
{
	GF_OriginalFormatBox *p;
	p = (GF_OriginalFormatBox *)a;
	fprintf(trace, "<OriginalFormatBox data_format=\"%s\">\n", gf_4cc_to_str(p->data_format));
	DumpBox(a, trace);
	gf_box_dump_done("OriginalFormatBox", a, trace);
	return GF_OK;
}

GF_Err schm_dump(GF_Box *a, FILE * trace)
{
	GF_SchemeTypeBox *p;
	p = (GF_SchemeTypeBox *)a;
	fprintf(trace, "<SchemeTypeBox scheme_type=\"%s\" scheme_version=\"%d\" ", gf_4cc_to_str(p->scheme_type), p->scheme_version);
	if (p->URI) fprintf(trace, "scheme_uri=\"%s\"", p->URI);
	fprintf(trace, ">\n");

	DumpBox(a, trace);
	gf_full_box_dump(a, trace);
	gf_box_dump_done("SchemeTypeBox", a, trace);
	return GF_OK;
}

GF_Err schi_dump(GF_Box *a, FILE * trace)
{
	GF_SchemeInformationBox *p;
	p = (GF_SchemeInformationBox *)a;
	fprintf(trace, "<SchemeInformationBox>\n");
	DumpBox(a, trace);
	if (p->ikms) gf_box_dump(p->ikms, trace);
	if (p->isfm) gf_box_dump(p->isfm, trace);
	if (p->okms) gf_box_dump(p->okms, trace);
	if (p->tenc) gf_box_dump(p->tenc, trace);
	gf_box_dump_done("SchemeInformationBox", a, trace);
	return GF_OK;
}

GF_Err iKMS_dump(GF_Box *a, FILE * trace)
{
	GF_ISMAKMSBox *p;
	p = (GF_ISMAKMSBox *)a;
	fprintf(trace, "<KMSBox kms_URI=\"%s\">\n", p->URI);
	DumpBox(a, trace);
	gf_full_box_dump(a, trace);

	gf_box_dump_done("KMSBox", a, trace);
	return GF_OK;

}

GF_Err iSFM_dump(GF_Box *a, FILE * trace)
{
	GF_ISMASampleFormatBox *p;
	const char *name = (a->type==GF_ISOM_BOX_TYPE_ISFM) ? "ISMASampleFormat" : "OMADRMAUFormatBox";
	p = (GF_ISMASampleFormatBox *)a;
	fprintf(trace, "<%s selective_encryption=\"%d\" key_indicator_length=\"%d\" IV_length=\"%d\">\n", name, p->selective_encryption, p->key_indicator_length, p->IV_length);
	DumpBox(a, trace);
	gf_full_box_dump(a, trace);

	gf_box_dump_done(NULL, a, trace);
	fprintf(trace, "</%s>\n", name);
	return GF_OK;
}

static void dump_data(FILE *trace, char *name, char *data, u32 data_size)
{
	u32 i;
	fprintf(trace, "%s=\"0x", name);
	for (i=0; i<data_size; i++) fprintf(trace, "%02X", (unsigned char) data[i]);
	fprintf(trace, "\" ");
}

GF_EXPORT
GF_Err gf_isom_dump_ismacryp_protection(GF_ISOFile *the_file, u32 trackNumber, FILE * trace)
{
	u32 i, count;
	GF_SampleEntryBox *entry;
	GF_Err e;
	GF_TrackBox *trak;

	trak = gf_isom_get_track_from_file(the_file, trackNumber);
	if (!trak) return GF_BAD_PARAM;


	fprintf(trace, "<ISMACrypSampleDescriptions>\n");
	count = gf_isom_get_sample_description_count(the_file, trackNumber);
	for (i=0; i<count; i++) {
		e = Media_GetSampleDesc(trak->Media, i+1, (GF_SampleEntryBox **) &entry, NULL);
		if (e) return e;

		switch (entry->type) {
		case GF_ISOM_BOX_TYPE_ENCA:
		case GF_ISOM_BOX_TYPE_ENCV:
		case GF_ISOM_BOX_TYPE_ENCT:
		case GF_ISOM_BOX_TYPE_ENCS:
			break;
		default:
			continue;
		}
		gf_box_dump(entry, trace);
	}
	fprintf(trace, "</ISMACrypSampleDescriptions>\n");
	return GF_OK;
}

GF_EXPORT
GF_Err gf_isom_dump_ismacryp_sample(GF_ISOFile *the_file, u32 trackNumber, u32 SampleNum, FILE * trace)
{
	GF_ISOSample *samp;
	GF_ISMASample  *isma_samp;
	u32 descIndex;

	samp = gf_isom_get_sample(the_file, trackNumber, SampleNum, &descIndex);
	if (!samp) return GF_BAD_PARAM;

	isma_samp = gf_isom_get_ismacryp_sample(the_file, trackNumber, samp, descIndex);
	if (!isma_samp) {
		gf_isom_sample_del(&samp);
		return GF_NOT_SUPPORTED;
	}

	fprintf(trace, "<ISMACrypSample SampleNumber=\"%d\" DataSize=\"%d\" CompositionTime=\""LLD"\" ", SampleNum, isma_samp->dataLength, LLD_CAST (samp->DTS+samp->CTS_Offset) );
	if (samp->CTS_Offset) fprintf(trace, "DecodingTime=\""LLD"\" ", LLD_CAST samp->DTS);
	if (gf_isom_has_sync_points(the_file, trackNumber)) fprintf(trace, "RandomAccessPoint=\"%s\" ", samp->IsRAP ? "Yes" : "No");
	fprintf(trace, "IsEncrypted=\"%s\" ", (isma_samp->flags & GF_ISOM_ISMA_IS_ENCRYPTED) ? "Yes" : "No");
	if (isma_samp->flags & GF_ISOM_ISMA_IS_ENCRYPTED) {
		fprintf(trace, "IV=\""LLD"\" ", LLD_CAST isma_samp->IV);
		if (isma_samp->key_indicator) dump_data(trace, "KeyIndicator", (char*)isma_samp->key_indicator, isma_samp->KI_length);
	}
	fprintf(trace, "/>\n");

	gf_isom_sample_del(&samp);
	gf_isom_ismacryp_delete_sample(isma_samp);
	return GF_OK;
}

/* end of ISMA 1.0 Encryption and Authentication V 1.0 */


/* Apple extensions */

static GF_Err apple_tag_dump(GF_Box *a, FILE * trace)
{
	GF_BitStream *bs;
	u32 val;
	Bool no_dump = 0;
	char *name = "Unknown";
	GF_ListItemBox *itune = (GF_ListItemBox *)a;
	switch (itune->type) {
	case GF_ISOM_BOX_TYPE_0xA9NAM: name = "Name"; break;
	case GF_ISOM_BOX_TYPE_0xA9CMT: name = "Comment"; break;
	case GF_ISOM_BOX_TYPE_0xA9DAY: name = "Created"; break;
	case GF_ISOM_BOX_TYPE_0xA9ART: name = "Artist"; break;
	case GF_ISOM_BOX_TYPE_0xA9TRK: name = "Track"; break;
	case GF_ISOM_BOX_TYPE_0xA9ALB: name = "Album"; break;
	case GF_ISOM_BOX_TYPE_0xA9COM: name = "Compositor"; break;
	case GF_ISOM_BOX_TYPE_0xA9WRT: name = "Writer"; break;
	case GF_ISOM_BOX_TYPE_0xA9TOO: name = "Tool"; break;
	case GF_ISOM_BOX_TYPE_0xA9CPY: name = "Copyright"; break;
	case GF_ISOM_BOX_TYPE_0xA9DES: name = "Description"; break;
	case GF_ISOM_BOX_TYPE_0xA9GEN:
	case GF_ISOM_BOX_TYPE_GNRE:
		name = "Genre"; break;
	case GF_ISOM_BOX_TYPE_aART: name = "AlbumArtist"; break;
	case GF_ISOM_BOX_TYPE_PGAP: name = "Gapeless"; break;
	case GF_ISOM_BOX_TYPE_DISK: name = "Disk"; break;
	case GF_ISOM_BOX_TYPE_TRKN: name = "TrackNumber"; break;
	case GF_ISOM_BOX_TYPE_TMPO: name = "Tempo"; break;
	case GF_ISOM_BOX_TYPE_CPIL: name = "Compilation"; break;
	case GF_ISOM_BOX_TYPE_COVR: name = "CoverArt"; no_dump = 1; break;
	case GF_ISOM_BOX_TYPE_iTunesSpecificInfo: name = "iTunesSpecific"; no_dump = 1; break;
	case GF_ISOM_BOX_TYPE_0xA9GRP: name = "Group"; break;
	case GF_ISOM_ITUNE_ENCODER: name = "Encoder"; break;
	}
	fprintf(trace, "<%sBox", name);
	if (!no_dump) {
		switch (itune->type) {
		case GF_ISOM_BOX_TYPE_DISK:
		case GF_ISOM_BOX_TYPE_TRKN:
			bs = gf_bs_new(itune->data->data, itune->data->dataSize, GF_BITSTREAM_READ);
			gf_bs_read_int(bs, 16);
			val = gf_bs_read_int(bs, 16);
			if (itune->type==GF_ISOM_BOX_TYPE_DISK) {
				fprintf(trace, " DiskNumber=\"%d\" NbDisks=\"%d\" ", val, gf_bs_read_int(bs, 16) );
			} else {
				fprintf(trace, " TrackNumber=\"%d\" NbTracks=\"%d\" ", val, gf_bs_read_int(bs, 16) );
			}
			gf_bs_del(bs);
			break;
		case GF_ISOM_BOX_TYPE_TMPO:
			bs = gf_bs_new(itune->data->data, itune->data->dataSize, GF_BITSTREAM_READ);
			fprintf(trace, " BPM=\"%d\" ", gf_bs_read_int(bs, 16) );
			gf_bs_del(bs);
			break;
		case GF_ISOM_BOX_TYPE_CPIL:
			fprintf(trace, " IsCompilation=\"%s\" ", itune->data->data[0] ? "yes" : "no");
			break;
		case GF_ISOM_BOX_TYPE_PGAP:
			fprintf(trace, " IsGapeless=\"%s\" ", itune->data->data[0] ? "yes" : "no");
			break;
		default:
			if (strcmp(name, "Unknown") && itune->data->data) {
				if (itune->data && itune->data->data[0]) {
					fprintf(trace, " value=\"%s\" ", itune->data->data);
				} else {
					fprintf(trace, " value=\"");
					DumpData(trace, itune->data->data, itune->data->dataSize);
					fprintf(trace, "\" ");
				}
			}
			break;
		}
	}
	fprintf(trace, ">\n");
	if (itune->data)
		gf_full_box_dump((GF_Box *)itune->data, trace);
	DumpBox(a, trace);
	gf_box_dump_done(NULL, a, trace);
	fprintf(trace, "</%sBox>\n", name);
	return GF_OK;
}

#ifndef GPAC_DISABLE_ISOM_ADOBE

GF_Err abst_dump(GF_Box *a, FILE * trace)
{
	u32 i;
	GF_AdobeBootstrapInfoBox *p = (GF_AdobeBootstrapInfoBox*)a;
	fprintf(trace, "<AdobeBootstrapBox BootstrapinfoVersion=\"%u\" Profile=\"%u\" Live=\"%u\" Update=\"%u\" TimeScale=\"%u\" CurrentMediaTime=\""LLU"\" SmpteTimeCodeOffset=\""LLU"\" ",
		p->bootstrapinfo_version, p->profile, p->live, p->update, p->time_scale, p->current_media_time, p->smpte_time_code_offset);
	if (p->movie_identifier)
		fprintf(trace, "MovieIdentifier=\"%s\" ", p->movie_identifier);
	if (p->drm_data)
		fprintf(trace, "DrmData=\"%s\" ", p->drm_data);
	if (p->meta_data)
		fprintf(trace, "MetaData=\"%s\" ", p->meta_data);
	fprintf(trace, ">\n");
	DumpBox(a, trace);
	gf_full_box_dump(a, trace);

	for (i=0; i<p->server_entry_count; i++) {
		char *str = (char*)gf_list_get(p->server_entry_table, i);
		fprintf(trace, "<ServerEntry>%s</ServerEntry>\n", str);
	}

	for (i=0; i<p->quality_entry_count; i++) {
		char *str = (char*)gf_list_get(p->quality_entry_table, i);
		fprintf(trace, "<QualityEntry>%s</QualityEntry>\n", str);
	}

	for (i=0; i<p->segment_run_table_count; i++)
		gf_box_dump(gf_list_get(p->segment_run_table_entries, i), trace);

	for (i=0; i<p->fragment_run_table_count; i++)
		gf_box_dump(gf_list_get(p->fragment_run_table_entries, i), trace);

	gf_box_dump_done("AdobeBootstrapBox", a, trace);
	return GF_OK;
}

GF_Err afra_dump(GF_Box *a, FILE * trace)
{
	u32 i;
	GF_AdobeFragRandomAccessBox *p = (GF_AdobeFragRandomAccessBox*)a;
	fprintf(trace, "<AdobeFragmentRandomAccessBox LongIDs=\"%u\" LongOffsets=\"%u\" TimeScale=\"%u\">\n", p->long_ids, p->long_offsets, p->time_scale);
	DumpBox(a, trace);
	gf_full_box_dump(a, trace);

	for (i=0; i<p->entry_count; i++) {
		GF_AfraEntry *ae = (GF_AfraEntry *)gf_list_get(p->local_access_entries, i);
		fprintf(trace, "<LocalAccessEntry Time=\""LLU"\" Offset=\""LLU"\"/>\n", ae->time, ae->offset);
	}

	for (i=0; i<p->global_entry_count; i++) {
		GF_GlobalAfraEntry *gae = (GF_GlobalAfraEntry *)gf_list_get(p->global_access_entries, i);
		fprintf(trace, "<GlobalAccessEntry Time=\""LLU"\" Segment=\"%u\" Fragment=\"%u\" AfraOffset=\""LLU"\" OffsetFromAfra=\""LLU"\"/>\n",
				gae->time, gae->segment, gae->fragment, gae->afra_offset, gae->offset_from_afra);
	}

	gf_box_dump_done("AdobeFragmentRandomAccessBox", a, trace);
	return GF_OK;
}

GF_Err afrt_dump(GF_Box *a, FILE * trace)
{
	u32 i;
	GF_AdobeFragmentRunTableBox *p = (GF_AdobeFragmentRunTableBox*)a;
	fprintf(trace, "<AdobeFragmentRunTableBox TimeScale=\"%u\">\n", p->timescale);
	DumpBox(a, trace);
	gf_full_box_dump(a, trace);

	for (i=0; i<p->quality_entry_count; i++) {
		char *str = (char*)gf_list_get(p->quality_segment_url_modifiers, i);
		fprintf(trace, "<QualityEntry>%s</QualityEntry>\n", str);
	}

	for (i=0; i<p->fragment_run_entry_count; i++) {
		GF_AdobeFragmentRunEntry *fre = (GF_AdobeFragmentRunEntry *)gf_list_get(p->fragment_run_entry_table, i);
		fprintf(trace, "<FragmentRunEntry FirstFragment=\"%u\" FirstFragmentTimestamp=\""LLU"\" FirstFragmentDuration=\"%u\"", fre->first_fragment, fre->first_fragment_timestamp, fre->fragment_duration);
		if (!fre->fragment_duration)
			fprintf(trace, " DiscontinuityIndicator=\"%u\"", fre->discontinuity_indicator);
		fprintf(trace, "/>\n");
	}

	gf_box_dump_done("AdobeFragmentRunTableBox", a, trace);
	return GF_OK;
}

GF_Err asrt_dump(GF_Box *a, FILE * trace)
{
	u32 i;
	GF_AdobeSegmentRunTableBox *p = (GF_AdobeSegmentRunTableBox*)a;
	fprintf(trace, "<AdobeSegmentRunTableBox>\n");
	DumpBox(a, trace);
	gf_full_box_dump(a, trace);
	
	for (i=0; i<p->quality_entry_count; i++) {
		char *str = (char*)gf_list_get(p->quality_segment_url_modifiers, i);
		fprintf(trace, "<QualityEntry>%s</QualityEntry>\n", str);
	}

	for (i=0; i<p->segment_run_entry_count; i++) {
		GF_AdobeSegmentRunEntry *sre = (GF_AdobeSegmentRunEntry *)gf_list_get(p->segment_run_entry_table, i);
		fprintf(trace, "<SegmentRunEntry FirstSegment=\"%u\" FragmentsPerSegment=\"%u\"/>\n", sre->first_segment, sre->fragment_per_segment);
	}

	gf_box_dump_done("AdobeSegmentRunTableBox", a, trace);
	return GF_OK;
}

#endif /*GPAC_DISABLE_ISOM_ADOBE*/

GF_Err ilst_dump(GF_Box *a, FILE * trace)
{
	u32 i;
	GF_Box *tag;
	GF_Err e;
	GF_ItemListBox *ptr;
	ptr = (GF_ItemListBox *)a;
	fprintf(trace, "<ItemListBox>\n");
	DumpBox(a, trace);

	i=0;
	while ( (tag = gf_list_enum(ptr->other_boxes, &i))) {
		e = apple_tag_dump(tag, trace);
		if(e) return e;
	}
	gf_box_dump_done("ItemListBox", NULL, trace);
	return GF_OK;
}

GF_Err ListEntry_dump(GF_Box *a, FILE * trace)
{
	fprintf(trace, "<ListEntry>\n");
	DumpBox(a, trace);
	gf_box_dump(a, trace);
	fprintf(trace, "</ListEntry>\n");
	return GF_OK;
}

GF_Err data_dump(GF_Box *a, FILE * trace)
{
	fprintf(trace, "<data>\n");
	DumpBox(a, trace);
	gf_full_box_dump(a, trace);
	fprintf(trace, "</data>\n");
	return GF_OK;
}

GF_Err ohdr_dump(GF_Box *a, FILE * trace)
{
	GF_OMADRMCommonHeaderBox *ptr = (GF_OMADRMCommonHeaderBox *)a;
	fprintf(trace, "<OMADRMCommonHeaderBox EncryptionMethod=\"%d\" PaddingScheme=\"%d\" PlaintextLength=\""LLD"\" ",
						ptr->EncryptionMethod, ptr->PaddingScheme, ptr->PlaintextLength);
	if (ptr->RightsIssuerURL) fprintf(trace, "RightsIssuerURL=\"%s\" ", ptr->RightsIssuerURL);
	if (ptr->ContentID) fprintf(trace, "ContentID=\"%s\" ", ptr->ContentID);
	if (ptr->TextualHeaders) {
		u32 i, offset;
		char *start = ptr->TextualHeaders;
		fprintf(trace, "TextualHeaders=\"");
		i=offset=0;
		while (i<ptr->TextualHeadersLen) {
			if (start[i]==0) {
				fprintf(trace, "%s ", start+offset);
				offset=i+1;
			}
			i++;
		}
		fprintf(trace, "%s\"  ", start+offset);
	}

	fprintf(trace, ">\n");
	gf_full_box_dump((GF_Box *)a, trace);
	gf_box_dump_done("OMADRMCommonHeaderBox", a, trace);
	return GF_OK;
}
GF_Err grpi_dump(GF_Box *a, FILE * trace)
{
	GF_OMADRMGroupIDBox *ptr = (GF_OMADRMGroupIDBox *)a;
	fprintf(trace, "<OMADRMGroupIDBox GroupID=\"%s\" EncryptionMethod=\"%d\" GroupKey=\" ", ptr->GroupID, ptr->GKEncryptionMethod);
	DumpData(trace, ptr->GroupKey, ptr->GKLength);
	fprintf(trace, ">\n");
	gf_full_box_dump((GF_Box *)a, trace);
	gf_box_dump_done("OMADRMGroupIDBox", a, trace);
	return GF_OK;
}
GF_Err mdri_dump(GF_Box *a, FILE * trace)
{
	//GF_OMADRMMutableInformationBox *ptr = (GF_OMADRMMutableInformationBox*)a;
	fprintf(trace, "<OMADRMMutableInformationBox>\n");
	gf_box_dump((GF_Box *)a, trace);
	gf_box_dump_done("OMADRMMutableInformationBox", a, trace);
	return GF_OK;
}
GF_Err odtt_dump(GF_Box *a, FILE * trace)
{
	GF_OMADRMTransactionTrackingBox *ptr = (GF_OMADRMTransactionTrackingBox *)a;
	fprintf(trace, "<OMADRMTransactionTrackingBox TransactionID=\"");
	DumpData(trace, ptr->TransactionID, 16);
	fprintf(trace, "\">\n");
	gf_full_box_dump((GF_Box *)a, trace);
	gf_box_dump_done("OMADRMTransactionTrackingBox", a, trace);
	return GF_OK;
}
GF_Err odrb_dump(GF_Box *a, FILE * trace)
{
	GF_OMADRMRightsObjectBox*ptr = (GF_OMADRMRightsObjectBox*)a;
	fprintf(trace, "<OMADRMRightsObjectBox OMARightsObject=\"");
	DumpData(trace, ptr->oma_ro, ptr->oma_ro_size);
	fprintf(trace, "\">\n");
	gf_full_box_dump((GF_Box *)a, trace);
	gf_box_dump_done("OMADRMRightsObjectBox", a, trace);
	return GF_OK;
}
GF_Err odkm_dump(GF_Box *a, FILE * trace)
{
	GF_OMADRMKMSBox *ptr = (GF_OMADRMKMSBox*)a;
	fprintf(trace, "<OMADRMKMSBox>\n");
	gf_full_box_dump((GF_Box *)a, trace);
	if (ptr->hdr) gf_box_dump((GF_Box *)ptr->hdr, trace);
	if (ptr->fmt) gf_box_dump((GF_Box *)ptr->fmt, trace);
	gf_box_dump_done("OMADRMKMSBox", a, trace);
	return GF_OK;
}


GF_Err pasp_dump(GF_Box *a, FILE * trace)
{
	GF_PixelAspectRatioBox *ptr = (GF_PixelAspectRatioBox*)a;
	fprintf(trace, "<PixelAspectRatioBox hSpacing=\"%d\" vSpacing=\"%d\" >\n", ptr->hSpacing, ptr->vSpacing);
	DumpBox((GF_Box *)a, trace);
	gf_box_dump_done("PixelAspectRatioBox", a, trace);
	return GF_OK;
}


GF_Err tsel_dump(GF_Box *a, FILE * trace)
{
	u32 i;
	GF_TrackSelectionBox *ptr = (GF_TrackSelectionBox *)a;
	fprintf(trace, "<TrackSelectionBox switchGroup=\"%d\" criteria=\"", ptr->switchGroup);
	for (i=0; i<ptr->attributeListCount;i++) {
		if (i) fprintf(trace, ";");
		fprintf(trace, "%s", gf_4cc_to_str(ptr->attributeList[i]));
	}
	fprintf(trace, "\">\n");
	gf_full_box_dump((GF_Box *)a, trace);
	gf_box_dump_done("TrackSelectionBox", a, trace);
	return GF_OK;
}

GF_Err metx_dump(GF_Box *a, FILE * trace)
{
	GF_MetaDataSampleEntryBox *ptr = (GF_MetaDataSampleEntryBox*)a;
	const char *name = (ptr->type==GF_ISOM_BOX_TYPE_METX) ? "XMLMetaDataSampleEntryBox" : "TextMetaDataSampleEntryBox";

	fprintf(trace, "<%s ", name);
	if (ptr->type==GF_ISOM_BOX_TYPE_METX) {
		fprintf(trace, "namespace=\"%s\" ", ptr->mime_type_or_namespace);
		if (ptr->xml_schema_loc) fprintf(trace, "schema_location=\"%s\" ", ptr->xml_schema_loc);
	} else {
		fprintf(trace, "mime_type=\"%s\" ", ptr->mime_type_or_namespace);
	}
	if (ptr->content_encoding) fprintf(trace, "content_encoding=\"%s\" ", ptr->content_encoding);
	fprintf(trace, ">\n");
	DumpBox(a, trace);

	if (ptr->bitrate) gf_box_dump(ptr->bitrate, trace);
	gf_box_array_dump(ptr->protections, trace);

	gf_box_dump_done(NULL, a, trace);
	fprintf(trace, "</%s>\n", name);
	return GF_OK;
}

GF_Err stse_dump(GF_Box *a, FILE * trace)
{
	GF_SimpleTextSampleEntryBox *ptr = (GF_SimpleTextSampleEntryBox*)a;
	const char *name = "SimpleTextSampleEntryBox";

	fprintf(trace, "<%s ", name);
	fprintf(trace, "mime_type=\"%s\" ", ptr->mime_type);
	if (ptr->content_encoding) fprintf(trace, "content_encoding=\"%s\" ", ptr->content_encoding);
	fprintf(trace, ">\n");
	DumpBox(a, trace);

	if (ptr->bitrate) gf_box_dump(ptr->bitrate, trace);
	if (ptr->config) gf_box_dump(ptr->config, trace);

	gf_box_dump_done(NULL, a, trace);
	fprintf(trace, "</%s>\n", name);
	return GF_OK;
}

GF_Err dims_dump(GF_Box *a, FILE * trace)
{
	GF_DIMSSampleEntryBox *p = (GF_DIMSSampleEntryBox*)a;

	fprintf(trace, "<DIMSSampleEntryBox dataReferenceIndex=\"%d\">\n", p->dataReferenceIndex);
	DumpBox(a, trace);
	if (p->config) gf_box_dump(p->config, trace);
	if (p->scripts) gf_box_dump(p->scripts, trace);
	if (p->bitrate) gf_box_dump(p->bitrate, trace);
	gf_box_array_dump(p->protections, trace);
	gf_box_dump_done("DIMSSampleEntryBox", a, trace);
	return GF_OK;
}

GF_Err diST_dump(GF_Box *a, FILE * trace)
{
	GF_DIMSScriptTypesBox *p = (GF_DIMSScriptTypesBox*)a;

	fprintf(trace, "<DIMSScriptTypesBox types=\"%s\">\n", p->content_script_types);
	DumpBox(a, trace);
	gf_box_dump_done("DIMSScriptTypesBox", a, trace);
	return GF_OK;
}
GF_Err dimC_dump(GF_Box *a, FILE * trace)
{
	GF_DIMSSceneConfigBox *p = (GF_DIMSSceneConfigBox *)a;

	fprintf(trace, "<DIMSSceneConfigBox profile=\"%d\" level=\"%d\" pathComponents=\"%d\" useFullRequestHosts=\"%d\" streamType=\"%d\" containsRedundant=\"%d\" textEncoding=\"%s\" contentEncoding=\"%s\" >\n",
		p->profile, p->level, p->pathComponents, p->fullRequestHost, p->streamType, p->containsRedundant, p->textEncoding, p->contentEncoding);
	DumpBox(a, trace);
	gf_box_dump_done("DIMSSceneConfigBox", a, trace);
	return GF_OK;
}


GF_Err dac3_dump(GF_Box *a, FILE * trace)
{
	GF_AC3ConfigBox *p = (GF_AC3ConfigBox *)a;

	if (p->cfg.is_ec3) {
		u32 i;
		fprintf(trace, "<EC3SpecificBox nb_streams=\"%d\" data_rate=\"%d\">\n", p->cfg.nb_streams, p->cfg.brcode);
		for (i=0; i<p->cfg.nb_streams; i++) {
			fprintf(trace, "<EC3StreamConfig fscod=\"%d\" bsid=\"%d\" bsmod=\"%d\" acmod=\"%d\" lfon=\"%d\" num_sub_dep=\"%d\" chan_loc=\"%d\"/>\n",
				p->cfg.streams[i].fscod, p->cfg.streams[i].bsid, p->cfg.streams[i].bsmod, p->cfg.streams[i].acmod, p->cfg.streams[i].lfon, p->cfg.streams[i].nb_dep_sub, p->cfg.streams[i].chan_loc);
		}
		a->type = GF_ISOM_BOX_TYPE_DEC3;
		DumpBox(a, trace);
		a->type = GF_ISOM_BOX_TYPE_DAC3;
		gf_box_dump_done("EC3SpecificBox", a, trace);
	} else {
		fprintf(trace, "<AC3SpecificBox fscod=\"%d\" bsid=\"%d\" bsmod=\"%d\" acmod=\"%d\" lfon=\"%d\" bit_rate_code=\"%d\">\n",
			p->cfg.streams[0].fscod, p->cfg.streams[0].bsid, p->cfg.streams[0].bsmod, p->cfg.streams[0].acmod, p->cfg.streams[0].lfon, p->cfg.brcode);
		DumpBox(a, trace);
		gf_box_dump_done("AC3SpecificBox", a, trace);
	}
	return GF_OK;
}

GF_Err ac3_dump(GF_Box *a, FILE * trace)
{
	GF_AC3SampleEntryBox *p = (GF_AC3SampleEntryBox *)a;
	if (p->is_ec3)
		fprintf(trace, "<EC3SampleEntryBox");
	else
		fprintf(trace, "<AC3SampleEntryBox");
	base_audio_entry_dump((GF_AudioSampleEntryBox *)p, trace);
	fprintf(trace, ">\n");

	if (p->is_ec3)
		a->type = GF_ISOM_BOX_TYPE_EC3;
	DumpBox(a, trace);
	if (p->is_ec3)
		a->type = GF_ISOM_BOX_TYPE_AC3;
	gf_box_dump(p->info, trace);
	gf_box_dump_done(p->is_ec3 ? "EC3SampleEntryBox" : "AC3SampleEntryBox", a, trace);
	return GF_OK;
}

GF_Err lsrc_dump(GF_Box *a, FILE * trace)
{
	GF_LASERConfigurationBox *p = (GF_LASERConfigurationBox *)a;

	fprintf(trace, "<LASeRConfigurationBox ");
	dump_data(trace, "LASeRHeader", p->hdr, p->hdr_size);
	fprintf(trace, ">");
	DumpBox(a, trace);
	gf_box_dump_done("LASeRConfigurationBox", a, trace);
	return GF_OK;
}

GF_Err lsr1_dump(GF_Box *a, FILE * trace)
{
	GF_LASeRSampleEntryBox *p = (GF_LASeRSampleEntryBox*)a;
	fprintf(trace, "<LASeRSampleEntryBox DataReferenceIndex=\"%d\">\n", p->dataReferenceIndex);
	DumpBox(a, trace);
	if (p->lsr_config) gf_box_dump(p->lsr_config, trace);
	if (p->bitrate) gf_box_dump(p->bitrate, trace);
	if (p->descr) gf_box_dump(p->descr, trace);
	gf_box_dump_done("LASeRSampleEntryBox", a, trace);
	return GF_OK;
}


GF_Err sidx_dump(GF_Box *a, FILE * trace)
{
	u32 i;
	GF_SegmentIndexBox *p = (GF_SegmentIndexBox *)a;
	fprintf(trace, "<SegmentIndexBox reference_ID=\"%d\" timescale=\"%d\" earliest_presentation_time=\""LLD"\" first_offset=\""LLD"\">\n", p->reference_ID, p->timescale, p->earliest_presentation_time, p->first_offset);
	DumpBox(a, trace);
	gf_full_box_dump(a, trace);
	
	for (i=0; i<p->nb_refs; i++) {
		fprintf(trace, "<Reference type=\"%d\" size=\"%d\" duration=\"%d\" startsWithSAP=\"%d\" SAP_type=\"%d\" SAPDeltaTime=\"%d\"/>\n", p->refs[i].reference_type, p->refs[i].reference_size, p->refs[i].subsegment_duration, p->refs[i].starts_with_SAP, p->refs[i].SAP_type, p->refs[i].SAP_delta_time);
	}
	gf_box_dump_done("SegmentIndexBox", a, trace);
	return GF_OK;
}

GF_Err pcrb_dump(GF_Box *a, FILE * trace)
{
	u32 i;
	GF_PcrInfoBox *p = (GF_PcrInfoBox *)a;
	fprintf(trace, "<MPEG2TSPCRInfoBox subsegment_count=\"%d\">\n", p->subsegment_count);
	DumpBox(a, trace);
	
	for (i=0; i<p->subsegment_count; i++) {
		fprintf(trace, "<PCRInfo PCR=\""LLU"\" />\n", p->pcr_values[i]);
	}
	gf_box_dump_done("MPEG2TSPCRInfoBox", a, trace);
	return GF_OK;
}

GF_Err subs_dump(GF_Box *a, FILE * trace)
{
	u32 entry_count, i, j;
	u16 subsample_count;
	GF_SubSampleInfoEntry *pSamp;
	GF_SubSampleEntry *pSubSamp;
	GF_SubSampleInformationBox *ptr = (GF_SubSampleInformationBox *) a;

	if (!a) return GF_BAD_PARAM;

	entry_count = gf_list_count(ptr->Samples);
	fprintf(trace, "<SubSampleInformationBox EntryCount=\"%d\">\n", entry_count);
	DumpBox(a, trace);

	for (i=0; i<entry_count; i++) {
		pSamp = (GF_SubSampleInfoEntry*) gf_list_get(ptr->Samples, i);

		subsample_count = gf_list_count(pSamp->SubSamples);

		fprintf(trace, "<SampleEntry SampleDelta=\"%d\" SubSampleCount=\"%d\">\n", pSamp->sample_delta, subsample_count);

		for (j=0; j<subsample_count; j++) {
			pSubSamp = (GF_SubSampleEntry*) gf_list_get(pSamp->SubSamples, j);	
			fprintf(trace, "<SubSample Size=\"%u\" Priority=\"%u\" Discardable=\"%d\" Reserved=\"%08X\"/>\n", pSubSamp->subsample_size, pSubSamp->subsample_priority, pSubSamp->discardable, pSubSamp->reserved);
		}
		fprintf(trace, "</SampleEntry>\n");
	} 

	gf_box_dump_done("SubSampleInformationBox", a, trace);
	return GF_OK;
}

#ifndef GPAC_DISABLE_ISOM_FRAGMENTS
GF_Err tfdt_dump(GF_Box *a, FILE * trace)
{
	GF_TFBaseMediaDecodeTimeBox *ptr = (GF_TFBaseMediaDecodeTimeBox*) a;
	if (!a) return GF_BAD_PARAM;

	fprintf(trace, "<TrackFragmentBaseMediaDecodeTimeBox baseMediaDecodeTime=\""LLD"\">\n", ptr->baseMediaDecodeTime);
	DumpBox(a, trace);
	gf_full_box_dump(a, trace);
	gf_box_dump_done("TrackFragmentBaseMediaDecodeTimeBox", a, trace);
	return GF_OK;
}
#endif /*GPAC_DISABLE_ISOM_FRAGMENTS*/

GF_Err rvcc_dump(GF_Box *a, FILE * trace)
{
	GF_RVCConfigurationBox *ptr = (GF_RVCConfigurationBox*) a;
	if (!a) return GF_BAD_PARAM;

	fprintf(trace, "<RVCConfigurationBox predefined=\"%d\"", ptr->predefined_rvc_config);
	if (! ptr->predefined_rvc_config) fprintf(trace, " rvc_meta_idx=\"%d\"", ptr->rvc_meta_idx);
	fprintf(trace, ">\n");
	DumpBox(a, trace);
	gf_box_dump_done("RVCConfigurationBox", a, trace);
	return GF_OK;
}

GF_Err sbgp_dump(GF_Box *a, FILE * trace)
{
	u32 i;
	GF_SampleGroupBox *ptr = (GF_SampleGroupBox*) a;
	if (!a) return GF_BAD_PARAM;

	fprintf(trace, "<SampleGroupBox grouping_type=\"%s\"", gf_4cc_to_str(ptr->grouping_type) );
	if (ptr->version==1) fprintf(trace, " grouping_type_parameter=\"%d\"", ptr->grouping_type_parameter);
	fprintf(trace, ">\n");
	DumpBox(a, trace);
	gf_full_box_dump((GF_Box *)a, trace);
	for (i=0; i<ptr->entry_count; i++) {
		fprintf(trace, "<SampleGroupBoxEntry sample_count=\"%d\" group_description_index=\"%d\"/>\n", ptr->sample_entries[i].sample_count, ptr->sample_entries[i].group_description_index );
	}
	gf_box_dump_done("SampleGroupBox", a, trace);
	return GF_OK;
}

GF_Err sgpd_dump(GF_Box *a, FILE * trace)
{
	u32 i;
	GF_SampleGroupDescriptionBox *ptr = (GF_SampleGroupDescriptionBox*) a;
	if (!a) return GF_BAD_PARAM;

	fprintf(trace, "<SampleGroupDescriptionBox grouping_type=\"%s\"", gf_4cc_to_str(ptr->grouping_type) );
	if (ptr->version==1) fprintf(trace, " default_length=\"%d\"", ptr->default_length);
	fprintf(trace, ">\n");
	DumpBox(a, trace);
	gf_full_box_dump((GF_Box *)a, trace);
	for (i=0; i<gf_list_count(ptr->group_descriptions); i++) {
		void *entry = gf_list_get(ptr->group_descriptions, i);
		switch (ptr->grouping_type) {
		case GF_4CC( 'r', 'o', 'l', 'l' ):
			fprintf(trace, "<RollRecoveryEntry roll_distance=\"%d\" />\n", ((GF_RollRecoveryEntry*)entry)->roll_distance );
			break;
		case GF_4CC( 'r', 'a', 'p', ' ' ):
			fprintf(trace, "<VisualRandomAccessEntry num_leading_samples_known=\"%s\"", ((GF_VisualRandomAccessEntry*)entry)->num_leading_samples_known ? "yes" : "no");
			if (((GF_VisualRandomAccessEntry*)entry)->num_leading_samples_known) fprintf(trace, " num_leading_samples=\"%d\" />", ((GF_VisualRandomAccessEntry*)entry)->num_leading_samples);
			fprintf(trace, "/>\n");
			break;
		default:
			fprintf(trace, "<DefaultSampleGroupDescriptionEntry size=\"%d\" data=\"", ((GF_DefaultSampleGroupDescriptionEntry*)entry)->length);
			DumpData(trace, (char *) ((GF_DefaultSampleGroupDescriptionEntry*)entry)->data,  ((GF_DefaultSampleGroupDescriptionEntry*)entry)->length);
			fprintf(trace, "\"/>\n");
		}
	}
	gf_box_dump_done("SampleGroupDescriptionBox", a, trace);
	return GF_OK;
}

GF_Err saiz_dump(GF_Box *a, FILE * trace)
{
	u32 i;
	GF_SampleAuxiliaryInfoSizeBox *ptr = (GF_SampleAuxiliaryInfoSizeBox*) a;
	if (!a) return GF_BAD_PARAM;

	fprintf(trace, "<SampleAuxiliaryInfoSizeBox default_sample_info_size=\"%d\" sample_count=\"%d\"", ptr->default_sample_info_size, ptr->sample_count);
	if (ptr->flags & 1) {
		if (isalnum(ptr->aux_info_type>>24)) {	
			fprintf(trace, " aux_info_type=\"%s\" aux_info_type_parameter=\"%d\"", gf_4cc_to_str(ptr->aux_info_type), ptr->aux_info_type_parameter);
		} else {
			fprintf(trace, " aux_info_type=\"%d\" aux_info_type_parameter=\"%d\"", ptr->aux_info_type, ptr->aux_info_type_parameter);
		}
	}
	fprintf(trace, ">\n");
	DumpBox(a, trace);
	gf_full_box_dump((GF_Box *)a, trace);
	if (ptr->default_sample_info_size==0) {
		for (i=0; i<ptr->sample_count; i++) {
			fprintf(trace, "<SAISize size=\"%d\" />\n", ptr->sample_info_size[i]);
		}
	}
	gf_box_dump_done("SampleAuxiliaryInfoSizeBox", a, trace);
	return GF_OK;
}

GF_Err saio_dump(GF_Box *a, FILE * trace)
{
	u32 i;
	GF_SampleAuxiliaryInfoOffsetBox *ptr = (GF_SampleAuxiliaryInfoOffsetBox*) a;
	if (!a) return GF_BAD_PARAM;

	fprintf(trace, "<SampleAuxiliaryInfoOffsetBox entry_count=\"%d\"", ptr->entry_count);
	if (ptr->flags & 1) {
		if (isalnum(ptr->aux_info_type>>24)) {	
			fprintf(trace, " aux_info_type=\"%s\" aux_info_type_parameter=\"%d\"", gf_4cc_to_str(ptr->aux_info_type), ptr->aux_info_type_parameter);
		} else {
			fprintf(trace, " aux_info_type=\"%d\" aux_info_type_parameter=\"%d\"", ptr->aux_info_type, ptr->aux_info_type_parameter);
		}
	}

	fprintf(trace, ">\n");
	DumpBox(a, trace);
	gf_full_box_dump((GF_Box *)a, trace);

	if (ptr->version==0) {
		for (i=0; i<ptr->entry_count; i++) {
			fprintf(trace, "<SAIChunkOffset offset=\"%d\"/>\n", ptr->offsets[i]);
		}
	} else {
		for (i=0; i<ptr->entry_count; i++) {
			fprintf(trace, "<SAIChunkOffset offset=\""LLD"\"/>\n", ptr->offsets_large[i]);
		}
	}
	gf_box_dump_done("SampleAuxiliaryInfoOffsetBox", a, trace);
	return GF_OK;
}

GF_Err pssh_dump(GF_Box *a, FILE * trace)
{
	GF_ProtectionSystemHeaderBox *ptr = (GF_ProtectionSystemHeaderBox*) a;
	if (!a) return GF_BAD_PARAM;

	fprintf(trace, "<ProtectionSystemHeaderBox SystemID=\"");
	DumpDataHex(trace, (char *) ptr->SystemID, 16);
	fprintf(trace, "\">\n");
	DumpBox(a, trace);
	gf_full_box_dump((GF_Box *)a, trace);

	if (ptr->KID_count) {
		u32 i;
		for (i=0; i<ptr->KID_count; i++) {
			fprintf(trace, " <PSSHKey KID=\"");
			DumpDataHex(trace, (char *) ptr->KIDs[i], 16);
			fprintf(trace, "\"/>\n");
		}
	}
	if (ptr->private_data_size) {
		fprintf(trace, " <PSSHData size=\"%d\" value=\"", ptr->private_data_size);
		DumpDataHex(trace, (char *) ptr->private_data, ptr->private_data_size);
		fprintf(trace, "\"/>\n");
	}
	gf_box_dump_done("ProtectionSystemHeaderBox", a, trace);
	return GF_OK;
}

GF_Err tenc_dump(GF_Box *a, FILE * trace)
{
	GF_TrackEncryptionBox *ptr = (GF_TrackEncryptionBox*) a;
	if (!a) return GF_BAD_PARAM;

	fprintf(trace, "<TrackEncryptionBox isEncrypted=\"%d\" IV_size=\"%d\" KID=\"", ptr->IsEncrypted, ptr->IV_size);
	DumpDataHex(trace, (char *) ptr->KID, 16);
	fprintf(trace, "\">\n");
	DumpBox(a, trace);
	gf_full_box_dump((GF_Box *)a, trace);
	gf_box_dump_done("TrackEncryptionBox", a, trace);
	return GF_OK;
}

GF_Err piff_pssh_dump(GF_Box *a, FILE * trace)
{
	GF_PIFFProtectionSystemHeaderBox *ptr = (GF_PIFFProtectionSystemHeaderBox*) a;
	if (!a) return GF_BAD_PARAM;

	fprintf(trace, "<PIFFProtectionSystemHeaderBox SystemID=\"");
	DumpDataHex(trace, (char *) ptr->SystemID, 16);
	fprintf(trace, "\" PrivateData=\"");
	DumpDataHex(trace, (char *) ptr->private_data, ptr->private_data_size);
	fprintf(trace, "\">\n");
	DumpBox(a, trace);
	fprintf(trace, "<FullBoxInfo Version=\"%d\" Flags=\"%d\"/>\n", ptr->version, ptr->flags);
	gf_box_dump_done("PIFFProtectionSystemHeaderBox", a, trace);
	return GF_OK;
}

GF_Err piff_tenc_dump(GF_Box *a, FILE * trace)
{
	GF_PIFFTrackEncryptionBox *ptr = (GF_PIFFTrackEncryptionBox*) a;
	if (!a) return GF_BAD_PARAM;

	fprintf(trace, "<PIFFTrackEncryptionBox AlgorithmID=\"%d\" IV_size=\"%d\" KID=\"", ptr->AlgorithmID, ptr->IV_size);
	DumpDataHex(trace,(char *) ptr->KID, 16);
	fprintf(trace, "\">\n");
	DumpBox(a, trace);
	fprintf(trace, "<FullBoxInfo Version=\"%d\" Flags=\"%d\"/>\n", ptr->version, ptr->flags);
	gf_box_dump_done("PIFFTrackEncryptionBox", a, trace);
	return GF_OK;
}

GF_Err piff_psec_dump(GF_Box *a, FILE * trace)
{
	u32 i, j, sample_count;
	GF_PIFFSampleEncryptionBox *ptr = (GF_PIFFSampleEncryptionBox *) a;
	if (!a) return GF_BAD_PARAM;

	sample_count = gf_list_count(ptr->samp_aux_info);
	fprintf(trace, "<PIFFSampleEncryptionBox sampleCount=\"%d\"", sample_count);
	if (ptr->flags & 1) {
		fprintf(trace, " AlgorithmID=\"%d\" IV_size=\"%d\" KID=\"", ptr->AlgorithmID, ptr->IV_size);
		DumpData(trace, (char *) ptr->KID, 16);
		fprintf(trace, "\"");
	}
	fprintf(trace, ">\n");
	DumpBox(a, trace);
	
	if (sample_count) {
		for (i=0; i<sample_count; i++) {
			GF_CENCSampleAuxInfo *cenc_sample = (GF_CENCSampleAuxInfo *)gf_list_get(ptr->samp_aux_info, i);

			if (cenc_sample) {
				if  (!strlen((char *)cenc_sample->IV)) continue;
				fprintf(trace, "<PIFFSampleEncryptionEntry IV=\"");
				DumpDataHex(trace, (char *) cenc_sample->IV, 16);
				if (ptr->flags & 0x2) {
					fprintf(trace, "\" SubsampleCount=\"%d\"", cenc_sample->subsample_count);
					fprintf(trace, ">\n");
				
					for (j=0; j<cenc_sample->subsample_count; j++) {
						fprintf(trace, "<PIFFSubSampleEncryptionEntry NumClearBytes=\"%d\" NumEncryptedBytes=\"%d\"/>\n", cenc_sample->subsamples[j].bytes_clear_data, cenc_sample->subsamples[j].bytes_encrypted_data);
					}
				}
				fprintf(trace, "</PIFFSampleEncryptionEntry>\n");
			}
		}
	}

	gf_box_dump_done("PIFFSampleEncryptionBox", a, trace);
	return GF_OK;
}

GF_Err senc_dump(GF_Box *a, FILE * trace)
{
	u32 i, j, sample_count;
	GF_SampleEncryptionBox *ptr = (GF_SampleEncryptionBox *) a;
	if (!a) return GF_BAD_PARAM;

	sample_count = gf_list_count(ptr->samp_aux_info);
	fprintf(trace, "<SampleEncryptionBox sampleCount=\"%d\">\n", sample_count);
	DumpBox(a, trace);
	//WARNING - PSEC (UUID) IS TYPECASTED TO SENC (FULL BOX) SO WE CANNOT USE USUAL FULL BOX FUNCTIONS
	fprintf(trace, "<FullBoxInfo Version=\"%d\" Flags=\"0x%X\"/>\n", ptr->version, ptr->flags);
	for (i=0; i<sample_count; i++) {
		GF_CENCSampleAuxInfo *cenc_sample = (GF_CENCSampleAuxInfo *)gf_list_get(ptr->samp_aux_info, i);

		if (cenc_sample) {
			fprintf(trace, "<SampleEncryptionEntry sampleCount=\"%d\" IV=\"", i+1);
			DumpDataHex(trace, (char *) cenc_sample->IV, 16);
			fprintf(trace, "\"");
			if (ptr->flags & 0x2) {
				fprintf(trace, " SubsampleCount=\"%d\"", cenc_sample->subsample_count);
				fprintf(trace, ">\n");
				
				for (j=0; j<cenc_sample->subsample_count; j++) {
					fprintf(trace, "<SubSampleEncryptionEntry NumClearBytes=\"%d\" NumEncryptedBytes=\"%d\"/>\n", cenc_sample->subsamples[j].bytes_clear_data, cenc_sample->subsamples[j].bytes_encrypted_data);
				}
			} else {
				fprintf(trace, ">\n");
			}
			fprintf(trace, "</SampleEncryptionEntry>\n");
		}
	}

	gf_box_dump_done("SampleEncryptionBox", a, trace);
	return GF_OK;
}

GF_Err prft_dump(GF_Box *a, FILE * trace)
{
	GF_ProducerReferenceTimeBox *ptr = (GF_ProducerReferenceTimeBox *) a;

	time_t secs = (ptr->ntp >> 32) - GF_NTP_SEC_1900_TO_1970;
	struct tm t = *gmtime(&secs);
	fprintf(trace, "<ProducerReferenceTimeBox referenceTrackID=\"%d\" timestamp=\"%d\" NTP_frac=\"%u\" UTCClock=\"%d-%02d-%02dT%02d:%02d:%02dZ\">\n", ptr->refTrackID, ptr->timestamp, ptr->ntp&0xFFFFFFFFULL, 1900+t.tm_year, t.tm_mon+1, t.tm_mday, t.tm_hour, t.tm_min, t.tm_sec);
	DumpBox(a, trace);
	gf_full_box_dump((GF_Box *)a, trace);
	gf_box_dump_done("ProducerReferenceTimeBox", a, trace);

	return GF_OK;
}

#endif /*GPAC_DISABLE_ISOM_DUMP*/

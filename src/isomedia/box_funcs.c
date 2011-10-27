/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Copyright (c) Jean Le Feuvre 2000-2005 
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

#ifndef GPAC_DISABLE_ISOM

//Add this funct to handle incomplete files...
//bytesExpected is 0 most of the time. If the file is incomplete, bytesExpected
//is the number of bytes missing to parse the box...
GF_Err gf_isom_parse_root_box(GF_Box **outBox, GF_BitStream *bs, u64 *bytesExpected, Bool progressive_mode)
{
	GF_Err ret;
	u64 start;
	//first make sure we can at least get the box size and type...
	//18 = size (int32) + type (int32)
	if (gf_bs_available(bs) < 8) {
		*bytesExpected = 8;
		return GF_ISOM_INCOMPLETE_FILE;
	}
	start = gf_bs_get_position(bs);
	ret = gf_isom_parse_box(outBox, bs);
	if (ret == GF_ISOM_INCOMPLETE_FILE) {
		*bytesExpected = (*outBox)->size;
		GF_LOG(progressive_mode ? GF_LOG_DEBUG : GF_LOG_ERROR, GF_LOG_CONTAINER, ("[iso file] Incomplete box %s\n", gf_4cc_to_str( (*outBox)->type) ));
		gf_bs_seek(bs, start);
		gf_isom_box_del(*outBox);
		*outBox = NULL;
	}
	return ret;
}


GF_Err gf_isom_parse_box_ex(GF_Box **outBox, GF_BitStream *bs, u32 parent_type)
{
	u32 type, hdr_size;
	u64 size, start, end;
	char uuid[16];
	GF_Err e;
	GF_Box *newBox;
	e = GF_OK;
	if ((bs == NULL) || (outBox == NULL) ) return GF_BAD_PARAM;
	*outBox = NULL;

	start = gf_bs_get_position(bs);

	size = (u64) gf_bs_read_u32(bs);
	hdr_size = 4;
	/*fix for some boxes found in some old hinted files*/
	if ((size >= 2) && (size <= 4)) {
		size = 4;
		type = GF_ISOM_BOX_TYPE_VOID;
	} else {
		/*now here's a bad thing: some files use size 0 for void atoms, some for "till end of file" indictaion..*/
		if (!size) {
			type = gf_bs_peek_bits(bs, 32, 0);
			if (!isalnum((type>>24)&0xFF) || !isalnum((type>>16)&0xFF) || !isalnum((type>>8)&0xFF) || !isalnum(type&0xFF)) {
				size = 4;
				type = GF_ISOM_BOX_TYPE_VOID;
			} else {
				goto proceed_box;
			}
		} else {
proceed_box:
			type = gf_bs_read_u32(bs);
			hdr_size += 4;
			/*no size means till end of file - EXCEPT FOR some old QuickTime boxes...*/
			if (type == GF_ISOM_BOX_TYPE_TOTL)
				size = 12;
			if (!size) {
				GF_LOG(GF_LOG_DEBUG, GF_LOG_CONTAINER, ("[iso file] Warning Read Box type %s size 0 reading till the end of file\n", gf_4cc_to_str(type)));
				size = gf_bs_available(bs) + 8;
			}
		}
	}
	/*handle uuid*/
	memset(uuid, 0, 16);
	if (type == GF_ISOM_BOX_TYPE_UUID ) {
		gf_bs_read_data(bs, uuid, 16);
		hdr_size += 16;
	}
	
	//handle large box
	if (size == 1) {
		size = gf_bs_read_u64(bs);
		hdr_size += 8;
	}
	GF_LOG(GF_LOG_DEBUG, GF_LOG_CONTAINER, ("[iso file] Read Box type %s size "LLD" start "LLD"\n", gf_4cc_to_str(type), LLD_CAST size, LLD_CAST start));

	if ( size < hdr_size ) {
		GF_LOG(GF_LOG_DEBUG, GF_LOG_CONTAINER, ("[iso file] Box size "LLD" less than box header size %d\n", LLD_CAST size, hdr_size));
		return GF_ISOM_INVALID_FILE;
	}

	if (parent_type && (parent_type==GF_ISOM_BOX_TYPE_TREF)) {
		newBox = gf_isom_box_new(GF_ISOM_BOX_TYPE_REFT);
		if (!newBox) return GF_OUT_OF_MEM;
		((GF_TrackReferenceTypeBox*)newBox)->reference_type = type;
	} else {
		//OK, create the box based on the type
		newBox = gf_isom_box_new(type);
		if (!newBox) return GF_OUT_OF_MEM;
	}

	//OK, init and read this box
	if (type==GF_ISOM_BOX_TYPE_UUID) memcpy(((GF_UUIDBox *)newBox)->uuid, uuid, 16);

	if (!newBox->type) newBox->type = type; 

	end = gf_bs_available(bs);
	if (size - hdr_size > end ) {
		newBox->size = size - hdr_size - end;
		*outBox = newBox;
		return GF_ISOM_INCOMPLETE_FILE;
	}
	//we need a special reading for these boxes...
	if ((newBox->type == GF_ISOM_BOX_TYPE_STDP) || (newBox->type == GF_ISOM_BOX_TYPE_SDTP)) {
		newBox->size = size;
		*outBox = newBox;
		return GF_OK;
	}
	
	newBox->size = size - hdr_size;
	e = gf_isom_box_read(newBox, bs);	
	newBox->size = size;
	end = gf_bs_get_position(bs);

	if (e && (e != GF_ISOM_INCOMPLETE_FILE)) {
		gf_isom_box_del(newBox);
		*outBox = NULL;
		GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[iso file] Read Box \"%s\" failed (%s)\n", gf_4cc_to_str(type), gf_error_to_string(e)));
		return e;
	}

	if (end-start > size) {
		GF_LOG(GF_LOG_WARNING, GF_LOG_CONTAINER, ("[iso file] Box \"%s\" size "LLU" invalid (read "LLU")\n", gf_4cc_to_str(type), LLU_CAST size, LLU_CAST (end-start) ));
		/*let's still try to load the file since no error was notified*/
		gf_bs_seek(bs, start+size);
	} else if (end-start < size) {
		u32 to_skip = (u32) (size-(end-start));
		GF_LOG(GF_LOG_WARNING, GF_LOG_CONTAINER, ("[iso file] Box \"%s\" has %d extra bytes\n", gf_4cc_to_str(type), to_skip));
		gf_bs_skip_bytes(bs, to_skip);
	}
	*outBox = newBox;
	return e;
}

GF_Err gf_isom_parse_box(GF_Box **outBox, GF_BitStream *bs)
{
	return gf_isom_parse_box_ex(outBox, bs, 0);
}

GF_Err gf_isom_full_box_read(GF_Box *ptr, GF_BitStream *bs)
{
	GF_FullBox *self = (GF_FullBox *) ptr;
	if (ptr->size<4) return GF_ISOM_INVALID_FILE;
	self->version = gf_bs_read_u8(bs);
	self->flags = gf_bs_read_u24(bs);
	ptr->size -= 4;
	return GF_OK;
}

void gf_isom_full_box_init(GF_Box *a)
{
	GF_FullBox *ptr = (GF_FullBox *)a;
	if (! ptr) return;
	ptr->flags = 0;
	ptr->version = 0;
}


void gf_isom_box_array_del(GF_List *boxList)
{
	u32 count, i;
	GF_Box *a;
	if (!boxList) return;
	count = gf_list_count(boxList); 
	for (i = 0; i < count; i++) {
		a = (GF_Box *)gf_list_get(boxList, i);
		if (a) gf_isom_box_del(a);
	}
	gf_list_del(boxList);
}

GF_Err gf_isom_read_box_list_ex(GF_Box *parent, GF_BitStream *bs, GF_Err (*add_box)(GF_Box *par, GF_Box *b), u32 parent_type)
{
	GF_Err e;
	GF_Box *a;
	while (parent->size) {
		e = gf_isom_parse_box_ex(&a, bs, parent_type);
		if (e) {
			if (a) gf_isom_box_del(a);
			return e;
		}
		if (parent->size < a->size) {
			if (a) gf_isom_box_del(a);
			return GF_OK;
			//return GF_ISOM_INVALID_FILE;
		}
		parent->size -= a->size;
		e = add_box(parent, a);
		if (e) {
			gf_isom_box_del(a);
			return e;
		}
	}
	return GF_OK;
}

GF_Err gf_isom_read_box_list(GF_Box *parent, GF_BitStream *bs, GF_Err (*add_box)(GF_Box *par, GF_Box *b))
{
	return gf_isom_read_box_list_ex(parent, bs, add_box, 0);
}

#ifndef GPAC_DISABLE_ISOM_WRITE

GF_Err gf_isom_box_get_size(GF_Box *ptr)
{
	ptr->size = 8;

	if (ptr->type == GF_ISOM_BOX_TYPE_UUID) {
		ptr->size += 16;
	}
	//the large size is handled during write, cause at this stage we don't know the size
	return GF_OK;
}

GF_Err gf_isom_full_box_get_size(GF_Box *ptr)
{
	GF_Err e;
	e = gf_isom_box_get_size(ptr);
	if (e) return e;
	ptr->size += 4;
	return GF_OK;
}


GF_Err gf_isom_box_write_header(GF_Box *ptr, GF_BitStream *bs)
{
	if (! bs || !ptr) return GF_BAD_PARAM;
	if (!ptr->size) return GF_ISOM_INVALID_FILE;

	if (ptr->size > 0xFFFFFFFF) {
		gf_bs_write_u32(bs, 1);
	} else {
		gf_bs_write_u32(bs, (u32) ptr->size);
	}
	gf_bs_write_u32(bs, ptr->type);
	if (ptr->type == GF_ISOM_BOX_TYPE_UUID) 
		gf_bs_write_data(bs, (char*)((GF_UUIDBox*)ptr)->uuid, 16);
	if (ptr->size > 0xFFFFFFFF) 
		gf_bs_write_u64(bs, ptr->size);
	return GF_OK;
}

GF_Err gf_isom_full_box_write(GF_Box *s, GF_BitStream *bs)
{
	GF_Err e;
	GF_FullBox *ptr = (GF_FullBox *)s;
	e = gf_isom_box_write_header(s, bs);
	if (e) return e;
	gf_bs_write_u8(bs, ptr->version);
	gf_bs_write_u24(bs, ptr->flags);
	return GF_OK;
}


GF_Err gf_isom_box_array_write(GF_Box *parent, GF_List *list, GF_BitStream *bs)
{
	u32 count, i;
	GF_Box *a;
	GF_Err e;
	if (!list) return GF_BAD_PARAM;
	count = gf_list_count(list);
	for (i = 0; i < count; i++) {
		a = (GF_Box *)gf_list_get(list, i);
		if (a) {
			e = gf_isom_box_write(a, bs);
			if (e) return e;
		}
	}
	return GF_OK;
}

GF_Err gf_isom_box_array_size(GF_Box *parent, GF_List *list)
{
	GF_Err e;
	u32 count, i;
	GF_Box *a;
	if (! list) return GF_BAD_PARAM;

	count = gf_list_count(list);
	for (i = 0; i < count; i++) {
		a = (GF_Box *)gf_list_get(list, i);
		if (a) {
			e = gf_isom_box_size(a);
			if (e) return e;
			parent->size += a->size;
		}
	}
	return GF_OK;
}

#endif /*GPAC_DISABLE_ISOM_WRITE*/

GF_Box *gf_isom_box_new(u32 boxType)
{
	GF_Box *a;
	switch (boxType) {
	case GF_ISOM_BOX_TYPE_REFT:
		return reftype_New();
	case GF_ISOM_BOX_TYPE_FREE: return free_New();
	case GF_ISOM_BOX_TYPE_SKIP:
		a = free_New();
		if (a) a->type = GF_ISOM_BOX_TYPE_SKIP;
		return a;
	case GF_ISOM_BOX_TYPE_MDAT: return mdat_New();
	case GF_ISOM_BOX_TYPE_MOOV: return moov_New();
	case GF_ISOM_BOX_TYPE_MVHD: return mvhd_New();
	case GF_ISOM_BOX_TYPE_MDHD: return mdhd_New();
	case GF_ISOM_BOX_TYPE_VMHD: return vmhd_New();
	case GF_ISOM_BOX_TYPE_SMHD: return smhd_New();
	case GF_ISOM_BOX_TYPE_HMHD: return hmhd_New();
	case GF_ISOM_BOX_TYPE_ODHD:
	case GF_ISOM_BOX_TYPE_CRHD:
	case GF_ISOM_BOX_TYPE_SDHD:
	case GF_ISOM_BOX_TYPE_NMHD:
		a = nmhd_New();
		if (a) a->type = boxType;
		return a;
	case GF_ISOM_BOX_TYPE_STBL: return stbl_New();
	case GF_ISOM_BOX_TYPE_DINF: return dinf_New();
	case GF_ISOM_BOX_TYPE_URL: return url_New();
	case GF_ISOM_BOX_TYPE_URN: return urn_New();
	case GF_ISOM_BOX_TYPE_CPRT: return cprt_New();
	case GF_ISOM_BOX_TYPE_CHPL: return chpl_New();
	case GF_ISOM_BOX_TYPE_HDLR: return hdlr_New();
	case GF_ISOM_BOX_TYPE_IODS: return iods_New();
	case GF_ISOM_BOX_TYPE_TRAK: return trak_New();
	case GF_ISOM_BOX_TYPE_MP4S: return mp4s_New();
	case GF_ISOM_BOX_TYPE_MP4V: return mp4v_New();
	case GF_ISOM_BOX_TYPE_MP4A: return mp4a_New();
	case GF_ISOM_BOX_TYPE_GNRM: return gnrm_New();
	case GF_ISOM_BOX_TYPE_GNRV: return gnrv_New();
	case GF_ISOM_BOX_TYPE_GNRA: return gnra_New();
	case GF_ISOM_BOX_TYPE_EDTS: return edts_New();
	case GF_ISOM_BOX_TYPE_UDTA: return udta_New();
	case GF_ISOM_BOX_TYPE_DREF: return dref_New();
	case GF_ISOM_BOX_TYPE_STSD: return stsd_New();
	case GF_ISOM_BOX_TYPE_STTS: return stts_New();
	case GF_ISOM_BOX_TYPE_CTTS: return ctts_New();
	case GF_ISOM_BOX_TYPE_STSH: return stsh_New();
	case GF_ISOM_BOX_TYPE_ELST: return elst_New();
	case GF_ISOM_BOX_TYPE_STSC: return stsc_New();
	case GF_ISOM_BOX_TYPE_STZ2:
	case GF_ISOM_BOX_TYPE_STSZ:
		a = stsz_New();
		if (a) a->type = boxType;
		return a;
	case GF_ISOM_BOX_TYPE_STCO: return stco_New();
	case GF_ISOM_BOX_TYPE_STSS: return stss_New();
	case GF_ISOM_BOX_TYPE_STDP: return stdp_New();
	case GF_ISOM_BOX_TYPE_SDTP: return sdtp_New();
	case GF_ISOM_BOX_TYPE_CO64: return co64_New();
	case GF_ISOM_BOX_TYPE_ESDS: return esds_New();
	case GF_ISOM_BOX_TYPE_MINF: return minf_New();
	case GF_ISOM_BOX_TYPE_TKHD: return tkhd_New();
	case GF_ISOM_BOX_TYPE_TREF: return tref_New();
	case GF_ISOM_BOX_TYPE_MDIA: return mdia_New();

	case GF_ISOM_BOX_TYPE_FTYP: 
	case GF_ISOM_BOX_TYPE_STYP: 
        a = ftyp_New();
        if (a) a->type = boxType;
        return a;
	case GF_ISOM_BOX_TYPE_FADB: return padb_New();
	case GF_ISOM_BOX_TYPE_VOID: return void_New();
	case GF_ISOM_BOX_TYPE_STSF: return stsf_New();
	case GF_ISOM_BOX_TYPE_PDIN: return pdin_New();

#ifndef GPAC_DISABLE_ISOM_HINTING
	case GF_ISOM_BOX_TYPE_RTP_STSD:
		a = ghnt_New();
		if (a) a->type = boxType;
		return a;
	case GF_ISOM_BOX_TYPE_RTPO: return rtpo_New();
	case GF_ISOM_BOX_TYPE_HNTI: return hnti_New();
	case GF_ISOM_BOX_TYPE_SDP: return sdp_New();
	case GF_ISOM_BOX_TYPE_HINF: return hinf_New();
	case GF_ISOM_BOX_TYPE_RELY: return rely_New();
	case GF_ISOM_BOX_TYPE_TIMS: return tims_New();
	case GF_ISOM_BOX_TYPE_TSRO: return tsro_New();
	case GF_ISOM_BOX_TYPE_SNRO: return snro_New();
	case GF_ISOM_BOX_TYPE_TRPY: return trpy_New();
	case GF_ISOM_BOX_TYPE_NUMP: return nump_New();
	case GF_ISOM_BOX_TYPE_TOTL: return totl_New();
	case GF_ISOM_BOX_TYPE_NPCK: return npck_New();
	case GF_ISOM_BOX_TYPE_TPYL: return tpyl_New();
	case GF_ISOM_BOX_TYPE_TPAY: return tpay_New();
	case GF_ISOM_BOX_TYPE_MAXR: return maxr_New();
	case GF_ISOM_BOX_TYPE_DMED: return dmed_New();
	case GF_ISOM_BOX_TYPE_DIMM: return dimm_New();
	case GF_ISOM_BOX_TYPE_DREP: return drep_New();
	case GF_ISOM_BOX_TYPE_TMIN: return tmin_New();
	case GF_ISOM_BOX_TYPE_TMAX: return tmax_New();
	case GF_ISOM_BOX_TYPE_PMAX: return pmax_New();
	case GF_ISOM_BOX_TYPE_DMAX: return dmax_New();
	case GF_ISOM_BOX_TYPE_PAYT: return payt_New();
	case GF_ISOM_BOX_TYPE_NAME: return name_New();
#endif /*GPAC_DISABLE_ISOM_HINTING*/

#ifndef	GPAC_DISABLE_ISOM_FRAGMENTS
	case GF_ISOM_BOX_TYPE_MVEX: return mvex_New();
	case GF_ISOM_BOX_TYPE_MEHD: return mehd_New();
	case GF_ISOM_BOX_TYPE_TREX: return trex_New();
	case GF_ISOM_BOX_TYPE_MOOF: return moof_New();
	case GF_ISOM_BOX_TYPE_MFHD: return mfhd_New();
	case GF_ISOM_BOX_TYPE_TRAF: return traf_New();
	case GF_ISOM_BOX_TYPE_TFHD: return tfhd_New();
	case GF_ISOM_BOX_TYPE_TRUN: return trun_New();
#endif 

	/*3GPP boxes*/
	case GF_ISOM_SUBTYPE_3GP_AMR:
	case GF_ISOM_SUBTYPE_3GP_AMR_WB:
	case GF_ISOM_SUBTYPE_3GP_EVRC:
	case GF_ISOM_SUBTYPE_3GP_QCELP:
	case GF_ISOM_SUBTYPE_3GP_SMV:
		return gppa_New(boxType);
	case GF_ISOM_SUBTYPE_3GP_H263: 
		return gppv_New(boxType);
	case GF_ISOM_BOX_TYPE_DAMR: 
	case GF_ISOM_BOX_TYPE_DEVC: 
	case GF_ISOM_BOX_TYPE_DQCP:
	case GF_ISOM_BOX_TYPE_DSMV:
	case GF_ISOM_BOX_TYPE_D263:
		return gppc_New(boxType);

	/*AVC boxes*/
	case GF_ISOM_BOX_TYPE_AVCC: 
	case GF_ISOM_BOX_TYPE_SVCC: 
		a = avcc_New();
		if (a) a->type = boxType;
		return a;

	case GF_ISOM_BOX_TYPE_BTRT: return btrt_New();
	case GF_ISOM_BOX_TYPE_M4DS: return m4ds_New();
	case GF_ISOM_BOX_TYPE_AVC1: return avc1_New();
	case GF_ISOM_BOX_TYPE_AVC2: return avc2_New();
	case GF_ISOM_BOX_TYPE_SVC1: return svc1_New();

	/*3GPP streaming text*/
	case GF_ISOM_BOX_TYPE_FTAB: return ftab_New();
	case GF_ISOM_BOX_TYPE_TX3G: return tx3g_New();
	case GF_ISOM_BOX_TYPE_TEXT: return text_New();
	case GF_ISOM_BOX_TYPE_STYL: return styl_New();
	case GF_ISOM_BOX_TYPE_HLIT: return hlit_New();
	case GF_ISOM_BOX_TYPE_HCLR: return hclr_New();
	case GF_ISOM_BOX_TYPE_KROK: return krok_New();
	case GF_ISOM_BOX_TYPE_DLAY: return dlay_New();
	case GF_ISOM_BOX_TYPE_HREF: return href_New();
	case GF_ISOM_BOX_TYPE_TBOX: return tbox_New();
	case GF_ISOM_BOX_TYPE_BLNK: return blnk_New();
	case GF_ISOM_BOX_TYPE_TWRP: return twrp_New();

	/* ISMA 1.0 Encryption and Authentication V 1.0 */
	case GF_ISOM_BOX_TYPE_IKMS: return iKMS_New();
	case GF_ISOM_BOX_TYPE_ISFM: return iSFM_New();

	/* ISO FF extensions for MPEG-21 */
	case GF_ISOM_BOX_TYPE_META: return meta_New();
	case GF_ISOM_BOX_TYPE_XML: return xml_New();
	case GF_ISOM_BOX_TYPE_BXML: return bxml_New();
	case GF_ISOM_BOX_TYPE_ILOC: return iloc_New();
	case GF_ISOM_BOX_TYPE_PITM: return pitm_New();
	case GF_ISOM_BOX_TYPE_IPRO: return ipro_New();
	case GF_ISOM_BOX_TYPE_INFE: return infe_New();
	case GF_ISOM_BOX_TYPE_IINF: return iinf_New();
	case GF_ISOM_BOX_TYPE_SINF: return sinf_New();
	case GF_ISOM_BOX_TYPE_FRMA: return frma_New();
	case GF_ISOM_BOX_TYPE_SCHM: return schm_New();
	case GF_ISOM_BOX_TYPE_SCHI: return schi_New();
	case GF_ISOM_BOX_TYPE_ENCA: return enca_New();
	case GF_ISOM_BOX_TYPE_ENCV: return encv_New();
	case GF_ISOM_BOX_TYPE_ENCS: return encs_New();
	case GF_ISOM_BOX_TYPE_UUID: return uuid_New();

	/* Apple extensions */
	case GF_ISOM_BOX_TYPE_ILST: return ilst_New();
	
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
	case GF_ISOM_BOX_TYPE_0xA9ENC:
	case GF_ISOM_BOX_TYPE_aART:
	case GF_ISOM_BOX_TYPE_GNRE:
	case GF_ISOM_BOX_TYPE_DISK:
	case GF_ISOM_BOX_TYPE_TRKN:
	case GF_ISOM_BOX_TYPE_TMPO:
	case GF_ISOM_BOX_TYPE_CPIL:
	case GF_ISOM_BOX_TYPE_PGAP:
	case GF_ISOM_BOX_TYPE_COVR:
		return ListItem_New(boxType);

	case GF_ISOM_BOX_TYPE_DATA: return data_New();
	
	case GF_ISOM_BOX_TYPE_OHDR: return ohdr_New();
	case GF_ISOM_BOX_TYPE_GRPI: return grpi_New();
	case GF_ISOM_BOX_TYPE_MDRI: return mdri_New();
	case GF_ISOM_BOX_TYPE_ODTT: return odtt_New();
	case GF_ISOM_BOX_TYPE_ODRB: return odrb_New();
	case GF_ISOM_BOX_TYPE_ODKM: return odkm_New();
	case GF_ISOM_BOX_TYPE_ODAF: 
		a = iSFM_New();
		a->type = GF_ISOM_BOX_TYPE_ODAF;
		return a;

	case GF_ISOM_BOX_TYPE_PASP: return pasp_New();
	case GF_ISOM_BOX_TYPE_TSEL: return tsel_New();

	case GF_ISOM_BOX_TYPE_DIMS: return dims_New();
	case GF_ISOM_BOX_TYPE_DIMC: return dimC_New();
	case GF_ISOM_BOX_TYPE_DIST: return diST_New();

	case GF_ISOM_BOX_TYPE_METX:
	case GF_ISOM_BOX_TYPE_METT:
		return metx_New(boxType);

	case GF_ISOM_BOX_TYPE_AC3: return ac3_New();
	case GF_ISOM_BOX_TYPE_DAC3: return dac3_New();
	
	case GF_ISOM_BOX_TYPE_LSRC: return lsrc_New();
	case GF_ISOM_BOX_TYPE_LSR1: return lsr1_New();

	case GF_ISOM_BOX_TYPE_SIDX: return sidx_New();
	case GF_ISOM_BOX_TYPE_SUBS: return subs_New();
	case GF_ISOM_BOX_TYPE_TFDT: return tfdt_New();
	case GF_ISOM_BOX_TYPE_RVCC: return rvcc_New();

	default:
		a = defa_New();
		if (a) a->type = boxType;
		return a;
	}
}


void gf_isom_box_del(GF_Box *a)
{
	if (!a) return;
	switch (a->type) {
	case GF_ISOM_BOX_TYPE_REFT:
		reftype_del(a);
		return;
	case GF_ISOM_BOX_TYPE_FREE:
	case GF_ISOM_BOX_TYPE_SKIP:
		free_del(a);
		return;
	case GF_ISOM_BOX_TYPE_MDAT: mdat_del(a); return;
	case GF_ISOM_BOX_TYPE_MOOV: moov_del(a); return;
	case GF_ISOM_BOX_TYPE_MVHD: mvhd_del(a); return;
	case GF_ISOM_BOX_TYPE_MDHD: mdhd_del(a); return;
	case GF_ISOM_BOX_TYPE_VMHD: vmhd_del(a); return;
	case GF_ISOM_BOX_TYPE_SMHD: smhd_del(a); return;
	case GF_ISOM_BOX_TYPE_HMHD: hmhd_del(a); return;
	case GF_ISOM_BOX_TYPE_ODHD:
	case GF_ISOM_BOX_TYPE_CRHD:
	case GF_ISOM_BOX_TYPE_SDHD:
	case GF_ISOM_BOX_TYPE_NMHD:
		nmhd_del(a);
		return;
	case GF_ISOM_BOX_TYPE_STBL: stbl_del(a); return;
	case GF_ISOM_BOX_TYPE_DINF: dinf_del(a); return;
	case GF_ISOM_BOX_TYPE_URL: url_del(a); return;
	case GF_ISOM_BOX_TYPE_URN: urn_del(a); return;
	case GF_ISOM_BOX_TYPE_CHPL: chpl_del(a); return;
	case GF_ISOM_BOX_TYPE_CPRT: cprt_del(a); return;
	case GF_ISOM_BOX_TYPE_HDLR: hdlr_del(a); return;
	case GF_ISOM_BOX_TYPE_IODS: iods_del(a); return;
	case GF_ISOM_BOX_TYPE_TRAK: trak_del(a); return;
	case GF_ISOM_BOX_TYPE_MP4S: mp4s_del(a); return;
	case GF_4CC('2','6','4','b'): 
	case GF_ISOM_BOX_TYPE_MP4V: 
		mp4v_del(a); 
		return;
	case GF_ISOM_BOX_TYPE_MP4A: mp4a_del(a); return;
	case GF_ISOM_BOX_TYPE_GNRM: gnrm_del(a); return;
	case GF_ISOM_BOX_TYPE_GNRV: gnrv_del(a); return;
	case GF_ISOM_BOX_TYPE_GNRA: gnra_del(a); return;
	case GF_ISOM_BOX_TYPE_EDTS: edts_del(a); return;
	case GF_ISOM_BOX_TYPE_UDTA: udta_del(a); return;
	case GF_ISOM_BOX_TYPE_DREF: dref_del(a); return;
	case GF_ISOM_BOX_TYPE_STSD: stsd_del(a); return;
	case GF_ISOM_BOX_TYPE_STTS: stts_del(a); return;
	case GF_ISOM_BOX_TYPE_CTTS: ctts_del(a); return;
	case GF_ISOM_BOX_TYPE_STSH: stsh_del(a); return;
	case GF_ISOM_BOX_TYPE_ELST: elst_del(a); return;
	case GF_ISOM_BOX_TYPE_STSC: stsc_del(a); return;
	case GF_ISOM_BOX_TYPE_STZ2:
	case GF_ISOM_BOX_TYPE_STSZ:
		stsz_del(a);
		return;
	case GF_ISOM_BOX_TYPE_STCO: stco_del(a); return;
	case GF_ISOM_BOX_TYPE_STSS: stss_del(a); return;
	case GF_ISOM_BOX_TYPE_STDP: stdp_del(a); return;
	case GF_ISOM_BOX_TYPE_SDTP: sdtp_del(a); return;
	case GF_ISOM_BOX_TYPE_CO64: co64_del(a); return;
	case GF_ISOM_BOX_TYPE_ESDS: esds_del(a); return;
	case GF_ISOM_BOX_TYPE_MINF: minf_del(a); return;
	case GF_ISOM_BOX_TYPE_TKHD: tkhd_del(a); return;
	case GF_ISOM_BOX_TYPE_TREF: tref_del(a); return;
	case GF_ISOM_BOX_TYPE_MDIA: mdia_del(a); return;
	case GF_ISOM_BOX_TYPE_FTYP: 
	case GF_ISOM_BOX_TYPE_STYP: 
        ftyp_del(a); 
        return;
	case GF_ISOM_BOX_TYPE_FADB: padb_del(a); return;
	case GF_ISOM_BOX_TYPE_VOID: void_del(a); return;
	case GF_ISOM_BOX_TYPE_STSF: stsf_del(a); return;
	case GF_ISOM_BOX_TYPE_PDIN: pdin_del(a); return;

#ifndef GPAC_DISABLE_ISOM_HINTING
	case GF_ISOM_BOX_TYPE_RTP_STSD: ghnt_del(a); return;
	case GF_ISOM_BOX_TYPE_RTPO: rtpo_del(a); return;
	case GF_ISOM_BOX_TYPE_HNTI: hnti_del(a); return;
	case GF_ISOM_BOX_TYPE_SDP: sdp_del(a); return;
	case GF_ISOM_BOX_TYPE_HINF: hinf_del(a); return;
	case GF_ISOM_BOX_TYPE_RELY: rely_del(a); return;
	case GF_ISOM_BOX_TYPE_TIMS: tims_del(a); return;
	case GF_ISOM_BOX_TYPE_TSRO: tsro_del(a); return;
	case GF_ISOM_BOX_TYPE_SNRO: snro_del(a); return;
	case GF_ISOM_BOX_TYPE_TRPY: trpy_del(a); return;
	case GF_ISOM_BOX_TYPE_NUMP: nump_del(a); return;
	case GF_ISOM_BOX_TYPE_TOTL: totl_del(a); return;
	case GF_ISOM_BOX_TYPE_NPCK: npck_del(a); return;
	case GF_ISOM_BOX_TYPE_TPYL: tpyl_del(a); return;
	case GF_ISOM_BOX_TYPE_TPAY: tpay_del(a); return;
	case GF_ISOM_BOX_TYPE_MAXR: maxr_del(a); return;
	case GF_ISOM_BOX_TYPE_DMED: dmed_del(a); return;
	case GF_ISOM_BOX_TYPE_DIMM: dimm_del(a); return;
	case GF_ISOM_BOX_TYPE_DREP: drep_del(a); return;
	case GF_ISOM_BOX_TYPE_TMIN: tmin_del(a); return;
	case GF_ISOM_BOX_TYPE_TMAX: tmax_del(a); return;
	case GF_ISOM_BOX_TYPE_PMAX: pmax_del(a); return;
	case GF_ISOM_BOX_TYPE_DMAX: dmax_del(a); return;
	case GF_ISOM_BOX_TYPE_PAYT: payt_del(a); return;
	case GF_ISOM_BOX_TYPE_NAME: name_del(a); return;
#endif /*GPAC_DISABLE_ISOM_HINTING*/

#ifndef	GPAC_DISABLE_ISOM_FRAGMENTS
	case GF_ISOM_BOX_TYPE_MVEX: mvex_del(a); return;
	case GF_ISOM_BOX_TYPE_MEHD: mehd_del(a); return;
	case GF_ISOM_BOX_TYPE_TREX: trex_del(a); return;
	case GF_ISOM_BOX_TYPE_MOOF: moof_del(a); return;
	case GF_ISOM_BOX_TYPE_MFHD: mfhd_del(a); return;
	case GF_ISOM_BOX_TYPE_TRAF: traf_del(a); return;
	case GF_ISOM_BOX_TYPE_TFHD: tfhd_del(a); return;
	case GF_ISOM_BOX_TYPE_TRUN: trun_del(a); return;
#endif

	case GF_ISOM_SUBTYPE_3GP_AMR:
	case GF_ISOM_SUBTYPE_3GP_AMR_WB:
	case GF_ISOM_SUBTYPE_3GP_EVRC:
	case GF_ISOM_SUBTYPE_3GP_QCELP:
	case GF_ISOM_SUBTYPE_3GP_SMV:
		gppa_del(a);
		return;
	case GF_ISOM_SUBTYPE_3GP_H263: gppv_del(a); return;
	case GF_ISOM_BOX_TYPE_DAMR: 
	case GF_ISOM_BOX_TYPE_DEVC: 
	case GF_ISOM_BOX_TYPE_DQCP:
	case GF_ISOM_BOX_TYPE_DSMV:
	case GF_ISOM_BOX_TYPE_D263: 
		gppc_del(a); return;

	/*AVC boxes*/
	case GF_ISOM_BOX_TYPE_AVCC: 
	case GF_ISOM_BOX_TYPE_SVCC: 
		avcc_del(a); return;
	case GF_ISOM_BOX_TYPE_BTRT: btrt_del(a); return;
	case GF_ISOM_BOX_TYPE_M4DS: m4ds_del(a); return;
	case GF_ISOM_BOX_TYPE_AVC1: 
	case GF_ISOM_BOX_TYPE_AVC2: 
	case GF_ISOM_BOX_TYPE_SVC1: 
		mp4v_del(a); return;

	/*3GPP streaming text*/
	case GF_ISOM_BOX_TYPE_FTAB: ftab_del(a); return;
	case GF_ISOM_BOX_TYPE_TX3G: tx3g_del(a); return;
	case GF_ISOM_BOX_TYPE_TEXT: text_del(a); return;
	case GF_ISOM_BOX_TYPE_STYL: styl_del(a); return;
	case GF_ISOM_BOX_TYPE_HLIT: hlit_del(a); return;
	case GF_ISOM_BOX_TYPE_HCLR: hclr_del(a); return;
	case GF_ISOM_BOX_TYPE_KROK: krok_del(a); return;
	case GF_ISOM_BOX_TYPE_DLAY: dlay_del(a); return;
	case GF_ISOM_BOX_TYPE_HREF: href_del(a); return;
	case GF_ISOM_BOX_TYPE_TBOX: tbox_del(a); return;
	case GF_ISOM_BOX_TYPE_BLNK: blnk_del(a); return;
	case GF_ISOM_BOX_TYPE_TWRP: twrp_del(a); return;

	/* ISMA 1.0 Encryption and Authentication V 1.0 */
	case GF_ISOM_BOX_TYPE_IKMS:			iKMS_del(a); return;
	case GF_ISOM_BOX_TYPE_ISFM:	iSFM_del(a); return;

	/* ISO FF extensions for MPEG-21 */
	case GF_ISOM_BOX_TYPE_META: meta_del(a); return;
	case GF_ISOM_BOX_TYPE_XML: xml_del(a); return;
	case GF_ISOM_BOX_TYPE_BXML: bxml_del(a); return;
	case GF_ISOM_BOX_TYPE_ILOC: iloc_del(a); return;
	case GF_ISOM_BOX_TYPE_PITM: pitm_del(a); return;
	case GF_ISOM_BOX_TYPE_IPRO: ipro_del(a); return;
	case GF_ISOM_BOX_TYPE_INFE: infe_del(a); return;
	case GF_ISOM_BOX_TYPE_IINF: iinf_del(a); return;
	case GF_ISOM_BOX_TYPE_SINF:	sinf_del(a); return;
	case GF_ISOM_BOX_TYPE_FRMA:	frma_del(a); return;
	case GF_ISOM_BOX_TYPE_SCHM:		schm_del(a); return;
	case GF_ISOM_BOX_TYPE_SCHI: schi_del(a); return;

	case GF_ISOM_BOX_TYPE_ENCA: 
	case GF_ISOM_BOX_TYPE_ENCV:
	case GF_ISOM_BOX_TYPE_ENCS:
		a->type = ((GF_SampleEntryBox *)a)->protection_info->original_format->data_format;
		gf_isom_box_del(a); 
		return;
	case GF_ISOM_BOX_TYPE_UUID:
		uuid_del(a);
		return;

	/* Apple extensions */
	case GF_ISOM_BOX_TYPE_ILST: ilst_del(a); return;
	
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
	case GF_ISOM_BOX_TYPE_0xA9ENC:
	case GF_ISOM_BOX_TYPE_aART:
	case GF_ISOM_BOX_TYPE_GNRE:
	case GF_ISOM_BOX_TYPE_DISK:
	case GF_ISOM_BOX_TYPE_TRKN:
	case GF_ISOM_BOX_TYPE_TMPO:
	case GF_ISOM_BOX_TYPE_CPIL:
	case GF_ISOM_BOX_TYPE_PGAP:
	case GF_ISOM_BOX_TYPE_COVR:
		ListItem_del(a);
		return;

	case GF_ISOM_BOX_TYPE_DATA: data_del(a); return;

	case GF_ISOM_BOX_TYPE_OHDR: ohdr_del(a); return;
	case GF_ISOM_BOX_TYPE_GRPI: grpi_del(a); return;
	case GF_ISOM_BOX_TYPE_MDRI: mdri_del(a); return;
	case GF_ISOM_BOX_TYPE_ODTT: odtt_del(a); return;
	case GF_ISOM_BOX_TYPE_ODRB: odrb_del(a); return;
	case GF_ISOM_BOX_TYPE_ODKM: odkm_del(a); return;
	case GF_ISOM_BOX_TYPE_ODAF: iSFM_del(a); return;

	case GF_ISOM_BOX_TYPE_PASP: pasp_del(a); return;
	case GF_ISOM_BOX_TYPE_TSEL: tsel_del(a); return;

	case GF_ISOM_BOX_TYPE_METX:
	case GF_ISOM_BOX_TYPE_METT:
		metx_del(a);
		return;

	case GF_ISOM_BOX_TYPE_DIMS: dims_del(a); return;
	case GF_ISOM_BOX_TYPE_DIMC: dimC_del(a); return;
	case GF_ISOM_BOX_TYPE_DIST: diST_del(a); return;

	case GF_ISOM_BOX_TYPE_AC3: ac3_del(a); return;
	case GF_ISOM_BOX_TYPE_DAC3: dac3_del(a); return;

	case GF_ISOM_BOX_TYPE_LSRC: lsrc_del(a); return;
	case GF_ISOM_BOX_TYPE_LSR1: lsr1_del(a); return;

	case GF_ISOM_BOX_TYPE_SIDX: sidx_del(a); return;
	case GF_ISOM_BOX_TYPE_SUBS: subs_del(a); return;
	case GF_ISOM_BOX_TYPE_TFDT: tfdt_del(a); return;
	case GF_ISOM_BOX_TYPE_RVCC: rvcc_del(a); return;

	default:
		defa_del(a);
		return;
	}
}




GF_Err gf_isom_box_read(GF_Box *a, GF_BitStream *bs)
{
	switch (a->type) {
	case GF_ISOM_BOX_TYPE_REFT:
		return reftype_Read(a, bs);
	case GF_ISOM_BOX_TYPE_FREE:
	case GF_ISOM_BOX_TYPE_SKIP:
		return free_Read(a, bs);
	case GF_ISOM_BOX_TYPE_MDAT: return mdat_Read(a, bs);
	case GF_ISOM_BOX_TYPE_MOOV: return moov_Read(a, bs);
	case GF_ISOM_BOX_TYPE_MVHD: return mvhd_Read(a, bs);
	case GF_ISOM_BOX_TYPE_MDHD: return mdhd_Read(a, bs);
	case GF_ISOM_BOX_TYPE_VMHD: return vmhd_Read(a, bs);
	case GF_ISOM_BOX_TYPE_SMHD: return smhd_Read(a, bs);
	case GF_ISOM_BOX_TYPE_HMHD: return hmhd_Read(a, bs);
	case GF_ISOM_BOX_TYPE_ODHD:
	case GF_ISOM_BOX_TYPE_CRHD:
	case GF_ISOM_BOX_TYPE_SDHD:
	case GF_ISOM_BOX_TYPE_NMHD:
		return nmhd_Read(a, bs);
	case GF_ISOM_BOX_TYPE_STBL: return stbl_Read(a, bs);
	case GF_ISOM_BOX_TYPE_DINF: return dinf_Read(a, bs);
	case GF_ISOM_BOX_TYPE_URL: return url_Read(a, bs);
	case GF_ISOM_BOX_TYPE_URN: return urn_Read(a, bs);
	case GF_ISOM_BOX_TYPE_CPRT: return cprt_Read(a, bs);
	case GF_ISOM_BOX_TYPE_HDLR: return hdlr_Read(a, bs);
	case GF_ISOM_BOX_TYPE_IODS: return iods_Read(a, bs);
	case GF_ISOM_BOX_TYPE_TRAK: return trak_Read(a, bs);
	case GF_ISOM_BOX_TYPE_MP4S: return mp4s_Read(a, bs);
	case GF_ISOM_BOX_TYPE_MP4V: return mp4v_Read(a, bs);
	case GF_ISOM_BOX_TYPE_MP4A: return mp4a_Read(a, bs);
	case GF_ISOM_BOX_TYPE_EDTS: return edts_Read(a, bs);
	case GF_ISOM_BOX_TYPE_UDTA: return udta_Read(a, bs);
	case GF_ISOM_BOX_TYPE_DREF: return dref_Read(a, bs);
	case GF_ISOM_BOX_TYPE_STSD: return stsd_Read(a, bs);
	case GF_ISOM_BOX_TYPE_STTS: return stts_Read(a, bs);
	case GF_ISOM_BOX_TYPE_CTTS: return ctts_Read(a, bs);
	case GF_ISOM_BOX_TYPE_STSH: return stsh_Read(a, bs);
	case GF_ISOM_BOX_TYPE_ELST: return elst_Read(a, bs);
	case GF_ISOM_BOX_TYPE_STSC: return stsc_Read(a, bs);
	case GF_ISOM_BOX_TYPE_STZ2:
	case GF_ISOM_BOX_TYPE_STSZ:
		return stsz_Read(a, bs);
	case GF_ISOM_BOX_TYPE_STCO: return stco_Read(a, bs);
	case GF_ISOM_BOX_TYPE_STSS: return stss_Read(a, bs);
	case GF_ISOM_BOX_TYPE_STDP: return stdp_Read(a, bs);
	case GF_ISOM_BOX_TYPE_SDTP: return sdtp_Read(a, bs);
	case GF_ISOM_BOX_TYPE_CO64: return co64_Read(a, bs);
	case GF_ISOM_BOX_TYPE_ESDS: return esds_Read(a, bs);
	case GF_ISOM_BOX_TYPE_MINF: return minf_Read(a, bs);
	case GF_ISOM_BOX_TYPE_TKHD: return tkhd_Read(a, bs);
	case GF_ISOM_BOX_TYPE_TREF: return tref_Read(a, bs);
	case GF_ISOM_BOX_TYPE_MDIA: return mdia_Read(a, bs);
	case GF_ISOM_BOX_TYPE_CHPL: return chpl_Read(a, bs);
	case GF_ISOM_BOX_TYPE_FTYP: 
	case GF_ISOM_BOX_TYPE_STYP: 
        return ftyp_Read(a, bs);
	case GF_ISOM_BOX_TYPE_FADB: return padb_Read(a, bs);
	case GF_ISOM_BOX_TYPE_VOID: return void_Read(a, bs);
	case GF_ISOM_BOX_TYPE_STSF: return stsf_Read(a, bs);
	case GF_ISOM_BOX_TYPE_PDIN: return pdin_Read(a, bs);
	
#ifndef GPAC_DISABLE_ISOM_HINTING
	case GF_ISOM_BOX_TYPE_RTP_STSD: return ghnt_Read(a, bs);
	case GF_ISOM_BOX_TYPE_RTPO: return rtpo_Read(a, bs);
	case GF_ISOM_BOX_TYPE_HNTI: return hnti_Read(a, bs);
	case GF_ISOM_BOX_TYPE_SDP: return sdp_Read(a, bs);
	case GF_ISOM_BOX_TYPE_HINF: return hinf_Read(a, bs);
	case GF_ISOM_BOX_TYPE_RELY: return rely_Read(a, bs);
	case GF_ISOM_BOX_TYPE_TIMS: return tims_Read(a, bs);
	case GF_ISOM_BOX_TYPE_TSRO: return tsro_Read(a, bs);
	case GF_ISOM_BOX_TYPE_SNRO: return snro_Read(a, bs);
	case GF_ISOM_BOX_TYPE_TRPY: return trpy_Read(a, bs);
	case GF_ISOM_BOX_TYPE_NUMP: return nump_Read(a, bs);
	case GF_ISOM_BOX_TYPE_TOTL: return totl_Read(a, bs);
	case GF_ISOM_BOX_TYPE_NPCK: return npck_Read(a, bs);
	case GF_ISOM_BOX_TYPE_TPYL: return tpyl_Read(a, bs);
	case GF_ISOM_BOX_TYPE_TPAY: return tpay_Read(a, bs);
	case GF_ISOM_BOX_TYPE_MAXR: return maxr_Read(a, bs);
	case GF_ISOM_BOX_TYPE_DMED: return dmed_Read(a, bs);
	case GF_ISOM_BOX_TYPE_DIMM: return dimm_Read(a, bs);
	case GF_ISOM_BOX_TYPE_DREP: return drep_Read(a, bs);
	case GF_ISOM_BOX_TYPE_TMIN: return tmin_Read(a, bs);
	case GF_ISOM_BOX_TYPE_TMAX: return tmax_Read(a, bs);
	case GF_ISOM_BOX_TYPE_PMAX: return pmax_Read(a, bs);
	case GF_ISOM_BOX_TYPE_DMAX: return dmax_Read(a, bs);
	case GF_ISOM_BOX_TYPE_PAYT: return payt_Read(a, bs);
	case GF_ISOM_BOX_TYPE_NAME: return name_Read(a, bs);
#endif /*GPAC_DISABLE_ISOM_HINTING*/
	
#ifndef	GPAC_DISABLE_ISOM_FRAGMENTS
	case GF_ISOM_BOX_TYPE_MVEX: return mvex_Read(a, bs);
	case GF_ISOM_BOX_TYPE_MEHD: return mehd_Read(a, bs);
	case GF_ISOM_BOX_TYPE_TREX: return trex_Read(a, bs);
	case GF_ISOM_BOX_TYPE_MOOF: return moof_Read(a, bs);
	case GF_ISOM_BOX_TYPE_MFHD: return mfhd_Read(a, bs);
	case GF_ISOM_BOX_TYPE_TRAF: return traf_Read(a, bs);
	case GF_ISOM_BOX_TYPE_TFHD: return tfhd_Read(a, bs);
	case GF_ISOM_BOX_TYPE_TRUN: return trun_Read(a, bs);
#endif
	

	/*3GPP boxes*/
	case GF_ISOM_SUBTYPE_3GP_AMR:
	case GF_ISOM_SUBTYPE_3GP_AMR_WB:
	case GF_ISOM_SUBTYPE_3GP_EVRC:
	case GF_ISOM_SUBTYPE_3GP_QCELP:
	case GF_ISOM_SUBTYPE_3GP_SMV:
		return gppa_Read(a, bs);
	case GF_ISOM_SUBTYPE_3GP_H263: return gppv_Read(a, bs);
	case GF_ISOM_BOX_TYPE_DAMR:
	case GF_ISOM_BOX_TYPE_DEVC: 
	case GF_ISOM_BOX_TYPE_DQCP:
	case GF_ISOM_BOX_TYPE_DSMV:
	case GF_ISOM_BOX_TYPE_D263:
		return gppc_Read(a, bs);

	case GF_ISOM_BOX_TYPE_AVCC: 
	case GF_ISOM_BOX_TYPE_SVCC: 
		return avcc_Read(a, bs);
	case GF_ISOM_BOX_TYPE_BTRT: return btrt_Read(a, bs);
	case GF_ISOM_BOX_TYPE_M4DS: return m4ds_Read(a, bs);
	case GF_ISOM_BOX_TYPE_AVC1: 
	case GF_ISOM_BOX_TYPE_AVC2: 
	case GF_ISOM_BOX_TYPE_SVC1: 
		return mp4v_Read(a, bs);

	/*3GPP streaming text*/
	case GF_ISOM_BOX_TYPE_FTAB: return ftab_Read(a, bs);
	case GF_ISOM_BOX_TYPE_TEXT: return text_Read(a, bs);
	case GF_ISOM_BOX_TYPE_TX3G: return tx3g_Read(a, bs);
	case GF_ISOM_BOX_TYPE_STYL: return styl_Read(a, bs);
	case GF_ISOM_BOX_TYPE_HLIT: return hlit_Read(a, bs);
	case GF_ISOM_BOX_TYPE_HCLR: return hclr_Read(a, bs);
	case GF_ISOM_BOX_TYPE_KROK: return krok_Read(a, bs);
	case GF_ISOM_BOX_TYPE_DLAY: return dlay_Read(a, bs);
	case GF_ISOM_BOX_TYPE_HREF: return href_Read(a, bs);
	case GF_ISOM_BOX_TYPE_TBOX: return tbox_Read(a, bs);
	case GF_ISOM_BOX_TYPE_BLNK: return blnk_Read(a, bs);
	case GF_ISOM_BOX_TYPE_TWRP: return twrp_Read(a, bs);

	/* ISMA 1.0 Encryption and Authentication V 1.0 */
	case GF_ISOM_BOX_TYPE_IKMS: return iKMS_Read(a, bs);
	case GF_ISOM_BOX_TYPE_ISFM: return iSFM_Read(a, bs);

	/* ISO FF extensions for MPEG-21 */
	case GF_ISOM_BOX_TYPE_META: return meta_Read(a, bs);
	case GF_ISOM_BOX_TYPE_XML: return xml_Read(a, bs);
	case GF_ISOM_BOX_TYPE_BXML: return bxml_Read(a, bs);
	case GF_ISOM_BOX_TYPE_ILOC: return iloc_Read(a, bs);
	case GF_ISOM_BOX_TYPE_PITM: return pitm_Read(a, bs);
	case GF_ISOM_BOX_TYPE_IPRO: return ipro_Read(a, bs);
	case GF_ISOM_BOX_TYPE_INFE: return infe_Read(a, bs);
	case GF_ISOM_BOX_TYPE_IINF: return iinf_Read(a, bs);
	case GF_ISOM_BOX_TYPE_SINF: return sinf_Read(a, bs);
	case GF_ISOM_BOX_TYPE_FRMA: return frma_Read(a, bs);
	case GF_ISOM_BOX_TYPE_SCHM: return schm_Read(a, bs);
	case GF_ISOM_BOX_TYPE_SCHI: return schi_Read(a, bs);
	case GF_ISOM_BOX_TYPE_ENCA: return mp4a_Read(a, bs);
	case GF_ISOM_BOX_TYPE_ENCV: return mp4v_Read(a, bs);
	case GF_ISOM_BOX_TYPE_ENCS: return mp4s_Read(a, bs);
	case GF_ISOM_BOX_TYPE_UUID: return uuid_Read(a, bs);
	
	/* Apple extensions */
	case GF_ISOM_BOX_TYPE_ILST: return ilst_Read(a, bs);

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
	case GF_ISOM_BOX_TYPE_0xA9ENC:
	case GF_ISOM_BOX_TYPE_aART:
	case GF_ISOM_BOX_TYPE_GNRE:
	case GF_ISOM_BOX_TYPE_DISK:
	case GF_ISOM_BOX_TYPE_TRKN:
	case GF_ISOM_BOX_TYPE_TMPO:
	case GF_ISOM_BOX_TYPE_CPIL:
	case GF_ISOM_BOX_TYPE_PGAP:
	case GF_ISOM_BOX_TYPE_COVR:
		return ListItem_Read(a, bs);

	case GF_ISOM_BOX_TYPE_DATA: return data_Read(a, bs);

	case GF_ISOM_BOX_TYPE_OHDR: return ohdr_Read(a, bs);
	case GF_ISOM_BOX_TYPE_GRPI: return grpi_Read(a, bs);
	case GF_ISOM_BOX_TYPE_MDRI: return mdri_Read(a, bs);
	case GF_ISOM_BOX_TYPE_ODTT: return odtt_Read(a, bs);
	case GF_ISOM_BOX_TYPE_ODRB: return odrb_Read(a, bs);
	case GF_ISOM_BOX_TYPE_ODKM: return odkm_Read(a, bs);
	case GF_ISOM_BOX_TYPE_ODAF: return iSFM_Read(a, bs);

	case GF_ISOM_BOX_TYPE_PASP: return pasp_Read(a, bs);
	case GF_ISOM_BOX_TYPE_TSEL: return tsel_Read(a, bs);

	case GF_ISOM_BOX_TYPE_METX:
	case GF_ISOM_BOX_TYPE_METT:
		return metx_Read(a, bs);

	case GF_ISOM_BOX_TYPE_DIMS: return dims_Read(a, bs);
	case GF_ISOM_BOX_TYPE_DIMC: return dimC_Read(a, bs);
	case GF_ISOM_BOX_TYPE_DIST: return diST_Read(a, bs);

	case GF_ISOM_BOX_TYPE_AC3: return ac3_Read(a, bs); 
	case GF_ISOM_BOX_TYPE_DAC3: return dac3_Read(a, bs); 

	case GF_ISOM_BOX_TYPE_LSRC: return lsrc_Read(a, bs); 
	case GF_ISOM_BOX_TYPE_LSR1: return lsr1_Read(a, bs); 

	case GF_ISOM_BOX_TYPE_SIDX: return sidx_Read(a, bs); 
	case GF_ISOM_BOX_TYPE_SUBS: return subs_Read(a, bs);
	case GF_ISOM_BOX_TYPE_TFDT: return tfdt_Read(a, bs);
	case GF_ISOM_BOX_TYPE_RVCC: return rvcc_Read(a, bs);

	default:
		return defa_Read(a, bs);
	}
}

#ifndef GPAC_DISABLE_ISOM_WRITE

GF_Err gf_isom_box_write(GF_Box *a, GF_BitStream *bs)
{
	switch (a->type) {
	case GF_ISOM_BOX_TYPE_REFT:
		return reftype_Write(a, bs);
	case GF_ISOM_BOX_TYPE_FREE:
	case GF_ISOM_BOX_TYPE_SKIP:
		return free_Write(a, bs);
	case GF_ISOM_BOX_TYPE_MDAT: return mdat_Write(a, bs);
	case GF_ISOM_BOX_TYPE_MOOV: return moov_Write(a, bs);
	case GF_ISOM_BOX_TYPE_MVHD: return mvhd_Write(a, bs);
	case GF_ISOM_BOX_TYPE_MDHD: return mdhd_Write(a, bs);
	case GF_ISOM_BOX_TYPE_VMHD: return vmhd_Write(a, bs);
	case GF_ISOM_BOX_TYPE_SMHD: return smhd_Write(a, bs);
	case GF_ISOM_BOX_TYPE_HMHD: return hmhd_Write(a, bs);
	case GF_ISOM_BOX_TYPE_ODHD:
	case GF_ISOM_BOX_TYPE_CRHD:
	case GF_ISOM_BOX_TYPE_SDHD:
	case GF_ISOM_BOX_TYPE_NMHD:
		return nmhd_Write(a, bs);
	case GF_ISOM_BOX_TYPE_STBL: return stbl_Write(a, bs);
	case GF_ISOM_BOX_TYPE_DINF: return dinf_Write(a, bs);
	case GF_ISOM_BOX_TYPE_URL: return url_Write(a, bs);
	case GF_ISOM_BOX_TYPE_URN: return urn_Write(a, bs);
	case GF_ISOM_BOX_TYPE_CHPL: return chpl_Write(a, bs);
	case GF_ISOM_BOX_TYPE_CPRT: return cprt_Write(a, bs);
	case GF_ISOM_BOX_TYPE_HDLR: return hdlr_Write(a, bs);
	case GF_ISOM_BOX_TYPE_IODS: return iods_Write(a, bs);
	case GF_ISOM_BOX_TYPE_TRAK: return trak_Write(a, bs);
	case GF_ISOM_BOX_TYPE_MP4S: return mp4s_Write(a, bs);
	case GF_ISOM_BOX_TYPE_MP4V: return mp4v_Write(a, bs);
	case GF_ISOM_BOX_TYPE_MP4A: return mp4a_Write(a, bs);
	case GF_ISOM_BOX_TYPE_GNRM: return gnrm_Write(a, bs);
	case GF_ISOM_BOX_TYPE_GNRV: return gnrv_Write(a, bs);
	case GF_ISOM_BOX_TYPE_GNRA: return gnra_Write(a, bs);
	case GF_ISOM_BOX_TYPE_EDTS: return edts_Write(a, bs);
	case GF_ISOM_BOX_TYPE_UDTA: return udta_Write(a, bs);
	case GF_ISOM_BOX_TYPE_DREF: return dref_Write(a, bs);
	case GF_ISOM_BOX_TYPE_STSD: return stsd_Write(a, bs);
	case GF_ISOM_BOX_TYPE_STTS: return stts_Write(a, bs);
	case GF_ISOM_BOX_TYPE_CTTS: return ctts_Write(a, bs);
	case GF_ISOM_BOX_TYPE_STSH: return stsh_Write(a, bs);
	case GF_ISOM_BOX_TYPE_ELST: return elst_Write(a, bs);
	case GF_ISOM_BOX_TYPE_STSC: return stsc_Write(a, bs);
	case GF_ISOM_BOX_TYPE_STZ2:
	case GF_ISOM_BOX_TYPE_STSZ:
		return stsz_Write(a, bs);
	case GF_ISOM_BOX_TYPE_STCO: return stco_Write(a, bs);
	case GF_ISOM_BOX_TYPE_STSS: return stss_Write(a, bs);
	case GF_ISOM_BOX_TYPE_STDP: return stdp_Write(a, bs);
	case GF_ISOM_BOX_TYPE_SDTP: return sdtp_Write(a, bs);
	case GF_ISOM_BOX_TYPE_CO64: return co64_Write(a, bs);
	case GF_ISOM_BOX_TYPE_ESDS: return esds_Write(a, bs);
	case GF_ISOM_BOX_TYPE_MINF: return minf_Write(a, bs);
	case GF_ISOM_BOX_TYPE_TKHD: return tkhd_Write(a, bs);
	case GF_ISOM_BOX_TYPE_TREF: return tref_Write(a, bs);
	case GF_ISOM_BOX_TYPE_MDIA: return mdia_Write(a, bs);
	case GF_ISOM_BOX_TYPE_FTYP: 
	case GF_ISOM_BOX_TYPE_STYP: 
        return ftyp_Write(a, bs);
	case GF_ISOM_BOX_TYPE_FADB: return padb_Write(a, bs);
	case GF_ISOM_BOX_TYPE_VOID: return void_Write(a, bs);
	case GF_ISOM_BOX_TYPE_STSF: return stsf_Write(a, bs);
	case GF_ISOM_BOX_TYPE_PDIN: return pdin_Write(a, bs);

#ifndef GPAC_DISABLE_ISOM_HINTING
	case GF_ISOM_BOX_TYPE_RTP_STSD: return ghnt_Write(a, bs);
	case GF_ISOM_BOX_TYPE_RTPO: return rtpo_Write(a, bs);
	case GF_ISOM_BOX_TYPE_HNTI: return hnti_Write(a, bs);
	case GF_ISOM_BOX_TYPE_SDP: return sdp_Write(a, bs);
	case GF_ISOM_BOX_TYPE_HINF: return hinf_Write(a, bs);
	case GF_ISOM_BOX_TYPE_RELY: return rely_Write(a, bs);
	case GF_ISOM_BOX_TYPE_TIMS: return tims_Write(a, bs);
	case GF_ISOM_BOX_TYPE_TSRO: return tsro_Write(a, bs);
	case GF_ISOM_BOX_TYPE_SNRO: return snro_Write(a, bs);
	case GF_ISOM_BOX_TYPE_TRPY: return trpy_Write(a, bs);
	case GF_ISOM_BOX_TYPE_NUMP: return nump_Write(a, bs);
	case GF_ISOM_BOX_TYPE_TOTL: return totl_Write(a, bs);
	case GF_ISOM_BOX_TYPE_NPCK: return npck_Write(a, bs);
	case GF_ISOM_BOX_TYPE_TPYL: return tpyl_Write(a, bs);
	case GF_ISOM_BOX_TYPE_TPAY: return tpay_Write(a, bs);
	case GF_ISOM_BOX_TYPE_MAXR: return maxr_Write(a, bs);
	case GF_ISOM_BOX_TYPE_DMED: return dmed_Write(a, bs);
	case GF_ISOM_BOX_TYPE_DIMM: return dimm_Write(a, bs);
	case GF_ISOM_BOX_TYPE_DREP: return drep_Write(a, bs);
	case GF_ISOM_BOX_TYPE_TMIN: return tmin_Write(a, bs);
	case GF_ISOM_BOX_TYPE_TMAX: return tmax_Write(a, bs);
	case GF_ISOM_BOX_TYPE_PMAX: return pmax_Write(a, bs);
	case GF_ISOM_BOX_TYPE_DMAX: return dmax_Write(a, bs);
	case GF_ISOM_BOX_TYPE_PAYT: return payt_Write(a, bs);
	case GF_ISOM_BOX_TYPE_NAME: return name_Write(a, bs);
#endif /*GPAC_DISABLE_ISOM_HINTING*/

#ifndef	GPAC_DISABLE_ISOM_FRAGMENTS
	case GF_ISOM_BOX_TYPE_MVEX: return mvex_Write(a, bs);
	case GF_ISOM_BOX_TYPE_MEHD: return mehd_Write(a, bs);
	case GF_ISOM_BOX_TYPE_TREX: return trex_Write(a, bs);
	case GF_ISOM_BOX_TYPE_MOOF: return moof_Write(a, bs);
	case GF_ISOM_BOX_TYPE_MFHD: return mfhd_Write(a, bs);
	case GF_ISOM_BOX_TYPE_TRAF: return traf_Write(a, bs);
	case GF_ISOM_BOX_TYPE_TFHD: return tfhd_Write(a, bs);
	case GF_ISOM_BOX_TYPE_TRUN: return trun_Write(a, bs);
#endif 

	/*3GPP boxes*/
	case GF_ISOM_SUBTYPE_3GP_AMR:
	case GF_ISOM_SUBTYPE_3GP_AMR_WB:
	case GF_ISOM_SUBTYPE_3GP_EVRC:
	case GF_ISOM_SUBTYPE_3GP_QCELP:
	case GF_ISOM_SUBTYPE_3GP_SMV:
		return gppa_Write(a, bs);
	case GF_ISOM_SUBTYPE_3GP_H263: 
		return gppv_Write(a, bs);
	case GF_ISOM_BOX_TYPE_DAMR:
	case GF_ISOM_BOX_TYPE_DEVC: 
	case GF_ISOM_BOX_TYPE_DQCP:
	case GF_ISOM_BOX_TYPE_DSMV:
	case GF_ISOM_BOX_TYPE_D263:
		return gppc_Write(a, bs);

	case GF_ISOM_BOX_TYPE_AVCC: 
	case GF_ISOM_BOX_TYPE_SVCC: 
		return avcc_Write(a, bs);
	case GF_ISOM_BOX_TYPE_BTRT: return btrt_Write(a, bs);
	case GF_ISOM_BOX_TYPE_M4DS: return m4ds_Write(a, bs);
	case GF_ISOM_BOX_TYPE_AVC1: 
	case GF_ISOM_BOX_TYPE_AVC2: 
	case GF_ISOM_BOX_TYPE_SVC1: 
		return mp4v_Write(a, bs);

	/*3GPP streaming text*/
	case GF_ISOM_BOX_TYPE_FTAB: return ftab_Write(a, bs);
	case GF_ISOM_BOX_TYPE_TX3G:	return tx3g_Write(a, bs);
	case GF_ISOM_BOX_TYPE_TEXT:	return text_Write(a, bs);
	case GF_ISOM_BOX_TYPE_STYL: return styl_Write(a, bs);
	case GF_ISOM_BOX_TYPE_HLIT: return hlit_Write(a, bs);
	case GF_ISOM_BOX_TYPE_HCLR: return hclr_Write(a, bs);
	case GF_ISOM_BOX_TYPE_KROK: return krok_Write(a, bs);
	case GF_ISOM_BOX_TYPE_DLAY: return dlay_Write(a, bs);
	case GF_ISOM_BOX_TYPE_HREF: return href_Write(a, bs);
	case GF_ISOM_BOX_TYPE_TBOX: return tbox_Write(a, bs);
	case GF_ISOM_BOX_TYPE_BLNK: return blnk_Write(a, bs);
	case GF_ISOM_BOX_TYPE_TWRP: return twrp_Write(a, bs);
	
	/* ISMA 1.0 Encryption and Authentication V 1.0 */
	case GF_ISOM_BOX_TYPE_IKMS: return iKMS_Write(a, bs);
	case GF_ISOM_BOX_TYPE_ISFM: return iSFM_Write(a, bs);

	/* ISO FF extensions for MPEG-21 */
	case GF_ISOM_BOX_TYPE_META: return meta_Write(a, bs);
	case GF_ISOM_BOX_TYPE_XML: return xml_Write(a, bs);
	case GF_ISOM_BOX_TYPE_BXML: return bxml_Write(a, bs);
	case GF_ISOM_BOX_TYPE_ILOC: return iloc_Write(a, bs);
	case GF_ISOM_BOX_TYPE_PITM: return pitm_Write(a, bs);
	case GF_ISOM_BOX_TYPE_IPRO: return ipro_Write(a, bs);
	case GF_ISOM_BOX_TYPE_INFE: return infe_Write(a, bs);
	case GF_ISOM_BOX_TYPE_IINF: return iinf_Write(a, bs);
	case GF_ISOM_BOX_TYPE_SINF: return sinf_Write(a, bs);
	case GF_ISOM_BOX_TYPE_FRMA: return frma_Write(a, bs);
	case GF_ISOM_BOX_TYPE_SCHM: return schm_Write(a, bs);
	case GF_ISOM_BOX_TYPE_SCHI: return schi_Write(a, bs);
	case GF_ISOM_BOX_TYPE_ENCA: return mp4a_Write(a, bs);
	case GF_ISOM_BOX_TYPE_ENCV: return mp4v_Write(a, bs);
	case GF_ISOM_BOX_TYPE_ENCS: return mp4s_Write(a, bs);
	case GF_ISOM_BOX_TYPE_UUID: return uuid_Write(a, bs);

	/* Apple extensions */
	case GF_ISOM_BOX_TYPE_ILST: return ilst_Write(a, bs);

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
	case GF_ISOM_BOX_TYPE_0xA9ENC:
	case GF_ISOM_BOX_TYPE_aART:
	case GF_ISOM_BOX_TYPE_GNRE:
	case GF_ISOM_BOX_TYPE_DISK:
	case GF_ISOM_BOX_TYPE_TRKN:
	case GF_ISOM_BOX_TYPE_TMPO:
	case GF_ISOM_BOX_TYPE_CPIL:
	case GF_ISOM_BOX_TYPE_PGAP:
	case GF_ISOM_BOX_TYPE_COVR:
		return ListItem_Write(a, bs);

	case GF_ISOM_BOX_TYPE_DATA: return data_Write(a, bs);

	case GF_ISOM_BOX_TYPE_OHDR: return ohdr_Write(a, bs);
	case GF_ISOM_BOX_TYPE_GRPI: return grpi_Write(a, bs);
	case GF_ISOM_BOX_TYPE_MDRI: return mdri_Write(a, bs);
	case GF_ISOM_BOX_TYPE_ODTT: return odtt_Write(a, bs);
	case GF_ISOM_BOX_TYPE_ODRB: return odrb_Write(a, bs);
	case GF_ISOM_BOX_TYPE_ODKM: return odkm_Write(a, bs);
	case GF_ISOM_BOX_TYPE_ODAF: return iSFM_Write(a, bs);

	case GF_ISOM_BOX_TYPE_PASP: return pasp_Write(a, bs);
	case GF_ISOM_BOX_TYPE_TSEL: return tsel_Write(a, bs);

	case GF_ISOM_BOX_TYPE_METX:
	case GF_ISOM_BOX_TYPE_METT:
		return metx_Write(a, bs);

	case GF_ISOM_BOX_TYPE_DIMS: return dims_Write(a, bs);
	case GF_ISOM_BOX_TYPE_DIMC: return dimC_Write(a, bs);
	case GF_ISOM_BOX_TYPE_DIST: return diST_Write(a, bs);

	case GF_ISOM_BOX_TYPE_AC3: return ac3_Write(a, bs); 
	case GF_ISOM_BOX_TYPE_DAC3: return dac3_Write(a, bs); 

	case GF_ISOM_BOX_TYPE_LSRC: return lsrc_Write(a, bs); 
	case GF_ISOM_BOX_TYPE_LSR1: return lsr1_Write(a, bs); 

	case GF_ISOM_BOX_TYPE_SIDX: return sidx_Write(a, bs); 
	case GF_ISOM_BOX_TYPE_SUBS: return subs_Write(a, bs);
	case GF_ISOM_BOX_TYPE_TFDT: return tfdt_Write(a, bs); 
	case GF_ISOM_BOX_TYPE_RVCC: return rvcc_Write(a, bs); 

	default:
		return defa_Write(a, bs);
	}
}


GF_Err gf_isom_box_size(GF_Box *a)
{
	switch (a->type) {
	case GF_ISOM_BOX_TYPE_REFT:
		return reftype_Size(a);
	case GF_ISOM_BOX_TYPE_FREE:
	case GF_ISOM_BOX_TYPE_SKIP:
		return free_Size(a);
	case GF_ISOM_BOX_TYPE_MDAT: return mdat_Size(a);
	case GF_ISOM_BOX_TYPE_MOOV: return moov_Size(a);
	case GF_ISOM_BOX_TYPE_MVHD: return mvhd_Size(a);
	case GF_ISOM_BOX_TYPE_MDHD: return mdhd_Size(a);
	case GF_ISOM_BOX_TYPE_VMHD: return vmhd_Size(a);
	case GF_ISOM_BOX_TYPE_SMHD: return smhd_Size(a);
	case GF_ISOM_BOX_TYPE_HMHD: return hmhd_Size(a);
	case GF_ISOM_BOX_TYPE_ODHD:
	case GF_ISOM_BOX_TYPE_CRHD:
	case GF_ISOM_BOX_TYPE_SDHD:
	case GF_ISOM_BOX_TYPE_NMHD:
		return nmhd_Size(a);
	case GF_ISOM_BOX_TYPE_STBL: return stbl_Size(a);
	case GF_ISOM_BOX_TYPE_DINF: return dinf_Size(a);
	case GF_ISOM_BOX_TYPE_URL: return url_Size(a);
	case GF_ISOM_BOX_TYPE_URN: return urn_Size(a);
	case GF_ISOM_BOX_TYPE_CHPL: return chpl_Size(a);
	case GF_ISOM_BOX_TYPE_CPRT: return cprt_Size(a);
	case GF_ISOM_BOX_TYPE_HDLR: return hdlr_Size(a);
	case GF_ISOM_BOX_TYPE_IODS: return iods_Size(a);
	case GF_ISOM_BOX_TYPE_TRAK: return trak_Size(a);
	case GF_ISOM_BOX_TYPE_MP4S: return mp4s_Size(a);
	case GF_ISOM_BOX_TYPE_MP4V: return mp4v_Size(a);
	case GF_ISOM_BOX_TYPE_MP4A: return mp4a_Size(a);
	case GF_ISOM_BOX_TYPE_GNRM: return gnrm_Size(a);
	case GF_ISOM_BOX_TYPE_GNRV: return gnrv_Size(a);
	case GF_ISOM_BOX_TYPE_GNRA: return gnra_Size(a);
	case GF_ISOM_BOX_TYPE_EDTS: return edts_Size(a);
	case GF_ISOM_BOX_TYPE_UDTA: return udta_Size(a);
	case GF_ISOM_BOX_TYPE_DREF: return dref_Size(a);
	case GF_ISOM_BOX_TYPE_STSD: return stsd_Size(a);
	case GF_ISOM_BOX_TYPE_STTS: return stts_Size(a);
	case GF_ISOM_BOX_TYPE_CTTS: return ctts_Size(a);
	case GF_ISOM_BOX_TYPE_STSH: return stsh_Size(a);
	case GF_ISOM_BOX_TYPE_ELST: return elst_Size(a);
	case GF_ISOM_BOX_TYPE_STSC: return stsc_Size(a);
	case GF_ISOM_BOX_TYPE_STZ2:
	case GF_ISOM_BOX_TYPE_STSZ:
		return stsz_Size(a);
	case GF_ISOM_BOX_TYPE_STCO: return stco_Size(a);
	case GF_ISOM_BOX_TYPE_STSS: return stss_Size(a);
	case GF_ISOM_BOX_TYPE_SDTP: return sdtp_Size(a);
	case GF_ISOM_BOX_TYPE_CO64: return co64_Size(a);
	case GF_ISOM_BOX_TYPE_ESDS: return esds_Size(a);
	case GF_ISOM_BOX_TYPE_MINF: return minf_Size(a);
	case GF_ISOM_BOX_TYPE_TKHD: return tkhd_Size(a);
	case GF_ISOM_BOX_TYPE_TREF: return tref_Size(a);
	case GF_ISOM_BOX_TYPE_MDIA: return mdia_Size(a);
	case GF_ISOM_BOX_TYPE_FTYP: 
	case GF_ISOM_BOX_TYPE_STYP: 
        return ftyp_Size(a);
	case GF_ISOM_BOX_TYPE_FADB: return padb_Size(a);
	case GF_ISOM_BOX_TYPE_VOID: return void_Size(a);
	case GF_ISOM_BOX_TYPE_STSF: return stsf_Size(a);
	case GF_ISOM_BOX_TYPE_PDIN: return pdin_Size(a);
	
#ifndef GPAC_DISABLE_ISOM_HINTING
	case GF_ISOM_BOX_TYPE_RTP_STSD: return ghnt_Size(a);
	case GF_ISOM_BOX_TYPE_RTPO: return rtpo_Size(a);
	case GF_ISOM_BOX_TYPE_HNTI: return hnti_Size(a);
	case GF_ISOM_BOX_TYPE_SDP: return sdp_Size(a);
	case GF_ISOM_BOX_TYPE_HINF: return hinf_Size(a);
	case GF_ISOM_BOX_TYPE_RELY: return rely_Size(a);
	case GF_ISOM_BOX_TYPE_TIMS: return tims_Size(a);
	case GF_ISOM_BOX_TYPE_TSRO: return tsro_Size(a);
	case GF_ISOM_BOX_TYPE_SNRO: return snro_Size(a);
	case GF_ISOM_BOX_TYPE_TRPY: return trpy_Size(a);
	case GF_ISOM_BOX_TYPE_NUMP: return nump_Size(a);
	case GF_ISOM_BOX_TYPE_TOTL: return totl_Size(a);
	case GF_ISOM_BOX_TYPE_NPCK: return npck_Size(a);
	case GF_ISOM_BOX_TYPE_TPYL: return tpyl_Size(a);
	case GF_ISOM_BOX_TYPE_TPAY: return tpay_Size(a);
	case GF_ISOM_BOX_TYPE_MAXR: return maxr_Size(a);
	case GF_ISOM_BOX_TYPE_DMED: return dmed_Size(a);
	case GF_ISOM_BOX_TYPE_DIMM: return dimm_Size(a);
	case GF_ISOM_BOX_TYPE_DREP: return drep_Size(a);
	case GF_ISOM_BOX_TYPE_TMIN: return tmin_Size(a);
	case GF_ISOM_BOX_TYPE_TMAX: return tmax_Size(a);
	case GF_ISOM_BOX_TYPE_PMAX: return pmax_Size(a);
	case GF_ISOM_BOX_TYPE_DMAX: return dmax_Size(a);
	case GF_ISOM_BOX_TYPE_PAYT: return payt_Size(a);
	case GF_ISOM_BOX_TYPE_NAME: return name_Size(a);
#endif /*GPAC_DISABLE_ISOM_HINTING*/


#ifndef	GPAC_DISABLE_ISOM_FRAGMENTS
	case GF_ISOM_BOX_TYPE_MVEX: return mvex_Size(a);
	case GF_ISOM_BOX_TYPE_MEHD: return mehd_Size(a);
	case GF_ISOM_BOX_TYPE_TREX: return trex_Size(a);
	case GF_ISOM_BOX_TYPE_MOOF: return moof_Size(a);
	case GF_ISOM_BOX_TYPE_MFHD: return mfhd_Size(a);
	case GF_ISOM_BOX_TYPE_TRAF: return traf_Size(a);
	case GF_ISOM_BOX_TYPE_TFHD: return tfhd_Size(a);
	case GF_ISOM_BOX_TYPE_TRUN: return trun_Size(a);
#endif
	
	/*3GPP boxes*/
	case GF_ISOM_SUBTYPE_3GP_AMR:
	case GF_ISOM_SUBTYPE_3GP_AMR_WB:
	case GF_ISOM_SUBTYPE_3GP_EVRC:
	case GF_ISOM_SUBTYPE_3GP_QCELP:
	case GF_ISOM_SUBTYPE_3GP_SMV:
		return gppa_Size(a);
	case GF_ISOM_SUBTYPE_3GP_H263: return gppv_Size(a);
	case GF_ISOM_BOX_TYPE_DAMR:
	case GF_ISOM_BOX_TYPE_DEVC: 
	case GF_ISOM_BOX_TYPE_DQCP:
	case GF_ISOM_BOX_TYPE_DSMV:
	case GF_ISOM_BOX_TYPE_D263:
		return gppc_Size(a);

	case GF_ISOM_BOX_TYPE_AVCC: 
	case GF_ISOM_BOX_TYPE_SVCC: 
		return avcc_Size(a);
	case GF_ISOM_BOX_TYPE_BTRT: return btrt_Size(a);
	case GF_ISOM_BOX_TYPE_M4DS: return m4ds_Size(a);
	case GF_ISOM_BOX_TYPE_AVC1: 
	case GF_ISOM_BOX_TYPE_AVC2: 
	case GF_ISOM_BOX_TYPE_SVC1: 
		return mp4v_Size(a);

	/*3GPP streaming text*/
	case GF_ISOM_BOX_TYPE_FTAB: return ftab_Size(a);
	case GF_ISOM_BOX_TYPE_TX3G: return tx3g_Size(a);
	case GF_ISOM_BOX_TYPE_TEXT:	return text_Size(a);
	case GF_ISOM_BOX_TYPE_STYL: return styl_Size(a);
	case GF_ISOM_BOX_TYPE_HLIT: return hlit_Size(a);
	case GF_ISOM_BOX_TYPE_HCLR: return hclr_Size(a);
	case GF_ISOM_BOX_TYPE_KROK: return krok_Size(a);
	case GF_ISOM_BOX_TYPE_DLAY: return dlay_Size(a);
	case GF_ISOM_BOX_TYPE_HREF: return href_Size(a);
	case GF_ISOM_BOX_TYPE_TBOX: return tbox_Size(a);
	case GF_ISOM_BOX_TYPE_BLNK: return blnk_Size(a);
	case GF_ISOM_BOX_TYPE_TWRP: return twrp_Size(a);

	/* ISMA 1.0 Encryption and Authentication V 1.0 */
	case GF_ISOM_BOX_TYPE_IKMS: return iKMS_Size(a);
	case GF_ISOM_BOX_TYPE_ISFM: return iSFM_Size(a);

	/* ISO FF extensions for MPEG-21 */
	case GF_ISOM_BOX_TYPE_META: return meta_Size(a);
	case GF_ISOM_BOX_TYPE_XML: return xml_Size(a);
	case GF_ISOM_BOX_TYPE_BXML: return bxml_Size(a);
	case GF_ISOM_BOX_TYPE_ILOC: return iloc_Size(a);
	case GF_ISOM_BOX_TYPE_PITM: return pitm_Size(a);
	case GF_ISOM_BOX_TYPE_IPRO: return ipro_Size(a);
	case GF_ISOM_BOX_TYPE_INFE: return infe_Size(a);
	case GF_ISOM_BOX_TYPE_IINF: return iinf_Size(a);
	case GF_ISOM_BOX_TYPE_SINF: return sinf_Size(a);
	case GF_ISOM_BOX_TYPE_FRMA: return frma_Size(a);
	case GF_ISOM_BOX_TYPE_SCHM: return schm_Size(a);
	case GF_ISOM_BOX_TYPE_SCHI: return schi_Size(a);
	case GF_ISOM_BOX_TYPE_ENCA: return mp4a_Size(a);
	case GF_ISOM_BOX_TYPE_ENCV: return mp4v_Size(a);
	case GF_ISOM_BOX_TYPE_ENCS: return mp4s_Size(a);
	case GF_ISOM_BOX_TYPE_UUID: return uuid_Size(a);

	/* Apple extensions */
	case GF_ISOM_BOX_TYPE_ILST: return ilst_Size(a);

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
	case GF_ISOM_BOX_TYPE_0xA9ENC:
	case GF_ISOM_BOX_TYPE_aART:
	case GF_ISOM_BOX_TYPE_GNRE:
	case GF_ISOM_BOX_TYPE_DISK:
	case GF_ISOM_BOX_TYPE_TRKN:
	case GF_ISOM_BOX_TYPE_TMPO:
	case GF_ISOM_BOX_TYPE_CPIL:
	case GF_ISOM_BOX_TYPE_PGAP:
	case GF_ISOM_BOX_TYPE_COVR:
		return ListItem_Size(a);

	case GF_ISOM_BOX_TYPE_DATA: return data_Size(a);

	case GF_ISOM_BOX_TYPE_OHDR: return ohdr_Size(a);
	case GF_ISOM_BOX_TYPE_GRPI: return grpi_Size(a);
	case GF_ISOM_BOX_TYPE_MDRI: return mdri_Size(a);
	case GF_ISOM_BOX_TYPE_ODTT: return odtt_Size(a);
	case GF_ISOM_BOX_TYPE_ODRB: return odrb_Size(a);
	case GF_ISOM_BOX_TYPE_ODKM: return odkm_Size(a);
	case GF_ISOM_BOX_TYPE_ODAF: return iSFM_Size(a);

	case GF_ISOM_BOX_TYPE_PASP: return pasp_Size(a);
	case GF_ISOM_BOX_TYPE_TSEL: return tsel_Size(a);

	case GF_ISOM_BOX_TYPE_METX:
	case GF_ISOM_BOX_TYPE_METT:
		return metx_Size(a);

	case GF_ISOM_BOX_TYPE_DIMS: return dims_Size(a);
	case GF_ISOM_BOX_TYPE_DIMC: return dimC_Size(a);
	case GF_ISOM_BOX_TYPE_DIST: return diST_Size(a);

	case GF_ISOM_BOX_TYPE_AC3: return ac3_Size(a); 
	case GF_ISOM_BOX_TYPE_DAC3: return dac3_Size(a); 

	case GF_ISOM_BOX_TYPE_LSRC: return lsrc_Size(a); 
	case GF_ISOM_BOX_TYPE_LSR1: return lsr1_Size(a); 

	case GF_ISOM_BOX_TYPE_SIDX: return sidx_Size(a); 
	case GF_ISOM_BOX_TYPE_SUBS: return subs_Size(a);
	case GF_ISOM_BOX_TYPE_TFDT: return tfdt_Size(a);
	case GF_ISOM_BOX_TYPE_RVCC: return rvcc_Size(a);

	default: return defa_Size(a);
	}
}

#endif /*GPAC_DISABLE_ISOM_WRITE*/ 

#endif /*GPAC_DISABLE_ISOM*/

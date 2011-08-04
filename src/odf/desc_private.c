/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Copyright (c) Jean Le Feuvre 2000-2005
 *					All rights reserved
 *
 *  This file is part of GPAC / MPEG-4 ObjectDescriptor sub-project
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

#include <gpac/internal/odf_dev.h>


//
//		CONSTRUCTORS
//
GF_Descriptor *gf_odf_create_descriptor(u8 tag)
{
	GF_Descriptor *desc;

	switch (tag) {
	case GF_ODF_IOD_TAG:
		return gf_odf_new_iod();
	case GF_ODF_OD_TAG:
		return gf_odf_new_od();
	case GF_ODF_ESD_TAG:
		return gf_odf_new_esd();
	case GF_ODF_DCD_TAG:
		return gf_odf_new_dcd();
	case GF_ODF_SLC_TAG:
		//default : we create it without any predefinition...
		return gf_odf_new_slc(0);
	case GF_ODF_MUXINFO_TAG:
		return gf_odf_new_muxinfo();
	case GF_ODF_BIFS_CFG_TAG:
		return gf_odf_new_bifs_cfg();
	case GF_ODF_UI_CFG_TAG:
		return gf_odf_new_ui_cfg();
	case GF_ODF_TEXT_CFG_TAG:
		return gf_odf_new_text_cfg();
	case GF_ODF_TX3G_TAG:
		return gf_odf_new_tx3g();
	case GF_ODF_ELEM_MASK_TAG:
		return gf_odf_New_ElemMask();
	case GF_ODF_LASER_CFG_TAG:
		return gf_odf_new_laser_cfg();

	case GF_ODF_DSI_TAG:
		desc = gf_odf_new_default();
		if (!desc) return desc;
		desc->tag = GF_ODF_DSI_TAG;
		return desc;

	case GF_ODF_AUX_VIDEO_DATA:
		return gf_odf_new_auxvid();

	case GF_ODF_SEGMENT_TAG:
		return gf_odf_new_segment();
	case GF_ODF_MEDIATIME_TAG:
		return gf_odf_new_mediatime();

	//File Format Specific
	case GF_ODF_ISOM_IOD_TAG:
		return gf_odf_new_isom_iod();
	case GF_ODF_ISOM_OD_TAG:
		return gf_odf_new_isom_od();
	case GF_ODF_ESD_INC_TAG:
		return gf_odf_new_esd_inc();
	case GF_ODF_ESD_REF_TAG:
		return gf_odf_new_esd_ref();
	case GF_ODF_LANG_TAG:
		return gf_odf_new_lang();

#ifndef GPAC_MINIMAL_ODF

	case GF_ODF_CI_TAG:
		return gf_odf_new_ci();
	case GF_ODF_SCI_TAG:
		return gf_odf_new_sup_cid();
	case GF_ODF_IPI_PTR_TAG:
		return gf_odf_new_ipi_ptr();
	//special case for the file format
	case GF_ODF_ISOM_IPI_PTR_TAG:
		desc = gf_odf_new_ipi_ptr();
		if (!desc) return desc;
		desc->tag = GF_ODF_ISOM_IPI_PTR_TAG;
		return desc;

	case GF_ODF_IPMP_PTR_TAG:
		return gf_odf_new_ipmp_ptr();
	case GF_ODF_IPMP_TAG:
		return gf_odf_new_ipmp();
	case GF_ODF_QOS_TAG:
		return gf_odf_new_qos();
	case GF_ODF_REG_TAG:
		return gf_odf_new_reg();
	case GF_ODF_CC_TAG:
		return gf_odf_new_cc();
	case GF_ODF_KW_TAG:
		return gf_odf_new_kw();
	case GF_ODF_RATING_TAG:
		return gf_odf_new_rating();
	case GF_ODF_SHORT_TEXT_TAG:
		return gf_odf_new_short_text();
	case GF_ODF_TEXT_TAG:
		return gf_odf_new_exp_text();
	case GF_ODF_CC_NAME_TAG:
		return gf_odf_new_cc_name();
	case GF_ODF_CC_DATE_TAG:
		return gf_odf_new_cc_date();
	case GF_ODF_OCI_NAME_TAG:
		return gf_odf_new_oci_name();
	case GF_ODF_OCI_DATE_TAG:
		return gf_odf_new_oci_date();
	case GF_ODF_SMPTE_TAG:
		return gf_odf_new_smpte_camera();
	case GF_ODF_EXT_PL_TAG:
		return gf_odf_new_pl_ext();
	case GF_ODF_PL_IDX_TAG:
		return gf_odf_new_pl_idx();

	case GF_ODF_IPMP_TL_TAG:
		return gf_odf_new_ipmp_tool_list();
	case GF_ODF_IPMP_TOOL_TAG:
		return gf_odf_new_ipmp_tool();

	case 0:
	case 0xFF:
		return NULL;
#endif /*GPAC_MINIMAL_ODF*/
	default:
		//ISO Reserved
		if ( (tag >= GF_ODF_ISO_RES_BEGIN_TAG) &&
			(tag <= GF_ODF_ISO_RES_END_TAG) ) {
			return NULL;
		}
		desc = gf_odf_new_default();
		if (!desc) return desc;
		desc->tag = tag;
		return desc;
	}
}

//
//		DESTRUCTORS
//
GF_Err gf_odf_delete_descriptor(GF_Descriptor *desc)
{
	switch (desc->tag) {
	case GF_ODF_IOD_TAG :
		return gf_odf_del_iod((GF_InitialObjectDescriptor *)desc);
	case GF_ODF_OD_TAG:
		return gf_odf_del_od((GF_ObjectDescriptor *)desc);
	case GF_ODF_ESD_TAG :
		return gf_odf_del_esd((GF_ESD *)desc);
	case GF_ODF_DCD_TAG :
		return gf_odf_del_dcd((GF_DecoderConfig *)desc);
	case GF_ODF_SLC_TAG:
		return gf_odf_del_slc((GF_SLConfig *)desc);

	case GF_ODF_ISOM_IOD_TAG:
		return gf_odf_del_isom_iod((GF_IsomInitialObjectDescriptor *)desc);
	case GF_ODF_ISOM_OD_TAG:
		return gf_odf_del_isom_od((GF_IsomObjectDescriptor *)desc);

	case GF_ODF_SEGMENT_TAG:
		return gf_odf_del_segment((GF_Segment *) desc);
	case GF_ODF_MEDIATIME_TAG:
		return gf_odf_del_mediatime((GF_MediaTime *) desc);

	case GF_ODF_MUXINFO_TAG:
		return gf_odf_del_muxinfo((GF_MuxInfo *)desc);
	case GF_ODF_BIFS_CFG_TAG:
		return gf_odf_del_bifs_cfg((GF_BIFSConfig *)desc);
	case GF_ODF_UI_CFG_TAG:
		return gf_odf_del_ui_cfg((GF_UIConfig *)desc);
	case GF_ODF_TEXT_CFG_TAG:
		return gf_odf_del_text_cfg((GF_TextConfig *)desc);
	case GF_ODF_TX3G_TAG:
		return gf_odf_del_tx3g((GF_TextSampleDescriptor*)desc);
	case GF_ODF_LASER_CFG_TAG:
		return gf_odf_del_laser_cfg((GF_LASERConfig *)desc);

	case GF_ODF_AUX_VIDEO_DATA:
		return gf_odf_del_auxvid((GF_AuxVideoDescriptor *)desc);

	case GF_ODF_LANG_TAG:
		return gf_odf_del_lang((GF_Language *)desc);

#ifndef GPAC_MINIMAL_ODF

	case GF_ODF_CC_TAG:
		return gf_odf_del_cc((GF_CCDescriptor *)desc);
	case GF_ODF_CC_DATE_TAG:
		return gf_odf_del_cc_date((GF_CC_Date *)desc);
	case GF_ODF_CC_NAME_TAG:
		return gf_odf_del_cc_name((GF_CC_Name *)desc);
	case GF_ODF_CI_TAG:
		return gf_odf_del_ci((GF_CIDesc *)desc);
	case GF_ODF_ESD_INC_TAG:
		return gf_odf_del_esd_inc((GF_ES_ID_Inc *)desc);
	case GF_ODF_ESD_REF_TAG:
		return gf_odf_del_esd_ref((GF_ES_ID_Ref *)desc);
	case GF_ODF_TEXT_TAG:
		return gf_odf_del_exp_text((GF_ExpandedTextual *)desc);
	case GF_ODF_EXT_PL_TAG:
		return gf_odf_del_pl_ext((GF_PLExt *)desc);
	case GF_ODF_IPI_PTR_TAG:
	case GF_ODF_ISOM_IPI_PTR_TAG:
		return gf_odf_del_ipi_ptr((GF_IPIPtr *)desc);
	case GF_ODF_IPMP_TAG:
		return gf_odf_del_ipmp((GF_IPMP_Descriptor *)desc);
	case GF_ODF_IPMP_PTR_TAG:
		return gf_odf_del_ipmp_ptr((GF_IPMPPtr *)desc);
	case GF_ODF_KW_TAG:
		return gf_odf_del_kw((GF_KeyWord *)desc);
	case GF_ODF_OCI_DATE_TAG:
		return gf_odf_del_oci_date((GF_OCI_Data *)desc);
	case GF_ODF_OCI_NAME_TAG:
		return gf_odf_del_oci_name((GF_OCICreators *)desc);
	case GF_ODF_PL_IDX_TAG:
		return gf_odf_del_pl_idx((GF_PL_IDX *)desc);
	case GF_ODF_QOS_TAG:
		return gf_odf_del_qos((GF_QoS_Descriptor *)desc);
	case GF_ODF_RATING_TAG:
		return gf_odf_del_rating((GF_Rating *)desc);
	case GF_ODF_REG_TAG:
		return gf_odf_del_reg((GF_Registration *)desc);
	case GF_ODF_SHORT_TEXT_TAG:
		return gf_odf_del_short_text((GF_ShortTextual *)desc);
	case GF_ODF_SMPTE_TAG:
		return gf_odf_del_smpte_camera((GF_SMPTECamera *)desc);
	case GF_ODF_SCI_TAG:
		return gf_odf_del_sup_cid((GF_SCIDesc *)desc);

	case GF_ODF_IPMP_TL_TAG:
		return gf_odf_del_ipmp_tool_list((GF_IPMP_ToolList *)desc);
	case GF_ODF_IPMP_TOOL_TAG:
		return gf_odf_del_ipmp_tool((GF_IPMP_Tool *)desc);

#endif /*GPAC_MINIMAL_ODF*/

	default:
		return gf_odf_del_default((GF_DefaultDescriptor *)desc);
	}
	return GF_OK;
}




//
//		READERS
//
GF_Err gf_odf_read_descriptor(GF_BitStream *bs, GF_Descriptor *desc, u32 DescSize)
{
	switch (desc->tag) {
	case GF_ODF_IOD_TAG :
		return gf_odf_read_iod(bs, (GF_InitialObjectDescriptor *)desc, DescSize);
	case GF_ODF_ESD_TAG :
		return gf_odf_read_esd(bs, (GF_ESD *)desc, DescSize);
	case GF_ODF_DCD_TAG :
		return gf_odf_read_dcd(bs, (GF_DecoderConfig *)desc, DescSize);
	case GF_ODF_SLC_TAG :
		return gf_odf_read_slc(bs, (GF_SLConfig *)desc, DescSize);
	case GF_ODF_OD_TAG:
		return gf_odf_read_od(bs, (GF_ObjectDescriptor *)desc, DescSize);

	//MP4 File Format
	case GF_ODF_ISOM_IOD_TAG:
		return gf_odf_read_isom_iod(bs, (GF_IsomInitialObjectDescriptor *)desc, DescSize);
	case GF_ODF_ISOM_OD_TAG:
		return gf_odf_read_isom_od(bs, (GF_IsomObjectDescriptor *)desc, DescSize);
	case GF_ODF_ESD_INC_TAG:
		return gf_odf_read_esd_inc(bs, (GF_ES_ID_Inc *)desc, DescSize);
	case GF_ODF_ESD_REF_TAG:
		return gf_odf_read_esd_ref(bs, (GF_ES_ID_Ref *)desc, DescSize);

	case GF_ODF_SEGMENT_TAG:
		return gf_odf_read_segment(bs, (GF_Segment *) desc, DescSize);
	case GF_ODF_MEDIATIME_TAG:
		return gf_odf_read_mediatime(bs, (GF_MediaTime *) desc, DescSize);
	case GF_ODF_MUXINFO_TAG:
		return gf_odf_read_muxinfo(bs, (GF_MuxInfo *) desc, DescSize);

	case GF_ODF_AUX_VIDEO_DATA:
		return gf_odf_read_auxvid(bs, (GF_AuxVideoDescriptor *)desc, DescSize);

	case GF_ODF_LANG_TAG:
		return gf_odf_read_lang(bs, (GF_Language *)desc, DescSize);

#ifndef GPAC_MINIMAL_ODF
	case GF_ODF_IPMP_TAG:
		return gf_odf_read_ipmp(bs, (GF_IPMP_Descriptor *)desc, DescSize);
	case GF_ODF_IPMP_PTR_TAG:
		return gf_odf_read_ipmp_ptr(bs, (GF_IPMPPtr *)desc, DescSize);

	case GF_ODF_CC_TAG:
		return gf_odf_read_cc(bs, (GF_CCDescriptor *)desc, DescSize);
	case GF_ODF_CC_DATE_TAG:
		return gf_odf_read_cc_date(bs, (GF_CC_Date *)desc, DescSize);
	case GF_ODF_CC_NAME_TAG:
		return gf_odf_read_cc_name(bs, (GF_CC_Name *)desc, DescSize);
	case GF_ODF_CI_TAG:
		return gf_odf_read_ci(bs, (GF_CIDesc *)desc, DescSize);
	case GF_ODF_TEXT_TAG:
		return gf_odf_read_exp_text(bs, (GF_ExpandedTextual *)desc, DescSize);
	case GF_ODF_EXT_PL_TAG:
		return gf_odf_read_pl_ext(bs, (GF_PLExt *)desc, DescSize);
	case GF_ODF_IPI_PTR_TAG:
	case GF_ODF_ISOM_IPI_PTR_TAG:
		return gf_odf_read_ipi_ptr(bs, (GF_IPIPtr *)desc, DescSize);
	case GF_ODF_KW_TAG:
		return gf_odf_read_kw(bs, (GF_KeyWord *)desc, DescSize);
	case GF_ODF_OCI_DATE_TAG:
		return gf_odf_read_oci_date(bs, (GF_OCI_Data *)desc, DescSize);
	case GF_ODF_OCI_NAME_TAG:
		return gf_odf_read_oci_name(bs, (GF_OCICreators *)desc, DescSize);
	case GF_ODF_PL_IDX_TAG:
		return gf_odf_read_pl_idx(bs, (GF_PL_IDX *)desc, DescSize);
	case GF_ODF_QOS_TAG:
		return gf_odf_read_qos(bs, (GF_QoS_Descriptor *)desc, DescSize);
	case GF_ODF_RATING_TAG:
		return gf_odf_read_rating(bs, (GF_Rating *)desc, DescSize);
	case GF_ODF_REG_TAG:
		return gf_odf_read_reg(bs, (GF_Registration *)desc, DescSize);
	case GF_ODF_SHORT_TEXT_TAG:
		return gf_odf_read_short_text(bs, (GF_ShortTextual *)desc, DescSize);
	case GF_ODF_SMPTE_TAG:
		return gf_odf_read_smpte_camera(bs, (GF_SMPTECamera *)desc, DescSize);
	case GF_ODF_SCI_TAG:
		return gf_odf_read_sup_cid(bs, (GF_SCIDesc *)desc, DescSize);
		
	case GF_ODF_IPMP_TL_TAG:
		return gf_odf_read_ipmp_tool_list(bs, (GF_IPMP_ToolList *)desc, DescSize);
	case GF_ODF_IPMP_TOOL_TAG:
		return gf_odf_read_ipmp_tool(bs, (GF_IPMP_Tool *)desc, DescSize);

#endif /*GPAC_MINIMAL_ODF*/
	//default:
	case GF_ODF_DSI_TAG:
	default:
		return gf_odf_read_default(bs, (GF_DefaultDescriptor *)desc, DescSize);
	}
	return GF_OK;
}





//
//		SIZE FUNCTION
//
GF_Err gf_odf_size_descriptor(GF_Descriptor *desc, u32 *outSize)
{
	switch(desc->tag) {
	case GF_ODF_IOD_TAG : 
		return gf_odf_size_iod((GF_InitialObjectDescriptor *)desc, outSize);
	case GF_ODF_ESD_TAG : 
		return gf_odf_size_esd((GF_ESD *)desc, outSize);
	case GF_ODF_DCD_TAG : 
		return gf_odf_size_dcd((GF_DecoderConfig *)desc, outSize);
	case GF_ODF_SLC_TAG : 
		return gf_odf_size_slc((GF_SLConfig *)desc, outSize);

	case GF_ODF_OD_TAG:
		return gf_odf_size_od((GF_ObjectDescriptor *)desc, outSize);
	case GF_ODF_ISOM_IOD_TAG:
		return gf_odf_size_isom_iod((GF_IsomInitialObjectDescriptor *)desc, outSize);
	case GF_ODF_ISOM_OD_TAG:
		return gf_odf_size_isom_od((GF_IsomObjectDescriptor *)desc, outSize);
	case GF_ODF_ESD_INC_TAG:
		return gf_odf_size_esd_inc((GF_ES_ID_Inc *)desc, outSize);
	case GF_ODF_ESD_REF_TAG:
		return gf_odf_size_esd_ref((GF_ES_ID_Ref *)desc, outSize);

	case GF_ODF_SEGMENT_TAG:
		return gf_odf_size_segment((GF_Segment *) desc, outSize);
	case GF_ODF_MEDIATIME_TAG:
		return gf_odf_size_mediatime((GF_MediaTime *) desc, outSize);
	case GF_ODF_MUXINFO_TAG:
		return gf_odf_size_muxinfo((GF_MuxInfo *) desc, outSize);
		
	case GF_ODF_AUX_VIDEO_DATA:
		return gf_odf_size_auxvid((GF_AuxVideoDescriptor *)desc, outSize);

	case GF_ODF_LANG_TAG:
		return gf_odf_size_lang((GF_Language *)desc, outSize);

#ifndef GPAC_MINIMAL_ODF
	case GF_ODF_CC_TAG:
		return gf_odf_size_cc((GF_CCDescriptor *)desc, outSize);
	case GF_ODF_CC_DATE_TAG:
		return gf_odf_size_cc_date((GF_CC_Date *)desc, outSize);
	case GF_ODF_CC_NAME_TAG:
		return gf_odf_size_cc_name((GF_CC_Name *)desc, outSize);
	case GF_ODF_CI_TAG:
		return gf_odf_size_ci((GF_CIDesc *)desc, outSize);
	case GF_ODF_TEXT_TAG:
		return gf_odf_size_exp_text((GF_ExpandedTextual *)desc, outSize);
	case GF_ODF_EXT_PL_TAG:
		return gf_odf_size_pl_ext((GF_PLExt *)desc, outSize);
	case GF_ODF_IPI_PTR_TAG:
	case GF_ODF_ISOM_IPI_PTR_TAG:
		return gf_odf_size_ipi_ptr((GF_IPIPtr *)desc, outSize);
	case GF_ODF_IPMP_TAG:
		return gf_odf_size_ipmp((GF_IPMP_Descriptor *)desc, outSize);
	case GF_ODF_IPMP_PTR_TAG:
		return gf_odf_size_ipmp_ptr((GF_IPMPPtr *)desc, outSize);
	case GF_ODF_KW_TAG:
		return gf_odf_size_kw((GF_KeyWord *)desc, outSize);
	case GF_ODF_OCI_DATE_TAG:
		return gf_odf_size_oci_date((GF_OCI_Data *)desc, outSize);
	case GF_ODF_OCI_NAME_TAG:
		return gf_odf_size_oci_name((GF_OCICreators *)desc, outSize);
	case GF_ODF_PL_IDX_TAG:
		return gf_odf_size_pl_idx((GF_PL_IDX *)desc, outSize);
	case GF_ODF_QOS_TAG:
		return gf_odf_size_qos((GF_QoS_Descriptor *)desc, outSize);
	case GF_ODF_RATING_TAG:
		return gf_odf_size_rating((GF_Rating *)desc, outSize);
	case GF_ODF_REG_TAG:
		return gf_odf_size_reg((GF_Registration *)desc, outSize);
	case GF_ODF_SHORT_TEXT_TAG:
		return gf_odf_size_short_text((GF_ShortTextual *)desc, outSize);
	case GF_ODF_SMPTE_TAG:
		return gf_odf_size_smpte_camera((GF_SMPTECamera *)desc, outSize);
	case GF_ODF_SCI_TAG:
		return gf_odf_size_sup_cid((GF_SCIDesc *)desc, outSize);


	case GF_ODF_IPMP_TL_TAG:
		return gf_odf_size_ipmp_tool_list((GF_IPMP_ToolList *)desc, outSize);
	case GF_ODF_IPMP_TOOL_TAG:
		return gf_odf_size_ipmp_tool((GF_IPMP_Tool *)desc, outSize);

#endif /*GPAC_MINIMAL_ODF*/
	default:
		/*don't write out l descriptors*/
		if ((desc->tag>=GF_ODF_MUXINFO_TAG) && (desc->tag<=GF_ODF_LASER_CFG_TAG)) {
			*outSize = 0;
			return GF_OK;
		}
		return gf_odf_size_default((GF_DefaultDescriptor *)desc, outSize);
	}
	return GF_OK;
}


//
//		WRITERS
//
GF_Err gf_odf_write_descriptor(GF_BitStream *bs, GF_Descriptor *desc)
{
	switch(desc->tag) {
	case GF_ODF_IOD_TAG : 
		return gf_odf_write_iod(bs, (GF_InitialObjectDescriptor *)desc);
	case GF_ODF_ESD_TAG : 
		return gf_odf_write_esd(bs, (GF_ESD *)desc);
	case GF_ODF_DCD_TAG : 
		return gf_odf_write_dcd(bs, (GF_DecoderConfig *)desc);
	case GF_ODF_SLC_TAG : 
		return gf_odf_write_slc(bs, (GF_SLConfig *)desc);
	case GF_ODF_ESD_INC_TAG:
		return gf_odf_write_esd_inc(bs, (GF_ES_ID_Inc *)desc);
	case GF_ODF_ESD_REF_TAG:
		return gf_odf_write_esd_ref(bs, (GF_ES_ID_Ref *)desc);


	case GF_ODF_ISOM_IOD_TAG:
		return gf_odf_write_isom_iod(bs, (GF_IsomInitialObjectDescriptor *)desc);
	case GF_ODF_ISOM_OD_TAG:
		return gf_odf_write_isom_od(bs, (GF_IsomObjectDescriptor *)desc);
	case GF_ODF_OD_TAG:
		return gf_odf_write_od(bs, (GF_ObjectDescriptor *)desc);
	case GF_ODF_SEGMENT_TAG:
		return gf_odf_write_segment(bs, (GF_Segment *) desc);
	case GF_ODF_MEDIATIME_TAG:
		return gf_odf_write_mediatime(bs, (GF_MediaTime *) desc);
	case GF_ODF_MUXINFO_TAG:
		return gf_odf_write_muxinfo(bs, (GF_MuxInfo *) desc);

	case GF_ODF_AUX_VIDEO_DATA:
		return gf_odf_write_auxvid(bs, (GF_AuxVideoDescriptor *)desc);

	case GF_ODF_LANG_TAG:
		return gf_odf_write_lang(bs, (GF_Language *)desc);

#ifndef GPAC_MINIMAL_ODF
	case GF_ODF_CC_TAG:
		return gf_odf_write_cc(bs, (GF_CCDescriptor *)desc);
	case GF_ODF_CC_DATE_TAG:
		return gf_odf_write_cc_date(bs, (GF_CC_Date *)desc);
	case GF_ODF_CC_NAME_TAG:
		return gf_odf_write_cc_name(bs, (GF_CC_Name *)desc);
	case GF_ODF_CI_TAG:
		return gf_odf_write_ci(bs, (GF_CIDesc *)desc);

	case GF_ODF_TEXT_TAG:
		return gf_odf_write_exp_text(bs, (GF_ExpandedTextual *)desc);
	case GF_ODF_EXT_PL_TAG:
		return gf_odf_write_pl_ext(bs, (GF_PLExt *)desc);
	case GF_ODF_IPI_PTR_TAG:
	case GF_ODF_ISOM_IPI_PTR_TAG:
		return gf_odf_write_ipi_ptr(bs, (GF_IPIPtr *)desc);
	case GF_ODF_IPMP_TAG:
		return gf_odf_write_ipmp(bs, (GF_IPMP_Descriptor *)desc);
	case GF_ODF_IPMP_PTR_TAG:
		return gf_odf_write_ipmp_ptr(bs, (GF_IPMPPtr *)desc);
	case GF_ODF_KW_TAG:
		return gf_odf_write_kw(bs, (GF_KeyWord *)desc);
	case GF_ODF_OCI_DATE_TAG:
		return gf_odf_write_oci_date(bs, (GF_OCI_Data *)desc);
	case GF_ODF_OCI_NAME_TAG:
		return gf_odf_write_oci_name(bs, (GF_OCICreators *)desc);
	case GF_ODF_PL_IDX_TAG:
		return gf_odf_write_pl_idx(bs, (GF_PL_IDX *)desc);
	case GF_ODF_QOS_TAG:
		return gf_odf_write_qos(bs, (GF_QoS_Descriptor *)desc);
	case GF_ODF_RATING_TAG:
		return gf_odf_write_rating(bs, (GF_Rating *)desc);
	case GF_ODF_REG_TAG:
		return gf_odf_write_reg(bs, (GF_Registration *)desc);
	case GF_ODF_SHORT_TEXT_TAG:
		return gf_odf_write_short_text(bs, (GF_ShortTextual *)desc);
	case GF_ODF_SMPTE_TAG:
		return gf_odf_write_smpte_camera(bs, (GF_SMPTECamera *)desc);
	case GF_ODF_SCI_TAG:
		return gf_odf_write_sup_cid(bs, (GF_SCIDesc *)desc);

	case GF_ODF_IPMP_TL_TAG:
		return gf_odf_write_ipmp_tool_list(bs, (GF_IPMP_ToolList *)desc);
	case GF_ODF_IPMP_TOOL_TAG:
		return gf_odf_write_ipmp_tool(bs, (GF_IPMP_Tool *)desc);
#endif /*GPAC_MINIMAL_ODF*/
	default:
		/*don't write out internal descriptors*/
		if ((desc->tag>=GF_ODF_MUXINFO_TAG) && (desc->tag<=GF_ODF_LASER_CFG_TAG))
			return GF_OK;
		return gf_odf_write_default(bs, (GF_DefaultDescriptor *)desc);
	}
	return GF_OK;
}

//
//		CONSTRUCTORS
//
GF_ODCom *gf_odf_create_command(u8 tag)
{
	GF_ODCom *com;
	switch (tag) {
	case GF_ODF_OD_UPDATE_TAG:
		return gf_odf_new_od_update();
	case GF_ODF_OD_REMOVE_TAG:
		return gf_odf_new_od_remove();
	case GF_ODF_ESD_UPDATE_TAG:
		return gf_odf_new_esd_update();
	case GF_ODF_ESD_REMOVE_TAG:
		return gf_odf_new_esd_remove();
	//special case for ESDRemove in the file format...
	case GF_ODF_ESD_REMOVE_REF_TAG:
		com = gf_odf_new_esd_remove();
		if (!com) return com;
		com->tag = GF_ODF_ESD_REMOVE_REF_TAG;
		return com;

	case GF_ODF_IPMP_UPDATE_TAG:
		return gf_odf_new_ipmp_update();
	case GF_ODF_IPMP_REMOVE_TAG:
		return gf_odf_new_ipmp_remove();

	default:
		if ( (tag >= GF_ODF_COM_ISO_BEGIN_TAG) && 
			( tag <= GF_ODF_COM_ISO_END_TAG) ) {
			return NULL;
		}
		com = gf_odf_new_base_command();
		if (!com) return com;
		com->tag = tag;
		return com;
	}
}


//
//		DESTRUCTORS
//
GF_Err gf_odf_delete_command(GF_ODCom *com)
{
	switch (com->tag) {
	case GF_ODF_OD_UPDATE_TAG:
		return gf_odf_del_od_update((GF_ODUpdate *)com);
	case GF_ODF_OD_REMOVE_TAG:
		return gf_odf_del_od_remove((GF_ODRemove *)com);

	case GF_ODF_ESD_UPDATE_TAG:
		return gf_odf_del_esd_update((GF_ESDUpdate *)com);
	case GF_ODF_ESD_REMOVE_TAG:
	case GF_ODF_ESD_REMOVE_REF_TAG:
		return gf_odf_del_esd_remove((GF_ESDRemove *)com);
	case GF_ODF_IPMP_UPDATE_TAG:
		return gf_odf_del_ipmp_update((GF_IPMPUpdate *)com);
	case GF_ODF_IPMP_REMOVE_TAG:
		return gf_odf_del_ipmp_remove((GF_IPMPRemove *)com);

	default:
		return gf_odf_del_base_command((GF_BaseODCom *)com);
	}
}


//
//		READERS
//
GF_Err gf_odf_read_command(GF_BitStream *bs, GF_ODCom *com, u32 gf_odf_size_command)
{
	switch (com->tag) {
	case GF_ODF_OD_UPDATE_TAG:
		return gf_odf_read_od_update(bs, (GF_ODUpdate *)com, gf_odf_size_command);
	case GF_ODF_OD_REMOVE_TAG:
		return gf_odf_read_od_remove(bs, (GF_ODRemove *)com, gf_odf_size_command);
	case GF_ODF_ESD_UPDATE_TAG:
		return gf_odf_read_esd_update(bs, (GF_ESDUpdate *)com, gf_odf_size_command);
	case GF_ODF_ESD_REMOVE_TAG:
	case GF_ODF_ESD_REMOVE_REF_TAG:
		return gf_odf_read_esd_remove(bs, (GF_ESDRemove *)com, gf_odf_size_command);
	case GF_ODF_IPMP_UPDATE_TAG:
		return gf_odf_read_ipmp_update(bs, (GF_IPMPUpdate *)com, gf_odf_size_command);
	case GF_ODF_IPMP_REMOVE_TAG:
		return gf_odf_read_ipmp_remove(bs, (GF_IPMPRemove *)com, gf_odf_size_command);	
	default:
		return gf_odf_read_base_command(bs, (GF_BaseODCom *)com, gf_odf_size_command);
	}
}



//
//		SIZE FUNCTION
//
GF_Err gf_odf_size_command(GF_ODCom *com, u32 *outSize)
{
	switch (com->tag) {
	case GF_ODF_OD_UPDATE_TAG:
		return gf_odf_size_od_update((GF_ODUpdate *)com, outSize);
	case GF_ODF_OD_REMOVE_TAG:
		return gf_odf_size_od_remove((GF_ODRemove *)com, outSize);

	case GF_ODF_ESD_UPDATE_TAG:
		return gf_odf_size_esd_update((GF_ESDUpdate *)com, outSize);
	case GF_ODF_ESD_REMOVE_TAG:
	case GF_ODF_ESD_REMOVE_REF_TAG:
		return gf_odf_size_esd_remove((GF_ESDRemove *)com, outSize);
	case GF_ODF_IPMP_UPDATE_TAG:
		return gf_odf_size_ipmp_update((GF_IPMPUpdate *)com, outSize);
	case GF_ODF_IPMP_REMOVE_TAG:
		return gf_odf_size_ipmp_remove((GF_IPMPRemove *)com, outSize);

	default:
		return gf_odf_size_base_command((GF_BaseODCom *)com, outSize);
	}
}


//
//		WRITERS
//
GF_Err gf_odf_write_command(GF_BitStream *bs, GF_ODCom *com)
{
	switch (com->tag) {
	case GF_ODF_OD_UPDATE_TAG:
		return gf_odf_write_od_update(bs, (GF_ODUpdate *)com);
	case GF_ODF_OD_REMOVE_TAG:
		return gf_odf_write_od_remove(bs, (GF_ODRemove *)com);

	case GF_ODF_ESD_UPDATE_TAG:
		return gf_odf_write_esd_update(bs, (GF_ESDUpdate *)com);
	case GF_ODF_ESD_REMOVE_TAG:
	case GF_ODF_ESD_REMOVE_REF_TAG:
		return gf_odf_write_esd_remove(bs, (GF_ESDRemove *)com);
	case GF_ODF_IPMP_UPDATE_TAG:
		return gf_odf_write_ipmp_update(bs, (GF_IPMPUpdate *)com);
	case GF_ODF_IPMP_REMOVE_TAG:
		return gf_odf_write_ipmp_remove(bs, (GF_IPMPRemove *)com);
	
	default:
		return gf_odf_write_base_command(bs, (GF_BaseODCom *)com);
	}
}

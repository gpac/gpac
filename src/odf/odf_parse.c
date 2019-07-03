/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2000-2012
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
#include <gpac/token.h>
#include <gpac/constants.h>
/*for import flags*/
#include <gpac/media_tools.h>


/* to complete...*/

u32 gf_odf_get_field_type(GF_Descriptor *desc, char *fieldName)
{
	switch (desc->tag) {
	case GF_ODF_IOD_TAG:
	case GF_ODF_OD_TAG:
		if (!stricmp(fieldName, "esDescr")) return GF_ODF_FT_OD_LIST;
		else if (!stricmp(fieldName, "ociDescr")) return GF_ODF_FT_OD_LIST;
		else if (!stricmp(fieldName, "ipmpDescrPtr")) return GF_ODF_FT_OD_LIST;
		else if (!stricmp(fieldName, "ipmpDescr")) return GF_ODF_FT_OD_LIST;
		else if (!stricmp(fieldName, "extDescr")) return GF_ODF_FT_OD_LIST;
		else if (!stricmp(fieldName, "toolListDescr")) return GF_ODF_FT_OD;
		return 0;
	case GF_ODF_DCD_TAG:
		if (!stricmp(fieldName, "decSpecificInfo")) return GF_ODF_FT_OD;
		if (!stricmp(fieldName, "profileLevelIndicationIndexDescr")) return GF_ODF_FT_OD_LIST;
		return 0;
	case GF_ODF_ESD_TAG:
		if (!stricmp(fieldName, "decConfigDescr")) return GF_ODF_FT_OD;
		if (!stricmp(fieldName, "muxInfo")) return GF_ODF_FT_OD;
		if (!stricmp(fieldName, "StreamSource")) return GF_ODF_FT_OD;
		if (!stricmp(fieldName, "slConfigDescr")) return GF_ODF_FT_OD;
		if (!stricmp(fieldName, "ipiPtr")) return GF_ODF_FT_OD;
		if (!stricmp(fieldName, "qosDescr")) return GF_ODF_FT_OD;
		if (!stricmp(fieldName, "regDescr")) return GF_ODF_FT_OD;
		if (!stricmp(fieldName, "langDescr")) return GF_ODF_FT_OD;
		if (!stricmp(fieldName, "ipIDS")) return GF_ODF_FT_OD_LIST;
		if (!stricmp(fieldName, "ipmpDescrPtr")) return GF_ODF_FT_OD_LIST;
		if (!stricmp(fieldName, "extDescr")) return GF_ODF_FT_OD_LIST;
		return 0;
	case GF_ODF_TEXT_CFG_TAG:
		if (!stricmp(fieldName, "SampleDescriptions")) return GF_ODF_FT_OD_LIST;
		return 0;
	case GF_ODF_IPMP_TAG:
		if (!stricmp(fieldName, "IPMPX_Data")) return GF_ODF_FT_IPMPX_LIST;
		return 0;
	case GF_ODF_IPMP_TL_TAG:
		if (!stricmp(fieldName, "ipmpTool")) return GF_ODF_FT_OD_LIST;
		return 0;
	case GF_ODF_IPMP_TOOL_TAG:
		if (!stricmp(fieldName, "toolParamDesc")) return GF_ODF_FT_IPMPX;
		return 0;
	case GF_ODF_BIFS_CFG_TAG:
		if (!stricmp(fieldName, "elementaryMask")) return GF_ODF_FT_OD_LIST;
		return 0;
	}
	return 0;
}

u32 gf_odf_get_tag_by_name(char *descName)
{
	if (!stricmp(descName, "ObjectDescriptor")) return GF_ODF_OD_TAG;
	if (!stricmp(descName, "InitialObjectDescriptor")) return GF_ODF_IOD_TAG;
	if (!stricmp(descName, "ES_Descriptor")) return GF_ODF_ESD_TAG;
	if (!stricmp(descName, "DecoderConfigDescriptor")) return GF_ODF_DCD_TAG;
	if (!stricmp(descName, "DecoderSpecificInfo")) return GF_ODF_DSI_TAG;
	if (!stricmp(descName, "DecoderSpecificInfoString")) return GF_ODF_DSI_TAG;
	if (!stricmp(descName, "SLConfigDescriptor")) return GF_ODF_SLC_TAG;
	if (!stricmp(descName, "SegmentDescriptor")) return GF_ODF_SEGMENT_TAG;
	if (!stricmp(descName, "MuxInfo")) return GF_ODF_MUXINFO_TAG;
	if (!stricmp(descName, "StreamSource")) return GF_ODF_MUXINFO_TAG;
	if (!stricmp(descName, "BIFSConfig") || !stricmp(descName, "BIFSv2Config")) return GF_ODF_BIFS_CFG_TAG;
	if (!stricmp(descName, "ElementaryMask")) return GF_ODF_ELEM_MASK_TAG;
	if (!stricmp(descName, "TextConfig")) return GF_ODF_TEXT_CFG_TAG;
	if (!stricmp(descName, "TextSampleDescriptor")) return GF_ODF_TX3G_TAG;
	if (!stricmp(descName, "UIConfig")) return GF_ODF_UI_CFG_TAG;
	if (!stricmp(descName, "ES_ID_Ref")) return GF_ODF_ESD_REF_TAG;
	if (!stricmp(descName, "ES_ID_Inc")) return GF_ODF_ESD_INC_TAG;
	if (!stricmp(descName, "AuxiliaryVideoData")) return GF_ODF_AUX_VIDEO_DATA;
	if (!stricmp(descName, "DefaultDescriptor")) return GF_ODF_DSI_TAG;
	if (!stricmp(descName, "LanguageDescriptor")) return GF_ODF_LANG_TAG;
	if (!stricmp(descName, "GPACLanguage")) return GF_ODF_GPAC_LANG;

#ifndef GPAC_MINIMAL_ODF
	if (!stricmp(descName, "MediaTimeDescriptor")) return GF_ODF_MEDIATIME_TAG;
	if (!stricmp(descName, "ContentIdentification")) return GF_ODF_CI_TAG;
	if (!stricmp(descName, "SuppContentIdentification")) return GF_ODF_SCI_TAG;
	if (!stricmp(descName, "IPIPtr")) return GF_ODF_IPI_PTR_TAG;
	if (!stricmp(descName, "IPMP_DescriptorPointer")) return GF_ODF_IPMP_PTR_TAG;
	if (!stricmp(descName, "IPMP_Descriptor")) return GF_ODF_IPMP_TAG;
	if (!stricmp(descName, "IPMP_ToolListDescriptor")) return GF_ODF_IPMP_TL_TAG;
	if (!stricmp(descName, "IPMP_Tool")) return GF_ODF_IPMP_TOOL_TAG;
	if (!stricmp(descName, "QoS")) return GF_ODF_QOS_TAG;
	if (!stricmp(descName, "RegistrationDescriptor")) return GF_ODF_REG_TAG;
	if (!stricmp(descName, "ExtensionPL")) return GF_ODF_EXT_PL_TAG;
	if (!stricmp(descName, "PL_IndicationIndex")) return GF_ODF_PL_IDX_TAG;
	if (!stricmp(descName, "ContentClassification")) return GF_ODF_CC_TAG;
	if (!stricmp(descName, "KeyWordDescriptor")) return GF_ODF_KW_TAG;
	if (!stricmp(descName, "RatingDescriptor")) return GF_ODF_RATING_TAG;
	if (!stricmp(descName, "ShortTextualDescriptor")) return GF_ODF_SHORT_TEXT_TAG;
	if (!stricmp(descName, "ExpandedTextualDescriptor")) return GF_ODF_TEXT_TAG;
	if (!stricmp(descName, "ContentCreatorName")) return GF_ODF_CC_NAME_TAG;
	if (!stricmp(descName, "ContentCreationDate")) return GF_ODF_CC_DATE_TAG;
	if (!stricmp(descName, "OCI_CreatorName")) return GF_ODF_OCI_NAME_TAG;
	if (!stricmp(descName, "OCI_CreationDate")) return GF_ODF_OCI_DATE_TAG;
	if (!stricmp(descName, "SmpteCameraPosition")) return GF_ODF_SMPTE_TAG;
#endif /*GPAC_MINIMAL_ODF*/
	return 0;
}

#include <gpac/internal/odf_parse_common.h>

void OD_ParseBinData(u8 *val, u8 **out_data, u32 *out_data_size)
{
	u32 i, c;
	char s[3];
	u32 len = (u32) strlen(val) / 3;
	if (*out_data) gf_free(*out_data);
	*out_data_size = len;
	*out_data = gf_malloc(sizeof(char) * len);
	s[2] = 0;
	for (i=0; i<len; i++) {
		s[0] = val[3*i+1];
		s[1] = val[3*i+2];
		sscanf(s, "%02X", &c);
		(*out_data)[i] = (unsigned char) c;
	}
}

GF_Err gf_odf_set_field(GF_Descriptor *desc, char *fieldName, char *val)
{
	Bool OD_ParseUIConfig(u8 *val, u8 **out_data, u32 *out_data_size);
	u32 ret = 0;

	if (!stricmp(val, "auto")) return GF_OK;
	else if (!stricmp(val, "unspecified")) return GF_OK;

	switch (desc->tag) {
	case GF_ODF_IOD_TAG:
	{
		GF_InitialObjectDescriptor *iod = (GF_InitialObjectDescriptor *)desc;
		if (!stricmp(fieldName, "objectDescriptorID") || !stricmp(fieldName, "binaryID")) ret += sscanf(val, "%hu", &iod->objectDescriptorID);
		else if (!stricmp(fieldName, "URLString")) {
			iod->URLString = gf_strdup(val);
			ret = 1;
		}
		else if (!stricmp(fieldName, "includeInlineProfileLevelFlag")) {
			GET_BOOL(iod->inlineProfileFlag)
			if (!ret) {
				iod->inlineProfileFlag = 0;
				ret = 1;
			}
		}
		else if (!stricmp(fieldName, "ODProfileLevelIndication")) {
			GET_U8(iod->OD_profileAndLevel)
			if (!ret) {
				iod->OD_profileAndLevel = 0xFE;
				ret = 1;
			}
		}
		else if (!stricmp(fieldName, "sceneProfileLevelIndication")) {
			GET_U8(iod->scene_profileAndLevel)
			if (!ret) {
				iod->scene_profileAndLevel = 0xFE;
				ret = 1;
			}
		}
		else if (!stricmp(fieldName, "audioProfileLevelIndication")) {
			GET_U8(iod->audio_profileAndLevel)
			if (!ret) {
				iod->audio_profileAndLevel = 0xFE;
				ret = 1;
			}
		}
		else if (!stricmp(fieldName, "visualProfileLevelIndication")) {
			GET_U8(iod->visual_profileAndLevel)
			if (!ret) {
				iod->visual_profileAndLevel = 0xFE;
				ret = 1;
			}
		}
		else if (!stricmp(fieldName, "graphicsProfileLevelIndication")) {
			GET_U8(iod->graphics_profileAndLevel)
			if (!ret) {
				iod->graphics_profileAndLevel = 0xFE;
				ret = 1;
			}
		}
	}
	break;
	case GF_ODF_OD_TAG:
	{
		GF_ObjectDescriptor *od = (GF_ObjectDescriptor *) desc;
		if (!stricmp(fieldName, "objectDescriptorID") || !stricmp(fieldName, "binaryID")) ret += sscanf(val, "%hu", &od->objectDescriptorID);
		else if (!stricmp(fieldName, "URLString")) {
			od->URLString = gf_strdup(val);
			ret = 1;
		}
	}
	break;
	case GF_ODF_DCD_TAG:
	{
		GF_DecoderConfig *dcd = (GF_DecoderConfig *)desc;
		if (!stricmp(fieldName, "objectTypeIndication")) {
			GET_U8(dcd->objectTypeIndication)
			/*XMT may use string*/
			if (!ret) {
				if (!stricmp(val, "MPEG4Systems1")) {
					dcd->objectTypeIndication = GF_CODECID_OD_V1;
					ret = 1;
				}
				else if (!stricmp(val, "MPEG4Systems2")) {
					dcd->objectTypeIndication = GF_CODECID_BIFS_V2;
					ret = 1;
				}
				else if (!stricmp(val, "MPEG4Visual")) {
					dcd->objectTypeIndication = GF_CODECID_MPEG4_PART2;
					ret = 1;
				}
				else if (!stricmp(val, "MPEG4Audio")) {
					dcd->objectTypeIndication = GF_CODECID_AAC_MPEG4;
					ret = 1;
				}
				else if (!stricmp(val, "MPEG2VisualSimple")) {
					dcd->objectTypeIndication = GF_CODECID_MPEG2_SIMPLE;
					ret = 1;
				}
				else if (!stricmp(val, "MPEG2VisualMain")) {
					dcd->objectTypeIndication = GF_CODECID_MPEG2_MAIN;
					ret = 1;
				}
				else if (!stricmp(val, "MPEG2VisualSNR")) {
					dcd->objectTypeIndication = GF_CODECID_MPEG2_SNR;
					ret = 1;
				}
				else if (!stricmp(val, "MPEG2VisualSpatial")) {
					dcd->objectTypeIndication = GF_CODECID_MPEG2_SPATIAL;
					ret = 1;
				}
				else if (!stricmp(val, "MPEG2VisualHigh")) {
					dcd->objectTypeIndication = GF_CODECID_MPEG2_HIGH;
					ret = 1;
				}
				else if (!stricmp(val, "MPEG2Visual422")) {
					dcd->objectTypeIndication = GF_CODECID_MPEG2_422;
					ret = 1;
				}
				else if (!stricmp(val, "MPEG2AudioMain")) {
					dcd->objectTypeIndication = GF_CODECID_AAC_MPEG2_MP;
					ret = 1;
				}
				else if (!stricmp(val, "MPEG2AudioLowComplexity")) {
					dcd->objectTypeIndication = GF_CODECID_AAC_MPEG2_LCP;
					ret = 1;
				}
				else if (!stricmp(val, "MPEG2AudioScaleableSamplingRate")) {
					dcd->objectTypeIndication = GF_CODECID_AAC_MPEG2_SSRP;
					ret = 1;
				}
				else if (!stricmp(val, "MPEG2AudioPart3")) {
					dcd->objectTypeIndication = GF_CODECID_MPEG2_PART3;
					ret = 1;
				}
				else if (!stricmp(val, "MPEG1Visual")) {
					dcd->objectTypeIndication = GF_CODECID_MPEG1;
					ret = 1;
				}
				else if (!stricmp(val, "MPEG1Audio")) {
					dcd->objectTypeIndication = GF_CODECID_MPEG_AUDIO;
					ret = 1;
				}
				else if (!stricmp(val, "JPEG")) {
					dcd->objectTypeIndication = GF_CODECID_JPEG;
					ret = 1;
				}
				else if (!stricmp(val, "PNG")) {
					dcd->objectTypeIndication = GF_CODECID_PNG;
					ret = 1;
				}
			}
		}
		else if (!stricmp(fieldName, "streamType")) {
			GET_U8(dcd->streamType)
			/*XMT may use string*/
			if (!ret) {
				if (!stricmp(val, "ObjectDescriptor")) {
					dcd->streamType = GF_STREAM_OD;
					ret = 1;
				}
				else if (!stricmp(val, "ClockReference")) {
					dcd->streamType = GF_STREAM_OCR;
					ret = 1;
				}
				else if (!stricmp(val, "SceneDescription")) {
					dcd->streamType = GF_STREAM_SCENE;
					ret = 1;
				}
				else if (!stricmp(val, "Visual")) {
					dcd->streamType = GF_STREAM_VISUAL;
					ret = 1;
				}
				else if (!stricmp(val, "Audio")) {
					dcd->streamType = GF_STREAM_AUDIO;
					ret = 1;
				}
				else if (!stricmp(val, "MPEG7")) {
					dcd->streamType = GF_STREAM_MPEG7;
					ret = 1;
				}
				else if (!stricmp(val, "IPMP")) {
					dcd->streamType = GF_STREAM_IPMP;
					ret = 1;
				}
				else if (!stricmp(val, "OCI")) {
					dcd->streamType = GF_STREAM_OCI;
					ret = 1;
				}
				else if (!stricmp(val, "MPEGJ")) {
					dcd->streamType = GF_STREAM_MPEGJ;
					ret = 1;
				}
			}
		}
		else if (!stricmp(fieldName, "upStream")) GET_BOOL(dcd->upstream)
			else if (!stricmp(fieldName, "bufferSizeDB")) ret += sscanf(val, "%u", &dcd->bufferSizeDB);
			else if (!stricmp(fieldName, "maxBitRate")) ret += sscanf(val, "%u", &dcd->maxBitrate);
			else if (!stricmp(fieldName, "avgBitRate")) ret += sscanf(val, "%u", &dcd->avgBitrate);
	}
	break;
	case GF_ODF_ESD_TAG:
	{
		GF_ESD *esd = (GF_ESD *)desc;
		if (!stricmp(fieldName, "ES_ID") || !stricmp(fieldName, "binaryID")) {
			ret += sscanf(val, "%hu", &esd->ESID);
		}
		else if (!stricmp(fieldName, "streamPriority")) GET_U8(esd->streamPriority)
		else if (!stricmp(fieldName, "dependsOn_ES_ID") || !stricmp(fieldName, "dependsOnESID")) ret += sscanf(val, "%hu", &esd->dependsOnESID);
		else if (!stricmp(fieldName, "OCR_ES_ID")) ret += sscanf(val, "%hu", &esd->OCRESID);
		else if (!stricmp(fieldName, "URLstring")) {
			esd->URLString = gf_strdup(val);
			ret = 1;
		}
		/*ignore*/
		else if (!stricmp(fieldName, "streamDependenceFlag")
			 || !stricmp(fieldName, "URL_Flag")
			 || !stricmp(fieldName, "OCRstreamFlag")
		)
			ret = 1;
	}
	break;
	case GF_ODF_SLC_TAG:
	{
		u32 ts;
		GF_SLConfig *slc = (GF_SLConfig*)desc;
		if (!stricmp(fieldName, "predefined")) GET_U8(slc->predefined)
		else if (!stricmp(fieldName, "useAccessUnitStartFlag")) GET_BOOL(slc->useAccessUnitStartFlag)
		else if (!stricmp(fieldName, "useAccessUnitEndFlag")) GET_BOOL(slc->useAccessUnitEndFlag)
		else if (!stricmp(fieldName, "useRandomAccessPointFlag")) GET_BOOL(slc->useRandomAccessPointFlag)
		else if (!stricmp(fieldName, "hasRandomAccessUnitsOnlyFlag") || !stricmp(fieldName, "useRandomAccessUnitsOnlyFlag")) GET_BOOL(slc->hasRandomAccessUnitsOnlyFlag)
		else if (!stricmp(fieldName, "usePaddingFlag")) GET_BOOL(slc->usePaddingFlag)
		else if (!stricmp(fieldName, "useTimeStampsFlag")) GET_BOOL(slc->useTimestampsFlag)
		else if (!stricmp(fieldName, "useIdleFlag")) GET_BOOL(slc->useIdleFlag)
		else if (!stricmp(fieldName, "timeStampResolution")) ret += sscanf(val, "%u", &slc->timestampResolution);
		else if (!stricmp(fieldName, "OCRResolution")) ret += sscanf(val, "%u", &slc->OCRResolution);
		else if (!stricmp(fieldName, "timeStampLength")) GET_U8(slc->timestampLength)
		else if (!stricmp(fieldName, "OCRLength")) GET_U8(slc->OCRLength)
		else if (!stricmp(fieldName, "AU_Length")) GET_U8(slc->AULength)
		else if (!stricmp(fieldName, "instantBitrateLength")) GET_U8(slc->instantBitrateLength)
		else if (!stricmp(fieldName, "degradationPriorityLength")) GET_U8(slc->degradationPriorityLength)
		else if (!stricmp(fieldName, "AU_seqNumLength")) GET_U8(slc->AUSeqNumLength)
		else if (!stricmp(fieldName, "packetSeqNumLength")) GET_U8(slc->packetSeqNumLength)
		else if (!stricmp(fieldName, "timeScale")) ret += sscanf(val, "%u", &slc->timeScale);
		else if (!stricmp(fieldName, "accessUnitDuration")) ret += sscanf(val, "%hu", &slc->AUDuration);
		else if (!stricmp(fieldName, "compositionUnitDuration")) ret += sscanf(val, "%hu", &slc->CUDuration);
		else if (!stricmp(fieldName, "startDecodingTimeStamp")) {
			ret += sscanf(val, "%u", &ts);
			slc->startDTS = ts;
		}
		else if (!stricmp(fieldName, "startCompositionTimeStamp")) {
			ret += sscanf(val, "%u", &ts);
			slc->startCTS = ts;
		}
		else if (!stricmp(fieldName, "durationFlag")) ret = 1;
	}
	break;
	case GF_ODF_ELEM_MASK_TAG:
	{
		GF_ElementaryMask* em = (GF_ElementaryMask*)desc;
		if (!stricmp(fieldName, "atNode")) {
			GET_U32(em->node_id);
			if (!ret || !em->node_id) em->node_name = gf_strdup(val);
			ret = 1;
		}
		else if (!stricmp(fieldName, "numDynFields")) ret = 1;
	}
	break;
	case GF_ODF_BIFS_CFG_TAG:
	{
		GF_BIFSConfig *bcd = (GF_BIFSConfig*)desc;
		if (!stricmp(val, "auto")) return GF_OK;
		if (!stricmp(fieldName, "nodeIDbits")) ret += sscanf(val, "%hu", &bcd->nodeIDbits);
		else if (!stricmp(fieldName, "routeIDbits")) ret += sscanf(val, "%hu", &bcd->routeIDbits);
		else if (!stricmp(fieldName, "protoIDbits")) ret += sscanf(val, "%hu", &bcd->protoIDbits);
		else if (!stricmp(fieldName, "isCommandStream")) {
			/*GET_BOOL(bcd->isCommandStream)*/ ret = 1;
		}
		else if (!stricmp(fieldName, "pixelMetric") || !stricmp(fieldName, "pixelMetrics")) GET_BOOL(bcd->pixelMetrics)
		else if (!stricmp(fieldName, "pixelWidth")) ret += sscanf(val, "%hu", &bcd->pixelWidth);
		else if (!stricmp(fieldName, "pixelHeight")) ret += sscanf(val, "%hu", &bcd->pixelHeight);
		else if (!stricmp(fieldName, "use3DMeshCoding")) ret = 1;
		else if (!stricmp(fieldName, "usePredictiveMFField")) ret = 1;
		else if (!stricmp(fieldName, "randomAccess")) GET_BOOL(bcd->randomAccess)
		else if (!stricmp(fieldName, "useNames")) GET_BOOL(bcd->useNames)
	}
	break;
	case GF_ODF_MUXINFO_TAG:
	{
		GF_MuxInfo *mi = (GF_MuxInfo *)desc;
		if (!stricmp(fieldName, "fileName") || !stricmp(fieldName, "url")) GET_STRING(mi->file_name)
		else if (!stricmp(fieldName, "streamFormat")) GET_STRING(mi->streamFormat)
		else if (!stricmp(fieldName, "GroupID")) ret += sscanf(val, "%u", &mi->GroupID);
		else if (!stricmp(fieldName, "startTime")) ret += sscanf(val, "%d", &mi->startTime);
		else if (!stricmp(fieldName, "duration")) ret += sscanf(val, "%u", &mi->duration);
		else if (!stricmp(fieldName, "carouselPeriod")) {
			ret += sscanf(val, "%u", &mi->carousel_period_plus_one);
			mi->carousel_period_plus_one += 1;
		}
		else if (!stricmp(fieldName, "aggregateOnESID")) ret += sscanf(val, "%hu", &mi->aggregate_on_esid);

#ifndef GPAC_DISABLE_MEDIA_IMPORT
		else if (!stricmp(fieldName, "compactSize"))
		{
			ret = 1;
			if (!stricmp(val, "true") || !stricmp(val, "1")) mi->import_flags |= GF_IMPORT_USE_COMPACT_SIZE;
		}
		else if (!stricmp(fieldName, "useDataReference")) {
			ret = 1;
			if (!stricmp(val, "true") || !stricmp(val, "1")) mi->import_flags |= GF_IMPORT_USE_DATAREF;
		}
		else if (!stricmp(fieldName, "noFrameDrop")) {
			ret = 1;
			if (!stricmp(val, "true") || !stricmp(val, "1")) mi->import_flags |= GF_IMPORT_NO_FRAME_DROP;
		}
		else if (!stricmp(fieldName, "SBR_Type")) {
			ret = 1;
			if (!stricmp(val, "implicit") || !stricmp(val, "1")) mi->import_flags |= GF_IMPORT_SBR_IMPLICIT;
			else if (!stricmp(val, "explicit") || !stricmp(val, "2")) mi->import_flags |= GF_IMPORT_SBR_EXPLICIT;
		}
#endif /*GPAC_DISABLE_MEDIA_IMPORT*/
		else if (!stricmp(fieldName, "textNode")) GET_STRING(mi->textNode)
		else if (!stricmp(fieldName, "fontNode")) GET_STRING(mi->fontNode)
		else if (!stricmp(fieldName, "frameRate")) {
			ret = 1;
			mi->frame_rate = atof(val);
		}
	}
	break;
	case GF_ODF_DSI_TAG:
	{
		GF_DefaultDescriptor *dsi = (GF_DefaultDescriptor*)desc;
		if (!stricmp(fieldName, "info")) {
			/*only parse true hexa strings*/
			if (val[0] == '%') {
				OD_ParseBinData(val, &dsi->data, &dsi->dataLength);
				ret = 1;
			} else if (!strnicmp(val, "file:", 5)) {
				gf_file_load_data(val+5, (u8 **) &dsi->data, &dsi->dataLength);
				ret = 1;
			} else if (!strlen(val)) ret = 1;
		}
		if (!stricmp(fieldName, "src")) {
			u32 len = (u32) strlen("data:application/octet-string,");
			if (strnicmp(val, "data:application/octet-string,", len)) break;
			val += len;
			/*only parse true hexa strings*/
			if (val[0] == '%') {
				OD_ParseBinData(val, &dsi->data, &dsi->dataLength);
				ret = 1;
			} else if (!strnicmp(val, "file:", 5)) {
				gf_file_load_data(val+5, (u8 **) &dsi->data, &dsi->dataLength);
				ret = 1;
			}
		}
	}
	break;
	case GF_ODF_SEGMENT_TAG:
	{
		GF_Segment *sd = (GF_Segment*)desc;
		if (!stricmp(fieldName, "start") || !stricmp(fieldName, "startTime")) GET_DOUBLE(sd->startTime)
		else if (!stricmp(fieldName, "duration")) GET_DOUBLE(sd->Duration)
		else if (!stricmp(fieldName, "name") || !stricmp(fieldName, "segmentName")) GET_STRING(sd->SegmentName)
	}
	break;
	case GF_ODF_UI_CFG_TAG:
	{
		GF_UIConfig *uic = (GF_UIConfig*)desc;
		if (!stricmp(fieldName, "deviceName")) GET_STRING(uic->deviceName)
		else if (!stricmp(fieldName, "termChar")) GET_U8(uic->termChar)
		else if (!stricmp(fieldName, "delChar")) GET_U8(uic->delChar)
		else if (!stricmp(fieldName, "uiData")) {
			/*only parse true hexa strings*/
			if (val[0] == '%') {
				OD_ParseBinData(val, &uic->ui_data, &uic->ui_data_length);
				ret = 1;
			} else if (!strnicmp(val, "file:", 5)) {
				gf_file_load_data(val+5, (u8 **) &uic->ui_data, &uic->ui_data_length);
				ret = 1;
			} else {
				ret = OD_ParseUIConfig(val, &uic->ui_data, &uic->ui_data_length);
			}
		}
	}
	break;
	case GF_ODF_ESD_INC_TAG:
	{
		GF_ES_ID_Inc *inc = (GF_ES_ID_Inc *)desc;
		if (!stricmp(fieldName, "trackID")) ret += sscanf(val, "%u", &inc->trackID);
	}
	break;
	case GF_ODF_ESD_REF_TAG:
	{
		GF_ES_ID_Ref *inc = (GF_ES_ID_Ref *)desc;
		if (!stricmp(fieldName, "trackID")) ret += sscanf(val, "%hu", &inc->trackRef);
	}
	break;
	case GF_ODF_TEXT_CFG_TAG:
	{
		GF_TextConfig *txt = (GF_TextConfig*)desc;
		if (!stricmp(fieldName, "3GPPBaseFormat")) GET_U8(txt->Base3GPPFormat)
		else if (!stricmp(fieldName, "MPEGExtendedFormat")) GET_U8(txt->MPEGExtendedFormat)
		else if (!stricmp(fieldName, "profileLevel")) GET_U8(txt->profileLevel)
		else if (!stricmp(fieldName, "durationClock") || !stricmp(fieldName, "timescale") ) GET_U32(txt->timescale)
		else if (!stricmp(fieldName, "layer")) GET_U32(txt->layer)
		else if (!stricmp(fieldName, "text_width")) GET_U32(txt->text_width)
		else if (!stricmp(fieldName, "text_height")) GET_U32(txt->text_height)
		else if (!stricmp(fieldName, "video_width")) {
			GET_U32(txt->video_width)
			txt->has_vid_info = GF_TRUE;
		}
		else if (!stricmp(fieldName, "video_height")) {
			GET_U32(txt->video_height)
			txt->has_vid_info = GF_TRUE;
		}
		else if (!stricmp(fieldName, "horizontal_offset")) {
			GET_S16(txt->horiz_offset)
			txt->has_vid_info = GF_TRUE;
		}
		else if (!stricmp(fieldName, "vertical_offset")) {
			GET_S32(txt->vert_offset)
			txt->has_vid_info = GF_TRUE;
		}
	}
	break;
	case GF_ODF_TX3G_TAG:
	{
		GF_TextSampleDescriptor *sd = (GF_TextSampleDescriptor*)desc;
		if (!stricmp(fieldName, "displayFlags")) GET_U32(sd->displayFlags)
		else if (!stricmp(fieldName, "horiz_justif")) GET_S32(sd->horiz_justif)
		else if (!stricmp(fieldName, "vert_justif")) GET_S32(sd->vert_justif)
		else if (!stricmp(fieldName, "back_color")) GET_S32(sd->back_color)
		else if (!stricmp(fieldName, "top")) GET_S32(sd->default_pos.top)
		else if (!stricmp(fieldName, "bottom")) GET_S32(sd->default_pos.bottom)
		else if (!stricmp(fieldName, "left")) GET_S32(sd->default_pos.left)
		else if (!stricmp(fieldName, "right")) GET_S32(sd->default_pos.right)
		else if (!stricmp(fieldName, "style_font_ID")) GET_S32(sd->default_style.fontID)
		else if (!stricmp(fieldName, "style_font_size")) GET_S32(sd->default_style.font_size)
		else if (!stricmp(fieldName, "style_text_color")) GET_U32(sd->default_style.text_color)
		else if (!stricmp(fieldName, "style_flags")) {
			char szStyles[1024];
			strcpy(szStyles, val);
			strlwr(szStyles);
			if (strstr(szStyles, "bold")) sd->default_style.style_flags |= GF_TXT_STYLE_BOLD;
			if (strstr(szStyles, "italic")) sd->default_style.style_flags |= GF_TXT_STYLE_ITALIC;
			if (strstr(szStyles, "underlined")) sd->default_style.style_flags |= GF_TXT_STYLE_UNDERLINED;
			ret = 1;
		}
		else if (!stricmp(fieldName, "fontID") || !stricmp(fieldName, "fontName")) {
			/*check if we need a new entry*/
			if (!sd->font_count) {
				sd->fonts = (GF_FontRecord*)gf_malloc(sizeof(GF_FontRecord));
				sd->font_count = 1;
				sd->fonts[0].fontID = 0;
				sd->fonts[0].fontName = NULL;
			} else {
				Bool realloc_fonts = GF_FALSE;
				if (!stricmp(fieldName, "fontID") && sd->fonts[sd->font_count-1].fontID) realloc_fonts = GF_TRUE;
				else if (!stricmp(fieldName, "fontName") && sd->fonts[sd->font_count-1].fontName) realloc_fonts = GF_TRUE;
				if (realloc_fonts) {
					sd->font_count += 1;
					sd->fonts = (GF_FontRecord*)gf_realloc(sd->fonts, sizeof(GF_FontRecord)*sd->font_count);
					sd->fonts[sd->font_count-1].fontID = 0;
					sd->fonts[sd->font_count-1].fontName = NULL;
				}
			}
			if (!stricmp(fieldName, "fontID")) GET_U32(sd->fonts[sd->font_count-1].fontID)
			if (!stricmp(fieldName, "fontName")) GET_STRING(sd->fonts[sd->font_count-1].fontName)
			ret = 1;
		}
	}
	break;
	case GF_ODF_IPMP_TAG:
	{
		GF_IPMP_Descriptor *ipmp = (GF_IPMP_Descriptor*)desc;
		if (!stricmp(fieldName, "IPMP_DescriptorID")) GET_U8(ipmp->IPMP_DescriptorID)
		else if (!stricmp(fieldName, "IPMPS_Type")) GET_U16(ipmp->IPMPS_Type)
		else if (!stricmp(fieldName, "IPMP_DescriptorIDEx")) GET_U16(ipmp->IPMP_DescriptorIDEx)
		else if (!stricmp(fieldName, "IPMP_ToolID")) {
			ret = 1;
			gf_bin128_parse(val, ipmp->IPMP_ToolID);
		}
		else if (!stricmp(fieldName, "controlPointCode")) GET_U8(ipmp->control_point)
		else if (!stricmp(fieldName, "sequenceCode")) GET_U8(ipmp->cp_sequence_code)
	}
	break;
	case GF_ODF_IPMP_PTR_TAG:
	{
		GF_IPMPPtr *ipmpd = (GF_IPMPPtr*)desc;
		if (!stricmp(fieldName, "IPMP_DescriptorID")) GET_U8(ipmpd->IPMP_DescriptorID)
		else if (!stricmp(fieldName, "IPMP_DescriptorIDEx"))  ret += sscanf(val, "%hu", &ipmpd->IPMP_DescriptorIDEx);
		else if (!stricmp(fieldName, "IPMP_ES_ID"))  ret += sscanf(val, "%hu", &ipmpd->IPMP_ES_ID);
	}
	break;
	case GF_ODF_LANG_TAG:
	case GF_ODF_GPAC_LANG:
	{
		GF_Language *ld = (GF_Language *)desc;
		if (!stricmp(fieldName, "languageCode")) {
			u32 li, l = (u32) strlen(val);
			ld->langCode = 0;
			for (li = 0; li < l; li++) {
				/* Warning: sensitive to big endian, little endian */
				ld->langCode |= (val[li] << (l-li-1)*8);
			}
			ret++;
		}
	}
	break;
	case GF_ODF_AUX_VIDEO_DATA:
	{
		GF_AuxVideoDescriptor *avd = (GF_AuxVideoDescriptor *)desc;
		if (!stricmp(fieldName, "aux_video_type"))  GET_U8(avd->aux_video_type)
		else if (!stricmp(fieldName, "position_offset_h"))  GET_U8(avd->position_offset_h)
		else if (!stricmp(fieldName, "position_offset_v"))  GET_U8(avd->position_offset_v)
		else if (!stricmp(fieldName, "knear"))  GET_U8(avd->knear)
		else if (!stricmp(fieldName, "kfar"))  GET_U8(avd->kfar)
		else if (!stricmp(fieldName, "parallax_zero"))  GET_U16(avd->parallax_zero)
		else if (!stricmp(fieldName, "parallax_scale"))  GET_U16(avd->parallax_scale)
		else if (!stricmp(fieldName, "dref"))  GET_U16(avd->dref)
		else if (!stricmp(fieldName, "wref"))  GET_U16(avd->wref)
	}
	break;
	case GF_ODF_IPMP_TOOL_TAG:
	{
		GF_IPMP_Tool *it = (GF_IPMP_Tool*)desc;
		if (!stricmp(fieldName, "IPMP_ToolID")) {
			ret = 1;
			gf_bin128_parse(val, it->IPMP_ToolID);
		}
		else if (!stricmp(fieldName, "ToolURL"))  GET_STRING(it->tool_url)
		}
	break;
	}

	return ret ? GF_OK : GF_BAD_PARAM;
}

Bool OD_ParseUIConfig(u8 *val, u8 **out_data, u32 *out_data_size)
{
#ifndef GPAC_MINIMAL_ODF
	GF_BitStream *bs;
	if (!strnicmp(val, "HTK:", 4)) {
		char szItem[100];
		s64 pos, bs_start, bs_cur;
		Bool has_word;
		u32 nb_phonems, nbWords = 0;
		bs_start = 0;
		nb_phonems = 0;
		bs = gf_bs_new(NULL, 0, GF_BITSTREAM_WRITE);
		/*we'll write the nb of words later on*/
		gf_bs_write_int(bs, 0, 8);
		has_word = GF_FALSE;
		/*parse all words*/
		val += 4;
		while (1) {
			pos = gf_token_get(val, 0, " ;", szItem, 100);
			if (pos>0) val += pos;
			if (!has_word) {
				has_word = GF_TRUE;
				nbWords++;
				nb_phonems = 0;
				bs_start = gf_bs_get_position(bs);
				/*nb phonems*/
				gf_bs_write_int(bs, 0, 8);
				gf_bs_write_data(bs, szItem, (u32) strlen(szItem));
				gf_bs_write_int(bs, 0, 8);
				continue;
			}
			if (pos>0) {

				nb_phonems ++;
				/*would be nicer with a phone book & use indexes*/
				if (!stricmp(szItem, "vcl")) {
					gf_bs_write_data(bs, "vc", 2);
				} else {
					gf_bs_write_data(bs, szItem, 2);
				}

				while (val[0] && (val[0]==' ')) val += 1;
			}

			if ((pos<0) || !val[0] || val[0]==';') {
				if (has_word) {
					has_word = GF_FALSE;
					bs_cur = gf_bs_get_position(bs);
					gf_bs_seek(bs, bs_start);
					gf_bs_write_int(bs, nb_phonems, 8);
					gf_bs_seek(bs, bs_cur);
				}
				if ((pos<0) || !val[0]) break;
				val += 1;
				while (val[0] && (val[0]==' ')) val += 1;
			}
		}
		if (nbWords) {
			bs_cur = gf_bs_get_position(bs);
			gf_bs_seek(bs, 0);
			gf_bs_write_int(bs, nbWords, 8);
			gf_bs_seek(bs, bs_cur);
			gf_bs_get_content(bs, out_data, out_data_size);
		}
		gf_bs_del(bs);
		return GF_TRUE;
	}
#endif
	return GF_FALSE;
}

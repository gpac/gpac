/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2000-2020
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

#ifndef GPAC_MINIMAL_ODF

void GF_IPMPX_AUTH_Delete(GF_IPMPX_Authentication *auth);


u8 gf_ipmpx_get_tag(char *dataName)
{
	if (!stricmp(dataName, "IPMP_KeyData")) return GF_IPMPX_KEY_DATA_TAG;
	else if (!stricmp(dataName, "IPMP_RightsData")) return GF_IPMPX_RIGHTS_DATA_TAG;
	else if (!stricmp(dataName, "IPMP_OpaqueData")) return GF_IPMPX_OPAQUE_DATA_TAG;
	else if (!stricmp(dataName, "IPMP_SecureContainer")) return GF_IPMPX_SECURE_CONTAINER_TAG;
	else if (!stricmp(dataName, "IPMP_InitAuthentication")) return GF_IPMPX_INIT_AUTHENTICATION_TAG;
	else if (!stricmp(dataName, "IPMP_TrustSecurityMetadata")) return GF_IPMPX_TRUST_SECURITY_METADATA_TAG;
	else if (!stricmp(dataName, "IPMP_TrustedTool")) return GF_IPMPX_TRUSTED_TOOL_TAG;
	else if (!stricmp(dataName, "IPMP_TrustSpecification")) return GF_IPMPX_TRUST_SPECIFICATION_TAG;
	else if (!stricmp(dataName, "IPMP_MutualAuthentication")) return GF_IPMPX_MUTUAL_AUTHENTICATION_TAG;
	else if (!stricmp(dataName, "IPMP_AlgorithmDescriptor")) return GF_IPMPX_ALGORITHM_DESCRIPTOR_TAG;
	else if (!stricmp(dataName, "IPMP_KeyDescriptor")) return GF_IPMPX_KEY_DESCRIPTOR_TAG;
	else if (!stricmp(dataName, "IPMP_GetToolsResponse")) return GF_IPMPX_GET_TOOLS_RESPONSE_TAG;
	else if (!stricmp(dataName, "IPMP_ParametricDescription")) return GF_IPMPX_PARAMETRIC_DESCRIPTION_TAG;
	else if (!stricmp(dataName, "IPMP_ParametricDescriptionItem")) return GF_IPMPX_PARAM_DESCRIPTOR_ITEM_TAG;
	else if (!stricmp(dataName, "IPMP_ToolParamCapabilitiesQuery")) return GF_IPMPX_PARAMETRIC_CAPS_QUERY_TAG;
	else if (!stricmp(dataName, "IPMP_ToolParamCapabilitiesResponse")) return GF_IPMPX_PARAMETRIC_CAPS_RESPONSE_TAG;
	else if (!stricmp(dataName, "IPMP_ConnectTool")) return GF_IPMPX_CONNECT_TOOL_TAG;
	else if (!stricmp(dataName, "IPMP_DisconnectTool")) return GF_IPMPX_DISCONNECT_TOOL_TAG;
	else if (!stricmp(dataName, "IPMP_GetToolContext")) return GF_IPMPX_GET_TOOL_CONTEXT_TAG;
	else if (!stricmp(dataName, "IPMP_GetToolContextResponse")) return GF_IPMPX_GET_TOOL_CONTEXT_RESPONSE_TAG;
	else if (!stricmp(dataName, "IPMP_AddToolNotificationListener")) return GF_IPMPX_ADD_TOOL_LISTENER_TAG;
	else if (!stricmp(dataName, "IPMP_RemoveToolNotificationListener")) return GF_IPMPX_REMOVE_TOOL_LISTENER_TAG;
	else if (!stricmp(dataName, "IPMP_NotifyToolEvent")) return GF_IPMPX_NOTIFY_TOOL_EVENT_TAG;
	else if (!stricmp(dataName, "IPMP_CanProcess")) return GF_IPMPX_CAN_PROCESS_TAG;
	else if (!stricmp(dataName, "IPMP_ToolAPI_Config")) return GF_IPMPX_TOOL_API_CONFIG_TAG;
	else if (!stricmp(dataName, "IPMP_AudioWatermarkingInit")) return GF_IPMPX_AUDIO_WM_INIT_TAG;
	else if (!stricmp(dataName, "IPMP_VideoWatermarkingInit")) return GF_IPMPX_VIDEO_WM_INIT_TAG;
	else if (!stricmp(dataName, "IPMP_SendAudioWatermark")) return GF_IPMPX_AUDIO_WM_SEND_TAG;
	else if (!stricmp(dataName, "IPMP_SendVideoWatermark")) return GF_IPMPX_VIDEO_WM_SEND_TAG;
	else if (!stricmp(dataName, "IPMP_SelectiveDecryptionInit")) return GF_IPMPX_SEL_DEC_INIT_TAG;
	else if (!stricmp(dataName, "IPMP_SelectiveBuffer")) return GF_IPMPX_SEL_ENC_BUFFER_TAG;
	else if (!stricmp(dataName, "IPMP_SelectiveField")) return GF_IPMPX_SEL_ENC_FIELD_TAG;
	else if (!stricmp(dataName, "ISMACryp_Data")) return GF_IPMPX_ISMACRYP_TAG;
	return 0;
}

u32 gf_ipmpx_get_field_type(GF_IPMPX_Data *p, char *fieldName)
{
	switch (p->tag) {
	case GF_IPMPX_KEY_DATA_TAG:
		if (!stricmp(fieldName, "keyBody")|| !stricmp(fieldName, "opaqueData")) return GF_ODF_FT_IPMPX_BA;
		break;
	case GF_IPMPX_RIGHTS_DATA_TAG:
		if (!stricmp(fieldName, "rightsInfo")) return GF_ODF_FT_IPMPX_BA;
		break;
	case GF_IPMPX_OPAQUE_DATA_TAG:
		if (!stricmp(fieldName, "OpaqueData")) return GF_ODF_FT_IPMPX_BA;
		break;
	case GF_IPMPX_SECURE_CONTAINER_TAG:
		if (!stricmp(fieldName, "encryptedData") || !stricmp(fieldName, "MAC")) return GF_ODF_FT_IPMPX_BA;
		else if (!stricmp(fieldName, "protectedMsg")) return GF_ODF_FT_IPMPX;
		break;
	case GF_IPMPX_TRUST_SECURITY_METADATA_TAG:
		if (!stricmp(fieldName, "trustedTools")) return GF_ODF_FT_IPMPX_LIST;
		break;
	case GF_IPMPX_TRUSTED_TOOL_TAG:
		if (!stricmp(fieldName, "trustSpecifications")) return GF_ODF_FT_IPMPX_LIST;
		break;
	case GF_IPMPX_TRUST_SPECIFICATION_TAG:
		if (!stricmp(fieldName, "CCTrustMetadata")) return GF_ODF_FT_IPMPX_BA;
		break;
	case GF_IPMPX_MUTUAL_AUTHENTICATION_TAG:
		if (!stricmp(fieldName, "candidateAlgorithms") || !stricmp(fieldName, "agreedAlgorithms")) return GF_ODF_FT_IPMPX_LIST;
		else if (!stricmp(fieldName, "certificates")) return GF_ODF_FT_IPMPX_BA_LIST;
		else if (!stricmp(fieldName, "publicKey") || !stricmp(fieldName, "trustData")) return GF_ODF_FT_IPMPX;
		else if (!stricmp(fieldName, "authCodes") || !stricmp(fieldName, "opaque") || !stricmp(fieldName, "AuthenticationData"))
			return GF_ODF_FT_IPMPX_BA;
		break;
	case GF_IPMPX_ALGORITHM_DESCRIPTOR_TAG:
		if (!stricmp(fieldName, "specAlgoID") || !stricmp(fieldName, "OpaqueData")) return GF_ODF_FT_IPMPX_BA;
		break;
	case GF_IPMPX_GET_TOOLS_RESPONSE_TAG:
		if (!stricmp(fieldName, "ipmp_tools")) return GF_ODF_FT_OD_LIST;
		break;
	case GF_IPMPX_PARAMETRIC_DESCRIPTION_TAG:
		if (!stricmp(fieldName, "descriptionComment")) return GF_ODF_FT_IPMPX_BA;
		else if (!stricmp(fieldName, "descriptions")) return GF_ODF_FT_IPMPX_LIST;
		break;
	case GF_IPMPX_PARAM_DESCRIPTOR_ITEM_TAG:
		/*all is IPMPX data*/
		return GF_ODF_FT_IPMPX_BA;
	case GF_IPMPX_PARAMETRIC_CAPS_QUERY_TAG:
		if (!stricmp(fieldName, "description")) return GF_ODF_FT_IPMPX;
		break;
	case GF_IPMPX_CONNECT_TOOL_TAG:
		if (!stricmp(fieldName, "toolDescriptor")) return GF_ODF_FT_OD;
		break;
	case GF_IPMPX_TOOL_API_CONFIG_TAG:
		if (!stricmp(fieldName, "opaqueData")) return GF_ODF_FT_IPMPX_BA;
		break;
	case GF_IPMPX_AUDIO_WM_SEND_TAG:
	case GF_IPMPX_VIDEO_WM_SEND_TAG:
		if (!stricmp(fieldName, "payload") || !stricmp(fieldName, "opaqueData")) return GF_ODF_FT_IPMPX_BA;
		break;
	case GF_IPMPX_SEL_DEC_INIT_TAG:
		if (!stricmp(fieldName, "SelectiveBuffers") || !stricmp(fieldName, "SelectiveFields")) return GF_ODF_FT_IPMPX_LIST;
		break;
	case GF_IPMPX_SEL_ENC_BUFFER_TAG:
		if (!stricmp(fieldName, "StreamCipher")) return GF_ODF_FT_IPMPX_BA;
		break;
	case GF_IPMPX_SEL_ENC_FIELD_TAG:
		if (!stricmp(fieldName, "shuffleSpecificInfo")) return GF_ODF_FT_IPMPX_BA;
		break;
	default:
		break;
	}
	return 0;
}

#include <gpac/internal/odf_parse_common.h>

void GF_IPMPX_ParseBinData(char *val, u8 **out_data, u32 *out_data_size)
{
	u32 i, c, len;
	char s[3];

	if (val[0] != '%') {
		len = *out_data_size = (u32) strlen(val);
		*out_data = (char*)gf_malloc(sizeof(char) * len);
		memcpy(*out_data, val, sizeof(char) * len);
		return;
	}

	len = (u32) strlen(val) / 3;
	if (*out_data) gf_free(*out_data);
	*out_data_size = len;
	*out_data = (char*)gf_malloc(sizeof(char) * len);
	s[2] = 0;
	for (i=0; i<len; i++) {
		s[0] = val[3*i+1];
		s[1] = val[3*i+2];
		sscanf(s, "%02X", &c);
		(*out_data)[i] = (unsigned char) c;
	}
}


void GF_IPMPX_ParseBin128(char *val, bin128 *data)
{
	if (!strnicmp(val, "0x", 2)) val+=2;

	if (strlen(val)<16) {
		GF_BitStream *bs;
		u32 int_val = atoi(val);
		bs = gf_bs_new((char*) (*data), 16, GF_BITSTREAM_WRITE);
		gf_bs_write_int(bs, 0, 32);
		gf_bs_write_int(bs, 0, 32);
		gf_bs_write_int(bs, 0, 32);
		gf_bs_write_int(bs, int_val, 32);
		gf_bs_del(bs);
	} else {
		u32 i, b;
		char szB[3];
		szB[2] = 0;
		for (i=0; i<16; i++) {
			szB[0] = val[2*i];
			szB[1] = val[2*i+1];
			sscanf(szB, "%x", &b);
			((char *)data)[i] = (u8) b;
		}
	}
}

void GF_IPMPX_ParseDate(char *val, GF_IPMPX_Date *date)
{
	if ((strlen(val)<7) || strnicmp(val, "0x", 2)) {
		GF_BitStream *bs;
		u32 int_val = atoi(val);
		bs = gf_bs_new((*date), 5, GF_BITSTREAM_WRITE);
		gf_bs_write_int(bs, 0, 8);
		gf_bs_write_int(bs, int_val, 32);
		gf_bs_del(bs);
	} else {
		val += 2;
		memcpy((char *) (*date), val, sizeof(char)*5);
	}
}

GF_Err GF_IPMPX_ParseEventType(char *val, GF_IPMPX_ListenType *eventType, u8 *eventTypeCount)
{
	char szVal[50];
	u32 i, j, len, v;
	*eventTypeCount = 0;
	len = (u32) strlen(val);
	j=0;
	for (i=0; i<len; i++) {
		switch (val[i]) {
		case '\'':
		case '\"':
		case ',':
		case ' ':
			break;
		default:
			szVal[j] = val[i];
			j++;
			if (i+1==len) break;
			continue;
		}
		if (j) {
			szVal[j] = 0;
			if (!strnicmp(szVal, "0x", 2)) {
				sscanf(szVal, "%x", &v);
				eventType[*eventTypeCount] = v;
			}
			else {
				sscanf(szVal, "%u", &v);
				eventType[*eventTypeCount] = v;
			}
			j=0;
			(*eventTypeCount) += 1;
			if (*eventTypeCount == 9) return GF_OK;
		}
	}
	return GF_OK;
}

GF_Err gf_ipmpx_data_parse_16(char *val, u16 **outData, u16 *outDataSize)
{
	char szVal[50];
	u32 i, j, len, v, alloc, count;
	u16 *data = (u16*)gf_malloc(sizeof(u16) * 100);
	alloc = 100;

	len = (u32) strlen(val);
	count = j = 0;
	for (i=0; i<len; i++) {
		switch (val[i]) {
		case '\'':
		case '\"':
		case ',':
		case ' ':
			break;
		default:
			szVal[j] = val[i];
			j++;
			if (i+1==len) break;
			continue;
		}
		if (j) {
			szVal[j] = 0;
			if (!strnicmp(szVal, "0x", 2)) {
				sscanf(szVal, "%x", &v);
				data[count] = v;
			}
			else {
				sscanf(szVal, "%u", &v);
				data[count] = v;
			}
			j=0;
			count += 1;
			if (count == alloc) {
				alloc += 100;
				data = (u16*)gf_realloc(data, sizeof(u16)*alloc);
			}
		}
	}
	(*outData) = (u16*)gf_realloc(data, sizeof(u16)*count);
	*outDataSize = count;
	return GF_OK;
}

GF_Err gf_ipmpx_set_field(GF_IPMPX_Data *_p, char *fieldName, char *val)
{
	u32 ret = 0;

	if (!stricmp(val, "auto")) return GF_OK;
	else if (!stricmp(val, "unspecified")) return GF_OK;

	switch (_p->tag) {
	case GF_IPMPX_KEY_DATA_TAG:
	{
		GF_IPMPX_KeyData *p = (GF_IPMPX_KeyData*)_p;
		if (!stricmp(fieldName, "hasStartDTS")) {
			if (!stricmp(val, "false") || !stricmp(val, "0") ) p->flags &= ~1;
			else p->flags |= 1;
			ret = 1;
		}
		else if (!stricmp(fieldName, "hasStartPacketID")) {
			if (!stricmp(val, "false") || !stricmp(val, "0") ) p->flags &= ~2;
			else p->flags |= 2;
			ret = 1;
		}
		else if (!stricmp(fieldName, "hasExpireDTS")) {
			if (!stricmp(val, "false") || !stricmp(val, "0") ) p->flags &= ~4;
			else p->flags |= 4;
			ret = 1;
		}
		else if (!stricmp(fieldName, "hasExpirePacketID")) {
			if (!stricmp(val, "false") || !stricmp(val, "0") ) p->flags &= ~8;
			else p->flags |= 8;
			ret = 1;
		}
		else if (!stricmp(fieldName, "startPacketID")) {
			GET_U32(p->startPacketID)
		}
		else if (!stricmp(fieldName, "expirePacketID")) {
			GET_U32(p->expirePacketID)
		}
		else if (!stricmp(fieldName, "startDTS")) {
			GET_U64(p->startDTS)
		}
		else if (!stricmp(fieldName, "expireDTS")) {
			GET_U64(p->expireDTS)
		}

	}
		break;
	case GF_IPMPX_SECURE_CONTAINER_TAG:
	{
		GF_IPMPX_SecureContainer*p = (GF_IPMPX_SecureContainer*)_p;
		if (!stricmp(fieldName, "isMACEncrypted")) GET_BOOL(p->isMACEncrypted)
	}
		break;
	case GF_IPMPX_INIT_AUTHENTICATION_TAG:
	{
		GF_IPMPX_InitAuthentication *p = (GF_IPMPX_InitAuthentication*)_p;
		if (!stricmp(fieldName, "Context")) GET_U32(p->Context)
		else if (!stricmp(fieldName, "AuthType")) GET_U8(p->AuthType)
	}
		break;
	case GF_IPMPX_TRUSTED_TOOL_TAG:
	{
		GF_IPMPX_TrustedTool *p = (GF_IPMPX_TrustedTool*)_p;
		if (!stricmp(fieldName, "toolID")) {
			GF_IPMPX_ParseBin128(val, &p->toolID);
			ret = 1;
		}
		else if (!stricmp(fieldName, "AuditDate")) {
			GF_IPMPX_ParseDate(val, &p->AuditDate);
			ret = 1;
		}
	}
	break;
	case GF_IPMPX_TRUST_SPECIFICATION_TAG:
	{
		GF_IPMPX_TrustSpecification *p = (GF_IPMPX_TrustSpecification*)_p;
		if (!stricmp(fieldName, "startDate")) {
			GF_IPMPX_ParseDate(val, &p->startDate);
			ret = 1;
		}
		else if (!stricmp(fieldName, "attackerProfile")) GET_U8(p->attackerProfile)
			else if (!stricmp(fieldName, "trustedDuration")) GET_U32(p->trustedDuration)
			}
	break;
	case GF_IPMPX_ALGORITHM_DESCRIPTOR_TAG:
	{
		GF_IPMPX_AUTH_AlgorithmDescriptor *p = (GF_IPMPX_AUTH_AlgorithmDescriptor *)_p;
		if (!stricmp(fieldName, "regAlgoID")) GET_U16(p->regAlgoID)
		}
	break;
	case GF_IPMPX_KEY_DESCRIPTOR_TAG:
	{
		GF_IPMPX_AUTH_KeyDescriptor *p = (GF_IPMPX_AUTH_KeyDescriptor *)_p;
		if (!stricmp(fieldName, "keyBody")) {
			u32 s;
			GF_IPMPX_ParseBinData(val, &p->keyBody, &s);
			p->keyBodyLength = s;
			ret = 1;
		}
	}
	break;
	case GF_IPMPX_PARAMETRIC_DESCRIPTION_TAG:
	{
		GF_IPMPX_ParametricDescription*p = (GF_IPMPX_ParametricDescription*)_p;
		if (!stricmp(fieldName, "majorVersion")) GET_U8(p->majorVersion)
			else if (!stricmp(fieldName, "minorVersion")) GET_U8(p->minorVersion)
			}
	break;
	case GF_IPMPX_PARAMETRIC_CAPS_RESPONSE_TAG:
	{
		GF_IPMPX_ToolParamCapabilitiesResponse*p = (GF_IPMPX_ToolParamCapabilitiesResponse*)_p;
		if (!stricmp(fieldName, "capabilitiesSupported")) GET_BOOL(p->capabilitiesSupported)
		}
	break;
	case GF_IPMPX_DISCONNECT_TOOL_TAG:
	{
		GF_IPMPX_DisconnectTool*p = (GF_IPMPX_DisconnectTool*)_p;
		if (!stricmp(fieldName, "IPMP_ToolContextID")) GET_U16(p->IPMP_ToolContextID)
		}
	break;
	case GF_IPMPX_GET_TOOL_CONTEXT_TAG:
	{
		GF_IPMPX_GetToolContext*p = (GF_IPMPX_GetToolContext*)_p;
		if (!stricmp(fieldName, "scope")) GET_U8(p->scope)
			else if (!stricmp(fieldName, "IPMP_DescriptorIDEx")) GET_U16(p->IPMP_DescriptorIDEx)
			}
	break;
	case GF_IPMPX_GET_TOOL_CONTEXT_RESPONSE_TAG:
	{
		GF_IPMPX_GetToolContextResponse*p = (GF_IPMPX_GetToolContextResponse*)_p;
		if (!stricmp(fieldName, "OD_ID")) GET_U16(p->OD_ID)
			else if (!stricmp(fieldName, "ESD_ID")) GET_U16(p->ESD_ID)
				else if (!stricmp(fieldName, "IPMP_ToolContextID")) GET_U16(p->IPMP_ToolContextID)
				}
	break;
	case GF_IPMPX_ADD_TOOL_LISTENER_TAG:
	{
		GF_IPMPX_AddToolNotificationListener*p = (GF_IPMPX_AddToolNotificationListener*)_p;
		if (!stricmp(fieldName, "eventType")) return GF_IPMPX_ParseEventType(val, p->eventType, &p->eventTypeCount);
		else if (!stricmp(fieldName, "scope")) GET_U8(p->scope)
		}
	break;
	case GF_IPMPX_REMOVE_TOOL_LISTENER_TAG:
	{
		GF_IPMPX_RemoveToolNotificationListener*p = (GF_IPMPX_RemoveToolNotificationListener*)_p;
		if (!stricmp(fieldName, "eventType")) return GF_IPMPX_ParseEventType(val, p->eventType, &p->eventTypeCount);
	}
	break;
	case GF_IPMPX_NOTIFY_TOOL_EVENT_TAG:
	{
		GF_IPMPX_NotifyToolEvent*p = (GF_IPMPX_NotifyToolEvent*)_p;
		if (!stricmp(fieldName, "OD_ID")) GET_U16(p->OD_ID)
			else if (!stricmp(fieldName, "ESD_ID")) GET_U16(p->ESD_ID)
				else if (!stricmp(fieldName, "IPMP_ToolContextID")) GET_U16(p->IPMP_ToolContextID)
					else if (!stricmp(fieldName, "eventType")) GET_U8(p->eventType)
					}
	break;
	case GF_IPMPX_CAN_PROCESS_TAG:
	{
		GF_IPMPX_CanProcess*p = (GF_IPMPX_CanProcess*)_p;
		if (!stricmp(fieldName, "canProcess")) GET_BOOL(p->canProcess)
		}
	break;
	case GF_IPMPX_TOOL_API_CONFIG_TAG:
	{
		GF_IPMPX_ToolAPI_Config*p = (GF_IPMPX_ToolAPI_Config*)_p;
		if (!stricmp(fieldName, "Instantiation_API_ID")) GET_U32(p->Instantiation_API_ID)
			else if (!stricmp(fieldName, "Messaging_API_ID")) GET_U32(p->Messaging_API_ID)
			}
	break;
	case GF_IPMPX_AUDIO_WM_INIT_TAG:
	case GF_IPMPX_VIDEO_WM_INIT_TAG:
	{
		GF_IPMPX_WatermarkingInit *p = (GF_IPMPX_WatermarkingInit*)_p;
		if (!stricmp(fieldName, "inputFormat")) GET_U8(p->inputFormat)
			else if (!stricmp(fieldName, "requiredOp")) GET_U8(p->requiredOp)
				else if (!stricmp(fieldName, "nChannels")) GET_U8(p->nChannels)
					else if (!stricmp(fieldName, "bitPerSample")) GET_U8(p->bitPerSample)
						else if (!stricmp(fieldName, "frequency")) GET_U32(p->frequency)
							else if (!stricmp(fieldName, "frame_horizontal_size")) GET_U16(p->frame_horizontal_size)
								else if (!stricmp(fieldName, "frame_vertical_size")) GET_U16(p->frame_vertical_size)
									else if (!stricmp(fieldName, "chroma_format")) GET_U8(p->chroma_format)
										else if (!stricmp(fieldName, "wmPayload")) {
											ret=1;
											GF_IPMPX_ParseBinData(val, &p->wmPayload, &p->wmPayloadLen);
										}
										else if (!stricmp(fieldName, "opaqueData")) {
											ret=1;
											GF_IPMPX_ParseBinData(val, &p->opaqueData, &p->opaqueDataSize);
										}
										else if (!stricmp(fieldName, "wmRecipientId")) GET_U16(p->wmRecipientId)
										}
	break;
	case GF_IPMPX_AUDIO_WM_SEND_TAG:
	case GF_IPMPX_VIDEO_WM_SEND_TAG:
	{
		GF_IPMPX_SendWatermark *p = (GF_IPMPX_SendWatermark*)_p;
		if (!stricmp(fieldName, "wm_status")) GET_U8(p->wm_status)
			else if (!stricmp(fieldName, "compression_status")) GET_U8(p->compression_status)
			}
	break;
	case GF_IPMPX_SEL_DEC_INIT_TAG:
	{
		GF_IPMPX_SelectiveDecryptionInit*p = (GF_IPMPX_SelectiveDecryptionInit*)_p;
		if (!stricmp(fieldName, "mediaTypeExtension")) GET_U8(p->mediaTypeExtension)
			else if (!stricmp(fieldName, "mediaTypeExtension")) GET_U8(p->mediaTypeExtension)
				else if (!stricmp(fieldName, "mediaTypeIndication")) GET_U8(p->mediaTypeIndication)
					else if (!stricmp(fieldName, "profileLevelIndication")) GET_U8(p->profileLevelIndication)
						else if (!stricmp(fieldName, "compliance")) GET_U8(p->compliance)
							else if (!stricmp(fieldName, "RLE_Data")) {
								ret=1;
								gf_ipmpx_data_parse_16(val, &p->RLE_Data, &p->RLE_DataLength);
							}
	}
	break;
	case GF_IPMPX_SEL_ENC_BUFFER_TAG:
	{
		GF_IPMPX_SelEncBuffer*p = (GF_IPMPX_SelEncBuffer*)_p;
		if (!stricmp(fieldName, "cipher_Id")) {
			GF_IPMPX_ParseBin128(val, &p->cipher_Id);
			ret = 1;
		}
		else if (!stricmp(fieldName, "syncBoundary")) GET_U8(p->syncBoundary)
			else if (!stricmp(fieldName, "mode")) GET_U8(p->mode)
				else if (!stricmp(fieldName, "blockSize")) GET_U16(p->blockSize)
					else if (!stricmp(fieldName, "keySize")) GET_U16(p->keySize)
					}
	break;
	case GF_IPMPX_SEL_ENC_FIELD_TAG:
	{
		GF_IPMPX_SelEncField*p = (GF_IPMPX_SelEncField*)_p;
		if (!stricmp(fieldName, "field_Id")) GET_U8(p->field_Id)
			else if (!stricmp(fieldName, "field_Scope")) GET_U8(p->field_Scope)
				else if (!stricmp(fieldName, "buf")) GET_U8(p->buf)
					else if (!stricmp(fieldName, "mappingTable")) {
						ret=1;
						gf_ipmpx_data_parse_16(val, &p->mappingTable, &p->mappingTableSize);
					}
	}
	break;
	case GF_IPMPX_ISMACRYP_TAG:
	{
		GF_IPMPX_ISMACryp*p = (GF_IPMPX_ISMACryp*)_p;
		if (!stricmp(fieldName, "crypto_suite")) GET_U8(p->cryptoSuite)
			else if (!stricmp(fieldName, "IV_length")) GET_U8(p->IV_length)
				else if (!stricmp(fieldName, "selective_encryption")) GET_BOOL(p->use_selective_encryption)
					else if (!stricmp(fieldName, "key_indicator_length")) GET_U8(p->key_indicator_length)
					}
	break;
	}
	return ret ? GF_OK : GF_BAD_PARAM;
}

GF_Err gf_ipmpx_set_sub_data(GF_IPMPX_Data *_p, char *fieldName, GF_IPMPX_Data *sp)
{
	switch (_p->tag) {
	case GF_IPMPX_SECURE_CONTAINER_TAG:
	{
		GF_IPMPX_SecureContainer*p = (GF_IPMPX_SecureContainer*)_p;
		if (p->protectedMsg) gf_ipmpx_data_del(p->protectedMsg);
		p->protectedMsg = sp;
		return GF_OK;
	}
	break;
	case GF_IPMPX_TRUST_SECURITY_METADATA_TAG:
	{
		GF_IPMPX_TrustSecurityMetadata *p = (GF_IPMPX_TrustSecurityMetadata*)_p;
		if (!sp || (sp->tag!=GF_IPMPX_TRUSTED_TOOL_TAG) ) return GF_BAD_PARAM;
		gf_list_add(p->TrustedTools, sp);
		return GF_OK;
	}
	break;
	case GF_IPMPX_TRUSTED_TOOL_TAG:
	{
		GF_IPMPX_TrustedTool *p = (GF_IPMPX_TrustedTool *)_p;
		if (!sp || (sp->tag!=GF_IPMPX_TRUST_SPECIFICATION_TAG) ) return GF_BAD_PARAM;
		gf_list_add(p->trustSpecifications, sp);
		return GF_OK;
	}
	break;
	case GF_IPMPX_MUTUAL_AUTHENTICATION_TAG:
	{
		GF_IPMPX_MutualAuthentication *p = (GF_IPMPX_MutualAuthentication *)_p;
		if (!sp) return GF_BAD_PARAM;
		switch (sp->tag) {
		case GF_IPMPX_ALGORITHM_DESCRIPTOR_TAG:
			sp->tag = GF_IPMPX_AUTH_AlgorithmDescr_Tag;
			if (!stricmp(fieldName, "candidateAlgorithms")) return gf_list_add(p->agreedAlgorithms, sp);
			else if (!stricmp(fieldName, "agreedAlgorithms")) return gf_list_add(p->agreedAlgorithms, sp);
			else return GF_BAD_PARAM;
		case GF_IPMPX_KEY_DESCRIPTOR_TAG:
			sp->tag = GF_IPMPX_AUTH_KeyDescr_Tag;
			if (!stricmp(fieldName, "candidateAlgorithms")) return gf_list_add(p->agreedAlgorithms, sp);
			else if (!stricmp(fieldName, "agreedAlgorithms")) return gf_list_add(p->agreedAlgorithms, sp);
			else if (!stricmp(fieldName, "publicKey")) {
				if (p->publicKey) GF_IPMPX_AUTH_Delete((GF_IPMPX_Authentication *) p->publicKey);
				p->publicKey = (GF_IPMPX_AUTH_KeyDescriptor *) sp;
				return GF_OK;
			}
			else return GF_BAD_PARAM;
		case GF_IPMPX_TRUST_SECURITY_METADATA_TAG:
			if (p->trustData) gf_ipmpx_data_del((GF_IPMPX_Data *)p->trustData);
			p->trustData = (GF_IPMPX_TrustSecurityMetadata*)sp;
			return GF_OK;
		default:
			return GF_BAD_PARAM;
		}
	}
	break;
	case GF_IPMPX_PARAMETRIC_DESCRIPTION_TAG:
	{
		GF_IPMPX_ParametricDescription*p = (GF_IPMPX_ParametricDescription*)_p;
		if (!sp || (sp->tag!=GF_IPMPX_PARAM_DESCRIPTOR_ITEM_TAG) ) return GF_BAD_PARAM;
		if (!stricmp(fieldName, "descriptions")) return gf_list_add(p->descriptions, sp);
		else return GF_BAD_PARAM;
	}
	break;
	case GF_IPMPX_PARAMETRIC_CAPS_QUERY_TAG:
	{
		GF_IPMPX_ToolParamCapabilitiesQuery*p = (GF_IPMPX_ToolParamCapabilitiesQuery*)_p;
		if (!sp || (sp->tag!=GF_IPMPX_PARAMETRIC_DESCRIPTION_TAG) ) return GF_BAD_PARAM;
		if (stricmp(fieldName, "description")) return GF_BAD_PARAM;
		if (p->description) gf_ipmpx_data_del((GF_IPMPX_Data *)p->description);
		p->description = (GF_IPMPX_ParametricDescription*)sp;
		return GF_OK;
	}
	break;
	case GF_IPMPX_SEL_DEC_INIT_TAG:
	{
		GF_IPMPX_SelectiveDecryptionInit*p = (GF_IPMPX_SelectiveDecryptionInit*)_p;
		if (!sp) return GF_BAD_PARAM;
		if (sp->tag==GF_IPMPX_SEL_ENC_BUFFER_TAG) return gf_list_add(p->SelEncBuffer, sp);
		else if (sp->tag==GF_IPMPX_SEL_ENC_FIELD_TAG) return gf_list_add(p->SelEncFields, sp);
		else return GF_BAD_PARAM;
	}
	break;
	}
	return GF_BAD_PARAM;
}

GF_Err gf_ipmpx_set_byte_array(GF_IPMPX_Data *p, char *field, char *str)
{
	GF_IPMPX_ByteArray *d;
	GF_IPMPX_ByteArray **dest;
	d = (GF_IPMPX_ByteArray*)gf_malloc(sizeof(GF_IPMPX_ByteArray));
	d->length = (u32) strlen(str);
	d->data = (char*)gf_malloc(sizeof(char)*d->length);
	memcpy(d->data, str, d->length);

	dest = NULL;
	switch (p->tag) {
	case GF_IPMPX_KEY_DATA_TAG:
		if (!stricmp(field, "keyBody")) dest = & ((GF_IPMPX_KeyData*)p)->keyBody;
		else if (!stricmp(field, "opaqueData")) dest = & ((GF_IPMPX_KeyData*)p)->OpaqueData;
		break;
	case GF_IPMPX_RIGHTS_DATA_TAG:
		if (!stricmp(field, "rightsInfo")) dest = & ((GF_IPMPX_RightsData*)p)->rightsInfo;
		break;
	case GF_IPMPX_OPAQUE_DATA_TAG:
		if (!stricmp(field, "opaqueData")) dest = & ((GF_IPMPX_OpaqueData*)p)->opaqueData;
		break;
	case GF_IPMPX_SECURE_CONTAINER_TAG:
		if (!stricmp(field, "encryptedData")) dest = & ((GF_IPMPX_SecureContainer*)p)->encryptedData;
		else if (!stricmp(field, "MAC")) dest = & ((GF_IPMPX_SecureContainer*)p)->MAC;
		break;
	case GF_IPMPX_TRUST_SPECIFICATION_TAG:
		if (!stricmp(field, "CCTrustMetadata")) dest = & ((GF_IPMPX_TrustSpecification*)p)->CCTrustMetadata;
		break;
	case GF_IPMPX_MUTUAL_AUTHENTICATION_TAG:
		if (!stricmp(field, "AuthenticationData")) dest = & ((GF_IPMPX_MutualAuthentication*)p)->AuthenticationData;
		else if (!stricmp(field, "opaque")) dest = & ((GF_IPMPX_MutualAuthentication*)p)->opaque;
		else if (!stricmp(field, "authCodes")) dest = & ((GF_IPMPX_MutualAuthentication*)p)->authCodes;
		else if (!stricmp(field, "certificates")) {
			gf_list_add(((GF_IPMPX_MutualAuthentication*)p)->certificates, d);
			return GF_OK;
		}
		break;
	case GF_IPMPX_PARAMETRIC_DESCRIPTION_TAG:
		if (!stricmp(field, "descriptionComment")) dest = & ((GF_IPMPX_ParametricDescription*)p)->descriptionComment;
		break;
	case GF_IPMPX_PARAM_DESCRIPTOR_ITEM_TAG:
		if (!stricmp(field, "class")) dest = & ((GF_IPMPX_ParametricDescriptionItem*)p)->main_class;
		else if (!stricmp(field, "subClass")) dest = & ((GF_IPMPX_ParametricDescriptionItem*)p)->subClass;
		else if (!stricmp(field, "typeData")) dest = & ((GF_IPMPX_ParametricDescriptionItem*)p)->typeData;
		else if (!stricmp(field, "type")) dest = & ((GF_IPMPX_ParametricDescriptionItem*)p)->type;
		else if (!stricmp(field, "addedData")) dest = & ((GF_IPMPX_ParametricDescriptionItem*)p)->addedData;
		break;
	case GF_IPMPX_TOOL_API_CONFIG_TAG:
		if (!stricmp(field, "opaqueData")) dest = & ((GF_IPMPX_ToolAPI_Config*)p)->opaqueData;
		break;
	case GF_IPMPX_AUDIO_WM_SEND_TAG:
	case GF_IPMPX_VIDEO_WM_SEND_TAG:
		if (!stricmp(field, "payload")) dest = & ((GF_IPMPX_SendWatermark *)p)->payload;
		else if (!stricmp(field, "opaqueData")) dest = & ((GF_IPMPX_SendWatermark*)p)->opaqueData;
		break;
	case GF_IPMPX_SEL_ENC_BUFFER_TAG:
		if (!stricmp(field, "StreamCipher")) dest = & ((GF_IPMPX_SelEncBuffer*)p)->Stream_Cipher_Specific_Init_Info;
		break;
	case GF_IPMPX_SEL_ENC_FIELD_TAG:
		if (!stricmp(field, "shuffleSpecificInfo")) dest = & ((GF_IPMPX_SelEncField*)p)->shuffleSpecificInfo;
		break;
	}
	if (!dest) {
		gf_free(d->data);
		gf_free(d);
		return GF_BAD_PARAM;
	}
	if ( (*dest) ) {
		if ((*dest)->data) gf_free((*dest)->data);
		gf_free((*dest));
	}
	(*dest) = d;
	return GF_OK;
}

#endif /*GPAC_MINIMAL_ODF*/


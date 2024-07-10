/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2000-2023
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

#if !defined(GPAC_DISABLE_OD_DUMP) && !defined(GPAC_MINIMAL_ODF)

#define GF_IPMPX_MAX_TREE		100

#define GF_IPMPX_FORMAT_INDENT( ind_buf, indent ) \
	{ \
		u32 z;	\
		gf_assert(GF_IPMPX_MAX_TREE>indent);	\
		for (z=0; z<indent; z++) ind_buf[z] = ' '; \
		ind_buf[z] = 0; \
	} \
 

static void StartList(FILE *trace, const char *name, u32 indent, Bool XMTDump)
{
	char ind_buf[GF_IPMPX_MAX_TREE];
	GF_IPMPX_FORMAT_INDENT(ind_buf, indent);
	if (XMTDump) {
		gf_fprintf(trace, "%s<%s>\n", ind_buf, name);
	} else {
		gf_fprintf(trace, "%s%s [\n", ind_buf, name);
	}
}
static void EndList(FILE *trace, const char *name, u32 indent, Bool XMTDump)
{
	char ind_buf[GF_IPMPX_MAX_TREE];
	GF_IPMPX_FORMAT_INDENT(ind_buf, indent);
	if (XMTDump) {
		gf_fprintf(trace, "%s</%s>\n", ind_buf, name);
	} else {
		gf_fprintf(trace, "%s]\n", ind_buf);
	}
}

static void StartElement(FILE *trace, const char *descName, u32 indent, Bool XMTDump)
{
	char ind_buf[GF_IPMPX_MAX_TREE];
	GF_IPMPX_FORMAT_INDENT(ind_buf, indent);

	gf_fprintf(trace, "%s", ind_buf);
	if (!XMTDump) {
		gf_fprintf(trace, "%s {\n", descName);
	} else {
		gf_fprintf(trace, "<%s ", descName);
	}
}

static void EndAttributes(FILE *trace, Bool XMTDump, Bool has_children)
{
	if (XMTDump) {
		if (has_children) {
			gf_fprintf(trace, ">\n");
		} else {
			gf_fprintf(trace, "/>\n");
		}
	}
}

static void EndElement(FILE *trace, const char *descName, u32 indent, Bool XMTDump)
{
	char ind_buf[GF_IPMPX_MAX_TREE];
	GF_IPMPX_FORMAT_INDENT(ind_buf, indent);

	gf_fprintf(trace, "%s", ind_buf);
	if (!XMTDump) {
		gf_fprintf(trace, "}\n");
	} else {
		gf_fprintf(trace, "</%s>\n", descName);
	}
}

static void StartAttribute(FILE *trace, const char *attName, u32 indent, Bool XMTDump)
{
	char ind_buf[GF_IPMPX_MAX_TREE];
	GF_IPMPX_FORMAT_INDENT(ind_buf, indent);
	if (!XMTDump) {
		gf_fprintf(trace, "%s%s ", ind_buf, attName);
	} else {
		gf_fprintf(trace, "%s=\"", attName);
	}
}
static void EndAttribute(FILE *trace, u32 indent, Bool XMTDump)
{
	if (!XMTDump) {
		gf_fprintf(trace, "\n");
	} else {
		gf_fprintf(trace, "\" ");
	}
}

static void DumpInt(FILE *trace, char *attName, u32  val, u32 indent, Bool XMTDump)
{
	if (!val) return;
	StartAttribute(trace, attName, indent, XMTDump);
	gf_fprintf(trace, "%d", val);
	EndAttribute(trace, indent, XMTDump);
}

static void DumpLargeInt(FILE *trace, char *attName, u64  val, u32 indent, Bool XMTDump)
{
	if (!val) return;
	StartAttribute(trace, attName, indent, XMTDump);
	gf_fprintf(trace, LLU, val);
	EndAttribute(trace, indent, XMTDump);
}

static void DumpBool(FILE *trace, char *attName, u32  val, u32 indent, Bool XMTDump)
{
	if (!val) return;

	StartAttribute(trace, attName, indent, XMTDump);
	gf_fprintf(trace, "%s", "true");
	EndAttribute(trace, indent, XMTDump);
}

static void DumpData(FILE *trace, const char *name, char *data, u32 dataLength, u32 indent, Bool XMTDump)
{
	u32 i;
	Bool ASCII_Dump;
	if (!name && !data) return;

	if (name) StartAttribute(trace, name, indent, XMTDump);
	if (!XMTDump) gf_fprintf(trace, "\"");

	ASCII_Dump = GF_TRUE;
	for (i=0; i<dataLength; i++) {
		if ((data[i]<32) || (data[i]>126)) {
			ASCII_Dump = GF_FALSE;
			break;
		}
	}
	if (!ASCII_Dump && XMTDump) gf_fprintf(trace, "data:application/octet-string,");
	for (i=0; i<dataLength; i++) {
		if (ASCII_Dump) {
			gf_fprintf(trace, "%c", (unsigned char) data[i]);
		} else {
			gf_fprintf(trace, "%%");
			gf_fprintf(trace, "%02X", (unsigned char) data[i]);
		}
	}
	if (!XMTDump) gf_fprintf(trace, "\"");
	if (name) EndAttribute(trace, indent, XMTDump);
}
static void DumpData_16(FILE *trace, char *name, u16 *data, u16 dataLength, u32 indent, Bool XMTDump)
{
	u32 i;
	if (!name && !data) return;
	if (name) StartAttribute(trace, name, indent, XMTDump);
	if (!XMTDump) gf_fprintf(trace, "\"");
	for (i=0; i<dataLength; i++) {
		if (XMTDump) {
			gf_fprintf(trace, "\'%d\'", data[i]);
			if (i+1<dataLength) gf_fprintf(trace, " ");
		} else {
			gf_fprintf(trace, "%d", data[i]);
			if (i+1<dataLength) gf_fprintf(trace, ", ");
		}
	}
	if (!XMTDump) gf_fprintf(trace, "\"");
	if (name) EndAttribute(trace, indent, XMTDump);
}
static void DumpBin128(FILE *trace, char *name, char *data, u32 indent, Bool XMTDump)
{
	u32 i;
	if (!name ||!data) return;
	StartAttribute(trace, name, indent, XMTDump);
	gf_fprintf(trace, "0x");
	i=0;
	while (!data[i] && (i<16)) i++;
	if (i==16) {
		gf_fprintf(trace, "00");
	} else {
		for (; i<16; i++) gf_fprintf(trace, "%02X", (unsigned char) data[i]);
	}
	EndAttribute(trace, indent, XMTDump);
}


static void DumpDate(FILE *trace, char *name, char *data, u32 indent, Bool XMTDump)
{
	u32 i;
	if (!name ||!data) return;
	StartAttribute(trace, name, indent, XMTDump);
	gf_fprintf(trace, "0x");
	for (i=0; i<5; i++) gf_fprintf(trace, "%02X", (unsigned char) data[i]);
	EndAttribute(trace, indent, XMTDump);
}

void gf_ipmpx_dump_ByteArray(GF_IPMPX_ByteArray *_p, const char *attName, FILE *trace, u32 indent, Bool XMTDump)
{
	if (_p && _p->data) {
		if (XMTDump) {
			StartElement(trace, attName ? attName : (char*)"ByteArray", indent, XMTDump);
			indent++;
			DumpData(trace, "array", _p->data, _p->length, indent, XMTDump);
			//indent--;
			EndAttributes(trace, GF_TRUE, GF_FALSE);
		} else {
			DumpData(trace, attName ? attName : "ByteArray", _p->data, _p->length, indent, GF_FALSE);
		}
	}
}


void gf_ipmpx_dump_AUTH(GF_IPMPX_Authentication *ipa, FILE *trace, u32 indent, Bool XMTDump)
{
	switch (ipa->tag) {
	case GF_IPMPX_AUTH_KeyDescr_Tag:
	{
		GF_IPMPX_AUTH_KeyDescriptor *p = (GF_IPMPX_AUTH_KeyDescriptor *)ipa;
		StartElement(trace, "IPMP_KeyDescriptor", indent, XMTDump);
		DumpData(trace, "keyBody", p->keyBody, p->keyBodyLength, indent+1, XMTDump);
		if (XMTDump) EndAttributes(trace, GF_TRUE, GF_FALSE);
		else EndElement(trace, "", indent, GF_FALSE);
	}
	break;
	case GF_IPMPX_AUTH_AlgorithmDescr_Tag:
	{
		GF_IPMPX_AUTH_AlgorithmDescriptor *p = (GF_IPMPX_AUTH_AlgorithmDescriptor *)ipa;
		StartElement(trace, "IPMP_AlgorithmDescriptor", indent, XMTDump);
		if (p->regAlgoID) {
			DumpInt(trace, "regAlgoID", p->regAlgoID, indent+1, XMTDump);
		} else {
			gf_ipmpx_dump_ByteArray(p->specAlgoID, "specAlgoID", trace, indent+1, XMTDump);
		}
		EndAttributes(trace, XMTDump, GF_TRUE);
		if (p->OpaqueData) gf_ipmpx_dump_ByteArray(p->OpaqueData, "OpaqueData", trace, indent+1, XMTDump);

		EndElement(trace, "IPMP_AlgorithmDescriptor", indent, XMTDump);
	}
	break;
	}
}

void gf_ipmpx_dump_BaseData(GF_IPMPX_Data *_p, FILE *trace, u32 indent, Bool XMTDump)
{
#if 0
	if (XMTDump) {
		StartElement(trace, "IPMP_BaseData", indent, XMTDump);
		DumpInt(trace, "dataID", _p->dataID, indent, 1);
		DumpInt(trace, "Version", _p->Version, indent, 1);
		EndLeafAttribute(trace, indent, XMTDump);
	} else {
		DumpInt(trace, "dataID", _p->dataID, indent, 0);
		DumpInt(trace, "Version", _p->Version, indent, 0);
	}
#endif
}

GF_Err gf_ipmpx_dump_OpaqueData(GF_IPMPX_Data *_p, FILE *trace, u32 indent, Bool XMTDump)
{
	GF_IPMPX_OpaqueData *p = (GF_IPMPX_OpaqueData *)_p;

	StartElement(trace, (p->tag==GF_IPMPX_RIGHTS_DATA_TAG) ? "IPMP_RightsData" : "IPMP_OpaqueData", indent, XMTDump);
	indent++;
	EndAttributes(trace, XMTDump, GF_TRUE);
	gf_ipmpx_dump_BaseData(_p, trace, indent, XMTDump);
	gf_ipmpx_dump_ByteArray(p->opaqueData, (p->tag==GF_IPMPX_RIGHTS_DATA_TAG) ? "rightsInfo" : "opaqueData", trace, indent, XMTDump);
	indent--;
	EndElement(trace, (p->tag==GF_IPMPX_RIGHTS_DATA_TAG) ? "IPMP_RightsData" : "IPMP_OpaqueData", indent, XMTDump);
	return GF_OK;
}

GF_Err gf_ipmpx_dump_KeyData(GF_IPMPX_Data *_p, FILE *trace, u32 indent, Bool XMTDump)
{
	GF_IPMPX_KeyData*p = (GF_IPMPX_KeyData*)_p;

	StartElement(trace, "IPMP_KeyData", indent, XMTDump);
	indent++;

	DumpBool(trace, "hasStartDTS", (p->flags & 1) ? 1 : 0, indent, XMTDump);
	DumpBool(trace, "hasStartPacketID", (p->flags & 2) ? 1 : 0, indent, XMTDump);
	DumpBool(trace, "hasEndDTS", (p->flags & 4) ? 1 : 0, indent, XMTDump);
	DumpBool(trace, "hasEndPacketID", (p->flags & 8) ? 1 : 0, indent, XMTDump);

	if (p->flags & 1) DumpLargeInt(trace, "startDTS", p->startDTS, indent, XMTDump);
	if (p->flags & 2) DumpInt(trace, "startPacketID", p->startPacketID, indent, XMTDump);
	if (p->flags & 4) DumpLargeInt(trace, "expireDTS", p->expireDTS, indent, XMTDump);
	if (p->flags & 8) DumpInt(trace, "expirePacketID", p->expirePacketID, indent, XMTDump);

	EndAttributes(trace, XMTDump, GF_TRUE);
	gf_ipmpx_dump_BaseData(_p, trace, indent, XMTDump);
	gf_ipmpx_dump_ByteArray(p->keyBody, "keyBody", trace, indent, XMTDump);
	gf_ipmpx_dump_ByteArray(p->OpaqueData, "OpaqueData", trace, indent, XMTDump);

	indent--;
	EndElement(trace, "IPMP_KeyData", indent, XMTDump);
	return GF_OK;
}
GF_Err gf_ipmpx_dump_SecureContainer(GF_IPMPX_Data *_p, FILE *trace, u32 indent, Bool XMTDump)
{
	GF_IPMPX_SecureContainer*p = (GF_IPMPX_SecureContainer*)_p;
	StartElement(trace, "IPMP_SecureContainer", indent, XMTDump);
	indent++;
	DumpBool(trace, "isMACEncrypted", p->isMACEncrypted, indent, XMTDump);
	EndAttributes(trace, XMTDump, GF_TRUE);
	gf_ipmpx_dump_BaseData(_p, trace, indent, XMTDump);
	if (p->encryptedData) gf_ipmpx_dump_ByteArray(p->encryptedData, "encryptedData", trace, indent, XMTDump);
	if (p->protectedMsg) gf_ipmpx_dump_data(p->protectedMsg, trace, indent, XMTDump);
	if (p->MAC) gf_ipmpx_dump_ByteArray(p->MAC, "MAC", trace, indent, XMTDump);
	indent--;
	EndElement(trace, "IPMP_SecureContainer", indent, XMTDump);
	return GF_OK;
}
GF_Err gf_ipmpx_dump_InitAuthentication(GF_IPMPX_Data *_p, FILE *trace, u32 indent, Bool XMTDump)
{
	GF_IPMPX_InitAuthentication*p = (GF_IPMPX_InitAuthentication*)_p;
	StartElement(trace, "IPMP_InitAuthentication", indent, XMTDump);
	indent++;
	DumpInt(trace, "Context", p->Context, indent, XMTDump);
	DumpInt(trace, "AuthType", p->AuthType, indent, XMTDump);
	EndAttributes(trace, XMTDump, GF_TRUE);
	gf_ipmpx_dump_BaseData(_p, trace, indent, XMTDump);
	indent--;
	EndElement(trace, "IPMP_InitAuthentication", indent, XMTDump);
	return GF_OK;
}

GF_Err gf_ipmpx_dump_TrustSecurityMetadata(GF_IPMPX_Data *_p, FILE *trace, u32 indent, Bool XMTDump)
{
	u32 i, j;
	GF_IPMPX_TrustSecurityMetadata*p = (GF_IPMPX_TrustSecurityMetadata*)_p;
	StartElement(trace, "IPMP_TrustSecurityMetadata", indent, XMTDump);
	indent++;
	EndAttributes(trace, XMTDump, GF_TRUE);
	gf_ipmpx_dump_BaseData(_p, trace, indent, XMTDump);

	StartList(trace, "trustedTools", indent, XMTDump);
	indent++;

	for (i=0; i<gf_list_count(p->TrustedTools); i++) {
		GF_IPMPX_TrustedTool *tt = (GF_IPMPX_TrustedTool *)gf_list_get(p->TrustedTools, i);
		StartElement(trace, "IPMP_TrustedTool", indent, XMTDump);
		indent++;
		DumpBin128(trace, "toolID", (char *)tt->toolID, indent, XMTDump);
		DumpDate(trace, "AuditDate", tt->AuditDate, indent, XMTDump);
		EndAttributes(trace, XMTDump, GF_TRUE);
		StartList(trace, "trustSpecifications", indent, XMTDump);
		indent++;
		for (j=0; j<gf_list_count(tt->trustSpecifications); j++) {
			GF_IPMPX_TrustSpecification *ts = (GF_IPMPX_TrustSpecification *)gf_list_get(tt->trustSpecifications, j);
			StartElement(trace, "IPMP_TrustSpecification", indent, XMTDump);
			indent++;
			DumpDate(trace, "startDate", ts->startDate, indent, XMTDump);
			DumpInt(trace, "attackerProfile", ts->attackerProfile, indent, XMTDump);
			DumpInt(trace, "trustedDuration", ts->trustedDuration, indent, XMTDump);
			EndAttributes(trace, XMTDump, GF_TRUE);
			if (ts->CCTrustMetadata) gf_ipmpx_dump_ByteArray(ts->CCTrustMetadata, "CCTrustMetadata", trace, indent, XMTDump);
			indent--;
			EndElement(trace, "IPMP_TrustSpecification", indent, XMTDump);
		}
		indent--;
		EndList(trace, "trustSpecifications", indent, XMTDump);
		indent--;
		EndElement(trace, "IPMP_TrustedTool", indent, XMTDump);
	}
	indent--;
	EndList(trace, "trustedTools", indent, XMTDump);

	indent--;
	EndElement(trace, "IPMP_TrustSecurityMetadata", indent, XMTDump);
	return GF_OK;
}

GF_Err gf_ipmpx_dump_MutualAuthentication(GF_IPMPX_Data *_p, FILE *trace, u32 indent, Bool XMTDump)
{
	u32 i, count;
	GF_IPMPX_MutualAuthentication *p = (GF_IPMPX_MutualAuthentication*)_p;
	StartElement(trace, "IPMP_MutualAuthentication", indent, XMTDump);
	indent++;
	DumpBool(trace, "failedNegotiation", p->failedNegotiation, indent, XMTDump);
	if (gf_list_count(p->certificates)) DumpInt(trace, "certType", p->certType, indent, XMTDump);

	EndAttributes(trace, XMTDump, GF_TRUE);
	gf_ipmpx_dump_BaseData(_p, trace, indent, XMTDump);

	StartList(trace, "candidateAlgorithms", indent, XMTDump);
	count = gf_list_count(p->candidateAlgorithms);
	indent++;
	for (i=0; i<count; i++) {
		GF_IPMPX_Authentication *ip_auth = (GF_IPMPX_Authentication *)gf_list_get(p->candidateAlgorithms, i);
		gf_ipmpx_dump_AUTH(ip_auth, trace, indent, XMTDump);
	}
	indent--;
	EndList(trace, "candidateAlgorithms", indent, XMTDump);

	StartList(trace, "agreedAlgorithms", indent, XMTDump);
	count = gf_list_count(p->agreedAlgorithms);
	indent++;
	for (i=0; i<count; i++) {
		GF_IPMPX_Authentication *ip_auth = (GF_IPMPX_Authentication *)gf_list_get(p->agreedAlgorithms, i);
		gf_ipmpx_dump_AUTH(ip_auth, trace, indent, XMTDump);
	}
	indent--;
	EndList(trace, "agreedAlgorithms", indent, XMTDump);

	if (p->AuthenticationData) gf_ipmpx_dump_ByteArray(p->AuthenticationData, "AuthenticationData", trace, indent, XMTDump);

	count = gf_list_count(p->certificates);
	if (count || p->opaque || p->publicKey) {
		/*type 1*/
		if (count) {
			StartList(trace, "certificates", indent, XMTDump);
			for (i=0; i<count; i++) {
				GF_IPMPX_ByteArray *ipd = (GF_IPMPX_ByteArray *)gf_list_get(p->certificates, i);
				if (XMTDump) {
					gf_ipmpx_dump_ByteArray(ipd, NULL, trace, indent, XMTDump);
				} else {
					StartAttribute(trace, "", indent, GF_FALSE);
					DumpData(trace, NULL, ipd->data, ipd->length, indent, GF_FALSE);
					if (i+1<count) gf_fprintf(trace, ",");
					gf_fprintf(trace, "\n");
				}
			}
			EndList(trace, "certificates", indent, XMTDump);
		}
		/*type 2*/
		else if (p->publicKey) {
			gf_ipmpx_dump_AUTH((GF_IPMPX_Authentication *) p->publicKey, trace, indent, XMTDump);
		}
		/*type 0xFE*/
		else if (p->opaque) {
			gf_ipmpx_dump_ByteArray(p->opaque, "opaque", trace, indent, XMTDump);
		}
		if (!XMTDump) StartAttribute(trace, "trustData", indent, GF_FALSE);
		else {
			StartElement(trace, "trustData", indent, XMTDump);
			EndAttributes(trace, XMTDump, GF_TRUE);
		}
		gf_ipmpx_dump_data((GF_IPMPX_Data *)p->trustData, trace, indent, XMTDump);
		if (XMTDump) EndElement(trace, "trustData", indent, XMTDump);
		gf_ipmpx_dump_ByteArray(p->authCodes, "authCodes", trace, indent, XMTDump);
	}

	indent--;
	EndElement(trace, "IPMP_MutualAuthentication", indent, XMTDump);
	return GF_OK;
}

GF_Err gf_ipmpx_dump_GetToolsResponse(GF_IPMPX_Data *_p, FILE *trace, u32 indent, Bool XMTDump)
{
	GF_IPMPX_GetToolsResponse*p = (GF_IPMPX_GetToolsResponse*)_p;
	StartElement(trace, "IPMP_GetToolsResponse", indent, XMTDump);
	indent++;
	EndAttributes(trace, XMTDump, GF_TRUE);
	gf_ipmpx_dump_BaseData(_p, trace, indent, XMTDump);
	DumpDescList(p->ipmp_tools, trace, indent, "IPMP_Tools", XMTDump, GF_FALSE);
	indent--;
	EndElement(trace, "IPMP_GetToolsResponse", indent, XMTDump);
	return GF_OK;
}

GF_Err gf_ipmpx_dump_ParametricDescription(GF_IPMPX_Data *_p, FILE *trace, u32 indent, Bool XMTDump)
{
	u32 i;
	GF_IPMPX_ParametricDescription*p = (GF_IPMPX_ParametricDescription*)_p;
	StartElement(trace, "IPMP_ParametricDescription", indent, XMTDump);
	indent++;
	DumpInt(trace, "majorVersion", p->majorVersion, indent, XMTDump);
	DumpInt(trace, "minorVersion", p->minorVersion, indent, XMTDump);
	EndAttributes(trace, XMTDump, GF_TRUE);
	gf_ipmpx_dump_BaseData(_p, trace, indent, XMTDump);
	gf_ipmpx_dump_ByteArray(p->descriptionComment, "descriptionComment", trace, indent, XMTDump);

	StartList(trace, "descriptions", indent, XMTDump);
	indent++;
	for (i=0; i<gf_list_count(p->descriptions); i++) {
		GF_IPMPX_ParametricDescriptionItem *it = (GF_IPMPX_ParametricDescriptionItem *)gf_list_get(p->descriptions, i);
		StartElement(trace, "IPMP_ParametricDescriptionItem", indent, XMTDump);
		indent++;
		EndAttributes(trace, XMTDump, GF_TRUE);
		gf_ipmpx_dump_ByteArray(it->main_class, "class", trace, indent, XMTDump);
		gf_ipmpx_dump_ByteArray(it->subClass, "subClass", trace, indent, XMTDump);
		gf_ipmpx_dump_ByteArray(it->typeData, "typeData", trace, indent, XMTDump);
		gf_ipmpx_dump_ByteArray(it->type, "type", trace, indent, XMTDump);
		gf_ipmpx_dump_ByteArray(it->addedData, "addedData", trace, indent, XMTDump);
		indent--;
		EndElement(trace, "IPMP_ParametricDescriptionItem", indent, XMTDump);
	}
	indent--;
	EndList(trace, "descriptions", indent, XMTDump);

	indent--;
	EndElement(trace, "IPMP_ParametricDescription", indent, XMTDump);
	return GF_OK;
}
GF_Err gf_ipmpx_dump_ToolParamCapabilitiesQuery(GF_IPMPX_Data *_p, FILE *trace, u32 indent, Bool XMTDump)
{
	GF_IPMPX_ToolParamCapabilitiesQuery*p = (GF_IPMPX_ToolParamCapabilitiesQuery*)_p;
	StartElement(trace, "IPMP_ToolParamCapabilitiesQuery", indent, XMTDump);
	indent++;
	EndAttributes(trace, XMTDump, GF_TRUE);
	gf_ipmpx_dump_BaseData(_p, trace, indent, XMTDump);
	if (!XMTDump) StartAttribute(trace, "description", indent, GF_FALSE);
	else {
		StartElement(trace, "description", indent, XMTDump);
		EndAttributes(trace, XMTDump, GF_TRUE);
	}
	gf_ipmpx_dump_data((GF_IPMPX_Data *) p->description, trace, indent, XMTDump);
	if (XMTDump) EndElement(trace, "description", indent, XMTDump);

	indent--;
	EndElement(trace, "IPMP_ToolParamCapabilitiesQuery", indent, XMTDump);
	return GF_OK;
}
GF_Err gf_ipmpx_dump_ToolParamCapabilitiesResponse(GF_IPMPX_Data *_p, FILE *trace, u32 indent, Bool XMTDump)
{
	GF_IPMPX_ToolParamCapabilitiesResponse*p = (GF_IPMPX_ToolParamCapabilitiesResponse*)_p;
	StartElement(trace, "IPMP_ToolParamCapabilitiesResponse", indent, XMTDump);
	indent++;
	DumpBool(trace, "capabilitiesSupported", p->capabilitiesSupported, indent, XMTDump);
	EndAttributes(trace, XMTDump, GF_TRUE);
	gf_ipmpx_dump_BaseData(_p, trace, indent, XMTDump);
	indent--;
	EndElement(trace, "IPMP_ToolParamCapabilitiesResponse", indent, XMTDump);
	return GF_OK;
}
GF_Err gf_ipmpx_dump_ConnectTool(GF_IPMPX_Data *_p, FILE *trace, u32 indent, Bool XMTDump)
{
	GF_IPMPX_ConnectTool*p = (GF_IPMPX_ConnectTool*)_p;
	StartElement(trace, "IPMP_ConnectTool", indent, XMTDump);
	indent++;
	EndAttributes(trace, XMTDump, GF_TRUE);
	gf_ipmpx_dump_BaseData(_p, trace, indent, XMTDump);
	if (!XMTDump) StartAttribute(trace, "toolDescriptor", indent, GF_FALSE);
	else {
		StartElement(trace, "toolDescriptor", indent, XMTDump);
		EndAttributes(trace, XMTDump, GF_TRUE);
	}
	gf_odf_dump_desc((GF_Descriptor *)p->toolDescriptor, trace, indent, XMTDump);
	if (XMTDump) EndElement(trace, "toolDescriptor", indent, XMTDump);

	indent--;
	EndElement(trace, "IPMP_ConnectTool", indent, XMTDump);
	return GF_OK;
}
GF_Err gf_ipmpx_dump_DisconnectTool(GF_IPMPX_Data *_p, FILE *trace, u32 indent, Bool XMTDump)
{
	GF_IPMPX_DisconnectTool*p = (GF_IPMPX_DisconnectTool*)_p;
	StartElement(trace, "IPMP_DisconnectTool", indent, XMTDump);
	indent++;
	DumpInt(trace, "IPMP_ToolContextID", p->IPMP_ToolContextID, indent, XMTDump);
	EndAttributes(trace, XMTDump, GF_TRUE);
	gf_ipmpx_dump_BaseData(_p, trace, indent, XMTDump);
	indent--;
	EndElement(trace, "IPMP_DisconnectTool", indent, XMTDump);
	return GF_OK;
}
GF_Err gf_ipmpx_dump_GetToolContext(GF_IPMPX_Data *_p, FILE *trace, u32 indent, Bool XMTDump)
{
	GF_IPMPX_GetToolContext*p = (GF_IPMPX_GetToolContext*)_p;
	StartElement(trace, "IPMP_GetToolContext", indent, XMTDump);
	indent++;
	DumpInt(trace, "scope", p->scope, indent, XMTDump);
	DumpInt(trace, "IPMP_DescriptorIDEx", p->IPMP_DescriptorIDEx, indent, XMTDump);
	EndAttributes(trace, XMTDump, GF_TRUE);
	gf_ipmpx_dump_BaseData(_p, trace, indent, XMTDump);
	indent--;
	EndElement(trace, "IPMP_GetToolContext", indent, XMTDump);
	return GF_OK;
}
GF_Err gf_ipmpx_dump_GetToolContextResponse(GF_IPMPX_Data *_p, FILE *trace, u32 indent, Bool XMTDump)
{
	GF_IPMPX_GetToolContextResponse*p = (GF_IPMPX_GetToolContextResponse*)_p;
	StartElement(trace, "IPMP_GetToolContextResponse", indent, XMTDump);
	indent++;
	DumpInt(trace, "OD_ID", p->OD_ID, indent, XMTDump);
	DumpInt(trace, "ESD_ID", p->ESD_ID, indent, XMTDump);
	DumpInt(trace, "IPMP_ToolContextID", p->IPMP_ToolContextID, indent, XMTDump);
	EndAttributes(trace, XMTDump, GF_TRUE);
	gf_ipmpx_dump_BaseData(_p, trace, indent, XMTDump);
	indent--;
	EndElement(trace, "IPMP_GetToolContextResponse", indent, XMTDump);
	return GF_OK;
}
GF_Err gf_ipmpx_dump_AddToolNotificationListener(GF_IPMPX_Data *_p, FILE *trace, u32 indent, Bool XMTDump)
{
	u32 i;
	GF_IPMPX_AddToolNotificationListener*p = (GF_IPMPX_AddToolNotificationListener*)_p;
	StartElement(trace, "IPMP_AddToolNotificationListener", indent, XMTDump);
	indent++;
	DumpInt(trace, "scope", p->scope, indent, XMTDump);
	StartAttribute(trace, "eventType", indent, XMTDump);
	if (!XMTDump) gf_fprintf(trace, "\"");
	for (i=0; i<p->eventTypeCount; i++) {
		if (XMTDump) {
			gf_fprintf(trace, "\'%d\'", p->eventType[i]);
			if (i+1<p->eventTypeCount) gf_fprintf(trace, " ");
		} else {
			gf_fprintf(trace, "%d", p->eventType[i]);
			if (i+1<p->eventTypeCount) gf_fprintf(trace, ",");
		}
	}
	if (!XMTDump) gf_fprintf(trace, "\"");
	EndAttribute(trace, indent, XMTDump);
	EndAttributes(trace, XMTDump, GF_TRUE);
	gf_ipmpx_dump_BaseData(_p, trace, indent, XMTDump);
	indent--;
	EndElement(trace, "IPMP_AddToolNotificationListener", indent, XMTDump);
	return GF_OK;
}
GF_Err gf_ipmpx_dump_RemoveToolNotificationListener(GF_IPMPX_Data *_p, FILE *trace, u32 indent, Bool XMTDump)
{
	u32 i;
	GF_IPMPX_RemoveToolNotificationListener*p = (GF_IPMPX_RemoveToolNotificationListener*)_p;
	StartElement(trace, "IPMP_RemoveToolNotificationListener", indent, XMTDump);
	indent++;
	StartAttribute(trace, "eventType", indent, XMTDump);
	if (!XMTDump) gf_fprintf(trace, "\"");
	for (i=0; i<p->eventTypeCount; i++) {
		if (XMTDump) {
			gf_fprintf(trace, "\'%d\'", p->eventType[i]);
			if (i+1<p->eventTypeCount) gf_fprintf(trace, " ");
		} else {
			gf_fprintf(trace, "%d", p->eventType[i]);
			if (i+1<p->eventTypeCount) gf_fprintf(trace, ",");
		}
	}
	if (!XMTDump) gf_fprintf(trace, "\"");
	EndAttribute(trace, indent, XMTDump);
	EndAttributes(trace, XMTDump, GF_TRUE);
	gf_ipmpx_dump_BaseData(_p, trace, indent, XMTDump);
	indent--;
	EndElement(trace, "IPMP_RemoveToolNotificationListener", indent, XMTDump);
	return GF_OK;
}
GF_Err gf_ipmpx_dump_NotifyToolEvent(GF_IPMPX_Data *_p, FILE *trace, u32 indent, Bool XMTDump)
{
	GF_IPMPX_NotifyToolEvent*p = (GF_IPMPX_NotifyToolEvent*)_p;
	StartElement(trace, "IPMP_NotifyToolEvent", indent, XMTDump);
	indent++;
	DumpInt(trace, "OD_ID", p->OD_ID, indent, XMTDump);
	DumpInt(trace, "ESD_ID", p->ESD_ID, indent, XMTDump);
	DumpInt(trace, "IPMP_ToolContextID", p->IPMP_ToolContextID, indent, XMTDump);
	DumpInt(trace, "eventType", p->eventType, indent, XMTDump);
	EndAttributes(trace, XMTDump, GF_TRUE);
	gf_ipmpx_dump_BaseData(_p, trace, indent, XMTDump);
	indent--;
	EndElement(trace, "IPMP_NotifyToolEvent", indent, XMTDump);
	return GF_OK;
}
GF_Err gf_ipmpx_dump_CanProcess(GF_IPMPX_Data *_p, FILE *trace, u32 indent, Bool XMTDump)
{
	GF_IPMPX_CanProcess*p = (GF_IPMPX_CanProcess*)_p;
	StartElement(trace, "IPMP_CanProcess", indent, XMTDump);
	indent++;
	DumpBool(trace, "canProcess", p->canProcess, indent, XMTDump);
	EndAttributes(trace, XMTDump, GF_TRUE);
	gf_ipmpx_dump_BaseData(_p, trace, indent, XMTDump);
	indent--;
	EndElement(trace, "IPMP_CanProcess", indent, XMTDump);
	return GF_OK;
}
GF_Err gf_ipmpx_dump_ToolAPI_Config(GF_IPMPX_Data *_p, FILE *trace, u32 indent, Bool XMTDump)
{
	GF_IPMPX_ToolAPI_Config*p = (GF_IPMPX_ToolAPI_Config*)_p;
	StartElement(trace, "IPMP_ToolAPI_Config", indent, XMTDump);
	indent++;
	DumpInt(trace, "Instantiation_API_ID", p->Instantiation_API_ID, indent, XMTDump);
	DumpInt(trace, "Messaging_API_ID", p->Messaging_API_ID, indent, XMTDump);
	EndAttributes(trace, XMTDump, GF_TRUE);
	gf_ipmpx_dump_BaseData(_p, trace, indent, XMTDump);
	gf_ipmpx_dump_ByteArray(p->opaqueData, "opaqueData", trace, indent, XMTDump);
	indent--;
	EndElement(trace, "IPMP_ToolAPI_Config", indent, XMTDump);
	return GF_OK;
}
GF_Err gf_ipmpx_dump_WatermarkingInit(GF_IPMPX_Data *_p, FILE *trace, u32 indent, Bool XMTDump)
{
	GF_IPMPX_WatermarkingInit*p = (GF_IPMPX_WatermarkingInit*)_p;
	StartElement(trace, (char*)  ( (_p->tag==GF_IPMPX_AUDIO_WM_INIT_TAG) ? "IPMP_AudioWatermarkingInit" : "IPMP_VideoWatermarkingInit"), indent, XMTDump);
	indent++;
	DumpInt(trace, "inputFormat", p->inputFormat, indent, XMTDump);
	DumpInt(trace, "requiredOp", p->requiredOp, indent, XMTDump);
	if (p->inputFormat==0x01) {
		if (_p->tag==GF_IPMPX_AUDIO_WM_INIT_TAG) {
			DumpInt(trace, "nChannels", p->nChannels, indent, XMTDump);
			DumpInt(trace, "bitPerSample", p->bitPerSample, indent, XMTDump);
			DumpInt(trace, "frequency", p->frequency, indent, XMTDump);
		} else {
			DumpInt(trace, "frame_horizontal_size", p->frame_horizontal_size, indent, XMTDump);
			DumpInt(trace, "frame_vertical_size", p->frame_vertical_size, indent, XMTDump);
			DumpInt(trace, "chroma_format", p->chroma_format, indent, XMTDump);
		}
	}
	switch (p->requiredOp) {
	case GF_IPMPX_WM_INSERT:
	case GF_IPMPX_WM_REMARK:
		DumpData(trace, "wmPayload", p->wmPayload, p->wmPayloadLen, indent, XMTDump);
		break;
	case GF_IPMPX_WM_EXTRACT:
	case GF_IPMPX_WM_DETECT_COMPRESSION:
		DumpInt(trace, "wmRecipientId", p->wmRecipientId, indent, XMTDump);
		break;
	}
	if (p->opaqueData) DumpData(trace, "opaqueData", p->opaqueData, p->opaqueDataSize, indent, XMTDump);

	EndAttributes(trace, XMTDump, GF_TRUE);
	gf_ipmpx_dump_BaseData(_p, trace, indent, XMTDump);
	indent--;
	EndElement(trace, (char*) ( (_p->tag==GF_IPMPX_AUDIO_WM_INIT_TAG) ? "IPMP_AudioWatermarkingInit" : "IPMP_VideoWatermarkingInit"), indent, XMTDump);
	return GF_OK;
}
GF_Err gf_ipmpx_dump_SendWatermark(GF_IPMPX_Data *_p, FILE *trace, u32 indent, Bool XMTDump)
{
	GF_IPMPX_SendWatermark*p = (GF_IPMPX_SendWatermark*)_p;
	StartElement(trace, (char*) ((_p->tag==GF_IPMPX_AUDIO_WM_SEND_TAG) ? "IPMP_SendAudioWatermark" : "IPMP_SendVideoWatermark"), indent, XMTDump);
	indent++;
	DumpInt(trace, "wmStatus", p->wm_status, indent, XMTDump);
	DumpInt(trace, "compression_status", p->compression_status, indent, XMTDump);
	EndAttributes(trace, XMTDump, GF_TRUE);
	gf_ipmpx_dump_BaseData(_p, trace, indent, XMTDump);
	if (p->wm_status==GF_IPMPX_WM_PAYLOAD) gf_ipmpx_dump_ByteArray(p->payload, "payload", trace, indent, XMTDump);
	if (p->opaqueData) gf_ipmpx_dump_ByteArray(p->opaqueData, "opaqueData", trace, indent, XMTDump);
	indent--;
	EndElement(trace, (char*) ( (_p->tag==GF_IPMPX_AUDIO_WM_SEND_TAG) ? "IPMP_SendAudioWatermark" : "IPMP_SendVideoWatermark"), indent, XMTDump);
	return GF_OK;
}

GF_Err gf_ipmpx_dump_SelectiveDecryptionInit(GF_IPMPX_Data *_p, FILE *trace, u32 indent, Bool XMTDump)
{
	u32 i, count;
	GF_IPMPX_SelectiveDecryptionInit*p = (GF_IPMPX_SelectiveDecryptionInit*)_p;
	StartElement(trace, "IPMP_SelectiveDecryptionInit", indent, XMTDump);
	indent++;
	DumpInt(trace, "mediaTypeExtension", p->mediaTypeExtension, indent, XMTDump);
	DumpInt(trace, "mediaTypeIndication", p->mediaTypeIndication, indent, XMTDump);
	DumpInt(trace, "profileLevelIndication", p->profileLevelIndication, indent, XMTDump);
	DumpInt(trace, "compliance", p->compliance, indent, XMTDump);
	if (p->RLE_Data) DumpData_16(trace, "RLE_Data", p->RLE_Data, p->RLE_DataLength, indent, XMTDump);
	EndAttributes(trace, XMTDump, GF_TRUE);
	gf_ipmpx_dump_BaseData(_p, trace, indent, XMTDump);

	count = gf_list_count(p->SelEncBuffer);
	if (count) {
		StartList(trace, "SelectiveBuffers", indent, XMTDump);
		indent++;
		for (i=0; i<count; i++) {
			GF_IPMPX_SelEncBuffer *sb = (GF_IPMPX_SelEncBuffer *)gf_list_get(p->SelEncBuffer, i);
			StartElement(trace, "IPMP_SelectiveBuffer", indent, XMTDump);
			indent++;
			DumpBin128(trace, "cipher_Id", (char*)sb->cipher_Id, indent, XMTDump);
			DumpInt(trace, "syncBoundary", sb->syncBoundary, indent, XMTDump);
			if (!sb->Stream_Cipher_Specific_Init_Info) {
				DumpInt(trace, "mode", sb->mode, indent, XMTDump);
				DumpInt(trace, "blockSize", sb->blockSize, indent, XMTDump);
				DumpInt(trace, "keySize", sb->keySize, indent, XMTDump);
			}
			EndAttributes(trace, XMTDump, GF_TRUE);
			if (sb->Stream_Cipher_Specific_Init_Info)
				gf_ipmpx_dump_ByteArray(sb->Stream_Cipher_Specific_Init_Info, "StreamCipher", trace, indent, XMTDump);

			indent--;
			EndElement(trace, "IPMP_SelectiveBuffer", indent, XMTDump);
		}
		indent--;
		EndList(trace, "SelectiveBuffers", indent, XMTDump);
	}

	count = gf_list_count(p->SelEncFields);
	if (!p->RLE_Data && count) {
		StartList(trace, "SelectiveFields", indent, XMTDump);
		indent++;
		for (i=0; i<count; i++) {
			GF_IPMPX_SelEncField *sf = (GF_IPMPX_SelEncField *)gf_list_get(p->SelEncFields, i);
			StartElement(trace, "IPMP_SelectiveField", indent, XMTDump);
			indent++;
			DumpInt(trace, "field_Id", sf->field_Id, indent, XMTDump);
			DumpInt(trace, "field_Scope", sf->field_Scope, indent, XMTDump);
			DumpInt(trace, "buf", sf->buf, indent, XMTDump);
			if (sf->mappingTable) DumpData_16(trace, "mappingTable", sf->mappingTable, sf->mappingTableSize, indent, XMTDump);
			EndAttributes(trace, XMTDump, GF_TRUE);
			if (sf->shuffleSpecificInfo)
				gf_ipmpx_dump_ByteArray(sf->shuffleSpecificInfo, "shuffleSpecificInfo", trace, indent, XMTDump);

			indent--;
			EndElement(trace, "IPMP_SelectiveField", indent, XMTDump);
		}
		indent--;
		EndList(trace, "SelectiveFields", indent, XMTDump);
	}

	indent--;
	EndElement(trace, "IPMP_SelectiveDecryptionInit", indent, XMTDump);
	return GF_OK;
}

GF_Err gf_ipmpx_dump_ISMACryp(GF_IPMPX_Data *_p, FILE *trace, u32 indent, Bool XMTDump)
{
	GF_IPMPX_ISMACryp*p = (GF_IPMPX_ISMACryp*)_p;
	StartElement(trace, "ISMACryp_Data", indent, XMTDump);
	indent++;
	DumpInt(trace, "crypto_suite", p->cryptoSuite, indent, XMTDump);
	DumpInt(trace, "IV_length", p->IV_length, indent, XMTDump);
	DumpBool(trace, "selective_encryption", p->use_selective_encryption, indent, XMTDump);
	DumpInt(trace, "key_indicator_length", p->key_indicator_length, indent, XMTDump);
	EndAttributes(trace, XMTDump, GF_TRUE);
	gf_ipmpx_dump_BaseData(_p, trace, indent, XMTDump);
	indent--;
	EndElement(trace, "ISMACryp_Data", indent, XMTDump);
	return GF_OK;
}

GF_Err gf_ipmpx_dump_data(GF_IPMPX_Data *_p, FILE *trace, u32 indent, Bool XMTDump)
{
	switch (_p->tag) {
	case GF_IPMPX_RIGHTS_DATA_TAG:
	case GF_IPMPX_OPAQUE_DATA_TAG:
		return gf_ipmpx_dump_OpaqueData(_p, trace, indent, XMTDump);
	case GF_IPMPX_KEY_DATA_TAG:
		return gf_ipmpx_dump_KeyData(_p, trace, indent, XMTDump);
	case GF_IPMPX_SECURE_CONTAINER_TAG:
		return gf_ipmpx_dump_SecureContainer(_p, trace, indent, XMTDump);
	case GF_IPMPX_INIT_AUTHENTICATION_TAG:
		return gf_ipmpx_dump_InitAuthentication(_p, trace, indent, XMTDump);
	case GF_IPMPX_TRUST_SECURITY_METADATA_TAG:
		return gf_ipmpx_dump_TrustSecurityMetadata(_p, trace, indent, XMTDump);
	case GF_IPMPX_MUTUAL_AUTHENTICATION_TAG:
		return gf_ipmpx_dump_MutualAuthentication(_p, trace, indent, XMTDump);
	case GF_IPMPX_GET_TOOLS_RESPONSE_TAG:
		return gf_ipmpx_dump_GetToolsResponse(_p, trace, indent, XMTDump);
	case GF_IPMPX_PARAMETRIC_DESCRIPTION_TAG:
		return gf_ipmpx_dump_ParametricDescription(_p, trace, indent, XMTDump);
	case GF_IPMPX_PARAMETRIC_CAPS_QUERY_TAG:
		return gf_ipmpx_dump_ToolParamCapabilitiesQuery(_p, trace, indent, XMTDump);
	case GF_IPMPX_PARAMETRIC_CAPS_RESPONSE_TAG:
		return gf_ipmpx_dump_ToolParamCapabilitiesResponse(_p, trace, indent, XMTDump);
	case GF_IPMPX_GET_TOOL_CONTEXT_TAG:
		return gf_ipmpx_dump_GetToolContext(_p, trace, indent, XMTDump);
	case GF_IPMPX_GET_TOOL_CONTEXT_RESPONSE_TAG:
		return gf_ipmpx_dump_GetToolContextResponse(_p, trace, indent, XMTDump);
	case GF_IPMPX_CONNECT_TOOL_TAG:
		return gf_ipmpx_dump_ConnectTool(_p, trace, indent, XMTDump);
	case GF_IPMPX_DISCONNECT_TOOL_TAG:
		return gf_ipmpx_dump_DisconnectTool(_p, trace, indent, XMTDump);
	case GF_IPMPX_ADD_TOOL_LISTENER_TAG:
		return gf_ipmpx_dump_AddToolNotificationListener(_p, trace, indent, XMTDump);
	case GF_IPMPX_REMOVE_TOOL_LISTENER_TAG:
		return gf_ipmpx_dump_RemoveToolNotificationListener(_p, trace, indent, XMTDump);
	case GF_IPMPX_NOTIFY_TOOL_EVENT_TAG:
		return gf_ipmpx_dump_NotifyToolEvent(_p, trace, indent, XMTDump);
	case GF_IPMPX_CAN_PROCESS_TAG:
		return gf_ipmpx_dump_CanProcess(_p, trace, indent, XMTDump);
	case GF_IPMPX_ISMACRYP_TAG:
		return gf_ipmpx_dump_ISMACryp(_p, trace, indent, XMTDump);
	case GF_IPMPX_TOOL_API_CONFIG_TAG:
		return gf_ipmpx_dump_ToolAPI_Config(_p, trace, indent, XMTDump);
	case GF_IPMPX_AUDIO_WM_INIT_TAG:
	case GF_IPMPX_VIDEO_WM_INIT_TAG:
		return gf_ipmpx_dump_WatermarkingInit(_p, trace, indent, XMTDump);
	case GF_IPMPX_AUDIO_WM_SEND_TAG:
	case GF_IPMPX_VIDEO_WM_SEND_TAG:
		return gf_ipmpx_dump_SendWatermark(_p, trace, indent, XMTDump);
	case GF_IPMPX_SEL_DEC_INIT_TAG:
		return gf_ipmpx_dump_SelectiveDecryptionInit(_p, trace, indent, XMTDump);

	/*
		case GF_IPMPX_USER_QUERY_TAG: return gf_ipmpx_dump_UserQuery(_p, trace, indent, XMTDump);
		case GF_IPMPX_USER_RESPONSE_TAG: return gf_ipmpx_dump_UserQueryResponse(_p, trace, indent, XMTDump);
	*/
	case GF_IPMPX_GET_TOOLS_TAG:
		return GF_BAD_PARAM;
	default:
		return GF_BAD_PARAM;
	}
}

#endif /*GPAC_DISABLE_OD_DUMP*/

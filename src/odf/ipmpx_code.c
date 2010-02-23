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

#ifndef GPAC_MINIMAL_ODF


#define GF_IPMPX_DATA_ALLOC(__ptr, __stname, __tag) \
	__ptr = (__stname*)gf_malloc(sizeof(__stname)); \
	if (!__ptr) return NULL;	\
	memset(__ptr, 0, sizeof(__stname));	\
	((GF_IPMPX_Data *)__ptr)->tag = __tag;	\
	((GF_IPMPX_Data *)__ptr)->Version = 0x01;	\


#define GF_IPMPX_DELETE_ARRAY(__ar) if (__ar) { if (__ar->data) gf_free(__ar->data); gf_free(__ar); }

u32 get_field_size(u32 size_desc)
{
	if (size_desc < 0x00000080) return 1;
	else if (size_desc < 0x00004000) return 2;
	else if (size_desc < 0x00200000) return 3;
	else return 4;
}

void write_var_size(GF_BitStream *bs, u32 size)
{
	unsigned char vals[4];
	u32 length = size;

	vals[3] = (unsigned char) (length & 0x7f); length >>= 7;
	vals[2] = (unsigned char) ((length & 0x7f) | 0x80); length >>= 7;
	vals[1] = (unsigned char) ((length & 0x7f) | 0x80); length >>= 7;
	vals[0] = (unsigned char) ((length & 0x7f) | 0x80); 
	if (size < 0x00000080) {
		gf_bs_write_int(bs, vals[3], 8);
	} else if (size < 0x00004000) {
		gf_bs_write_int(bs, vals[2], 8);
		gf_bs_write_int(bs, vals[3], 8);
	} else if (size < 0x00200000) {
		gf_bs_write_int(bs, vals[1], 8);
		gf_bs_write_int(bs, vals[2], 8);
		gf_bs_write_int(bs, vals[3], 8);
	} else if (size < 0x10000000) {
		gf_bs_write_int(bs, vals[0], 8);
		gf_bs_write_int(bs, vals[1], 8);
		gf_bs_write_int(bs, vals[2], 8);
		gf_bs_write_int(bs, vals[3], 8);
	} 
}

GF_IPMPX_ByteArray *GF_IPMPX_GetByteArray(GF_BitStream *bs)
{
	GF_IPMPX_ByteArray *ba;
	u32 val, size;
	size = 0;
	do {
		val = gf_bs_read_int(bs, 8);
		size <<= 7;
		size |= val & 0x7F;
	} while ( val & 0x80 );
	if (!size) return NULL;
	ba = (GF_IPMPX_ByteArray*)gf_malloc(sizeof(GF_IPMPX_ByteArray));
	ba->data = (char*)gf_malloc(sizeof(char)*size);
	gf_bs_read_data(bs, ba->data, size);
	ba->length = size;
	return ba;
}

u32 GF_IPMPX_GetByteArraySize(GF_IPMPX_ByteArray *ba)
{
	if (!ba) return 1;
	return ba->length + get_field_size(ba->length);
}

void GF_IPMPX_WriteByteArray(GF_BitStream *bs, GF_IPMPX_ByteArray *ba)
{
	if (!ba || !ba->data) {
		write_var_size(bs, 0);
	} else {
		write_var_size(bs, ba->length);
		gf_bs_write_data(bs, ba->data, ba->length);
	}
}

void GF_IPMPX_AUTH_Delete(GF_IPMPX_Authentication *auth)
{
	if (!auth) return;
	switch (auth->tag) {
	case GF_IPMPX_AUTH_AlgorithmDescr_Tag:
		{
			GF_IPMPX_AUTH_AlgorithmDescriptor *p = (GF_IPMPX_AUTH_AlgorithmDescriptor *)auth;
			GF_IPMPX_DELETE_ARRAY(p->specAlgoID);
			GF_IPMPX_DELETE_ARRAY(p->OpaqueData);
			gf_free(p);
		}
		break;
	case GF_IPMPX_AUTH_KeyDescr_Tag:
		{
			GF_IPMPX_AUTH_KeyDescriptor *p = (GF_IPMPX_AUTH_KeyDescriptor *)auth;
			if (p->keyBody) gf_free(p->keyBody);
			gf_free(p);
		}
		break;
	}
}

u32 GF_IPMPX_AUTH_Size(GF_IPMPX_Authentication *auth)
{
	u32 size;
	if (!auth) return 0;
	switch (auth->tag) {
	case GF_IPMPX_AUTH_AlgorithmDescr_Tag:
		{
			GF_IPMPX_AUTH_AlgorithmDescriptor *p = (GF_IPMPX_AUTH_AlgorithmDescriptor *)auth;
			size = 1 + (p->specAlgoID ? GF_IPMPX_GetByteArraySize(p->specAlgoID) : 2);
			size += GF_IPMPX_GetByteArraySize(p->OpaqueData);
			return size;
		}
		break;
	case GF_IPMPX_AUTH_KeyDescr_Tag:
		{
			GF_IPMPX_AUTH_KeyDescriptor *p = (GF_IPMPX_AUTH_KeyDescriptor *)auth;
			size = p->keyBodyLength;
			return size;
		}
		break;
	default:
		return 0;
	}
}

u32 GF_IPMPX_AUTH_FullSize(GF_IPMPX_Authentication *auth)
{
	u32 size = GF_IPMPX_AUTH_Size(auth);
	size += get_field_size(size);
	size += 1;/*tag*/
	return size;
}

GF_Err WriteGF_IPMPX_AUTH(GF_BitStream *bs, GF_IPMPX_Authentication *auth)
{
	u32 size;

	if (!auth) return GF_OK;
	gf_bs_write_int(bs, auth->tag, 8);

	size = GF_IPMPX_AUTH_Size(auth);
	write_var_size(bs, size);

	switch (auth->tag) {
	case GF_IPMPX_AUTH_AlgorithmDescr_Tag:
		{
			GF_IPMPX_AUTH_AlgorithmDescriptor *p = (GF_IPMPX_AUTH_AlgorithmDescriptor *)auth;
			gf_bs_write_int(bs, p->specAlgoID ? 0 : 1, 1);
			gf_bs_write_int(bs, 0, 7);
			if (p->specAlgoID) {
				GF_IPMPX_WriteByteArray(bs, p->specAlgoID);
			} else {
				gf_bs_write_int(bs, p->regAlgoID, 16);
			}
			GF_IPMPX_WriteByteArray(bs, p->OpaqueData);
		}
		break;
	case GF_IPMPX_AUTH_KeyDescr_Tag:
		{
			GF_IPMPX_AUTH_KeyDescriptor *p = (GF_IPMPX_AUTH_KeyDescriptor *)auth;
			/*tag*/
			gf_bs_write_data(bs, p->keyBody, p->keyBodyLength);
		}
		break;
	default:
		break;
	}
	return GF_OK;
}

GF_Err GF_IPMPX_AUTH_Parse(GF_BitStream *bs, GF_IPMPX_Authentication **auth)
{
	u32 val, size, tag;

	tag = gf_bs_read_int(bs, 8);
	size = 0;
	do {
		val = gf_bs_read_int(bs, 8);
		size <<= 7;
		size |= val & 0x7F;
	} while ( val & 0x80 );
	/*don't know if tolerated in IPMPX*/
	if (!size) return GF_OK;

	switch (tag) {
	case GF_IPMPX_AUTH_KeyDescr_Tag:
		{
			GF_IPMPX_AUTH_KeyDescriptor *p;
			GF_SAFEALLOC(p, GF_IPMPX_AUTH_KeyDescriptor);
			if (!p) return GF_OUT_OF_MEM;
			p->tag = tag;
			p->keyBodyLength = size;
			p->keyBody = (char*)gf_malloc(sizeof(char)*size);
			gf_bs_read_data(bs, p->keyBody, size);
			*auth = (GF_IPMPX_Authentication *)p;
			return GF_OK;
		}
		break;
	case GF_IPMPX_AUTH_AlgorithmDescr_Tag:
		{
			Bool isReg;
			GF_IPMPX_AUTH_AlgorithmDescriptor *p;
			GF_SAFEALLOC(p, GF_IPMPX_AUTH_AlgorithmDescriptor);
			if (!p) return GF_OUT_OF_MEM;
			p->tag = tag;
			isReg = gf_bs_read_int(bs, 1);
			gf_bs_read_int(bs, 7);
			if (isReg) {
				p->regAlgoID = gf_bs_read_int(bs, 16);
			} else {
				p->specAlgoID = GF_IPMPX_GetByteArray(bs);
			}
			p->OpaqueData = GF_IPMPX_GetByteArray(bs);
			*auth = (GF_IPMPX_Authentication *)p;
			return GF_OK;
		}
		break;
	default:
		break;
	}
	return GF_NON_COMPLIANT_BITSTREAM;
}


GF_Err GF_IPMPX_ReadData(GF_BitStream *bs, GF_IPMPX_Data *_p, u32 size);

u32 gf_ipmpx_data_full_size(GF_IPMPX_Data *p)
{
	u32 size;
	if (!p) return 0;
	size = gf_ipmpx_data_size(p);
	size += 5;	/*Version and dataID*/
	size += get_field_size(size);
	size += 1;/*tag*/
	return size;
}

GF_Err gf_ipmpx_data_parse(GF_BitStream *bs, GF_IPMPX_Data **out_data)
{
	GF_Err e;
	u32 val, size;
	u8 tag;
	GF_IPMPX_Data *p;

	*out_data = NULL;
	tag = gf_bs_read_int(bs, 8);
	size = 0;
	do {
		val = gf_bs_read_int(bs, 8);
		size <<= 7;
		size |= val & 0x7F;
	} while ( val & 0x80 );
	/*don't know if tolerated in IPMPX*/
	if (!size) return GF_OK;

	p = gf_ipmpx_data_new(tag);
	if (!p) return GF_NON_COMPLIANT_BITSTREAM;
	p->Version = gf_bs_read_int(bs, 8);
	p->dataID = gf_bs_read_int(bs, 32);
	e = GF_IPMPX_ReadData(bs, p, size);
	if (e) {
		gf_ipmpx_data_del(p);
		return e;
	}
	*out_data = p;
	return GF_OK;
}

GF_Err GF_IPMPX_WriteBase(GF_BitStream *bs, GF_IPMPX_Data *p)
{
	u32 size;

	if (!p) return GF_BAD_PARAM;
	size = gf_ipmpx_data_size(p);
	size += 5;	/*Version & dataID*/
	gf_bs_write_int(bs, p->tag, 8);
	write_var_size(bs, size);
	gf_bs_write_int(bs, p->Version, 8);
	gf_bs_write_int(bs, p->dataID, 32);
	return GF_OK;
}


static GF_IPMPX_Data *NewGF_IPMPX_InitAuthentication()
{
	GF_IPMPX_InitAuthentication *ptr;
	GF_IPMPX_DATA_ALLOC(ptr, GF_IPMPX_InitAuthentication, GF_IPMPX_INIT_AUTHENTICATION_TAG);
	return (GF_IPMPX_Data *) ptr;
}
static void DelGF_IPMPX_InitAuthentication(GF_IPMPX_Data *_p)
{
	GF_IPMPX_InitAuthentication *p = (GF_IPMPX_InitAuthentication*)_p;
	gf_free(p);
}
static GF_Err ReadGF_IPMPX_InitAuthentication(GF_BitStream *bs, GF_IPMPX_Data *_p, u32 size)
{
	GF_IPMPX_InitAuthentication *p = (GF_IPMPX_InitAuthentication*)_p;
	p->Context = gf_bs_read_int(bs, 32);
	p->AuthType = gf_bs_read_int(bs, 8);
	return GF_OK;
}
static u32 SizeGF_IPMPX_InitAuthentication(GF_IPMPX_Data *_p)
{
	return 5;
}
static GF_Err WriteGF_IPMPX_InitAuthentication(GF_BitStream *bs, GF_IPMPX_Data *_p)
{
	GF_IPMPX_InitAuthentication *p = (GF_IPMPX_InitAuthentication*)_p;
	gf_bs_write_int(bs, p->Context, 32);
	gf_bs_write_int(bs, p->AuthType, 8);
	return GF_OK;
}


static GF_IPMPX_Data *NewGF_IPMPX_MutualAuthentication()
{
	GF_IPMPX_MutualAuthentication *ptr;
	GF_IPMPX_DATA_ALLOC(ptr, GF_IPMPX_MutualAuthentication, GF_IPMPX_MUTUAL_AUTHENTICATION_TAG);
	ptr->candidateAlgorithms = gf_list_new();
	ptr->agreedAlgorithms = gf_list_new();
	ptr->certificates = gf_list_new();
	return (GF_IPMPX_Data *) ptr;
}

static void delete_algo_list(GF_List *algos)
{
	u32 i;
	for (i=0;i<gf_list_count(algos); i++) {
		GF_IPMPX_Authentication *ip_auth = (GF_IPMPX_Authentication *)gf_list_get(algos, i);
		GF_IPMPX_AUTH_Delete(ip_auth);
	}
	gf_list_del(algos);
}
static void DelGF_IPMPX_MutualAuthentication(GF_IPMPX_Data *_p)
{
	GF_IPMPX_MutualAuthentication *p = (GF_IPMPX_MutualAuthentication *)_p;
	delete_algo_list(p->candidateAlgorithms);
	delete_algo_list(p->agreedAlgorithms);
	GF_IPMPX_DELETE_ARRAY(p->AuthenticationData);
	GF_IPMPX_DELETE_ARRAY(p->opaque);
	GF_IPMPX_DELETE_ARRAY(p->authCodes);
	gf_ipmpx_data_del((GF_IPMPX_Data *)p->trustData);
	GF_IPMPX_AUTH_Delete((GF_IPMPX_Authentication*)p->publicKey);
	while (gf_list_count(p->certificates)) {
		GF_IPMPX_ByteArray *ba = (GF_IPMPX_ByteArray *)gf_list_get(p->certificates, 0);
		gf_list_rem(p->certificates, 0);
		GF_IPMPX_DELETE_ARRAY(ba);
	}
	gf_list_del(p->certificates);
	gf_free(p);
}
static GF_Err ReadGF_IPMPX_MutualAuthentication(GF_BitStream *bs, GF_IPMPX_Data *_p, u32 size)
{
	GF_Err e;
	u32 i, count;
	Bool requestNegotiation, successNegotiation, inclAuthenticationData, inclAuthCodes;
	GF_IPMPX_MutualAuthentication *p = (GF_IPMPX_MutualAuthentication *)_p;
	
	requestNegotiation = gf_bs_read_int(bs, 1);
	successNegotiation = gf_bs_read_int(bs, 1);
	p->failedNegotiation =  gf_bs_read_int(bs, 1);
	inclAuthenticationData = gf_bs_read_int(bs, 1);
	inclAuthCodes = gf_bs_read_int(bs, 1);
	gf_bs_read_int(bs, 3);

	if (requestNegotiation) {
		count = gf_bs_read_int(bs, 8);
		for (i=0; i<count; i++) {
			GF_IPMPX_Authentication *auth;
			e = GF_IPMPX_AUTH_Parse(bs, &auth);
			if (e) return e;
			gf_list_add(p->candidateAlgorithms, auth);
		}
	}
	if (successNegotiation) {
		count = gf_bs_read_int(bs, 8);
		for (i=0; i<count; i++) {
			GF_IPMPX_Authentication *auth;
			e = GF_IPMPX_AUTH_Parse(bs, &auth);
			if (e) return e;
			gf_list_add(p->agreedAlgorithms, auth);
		}
	}
	if (inclAuthenticationData) p->AuthenticationData = GF_IPMPX_GetByteArray(bs);
	if (inclAuthCodes) {
		/*unspecified in spec, IM1 uses 8 bits*/
		u32 type = gf_bs_read_int(bs, 8);
		switch (type) {
		case 0x01:
			count = gf_bs_read_int(bs, 8);
			p->certType = gf_bs_read_int(bs, 32);
			for (i=0; i<count; i++) {
				GF_IPMPX_ByteArray *ipd = GF_IPMPX_GetByteArray(bs);
				if (ipd) gf_list_add(p->certificates, ipd);
			}
			break;
		case 0x02:
			e = GF_IPMPX_AUTH_Parse(bs, (GF_IPMPX_Authentication**) &p->publicKey);
			if (e) return e;
			break;
		case 0xFE:
			p->opaque = GF_IPMPX_GetByteArray(bs);
			break;
		default:
			break;
		}
		e = gf_ipmpx_data_parse(bs, (GF_IPMPX_Data **) &p->trustData);
		if (e) return e;
		p->authCodes = GF_IPMPX_GetByteArray(bs);
	}
	return GF_OK;
}
static u32 SizeGF_IPMPX_MutualAuthentication(GF_IPMPX_Data *_p)
{
	u32 size, i, count;
	GF_IPMPX_MutualAuthentication *p = (GF_IPMPX_MutualAuthentication *)_p;
	size = 1;
	count = gf_list_count(p->candidateAlgorithms);
	if (count) {
		size += 1;
		for (i=0; i<count; i++) {
			GF_IPMPX_Authentication *ip_auth = (GF_IPMPX_Authentication *)gf_list_get(p->candidateAlgorithms, i);
			size += GF_IPMPX_AUTH_FullSize(ip_auth);
		}
	}
	count = gf_list_count(p->agreedAlgorithms);
	if (count) {
		size += 1;
		for (i=0; i<count; i++) {
			GF_IPMPX_Authentication *ip_auth = (GF_IPMPX_Authentication *)gf_list_get(p->agreedAlgorithms, i);
			size += GF_IPMPX_AUTH_FullSize(ip_auth);
		}
	}
	if (p->AuthenticationData) size += GF_IPMPX_GetByteArraySize(p->AuthenticationData);
	
	count = gf_list_count(p->certificates);
	if (count || p->opaque || p->publicKey) {
		size += 1;
		/*type 1*/
		if (count) {
			size += 1+4;
			for (i=0; i<count; i++) {
				GF_IPMPX_ByteArray *ba = (GF_IPMPX_ByteArray *)gf_list_get(p->certificates, i);
				size += GF_IPMPX_GetByteArraySize(ba);
			}
		}
		/*type 2*/
		else if (p->publicKey) {
			size += GF_IPMPX_AUTH_FullSize((GF_IPMPX_Authentication*)p->publicKey);
		}
		/*type 0xFE*/
		else if (p->opaque) {
			size += GF_IPMPX_GetByteArraySize(p->opaque);
		}
		size += gf_ipmpx_data_full_size((GF_IPMPX_Data *)p->trustData);
		size += GF_IPMPX_GetByteArraySize(p->authCodes);
	}
	return size;
}
static GF_Err WriteGF_IPMPX_MutualAuthentication(GF_BitStream *bs, GF_IPMPX_Data *_p)
{
	u32 i, count;
	GF_IPMPX_MutualAuthentication *p = (GF_IPMPX_MutualAuthentication *)_p;

	gf_bs_write_int(bs, gf_list_count(p->candidateAlgorithms) ? 1 : 0, 1);
	gf_bs_write_int(bs, gf_list_count(p->agreedAlgorithms) ? 1 : 0, 1);
	gf_bs_write_int(bs, p->failedNegotiation ? 1 : 0, 1);
	gf_bs_write_int(bs, p->AuthenticationData ? 1 : 0, 1);
	if (gf_list_count(p->certificates) || p->opaque || p->publicKey) {
		gf_bs_write_int(bs, 1, 1);
	} else {
		gf_bs_write_int(bs, 0, 1);
	}
	gf_bs_write_int(bs, 0, 3);

	count = gf_list_count(p->candidateAlgorithms);
	if (count) {
		gf_bs_write_int(bs, count, 8);
		for (i=0; i<count; i++) {
			GF_IPMPX_Authentication *ip_auth = (GF_IPMPX_Authentication *)gf_list_get(p->candidateAlgorithms, i);
			WriteGF_IPMPX_AUTH(bs, ip_auth);
		}
	}
	count = gf_list_count(p->agreedAlgorithms);
	if (count) {
		gf_bs_write_int(bs, count, 8);
		for (i=0; i<count; i++) {
			GF_IPMPX_Authentication *ip_auth = (GF_IPMPX_Authentication *)gf_list_get(p->agreedAlgorithms, i);
			WriteGF_IPMPX_AUTH(bs, ip_auth);
		}
	}
	if (p->AuthenticationData) GF_IPMPX_WriteByteArray(bs, p->AuthenticationData);
	
	count = gf_list_count(p->certificates);
	if (count || p->opaque || p->publicKey) {
		/*type 1*/
		if (count) {
			gf_bs_write_int(bs, 0x01, 8);

			gf_bs_write_int(bs, count, 8);
			gf_bs_write_int(bs, p->certType, 32);
			for (i=0; i<count; i++) {
				GF_IPMPX_ByteArray *ipd = (GF_IPMPX_ByteArray *)gf_list_get(p->certificates, i);
				if (ipd) GF_IPMPX_WriteByteArray(bs, ipd);
			}
		}
		/*type 2*/
		else if (p->publicKey) {
			gf_bs_write_int(bs, 0x02, 8);
			WriteGF_IPMPX_AUTH(bs, (GF_IPMPX_Authentication *) p->publicKey);
		}
		/*type 0xFE*/
		else if (p->opaque) {
			gf_bs_write_int(bs, 0xFE, 8);
			GF_IPMPX_WriteByteArray(bs, p->opaque);
		}
		gf_ipmpx_data_write(bs, (GF_IPMPX_Data *)p->trustData);
		GF_IPMPX_WriteByteArray(bs, p->authCodes);
	}
	return GF_OK;
}

static GF_IPMPX_Data *NewGF_IPMPX_TrustSecurityMetadata()
{
	GF_IPMPX_TrustSecurityMetadata *ptr;
	GF_IPMPX_DATA_ALLOC(ptr, GF_IPMPX_TrustSecurityMetadata, GF_IPMPX_TRUST_SECURITY_METADATA_TAG);
	ptr->TrustedTools = gf_list_new();
	return (GF_IPMPX_Data *) ptr;
}
static void DelGF_IPMPX_TrustSecurityMetadata(GF_IPMPX_Data *_p)
{
	GF_IPMPX_TrustSecurityMetadata *p = (GF_IPMPX_TrustSecurityMetadata *)_p;
	while (gf_list_count(p->TrustedTools)) {
		GF_IPMPX_TrustedTool *tt = (GF_IPMPX_TrustedTool *)gf_list_get(p->TrustedTools, 0);
		gf_list_rem(p->TrustedTools, 0);
		while (gf_list_count(tt->trustSpecifications)) {
			GF_IPMPX_TrustSpecification *tts = (GF_IPMPX_TrustSpecification *)gf_list_get(tt->trustSpecifications, 0);
			gf_list_rem(tt->trustSpecifications, 0);
			GF_IPMPX_DELETE_ARRAY(tts->CCTrustMetadata);
			gf_free(tts);
		}
		gf_list_del(tt->trustSpecifications);
		gf_free(tt);
	}
	gf_list_del(p->TrustedTools);
	gf_free(p);
}
static GF_Err ReadGF_IPMPX_TrustSecurityMetadata(GF_BitStream *bs, GF_IPMPX_Data *_p, u32 size)
{
	u32 nbTools, nbSpec;
	GF_IPMPX_TrustSecurityMetadata *p = (GF_IPMPX_TrustSecurityMetadata *)_p;
	nbTools = gf_bs_read_int(bs, 16);
	while (nbTools) {
		GF_IPMPX_TrustedTool *tt = (GF_IPMPX_TrustedTool *)gf_malloc(sizeof(GF_IPMPX_TrustedTool));
		if (!tt) return GF_OUT_OF_MEM;
		memset(tt, 0, sizeof(GF_IPMPX_TrustedTool));
		tt->tag = GF_IPMPX_TRUSTED_TOOL_TAG;
		nbTools--;
		gf_list_add(p->TrustedTools, tt);
		gf_bs_read_data(bs, (char*)tt->toolID, 16);
		gf_bs_read_data(bs, tt->AuditDate, 5);
		tt->trustSpecifications = gf_list_new();
		nbSpec = gf_bs_read_int(bs, 16);
		while (nbSpec) {
			Bool has_cc;
			GF_IPMPX_TrustSpecification *tts = (GF_IPMPX_TrustSpecification *)gf_malloc(sizeof(GF_IPMPX_TrustSpecification));
			if (!tts) return GF_OUT_OF_MEM;
			memset(tts, 0, sizeof(GF_IPMPX_TrustSpecification));
			tts->tag = GF_IPMPX_TRUST_SPECIFICATION_TAG;
			nbSpec--;
			gf_list_add(tt->trustSpecifications, tts);
			has_cc = gf_bs_read_int(bs, 1);
			gf_bs_read_int(bs, 7);
			if (has_cc) {
				tts->CCTrustMetadata = GF_IPMPX_GetByteArray(bs);
			} else {
				gf_bs_read_data(bs, tts->startDate, 5);
				tts->attackerProfile = gf_bs_read_int(bs, 2);
				gf_bs_read_int(bs, 6);
				tts->trustedDuration = gf_bs_read_int(bs, 32);
			}
		}
	}
	return GF_OK;
}
static u32 SizeGF_IPMPX_TrustSecurityMetadata(GF_IPMPX_Data *_p)
{
	u32 size, i, j;
	GF_IPMPX_TrustSecurityMetadata *p = (GF_IPMPX_TrustSecurityMetadata *)_p;
	size = 2;
	for (i=0;i<gf_list_count(p->TrustedTools); i++) {
		GF_IPMPX_TrustedTool *tt = (GF_IPMPX_TrustedTool *)gf_list_get(p->TrustedTools, i);
		size += 23;
		for (j=0; j<gf_list_count(tt->trustSpecifications); j++) {
			GF_IPMPX_TrustSpecification *tts = (GF_IPMPX_TrustSpecification *)gf_list_get(tt->trustSpecifications, j);
			size += 1;
			if (tts->CCTrustMetadata) size += GF_IPMPX_GetByteArraySize(tts->CCTrustMetadata);
			else size += 10;
		}
	}
	return size;
}
static GF_Err WriteGF_IPMPX_TrustSecurityMetadata(GF_BitStream *bs, GF_IPMPX_Data *_p)
{
	u32 i, j, c1, c2;
	GF_IPMPX_TrustSecurityMetadata *p = (GF_IPMPX_TrustSecurityMetadata *)_p;

	c1 = gf_list_count(p->TrustedTools);
	gf_bs_write_int(bs, c1, 16);
	for (i=0;i<c1; i++) {
		GF_IPMPX_TrustedTool *tt = (GF_IPMPX_TrustedTool *)gf_list_get(p->TrustedTools, i);
		gf_bs_write_data(bs, (char*)tt->toolID, 16);
		gf_bs_write_data(bs, (char*)tt->AuditDate, 5);
		c2 = gf_list_count(tt->trustSpecifications);
		gf_bs_write_int(bs, c2, 16);

		for (j=0; j<c2; j++) {
			GF_IPMPX_TrustSpecification *tts = (GF_IPMPX_TrustSpecification *)gf_list_get(tt->trustSpecifications, j);
			gf_bs_write_int(bs, tts->CCTrustMetadata ? 1 : 0, 1);
			gf_bs_write_int(bs, 0, 7);
			if (tts->CCTrustMetadata) GF_IPMPX_WriteByteArray(bs, tts->CCTrustMetadata);
			else {
				gf_bs_write_data(bs, tts->startDate, 5);
				gf_bs_write_int(bs, tts->attackerProfile, 2);
				gf_bs_write_int(bs, 0, 6);
				gf_bs_write_int(bs, tts->trustedDuration, 32);
			}
		}
	}
	return GF_OK;
}

static GF_IPMPX_Data *NewGF_IPMPX_SecureContainer()
{
	GF_IPMPX_SecureContainer *ptr;
	GF_IPMPX_DATA_ALLOC(ptr, GF_IPMPX_SecureContainer, GF_IPMPX_SECURE_CONTAINER_TAG);
	return (GF_IPMPX_Data *) ptr;
}
static void DelGF_IPMPX_SecureContainer(GF_IPMPX_Data *_p)
{
	GF_IPMPX_SecureContainer *p = (GF_IPMPX_SecureContainer *)_p;
	GF_IPMPX_DELETE_ARRAY(p->encryptedData);
	GF_IPMPX_DELETE_ARRAY(p->MAC);
	gf_ipmpx_data_del(p->protectedMsg);
	gf_free(p);
}
static GF_Err ReadGF_IPMPX_SecureContainer(GF_BitStream *bs, GF_IPMPX_Data *_p, u32 size)
{
	Bool has_enc, has_mac;
	GF_IPMPX_SecureContainer *p = (GF_IPMPX_SecureContainer *)_p;
	has_enc = gf_bs_read_int(bs, 1);
	has_mac = gf_bs_read_int(bs, 1);
	p->isMACEncrypted = gf_bs_read_int(bs, 1);
	gf_bs_read_int(bs, 5);
	if (has_enc) {
		p->encryptedData = GF_IPMPX_GetByteArray(bs);
		if (has_mac && !p->isMACEncrypted) p->MAC = GF_IPMPX_GetByteArray(bs);
	} else {
		GF_Err e = gf_ipmpx_data_parse(bs, &p->protectedMsg);
		if (e) return e;
		if (has_mac) p->MAC = GF_IPMPX_GetByteArray(bs);
	}
	return GF_OK;
}
static u32 SizeGF_IPMPX_SecureContainer(GF_IPMPX_Data *_p)
{
	u32 size = 1;
	GF_IPMPX_SecureContainer *p = (GF_IPMPX_SecureContainer *)_p;
	if (p->MAC) p->isMACEncrypted = 0;
	if (p->encryptedData) {
		size += GF_IPMPX_GetByteArraySize(p->encryptedData);
		if (p->MAC) size += GF_IPMPX_GetByteArraySize(p->MAC);
	} else {
		size += gf_ipmpx_data_full_size(p->protectedMsg);
		if (p->MAC) size += GF_IPMPX_GetByteArraySize(p->MAC);
	}
	return size;
}
static GF_Err WriteGF_IPMPX_SecureContainer(GF_BitStream *bs, GF_IPMPX_Data *_p)
{
	GF_IPMPX_SecureContainer *p = (GF_IPMPX_SecureContainer *)_p;
	if (p->MAC) p->isMACEncrypted = 0;
	gf_bs_write_int(bs, p->encryptedData ? 1 : 0, 1);
	gf_bs_write_int(bs, (p->MAC || p->isMACEncrypted) ? 1 : 0, 1);
	gf_bs_write_int(bs, p->isMACEncrypted, 1);
	gf_bs_write_int(bs, 0, 5);
	if (p->encryptedData) {
		GF_IPMPX_WriteByteArray(bs, p->encryptedData);
		if (p->MAC) GF_IPMPX_WriteByteArray(bs, p->MAC);
	} else {
		GF_Err e = gf_ipmpx_data_write(bs, p->protectedMsg);
		if (e) return e;
		if (p->MAC) GF_IPMPX_WriteByteArray(bs, p->MAC);
	}
	return GF_OK;
}

static GF_IPMPX_Data *NewGF_IPMPX_GetToolsResponse()
{
	GF_IPMPX_GetToolsResponse *ptr;
	GF_IPMPX_DATA_ALLOC(ptr, GF_IPMPX_GetToolsResponse, GF_IPMPX_GET_TOOLS_RESPONSE_TAG);
	ptr->ipmp_tools = gf_list_new();
	return (GF_IPMPX_Data *) ptr;
}
static void DelGF_IPMPX_GetToolsResponse(GF_IPMPX_Data *_p)
{
	GF_IPMPX_GetToolsResponse *p = (GF_IPMPX_GetToolsResponse *)_p;
	while (gf_list_count(p->ipmp_tools)) {
		/*IPMPTools are descriptors*/
		GF_Descriptor *d = (GF_Descriptor *)gf_list_get(p->ipmp_tools, 0);
		gf_list_rem(p->ipmp_tools, 0);
		gf_odf_desc_del((GF_Descriptor*)d);
	}
	gf_list_del(p->ipmp_tools);
	gf_free(p);
}
static GF_Err ReadGF_IPMPX_GetToolsResponse(GF_BitStream *bs, GF_IPMPX_Data *_p, u32 size)
{
	u32 NbBytes = 0;
	GF_IPMPX_GetToolsResponse *p = (GF_IPMPX_GetToolsResponse *)_p;

	while (size>NbBytes) {
		u32 desc_size, start_o;
		GF_Descriptor *desc;
		GF_Err e;
		start_o = (u32) gf_bs_get_position(bs);
		e = gf_odf_parse_descriptor(bs, &desc, &desc_size);
		if (e) return e;
		gf_list_add(p->ipmp_tools, desc);
		NbBytes += (u32) gf_bs_get_position(bs) - start_o;
	}
	if (size<NbBytes) return GF_NON_COMPLIANT_BITSTREAM;
	return GF_OK;
}
static u32 SizeGF_IPMPX_GetToolsResponse(GF_IPMPX_Data *_p)
{
	u32 size = 0;
	GF_IPMPX_GetToolsResponse *p = (GF_IPMPX_GetToolsResponse *)_p;
	gf_odf_size_descriptor_list(p->ipmp_tools, &size);
	return size;
}
static GF_Err WriteGF_IPMPX_GetToolsResponse(GF_BitStream *bs, GF_IPMPX_Data *_p)
{
	u32 i;
	GF_IPMPX_GetToolsResponse *p = (GF_IPMPX_GetToolsResponse *)_p;

	for (i=0; i<gf_list_count(p->ipmp_tools); i++) {
		GF_Descriptor *desc = (GF_Descriptor *)gf_list_get(p->ipmp_tools, i);
		gf_odf_write_descriptor(bs, desc);
	}
	return GF_OK;
}
static GF_IPMPX_Data *NewGF_IPMPX_ParametricDescription()
{
	GF_IPMPX_ParametricDescription *ptr;
	GF_IPMPX_DATA_ALLOC(ptr, GF_IPMPX_ParametricDescription, GF_IPMPX_PARAMETRIC_DESCRIPTION_TAG);
	ptr->descriptions = gf_list_new();
	return (GF_IPMPX_Data *) ptr;
}
static void DelGF_IPMPX_ParametricDescription(GF_IPMPX_Data *_p)
{
	GF_IPMPX_ParametricDescription *p = (GF_IPMPX_ParametricDescription *)_p;
	GF_IPMPX_DELETE_ARRAY(p->descriptionComment);
	while (gf_list_count(p->descriptions)) {
		GF_IPMPX_ParametricDescriptionItem *it = (GF_IPMPX_ParametricDescriptionItem *)gf_list_get(p->descriptions, 0);
		gf_list_rem(p->descriptions, 0);
		GF_IPMPX_DELETE_ARRAY(it->main_class);
		GF_IPMPX_DELETE_ARRAY(it->subClass);
		GF_IPMPX_DELETE_ARRAY(it->typeData);
		GF_IPMPX_DELETE_ARRAY(it->type);
		GF_IPMPX_DELETE_ARRAY(it->addedData);
		gf_free(it);
	}
	gf_list_del(p->descriptions);
	gf_free(p);
}
static GF_Err ReadGF_IPMPX_ParametricDescription(GF_BitStream *bs, GF_IPMPX_Data *_p, u32 size)
{
	u32 count;
	GF_IPMPX_ParametricDescription *p = (GF_IPMPX_ParametricDescription *)_p;
	p->descriptionComment = GF_IPMPX_GetByteArray(bs);
	p->majorVersion = gf_bs_read_int(bs, 8);
	p->minorVersion = gf_bs_read_int(bs, 8);
	count = gf_bs_read_int(bs, 32);
	while (count) {
		GF_IPMPX_ParametricDescriptionItem *it = (GF_IPMPX_ParametricDescriptionItem *)gf_malloc(sizeof(GF_IPMPX_ParametricDescriptionItem));
		gf_list_add(p->descriptions, it);
		count--;
		it->main_class = GF_IPMPX_GetByteArray(bs);
		it->subClass = GF_IPMPX_GetByteArray(bs);
		it->typeData = GF_IPMPX_GetByteArray(bs);
		it->type = GF_IPMPX_GetByteArray(bs);
		it->addedData = GF_IPMPX_GetByteArray(bs);
	}
	return GF_OK;
}
static u32 SizeGF_IPMPX_ParametricDescription(GF_IPMPX_Data *_p)
{
	u32 size, i;
	GF_IPMPX_ParametricDescription *p = (GF_IPMPX_ParametricDescription *)_p;
	size = GF_IPMPX_GetByteArraySize(p->descriptionComment);
	size += 6;
	for (i=0; i<gf_list_count(p->descriptions); i++) {
		GF_IPMPX_ParametricDescriptionItem *it = (GF_IPMPX_ParametricDescriptionItem *)gf_list_get(p->descriptions, i);
		size += GF_IPMPX_GetByteArraySize(it->main_class);
		size += GF_IPMPX_GetByteArraySize(it->subClass);
		size += GF_IPMPX_GetByteArraySize(it->typeData);
		size += GF_IPMPX_GetByteArraySize(it->type);
		size += GF_IPMPX_GetByteArraySize(it->addedData);
	}
	return size;
}
static GF_Err WriteGF_IPMPX_ParametricDescription(GF_BitStream *bs, GF_IPMPX_Data *_p)
{
	u32 i;
	GF_IPMPX_ParametricDescription *p = (GF_IPMPX_ParametricDescription *)_p;
	GF_IPMPX_WriteByteArray(bs, p->descriptionComment);
	gf_bs_write_int(bs, p->majorVersion, 8);
	gf_bs_write_int(bs, p->minorVersion, 8);
	gf_bs_write_int(bs, gf_list_count(p->descriptions), 32);

	for (i=0; i<gf_list_count(p->descriptions); i++) {
		GF_IPMPX_ParametricDescriptionItem *it = (GF_IPMPX_ParametricDescriptionItem *)gf_list_get(p->descriptions, i);
		GF_IPMPX_WriteByteArray(bs, it->main_class);
		GF_IPMPX_WriteByteArray(bs, it->subClass);
		GF_IPMPX_WriteByteArray(bs, it->typeData);
		GF_IPMPX_WriteByteArray(bs, it->type);
		GF_IPMPX_WriteByteArray(bs, it->addedData);
	}
	return GF_OK;
}
static GF_IPMPX_Data *NewGF_IPMPX_ToolParamCapabilitiesQuery()
{
	GF_IPMPX_ToolParamCapabilitiesQuery*ptr;
	GF_IPMPX_DATA_ALLOC(ptr, GF_IPMPX_ToolParamCapabilitiesQuery, GF_IPMPX_PARAMETRIC_CAPS_QUERY_TAG);
	return (GF_IPMPX_Data *) ptr;
}
static void DelGF_IPMPX_ToolParamCapabilitiesQuery(GF_IPMPX_Data *_p)
{
	GF_IPMPX_ToolParamCapabilitiesQuery *p = (GF_IPMPX_ToolParamCapabilitiesQuery *)_p;
	gf_ipmpx_data_del((GF_IPMPX_Data *) p->description);
	gf_free(p);
}
static GF_Err ReadGF_IPMPX_ToolParamCapabilitiesQuery(GF_BitStream *bs, GF_IPMPX_Data *_p, u32 size)
{
	GF_IPMPX_ToolParamCapabilitiesQuery *p = (GF_IPMPX_ToolParamCapabilitiesQuery*)_p;
	return gf_ipmpx_data_parse(bs, (GF_IPMPX_Data **) &p->description);
}
static u32 SizeGF_IPMPX_ToolParamCapabilitiesQuery(GF_IPMPX_Data *_p)
{
	GF_IPMPX_ToolParamCapabilitiesQuery *p = (GF_IPMPX_ToolParamCapabilitiesQuery*)_p;
	return gf_ipmpx_data_full_size((GF_IPMPX_Data *) p->description);
}
static GF_Err WriteGF_IPMPX_ToolParamCapabilitiesQuery(GF_BitStream *bs, GF_IPMPX_Data *_p)
{
	GF_IPMPX_ToolParamCapabilitiesQuery *p = (GF_IPMPX_ToolParamCapabilitiesQuery*)_p;
	return gf_ipmpx_data_write(bs, (GF_IPMPX_Data *) p->description);
}
static GF_IPMPX_Data *NewGF_IPMPX_ToolParamCapabilitiesResponse()
{
	GF_IPMPX_ToolParamCapabilitiesResponse*ptr;
	GF_IPMPX_DATA_ALLOC(ptr, GF_IPMPX_ToolParamCapabilitiesResponse, GF_IPMPX_PARAMETRIC_CAPS_RESPONSE_TAG);
	return (GF_IPMPX_Data *) ptr;
}
static void DelGF_IPMPX_ToolParamCapabilitiesResponse(GF_IPMPX_Data *_p)
{
	gf_free(_p);
}
static GF_Err ReadGF_IPMPX_ToolParamCapabilitiesResponse(GF_BitStream *bs, GF_IPMPX_Data *_p, u32 size)
{
	GF_IPMPX_ToolParamCapabilitiesResponse *p = (GF_IPMPX_ToolParamCapabilitiesResponse*)_p;
	p->capabilitiesSupported = gf_bs_read_int(bs, 1);
	gf_bs_read_int(bs, 7);
	return GF_OK;
}
static u32 SizeGF_IPMPX_ToolParamCapabilitiesResponse(GF_IPMPX_Data *_p)
{
	return 1;
}
static GF_Err WriteGF_IPMPX_ToolParamCapabilitiesResponse(GF_BitStream *bs, GF_IPMPX_Data *_p)
{
	GF_IPMPX_ToolParamCapabilitiesResponse*p = (GF_IPMPX_ToolParamCapabilitiesResponse*)_p;
	gf_bs_write_int(bs, p->capabilitiesSupported, 1);
	gf_bs_write_int(bs, 0, 7);
	return GF_OK;
}

static GF_IPMPX_Data *NewGF_IPMPX_ConnectTool()
{
	GF_IPMPX_ConnectTool*ptr;
	GF_IPMPX_DATA_ALLOC(ptr, GF_IPMPX_ConnectTool, GF_IPMPX_CONNECT_TOOL_TAG);
	return (GF_IPMPX_Data *) ptr;
}
static void DelGF_IPMPX_ConnectTool(GF_IPMPX_Data *_p)
{
	GF_IPMPX_ConnectTool *p = (GF_IPMPX_ConnectTool*)_p;
	if (p->toolDescriptor) gf_odf_desc_del((GF_Descriptor *)p->toolDescriptor);
	gf_free(p);
}
static GF_Err ReadGF_IPMPX_ConnectTool(GF_BitStream *bs, GF_IPMPX_Data *_p, u32 size)
{
	u32 dsize;
	GF_IPMPX_ConnectTool*p = (GF_IPMPX_ConnectTool*)_p;
	return gf_odf_parse_descriptor(bs, (GF_Descriptor **) &p->toolDescriptor, &dsize);
}
static u32 SizeGF_IPMPX_ConnectTool(GF_IPMPX_Data *_p)
{
	u32 size;
	GF_IPMPX_ConnectTool*p = (GF_IPMPX_ConnectTool*)_p;
	gf_odf_size_descriptor((GF_Descriptor *)p->toolDescriptor, &size);
	size += gf_odf_size_field_size(size);
	return size;
}
static GF_Err WriteGF_IPMPX_ConnectTool(GF_BitStream *bs, GF_IPMPX_Data *_p)
{
	GF_IPMPX_ConnectTool*p = (GF_IPMPX_ConnectTool*)_p;
	gf_odf_write_descriptor(bs, (GF_Descriptor *)p->toolDescriptor);
	return GF_OK;
}
static GF_IPMPX_Data *NewGF_IPMPX_DisconnectTool()
{
	GF_IPMPX_DisconnectTool*ptr;
	GF_IPMPX_DATA_ALLOC(ptr, GF_IPMPX_DisconnectTool, GF_IPMPX_DISCONNECT_TOOL_TAG);
	return (GF_IPMPX_Data *) ptr;
}
static void DelGF_IPMPX_DisconnectTool(GF_IPMPX_Data *_p)
{
	gf_free(_p);
}
static GF_Err ReadGF_IPMPX_DisconnectTool(GF_BitStream *bs, GF_IPMPX_Data *_p, u32 size)
{
	GF_IPMPX_DisconnectTool*p = (GF_IPMPX_DisconnectTool*)_p;
	p->IPMP_ToolContextID = gf_bs_read_int(bs, 32);
	return GF_OK;
}
static u32 SizeGF_IPMPX_DisconnectTool(GF_IPMPX_Data *_p)
{
	return 4;
}
static GF_Err WriteGF_IPMPX_DisconnectTool(GF_BitStream *bs, GF_IPMPX_Data *_p)
{
	GF_IPMPX_DisconnectTool*p = (GF_IPMPX_DisconnectTool*)_p;
	gf_bs_write_int(bs, p->IPMP_ToolContextID, 32);
	return GF_OK;
}

static GF_IPMPX_Data *NewGF_IPMPX_GetToolContext()
{
	GF_IPMPX_GetToolContext*ptr;
	GF_IPMPX_DATA_ALLOC(ptr, GF_IPMPX_GetToolContext, GF_IPMPX_GET_TOOL_CONTEXT_TAG);
	return (GF_IPMPX_Data *) ptr;
}
static void DelGF_IPMPX_GetToolContext(GF_IPMPX_Data *_p)
{
	gf_free(_p);
}
static GF_Err ReadGF_IPMPX_GetToolContext(GF_BitStream *bs, GF_IPMPX_Data *_p, u32 size)
{
	Bool has_idex;
	GF_IPMPX_GetToolContext*p = (GF_IPMPX_GetToolContext*)_p;
	p->scope = gf_bs_read_int(bs, 3);
	has_idex = gf_bs_read_int(bs, 1);
	gf_bs_read_int(bs, 4);
	if (has_idex) p->IPMP_DescriptorIDEx = gf_bs_read_int(bs, 16);

	return GF_OK;
}
static u32 SizeGF_IPMPX_GetToolContext(GF_IPMPX_Data *_p)
{
	GF_IPMPX_GetToolContext*p = (GF_IPMPX_GetToolContext*)_p;
	return p->IPMP_DescriptorIDEx ? 3 : 1;
}
static GF_Err WriteGF_IPMPX_GetToolContext(GF_BitStream *bs, GF_IPMPX_Data *_p)
{
	GF_IPMPX_GetToolContext*p = (GF_IPMPX_GetToolContext*)_p;
	gf_bs_write_int(bs, p->scope, 3);
	gf_bs_write_int(bs, p->IPMP_DescriptorIDEx ? 1 : 0, 1);
	gf_bs_write_int(bs, 0, 4);
	if (p->IPMP_DescriptorIDEx) gf_bs_write_int(bs, p->IPMP_DescriptorIDEx, 16);
	return GF_OK;
}
static GF_IPMPX_Data *NewGF_IPMPX_GetToolContextResponse()
{
	GF_IPMPX_GetToolContextResponse*ptr;
	GF_IPMPX_DATA_ALLOC(ptr, GF_IPMPX_GetToolContextResponse, GF_IPMPX_GET_TOOL_CONTEXT_RESPONSE_TAG);
	return (GF_IPMPX_Data *) ptr;
}
static void DelGF_IPMPX_GetToolContextResponse(GF_IPMPX_Data *_p)
{
	gf_free(_p);
}
static GF_Err ReadGF_IPMPX_GetToolContextResponse(GF_BitStream *bs, GF_IPMPX_Data *_p, u32 size)
{
	Bool has_esid;
	GF_IPMPX_GetToolContextResponse*p = (GF_IPMPX_GetToolContextResponse*)_p;
	has_esid = gf_bs_read_int(bs, 1);
	gf_bs_read_int(bs, 5);
	p->OD_ID = gf_bs_read_int(bs, 10);
	if (has_esid) p->ESD_ID = gf_bs_read_int(bs, 16);
	p->IPMP_ToolContextID = gf_bs_read_int(bs, 32);
	return GF_OK;
}
static u32 SizeGF_IPMPX_GetToolContextResponse(GF_IPMPX_Data *_p)
{
	GF_IPMPX_GetToolContextResponse*p = (GF_IPMPX_GetToolContextResponse*)_p;
	return p->ESD_ID ? 8 : 6;
}
static GF_Err WriteGF_IPMPX_GetToolContextResponse(GF_BitStream *bs, GF_IPMPX_Data *_p)
{
	GF_IPMPX_GetToolContextResponse*p = (GF_IPMPX_GetToolContextResponse*)_p;
	gf_bs_write_int(bs, p->ESD_ID ? 1 : 0, 1);
	gf_bs_write_int(bs, 0, 5);
	gf_bs_write_int(bs, p->OD_ID, 10);
	if (p->ESD_ID) gf_bs_write_int(bs, p->ESD_ID, 16);
	gf_bs_write_int(bs, p->IPMP_ToolContextID, 32);
	return GF_OK;
}


static GF_IPMPX_Data *NewGF_IPMPX_AddToolNotificationListener()
{
	GF_IPMPX_AddToolNotificationListener*ptr;
	GF_IPMPX_DATA_ALLOC(ptr, GF_IPMPX_AddToolNotificationListener, GF_IPMPX_ADD_TOOL_LISTENER_TAG);
	return (GF_IPMPX_Data *) ptr;
}
static void DelGF_IPMPX_AddToolNotificationListener(GF_IPMPX_Data *_p)
{
	gf_free(_p);
}
static GF_Err ReadGF_IPMPX_AddToolNotificationListener(GF_BitStream *bs, GF_IPMPX_Data *_p, u32 size)
{
	u32 i;
	GF_IPMPX_AddToolNotificationListener*p = (GF_IPMPX_AddToolNotificationListener*)_p;
	p->scope = gf_bs_read_int(bs, 3);
	gf_bs_read_int(bs, 5);
	p->eventTypeCount = gf_bs_read_int(bs, 8);
	for (i=0;i<p->eventTypeCount; i++) p->eventType[i] = gf_bs_read_int(bs, 8);
	return GF_OK;
}
static u32 SizeGF_IPMPX_AddToolNotificationListener(GF_IPMPX_Data *_p)
{
	GF_IPMPX_AddToolNotificationListener*p = (GF_IPMPX_AddToolNotificationListener*)_p;
	return p->eventTypeCount + 2;
}
static GF_Err WriteGF_IPMPX_AddToolNotificationListener(GF_BitStream *bs, GF_IPMPX_Data *_p)
{
	u32 i;
	GF_IPMPX_AddToolNotificationListener*p = (GF_IPMPX_AddToolNotificationListener*)_p;
	gf_bs_write_int(bs, p->scope, 3);
	gf_bs_write_int(bs, 0, 5);
	gf_bs_write_int(bs, p->eventTypeCount, 8);
	for (i=0;i<p->eventTypeCount; i++) gf_bs_write_int(bs, p->eventType[i], 8);
	return GF_OK;
}
static GF_IPMPX_Data *NewGF_IPMPX_RemoveToolNotificationListener()
{
	GF_IPMPX_RemoveToolNotificationListener*ptr;
	GF_IPMPX_DATA_ALLOC(ptr, GF_IPMPX_RemoveToolNotificationListener, GF_IPMPX_REMOVE_TOOL_LISTENER_TAG);
	return (GF_IPMPX_Data *) ptr;
}
static void DelGF_IPMPX_RemoveToolNotificationListener(GF_IPMPX_Data *_p)
{
	gf_free(_p);
}
static GF_Err ReadGF_IPMPX_RemoveToolNotificationListener(GF_BitStream *bs, GF_IPMPX_Data *_p, u32 size)
{
	u32 i;
	GF_IPMPX_RemoveToolNotificationListener*p = (GF_IPMPX_RemoveToolNotificationListener*)_p;
	p->eventTypeCount = gf_bs_read_int(bs, 8);
	for (i=0;i<p->eventTypeCount; i++) p->eventType[i] = gf_bs_read_int(bs, 8);
	return GF_OK;
}
static u32 SizeGF_IPMPX_RemoveToolNotificationListener(GF_IPMPX_Data *_p)
{
	GF_IPMPX_RemoveToolNotificationListener*p = (GF_IPMPX_RemoveToolNotificationListener*)_p;
	return p->eventTypeCount + 1;
}
static GF_Err WriteGF_IPMPX_RemoveToolNotificationListener(GF_BitStream *bs, GF_IPMPX_Data *_p)
{
	u32 i;
	GF_IPMPX_RemoveToolNotificationListener*p = (GF_IPMPX_RemoveToolNotificationListener*)_p;
	gf_bs_write_int(bs, p->eventTypeCount, 8);
	for (i=0;i<p->eventTypeCount; i++) gf_bs_write_int(bs, p->eventType[i], 8);
	return GF_OK;
}

static GF_IPMPX_Data *NewGF_IPMPX_NotifyToolEvent()
{
	GF_IPMPX_NotifyToolEvent*ptr;
	GF_IPMPX_DATA_ALLOC(ptr, GF_IPMPX_NotifyToolEvent, GF_IPMPX_NOTIFY_TOOL_EVENT_TAG);
	return (GF_IPMPX_Data *) ptr;
}
static void DelGF_IPMPX_NotifyToolEvent(GF_IPMPX_Data *_p)
{
	gf_free(_p);
}
static GF_Err ReadGF_IPMPX_NotifyToolEvent(GF_BitStream *bs, GF_IPMPX_Data *_p, u32 size)
{
	Bool has_id;
	GF_IPMPX_NotifyToolEvent*p = (GF_IPMPX_NotifyToolEvent*)_p;
	has_id = gf_bs_read_int(bs, 1);
	gf_bs_read_int(bs, 7);
	if (has_id) {
		p->OD_ID = gf_bs_read_int(bs, 10);
		gf_bs_read_int(bs, 6);
		p->ESD_ID = gf_bs_read_int(bs, 16);
	}
	p->eventType = gf_bs_read_int(bs, 8);
	p->IPMP_ToolContextID = gf_bs_read_int(bs, 32);
	return GF_OK;
}
static u32 SizeGF_IPMPX_NotifyToolEvent(GF_IPMPX_Data *_p)
{
	GF_IPMPX_NotifyToolEvent*p = (GF_IPMPX_NotifyToolEvent*)_p;
	return (p->OD_ID || p->ESD_ID) ? 10 : 6;
}
static GF_Err WriteGF_IPMPX_NotifyToolEvent(GF_BitStream *bs, GF_IPMPX_Data *_p)
{
	GF_IPMPX_NotifyToolEvent*p = (GF_IPMPX_NotifyToolEvent*)_p;
	gf_bs_write_int(bs, (p->OD_ID || p->ESD_ID) ? 1 : 0, 1);
	gf_bs_write_int(bs, 0, 7);
	if (p->OD_ID || p->ESD_ID) {
		gf_bs_write_int(bs, p->OD_ID, 10);
		gf_bs_write_int(bs, 0, 6);
		gf_bs_write_int(bs, p->ESD_ID, 16);
	}
	gf_bs_write_int(bs, p->eventType, 8);
	gf_bs_write_int(bs, p->IPMP_ToolContextID, 32);
	return GF_OK;
}
static GF_IPMPX_Data *NewGF_IPMPX_CanProcess()
{
	GF_IPMPX_CanProcess*ptr;
	GF_IPMPX_DATA_ALLOC(ptr, GF_IPMPX_CanProcess, GF_IPMPX_CAN_PROCESS_TAG);
	return (GF_IPMPX_Data *) ptr;
}
static void DelGF_IPMPX_CanProcess(GF_IPMPX_Data *_p)
{
	gf_free(_p);
}
static GF_Err ReadGF_IPMPX_CanProcess(GF_BitStream *bs, GF_IPMPX_Data *_p, u32 size)
{
	GF_IPMPX_CanProcess*p = (GF_IPMPX_CanProcess*)_p;
	p->canProcess = gf_bs_read_int(bs, 1);
	gf_bs_read_int(bs, 7);
	return GF_OK;
}
static u32 SizeGF_IPMPX_CanProcess(GF_IPMPX_Data *_p)
{
	return 1;
}
static GF_Err WriteGF_IPMPX_CanProcess(GF_BitStream *bs, GF_IPMPX_Data *_p)
{
	GF_IPMPX_CanProcess*p = (GF_IPMPX_CanProcess*)_p;
	gf_bs_write_int(bs, p->canProcess ? 1 : 0, 1);
	gf_bs_write_int(bs, 0, 7);
	return GF_OK;
}
static GF_IPMPX_Data *NewGF_IPMPX_OpaqueData(u8 tag)
{
	GF_IPMPX_OpaqueData*ptr;
	GF_IPMPX_DATA_ALLOC(ptr, GF_IPMPX_OpaqueData, tag);
	return (GF_IPMPX_Data *) ptr;
}
static void DelGF_IPMPX_OpaqueData(GF_IPMPX_Data *_p)
{
	GF_IPMPX_OpaqueData *p = (GF_IPMPX_OpaqueData*)_p;
	GF_IPMPX_DELETE_ARRAY(p->opaqueData);
	gf_free(p);
}
static GF_Err ReadGF_IPMPX_OpaqueData(GF_BitStream *bs, GF_IPMPX_Data *_p, u32 size)
{
	GF_IPMPX_OpaqueData*p = (GF_IPMPX_OpaqueData*)_p;
	p->opaqueData = GF_IPMPX_GetByteArray(bs);
	return GF_OK;
}
static u32 SizeGF_IPMPX_OpaqueData(GF_IPMPX_Data *_p)
{
	GF_IPMPX_OpaqueData*p = (GF_IPMPX_OpaqueData*)_p;
	return GF_IPMPX_GetByteArraySize(p->opaqueData);
}
static GF_Err WriteGF_IPMPX_OpaqueData(GF_BitStream *bs, GF_IPMPX_Data *_p)
{
	GF_IPMPX_OpaqueData*p = (GF_IPMPX_OpaqueData*)_p;
	GF_IPMPX_WriteByteArray(bs, p->opaqueData);
	return GF_OK;
}
static GF_IPMPX_Data *NewGF_IPMPX_KeyData()
{
	GF_IPMPX_KeyData*ptr;
	GF_IPMPX_DATA_ALLOC(ptr, GF_IPMPX_KeyData, GF_IPMPX_KEY_DATA_TAG);
	return (GF_IPMPX_Data *) ptr;
}
static void DelGF_IPMPX_KeyData(GF_IPMPX_Data *_p)
{
	GF_IPMPX_KeyData*p = (GF_IPMPX_KeyData*)_p;
	GF_IPMPX_DELETE_ARRAY(p->keyBody);
	GF_IPMPX_DELETE_ARRAY(p->OpaqueData);
	gf_free(p);
}
static GF_Err ReadGF_IPMPX_KeyData(GF_BitStream *bs, GF_IPMPX_Data *_p, u32 size)
{
	GF_IPMPX_KeyData*p = (GF_IPMPX_KeyData*)_p;
	p->keyBody = GF_IPMPX_GetByteArray(bs);
	p->flags = 0;
	if (gf_bs_read_int(bs, 1)) p->flags |= 1;
	if (gf_bs_read_int(bs, 1)) p->flags |= 1<<1;
	if (gf_bs_read_int(bs, 1)) p->flags |= 1<<2;
	if (gf_bs_read_int(bs, 1)) p->flags |= 1<<3;
	gf_bs_read_int(bs, 4);
	if (p->flags & (1)) p->startDTS = gf_bs_read_long_int(bs, 64);
	if (p->flags & (1<<1)) p->startPacketID = gf_bs_read_int(bs, 32);
	if (p->flags & (1<<2)) p->expireDTS = gf_bs_read_long_int(bs, 64);
	if (p->flags & (1<<3)) p->expirePacketID = gf_bs_read_int(bs, 32);
	p->OpaqueData = GF_IPMPX_GetByteArray(bs);
	return GF_OK;
}
static u32 SizeGF_IPMPX_KeyData(GF_IPMPX_Data *_p)
{
	u32 size = 0;
	GF_IPMPX_KeyData*p = (GF_IPMPX_KeyData*)_p;
	size += GF_IPMPX_GetByteArraySize(p->keyBody);
	size += 1;
	if (p->flags & (1)) size += 8;
	if (p->flags & (1<<1)) size += 4;
	if (p->flags & (1<<2)) size += 8;
	if (p->flags & (1<<3)) size += 4;
	size += GF_IPMPX_GetByteArraySize(p->OpaqueData);
	return size;
}
static GF_Err WriteGF_IPMPX_KeyData(GF_BitStream *bs, GF_IPMPX_Data *_p)
{
	GF_IPMPX_KeyData*p = (GF_IPMPX_KeyData*)_p;
	GF_IPMPX_WriteByteArray(bs, p->keyBody);
	gf_bs_write_int(bs, (p->flags & (1)) ? 1 : 0, 1);
	gf_bs_write_int(bs, (p->flags & (1<<1)) ? 1 : 0, 1);
	gf_bs_write_int(bs, (p->flags & (1<<2)) ? 1 : 0, 1);
	gf_bs_write_int(bs, (p->flags & (1<<3)) ? 1 : 0, 1);
	gf_bs_write_int(bs, 0, 4);
	if (p->flags & (1)) gf_bs_write_long_int(bs, p->startDTS, 64);
	if (p->flags & (1<<1)) gf_bs_write_int(bs, p->startPacketID, 32);
	if (p->flags & (1<<2)) gf_bs_write_long_int(bs, p->expireDTS, 64);
	if (p->flags & (1<<3)) gf_bs_write_int(bs, p->expirePacketID, 32);
	GF_IPMPX_WriteByteArray(bs, p->OpaqueData);
	return GF_OK;
}

static GF_IPMPX_Data *NewGF_IPMPX_SelectiveDecryptionInit()
{
	GF_IPMPX_SelectiveDecryptionInit*ptr;
	GF_IPMPX_DATA_ALLOC(ptr, GF_IPMPX_SelectiveDecryptionInit, GF_IPMPX_SEL_DEC_INIT_TAG);
	ptr->SelEncBuffer = gf_list_new();
	ptr->SelEncFields = gf_list_new();
	return (GF_IPMPX_Data *) ptr;
}
static void DelGF_IPMPX_SelectiveDecryptionInit(GF_IPMPX_Data *_p)
{
	GF_IPMPX_SelectiveDecryptionInit*p = (GF_IPMPX_SelectiveDecryptionInit*)_p;
	while (gf_list_count(p->SelEncBuffer)) {
		GF_IPMPX_SelEncBuffer *sb = (GF_IPMPX_SelEncBuffer *)gf_list_get(p->SelEncBuffer, 0);
		gf_list_rem(p->SelEncBuffer, 0);
		GF_IPMPX_DELETE_ARRAY(sb->Stream_Cipher_Specific_Init_Info);
		gf_free(sb);
	}
	gf_list_del(p->SelEncBuffer);
	while (gf_list_count(p->SelEncFields)) {
		GF_IPMPX_SelEncField*sf = (GF_IPMPX_SelEncField*)gf_list_get(p->SelEncFields, 0);
		gf_list_rem(p->SelEncFields, 0);
		GF_IPMPX_DELETE_ARRAY(sf->shuffleSpecificInfo);
		if (sf->mappingTable) gf_free(sf->mappingTable);
		gf_free(sf);
	}
	gf_list_del(p->SelEncFields);
	if (p->RLE_Data) gf_free(p->RLE_Data);
	gf_free(p);
}
static GF_Err ReadGF_IPMPX_SelectiveDecryptionInit(GF_BitStream *bs, GF_IPMPX_Data *_p, u32 size)
{
	u32 count, i;
	Bool is_spec;
	GF_IPMPX_SelectiveDecryptionInit*p = (GF_IPMPX_SelectiveDecryptionInit*)_p;
	p->mediaTypeExtension = gf_bs_read_int(bs, 8);
	p->mediaTypeIndication = gf_bs_read_int(bs, 8);
	p->profileLevelIndication = gf_bs_read_int(bs, 8);
	p->compliance = gf_bs_read_int(bs, 8);
	count = gf_bs_read_int(bs, 8);
	while (count) {
		Bool is_block;
		GF_IPMPX_SelEncBuffer *sb;
		GF_SAFEALLOC(sb, GF_IPMPX_SelEncBuffer);
		gf_list_add(p->SelEncBuffer, sb);
		count--;
		gf_bs_read_data(bs, (char*)sb->cipher_Id, 16);
		sb->syncBoundary = gf_bs_read_int(bs, 8);
		is_block = gf_bs_read_int(bs, 1);
		gf_bs_read_int(bs, 7);
		if (is_block) {
			sb->mode = gf_bs_read_int(bs, 8);
			sb->blockSize = gf_bs_read_int(bs, 16);
			sb->keySize = gf_bs_read_int(bs, 16);
		} else {
			sb->Stream_Cipher_Specific_Init_Info = GF_IPMPX_GetByteArray(bs);
		}
	}
	is_spec = gf_bs_read_int(bs, 1);
	gf_bs_read_int(bs, 7);
	if (is_spec) {
		Bool is_map;
		count = gf_bs_read_int(bs, 8);
		while (count) {
			GF_IPMPX_SelEncField *sf;
			GF_SAFEALLOC(sf, GF_IPMPX_SelEncField);
			gf_list_add(p->SelEncFields, sf);
			count--;
			sf->field_Id = gf_bs_read_int(bs, 8);
			sf->field_Scope = gf_bs_read_int(bs, 3);
			gf_bs_read_int(bs, 5);
			sf->buf = gf_bs_read_int(bs, 8);
			is_map = gf_bs_read_int(bs, 1);
			gf_bs_read_int(bs, 7);
			if (is_map) {
				Bool sendMapTable = gf_bs_read_int(bs, 1);
				Bool isShuffled = gf_bs_read_int(bs, 1);
				gf_bs_read_int(bs, 6);
				if (sendMapTable) {
					sf->mappingTableSize = gf_bs_read_int(bs, 16);
					sf->mappingTable = (u16*)gf_malloc(sizeof(u16) * sf->mappingTableSize);
					for (i=0; i<sf->mappingTableSize; i++) sf->mappingTable[i] = gf_bs_read_int(bs, 16);
				}
				if (isShuffled) sf->shuffleSpecificInfo = GF_IPMPX_GetByteArray(bs);
			}
		}
	} else {
		p->RLE_DataLength = gf_bs_read_int(bs, 16);
		p->RLE_Data = (u16*)gf_malloc(sizeof(u16)*p->RLE_DataLength);
		for (i=0; i<p->RLE_DataLength; i++) p->RLE_Data[i] = gf_bs_read_int(bs, 16);
	}
	return GF_OK;
}
static u32 SizeGF_IPMPX_SelectiveDecryptionInit(GF_IPMPX_Data *_p)
{
	u32 size, i;
	GF_IPMPX_SelectiveDecryptionInit*p = (GF_IPMPX_SelectiveDecryptionInit*)_p;

	size = 5;
	for (i=0; i<gf_list_count(p->SelEncBuffer); i++) {
		GF_IPMPX_SelEncBuffer *sb = (GF_IPMPX_SelEncBuffer *)gf_list_get(p->SelEncBuffer, i);
		size += 18;
		if (sb->Stream_Cipher_Specific_Init_Info) {
			size += GF_IPMPX_GetByteArraySize(sb->Stream_Cipher_Specific_Init_Info);
		} else {
			size += 5;
		}
	}
	size += 1;
	if (p->RLE_Data) {
		size += 2 + 2*p->RLE_DataLength;
	} else {
		size += 1;
		for (i=0; i<gf_list_count(p->SelEncFields); i++) {
			GF_IPMPX_SelEncField *sf = (GF_IPMPX_SelEncField *)gf_list_get(p->SelEncFields, i);
			size += 4;
			if (sf->mappingTable || sf->shuffleSpecificInfo) {
				size += 1;
				if (sf->mappingTable) size += 2 + 2*sf->mappingTableSize;
				if (sf->shuffleSpecificInfo) size += GF_IPMPX_GetByteArraySize(sf->shuffleSpecificInfo);
			}
		}
	}
	return size;
}
static GF_Err WriteGF_IPMPX_SelectiveDecryptionInit(GF_BitStream *bs, GF_IPMPX_Data *_p)
{
	u32 count, i;
	GF_IPMPX_SelectiveDecryptionInit*p = (GF_IPMPX_SelectiveDecryptionInit*)_p;

	gf_bs_write_int(bs, p->mediaTypeExtension, 8);
	gf_bs_write_int(bs, p->mediaTypeIndication, 8);
	gf_bs_write_int(bs, p->profileLevelIndication, 8);
	gf_bs_write_int(bs, p->compliance, 8);

	count = gf_list_count(p->SelEncBuffer);
	gf_bs_write_int(bs, count, 8);
	for (i=0; i<count; i++) {
		GF_IPMPX_SelEncBuffer *sb = (GF_IPMPX_SelEncBuffer *)gf_list_get(p->SelEncBuffer, i);
		gf_bs_write_data(bs, (char*)sb->cipher_Id, 16);
		gf_bs_write_int(bs, sb->syncBoundary, 8);
		gf_bs_write_int(bs, sb->Stream_Cipher_Specific_Init_Info ? 0 : 1, 1);
		gf_bs_write_int(bs, 0, 7);
		if (sb->Stream_Cipher_Specific_Init_Info) {
			GF_IPMPX_WriteByteArray(bs, sb->Stream_Cipher_Specific_Init_Info);
		} else {
			gf_bs_write_int(bs, sb->mode, 8);
			gf_bs_write_int(bs, sb->blockSize, 16);
			gf_bs_write_int(bs, sb->keySize, 16);
		}
	}
	gf_bs_write_int(bs, p->RLE_Data ? 0 : 1, 1);
	gf_bs_write_int(bs, 0, 7);
	if (p->RLE_Data) {
		gf_bs_write_int(bs, p->RLE_DataLength, 16);
		for (i=0; i<p->RLE_DataLength; i++) gf_bs_write_int(bs, p->RLE_Data[i], 16);
	} else {
		count = gf_list_count(p->SelEncFields);
		gf_bs_write_int(bs, count, 8);
		for (i=0; i<count; i++) {
			GF_IPMPX_SelEncField *sf = (GF_IPMPX_SelEncField *)gf_list_get(p->SelEncFields, i);
			gf_bs_write_int(bs, sf->field_Id, 8);
			gf_bs_write_int(bs, sf->field_Scope, 3);
			gf_bs_write_int(bs, 0, 5);
			gf_bs_write_int(bs, sf->buf, 8);
			gf_bs_write_int(bs, (sf->mappingTable || sf->shuffleSpecificInfo) ? 1 : 0, 1);
			gf_bs_write_int(bs, 0, 7);
			if (sf->mappingTable || sf->shuffleSpecificInfo) {
				gf_bs_write_int(bs, sf->mappingTable ? 1 : 0, 1);
				gf_bs_write_int(bs, sf->shuffleSpecificInfo ? 1 : 0, 1);
				gf_bs_write_int(bs, 0, 6);
				if (sf->mappingTable) {
					gf_bs_write_int(bs, sf->mappingTableSize, 16);
					for (i=0; i<sf->mappingTableSize; i++) gf_bs_write_int(bs, sf->mappingTable[i], 16);
				}
				if (sf->shuffleSpecificInfo) GF_IPMPX_WriteByteArray(bs, sf->shuffleSpecificInfo);
			}
		}
	}
	return GF_OK;
}
static GF_IPMPX_Data *NewGF_IPMPX_WatermarkingInit(u8 tag)
{
	GF_IPMPX_WatermarkingInit *ptr;
	GF_IPMPX_DATA_ALLOC(ptr, GF_IPMPX_WatermarkingInit, tag);
	return (GF_IPMPX_Data *) ptr;
}
static void DelGF_IPMPX_WatermarkingInit(GF_IPMPX_Data *_p)
{
	GF_IPMPX_WatermarkingInit *p = (GF_IPMPX_WatermarkingInit*)_p;
	if (p->wmPayload) gf_free(p->wmPayload);
	if (p->opaqueData) gf_free(p->opaqueData);
	gf_free(p);
}
static GF_Err ReadGF_IPMPX_WatermarkingInit(GF_BitStream *bs, GF_IPMPX_Data *_p, u32 size)
{
	Bool has_opaque_data;
	GF_IPMPX_WatermarkingInit *p = (GF_IPMPX_WatermarkingInit*)_p;

	p->inputFormat = gf_bs_read_int(bs, 8);
	p->requiredOp = gf_bs_read_int(bs, 4);
	has_opaque_data = gf_bs_read_int(bs, 1);
	gf_bs_read_int(bs, 3);
	if (p->inputFormat==0x01) {
		if (p->tag == GF_IPMPX_AUDIO_WM_INIT_TAG) {
			p->nChannels = gf_bs_read_int(bs, 8);
			p->bitPerSample = gf_bs_read_int(bs, 8);
			p->frequency = gf_bs_read_int(bs, 32);
		} else {
			p->frame_horizontal_size = gf_bs_read_int(bs, 16);
			p->frame_vertical_size = gf_bs_read_int(bs, 16);
			p->chroma_format = gf_bs_read_int(bs, 8);
		}
	}
	switch (p->requiredOp) {
	case GF_IPMPX_WM_INSERT:
	case GF_IPMPX_WM_REMARK:
		p->wmPayloadLen = gf_bs_read_int(bs, 16);
		p->wmPayload = (char*)gf_malloc(sizeof(u8) * p->wmPayloadLen);
		gf_bs_read_data(bs, p->wmPayload, p->wmPayloadLen);
		break;
	case GF_IPMPX_WM_EXTRACT:
	case GF_IPMPX_WM_DETECT_COMPRESSION:
		p->wmRecipientId = gf_bs_read_int(bs, 16);
		break;
	}
	if (has_opaque_data) {
		p->opaqueDataSize = gf_bs_read_int(bs, 16);
		p->opaqueData = (char*)gf_malloc(sizeof(u8) * p->wmPayloadLen);
		gf_bs_read_data(bs, p->opaqueData, p->opaqueDataSize);
	}
	return GF_OK;
}
static u32 SizeGF_IPMPX_WatermarkingInit(GF_IPMPX_Data *_p)
{
	u32 size;
	GF_IPMPX_WatermarkingInit *p = (GF_IPMPX_WatermarkingInit*)_p;
	size = 2;
	if (p->inputFormat==0x01) size += (p->tag == GF_IPMPX_AUDIO_WM_INIT_TAG) ? 6 : 5;
	switch (p->requiredOp) {
	case GF_IPMPX_WM_INSERT:
	case GF_IPMPX_WM_REMARK:
		size += 2+p->wmPayloadLen;
		break;
	case GF_IPMPX_WM_EXTRACT:
	case GF_IPMPX_WM_DETECT_COMPRESSION:
		size += 2;
		break;
	}
	if (p->opaqueData) size += p->opaqueDataSize + 2;
	return size;
}
static GF_Err WriteGF_IPMPX_WatermarkingInit(GF_BitStream *bs, GF_IPMPX_Data *_p)
{
	GF_IPMPX_WatermarkingInit*p = (GF_IPMPX_WatermarkingInit*)_p;

	gf_bs_write_int(bs, p->inputFormat, 8);
	gf_bs_write_int(bs, p->requiredOp, 4);
	gf_bs_write_int(bs, p->opaqueData ? 1 : 0, 1);
	gf_bs_write_int(bs, 0, 3);
	if (p->inputFormat==0x01) {
		if (p->tag == GF_IPMPX_AUDIO_WM_INIT_TAG) {
			gf_bs_write_int(bs, p->nChannels, 8);
			gf_bs_write_int(bs, p->bitPerSample, 8);
			gf_bs_write_int(bs, p->frequency, 32);
		} else {
			gf_bs_write_int(bs, p->frame_horizontal_size, 16);
			gf_bs_write_int(bs, p->frame_vertical_size, 16);
			gf_bs_write_int(bs, p->chroma_format, 8);
		}
	}
	switch (p->requiredOp) {
	case GF_IPMPX_WM_INSERT:
	case GF_IPMPX_WM_REMARK:
		gf_bs_write_int(bs, p->wmPayloadLen, 16);
		gf_bs_write_data(bs, p->wmPayload, p->wmPayloadLen);
		break;
	case GF_IPMPX_WM_EXTRACT:
	case GF_IPMPX_WM_DETECT_COMPRESSION:
		gf_bs_write_int(bs, p->wmRecipientId, 16);
		break;
	}
	if (p->opaqueData) {
		gf_bs_write_int(bs, p->opaqueDataSize, 16);
		gf_bs_write_data(bs, p->opaqueData, p->opaqueDataSize);
	}
	return GF_OK;
}
static GF_IPMPX_Data *NewGF_IPMPX_SendWatermark(u8 tag)
{
	GF_IPMPX_SendWatermark*ptr;
	GF_IPMPX_DATA_ALLOC(ptr, GF_IPMPX_SendWatermark, tag);
	return (GF_IPMPX_Data *) ptr;
}
static void DelGF_IPMPX_SendWatermark(GF_IPMPX_Data *_p)
{
	GF_IPMPX_SendWatermark*p = (GF_IPMPX_SendWatermark*)_p;
	GF_IPMPX_DELETE_ARRAY(p->payload);
	GF_IPMPX_DELETE_ARRAY(p->opaqueData);
	gf_free(p);
}
static GF_Err ReadGF_IPMPX_SendWatermark(GF_BitStream *bs, GF_IPMPX_Data *_p, u32 size)
{
	Bool has_op_data;
	GF_IPMPX_SendWatermark *p = (GF_IPMPX_SendWatermark*)_p;
	p->wm_status = gf_bs_read_int(bs, 2);
	p->compression_status = gf_bs_read_int(bs, 2);
	has_op_data = gf_bs_read_int(bs, 1);
	gf_bs_read_int(bs, 3);
	if (p->wm_status==GF_IPMPX_WM_PAYLOAD) p->payload = GF_IPMPX_GetByteArray(bs);
	if (has_op_data) p->opaqueData = GF_IPMPX_GetByteArray(bs);
	return GF_OK;
}
static u32 SizeGF_IPMPX_SendWatermark(GF_IPMPX_Data *_p)
{
	u32 size;
	GF_IPMPX_SendWatermark *p = (GF_IPMPX_SendWatermark*)_p;
	size = 1;
	if (p->payload) size += GF_IPMPX_GetByteArraySize(p->payload);
	if (p->opaqueData) size += GF_IPMPX_GetByteArraySize(p->opaqueData);
	return size;
}
static GF_Err WriteGF_IPMPX_SendWatermark(GF_BitStream *bs, GF_IPMPX_Data *_p)
{
	GF_IPMPX_SendWatermark*p = (GF_IPMPX_SendWatermark*)_p;
	if (p->payload) p->wm_status = GF_IPMPX_WM_PAYLOAD;
	gf_bs_write_int(bs, p->wm_status, 2);
	gf_bs_write_int(bs, p->compression_status, 2);
	gf_bs_write_int(bs, p->opaqueData ? 1 : 0, 1);
	gf_bs_write_int(bs, 0, 3);
	if (p->payload) GF_IPMPX_WriteByteArray(bs, p->payload);
	if (p->opaqueData) GF_IPMPX_WriteByteArray(bs, p->opaqueData);
	return GF_OK;
}

static GF_IPMPX_Data *NewGF_IPMPX_ToolAPI_Config()
{
	GF_IPMPX_ToolAPI_Config*ptr;
	GF_IPMPX_DATA_ALLOC(ptr, GF_IPMPX_ToolAPI_Config, GF_IPMPX_TOOL_API_CONFIG_TAG);
	return (GF_IPMPX_Data *) ptr;
}
static void DelGF_IPMPX_ToolAPI_Config(GF_IPMPX_Data *_p)
{
	GF_IPMPX_ToolAPI_Config*p = (GF_IPMPX_ToolAPI_Config*)_p;
	GF_IPMPX_DELETE_ARRAY(p->opaqueData);
	gf_free(p);
}
static GF_Err ReadGF_IPMPX_ToolAPI_Config(GF_BitStream *bs, GF_IPMPX_Data *_p, u32 size)
{
	Bool has_i, has_m;
	GF_IPMPX_ToolAPI_Config*p = (GF_IPMPX_ToolAPI_Config*)_p;
	has_i = gf_bs_read_int(bs, 1);
	has_m = gf_bs_read_int(bs, 1);
	gf_bs_read_int(bs, 6);
	if (has_i) p->Instantiation_API_ID = gf_bs_read_int(bs, 32);
	if (has_m) p->Messaging_API_ID = gf_bs_read_int(bs, 32);
	p->opaqueData = GF_IPMPX_GetByteArray(bs);
	return GF_OK;
}
static u32 SizeGF_IPMPX_ToolAPI_Config(GF_IPMPX_Data *_p)
{
	u32 size;
	GF_IPMPX_ToolAPI_Config *p = (GF_IPMPX_ToolAPI_Config*)_p;
	size = 1;
	if (p->Instantiation_API_ID) size += 4;
	if (p->Messaging_API_ID ) size += 4;
	size += GF_IPMPX_GetByteArraySize(p->opaqueData);
	return size;
}
static GF_Err WriteGF_IPMPX_ToolAPI_Config(GF_BitStream *bs, GF_IPMPX_Data *_p)
{
	GF_IPMPX_ToolAPI_Config*p = (GF_IPMPX_ToolAPI_Config*)_p;
	gf_bs_write_int(bs, p->Instantiation_API_ID ? 1 : 0, 1);
	gf_bs_write_int(bs, p->Messaging_API_ID ? 1 : 0, 1);
	gf_bs_write_int(bs, 0, 6);
	if (p->Instantiation_API_ID) gf_bs_write_int(bs, p->Instantiation_API_ID, 32);
	if (p->Messaging_API_ID) gf_bs_write_int(bs, p->Messaging_API_ID, 32);
	GF_IPMPX_WriteByteArray(bs, p->opaqueData);
	return GF_OK;
}

static GF_IPMPX_Data *NewGF_IPMPX_ISMACryp()
{
	GF_IPMPX_ISMACryp *ptr;
	GF_IPMPX_DATA_ALLOC(ptr, GF_IPMPX_ISMACryp, GF_IPMPX_ISMACRYP_TAG);
	return (GF_IPMPX_Data *) ptr;
}
static void DelGF_IPMPX_ISMACryp(GF_IPMPX_Data *_p)
{
	GF_IPMPX_ISMACryp*p = (GF_IPMPX_ISMACryp*)_p;
	gf_free(p);
}
static GF_Err ReadGF_IPMPX_ISMACryp(GF_BitStream *bs, GF_IPMPX_Data *_p, u32 size)
{
	GF_IPMPX_ISMACryp*p = (GF_IPMPX_ISMACryp*)_p;
	p->cryptoSuite = gf_bs_read_int(bs, 8);
	p->IV_length = gf_bs_read_int(bs, 8);
	p->use_selective_encryption = gf_bs_read_int(bs, 1);
	gf_bs_read_int(bs, 7);
	p->key_indicator_length = gf_bs_read_int(bs, 8);
	return GF_OK;
}
static u32 SizeGF_IPMPX_ISMACryp(GF_IPMPX_Data *_p)
{
	return 4;
}
static GF_Err WriteGF_IPMPX_ISMACryp(GF_BitStream *bs, GF_IPMPX_Data *_p)
{
	GF_IPMPX_ISMACryp*p = (GF_IPMPX_ISMACryp*)_p;
	gf_bs_write_int(bs, p->cryptoSuite, 8);
	gf_bs_write_int(bs, p->IV_length, 8);
	gf_bs_write_int(bs, p->use_selective_encryption ? 1 : 0, 1);
	gf_bs_write_int(bs, 0, 7);
	gf_bs_write_int(bs, p->key_indicator_length, 8);
	return GF_OK;
}

GF_IPMPX_Data *gf_ipmpx_data_new(u8 tag)
{
	switch (tag) {
	case GF_IPMPX_RIGHTS_DATA_TAG:
	case GF_IPMPX_OPAQUE_DATA_TAG: return NewGF_IPMPX_OpaqueData(tag);
	case GF_IPMPX_KEY_DATA_TAG: return NewGF_IPMPX_KeyData();
	case GF_IPMPX_SECURE_CONTAINER_TAG: return NewGF_IPMPX_SecureContainer();
	case GF_IPMPX_ADD_TOOL_LISTENER_TAG: return NewGF_IPMPX_AddToolNotificationListener();
	case GF_IPMPX_REMOVE_TOOL_LISTENER_TAG: return NewGF_IPMPX_RemoveToolNotificationListener();
	case GF_IPMPX_INIT_AUTHENTICATION_TAG: return NewGF_IPMPX_InitAuthentication();
	case GF_IPMPX_MUTUAL_AUTHENTICATION_TAG: return NewGF_IPMPX_MutualAuthentication();
	case GF_IPMPX_PARAMETRIC_DESCRIPTION_TAG: return NewGF_IPMPX_ParametricDescription();
	case GF_IPMPX_PARAMETRIC_CAPS_QUERY_TAG: return NewGF_IPMPX_ToolParamCapabilitiesQuery();
	case GF_IPMPX_PARAMETRIC_CAPS_RESPONSE_TAG: return NewGF_IPMPX_ToolParamCapabilitiesResponse();
	case GF_IPMPX_GET_TOOLS_RESPONSE_TAG: return NewGF_IPMPX_GetToolsResponse();
	case GF_IPMPX_GET_TOOL_CONTEXT_TAG: return NewGF_IPMPX_GetToolContext();
	case GF_IPMPX_GET_TOOL_CONTEXT_RESPONSE_TAG: return NewGF_IPMPX_GetToolContextResponse();
	case GF_IPMPX_CONNECT_TOOL_TAG: return NewGF_IPMPX_ConnectTool();
	case GF_IPMPX_DISCONNECT_TOOL_TAG: return NewGF_IPMPX_DisconnectTool();
	case GF_IPMPX_NOTIFY_TOOL_EVENT_TAG: return NewGF_IPMPX_NotifyToolEvent();
	case GF_IPMPX_CAN_PROCESS_TAG: return NewGF_IPMPX_CanProcess();
	case GF_IPMPX_TRUST_SECURITY_METADATA_TAG: return NewGF_IPMPX_TrustSecurityMetadata();
	case GF_IPMPX_ISMACRYP_TAG: return NewGF_IPMPX_ISMACryp();
	case GF_IPMPX_GET_TOOLS_TAG: 
	{
		GF_IPMPX_Data *p;
		GF_IPMPX_DATA_ALLOC(p, GF_IPMPX_Data, GF_IPMPX_GET_TOOLS_TAG);
		return p;
	}
	case GF_IPMPX_TRUSTED_TOOL_TAG:
	{
		GF_IPMPX_TrustedTool *p;
		GF_IPMPX_DATA_ALLOC(p, GF_IPMPX_TrustedTool, GF_IPMPX_TRUSTED_TOOL_TAG);
		p->trustSpecifications = gf_list_new();
		return (GF_IPMPX_Data *)p;
	}
	case GF_IPMPX_TRUST_SPECIFICATION_TAG:
	{
		GF_IPMPX_TrustSpecification *p;
		GF_IPMPX_DATA_ALLOC(p, GF_IPMPX_TrustSpecification, GF_IPMPX_TRUST_SPECIFICATION_TAG);
		return (GF_IPMPX_Data *)p;
	}
	case GF_IPMPX_TOOL_API_CONFIG_TAG: return NewGF_IPMPX_ToolAPI_Config();
	case GF_IPMPX_SEL_DEC_INIT_TAG: return NewGF_IPMPX_SelectiveDecryptionInit();
	case GF_IPMPX_AUDIO_WM_INIT_TAG: 
	case GF_IPMPX_VIDEO_WM_INIT_TAG: 
		return NewGF_IPMPX_WatermarkingInit(tag);
	case GF_IPMPX_AUDIO_WM_SEND_TAG: 
	case GF_IPMPX_VIDEO_WM_SEND_TAG: 
		return NewGF_IPMPX_SendWatermark(tag);

	case GF_IPMPX_ALGORITHM_DESCRIPTOR_TAG:
	{
		GF_IPMPX_AUTH_AlgorithmDescriptor *p = (GF_IPMPX_AUTH_AlgorithmDescriptor *)gf_malloc(sizeof(GF_IPMPX_AUTH_AlgorithmDescriptor));
		if (!p) return NULL;
		memset(p, 0, sizeof(GF_IPMPX_AUTH_AlgorithmDescriptor));
		p->tag = GF_IPMPX_ALGORITHM_DESCRIPTOR_TAG;
		return (GF_IPMPX_Data *) p;
	}
	case GF_IPMPX_KEY_DESCRIPTOR_TAG:
	{
		GF_IPMPX_AUTH_KeyDescriptor *p = (GF_IPMPX_AUTH_KeyDescriptor *)gf_malloc(sizeof(GF_IPMPX_AUTH_KeyDescriptor));
		if (!p) return NULL;
		memset(p, 0, sizeof(GF_IPMPX_AUTH_KeyDescriptor));
		p->tag = GF_IPMPX_KEY_DESCRIPTOR_TAG;
		return (GF_IPMPX_Data *) p;
	}

	case GF_IPMPX_PARAM_DESCRIPTOR_ITEM_TAG:
	{
		GF_IPMPX_AUTH_KeyDescriptor *p = (GF_IPMPX_AUTH_KeyDescriptor *)gf_malloc(sizeof(GF_IPMPX_ParametricDescriptionItem));
		if (!p) return NULL;
		memset(p, 0, sizeof(GF_IPMPX_ParametricDescriptionItem));
		p->tag = GF_IPMPX_PARAM_DESCRIPTOR_ITEM_TAG;
		return (GF_IPMPX_Data *) p;
	}
	case GF_IPMPX_SEL_ENC_BUFFER_TAG:
	{
		GF_IPMPX_SelEncBuffer*p = (GF_IPMPX_SelEncBuffer*)gf_malloc(sizeof(GF_IPMPX_SelEncBuffer));
		if (!p) return NULL;
		memset(p, 0, sizeof(GF_IPMPX_SelEncBuffer));
		p->tag = GF_IPMPX_SEL_ENC_BUFFER_TAG;
		return (GF_IPMPX_Data *) p;
	}
	case GF_IPMPX_SEL_ENC_FIELD_TAG:
	{
		GF_IPMPX_SelEncField*p = (GF_IPMPX_SelEncField*)gf_malloc(sizeof(GF_IPMPX_SelEncField));
		if (!p) return NULL;
		memset(p, 0, sizeof(GF_IPMPX_SelEncField));
		p->tag = GF_IPMPX_SEL_ENC_FIELD_TAG;
		return (GF_IPMPX_Data *) p;
	}

/*	
	case GF_IPMPX_USER_QUERY_TAG: return NewGF_IPMPX_UserQuery();
	case GF_IPMPX_USER_RESPONSE_TAG: return NewGF_IPMPX_UserQueryResponse();
*/
	default: return NULL;
	}

}

void gf_ipmpx_data_del(GF_IPMPX_Data *_p)
{
	if (!_p) return;
	switch (_p->tag) {
	case GF_IPMPX_RIGHTS_DATA_TAG:
	case GF_IPMPX_OPAQUE_DATA_TAG: DelGF_IPMPX_OpaqueData(_p); return;
	case GF_IPMPX_KEY_DATA_TAG: DelGF_IPMPX_KeyData(_p); return;
	case GF_IPMPX_SECURE_CONTAINER_TAG: DelGF_IPMPX_SecureContainer(_p); return;
	case GF_IPMPX_ADD_TOOL_LISTENER_TAG: DelGF_IPMPX_AddToolNotificationListener(_p); return;
	case GF_IPMPX_REMOVE_TOOL_LISTENER_TAG: DelGF_IPMPX_RemoveToolNotificationListener(_p); return;
	case GF_IPMPX_INIT_AUTHENTICATION_TAG: DelGF_IPMPX_InitAuthentication(_p); return;
	case GF_IPMPX_MUTUAL_AUTHENTICATION_TAG: DelGF_IPMPX_MutualAuthentication(_p); return;
	case GF_IPMPX_PARAMETRIC_DESCRIPTION_TAG: DelGF_IPMPX_ParametricDescription(_p); return;
	case GF_IPMPX_PARAMETRIC_CAPS_QUERY_TAG: DelGF_IPMPX_ToolParamCapabilitiesQuery(_p); return;
	case GF_IPMPX_PARAMETRIC_CAPS_RESPONSE_TAG: DelGF_IPMPX_ToolParamCapabilitiesResponse(_p); return;
	case GF_IPMPX_GET_TOOLS_RESPONSE_TAG: DelGF_IPMPX_GetToolsResponse(_p); return;
	case GF_IPMPX_GET_TOOL_CONTEXT_TAG: DelGF_IPMPX_GetToolContext(_p); return;
	case GF_IPMPX_GET_TOOL_CONTEXT_RESPONSE_TAG: DelGF_IPMPX_GetToolContextResponse(_p); return;
	case GF_IPMPX_CONNECT_TOOL_TAG: DelGF_IPMPX_ConnectTool(_p); return;
	case GF_IPMPX_DISCONNECT_TOOL_TAG: DelGF_IPMPX_DisconnectTool(_p); return;
	case GF_IPMPX_NOTIFY_TOOL_EVENT_TAG: DelGF_IPMPX_NotifyToolEvent(_p); return;
	case GF_IPMPX_CAN_PROCESS_TAG: DelGF_IPMPX_CanProcess(_p); return;
	case GF_IPMPX_TRUST_SECURITY_METADATA_TAG: DelGF_IPMPX_TrustSecurityMetadata(_p); return;
	case GF_IPMPX_TOOL_API_CONFIG_TAG: DelGF_IPMPX_ToolAPI_Config(_p); return;
	case GF_IPMPX_ISMACRYP_TAG: DelGF_IPMPX_ISMACryp(_p); return;
	case GF_IPMPX_SEL_DEC_INIT_TAG: DelGF_IPMPX_SelectiveDecryptionInit(_p); return;
	case GF_IPMPX_AUDIO_WM_INIT_TAG: 
	case GF_IPMPX_VIDEO_WM_INIT_TAG: 
		DelGF_IPMPX_WatermarkingInit(_p); return;
	case GF_IPMPX_AUDIO_WM_SEND_TAG: 
	case GF_IPMPX_VIDEO_WM_SEND_TAG: 
		DelGF_IPMPX_SendWatermark(_p); return;

/*
	case GF_IPMPX_USER_QUERY_TAG: DelGF_IPMPX_UserQuery(_p); return;
	case GF_IPMPX_USER_RESPONSE_TAG: DelGF_IPMPX_UserQueryResponse(_p); return;
*/
	case GF_IPMPX_TRUSTED_TOOL_TAG:
	{
		GF_IPMPX_TrustedTool *p = (GF_IPMPX_TrustedTool *)_p;
		while (gf_list_count(p->trustSpecifications)) {
			GF_IPMPX_Data *ts = (GF_IPMPX_Data *)gf_list_get(p->trustSpecifications, 0);
			gf_list_rem(p->trustSpecifications, 0);
			gf_ipmpx_data_del(ts);
		}
		gf_list_del(p->trustSpecifications);
		gf_free(p);
		return;
	}
	case GF_IPMPX_TRUST_SPECIFICATION_TAG:
	{
		GF_IPMPX_TrustSpecification *p = (GF_IPMPX_TrustSpecification *)_p;
		GF_IPMPX_DELETE_ARRAY(p->CCTrustMetadata);
		gf_free(p);
		return;
	}
	case GF_IPMPX_PARAM_DESCRIPTOR_ITEM_TAG:
	{
		GF_IPMPX_ParametricDescriptionItem *p = (GF_IPMPX_ParametricDescriptionItem*)_p;
		GF_IPMPX_DELETE_ARRAY(p->main_class);
		GF_IPMPX_DELETE_ARRAY(p->subClass);
		GF_IPMPX_DELETE_ARRAY(p->typeData);
		GF_IPMPX_DELETE_ARRAY(p->type);
		GF_IPMPX_DELETE_ARRAY(p->addedData);
		gf_free(p);
		return;
	}
	case GF_IPMPX_ALGORITHM_DESCRIPTOR_TAG:
		_p->tag = GF_IPMPX_AUTH_AlgorithmDescr_Tag;
		GF_IPMPX_AUTH_Delete((GF_IPMPX_Authentication*)_p);
		return;
	case GF_IPMPX_KEY_DESCRIPTOR_TAG:
		_p->tag = GF_IPMPX_AUTH_KeyDescr_Tag;
		GF_IPMPX_AUTH_Delete((GF_IPMPX_Authentication*)_p);
		return;

	case GF_IPMPX_SEL_ENC_BUFFER_TAG:
	{
		GF_IPMPX_SelEncBuffer*p = (GF_IPMPX_SelEncBuffer*)_p;
		GF_IPMPX_DELETE_ARRAY(p->Stream_Cipher_Specific_Init_Info);
		gf_free(p);
		return;
	}
	case GF_IPMPX_SEL_ENC_FIELD_TAG:
	{
		GF_IPMPX_SelEncField*p = (GF_IPMPX_SelEncField*)_p;
		GF_IPMPX_DELETE_ARRAY(p->shuffleSpecificInfo);
		if (p->mappingTable) gf_free(p->mappingTable);
		gf_free(p);
		return;
	}

	case GF_IPMPX_GET_TOOLS_TAG:
	default: 
		gf_free(_p);
		return;
	}
}

GF_Err GF_IPMPX_ReadData(GF_BitStream *bs, GF_IPMPX_Data *_p, u32 read)
{
	switch (_p->tag) {
	case GF_IPMPX_RIGHTS_DATA_TAG:
	case GF_IPMPX_OPAQUE_DATA_TAG: return ReadGF_IPMPX_OpaqueData(bs, _p, read);
	case GF_IPMPX_KEY_DATA_TAG: return ReadGF_IPMPX_KeyData(bs, _p, read);
	case GF_IPMPX_SECURE_CONTAINER_TAG: return ReadGF_IPMPX_SecureContainer(bs, _p, read);
	case GF_IPMPX_ADD_TOOL_LISTENER_TAG: return ReadGF_IPMPX_AddToolNotificationListener(bs, _p, read);
	case GF_IPMPX_REMOVE_TOOL_LISTENER_TAG: return ReadGF_IPMPX_RemoveToolNotificationListener(bs, _p, read);
	case GF_IPMPX_INIT_AUTHENTICATION_TAG: return ReadGF_IPMPX_InitAuthentication(bs, _p, read);
	case GF_IPMPX_MUTUAL_AUTHENTICATION_TAG: return ReadGF_IPMPX_MutualAuthentication(bs, _p, read);
	case GF_IPMPX_PARAMETRIC_DESCRIPTION_TAG: return ReadGF_IPMPX_ParametricDescription(bs, _p, read);
	case GF_IPMPX_PARAMETRIC_CAPS_QUERY_TAG: return ReadGF_IPMPX_ToolParamCapabilitiesQuery(bs, _p, read);
	case GF_IPMPX_PARAMETRIC_CAPS_RESPONSE_TAG: return ReadGF_IPMPX_ToolParamCapabilitiesResponse(bs, _p, read);
	case GF_IPMPX_GET_TOOLS_RESPONSE_TAG: return ReadGF_IPMPX_GetToolsResponse(bs, _p, read);
	case GF_IPMPX_GET_TOOL_CONTEXT_TAG: return ReadGF_IPMPX_GetToolContext(bs, _p, read);
	case GF_IPMPX_GET_TOOL_CONTEXT_RESPONSE_TAG: return ReadGF_IPMPX_GetToolContextResponse(bs, _p, read);
	case GF_IPMPX_CONNECT_TOOL_TAG: return ReadGF_IPMPX_ConnectTool(bs, _p, read);
	case GF_IPMPX_DISCONNECT_TOOL_TAG: return ReadGF_IPMPX_DisconnectTool(bs, _p, read);
	case GF_IPMPX_NOTIFY_TOOL_EVENT_TAG: return ReadGF_IPMPX_NotifyToolEvent(bs, _p, read);
	case GF_IPMPX_CAN_PROCESS_TAG: return ReadGF_IPMPX_CanProcess(bs, _p, read);
	case GF_IPMPX_TRUST_SECURITY_METADATA_TAG: return ReadGF_IPMPX_TrustSecurityMetadata(bs, _p, read);
	case GF_IPMPX_TOOL_API_CONFIG_TAG: return ReadGF_IPMPX_ToolAPI_Config(bs, _p, read);
	case GF_IPMPX_ISMACRYP_TAG: return ReadGF_IPMPX_ISMACryp(bs, _p, read);
	case GF_IPMPX_SEL_DEC_INIT_TAG: return ReadGF_IPMPX_SelectiveDecryptionInit(bs, _p, read);
	case GF_IPMPX_AUDIO_WM_INIT_TAG: 
	case GF_IPMPX_VIDEO_WM_INIT_TAG: 
		return ReadGF_IPMPX_WatermarkingInit(bs, _p, read);
	case GF_IPMPX_AUDIO_WM_SEND_TAG: 
	case GF_IPMPX_VIDEO_WM_SEND_TAG: 
		return ReadGF_IPMPX_SendWatermark(bs, _p, read);

/*
	case GF_IPMPX_USER_QUERY_TAG: return ReadGF_IPMPX_UserQuery(bs, _p, read);
	case GF_IPMPX_USER_RESPONSE_TAG: return ReadGF_IPMPX_UserQueryResponse(bs, _p, read);
*/
	case GF_IPMPX_GET_TOOLS_TAG: return GF_OK;
	default: return GF_BAD_PARAM;
	}
}

u32 gf_ipmpx_data_size(GF_IPMPX_Data *_p)
{
	switch (_p->tag) {
	case GF_IPMPX_RIGHTS_DATA_TAG:
	case GF_IPMPX_OPAQUE_DATA_TAG: return SizeGF_IPMPX_OpaqueData(_p);
	case GF_IPMPX_KEY_DATA_TAG: return SizeGF_IPMPX_KeyData(_p);
	case GF_IPMPX_SECURE_CONTAINER_TAG: return SizeGF_IPMPX_SecureContainer(_p);
	case GF_IPMPX_ADD_TOOL_LISTENER_TAG: return SizeGF_IPMPX_AddToolNotificationListener(_p);
	case GF_IPMPX_REMOVE_TOOL_LISTENER_TAG: return SizeGF_IPMPX_RemoveToolNotificationListener(_p);
	case GF_IPMPX_INIT_AUTHENTICATION_TAG: return SizeGF_IPMPX_InitAuthentication(_p);
	case GF_IPMPX_MUTUAL_AUTHENTICATION_TAG: return SizeGF_IPMPX_MutualAuthentication(_p);
	case GF_IPMPX_PARAMETRIC_DESCRIPTION_TAG: return SizeGF_IPMPX_ParametricDescription(_p);
	case GF_IPMPX_PARAMETRIC_CAPS_QUERY_TAG: return SizeGF_IPMPX_ToolParamCapabilitiesQuery(_p);
	case GF_IPMPX_PARAMETRIC_CAPS_RESPONSE_TAG: return SizeGF_IPMPX_ToolParamCapabilitiesResponse(_p);
	case GF_IPMPX_GET_TOOLS_RESPONSE_TAG: return SizeGF_IPMPX_GetToolsResponse(_p);
	case GF_IPMPX_GET_TOOL_CONTEXT_TAG: return SizeGF_IPMPX_GetToolContext(_p);
	case GF_IPMPX_GET_TOOL_CONTEXT_RESPONSE_TAG: return SizeGF_IPMPX_GetToolContextResponse(_p);
	case GF_IPMPX_CONNECT_TOOL_TAG: return SizeGF_IPMPX_ConnectTool(_p);
	case GF_IPMPX_DISCONNECT_TOOL_TAG: return SizeGF_IPMPX_DisconnectTool(_p);
	case GF_IPMPX_NOTIFY_TOOL_EVENT_TAG: return SizeGF_IPMPX_NotifyToolEvent(_p);
	case GF_IPMPX_CAN_PROCESS_TAG: return SizeGF_IPMPX_CanProcess(_p);
	case GF_IPMPX_TRUST_SECURITY_METADATA_TAG: return SizeGF_IPMPX_TrustSecurityMetadata(_p);
	case GF_IPMPX_TOOL_API_CONFIG_TAG: return SizeGF_IPMPX_ToolAPI_Config(_p);
	case GF_IPMPX_ISMACRYP_TAG: return SizeGF_IPMPX_ISMACryp(_p);
	case GF_IPMPX_SEL_DEC_INIT_TAG: return SizeGF_IPMPX_SelectiveDecryptionInit(_p);
	case GF_IPMPX_AUDIO_WM_INIT_TAG: 
	case GF_IPMPX_VIDEO_WM_INIT_TAG: 
		return SizeGF_IPMPX_WatermarkingInit(_p);
	case GF_IPMPX_AUDIO_WM_SEND_TAG: 
	case GF_IPMPX_VIDEO_WM_SEND_TAG: 
		return SizeGF_IPMPX_SendWatermark(_p);

/*
	case GF_IPMPX_USER_QUERY_TAG: return SizeGF_IPMPX_UserQuery(_p);
	case GF_IPMPX_USER_RESPONSE_TAG: return SizeGF_IPMPX_UserQueryResponse(_p);
*/
	case GF_IPMPX_GET_TOOLS_TAG: return 0;
	default: return GF_BAD_PARAM;
	}
}

GF_Err gf_ipmpx_data_write(GF_BitStream *bs, GF_IPMPX_Data *_p)
{
	GF_Err e;
	if (!_p) return GF_OK;
	e = GF_IPMPX_WriteBase(bs, _p);
	if (e) return e;
	switch (_p->tag) {
	case GF_IPMPX_RIGHTS_DATA_TAG:
	case GF_IPMPX_OPAQUE_DATA_TAG: return WriteGF_IPMPX_OpaqueData(bs, _p);
	case GF_IPMPX_KEY_DATA_TAG: return WriteGF_IPMPX_KeyData(bs, _p);
	case GF_IPMPX_SECURE_CONTAINER_TAG: return WriteGF_IPMPX_SecureContainer(bs, _p);
	case GF_IPMPX_ADD_TOOL_LISTENER_TAG: return WriteGF_IPMPX_AddToolNotificationListener(bs, _p);
	case GF_IPMPX_REMOVE_TOOL_LISTENER_TAG: return WriteGF_IPMPX_RemoveToolNotificationListener(bs, _p);
	case GF_IPMPX_INIT_AUTHENTICATION_TAG: return WriteGF_IPMPX_InitAuthentication(bs, _p);
	case GF_IPMPX_MUTUAL_AUTHENTICATION_TAG: return WriteGF_IPMPX_MutualAuthentication(bs, _p);
	case GF_IPMPX_PARAMETRIC_DESCRIPTION_TAG: return WriteGF_IPMPX_ParametricDescription(bs, _p);
	case GF_IPMPX_PARAMETRIC_CAPS_QUERY_TAG: return WriteGF_IPMPX_ToolParamCapabilitiesQuery(bs, _p);
	case GF_IPMPX_PARAMETRIC_CAPS_RESPONSE_TAG: return WriteGF_IPMPX_ToolParamCapabilitiesResponse(bs, _p);
	case GF_IPMPX_GET_TOOLS_RESPONSE_TAG: return WriteGF_IPMPX_GetToolsResponse(bs, _p);
	case GF_IPMPX_GET_TOOL_CONTEXT_TAG: return WriteGF_IPMPX_GetToolContext(bs, _p);
	case GF_IPMPX_GET_TOOL_CONTEXT_RESPONSE_TAG: return WriteGF_IPMPX_GetToolContextResponse(bs, _p);
	case GF_IPMPX_CONNECT_TOOL_TAG: return WriteGF_IPMPX_ConnectTool(bs, _p);
	case GF_IPMPX_DISCONNECT_TOOL_TAG: return WriteGF_IPMPX_DisconnectTool(bs, _p);
	case GF_IPMPX_NOTIFY_TOOL_EVENT_TAG: return WriteGF_IPMPX_NotifyToolEvent(bs, _p);
	case GF_IPMPX_CAN_PROCESS_TAG: return WriteGF_IPMPX_CanProcess(bs, _p);
	case GF_IPMPX_TRUST_SECURITY_METADATA_TAG: return WriteGF_IPMPX_TrustSecurityMetadata(bs, _p);
	case GF_IPMPX_TOOL_API_CONFIG_TAG: return WriteGF_IPMPX_ToolAPI_Config(bs, _p);
	case GF_IPMPX_ISMACRYP_TAG: return WriteGF_IPMPX_ISMACryp(bs, _p);
	case GF_IPMPX_SEL_DEC_INIT_TAG: return WriteGF_IPMPX_SelectiveDecryptionInit(bs, _p);
	case GF_IPMPX_AUDIO_WM_INIT_TAG: 
	case GF_IPMPX_VIDEO_WM_INIT_TAG: 
		return WriteGF_IPMPX_WatermarkingInit(bs, _p);
	case GF_IPMPX_AUDIO_WM_SEND_TAG: 
	case GF_IPMPX_VIDEO_WM_SEND_TAG: 
		return WriteGF_IPMPX_SendWatermark(bs, _p);

/*
	case GF_IPMPX_USER_QUERY_TAG: return WriteGF_IPMPX_UserQuery(bs, _p);
	case GF_IPMPX_USER_RESPONSE_TAG: return WriteGF_IPMPX_UserQueryResponse(bs, _p);
*/
	case GF_IPMPX_GET_TOOLS_TAG: return GF_OK;
	default: return GF_BAD_PARAM;
	}
}

#endif /*GPAC_MINIMAL_ODF*/

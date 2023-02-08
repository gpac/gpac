/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2000-2022
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
GF_Err gf_isom_parse_root_box(GF_Box **outBox, GF_BitStream *bs, u32 *box_type, u64 *bytesExpected, Bool progressive_mode)
{
	GF_Err ret;
	u64 start;
	start = gf_bs_get_position(bs);
	ret = gf_isom_box_parse_ex(outBox, bs, 0, GF_TRUE, 0);
	if (ret == GF_ISOM_INCOMPLETE_FILE) {
		if (!*outBox) {
			// We could not even read the box size, we at least need 8 bytes
			*bytesExpected = 8;
			if (box_type) *box_type = 0;
			GF_LOG(progressive_mode ? GF_LOG_DEBUG : GF_LOG_ERROR, GF_LOG_CONTAINER, ("[iso file] Incomplete box - start "LLU"\n", start));
		}
		else {
			u32 type = (*outBox)->type;
			if (type==GF_ISOM_BOX_TYPE_UNKNOWN)
				type = ((GF_UnknownBox *) (*outBox))->original_4cc;

			*bytesExpected = (*outBox)->size;
			if (box_type) *box_type = (*outBox)->type;

			GF_LOG(progressive_mode ? GF_LOG_DEBUG : GF_LOG_ERROR, GF_LOG_CONTAINER, ("[iso file] Incomplete box %s - start "LLU" size "LLU"\n", gf_4cc_to_str(type), start, (*outBox)->size));
			gf_isom_box_del(*outBox);
			*outBox = NULL;
		}
		gf_bs_seek(bs, start);
	}
	return ret;
}

u32 gf_isom_solve_uuid_box(u8 *UUID)
{
	u32 i;
	char strUUID[33], strChar[3];
	strUUID[0] = 0;
	strUUID[32] = 0;
	for (i=0; i<16; i++) {
		snprintf(strChar, 3, "%02X", (unsigned char) UUID[i]);
		strcat(strUUID, strChar);
	}
	if (!strnicmp(strUUID, "8974dbce7be74c5184f97148f9882554", 32))
		return GF_ISOM_BOX_UUID_TENC;
	if (!strnicmp(strUUID, "A5D40B30E81411DDBA2F0800200C9A66", 32))
		return GF_ISOM_BOX_UUID_MSSM;
	if (!strnicmp(strUUID, "D4807EF2CA3946958E5426CB9E46A79F", 32))
		return GF_ISOM_BOX_UUID_TFRF;
	if (!strnicmp(strUUID, "6D1D9B0542D544E680E2141DAFF757B2", 32))
		return GF_ISOM_BOX_UUID_TFXD;
	if (!strnicmp(strUUID, "A2394F525A9B4F14A2446C427C648DF4", 32))
		return GF_ISOM_BOX_UUID_PSEC;
	if (!strnicmp(strUUID, "D08A4F1810F34A82B6C832D8ABA183D3", 32))
		return GF_ISOM_BOX_UUID_PSSH;

	return 0;
}

static GF_Err gf_isom_full_box_read(GF_Box *ptr, GF_BitStream *bs);

u64 unused_bytes = 0;

#define GF_SKIP_BOX 10

GF_Err gf_isom_box_parse_ex(GF_Box **outBox, GF_BitStream *bs, u32 parent_type, Bool is_root_box, u64 parent_size)
{
	u32 type, uuid_type, hdr_size, restore_type;
	u64 size, start, comp_start, end;
	char uuid[16];
	GF_Err e;
	GF_BitStream *uncomp_bs = NULL;
	u8 *uncomp_data = NULL;
	u32 compressed_size=0;
	GF_Box *newBox;
	Bool skip_logs = (gf_bs_get_cookie(bs) & GF_ISOM_BS_COOKIE_NO_LOGS ) ? GF_TRUE : GF_FALSE;
	Bool is_special = GF_TRUE;

	if ((bs == NULL) || (outBox == NULL) ) return GF_BAD_PARAM;
	*outBox = NULL;
	if (gf_bs_available(bs) < 8) {
		return GF_ISOM_INCOMPLETE_FILE;
	}

	comp_start = start = gf_bs_get_position(bs);

	uuid_type = 0;
	size = (u64) gf_bs_read_u32(bs);
	hdr_size = 4;
	/*fix for some boxes found in some old hinted files*/
	if ((size >= 2) && (size <= 4)) {
		size = 4;
		type = GF_ISOM_BOX_TYPE_VOID;
	} else {
		type = gf_bs_read_u32(bs);
		hdr_size += 4;
		/*no size means till end of file - EXCEPT FOR some old QuickTime boxes...*/
		if (type == GF_ISOM_BOX_TYPE_TOTL)
			size = 12;
		if (!size) {
			if (is_root_box) {
				if (!skip_logs) {
					GF_LOG(GF_LOG_DEBUG, GF_LOG_CONTAINER, ("[iso file] Warning Read Box type %s (0x%08X) size 0 reading till the end of file\n", gf_4cc_to_str(type), type));
				}
				size = gf_bs_available(bs) + 8;
			} else {
				if (!skip_logs) {
					GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[iso file] Read Box type %s (0x%08X) at position "LLU" has size 0 but is not at root/file level. Forbidden, skipping end of parent box !\n", gf_4cc_to_str(type), type, start));
					return GF_SKIP_BOX;
				}
				return GF_OK;
			}
		}
		if (is_root_box && (size>=8)) {
			Bool do_uncompress = GF_FALSE;
			u8 *compb = NULL;
			u32 osize = 0;
			u32 otype = type;
			if (type==GF_4CC('!', 'm', 'o', 'f')) {
				do_uncompress = GF_TRUE;
				type = GF_ISOM_BOX_TYPE_MOOF;
			}
			else if (type==GF_4CC('!', 'm', 'o', 'v')) {
				do_uncompress = GF_TRUE;
				type = GF_ISOM_BOX_TYPE_MOOV;
			}
			else if (type==GF_4CC('!', 's', 'i', 'x')) {
				do_uncompress = GF_TRUE;
				type = GF_ISOM_BOX_TYPE_SIDX;
			}
			else if (type==GF_4CC('!', 's', 's', 'x')) {
				do_uncompress = GF_TRUE;
				type = GF_ISOM_BOX_TYPE_SSIX;
			}

			if (do_uncompress) {
				compb = gf_malloc((u32) (size-8));
				if (!compb) return GF_OUT_OF_MEM;

				compressed_size = (u32) (size - 8);
				gf_bs_read_data(bs, compb, compressed_size);
				e = gf_gz_decompress_payload(compb, compressed_size, &uncomp_data, &osize);
				if (e) {
					gf_free(compb);
					GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[iso file] Failed to uncompress payload for box type %s (0x%08X)\n", gf_4cc_to_str(otype), otype));
					return e;
				}

				//keep size as complete box size for tests below
				size = osize + 8;
				uncomp_bs = gf_bs_new(uncomp_data, osize, GF_BITSTREAM_READ);
				bs = uncomp_bs;
				start = 0;
				gf_free(compb);
			}
		}
	}

#define ERR_EXIT(_e) { \
		if (uncomp_bs) {\
			gf_free(uncomp_data);\
			gf_bs_del(uncomp_bs); \
		}\
		return _e;\
	}

	/*handle uuid*/
	memset(uuid, 0, 16);
	if (type == GF_ISOM_BOX_TYPE_UUID ) {
		if (gf_bs_available(bs) < 16) {
			ERR_EXIT(GF_ISOM_INCOMPLETE_FILE);
		}
		gf_bs_read_data(bs, uuid, 16);
		hdr_size += 16;
		uuid_type = gf_isom_solve_uuid_box(uuid);
	}

	//handle large box
	if (size == 1) {
		if (gf_bs_available(bs) < 8) {
			ERR_EXIT(GF_ISOM_INCOMPLETE_FILE);
		}
		size = gf_bs_read_u64(bs);
		hdr_size += 8;
	}
	if (!skip_logs)
		GF_LOG(GF_LOG_DEBUG, GF_LOG_CONTAINER, ("[iso file] Read Box type %s size "LLD" start "LLD"\n", gf_4cc_to_str(type), size,  start));

	if ( size < hdr_size ) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[iso file] Box %s size "LLD" less than box header size %d\n", gf_4cc_to_str(type), size, hdr_size));
		ERR_EXIT(GF_ISOM_INVALID_FILE);
	}
	//if parent size is given, make sure box fits within parent
	if (parent_size && (parent_size<size)) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[iso file] Box %s size "LLU" is larger than remaining parent size "LLU"\n", gf_4cc_to_str(type), size, parent_size ));
		ERR_EXIT(GF_ISOM_INVALID_FILE);
	}
	restore_type = 0;
	if ((parent_type==GF_ISOM_BOX_TYPE_STSD) && (type==GF_QT_SUBTYPE_RAW) ) {
		u64 cookie = gf_bs_get_cookie(bs);
		restore_type = type;
		if (cookie & GF_ISOM_BS_COOKIE_VISUAL_TRACK)
			type = GF_QT_SUBTYPE_RAW_VID;
		else
			type = GF_QT_SUBTYPE_RAW_AUD;

	}

	//some special boxes (references and track groups) are handled by a single generic box with an associated ref/group type
	if (parent_type && (parent_type == GF_ISOM_BOX_TYPE_TREF)) {
		newBox = gf_isom_box_new(GF_ISOM_BOX_TYPE_REFT);
		if (!newBox) ERR_EXIT(GF_OUT_OF_MEM);
		((GF_TrackReferenceTypeBox*)newBox)->reference_type = type;
	} else if (parent_type && (parent_type == GF_ISOM_BOX_TYPE_IREF)) {
		newBox = gf_isom_box_new(GF_ISOM_BOX_TYPE_REFI);
		if (!newBox) ERR_EXIT(GF_OUT_OF_MEM);
		((GF_ItemReferenceTypeBox*)newBox)->reference_type = type;
	} else if (parent_type && (parent_type == GF_ISOM_BOX_TYPE_TRGR)) {
		newBox = gf_isom_box_new(GF_ISOM_BOX_TYPE_TRGT);
		if (!newBox) ERR_EXIT(GF_OUT_OF_MEM);
		((GF_TrackGroupTypeBox*)newBox)->group_type = type;
	} else if (parent_type && (parent_type == GF_ISOM_BOX_TYPE_GRPL)) {
		newBox = gf_isom_box_new(GF_ISOM_BOX_TYPE_GRPT);
		if (!newBox) ERR_EXIT(GF_OUT_OF_MEM);
		((GF_EntityToGroupTypeBox*)newBox)->grouping_type = type;
	} else {
		//OK, create the box based on the type
		is_special = GF_FALSE;
		newBox = gf_isom_box_new_ex(uuid_type ? uuid_type : type, parent_type, skip_logs, is_root_box);
		if (!newBox) ERR_EXIT(GF_OUT_OF_MEM);
	}

	//OK, init and read this box
	if (type==GF_ISOM_BOX_TYPE_UUID && !is_special) {
		memcpy(((GF_UUIDBox *)newBox)->uuid, uuid, 16);
		((GF_UUIDBox *)newBox)->internal_4cc = uuid_type;
	}

	if (!newBox->type) newBox->type = type;
	if (restore_type)
		newBox->type = restore_type;

	end = gf_bs_available(bs);
	if (size - hdr_size > end ) {
		newBox->size = size - hdr_size - end;
		*outBox = newBox;
		ERR_EXIT(GF_ISOM_INCOMPLETE_FILE);
	}

	newBox->size = size - hdr_size;

	e = gf_isom_full_box_read(newBox, bs);
	if (!e) e = gf_isom_box_read(newBox, bs);
	if (e) {
		if (gf_opts_get_bool("core", "no-check"))
			e = GF_OK;
	}
	newBox->size = size;
	end = gf_bs_get_position(bs);

	if (uncomp_bs) {
		gf_free(uncomp_data);
		gf_bs_del(uncomp_bs);
		if (e) {
			gf_isom_box_del(newBox);
			*outBox = NULL;
			return e;
		}
		//move size to real bitstream offsets for tests below
		size -= 8;
		//remember compressed vs real size info for moof in order to properly recompute data_offset/base_data_offset
		if (type==GF_ISOM_BOX_TYPE_MOOF) {
			((GF_MovieFragmentBox *)newBox)->compressed_diff = (s32)size - (s32)compressed_size;
		}
		//remember compressed vs real size info for moov in order to properly recompute chunk offset
		else if (type==GF_ISOM_BOX_TYPE_MOOV) {
			((GF_MovieBox *)newBox)->compressed_diff = (s32)size - (s32)compressed_size;
			((GF_MovieBox *)newBox)->file_offset = comp_start;
		}
		//remember compressed vs real size info for dump
		else if (type==GF_ISOM_BOX_TYPE_SIDX) {
			((GF_SegmentIndexBox *)newBox)->compressed_diff = (s32)size - (s32)compressed_size;
		}
		//remember compressed vs real size info for dump
		else if (type==GF_ISOM_BOX_TYPE_SSIX) {
			((GF_SubsegmentIndexBox *)newBox)->compressed_diff = (s32)size - (s32)compressed_size;
		}
		newBox->internal_flags = GF_ISOM_BOX_COMPRESSED;
	}


	if (e && (e != GF_ISOM_INCOMPLETE_FILE)) {
		gf_isom_box_del(newBox);
		*outBox = NULL;
		if (is_root_box && (e==GF_SKIP_BOX))
			e = GF_ISOM_INVALID_FILE;

		if (!skip_logs && (e!=GF_SKIP_BOX)) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[iso file] Read Box \"%s\" (start "LLU") failed (%s) - skipping\n", gf_4cc_to_str(type), start, gf_error_to_string(e)));
		}
		//we don't try to reparse known boxes that have been failing (too dangerous)
		return e;
	}

	if (end-start > size) {
		if (!skip_logs) {
			GF_LOG(GF_LOG_WARNING, GF_LOG_CONTAINER, ("[iso file] Box \"%s\" size "LLU" (start "LLU") invalid (read "LLU")\n", gf_4cc_to_str(type), size, start, (end-start) ));
		}
		/*let's still try to load the file since no error was notified*/
		gf_bs_seek(bs, start+size);
	} else if (end-start < size) {
		u32 to_skip = (u32) (size-(end-start));
		if (!skip_logs) {
			if ((to_skip!=4) || gf_bs_peek_bits(bs, 32, 0)) {
				GF_LOG(GF_LOG_WARNING, GF_LOG_CONTAINER, ("[iso file] Box \"%s\" (start "LLU") has %u extra bytes\n", gf_4cc_to_str(type), start, to_skip));
				unused_bytes += to_skip;
			}
		}
		gf_bs_skip_bytes(bs, to_skip);
	}
	*outBox = newBox;

	return e;
}

GF_EXPORT
GF_Err gf_isom_box_parse(GF_Box **outBox, GF_BitStream *bs)
{
	return gf_isom_box_parse_ex(outBox, bs, 0, GF_FALSE, 0);
}

void gf_isom_box_array_reset(GF_List *boxlist)
{
	u32 count, i;
	if (!boxlist) return;
	count = gf_list_count(boxlist);
	for (i = 0; i < count; i++) {
		GF_Box *a = (GF_Box *)gf_list_get(boxlist, i);
		if (a) gf_isom_box_del(a);
	}
}

void gf_isom_box_array_del(GF_List *boxlist)
{
	gf_isom_box_array_reset(boxlist);
	gf_list_del(boxlist);
}

void gf_isom_box_array_reset_parent(GF_List **child_boxes, GF_List *boxlist)
{
	u32 count, i;
	if (!boxlist) return;
	count = gf_list_count(boxlist);
	for (i = 0; i < count; i++) {
		GF_Box *a = (GF_Box *)gf_list_get(boxlist, i);
		if (a) gf_isom_box_del_parent(child_boxes, a);
	}
	gf_list_reset(boxlist);
}
void gf_isom_box_array_del_parent(GF_List **child_boxes, GF_List *boxlist)
{
	if (!boxlist) return;
	gf_isom_box_array_reset_parent(child_boxes, boxlist);
	gf_list_del(boxlist);
}


#ifndef GPAC_DISABLE_ISOM_WRITE

GF_EXPORT
GF_Err gf_isom_box_write_header(GF_Box *ptr, GF_BitStream *bs)
{
	u64 start;
	if (! bs || !ptr) return GF_BAD_PARAM;
	if (!ptr->size) return GF_ISOM_INVALID_FILE;

	start = gf_bs_get_position(bs);
	if (ptr->size > 0xFFFFFFFF) {
		gf_bs_write_u32(bs, 1);
	} else {
		gf_bs_write_u32(bs, (u32) ptr->size);
	}
	gf_bs_write_u32(bs, ptr->type);
	if (ptr->type == GF_ISOM_BOX_TYPE_UUID) {
		u32 i;
		Bool conv_uuid = GF_TRUE;
		GF_UUIDBox *uuidb = (GF_UUIDBox *)ptr;
		char strUUID[32];

		switch (uuidb->internal_4cc) {
		case GF_ISOM_BOX_UUID_TENC:
			memcpy(strUUID, "8974dbce7be74c5184f97148f9882554", 32);
			break;
		case GF_ISOM_BOX_UUID_PSEC:
			memcpy(strUUID, "A2394F525A9B4F14A2446C427C648DF4", 32);
			break;
		case GF_ISOM_BOX_UUID_MSSM:
			memcpy(strUUID, "A5D40B30E81411DDBA2F0800200C9A66", 32);
			break;
		case GF_ISOM_BOX_UUID_PSSH:
			memcpy(strUUID, "D08A4F1810F34A82B6C832D8ABA183D3", 32);
			break;
		case GF_ISOM_BOX_UUID_TFXD:
			memcpy(strUUID, "6D1D9B0542D544E680E2141DAFF757B2", 32);
			break;
		default:
			conv_uuid = GF_FALSE;
			break;
		}

		if (conv_uuid) {
			char uuid[16];
			for (i = 0; i < 16; i++) {
				char t[3];
				t[2] = 0;
				t[0] = strUUID[2*i];
				t[1] = strUUID[2*i+1];
				uuid[i] = (u8) strtol(t, NULL, 16);
			}
			gf_bs_write_data(bs, uuid, 16);
		} else {
			gf_bs_write_data(bs, uuidb->uuid, 16);
		}
	}
	if (ptr->size > 0xFFFFFFFF)
		gf_bs_write_u64(bs, ptr->size);

	GF_LOG(GF_LOG_DEBUG, GF_LOG_CONTAINER, ("[iso file] Written Box type %s size "LLD" start "LLD"\n", gf_4cc_to_str(ptr->type), ptr->size, start));

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
	GF_Err e;
	if (!list) return GF_OK;
	count = gf_list_count(list);
	for (i = 0; i < count; i++) {
		GF_Box *a = (GF_Box *)gf_list_get(list, i);
		if (a) {
			e = gf_isom_box_write(a, bs);
			if (e) {
				GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("ISOBMF: Error %s writing box %s\n", gf_error_to_string(e), gf_4cc_to_str(a->type) ));
				return e;
			}
		}
	}
	return GF_OK;
}


GF_Err gf_isom_box_array_size(GF_Box *parent, GF_List *list)
{
	GF_Err e;
	u32 count, i;
	if (! list) return GF_OK;

	count = gf_list_count(list);
	for (i = 0; i < count; i++) {
		GF_Box *a = (GF_Box *)gf_list_get(list, i);
		if (a) {
			e = gf_isom_box_size(a);
			if (e) {
				GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("ISOBMF: Error %s computing box %s size\n", gf_error_to_string(e), gf_4cc_to_str(a->type) ));
				return e;
			}
			parent->size += a->size;
		}
	}
	return GF_OK;
}

#endif /*GPAC_DISABLE_ISOM_WRITE*/



GF_Box * unkn_box_new();
void unkn_box_del(GF_Box *);
GF_Err unkn_box_read(GF_Box *s, GF_BitStream *bs);
GF_Err unkn_box_write(GF_Box *s, GF_BitStream *bs);
GF_Err unkn_box_size(GF_Box *s);
GF_Err unkn_box_dump(GF_Box *a, FILE * trace);

//definition of boxes new/del/read/write/size. For now still exported since some files other than box_funcs.c call them
//this should be fixed by only using gf_isom_box_new

#define ISOM_BOX_IMPL_DECL(a_name) \
		GF_Box * a_name##_box_new(); \
		void a_name##_box_del(GF_Box *); \
		GF_Err a_name##_box_read(GF_Box *s, GF_BitStream *bs); \
		GF_Err a_name##_box_write(GF_Box *s, GF_BitStream *bs); \
		GF_Err a_name##_box_size(GF_Box *s);\
		GF_Err a_name##_box_dump(GF_Box *a, FILE * trace);

#define ISOM_BOX_IMPL_DECL_CHILD(a_name) \
		GF_Box * a_name##_box_new(); \
		void a_name##_box_del(GF_Box *); \
		GF_Err a_name##_box_read(GF_Box *s, GF_BitStream *bs); \
		GF_Err a_name##_box_write(GF_Box *s, GF_BitStream *bs); \
		GF_Err a_name##_box_size(GF_Box *s);\
		GF_Err a_name##_box_dump(GF_Box *a, FILE * trace);\
		GF_Err a_name##_on_child_box(GF_Box *s, GF_Box *a, Bool is_rem);\


ISOM_BOX_IMPL_DECL(reftype)
ISOM_BOX_IMPL_DECL(ireftype)
ISOM_BOX_IMPL_DECL(free)
ISOM_BOX_IMPL_DECL(wide)
ISOM_BOX_IMPL_DECL(mdat)
ISOM_BOX_IMPL_DECL(imda)
ISOM_BOX_IMPL_DECL_CHILD(moov)
ISOM_BOX_IMPL_DECL(mvhd)
ISOM_BOX_IMPL_DECL(mdhd)
ISOM_BOX_IMPL_DECL(vmhd)
ISOM_BOX_IMPL_DECL(smhd)
ISOM_BOX_IMPL_DECL(hmhd)
ISOM_BOX_IMPL_DECL(nmhd)
ISOM_BOX_IMPL_DECL_CHILD(stbl)
ISOM_BOX_IMPL_DECL_CHILD(dinf)
ISOM_BOX_IMPL_DECL(url)
ISOM_BOX_IMPL_DECL(urn)
ISOM_BOX_IMPL_DECL(cprt)
ISOM_BOX_IMPL_DECL(kind)
ISOM_BOX_IMPL_DECL(chpl)
ISOM_BOX_IMPL_DECL(hdlr)
ISOM_BOX_IMPL_DECL(iods)
ISOM_BOX_IMPL_DECL_CHILD(trak)
ISOM_BOX_IMPL_DECL_CHILD(mp4s)
ISOM_BOX_IMPL_DECL_CHILD(audio_sample_entry)
ISOM_BOX_IMPL_DECL(gen_sample_entry)
ISOM_BOX_IMPL_DECL_CHILD(edts)
ISOM_BOX_IMPL_DECL_CHILD(udta)
ISOM_BOX_IMPL_DECL(dref)
ISOM_BOX_IMPL_DECL_CHILD(stsd)
ISOM_BOX_IMPL_DECL(stts)
ISOM_BOX_IMPL_DECL(ctts)
ISOM_BOX_IMPL_DECL(stsh)
ISOM_BOX_IMPL_DECL(elst)
ISOM_BOX_IMPL_DECL(stsc)
ISOM_BOX_IMPL_DECL(stsz)
ISOM_BOX_IMPL_DECL(stco)
ISOM_BOX_IMPL_DECL(stss)
ISOM_BOX_IMPL_DECL(stdp)
ISOM_BOX_IMPL_DECL(sdtp)
ISOM_BOX_IMPL_DECL(co64)
ISOM_BOX_IMPL_DECL(esds)
ISOM_BOX_IMPL_DECL_CHILD(minf)
ISOM_BOX_IMPL_DECL(tkhd)
ISOM_BOX_IMPL_DECL(tref)
ISOM_BOX_IMPL_DECL_CHILD(mdia)
ISOM_BOX_IMPL_DECL_CHILD(mfra)
ISOM_BOX_IMPL_DECL(tfra)
ISOM_BOX_IMPL_DECL(mfro)
ISOM_BOX_IMPL_DECL(uuid)
ISOM_BOX_IMPL_DECL(void)
ISOM_BOX_IMPL_DECL(gnrm)
ISOM_BOX_IMPL_DECL(gnrv)
ISOM_BOX_IMPL_DECL(gnra)
ISOM_BOX_IMPL_DECL(pdin)
ISOM_BOX_IMPL_DECL(def_parent)
ISOM_BOX_IMPL_DECL(def_parent_full)
ISOM_BOX_IMPL_DECL(csgp)


#ifndef GPAC_DISABLE_ISOM_HINTING

ISOM_BOX_IMPL_DECL_CHILD(hinf)
ISOM_BOX_IMPL_DECL(trpy)
ISOM_BOX_IMPL_DECL(totl)
ISOM_BOX_IMPL_DECL(nump)
ISOM_BOX_IMPL_DECL(npck)
ISOM_BOX_IMPL_DECL(tpyl)
ISOM_BOX_IMPL_DECL(tpay)
ISOM_BOX_IMPL_DECL(maxr)
ISOM_BOX_IMPL_DECL(dmed)
ISOM_BOX_IMPL_DECL(dimm)
ISOM_BOX_IMPL_DECL(drep)
ISOM_BOX_IMPL_DECL(tmin)
ISOM_BOX_IMPL_DECL(tmax)
ISOM_BOX_IMPL_DECL(pmax)
ISOM_BOX_IMPL_DECL(dmax)
ISOM_BOX_IMPL_DECL(payt)
ISOM_BOX_IMPL_DECL(name)
ISOM_BOX_IMPL_DECL(rely)
ISOM_BOX_IMPL_DECL(snro)
ISOM_BOX_IMPL_DECL(tims)
ISOM_BOX_IMPL_DECL(tsro)
ISOM_BOX_IMPL_DECL(ghnt)
ISOM_BOX_IMPL_DECL_CHILD(hnti)
ISOM_BOX_IMPL_DECL(sdp)
ISOM_BOX_IMPL_DECL(rtpo)
ISOM_BOX_IMPL_DECL(tssy)
ISOM_BOX_IMPL_DECL(rssr)
ISOM_BOX_IMPL_DECL_CHILD(srpp)
ISOM_BOX_IMPL_DECL(rtp_hnti)

#endif

ISOM_BOX_IMPL_DECL(ftyp)
ISOM_BOX_IMPL_DECL(padb)
ISOM_BOX_IMPL_DECL(gppc)


#ifndef	GPAC_DISABLE_ISOM_FRAGMENTS
ISOM_BOX_IMPL_DECL_CHILD(mvex)
ISOM_BOX_IMPL_DECL(trex)
ISOM_BOX_IMPL_DECL_CHILD(moof)
ISOM_BOX_IMPL_DECL(mfhd)
ISOM_BOX_IMPL_DECL_CHILD(traf)
ISOM_BOX_IMPL_DECL(tfhd)
ISOM_BOX_IMPL_DECL(trun)
ISOM_BOX_IMPL_DECL(styp)
ISOM_BOX_IMPL_DECL(mehd)
/*smooth streaming timing*/
ISOM_BOX_IMPL_DECL(tfxd)
ISOM_BOX_IMPL_DECL(tfrf)

#endif

/*avc ext*/
ISOM_BOX_IMPL_DECL(avcc)
ISOM_BOX_IMPL_DECL_CHILD(video_sample_entry)
ISOM_BOX_IMPL_DECL(m4ds)
ISOM_BOX_IMPL_DECL(btrt)
ISOM_BOX_IMPL_DECL(mehd)

/*3GPP streaming text*/
ISOM_BOX_IMPL_DECL(ftab)
ISOM_BOX_IMPL_DECL_CHILD(tx3g)
ISOM_BOX_IMPL_DECL(text)
ISOM_BOX_IMPL_DECL(styl)
ISOM_BOX_IMPL_DECL(hlit)
ISOM_BOX_IMPL_DECL(hclr)
ISOM_BOX_IMPL_DECL(krok)
ISOM_BOX_IMPL_DECL(dlay)
ISOM_BOX_IMPL_DECL(href)
ISOM_BOX_IMPL_DECL(tbox)
ISOM_BOX_IMPL_DECL(blnk)
ISOM_BOX_IMPL_DECL(twrp)


#ifndef GPAC_DISABLE_VTT

/*WebVTT boxes*/
ISOM_BOX_IMPL_DECL(boxstring);
ISOM_BOX_IMPL_DECL_CHILD(vtcu)
ISOM_BOX_IMPL_DECL(vtte)
ISOM_BOX_IMPL_DECL_CHILD(wvtt)

#endif //GPAC_DISABLE_VTT

/* Items functions */
ISOM_BOX_IMPL_DECL_CHILD(meta)
ISOM_BOX_IMPL_DECL(xml)
ISOM_BOX_IMPL_DECL(bxml)
ISOM_BOX_IMPL_DECL(iloc)
ISOM_BOX_IMPL_DECL(pitm)
ISOM_BOX_IMPL_DECL_CHILD(ipro)
ISOM_BOX_IMPL_DECL(infe)
ISOM_BOX_IMPL_DECL_CHILD(iinf)
ISOM_BOX_IMPL_DECL_CHILD(iref)
ISOM_BOX_IMPL_DECL_CHILD(sinf)
ISOM_BOX_IMPL_DECL(frma)
ISOM_BOX_IMPL_DECL(schm)
ISOM_BOX_IMPL_DECL_CHILD(schi)
ISOM_BOX_IMPL_DECL(enca)
ISOM_BOX_IMPL_DECL(encs)
ISOM_BOX_IMPL_DECL(encv)
ISOM_BOX_IMPL_DECL(resv)


/** ISMACryp functions **/
ISOM_BOX_IMPL_DECL(iKMS)
ISOM_BOX_IMPL_DECL(iSFM)
ISOM_BOX_IMPL_DECL(iSLT)

#ifndef GPAC_DISABLE_ISOM_ADOBE
/* Adobe extensions */
ISOM_BOX_IMPL_DECL(abst)
ISOM_BOX_IMPL_DECL(afra)
ISOM_BOX_IMPL_DECL(asrt)
ISOM_BOX_IMPL_DECL(afrt)
#endif /*GPAC_DISABLE_ISOM_ADOBE*/

/* Apple extensions */
ISOM_BOX_IMPL_DECL(ilst)
ISOM_BOX_IMPL_DECL(ilst_item)
ISOM_BOX_IMPL_DECL(databox)
ISOM_BOX_IMPL_DECL(gmin)
ISOM_BOX_IMPL_DECL(alis)
ISOM_BOX_IMPL_DECL(clef)

/*OMA extensions*/
ISOM_BOX_IMPL_DECL(ohdr)
ISOM_BOX_IMPL_DECL(grpi)
ISOM_BOX_IMPL_DECL(mdri)
ISOM_BOX_IMPL_DECL(odtt)
ISOM_BOX_IMPL_DECL(odrb)
ISOM_BOX_IMPL_DECL(odkm)


ISOM_BOX_IMPL_DECL(pasp)
ISOM_BOX_IMPL_DECL(clap)
ISOM_BOX_IMPL_DECL_CHILD(metx)
ISOM_BOX_IMPL_DECL(txtc)
ISOM_BOX_IMPL_DECL(tsel)
ISOM_BOX_IMPL_DECL(dimC)
ISOM_BOX_IMPL_DECL_CHILD(dims)
ISOM_BOX_IMPL_DECL(diST)
ISOM_BOX_IMPL_DECL(ac3)
ISOM_BOX_IMPL_DECL(ec3)
ISOM_BOX_IMPL_DECL(dac3)
ISOM_BOX_IMPL_DECL(dec3)
ISOM_BOX_IMPL_DECL(dmlp)
ISOM_BOX_IMPL_DECL(lsrc)
ISOM_BOX_IMPL_DECL_CHILD(lsr1)
ISOM_BOX_IMPL_DECL(mvcg)
ISOM_BOX_IMPL_DECL(vwid)

ISOM_BOX_IMPL_DECL(subs)

ISOM_BOX_IMPL_DECL(tmcd)
ISOM_BOX_IMPL_DECL(tcmi)
ISOM_BOX_IMPL_DECL(fiel)
ISOM_BOX_IMPL_DECL(gama)
ISOM_BOX_IMPL_DECL(chrm)
ISOM_BOX_IMPL_DECL(chan)


#ifndef GPAC_DISABLE_ISOM_FRAGMENTS
ISOM_BOX_IMPL_DECL(sidx)
ISOM_BOX_IMPL_DECL(ssix)
ISOM_BOX_IMPL_DECL(leva)
ISOM_BOX_IMPL_DECL(pcrb)
ISOM_BOX_IMPL_DECL(tfdt)
ISOM_BOX_IMPL_DECL(emsg)

#endif

ISOM_BOX_IMPL_DECL(rvcc)
ISOM_BOX_IMPL_DECL(sbgp)
ISOM_BOX_IMPL_DECL(sgpd)
ISOM_BOX_IMPL_DECL(saiz)
ISOM_BOX_IMPL_DECL(saio)

ISOM_BOX_IMPL_DECL(pssh)

ISOM_BOX_IMPL_DECL(tenc)
ISOM_BOX_IMPL_DECL(piff_tenc)
ISOM_BOX_IMPL_DECL(piff_psec)
ISOM_BOX_IMPL_DECL(piff_pssh)
ISOM_BOX_IMPL_DECL(senc)
ISOM_BOX_IMPL_DECL(cslg)
ISOM_BOX_IMPL_DECL(ccst)
ISOM_BOX_IMPL_DECL(auxi)
ISOM_BOX_IMPL_DECL(hvcc)
ISOM_BOX_IMPL_DECL(av1c)
ISOM_BOX_IMPL_DECL(dOps)
ISOM_BOX_IMPL_DECL(prft)
ISOM_BOX_IMPL_DECL(vvcc)
ISOM_BOX_IMPL_DECL(vvnc)

//VPx
ISOM_BOX_IMPL_DECL(vpcc)
ISOM_BOX_IMPL_DECL(SmDm)
ISOM_BOX_IMPL_DECL(CoLL)

ISOM_BOX_IMPL_DECL(trep)

//FEC
ISOM_BOX_IMPL_DECL_CHILD(fiin)
ISOM_BOX_IMPL_DECL_CHILD(paen)
ISOM_BOX_IMPL_DECL(fpar)
ISOM_BOX_IMPL_DECL(fecr)
ISOM_BOX_IMPL_DECL(segr)
ISOM_BOX_IMPL_DECL(gitn)
ISOM_BOX_IMPL_DECL_CHILD(fdsa)
ISOM_BOX_IMPL_DECL(fdpa)
ISOM_BOX_IMPL_DECL(extr)


/*
	Adobe's protection boxes
*/
ISOM_BOX_IMPL_DECL_CHILD(adkm)
ISOM_BOX_IMPL_DECL_CHILD(ahdr)
ISOM_BOX_IMPL_DECL_CHILD(aprm)
ISOM_BOX_IMPL_DECL(aeib)
ISOM_BOX_IMPL_DECL_CHILD(akey)
ISOM_BOX_IMPL_DECL(flxs)
ISOM_BOX_IMPL_DECL(adaf)

/* Image File Format declarations */
ISOM_BOX_IMPL_DECL(ispe)
ISOM_BOX_IMPL_DECL(colr)
ISOM_BOX_IMPL_DECL(pixi)
ISOM_BOX_IMPL_DECL(rloc)
ISOM_BOX_IMPL_DECL(irot)
ISOM_BOX_IMPL_DECL(imir)
ISOM_BOX_IMPL_DECL(ipco)
ISOM_BOX_IMPL_DECL_CHILD(iprp)
ISOM_BOX_IMPL_DECL(ipma)
ISOM_BOX_IMPL_DECL_CHILD(trgr)
ISOM_BOX_IMPL_DECL(trgt)
ISOM_BOX_IMPL_DECL(ienc)
ISOM_BOX_IMPL_DECL(iaux)

/* MIAF declarations */
ISOM_BOX_IMPL_DECL(clli)
ISOM_BOX_IMPL_DECL(mdcv)

/* AVIF declarations */
ISOM_BOX_IMPL_DECL(a1lx)
ISOM_BOX_IMPL_DECL(a1op)

ISOM_BOX_IMPL_DECL(grpl)

ISOM_BOX_IMPL_DECL_CHILD(strk)
ISOM_BOX_IMPL_DECL(stri)
ISOM_BOX_IMPL_DECL(stsg)
ISOM_BOX_IMPL_DECL(elng)
ISOM_BOX_IMPL_DECL(stvi)
ISOM_BOX_IMPL_DECL(auxc)
ISOM_BOX_IMPL_DECL(oinf)
ISOM_BOX_IMPL_DECL(tols)

ISOM_BOX_IMPL_DECL(trik)
ISOM_BOX_IMPL_DECL(bloc)
ISOM_BOX_IMPL_DECL(ainf)
ISOM_BOX_IMPL_DECL(mhac)
ISOM_BOX_IMPL_DECL(mhap)

ISOM_BOX_IMPL_DECL(grptype)

ISOM_BOX_IMPL_DECL_CHILD(jp2h)
ISOM_BOX_IMPL_DECL(ihdr)
ISOM_BOX_IMPL_DECL(load)

/* Dolby Vision */
ISOM_BOX_IMPL_DECL(dvcC)
ISOM_BOX_IMPL_DECL(dvvC)
ISOM_BOX_IMPL_DECL(dvhe)
ISOM_BOX_IMPL_DECL(dfla)

ISOM_BOX_IMPL_DECL(pcmC)
ISOM_BOX_IMPL_DECL(chnl)

ISOM_BOX_IMPL_DECL(xtra)

ISOM_BOX_IMPL_DECL(st3d)
ISOM_BOX_IMPL_DECL(svhd)
ISOM_BOX_IMPL_DECL(prhd)
ISOM_BOX_IMPL_DECL(proj_type)
//ISOM_BOX_IMPL_DECL(mesh)

ISOM_BOX_IMPL_DECL(keys)

#define BOX_DEFINE(__type, b_rad, __par) { __type, b_rad##_box_new, b_rad##_box_del, b_rad##_box_read, b_rad##_box_write, b_rad##_box_size, b_rad##_box_dump, 0, 0, 0, __par, "p12", GF_FALSE}

#define BOX_DEFINE_CHILD(__type, b_rad, __par) { __type, b_rad##_box_new, b_rad##_box_del, b_rad##_box_read, b_rad##_box_write, b_rad##_box_size, b_rad##_box_dump, 0, 0, 0, __par, "p12", GF_FALSE, b_rad##_on_child_box}

#define BOX_DEFINE_S(__type, b_rad, __par, __spec) { __type, b_rad##_box_new, b_rad##_box_del, b_rad##_box_read, b_rad##_box_write, b_rad##_box_size, b_rad##_box_dump, 0, 0, 0, __par, __spec, GF_FALSE }

#define BOX_DEFINE_S_CHILD(__type, b_rad, __par, __spec) { __type, b_rad##_box_new, b_rad##_box_del, b_rad##_box_read, b_rad##_box_write, b_rad##_box_size, b_rad##_box_dump, 0, 0, 0, __par, __spec, GF_FALSE, b_rad##_on_child_box}

#define FBOX_DEFINE(__type, b_rad, __par, __max_v) { __type, b_rad##_box_new, b_rad##_box_del, b_rad##_box_read, b_rad##_box_write, b_rad##_box_size, b_rad##_box_dump, 0, 1+__max_v, 0, __par, "p12", GF_FALSE }

#define FBOX_DEFINE_CHILD(__type, b_rad, __par, __max_v) { __type, b_rad##_box_new, b_rad##_box_del, b_rad##_box_read, b_rad##_box_write, b_rad##_box_size, b_rad##_box_dump, 0, 1+__max_v, 0, __par, "p12", GF_FALSE, b_rad##_on_child_box}

#define FBOX_DEFINE_FLAGS(__type, b_rad, __par, __max_v, flags) { __type, b_rad##_box_new, b_rad##_box_del, b_rad##_box_read, b_rad##_box_write, b_rad##_box_size, b_rad##_box_dump, 0, 1+__max_v, flags, __par, "p12", GF_FALSE }

#define FBOX_DEFINE_FLAGS_S(__type, b_rad, __par, __max_v, flags, __spec) { __type, b_rad##_box_new, b_rad##_box_del, b_rad##_box_read, b_rad##_box_write, b_rad##_box_size, b_rad##_box_dump, 0, 1+__max_v, flags, __par, __spec, GF_FALSE }

#define FBOX_DEFINE_S(__type, b_rad, __par, __max_v, __spec) { __type, b_rad##_box_new, b_rad##_box_del, b_rad##_box_read, b_rad##_box_write, b_rad##_box_size, b_rad##_box_dump, 0, 1+__max_v, 0, __par, __spec, GF_FALSE }

#define FBOX_DEFINE_S_CHILD(__type, b_rad, __par, __max_v, __spec) { __type, b_rad##_box_new, b_rad##_box_del, b_rad##_box_read, b_rad##_box_write, b_rad##_box_size, b_rad##_box_dump, 0, 1+__max_v, 0, __par, __spec, GF_FALSE, b_rad##_on_child_box}

#define TREF_DEFINE(__type, b_rad, __par, __4cc, __spec) { __type, b_rad##_box_new, b_rad##_box_del, b_rad##_box_read, b_rad##_box_write, b_rad##_box_size, b_rad##_box_dump, __4cc, 0, 0, __par, __spec, GF_FALSE }

#define TRGT_DEFINE(__type, b_rad, __par, __4cc, max_version, __spec) { __type, b_rad##_box_new, b_rad##_box_del, b_rad##_box_read, b_rad##_box_write, b_rad##_box_size, b_rad##_box_dump, __4cc, 1+max_version, 0, __par, __spec, GF_FALSE }

#define SGPD_DEFINE(__type, b_rad, __par, __4cc, __spec) { __type, b_rad##_box_new, b_rad##_box_del, b_rad##_box_read, b_rad##_box_write, b_rad##_box_size, b_rad##_box_dump, __4cc, 1, 0, __par, __spec, GF_FALSE }

#define ITUNES_TAG(_val) \
	BOX_DEFINE_S( _val, ilst_item, "ilst data", "apple")

static struct box_registry_entry {
	u32 box_4cc;
	GF_Box * (*new_fn)();
	void (*del_fn)(GF_Box *a);
	GF_Err (*read_fn)(GF_Box *s, GF_BitStream *bs);
	GF_Err (*write_fn)(GF_Box *s, GF_BitStream *bs);
	GF_Err (*size_fn)(GF_Box *a);
	GF_Err (*dump_fn)(GF_Box *a, FILE *trace);
	u32 alt_4cc;//used for sample grouping type and track / item reference types
	u8 max_version_plus_one;
	u32 flags;
	const char *parents_4cc;
	const char *spec;
	Bool disabled;
	GF_Err (*add_rem_fn)(GF_Box *par, GF_Box *b, Bool is_remove);
} box_registry [] =
{
	//DO NOT MOVE THE FIRST ENTRY
	BOX_DEFINE_S(GF_ISOM_BOX_TYPE_UNKNOWN, unkn, "unknown", "unknown"),
	BOX_DEFINE_S(GF_ISOM_BOX_TYPE_UUID, uuid, "unknown", "unknown"),

	//all track reference types
	TREF_DEFINE(GF_ISOM_BOX_TYPE_REFT, reftype, "tref", GF_ISOM_REF_META, "p12"),
	TREF_DEFINE(GF_ISOM_BOX_TYPE_REFT, reftype, "tref", GF_ISOM_REF_HINT, "p12"),
	TREF_DEFINE(GF_ISOM_BOX_TYPE_REFT, reftype, "tref", GF_ISOM_REF_FONT, "p12"),
	TREF_DEFINE(GF_ISOM_BOX_TYPE_REFT, reftype, "tref", GF_ISOM_REF_HIND, "p12"),
	TREF_DEFINE(GF_ISOM_BOX_TYPE_REFT, reftype, "tref", GF_ISOM_REF_VDEP, "p12"),
	TREF_DEFINE(GF_ISOM_BOX_TYPE_REFT, reftype, "tref", GF_ISOM_REF_VPLX, "p12"),
	TREF_DEFINE(GF_ISOM_BOX_TYPE_REFT, reftype, "tref", GF_ISOM_REF_SUBT, "p12"),
	TREF_DEFINE(GF_ISOM_BOX_TYPE_REFT, reftype, "tref", GF_ISOM_REF_THUMB, "p12"),

	TREF_DEFINE(GF_ISOM_BOX_TYPE_REFT, reftype, "tref", GF_ISOM_REF_OD, "p14"),
	TREF_DEFINE(GF_ISOM_BOX_TYPE_REFT, reftype, "tref", GF_ISOM_REF_DECODE, "p14"),
	TREF_DEFINE(GF_ISOM_BOX_TYPE_REFT, reftype, "tref", GF_ISOM_REF_OCR, "p14"),
	TREF_DEFINE(GF_ISOM_BOX_TYPE_REFT, reftype, "tref", GF_ISOM_REF_IPI, "p14"),

	TREF_DEFINE(GF_ISOM_BOX_TYPE_REFT, reftype, "tref", GF_ISOM_REF_BASE, "p15"),
	TREF_DEFINE(GF_ISOM_BOX_TYPE_REFT, reftype, "tref", GF_ISOM_REF_SCAL, "p15"),
	TREF_DEFINE(GF_ISOM_BOX_TYPE_REFT, reftype, "tref", GF_ISOM_REF_TBAS, "p15"),
	TREF_DEFINE(GF_ISOM_BOX_TYPE_REFT, reftype, "tref", GF_ISOM_REF_SABT, "p15"),
	TREF_DEFINE(GF_ISOM_BOX_TYPE_REFT, reftype, "tref", GF_ISOM_REF_OREF, "p15"),

	TREF_DEFINE(GF_ISOM_BOX_TYPE_REFT, reftype, "tref", GF_ISOM_REF_ADDA, "p12"),
	TREF_DEFINE(GF_ISOM_BOX_TYPE_REFT, reftype, "tref", GF_ISOM_REF_ADRC, "p12"),
	TREF_DEFINE(GF_ISOM_BOX_TYPE_REFT, reftype, "tref", GF_ISOM_REF_ILOC, "p12"),
	TREF_DEFINE(GF_ISOM_BOX_TYPE_REFT, reftype, "tref", GF_ISOM_REF_AVCP, "p15"),
	TREF_DEFINE(GF_ISOM_BOX_TYPE_REFT, reftype, "tref", GF_ISOM_REF_SWTO, "p15"),
	TREF_DEFINE(GF_ISOM_BOX_TYPE_REFT, reftype, "tref", GF_ISOM_REF_SWFR, "p15"),

	TREF_DEFINE(GF_ISOM_BOX_TYPE_REFT, reftype, "tref", GF_ISOM_REF_CHAP, "apple"),
	TREF_DEFINE(GF_ISOM_BOX_TYPE_REFT, reftype, "tref", GF_ISOM_REF_TMCD, "apple"),
	TREF_DEFINE(GF_ISOM_BOX_TYPE_REFT, reftype, "tref", GF_ISOM_REF_CDEP, "apple"),
	TREF_DEFINE(GF_ISOM_BOX_TYPE_REFT, reftype, "tref", GF_ISOM_REF_SCPT, "apple"),
	TREF_DEFINE(GF_ISOM_BOX_TYPE_REFT, reftype, "tref", GF_ISOM_REF_SSRC, "apple"),
	TREF_DEFINE(GF_ISOM_BOX_TYPE_REFT, reftype, "tref", GF_ISOM_REF_LYRA, "apple"),

	//all item reference types
	TREF_DEFINE( GF_ISOM_BOX_TYPE_REFI, ireftype, "iref", GF_ISOM_REF_TBAS, "p12"),
	TREF_DEFINE( GF_ISOM_BOX_TYPE_REFI, ireftype, "iref", GF_ISOM_REF_ILOC, "p12"),
	TREF_DEFINE( GF_ISOM_BOX_TYPE_REFI, ireftype, "iref", GF_ISOM_REF_FDEL, "p12"),

	//all sample group descriptions
	SGPD_DEFINE( GF_ISOM_BOX_TYPE_SGPD, sgpd, "stbl traf", GF_ISOM_SAMPLE_GROUP_ROLL, "p12"),
	SGPD_DEFINE( GF_ISOM_BOX_TYPE_SGPD, sgpd, "stbl traf", GF_ISOM_SAMPLE_GROUP_PROL, "p12"),
	SGPD_DEFINE( GF_ISOM_BOX_TYPE_SGPD, sgpd, "stbl traf", GF_ISOM_SAMPLE_GROUP_RAP, "p12"),
	SGPD_DEFINE( GF_ISOM_BOX_TYPE_SGPD, sgpd, "stbl traf", GF_ISOM_SAMPLE_GROUP_SEIG, "p15"),
	SGPD_DEFINE( GF_ISOM_BOX_TYPE_SGPD, sgpd, "stbl traf", GF_ISOM_SAMPLE_GROUP_OINF, "p15"),
	SGPD_DEFINE( GF_ISOM_BOX_TYPE_SGPD, sgpd, "stbl traf", GF_ISOM_SAMPLE_GROUP_LINF, "p15"),
	SGPD_DEFINE( GF_ISOM_BOX_TYPE_SGPD, sgpd, "stbl traf", GF_ISOM_SAMPLE_GROUP_TRIF, "p15"),
	SGPD_DEFINE( GF_ISOM_BOX_TYPE_SGPD, sgpd, "stbl traf", GF_ISOM_SAMPLE_GROUP_NALM, "p15"),
	SGPD_DEFINE( GF_ISOM_BOX_TYPE_SGPD, sgpd, "stbl traf", GF_ISOM_SAMPLE_GROUP_TELE, "p12"),
	SGPD_DEFINE( GF_ISOM_BOX_TYPE_SGPD, sgpd, "stbl traf", GF_ISOM_SAMPLE_GROUP_RASH, "p12"),
	SGPD_DEFINE( GF_ISOM_BOX_TYPE_SGPD, sgpd, "stbl traf", GF_ISOM_SAMPLE_GROUP_ALST, "p12"),
	SGPD_DEFINE( GF_ISOM_BOX_TYPE_SGPD, sgpd, "stbl traf", GF_ISOM_SAMPLE_GROUP_SAP, "p12"),

	SGPD_DEFINE( GF_ISOM_BOX_TYPE_SGPD, sgpd, "stbl traf", GF_ISOM_SAMPLE_GROUP_AVLL, "p15"),
	SGPD_DEFINE( GF_ISOM_BOX_TYPE_SGPD, sgpd, "stbl traf", GF_ISOM_SAMPLE_GROUP_AVSS, "p15"),
	SGPD_DEFINE( GF_ISOM_BOX_TYPE_SGPD, sgpd, "stbl traf", GF_ISOM_SAMPLE_GROUP_DTRT, "p15"),
	SGPD_DEFINE( GF_ISOM_BOX_TYPE_SGPD, sgpd, "stbl traf", GF_ISOM_SAMPLE_GROUP_MVIF, "p15"),
	SGPD_DEFINE( GF_ISOM_BOX_TYPE_SGPD, sgpd, "stbl traf", GF_ISOM_SAMPLE_GROUP_SCIF, "p15"),
	SGPD_DEFINE( GF_ISOM_BOX_TYPE_SGPD, sgpd, "stbl traf", GF_ISOM_SAMPLE_GROUP_SCNM, "p15"),
	SGPD_DEFINE( GF_ISOM_BOX_TYPE_SGPD, sgpd, "stbl traf", GF_ISOM_SAMPLE_GROUP_STSA, "p15"),
	SGPD_DEFINE( GF_ISOM_BOX_TYPE_SGPD, sgpd, "stbl traf", GF_ISOM_SAMPLE_GROUP_TSAS, "p15"),
	SGPD_DEFINE( GF_ISOM_BOX_TYPE_SGPD, sgpd, "stbl traf", GF_ISOM_SAMPLE_GROUP_SYNC, "p15"),
	SGPD_DEFINE( GF_ISOM_BOX_TYPE_SGPD, sgpd, "stbl traf", GF_ISOM_SAMPLE_GROUP_TSCL, "p15"),
	SGPD_DEFINE( GF_ISOM_BOX_TYPE_SGPD, sgpd, "stbl traf", GF_ISOM_SAMPLE_GROUP_VIPR, "p15"),
	SGPD_DEFINE( GF_ISOM_BOX_TYPE_SGPD, sgpd, "stbl traf", GF_ISOM_SAMPLE_GROUP_LBLI, "p15"),
	SGPD_DEFINE( GF_ISOM_BOX_TYPE_SGPD, sgpd, "stbl traf", GF_ISOM_SAMPLE_GROUP_SPOR, "p15"),

	SGPD_DEFINE( GF_ISOM_BOX_TYPE_SGPD, sgpd, "stbl traf", GF_ISOM_SAMPLE_GROUP_3GAG, "3gpp"),
	SGPD_DEFINE( GF_ISOM_BOX_TYPE_SGPD, sgpd, "stbl traf", GF_ISOM_SAMPLE_GROUP_AVCB, "3gpp"),

	//internal boxes
	BOX_DEFINE_S( GF_ISOM_BOX_TYPE_GNRM, gnrm, "stsd", "unknown"),
	BOX_DEFINE_S( GF_ISOM_BOX_TYPE_GNRV, gnrv, "stsd", "unknown"),
	BOX_DEFINE_S( GF_ISOM_BOX_TYPE_GNRA, gnra, "stsd", "unknown"),

	//all track group types
	TRGT_DEFINE( GF_ISOM_BOX_TYPE_TRGT, trgt, "trgr", GF_ISOM_BOX_TYPE_MSRC, 0, "p12" ),
	TRGT_DEFINE( GF_ISOM_BOX_TYPE_TRGT, trgt, "trgr", GF_ISOM_BOX_TYPE_STER, 0, "p12" ),
	TRGT_DEFINE( GF_ISOM_BOX_TYPE_TRGT, trgt, "trgr", GF_ISOM_BOX_TYPE_CSTG, 0, "p15" ),

	//part12 boxes
	BOX_DEFINE( GF_ISOM_BOX_TYPE_FREE, free, "*"),
	BOX_DEFINE( GF_ISOM_BOX_TYPE_SKIP, free, "*"),
	BOX_DEFINE( GF_ISOM_BOX_TYPE_MDAT, mdat, "file"),
	BOX_DEFINE( GF_ISOM_BOX_TYPE_IDAT, mdat, "meta"),
	BOX_DEFINE( GF_ISOM_BOX_TYPE_IMDA, mdat, "file"),
	BOX_DEFINE_CHILD( GF_ISOM_BOX_TYPE_MOOV, moov, "file"),
	FBOX_DEFINE( GF_ISOM_BOX_TYPE_MVHD, mvhd, "moov", 1),
	FBOX_DEFINE( GF_ISOM_BOX_TYPE_MDHD, mdhd, "mdia", 1),
	FBOX_DEFINE( GF_ISOM_BOX_TYPE_VMHD, vmhd, "minf", 0),
	FBOX_DEFINE( GF_ISOM_BOX_TYPE_SMHD, smhd, "minf", 0),
	FBOX_DEFINE( GF_ISOM_BOX_TYPE_HMHD, hmhd, "minf", 0),
	FBOX_DEFINE( GF_ISOM_BOX_TYPE_NMHD, nmhd, "minf", 0),
	FBOX_DEFINE( GF_ISOM_BOX_TYPE_STHD, nmhd, "minf", 0),
	BOX_DEFINE_CHILD( GF_ISOM_BOX_TYPE_STBL, stbl, "minf"),
	BOX_DEFINE_CHILD( GF_ISOM_BOX_TYPE_DINF, dinf, "minf meta"),
	FBOX_DEFINE_FLAGS( GF_ISOM_BOX_TYPE_URL, url, "dref", 0, 1),
	FBOX_DEFINE_FLAGS( GF_ISOM_BOX_TYPE_URN, urn, "dref", 0, 1),
	FBOX_DEFINE( GF_ISOM_BOX_TYPE_CPRT, cprt, "udta", 0),
	FBOX_DEFINE( GF_ISOM_BOX_TYPE_KIND, kind, "udta", 0),
	FBOX_DEFINE( GF_ISOM_BOX_TYPE_HDLR, hdlr, "mdia meta minf", 0),	//minf container is OK in QT ...
	BOX_DEFINE_CHILD( GF_ISOM_BOX_TYPE_TRAK, trak, "moov"),
	BOX_DEFINE_CHILD( GF_ISOM_BOX_TYPE_EDTS, edts, "trak"),
	BOX_DEFINE_CHILD( GF_ISOM_BOX_TYPE_UDTA, udta, "moov trak moof traf"),
	FBOX_DEFINE( GF_ISOM_BOX_TYPE_DREF, dref, "dinf", 0),
	FBOX_DEFINE_CHILD( GF_ISOM_BOX_TYPE_STSD, stsd, "stbl", 0),
	FBOX_DEFINE( GF_ISOM_BOX_TYPE_STTS, stts, "stbl", 0),
	FBOX_DEFINE( GF_ISOM_BOX_TYPE_CTTS, ctts, "stbl", 1),
	FBOX_DEFINE( GF_ISOM_BOX_TYPE_CSLG, cslg, "stbl trep", 1),
	FBOX_DEFINE( GF_ISOM_BOX_TYPE_STSH, stsh, "stbl", 0),
	FBOX_DEFINE( GF_ISOM_BOX_TYPE_ELST, elst, "edts", 1),
	FBOX_DEFINE( GF_ISOM_BOX_TYPE_STSC, stsc, "stbl", 0),
	FBOX_DEFINE( GF_ISOM_BOX_TYPE_STSZ, stsz, "stbl", 0),
	FBOX_DEFINE( GF_ISOM_BOX_TYPE_STZ2, stsz, "stbl", 0),
	FBOX_DEFINE( GF_ISOM_BOX_TYPE_STCO, stco, "stbl", 0),
	FBOX_DEFINE( GF_ISOM_BOX_TYPE_STSS, stss, "stbl", 0),
	FBOX_DEFINE( GF_ISOM_BOX_TYPE_STDP, stdp, "stbl", 0),
	FBOX_DEFINE( GF_ISOM_BOX_TYPE_SDTP, sdtp, "stbl traf", 0),
	FBOX_DEFINE( GF_ISOM_BOX_TYPE_CO64, co64, "stbl", 0),
	BOX_DEFINE_CHILD( GF_ISOM_BOX_TYPE_MINF, minf, "mdia"),
	FBOX_DEFINE_FLAGS(GF_ISOM_BOX_TYPE_TKHD, tkhd, "trak", 1, 0x000001 | 0x000002 | 0x000004 | 0x000008),
	BOX_DEFINE( GF_ISOM_BOX_TYPE_TREF, tref, "trak"),
	BOX_DEFINE_CHILD( GF_ISOM_BOX_TYPE_MDIA, mdia, "trak"),
	BOX_DEFINE_CHILD( GF_ISOM_BOX_TYPE_MFRA, mfra, "file"),
	FBOX_DEFINE( GF_ISOM_BOX_TYPE_MFRO, mfro, "mfra", 0),
	FBOX_DEFINE( GF_ISOM_BOX_TYPE_TFRA, tfra, "mfra", 1),
	FBOX_DEFINE( GF_ISOM_BOX_TYPE_ELNG, elng, "mdia", 0),
	FBOX_DEFINE( GF_ISOM_BOX_TYPE_PDIN, pdin, "file", 0),
	FBOX_DEFINE( GF_ISOM_BOX_TYPE_SBGP, sbgp, "stbl traf", 1),
	FBOX_DEFINE( GF_ISOM_BOX_TYPE_SGPD, sgpd, "stbl traf", 2),
	BOX_DEFINE( GF_ISOM_BOX_TYPE_CSGP, csgp, "stbl traf"),
	FBOX_DEFINE_FLAGS(GF_ISOM_BOX_TYPE_SAIZ, saiz, "stbl traf", 0, 0),
	FBOX_DEFINE_FLAGS(GF_ISOM_BOX_TYPE_SAIZ, saiz, "stbl traf", 0, 1),
	FBOX_DEFINE_FLAGS(GF_ISOM_BOX_TYPE_SAIO, saio, "stbl traf", 1, 0),
	FBOX_DEFINE_FLAGS(GF_ISOM_BOX_TYPE_SAIO, saio, "stbl traf", 1, 1),
	FBOX_DEFINE_FLAGS( GF_ISOM_BOX_TYPE_SUBS, subs, "stbl traf", 0, 7), //warning flags are not used as a bit mask but as an enum!!
	BOX_DEFINE_CHILD( GF_ISOM_BOX_TYPE_TRGR, trgr, "trak"),
	BOX_DEFINE( GF_ISOM_BOX_TYPE_FTYP, ftyp, "file otyp"),
	BOX_DEFINE( GF_ISOM_BOX_TYPE_OTYP, def_parent, "file"),
	FBOX_DEFINE( GF_ISOM_BOX_TYPE_PADB, padb, "stbl", 0),
	BOX_DEFINE( GF_ISOM_BOX_TYPE_BTRT, btrt, "sample_entry"),
	BOX_DEFINE( GF_ISOM_BOX_TYPE_PASP, pasp, "video_sample_entry ipco"),
	BOX_DEFINE( GF_ISOM_BOX_TYPE_CLAP, clap, "video_sample_entry ipco"),
	BOX_DEFINE_CHILD( GF_ISOM_BOX_TYPE_META, meta, "file moov trak moof traf udta"),	//apple uses meta in moov->udta
	FBOX_DEFINE( GF_ISOM_BOX_TYPE_XML, xml, "meta", 0),
	FBOX_DEFINE( GF_ISOM_BOX_TYPE_BXML, bxml, "meta", 0),
	FBOX_DEFINE( GF_ISOM_BOX_TYPE_ILOC, iloc, "meta", 2),
	FBOX_DEFINE( GF_ISOM_BOX_TYPE_PITM, pitm, "meta", 1),
	FBOX_DEFINE_CHILD( GF_ISOM_BOX_TYPE_IPRO, ipro, "meta", 0),
	FBOX_DEFINE( GF_ISOM_BOX_TYPE_INFE, infe, "iinf", 3),
	FBOX_DEFINE_CHILD( GF_ISOM_BOX_TYPE_IINF, iinf, "meta", 1),
	FBOX_DEFINE_CHILD( GF_ISOM_BOX_TYPE_IREF, iref, "meta", 1),
	BOX_DEFINE_CHILD( GF_ISOM_BOX_TYPE_SINF, sinf, "ipro sample_entry"),
	BOX_DEFINE( GF_ISOM_BOX_TYPE_RINF, sinf, "sample_entry"),
	BOX_DEFINE( GF_ISOM_BOX_TYPE_FRMA, frma, "sinf rinf"),
	FBOX_DEFINE_FLAGS(GF_ISOM_BOX_TYPE_SCHM, schm, "sinf rinf", 0, 1),
	BOX_DEFINE_CHILD( GF_ISOM_BOX_TYPE_SCHI, schi, "sinf rinf"),
	BOX_DEFINE_CHILD( GF_ISOM_BOX_TYPE_ENCA, audio_sample_entry, "stsd"),
	BOX_DEFINE_CHILD( GF_ISOM_BOX_TYPE_ENCV, video_sample_entry, "stsd"),
	BOX_DEFINE_CHILD( GF_ISOM_BOX_TYPE_RESV, video_sample_entry, "stsd"),
	FBOX_DEFINE( GF_ISOM_BOX_TYPE_TSEL, tsel, "udta", 0),
	BOX_DEFINE_CHILD( GF_ISOM_BOX_TYPE_STRK, strk, "udta"),
	FBOX_DEFINE( GF_ISOM_BOX_TYPE_STRI, stri, "strk", 0),
	BOX_DEFINE( GF_ISOM_BOX_TYPE_STRD, def_parent, "strk"),
	FBOX_DEFINE( GF_ISOM_BOX_TYPE_STSG, stsg, "strd", 0),

	BOX_DEFINE_CHILD( GF_ISOM_BOX_TYPE_ENCS, mp4s, "stsd"),
	//THIS HAS TO BE FIXED, not extensible
	BOX_DEFINE_CHILD( GF_ISOM_BOX_TYPE_ENCT, mp4s, "stsd"),
	BOX_DEFINE_CHILD( GF_ISOM_BOX_TYPE_ENCM, mp4s, "stsd"),
	BOX_DEFINE_CHILD( GF_ISOM_BOX_TYPE_ENCF, mp4s, "stsd"),

	BOX_DEFINE_CHILD( GF_ISOM_BOX_TYPE_METX, metx, "stsd"),
	BOX_DEFINE_CHILD( GF_ISOM_BOX_TYPE_METT, metx, "stsd"),
	FBOX_DEFINE( GF_ISOM_BOX_TYPE_STVI, stvi, "schi", 0),


	FBOX_DEFINE( GF_ISOM_BOX_TYPE_CHNL, chnl, "audio_sample_entry", 0),

	//FEC
	FBOX_DEFINE_CHILD( GF_ISOM_BOX_TYPE_FIIN, fiin, "meta", 0),
	BOX_DEFINE_CHILD( GF_ISOM_BOX_TYPE_PAEN, paen, "fiin"),
	FBOX_DEFINE( GF_ISOM_BOX_TYPE_FPAR, fpar, "paen", 1),
	FBOX_DEFINE( GF_ISOM_BOX_TYPE_FECR, fecr, "paen", 1),
	//fire uses the same box syntax as fecr
	FBOX_DEFINE( GF_ISOM_BOX_TYPE_FIRE, fecr, "paen", 1),
	BOX_DEFINE( GF_ISOM_BOX_TYPE_SEGR, segr, "fiin"),
	FBOX_DEFINE( GF_ISOM_BOX_TYPE_GITN, gitn, "fiin", 0),

#ifndef GPAC_DISABLE_ISOM_HINTING
	BOX_DEFINE_CHILD( GF_ISOM_BOX_TYPE_FDSA, fdsa, "fdp_sample"),
	BOX_DEFINE( GF_ISOM_BOX_TYPE_FDPA, fdpa, "fdsa"),
	BOX_DEFINE( GF_ISOM_BOX_TYPE_EXTR, extr, "fdsa"),
#endif

	//full boxes todo
	//FBOX_DEFINE( GF_ISOM_BOX_TYPE_ASSP, assp, 1),
	//FBOX_DEFINE( GF_ISOM_BOX_TYPE_MERE, assp, 0),
	//FBOX_DEFINE( GF_ISOM_BOX_TYPE_SRAT, srat, 0),
	//FBOX_DEFINE( GF_ISOM_BOX_TYPE_CHNL, chnl, 0),
	//FBOX_DEFINE( GF_ISOM_BOX_TYPE_DMIX, dmix, 0),
	//FBOX_DEFINE( GF_ISOM_BOX_TYPE_TLOU, alou, 0),
	//FBOX_DEFINE( GF_ISOM_BOX_TYPE_ALOU, alou, 0),
	//FBOX_DEFINE( GF_ISOM_BOX_TYPE_URI, uri, 0),
	//FBOX_DEFINE( GF_ISOM_BOX_TYPE_URII, urii, 0),

#ifndef GPAC_DISABLE_ISOM_HINTING
	BOX_DEFINE( GF_ISOM_BOX_TYPE_RTP_STSD, ghnt, "stsd"),
	BOX_DEFINE( GF_ISOM_BOX_TYPE_SRTP_STSD, ghnt, "stsd"),
	BOX_DEFINE( GF_ISOM_BOX_TYPE_FDP_STSD, ghnt, "stsd"),
	BOX_DEFINE( GF_ISOM_BOX_TYPE_RRTP_STSD, ghnt, "stsd"),
	BOX_DEFINE( GF_ISOM_BOX_TYPE_RTCP_STSD, ghnt, "stsd"),
	BOX_DEFINE_CHILD( GF_ISOM_BOX_TYPE_HNTI, hnti, "udta"),
	BOX_DEFINE( GF_ISOM_BOX_TYPE_SDP, sdp, "hnti"),
	BOX_DEFINE_CHILD( GF_ISOM_BOX_TYPE_HINF, hinf, "udta"),
	BOX_DEFINE( GF_ISOM_BOX_TYPE_TRPY, trpy, "hinf"),
	BOX_DEFINE( GF_ISOM_BOX_TYPE_NUMP, nump, "hinf"),
	BOX_DEFINE( GF_ISOM_BOX_TYPE_TPYL, tpyl, "hinf"),
	BOX_DEFINE( GF_ISOM_BOX_TYPE_TOTL, totl, "hinf"),
	BOX_DEFINE( GF_ISOM_BOX_TYPE_NPCK, npck, "hinf"),
	BOX_DEFINE( GF_ISOM_BOX_TYPE_TPAY, tpay, "hinf"),
	BOX_DEFINE( GF_ISOM_BOX_TYPE_MAXR, maxr, "hinf"),
	BOX_DEFINE( GF_ISOM_BOX_TYPE_DMED, dmed, "hinf"),
	BOX_DEFINE( GF_ISOM_BOX_TYPE_DIMM, dimm, "hinf"),
	BOX_DEFINE( GF_ISOM_BOX_TYPE_DREP, drep, "hinf"),
	BOX_DEFINE( GF_ISOM_BOX_TYPE_TMIN, tmin, "hinf"),
	BOX_DEFINE( GF_ISOM_BOX_TYPE_TMAX, tmax, "hinf"),
	BOX_DEFINE( GF_ISOM_BOX_TYPE_PMAX, pmax, "hinf"),
	BOX_DEFINE( GF_ISOM_BOX_TYPE_DMAX, dmax, "hinf"),
	BOX_DEFINE( GF_ISOM_BOX_TYPE_PAYT, payt, "hinf"),
	BOX_DEFINE( GF_ISOM_BOX_TYPE_RTP, rtp_hnti, "hnti"),

	BOX_DEFINE( GF_ISOM_BOX_TYPE_RTPO, rtpo, "rtp_packet"),

	BOX_DEFINE( GF_ISOM_BOX_TYPE_RELY, rely, "rtp srtp"),
	BOX_DEFINE( GF_ISOM_BOX_TYPE_TIMS, tims, "rtp srtp rrtp"),
	BOX_DEFINE( GF_ISOM_BOX_TYPE_TSRO, tsro, "rtp srtp rrtp"),
	BOX_DEFINE( GF_ISOM_BOX_TYPE_SNRO, snro, "rtp srtp"),
	BOX_DEFINE( GF_ISOM_BOX_TYPE_NAME, name, "udta"),
	BOX_DEFINE( GF_ISOM_BOX_TYPE_TSSY, tssy, "rrtp"),
	BOX_DEFINE( GF_ISOM_BOX_TYPE_RSSR, rssr, "rrtp"),
	FBOX_DEFINE_CHILD( GF_ISOM_BOX_TYPE_SRPP, srpp, "srtp", 0),

#endif

#ifndef	GPAC_DISABLE_ISOM_FRAGMENTS
	BOX_DEFINE_CHILD( GF_ISOM_BOX_TYPE_MVEX, mvex, "moov"),
	FBOX_DEFINE( GF_ISOM_BOX_TYPE_MEHD, mehd, "mvex", 1),
	FBOX_DEFINE( GF_ISOM_BOX_TYPE_TREX, trex, "mvex", 0),
	FBOX_DEFINE( GF_ISOM_BOX_TYPE_LEVA, leva, "mvex", 0),
	FBOX_DEFINE( GF_ISOM_BOX_TYPE_TREP, trep, "mvex", 0),
	BOX_DEFINE_CHILD( GF_ISOM_BOX_TYPE_MOOF, moof, "file"),
	FBOX_DEFINE( GF_ISOM_BOX_TYPE_MFHD, mfhd, "moof", 0),
	BOX_DEFINE_CHILD( GF_ISOM_BOX_TYPE_TRAF, traf, "moof"),
	FBOX_DEFINE_FLAGS(GF_ISOM_BOX_TYPE_TFHD, tfhd, "traf", 0, 0x000001|0x000002|0x000008|0x000010|0x000020|0x010000|0x020000),
	FBOX_DEFINE_FLAGS(GF_ISOM_BOX_TYPE_TRUN, trun, "traf", 0, 0x000001|0x000004|0x000100|0x000200|0x000400|0x000800),
#ifdef GF_ENABLE_CTRN
	FBOX_DEFINE_FLAGS(GF_ISOM_BOX_TYPE_CTRN, trun, "traf", 0, 0),
#endif
	FBOX_DEFINE( GF_ISOM_BOX_TYPE_TFDT, tfdt, "traf", 1),
	BOX_DEFINE( GF_ISOM_BOX_TYPE_STYP, ftyp, "file"),
	FBOX_DEFINE( GF_ISOM_BOX_TYPE_PRFT, prft, "file", 1),
	FBOX_DEFINE( GF_ISOM_BOX_TYPE_SIDX, sidx, "file", 1),
	FBOX_DEFINE( GF_ISOM_BOX_TYPE_SSIX, ssix, "file", 0),
	BOX_DEFINE_S( GF_ISOM_BOX_TYPE_PCRB, pcrb, "file", "dash"),
	FBOX_DEFINE_S( GF_ISOM_BOX_TYPE_EMSG, emsg, "file", 1, "dash"),
#endif


	//part14 boxes
	FBOX_DEFINE_S( GF_ISOM_BOX_TYPE_IODS, iods, "moov", 0, "p14"),
	BOX_DEFINE_S_CHILD( GF_ISOM_BOX_TYPE_MP4S, mp4s, "stsd", "p14"),
	BOX_DEFINE_S_CHILD( GF_ISOM_BOX_TYPE_MP4V, video_sample_entry, "stsd", "p14"),
	BOX_DEFINE_S_CHILD( GF_ISOM_BOX_TYPE_MP4A, audio_sample_entry, "stsd", "p14"),
	BOX_DEFINE_S( GF_ISOM_BOX_TYPE_M4DS, m4ds, "sample_entry", "p14"),
	FBOX_DEFINE_S( GF_ISOM_BOX_TYPE_ESDS, esds, "mp4a mp4s mp4v encv enca encs resv wave", 0, "p14"),

	//part 15 boxes
	BOX_DEFINE_S( GF_ISOM_BOX_TYPE_AVCC, avcc, "avc1 avc2 avc3 avc4 encv resv ipco dva1 dvav", "p15"),
	BOX_DEFINE_S( GF_ISOM_BOX_TYPE_SVCC, avcc, "avc1 avc2 avc3 avc4 svc1 svc2 encv resv", "p15"),
	BOX_DEFINE_S( GF_ISOM_BOX_TYPE_MVCC, avcc, "avc1 avc2 avc3 avc4 mvc1 mvc2 encv resv", "p15"),
	BOX_DEFINE_S( GF_ISOM_BOX_TYPE_HVCC, hvcc, "hvc1 hev1 hvc2 hev2 encv resv ipco dvh1 dvhe", "p15"),
	BOX_DEFINE_S( GF_ISOM_BOX_TYPE_LHVC, hvcc, "hvc1 hev1 hvc2 hev2 lhv1 lhe1 encv resv ipco", "p15"),
	FBOX_DEFINE_S( GF_ISOM_BOX_TYPE_VVCC, vvcc, "vvc1 vvi1 encv resv ipco dvhe", 0, "p15"),
	FBOX_DEFINE_S( GF_ISOM_BOX_TYPE_VVNC, vvnc, "vvs1 encv resv ipco dvhe", 0, "p15"),
	BOX_DEFINE_S_CHILD( GF_ISOM_BOX_TYPE_AVC1, video_sample_entry, "stsd", "p15"),
	BOX_DEFINE_S_CHILD( GF_ISOM_BOX_TYPE_AVC2, video_sample_entry, "stsd", "p15"),
	BOX_DEFINE_S_CHILD( GF_ISOM_BOX_TYPE_AVC3, video_sample_entry, "stsd", "p15"),
	BOX_DEFINE_S_CHILD( GF_ISOM_BOX_TYPE_AVC4, video_sample_entry, "stsd", "p15"),
	BOX_DEFINE_S_CHILD( GF_ISOM_BOX_TYPE_SVC1, video_sample_entry, "stsd", "p15"),
	BOX_DEFINE_S_CHILD( GF_ISOM_BOX_TYPE_MVC1, video_sample_entry, "stsd", "p15"),
	BOX_DEFINE_S_CHILD( GF_ISOM_BOX_TYPE_HVC1, video_sample_entry, "stsd", "p15"),
	BOX_DEFINE_S_CHILD( GF_ISOM_BOX_TYPE_HEV1, video_sample_entry, "stsd", "p15"),
	BOX_DEFINE_S_CHILD( GF_ISOM_BOX_TYPE_HVC2, video_sample_entry, "stsd", "p15"),
	BOX_DEFINE_S_CHILD( GF_ISOM_BOX_TYPE_HEV2, video_sample_entry, "stsd", "p15"),
	BOX_DEFINE_S_CHILD( GF_ISOM_BOX_TYPE_LHV1, video_sample_entry, "stsd", "p15"),
	BOX_DEFINE_S_CHILD( GF_ISOM_BOX_TYPE_LHE1, video_sample_entry, "stsd", "p15"),
	BOX_DEFINE_S_CHILD( GF_ISOM_BOX_TYPE_HVT1, video_sample_entry, "stsd", "p15"),
	BOX_DEFINE_S_CHILD( GF_ISOM_BOX_TYPE_VVC1, video_sample_entry, "stsd", "p15"),
	BOX_DEFINE_S_CHILD( GF_ISOM_BOX_TYPE_VVI1, video_sample_entry, "stsd", "p15"),
	BOX_DEFINE_S_CHILD( GF_ISOM_BOX_TYPE_VVS1, video_sample_entry, "stsd", "p15"),
	FBOX_DEFINE_S(GF_ISOM_BOX_TYPE_MVCI, def_parent_full, "minf", 0, "p15"),
	FBOX_DEFINE_S(GF_ISOM_BOX_TYPE_MVCG, mvcg, "mvci", 0, "p15"),
	FBOX_DEFINE_S( GF_ISOM_BOX_TYPE_VWID, vwid, "video_sample_entry", 0, "p15"),

	//mpegh 3D audio boxes
	BOX_DEFINE_S_CHILD( GF_ISOM_BOX_TYPE_MHA1, audio_sample_entry, "stsd", "mpegh3Daudio"),
	BOX_DEFINE_S_CHILD( GF_ISOM_BOX_TYPE_MHA2, audio_sample_entry, "stsd", "mpegh3Daudio"),
	BOX_DEFINE_S_CHILD( GF_ISOM_BOX_TYPE_MHM1, audio_sample_entry, "stsd", "mpegh3Daudio"),
	BOX_DEFINE_S_CHILD( GF_ISOM_BOX_TYPE_MHM2, audio_sample_entry, "stsd", "mpegh3Daudio"),
	BOX_DEFINE_S( GF_ISOM_BOX_TYPE_MHAC, mhac, "mha1 mha2 mhm1 mhm2 wave", "mpegh3Daudio"),
	BOX_DEFINE_S( GF_ISOM_BOX_TYPE_MHAP, mhap, "mha1 mha2 mhm1 mhm2 wave", "mpegh3Daudio"),

	BOX_DEFINE_S_CHILD( GF_ISOM_BOX_TYPE_IPCM, audio_sample_entry, "stsd", "23003_5"),
	BOX_DEFINE_S_CHILD( GF_ISOM_BOX_TYPE_FPCM, audio_sample_entry, "stsd", "23003_5"),
	FBOX_DEFINE_S( GF_ISOM_BOX_TYPE_PCMC, pcmC, "ipcm fpcm", 0, "23003_5"),

	//AV1 in ISOBMFF boxes
	BOX_DEFINE_S_CHILD(GF_ISOM_BOX_TYPE_AV01, video_sample_entry, "stsd", "av1"),
	BOX_DEFINE_S(GF_ISOM_BOX_TYPE_AV1C, av1c, "av01 encv resv ipco dav1", "av1"),

	// VP8-9 boxes
	FBOX_DEFINE_FLAGS_S( GF_ISOM_BOX_TYPE_VPCC, vpcc, "vp08 vp09 encv resv", 1, 0, "vp"),
	BOX_DEFINE_S_CHILD( GF_ISOM_BOX_TYPE_VP08, video_sample_entry, "stsd", "vp"),
	BOX_DEFINE_S_CHILD( GF_ISOM_BOX_TYPE_VP09, video_sample_entry, "stsd", "vp"),
	FBOX_DEFINE_FLAGS_S(GF_ISOM_BOX_TYPE_SMDM, SmDm, "vp08 vp09 encv resv", 1, 0, "vp"),
	FBOX_DEFINE_FLAGS_S(GF_ISOM_BOX_TYPE_COLL, CoLL, "vp08 vp09 encv resv", 1, 0, "vp"),

	//Opus in ISOBMFF boxes
#ifndef GPAC_DISABLE_OGG
	BOX_DEFINE_S_CHILD(GF_ISOM_BOX_TYPE_OPUS, audio_sample_entry, "stsd", "Opus"),
	BOX_DEFINE_S(GF_ISOM_BOX_TYPE_DOPS, dOps, "Opus wave enca", "Opus"),
#endif

	//part20 boxes
	BOX_DEFINE_S_CHILD( GF_ISOM_BOX_TYPE_LSR1, lsr1, "stsd", "p20"),
	BOX_DEFINE_S( GF_ISOM_BOX_TYPE_LSRC, lsrc, "lsr1", "p20"),

	//part30 boxes
#ifndef GPAC_DISABLE_TTXT
	BOX_DEFINE_CHILD( GF_ISOM_BOX_TYPE_STXT, metx, "stsd"),
	FBOX_DEFINE( GF_ISOM_BOX_TYPE_TXTC, txtc, "stxt mett sbtt", 0),
	//we allow mime in any sample entry, not restricted in the spec ...
	FBOX_DEFINE( GF_ISOM_BOX_TYPE_MIME, txtc, "sample_entry", 0),
#ifndef GPAC_DISABLE_VTT
	BOX_DEFINE_S_CHILD( GF_ISOM_BOX_TYPE_WVTT, wvtt, "stsd", "p30"),
	BOX_DEFINE_S_CHILD( GF_ISOM_BOX_TYPE_VTCC_CUE, vtcu, "vtt_sample", "p30"),
	BOX_DEFINE_S( GF_ISOM_BOX_TYPE_VTTE, vtte, "vtt_sample", "p30"),
	BOX_DEFINE_S( GF_ISOM_BOX_TYPE_VTTC_CONFIG, boxstring, "wvtt", "p30"),
	BOX_DEFINE_S( GF_ISOM_BOX_TYPE_CTIM, boxstring, "vttc", "p30"),
	BOX_DEFINE_S( GF_ISOM_BOX_TYPE_IDEN, boxstring, "vttc", "p30"),
	BOX_DEFINE_S( GF_ISOM_BOX_TYPE_STTG, boxstring, "vttc", "p30"),
	BOX_DEFINE_S( GF_ISOM_BOX_TYPE_PAYL, boxstring, "vttc", "p30"),
	BOX_DEFINE_S( GF_ISOM_BOX_TYPE_VTTA, boxstring, "vttc", "p30"),
#endif
	BOX_DEFINE_CHILD( GF_ISOM_BOX_TYPE_STPP, metx, "stsd"),
	BOX_DEFINE_CHILD( GF_ISOM_BOX_TYPE_SBTT, metx, "stsd"),
#endif

	//Image File Format
	BOX_DEFINE_S_CHILD( GF_ISOM_BOX_TYPE_IPRP, iprp, "meta", "iff"),
	BOX_DEFINE_S( GF_ISOM_BOX_TYPE_IPCO, ipco, "iprp", "iff"),
	FBOX_DEFINE_S( GF_ISOM_BOX_TYPE_ISPE, ispe, "ipco", 0, "iff"),
	BOX_DEFINE_S( GF_ISOM_BOX_TYPE_COLR, colr, "video_sample_entry ipco encv resv", "iff"),
	//defined as a secondary box for now to avoid conflicts with master hashes
	BOX_DEFINE_S( GF_ISOM_BOX_TYPE_COLR, colr, "jp2h", "iff"),
	FBOX_DEFINE_S( GF_ISOM_BOX_TYPE_PIXI, pixi, "ipco", 0, "iff"),
	FBOX_DEFINE_S( GF_ISOM_BOX_TYPE_RLOC, rloc, "ipco", 0, "iff"),
	BOX_DEFINE_S( GF_ISOM_BOX_TYPE_IROT, irot, "ipco", "iff"),
	BOX_DEFINE_S( GF_ISOM_BOX_TYPE_IMIR, imir, "ipco", "iff"),
	FBOX_DEFINE_FLAGS_S( GF_ISOM_BOX_TYPE_IPMA, ipma, "iprp", 1, 1, "iff"),
	BOX_DEFINE_S( GF_ISOM_BOX_TYPE_GRPL, grpl, "meta", "iff"),
	FBOX_DEFINE_S( GF_ISOM_BOX_TYPE_CCST, ccst, "sample_entry", 0, "iff"),
	FBOX_DEFINE_S( GF_ISOM_BOX_TYPE_AUXI, auxi, "sample_entry", 0, "iff"),
	TRGT_DEFINE(GF_ISOM_BOX_TYPE_GRPT, grptype, "grpl", GF_ISOM_BOX_TYPE_ALTR, 0, "iff"),
	FBOX_DEFINE_S( GF_ISOM_BOX_TYPE_AUXC, auxc, "ipco", 0, "iff"),
	FBOX_DEFINE_S( GF_ISOM_BOX_TYPE_OINF, oinf, "ipco", 0, "iff"),
	FBOX_DEFINE_S( GF_ISOM_BOX_TYPE_TOLS, tols, "ipco", 0, "iff"),
	FBOX_DEFINE_S( GF_ISOM_BOX_TYPE_IENC, ienc, "ipco", 0, "cenc"),
	FBOX_DEFINE_S( GF_ISOM_BOX_TYPE_IAUX, iaux, "ipco", 0, "cenc"),

	//MIAF
	BOX_DEFINE_S(GF_ISOM_BOX_TYPE_CLLI, clli, "video_sample_entry ipco encv resv", "p12"),
	BOX_DEFINE_S(GF_ISOM_BOX_TYPE_MDCV, mdcv, "video_sample_entry ipco encv resv", "p12"),

	//AVIF
	BOX_DEFINE_S(GF_ISOM_BOX_TYPE_A1LX, a1lx, "ipco", "avif"),
	BOX_DEFINE_S(GF_ISOM_BOX_TYPE_A1OP, a1op, "ipco", "avif"),

	//other MPEG boxes
	BOX_DEFINE_S( GF_ISOM_BOX_TYPE_RVCC, rvcc, "avc1 avc2 avc3 avc4 svc1 svc2 hvc1 hev1 hvc2 hev2 lhv1 lhe1 encv resv", "rvc"),

	//3GPP boxes
	BOX_DEFINE_S_CHILD( GF_ISOM_SUBTYPE_3GP_AMR, audio_sample_entry, "stsd", "3gpp"),
	BOX_DEFINE_S_CHILD( GF_ISOM_SUBTYPE_3GP_AMR_WB, audio_sample_entry, "stsd", "3gpp"),
	BOX_DEFINE_S_CHILD( GF_ISOM_SUBTYPE_3GP_QCELP, audio_sample_entry, "stsd", "3gpp"),
	BOX_DEFINE_S_CHILD( GF_ISOM_SUBTYPE_3GP_EVRC, audio_sample_entry, "stsd", "3gpp"),
	BOX_DEFINE_S_CHILD( GF_ISOM_SUBTYPE_3GP_SMV, audio_sample_entry, "stsd", "3gpp"),
	BOX_DEFINE_S_CHILD( GF_ISOM_SUBTYPE_3GP_H263, video_sample_entry, "stsd", "3gpp"),
	BOX_DEFINE_S( GF_ISOM_BOX_TYPE_DAMR, gppc, "samr sawb enca", "3gpp"),
	BOX_DEFINE_S( GF_ISOM_BOX_TYPE_DEVC, gppc, "sevc enca", "3gpp"),
	BOX_DEFINE_S( GF_ISOM_BOX_TYPE_DQCP, gppc, "sqcp enca", "3gpp"),
	BOX_DEFINE_S( GF_ISOM_BOX_TYPE_DSMV, gppc, "ssmv enca", "3gpp"),
	BOX_DEFINE_S( GF_ISOM_BOX_TYPE_D263, gppc, "s263 encv", "3gpp"),
	//3gpp timed text
	BOX_DEFINE_S_CHILD( GF_ISOM_BOX_TYPE_TX3G, tx3g, "stsd", "3gpp"),
	BOX_DEFINE_S( GF_ISOM_BOX_TYPE_TEXT, text, "stsd", "apple"),
	BOX_DEFINE_S( GF_ISOM_BOX_TYPE_FTAB, ftab, "tx3g text enct", "3gpp"),
	BOX_DEFINE_S( GF_ISOM_BOX_TYPE_STYL, styl, "text_sample", "3gpp"),
	BOX_DEFINE_S( GF_ISOM_BOX_TYPE_HLIT, hlit, "text_sample", "3gpp"),
	BOX_DEFINE_S( GF_ISOM_BOX_TYPE_HCLR, hclr, "text_sample", "3gpp"),
	BOX_DEFINE_S( GF_ISOM_BOX_TYPE_KROK, krok, "text_sample", "3gpp"),
	BOX_DEFINE_S( GF_ISOM_BOX_TYPE_DLAY, dlay, "text_sample", "3gpp"),
	BOX_DEFINE_S( GF_ISOM_BOX_TYPE_HREF, href, "text_sample", "3gpp"),
	BOX_DEFINE_S( GF_ISOM_BOX_TYPE_TBOX, tbox, "text_sample", "3gpp"),
	BOX_DEFINE_S( GF_ISOM_BOX_TYPE_BLNK, blnk, "text_sample", "3gpp"),
	BOX_DEFINE_S( GF_ISOM_BOX_TYPE_TWRP, twrp, "text_sample", "3gpp"),
	//3GPP dims
	BOX_DEFINE_S_CHILD( GF_ISOM_BOX_TYPE_DIMS, dims, "stsd", "3gpp"),
	BOX_DEFINE_S( GF_ISOM_BOX_TYPE_DIMC, dimC, "dims encs", "3gpp"),
	BOX_DEFINE_S( GF_ISOM_BOX_TYPE_DIST, diST, "dims", "3gpp"),


	//CENC boxes
	FBOX_DEFINE_S( GF_ISOM_BOX_TYPE_PSSH, pssh, "moov moof meta", 0, "cenc"),
	FBOX_DEFINE_S( GF_ISOM_BOX_TYPE_TENC, tenc, "schi", 1, "cenc"),
	BOX_DEFINE_S( GF_ISOM_BOX_TYPE_SENC, senc, "trak traf", "cenc"),

	// ISMA 1.1 boxes
	FBOX_DEFINE_S( GF_ISOM_BOX_TYPE_IKMS, iKMS, "schi", 0, "isma"),
	FBOX_DEFINE_S( GF_ISOM_BOX_TYPE_ISFM, iSFM, "schi", 0, "isma"),
	FBOX_DEFINE_S( GF_ISOM_BOX_TYPE_ISLT, iSLT, "schi", 0, "isma"),

	//OMA boxes
	FBOX_DEFINE_S( GF_ISOM_BOX_TYPE_ODKM, odkm, "schi", 0, "oma"),
	FBOX_DEFINE_S( GF_ISOM_BOX_TYPE_OHDR, ohdr, "odkm", 0, "oma"),
	FBOX_DEFINE_S( GF_ISOM_BOX_TYPE_GRPI, grpi, "ohdr", 0, "oma"),
	BOX_DEFINE_S( GF_ISOM_BOX_TYPE_MDRI, mdri, "file", "oma"),
	FBOX_DEFINE_S( GF_ISOM_BOX_TYPE_ODTT, odtt, "mdri", 0, "oma"),
	FBOX_DEFINE_S( GF_ISOM_BOX_TYPE_ODRB, odrb, "mdri", 0, "oma"),
	FBOX_DEFINE_S( GF_ISOM_BOX_TYPE_ODAF, iSFM, "schi", 0, "oma"),

	//apple boxes
	BOX_DEFINE_S_CHILD( GF_ISOM_BOX_TYPE_MP3, audio_sample_entry, "stsd", "apple"),
	FBOX_DEFINE_S( GF_ISOM_BOX_TYPE_CHPL, chpl, "udta", 0, "apple"),
	BOX_DEFINE_S( GF_ISOM_BOX_TYPE_VOID, void, "", "apple"),
	BOX_DEFINE_S(GF_QT_BOX_TYPE_WIDE, wide, "*", "apple"),
	BOX_DEFINE_S( GF_ISOM_BOX_TYPE_ILST, ilst, "meta", "apple"),
	FBOX_DEFINE_S( GF_ISOM_BOX_TYPE_DATA, databox, "ilst *", 0, "apple"),

	ITUNES_TAG(GF_ISOM_ITUNE_NAME),
	ITUNES_TAG(GF_ISOM_ITUNE_ARTIST),
	ITUNES_TAG(GF_ISOM_ITUNE_ALBUM_ARTIST),
	ITUNES_TAG(GF_ISOM_ITUNE_ALBUM),
	ITUNES_TAG(GF_ISOM_ITUNE_GROUP),
	ITUNES_TAG(GF_ISOM_ITUNE_WRITER),
	ITUNES_TAG(GF_ISOM_ITUNE_COMMENT),
	ITUNES_TAG(GF_ISOM_ITUNE_GENRE_USER),
	ITUNES_TAG(GF_ISOM_ITUNE_GENRE),
	ITUNES_TAG(GF_ISOM_ITUNE_CREATED),
	ITUNES_TAG(GF_ISOM_ITUNE_TRACKNUMBER),
	ITUNES_TAG(GF_ISOM_ITUNE_DISK),
	ITUNES_TAG(GF_ISOM_ITUNE_TEMPO),
	ITUNES_TAG(GF_ISOM_ITUNE_COMPILATION),
	ITUNES_TAG(GF_ISOM_ITUNE_TV_SHOW),
	ITUNES_TAG(GF_ISOM_ITUNE_TV_EPISODE),
	ITUNES_TAG(GF_ISOM_ITUNE_TV_SEASON),
	ITUNES_TAG(GF_ISOM_ITUNE_TV_EPISODE_NUM),
	ITUNES_TAG(GF_ISOM_ITUNE_TV_NETWORK),
	ITUNES_TAG(GF_ISOM_ITUNE_DESCRIPTION),
	ITUNES_TAG(GF_ISOM_ITUNE_LONG_DESCRIPTION),
	ITUNES_TAG(GF_ISOM_ITUNE_LYRICS),
	ITUNES_TAG(GF_ISOM_ITUNE_SORT_NAME),
	ITUNES_TAG(GF_ISOM_ITUNE_SORT_ARTIST),
	ITUNES_TAG(GF_ISOM_ITUNE_SORT_ALB_ARTIST),
	ITUNES_TAG(GF_ISOM_ITUNE_SORT_ALBUM),
	ITUNES_TAG(GF_ISOM_ITUNE_SORT_COMPOSER),
	ITUNES_TAG(GF_ISOM_ITUNE_SORT_SHOW),
	ITUNES_TAG(GF_ISOM_ITUNE_COVER_ART),
	ITUNES_TAG(GF_ISOM_ITUNE_COPYRIGHT),
	ITUNES_TAG(GF_ISOM_ITUNE_TOOL),
	ITUNES_TAG(GF_ISOM_ITUNE_ENCODER),
	ITUNES_TAG(GF_ISOM_ITUNE_PURCHASE_DATE),
	ITUNES_TAG(GF_ISOM_ITUNE_PODCAST),
	ITUNES_TAG(GF_ISOM_ITUNE_PODCAST_URL),
	ITUNES_TAG(GF_ISOM_ITUNE_KEYWORDS),
	ITUNES_TAG(GF_ISOM_ITUNE_CATEGORY),
	ITUNES_TAG(GF_ISOM_ITUNE_HD_VIDEO),
	ITUNES_TAG(GF_ISOM_ITUNE_MEDIA_TYPE),
	ITUNES_TAG(GF_ISOM_ITUNE_RATING),
	ITUNES_TAG(GF_ISOM_ITUNE_GAPLESS),
	ITUNES_TAG(GF_ISOM_ITUNE_COMPOSER),
	ITUNES_TAG(GF_ISOM_ITUNE_TRACK),
	ITUNES_TAG(GF_ISOM_ITUNE_CONDUCTOR),
	ITUNES_TAG(GF_ISOM_ITUNE_ART_DIRECTOR),
	ITUNES_TAG(GF_ISOM_ITUNE_ARRANGER),
	ITUNES_TAG(GF_ISOM_ITUNE_LYRICIST),
	ITUNES_TAG(GF_ISOM_ITUNE_COPY_ACK),
	ITUNES_TAG(GF_ISOM_ITUNE_SONG_DESC),
	ITUNES_TAG(GF_ISOM_ITUNE_DIRECTOR),
	ITUNES_TAG(GF_ISOM_ITUNE_EQ_PRESET),
	ITUNES_TAG(GF_ISOM_ITUNE_LINER_NOTES),
	ITUNES_TAG(GF_ISOM_ITUNE_REC_COMPANY),
	ITUNES_TAG(GF_ISOM_ITUNE_ORIG_ARTIST),
	ITUNES_TAG(GF_ISOM_ITUNE_PHONO_RIGHTS),
	ITUNES_TAG(GF_ISOM_ITUNE_PRODUCER),
	ITUNES_TAG(GF_ISOM_ITUNE_PERFORMER),
	ITUNES_TAG(GF_ISOM_ITUNE_PUBLISHER),
	ITUNES_TAG(GF_ISOM_ITUNE_SOUND_ENG),
	ITUNES_TAG(GF_ISOM_ITUNE_SOLOIST),
	ITUNES_TAG(GF_ISOM_ITUNE_CREDITS),
	ITUNES_TAG(GF_ISOM_ITUNE_THANKS),
	ITUNES_TAG(GF_ISOM_ITUNE_ONLINE),
	ITUNES_TAG(GF_ISOM_ITUNE_EXEC_PRODUCER),
	ITUNES_TAG(GF_ISOM_ITUNE_LOCATION),

	BOX_DEFINE_S( GF_ISOM_BOX_TYPE_iTunesSpecificInfo, ilst_item, "ilst data", "apple"),
	BOX_DEFINE_S(GF_ISOM_BOX_TYPE_GMHD, def_parent, "minf", "apple"),
	BOX_DEFINE_S(GF_QT_BOX_TYPE_LOAD, load, "trak", "apple"),
	BOX_DEFINE_S(GF_QT_BOX_TYPE_TAPT, def_parent, "trak", "apple"),
	FBOX_DEFINE_S( GF_QT_BOX_TYPE_GMIN, gmin, "gmhd", 0, "apple"),
	FBOX_DEFINE_FLAGS_S( GF_QT_BOX_TYPE_ALIS, alis, "dref", 0, 1, "apple"),
	FBOX_DEFINE_FLAGS_S( GF_QT_BOX_TYPE_CIOS, alis, "dref", 0, 1, "apple"),
	FBOX_DEFINE_S( GF_QT_BOX_TYPE_CLEF, clef, "tapt", 0, "apple"),
	FBOX_DEFINE_S( GF_QT_BOX_TYPE_PROF, clef, "tapt", 0, "apple"),
	FBOX_DEFINE_S( GF_QT_BOX_TYPE_ENOF, clef, "tapt", 0, "apple"),
	BOX_DEFINE_S( GF_QT_BOX_TYPE_WAVE, def_parent, "audio_sample_entry", "apple"),
	FBOX_DEFINE_S( GF_QT_BOX_TYPE_CHAN, chan, "audio_sample_entry", 0, "apple"),
	BOX_DEFINE_S( GF_QT_BOX_TYPE_FRMA, frma, "wave", "apple"),
	BOX_DEFINE_S( GF_QT_BOX_TYPE_TERMINATOR, unkn, "wave", "apple"),
	BOX_DEFINE_S( GF_QT_BOX_TYPE_ENDA, chrm, "wave", "apple"),
	BOX_DEFINE_S( GF_QT_BOX_TYPE_TMCD, def_parent, "gmhd", "apple"),
	BOX_DEFINE_S( GF_QT_BOX_TYPE_NAME, unkn, "tmcd", "apple"),
	FBOX_DEFINE_S( GF_QT_BOX_TYPE_TCMI, tcmi, "tmcd", 0, "apple"),
	BOX_DEFINE_S( GF_QT_BOX_TYPE_FIEL, fiel, "video_sample_entry", "apple"),
	BOX_DEFINE_S( GF_QT_BOX_TYPE_GAMA, gama, "video_sample_entry", "apple"),
	BOX_DEFINE_S( GF_QT_BOX_TYPE_CHRM, chrm, "video_sample_entry", "apple"),
	BOX_DEFINE_S( GF_ISOM_SUBTYPE_TEXT, unkn, "gmhd", "apple"),

	//QT and prores sample entry types
	BOX_DEFINE_S( GF_QT_BOX_TYPE_TMCD, tmcd, "stsd", "apple"),
	BOX_DEFINE_S( GF_QT_SUBTYPE_C608, gen_sample_entry, "stsd", "apple"),
	BOX_DEFINE_S_CHILD( GF_QT_SUBTYPE_APCH, video_sample_entry, "stsd", "apple"),
	BOX_DEFINE_S_CHILD( GF_QT_SUBTYPE_APCO, video_sample_entry, "stsd", "apple"),
	BOX_DEFINE_S_CHILD( GF_QT_SUBTYPE_APCS, video_sample_entry, "stsd", "apple"),
	BOX_DEFINE_S_CHILD( GF_QT_SUBTYPE_APCN, video_sample_entry, "stsd", "apple"),
	BOX_DEFINE_S_CHILD( GF_QT_SUBTYPE_AP4X, video_sample_entry, "stsd", "apple"),
	BOX_DEFINE_S_CHILD( GF_QT_SUBTYPE_AP4H, video_sample_entry, "stsd", "apple"),
	BOX_DEFINE_S_CHILD( GF_QT_SUBTYPE_RAW_AUD, audio_sample_entry, "stsd", "apple"),
	BOX_DEFINE_S_CHILD( GF_QT_SUBTYPE_TWOS, audio_sample_entry, "stsd", "apple"),
	BOX_DEFINE_S_CHILD( GF_QT_SUBTYPE_SOWT, audio_sample_entry, "stsd", "apple"),
	BOX_DEFINE_S_CHILD( GF_QT_SUBTYPE_FL32, audio_sample_entry, "stsd", "apple"),
	BOX_DEFINE_S_CHILD( GF_QT_SUBTYPE_FL64, audio_sample_entry, "stsd", "apple"),
	BOX_DEFINE_S_CHILD( GF_QT_SUBTYPE_IN24, audio_sample_entry, "stsd", "apple"),
	BOX_DEFINE_S_CHILD( GF_QT_SUBTYPE_IN32, audio_sample_entry, "stsd", "apple"),
	BOX_DEFINE_S_CHILD( GF_QT_SUBTYPE_ULAW, audio_sample_entry, "stsd", "apple"),
	BOX_DEFINE_S_CHILD( GF_QT_SUBTYPE_ALAW, audio_sample_entry, "stsd", "apple"),
	BOX_DEFINE_S_CHILD( GF_QT_SUBTYPE_ADPCM, audio_sample_entry, "stsd", "apple"),
	BOX_DEFINE_S_CHILD( GF_QT_SUBTYPE_IMA_ADPCM, audio_sample_entry, "stsd", "apple"),
	BOX_DEFINE_S_CHILD( GF_QT_SUBTYPE_DVCA, audio_sample_entry, "stsd", "apple"),
	BOX_DEFINE_S_CHILD( GF_QT_SUBTYPE_QDMC, audio_sample_entry, "stsd", "apple"),
	BOX_DEFINE_S_CHILD( GF_QT_SUBTYPE_QDMC2, audio_sample_entry, "stsd", "apple"),
	BOX_DEFINE_S_CHILD( GF_QT_SUBTYPE_QCELP, audio_sample_entry, "stsd", "apple"),
	BOX_DEFINE_S_CHILD( GF_QT_SUBTYPE_kMP3, audio_sample_entry, "stsd", "apple"),

	BOX_DEFINE_S_CHILD( GF_QT_SUBTYPE_RAW_VID, video_sample_entry, "stsd", "apple"),
	BOX_DEFINE_S_CHILD( GF_QT_SUBTYPE_YUYV, video_sample_entry, "stsd", "apple"),
	BOX_DEFINE_S_CHILD( GF_QT_SUBTYPE_UYVY, video_sample_entry, "stsd", "apple"),
	BOX_DEFINE_S_CHILD( GF_QT_SUBTYPE_YUV444, video_sample_entry, "stsd", "apple"),
	BOX_DEFINE_S_CHILD( GF_QT_SUBTYPE_YUVA444, video_sample_entry, "stsd", "apple"),
	BOX_DEFINE_S_CHILD( GF_QT_SUBTYPE_YUV422_10, video_sample_entry, "stsd", "apple"),
	BOX_DEFINE_S_CHILD( GF_QT_SUBTYPE_YUV444_10, video_sample_entry, "stsd", "apple"),
	BOX_DEFINE_S_CHILD( GF_QT_SUBTYPE_YUV422_16, video_sample_entry, "stsd", "apple"),
	BOX_DEFINE_S_CHILD( GF_QT_SUBTYPE_YUV420, video_sample_entry, "stsd", "apple"),
	BOX_DEFINE_S_CHILD( GF_QT_SUBTYPE_I420, video_sample_entry, "stsd", "apple"),
	BOX_DEFINE_S_CHILD( GF_QT_SUBTYPE_IYUV, video_sample_entry, "stsd", "apple"),
	BOX_DEFINE_S_CHILD( GF_QT_SUBTYPE_YV12, video_sample_entry, "stsd", "apple"),
	BOX_DEFINE_S_CHILD( GF_QT_SUBTYPE_YVYU, video_sample_entry, "stsd", "apple"),
	BOX_DEFINE_S_CHILD( GF_QT_SUBTYPE_RGBA, video_sample_entry, "stsd", "apple"),
	BOX_DEFINE_S_CHILD( GF_QT_SUBTYPE_ABGR, video_sample_entry, "stsd", "apple"),

	FBOX_DEFINE_S(GF_QT_BOX_TYPE_STPS, stss, "stbl", 0, "apple"),

	//dolby boxes
	BOX_DEFINE_S_CHILD( GF_ISOM_BOX_TYPE_AC3, audio_sample_entry, "stsd", "dolby"),
	BOX_DEFINE_S_CHILD( GF_ISOM_BOX_TYPE_EC3, audio_sample_entry, "stsd", "dolby"),
	BOX_DEFINE_S( GF_ISOM_BOX_TYPE_DAC3, dac3, "ac-3 wave enca", "dolby"),
	{GF_ISOM_BOX_TYPE_DEC3, dec3_box_new, dac3_box_del, dac3_box_read, dac3_box_write, dac3_box_size, dac3_box_dump, 0, 0, 0, "ec-3 wave enca", "dolby" },
	BOX_DEFINE_S(GF_ISOM_BOX_TYPE_DVCC, dvcC, "dvav dva1 dvhe dvh1 dav1 avc1 avc2 avc3 avc4 hev1 hvc1 av01 encv resv", "DolbyVision"),
	BOX_DEFINE_S(GF_ISOM_BOX_TYPE_DVVC, dvvC, "dvav dva1 dvhe dvh1 dav1 avc1 avc2 avc3 avc4 hev1 hvc1 av01 encv resv", "DolbyVision"),
	BOX_DEFINE_S_CHILD(GF_ISOM_BOX_TYPE_DVHE, video_sample_entry, "stsd", "DolbyVision"),
	BOX_DEFINE_S_CHILD(GF_ISOM_BOX_TYPE_DVH1, video_sample_entry, "stsd", "DolbyVision"),
	BOX_DEFINE_S_CHILD(GF_ISOM_BOX_TYPE_DVA1, video_sample_entry, "stsd", "DolbyVision"),
	BOX_DEFINE_S_CHILD(GF_ISOM_BOX_TYPE_DVAV, video_sample_entry, "stsd", "DolbyVision"),
	BOX_DEFINE_S_CHILD(GF_ISOM_BOX_TYPE_DAV1, video_sample_entry, "stsd", "DolbyVision"),
	BOX_DEFINE_S_CHILD( GF_ISOM_BOX_TYPE_MLPA, audio_sample_entry, "stsd", "dolby"),
	BOX_DEFINE_S( GF_ISOM_BOX_TYPE_DMLP, dmlp, "mlpa enca", "dolby"),

	BOX_DEFINE_S( GF_ISOM_BOX_TYPE_HVCE, hvcc, "hvc1 hev1 hvc2 hev2 encv resv ipco dvh1 dvhe", "DolbyVision"),
	BOX_DEFINE_S( GF_ISOM_BOX_TYPE_AVCE, avcc, "avc1 avc2 avc3 avc4 encv resv ipco dva1 dvav", "DolbyVision"),


	//Adobe boxes
#ifndef GPAC_DISABLE_ISOM_ADOBE
	FBOX_DEFINE_S( GF_ISOM_BOX_TYPE_ABST, abst, "file", 0, "adobe"),
	FBOX_DEFINE_S( GF_ISOM_BOX_TYPE_AFRA, afra, "file", 0, "adobe"),
	FBOX_DEFINE_S( GF_ISOM_BOX_TYPE_ASRT, asrt, "abst", 0, "adobe"),
	FBOX_DEFINE_S( GF_ISOM_BOX_TYPE_AFRT, afrt, "abst", 0, "adobe"),
#endif
	/*Adobe's protection boxes*/
	FBOX_DEFINE_S_CHILD( GF_ISOM_BOX_TYPE_ADKM, adkm, "schi", 0, "adobe"),
	FBOX_DEFINE_S_CHILD( GF_ISOM_BOX_TYPE_AHDR, ahdr, "adkm", 0, "adobe"),
	FBOX_DEFINE_S( GF_ISOM_BOX_TYPE_ADAF, adaf, "adkm", 0, "adobe"),
	FBOX_DEFINE_S_CHILD( GF_ISOM_BOX_TYPE_APRM, aprm, "ahdr", 0, "adobe"),
	FBOX_DEFINE_S( GF_ISOM_BOX_TYPE_AEIB, aeib, "aprm", 0, "adobe"),
	FBOX_DEFINE_S_CHILD( GF_ISOM_BOX_TYPE_AKEY, akey, "aprm", 0, "adobe"),
	BOX_DEFINE_S( GF_ISOM_BOX_TYPE_FLXS, flxs, "akey", "adobe"),

	FBOX_DEFINE_S( GF_ISOM_BOX_TYPE_TRIK, trik, "traf", 0, "dece"),
	FBOX_DEFINE_S( GF_ISOM_BOX_TYPE_BLOC, bloc, "file", 0, "dece"),
	FBOX_DEFINE_FLAGS_S(GF_ISOM_BOX_TYPE_AINF, ainf, "moov", 0, 0x000001, "dece"),


	//internally handled UUID for smooth - the code points are only used during creation and assigned to UUIDBox->internal4CC
	//the box type is still "uuid", and the factory is used to read/write/size/dump the code
	BOX_DEFINE_S(GF_ISOM_BOX_UUID_TENC, piff_tenc, "schi", "smooth"),
	BOX_DEFINE_S(GF_ISOM_BOX_UUID_PSEC, piff_psec, "trak traf", "smooth"),
	BOX_DEFINE_S(GF_ISOM_BOX_UUID_PSSH, piff_pssh, "moov moof", "smooth"),
	BOX_DEFINE_S(GF_ISOM_BOX_UUID_TFXD, tfxd, "traf", "smooth"),
	BOX_DEFINE_S(GF_ISOM_BOX_UUID_TFRF, tfrf, "traf", "smooth"),
	BOX_DEFINE_S(GF_ISOM_BOX_UUID_MSSM, uuid, "file", "smooth"),
	BOX_DEFINE_S(GF_ISOM_BOX_UUID_TFRF, uuid, "traf", "smooth"),


	//J2K boxes
	BOX_DEFINE_S_CHILD(GF_ISOM_BOX_TYPE_MJP2, video_sample_entry, "stsd", "j2k"),
	BOX_DEFINE_S_CHILD(GF_ISOM_BOX_TYPE_JP2H, jp2h, "mjp2 encv", "j2k"),
	BOX_DEFINE_S(GF_ISOM_BOX_TYPE_IHDR, ihdr, "jp2h", "j2k"),

	/* Image tracks */
	BOX_DEFINE_S_CHILD(GF_ISOM_BOX_TYPE_JPEG, video_sample_entry, "stsd", "apple"),
	BOX_DEFINE_S_CHILD(GF_ISOM_BOX_TYPE_JP2K, video_sample_entry, "stsd", "apple"),
	BOX_DEFINE_S_CHILD(GF_ISOM_BOX_TYPE_PNG, video_sample_entry, "stsd", "apple"),


	//flac in ISOBMFF boxes
	BOX_DEFINE_S_CHILD(GF_ISOM_BOX_TYPE_FLAC, audio_sample_entry, "stsd", "Flac"),
	FBOX_DEFINE_S(GF_ISOM_BOX_TYPE_DFLA, dfla, "fLaC enca", 0, "Flac"),

	BOX_DEFINE_S(GF_ISOM_BOX_TYPE_XTRA, xtra, "udta", "WMA"),

	FBOX_DEFINE_S( GF_ISOM_BOX_TYPE_ST3D, st3d, "video_sample_entry", 0, "youtube"),

	BOX_DEFINE_S( GF_ISOM_BOX_TYPE_SV3D, def_parent, "video_sample_entry", "youtube"),
	FBOX_DEFINE_S( GF_ISOM_BOX_TYPE_SVHD, svhd, "sv3d", 0, "youtube"),
	BOX_DEFINE_S( GF_ISOM_BOX_TYPE_PROJ, def_parent, "sv3d", "youtube"),
	FBOX_DEFINE_S( GF_ISOM_BOX_TYPE_PRHD, prhd, "proj", 0, "youtube"),
	FBOX_DEFINE_S( GF_ISOM_BOX_TYPE_CBMP, proj_type, "proj", 0, "youtube"),
	FBOX_DEFINE_S( GF_ISOM_BOX_TYPE_EQUI, proj_type, "proj", 0, "youtube"),
	FBOX_DEFINE_S( GF_ISOM_BOX_TYPE_MESH, proj_type, "proj", 0, "youtube"),

	FBOX_DEFINE_S( GF_ISOM_BOX_TYPE_KEYS, keys, "meta", 0, "apple"),

	BOX_DEFINE_S(GF_QT_SUBTYPE_ALAC, unkn, "stsd", "apple"),
	BOX_DEFINE_S(GF_ISOM_SUBTYPE_FFV1, unkn, "stsd", "ffmpeg"),

	BOX_DEFINE_S(GF_4CC('v','c','-','1'), unkn, "stsd", "smpte-RP2025"),
	BOX_DEFINE_S(GF_4CC('d','v','c','1'), unkn, "vc-1 ipco", "smpte-RP2025"),

	BOX_DEFINE_S(GF_4CC('G','M','C','W'), unkn, "stsd", "GPAC"),
	BOX_DEFINE_S(GF_4CC('G','M','C','C'), unkn, "GMCW", "GPAC"),

	/* for now we don't parse these*/
	BOX_DEFINE_S(GF_4CC('u','n','c','C'), unkn, "video_sample_entry ipco", "rawff"),
	BOX_DEFINE_S(GF_4CC('c','m','p','d'), unkn, "video_sample_entry ipco", "rawff"),
	BOX_DEFINE_S(GF_4CC('c','p','a','l'), unkn, "video_sample_entry ipco", "rawff"),
	BOX_DEFINE_S(GF_4CC('c','p','a','t'), unkn, "video_sample_entry ipco", "rawff"),
	BOX_DEFINE_S(GF_4CC('c','l','e','v'), unkn, "video_sample_entry ipco", "rawff"),
	BOX_DEFINE_S(GF_4CC('s','p','l','z'), unkn, "video_sample_entry ipco", "rawff"),
	BOX_DEFINE_S(GF_4CC('s','n','u','c'), unkn, "video_sample_entry ipco", "rawff"),
	BOX_DEFINE_S(GF_4CC('s','b','p','m'), unkn, "video_sample_entry ipco", "rawff"),
	BOX_DEFINE_S(GF_4CC('c','l','o','c'), unkn, "video_sample_entry ipco", "rawff"),
	BOX_DEFINE_S(GF_4CC('f','p','c','k'), unkn, "video_sample_entry ipco", "rawff"),
	BOX_DEFINE_S(GF_4CC('d','i','s','i'), unkn, "video_sample_entry ipco", "rawff"),
	BOX_DEFINE_S(GF_4CC('d','e','p','i'), unkn, "video_sample_entry ipco", "rawff"),

/*
	GF_ISOM_BOX_TYPE_CBMP	= GF_4CC( 'c', 'b', 'm', 'p' ),
	GF_ISOM_BOX_TYPE_EQUI	= GF_4CC( 'e', 'q', 'u', 'i' ),
	GF_ISOM_BOX_TYPE_MSHP	= GF_4CC( 'm', 's', 'h', 'p' ),
	GF_ISOM_BOX_TYPE_MESH	= GF_4CC( 'm', 'e', 's', 'h' ),
*/

};

Bool gf_box_valid_in_parent(GF_Box *a, const char *parent_4cc)
{
	if (!a || !a->registry || !a->registry->parents_4cc) return GF_FALSE;
	if (strstr(a->registry->parents_4cc, parent_4cc) != NULL) return GF_TRUE;
	return GF_FALSE;
}

GF_EXPORT
u32 gf_isom_get_num_supported_boxes()
{
	return sizeof(box_registry) / sizeof(struct box_registry_entry);
}

void gf_isom_registry_disable(u32 boxCode, Bool disable)
{
	u32 i=0, count = gf_isom_get_num_supported_boxes();
	for (i=1; i<count; i++) {
		if (box_registry[i].box_4cc==boxCode) {
			box_registry[i].disabled = disable;
			return;
		}
	}
}

static u32 get_box_reg_idx(u32 boxCode, u32 parent_type, u32 start_from)
{
	u32 i=0, count = gf_isom_get_num_supported_boxes();
	if (!start_from) start_from = 1;

	for (i=start_from; i<count; i++) {
		u32 start_par_from;
		if (box_registry[i].box_4cc != boxCode)
			continue;

		if (!parent_type)
			return i;
		if (strstr(box_registry[i].parents_4cc, gf_4cc_to_str(parent_type)) != NULL)
			return i;
		if (strstr(box_registry[i].parents_4cc, "*") != NULL)
			return i;

		if (strstr(box_registry[i].parents_4cc, "sample_entry") == NULL)
			continue;

		/*parent is a sample entry, check if the parent_type matches a sample entry box (eg its parent must be stsd)*/

		if (parent_type==GF_QT_SUBTYPE_RAW)
			return i;

		start_par_from = 0;
		while (parent_type) {
			//locate parent registry
			u32 j = get_box_reg_idx(parent_type, 0, start_par_from);
			if (!j) break;
			//if parent registry has "stsd" as parent, this is a sample entry
			if (box_registry[j].parents_4cc && (strstr(box_registry[j].parents_4cc, "stsd") != NULL))
				return i;
			start_par_from = j+1;
		}
	}
	return 0;
}

GF_Box *gf_isom_box_new_ex(u32 boxType, u32 parentType, Bool skip_logs, Bool is_root_box)
{
	GF_Box *a;
	s32 idx = get_box_reg_idx(boxType, parentType, 0);
	if (idx==0) {
#ifndef GPAC_DISABLE_LOG
		if (!skip_logs && (boxType != GF_ISOM_BOX_TYPE_UNKNOWN) && (boxType != GF_ISOM_BOX_TYPE_UUID)) {
			switch (parentType) {
			case GF_ISOM_BOX_TYPE_ILST:
			case GF_ISOM_BOX_TYPE_META:
			case GF_ISOM_BOX_TYPE_UDTA:
			case GF_ISOM_BOX_TYPE_UNKNOWN:
			case GF_ISOM_BOX_TYPE_iTunesSpecificInfo:
			case GF_QT_BOX_TYPE_WAVE:
				break;
			//some sample descritions are handled as generic ones but we know them, don't warn
			case GF_ISOM_BOX_TYPE_STSD:
				//fallthrough
			default:
				if (boxType==GF_ISOM_BOX_TYPE_GDAT) break;

				if (is_root_box) {
					GF_LOG(GF_LOG_INFO, GF_LOG_CONTAINER, ("[iso file] Unknown top-level box type %s\n", gf_4cc_to_str(boxType)));
				} else if (parentType) {
					char szName[GF_4CC_MSIZE];
					strcpy(szName, gf_4cc_to_str(parentType));
					GF_LOG(GF_LOG_INFO, GF_LOG_CONTAINER, ("[iso file] Unknown box type %s in parent %s\n", gf_4cc_to_str(boxType), szName));
				} else {
					GF_LOG(GF_LOG_INFO, GF_LOG_CONTAINER, ("[iso file] Unknown box type %s\n", gf_4cc_to_str(boxType)));
				}
				break;
			}
		}
#endif
        if (boxType==GF_ISOM_BOX_TYPE_UUID) {
            a = uuid_box_new();
            if (a) a->registry = &box_registry[1];
        } else {
            a = unkn_box_new();
            if (a) {
            	((GF_UnknownBox *)a)->original_4cc = boxType;
            	a->registry = &box_registry[0];
			}
        }
		return a;
	}
	a = box_registry[idx].new_fn();

	if (a) {
		if (a->type!=GF_ISOM_BOX_TYPE_UUID) {
			if (a->type==GF_ISOM_BOX_TYPE_UNKNOWN) {
				((GF_UnknownBox *)a)->original_4cc = boxType;
			} else {
				a->type = boxType;
			}
		}
		a->registry = &box_registry[idx];

		if ((a->type==GF_ISOM_BOX_TYPE_COLR) && (parentType==GF_ISOM_BOX_TYPE_JP2H)) {
			((GF_ColourInformationBox *)a)->is_jp2 = GF_TRUE;
		}

	}
	return a;
}

GF_EXPORT
GF_Box *gf_isom_box_new(u32 boxType)
{
	return gf_isom_box_new_ex(boxType, 0, 0, GF_FALSE);
}

GF_Err gf_isom_box_array_read(GF_Box *parent, GF_BitStream *bs)
{
	GF_Err e;
	u32 parent_type = parent->type;
	GF_Box *a = NULL;
	Bool skip_logs = (gf_bs_get_cookie(bs) & GF_ISOM_BS_COOKIE_NO_LOGS ) ? GF_TRUE : GF_FALSE;

	//we may have terminators in some QT files (4 bytes set to 0 ...)
	while (parent->size>=8) {
		e = gf_isom_box_parse_ex(&a, bs, parent_type, GF_FALSE, skip_logs ? 0 : parent->size);
		if (e) {
			if (a) gf_isom_box_del(a);
			return (e==GF_SKIP_BOX) ? GF_OK : e;
		}
		//sub box parsing aborted with no error
		if (!a) return GF_OK;

		if (parent->size < a->size) {
			if (!skip_logs) {
				GF_LOG(GF_LOG_WARNING, GF_LOG_CONTAINER, ("[iso file] Box \"%s\" is larger than container box\n", gf_4cc_to_str(a->type)));
			}
			if (!gf_opts_get_bool("core", "no-check")) {
				gf_isom_box_del(a);
				return GF_ISOM_INVALID_FILE;
			}
			parent->size = 0;
		} else {
			parent->size -= a->size;
		}

		//check container validity
		if (parent_type && strlen(a->registry->parents_4cc)) {
			Bool parent_OK = GF_FALSE;
			const char *parent_code = gf_4cc_to_str(parent->type);
			if (parent->type == GF_ISOM_BOX_TYPE_UNKNOWN)
				parent_code = gf_4cc_to_str( ((GF_UnknownBox*)parent)->original_4cc );
			if (strstr(a->registry->parents_4cc, parent_code) != NULL) {
				parent_OK = GF_TRUE;
			} else if (!strcmp(a->registry->parents_4cc, "*") || strstr(a->registry->parents_4cc, "* ") || strstr(a->registry->parents_4cc, " *")) {
				parent_OK = GF_TRUE;
			} else {
				//parent must be a sample entry
				if (strstr(a->registry->parents_4cc, "sample_entry") !=	NULL) {
					//parent is in an stsd
					if (strstr(parent->registry->parents_4cc, "stsd") != NULL) {
						if (strstr(a->registry->parents_4cc, "video_sample_entry") !=	NULL) {
							if (((GF_SampleEntryBox*)parent)->internal_type==GF_ISOM_SAMPLE_ENTRY_VIDEO) {
								parent_OK = GF_TRUE;
							}
						} else {
							parent_OK = GF_TRUE;
						}
					}
				}
				//other types are sample formats, eg a 3GPP text sample, RTP hint sample or VTT cue. Not handled at this level
				else if (a->type==GF_ISOM_BOX_TYPE_UNKNOWN) parent_OK = GF_TRUE;
				else if (a->type==GF_ISOM_BOX_TYPE_UUID) parent_OK = GF_TRUE;
			}
			if (! parent_OK && !skip_logs) {
				GF_LOG(GF_LOG_WARNING, GF_LOG_CONTAINER, ("[iso file] Box \"%s\" is invalid in container %s\n", gf_4cc_to_str(a->type), parent_code));
			}
		}

		//always register boxes
		if (!parent->child_boxes) {
			parent->child_boxes = gf_list_new();
			if (!parent->child_boxes) return GF_OUT_OF_MEM;
		}
		e = gf_list_add(parent->child_boxes, a);
		if (e) return e;

		if (parent->registry && parent->registry->add_rem_fn) {
			e = parent->registry->add_rem_fn(parent, a, GF_FALSE);
			if (e) {
				if (e == GF_ISOM_INVALID_MEDIA) return GF_OK;
				//if the box is no longer present, consider it destroyed
				if (gf_list_find(parent->child_boxes, a) >=0) {
					gf_isom_box_del_parent(&parent->child_boxes, a);
				}
				return e;
			}
		}
	}
	return GF_OK;
}

GF_EXPORT
void gf_isom_box_del(GF_Box *a)
{
	GF_List *child_boxes;
	const struct box_registry_entry *a_box_registry;
	if (!a) return;

	child_boxes	= a->child_boxes;
	a->child_boxes = NULL;

	a_box_registry = a->registry;
	if (!a_box_registry) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[iso file] Delete invalid box type %s without registry\n", gf_4cc_to_str(a->type) ));
	} else {
		a_box_registry->del_fn(a);
	}
	//delete the other boxes after deleting the box for dumper case where all child boxes are stored in otherbox
	if (child_boxes) {
		gf_isom_box_array_del(child_boxes);
	}
}


GF_Err gf_isom_box_read(GF_Box *a, GF_BitStream *bs)
{
	if (!a) return GF_BAD_PARAM;
	if (!a->registry) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[iso file] Read invalid box type %s without registry\n", gf_4cc_to_str(a->type) ));
		return GF_ISOM_INVALID_FILE;
	}
	return a->registry->read_fn(a, bs);
}

#ifndef GPAC_DISABLE_ISOM_WRITE

GF_Err gf_isom_box_write_listing(GF_Box *a, GF_BitStream *bs)
{
	if (!a) return GF_BAD_PARAM;
	if (!a->registry) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[iso file] Write invalid box type %s without registry\n", gf_4cc_to_str(a->type) ));
		return GF_ISOM_INVALID_FILE;
	}
	return a->registry->write_fn(a, bs);
}


void gf_isom_check_position(GF_Box *s, GF_Box *child, u32 *pos)
{
	if (!s || !s->child_boxes || !child || !pos) return;
	if (s->internal_flags & GF_ISOM_ORDER_FREEZE)
		return;

	s32 cur_pos = gf_list_find(s->child_boxes, child);

	//happens when partially cloning boxes
	if (cur_pos < 0) return;

	if (cur_pos != (s32) *pos) {
		gf_list_del_item(s->child_boxes, child);
		gf_list_insert(s->child_boxes, child, *pos);
	}
	(*pos)++;
}

void gf_isom_check_position_list(GF_Box *s, GF_List *childlist, u32 *pos)
{
	u32 i, count;
	if (!s || (s->internal_flags & GF_ISOM_ORDER_FREEZE))
		return;
	count = gf_list_count(childlist);
	for (i=0; i<count; i++) {
		GF_Box *child = gf_list_get(childlist, i);
		gf_isom_check_position(s, child, pos);
	}
}


GF_EXPORT
GF_Err gf_isom_box_write(GF_Box *a, GF_BitStream *bs)
{
	GF_Err e;
	u64 pos = gf_bs_get_position(bs);
	if (!a) return GF_BAD_PARAM;
	//box has been disabled, do not write
	if (!a->size) return GF_OK;

	if (a->registry->disabled) {
		GF_LOG(GF_LOG_DEBUG, GF_LOG_CONTAINER, ("[iso file] Box %s disabled registry, skip write\n", gf_4cc_to_str(a->type)));
		return GF_OK;
	}

	GF_LOG(GF_LOG_DEBUG, GF_LOG_CONTAINER, ("[iso file] Box %s size %d write\n", gf_4cc_to_str(a->type), a->size));
	e = gf_isom_box_write_listing(a, bs);
	if (e) return e;
	if (a->child_boxes) {
		e = gf_isom_box_array_write(a, a->child_boxes, bs);
	}
	pos = gf_bs_get_position(bs) - pos;
	if (pos != a->size) {
		if (a->type != GF_ISOM_BOX_TYPE_MDAT) {
			GF_LOG(GF_LOG_WARNING, GF_LOG_CONTAINER, ("[iso file] Box %s wrote "LLU" bytes but size is "LLU"\n", gf_4cc_to_str(a->type), pos, a->size ));
		}
	}
	return e;
}

static GF_Err gf_isom_box_size_listing(GF_Box *a)
{
	if (!a) return GF_BAD_PARAM;
	if (!a->registry) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[iso file] Size invalid box type %s without registry\n", gf_4cc_to_str(a->type) ));
		return GF_ISOM_INVALID_FILE;
	}
	a->size = 8;

	if (a->type == GF_ISOM_BOX_TYPE_UUID) {
		a->size += 16;
	}
	//the large size is handled during write, cause at this stage we don't know the size
	if (a->registry->max_version_plus_one) {
		a->size += 4;
	}
	return a->registry->size_fn(a);
}

GF_EXPORT
GF_Err gf_isom_box_size(GF_Box *a)
{
	GF_Err e;
	if (!a) return GF_BAD_PARAM;
	if (a->registry->disabled) {
		a->size = 0;
		return GF_OK;
	}
	e = gf_isom_box_size_listing(a);
	if (e) return e;
	//box size set to 0 (not even a header), abort traversal
	if (!a->size) return GF_OK;

	if (a->child_boxes) {
		e = gf_isom_box_array_size(a, a->child_boxes);
		if (e) return e;
	}
	return GF_OK;
}

#endif /*GPAC_DISABLE_ISOM_WRITE*/

static GF_Err gf_isom_full_box_read(GF_Box *ptr, GF_BitStream *bs)
{
	if (ptr->registry->max_version_plus_one) {
		GF_FullBox *self = (GF_FullBox *) ptr;
		ISOM_DECREASE_SIZE(ptr, 4)
		self->version = gf_bs_read_u8(bs);
		self->flags = gf_bs_read_u24(bs);
	}
	return GF_OK;
}


GF_EXPORT
GF_Err gf_isom_dump_supported_box(u32 idx, FILE * trace)
{
	u32 i;
	u32 nb_versions=0;
	GF_Err e;

	if (box_registry[idx].max_version_plus_one) {
		nb_versions = box_registry[idx].max_version_plus_one - 1;
	}
	for (i = 0; i <= nb_versions; i++) {
		GF_Box *a = box_registry[idx].new_fn();
		if (!a) return GF_OUT_OF_MEM;

		a->registry = &box_registry[idx];

		if (box_registry[idx].alt_4cc) {
			if (a->type==GF_ISOM_BOX_TYPE_REFT)
				((GF_TrackReferenceTypeBox*)a)->reference_type = box_registry[idx].alt_4cc;
			else if (a->type==GF_ISOM_BOX_TYPE_REFI)
				((GF_ItemReferenceTypeBox*)a)->reference_type = box_registry[idx].alt_4cc;
			else if (a->type==GF_ISOM_BOX_TYPE_TRGT)
				((GF_TrackGroupTypeBox*)a)->group_type = box_registry[idx].alt_4cc;
			else if (a->type==GF_ISOM_BOX_TYPE_SGPD)
				((GF_SampleGroupDescriptionBox*)a)->grouping_type = box_registry[idx].alt_4cc;
			else if (a->type==GF_ISOM_BOX_TYPE_GRPT)
				((GF_EntityToGroupTypeBox*)a)->grouping_type = box_registry[idx].alt_4cc;
		}
		if (box_registry[idx].max_version_plus_one) {
			((GF_FullBox *)a)->version = i;
		}
		if (box_registry[idx].flags) {
			u32 flag_mask=1;
			u32 flags = box_registry[idx].flags;
			((GF_FullBox *)a)->flags = 0;
			e = gf_isom_box_dump(a, trace);

			//we dump all flags individually and this for all version, in order to simplify the XSLT processing
			while (!e) {
				u32 flag = flags & flag_mask;
				flag_mask <<= 1;
				if (flag) {
					((GF_FullBox *)a)->flags = flag;
					e = gf_isom_box_dump(a, trace);
				}
				if (flag_mask > flags) break;
				if (flag_mask == 0x80000000) break;
			}

		} else {
			e = gf_isom_box_dump(a, trace);
		}

		gf_isom_box_del(a);
	}
	return e;
}

GF_EXPORT
u32 gf_isom_get_supported_box_type(u32 idx)
{
	return box_registry[idx].box_4cc;
}

#ifndef GPAC_DISABLE_ISOM_DUMP

GF_Err gf_isom_box_dump_start_ex(GF_Box *a, const char *name, FILE * trace, Bool force_version)
{
	gf_fprintf(trace, "<%s ", name);
	if (a->size > 0xFFFFFFFF) {
		gf_fprintf(trace, "LargeSize=\""LLU"\" ", a->size);
	} else {
		gf_fprintf(trace, "Size=\"%u\" ", (u32) a->size);
	}
	if (a->type==GF_ISOM_BOX_TYPE_UNKNOWN) {
		gf_fprintf(trace, "Type=\"%s\" ", gf_4cc_to_str(((GF_UnknownBox*)a)->original_4cc));
	} else {
		gf_fprintf(trace, "Type=\"%s\" ", gf_4cc_to_str(a->type));
	}

	if (a->type == GF_ISOM_BOX_TYPE_UUID) {
		u32 i;
		gf_fprintf(trace, "UUID=\"{");
		for (i=0; i<16; i++) {
			gf_fprintf(trace, "%02X", (unsigned char) ((GF_UUIDBox*)a)->uuid[i]);
			if ((i<15) && (i%4)==3) gf_fprintf(trace, "-");
		}
		gf_fprintf(trace, "}\" ");
	}

	if (a->registry->max_version_plus_one || force_version) {
		gf_fprintf(trace, "Version=\"%d\" Flags=\"%d\" ", ((GF_FullBox*)a)->version,((GF_FullBox*)a)->flags);
	}
	gf_fprintf(trace, "Specification=\"%s\" ", a->registry->spec);

	//don't write containers in test mode, that would require rewriting hashes whenever spec changes
	if (!gf_sys_is_test_mode()) {
		gf_fprintf(trace, "Container=\"%s\" ", a->registry->parents_4cc);
	}
	return GF_OK;
}
GF_Err gf_isom_box_dump_start(GF_Box *a, const char *name, FILE * trace)
{
	return gf_isom_box_dump_start_ex(a, name, trace, GF_FALSE);
}

GF_Err gf_isom_box_dump(void *ptr, FILE * trace)
{
	GF_Box *a = (GF_Box *) ptr;

	if (!a) {
		gf_fprintf(trace, "<!--ERROR: NULL Box Found-->\n");
		return GF_OK;
	}
	if (!a->registry) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[isom] trying to dump box %s not registered\n", gf_4cc_to_str(a->type) ));
		return GF_ISOM_INVALID_FILE;
	}
	a->registry->dump_fn(a, trace);
	return GF_OK;
}

void gf_isom_box_dump_done(const char *name, GF_Box *ptr, FILE *trace)
{
	if (ptr && ptr->child_boxes) {
		gf_isom_box_array_dump(ptr->child_boxes, trace);
	}
	if (name)
		gf_fprintf(trace, "</%s>\n", name);
}

Bool gf_isom_box_is_file_level(GF_Box *s)
{
	if (!s || !s->registry) return GF_FALSE;
	if (strstr(s->registry->parents_4cc, "file")!= NULL) return GF_TRUE;
	if (strstr(s->registry->parents_4cc, "*")!= NULL) return GF_TRUE;
	return GF_FALSE;
}
#endif


GF_Box *gf_isom_box_find_child(GF_List *children, u32 code)
{
	u32 i, count;
	if (!children) return NULL;
	count = gf_list_count(children);
	for (i=0; i<count; i++) {
		GF_Box *c = gf_list_get(children, i);
		if (c->type==code) return c;

		if (c->type==GF_ISOM_BOX_TYPE_UNKNOWN) {
			if (((GF_UnknownBox*)c)->original_4cc==code)
				return c;
		}
		if (c->type==GF_ISOM_BOX_TYPE_UUID) {
			if (((GF_UUIDBox*)c)->internal_4cc==code)
				return c;
		}
	}
	return NULL;
}

Bool gf_isom_box_check_unique(GF_List *children, GF_Box *a)
{
	u32 i, count;
	if (!children) return GF_TRUE;
	count = gf_list_count(children);
	for (i=0; i<count; i++) {
		GF_Box *c = gf_list_get(children, i);
		if (c==a) continue;
		if (c->type==a->type) return GF_FALSE;
	}
	return GF_TRUE;
}

void gf_isom_box_del_parent(GF_List **child_boxes, GF_Box*b)
{
	if (child_boxes) {
		gf_list_del_item(*child_boxes, b);
		if (!gf_list_count(*child_boxes)) {
			gf_list_del(*child_boxes);
			*child_boxes = NULL;
		}
	}
	gf_isom_box_del(b);
}

void gf_isom_box_remove_from_parent(GF_Box *parent_box, GF_Box *box)
{
	if (parent_box && parent_box->registry && parent_box->registry->add_rem_fn) {
		parent_box->registry->add_rem_fn(parent_box, box, GF_TRUE);
	}
}

GF_Box *gf_isom_box_new_parent(GF_List **parent, u32 code)
{
	GF_Box *b = gf_isom_box_new(code);
	if (!b) return NULL;
	if (! (*parent) ) (*parent)  = gf_list_new();
	gf_list_add(*parent, b);
	return b;
}

void gf_isom_box_freeze_order(GF_Box *box)
{
	u32 i=0;
	GF_Box *child;
	if (!box) return;
	box->internal_flags |= GF_ISOM_ORDER_FREEZE;

	while ((child = gf_list_enum(box->child_boxes, &i))) {
		gf_isom_box_freeze_order(child);
	}

}
#endif /*GPAC_DISABLE_ISOM*/

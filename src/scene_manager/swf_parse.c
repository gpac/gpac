/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2000-2012
 *					All rights reserved
 *
 *  This file is part of GPAC / Scene Management sub-project
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


#include <gpac/nodes_mpeg4.h>
#include <gpac/internal/swf_dev.h>
#include <gpac/avparse.h>

#ifndef GPAC_DISABLE_SWF_IMPORT


#include <zlib.h>

enum
{
	SWF_END = 0,
	SWF_SHOWFRAME = 1,
	SWF_DEFINESHAPE = 2,
	SWF_FREECHARACTER = 3,
	SWF_PLACEOBJECT = 4,
	SWF_REMOVEOBJECT = 5,
	SWF_DEFINEBITS = 6,
	SWF_DEFINEBITSJPEG = 6,
	SWF_DEFINEBUTTON = 7,
	SWF_JPEGTABLES = 8,
	SWF_SETBACKGROUNDCOLOR = 9,
	SWF_DEFINEFONT = 10,
	SWF_DEFINETEXT = 11,
	SWF_DOACTION = 12,
	SWF_DEFINEFONTINFO = 13,
	SWF_DEFINESOUND = 14,
	SWF_STARTSOUND = 15,
	SWF_DEFINEBUTTONSOUND = 17,
	SWF_SOUNDSTREAMHEAD = 18,
	SWF_SOUNDSTREAMBLOCK = 19,
	SWF_DEFINEBITSLOSSLESS = 20,
	SWF_DEFINEBITSJPEG2 = 21,
	SWF_DEFINESHAPE2 = 22,
	SWF_DEFINEBUTTONCXFORM = 23,
	SWF_PROTECT = 24,
	SWF_PLACEOBJECT2 = 26,
	SWF_REMOVEOBJECT2 = 28,
	SWF_DEFINESHAPE3 = 32,
	SWF_DEFINETEXT2 = 33,
	SWF_DEFINEBUTTON2 = 34,
	SWF_DEFINEBITSJPEG3 = 35,
	SWF_DEFINEBITSLOSSLESS2 = 36,
	SWF_DEFINEEDITTEXT = 37,
	SWF_DEFINEMOVIE = 38,
	SWF_DEFINESPRITE = 39,
	SWF_NAMECHARACTER = 40,
	SWF_SERIALNUMBER = 41,
	SWF_GENERATORTEXT = 42,
	SWF_FRAMELABEL = 43,
	SWF_SOUNDSTREAMHEAD2 = 45,
	SWF_DEFINEMORPHSHAPE = 46,
	SWF_DEFINEFONT2 = 48,
	SWF_TEMPLATECOMMAND = 49,
	SWF_GENERATOR3 = 51,
	SWF_EXTERNALFONT = 52,
	SWF_EXPORTASSETS = 56,
	SWF_IMPORTASSETS	= 57,
	SWF_ENABLEDEBUGGER = 58,
	SWF_MX0 = 59,
	SWF_MX1 = 60,
	SWF_MX2 = 61,
	SWF_MX3 = 62,
	SWF_MX4 = 63,
	SWF_REFLEX = 777
};


static void swf_init_decompress(SWFReader *read)
{
	u32 size, dst_size;
	uLongf destLen;
	char *src, *dst;

	assert(gf_bs_get_size(read->bs)-8 < 0x80000000); /*must fit within 32 bits*/
	size = (u32) gf_bs_get_size(read->bs)-8;
	dst_size = read->length;
	src = gf_malloc(sizeof(char)*size);
	dst = gf_malloc(sizeof(char)*dst_size);
	memset(dst, 0, sizeof(char)*8);
	gf_bs_read_data(read->bs, src, size);
	dst_size -= 8;
	destLen = (uLongf)dst_size;
	uncompress((Bytef *) dst+8, &destLen, (Bytef *) src, size);
	dst_size += 8;
	gf_free(src);
	read->mem = dst;
	gf_bs_del(read->bs);
	read->bs = gf_bs_new(read->mem, dst_size, GF_BITSTREAM_READ);
	gf_bs_skip_bytes(read->bs, 8);
}


static GF_Err swf_seek_file_to(SWFReader *read, u32 size)
{
	return gf_bs_seek(read->bs, size);
}

static u32 swf_get_file_pos(SWFReader *read)
{
	return (u32) gf_bs_get_position(read->bs);
}

static u32 swf_read_data(SWFReader *read, char *data, u32 data_size)
{
	return gf_bs_read_data(read->bs, data, data_size);
}

static u32 swf_read_int(SWFReader *read, u32 nbBits)
{
	return gf_bs_read_int(read->bs, nbBits);
}

static s32 swf_read_sint(SWFReader *read, u32 nbBits)
{
	u32 r = 0;
	u32 i;
	if (!nbBits)return 0;
	r = swf_read_int(read, 1) ? 0xFFFFFFFF : 0;
	for (i=1; i<nbBits; i++) {
		r <<= 1;
		r |= swf_read_int(read, 1);
	}
	return (s32) r;
}

static u32 swf_align(SWFReader *read)
{
	return gf_bs_align(read->bs);
}

static void swf_skip_data(SWFReader *read, u32 size)
{
	while (size && !read->ioerr) {
		swf_read_int(read, 8);
		size --;
	}
}

static void swf_get_rec(SWFReader *read, SWFRec *rc)
{
	u32 nbbits;
	swf_align(read);
	nbbits = swf_read_int(read, 5);
	rc->x = FLT2FIX( swf_read_sint(read, nbbits) * SWF_TWIP_SCALE );
	rc->w = FLT2FIX( swf_read_sint(read, nbbits) * SWF_TWIP_SCALE );
	rc->w -= rc->x;
	rc->y = FLT2FIX( swf_read_sint(read, nbbits) * SWF_TWIP_SCALE );
	rc->h = FLT2FIX( swf_read_sint(read, nbbits) * SWF_TWIP_SCALE );
	rc->h -= rc->y;
}

static u32 swf_get_32(SWFReader *read)
{
	u32 val, res;
	val = swf_read_int(read, 32);
	res = (val&0xFF);
	res <<=8;
	res |= ((val>>8)&0xFF);
	res<<=8;
	res |= ((val>>16)&0xFF);
	res<<=8;
	res|= ((val>>24)&0xFF);
	return res;
}

static u16 swf_get_16(SWFReader *read)
{
	u16 val, res;
	val = swf_read_int(read, 16);
	res = (val&0xFF);
	res <<=8;
	res |= ((val>>8)&0xFF);
	return res;
}

static s16 swf_get_s16(SWFReader *read)
{
	s16 val;
	u8 v1;
	v1 = swf_read_int(read, 8);
	val = swf_read_sint(read, 8);
	val = (val<<8)&0xFF00;
	val |= (v1&0xFF);
	return val;
}

static u32 swf_get_color(SWFReader *read)
{
	u32 res;
	res = 0xFF00;
	res |= swf_read_int(read, 8);
	res<<=8;
	res |= swf_read_int(read, 8);
	res<<=8;
	res |= swf_read_int(read, 8);
	return res;
}

static u32 swf_get_argb(SWFReader *read)
{
	u32 res, al;
	res = swf_read_int(read, 8);
	res<<=8;
	res |= swf_read_int(read, 8);
	res<<=8;
	res |= swf_read_int(read, 8);
	al = swf_read_int(read, 8);
	return ((al<<24) | res);
}

static u32 swf_get_matrix(SWFReader *read, GF_Matrix2D *mat)
{
	u32 bits_read;
	u32 flag, nb_bits;

	memset(mat, 0, sizeof(GF_Matrix2D));
	mat->m[0] = mat->m[4] = FIX_ONE;

	bits_read = swf_align(read);

	flag = swf_read_int(read, 1);
	bits_read += 1;
	if (flag) {
		nb_bits = swf_read_int(read, 5);
#ifdef GPAC_FIXED_POINT
		mat->m[0] = swf_read_sint(read, nb_bits);
		mat->m[4] = swf_read_sint(read, nb_bits);
#else
		mat->m[0] = (Float) swf_read_sint(read, nb_bits);
		mat->m[0] /= 0x10000;
		mat->m[4] = (Float) swf_read_sint(read, nb_bits);
		mat->m[4] /= 0x10000;
#endif
		bits_read += 5 + 2*nb_bits;
	}
	flag = swf_read_int(read, 1);
	bits_read += 1;
	if (flag) {
		nb_bits = swf_read_int(read, 5);
		/*WATCHOUT FOR ORDER*/
#ifdef GPAC_FIXED_POINT
		mat->m[3] = swf_read_sint(read, nb_bits);
		mat->m[1] = swf_read_sint(read, nb_bits);
#else
		mat->m[3] = (Float) swf_read_sint(read, nb_bits);
		mat->m[3] /= 0x10000;
		mat->m[1] = (Float) swf_read_sint(read, nb_bits);
		mat->m[1] /= 0x10000;
#endif
		bits_read += 5 + 2*nb_bits;
	}
	nb_bits = swf_read_int(read, 5);
	bits_read += 5 + 2*nb_bits;
	if (nb_bits) {
		mat->m[2] = FLT2FIX( swf_read_sint(read, nb_bits) * SWF_TWIP_SCALE );
		mat->m[5] = FLT2FIX( swf_read_sint(read, nb_bits) * SWF_TWIP_SCALE );
	}
	return bits_read;
}

#define SWF_COLOR_SCALE				(1/256.0f)

static void swf_get_colormatrix(SWFReader *read, GF_ColorMatrix *cmat)
{
	Bool has_add, has_mul;
	u32 nbbits;
	memset(cmat, 0, sizeof(GF_ColorMatrix));
	cmat->m[0] = cmat->m[6] = cmat->m[12] = cmat->m[18] = FIX_ONE;

	has_add = swf_read_int(read, 1);
	has_mul = swf_read_int(read, 1);
	nbbits = swf_read_int(read, 4);
	if (has_mul) {
		cmat->m[0] = FLT2FIX( swf_read_sint(read, nbbits) * SWF_COLOR_SCALE );
		cmat->m[6] = FLT2FIX( swf_read_sint(read, nbbits) * SWF_COLOR_SCALE );
		cmat->m[12] = FLT2FIX( swf_read_sint(read, nbbits) * SWF_COLOR_SCALE );
		cmat->m[18] = FLT2FIX( swf_read_sint(read, nbbits) * SWF_COLOR_SCALE );
	}
	if (has_add) {
		cmat->m[4] = FLT2FIX( swf_read_sint(read, nbbits) * SWF_COLOR_SCALE );
		cmat->m[9] = FLT2FIX( swf_read_sint(read, nbbits) * SWF_COLOR_SCALE );
		cmat->m[14] = FLT2FIX( swf_read_sint(read, nbbits) * SWF_COLOR_SCALE );
		cmat->m[19] = FLT2FIX( swf_read_sint(read, nbbits) * SWF_COLOR_SCALE );
	}
	cmat->identity = 0;
	if ((cmat->m[0] == cmat->m[6])
	        && (cmat->m[0] == cmat->m[12])
	        && (cmat->m[0] == cmat->m[18])
	        && (cmat->m[0] == FIX_ONE)
	        && (cmat->m[4] == cmat->m[9])
	        && (cmat->m[4] == cmat->m[14])
	        && (cmat->m[4] == cmat->m[19])
	        && (cmat->m[4] == 0))
		cmat->identity = 1;
}

static char *swf_get_string(SWFReader *read)
{
	char szName[1024];
	char *name;
	u32 i = 0;

	if (read->size>1024) {
		name = gf_malloc(sizeof(char)*read->size);
	} else {
		name = szName;
	}
	while (1) {
		name[i] = swf_read_int(read, 8);
		if (!name[i]) break;
		i++;
	}
	if (read->size>1024) {
		return gf_realloc(name, sizeof(char)*(strlen(name)+1));
	} else {
		return gf_strdup(szName);
	}
}

static SWFShapeRec *swf_new_shape_rec()
{
	SWFShapeRec *style;
	GF_SAFEALLOC(style, SWFShapeRec);
	if (!style) return NULL;
	GF_SAFEALLOC(style->path, SWFPath);
	if (!style->path) {
		gf_free(style);
		return NULL;
	}
	return style;
}

static SWFShapeRec *swf_clone_shape_rec(SWFShapeRec *old_sr)
{
	SWFShapeRec *new_sr = (SWFShapeRec *)gf_malloc(sizeof(SWFShapeRec));
	memcpy(new_sr, old_sr, sizeof(SWFShapeRec));
	new_sr->path = (SWFPath*)gf_malloc(sizeof(SWFPath));
	memset(new_sr->path, 0, sizeof(SWFPath));

	if (old_sr->nbGrad) {
		new_sr->grad_col = (u32*)gf_malloc(sizeof(u32) * old_sr->nbGrad);
		memcpy(new_sr->grad_col, old_sr->grad_col, sizeof(u32) * old_sr->nbGrad);
		new_sr->grad_ratio = (u8*)gf_malloc(sizeof(u8) * old_sr->nbGrad);
		memcpy(new_sr->grad_ratio, old_sr->grad_ratio, sizeof(u8) * old_sr->nbGrad);
	}
	return new_sr;
}

/*parse/append fill and line styles*/
static void swf_parse_styles(SWFReader *read, u32 revision, SWFShape *shape, u32 *bits_fill, u32 *bits_line)
{
	u32 i, j, count;
	SWFShapeRec *style;

	swf_align(read);

	/*get fill styles*/
	count = swf_read_int(read, 8);
	if (revision && (count== 0xFF)) count = swf_get_16(read);
	if (count) {
		for (i=0; i<count; i++) {
			style = swf_new_shape_rec();

			style->solid_col = 0xFF00FF00;
			style->type = swf_read_int(read, 8);

			/*gradient fill*/
			if (style->type & 0x10) {
				swf_get_matrix(read, &style->mat);
				swf_align(read);
				style->nbGrad = swf_read_int(read, 8);
				if (style->nbGrad) {
					style->grad_col = (u32 *) gf_malloc(sizeof(u32) * style->nbGrad);
					style->grad_ratio = (u8 *) gf_malloc(sizeof(u8) * style->nbGrad);
					for (j=0; j<style->nbGrad; j++) {
						style->grad_ratio[j] = swf_read_int(read, 8);
						if (revision==2) style->grad_col[j] = swf_get_argb(read);
						else style->grad_col[j] = swf_get_color(read);
					}
					style->solid_col = style->grad_col[0];

					/*make sure we have keys between 0 and 1.0 for BIFS (0 and 255 in swf)*/
					if (style->grad_ratio[0] != 0) {
						u32 i;
						u32 *grad_col;
						u8 *grad_ratio;
						grad_ratio = (u8 *) gf_malloc(sizeof(u8) * (style->nbGrad+1));
						grad_col = (u32 *) gf_malloc(sizeof(u32) * (style->nbGrad+1));
						grad_col[0] = style->grad_col[0];
						grad_ratio[0] = 0;
						for (i=0; i<style->nbGrad; i++) {
							grad_col[i+1] = style->grad_col[i];
							grad_ratio[i+1] = style->grad_ratio[i];
						}
						gf_free(style->grad_col);
						style->grad_col = grad_col;
						gf_free(style->grad_ratio);
						style->grad_ratio = grad_ratio;
						style->nbGrad++;
					}
					if (style->grad_ratio[style->nbGrad-1] != 255) {
						u32 *grad_col = (u32*)gf_malloc(sizeof(u32) * (style->nbGrad+1));
						u8 *grad_ratio = (u8*)gf_malloc(sizeof(u8) * (style->nbGrad+1));
						memcpy(grad_col, style->grad_col, sizeof(u32) * style->nbGrad);
						memcpy(grad_ratio, style->grad_ratio, sizeof(u8) * style->nbGrad);
						grad_col[style->nbGrad] = style->grad_col[style->nbGrad-1];
						grad_ratio[style->nbGrad] = 255;
						gf_free(style->grad_col);
						style->grad_col = grad_col;
						gf_free(style->grad_ratio);
						style->grad_ratio = grad_ratio;
						style->nbGrad++;
					}

				} else {
					style->solid_col = 0xFF;
				}
			}
			/*bitmap fill*/
			else if (style->type & 0x40) {
				style->img_id = swf_get_16(read);
				if (style->img_id == 65535) {
					style->img_id = 0;
					style->type = 0;
					style->solid_col = 0xFF00FFFF;
				}
				swf_get_matrix(read, &style->mat);
			}
			/*solid fill*/
			else {
				if (revision==2) style->solid_col = swf_get_argb(read);
				else style->solid_col = swf_get_color(read);
			}
			gf_list_add(shape->fill_right, style);
			style = swf_clone_shape_rec(style);
			gf_list_add(shape->fill_left, style);
		}
	}

	swf_align(read);
	/*get line styles*/
	count = swf_read_int(read, 8);
	if (revision && (count==0xFF)) count = swf_get_16(read);
	if (count) {
		for (i=0; i<count; i++) {
			style = swf_new_shape_rec();
			gf_list_add(shape->lines, style);
			style->width = FLT2FIX( swf_get_16(read) * SWF_TWIP_SCALE );
			if (revision==2) style->solid_col = swf_get_argb(read);
			else style->solid_col = swf_get_color(read);
		}
	}

	swf_align(read);
	*bits_fill = swf_read_int(read, 4);
	*bits_line = swf_read_int(read, 4);
}

static void swf_path_realloc_pts(SWFPath *path, u32 nbPts)
{
	if (path)
		path->pts = (SFVec2f*)gf_realloc(path->pts, sizeof(SFVec2f) * (path->nbPts + nbPts));
}

static void swf_path_add_com(SWFShapeRec *sr, SFVec2f pt, SFVec2f ctr, u32 type)
{
	/*not an error*/
	if (!sr) return;

	sr->path->types = (u32*)gf_realloc(sr->path->types, sizeof(u32) * (sr->path->nbType+1));

	sr->path->types[sr->path->nbType] = type;
	switch (type) {
	case 2:
		swf_path_realloc_pts(sr->path, 2);
		sr->path->pts[sr->path->nbPts] = ctr;
		sr->path->pts[sr->path->nbPts+1] = pt;
		sr->path->nbPts+=2;
		break;
	case 1:
	default:
		swf_path_realloc_pts(sr->path, 1);
		sr->path->pts[sr->path->nbPts] = pt;
		sr->path->nbPts++;
		break;
	}
	sr->path->nbType++;
}

static void swf_referse_path(SWFPath *path)
{
	u32 i, j, pti, ptj;
	u32 *types;
	SFVec2f *pts;

	if (path->nbType<=1) return;

	types = (u32 *) gf_malloc(sizeof(u32) * path->nbType);
	pts = (SFVec2f *) gf_malloc(sizeof(SFVec2f) * path->nbPts);


	/*need first moveTo*/
	types[0] = 0;
	pts[0] = path->pts[path->nbPts - 1];
	pti = path->nbPts - 2;
	ptj = 1;
	j=1;

	for (i=0; i<path->nbType-1; i++) {
		types[j] = path->types[path->nbType - i - 1];
		switch (types[j]) {
		case 2:
			assert(ptj<=path->nbPts-2);
			pts[ptj] = path->pts[pti];
			pts[ptj+1] = path->pts[pti-1];
			pti-=2;
			ptj+=2;
			break;
		case 1:
			assert(ptj<=path->nbPts-1);
			pts[ptj] = path->pts[pti];
			pti--;
			ptj++;
			break;
		case 0:
			assert(ptj<=path->nbPts-1);
			pts[ptj] = path->pts[pti];
			pti--;
			ptj++;
			break;
		}
		j++;
	}
	gf_free(path->pts);
	path->pts = pts;
	gf_free(path->types);
	path->types = types;
}

static void swf_free_shape_rec(SWFShapeRec *ptr)
{
	if (ptr->grad_col) gf_free(ptr->grad_col);
	if (ptr->grad_ratio) gf_free(ptr->grad_ratio);
	if (ptr->path) {
		if (ptr->path->pts) gf_free(ptr->path->pts);
		if (ptr->path->types) gf_free(ptr->path->types);
		if (ptr->path->idx) gf_free(ptr->path->idx);
		gf_free(ptr->path);
	}
	gf_free(ptr);
}

static void swf_reset_rec_list(GF_List *recs)
{
	while (gf_list_count(recs)) {
		SWFShapeRec *tmp = (SWFShapeRec *)gf_list_get(recs, 0);
		gf_list_rem(recs, 0);
		swf_free_shape_rec(tmp);
	}
}

static void swf_append_path(SWFPath *a, SWFPath *b)
{
	if (b->nbType<=1) return;

	a->pts = (SFVec2f*)gf_realloc(a->pts, sizeof(SFVec2f) * (a->nbPts + b->nbPts));
	memcpy(&a->pts[a->nbPts], b->pts, sizeof(SFVec2f)*b->nbPts);
	a->nbPts += b->nbPts;

	a->types = (u32*)gf_realloc(a->types, sizeof(u32)*(a->nbType+ b->nbType));
	memcpy(&a->types[a->nbType], b->types, sizeof(u32)*b->nbType);
	a->nbType += b->nbType;
}

static void swf_path_add_type(SWFPath *path, u32 val)
{
	path->types = (u32*)gf_realloc(path->types, sizeof(u32) * (path->nbType + 1));
	path->types[path->nbType] = val;
	path->nbType++;
}

static void swf_resort_path(SWFPath *a, SWFReader *read)
{
	u32 idx, i, j;
	GF_List *paths;
	SWFPath *sorted, *p, *np;

	if (!a->nbType) return;

	paths = gf_list_new();
	GF_SAFEALLOC(sorted, SWFPath);
	if (!sorted) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_PARSER, ("[SWF Parsing] Fail to allocate path for resorting\n"));
		return;
	}
	swf_path_realloc_pts(sorted, 1);
	sorted->pts[sorted->nbPts] = a->pts[0];
	sorted->nbPts++;
	swf_path_add_type(sorted, 0);
	gf_list_add(paths, sorted);

	/*1- split all paths*/
	idx = 1;
	for (i=1; i<a->nbType; i++) {
		switch (a->types[i]) {
		case 2:
			swf_path_realloc_pts(sorted, 2);
			sorted->pts[sorted->nbPts] = a->pts[idx];
			sorted->pts[sorted->nbPts+1] = a->pts[idx+1];
			sorted->nbPts+=2;
			swf_path_add_type(sorted, 2);
			idx += 2;
			break;
		case 1:
			swf_path_realloc_pts(sorted, 1);
			sorted->pts[sorted->nbPts] = a->pts[idx];
			sorted->nbPts+=1;
			swf_path_add_type(sorted, 1);
			idx += 1;
			break;
		case 0:
			GF_SAFEALLOC(sorted , SWFPath);
			swf_path_realloc_pts(sorted, 1);
			sorted->pts[sorted->nbPts] = a->pts[idx];
			sorted->nbPts++;
			swf_path_add_type(sorted, 0);
			gf_list_add(paths, sorted);
			idx += 1;
			break;
		}
	}

restart:
	for (i=0; i<gf_list_count(paths); i++) {
		p = (SWFPath*)gf_list_get(paths, i);

		for (j=i+1; j < gf_list_count(paths); j++) {
			np = (SWFPath*)gf_list_get(paths, j);

			/*check if any next subpath ends at the same place we're starting*/
			if ((np->pts[np->nbPts-1].x == p->pts[0].x) && (np->pts[np->nbPts-1].y == p->pts[0].y)) {
				u32 k;
				idx = 1;
				for (k=1; k<p->nbType; k++) {
					switch (p->types[k]) {
					case 2:
						swf_path_realloc_pts(np, 2);
						np->pts[np->nbPts] = p->pts[idx];
						np->pts[np->nbPts+1] = p->pts[idx+1];
						np->nbPts+=2;
						swf_path_add_type(np, 2);
						idx += 2;
						break;
					case 1:
						swf_path_realloc_pts(np, 1);
						np->pts[np->nbPts] = p->pts[idx];
						np->nbPts+=1;
						swf_path_add_type(np, 1);
						idx += 1;
						break;
					default:
						assert(0);
						break;
					}
				}
				gf_free(p->pts);
				gf_free(p->types);
				gf_free(p);
				gf_list_rem(paths, i);
				goto restart;
			}
			/*check if any next subpath starts at the same place we're ending*/
			else if ((p->pts[p->nbPts-1].x == np->pts[0].x) && (p->pts[p->nbPts-1].y == np->pts[0].y)) {
				u32 k;
				idx = 1;
				for (k=1; k<np->nbType; k++) {
					switch (np->types[k]) {
					case 2:
						swf_path_realloc_pts(p, 2);
						p->pts[p->nbPts] = np->pts[idx];
						p->pts[p->nbPts+1] = np->pts[idx+1];
						p->nbPts+=2;
						swf_path_add_type(p, 2);
						idx += 2;
						break;
					case 1:
						swf_path_realloc_pts(p, 1);
						p->pts[p->nbPts] = np->pts[idx];
						p->nbPts+=1;
						swf_path_add_type(p, 1);
						idx += 1;
						break;
					default:
						assert(0);
						break;
					}
				}
				gf_free(np->pts);
				gf_free(np->types);
				gf_free(np);
				gf_list_rem(paths, j);
				j--;
			}
		}
	}

	/*reassemble path*/
	gf_free(a->pts);
	gf_free(a->types);
	memset(a, 0, sizeof(SWFPath));

	while (gf_list_count(paths)) {
		sorted = (SWFPath*)gf_list_get(paths, 0);
		if (read->flat_limit==0) {
			swf_append_path(a, sorted);
		} else {
			Bool prev_is_line_to = 0;
			idx = 0;
			for (i=0; i<sorted->nbType; i++) {
				switch (sorted->types[i]) {
				case 2:
					swf_path_realloc_pts(a, 2);
					a->pts[a->nbPts] = sorted->pts[idx];
					a->pts[a->nbPts+1] = sorted->pts[idx+1];
					a->nbPts+=2;
					swf_path_add_type(a, 2);
					idx += 2;
					prev_is_line_to = 0;
					break;
				case 1:
					if (prev_is_line_to) {
						Fixed angle;
						Bool flatten = 0;
						SFVec2f v1, v2;
						v1.x = a->pts[a->nbPts-1].x - a->pts[a->nbPts-2].x;
						v1.y = a->pts[a->nbPts-1].y - a->pts[a->nbPts-2].y;
						v2.x = a->pts[a->nbPts-1].x - sorted->pts[idx].x;
						v2.y = a->pts[a->nbPts-1].y - sorted->pts[idx].y;

						angle = gf_mulfix(v1.x,v2.x) + gf_mulfix(v1.y,v2.y);
						/*get magnitudes*/
						v1.x = gf_v2d_len(&v1);
						v2.x = gf_v2d_len(&v2);
						if (!v1.x || !v2.x) flatten = 1;
						else {
							Fixed h_pi = GF_PI / 2;
							angle = gf_divfix(angle, gf_mulfix(v1.x, v2.x));
							if (angle + FIX_EPSILON >= FIX_ONE) angle = 0;
							else if (angle - FIX_EPSILON <= -FIX_ONE) angle = GF_PI;
							else angle = gf_acos(angle);

							if (angle<0) angle += h_pi;
							angle = ABSDIFF(angle, h_pi);
							if (angle < read->flat_limit) flatten = 1;
						}
						if (flatten) {
							a->pts[a->nbPts-1] = sorted->pts[idx];
							idx++;
							read->flatten_points++;
							break;
						}
					}
					swf_path_realloc_pts(a, 1);
					a->pts[a->nbPts] = sorted->pts[idx];
					a->nbPts+=1;
					swf_path_add_type(a, 1);
					idx += 1;
					prev_is_line_to = 1;
					break;
				case 0:
					swf_path_realloc_pts(a, 1);
					a->pts[a->nbPts] = sorted->pts[idx];
					a->nbPts+=1;
					swf_path_add_type(a, 0);
					idx += 1;
					prev_is_line_to = 0;
					break;
				}
			}
		}
		gf_free(sorted->pts);
		gf_free(sorted->types);
		gf_free(sorted);
		gf_list_rem(paths, 0);
	}
	gf_list_del(paths);
}

/*
	Notes on SWF->BIFS conversion - some ideas taken from libswfdec
	A single fillStyle has 2 associated path, one used for left fill, one for right fill
	This is then a 4 step process:
	1- blindly parse swf shape, and add point/lines to the proper left/right path
	2- for each fillStyles, revert the right path so that it becomes a left path
	3- concatenate left and right paths
	4- resort all subelements of the final path, making sure moveTo introduced by the SWF coding (due to style changes)
	are removed.
		Ex: if path is
			A->C, B->A, C->B = moveTo(A), lineTo(C), moveTo(B), lineTo (A), moveTo(C), lineTo(B)
		we restort and remove unneeded moves to get
			A->C->B = moveTo(A), lineTo(C), lineTo(B), lineTo(A)
*/
static GF_Err swf_flush_shape(SWFReader *read, SWFShape *shape, SWFFont *font, Bool last_shape)
{
	GF_Err e;
	SWFShapeRec *sf0, *sf1, *sl;
	u32 i, count;
	count = gf_list_count(shape->fill_left);
	for (i=0; i<count; i++) {
		sf0 = (SWFShapeRec*)gf_list_get(shape->fill_left, i);
		sf1 = (SWFShapeRec*)gf_list_get(shape->fill_right, i);
		/*reverse right path*/
		swf_referse_path(sf1->path);
		/*concatenate with left path*/
		swf_append_path(sf0->path, sf1->path);
		/*resort all path curves*/
		swf_resort_path(sf0->path, read);
	}
	/*remove dummy fill_left*/
	for (i=0; i<gf_list_count(shape->fill_left); i++) {
		sf0 = (SWFShapeRec*)gf_list_get(shape->fill_left, i);
		if (sf0->path->nbType<=1) {
			gf_list_rem(shape->fill_left, i);
			swf_free_shape_rec(sf0);
			i--;
		}
	}
	/*remove dummy lines*/
	for (i=0; i<gf_list_count(shape->lines); i++) {
		sl = (SWFShapeRec*)gf_list_get(shape->lines, i);
		if (sl->path->nbType<1) {
			gf_list_rem(shape->lines, i);
			swf_free_shape_rec(sl);
			i--;
		} else {
			swf_resort_path(sl->path, read);
		}
	}

	/*now translate a flash shape record into BIFS*/
	e = read->define_shape(read, shape, font, last_shape);

	/*delete shape*/
	swf_reset_rec_list(shape->fill_left);
	swf_reset_rec_list(shape->fill_right);
	swf_reset_rec_list(shape->lines);
	return e;
}

static GF_Err swf_parse_shape_def(SWFReader *read, SWFFont *font, u32 revision)
{
	u32 nbBits, comType;
	s32 x, y;
	SFVec2f orig, ctrl, end;
	Bool flag;
	u32 fill0, fill1, strike;
	u32 bits_fill, bits_line;
	SWFShape shape;
	Bool is_empty;
	SWFShapeRec *sf0, *sf1, *sl;

	memset(&shape, 0, sizeof(SWFShape));
	shape.fill_left = gf_list_new();
	shape.fill_right = gf_list_new();
	shape.lines = gf_list_new();
	ctrl.x = ctrl.y = 0;
	swf_align(read);

	/*regular shape - get initial styles*/
	if (!font) {
		shape.ID = swf_get_16(read);
		swf_get_rec(read, &shape.rc);
		swf_parse_styles(read, revision, &shape, &bits_fill, &bits_line);
	}
	/*glyph*/
	else {
		bits_fill = swf_read_int(read, 4);
		bits_line = swf_read_int(read, 4);

		/*fonts are usually defined without styles*/
		if ((read->tag == SWF_DEFINEFONT) || (read->tag==SWF_DEFINEFONT2)) {
			sf0 = swf_new_shape_rec();
			gf_list_add(shape.fill_right, sf0);
			sf0 = swf_new_shape_rec();
			gf_list_add(shape.fill_left, sf0);
			sf0->solid_col = 0xFF000000;
			sf0->type = 0;
		}
	}

	is_empty = 1;

	/*parse all points*/
	fill0 = fill1 = strike = 0;
	sf0 = sf1 = sl = NULL;
	x = y = 0;
	while (1) {
		flag = swf_read_int(read, 1);
		if (!flag) {
			Bool new_style = swf_read_int(read, 1);
			Bool set_strike = swf_read_int(read, 1);
			Bool set_fill1 = swf_read_int(read, 1);
			Bool set_fill0 = swf_read_int(read, 1);
			Bool move_to = swf_read_int(read, 1);
			/*end of shape*/
			if (!new_style && !set_strike && !set_fill0 && !set_fill1 && !move_to) break;

			is_empty = 0;

			if (move_to) {
				nbBits = swf_read_int(read, 5);
				x = swf_read_sint(read, nbBits);
				y = swf_read_sint(read, nbBits);
			}
			if (set_fill0) fill0 = swf_read_int(read, bits_fill);
			if (set_fill1) fill1 = swf_read_int(read, bits_fill);
			if (set_strike) strike = swf_read_int(read, bits_line);
			/*looks like newStyle does not append styles but define a new set - old styles can no
			longer be referenced*/
			if (new_style) {
				/*flush current shape record into BIFS*/
				swf_flush_shape(read, &shape, font, 0);
				swf_parse_styles(read, revision, &shape, &bits_fill, &bits_line);
			}

			if (read->flags & GF_SM_SWF_NO_LINE) strike = 0;

			/*moveto*/
			orig.x = FLT2FIX( x * SWF_TWIP_SCALE );
			orig.y = FLT2FIX( y * SWF_TWIP_SCALE );
			end = orig;

			sf0 = fill0 ? (SWFShapeRec*)gf_list_get(shape.fill_left, fill0 - 1) : NULL;
			sf1 = fill1 ? (SWFShapeRec*)gf_list_get(shape.fill_right, fill1 - 1) : NULL;
			sl = strike ? (SWFShapeRec*)gf_list_get(shape.lines, strike - 1) : NULL;

			if (move_to) {
				swf_path_add_com(sf0, end, ctrl, 0);
				swf_path_add_com(sf1, end, ctrl, 0);
				swf_path_add_com(sl, end, ctrl, 0);
			} else {
				if (set_fill0) swf_path_add_com(sf0, end, ctrl, 0);
				if (set_fill1) swf_path_add_com(sf1, end, ctrl, 0);
				if (set_strike) swf_path_add_com(sl, end, ctrl, 0);
			}

		} else {
			flag = swf_read_int(read, 1);
			/*quadratic curve*/
			if (!flag) {
				nbBits = 2 + swf_read_int(read, 4);
				x += swf_read_sint(read, nbBits);
				y += swf_read_sint(read, nbBits);
				ctrl.x = FLT2FIX( x * SWF_TWIP_SCALE );
				ctrl.y = FLT2FIX( y * SWF_TWIP_SCALE );
				x += swf_read_sint(read, nbBits);
				y += swf_read_sint(read, nbBits);
				end.x = FLT2FIX( x * SWF_TWIP_SCALE );
				end.y = FLT2FIX( y * SWF_TWIP_SCALE );
				/*curveTo*/
				comType = 2;
			}
			/*straight line*/
			else {
				nbBits = 2 + swf_read_int(read, 4);
				flag = swf_read_int(read, 1);
				if (flag) {
					x += swf_read_sint(read, nbBits);
					y += swf_read_sint(read, nbBits);
				} else {
					flag = swf_read_int(read, 1);
					if (flag) {
						y += swf_read_sint(read, nbBits);
					} else {
						x += swf_read_sint(read, nbBits);
					}
				}
				/*lineTo*/
				comType = 1;
				end.x = FLT2FIX( x * SWF_TWIP_SCALE );
				end.y = FLT2FIX( y * SWF_TWIP_SCALE );
			}
			swf_path_add_com(sf0, end, ctrl, comType);
			swf_path_add_com(sf1, end, ctrl, comType);
			swf_path_add_com(sl, end, ctrl, comType);
		}
	}

	if (is_empty) {
		swf_reset_rec_list(shape.fill_left);
		swf_reset_rec_list(shape.fill_right);
		swf_reset_rec_list(shape.lines);
	}

	swf_align(read);

	/*now translate a flash shape record*/
	swf_flush_shape(read, &shape, font, 1);

	/*delete shape*/
	swf_reset_rec_list(shape.fill_left);
	swf_reset_rec_list(shape.fill_right);
	swf_reset_rec_list(shape.lines);
	gf_list_del(shape.fill_left);
	gf_list_del(shape.fill_right);
	gf_list_del(shape.lines);

	return GF_OK;
}

SWFFont *swf_find_font(SWFReader *read, u32 ID)
{
	u32 i, count;
	count = gf_list_count(read->fonts);
	for (i=0; i<count; i++) {
		SWFFont *ft = (SWFFont *)gf_list_get(read->fonts, i);
		if (ft->fontID==ID) return ft;
	}
	return NULL;
}

static DispShape *swf_get_depth_entry(SWFReader *read, u32 Depth, Bool create)
{
	u32 i;
	DispShape *tmp;
	i=0;
	while ((tmp = (DispShape *)gf_list_enum(read->display_list, &i))) {
		if (tmp->depth == Depth) return tmp;
	}
	if (!create) return NULL;
	GF_SAFEALLOC(tmp , DispShape);
	tmp->depth = Depth;
	tmp->char_id = 0;
	gf_list_add(read->display_list, tmp);

	memset(&tmp->mat, 0, sizeof(GF_Matrix2D));
	tmp->mat.m[0] = tmp->mat.m[4] = FIX_ONE;

	memset(&tmp->cmat, 0, sizeof(GF_ColorMatrix));
	tmp->cmat.m[0] = tmp->cmat.m[6] = tmp->cmat.m[12] = tmp->cmat.m[18] = FIX_ONE;
	tmp->cmat.identity = 1;
	return tmp;
}


static GF_Err swf_func_skip(SWFReader *read)
{
	if (!read) return GF_OK;
	swf_skip_data(read, read->size);
	return read->ioerr;
}

static GF_Err swf_set_backcol(SWFReader *read)
{
	u32 col = swf_get_color(read);
	return read->set_backcol(read, col);
}

static GF_Err swf_actions(SWFReader *read, u32 mask, u32 key)
{
	u32 skip_actions = 0;
	u8 action_code = swf_read_int(read, 8);
	u16 length = 0;
	read->has_interact = 1;


#define DO_ACT(_code) { act.type = _code; read->action(read, &act); break; }

	while (action_code) {
		if (action_code > 0x80) length = swf_get_16(read);
		else length = 0;

		if (read->no_as || skip_actions) {
			swf_skip_data(read, length);
			if (skip_actions) skip_actions--;
		} else {
			SWFAction act;
			memset(&act, 0, sizeof(SWFAction));
			act.button_mask = mask;
			act.button_key = key;

			switch (action_code) {
			/* SWF 3 Action Model */
			case 0x81: /* goto frame */
				assert (length == 2);
				act.type = GF_SWF_AS3_GOTO_FRAME;
				act.frame_number = swf_get_16(read);
				read->action(read, &act);
				break;
			case 0x83: /* get URL */
				act.type = GF_SWF_AS3_GET_URL;
				act.url = swf_get_string(read);
				act.target = swf_get_string(read);
				read->action(read, &act);
				gf_free(act.url);
				gf_free(act.target);
				break;
			/* next frame */
			case 0x04:
				DO_ACT(GF_SWF_AS3_NEXT_FRAME)
			/* previous frame */
			case 0x05:
				DO_ACT(GF_SWF_AS3_PREV_FRAME)
			/* play */
			case 0x06:
				DO_ACT(GF_SWF_AS3_PLAY)
			/* stop */
			case 0x07:
				DO_ACT(GF_SWF_AS3_STOP)
			/* toggle quality */
			case 0x08:
				DO_ACT(GF_SWF_AS3_TOGGLE_QUALITY)
			/* stop sounds*/
			case 0x09:
				DO_ACT(GF_SWF_AS3_STOP_SOUNDS)
			/* wait for frame */
			case 0x8A:
				assert (length == 3);
				act.type = GF_SWF_AS3_WAIT_FOR_FRAME;
				act.frame_number = swf_get_16(read);
				skip_actions = swf_read_int(read, 8);
				if (read->action(read, &act)) skip_actions = 0;
				break;
			/* set target */
			case 0x8B:
				act.type = GF_SWF_AS3_SET_TARGET;
				act.target = swf_get_string(read);
				read->action(read, &act);
				gf_free(act.target);
				break;
			/* goto label */
			case 0x8C:
				act.type = GF_SWF_AS3_GOTO_LABEL;
				act.target = swf_get_string(read);
				read->action(read, &act);
				gf_free(act.target);
				break;
			default:
//				swf_report(read, GF_OK, "Skipping unsupported action %x", action_code);
				if (length) swf_skip_data(read, length);
				break;
			}
		}
		action_code = swf_read_int(read, 8);
	}
#undef DO_ACT

	return GF_OK;
}

static GF_Err swf_def_button(SWFReader *read, u32 revision)
{
	SWF_Button button;
	Bool has_actions;

	memset(&button, 0, sizeof(SWF_Button));
	has_actions = 0;
	button.count = 0;
	button.ID = swf_get_16(read);
	if (revision==1) {
		gf_bs_read_int(read->bs, 7);
		gf_bs_read_int(read->bs, 1);
		has_actions = swf_get_16(read);
	}
	while (1) {
		SWF_ButtonRecord *rec = &button.buttons[button.count];
		gf_bs_read_int(read->bs, 4);
		rec->hitTest = gf_bs_read_int(read->bs, 1);
		rec->down = gf_bs_read_int(read->bs, 1);
		rec->over = gf_bs_read_int(read->bs, 1);
		rec->up = gf_bs_read_int(read->bs, 1);
		if (!rec->hitTest && !rec->up && !rec->over && !rec->down) break;
		rec->character_id = swf_get_16(read);
		rec->depth = swf_get_16(read);
		swf_get_matrix(read, &rec->mx);
		if (revision==1) {
			swf_align(read);
			swf_get_colormatrix(read, &rec->cmx);
		}
		else gf_cmx_init(&rec->cmx);
		gf_bs_align(read->bs);
		button.count++;
	}
	read->define_button(read, &button);
	if (revision==0) {
		swf_actions(read, GF_SWF_COND_OVERUP_TO_OVERDOWN, 0);
	} else {
		while (has_actions) {
			u32 i, mask, key;
			has_actions = swf_get_16(read);
			mask = 0;
			for (i=0; i<8; i++) {
				if (swf_read_int(read, 1))
					mask |= 1<<i;
			}
			key = swf_read_int(read, 7);
			if (swf_read_int(read, 1))
				mask |= GF_SWF_COND_OVERDOWN_TO_IDLE;

			swf_actions(read, mask, key);
		}
	}
	read->define_button(read, NULL);
	return GF_OK;
}

static Bool swf_mat_is_identity(GF_Matrix2D *mat)
{
	if (mat->m[0] != FIX_ONE) return 0;
	if (mat->m[4] != FIX_ONE) return 0;
	if (mat->m[1]) return 0;
	if (mat->m[2]) return 0;
	if (mat->m[3]) return 0;
	if (mat->m[5]) return 0;
	return 1;
}

static GF_Err swf_place_obj(SWFReader *read, u32 revision)
{
	GF_Err e;
	u32 shape_id;
	u32 ID, bitsize;
	u32 clip_depth;
	GF_Matrix2D mat;
	GF_ColorMatrix cmat;
	DispShape *ds;
	char *name;
	u32 depth, type;
	Bool had_depth;
	/*SWF flags*/
	Bool has_clip_actions, has_clip, has_name, has_ratio, has_cmat, has_mat, has_id, has_move;

	name = NULL;
	clip_depth = 0;
	ID = 0;
	depth = 0;
	has_cmat = has_mat = has_move = 0;

	gf_cmx_init(&cmat);
	gf_mx2d_init(mat);
	/*place*/
	type = SWF_PLACE;

	/*SWF 1.0*/
	if (revision==0) {
		ID = swf_get_16(read);
		depth = swf_get_16(read);
		bitsize = 32;
		bitsize += swf_get_matrix(read, &mat);
		has_mat = 1;
		bitsize += swf_align(read);
		/*size exceeds matrix, parse col mat*/
		if (bitsize < read->size*8) {
			swf_get_colormatrix(read, &cmat);
			has_cmat = 1;
			swf_align(read);
		}
	}
	/*SWF 3.0*/
	else if (revision==1) {
		/*reserved*/
		has_clip_actions = swf_read_int(read, 1);
		has_clip = swf_read_int(read, 1);
		has_name = swf_read_int(read, 1);
		has_ratio = swf_read_int(read, 1);
		has_cmat = swf_read_int(read, 1);
		has_mat = swf_read_int(read, 1);
		has_id = swf_read_int(read, 1);
		has_move = swf_read_int(read, 1);

		depth = swf_get_16(read);
		if (has_id) ID = swf_get_16(read);
		if (has_mat) {
			swf_get_matrix(read, &mat);
			swf_align(read);
		}
		if (has_cmat) {
			swf_align(read);
			swf_get_colormatrix(read, &cmat);
			swf_align(read);
		}
		if (has_ratio) /*ratio = */swf_get_16(read);
		if (has_clip) clip_depth = swf_get_16(read);

		if (has_name) {
			name = swf_get_string(read);
			gf_free(name);
		}
		if (has_clip_actions) {
			swf_get_16(read);
			swf_get_16(read);
		}
		/*replace*/
		if (has_id && has_move) type = SWF_REPLACE;
		/*move*/
		else if (!has_id && has_move) type = SWF_MOVE;
		/*place*/
		else type = SWF_PLACE;
	}

	if (clip_depth) {
		swf_report(read, GF_NOT_SUPPORTED, "Clipping not supported - ignoring");
		return GF_OK;
	}

	/*1: check depth of display list*/
	had_depth = read->allocate_depth(read, depth);
	/*check validity*/
	if ((type==SWF_MOVE) && !had_depth) swf_report(read, GF_BAD_PARAM, "Accessing empty depth level %d", depth);

	ds = NULL;

	/*usual case: (re)place depth level*/
	switch (type) {
	case SWF_MOVE:
		ds = swf_get_depth_entry(read, depth, 0);
		shape_id = ds ? ds->char_id : 0;
		break;
	case SWF_REPLACE:
	case SWF_PLACE:
	default:
		shape_id = ID;
		break;
	}

	if (!shape_id) {
		swf_report(read, GF_BAD_PARAM, "%s unfound object (ID %d)", (type==SWF_MOVE) ? "Moving" : ((type==SWF_PLACE) ? "Placing" : "Replacing"), ID);
		return GF_OK;
	}
	/*restore prev matrix if needed*/
	if (type==SWF_REPLACE) {
		if (!ds) ds = swf_get_depth_entry(read, depth, 0);
		if (ds) {
			if (!has_mat) {
				memcpy(&mat, &ds->mat, sizeof(GF_Matrix2D));
				has_mat = 1;
			}
			if (!has_cmat) {
				memcpy(&cmat, &ds->cmat, sizeof(GF_ColorMatrix));
				has_cmat = 1;
			}
		}
	}

	/*check for identity matrices*/
	if (has_cmat && cmat.identity) has_cmat = 0;
	if (has_mat && swf_mat_is_identity(&mat)) has_mat = 0;

	/*store in display list*/
	ds = swf_get_depth_entry(read, depth, 1);
	e = read->place_obj(read, depth, shape_id, ds->char_id, type,
	                    has_mat ? &mat : NULL,
	                    has_cmat ? &cmat : NULL,
	                    swf_mat_is_identity(&ds->mat) ? NULL : &ds->mat,
	                    ds->cmat.identity ? NULL : &ds->cmat);

	/*remember matrices*/
	memcpy(&ds->mat, &mat, sizeof(GF_Matrix2D));
	memcpy(&ds->cmat, &cmat, sizeof(GF_ColorMatrix));
	ds->char_id = shape_id;

	if (e) swf_report(read, e, "Error %s object ID %d", (type==SWF_MOVE) ? "Moving" : ((type==SWF_PLACE) ? "Placing" : "Replacing"), shape_id);
	return GF_OK;
}

static GF_Err swf_remove_obj(SWFReader *read, u32 revision)
{
	GF_Err e;
	DispShape *ds;
	u32 depth;
	if (revision==0) swf_get_16(read);
	depth = swf_get_16(read);
	ds = swf_get_depth_entry(read, depth, 0);
	/*this happens if a placeObject has failed*/
	if (!ds) return GF_OK;
	e = read->remove_obj(read, depth, ds->char_id);
	ds->char_id = 0;
	return e;
}

static GF_Err swf_show_frame(SWFReader *read)
{
	GF_Err e;
	e = read->show_frame(read);
	read->current_frame ++;
	return e;
}

static GF_Err swf_def_font(SWFReader *read, u32 revision)
{
	u32 i, count;
	GF_Err e;
	SWFFont *ft;
	u32 *offset_table = NULL;
	u32 start;

	GF_SAFEALLOC(ft, SWFFont);
	if (!ft) return GF_OUT_OF_MEM;

	ft->glyphs = gf_list_new();
	ft->fontID = swf_get_16(read);
	e = GF_OK;


	if (revision==0) {
		u32 count;

		start = swf_get_file_pos(read);

		count = swf_get_16(read);
		ft->nbGlyphs = count / 2;
		offset_table = (u32*)gf_malloc(sizeof(u32) * ft->nbGlyphs);
		offset_table[0] = 0;
		for (i=1; i<ft->nbGlyphs; i++) offset_table[i] = swf_get_16(read);

		for (i=0; i<ft->nbGlyphs; i++) {
			swf_align(read);
			e = swf_seek_file_to(read, start + offset_table[i]);
			if (e) break;
			swf_parse_shape_def(read, ft, 0);
		}
		gf_free(offset_table);
		if (e) return e;
	} else if (revision==1) {
		SWFRec rc;
		Bool wide_offset, wide_codes;
		u32 code_offset, checkpos;
		ft->has_layout = swf_read_int(read, 1);
		ft->has_shiftJIS = swf_read_int(read, 1);
		ft->is_unicode = swf_read_int(read, 1);
		ft->is_ansi = swf_read_int(read, 1);
		wide_offset = swf_read_int(read, 1);
		wide_codes = swf_read_int(read, 1);
		ft->is_italic = swf_read_int(read, 1);
		ft->is_bold = swf_read_int(read, 1);
		swf_read_int(read, 8);
		count = swf_read_int(read, 8);
		ft->fontName = (char*)gf_malloc(sizeof(u8)*count+1);
		ft->fontName[count] = 0;
		for (i=0; i<count; i++) ft->fontName[i] = swf_read_int(read, 8);

		ft->nbGlyphs = swf_get_16(read);
		start = swf_get_file_pos(read);

		if (ft->nbGlyphs) {
			offset_table = (u32*)gf_malloc(sizeof(u32) * ft->nbGlyphs);
			for (i=0; i<ft->nbGlyphs; i++) {
				if (wide_offset) offset_table[i] = swf_get_32(read);
				else offset_table[i] = swf_get_16(read);
			}
		}

		if (wide_offset) {
			code_offset = swf_get_32(read);
		} else {
			code_offset = swf_get_16(read);
		}

		if (ft->nbGlyphs) {
			for (i=0; i<ft->nbGlyphs; i++) {
				swf_align(read);
				e = swf_seek_file_to(read, start + offset_table[i]);
				if (e) break;

				swf_parse_shape_def(read, ft, 0);
			}
			gf_free(offset_table);
			if (e) return e;

			checkpos = swf_get_file_pos(read);
			if (checkpos != start + code_offset) {
				GF_LOG(GF_LOG_ERROR, GF_LOG_PARSER, ("[SWF Parsing] bad code offset in font\n"));
				return GF_NON_COMPLIANT_BITSTREAM;
			}

			ft->glyph_codes = (u16*)gf_malloc(sizeof(u16) * ft->nbGlyphs);
			for (i=0; i<ft->nbGlyphs; i++) {
				if (wide_codes) ft->glyph_codes[i] = swf_get_16(read);
				else ft->glyph_codes[i] = swf_read_int(read, 8);
			}
		}
		if (ft->has_layout) {
			ft->ascent = swf_get_s16(read);
			ft->descent = swf_get_s16(read);
			ft->leading = swf_get_s16(read);
			if (ft->nbGlyphs) {
				ft->glyph_adv = (s16*)gf_malloc(sizeof(s16) * ft->nbGlyphs);
				for (i=0; i<ft->nbGlyphs; i++) ft->glyph_adv[i] = swf_get_s16(read);
				for (i=0; i<ft->nbGlyphs; i++) swf_get_rec(read, &rc);
			}
			/*kerning info*/
			count = swf_get_16(read);
			for (i=0; i<count; i++) {
				if (wide_codes) {
					swf_get_16(read);
					swf_get_16(read);
				} else {
					swf_read_int(read, 8);
					swf_read_int(read, 8);
				}
				swf_get_s16(read);
			}
		}
	}

	gf_list_add(read->fonts, ft);
	return GF_OK;
}

static GF_Err swf_def_font_info(SWFReader *read)
{
	SWFFont *ft;
	Bool wide_chars;
	u32 i, count;

	i = swf_get_16(read);
	ft = swf_find_font(read, i);
	if (!ft) {
		swf_report(read, GF_BAD_PARAM, "Cannot locate font ID %d", i);
		return GF_BAD_PARAM;
	}
	/*overwrite font info*/
	if (ft->fontName) gf_free(ft->fontName);
	count = swf_read_int(read, 8);
	ft->fontName = (char*)gf_malloc(sizeof(char) * (count+1));
	ft->fontName[count] = 0;
	for (i=0; i<count; i++) ft->fontName[i] = swf_read_int(read, 8);
	swf_read_int(read, 2);
	ft->is_unicode = swf_read_int(read, 1);
	ft->has_shiftJIS = swf_read_int(read, 1);
	ft->is_ansi = swf_read_int(read, 1);
	ft->is_italic = swf_read_int(read, 1);
	ft->is_bold = swf_read_int(read, 1);
	/*TODO - this should be remapped to a font data stream, we currently only assume the glyph code
	table is the same as the original font file...*/
	wide_chars = swf_read_int(read, 1);
	if (ft->glyph_codes) gf_free(ft->glyph_codes);
	ft->glyph_codes = (u16*)gf_malloc(sizeof(u16) * ft->nbGlyphs);

	for (i=0; i<ft->nbGlyphs; i++) {
		if (wide_chars) ft->glyph_codes[i] = swf_get_16(read);
		else ft->glyph_codes[i] = swf_read_int(read, 8);
	}
	return GF_OK;
}

static GF_Err swf_def_text(SWFReader *read, u32 revision)
{
	SWFRec rc;
	SWFText txt;
	Bool flag;
	u32 nbits_adv, nbits_glyph, i, col, fontID, count, font_height;
	Fixed offX, offY;
	GF_Err e;

	txt.ID = swf_get_16(read);
	swf_get_rec(read, &rc);
	swf_get_matrix(read, &txt.mat);
	txt.text = gf_list_new();

	swf_align(read);
	nbits_glyph = swf_read_int(read, 8);
	nbits_adv = swf_read_int(read, 8);
	fontID = 0;
	offX = offY = 0;
	font_height = 0;
	col = 0xFF000000;
	e = GF_OK;

	while (1) {
		flag = swf_read_int(read, 1);
		/*regular glyph record*/
		if (!flag) {
			SWFGlyphRec *gr;
			count = swf_read_int(read, 7);
			if (!count) break;

			if (!fontID) {
				e = GF_BAD_PARAM;
				swf_report(read, GF_BAD_PARAM, "Defining text %d without assigning font", fontID);
				break;
			}

			GF_SAFEALLOC(gr, SWFGlyphRec);
			if (!gr) return GF_OUT_OF_MEM;

			gf_list_add(txt.text, gr);
			gr->fontID = fontID;
			gr->fontSize = font_height;
			gr->col = col;
			gr->orig_x = offX;
			gr->orig_y = offY;
			gr->nbGlyphs = count;
			gr->indexes = (u32*)gf_malloc(sizeof(u32) * gr->nbGlyphs);
			gr->dx = (Fixed*)gf_malloc(sizeof(Fixed) * gr->nbGlyphs);
			for (i=0; i<gr->nbGlyphs; i++) {
				gr->indexes[i] = swf_read_int(read, nbits_glyph);
				gr->dx[i] = FLT2FIX( swf_read_int(read, nbits_adv) * SWF_TWIP_SCALE );
			}
			swf_align(read);
		}
		/*text state change*/
		else {
			Bool has_font, has_col, has_y_off, has_x_off;
			/*reserved*/
			swf_read_int(read, 3);
			has_font = swf_read_int(read, 1);
			has_col = swf_read_int(read, 1);
			has_y_off = swf_read_int(read, 1);
			has_x_off = swf_read_int(read, 1);

			/*end of rec*/
			if (!has_font && !has_col && !has_y_off && !has_x_off) break;
			if (has_font) fontID = swf_get_16(read);
			if (has_col) {
				if (revision==0) col = swf_get_color(read);
				else col = swf_get_argb(read);
			}
			/*openSWF spec seems to have wrong order here*/
			if (has_x_off) offX = FLT2FIX( swf_get_s16(read) * SWF_TWIP_SCALE );
			if (has_y_off) offY = FLT2FIX( swf_get_s16(read) * SWF_TWIP_SCALE );
			if (has_font) font_height = swf_get_16(read);
		}
	}

	if (e) goto exit;

	if (! (read->flags & GF_SM_SWF_NO_TEXT) ) {
		e = read->define_text(read, &txt);
	}

exit:
	while (gf_list_count(txt.text)) {
		SWFGlyphRec *gr = (SWFGlyphRec *)gf_list_get(txt.text, 0);
		gf_list_rem(txt.text, 0);
		if (gr->indexes) gf_free(gr->indexes);
		if (gr->dx) gf_free(gr->dx);
		gf_free(gr);
	}
	gf_list_del(txt.text);

	return e;
}


static GF_Err swf_def_edit_text(SWFReader *read)
{
	GF_Err e;
	SWFEditText txt;
	char *var_name;
	Bool has_text, has_text_color, has_max_length, has_font;

	memset(&txt, 0, sizeof(SWFEditText));
	txt.color = 0xFF000000;

	txt.ID = swf_get_16(read);
	swf_get_rec(read, &txt.bounds);
	swf_align(read);

	has_text = swf_read_int(read, 1);
	txt.word_wrap = swf_read_int(read, 1);
	txt.multiline = swf_read_int(read, 1);
	txt.password = swf_read_int(read, 1);
	txt.read_only = swf_read_int(read, 1);
	has_text_color = swf_read_int(read, 1);
	has_max_length = swf_read_int(read, 1);
	has_font = swf_read_int(read, 1);
	/*reserved*/swf_read_int(read, 1);
	txt.auto_size = swf_read_int(read, 1);
	txt.has_layout = swf_read_int(read, 1);
	txt.no_select = swf_read_int(read, 1);
	txt.border = swf_read_int(read, 1);
	/*reserved*/swf_read_int(read, 1);
	txt.html = swf_read_int(read, 1);
	txt.outlines = swf_read_int(read, 1);

	if (has_font) {
		txt.fontID = swf_get_16(read);
		txt.font_height = FLT2FIX( swf_get_16(read) * SWF_TWIP_SCALE );
	}
	if (has_text_color) txt.color = swf_get_argb(read);
	if (has_max_length) txt.max_length = FLT2FIX( swf_get_16(read) * SWF_TWIP_SCALE );

	if (txt.has_layout) {
		txt.align = swf_read_int(read, 8);
		txt.left = FLT2FIX( swf_get_16(read) * SWF_TWIP_SCALE );
		txt.right = FLT2FIX( swf_get_16(read) * SWF_TWIP_SCALE );
		txt.indent = FLT2FIX( swf_get_16(read) * SWF_TWIP_SCALE );
		txt.leading = FLT2FIX( swf_get_16(read) * SWF_TWIP_SCALE );
	}
	var_name = swf_get_string(read);
	if (has_text) txt.init_value = swf_get_string(read);

	e = GF_OK;
	if (! (read->flags & GF_SM_SWF_NO_TEXT) ) {
		e = read->define_edit_text(read, &txt);
	}
	gf_free(var_name);
	if (txt.init_value) gf_free(txt.init_value);

	return e;
}

static void swf_delete_sound_stream(SWFReader *read)
{
	if (!read->sound_stream) return;
	if (read->sound_stream->output) gf_fclose(read->sound_stream->output);
	if (read->sound_stream->szFileName) gf_free(read->sound_stream->szFileName);
	gf_free(read->sound_stream);
	read->sound_stream = NULL;
}

static GF_Err swf_def_sprite(SWFReader *read)
{
	GF_Err e;
	GF_List *prev_dlist;
	u32 frame_count;
	Bool prev_sprite;
	u32 prev_frame, prev_depth;
	SWFSound *snd;

	prev_sprite = read->current_sprite_id;
	read->current_sprite_id = swf_get_16(read);
	frame_count = swf_get_16(read);

	/*store frame state*/
	prev_frame = read->current_frame;
	read->current_frame = 0;
	/*store soundStream state*/
	snd = read->sound_stream;
	read->sound_stream = NULL;
	/*store depth state*/
	prev_depth = read->max_depth;
	read->max_depth = 0;

	prev_dlist = read->display_list;
	read->display_list = gf_list_new();

	e = read->define_sprite(read, frame_count);
	if (e) return e;

	/*close sprite soundStream*/
	swf_delete_sound_stream(read);
	/*restore sound stream*/
	read->sound_stream = snd;
	read->max_depth = prev_depth;

	while (gf_list_count(read->display_list)) {
		DispShape *s = (DispShape *)gf_list_get(read->display_list, 0);
		gf_list_rem(read->display_list, 0);
		gf_free(s);
	}
	gf_list_del(read->display_list);
	read->display_list = prev_dlist;

	read->current_frame = prev_frame;
	read->current_sprite_id = prev_sprite;

	read->tag = SWF_DEFINESPRITE;
	return GF_OK;
}

static GF_Err swf_def_sound(SWFReader *read)
{
	SWFSound *snd;
	GF_SAFEALLOC(snd , SWFSound);
	if (!snd) return GF_OUT_OF_MEM;
	snd->ID = swf_get_16(read);
	snd->format = swf_read_int(read, 4);
	snd->sound_rate = swf_read_int(read, 2);
	snd->bits_per_sample = swf_read_int(read, 1) ? 16 : 8;
	snd->stereo = swf_read_int(read, 1);
	snd->sample_count = swf_get_32(read);

	switch (snd->format) {
	/*raw PCM*/
	case 0:
		swf_report(read, GF_NOT_SUPPORTED, "Raw PCM Audio not supported");
		gf_free(snd);
		break;
	/*ADPCM*/
	case 1:
		swf_report(read, GF_NOT_SUPPORTED, "AD-PCM Audio not supported");
		gf_free(snd);
		break;
	/*MP3*/
	case 2:
	{
		char szName[1024];
		u32 alloc_size, tot_size;
		char *frame;

		sprintf(szName, "swf_sound_%d.mp3", snd->ID);
		if (read->localPath) {
			snd->szFileName = (char*)gf_malloc(sizeof(char)*GF_MAX_PATH);
			strcpy(snd->szFileName, read->localPath);
			strcat(snd->szFileName, szName);
		} else {
			snd->szFileName = gf_strdup(szName);
		}
		snd->output = gf_fopen(snd->szFileName, "wb");

		alloc_size = 4096;
		frame = (char*)gf_malloc(sizeof(char)*4096);
		snd->frame_delay_ms = swf_get_16(read);
		snd->frame_delay_ms = read->current_frame*1000;
		snd->frame_delay_ms /= read->frame_rate;
		tot_size = 9;
		/*parse all frames*/
		while (tot_size<read->size) {
			u32 toread = read->size - tot_size;
			if (toread>alloc_size) toread = alloc_size;
			swf_read_data(read, frame, toread);
			gf_fwrite(frame, sizeof(char)*toread, 1, snd->output);
			tot_size += toread;
		}

		gf_free(frame);
		return gf_list_add(read->sounds, snd);
	}
	case 3:
		swf_report(read, GF_NOT_SUPPORTED, "Unrecognized sound format");
		gf_free(snd);
		break;
	}
	return GF_OK;
}


typedef struct
{
	u32 sync_flags;
	u32 in_point, out_point;
	u32 nb_loops;
} SoundInfo;

static SoundInfo swf_skip_soundinfo(SWFReader *read)
{
	SoundInfo si;
	u32 sync_flags = swf_read_int(read, 4);
	Bool has_env = swf_read_int(read, 1);
	Bool has_loops = swf_read_int(read, 1);
	Bool has_out_pt = swf_read_int(read, 1);
	Bool has_in_pt = swf_read_int(read, 1);

	memset(&si, 0, sizeof(SoundInfo));
	si.sync_flags = sync_flags;
	if (has_in_pt) si.in_point = swf_get_32(read);
	if (has_out_pt) si.out_point = swf_get_32(read);
	if (has_loops) si.nb_loops = swf_get_16(read);
	/*we ignore the envelope*/
	if (has_env) {
		u32 i;
		u32 nb_ctrl = swf_read_int(read, 8);
		for (i=0; i<nb_ctrl; i++) {
			swf_read_int(read, 32);	/*mark44*/
			swf_read_int(read, 16);	/*l0*/
			swf_read_int(read, 16);	/*l1*/
		}
	}
	return si;
}

static SWFSound *sndswf_get_sound(SWFReader *read, u32 ID)
{
	u32 i;
	SWFSound *snd;
	i=0;
	while ((snd = (SWFSound *)gf_list_enum(read->sounds, &i))) {
		if (snd->ID==ID) return snd;
	}
	return NULL;
}

static GF_Err swf_start_sound(SWFReader *read)
{
	SWFSound *snd;
	u32 ID = swf_get_16(read);
	SoundInfo si;
	si = swf_skip_soundinfo(read);

	snd = sndswf_get_sound(read, ID);
	if (!snd) {
		swf_report(read, GF_BAD_PARAM, "Cannot find sound with ID %d", ID);
		return GF_OK;
	}
	if (!snd->is_setup) {
		GF_Err e = read->setup_sound(read, snd, 0);
		if (e) return e;
		snd->is_setup = 1;
	}
	return read->start_sound(read, snd, (si.sync_flags & 0x2) ? 1 : 0);
}

static GF_Err swf_soundstream_hdr(SWFReader *read)
{
	char szName[1024];
	SWFSound *snd;

	if (read->sound_stream) {
		swf_report(read, GF_BAD_PARAM, "More than one sound stream for current timeline!!");
		return swf_func_skip(read);
	}

	GF_SAFEALLOC(snd, SWFSound);
	if (!snd) return GF_OUT_OF_MEM;

	/*rec_mix = */swf_read_int(read, 8);
	/*0: uncompressed, 1: ADPCM, 2: MP3*/
	snd->format = swf_read_int(read, 4);
	/*0: 5.5k, 1: 11k, 2: 2: 22k, 3: 44k*/
	snd->sound_rate = swf_read_int(read, 2);
	/*0: 8 bit, 1: 16 bit*/
	snd->bits_per_sample = swf_read_int(read, 1) ? 16 : 8;
	/*0: mono, 8 1: stereo*/
	snd->stereo = swf_read_int(read, 1);
	/*samplesperframe hint*/
	swf_read_int(read, 16);

	switch (snd->format) {
	/*raw PCM*/
	case 0:
		swf_report(read, GF_NOT_SUPPORTED, "Raw PCM Audio not supported");
		gf_free(snd);
		break;
	/*ADPCM*/
	case 1:
		swf_report(read, GF_NOT_SUPPORTED, "AD-PCM Audio not supported");
		gf_free(snd);
		break;
	/*MP3*/
	case 2:
		read->sound_stream = snd;
		if (read->localPath) {
			sprintf(szName, "%s/swf_soundstream_%d.mp3", read->localPath, read->current_sprite_id);
		} else {
			sprintf(szName, "swf_soundstream_%d.mp3", read->current_sprite_id);
		}
		read->sound_stream->szFileName = gf_strdup(szName);
		read->setup_sound(read, read->sound_stream, 0);
		break;
	case 3:
		swf_report(read, GF_NOT_SUPPORTED, "Unrecognized sound format");
		gf_free(snd);
		break;
	}
	return GF_OK;
}

static GF_Err swf_soundstream_block(SWFReader *read)
{
#ifdef GPAC_DISABLE_AV_PARSERS
	return swf_func_skip(read);
#else
	unsigned char bytes[4];
	u32 hdr, alloc_size, size, tot_size, samplesPerFrame;
	char *frame;

	/*note we're doing only MP3*/
	if (!read->sound_stream) return swf_func_skip(read);

	samplesPerFrame = swf_get_16(read);
	/*delay = */swf_get_16(read);

	if (!read->sound_stream->is_setup) {

		/*error at setup*/
		if (!read->sound_stream->output) {
			read->sound_stream->output = gf_fopen(read->sound_stream->szFileName, "wb");
			if (!read->sound_stream->output)
				return swf_func_skip(read);
		}
		/*store TS of first AU*/
		read->sound_stream->frame_delay_ms = read->current_frame*1000;
		read->sound_stream->frame_delay_ms /= read->frame_rate;
		read->setup_sound(read, read->sound_stream, 1);
		read->sound_stream->is_setup = 1;
	}

	if (!samplesPerFrame) return GF_OK;

	alloc_size = 1;
	frame = (char*)gf_malloc(sizeof(char));
	tot_size = 4;
	/*parse all frames*/
	while (1) {
		bytes[0] = swf_read_int(read, 8);
		bytes[1] = swf_read_int(read, 8);
		bytes[2] = swf_read_int(read, 8);
		bytes[3] = swf_read_int(read, 8);
		hdr = GF_4CC(bytes[0], bytes[1], bytes[2], bytes[3]);
		size = gf_mp3_frame_size(hdr);
		if (alloc_size<size-4) {
			frame = (char*)gf_realloc(frame, sizeof(char)*(size-4));
			alloc_size = size-4;
		}
		/*watchout for truncated framesif */
		if (tot_size + size >= read->size) size = read->size - tot_size;

		swf_read_data(read, frame, size-4);
		gf_fwrite(bytes, sizeof(char)*4, 1, read->sound_stream->output);
		gf_fwrite(frame, sizeof(char)*(size-4), 1, read->sound_stream->output);
		if (tot_size + size >= read->size) break;
		tot_size += size;
	}
	gf_free(frame);
	return GF_OK;
#endif
}

static GF_Err swf_def_hdr_jpeg(SWFReader *read)
{
	if (!read) return GF_OK;
	if (read->jpeg_hdr) {
		swf_report(read, GF_NON_COMPLIANT_BITSTREAM, "JPEG Table already defined in file");
		return GF_NON_COMPLIANT_BITSTREAM;
	}
	read->jpeg_hdr_size = read->size;
	if (read->size) {
		read->jpeg_hdr = gf_malloc(sizeof(char)*read->size);
		swf_read_data(read, (char *) read->jpeg_hdr, read->size);
	}
	return GF_OK;
}


static GF_Err swf_def_bits_jpeg(SWFReader *read, u32 version)
{
	u32 ID;
	FILE *file = NULL;
	char szName[1024];
	u8 *buf;
	u32 skip = 0;
	u32 AlphaPlaneSize = 0;
	u32 size = read->size;

	ID = swf_get_16(read);
	size -= 2;
	if (version==3) {
		u32 offset =  swf_get_32(read);
		size -= 4;
		AlphaPlaneSize = size - offset;
		size = offset;
	}

	/*dump file*/
	if (read->localPath) {
		sprintf(szName, "%s/swf_jpeg_%d.jpg", read->localPath, ID);
	} else {
		sprintf(szName, "swf_jpeg_%d.jpg", ID);
	}

	if (version!=3)
		file = gf_fopen(szName, "wb");

	if (version==1 && read->jpeg_hdr_size) {
		/*remove JPEG EOI*/
		gf_fwrite(read->jpeg_hdr, 1, read->jpeg_hdr_size-2, file);
		/*remove JPEG SOI*/
		swf_get_16(read);
		size-=2;
	}
	buf = gf_malloc(sizeof(u8)*size);
	swf_read_data(read, (char *) buf, size);
	if (version==1) {
		gf_fwrite(buf, 1, size, file);
	} else {
		u32 i;
		for (i=0; i<size; i++) {
			if ((i+4<size)
			        && (buf[i]==0xFF) && (buf[i+1]==0xD9)
			        && (buf[i+2]==0xFF) && (buf[i+3]==0xD8)
			   ) {
				memmove(buf+i, buf+i+4, sizeof(char)*(size-i-4));
				size -= 4;
				break;
			}
		}
		if ((buf[0]==0xFF) && (buf[1]==0xD8) && (buf[2]==0xFF) && (buf[3]==0xD8)) {
			skip = 2;
		}
		if (version==2)
			gf_fwrite(buf+skip, 1, size-skip, file);
	}
	if (version!=3)
		gf_fclose(file);

	if (version==3) {
#ifndef GPAC_DISABLE_AV_PARSERS
		char *dst, *raw;
		GF_Err e;
		u32 codecid;
		u32 osize, w, h, j, pf;
		GF_BitStream *bs;

		/*decompress jpeg*/
		bs = gf_bs_new( (char *) buf+skip, size-skip, GF_BITSTREAM_READ);
		gf_img_parse(bs, &codecid, &w, &h, NULL, NULL);
		gf_bs_del(bs);

		osize = w*h*4;
		raw = gf_malloc(sizeof(char)*osize);
		memset(raw, 0, sizeof(char)*osize);
		e = gf_img_jpeg_dec(buf+skip, size-skip, &w, &h, &pf, raw, &osize, 4);
		if (e != GF_OK) {
			swf_report(read, e, "Cannopt decode JPEG image");
		}

		/*read alpha map and decompress it*/
		if (size<AlphaPlaneSize) buf = gf_realloc(buf, sizeof(u8)*AlphaPlaneSize);
		swf_read_data(read, (char *) buf, AlphaPlaneSize);

		osize = w*h;
		dst = gf_malloc(sizeof(char)*osize);
		uncompress((Bytef *) dst, (uLongf *) &osize, buf, AlphaPlaneSize);
		/*write alpha channel*/
		for (j=0; j<osize; j++) {
			raw[4*j + 3] = dst[j];
		}
		gf_free(dst);

		/*write png*/
		if (read->localPath) {
			sprintf(szName, "%s/swf_png_%d.png", read->localPath, ID);
		} else {
			sprintf(szName, "swf_png_%d.png", ID);
		}

		osize = w*h*4;
		buf = gf_realloc(buf, sizeof(char)*osize);
		gf_img_png_enc(raw, w, h, h*4, GF_PIXEL_RGBA, (char *)buf, &osize);

		file = gf_fopen(szName, "wb");
		gf_fwrite(buf, 1, osize, file);
		gf_fclose(file);

		gf_free(raw);
#endif //GPAC_DISABLE_AV_PARSERS
	}
	gf_free(buf);

	return read->setup_image(read, ID, szName);
}


static const char *swf_get_tag_name(u32 tag)
{
	switch (tag) {
	case SWF_END:
		return "End";
	case SWF_SHOWFRAME:
		return "ShowFrame";
	case SWF_DEFINESHAPE:
		return "DefineShape";
	case SWF_FREECHARACTER:
		return "FreeCharacter";
	case SWF_PLACEOBJECT:
		return "PlaceObject";
	case SWF_REMOVEOBJECT:
		return "RemoveObject";
	case SWF_DEFINEBITSJPEG:
		return "DefineBitsJPEG";
	case SWF_DEFINEBUTTON:
		return "DefineButton";
	case SWF_JPEGTABLES:
		return "JPEGTables";
	case SWF_SETBACKGROUNDCOLOR:
		return "SetBackgroundColor";
	case SWF_DEFINEFONT:
		return "DefineFont";
	case SWF_DEFINETEXT:
		return "DefineText";
	case SWF_DOACTION:
		return "DoAction";
	case SWF_DEFINEFONTINFO:
		return "DefineFontInfo";
	case SWF_DEFINESOUND:
		return "DefineSound";
	case SWF_STARTSOUND:
		return "StartSound";
	case SWF_DEFINEBUTTONSOUND:
		return "DefineButtonSound";
	case SWF_SOUNDSTREAMHEAD:
		return "SoundStreamHead";
	case SWF_SOUNDSTREAMBLOCK:
		return "SoundStreamBlock";
	case SWF_DEFINEBITSLOSSLESS:
		return "DefineBitsLossless";
	case SWF_DEFINEBITSJPEG2:
		return "DefineBitsJPEG2";
	case SWF_DEFINESHAPE2:
		return "DefineShape2";
	case SWF_DEFINEBUTTONCXFORM:
		return "DefineButtonCXForm";
	case SWF_PROTECT:
		return "Protect";
	case SWF_PLACEOBJECT2:
		return "PlaceObject2";
	case SWF_REMOVEOBJECT2:
		return "RemoveObject2";
	case SWF_DEFINESHAPE3:
		return "DefineShape3";
	case SWF_DEFINETEXT2:
		return "DefineText2";
	case SWF_DEFINEBUTTON2:
		return "DefineButton2";
	case SWF_DEFINEBITSJPEG3:
		return "DefineBitsJPEG3";
	case SWF_DEFINEBITSLOSSLESS2:
		return "DefineBitsLossless2";
	case SWF_DEFINEEDITTEXT:
		return "DefineEditText";
	case SWF_DEFINEMOVIE:
		return "DefineMovie";
	case SWF_DEFINESPRITE:
		return "DefineSprite";
	case SWF_NAMECHARACTER:
		return "NameCharacter";
	case SWF_SERIALNUMBER:
		return "SerialNumber";
	case SWF_GENERATORTEXT:
		return "GeneratorText";
	case SWF_FRAMELABEL:
		return "FrameLabel";
	case SWF_SOUNDSTREAMHEAD2:
		return "SoundStreamHead2";
	case SWF_DEFINEMORPHSHAPE:
		return "DefineMorphShape";
	case SWF_DEFINEFONT2:
		return "DefineFont2";
	case SWF_TEMPLATECOMMAND:
		return "TemplateCommand";
	case SWF_GENERATOR3:
		return "Generator3";
	case SWF_EXTERNALFONT:
		return "ExternalFont";
	case SWF_EXPORTASSETS:
		return "ExportAssets";
	case SWF_IMPORTASSETS:
		return "ImportAssets";
	case SWF_ENABLEDEBUGGER:
		return "EnableDebugger";
	case SWF_MX0:
		return "MX0";
	case SWF_MX1:
		return "MX1";
	case SWF_MX2:
		return "MX2";
	case SWF_MX3:
		return "MX3";
	case SWF_MX4:
		return "MX4";
	default:
		return "UnknownTag";
	}
}

static GF_Err swf_unknown_tag(SWFReader *read)
{
	if (!read) return GF_OK;
	swf_report(read, GF_NOT_SUPPORTED, "Tag %s (0x%2x) not implemented - skipping", swf_get_tag_name(read->tag), read->tag);
	return swf_func_skip(read);
}

static GF_Err swf_process_tag(SWFReader *read)
{
	switch (read->tag) {
	case SWF_END:
		return GF_OK;
	case SWF_PROTECT:
		return GF_OK;
	case SWF_SETBACKGROUNDCOLOR:
		return swf_set_backcol(read);
	case SWF_DEFINESHAPE:
		return swf_parse_shape_def(read, NULL, 0);
	case SWF_DEFINESHAPE2:
		return swf_parse_shape_def(read, NULL, 1);
	case SWF_DEFINESHAPE3:
		return swf_parse_shape_def(read, NULL, 2);
	case SWF_PLACEOBJECT:
		return swf_place_obj(read, 0);
	case SWF_PLACEOBJECT2:
		return swf_place_obj(read, 1);
	case SWF_REMOVEOBJECT:
		return swf_remove_obj(read, 0);
	case SWF_REMOVEOBJECT2:
		return swf_remove_obj(read, 1);
	case SWF_SHOWFRAME:
		return swf_show_frame(read);
	case SWF_DEFINEFONT:
		return swf_def_font(read, 0);
	case SWF_DEFINEFONT2:
		return swf_def_font(read, 1);
	case SWF_DEFINEFONTINFO:
		return swf_def_font_info(read);
	case SWF_DEFINETEXT:
		return swf_def_text(read, 0);
	case SWF_DEFINETEXT2:
		return swf_def_text(read, 1);
	case SWF_DEFINEEDITTEXT:
		return swf_def_edit_text(read);
	case SWF_DEFINESPRITE:
		return swf_def_sprite(read);
	/*no revision needed*/
	case SWF_SOUNDSTREAMHEAD:
	case SWF_SOUNDSTREAMHEAD2:
		return swf_soundstream_hdr(read);
	case SWF_DEFINESOUND:
		return swf_def_sound(read);
	case SWF_STARTSOUND:
		return swf_start_sound(read);
	case SWF_SOUNDSTREAMBLOCK:
		return swf_soundstream_block(read);

	case SWF_DEFINEBUTTON:
		return swf_def_button(read, 0);
	case SWF_DEFINEBUTTON2:
		return swf_def_button(read, 1);
//	case SWF_DEFINEBUTTONSOUND:
	case SWF_DOACTION:
		return swf_actions(read, 0, 0);
	case SWF_FRAMELABEL:
	{
		char *framelabel = swf_get_string(read);
		gf_free(framelabel);
		return GF_OK;
	}

	case SWF_JPEGTABLES:
		return swf_def_hdr_jpeg(read);
	case SWF_DEFINEBITSJPEG:
		return swf_def_bits_jpeg(read, 1);
	case SWF_DEFINEBITSJPEG2:
		return swf_def_bits_jpeg(read, 2);
	case SWF_DEFINEBITSJPEG3:
		return swf_def_bits_jpeg(read, 3);

	default:
		return swf_unknown_tag(read);
	}
}

GF_Err swf_parse_tag(SWFReader *read)
{
	GF_Err e;
	s32 diff;
	u16 hdr;
	u32 pos;


	hdr = swf_get_16(read);
	read->tag = hdr>>6;
	read->size = hdr & 0x3f;
	if (read->size == 0x3f) {
		swf_align(read);
		read->size = swf_get_32(read);
	}
	pos = swf_get_file_pos(read);
	diff = pos + read->size;
	gf_set_progress("SWF Parsing", pos, read->length);

	e = swf_process_tag(read);
	swf_align(read);

	diff -= swf_get_file_pos(read);
	if (diff<0) {
		swf_report(read, GF_IO_ERR, "tag %s over-read of %d bytes (size %d)", swf_get_tag_name(read->tag), -1*diff, read->size);
		return GF_IO_ERR;
	} else {
		swf_read_int(read, diff*8);
	}


	if (!e && !read->tag) {
		return GF_EOS;
	}

	if (read->ioerr) {
		swf_report(read, GF_IO_ERR, "bitstream IO err (tag size %d)", read->size);
		return read->ioerr;
	}
	return e;
}



GF_Err swf_parse_sprite(SWFReader *read)
{
	/*parse*/
	while (1) {
		GF_Err e = swf_parse_tag(read);
		if (e<0) {
			swf_report(read, e, "Error parsing tag %s", swf_get_tag_name(read->tag));
			return e;
		}
		/*done with sprite*/
		if (read->tag==SWF_END) break;
	}
	return GF_OK;
}


void swf_report(SWFReader *read, GF_Err e, char *format, ...)
{
#ifndef GPAC_DISABLE_LOG
	if (gf_log_tool_level_on(GF_LOG_PARSER, e ? GF_LOG_ERROR : GF_LOG_WARNING)) {
		char szMsg[2048];
		va_list args;
		va_start(args, format);
		vsnprintf(szMsg, 2048, format, args);
		va_end(args);
		GF_LOG((u32) (e ? GF_LOG_ERROR : GF_LOG_WARNING), GF_LOG_PARSER, ("[SWF Parsing] %s (frame %d)\n", szMsg, read->current_frame+1) );
	}
#endif
}


static void swf_io_error(void *par)
{
	SWFReader *read = (SWFReader *)par;
	if (read) read->ioerr = GF_IO_ERR;
}

GF_Err gf_sm_load_run_swf(GF_SceneLoader *load)
{
	GF_Err e;
	SWFReader *read = (SWFReader *)load->loader_priv;
	if (!read) return GF_BAD_PARAM;

	/*parse all tags*/
	while (1) {
		e = swf_parse_tag(read);
		if (e) break;
	}
	gf_set_progress("SWF Parsing", read->length, read->length);

	if (e==GF_EOS) {
		if (read->finalize)
			read->finalize(read);
		e = GF_OK;
	}
	if (!e) {
		if (read->flat_limit != 0)
			swf_report(read, GF_OK, "%d points removed while parsing shapes (Flattening limit %.4f)", read->flatten_points, read->flat_limit);

		if (read->no_as && read->has_interact) swf_report(read, GF_OK, "ActionScripts and interactions have been removed");
	} else
		swf_report(read, e, "Error parsing tag %s", swf_get_tag_name(read->tag));


	return e;
}

void gf_swf_reader_del(SWFReader *read)
{
	if (!read) return;
	gf_bs_del(read->bs);
	if (read->mem) gf_free(read->mem);

	while (gf_list_count(read->display_list)) {
		DispShape *s = (DispShape *)gf_list_get(read->display_list, 0);
		gf_list_rem(read->display_list, 0);
		gf_free(s);
	}
	gf_list_del(read->display_list);
	while (gf_list_count(read->fonts)) {
		SWFFont *ft = (SWFFont *)gf_list_get(read->fonts, 0);
		gf_list_rem(read->fonts, 0);
		if (ft->glyph_adv) gf_free(ft->glyph_adv);
		if (ft->glyph_codes) gf_free(ft->glyph_codes);
		if (ft->fontName) gf_free(ft->fontName);
		gf_list_del(ft->glyphs);
		gf_free(ft);
	}
	gf_list_del(read->fonts);
	gf_list_del(read->apps);

	while (gf_list_count(read->sounds)) {
		SWFSound *snd = (SWFSound *)gf_list_get(read->sounds, 0);
		gf_list_rem(read->sounds, 0);
		if (snd->output) gf_fclose(snd->output);
		if (snd->szFileName) gf_free(snd->szFileName);
		gf_free(snd);
	}
	gf_list_del(read->sounds);
	swf_delete_sound_stream(read);

	if (read->jpeg_hdr) gf_free(read->jpeg_hdr);
	if (read->localPath) gf_free(read->localPath);
	gf_fclose(read->input);
	gf_free(read->inputName);
	gf_free(read);
}

void gf_sm_load_done_swf(GF_SceneLoader *load)
{
	SWFReader *read = (SWFReader *) load->loader_priv;
	if (!read) return;
	if (read->svg_file) {
		gf_fclose(read->svg_file);
		read->svg_file = NULL;
	}
	gf_swf_reader_del(read);
	load->loader_priv = NULL;
}

SWFReader *gf_swf_reader_new(const char *localPath, const char *inputName)
{
	SWFReader *read;
	FILE *input;
	input = gf_fopen(inputName, "rb");
	if (!input) return NULL;

	GF_SAFEALLOC(read, SWFReader);
	if (!read) return NULL;
	read->inputName = gf_strdup(inputName);
	read->input = input;
	read->bs = gf_bs_from_file(input, GF_BITSTREAM_READ);
	gf_bs_set_eos_callback(read->bs, swf_io_error, &read);
	read->display_list = gf_list_new();
	read->fonts = gf_list_new();
	read->apps = gf_list_new();
	read->sounds = gf_list_new();

	if (localPath) {
		read->localPath = gf_strdup(localPath);
	} else {
		char *c;
		read->localPath = gf_strdup(inputName);
		c = strrchr(read->localPath, GF_PATH_SEPARATOR);
		if (c) c[1] = 0;
		else {
			gf_free(read->localPath);
			read->localPath = NULL;
		}
	}

	return read;
}

GF_Err gf_swf_reader_set_user_mode(SWFReader *read, void *user,
                                   GF_Err (*add_sample)(void *user, const u8 *data, u32 length, u64 timestamp, Bool isRap),
                                   GF_Err (*add_header)(void *user, const u8 *data, u32 length, Bool isHeader))
{
	if (!read) return GF_BAD_PARAM;
	read->user = user;
	read->add_header = add_header;
	read->add_sample = add_sample;
	return GF_OK;
}

GF_Err gf_swf_read_header(SWFReader *read)
{
	SWFRec rc;
	u8 sig[3];

	/*get signature*/
	sig[0] = gf_bs_read_u8(read->bs);
	sig[1] = gf_bs_read_u8(read->bs);
	sig[2] = gf_bs_read_u8(read->bs);
	/*"FWS" or "CWS"*/
	if ( ((sig[0] != 'F') && (sig[0] != 'C')) || (sig[1] != 'W') || (sig[2] != 'S') ) {
		return GF_URL_ERROR;
	}
	/*version = */gf_bs_read_u8(read->bs);
	read->length = swf_get_32(read);

	/*if compressed decompress the whole file*/
	if (sig[0] == 'C') {
		swf_init_decompress(read);
	}

	swf_get_rec(read, &rc);
	read->width = rc.w;
	read->height = rc.h;

	swf_align(read);
	read->frame_rate = swf_get_16(read)>>8;
	read->frame_count = swf_get_16(read);
	GF_LOG(GF_LOG_INFO, GF_LOG_PARSER, ("SWF Import - Scene Size %gx%g - %d frames @ %d FPS\n", read->width, read->height, read->frame_count, read->frame_rate));
	return GF_OK;
}

GF_Err gf_swf_get_duration(SWFReader *read, u32 *frame_rate, u32 *frame_count)
{
	*frame_rate = read->frame_rate;
	*frame_count = read->frame_count;
	return GF_OK;
}

GF_Err gf_sm_load_init_swf(GF_SceneLoader *load)
{
	SWFReader *read;
	GF_Err e;

	if (!load->ctx || !load->scene_graph || !load->fileName) return GF_BAD_PARAM;

#ifdef GPAC_ENABLE_COVERAGE
	if (gf_sys_is_test_mode()) {
		swf_func_skip(NULL);
		swf_def_hdr_jpeg(NULL);
		swf_get_tag_name(SWF_FREECHARACTER);
		swf_unknown_tag(NULL);
		swf_io_error(NULL);
	}
#endif

	read = gf_swf_reader_new(load->localPath, load->fileName);
	read->load = load;
	read->flags = load->swf_import_flags;
	read->flat_limit = FLT2FIX(load->swf_flatten_limit);
	load->loader_priv = read;

	gf_swf_read_header(read);
	load->ctx->scene_width = FIX2INT(read->width);
	load->ctx->scene_height = FIX2INT(read->height);
	load->ctx->is_pixel_metrics = 1;

	if (!(load->swf_import_flags & GF_SM_SWF_SPLIT_TIMELINE) ) {
		swf_report(read, GF_OK, "ActionScript disabled");
		read->no_as = 1;
	}

	if (!(load->swf_import_flags & GF_SM_SWF_USE_SVG)) {
#ifndef GPAC_DISABLE_VRML
		e = swf_to_bifs_init(read);
#endif
	} else {
#ifndef GPAC_DISABLE_SVG
		char svgFileName[GF_MAX_PATH];
		FILE *svgFile;
		if (load->svgOutFile) {
			if (load->localPath) {
				sprintf(svgFileName, "%s%c%s.svg", load->localPath, GF_PATH_SEPARATOR, load->svgOutFile);
			} else {
				sprintf(svgFileName, "%s.svg", load->svgOutFile);
			}
			svgFile = gf_fopen(svgFileName, "wt");
			if (!svgFile) return GF_BAD_PARAM;
			read->svg_file = svgFile;
		} else {
			svgFile = stdout;
		}
		gf_swf_reader_set_user_mode(read, svgFile, swf_svg_write_text_sample, swf_svg_write_text_header);
		e = swf_to_svg_init(read, read->flags, load->swf_flatten_limit);
#endif
	}
	if (e) goto exit;

	/*parse all tags*/
	while (e == GF_OK) {
		e = swf_parse_tag(read);
		if (read->current_frame==1) break;
	}
	if (e==GF_EOS) e = GF_OK;

	load->done = gf_sm_load_done_swf;
	load->process = gf_sm_load_run_swf;

exit:
	if (e) gf_sm_load_done_swf(load);
	return e;
}

#endif /*GPAC_DISABLE_SWF_IMPORT*/


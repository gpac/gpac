/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Copyright (c) Jean Le Feuvre 2000-2005 
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


#ifndef GPAC_READ_ONLY

const char *swf_get_tag(u32 tag);


u16 swf_get_od_id(SWFReader *read)
{
	return ++read->prev_od_id;
}
u16 swf_get_es_id(SWFReader *read)
{
	return ++read->prev_es_id;
}

void swf_init_decompress(SWFReader *read)
{
	read->compressed = (read->sig[0] == 'C') ? 1 : 0; 
	if (!read->compressed) return;
}
void swf_done_decompress(SWFReader *read)
{
}


GF_Err swf_seek_file_to(SWFReader *read, u32 size)
{
	if (!read->compressed) return gf_bs_seek(read->bs, size);
	return GF_NOT_SUPPORTED;
}

u32 swf_get_file_pos(SWFReader *read)
{
	if (!read->compressed) return (u32) gf_bs_get_position(read->bs);
	return 0;
}

u32 swf_read_data(SWFReader *read, char *data, u32 data_size)
{
	if (!read->compressed) return gf_bs_read_data(read->bs, data, data_size);
	return 0;
}

u32 swf_read_int(SWFReader *read, u32 nbBits)
{
	if (!read->compressed) return gf_bs_read_int(read->bs, nbBits);
	return 0;
}

s32 swf_read_sint(SWFReader *read, u32 nbBits)
{
	s32 r = 0;
	u32 i;
	if (!nbBits)return 0;
	r = -1 * (s32) swf_read_int(read, 1);
	for (i=1; i<nbBits; i++){
		r <<= 1;
		r |= swf_read_int(read, 1);
	}
	return r;
}


u32 swf_align(SWFReader *read)
{
	if (!read->compressed) return gf_bs_align(read->bs);
	return 0;
}
void swf_skip_data(SWFReader *read, u32 size)
{
	while (size && !read->ioerr) {
		swf_read_int(read, 8);
		size --;
	}
}
void swf_get_rec(SWFReader *read, SWFRec *rc)
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
u32 swf_get_32(SWFReader *read)
{
	u32 val, res;
	val = swf_read_int(read, 32);
	res = (val&0xFF); res <<=8;
	res |= ((val>>8)&0xFF); res<<=8;
	res |= ((val>>16)&0xFF); res<<=8;
	res|= ((val>>24)&0xFF);
	return res;
}
u16 swf_get_16(SWFReader *read)
{
	u16 val, res;
	val = swf_read_int(read, 16);
	res = (val&0xFF); res <<=8;
	res |= ((val>>8)&0xFF);
	return res;
}
s16 swf_get_s16(SWFReader *read)
{
	s16 val;
	u8 v1;
	v1 = swf_read_int(read, 8);
	val = swf_read_sint(read, 8);
	val = (val<<8)&0xFF00; 
	val |= (v1&0xFF);
	return val;
}
u32 swf_get_color(SWFReader *read)
{
	u32 res;
	res = 0xFF00;
	res |= swf_read_int(read, 8); res<<=8;
	res |= swf_read_int(read, 8); res<<=8;
	res |= swf_read_int(read, 8);
	return res;
}
u32 swf_get_argb(SWFReader *read)
{
	u32 res, al;
	res = swf_read_int(read, 8); res<<=8;
	res |= swf_read_int(read, 8); res<<=8;
	res |= swf_read_int(read, 8);
	al = swf_read_int(read, 8);
	return ((al<<24) | res);
}
u32 swf_get_matrix(SWFReader *read, GF_Matrix2D *mat, Bool rescale)
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

	/*for gradients and bitmap texture transforms*/
	if (rescale) {
		mat->m[0] = gf_mulfix(mat->m[0], FLT2FIX(SWF_TWIP_SCALE));
		mat->m[1] = gf_mulfix(mat->m[1], FLT2FIX(SWF_TWIP_SCALE));
		mat->m[3] = gf_mulfix(mat->m[3], FLT2FIX(SWF_TWIP_SCALE));
		mat->m[4] = gf_mulfix(mat->m[4], FLT2FIX(SWF_TWIP_SCALE));
	}
	return bits_read;
}

void swf_get_colormatrix(SWFReader *read, GF_ColorMatrix *cmat)
{
	Bool has_add, has_mul;
	u32 nbbits;
	memset(cmat, 0, sizeof(GF_ColorMatrix));
	cmat->m[0] = cmat->m[6] = cmat->m[12] = cmat->m[18] = FIX_ONE;

	swf_align(read);
	has_add = swf_read_int(read, 1);
	has_mul = swf_read_int(read, 1);
	nbbits = swf_read_int(read, 4);
	if (has_mul) {
		cmat->m[0] = FLT2FIX( swf_read_int(read, nbbits) * SWF_COLOR_SCALE );
		cmat->m[6] = FLT2FIX( swf_read_int(read, nbbits) * SWF_COLOR_SCALE );
		cmat->m[12] = FLT2FIX( swf_read_int(read, nbbits) * SWF_COLOR_SCALE );
		cmat->m[18] = FLT2FIX( swf_read_int(read, nbbits) * SWF_COLOR_SCALE );
	}
	if (has_add) {
		cmat->m[4] = FLT2FIX( swf_read_int(read, nbbits) * SWF_COLOR_SCALE );
		cmat->m[9] = FLT2FIX( swf_read_int(read, nbbits) * SWF_COLOR_SCALE );
		cmat->m[14] = FLT2FIX( swf_read_int(read, nbbits) * SWF_COLOR_SCALE );
		cmat->m[19] = FLT2FIX( swf_read_int(read, nbbits) * SWF_COLOR_SCALE );
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

char *swf_get_string(SWFReader *read)
{
	char szName[1024];
	u32 i = 0;
	while (1) {
		szName[i] = swf_read_int(read, 8);
		if (!szName[i]) break;
		i++;
	}
	return strdup(szName);
}

GF_Node *SWF_NewNode(SWFReader *read, u32 tag)
{
	GF_Node *n = gf_node_new(read->load->scene_graph, tag);
	if (n) gf_node_init(n);
	return n;
}

Bool SWF_CheckDepth(SWFReader *read, u32 depth)
{
	GF_Node *disp, *empty;
	if (read->max_depth > depth) return 1;
	/*modify display list*/
	disp = gf_sg_find_node_by_name(read->load->scene_graph, "DISPLAYLIST");

	empty = gf_sg_find_node_by_name(read->load->scene_graph, "EMPTYSHAPE");
	while (read->max_depth<=depth) {
		gf_node_insert_child(disp, empty, -1);
		gf_node_register(empty, disp);
		read->max_depth++;
	}
	return 0;
}


SWFShapeRec *swf_new_shape_rec()
{
	SWFShapeRec *style = malloc(sizeof(SWFShapeRec));
	memset(style, 0, sizeof(SWFShapeRec));
	style->path = malloc(sizeof(SWFPath));
	memset(style->path, 0, sizeof(SWFPath));
	return style;
}
SWFShapeRec *swf_clone_shape_rec(SWFShapeRec *old_sr)
{
	SWFShapeRec *new_sr = malloc(sizeof(SWFShapeRec));
	memcpy(new_sr, old_sr, sizeof(SWFShapeRec));
	new_sr->path = malloc(sizeof(SWFPath));
	memset(new_sr->path, 0, sizeof(SWFPath));

	if (old_sr->nbGrad) {
		new_sr->grad_col = malloc(sizeof(u32) * old_sr->nbGrad);
		memcpy(new_sr->grad_col, old_sr->grad_col, sizeof(u32) * old_sr->nbGrad);
		new_sr->grad_ratio = malloc(sizeof(u8) * old_sr->nbGrad);
		memcpy(new_sr->grad_ratio, old_sr->grad_ratio, sizeof(u8) * old_sr->nbGrad);
	}
	return new_sr;
}

/*parse/append fill and line styles*/
void swf_parse_styles(SWFReader *read, u32 revision, SWFShape *shape, u32 *bits_fill, u32 *bits_line)
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
				swf_get_matrix(read, &style->mat, 1);
				swf_align(read);
				style->nbGrad = swf_read_int(read, 8);
				if (style->nbGrad) {
					GF_SAFEALLOC(style->grad_col, sizeof(u32) * style->nbGrad);
					GF_SAFEALLOC(style->grad_ratio, sizeof(u8) * style->nbGrad);
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
						GF_SAFEALLOC(grad_ratio, sizeof(u8) * (style->nbGrad+1));
						GF_SAFEALLOC(grad_col, sizeof(u32) * (style->nbGrad+1));
						grad_col[0] = style->grad_col[0];
						grad_ratio[0] = 0;
						for (i=0; i<style->nbGrad; i++) {
							grad_col[i+1] = style->grad_col[i];
							grad_ratio[i+1] = style->grad_ratio[i];
						}
						free(style->grad_col);
						style->grad_col = grad_col;
						free(style->grad_ratio);
						style->grad_ratio = grad_ratio;
						style->nbGrad++;
					}
					if (style->grad_ratio[style->nbGrad-1] != 255) {
						u32 *grad_col = malloc(sizeof(u32) * (style->nbGrad+1));
						u8 *grad_ratio = malloc(sizeof(u8) * (style->nbGrad+1));
						memcpy(grad_col, style->grad_col, sizeof(u32) * style->nbGrad);
						memcpy(grad_ratio, style->grad_ratio, sizeof(u8) * style->nbGrad);
						grad_col[style->nbGrad] = style->grad_col[style->nbGrad-1];
						grad_ratio[style->nbGrad] = 255;
						free(style->grad_col);
						style->grad_col = grad_col;
						free(style->grad_ratio);
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
				swf_get_matrix(read, &style->mat, 1);
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

void swf_path_realloc_pts(SWFPath *path, u32 nbPts)
{
	path->pts = realloc(path->pts, sizeof(SFVec2f) * (path->nbPts + nbPts));
}
void swf_path_add_com(SWFShapeRec *sr, SFVec2f pt, SFVec2f ctr, u32 type)
{
	/*not an error*/
	if (!sr) return;

	sr->path->types = realloc(sr->path->types, sizeof(u32) * (sr->path->nbType+1));

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

void swf_referse_path(SWFPath *path)
{
	u32 i, j, pti, ptj;
	u32 *types;
	SFVec2f *pts;

	if (path->nbType<=1) return;

	types = (u32 *) malloc(sizeof(u32) * path->nbType);
	pts = (SFVec2f *) malloc(sizeof(SFVec2f) * path->nbPts);


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
	free(path->pts);
	path->pts = pts;
	free(path->types);
	path->types = types;
}

void swf_free_shape_rec(SWFShapeRec *ptr)
{
	if (ptr->grad_col) free(ptr->grad_col);
	if (ptr->grad_ratio) free(ptr->grad_ratio);
	if (ptr->path) {
		if (ptr->path->pts) free(ptr->path->pts);
		if (ptr->path->types) free(ptr->path->types);
		free(ptr->path);
	}
	free(ptr);
}

void swf_free_rec_list(GF_List *recs)
{
	while (gf_list_count(recs)) {
		SWFShapeRec *tmp = gf_list_get(recs, 0);
		gf_list_rem(recs, 0);
		swf_free_shape_rec(tmp);
	}
	gf_list_del(recs);
}

void swf_append_path(SWFPath *a, SWFPath *b)
{
	if (b->nbType<=1) return;

	a->pts = realloc(a->pts, sizeof(SFVec2f) * (a->nbPts + b->nbPts));
	memcpy(&a->pts[a->nbPts], b->pts, sizeof(SFVec2f)*b->nbPts);
	a->nbPts += b->nbPts;

	a->types = realloc(a->types, sizeof(u32)*(a->nbType+ b->nbType));
	memcpy(&a->types[a->nbType], b->types, sizeof(u32)*b->nbType);
	a->nbType += b->nbType;
}

void swf_path_add_type(SWFPath *path, u32 val)
{
	path->types = realloc(path->types, sizeof(u32) * (path->nbType + 1));
	path->types[path->nbType] = val;
	path->nbType++;
}

void swf_resort_path(SWFPath *a, SWFReader *read)
{
	u32 idx, i, j;
	GF_List *paths;
	SWFPath *sorted, *p, *np;

	if (!a->nbType) return;

	paths = gf_list_new();
	sorted = malloc(sizeof(SWFPath));
	memset(sorted, 0, sizeof(SWFPath));
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
			sorted = malloc(sizeof(SWFPath));
			memset(sorted, 0, sizeof(SWFPath));
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
		p = gf_list_get(paths, i);
		j=i+1;
		for (j=i+1; j < gf_list_count(paths); j++) {
			np = gf_list_get(paths, j);
	
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
				free(p->pts);
				free(p->types);
				free(p);
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
				free(np->pts);
				free(np->types);
				free(np);
				gf_list_rem(paths, j);
				j--;
			}
		}
	}

	/*reassemble path*/
	free(a->pts);
	free(a->types);
	memset(a, 0, sizeof(SWFPath));

	while (gf_list_count(paths)) {
		sorted = gf_list_get(paths, 0);
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
		free(sorted->pts);
		free(sorted->types);
		free(sorted);
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

GF_Node *swf_parse_shape_def(SWFReader *read, Bool has_styles, u32 revision)
{
	u32 ID, nbBits, comType, i, count;
	s32 x, y;
	SFVec2f orig, ctrl, end;
	Bool flag;
	u32 fill0, fill1, strike;
	SWFRec rc;
	u32 bits_fill, bits_line;
	SWFShape shape;
	Bool is_empty;
	GF_Node *n;
	u32 fill_base, line_base;
	SWFShapeRec *sf0, *sf1, *sl;

	memset(&shape, 0, sizeof(SWFShape));
	shape.fill_left = gf_list_new();
	shape.fill_right = gf_list_new();
	shape.lines = gf_list_new();
	ctrl.x = ctrl.y = 0;
	swf_align(read);
	ID = 0;

	/*get initial styles*/
	if (has_styles) {
		ID = swf_get_16(read);
		/*don't care about that...*/
		swf_get_rec(read, &rc);
		swf_parse_styles(read, revision, &shape, &bits_fill, &bits_line);
	} else {
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
	fill_base = line_base = 0;
	
	is_empty = 1;
	comType = 0;
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
			if (set_fill0) fill0 = fill_base + swf_read_int(read, bits_fill);
			if (set_fill1) fill1 = fill_base + swf_read_int(read, bits_fill);
			if (set_strike) strike = line_base + swf_read_int(read, bits_line);
			/*looks like newStyle does not append styles but define a new set - old styles can no 
			longer be referenced*/
			if (new_style) {
				fill_base += gf_list_count(shape.fill_left);
				line_base += gf_list_count(shape.lines);
				swf_parse_styles(read, revision, &shape, &bits_fill, &bits_line);
			}

			if (read->flags & GF_SM_SWF_NO_LINE) strike = 0;

			/*moveto*/
			comType = 0;
			orig.x = FLT2FIX( x * SWF_TWIP_SCALE );
			orig.y = FLT2FIX( y * SWF_TWIP_SCALE );
			end = orig;

			sf0 = fill0 ? gf_list_get(shape.fill_left, fill0 - 1) : NULL;
			sf1 = fill1 ? gf_list_get(shape.fill_right, fill1 - 1) : NULL;
			sl = strike ? gf_list_get(shape.lines, strike - 1) : NULL;

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
		swf_free_rec_list(shape.fill_left);
		swf_free_rec_list(shape.fill_right);
		swf_free_rec_list(shape.lines);
		return NULL;
	}

	swf_align(read);

	count = gf_list_count(shape.fill_left);
	for (i=0; i<count; i++) {
		sf0 = gf_list_get(shape.fill_left, i);
		sf1 = gf_list_get(shape.fill_right, i);
		/*reverse right path*/
		swf_referse_path(sf1->path);
		/*concatenate with left path*/
		swf_append_path(sf0->path, sf1->path);
		/*resort all path curves*/
		swf_resort_path(sf0->path, read);
	}
	/*remove dummy fill_left*/
	for (i=0; i<gf_list_count(shape.fill_left); i++) {
		sf0 = gf_list_get(shape.fill_left, i);
		if (sf0->path->nbType<=1) {
			gf_list_rem(shape.fill_left, i);
			swf_free_shape_rec(sf0);
			i--;
		}
	}
	/*remove dummy lines*/
	for (i=0; i<gf_list_count(shape.lines); i++) {
		sl = gf_list_get(shape.lines, i);
		if (sl->path->nbType<1) {
			gf_list_rem(shape.lines, i);
			swf_free_shape_rec(sl);
			i--;
		} else {
			swf_resort_path(sl->path, read);
		}
	}

	/*now translate a flash shape record into BIFS*/
	shape.ID = ID;
	n = SWFShapeToBIFS(read, &shape);

	/*delete shape*/
	swf_free_rec_list(shape.fill_left);
	swf_free_rec_list(shape.fill_right);
	swf_free_rec_list(shape.lines);

	if (n && has_styles) {
		char szDEF[1024];
		sprintf(szDEF, "Shape%d", ID);
		read->load->ctx->max_node_id++;
		ID = read->load->ctx->max_node_id;
		gf_node_set_id(n, ID, szDEF);
	}
	return n;
}

SWFFont *SWF_FindFont(SWFReader *read, u32 ID)
{
	u32 i, count;
	count = gf_list_count(read->fonts);
	for (i=0; i<count; i++) {
		SWFFont *ft = gf_list_get(read->fonts, i);
		if (ft->fontID==ID) return ft;
	}
	return NULL;
}

GF_Node *SWF_GetNode(SWFReader *read, u32 ID)
{
	GF_Node *n;
	char szDEF[1024];
	sprintf(szDEF, "Shape%d", ID);
	n = gf_sg_find_node_by_name(read->load->scene_graph, szDEF);
	if (n) return n;
	sprintf(szDEF, "Text%d", ID);
	n = gf_sg_find_node_by_name(read->load->scene_graph, szDEF);
	if (n) return n;
	sprintf(szDEF, "Button%d", ID);
	n = gf_sg_find_node_by_name(read->load->scene_graph, szDEF);
	if (n) return n;
	return NULL;
}
DispShape *SWF_GetDepthEntry(SWFReader *read, u32 Depth, Bool create)
{
	u32 i;
	DispShape *tmp;
	i=0;
	while ((tmp = gf_list_enum(read->display_list, &i))) {
		if (tmp->depth == Depth) return tmp;
	}
	if (!create) return NULL;
	tmp = malloc(sizeof(DispShape));
	tmp->depth = Depth;
	tmp->n = NULL;
	gf_list_add(read->display_list, tmp);

	memset(&tmp->mat, 0, sizeof(GF_Matrix2D));
	tmp->mat.m[0] = tmp->mat.m[4] = FIX_ONE;
	
	memset(&tmp->cmat, 0, sizeof(GF_ColorMatrix));
	tmp->cmat.m[0] = tmp->cmat.m[6] = tmp->cmat.m[12] = tmp->cmat.m[18] = FIX_ONE;
	tmp->cmat.identity = 1;
	return tmp;
}



GF_Err swf_func_skip(SWFReader *read)
{
	swf_skip_data(read, read->size);
	return read->ioerr;
}
GF_Err swf_unknown_tag(SWFReader *read)
{
	swf_report(read, GF_NOT_SUPPORTED, "Tag not implemented - skipping");
	return swf_func_skip(read);
}
GF_Err swf_set_backcol(SWFReader *read)
{
	u32 col;
	GF_Command *com;
	GF_CommandField *f;
	com = gf_sg_command_new(read->load->scene_graph, GF_SG_FIELD_REPLACE);
	com->node = gf_sg_find_node_by_name(read->load->scene_graph, "BACKGROUND");
	gf_node_register(com->node, NULL);
	f = gf_sg_command_field_new(com);
	f->field_ptr = gf_sg_vrml_field_pointer_new(GF_SG_VRML_SFCOLOR);
	f->fieldType = GF_SG_VRML_SFCOLOR;
	f->fieldIndex = 1;	/*backColor index*/
	col = swf_get_color(read);
	((SFColor *)f->field_ptr)->red = INT2FIX((col>>16) & 0xFF) / 255;
	((SFColor *)f->field_ptr)->green = INT2FIX((col>>8) & 0xFF) / 255;
	((SFColor *)f->field_ptr)->blue = INT2FIX((col) & 0xFF) / 255;
	gf_list_add(read->bifs_au->commands, com);
	return GF_OK;
}


GF_Err SWF_InsertNode(SWFReader *read, GF_Node *n)
{
	GF_Command *com;
	GF_CommandField *f;

	if (read->flags & GF_SM_SWF_STATIC_DICT) {
		M_Switch *par = (M_Switch *)gf_sg_find_node_by_name(read->load->scene_graph, "DICTIONARY");
		gf_list_add(par->choice, n);
		gf_node_register((GF_Node *)n, (GF_Node *)par);
	} else {
		com = gf_sg_command_new(read->load->scene_graph, GF_SG_INDEXED_INSERT);
		com->node = gf_sg_find_node_by_name(read->load->scene_graph, "DICTIONARY");
		gf_node_register(com->node, NULL);
		f = gf_sg_command_field_new(com);
		f->field_ptr = &f->new_node;
		f->fieldType = GF_SG_VRML_SFNODE;
		f->fieldIndex = 0;	/*choice index*/
		f->pos = -1;
		f->new_node = n;
		gf_node_register(f->new_node, NULL);
		gf_list_add(read->bifs_au->commands, com);
	}
	return GF_OK;
}

GF_Err swf_def_shape(SWFReader *read, u32 revision)
{
	GF_Node *new_node;
	new_node = swf_parse_shape_def(read, 1, revision);
	if (!new_node) return GF_OK;
	return SWF_InsertNode(read, new_node);
}

typedef struct
{
	Bool hitTest, down, over, up;
	u32 character_id;
	u16 depth;
	GF_Matrix2D mx;
	GF_ColorMatrix cmx;
} SWF_ButtonRecord;

GF_Err swf_def_button(SWFReader *read, u32 revision)
{
	char szName[1024];
	SWF_ButtonRecord recs[40];
	u32 i, ID, nb_but_rec;
	M_Switch *button;
	Bool has_actions;

	has_actions = 0;
	ID = swf_get_16(read);
	if (revision==1) {
		gf_bs_read_int(read->bs, 7);
		gf_bs_read_int(read->bs, 1);
		has_actions = swf_get_16(read);
	}
	nb_but_rec = 0;
	while (1) {
		SWF_ButtonRecord *rec = &recs[nb_but_rec];
		gf_bs_read_int(read->bs, 4);
		rec->hitTest = gf_bs_read_int(read->bs, 1);
		rec->down = gf_bs_read_int(read->bs, 1);
		rec->over = gf_bs_read_int(read->bs, 1);
		rec->up = gf_bs_read_int(read->bs, 1);
		if (!rec->hitTest && !rec->up && !rec->over && !rec->down) break;
		rec->character_id = swf_get_16(read);
		rec->depth = swf_get_16(read);
		swf_get_matrix(read, &rec->mx, 0);
		if (revision==1) swf_get_colormatrix(read, &rec->cmx);
		else gf_cmx_init(&rec->cmx);
		gf_bs_align(read->bs);
		nb_but_rec++;
	}
	if (revision==0) {
		while (1) {
			u32 act_type = gf_bs_read_u8(read->bs);
			if (!act_type) break; 
			if (act_type > 0x80) {
				u32 len = swf_get_16(read); 
				gf_bs_skip_bytes(read->bs, len);
			}
		}
	} else {
		while (has_actions) {
			has_actions = swf_get_16(read);
			swf_get_16(read);
			while (1) {
				u32 act_type = gf_bs_read_u8(read->bs);
				if (act_type > 0x80) {
					u32 len = swf_get_16(read); 
					gf_bs_skip_bytes(read->bs, len);
				}
			}
		}
	}
	button = (M_Switch *) SWF_NewNode(read, TAG_MPEG4_Switch);
	sprintf(szName, "Button%d", ID);
	read->load->ctx->max_node_id++;
	ID = read->load->ctx->max_node_id;
	gf_node_set_id((GF_Node *)button, ID, szName);
	SWF_InsertNode(read, (GF_Node *)button);
	/*by default show first character*/
	button->whichChoice = 0;
	for (i=0; i<nb_but_rec; i++) {
		GF_Node *character = SWF_GetNode(read, recs[i].character_id);
		if (character) {
			gf_list_add(button->choice, character);
			gf_node_register(character, (GF_Node *)button);
		}
	}
	return GF_OK;
}

Bool swf_mat_is_identity(GF_Matrix2D *mat)
{
	if (mat->m[0] != FIX_ONE) return 0;
	if (mat->m[4] != FIX_ONE) return 0;
	if (mat->m[1] != FIX_ONE) return 0;
	if (mat->m[2] != FIX_ONE) return 0;
	if (mat->m[3] != FIX_ONE) return 0;
	if (mat->m[5] != FIX_ONE) return 0;
	return 1;
}

enum
{
	SWF_PLACE,
	SWF_REPLACE,
	SWF_MOVE,
};
GF_Err swf_place_obj(SWFReader *read, u32 revision)
{
	GF_Command *com;
	GF_CommandField *f;
	u32 ID, bitsize, ratio;
	u32 clip_depth;
	GF_Matrix2D mat;
	GF_ColorMatrix cmat;
	GF_Node *shape, *par;
	DispShape *ds;
	char *name;
	char szDEF[100];
	u32 depth, type;
	Bool had_depth, is_sprite;
	/*SWF flags*/
	Bool has_clip, has_name, has_ratio, has_cmat, has_mat, has_id, has_move;
	
	name = NULL;
	clip_depth = 0;
	ID = 0;
	depth = 0;
	has_clip = has_name = has_ratio = has_cmat = has_mat = has_id = has_move = 0;

	gf_cmx_init(&cmat);
	gf_mx2d_init(mat);
	/*place*/
	type = SWF_PLACE;

	/*SWF 1.0*/
	if (revision==0) {
		ID = swf_get_16(read);
		has_id = 1;
		depth = swf_get_16(read);
		bitsize = 32;
		bitsize += swf_get_matrix(read, &mat, 0);
		has_mat = 1;
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
		swf_read_int(read, 1);
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
			swf_get_matrix(read, &mat, 0);
			swf_align(read);
		}
		if (has_cmat) {
			swf_get_colormatrix(read, &cmat);
			swf_align(read);
		}
		if (has_ratio) ratio = swf_get_16(read);
		if (has_clip) clip_depth = swf_get_16(read);

		if (has_name) {
			name = swf_get_string(read);
			free(name);
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
	had_depth = SWF_CheckDepth(read, depth);
	/*check validity*/
	if ((type==SWF_MOVE) && !had_depth) swf_report(read, GF_BAD_PARAM, "Accessing empty depth level %d", depth);

	ds = NULL;
	shape = NULL;
	/*usual case: (re)place depth level*/
	switch (type) {
	case SWF_MOVE:
		ds = SWF_GetDepthEntry(read, depth, 0);
		shape = ds ? ds->n : NULL;
		break;
	case SWF_REPLACE:
	case SWF_PLACE:
	default:
		assert(has_id);
		shape = SWF_GetNode(read, ID);
		break;
	}

	is_sprite = 0;
	if (!shape) {
		/*this may be a sprite*/
		if (type != SWF_MOVE) {
			sprintf(szDEF, "Sprite%d_root", ID);
			shape = gf_sg_find_node_by_name(read->load->scene_graph, szDEF);
			if (shape) is_sprite = 1;
		}
		if (!shape) {
			swf_report(read, GF_BAD_PARAM, "%s unfound object (ID %d)", (type==SWF_MOVE) ? "Moving" : ((type==SWF_PLACE) ? "Placing" : "Replacing"), ID);
			return GF_OK;
		}
	}
	/*restore prev matrix if needed*/
	if (type==SWF_REPLACE) {
		if (!ds) ds = SWF_GetDepthEntry(read, depth, 0);
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


	/*then add cmat/mat and node*/
	par = NULL;
	if (!has_mat && !has_cmat) {
		par = shape;
	} else {
		if (has_mat) par = SWF_GetBIFSMatrix(read, &mat);
		if (has_cmat) {
			GF_Node *cm = SWF_GetBIFSColorMatrix(read, &cmat);
			if (!par) {
				par = cm;
				gf_node_insert_child(par, shape, -1);
				gf_node_register(shape, par);
			} else {
				gf_node_insert_child(par, cm, -1);
				gf_node_register(cm, par);
				gf_node_insert_child(cm, shape, -1);
				gf_node_register(shape, cm);
			}
		} else {
			gf_node_insert_child(par, shape, -1);
			gf_node_register(shape, par);
		}
	}
	/*store in display list*/
	ds = SWF_GetDepthEntry(read, depth, 1);
	ds->n = shape;
	/*remember matrices*/
	memcpy(&ds->mat, &mat, sizeof(GF_Matrix2D));
	memcpy(&ds->cmat, &cmat, sizeof(GF_ColorMatrix));

	/*and write command*/
	com = gf_sg_command_new(read->load->scene_graph, GF_SG_INDEXED_REPLACE);
	/*in sprite definiton, modify at sprite root level*/
	if (0 && read->current_sprite_id) {
		sprintf(szDEF, "Sprite%d_root", read->current_sprite_id);
		com->node = gf_sg_find_node_by_name(read->load->scene_graph, szDEF);
		depth = 0;
	} else {
		com->node = gf_sg_find_node_by_name(read->load->scene_graph, "DISPLAYLIST");
	}
	gf_node_register(com->node, NULL);
	f = gf_sg_command_field_new(com);
	f->field_ptr = &f->new_node;
	f->fieldType = GF_SG_VRML_SFNODE;
	f->pos = depth;
	f->fieldIndex = 2;	/*children index*/
	f->new_node = par;
	gf_node_register(f->new_node, com->node);
	gf_list_add(read->bifs_au->commands, com);

	/*starts anim*/
	if (is_sprite) {
		sprintf(szDEF, "Sprite%d_ctrl", ID);
		com = gf_sg_command_new(read->load->scene_graph, GF_SG_FIELD_REPLACE);
		com->node = gf_sg_find_node_by_name(read->load->scene_graph, szDEF);
		gf_node_register(com->node, NULL);
		f = gf_sg_command_field_new(com);
		f->field_ptr = gf_sg_vrml_field_pointer_new(GF_SG_VRML_SFTIME);
		*(SFTime *)f->field_ptr = ((Double) (s64) read->bifs_au->timing) / read->bifs_es->timeScale;
		f->fieldType = GF_SG_VRML_SFTIME;
		f->fieldIndex = 2;	/*startTime index*/
		gf_list_add(read->bifs_au->commands, com);
	}
	return GF_OK;
}

GF_Err swf_remove_obj(SWFReader *read, u32 revision)
{
	GF_Command *com;
	GF_CommandField *f;
	DispShape *ds;
	u32 depth;
	if (revision==0) swf_get_16(read);
	depth = swf_get_16(read);
	ds = SWF_GetDepthEntry(read, depth, 0);
	/*this happens if a placeObject has failed*/
	if (!ds) return GF_OK;
	ds->n = NULL;

	com = gf_sg_command_new(read->load->scene_graph, GF_SG_INDEXED_REPLACE);
	com->node = gf_sg_find_node_by_name(read->load->scene_graph, "DISPLAYLIST");
	gf_node_register(com->node, NULL);
	f = gf_sg_command_field_new(com);
	f->field_ptr = &f->new_node;
	f->fieldType = GF_SG_VRML_SFNODE;
	f->pos = depth;
	f->fieldIndex = 2;	/*children index*/
	f->new_node = gf_sg_find_node_by_name(read->load->scene_graph, "EMPTYSHAPE");
	gf_node_register(f->new_node, com->node);
	gf_list_add(read->bifs_au->commands, com);
	return GF_OK;
}

GF_Err swf_show_frame(SWFReader *read)
{
	u32 ts;
	Bool is_rap;

	/*hack to allow for empty BIFS AU to be encoded in order to keep the frame-rate (this reduces MP4 table size...)*/
	if (0 && !gf_list_count(read->bifs_au->commands)) {
		GF_Command *com;
		GF_CommandField *f;
		com = gf_sg_command_new(read->load->scene_graph, GF_SG_FIELD_REPLACE);
		com->node = gf_sg_find_node_by_name(read->load->scene_graph, "DICTIONARY");
		gf_node_register(com->node, NULL);
		f = gf_sg_command_field_new(com);
		f->field_ptr = gf_sg_vrml_field_pointer_new(GF_SG_VRML_SFINT32);
		f->fieldType = GF_SG_VRML_SFINT32;
		f->fieldIndex = 1;	/*whichCoice index*/
		/*replace by same value*/
		*((SFInt32 *)f->field_ptr) = -1;
		gf_list_add(read->bifs_au->commands, com);
	}

	/*flush AU*/
	read->current_frame ++;
	ts = read->current_frame * 100;
	is_rap = read->current_sprite_id ? 1 : 0;
	/*if we use ctrl stream, same thing*/
	read->bifs_au = gf_sm_stream_au_new(read->bifs_es, ts, 0, 
				/*all frames in sprites are RAP (define is not allowed in sprites)
				if we use a ctrl stream, all AUs are RAP (defines are placed in a static dictionary)
				*/
				(read->current_sprite_id || (read->flags & GF_SM_SWF_SPLIT_TIMELINE)) ? 1 : 0);
	return GF_OK;
}

GF_Err swf_def_font(SWFReader *read, u32 revision)
{
	u32 i, count;
	GF_Err e;
	GF_Node *glyph;
	SWFFont *ft;
	u32 *offset_table;
	u32 start;
	ft = malloc(sizeof(SWFFont));
	memset(ft, 0, sizeof(SWFFont));
	ft->glyphs = gf_list_new();
	ft->fontID = swf_get_16(read);
	e = GF_OK;


	if (revision==0) {
		u32 count;

		start = swf_get_file_pos(read);

		count = swf_get_16(read);
		ft->nbGlyphs = count / 2;
		offset_table = malloc(sizeof(u32) * ft->nbGlyphs);
	    offset_table[0] = 0;
		for (i=1; i<ft->nbGlyphs; i++) offset_table[i] = swf_get_16(read);

		for (i=0; i<ft->nbGlyphs; i++) {
			swf_align(read);
			e = swf_seek_file_to(read, start + offset_table[i]);
			if (e) break;
			while (1) {
				glyph = swf_parse_shape_def(read, 0, 0);
				/*not a mistake, that's likelly space char*/
				if (!glyph) glyph = SWF_NewNode(read, TAG_MPEG4_Shape);
				gf_list_add(ft->glyphs, glyph);
				gf_node_register(glyph, NULL);
				break;
			}
		}
		free(offset_table);
		if (e) return e;
	} else if (revision==1) {
		SWFRec rc;
		Bool wide_offset, wide_codes;
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
		ft->fontName = malloc(sizeof(u8)*count+1);
		ft->fontName[count] = 0;
		for (i=0; i<count; i++) ft->fontName[i] = swf_read_int(read, 8);

		ft->nbGlyphs = swf_get_16(read);
		start = swf_get_file_pos(read);

		if (ft->nbGlyphs) {
			u32 code_offset, checkpos;

			offset_table = malloc(sizeof(u32) * ft->nbGlyphs);
			for (i=0; i<ft->nbGlyphs; i++) {
				if (wide_offset) offset_table[i] = swf_get_32(read);
				else offset_table[i] = swf_get_16(read);
			}
			
			if (wide_offset) {
				code_offset = swf_get_32(read);
			} else {
				code_offset = swf_get_16(read);
			}

			for (i=0; i<ft->nbGlyphs; i++) {
				swf_align(read);
				e = swf_seek_file_to(read, start + offset_table[i]);
				if (e) break;
				while (1) {
					glyph = swf_parse_shape_def(read, 0, 0);
					if (!glyph) continue;
					gf_list_add(ft->glyphs, glyph);
					gf_node_register(glyph, NULL);
					break;
				}
			}
			free(offset_table);
			if (e) return e;

			checkpos = swf_get_file_pos(read);
			if (checkpos != start + code_offset) {
				GF_LOG(GF_LOG_ERROR, GF_LOG_CODING, ("[SWF Parsing] bad code offset in font\n"));
				return GF_NON_COMPLIANT_BITSTREAM;
			}

			ft->glyph_codes = malloc(sizeof(u16) * ft->nbGlyphs);
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
				ft->glyph_adv = malloc(sizeof(s16) * ft->nbGlyphs);
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

GF_Err swf_def_font_info(SWFReader *read)
{
	SWFFont *ft;
	Bool wide_chars;
	u32 i, count;
	
	i = swf_get_16(read);
	ft = SWF_FindFont(read, i);
	if (!ft) {
		swf_report(read, GF_BAD_PARAM, "Cannot locate font ID %d", i);
		return GF_BAD_PARAM;
	}
	/*overwrite font info*/
	if (ft->fontName) free(ft->fontName);
	count = swf_read_int(read, 8);
	ft->fontName = malloc(sizeof(char) * (count+1));
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
	if (ft->glyph_codes) free(ft->glyph_codes);
	ft->glyph_codes = malloc(sizeof(u16) * ft->nbGlyphs);

	for (i=0; i<ft->nbGlyphs; i++) {
		if (wide_chars) ft->glyph_codes[i] = swf_get_16(read);
		else ft->glyph_codes[i] = swf_read_int(read, 8);
	}
	return GF_OK;
}

GF_Err swf_def_text(SWFReader *read, u32 revision)
{
	SWFRec rc;
	SWFText txt;
	Bool flag;
	GF_Node *n;
	u32 ID, nbits_adv, nbits_glyph, i, col, fontID, count;
	Fixed offX, offY, fontHeight;
	GF_Err e;

	ID = swf_get_16(read);
	swf_get_rec(read, &rc);
	swf_get_matrix(read, &txt.mat, 0);
	txt.text = gf_list_new();

	swf_align(read);
	nbits_glyph = swf_read_int(read, 8);
	nbits_adv = swf_read_int(read, 8);
	fontID = 0;
	offX = offY = fontHeight = 0;
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

			gr = malloc(sizeof(SWFGlyphRec));
			memset(gr, 0, sizeof(SWFGlyphRec));
			gf_list_add(txt.text, gr);
			gr->fontID = fontID;
			gr->fontHeight = fontHeight;
			gr->col = col;
			gr->orig_x = offX;
			gr->orig_y = offY;
			gr->nbGlyphs = count;
			gr->indexes = malloc(sizeof(u32) * gr->nbGlyphs);
			gr->dx = malloc(sizeof(Fixed) * gr->nbGlyphs);
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
			if (has_font) fontHeight = FLT2FIX( swf_get_16(read) * SWF_TEXT_SCALE );
		}
	}

	if (e) goto exit;

	if (! (read->flags & GF_SM_SWF_NO_TEXT) ) {
		n = SWFTextToBIFS(read, &txt);
		if (n) {
			char szDEF[1024];
			sprintf(szDEF, "Text%d", ID);
			read->load->ctx->max_node_id++;
			ID = read->load->ctx->max_node_id;
			gf_node_set_id(n, ID, szDEF);
			SWF_InsertNode(read, n);
		}
	}

exit:
	while (gf_list_count(txt.text)) {
		SWFGlyphRec *gr = gf_list_get(txt.text, 0);
		gf_list_rem(txt.text, 0);
		if (gr->indexes) free(gr->indexes);
		if (gr->dx) free(gr->dx);
		free(gr);
	}
	gf_list_del(txt.text);

	return e;
}

GF_Err swf_init_od(SWFReader *read)
{
	GF_ESD *esd;
	if (read->od_es) return GF_OK;
	read->od_es = gf_sm_stream_new(read->load->ctx, 2, 1, 1);
	if (!read->od_es) return GF_OUT_OF_MEM;
	if (!read->load->ctx->root_od) {
		GF_BIFSConfig *bc;
		read->load->ctx->root_od = (GF_ObjectDescriptor *) gf_odf_desc_new(GF_ODF_IOD_TAG);
		/*add BIFS stream*/
		esd = (GF_ESD *) gf_odf_desc_esd_new(0);
		if (!esd) return GF_OUT_OF_MEM;
		esd->decoderConfig->streamType = GF_STREAM_SCENE;
		esd->decoderConfig->objectTypeIndication = 1;
		esd->slConfig->timestampResolution = read->bifs_es->timeScale;
		esd->ESID = 1;
		gf_list_add(read->load->ctx->root_od->ESDescriptors, esd);
		read->load->ctx->root_od->objectDescriptorID = 1;
		gf_odf_desc_del((GF_Descriptor *) esd->decoderConfig->decoderSpecificInfo);
		bc = (GF_BIFSConfig *) gf_odf_desc_new(GF_ODF_BIFS_CFG_TAG);
		bc->pixelMetrics = 1;
		bc->pixelWidth = (u16) read->width;
		bc->pixelHeight = (u16) read->height;
		esd->decoderConfig->decoderSpecificInfo = (GF_DefaultDescriptor *) bc;
	}
	if (!read->load->ctx->root_od) return GF_OUT_OF_MEM;
	esd = (GF_ESD *) gf_odf_desc_esd_new(0);
	if (!esd) return GF_OUT_OF_MEM;
	esd->decoderConfig->streamType = GF_STREAM_OD;
	esd->decoderConfig->objectTypeIndication = 1;
	esd->slConfig->timestampResolution = read->od_es->timeScale = read->bifs_es->timeScale;
	esd->ESID = 2;
	esd->OCRESID = 1;
	gf_odf_desc_del((GF_Descriptor *) esd->decoderConfig->decoderSpecificInfo);
	esd->decoderConfig->decoderSpecificInfo = NULL;
	return gf_list_add(read->load->ctx->root_od->ESDescriptors, esd);
}

GF_Err swf_insert_od(SWFReader *read, u32 at_time, GF_ObjectDescriptor *od)
{
	u32 i;
	GF_ODUpdate *com;
	read->od_au = gf_sm_stream_au_new(read->od_es, at_time, 0, 1);
	if (!read->od_au) return GF_OUT_OF_MEM;

	i=0;
	while ((com = gf_list_enum(read->od_au->commands, &i))) {
		if (com->tag == GF_ODF_OD_UPDATE_TAG) {
			gf_list_add(com->objectDescriptors, od);
			return GF_OK;
		}
	}
	com = (GF_ODUpdate *) gf_odf_com_new(GF_ODF_OD_UPDATE_TAG);
	gf_list_add(com->objectDescriptors, od);
	return gf_list_add(read->od_au->commands, com);
}


void swf_delete_sound_stream(SWFReader *read)
{
	if (!read->sound_stream) return;
	if (read->sound_stream->output) fclose(read->sound_stream->output);
	if (read->sound_stream->szFileName) free(read->sound_stream->szFileName);
	free(read->sound_stream);
	read->sound_stream = NULL;
}



GF_Err swf_def_sprite(SWFReader *read)
{
	GF_Err SWF_ParseTag(SWFReader *read);
	GF_Err e;
	GF_ObjectDescriptor *od;
	GF_ESD *esd;
	u32 spriteID, ID;
	u32 frame_count;
	Bool prev_sprite;
	u32 prev_frame;
	GF_Node *n, *par;
	GF_FieldInfo info;
	char szDEF[100];
	SWFSound *snd;

	GF_StreamContext *prev_sc;
	GF_AUContext *prev_au;

	spriteID = swf_get_16(read);
	frame_count = swf_get_16(read);

	/*init OD*/
	e = swf_init_od(read);
	if (e) return e;

	/*create animationStream object*/
	od = (GF_ObjectDescriptor *) gf_odf_desc_new(GF_ODF_OD_TAG);
	if (!od) return GF_OUT_OF_MEM;

	od->objectDescriptorID = swf_get_od_id(read);
	esd = (GF_ESD *) gf_odf_desc_esd_new(0);
	if (!esd) return GF_OUT_OF_MEM;
	esd->ESID = swf_get_es_id(read);
	/*sprite runs on its own timeline*/
	esd->OCRESID = esd->ESID;
	/*always depends on main scene*/
	esd->dependsOnESID = 1;
	esd->decoderConfig->streamType = GF_STREAM_SCENE;
	esd->decoderConfig->objectTypeIndication = 1;
	esd->slConfig->timestampResolution = read->bifs_es->timeScale;
	gf_odf_desc_del((GF_Descriptor *) esd->decoderConfig->decoderSpecificInfo);
	esd->decoderConfig->decoderSpecificInfo = NULL;
	gf_list_add(od->ESDescriptors, esd);

	/*by default insert OD at begining*/
	e = swf_insert_od(read, 0, od);
	if (e) {
		gf_odf_desc_del((GF_Descriptor *) od);
		return e;
	}

	/*create AS for sprite - all AS are created in initial scene replace*/
	n = SWF_NewNode(read, TAG_MPEG4_AnimationStream);
	sprintf(szDEF, "Sprite%d_ctrl", spriteID);

	read->load->ctx->max_node_id++;
	ID = read->load->ctx->max_node_id;
	gf_node_set_id(n, ID, szDEF);

	gf_node_insert_child(read->root, n, 0);
	gf_node_register(n, read->root);
	/*assign URL*/
	gf_node_get_field_by_name(n, "url", &info);
	gf_sg_vrml_mf_alloc(info.far_ptr, info.fieldType, 1);
	((MFURL*)info.far_ptr)->vals[0].OD_ID = od->objectDescriptorID;
	/*inactive by default (until inserted)*/
	((M_AnimationStream *)n)->startTime = -1;
	/*loop by default - not 100% sure from SWF spec, I believe a sprite loops until removed from DList*/
	((M_AnimationStream *)n)->loop = 0;

	/*create sprite grouping node*/
	n = SWF_NewNode(read, TAG_MPEG4_Group);
	sprintf(szDEF, "Sprite%d_root", spriteID);

	read->load->ctx->max_node_id++;
	ID = read->load->ctx->max_node_id;
	gf_node_set_id(n, ID, szDEF);
	par = gf_sg_find_node_by_name(read->load->scene_graph, "DICTIONARY");
	assert(par);
	gf_list_add(((M_Switch *)par)->choice, n);
	gf_node_register(n, par);
	par = gf_sg_find_node_by_name(read->load->scene_graph, "EMPTYSHAPE");
	gf_node_insert_child(n, par, -1);
	gf_node_register(par, n);

	/*store BIFS context*/
	prev_frame = read->current_frame;
	prev_sc = read->bifs_es;
	prev_au = read->bifs_au;
	prev_sprite = read->current_sprite_id;
	/*create new BIFS stream*/
	read->bifs_es = gf_sm_stream_new(read->load->ctx, esd->ESID, GF_STREAM_SCENE, 1);
	read->bifs_es->timeScale = prev_sc->timeScale;
	read->current_frame = 0;
	/*create first AU*/
	read->bifs_au = gf_sm_stream_au_new(read->bifs_es, 0, 0, 1);
	read->current_sprite_id = spriteID;
	/*store soundStream*/
	snd = read->sound_stream;
	read->sound_stream = NULL;

	/*and parse*/
	while (1) {
		e = SWF_ParseTag(read);
		if (e<0) return e;
		/*done with sprite*/
		if (read->tag==SWF_END) break;
	}
	/*restore BIFS context*/
	read->current_frame = prev_frame;
	read->bifs_es = prev_sc;
	read->bifs_au = prev_au;
	read->current_sprite_id = prev_sprite;

	/*close sprite soundStream*/
	swf_delete_sound_stream(read);
	/*restore sound stream*/
	read->sound_stream = snd;

	read->tag = SWF_DEFINESPRITE;
	return GF_OK;
}

GF_Err swf_def_sound(SWFReader *read)
{
	SWFSound *snd;
	snd = malloc(sizeof(SWFSound));
	memset(snd, 0, sizeof(SWFSound));
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
		free(snd);
		break;
	/*ADPCM*/
	case 1:
		swf_report(read, GF_NOT_SUPPORTED, "AD-PCM Audio not supported");
		free(snd);
		break;
	/*MP3*/
	case 2:
	{
		unsigned char bytes[4];
		char szName[1024];
		u32 hdr, alloc_size, size, tot_size;
		char *frame;

		sprintf(szName, "swf_sound_%d.mp3", snd->ID);
		if (read->localPath) {
			snd->szFileName = malloc(sizeof(char)*GF_MAX_PATH);
			strcpy(snd->szFileName, read->localPath);
			strcat(snd->szFileName, szName);
		} else {
			snd->szFileName = strdup(szName);
		}
		snd->output = fopen(snd->szFileName, "wb");

		alloc_size = 1;
		frame = malloc(sizeof(char));
		snd->frame_delay_ms = swf_get_16(read);
		snd->frame_delay_ms = 0;
		tot_size = 9;
		/*parse all frames*/
		while (1) {
			bytes[0] = swf_read_int(read, 8);
			bytes[1] = swf_read_int(read, 8);
			bytes[2] = swf_read_int(read, 8);
			bytes[3] = swf_read_int(read, 8);
			hdr = GF_4CC(bytes[0], bytes[1], bytes[2], bytes[3]);
			size = gf_mp3_frame_size(hdr);
			if (alloc_size<size-4) {
				frame = realloc(frame, sizeof(char)*(size-4));
				alloc_size = size-4;
			}
			/*watchout for truncated framesif */
			if (tot_size + size >= read->size) size = read->size - tot_size;

			swf_read_data(read, frame, size-4);
			fwrite(bytes, sizeof(char)*4, 1, snd->output);
			fwrite(frame, sizeof(char)*(size-4), 1, snd->output);
			if (tot_size + size >= read->size) break;
			tot_size += size;
		}
		free(frame);
		return gf_list_add(read->sounds, snd);
	}
	case 3:
		swf_report(read, GF_NOT_SUPPORTED, "Unrecognized sound format");
		free(snd);
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

SoundInfo swf_skip_soundinfo(SWFReader *read)
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

SWFSound *sndswf_get_sound(SWFReader *read, u32 ID)
{
	u32 i;
	SWFSound *snd;
	i=0;
	while ((snd = gf_list_enum(read->sounds, &i))) {
		if (snd->ID==ID) return snd;
	}
	return NULL;
}


GF_Err swf_setup_sound(SWFReader *read, SWFSound *snd)
{
	GF_Err e;
	GF_ObjectDescriptor *od;
	GF_ESD *esd;
	GF_MuxInfo *mux;
	GF_Node *n, *par;
	GF_FieldInfo info;
	u32 ID;
	char szDEF[100];

	e = swf_init_od(read);
	if (e) return e;

	/*create audio object*/
	od = (GF_ObjectDescriptor *) gf_odf_desc_new(GF_ODF_OD_TAG);
	if (!od) return GF_OUT_OF_MEM;
	od->objectDescriptorID = swf_get_od_id(read);
	esd = (GF_ESD *) gf_odf_desc_new(GF_ODF_ESD_TAG);
	if (!esd) return GF_OUT_OF_MEM;
	esd->ESID = swf_get_es_id(read);
	if (snd->ID) {
		/*sound runs on its own timeline*/
		esd->OCRESID = esd->ESID;
	} else {
		/*soundstream runs on movie/sprite timeline*/
		esd->OCRESID = read->bifs_es->ESID;
	}
	gf_list_add(od->ESDescriptors, esd);

	/*setup mux info*/
	mux = (GF_MuxInfo*)gf_odf_desc_new(GF_ODF_MUXINFO_TAG);
	mux->file_name = strdup(snd->szFileName);
	mux->startTime = snd->frame_delay_ms;
	/*MP3 in, destroy file once done*/
	if (snd->format==2) mux->delete_file = 1;
	gf_list_add(esd->extensionDescriptors, mux);


	/*by default insert OD at begining*/
	e = swf_insert_od(read, 0, od);
	if (e) {
		gf_odf_desc_del((GF_Descriptor *) od);
		return e;
	}
	/*create sound & audio clip*/
	n = SWF_NewNode(read, TAG_MPEG4_Sound2D);
	gf_node_insert_child(read->root, n, 0);
	gf_node_register(n, read->root);
	par = n;
	n = SWF_NewNode(read, TAG_MPEG4_AudioClip);
	/*soundStream doesn't have ID and doesn't need to be accessed*/
	if (snd->ID) {
		sprintf(szDEF, "Sound%d", snd->ID);
		read->load->ctx->max_node_id++;
		ID = read->load->ctx->max_node_id;
		gf_node_set_id(n, ID, szDEF);
	}
	((M_Sound2D *)par)->source = n;
	gf_node_register(n, par);
	/*assign URL*/
	gf_node_get_field_by_name(n, "url", &info);
	gf_sg_vrml_mf_alloc(info.far_ptr, info.fieldType, 1);
	((MFURL *)info.far_ptr)->vals[0].OD_ID = od->objectDescriptorID;

	snd->is_setup = 1;
	return GF_OK;
}

GF_Err swf_start_sound(SWFReader *read)
{
	char szDEF[100];
	GF_Command *com;
	GF_FieldInfo info;
	GF_Node *sound2D;
	GF_CommandField *f;
	SWFSound *snd;
	u32 ID = swf_get_16(read);
	SoundInfo si = swf_skip_soundinfo(read);

	snd = sndswf_get_sound(read, ID);
	if (!snd) {
		swf_report(read, GF_BAD_PARAM, "Cannot find sound with ID %d", ID);
		return GF_OK;
	}
	if (!snd->is_setup) {
		GF_Err e = swf_setup_sound(read, snd);
		if (e) return e;
	}

	sprintf(szDEF, "Sound%d", snd->ID);
	sound2D = gf_sg_find_node_by_name(read->load->scene_graph, szDEF);
	/*check flags*/
	if (si.sync_flags & 0x2) {
		/*need a STOP*/
		com = gf_sg_command_new(read->load->scene_graph, GF_SG_FIELD_REPLACE);
		com->node = sound2D;
		gf_node_register(com->node, NULL);
		gf_node_get_field_by_name(sound2D, "stopTime", &info);
		f = gf_sg_command_field_new(com);
		f->field_ptr = gf_sg_vrml_field_pointer_new(GF_SG_VRML_SFTIME);
		f->fieldType = GF_SG_VRML_SFTIME;
		f->fieldIndex = info.fieldIndex;
		/*replace by "now"*/
		*(SFTime *)f->field_ptr = ((Double)(s64) read->bifs_au->timing) / read->bifs_es->timeScale;
		*(SFTime *)f->field_ptr = 0;
		gf_list_add(read->bifs_au->commands, com);
	}

	com = gf_sg_command_new(read->load->scene_graph, GF_SG_FIELD_REPLACE);
	com->node = sound2D;
	gf_node_register(com->node, NULL);
	gf_node_get_field_by_name(sound2D, "startTime", &info);
	f = gf_sg_command_field_new(com);
	f->field_ptr = gf_sg_vrml_field_pointer_new(GF_SG_VRML_SFTIME);
	f->fieldType = GF_SG_VRML_SFTIME;
	f->fieldIndex = info.fieldIndex;
	/*replace by "now"*/
	*(SFTime *)f->field_ptr = ((Double)(s64) read->bifs_au->timing) / read->bifs_es->timeScale;
	*(SFTime *)f->field_ptr = 0;
	gf_list_add(read->bifs_au->commands, com);

	return GF_OK;
}

GF_Err swf_soundstream_hdr(SWFReader *read)
{
	u8 rec_mix;
	u32 samplesperframe;
	SWFSound *snd;

	if (read->sound_stream) {
		swf_report(read, GF_BAD_PARAM, "More than one sound stream for current timeline!!");
		return swf_func_skip(read);
	}
	
	snd = malloc(sizeof(SWFSound));
	memset(snd, 0, sizeof(SWFSound));

	rec_mix = swf_read_int(read, 8);
	/*0: uncompressed, 1: ADPCM, 2: MP3*/
	snd->format = swf_read_int(read, 4);
	/*0: 5.5k, 1: 11k, 2: 2: 22k, 3: 44k*/
	snd->sound_rate = swf_read_int(read, 2);
	/*0: 8 bit, 1: 16 bit*/
	snd->bits_per_sample = swf_read_int(read, 1) ? 16 : 8;
	/*0: mono, 8 1: stereo*/
	snd->stereo = swf_read_int(read, 1);
	/*samplesperframe hint*/
	samplesperframe = swf_read_int(read, 16);

	switch (snd->format) {
	/*raw PCM*/
	case 0:
		swf_report(read, GF_NOT_SUPPORTED, "Raw PCM Audio not supported");
		free(snd);
		break;
	/*ADPCM*/
	case 1:
		swf_report(read, GF_NOT_SUPPORTED, "AD-PCM Audio not supported");
		free(snd);
		break;
	/*MP3*/
	case 2:
		read->sound_stream = snd;
		break;
	case 3:
		swf_report(read, GF_NOT_SUPPORTED, "Unrecognized sound format");
		free(snd);
		break;
	}
	return GF_OK;
}

GF_Err swf_soundstream_block(SWFReader *read)
{
	unsigned char bytes[4];
	u32 hdr, alloc_size, size, tot_size, samplesPerFrame, delay;
	char *frame;

	/*note we're doing only MP3*/
	if (!read->sound_stream) return swf_func_skip(read);

	samplesPerFrame = swf_get_16(read);
	delay = swf_get_16(read);

	if (!read->sound_stream->is_setup) {
		if (!read->sound_stream->szFileName) {
			char szName[1024];
			sprintf(szName, "swf_soundstream_%d.mp3", (u32) read->sound_stream);
			if (read->localPath) {
				read->sound_stream->szFileName = malloc(sizeof(char)*GF_MAX_PATH);
				strcpy(read->sound_stream->szFileName, read->localPath);
				strcat(read->sound_stream->szFileName, szName);
			} else {
				read->sound_stream->szFileName = strdup(szName);
			}
			read->sound_stream->output = fopen(read->sound_stream->szFileName, "wb");
		}
		/*error at setup*/
		if (!read->sound_stream->output) return swf_func_skip(read);
		/*store TS of first AU*/
		read->sound_stream->frame_delay_ms = read->current_frame*1000;
		read->sound_stream->frame_delay_ms /= read->frame_rate;
		read->sound_stream->frame_delay_ms = delay;
		swf_setup_sound(read, read->sound_stream);
	}

	if (!samplesPerFrame) return GF_OK;

	alloc_size = 1;
	frame = malloc(sizeof(char));
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
			frame = realloc(frame, sizeof(char)*(size-4));
			alloc_size = size-4;
		}
		/*watchout for truncated framesif */
		if (tot_size + size >= read->size) size = read->size - tot_size;

		swf_read_data(read, frame, size-4);
		fwrite(bytes, sizeof(char)*4, 1, read->sound_stream->output);
		fwrite(frame, sizeof(char)*(size-4), 1, read->sound_stream->output);
		if (tot_size + size >= read->size) break;
		tot_size += size;
	}
	free(frame);
	return GF_OK;
}

GF_Err swf_process_tag(SWFReader *read)
{
	switch (read->tag) {
	case SWF_END: return GF_OK; /*void*/
	case SWF_PROTECT: return GF_OK;
	case SWF_SETBACKGROUNDCOLOR: return swf_set_backcol(read);
	case SWF_DEFINESHAPE: return swf_def_shape(read, 0);
	case SWF_DEFINESHAPE2: return swf_def_shape(read, 1);
	case SWF_DEFINESHAPE3: return swf_def_shape(read, 2);
	case SWF_PLACEOBJECT: return swf_place_obj(read, 0);
	case SWF_PLACEOBJECT2: return swf_place_obj(read, 1);
	case SWF_SHOWFRAME: return swf_show_frame(read);
	case SWF_REMOVEOBJECT: return swf_remove_obj(read, 0);
	case SWF_REMOVEOBJECT2: return swf_remove_obj(read, 1);
	case SWF_DEFINEFONT: return swf_def_font(read, 0);
	case SWF_DEFINEFONT2: return swf_def_font(read, 1);
	case SWF_DEFINEFONTINFO: return swf_def_font_info(read);
	case SWF_DEFINETEXT: return swf_def_text(read, 0);
	case SWF_DEFINETEXT2: return swf_def_text(read, 1);
	case SWF_DEFINESPRITE: return swf_def_sprite(read);
	/*no revision needed*/
	case SWF_SOUNDSTREAMHEAD: 
	case SWF_SOUNDSTREAMHEAD2:
		return swf_soundstream_hdr(read);
	case SWF_DEFINESOUND: return swf_def_sound(read);
	case SWF_STARTSOUND: return swf_start_sound(read);
	case SWF_SOUNDSTREAMBLOCK: return swf_soundstream_block(read);

	case SWF_DEFINEBUTTON: return swf_def_button(read, 0);
	case SWF_DEFINEBUTTON2: return swf_def_button(read, 1);
	case SWF_DEFINEBUTTONSOUND:
	case SWF_DOACTION:
		read->has_interact = 1;
		swf_report(read, GF_OK, "skipping tag %s", swf_get_tag(read->tag) );
		return swf_func_skip(read);

/*	case SWF_DEFINEBITSJPEG: return swf_def_bits_jpeg(read);
	case SWF_JPEGTABLES: return swf_def_hdr_jpeg(read);
	case SWF_SOUNDSTREAMHEAD: return swf_sound_hdr(read);
	case SWF_SOUNDSTREAMBLOCK: return swf_sound_block(read);
	case SWF_DEFINEBITSLOSSLESS: return swf_def_bits_lossless(read);
	case SWF_DEFINEBITSJPEG2: return swf_def_bits_jpegV2(read);
	case SWF_DEFINEBITSJPEG3: return swf_def_bits_jpegV3(read);
	case SWF_DEFINEBITSLOSSLESS2: return swf_def_bits_losslessV2(read);
	case SWF_FRAMELABEL: return swf_def_frame_label(read);
*/	default: return swf_unknown_tag(read);
	}
}

GF_Err SWF_ParseTag(SWFReader *read)
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
		swf_report(read, GF_IO_ERR, "tag over-read of %d bytes (size %d)", -1*diff, read->size);
		return GF_IO_ERR;
	} else {
		swf_read_int(read, diff*8);
	}


	if (!e && !read->tag) return GF_EOS;
	if (read->ioerr) {
		swf_report(read, GF_IO_ERR, "bitstream IO err (tag size %d)", read->size);
		return read->ioerr;
	}
	return e;
}


void swf_report(SWFReader *read, GF_Err e, char *format, ...)
{
#ifndef GPAC_DISABLE_LOG
	if (gf_log_level && (gf_log_tools & GF_LOG_PARSER)) {
		char szMsg[2048];
		va_list args;
		va_start(args, format);
		vsprintf(szMsg, format, args);
		va_end(args);
		GF_LOG((u32) (e ? GF_LOG_ERROR : GF_LOG_WARNING), GF_LOG_PARSER, ("[SWF Parsing] %s (frame %d)\n", szMsg, read->current_frame+1) );
	}
#endif
}



const char *swf_get_tag(u32 tag)
{
	switch (tag) {
	case SWF_END: return "End";
	case SWF_SHOWFRAME: return "ShowFrame";
	case SWF_DEFINESHAPE: return "DefineShape";
	case SWF_FREECHARACTER: return "FreeCharacter";
	case SWF_PLACEOBJECT: return "PlaceObject";
	case SWF_REMOVEOBJECT: return "RemoveObject";
	case SWF_DEFINEBITSJPEG: return "DefineBitsJPEG";
	case SWF_DEFINEBUTTON: return "DefineButton";
	case SWF_JPEGTABLES: return "JPEGTables";
	case SWF_SETBACKGROUNDCOLOR: return "SetBackgroundColor";
	case SWF_DEFINEFONT: return "DefineFont";
	case SWF_DEFINETEXT: return "DefineText";
	case SWF_DOACTION: return "DoAction";
	case SWF_DEFINEFONTINFO: return "DefineFontInfo";
	case SWF_DEFINESOUND: return "DefineSound";
	case SWF_STARTSOUND: return "StartSound";
	case SWF_DEFINEBUTTONSOUND: return "DefineButtonSound";
	case SWF_SOUNDSTREAMHEAD: return "SoundStreamHead";
	case SWF_SOUNDSTREAMBLOCK: return "SoundStreamBlock";
	case SWF_DEFINEBITSLOSSLESS: return "DefineBitsLossless";
	case SWF_DEFINEBITSJPEG2: return "DefineBitsJPEG2";
	case SWF_DEFINESHAPE2: return "DefineShape2";
	case SWF_DEFINEBUTTONCXFORM: return "DefineButtonCXForm";
	case SWF_PROTECT: return "Protect";
	case SWF_PLACEOBJECT2: return "PlaceObject2";
	case SWF_REMOVEOBJECT2: return "RemoveObject2";
	case SWF_DEFINESHAPE3: return "DefineShape3";
	case SWF_DEFINETEXT2: return "DefineText2";
	case SWF_DEFINEBUTTON2: return "DefineButton2";
	case SWF_DEFINEBITSJPEG3: return "DefineBitsJPEG3";
	case SWF_DEFINEBITSLOSSLESS2: return "DefineBitsLossless2";
	case SWF_DEFINEEDITTEXT: return "DefineEditText";
	case SWF_DEFINEMOVIE: return "DefineMovie";
	case SWF_DEFINESPRITE: return "DefineSprite";
	case SWF_NAMECHARACTER: return "NameCharacter";
	case SWF_SERIALNUMBER: return "SerialNumber";
	case SWF_GENERATORTEXT: return "GeneratorText";
	case SWF_FRAMELABEL: return "FrameLabel";
	case SWF_SOUNDSTREAMHEAD2: return "SoundStreamHead2";
	case SWF_DEFINEMORPHSHAPE: return "DefineMorphShape";
	case SWF_DEFINEFONT2: return "DefineFont2";
	case SWF_TEMPLATECOMMAND: return "TemplateCommand";
	case SWF_GENERATOR3: return "Generator3";
	case SWF_EXTERNALFONT: return "ExternalFont";
	case SWF_EXPORTASSETS: return "ExportAssets";
	case SWF_IMPORTASSETS: return "ImportAssets";
	case SWF_ENABLEDEBUGGER: return "EnableDebugger";
	case SWF_MX0: return "MX0";
	case SWF_MX1: return "MX1";
	case SWF_MX2: return "MX2";
	case SWF_MX3: return "MX3";
	case SWF_MX4: return "MX4";
	default: return "UnknownTag";
	}
}



/*defines internal structure of the scene*/
GF_Err SWF_InitContext(SWFReader *read)
{
	GF_Err e;
	char szMsg[1000];
	GF_ObjectDescriptor *od;
	GF_ESD *esd;
	u32 ID;
	GF_FieldInfo info;
	GF_StreamContext *prev_sc;
	GF_Command *com;
	GF_Node *n, *n2;

	/*create BIFS stream*/
	read->bifs_es = gf_sm_stream_new(read->load->ctx, 1, GF_STREAM_SCENE, 0x01);
	read->bifs_es->timeScale = read->frame_rate*100;

	read->bifs_au = gf_sm_stream_au_new(read->bifs_es, 0, 0.0, 1);
	/*create scene replace command*/
	com = gf_sg_command_new(read->load->scene_graph, GF_SG_SCENE_REPLACE);
	read->load->ctx->scene_width = FIX2INT(read->width);
	read->load->ctx->scene_height = FIX2INT(read->height);
	read->load->ctx->is_pixel_metrics = 1;

	gf_list_add(read->bifs_au->commands, com);
	read->load->scene_graph = read->load->scene_graph;

	/*create base tree*/
	com->node = read->root = SWF_NewNode(read, TAG_MPEG4_OrderedGroup);
	gf_node_register(read->root, NULL);

	/*hehehe*/
	n = SWF_NewNode(read, TAG_MPEG4_WorldInfo);
	gf_node_insert_child(read->root, n, -1);
	gf_node_register(n, read->root);
	((M_WorldInfo *)n)->title.buffer = strdup("GPAC SWF CONVERTION DISCLAIMER");
	gf_sg_vrml_mf_alloc( & ((M_WorldInfo *)n)->info, GF_SG_VRML_MFSTRING, 3);

	sprintf(szMsg, "%s file converted to MPEG-4 Systems", read->load->fileName);
	((M_WorldInfo *)n)->info.vals[0] = strdup(szMsg);
	((M_WorldInfo *)n)->info.vals[1] = strdup("Conversion done using GPAC version " GPAC_VERSION " - (C) 2000-2005 GPAC");
	((M_WorldInfo *)n)->info.vals[2] = strdup("Macromedia SWF to MPEG-4 Conversion mapping released under GPL license");

	/*background*/
	n = SWF_NewNode(read, TAG_MPEG4_Background2D);
	gf_node_set_id(n, 1, "BACKGROUND");
	gf_node_insert_child(read->root, n, -1);
	gf_node_register(n, read->root);
	
	/*dictionary*/
	n = SWF_NewNode(read, TAG_MPEG4_Switch);
	gf_node_set_id(n, 2, "DICTIONARY");
	gf_node_insert_child(read->root, n, -1);
	gf_node_register(n, read->root);
	/*empty shape to fill depth levels & sprites roots*/
	n2 = SWF_NewNode(read, TAG_MPEG4_Shape);
	gf_node_set_id(n2, 3, "EMPTYSHAPE");
	gf_list_add( ((M_Switch *)n)->choice, n2);
	gf_node_register(n2, n);

	/*display list*/
	n = SWF_NewNode(read, TAG_MPEG4_Transform2D);
	gf_node_set_id(n, 4, "DISPLAYLIST");
	gf_node_insert_child(read->root, n, -1);
	gf_node_register(n, read->root);
	/*update w/h transform*/
	((M_Transform2D *)n)->scale.y = -FIX_ONE;
	((M_Transform2D *)n)->translation.x = -read->width/2;
	((M_Transform2D *)n)->translation.y = read->height/2;

	read->load->ctx->max_node_id = 5;

	/*always reserve OD_ID=1 for main ctrl stream if any*/
	read->prev_od_id = 1;
	/*always reserve ES_ID=2 for OD stream, 3 for main ctrl stream if any*/
	read->prev_es_id = 3;

	/*no control stream*/
	if (!(read->flags & GF_SM_SWF_SPLIT_TIMELINE)) return GF_OK;

	/*init OD*/
	e = swf_init_od(read);
	if (e) return e;

	/*create animationStream object*/
	od = (GF_ObjectDescriptor *) gf_odf_desc_new(GF_ODF_OD_TAG);
	if (!od) return GF_OUT_OF_MEM;
	od->objectDescriptorID = 1;
	esd = (GF_ESD *) gf_odf_desc_esd_new(0);
	if (!esd) return GF_OUT_OF_MEM;
	esd->ESID = esd->OCRESID = 3;
	esd->dependsOnESID = 1;
	esd->decoderConfig->streamType = GF_STREAM_SCENE;
	esd->decoderConfig->objectTypeIndication = 1;
	esd->slConfig->timestampResolution = read->bifs_es->timeScale;
	gf_odf_desc_del((GF_Descriptor *) esd->decoderConfig->decoderSpecificInfo);
	esd->decoderConfig->decoderSpecificInfo = NULL;
	gf_list_add(od->ESDescriptors, esd);
	e = swf_insert_od(read, 0, od);
	if (e) {
		gf_odf_desc_del((GF_Descriptor *) od);
		return e;
	}

	/*setup a new BIFS context*/
	prev_sc = read->bifs_es;
	read->bifs_es = gf_sm_stream_new(read->load->ctx, esd->ESID, GF_STREAM_SCENE, 1);
	read->bifs_es->timeScale = prev_sc->timeScale;
	/*create first AU*/
	read->bifs_au = gf_sm_stream_au_new(read->bifs_es, 0, 0, 1);

	if (read->flags & GF_SM_SWF_NO_ANIM_STREAM) return GF_OK;

	/*setup the animationStream node*/
	n = SWF_NewNode(read, TAG_MPEG4_AnimationStream);
	read->load->ctx->max_node_id++;
	ID = read->load->ctx->max_node_id;
	gf_node_set_id(n, ID, "MovieControl");

	gf_node_insert_child(read->root, n, 0);
	gf_node_register(n, read->root);
	/*assign URL*/
	gf_node_get_field_by_name(n, "url", &info);
	gf_sg_vrml_mf_alloc(info.far_ptr, info.fieldType, 1);
	((MFURL*)info.far_ptr)->vals[0].OD_ID = 1;
	/*run from start*/
	((M_AnimationStream *)n)->startTime = 0;
	/*and always loop*/
	((M_AnimationStream *)n)->loop = 1;

	return GF_OK;
}


void SWF_IOErr(void *par)
{
	SWFReader *read = (SWFReader *)par;
	read->ioerr = GF_IO_ERR;
}

GF_Err gf_sm_load_run_SWF(GF_SceneLoader *load)
{
	GF_Err e;
	SWFReader *read = (SWFReader *)load->loader_priv;
	if (!read) return GF_BAD_PARAM;

	/*parse all tags*/
	e = GF_OK;
	while (e == GF_OK) e = SWF_ParseTag(read);
	gf_set_progress("SWF Parsing", read->length, read->length);

	if (e==GF_EOS) e = GF_OK;
	if (!e) {
		if (read->flat_limit != 0) 
			swf_report(read, GF_OK, "%d points removed while parsing shapes (Flattening limit %.4f)", read->flatten_points, read->flat_limit);

		if (read->has_interact) swf_report(read, GF_OK, "ActionScripts and interactions are not supported and have been removed");
	}
	return e;
}

void gf_sm_load_done_SWF(GF_SceneLoader *load)
{
	SWFReader *read = (SWFReader *) load->loader_priv;
	if (!read) return;

	if (read->compressed) swf_done_decompress(read);
	gf_bs_del(read->bs);
	while (gf_list_count(read->display_list)) {
		DispShape *s = gf_list_get(read->display_list, 0);
		gf_list_rem(read->display_list, 0);
		free(s);
	}
	gf_list_del(read->display_list);
	while (gf_list_count(read->fonts)) {
		SWFFont *ft = gf_list_get(read->fonts, 0);
		gf_list_rem(read->fonts, 0);
		if (ft->glyph_adv) free(ft->glyph_adv);
		if (ft->glyph_codes) free(ft->glyph_codes);
		if (ft->fontName) free(ft->fontName);
		while (gf_list_count(ft->glyphs)) {
			GF_Node *gl = gf_list_get(ft->glyphs, 0);
			gf_list_rem(ft->glyphs, 0);
			gf_node_unregister(gl, NULL);
		}
		gf_list_del(ft->glyphs);
		free(ft);
	}
	gf_list_del(read->fonts);
	gf_list_del(read->apps);

	while (gf_list_count(read->sounds)) {
		SWFSound *snd = gf_list_get(read->sounds, 0);
		gf_list_rem(read->sounds, 0);
		if (snd->output) fclose(snd->output);
		if (snd->szFileName) free(snd->szFileName);
		free(snd);
	}
	gf_list_del(read->sounds);
	swf_delete_sound_stream(read);
	if (read->localPath) free(read->localPath);
	fclose(read->input);
	free(read);
	load->loader_priv = NULL;
}

GF_Err gf_sm_load_init_SWF(GF_SceneLoader *load)
{
	SWFReader *read;
	SWFRec rc;
	GF_Err e;
	FILE *input;

	if (!load->ctx || !load->scene_graph || !load->fileName) return GF_BAD_PARAM;
	input = fopen(load->fileName, "rb");
	if (!input) return GF_URL_ERROR;

	GF_SAFEALLOC(read, sizeof(SWFReader));
	read->load = load;
	
	e = GF_OK;

	read->input = input;
	read->bs = gf_bs_from_file(input, GF_BITSTREAM_READ);
	gf_bs_set_eos_callback(read->bs, SWF_IOErr, &read);
	read->display_list = gf_list_new();
	read->fonts = gf_list_new();
	read->apps = gf_list_new();
	read->sounds = gf_list_new();

	read->flags = load->swf_import_flags;
	read->flat_limit = FLT2FIX(load->swf_flatten_limit);
	if (load->localPath) read->localPath = strdup(load->localPath);
	else {
		char *c;
		read->localPath = strdup(load->fileName);
		c = strrchr(read->localPath, GF_PATH_SEPARATOR);
		if (c) c[1] = 0;
		else {
			free(read->localPath);
			read->localPath = NULL;
		}
	}

	load->loader_priv = read;

	/*get signature*/
	read->sig[0] = gf_bs_read_u8(read->bs);
	read->sig[1] = gf_bs_read_u8(read->bs);
	read->sig[2] = gf_bs_read_u8(read->bs);
	/*"FWS" or "CWS"*/
	if ( ((read->sig[0] != 'F') && (read->sig[0] != 'C')) || (read->sig[1] != 'W') || (read->sig[2] != 'S') ) {
		e = GF_URL_ERROR;
		goto exit;
	}
	read->version = gf_bs_read_u8(read->bs);
	read->length = swf_get_32(read);

	/*if compressed decompress the whole file*/
	swf_init_decompress(read);
	
	swf_get_rec(read, &rc);
	read->width = rc.w;
	read->height = rc.h;
	load->ctx->scene_width = FIX2INT(read->width);
	load->ctx->scene_height = FIX2INT(read->height);
	load->ctx->is_pixel_metrics = 1;
	
	swf_align(read);
	read->frame_rate = swf_get_16(read)>>8;
	read->frame_count = swf_get_16(read);
	
	GF_LOG(GF_LOG_INFO, GF_LOG_PARSER, ("SWF Import - Scene Size %dx%d - %d frames @ %d FPS", load->ctx->scene_width, load->ctx->scene_height, read->frame_count, read->frame_rate));

	/*init scene*/
	if (read->flags & GF_SM_SWF_SPLIT_TIMELINE) read->flags |= GF_SM_SWF_STATIC_DICT;
	
	e = SWF_InitContext(read);

	/*parse all tags*/
	while (e == GF_OK) {
		e = SWF_ParseTag(read);
		if (read->current_frame==1) break;
	}
	if (e==GF_EOS) e = GF_OK;

exit:
	if (e) gf_sm_load_done_SWF(load);
	return e;
}

#endif


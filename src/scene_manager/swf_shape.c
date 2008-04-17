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
#include <gpac/utf.h>
#include <gpac/internal/swf_dev.h>

#ifndef GPAC_READ_ONLY

SFColor get_bifs_col(u32 ARGB)
{
	SFColor val;
	val.red = INT2FIX((ARGB>>16)&0xFF) / 255;
	val.green = INT2FIX((ARGB>>8)&0xFF) / 255;
	val.blue = INT2FIX((ARGB)&0xFF) / 255;
	return val;
}
Fixed get_bifs_alpha(u32 ARGB)
{
	return INT2FIX((ARGB>>24)&0xFF) / 255;
}

void SWF_InsertAppearance(SWFReader *read, GF_Node *app)
{
	M_Shape *s = (M_Shape *) SWF_NewNode(read, TAG_MPEG4_Shape);
	s->appearance = app;
	gf_node_register(app, (GF_Node *) s);

	swf_insert_symbol(read, (GF_Node *)s);
}

Bool col_equal(SFColor c1, SFColor c2)
{
	if (c1.red != c2.red) return 0;
	if (c1.green != c2.green) return 0;
	if (c1.blue != c2.blue) return 0;
	return 1;
}

GF_Node *SWF_GetAppearance(SWFReader *read, GF_Node *parent, u32 fill_col, Fixed line_width, u32 l_col)
{
	char szDEF[1024];
	u32 ID, i;
	SFColor fc, lc;
	Fixed fill_transp, line_transp;
	M_Appearance *app;
	M_Material2D *mat;

	fc = get_bifs_col(fill_col);
	fill_transp = FIX_ONE - get_bifs_alpha(fill_col);
	if (fill_transp<0) fill_transp=0;
	lc = get_bifs_col(l_col);
	line_transp = FIX_ONE - get_bifs_alpha(l_col);
	if (line_transp<0) line_transp=0;

	i=0;
	while ((app = (M_Appearance*)gf_list_enum(read->apps, &i))) {
		mat = (M_Material2D *)app->material;
		if (!line_width) {
			if (mat->lineProps || !mat->filled) continue;
		} else {
			if (!mat->lineProps) continue;
			if (!col_equal(((M_LineProperties *)mat->lineProps)->lineColor, lc)) continue;
			if (((M_LineProperties *)mat->lineProps)->width != line_width) continue;
		}
		if (!mat->filled && fill_col) continue;
		if (mat->filled) {
			if (!fill_col) continue;
			if (mat->transparency != fill_transp) continue;
			if (!col_equal(mat->emissiveColor, fc)) continue;
		}
		/*OK same appearance let's go*/
		gf_node_register((GF_Node *)app, parent);
		return (GF_Node *)app;
	}

	app = (M_Appearance *) SWF_NewNode(read, TAG_MPEG4_Appearance);
	app->material = SWF_NewNode(read, TAG_MPEG4_Material2D);
	gf_node_register(app->material, (GF_Node *)app);
	((M_Material2D *)app->material)->filled = 0;

	if (fill_col) {
		((M_Material2D *)app->material)->filled = 1;
		((M_Material2D *)app->material)->emissiveColor = fc;
		((M_Material2D *)app->material)->transparency = fill_transp;
	}
	if (line_width && l_col) {
		if (read->flags & GF_SM_SWF_SCALABLE_LINE) {
			M_XLineProperties *lp = (M_XLineProperties *) SWF_NewNode(read, TAG_MPEG4_XLineProperties);
			((M_Material2D *)app->material)->lineProps = (GF_Node *) lp;
			lp->width = line_width;
			lp->lineColor = lc;
			lp->isScalable = 1;
			lp->transparency = line_transp;
			gf_node_register((GF_Node *)lp, app->material);
		} else {
			M_LineProperties *lp = (M_LineProperties *) SWF_NewNode(read, TAG_MPEG4_LineProperties);
			((M_Material2D *)app->material)->lineProps = (GF_Node *) lp;
			lp->width = line_width;
			lp->lineColor = lc;
			gf_node_register((GF_Node *)lp, app->material);
		}
	}

	gf_node_register((GF_Node *)app, parent);

	if (read->load->swf_import_flags & GF_SM_SWF_REUSE_APPEARANCE) {
		sprintf(szDEF, "FILLAPP_%d", gf_list_count(read->apps));
		read->load->ctx->max_node_id++;
		ID = read->load->ctx->max_node_id;

		gf_node_set_id((GF_Node *)app, ID, szDEF);
		SWF_InsertAppearance(read, (GF_Node *)app);
		gf_list_add(read->apps, app);
	}
	return (GF_Node *) app;
}


static GF_Rect SWF_GetCenteredBounds(SWFShape *shape, SWFShapeRec *srec)
{
	GF_Rect rc;
#if 1
	u32 i;
	Fixed xm, ym, xM, yM;
	xM = yM = FIX_MIN;
	xm = ym = FIX_MAX;

	for (i=0; i<srec->path->nbPts; i++) {
		if (srec->path->pts[i].x<=xm) xm = srec->path->pts[i].x;
		if (srec->path->pts[i].x>=xM) xM = srec->path->pts[i].x;
		if (srec->path->pts[i].y<=ym) ym = srec->path->pts[i].y;
		if (srec->path->pts[i].y>=yM) yM = srec->path->pts[i].y;
	}
	rc.width = xM-xm;
	rc.height = yM-ym;
	rc.x = xm;
	rc.y = yM;
#else
	rc.width = shape->rc.w;
	rc.height = shape->rc.h;
	rc.x = shape->rc.x;
	rc.y = shape->rc.y + rc.height;
#endif
	return rc;
}

static GF_Node *SWF_GetGradient(SWFReader *read, GF_Node *parent, SWFShape *shape, SWFShapeRec *srec)
{
	Bool is_radial, has_alpha;
	GF_Rect rc;
	GF_Matrix2D mx;
	u32 i;
	MFFloat *keys;
	MFColor *values;
	GF_FieldInfo info;
	M_Appearance *app = (M_Appearance *) SWF_NewNode(read, TAG_MPEG4_Appearance);
	gf_node_register((GF_Node *)app, parent);
	app->material = SWF_NewNode(read, TAG_MPEG4_Material2D);
	gf_node_register(app->material, (GF_Node *)app);
	((M_Material2D *)app->material)->filled = 1;

	is_radial = (srec->type==0x12) ? 1 : 0;
	app->texture = SWF_NewNode(read, is_radial ? TAG_MPEG4_RadialGradient : TAG_MPEG4_LinearGradient);
	gf_node_register((GF_Node *) app->texture, (GF_Node *) app);

	/*set keys*/
	gf_node_get_field_by_name(app->texture, "key", &info);
	gf_sg_vrml_mf_alloc(info.far_ptr, info.fieldType, srec->nbGrad);
	keys = (MFFloat *)info.far_ptr;
	for (i=0; i<srec->nbGrad; i++) {
		keys->vals[i] = srec->grad_ratio[i];
		keys->vals[i] /= 255;
	}

	/*set colors*/
	gf_node_get_field_by_name(app->texture, "keyValue", &info);
	gf_sg_vrml_mf_alloc(info.far_ptr, info.fieldType, srec->nbGrad);
	values = (MFColor *)info.far_ptr;
	has_alpha = 0;
	for (i=0; i<srec->nbGrad; i++) {
		values->vals[i] = get_bifs_col(srec->grad_col[i]);
		if (get_bifs_alpha(srec->grad_col[i]) != FIX_ONE) has_alpha = 1;
	}
	/*set opacity*/
	if (has_alpha) {
		gf_node_get_field_by_name(app->texture, "opacity", &info);
		gf_sg_vrml_mf_alloc(info.far_ptr, info.fieldType, srec->nbGrad);
		keys = (MFFloat *)info.far_ptr;
		for (i=0; i<srec->nbGrad; i++) {
			keys->vals[i] = get_bifs_alpha(srec->grad_col[i]);
		}
		/*and remove material !!*/
		((M_Material2D *)app->material)->filled = 0;
		((M_Material2D *)app->material)->lineProps = SWF_NewNode(read, TAG_MPEG4_LineProperties);;
		((M_LineProperties *)((M_Material2D *)app->material)->lineProps)->width = 0;
		gf_node_register(((M_Material2D *)app->material)->lineProps, app->material);
	}

	rc = SWF_GetCenteredBounds(shape, srec);

	gf_mx2d_init(mx);
	mx.m[0] = 1/rc.width;
	mx.m[2] = - rc.x/rc.width;
	mx.m[4] = 1/rc.height;
	mx.m[5] = 1 - rc.y/rc.height;

	gf_mx2d_pre_multiply(&mx, &srec->mat);

	/*define gradient in SWF pixel coordinates*/
	if (is_radial ) {
		gf_node_get_field_by_name(app->texture, "center", &info);
		((SFVec2f*)info.far_ptr)->x = 0;
		((SFVec2f*)info.far_ptr)->y = 0;

		gf_node_get_field_by_name(app->texture, "radius", &info);
		*((SFFloat*)info.far_ptr) = FLT2FIX(819.20);
	} else {
		gf_node_get_field_by_name(app->texture, "startPoint", &info);
		((SFVec2f*)info.far_ptr)->x = FLT2FIX(-819.20);

		gf_node_get_field_by_name(app->texture, "endPoint", &info);
		((SFVec2f*)info.far_ptr)->x = FLT2FIX(819.20);
	}

	/*matrix from local coordinates to texture coordiantes (Y-flip for BIFS texture coordinates)*/
	gf_mx2d_init(mx);
	mx.m[0] = 1/rc.width;
	mx.m[2] = - rc.x/rc.width;
	mx.m[4] = 1/rc.height;
	mx.m[5] = 1 - rc.y/rc.height;
	/*pre-multiply SWF->local coords matrix*/
	gf_mx2d_pre_multiply(&mx, &srec->mat);

	gf_node_get_field_by_name(app->texture, "transform", &info);
	*((GF_Node **)info.far_ptr) = SWF_GetBIFSMatrix(read, &mx);
	gf_node_register(*((GF_Node **)info.far_ptr), app->texture);
	return (GF_Node *) app;
}

static GF_Node *SWF_GetBitmap(SWFReader *read, GF_Node *parent, SWFShape *shape, SWFShapeRec *srec)
{
	GF_Matrix2D mx;
	GF_Node *bmp;
	GF_FieldInfo info;
	M_Appearance *app;
	char szDEF[100];

	sprintf(szDEF, "Bitmap%d", srec->img_id);
	bmp = gf_sg_find_node_by_name(read->load->scene_graph, szDEF);
	if (!bmp) return NULL;
	app = (M_Appearance *) SWF_NewNode(read, TAG_MPEG4_Appearance);
	gf_node_register((GF_Node *)app, parent);
	app->material = SWF_NewNode(read, TAG_MPEG4_Material2D);
	gf_node_register(app->material, (GF_Node *)app);
	((M_Material2D *)app->material)->filled = 1;

	app->texture = bmp;
	gf_node_register(bmp, (GF_Node *)app);

	gf_mx2d_copy(mx, srec->mat);
	mx.m[0] *= SWF_TWIP_SCALE;
	mx.m[4] *= SWF_TWIP_SCALE;
	gf_mx2d_add_scale(&mx, FIX_ONE, -FIX_ONE);

	gf_node_get_field_by_name((GF_Node*)app, "textureTransform", &info);
	*((GF_Node **)info.far_ptr) = SWF_GetBIFSMatrix(read, &mx);
	gf_node_register(*((GF_Node **)info.far_ptr), (GF_Node*)app);
	return (GF_Node *) app;
}

void SWFShape_SetAppearance(SWFReader *read, SWFShape *shape, M_Shape *n, SWFShapeRec *srec, Bool is_fill)
{
	/*get regular appearance reuse*/
	if (is_fill) {
		switch (srec->type) {
		/*solid/alpha fill*/
		case 0x00:
			n->appearance = SWF_GetAppearance(read, (GF_Node *) n, srec->solid_col, 0, 0);
			break;
		case 0x10:
		case 0x12:
			if (read->flags & GF_SM_SWF_NO_GRADIENT) {
				u32 col = srec->grad_col[srec->nbGrad/2];
				col |= 0xFF000000;
				n->appearance = SWF_GetAppearance(read, (GF_Node *) n, col, 0, 0);
			} else {
				n->appearance = SWF_GetGradient(read, (GF_Node *) n, shape, srec);
			}
			break;
		case 0x40:
		case 0x41:
		case 0x42:
		case 0x43:
			n->appearance = SWF_GetBitmap(read, (GF_Node *) n, shape, srec);
			break;
		default:
			swf_report(read, GF_NOT_SUPPORTED, "fill_style %x not supported", srec->type);
			break;
		}
	} else {
		n->appearance = SWF_GetAppearance(read, (GF_Node *) n, 0, srec->width, srec->solid_col);
	}
}

/*translate a flash sub shape with only one path (eg one looking style) to a BIFS Shape node*/
GF_Node *SWFShapeToCurve2D(SWFReader *read, SWFShape *shape, SWFShapeRec *srec, Bool is_fill)
{
	u32 pt_idx, i;
	Bool use_xcurve;
	void *fptr;
	SFVec2f ct1, ct2, ct, pt, move_orig;
	M_Curve2D *curve;
	M_Coordinate2D *points;
	M_Shape *n = (M_Shape *) SWF_NewNode(read, TAG_MPEG4_Shape);

	SWFShape_SetAppearance(read, shape, n, srec, is_fill);

	use_xcurve = (read->flags & GF_SM_SWF_QUAD_CURVE) ? 1 : 0;
	if (use_xcurve) {
		curve = (M_Curve2D *) SWF_NewNode(read, TAG_MPEG4_XCurve2D);
	} else {
		curve = (M_Curve2D *) SWF_NewNode(read, TAG_MPEG4_Curve2D);
	}
	points = (M_Coordinate2D *) SWF_NewNode(read, TAG_MPEG4_Coordinate2D);
	n->geometry = (GF_Node *) curve;
	gf_node_register((GF_Node *) curve, (GF_Node *)n);
	curve->point = (GF_Node *) points;
	gf_node_register((GF_Node *) points, (GF_Node *) curve);
	curve->fineness = FIX_ONE;

	assert(srec->path->nbType);

	pt_idx = 0;
	for (i=0; i<srec->path->nbType; i++) {
		switch (srec->path->types[i]) {
		/*moveTo*/
		case 0:
			/*first moveTo implicit in BIFS*/
			if (i) {
				gf_sg_vrml_mf_append(&curve->type, GF_SG_VRML_MFINT32, &fptr);
				*((SFInt32 *)fptr) = 0;
			}
			gf_sg_vrml_mf_append(&points->point, GF_SG_VRML_MFVEC2F, &fptr);
			((SFVec2f *)fptr)->x = srec->path->pts[pt_idx].x;
			((SFVec2f *)fptr)->y = srec->path->pts[pt_idx].y;
			move_orig = srec->path->pts[pt_idx];
			pt_idx++;
			break;
		/*lineTo*/
		case 1:
			gf_sg_vrml_mf_append(&curve->type, GF_SG_VRML_MFINT32, &fptr);
			*((SFInt32 *)fptr) = 1;
			gf_sg_vrml_mf_append(&points->point, GF_SG_VRML_MFVEC2F, &fptr);
			((SFVec2f *)fptr)->x = srec->path->pts[pt_idx].x;
			((SFVec2f *)fptr)->y = srec->path->pts[pt_idx].y;
			pt_idx++;
			break;
		/*curveTo*/
		case 2:
			/*XCurve2D has quad arcs*/
			if (use_xcurve) {
				gf_sg_vrml_mf_append(&curve->type, GF_SG_VRML_MFINT32, &fptr);
				*((SFInt32 *)fptr) = 7;
				gf_sg_vrml_mf_append(&points->point, GF_SG_VRML_MFVEC2F, &fptr);
				((SFVec2f *)fptr)->x = srec->path->pts[pt_idx].x;
				((SFVec2f *)fptr)->y = srec->path->pts[pt_idx].y;
				gf_sg_vrml_mf_append(&points->point, GF_SG_VRML_MFVEC2F, &fptr);
				((SFVec2f *)fptr)->x = srec->path->pts[pt_idx+1].x;
				((SFVec2f *)fptr)->y = srec->path->pts[pt_idx+1].y;
				pt_idx+=2;
			} else {
				gf_sg_vrml_mf_append(&curve->type, GF_SG_VRML_MFINT32, &fptr);
				*((SFInt32 *)fptr) = 2;
				/*recompute cubic from quad*/
				ct.x = srec->path->pts[pt_idx].x;
				ct.y = srec->path->pts[pt_idx].y;
				pt.x = srec->path->pts[pt_idx-1].x;
				pt.y = srec->path->pts[pt_idx-1].y;
				ct1.x = pt.x + 2*(ct.x - pt.x)/3;
				ct1.y = pt.y + 2*(ct.y - pt.y)/3;
				ct.x = srec->path->pts[pt_idx+1].x;
				ct.y = srec->path->pts[pt_idx+1].y;
				ct2.x = ct1.x + (ct.x - pt.x) / 3;
				ct2.y = ct1.y + (ct.y - pt.y) / 3;

				gf_sg_vrml_mf_append(&points->point, GF_SG_VRML_MFVEC2F, &fptr);
				((SFVec2f *)fptr)->x = ct1.x;
				((SFVec2f *)fptr)->y = ct1.y;
				gf_sg_vrml_mf_append(&points->point, GF_SG_VRML_MFVEC2F, &fptr);
				((SFVec2f *)fptr)->x = ct2.x;
				((SFVec2f *)fptr)->y = ct2.y;
				gf_sg_vrml_mf_append(&points->point, GF_SG_VRML_MFVEC2F, &fptr);
				((SFVec2f *)fptr)->x = ct.x;
				((SFVec2f *)fptr)->y = ct.y;
				pt_idx+=2;
			}
			break;
		}
	}
	return (GF_Node *) n;
}

void SWF_MergeCurve2D(M_Curve2D *s, M_Curve2D *tomerge)
{
	u32 i, pt_idx, j;
	SFVec2f pt;
	void *ptr;
	M_Coordinate2D *dest, *orig;
	dest = (M_Coordinate2D *) s->point;
	orig = (M_Coordinate2D *) tomerge->point;

	if (!tomerge->type.count) return;
	if (!orig->point.count) return;
	pt = orig->point.vals[0];

	if (s->type.vals[s->type.count - 1] == 0) {
		dest->point.vals[dest->point.count - 1] = pt;
	} else {
		gf_sg_vrml_mf_append(&s->type, GF_SG_VRML_MFINT32, &ptr);
		*((SFInt32 *)ptr) = 0;
		gf_sg_vrml_mf_append(&dest->point, GF_SG_VRML_MFVEC2F, &ptr);
		*((SFVec2f *)ptr) = pt;
	}
	
	i = 0;
	if (tomerge->type.vals[0] == 0) i=1;
	pt_idx = 1;

	for (; i<tomerge->type.count; i++) {
		switch (tomerge->type.vals[i]) {
		case 0:
			if (s->type.vals[s->type.count - 1] == 0) {
				dest->point.vals[dest->point.count - 1] = pt;
			} else {
				gf_sg_vrml_mf_append(&s->type, GF_SG_VRML_MFINT32, &ptr);
				*((SFInt32 *)ptr) = 0;
				gf_sg_vrml_mf_append(&dest->point, GF_SG_VRML_MFVEC2F, &ptr);
				*((SFVec2f *)ptr) = orig->point.vals[pt_idx];
			}
			pt_idx++;
			break;
		case 1:
			gf_sg_vrml_mf_append(&s->type, GF_SG_VRML_MFINT32, &ptr);
			*((SFInt32 *)ptr) = 1;
			gf_sg_vrml_mf_append(&dest->point, GF_SG_VRML_MFVEC2F, &ptr);
			*((SFVec2f *)ptr) = orig->point.vals[pt_idx];
			pt_idx++;
			break;
		case 2:
			gf_sg_vrml_mf_append(&s->type, GF_SG_VRML_MFINT32, &ptr);
			*((SFInt32 *)ptr) = 2;
			for (j=0; j<3; j++) {
				gf_sg_vrml_mf_append(&dest->point, GF_SG_VRML_MFVEC2F, &ptr);
				*((SFVec2f *)ptr) = orig->point.vals[pt_idx];
				pt_idx++;
			}
			break;
		case 7:
			gf_sg_vrml_mf_append(&s->type, GF_SG_VRML_MFINT32, &ptr);
			*((SFInt32 *)ptr) = 7;
			for (j=0; j<2; j++) {
				gf_sg_vrml_mf_append(&dest->point, GF_SG_VRML_MFVEC2F, &ptr);
				*((SFVec2f *)ptr) = orig->point.vals[pt_idx];
				pt_idx++;
			}
			break;
		}
	}
}

void SWFShape_InsertBIFSShape(M_OrderedGroup *og, M_Shape *n)
{
#if 1
	M_Shape *prev;
	GF_ChildNodeItem *l = og->children;
	while (l) {
		prev = (M_Shape*)l->node;
		if (prev->appearance == n->appearance) {
			SWF_MergeCurve2D( (M_Curve2D *)prev->geometry, (M_Curve2D *)n->geometry);
			gf_node_register((GF_Node *)n, NULL);
			gf_node_unregister((GF_Node *)n, NULL);
			return;
		}
		l = l->next;
	}
#endif
	gf_node_insert_child((GF_Node *)og, (GF_Node *)n, -1);
	gf_node_register((GF_Node *) n, (GF_Node *) og);
}

/*this is the core of the parser, translates flash to BIFS shapes*/
GF_Node *SWFShapeToBIFS(SWFReader *read, SWFShape *shape, GF_Node *_self, Bool last_shape)
{
	GF_Node *n;
	GF_Node *og;
	u32 i;
	SWFShapeRec *srec;

	og = _self;
	/*we need a grouping node*/
	if (!_self) {

		/*direct match, no top group*/
		if (last_shape && (gf_list_count(shape->fill_left) + gf_list_count(shape->lines)==1)) {
			Bool is_fill = 1;
			srec = (SWFShapeRec*)gf_list_get(shape->fill_left, 0);
			if (!srec) {
				srec = (SWFShapeRec*)gf_list_get(shape->lines, 0);
				is_fill = 0;
			}
			return SWFShapeToCurve2D(read, shape, srec, is_fill);
		}
		og = SWF_NewNode(read, TAG_MPEG4_OrderedGroup);
	}

	i=0;
	while ((srec = (SWFShapeRec*)gf_list_enum(shape->fill_left, &i))) {
		n = SWFShapeToCurve2D(read, shape, srec, 1);
		if (n) SWFShape_InsertBIFSShape((M_OrderedGroup*)og, (M_Shape *)n);
	}
	i=0;
	while ((srec = (SWFShapeRec*)gf_list_enum(shape->lines, &i))) {
		n = SWFShapeToCurve2D(read, shape, srec, 0);
		if (n) SWFShape_InsertBIFSShape((M_OrderedGroup*)og, (M_Shape *)n);
	}
	return og;
}



GF_Node *SWF_GetBIFSMatrix(SWFReader *read, GF_Matrix2D *mat)
{
	M_TransformMatrix2D *tm = (M_TransformMatrix2D *)SWF_NewNode(read, TAG_MPEG4_TransformMatrix2D);
	tm->mxx = mat->m[0];
	tm->mxy = mat->m[1];
	tm->tx = mat->m[2];
	tm->myx = mat->m[3];
	tm->myy = mat->m[4];
	tm->ty = mat->m[5];
	return (GF_Node *) tm;
}

GF_Node *SWF_GetBIFSColorMatrix(SWFReader *read, GF_ColorMatrix *cmat)
{
	M_ColorTransform *ct = (M_ColorTransform*)SWF_NewNode(read, TAG_MPEG4_ColorTransform);
	ct->mrr = cmat->m[0];
	ct->mrg = cmat->m[1];
	ct->mrb = cmat->m[2];
	ct->mra = cmat->m[3];
	ct->tr = cmat->m[4];
	ct->mgr = cmat->m[5];
	ct->mgg = cmat->m[6];
	ct->mgb = cmat->m[7];
	ct->mga = cmat->m[8];
	ct->tg = cmat->m[9];
	ct->mbr = cmat->m[10];
	ct->mbg = cmat->m[11];
	ct->mbb = cmat->m[12];
	ct->mba = cmat->m[13];
	ct->tb = cmat->m[14];
	ct->mar = cmat->m[15];
	ct->mag = cmat->m[16];
	ct->mab = cmat->m[17];
	ct->maa = cmat->m[18];
	ct->ta = cmat->m[19];
	return (GF_Node *) ct;
}


GF_Node *SWF_GetGlyph(SWFReader *read, u32 fontID, u32 gl_index, GF_Node *par)
{
	char szDEF[1024];
	u32 ID;
	GF_Node *n, *glyph;
	SWFFont *ft;

	sprintf(szDEF, "FT%d_GL%d", fontID, gl_index);
	n = gf_sg_find_node_by_name(read->load->scene_graph, szDEF);
	if (n) {
		gf_node_register(n, par);
		return n;
	}

	/*first use of glyph in file*/
	ft = SWF_FindFont(read, fontID);
	if (!ft) {
		swf_report(read, GF_BAD_PARAM, "Cannot find font %d - skipping glyph", fontID);
		return NULL;
	}
	if (ft->nbGlyphs <= gl_index) {
		swf_report(read, GF_BAD_PARAM, "Glyph #%d not found in font %d - skipping", gl_index, fontID);
		return NULL;
	}
	n = (GF_Node*)gf_list_get(ft->glyphs, gl_index);
	if (gf_node_get_tag(n) != TAG_MPEG4_Shape) {
		swf_report(read, GF_BAD_PARAM, "Glyph #%d in font %d not a shape (translated in %s) - skipping", gl_index, fontID, gf_node_get_class_name(n));
		return NULL;
	}
	glyph = ((M_Shape *)n)->geometry;
	/*space*/
	if (!glyph) return NULL;

	read->load->ctx->max_node_id++;
	ID = read->load->ctx->max_node_id;
	gf_node_set_id(glyph, ID, szDEF);
	gf_node_register(glyph, par);

	/*also insert glyph*/
	swf_insert_symbol(read, n);

	return glyph;
}

GF_Node *SWFTextToBIFS(SWFReader *read, SWFText *text)
{
	u32 i, j;
	Bool use_text;
	Fixed dx;
	SWFGlyphRec *gr;
	SWFFont *ft;
	M_Transform2D *par, *gl_par;
	M_Shape *gl;
	M_TransformMatrix2D *tr;

	use_text = (read->flags & GF_SM_SWF_NO_FONT) ? 1 : 0;
	tr = (M_TransformMatrix2D *) SWF_NewNode(read, TAG_MPEG4_TransformMatrix2D);
	tr->mxx = text->mat.m[0];
	tr->mxy = text->mat.m[1];
	tr->tx = text->mat.m[2];
	tr->myx = text->mat.m[3];
	tr->myy = text->mat.m[4];
	tr->ty = text->mat.m[5];


	i=0;
	while ((gr = (SWFGlyphRec*)gf_list_enum(text->text, &i))) {
		par = (M_Transform2D *) SWF_NewNode(read, TAG_MPEG4_Transform2D);
		par->translation.x = gr->orig_x;
		par->translation.y = gr->orig_y;

		ft = NULL;
		if (use_text) {
			ft = SWF_FindFont(read, gr->fontID);
			if (!ft->glyph_codes) {
				use_text = 0;
				swf_report(read, GF_BAD_PARAM, "Font glyphs are not defined, cannot reference extern font - Forcing glyph embedding");
			}
		}

		if (!use_text) {
			par->scale.x = gr->fontHeight;
			par->scale.y = gr->fontHeight;
		} else {
			/*don't forget we're flipped at top level...*/
			par->scale.y = -FIX_ONE;
		}
		gf_node_insert_child((GF_Node *)tr, (GF_Node *) par, -1);
		gf_node_register((GF_Node *) par, (GF_Node *)tr);

		if (use_text) {
			u16 *str_w, *widestr;
			char *str;
			void *ptr;
			M_Text *t = (M_Text *) SWF_NewNode(read, TAG_MPEG4_Text);
			M_FontStyle *f = (M_FontStyle *) SWF_NewNode(read, TAG_MPEG4_FontStyle);
			t->fontStyle = (GF_Node *) f;
			gf_node_register(t->fontStyle, (GF_Node *) t);

			/*restore back the font height in pixels (it's currently in SWF glyph design units)*/
			f->size = gf_mulfix(gr->fontHeight, FLT2FIX(SWF_TWIP_SCALE / SWF_TEXT_SCALE));

			if (ft->fontName) {
				gf_sg_vrml_mf_reset(&f->family, GF_SG_VRML_MFSTRING);
				gf_sg_vrml_mf_append(&f->family, GF_SG_VRML_MFSTRING, &ptr);
				((SFString*)ptr)->buffer = strdup(ft->fontName);
			}
			gf_sg_vrml_mf_reset(&f->justify, GF_SG_VRML_MFSTRING);
			gf_sg_vrml_mf_append(&f->justify, GF_SG_VRML_MFSTRING, &ptr);
			((SFString*)ptr)->buffer = strdup("BEGIN");

			if (f->style.buffer) free(f->style.buffer);
			if (ft->is_italic && ft->is_bold) f->style.buffer = strdup("BOLDITALIC");
			else if (ft->is_bold) f->style.buffer = strdup("BOLD");
			else if (ft->is_italic) f->style.buffer = strdup("ITALIC");
			else f->style.buffer = strdup("PLAIN");

			/*convert to UTF-8*/
			str_w = (u16*)malloc(sizeof(u16) * (gr->nbGlyphs+1));
			for (j=0; j<gr->nbGlyphs; j++) str_w[j] = ft->glyph_codes[gr->indexes[j]];
			str_w[j] = 0;
			str = (char*)malloc(sizeof(char) * (gr->nbGlyphs+2));
			widestr = str_w;
			j = gf_utf8_wcstombs(str, sizeof(u8) * (gr->nbGlyphs+1), (const unsigned short **) &widestr);
			if (j != (u32) -1) {
				str[j] = 0;
				gf_sg_vrml_mf_reset(&t->string, GF_SG_VRML_MFSTRING);
				gf_sg_vrml_mf_append(&t->string, GF_SG_VRML_MFSTRING, &ptr);
				((SFString*)ptr)->buffer = (char*)malloc(sizeof(char) * (j+1));
				memcpy(((SFString*)ptr)->buffer, str, sizeof(char) * (j+1));
			}

			free(str);
			free(str_w);

			gl = (M_Shape *) SWF_NewNode(read, TAG_MPEG4_Shape);
			gl->appearance = SWF_GetAppearance(read, (GF_Node *) gl, gr->col, 0, 0);				
			gl->geometry = (GF_Node *) t;
			gf_node_register(gl->geometry, (GF_Node *) gl);
			gf_node_insert_child((GF_Node *) par, (GF_Node *)gl, -1);
			gf_node_register((GF_Node *) gl, (GF_Node *) par);
		} else {

			/*convert glyphs*/
			dx = 0;
			for (j=0; j<gr->nbGlyphs; j++) {
				gl = (M_Shape *) SWF_NewNode(read, TAG_MPEG4_Shape);
				gl->geometry = SWF_GetGlyph(read, gr->fontID, gr->indexes[j], (GF_Node *) gl);

				if (!gl->geometry) {
					gf_node_register((GF_Node *) gl, NULL);
					gf_node_unregister((GF_Node *) gl, NULL);
					dx += gr->dx[j];
					continue;
				}
				assert((gf_node_get_tag(gl->geometry)==TAG_MPEG4_Curve2D) || (gf_node_get_tag(gl->geometry)==TAG_MPEG4_XCurve2D));

				gl_par = (M_Transform2D *) SWF_NewNode(read, TAG_MPEG4_Transform2D);
				gl->appearance = SWF_GetAppearance(read, (GF_Node *) gl, gr->col, 0, 0);

				gl_par->translation.x = gf_divfix(dx, gr->fontHeight);
				dx += gr->dx[j];

				gf_node_insert_child((GF_Node *) gl_par, (GF_Node *)gl, -1);
				gf_node_register((GF_Node *) gl, (GF_Node *) gl_par);
				gf_node_insert_child((GF_Node *) par, (GF_Node *)gl_par, -1);
				gf_node_register((GF_Node *) gl_par, (GF_Node *) par);
			}
		}
	}

	return (GF_Node *)tr;
}

#endif


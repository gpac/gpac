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
#include <gpac/xml.h>
#include <gpac/internal/swf_dev.h>

#ifndef GPAC_READ_ONLY

static GF_Err s2b_insert_symbol(SWFReader *read, GF_Node *n)
{
	GF_Command *com;
	GF_CommandField *f;

	if (read->flags & GF_SM_SWF_STATIC_DICT) {
		M_Switch *par = (M_Switch *)gf_sg_find_node_by_name(read->load->scene_graph, "DICTIONARY");
		gf_node_list_add_child(&par->choice, n);
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
		if (read->bifs_dict_au)
			gf_list_add(read->bifs_dict_au->commands, com);
		else
			gf_list_add(read->bifs_au->commands, com);
	}
	return GF_OK;
}

static GF_Node *s2b_get_node(SWFReader *read, u32 ID)
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

static GF_Node *s2b_new_node(SWFReader *read, u32 tag)
{
	GF_Node *n = gf_node_new(read->load->scene_graph, tag);
	if (n) gf_node_init(n);
	return n;
}


static GF_Node *s2b_get_matrix(SWFReader *read, GF_Matrix2D *mat)
{
	M_TransformMatrix2D *tm = (M_TransformMatrix2D *)s2b_new_node(read, TAG_MPEG4_TransformMatrix2D);
	tm->mxx = mat->m[0];
	tm->mxy = mat->m[1];
	tm->tx = mat->m[2];
	tm->myx = mat->m[3];
	tm->myy = mat->m[4];
	tm->ty = mat->m[5];
	return (GF_Node *) tm;
}

static GF_Node *s2b_get_color_matrix(SWFReader *read, GF_ColorMatrix *cmat)
{
	M_ColorTransform *ct = (M_ColorTransform*)s2b_new_node(read, TAG_MPEG4_ColorTransform);
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


static SFColor s2b_get_color(u32 ARGB)
{
	SFColor val;
	val.red = INT2FIX((ARGB>>16)&0xFF) / 255;
	val.green = INT2FIX((ARGB>>8)&0xFF) / 255;
	val.blue = INT2FIX((ARGB)&0xFF) / 255;
	return val;
}

static Fixed s2b_get_alpha(u32 ARGB)
{
	return INT2FIX((ARGB>>24)&0xFF) / 255;
}

static void s2b_insert_appearance(SWFReader *read, GF_Node *app)
{
	M_Shape *s = (M_Shape *) s2b_new_node(read, TAG_MPEG4_Shape);
	s->appearance = app;
	gf_node_register(app, (GF_Node *) s);

	s2b_insert_symbol(read, (GF_Node *)s);
}

static Bool s2b_same_color(SFColor c1, SFColor c2)
{
	if (c1.red != c2.red) return 0;
	if (c1.green != c2.green) return 0;
	if (c1.blue != c2.blue) return 0;
	return 1;
}

static GF_Node *s2b_get_appearance(SWFReader *read, GF_Node *parent, u32 fill_col, Fixed line_width, u32 l_col)
{
	char szDEF[1024];
	u32 ID, i;
	SFColor fc, lc;
	Fixed fill_transp, line_transp;
	M_Appearance *app;
	M_Material2D *mat;

	fc = s2b_get_color(fill_col);
	fill_transp = FIX_ONE - s2b_get_alpha(fill_col);
	if (fill_transp<0) fill_transp=0;
	lc = s2b_get_color(l_col);
	line_transp = FIX_ONE - s2b_get_alpha(l_col);
	if (line_transp<0) line_transp=0;

	i=0;
	while ((app = (M_Appearance*)gf_list_enum(read->apps, &i))) {
		mat = (M_Material2D *)app->material;
		if (!line_width) {
			if (mat->lineProps || !mat->filled) continue;
		} else {
			if (!mat->lineProps) continue;
			if (!s2b_same_color(((M_LineProperties *)mat->lineProps)->lineColor, lc)) continue;
			if (((M_LineProperties *)mat->lineProps)->width != line_width) continue;
		}
		if (!mat->filled && fill_col) continue;
		if (mat->filled) {
			if (!fill_col) continue;
			if (mat->transparency != fill_transp) continue;
			if (!s2b_same_color(mat->emissiveColor, fc)) continue;
		}
		/*OK same appearance let's go*/
		gf_node_register((GF_Node *)app, parent);
		return (GF_Node *)app;
	}

	app = (M_Appearance *) s2b_new_node(read, TAG_MPEG4_Appearance);
	app->material = s2b_new_node(read, TAG_MPEG4_Material2D);
	gf_node_register(app->material, (GF_Node *)app);
	((M_Material2D *)app->material)->filled = 0;

	if (fill_col) {
		((M_Material2D *)app->material)->filled = 1;
		((M_Material2D *)app->material)->emissiveColor = fc;
		((M_Material2D *)app->material)->transparency = fill_transp;
	}
	if (line_width && l_col) {
		if (read->flags & GF_SM_SWF_SCALABLE_LINE) {
			M_XLineProperties *lp = (M_XLineProperties *) s2b_new_node(read, TAG_MPEG4_XLineProperties);
			((M_Material2D *)app->material)->lineProps = (GF_Node *) lp;
			lp->width = line_width;
			lp->lineColor = lc;
			lp->isScalable = 1;
			lp->transparency = line_transp;
			gf_node_register((GF_Node *)lp, app->material);
		} else {
			M_LineProperties *lp = (M_LineProperties *) s2b_new_node(read, TAG_MPEG4_LineProperties);
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
		s2b_insert_appearance(read, (GF_Node *)app);
		gf_list_add(read->apps, app);
	}
	return (GF_Node *) app;
}


static GF_Rect s2b_get_center_bounds(SWFShape *shape, SWFShapeRec *srec)
{
	GF_Rect rc;
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
	return rc;
}

static GF_Node *s2b_get_gradient(SWFReader *read, GF_Node *parent, SWFShape *shape, SWFShapeRec *srec)
{
	Bool is_radial, has_alpha;
	GF_Rect rc;
	GF_Matrix2D mx;
	u32 i;
	MFFloat *keys;
	MFColor *values;
	GF_FieldInfo info;
	M_Appearance *app = (M_Appearance *) s2b_new_node(read, TAG_MPEG4_Appearance);
	gf_node_register((GF_Node *)app, parent);
	app->material = s2b_new_node(read, TAG_MPEG4_Material2D);
	gf_node_register(app->material, (GF_Node *)app);
	((M_Material2D *)app->material)->filled = 1;

	is_radial = (srec->type==0x12) ? 1 : 0;
	app->texture = s2b_new_node(read, is_radial ? TAG_MPEG4_RadialGradient : TAG_MPEG4_LinearGradient);
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
		values->vals[i] = s2b_get_color(srec->grad_col[i]);
		if (s2b_get_alpha(srec->grad_col[i]) != FIX_ONE) has_alpha = 1;
	}
	/*set opacity*/
	if (has_alpha) {
		gf_node_get_field_by_name(app->texture, "opacity", &info);
		gf_sg_vrml_mf_alloc(info.far_ptr, info.fieldType, srec->nbGrad);
		keys = (MFFloat *)info.far_ptr;
		for (i=0; i<srec->nbGrad; i++) {
			keys->vals[i] = s2b_get_alpha(srec->grad_col[i]);
		}
		/*and remove material !!*/
		((M_Material2D *)app->material)->filled = 0;
		((M_Material2D *)app->material)->lineProps = s2b_new_node(read, TAG_MPEG4_LineProperties);;
		((M_LineProperties *)((M_Material2D *)app->material)->lineProps)->width = 0;
		gf_node_register(((M_Material2D *)app->material)->lineProps, app->material);
	}

	rc = s2b_get_center_bounds(shape, srec);

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
	*((GF_Node **)info.far_ptr) = s2b_get_matrix(read, &mx);
	gf_node_register(*((GF_Node **)info.far_ptr), app->texture);
	return (GF_Node *) app;
}

static GF_Node *s2b_get_bitmap(SWFReader *read, GF_Node *parent, SWFShape *shape, SWFShapeRec *srec)
{
	GF_Matrix2D mx;
	GF_Node *bmp;
	GF_FieldInfo info;
	M_Appearance *app;
	char szDEF[100];

	sprintf(szDEF, "Bitmap%d", srec->img_id);
	bmp = gf_sg_find_node_by_name(read->load->scene_graph, szDEF);
	if (!bmp) return NULL;
	app = (M_Appearance *) s2b_new_node(read, TAG_MPEG4_Appearance);
	gf_node_register((GF_Node *)app, parent);
	app->material = s2b_new_node(read, TAG_MPEG4_Material2D);
	gf_node_register(app->material, (GF_Node *)app);
	((M_Material2D *)app->material)->filled = 1;

	app->texture = bmp;
	gf_node_register(bmp, (GF_Node *)app);

	gf_mx2d_copy(mx, srec->mat);
	mx.m[0] *= SWF_TWIP_SCALE;
	mx.m[4] *= SWF_TWIP_SCALE;
	gf_mx2d_add_scale(&mx, FIX_ONE, -FIX_ONE);

	gf_node_get_field_by_name((GF_Node*)app, "textureTransform", &info);
	*((GF_Node **)info.far_ptr) = s2b_get_matrix(read, &mx);
	gf_node_register(*((GF_Node **)info.far_ptr), (GF_Node*)app);
	return (GF_Node *) app;
}

static void s2b_set_appearance(SWFReader *read, SWFShape *shape, M_Shape *n, SWFShapeRec *srec, Bool is_fill)
{
	/*get regular appearance reuse*/
	if (is_fill) {
		switch (srec->type) {
		/*solid/alpha fill*/
		case 0x00:
			n->appearance = s2b_get_appearance(read, (GF_Node *) n, srec->solid_col, 0, 0);
			break;
		case 0x10:
		case 0x12:
			if (read->flags & GF_SM_SWF_NO_GRADIENT) {
				u32 col = srec->grad_col[srec->nbGrad/2];
				col |= 0xFF000000;
				n->appearance = s2b_get_appearance(read, (GF_Node *) n, col, 0, 0);
			} else {
				n->appearance = s2b_get_gradient(read, (GF_Node *) n, shape, srec);
			}
			break;
		case 0x40:
		case 0x41:
		case 0x42:
		case 0x43:
			n->appearance = s2b_get_bitmap(read, (GF_Node *) n, shape, srec);
			break;
		default:
			swf_report(read, GF_NOT_SUPPORTED, "fill_style %x not supported", srec->type);
			break;
		}
	} else {
		n->appearance = s2b_get_appearance(read, (GF_Node *) n, 0, srec->width, srec->solid_col);
	}
}

/*translate a flash sub shape with only one path (eg one looking style) to a BIFS Shape node*/
static GF_Node *s2b_shape_to_curve2d(SWFReader *read, SWFShape *shape, SWFShapeRec *srec, Bool is_fill)
{
	u32 pt_idx, i;
	Bool use_xcurve;
	void *fptr;
	SFVec2f ct1, ct2, ct, pt, move_orig;
	M_Curve2D *curve;
	M_Coordinate2D *points;
	M_Shape *n = (M_Shape *) s2b_new_node(read, TAG_MPEG4_Shape);

	s2b_set_appearance(read, shape, n, srec, is_fill);

	use_xcurve = (read->flags & GF_SM_SWF_QUAD_CURVE) ? 1 : 0;
	if (use_xcurve) {
		curve = (M_Curve2D *) s2b_new_node(read, TAG_MPEG4_XCurve2D);
	} else {
		curve = (M_Curve2D *) s2b_new_node(read, TAG_MPEG4_Curve2D);
	}
	points = (M_Coordinate2D *) s2b_new_node(read, TAG_MPEG4_Coordinate2D);
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

static void s2b_merge_curve2d(M_Curve2D *s, M_Curve2D *tomerge)
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

static void s2b_insert_shape(M_OrderedGroup *og, M_Shape *n)
{
	M_Shape *prev;
	GF_ChildNodeItem *l = og->children;
	while (l) {
		prev = (M_Shape*)l->node;
		if (prev->appearance == n->appearance) {
			s2b_merge_curve2d( (M_Curve2D *)prev->geometry, (M_Curve2D *)n->geometry);
			gf_node_register((GF_Node *)n, NULL);
			gf_node_unregister((GF_Node *)n, NULL);
			return;
		}
		l = l->next;
	}
	gf_node_insert_child((GF_Node *)og, (GF_Node *)n, -1);
	gf_node_register((GF_Node *) n, (GF_Node *) og);
}

/*translates flash to BIFS shapes*/
static GF_Err swf_bifs_define_shape(SWFReader *read, SWFShape *shape, SWFFont *parent_font, Bool last_sub_shape)
{
	GF_Node *n;
	GF_Node *og;
	u32 i;
	SWFShapeRec *srec;

	og = read->cur_shape;
	/*we need a grouping node*/
	if (!read->cur_shape) {

		/*empty shape*/
		if (!shape) {
			n = s2b_new_node(read, TAG_MPEG4_Shape);
		} 
		/*direct match, no top group*/
		else if (last_sub_shape && (gf_list_count(shape->fill_left) + gf_list_count(shape->lines)==1)) {
			Bool is_fill = 1;
			srec = (SWFShapeRec*)gf_list_get(shape->fill_left, 0);
			if (!srec) {
				srec = (SWFShapeRec*)gf_list_get(shape->lines, 0);
				is_fill = 0;
			}
			n = s2b_shape_to_curve2d(read, shape, srec, is_fill);
		} else {
			og = s2b_new_node(read, TAG_MPEG4_OrderedGroup);
			n = og;
		}
		/*register*/

		if (n) {
			if (parent_font) {
				/*not a mistake, that's likelly space char*/
				//if (!glyph) glyph = s2b_new_node(read, TAG_MPEG4_Shape);
				gf_list_add(parent_font->glyphs, n);
				gf_node_register(n, NULL);
			} else {
				char szDEF[1024];
				u32 ID;
				sprintf(szDEF, "Shape%d", shape->ID);
				read->load->ctx->max_node_id++;
				ID = read->load->ctx->max_node_id;
				gf_node_set_id(n, ID, szDEF);

				s2b_insert_symbol(read, n);
			}
		}

		/**/
		if (!og) return GF_OK;
	}

	i=0;
	while ((srec = (SWFShapeRec*)gf_list_enum(shape->fill_left, &i))) {
		n = s2b_shape_to_curve2d(read, shape, srec, 1);
		if (n) s2b_insert_shape((M_OrderedGroup*)og, (M_Shape *)n);
	}
	i=0;
	while ((srec = (SWFShapeRec*)gf_list_enum(shape->lines, &i))) {
		n = s2b_shape_to_curve2d(read, shape, srec, 0);
		if (n) s2b_insert_shape((M_OrderedGroup*)og, (M_Shape *)n);
	}

	if (last_sub_shape) read->cur_shape = NULL;
	else read->cur_shape = og;
	return GF_OK;
}


static GF_Node *s2b_get_glyph(SWFReader *read, u32 fontID, u32 gl_index, GF_Node *par)
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
	ft = swf_find_font(read, fontID);
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
	s2b_insert_symbol(read, n);

	return glyph;
}

static GF_Err swf_bifs_define_text(SWFReader *read, SWFText *text)
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
	tr = (M_TransformMatrix2D *) s2b_new_node(read, TAG_MPEG4_TransformMatrix2D);
	tr->mxx = text->mat.m[0];
	tr->mxy = text->mat.m[1];
	tr->tx = text->mat.m[2];
	tr->myx = text->mat.m[3];
	tr->myy = text->mat.m[4];
	tr->ty = text->mat.m[5];


	i=0;
	while ((gr = (SWFGlyphRec*)gf_list_enum(text->text, &i))) {
		par = (M_Transform2D *) s2b_new_node(read, TAG_MPEG4_Transform2D);
		par->translation.x = gr->orig_x;
		par->translation.y = gr->orig_y;

		ft = NULL;
		if (use_text) {
			ft = swf_find_font(read, gr->fontID);
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
			M_Text *t = (M_Text *) s2b_new_node(read, TAG_MPEG4_Text);
			M_FontStyle *f = (M_FontStyle *) s2b_new_node(read, TAG_MPEG4_FontStyle);
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

			gl = (M_Shape *) s2b_new_node(read, TAG_MPEG4_Shape);
			gl->appearance = s2b_get_appearance(read, (GF_Node *) gl, gr->col, 0, 0);				
			gl->geometry = (GF_Node *) t;
			gf_node_register(gl->geometry, (GF_Node *) gl);
			gf_node_insert_child((GF_Node *) par, (GF_Node *)gl, -1);
			gf_node_register((GF_Node *) gl, (GF_Node *) par);
		} else {

			/*convert glyphs*/
			dx = 0;
			for (j=0; j<gr->nbGlyphs; j++) {
				gl = (M_Shape *) s2b_new_node(read, TAG_MPEG4_Shape);
				gl->geometry = s2b_get_glyph(read, gr->fontID, gr->indexes[j], (GF_Node *) gl);

				if (!gl->geometry) {
					gf_node_register((GF_Node *) gl, NULL);
					gf_node_unregister((GF_Node *) gl, NULL);
					dx += gr->dx[j];
					continue;
				}
				assert((gf_node_get_tag(gl->geometry)==TAG_MPEG4_Curve2D) || (gf_node_get_tag(gl->geometry)==TAG_MPEG4_XCurve2D));

				gl_par = (M_Transform2D *) s2b_new_node(read, TAG_MPEG4_Transform2D);
				gl->appearance = s2b_get_appearance(read, (GF_Node *) gl, gr->col, 0, 0);

				gl_par->translation.x = gf_divfix(dx, gr->fontHeight);
				dx += gr->dx[j];

				gf_node_insert_child((GF_Node *) gl_par, (GF_Node *)gl, -1);
				gf_node_register((GF_Node *) gl, (GF_Node *) gl_par);
				gf_node_insert_child((GF_Node *) par, (GF_Node *)gl_par, -1);
				gf_node_register((GF_Node *) gl_par, (GF_Node *) par);
			}
		}
	}

	if (tr) {
		char szDEF[1024];
		u32 ID;
		sprintf(szDEF, "Text%d", text->ID);
		read->load->ctx->max_node_id++;
		ID = read->load->ctx->max_node_id;
		gf_node_set_id((GF_Node *)tr, ID, szDEF);
		s2b_insert_symbol(read, (GF_Node *)tr);
	}
	return GF_OK;
}


typedef struct
{
	char *final;
	u32 len;
} SWFFlatText;

static void swf_nstart(void *sax_cbck, const char *node_name, const char *name_space, const GF_XMLAttribute *attributes, u32 nb_attributes)
{
}
static void swf_nend(void *sax_cbck, const char *node_name, const char *name_space)
{
}
static void swf_ntext(void *sax_cbck, const char *content, Bool is_cdata)
{
	u32 len;
	SWFFlatText *t;
	if (!content || is_cdata) return;
	t = (SWFFlatText *)sax_cbck;
	len = strlen(content);
	if (!len) return;
	t->final = realloc(t->final, sizeof(char)*(t->len+len+1));
	t->final [t->len] = 0;
	strcat(t->final, content);
	t->len = strlen(t->final)+1;
}


static GF_Err swf_bifs_define_edit_text(SWFReader *read, SWFEditText *text)
{
	char styles[1024];
	char *ptr;
	Bool use_layout;
	M_Layout *layout = NULL;
	M_Shape *txt;
	M_Text *t;
	M_FontStyle *f;
	M_Transform2D *tr;

	tr = (M_Transform2D *) s2b_new_node(read, TAG_MPEG4_Transform2D);
	tr->scale.y = -FIX_ONE;

	use_layout = 0;
	if (text->align==3)
		use_layout = 0;

	if (use_layout) {
		layout = (M_Layout *) s2b_new_node(read, TAG_MPEG4_Layout);
	}

	t = (M_Text *) s2b_new_node(read, TAG_MPEG4_Text);
	f = (M_FontStyle *) s2b_new_node(read, TAG_MPEG4_FontStyle);
	t->fontStyle = (GF_Node *) f;
	gf_node_register(t->fontStyle, (GF_Node *) t);

	/*restore back the font height in pixels (it's currently in SWF glyph design units)*/
	f->size = text->font_height;
	f->spacing = text->font_height + text->leading;

	gf_sg_vrml_mf_reset(&f->justify, GF_SG_VRML_MFSTRING);
	gf_sg_vrml_mf_append(&f->justify, GF_SG_VRML_MFSTRING, &ptr);
	switch (text->align) {
	case 0:
		((SFString*)ptr)->buffer = strdup("BEGIN"); 
		break;
	case 1:
		((SFString*)ptr)->buffer = strdup("END"); 
		break;
	case 3:
		((SFString*)ptr)->buffer = strdup("JUSTIFY"); 
		break;
	default:
		((SFString*)ptr)->buffer = strdup("MIDDLE"); 
		break;
	}

	strcpy(styles, "");
	if (!text->read_only) strcat(styles, "EDITABLE");
	if (text->password) strcat(styles, "PASSWORD");
	
	if (f->style.buffer) free(f->style.buffer);
	f->style.buffer = strdup(styles);

	if (text->init_value) {
		gf_sg_vrml_mf_reset(&t->string, GF_SG_VRML_MFSTRING);
		gf_sg_vrml_mf_append(&t->string, GF_SG_VRML_MFSTRING, &ptr);

		if (text->html) {
			GF_SAXParser *xml;
			SWFFlatText flat;
			flat.final = 0;
			flat.len = 0;
			xml = gf_xml_sax_new(swf_nstart, swf_nend, swf_ntext, &flat);
			gf_xml_sax_init(xml, NULL);
			gf_xml_sax_parse(xml, text->init_value);
			gf_xml_sax_del(xml);

			if (flat.final) {
				((SFString*)ptr)->buffer = strdup(flat.final);
				free(flat.final);
			}
		} else {
			((SFString*)ptr)->buffer = strdup(text->init_value);
		}
	}


	txt = (M_Shape *) s2b_new_node(read, TAG_MPEG4_Shape);
	txt->appearance = s2b_get_appearance(read, (GF_Node *) txt, text->color, 0, 0);				
	txt->geometry = (GF_Node *) t;
	gf_node_register(txt->geometry, (GF_Node *) txt);

	if (layout) {
		gf_node_insert_child((GF_Node *) layout, (GF_Node *)txt, -1);
		gf_node_register((GF_Node *) txt, (GF_Node *) layout);

		gf_node_insert_child((GF_Node *) tr, (GF_Node *)layout, -1);
		gf_node_register((GF_Node *) layout, (GF_Node *) tr);
	} else {
		gf_node_insert_child((GF_Node *) tr, (GF_Node *)txt, -1);
		gf_node_register((GF_Node *) txt, (GF_Node *) tr);
	} 
	if (tr) {
		char szDEF[1024];
		u32 ID;
		sprintf(szDEF, "Text%d", text->ID);
		read->load->ctx->max_node_id++;
		ID = read->load->ctx->max_node_id;
		gf_node_set_id((GF_Node*)tr, ID, szDEF);
		s2b_insert_symbol(read, (GF_Node*)tr);
	}
	return GF_OK;
}


/*called upon end of sprite or clip*/
static void swf_bifs_end_of_clip(SWFReader *read)
{
	char szDEF[1024];
	u32 i;
	GF_AUContext *au;
	GF_Command *com;
	GF_CommandField *f;
	GF_Node *empty;
	
	return;

	empty = gf_sg_find_node_by_name(read->load->scene_graph, "EMPTYSHAPE");

	au = gf_list_get(read->bifs_es->AUs, 0);
	for (i=0; i<read->max_depth; i++) {
		/*and write command*/
		com = gf_sg_command_new(read->load->scene_graph, GF_SG_INDEXED_REPLACE);
		/*in sprite definiton, modify at sprite root level*/
		if (read->current_sprite_id) {
			sprintf(szDEF, "Sprite%d_displaylist", read->current_sprite_id);
			com->node = gf_sg_find_node_by_name(read->load->scene_graph, szDEF);
		} else {
			com->node = gf_sg_find_node_by_name(read->load->scene_graph, "DISPLAYLIST");
		}
		gf_node_register(com->node, NULL);
		f = gf_sg_command_field_new(com);
		f->field_ptr = &f->new_node;
		f->fieldType = GF_SG_VRML_SFNODE;
		f->pos = i;
		f->fieldIndex = 2;	/*children index*/
		f->new_node = empty;
		gf_node_register(f->new_node, com->node);

		gf_list_insert(au->commands, com, i);
	}
}

static Bool swf_bifs_allocate_depth(SWFReader *read, u32 depth)
{
	GF_Node *disp, *empty;
	if (read->max_depth > depth) return 1;

	/*modify sprite display list*/
	if (read->current_sprite_id) {
		char szDEF[100];
		sprintf(szDEF, "Sprite%d_displaylist", read->current_sprite_id);
		disp = gf_sg_find_node_by_name(read->load->scene_graph, szDEF);
	} 
	/*modify display list*/
	else {
		disp = gf_sg_find_node_by_name(read->load->scene_graph, "DISPLAYLIST");
	}

	empty = gf_sg_find_node_by_name(read->load->scene_graph, "EMPTYSHAPE");
	while (read->max_depth<=depth) {
		gf_node_insert_child(disp, empty, -1);
		gf_node_register(empty, disp);
		read->max_depth++;
	}
	return 0;
}

static GF_Err swf_init_od(SWFReader *read, Bool root_only)
{
	GF_ESD *esd;

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
	if (root_only) return GF_OK;

	if (read->od_es) return GF_OK;
	read->od_es = gf_sm_stream_new(read->load->ctx, 2, 1, 1);
	if (!read->od_es) return GF_OUT_OF_MEM;

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

static GF_Err swf_insert_od(SWFReader *read, u32 at_time, GF_ObjectDescriptor *od)
{
	u32 i;
	GF_ODUpdate *com;
	read->od_au = gf_sm_stream_au_new(read->od_es, at_time, 0, 1);
	if (!read->od_au) return GF_OUT_OF_MEM;

	i=0;
	while ((com = (GF_ODUpdate *)gf_list_enum(read->od_au->commands, &i))) {
		if (com->tag == GF_ODF_OD_UPDATE_TAG) {
			gf_list_add(com->objectDescriptors, od);
			return GF_OK;
		}
	}
	com = (GF_ODUpdate *) gf_odf_com_new(GF_ODF_OD_UPDATE_TAG);
	gf_list_add(com->objectDescriptors, od);
	return gf_list_add(read->od_au->commands, com);
}

static u16 swf_get_od_id(SWFReader *read)
{
	return ++read->prev_od_id;
}

static u16 swf_get_es_id(SWFReader *read)
{
	return ++read->prev_es_id;
}


static GF_Err swf_bifs_define_sprite(SWFReader *read, u32 nb_frames)
{
	GF_Err e;
	GF_ObjectDescriptor *od;
	GF_ESD *esd;
	u32 ID;
	GF_Node *n, *par;
	GF_FieldInfo info;
	char szDEF[100];
	GF_StreamContext *prev_sc;
	GF_AUContext *prev_au;

	/*init OD*/
	e = swf_init_od(read, 0);
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
	n = s2b_new_node(read, TAG_MPEG4_AnimationStream);
	sprintf(szDEF, "Sprite%d_ctrl", read->current_sprite_id);

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
	n = s2b_new_node(read, TAG_MPEG4_Group);
	sprintf(szDEF, "Sprite%d_displaylist", read->current_sprite_id);

	read->load->ctx->max_node_id++;
	ID = read->load->ctx->max_node_id;
	gf_node_set_id(n, ID, szDEF);
	par = gf_sg_find_node_by_name(read->load->scene_graph, "DICTIONARY");
	assert(par);
	gf_node_list_add_child(&((M_Switch *)par)->choice, n);
	gf_node_register(n, par);
	par = gf_sg_find_node_by_name(read->load->scene_graph, "EMPTYSHAPE");
	gf_node_insert_child(n, par, -1);
	gf_node_register(par, n);

	/*store BIFS context*/
	prev_sc = read->bifs_es;
	prev_au = read->bifs_au;
	/*create new BIFS stream*/
	read->bifs_es = gf_sm_stream_new(read->load->ctx, esd->ESID, GF_STREAM_SCENE, 1);
	read->bifs_es->timeScale = prev_sc->timeScale;
	/*create first AU*/
	read->bifs_au = gf_sm_stream_au_new(read->bifs_es, 0, 0, 1);

	e = swf_parse_sprite(read);
	if (e) return e;

	swf_bifs_end_of_clip(read);

	/*restore BIFS context*/
	read->bifs_es = prev_sc;
	read->bifs_au = prev_au;

	return GF_OK;
}

static GF_Err swf_bifs_setup_sound(SWFReader *read, SWFSound *snd)
{
	GF_Err e;
	GF_ObjectDescriptor *od;
	GF_ESD *esd;
	GF_MuxInfo *mux;
	GF_Node *n, *par;
	GF_FieldInfo info;
	u32 ID;
	char szDEF[100];

	e = swf_init_od(read, 0);
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
	n = s2b_new_node(read, TAG_MPEG4_Sound2D);
	gf_node_insert_child(read->root, n, 0);
	gf_node_register(n, read->root);
	par = n;
	n = s2b_new_node(read, TAG_MPEG4_AudioClip);
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

	gf_node_get_field_by_name(n, "startTime", &info);
	*((SFTime *)info.far_ptr) = FLT2FIX(snd->frame_delay_ms/1000.0f);

	snd->is_setup = 1;
	return GF_OK;
}

static GF_Err swf_bifs_start_sound(SWFReader *read, SWFSound *snd, Bool stop)
{
	char szDEF[100];
	GF_Command *com;
	GF_FieldInfo info;
	GF_Node *sound2D;
	GF_CommandField *f;

	sprintf(szDEF, "Sound%d", snd->ID);
	sound2D = gf_sg_find_node_by_name(read->load->scene_graph, szDEF);
	/*check flags*/
	if (stop) {
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

static GF_Err swf_bifs_setup_image(SWFReader *read, u32 ID, char *fileName)
{

	GF_Err e;
	GF_ObjectDescriptor *od;
	GF_ESD *esd;
	GF_MuxInfo *mux;
	GF_Node *n, *par;
	GF_FieldInfo info;
	char szDEF[100];
	u32 AlphaPlaneSize = 0;
	
	
	e = swf_init_od(read, 0);
	if (e) return e;

	/*create visual object*/
	od = (GF_ObjectDescriptor *) gf_odf_desc_new(GF_ODF_OD_TAG);
	if (!od) return GF_OUT_OF_MEM;
	od->objectDescriptorID = swf_get_od_id(read);
	esd = (GF_ESD *) gf_odf_desc_new(GF_ODF_ESD_TAG);
	if (!esd) return GF_OUT_OF_MEM;
	esd->ESID = swf_get_es_id(read);
	esd->OCRESID = esd->ESID;
	gf_list_add(od->ESDescriptors, esd);

	/*setup mux info*/
	mux = (GF_MuxInfo*)gf_odf_desc_new(GF_ODF_MUXINFO_TAG);

	mux->file_name = strdup(fileName);
	/*destroy file once done*/
	//mux->delete_file = 1;
	gf_list_add(esd->extensionDescriptors, mux);


	/*by default insert OD at begining*/
	e = swf_insert_od(read, 0, od);
	if (e) {
		gf_odf_desc_del((GF_Descriptor *) od);
		return e;
	}
	/*create appearance clip*/
	par = s2b_new_node(read, TAG_MPEG4_Shape);
	s2b_insert_symbol(read, par);
	n = s2b_new_node(read, TAG_MPEG4_Appearance);
	((M_Shape *)par)->appearance = n;
	gf_node_register(n, par);

	par = n;
	n = s2b_new_node(read, TAG_MPEG4_ImageTexture);
	((M_Appearance *)par)->texture = n;
	gf_node_register(n, par);

	sprintf(szDEF, "Bitmap%d", ID);
	read->load->ctx->max_node_id++;
	ID = read->load->ctx->max_node_id;
	gf_node_set_id(n, ID, szDEF);

	/*assign URL*/
	gf_node_get_field_by_name(n, "url", &info);
	gf_sg_vrml_mf_alloc(info.far_ptr, info.fieldType, 1);
	((MFURL *)info.far_ptr)->vals[0].OD_ID = od->objectDescriptorID;

	return GF_OK;
}

static GF_Err swf_bifs_set_backcol(SWFReader *read, u32 xrgb)
{
	GF_Command *com;
	GF_CommandField *f;
	com = gf_sg_command_new(read->load->scene_graph, GF_SG_FIELD_REPLACE);
	com->node = gf_sg_find_node_by_name(read->load->scene_graph, "BACKGROUND");
	gf_node_register(com->node, NULL);
	f = gf_sg_command_field_new(com);
	f->field_ptr = gf_sg_vrml_field_pointer_new(GF_SG_VRML_SFCOLOR);
	f->fieldType = GF_SG_VRML_SFCOLOR;
	f->fieldIndex = 1;	/*backColor index*/
	((SFColor *)f->field_ptr)->red = INT2FIX((xrgb>>16) & 0xFF) / 255;
	((SFColor *)f->field_ptr)->green = INT2FIX((xrgb>>8) & 0xFF) / 255;
	((SFColor *)f->field_ptr)->blue = INT2FIX((xrgb) & 0xFF) / 255;
	return gf_list_add(read->bifs_au->commands, com);
}


static GF_Err swf_bifs_place_obj(SWFReader *read, u32 depth, u32 ID, u32 type, GF_Matrix2D *mat, GF_ColorMatrix *cmat)
{
	GF_Command *com;
	GF_CommandField *f;
	GF_Node *shape, *par;
	char szDEF[100];
	Bool is_sprite;

	shape = s2b_get_node(read, ID);
	is_sprite = 0;
	if (!shape) {
		sprintf(szDEF, "Sprite%d_displaylist", ID);
		shape = gf_sg_find_node_by_name(read->load->scene_graph, szDEF);
		if (shape) is_sprite = 1;
	}
	if (!shape) return GF_BAD_PARAM;

	/*then add cmat/mat and node*/
	par = NULL;
	if (!mat && !cmat) {
		par = shape;
	} else {
		if (mat) par = s2b_get_matrix(read, mat);
		if (cmat) {
			GF_Node *cm = s2b_get_color_matrix(read, cmat);
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

	/*and write command*/
	com = gf_sg_command_new(read->load->scene_graph, GF_SG_INDEXED_REPLACE);
	/*in sprite definiton, modify at sprite root level*/
	if (read->current_sprite_id) {
		sprintf(szDEF, "Sprite%d_displaylist", read->current_sprite_id);
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

static GF_Err swf_bifs_remove_obj(SWFReader *read, u32 depth)
{
	GF_Command *com;
	GF_CommandField *f;

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

static GF_Err swf_bifs_show_frame(SWFReader *read)
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

	/*create a new AU for next frame*/
	ts = (read->current_frame + 1) * 100;

#if 0
	/*all frames in sprites are RAP (define is not allowed in sprites)
	if we use a ctrl stream, all AUs are RAP (defines are placed in a static dictionary)
	*/
	is_rap = (read->current_sprite_id || (read->flags & GF_SM_SWF_SPLIT_TIMELINE)) ? 1 : 0;
#else
	/*"The contents of the second frame are the cumulative effect of performing all the control tag operations from 
	the beginning of the file to the second ShowFrame tag, and so on"
	using RAP=0 forces reprocessing of previous frames when seeking/jumping*/
	is_rap = 0;
#endif
	/*if we use ctrl stream, same thing*/
	read->bifs_au = gf_sm_stream_au_new(read->bifs_es, ts, 0, is_rap);

	/*not in a sprite def, using a control stream and no static dictionary: create a new AU for new symbols*/
	if (!read->current_sprite_id 
		&& (read->flags & GF_SM_SWF_SPLIT_TIMELINE) 
		&& !(read->flags & GF_SM_SWF_STATIC_DICT) 
	)
		read->bifs_dict_au = gf_sm_stream_au_new(read->bifs_dict_es, ts, 0, 0);
	
	return GF_OK;
}

static GF_Err swf_bifs_define_button(SWFReader *read, SWF_Button *btn)
{
	char szName[1024];
	u32 i, ID;
	M_Switch *button = (M_Switch *) s2b_new_node(read, TAG_MPEG4_Switch);
	sprintf(szName, "Button%d", btn->ID);
	read->load->ctx->max_node_id++;
	ID = read->load->ctx->max_node_id;
	gf_node_set_id((GF_Node *)button, ID, szName);
	s2b_insert_symbol(read, (GF_Node *)button);
	/*by default show first character*/
	button->whichChoice = 0;
	for (i=0; i<btn->count; i++) {
		GF_Node *character = s2b_get_node(read, btn->buttons[i].character_id);
		if (character) {
			gf_node_list_add_child(&button->choice, character);
			gf_node_register(character, (GF_Node *)button);
		}
	}
	return GF_OK;
}

static void swf_bifs_finalize(SWFReader *read)
{
	u32 i, count;

	swf_bifs_end_of_clip(read);	

	count = gf_list_count(read->fonts);
	for (i=0;i<count; i++) {
		SWFFont *ft = (SWFFont *)gf_list_get(read->fonts, i);
		while (gf_list_count(ft->glyphs)) {
			GF_Node *gl = (GF_Node *)gf_list_get(ft->glyphs, 0);
			gf_list_rem(ft->glyphs, 0);
			gf_node_unregister(gl, NULL);
		}
	}
}

GF_Err swf_to_bifs_init(SWFReader *read)
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

	/*init callbacks*/
	read->show_frame = swf_bifs_show_frame;
	read->allocate_depth = swf_bifs_allocate_depth;
	read->place_obj = swf_bifs_place_obj;
	read->remove_obj = swf_bifs_remove_obj;
	read->define_shape = swf_bifs_define_shape;
	read->define_sprite = swf_bifs_define_sprite;
	read->set_backcol = swf_bifs_set_backcol;
	read->define_button = swf_bifs_define_button;
	read->define_text = swf_bifs_define_text;
	read->define_edit_text = swf_bifs_define_edit_text;
	read->setup_sound = swf_bifs_setup_sound;
	read->start_sound = swf_bifs_start_sound;
	read->setup_image = swf_bifs_setup_image;
	read->finalize = swf_bifs_finalize;

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
	com->node = read->root = s2b_new_node(read, TAG_MPEG4_OrderedGroup);
	gf_node_register(read->root, NULL);

	/*hehehe*/
	n = s2b_new_node(read, TAG_MPEG4_WorldInfo);
	gf_node_insert_child(read->root, n, -1);
	gf_node_register(n, read->root);
	((M_WorldInfo *)n)->title.buffer = strdup("GPAC SWF CONVERTION DISCLAIMER");
	gf_sg_vrml_mf_alloc( & ((M_WorldInfo *)n)->info, GF_SG_VRML_MFSTRING, 3);

	sprintf(szMsg, "%s file converted to MPEG-4 Systems", read->load->fileName);
	((M_WorldInfo *)n)->info.vals[0] = strdup(szMsg);
	((M_WorldInfo *)n)->info.vals[1] = strdup("Conversion done using GPAC version " GPAC_FULL_VERSION " - (C) 2000-2005 GPAC");
	((M_WorldInfo *)n)->info.vals[2] = strdup("Macromedia SWF to MPEG-4 Conversion mapping released under GPL license");

	/*background*/
	n = s2b_new_node(read, TAG_MPEG4_Background2D);
	((M_Background2D *)n)->backColor.red = FIX_ONE;
	((M_Background2D *)n)->backColor.green = FIX_ONE;
	((M_Background2D *)n)->backColor.blue = FIX_ONE;
	gf_node_set_id(n, 1, "BACKGROUND");
	gf_node_insert_child(read->root, n, -1);
	gf_node_register(n, read->root);
	
	/*dictionary*/
	n = s2b_new_node(read, TAG_MPEG4_Switch);
	gf_node_set_id(n, 2, "DICTIONARY");
	gf_node_insert_child(read->root, n, -1);
	gf_node_register(n, read->root);
	/*empty shape to fill depth levels & sprites roots*/
	n2 = s2b_new_node(read, TAG_MPEG4_Shape);
	gf_node_set_id(n2, 3, "EMPTYSHAPE");
	gf_node_list_add_child( &((M_Switch *)n)->choice, n2);
	gf_node_register(n2, n);

	/*display list*/
	n = s2b_new_node(read, TAG_MPEG4_Transform2D);
	gf_node_set_id(n, 4, "DISPLAYLIST");
	gf_node_insert_child(read->root, n, -1);
	gf_node_register(n, read->root);
	/*update w/h transform*/
	((M_Transform2D *)n)->scale.y = -FIX_ONE;
	((M_Transform2D *)n)->translation.x = -read->width/2;
	((M_Transform2D *)n)->translation.y = read->height/2;

	read->load->ctx->max_node_id = 5;


	swf_init_od(read, 1);

	/*always reserve OD_ID=1 for main ctrl stream if any*/
	read->prev_od_id = 1;
	/*always reserve ES_ID=2 for OD stream, 3 for main ctrl stream if any*/
	read->prev_es_id = 3;

	/*no control stream*/
	if (!(read->flags & GF_SM_SWF_SPLIT_TIMELINE)) return GF_OK;

	/*init OD*/
	e = swf_init_od(read, 0);
	if (e) return e;

	/*we want a dynamic dictionary*/
	if (!(read->flags & GF_SM_SWF_STATIC_DICT)) {
		read->bifs_dict_es = read->bifs_es;
		read->bifs_dict_au = read->bifs_au;
	}

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
	n = s2b_new_node(read, TAG_MPEG4_AnimationStream);
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
	((M_AnimationStream *)n)->loop = 0;

	return GF_OK;
}

#endif


/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2000-2023
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
#include <gpac/internal/scenegraph_dev.h>


#ifndef GPAC_DISABLE_VRML

#ifndef GPAC_DISABLE_SWF_IMPORT

#define SWF_TEXT_SCALE				(1/1024.0f)

typedef struct
{
	u32 btn_id;
	u32 sprite_up_id;
} S2BBtnRec;

static GF_Err s2b_insert_symbol(SWFReader *read, GF_Node *n)
{
	GF_CommandField *f;

	if (read->flags & GF_SM_SWF_STATIC_DICT) {
		M_Switch *par = (M_Switch *)gf_sg_find_node_by_name(read->load->scene_graph, "DICTIONARY");
		gf_node_list_add_child(&par->choice, n);
		gf_node_register((GF_Node *)n, (GF_Node *)par);
	} else {
		GF_Command *com = gf_sg_command_new(read->load->scene_graph, GF_SG_INDEXED_INSERT);
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
	u32 ID, i;
	SFColor fc, lc;
	Fixed fill_transp, line_transp;
	M_Appearance *app;

	fc = s2b_get_color(fill_col);
	fill_transp = FIX_ONE - s2b_get_alpha(fill_col);
	if (fill_transp<0) fill_transp=0;
	lc = s2b_get_color(l_col);
	line_transp = FIX_ONE - s2b_get_alpha(l_col);
	if (line_transp<0) line_transp=0;

	i=0;
	while ((app = (M_Appearance*)gf_list_enum(read->apps, &i))) {
		M_Material2D *mat = (M_Material2D *)app->material;
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
		char szDEF[1024];
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
		keys->vals[i] = INT2FIX(srec->grad_ratio[i])/255;
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
		((M_Material2D *)app->material)->lineProps = s2b_new_node(read, TAG_MPEG4_LineProperties);
		((M_LineProperties *)((M_Material2D *)app->material)->lineProps)->width = 0;
		gf_node_register(((M_Material2D *)app->material)->lineProps, app->material);
	}

	rc = s2b_get_center_bounds(shape, srec);

	gf_mx2d_init(mx);
	mx.m[0] = gf_invfix(rc.width);
	mx.m[2] = - gf_divfix(rc.x, rc.width);
	mx.m[4] = gf_invfix(rc.height);
	mx.m[5] = FIX_ONE - gf_divfix(rc.y, rc.height);

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
	mx.m[0] = gf_invfix(rc.width);
	mx.m[2] = - gf_divfix(rc.x, rc.width);
	mx.m[4] = gf_invfix(rc.height);
	mx.m[5] = FIX_ONE - gf_divfix(rc.y, rc.height);
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

	gf_mx2d_init(mx);
	gf_mx2d_add_scale(&mx, FIX_ONE, -FIX_ONE);
//	gf_mx2d_add_scale(&mx, SWF_TWIP_SCALE, -SWF_TWIP_SCALE);
//	gf_mx2d_pre_multiply(&mx, &srec->mat);

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
static GF_Node *s2b_shape_to_curve2d(SWFReader *read, SWFShape *shape, SWFShapeRec *srec, Bool is_fill, M_Coordinate2D *c)
{
	u32 pt_idx, i;
	Bool use_xcurve;
	void *fptr;
	SFVec2f ct1, ct2, ct, pt;
	MFInt32 *types, *idx;
	M_Coordinate2D *points;
	GF_Node *ic2d = NULL;
	M_Shape *n = (M_Shape *) s2b_new_node(read, TAG_MPEG4_Shape);

	s2b_set_appearance(read, shape, n, srec, is_fill);

	use_xcurve = (read->flags & GF_SM_SWF_QUAD_CURVE) ? 1 : 0;
	if (c) {
		GF_FieldInfo info;
		ic2d = gf_sg_proto_create_instance(read->load->scene_graph, gf_sg_find_proto(read->load->scene_graph, 0, "IndexedCurve2D"));
		points = c;

		gf_node_get_field_by_name(ic2d, "type", &info);
		types = (MFInt32 *)info.far_ptr;

		gf_node_get_field_by_name(ic2d, "coordIndex", &info);
		idx = (MFInt32 *)info.far_ptr;

		gf_node_get_field_by_name(ic2d, "coord", &info);
		*(GF_Node **)info.far_ptr = (GF_Node *)c;
		gf_node_register((GF_Node *)c, ic2d);

		n->geometry = ic2d;
		gf_node_register(ic2d, (GF_Node *)n);
	} else {
		M_Curve2D *curve;
		if (use_xcurve) {
			curve = (M_Curve2D *) s2b_new_node(read, TAG_MPEG4_XCurve2D);
		} else {
			curve = (M_Curve2D *) s2b_new_node(read, TAG_MPEG4_Curve2D);
		}
		points = (M_Coordinate2D *) s2b_new_node(read, TAG_MPEG4_Coordinate2D);
		curve->point = (GF_Node *) points;

		gf_node_register((GF_Node *) points, (GF_Node *) curve);
		curve->fineness = FIX_ONE;
		types = &curve->type;
		idx = NULL;
		n->geometry = (GF_Node *) curve;
		gf_node_register((GF_Node *) curve, (GF_Node *)n);
	}


	gf_assert(srec->path->nbType);

	pt_idx = 0;
	for (i=0; i<srec->path->nbType; i++) {
		switch (srec->path->types[i]) {
		/*moveTo*/
		case 0:
			/*first moveTo implicit in BIFS*/
			if (i) {
				gf_sg_vrml_mf_append(types, GF_SG_VRML_MFINT32, &fptr);
				*((SFInt32 *)fptr) = 0;
			}
			if (c) {
				gf_sg_vrml_mf_append(idx, GF_SG_VRML_MFINT32, &fptr);
				*((SFInt32 *)fptr) = srec->path->idx[pt_idx];
			} else {
				gf_sg_vrml_mf_append(&points->point, GF_SG_VRML_MFVEC2F, &fptr);
				((SFVec2f *)fptr)->x = srec->path->pts[pt_idx].x;
				((SFVec2f *)fptr)->y = srec->path->pts[pt_idx].y;
			}
			pt_idx++;
			break;
		/*lineTo*/
		case 1:
			gf_sg_vrml_mf_append(types, GF_SG_VRML_MFINT32, &fptr);
			*((SFInt32 *)fptr) = 1;
			if (c) {
				gf_sg_vrml_mf_append(idx, GF_SG_VRML_MFINT32, &fptr);
				*((SFInt32 *)fptr) = srec->path->idx[pt_idx];
			} else {
				gf_sg_vrml_mf_append(&points->point, GF_SG_VRML_MFVEC2F, &fptr);
				((SFVec2f *)fptr)->x = srec->path->pts[pt_idx].x;
				((SFVec2f *)fptr)->y = srec->path->pts[pt_idx].y;
			}
			pt_idx++;
			break;
		/*curveTo*/
		case 2:
			/*XCurve2D has quad arcs*/
			if (c || use_xcurve) {
				gf_sg_vrml_mf_append(types, GF_SG_VRML_MFINT32, &fptr);
				*((SFInt32 *)fptr) = 7;

				if (c) {
					gf_sg_vrml_mf_append(idx, GF_SG_VRML_MFINT32, &fptr);
					*((SFInt32 *)fptr) = srec->path->idx[pt_idx];
					gf_sg_vrml_mf_append(idx, GF_SG_VRML_MFINT32, &fptr);
					*((SFInt32 *)fptr) = srec->path->idx[pt_idx+1];
				} else {
					gf_sg_vrml_mf_append(&points->point, GF_SG_VRML_MFVEC2F, &fptr);
					((SFVec2f *)fptr)->x = srec->path->pts[pt_idx].x;
					((SFVec2f *)fptr)->y = srec->path->pts[pt_idx].y;
					gf_sg_vrml_mf_append(&points->point, GF_SG_VRML_MFVEC2F, &fptr);
					((SFVec2f *)fptr)->x = srec->path->pts[pt_idx+1].x;
					((SFVec2f *)fptr)->y = srec->path->pts[pt_idx+1].y;
				}

				pt_idx+=2;
			} else {
				gf_sg_vrml_mf_append(types, GF_SG_VRML_MFINT32, &fptr);
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

	if (ic2d) gf_node_init(ic2d);

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

static void s2b_insert_shape(M_OrderedGroup *og, M_Shape *n, Bool is_proto)
{
	GF_ChildNodeItem *l = og->children;
	if (!is_proto) {
		while (l) {
			M_Shape *prev = (M_Shape*)l->node;
			if (prev->appearance == n->appearance) {
				s2b_merge_curve2d( (M_Curve2D *)prev->geometry, (M_Curve2D *)n->geometry);
				gf_node_register((GF_Node *)n, NULL);
				gf_node_unregister((GF_Node *)n, NULL);
				return;
			}
			l = l->next;
		}
	}
	gf_node_insert_child((GF_Node *)og, (GF_Node *)n, -1);
	gf_node_register((GF_Node *) n, (GF_Node *) og);
}

static void s2b_insert_rec_in_coord(M_Coordinate2D *c, SWFShapeRec *srec)
{
	u32 i, j;
	srec->path->idx = gf_malloc(sizeof(u32)*srec->path->nbPts);

	for (i=0; i<srec->path->nbPts; i++) {
		for (j=0; j<c->point.count; j++) {
			if ( (c->point.vals[j].x == srec->path->pts[i].x) && (c->point.vals[j].y == srec->path->pts[i].y)) {
				break;
			}
		}
		if (j==c->point.count) {
			c->point.count++;
			c->point.vals = gf_realloc(c->point.vals, sizeof(SFVec2f)*c->point.count);
			c->point.vals[j] = srec->path->pts[i];
		}
		srec->path->idx[i] = j;
	}
}

/*translates flash to BIFS shapes*/
static GF_Err swf_bifs_define_shape(SWFReader *read, SWFShape *shape, SWFFont *parent_font, Bool last_sub_shape)
{
	GF_Node *n;
	GF_Node *og;
	M_Coordinate2D *c;
	char szDEF[1024];
	u32 ID;
	u32 i;
	SWFShapeRec *srec;

	og = read->cur_shape;
	/*we need a grouping node*/
	if (!read->cur_shape) {

		/*empty shape - for fonts, not a mistake, that's likely space char*/
		if (!shape) {
			if (!parent_font)
				return GF_OK;
			n = s2b_new_node(read, TAG_MPEG4_Shape);
		}
		/*direct match, no top group*/
		else if (last_sub_shape && (gf_list_count(shape->fill_left) + gf_list_count(shape->lines)<=1)) {
			Bool is_fill = 1;
			srec = (SWFShapeRec*)gf_list_get(shape->fill_left, 0);
			if (!srec) {
				srec = (SWFShapeRec*)gf_list_get(shape->lines, 0);
				is_fill = 0;
			}
			if (!srec) {
				n = s2b_new_node(read, TAG_MPEG4_Shape);
			} else {
				n = s2b_shape_to_curve2d(read, shape, srec, is_fill, NULL);
			}
		} else {
			og = s2b_new_node(read, TAG_MPEG4_OrderedGroup);
			n = og;
		}
		/*register*/

		if (n) {
			if (parent_font) {
				gf_list_add(parent_font->glyphs, n);
				gf_node_register(n, NULL);
			} else {
				sprintf(szDEF, "Shape%d", shape->ID);
				read->load->ctx->max_node_id++;
				ID = read->load->ctx->max_node_id;
				gf_node_set_id(n, ID, szDEF);

				s2b_insert_symbol(read, n);
			}
		}
		if (!og) return GF_OK;
	}

	c = NULL;
	if (read->flags & GF_SM_SWF_USE_IC2D) {
		c = (M_Coordinate2D *)gf_node_new(read->load->scene_graph, TAG_MPEG4_Coordinate2D);
		sprintf(szDEF, "ShapePts%d", shape->ID);
		read->load->ctx->max_node_id++;
		ID = read->load->ctx->max_node_id;
		gf_node_set_id((GF_Node*)c, ID, szDEF);
	}

	i=0;
	while ((srec = (SWFShapeRec*)gf_list_enum(shape->fill_left, &i))) {
		if (c)
			s2b_insert_rec_in_coord(c, srec);

		n = s2b_shape_to_curve2d(read, shape, srec, 1, c);
		if (n) s2b_insert_shape((M_OrderedGroup*)og, (M_Shape *)n, c ? 1 : 0);
	}
	i=0;
	while ((srec = (SWFShapeRec*)gf_list_enum(shape->lines, &i))) {
		if (c)
			s2b_insert_rec_in_coord(c, srec);

		n = s2b_shape_to_curve2d(read, shape, srec, 0, c);
		if (n) s2b_insert_shape((M_OrderedGroup*)og, (M_Shape *)n, c ? 1 : 0);
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
			par->scale.y = par->scale.x = FLT2FIX(gr->fontSize * SWF_TEXT_SCALE);
		} else {
			/*don't forget we're flipped at top level...*/
			par->scale.y = -FIX_ONE;
		}
		gf_node_insert_child((GF_Node *)tr, (GF_Node *) par, -1);
		gf_node_register((GF_Node *) par, (GF_Node *)tr);

		if (use_text) {
			u32 _len;
			u16 *str_w, *widestr;
			char *str;
			void *ptr;
			M_Text *t = (M_Text *) s2b_new_node(read, TAG_MPEG4_Text);
			M_FontStyle *f = (M_FontStyle *) s2b_new_node(read, TAG_MPEG4_FontStyle);
			t->fontStyle = (GF_Node *) f;
			gf_node_register(t->fontStyle, (GF_Node *) t);

			/*restore back the font height in pixels (it's currently in SWF glyph design units)*/
			f->size = FLT2FIX(gr->fontSize * SWF_TWIP_SCALE);

			if (ft->fontName) {
				gf_sg_vrml_mf_reset(&f->family, GF_SG_VRML_MFSTRING);
				gf_sg_vrml_mf_append(&f->family, GF_SG_VRML_MFSTRING, &ptr);
				((SFString*)ptr)->buffer = gf_strdup(ft->fontName);
			}
			gf_sg_vrml_mf_reset(&f->justify, GF_SG_VRML_MFSTRING);
			gf_sg_vrml_mf_append(&f->justify, GF_SG_VRML_MFSTRING, &ptr);
			((SFString*)ptr)->buffer = gf_strdup("BEGIN");

			if (f->style.buffer) gf_free(f->style.buffer);
			if (ft->is_italic && ft->is_bold) f->style.buffer = gf_strdup("BOLDITALIC");
			else if (ft->is_bold) f->style.buffer = gf_strdup("BOLD");
			else if (ft->is_italic) f->style.buffer = gf_strdup("ITALIC");
			else f->style.buffer = gf_strdup("PLAIN");

			/*convert to UTF-8*/
			str_w = (u16*)gf_malloc(sizeof(u16) * (gr->nbGlyphs+1));
			for (j=0; j<gr->nbGlyphs; j++) str_w[j] = ft->glyph_codes[gr->indexes[j]];
			str_w[j] = 0;
			str = (char*)gf_malloc(sizeof(char) * (gr->nbGlyphs+2));
			widestr = str_w;
			_len = gf_utf8_wcstombs(str, sizeof(u8) * (gr->nbGlyphs+1), (const unsigned short **) &widestr);
			if (_len != GF_UTF8_FAIL) {
				j = _len;
				str[j] = 0;
				gf_sg_vrml_mf_reset(&t->string, GF_SG_VRML_MFSTRING);
				gf_sg_vrml_mf_append(&t->string, GF_SG_VRML_MFSTRING, &ptr);
				((SFString*)ptr)->buffer = (char*)gf_malloc(sizeof(char) * (j+1));
				memcpy(((SFString*)ptr)->buffer, str, sizeof(char) * (j+1));
			}

			gf_free(str);
			gf_free(str_w);

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
				gf_assert((gf_node_get_tag(gl->geometry)==TAG_MPEG4_Curve2D) || (gf_node_get_tag(gl->geometry)==TAG_MPEG4_XCurve2D));

				gl_par = (M_Transform2D *) s2b_new_node(read, TAG_MPEG4_Transform2D);
				gl->appearance = s2b_get_appearance(read, (GF_Node *) gl, gr->col, 0, 0);

				gl_par->translation.x = gf_divfix(dx, FLT2FIX(gr->fontSize * SWF_TEXT_SCALE) );
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
	if (!content || is_cdata || !sax_cbck) return;
	t = (SWFFlatText *)sax_cbck;
	len = (u32) strlen(content);
	if (!len) return;
	t->final = gf_realloc(t->final, sizeof(char)*(t->len+len+1));
	t->final [t->len] = 0;
	strcat(t->final, content);
	t->len = (u32) strlen(t->final)+1;
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
	if (text->align==3) use_layout = 1;
	else if (text->multiline) use_layout = 1;

	if (use_layout) {
		layout = (M_Layout *) s2b_new_node(read, TAG_MPEG4_Layout);
		tr->translation.x = read->width/2;
		tr->translation.y = read->height/2;
	}

	t = (M_Text *) s2b_new_node(read, TAG_MPEG4_Text);
	f = (M_FontStyle *) s2b_new_node(read, TAG_MPEG4_FontStyle);
	t->fontStyle = (GF_Node *) f;
	gf_node_register(t->fontStyle, (GF_Node *) t);

	/*restore back the font height in pixels (it's currently in SWF glyph design units)*/
	f->size = text->font_height;
	f->spacing = text->font_height + text->leading;

	gf_sg_vrml_mf_reset(&f->justify, GF_SG_VRML_MFSTRING);
	gf_sg_vrml_mf_append(&f->justify, GF_SG_VRML_MFSTRING, (void**)&ptr);
	switch (text->align) {
	case 0:
		((SFString*)ptr)->buffer = gf_strdup("BEGIN");
		break;
	case 1:
		((SFString*)ptr)->buffer = gf_strdup("END");
		break;
	case 3:
		((SFString*)ptr)->buffer = gf_strdup("JUSTIFY");
		break;
	default:
		((SFString*)ptr)->buffer = gf_strdup("MIDDLE");
		break;
	}

	strcpy(styles, "");
	if (!text->read_only) strcat(styles, "EDITABLE");
	if (text->password) strcat(styles, "PASSWORD");

	if (f->style.buffer) gf_free(f->style.buffer);
	f->style.buffer = gf_strdup(styles);

	if (text->init_value) {
		gf_sg_vrml_mf_reset(&t->string, GF_SG_VRML_MFSTRING);
		gf_sg_vrml_mf_append(&t->string, GF_SG_VRML_MFSTRING, (void**)&ptr);

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
				((SFString*)ptr)->buffer = gf_strdup(flat.final);
				gf_free(flat.final);
			}
		} else {
			((SFString*)ptr)->buffer = gf_strdup(text->init_value);
		}
	}


	txt = (M_Shape *) s2b_new_node(read, TAG_MPEG4_Shape);
	txt->appearance = s2b_get_appearance(read, (GF_Node *) txt, text->color, 0, 0);
	txt->geometry = (GF_Node *) t;
	gf_node_register(txt->geometry, (GF_Node *) txt);

	if (layout) {
		gf_sg_vrml_mf_reset(&layout->justify, GF_SG_VRML_MFSTRING);
		gf_sg_vrml_mf_append(&layout->justify, GF_SG_VRML_MFSTRING, NULL);
		switch (text->align) {
		case 0:
			layout->justify.vals[0] = gf_strdup("BEGIN");
			break;
		case 1:
			layout->justify.vals[0] = gf_strdup("END");
			break;
		case 3:
			layout->justify.vals[0] = gf_strdup("JUSTIFY");
			break;
		default:
			layout->justify.vals[0] = gf_strdup("MIDDLE");
			break;
		}
		if (text->multiline || text->word_wrap) layout->wrap = 1;

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
#if 0
	char szDEF[1024];
	u32 i;
	GF_AUContext *au;
	GF_Command *com;
	GF_CommandField *f;
	GF_Node *empty;

	empty = gf_sg_find_node_by_name(read->load->scene_graph, "Shape0");

	au = gf_list_get(read->bifs_es->AUs, 0);
	for (i=0; i<read->max_depth; i++) {
		/*and write command*/
		com = gf_sg_command_new(read->load->scene_graph, GF_SG_INDEXED_REPLACE);
		sprintf(szDEF, "CLIP%d_DL", read->current_sprite_id);
		com->node = gf_sg_find_node_by_name(read->load->scene_graph, szDEF);

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
#endif

}

static Bool swf_bifs_allocate_depth(SWFReader *read, u32 depth)
{
	char szDEF[100];
	GF_Node *disp, *empty;
	if (read->max_depth > depth) return 1;

	sprintf(szDEF, "CLIP%d_DL", read->current_sprite_id);
	disp = gf_sg_find_node_by_name(read->load->scene_graph, szDEF);

	empty = gf_sg_find_node_by_name(read->load->scene_graph, "Shape0");
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
		esd->decoderConfig->objectTypeIndication = GF_CODECID_BIFS;
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
	read->od_es = gf_sm_stream_new(read->load->ctx, 2, GF_STREAM_OD, GF_CODECID_OD_V1);
	if (!read->od_es) return GF_OUT_OF_MEM;

	esd = (GF_ESD *) gf_odf_desc_esd_new(0);
	if (!esd) return GF_OUT_OF_MEM;
	esd->decoderConfig->streamType = GF_STREAM_OD;
	esd->decoderConfig->objectTypeIndication = GF_CODECID_BIFS;
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
	esd->decoderConfig->objectTypeIndication = GF_CODECID_BIFS;
	esd->slConfig->timestampResolution = read->bifs_es->timeScale;
	gf_odf_desc_del((GF_Descriptor *) esd->decoderConfig->decoderSpecificInfo);
	esd->decoderConfig->decoderSpecificInfo = NULL;
	gf_list_add(od->ESDescriptors, esd);

	/*by default insert OD at beginning*/
	e = swf_insert_od(read, 0, od);
	if (e) {
		gf_odf_desc_del((GF_Descriptor *) od);
		return e;
	}

	/*create AS for sprite - all AS are created in initial scene replace*/
	n = s2b_new_node(read, TAG_MPEG4_AnimationStream);
	gf_node_insert_child(read->root, n, 0);
	gf_node_register(n, read->root);
	/*assign URL*/
	gf_node_get_field_by_name(n, "url", &info);
	gf_sg_vrml_mf_alloc(info.far_ptr, info.fieldType, 1);
	((MFURL*)info.far_ptr)->vals[0].OD_ID = od->objectDescriptorID;
	((M_AnimationStream *)n)->startTime = 0;

	n = s2b_new_node(read, TAG_MPEG4_MediaControl);
	sprintf(szDEF, "CLIP%d_CTRL", read->current_sprite_id);
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
	((M_MediaControl *)n)->mediaSpeed = 0;
	((M_MediaControl *)n)->loop = 1;

	/*create sprite grouping node*/
	n = s2b_new_node(read, TAG_MPEG4_Group);
	sprintf(szDEF, "CLIP%d_DL", read->current_sprite_id);

	read->load->ctx->max_node_id++;
	ID = read->load->ctx->max_node_id;
	gf_node_set_id(n, ID, szDEF);
	par = gf_sg_find_node_by_name(read->load->scene_graph, "DICTIONARY");
	gf_assert(par);
	gf_node_list_add_child(&((M_Switch *)par)->choice, n);
	gf_node_register(n, par);
	par = gf_sg_find_node_by_name(read->load->scene_graph, "Shape0");
	gf_node_insert_child(n, par, -1);
	gf_node_register(par, n);

	/*store BIFS context*/
	prev_sc = read->bifs_es;
	prev_au = read->bifs_au;
	/*create new BIFS stream*/
	read->bifs_es = gf_sm_stream_new(read->load->ctx, esd->ESID, GF_STREAM_SCENE, GF_CODECID_BIFS);
	read->bifs_es->timeScale = prev_sc->timeScale;
	read->bifs_es->imp_exp_time = prev_sc->imp_exp_time + prev_au->timing;

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

static GF_Err swf_bifs_setup_sound(SWFReader *read, SWFSound *snd, Bool soundstream_first_block)
{
	GF_Err e;
	GF_ObjectDescriptor *od;
	GF_ESD *esd;
	GF_MuxInfo *mux;
	GF_Node *n, *par;
	GF_FieldInfo info;
	u32 ID;
	char szDEF[100];

	/*soundstream header, only declare the associated MediaControl node for later actions*/
	if (!snd->ID && !soundstream_first_block) {
		n = s2b_new_node(read, TAG_MPEG4_MediaControl);
		sprintf(szDEF, "CLIP%d_SND", read->current_sprite_id);
		read->load->ctx->max_node_id++;
		ID = read->load->ctx->max_node_id;
		gf_node_set_id(n, ID, szDEF);

		gf_node_insert_child(read->root, n, 0);
		gf_node_register(n, read->root);
		return GF_OK;
	}

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
		esd->OCRESID = read->bifs_es ? read->bifs_es->ESID : esd->ESID;
	}
	gf_list_add(od->ESDescriptors, esd);

	/*setup mux info*/
	mux = (GF_MuxInfo*)gf_odf_desc_new(GF_ODF_MUXINFO_TAG);
	mux->file_name = gf_strdup(snd->szFileName);
//	mux->startTime = snd->frame_delay_ms;
	mux->startTime = 0;
	/*MP3 in, destroy file once done*/
	if (snd->format==2) mux->delete_file = 1;
	gf_list_add(esd->extensionDescriptors, mux);


	/*by default insert OD at beginning*/
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
	((M_Sound2D *)par)->source = n;
	gf_node_register(n, par);
	/*assign URL*/
	gf_node_get_field_by_name(n, "url", &info);
	gf_sg_vrml_mf_alloc(info.far_ptr, info.fieldType, 1);
	((MFURL *)info.far_ptr)->vals[0].OD_ID = od->objectDescriptorID;

	((M_AudioClip*)n)->startTime = -1.0;

	/*regular sound: set an ID to do play/stop*/
	if (snd->ID) {
		sprintf(szDEF, "Sound%d", snd->ID);
		read->load->ctx->max_node_id++;
		ID = read->load->ctx->max_node_id;
		gf_node_set_id(n, ID, szDEF);
	}
	/*soundStream - add a MediaControl*/
	else {
		/*if sprite always have the media active but controled by its mediaControl*/
		if (read->current_sprite_id) {
			((M_AudioClip*)n)->startTime = 0;
		}
		/*otherwise start the media at the first soundstream block*/
		else {
			((M_AudioClip*)n)->startTime = snd->frame_delay_ms/1000.0;
		}

		sprintf(szDEF, "CLIP%d_SND", read->current_sprite_id);
		n = gf_sg_find_node_by_name(read->load->scene_graph, szDEF);

		/*assign URL*/
		gf_node_get_field_by_name(n, "url", &info);
		gf_sg_vrml_mf_alloc(info.far_ptr, info.fieldType, 1);
		((MFURL*)info.far_ptr)->vals[0].OD_ID = od->objectDescriptorID;
		((M_MediaControl *)n)->loop = 0;

		/*inactive by default (until inserted)*/
		if (read->current_sprite_id) {
			((M_MediaControl *)n)->mediaSpeed = 0;
		} else {
			((M_MediaControl *)n)->mediaSpeed = FIX_ONE;
		}
	}
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

	mux->file_name = gf_strdup(fileName);
	/*destroy file once done*/
	//mux->delete_file = 1;
	gf_list_add(esd->extensionDescriptors, mux);


	/*by default insert OD at beginning*/
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


static GF_Node *s2b_wrap_node(SWFReader *read, GF_Node *node, GF_Matrix2D *mat, GF_ColorMatrix *cmat)
{
	GF_Node *par;
	if (mat && gf_mx2d_is_identity(*mat)) mat = NULL;
	if (cmat && cmat->identity) cmat = NULL;

	/*then add cmat/mat and node*/
	par = NULL;
	if (!mat && !cmat) {
		par = node;
	} else {
		if (mat) par = s2b_get_matrix(read, mat);
		if (cmat) {
			GF_Node *cm = s2b_get_color_matrix(read, cmat);
			if (!par) {
				par = cm;
				gf_node_insert_child(par, node, -1);
				gf_node_register(node, par);
			} else {
				gf_node_insert_child(par, cm, -1);
				gf_node_register(cm, par);
				gf_node_insert_child(cm, node, -1);
				gf_node_register(node, cm);
			}
		} else {
			gf_node_insert_child(par, node, -1);
			gf_node_register(node, par);
		}
	}
	return par;
}



static void s2b_set_field(SWFReader *read, GF_List *dst, GF_Node *n, char *fieldName, s32 pos, u32 type, void *val, Bool insert)
{
	u32 i, count;
	GF_FieldInfo info;
	GF_Command *com = NULL;
	GF_CommandField *f;

	gf_node_get_field_by_name(n, fieldName, &info);

	count = gf_list_count(dst);
	for (i=0; i<count; i++) {
		com = gf_list_get(dst, i);
		if (com->node!=n) continue;
		f = gf_list_get(com->command_fields, 0);
		if (f->fieldIndex != info.fieldIndex) continue;
		if (f->pos != pos) continue;

		if (insert) return;

		if (type==GF_SG_VRML_SFSTRING) {
			if (((SFString*)f->field_ptr)->buffer)
				gf_free(((SFString*)f->field_ptr)->buffer);
			((SFString*)f->field_ptr)->buffer = gf_strdup( (char *) val);
		} else {
			gf_sg_vrml_field_copy(f->field_ptr, val, type);
		}
		gf_list_rem(dst, i);
		gf_list_add(dst, com);
		return;
	}

	com = gf_sg_command_new(read->load->scene_graph, (pos<0) ? GF_SG_FIELD_REPLACE : GF_SG_INDEXED_REPLACE);
	com->node = n;
	gf_node_register(com->node, NULL);
	f = gf_sg_command_field_new(com);
	f->field_ptr = gf_sg_vrml_field_pointer_new(type);
	if (type==GF_SG_VRML_SFSTRING) {
		((SFString*)f->field_ptr)->buffer = gf_strdup( (char *) val);
	} else {
		gf_sg_vrml_field_copy(f->field_ptr, val, type);
	}
	f->fieldType = type;
	f->pos = pos;
	f->fieldIndex = info.fieldIndex;

	if (insert)
		gf_list_insert(dst, com, 0);
	else
		gf_list_add(dst, com);
}

static GF_Err swf_bifs_set_backcol(SWFReader *read, u32 xrgb)
{
	SFColor rgb;
	GF_Node *bck = gf_sg_find_node_by_name(read->load->scene_graph, "BACKGROUND");

	rgb.red = INT2FIX((xrgb>>16) & 0xFF) / 255;
	rgb.green = INT2FIX((xrgb>>8) & 0xFF) / 255;
	rgb.blue = INT2FIX((xrgb) & 0xFF) / 255;
	s2b_set_field(read, read->bifs_au->commands, bck, "backColor", -1, GF_SG_VRML_SFCOLOR, &rgb, 0);
	return GF_OK;
}

static GF_Err swf_bifs_start_sound(SWFReader *read, SWFSound *snd, Bool stop)
{
	GF_Node *sound2D;
	SFTime t = 0;
	char szDEF[100];

	sprintf(szDEF, "Sound%d", snd->ID);
	sound2D = gf_sg_find_node_by_name(read->load->scene_graph, szDEF);
	/*check flags*/
	if (sound2D)
		s2b_set_field(read, read->bifs_au->commands, sound2D, stop ? "stopTime" : "startTime", -1, GF_SG_VRML_SFTIME, &t, 0);

	return GF_OK;
}

static void s2b_control_sprite(SWFReader *read, GF_List *dst, u32 ID, Bool stop, Bool set_time, SFTime mediaStartTime, Bool rev_order)
{
	u32 i;
	GF_Node *obj;
	char szDEF[100];
	SFFloat t;
	sprintf(szDEF, "CLIP%d_CTRL", ID);
	obj = gf_sg_find_node_by_name(read->load->scene_graph, szDEF);
	if (!obj) return;

	/*filter the current control sequence: if the sprite is marked as started, skip the control. This happens
	when a sprite is used in the different states of a button*/
	for (i=0; i<gf_list_count(dst); i++) {
		GF_Command *com = gf_list_get(dst, i);
		if (com->node==obj) {
			GF_CommandField *f = gf_list_get(com->command_fields, 0);
			if ((f->fieldIndex == 3) && (*((SFFloat*)f->field_ptr) != 0))
				return;
		}
	}


	if (set_time)
		s2b_set_field(read, dst, obj, "mediaStartTime", -1, GF_SG_VRML_SFTIME, &mediaStartTime, rev_order);
	t = stop ? 0 : FIX_ONE;
	s2b_set_field(read, dst, obj, "mediaSpeed", -1, GF_SG_VRML_SFFLOAT, &t, rev_order);

	sprintf(szDEF, "CLIP%d_SND", ID);
	obj = gf_sg_find_node_by_name(read->load->scene_graph, szDEF);
	if (!obj) return;
	if (set_time) {
		mediaStartTime -= read->sound_stream->frame_delay_ms/1000.0;
		if (mediaStartTime<0) mediaStartTime = 0;
		s2b_set_field(read, dst, obj, "mediaStartTime", -1, GF_SG_VRML_SFTIME, &mediaStartTime, rev_order);
	}
	t = stop ? 0 : FIX_ONE;
	s2b_set_field(read, dst, obj, "mediaSpeed", -1, GF_SG_VRML_SFFLOAT, &t, rev_order);
}

static GF_Err swf_bifs_place_obj(SWFReader *read, u32 depth, u32 ID, u32 prev_id, u32 type, GF_Matrix2D *mat, GF_ColorMatrix *cmat, GF_Matrix2D *prev_mat, GF_ColorMatrix *prev_cmat)
{
	GF_Command *com;
	GF_CommandField *f;
	GF_Node *obj, *par;
	char szDEF[100];
	Bool is_sprite;

	obj = s2b_get_node(read, ID);
	is_sprite = 0;
	if (!obj) {
		sprintf(szDEF, "CLIP%d_DL", ID);
		obj = gf_sg_find_node_by_name(read->load->scene_graph, szDEF);
		if (obj) is_sprite = 1;
	}
	if (!obj) return GF_BAD_PARAM;

	/*then add cmat/mat and node*/
	par = s2b_wrap_node(read, obj, mat, cmat);

	/*and write command*/
	com = gf_sg_command_new(read->load->scene_graph, GF_SG_INDEXED_REPLACE);
	sprintf(szDEF, "CLIP%d_DL", read->current_sprite_id);
	com->node = gf_sg_find_node_by_name(read->load->scene_graph, szDEF);
	gf_node_register(com->node, NULL);
	f = gf_sg_command_field_new(com);
	f->field_ptr = &f->new_node;
	f->fieldType = GF_SG_VRML_SFNODE;
	f->pos = depth;
	f->fieldIndex = 2;	/*children index*/
	f->new_node = par;
	gf_node_register(f->new_node, NULL);
	gf_list_add(read->bifs_au->commands, com);

	if (ID==prev_id) return GF_OK;

	strcpy(szDEF, gf_node_get_name(obj));
	/*when inserting a button, trigger a pause*/
	if (!strnicmp(szDEF, "Button", 6)) {
		u32 i, count;
		s2b_control_sprite(read, read->bifs_au->commands, read->current_sprite_id, 1, 0, 0, 1);

		count = gf_list_count(read->buttons);
		for (i=0; i<count; i++) {
			S2BBtnRec *btnrec = gf_list_get(read->buttons, i);
			if (btnrec->btn_id==ID) {
				s2b_control_sprite(read, read->bifs_au->commands, btnrec->sprite_up_id, 0, 0, 0, 1);
			}
		}
	}
	/*starts anim*/
	else if (is_sprite) {
		s2b_control_sprite(read, read->bifs_au->commands, ID, 0, 1, 0, 0);
		if (prev_id) {
			s2b_control_sprite(read, read->bifs_au->commands, prev_id, 1, 0, 0, 0);
		}
	}
	return GF_OK;
}

static GF_Err swf_bifs_remove_obj(SWFReader *read, u32 depth, u32 ID)
{
	char szDEF[100];
	GF_Command *com;
	GF_CommandField *f;

	com = gf_sg_command_new(read->load->scene_graph, GF_SG_INDEXED_REPLACE);
	sprintf(szDEF, "CLIP%d_DL", read->current_sprite_id);
	com->node = gf_sg_find_node_by_name(read->load->scene_graph, szDEF);
	gf_node_register(com->node, NULL);
	f = gf_sg_command_field_new(com);
	f->field_ptr = &f->new_node;
	f->fieldType = GF_SG_VRML_SFNODE;
	f->pos = depth;
	f->fieldIndex = 2;	/*children index*/
	f->new_node = gf_sg_find_node_by_name(read->load->scene_graph, "Shape0");
	gf_node_register(f->new_node, NULL);
	gf_list_add(read->bifs_au->commands, com);
	/*check if this is a sprite*/
	if (ID)
		s2b_control_sprite(read, read->bifs_au->commands, ID, 1, 0, 0, 0);
	return GF_OK;
}

static GF_Err swf_bifs_show_frame(SWFReader *read)
{
	u32 ts;
	Bool is_rap;

	/*hack to allow for empty BIFS AU to be encoded in order to keep the frame-rate (this reduces MP4 table size...)
	todo: make this an option - commented for now in both master and filters*/
#if 0
	if (!gf_list_count(read->bifs_au->commands)) {
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
#endif

	/*create a new AU for next frame*/
	ts = (read->current_frame + 1) * 100;

#if 1
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

	/*not in a sprite def, using a control stream and no wait_frame: create a new AU for new symbols
	*/
	if (!read->current_sprite_id) {
		if (read->bifs_dict_au && !read->wait_frame) {
			read->bifs_dict_au = gf_sm_stream_au_new(read->bifs_dict_es, ts, 0, 0);
		}
		/*if wait_frame is specified, aggregate all dictionary commands until frame is reached*/
		if (read->wait_frame && read->wait_frame<=read->current_frame)
			read->wait_frame = 0;
	}

	return GF_OK;
}


static GF_Node *s2b_button_add_child(SWFReader *read, GF_Node *button, u32 tag, char *def_name, s32 pos)
{
	GF_Node *n = s2b_new_node(read, tag);

	if (def_name) {
		u32 ID;
		read->load->ctx->max_node_id++;
		ID = read->load->ctx->max_node_id;
		gf_node_set_id((GF_Node *)n, ID, def_name);
	}

	gf_node_insert_child((GF_Node *)button, (GF_Node *)n, pos);
	gf_node_register((GF_Node *) n, (GF_Node *) button);
	return n;
}

static void s2b_button_add_route(SWFReader *read, GF_Node *fromNode, u32 fromField, GF_Node *toNode, u32 toField)
{
	GF_Command *com = gf_sg_command_new(read->load->scene_graph, GF_SG_ROUTE_INSERT);
	com->fromNodeID = gf_node_get_id(fromNode);
	com->fromFieldIndex = fromField;
	com->toNodeID = gf_node_get_id(toNode);
	com->toFieldIndex = toField;

	if (read->bifs_dict_au)
		gf_list_add(read->bifs_dict_au->commands, com);
	else
		gf_list_add(read->bifs_au->commands, com);
}


static GF_Err swf_bifs_define_button(SWFReader *read, SWF_Button *btn)
{
	char szName[1024];
	M_Switch *button;
	SWF_ButtonRecord *br;
	GF_Node *btn_root, *n, *btn_ts;
	u32 i, ID, pos;

	if (!btn) {
		read->btn = NULL;
		read->btn_over = read->btn_not_over = read->btn_active = read->btn_not_active = NULL;
		return GF_OK;
	}

	read->btn = btn;

	btn_root = s2b_new_node(read, TAG_MPEG4_Transform2D);
	sprintf(szName, "Button%d", btn->ID);
	read->load->ctx->max_node_id++;
	ID = read->load->ctx->max_node_id;
	gf_node_set_id((GF_Node *)btn_root, ID, szName);

	n = s2b_button_add_child(read, btn_root, TAG_MPEG4_ColorTransform, NULL, -1);
	((M_ColorTransform*)n)->maa = ((M_ColorTransform*)n)->mab = ((M_ColorTransform*)n)->mar = ((M_ColorTransform*)n)->mag = ((M_ColorTransform*)n)->ta = 0;

	/*locate hit buttons and add them to the color transform*/
	for (i=0; i<btn->count; i++) {
		GF_Node *character;
		br = &btn->buttons[i];
		if (!br->hitTest) continue;
		character = s2b_get_node(read, br->character_id);
		if (!character) {
			sprintf(szName, "CLIP%d_DL", br->character_id);
			character = gf_sg_find_node_by_name(read->load->scene_graph, szName);
		}
		if (character) {
			gf_node_list_add_child(&((GF_ParentNode*)n)->children, character);
			gf_node_register(character, (GF_Node *)n);
		}
	}
	/*add touch sensor to the color transform*/
	sprintf(szName, "BTN%d_TS", read->btn->ID);
	btn_ts = s2b_button_add_child(read, n, TAG_MPEG4_TouchSensor, szName, -1);

	s2b_insert_symbol(read, (GF_Node *)btn_root);

	/*isActive handler*/
	sprintf(szName, "BTN%d_CA", read->btn->ID);
	n = s2b_button_add_child(read, btn_root, TAG_MPEG4_Conditional, szName, -1);
	read->btn_active = ((M_Conditional*)n)->buffer.commandList;
	s2b_button_add_route(read, btn_ts, 4, n, 0);

	/*!isActive handler*/
	sprintf(szName, "BTN%d_CNA", read->btn->ID);
	n = s2b_button_add_child(read, btn_root, TAG_MPEG4_Conditional, szName, -1);
	read->btn_not_active = ((M_Conditional*)n)->buffer.commandList;
	s2b_button_add_route(read, btn_ts, 4, n, 1);

	/*isOver handler*/
	sprintf(szName, "BTN%d_CO", read->btn->ID);
	n = s2b_button_add_child(read, btn_root, TAG_MPEG4_Conditional, szName, -1);
	read->btn_over = ((M_Conditional*)n)->buffer.commandList;
	s2b_button_add_route(read, btn_ts, 5, n, 0);

	/*!isOver handler*/
	sprintf(szName, "BTN%d_CNO", read->btn->ID);
	n = s2b_button_add_child(read, btn_root, TAG_MPEG4_Conditional, szName, -1);
	read->btn_not_over = ((M_Conditional*)n)->buffer.commandList;
	s2b_button_add_route(read, btn_ts, 5, n, 1);

	/*by default show first character*/
	pos = 0;
	for (i=0; i<btn->count; i++) {
		GF_Node *sprite_ctrl = NULL;
		GF_Node *character;
		br = &btn->buttons[i];
		if (!br->up && !br->down && !br->over) continue;

		character = s2b_get_node(read, br->character_id);

		if (!character) {
			sprintf(szName, "CLIP%d_DL", br->character_id);
			character = gf_sg_find_node_by_name(read->load->scene_graph, szName);
			if (character) {
				sprintf(szName, "CLIP%d_CTRL", br->character_id);
				sprite_ctrl = gf_sg_find_node_by_name(read->load->scene_graph, szName);
			}
		}
		if (character) {
			SFInt32 choice = 0;
			n = s2b_wrap_node(read, character, &br->mx, &br->cmx);

			sprintf(szName, "BTN%d_R%d", btn->ID, i+1);
			button = (M_Switch *) s2b_button_add_child(read, btn_root, TAG_MPEG4_Switch, szName, pos);
			pos++;

			gf_node_list_add_child(&button->choice, n);
			gf_node_register(n, (GF_Node *)button);
			/*initial state*/
			if (br->up) {
				button->whichChoice = 0;
				/*register this button for sprite start upon place_obj*/
				if (sprite_ctrl) {
					S2BBtnRec *btnrec;
					if (!read->buttons) read->buttons = gf_list_new();
					btnrec = gf_malloc(sizeof(S2BBtnRec));
					btnrec->btn_id = btn->ID;
					btnrec->sprite_up_id = br->character_id;
					gf_list_add(read->buttons, btnrec);
				}

			} else {
				button->whichChoice = -1;
			}

			choice = br->up ? 0 : -1;
			s2b_set_field(read, read->btn_not_over, (GF_Node *)button, "whichChoice", -1, GF_SG_VRML_SFINT32, &choice, 0);
			/*start or stop sprite if button is up or not*/
			if (sprite_ctrl) {
				s2b_control_sprite(read, read->btn_not_over, br->character_id, choice, 1, 0, 0);
			}

			choice = br->down ? 0 : -1;
			s2b_set_field(read, read->btn_active, (GF_Node *)button, "whichChoice", -1, GF_SG_VRML_SFINT32, &choice, 0);
			if (sprite_ctrl && !br->over) {
				s2b_control_sprite(read, read->btn_active, br->character_id, choice, 1, 0, 0);
			}

			choice = br->over ? 0 : -1;
			s2b_set_field(read, read->btn_not_active, (GF_Node *)button, "whichChoice", -1, GF_SG_VRML_SFINT32, &choice, 0);
			s2b_set_field(read, read->btn_over, (GF_Node *)button, "whichChoice", -1, GF_SG_VRML_SFINT32, &choice, 0);
			if (sprite_ctrl) {
				s2b_control_sprite(read, read->btn_over, br->character_id, choice, 1, 0, 0);
				if (!br->down)
					s2b_control_sprite(read, read->btn_not_active, br->character_id, choice, 1, 0, 0);
			}
		}
	}

	return GF_OK;
}

static void swf_bifs_finalize(SWFReader *read, Bool is_destroy)
{
	u32 i, count;
	if (!is_destroy)
		swf_bifs_end_of_clip(read);

	while (gf_list_count(read->buttons)) {
		S2BBtnRec *btnrec = gf_list_get(read->buttons, 0);
		gf_list_rem(read->buttons, 0);
		gf_free(btnrec);
	}
	gf_list_del(read->buttons);
	read->buttons = NULL;

	count = gf_list_count(read->fonts);
	for (i=0; i<count; i++) {
		SWFFont *ft = (SWFFont *)gf_list_get(read->fonts, i);
		while (gf_list_count(ft->glyphs)) {
			GF_Node *gl = (GF_Node *)gf_list_get(ft->glyphs, 0);
			gf_list_rem(ft->glyphs, 0);
			gf_node_unregister(gl, NULL);
		}
	}
}

Bool swf_bifs_action(SWFReader *read, SWFAction *act)
{
	GF_List *dst;
	MFURL url;
	SFURL sfurl;
	Bool bval;
	GF_Node *n;
	Double time;

	dst = read->bifs_au->commands;
	if (read->btn) {
		if (act->button_mask & GF_SWF_COND_OVERUP_TO_OVERDOWN) dst = read->btn_active;
		else if (act->button_mask & GF_SWF_COND_IDLE_TO_OVERUP) dst = read->btn_over;
		else if (act->button_mask & GF_SWF_COND_OVERUP_TO_IDLE) dst = read->btn_not_over;
		else dst = read->btn_not_active;
	}

	switch (act->type) {
	case GF_SWF_AS3_WAIT_FOR_FRAME:
		/*while correct, this is not optimal, we set the wait-frame upon GOTO frame*/
//		read->wait_frame = act->frame_number;
		break;
	case GF_SWF_AS3_GOTO_FRAME:
		if (act->frame_number>read->current_frame)
			read->wait_frame = act->frame_number;

		time = act->frame_number ? act->frame_number +1: 0;
		time /= read->frame_rate;
		s2b_control_sprite(read, dst, read->current_sprite_id, 0, 1, time, 0);
		break;
	case GF_SWF_AS3_GET_URL:
		n = gf_sg_find_node_by_name(read->load->scene_graph, "MOVIE_URL");
		sfurl.OD_ID = 0;
		sfurl.url = act->url;
		url.count = 1;
		url.vals = &sfurl;
		s2b_set_field(read, dst, n, "url", -1, GF_SG_VRML_MFURL, &url, 0);
		bval = 1;
		s2b_set_field(read, dst, n, "activate", -1, GF_SG_VRML_SFBOOL, &bval, 0);
		break;
	case GF_SWF_AS3_PLAY:
		s2b_control_sprite(read, dst, read->current_sprite_id, 0, 1, -1, 0);
		break;
	case GF_SWF_AS3_STOP:
		s2b_control_sprite(read, dst, read->current_sprite_id, 1, 0, 0, 0);
		break;
	default:
		return 0;
	}

	return 1;
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
	read->action = swf_bifs_action;
	read->finalize = swf_bifs_finalize;

#ifdef GPAC_ENABLE_COVERAGE
	if (gf_sys_is_cov_mode()) {
		swf_nstart(NULL, NULL, NULL, NULL, 0);
		swf_nend(NULL, NULL, NULL);
		swf_ntext(NULL, NULL, GF_FALSE);
	}
#endif

	/*create BIFS stream*/
	read->bifs_es = gf_sm_stream_new(read->load->ctx, 1, GF_STREAM_SCENE, GF_CODECID_BIFS);
	read->bifs_es->timeScale = read->frame_rate*100;

	read->bifs_au = gf_sm_stream_au_new(read->bifs_es, 0, 0.0, 1);
	/*create scene replace command*/
	com = gf_sg_command_new(read->load->scene_graph, GF_SG_SCENE_REPLACE);
	read->load->ctx->scene_width = FIX2INT(read->width);
	read->load->ctx->scene_height = FIX2INT(read->height);
	read->load->ctx->is_pixel_metrics = 1;

	gf_list_add(read->bifs_au->commands, com);

	/*create base tree*/
	com->node = read->root = s2b_new_node(read, TAG_MPEG4_OrderedGroup);
	gf_node_register(read->root, NULL);

	/*hehehe*/
	n = s2b_new_node(read, TAG_MPEG4_WorldInfo);
	gf_node_insert_child(read->root, n, -1);
	gf_node_register(n, read->root);
	((M_WorldInfo *)n)->title.buffer = gf_strdup("GPAC SWF CONVERTION DISCLAIMER");
	gf_sg_vrml_mf_alloc( & ((M_WorldInfo *)n)->info, GF_SG_VRML_MFSTRING, 2);

	sprintf(szMsg, "%s file converted to MPEG-4 Systems", read->load->fileName);
	((M_WorldInfo *)n)->info.vals[0] = gf_strdup(szMsg);
	if (gf_sys_is_test_mode()) {
		sprintf(szMsg, "Conversion done using GPAC");
	} else {
		sprintf(szMsg, "Conversion done using GPAC version %s - %s", gf_gpac_version(), gf_gpac_copyright() );
	}
	((M_WorldInfo *)n)->info.vals[1] = gf_strdup(szMsg);

	/*background*/
	n = s2b_new_node(read, TAG_MPEG4_Background2D);
	((M_Background2D *)n)->backColor.red = FIX_ONE;
	((M_Background2D *)n)->backColor.green = FIX_ONE;
	((M_Background2D *)n)->backColor.blue = FIX_ONE;
	gf_node_set_id(n, 1, "BACKGROUND");
	gf_node_insert_child(read->root, n, -1);
	gf_node_register(n, read->root);

	/*movie anchor*/
	n = s2b_new_node(read, TAG_MPEG4_Anchor);
	gf_node_set_id(n, 2, "MOVIE_URL");
	gf_node_insert_child(read->root, n, -1);
	gf_node_register(n, read->root);

	/*dictionary*/
	n = s2b_new_node(read, TAG_MPEG4_Switch);
	gf_node_set_id(n, 3, "DICTIONARY");
	gf_node_insert_child(read->root, n, -1);
	gf_node_register(n, read->root);
	/*empty shape to fill depth levels & sprites roots*/
	n2 = s2b_new_node(read, TAG_MPEG4_Shape);
	gf_node_set_id(n2, 4, "Shape0");
	gf_node_list_add_child( &((M_Switch *)n)->choice, n2);
	gf_node_register(n2, n);

	/*display list*/
	n = s2b_new_node(read, TAG_MPEG4_Transform2D);
	gf_node_set_id(n, 5, "CLIP0_DL");
	gf_node_insert_child(read->root, n, -1);
	gf_node_register(n, read->root);
	/*update w/h transform*/
	((M_Transform2D *)n)->scale.y = -FIX_ONE;
	((M_Transform2D *)n)->translation.x = -read->width/2;
	((M_Transform2D *)n)->translation.y = read->height/2;

	read->load->ctx->max_node_id = 5;


	swf_init_od(read, 1);

	/*always reserve ES_ID=2 for OD stream, 3 for main ctrl stream if any*/
	read->prev_es_id = 3;
	/*always reserve OD_ID=1 for main ctrl stream if any - use same IDs are ESs*/
	read->prev_od_id = 3;


	/*setup IndexedCurve2D proto*/
	if (read->flags & GF_SM_SWF_USE_IC2D) {
		GF_ProtoFieldInterface *pfield;
		SFURL *url;
		Fixed ftMin, ftMax;
		GF_Proto *proto = gf_sg_proto_new(read->load->scene_graph, 1, "IndexedCurve2D", 0);
		if (read->load->ctx) read->load->ctx->max_proto_id = 1;
		gf_sg_vrml_mf_reset(&proto->ExternProto, GF_SG_VRML_MFURL);
		gf_sg_vrml_mf_append(&proto->ExternProto, GF_SG_VRML_MFURL, (void **) &url);
		url->url = gf_strdup("urn:inet:gpac:builtin:IndexedCurve2D");

		gf_sg_proto_field_new(proto, GF_SG_VRML_SFNODE, GF_SG_EVENT_EXPOSED_FIELD, "coord");

		pfield = gf_sg_proto_field_new(proto, GF_SG_VRML_SFFLOAT, GF_SG_EVENT_EXPOSED_FIELD, "fineness");
		gf_sg_proto_field_get_field(pfield, &info);
		*((SFFloat*)info.far_ptr) = FIX_ONE/2;

		pfield = gf_sg_proto_field_new(proto, GF_SG_VRML_MFINT32, GF_SG_EVENT_EXPOSED_FIELD, "type");
		ftMin = 0;
		ftMax = 15*FIX_ONE;
		gf_bifs_proto_field_set_aq_info(pfield, 13, 1, GF_SG_VRML_SFINT32, &ftMin, &ftMax, 4);

		pfield = gf_sg_proto_field_new(proto, GF_SG_VRML_MFINT32, GF_SG_EVENT_EXPOSED_FIELD, "coordIndex");
		ftMin = 0;
		ftMax = FIX_MAX;
		gf_bifs_proto_field_set_aq_info(pfield, 14, 1, GF_SG_VRML_SFINT32, &ftMin, &ftMax, 0);
	}

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
	esd->decoderConfig->objectTypeIndication = GF_CODECID_BIFS;
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
	read->bifs_es = gf_sm_stream_new(read->load->ctx, esd->ESID, GF_STREAM_SCENE, GF_CODECID_BIFS);
	read->bifs_es->timeScale = prev_sc->timeScale;
	/*create first AU*/
	read->bifs_au = gf_sm_stream_au_new(read->bifs_es, 0, 0, 1);

	/*setup the animationStream node*/
	n = s2b_new_node(read, TAG_MPEG4_AnimationStream);
	gf_node_insert_child(read->root, n, 0);
	gf_node_register(n, read->root);
	/*assign URL*/
	gf_node_get_field_by_name(n, "url", &info);
	gf_sg_vrml_mf_alloc(info.far_ptr, info.fieldType, 1);
	((MFURL*)info.far_ptr)->vals[0].OD_ID = 1;
	/*run from start*/
	((M_AnimationStream *)n)->startTime = 0;
	((M_AnimationStream *)n)->loop = 0;

	/*setup the MediaControl node*/
	n = s2b_new_node(read, TAG_MPEG4_MediaControl);
	read->load->ctx->max_node_id++;
	ID = read->load->ctx->max_node_id;
	gf_node_set_id(n, ID, "CLIP0_CTRL");
	gf_node_insert_child(read->root, n, 0);
	gf_node_register(n, read->root);
	/*assign URL*/
	gf_node_get_field_by_name(n, "url", &info);
	gf_sg_vrml_mf_alloc(info.far_ptr, info.fieldType, 1);
	((MFURL*)info.far_ptr)->vals[0].OD_ID = 1;
	/*run from start*/
	((M_MediaControl *)n)->loop = 0;

	return GF_OK;
}

#endif /*GPAC_DISABLE_SWF_IMPORT*/

#endif /*GPAC_DISABLE_VRML*/

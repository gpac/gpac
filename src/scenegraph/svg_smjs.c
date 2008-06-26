/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean le Feuvre
 *				Copyright (c) 2005-200X ENST
 *					All rights reserved
 *
 *  This file is part of GPAC / Scene Graph sub-project
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

#include <gpac/internal/scenegraph_dev.h>

#ifndef GPAC_DISABLE_SVG
#include <gpac/events.h>
#include <gpac/nodes_svg_da.h>


#ifdef GPAC_HAS_SPIDERMONKEY

#include <jsapi.h> 

jsval dom_element_construct(JSContext *c, GF_Node *n);
GF_Node *dom_get_node(JSContext *c, JSObject *obj);

JSBool dom_event_add_listener(JSContext *c, JSObject *obj, uintN argc, jsval *argv, jsval *rval);
JSBool dom_event_remove_listener(JSContext *c, JSObject *obj, uintN argc, jsval *argv, jsval *rval);

void dom_node_set_textContent(GF_Node *n, char *text);
char *dom_node_flatten_text(GF_Node *n);


#define uDOM_SETUP_CLASS(the_class, cname, flag, getp, setp, fin)	\
	memset(&the_class, 0, sizeof(the_class));	\
	the_class.name = cname;	\
	the_class.flags = flag;	\
	the_class.addProperty = JS_PropertyStub;	\
	the_class.delProperty = JS_PropertyStub;	\
	the_class.getProperty = getp;	\
	the_class.setProperty = setp;	\
	the_class.enumerate = JS_EnumerateStub;	\
	the_class.resolve = JS_ResolveStub;		\
	the_class.convert = JS_ConvertStub;		\
	the_class.finalize = fin;	



#define _ScriptMessage(_sg, _e, _msg) {\
			GF_JSAPIParam par;	\
			par.info.e = _e;			\
			par.info.msg = _msg;		\
			_sg->script_action(_sg->script_action_cbck, GF_JSAPI_OP_MESSAGE, NULL, &par);\
		}

static GFINLINE Bool ScriptAction(GF_SceneGraph *scene, u32 type, GF_Node *node, GF_JSAPIParam *param)
{
	if (scene->script_action) 
		return scene->script_action(scene->script_action_cbck, type, node, param);
	return 0;
}

typedef struct
{
	u32 nb_inst;
	/*SVG uDOM classes*/
	JSClass globalClass;
	JSClass connectionClass;
	JSClass rgbClass;
	JSClass rectClass;
	JSClass pointClass;
	JSClass pathClass;
	JSClass matrixClass;
} GF_SVGuDOM;
static GF_SVGuDOM *svg_rt = NULL;

static JSObject *svg_new_path_object(JSContext *c, SVG_PathData *d);


static void svg_node_changed(GF_Node *n, GF_FieldInfo *info)
{
	if (!info) {
		gf_node_changed(n, NULL);
	} else {
		u32 flag = gf_svg_get_modification_flags((SVG_Element *)n, info);
		gf_node_dirty_set(n, flag, 0);
	}
	/*trigger rendering*/
	if (n->sgprivate->scenegraph->NodeCallback)
		n->sgprivate->scenegraph->NodeCallback(n->sgprivate->scenegraph->userpriv, GF_SG_CALLBACK_MODIFIED, n, info);
}


/*note we are using float to avoid conversions fixed to/from JS native */
typedef struct
{
	u32 r, g, b;
} rgbCI;


typedef struct
{
	Float x, y, w, h;
	/*if set, this is the svg.viewport uDOM object, its values are updated upon query*/
	GF_SceneGraph *sg;
} rectCI;

typedef struct
{
	Float x, y;
	/*if set, this is the svg.currentTranslate uDOM object, its values are updated upon query*/
	GF_SceneGraph *sg;
} pointCI;

typedef struct
{
	Float x, y;
} ptCI;

typedef struct
{
	u32 nb_coms;
	u8 *tags;
	ptCI *pts;
} pathCI;

static JSBool svg_connection_create(JSContext *c, JSObject *obj, uintN argc, jsval *argv, jsval *rval);

static JSBool svg_nav_to_location(JSContext *c, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	GF_JSAPIParam par;
	GF_SceneGraph *sg;
	if ((argc!=1) || !JS_InstanceOf(c, obj, &svg_rt->globalClass, NULL)) return JS_FALSE;
	sg = JS_GetContextPrivate(c);
	par.uri.url = JS_GetStringBytes(JSVAL_TO_STRING(argv[0]));
	par.uri.nb_params = 0;
	ScriptAction(sg, GF_JSAPI_OP_LOAD_URL, sg->RootNode, &par);
	return JS_FALSE;
}

static void svg_script_error(JSContext *c, const char *msg, JSErrorReport *jserr)
{
	GF_SceneGraph *sg = JS_GetContextPrivate(c);
	_ScriptMessage(sg, GF_SCRIPT_ERROR, msg);
}

static JSBool svg_echo(JSContext *c, JSObject *p, uintN argc, jsval *argv, jsval *rval)
{
	u32 i;
	char buf[5000];
	GF_SceneGraph *sg = JS_GetContextPrivate(c);
	if (!sg) return JS_FALSE;

	strcpy(buf, "");
	for (i = 0; i < argc; i++) {
		JSString *str = JS_ValueToString(c, argv[i]);
		if (!str) return JS_FALSE;
		if (i) strcat(buf, " ");
		strcat(buf, JS_GetStringBytes(str));
	}
	_ScriptMessage(sg, GF_SCRIPT_INFO, buf);
	return JS_TRUE;
}
static JSBool global_getProperty(JSContext *c, JSObject *obj, jsval id, jsval *vp)
{
	GF_SceneGraph *sg;
	if (!JS_InstanceOf(c, obj, &svg_rt->globalClass, NULL) ) return JS_FALSE;
	sg = JS_GetContextPrivate(c);
	if (JSVAL_IS_INT(id)) {
		switch (JSVAL_TO_INT(id)) {
		/*namespaceURI*/
		case 0: 
			return JS_TRUE;
		/*parent*/
		case 1: 
			*vp = JSVAL_VOID;
			if (sg->parent_scene && sg->parent_scene->svg_js) *vp = OBJECT_TO_JSVAL(sg->parent_scene->svg_js->global);
			return JS_TRUE;
		default:
			return JS_FALSE;
		}
	}
	return JS_TRUE;
}

/*TODO - try to be more precise...*/
static JSBool dom_imp_has_feature(JSContext *c, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	*rval = BOOLEAN_TO_JSVAL(0);
	if (argc) {
		u32 len;
		char sep;
		char *fname = JS_GetStringBytes(JS_ValueToString(c, argv[0]));
		if (!fname) return JS_FALSE;
		while (strchr(" \t\n\r", fname[0])) fname++;
		len = strlen(fname);
		while (len && strchr(" \t\n\r", fname[len-1])) len--;
		sep = fname[len];
		fname[len] = 0;
		if (!stricmp(fname, "xml")) *rval = BOOLEAN_TO_JSVAL(1);
		else if (!stricmp(fname, "core")) *rval = BOOLEAN_TO_JSVAL(1);
		else if (!stricmp(fname, "traversal")) *rval = BOOLEAN_TO_JSVAL(1);
		else if (!stricmp(fname, "uievents")) *rval = BOOLEAN_TO_JSVAL(1);
		else if (!stricmp(fname, "mouseevents")) *rval = BOOLEAN_TO_JSVAL(1);
		else if (!stricmp(fname, "mutationevents")) *rval = BOOLEAN_TO_JSVAL(1);
		else if (!stricmp(fname, "events")) *rval = BOOLEAN_TO_JSVAL(1);
		
		fname[len] = sep;
	}
	return JS_TRUE;
}

static Bool svg_udom_smil_check_instance(JSContext *c, JSObject *obj)
{
	GF_Node *n = dom_get_node(c, obj);
	if (!n) return 0;
	switch (n->sgprivate->tag) {
	case TAG_SVG_animate:
	case TAG_SVG_animateColor:
	case TAG_SVG_animateMotion:
	case TAG_SVG_animateTransform:
	case TAG_SVG_animation:
	case TAG_SVG_audio:
	case TAG_SVG_video:
	case TAG_SVG_set:
	/*not sure about this one...*/
	case TAG_SVG_discard:
		return 1;
	}
	return 0;
}


/*TODO*/
JSBool svg_udom_smil_begin(JSContext *c, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	if (!svg_udom_smil_check_instance(c, obj) ) return JS_FALSE;
	return JS_FALSE;
}
/*TODO*/
JSBool svg_udom_smil_end(JSContext *c, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	if (!svg_udom_smil_check_instance(c, obj) ) return JS_FALSE;
	return JS_FALSE;
}
/*TODO*/
JSBool svg_udom_smil_pause(JSContext *c, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	if (!svg_udom_smil_check_instance(c, obj) ) return JS_FALSE;
	return JS_FALSE;
}
/*TODO*/
JSBool svg_udom_smil_resume(JSContext *c, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	if (!svg_udom_smil_check_instance(c, obj) ) return JS_FALSE;
	return JS_FALSE;
}

JSBool svg_udom_get_trait(JSContext *c, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	char attValue[1024];
	GF_Err e;
	GF_FieldInfo info;
	GF_Node *n = dom_get_node(c, obj);
	if (!n) return JS_FALSE;

	if (!JSVAL_IS_STRING(argv[0])) return JS_FALSE;
	if (argc==2) {
		char fullName[1024];
		if (!JSVAL_IS_STRING(argv[1])) return JS_FALSE;

		strcpy(fullName, JS_GetStringBytes(JSVAL_TO_STRING(argv[0])) );
		strcat(fullName, ":");
		strcat(fullName, JS_GetStringBytes(JSVAL_TO_STRING(argv[1])) );
		e = gf_node_get_field_by_name(n, fullName, &info);
	} else if (argc==1) {
		char *name = JS_GetStringBytes(JSVAL_TO_STRING(argv[0]));
		if (!strcmp(name, "#text")) {
			char *res = dom_node_flatten_text(n);
			*rval = STRING_TO_JSVAL( JS_NewStringCopyZ(c, res) );
			free(res);
			return JS_TRUE;
		}
		e = gf_node_get_field_by_name(n, name, &info);
	} else return JS_FALSE;

	if (e!=GF_OK) return JS_FALSE;
	*rval = JSVAL_VOID;

	switch (info.fieldType) {
	/* inheritable floats */
	case SVG_FontSize_datatype:
	case SVG_Color_datatype:
	case SVG_Paint_datatype:
	/* inheritable float and unit */
	case SVG_Length_datatype:
	case SVG_Coordinate_datatype:
	/*Number*/
	case SVG_Number_datatype:

/*all string traits*/
	case SVG_Boolean_datatype:
	case SVG_FillRule_datatype:
	case SVG_StrokeLineJoin_datatype:
	case SVG_StrokeLineCap_datatype:
	case SVG_FontStyle_datatype:
	case SVG_FontWeight_datatype:
	case SVG_FontVariant_datatype:
	case SVG_TextAnchor_datatype:
	case SVG_Display_datatype:
	case SVG_Visibility_datatype:
	case SVG_GradientUnit_datatype:
	case SVG_PreserveAspectRatio_datatype:
	case XML_Space_datatype:
	case XMLEV_Propagate_datatype:
	case XMLEV_DefaultAction_datatype:
	case XMLEV_Phase_datatype:
	case SMIL_SyncBehavior_datatype:
	case SMIL_SyncTolerance_datatype:
	case SMIL_AttributeType_datatype:
	case SMIL_CalcMode_datatype:
	case SMIL_Additive_datatype:
	case SMIL_Accumulate_datatype:
	case SMIL_Restart_datatype:
	case SMIL_Fill_datatype:
	case SVG_Overflow_datatype:
	case SVG_ZoomAndPan_datatype:
	case SVG_DisplayAlign_datatype:
	case SVG_TextAlign_datatype:
	case SVG_PointerEvents_datatype:
	case SVG_RenderingHint_datatype:
	case SVG_VectorEffect_datatype:
	case SVG_PlaybackOrder_datatype:
	case SVG_TimelineBegin_datatype:
/*end of string traits*/
/*DOM string traits*/
	case SVG_FontFamily_datatype:
	case XMLRI_datatype:
	case SVG_String_datatype:
	case SVG_ContentType_datatype:
	case SVG_LanguageID_datatype:
	case SVG_Focus_datatype:
	case SVG_ID_datatype:
	case SVG_GradientOffset_datatype:
/*end of DOM string traits*/
		gf_svg_dump_attribute(n, &info, attValue);
		*rval = STRING_TO_JSVAL( JS_NewStringCopyZ(c, attValue) );
		return JS_TRUE;
		/*dump to trait*/
		break;

#if 0
/*SVGT 1.2 default traits*/
	case SMIL_KeyTimes_datatype:
	case SMIL_KeyPoints_datatype:
	case SMIL_KeySplines_datatype:
	case SVG_Coordinates_datatype:
	case SVG_StrokeDashArray_datatype:
	case SVG_Points_datatype:
	case SVG_Motion_datatype:
/*end SVGT 1.2 default traits*/

/*unimplemented/unnkown/FIXME traits*/
	case SMIL_SyncTolerance_datatype:
	case SVG_TransformType_datatype:
	case SVG_TransformList_datatype:
	case SMIL_AnimateValue_datatype:
	case SMIL_AnimateValues_datatype:
	case SMIL_AttributeName_datatype:
	case SMIL_Times_datatype:
	case SMIL_Duration_datatype:
	case SMIL_RepeatCount_datatype:
	default:
/*end unimplemented/unnkown/FIXME traits*/
		return JS_FALSE;
#endif
	}
	return JS_FALSE;
}

JSBool svg_udom_get_float_trait(JSContext *c, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	char *szName;
	GF_FieldInfo info;
	GF_Node *n = dom_get_node(c, obj);
	if (!n) return JS_FALSE;

	if (argc!=1) return JS_FALSE;
	if (!JSVAL_IS_STRING(argv[0])) return JS_FALSE;
	szName = JS_GetStringBytes(JSVAL_TO_STRING(argv[0]));
	*rval = JSVAL_VOID;
	if (gf_node_get_field_by_name(n, szName, &info) != GF_OK) return JS_FALSE;

	switch (info.fieldType) {
	/* inheritable floats */
	case SVG_Number_datatype:
	case SVG_FontSize_datatype:
	case SVG_Length_datatype:
	case SVG_Coordinate_datatype:
	{
		SVG_Number *l = (SVG_Number *)info.far_ptr;
		if (l->type==SVG_NUMBER_AUTO || l->type==SVG_NUMBER_INHERIT) return JS_FALSE;
		*rval = DOUBLE_TO_JSVAL( JS_NewDouble(c, FIX2FLT(l->value) ) );
		return JS_TRUE;
	}
	default:
		return JS_FALSE;
	}
	return JS_FALSE;
}

JSBool svg_udom_get_matrix_trait(JSContext *c, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	char *szName;
	JSObject *mO;
	GF_FieldInfo info;
	GF_Node *n = dom_get_node(c, obj);
	if (!n) return JS_FALSE;

	if (argc!=1) return JS_FALSE;
	if (!JSVAL_IS_STRING(argv[0])) return JS_FALSE;
	szName = JS_GetStringBytes(JSVAL_TO_STRING(argv[0]));
	*rval = JSVAL_VOID;
	if (gf_node_get_field_by_name(n, szName, &info) != GF_OK) return JS_FALSE;

	if (info.fieldType==SVG_Transform_datatype) {
		GF_Matrix2D *mx = malloc(sizeof(GF_Matrix2D));
		mO = JS_NewObject(c, &svg_rt->matrixClass, 0, 0);
		gf_mx2d_init(*mx);
		gf_mx2d_copy(*mx, ((SVG_Transform*)info.far_ptr)->mat);

		JS_SetPrivate(c, mO, mx);
		*rval = OBJECT_TO_JSVAL(mO);
		return JS_TRUE;
	}
	return JS_FALSE;
}
JSBool svg_udom_get_rect_trait(JSContext *c, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	JSObject *newObj;
	char *szName;
	GF_FieldInfo info;
	GF_Node *n = dom_get_node(c, obj);
	if (!n) return JS_FALSE;

	if (argc!=1) return JS_FALSE;
	if (!JSVAL_IS_STRING(argv[0])) return JS_FALSE;
	szName = JS_GetStringBytes(JSVAL_TO_STRING(argv[0]));
	*rval = JSVAL_VOID;
	if (gf_node_get_field_by_name(n, szName, &info) != GF_OK) return JS_FALSE;

	if (info.fieldType==SVG_ViewBox_datatype) {
		rectCI *rc;
		SVG_ViewBox *v = (SVG_ViewBox *)info.far_ptr;
		newObj = JS_NewObject(c, &svg_rt->rectClass, 0, 0);
		GF_SAFEALLOC(rc, rectCI);
		rc->x = FIX2FLT(v->x);
		rc->y = FIX2FLT(v->y);
		rc->w = FIX2FLT(v->width);
		rc->h = FIX2FLT(v->height);
		JS_SetPrivate(c, newObj, rc);
		*rval = OBJECT_TO_JSVAL(newObj);
		return JS_TRUE;
	}
	return JS_FALSE;
}
JSBool svg_udom_get_path_trait(JSContext *c, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	char *szName;
	GF_FieldInfo info;
	GF_Node *n = dom_get_node(c, obj);
	if (!n) return JS_FALSE;

	if (argc!=1) return JS_FALSE;
	if (!JSVAL_IS_STRING(argv[0])) return JS_FALSE;
	szName = JS_GetStringBytes(JSVAL_TO_STRING(argv[0]));
	*rval = JSVAL_VOID;
	if (gf_node_get_field_by_name(n, szName, &info) != GF_OK) return JS_FALSE;

	if (info.fieldType==SVG_PathData_datatype) {
		*rval = OBJECT_TO_JSVAL( svg_new_path_object(c, info.far_ptr) );
		return JS_TRUE;
	}
	return JS_FALSE;
}
JSBool svg_udom_get_rgb_color_trait(JSContext *c, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	char *szName;
	GF_FieldInfo info;
	rgbCI *rgb;
	JSObject *newObj;
	GF_Node *n = dom_get_node(c, obj);
	if (!n) return JS_FALSE;

	if (argc!=1) return JS_FALSE;
	if (!JSVAL_IS_STRING(argv[0])) return JS_FALSE;
	szName = JS_GetStringBytes(JSVAL_TO_STRING(argv[0]));
	*rval = JSVAL_VOID;
	if (gf_node_get_field_by_name(n, szName, &info) != GF_OK) return JS_FALSE;

	switch (info.fieldType) {
	case SVG_Color_datatype:
	{
		SVG_Color *col = (SVG_Color *)info.far_ptr;
		if (col->type == SVG_COLOR_CURRENTCOLOR) return JS_FALSE;
		if (col->type == SVG_COLOR_INHERIT) return JS_FALSE;
		newObj = JS_NewObject(c, &svg_rt->rgbClass, 0, 0);
		GF_SAFEALLOC(rgb, rgbCI);
		rgb->r = (u8) (255*FIX2FLT(col->red)) ;
		rgb->g = (u8) (255*FIX2FLT(col->green)) ;
		rgb->b = (u8) (255*FIX2FLT(col->blue)) ;
		JS_SetPrivate(c, newObj, rgb);
		*rval = OBJECT_TO_JSVAL(newObj);
		return JS_TRUE;
	}
		break;
	case SVG_Paint_datatype:
	{
		SVG_Paint *paint = (SVG_Paint *)info.far_ptr;
		if (1 || paint->type==SVG_PAINT_COLOR) {
			newObj = JS_NewObject(c, &svg_rt->rgbClass, 0, 0);
			GF_SAFEALLOC(rgb, rgbCI);
			rgb->r = (u8) (255*FIX2FLT(paint->color.red) );
			rgb->g = (u8) (255*FIX2FLT(paint->color.green) );
			rgb->b = (u8) (255*FIX2FLT(paint->color.blue) );
			JS_SetPrivate(c, newObj, rgb);
			*rval = OBJECT_TO_JSVAL(newObj);
			return JS_TRUE;
		}
		return JS_FALSE;
	}
	}
	return JS_FALSE;
}

JSBool svg_udom_set_trait(JSContext *c, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	GF_Err e;
	GF_FieldInfo info;
	char *val = NULL;
	GF_Node *n = dom_get_node(c, obj);
	if (!n) return JS_FALSE;

	if (!JSVAL_IS_STRING(argv[0])) return JS_FALSE;
	if (argc==3) {
		char fullName[1024];
		if (!JSVAL_IS_STRING(argv[1])) return JS_FALSE;

		strcpy(fullName, JS_GetStringBytes(JSVAL_TO_STRING(argv[0])) );
		strcat(fullName, ":");
		strcat(fullName, JS_GetStringBytes(JSVAL_TO_STRING(argv[1])) );
		e = gf_node_get_field_by_name(n, fullName, &info);
		val = JS_GetStringBytes(JSVAL_TO_STRING(argv[2]));
	} else if (argc==2) {
		val = JS_GetStringBytes(JSVAL_TO_STRING(argv[0]));
		if (!strcmp(val, "#text")) {
			if (!JSVAL_IS_STRING(argv[1])) return JS_FALSE;
			dom_node_set_textContent(n, JS_GetStringBytes(JSVAL_TO_STRING(argv[1])) );
			return JS_TRUE;
		} else {
			e = gf_node_get_field_by_name(n, val, &info);
			val = JS_GetStringBytes(JSVAL_TO_STRING(argv[1]));
		}

	} else e = GF_BAD_PARAM;

	if (!val || (e!=GF_OK)) return JS_FALSE;
	*rval = JSVAL_VOID;

	switch (info.fieldType) {
	/* inheritable floats */
	case SVG_FontSize_datatype:
	case SVG_Color_datatype:
	case SVG_Paint_datatype:
	/* inheritable float and unit */
	case SVG_Length_datatype:
	case SVG_Coordinate_datatype:
	/*Number*/
	case SVG_Number_datatype:

/*all string traits*/
	case SVG_Boolean_datatype:
	case SVG_FillRule_datatype:
	case SVG_StrokeLineJoin_datatype:
	case SVG_StrokeLineCap_datatype:
	case SVG_FontStyle_datatype:
	case SVG_FontWeight_datatype:
	case SVG_FontVariant_datatype:
	case SVG_TextAnchor_datatype:
	case SVG_Display_datatype:
	case SVG_Visibility_datatype:
	case SMIL_AttributeType_datatype:
	case SMIL_CalcMode_datatype:
	case SMIL_Additive_datatype:
	case SMIL_Accumulate_datatype:
	case SMIL_Restart_datatype:
	case SMIL_Fill_datatype:
	case SVG_GradientUnit_datatype:
	case SVG_PreserveAspectRatio_datatype:
/*end of string traits*/
/*DOM string traits*/
	case SVG_FontFamily_datatype:
	case XMLRI_datatype:
	case SVG_String_datatype:
	case SVG_ContentType_datatype:
	case SVG_LanguageID_datatype:
/*end of DOM string traits*/
		gf_svg_parse_attribute(n, &info, val, 0);
		svg_node_changed(n, &info);
		break;

#if 0
/*SVGT 1.2 default traits*/
	case SMIL_KeyTimes_datatype:
	case SMIL_KeyPoints_datatype:
	case SMIL_KeySplines_datatype:
	case SVG_Coordinates_datatype:
	case SVG_StrokeDashArray_datatype:
	case SVG_Points_datatype:
	case SVG_Motion_datatype:
/*end SVGT 1.2 default traits*/

/*unimplemented/unnkown/FIXME traits*/
	case SMIL_SyncTolerance_datatype:
	case SVG_TransformType_datatype:
	case SVG_TransformList_datatype:
	case SMIL_AnimateValue_datatype:
	case SMIL_AnimateValues_datatype:
	case SMIL_AttributeName_datatype:
	case SMIL_Times_datatype:
	case SMIL_Duration_datatype:
	case SMIL_RepeatCount_datatype:
	default:
/*end unimplemented/unnkown/FIXME traits*/
		return JS_FALSE;
#endif
	}
	return JS_TRUE;
}

JSBool svg_udom_set_float_trait(JSContext *c, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	GF_FieldInfo info;
	jsdouble d;
	GF_Node *n = dom_get_node(c, obj);
	if (!n) return JS_FALSE;

	if (argc!=2) return JS_FALSE;
	if (!JSVAL_IS_STRING(argv[0])) return JS_FALSE;
	if (!JSVAL_IS_NUMBER(argv[1])) return JS_FALSE;
	JS_ValueToNumber(c, argv[1], &d);
	*rval = JSVAL_VOID;
	if (gf_node_get_field_by_name(n, JS_GetStringBytes(JSVAL_TO_STRING(argv[0])), &info) != GF_OK) return JS_FALSE;

	switch (info.fieldType) {
	/* inheritable floats */
	case SVG_FontSize_datatype:
	case SVG_Length_datatype:
	case SVG_Coordinate_datatype:
	case SVG_Number_datatype:
	{
		SVG_Number *l = (SVG_Number *)info.far_ptr;
		l->type=SVG_NUMBER_VALUE;
		l->value = FLT2FIX(d);
		break;
	}
	default:
		return JS_FALSE;
	}
	svg_node_changed(n, &info);
	return JS_TRUE;
}
JSBool svg_udom_set_matrix_trait(JSContext *c, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	JSObject *mO;
	char *szName;
	GF_FieldInfo info;
	GF_Matrix2D *mx;
	GF_Node *n = dom_get_node(c, obj);
	if (!n) return JS_FALSE;

	if (argc!=2) return JS_FALSE;
	if (!JSVAL_IS_STRING(argv[0])) return JS_FALSE;
	szName = JS_GetStringBytes(JSVAL_TO_STRING(argv[0]));
	if (!JSVAL_IS_OBJECT(argv[1])) return JS_FALSE;
	mO = JSVAL_TO_OBJECT(argv[1]);
	if (!JS_InstanceOf(c, mO, &svg_rt->matrixClass, NULL) ) return JS_FALSE;
	mx = JS_GetPrivate(c, mO);
	if (!mx) return JS_FALSE;

	if (gf_node_get_field_by_name(n, szName, &info) != GF_OK) return JS_FALSE;

	if (info.fieldType==SVG_Transform_datatype) {
		gf_mx2d_copy(((SVG_Transform*)info.far_ptr)->mat, *mx);
		svg_node_changed(n, NULL);
		return JS_TRUE;
	}
	return JS_FALSE;
}
JSBool svg_udom_set_rect_trait(JSContext *c, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	JSObject *rO;
	char *szName;
	GF_FieldInfo info;
	rectCI *rc;
	GF_Node *n = dom_get_node(c, obj);
	if (!n) return JS_FALSE;

	if (argc!=2) return JS_FALSE;
	if (!JSVAL_IS_STRING(argv[0])) return JS_FALSE;
	szName = JS_GetStringBytes(JSVAL_TO_STRING(argv[0]));
	if (!JSVAL_IS_OBJECT(argv[1])) return JS_FALSE;
	rO = JSVAL_TO_OBJECT(argv[1]);
	if (!JS_InstanceOf(c, rO, &svg_rt->rectClass, NULL) ) return JS_FALSE;
	rc = JS_GetPrivate(c, rO);
	if (!rc) return JS_FALSE;

	if (gf_node_get_field_by_name(n, szName, &info) != GF_OK) return JS_FALSE;

	if (info.fieldType==SVG_ViewBox_datatype) {
		SVG_ViewBox *v = (SVG_ViewBox *)info.far_ptr;
		v->x = FLT2FIX(rc->x);
		v->y = FLT2FIX(rc->y);
		v->width = FLT2FIX(rc->w);
		v->height = FLT2FIX(rc->h);
		svg_node_changed(n, NULL);
		return JS_TRUE;
	}
	return JS_FALSE;
}
JSBool svg_udom_set_path_trait(JSContext *c, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	pathCI *path;
	GF_FieldInfo info;
	JSObject *pO;
	GF_Node *n = dom_get_node(c, obj);
	if (!n) return JS_FALSE;

	if (argc!=2) return JS_FALSE;
	if (!JSVAL_IS_STRING(argv[0])) return JS_FALSE;
	if (!JSVAL_IS_OBJECT(argv[1])) return JS_FALSE;
	pO = JSVAL_TO_OBJECT(argv[1]);
	if (!JS_InstanceOf(c, pO, &svg_rt->pathClass, NULL) ) return JS_FALSE;
	path = JS_GetPrivate(c, pO);
	if (!path) return JS_FALSE;
	if (gf_node_get_field_by_name(n, JS_GetStringBytes(JSVAL_TO_STRING(argv[0])), &info) != GF_OK) return JS_FALSE;

	if (info.fieldType==SVG_PathData_datatype) {
#if USE_GF_PATH
#else
		u32 i;
		u32 nb_pts;
		SVG_PathData *d = (SVG_PathData *)info.far_ptr;
		while (gf_list_count(d->commands)) {
			u8 *t = gf_list_get(d->commands, 0);
			gf_list_rem(d->commands, 0);
			free(t);
		}
		while (gf_list_count(d->points)) {
			SVG_Point *t = gf_list_get(d->points, 0);
			gf_list_rem(d->points, 0);
			free(t);
		}
		nb_pts = 0;
		for (i=0; i<path->nb_coms; i++) {
			u8 *t = malloc(sizeof(u8));
			*t = path->tags[i];
			gf_list_add(d->commands, t);
			switch (*t) {
			case 0:
			case 1: nb_pts++; break;
			case 2: nb_pts+=3; break;
			case 4: nb_pts+=2; break;
			}
		}
		for (i=0; i<nb_pts; i++) {
			SVG_Point *t = malloc(sizeof(SVG_Point));
			t->x = FLT2FIX(path->pts[i].x);
			t->y = FLT2FIX(path->pts[i].y);
			gf_list_add(d->points, t);
		}
		svg_node_changed(n, NULL);
		return JS_TRUE;
#endif
	}
	return JS_FALSE;
}

JSBool svg_udom_set_rgb_color_trait(JSContext *c, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	GF_FieldInfo info;
	rgbCI *rgb;
	JSObject *colO;
	GF_Node *n = dom_get_node(c, obj);
	if (!n) return JS_FALSE;

	if (argc!=2) return JS_FALSE;
	if (!JSVAL_IS_STRING(argv[0])) return JS_FALSE;
	if (!JSVAL_IS_OBJECT(argv[1])) return JS_FALSE;
	colO = JSVAL_TO_OBJECT(argv[1]);
	if (!JS_InstanceOf(c, colO, &svg_rt->rgbClass, NULL) ) return JS_FALSE;
	rgb = JS_GetPrivate(c, colO);
	if (!rgb) return JS_FALSE;

	*rval = JSVAL_VOID;
	if (gf_node_get_field_by_name(n, JS_GetStringBytes(JSVAL_TO_STRING(argv[0])), &info) != GF_OK) return JS_FALSE;

	switch (info.fieldType) {
	case SVG_Color_datatype:
	{
		SVG_Color *col = (SVG_Color *)info.far_ptr;
		col->type = SVG_COLOR_RGBCOLOR;
		col->red = FLT2FIX(rgb->r / 255.0f);
		col->green = FLT2FIX(rgb->g / 255.0f);
		col->blue = FLT2FIX(rgb->b / 255.0f);
		svg_node_changed(n, &info);
		return JS_TRUE;
	}
	case SVG_Paint_datatype:
	{
		SVG_Paint *paint = (SVG_Paint *)info.far_ptr;
		paint->type = SVG_PAINT_COLOR;
		paint->color.type = SVG_COLOR_RGBCOLOR;
		paint->color.red = FLT2FIX(rgb->r / 255.0f);
		paint->color.green = FLT2FIX(rgb->g / 255.0f);
		paint->color.blue = FLT2FIX(rgb->b / 255.0f);
		svg_node_changed(n, &info);
		return JS_TRUE;
	}
	}
	return JS_FALSE;
}

static JSBool svg_get_bbox(JSContext *c, JSObject *obj, uintN argc, jsval *argv, jsval *rval, Bool get_screen)
{
	GF_JSAPIParam par;
	GF_Node *n = dom_get_node(c, obj);
	if (!n || argc) return JS_FALSE;

	par.bbox.is_set = 0;
	if (ScriptAction(n->sgprivate->scenegraph, get_screen ? GF_JSAPI_OP_GET_SCREEN_BBOX : GF_JSAPI_OP_GET_LOCAL_BBOX, (GF_Node *)n, &par) && par.bbox.is_set) {
		JSObject *rO = JS_NewObject(c, &svg_rt->rectClass, 0, 0);
		rectCI *rc = malloc(sizeof(rectCI));
		rc->sg = NULL;
		rc->x = FIX2FLT(par.bbox.min_edge.x);
		/*BBox is in 3D coord system style*/
		rc->y = FIX2FLT(par.bbox.min_edge.y);
		rc->w = FIX2FLT(par.bbox.max_edge.x - par.bbox.min_edge.x);
		rc->h = FIX2FLT(par.bbox.max_edge.y - par.bbox.min_edge.y);
		JS_SetPrivate(c, rO, rc);
		*rval = OBJECT_TO_JSVAL(rO);
		return JS_TRUE;
	}
	return JS_FALSE;
}
JSBool svg_udom_get_local_bbox(JSContext *c, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	return svg_get_bbox(c, obj, argc, argv, rval, 0);
}
JSBool svg_udom_get_screen_bbox(JSContext *c, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	return svg_get_bbox(c, obj, argc, argv, rval, 1);
}

JSBool svg_udom_get_screen_ctm(JSContext *c, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	GF_JSAPIParam par;
	GF_Node *n = dom_get_node(c, obj);
	if (!n || argc) return JS_FALSE;

	if (ScriptAction(n->sgprivate->scenegraph, GF_JSAPI_OP_GET_TRANSFORM, (GF_Node *)n, &par)) {
		JSObject *mO = JS_NewObject(c, &svg_rt->matrixClass, 0, 0);
		GF_Matrix2D *mx = malloc(sizeof(GF_Matrix2D));
		gf_mx2d_from_mx(mx, &par.mx);
		JS_SetPrivate(c, mO, mx);
		*rval = OBJECT_TO_JSVAL(mO);
		return JS_TRUE;
	}
	return JS_FALSE;
}

JSBool svg_udom_create_matrix_components(JSContext *c, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	GF_Matrix2D *mx;
	JSObject *mat;
	jsdouble v;
	GF_Node *n = dom_get_node(c, obj);
	if (!n) return JS_FALSE;
	if (argc!=6) return JS_FALSE;
	
	GF_SAFEALLOC(mx, GF_Matrix2D)
	JS_ValueToNumber(c, argv[0], &v);
	mx->m[0] = FLT2FIX(v);
	JS_ValueToNumber(c, argv[1], &v);
	mx->m[3] = FLT2FIX(v);
	JS_ValueToNumber(c, argv[2], &v);
	mx->m[1] = FLT2FIX(v);
	JS_ValueToNumber(c, argv[3], &v);
	mx->m[4] = FLT2FIX(v);
	JS_ValueToNumber(c, argv[4], &v);
	mx->m[2] = FLT2FIX(v);
	JS_ValueToNumber(c, argv[5], &v);
	mx->m[5] = FLT2FIX(v);
	mat = JS_NewObject(c, &svg_rt->matrixClass, 0, 0);
	JS_SetPrivate(c, mat, mx);
	*rval = OBJECT_TO_JSVAL(mat);
	return JS_TRUE;
}
JSBool svg_udom_create_rect(JSContext *c, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	rectCI *rc;
	JSObject *r;
	GF_Node *n = dom_get_node(c, obj);
	if (!n || argc) return JS_FALSE;

	GF_SAFEALLOC(rc, rectCI);
	r = JS_NewObject(c, &svg_rt->rectClass, 0, 0);
	JS_SetPrivate(c, r, rc);
	*rval = OBJECT_TO_JSVAL(r);
	return JS_TRUE;
}
JSBool svg_udom_create_path(JSContext *c, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	pathCI *path;
	JSObject *p;
	GF_Node *n = dom_get_node(c, obj);
	if (!n || argc) return JS_FALSE;

	GF_SAFEALLOC(path, pathCI);
	p = JS_NewObject(c, &svg_rt->pathClass, 0, 0);
	JS_SetPrivate(c, p, path);
	*rval = OBJECT_TO_JSVAL(p);
	return JS_TRUE;
}
JSBool svg_udom_create_color(JSContext *c, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	rgbCI *col;
	JSObject *p;
	GF_Node *n = dom_get_node(c, obj);
	if (!n) return JS_FALSE;
	if (argc!=3) return JS_FALSE;

	GF_SAFEALLOC(col, rgbCI);
	col->r = JSVAL_TO_INT(argv[0]);
	col->g = JSVAL_TO_INT(argv[1]);
	col->b = JSVAL_TO_INT(argv[2]);
	p = JS_NewObject(c, &svg_rt->rgbClass, 0, 0);
	JS_SetPrivate(c, p, col);
	*rval = OBJECT_TO_JSVAL(p);
	return JS_TRUE;
}

JSBool svg_udom_move_focus(JSContext *c, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	GF_JSAPIParam par;
	GF_Node *n = dom_get_node(c, obj);
	if (!n) return JS_FALSE;
	if ((argc!=1) || !JSVAL_IS_OBJECT(argv[0])) return JS_FALSE;

	par.opt = JSVAL_TO_INT(argv[1]);
	if (ScriptAction(n->sgprivate->scenegraph, GF_JSAPI_OP_SET_FOCUS, (GF_Node *)n, &par)) 
		return JS_TRUE;
	return JS_FALSE;
}
JSBool svg_udom_set_focus(JSContext *c, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	GF_JSAPIParam par;
	GF_Node *n = dom_get_node(c, obj);
	if (!n) return JS_FALSE;
	if ((argc!=1) || !JSVAL_IS_OBJECT(argv[0])) return JS_FALSE;

	par.node = dom_get_node(c, JSVAL_TO_OBJECT(argv[0]));

	/*NOT IN THE GRAPH*/
	if (!par.node || !par.node->sgprivate->num_instances) return JS_FALSE;
	if (ScriptAction(n->sgprivate->scenegraph, GF_JSAPI_OP_SET_FOCUS, (GF_Node *)n, &par)) 
		return JS_TRUE;
	return JS_FALSE;
}
JSBool svg_udom_get_focus(JSContext *c, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	GF_JSAPIParam par;
	GF_Node *n = dom_get_node(c, obj);
	if (!n || argc) return JS_FALSE;
	
	*rval = JSVAL_VOID;
	if (!ScriptAction(n->sgprivate->scenegraph, GF_JSAPI_OP_GET_FOCUS, (GF_Node *)n, &par)) 
		return JS_FALSE;

	if (par.node) {
		*rval = dom_element_construct(c, par.node);
	}
	return JS_TRUE;
}

JSBool svg_udom_get_property(JSContext *c, GF_Node *_n, u32 svg_prop_id, jsval *vp)
{
	GF_JSAPIParam par;
	jsdouble *d;
	JSString *s;
	SVG_Element *n = (SVG_Element*)_n;

	switch (svg_prop_id) {
	/*id*/
	case 0: 
	{
		const char *node_name = gf_node_get_name((GF_Node*)n);
		if (node_name) {
			s = JS_NewStringCopyZ(c, node_name);
			*vp = STRING_TO_JSVAL( s );
			return JS_TRUE;
		}
		return JS_FALSE;
	}
	/*currentScale*/
	case 1:
		if (n->sgprivate->tag!=TAG_SVG_svg) return JS_FALSE;
		if (ScriptAction(n->sgprivate->scenegraph, GF_JSAPI_OP_GET_SCALE, (GF_Node *)n, &par)) {
			d = JS_NewDouble(c, FIX2FLT(par.val) );
			*vp = DOUBLE_TO_JSVAL(d);
			return JS_TRUE;
		}
		return JS_FALSE;
	/*currentRotate*/
	case 2: 
		if (n->sgprivate->tag!=TAG_SVG_svg) return JS_FALSE;
		if (ScriptAction(n->sgprivate->scenegraph, GF_JSAPI_OP_GET_ROTATION, (GF_Node *)n, &par)) {
			d = JS_NewDouble(c, FIX2FLT(par.val) );
			*vp = DOUBLE_TO_JSVAL(d);
			return JS_TRUE;
		}
		return JS_FALSE;
	/*currentTranslate*/
	case 3:
		if (n->sgprivate->tag!=TAG_SVG_svg) return JS_FALSE;
		if (ScriptAction(n->sgprivate->scenegraph, GF_JSAPI_OP_GET_TRANSLATE, (GF_Node *)n, &par)) {
			JSObject *r = JS_NewObject(c, &svg_rt->pointClass, 0, 0);
			pointCI *rc = malloc(sizeof(pointCI));
			rc->x = FIX2FLT(par.pt.x);
			rc->y = FIX2FLT(par.pt.y);
			rc->sg = n->sgprivate->scenegraph;
			JS_SetPrivate(c, r, rc);
			*vp = OBJECT_TO_JSVAL(r);
			return JS_TRUE;
		}
		return JS_FALSE;
	/*viewport*/
	case 4:
		if (n->sgprivate->tag!=TAG_SVG_svg) return JS_FALSE;
		if (ScriptAction(n->sgprivate->scenegraph, GF_JSAPI_OP_GET_VIEWPORT, (GF_Node *)n, &par)) {
			JSObject *r = JS_NewObject(c, &svg_rt->rectClass, 0, 0);
			rectCI *rc = malloc(sizeof(rectCI));
			rc->x = FIX2FLT(par.rc.x);
			rc->y = FIX2FLT(par.rc.y);
			rc->w = FIX2FLT(par.rc.width);
			rc->h = FIX2FLT(par.rc.height);
			rc->sg = n->sgprivate->scenegraph;
			JS_SetPrivate(c, r, rc);
			*vp = OBJECT_TO_JSVAL(r);
			return JS_TRUE;
		}
		return JS_FALSE;
	/*currentTime*/
	case 5: 
		d = JS_NewDouble(c, gf_node_get_scene_time((GF_Node *)n) );
		*vp = DOUBLE_TO_JSVAL(d);
		return JS_TRUE;
	/*isPaused*/
	case 6: 
		*vp = BOOLEAN_TO_JSVAL(JS_FALSE);
		return JS_TRUE;
	/*ownerSVGElement*/
	case 7:
		while (1) {
			GF_Node *par = gf_node_get_parent(_n, 0);
			if (!par) return JS_FALSE;
			if (par->sgprivate->tag==TAG_SVG_svg) {
				*vp = dom_element_construct(c, par);
				return JS_TRUE;
			}
			_n = par;
		}
		return JS_FALSE;
	default: 
		return JS_FALSE;
	}

#if 0
	} else if (JSVAL_IS_STRING(id)) {
		GF_Node *n = JS_GetPrivate(c, obj);
		GF_FieldInfo info;
		char *name = JS_GetStringBytes(JSVAL_TO_STRING(id));
		if ( gf_node_get_field_by_name(n, name, &info) != GF_OK) return JS_TRUE;

		switch (info.fieldType) {
		case SVG_Length_datatype:
		case SVG_Coordinate_datatype: 
		case SVG_FontSize_datatype:
		case SVG_Rotate_datatype:
		case SVG_Number_datatype:
			if ( ((SVG_Number*)info.far_ptr)->type==SVG_NUMBER_INHERIT) {
				JSString *s = JS_NewStringCopyZ(c, "inherit");
				*vp = STRING_TO_JSVAL(s);
			} else {
				jsdouble *d = JS_NewDouble(c, FIX2FLT(((SVG_Number*)info.far_ptr)->value) );
				*vp = DOUBLE_TO_JSVAL(d);
			}
			return JS_TRUE;
		}
	}
	return JS_TRUE;
#endif
}

JSBool svg_udom_set_property(JSContext *c, GF_Node *n, u32 svg_prop_id, jsval *vp)
{
	GF_JSAPIParam par;
	jsdouble d;

	switch (svg_prop_id) {
	/*id - FIXME*/
	case 0: 
		return JS_TRUE;
	/*currentScale*/
	case 1: 
		if (!JSVAL_IS_NUMBER(*vp) || (n->sgprivate->tag!=TAG_SVG_svg)) return JS_FALSE;
		JS_ValueToNumber(c, *vp, &d);
		par.val = FLT2FIX(d);
		if (ScriptAction(n->sgprivate->scenegraph, GF_JSAPI_OP_SET_SCALE, (GF_Node *)n, &par)) {
			return JS_TRUE;
		}
		return JS_FALSE;
	/*currentRotate*/
	case 2: 
		if (!JSVAL_IS_NUMBER(*vp) || (n->sgprivate->tag!=TAG_SVG_svg)) return JS_FALSE;
		JS_ValueToNumber(c, *vp, &d);
		par.val = FLT2FIX(d);
		if (ScriptAction(n->sgprivate->scenegraph, GF_JSAPI_OP_SET_ROTATION, (GF_Node *)n, &par)) {
			return JS_TRUE;
		}
		return JS_FALSE;
	/*currentTime*/
	case 5:
		if (!JSVAL_IS_NUMBER(*vp) || (n->sgprivate->tag!=TAG_SVG_svg)) return JS_FALSE;
		JS_ValueToNumber(c, *vp, &d);
		par.time = d;
		if (ScriptAction(n->sgprivate->scenegraph, GF_JSAPI_OP_SET_TIME, (GF_Node *)n, &par)) {
			return JS_TRUE;
		}
		return JS_FALSE;
	default: 
		return JS_FALSE;
	}
#if 0
	} else if (JSVAL_IS_STRING(id)) {
		GF_Node *n = JS_GetPrivate(c, obj);
		GF_FieldInfo info;
		char *name = JS_GetStringBytes(JSVAL_TO_STRING(id));
		if ( gf_node_get_field_by_name(n, name, &info) != GF_OK) return JS_TRUE;

		if (JSVAL_IS_STRING(*vp)) {
			char *str = JS_GetStringBytes(JSVAL_TO_STRING(*vp));
			gf_svg_parse_attribute(n, &info, str, 0);
		} else {
			switch (info.fieldType) {
			case SVG_Length_datatype:
			case SVG_Coordinate_datatype: 
			case SVG_FontSize_datatype:
			case SVG_Rotate_datatype:
			case SVG_Number_datatype:
				if (JSVAL_IS_NUMBER(*vp)) {
					jsdouble d;
					JS_ValueToNumber(c, *vp, &d);
					((SVG_Number*)info.far_ptr)->type = SVG_NUMBER_VALUE;
					((SVG_Number*)info.far_ptr)->value = FLT2FIX(d);
				} else {
					return JS_FALSE;
				}
				break;
			default:
				return JS_FALSE;
			}
			svg_node_changed(n, &info);
			return JS_TRUE;
		}
	}
	return JS_TRUE;
#endif
}



static JSBool svg_connection_set_encoding(JSContext *c, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	return JS_FALSE;
}
static JSBool svg_connection_create(JSContext *c, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	return JS_FALSE;
}
static JSBool svg_connection_connect(JSContext *c, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	return JS_FALSE;
}
static JSBool svg_connection_send(JSContext *c, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	return JS_FALSE;
}
static JSBool svg_connection_close(JSContext *c, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	return JS_FALSE;
}
static JSPropertySpec connectionProps[] = {
	{"connected",			0,       JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_READONLY},
	{0}
};
static JSFunctionSpec connectionFuncs[] = {
	/*eventTarget interface*/
	{"addEventListenerNS", dom_event_add_listener, 4},
	{"removeEventListenerNS", dom_event_remove_listener, 4},
	{"addEventListenerNS", dom_event_add_listener, 3},
	{"removeEventListenerNS", dom_event_remove_listener, 3},
	/*connection interface*/
	{"setEncoding", svg_connection_set_encoding, 1},
	{"connect", svg_connection_connect, 1},
	{"send", svg_connection_send, 1},
	{"close", svg_connection_close, 0},
	{0}
};

static void baseCI_finalize(JSContext *c, JSObject *obj)
{
	void *data = JS_GetPrivate(c, obj);
	if (data) free(data);
}
static JSBool rgb_constructor(JSContext *c, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	rgbCI *p;
	GF_SAFEALLOC(p, rgbCI);
	JS_SetPrivate(c, obj, p);
	*rval = OBJECT_TO_JSVAL(obj);
	return JS_TRUE;
}

static JSBool rgb_getProperty(JSContext *c, JSObject *obj, jsval id, jsval *vp)
{
	if (!JS_InstanceOf(c, obj, &svg_rt->rgbClass, NULL) ) return JS_FALSE;
	if (JSVAL_IS_INT(id)) {
		rgbCI *col = JS_GetPrivate(c, obj);
		if (!col) return JS_FALSE;
		switch (JSVAL_TO_INT(id)) {
		case 0: *vp = INT_TO_JSVAL(col->r); return JS_TRUE;
		case 1: *vp = INT_TO_JSVAL(col->g); return JS_TRUE;
		case 2: *vp = INT_TO_JSVAL(col->b); return JS_TRUE;
		default:
			return JS_FALSE;
		}
	}
	return JS_TRUE;
}
static JSBool rgb_setProperty(JSContext *c, JSObject *obj, jsval id, jsval *vp)
{
	if (!JS_InstanceOf(c, obj, &svg_rt->rgbClass, NULL) ) return JS_FALSE;
	if (JSVAL_IS_INT(id)) {
		rgbCI *col = JS_GetPrivate(c, obj);
		if (!col) return JS_FALSE;
		switch (JSVAL_TO_INT(id)) {
		case 0: col->r = JSVAL_TO_INT(*vp); return JS_TRUE;
		case 1: col->g = JSVAL_TO_INT(*vp); return JS_TRUE;
		case 2: col->b = JSVAL_TO_INT(*vp); return JS_TRUE;
		default: return JS_FALSE;
		}
	}
	return JS_TRUE;
}


static JSBool rect_constructor(JSContext *c, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	rectCI *p;
	GF_SAFEALLOC(p, rectCI);
	JS_SetPrivate(c, obj, p);
	*rval = OBJECT_TO_JSVAL(obj);
	return JS_TRUE;
}

static JSBool rect_getProperty(JSContext *c, JSObject *obj, jsval id, jsval *vp)
{
	if (!JS_InstanceOf(c, obj, &svg_rt->rectClass, NULL) ) return JS_FALSE;
	if (JSVAL_IS_INT(id)) {
		jsdouble *d;
		rectCI *rc = JS_GetPrivate(c, obj);
		if (!rc) return JS_FALSE;
		if (rc->sg) {
			GF_JSAPIParam par;
			ScriptAction(rc->sg, GF_JSAPI_OP_GET_VIEWPORT, rc->sg->RootNode, &par);
			rc->x = FIX2FLT(par.rc.x);
			rc->y = FIX2FLT(par.rc.y);
			rc->w = FIX2FLT(par.rc.width);
			rc->h = FIX2FLT(par.rc.height);
		}
		switch (JSVAL_TO_INT(id)) {
		case 0: d = JS_NewDouble(c, rc->x); *vp = DOUBLE_TO_JSVAL(d); return JS_TRUE;
		case 1: d = JS_NewDouble(c, rc->y); *vp = DOUBLE_TO_JSVAL(d); return JS_TRUE;
		case 2: d = JS_NewDouble(c, rc->w); *vp = DOUBLE_TO_JSVAL(d); return JS_TRUE;
		case 3: d = JS_NewDouble(c, rc->h); *vp = DOUBLE_TO_JSVAL(d); return JS_TRUE;
		default: return JS_FALSE;
		}
	}
	return JS_TRUE;
}
static JSBool rect_setProperty(JSContext *c, JSObject *obj, jsval id, jsval *vp)
{
	if (!JS_InstanceOf(c, obj, &svg_rt->rectClass, NULL) ) return JS_FALSE;
	if (JSVAL_IS_INT(id)) {
		jsdouble d;
		rectCI *rc = JS_GetPrivate(c, obj);
		if (!rc) return JS_FALSE;
		JS_ValueToNumber(c, *vp, &d);
		switch (JSVAL_TO_INT(id)) {
		case 0: rc->x = (Float) d; return JS_TRUE;
		case 1: rc->y = (Float) d; return JS_TRUE;
		case 2: rc->w = (Float) d; return JS_TRUE;
		case 3: rc->h = (Float) d; return JS_TRUE;
		default: return JS_FALSE;
		}
	}
	return JS_TRUE;
}

static JSBool point_constructor(JSContext *c, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	pointCI *p;
	GF_SAFEALLOC(p, pointCI);
	JS_SetPrivate(c, obj, p);
	*rval = OBJECT_TO_JSVAL(obj);
	return JS_TRUE;
}

static JSBool point_getProperty(JSContext *c, JSObject *obj, jsval id, jsval *vp)
{
	if (!JS_InstanceOf(c, obj, &svg_rt->pointClass, NULL) ) return JS_FALSE;
	if (JSVAL_IS_INT(id)) {
		jsdouble *d;
		pointCI *pt = JS_GetPrivate(c, obj);
		if (!pt) return JS_FALSE;
		if (pt->sg) {
			GF_JSAPIParam par;
			ScriptAction(pt->sg, GF_JSAPI_OP_GET_TRANSLATE, pt->sg->RootNode, &par);
			pt->x = FIX2FLT(par.pt.x);
			pt->y = FIX2FLT(par.pt.y);
		}
		switch (JSVAL_TO_INT(id)) {
		case 0: d = JS_NewDouble(c, pt->x); *vp = DOUBLE_TO_JSVAL(d); return JS_TRUE;
		case 1: d = JS_NewDouble(c, pt->y); *vp = DOUBLE_TO_JSVAL(d); return JS_TRUE;
		default: return JS_FALSE;
		}
	}
	return JS_TRUE;
}
static JSBool point_setProperty(JSContext *c, JSObject *obj, jsval id, jsval *vp)
{
	if (!JS_InstanceOf(c, obj, &svg_rt->pointClass, NULL) ) return JS_FALSE;
	if (JSVAL_IS_INT(id)) {
		jsdouble d;
		pointCI *pt = JS_GetPrivate(c, obj);
		if (!pt) return JS_FALSE;
		JS_ValueToNumber(c, *vp, &d);
		switch (JSVAL_TO_INT(id)) {
		case 0: pt->x = (Float) d; break;
		case 1: pt->y = (Float) d; break;
		default: return JS_FALSE;
		}
		if (pt->sg) {
			GF_JSAPIParam par;
			par.pt.x = FLT2FIX(pt->x);
			par.pt.y = FLT2FIX(pt->y);
			ScriptAction(pt->sg, GF_JSAPI_OP_SET_TRANSLATE, pt->sg->RootNode, &par);
		}
		return JS_TRUE;
	}
	return JS_TRUE;
}

static JSObject *svg_new_path_object(JSContext *c, SVG_PathData *d)
{
#if USE_GF_PATH
	return NULL;
#else
	JSObject *obj;
	pathCI *p;
	GF_SAFEALLOC(p, pathCI);
	if (d) {
		u32 i, count;
		p->nb_coms = gf_list_count(d->commands);
		p->tags = malloc(sizeof(u8) * p->nb_coms);
		for (i=0; i<p->nb_coms; i++) p->tags[i] = * (u8 *) gf_list_get(d->commands, i);
		count = gf_list_count(d->points);
		p->pts = malloc(sizeof(pointCI) * count);
		for (i=0; i<count; i++) {
			GF_Point2D *pt = gf_list_get(d->commands, i);
			p->pts[i].x = FIX2FLT(pt->x);
			p->pts[i].y = FIX2FLT(pt->y);
		}
	}
	obj = JS_NewObject(c, &svg_rt->pathClass, 0, 0);
	JS_SetPrivate(c, obj, p);
	return obj;
#endif
}

static JSBool pathCI_constructor(JSContext *c, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	pathCI *p;
	GF_SAFEALLOC(p, pathCI);
	JS_SetPrivate(c, obj, p);
	*rval = OBJECT_TO_JSVAL(obj);
	return JS_TRUE;
}

static void pathCI_finalize(JSContext *c, JSObject *obj)
{
	pathCI *p = JS_GetPrivate(c, obj);
	if (p) {
		if (p->pts) free(p->pts);
		if (p->tags) free(p->tags);
		free(p);
	}
}
static JSBool path_getProperty(JSContext *c, JSObject *obj, jsval id, jsval *vp)
{
	if (!JS_InstanceOf(c, obj, &svg_rt->pathClass, NULL) ) return JS_FALSE;
	if (JSVAL_IS_INT(id)) {
		pathCI *p = JS_GetPrivate(c, obj);
		if (!p) return JS_FALSE;
		switch (JSVAL_TO_INT(id)) {
		case 0: *vp = INT_TO_JSVAL(p->nb_coms); return JS_TRUE;
		default: return JS_FALSE;
		}
	}
	return JS_TRUE;
}
static JSBool svg_path_get_segment(JSContext *c, JSObject *obj, uintN argc, jsval *argv, jsval *vp)
{
	pathCI *p;
	u32 idx;
	if (!JS_InstanceOf(c, obj, &svg_rt->pathClass, NULL) ) return JS_FALSE;
	p = JS_GetPrivate(c, obj);
	if (!p) return JS_FALSE;
	if ((argc!=1) || !JSVAL_IS_INT(argv[0])) return JS_FALSE;
	idx = JSVAL_TO_INT(argv[0]);
	if (idx>=p->nb_coms) return JS_FALSE;
	switch (p->tags[idx]) {
	case 0: *vp = INT_TO_JSVAL(77); return JS_TRUE;	/* Move To */
	case 1: *vp = INT_TO_JSVAL(76); return JS_TRUE;	/* Line To */
	case 2:/* Curve To */
	case 3:/* next Curve To */
		*vp = INT_TO_JSVAL(67); return JS_TRUE;	
	case 4:/* Quad To */
	case 5:/* next Quad To */
		*vp = INT_TO_JSVAL(81); return JS_TRUE;	
	case 6: *vp = INT_TO_JSVAL(90); return JS_TRUE;	/* Close */
	}
	return JS_FALSE;
}

static JSBool svg_path_get_segment_param(JSContext *c, JSObject *obj, uintN argc, jsval *argv, jsval *vp)
{
	jsdouble *d;
	pathCI *p;
	ptCI *pt;
	u32 i, idx, param_idx, pt_idx;
	if (!JS_InstanceOf(c, obj, &svg_rt->pathClass, NULL) ) return JS_FALSE;
	p = JS_GetPrivate(c, obj);
	if (!p) return JS_FALSE;
	if ((argc!=2) || !JSVAL_IS_INT(argv[0]) || !JSVAL_IS_INT(argv[1])) return JS_FALSE;
	idx = JSVAL_TO_INT(argv[0]);
	param_idx = JSVAL_TO_INT(argv[1]);
	if (idx>=p->nb_coms) return JS_FALSE;
	pt_idx = 0;
	for (i=0; i<idx; i++) {
		switch (p->tags[i]) {
		case 0: pt_idx++; break;
		case 1: pt_idx++; break;
		case 2: pt_idx+=3; break;
		case 3: pt_idx+=2; break;
		case 4: pt_idx+=2; break;
		case 5: pt_idx+=1; break;
		}
	}
	switch (p->tags[idx]) {
	case 0: 
	case 1: 
		if (param_idx>1) return JS_FALSE;
		pt = &p->pts[pt_idx];
		d = JS_NewDouble(c, param_idx ? pt->y : pt->x); 
		*vp = DOUBLE_TO_JSVAL(d); 
		return JS_TRUE;
	case 2:/* Curve To */
		if (param_idx>5) return JS_FALSE;
		pt = &p->pts[pt_idx + (param_idx/2) ];
		d = JS_NewDouble(c, (param_idx%2) ? pt->y : pt->x);
		*vp = DOUBLE_TO_JSVAL(d); 
		return JS_TRUE;
	case 3:/* Next Curve To */
		if (param_idx>5) return JS_FALSE;
		if (param_idx<2) {
			pt = &p->pts[pt_idx - 1];
			d = JS_NewDouble(c, param_idx ? pt->y : pt->x);
		} else {
			param_idx-=2;
			pt = &p->pts[pt_idx + (param_idx/2)];
			d = JS_NewDouble(c, (param_idx%2) ? pt->y : pt->x);
		}
		*vp = DOUBLE_TO_JSVAL(d); 
		return JS_TRUE;
	case 4:/* Quad To */
		if (param_idx>3) return JS_FALSE;
		pt = &p->pts[pt_idx + (param_idx/2) ];
		d = JS_NewDouble(c, (param_idx%2) ? pt->y : pt->x);
		*vp = DOUBLE_TO_JSVAL(d); 
		return JS_TRUE;
	case 5:/* Next Quad To */
		if (param_idx>3) return JS_FALSE;
		if (param_idx<2) {
			pt = &p->pts[pt_idx - 1];
			d = JS_NewDouble(c, param_idx ? pt->y : pt->x);
		} else {
			param_idx-=2;
			pt = &p->pts[pt_idx];
			d = JS_NewDouble(c, param_idx ? pt->y : pt->x);
		}
		*vp = DOUBLE_TO_JSVAL(d); 
		return JS_TRUE;
		/*spec is quite obscur here*/
	case 6:
		d = JS_NewDouble(c, 0); 
		*vp = DOUBLE_TO_JSVAL(d);  
		return JS_TRUE;
	}
	return JS_FALSE;
}

static u32 svg_path_realloc_pts(pathCI *p, u32 nb_pts)
{
	u32 i, orig_pts;
	orig_pts = 0;
	for (i=0; i<p->nb_coms; i++) {
		switch (p->tags[i]) {
		case 0: orig_pts++; break;
		case 1: orig_pts++; break;
		case 2: orig_pts+=3; break;
		case 3: orig_pts+=2; break;
		case 4: orig_pts+=2; break;
		case 5: orig_pts+=1; break;
		}
	}
	p->pts = realloc(p->pts, sizeof(ptCI)*(nb_pts+orig_pts));
	return orig_pts;
}
static JSBool svg_path_move_to(JSContext *c, JSObject *obj, uintN argc, jsval *argv, jsval *vp)
{
	pathCI *p;
	jsdouble x, y;
	u32 nb_pts;
	if (!JS_InstanceOf(c, obj, &svg_rt->pathClass, NULL) ) return JS_FALSE;
	p = JS_GetPrivate(c, obj);
	if (!p) return JS_FALSE;
	if ((argc!=2) || !JSVAL_IS_NUMBER(argv[0]) || !JSVAL_IS_NUMBER(argv[1])) return JS_FALSE;
	JS_ValueToNumber(c, argv[0], &x);
	JS_ValueToNumber(c, argv[1], &y);
	nb_pts = svg_path_realloc_pts(p, 1);
	p->pts[nb_pts].x = (Float) x;
	p->pts[nb_pts].y = (Float) y;
	p->tags = realloc(p->tags, sizeof(u8)*(p->nb_coms+1) );
	p->tags[p->nb_coms] = 0;
	p->nb_coms++;
	return JS_TRUE;
}
static JSBool svg_path_line_to(JSContext *c, JSObject *obj, uintN argc, jsval *argv, jsval *vp)
{
	pathCI *p;
	jsdouble x, y;
	u32 nb_pts;
	if (!JS_InstanceOf(c, obj, &svg_rt->pathClass, NULL) ) return JS_FALSE;
	p = JS_GetPrivate(c, obj);
	if (!p) return JS_FALSE;
	if ((argc!=2) || !JSVAL_IS_NUMBER(argv[0]) || !JSVAL_IS_NUMBER(argv[1])) return JS_FALSE;
	JS_ValueToNumber(c, argv[0], &x);
	JS_ValueToNumber(c, argv[1], &y);
	nb_pts = svg_path_realloc_pts(p, 1);
	p->pts[nb_pts].x = (Float) x;
	p->pts[nb_pts].y = (Float) y;
	p->tags = realloc(p->tags, sizeof(u8)*(p->nb_coms+1) );
	p->tags[p->nb_coms] = 1;
	p->nb_coms++;
	return JS_TRUE;
}

static JSBool svg_path_quad_to(JSContext *c, JSObject *obj, uintN argc, jsval *argv, jsval *vp)
{
	pathCI *p;
	jsdouble x1, y1, x2, y2;
	u32 nb_pts;
	if (!JS_InstanceOf(c, obj, &svg_rt->pathClass, NULL) ) return JS_FALSE;
	p = JS_GetPrivate(c, obj);
	if (!p) return JS_FALSE;
	if ((argc!=4) || !JSVAL_IS_NUMBER(argv[0]) || !JSVAL_IS_NUMBER(argv[1]) || !JSVAL_IS_NUMBER(argv[2]) || !JSVAL_IS_NUMBER(argv[3])) return JS_FALSE;
	JS_ValueToNumber(c, argv[0], &x1);
	JS_ValueToNumber(c, argv[1], &y1);
	JS_ValueToNumber(c, argv[2], &x2);
	JS_ValueToNumber(c, argv[3], &y2);
	nb_pts = svg_path_realloc_pts(p, 2);
	p->pts[nb_pts].x = (Float) x1; p->pts[nb_pts].y = (Float) y1;
	p->pts[nb_pts+1].x = (Float) x2; p->pts[nb_pts+1].y = (Float) y2;
	p->tags = realloc(p->tags, sizeof(u8)*(p->nb_coms+1) );
	p->tags[p->nb_coms] = 4;
	p->nb_coms++;
	return JS_TRUE;
}
static JSBool svg_path_curve_to(JSContext *c, JSObject *obj, uintN argc, jsval *argv, jsval *vp)
{
	pathCI *p;
	jsdouble x1, y1, x2, y2, x, y;
	u32 nb_pts;
	if (!JS_InstanceOf(c, obj, &svg_rt->pathClass, NULL) ) return JS_FALSE;
	p = JS_GetPrivate(c, obj);
	if (!p) return JS_FALSE;
	if ((argc!=6) || !JSVAL_IS_NUMBER(argv[0]) || !JSVAL_IS_NUMBER(argv[1]) || !JSVAL_IS_NUMBER(argv[2]) || !JSVAL_IS_NUMBER(argv[3]) || !JSVAL_IS_NUMBER(argv[4]) || !JSVAL_IS_NUMBER(argv[5])) return JS_FALSE;
	JS_ValueToNumber(c, argv[0], &x1);
	JS_ValueToNumber(c, argv[1], &y1);
	JS_ValueToNumber(c, argv[2], &x2);
	JS_ValueToNumber(c, argv[3], &y2);
	JS_ValueToNumber(c, argv[4], &x);
	JS_ValueToNumber(c, argv[5], &y);
	nb_pts = svg_path_realloc_pts(p, 3);
	p->pts[nb_pts].x = (Float) x1; p->pts[nb_pts].y = (Float) y1;
	p->pts[nb_pts+1].x = (Float) x2; p->pts[nb_pts+1].y = (Float) y2;
	p->pts[nb_pts+2].x = (Float) x; p->pts[nb_pts+2].y = (Float) y;
	p->tags = realloc(p->tags, sizeof(u8)*(p->nb_coms+1) );
	p->tags[p->nb_coms] = 2;
	p->nb_coms++;
	return JS_TRUE;
}
static JSBool svg_path_close(JSContext *c, JSObject *obj, uintN argc, jsval *argv, jsval *vp)
{
	pathCI *p;
	if (!JS_InstanceOf(c, obj, &svg_rt->pathClass, NULL) ) return JS_FALSE;
	p = JS_GetPrivate(c, obj);
	if (!p) return JS_FALSE;
	if (argc) return JS_FALSE;
	p->tags = realloc(p->tags, sizeof(u8)*(p->nb_coms+1) );
	p->tags[p->nb_coms] = 6;
	p->nb_coms++;
	return JS_TRUE;
}

static JSBool svg_mx2d_constructor(JSContext *c, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	GF_Matrix2D *mx = malloc(sizeof(GF_Matrix2D));
	gf_mx2d_init(*mx);
	JS_SetPrivate(c, obj, mx);
	*rval = OBJECT_TO_JSVAL(obj);
	return JS_TRUE;
}

static JSBool svg_mx2d_get_component(JSContext *c, JSObject *obj, uintN argc, jsval *argv, jsval *vp)
{
	jsdouble *d;
	GF_Matrix2D *mx;
	if (!JS_InstanceOf(c, obj, &svg_rt->matrixClass, NULL) ) return JS_FALSE;
	mx = JS_GetPrivate(c, obj);
	if (!mx || (argc!=1)) return JS_FALSE;
	if (!JSVAL_IS_INT(argv[0])) return JS_FALSE;
	switch (JSVAL_TO_INT(argv[0])) {
	case 0: d = JS_NewDouble(c, FIX2FLT(mx->m[0])); *vp = DOUBLE_TO_JSVAL(d); return JS_TRUE;
	case 1: d = JS_NewDouble(c, FIX2FLT(mx->m[3])); *vp = DOUBLE_TO_JSVAL(d); return JS_TRUE;
	case 2: d = JS_NewDouble(c, FIX2FLT(mx->m[1])); *vp = DOUBLE_TO_JSVAL(d); return JS_TRUE;
	case 3: d = JS_NewDouble(c, FIX2FLT(mx->m[4])); *vp = DOUBLE_TO_JSVAL(d); return JS_TRUE;
	case 4: d = JS_NewDouble(c, FIX2FLT(mx->m[2])); *vp = DOUBLE_TO_JSVAL(d); return JS_TRUE;
	case 5: d = JS_NewDouble(c, FIX2FLT(mx->m[5])); *vp = DOUBLE_TO_JSVAL(d); return JS_TRUE;
	}
	return JS_FALSE;
}

static JSBool svg_mx2d_multiply(JSContext *c, JSObject *obj, uintN argc, jsval *argv, jsval *vp)
{
	JSObject *mat;
	GF_Matrix2D *mx1, *mx2;
	if (!JS_InstanceOf(c, obj, &svg_rt->matrixClass, NULL) ) return JS_FALSE;
	mx1 = JS_GetPrivate(c, obj);
	if (!mx1 || (argc!=1)) return JS_FALSE;
	if (!JSVAL_IS_OBJECT(argv[0])) return JS_FALSE;
	mat = JSVAL_TO_OBJECT(argv[0]);
	if (!JS_InstanceOf(c, mat, &svg_rt->matrixClass, NULL) ) return JS_FALSE;
	mx2 = JS_GetPrivate(c, mat);
	if (!mx2) return JS_FALSE;
	gf_mx2d_add_matrix(mx1, mx2);
	*vp = OBJECT_TO_JSVAL(obj);
	return JS_TRUE;
}

static JSBool svg_mx2d_inverse(JSContext *c, JSObject *obj, uintN argc, jsval *argv, jsval *vp)
{
	GF_Matrix2D *mx1;
	if (!JS_InstanceOf(c, obj, &svg_rt->matrixClass, NULL) ) return JS_FALSE;
	mx1 = JS_GetPrivate(c, obj);
	if (!mx1) return JS_FALSE;
	gf_mx2d_inverse(mx1);
	*vp = OBJECT_TO_JSVAL(obj);
	return JS_TRUE;
}

static JSBool svg_mx2d_translate(JSContext *c, JSObject *obj, uintN argc, jsval *argv, jsval *vp)
{
	jsdouble x, y;
	GF_Matrix2D *mx1;
	if (!JS_InstanceOf(c, obj, &svg_rt->matrixClass, NULL) ) return JS_FALSE;
	mx1 = JS_GetPrivate(c, obj);
	if (!mx1 || (argc!=2)) return JS_FALSE;
	JS_ValueToNumber(c, argv[0], &x);
	JS_ValueToNumber(c, argv[1], &y);
	gf_mx2d_add_translation(mx1, FLT2FIX(x), FLT2FIX(y));
	*vp = OBJECT_TO_JSVAL(obj);
	return JS_TRUE;
}

static JSBool svg_mx2d_scale(JSContext *c, JSObject *obj, uintN argc, jsval *argv, jsval *vp)
{
	jsdouble scale;
	GF_Matrix2D *mx1;
	if (!JS_InstanceOf(c, obj, &svg_rt->matrixClass, NULL) ) return JS_FALSE;
	mx1 = JS_GetPrivate(c, obj);
	if (!mx1 || (argc!=1)) return JS_FALSE;
	JS_ValueToNumber(c, argv[0], &scale);
	gf_mx2d_add_scale(mx1, FLT2FIX(scale), FLT2FIX(scale));
	*vp = OBJECT_TO_JSVAL(obj);
	return JS_TRUE;
}

static JSBool svg_mx2d_rotate(JSContext *c, JSObject *obj, uintN argc, jsval *argv, jsval *vp)
{
	jsdouble angle;
	GF_Matrix2D *mx1;
	if (!JS_InstanceOf(c, obj, &svg_rt->matrixClass, NULL) ) return JS_FALSE;
	mx1 = JS_GetPrivate(c, obj);
	if (!mx1 || (argc!=1)) return JS_FALSE;
	JS_ValueToNumber(c, argv[0], &angle);
	gf_mx2d_add_rotation(mx1, 0, 0, gf_mulfix(FLT2FIX(angle/180), GF_PI));
	*vp = OBJECT_TO_JSVAL(obj);
	return JS_TRUE;
}

jsval svg_udom_new_rect(JSContext *c, Fixed x, Fixed y, Fixed width, Fixed height)
{
	JSObject *r = JS_NewObject(c, &svg_rt->rectClass, 0, 0);
	rectCI *rc = malloc(sizeof(rectCI));
	rc->x = FIX2FLT(x);
	rc->y = FIX2FLT(y);
	rc->w = FIX2FLT(width);
	rc->h = FIX2FLT(height);
	rc->sg = NULL;
	JS_SetPrivate(c, r, rc);
	return OBJECT_TO_JSVAL(r);
}

jsval svg_udom_new_point(JSContext *c, Fixed x, Fixed y)
{
	JSObject *p = JS_NewObject(c, &svg_rt->pointClass, 0, 0);
	pointCI *pt = malloc(sizeof(pointCI));
	pt->x = FIX2FLT(x);
	pt->y = FIX2FLT(y);
	pt->sg = NULL;
	JS_SetPrivate(c, p, pt);
	return OBJECT_TO_JSVAL(p);
}


static void svg_init_js_api(GF_SceneGraph *scene)
{
	JS_SetContextPrivate(scene->svg_js->js_ctx, scene);
	JS_SetErrorReporter(scene->svg_js->js_ctx, svg_script_error);

	/*init global object*/
	scene->svg_js->global = JS_NewObject(scene->svg_js->js_ctx, &svg_rt->globalClass, 0, 0 );
	JS_InitStandardClasses(scene->svg_js->js_ctx, scene->svg_js->global);
	/*remember pointer to scene graph!!*/
	JS_SetPrivate(scene->svg_js->js_ctx, scene->svg_js->global, scene);
	{
		JSPropertySpec globalClassProps[] = {
			{"connected",	0,       JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_READONLY},
			{"parent",		1,       JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_READONLY},
			{0}
		};
		JSFunctionSpec globalClassFuncs[] = {
			{"createConnection", svg_connection_create, 0},
			{"gotoLocation", svg_nav_to_location, 1},
			{"alert",           svg_echo,          0},
			/*technically, this is part of Implementation interface, not global, but let's try not to complicate things too much*/
			{"hasFeature", dom_imp_has_feature, 2},
			{0}
		};
		JS_DefineFunctions(scene->svg_js->js_ctx, scene->svg_js->global, globalClassFuncs);
		JS_DefineProperties(scene->svg_js->js_ctx, scene->svg_js->global, globalClassProps);
	}

	/*initialize DOM core */
	dom_js_load(scene, scene->svg_js->js_ctx, scene->svg_js->global);
	/*create document object, and remember it*/
	dom_js_define_document(scene->svg_js->js_ctx, scene->svg_js->global, scene);
	/*create event object, and remember it*/
	scene->svg_js->event = dom_js_define_event(scene->svg_js->js_ctx, scene->svg_js->global);

	/*RGBColor class*/
	{
		JSPropertySpec rgbClassProps[] = {
			{"red",		0,       JSPROP_ENUMERATE | JSPROP_PERMANENT },
			{"green",	1,       JSPROP_ENUMERATE | JSPROP_PERMANENT },
			{"blue",	2,       JSPROP_ENUMERATE | JSPROP_PERMANENT },
			{0}
		};
		JS_InitClass(scene->svg_js->js_ctx, scene->svg_js->global, 0, &svg_rt->rgbClass, rgb_constructor, 0, rgbClassProps, 0, 0, 0);
	}
	/*SVGRect class*/
	{
		JSPropertySpec rectClassProps[] = {
			{"x",		0,       JSPROP_ENUMERATE | JSPROP_PERMANENT },
			{"y",		1,       JSPROP_ENUMERATE | JSPROP_PERMANENT },
			{"width",	2,       JSPROP_ENUMERATE | JSPROP_PERMANENT },
			{"height",	3,       JSPROP_ENUMERATE | JSPROP_PERMANENT },
			{0}
		};
		JS_InitClass(scene->svg_js->js_ctx, scene->svg_js->global, 0, &svg_rt->rectClass, rect_constructor, 0, rectClassProps, 0, 0, 0);
	}
	/*SVGPoint class*/
	{
		JSPropertySpec pointClassProps[] = {
			{"x",	0,       JSPROP_ENUMERATE | JSPROP_PERMANENT },
			{"y",	1,       JSPROP_ENUMERATE | JSPROP_PERMANENT },
			{0}
		};
		JS_InitClass(scene->svg_js->js_ctx, scene->svg_js->global, 0, &svg_rt->pointClass, point_constructor, 0, pointClassProps, 0, 0, 0);
	}
	/*SVGMatrix class*/
	{
		JSFunctionSpec matrixClassFuncs[] = {
			{"getComponent", svg_mx2d_get_component, 1},
			{"mMultiply", svg_mx2d_multiply, 1},
			{"inverse", svg_mx2d_inverse, 0},
			{"mTranslate", svg_mx2d_translate, 2},
			{"mScale", svg_mx2d_scale, 1},
			{"mRotate", svg_mx2d_rotate, 1},
			{0}
		};
		JS_InitClass(scene->svg_js->js_ctx, scene->svg_js->global, 0, &svg_rt->matrixClass, svg_mx2d_constructor, 0, 0, matrixClassFuncs, 0, 0);
	}
	/*SVGPath class*/
	{
		JSFunctionSpec pathClassFuncs[] = {
			{"getSegment", svg_path_get_segment, 1},
			{"getSegmentParam", svg_path_get_segment_param, 2},
			{"moveTo", svg_path_move_to, 2},
			{"lineTo", svg_path_line_to, 2},
			{"quadTo", svg_path_quad_to, 4},
			{"curveTo", svg_path_curve_to, 6},
			{"close", svg_path_close, 0},
			{0}
		};
		JSPropertySpec pathClassProps[] = {
			{"numberOfSegments",	0,       JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_READONLY},
			{0}
		};
		JS_InitClass(scene->svg_js->js_ctx, scene->svg_js->global, 0, &svg_rt->pathClass, pathCI_constructor, 0, pathClassProps, pathClassFuncs, 0, 0);
	}
}

Bool svg_script_execute(GF_SceneGraph *sg, char *utf8_script, GF_DOM_Event *event)
{
	char szFuncName[1024];
	JSBool ret;
	jsval rval;
	GF_DOM_Event *prev_event = NULL;
	char *sep = strchr(utf8_script, '(');

	if (!sep) {
		strcpy(szFuncName, utf8_script);
		strcat(szFuncName, "(evt)");
		utf8_script = szFuncName;
	}
	prev_event = JS_GetPrivate(sg->svg_js->js_ctx, sg->svg_js->event);
	JS_SetPrivate(sg->svg_js->js_ctx, sg->svg_js->event, event);
	ret = JS_EvaluateScript(sg->svg_js->js_ctx, sg->svg_js->global, utf8_script, strlen(utf8_script), 0, 0, &rval);
	JS_SetPrivate(sg->svg_js->js_ctx, sg->svg_js->event, prev_event);

	/*clean-up*/
	JS_GC(sg->svg_js->js_ctx);

	if (ret==JS_FALSE) {
		char *sep = strchr(utf8_script, '(');
		if (!sep) return 0;
		sep[0] = 0;
		JS_LookupProperty(sg->svg_js->js_ctx, sg->svg_js->global, utf8_script, &rval);
		sep[0] = '(';
		if (JSVAL_IS_VOID(rval)) return 0;
	}
	return 1;
}

static void svg_script_predestroy(GF_Node *n, void *eff, Bool is_destroy)
{
	if (is_destroy) {
		GF_SVGJS *svg_js = n->sgprivate->scenegraph->svg_js;
		/*unregister script from parent scene (cf base_scenegraph::sg_reset) */
		gf_list_del_item(n->sgprivate->scenegraph->scripts, n);

		if (svg_js->nb_scripts) {
			svg_js->nb_scripts--;
			if (!svg_js->nb_scripts) {
				gf_sg_ecmascript_del(svg_js->js_ctx);
				dom_js_unload(svg_js->js_ctx, svg_js->global);
				free(svg_js);
				n->sgprivate->scenegraph->svg_js = NULL;
				assert(svg_rt);
				svg_rt->nb_inst--;
				if (!svg_rt->nb_inst) {
					free(svg_rt);
					svg_rt = NULL;
				}
			}
		}
	}
}

static GF_Err JSScript_CreateSVGContext(GF_SceneGraph *sg)
{
	GF_SVGJS *svg_js;
	GF_SAFEALLOC(svg_js, GF_SVGJS);
	/*create new ecmascript context*/
	svg_js->js_ctx = gf_sg_ecmascript_new();
	if (!svg_js->js_ctx) {
		free(svg_js);
		return GF_SCRIPT_ERROR;
	}
	if (!svg_rt) {
		GF_SAFEALLOC(svg_rt, GF_SVGuDOM);
		uDOM_SETUP_CLASS(svg_rt->globalClass, "global", JSCLASS_HAS_PRIVATE, global_getProperty, JS_PropertyStub, JS_FinalizeStub);
		uDOM_SETUP_CLASS(svg_rt->rgbClass, "SVGRGBColor", JSCLASS_HAS_PRIVATE, rgb_getProperty, rgb_setProperty, baseCI_finalize);
		uDOM_SETUP_CLASS(svg_rt->rectClass, "SVGRect", JSCLASS_HAS_PRIVATE, rect_getProperty, rect_setProperty, baseCI_finalize);
		uDOM_SETUP_CLASS(svg_rt->pointClass, "SVGPoint", JSCLASS_HAS_PRIVATE, point_getProperty, point_setProperty, baseCI_finalize);
		uDOM_SETUP_CLASS(svg_rt->matrixClass, "SVGMatrix", JSCLASS_HAS_PRIVATE, JS_PropertyStub, JS_PropertyStub, baseCI_finalize);
		uDOM_SETUP_CLASS(svg_rt->pathClass, "SVGPath", JSCLASS_HAS_PRIVATE, path_getProperty, JS_PropertyStub, pathCI_finalize);
	}
	svg_rt->nb_inst++;

	svg_js->script_execute = svg_script_execute;
	svg_js->handler_execute = svg_script_execute_handler;
	sg->svg_js = svg_js;
	/*load SVG & DOM APIs*/
	svg_init_js_api(sg);
	return GF_OK;
}

GF_DOMText *svg_get_text_child(GF_Node *node)
{
	GF_ChildNodeItem *child;
	GF_DOMText *txt;
	txt = NULL;
	child = ((SVG_Element*)node)->children;
	if (! child) return NULL;
	while (child) {
		txt = (GF_DOMText*)child->node;
		if ((txt->sgprivate->tag==TAG_DOMText) && txt->textContent) return txt;
		txt = NULL;
		child = child->next;
	}
	return NULL;
}

static Bool svg_js_load_script(GF_Node *script, char *file)
{
	FILE *jsf;
	char *jsscript;
	u32 fsize;
	Bool success = 1;
	JSBool ret;
	jsval rval;
	GF_SVGJS *svg_js;

	svg_js = script->sgprivate->scenegraph->svg_js;
	jsf = fopen(file, "rb");
	if (!jsf) return 0;

	fseek(jsf, 0, SEEK_END);
	fsize = ftell(jsf);
	fseek(jsf, 0, SEEK_SET);
	jsscript = malloc(sizeof(char)*(fsize+1));
	fread(jsscript, sizeof(char)*fsize, 1, jsf);
	fclose(jsf);
	jsscript[fsize] = 0;

	/*for handler, only load code*/
	if (script->sgprivate->tag==TAG_SVG_handler) {
		GF_DOMText *txt = gf_dom_add_text_node(script, jsscript);
		txt->type = GF_DOM_TEXT_INSERTED;
		return 1;
	}

	ret = JS_EvaluateScript(svg_js->js_ctx, svg_js->global, jsscript, sizeof(char)*fsize, 0, 0, &rval);
	if (ret==JS_FALSE) success = 0;

	free(jsscript);
	return success;
}

#include <gpac/download.h>
#include <gpac/network.h>

/* Download is performed asynchronously, so the script loading must be performed in this function */
static void JS_SVG_NetIO(void *cbck, GF_NETIO_Parameter *param)
{
	GF_Err e;
	JSFileDownload *jsdnload = (JSFileDownload *)cbck;
	GF_Node *node = jsdnload->node;

	e = param->error;
	if (param->msg_type==GF_NETIO_DATA_TRANSFERED) {
		const char *szCache = gf_dm_sess_get_cache_name(jsdnload->sess);
		if (!svg_js_load_script(jsdnload->node, (char *) szCache))
			e = GF_SCRIPT_ERROR;
		else
			e = GF_OK;

	} else {
		if (!e) {
			/* the download is going on */
			return;
		} else {
			/* there was an error during the download */
		}
	}

	/*destroy current download session (ie, destroy ourselves)*/
	gf_dm_sess_del(jsdnload->sess);
	free(jsdnload);

	if (e) {
		GF_JSAPIParam par;
		par.info.e = e;
		par.info.msg = "Cannot fetch script";
		ScriptAction(node->sgprivate->scenegraph, GF_JSAPI_OP_MESSAGE, NULL, &par);
	}
}

void JSScript_LoadSVG(GF_Node *node)
{
	GF_DOMText *txt;
	GF_SVGJS *svg_js;
	JSBool ret;
	jsval rval;
	GF_FieldInfo href_info;

	if (!node->sgprivate->scenegraph->svg_js) {
		if (JSScript_CreateSVGContext(node->sgprivate->scenegraph) != GF_OK) return;
	}

	/*register script width parent scene (cf base_scenegraph::sg_reset) */
	gf_list_add(node->sgprivate->scenegraph->scripts, node);

	svg_js = node->sgprivate->scenegraph->svg_js;
	if (!node->sgprivate->UserCallback) {
		svg_js->nb_scripts++;
		node->sgprivate->UserCallback = svg_script_predestroy;
	}
	/*if href download the script file*/
	if (gf_svg_get_attribute_by_tag(node, TAG_SVG_ATT_xlink_href, 0, 0, &href_info) == GF_OK) {
		GF_JSAPIParam par;
		char *url;
		GF_Err e;
		JSFileDownload *jsdnload;
		XMLRI *xmlri = (XMLRI *)href_info.far_ptr;
	
		/* getting the base uri of the scene and concatenating */
		ScriptAction(node->sgprivate->scenegraph, GF_JSAPI_OP_GET_SCENE_URI, node, &par);
		url = NULL;
		if (par.uri.url) url = gf_url_concatenate(par.uri.url, xmlri->string);
		if (!url) url = strdup(xmlri->string);

		/* if the file is local, we don't need to download it */
		if (!strstr(url, "://") || !strnicmp(url, "file://", 7)) {
			svg_js_load_script(node, url);
		} else {

			/* getting a download manager */
			par.dnld_man = NULL;
			ScriptAction(node->sgprivate->scenegraph, GF_JSAPI_OP_GET_DOWNLOAD_MANAGER, NULL, &par);
			if (!par.dnld_man) return;

			GF_SAFEALLOC(jsdnload, JSFileDownload);
			jsdnload->node = node;
			jsdnload->sess = gf_dm_sess_new(par.dnld_man, url, 0, JS_SVG_NetIO, jsdnload, &e);
			free(url);
			if (!jsdnload->sess) {
				free(jsdnload);
			}
		}
	} 
	/*for scripts only, execute*/
	else if (node->sgprivate->tag == TAG_SVG_script) {
		txt = svg_get_text_child(node);
		if (!txt) return;
		ret = JS_EvaluateScript(svg_js->js_ctx, svg_js->global, txt->textContent, strlen(txt->textContent), 0, 0, &rval);
		if (ret==JS_FALSE) {
			_ScriptMessage(node->sgprivate->scenegraph, GF_SCRIPT_ERROR, "SVG: Invalid script");
			return;
		}
	}
}

Bool svg_script_execute_handler(GF_Node *node, GF_DOM_Event *event)
{
	GF_DOMText *txt;
	GF_SVGJS *svg_js;
	JSBool ret = JS_FALSE;
	GF_DOM_Event *prev_event = NULL;
	jsval fval, rval;

	txt = svg_get_text_child(node);
	if (!txt) return 0;

	GF_LOG(GF_LOG_DEBUG, GF_LOG_INTERACT, ("[DOM Events] Executing script code from handler\n"));

	svg_js = node->sgprivate->scenegraph->svg_js;

	prev_event = JS_GetPrivate(svg_js->js_ctx, svg_js->event);
	/*break loops*/
	if (prev_event && (prev_event->type==event->type) && (prev_event->target==event->target)) 
		return 0;
	JS_SetPrivate(svg_js->js_ctx, svg_js->event, event);

	if (JS_LookupProperty(svg_js->js_ctx, svg_js->global, txt->textContent, &fval) && !JSVAL_IS_VOID(fval) ) {
		if (svg_script_execute(node->sgprivate->scenegraph, txt->textContent, event)) 
			ret = JS_TRUE;
	} else {
		ret = JS_EvaluateScript(svg_js->js_ctx, svg_js->global, txt->textContent, strlen(txt->textContent), 0, 0, &rval);
	}
	JS_SetPrivate(svg_js->js_ctx, svg_js->event, prev_event);

	if (ret==JS_FALSE) {
		_ScriptMessage(node->sgprivate->scenegraph, GF_SCRIPT_ERROR, "SVG: Invalid handler textContent");
		return 0;
	}
	return 1;
}

#endif

#endif

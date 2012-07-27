/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2000-2012
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
/*MPEG4 tags (for internal nodes)*/
#include <gpac/nodes_mpeg4.h>


#ifndef GPAC_DISABLE_VRML

static void format_sftime_string(Fixed _val, char *str)
{
	u32 h, m, s;
	Bool neg = 0;
	Float val = FIX2FLT(_val);
	if (val<0) {
		val = -val;
		neg = 1;
	}
	h = (u32) (val/3600);
	m = (u32) (val/60) - h*60;
	s = (u32) (val) - h*3600 - m*60;
	sprintf(str, "%s%02d:%02d:%02d", neg ? "-" : "", h, m, s);
}

static void valuator_get_output(M_Valuator *p, GenMFField *inMFField, u32 inType, Bool do_sum, u32 i, SFVec4f *output, u32 *num_out)
{
	
	switch (inType) {
	case GF_SG_VRML_MFINT32:
	{
		SFInt32 sfi = ((MFInt32 *)inMFField)->vals[i];
		Fixed vi = INT2FIX(sfi);
		output->x = gf_mulfix(p->Factor1, vi) + p->Offset1;
		output->y = gf_mulfix(p->Factor2, vi) + p->Offset2;
		output->z = gf_mulfix(p->Factor3, vi) + p->Offset3;
		output->q = gf_mulfix(p->Factor4, vi) + p->Offset4;
	}
		break;
	case GF_SG_VRML_MFFLOAT:
	{
		Fixed sff = ((MFFloat *)inMFField)->vals[i];
		output->x = gf_mulfix(p->Factor1, sff) + p->Offset1;
		output->y = gf_mulfix(p->Factor2, sff) + p->Offset2;
		output->z = gf_mulfix(p->Factor3, sff) + p->Offset3;
		output->q = gf_mulfix(p->Factor4, sff) + p->Offset4;
	}
		break;
	case GF_SG_VRML_MFCOLOR:
	{
		SFColor sfc = ((MFColor *)inMFField)->vals[i];
		output->x = gf_mulfix(p->Factor1, sfc.red) + p->Offset1;
		output->y = gf_mulfix(p->Factor2, sfc.green) + p->Offset2;
		output->z = gf_mulfix(p->Factor3, sfc.blue) + p->Offset3;
		output->q = p->Offset4;
		*num_out = 3;
	}
		break;
	case GF_SG_VRML_MFVEC2F:
	{
		SFVec2f sfv = ((MFVec2f *)inMFField)->vals[i];
		output->x = gf_mulfix(p->Factor1, sfv.x) + p->Offset1;
		output->y = gf_mulfix(p->Factor2, sfv.y) + p->Offset2;
		output->z = p->Offset3;
		output->q = p->Offset4;
		*num_out = 2;
	}
		break;
	case GF_SG_VRML_MFVEC3F:
	{
		SFVec3f sfv = ((MFVec3f *)inMFField)->vals[i];
		output->x = gf_mulfix(p->Factor1, sfv.x) + p->Offset1;
		output->y = gf_mulfix(p->Factor2, sfv.y) + p->Offset2;
		output->z = gf_mulfix(p->Factor3, sfv.z) + p->Offset3;
		output->q = p->Offset4;
		*num_out = 3;
	}
		break;
	case GF_SG_VRML_MFVEC4F:
	case GF_SG_VRML_MFROTATION:
	{
		SFVec4f sfv = ((MFVec4f *)inMFField)->vals[i];
		output->x = gf_mulfix(p->Factor1, sfv.x) + p->Offset1;
		output->y = gf_mulfix(p->Factor2, sfv.y) + p->Offset2;
		output->z = gf_mulfix(p->Factor3, sfv.z) + p->Offset3;
		output->q = gf_mulfix(p->Factor4, sfv.q) + p->Offset4;
		*num_out = 4;
	}
		break;
	case GF_SG_VRML_MFSTRING:
		/*cf below*/
		output->x = output->y = output->z = output->q = 0;
		if (((MFString *)inMFField)->vals[i]) {
			if (!stricmp(((MFString *)inMFField)->vals[i], "true")) {
				output->x = output->y = output->z = output->q = FIX_ONE;
			} else if (!strstr(((MFString *)inMFField)->vals[i], ".")) {
				output->x = INT2FIX( atoi(((MFString *)inMFField)->vals[i]) );
				output->y = output->z = output->q = output->x;
			} else {
				output->x = FLT2FIX( atof(((MFString *)inMFField)->vals[i]) );
				output->y = output->z = output->q = output->x;
			}
		}

		output->x = gf_mulfix(p->Factor1, output->x) + p->Offset1;
		output->y = gf_mulfix(p->Factor2, output->y) + p->Offset2;
		output->z = gf_mulfix(p->Factor3, output->z) + p->Offset3;
		output->q = gf_mulfix(p->Factor4, output->q) + p->Offset4;
		break;

	}
	if (do_sum) {
		output->x = output->x + output->y + output->z + output->q;
		output->y = output->z = output->q = output->x;
	}
}

static void SetValuatorOutput(M_Valuator *p, SFVec4f *inSFField, GenMFField *inMFField, u32 inType)
{
	char str[500];
	u32 i, j;
	GF_Route *r;
	SFVec4f output, sf_out;
	MFVec4f *mf_output = (MFVec4f *)gf_node_get_private((GF_Node*)p);
	u32 count, num_out;
	Bool do_sum;
	output.x = output.y = output.z = output.q = 0;
	sf_out.x = sf_out.y = sf_out.z = sf_out.q = 0;
	if (!p->sgprivate->interact || !mf_output) return;

	do_sum = p->Sum;
	num_out = 1;

	if (!inMFField) {
		count = 1;
		output.x = gf_mulfix(p->Factor1, inSFField->x) + p->Offset1;
		output.y = gf_mulfix(p->Factor2, inSFField->y) + p->Offset2;
		output.z = gf_mulfix(p->Factor3, inSFField->z) + p->Offset3;
		output.q = gf_mulfix(p->Factor4, inSFField->q) + p->Offset4;

		if (do_sum) {
			output.x = output.x + output.y + output.z + output.q;
			output.y = output.z = output.q = output.x;
			do_sum = 0;
		}

		switch (inType) {
		case GF_SG_VRML_SFVEC2F:
			num_out = 2;
			break;
		case GF_SG_VRML_SFVEC3F:
		case GF_SG_VRML_SFCOLOR:
			num_out = 3;
			break;
		case GF_SG_VRML_SFVEC4F:
		case GF_SG_VRML_SFROTATION:
			num_out = 4;
			break;
		}
	} else {
		count = inMFField->count;
	}

	/*allocate temp buffer and compute values*/
	gf_sg_vrml_mf_alloc(mf_output, GF_SG_VRML_MFVEC4F, count);
	for (i=0; i<count; i++) {
		if (inType) {
			valuator_get_output(p, inMFField, inType, do_sum, i, &output, &num_out);
			mf_output->vals[i] = output;
		} else if (!i) {
			mf_output->vals[0] = output;
		}
		if (!i) sf_out = output;
	}
	
	gf_sg_vrml_mf_alloc(&p->outMFColor, GF_SG_VRML_MFCOLOR, count);
	gf_sg_vrml_mf_alloc(&p->outMFFloat, GF_SG_VRML_MFFLOAT, count);
	gf_sg_vrml_mf_alloc(&p->outMFInt32, GF_SG_VRML_MFINT32, count);
	gf_sg_vrml_mf_alloc(&p->outMFRotation, GF_SG_VRML_MFROTATION, count);
	gf_sg_vrml_mf_alloc(&p->outMFString, GF_SG_VRML_MFSTRING, count);	gf_sg_vrml_mf_alloc(&p->outMFVec2f, GF_SG_VRML_MFVEC2F, count);
	gf_sg_vrml_mf_alloc(&p->outMFVec3f, GF_SG_VRML_MFVEC3F, count);

	/*valuator is a special case, all routes are triggered*/
	j=0;
	while ((r = (GF_Route*)gf_list_enum(p->sgprivate->interact->routes, &j))) {
		if (r->FromNode != (GF_Node *)p) continue;
		if (!r->is_setup) gf_sg_route_setup(r);
		if (r->FromField.eventType != GF_SG_EVENT_OUT) continue;

		/*TODO we could optimize more and check if the field has been set or not ...*/
		switch (r->FromField.fieldType) {
		case GF_SG_VRML_SFBOOL:
			p->outSFBool = (Bool) (sf_out.x ? 1 : 0);
			break;
		case GF_SG_VRML_SFFLOAT:
			p->outSFFloat = sf_out.x;
			break;
		case GF_SG_VRML_SFINT32:
			p->outSFInt32 = FIX2INT(sf_out.x);
			break;
		case GF_SG_VRML_SFTIME:
			p->outSFTime = (SFTime) FIX2FLT(sf_out.x);
			break;
		case GF_SG_VRML_SFROTATION:
			p->outSFRotation.x = sf_out.x;
			p->outSFRotation.y = sf_out.y;
			p->outSFRotation.z = sf_out.z;
			p->outSFRotation.q = sf_out.q;
			break;
		case GF_SG_VRML_SFCOLOR:
			p->outSFColor.red = sf_out.x;
			p->outSFColor.green = sf_out.y;
			p->outSFColor.blue = sf_out.z;
			break;
		case GF_SG_VRML_SFVEC2F:
			p->outSFVec2f.x = sf_out.x;
			p->outSFVec2f.y = sf_out.y;
			break;
		case GF_SG_VRML_SFVEC3F:
			p->outSFVec3f.x = sf_out.x;
			p->outSFVec3f.y = sf_out.y;
			p->outSFVec3f.z = sf_out.z;
			break;
		case GF_SG_VRML_SFSTRING:
			if (num_out==1) {
				if (inType==GF_SG_VRML_SFTIME) {
					format_sftime_string(output.x, str);
				} else {
					sprintf(str, "%.6f", FIX2FLT(sf_out.x));
				}
			} else if (num_out==2) {
				sprintf(str, "%.4f %.4f", FIX2FLT(sf_out.x), FIX2FLT(sf_out.y));
			} else if (num_out==3) {
				sprintf(str, "%.3f %.3f %.3f", FIX2FLT(sf_out.x), FIX2FLT(sf_out.y), FIX2FLT(sf_out.z));
			} else if (num_out==4) {
				sprintf(str, "%.2f %.2f %.2f %.2f", FIX2FLT(sf_out.x), FIX2FLT(sf_out.y), FIX2FLT(sf_out.z), FIX2FLT(sf_out.q));
			}
			if (p->outSFString.buffer ) gf_free(p->outSFString.buffer);
			p->outSFString.buffer = gf_strdup(str);
			break;


		case GF_SG_VRML_MFFLOAT:
			gf_sg_vrml_mf_alloc(&p->outMFFloat, GF_SG_VRML_MFFLOAT, count);
			for (i=0; i<count; i++) 
				p->outMFFloat.vals[i] = mf_output->vals[i].x;
			break;
		case GF_SG_VRML_MFINT32:
			gf_sg_vrml_mf_alloc(&p->outMFInt32, GF_SG_VRML_MFINT32, count);
			for (i=0; i<count; i++) 
				p->outMFInt32.vals[i] = FIX2INT(mf_output->vals[i].x);
			break;
		case GF_SG_VRML_MFCOLOR:
			gf_sg_vrml_mf_alloc(&p->outMFColor, GF_SG_VRML_MFCOLOR, count);
			for (i=0; i<count; i++) {
				p->outMFColor.vals[i].red = mf_output->vals[i].x;
				p->outMFColor.vals[i].green = mf_output->vals[i].y;
				p->outMFColor.vals[i].blue = mf_output->vals[i].z;
			}
			break;
		case GF_SG_VRML_MFVEC2F:
			gf_sg_vrml_mf_alloc(&p->outMFVec2f, GF_SG_VRML_MFVEC2F, count);
			for (i=0; i<count; i++) {
				p->outMFVec2f.vals[i].x = mf_output->vals[i].x;
				p->outMFVec2f.vals[i].y = mf_output->vals[i].y;
			}
			break;
		case GF_SG_VRML_MFVEC3F:
			gf_sg_vrml_mf_alloc(&p->outMFVec3f, GF_SG_VRML_MFVEC3F, count);
			for (i=0; i<count; i++) {
				p->outMFVec3f.vals[i].x = mf_output->vals[i].x;
				p->outMFVec3f.vals[i].y = mf_output->vals[i].y;
				p->outMFVec3f.vals[i].z = mf_output->vals[i].z;
			}
			break;
		case GF_SG_VRML_MFROTATION:
			gf_sg_vrml_mf_alloc(&p->outMFRotation, GF_SG_VRML_MFROTATION, count);
			for (i=0; i<count; i++) {
				p->outMFRotation.vals[i].x = mf_output->vals[i].x;
				p->outMFRotation.vals[i].y = mf_output->vals[i].y;
				p->outMFRotation.vals[i].z = mf_output->vals[i].z;
				p->outMFRotation.vals[i].q = mf_output->vals[i].q;
			}
			break;
		case GF_SG_VRML_MFSTRING:
			gf_sg_vrml_mf_alloc(&p->outMFString, GF_SG_VRML_MFSTRING, count);	gf_sg_vrml_mf_alloc(&p->outMFVec2f, GF_SG_VRML_MFVEC2F, count);
			for (i=0; i<count; i++) {
				if (num_out==1) {
					if (inType==GF_SG_VRML_SFTIME) {
						format_sftime_string(mf_output->vals[i].x, str);
					} else {
						sprintf(str, "%g", FIX2FLT(mf_output->vals[i].x));
					}
				} else if (num_out==2) {
					sprintf(str, "%g %g", FIX2FLT(mf_output->vals[i].x), FIX2FLT(mf_output->vals[i].y));
				} else if (num_out==3) {
					sprintf(str, "%g %g %g", FIX2FLT(mf_output->vals[i].x), FIX2FLT(mf_output->vals[i].y), FIX2FLT(mf_output->vals[i].z));
				} else if (num_out==4) {
					sprintf(str, "%g %g %g %g", FIX2FLT(mf_output->vals[i].x), FIX2FLT(mf_output->vals[i].y), FIX2FLT(mf_output->vals[i].z), FIX2FLT(mf_output->vals[i].q));
				}
				if (p->outMFString.vals[i]) gf_free(p->outMFString.vals[i]);
				p->outMFString.vals[i] = gf_strdup(str);
			}
			break;
		}
		if (r->IS_route) {
			gf_sg_route_activate(r);
		} else {
			gf_sg_route_queue(p->sgprivate->scenegraph, r);
		}
	}
}


/*
valuator spec (9.4.2.116.2)
"In the special case of a scalar input type (e.g. SFBool, SFInt32) that is cast to a vectorial output type (e.g.
SFVec2f), for all components i of output.i, input.i shall take the value of the scalar input type, after appropriate type
conversion"
*/

static void Valuator_SetInSFBool(GF_Node *n, GF_Route *route)
{
	SFVec4f val;
	M_Valuator *_this = (M_Valuator *) n;
	val.x = val.y = val.z = val.q = _this->inSFBool ? FIX_ONE : 0;
	SetValuatorOutput(_this, &val, NULL, GF_SG_VRML_SFBOOL);
}
static void Valuator_SetInSFFloat(GF_Node *n, GF_Route *route)
{
	SFVec4f val;
	M_Valuator *_this = (M_Valuator *) n;
	val.x = val.y = val.z = val.q = _this->inSFFloat;
	SetValuatorOutput(_this, &val, NULL, GF_SG_VRML_SFFLOAT);
}
static void Valuator_SetInSFInt32(GF_Node *n, GF_Route *route)
{
	SFVec4f val;
	M_Valuator *_this = (M_Valuator *) n;
	val.x = val.y = val.z = val.q = INT2FIX(_this->inSFInt32);
	SetValuatorOutput(_this, &val, NULL, GF_SG_VRML_SFINT32);
}
static void Valuator_SetInSFTime(GF_Node *n, GF_Route *route)
{
	SFVec4f val;
	M_Valuator *_this = (M_Valuator *) n;
	val.x = val.y = val.z = val.q = FLT2FIX(_this->inSFTime);
	SetValuatorOutput(_this, &val, NULL, GF_SG_VRML_SFTIME);
}
static void Valuator_SetInSFColor(GF_Node *n, GF_Route *route)
{
	SFVec4f val;
	M_Valuator *_this = (M_Valuator *) n;
	val.x = _this->inSFColor.red;
	val.y = _this->inSFColor.green;
	val.z = _this->inSFColor.blue;
	val.q = 0;
	SetValuatorOutput(_this, &val, NULL, GF_SG_VRML_SFCOLOR);
}
static void Valuator_SetInSFVec2f(GF_Node *n, GF_Route *route)
{
	SFVec4f val;
	M_Valuator *_this = (M_Valuator *) n;
	val.x = _this->inSFVec2f.x;
	val.y = _this->inSFVec2f.y;
	val.z = val.q = 0;
	SetValuatorOutput(_this, &val, NULL, GF_SG_VRML_SFVEC2F);
}
static void Valuator_SetInSFVec3f(GF_Node *n, GF_Route *route)
{
	SFVec4f val;
	M_Valuator *_this = (M_Valuator *) n;
	val.x = _this->inSFVec3f.x;
	val.y = _this->inSFVec3f.y;
	val.z = _this->inSFVec3f.z;
	val.q = 0;
	SetValuatorOutput(_this, &val, NULL, GF_SG_VRML_SFVEC3F);
}
static void Valuator_SetInSFRotation(GF_Node *n, GF_Route *route)
{
	SFVec4f val;
	M_Valuator *_this = (M_Valuator *) n;
	val = _this->inSFRotation;
	SetValuatorOutput(_this, &val, NULL, GF_SG_VRML_SFROTATION);
}

/*
valuator spec (9.4.2.116.2)
Convert if the content of the string represents an int, float or
double value. ‘Boolean’ string values 'true' and 'false' are
converted to 1.0 and 0.0 respectively. Any other string is converted to 0.0
*/
static void Valuator_SetInSFString(GF_Node *n, GF_Route *route)
{
	SFVec4f val;
	M_Valuator *_this = (M_Valuator *) n;
	val.x = val.y = val.z = val.q = 0;
	if (! _this->inSFString.buffer) return;
	if (!stricmp(_this->inSFString.buffer, "true")) {
		val.x = val.y = val.z = val.q = FIX_ONE;
	} else if (!strstr(_this->inSFString.buffer, ".")) {
		val.x = INT2FIX( atoi(_this->inSFString.buffer) );
		val.y = val.z = val.q = val.x;
	} else {
		val.x = FLT2FIX( atof(_this->inSFString.buffer) );
		val.y = val.z = val.q = val.x;
	}
	SetValuatorOutput(_this, &val, NULL, GF_SG_VRML_SFSTRING);
}

static void Valuator_SetInMFColor(GF_Node *n, GF_Route *route)
{
	M_Valuator *_this = (M_Valuator *) n;
	SetValuatorOutput(_this, NULL, (GenMFField *) &_this->inMFColor, GF_SG_VRML_MFCOLOR);
}

static void Valuator_SetInMFFloat(GF_Node *n, GF_Route *route)
{
	M_Valuator *_this = (M_Valuator *) n;
	SetValuatorOutput(_this, NULL, (GenMFField *) &_this->inMFFloat, GF_SG_VRML_MFFLOAT);
}
static void Valuator_SetInMFInt32(GF_Node *n, GF_Route *route)
{
	M_Valuator *_this = (M_Valuator *) n;
	SetValuatorOutput(_this, NULL, (GenMFField *) &_this->inMFInt32, GF_SG_VRML_MFINT32);
} 
static void Valuator_SetInMFVec2f(GF_Node *n, GF_Route *route)
{
	M_Valuator *_this = (M_Valuator *) n;
	SetValuatorOutput(_this, NULL, (GenMFField *) &_this->inMFVec2f, GF_SG_VRML_MFVEC2F);
}
static void Valuator_SetInMFVec3f(GF_Node *n, GF_Route *route)
{
	M_Valuator *_this = (M_Valuator *) n;
	SetValuatorOutput(_this, NULL, (GenMFField *) &_this->inMFVec3f, GF_SG_VRML_MFVEC3F);
}
static void Valuator_SetInMFRotation(GF_Node *n, GF_Route *route)
{
	M_Valuator *_this = (M_Valuator *) n;
	SetValuatorOutput(_this, NULL, (GenMFField *) &_this->inMFRotation, GF_SG_VRML_MFROTATION);
}
static void Valuator_SetInMFString(GF_Node *n, GF_Route *route)
{
	M_Valuator *_this = (M_Valuator *) n;
	SetValuatorOutput(_this, NULL, (GenMFField *) &_this->inMFString, GF_SG_VRML_MFSTRING);
}


static void valuator_destroy(GF_Node *node, void *rs, Bool is_destroy)
{
	if (is_destroy) {
		MFVec4f *stack = (MFVec4f *)gf_node_get_private(node);
		gf_sg_vrml_field_pointer_del(stack, GF_SG_VRML_MFROTATION);
	}
}

Bool InitValuator(M_Valuator *node)
{
	MFVec4f *temp = gf_sg_vrml_field_pointer_new(GF_SG_VRML_MFROTATION);
	if (!temp) return 1;

	node->on_inSFTime = Valuator_SetInSFTime;
	node->on_inSFBool = Valuator_SetInSFBool;
	node->on_inSFColor = Valuator_SetInSFColor;
	node->on_inSFInt32 = Valuator_SetInSFInt32;
	node->on_inSFFloat = Valuator_SetInSFFloat;
	node->on_inSFVec2f = Valuator_SetInSFVec2f;
	node->on_inSFVec3f = Valuator_SetInSFVec3f;
	node->on_inSFRotation = Valuator_SetInSFRotation;
	node->on_inSFString = Valuator_SetInSFString;
	node->on_inMFColor = Valuator_SetInMFColor;
	node->on_inMFInt32 = Valuator_SetInMFInt32;
	node->on_inMFFloat = Valuator_SetInMFFloat;
	node->on_inMFVec2f = Valuator_SetInMFVec2f;
	node->on_inMFVec3f = Valuator_SetInMFVec3f;
	node->on_inMFRotation = Valuator_SetInMFRotation;
	node->on_inMFString = Valuator_SetInMFString;

	gf_node_set_private((GF_Node*)node, temp);
	gf_node_set_callback_function((GF_Node*)node, valuator_destroy);
	
	return 1;
}


#endif /*GPAC_DISABLE_VRML*/

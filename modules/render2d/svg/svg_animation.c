/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Copyright (c) Cyril Concolato 2004
 *					All rights reserved
 *
 *  This file is part of GPAC / SVG Rendering sub-project
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
#include "svg_stacks.h"

#ifndef GPAC_DISABLE_SVG

static Fixed SVG_Interpolate(Fixed keyValue1, Fixed keyValue2, Fixed fraction)
{
	return gf_mulfix(keyValue2 - keyValue1, fraction) + keyValue1;
}


/* Color functions */
/* c = a + b */
static void SVG_AddColor(SVG_Color a, SVG_Color b, SVG_Color *c)
{
	c->red   = a.red   + b.red;
	c->green = a.green + b.green;
	c->blue  = a.blue  + b.blue;
}

/* c = k*a */
static void SVG_MulColor(SVG_Color a, Fixed k, SVG_Color *c)
{
	c->red   = k * a.red;
	c->green = k * a.green;
	c->blue  = k * a.blue;
}

/* max(0, min(1, color_component)) */
static void SVG_ClampColor(SVG_Color *a)
{
	a->red   = MAX(0, MIN(FIX_ONE, a->red));
	a->green = MAX(0, MIN(FIX_ONE, a->green));
	a->blue  = MAX(0, MIN(FIX_ONE, a->blue));
}

/* a = b */
static void SVG_AssignColor(SVG_Color *a, SVG_Color b)
{
	a->red   = b.red;
	a->green = b.green;
	a->blue  = b.blue;
}

static void SVG_AccumulateColor(u8 accumulate, 
								u32 nb_iterations, 
								SVG_Color current,
								SVG_Color last,
								SVG_Color *accumulated)
{
	if (accumulate == SMILAccumulateValue_none) {
		SVG_AssignColor(accumulated, current);
	} else {
		SVG_Color tmp;
		SVG_MulColor(last, INT2FIX(nb_iterations), &tmp);
		SVG_AddColor(current, tmp, &tmp);
		SVG_ClampColor(&tmp);
		SVG_AssignColor(accumulated, tmp);
	}
}

static void SVG_ApplyColor(u8 additive, 
						   SVG_Color dom_value, 
						   SVG_Color *target, 
						   SVG_Color toApply)
{
	if (additive == SMILAdditiveValue_replace) {
		SVG_AssignColor(target, toApply);
	} else {
		SVG_Color tmp;
		SVG_AddColor(toApply, *target, &tmp);
		SVG_ClampColor(&tmp);
		SVG_AssignColor(target, tmp);
	}
}
/* end of color */

/* Motion functions */
static void SVG_AssignPoint(SVG_Point *a, SVG_Point b)
{
	a->x = b.x;
	a->y = b.y;
}

static void SVG_MulPoint(SVG_Point a, Fixed k, SVG_Point *c)
{
	c->x = a.x * k;
	c->y = a.y * k;
}

static void SVG_AddPoint(SVG_Point a, SVG_Point b, SVG_Point *c)
{
	c->x = a.x + b.x;
	c->y = a.y + b.y;
}

static void SVG_AccumulateMotion(u8 accumulate, 
								 u32 nb_iterations, 
								 SVG_Point current,
								 SVG_Point last,
								 SVG_Point *accumulated)
{
	if (accumulate == SMILAccumulateValue_none) {
		SVG_AssignPoint(accumulated, current);
	} else {
		SVG_Point tmp;
		SVG_MulPoint(last, INT2FIX(nb_iterations), &tmp);
		SVG_AddPoint(current, tmp, accumulated);
	}
}

static void SVG_ApplyMotion(u8 additive, 
							SVG_Matrix dom_value, 
							SVG_Matrix *target, 
							SVG_Point toApply)
{
	if (additive == SMILAdditiveValue_replace) {
		target->m[2] = toApply.x;
		target->m[5] = toApply.y;
	} else {
		target->m[2] = dom_value.m[2] + toApply.x;
		target->m[5] = dom_value.m[5] + toApply.y;
	}
}
/* Motion functions end */

/* SVG_Length functions */
static void SVG_AssignLength(SVG_Length *a, SVG_Length b)
{
	a->number = b.number;
}

static void SVG_MulLength(SVG_Length a, Fixed k, SVG_Length *c)
{
	c->number = a.number * k;
}

static void SVG_AddLength(SVG_Length a, SVG_Length b, SVG_Length *c) 
{
	c->number = a.number + b.number;
}

static void SVG_AccumulateLength(u8 accumulate, 
								 u32 nb_iterations, 
								 SVG_Length current,
								 SVG_Length last,
								 SVG_Length *accumulated)
{
	if (accumulate == SMILAccumulateValue_none) {
		SVG_AssignLength(accumulated, current);
	} else {
		SVG_Length tmp;
		SVG_MulLength(last, INT2FIX(nb_iterations), &tmp);
		SVG_AddLength(current, tmp, accumulated);
	}
}

static void SVG_ApplyLength(u8 additive, 
							SVG_Length dom_value, 
							SVG_Length *target, 
							SVG_Length toApply)
{
	if (additive == SMILAdditiveValue_replace) {
		SVG_AssignLength(target, toApply);
	} else {
		SVG_AddLength(dom_value, toApply, target);
	}
}
/* end of Length functions */

/* Inheritable float functions */
static void SVG_AssignIFloat(SVGInheritableFloat *a, SVGInheritableFloat b)
{
	a->value = b.value;
}

static void SVG_MulIFloat(SVGInheritableFloat a, Fixed k, SVGInheritableFloat *c)
{
	c->value = a.value * k;
}

static void SVG_AddIFloat(SVGInheritableFloat a, SVGInheritableFloat b, SVGInheritableFloat *c)
{
	c->value = a.value + b.value;
}

static void SVG_AccumulateIFloat(u8 accumulate, u32 nb_iterations, 
								SVGInheritableFloat current,
								SVGInheritableFloat last,
								SVGInheritableFloat *accumulated)
{
	if (accumulate == SMILAccumulateValue_none) {
		SVG_AssignIFloat(accumulated, current);
	} else {
		SVGInheritableFloat tmp;
		SVG_MulIFloat(last, INT2FIX(nb_iterations), &tmp);
		SVG_AddIFloat(current, tmp, accumulated);
	}
}

static void SVG_ApplyIFloat(u8 additive, 
							SVGInheritableFloat dom_value, 
							SVGInheritableFloat *target, 
							SVGInheritableFloat toApply)
{
	if (additive == SMILAdditiveValue_replace) {
		SVG_AssignIFloat(target, toApply);
	} else {
		SVG_AddIFloat(dom_value, toApply, target);
	}
}
/* end of IFloat functions */

/* Transform functions */
static void SVG_AccumulateTransform(u8 transformType, 
									u8 accumulate, 
									u32 nb_iterations, 
									void *current,
									void *last, 
									SVG_Matrix *accumulated)
{
	gf_mx2d_init((*accumulated));
	if (accumulate == SMILAccumulateValue_none) {
		switch(transformType) {
		case SVG_TRANSFORM_TRANSLATE:
			gf_mx2d_add_translation(accumulated, ((SVG_Point *)current)->x, ((SVG_Point *)current)->y);
			break;
		case SVG_TRANSFORM_SCALE:
			gf_mx2d_add_scale(accumulated, ((SVG_Point *)current)->x, ((SVG_Point *)current)->y);
			break;
		case SVG_TRANSFORM_ROTATE:
			gf_mx2d_add_rotation(accumulated, ((SVG_Point_Angle *)current)->x, ((SVG_Point_Angle *)current)->y,
										   ((SVG_Point_Angle *)current)->angle);
			break;
		case SVG_TRANSFORM_SKEWX:
		case SVG_TRANSFORM_SKEWY:
			/* TODO: */
			break;
		}
	} else {
		switch(transformType) {
		case SVG_TRANSFORM_TRANSLATE:
			gf_mx2d_add_translation(accumulated, ((SVG_Point *)current)->x + nb_iterations*((SVG_Point *)last)->x,
											  ((SVG_Point *)current)->y + nb_iterations*((SVG_Point *)last)->y);
			break;
		case SVG_TRANSFORM_SCALE:
			gf_mx2d_add_scale(accumulated, ((SVG_Point *)current)->x + nb_iterations*((SVG_Point *)last)->x,
						   				((SVG_Point *)current)->y + nb_iterations*((SVG_Point *)last)->y);
			break;
		case SVG_TRANSFORM_ROTATE:
			gf_mx2d_add_rotation(accumulated, ((SVG_Point_Angle *)current)->x + nb_iterations*((SVG_Point_Angle *)last)->x,
						   				((SVG_Point_Angle *)current)->y + nb_iterations*((SVG_Point_Angle *)last)->y,
										(((SVG_Point_Angle *)current)->angle + nb_iterations*((SVG_Point_Angle *)last)->angle));
			break;
		case SVG_TRANSFORM_SKEWX:
		case SVG_TRANSFORM_SKEWY:
			/* TODO: */
			break;
		}
	}
}

static void SVG_ApplyTransform(u8 transformType, 
							   u8 additive, 
							   SVG_Matrix dom_value,
							   SVG_Matrix *target, 
							   SVG_Matrix toApply)
{
	if (additive == SMILAdditiveValue_replace) {
		gf_mx2d_copy((*target), toApply);
	} else {
		gf_mx2d_add_matrix(target, &toApply);
	}
}
/* end of Transform functions */


static void SVG_Apply(SMIL_AnimationStack *stack, void *value) 
{
	void *last_specified_value = SMIL_GetLastSpecifiedValue(stack);
	switch(stack->targetAttributeType) {
	case SVG_Color_datatype:
		{
			SVG_Color tmp;
			if (stack->accumulate) {
				SVG_AccumulateColor(*(stack->accumulate), 
									stack->nb_iterations, 
									*(SVG_Color *)value,
									*(SVG_Color *)last_specified_value,
									&tmp);
				SVG_ApplyColor(*(stack->additive), 
							   *(SVG_Color *)stack->init_value,
							   (SVG_Color *)stack->targetAttribute, 
							   tmp);
			} else {
				/* should be a set element */
				SVG_ApplyColor(SMILAccumulateValue_none, 
							   *(SVG_Color *)stack->init_value,
							   (SVG_Color *)stack->targetAttribute, 
							   *(SVG_Color *)value);
			}
		}
		gf_node_dirty_set(stack->target_element, GF_SG_SVG_APPEARANCE_DIRTY, 0);
		gf_sr_invalidate(stack->compositor, NULL);
		return;

	case SVG_Paint_datatype:
		{
			SVG_Color tmp;
			if (stack->accumulate) {
				SVG_AccumulateColor(*(stack->accumulate), 
									stack->nb_iterations, 
									*(SVG_Color *)value,
									*((SVG_Paint *)last_specified_value)->color,
									&tmp);
				SVG_ApplyColor(*(stack->additive), 
							   *((SVG_Paint *)stack->init_value)->color,
							   ((SVG_Paint *)stack->targetAttribute)->color, 
							   tmp);
			} else {
				SVG_ApplyColor(SMILAccumulateValue_none, 
							   *((SVG_Paint *)stack->init_value)->color,
							   ((SVG_Paint *)stack->targetAttribute)->color, 
							   *(SVG_Color *)value);
			}
			/* TODO: fix me */
			((SVG_Paint *)stack->targetAttribute)->paintType = SVG_PAINTTYPE_COLOR;
		}
		gf_node_dirty_set(stack->target_element, GF_SG_SVG_APPEARANCE_DIRTY, 0);
		gf_sr_invalidate(stack->compositor, NULL);
		return;

	case SVG_StrokeWidthValue_datatype:
	case SVG_Length_datatype:
	case SVG_Coordinate_datatype:
		{
			SVG_Length tmp;
			/* TODO: what if the values are in different units */
			if (stack->accumulate) {
				SVG_AccumulateLength(*(stack->accumulate), 
									 stack->nb_iterations,
									 *(SVG_Length *)value, 
									 *(SVG_Length *)last_specified_value, 
									 &tmp);
				SVG_ApplyLength(*(stack->additive), 
					            *(SVG_Length *)stack->init_value, 
								(SVG_Length *)stack->targetAttribute, 
								tmp);
			} else {
				SVG_ApplyLength(SMILAccumulateValue_none, 
								*(SVG_Length *)stack->init_value, 
								(SVG_Length *)stack->targetAttribute, 
								*(SVG_Length *)value);
			}
		}
		break;
	case SVG_TransformList_datatype:
		{
			SVG_Matrix tmp;
			if (stack->accumulate) {
				SVG_AccumulateTransform(stack->transformType, 
										*(stack->accumulate), 
										stack->nb_iterations,
										value, 
										last_specified_value, 
										&tmp);
				SVG_ApplyTransform(stack->transformType, 
								   *(stack->additive), 
								   *(SVG_Matrix *)stack->init_value,
								   stack->targetAttribute, 
								   tmp);
			} else {
				SVG_ApplyTransform(stack->transformType, 
								   SMILAccumulateValue_none, 
								   *(SVG_Matrix *)stack->init_value,
								   stack->targetAttribute, 
								   *(GF_Matrix2D *)value);
			}
		}
		break;
	case SVG_DisplayValue_datatype:
	case SVG_VisibilityValue_datatype:
		{
			u8 *targetValue = (u8 *)stack->targetAttribute;
			u8 *typed_value = (u8 *)value;
			*targetValue = *typed_value;
		}
		break;
	case SVG_Motion_datatype:
		{ 
			SVG_Point tmp;
			if (stack->accumulate) {
				SVG_AccumulateMotion(*(stack->accumulate), 
									 stack->nb_iterations, 
									 *(SVG_Point *)value, 
									 *(SVG_Point *)last_specified_value, 
									 &tmp);
				SVG_ApplyMotion(*(stack->additive), 
							    *(SVG_Matrix *)stack->init_value,
								(SVG_Matrix *)stack->targetAttribute, 
								tmp);
			} else {
				SVG_ApplyMotion(SMILAccumulateValue_none, 
					            *(SVG_Matrix *)stack->init_value,
								(SVG_Matrix *)stack->targetAttribute, 
								*(SVG_Point *)value);
			}
		}
		break;
	case SVG_StrokeDashOffsetValue_datatype:
	case SVG_OpacityValue_datatype:
	case SVG_FontSizeValue_datatype:
	case SVG_StrokeMiterLimitValue_datatype:
		{
			SVGInheritableFloat tmp;
			/* TODO: what if the values are in different units */
			if (stack->accumulate) {
				SVG_AccumulateIFloat(*(stack->accumulate), 
									 stack->nb_iterations, 
									 *(SVGInheritableFloat *)value, 
									 *(SVGInheritableFloat *)last_specified_value, 
									 &tmp);
				SVG_ApplyIFloat(*(stack->additive), 
					            *(SVGInheritableFloat *)stack->init_value,
								(SVGInheritableFloat *)stack->targetAttribute, 
								tmp);
			} else {
				SVG_ApplyIFloat(SMILAccumulateValue_none, 
					            *(SVGInheritableFloat *)stack->init_value,
					            (SVGInheritableFloat *)stack->targetAttribute, 
								*(SVGInheritableFloat *)value);
			}
		}
		break;
	default:
		//fprintf(stdout, "Animation type not supported: %d\n",stack->targetAttributeType);
		return;
	}
	gf_node_dirty_set(stack->target_element, 0, 0);
	gf_sr_invalidate(stack->compositor, NULL);
}

static void SVG_InterpolateAndAccumulate(SMIL_AnimationStack *stack, 
										 Fixed interpFraction, 
										 void *value1, 
										 void *value2)
{
	switch(stack->targetAttributeType) {
	case SVG_Paint_datatype:
		{
			SVG_Color toColor;
			SVG_Color *colorA = ((SVG_Paint *)value1)->color;
			SVG_Color *colorB = ((SVG_Paint *)value2)->color;
			toColor.red = SVG_Interpolate(colorA->red, colorB->red, interpFraction);
			toColor.green = SVG_Interpolate(colorA->green, colorB->green, interpFraction);
			toColor.blue = SVG_Interpolate(colorA->blue, colorB->blue, interpFraction);
			SVG_Apply(stack, &toColor);
		}
		break;
	case SVG_StrokeWidthValue_datatype:
	case SVG_Length_datatype:
	case SVG_Coordinate_datatype:
		{
			SVG_Length toLength;
			toLength.number = SVG_Interpolate(((SVG_Length *)value1)->number, 
											  ((SVG_Length *)value2)->number, 
											  interpFraction);
			SVG_Apply(stack, &toLength);
		}
		break;
	case SVG_TransformList_datatype:
		{
			switch(stack->transformType) {
			case SVG_TRANSFORM_TRANSLATE:
			case SVG_TRANSFORM_SCALE:
				{
					SVG_Point toPoint;
					SVG_Point *m1 = (SVG_Point *)value1;
					SVG_Point *m2 = (SVG_Point *)value2;
					toPoint.x = SVG_Interpolate(m1->x, m2->x, interpFraction); 
					toPoint.y = SVG_Interpolate(m1->y, m2->y, interpFraction); 
					SVG_Apply(stack, &toPoint);
				}
				break;
			case SVG_TRANSFORM_ROTATE:
				{
					SVG_Point_Angle toPointAngle;
					SVG_Point_Angle *a1 = (SVG_Point_Angle *)value1;
					SVG_Point_Angle *a2 = (SVG_Point_Angle *)value2;
					toPointAngle.angle = SVG_Interpolate(a1->angle, a2->angle, interpFraction); 
					toPointAngle.x = SVG_Interpolate(a1->x, a2->x, interpFraction); 
					toPointAngle.y = SVG_Interpolate(a1->y, a2->y, interpFraction); 
					SVG_Apply(stack, &toPointAngle);
				}
				break;
			case SVG_TRANSFORM_SKEWX:
			case SVG_TRANSFORM_SKEWY:
				/* TODO: */
				break;
			}
		}
		break;
	case SVG_Motion_datatype:
		{
			SVG_Point toPoint;
			SVG_Point *pointA = ((SVG_Point *)value1);
			SVG_Point *pointB = ((SVG_Point *)value2);
			toPoint.x = SVG_Interpolate(pointA->x, pointB->x, interpFraction);
			toPoint.y = SVG_Interpolate(pointA->y, pointB->y, interpFraction);
			SVG_Apply(stack, &toPoint);
		}
		break;
	case SVG_StrokeDashOffsetValue_datatype:
	case SVG_OpacityValue_datatype:
	case SVG_FontSizeValue_datatype:
	case SVG_StrokeMiterLimitValue_datatype:
		{
			SVGInheritableFloat toFloat;
			toFloat.value = SVG_Interpolate(((SVGInheritableFloat *)value1)->value, 
											((SVGInheritableFloat *)value2)->value, 
											interpFraction);
			SVG_Apply(stack, &toFloat);
		}
		break;
	case SVG_DisplayValue_datatype:
	case SVG_VisibilityValue_datatype:
		{
			u8 toInt;
			toInt = FIX2INT(SVG_Interpolate(INT2FIX(*(u8 *)value1), INT2FIX(*(u8 *)value2), interpFraction));
			SVG_Apply(stack, &toInt);
		}
		break;
	default:
		//fprintf(stdout, "Animation type not supported: %d\n", stack->targetAttributeType);
		break;
	}
}


static void SVG_SetDiscreteValueAndAccumulate(SMIL_AnimationStack *stack, void *value) 
{
	SVG_Apply(stack, value);
}

static void SVG_SaveBaseValue(SMIL_AnimationStack *stack)
{
	switch(stack->targetAttributeType) {
	case SVG_Color_datatype:
		GF_SAFEALLOC(stack->init_value, sizeof(SVG_Color))
		memcpy(stack->init_value, stack->targetAttribute, sizeof(SVG_Color));
		break;
	case SVG_Paint_datatype:
		GF_SAFEALLOC(stack->init_value, sizeof(SVG_Paint))
		memcpy((SVG_Paint *)stack->init_value, (SVG_Paint *)stack->targetAttribute, sizeof(SVG_Paint));
		GF_SAFEALLOC(((SVG_Paint *)stack->init_value)->color, sizeof(SVG_Color))
		memcpy(((SVG_Paint *)stack->init_value)->color, ((SVG_Paint *)stack->targetAttribute)->color, sizeof(SVG_Color));
		break;
	case SVG_StrokeWidthValue_datatype:
	case SVG_Length_datatype:
	case SVG_Coordinate_datatype:
		GF_SAFEALLOC(stack->init_value, sizeof(SVG_Length))
		memcpy(stack->init_value, stack->targetAttribute, sizeof(SVG_Length));
		break;
	case SVG_TransformList_datatype:
		GF_SAFEALLOC(stack->init_value, sizeof(SVG_Matrix))
		memcpy(stack->init_value, (SVG_Matrix *)stack->targetAttribute, sizeof(SVG_Matrix));
/*		fprintf(stdout, "a=%f, b=%f, c=%f, d=%f, e=%f, f=%f\n", FIX2FLT(((SVG_Matrix *)stack->init_value)->m[0]),
															  FIX2FLT(((SVG_Matrix *)stack->init_value)->m[1]),
															  FIX2FLT(((SVG_Matrix *)stack->init_value)->m[2]),
															  FIX2FLT(((SVG_Matrix *)stack->init_value)->m[3]),
															  FIX2FLT(((SVG_Matrix *)stack->init_value)->m[4]),
															  FIX2FLT(((SVG_Matrix *)stack->init_value)->m[5]));*/
		break;
	case SVG_DisplayValue_datatype:
	case SVG_VisibilityValue_datatype:
		GF_SAFEALLOC(stack->init_value, sizeof(u8))
		memcpy(stack->init_value, stack->targetAttribute, sizeof(u8));
		break;
	case SVG_StrokeDashOffsetValue_datatype:
	case SVG_OpacityValue_datatype:
	case SVG_FontSizeValue_datatype:
	case SVG_StrokeMiterLimitValue_datatype:
		GF_SAFEALLOC(stack->init_value, sizeof(SVGInheritableFloat))
		memcpy(stack->init_value, stack->targetAttribute, sizeof(SVGInheritableFloat));
		break;
	case SVG_Motion_datatype:
		/* Motion type is an exception: it gathers to attributes x,y into one point */
		GF_SAFEALLOC(stack->init_value, sizeof(SVG_Point))
		((SVG_Point *)stack->init_value)->x = ((SVG_Matrix *)stack->targetAttribute)->m[2];
		((SVG_Point *)stack->init_value)->y = ((SVG_Matrix *)stack->targetAttribute)->m[5];
		break;
	default:
		fprintf(stdout, "Animation type not supported: %d\n", stack->targetAttributeType);
	}
}

/* TODO: check if the accumulation step needs to be done here ... */
static void SVG_RestoreValue(SMIL_AnimationStack *stack, Bool init_or_last)
{
	void *value = (init_or_last?stack->init_value:SMIL_GetLastSpecifiedValue(stack));
	if (!value) return;

	switch(stack->targetAttributeType) {
	case SVG_Color_datatype:
		if (init_or_last) {
			SVG_ApplyColor(SMILAdditiveValue_replace, 
						   *(SVG_Color *)stack->init_value,
						   (SVG_Color *)stack->targetAttribute, 
						   *(SVG_Color *)value);
		} else {
			SVG_Color tmp;
			SVG_AccumulateColor(*(stack->accumulate), 
								FIX2INT(*stack->repeatCount)-1, 
								*(SVG_Color *)value, 
								*(SVG_Color *)value, 
								&tmp);
			SVG_ApplyColor(*stack->additive, 
						   *(SVG_Color *)stack->init_value,
						   (SVG_Color *)stack->targetAttribute, 
						   tmp);
		}
		break;
	case SVG_Paint_datatype:
		/*TODO: do it with the SVG_Paint type not SVG_Color, otherwise pb with paint.type 
		e.g. will not restore inherit */
		if (init_or_last) {
			SVG_ApplyColor(SMILAdditiveValue_replace, 
						   *((SVG_Paint *)stack->init_value)->color, 
						   ((SVG_Paint *)stack->targetAttribute)->color, 
						   *(SVG_Color *)value);
		} else {
			SVG_Color tmp;
			SVG_AccumulateColor(*(stack->accumulate), 
								FIX2INT(*stack->repeatCount)-1, 
								*(SVG_Color *)value, 
								*(SVG_Color *)value, 
								&tmp);
			SVG_ApplyColor(*stack->additive, 
						   *((SVG_Paint *)stack->init_value)->color, 
						   ((SVG_Paint *)stack->targetAttribute)->color, 
						   tmp);
			/*TODO: fix me : restore type value */
			((SVG_Paint *)stack->targetAttribute)->paintType = SVG_PAINTTYPE_COLOR;
		}
		break;
	case SVG_StrokeWidthValue_datatype:
	case SVG_Length_datatype:
	case SVG_Coordinate_datatype:
		if (init_or_last) {
			SVG_ApplyLength(SMILAdditiveValue_replace, 
							*(SVG_Length *)stack->init_value, 
							(SVG_Length *)stack->targetAttribute, 
							*(SVG_Length *)value);
		} else {
			SVG_Length tmp;
			SVG_AccumulateLength(*(stack->accumulate), 
								 FIX2INT(*stack->repeatCount)-1, 
								 *(SVG_Length *)value, 
								 *(SVG_Length *)value, 
								 &tmp);
			SVG_ApplyLength(*(stack->additive), 
							*(SVG_Length *)stack->init_value, 
							(SVG_Length *)stack->targetAttribute, 
							tmp);
		}
		break;
	case SVG_TransformList_datatype:
		if (init_or_last) {
			SVG_ApplyTransform(stack->transformType, 
							   SMILAdditiveValue_replace, 
							   *(SVG_Matrix *)stack->init_value, 
							   (SVG_Matrix *)stack->targetAttribute, 
							   *(SVG_Matrix *)value);
/*			fprintf(stdout, "a=%f, b=%f, c=%f, d=%f, e=%f, f=%f\n", FIX2FLT(((SVG_Matrix *)value)->m[0]),
																	FIX2FLT(((SVG_Matrix *)value)->m[1]),
																	FIX2FLT(((SVG_Matrix *)value)->m[2]),
																	FIX2FLT(((SVG_Matrix *)value)->m[3]),
																	FIX2FLT(((SVG_Matrix *)value)->m[4]),
																	FIX2FLT(((SVG_Matrix *)value)->m[5]));*/

		} else {
			/*TODO: accumulation ... ? */
			SVG_Matrix tmp;
			SVG_AccumulateTransform(stack->transformType, 
									*(stack->accumulate), 
									FIX2INT(*stack->repeatCount)-1,
									value, 
									value, 
									&tmp);
			SVG_ApplyTransform(stack->transformType, 
							   *stack->additive, 
							   *(SVG_Matrix *)stack->init_value, 
							   (SVG_Matrix *)stack->targetAttribute, 
							   tmp);
/*			fprintf(stdout, "a=%f, b=%f, c=%f, d=%f, e=%f, f=%f\n", FIX2FLT(tmp.m[0]),
																	FIX2FLT(tmp.m[1]),
																	FIX2FLT(tmp.m[2]),
																	FIX2FLT(tmp.m[3]),
																	FIX2FLT(tmp.m[4]),
																	FIX2FLT(tmp.m[5]));*/		
		}
		break;
	case SVG_DisplayValue_datatype:
	case SVG_VisibilityValue_datatype:
		*((u8 *)stack->targetAttribute) = *(u8 *)value;
		break;
	case SVG_Motion_datatype:
		if (init_or_last) {
			SVG_ApplyMotion(SMILAdditiveValue_replace, 
							*(SVG_Matrix *)stack->init_value, 
							(SVG_Matrix *)stack->targetAttribute, 
							*(SVG_Point *)value);
		} else {
			SVG_Point tmp;
			SVG_AccumulateMotion(*(stack->accumulate), 
								 FIX2INT(*stack->repeatCount)-1, 
								 *(SVG_Point *)value, 
								 *(SVG_Point *)value, 
								 &tmp);
			SVG_ApplyMotion(*(stack->additive), 
							*(SVG_Matrix *)stack->init_value, 
							(SVG_Matrix *)stack->targetAttribute, 
							tmp);
		}
		break;
	case SVG_StrokeDashOffsetValue_datatype:
	case SVG_OpacityValue_datatype:
	case SVG_FontSizeValue_datatype:
	case SVG_StrokeMiterLimitValue_datatype:
		if (init_or_last) {
			SVG_ApplyIFloat(SMILAdditiveValue_replace, 
							*(SVGInheritableFloat *)stack->init_value, 
							(SVGInheritableFloat *)stack->targetAttribute, 
							*(SVGInheritableFloat *)value);
		} else {
			SVGInheritableFloat tmp;
			/* TODO: what if the values are in different units */
			SVG_AccumulateIFloat(*(stack->accumulate), 
								 FIX2INT(*stack->repeatCount)-1, 
								 *(SVGInheritableFloat *)value, 
								 *(SVGInheritableFloat *)value, 
								 &tmp);
			SVG_ApplyIFloat(*(stack->additive), 
							*(SVGInheritableFloat *)stack->init_value, 
							(SVGInheritableFloat *)stack->targetAttribute, 
							tmp);
		}
		break;
	default:
		fprintf(stdout, "Animation type not supported: %d\n",stack->targetAttributeType);
	}
}

static void SVG_DeleteStack(SMIL_AnimationStack *stack)
{
	switch(stack->targetAttributeType) {
	case SVG_Paint_datatype:
		free(((SVG_Paint *)stack->init_value)->color);
		free(stack->init_value);
		break;
	case SVG_Color_datatype:
	case SVG_StrokeWidthValue_datatype:
	case SVG_Length_datatype:
	case SVG_Coordinate_datatype:
	case SVG_DisplayValue_datatype:
	case SVG_VisibilityValue_datatype:
	case SVG_StrokeDashOffsetValue_datatype:
	case SVG_OpacityValue_datatype:
	case SVG_FontSizeValue_datatype:
	case SVG_StrokeMiterLimitValue_datatype:
	case SVG_TransformList_datatype:
	case SVG_Motion_datatype:
		free(stack->init_value);
		break;
	default:
		fprintf(stdout, "Animation type not supported: %d\n", stack->targetAttributeType);
	}
}

static void SVG_Init_SMILAnimationStackAPI(SMIL_AnimationStack *stack)
{
	stack->SetDiscreteValueAndAccumulate = SVG_SetDiscreteValueAndAccumulate;
	stack->InterpolateAndAccumulate = SVG_InterpolateAndAccumulate;
	stack->SaveBaseValue = SVG_SaveBaseValue;
	stack->RestoreValue = SVG_RestoreValue;
	stack->DeleteStack = SVG_DeleteStack;
}

void SVG_Init_set(Render2D *sr, GF_Node *node)
{
	SVGsetElement *set = (SVGsetElement *)node;
	SMIL_AnimationStack *stack = SMIL_Init_AnimationStack(sr, node, SMIL_Update_Animation);
	
	SVG_Init_SMILAnimationStackAPI(stack);

	stack->target_element = (GF_Node*)set->xlink_href.target_element;
	stack->targetAttributeType = set->attributeName.fieldType; 
	stack->targetAttribute = set->attributeName.far_ptr; 

	stack->begins = &(set->begin); 
	stack->dur = &(set->dur); 
	stack->ends = &(set->end); 
	stack->repeatCount = &(set->repeatCount); 
	stack->repeatDur = &(set->repeatDur); 
	stack->restart = &(set->restart); 
	stack->min = &(set->min); 
	stack->max = &(set->max); 
	stack->fill = &(set->fill); 
	stack->to = &(set->to);
}

void SVG_Init_animate(Render2D *sr, GF_Node *node)
{
	SVGanimateElement *animate = (SVGanimateElement *)node;
	SMIL_AnimationStack *stack = SMIL_Init_AnimationStack(sr, node, SMIL_Update_Animation);
	
	SVG_Init_SMILAnimationStackAPI(stack);

	stack->target_element = (GF_Node*)animate->xlink_href.target_element;
	stack->targetAttributeType = animate->attributeName.fieldType; 
	stack->targetAttribute = animate->attributeName.far_ptr; 

	stack->begins = &(animate->begin); 
	stack->dur = &(animate->dur); 
	stack->ends = &(animate->end); 
	stack->repeatCount = &(animate->repeatCount); 
	stack->repeatDur = &(animate->repeatDur); 
	stack->restart = &(animate->restart); 
	stack->min = &(animate->min); 
	stack->max = &(animate->max); 
	stack->fill = &(animate->fill); 
	stack->keyTimes = &(animate->keyTimes);
	stack->keySplines = &(animate->keySplines);
	stack->values = &(animate->values);
	stack->from = &(animate->from);
	stack->to = &(animate->to); 
	stack->by = &(animate->by); 
	stack->calcMode = &(animate->calcMode); 
	stack->additive = &(animate->additive); 
	stack->accumulate = &(animate->accumulate); 
}

void SVG_Init_animateColor(Render2D *sr, GF_Node *node)
{
	SVGanimateColorElement *ac = (SVGanimateColorElement *)node;
	SMIL_AnimationStack *stack = SMIL_Init_AnimationStack(sr, node, SMIL_Update_Animation);
	
	SVG_Init_SMILAnimationStackAPI(stack);

	stack->target_element = (GF_Node*)ac->xlink_href.target_element;
	stack->targetAttributeType = ac->attributeName.fieldType; 
	stack->targetAttribute = ac->attributeName.far_ptr; 

	stack->begins = &(ac->begin); 
	stack->dur = &(ac->dur); 
	stack->ends = &(ac->end); 
	stack->repeatCount = &(ac->repeatCount); 
	stack->repeatDur = &(ac->repeatDur); 
	stack->restart = &(ac->restart); 
	stack->min = &(ac->min); 
	stack->max = &(ac->max); 
	stack->fill = &(ac->fill); 
	stack->keyTimes = &(ac->keyTimes);
	stack->keySplines = &(ac->keySplines);
	stack->values = &(ac->values);
	stack->from = &(ac->from);
	stack->to = &(ac->to); 
	stack->by = &(ac->by); 
	stack->calcMode = &(ac->calcMode); 
	stack->additive = &(ac->additive); 
	stack->accumulate = &(ac->accumulate); 
}

void SVG_Init_animateTransform(Render2D *sr, GF_Node *node)
{
	GF_FieldInfo info;
	SVGanimateTransformElement *at = (SVGanimateTransformElement *)node;
	SMIL_AnimationStack *stack = SMIL_Init_AnimationStack(sr, node, SMIL_Update_Animation);
	
	SVG_Init_SMILAnimationStackAPI(stack);

	stack->target_element = (GF_Node*)at->xlink_href.target_element;
	stack->targetAttributeType = at->attributeName.fieldType; 
	if (!gf_node_get_field_by_name(stack->target_element, "transform", &info)) {
		GF_List *trlist = *(SVG_TransformList *)info.far_ptr;
		SVG_Transform *tr = gf_list_get(trlist, 0);
		if (!tr) {
			GF_SAFEALLOC(tr, sizeof(SVG_Transform));
			gf_mx2d_init(tr->matrix);
			gf_list_add(trlist, tr);
		}
		stack->targetAttribute = &(tr->matrix);
	}
	if (!gf_node_get_field_by_name((GF_Node *)at, "type", &info)) {
		stack->transformType = *(SVG_AnimateTransformTypeValue *)info.far_ptr;
	}

	stack->begins = &(at->begin); 
	stack->dur = &(at->dur); 
	stack->ends = &(at->end); 
	stack->repeatCount = &(at->repeatCount); 
	stack->repeatDur = &(at->repeatDur); 
	stack->restart = &(at->restart); 
	stack->min = &(at->min); 
	stack->max = &(at->max); 
	stack->fill = &(at->fill); 
	stack->keyTimes = &(at->keyTimes);
	stack->keySplines = &(at->keySplines);
	stack->values = &(at->values);
	stack->from = &(at->from);
	stack->to = &(at->to); 
	stack->by = &(at->by); 
	stack->calcMode = &(at->calcMode); 
	stack->additive = &(at->additive); 
	stack->accumulate = &(at->accumulate); 
}

void SVG_Init_animateMotion(Render2D *sr, GF_Node *node)
{
	GF_FieldInfo info;
	SVGanimateMotionElement *am = (SVGanimateMotionElement *)node;
	SMIL_AnimationStack *stack = SMIL_Init_AnimationStack(sr, node, SMIL_Update_Animation);
	
	SVG_Init_SMILAnimationStackAPI(stack);

	stack->target_element = (GF_Node*)am->xlink_href.target_element;
	stack->targetAttributeType = SVG_Motion_datatype; 
	if (!gf_node_get_field_by_name(stack->target_element, "transform", &info)) {
		GF_List *trlist = *(SVG_TransformList *)info.far_ptr;
		SVG_Transform *tr = gf_list_get(trlist, 0);
		if (!tr) {
			GF_SAFEALLOC(tr, sizeof(SVG_Transform));
			gf_mx2d_init(tr->matrix);
			gf_list_add(trlist, tr);
		}
		stack->targetAttribute = &(tr->matrix);
	}

	stack->begins = &(am->begin); 
	stack->dur = &(am->dur); 
	stack->ends = &(am->end); 
	stack->repeatCount = &(am->repeatCount); 
	stack->repeatDur = &(am->repeatDur); 
	stack->restart = &(am->restart); 
	stack->min = &(am->min); 
	stack->max = &(am->max); 
	stack->fill = &(am->fill); 
	stack->keyTimes = &(am->keyTimes);
	stack->keySplines = &(am->keySplines);
	stack->values = &(am->values);
	stack->from = &(am->from);
	stack->to = &(am->to); 
	stack->by = &(am->by); 
	stack->calcMode = &(am->calcMode); 
	stack->additive = &(am->additive); 
	stack->accumulate = &(am->accumulate); 
}

void SVG_Init_discard(Render2D *sr, GF_Node *node)
{
	SVGdiscardElement *d = (SVGdiscardElement *)node;
	SMIL_AnimationStack *stack = SMIL_Init_AnimationStack(sr, node, SMIL_Update_Animation);
	
	SVG_Init_SMILAnimationStackAPI(stack);

	stack->target_element = (GF_Node*)d->xlink_href.target_element;

	stack->begins = &(d->begin); 
	/* 'from', 'to', 'by', 'values', 'keyTimes', 'keySplines', ... are NULL */
}

#endif

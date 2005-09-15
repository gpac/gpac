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
/* max(0, min(1, color_component)) */
static void SVG_ClampColor(SVG_Color *a)
{
	a->red   = MAX(0, MIN(FIX_ONE, a->red));
	a->green = MAX(0, MIN(FIX_ONE, a->green));
	a->blue  = MAX(0, MIN(FIX_ONE, a->blue));
}

/* c = a + b */
static void SVG_AddColor(SVG_Color *a, SVG_Color *b, SVG_Color *c)
{
	c->red   = a->red   + b->red;
	c->green = a->green + b->green;
	c->blue  = a->blue  + b->blue;
	SVG_ClampColor(c);
}

/* c = k*a */
static void SVG_MulColor(SVG_Color *a, Fixed k, SVG_Color *c)
{
	c->red   = k * a->red;
	c->green = k * a->green;
	c->blue  = k * a->blue;
}

/* a = b */
static void SVG_SetColor(SVG_Color *a, SVG_Color *b)
{
	a->red   = b->red;
	a->green = b->green;
	a->blue  = b->blue;
}

static void SVG_ApplyAccumulateColor(u32 nb_iterations, 
									 SVG_Color *current,
									 SVG_Color *last,
									 SVG_Color *accumulated)
{
	SVG_Color tmp;
	SVG_MulColor(last, INT2FIX(nb_iterations), &tmp);
	SVG_AddColor(current, &tmp, &tmp);
	SVG_ClampColor(&tmp);
	SVG_SetColor(accumulated, &tmp);
}

static void SVG_InvalidateColor(SMIL_AnimationStack *stack)
{
	/*
	gf_node_dirty_set(stack->target_element, GF_SG_SVG_APPEARANCE_DIRTY, 0);
	gf_sr_invalidate(stack->compositor, NULL);
	*/
}

static void SVG_InterpolateColor(Fixed interpolation_coefficient, SVG_Color *value1, SVG_Color *value2, SVG_Color *target)
{
	target->red = SVG_Interpolate(value1->red, value2->red, interpolation_coefficient);
	target->green = SVG_Interpolate(value1->green, value2->green, interpolation_coefficient);
	target->blue = SVG_Interpolate(value1->blue, value2->blue, interpolation_coefficient);
}

static void SVG_InitStackValuesColor(SMIL_AnimationStack *stack)
{
	GF_SAFEALLOC(stack->base_value, sizeof(SVG_Color))
	GF_SAFEALLOC(stack->tmp_value, sizeof(SVG_Color))
	memcpy(stack->base_value, stack->targetAttribute, sizeof(SVG_Color));
}

static void SVG_DeleteStackValuesColor(SMIL_AnimationStack *stack)
{
	free(stack->base_value);
	free(stack->tmp_value);
}
/* end of color */

/* Paint functions */
static void SVG_AddPaint(SVG_Paint *a, SVG_Paint *b, SVG_Paint *c)
{
	SVG_AddColor(a->color, b->color, c->color);
	/* TODO: fix me */
	c->paintType = a->paintType;
}

/* c = k*a */
static void SVG_MulPaint(SVG_Paint *a, Fixed k, SVG_Paint *c)
{
	SVG_MulColor(a->color, k, c->color);
	c->paintType = a->paintType;
}

/* a = b */
static void SVG_SetPaint(SVG_Paint *a, SVG_Paint *b)
{
	SVG_SetColor(a->color, b->color);
	a->paintType = b->paintType;
}

static void SVG_ApplyAccumulatePaint(u32 nb_iterations, 
									 SVG_Paint *current,
									 SVG_Paint *last,
									 SVG_Paint *accumulated)
{
	SVG_ApplyAccumulateColor(nb_iterations, current->color, last->color, accumulated->color);
	/* TODO: fix me */
	accumulated->paintType = current->paintType;
}

static void SVG_InvalidatePaint(SMIL_AnimationStack *stack)
{
	gf_node_dirty_set(stack->target_element, GF_SG_SVG_APPEARANCE_DIRTY, 0);
	gf_sr_invalidate(stack->compositor, NULL);
}

static void SVG_InterpolatePaint(Fixed interpolation_coefficient, SVG_Paint *value1, SVG_Paint *value2, SVG_Paint *target)
{
	SVG_InterpolateColor(interpolation_coefficient, value1->color, value2->color, target->color);
	/* TODO: fix me */
	target->paintType = value1->paintType;
}

static void SVG_InitStackValuesPaint(SMIL_AnimationStack *stack)
{
	GF_SAFEALLOC(stack->base_value, sizeof(SVG_Paint))
	GF_SAFEALLOC(((SVG_Paint *)stack->base_value)->color, sizeof(SVG_Color))
	((SVG_Paint *)stack->base_value)->paintType = ((SVG_Paint *)stack->targetAttribute)->paintType;
	memcpy(((SVG_Paint *)stack->base_value)->color, ((SVG_Paint *)stack->targetAttribute)->color, sizeof(SVG_Color));
	GF_SAFEALLOC(stack->tmp_value, sizeof(SVG_Paint))
	GF_SAFEALLOC(((SVG_Paint *)stack->tmp_value)->color, sizeof(SVG_Color))
}

static void SVG_DeleteStackValuesPaint(SMIL_AnimationStack *stack)
{
	free(((SVG_Paint *)stack->base_value)->color);
	free(stack->base_value);
	free(((SVG_Paint *)stack->tmp_value)->color);
	free(stack->tmp_value);
}
/* end of Paint functions */

/* Motion functions */
static void SVG_ApplyAccumulateMotion(u32 nb_iterations, SVG_Matrix *current, SVG_Point *last, SVG_Matrix *accumulated)
{
	accumulated->m[2] = current->m[2] + nb_iterations*last->x;
	accumulated->m[5] = current->m[5] + nb_iterations*last->y;
}

static void SVG_InvalidateMotion(SMIL_AnimationStack *stack)
{
	/* TODO */
}

static void SVG_InterpolateMotion(Fixed interpolation_coefficient, SVG_Point *value1, SVG_Point *value2, SVG_Matrix *target)
{
	target->m[2] = SVG_Interpolate(value1->x, value2->x, interpolation_coefficient);
	target->m[5] = SVG_Interpolate(value1->y, value2->y, interpolation_coefficient);
}

static void SVG_SetMotion(SVG_Matrix *a, SVG_Point *b)
{
	a->m[2] = b->x;
	a->m[5] = b->y;
}

/* SVG_AssignMotion is equal to SVG_AssignTransform */
/* SVG_InitStackValuesMotion is equal to SVG_InitStackValuesTransform */
/* SVG_DeleteStackValuesMotion is equal to SVG_DeleteStackValuesTransform */
/* Motion functions end */

/* SVG_Length functions */
static void SVG_SetLength(SVG_Length *a, SVG_Length *b)
{
	a->number = b->number;
}

static void SVG_MulLength(SVG_Length *a, Fixed k, SVG_Length *c)
{
	c->number = a->number * k;
}

static void SVG_AddLength(SVG_Length *a, SVG_Length *b, SVG_Length *c) 
{
	c->number = a->number + b->number;
}

static void SVG_ApplyAccumulateLength(u32 nb_iterations, SVG_Length *current, SVG_Length *last, SVG_Length *accumulated)
{
	SVG_Length tmp;
	SVG_MulLength(last, INT2FIX(nb_iterations), &tmp);
	SVG_AddLength(current, &tmp, accumulated);
}

static void SVG_InterpolateLength(Fixed interpolation_coefficient, SVG_Length *value1, SVG_Length *value2, SVG_Length *target)
{
	target->number = SVG_Interpolate(value1->number, value2->number, interpolation_coefficient);
}

static void SVG_InitStackValuesLength(SMIL_AnimationStack *stack)
{
	GF_SAFEALLOC(stack->base_value, sizeof(SVG_Length))
	memcpy(stack->base_value, stack->targetAttribute, sizeof(SVG_Length));
	GF_SAFEALLOC(stack->tmp_value, sizeof(SVG_Length))
}

static void SVG_DeleteStackValuesLength(SMIL_AnimationStack *stack)
{
	free(stack->base_value);
	free(stack->tmp_value);
}

static void SVG_InvalidateLength(SMIL_AnimationStack *stack)
{
	/* TODO */
}
/* end of Length functions */

/* Inheritable float functions */
static void SVG_SetIFloat(SVGInheritableFloat *a, SVGInheritableFloat *b)
{
	a->value = b->value;
}

static void SVG_MulIFloat(SVGInheritableFloat *a, Fixed k, SVGInheritableFloat *c)
{
	c->value = a->value * k;
}

static void SVG_AddIFloat(SVGInheritableFloat *a, SVGInheritableFloat *b, SVGInheritableFloat *c)
{
	c->value = a->value + b->value;
}

static void SVG_ApplyAccumulateIFloat(u32 nb_iterations, SVGInheritableFloat *current, SVGInheritableFloat *last, SVGInheritableFloat *accumulated)
{
	SVGInheritableFloat tmp;
	SVG_MulIFloat(last, INT2FIX(nb_iterations), &tmp);
	SVG_AddIFloat(current, &tmp, accumulated);
}

static void SVG_InterpolateIFloat(Fixed interpolation_coefficient, SVGInheritableFloat *value1, SVGInheritableFloat *value2, SVGInheritableFloat *target)
{
	target->value = SVG_Interpolate(value1->value, value2->value, interpolation_coefficient);
}

static void SVG_InitStackValuesIFloat(SMIL_AnimationStack *stack)
{
	GF_SAFEALLOC(stack->base_value, sizeof(SVGInheritableFloat))
	memcpy(stack->base_value, stack->targetAttribute, sizeof(SVGInheritableFloat));
	GF_SAFEALLOC(stack->tmp_value, sizeof(SVGInheritableFloat))
}

static void SVG_DeleteStackValuesIFloat(SMIL_AnimationStack *stack)
{
	free(stack->base_value);
	free(stack->tmp_value);
}

static void SVG_InvalidateIFloat(SMIL_AnimationStack *stack)
{
	/* TODO */
}
/* end of IFloat functions */

/* Transform functions */
static void printMatrix(const char *s, SVG_Matrix *target)
{
	fprintf(stdout, "%s %.2f,%.2f,%.2f,%.2f,%.2f,%.2f\n", s, target->m[0], target->m[1], target->m[2], target->m[3], target->m[4], target->m[5]);
}

static void SVG_SetTranslation(SVG_Matrix *a, SVG_Point *b)
{
	gf_mx2d_init(*a);
	gf_mx2d_add_translation(a, b->x, b->y);
//	printMatrix("\nSet Translation", a);
}

static void SVG_ApplyAccumulateTranslation(u32 nb_iterations, SVG_Matrix *current, SVG_Point *last, SVG_Matrix *accumulated)
{
	SVG_Matrix tmp;
	gf_mx2d_init(tmp);
	gf_mx2d_add_translation(&tmp, 
							current->m[2] + nb_iterations*last->x,
							current->m[5] + nb_iterations*last->y);
//	printMatrix("\nAccumulate Scale\nUnderlying value", accumulated);
//	printMatrix("result", &tmp);
	gf_mx2d_copy(*accumulated, tmp);
}

static void SVG_InterpolateTranslation(Fixed interpolation_coefficient, SVG_Point *value1, SVG_Point *value2, SVG_Matrix *target)
{
	gf_mx2d_init(*target);
	target->m[2] = SVG_Interpolate(value1->x, value2->x, interpolation_coefficient); 
	target->m[5] = SVG_Interpolate(value1->y, value2->y, interpolation_coefficient); 
//	printMatrix("\nInterpolate Translation", target);
}

static void SVG_SetScale(SVG_Matrix *a, SVG_Point *b)
{
	gf_mx2d_init(*a);
	gf_mx2d_add_scale(a, b->x, b->y);
//	printMatrix("\nSet Scale\n", a);
}

static void SVG_ApplyAccumulateScale(u32 nb_iterations, SVG_Matrix *current, SVG_Point *last, SVG_Matrix *accumulated)
{
	SVG_Matrix tmp;
	gf_mx2d_init(tmp);
	gf_mx2d_add_scale(&tmp, 
					  current->m[0] + nb_iterations*last->x,
					  current->m[4] + nb_iterations*last->y);
	printMatrix("\nAccumulate Scale\nUnderlying value", accumulated);
	printMatrix("result", &tmp);
	gf_mx2d_copy(*accumulated, tmp);
}

static void SVG_InterpolateScale(Fixed interpolation_coefficient, SVG_Point *value1, SVG_Point *value2, SVG_Matrix *target)
{
	gf_mx2d_init(*target);
	target->m[0] = SVG_Interpolate(value1->x, value2->x, interpolation_coefficient); 
	target->m[4] = SVG_Interpolate(value1->y, value2->y, interpolation_coefficient); 
//	printMatrix("\nInterpolate Scale\n", target);
}

static void SVG_SetRotate(SVG_Matrix *a, SVG_Point_Angle *b)
{
	gf_mx2d_init(*a);
	gf_mx2d_add_rotation(a, b->x, b->y, b->angle);
//	printMatrix("\nSet Rotate\n", a);
}

static void SVG_ApplyAccumulateRotate(u32 nb_iterations, SVG_Matrix *current, SVG_Point_Angle *last, SVG_Matrix *accumulated)
{
	SVG_Matrix tmp;
	gf_mx2d_init(tmp);
	gf_mx2d_add_rotation(&tmp, nb_iterations*last->x, nb_iterations*last->y, nb_iterations*last->angle);
	gf_mx2d_add_matrix(&tmp, current);
	gf_mx2d_add_matrix(&tmp, accumulated);
	gf_mx2d_copy(*accumulated, tmp);
}

static void SVG_InterpolateRotate(Fixed interpolation_coefficient, SVG_Point_Angle *value1, SVG_Point_Angle *value2, SVG_Matrix *target)
{
	gf_mx2d_init(*target);
	gf_mx2d_add_rotation(target, 
						 SVG_Interpolate(value1->x, value2->x, interpolation_coefficient),
						 SVG_Interpolate(value1->y, value2->y, interpolation_coefficient),
						 SVG_Interpolate(value1->angle, value2->angle, interpolation_coefficient));
//	printMatrix("\nInterpolate Rotate", target);
}

static void SVG_SetSkewX(SVG_Matrix *a, SVG_Matrix *b)
{
}

static void SVG_ApplyAccumulateSkewX(u32 nb_iterations, void *current, void *last, SVG_Matrix *accumulated)
{
	gf_mx2d_init(*accumulated);
	/* TODO */
}

static void SVG_InterpolateSkewX(Fixed interpolation_coefficient, void *value1, void *value2, SVG_Matrix *target)
{
	/* TODO */
}

static void SVG_SetSkewY(SVG_Matrix *a, SVG_Matrix *b)
{
}

static void SVG_ApplyAccumulateSkewY(u32 nb_iterations, void *current, void *last, SVG_Matrix *accumulated)
{
	gf_mx2d_init((*(SVG_Matrix *)accumulated));
	/* TODO */
}

static void SVG_InterpolateSkewY(Fixed interpolation_coefficient, void *value1, void *value2, SVG_Matrix *target)
{
	/* TODO */
}

static void SVG_ApplyAdditiveTransform(SVG_Matrix *underlying_value, SVG_Matrix *toApply, SVG_Matrix *target)
{
	SVG_Matrix tmp;
//	printMatrix("\nAdditive Transform \nunderlying value", underlying_value);
//	printMatrix("to add", toApply);
	gf_mx2d_copy(tmp, *toApply);
	gf_mx2d_add_matrix(&tmp, underlying_value);
	gf_mx2d_copy(*target, tmp);
//	printMatrix("result", target);
}

static void SVG_AssignTransform(SVG_Matrix *a, SVG_Matrix *b)
{
	gf_mx2d_copy(*a, *b);
//	printMatrix("\nAssign", a);
}

static void SVG_InitStackValuesTransform(SMIL_AnimationStack *stack)
{
	GF_SAFEALLOC(stack->base_value, sizeof(SVG_Matrix))
	memcpy(stack->base_value, stack->targetAttribute, sizeof(SVG_Matrix));
	GF_SAFEALLOC(stack->tmp_value, sizeof(SVG_Matrix))
	gf_mx2d_init(*(SVG_Matrix *)stack->tmp_value);
//	printMatrix("\nDOM value", (SVG_Matrix *)stack->base_value);
}

static void SVG_DeleteStackValuesTransform(SMIL_AnimationStack *stack)
{
	free(stack->base_value);
	free(stack->tmp_value);
}

static void SVG_InvalidateTransform(SMIL_AnimationStack *stack)
{
	/* TODO */
}
/* end of Transform functions */

/* keyword functions */
static void SVG_SetKeyword(void *a, void *b)
{
	*(u8 *)a = *(u8 *)b;
}

static void SVG_InterpolateKeyword(Fixed interpolation_coefficient, void *value1, void *value2, void *target) 
{
	if (interpolation_coefficient < INT2FIX(0.5)) 
		*(u8 *)target = *(u8 *)value1;
	else 
		*(u8 *)target = *(u8 *)value2;
}

static void SVG_InvalidateKeyword(SMIL_AnimationStack *stack)
{
	/* TODO */
}


static void SVG_InitStackValuesKeyword(SMIL_AnimationStack *stack)
{
	GF_SAFEALLOC(stack->base_value, sizeof(u8))
	memcpy(stack->base_value, stack->targetAttribute, sizeof(u8));
	GF_SAFEALLOC(stack->tmp_value, sizeof(u8))
}

static void SVG_DeleteStackValuesKeyword(SMIL_AnimationStack *stack)
{
	free(stack->base_value);
	free(stack->tmp_value);
}
/* end of keyword functions */

/* Dash Array animation functions */
static void SVG_SetDashArray(SVG_StrokeDashArrayValue *a, SVG_StrokeDashArrayValue *b)
{
	a->type = b->type;
	if (a->array.vals) free(a->array.vals);
	a->array.count = b->array.count;
	a->array.vals = malloc(sizeof(Fixed)*a->array.count);
	memcpy(a->array.vals, b->array.vals, sizeof(Fixed)*a->array.count);
}

static void SVG_MulDashArray(SVG_StrokeDashArrayValue *a, Fixed k, SVG_StrokeDashArrayValue *c)
{
	/* TODO */
}

static void SVG_AddDashArray(SVG_StrokeDashArrayValue *a, SVG_StrokeDashArrayValue *b, SVG_StrokeDashArrayValue *c)
{
	/* TODO */
}

static void SVG_ApplyAccumulateDashArray(u32 nb_iterations, 
										 SVG_StrokeDashArrayValue *current, 
										 SVG_StrokeDashArrayValue *last, 
										 SVG_StrokeDashArrayValue *accumulated)
{
	/* TODO */
}

static void SVG_InterpolateDashArray(Fixed interpolation_coefficient, 
									 SVG_StrokeDashArrayValue *value1, 
									 SVG_StrokeDashArrayValue *value2, 
									 SVG_StrokeDashArrayValue *target)
{
	u32 i;
	target->type = value1->type;
	if (target->array.vals) free(target->array.vals);
	target->array.count = value1->array.count;
	GF_SAFEALLOC(target->array.vals, sizeof(Fixed)*target->array.count)
	for (i = 0; i < target->array.count; i++) {
		target->array.vals[i] = SVG_Interpolate(value1->array.vals[i], 
												value2->array.vals[i], 
												interpolation_coefficient);
	}
}

static void SVG_InitStackValuesDashArray(SMIL_AnimationStack *stack)
{
	GF_SAFEALLOC(stack->base_value, sizeof(SVG_StrokeDashArrayValue))
	GF_SAFEALLOC(stack->tmp_value, sizeof(SVG_StrokeDashArrayValue))
}

static void SVG_DeleteStackValuesDashArray(SMIL_AnimationStack *stack)
{
	if (((SVG_StrokeDashArrayValue *)stack->base_value)->array.vals) 
		free(((SVG_StrokeDashArrayValue *)stack->base_value)->array.vals);
	free(stack->base_value);
	if (((SVG_StrokeDashArrayValue *)stack->tmp_value)->array.vals) 
		free(((SVG_StrokeDashArrayValue *)stack->tmp_value)->array.vals);
	free(stack->tmp_value);
}

static void SVG_InvalidateDashArray(SMIL_AnimationStack *stack)
{
	/* TODO */
}
/* end of Dash Array animation functions */

/* SVG Rect animation functions */
static void SVG_SetRect(SVG_Rect *a, SVG_Rect *b)
{
	memcpy(a, b, sizeof(SVG_Rect));
}

static void SVG_MulRect(SVG_Rect *a, Fixed k, SVG_Rect *c)
{
	c->x = a->x * k;
	c->y = a->y * k;
	c->width = a->width * k;
	c->height = a->height * k;
}

static void SVG_AddRect(SVG_Rect *a, SVG_Rect *b, SVG_Rect *c)
{
	c->x = a->x + b->x;
	c->y = a->y + b->y;
	c->width = a->width + b->width;
	c->height= a->height + b->height;
}

static void SVG_ApplyAccumulateRect(u32 nb_iterations, SVG_Rect *current, SVG_Rect *last, SVG_Rect*accumulated)
{
	accumulated->x = current->x + nb_iterations*last->x;
	accumulated->y = current->y + nb_iterations*last->y;
	accumulated->width = current->width + nb_iterations*last->width;
	accumulated->height= current->height + nb_iterations*last->height;
}

static void SVG_InterpolateRect(Fixed interpolation_coefficient, SVG_Rect *value1, SVG_Rect *value2, SVG_Rect *target)
{
	target->x = SVG_Interpolate(value1->x, value2->x, interpolation_coefficient);
	target->y = SVG_Interpolate(value1->y, value2->y, interpolation_coefficient);
	target->width = SVG_Interpolate(value1->width, value2->width, interpolation_coefficient);
	target->height = SVG_Interpolate(value1->height, value2->height, interpolation_coefficient);

}

static void SVG_InitStackValuesRect(SMIL_AnimationStack *stack)
{
	GF_SAFEALLOC(stack->base_value, sizeof(SVG_Rect))
	memcpy(stack->base_value, stack->targetAttribute, sizeof(SVG_Rect));
	GF_SAFEALLOC(stack->tmp_value, sizeof(SVG_Rect))
}

static void SVG_DeleteStackValuesRect(SMIL_AnimationStack *stack)
{
	free(stack->base_value);
	free(stack->tmp_value);
}

static void SVG_InvalidateRect(SMIL_AnimationStack *stack)
{
	/* TODO */
}
/* end of SVG Rect animation functions */

/* SVG IRI animation functions */
static void SVG_SetIRI(SVG_IRI *a, SVG_IRI *b)
{
	memcpy(a, b, sizeof(SVG_IRI));
}

static void SVG_MulIRI(SVG_IRI *a, Fixed k, SVG_IRI *c)
{
}

static void SVG_AddIRI(SVG_IRI *a, SVG_Rect *b, SVG_IRI *c)
{
}

static void SVG_ApplyAccumulateIRI(u32 nb_iterations, SVG_IRI *current, SVG_IRI *last, SVG_IRI*accumulated)
{
}

static void SVG_InterpolateIRI(Fixed interpolation_coefficient, SVG_IRI *value1, SVG_IRI *value2, SVG_IRI *target)
{
	if (interpolation_coefficient < INT2FIX(0.5)) 
		SVG_SetIRI(target,value1);
	else 
		SVG_SetIRI(target,value2);
}

static void SVG_InitStackValuesIRI(SMIL_AnimationStack *stack)
{
	GF_SAFEALLOC(stack->base_value, sizeof(SVG_IRI))
	memcpy(stack->base_value, stack->targetAttribute, sizeof(SVG_IRI));
	GF_SAFEALLOC(stack->tmp_value, sizeof(SVG_IRI))
}

static void SVG_DeleteStackValuesIRI(SMIL_AnimationStack *stack)
{
	free(stack->base_value);
	free(stack->tmp_value);
}

static void SVG_InvalidateIRI(SMIL_AnimationStack *stack)
{
	/* TODO */
}
/* end of SVG IRI animation functions */

/* SVG Points functions */
static void SVG_SetPoints(SVG_Points *a, SVG_Points *b)
{
	u32 i, count;

	for (i = 0; i < gf_list_count(*a); i++) {
		SVG_Point *pt = gf_list_get(*a, i);
		free(pt);
	}
	gf_list_reset(*a);
	
	count = gf_list_count(*b);
	for (i = 0; i < count; i ++) {
		SVG_Point *ptb = gf_list_get(*b, i);
		SVG_Point *pta;
		GF_SAFEALLOC(pta, sizeof(SVG_Point))
		pta->x = ptb->x;
		pta->y = ptb->y;
		gf_list_add(*a, pta);
	}
}

static void SVG_AddPoints(SVG_Points *a, SVG_Points *b, SVG_Points *c)
{
}

static void SVG_ApplyAccumulatePoints(u32 nb_iterations, SVG_Points *current, SVG_Points *last, SVG_Points *accumulated)
{
}

static void SVG_InterpolatePoints(Fixed interpolation_coefficient, SVG_Points *value1, SVG_Points *value2, SVG_Points *target)
{
	u32 i, count;

	for (i = 0; i < gf_list_count(*target); i++) {
		SVG_Point *pt = gf_list_get(*target, i);
		free(pt);
	}
	gf_list_reset(*target);
	
	count = gf_list_count(*value1);
	for (i = 0; i < count; i ++) {
		SVG_Point *pt3;
		SVG_Point *pt1 = gf_list_get(*value1, i);
		SVG_Point *pt2 = gf_list_get(*value2, i);
		GF_SAFEALLOC(pt3, sizeof(SVG_Point))
		pt3->x = SVG_Interpolate(pt1->x, pt2->x, interpolation_coefficient);
		pt3->y = SVG_Interpolate(pt1->y, pt2->y, interpolation_coefficient);
		gf_list_add(*target, pt3);
	}
}

static void SVG_InitStackValuesPoints(SMIL_AnimationStack *stack)
{
	u32 i, count;
	SVG_Point *c, *nc;
	GF_SAFEALLOC(stack->base_value, sizeof(SVG_Points));
	*(SVG_Points*)stack->base_value = gf_list_new();
	count = gf_list_count(*(SVG_Points *)stack->targetAttribute);
	for (i = 0; i < count; i ++) {
		c = gf_list_get(*(SVG_Points *)stack->targetAttribute, i);
		nc = malloc(sizeof(SVG_Point));
		memcpy(nc, c, sizeof(SVG_Point));
		gf_list_add(*(SVG_Points *)stack->base_value, nc);
	}
	GF_SAFEALLOC(stack->tmp_value, sizeof(SVG_Points));
	*(SVG_Points*)stack->tmp_value = gf_list_new();
}

static void SVG_DeleteStackValuesPoints(SMIL_AnimationStack *stack)
{
	SVG_DeletePoints(*(SVG_Points *)stack->base_value);
	SVG_DeletePoints(*(SVG_Points *)stack->tmp_value);
}

static void SVG_InvalidatePoints(SMIL_AnimationStack *stack)
{
	/* TODO */
}
/* end of SVG Coordinates animation functions */

/* SVG Coordinates functions */
static void SVG_SetCoords(GF_List *a, GF_List *b)
{
}

static void SVG_AddCoords(GF_List *a, SVG_Rect *b, GF_List *c)
{
}

static void SVG_ApplyAccumulateCoords(u32 nb_iterations, GF_List *current, GF_List *last, GF_List*accumulated)
{
}

static void SVG_InterpolateCoords(Fixed interpolation_coefficient, GF_List *value1, GF_List *value2, GF_List *target)
{
}

static void SVG_InitStackValuesCoords(SMIL_AnimationStack *stack)
{
}

static void SVG_DeleteStackValuesCoords(SMIL_AnimationStack *stack)
{
}

static void SVG_InvalidateCoords(SMIL_AnimationStack *stack)
{
	/* TODO */
}
/* end of SVG Coordinates animation functions */

/* SVG String functions */
static void SVG_SetString(SVG_String *a, SVG_String *b)
{
	if (*a) free(*a);
	*a = strdup(*b);
}

static void SVG_MulString(SVG_String *a, Fixed k, SVG_String *c)
{
}

static void SVG_AddString(SVG_String *a, SVG_Rect *b, SVG_String *c)
{
}

static void SVG_ApplyAccumulateString(u32 nb_iterations, SVG_String *current, SVG_String *last, SVG_String*accumulated)
{
}

static void SVG_InterpolateString(Fixed interpolation_coefficient, SVG_String *value1, SVG_String *value2, SVG_String *target)
{
	if (interpolation_coefficient < INT2FIX(0.5)) 
		SVG_SetString(target,value1);
	else 
		SVG_SetString(target,value2);
}

static void SVG_InitStackValuesString(SMIL_AnimationStack *stack)
{
	GF_SAFEALLOC(stack->base_value, sizeof(SVG_String))
	*(SVG_String *)stack->base_value = strdup(*(SVG_String *)stack->targetAttribute);
	GF_SAFEALLOC(stack->tmp_value, sizeof(SVG_String))
}

static void SVG_DeleteStackValuesString(SMIL_AnimationStack *stack)
{
	if (*(SVG_String *)stack->base_value) free(*(SVG_String *)stack->base_value);
	free(stack->base_value);
	if (*(SVG_String *)stack->tmp_value) free(*(SVG_String *)stack->tmp_value);
	free(stack->tmp_value);
}

static void SVG_InvalidateString(SMIL_AnimationStack *stack)
{
	/* TODO */
}
/* end of SVG String animation functions */

/* SVG Font Family functions */
static void SVG_SetFontFamily(SVG_FontFamilyValue *a, SVG_FontFamilyValue *b)
{
	a->type = b->type;
	if (a->value) free(a->value);
	a->value = strdup(b->value);
}

static void SVG_MulFontFamily(SVG_FontFamilyValue *a, Fixed k, SVG_FontFamilyValue *c)
{
}

static void SVG_AddFontFamily(SVG_FontFamilyValue *a, SVG_FontFamilyValue *b, SVG_FontFamilyValue *c)
{
}

static void SVG_ApplyAccumulateFontFamily(u32 nb_iterations, SVG_FontFamilyValue *current, SVG_FontFamilyValue *last, SVG_FontFamilyValue*accumulated)
{
}

static void SVG_InterpolateFontFamily(Fixed interpolation_coefficient, SVG_FontFamilyValue *value1, SVG_FontFamilyValue *value2, SVG_FontFamilyValue *target)
{
	if (interpolation_coefficient < INT2FIX(0.5)) 
		SVG_SetFontFamily(target,value1);
	else 
		SVG_SetFontFamily(target,value2);
}

static void SVG_InitStackValuesFontFamily(SMIL_AnimationStack *stack)
{
	GF_SAFEALLOC(stack->base_value, sizeof(SVG_FontFamilyValue))
	((SVG_FontFamilyValue *)stack->base_value)->type = ((SVG_FontFamilyValue *)stack->targetAttribute)->type;
	((SVG_FontFamilyValue *)stack->base_value)->value = strdup(((SVG_FontFamilyValue *)stack->targetAttribute)->value);
	GF_SAFEALLOC(stack->tmp_value, sizeof(SVG_FontFamilyValue))
}

static void SVG_DeleteStackValuesFontFamily(SMIL_AnimationStack *stack)
{
	if (((SVG_FontFamilyValue *)stack->base_value)->value) free(((SVG_FontFamilyValue *)stack->base_value)->value);
	free(stack->base_value);
	if (((SVG_FontFamilyValue *)stack->tmp_value)->value) free(((SVG_FontFamilyValue *)stack->tmp_value)->value);
	free(stack->tmp_value);
}

static void SVG_InvalidateFontFamily(SMIL_AnimationStack *stack)
{
	/* TODO */
}
/* end of SVG String animation functions */

/* Dash Path functions */
static void SVG_SetPath(SVG_PathData *a, SVG_PathData *b)
{
	u32 i, count;

	for (i = 0; i < gf_list_count(a->path_commands); i++) {
		u8 *command = gf_list_get(a->path_commands, i);
		free(command);
	}
	gf_list_reset(a->path_commands);
	for (i = 0; i < gf_list_count(a->path_points); i++) {
		SVG_Point *pt = gf_list_get(a->path_points, i);
		free(pt);
	}
	gf_list_reset(a->path_points);
	
	count = gf_list_count(b->path_commands);
	for (i = 0; i < count; i ++) {
		u8 *nc = malloc(sizeof(u8));
		*nc = *(u8*)gf_list_get(b->path_commands, i);
		gf_list_add(a->path_commands, nc);
	}
	count = gf_list_count(b->path_points);
	for (i = 0; i < count; i ++) {
		SVG_Point *ptb = gf_list_get(b->path_points, i);
		SVG_Point *pta;
		GF_SAFEALLOC(pta, sizeof(SVG_Point))
		pta->x = ptb->x;
		pta->y = ptb->y;
		gf_list_add(a->path_points, pta);
	}
}

static void SVG_MulPath(SVG_PathData *a, Fixed k, SVG_PathData *c)
{
	/* TODO */
}

static void SVG_AddPath(SVG_PathData *a, SVG_PathData *b, SVG_PathData *c)
{
	/* TODO */
}

static void SVG_ApplyAccumulatePath(u32 nb_iterations, SVG_PathData *current, SVG_PathData *last, SVG_PathData*accumulated)
{
	/* TODO */
}

static void SVG_InterpolatePath(Fixed interpolation_coefficient, SVG_PathData *value1, SVG_PathData *value2, SVG_PathData *target)
{
	u32 i, count;

	for (i = 0; i < gf_list_count(target->path_commands); i++) {
		u8 *command = gf_list_get(target->path_commands, i);
		free(command);
	}
	for (i = 0; i < gf_list_count(target->path_points); i++) {
		SVG_Point *pt = gf_list_get(target->path_points, i);
		free(pt);
	}
	gf_list_reset(target->path_commands);
	gf_list_reset(target->path_points);
	
	count = gf_list_count(value1->path_commands);
	for (i = 0; i < count; i ++) {
		u8 *nc = malloc(sizeof(u8));
		*nc = *(u8*)gf_list_get(value1->path_commands, i);
		gf_list_add(target->path_commands, nc);
	}
	count = gf_list_count(value1->path_points);
	for (i = 0; i < count; i ++) {
		SVG_Point *pt3;
		SVG_Point *pt1 = gf_list_get(value1->path_points, i);
		SVG_Point *pt2 = gf_list_get(value2->path_points, i);
		GF_SAFEALLOC(pt3, sizeof(SVG_Point))
		pt3->x = SVG_Interpolate(pt1->x, pt2->x, interpolation_coefficient);
		pt3->y = SVG_Interpolate(pt1->y, pt2->y, interpolation_coefficient);
		gf_list_add(target->path_points, pt3);
	}
}

static void SVG_InitStackValuesPath(SMIL_AnimationStack *stack)
{
	GF_SAFEALLOC(stack->base_value, sizeof(SVG_PathData))
	((SVG_PathData *)stack->base_value)->path_commands = gf_list_new();
	((SVG_PathData *)stack->base_value)->path_points = gf_list_new();
	GF_SAFEALLOC(stack->tmp_value, sizeof(SVG_PathData))
	((SVG_PathData *)stack->tmp_value)->path_commands = gf_list_new();
	((SVG_PathData *)stack->tmp_value)->path_points = gf_list_new();
}

static void SVG_DeleteStackValuesPath(SMIL_AnimationStack *stack)
{
	SVG_DeletePath(((SVG_PathData *)stack->base_value));
	SVG_DeletePath(((SVG_PathData *)stack->tmp_value));
}

static void SVG_InvalidatePath(SMIL_AnimationStack *stack)
{
	/* TODO */
}
/* end of Path animation functions */

static void SVG_DeleteStack(SMIL_AnimationStack *stack)
{
	stack->DeleteStackValues(stack);
}

static void SVG_Init_SMILAnimationStackAPI(SMIL_AnimationStack *stack)
{
	switch(stack->targetAttributeType)
	{
	case SVG_Color_datatype:
		stack->InitStackValues = SVG_InitStackValuesColor;
		stack->DeleteStackValues = SVG_DeleteStackValuesColor;
		stack->Set = SVG_SetColor;
		stack->Assign = SVG_SetColor;
		stack->Interpolate = SVG_InterpolateColor;
		stack->ApplyAdditive = SVG_AddColor;
		stack->ApplyAccumulate = SVG_ApplyAccumulateColor;
		stack->Invalidate = SVG_InvalidateColor;
		break;
	case SVG_Paint_datatype:
		stack->InitStackValues = SVG_InitStackValuesPaint;
		stack->DeleteStackValues = SVG_DeleteStackValuesPaint;
		stack->Set = SVG_SetPaint;
		stack->Assign = SVG_SetPaint;
		stack->Interpolate = SVG_InterpolatePaint;
		stack->ApplyAdditive = SVG_AddPaint;
		stack->ApplyAccumulate = SVG_ApplyAccumulatePaint;
		stack->Invalidate = SVG_InvalidatePaint;
		break;
	case SVG_StrokeWidthValue_datatype:
	case SVG_Length_datatype:
	case SVG_Coordinate_datatype:
		stack->InitStackValues = SVG_InitStackValuesLength;
		stack->DeleteStackValues = SVG_DeleteStackValuesLength;
		stack->Set = SVG_SetLength;
		stack->Assign = SVG_SetLength;
		stack->Interpolate = SVG_InterpolateLength;
		stack->ApplyAdditive = SVG_AddLength;
		stack->ApplyAccumulate = SVG_ApplyAccumulateLength;
		stack->Invalidate = SVG_InvalidateLength;
		break;
	case SVG_TransformList_datatype:
		stack->InitStackValues = SVG_InitStackValuesTransform;
		stack->DeleteStackValues = SVG_DeleteStackValuesTransform;
		stack->ApplyAdditive = SVG_ApplyAdditiveTransform;
		stack->Invalidate = SVG_InvalidateTransform;
		stack->Assign = SVG_AssignTransform;
		switch (stack->transformType) {
		case SVG_TRANSFORM_TRANSLATE:
			stack->Set = SVG_SetTranslation;
			stack->Interpolate = SVG_InterpolateTranslation;
			stack->ApplyAccumulate = SVG_ApplyAccumulateTranslation;
			break;
		case SVG_TRANSFORM_SCALE:
			stack->Set = SVG_SetScale;
			stack->Interpolate = SVG_InterpolateScale;
			stack->ApplyAccumulate = SVG_ApplyAccumulateScale;
			break;
		case SVG_TRANSFORM_ROTATE:
			stack->Set = SVG_SetRotate;
			stack->Interpolate = SVG_InterpolateRotate;
			stack->ApplyAccumulate = SVG_ApplyAccumulateRotate;
			break;
		case SVG_TRANSFORM_SKEWX:
			stack->Set = SVG_SetSkewX;
			stack->Interpolate = SVG_InterpolateSkewX;
			stack->ApplyAccumulate = SVG_ApplyAccumulateSkewX;
			break;
		case SVG_TRANSFORM_SKEWY:
			stack->Set = SVG_SetSkewY;
			stack->Interpolate = SVG_InterpolateSkewY;
			stack->ApplyAccumulate = SVG_ApplyAccumulateSkewY;
			break;
		default:
			/*TODO*/
			break;
		}
		break;
	/* The following are keyword animations */
	case SVG_TextAnchorValue_datatype:
	case SVG_FontStyleValue_datatype:
	case SVG_ClipFillRule_datatype:
	case SVG_StrokeLineCapValue_datatype:
	case SVG_StrokeLineJoinValue_datatype:
	case SVG_DisplayValue_datatype:
	case SVG_VisibilityValue_datatype:
		stack->InitStackValues = SVG_InitStackValuesKeyword;
		stack->DeleteStackValues = SVG_DeleteStackValuesKeyword;
		stack->Set = SVG_SetKeyword;
		stack->Assign = SVG_SetKeyword;
		stack->Interpolate = SVG_InterpolateKeyword;
		stack->ApplyAdditive = NULL;
		stack->ApplyAccumulate = NULL;
		stack->Invalidate = SVG_InvalidateKeyword;
		break;
	case SVG_Motion_datatype:
		stack->InitStackValues = SVG_InitStackValuesTransform;
		stack->DeleteStackValues = SVG_DeleteStackValuesTransform;
		stack->Set = SVG_SetMotion;
		stack->Assign = SVG_AssignTransform;
		stack->Interpolate = SVG_InterpolateMotion;
		stack->ApplyAdditive = SVG_ApplyAdditiveTransform;
		stack->ApplyAccumulate = SVG_ApplyAccumulateMotion;
		stack->Invalidate = SVG_InvalidateMotion;
		break;
	case SVG_StrokeDashOffsetValue_datatype:
	case SVG_OpacityValue_datatype:
	case SVG_FontSizeValue_datatype:
	case SVG_StrokeMiterLimitValue_datatype:
		stack->InitStackValues = SVG_InitStackValuesIFloat;
		stack->DeleteStackValues = SVG_DeleteStackValuesIFloat;
		stack->Set = SVG_SetIFloat;
		stack->Assign = SVG_SetIFloat;
		stack->Interpolate = SVG_InterpolateIFloat;
		stack->ApplyAdditive = SVG_AddIFloat;
		stack->ApplyAccumulate = SVG_ApplyAccumulateIFloat;
		stack->Invalidate = SVG_InvalidateIFloat;
		break;
	case SVG_StrokeDashArrayValue_datatype:
		stack->InitStackValues = SVG_InitStackValuesDashArray;
		stack->DeleteStackValues = SVG_DeleteStackValuesDashArray;
		stack->Set = SVG_SetDashArray;
		stack->Assign = SVG_SetDashArray;
		stack->Interpolate = SVG_InterpolateDashArray;
		stack->ApplyAdditive = SVG_AddDashArray;
		stack->ApplyAccumulate = SVG_ApplyAccumulateDashArray;
		stack->Invalidate = SVG_InvalidateDashArray;
		break;
	case SVG_ViewBoxSpec_datatype:
		stack->InitStackValues = SVG_InitStackValuesRect;
		stack->DeleteStackValues = SVG_DeleteStackValuesRect;
		stack->Set = SVG_SetRect;
		stack->Assign = SVG_SetRect;
		stack->Interpolate = SVG_InterpolateRect;
		stack->ApplyAdditive = SVG_AddRect;
		stack->ApplyAccumulate = SVG_ApplyAccumulateRect;
		stack->Invalidate = SVG_InvalidateRect;
		break;
	case SVG_IRI_datatype:
		stack->InitStackValues = SVG_InitStackValuesIRI;
		stack->DeleteStackValues = SVG_DeleteStackValuesIRI;
		stack->Set = SVG_SetIRI;
		stack->Assign = SVG_SetIRI;
		stack->Interpolate = SVG_InterpolateIRI;
		stack->ApplyAdditive = SVG_AddIRI;
		stack->ApplyAccumulate = SVG_ApplyAccumulateIRI;
		stack->Invalidate = SVG_InvalidateIRI;
		break;
	case SVG_Coordinates_datatype:
		stack->InitStackValues = SVG_InitStackValuesCoords;
		stack->DeleteStackValues = SVG_DeleteStackValuesCoords;
		stack->Set = SVG_SetCoords;
		stack->Assign = SVG_SetCoords;
		stack->Interpolate = SVG_InterpolateCoords;
		stack->ApplyAdditive = SVG_AddCoords;
		stack->ApplyAccumulate = SVG_ApplyAccumulateCoords;
		stack->Invalidate = SVG_InvalidateCoords;
		break;
	case SVG_Points_datatype:
		stack->InitStackValues = SVG_InitStackValuesPoints;
		stack->DeleteStackValues = SVG_DeleteStackValuesPoints;
		stack->Set = SVG_SetPoints;
		stack->Assign = SVG_SetPoints;
		stack->Interpolate = SVG_InterpolatePoints;
		stack->ApplyAdditive = SVG_AddPoints;
		stack->ApplyAccumulate = SVG_ApplyAccumulatePoints;
		stack->Invalidate = SVG_InvalidatePoints;
		break;
	case SVG_String_datatype:
		stack->InitStackValues = SVG_InitStackValuesString;
		stack->DeleteStackValues = SVG_DeleteStackValuesString;
		stack->Set = SVG_SetString;
		stack->Assign = SVG_SetString;
		stack->Interpolate = SVG_InterpolateString;
		stack->ApplyAdditive = SVG_AddString;
		stack->ApplyAccumulate = SVG_ApplyAccumulateString;
		stack->Invalidate = SVG_InvalidateString;
		break;
	case SVG_FontFamilyValue_datatype:
		stack->InitStackValues = SVG_InitStackValuesFontFamily;
		stack->DeleteStackValues = SVG_DeleteStackValuesFontFamily;
		stack->Set = SVG_SetFontFamily;
		stack->Assign = SVG_SetFontFamily;
		stack->Interpolate = SVG_InterpolateFontFamily;
		stack->ApplyAdditive = SVG_AddFontFamily;
		stack->ApplyAccumulate = SVG_ApplyAccumulateFontFamily;
		stack->Invalidate = SVG_InvalidateFontFamily;
		break;
	case SVG_PathData_datatype:
		stack->InitStackValues = SVG_InitStackValuesPath;
		stack->DeleteStackValues = SVG_DeleteStackValuesPath;
		stack->Set = SVG_SetPath;
		stack->Assign = SVG_SetPath;
		stack->Interpolate = SVG_InterpolatePath;
		stack->ApplyAdditive = SVG_AddPath;
		stack->ApplyAccumulate = SVG_ApplyAccumulatePath;
		stack->Invalidate = SVG_InvalidatePath;
		break;
	default:
		fprintf(stderr, "Animation type not supported %d\n", stack->targetAttributeType);
	}
	stack->DeleteStack = SVG_DeleteStack;
}



/* Initialisation functions for all animation elements in SVG */

void SVG_Init_set(Render2D *sr, GF_Node *node)
{
	SVGsetElement *set = (SVGsetElement *)node;
	SMIL_AnimationStack *stack = SMIL_Init_AnimationStack(sr, node);
	
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

	SVG_Init_SMILAnimationStackAPI(stack);
	SMIL_InitAnimateFunction(stack);

}

void SVG_Init_animate(Render2D *sr, GF_Node *node)
{
	SVGanimateElement *animate = (SVGanimateElement *)node;
	SMIL_AnimationStack *stack = SMIL_Init_AnimationStack(sr, node);
	
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

	SVG_Init_SMILAnimationStackAPI(stack);
	SMIL_InitAnimateFunction(stack);

}

void SVG_Init_animateColor(Render2D *sr, GF_Node *node)
{
	SVGanimateColorElement *ac = (SVGanimateColorElement *)node;
	SMIL_AnimationStack *stack = SMIL_Init_AnimationStack(sr, node);
	
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

	SVG_Init_SMILAnimationStackAPI(stack);
	SMIL_InitAnimateFunction(stack);

}

void SVG_Init_animateTransform(Render2D *sr, GF_Node *node)
{
	GF_FieldInfo info;
	SVGanimateTransformElement *at = (SVGanimateTransformElement *)node;
	SMIL_AnimationStack *stack = SMIL_Init_AnimationStack(sr, node);
	
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

	SVG_Init_SMILAnimationStackAPI(stack);
	SMIL_InitAnimateFunction(stack);

}

void SVG_Init_animateMotion(Render2D *sr, GF_Node *node)
{
	GF_FieldInfo info;
	SVGanimateMotionElement *am = (SVGanimateMotionElement *)node;
	SMIL_AnimationStack *stack = SMIL_Init_AnimationStack(sr, node);
	
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
	stack->keyPoints = &(am->keyPoints);
	stack->keySplines = &(am->keySplines);
	stack->values = &(am->values);
	stack->from = &(am->from);
	stack->to = &(am->to); 
	stack->by = &(am->by); 
	stack->calcMode = &(am->calcMode); 
	stack->additive = &(am->additive); 
	stack->accumulate = &(am->accumulate); 

	if (!stack->to->datatype && !stack->by->datatype && !stack->values->datatype) stack->path = &(am->path);

	SVG_Init_SMILAnimationStackAPI(stack);
	SMIL_InitAnimateFunction(stack);

}

void SVG_Init_discard(Render2D *sr, GF_Node *node)
{
	SVGdiscardElement *d = (SVGdiscardElement *)node;
	SMIL_AnimationStack *stack = SMIL_Init_AnimationStack(sr, node);
	
	stack->target_element = (GF_Node*)d->xlink_href.target_element;

	stack->begins = &(d->begin); 
	/* 'from', 'to', 'by', 'values', 'keyTimes', 'keySplines', ... are NULL */

	SVG_Init_SMILAnimationStackAPI(stack);
	SMIL_InitAnimateFunction(stack);
}

#endif

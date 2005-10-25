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

static void SVG_InvalidateAndDirtyAppearance(SMIL_AnimationStack *stack)
{
	//fprintf(stdout, "Invalidating Appearance\n");
	gf_node_dirty_set(stack->target_element, GF_SG_SVG_APPEARANCE_DIRTY, 0);
	gf_sr_invalidate(stack->compositor, NULL);
}

static void SVG_InvalidateAndDirtyGeometry(SMIL_AnimationStack *stack)
{
	//fprintf(stdout, "Invalidating Geometry\n");
	gf_node_dirty_set(stack->target_element, GF_SG_SVG_GEOMETRY_DIRTY, 0);
	gf_sr_invalidate(stack->compositor, NULL);
}

static void SVG_InvalidateAndDirtyAll(SMIL_AnimationStack *stack)
{
	//fprintf(stdout, "Invalidating Appearance and Geometry\n");
	/* TODO: determine if appaearance or geometry have been modified */
	gf_node_dirty_set(stack->target_element, GF_SG_SVG_GEOMETRY_DIRTY | GF_SG_SVG_APPEARANCE_DIRTY, 0);
	gf_sr_invalidate(stack->compositor, NULL);
}

static void SVG_InvalidateNodeOnly(SMIL_AnimationStack *stack)
{
	//fprintf(stdout, "Invalidating Node %8x\n", stack);
	gf_sr_invalidate(stack->compositor, NULL);
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
static void SVG_AddColor(SMIL_AnimationStack *stack, SVG_Color *a, SVG_Color *b, SVG_Color *c)
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
static void SVG_SetColor(SMIL_AnimationStack *stack, SVG_Color *a, SVG_Color *b)
{
	a->red   = b->red;
	a->green = b->green;
	a->blue  = b->blue;
}

static u32 SVG_CompareColor(SMIL_AnimationStack *stack, SVG_Color *a, SVG_Color *b)
{
	if (a->type != b->type) return 1;
	if (a->red != b->red) return 1;
	if (a->green != b->green) return 1;
	if (a->blue != b->blue) return 1;
	return 0;
}

static void SVG_ApplyAccumulateColor(SMIL_AnimationStack *stack, 
									 u32 nb_iterations, 
									 SVG_Color *current,
									 SVG_Color *last,
									 SVG_Color *accumulated)
{
	SVG_Color tmp;
	SVG_MulColor(last, INT2FIX(nb_iterations), &tmp);
	SVG_AddColor(stack, current, &tmp, &tmp);
	SVG_ClampColor(&tmp);
	SVG_SetColor(stack, accumulated, &tmp);
}

static void SVG_InterpolateColor(SMIL_AnimationStack *stack, Fixed interpolation_coefficient, SVG_Color *value1, SVG_Color *value2, SVG_Color *target)
{
	target->type = value1->type;
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
static void SVG_AddPaint(SMIL_AnimationStack *stack, SVG_Paint *a, SVG_Paint *b, SVG_Paint *c)
{
	SVG_AddColor(stack, a->color, b->color, c->color);
	/* TODO: fix me */
	c->type = a->type;
}

/* c = k*a */
static void SVG_MulPaint(SVG_Paint *a, Fixed k, SVG_Paint *c)
{
	SVG_MulColor(a->color, k, c->color);
	c->type = a->type;
}

/* a = b */
static void SVG_SetPaint(SMIL_AnimationStack *stack, SVG_Paint *a, SVG_Paint *b)
{
	SVG_SetColor(stack, a->color, b->color);
	a->type = b->type;
}

static u32 SVG_ComparePaint(SMIL_AnimationStack *stack, SVG_Paint *a, SVG_Paint *b)
{
	if (a->type != b->type) return 1;
	if ((!a->uri && b->uri) || (a->uri && !b->uri)) return 1;
	if (a->uri && strcmp(a->uri, b->uri)) return 1;
	return SVG_CompareColor(stack, a->color, b->color);
}

static void SVG_ApplyAccumulatePaint(SMIL_AnimationStack *stack, 
									 u32 nb_iterations, 
									 SVG_Paint *current,
									 SVG_Paint *last,
									 SVG_Paint *accumulated)
{
	SVG_ApplyAccumulateColor(stack, nb_iterations, current->color, last->color, accumulated->color);
	/* TODO: fix me */
	accumulated->type = current->type;
}

static void SVG_InterpolatePaint(SMIL_AnimationStack *stack, Fixed interpolation_coefficient, SVG_Paint *value1, SVG_Paint *value2, SVG_Paint *target)
{
	SVG_InterpolateColor(stack, interpolation_coefficient, value1->color, value2->color, target->color);
	/* TODO: fix me */
	target->type = value1->type;
}

static void SVG_InitStackValuesPaint(SMIL_AnimationStack *stack)
{
	GF_SAFEALLOC(stack->base_value, sizeof(SVG_Paint))
	GF_SAFEALLOC(((SVG_Paint *)stack->base_value)->color, sizeof(SVG_Color))
	((SVG_Paint *)stack->base_value)->type = ((SVG_Paint *)stack->targetAttribute)->type;
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
static void SVG_ApplyAccumulateMotion(SMIL_AnimationStack *stack, u32 nb_iterations, SVG_Matrix *current, SVG_Point *last, SVG_Matrix *accumulated)
{
	accumulated->m[2] = current->m[2] + nb_iterations*last->x;
	accumulated->m[5] = current->m[5] + nb_iterations*last->y;
}

static void SVG_InterpolateMotion(SMIL_AnimationStack *stack, Fixed interpolation_coefficient, SVG_Point *value1, SVG_Point *value2, SVG_Matrix *target)
{
	target->m[2] = SVG_Interpolate(value1->x, value2->x, interpolation_coefficient);
	target->m[5] = SVG_Interpolate(value1->y, value2->y, interpolation_coefficient);
}

static void SVG_SetMotion(SMIL_AnimationStack *stack, SVG_Matrix *a, SVG_Point *b)
{
	a->m[2] = b->x;
	a->m[5] = b->y;
}

/* SVG_AssignMotion is equal to SVG_SetTransform*/
/* SVG_InitStackValuesMotion is equal to SVG_InitStackValuesTransform */
/* SVG_DeleteStackValuesMotion is equal to SVG_DeleteStackValuesTransform */
/* SVG_CompareMotion is equal to SVG_CompareTransform */
/* Motion functions end */

/* SVG_Length functions */
static void SVG_SetLength(SMIL_AnimationStack *stack, SVG_Length *a, SVG_Length *b)
{
	a->number = b->number;
}

static void SVG_MulLength(SVG_Length *a, Fixed k, SVG_Length *c)
{
	c->number = a->number * k;
}

static void SVG_AddLength(SMIL_AnimationStack *stack, SVG_Length *a, SVG_Length *b, SVG_Length *c) 
{
	c->number = a->number + b->number;
}

static u32 SVG_CompareLength(SMIL_AnimationStack *stack, SVG_Length *a, SVG_Length *b)
{
	if (a->type != b->type) return 1;
	return (a->number != b->number);
}

static void SVG_ApplyAccumulateLength(SMIL_AnimationStack *stack, u32 nb_iterations, SVG_Length *current, SVG_Length *last, SVG_Length *accumulated)
{
	SVG_Length tmp;
	SVG_MulLength(last, INT2FIX(nb_iterations), &tmp);
	SVG_AddLength(stack, current, &tmp, accumulated);
}

static void SVG_InterpolateLength(SMIL_AnimationStack *stack, Fixed interpolation_coefficient, SVG_Length *value1, SVG_Length *value2, SVG_Length *target)
{
	target->type = value1->type;
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

/* end of Length functions */

/* Inheritable float functions */
static void SVG_SetIFloat(SMIL_AnimationStack *stack, SVGInheritableFloat *a, SVGInheritableFloat *b)
{
	a->type = b->type;
	a->value = b->value;
}

static void SVG_MulIFloat(SVGInheritableFloat *a, Fixed k, SVGInheritableFloat *c)
{
	c->type = a->type;
	c->value = a->value * k;
}

static void SVG_AddIFloat(SMIL_AnimationStack *stack, SVGInheritableFloat *a, SVGInheritableFloat *b, SVGInheritableFloat *c)
{
	c->type = a->type;
	c->value = a->value + b->value;
}

static u32 SVG_CompareIFloat(SMIL_AnimationStack *stack, SVGInheritableFloat *a, SVGInheritableFloat *b)
{
	if(a->type != b->type) return 1;
	return (a->value != b->value);
}

static void SVG_ApplyAccumulateIFloat(SMIL_AnimationStack *stack, u32 nb_iterations, SVGInheritableFloat *current, SVGInheritableFloat *last, SVGInheritableFloat *accumulated)
{
	SVGInheritableFloat tmp;
	SVG_MulIFloat(last, INT2FIX(nb_iterations), &tmp);
	SVG_AddIFloat(stack, current, &tmp, accumulated);
}

static void SVG_InterpolateIFloat(SMIL_AnimationStack *stack, Fixed interpolation_coefficient, SVGInheritableFloat *value1, SVGInheritableFloat *value2, SVGInheritableFloat *target)
{
	target->type = value1->type;
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
/* end of IFloat functions */

/* Transform functions */
static void printMatrix(const char *s, SVG_Matrix *target)
{
	//fprintf(stdout, "%s %.2f,%.2f,%.2f,%.2f,%.2f,%.2f\n", s, target->m[0], target->m[1], target->m[2], target->m[3], target->m[4], target->m[5]);
}

static void SVG_SetTranslation(SMIL_AnimationStack *stack, SVG_Matrix *a, SVG_Point *b)
{
	gf_mx2d_init(*a);
	gf_mx2d_add_translation(a, b->x, b->y);
//	printMatrix("\nSet Translation", a);
}

static void SVG_ApplyAccumulateTranslation(SMIL_AnimationStack *stack, u32 nb_iterations, SVG_Matrix *current, SVG_Point *last, SVG_Matrix *accumulated)
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

static void SVG_InterpolateTranslation(SMIL_AnimationStack *stack, Fixed interpolation_coefficient, SVG_Point *value1, SVG_Point *value2, SVG_Matrix *target)
{
	gf_mx2d_init(*target);
	target->m[2] = SVG_Interpolate(value1->x, value2->x, interpolation_coefficient); 
	target->m[5] = SVG_Interpolate(value1->y, value2->y, interpolation_coefficient); 
//	printMatrix("\nInterpolate Translation", target);
}

static void SVG_SetScale(SMIL_AnimationStack *stack, SVG_Matrix *a, SVG_Point *b)
{
	gf_mx2d_init(*a);
	gf_mx2d_add_scale(a, b->x, b->y);
//	printMatrix("\nSet Scale\n", a);
}

static void SVG_ApplyAccumulateScale(SMIL_AnimationStack *stack, u32 nb_iterations, SVG_Matrix *current, SVG_Point *last, SVG_Matrix *accumulated)
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

static void SVG_InterpolateScale(SMIL_AnimationStack *stack, Fixed interpolation_coefficient, SVG_Point *value1, SVG_Point *value2, SVG_Matrix *target)
{
	gf_mx2d_init(*target);
	target->m[0] = SVG_Interpolate(value1->x, value2->x, interpolation_coefficient); 
	target->m[4] = SVG_Interpolate(value1->y, value2->y, interpolation_coefficient); 
//	printMatrix("\nInterpolate Scale\n", target);
}

static void SVG_SetRotate(SMIL_AnimationStack *stack, SVG_Matrix *a, SVG_Point_Angle *b)
{
	gf_mx2d_init(*a);
	gf_mx2d_add_rotation(a, b->x, b->y, b->angle);
//	printMatrix("\nSet Rotate\n", a);
}

static void SVG_ApplyAccumulateRotate(SMIL_AnimationStack *stack, u32 nb_iterations, SVG_Matrix *current, SVG_Point_Angle *last, SVG_Matrix *accumulated)
{
	SVG_Matrix tmp;
	gf_mx2d_init(tmp);
	gf_mx2d_add_rotation(&tmp, nb_iterations*last->x, nb_iterations*last->y, nb_iterations*last->angle);
	gf_mx2d_add_matrix(&tmp, current);
	gf_mx2d_add_matrix(&tmp, accumulated);
	gf_mx2d_copy(*accumulated, tmp);
}

static void SVG_InterpolateRotate(SMIL_AnimationStack *stack, Fixed interpolation_coefficient, SVG_Point_Angle *value1, SVG_Point_Angle *value2, SVG_Matrix *target)
{
	gf_mx2d_init(*target);
	gf_mx2d_add_rotation(target, 
						 SVG_Interpolate(value1->x, value2->x, interpolation_coefficient),
						 SVG_Interpolate(value1->y, value2->y, interpolation_coefficient),
						 SVG_Interpolate(value1->angle, value2->angle, interpolation_coefficient));
//	printMatrix("\nInterpolate Rotate", target);
}

static void SVG_SetSkewX(SMIL_AnimationStack *stack, SVG_Matrix *a, Fixed *b)
{
	gf_mx2d_init(*a);
	gf_mx2d_add_skew_x(a, *b);
//	printMatrix("\nSet Skew X\n", a);
}

static void SVG_ApplyAccumulateSkewX(SMIL_AnimationStack *stack, u32 nb_iterations, SVG_Matrix *current, Fixed *last, SVG_Matrix *accumulated)
{
	SVG_Matrix tmp;
	gf_mx2d_init(tmp);
	gf_mx2d_add_skew_x(&tmp, nb_iterations*(*last));
	gf_mx2d_add_matrix(&tmp, current);
	gf_mx2d_add_matrix(&tmp, accumulated);
	gf_mx2d_copy(*accumulated, tmp);
//	printMatrix("\nAccumulate Skew X\n", a);
}

static void SVG_InterpolateSkewX(SMIL_AnimationStack *stack, Fixed interpolation_coefficient, Fixed *value1, Fixed *value2, SVG_Matrix *target)
{
	gf_mx2d_init(*target);
	gf_mx2d_add_skew_x(target, 
					   SVG_Interpolate(*value1, *value2, interpolation_coefficient));
//	printMatrix("\nInterpolate Skew X\n", a);
}

static void SVG_SetSkewY(SMIL_AnimationStack *stack, SVG_Matrix *a, Fixed *b)
{
	gf_mx2d_init(*a);
	gf_mx2d_add_skew_y(a, *b);
//	printMatrix("\nSet Skew Y\n", a);
}

static void SVG_ApplyAccumulateSkewY(SMIL_AnimationStack *stack, u32 nb_iterations, SVG_Matrix *current, Fixed *last, SVG_Matrix *accumulated)
{
	SVG_Matrix tmp;
	gf_mx2d_init(tmp);
	gf_mx2d_add_skew_y(&tmp, nb_iterations*(*last));
	gf_mx2d_add_matrix(&tmp, current);
	gf_mx2d_add_matrix(&tmp, accumulated);
	gf_mx2d_copy(*accumulated, tmp);
//	printMatrix("\nAccumulate Skew Y\n", a);
}

static void SVG_InterpolateSkewY(SMIL_AnimationStack *stack, Fixed interpolation_coefficient, Fixed *value1, Fixed *value2, SVG_Matrix *target)
{
	gf_mx2d_init(*target);
	gf_mx2d_add_skew_y(target, 
					   SVG_Interpolate(*value1, *value2, interpolation_coefficient));
//	printMatrix("\nInterpolate Skew Y\n", a);
}

static void SVG_InterpolateTransform(SMIL_AnimationStack *stack, Fixed interpolation_coefficient, SVG_Matrix *value1, SVG_Matrix *value2, SVG_Matrix *target)
{
	target->m[0] = SVG_Interpolate(value1->m[0], value2->m[0], interpolation_coefficient);
	target->m[1] = SVG_Interpolate(value1->m[1], value2->m[1], interpolation_coefficient);
	target->m[2] = SVG_Interpolate(value1->m[2], value2->m[2], interpolation_coefficient);
	target->m[3] = SVG_Interpolate(value1->m[3], value2->m[3], interpolation_coefficient);
	target->m[4] = SVG_Interpolate(value1->m[4], value2->m[4], interpolation_coefficient);
	target->m[5] = SVG_Interpolate(value1->m[5], value2->m[5], interpolation_coefficient);
//	printMatrix("\nInterpolate Transform", a);
}

static void SVG_ApplyAccumulateTransform(SMIL_AnimationStack *stack, u32 nb_iterations, SVG_Matrix *current, SVG_Matrix *last, SVG_Matrix *accumulated)
{
	fprintf(stdout, "Warning: accumulation of matrix is not implemented\n");
//	printMatrix("\nAccumulate Transform", a);
}

static void SVG_ApplyAdditiveTransform(SMIL_AnimationStack *stack, SVG_Matrix *underlying_value, SVG_Matrix *toApply, SVG_Matrix *target)
{
	SVG_Matrix tmp;
//	printMatrix("\nAdditive Transform \nunderlying value", underlying_value);
//	printMatrix("to add", toApply);
	gf_mx2d_copy(tmp, *toApply);
	gf_mx2d_add_matrix(&tmp, underlying_value);
	gf_mx2d_copy(*target, tmp);
//	printMatrix("result", target);
}

static void SVG_SetTransform(SMIL_AnimationStack *stack, SVG_Matrix *a, SVG_Matrix *b)
{
	gf_mx2d_copy(*a, *b);
//	printMatrix("\nAssign", a);
}

static u32 SVG_CompareTransform(SMIL_AnimationStack *stack, SVG_Matrix *a, SVG_Matrix *b)
{
	if (a->m[0] != b->m[0]) return 1;
	if (a->m[1] != b->m[1]) return 1;
	if (a->m[2] != b->m[2]) return 1;
	if (a->m[3] != b->m[3]) return 1;
	if (a->m[4] != b->m[4]) return 1;
	if (a->m[5] != b->m[5]) return 1;
	return 0;
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

/* end of Transform functions */

/* keyword functions */
static void SVG_SetKeyword(SMIL_AnimationStack *stack, u8 *a, u8 *b)
{
	*a = *b;
}

static u32 SVG_CompareKeyword(SMIL_AnimationStack *stack, u8 *a, u8 *b)
{
	return (*a != *b);
}

static void SVG_ApplyAdditiveKeyword(SMIL_AnimationStack *stack, u8 *a, u8* b, u8 *c)
{
	/* Nothing to be done */
}
static void SVG_ApplyAccumulateKeyword(SMIL_AnimationStack *stack, u32 nb_iterations, u8 *current, u8 *last, u8 *accumulated)
{
	/* Nothing to be done */
}

static void SVG_InterpolateKeyword(SMIL_AnimationStack *stack, Fixed interpolation_coefficient, u8 *value1, u8 *value2, u8 *target) 
{
	if (interpolation_coefficient < INT2FIX(0.5)) 
		*target = *value1;
	else 
		*target = *value2;
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
static void SVG_SetDashArray(SMIL_AnimationStack *stack, SVG_StrokeDashArray *a, SVG_StrokeDashArray *b)
{
	a->type = b->type;
	if (a->array.vals) free(a->array.vals);
	a->array.count = b->array.count;
	a->array.vals = malloc(sizeof(Fixed)*a->array.count);
	memcpy(a->array.vals, b->array.vals, sizeof(Fixed)*a->array.count);
}

static void SVG_MulDashArray(SVG_StrokeDashArray *a, Fixed k, SVG_StrokeDashArray *c)
{
	/* TODO */
}

static void SVG_AddDashArray(SMIL_AnimationStack *stack, SVG_StrokeDashArray *a, SVG_StrokeDashArray *b, SVG_StrokeDashArray *c)
{
	/* TODO */
}

static void SVG_ApplyAccumulateDashArray(SMIL_AnimationStack *stack, u32 nb_iterations, 
										 SVG_StrokeDashArray *current, 
										 SVG_StrokeDashArray *last, 
										 SVG_StrokeDashArray *accumulated)
{
	/* TODO */
}

static void SVG_InterpolateDashArray(SMIL_AnimationStack *stack, Fixed interpolation_coefficient, 
									 SVG_StrokeDashArray *value1, 
									 SVG_StrokeDashArray *value2, 
									 SVG_StrokeDashArray *target)
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

static u32 SVG_CompareDashArray(SMIL_AnimationStack *stack, SVG_StrokeDashArray *a, 
								SVG_StrokeDashArray *b)
{
	if (a->type != b->type) return 1;
	if (a->array.count != b->array.count) return 1;
	return memcmp(a->array.vals, b->array.vals, sizeof(Fixed)*a->array.count);
}

static void SVG_InitStackValuesDashArray(SMIL_AnimationStack *stack)
{
	GF_SAFEALLOC(stack->base_value, sizeof(SVG_StrokeDashArray))
	GF_SAFEALLOC(stack->tmp_value, sizeof(SVG_StrokeDashArray))
}

static void SVG_DeleteStackValuesDashArray(SMIL_AnimationStack *stack)
{
	if (((SVG_StrokeDashArray *)stack->base_value)->array.vals) 
		free(((SVG_StrokeDashArray *)stack->base_value)->array.vals);
	free(stack->base_value);
	if (((SVG_StrokeDashArray *)stack->tmp_value)->array.vals) 
		free(((SVG_StrokeDashArray *)stack->tmp_value)->array.vals);
	free(stack->tmp_value);
}

/* end of Dash Array animation functions */

/* SVG Rect animation functions */
static void SVG_SetRect(SMIL_AnimationStack *stack, SVG_Rect *a, SVG_Rect *b)
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

static void SVG_AddRect(SMIL_AnimationStack *stack, SVG_Rect *a, SVG_Rect *b, SVG_Rect *c)
{
	c->x = a->x + b->x;
	c->y = a->y + b->y;
	c->width = a->width + b->width;
	c->height= a->height + b->height;
}

static u32 SVG_CompareRect(SMIL_AnimationStack *stack, SVG_Rect *a, SVG_Rect *b)
{
	if (a->x != b->x) return 1;
	if (a->y != b->y) return 1;
	if (a->height != b->height) return 1;
	if (a->width != b->width) return 1;
	return 0;
}

static void SVG_ApplyAccumulateRect(SMIL_AnimationStack *stack, u32 nb_iterations, SVG_Rect *current, SVG_Rect *last, SVG_Rect*accumulated)
{
	accumulated->x = current->x + nb_iterations*last->x;
	accumulated->y = current->y + nb_iterations*last->y;
	accumulated->width = current->width + nb_iterations*last->width;
	accumulated->height= current->height + nb_iterations*last->height;
}

static void SVG_InterpolateRect(SMIL_AnimationStack *stack, Fixed interpolation_coefficient, SVG_Rect *value1, SVG_Rect *value2, SVG_Rect *target)
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
/* end of SVG Rect animation functions */

/* SVG IRI animation functions */
static void SVG_SetIRI(SMIL_AnimationStack *stack, SVG_IRI *a, SVG_IRI *b)
{
	*a = *b;
}

static void SVG_AssignIRI(SMIL_AnimationStack *stack, SVG_IRI *a, SVG_IRI *b)
{
	a->type = b->type;
	if (a->iri) free(a->iri);
	if (b->iri) a->iri = strdup(b->iri);
	if (!a->iri_owner) a->iri_owner = (SVGElement *)stack->target_element;
	//if (a->target) gf_node_unregister((GF_Node *)a->target, (GF_Node *)stack->target_element);
	a->target = b->target;
	// Owner does not change but the iri may not have been parsed so the owner is not set
	//if (a->target) gf_node_register((GF_Node *)a->target, (GF_Node *)a->iri_owner);
}

static void SVG_MulIRI(SVG_IRI *a, Fixed k, SVG_IRI *c)
{
}

static void SVG_AddIRI(SMIL_AnimationStack *stack, SVG_IRI *a, SVG_Rect *b, SVG_IRI *c)
{
}

static u32 SVG_CompareIRI(SMIL_AnimationStack *stack, SVG_IRI *a, SVG_IRI *b)
{
	if (a->type != b->type) return 1;
	if (a->target || b->target) {
		if (a->target != b->target) return 1;
		return 0;
	}
	if (a->iri || b->iri) {
		return (a->iri != b->iri);
	}
	if (!a->iri && !b->iri) return 0;
	return strcmp(a->iri, b->iri);
}

static void SVG_ApplyAccumulateIRI(SMIL_AnimationStack *stack, u32 nb_iterations, SVG_IRI *current, SVG_IRI *last, SVG_IRI*accumulated)
{
}

static void SVG_InterpolateIRI(SMIL_AnimationStack *stack, Fixed interpolation_coefficient, SVG_IRI *value1, SVG_IRI *value2, SVG_IRI *target)
{
	if (interpolation_coefficient < INT2FIX(0.5)) 
		SVG_SetIRI(stack, target,value1);
	else 
		SVG_SetIRI(stack, target,value2);
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
/* end of SVG IRI animation functions */

/* SVG Points functions */
static void SVG_SetPoints(SMIL_AnimationStack *stack, SVG_Points *a, SVG_Points *b)
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

static void SVG_AddPoints(SMIL_AnimationStack *stack, SVG_Points *a, SVG_Points *b, SVG_Points *c)
{
}

static u32 SVG_ComparePoints(SMIL_AnimationStack *stack, SVG_Points *a, SVG_Points *b)
{
	u32 i, count;

	count= gf_list_count(*a);
	if (count != gf_list_count(*b)) return 1;

	for (i = 0; i < count; i++) {
		SVG_Point *pta = gf_list_get(*a, i);
		SVG_Point *ptb = gf_list_get(*b, i);
		if ((pta->x != ptb->x) || (pta->y != ptb->y)) return 1; 
	}
	return 0;
}

static void SVG_ApplyAccumulatePoints(SMIL_AnimationStack *stack, u32 nb_iterations, SVG_Points *current, SVG_Points *last, SVG_Points *accumulated)
{
}

static void SVG_InterpolatePoints(SMIL_AnimationStack *stack, Fixed interpolation_coefficient, SVG_Points *value1, SVG_Points *value2, SVG_Points *target)
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

/* end of SVG Coordinates animation functions */

/* SVG Coordinates functions */
static void SVG_SetCoords(SMIL_AnimationStack *stack, SVG_Coordinates *a, SVG_Coordinates *b)
{
	u32 i, count;

	for (i = 0; i < gf_list_count(*a); i++) {
		SVG_Coordinate *c = gf_list_get(*a, i);
		free(c);
	}
	gf_list_reset(*a);
	
	count = gf_list_count(*b);
	for (i = 0; i < count; i ++) {
		SVG_Coordinate *cb = gf_list_get(*b, i);
		SVG_Coordinate *ca;
		GF_SAFEALLOC(ca, sizeof(SVG_Point))
		*ca = *cb;
		gf_list_add(*a, ca);
	}
}

static void SVG_AddCoords(SMIL_AnimationStack *stack, SVG_Coordinates *a, SVG_Coordinates *b, SVG_Coordinates *c)
{
}

static u32 SVG_CompareCoords(SMIL_AnimationStack *stack, SVG_Coordinates *a, SVG_Coordinates *b)
{
	u32 i, count;

	count= gf_list_count(*a);
	if (count != gf_list_count(*b)) return 1;

	for (i = 0; i < count; i++) {
		SVG_Coordinate *ca = gf_list_get(*a, i);
		SVG_Coordinate *cb = gf_list_get(*b, i);
		if ((ca->number != cb->number) || (ca->type != cb->type)) return 1; 
	}
	return 0;
}

static void SVG_ApplyAccumulateCoords(SMIL_AnimationStack *stack, u32 nb_iterations, SVG_Coordinates *current, SVG_Coordinates  *last, SVG_Coordinates *accumulated)
{
}

static void SVG_InterpolateCoords(SMIL_AnimationStack *stack, Fixed interpolation_coefficient, SVG_Coordinates *value1, SVG_Coordinates *value2, SVG_Coordinates *target)
{
	u32 i, count;

	for (i = 0; i < gf_list_count(*target); i++) {
		SVG_Coordinate *c = gf_list_get(*target, i);
		free(c);
	}
	gf_list_reset(*target);
	
	count = gf_list_count(*value1);
	for (i = 0; i < count; i ++) {
		SVG_Coordinate *c3;
		SVG_Coordinate *c1 = gf_list_get(*value1, i);
		SVG_Coordinate *c2 = gf_list_get(*value2, i);
		GF_SAFEALLOC(c3, sizeof(SVG_Coordinate))
		c3->type = c1->type;
		c3->number = SVG_Interpolate(c1->number, c2->number, interpolation_coefficient);
		gf_list_add(*target, c3);
	}
}

static void SVG_InitStackValuesCoords(SMIL_AnimationStack *stack)
{
	u32 i, count;
	SVG_Coordinate *c, *nc;
	GF_SAFEALLOC(stack->base_value, sizeof(SVG_Coordinates));
	*(SVG_Coordinates*)stack->base_value = gf_list_new();
	count = gf_list_count(*(SVG_Coordinates*)stack->targetAttribute);
	for (i = 0; i < count; i ++) {
		c = gf_list_get(*(SVG_Coordinates *)stack->targetAttribute, i);
		nc = malloc(sizeof(SVG_Coordinate));
		*nc = *c;
		gf_list_add(*(SVG_Coordinates *)stack->base_value, nc);
	}
	GF_SAFEALLOC(stack->tmp_value, sizeof(SVG_Coordinates));
	*(SVG_Coordinates*)stack->tmp_value = gf_list_new();
}

static void SVG_DeleteStackValuesCoords(SMIL_AnimationStack *stack)
{
	SVG_DeleteCoordinates(*(SVG_Coordinates *)stack->base_value);
	SVG_DeleteCoordinates(*(SVG_Coordinates *)stack->tmp_value);
}
/* end of SVG Coordinates animation functions */

/* SVG String functions */
static void SVG_SetString(SMIL_AnimationStack *stack, SVG_String *a, SVG_String *b)
{
	if (*a) free(*a);
	*a = strdup(*b);
}

static void SVG_MulString(SVG_String *a, Fixed k, SVG_String *c)
{
}

static void SVG_AddString(SMIL_AnimationStack *stack, SVG_String *a, SVG_Rect *b, SVG_String *c)
{
}

static u32 SVG_CompareString(SMIL_AnimationStack *stack, SVG_String *a, SVG_String *b)
{
	return strcmp(*a, *b);
}

static void SVG_ApplyAccumulateString(SMIL_AnimationStack *stack, u32 nb_iterations, SVG_String *current, SVG_String *last, SVG_String*accumulated)
{
}

static void SVG_InterpolateString(SMIL_AnimationStack *stack, Fixed interpolation_coefficient, SVG_String *value1, SVG_String *value2, SVG_String *target)
{
	if (interpolation_coefficient < INT2FIX(0.5)) 
		SVG_SetString(stack, target,value1);
	else 
		SVG_SetString(stack, target,value2);
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
/* end of SVG String animation functions */

/* SVG Font Family functions */
static void SVG_SetFontFamily(SMIL_AnimationStack *stack, SVG_FontFamily *a, SVG_FontFamily *b)
{
	a->type = b->type;
	if (a->value) free(a->value);
	a->value = strdup(b->value);
}

static void SVG_MulFontFamily(SVG_FontFamily *a, Fixed k, SVG_FontFamily *c)
{
}

static void SVG_AddFontFamily(SMIL_AnimationStack *stack, SVG_FontFamily *a, SVG_FontFamily *b, SVG_FontFamily *c)
{
}

static u32 SVG_CompareFontFamily(SMIL_AnimationStack *stack, SVG_FontFamily *a, SVG_FontFamily *b)
{
	if (a->type != b->type) return 1;
	if (!a->value || !b->value) return (a->value != b->value);
	return strcmp(a->value, b->value);
}

static void SVG_ApplyAccumulateFontFamily(SMIL_AnimationStack *stack, u32 nb_iterations, SVG_FontFamily *current, SVG_FontFamily *last, SVG_FontFamily*accumulated)
{
}

static void SVG_InterpolateFontFamily(SMIL_AnimationStack *stack, Fixed interpolation_coefficient, SVG_FontFamily *value1, SVG_FontFamily *value2, SVG_FontFamily *target)
{
	if (interpolation_coefficient < INT2FIX(0.5)) 
		SVG_SetFontFamily(stack, target,value1);
	else 
		SVG_SetFontFamily(stack, target,value2);
}

static void SVG_InitStackValuesFontFamily(SMIL_AnimationStack *stack)
{
	GF_SAFEALLOC(stack->base_value, sizeof(SVG_FontFamily))
	((SVG_FontFamily *)stack->base_value)->type = ((SVG_FontFamily *)stack->targetAttribute)->type;
	((SVG_FontFamily *)stack->base_value)->value = strdup(((SVG_FontFamily *)stack->targetAttribute)->value);
	GF_SAFEALLOC(stack->tmp_value, sizeof(SVG_FontFamily))
}

static void SVG_DeleteStackValuesFontFamily(SMIL_AnimationStack *stack)
{
	if (((SVG_FontFamily *)stack->base_value)->value) free(((SVG_FontFamily *)stack->base_value)->value);
	free(stack->base_value);
	if (((SVG_FontFamily *)stack->tmp_value)->value) free(((SVG_FontFamily *)stack->tmp_value)->value);
	free(stack->tmp_value);
}
/* end of SVG String animation functions */

/* Dash Path functions */
static void SVG_SetPath(SMIL_AnimationStack *stack, SVG_PathData *a, SVG_PathData *b)
{
	u32 i, count;

	for (i = 0; i < gf_list_count(a->commands); i++) {
		u8 *command = gf_list_get(a->commands, i);
		free(command);
	}
	gf_list_reset(a->commands);
	for (i = 0; i < gf_list_count(a->points); i++) {
		SVG_Point *pt = gf_list_get(a->points, i);
		free(pt);
	}
	gf_list_reset(a->points);
	
	count = gf_list_count(b->commands);
	for (i = 0; i < count; i ++) {
		u8 *nc = malloc(sizeof(u8));
		*nc = *(u8*)gf_list_get(b->commands, i);
		gf_list_add(a->commands, nc);
	}
	count = gf_list_count(b->points);
	for (i = 0; i < count; i ++) {
		SVG_Point *ptb = gf_list_get(b->points, i);
		SVG_Point *pta;
		GF_SAFEALLOC(pta, sizeof(SVG_Point))
		pta->x = ptb->x;
		pta->y = ptb->y;
		gf_list_add(a->points, pta);
	}
}

static void SVG_MulPath(SVG_PathData *a, Fixed k, SVG_PathData *c)
{
	/* TODO */
}

static void SVG_AddPath(SMIL_AnimationStack *stack, SVG_PathData *a, SVG_PathData *b, SVG_PathData *c)
{
	/* TODO */
}

static u32 SVG_ComparePath(SMIL_AnimationStack *stack, SVG_PathData *a, SVG_PathData *b)
{
	u32 i, ccount, pcount;

	ccount = gf_list_count(a->commands);
	pcount = gf_list_count(a->points);

	if (ccount != gf_list_count(b->commands)) return 1;
	if (pcount != gf_list_count(b->points)) return 1;

	for (i = 0; i < ccount; i++) {
		u8 *ac = gf_list_get(a->commands, i);
		u8 *bc = gf_list_get(b->commands, i);
		if (*ac != *bc) return 1;
	}
	for (i = 0; i < pcount; i++) {
		SVG_Point *pta = gf_list_get(a->points, i);
		SVG_Point *ptb = gf_list_get(b->points, i);
		if ((pta->x != ptb->x) || (pta->y != ptb->y)) return 1; 
	}
	return 0;
}

static void SVG_ApplyAccumulatePath(SMIL_AnimationStack *stack, u32 nb_iterations, SVG_PathData *current, SVG_PathData *last, SVG_PathData*accumulated)
{
	/* TODO */
}

static void SVG_InterpolatePath(SMIL_AnimationStack *stack, Fixed interpolation_coefficient, SVG_PathData *value1, SVG_PathData *value2, SVG_PathData *target)
{
	u32 i, count;

	for (i = 0; i < gf_list_count(target->commands); i++) {
		u8 *command = gf_list_get(target->commands, i);
		free(command);
	}
	for (i = 0; i < gf_list_count(target->points); i++) {
		SVG_Point *pt = gf_list_get(target->points, i);
		free(pt);
	}
	gf_list_reset(target->commands);
	gf_list_reset(target->points);
	
	count = gf_list_count(value1->commands);
	for (i = 0; i < count; i ++) {
		u8 *nc = malloc(sizeof(u8));
		*nc = *(u8*)gf_list_get(value1->commands, i);
		gf_list_add(target->commands, nc);
	}
	count = gf_list_count(value1->points);
	for (i = 0; i < count; i ++) {
		SVG_Point *pt3;
		SVG_Point *pt1 = gf_list_get(value1->points, i);
		SVG_Point *pt2 = gf_list_get(value2->points, i);
		GF_SAFEALLOC(pt3, sizeof(SVG_Point))
		pt3->x = SVG_Interpolate(pt1->x, pt2->x, interpolation_coefficient);
		pt3->y = SVG_Interpolate(pt1->y, pt2->y, interpolation_coefficient);
		gf_list_add(target->points, pt3);
	}
}

static void SVG_InitStackValuesPath(SMIL_AnimationStack *stack)
{
	GF_SAFEALLOC(stack->base_value, sizeof(SVG_PathData))
	((SVG_PathData *)stack->base_value)->commands = gf_list_new();
	((SVG_PathData *)stack->base_value)->points = gf_list_new();
	GF_SAFEALLOC(stack->tmp_value, sizeof(SVG_PathData))
	((SVG_PathData *)stack->tmp_value)->commands = gf_list_new();
	((SVG_PathData *)stack->tmp_value)->points = gf_list_new();
}

static void SVG_DeleteStackValuesPath(SMIL_AnimationStack *stack)
{
	SVG_DeletePath(((SVG_PathData *)stack->base_value));
	free(stack->base_value);
	SVG_DeletePath(((SVG_PathData *)stack->tmp_value));
	free(stack->tmp_value);
}
/* end of Path animation functions */

static void SVG_DeleteStack(SMIL_AnimationStack *stack)
{
	stack->DeleteStackValues(stack);
}

static void SVG_NoInitStackValues(void *stack) {}
static void SVG_NoDeleteStackValues(void *stack) {}
static void SVG_NoSet(void *stack, void *target, void *value) {}
static void SVG_NoAssign(void *stack, void *target, void *value) {}
static u32  SVG_NoCompare(void *stack, void *a, void *b) { return 0; }
static void SVG_NoInterpolate(void *stack, Fixed interpolation_coefficient, void *value1, void *value2, void *target) {}
static void SVG_NoApplyAdditive(void *stack, void *current_value, void *toApply, void *target) {}
static void SVG_NoApplyAccumulate(void *stack, u32 nb_iterations, void *current, void *last, void *accumulated) {}
static void SVG_NoInvalidate(void *stack){}

static void SVG_Init_SMILAnimationStackAPI(SMIL_AnimationStack *stack)
{
	stack->DeleteStack = SVG_DeleteStack;
	stack->previous_key_index = -1;
	if (!stack->dur) { /* this is a discard */
		stack->InitStackValues = SVG_NoInitStackValues;
		stack->DeleteStackValues = SVG_NoDeleteStackValues;
		stack->Set = SVG_NoSet;
		stack->Assign = SVG_NoAssign;
		stack->Interpolate = SVG_NoInterpolate;
		stack->ApplyAdditive = SVG_NoApplyAdditive;
		stack->ApplyAccumulate = SVG_NoApplyAccumulate;
		stack->Invalidate = SVG_NoInvalidate;
		stack->Compare = SVG_NoCompare;
		return;
	}
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
		stack->Invalidate = SVG_InvalidateAndDirtyAppearance;
		stack->Compare = SVG_CompareColor;
		break;
	case SVG_Paint_datatype:
		stack->InitStackValues = SVG_InitStackValuesPaint;
		stack->DeleteStackValues = SVG_DeleteStackValuesPaint;
		stack->Set = SVG_SetPaint;
		stack->Assign = SVG_SetPaint;
		stack->Interpolate = SVG_InterpolatePaint;
		stack->ApplyAdditive = SVG_AddPaint;
		stack->ApplyAccumulate = SVG_ApplyAccumulatePaint;
		stack->Invalidate = SVG_InvalidateAndDirtyAll;
		stack->Compare = SVG_ComparePaint;
		break;
	case SVG_StrokeWidth_datatype:
	case SVG_Length_datatype:
	case SVG_Coordinate_datatype:
		stack->InitStackValues = SVG_InitStackValuesLength;
		stack->DeleteStackValues = SVG_DeleteStackValuesLength;
		stack->Set = SVG_SetLength;
		stack->Assign = SVG_SetLength;
		stack->Interpolate = SVG_InterpolateLength;
		stack->ApplyAdditive = SVG_AddLength;
		stack->ApplyAccumulate = SVG_ApplyAccumulateLength;
		stack->Invalidate = SVG_InvalidateAndDirtyAll;
		stack->Compare = SVG_CompareLength;
		break;
	case SVG_TransformList_datatype:
		stack->InitStackValues = SVG_InitStackValuesTransform;
		stack->DeleteStackValues = SVG_DeleteStackValuesTransform;
		stack->ApplyAdditive = SVG_ApplyAdditiveTransform;
		stack->Invalidate = SVG_InvalidateNodeOnly;
		stack->Assign = SVG_SetTransform;
		stack->Compare = SVG_CompareTransform;
		stack->Set = SVG_SetTransform;
		stack->Interpolate = SVG_InterpolateTransform;
		stack->ApplyAccumulate = SVG_ApplyAccumulateTransform;
		break;
	case SVG_TransformType_datatype:
		stack->InitStackValues = SVG_InitStackValuesTransform;
		stack->DeleteStackValues = SVG_DeleteStackValuesTransform;
		stack->ApplyAdditive = SVG_ApplyAdditiveTransform;
		stack->Invalidate = SVG_InvalidateNodeOnly;
		stack->Assign = SVG_SetTransform;
		stack->Compare = SVG_CompareTransform;
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
			stack->Set = SVG_SetTransform;
			stack->Interpolate = SVG_InterpolateTransform;
			stack->ApplyAccumulate = SVG_ApplyAccumulateTransform;
			break;
		}
		break;
	/* The following are keyword animations */
	case SVG_TextAnchor_datatype:
	case SVG_FontStyle_datatype:
		stack->InitStackValues = SVG_InitStackValuesKeyword;
		stack->DeleteStackValues = SVG_DeleteStackValuesKeyword;
		stack->Set = SVG_SetKeyword;
		stack->Assign = SVG_SetKeyword;
		stack->Interpolate = SVG_InterpolateKeyword;
		stack->ApplyAdditive = SVG_ApplyAdditiveKeyword;
		stack->ApplyAccumulate = SVG_ApplyAccumulateKeyword;
		stack->Compare = SVG_CompareKeyword;
		stack->Invalidate = SVG_InvalidateAndDirtyAll;
		break;
	case SVG_FillRule_datatype:
	case SVG_StrokeLineCap_datatype:
	case SVG_StrokeLineJoin_datatype:
		stack->InitStackValues = SVG_InitStackValuesKeyword;
		stack->DeleteStackValues = SVG_DeleteStackValuesKeyword;
		stack->Set = SVG_SetKeyword;
		stack->Assign = SVG_SetKeyword;
		stack->Interpolate = SVG_InterpolateKeyword;
		stack->ApplyAdditive = SVG_ApplyAdditiveKeyword;
		stack->ApplyAccumulate = SVG_ApplyAccumulateKeyword;
		stack->Compare = SVG_CompareKeyword;
		stack->Invalidate = SVG_InvalidateAndDirtyAll;
		break;
	case SVG_Display_datatype:
	case SVG_Visibility_datatype:
		stack->InitStackValues = SVG_InitStackValuesKeyword;
		stack->DeleteStackValues = SVG_DeleteStackValuesKeyword;
		stack->Set = SVG_SetKeyword;
		stack->Assign = SVG_SetKeyword;
		stack->Interpolate = SVG_InterpolateKeyword;
		stack->ApplyAdditive = SVG_ApplyAdditiveKeyword;
		stack->ApplyAccumulate = SVG_ApplyAccumulateKeyword;
		stack->Compare = SVG_CompareKeyword;
		stack->Invalidate = SVG_InvalidateNodeOnly;
		break;
	case SVG_Motion_datatype:
		stack->InitStackValues = SVG_InitStackValuesTransform;
		stack->DeleteStackValues = SVG_DeleteStackValuesTransform;
		stack->Set = SVG_SetMotion;
		stack->Assign = SVG_SetTransform;
		stack->Interpolate = SVG_InterpolateMotion;
		stack->ApplyAdditive = SVG_ApplyAdditiveTransform;
		stack->ApplyAccumulate = SVG_ApplyAccumulateMotion;
		stack->Invalidate = SVG_InvalidateNodeOnly;
		stack->Compare = SVG_CompareTransform;
		break;
	case SVG_StrokeDashOffset_datatype:
	case SVG_Opacity_datatype:
	case SVG_StrokeMiterLimit_datatype:
		stack->InitStackValues = SVG_InitStackValuesIFloat;
		stack->DeleteStackValues = SVG_DeleteStackValuesIFloat;
		stack->Set = SVG_SetIFloat;
		stack->Assign = SVG_SetIFloat;
		stack->Interpolate = SVG_InterpolateIFloat;
		stack->ApplyAdditive = SVG_AddIFloat;
		stack->ApplyAccumulate = SVG_ApplyAccumulateIFloat;
		stack->Invalidate = SVG_InvalidateAndDirtyAll;
		stack->Compare = SVG_CompareIFloat;
		break;
	case SVG_FontSize_datatype:
		stack->InitStackValues = SVG_InitStackValuesIFloat;
		stack->DeleteStackValues = SVG_DeleteStackValuesIFloat;
		stack->Set = SVG_SetIFloat;
		stack->Assign = SVG_SetIFloat;
		stack->Interpolate = SVG_InterpolateIFloat;
		stack->ApplyAdditive = SVG_AddIFloat;
		stack->ApplyAccumulate = SVG_ApplyAccumulateIFloat;
		stack->Invalidate = SVG_InvalidateAndDirtyAll;
		stack->Compare = SVG_CompareIFloat;
		break;
	case SVG_StrokeDashArray_datatype:
		stack->InitStackValues = SVG_InitStackValuesDashArray;
		stack->DeleteStackValues = SVG_DeleteStackValuesDashArray;
		stack->Set = SVG_SetDashArray;
		stack->Assign = SVG_SetDashArray;
		stack->Interpolate = SVG_InterpolateDashArray;
		stack->ApplyAdditive = SVG_AddDashArray;
		stack->ApplyAccumulate = SVG_ApplyAccumulateDashArray;
		stack->Invalidate = SVG_InvalidateAndDirtyAll;
		stack->Compare = SVG_CompareDashArray;
		break;
	case SVG_ViewBox_datatype:
		stack->InitStackValues = SVG_InitStackValuesRect;
		stack->DeleteStackValues = SVG_DeleteStackValuesRect;
		stack->Set = SVG_SetRect;
		stack->Assign = SVG_SetRect;
		stack->Interpolate = SVG_InterpolateRect;
		stack->ApplyAdditive = SVG_AddRect;
		stack->ApplyAccumulate = SVG_ApplyAccumulateRect;
		stack->Invalidate = SVG_InvalidateNodeOnly;
		stack->Compare = SVG_CompareRect;
		break;
	case SVG_IRI_datatype:
		stack->InitStackValues = SVG_InitStackValuesIRI;
		stack->DeleteStackValues = SVG_DeleteStackValuesIRI;
		stack->Set = SVG_SetIRI;
		stack->Assign = SVG_AssignIRI;
		stack->Interpolate = SVG_InterpolateIRI;
		stack->ApplyAdditive = SVG_AddIRI;
		stack->ApplyAccumulate = SVG_ApplyAccumulateIRI;
		stack->Invalidate = SVG_InvalidateNodeOnly;
		stack->Compare = SVG_CompareIRI;
		break;
	case SVG_Coordinates_datatype:
		stack->InitStackValues = SVG_InitStackValuesCoords;
		stack->DeleteStackValues = SVG_DeleteStackValuesCoords;
		stack->Set = SVG_SetCoords;
		stack->Assign = SVG_SetCoords;
		stack->Interpolate = SVG_InterpolateCoords;
		stack->ApplyAdditive = SVG_AddCoords;
		stack->ApplyAccumulate = SVG_ApplyAccumulateCoords;
		stack->Invalidate = SVG_InvalidateAndDirtyGeometry;
		stack->Compare = SVG_CompareCoords;
		break;
	case SVG_Points_datatype:
		stack->InitStackValues = SVG_InitStackValuesPoints;
		stack->DeleteStackValues = SVG_DeleteStackValuesPoints;
		stack->Set = SVG_SetPoints;
		stack->Assign = SVG_SetPoints;
		stack->Interpolate = SVG_InterpolatePoints;
		stack->ApplyAdditive = SVG_AddPoints;
		stack->ApplyAccumulate = SVG_ApplyAccumulatePoints;
		stack->Invalidate = SVG_InvalidateAndDirtyGeometry;
		stack->Compare = SVG_ComparePoints;
		break;
	case SVG_String_datatype:
		stack->InitStackValues = SVG_InitStackValuesString;
		stack->DeleteStackValues = SVG_DeleteStackValuesString;
		stack->Set = SVG_SetString;
		stack->Assign = SVG_SetString;
		stack->Interpolate = SVG_InterpolateString;
		stack->ApplyAdditive = SVG_AddString;
		stack->ApplyAccumulate = SVG_ApplyAccumulateString;
		stack->Invalidate = SVG_InvalidateNodeOnly;
		stack->Compare = SVG_CompareString;
		break;
	case SVG_FontFamily_datatype:
		stack->InitStackValues = SVG_InitStackValuesFontFamily;
		stack->DeleteStackValues = SVG_DeleteStackValuesFontFamily;
		stack->Set = SVG_SetFontFamily;
		stack->Assign = SVG_SetFontFamily;
		stack->Interpolate = SVG_InterpolateFontFamily;
		stack->ApplyAdditive = SVG_AddFontFamily;
		stack->ApplyAccumulate = SVG_ApplyAccumulateFontFamily;
		stack->Invalidate = SVG_InvalidateAndDirtyGeometry;
		stack->Compare = SVG_CompareFontFamily;
		break;
	case SVG_PathData_datatype:
		stack->InitStackValues = SVG_InitStackValuesPath;
		stack->DeleteStackValues = SVG_DeleteStackValuesPath;
		stack->Set = SVG_SetPath;
		stack->Assign = SVG_SetPath;
		stack->Interpolate = SVG_InterpolatePath;
		stack->ApplyAdditive = SVG_AddPath;
		stack->ApplyAccumulate = SVG_ApplyAccumulatePath;
		stack->Invalidate = SVG_InvalidateAndDirtyGeometry;
		stack->Compare = SVG_ComparePath;
		break;
	default:
		fprintf(stderr, "Animation type not supported %d\n", stack->targetAttributeType);
		stack->InitStackValues = SVG_NoInitStackValues;
		stack->DeleteStackValues = SVG_NoDeleteStackValues;
		stack->Set = SVG_NoSet;
		stack->Assign = SVG_NoAssign;
		stack->Interpolate = SVG_NoInterpolate;
		stack->ApplyAdditive = SVG_NoApplyAdditive;
		stack->ApplyAccumulate = SVG_NoApplyAccumulate;
		stack->Invalidate = SVG_NoInvalidate;
		stack->Compare = SVG_NoCompare;
	}
}

/* Initialisation functions for all animation elements in SVG */
void SVG_Init_set(Render2D *sr, GF_Node *node)
{
	GF_FieldInfo info;
	SVGsetElement *set = (SVGsetElement *)node;
	SMIL_AnimationStack *stack;
	
	if (!(GF_Node*)set->xlink_href.target) return;

	stack = SMIL_Init_AnimationStack(sr, node);
	
	stack->target_element = (GF_Node*)set->xlink_href.target;
	stack->targetAttributeType = set->attributeName.type; 
	stack->targetAttribute = set->attributeName.field_ptr; 
	if (stack->targetAttributeType == SVG_TransformList_datatype && 
		!gf_node_get_field_by_name(stack->target_element, "transform", &info)) {
		GF_List *trlist = *(SVG_TransformList *)info.far_ptr;
		SVG_Transform *tr = gf_list_get(trlist, 0);
		if (!tr) {
			GF_SAFEALLOC(tr, sizeof(SVG_Transform));
			gf_mx2d_init(tr->matrix);
			gf_list_add(trlist, tr);
		}
		stack->targetAttribute = &(tr->matrix);
	}

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
	GF_FieldInfo info;
	SVGanimateElement *animate = (SVGanimateElement *)node;
	SMIL_AnimationStack *stack;
	
	if (!(GF_Node*)animate->xlink_href.target) return;

	stack = SMIL_Init_AnimationStack(sr, node);
	
	stack->target_element = (GF_Node*)animate->xlink_href.target;
	stack->targetAttributeType = animate->attributeName.type; 
	stack->targetAttribute = animate->attributeName.field_ptr; 
	if (stack->targetAttributeType == SVG_TransformList_datatype && 
		!gf_node_get_field_by_name(stack->target_element, "transform", &info)) {
		GF_List *trlist = *(SVG_TransformList *)info.far_ptr;
		SVG_Transform *tr = gf_list_get(trlist, 0);
		if (!tr) {
			GF_SAFEALLOC(tr, sizeof(SVG_Transform));
			gf_mx2d_init(tr->matrix);
			gf_list_add(trlist, tr);
		}
		stack->targetAttribute = &(tr->matrix);
	}

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
	SMIL_AnimationStack *stack;

	if (!(GF_Node*)ac->xlink_href.target) return;

	
	stack = SMIL_Init_AnimationStack(sr, node);
	
	stack->target_element = (GF_Node*)ac->xlink_href.target;
	stack->targetAttributeType = ac->attributeName.type; 
	stack->targetAttribute = ac->attributeName.field_ptr; 

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
	SMIL_AnimationStack *stack;
	
	if (!(GF_Node*)at->xlink_href.target) return;

	stack = SMIL_Init_AnimationStack(sr, node);
	
	stack->target_element = (GF_Node*)at->xlink_href.target;
	stack->targetAttributeType = SVG_TransformType_datatype; 
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
		stack->transformType = *(SVG_TransformType *)info.far_ptr;
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
	SMIL_AnimationStack *stack;

	if (!(GF_Node*)am->xlink_href.target) return;

	
	stack = SMIL_Init_AnimationStack(sr, node);
	
	stack->target_element = (GF_Node*)am->xlink_href.target;
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
	SMIL_AnimationStack *stack;

	if (!(GF_Node*)d->xlink_href.target) return;

	
	stack = SMIL_Init_AnimationStack(sr, node);
	
	stack->target_element = (GF_Node*)d->xlink_href.target;

	stack->begins = &(d->begin); 
	/* 'from', 'to', 'by', 'values', 'keyTimes', 'keySplines', ... are NULL */

	SVG_Init_SMILAnimationStackAPI(stack);
	SMIL_InitAnimateFunction(stack);
}

#endif

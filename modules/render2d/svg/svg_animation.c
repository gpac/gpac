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

GF_Err svg_attributes_muladd(Fixed alpha, GF_FieldInfo *a, 
							 Fixed beta, GF_FieldInfo *b, 
							 GF_FieldInfo *c, 
							 Bool clamp);
GF_Err svg_attributes_copy(GF_FieldInfo *a, GF_FieldInfo *b, Bool clamp);
GF_Err svg_attributes_add(GF_FieldInfo *a, GF_FieldInfo *b, GF_FieldInfo *c, Bool clamp);
GF_Err svg_attributes_interpolate(GF_FieldInfo *a, GF_FieldInfo *b, GF_FieldInfo *c, Fixed coef, Bool clamp);


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

static void SVG_NoSet(void *stack, GF_FieldInfo target, GF_FieldInfo value) {}
static void SVG_NoAssign(void *stack, GF_FieldInfo target, GF_FieldInfo value) {}
static u32  SVG_NoCompare(void *stack, GF_FieldInfo a, GF_FieldInfo b) { return 0; }
static void SVG_NoInterpolate(void *stack, Fixed interpolation_coefficient, GF_FieldInfo value1, GF_FieldInfo value2, GF_FieldInfo target) {}
static void SVG_NoApplyAdditive(void *stack, GF_FieldInfo current_value, GF_FieldInfo toApply, GF_FieldInfo target) {}
static void SVG_NoApplyAccumulate(void *stack, u32 nb_iterations, GF_FieldInfo current, GF_FieldInfo last, GF_FieldInfo accumulated) {}
static void SVG_NoInvalidate(void *stack){}

static void SVG_InitStackValues(SMIL_AnimationStack *stack, u32 tmp_value_type, u32 tmp_value_transform_type) {
	GF_FieldInfo target_info;

	stack->base_value.fieldType = stack->target_attribute.type;
	stack->base_value.eventType = stack->target_attribute.transform_type;
	stack->base_value.far_ptr	= svg_create_value_from_attributetype(stack->base_value.fieldType, 
																	stack->base_value.eventType);	 
	target_info.fieldType	= stack->target_attribute.type;
	target_info.eventType	= stack->target_attribute.transform_type;
	target_info.far_ptr		= stack->target_attribute.field_ptr;
	
	svg_attributes_copy(&stack->base_value, &target_info, 0);

	stack->tmp_value.fieldType	= tmp_value_type;
	stack->tmp_value.eventType	= tmp_value_transform_type;
	stack->tmp_value.far_ptr	= svg_create_value_from_attributetype(stack->tmp_value.fieldType, 
																	  stack->tmp_value.eventType);	 
}

static void SVG_InitStackValuesSameType(SMIL_AnimationStack *stack) {
	SVG_InitStackValues(stack, stack->target_attribute.type, stack->target_attribute.transform_type);
}


static void SVG_DeleteStackValues(void *stack) 
{
	return;
}

static void SVG_DeleteStack(SMIL_AnimationStack *stack)
{
	SVG_DeleteStackValues(stack);
}

static void SVG_Set(void *stack, GF_FieldInfo target, GF_FieldInfo value) 
{
	svg_attributes_copy(&target, &value, 0);
}

static u32  SVG_Compare(void *stack, GF_FieldInfo a, GF_FieldInfo b) 
{ 
	return 1; 
}

static void SVG_Interpolate(void *stack, Fixed interpolation_coefficient, GF_FieldInfo value1, GF_FieldInfo value2, GF_FieldInfo target) 
{
	svg_attributes_interpolate(&value1, &value2, &target, interpolation_coefficient, 1);
}

static void SVG_ApplyAdditive(void *stack, GF_FieldInfo current_value, GF_FieldInfo toApply, GF_FieldInfo target) 
{
	svg_attributes_add(&current_value, &toApply, &target, 1);
}

static void SVG_ApplyAccumulate(void *stack, u32 nb_iterations, GF_FieldInfo current, GF_FieldInfo last, GF_FieldInfo accumulated) 
{
	svg_attributes_muladd(INT2FIX(nb_iterations), &last, FIX_ONE, &current, &accumulated, 1);
}

static void SVG_Invalidate(void *stack)
{
	SVG_InvalidateAndDirtyAll(stack);
}

static void SVG_Init_SMILAnimationStackAPI(SMIL_AnimationStack *stack)
{
	stack->DeleteStack = SVG_DeleteStack;
	stack->previous_key_index = -1;
	if (!stack->dur) { /* this is a discard */
		stack->Set = SVG_NoSet;
		stack->Assign = SVG_NoAssign;
		stack->Interpolate = SVG_NoInterpolate;
		stack->ApplyAdditive = SVG_NoApplyAdditive;
		stack->ApplyAccumulate = SVG_NoApplyAccumulate;
		stack->Invalidate = SVG_NoInvalidate;
		stack->Compare = SVG_NoCompare;
		return;
	}
	stack->Set = SVG_Set;
	stack->Assign = SVG_Set;
	stack->Interpolate = SVG_Interpolate;
	stack->ApplyAdditive = SVG_ApplyAdditive;
	stack->ApplyAccumulate = SVG_ApplyAccumulate;
	stack->Invalidate = SVG_Invalidate;
	stack->Compare = SVG_Compare;

}

/* Initialisation functions for all animation elements in SVG */
void SVG_Init_set(Render2D *sr, GF_Node *node)
{
	SVGsetElement *set = (SVGsetElement *)node;
	SMIL_AnimationStack *stack;
	
	if (!(GF_Node*)set->xlink_href.target) return;

	stack = SMIL_Init_AnimationStack(sr, node);
	
	stack->target_element = (GF_Node*)set->xlink_href.target;
	stack->target_attribute = set->attributeName; 
	SVG_InitStackValuesSameType(stack);

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
	SMIL_AnimationStack *stack;
	
	if (!(GF_Node*)animate->xlink_href.target) return;

	stack = SMIL_Init_AnimationStack(sr, node);
	
	stack->target_element = (GF_Node*)animate->xlink_href.target;
	stack->target_attribute = animate->attributeName; 
	SVG_InitStackValuesSameType(stack);

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
	stack->target_attribute = ac->attributeName; 
	SVG_InitStackValuesSameType(stack);

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
	u32 transform_type = 0;
	SVGanimateTransformElement *at = (SVGanimateTransformElement *)node;
	SMIL_AnimationStack *stack;
	
	if (!(GF_Node*)at->xlink_href.target) return;

	stack = SMIL_Init_AnimationStack(sr, node);
	
	stack->target_element = (GF_Node*)at->xlink_href.target;
	stack->target_attribute.type = SVG_Matrix_datatype;
	stack->target_attribute.transform_type = 0;
	if (!gf_node_get_field_by_name(stack->target_element, "transform", &info)) {
		stack->target_attribute.field_ptr = info.far_ptr; 
	}
	if (!gf_node_get_field_by_name(node, "type", &info)) {
		transform_type = *(u8*)info.far_ptr; 
	}
	SVG_InitStackValues(stack, SVG_Matrix_datatype, transform_type);

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
	stack->target_attribute.type		   = SVG_Matrix_datatype;
	stack->target_attribute.transform_type = 0;
	if (!gf_node_get_field_by_name(stack->target_element, "transform", &info)) {
		stack->target_attribute.field_ptr = info.far_ptr; 
	}
	SVG_InitStackValues(stack, SVG_Motion_datatype, 0);

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

	if (!stack->to->type && !stack->by->type && !stack->values->type) stack->path = &(am->path);

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

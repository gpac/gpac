/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Copyright (c) Cyril Concolato 2004
 *					All rights reserved
 *
 *  This file is part of GPAC / SMIL Rendering sub-project
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
#ifndef _SMIL_STACKS_H
#define _SMIL_STACKS_H

#include "../render2d.h"

#ifndef GPAC_DISABLE_SMIL

#include "../stacks2d.h"

/* Animation functions */

/* status of an SMIL animated element */ 
enum {
	SMIL_STATUS_STARTUP = 0,
	SMIL_STATUS_WAITING_TO_BEGIN,
	SMIL_STATUS_ACTIVE,
	SMIL_STATUS_END_INTERVAL,
	SMIL_STATUS_POST_ACTIVE,
	SMIL_STATUS_DONE
};

typedef struct {
	u32 activation_cycle;
	u32 nb_iterations;

	/* negative values mean indefinite */
	Double begin, 
		   end,
		   simple_duration, 
		   active_duration;

} SMIL_Interval;

typedef struct _smil_anim_stack
{
	GF_TimeNode time_handle;
	GF_Renderer *compositor;

	Bool is_registered_in_cycle;

	/* SMIL element life-cycle status */
	u8 status;
	u32 cycle_number;

	/* List of possible intervals for activation of the animation */
	GF_List *intervals;
	s32	current_interval_index;
	SMIL_Interval *current_interval;

	s32 previous_key_index;
	Fixed previous_coef;
	Bool target_value_changed;

	/* stores the DOM value */
	GF_FieldInfo base_value;

	/* stores the intermediate value during computation*/
	GF_FieldInfo tmp_value;

	/* animation attributes of the timenode */
	
	/* attributes to identify the target element of an animation: 
	       SVG.Animation.attrib 
	*/
	GF_Node *target_element;
	
	/* the target attribute or property */
	SMIL_AttributeName target_attribute;
	
	/* attributes to control the timing of the animation 
	       SVG.AnimationTiming.attrib
	*/
	SMIL_Times *begins; 
	SMIL_Duration *dur; 
	SMIL_Times *ends; 
	SMIL_Restart *restart; 
	SMIL_RepeatCount *repeatCount; 
	SMIL_Duration *repeatDur; 
	SMIL_Fill *fill; 
	SMIL_Duration *min; 
	SMIL_Duration *max; 
	/* attributes that define animation values over time 
	       SVG.AnimationValue.attrib
	*/
	SMIL_CalcMode *calcMode; 
	SMIL_AnimateValues *values; 
	SMIL_KeyTimes *keyTimes;
	u32 last_keytime_index;
	SMIL_KeySplines *keySplines; 
	SMIL_KeyPoints *keyPoints; 
	SMIL_AnimateValue *from; 
	SMIL_AnimateValue *to; 
	SMIL_AnimateValue *by; 
	/* attributes to control whether animations are additive 
	       SVG.AnimationAddition.attrib
	*/
	SMIL_Additive *additive; 
	SMIL_Accumulate *accumulate; 

	/* additional attributes for animateMotion*/
	/* */
	SVG_PathData *path;

	/* additional attributes for animateTransform*/
	/* */

	/* generic api for animation */
	void (*DeleteStack)(struct _smil_anim_stack *stack);

	void (*Animate)(struct _smil_anim_stack *stack, Double sceneTime);

	/* Sets the target using the given value
	   The target type depends on the animation.
	   The value type depends on the animation and is not necessarily the same as the target type. */
	void (*Set)(struct _smil_anim_stack *stack, GF_FieldInfo target, GF_FieldInfo value);

	/* Sets the target using the given value
	   The target type depends on the animation.
	   The value type MUST BE the same as the target type. */
	void (*Assign)(struct _smil_anim_stack *stack, GF_FieldInfo target, GF_FieldInfo value);

	/* returns 1 if different, a and b must be non NULL and of same type */
	u32 (*Compare)(struct _smil_anim_stack *stack, GF_FieldInfo a, GF_FieldInfo b);

	/* Linearly interpolates a value from value1 to value2 using the given coef
	   The target type depends on the animation.
	   The value1 and value2 type depends on the animation and are not necessarily the same as the target type. */
	void (*Interpolate)(struct _smil_anim_stack *stack, Fixed interpolation_coefficient, GF_FieldInfo value1, GF_FieldInfo value2, GF_FieldInfo target);

	/* Current value can be either the dom or the tmp value depending on the value of additive
	   all parameters are of same type */
	void (*ApplyAdditive)(struct _smil_anim_stack *stack, GF_FieldInfo current_value, GF_FieldInfo toApply, GF_FieldInfo target);
	/* current is the temporary value, 
	   last is the last specified in the animation 
	   accumulated is the result
	   current and accumulated are of the same type. last may not be of the same type */
	void (*ApplyAccumulate)(struct _smil_anim_stack *stack, u32 nb_iterations, GF_FieldInfo current, GF_FieldInfo last, GF_FieldInfo accumulated);

	void (*Invalidate)(struct _smil_anim_stack *stack);

} SMIL_AnimationStack;

/* To be called for GPAC core initialisation of timed element */
SMIL_AnimationStack *SMIL_Init_AnimationStack(Render2D *sr, GF_Node *node);

/* To be called after the animation attributes have been assigned in the stack */
void SMIL_InitAnimateFunction(SMIL_AnimationStack *stack);

void SMIL_Update_Animation(GF_TimeNode *timenode);
void SMIL_Modified_Animation(GF_Node *node);

#endif // GPAC_DISABLE_SMIL

#endif // _SMIL_STACKS_H


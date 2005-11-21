/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Copyright (c) Jean Le Feuvre 2000-2005
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


/*
	DO NOT MOFIFY - File generated on GMT Mon Nov 21 16:56:09 2005

	BY MPEG4Gen for GPAC Version 0.4.1-DEV
*/

#include <gpac/nodes_mpeg4.h>


#include <gpac/internal/scenegraph_dev.h>

/*
	Anchor Node deletion
*/

static void Anchor_Del(GF_Node *node)
{
	M_Anchor *p = (M_Anchor *) node;
	gf_sg_vrml_parent_reset((GF_Node *) p);	
	gf_sg_sfstring_del(p->description);
	gf_sg_mfstring_del(p->parameter);
	gf_sg_mfurl_del(p->url);
	gf_node_free((GF_Node *) p);
}

static const u16 Anchor_Def2All[] = { 2, 3, 4, 5};
static const u16 Anchor_In2All[] = { 0, 1, 2, 3, 4, 5, 6};
static const u16 Anchor_Out2All[] = { 2, 3, 4, 5};

static u32 Anchor_get_field_count(GF_Node *node, u8 IndexMode)
{
	switch(IndexMode) {
	case GF_SG_FIELD_CODING_IN: return 7;
	case GF_SG_FIELD_CODING_DEF: return 4;
	case GF_SG_FIELD_CODING_OUT: return 4;
	case GF_SG_FIELD_CODING_DYN: return 0;
	default:
		return 7;
	}
}

static GF_Err Anchor_get_field_index(GF_Node *n, u32 inField, u8 IndexMode, u32 *allField)
{
	switch(IndexMode) {
	case GF_SG_FIELD_CODING_IN:
		*allField = Anchor_In2All[inField];
		return GF_OK;
	case GF_SG_FIELD_CODING_DEF:
		*allField = Anchor_Def2All[inField];
		return GF_OK;
	case GF_SG_FIELD_CODING_OUT:
		*allField = Anchor_Out2All[inField];
		return GF_OK;
	default:
		return GF_BAD_PARAM;
	}
}
static GF_Err Anchor_get_field(GF_Node *node, GF_FieldInfo *info)
{
	switch (info->fieldIndex) {
	case 0:
		info->name = "addChildren";
		info->eventType = GF_SG_EVENT_IN;
		info->on_event_in = ((M_Anchor *)node)->on_addChildren;
		info->fieldType = GF_SG_VRML_MFNODE;
		info->NDTtype = NDT_SF3DNode;
		info->far_ptr = & ((M_Anchor *)node)->addChildren;
		return GF_OK;
	case 1:
		info->name = "removeChildren";
		info->eventType = GF_SG_EVENT_IN;
		info->on_event_in = ((M_Anchor *)node)->on_removeChildren;
		info->fieldType = GF_SG_VRML_MFNODE;
		info->NDTtype = NDT_SF3DNode;
		info->far_ptr = & ((M_Anchor *)node)->removeChildren;
		return GF_OK;
	case 2:
		info->name = "children";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_MFNODE;
		info->NDTtype = NDT_SF3DNode;
		info->far_ptr = & ((M_Anchor *)node)->children;
		return GF_OK;
	case 3:
		info->name = "description";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFSTRING;
		info->far_ptr = & ((M_Anchor *) node)->description;
		return GF_OK;
	case 4:
		info->name = "parameter";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_MFSTRING;
		info->far_ptr = & ((M_Anchor *) node)->parameter;
		return GF_OK;
	case 5:
		info->name = "url";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_MFURL;
		info->far_ptr = & ((M_Anchor *) node)->url;
		return GF_OK;
	case 6:
		info->name = "activate";
		info->eventType = GF_SG_EVENT_IN;
		info->on_event_in = ((M_Anchor *)node)->on_activate;
		info->fieldType = GF_SG_VRML_SFBOOL;
		info->far_ptr = & ((M_Anchor *) node)->activate;
		return GF_OK;
	default:
		return GF_BAD_PARAM;
	}
}

static Bool Anchor_get_aq_info(GF_Node *n, u32 FieldIndex, u8 *QType, u8 *AType, Fixed *b_min, Fixed *b_max, u32 *QT13_bits)
{
	switch (FieldIndex) {
	default:
		return 0;
	}
}



GF_Node *Anchor_Create()
{
	M_Anchor *p;
	GF_SAFEALLOC(p, sizeof(M_Anchor));
	if(!p) return NULL;
	gf_node_setup((GF_Node *)p, TAG_MPEG4_Anchor);
	gf_sg_vrml_parent_setup((GF_Node *) p);

#ifdef GF_NODE_USE_POINTERS
	((GF_Node *)p)->sgprivate->name = "Anchor";
	((GF_Node *)p)->sgprivate->node_del = Anchor_Del;
	((GF_Node *)p)->sgprivate->get_field_count = Anchor_get_field_count;
	((GF_Node *)p)->sgprivate->get_field = Anchor_get_field;
#endif


	/*default field values*/
	return (GF_Node *)p;
}


/*
	AnimationStream Node deletion
*/

static void AnimationStream_Del(GF_Node *node)
{
	M_AnimationStream *p = (M_AnimationStream *) node;
	gf_sg_mfurl_del(p->url);
	gf_node_free((GF_Node *) p);
}

static const u16 AnimationStream_Def2All[] = { 0, 1, 2, 3, 4};
static const u16 AnimationStream_In2All[] = { 0, 1, 2, 3, 4};
static const u16 AnimationStream_Out2All[] = { 0, 1, 2, 3, 4, 5, 6};
static const u16 AnimationStream_Dyn2All[] = { 1};

static u32 AnimationStream_get_field_count(GF_Node *node, u8 IndexMode)
{
	switch(IndexMode) {
	case GF_SG_FIELD_CODING_IN: return 5;
	case GF_SG_FIELD_CODING_DEF: return 5;
	case GF_SG_FIELD_CODING_OUT: return 7;
	case GF_SG_FIELD_CODING_DYN: return 1;
	default:
		return 7;
	}
}

static GF_Err AnimationStream_get_field_index(GF_Node *n, u32 inField, u8 IndexMode, u32 *allField)
{
	switch(IndexMode) {
	case GF_SG_FIELD_CODING_IN:
		*allField = AnimationStream_In2All[inField];
		return GF_OK;
	case GF_SG_FIELD_CODING_DEF:
		*allField = AnimationStream_Def2All[inField];
		return GF_OK;
	case GF_SG_FIELD_CODING_OUT:
		*allField = AnimationStream_Out2All[inField];
		return GF_OK;
	case GF_SG_FIELD_CODING_DYN:
		*allField = AnimationStream_Dyn2All[inField];
		return GF_OK;
	default:
		return GF_BAD_PARAM;
	}
}
static GF_Err AnimationStream_get_field(GF_Node *node, GF_FieldInfo *info)
{
	switch (info->fieldIndex) {
	case 0:
		info->name = "loop";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFBOOL;
		info->far_ptr = & ((M_AnimationStream *) node)->loop;
		return GF_OK;
	case 1:
		info->name = "speed";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFFLOAT;
		info->far_ptr = & ((M_AnimationStream *) node)->speed;
		return GF_OK;
	case 2:
		info->name = "startTime";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFTIME;
		info->far_ptr = & ((M_AnimationStream *) node)->startTime;
		return GF_OK;
	case 3:
		info->name = "stopTime";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFTIME;
		info->far_ptr = & ((M_AnimationStream *) node)->stopTime;
		return GF_OK;
	case 4:
		info->name = "url";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_MFURL;
		info->far_ptr = & ((M_AnimationStream *) node)->url;
		return GF_OK;
	case 5:
		info->name = "duration_changed";
		info->eventType = GF_SG_EVENT_OUT;
		info->fieldType = GF_SG_VRML_SFTIME;
		info->far_ptr = & ((M_AnimationStream *) node)->duration_changed;
		return GF_OK;
	case 6:
		info->name = "isActive";
		info->eventType = GF_SG_EVENT_OUT;
		info->fieldType = GF_SG_VRML_SFBOOL;
		info->far_ptr = & ((M_AnimationStream *) node)->isActive;
		return GF_OK;
	default:
		return GF_BAD_PARAM;
	}
}

static Bool AnimationStream_get_aq_info(GF_Node *n, u32 FieldIndex, u8 *QType, u8 *AType, Fixed *b_min, Fixed *b_max, u32 *QT13_bits)
{
	switch (FieldIndex) {
	case 1:
		*AType = 7;
		*QType = 0;
		*b_min = FIX_MIN;
		*b_max = FIX_MAX;
		return 1;
	default:
		return 0;
	}
}



GF_Node *AnimationStream_Create()
{
	M_AnimationStream *p;
	GF_SAFEALLOC(p, sizeof(M_AnimationStream));
	if(!p) return NULL;
	gf_node_setup((GF_Node *)p, TAG_MPEG4_AnimationStream);

#ifdef GF_NODE_USE_POINTERS
	((GF_Node *)p)->sgprivate->name = "AnimationStream";
	((GF_Node *)p)->sgprivate->node_del = AnimationStream_Del;
	((GF_Node *)p)->sgprivate->get_field_count = AnimationStream_get_field_count;
	((GF_Node *)p)->sgprivate->get_field = AnimationStream_get_field;
#endif


	/*default field values*/
	p->speed = FLT2FIX(1.0);
	p->startTime = 0;
	p->stopTime = 0;
	return (GF_Node *)p;
}


/*
	Appearance Node deletion
*/

static void Appearance_Del(GF_Node *node)
{
	M_Appearance *p = (M_Appearance *) node;
	gf_node_unregister((GF_Node *) p->material, (GF_Node *) p);	
	gf_node_unregister((GF_Node *) p->texture, (GF_Node *) p);	
	gf_node_unregister((GF_Node *) p->textureTransform, (GF_Node *) p);	
	gf_node_free((GF_Node *) p);
}

static const u16 Appearance_Def2All[] = { 0, 1, 2};
static const u16 Appearance_In2All[] = { 0, 1, 2};
static const u16 Appearance_Out2All[] = { 0, 1, 2};

static u32 Appearance_get_field_count(GF_Node *node, u8 IndexMode)
{
	switch(IndexMode) {
	case GF_SG_FIELD_CODING_IN: return 3;
	case GF_SG_FIELD_CODING_DEF: return 3;
	case GF_SG_FIELD_CODING_OUT: return 3;
	case GF_SG_FIELD_CODING_DYN: return 0;
	default:
		return 3;
	}
}

static GF_Err Appearance_get_field_index(GF_Node *n, u32 inField, u8 IndexMode, u32 *allField)
{
	switch(IndexMode) {
	case GF_SG_FIELD_CODING_IN:
		*allField = Appearance_In2All[inField];
		return GF_OK;
	case GF_SG_FIELD_CODING_DEF:
		*allField = Appearance_Def2All[inField];
		return GF_OK;
	case GF_SG_FIELD_CODING_OUT:
		*allField = Appearance_Out2All[inField];
		return GF_OK;
	default:
		return GF_BAD_PARAM;
	}
}
static GF_Err Appearance_get_field(GF_Node *node, GF_FieldInfo *info)
{
	switch (info->fieldIndex) {
	case 0:
		info->name = "material";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFNODE;
		info->NDTtype = NDT_SFMaterialNode;
		info->far_ptr = & ((M_Appearance *)node)->material;
		return GF_OK;
	case 1:
		info->name = "texture";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFNODE;
		info->NDTtype = NDT_SFTextureNode;
		info->far_ptr = & ((M_Appearance *)node)->texture;
		return GF_OK;
	case 2:
		info->name = "textureTransform";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFNODE;
		info->NDTtype = NDT_SFTextureTransformNode;
		info->far_ptr = & ((M_Appearance *)node)->textureTransform;
		return GF_OK;
	default:
		return GF_BAD_PARAM;
	}
}

static Bool Appearance_get_aq_info(GF_Node *n, u32 FieldIndex, u8 *QType, u8 *AType, Fixed *b_min, Fixed *b_max, u32 *QT13_bits)
{
	switch (FieldIndex) {
	default:
		return 0;
	}
}



GF_Node *Appearance_Create()
{
	M_Appearance *p;
	GF_SAFEALLOC(p, sizeof(M_Appearance));
	if(!p) return NULL;
	gf_node_setup((GF_Node *)p, TAG_MPEG4_Appearance);

#ifdef GF_NODE_USE_POINTERS
	((GF_Node *)p)->sgprivate->name = "Appearance";
	((GF_Node *)p)->sgprivate->node_del = Appearance_Del;
	((GF_Node *)p)->sgprivate->get_field_count = Appearance_get_field_count;
	((GF_Node *)p)->sgprivate->get_field = Appearance_get_field;
#endif


	/*default field values*/
	return (GF_Node *)p;
}


/*
	AudioBuffer Node deletion
*/

static void AudioBuffer_Del(GF_Node *node)
{
	M_AudioBuffer *p = (M_AudioBuffer *) node;
	gf_node_list_del((GF_List *) p->children, (GF_Node *) p);	
	gf_sg_mfint32_del(p->phaseGroup);
	gf_node_free((GF_Node *) p);
}

static const u16 AudioBuffer_Def2All[] = { 0, 1, 2, 3, 4, 5, 6, 7};
static const u16 AudioBuffer_In2All[] = { 0, 1, 2, 3, 4, 5, 6, 7};
static const u16 AudioBuffer_Out2All[] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9};
static const u16 AudioBuffer_Dyn2All[] = { 1};

static u32 AudioBuffer_get_field_count(GF_Node *node, u8 IndexMode)
{
	switch(IndexMode) {
	case GF_SG_FIELD_CODING_IN: return 8;
	case GF_SG_FIELD_CODING_DEF: return 8;
	case GF_SG_FIELD_CODING_OUT: return 10;
	case GF_SG_FIELD_CODING_DYN: return 1;
	default:
		return 10;
	}
}

static GF_Err AudioBuffer_get_field_index(GF_Node *n, u32 inField, u8 IndexMode, u32 *allField)
{
	switch(IndexMode) {
	case GF_SG_FIELD_CODING_IN:
		*allField = AudioBuffer_In2All[inField];
		return GF_OK;
	case GF_SG_FIELD_CODING_DEF:
		*allField = AudioBuffer_Def2All[inField];
		return GF_OK;
	case GF_SG_FIELD_CODING_OUT:
		*allField = AudioBuffer_Out2All[inField];
		return GF_OK;
	case GF_SG_FIELD_CODING_DYN:
		*allField = AudioBuffer_Dyn2All[inField];
		return GF_OK;
	default:
		return GF_BAD_PARAM;
	}
}
static GF_Err AudioBuffer_get_field(GF_Node *node, GF_FieldInfo *info)
{
	switch (info->fieldIndex) {
	case 0:
		info->name = "loop";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFBOOL;
		info->far_ptr = & ((M_AudioBuffer *) node)->loop;
		return GF_OK;
	case 1:
		info->name = "pitch";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFFLOAT;
		info->far_ptr = & ((M_AudioBuffer *) node)->pitch;
		return GF_OK;
	case 2:
		info->name = "startTime";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFTIME;
		info->far_ptr = & ((M_AudioBuffer *) node)->startTime;
		return GF_OK;
	case 3:
		info->name = "stopTime";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFTIME;
		info->far_ptr = & ((M_AudioBuffer *) node)->stopTime;
		return GF_OK;
	case 4:
		info->name = "children";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_MFNODE;
		info->NDTtype = NDT_SFAudioNode;
		info->far_ptr = & ((M_AudioBuffer *)node)->children;
		return GF_OK;
	case 5:
		info->name = "numChan";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFINT32;
		info->far_ptr = & ((M_AudioBuffer *) node)->numChan;
		return GF_OK;
	case 6:
		info->name = "phaseGroup";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_MFINT32;
		info->far_ptr = & ((M_AudioBuffer *) node)->phaseGroup;
		return GF_OK;
	case 7:
		info->name = "length";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFFLOAT;
		info->far_ptr = & ((M_AudioBuffer *) node)->length;
		return GF_OK;
	case 8:
		info->name = "duration_changed";
		info->eventType = GF_SG_EVENT_OUT;
		info->fieldType = GF_SG_VRML_SFTIME;
		info->far_ptr = & ((M_AudioBuffer *) node)->duration_changed;
		return GF_OK;
	case 9:
		info->name = "isActive";
		info->eventType = GF_SG_EVENT_OUT;
		info->fieldType = GF_SG_VRML_SFBOOL;
		info->far_ptr = & ((M_AudioBuffer *) node)->isActive;
		return GF_OK;
	default:
		return GF_BAD_PARAM;
	}
}

static Bool AudioBuffer_get_aq_info(GF_Node *n, u32 FieldIndex, u8 *QType, u8 *AType, Fixed *b_min, Fixed *b_max, u32 *QT13_bits)
{
	switch (FieldIndex) {
	case 1:
		*AType = 7;
		*QType = 0;
		*b_min = FLT2FIX(0);
		*b_max = FIX_MAX;
		return 1;
	case 2:
		*AType = 0;
		*QType = 0;
		*b_min = FLT2FIX(0);
		*b_max = FIX_MAX;
		return 1;
	case 3:
		*AType = 0;
		*QType = 0;
		*b_min = FLT2FIX(0);
		*b_max = FIX_MAX;
		return 1;
	case 5:
		*AType = 0;
		*QType = 13;
		*QT13_bits = 8;
		*b_min = FLT2FIX(0);
		*b_max = FLT2FIX(255);
		return 1;
	case 7:
		*AType = 0;
		*QType = 0;
		*b_min = FLT2FIX(0);
		*b_max = FIX_MAX;
		return 1;
	default:
		return 0;
	}
}



GF_Node *AudioBuffer_Create()
{
	M_AudioBuffer *p;
	GF_SAFEALLOC(p, sizeof(M_AudioBuffer));
	if(!p) return NULL;
	gf_node_setup((GF_Node *)p, TAG_MPEG4_AudioBuffer);
	p->children = gf_list_new();	

#ifdef GF_NODE_USE_POINTERS
	((GF_Node *)p)->sgprivate->name = "AudioBuffer";
	((GF_Node *)p)->sgprivate->node_del = AudioBuffer_Del;
	((GF_Node *)p)->sgprivate->get_field_count = AudioBuffer_get_field_count;
	((GF_Node *)p)->sgprivate->get_field = AudioBuffer_get_field;
#endif


	/*default field values*/
	p->pitch = FLT2FIX(1);
	p->startTime = 0;
	p->stopTime = 0;
	p->numChan = 1;
	p->phaseGroup.vals = malloc(sizeof(SFInt32)*1);
	p->phaseGroup.count = 1;
	p->phaseGroup.vals[0] = 1;
	p->length = FLT2FIX(0.0);
	return (GF_Node *)p;
}


/*
	AudioClip Node deletion
*/

static void AudioClip_Del(GF_Node *node)
{
	M_AudioClip *p = (M_AudioClip *) node;
	gf_sg_sfstring_del(p->description);
	gf_sg_mfurl_del(p->url);
	gf_node_free((GF_Node *) p);
}

static const u16 AudioClip_Def2All[] = { 0, 1, 2, 3, 4, 5};
static const u16 AudioClip_In2All[] = { 0, 1, 2, 3, 4, 5};
static const u16 AudioClip_Out2All[] = { 0, 1, 2, 3, 4, 5, 6, 7};
static const u16 AudioClip_Dyn2All[] = { 2};

static u32 AudioClip_get_field_count(GF_Node *node, u8 IndexMode)
{
	switch(IndexMode) {
	case GF_SG_FIELD_CODING_IN: return 6;
	case GF_SG_FIELD_CODING_DEF: return 6;
	case GF_SG_FIELD_CODING_OUT: return 8;
	case GF_SG_FIELD_CODING_DYN: return 1;
	default:
		return 8;
	}
}

static GF_Err AudioClip_get_field_index(GF_Node *n, u32 inField, u8 IndexMode, u32 *allField)
{
	switch(IndexMode) {
	case GF_SG_FIELD_CODING_IN:
		*allField = AudioClip_In2All[inField];
		return GF_OK;
	case GF_SG_FIELD_CODING_DEF:
		*allField = AudioClip_Def2All[inField];
		return GF_OK;
	case GF_SG_FIELD_CODING_OUT:
		*allField = AudioClip_Out2All[inField];
		return GF_OK;
	case GF_SG_FIELD_CODING_DYN:
		*allField = AudioClip_Dyn2All[inField];
		return GF_OK;
	default:
		return GF_BAD_PARAM;
	}
}
static GF_Err AudioClip_get_field(GF_Node *node, GF_FieldInfo *info)
{
	switch (info->fieldIndex) {
	case 0:
		info->name = "description";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFSTRING;
		info->far_ptr = & ((M_AudioClip *) node)->description;
		return GF_OK;
	case 1:
		info->name = "loop";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFBOOL;
		info->far_ptr = & ((M_AudioClip *) node)->loop;
		return GF_OK;
	case 2:
		info->name = "pitch";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFFLOAT;
		info->far_ptr = & ((M_AudioClip *) node)->pitch;
		return GF_OK;
	case 3:
		info->name = "startTime";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFTIME;
		info->far_ptr = & ((M_AudioClip *) node)->startTime;
		return GF_OK;
	case 4:
		info->name = "stopTime";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFTIME;
		info->far_ptr = & ((M_AudioClip *) node)->stopTime;
		return GF_OK;
	case 5:
		info->name = "url";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_MFURL;
		info->far_ptr = & ((M_AudioClip *) node)->url;
		return GF_OK;
	case 6:
		info->name = "duration_changed";
		info->eventType = GF_SG_EVENT_OUT;
		info->fieldType = GF_SG_VRML_SFTIME;
		info->far_ptr = & ((M_AudioClip *) node)->duration_changed;
		return GF_OK;
	case 7:
		info->name = "isActive";
		info->eventType = GF_SG_EVENT_OUT;
		info->fieldType = GF_SG_VRML_SFBOOL;
		info->far_ptr = & ((M_AudioClip *) node)->isActive;
		return GF_OK;
	default:
		return GF_BAD_PARAM;
	}
}

static Bool AudioClip_get_aq_info(GF_Node *n, u32 FieldIndex, u8 *QType, u8 *AType, Fixed *b_min, Fixed *b_max, u32 *QT13_bits)
{
	switch (FieldIndex) {
	case 2:
		*AType = 7;
		*QType = 0;
		*b_min = FLT2FIX(0);
		*b_max = FIX_MAX;
		return 1;
	default:
		return 0;
	}
}



GF_Node *AudioClip_Create()
{
	M_AudioClip *p;
	GF_SAFEALLOC(p, sizeof(M_AudioClip));
	if(!p) return NULL;
	gf_node_setup((GF_Node *)p, TAG_MPEG4_AudioClip);

#ifdef GF_NODE_USE_POINTERS
	((GF_Node *)p)->sgprivate->name = "AudioClip";
	((GF_Node *)p)->sgprivate->node_del = AudioClip_Del;
	((GF_Node *)p)->sgprivate->get_field_count = AudioClip_get_field_count;
	((GF_Node *)p)->sgprivate->get_field = AudioClip_get_field;
#endif


	/*default field values*/
	p->pitch = FLT2FIX(1.0);
	p->startTime = 0;
	p->stopTime = 0;
	return (GF_Node *)p;
}


/*
	AudioDelay Node deletion
*/

static void AudioDelay_Del(GF_Node *node)
{
	M_AudioDelay *p = (M_AudioDelay *) node;
	gf_sg_vrml_parent_reset((GF_Node *) p);	
	gf_sg_mfint32_del(p->phaseGroup);
	gf_node_free((GF_Node *) p);
}

static const u16 AudioDelay_Def2All[] = { 2, 3, 4, 5};
static const u16 AudioDelay_In2All[] = { 0, 1, 2, 3};
static const u16 AudioDelay_Out2All[] = { 2, 3};

static u32 AudioDelay_get_field_count(GF_Node *node, u8 IndexMode)
{
	switch(IndexMode) {
	case GF_SG_FIELD_CODING_IN: return 4;
	case GF_SG_FIELD_CODING_DEF: return 4;
	case GF_SG_FIELD_CODING_OUT: return 2;
	case GF_SG_FIELD_CODING_DYN: return 0;
	default:
		return 6;
	}
}

static GF_Err AudioDelay_get_field_index(GF_Node *n, u32 inField, u8 IndexMode, u32 *allField)
{
	switch(IndexMode) {
	case GF_SG_FIELD_CODING_IN:
		*allField = AudioDelay_In2All[inField];
		return GF_OK;
	case GF_SG_FIELD_CODING_DEF:
		*allField = AudioDelay_Def2All[inField];
		return GF_OK;
	case GF_SG_FIELD_CODING_OUT:
		*allField = AudioDelay_Out2All[inField];
		return GF_OK;
	default:
		return GF_BAD_PARAM;
	}
}
static GF_Err AudioDelay_get_field(GF_Node *node, GF_FieldInfo *info)
{
	switch (info->fieldIndex) {
	case 0:
		info->name = "addChildren";
		info->eventType = GF_SG_EVENT_IN;
		info->on_event_in = ((M_AudioDelay *)node)->on_addChildren;
		info->fieldType = GF_SG_VRML_MFNODE;
		info->NDTtype = NDT_SFAudioNode;
		info->far_ptr = & ((M_AudioDelay *)node)->addChildren;
		return GF_OK;
	case 1:
		info->name = "removeChildren";
		info->eventType = GF_SG_EVENT_IN;
		info->on_event_in = ((M_AudioDelay *)node)->on_removeChildren;
		info->fieldType = GF_SG_VRML_MFNODE;
		info->NDTtype = NDT_SFAudioNode;
		info->far_ptr = & ((M_AudioDelay *)node)->removeChildren;
		return GF_OK;
	case 2:
		info->name = "children";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_MFNODE;
		info->NDTtype = NDT_SFAudioNode;
		info->far_ptr = & ((M_AudioDelay *)node)->children;
		return GF_OK;
	case 3:
		info->name = "delay";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFTIME;
		info->far_ptr = & ((M_AudioDelay *) node)->delay;
		return GF_OK;
	case 4:
		info->name = "numChan";
		info->eventType = GF_SG_EVENT_FIELD;
		info->fieldType = GF_SG_VRML_SFINT32;
		info->far_ptr = & ((M_AudioDelay *) node)->numChan;
		return GF_OK;
	case 5:
		info->name = "phaseGroup";
		info->eventType = GF_SG_EVENT_FIELD;
		info->fieldType = GF_SG_VRML_MFINT32;
		info->far_ptr = & ((M_AudioDelay *) node)->phaseGroup;
		return GF_OK;
	default:
		return GF_BAD_PARAM;
	}
}

static Bool AudioDelay_get_aq_info(GF_Node *n, u32 FieldIndex, u8 *QType, u8 *AType, Fixed *b_min, Fixed *b_max, u32 *QT13_bits)
{
	switch (FieldIndex) {
	case 4:
		*AType = 0;
		*QType = 13;
		*QT13_bits = 8;
		*b_min = FLT2FIX(0);
		*b_max = FLT2FIX(255);
		return 1;
	case 5:
		*AType = 0;
		*QType = 13;
		*QT13_bits = 8;
		*b_min = FLT2FIX(0);
		*b_max = FLT2FIX(255);
		return 1;
	default:
		return 0;
	}
}



GF_Node *AudioDelay_Create()
{
	M_AudioDelay *p;
	GF_SAFEALLOC(p, sizeof(M_AudioDelay));
	if(!p) return NULL;
	gf_node_setup((GF_Node *)p, TAG_MPEG4_AudioDelay);
	gf_sg_vrml_parent_setup((GF_Node *) p);

#ifdef GF_NODE_USE_POINTERS
	((GF_Node *)p)->sgprivate->name = "AudioDelay";
	((GF_Node *)p)->sgprivate->node_del = AudioDelay_Del;
	((GF_Node *)p)->sgprivate->get_field_count = AudioDelay_get_field_count;
	((GF_Node *)p)->sgprivate->get_field = AudioDelay_get_field;
#endif


	/*default field values*/
	p->delay = 0;
	p->numChan = 1;
	return (GF_Node *)p;
}


/*
	AudioFX Node deletion
*/

static void AudioFX_Del(GF_Node *node)
{
	M_AudioFX *p = (M_AudioFX *) node;
	gf_sg_vrml_parent_reset((GF_Node *) p);	
	gf_sg_sfstring_del(p->orch);
	gf_sg_sfstring_del(p->score);
	gf_sg_mffloat_del(p->params);
	gf_sg_mfint32_del(p->phaseGroup);
	gf_node_free((GF_Node *) p);
}

static const u16 AudioFX_Def2All[] = { 2, 3, 4, 5, 6, 7};
static const u16 AudioFX_In2All[] = { 0, 1, 2, 3, 4, 5};
static const u16 AudioFX_Out2All[] = { 2, 3, 4, 5};
static const u16 AudioFX_Dyn2All[] = { 5};

static u32 AudioFX_get_field_count(GF_Node *node, u8 IndexMode)
{
	switch(IndexMode) {
	case GF_SG_FIELD_CODING_IN: return 6;
	case GF_SG_FIELD_CODING_DEF: return 6;
	case GF_SG_FIELD_CODING_OUT: return 4;
	case GF_SG_FIELD_CODING_DYN: return 1;
	default:
		return 8;
	}
}

static GF_Err AudioFX_get_field_index(GF_Node *n, u32 inField, u8 IndexMode, u32 *allField)
{
	switch(IndexMode) {
	case GF_SG_FIELD_CODING_IN:
		*allField = AudioFX_In2All[inField];
		return GF_OK;
	case GF_SG_FIELD_CODING_DEF:
		*allField = AudioFX_Def2All[inField];
		return GF_OK;
	case GF_SG_FIELD_CODING_OUT:
		*allField = AudioFX_Out2All[inField];
		return GF_OK;
	case GF_SG_FIELD_CODING_DYN:
		*allField = AudioFX_Dyn2All[inField];
		return GF_OK;
	default:
		return GF_BAD_PARAM;
	}
}
static GF_Err AudioFX_get_field(GF_Node *node, GF_FieldInfo *info)
{
	switch (info->fieldIndex) {
	case 0:
		info->name = "addChildren";
		info->eventType = GF_SG_EVENT_IN;
		info->on_event_in = ((M_AudioFX *)node)->on_addChildren;
		info->fieldType = GF_SG_VRML_MFNODE;
		info->NDTtype = NDT_SFAudioNode;
		info->far_ptr = & ((M_AudioFX *)node)->addChildren;
		return GF_OK;
	case 1:
		info->name = "removeChildren";
		info->eventType = GF_SG_EVENT_IN;
		info->on_event_in = ((M_AudioFX *)node)->on_removeChildren;
		info->fieldType = GF_SG_VRML_MFNODE;
		info->NDTtype = NDT_SFAudioNode;
		info->far_ptr = & ((M_AudioFX *)node)->removeChildren;
		return GF_OK;
	case 2:
		info->name = "children";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_MFNODE;
		info->NDTtype = NDT_SFAudioNode;
		info->far_ptr = & ((M_AudioFX *)node)->children;
		return GF_OK;
	case 3:
		info->name = "orch";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFSTRING;
		info->far_ptr = & ((M_AudioFX *) node)->orch;
		return GF_OK;
	case 4:
		info->name = "score";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFSTRING;
		info->far_ptr = & ((M_AudioFX *) node)->score;
		return GF_OK;
	case 5:
		info->name = "params";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_MFFLOAT;
		info->far_ptr = & ((M_AudioFX *) node)->params;
		return GF_OK;
	case 6:
		info->name = "numChan";
		info->eventType = GF_SG_EVENT_FIELD;
		info->fieldType = GF_SG_VRML_SFINT32;
		info->far_ptr = & ((M_AudioFX *) node)->numChan;
		return GF_OK;
	case 7:
		info->name = "phaseGroup";
		info->eventType = GF_SG_EVENT_FIELD;
		info->fieldType = GF_SG_VRML_MFINT32;
		info->far_ptr = & ((M_AudioFX *) node)->phaseGroup;
		return GF_OK;
	default:
		return GF_BAD_PARAM;
	}
}

static Bool AudioFX_get_aq_info(GF_Node *n, u32 FieldIndex, u8 *QType, u8 *AType, Fixed *b_min, Fixed *b_max, u32 *QT13_bits)
{
	switch (FieldIndex) {
	case 5:
		*AType = 7;
		*QType = 0;
		*b_min = FIX_MIN;
		*b_max = FIX_MAX;
		return 1;
	case 6:
		*AType = 0;
		*QType = 13;
		*QT13_bits = 8;
		*b_min = FLT2FIX(0);
		*b_max = FLT2FIX(255);
		return 1;
	case 7:
		*AType = 0;
		*QType = 13;
		*QT13_bits = 8;
		*b_min = FLT2FIX(0);
		*b_max = FLT2FIX(255);
		return 1;
	default:
		return 0;
	}
}



GF_Node *AudioFX_Create()
{
	M_AudioFX *p;
	GF_SAFEALLOC(p, sizeof(M_AudioFX));
	if(!p) return NULL;
	gf_node_setup((GF_Node *)p, TAG_MPEG4_AudioFX);
	gf_sg_vrml_parent_setup((GF_Node *) p);

#ifdef GF_NODE_USE_POINTERS
	((GF_Node *)p)->sgprivate->name = "AudioFX";
	((GF_Node *)p)->sgprivate->node_del = AudioFX_Del;
	((GF_Node *)p)->sgprivate->get_field_count = AudioFX_get_field_count;
	((GF_Node *)p)->sgprivate->get_field = AudioFX_get_field;
#endif


	/*default field values*/
	p->numChan = 1;
	return (GF_Node *)p;
}


/*
	AudioMix Node deletion
*/

static void AudioMix_Del(GF_Node *node)
{
	M_AudioMix *p = (M_AudioMix *) node;
	gf_sg_vrml_parent_reset((GF_Node *) p);	
	gf_sg_mffloat_del(p->matrix);
	gf_sg_mfint32_del(p->phaseGroup);
	gf_node_free((GF_Node *) p);
}

static const u16 AudioMix_Def2All[] = { 2, 3, 4, 5, 6};
static const u16 AudioMix_In2All[] = { 0, 1, 2, 3, 4};
static const u16 AudioMix_Out2All[] = { 2, 3, 4};
static const u16 AudioMix_Dyn2All[] = { 4};

static u32 AudioMix_get_field_count(GF_Node *node, u8 IndexMode)
{
	switch(IndexMode) {
	case GF_SG_FIELD_CODING_IN: return 5;
	case GF_SG_FIELD_CODING_DEF: return 5;
	case GF_SG_FIELD_CODING_OUT: return 3;
	case GF_SG_FIELD_CODING_DYN: return 1;
	default:
		return 7;
	}
}

static GF_Err AudioMix_get_field_index(GF_Node *n, u32 inField, u8 IndexMode, u32 *allField)
{
	switch(IndexMode) {
	case GF_SG_FIELD_CODING_IN:
		*allField = AudioMix_In2All[inField];
		return GF_OK;
	case GF_SG_FIELD_CODING_DEF:
		*allField = AudioMix_Def2All[inField];
		return GF_OK;
	case GF_SG_FIELD_CODING_OUT:
		*allField = AudioMix_Out2All[inField];
		return GF_OK;
	case GF_SG_FIELD_CODING_DYN:
		*allField = AudioMix_Dyn2All[inField];
		return GF_OK;
	default:
		return GF_BAD_PARAM;
	}
}
static GF_Err AudioMix_get_field(GF_Node *node, GF_FieldInfo *info)
{
	switch (info->fieldIndex) {
	case 0:
		info->name = "addChildren";
		info->eventType = GF_SG_EVENT_IN;
		info->on_event_in = ((M_AudioMix *)node)->on_addChildren;
		info->fieldType = GF_SG_VRML_MFNODE;
		info->NDTtype = NDT_SFAudioNode;
		info->far_ptr = & ((M_AudioMix *)node)->addChildren;
		return GF_OK;
	case 1:
		info->name = "removeChildren";
		info->eventType = GF_SG_EVENT_IN;
		info->on_event_in = ((M_AudioMix *)node)->on_removeChildren;
		info->fieldType = GF_SG_VRML_MFNODE;
		info->NDTtype = NDT_SFAudioNode;
		info->far_ptr = & ((M_AudioMix *)node)->removeChildren;
		return GF_OK;
	case 2:
		info->name = "children";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_MFNODE;
		info->NDTtype = NDT_SFAudioNode;
		info->far_ptr = & ((M_AudioMix *)node)->children;
		return GF_OK;
	case 3:
		info->name = "numInputs";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFINT32;
		info->far_ptr = & ((M_AudioMix *) node)->numInputs;
		return GF_OK;
	case 4:
		info->name = "matrix";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_MFFLOAT;
		info->far_ptr = & ((M_AudioMix *) node)->matrix;
		return GF_OK;
	case 5:
		info->name = "numChan";
		info->eventType = GF_SG_EVENT_FIELD;
		info->fieldType = GF_SG_VRML_SFINT32;
		info->far_ptr = & ((M_AudioMix *) node)->numChan;
		return GF_OK;
	case 6:
		info->name = "phaseGroup";
		info->eventType = GF_SG_EVENT_FIELD;
		info->fieldType = GF_SG_VRML_MFINT32;
		info->far_ptr = & ((M_AudioMix *) node)->phaseGroup;
		return GF_OK;
	default:
		return GF_BAD_PARAM;
	}
}

static Bool AudioMix_get_aq_info(GF_Node *n, u32 FieldIndex, u8 *QType, u8 *AType, Fixed *b_min, Fixed *b_max, u32 *QT13_bits)
{
	switch (FieldIndex) {
	case 3:
		*AType = 0;
		*QType = 13;
		*QT13_bits = 8;
		*b_min = FLT2FIX(1);
		*b_max = FLT2FIX(255);
		return 1;
	case 4:
		*AType = 7;
		*QType = 0;
		*b_min = FLT2FIX(0);
		*b_max = FLT2FIX(1);
		return 1;
	case 5:
		*AType = 0;
		*QType = 13;
		*QT13_bits = 8;
		*b_min = FLT2FIX(0);
		*b_max = FLT2FIX(255);
		return 1;
	case 6:
		*AType = 0;
		*QType = 13;
		*QT13_bits = 8;
		*b_min = FLT2FIX(0);
		*b_max = FLT2FIX(255);
		return 1;
	default:
		return 0;
	}
}



GF_Node *AudioMix_Create()
{
	M_AudioMix *p;
	GF_SAFEALLOC(p, sizeof(M_AudioMix));
	if(!p) return NULL;
	gf_node_setup((GF_Node *)p, TAG_MPEG4_AudioMix);
	gf_sg_vrml_parent_setup((GF_Node *) p);

#ifdef GF_NODE_USE_POINTERS
	((GF_Node *)p)->sgprivate->name = "AudioMix";
	((GF_Node *)p)->sgprivate->node_del = AudioMix_Del;
	((GF_Node *)p)->sgprivate->get_field_count = AudioMix_get_field_count;
	((GF_Node *)p)->sgprivate->get_field = AudioMix_get_field;
#endif


	/*default field values*/
	p->numInputs = 1;
	p->numChan = 1;
	return (GF_Node *)p;
}


/*
	AudioSource Node deletion
*/

static void AudioSource_Del(GF_Node *node)
{
	M_AudioSource *p = (M_AudioSource *) node;
	gf_sg_vrml_parent_reset((GF_Node *) p);	
	gf_sg_mfurl_del(p->url);
	gf_sg_mfint32_del(p->phaseGroup);
	gf_node_free((GF_Node *) p);
}

static const u16 AudioSource_Def2All[] = { 2, 3, 4, 5, 6, 7, 8, 9};
static const u16 AudioSource_In2All[] = { 0, 1, 2, 3, 4, 5, 6, 7};
static const u16 AudioSource_Out2All[] = { 2, 3, 4, 5, 6, 7};
static const u16 AudioSource_Dyn2All[] = { 4, 5};

static u32 AudioSource_get_field_count(GF_Node *node, u8 IndexMode)
{
	switch(IndexMode) {
	case GF_SG_FIELD_CODING_IN: return 8;
	case GF_SG_FIELD_CODING_DEF: return 8;
	case GF_SG_FIELD_CODING_OUT: return 6;
	case GF_SG_FIELD_CODING_DYN: return 2;
	default:
		return 10;
	}
}

static GF_Err AudioSource_get_field_index(GF_Node *n, u32 inField, u8 IndexMode, u32 *allField)
{
	switch(IndexMode) {
	case GF_SG_FIELD_CODING_IN:
		*allField = AudioSource_In2All[inField];
		return GF_OK;
	case GF_SG_FIELD_CODING_DEF:
		*allField = AudioSource_Def2All[inField];
		return GF_OK;
	case GF_SG_FIELD_CODING_OUT:
		*allField = AudioSource_Out2All[inField];
		return GF_OK;
	case GF_SG_FIELD_CODING_DYN:
		*allField = AudioSource_Dyn2All[inField];
		return GF_OK;
	default:
		return GF_BAD_PARAM;
	}
}
static GF_Err AudioSource_get_field(GF_Node *node, GF_FieldInfo *info)
{
	switch (info->fieldIndex) {
	case 0:
		info->name = "addChildren";
		info->eventType = GF_SG_EVENT_IN;
		info->on_event_in = ((M_AudioSource *)node)->on_addChildren;
		info->fieldType = GF_SG_VRML_MFNODE;
		info->NDTtype = NDT_SFAudioNode;
		info->far_ptr = & ((M_AudioSource *)node)->addChildren;
		return GF_OK;
	case 1:
		info->name = "removeChildren";
		info->eventType = GF_SG_EVENT_IN;
		info->on_event_in = ((M_AudioSource *)node)->on_removeChildren;
		info->fieldType = GF_SG_VRML_MFNODE;
		info->NDTtype = NDT_SFAudioNode;
		info->far_ptr = & ((M_AudioSource *)node)->removeChildren;
		return GF_OK;
	case 2:
		info->name = "children";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_MFNODE;
		info->NDTtype = NDT_SFAudioNode;
		info->far_ptr = & ((M_AudioSource *)node)->children;
		return GF_OK;
	case 3:
		info->name = "url";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_MFURL;
		info->far_ptr = & ((M_AudioSource *) node)->url;
		return GF_OK;
	case 4:
		info->name = "pitch";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFFLOAT;
		info->far_ptr = & ((M_AudioSource *) node)->pitch;
		return GF_OK;
	case 5:
		info->name = "speed";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFFLOAT;
		info->far_ptr = & ((M_AudioSource *) node)->speed;
		return GF_OK;
	case 6:
		info->name = "startTime";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFTIME;
		info->far_ptr = & ((M_AudioSource *) node)->startTime;
		return GF_OK;
	case 7:
		info->name = "stopTime";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFTIME;
		info->far_ptr = & ((M_AudioSource *) node)->stopTime;
		return GF_OK;
	case 8:
		info->name = "numChan";
		info->eventType = GF_SG_EVENT_FIELD;
		info->fieldType = GF_SG_VRML_SFINT32;
		info->far_ptr = & ((M_AudioSource *) node)->numChan;
		return GF_OK;
	case 9:
		info->name = "phaseGroup";
		info->eventType = GF_SG_EVENT_FIELD;
		info->fieldType = GF_SG_VRML_MFINT32;
		info->far_ptr = & ((M_AudioSource *) node)->phaseGroup;
		return GF_OK;
	default:
		return GF_BAD_PARAM;
	}
}

static Bool AudioSource_get_aq_info(GF_Node *n, u32 FieldIndex, u8 *QType, u8 *AType, Fixed *b_min, Fixed *b_max, u32 *QT13_bits)
{
	switch (FieldIndex) {
	case 4:
		*AType = 7;
		*QType = 0;
		*b_min = FLT2FIX(0);
		*b_max = FIX_MAX;
		return 1;
	case 5:
		*AType = 7;
		*QType = 0;
		*b_min = FLT2FIX(0);
		*b_max = FIX_MAX;
		return 1;
	case 8:
		*AType = 0;
		*QType = 13;
		*QT13_bits = 8;
		*b_min = FLT2FIX(0);
		*b_max = FLT2FIX(255);
		return 1;
	case 9:
		*AType = 0;
		*QType = 13;
		*QT13_bits = 8;
		*b_min = FLT2FIX(0);
		*b_max = FLT2FIX(255);
		return 1;
	default:
		return 0;
	}
}



GF_Node *AudioSource_Create()
{
	M_AudioSource *p;
	GF_SAFEALLOC(p, sizeof(M_AudioSource));
	if(!p) return NULL;
	gf_node_setup((GF_Node *)p, TAG_MPEG4_AudioSource);
	gf_sg_vrml_parent_setup((GF_Node *) p);

#ifdef GF_NODE_USE_POINTERS
	((GF_Node *)p)->sgprivate->name = "AudioSource";
	((GF_Node *)p)->sgprivate->node_del = AudioSource_Del;
	((GF_Node *)p)->sgprivate->get_field_count = AudioSource_get_field_count;
	((GF_Node *)p)->sgprivate->get_field = AudioSource_get_field;
#endif


	/*default field values*/
	p->pitch = FLT2FIX(1);
	p->speed = FLT2FIX(1);
	p->startTime = 0;
	p->stopTime = 0;
	p->numChan = 1;
	return (GF_Node *)p;
}


/*
	AudioSwitch Node deletion
*/

static void AudioSwitch_Del(GF_Node *node)
{
	M_AudioSwitch *p = (M_AudioSwitch *) node;
	gf_sg_vrml_parent_reset((GF_Node *) p);	
	gf_sg_mfint32_del(p->whichChoice);
	gf_sg_mfint32_del(p->phaseGroup);
	gf_node_free((GF_Node *) p);
}

static const u16 AudioSwitch_Def2All[] = { 2, 3, 4, 5};
static const u16 AudioSwitch_In2All[] = { 0, 1, 2, 3};
static const u16 AudioSwitch_Out2All[] = { 2, 3};

static u32 AudioSwitch_get_field_count(GF_Node *node, u8 IndexMode)
{
	switch(IndexMode) {
	case GF_SG_FIELD_CODING_IN: return 4;
	case GF_SG_FIELD_CODING_DEF: return 4;
	case GF_SG_FIELD_CODING_OUT: return 2;
	case GF_SG_FIELD_CODING_DYN: return 0;
	default:
		return 6;
	}
}

static GF_Err AudioSwitch_get_field_index(GF_Node *n, u32 inField, u8 IndexMode, u32 *allField)
{
	switch(IndexMode) {
	case GF_SG_FIELD_CODING_IN:
		*allField = AudioSwitch_In2All[inField];
		return GF_OK;
	case GF_SG_FIELD_CODING_DEF:
		*allField = AudioSwitch_Def2All[inField];
		return GF_OK;
	case GF_SG_FIELD_CODING_OUT:
		*allField = AudioSwitch_Out2All[inField];
		return GF_OK;
	default:
		return GF_BAD_PARAM;
	}
}
static GF_Err AudioSwitch_get_field(GF_Node *node, GF_FieldInfo *info)
{
	switch (info->fieldIndex) {
	case 0:
		info->name = "addChildren";
		info->eventType = GF_SG_EVENT_IN;
		info->on_event_in = ((M_AudioSwitch *)node)->on_addChildren;
		info->fieldType = GF_SG_VRML_MFNODE;
		info->NDTtype = NDT_SFAudioNode;
		info->far_ptr = & ((M_AudioSwitch *)node)->addChildren;
		return GF_OK;
	case 1:
		info->name = "removeChildren";
		info->eventType = GF_SG_EVENT_IN;
		info->on_event_in = ((M_AudioSwitch *)node)->on_removeChildren;
		info->fieldType = GF_SG_VRML_MFNODE;
		info->NDTtype = NDT_SFAudioNode;
		info->far_ptr = & ((M_AudioSwitch *)node)->removeChildren;
		return GF_OK;
	case 2:
		info->name = "children";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_MFNODE;
		info->NDTtype = NDT_SFAudioNode;
		info->far_ptr = & ((M_AudioSwitch *)node)->children;
		return GF_OK;
	case 3:
		info->name = "whichChoice";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_MFINT32;
		info->far_ptr = & ((M_AudioSwitch *) node)->whichChoice;
		return GF_OK;
	case 4:
		info->name = "numChan";
		info->eventType = GF_SG_EVENT_FIELD;
		info->fieldType = GF_SG_VRML_SFINT32;
		info->far_ptr = & ((M_AudioSwitch *) node)->numChan;
		return GF_OK;
	case 5:
		info->name = "phaseGroup";
		info->eventType = GF_SG_EVENT_FIELD;
		info->fieldType = GF_SG_VRML_MFINT32;
		info->far_ptr = & ((M_AudioSwitch *) node)->phaseGroup;
		return GF_OK;
	default:
		return GF_BAD_PARAM;
	}
}

static Bool AudioSwitch_get_aq_info(GF_Node *n, u32 FieldIndex, u8 *QType, u8 *AType, Fixed *b_min, Fixed *b_max, u32 *QT13_bits)
{
	switch (FieldIndex) {
	case 3:
		*AType = 0;
		*QType = 13;
		*QT13_bits = 1;
		*b_min = FLT2FIX(0);
		*b_max = FLT2FIX(1);
		return 1;
	case 4:
		*AType = 0;
		*QType = 13;
		*QT13_bits = 8;
		*b_min = FLT2FIX(0);
		*b_max = FLT2FIX(255);
		return 1;
	case 5:
		*AType = 0;
		*QType = 13;
		*QT13_bits = 8;
		*b_min = FLT2FIX(0);
		*b_max = FLT2FIX(255);
		return 1;
	default:
		return 0;
	}
}



GF_Node *AudioSwitch_Create()
{
	M_AudioSwitch *p;
	GF_SAFEALLOC(p, sizeof(M_AudioSwitch));
	if(!p) return NULL;
	gf_node_setup((GF_Node *)p, TAG_MPEG4_AudioSwitch);
	gf_sg_vrml_parent_setup((GF_Node *) p);

#ifdef GF_NODE_USE_POINTERS
	((GF_Node *)p)->sgprivate->name = "AudioSwitch";
	((GF_Node *)p)->sgprivate->node_del = AudioSwitch_Del;
	((GF_Node *)p)->sgprivate->get_field_count = AudioSwitch_get_field_count;
	((GF_Node *)p)->sgprivate->get_field = AudioSwitch_get_field;
#endif


	/*default field values*/
	p->numChan = 1;
	return (GF_Node *)p;
}


/*
	Background Node deletion
*/

static void Background_Del(GF_Node *node)
{
	M_Background *p = (M_Background *) node;
	gf_sg_mffloat_del(p->groundAngle);
	gf_sg_mfcolor_del(p->groundColor);
	gf_sg_mfurl_del(p->backUrl);
	gf_sg_mfurl_del(p->bottomUrl);
	gf_sg_mfurl_del(p->frontUrl);
	gf_sg_mfurl_del(p->leftUrl);
	gf_sg_mfurl_del(p->rightUrl);
	gf_sg_mfurl_del(p->topUrl);
	gf_sg_mffloat_del(p->skyAngle);
	gf_sg_mfcolor_del(p->skyColor);
	gf_node_free((GF_Node *) p);
}

static const u16 Background_Def2All[] = { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
static const u16 Background_In2All[] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
static const u16 Background_Out2All[] = { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11};
static const u16 Background_Dyn2All[] = { 1, 2, 9, 10};

static u32 Background_get_field_count(GF_Node *node, u8 IndexMode)
{
	switch(IndexMode) {
	case GF_SG_FIELD_CODING_IN: return 11;
	case GF_SG_FIELD_CODING_DEF: return 10;
	case GF_SG_FIELD_CODING_OUT: return 11;
	case GF_SG_FIELD_CODING_DYN: return 4;
	default:
		return 12;
	}
}

static GF_Err Background_get_field_index(GF_Node *n, u32 inField, u8 IndexMode, u32 *allField)
{
	switch(IndexMode) {
	case GF_SG_FIELD_CODING_IN:
		*allField = Background_In2All[inField];
		return GF_OK;
	case GF_SG_FIELD_CODING_DEF:
		*allField = Background_Def2All[inField];
		return GF_OK;
	case GF_SG_FIELD_CODING_OUT:
		*allField = Background_Out2All[inField];
		return GF_OK;
	case GF_SG_FIELD_CODING_DYN:
		*allField = Background_Dyn2All[inField];
		return GF_OK;
	default:
		return GF_BAD_PARAM;
	}
}
static GF_Err Background_get_field(GF_Node *node, GF_FieldInfo *info)
{
	switch (info->fieldIndex) {
	case 0:
		info->name = "set_bind";
		info->eventType = GF_SG_EVENT_IN;
		info->on_event_in = ((M_Background *)node)->on_set_bind;
		info->fieldType = GF_SG_VRML_SFBOOL;
		info->far_ptr = & ((M_Background *) node)->set_bind;
		return GF_OK;
	case 1:
		info->name = "groundAngle";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_MFFLOAT;
		info->far_ptr = & ((M_Background *) node)->groundAngle;
		return GF_OK;
	case 2:
		info->name = "groundColor";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_MFCOLOR;
		info->far_ptr = & ((M_Background *) node)->groundColor;
		return GF_OK;
	case 3:
		info->name = "backUrl";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_MFURL;
		info->far_ptr = & ((M_Background *) node)->backUrl;
		return GF_OK;
	case 4:
		info->name = "bottomUrl";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_MFURL;
		info->far_ptr = & ((M_Background *) node)->bottomUrl;
		return GF_OK;
	case 5:
		info->name = "frontUrl";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_MFURL;
		info->far_ptr = & ((M_Background *) node)->frontUrl;
		return GF_OK;
	case 6:
		info->name = "leftUrl";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_MFURL;
		info->far_ptr = & ((M_Background *) node)->leftUrl;
		return GF_OK;
	case 7:
		info->name = "rightUrl";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_MFURL;
		info->far_ptr = & ((M_Background *) node)->rightUrl;
		return GF_OK;
	case 8:
		info->name = "topUrl";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_MFURL;
		info->far_ptr = & ((M_Background *) node)->topUrl;
		return GF_OK;
	case 9:
		info->name = "skyAngle";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_MFFLOAT;
		info->far_ptr = & ((M_Background *) node)->skyAngle;
		return GF_OK;
	case 10:
		info->name = "skyColor";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_MFCOLOR;
		info->far_ptr = & ((M_Background *) node)->skyColor;
		return GF_OK;
	case 11:
		info->name = "isBound";
		info->eventType = GF_SG_EVENT_OUT;
		info->fieldType = GF_SG_VRML_SFBOOL;
		info->far_ptr = & ((M_Background *) node)->isBound;
		return GF_OK;
	default:
		return GF_BAD_PARAM;
	}
}

static Bool Background_get_aq_info(GF_Node *n, u32 FieldIndex, u8 *QType, u8 *AType, Fixed *b_min, Fixed *b_max, u32 *QT13_bits)
{
	switch (FieldIndex) {
	case 1:
		*AType = 8;
		*QType = 6;
		*b_min = FLT2FIX(0);
		*b_max = FLT2FIX(1.5707963);
		return 1;
	case 2:
		*AType = 4;
		*QType = 4;
		*b_min = FLT2FIX(0);
		*b_max = FLT2FIX(1);
		return 1;
	case 9:
		*AType = 8;
		*QType = 6;
		*b_min = FLT2FIX(0);
		*b_max = FLT2FIX(3.14159265);
		return 1;
	case 10:
		*AType = 4;
		*QType = 4;
		*b_min = FLT2FIX(0);
		*b_max = FLT2FIX(1);
		return 1;
	default:
		return 0;
	}
}



GF_Node *Background_Create()
{
	M_Background *p;
	GF_SAFEALLOC(p, sizeof(M_Background));
	if(!p) return NULL;
	gf_node_setup((GF_Node *)p, TAG_MPEG4_Background);

#ifdef GF_NODE_USE_POINTERS
	((GF_Node *)p)->sgprivate->name = "Background";
	((GF_Node *)p)->sgprivate->node_del = Background_Del;
	((GF_Node *)p)->sgprivate->get_field_count = Background_get_field_count;
	((GF_Node *)p)->sgprivate->get_field = Background_get_field;
#endif


	/*default field values*/
	p->skyColor.vals = malloc(sizeof(SFColor)*1);
	p->skyColor.count = 1;
	p->skyColor.vals[0].red = FLT2FIX(0);
	p->skyColor.vals[0].green = FLT2FIX(0);
	p->skyColor.vals[0].blue = FLT2FIX(0);
	return (GF_Node *)p;
}


/*
	Background2D Node deletion
*/

static void Background2D_Del(GF_Node *node)
{
	M_Background2D *p = (M_Background2D *) node;
	gf_sg_mfurl_del(p->url);
	gf_node_free((GF_Node *) p);
}

static const u16 Background2D_Def2All[] = { 1, 2};
static const u16 Background2D_In2All[] = { 0, 1, 2};
static const u16 Background2D_Out2All[] = { 1, 2, 3};
static const u16 Background2D_Dyn2All[] = { 1};

static u32 Background2D_get_field_count(GF_Node *node, u8 IndexMode)
{
	switch(IndexMode) {
	case GF_SG_FIELD_CODING_IN: return 3;
	case GF_SG_FIELD_CODING_DEF: return 2;
	case GF_SG_FIELD_CODING_OUT: return 3;
	case GF_SG_FIELD_CODING_DYN: return 1;
	default:
		return 4;
	}
}

static GF_Err Background2D_get_field_index(GF_Node *n, u32 inField, u8 IndexMode, u32 *allField)
{
	switch(IndexMode) {
	case GF_SG_FIELD_CODING_IN:
		*allField = Background2D_In2All[inField];
		return GF_OK;
	case GF_SG_FIELD_CODING_DEF:
		*allField = Background2D_Def2All[inField];
		return GF_OK;
	case GF_SG_FIELD_CODING_OUT:
		*allField = Background2D_Out2All[inField];
		return GF_OK;
	case GF_SG_FIELD_CODING_DYN:
		*allField = Background2D_Dyn2All[inField];
		return GF_OK;
	default:
		return GF_BAD_PARAM;
	}
}
static GF_Err Background2D_get_field(GF_Node *node, GF_FieldInfo *info)
{
	switch (info->fieldIndex) {
	case 0:
		info->name = "set_bind";
		info->eventType = GF_SG_EVENT_IN;
		info->on_event_in = ((M_Background2D *)node)->on_set_bind;
		info->fieldType = GF_SG_VRML_SFBOOL;
		info->far_ptr = & ((M_Background2D *) node)->set_bind;
		return GF_OK;
	case 1:
		info->name = "backColor";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFCOLOR;
		info->far_ptr = & ((M_Background2D *) node)->backColor;
		return GF_OK;
	case 2:
		info->name = "url";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_MFURL;
		info->far_ptr = & ((M_Background2D *) node)->url;
		return GF_OK;
	case 3:
		info->name = "isBound";
		info->eventType = GF_SG_EVENT_OUT;
		info->fieldType = GF_SG_VRML_SFBOOL;
		info->far_ptr = & ((M_Background2D *) node)->isBound;
		return GF_OK;
	default:
		return GF_BAD_PARAM;
	}
}

static Bool Background2D_get_aq_info(GF_Node *n, u32 FieldIndex, u8 *QType, u8 *AType, Fixed *b_min, Fixed *b_max, u32 *QT13_bits)
{
	switch (FieldIndex) {
	case 1:
		*AType = 4;
		*QType = 4;
		*b_min = FLT2FIX(0);
		*b_max = FLT2FIX(1);
		return 1;
	default:
		return 0;
	}
}



GF_Node *Background2D_Create()
{
	M_Background2D *p;
	GF_SAFEALLOC(p, sizeof(M_Background2D));
	if(!p) return NULL;
	gf_node_setup((GF_Node *)p, TAG_MPEG4_Background2D);

#ifdef GF_NODE_USE_POINTERS
	((GF_Node *)p)->sgprivate->name = "Background2D";
	((GF_Node *)p)->sgprivate->node_del = Background2D_Del;
	((GF_Node *)p)->sgprivate->get_field_count = Background2D_get_field_count;
	((GF_Node *)p)->sgprivate->get_field = Background2D_get_field;
#endif


	/*default field values*/
	p->backColor.red = FLT2FIX(0);
	p->backColor.green = FLT2FIX(0);
	p->backColor.blue = FLT2FIX(0);
	return (GF_Node *)p;
}


/*
	Billboard Node deletion
*/

static void Billboard_Del(GF_Node *node)
{
	M_Billboard *p = (M_Billboard *) node;
	gf_sg_vrml_parent_reset((GF_Node *) p);	
	gf_node_free((GF_Node *) p);
}

static const u16 Billboard_Def2All[] = { 2, 3};
static const u16 Billboard_In2All[] = { 0, 1, 2, 3};
static const u16 Billboard_Out2All[] = { 2, 3};
static const u16 Billboard_Dyn2All[] = { 3};

static u32 Billboard_get_field_count(GF_Node *node, u8 IndexMode)
{
	switch(IndexMode) {
	case GF_SG_FIELD_CODING_IN: return 4;
	case GF_SG_FIELD_CODING_DEF: return 2;
	case GF_SG_FIELD_CODING_OUT: return 2;
	case GF_SG_FIELD_CODING_DYN: return 1;
	default:
		return 4;
	}
}

static GF_Err Billboard_get_field_index(GF_Node *n, u32 inField, u8 IndexMode, u32 *allField)
{
	switch(IndexMode) {
	case GF_SG_FIELD_CODING_IN:
		*allField = Billboard_In2All[inField];
		return GF_OK;
	case GF_SG_FIELD_CODING_DEF:
		*allField = Billboard_Def2All[inField];
		return GF_OK;
	case GF_SG_FIELD_CODING_OUT:
		*allField = Billboard_Out2All[inField];
		return GF_OK;
	case GF_SG_FIELD_CODING_DYN:
		*allField = Billboard_Dyn2All[inField];
		return GF_OK;
	default:
		return GF_BAD_PARAM;
	}
}
static GF_Err Billboard_get_field(GF_Node *node, GF_FieldInfo *info)
{
	switch (info->fieldIndex) {
	case 0:
		info->name = "addChildren";
		info->eventType = GF_SG_EVENT_IN;
		info->on_event_in = ((M_Billboard *)node)->on_addChildren;
		info->fieldType = GF_SG_VRML_MFNODE;
		info->NDTtype = NDT_SF3DNode;
		info->far_ptr = & ((M_Billboard *)node)->addChildren;
		return GF_OK;
	case 1:
		info->name = "removeChildren";
		info->eventType = GF_SG_EVENT_IN;
		info->on_event_in = ((M_Billboard *)node)->on_removeChildren;
		info->fieldType = GF_SG_VRML_MFNODE;
		info->NDTtype = NDT_SF3DNode;
		info->far_ptr = & ((M_Billboard *)node)->removeChildren;
		return GF_OK;
	case 2:
		info->name = "children";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_MFNODE;
		info->NDTtype = NDT_SF3DNode;
		info->far_ptr = & ((M_Billboard *)node)->children;
		return GF_OK;
	case 3:
		info->name = "axisOfRotation";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFVEC3F;
		info->far_ptr = & ((M_Billboard *) node)->axisOfRotation;
		return GF_OK;
	default:
		return GF_BAD_PARAM;
	}
}

static Bool Billboard_get_aq_info(GF_Node *n, u32 FieldIndex, u8 *QType, u8 *AType, Fixed *b_min, Fixed *b_max, u32 *QT13_bits)
{
	switch (FieldIndex) {
	case 3:
		*AType = 9;
		*QType = 9;
		return 1;
	default:
		return 0;
	}
}



GF_Node *Billboard_Create()
{
	M_Billboard *p;
	GF_SAFEALLOC(p, sizeof(M_Billboard));
	if(!p) return NULL;
	gf_node_setup((GF_Node *)p, TAG_MPEG4_Billboard);
	gf_sg_vrml_parent_setup((GF_Node *) p);

#ifdef GF_NODE_USE_POINTERS
	((GF_Node *)p)->sgprivate->name = "Billboard";
	((GF_Node *)p)->sgprivate->node_del = Billboard_Del;
	((GF_Node *)p)->sgprivate->get_field_count = Billboard_get_field_count;
	((GF_Node *)p)->sgprivate->get_field = Billboard_get_field;
#endif


	/*default field values*/
	p->axisOfRotation.x = FLT2FIX(0);
	p->axisOfRotation.y = FLT2FIX(1);
	p->axisOfRotation.z = FLT2FIX(0);
	return (GF_Node *)p;
}


/*
	Bitmap Node deletion
*/

static void Bitmap_Del(GF_Node *node)
{
	M_Bitmap *p = (M_Bitmap *) node;
	gf_node_free((GF_Node *) p);
}

static const u16 Bitmap_Def2All[] = { 0};
static const u16 Bitmap_In2All[] = { 0};
static const u16 Bitmap_Out2All[] = { 0};
static const u16 Bitmap_Dyn2All[] = { 0};

static u32 Bitmap_get_field_count(GF_Node *node, u8 IndexMode)
{
	switch(IndexMode) {
	case GF_SG_FIELD_CODING_IN: return 1;
	case GF_SG_FIELD_CODING_DEF: return 1;
	case GF_SG_FIELD_CODING_OUT: return 1;
	case GF_SG_FIELD_CODING_DYN: return 1;
	default:
		return 1;
	}
}

static GF_Err Bitmap_get_field_index(GF_Node *n, u32 inField, u8 IndexMode, u32 *allField)
{
	switch(IndexMode) {
	case GF_SG_FIELD_CODING_IN:
		*allField = Bitmap_In2All[inField];
		return GF_OK;
	case GF_SG_FIELD_CODING_DEF:
		*allField = Bitmap_Def2All[inField];
		return GF_OK;
	case GF_SG_FIELD_CODING_OUT:
		*allField = Bitmap_Out2All[inField];
		return GF_OK;
	case GF_SG_FIELD_CODING_DYN:
		*allField = Bitmap_Dyn2All[inField];
		return GF_OK;
	default:
		return GF_BAD_PARAM;
	}
}
static GF_Err Bitmap_get_field(GF_Node *node, GF_FieldInfo *info)
{
	switch (info->fieldIndex) {
	case 0:
		info->name = "scale";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFVEC2F;
		info->far_ptr = & ((M_Bitmap *) node)->scale;
		return GF_OK;
	default:
		return GF_BAD_PARAM;
	}
}

static Bool Bitmap_get_aq_info(GF_Node *n, u32 FieldIndex, u8 *QType, u8 *AType, Fixed *b_min, Fixed *b_max, u32 *QT13_bits)
{
	switch (FieldIndex) {
	case 0:
		*AType = 12;
		*QType = 12;
		*b_min = FLT2FIX(-1);
		*b_max = FIX_MAX;
		return 1;
	default:
		return 0;
	}
}



GF_Node *Bitmap_Create()
{
	M_Bitmap *p;
	GF_SAFEALLOC(p, sizeof(M_Bitmap));
	if(!p) return NULL;
	gf_node_setup((GF_Node *)p, TAG_MPEG4_Bitmap);

#ifdef GF_NODE_USE_POINTERS
	((GF_Node *)p)->sgprivate->name = "Bitmap";
	((GF_Node *)p)->sgprivate->node_del = Bitmap_Del;
	((GF_Node *)p)->sgprivate->get_field_count = Bitmap_get_field_count;
	((GF_Node *)p)->sgprivate->get_field = Bitmap_get_field;
#endif


	/*default field values*/
	p->scale.x = FLT2FIX(-1);
	p->scale.y = FLT2FIX(-1);
	return (GF_Node *)p;
}


/*
	Box Node deletion
*/

static void Box_Del(GF_Node *node)
{
	M_Box *p = (M_Box *) node;
	gf_node_free((GF_Node *) p);
}

static const u16 Box_Def2All[] = { 0};

static u32 Box_get_field_count(GF_Node *node, u8 IndexMode)
{
	switch(IndexMode) {
	case GF_SG_FIELD_CODING_IN: return 0;
	case GF_SG_FIELD_CODING_DEF: return 1;
	case GF_SG_FIELD_CODING_OUT: return 0;
	case GF_SG_FIELD_CODING_DYN: return 0;
	default:
		return 1;
	}
}

static GF_Err Box_get_field_index(GF_Node *n, u32 inField, u8 IndexMode, u32 *allField)
{
	switch(IndexMode) {
	case GF_SG_FIELD_CODING_DEF:
		*allField = Box_Def2All[inField];
		return GF_OK;
	default:
		return GF_BAD_PARAM;
	}
}
static GF_Err Box_get_field(GF_Node *node, GF_FieldInfo *info)
{
	switch (info->fieldIndex) {
	case 0:
		info->name = "size";
		info->eventType = GF_SG_EVENT_FIELD;
		info->fieldType = GF_SG_VRML_SFVEC3F;
		info->far_ptr = & ((M_Box *) node)->size;
		return GF_OK;
	default:
		return GF_BAD_PARAM;
	}
}

static Bool Box_get_aq_info(GF_Node *n, u32 FieldIndex, u8 *QType, u8 *AType, Fixed *b_min, Fixed *b_max, u32 *QT13_bits)
{
	switch (FieldIndex) {
	case 0:
		*AType = 0;
		*QType = 11;
		*b_min = FLT2FIX(0);
		*b_max = FIX_MAX;
		return 1;
	default:
		return 0;
	}
}



GF_Node *Box_Create()
{
	M_Box *p;
	GF_SAFEALLOC(p, sizeof(M_Box));
	if(!p) return NULL;
	gf_node_setup((GF_Node *)p, TAG_MPEG4_Box);

#ifdef GF_NODE_USE_POINTERS
	((GF_Node *)p)->sgprivate->name = "Box";
	((GF_Node *)p)->sgprivate->node_del = Box_Del;
	((GF_Node *)p)->sgprivate->get_field_count = Box_get_field_count;
	((GF_Node *)p)->sgprivate->get_field = Box_get_field;
#endif


	/*default field values*/
	p->size.x = FLT2FIX(2);
	p->size.y = FLT2FIX(2);
	p->size.z = FLT2FIX(2);
	return (GF_Node *)p;
}


/*
	Circle Node deletion
*/

static void Circle_Del(GF_Node *node)
{
	M_Circle *p = (M_Circle *) node;
	gf_node_free((GF_Node *) p);
}

static const u16 Circle_Def2All[] = { 0};
static const u16 Circle_In2All[] = { 0};
static const u16 Circle_Out2All[] = { 0};
static const u16 Circle_Dyn2All[] = { 0};

static u32 Circle_get_field_count(GF_Node *node, u8 IndexMode)
{
	switch(IndexMode) {
	case GF_SG_FIELD_CODING_IN: return 1;
	case GF_SG_FIELD_CODING_DEF: return 1;
	case GF_SG_FIELD_CODING_OUT: return 1;
	case GF_SG_FIELD_CODING_DYN: return 1;
	default:
		return 1;
	}
}

static GF_Err Circle_get_field_index(GF_Node *n, u32 inField, u8 IndexMode, u32 *allField)
{
	switch(IndexMode) {
	case GF_SG_FIELD_CODING_IN:
		*allField = Circle_In2All[inField];
		return GF_OK;
	case GF_SG_FIELD_CODING_DEF:
		*allField = Circle_Def2All[inField];
		return GF_OK;
	case GF_SG_FIELD_CODING_OUT:
		*allField = Circle_Out2All[inField];
		return GF_OK;
	case GF_SG_FIELD_CODING_DYN:
		*allField = Circle_Dyn2All[inField];
		return GF_OK;
	default:
		return GF_BAD_PARAM;
	}
}
static GF_Err Circle_get_field(GF_Node *node, GF_FieldInfo *info)
{
	switch (info->fieldIndex) {
	case 0:
		info->name = "radius";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFFLOAT;
		info->far_ptr = & ((M_Circle *) node)->radius;
		return GF_OK;
	default:
		return GF_BAD_PARAM;
	}
}

static Bool Circle_get_aq_info(GF_Node *n, u32 FieldIndex, u8 *QType, u8 *AType, Fixed *b_min, Fixed *b_max, u32 *QT13_bits)
{
	switch (FieldIndex) {
	case 0:
		*AType = 7;
		*QType = 12;
		*b_min = FLT2FIX(0);
		*b_max = FIX_MAX;
		return 1;
	default:
		return 0;
	}
}



GF_Node *Circle_Create()
{
	M_Circle *p;
	GF_SAFEALLOC(p, sizeof(M_Circle));
	if(!p) return NULL;
	gf_node_setup((GF_Node *)p, TAG_MPEG4_Circle);

#ifdef GF_NODE_USE_POINTERS
	((GF_Node *)p)->sgprivate->name = "Circle";
	((GF_Node *)p)->sgprivate->node_del = Circle_Del;
	((GF_Node *)p)->sgprivate->get_field_count = Circle_get_field_count;
	((GF_Node *)p)->sgprivate->get_field = Circle_get_field;
#endif


	/*default field values*/
	p->radius = FLT2FIX(1);
	return (GF_Node *)p;
}


/*
	Collision Node deletion
*/

static void Collision_Del(GF_Node *node)
{
	M_Collision *p = (M_Collision *) node;
	gf_sg_vrml_parent_reset((GF_Node *) p);	
	gf_node_unregister((GF_Node *) p->proxy, (GF_Node *) p);	
	gf_node_free((GF_Node *) p);
}

static const u16 Collision_Def2All[] = { 2, 3, 4};
static const u16 Collision_In2All[] = { 0, 1, 2, 3};
static const u16 Collision_Out2All[] = { 2, 3, 5};

static u32 Collision_get_field_count(GF_Node *node, u8 IndexMode)
{
	switch(IndexMode) {
	case GF_SG_FIELD_CODING_IN: return 4;
	case GF_SG_FIELD_CODING_DEF: return 3;
	case GF_SG_FIELD_CODING_OUT: return 3;
	case GF_SG_FIELD_CODING_DYN: return 0;
	default:
		return 6;
	}
}

static GF_Err Collision_get_field_index(GF_Node *n, u32 inField, u8 IndexMode, u32 *allField)
{
	switch(IndexMode) {
	case GF_SG_FIELD_CODING_IN:
		*allField = Collision_In2All[inField];
		return GF_OK;
	case GF_SG_FIELD_CODING_DEF:
		*allField = Collision_Def2All[inField];
		return GF_OK;
	case GF_SG_FIELD_CODING_OUT:
		*allField = Collision_Out2All[inField];
		return GF_OK;
	default:
		return GF_BAD_PARAM;
	}
}
static GF_Err Collision_get_field(GF_Node *node, GF_FieldInfo *info)
{
	switch (info->fieldIndex) {
	case 0:
		info->name = "addChildren";
		info->eventType = GF_SG_EVENT_IN;
		info->on_event_in = ((M_Collision *)node)->on_addChildren;
		info->fieldType = GF_SG_VRML_MFNODE;
		info->NDTtype = NDT_SF3DNode;
		info->far_ptr = & ((M_Collision *)node)->addChildren;
		return GF_OK;
	case 1:
		info->name = "removeChildren";
		info->eventType = GF_SG_EVENT_IN;
		info->on_event_in = ((M_Collision *)node)->on_removeChildren;
		info->fieldType = GF_SG_VRML_MFNODE;
		info->NDTtype = NDT_SF3DNode;
		info->far_ptr = & ((M_Collision *)node)->removeChildren;
		return GF_OK;
	case 2:
		info->name = "children";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_MFNODE;
		info->NDTtype = NDT_SF3DNode;
		info->far_ptr = & ((M_Collision *)node)->children;
		return GF_OK;
	case 3:
		info->name = "collide";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFBOOL;
		info->far_ptr = & ((M_Collision *) node)->collide;
		return GF_OK;
	case 4:
		info->name = "proxy";
		info->eventType = GF_SG_EVENT_FIELD;
		info->fieldType = GF_SG_VRML_SFNODE;
		info->NDTtype = NDT_SF3DNode;
		info->far_ptr = & ((M_Collision *)node)->proxy;
		return GF_OK;
	case 5:
		info->name = "collideTime";
		info->eventType = GF_SG_EVENT_OUT;
		info->fieldType = GF_SG_VRML_SFTIME;
		info->far_ptr = & ((M_Collision *) node)->collideTime;
		return GF_OK;
	default:
		return GF_BAD_PARAM;
	}
}

static Bool Collision_get_aq_info(GF_Node *n, u32 FieldIndex, u8 *QType, u8 *AType, Fixed *b_min, Fixed *b_max, u32 *QT13_bits)
{
	switch (FieldIndex) {
	default:
		return 0;
	}
}



GF_Node *Collision_Create()
{
	M_Collision *p;
	GF_SAFEALLOC(p, sizeof(M_Collision));
	if(!p) return NULL;
	gf_node_setup((GF_Node *)p, TAG_MPEG4_Collision);
	gf_sg_vrml_parent_setup((GF_Node *) p);

#ifdef GF_NODE_USE_POINTERS
	((GF_Node *)p)->sgprivate->name = "Collision";
	((GF_Node *)p)->sgprivate->node_del = Collision_Del;
	((GF_Node *)p)->sgprivate->get_field_count = Collision_get_field_count;
	((GF_Node *)p)->sgprivate->get_field = Collision_get_field;
#endif


	/*default field values*/
	p->collide = 1;
	return (GF_Node *)p;
}


/*
	Color Node deletion
*/

static void Color_Del(GF_Node *node)
{
	M_Color *p = (M_Color *) node;
	gf_sg_mfcolor_del(p->color);
	gf_node_free((GF_Node *) p);
}

static const u16 Color_Def2All[] = { 0};
static const u16 Color_In2All[] = { 0};
static const u16 Color_Out2All[] = { 0};
static const u16 Color_Dyn2All[] = { 0};

static u32 Color_get_field_count(GF_Node *node, u8 IndexMode)
{
	switch(IndexMode) {
	case GF_SG_FIELD_CODING_IN: return 1;
	case GF_SG_FIELD_CODING_DEF: return 1;
	case GF_SG_FIELD_CODING_OUT: return 1;
	case GF_SG_FIELD_CODING_DYN: return 1;
	default:
		return 1;
	}
}

static GF_Err Color_get_field_index(GF_Node *n, u32 inField, u8 IndexMode, u32 *allField)
{
	switch(IndexMode) {
	case GF_SG_FIELD_CODING_IN:
		*allField = Color_In2All[inField];
		return GF_OK;
	case GF_SG_FIELD_CODING_DEF:
		*allField = Color_Def2All[inField];
		return GF_OK;
	case GF_SG_FIELD_CODING_OUT:
		*allField = Color_Out2All[inField];
		return GF_OK;
	case GF_SG_FIELD_CODING_DYN:
		*allField = Color_Dyn2All[inField];
		return GF_OK;
	default:
		return GF_BAD_PARAM;
	}
}
static GF_Err Color_get_field(GF_Node *node, GF_FieldInfo *info)
{
	switch (info->fieldIndex) {
	case 0:
		info->name = "color";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_MFCOLOR;
		info->far_ptr = & ((M_Color *) node)->color;
		return GF_OK;
	default:
		return GF_BAD_PARAM;
	}
}

static Bool Color_get_aq_info(GF_Node *n, u32 FieldIndex, u8 *QType, u8 *AType, Fixed *b_min, Fixed *b_max, u32 *QT13_bits)
{
	switch (FieldIndex) {
	case 0:
		*AType = 4;
		*QType = 4;
		*b_min = FLT2FIX(0);
		*b_max = FLT2FIX(1);
		return 1;
	default:
		return 0;
	}
}



GF_Node *Color_Create()
{
	M_Color *p;
	GF_SAFEALLOC(p, sizeof(M_Color));
	if(!p) return NULL;
	gf_node_setup((GF_Node *)p, TAG_MPEG4_Color);

#ifdef GF_NODE_USE_POINTERS
	((GF_Node *)p)->sgprivate->name = "Color";
	((GF_Node *)p)->sgprivate->node_del = Color_Del;
	((GF_Node *)p)->sgprivate->get_field_count = Color_get_field_count;
	((GF_Node *)p)->sgprivate->get_field = Color_get_field;
#endif


	/*default field values*/
	return (GF_Node *)p;
}


/*
	ColorInterpolator Node deletion
*/

static void ColorInterpolator_Del(GF_Node *node)
{
	M_ColorInterpolator *p = (M_ColorInterpolator *) node;
	gf_sg_mffloat_del(p->key);
	gf_sg_mfcolor_del(p->keyValue);
	gf_node_free((GF_Node *) p);
}

static const u16 ColorInterpolator_Def2All[] = { 1, 2};
static const u16 ColorInterpolator_In2All[] = { 0, 1, 2};
static const u16 ColorInterpolator_Out2All[] = { 1, 2, 3};

static u32 ColorInterpolator_get_field_count(GF_Node *node, u8 IndexMode)
{
	switch(IndexMode) {
	case GF_SG_FIELD_CODING_IN: return 3;
	case GF_SG_FIELD_CODING_DEF: return 2;
	case GF_SG_FIELD_CODING_OUT: return 3;
	case GF_SG_FIELD_CODING_DYN: return 0;
	default:
		return 4;
	}
}

static GF_Err ColorInterpolator_get_field_index(GF_Node *n, u32 inField, u8 IndexMode, u32 *allField)
{
	switch(IndexMode) {
	case GF_SG_FIELD_CODING_IN:
		*allField = ColorInterpolator_In2All[inField];
		return GF_OK;
	case GF_SG_FIELD_CODING_DEF:
		*allField = ColorInterpolator_Def2All[inField];
		return GF_OK;
	case GF_SG_FIELD_CODING_OUT:
		*allField = ColorInterpolator_Out2All[inField];
		return GF_OK;
	default:
		return GF_BAD_PARAM;
	}
}
static GF_Err ColorInterpolator_get_field(GF_Node *node, GF_FieldInfo *info)
{
	switch (info->fieldIndex) {
	case 0:
		info->name = "set_fraction";
		info->eventType = GF_SG_EVENT_IN;
		info->on_event_in = ((M_ColorInterpolator *)node)->on_set_fraction;
		info->fieldType = GF_SG_VRML_SFFLOAT;
		info->far_ptr = & ((M_ColorInterpolator *) node)->set_fraction;
		return GF_OK;
	case 1:
		info->name = "key";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_MFFLOAT;
		info->far_ptr = & ((M_ColorInterpolator *) node)->key;
		return GF_OK;
	case 2:
		info->name = "keyValue";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_MFCOLOR;
		info->far_ptr = & ((M_ColorInterpolator *) node)->keyValue;
		return GF_OK;
	case 3:
		info->name = "value_changed";
		info->eventType = GF_SG_EVENT_OUT;
		info->fieldType = GF_SG_VRML_SFCOLOR;
		info->far_ptr = & ((M_ColorInterpolator *) node)->value_changed;
		return GF_OK;
	default:
		return GF_BAD_PARAM;
	}
}

static Bool ColorInterpolator_get_aq_info(GF_Node *n, u32 FieldIndex, u8 *QType, u8 *AType, Fixed *b_min, Fixed *b_max, u32 *QT13_bits)
{
	switch (FieldIndex) {
	case 1:
		*AType = 0;
		*QType = 8;
		*b_min = FLT2FIX(0);
		*b_max = FLT2FIX(1);
		return 1;
	case 2:
		*AType = 0;
		*QType = 4;
		*b_min = FLT2FIX(0);
		*b_max = FLT2FIX(1);
		return 1;
	default:
		return 0;
	}
}



GF_Node *ColorInterpolator_Create()
{
	M_ColorInterpolator *p;
	GF_SAFEALLOC(p, sizeof(M_ColorInterpolator));
	if(!p) return NULL;
	gf_node_setup((GF_Node *)p, TAG_MPEG4_ColorInterpolator);

#ifdef GF_NODE_USE_POINTERS
	((GF_Node *)p)->sgprivate->name = "ColorInterpolator";
	((GF_Node *)p)->sgprivate->node_del = ColorInterpolator_Del;
	((GF_Node *)p)->sgprivate->get_field_count = ColorInterpolator_get_field_count;
	((GF_Node *)p)->sgprivate->get_field = ColorInterpolator_get_field;
#endif


	/*default field values*/
	return (GF_Node *)p;
}


/*
	CompositeTexture2D Node deletion
*/

static void CompositeTexture2D_Del(GF_Node *node)
{
	M_CompositeTexture2D *p = (M_CompositeTexture2D *) node;
	gf_sg_vrml_parent_reset((GF_Node *) p);	
	gf_node_unregister((GF_Node *) p->background, (GF_Node *) p);	
	gf_node_unregister((GF_Node *) p->viewport, (GF_Node *) p);	
	gf_node_free((GF_Node *) p);
}

static const u16 CompositeTexture2D_Def2All[] = { 2, 3, 4, 5, 6};
static const u16 CompositeTexture2D_In2All[] = { 0, 1, 2, 3, 4, 5, 6};
static const u16 CompositeTexture2D_Out2All[] = { 2, 3, 4, 5, 6};

static u32 CompositeTexture2D_get_field_count(GF_Node *node, u8 IndexMode)
{
	switch(IndexMode) {
	case GF_SG_FIELD_CODING_IN: return 7;
	case GF_SG_FIELD_CODING_DEF: return 5;
	case GF_SG_FIELD_CODING_OUT: return 5;
	case GF_SG_FIELD_CODING_DYN: return 0;
	default:
		return 7;
	}
}

static GF_Err CompositeTexture2D_get_field_index(GF_Node *n, u32 inField, u8 IndexMode, u32 *allField)
{
	switch(IndexMode) {
	case GF_SG_FIELD_CODING_IN:
		*allField = CompositeTexture2D_In2All[inField];
		return GF_OK;
	case GF_SG_FIELD_CODING_DEF:
		*allField = CompositeTexture2D_Def2All[inField];
		return GF_OK;
	case GF_SG_FIELD_CODING_OUT:
		*allField = CompositeTexture2D_Out2All[inField];
		return GF_OK;
	default:
		return GF_BAD_PARAM;
	}
}
static GF_Err CompositeTexture2D_get_field(GF_Node *node, GF_FieldInfo *info)
{
	switch (info->fieldIndex) {
	case 0:
		info->name = "addChildren";
		info->eventType = GF_SG_EVENT_IN;
		info->on_event_in = ((M_CompositeTexture2D *)node)->on_addChildren;
		info->fieldType = GF_SG_VRML_MFNODE;
		info->NDTtype = NDT_SF2DNode;
		info->far_ptr = & ((M_CompositeTexture2D *)node)->addChildren;
		return GF_OK;
	case 1:
		info->name = "removeChildren";
		info->eventType = GF_SG_EVENT_IN;
		info->on_event_in = ((M_CompositeTexture2D *)node)->on_removeChildren;
		info->fieldType = GF_SG_VRML_MFNODE;
		info->NDTtype = NDT_SF2DNode;
		info->far_ptr = & ((M_CompositeTexture2D *)node)->removeChildren;
		return GF_OK;
	case 2:
		info->name = "children";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_MFNODE;
		info->NDTtype = NDT_SF2DNode;
		info->far_ptr = & ((M_CompositeTexture2D *)node)->children;
		return GF_OK;
	case 3:
		info->name = "pixelWidth";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFINT32;
		info->far_ptr = & ((M_CompositeTexture2D *) node)->pixelWidth;
		return GF_OK;
	case 4:
		info->name = "pixelHeight";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFINT32;
		info->far_ptr = & ((M_CompositeTexture2D *) node)->pixelHeight;
		return GF_OK;
	case 5:
		info->name = "background";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFNODE;
		info->NDTtype = NDT_SFBackground2DNode;
		info->far_ptr = & ((M_CompositeTexture2D *)node)->background;
		return GF_OK;
	case 6:
		info->name = "viewport";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFNODE;
		info->NDTtype = NDT_SFViewportNode;
		info->far_ptr = & ((M_CompositeTexture2D *)node)->viewport;
		return GF_OK;
	default:
		return GF_BAD_PARAM;
	}
}

static Bool CompositeTexture2D_get_aq_info(GF_Node *n, u32 FieldIndex, u8 *QType, u8 *AType, Fixed *b_min, Fixed *b_max, u32 *QT13_bits)
{
	switch (FieldIndex) {
	case 3:
		*AType = 0;
		*QType = 13;
		*QT13_bits = 16;
		*b_min = FLT2FIX(0);
		*b_max = FIX_MAX /*WARNING: modified to allow 16.16 fixed point version!!*/;
		return 1;
	case 4:
		*AType = 0;
		*QType = 13;
		*QT13_bits = 16;
		*b_min = FLT2FIX(0);
		*b_max = FIX_MAX /*WARNING: modified to allow 16.16 fixed point version!!*/;
		return 1;
	default:
		return 0;
	}
}



GF_Node *CompositeTexture2D_Create()
{
	M_CompositeTexture2D *p;
	GF_SAFEALLOC(p, sizeof(M_CompositeTexture2D));
	if(!p) return NULL;
	gf_node_setup((GF_Node *)p, TAG_MPEG4_CompositeTexture2D);
	gf_sg_vrml_parent_setup((GF_Node *) p);

#ifdef GF_NODE_USE_POINTERS
	((GF_Node *)p)->sgprivate->name = "CompositeTexture2D";
	((GF_Node *)p)->sgprivate->node_del = CompositeTexture2D_Del;
	((GF_Node *)p)->sgprivate->get_field_count = CompositeTexture2D_get_field_count;
	((GF_Node *)p)->sgprivate->get_field = CompositeTexture2D_get_field;
#endif


	/*default field values*/
	p->pixelWidth = -1;
	p->pixelHeight = -1;
	return (GF_Node *)p;
}


/*
	CompositeTexture3D Node deletion
*/

static void CompositeTexture3D_Del(GF_Node *node)
{
	M_CompositeTexture3D *p = (M_CompositeTexture3D *) node;
	gf_sg_vrml_parent_reset((GF_Node *) p);	
	gf_node_unregister((GF_Node *) p->background, (GF_Node *) p);	
	gf_node_unregister((GF_Node *) p->fog, (GF_Node *) p);	
	gf_node_unregister((GF_Node *) p->navigationInfo, (GF_Node *) p);	
	gf_node_unregister((GF_Node *) p->viewpoint, (GF_Node *) p);	
	gf_node_free((GF_Node *) p);
}

static const u16 CompositeTexture3D_Def2All[] = { 2, 3, 4, 5, 6, 7, 8};
static const u16 CompositeTexture3D_In2All[] = { 0, 1, 2, 3, 4, 5, 6, 7, 8};
static const u16 CompositeTexture3D_Out2All[] = { 2, 3, 4, 5, 6, 7, 8};

static u32 CompositeTexture3D_get_field_count(GF_Node *node, u8 IndexMode)
{
	switch(IndexMode) {
	case GF_SG_FIELD_CODING_IN: return 9;
	case GF_SG_FIELD_CODING_DEF: return 7;
	case GF_SG_FIELD_CODING_OUT: return 7;
	case GF_SG_FIELD_CODING_DYN: return 0;
	default:
		return 9;
	}
}

static GF_Err CompositeTexture3D_get_field_index(GF_Node *n, u32 inField, u8 IndexMode, u32 *allField)
{
	switch(IndexMode) {
	case GF_SG_FIELD_CODING_IN:
		*allField = CompositeTexture3D_In2All[inField];
		return GF_OK;
	case GF_SG_FIELD_CODING_DEF:
		*allField = CompositeTexture3D_Def2All[inField];
		return GF_OK;
	case GF_SG_FIELD_CODING_OUT:
		*allField = CompositeTexture3D_Out2All[inField];
		return GF_OK;
	default:
		return GF_BAD_PARAM;
	}
}
static GF_Err CompositeTexture3D_get_field(GF_Node *node, GF_FieldInfo *info)
{
	switch (info->fieldIndex) {
	case 0:
		info->name = "addChildren";
		info->eventType = GF_SG_EVENT_IN;
		info->on_event_in = ((M_CompositeTexture3D *)node)->on_addChildren;
		info->fieldType = GF_SG_VRML_MFNODE;
		info->NDTtype = NDT_SF3DNode;
		info->far_ptr = & ((M_CompositeTexture3D *)node)->addChildren;
		return GF_OK;
	case 1:
		info->name = "removeChildren";
		info->eventType = GF_SG_EVENT_IN;
		info->on_event_in = ((M_CompositeTexture3D *)node)->on_removeChildren;
		info->fieldType = GF_SG_VRML_MFNODE;
		info->NDTtype = NDT_SF3DNode;
		info->far_ptr = & ((M_CompositeTexture3D *)node)->removeChildren;
		return GF_OK;
	case 2:
		info->name = "children";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_MFNODE;
		info->NDTtype = NDT_SF3DNode;
		info->far_ptr = & ((M_CompositeTexture3D *)node)->children;
		return GF_OK;
	case 3:
		info->name = "pixelWidth";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFINT32;
		info->far_ptr = & ((M_CompositeTexture3D *) node)->pixelWidth;
		return GF_OK;
	case 4:
		info->name = "pixelHeight";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFINT32;
		info->far_ptr = & ((M_CompositeTexture3D *) node)->pixelHeight;
		return GF_OK;
	case 5:
		info->name = "background";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFNODE;
		info->NDTtype = NDT_SFBackground3DNode;
		info->far_ptr = & ((M_CompositeTexture3D *)node)->background;
		return GF_OK;
	case 6:
		info->name = "fog";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFNODE;
		info->NDTtype = NDT_SFFogNode;
		info->far_ptr = & ((M_CompositeTexture3D *)node)->fog;
		return GF_OK;
	case 7:
		info->name = "navigationInfo";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFNODE;
		info->NDTtype = NDT_SFNavigationInfoNode;
		info->far_ptr = & ((M_CompositeTexture3D *)node)->navigationInfo;
		return GF_OK;
	case 8:
		info->name = "viewpoint";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFNODE;
		info->NDTtype = NDT_SFViewpointNode;
		info->far_ptr = & ((M_CompositeTexture3D *)node)->viewpoint;
		return GF_OK;
	default:
		return GF_BAD_PARAM;
	}
}

static Bool CompositeTexture3D_get_aq_info(GF_Node *n, u32 FieldIndex, u8 *QType, u8 *AType, Fixed *b_min, Fixed *b_max, u32 *QT13_bits)
{
	switch (FieldIndex) {
	case 3:
		*AType = 0;
		*QType = 13;
		*QT13_bits = 16;
		*b_min = FLT2FIX(0);
		*b_max = FIX_MAX /*WARNING: modified to allow 16.16 fixed point version!!*/;
		return 1;
	case 4:
		*AType = 0;
		*QType = 13;
		*QT13_bits = 16;
		*b_min = FLT2FIX(0);
		*b_max = FIX_MAX /*WARNING: modified to allow 16.16 fixed point version!!*/;
		return 1;
	default:
		return 0;
	}
}



GF_Node *CompositeTexture3D_Create()
{
	M_CompositeTexture3D *p;
	GF_SAFEALLOC(p, sizeof(M_CompositeTexture3D));
	if(!p) return NULL;
	gf_node_setup((GF_Node *)p, TAG_MPEG4_CompositeTexture3D);
	gf_sg_vrml_parent_setup((GF_Node *) p);

#ifdef GF_NODE_USE_POINTERS
	((GF_Node *)p)->sgprivate->name = "CompositeTexture3D";
	((GF_Node *)p)->sgprivate->node_del = CompositeTexture3D_Del;
	((GF_Node *)p)->sgprivate->get_field_count = CompositeTexture3D_get_field_count;
	((GF_Node *)p)->sgprivate->get_field = CompositeTexture3D_get_field;
#endif


	/*default field values*/
	p->pixelWidth = -1;
	p->pixelHeight = -1;
	return (GF_Node *)p;
}


/*
	Conditional Node deletion
*/

static void Conditional_Del(GF_Node *node)
{
	M_Conditional *p = (M_Conditional *) node;
	gf_sg_sfcommand_del(p->buffer);
	gf_node_free((GF_Node *) p);
}

static const u16 Conditional_Def2All[] = { 2};
static const u16 Conditional_In2All[] = { 0, 1, 2};
static const u16 Conditional_Out2All[] = { 2, 3};

static u32 Conditional_get_field_count(GF_Node *node, u8 IndexMode)
{
	switch(IndexMode) {
	case GF_SG_FIELD_CODING_IN: return 3;
	case GF_SG_FIELD_CODING_DEF: return 1;
	case GF_SG_FIELD_CODING_OUT: return 2;
	case GF_SG_FIELD_CODING_DYN: return 0;
	default:
		return 4;
	}
}

static GF_Err Conditional_get_field_index(GF_Node *n, u32 inField, u8 IndexMode, u32 *allField)
{
	switch(IndexMode) {
	case GF_SG_FIELD_CODING_IN:
		*allField = Conditional_In2All[inField];
		return GF_OK;
	case GF_SG_FIELD_CODING_DEF:
		*allField = Conditional_Def2All[inField];
		return GF_OK;
	case GF_SG_FIELD_CODING_OUT:
		*allField = Conditional_Out2All[inField];
		return GF_OK;
	default:
		return GF_BAD_PARAM;
	}
}
static GF_Err Conditional_get_field(GF_Node *node, GF_FieldInfo *info)
{
	switch (info->fieldIndex) {
	case 0:
		info->name = "activate";
		info->eventType = GF_SG_EVENT_IN;
		info->on_event_in = ((M_Conditional *)node)->on_activate;
		info->fieldType = GF_SG_VRML_SFBOOL;
		info->far_ptr = & ((M_Conditional *) node)->activate;
		return GF_OK;
	case 1:
		info->name = "reverseActivate";
		info->eventType = GF_SG_EVENT_IN;
		info->on_event_in = ((M_Conditional *)node)->on_reverseActivate;
		info->fieldType = GF_SG_VRML_SFBOOL;
		info->far_ptr = & ((M_Conditional *) node)->reverseActivate;
		return GF_OK;
	case 2:
		info->name = "buffer";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFCOMMANDBUFFER;
		info->far_ptr = & ((M_Conditional *) node)->buffer;
		return GF_OK;
	case 3:
		info->name = "isActive";
		info->eventType = GF_SG_EVENT_OUT;
		info->fieldType = GF_SG_VRML_SFBOOL;
		info->far_ptr = & ((M_Conditional *) node)->isActive;
		return GF_OK;
	default:
		return GF_BAD_PARAM;
	}
}

static Bool Conditional_get_aq_info(GF_Node *n, u32 FieldIndex, u8 *QType, u8 *AType, Fixed *b_min, Fixed *b_max, u32 *QT13_bits)
{
	switch (FieldIndex) {
	default:
		return 0;
	}
}



GF_Node *Conditional_Create()
{
	M_Conditional *p;
	GF_SAFEALLOC(p, sizeof(M_Conditional));
	if(!p) return NULL;
	gf_node_setup((GF_Node *)p, TAG_MPEG4_Conditional);
	p->buffer.commandList = gf_list_new();	

#ifdef GF_NODE_USE_POINTERS
	((GF_Node *)p)->sgprivate->name = "Conditional";
	((GF_Node *)p)->sgprivate->node_del = Conditional_Del;
	((GF_Node *)p)->sgprivate->get_field_count = Conditional_get_field_count;
	((GF_Node *)p)->sgprivate->get_field = Conditional_get_field;
#endif


	/*default field values*/
	return (GF_Node *)p;
}


/*
	Cone Node deletion
*/

static void Cone_Del(GF_Node *node)
{
	M_Cone *p = (M_Cone *) node;
	gf_node_free((GF_Node *) p);
}

static const u16 Cone_Def2All[] = { 0, 1, 2, 3};

static u32 Cone_get_field_count(GF_Node *node, u8 IndexMode)
{
	switch(IndexMode) {
	case GF_SG_FIELD_CODING_IN: return 0;
	case GF_SG_FIELD_CODING_DEF: return 4;
	case GF_SG_FIELD_CODING_OUT: return 0;
	case GF_SG_FIELD_CODING_DYN: return 0;
	default:
		return 4;
	}
}

static GF_Err Cone_get_field_index(GF_Node *n, u32 inField, u8 IndexMode, u32 *allField)
{
	switch(IndexMode) {
	case GF_SG_FIELD_CODING_DEF:
		*allField = Cone_Def2All[inField];
		return GF_OK;
	default:
		return GF_BAD_PARAM;
	}
}
static GF_Err Cone_get_field(GF_Node *node, GF_FieldInfo *info)
{
	switch (info->fieldIndex) {
	case 0:
		info->name = "bottomRadius";
		info->eventType = GF_SG_EVENT_FIELD;
		info->fieldType = GF_SG_VRML_SFFLOAT;
		info->far_ptr = & ((M_Cone *) node)->bottomRadius;
		return GF_OK;
	case 1:
		info->name = "height";
		info->eventType = GF_SG_EVENT_FIELD;
		info->fieldType = GF_SG_VRML_SFFLOAT;
		info->far_ptr = & ((M_Cone *) node)->height;
		return GF_OK;
	case 2:
		info->name = "side";
		info->eventType = GF_SG_EVENT_FIELD;
		info->fieldType = GF_SG_VRML_SFBOOL;
		info->far_ptr = & ((M_Cone *) node)->side;
		return GF_OK;
	case 3:
		info->name = "bottom";
		info->eventType = GF_SG_EVENT_FIELD;
		info->fieldType = GF_SG_VRML_SFBOOL;
		info->far_ptr = & ((M_Cone *) node)->bottom;
		return GF_OK;
	default:
		return GF_BAD_PARAM;
	}
}

static Bool Cone_get_aq_info(GF_Node *n, u32 FieldIndex, u8 *QType, u8 *AType, Fixed *b_min, Fixed *b_max, u32 *QT13_bits)
{
	switch (FieldIndex) {
	case 0:
		*AType = 0;
		*QType = 11;
		*b_min = FLT2FIX(0);
		*b_max = FIX_MAX;
		return 1;
	case 1:
		*AType = 0;
		*QType = 11;
		*b_min = FLT2FIX(0);
		*b_max = FIX_MAX;
		return 1;
	default:
		return 0;
	}
}



GF_Node *Cone_Create()
{
	M_Cone *p;
	GF_SAFEALLOC(p, sizeof(M_Cone));
	if(!p) return NULL;
	gf_node_setup((GF_Node *)p, TAG_MPEG4_Cone);

#ifdef GF_NODE_USE_POINTERS
	((GF_Node *)p)->sgprivate->name = "Cone";
	((GF_Node *)p)->sgprivate->node_del = Cone_Del;
	((GF_Node *)p)->sgprivate->get_field_count = Cone_get_field_count;
	((GF_Node *)p)->sgprivate->get_field = Cone_get_field;
#endif


	/*default field values*/
	p->bottomRadius = FLT2FIX(1);
	p->height = FLT2FIX(2);
	p->side = 1;
	p->bottom = 1;
	return (GF_Node *)p;
}


/*
	Coordinate Node deletion
*/

static void Coordinate_Del(GF_Node *node)
{
	M_Coordinate *p = (M_Coordinate *) node;
	gf_sg_mfvec3f_del(p->point);
	gf_node_free((GF_Node *) p);
}

static const u16 Coordinate_Def2All[] = { 0};
static const u16 Coordinate_In2All[] = { 0};
static const u16 Coordinate_Out2All[] = { 0};
static const u16 Coordinate_Dyn2All[] = { 0};

static u32 Coordinate_get_field_count(GF_Node *node, u8 IndexMode)
{
	switch(IndexMode) {
	case GF_SG_FIELD_CODING_IN: return 1;
	case GF_SG_FIELD_CODING_DEF: return 1;
	case GF_SG_FIELD_CODING_OUT: return 1;
	case GF_SG_FIELD_CODING_DYN: return 1;
	default:
		return 1;
	}
}

static GF_Err Coordinate_get_field_index(GF_Node *n, u32 inField, u8 IndexMode, u32 *allField)
{
	switch(IndexMode) {
	case GF_SG_FIELD_CODING_IN:
		*allField = Coordinate_In2All[inField];
		return GF_OK;
	case GF_SG_FIELD_CODING_DEF:
		*allField = Coordinate_Def2All[inField];
		return GF_OK;
	case GF_SG_FIELD_CODING_OUT:
		*allField = Coordinate_Out2All[inField];
		return GF_OK;
	case GF_SG_FIELD_CODING_DYN:
		*allField = Coordinate_Dyn2All[inField];
		return GF_OK;
	default:
		return GF_BAD_PARAM;
	}
}
static GF_Err Coordinate_get_field(GF_Node *node, GF_FieldInfo *info)
{
	switch (info->fieldIndex) {
	case 0:
		info->name = "point";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_MFVEC3F;
		info->far_ptr = & ((M_Coordinate *) node)->point;
		return GF_OK;
	default:
		return GF_BAD_PARAM;
	}
}

static Bool Coordinate_get_aq_info(GF_Node *n, u32 FieldIndex, u8 *QType, u8 *AType, Fixed *b_min, Fixed *b_max, u32 *QT13_bits)
{
	switch (FieldIndex) {
	case 0:
		*AType = 1;
		*QType = 1;
		*b_min = FIX_MIN;
		*b_max = FIX_MAX;
		return 1;
	default:
		return 0;
	}
}



GF_Node *Coordinate_Create()
{
	M_Coordinate *p;
	GF_SAFEALLOC(p, sizeof(M_Coordinate));
	if(!p) return NULL;
	gf_node_setup((GF_Node *)p, TAG_MPEG4_Coordinate);

#ifdef GF_NODE_USE_POINTERS
	((GF_Node *)p)->sgprivate->name = "Coordinate";
	((GF_Node *)p)->sgprivate->node_del = Coordinate_Del;
	((GF_Node *)p)->sgprivate->get_field_count = Coordinate_get_field_count;
	((GF_Node *)p)->sgprivate->get_field = Coordinate_get_field;
#endif


	/*default field values*/
	return (GF_Node *)p;
}


/*
	Coordinate2D Node deletion
*/

static void Coordinate2D_Del(GF_Node *node)
{
	M_Coordinate2D *p = (M_Coordinate2D *) node;
	gf_sg_mfvec2f_del(p->point);
	gf_node_free((GF_Node *) p);
}

static const u16 Coordinate2D_Def2All[] = { 0};
static const u16 Coordinate2D_In2All[] = { 0};
static const u16 Coordinate2D_Out2All[] = { 0};
static const u16 Coordinate2D_Dyn2All[] = { 0};

static u32 Coordinate2D_get_field_count(GF_Node *node, u8 IndexMode)
{
	switch(IndexMode) {
	case GF_SG_FIELD_CODING_IN: return 1;
	case GF_SG_FIELD_CODING_DEF: return 1;
	case GF_SG_FIELD_CODING_OUT: return 1;
	case GF_SG_FIELD_CODING_DYN: return 1;
	default:
		return 1;
	}
}

static GF_Err Coordinate2D_get_field_index(GF_Node *n, u32 inField, u8 IndexMode, u32 *allField)
{
	switch(IndexMode) {
	case GF_SG_FIELD_CODING_IN:
		*allField = Coordinate2D_In2All[inField];
		return GF_OK;
	case GF_SG_FIELD_CODING_DEF:
		*allField = Coordinate2D_Def2All[inField];
		return GF_OK;
	case GF_SG_FIELD_CODING_OUT:
		*allField = Coordinate2D_Out2All[inField];
		return GF_OK;
	case GF_SG_FIELD_CODING_DYN:
		*allField = Coordinate2D_Dyn2All[inField];
		return GF_OK;
	default:
		return GF_BAD_PARAM;
	}
}
static GF_Err Coordinate2D_get_field(GF_Node *node, GF_FieldInfo *info)
{
	switch (info->fieldIndex) {
	case 0:
		info->name = "point";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_MFVEC2F;
		info->far_ptr = & ((M_Coordinate2D *) node)->point;
		return GF_OK;
	default:
		return GF_BAD_PARAM;
	}
}

static Bool Coordinate2D_get_aq_info(GF_Node *n, u32 FieldIndex, u8 *QType, u8 *AType, Fixed *b_min, Fixed *b_max, u32 *QT13_bits)
{
	switch (FieldIndex) {
	case 0:
		*AType = 2;
		*QType = 2;
		*b_min = FIX_MIN;
		*b_max = FIX_MAX;
		return 1;
	default:
		return 0;
	}
}



GF_Node *Coordinate2D_Create()
{
	M_Coordinate2D *p;
	GF_SAFEALLOC(p, sizeof(M_Coordinate2D));
	if(!p) return NULL;
	gf_node_setup((GF_Node *)p, TAG_MPEG4_Coordinate2D);

#ifdef GF_NODE_USE_POINTERS
	((GF_Node *)p)->sgprivate->name = "Coordinate2D";
	((GF_Node *)p)->sgprivate->node_del = Coordinate2D_Del;
	((GF_Node *)p)->sgprivate->get_field_count = Coordinate2D_get_field_count;
	((GF_Node *)p)->sgprivate->get_field = Coordinate2D_get_field;
#endif


	/*default field values*/
	return (GF_Node *)p;
}


/*
	CoordinateInterpolator Node deletion
*/

static void CoordinateInterpolator_Del(GF_Node *node)
{
	M_CoordinateInterpolator *p = (M_CoordinateInterpolator *) node;
	gf_sg_mffloat_del(p->key);
	gf_sg_mfvec3f_del(p->keyValue);
	gf_sg_mfvec3f_del(p->value_changed);
	gf_node_free((GF_Node *) p);
}

static const u16 CoordinateInterpolator_Def2All[] = { 1, 2};
static const u16 CoordinateInterpolator_In2All[] = { 0, 1, 2};
static const u16 CoordinateInterpolator_Out2All[] = { 1, 2, 3};

static u32 CoordinateInterpolator_get_field_count(GF_Node *node, u8 IndexMode)
{
	switch(IndexMode) {
	case GF_SG_FIELD_CODING_IN: return 3;
	case GF_SG_FIELD_CODING_DEF: return 2;
	case GF_SG_FIELD_CODING_OUT: return 3;
	case GF_SG_FIELD_CODING_DYN: return 0;
	default:
		return 4;
	}
}

static GF_Err CoordinateInterpolator_get_field_index(GF_Node *n, u32 inField, u8 IndexMode, u32 *allField)
{
	switch(IndexMode) {
	case GF_SG_FIELD_CODING_IN:
		*allField = CoordinateInterpolator_In2All[inField];
		return GF_OK;
	case GF_SG_FIELD_CODING_DEF:
		*allField = CoordinateInterpolator_Def2All[inField];
		return GF_OK;
	case GF_SG_FIELD_CODING_OUT:
		*allField = CoordinateInterpolator_Out2All[inField];
		return GF_OK;
	default:
		return GF_BAD_PARAM;
	}
}
static GF_Err CoordinateInterpolator_get_field(GF_Node *node, GF_FieldInfo *info)
{
	switch (info->fieldIndex) {
	case 0:
		info->name = "set_fraction";
		info->eventType = GF_SG_EVENT_IN;
		info->on_event_in = ((M_CoordinateInterpolator *)node)->on_set_fraction;
		info->fieldType = GF_SG_VRML_SFFLOAT;
		info->far_ptr = & ((M_CoordinateInterpolator *) node)->set_fraction;
		return GF_OK;
	case 1:
		info->name = "key";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_MFFLOAT;
		info->far_ptr = & ((M_CoordinateInterpolator *) node)->key;
		return GF_OK;
	case 2:
		info->name = "keyValue";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_MFVEC3F;
		info->far_ptr = & ((M_CoordinateInterpolator *) node)->keyValue;
		return GF_OK;
	case 3:
		info->name = "value_changed";
		info->eventType = GF_SG_EVENT_OUT;
		info->fieldType = GF_SG_VRML_MFVEC3F;
		info->far_ptr = & ((M_CoordinateInterpolator *) node)->value_changed;
		return GF_OK;
	default:
		return GF_BAD_PARAM;
	}
}

static Bool CoordinateInterpolator_get_aq_info(GF_Node *n, u32 FieldIndex, u8 *QType, u8 *AType, Fixed *b_min, Fixed *b_max, u32 *QT13_bits)
{
	switch (FieldIndex) {
	case 1:
		*AType = 0;
		*QType = 8;
		*b_min = FLT2FIX(0);
		*b_max = FLT2FIX(1);
		return 1;
	case 2:
		*AType = 0;
		*QType = 1;
		*b_min = FIX_MIN;
		*b_max = FIX_MAX;
		return 1;
	default:
		return 0;
	}
}



GF_Node *CoordinateInterpolator_Create()
{
	M_CoordinateInterpolator *p;
	GF_SAFEALLOC(p, sizeof(M_CoordinateInterpolator));
	if(!p) return NULL;
	gf_node_setup((GF_Node *)p, TAG_MPEG4_CoordinateInterpolator);

#ifdef GF_NODE_USE_POINTERS
	((GF_Node *)p)->sgprivate->name = "CoordinateInterpolator";
	((GF_Node *)p)->sgprivate->node_del = CoordinateInterpolator_Del;
	((GF_Node *)p)->sgprivate->get_field_count = CoordinateInterpolator_get_field_count;
	((GF_Node *)p)->sgprivate->get_field = CoordinateInterpolator_get_field;
#endif


	/*default field values*/
	return (GF_Node *)p;
}


/*
	CoordinateInterpolator2D Node deletion
*/

static void CoordinateInterpolator2D_Del(GF_Node *node)
{
	M_CoordinateInterpolator2D *p = (M_CoordinateInterpolator2D *) node;
	gf_sg_mffloat_del(p->key);
	gf_sg_mfvec2f_del(p->keyValue);
	gf_sg_mfvec2f_del(p->value_changed);
	gf_node_free((GF_Node *) p);
}

static const u16 CoordinateInterpolator2D_Def2All[] = { 1, 2};
static const u16 CoordinateInterpolator2D_In2All[] = { 0, 1, 2};
static const u16 CoordinateInterpolator2D_Out2All[] = { 1, 2, 3};

static u32 CoordinateInterpolator2D_get_field_count(GF_Node *node, u8 IndexMode)
{
	switch(IndexMode) {
	case GF_SG_FIELD_CODING_IN: return 3;
	case GF_SG_FIELD_CODING_DEF: return 2;
	case GF_SG_FIELD_CODING_OUT: return 3;
	case GF_SG_FIELD_CODING_DYN: return 0;
	default:
		return 4;
	}
}

static GF_Err CoordinateInterpolator2D_get_field_index(GF_Node *n, u32 inField, u8 IndexMode, u32 *allField)
{
	switch(IndexMode) {
	case GF_SG_FIELD_CODING_IN:
		*allField = CoordinateInterpolator2D_In2All[inField];
		return GF_OK;
	case GF_SG_FIELD_CODING_DEF:
		*allField = CoordinateInterpolator2D_Def2All[inField];
		return GF_OK;
	case GF_SG_FIELD_CODING_OUT:
		*allField = CoordinateInterpolator2D_Out2All[inField];
		return GF_OK;
	default:
		return GF_BAD_PARAM;
	}
}
static GF_Err CoordinateInterpolator2D_get_field(GF_Node *node, GF_FieldInfo *info)
{
	switch (info->fieldIndex) {
	case 0:
		info->name = "set_fraction";
		info->eventType = GF_SG_EVENT_IN;
		info->on_event_in = ((M_CoordinateInterpolator2D *)node)->on_set_fraction;
		info->fieldType = GF_SG_VRML_SFFLOAT;
		info->far_ptr = & ((M_CoordinateInterpolator2D *) node)->set_fraction;
		return GF_OK;
	case 1:
		info->name = "key";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_MFFLOAT;
		info->far_ptr = & ((M_CoordinateInterpolator2D *) node)->key;
		return GF_OK;
	case 2:
		info->name = "keyValue";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_MFVEC2F;
		info->far_ptr = & ((M_CoordinateInterpolator2D *) node)->keyValue;
		return GF_OK;
	case 3:
		info->name = "value_changed";
		info->eventType = GF_SG_EVENT_OUT;
		info->fieldType = GF_SG_VRML_MFVEC2F;
		info->far_ptr = & ((M_CoordinateInterpolator2D *) node)->value_changed;
		return GF_OK;
	default:
		return GF_BAD_PARAM;
	}
}

static Bool CoordinateInterpolator2D_get_aq_info(GF_Node *n, u32 FieldIndex, u8 *QType, u8 *AType, Fixed *b_min, Fixed *b_max, u32 *QT13_bits)
{
	switch (FieldIndex) {
	case 1:
		*AType = 0;
		*QType = 8;
		*b_min = FLT2FIX(0);
		*b_max = FLT2FIX(1);
		return 1;
	case 2:
		*AType = 0;
		*QType = 2;
		*b_min = FIX_MIN;
		*b_max = FIX_MAX;
		return 1;
	default:
		return 0;
	}
}



GF_Node *CoordinateInterpolator2D_Create()
{
	M_CoordinateInterpolator2D *p;
	GF_SAFEALLOC(p, sizeof(M_CoordinateInterpolator2D));
	if(!p) return NULL;
	gf_node_setup((GF_Node *)p, TAG_MPEG4_CoordinateInterpolator2D);

#ifdef GF_NODE_USE_POINTERS
	((GF_Node *)p)->sgprivate->name = "CoordinateInterpolator2D";
	((GF_Node *)p)->sgprivate->node_del = CoordinateInterpolator2D_Del;
	((GF_Node *)p)->sgprivate->get_field_count = CoordinateInterpolator2D_get_field_count;
	((GF_Node *)p)->sgprivate->get_field = CoordinateInterpolator2D_get_field;
#endif


	/*default field values*/
	return (GF_Node *)p;
}


/*
	Curve2D Node deletion
*/

static void Curve2D_Del(GF_Node *node)
{
	M_Curve2D *p = (M_Curve2D *) node;
	gf_node_unregister((GF_Node *) p->point, (GF_Node *) p);	
	gf_sg_mfint32_del(p->type);
	gf_node_free((GF_Node *) p);
}

static const u16 Curve2D_Def2All[] = { 0, 1, 2};
static const u16 Curve2D_In2All[] = { 0, 1, 2};
static const u16 Curve2D_Out2All[] = { 0, 1, 2};
static const u16 Curve2D_Dyn2All[] = { 1};

static u32 Curve2D_get_field_count(GF_Node *node, u8 IndexMode)
{
	switch(IndexMode) {
	case GF_SG_FIELD_CODING_IN: return 3;
	case GF_SG_FIELD_CODING_DEF: return 3;
	case GF_SG_FIELD_CODING_OUT: return 3;
	case GF_SG_FIELD_CODING_DYN: return 1;
	default:
		return 3;
	}
}

static GF_Err Curve2D_get_field_index(GF_Node *n, u32 inField, u8 IndexMode, u32 *allField)
{
	switch(IndexMode) {
	case GF_SG_FIELD_CODING_IN:
		*allField = Curve2D_In2All[inField];
		return GF_OK;
	case GF_SG_FIELD_CODING_DEF:
		*allField = Curve2D_Def2All[inField];
		return GF_OK;
	case GF_SG_FIELD_CODING_OUT:
		*allField = Curve2D_Out2All[inField];
		return GF_OK;
	case GF_SG_FIELD_CODING_DYN:
		*allField = Curve2D_Dyn2All[inField];
		return GF_OK;
	default:
		return GF_BAD_PARAM;
	}
}
static GF_Err Curve2D_get_field(GF_Node *node, GF_FieldInfo *info)
{
	switch (info->fieldIndex) {
	case 0:
		info->name = "point";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFNODE;
		info->NDTtype = NDT_SFCoordinate2DNode;
		info->far_ptr = & ((M_Curve2D *)node)->point;
		return GF_OK;
	case 1:
		info->name = "fineness";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFFLOAT;
		info->far_ptr = & ((M_Curve2D *) node)->fineness;
		return GF_OK;
	case 2:
		info->name = "type";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_MFINT32;
		info->far_ptr = & ((M_Curve2D *) node)->type;
		return GF_OK;
	default:
		return GF_BAD_PARAM;
	}
}

static Bool Curve2D_get_aq_info(GF_Node *n, u32 FieldIndex, u8 *QType, u8 *AType, Fixed *b_min, Fixed *b_max, u32 *QT13_bits)
{
	switch (FieldIndex) {
	case 1:
		*AType = 7;
		*QType = 0;
		*b_min = FLT2FIX(0);
		*b_max = FLT2FIX(1);
		return 1;
	case 2:
		*AType = 0;
		*QType = 13;
		*QT13_bits = 2;
		*b_min = FLT2FIX(0);
		*b_max = FLT2FIX(3);
		return 1;
	default:
		return 0;
	}
}



GF_Node *Curve2D_Create()
{
	M_Curve2D *p;
	GF_SAFEALLOC(p, sizeof(M_Curve2D));
	if(!p) return NULL;
	gf_node_setup((GF_Node *)p, TAG_MPEG4_Curve2D);

#ifdef GF_NODE_USE_POINTERS
	((GF_Node *)p)->sgprivate->name = "Curve2D";
	((GF_Node *)p)->sgprivate->node_del = Curve2D_Del;
	((GF_Node *)p)->sgprivate->get_field_count = Curve2D_get_field_count;
	((GF_Node *)p)->sgprivate->get_field = Curve2D_get_field;
#endif


	/*default field values*/
	p->fineness = FLT2FIX(0.5);
	return (GF_Node *)p;
}


/*
	Cylinder Node deletion
*/

static void Cylinder_Del(GF_Node *node)
{
	M_Cylinder *p = (M_Cylinder *) node;
	gf_node_free((GF_Node *) p);
}

static const u16 Cylinder_Def2All[] = { 0, 1, 2, 3, 4};

static u32 Cylinder_get_field_count(GF_Node *node, u8 IndexMode)
{
	switch(IndexMode) {
	case GF_SG_FIELD_CODING_IN: return 0;
	case GF_SG_FIELD_CODING_DEF: return 5;
	case GF_SG_FIELD_CODING_OUT: return 0;
	case GF_SG_FIELD_CODING_DYN: return 0;
	default:
		return 5;
	}
}

static GF_Err Cylinder_get_field_index(GF_Node *n, u32 inField, u8 IndexMode, u32 *allField)
{
	switch(IndexMode) {
	case GF_SG_FIELD_CODING_DEF:
		*allField = Cylinder_Def2All[inField];
		return GF_OK;
	default:
		return GF_BAD_PARAM;
	}
}
static GF_Err Cylinder_get_field(GF_Node *node, GF_FieldInfo *info)
{
	switch (info->fieldIndex) {
	case 0:
		info->name = "bottom";
		info->eventType = GF_SG_EVENT_FIELD;
		info->fieldType = GF_SG_VRML_SFBOOL;
		info->far_ptr = & ((M_Cylinder *) node)->bottom;
		return GF_OK;
	case 1:
		info->name = "height";
		info->eventType = GF_SG_EVENT_FIELD;
		info->fieldType = GF_SG_VRML_SFFLOAT;
		info->far_ptr = & ((M_Cylinder *) node)->height;
		return GF_OK;
	case 2:
		info->name = "radius";
		info->eventType = GF_SG_EVENT_FIELD;
		info->fieldType = GF_SG_VRML_SFFLOAT;
		info->far_ptr = & ((M_Cylinder *) node)->radius;
		return GF_OK;
	case 3:
		info->name = "side";
		info->eventType = GF_SG_EVENT_FIELD;
		info->fieldType = GF_SG_VRML_SFBOOL;
		info->far_ptr = & ((M_Cylinder *) node)->side;
		return GF_OK;
	case 4:
		info->name = "top";
		info->eventType = GF_SG_EVENT_FIELD;
		info->fieldType = GF_SG_VRML_SFBOOL;
		info->far_ptr = & ((M_Cylinder *) node)->top;
		return GF_OK;
	default:
		return GF_BAD_PARAM;
	}
}

static Bool Cylinder_get_aq_info(GF_Node *n, u32 FieldIndex, u8 *QType, u8 *AType, Fixed *b_min, Fixed *b_max, u32 *QT13_bits)
{
	switch (FieldIndex) {
	case 1:
		*AType = 0;
		*QType = 11;
		*b_min = FLT2FIX(0);
		*b_max = FIX_MAX;
		return 1;
	case 2:
		*AType = 0;
		*QType = 11;
		*b_min = FLT2FIX(0);
		*b_max = FIX_MAX;
		return 1;
	default:
		return 0;
	}
}



GF_Node *Cylinder_Create()
{
	M_Cylinder *p;
	GF_SAFEALLOC(p, sizeof(M_Cylinder));
	if(!p) return NULL;
	gf_node_setup((GF_Node *)p, TAG_MPEG4_Cylinder);

#ifdef GF_NODE_USE_POINTERS
	((GF_Node *)p)->sgprivate->name = "Cylinder";
	((GF_Node *)p)->sgprivate->node_del = Cylinder_Del;
	((GF_Node *)p)->sgprivate->get_field_count = Cylinder_get_field_count;
	((GF_Node *)p)->sgprivate->get_field = Cylinder_get_field;
#endif


	/*default field values*/
	p->bottom = 1;
	p->height = FLT2FIX(2);
	p->radius = FLT2FIX(1);
	p->side = 1;
	p->top = 1;
	return (GF_Node *)p;
}


/*
	CylinderSensor Node deletion
*/

static void CylinderSensor_Del(GF_Node *node)
{
	M_CylinderSensor *p = (M_CylinderSensor *) node;
	gf_node_free((GF_Node *) p);
}

static const u16 CylinderSensor_Def2All[] = { 0, 1, 2, 3, 4, 5};
static const u16 CylinderSensor_In2All[] = { 0, 1, 2, 3, 4, 5};
static const u16 CylinderSensor_Out2All[] = { 0, 1, 2, 3, 4, 5, 6, 7, 8};

static u32 CylinderSensor_get_field_count(GF_Node *node, u8 IndexMode)
{
	switch(IndexMode) {
	case GF_SG_FIELD_CODING_IN: return 6;
	case GF_SG_FIELD_CODING_DEF: return 6;
	case GF_SG_FIELD_CODING_OUT: return 9;
	case GF_SG_FIELD_CODING_DYN: return 0;
	default:
		return 9;
	}
}

static GF_Err CylinderSensor_get_field_index(GF_Node *n, u32 inField, u8 IndexMode, u32 *allField)
{
	switch(IndexMode) {
	case GF_SG_FIELD_CODING_IN:
		*allField = CylinderSensor_In2All[inField];
		return GF_OK;
	case GF_SG_FIELD_CODING_DEF:
		*allField = CylinderSensor_Def2All[inField];
		return GF_OK;
	case GF_SG_FIELD_CODING_OUT:
		*allField = CylinderSensor_Out2All[inField];
		return GF_OK;
	default:
		return GF_BAD_PARAM;
	}
}
static GF_Err CylinderSensor_get_field(GF_Node *node, GF_FieldInfo *info)
{
	switch (info->fieldIndex) {
	case 0:
		info->name = "autoOffset";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFBOOL;
		info->far_ptr = & ((M_CylinderSensor *) node)->autoOffset;
		return GF_OK;
	case 1:
		info->name = "diskAngle";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFFLOAT;
		info->far_ptr = & ((M_CylinderSensor *) node)->diskAngle;
		return GF_OK;
	case 2:
		info->name = "enabled";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFBOOL;
		info->far_ptr = & ((M_CylinderSensor *) node)->enabled;
		return GF_OK;
	case 3:
		info->name = "maxAngle";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFFLOAT;
		info->far_ptr = & ((M_CylinderSensor *) node)->maxAngle;
		return GF_OK;
	case 4:
		info->name = "minAngle";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFFLOAT;
		info->far_ptr = & ((M_CylinderSensor *) node)->minAngle;
		return GF_OK;
	case 5:
		info->name = "offset";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFFLOAT;
		info->far_ptr = & ((M_CylinderSensor *) node)->offset;
		return GF_OK;
	case 6:
		info->name = "isActive";
		info->eventType = GF_SG_EVENT_OUT;
		info->fieldType = GF_SG_VRML_SFBOOL;
		info->far_ptr = & ((M_CylinderSensor *) node)->isActive;
		return GF_OK;
	case 7:
		info->name = "rotation_changed";
		info->eventType = GF_SG_EVENT_OUT;
		info->fieldType = GF_SG_VRML_SFROTATION;
		info->far_ptr = & ((M_CylinderSensor *) node)->rotation_changed;
		return GF_OK;
	case 8:
		info->name = "trackPoint_changed";
		info->eventType = GF_SG_EVENT_OUT;
		info->fieldType = GF_SG_VRML_SFVEC3F;
		info->far_ptr = & ((M_CylinderSensor *) node)->trackPoint_changed;
		return GF_OK;
	default:
		return GF_BAD_PARAM;
	}
}

static Bool CylinderSensor_get_aq_info(GF_Node *n, u32 FieldIndex, u8 *QType, u8 *AType, Fixed *b_min, Fixed *b_max, u32 *QT13_bits)
{
	switch (FieldIndex) {
	case 1:
		*AType = 0;
		*QType = 6;
		*b_min = FLT2FIX(0);
		*b_max = FLT2FIX(1.5707963);
		return 1;
	case 3:
		*AType = 0;
		*QType = 6;
		*b_min = FLT2FIX(-6.2831853);
		*b_max = FLT2FIX(6.2831853);
		return 1;
	case 4:
		*AType = 0;
		*QType = 6;
		*b_min = FLT2FIX(-6.2831853);
		*b_max = FLT2FIX(6.2831853);
		return 1;
	case 5:
		*AType = 0;
		*QType = 6;
		*b_min = FLT2FIX(0);
		*b_max = FLT2FIX(6.2831853);
		return 1;
	default:
		return 0;
	}
}



GF_Node *CylinderSensor_Create()
{
	M_CylinderSensor *p;
	GF_SAFEALLOC(p, sizeof(M_CylinderSensor));
	if(!p) return NULL;
	gf_node_setup((GF_Node *)p, TAG_MPEG4_CylinderSensor);

#ifdef GF_NODE_USE_POINTERS
	((GF_Node *)p)->sgprivate->name = "CylinderSensor";
	((GF_Node *)p)->sgprivate->node_del = CylinderSensor_Del;
	((GF_Node *)p)->sgprivate->get_field_count = CylinderSensor_get_field_count;
	((GF_Node *)p)->sgprivate->get_field = CylinderSensor_get_field;
#endif


	/*default field values*/
	p->autoOffset = 1;
	p->diskAngle = FLT2FIX(0.262);
	p->enabled = 1;
	p->maxAngle = FLT2FIX(-1);
	p->minAngle = FLT2FIX(0);
	p->offset = FLT2FIX(0);
	return (GF_Node *)p;
}


/*
	DirectionalLight Node deletion
*/

static void DirectionalLight_Del(GF_Node *node)
{
	M_DirectionalLight *p = (M_DirectionalLight *) node;
	gf_node_free((GF_Node *) p);
}

static const u16 DirectionalLight_Def2All[] = { 0, 1, 2, 3, 4};
static const u16 DirectionalLight_In2All[] = { 0, 1, 2, 3, 4};
static const u16 DirectionalLight_Out2All[] = { 0, 1, 2, 3, 4};
static const u16 DirectionalLight_Dyn2All[] = { 0, 1, 2, 3};

static u32 DirectionalLight_get_field_count(GF_Node *node, u8 IndexMode)
{
	switch(IndexMode) {
	case GF_SG_FIELD_CODING_IN: return 5;
	case GF_SG_FIELD_CODING_DEF: return 5;
	case GF_SG_FIELD_CODING_OUT: return 5;
	case GF_SG_FIELD_CODING_DYN: return 4;
	default:
		return 5;
	}
}

static GF_Err DirectionalLight_get_field_index(GF_Node *n, u32 inField, u8 IndexMode, u32 *allField)
{
	switch(IndexMode) {
	case GF_SG_FIELD_CODING_IN:
		*allField = DirectionalLight_In2All[inField];
		return GF_OK;
	case GF_SG_FIELD_CODING_DEF:
		*allField = DirectionalLight_Def2All[inField];
		return GF_OK;
	case GF_SG_FIELD_CODING_OUT:
		*allField = DirectionalLight_Out2All[inField];
		return GF_OK;
	case GF_SG_FIELD_CODING_DYN:
		*allField = DirectionalLight_Dyn2All[inField];
		return GF_OK;
	default:
		return GF_BAD_PARAM;
	}
}
static GF_Err DirectionalLight_get_field(GF_Node *node, GF_FieldInfo *info)
{
	switch (info->fieldIndex) {
	case 0:
		info->name = "ambientIntensity";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFFLOAT;
		info->far_ptr = & ((M_DirectionalLight *) node)->ambientIntensity;
		return GF_OK;
	case 1:
		info->name = "color";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFCOLOR;
		info->far_ptr = & ((M_DirectionalLight *) node)->color;
		return GF_OK;
	case 2:
		info->name = "direction";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFVEC3F;
		info->far_ptr = & ((M_DirectionalLight *) node)->direction;
		return GF_OK;
	case 3:
		info->name = "intensity";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFFLOAT;
		info->far_ptr = & ((M_DirectionalLight *) node)->intensity;
		return GF_OK;
	case 4:
		info->name = "on";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFBOOL;
		info->far_ptr = & ((M_DirectionalLight *) node)->on;
		return GF_OK;
	default:
		return GF_BAD_PARAM;
	}
}

static Bool DirectionalLight_get_aq_info(GF_Node *n, u32 FieldIndex, u8 *QType, u8 *AType, Fixed *b_min, Fixed *b_max, u32 *QT13_bits)
{
	switch (FieldIndex) {
	case 0:
		*AType = 8;
		*QType = 4;
		*b_min = FLT2FIX(0);
		*b_max = FLT2FIX(1);
		return 1;
	case 1:
		*AType = 4;
		*QType = 4;
		*b_min = FLT2FIX(0);
		*b_max = FLT2FIX(1);
		return 1;
	case 2:
		*AType = 9;
		*QType = 9;
		return 1;
	case 3:
		*AType = 8;
		*QType = 4;
		*b_min = FLT2FIX(0);
		*b_max = FLT2FIX(1);
		return 1;
	default:
		return 0;
	}
}



GF_Node *DirectionalLight_Create()
{
	M_DirectionalLight *p;
	GF_SAFEALLOC(p, sizeof(M_DirectionalLight));
	if(!p) return NULL;
	gf_node_setup((GF_Node *)p, TAG_MPEG4_DirectionalLight);

#ifdef GF_NODE_USE_POINTERS
	((GF_Node *)p)->sgprivate->name = "DirectionalLight";
	((GF_Node *)p)->sgprivate->node_del = DirectionalLight_Del;
	((GF_Node *)p)->sgprivate->get_field_count = DirectionalLight_get_field_count;
	((GF_Node *)p)->sgprivate->get_field = DirectionalLight_get_field;
#endif


	/*default field values*/
	p->ambientIntensity = FLT2FIX(0);
	p->color.red = FLT2FIX(1);
	p->color.green = FLT2FIX(1);
	p->color.blue = FLT2FIX(1);
	p->direction.x = FLT2FIX(0);
	p->direction.y = FLT2FIX(0);
	p->direction.z = FLT2FIX(-1);
	p->intensity = FLT2FIX(1);
	p->on = 1;
	return (GF_Node *)p;
}


/*
	DiscSensor Node deletion
*/

static void DiscSensor_Del(GF_Node *node)
{
	M_DiscSensor *p = (M_DiscSensor *) node;
	gf_node_free((GF_Node *) p);
}

static const u16 DiscSensor_Def2All[] = { 0, 1, 2, 3, 4};
static const u16 DiscSensor_In2All[] = { 0, 1, 2, 3, 4};
static const u16 DiscSensor_Out2All[] = { 0, 1, 2, 3, 4, 5, 6, 7};

static u32 DiscSensor_get_field_count(GF_Node *node, u8 IndexMode)
{
	switch(IndexMode) {
	case GF_SG_FIELD_CODING_IN: return 5;
	case GF_SG_FIELD_CODING_DEF: return 5;
	case GF_SG_FIELD_CODING_OUT: return 8;
	case GF_SG_FIELD_CODING_DYN: return 0;
	default:
		return 8;
	}
}

static GF_Err DiscSensor_get_field_index(GF_Node *n, u32 inField, u8 IndexMode, u32 *allField)
{
	switch(IndexMode) {
	case GF_SG_FIELD_CODING_IN:
		*allField = DiscSensor_In2All[inField];
		return GF_OK;
	case GF_SG_FIELD_CODING_DEF:
		*allField = DiscSensor_Def2All[inField];
		return GF_OK;
	case GF_SG_FIELD_CODING_OUT:
		*allField = DiscSensor_Out2All[inField];
		return GF_OK;
	default:
		return GF_BAD_PARAM;
	}
}
static GF_Err DiscSensor_get_field(GF_Node *node, GF_FieldInfo *info)
{
	switch (info->fieldIndex) {
	case 0:
		info->name = "autoOffset";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFBOOL;
		info->far_ptr = & ((M_DiscSensor *) node)->autoOffset;
		return GF_OK;
	case 1:
		info->name = "enabled";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFBOOL;
		info->far_ptr = & ((M_DiscSensor *) node)->enabled;
		return GF_OK;
	case 2:
		info->name = "maxAngle";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFFLOAT;
		info->far_ptr = & ((M_DiscSensor *) node)->maxAngle;
		return GF_OK;
	case 3:
		info->name = "minAngle";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFFLOAT;
		info->far_ptr = & ((M_DiscSensor *) node)->minAngle;
		return GF_OK;
	case 4:
		info->name = "offset";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFFLOAT;
		info->far_ptr = & ((M_DiscSensor *) node)->offset;
		return GF_OK;
	case 5:
		info->name = "isActive";
		info->eventType = GF_SG_EVENT_OUT;
		info->fieldType = GF_SG_VRML_SFBOOL;
		info->far_ptr = & ((M_DiscSensor *) node)->isActive;
		return GF_OK;
	case 6:
		info->name = "rotation_changed";
		info->eventType = GF_SG_EVENT_OUT;
		info->fieldType = GF_SG_VRML_SFFLOAT;
		info->far_ptr = & ((M_DiscSensor *) node)->rotation_changed;
		return GF_OK;
	case 7:
		info->name = "trackPoint_changed";
		info->eventType = GF_SG_EVENT_OUT;
		info->fieldType = GF_SG_VRML_SFVEC2F;
		info->far_ptr = & ((M_DiscSensor *) node)->trackPoint_changed;
		return GF_OK;
	default:
		return GF_BAD_PARAM;
	}
}

static Bool DiscSensor_get_aq_info(GF_Node *n, u32 FieldIndex, u8 *QType, u8 *AType, Fixed *b_min, Fixed *b_max, u32 *QT13_bits)
{
	switch (FieldIndex) {
	case 2:
		*AType = 0;
		*QType = 6;
		*b_min = FLT2FIX(-6.2831853);
		*b_max = FLT2FIX(6.2831853);
		return 1;
	case 3:
		*AType = 0;
		*QType = 6;
		*b_min = FLT2FIX(-6.2831853);
		*b_max = FLT2FIX(6.2831853);
		return 1;
	case 4:
		*AType = 0;
		*QType = 6;
		*b_min = FLT2FIX(0);
		*b_max = FLT2FIX(6.2831853);
		return 1;
	default:
		return 0;
	}
}



GF_Node *DiscSensor_Create()
{
	M_DiscSensor *p;
	GF_SAFEALLOC(p, sizeof(M_DiscSensor));
	if(!p) return NULL;
	gf_node_setup((GF_Node *)p, TAG_MPEG4_DiscSensor);

#ifdef GF_NODE_USE_POINTERS
	((GF_Node *)p)->sgprivate->name = "DiscSensor";
	((GF_Node *)p)->sgprivate->node_del = DiscSensor_Del;
	((GF_Node *)p)->sgprivate->get_field_count = DiscSensor_get_field_count;
	((GF_Node *)p)->sgprivate->get_field = DiscSensor_get_field;
#endif


	/*default field values*/
	p->autoOffset = 1;
	p->enabled = 1;
	p->maxAngle = FLT2FIX(-1);
	p->minAngle = FLT2FIX(0);
	p->offset = FLT2FIX(0);
	return (GF_Node *)p;
}


/*
	ElevationGrid Node deletion
*/

static void ElevationGrid_Del(GF_Node *node)
{
	M_ElevationGrid *p = (M_ElevationGrid *) node;
	gf_sg_mffloat_del(p->set_height);
	gf_node_unregister((GF_Node *) p->color, (GF_Node *) p);	
	gf_node_unregister((GF_Node *) p->normal, (GF_Node *) p);	
	gf_node_unregister((GF_Node *) p->texCoord, (GF_Node *) p);	
	gf_sg_mffloat_del(p->height);
	gf_node_free((GF_Node *) p);
}

static const u16 ElevationGrid_Def2All[] = { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13};
static const u16 ElevationGrid_In2All[] = { 0, 1, 2, 3};
static const u16 ElevationGrid_Out2All[] = { 1, 2, 3};

static u32 ElevationGrid_get_field_count(GF_Node *node, u8 IndexMode)
{
	switch(IndexMode) {
	case GF_SG_FIELD_CODING_IN: return 4;
	case GF_SG_FIELD_CODING_DEF: return 13;
	case GF_SG_FIELD_CODING_OUT: return 3;
	case GF_SG_FIELD_CODING_DYN: return 0;
	default:
		return 14;
	}
}

static GF_Err ElevationGrid_get_field_index(GF_Node *n, u32 inField, u8 IndexMode, u32 *allField)
{
	switch(IndexMode) {
	case GF_SG_FIELD_CODING_IN:
		*allField = ElevationGrid_In2All[inField];
		return GF_OK;
	case GF_SG_FIELD_CODING_DEF:
		*allField = ElevationGrid_Def2All[inField];
		return GF_OK;
	case GF_SG_FIELD_CODING_OUT:
		*allField = ElevationGrid_Out2All[inField];
		return GF_OK;
	default:
		return GF_BAD_PARAM;
	}
}
static GF_Err ElevationGrid_get_field(GF_Node *node, GF_FieldInfo *info)
{
	switch (info->fieldIndex) {
	case 0:
		info->name = "set_height";
		info->eventType = GF_SG_EVENT_IN;
		info->on_event_in = ((M_ElevationGrid *)node)->on_set_height;
		info->fieldType = GF_SG_VRML_MFFLOAT;
		info->far_ptr = & ((M_ElevationGrid *) node)->set_height;
		return GF_OK;
	case 1:
		info->name = "color";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFNODE;
		info->NDTtype = NDT_SFColorNode;
		info->far_ptr = & ((M_ElevationGrid *)node)->color;
		return GF_OK;
	case 2:
		info->name = "normal";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFNODE;
		info->NDTtype = NDT_SFNormalNode;
		info->far_ptr = & ((M_ElevationGrid *)node)->normal;
		return GF_OK;
	case 3:
		info->name = "texCoord";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFNODE;
		info->NDTtype = NDT_SFTextureCoordinateNode;
		info->far_ptr = & ((M_ElevationGrid *)node)->texCoord;
		return GF_OK;
	case 4:
		info->name = "height";
		info->eventType = GF_SG_EVENT_FIELD;
		info->fieldType = GF_SG_VRML_MFFLOAT;
		info->far_ptr = & ((M_ElevationGrid *) node)->height;
		return GF_OK;
	case 5:
		info->name = "ccw";
		info->eventType = GF_SG_EVENT_FIELD;
		info->fieldType = GF_SG_VRML_SFBOOL;
		info->far_ptr = & ((M_ElevationGrid *) node)->ccw;
		return GF_OK;
	case 6:
		info->name = "colorPerVertex";
		info->eventType = GF_SG_EVENT_FIELD;
		info->fieldType = GF_SG_VRML_SFBOOL;
		info->far_ptr = & ((M_ElevationGrid *) node)->colorPerVertex;
		return GF_OK;
	case 7:
		info->name = "creaseAngle";
		info->eventType = GF_SG_EVENT_FIELD;
		info->fieldType = GF_SG_VRML_SFFLOAT;
		info->far_ptr = & ((M_ElevationGrid *) node)->creaseAngle;
		return GF_OK;
	case 8:
		info->name = "normalPerVertex";
		info->eventType = GF_SG_EVENT_FIELD;
		info->fieldType = GF_SG_VRML_SFBOOL;
		info->far_ptr = & ((M_ElevationGrid *) node)->normalPerVertex;
		return GF_OK;
	case 9:
		info->name = "solid";
		info->eventType = GF_SG_EVENT_FIELD;
		info->fieldType = GF_SG_VRML_SFBOOL;
		info->far_ptr = & ((M_ElevationGrid *) node)->solid;
		return GF_OK;
	case 10:
		info->name = "xDimension";
		info->eventType = GF_SG_EVENT_FIELD;
		info->fieldType = GF_SG_VRML_SFINT32;
		info->far_ptr = & ((M_ElevationGrid *) node)->xDimension;
		return GF_OK;
	case 11:
		info->name = "xSpacing";
		info->eventType = GF_SG_EVENT_FIELD;
		info->fieldType = GF_SG_VRML_SFFLOAT;
		info->far_ptr = & ((M_ElevationGrid *) node)->xSpacing;
		return GF_OK;
	case 12:
		info->name = "zDimension";
		info->eventType = GF_SG_EVENT_FIELD;
		info->fieldType = GF_SG_VRML_SFINT32;
		info->far_ptr = & ((M_ElevationGrid *) node)->zDimension;
		return GF_OK;
	case 13:
		info->name = "zSpacing";
		info->eventType = GF_SG_EVENT_FIELD;
		info->fieldType = GF_SG_VRML_SFFLOAT;
		info->far_ptr = & ((M_ElevationGrid *) node)->zSpacing;
		return GF_OK;
	default:
		return GF_BAD_PARAM;
	}
}

static Bool ElevationGrid_get_aq_info(GF_Node *n, u32 FieldIndex, u8 *QType, u8 *AType, Fixed *b_min, Fixed *b_max, u32 *QT13_bits)
{
	switch (FieldIndex) {
	case 4:
		*AType = 7;
		*QType = 11;
		*b_min = FIX_MIN;
		*b_max = FIX_MAX;
		return 1;
	case 7:
		*AType = 0;
		*QType = 6;
		*b_min = FLT2FIX(0);
		*b_max = FLT2FIX(6.2831853);
		return 1;
	case 10:
		*AType = 0;
		*QType = 11;
		*b_min = FLT2FIX(0);
		*b_max = FIX_MAX;
		return 1;
	case 11:
		*AType = 0;
		*QType = 11;
		*b_min = FLT2FIX(0);
		*b_max = FIX_MAX;
		return 1;
	case 12:
		*AType = 0;
		*QType = 11;
		*b_min = FLT2FIX(0);
		*b_max = FIX_MAX;
		return 1;
	case 13:
		*AType = 0;
		*QType = 11;
		*b_min = FLT2FIX(0);
		*b_max = FIX_MAX;
		return 1;
	default:
		return 0;
	}
}



GF_Node *ElevationGrid_Create()
{
	M_ElevationGrid *p;
	GF_SAFEALLOC(p, sizeof(M_ElevationGrid));
	if(!p) return NULL;
	gf_node_setup((GF_Node *)p, TAG_MPEG4_ElevationGrid);

#ifdef GF_NODE_USE_POINTERS
	((GF_Node *)p)->sgprivate->name = "ElevationGrid";
	((GF_Node *)p)->sgprivate->node_del = ElevationGrid_Del;
	((GF_Node *)p)->sgprivate->get_field_count = ElevationGrid_get_field_count;
	((GF_Node *)p)->sgprivate->get_field = ElevationGrid_get_field;
#endif


	/*default field values*/
	p->ccw = 1;
	p->colorPerVertex = 1;
	p->creaseAngle = FLT2FIX(0.0);
	p->normalPerVertex = 1;
	p->solid = 1;
	p->xDimension = 0;
	p->xSpacing = FLT2FIX(1.0);
	p->zDimension = 0;
	p->zSpacing = FLT2FIX(1.0);
	return (GF_Node *)p;
}


/*
	Extrusion Node deletion
*/

static void Extrusion_Del(GF_Node *node)
{
	M_Extrusion *p = (M_Extrusion *) node;
	gf_sg_mfvec2f_del(p->set_crossSection);
	gf_sg_mfrotation_del(p->set_orientation);
	gf_sg_mfvec2f_del(p->set_scale);
	gf_sg_mfvec3f_del(p->set_spine);
	gf_sg_mfvec2f_del(p->crossSection);
	gf_sg_mfrotation_del(p->orientation);
	gf_sg_mfvec2f_del(p->scale);
	gf_sg_mfvec3f_del(p->spine);
	gf_node_free((GF_Node *) p);
}

static const u16 Extrusion_Def2All[] = { 4, 5, 6, 7, 8, 9, 10, 11, 12, 13};
static const u16 Extrusion_In2All[] = { 0, 1, 2, 3};

static u32 Extrusion_get_field_count(GF_Node *node, u8 IndexMode)
{
	switch(IndexMode) {
	case GF_SG_FIELD_CODING_IN: return 4;
	case GF_SG_FIELD_CODING_DEF: return 10;
	case GF_SG_FIELD_CODING_OUT: return 0;
	case GF_SG_FIELD_CODING_DYN: return 0;
	default:
		return 14;
	}
}

static GF_Err Extrusion_get_field_index(GF_Node *n, u32 inField, u8 IndexMode, u32 *allField)
{
	switch(IndexMode) {
	case GF_SG_FIELD_CODING_IN:
		*allField = Extrusion_In2All[inField];
		return GF_OK;
	case GF_SG_FIELD_CODING_DEF:
		*allField = Extrusion_Def2All[inField];
		return GF_OK;
	default:
		return GF_BAD_PARAM;
	}
}
static GF_Err Extrusion_get_field(GF_Node *node, GF_FieldInfo *info)
{
	switch (info->fieldIndex) {
	case 0:
		info->name = "set_crossSection";
		info->eventType = GF_SG_EVENT_IN;
		info->on_event_in = ((M_Extrusion *)node)->on_set_crossSection;
		info->fieldType = GF_SG_VRML_MFVEC2F;
		info->far_ptr = & ((M_Extrusion *) node)->set_crossSection;
		return GF_OK;
	case 1:
		info->name = "set_orientation";
		info->eventType = GF_SG_EVENT_IN;
		info->on_event_in = ((M_Extrusion *)node)->on_set_orientation;
		info->fieldType = GF_SG_VRML_MFROTATION;
		info->far_ptr = & ((M_Extrusion *) node)->set_orientation;
		return GF_OK;
	case 2:
		info->name = "set_scale";
		info->eventType = GF_SG_EVENT_IN;
		info->on_event_in = ((M_Extrusion *)node)->on_set_scale;
		info->fieldType = GF_SG_VRML_MFVEC2F;
		info->far_ptr = & ((M_Extrusion *) node)->set_scale;
		return GF_OK;
	case 3:
		info->name = "set_spine";
		info->eventType = GF_SG_EVENT_IN;
		info->on_event_in = ((M_Extrusion *)node)->on_set_spine;
		info->fieldType = GF_SG_VRML_MFVEC3F;
		info->far_ptr = & ((M_Extrusion *) node)->set_spine;
		return GF_OK;
	case 4:
		info->name = "beginCap";
		info->eventType = GF_SG_EVENT_FIELD;
		info->fieldType = GF_SG_VRML_SFBOOL;
		info->far_ptr = & ((M_Extrusion *) node)->beginCap;
		return GF_OK;
	case 5:
		info->name = "ccw";
		info->eventType = GF_SG_EVENT_FIELD;
		info->fieldType = GF_SG_VRML_SFBOOL;
		info->far_ptr = & ((M_Extrusion *) node)->ccw;
		return GF_OK;
	case 6:
		info->name = "convex";
		info->eventType = GF_SG_EVENT_FIELD;
		info->fieldType = GF_SG_VRML_SFBOOL;
		info->far_ptr = & ((M_Extrusion *) node)->convex;
		return GF_OK;
	case 7:
		info->name = "creaseAngle";
		info->eventType = GF_SG_EVENT_FIELD;
		info->fieldType = GF_SG_VRML_SFFLOAT;
		info->far_ptr = & ((M_Extrusion *) node)->creaseAngle;
		return GF_OK;
	case 8:
		info->name = "crossSection";
		info->eventType = GF_SG_EVENT_FIELD;
		info->fieldType = GF_SG_VRML_MFVEC2F;
		info->far_ptr = & ((M_Extrusion *) node)->crossSection;
		return GF_OK;
	case 9:
		info->name = "endCap";
		info->eventType = GF_SG_EVENT_FIELD;
		info->fieldType = GF_SG_VRML_SFBOOL;
		info->far_ptr = & ((M_Extrusion *) node)->endCap;
		return GF_OK;
	case 10:
		info->name = "orientation";
		info->eventType = GF_SG_EVENT_FIELD;
		info->fieldType = GF_SG_VRML_MFROTATION;
		info->far_ptr = & ((M_Extrusion *) node)->orientation;
		return GF_OK;
	case 11:
		info->name = "scale";
		info->eventType = GF_SG_EVENT_FIELD;
		info->fieldType = GF_SG_VRML_MFVEC2F;
		info->far_ptr = & ((M_Extrusion *) node)->scale;
		return GF_OK;
	case 12:
		info->name = "solid";
		info->eventType = GF_SG_EVENT_FIELD;
		info->fieldType = GF_SG_VRML_SFBOOL;
		info->far_ptr = & ((M_Extrusion *) node)->solid;
		return GF_OK;
	case 13:
		info->name = "spine";
		info->eventType = GF_SG_EVENT_FIELD;
		info->fieldType = GF_SG_VRML_MFVEC3F;
		info->far_ptr = & ((M_Extrusion *) node)->spine;
		return GF_OK;
	default:
		return GF_BAD_PARAM;
	}
}

static Bool Extrusion_get_aq_info(GF_Node *n, u32 FieldIndex, u8 *QType, u8 *AType, Fixed *b_min, Fixed *b_max, u32 *QT13_bits)
{
	switch (FieldIndex) {
	case 7:
		*AType = 0;
		*QType = 6;
		*b_min = FLT2FIX(0);
		*b_max = FLT2FIX(6.2831853);
		return 1;
	case 8:
		*AType = 0;
		*QType = 2;
		*b_min = FIX_MIN;
		*b_max = FIX_MAX;
		return 1;
	case 10:
		*AType = 0;
		*QType = 10;
		*b_min = FIX_MIN;
		*b_max = FIX_MAX;
		return 1;
	case 11:
		*AType = 0;
		*QType = 7;
		*b_min = FLT2FIX(0);
		*b_max = FIX_MAX;
		return 1;
	case 13:
		*AType = 0;
		*QType = 1;
		*b_min = FIX_MIN;
		*b_max = FIX_MAX;
		return 1;
	default:
		return 0;
	}
}



GF_Node *Extrusion_Create()
{
	M_Extrusion *p;
	GF_SAFEALLOC(p, sizeof(M_Extrusion));
	if(!p) return NULL;
	gf_node_setup((GF_Node *)p, TAG_MPEG4_Extrusion);

#ifdef GF_NODE_USE_POINTERS
	((GF_Node *)p)->sgprivate->name = "Extrusion";
	((GF_Node *)p)->sgprivate->node_del = Extrusion_Del;
	((GF_Node *)p)->sgprivate->get_field_count = Extrusion_get_field_count;
	((GF_Node *)p)->sgprivate->get_field = Extrusion_get_field;
#endif


	/*default field values*/
	p->beginCap = 1;
	p->ccw = 1;
	p->convex = 1;
	p->creaseAngle = FLT2FIX(0.0);
	p->crossSection.vals = malloc(sizeof(SFVec2f)*5);
	p->crossSection.count = 5;
	p->crossSection.vals[0].x = FLT2FIX(1);
	p->crossSection.vals[0].y = FLT2FIX(1);
	p->crossSection.vals[1].x = FLT2FIX(1);
	p->crossSection.vals[1].y = FLT2FIX(-1);
	p->crossSection.vals[2].x = FLT2FIX(-1);
	p->crossSection.vals[2].y = FLT2FIX(-1);
	p->crossSection.vals[3].x = FLT2FIX(-1);
	p->crossSection.vals[3].y = FLT2FIX(1);
	p->crossSection.vals[4].x = FLT2FIX(1);
	p->crossSection.vals[4].y = FLT2FIX(1);
	p->endCap = 1;
	p->orientation.vals = malloc(sizeof(GF_Vec4)*1);
	p->orientation.count = 1;
	p->orientation.vals[0].x = FLT2FIX(0);
	p->orientation.vals[0].y = FLT2FIX(0);
	p->orientation.vals[0].z = FLT2FIX(1);
	p->orientation.vals[0].q = FLT2FIX(0);
	p->scale.vals = malloc(sizeof(SFVec2f)*1);
	p->scale.count = 1;
	p->scale.vals[0].x = FLT2FIX(1);
	p->scale.vals[0].y = FLT2FIX(1);
	p->solid = 1;
	p->spine.vals = malloc(sizeof(SFVec3f)*2);
	p->spine.count = 2;
	p->spine.vals[0].x = FLT2FIX(0);
	p->spine.vals[0].y = FLT2FIX(0);
	p->spine.vals[0].z = FLT2FIX(0);
	p->spine.vals[1].x = FLT2FIX(0);
	p->spine.vals[1].y = FLT2FIX(1);
	p->spine.vals[1].z = FLT2FIX(0);
	return (GF_Node *)p;
}


/*
	Fog Node deletion
*/

static void Fog_Del(GF_Node *node)
{
	M_Fog *p = (M_Fog *) node;
	gf_sg_sfstring_del(p->fogType);
	gf_node_free((GF_Node *) p);
}

static const u16 Fog_Def2All[] = { 0, 1, 2};
static const u16 Fog_In2All[] = { 0, 1, 2, 3};
static const u16 Fog_Out2All[] = { 0, 1, 2, 4};
static const u16 Fog_Dyn2All[] = { 0, 2};

static u32 Fog_get_field_count(GF_Node *node, u8 IndexMode)
{
	switch(IndexMode) {
	case GF_SG_FIELD_CODING_IN: return 4;
	case GF_SG_FIELD_CODING_DEF: return 3;
	case GF_SG_FIELD_CODING_OUT: return 4;
	case GF_SG_FIELD_CODING_DYN: return 2;
	default:
		return 5;
	}
}

static GF_Err Fog_get_field_index(GF_Node *n, u32 inField, u8 IndexMode, u32 *allField)
{
	switch(IndexMode) {
	case GF_SG_FIELD_CODING_IN:
		*allField = Fog_In2All[inField];
		return GF_OK;
	case GF_SG_FIELD_CODING_DEF:
		*allField = Fog_Def2All[inField];
		return GF_OK;
	case GF_SG_FIELD_CODING_OUT:
		*allField = Fog_Out2All[inField];
		return GF_OK;
	case GF_SG_FIELD_CODING_DYN:
		*allField = Fog_Dyn2All[inField];
		return GF_OK;
	default:
		return GF_BAD_PARAM;
	}
}
static GF_Err Fog_get_field(GF_Node *node, GF_FieldInfo *info)
{
	switch (info->fieldIndex) {
	case 0:
		info->name = "color";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFCOLOR;
		info->far_ptr = & ((M_Fog *) node)->color;
		return GF_OK;
	case 1:
		info->name = "fogType";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFSTRING;
		info->far_ptr = & ((M_Fog *) node)->fogType;
		return GF_OK;
	case 2:
		info->name = "visibilityRange";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFFLOAT;
		info->far_ptr = & ((M_Fog *) node)->visibilityRange;
		return GF_OK;
	case 3:
		info->name = "set_bind";
		info->eventType = GF_SG_EVENT_IN;
		info->on_event_in = ((M_Fog *)node)->on_set_bind;
		info->fieldType = GF_SG_VRML_SFBOOL;
		info->far_ptr = & ((M_Fog *) node)->set_bind;
		return GF_OK;
	case 4:
		info->name = "isBound";
		info->eventType = GF_SG_EVENT_OUT;
		info->fieldType = GF_SG_VRML_SFBOOL;
		info->far_ptr = & ((M_Fog *) node)->isBound;
		return GF_OK;
	default:
		return GF_BAD_PARAM;
	}
}

static Bool Fog_get_aq_info(GF_Node *n, u32 FieldIndex, u8 *QType, u8 *AType, Fixed *b_min, Fixed *b_max, u32 *QT13_bits)
{
	switch (FieldIndex) {
	case 0:
		*AType = 4;
		*QType = 4;
		*b_min = FLT2FIX(0);
		*b_max = FLT2FIX(1);
		return 1;
	case 2:
		*AType = 7;
		*QType = 11;
		*b_min = FLT2FIX(0);
		*b_max = FIX_MAX;
		return 1;
	default:
		return 0;
	}
}



GF_Node *Fog_Create()
{
	M_Fog *p;
	GF_SAFEALLOC(p, sizeof(M_Fog));
	if(!p) return NULL;
	gf_node_setup((GF_Node *)p, TAG_MPEG4_Fog);

#ifdef GF_NODE_USE_POINTERS
	((GF_Node *)p)->sgprivate->name = "Fog";
	((GF_Node *)p)->sgprivate->node_del = Fog_Del;
	((GF_Node *)p)->sgprivate->get_field_count = Fog_get_field_count;
	((GF_Node *)p)->sgprivate->get_field = Fog_get_field;
#endif


	/*default field values*/
	p->color.red = FLT2FIX(1);
	p->color.green = FLT2FIX(1);
	p->color.blue = FLT2FIX(1);
	p->fogType.buffer = malloc(sizeof(char) * 7);
	strcpy(p->fogType.buffer, "LINEAR");
	p->visibilityRange = FLT2FIX(0);
	return (GF_Node *)p;
}


/*
	FontStyle Node deletion
*/

static void FontStyle_Del(GF_Node *node)
{
	M_FontStyle *p = (M_FontStyle *) node;
	gf_sg_mfstring_del(p->family);
	gf_sg_mfstring_del(p->justify);
	gf_sg_sfstring_del(p->language);
	gf_sg_sfstring_del(p->style);
	gf_node_free((GF_Node *) p);
}

static const u16 FontStyle_Def2All[] = { 0, 1, 2, 3, 4, 5, 6, 7, 8};
static const u16 FontStyle_In2All[] = { 0, 1, 2, 3, 4, 5, 6, 7, 8};
static const u16 FontStyle_Out2All[] = { 0, 1, 2, 3, 4, 5, 6, 7, 8};

static u32 FontStyle_get_field_count(GF_Node *node, u8 IndexMode)
{
	switch(IndexMode) {
	case GF_SG_FIELD_CODING_IN: return 9;
	case GF_SG_FIELD_CODING_DEF: return 9;
	case GF_SG_FIELD_CODING_OUT: return 9;
	case GF_SG_FIELD_CODING_DYN: return 0;
	default:
		return 9;
	}
}

static GF_Err FontStyle_get_field_index(GF_Node *n, u32 inField, u8 IndexMode, u32 *allField)
{
	switch(IndexMode) {
	case GF_SG_FIELD_CODING_IN:
		*allField = FontStyle_In2All[inField];
		return GF_OK;
	case GF_SG_FIELD_CODING_DEF:
		*allField = FontStyle_Def2All[inField];
		return GF_OK;
	case GF_SG_FIELD_CODING_OUT:
		*allField = FontStyle_Out2All[inField];
		return GF_OK;
	default:
		return GF_BAD_PARAM;
	}
}
static GF_Err FontStyle_get_field(GF_Node *node, GF_FieldInfo *info)
{
	switch (info->fieldIndex) {
	case 0:
		info->name = "family";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_MFSTRING;
		info->far_ptr = & ((M_FontStyle *) node)->family;
		return GF_OK;
	case 1:
		info->name = "horizontal";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFBOOL;
		info->far_ptr = & ((M_FontStyle *) node)->horizontal;
		return GF_OK;
	case 2:
		info->name = "justify";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_MFSTRING;
		info->far_ptr = & ((M_FontStyle *) node)->justify;
		return GF_OK;
	case 3:
		info->name = "language";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFSTRING;
		info->far_ptr = & ((M_FontStyle *) node)->language;
		return GF_OK;
	case 4:
		info->name = "leftToRight";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFBOOL;
		info->far_ptr = & ((M_FontStyle *) node)->leftToRight;
		return GF_OK;
	case 5:
		info->name = "size";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFFLOAT;
		info->far_ptr = & ((M_FontStyle *) node)->size;
		return GF_OK;
	case 6:
		info->name = "spacing";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFFLOAT;
		info->far_ptr = & ((M_FontStyle *) node)->spacing;
		return GF_OK;
	case 7:
		info->name = "style";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFSTRING;
		info->far_ptr = & ((M_FontStyle *) node)->style;
		return GF_OK;
	case 8:
		info->name = "topToBottom";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFBOOL;
		info->far_ptr = & ((M_FontStyle *) node)->topToBottom;
		return GF_OK;
	default:
		return GF_BAD_PARAM;
	}
}

static Bool FontStyle_get_aq_info(GF_Node *n, u32 FieldIndex, u8 *QType, u8 *AType, Fixed *b_min, Fixed *b_max, u32 *QT13_bits)
{
	switch (FieldIndex) {
	case 5:
		*AType = 0;
		*QType = 11;
		*b_min = FLT2FIX(0);
		*b_max = FIX_MAX;
		return 1;
	case 6:
		*AType = 0;
		*QType = 11;
		*b_min = FLT2FIX(0);
		*b_max = FIX_MAX;
		return 1;
	default:
		return 0;
	}
}



GF_Node *FontStyle_Create()
{
	M_FontStyle *p;
	GF_SAFEALLOC(p, sizeof(M_FontStyle));
	if(!p) return NULL;
	gf_node_setup((GF_Node *)p, TAG_MPEG4_FontStyle);

#ifdef GF_NODE_USE_POINTERS
	((GF_Node *)p)->sgprivate->name = "FontStyle";
	((GF_Node *)p)->sgprivate->node_del = FontStyle_Del;
	((GF_Node *)p)->sgprivate->get_field_count = FontStyle_get_field_count;
	((GF_Node *)p)->sgprivate->get_field = FontStyle_get_field;
#endif


	/*default field values*/
	p->family.vals = malloc(sizeof(SFString)*1);
	p->family.count = 1;
	p->family.vals[0] = malloc(sizeof(char) * 6);
	strcpy(p->family.vals[0], "SERIF");
	p->horizontal = 1;
	p->justify.vals = malloc(sizeof(SFString)*1);
	p->justify.count = 1;
	p->justify.vals[0] = malloc(sizeof(char) * 6);
	strcpy(p->justify.vals[0], "BEGIN");
	p->leftToRight = 1;
	p->size = FLT2FIX(1.0);
	p->spacing = FLT2FIX(1.0);
	p->style.buffer = malloc(sizeof(char) * 6);
	strcpy(p->style.buffer, "PLAIN");
	p->topToBottom = 1;
	return (GF_Node *)p;
}


/*
	Form Node deletion
*/

static void Form_Del(GF_Node *node)
{
	M_Form *p = (M_Form *) node;
	gf_sg_vrml_parent_reset((GF_Node *) p);	
	gf_sg_mfint32_del(p->groups);
	gf_sg_mfstring_del(p->constraints);
	gf_sg_mfint32_del(p->groupsIndex);
	gf_node_free((GF_Node *) p);
}

static const u16 Form_Def2All[] = { 2, 3, 4, 5, 6};
static const u16 Form_In2All[] = { 0, 1, 2, 3, 4, 5, 6};
static const u16 Form_Out2All[] = { 2, 3, 4, 5, 6};
static const u16 Form_Dyn2All[] = { 3};

static u32 Form_get_field_count(GF_Node *node, u8 IndexMode)
{
	switch(IndexMode) {
	case GF_SG_FIELD_CODING_IN: return 7;
	case GF_SG_FIELD_CODING_DEF: return 5;
	case GF_SG_FIELD_CODING_OUT: return 5;
	case GF_SG_FIELD_CODING_DYN: return 1;
	default:
		return 7;
	}
}

static GF_Err Form_get_field_index(GF_Node *n, u32 inField, u8 IndexMode, u32 *allField)
{
	switch(IndexMode) {
	case GF_SG_FIELD_CODING_IN:
		*allField = Form_In2All[inField];
		return GF_OK;
	case GF_SG_FIELD_CODING_DEF:
		*allField = Form_Def2All[inField];
		return GF_OK;
	case GF_SG_FIELD_CODING_OUT:
		*allField = Form_Out2All[inField];
		return GF_OK;
	case GF_SG_FIELD_CODING_DYN:
		*allField = Form_Dyn2All[inField];
		return GF_OK;
	default:
		return GF_BAD_PARAM;
	}
}
static GF_Err Form_get_field(GF_Node *node, GF_FieldInfo *info)
{
	switch (info->fieldIndex) {
	case 0:
		info->name = "addChildren";
		info->eventType = GF_SG_EVENT_IN;
		info->on_event_in = ((M_Form *)node)->on_addChildren;
		info->fieldType = GF_SG_VRML_MFNODE;
		info->NDTtype = NDT_SF2DNode;
		info->far_ptr = & ((M_Form *)node)->addChildren;
		return GF_OK;
	case 1:
		info->name = "removeChildren";
		info->eventType = GF_SG_EVENT_IN;
		info->on_event_in = ((M_Form *)node)->on_removeChildren;
		info->fieldType = GF_SG_VRML_MFNODE;
		info->NDTtype = NDT_SF2DNode;
		info->far_ptr = & ((M_Form *)node)->removeChildren;
		return GF_OK;
	case 2:
		info->name = "children";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_MFNODE;
		info->NDTtype = NDT_SF2DNode;
		info->far_ptr = & ((M_Form *)node)->children;
		return GF_OK;
	case 3:
		info->name = "size";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFVEC2F;
		info->far_ptr = & ((M_Form *) node)->size;
		return GF_OK;
	case 4:
		info->name = "groups";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_MFINT32;
		info->far_ptr = & ((M_Form *) node)->groups;
		return GF_OK;
	case 5:
		info->name = "constraints";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_MFSTRING;
		info->far_ptr = & ((M_Form *) node)->constraints;
		return GF_OK;
	case 6:
		info->name = "groupsIndex";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_MFINT32;
		info->far_ptr = & ((M_Form *) node)->groupsIndex;
		return GF_OK;
	default:
		return GF_BAD_PARAM;
	}
}

static Bool Form_get_aq_info(GF_Node *n, u32 FieldIndex, u8 *QType, u8 *AType, Fixed *b_min, Fixed *b_max, u32 *QT13_bits)
{
	switch (FieldIndex) {
	case 3:
		*AType = 12;
		*QType = 12;
		*b_min = FLT2FIX(0);
		*b_max = FIX_MAX;
		return 1;
	case 4:
		*AType = 0;
		*QType = 13;
		*QT13_bits = 10;
		*b_min = FLT2FIX(-1);
		*b_max = FLT2FIX(1022);
		return 1;
	case 6:
		*AType = 0;
		*QType = 13;
		*QT13_bits = 10;
		*b_min = FLT2FIX(-1);
		*b_max = FLT2FIX(1022);
		return 1;
	default:
		return 0;
	}
}



GF_Node *Form_Create()
{
	M_Form *p;
	GF_SAFEALLOC(p, sizeof(M_Form));
	if(!p) return NULL;
	gf_node_setup((GF_Node *)p, TAG_MPEG4_Form);
	gf_sg_vrml_parent_setup((GF_Node *) p);

#ifdef GF_NODE_USE_POINTERS
	((GF_Node *)p)->sgprivate->name = "Form";
	((GF_Node *)p)->sgprivate->node_del = Form_Del;
	((GF_Node *)p)->sgprivate->get_field_count = Form_get_field_count;
	((GF_Node *)p)->sgprivate->get_field = Form_get_field;
#endif


	/*default field values*/
	p->size.x = FLT2FIX(-1);
	p->size.y = FLT2FIX(-1);
	return (GF_Node *)p;
}


/*
	Group Node deletion
*/

static void Group_Del(GF_Node *node)
{
	M_Group *p = (M_Group *) node;
	gf_sg_vrml_parent_reset((GF_Node *) p);	
	gf_node_free((GF_Node *) p);
}

static const u16 Group_Def2All[] = { 2};
static const u16 Group_In2All[] = { 0, 1, 2};
static const u16 Group_Out2All[] = { 2};

static u32 Group_get_field_count(GF_Node *node, u8 IndexMode)
{
	switch(IndexMode) {
	case GF_SG_FIELD_CODING_IN: return 3;
	case GF_SG_FIELD_CODING_DEF: return 1;
	case GF_SG_FIELD_CODING_OUT: return 1;
	case GF_SG_FIELD_CODING_DYN: return 0;
	default:
		return 3;
	}
}

static GF_Err Group_get_field_index(GF_Node *n, u32 inField, u8 IndexMode, u32 *allField)
{
	switch(IndexMode) {
	case GF_SG_FIELD_CODING_IN:
		*allField = Group_In2All[inField];
		return GF_OK;
	case GF_SG_FIELD_CODING_DEF:
		*allField = Group_Def2All[inField];
		return GF_OK;
	case GF_SG_FIELD_CODING_OUT:
		*allField = Group_Out2All[inField];
		return GF_OK;
	default:
		return GF_BAD_PARAM;
	}
}
static GF_Err Group_get_field(GF_Node *node, GF_FieldInfo *info)
{
	switch (info->fieldIndex) {
	case 0:
		info->name = "addChildren";
		info->eventType = GF_SG_EVENT_IN;
		info->on_event_in = ((M_Group *)node)->on_addChildren;
		info->fieldType = GF_SG_VRML_MFNODE;
		info->NDTtype = NDT_SF3DNode;
		info->far_ptr = & ((M_Group *)node)->addChildren;
		return GF_OK;
	case 1:
		info->name = "removeChildren";
		info->eventType = GF_SG_EVENT_IN;
		info->on_event_in = ((M_Group *)node)->on_removeChildren;
		info->fieldType = GF_SG_VRML_MFNODE;
		info->NDTtype = NDT_SF3DNode;
		info->far_ptr = & ((M_Group *)node)->removeChildren;
		return GF_OK;
	case 2:
		info->name = "children";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_MFNODE;
		info->NDTtype = NDT_SF3DNode;
		info->far_ptr = & ((M_Group *)node)->children;
		return GF_OK;
	default:
		return GF_BAD_PARAM;
	}
}

static Bool Group_get_aq_info(GF_Node *n, u32 FieldIndex, u8 *QType, u8 *AType, Fixed *b_min, Fixed *b_max, u32 *QT13_bits)
{
	switch (FieldIndex) {
	default:
		return 0;
	}
}



GF_Node *Group_Create()
{
	M_Group *p;
	GF_SAFEALLOC(p, sizeof(M_Group));
	if(!p) return NULL;
	gf_node_setup((GF_Node *)p, TAG_MPEG4_Group);
	gf_sg_vrml_parent_setup((GF_Node *) p);

#ifdef GF_NODE_USE_POINTERS
	((GF_Node *)p)->sgprivate->name = "Group";
	((GF_Node *)p)->sgprivate->node_del = Group_Del;
	((GF_Node *)p)->sgprivate->get_field_count = Group_get_field_count;
	((GF_Node *)p)->sgprivate->get_field = Group_get_field;
#endif


	/*default field values*/
	return (GF_Node *)p;
}


/*
	ImageTexture Node deletion
*/

static void ImageTexture_Del(GF_Node *node)
{
	M_ImageTexture *p = (M_ImageTexture *) node;
	gf_sg_mfurl_del(p->url);
	gf_node_free((GF_Node *) p);
}

static const u16 ImageTexture_Def2All[] = { 0, 1, 2};
static const u16 ImageTexture_In2All[] = { 0};
static const u16 ImageTexture_Out2All[] = { 0};

static u32 ImageTexture_get_field_count(GF_Node *node, u8 IndexMode)
{
	switch(IndexMode) {
	case GF_SG_FIELD_CODING_IN: return 1;
	case GF_SG_FIELD_CODING_DEF: return 3;
	case GF_SG_FIELD_CODING_OUT: return 1;
	case GF_SG_FIELD_CODING_DYN: return 0;
	default:
		return 3;
	}
}

static GF_Err ImageTexture_get_field_index(GF_Node *n, u32 inField, u8 IndexMode, u32 *allField)
{
	switch(IndexMode) {
	case GF_SG_FIELD_CODING_IN:
		*allField = ImageTexture_In2All[inField];
		return GF_OK;
	case GF_SG_FIELD_CODING_DEF:
		*allField = ImageTexture_Def2All[inField];
		return GF_OK;
	case GF_SG_FIELD_CODING_OUT:
		*allField = ImageTexture_Out2All[inField];
		return GF_OK;
	default:
		return GF_BAD_PARAM;
	}
}
static GF_Err ImageTexture_get_field(GF_Node *node, GF_FieldInfo *info)
{
	switch (info->fieldIndex) {
	case 0:
		info->name = "url";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_MFURL;
		info->far_ptr = & ((M_ImageTexture *) node)->url;
		return GF_OK;
	case 1:
		info->name = "repeatS";
		info->eventType = GF_SG_EVENT_FIELD;
		info->fieldType = GF_SG_VRML_SFBOOL;
		info->far_ptr = & ((M_ImageTexture *) node)->repeatS;
		return GF_OK;
	case 2:
		info->name = "repeatT";
		info->eventType = GF_SG_EVENT_FIELD;
		info->fieldType = GF_SG_VRML_SFBOOL;
		info->far_ptr = & ((M_ImageTexture *) node)->repeatT;
		return GF_OK;
	default:
		return GF_BAD_PARAM;
	}
}

static Bool ImageTexture_get_aq_info(GF_Node *n, u32 FieldIndex, u8 *QType, u8 *AType, Fixed *b_min, Fixed *b_max, u32 *QT13_bits)
{
	switch (FieldIndex) {
	default:
		return 0;
	}
}



GF_Node *ImageTexture_Create()
{
	M_ImageTexture *p;
	GF_SAFEALLOC(p, sizeof(M_ImageTexture));
	if(!p) return NULL;
	gf_node_setup((GF_Node *)p, TAG_MPEG4_ImageTexture);

#ifdef GF_NODE_USE_POINTERS
	((GF_Node *)p)->sgprivate->name = "ImageTexture";
	((GF_Node *)p)->sgprivate->node_del = ImageTexture_Del;
	((GF_Node *)p)->sgprivate->get_field_count = ImageTexture_get_field_count;
	((GF_Node *)p)->sgprivate->get_field = ImageTexture_get_field;
#endif


	/*default field values*/
	p->repeatS = 1;
	p->repeatT = 1;
	return (GF_Node *)p;
}


/*
	IndexedFaceSet Node deletion
*/

static void IndexedFaceSet_Del(GF_Node *node)
{
	M_IndexedFaceSet *p = (M_IndexedFaceSet *) node;
	gf_sg_mfint32_del(p->set_colorIndex);
	gf_sg_mfint32_del(p->set_coordIndex);
	gf_sg_mfint32_del(p->set_normalIndex);
	gf_sg_mfint32_del(p->set_texCoordIndex);
	gf_node_unregister((GF_Node *) p->color, (GF_Node *) p);	
	gf_node_unregister((GF_Node *) p->coord, (GF_Node *) p);	
	gf_node_unregister((GF_Node *) p->normal, (GF_Node *) p);	
	gf_node_unregister((GF_Node *) p->texCoord, (GF_Node *) p);	
	gf_sg_mfint32_del(p->colorIndex);
	gf_sg_mfint32_del(p->coordIndex);
	gf_sg_mfint32_del(p->normalIndex);
	gf_sg_mfint32_del(p->texCoordIndex);
	gf_node_free((GF_Node *) p);
}

static const u16 IndexedFaceSet_Def2All[] = { 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17};
static const u16 IndexedFaceSet_In2All[] = { 0, 1, 2, 3, 4, 5, 6, 7};
static const u16 IndexedFaceSet_Out2All[] = { 4, 5, 6, 7};

static u32 IndexedFaceSet_get_field_count(GF_Node *node, u8 IndexMode)
{
	switch(IndexMode) {
	case GF_SG_FIELD_CODING_IN: return 8;
	case GF_SG_FIELD_CODING_DEF: return 14;
	case GF_SG_FIELD_CODING_OUT: return 4;
	case GF_SG_FIELD_CODING_DYN: return 0;
	default:
		return 18;
	}
}

static GF_Err IndexedFaceSet_get_field_index(GF_Node *n, u32 inField, u8 IndexMode, u32 *allField)
{
	switch(IndexMode) {
	case GF_SG_FIELD_CODING_IN:
		*allField = IndexedFaceSet_In2All[inField];
		return GF_OK;
	case GF_SG_FIELD_CODING_DEF:
		*allField = IndexedFaceSet_Def2All[inField];
		return GF_OK;
	case GF_SG_FIELD_CODING_OUT:
		*allField = IndexedFaceSet_Out2All[inField];
		return GF_OK;
	default:
		return GF_BAD_PARAM;
	}
}
static GF_Err IndexedFaceSet_get_field(GF_Node *node, GF_FieldInfo *info)
{
	switch (info->fieldIndex) {
	case 0:
		info->name = "set_colorIndex";
		info->eventType = GF_SG_EVENT_IN;
		info->on_event_in = ((M_IndexedFaceSet *)node)->on_set_colorIndex;
		info->fieldType = GF_SG_VRML_MFINT32;
		info->far_ptr = & ((M_IndexedFaceSet *) node)->set_colorIndex;
		return GF_OK;
	case 1:
		info->name = "set_coordIndex";
		info->eventType = GF_SG_EVENT_IN;
		info->on_event_in = ((M_IndexedFaceSet *)node)->on_set_coordIndex;
		info->fieldType = GF_SG_VRML_MFINT32;
		info->far_ptr = & ((M_IndexedFaceSet *) node)->set_coordIndex;
		return GF_OK;
	case 2:
		info->name = "set_normalIndex";
		info->eventType = GF_SG_EVENT_IN;
		info->on_event_in = ((M_IndexedFaceSet *)node)->on_set_normalIndex;
		info->fieldType = GF_SG_VRML_MFINT32;
		info->far_ptr = & ((M_IndexedFaceSet *) node)->set_normalIndex;
		return GF_OK;
	case 3:
		info->name = "set_texCoordIndex";
		info->eventType = GF_SG_EVENT_IN;
		info->on_event_in = ((M_IndexedFaceSet *)node)->on_set_texCoordIndex;
		info->fieldType = GF_SG_VRML_MFINT32;
		info->far_ptr = & ((M_IndexedFaceSet *) node)->set_texCoordIndex;
		return GF_OK;
	case 4:
		info->name = "color";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFNODE;
		info->NDTtype = NDT_SFColorNode;
		info->far_ptr = & ((M_IndexedFaceSet *)node)->color;
		return GF_OK;
	case 5:
		info->name = "coord";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFNODE;
		info->NDTtype = NDT_SFCoordinateNode;
		info->far_ptr = & ((M_IndexedFaceSet *)node)->coord;
		return GF_OK;
	case 6:
		info->name = "normal";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFNODE;
		info->NDTtype = NDT_SFNormalNode;
		info->far_ptr = & ((M_IndexedFaceSet *)node)->normal;
		return GF_OK;
	case 7:
		info->name = "texCoord";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFNODE;
		info->NDTtype = NDT_SFTextureCoordinateNode;
		info->far_ptr = & ((M_IndexedFaceSet *)node)->texCoord;
		return GF_OK;
	case 8:
		info->name = "ccw";
		info->eventType = GF_SG_EVENT_FIELD;
		info->fieldType = GF_SG_VRML_SFBOOL;
		info->far_ptr = & ((M_IndexedFaceSet *) node)->ccw;
		return GF_OK;
	case 9:
		info->name = "colorIndex";
		info->eventType = GF_SG_EVENT_FIELD;
		info->fieldType = GF_SG_VRML_MFINT32;
		info->far_ptr = & ((M_IndexedFaceSet *) node)->colorIndex;
		return GF_OK;
	case 10:
		info->name = "colorPerVertex";
		info->eventType = GF_SG_EVENT_FIELD;
		info->fieldType = GF_SG_VRML_SFBOOL;
		info->far_ptr = & ((M_IndexedFaceSet *) node)->colorPerVertex;
		return GF_OK;
	case 11:
		info->name = "convex";
		info->eventType = GF_SG_EVENT_FIELD;
		info->fieldType = GF_SG_VRML_SFBOOL;
		info->far_ptr = & ((M_IndexedFaceSet *) node)->convex;
		return GF_OK;
	case 12:
		info->name = "coordIndex";
		info->eventType = GF_SG_EVENT_FIELD;
		info->fieldType = GF_SG_VRML_MFINT32;
		info->far_ptr = & ((M_IndexedFaceSet *) node)->coordIndex;
		return GF_OK;
	case 13:
		info->name = "creaseAngle";
		info->eventType = GF_SG_EVENT_FIELD;
		info->fieldType = GF_SG_VRML_SFFLOAT;
		info->far_ptr = & ((M_IndexedFaceSet *) node)->creaseAngle;
		return GF_OK;
	case 14:
		info->name = "normalIndex";
		info->eventType = GF_SG_EVENT_FIELD;
		info->fieldType = GF_SG_VRML_MFINT32;
		info->far_ptr = & ((M_IndexedFaceSet *) node)->normalIndex;
		return GF_OK;
	case 15:
		info->name = "normalPerVertex";
		info->eventType = GF_SG_EVENT_FIELD;
		info->fieldType = GF_SG_VRML_SFBOOL;
		info->far_ptr = & ((M_IndexedFaceSet *) node)->normalPerVertex;
		return GF_OK;
	case 16:
		info->name = "solid";
		info->eventType = GF_SG_EVENT_FIELD;
		info->fieldType = GF_SG_VRML_SFBOOL;
		info->far_ptr = & ((M_IndexedFaceSet *) node)->solid;
		return GF_OK;
	case 17:
		info->name = "texCoordIndex";
		info->eventType = GF_SG_EVENT_FIELD;
		info->fieldType = GF_SG_VRML_MFINT32;
		info->far_ptr = & ((M_IndexedFaceSet *) node)->texCoordIndex;
		return GF_OK;
	default:
		return GF_BAD_PARAM;
	}
}

static Bool IndexedFaceSet_get_aq_info(GF_Node *n, u32 FieldIndex, u8 *QType, u8 *AType, Fixed *b_min, Fixed *b_max, u32 *QT13_bits)
{
	switch (FieldIndex) {
	case 9:
		*AType = 0;
		*QType = 14;
		*b_min = FLT2FIX(-1);
		*b_max = FIX_MAX;
		return 1;
	case 12:
		*AType = 0;
		*QType = 14;
		*b_min = FLT2FIX(-1);
		*b_max = FIX_MAX;
		return 1;
	case 13:
		*AType = 0;
		*QType = 6;
		*b_min = FLT2FIX(0);
		*b_max = FLT2FIX(6.2831853);
		return 1;
	case 14:
		*AType = 0;
		*QType = 14;
		*b_min = FLT2FIX(-1);
		*b_max = FIX_MAX;
		return 1;
	case 17:
		*AType = 0;
		*QType = 14;
		*b_min = FLT2FIX(-1);
		*b_max = FIX_MAX;
		return 1;
	default:
		return 0;
	}
}



GF_Node *IndexedFaceSet_Create()
{
	M_IndexedFaceSet *p;
	GF_SAFEALLOC(p, sizeof(M_IndexedFaceSet));
	if(!p) return NULL;
	gf_node_setup((GF_Node *)p, TAG_MPEG4_IndexedFaceSet);

#ifdef GF_NODE_USE_POINTERS
	((GF_Node *)p)->sgprivate->name = "IndexedFaceSet";
	((GF_Node *)p)->sgprivate->node_del = IndexedFaceSet_Del;
	((GF_Node *)p)->sgprivate->get_field_count = IndexedFaceSet_get_field_count;
	((GF_Node *)p)->sgprivate->get_field = IndexedFaceSet_get_field;
#endif


	/*default field values*/
	p->ccw = 1;
	p->colorPerVertex = 1;
	p->convex = 1;
	p->creaseAngle = FLT2FIX(0.0);
	p->normalPerVertex = 1;
	p->solid = 1;
	return (GF_Node *)p;
}


/*
	IndexedFaceSet2D Node deletion
*/

static void IndexedFaceSet2D_Del(GF_Node *node)
{
	M_IndexedFaceSet2D *p = (M_IndexedFaceSet2D *) node;
	gf_sg_mfint32_del(p->set_colorIndex);
	gf_sg_mfint32_del(p->set_coordIndex);
	gf_sg_mfint32_del(p->set_texCoordIndex);
	gf_node_unregister((GF_Node *) p->color, (GF_Node *) p);	
	gf_node_unregister((GF_Node *) p->coord, (GF_Node *) p);	
	gf_node_unregister((GF_Node *) p->texCoord, (GF_Node *) p);	
	gf_sg_mfint32_del(p->colorIndex);
	gf_sg_mfint32_del(p->coordIndex);
	gf_sg_mfint32_del(p->texCoordIndex);
	gf_node_free((GF_Node *) p);
}

static const u16 IndexedFaceSet2D_Def2All[] = { 3, 4, 5, 6, 7, 8, 9, 10};
static const u16 IndexedFaceSet2D_In2All[] = { 0, 1, 2, 3, 4, 5};
static const u16 IndexedFaceSet2D_Out2All[] = { 3, 4, 5};

static u32 IndexedFaceSet2D_get_field_count(GF_Node *node, u8 IndexMode)
{
	switch(IndexMode) {
	case GF_SG_FIELD_CODING_IN: return 6;
	case GF_SG_FIELD_CODING_DEF: return 8;
	case GF_SG_FIELD_CODING_OUT: return 3;
	case GF_SG_FIELD_CODING_DYN: return 0;
	default:
		return 11;
	}
}

static GF_Err IndexedFaceSet2D_get_field_index(GF_Node *n, u32 inField, u8 IndexMode, u32 *allField)
{
	switch(IndexMode) {
	case GF_SG_FIELD_CODING_IN:
		*allField = IndexedFaceSet2D_In2All[inField];
		return GF_OK;
	case GF_SG_FIELD_CODING_DEF:
		*allField = IndexedFaceSet2D_Def2All[inField];
		return GF_OK;
	case GF_SG_FIELD_CODING_OUT:
		*allField = IndexedFaceSet2D_Out2All[inField];
		return GF_OK;
	default:
		return GF_BAD_PARAM;
	}
}
static GF_Err IndexedFaceSet2D_get_field(GF_Node *node, GF_FieldInfo *info)
{
	switch (info->fieldIndex) {
	case 0:
		info->name = "set_colorIndex";
		info->eventType = GF_SG_EVENT_IN;
		info->on_event_in = ((M_IndexedFaceSet2D *)node)->on_set_colorIndex;
		info->fieldType = GF_SG_VRML_MFINT32;
		info->far_ptr = & ((M_IndexedFaceSet2D *) node)->set_colorIndex;
		return GF_OK;
	case 1:
		info->name = "set_coordIndex";
		info->eventType = GF_SG_EVENT_IN;
		info->on_event_in = ((M_IndexedFaceSet2D *)node)->on_set_coordIndex;
		info->fieldType = GF_SG_VRML_MFINT32;
		info->far_ptr = & ((M_IndexedFaceSet2D *) node)->set_coordIndex;
		return GF_OK;
	case 2:
		info->name = "set_texCoordIndex";
		info->eventType = GF_SG_EVENT_IN;
		info->on_event_in = ((M_IndexedFaceSet2D *)node)->on_set_texCoordIndex;
		info->fieldType = GF_SG_VRML_MFINT32;
		info->far_ptr = & ((M_IndexedFaceSet2D *) node)->set_texCoordIndex;
		return GF_OK;
	case 3:
		info->name = "color";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFNODE;
		info->NDTtype = NDT_SFColorNode;
		info->far_ptr = & ((M_IndexedFaceSet2D *)node)->color;
		return GF_OK;
	case 4:
		info->name = "coord";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFNODE;
		info->NDTtype = NDT_SFCoordinate2DNode;
		info->far_ptr = & ((M_IndexedFaceSet2D *)node)->coord;
		return GF_OK;
	case 5:
		info->name = "texCoord";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFNODE;
		info->NDTtype = NDT_SFTextureCoordinateNode;
		info->far_ptr = & ((M_IndexedFaceSet2D *)node)->texCoord;
		return GF_OK;
	case 6:
		info->name = "colorIndex";
		info->eventType = GF_SG_EVENT_FIELD;
		info->fieldType = GF_SG_VRML_MFINT32;
		info->far_ptr = & ((M_IndexedFaceSet2D *) node)->colorIndex;
		return GF_OK;
	case 7:
		info->name = "colorPerVertex";
		info->eventType = GF_SG_EVENT_FIELD;
		info->fieldType = GF_SG_VRML_SFBOOL;
		info->far_ptr = & ((M_IndexedFaceSet2D *) node)->colorPerVertex;
		return GF_OK;
	case 8:
		info->name = "convex";
		info->eventType = GF_SG_EVENT_FIELD;
		info->fieldType = GF_SG_VRML_SFBOOL;
		info->far_ptr = & ((M_IndexedFaceSet2D *) node)->convex;
		return GF_OK;
	case 9:
		info->name = "coordIndex";
		info->eventType = GF_SG_EVENT_FIELD;
		info->fieldType = GF_SG_VRML_MFINT32;
		info->far_ptr = & ((M_IndexedFaceSet2D *) node)->coordIndex;
		return GF_OK;
	case 10:
		info->name = "texCoordIndex";
		info->eventType = GF_SG_EVENT_FIELD;
		info->fieldType = GF_SG_VRML_MFINT32;
		info->far_ptr = & ((M_IndexedFaceSet2D *) node)->texCoordIndex;
		return GF_OK;
	default:
		return GF_BAD_PARAM;
	}
}

static Bool IndexedFaceSet2D_get_aq_info(GF_Node *n, u32 FieldIndex, u8 *QType, u8 *AType, Fixed *b_min, Fixed *b_max, u32 *QT13_bits)
{
	switch (FieldIndex) {
	case 6:
		*AType = 0;
		*QType = 14;
		*b_min = FLT2FIX(-1);
		*b_max = FIX_MAX;
		return 1;
	case 9:
		*AType = 0;
		*QType = 14;
		*b_min = FLT2FIX(-1);
		*b_max = FIX_MAX;
		return 1;
	case 10:
		*AType = 0;
		*QType = 14;
		*b_min = FLT2FIX(-1);
		*b_max = FIX_MAX;
		return 1;
	default:
		return 0;
	}
}



GF_Node *IndexedFaceSet2D_Create()
{
	M_IndexedFaceSet2D *p;
	GF_SAFEALLOC(p, sizeof(M_IndexedFaceSet2D));
	if(!p) return NULL;
	gf_node_setup((GF_Node *)p, TAG_MPEG4_IndexedFaceSet2D);

#ifdef GF_NODE_USE_POINTERS
	((GF_Node *)p)->sgprivate->name = "IndexedFaceSet2D";
	((GF_Node *)p)->sgprivate->node_del = IndexedFaceSet2D_Del;
	((GF_Node *)p)->sgprivate->get_field_count = IndexedFaceSet2D_get_field_count;
	((GF_Node *)p)->sgprivate->get_field = IndexedFaceSet2D_get_field;
#endif


	/*default field values*/
	p->colorPerVertex = 1;
	p->convex = 1;
	return (GF_Node *)p;
}


/*
	IndexedLineSet Node deletion
*/

static void IndexedLineSet_Del(GF_Node *node)
{
	M_IndexedLineSet *p = (M_IndexedLineSet *) node;
	gf_sg_mfint32_del(p->set_colorIndex);
	gf_sg_mfint32_del(p->set_coordIndex);
	gf_node_unregister((GF_Node *) p->color, (GF_Node *) p);	
	gf_node_unregister((GF_Node *) p->coord, (GF_Node *) p);	
	gf_sg_mfint32_del(p->colorIndex);
	gf_sg_mfint32_del(p->coordIndex);
	gf_node_free((GF_Node *) p);
}

static const u16 IndexedLineSet_Def2All[] = { 2, 3, 4, 5, 6};
static const u16 IndexedLineSet_In2All[] = { 0, 1, 2, 3};
static const u16 IndexedLineSet_Out2All[] = { 2, 3};

static u32 IndexedLineSet_get_field_count(GF_Node *node, u8 IndexMode)
{
	switch(IndexMode) {
	case GF_SG_FIELD_CODING_IN: return 4;
	case GF_SG_FIELD_CODING_DEF: return 5;
	case GF_SG_FIELD_CODING_OUT: return 2;
	case GF_SG_FIELD_CODING_DYN: return 0;
	default:
		return 7;
	}
}

static GF_Err IndexedLineSet_get_field_index(GF_Node *n, u32 inField, u8 IndexMode, u32 *allField)
{
	switch(IndexMode) {
	case GF_SG_FIELD_CODING_IN:
		*allField = IndexedLineSet_In2All[inField];
		return GF_OK;
	case GF_SG_FIELD_CODING_DEF:
		*allField = IndexedLineSet_Def2All[inField];
		return GF_OK;
	case GF_SG_FIELD_CODING_OUT:
		*allField = IndexedLineSet_Out2All[inField];
		return GF_OK;
	default:
		return GF_BAD_PARAM;
	}
}
static GF_Err IndexedLineSet_get_field(GF_Node *node, GF_FieldInfo *info)
{
	switch (info->fieldIndex) {
	case 0:
		info->name = "set_colorIndex";
		info->eventType = GF_SG_EVENT_IN;
		info->on_event_in = ((M_IndexedLineSet *)node)->on_set_colorIndex;
		info->fieldType = GF_SG_VRML_MFINT32;
		info->far_ptr = & ((M_IndexedLineSet *) node)->set_colorIndex;
		return GF_OK;
	case 1:
		info->name = "set_coordIndex";
		info->eventType = GF_SG_EVENT_IN;
		info->on_event_in = ((M_IndexedLineSet *)node)->on_set_coordIndex;
		info->fieldType = GF_SG_VRML_MFINT32;
		info->far_ptr = & ((M_IndexedLineSet *) node)->set_coordIndex;
		return GF_OK;
	case 2:
		info->name = "color";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFNODE;
		info->NDTtype = NDT_SFColorNode;
		info->far_ptr = & ((M_IndexedLineSet *)node)->color;
		return GF_OK;
	case 3:
		info->name = "coord";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFNODE;
		info->NDTtype = NDT_SFCoordinateNode;
		info->far_ptr = & ((M_IndexedLineSet *)node)->coord;
		return GF_OK;
	case 4:
		info->name = "colorIndex";
		info->eventType = GF_SG_EVENT_FIELD;
		info->fieldType = GF_SG_VRML_MFINT32;
		info->far_ptr = & ((M_IndexedLineSet *) node)->colorIndex;
		return GF_OK;
	case 5:
		info->name = "colorPerVertex";
		info->eventType = GF_SG_EVENT_FIELD;
		info->fieldType = GF_SG_VRML_SFBOOL;
		info->far_ptr = & ((M_IndexedLineSet *) node)->colorPerVertex;
		return GF_OK;
	case 6:
		info->name = "coordIndex";
		info->eventType = GF_SG_EVENT_FIELD;
		info->fieldType = GF_SG_VRML_MFINT32;
		info->far_ptr = & ((M_IndexedLineSet *) node)->coordIndex;
		return GF_OK;
	default:
		return GF_BAD_PARAM;
	}
}

static Bool IndexedLineSet_get_aq_info(GF_Node *n, u32 FieldIndex, u8 *QType, u8 *AType, Fixed *b_min, Fixed *b_max, u32 *QT13_bits)
{
	switch (FieldIndex) {
	case 4:
		*AType = 0;
		*QType = 14;
		*b_min = FLT2FIX(-1);
		*b_max = FIX_MAX;
		return 1;
	case 6:
		*AType = 0;
		*QType = 14;
		*b_min = FLT2FIX(-1);
		*b_max = FIX_MAX;
		return 1;
	default:
		return 0;
	}
}



GF_Node *IndexedLineSet_Create()
{
	M_IndexedLineSet *p;
	GF_SAFEALLOC(p, sizeof(M_IndexedLineSet));
	if(!p) return NULL;
	gf_node_setup((GF_Node *)p, TAG_MPEG4_IndexedLineSet);

#ifdef GF_NODE_USE_POINTERS
	((GF_Node *)p)->sgprivate->name = "IndexedLineSet";
	((GF_Node *)p)->sgprivate->node_del = IndexedLineSet_Del;
	((GF_Node *)p)->sgprivate->get_field_count = IndexedLineSet_get_field_count;
	((GF_Node *)p)->sgprivate->get_field = IndexedLineSet_get_field;
#endif


	/*default field values*/
	p->colorPerVertex = 1;
	return (GF_Node *)p;
}


/*
	IndexedLineSet2D Node deletion
*/

static void IndexedLineSet2D_Del(GF_Node *node)
{
	M_IndexedLineSet2D *p = (M_IndexedLineSet2D *) node;
	gf_sg_mfint32_del(p->set_colorIndex);
	gf_sg_mfint32_del(p->set_coordIndex);
	gf_node_unregister((GF_Node *) p->color, (GF_Node *) p);	
	gf_node_unregister((GF_Node *) p->coord, (GF_Node *) p);	
	gf_sg_mfint32_del(p->colorIndex);
	gf_sg_mfint32_del(p->coordIndex);
	gf_node_free((GF_Node *) p);
}

static const u16 IndexedLineSet2D_Def2All[] = { 2, 3, 4, 5, 6};
static const u16 IndexedLineSet2D_In2All[] = { 0, 1, 2, 3};
static const u16 IndexedLineSet2D_Out2All[] = { 2, 3};

static u32 IndexedLineSet2D_get_field_count(GF_Node *node, u8 IndexMode)
{
	switch(IndexMode) {
	case GF_SG_FIELD_CODING_IN: return 4;
	case GF_SG_FIELD_CODING_DEF: return 5;
	case GF_SG_FIELD_CODING_OUT: return 2;
	case GF_SG_FIELD_CODING_DYN: return 0;
	default:
		return 7;
	}
}

static GF_Err IndexedLineSet2D_get_field_index(GF_Node *n, u32 inField, u8 IndexMode, u32 *allField)
{
	switch(IndexMode) {
	case GF_SG_FIELD_CODING_IN:
		*allField = IndexedLineSet2D_In2All[inField];
		return GF_OK;
	case GF_SG_FIELD_CODING_DEF:
		*allField = IndexedLineSet2D_Def2All[inField];
		return GF_OK;
	case GF_SG_FIELD_CODING_OUT:
		*allField = IndexedLineSet2D_Out2All[inField];
		return GF_OK;
	default:
		return GF_BAD_PARAM;
	}
}
static GF_Err IndexedLineSet2D_get_field(GF_Node *node, GF_FieldInfo *info)
{
	switch (info->fieldIndex) {
	case 0:
		info->name = "set_colorIndex";
		info->eventType = GF_SG_EVENT_IN;
		info->on_event_in = ((M_IndexedLineSet2D *)node)->on_set_colorIndex;
		info->fieldType = GF_SG_VRML_MFINT32;
		info->far_ptr = & ((M_IndexedLineSet2D *) node)->set_colorIndex;
		return GF_OK;
	case 1:
		info->name = "set_coordIndex";
		info->eventType = GF_SG_EVENT_IN;
		info->on_event_in = ((M_IndexedLineSet2D *)node)->on_set_coordIndex;
		info->fieldType = GF_SG_VRML_MFINT32;
		info->far_ptr = & ((M_IndexedLineSet2D *) node)->set_coordIndex;
		return GF_OK;
	case 2:
		info->name = "color";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFNODE;
		info->NDTtype = NDT_SFColorNode;
		info->far_ptr = & ((M_IndexedLineSet2D *)node)->color;
		return GF_OK;
	case 3:
		info->name = "coord";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFNODE;
		info->NDTtype = NDT_SFCoordinate2DNode;
		info->far_ptr = & ((M_IndexedLineSet2D *)node)->coord;
		return GF_OK;
	case 4:
		info->name = "colorIndex";
		info->eventType = GF_SG_EVENT_FIELD;
		info->fieldType = GF_SG_VRML_MFINT32;
		info->far_ptr = & ((M_IndexedLineSet2D *) node)->colorIndex;
		return GF_OK;
	case 5:
		info->name = "colorPerVertex";
		info->eventType = GF_SG_EVENT_FIELD;
		info->fieldType = GF_SG_VRML_SFBOOL;
		info->far_ptr = & ((M_IndexedLineSet2D *) node)->colorPerVertex;
		return GF_OK;
	case 6:
		info->name = "coordIndex";
		info->eventType = GF_SG_EVENT_FIELD;
		info->fieldType = GF_SG_VRML_MFINT32;
		info->far_ptr = & ((M_IndexedLineSet2D *) node)->coordIndex;
		return GF_OK;
	default:
		return GF_BAD_PARAM;
	}
}

static Bool IndexedLineSet2D_get_aq_info(GF_Node *n, u32 FieldIndex, u8 *QType, u8 *AType, Fixed *b_min, Fixed *b_max, u32 *QT13_bits)
{
	switch (FieldIndex) {
	case 4:
		*AType = 0;
		*QType = 14;
		*b_min = FLT2FIX(-1);
		*b_max = FIX_MAX;
		return 1;
	case 6:
		*AType = 0;
		*QType = 14;
		*b_min = FLT2FIX(-1);
		*b_max = FIX_MAX;
		return 1;
	default:
		return 0;
	}
}



GF_Node *IndexedLineSet2D_Create()
{
	M_IndexedLineSet2D *p;
	GF_SAFEALLOC(p, sizeof(M_IndexedLineSet2D));
	if(!p) return NULL;
	gf_node_setup((GF_Node *)p, TAG_MPEG4_IndexedLineSet2D);

#ifdef GF_NODE_USE_POINTERS
	((GF_Node *)p)->sgprivate->name = "IndexedLineSet2D";
	((GF_Node *)p)->sgprivate->node_del = IndexedLineSet2D_Del;
	((GF_Node *)p)->sgprivate->get_field_count = IndexedLineSet2D_get_field_count;
	((GF_Node *)p)->sgprivate->get_field = IndexedLineSet2D_get_field;
#endif


	/*default field values*/
	p->colorPerVertex = 1;
	return (GF_Node *)p;
}


/*
	Inline Node deletion
*/

static void Inline_Del(GF_Node *node)
{
	M_Inline *p = (M_Inline *) node;
	gf_sg_mfurl_del(p->url);
	gf_node_free((GF_Node *) p);
}

static const u16 Inline_Def2All[] = { 0};
static const u16 Inline_In2All[] = { 0};
static const u16 Inline_Out2All[] = { 0};

static u32 Inline_get_field_count(GF_Node *node, u8 IndexMode)
{
	switch(IndexMode) {
	case GF_SG_FIELD_CODING_IN: return 1;
	case GF_SG_FIELD_CODING_DEF: return 1;
	case GF_SG_FIELD_CODING_OUT: return 1;
	case GF_SG_FIELD_CODING_DYN: return 0;
	default:
		return 1;
	}
}

static GF_Err Inline_get_field_index(GF_Node *n, u32 inField, u8 IndexMode, u32 *allField)
{
	switch(IndexMode) {
	case GF_SG_FIELD_CODING_IN:
		*allField = Inline_In2All[inField];
		return GF_OK;
	case GF_SG_FIELD_CODING_DEF:
		*allField = Inline_Def2All[inField];
		return GF_OK;
	case GF_SG_FIELD_CODING_OUT:
		*allField = Inline_Out2All[inField];
		return GF_OK;
	default:
		return GF_BAD_PARAM;
	}
}
static GF_Err Inline_get_field(GF_Node *node, GF_FieldInfo *info)
{
	switch (info->fieldIndex) {
	case 0:
		info->name = "url";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_MFURL;
		info->far_ptr = & ((M_Inline *) node)->url;
		return GF_OK;
	default:
		return GF_BAD_PARAM;
	}
}

static Bool Inline_get_aq_info(GF_Node *n, u32 FieldIndex, u8 *QType, u8 *AType, Fixed *b_min, Fixed *b_max, u32 *QT13_bits)
{
	switch (FieldIndex) {
	default:
		return 0;
	}
}



GF_Node *Inline_Create()
{
	M_Inline *p;
	GF_SAFEALLOC(p, sizeof(M_Inline));
	if(!p) return NULL;
	gf_node_setup((GF_Node *)p, TAG_MPEG4_Inline);

#ifdef GF_NODE_USE_POINTERS
	((GF_Node *)p)->sgprivate->name = "Inline";
	((GF_Node *)p)->sgprivate->node_del = Inline_Del;
	((GF_Node *)p)->sgprivate->get_field_count = Inline_get_field_count;
	((GF_Node *)p)->sgprivate->get_field = Inline_get_field;
#endif


	/*default field values*/
	return (GF_Node *)p;
}


/*
	LOD Node deletion
*/

static void LOD_Del(GF_Node *node)
{
	M_LOD *p = (M_LOD *) node;
	gf_node_list_del((GF_List *) p->level, (GF_Node *) p);	
	gf_sg_mffloat_del(p->range);
	gf_node_free((GF_Node *) p);
}

static const u16 LOD_Def2All[] = { 0, 1, 2};
static const u16 LOD_In2All[] = { 0};
static const u16 LOD_Out2All[] = { 0};

static u32 LOD_get_field_count(GF_Node *node, u8 IndexMode)
{
	switch(IndexMode) {
	case GF_SG_FIELD_CODING_IN: return 1;
	case GF_SG_FIELD_CODING_DEF: return 3;
	case GF_SG_FIELD_CODING_OUT: return 1;
	case GF_SG_FIELD_CODING_DYN: return 0;
	default:
		return 3;
	}
}

static GF_Err LOD_get_field_index(GF_Node *n, u32 inField, u8 IndexMode, u32 *allField)
{
	switch(IndexMode) {
	case GF_SG_FIELD_CODING_IN:
		*allField = LOD_In2All[inField];
		return GF_OK;
	case GF_SG_FIELD_CODING_DEF:
		*allField = LOD_Def2All[inField];
		return GF_OK;
	case GF_SG_FIELD_CODING_OUT:
		*allField = LOD_Out2All[inField];
		return GF_OK;
	default:
		return GF_BAD_PARAM;
	}
}
static GF_Err LOD_get_field(GF_Node *node, GF_FieldInfo *info)
{
	switch (info->fieldIndex) {
	case 0:
		info->name = "level";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_MFNODE;
		info->NDTtype = NDT_SF3DNode;
		info->far_ptr = & ((M_LOD *)node)->level;
		return GF_OK;
	case 1:
		info->name = "center";
		info->eventType = GF_SG_EVENT_FIELD;
		info->fieldType = GF_SG_VRML_SFVEC3F;
		info->far_ptr = & ((M_LOD *) node)->center;
		return GF_OK;
	case 2:
		info->name = "range";
		info->eventType = GF_SG_EVENT_FIELD;
		info->fieldType = GF_SG_VRML_MFFLOAT;
		info->far_ptr = & ((M_LOD *) node)->range;
		return GF_OK;
	default:
		return GF_BAD_PARAM;
	}
}

static Bool LOD_get_aq_info(GF_Node *n, u32 FieldIndex, u8 *QType, u8 *AType, Fixed *b_min, Fixed *b_max, u32 *QT13_bits)
{
	switch (FieldIndex) {
	case 1:
		*AType = 0;
		*QType = 1;
		*b_min = FIX_MIN;
		*b_max = FIX_MAX;
		return 1;
	case 2:
		*AType = 0;
		*QType = 11;
		*b_min = FLT2FIX(0);
		*b_max = FIX_MAX;
		return 1;
	default:
		return 0;
	}
}



GF_Node *LOD_Create()
{
	M_LOD *p;
	GF_SAFEALLOC(p, sizeof(M_LOD));
	if(!p) return NULL;
	gf_node_setup((GF_Node *)p, TAG_MPEG4_LOD);
	p->level = gf_list_new();	

#ifdef GF_NODE_USE_POINTERS
	((GF_Node *)p)->sgprivate->name = "LOD";
	((GF_Node *)p)->sgprivate->node_del = LOD_Del;
	((GF_Node *)p)->sgprivate->get_field_count = LOD_get_field_count;
	((GF_Node *)p)->sgprivate->get_field = LOD_get_field;
#endif


	/*default field values*/
	p->center.x = FLT2FIX(0);
	p->center.y = FLT2FIX(0);
	p->center.z = FLT2FIX(0);
	return (GF_Node *)p;
}


/*
	Layer2D Node deletion
*/

static void Layer2D_Del(GF_Node *node)
{
	M_Layer2D *p = (M_Layer2D *) node;
	gf_sg_vrml_parent_reset((GF_Node *) p);	
	gf_node_unregister((GF_Node *) p->background, (GF_Node *) p);	
	gf_node_unregister((GF_Node *) p->viewport, (GF_Node *) p);	
	gf_node_free((GF_Node *) p);
}

static const u16 Layer2D_Def2All[] = { 2, 3, 4, 5};
static const u16 Layer2D_In2All[] = { 0, 1, 2, 3, 4, 5};
static const u16 Layer2D_Out2All[] = { 2, 3, 4, 5};
static const u16 Layer2D_Dyn2All[] = { 3};

static u32 Layer2D_get_field_count(GF_Node *node, u8 IndexMode)
{
	switch(IndexMode) {
	case GF_SG_FIELD_CODING_IN: return 6;
	case GF_SG_FIELD_CODING_DEF: return 4;
	case GF_SG_FIELD_CODING_OUT: return 4;
	case GF_SG_FIELD_CODING_DYN: return 1;
	default:
		return 6;
	}
}

static GF_Err Layer2D_get_field_index(GF_Node *n, u32 inField, u8 IndexMode, u32 *allField)
{
	switch(IndexMode) {
	case GF_SG_FIELD_CODING_IN:
		*allField = Layer2D_In2All[inField];
		return GF_OK;
	case GF_SG_FIELD_CODING_DEF:
		*allField = Layer2D_Def2All[inField];
		return GF_OK;
	case GF_SG_FIELD_CODING_OUT:
		*allField = Layer2D_Out2All[inField];
		return GF_OK;
	case GF_SG_FIELD_CODING_DYN:
		*allField = Layer2D_Dyn2All[inField];
		return GF_OK;
	default:
		return GF_BAD_PARAM;
	}
}
static GF_Err Layer2D_get_field(GF_Node *node, GF_FieldInfo *info)
{
	switch (info->fieldIndex) {
	case 0:
		info->name = "addChildren";
		info->eventType = GF_SG_EVENT_IN;
		info->on_event_in = ((M_Layer2D *)node)->on_addChildren;
		info->fieldType = GF_SG_VRML_MFNODE;
		info->NDTtype = NDT_SF2DNode;
		info->far_ptr = & ((M_Layer2D *)node)->addChildren;
		return GF_OK;
	case 1:
		info->name = "removeChildren";
		info->eventType = GF_SG_EVENT_IN;
		info->on_event_in = ((M_Layer2D *)node)->on_removeChildren;
		info->fieldType = GF_SG_VRML_MFNODE;
		info->NDTtype = NDT_SF2DNode;
		info->far_ptr = & ((M_Layer2D *)node)->removeChildren;
		return GF_OK;
	case 2:
		info->name = "children";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_MFNODE;
		info->NDTtype = NDT_SF2DNode;
		info->far_ptr = & ((M_Layer2D *)node)->children;
		return GF_OK;
	case 3:
		info->name = "size";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFVEC2F;
		info->far_ptr = & ((M_Layer2D *) node)->size;
		return GF_OK;
	case 4:
		info->name = "background";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFNODE;
		info->NDTtype = NDT_SFBackground2DNode;
		info->far_ptr = & ((M_Layer2D *)node)->background;
		return GF_OK;
	case 5:
		info->name = "viewport";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFNODE;
		info->NDTtype = NDT_SFViewportNode;
		info->far_ptr = & ((M_Layer2D *)node)->viewport;
		return GF_OK;
	default:
		return GF_BAD_PARAM;
	}
}

static Bool Layer2D_get_aq_info(GF_Node *n, u32 FieldIndex, u8 *QType, u8 *AType, Fixed *b_min, Fixed *b_max, u32 *QT13_bits)
{
	switch (FieldIndex) {
	case 3:
		*AType = 12;
		*QType = 12;
		*b_min = FIX_MIN;
		*b_max = FIX_MAX;
		return 1;
	default:
		return 0;
	}
}



GF_Node *Layer2D_Create()
{
	M_Layer2D *p;
	GF_SAFEALLOC(p, sizeof(M_Layer2D));
	if(!p) return NULL;
	gf_node_setup((GF_Node *)p, TAG_MPEG4_Layer2D);
	gf_sg_vrml_parent_setup((GF_Node *) p);

#ifdef GF_NODE_USE_POINTERS
	((GF_Node *)p)->sgprivate->name = "Layer2D";
	((GF_Node *)p)->sgprivate->node_del = Layer2D_Del;
	((GF_Node *)p)->sgprivate->get_field_count = Layer2D_get_field_count;
	((GF_Node *)p)->sgprivate->get_field = Layer2D_get_field;
#endif


	/*default field values*/
	p->size.x = FLT2FIX(-1);
	p->size.y = FLT2FIX(-1);
	return (GF_Node *)p;
}


/*
	Layer3D Node deletion
*/

static void Layer3D_Del(GF_Node *node)
{
	M_Layer3D *p = (M_Layer3D *) node;
	gf_sg_vrml_parent_reset((GF_Node *) p);	
	gf_node_unregister((GF_Node *) p->background, (GF_Node *) p);	
	gf_node_unregister((GF_Node *) p->fog, (GF_Node *) p);	
	gf_node_unregister((GF_Node *) p->navigationInfo, (GF_Node *) p);	
	gf_node_unregister((GF_Node *) p->viewpoint, (GF_Node *) p);	
	gf_node_free((GF_Node *) p);
}

static const u16 Layer3D_Def2All[] = { 2, 3, 4, 5, 6, 7};
static const u16 Layer3D_In2All[] = { 0, 1, 2, 3, 4, 5, 6, 7};
static const u16 Layer3D_Out2All[] = { 2, 3, 4, 5, 6, 7};
static const u16 Layer3D_Dyn2All[] = { 3};

static u32 Layer3D_get_field_count(GF_Node *node, u8 IndexMode)
{
	switch(IndexMode) {
	case GF_SG_FIELD_CODING_IN: return 8;
	case GF_SG_FIELD_CODING_DEF: return 6;
	case GF_SG_FIELD_CODING_OUT: return 6;
	case GF_SG_FIELD_CODING_DYN: return 1;
	default:
		return 8;
	}
}

static GF_Err Layer3D_get_field_index(GF_Node *n, u32 inField, u8 IndexMode, u32 *allField)
{
	switch(IndexMode) {
	case GF_SG_FIELD_CODING_IN:
		*allField = Layer3D_In2All[inField];
		return GF_OK;
	case GF_SG_FIELD_CODING_DEF:
		*allField = Layer3D_Def2All[inField];
		return GF_OK;
	case GF_SG_FIELD_CODING_OUT:
		*allField = Layer3D_Out2All[inField];
		return GF_OK;
	case GF_SG_FIELD_CODING_DYN:
		*allField = Layer3D_Dyn2All[inField];
		return GF_OK;
	default:
		return GF_BAD_PARAM;
	}
}
static GF_Err Layer3D_get_field(GF_Node *node, GF_FieldInfo *info)
{
	switch (info->fieldIndex) {
	case 0:
		info->name = "addChildren";
		info->eventType = GF_SG_EVENT_IN;
		info->on_event_in = ((M_Layer3D *)node)->on_addChildren;
		info->fieldType = GF_SG_VRML_MFNODE;
		info->NDTtype = NDT_SF3DNode;
		info->far_ptr = & ((M_Layer3D *)node)->addChildren;
		return GF_OK;
	case 1:
		info->name = "removeChildren";
		info->eventType = GF_SG_EVENT_IN;
		info->on_event_in = ((M_Layer3D *)node)->on_removeChildren;
		info->fieldType = GF_SG_VRML_MFNODE;
		info->NDTtype = NDT_SF3DNode;
		info->far_ptr = & ((M_Layer3D *)node)->removeChildren;
		return GF_OK;
	case 2:
		info->name = "children";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_MFNODE;
		info->NDTtype = NDT_SF3DNode;
		info->far_ptr = & ((M_Layer3D *)node)->children;
		return GF_OK;
	case 3:
		info->name = "size";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFVEC2F;
		info->far_ptr = & ((M_Layer3D *) node)->size;
		return GF_OK;
	case 4:
		info->name = "background";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFNODE;
		info->NDTtype = NDT_SFBackground3DNode;
		info->far_ptr = & ((M_Layer3D *)node)->background;
		return GF_OK;
	case 5:
		info->name = "fog";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFNODE;
		info->NDTtype = NDT_SFFogNode;
		info->far_ptr = & ((M_Layer3D *)node)->fog;
		return GF_OK;
	case 6:
		info->name = "navigationInfo";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFNODE;
		info->NDTtype = NDT_SFNavigationInfoNode;
		info->far_ptr = & ((M_Layer3D *)node)->navigationInfo;
		return GF_OK;
	case 7:
		info->name = "viewpoint";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFNODE;
		info->NDTtype = NDT_SFViewpointNode;
		info->far_ptr = & ((M_Layer3D *)node)->viewpoint;
		return GF_OK;
	default:
		return GF_BAD_PARAM;
	}
}

static Bool Layer3D_get_aq_info(GF_Node *n, u32 FieldIndex, u8 *QType, u8 *AType, Fixed *b_min, Fixed *b_max, u32 *QT13_bits)
{
	switch (FieldIndex) {
	case 3:
		*AType = 12;
		*QType = 12;
		*b_min = FIX_MIN;
		*b_max = FIX_MAX;
		return 1;
	default:
		return 0;
	}
}



GF_Node *Layer3D_Create()
{
	M_Layer3D *p;
	GF_SAFEALLOC(p, sizeof(M_Layer3D));
	if(!p) return NULL;
	gf_node_setup((GF_Node *)p, TAG_MPEG4_Layer3D);
	gf_sg_vrml_parent_setup((GF_Node *) p);

#ifdef GF_NODE_USE_POINTERS
	((GF_Node *)p)->sgprivate->name = "Layer3D";
	((GF_Node *)p)->sgprivate->node_del = Layer3D_Del;
	((GF_Node *)p)->sgprivate->get_field_count = Layer3D_get_field_count;
	((GF_Node *)p)->sgprivate->get_field = Layer3D_get_field;
#endif


	/*default field values*/
	p->size.x = FLT2FIX(-1);
	p->size.y = FLT2FIX(-1);
	return (GF_Node *)p;
}


/*
	Layout Node deletion
*/

static void Layout_Del(GF_Node *node)
{
	M_Layout *p = (M_Layout *) node;
	gf_sg_vrml_parent_reset((GF_Node *) p);	
	gf_sg_mfstring_del(p->justify);
	gf_node_free((GF_Node *) p);
}

static const u16 Layout_Def2All[] = { 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13};
static const u16 Layout_In2All[] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13};
static const u16 Layout_Out2All[] = { 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13};
static const u16 Layout_Dyn2All[] = { 4, 9, 13};

static u32 Layout_get_field_count(GF_Node *node, u8 IndexMode)
{
	switch(IndexMode) {
	case GF_SG_FIELD_CODING_IN: return 14;
	case GF_SG_FIELD_CODING_DEF: return 12;
	case GF_SG_FIELD_CODING_OUT: return 12;
	case GF_SG_FIELD_CODING_DYN: return 3;
	default:
		return 14;
	}
}

static GF_Err Layout_get_field_index(GF_Node *n, u32 inField, u8 IndexMode, u32 *allField)
{
	switch(IndexMode) {
	case GF_SG_FIELD_CODING_IN:
		*allField = Layout_In2All[inField];
		return GF_OK;
	case GF_SG_FIELD_CODING_DEF:
		*allField = Layout_Def2All[inField];
		return GF_OK;
	case GF_SG_FIELD_CODING_OUT:
		*allField = Layout_Out2All[inField];
		return GF_OK;
	case GF_SG_FIELD_CODING_DYN:
		*allField = Layout_Dyn2All[inField];
		return GF_OK;
	default:
		return GF_BAD_PARAM;
	}
}
static GF_Err Layout_get_field(GF_Node *node, GF_FieldInfo *info)
{
	switch (info->fieldIndex) {
	case 0:
		info->name = "addChildren";
		info->eventType = GF_SG_EVENT_IN;
		info->on_event_in = ((M_Layout *)node)->on_addChildren;
		info->fieldType = GF_SG_VRML_MFNODE;
		info->NDTtype = NDT_SF2DNode;
		info->far_ptr = & ((M_Layout *)node)->addChildren;
		return GF_OK;
	case 1:
		info->name = "removeChildren";
		info->eventType = GF_SG_EVENT_IN;
		info->on_event_in = ((M_Layout *)node)->on_removeChildren;
		info->fieldType = GF_SG_VRML_MFNODE;
		info->NDTtype = NDT_SF2DNode;
		info->far_ptr = & ((M_Layout *)node)->removeChildren;
		return GF_OK;
	case 2:
		info->name = "children";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_MFNODE;
		info->NDTtype = NDT_SF2DNode;
		info->far_ptr = & ((M_Layout *)node)->children;
		return GF_OK;
	case 3:
		info->name = "wrap";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFBOOL;
		info->far_ptr = & ((M_Layout *) node)->wrap;
		return GF_OK;
	case 4:
		info->name = "size";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFVEC2F;
		info->far_ptr = & ((M_Layout *) node)->size;
		return GF_OK;
	case 5:
		info->name = "horizontal";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFBOOL;
		info->far_ptr = & ((M_Layout *) node)->horizontal;
		return GF_OK;
	case 6:
		info->name = "justify";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_MFSTRING;
		info->far_ptr = & ((M_Layout *) node)->justify;
		return GF_OK;
	case 7:
		info->name = "leftToRight";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFBOOL;
		info->far_ptr = & ((M_Layout *) node)->leftToRight;
		return GF_OK;
	case 8:
		info->name = "topToBottom";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFBOOL;
		info->far_ptr = & ((M_Layout *) node)->topToBottom;
		return GF_OK;
	case 9:
		info->name = "spacing";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFFLOAT;
		info->far_ptr = & ((M_Layout *) node)->spacing;
		return GF_OK;
	case 10:
		info->name = "smoothScroll";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFBOOL;
		info->far_ptr = & ((M_Layout *) node)->smoothScroll;
		return GF_OK;
	case 11:
		info->name = "loop";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFBOOL;
		info->far_ptr = & ((M_Layout *) node)->loop;
		return GF_OK;
	case 12:
		info->name = "scrollVertical";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFBOOL;
		info->far_ptr = & ((M_Layout *) node)->scrollVertical;
		return GF_OK;
	case 13:
		info->name = "scrollRate";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFFLOAT;
		info->far_ptr = & ((M_Layout *) node)->scrollRate;
		return GF_OK;
	default:
		return GF_BAD_PARAM;
	}
}

static Bool Layout_get_aq_info(GF_Node *n, u32 FieldIndex, u8 *QType, u8 *AType, Fixed *b_min, Fixed *b_max, u32 *QT13_bits)
{
	switch (FieldIndex) {
	case 4:
		*AType = 12;
		*QType = 12;
		*b_min = FLT2FIX(0);
		*b_max = FIX_MAX;
		return 1;
	case 9:
		*AType = 7;
		*QType = 0;
		*b_min = FLT2FIX(0);
		*b_max = FIX_MAX;
		return 1;
	case 13:
		*AType = 7;
		*QType = 0;
		*b_min = FIX_MIN;
		*b_max = FIX_MAX;
		return 1;
	default:
		return 0;
	}
}



GF_Node *Layout_Create()
{
	M_Layout *p;
	GF_SAFEALLOC(p, sizeof(M_Layout));
	if(!p) return NULL;
	gf_node_setup((GF_Node *)p, TAG_MPEG4_Layout);
	gf_sg_vrml_parent_setup((GF_Node *) p);

#ifdef GF_NODE_USE_POINTERS
	((GF_Node *)p)->sgprivate->name = "Layout";
	((GF_Node *)p)->sgprivate->node_del = Layout_Del;
	((GF_Node *)p)->sgprivate->get_field_count = Layout_get_field_count;
	((GF_Node *)p)->sgprivate->get_field = Layout_get_field;
#endif


	/*default field values*/
	p->size.x = FLT2FIX(-1);
	p->size.y = FLT2FIX(-1);
	p->horizontal = 1;
	p->justify.vals = malloc(sizeof(SFString)*1);
	p->justify.count = 1;
	p->justify.vals[0] = malloc(sizeof(char) * 6);
	strcpy(p->justify.vals[0], "BEGIN");
	p->leftToRight = 1;
	p->topToBottom = 1;
	p->spacing = FLT2FIX(1);
	p->scrollVertical = 1;
	p->scrollRate = FLT2FIX(0);
	return (GF_Node *)p;
}


/*
	LineProperties Node deletion
*/

static void LineProperties_Del(GF_Node *node)
{
	M_LineProperties *p = (M_LineProperties *) node;
	gf_node_free((GF_Node *) p);
}

static const u16 LineProperties_Def2All[] = { 0, 1, 2};
static const u16 LineProperties_In2All[] = { 0, 1, 2};
static const u16 LineProperties_Out2All[] = { 0, 1, 2};
static const u16 LineProperties_Dyn2All[] = { 0, 2};

static u32 LineProperties_get_field_count(GF_Node *node, u8 IndexMode)
{
	switch(IndexMode) {
	case GF_SG_FIELD_CODING_IN: return 3;
	case GF_SG_FIELD_CODING_DEF: return 3;
	case GF_SG_FIELD_CODING_OUT: return 3;
	case GF_SG_FIELD_CODING_DYN: return 2;
	default:
		return 3;
	}
}

static GF_Err LineProperties_get_field_index(GF_Node *n, u32 inField, u8 IndexMode, u32 *allField)
{
	switch(IndexMode) {
	case GF_SG_FIELD_CODING_IN:
		*allField = LineProperties_In2All[inField];
		return GF_OK;
	case GF_SG_FIELD_CODING_DEF:
		*allField = LineProperties_Def2All[inField];
		return GF_OK;
	case GF_SG_FIELD_CODING_OUT:
		*allField = LineProperties_Out2All[inField];
		return GF_OK;
	case GF_SG_FIELD_CODING_DYN:
		*allField = LineProperties_Dyn2All[inField];
		return GF_OK;
	default:
		return GF_BAD_PARAM;
	}
}
static GF_Err LineProperties_get_field(GF_Node *node, GF_FieldInfo *info)
{
	switch (info->fieldIndex) {
	case 0:
		info->name = "lineColor";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFCOLOR;
		info->far_ptr = & ((M_LineProperties *) node)->lineColor;
		return GF_OK;
	case 1:
		info->name = "lineStyle";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFINT32;
		info->far_ptr = & ((M_LineProperties *) node)->lineStyle;
		return GF_OK;
	case 2:
		info->name = "width";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFFLOAT;
		info->far_ptr = & ((M_LineProperties *) node)->width;
		return GF_OK;
	default:
		return GF_BAD_PARAM;
	}
}

static Bool LineProperties_get_aq_info(GF_Node *n, u32 FieldIndex, u8 *QType, u8 *AType, Fixed *b_min, Fixed *b_max, u32 *QT13_bits)
{
	switch (FieldIndex) {
	case 0:
		*AType = 4;
		*QType = 4;
		*b_min = FLT2FIX(0);
		*b_max = FLT2FIX(1);
		return 1;
	case 1:
		*AType = 0;
		*QType = 13;
		*QT13_bits = 3;
		*b_min = FLT2FIX(0);
		*b_max = FLT2FIX(5);
		return 1;
	case 2:
		*AType = 7;
		*QType = 12;
		*b_min = FLT2FIX(0);
		*b_max = FIX_MAX;
		return 1;
	default:
		return 0;
	}
}



GF_Node *LineProperties_Create()
{
	M_LineProperties *p;
	GF_SAFEALLOC(p, sizeof(M_LineProperties));
	if(!p) return NULL;
	gf_node_setup((GF_Node *)p, TAG_MPEG4_LineProperties);

#ifdef GF_NODE_USE_POINTERS
	((GF_Node *)p)->sgprivate->name = "LineProperties";
	((GF_Node *)p)->sgprivate->node_del = LineProperties_Del;
	((GF_Node *)p)->sgprivate->get_field_count = LineProperties_get_field_count;
	((GF_Node *)p)->sgprivate->get_field = LineProperties_get_field;
#endif


	/*default field values*/
	p->lineColor.red = FLT2FIX(0);
	p->lineColor.green = FLT2FIX(0);
	p->lineColor.blue = FLT2FIX(0);
	p->lineStyle = 0;
	p->width = FLT2FIX(1);
	return (GF_Node *)p;
}


/*
	ListeningPoint Node deletion
*/

static void ListeningPoint_Del(GF_Node *node)
{
	M_ListeningPoint *p = (M_ListeningPoint *) node;
	gf_sg_sfstring_del(p->description);
	gf_node_free((GF_Node *) p);
}

static const u16 ListeningPoint_Def2All[] = { 1, 2, 3, 4};
static const u16 ListeningPoint_In2All[] = { 0, 1, 2, 3};
static const u16 ListeningPoint_Out2All[] = { 1, 2, 3, 5, 6};
static const u16 ListeningPoint_Dyn2All[] = { 2, 3};

static u32 ListeningPoint_get_field_count(GF_Node *node, u8 IndexMode)
{
	switch(IndexMode) {
	case GF_SG_FIELD_CODING_IN: return 4;
	case GF_SG_FIELD_CODING_DEF: return 4;
	case GF_SG_FIELD_CODING_OUT: return 5;
	case GF_SG_FIELD_CODING_DYN: return 2;
	default:
		return 7;
	}
}

static GF_Err ListeningPoint_get_field_index(GF_Node *n, u32 inField, u8 IndexMode, u32 *allField)
{
	switch(IndexMode) {
	case GF_SG_FIELD_CODING_IN:
		*allField = ListeningPoint_In2All[inField];
		return GF_OK;
	case GF_SG_FIELD_CODING_DEF:
		*allField = ListeningPoint_Def2All[inField];
		return GF_OK;
	case GF_SG_FIELD_CODING_OUT:
		*allField = ListeningPoint_Out2All[inField];
		return GF_OK;
	case GF_SG_FIELD_CODING_DYN:
		*allField = ListeningPoint_Dyn2All[inField];
		return GF_OK;
	default:
		return GF_BAD_PARAM;
	}
}
static GF_Err ListeningPoint_get_field(GF_Node *node, GF_FieldInfo *info)
{
	switch (info->fieldIndex) {
	case 0:
		info->name = "set_bind";
		info->eventType = GF_SG_EVENT_IN;
		info->on_event_in = ((M_ListeningPoint *)node)->on_set_bind;
		info->fieldType = GF_SG_VRML_SFBOOL;
		info->far_ptr = & ((M_ListeningPoint *) node)->set_bind;
		return GF_OK;
	case 1:
		info->name = "jump";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFBOOL;
		info->far_ptr = & ((M_ListeningPoint *) node)->jump;
		return GF_OK;
	case 2:
		info->name = "orientation";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFROTATION;
		info->far_ptr = & ((M_ListeningPoint *) node)->orientation;
		return GF_OK;
	case 3:
		info->name = "position";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFVEC3F;
		info->far_ptr = & ((M_ListeningPoint *) node)->position;
		return GF_OK;
	case 4:
		info->name = "description";
		info->eventType = GF_SG_EVENT_FIELD;
		info->fieldType = GF_SG_VRML_SFSTRING;
		info->far_ptr = & ((M_ListeningPoint *) node)->description;
		return GF_OK;
	case 5:
		info->name = "bindTime";
		info->eventType = GF_SG_EVENT_OUT;
		info->fieldType = GF_SG_VRML_SFTIME;
		info->far_ptr = & ((M_ListeningPoint *) node)->bindTime;
		return GF_OK;
	case 6:
		info->name = "isBound";
		info->eventType = GF_SG_EVENT_OUT;
		info->fieldType = GF_SG_VRML_SFBOOL;
		info->far_ptr = & ((M_ListeningPoint *) node)->isBound;
		return GF_OK;
	default:
		return GF_BAD_PARAM;
	}
}

static Bool ListeningPoint_get_aq_info(GF_Node *n, u32 FieldIndex, u8 *QType, u8 *AType, Fixed *b_min, Fixed *b_max, u32 *QT13_bits)
{
	switch (FieldIndex) {
	case 2:
		*AType = 10;
		*QType = 10;
		return 1;
	case 3:
		*AType = 1;
		*QType = 1;
		*b_min = FIX_MIN;
		*b_max = FIX_MAX;
		return 1;
	default:
		return 0;
	}
}



GF_Node *ListeningPoint_Create()
{
	M_ListeningPoint *p;
	GF_SAFEALLOC(p, sizeof(M_ListeningPoint));
	if(!p) return NULL;
	gf_node_setup((GF_Node *)p, TAG_MPEG4_ListeningPoint);

#ifdef GF_NODE_USE_POINTERS
	((GF_Node *)p)->sgprivate->name = "ListeningPoint";
	((GF_Node *)p)->sgprivate->node_del = ListeningPoint_Del;
	((GF_Node *)p)->sgprivate->get_field_count = ListeningPoint_get_field_count;
	((GF_Node *)p)->sgprivate->get_field = ListeningPoint_get_field;
#endif


	/*default field values*/
	p->jump = 1;
	p->orientation.x = FLT2FIX(0);
	p->orientation.y = FLT2FIX(0);
	p->orientation.z = FLT2FIX(1);
	p->orientation.q = FLT2FIX(0);
	p->position.x = FLT2FIX(0);
	p->position.y = FLT2FIX(0);
	p->position.z = FLT2FIX(10);
	return (GF_Node *)p;
}


/*
	Material Node deletion
*/

static void Material_Del(GF_Node *node)
{
	M_Material *p = (M_Material *) node;
	gf_node_free((GF_Node *) p);
}

static const u16 Material_Def2All[] = { 0, 1, 2, 3, 4, 5};
static const u16 Material_In2All[] = { 0, 1, 2, 3, 4, 5};
static const u16 Material_Out2All[] = { 0, 1, 2, 3, 4, 5};
static const u16 Material_Dyn2All[] = { 0, 1, 2, 3, 4, 5};

static u32 Material_get_field_count(GF_Node *node, u8 IndexMode)
{
	switch(IndexMode) {
	case GF_SG_FIELD_CODING_IN: return 6;
	case GF_SG_FIELD_CODING_DEF: return 6;
	case GF_SG_FIELD_CODING_OUT: return 6;
	case GF_SG_FIELD_CODING_DYN: return 6;
	default:
		return 6;
	}
}

static GF_Err Material_get_field_index(GF_Node *n, u32 inField, u8 IndexMode, u32 *allField)
{
	switch(IndexMode) {
	case GF_SG_FIELD_CODING_IN:
		*allField = Material_In2All[inField];
		return GF_OK;
	case GF_SG_FIELD_CODING_DEF:
		*allField = Material_Def2All[inField];
		return GF_OK;
	case GF_SG_FIELD_CODING_OUT:
		*allField = Material_Out2All[inField];
		return GF_OK;
	case GF_SG_FIELD_CODING_DYN:
		*allField = Material_Dyn2All[inField];
		return GF_OK;
	default:
		return GF_BAD_PARAM;
	}
}
static GF_Err Material_get_field(GF_Node *node, GF_FieldInfo *info)
{
	switch (info->fieldIndex) {
	case 0:
		info->name = "ambientIntensity";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFFLOAT;
		info->far_ptr = & ((M_Material *) node)->ambientIntensity;
		return GF_OK;
	case 1:
		info->name = "diffuseColor";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFCOLOR;
		info->far_ptr = & ((M_Material *) node)->diffuseColor;
		return GF_OK;
	case 2:
		info->name = "emissiveColor";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFCOLOR;
		info->far_ptr = & ((M_Material *) node)->emissiveColor;
		return GF_OK;
	case 3:
		info->name = "shininess";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFFLOAT;
		info->far_ptr = & ((M_Material *) node)->shininess;
		return GF_OK;
	case 4:
		info->name = "specularColor";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFCOLOR;
		info->far_ptr = & ((M_Material *) node)->specularColor;
		return GF_OK;
	case 5:
		info->name = "transparency";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFFLOAT;
		info->far_ptr = & ((M_Material *) node)->transparency;
		return GF_OK;
	default:
		return GF_BAD_PARAM;
	}
}

static Bool Material_get_aq_info(GF_Node *n, u32 FieldIndex, u8 *QType, u8 *AType, Fixed *b_min, Fixed *b_max, u32 *QT13_bits)
{
	switch (FieldIndex) {
	case 0:
		*AType = 8;
		*QType = 4;
		*b_min = FLT2FIX(0);
		*b_max = FLT2FIX(1);
		return 1;
	case 1:
		*AType = 4;
		*QType = 4;
		*b_min = FLT2FIX(0);
		*b_max = FLT2FIX(1);
		return 1;
	case 2:
		*AType = 4;
		*QType = 4;
		*b_min = FLT2FIX(0);
		*b_max = FLT2FIX(1);
		return 1;
	case 3:
		*AType = 8;
		*QType = 4;
		*b_min = FLT2FIX(0);
		*b_max = FLT2FIX(1);
		return 1;
	case 4:
		*AType = 4;
		*QType = 4;
		*b_min = FLT2FIX(0);
		*b_max = FLT2FIX(1);
		return 1;
	case 5:
		*AType = 8;
		*QType = 4;
		*b_min = FLT2FIX(0);
		*b_max = FLT2FIX(1);
		return 1;
	default:
		return 0;
	}
}



GF_Node *Material_Create()
{
	M_Material *p;
	GF_SAFEALLOC(p, sizeof(M_Material));
	if(!p) return NULL;
	gf_node_setup((GF_Node *)p, TAG_MPEG4_Material);

#ifdef GF_NODE_USE_POINTERS
	((GF_Node *)p)->sgprivate->name = "Material";
	((GF_Node *)p)->sgprivate->node_del = Material_Del;
	((GF_Node *)p)->sgprivate->get_field_count = Material_get_field_count;
	((GF_Node *)p)->sgprivate->get_field = Material_get_field;
#endif


	/*default field values*/
	p->ambientIntensity = FLT2FIX(0.2);
	p->diffuseColor.red = FLT2FIX(0.8);
	p->diffuseColor.green = FLT2FIX(0.8);
	p->diffuseColor.blue = FLT2FIX(0.8);
	p->emissiveColor.red = FLT2FIX(0);
	p->emissiveColor.green = FLT2FIX(0);
	p->emissiveColor.blue = FLT2FIX(0);
	p->shininess = FLT2FIX(0.2);
	p->specularColor.red = FLT2FIX(0);
	p->specularColor.green = FLT2FIX(0);
	p->specularColor.blue = FLT2FIX(0);
	p->transparency = FLT2FIX(0);
	return (GF_Node *)p;
}


/*
	Material2D Node deletion
*/

static void Material2D_Del(GF_Node *node)
{
	M_Material2D *p = (M_Material2D *) node;
	gf_node_unregister((GF_Node *) p->lineProps, (GF_Node *) p);	
	gf_node_free((GF_Node *) p);
}

static const u16 Material2D_Def2All[] = { 0, 1, 2, 3};
static const u16 Material2D_In2All[] = { 0, 1, 2, 3};
static const u16 Material2D_Out2All[] = { 0, 1, 2, 3};
static const u16 Material2D_Dyn2All[] = { 0, 3};

static u32 Material2D_get_field_count(GF_Node *node, u8 IndexMode)
{
	switch(IndexMode) {
	case GF_SG_FIELD_CODING_IN: return 4;
	case GF_SG_FIELD_CODING_DEF: return 4;
	case GF_SG_FIELD_CODING_OUT: return 4;
	case GF_SG_FIELD_CODING_DYN: return 2;
	default:
		return 4;
	}
}

static GF_Err Material2D_get_field_index(GF_Node *n, u32 inField, u8 IndexMode, u32 *allField)
{
	switch(IndexMode) {
	case GF_SG_FIELD_CODING_IN:
		*allField = Material2D_In2All[inField];
		return GF_OK;
	case GF_SG_FIELD_CODING_DEF:
		*allField = Material2D_Def2All[inField];
		return GF_OK;
	case GF_SG_FIELD_CODING_OUT:
		*allField = Material2D_Out2All[inField];
		return GF_OK;
	case GF_SG_FIELD_CODING_DYN:
		*allField = Material2D_Dyn2All[inField];
		return GF_OK;
	default:
		return GF_BAD_PARAM;
	}
}
static GF_Err Material2D_get_field(GF_Node *node, GF_FieldInfo *info)
{
	switch (info->fieldIndex) {
	case 0:
		info->name = "emissiveColor";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFCOLOR;
		info->far_ptr = & ((M_Material2D *) node)->emissiveColor;
		return GF_OK;
	case 1:
		info->name = "filled";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFBOOL;
		info->far_ptr = & ((M_Material2D *) node)->filled;
		return GF_OK;
	case 2:
		info->name = "lineProps";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFNODE;
		info->NDTtype = NDT_SFLinePropertiesNode;
		info->far_ptr = & ((M_Material2D *)node)->lineProps;
		return GF_OK;
	case 3:
		info->name = "transparency";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFFLOAT;
		info->far_ptr = & ((M_Material2D *) node)->transparency;
		return GF_OK;
	default:
		return GF_BAD_PARAM;
	}
}

static Bool Material2D_get_aq_info(GF_Node *n, u32 FieldIndex, u8 *QType, u8 *AType, Fixed *b_min, Fixed *b_max, u32 *QT13_bits)
{
	switch (FieldIndex) {
	case 0:
		*AType = 4;
		*QType = 4;
		*b_min = FLT2FIX(0);
		*b_max = FLT2FIX(1);
		return 1;
	case 3:
		*AType = 8;
		*QType = 4;
		*b_min = FLT2FIX(0);
		*b_max = FLT2FIX(1);
		return 1;
	default:
		return 0;
	}
}



GF_Node *Material2D_Create()
{
	M_Material2D *p;
	GF_SAFEALLOC(p, sizeof(M_Material2D));
	if(!p) return NULL;
	gf_node_setup((GF_Node *)p, TAG_MPEG4_Material2D);

#ifdef GF_NODE_USE_POINTERS
	((GF_Node *)p)->sgprivate->name = "Material2D";
	((GF_Node *)p)->sgprivate->node_del = Material2D_Del;
	((GF_Node *)p)->sgprivate->get_field_count = Material2D_get_field_count;
	((GF_Node *)p)->sgprivate->get_field = Material2D_get_field;
#endif


	/*default field values*/
	p->emissiveColor.red = FLT2FIX(0.8);
	p->emissiveColor.green = FLT2FIX(0.8);
	p->emissiveColor.blue = FLT2FIX(0.8);
	p->transparency = FLT2FIX(0);
	return (GF_Node *)p;
}


/*
	MovieTexture Node deletion
*/

static void MovieTexture_Del(GF_Node *node)
{
	M_MovieTexture *p = (M_MovieTexture *) node;
	gf_sg_mfurl_del(p->url);
	gf_node_free((GF_Node *) p);
}

static const u16 MovieTexture_Def2All[] = { 0, 1, 2, 3, 4, 5, 6};
static const u16 MovieTexture_In2All[] = { 0, 1, 2, 3, 4};
static const u16 MovieTexture_Out2All[] = { 0, 1, 2, 3, 4, 7, 8};
static const u16 MovieTexture_Dyn2All[] = { 1};

static u32 MovieTexture_get_field_count(GF_Node *node, u8 IndexMode)
{
	switch(IndexMode) {
	case GF_SG_FIELD_CODING_IN: return 5;
	case GF_SG_FIELD_CODING_DEF: return 7;
	case GF_SG_FIELD_CODING_OUT: return 7;
	case GF_SG_FIELD_CODING_DYN: return 1;
	default:
		return 9;
	}
}

static GF_Err MovieTexture_get_field_index(GF_Node *n, u32 inField, u8 IndexMode, u32 *allField)
{
	switch(IndexMode) {
	case GF_SG_FIELD_CODING_IN:
		*allField = MovieTexture_In2All[inField];
		return GF_OK;
	case GF_SG_FIELD_CODING_DEF:
		*allField = MovieTexture_Def2All[inField];
		return GF_OK;
	case GF_SG_FIELD_CODING_OUT:
		*allField = MovieTexture_Out2All[inField];
		return GF_OK;
	case GF_SG_FIELD_CODING_DYN:
		*allField = MovieTexture_Dyn2All[inField];
		return GF_OK;
	default:
		return GF_BAD_PARAM;
	}
}
static GF_Err MovieTexture_get_field(GF_Node *node, GF_FieldInfo *info)
{
	switch (info->fieldIndex) {
	case 0:
		info->name = "loop";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFBOOL;
		info->far_ptr = & ((M_MovieTexture *) node)->loop;
		return GF_OK;
	case 1:
		info->name = "speed";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFFLOAT;
		info->far_ptr = & ((M_MovieTexture *) node)->speed;
		return GF_OK;
	case 2:
		info->name = "startTime";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFTIME;
		info->far_ptr = & ((M_MovieTexture *) node)->startTime;
		return GF_OK;
	case 3:
		info->name = "stopTime";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFTIME;
		info->far_ptr = & ((M_MovieTexture *) node)->stopTime;
		return GF_OK;
	case 4:
		info->name = "url";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_MFURL;
		info->far_ptr = & ((M_MovieTexture *) node)->url;
		return GF_OK;
	case 5:
		info->name = "repeatS";
		info->eventType = GF_SG_EVENT_FIELD;
		info->fieldType = GF_SG_VRML_SFBOOL;
		info->far_ptr = & ((M_MovieTexture *) node)->repeatS;
		return GF_OK;
	case 6:
		info->name = "repeatT";
		info->eventType = GF_SG_EVENT_FIELD;
		info->fieldType = GF_SG_VRML_SFBOOL;
		info->far_ptr = & ((M_MovieTexture *) node)->repeatT;
		return GF_OK;
	case 7:
		info->name = "duration_changed";
		info->eventType = GF_SG_EVENT_OUT;
		info->fieldType = GF_SG_VRML_SFTIME;
		info->far_ptr = & ((M_MovieTexture *) node)->duration_changed;
		return GF_OK;
	case 8:
		info->name = "isActive";
		info->eventType = GF_SG_EVENT_OUT;
		info->fieldType = GF_SG_VRML_SFBOOL;
		info->far_ptr = & ((M_MovieTexture *) node)->isActive;
		return GF_OK;
	default:
		return GF_BAD_PARAM;
	}
}

static Bool MovieTexture_get_aq_info(GF_Node *n, u32 FieldIndex, u8 *QType, u8 *AType, Fixed *b_min, Fixed *b_max, u32 *QT13_bits)
{
	switch (FieldIndex) {
	case 1:
		*AType = 7;
		*QType = 0;
		*b_min = FIX_MIN;
		*b_max = FIX_MAX;
		return 1;
	default:
		return 0;
	}
}



GF_Node *MovieTexture_Create()
{
	M_MovieTexture *p;
	GF_SAFEALLOC(p, sizeof(M_MovieTexture));
	if(!p) return NULL;
	gf_node_setup((GF_Node *)p, TAG_MPEG4_MovieTexture);

#ifdef GF_NODE_USE_POINTERS
	((GF_Node *)p)->sgprivate->name = "MovieTexture";
	((GF_Node *)p)->sgprivate->node_del = MovieTexture_Del;
	((GF_Node *)p)->sgprivate->get_field_count = MovieTexture_get_field_count;
	((GF_Node *)p)->sgprivate->get_field = MovieTexture_get_field;
#endif


	/*default field values*/
	p->speed = FLT2FIX(1.0);
	p->startTime = 0;
	p->stopTime = 0;
	p->repeatS = 1;
	p->repeatT = 1;
	return (GF_Node *)p;
}


/*
	NavigationInfo Node deletion
*/

static void NavigationInfo_Del(GF_Node *node)
{
	M_NavigationInfo *p = (M_NavigationInfo *) node;
	gf_sg_mffloat_del(p->avatarSize);
	gf_sg_mfstring_del(p->type);
	gf_node_free((GF_Node *) p);
}

static const u16 NavigationInfo_Def2All[] = { 1, 2, 3, 4, 5};
static const u16 NavigationInfo_In2All[] = { 0, 1, 2, 3, 4, 5};
static const u16 NavigationInfo_Out2All[] = { 1, 2, 3, 4, 5, 6};
static const u16 NavigationInfo_Dyn2All[] = { 5};

static u32 NavigationInfo_get_field_count(GF_Node *node, u8 IndexMode)
{
	switch(IndexMode) {
	case GF_SG_FIELD_CODING_IN: return 6;
	case GF_SG_FIELD_CODING_DEF: return 5;
	case GF_SG_FIELD_CODING_OUT: return 6;
	case GF_SG_FIELD_CODING_DYN: return 1;
	default:
		return 7;
	}
}

static GF_Err NavigationInfo_get_field_index(GF_Node *n, u32 inField, u8 IndexMode, u32 *allField)
{
	switch(IndexMode) {
	case GF_SG_FIELD_CODING_IN:
		*allField = NavigationInfo_In2All[inField];
		return GF_OK;
	case GF_SG_FIELD_CODING_DEF:
		*allField = NavigationInfo_Def2All[inField];
		return GF_OK;
	case GF_SG_FIELD_CODING_OUT:
		*allField = NavigationInfo_Out2All[inField];
		return GF_OK;
	case GF_SG_FIELD_CODING_DYN:
		*allField = NavigationInfo_Dyn2All[inField];
		return GF_OK;
	default:
		return GF_BAD_PARAM;
	}
}
static GF_Err NavigationInfo_get_field(GF_Node *node, GF_FieldInfo *info)
{
	switch (info->fieldIndex) {
	case 0:
		info->name = "set_bind";
		info->eventType = GF_SG_EVENT_IN;
		info->on_event_in = ((M_NavigationInfo *)node)->on_set_bind;
		info->fieldType = GF_SG_VRML_SFBOOL;
		info->far_ptr = & ((M_NavigationInfo *) node)->set_bind;
		return GF_OK;
	case 1:
		info->name = "avatarSize";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_MFFLOAT;
		info->far_ptr = & ((M_NavigationInfo *) node)->avatarSize;
		return GF_OK;
	case 2:
		info->name = "headlight";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFBOOL;
		info->far_ptr = & ((M_NavigationInfo *) node)->headlight;
		return GF_OK;
	case 3:
		info->name = "speed";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFFLOAT;
		info->far_ptr = & ((M_NavigationInfo *) node)->speed;
		return GF_OK;
	case 4:
		info->name = "type";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_MFSTRING;
		info->far_ptr = & ((M_NavigationInfo *) node)->type;
		return GF_OK;
	case 5:
		info->name = "visibilityLimit";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFFLOAT;
		info->far_ptr = & ((M_NavigationInfo *) node)->visibilityLimit;
		return GF_OK;
	case 6:
		info->name = "isBound";
		info->eventType = GF_SG_EVENT_OUT;
		info->fieldType = GF_SG_VRML_SFBOOL;
		info->far_ptr = & ((M_NavigationInfo *) node)->isBound;
		return GF_OK;
	default:
		return GF_BAD_PARAM;
	}
}

static Bool NavigationInfo_get_aq_info(GF_Node *n, u32 FieldIndex, u8 *QType, u8 *AType, Fixed *b_min, Fixed *b_max, u32 *QT13_bits)
{
	switch (FieldIndex) {
	case 1:
		*AType = 0;
		*QType = 11;
		*b_min = FLT2FIX(0);
		*b_max = FIX_MAX;
		return 1;
	case 3:
		*AType = 0;
		*QType = 0;
		*b_min = FLT2FIX(0);
		*b_max = FIX_MAX;
		return 1;
	case 5:
		*AType = 7;
		*QType = 11;
		*b_min = FLT2FIX(0);
		*b_max = FIX_MAX;
		return 1;
	default:
		return 0;
	}
}



GF_Node *NavigationInfo_Create()
{
	M_NavigationInfo *p;
	GF_SAFEALLOC(p, sizeof(M_NavigationInfo));
	if(!p) return NULL;
	gf_node_setup((GF_Node *)p, TAG_MPEG4_NavigationInfo);

#ifdef GF_NODE_USE_POINTERS
	((GF_Node *)p)->sgprivate->name = "NavigationInfo";
	((GF_Node *)p)->sgprivate->node_del = NavigationInfo_Del;
	((GF_Node *)p)->sgprivate->get_field_count = NavigationInfo_get_field_count;
	((GF_Node *)p)->sgprivate->get_field = NavigationInfo_get_field;
#endif


	/*default field values*/
	p->avatarSize.vals = malloc(sizeof(SFFloat)*3);
	p->avatarSize.count = 3;
	p->avatarSize.vals[0] = FLT2FIX(0.25);
	p->avatarSize.vals[1] = FLT2FIX(1.6);
	p->avatarSize.vals[2] = FLT2FIX(0.75);
	p->headlight = 1;
	p->speed = FLT2FIX(1.0);
	p->type.vals = malloc(sizeof(SFString)*2);
	p->type.count = 2;
	p->type.vals[0] = malloc(sizeof(char) * 5);
	strcpy(p->type.vals[0], "WALK");
	p->type.vals[1] = malloc(sizeof(char) * 4);
	strcpy(p->type.vals[1], "ANY");
	p->visibilityLimit = FLT2FIX(0.0);
	return (GF_Node *)p;
}


/*
	Normal Node deletion
*/

static void Normal_Del(GF_Node *node)
{
	M_Normal *p = (M_Normal *) node;
	gf_sg_mfvec3f_del(p->vector);
	gf_node_free((GF_Node *) p);
}

static const u16 Normal_Def2All[] = { 0};
static const u16 Normal_In2All[] = { 0};
static const u16 Normal_Out2All[] = { 0};
static const u16 Normal_Dyn2All[] = { 0};

static u32 Normal_get_field_count(GF_Node *node, u8 IndexMode)
{
	switch(IndexMode) {
	case GF_SG_FIELD_CODING_IN: return 1;
	case GF_SG_FIELD_CODING_DEF: return 1;
	case GF_SG_FIELD_CODING_OUT: return 1;
	case GF_SG_FIELD_CODING_DYN: return 1;
	default:
		return 1;
	}
}

static GF_Err Normal_get_field_index(GF_Node *n, u32 inField, u8 IndexMode, u32 *allField)
{
	switch(IndexMode) {
	case GF_SG_FIELD_CODING_IN:
		*allField = Normal_In2All[inField];
		return GF_OK;
	case GF_SG_FIELD_CODING_DEF:
		*allField = Normal_Def2All[inField];
		return GF_OK;
	case GF_SG_FIELD_CODING_OUT:
		*allField = Normal_Out2All[inField];
		return GF_OK;
	case GF_SG_FIELD_CODING_DYN:
		*allField = Normal_Dyn2All[inField];
		return GF_OK;
	default:
		return GF_BAD_PARAM;
	}
}
static GF_Err Normal_get_field(GF_Node *node, GF_FieldInfo *info)
{
	switch (info->fieldIndex) {
	case 0:
		info->name = "vector";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_MFVEC3F;
		info->far_ptr = & ((M_Normal *) node)->vector;
		return GF_OK;
	default:
		return GF_BAD_PARAM;
	}
}

static Bool Normal_get_aq_info(GF_Node *n, u32 FieldIndex, u8 *QType, u8 *AType, Fixed *b_min, Fixed *b_max, u32 *QT13_bits)
{
	switch (FieldIndex) {
	case 0:
		*AType = 9;
		*QType = 9;
		return 1;
	default:
		return 0;
	}
}



GF_Node *Normal_Create()
{
	M_Normal *p;
	GF_SAFEALLOC(p, sizeof(M_Normal));
	if(!p) return NULL;
	gf_node_setup((GF_Node *)p, TAG_MPEG4_Normal);

#ifdef GF_NODE_USE_POINTERS
	((GF_Node *)p)->sgprivate->name = "Normal";
	((GF_Node *)p)->sgprivate->node_del = Normal_Del;
	((GF_Node *)p)->sgprivate->get_field_count = Normal_get_field_count;
	((GF_Node *)p)->sgprivate->get_field = Normal_get_field;
#endif


	/*default field values*/
	return (GF_Node *)p;
}


/*
	NormalInterpolator Node deletion
*/

static void NormalInterpolator_Del(GF_Node *node)
{
	M_NormalInterpolator *p = (M_NormalInterpolator *) node;
	gf_sg_mffloat_del(p->key);
	gf_sg_mfvec3f_del(p->keyValue);
	gf_sg_mfvec3f_del(p->value_changed);
	gf_node_free((GF_Node *) p);
}

static const u16 NormalInterpolator_Def2All[] = { 1, 2};
static const u16 NormalInterpolator_In2All[] = { 0, 1, 2};
static const u16 NormalInterpolator_Out2All[] = { 1, 2, 3};

static u32 NormalInterpolator_get_field_count(GF_Node *node, u8 IndexMode)
{
	switch(IndexMode) {
	case GF_SG_FIELD_CODING_IN: return 3;
	case GF_SG_FIELD_CODING_DEF: return 2;
	case GF_SG_FIELD_CODING_OUT: return 3;
	case GF_SG_FIELD_CODING_DYN: return 0;
	default:
		return 4;
	}
}

static GF_Err NormalInterpolator_get_field_index(GF_Node *n, u32 inField, u8 IndexMode, u32 *allField)
{
	switch(IndexMode) {
	case GF_SG_FIELD_CODING_IN:
		*allField = NormalInterpolator_In2All[inField];
		return GF_OK;
	case GF_SG_FIELD_CODING_DEF:
		*allField = NormalInterpolator_Def2All[inField];
		return GF_OK;
	case GF_SG_FIELD_CODING_OUT:
		*allField = NormalInterpolator_Out2All[inField];
		return GF_OK;
	default:
		return GF_BAD_PARAM;
	}
}
static GF_Err NormalInterpolator_get_field(GF_Node *node, GF_FieldInfo *info)
{
	switch (info->fieldIndex) {
	case 0:
		info->name = "set_fraction";
		info->eventType = GF_SG_EVENT_IN;
		info->on_event_in = ((M_NormalInterpolator *)node)->on_set_fraction;
		info->fieldType = GF_SG_VRML_SFFLOAT;
		info->far_ptr = & ((M_NormalInterpolator *) node)->set_fraction;
		return GF_OK;
	case 1:
		info->name = "key";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_MFFLOAT;
		info->far_ptr = & ((M_NormalInterpolator *) node)->key;
		return GF_OK;
	case 2:
		info->name = "keyValue";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_MFVEC3F;
		info->far_ptr = & ((M_NormalInterpolator *) node)->keyValue;
		return GF_OK;
	case 3:
		info->name = "value_changed";
		info->eventType = GF_SG_EVENT_OUT;
		info->fieldType = GF_SG_VRML_MFVEC3F;
		info->far_ptr = & ((M_NormalInterpolator *) node)->value_changed;
		return GF_OK;
	default:
		return GF_BAD_PARAM;
	}
}

static Bool NormalInterpolator_get_aq_info(GF_Node *n, u32 FieldIndex, u8 *QType, u8 *AType, Fixed *b_min, Fixed *b_max, u32 *QT13_bits)
{
	switch (FieldIndex) {
	case 1:
		*AType = 0;
		*QType = 8;
		*b_min = FLT2FIX(0);
		*b_max = FLT2FIX(1);
		return 1;
	case 2:
		*AType = 0;
		*QType = 9;
		*b_min = FIX_MIN;
		*b_max = FIX_MAX;
		return 1;
	default:
		return 0;
	}
}



GF_Node *NormalInterpolator_Create()
{
	M_NormalInterpolator *p;
	GF_SAFEALLOC(p, sizeof(M_NormalInterpolator));
	if(!p) return NULL;
	gf_node_setup((GF_Node *)p, TAG_MPEG4_NormalInterpolator);

#ifdef GF_NODE_USE_POINTERS
	((GF_Node *)p)->sgprivate->name = "NormalInterpolator";
	((GF_Node *)p)->sgprivate->node_del = NormalInterpolator_Del;
	((GF_Node *)p)->sgprivate->get_field_count = NormalInterpolator_get_field_count;
	((GF_Node *)p)->sgprivate->get_field = NormalInterpolator_get_field;
#endif


	/*default field values*/
	return (GF_Node *)p;
}


/*
	OrderedGroup Node deletion
*/

static void OrderedGroup_Del(GF_Node *node)
{
	M_OrderedGroup *p = (M_OrderedGroup *) node;
	gf_sg_vrml_parent_reset((GF_Node *) p);	
	gf_sg_mffloat_del(p->order);
	gf_node_free((GF_Node *) p);
}

static const u16 OrderedGroup_Def2All[] = { 2, 3};
static const u16 OrderedGroup_In2All[] = { 0, 1, 2, 3};
static const u16 OrderedGroup_Out2All[] = { 2, 3};

static u32 OrderedGroup_get_field_count(GF_Node *node, u8 IndexMode)
{
	switch(IndexMode) {
	case GF_SG_FIELD_CODING_IN: return 4;
	case GF_SG_FIELD_CODING_DEF: return 2;
	case GF_SG_FIELD_CODING_OUT: return 2;
	case GF_SG_FIELD_CODING_DYN: return 0;
	default:
		return 4;
	}
}

static GF_Err OrderedGroup_get_field_index(GF_Node *n, u32 inField, u8 IndexMode, u32 *allField)
{
	switch(IndexMode) {
	case GF_SG_FIELD_CODING_IN:
		*allField = OrderedGroup_In2All[inField];
		return GF_OK;
	case GF_SG_FIELD_CODING_DEF:
		*allField = OrderedGroup_Def2All[inField];
		return GF_OK;
	case GF_SG_FIELD_CODING_OUT:
		*allField = OrderedGroup_Out2All[inField];
		return GF_OK;
	default:
		return GF_BAD_PARAM;
	}
}
static GF_Err OrderedGroup_get_field(GF_Node *node, GF_FieldInfo *info)
{
	switch (info->fieldIndex) {
	case 0:
		info->name = "addChildren";
		info->eventType = GF_SG_EVENT_IN;
		info->on_event_in = ((M_OrderedGroup *)node)->on_addChildren;
		info->fieldType = GF_SG_VRML_MFNODE;
		info->NDTtype = NDT_SF3DNode;
		info->far_ptr = & ((M_OrderedGroup *)node)->addChildren;
		return GF_OK;
	case 1:
		info->name = "removeChildren";
		info->eventType = GF_SG_EVENT_IN;
		info->on_event_in = ((M_OrderedGroup *)node)->on_removeChildren;
		info->fieldType = GF_SG_VRML_MFNODE;
		info->NDTtype = NDT_SF3DNode;
		info->far_ptr = & ((M_OrderedGroup *)node)->removeChildren;
		return GF_OK;
	case 2:
		info->name = "children";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_MFNODE;
		info->NDTtype = NDT_SF3DNode;
		info->far_ptr = & ((M_OrderedGroup *)node)->children;
		return GF_OK;
	case 3:
		info->name = "order";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_MFFLOAT;
		info->far_ptr = & ((M_OrderedGroup *) node)->order;
		return GF_OK;
	default:
		return GF_BAD_PARAM;
	}
}

static Bool OrderedGroup_get_aq_info(GF_Node *n, u32 FieldIndex, u8 *QType, u8 *AType, Fixed *b_min, Fixed *b_max, u32 *QT13_bits)
{
	switch (FieldIndex) {
	case 3:
		*AType = 0;
		*QType = 3;
		*b_min = FLT2FIX(0);
		*b_max = FIX_MAX;
		return 1;
	default:
		return 0;
	}
}



GF_Node *OrderedGroup_Create()
{
	M_OrderedGroup *p;
	GF_SAFEALLOC(p, sizeof(M_OrderedGroup));
	if(!p) return NULL;
	gf_node_setup((GF_Node *)p, TAG_MPEG4_OrderedGroup);
	gf_sg_vrml_parent_setup((GF_Node *) p);

#ifdef GF_NODE_USE_POINTERS
	((GF_Node *)p)->sgprivate->name = "OrderedGroup";
	((GF_Node *)p)->sgprivate->node_del = OrderedGroup_Del;
	((GF_Node *)p)->sgprivate->get_field_count = OrderedGroup_get_field_count;
	((GF_Node *)p)->sgprivate->get_field = OrderedGroup_get_field;
#endif


	/*default field values*/
	return (GF_Node *)p;
}


/*
	OrientationInterpolator Node deletion
*/

static void OrientationInterpolator_Del(GF_Node *node)
{
	M_OrientationInterpolator *p = (M_OrientationInterpolator *) node;
	gf_sg_mffloat_del(p->key);
	gf_sg_mfrotation_del(p->keyValue);
	gf_node_free((GF_Node *) p);
}

static const u16 OrientationInterpolator_Def2All[] = { 1, 2};
static const u16 OrientationInterpolator_In2All[] = { 0, 1, 2};
static const u16 OrientationInterpolator_Out2All[] = { 1, 2, 3};

static u32 OrientationInterpolator_get_field_count(GF_Node *node, u8 IndexMode)
{
	switch(IndexMode) {
	case GF_SG_FIELD_CODING_IN: return 3;
	case GF_SG_FIELD_CODING_DEF: return 2;
	case GF_SG_FIELD_CODING_OUT: return 3;
	case GF_SG_FIELD_CODING_DYN: return 0;
	default:
		return 4;
	}
}

static GF_Err OrientationInterpolator_get_field_index(GF_Node *n, u32 inField, u8 IndexMode, u32 *allField)
{
	switch(IndexMode) {
	case GF_SG_FIELD_CODING_IN:
		*allField = OrientationInterpolator_In2All[inField];
		return GF_OK;
	case GF_SG_FIELD_CODING_DEF:
		*allField = OrientationInterpolator_Def2All[inField];
		return GF_OK;
	case GF_SG_FIELD_CODING_OUT:
		*allField = OrientationInterpolator_Out2All[inField];
		return GF_OK;
	default:
		return GF_BAD_PARAM;
	}
}
static GF_Err OrientationInterpolator_get_field(GF_Node *node, GF_FieldInfo *info)
{
	switch (info->fieldIndex) {
	case 0:
		info->name = "set_fraction";
		info->eventType = GF_SG_EVENT_IN;
		info->on_event_in = ((M_OrientationInterpolator *)node)->on_set_fraction;
		info->fieldType = GF_SG_VRML_SFFLOAT;
		info->far_ptr = & ((M_OrientationInterpolator *) node)->set_fraction;
		return GF_OK;
	case 1:
		info->name = "key";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_MFFLOAT;
		info->far_ptr = & ((M_OrientationInterpolator *) node)->key;
		return GF_OK;
	case 2:
		info->name = "keyValue";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_MFROTATION;
		info->far_ptr = & ((M_OrientationInterpolator *) node)->keyValue;
		return GF_OK;
	case 3:
		info->name = "value_changed";
		info->eventType = GF_SG_EVENT_OUT;
		info->fieldType = GF_SG_VRML_SFROTATION;
		info->far_ptr = & ((M_OrientationInterpolator *) node)->value_changed;
		return GF_OK;
	default:
		return GF_BAD_PARAM;
	}
}

static Bool OrientationInterpolator_get_aq_info(GF_Node *n, u32 FieldIndex, u8 *QType, u8 *AType, Fixed *b_min, Fixed *b_max, u32 *QT13_bits)
{
	switch (FieldIndex) {
	case 1:
		*AType = 0;
		*QType = 8;
		*b_min = FLT2FIX(0);
		*b_max = FLT2FIX(1);
		return 1;
	case 2:
		*AType = 0;
		*QType = 10;
		*b_min = FIX_MIN;
		*b_max = FIX_MAX;
		return 1;
	default:
		return 0;
	}
}



GF_Node *OrientationInterpolator_Create()
{
	M_OrientationInterpolator *p;
	GF_SAFEALLOC(p, sizeof(M_OrientationInterpolator));
	if(!p) return NULL;
	gf_node_setup((GF_Node *)p, TAG_MPEG4_OrientationInterpolator);

#ifdef GF_NODE_USE_POINTERS
	((GF_Node *)p)->sgprivate->name = "OrientationInterpolator";
	((GF_Node *)p)->sgprivate->node_del = OrientationInterpolator_Del;
	((GF_Node *)p)->sgprivate->get_field_count = OrientationInterpolator_get_field_count;
	((GF_Node *)p)->sgprivate->get_field = OrientationInterpolator_get_field;
#endif


	/*default field values*/
	return (GF_Node *)p;
}


/*
	PixelTexture Node deletion
*/

static void PixelTexture_Del(GF_Node *node)
{
	M_PixelTexture *p = (M_PixelTexture *) node;
	gf_sg_sfimage_del(p->image);
	gf_node_free((GF_Node *) p);
}

static const u16 PixelTexture_Def2All[] = { 0, 1, 2};
static const u16 PixelTexture_In2All[] = { 0};
static const u16 PixelTexture_Out2All[] = { 0};

static u32 PixelTexture_get_field_count(GF_Node *node, u8 IndexMode)
{
	switch(IndexMode) {
	case GF_SG_FIELD_CODING_IN: return 1;
	case GF_SG_FIELD_CODING_DEF: return 3;
	case GF_SG_FIELD_CODING_OUT: return 1;
	case GF_SG_FIELD_CODING_DYN: return 0;
	default:
		return 3;
	}
}

static GF_Err PixelTexture_get_field_index(GF_Node *n, u32 inField, u8 IndexMode, u32 *allField)
{
	switch(IndexMode) {
	case GF_SG_FIELD_CODING_IN:
		*allField = PixelTexture_In2All[inField];
		return GF_OK;
	case GF_SG_FIELD_CODING_DEF:
		*allField = PixelTexture_Def2All[inField];
		return GF_OK;
	case GF_SG_FIELD_CODING_OUT:
		*allField = PixelTexture_Out2All[inField];
		return GF_OK;
	default:
		return GF_BAD_PARAM;
	}
}
static GF_Err PixelTexture_get_field(GF_Node *node, GF_FieldInfo *info)
{
	switch (info->fieldIndex) {
	case 0:
		info->name = "image";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFIMAGE;
		info->far_ptr = & ((M_PixelTexture *) node)->image;
		return GF_OK;
	case 1:
		info->name = "repeatS";
		info->eventType = GF_SG_EVENT_FIELD;
		info->fieldType = GF_SG_VRML_SFBOOL;
		info->far_ptr = & ((M_PixelTexture *) node)->repeatS;
		return GF_OK;
	case 2:
		info->name = "repeatT";
		info->eventType = GF_SG_EVENT_FIELD;
		info->fieldType = GF_SG_VRML_SFBOOL;
		info->far_ptr = & ((M_PixelTexture *) node)->repeatT;
		return GF_OK;
	default:
		return GF_BAD_PARAM;
	}
}

static Bool PixelTexture_get_aq_info(GF_Node *n, u32 FieldIndex, u8 *QType, u8 *AType, Fixed *b_min, Fixed *b_max, u32 *QT13_bits)
{
	switch (FieldIndex) {
	case 0:
		*AType = 0;
		*QType = 0;
		return 1;
	default:
		return 0;
	}
}



GF_Node *PixelTexture_Create()
{
	M_PixelTexture *p;
	GF_SAFEALLOC(p, sizeof(M_PixelTexture));
	if(!p) return NULL;
	gf_node_setup((GF_Node *)p, TAG_MPEG4_PixelTexture);

#ifdef GF_NODE_USE_POINTERS
	((GF_Node *)p)->sgprivate->name = "PixelTexture";
	((GF_Node *)p)->sgprivate->node_del = PixelTexture_Del;
	((GF_Node *)p)->sgprivate->get_field_count = PixelTexture_get_field_count;
	((GF_Node *)p)->sgprivate->get_field = PixelTexture_get_field;
#endif


	/*default field values*/
	p->repeatS = 1;
	p->repeatT = 1;
	return (GF_Node *)p;
}


/*
	PlaneSensor Node deletion
*/

static void PlaneSensor_Del(GF_Node *node)
{
	M_PlaneSensor *p = (M_PlaneSensor *) node;
	gf_node_free((GF_Node *) p);
}

static const u16 PlaneSensor_Def2All[] = { 0, 1, 2, 3, 4};
static const u16 PlaneSensor_In2All[] = { 0, 1, 2, 3, 4};
static const u16 PlaneSensor_Out2All[] = { 0, 1, 2, 3, 4, 5, 6, 7};

static u32 PlaneSensor_get_field_count(GF_Node *node, u8 IndexMode)
{
	switch(IndexMode) {
	case GF_SG_FIELD_CODING_IN: return 5;
	case GF_SG_FIELD_CODING_DEF: return 5;
	case GF_SG_FIELD_CODING_OUT: return 8;
	case GF_SG_FIELD_CODING_DYN: return 0;
	default:
		return 8;
	}
}

static GF_Err PlaneSensor_get_field_index(GF_Node *n, u32 inField, u8 IndexMode, u32 *allField)
{
	switch(IndexMode) {
	case GF_SG_FIELD_CODING_IN:
		*allField = PlaneSensor_In2All[inField];
		return GF_OK;
	case GF_SG_FIELD_CODING_DEF:
		*allField = PlaneSensor_Def2All[inField];
		return GF_OK;
	case GF_SG_FIELD_CODING_OUT:
		*allField = PlaneSensor_Out2All[inField];
		return GF_OK;
	default:
		return GF_BAD_PARAM;
	}
}
static GF_Err PlaneSensor_get_field(GF_Node *node, GF_FieldInfo *info)
{
	switch (info->fieldIndex) {
	case 0:
		info->name = "autoOffset";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFBOOL;
		info->far_ptr = & ((M_PlaneSensor *) node)->autoOffset;
		return GF_OK;
	case 1:
		info->name = "enabled";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFBOOL;
		info->far_ptr = & ((M_PlaneSensor *) node)->enabled;
		return GF_OK;
	case 2:
		info->name = "maxPosition";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFVEC2F;
		info->far_ptr = & ((M_PlaneSensor *) node)->maxPosition;
		return GF_OK;
	case 3:
		info->name = "minPosition";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFVEC2F;
		info->far_ptr = & ((M_PlaneSensor *) node)->minPosition;
		return GF_OK;
	case 4:
		info->name = "offset";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFVEC3F;
		info->far_ptr = & ((M_PlaneSensor *) node)->offset;
		return GF_OK;
	case 5:
		info->name = "isActive";
		info->eventType = GF_SG_EVENT_OUT;
		info->fieldType = GF_SG_VRML_SFBOOL;
		info->far_ptr = & ((M_PlaneSensor *) node)->isActive;
		return GF_OK;
	case 6:
		info->name = "trackPoint_changed";
		info->eventType = GF_SG_EVENT_OUT;
		info->fieldType = GF_SG_VRML_SFVEC3F;
		info->far_ptr = & ((M_PlaneSensor *) node)->trackPoint_changed;
		return GF_OK;
	case 7:
		info->name = "translation_changed";
		info->eventType = GF_SG_EVENT_OUT;
		info->fieldType = GF_SG_VRML_SFVEC3F;
		info->far_ptr = & ((M_PlaneSensor *) node)->translation_changed;
		return GF_OK;
	default:
		return GF_BAD_PARAM;
	}
}

static Bool PlaneSensor_get_aq_info(GF_Node *n, u32 FieldIndex, u8 *QType, u8 *AType, Fixed *b_min, Fixed *b_max, u32 *QT13_bits)
{
	switch (FieldIndex) {
	case 2:
		*AType = 0;
		*QType = 2;
		*b_min = FIX_MIN;
		*b_max = FIX_MAX;
		return 1;
	case 3:
		*AType = 0;
		*QType = 2;
		*b_min = FIX_MIN;
		*b_max = FIX_MAX;
		return 1;
	case 4:
		*AType = 0;
		*QType = 1;
		*b_min = FIX_MIN;
		*b_max = FIX_MAX;
		return 1;
	default:
		return 0;
	}
}



GF_Node *PlaneSensor_Create()
{
	M_PlaneSensor *p;
	GF_SAFEALLOC(p, sizeof(M_PlaneSensor));
	if(!p) return NULL;
	gf_node_setup((GF_Node *)p, TAG_MPEG4_PlaneSensor);

#ifdef GF_NODE_USE_POINTERS
	((GF_Node *)p)->sgprivate->name = "PlaneSensor";
	((GF_Node *)p)->sgprivate->node_del = PlaneSensor_Del;
	((GF_Node *)p)->sgprivate->get_field_count = PlaneSensor_get_field_count;
	((GF_Node *)p)->sgprivate->get_field = PlaneSensor_get_field;
#endif


	/*default field values*/
	p->autoOffset = 1;
	p->enabled = 1;
	p->maxPosition.x = FLT2FIX(-1);
	p->maxPosition.y = FLT2FIX(-1);
	p->minPosition.x = FLT2FIX(0);
	p->minPosition.y = FLT2FIX(0);
	p->offset.x = FLT2FIX(0);
	p->offset.y = FLT2FIX(0);
	p->offset.z = FLT2FIX(0);
	return (GF_Node *)p;
}


/*
	PlaneSensor2D Node deletion
*/

static void PlaneSensor2D_Del(GF_Node *node)
{
	M_PlaneSensor2D *p = (M_PlaneSensor2D *) node;
	gf_node_free((GF_Node *) p);
}

static const u16 PlaneSensor2D_Def2All[] = { 0, 1, 2, 3, 4};
static const u16 PlaneSensor2D_In2All[] = { 0, 1, 2, 3, 4};
static const u16 PlaneSensor2D_Out2All[] = { 0, 1, 2, 3, 4, 5, 6, 7};

static u32 PlaneSensor2D_get_field_count(GF_Node *node, u8 IndexMode)
{
	switch(IndexMode) {
	case GF_SG_FIELD_CODING_IN: return 5;
	case GF_SG_FIELD_CODING_DEF: return 5;
	case GF_SG_FIELD_CODING_OUT: return 8;
	case GF_SG_FIELD_CODING_DYN: return 0;
	default:
		return 8;
	}
}

static GF_Err PlaneSensor2D_get_field_index(GF_Node *n, u32 inField, u8 IndexMode, u32 *allField)
{
	switch(IndexMode) {
	case GF_SG_FIELD_CODING_IN:
		*allField = PlaneSensor2D_In2All[inField];
		return GF_OK;
	case GF_SG_FIELD_CODING_DEF:
		*allField = PlaneSensor2D_Def2All[inField];
		return GF_OK;
	case GF_SG_FIELD_CODING_OUT:
		*allField = PlaneSensor2D_Out2All[inField];
		return GF_OK;
	default:
		return GF_BAD_PARAM;
	}
}
static GF_Err PlaneSensor2D_get_field(GF_Node *node, GF_FieldInfo *info)
{
	switch (info->fieldIndex) {
	case 0:
		info->name = "autoOffset";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFBOOL;
		info->far_ptr = & ((M_PlaneSensor2D *) node)->autoOffset;
		return GF_OK;
	case 1:
		info->name = "enabled";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFBOOL;
		info->far_ptr = & ((M_PlaneSensor2D *) node)->enabled;
		return GF_OK;
	case 2:
		info->name = "maxPosition";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFVEC2F;
		info->far_ptr = & ((M_PlaneSensor2D *) node)->maxPosition;
		return GF_OK;
	case 3:
		info->name = "minPosition";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFVEC2F;
		info->far_ptr = & ((M_PlaneSensor2D *) node)->minPosition;
		return GF_OK;
	case 4:
		info->name = "offset";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFVEC2F;
		info->far_ptr = & ((M_PlaneSensor2D *) node)->offset;
		return GF_OK;
	case 5:
		info->name = "isActive";
		info->eventType = GF_SG_EVENT_OUT;
		info->fieldType = GF_SG_VRML_SFBOOL;
		info->far_ptr = & ((M_PlaneSensor2D *) node)->isActive;
		return GF_OK;
	case 6:
		info->name = "trackPoint_changed";
		info->eventType = GF_SG_EVENT_OUT;
		info->fieldType = GF_SG_VRML_SFVEC2F;
		info->far_ptr = & ((M_PlaneSensor2D *) node)->trackPoint_changed;
		return GF_OK;
	case 7:
		info->name = "translation_changed";
		info->eventType = GF_SG_EVENT_OUT;
		info->fieldType = GF_SG_VRML_SFVEC2F;
		info->far_ptr = & ((M_PlaneSensor2D *) node)->translation_changed;
		return GF_OK;
	default:
		return GF_BAD_PARAM;
	}
}

static Bool PlaneSensor2D_get_aq_info(GF_Node *n, u32 FieldIndex, u8 *QType, u8 *AType, Fixed *b_min, Fixed *b_max, u32 *QT13_bits)
{
	switch (FieldIndex) {
	case 2:
		*AType = 0;
		*QType = 2;
		*b_min = FIX_MIN;
		*b_max = FIX_MAX;
		return 1;
	case 3:
		*AType = 0;
		*QType = 2;
		*b_min = FIX_MIN;
		*b_max = FIX_MAX;
		return 1;
	case 4:
		*AType = 0;
		*QType = 12;
		*b_min = FIX_MIN;
		*b_max = FIX_MAX;
		return 1;
	default:
		return 0;
	}
}



GF_Node *PlaneSensor2D_Create()
{
	M_PlaneSensor2D *p;
	GF_SAFEALLOC(p, sizeof(M_PlaneSensor2D));
	if(!p) return NULL;
	gf_node_setup((GF_Node *)p, TAG_MPEG4_PlaneSensor2D);

#ifdef GF_NODE_USE_POINTERS
	((GF_Node *)p)->sgprivate->name = "PlaneSensor2D";
	((GF_Node *)p)->sgprivate->node_del = PlaneSensor2D_Del;
	((GF_Node *)p)->sgprivate->get_field_count = PlaneSensor2D_get_field_count;
	((GF_Node *)p)->sgprivate->get_field = PlaneSensor2D_get_field;
#endif


	/*default field values*/
	p->autoOffset = 1;
	p->enabled = 1;
	p->maxPosition.x = FLT2FIX(0);
	p->maxPosition.y = FLT2FIX(0);
	p->minPosition.x = FLT2FIX(0);
	p->minPosition.y = FLT2FIX(0);
	p->offset.x = FLT2FIX(0);
	p->offset.y = FLT2FIX(0);
	return (GF_Node *)p;
}


/*
	PointLight Node deletion
*/

static void PointLight_Del(GF_Node *node)
{
	M_PointLight *p = (M_PointLight *) node;
	gf_node_free((GF_Node *) p);
}

static const u16 PointLight_Def2All[] = { 0, 1, 2, 3, 4, 5, 6};
static const u16 PointLight_In2All[] = { 0, 1, 2, 3, 4, 5, 6};
static const u16 PointLight_Out2All[] = { 0, 1, 2, 3, 4, 5, 6};
static const u16 PointLight_Dyn2All[] = { 0, 1, 2, 3, 4, 6};

static u32 PointLight_get_field_count(GF_Node *node, u8 IndexMode)
{
	switch(IndexMode) {
	case GF_SG_FIELD_CODING_IN: return 7;
	case GF_SG_FIELD_CODING_DEF: return 7;
	case GF_SG_FIELD_CODING_OUT: return 7;
	case GF_SG_FIELD_CODING_DYN: return 6;
	default:
		return 7;
	}
}

static GF_Err PointLight_get_field_index(GF_Node *n, u32 inField, u8 IndexMode, u32 *allField)
{
	switch(IndexMode) {
	case GF_SG_FIELD_CODING_IN:
		*allField = PointLight_In2All[inField];
		return GF_OK;
	case GF_SG_FIELD_CODING_DEF:
		*allField = PointLight_Def2All[inField];
		return GF_OK;
	case GF_SG_FIELD_CODING_OUT:
		*allField = PointLight_Out2All[inField];
		return GF_OK;
	case GF_SG_FIELD_CODING_DYN:
		*allField = PointLight_Dyn2All[inField];
		return GF_OK;
	default:
		return GF_BAD_PARAM;
	}
}
static GF_Err PointLight_get_field(GF_Node *node, GF_FieldInfo *info)
{
	switch (info->fieldIndex) {
	case 0:
		info->name = "ambientIntensity";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFFLOAT;
		info->far_ptr = & ((M_PointLight *) node)->ambientIntensity;
		return GF_OK;
	case 1:
		info->name = "attenuation";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFVEC3F;
		info->far_ptr = & ((M_PointLight *) node)->attenuation;
		return GF_OK;
	case 2:
		info->name = "color";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFCOLOR;
		info->far_ptr = & ((M_PointLight *) node)->color;
		return GF_OK;
	case 3:
		info->name = "intensity";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFFLOAT;
		info->far_ptr = & ((M_PointLight *) node)->intensity;
		return GF_OK;
	case 4:
		info->name = "location";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFVEC3F;
		info->far_ptr = & ((M_PointLight *) node)->location;
		return GF_OK;
	case 5:
		info->name = "on";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFBOOL;
		info->far_ptr = & ((M_PointLight *) node)->on;
		return GF_OK;
	case 6:
		info->name = "radius";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFFLOAT;
		info->far_ptr = & ((M_PointLight *) node)->radius;
		return GF_OK;
	default:
		return GF_BAD_PARAM;
	}
}

static Bool PointLight_get_aq_info(GF_Node *n, u32 FieldIndex, u8 *QType, u8 *AType, Fixed *b_min, Fixed *b_max, u32 *QT13_bits)
{
	switch (FieldIndex) {
	case 0:
		*AType = 8;
		*QType = 4;
		*b_min = FLT2FIX(0);
		*b_max = FLT2FIX(1);
		return 1;
	case 1:
		*AType = 1;
		*QType = 11;
		*b_min = FLT2FIX(0);
		*b_max = FIX_MAX;
		return 1;
	case 2:
		*AType = 4;
		*QType = 4;
		*b_min = FLT2FIX(0);
		*b_max = FLT2FIX(1);
		return 1;
	case 3:
		*AType = 8;
		*QType = 4;
		*b_min = FLT2FIX(0);
		*b_max = FLT2FIX(1);
		return 1;
	case 4:
		*AType = 1;
		*QType = 1;
		*b_min = FIX_MIN;
		*b_max = FIX_MAX;
		return 1;
	case 6:
		*AType = 7;
		*QType = 11;
		*b_min = FLT2FIX(0);
		*b_max = FIX_MAX;
		return 1;
	default:
		return 0;
	}
}



GF_Node *PointLight_Create()
{
	M_PointLight *p;
	GF_SAFEALLOC(p, sizeof(M_PointLight));
	if(!p) return NULL;
	gf_node_setup((GF_Node *)p, TAG_MPEG4_PointLight);

#ifdef GF_NODE_USE_POINTERS
	((GF_Node *)p)->sgprivate->name = "PointLight";
	((GF_Node *)p)->sgprivate->node_del = PointLight_Del;
	((GF_Node *)p)->sgprivate->get_field_count = PointLight_get_field_count;
	((GF_Node *)p)->sgprivate->get_field = PointLight_get_field;
#endif


	/*default field values*/
	p->ambientIntensity = FLT2FIX(0);
	p->attenuation.x = FLT2FIX(1);
	p->attenuation.y = FLT2FIX(0);
	p->attenuation.z = FLT2FIX(0);
	p->color.red = FLT2FIX(1);
	p->color.green = FLT2FIX(1);
	p->color.blue = FLT2FIX(1);
	p->intensity = FLT2FIX(1);
	p->location.x = FLT2FIX(0);
	p->location.y = FLT2FIX(0);
	p->location.z = FLT2FIX(0);
	p->on = 1;
	p->radius = FLT2FIX(100);
	return (GF_Node *)p;
}


/*
	PointSet Node deletion
*/

static void PointSet_Del(GF_Node *node)
{
	M_PointSet *p = (M_PointSet *) node;
	gf_node_unregister((GF_Node *) p->color, (GF_Node *) p);	
	gf_node_unregister((GF_Node *) p->coord, (GF_Node *) p);	
	gf_node_free((GF_Node *) p);
}

static const u16 PointSet_Def2All[] = { 0, 1};
static const u16 PointSet_In2All[] = { 0, 1};
static const u16 PointSet_Out2All[] = { 0, 1};

static u32 PointSet_get_field_count(GF_Node *node, u8 IndexMode)
{
	switch(IndexMode) {
	case GF_SG_FIELD_CODING_IN: return 2;
	case GF_SG_FIELD_CODING_DEF: return 2;
	case GF_SG_FIELD_CODING_OUT: return 2;
	case GF_SG_FIELD_CODING_DYN: return 0;
	default:
		return 2;
	}
}

static GF_Err PointSet_get_field_index(GF_Node *n, u32 inField, u8 IndexMode, u32 *allField)
{
	switch(IndexMode) {
	case GF_SG_FIELD_CODING_IN:
		*allField = PointSet_In2All[inField];
		return GF_OK;
	case GF_SG_FIELD_CODING_DEF:
		*allField = PointSet_Def2All[inField];
		return GF_OK;
	case GF_SG_FIELD_CODING_OUT:
		*allField = PointSet_Out2All[inField];
		return GF_OK;
	default:
		return GF_BAD_PARAM;
	}
}
static GF_Err PointSet_get_field(GF_Node *node, GF_FieldInfo *info)
{
	switch (info->fieldIndex) {
	case 0:
		info->name = "color";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFNODE;
		info->NDTtype = NDT_SFColorNode;
		info->far_ptr = & ((M_PointSet *)node)->color;
		return GF_OK;
	case 1:
		info->name = "coord";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFNODE;
		info->NDTtype = NDT_SFCoordinateNode;
		info->far_ptr = & ((M_PointSet *)node)->coord;
		return GF_OK;
	default:
		return GF_BAD_PARAM;
	}
}

static Bool PointSet_get_aq_info(GF_Node *n, u32 FieldIndex, u8 *QType, u8 *AType, Fixed *b_min, Fixed *b_max, u32 *QT13_bits)
{
	switch (FieldIndex) {
	default:
		return 0;
	}
}



GF_Node *PointSet_Create()
{
	M_PointSet *p;
	GF_SAFEALLOC(p, sizeof(M_PointSet));
	if(!p) return NULL;
	gf_node_setup((GF_Node *)p, TAG_MPEG4_PointSet);

#ifdef GF_NODE_USE_POINTERS
	((GF_Node *)p)->sgprivate->name = "PointSet";
	((GF_Node *)p)->sgprivate->node_del = PointSet_Del;
	((GF_Node *)p)->sgprivate->get_field_count = PointSet_get_field_count;
	((GF_Node *)p)->sgprivate->get_field = PointSet_get_field;
#endif


	/*default field values*/
	return (GF_Node *)p;
}


/*
	PointSet2D Node deletion
*/

static void PointSet2D_Del(GF_Node *node)
{
	M_PointSet2D *p = (M_PointSet2D *) node;
	gf_node_unregister((GF_Node *) p->color, (GF_Node *) p);	
	gf_node_unregister((GF_Node *) p->coord, (GF_Node *) p);	
	gf_node_free((GF_Node *) p);
}

static const u16 PointSet2D_Def2All[] = { 0, 1};
static const u16 PointSet2D_In2All[] = { 0, 1};
static const u16 PointSet2D_Out2All[] = { 0, 1};

static u32 PointSet2D_get_field_count(GF_Node *node, u8 IndexMode)
{
	switch(IndexMode) {
	case GF_SG_FIELD_CODING_IN: return 2;
	case GF_SG_FIELD_CODING_DEF: return 2;
	case GF_SG_FIELD_CODING_OUT: return 2;
	case GF_SG_FIELD_CODING_DYN: return 0;
	default:
		return 2;
	}
}

static GF_Err PointSet2D_get_field_index(GF_Node *n, u32 inField, u8 IndexMode, u32 *allField)
{
	switch(IndexMode) {
	case GF_SG_FIELD_CODING_IN:
		*allField = PointSet2D_In2All[inField];
		return GF_OK;
	case GF_SG_FIELD_CODING_DEF:
		*allField = PointSet2D_Def2All[inField];
		return GF_OK;
	case GF_SG_FIELD_CODING_OUT:
		*allField = PointSet2D_Out2All[inField];
		return GF_OK;
	default:
		return GF_BAD_PARAM;
	}
}
static GF_Err PointSet2D_get_field(GF_Node *node, GF_FieldInfo *info)
{
	switch (info->fieldIndex) {
	case 0:
		info->name = "color";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFNODE;
		info->NDTtype = NDT_SFColorNode;
		info->far_ptr = & ((M_PointSet2D *)node)->color;
		return GF_OK;
	case 1:
		info->name = "coord";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFNODE;
		info->NDTtype = NDT_SFCoordinate2DNode;
		info->far_ptr = & ((M_PointSet2D *)node)->coord;
		return GF_OK;
	default:
		return GF_BAD_PARAM;
	}
}

static Bool PointSet2D_get_aq_info(GF_Node *n, u32 FieldIndex, u8 *QType, u8 *AType, Fixed *b_min, Fixed *b_max, u32 *QT13_bits)
{
	switch (FieldIndex) {
	default:
		return 0;
	}
}



GF_Node *PointSet2D_Create()
{
	M_PointSet2D *p;
	GF_SAFEALLOC(p, sizeof(M_PointSet2D));
	if(!p) return NULL;
	gf_node_setup((GF_Node *)p, TAG_MPEG4_PointSet2D);

#ifdef GF_NODE_USE_POINTERS
	((GF_Node *)p)->sgprivate->name = "PointSet2D";
	((GF_Node *)p)->sgprivate->node_del = PointSet2D_Del;
	((GF_Node *)p)->sgprivate->get_field_count = PointSet2D_get_field_count;
	((GF_Node *)p)->sgprivate->get_field = PointSet2D_get_field;
#endif


	/*default field values*/
	return (GF_Node *)p;
}


/*
	PositionInterpolator Node deletion
*/

static void PositionInterpolator_Del(GF_Node *node)
{
	M_PositionInterpolator *p = (M_PositionInterpolator *) node;
	gf_sg_mffloat_del(p->key);
	gf_sg_mfvec3f_del(p->keyValue);
	gf_node_free((GF_Node *) p);
}

static const u16 PositionInterpolator_Def2All[] = { 1, 2};
static const u16 PositionInterpolator_In2All[] = { 0, 1, 2};
static const u16 PositionInterpolator_Out2All[] = { 1, 2, 3};

static u32 PositionInterpolator_get_field_count(GF_Node *node, u8 IndexMode)
{
	switch(IndexMode) {
	case GF_SG_FIELD_CODING_IN: return 3;
	case GF_SG_FIELD_CODING_DEF: return 2;
	case GF_SG_FIELD_CODING_OUT: return 3;
	case GF_SG_FIELD_CODING_DYN: return 0;
	default:
		return 4;
	}
}

static GF_Err PositionInterpolator_get_field_index(GF_Node *n, u32 inField, u8 IndexMode, u32 *allField)
{
	switch(IndexMode) {
	case GF_SG_FIELD_CODING_IN:
		*allField = PositionInterpolator_In2All[inField];
		return GF_OK;
	case GF_SG_FIELD_CODING_DEF:
		*allField = PositionInterpolator_Def2All[inField];
		return GF_OK;
	case GF_SG_FIELD_CODING_OUT:
		*allField = PositionInterpolator_Out2All[inField];
		return GF_OK;
	default:
		return GF_BAD_PARAM;
	}
}
static GF_Err PositionInterpolator_get_field(GF_Node *node, GF_FieldInfo *info)
{
	switch (info->fieldIndex) {
	case 0:
		info->name = "set_fraction";
		info->eventType = GF_SG_EVENT_IN;
		info->on_event_in = ((M_PositionInterpolator *)node)->on_set_fraction;
		info->fieldType = GF_SG_VRML_SFFLOAT;
		info->far_ptr = & ((M_PositionInterpolator *) node)->set_fraction;
		return GF_OK;
	case 1:
		info->name = "key";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_MFFLOAT;
		info->far_ptr = & ((M_PositionInterpolator *) node)->key;
		return GF_OK;
	case 2:
		info->name = "keyValue";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_MFVEC3F;
		info->far_ptr = & ((M_PositionInterpolator *) node)->keyValue;
		return GF_OK;
	case 3:
		info->name = "value_changed";
		info->eventType = GF_SG_EVENT_OUT;
		info->fieldType = GF_SG_VRML_SFVEC3F;
		info->far_ptr = & ((M_PositionInterpolator *) node)->value_changed;
		return GF_OK;
	default:
		return GF_BAD_PARAM;
	}
}

static Bool PositionInterpolator_get_aq_info(GF_Node *n, u32 FieldIndex, u8 *QType, u8 *AType, Fixed *b_min, Fixed *b_max, u32 *QT13_bits)
{
	switch (FieldIndex) {
	case 1:
		*AType = 0;
		*QType = 8;
		*b_min = FLT2FIX(0);
		*b_max = FLT2FIX(1);
		return 1;
	case 2:
		*AType = 0;
		*QType = 1;
		*b_min = FIX_MIN;
		*b_max = FIX_MAX;
		return 1;
	default:
		return 0;
	}
}



GF_Node *PositionInterpolator_Create()
{
	M_PositionInterpolator *p;
	GF_SAFEALLOC(p, sizeof(M_PositionInterpolator));
	if(!p) return NULL;
	gf_node_setup((GF_Node *)p, TAG_MPEG4_PositionInterpolator);

#ifdef GF_NODE_USE_POINTERS
	((GF_Node *)p)->sgprivate->name = "PositionInterpolator";
	((GF_Node *)p)->sgprivate->node_del = PositionInterpolator_Del;
	((GF_Node *)p)->sgprivate->get_field_count = PositionInterpolator_get_field_count;
	((GF_Node *)p)->sgprivate->get_field = PositionInterpolator_get_field;
#endif


	/*default field values*/
	return (GF_Node *)p;
}


/*
	PositionInterpolator2D Node deletion
*/

static void PositionInterpolator2D_Del(GF_Node *node)
{
	M_PositionInterpolator2D *p = (M_PositionInterpolator2D *) node;
	gf_sg_mffloat_del(p->key);
	gf_sg_mfvec2f_del(p->keyValue);
	gf_node_free((GF_Node *) p);
}

static const u16 PositionInterpolator2D_Def2All[] = { 1, 2};
static const u16 PositionInterpolator2D_In2All[] = { 0, 1, 2};
static const u16 PositionInterpolator2D_Out2All[] = { 1, 2, 3};

static u32 PositionInterpolator2D_get_field_count(GF_Node *node, u8 IndexMode)
{
	switch(IndexMode) {
	case GF_SG_FIELD_CODING_IN: return 3;
	case GF_SG_FIELD_CODING_DEF: return 2;
	case GF_SG_FIELD_CODING_OUT: return 3;
	case GF_SG_FIELD_CODING_DYN: return 0;
	default:
		return 4;
	}
}

static GF_Err PositionInterpolator2D_get_field_index(GF_Node *n, u32 inField, u8 IndexMode, u32 *allField)
{
	switch(IndexMode) {
	case GF_SG_FIELD_CODING_IN:
		*allField = PositionInterpolator2D_In2All[inField];
		return GF_OK;
	case GF_SG_FIELD_CODING_DEF:
		*allField = PositionInterpolator2D_Def2All[inField];
		return GF_OK;
	case GF_SG_FIELD_CODING_OUT:
		*allField = PositionInterpolator2D_Out2All[inField];
		return GF_OK;
	default:
		return GF_BAD_PARAM;
	}
}
static GF_Err PositionInterpolator2D_get_field(GF_Node *node, GF_FieldInfo *info)
{
	switch (info->fieldIndex) {
	case 0:
		info->name = "set_fraction";
		info->eventType = GF_SG_EVENT_IN;
		info->on_event_in = ((M_PositionInterpolator2D *)node)->on_set_fraction;
		info->fieldType = GF_SG_VRML_SFFLOAT;
		info->far_ptr = & ((M_PositionInterpolator2D *) node)->set_fraction;
		return GF_OK;
	case 1:
		info->name = "key";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_MFFLOAT;
		info->far_ptr = & ((M_PositionInterpolator2D *) node)->key;
		return GF_OK;
	case 2:
		info->name = "keyValue";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_MFVEC2F;
		info->far_ptr = & ((M_PositionInterpolator2D *) node)->keyValue;
		return GF_OK;
	case 3:
		info->name = "value_changed";
		info->eventType = GF_SG_EVENT_OUT;
		info->fieldType = GF_SG_VRML_SFVEC2F;
		info->far_ptr = & ((M_PositionInterpolator2D *) node)->value_changed;
		return GF_OK;
	default:
		return GF_BAD_PARAM;
	}
}

static Bool PositionInterpolator2D_get_aq_info(GF_Node *n, u32 FieldIndex, u8 *QType, u8 *AType, Fixed *b_min, Fixed *b_max, u32 *QT13_bits)
{
	switch (FieldIndex) {
	case 1:
		*AType = 0;
		*QType = 8;
		*b_min = FLT2FIX(0);
		*b_max = FLT2FIX(1);
		return 1;
	case 2:
		*AType = 0;
		*QType = 2;
		*b_min = FIX_MIN;
		*b_max = FIX_MAX;
		return 1;
	default:
		return 0;
	}
}



GF_Node *PositionInterpolator2D_Create()
{
	M_PositionInterpolator2D *p;
	GF_SAFEALLOC(p, sizeof(M_PositionInterpolator2D));
	if(!p) return NULL;
	gf_node_setup((GF_Node *)p, TAG_MPEG4_PositionInterpolator2D);

#ifdef GF_NODE_USE_POINTERS
	((GF_Node *)p)->sgprivate->name = "PositionInterpolator2D";
	((GF_Node *)p)->sgprivate->node_del = PositionInterpolator2D_Del;
	((GF_Node *)p)->sgprivate->get_field_count = PositionInterpolator2D_get_field_count;
	((GF_Node *)p)->sgprivate->get_field = PositionInterpolator2D_get_field;
#endif


	/*default field values*/
	return (GF_Node *)p;
}


/*
	ProximitySensor2D Node deletion
*/

static void ProximitySensor2D_Del(GF_Node *node)
{
	M_ProximitySensor2D *p = (M_ProximitySensor2D *) node;
	gf_node_free((GF_Node *) p);
}

static const u16 ProximitySensor2D_Def2All[] = { 0, 1, 2};
static const u16 ProximitySensor2D_In2All[] = { 0, 1, 2};
static const u16 ProximitySensor2D_Out2All[] = { 0, 1, 2, 3, 4, 5, 6, 7};

static u32 ProximitySensor2D_get_field_count(GF_Node *node, u8 IndexMode)
{
	switch(IndexMode) {
	case GF_SG_FIELD_CODING_IN: return 3;
	case GF_SG_FIELD_CODING_DEF: return 3;
	case GF_SG_FIELD_CODING_OUT: return 8;
	case GF_SG_FIELD_CODING_DYN: return 0;
	default:
		return 8;
	}
}

static GF_Err ProximitySensor2D_get_field_index(GF_Node *n, u32 inField, u8 IndexMode, u32 *allField)
{
	switch(IndexMode) {
	case GF_SG_FIELD_CODING_IN:
		*allField = ProximitySensor2D_In2All[inField];
		return GF_OK;
	case GF_SG_FIELD_CODING_DEF:
		*allField = ProximitySensor2D_Def2All[inField];
		return GF_OK;
	case GF_SG_FIELD_CODING_OUT:
		*allField = ProximitySensor2D_Out2All[inField];
		return GF_OK;
	default:
		return GF_BAD_PARAM;
	}
}
static GF_Err ProximitySensor2D_get_field(GF_Node *node, GF_FieldInfo *info)
{
	switch (info->fieldIndex) {
	case 0:
		info->name = "center";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFVEC2F;
		info->far_ptr = & ((M_ProximitySensor2D *) node)->center;
		return GF_OK;
	case 1:
		info->name = "size";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFVEC2F;
		info->far_ptr = & ((M_ProximitySensor2D *) node)->size;
		return GF_OK;
	case 2:
		info->name = "enabled";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFBOOL;
		info->far_ptr = & ((M_ProximitySensor2D *) node)->enabled;
		return GF_OK;
	case 3:
		info->name = "isActive";
		info->eventType = GF_SG_EVENT_OUT;
		info->fieldType = GF_SG_VRML_SFBOOL;
		info->far_ptr = & ((M_ProximitySensor2D *) node)->isActive;
		return GF_OK;
	case 4:
		info->name = "position_changed";
		info->eventType = GF_SG_EVENT_OUT;
		info->fieldType = GF_SG_VRML_SFVEC2F;
		info->far_ptr = & ((M_ProximitySensor2D *) node)->position_changed;
		return GF_OK;
	case 5:
		info->name = "orientation_changed";
		info->eventType = GF_SG_EVENT_OUT;
		info->fieldType = GF_SG_VRML_SFFLOAT;
		info->far_ptr = & ((M_ProximitySensor2D *) node)->orientation_changed;
		return GF_OK;
	case 6:
		info->name = "enterTime";
		info->eventType = GF_SG_EVENT_OUT;
		info->fieldType = GF_SG_VRML_SFTIME;
		info->far_ptr = & ((M_ProximitySensor2D *) node)->enterTime;
		return GF_OK;
	case 7:
		info->name = "exitTime";
		info->eventType = GF_SG_EVENT_OUT;
		info->fieldType = GF_SG_VRML_SFTIME;
		info->far_ptr = & ((M_ProximitySensor2D *) node)->exitTime;
		return GF_OK;
	default:
		return GF_BAD_PARAM;
	}
}

static Bool ProximitySensor2D_get_aq_info(GF_Node *n, u32 FieldIndex, u8 *QType, u8 *AType, Fixed *b_min, Fixed *b_max, u32 *QT13_bits)
{
	switch (FieldIndex) {
	case 0:
		*AType = 0;
		*QType = 2;
		*b_min = FLT2FIX(-1);
		*b_max = FIX_MAX;
		return 1;
	case 1:
		*AType = 0;
		*QType = 12;
		*b_min = FLT2FIX(0);
		*b_max = FIX_MAX;
		return 1;
	default:
		return 0;
	}
}



GF_Node *ProximitySensor2D_Create()
{
	M_ProximitySensor2D *p;
	GF_SAFEALLOC(p, sizeof(M_ProximitySensor2D));
	if(!p) return NULL;
	gf_node_setup((GF_Node *)p, TAG_MPEG4_ProximitySensor2D);

#ifdef GF_NODE_USE_POINTERS
	((GF_Node *)p)->sgprivate->name = "ProximitySensor2D";
	((GF_Node *)p)->sgprivate->node_del = ProximitySensor2D_Del;
	((GF_Node *)p)->sgprivate->get_field_count = ProximitySensor2D_get_field_count;
	((GF_Node *)p)->sgprivate->get_field = ProximitySensor2D_get_field;
#endif


	/*default field values*/
	p->center.x = FLT2FIX(0);
	p->center.y = FLT2FIX(0);
	p->size.x = FLT2FIX(0);
	p->size.y = FLT2FIX(0);
	p->enabled = 1;
	return (GF_Node *)p;
}


/*
	ProximitySensor Node deletion
*/

static void ProximitySensor_Del(GF_Node *node)
{
	M_ProximitySensor *p = (M_ProximitySensor *) node;
	gf_node_free((GF_Node *) p);
}

static const u16 ProximitySensor_Def2All[] = { 0, 1, 2};
static const u16 ProximitySensor_In2All[] = { 0, 1, 2};
static const u16 ProximitySensor_Out2All[] = { 0, 1, 2, 3, 4, 5, 6, 7};

static u32 ProximitySensor_get_field_count(GF_Node *node, u8 IndexMode)
{
	switch(IndexMode) {
	case GF_SG_FIELD_CODING_IN: return 3;
	case GF_SG_FIELD_CODING_DEF: return 3;
	case GF_SG_FIELD_CODING_OUT: return 8;
	case GF_SG_FIELD_CODING_DYN: return 0;
	default:
		return 8;
	}
}

static GF_Err ProximitySensor_get_field_index(GF_Node *n, u32 inField, u8 IndexMode, u32 *allField)
{
	switch(IndexMode) {
	case GF_SG_FIELD_CODING_IN:
		*allField = ProximitySensor_In2All[inField];
		return GF_OK;
	case GF_SG_FIELD_CODING_DEF:
		*allField = ProximitySensor_Def2All[inField];
		return GF_OK;
	case GF_SG_FIELD_CODING_OUT:
		*allField = ProximitySensor_Out2All[inField];
		return GF_OK;
	default:
		return GF_BAD_PARAM;
	}
}
static GF_Err ProximitySensor_get_field(GF_Node *node, GF_FieldInfo *info)
{
	switch (info->fieldIndex) {
	case 0:
		info->name = "center";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFVEC3F;
		info->far_ptr = & ((M_ProximitySensor *) node)->center;
		return GF_OK;
	case 1:
		info->name = "size";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFVEC3F;
		info->far_ptr = & ((M_ProximitySensor *) node)->size;
		return GF_OK;
	case 2:
		info->name = "enabled";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFBOOL;
		info->far_ptr = & ((M_ProximitySensor *) node)->enabled;
		return GF_OK;
	case 3:
		info->name = "isActive";
		info->eventType = GF_SG_EVENT_OUT;
		info->fieldType = GF_SG_VRML_SFBOOL;
		info->far_ptr = & ((M_ProximitySensor *) node)->isActive;
		return GF_OK;
	case 4:
		info->name = "position_changed";
		info->eventType = GF_SG_EVENT_OUT;
		info->fieldType = GF_SG_VRML_SFVEC3F;
		info->far_ptr = & ((M_ProximitySensor *) node)->position_changed;
		return GF_OK;
	case 5:
		info->name = "orientation_changed";
		info->eventType = GF_SG_EVENT_OUT;
		info->fieldType = GF_SG_VRML_SFROTATION;
		info->far_ptr = & ((M_ProximitySensor *) node)->orientation_changed;
		return GF_OK;
	case 6:
		info->name = "enterTime";
		info->eventType = GF_SG_EVENT_OUT;
		info->fieldType = GF_SG_VRML_SFTIME;
		info->far_ptr = & ((M_ProximitySensor *) node)->enterTime;
		return GF_OK;
	case 7:
		info->name = "exitTime";
		info->eventType = GF_SG_EVENT_OUT;
		info->fieldType = GF_SG_VRML_SFTIME;
		info->far_ptr = & ((M_ProximitySensor *) node)->exitTime;
		return GF_OK;
	default:
		return GF_BAD_PARAM;
	}
}

static Bool ProximitySensor_get_aq_info(GF_Node *n, u32 FieldIndex, u8 *QType, u8 *AType, Fixed *b_min, Fixed *b_max, u32 *QT13_bits)
{
	switch (FieldIndex) {
	case 0:
		*AType = 0;
		*QType = 1;
		*b_min = FIX_MIN;
		*b_max = FIX_MAX;
		return 1;
	case 1:
		*AType = 0;
		*QType = 11;
		*b_min = FLT2FIX(0);
		*b_max = FIX_MAX;
		return 1;
	default:
		return 0;
	}
}



GF_Node *ProximitySensor_Create()
{
	M_ProximitySensor *p;
	GF_SAFEALLOC(p, sizeof(M_ProximitySensor));
	if(!p) return NULL;
	gf_node_setup((GF_Node *)p, TAG_MPEG4_ProximitySensor);

#ifdef GF_NODE_USE_POINTERS
	((GF_Node *)p)->sgprivate->name = "ProximitySensor";
	((GF_Node *)p)->sgprivate->node_del = ProximitySensor_Del;
	((GF_Node *)p)->sgprivate->get_field_count = ProximitySensor_get_field_count;
	((GF_Node *)p)->sgprivate->get_field = ProximitySensor_get_field;
#endif


	/*default field values*/
	p->center.x = FLT2FIX(0);
	p->center.y = FLT2FIX(0);
	p->center.z = FLT2FIX(0);
	p->size.x = FLT2FIX(0);
	p->size.y = FLT2FIX(0);
	p->size.z = FLT2FIX(0);
	p->enabled = 1;
	return (GF_Node *)p;
}


/*
	QuantizationParameter Node deletion
*/

static void QuantizationParameter_Del(GF_Node *node)
{
	M_QuantizationParameter *p = (M_QuantizationParameter *) node;
	gf_node_free((GF_Node *) p);
}

static const u16 QuantizationParameter_Def2All[] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39};

static u32 QuantizationParameter_get_field_count(GF_Node *node, u8 IndexMode)
{
	switch(IndexMode) {
	case GF_SG_FIELD_CODING_IN: return 0;
	case GF_SG_FIELD_CODING_DEF: return 40;
	case GF_SG_FIELD_CODING_OUT: return 0;
	case GF_SG_FIELD_CODING_DYN: return 0;
	default:
		return 40;
	}
}

static GF_Err QuantizationParameter_get_field_index(GF_Node *n, u32 inField, u8 IndexMode, u32 *allField)
{
	switch(IndexMode) {
	case GF_SG_FIELD_CODING_DEF:
		*allField = QuantizationParameter_Def2All[inField];
		return GF_OK;
	default:
		return GF_BAD_PARAM;
	}
}
static GF_Err QuantizationParameter_get_field(GF_Node *node, GF_FieldInfo *info)
{
	switch (info->fieldIndex) {
	case 0:
		info->name = "isLocal";
		info->eventType = GF_SG_EVENT_FIELD;
		info->fieldType = GF_SG_VRML_SFBOOL;
		info->far_ptr = & ((M_QuantizationParameter *) node)->isLocal;
		return GF_OK;
	case 1:
		info->name = "position3DQuant";
		info->eventType = GF_SG_EVENT_FIELD;
		info->fieldType = GF_SG_VRML_SFBOOL;
		info->far_ptr = & ((M_QuantizationParameter *) node)->position3DQuant;
		return GF_OK;
	case 2:
		info->name = "position3DMin";
		info->eventType = GF_SG_EVENT_FIELD;
		info->fieldType = GF_SG_VRML_SFVEC3F;
		info->far_ptr = & ((M_QuantizationParameter *) node)->position3DMin;
		return GF_OK;
	case 3:
		info->name = "position3DMax";
		info->eventType = GF_SG_EVENT_FIELD;
		info->fieldType = GF_SG_VRML_SFVEC3F;
		info->far_ptr = & ((M_QuantizationParameter *) node)->position3DMax;
		return GF_OK;
	case 4:
		info->name = "position3DNbBits";
		info->eventType = GF_SG_EVENT_FIELD;
		info->fieldType = GF_SG_VRML_SFINT32;
		info->far_ptr = & ((M_QuantizationParameter *) node)->position3DNbBits;
		return GF_OK;
	case 5:
		info->name = "position2DQuant";
		info->eventType = GF_SG_EVENT_FIELD;
		info->fieldType = GF_SG_VRML_SFBOOL;
		info->far_ptr = & ((M_QuantizationParameter *) node)->position2DQuant;
		return GF_OK;
	case 6:
		info->name = "position2DMin";
		info->eventType = GF_SG_EVENT_FIELD;
		info->fieldType = GF_SG_VRML_SFVEC2F;
		info->far_ptr = & ((M_QuantizationParameter *) node)->position2DMin;
		return GF_OK;
	case 7:
		info->name = "position2DMax";
		info->eventType = GF_SG_EVENT_FIELD;
		info->fieldType = GF_SG_VRML_SFVEC2F;
		info->far_ptr = & ((M_QuantizationParameter *) node)->position2DMax;
		return GF_OK;
	case 8:
		info->name = "position2DNbBits";
		info->eventType = GF_SG_EVENT_FIELD;
		info->fieldType = GF_SG_VRML_SFINT32;
		info->far_ptr = & ((M_QuantizationParameter *) node)->position2DNbBits;
		return GF_OK;
	case 9:
		info->name = "drawOrderQuant";
		info->eventType = GF_SG_EVENT_FIELD;
		info->fieldType = GF_SG_VRML_SFBOOL;
		info->far_ptr = & ((M_QuantizationParameter *) node)->drawOrderQuant;
		return GF_OK;
	case 10:
		info->name = "drawOrderMin";
		info->eventType = GF_SG_EVENT_FIELD;
		info->fieldType = GF_SG_VRML_SFFLOAT;
		info->far_ptr = & ((M_QuantizationParameter *) node)->drawOrderMin;
		return GF_OK;
	case 11:
		info->name = "drawOrderMax";
		info->eventType = GF_SG_EVENT_FIELD;
		info->fieldType = GF_SG_VRML_SFFLOAT;
		info->far_ptr = & ((M_QuantizationParameter *) node)->drawOrderMax;
		return GF_OK;
	case 12:
		info->name = "drawOrderNbBits";
		info->eventType = GF_SG_EVENT_FIELD;
		info->fieldType = GF_SG_VRML_SFINT32;
		info->far_ptr = & ((M_QuantizationParameter *) node)->drawOrderNbBits;
		return GF_OK;
	case 13:
		info->name = "colorQuant";
		info->eventType = GF_SG_EVENT_FIELD;
		info->fieldType = GF_SG_VRML_SFBOOL;
		info->far_ptr = & ((M_QuantizationParameter *) node)->colorQuant;
		return GF_OK;
	case 14:
		info->name = "colorMin";
		info->eventType = GF_SG_EVENT_FIELD;
		info->fieldType = GF_SG_VRML_SFFLOAT;
		info->far_ptr = & ((M_QuantizationParameter *) node)->colorMin;
		return GF_OK;
	case 15:
		info->name = "colorMax";
		info->eventType = GF_SG_EVENT_FIELD;
		info->fieldType = GF_SG_VRML_SFFLOAT;
		info->far_ptr = & ((M_QuantizationParameter *) node)->colorMax;
		return GF_OK;
	case 16:
		info->name = "colorNbBits";
		info->eventType = GF_SG_EVENT_FIELD;
		info->fieldType = GF_SG_VRML_SFINT32;
		info->far_ptr = & ((M_QuantizationParameter *) node)->colorNbBits;
		return GF_OK;
	case 17:
		info->name = "textureCoordinateQuant";
		info->eventType = GF_SG_EVENT_FIELD;
		info->fieldType = GF_SG_VRML_SFBOOL;
		info->far_ptr = & ((M_QuantizationParameter *) node)->textureCoordinateQuant;
		return GF_OK;
	case 18:
		info->name = "textureCoordinateMin";
		info->eventType = GF_SG_EVENT_FIELD;
		info->fieldType = GF_SG_VRML_SFFLOAT;
		info->far_ptr = & ((M_QuantizationParameter *) node)->textureCoordinateMin;
		return GF_OK;
	case 19:
		info->name = "textureCoordinateMax";
		info->eventType = GF_SG_EVENT_FIELD;
		info->fieldType = GF_SG_VRML_SFFLOAT;
		info->far_ptr = & ((M_QuantizationParameter *) node)->textureCoordinateMax;
		return GF_OK;
	case 20:
		info->name = "textureCoordinateNbBits";
		info->eventType = GF_SG_EVENT_FIELD;
		info->fieldType = GF_SG_VRML_SFINT32;
		info->far_ptr = & ((M_QuantizationParameter *) node)->textureCoordinateNbBits;
		return GF_OK;
	case 21:
		info->name = "angleQuant";
		info->eventType = GF_SG_EVENT_FIELD;
		info->fieldType = GF_SG_VRML_SFBOOL;
		info->far_ptr = & ((M_QuantizationParameter *) node)->angleQuant;
		return GF_OK;
	case 22:
		info->name = "angleMin";
		info->eventType = GF_SG_EVENT_FIELD;
		info->fieldType = GF_SG_VRML_SFFLOAT;
		info->far_ptr = & ((M_QuantizationParameter *) node)->angleMin;
		return GF_OK;
	case 23:
		info->name = "angleMax";
		info->eventType = GF_SG_EVENT_FIELD;
		info->fieldType = GF_SG_VRML_SFFLOAT;
		info->far_ptr = & ((M_QuantizationParameter *) node)->angleMax;
		return GF_OK;
	case 24:
		info->name = "angleNbBits";
		info->eventType = GF_SG_EVENT_FIELD;
		info->fieldType = GF_SG_VRML_SFINT32;
		info->far_ptr = & ((M_QuantizationParameter *) node)->angleNbBits;
		return GF_OK;
	case 25:
		info->name = "scaleQuant";
		info->eventType = GF_SG_EVENT_FIELD;
		info->fieldType = GF_SG_VRML_SFBOOL;
		info->far_ptr = & ((M_QuantizationParameter *) node)->scaleQuant;
		return GF_OK;
	case 26:
		info->name = "scaleMin";
		info->eventType = GF_SG_EVENT_FIELD;
		info->fieldType = GF_SG_VRML_SFFLOAT;
		info->far_ptr = & ((M_QuantizationParameter *) node)->scaleMin;
		return GF_OK;
	case 27:
		info->name = "scaleMax";
		info->eventType = GF_SG_EVENT_FIELD;
		info->fieldType = GF_SG_VRML_SFFLOAT;
		info->far_ptr = & ((M_QuantizationParameter *) node)->scaleMax;
		return GF_OK;
	case 28:
		info->name = "scaleNbBits";
		info->eventType = GF_SG_EVENT_FIELD;
		info->fieldType = GF_SG_VRML_SFINT32;
		info->far_ptr = & ((M_QuantizationParameter *) node)->scaleNbBits;
		return GF_OK;
	case 29:
		info->name = "keyQuant";
		info->eventType = GF_SG_EVENT_FIELD;
		info->fieldType = GF_SG_VRML_SFBOOL;
		info->far_ptr = & ((M_QuantizationParameter *) node)->keyQuant;
		return GF_OK;
	case 30:
		info->name = "keyMin";
		info->eventType = GF_SG_EVENT_FIELD;
		info->fieldType = GF_SG_VRML_SFFLOAT;
		info->far_ptr = & ((M_QuantizationParameter *) node)->keyMin;
		return GF_OK;
	case 31:
		info->name = "keyMax";
		info->eventType = GF_SG_EVENT_FIELD;
		info->fieldType = GF_SG_VRML_SFFLOAT;
		info->far_ptr = & ((M_QuantizationParameter *) node)->keyMax;
		return GF_OK;
	case 32:
		info->name = "keyNbBits";
		info->eventType = GF_SG_EVENT_FIELD;
		info->fieldType = GF_SG_VRML_SFINT32;
		info->far_ptr = & ((M_QuantizationParameter *) node)->keyNbBits;
		return GF_OK;
	case 33:
		info->name = "normalQuant";
		info->eventType = GF_SG_EVENT_FIELD;
		info->fieldType = GF_SG_VRML_SFBOOL;
		info->far_ptr = & ((M_QuantizationParameter *) node)->normalQuant;
		return GF_OK;
	case 34:
		info->name = "normalNbBits";
		info->eventType = GF_SG_EVENT_FIELD;
		info->fieldType = GF_SG_VRML_SFINT32;
		info->far_ptr = & ((M_QuantizationParameter *) node)->normalNbBits;
		return GF_OK;
	case 35:
		info->name = "sizeQuant";
		info->eventType = GF_SG_EVENT_FIELD;
		info->fieldType = GF_SG_VRML_SFBOOL;
		info->far_ptr = & ((M_QuantizationParameter *) node)->sizeQuant;
		return GF_OK;
	case 36:
		info->name = "sizeMin";
		info->eventType = GF_SG_EVENT_FIELD;
		info->fieldType = GF_SG_VRML_SFFLOAT;
		info->far_ptr = & ((M_QuantizationParameter *) node)->sizeMin;
		return GF_OK;
	case 37:
		info->name = "sizeMax";
		info->eventType = GF_SG_EVENT_FIELD;
		info->fieldType = GF_SG_VRML_SFFLOAT;
		info->far_ptr = & ((M_QuantizationParameter *) node)->sizeMax;
		return GF_OK;
	case 38:
		info->name = "sizeNbBits";
		info->eventType = GF_SG_EVENT_FIELD;
		info->fieldType = GF_SG_VRML_SFINT32;
		info->far_ptr = & ((M_QuantizationParameter *) node)->sizeNbBits;
		return GF_OK;
	case 39:
		info->name = "useEfficientCoding";
		info->eventType = GF_SG_EVENT_FIELD;
		info->fieldType = GF_SG_VRML_SFBOOL;
		info->far_ptr = & ((M_QuantizationParameter *) node)->useEfficientCoding;
		return GF_OK;
	default:
		return GF_BAD_PARAM;
	}
}

static Bool QuantizationParameter_get_aq_info(GF_Node *n, u32 FieldIndex, u8 *QType, u8 *AType, Fixed *b_min, Fixed *b_max, u32 *QT13_bits)
{
	switch (FieldIndex) {
	case 2:
		*AType = 0;
		*QType = 0;
		*b_min = FIX_MIN;
		*b_max = FIX_MAX;
		return 1;
	case 3:
		*AType = 0;
		*QType = 0;
		*b_min = FIX_MIN;
		*b_max = FIX_MAX;
		return 1;
	case 4:
		*AType = 0;
		*QType = 13;
		*QT13_bits = 5;
		*b_min = FLT2FIX(0);
		*b_max = FLT2FIX(31);
		return 1;
	case 6:
		*AType = 0;
		*QType = 0;
		*b_min = FIX_MIN;
		*b_max = FIX_MAX;
		return 1;
	case 7:
		*AType = 0;
		*QType = 0;
		*b_min = FIX_MIN;
		*b_max = FIX_MAX;
		return 1;
	case 8:
		*AType = 0;
		*QType = 13;
		*QT13_bits = 5;
		*b_min = FLT2FIX(0);
		*b_max = FLT2FIX(31);
		return 1;
	case 10:
		*AType = 0;
		*QType = 0;
		*b_min = FIX_MIN;
		*b_max = FIX_MAX;
		return 1;
	case 11:
		*AType = 0;
		*QType = 0;
		*b_min = FIX_MIN;
		*b_max = FIX_MAX;
		return 1;
	case 12:
		*AType = 0;
		*QType = 13;
		*QT13_bits = 5;
		*b_min = FLT2FIX(0);
		*b_max = FLT2FIX(31);
		return 1;
	case 14:
		*AType = 0;
		*QType = 0;
		*b_min = FLT2FIX(0);
		*b_max = FLT2FIX(1);
		return 1;
	case 15:
		*AType = 0;
		*QType = 0;
		*b_min = FLT2FIX(0);
		*b_max = FLT2FIX(1);
		return 1;
	case 16:
		*AType = 0;
		*QType = 13;
		*QT13_bits = 5;
		*b_min = FLT2FIX(0);
		*b_max = FLT2FIX(31);
		return 1;
	case 18:
		*AType = 0;
		*QType = 0;
		*b_min = FLT2FIX(0);
		*b_max = FLT2FIX(1);
		return 1;
	case 19:
		*AType = 0;
		*QType = 0;
		*b_min = FLT2FIX(0);
		*b_max = FLT2FIX(1);
		return 1;
	case 20:
		*AType = 0;
		*QType = 13;
		*QT13_bits = 5;
		*b_min = FLT2FIX(0);
		*b_max = FLT2FIX(31);
		return 1;
	case 22:
		*AType = 0;
		*QType = 0;
		*b_min = FLT2FIX(0);
		*b_max = FLT2FIX(6.2831853);
		return 1;
	case 23:
		*AType = 0;
		*QType = 0;
		*b_min = FLT2FIX(0);
		*b_max = FLT2FIX(6.2831853);
		return 1;
	case 24:
		*AType = 0;
		*QType = 13;
		*QT13_bits = 5;
		*b_min = FLT2FIX(0);
		*b_max = FLT2FIX(31);
		return 1;
	case 26:
		*AType = 0;
		*QType = 0;
		*b_min = FIX_MIN;
		*b_max = FIX_MAX;
		return 1;
	case 27:
		*AType = 0;
		*QType = 0;
		*b_min = FIX_MIN;
		*b_max = FIX_MAX;
		return 1;
	case 28:
		*AType = 0;
		*QType = 13;
		*QT13_bits = 5;
		*b_min = FLT2FIX(0);
		*b_max = FLT2FIX(31);
		return 1;
	case 30:
		*AType = 0;
		*QType = 0;
		*b_min = FIX_MIN;
		*b_max = FIX_MAX;
		return 1;
	case 31:
		*AType = 0;
		*QType = 0;
		*b_min = FIX_MIN;
		*b_max = FIX_MAX;
		return 1;
	case 32:
		*AType = 0;
		*QType = 13;
		*QT13_bits = 5;
		*b_min = FLT2FIX(0);
		*b_max = FLT2FIX(31);
		return 1;
	case 34:
		*AType = 0;
		*QType = 13;
		*QT13_bits = 5;
		*b_min = FLT2FIX(0);
		*b_max = FLT2FIX(31);
		return 1;
	case 36:
		*AType = 0;
		*QType = 0;
		*b_min = FIX_MIN;
		*b_max = FIX_MAX;
		return 1;
	case 37:
		*AType = 0;
		*QType = 0;
		*b_min = FIX_MIN;
		*b_max = FIX_MAX;
		return 1;
	case 38:
		*AType = 0;
		*QType = 13;
		*QT13_bits = 5;
		*b_min = FLT2FIX(0);
		*b_max = FLT2FIX(31);
		return 1;
	default:
		return 0;
	}
}



GF_Node *QuantizationParameter_Create()
{
	M_QuantizationParameter *p;
	GF_SAFEALLOC(p, sizeof(M_QuantizationParameter));
	if(!p) return NULL;
	gf_node_setup((GF_Node *)p, TAG_MPEG4_QuantizationParameter);

#ifdef GF_NODE_USE_POINTERS
	((GF_Node *)p)->sgprivate->name = "QuantizationParameter";
	((GF_Node *)p)->sgprivate->node_del = QuantizationParameter_Del;
	((GF_Node *)p)->sgprivate->get_field_count = QuantizationParameter_get_field_count;
	((GF_Node *)p)->sgprivate->get_field = QuantizationParameter_get_field;
#endif


	/*default field values*/
	p->position3DMin.x = FIX_MIN;
	p->position3DMin.y = FIX_MIN;
	p->position3DMin.z = FIX_MIN;
	p->position3DMax.x = FIX_MAX;
	p->position3DMax.y = FIX_MAX;
	p->position3DMax.z = FIX_MAX;
	p->position3DNbBits = 16;
	p->position2DMin.x = FIX_MIN;
	p->position2DMin.y = FIX_MIN;
	p->position2DMax.x = FIX_MAX;
	p->position2DMax.y = FIX_MAX;
	p->position2DNbBits = 16;
	p->drawOrderMin = FIX_MIN;
	p->drawOrderMax = FIX_MAX;
	p->drawOrderNbBits = 8;
	p->colorQuant = 1;
	p->colorMin = FLT2FIX(0.0);
	p->colorMax = FLT2FIX(1.0);
	p->colorNbBits = 8;
	p->textureCoordinateQuant = 1;
	p->textureCoordinateMin = FLT2FIX(0);
	p->textureCoordinateMax = FLT2FIX(1);
	p->textureCoordinateNbBits = 16;
	p->angleQuant = 1;
	p->angleMin = FLT2FIX(0.0);
	p->angleMax = FLT2FIX(6.2831853);
	p->angleNbBits = 16;
	p->scaleMin = FLT2FIX(0.0);
	p->scaleMax = FIX_MAX;
	p->scaleNbBits = 8;
	p->keyQuant = 1;
	p->keyMin = FLT2FIX(0.0);
	p->keyMax = FLT2FIX(1.0);
	p->keyNbBits = 8;
	p->normalQuant = 1;
	p->normalNbBits = 8;
	p->sizeMin = FLT2FIX(0);
	p->sizeMax = FIX_MAX;
	p->sizeNbBits = 8;
	return (GF_Node *)p;
}


/*
	Rectangle Node deletion
*/

static void Rectangle_Del(GF_Node *node)
{
	M_Rectangle *p = (M_Rectangle *) node;
	gf_node_free((GF_Node *) p);
}

static const u16 Rectangle_Def2All[] = { 0};
static const u16 Rectangle_In2All[] = { 0};
static const u16 Rectangle_Out2All[] = { 0};
static const u16 Rectangle_Dyn2All[] = { 0};

static u32 Rectangle_get_field_count(GF_Node *node, u8 IndexMode)
{
	switch(IndexMode) {
	case GF_SG_FIELD_CODING_IN: return 1;
	case GF_SG_FIELD_CODING_DEF: return 1;
	case GF_SG_FIELD_CODING_OUT: return 1;
	case GF_SG_FIELD_CODING_DYN: return 1;
	default:
		return 1;
	}
}

static GF_Err Rectangle_get_field_index(GF_Node *n, u32 inField, u8 IndexMode, u32 *allField)
{
	switch(IndexMode) {
	case GF_SG_FIELD_CODING_IN:
		*allField = Rectangle_In2All[inField];
		return GF_OK;
	case GF_SG_FIELD_CODING_DEF:
		*allField = Rectangle_Def2All[inField];
		return GF_OK;
	case GF_SG_FIELD_CODING_OUT:
		*allField = Rectangle_Out2All[inField];
		return GF_OK;
	case GF_SG_FIELD_CODING_DYN:
		*allField = Rectangle_Dyn2All[inField];
		return GF_OK;
	default:
		return GF_BAD_PARAM;
	}
}
static GF_Err Rectangle_get_field(GF_Node *node, GF_FieldInfo *info)
{
	switch (info->fieldIndex) {
	case 0:
		info->name = "size";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFVEC2F;
		info->far_ptr = & ((M_Rectangle *) node)->size;
		return GF_OK;
	default:
		return GF_BAD_PARAM;
	}
}

static Bool Rectangle_get_aq_info(GF_Node *n, u32 FieldIndex, u8 *QType, u8 *AType, Fixed *b_min, Fixed *b_max, u32 *QT13_bits)
{
	switch (FieldIndex) {
	case 0:
		*AType = 2;
		*QType = 12;
		*b_min = FLT2FIX(0);
		*b_max = FIX_MAX;
		return 1;
	default:
		return 0;
	}
}



GF_Node *Rectangle_Create()
{
	M_Rectangle *p;
	GF_SAFEALLOC(p, sizeof(M_Rectangle));
	if(!p) return NULL;
	gf_node_setup((GF_Node *)p, TAG_MPEG4_Rectangle);

#ifdef GF_NODE_USE_POINTERS
	((GF_Node *)p)->sgprivate->name = "Rectangle";
	((GF_Node *)p)->sgprivate->node_del = Rectangle_Del;
	((GF_Node *)p)->sgprivate->get_field_count = Rectangle_get_field_count;
	((GF_Node *)p)->sgprivate->get_field = Rectangle_get_field;
#endif


	/*default field values*/
	p->size.x = FLT2FIX(2);
	p->size.y = FLT2FIX(2);
	return (GF_Node *)p;
}


/*
	ScalarInterpolator Node deletion
*/

static void ScalarInterpolator_Del(GF_Node *node)
{
	M_ScalarInterpolator *p = (M_ScalarInterpolator *) node;
	gf_sg_mffloat_del(p->key);
	gf_sg_mffloat_del(p->keyValue);
	gf_node_free((GF_Node *) p);
}

static const u16 ScalarInterpolator_Def2All[] = { 1, 2};
static const u16 ScalarInterpolator_In2All[] = { 0, 1, 2};
static const u16 ScalarInterpolator_Out2All[] = { 1, 2, 3};

static u32 ScalarInterpolator_get_field_count(GF_Node *node, u8 IndexMode)
{
	switch(IndexMode) {
	case GF_SG_FIELD_CODING_IN: return 3;
	case GF_SG_FIELD_CODING_DEF: return 2;
	case GF_SG_FIELD_CODING_OUT: return 3;
	case GF_SG_FIELD_CODING_DYN: return 0;
	default:
		return 4;
	}
}

static GF_Err ScalarInterpolator_get_field_index(GF_Node *n, u32 inField, u8 IndexMode, u32 *allField)
{
	switch(IndexMode) {
	case GF_SG_FIELD_CODING_IN:
		*allField = ScalarInterpolator_In2All[inField];
		return GF_OK;
	case GF_SG_FIELD_CODING_DEF:
		*allField = ScalarInterpolator_Def2All[inField];
		return GF_OK;
	case GF_SG_FIELD_CODING_OUT:
		*allField = ScalarInterpolator_Out2All[inField];
		return GF_OK;
	default:
		return GF_BAD_PARAM;
	}
}
static GF_Err ScalarInterpolator_get_field(GF_Node *node, GF_FieldInfo *info)
{
	switch (info->fieldIndex) {
	case 0:
		info->name = "set_fraction";
		info->eventType = GF_SG_EVENT_IN;
		info->on_event_in = ((M_ScalarInterpolator *)node)->on_set_fraction;
		info->fieldType = GF_SG_VRML_SFFLOAT;
		info->far_ptr = & ((M_ScalarInterpolator *) node)->set_fraction;
		return GF_OK;
	case 1:
		info->name = "key";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_MFFLOAT;
		info->far_ptr = & ((M_ScalarInterpolator *) node)->key;
		return GF_OK;
	case 2:
		info->name = "keyValue";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_MFFLOAT;
		info->far_ptr = & ((M_ScalarInterpolator *) node)->keyValue;
		return GF_OK;
	case 3:
		info->name = "value_changed";
		info->eventType = GF_SG_EVENT_OUT;
		info->fieldType = GF_SG_VRML_SFFLOAT;
		info->far_ptr = & ((M_ScalarInterpolator *) node)->value_changed;
		return GF_OK;
	default:
		return GF_BAD_PARAM;
	}
}

static Bool ScalarInterpolator_get_aq_info(GF_Node *n, u32 FieldIndex, u8 *QType, u8 *AType, Fixed *b_min, Fixed *b_max, u32 *QT13_bits)
{
	switch (FieldIndex) {
	case 1:
		*AType = 0;
		*QType = 8;
		*b_min = FLT2FIX(0);
		*b_max = FLT2FIX(1);
		return 1;
	case 2:
		*AType = 0;
		*QType = 0;
		*b_min = FIX_MIN;
		*b_max = FIX_MAX;
		return 1;
	default:
		return 0;
	}
}



GF_Node *ScalarInterpolator_Create()
{
	M_ScalarInterpolator *p;
	GF_SAFEALLOC(p, sizeof(M_ScalarInterpolator));
	if(!p) return NULL;
	gf_node_setup((GF_Node *)p, TAG_MPEG4_ScalarInterpolator);

#ifdef GF_NODE_USE_POINTERS
	((GF_Node *)p)->sgprivate->name = "ScalarInterpolator";
	((GF_Node *)p)->sgprivate->node_del = ScalarInterpolator_Del;
	((GF_Node *)p)->sgprivate->get_field_count = ScalarInterpolator_get_field_count;
	((GF_Node *)p)->sgprivate->get_field = ScalarInterpolator_get_field;
#endif


	/*default field values*/
	return (GF_Node *)p;
}


/*
	Script Node deletion
*/

static void Script_Del(GF_Node *node)
{
	M_Script *p = (M_Script *) node;
	gf_sg_mfscript_del(p->url);
	gf_node_free((GF_Node *) p);
}

static const u16 Script_Def2All[] = { 0, 1, 2};
static const u16 Script_In2All[] = { 0};
static const u16 Script_Out2All[] = { 0};

static u32 Script_get_field_count(GF_Node *node, u8 IndexMode)
{
	switch(IndexMode) {
	case GF_SG_FIELD_CODING_IN: return 1;
	case GF_SG_FIELD_CODING_DEF: return 3;
	case GF_SG_FIELD_CODING_OUT: return 1;
	case GF_SG_FIELD_CODING_DYN: return 0;
	default:
		return 3;
	}
}

static GF_Err Script_get_field_index(GF_Node *n, u32 inField, u8 IndexMode, u32 *allField)
{
	switch(IndexMode) {
	case GF_SG_FIELD_CODING_IN:
		*allField = Script_In2All[inField];
		return GF_OK;
	case GF_SG_FIELD_CODING_DEF:
		*allField = Script_Def2All[inField];
		return GF_OK;
	case GF_SG_FIELD_CODING_OUT:
		*allField = Script_Out2All[inField];
		return GF_OK;
	default:
		return GF_BAD_PARAM;
	}
}
static GF_Err Script_get_field(GF_Node *node, GF_FieldInfo *info)
{
	switch (info->fieldIndex) {
	case 0:
		info->name = "url";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_MFSCRIPT;
		info->far_ptr = & ((M_Script *) node)->url;
		return GF_OK;
	case 1:
		info->name = "directOutput";
		info->eventType = GF_SG_EVENT_FIELD;
		info->fieldType = GF_SG_VRML_SFBOOL;
		info->far_ptr = & ((M_Script *) node)->directOutput;
		return GF_OK;
	case 2:
		info->name = "mustEvaluate";
		info->eventType = GF_SG_EVENT_FIELD;
		info->fieldType = GF_SG_VRML_SFBOOL;
		info->far_ptr = & ((M_Script *) node)->mustEvaluate;
		return GF_OK;
	default:
		return GF_BAD_PARAM;
	}
}

static Bool Script_get_aq_info(GF_Node *n, u32 FieldIndex, u8 *QType, u8 *AType, Fixed *b_min, Fixed *b_max, u32 *QT13_bits)
{
	switch (FieldIndex) {
	default:
		return 0;
	}
}



GF_Node *Script_Create()
{
	M_Script *p;
	GF_SAFEALLOC(p, sizeof(M_Script));
	if(!p) return NULL;
	gf_node_setup((GF_Node *)p, TAG_MPEG4_Script);

#ifdef GF_NODE_USE_POINTERS
	((GF_Node *)p)->sgprivate->name = "Script";
	((GF_Node *)p)->sgprivate->node_del = Script_Del;
	((GF_Node *)p)->sgprivate->get_field_count = Script_get_field_count;
	((GF_Node *)p)->sgprivate->get_field = Script_get_field;
#endif


	/*default field values*/
	return (GF_Node *)p;
}


/*
	Shape Node deletion
*/

static void Shape_Del(GF_Node *node)
{
	M_Shape *p = (M_Shape *) node;
	gf_node_unregister((GF_Node *) p->appearance, (GF_Node *) p);	
	gf_node_unregister((GF_Node *) p->geometry, (GF_Node *) p);	
	gf_node_free((GF_Node *) p);
}

static const u16 Shape_Def2All[] = { 0, 1};
static const u16 Shape_In2All[] = { 0, 1};
static const u16 Shape_Out2All[] = { 0, 1};

static u32 Shape_get_field_count(GF_Node *node, u8 IndexMode)
{
	switch(IndexMode) {
	case GF_SG_FIELD_CODING_IN: return 2;
	case GF_SG_FIELD_CODING_DEF: return 2;
	case GF_SG_FIELD_CODING_OUT: return 2;
	case GF_SG_FIELD_CODING_DYN: return 0;
	default:
		return 2;
	}
}

static GF_Err Shape_get_field_index(GF_Node *n, u32 inField, u8 IndexMode, u32 *allField)
{
	switch(IndexMode) {
	case GF_SG_FIELD_CODING_IN:
		*allField = Shape_In2All[inField];
		return GF_OK;
	case GF_SG_FIELD_CODING_DEF:
		*allField = Shape_Def2All[inField];
		return GF_OK;
	case GF_SG_FIELD_CODING_OUT:
		*allField = Shape_Out2All[inField];
		return GF_OK;
	default:
		return GF_BAD_PARAM;
	}
}
static GF_Err Shape_get_field(GF_Node *node, GF_FieldInfo *info)
{
	switch (info->fieldIndex) {
	case 0:
		info->name = "appearance";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFNODE;
		info->NDTtype = NDT_SFAppearanceNode;
		info->far_ptr = & ((M_Shape *)node)->appearance;
		return GF_OK;
	case 1:
		info->name = "geometry";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFNODE;
		info->NDTtype = NDT_SFGeometryNode;
		info->far_ptr = & ((M_Shape *)node)->geometry;
		return GF_OK;
	default:
		return GF_BAD_PARAM;
	}
}

static Bool Shape_get_aq_info(GF_Node *n, u32 FieldIndex, u8 *QType, u8 *AType, Fixed *b_min, Fixed *b_max, u32 *QT13_bits)
{
	switch (FieldIndex) {
	default:
		return 0;
	}
}



GF_Node *Shape_Create()
{
	M_Shape *p;
	GF_SAFEALLOC(p, sizeof(M_Shape));
	if(!p) return NULL;
	gf_node_setup((GF_Node *)p, TAG_MPEG4_Shape);

#ifdef GF_NODE_USE_POINTERS
	((GF_Node *)p)->sgprivate->name = "Shape";
	((GF_Node *)p)->sgprivate->node_del = Shape_Del;
	((GF_Node *)p)->sgprivate->get_field_count = Shape_get_field_count;
	((GF_Node *)p)->sgprivate->get_field = Shape_get_field;
#endif


	/*default field values*/
	return (GF_Node *)p;
}


/*
	Sound Node deletion
*/

static void Sound_Del(GF_Node *node)
{
	M_Sound *p = (M_Sound *) node;
	gf_node_unregister((GF_Node *) p->source, (GF_Node *) p);	
	gf_node_free((GF_Node *) p);
}

static const u16 Sound_Def2All[] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9};
static const u16 Sound_In2All[] = { 0, 1, 2, 3, 4, 5, 6, 7, 8};
static const u16 Sound_Out2All[] = { 0, 1, 2, 3, 4, 5, 6, 7, 8};
static const u16 Sound_Dyn2All[] = { 1, 2, 3, 4, 5, 6};

static u32 Sound_get_field_count(GF_Node *node, u8 IndexMode)
{
	switch(IndexMode) {
	case GF_SG_FIELD_CODING_IN: return 9;
	case GF_SG_FIELD_CODING_DEF: return 10;
	case GF_SG_FIELD_CODING_OUT: return 9;
	case GF_SG_FIELD_CODING_DYN: return 6;
	default:
		return 10;
	}
}

static GF_Err Sound_get_field_index(GF_Node *n, u32 inField, u8 IndexMode, u32 *allField)
{
	switch(IndexMode) {
	case GF_SG_FIELD_CODING_IN:
		*allField = Sound_In2All[inField];
		return GF_OK;
	case GF_SG_FIELD_CODING_DEF:
		*allField = Sound_Def2All[inField];
		return GF_OK;
	case GF_SG_FIELD_CODING_OUT:
		*allField = Sound_Out2All[inField];
		return GF_OK;
	case GF_SG_FIELD_CODING_DYN:
		*allField = Sound_Dyn2All[inField];
		return GF_OK;
	default:
		return GF_BAD_PARAM;
	}
}
static GF_Err Sound_get_field(GF_Node *node, GF_FieldInfo *info)
{
	switch (info->fieldIndex) {
	case 0:
		info->name = "direction";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFVEC3F;
		info->far_ptr = & ((M_Sound *) node)->direction;
		return GF_OK;
	case 1:
		info->name = "intensity";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFFLOAT;
		info->far_ptr = & ((M_Sound *) node)->intensity;
		return GF_OK;
	case 2:
		info->name = "location";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFVEC3F;
		info->far_ptr = & ((M_Sound *) node)->location;
		return GF_OK;
	case 3:
		info->name = "maxBack";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFFLOAT;
		info->far_ptr = & ((M_Sound *) node)->maxBack;
		return GF_OK;
	case 4:
		info->name = "maxFront";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFFLOAT;
		info->far_ptr = & ((M_Sound *) node)->maxFront;
		return GF_OK;
	case 5:
		info->name = "minBack";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFFLOAT;
		info->far_ptr = & ((M_Sound *) node)->minBack;
		return GF_OK;
	case 6:
		info->name = "minFront";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFFLOAT;
		info->far_ptr = & ((M_Sound *) node)->minFront;
		return GF_OK;
	case 7:
		info->name = "priority";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFFLOAT;
		info->far_ptr = & ((M_Sound *) node)->priority;
		return GF_OK;
	case 8:
		info->name = "source";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFNODE;
		info->NDTtype = NDT_SFAudioNode;
		info->far_ptr = & ((M_Sound *)node)->source;
		return GF_OK;
	case 9:
		info->name = "spatialize";
		info->eventType = GF_SG_EVENT_FIELD;
		info->fieldType = GF_SG_VRML_SFBOOL;
		info->far_ptr = & ((M_Sound *) node)->spatialize;
		return GF_OK;
	default:
		return GF_BAD_PARAM;
	}
}

static Bool Sound_get_aq_info(GF_Node *n, u32 FieldIndex, u8 *QType, u8 *AType, Fixed *b_min, Fixed *b_max, u32 *QT13_bits)
{
	switch (FieldIndex) {
	case 0:
		*AType = 0;
		*QType = 9;
		*b_min = FIX_MIN;
		*b_max = FIX_MAX;
		return 1;
	case 1:
		*AType = 7;
		*QType = 4;
		*b_min = FLT2FIX(0);
		*b_max = FLT2FIX(1);
		return 1;
	case 2:
		*AType = 1;
		*QType = 1;
		*b_min = FIX_MIN;
		*b_max = FIX_MAX;
		return 1;
	case 3:
		*AType = 7;
		*QType = 11;
		*b_min = FLT2FIX(0);
		*b_max = FIX_MAX;
		return 1;
	case 4:
		*AType = 7;
		*QType = 11;
		*b_min = FLT2FIX(0);
		*b_max = FIX_MAX;
		return 1;
	case 5:
		*AType = 7;
		*QType = 11;
		*b_min = FLT2FIX(0);
		*b_max = FIX_MAX;
		return 1;
	case 6:
		*AType = 7;
		*QType = 11;
		*b_min = FLT2FIX(0);
		*b_max = FIX_MAX;
		return 1;
	case 7:
		*AType = 0;
		*QType = 4;
		*b_min = FLT2FIX(0);
		*b_max = FLT2FIX(1);
		return 1;
	default:
		return 0;
	}
}



GF_Node *Sound_Create()
{
	M_Sound *p;
	GF_SAFEALLOC(p, sizeof(M_Sound));
	if(!p) return NULL;
	gf_node_setup((GF_Node *)p, TAG_MPEG4_Sound);

#ifdef GF_NODE_USE_POINTERS
	((GF_Node *)p)->sgprivate->name = "Sound";
	((GF_Node *)p)->sgprivate->node_del = Sound_Del;
	((GF_Node *)p)->sgprivate->get_field_count = Sound_get_field_count;
	((GF_Node *)p)->sgprivate->get_field = Sound_get_field;
#endif


	/*default field values*/
	p->direction.x = FLT2FIX(0);
	p->direction.y = FLT2FIX(0);
	p->direction.z = FLT2FIX(1);
	p->intensity = FLT2FIX(1);
	p->location.x = FLT2FIX(0);
	p->location.y = FLT2FIX(0);
	p->location.z = FLT2FIX(0);
	p->maxBack = FLT2FIX(10);
	p->maxFront = FLT2FIX(10);
	p->minBack = FLT2FIX(1);
	p->minFront = FLT2FIX(1);
	p->priority = FLT2FIX(0);
	p->spatialize = 1;
	return (GF_Node *)p;
}


/*
	Sound2D Node deletion
*/

static void Sound2D_Del(GF_Node *node)
{
	M_Sound2D *p = (M_Sound2D *) node;
	gf_node_unregister((GF_Node *) p->source, (GF_Node *) p);	
	gf_node_free((GF_Node *) p);
}

static const u16 Sound2D_Def2All[] = { 0, 1, 2, 3};
static const u16 Sound2D_In2All[] = { 0, 1, 2};
static const u16 Sound2D_Out2All[] = { 0, 1, 2};
static const u16 Sound2D_Dyn2All[] = { 0, 1};

static u32 Sound2D_get_field_count(GF_Node *node, u8 IndexMode)
{
	switch(IndexMode) {
	case GF_SG_FIELD_CODING_IN: return 3;
	case GF_SG_FIELD_CODING_DEF: return 4;
	case GF_SG_FIELD_CODING_OUT: return 3;
	case GF_SG_FIELD_CODING_DYN: return 2;
	default:
		return 4;
	}
}

static GF_Err Sound2D_get_field_index(GF_Node *n, u32 inField, u8 IndexMode, u32 *allField)
{
	switch(IndexMode) {
	case GF_SG_FIELD_CODING_IN:
		*allField = Sound2D_In2All[inField];
		return GF_OK;
	case GF_SG_FIELD_CODING_DEF:
		*allField = Sound2D_Def2All[inField];
		return GF_OK;
	case GF_SG_FIELD_CODING_OUT:
		*allField = Sound2D_Out2All[inField];
		return GF_OK;
	case GF_SG_FIELD_CODING_DYN:
		*allField = Sound2D_Dyn2All[inField];
		return GF_OK;
	default:
		return GF_BAD_PARAM;
	}
}
static GF_Err Sound2D_get_field(GF_Node *node, GF_FieldInfo *info)
{
	switch (info->fieldIndex) {
	case 0:
		info->name = "intensity";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFFLOAT;
		info->far_ptr = & ((M_Sound2D *) node)->intensity;
		return GF_OK;
	case 1:
		info->name = "location";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFVEC2F;
		info->far_ptr = & ((M_Sound2D *) node)->location;
		return GF_OK;
	case 2:
		info->name = "source";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFNODE;
		info->NDTtype = NDT_SFAudioNode;
		info->far_ptr = & ((M_Sound2D *)node)->source;
		return GF_OK;
	case 3:
		info->name = "spatialize";
		info->eventType = GF_SG_EVENT_FIELD;
		info->fieldType = GF_SG_VRML_SFBOOL;
		info->far_ptr = & ((M_Sound2D *) node)->spatialize;
		return GF_OK;
	default:
		return GF_BAD_PARAM;
	}
}

static Bool Sound2D_get_aq_info(GF_Node *n, u32 FieldIndex, u8 *QType, u8 *AType, Fixed *b_min, Fixed *b_max, u32 *QT13_bits)
{
	switch (FieldIndex) {
	case 0:
		*AType = 7;
		*QType = 4;
		*b_min = FLT2FIX(0);
		*b_max = FLT2FIX(1);
		return 1;
	case 1:
		*AType = 2;
		*QType = 2;
		*b_min = FIX_MIN;
		*b_max = FIX_MAX;
		return 1;
	default:
		return 0;
	}
}



GF_Node *Sound2D_Create()
{
	M_Sound2D *p;
	GF_SAFEALLOC(p, sizeof(M_Sound2D));
	if(!p) return NULL;
	gf_node_setup((GF_Node *)p, TAG_MPEG4_Sound2D);

#ifdef GF_NODE_USE_POINTERS
	((GF_Node *)p)->sgprivate->name = "Sound2D";
	((GF_Node *)p)->sgprivate->node_del = Sound2D_Del;
	((GF_Node *)p)->sgprivate->get_field_count = Sound2D_get_field_count;
	((GF_Node *)p)->sgprivate->get_field = Sound2D_get_field;
#endif


	/*default field values*/
	p->intensity = FLT2FIX(1);
	p->location.x = FLT2FIX(0);
	p->location.y = FLT2FIX(0);
	p->spatialize = 1;
	return (GF_Node *)p;
}


/*
	Sphere Node deletion
*/

static void Sphere_Del(GF_Node *node)
{
	M_Sphere *p = (M_Sphere *) node;
	gf_node_free((GF_Node *) p);
}

static const u16 Sphere_Def2All[] = { 0};

static u32 Sphere_get_field_count(GF_Node *node, u8 IndexMode)
{
	switch(IndexMode) {
	case GF_SG_FIELD_CODING_IN: return 0;
	case GF_SG_FIELD_CODING_DEF: return 1;
	case GF_SG_FIELD_CODING_OUT: return 0;
	case GF_SG_FIELD_CODING_DYN: return 0;
	default:
		return 1;
	}
}

static GF_Err Sphere_get_field_index(GF_Node *n, u32 inField, u8 IndexMode, u32 *allField)
{
	switch(IndexMode) {
	case GF_SG_FIELD_CODING_DEF:
		*allField = Sphere_Def2All[inField];
		return GF_OK;
	default:
		return GF_BAD_PARAM;
	}
}
static GF_Err Sphere_get_field(GF_Node *node, GF_FieldInfo *info)
{
	switch (info->fieldIndex) {
	case 0:
		info->name = "radius";
		info->eventType = GF_SG_EVENT_FIELD;
		info->fieldType = GF_SG_VRML_SFFLOAT;
		info->far_ptr = & ((M_Sphere *) node)->radius;
		return GF_OK;
	default:
		return GF_BAD_PARAM;
	}
}

static Bool Sphere_get_aq_info(GF_Node *n, u32 FieldIndex, u8 *QType, u8 *AType, Fixed *b_min, Fixed *b_max, u32 *QT13_bits)
{
	switch (FieldIndex) {
	case 0:
		*AType = 0;
		*QType = 11;
		*b_min = FLT2FIX(0);
		*b_max = FIX_MAX;
		return 1;
	default:
		return 0;
	}
}



GF_Node *Sphere_Create()
{
	M_Sphere *p;
	GF_SAFEALLOC(p, sizeof(M_Sphere));
	if(!p) return NULL;
	gf_node_setup((GF_Node *)p, TAG_MPEG4_Sphere);

#ifdef GF_NODE_USE_POINTERS
	((GF_Node *)p)->sgprivate->name = "Sphere";
	((GF_Node *)p)->sgprivate->node_del = Sphere_Del;
	((GF_Node *)p)->sgprivate->get_field_count = Sphere_get_field_count;
	((GF_Node *)p)->sgprivate->get_field = Sphere_get_field;
#endif


	/*default field values*/
	p->radius = FLT2FIX(1);
	return (GF_Node *)p;
}


/*
	SphereSensor Node deletion
*/

static void SphereSensor_Del(GF_Node *node)
{
	M_SphereSensor *p = (M_SphereSensor *) node;
	gf_node_free((GF_Node *) p);
}

static const u16 SphereSensor_Def2All[] = { 0, 1, 2};
static const u16 SphereSensor_In2All[] = { 0, 1, 2};
static const u16 SphereSensor_Out2All[] = { 0, 1, 2, 3, 4, 5};

static u32 SphereSensor_get_field_count(GF_Node *node, u8 IndexMode)
{
	switch(IndexMode) {
	case GF_SG_FIELD_CODING_IN: return 3;
	case GF_SG_FIELD_CODING_DEF: return 3;
	case GF_SG_FIELD_CODING_OUT: return 6;
	case GF_SG_FIELD_CODING_DYN: return 0;
	default:
		return 6;
	}
}

static GF_Err SphereSensor_get_field_index(GF_Node *n, u32 inField, u8 IndexMode, u32 *allField)
{
	switch(IndexMode) {
	case GF_SG_FIELD_CODING_IN:
		*allField = SphereSensor_In2All[inField];
		return GF_OK;
	case GF_SG_FIELD_CODING_DEF:
		*allField = SphereSensor_Def2All[inField];
		return GF_OK;
	case GF_SG_FIELD_CODING_OUT:
		*allField = SphereSensor_Out2All[inField];
		return GF_OK;
	default:
		return GF_BAD_PARAM;
	}
}
static GF_Err SphereSensor_get_field(GF_Node *node, GF_FieldInfo *info)
{
	switch (info->fieldIndex) {
	case 0:
		info->name = "autoOffset";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFBOOL;
		info->far_ptr = & ((M_SphereSensor *) node)->autoOffset;
		return GF_OK;
	case 1:
		info->name = "enabled";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFBOOL;
		info->far_ptr = & ((M_SphereSensor *) node)->enabled;
		return GF_OK;
	case 2:
		info->name = "offset";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFROTATION;
		info->far_ptr = & ((M_SphereSensor *) node)->offset;
		return GF_OK;
	case 3:
		info->name = "isActive";
		info->eventType = GF_SG_EVENT_OUT;
		info->fieldType = GF_SG_VRML_SFBOOL;
		info->far_ptr = & ((M_SphereSensor *) node)->isActive;
		return GF_OK;
	case 4:
		info->name = "rotation_changed";
		info->eventType = GF_SG_EVENT_OUT;
		info->fieldType = GF_SG_VRML_SFROTATION;
		info->far_ptr = & ((M_SphereSensor *) node)->rotation_changed;
		return GF_OK;
	case 5:
		info->name = "trackPoint_changed";
		info->eventType = GF_SG_EVENT_OUT;
		info->fieldType = GF_SG_VRML_SFVEC3F;
		info->far_ptr = & ((M_SphereSensor *) node)->trackPoint_changed;
		return GF_OK;
	default:
		return GF_BAD_PARAM;
	}
}

static Bool SphereSensor_get_aq_info(GF_Node *n, u32 FieldIndex, u8 *QType, u8 *AType, Fixed *b_min, Fixed *b_max, u32 *QT13_bits)
{
	switch (FieldIndex) {
	case 2:
		*AType = 0;
		*QType = 10;
		*b_min = FIX_MIN;
		*b_max = FIX_MAX;
		return 1;
	default:
		return 0;
	}
}



GF_Node *SphereSensor_Create()
{
	M_SphereSensor *p;
	GF_SAFEALLOC(p, sizeof(M_SphereSensor));
	if(!p) return NULL;
	gf_node_setup((GF_Node *)p, TAG_MPEG4_SphereSensor);

#ifdef GF_NODE_USE_POINTERS
	((GF_Node *)p)->sgprivate->name = "SphereSensor";
	((GF_Node *)p)->sgprivate->node_del = SphereSensor_Del;
	((GF_Node *)p)->sgprivate->get_field_count = SphereSensor_get_field_count;
	((GF_Node *)p)->sgprivate->get_field = SphereSensor_get_field;
#endif


	/*default field values*/
	p->autoOffset = 1;
	p->enabled = 1;
	p->offset.x = FLT2FIX(0);
	p->offset.y = FLT2FIX(1);
	p->offset.z = FLT2FIX(0);
	p->offset.q = FLT2FIX(0);
	return (GF_Node *)p;
}


/*
	SpotLight Node deletion
*/

static void SpotLight_Del(GF_Node *node)
{
	M_SpotLight *p = (M_SpotLight *) node;
	gf_node_free((GF_Node *) p);
}

static const u16 SpotLight_Def2All[] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9};
static const u16 SpotLight_In2All[] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9};
static const u16 SpotLight_Out2All[] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9};
static const u16 SpotLight_Dyn2All[] = { 0, 1, 2, 3, 4, 5, 6, 7, 9};

static u32 SpotLight_get_field_count(GF_Node *node, u8 IndexMode)
{
	switch(IndexMode) {
	case GF_SG_FIELD_CODING_IN: return 10;
	case GF_SG_FIELD_CODING_DEF: return 10;
	case GF_SG_FIELD_CODING_OUT: return 10;
	case GF_SG_FIELD_CODING_DYN: return 9;
	default:
		return 10;
	}
}

static GF_Err SpotLight_get_field_index(GF_Node *n, u32 inField, u8 IndexMode, u32 *allField)
{
	switch(IndexMode) {
	case GF_SG_FIELD_CODING_IN:
		*allField = SpotLight_In2All[inField];
		return GF_OK;
	case GF_SG_FIELD_CODING_DEF:
		*allField = SpotLight_Def2All[inField];
		return GF_OK;
	case GF_SG_FIELD_CODING_OUT:
		*allField = SpotLight_Out2All[inField];
		return GF_OK;
	case GF_SG_FIELD_CODING_DYN:
		*allField = SpotLight_Dyn2All[inField];
		return GF_OK;
	default:
		return GF_BAD_PARAM;
	}
}
static GF_Err SpotLight_get_field(GF_Node *node, GF_FieldInfo *info)
{
	switch (info->fieldIndex) {
	case 0:
		info->name = "ambientIntensity";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFFLOAT;
		info->far_ptr = & ((M_SpotLight *) node)->ambientIntensity;
		return GF_OK;
	case 1:
		info->name = "attenuation";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFVEC3F;
		info->far_ptr = & ((M_SpotLight *) node)->attenuation;
		return GF_OK;
	case 2:
		info->name = "beamWidth";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFFLOAT;
		info->far_ptr = & ((M_SpotLight *) node)->beamWidth;
		return GF_OK;
	case 3:
		info->name = "color";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFCOLOR;
		info->far_ptr = & ((M_SpotLight *) node)->color;
		return GF_OK;
	case 4:
		info->name = "cutOffAngle";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFFLOAT;
		info->far_ptr = & ((M_SpotLight *) node)->cutOffAngle;
		return GF_OK;
	case 5:
		info->name = "direction";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFVEC3F;
		info->far_ptr = & ((M_SpotLight *) node)->direction;
		return GF_OK;
	case 6:
		info->name = "intensity";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFFLOAT;
		info->far_ptr = & ((M_SpotLight *) node)->intensity;
		return GF_OK;
	case 7:
		info->name = "location";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFVEC3F;
		info->far_ptr = & ((M_SpotLight *) node)->location;
		return GF_OK;
	case 8:
		info->name = "on";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFBOOL;
		info->far_ptr = & ((M_SpotLight *) node)->on;
		return GF_OK;
	case 9:
		info->name = "radius";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFFLOAT;
		info->far_ptr = & ((M_SpotLight *) node)->radius;
		return GF_OK;
	default:
		return GF_BAD_PARAM;
	}
}

static Bool SpotLight_get_aq_info(GF_Node *n, u32 FieldIndex, u8 *QType, u8 *AType, Fixed *b_min, Fixed *b_max, u32 *QT13_bits)
{
	switch (FieldIndex) {
	case 0:
		*AType = 8;
		*QType = 4;
		*b_min = FLT2FIX(0);
		*b_max = FLT2FIX(1);
		return 1;
	case 1:
		*AType = 1;
		*QType = 11;
		*b_min = FLT2FIX(0);
		*b_max = FIX_MAX;
		return 1;
	case 2:
		*AType = 8;
		*QType = 6;
		*b_min = FLT2FIX(0);
		*b_max = FLT2FIX(1.5707963);
		return 1;
	case 3:
		*AType = 4;
		*QType = 4;
		*b_min = FLT2FIX(0);
		*b_max = FLT2FIX(1);
		return 1;
	case 4:
		*AType = 8;
		*QType = 6;
		*b_min = FLT2FIX(0);
		*b_max = FLT2FIX(1.5707963);
		return 1;
	case 5:
		*AType = 9;
		*QType = 9;
		*b_min = FIX_MIN;
		*b_max = FIX_MAX;
		return 1;
	case 6:
		*AType = 8;
		*QType = 4;
		*b_min = FLT2FIX(0);
		*b_max = FLT2FIX(1);
		return 1;
	case 7:
		*AType = 1;
		*QType = 1;
		*b_min = FIX_MIN;
		*b_max = FIX_MAX;
		return 1;
	case 9:
		*AType = 7;
		*QType = 11;
		*b_min = FLT2FIX(0);
		*b_max = FIX_MAX;
		return 1;
	default:
		return 0;
	}
}



GF_Node *SpotLight_Create()
{
	M_SpotLight *p;
	GF_SAFEALLOC(p, sizeof(M_SpotLight));
	if(!p) return NULL;
	gf_node_setup((GF_Node *)p, TAG_MPEG4_SpotLight);

#ifdef GF_NODE_USE_POINTERS
	((GF_Node *)p)->sgprivate->name = "SpotLight";
	((GF_Node *)p)->sgprivate->node_del = SpotLight_Del;
	((GF_Node *)p)->sgprivate->get_field_count = SpotLight_get_field_count;
	((GF_Node *)p)->sgprivate->get_field = SpotLight_get_field;
#endif


	/*default field values*/
	p->ambientIntensity = FLT2FIX(0);
	p->attenuation.x = FLT2FIX(1);
	p->attenuation.y = FLT2FIX(0);
	p->attenuation.z = FLT2FIX(0);
	p->beamWidth = FLT2FIX(1.570796);
	p->color.red = FLT2FIX(1);
	p->color.green = FLT2FIX(1);
	p->color.blue = FLT2FIX(1);
	p->cutOffAngle = FLT2FIX(0.785398);
	p->direction.x = FLT2FIX(0);
	p->direction.y = FLT2FIX(0);
	p->direction.z = FLT2FIX(-1);
	p->intensity = FLT2FIX(1);
	p->location.x = FLT2FIX(0);
	p->location.y = FLT2FIX(0);
	p->location.z = FLT2FIX(0);
	p->on = 1;
	p->radius = FLT2FIX(100);
	return (GF_Node *)p;
}


/*
	Switch Node deletion
*/

static void Switch_Del(GF_Node *node)
{
	M_Switch *p = (M_Switch *) node;
	gf_node_list_del((GF_List *) p->choice, (GF_Node *) p);	
	gf_node_free((GF_Node *) p);
}

static const u16 Switch_Def2All[] = { 0, 1};
static const u16 Switch_In2All[] = { 0, 1};
static const u16 Switch_Out2All[] = { 0, 1};

static u32 Switch_get_field_count(GF_Node *node, u8 IndexMode)
{
	switch(IndexMode) {
	case GF_SG_FIELD_CODING_IN: return 2;
	case GF_SG_FIELD_CODING_DEF: return 2;
	case GF_SG_FIELD_CODING_OUT: return 2;
	case GF_SG_FIELD_CODING_DYN: return 0;
	default:
		return 2;
	}
}

static GF_Err Switch_get_field_index(GF_Node *n, u32 inField, u8 IndexMode, u32 *allField)
{
	switch(IndexMode) {
	case GF_SG_FIELD_CODING_IN:
		*allField = Switch_In2All[inField];
		return GF_OK;
	case GF_SG_FIELD_CODING_DEF:
		*allField = Switch_Def2All[inField];
		return GF_OK;
	case GF_SG_FIELD_CODING_OUT:
		*allField = Switch_Out2All[inField];
		return GF_OK;
	default:
		return GF_BAD_PARAM;
	}
}
static GF_Err Switch_get_field(GF_Node *node, GF_FieldInfo *info)
{
	switch (info->fieldIndex) {
	case 0:
		info->name = "choice";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_MFNODE;
		info->NDTtype = NDT_SF3DNode;
		info->far_ptr = & ((M_Switch *)node)->choice;
		return GF_OK;
	case 1:
		info->name = "whichChoice";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFINT32;
		info->far_ptr = & ((M_Switch *) node)->whichChoice;
		return GF_OK;
	default:
		return GF_BAD_PARAM;
	}
}

static Bool Switch_get_aq_info(GF_Node *n, u32 FieldIndex, u8 *QType, u8 *AType, Fixed *b_min, Fixed *b_max, u32 *QT13_bits)
{
	switch (FieldIndex) {
	case 1:
		*AType = 0;
		*QType = 13;
		*QT13_bits = 10;
		*b_min = FLT2FIX(-1);
		*b_max = FLT2FIX( 1022);
		return 1;
	default:
		return 0;
	}
}



GF_Node *Switch_Create()
{
	M_Switch *p;
	GF_SAFEALLOC(p, sizeof(M_Switch));
	if(!p) return NULL;
	gf_node_setup((GF_Node *)p, TAG_MPEG4_Switch);
	p->choice = gf_list_new();	

#ifdef GF_NODE_USE_POINTERS
	((GF_Node *)p)->sgprivate->name = "Switch";
	((GF_Node *)p)->sgprivate->node_del = Switch_Del;
	((GF_Node *)p)->sgprivate->get_field_count = Switch_get_field_count;
	((GF_Node *)p)->sgprivate->get_field = Switch_get_field;
#endif


	/*default field values*/
	p->whichChoice = -1;
	return (GF_Node *)p;
}


/*
	TermCap Node deletion
*/

static void TermCap_Del(GF_Node *node)
{
	M_TermCap *p = (M_TermCap *) node;
	gf_node_free((GF_Node *) p);
}

static const u16 TermCap_Def2All[] = { 1};
static const u16 TermCap_In2All[] = { 0, 1};
static const u16 TermCap_Out2All[] = { 1, 2};

static u32 TermCap_get_field_count(GF_Node *node, u8 IndexMode)
{
	switch(IndexMode) {
	case GF_SG_FIELD_CODING_IN: return 2;
	case GF_SG_FIELD_CODING_DEF: return 1;
	case GF_SG_FIELD_CODING_OUT: return 2;
	case GF_SG_FIELD_CODING_DYN: return 0;
	default:
		return 3;
	}
}

static GF_Err TermCap_get_field_index(GF_Node *n, u32 inField, u8 IndexMode, u32 *allField)
{
	switch(IndexMode) {
	case GF_SG_FIELD_CODING_IN:
		*allField = TermCap_In2All[inField];
		return GF_OK;
	case GF_SG_FIELD_CODING_DEF:
		*allField = TermCap_Def2All[inField];
		return GF_OK;
	case GF_SG_FIELD_CODING_OUT:
		*allField = TermCap_Out2All[inField];
		return GF_OK;
	default:
		return GF_BAD_PARAM;
	}
}
static GF_Err TermCap_get_field(GF_Node *node, GF_FieldInfo *info)
{
	switch (info->fieldIndex) {
	case 0:
		info->name = "evaluate";
		info->eventType = GF_SG_EVENT_IN;
		info->on_event_in = ((M_TermCap *)node)->on_evaluate;
		info->fieldType = GF_SG_VRML_SFTIME;
		info->far_ptr = & ((M_TermCap *) node)->evaluate;
		return GF_OK;
	case 1:
		info->name = "capability";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFINT32;
		info->far_ptr = & ((M_TermCap *) node)->capability;
		return GF_OK;
	case 2:
		info->name = "value";
		info->eventType = GF_SG_EVENT_OUT;
		info->fieldType = GF_SG_VRML_SFINT32;
		info->far_ptr = & ((M_TermCap *) node)->value;
		return GF_OK;
	default:
		return GF_BAD_PARAM;
	}
}

static Bool TermCap_get_aq_info(GF_Node *n, u32 FieldIndex, u8 *QType, u8 *AType, Fixed *b_min, Fixed *b_max, u32 *QT13_bits)
{
	switch (FieldIndex) {
	case 1:
		*AType = 0;
		*QType = 13;
		*QT13_bits = 7;
		*b_min = FLT2FIX(0);
		*b_max = FLT2FIX(127);
		return 1;
	case 2:
		*AType = 0;
		*QType = 13;
		*QT13_bits = 3;
		*b_min = FLT2FIX(0);
		*b_max = FLT2FIX(7);
		return 1;
	default:
		return 0;
	}
}



GF_Node *TermCap_Create()
{
	M_TermCap *p;
	GF_SAFEALLOC(p, sizeof(M_TermCap));
	if(!p) return NULL;
	gf_node_setup((GF_Node *)p, TAG_MPEG4_TermCap);

#ifdef GF_NODE_USE_POINTERS
	((GF_Node *)p)->sgprivate->name = "TermCap";
	((GF_Node *)p)->sgprivate->node_del = TermCap_Del;
	((GF_Node *)p)->sgprivate->get_field_count = TermCap_get_field_count;
	((GF_Node *)p)->sgprivate->get_field = TermCap_get_field;
#endif


	/*default field values*/
	p->capability = 0;
	return (GF_Node *)p;
}


/*
	Text Node deletion
*/

static void Text_Del(GF_Node *node)
{
	M_Text *p = (M_Text *) node;
	gf_sg_mfstring_del(p->string);
	gf_sg_mffloat_del(p->length);
	gf_node_unregister((GF_Node *) p->fontStyle, (GF_Node *) p);	
	gf_node_free((GF_Node *) p);
}

static const u16 Text_Def2All[] = { 0, 1, 2, 3};
static const u16 Text_In2All[] = { 0, 1, 2, 3};
static const u16 Text_Out2All[] = { 0, 1, 2, 3};
static const u16 Text_Dyn2All[] = { 1, 3};

static u32 Text_get_field_count(GF_Node *node, u8 IndexMode)
{
	switch(IndexMode) {
	case GF_SG_FIELD_CODING_IN: return 4;
	case GF_SG_FIELD_CODING_DEF: return 4;
	case GF_SG_FIELD_CODING_OUT: return 4;
	case GF_SG_FIELD_CODING_DYN: return 2;
	default:
		return 4;
	}
}

static GF_Err Text_get_field_index(GF_Node *n, u32 inField, u8 IndexMode, u32 *allField)
{
	switch(IndexMode) {
	case GF_SG_FIELD_CODING_IN:
		*allField = Text_In2All[inField];
		return GF_OK;
	case GF_SG_FIELD_CODING_DEF:
		*allField = Text_Def2All[inField];
		return GF_OK;
	case GF_SG_FIELD_CODING_OUT:
		*allField = Text_Out2All[inField];
		return GF_OK;
	case GF_SG_FIELD_CODING_DYN:
		*allField = Text_Dyn2All[inField];
		return GF_OK;
	default:
		return GF_BAD_PARAM;
	}
}
static GF_Err Text_get_field(GF_Node *node, GF_FieldInfo *info)
{
	switch (info->fieldIndex) {
	case 0:
		info->name = "string";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_MFSTRING;
		info->far_ptr = & ((M_Text *) node)->string;
		return GF_OK;
	case 1:
		info->name = "length";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_MFFLOAT;
		info->far_ptr = & ((M_Text *) node)->length;
		return GF_OK;
	case 2:
		info->name = "fontStyle";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFNODE;
		info->NDTtype = NDT_SFFontStyleNode;
		info->far_ptr = & ((M_Text *)node)->fontStyle;
		return GF_OK;
	case 3:
		info->name = "maxExtent";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFFLOAT;
		info->far_ptr = & ((M_Text *) node)->maxExtent;
		return GF_OK;
	default:
		return GF_BAD_PARAM;
	}
}

static Bool Text_get_aq_info(GF_Node *n, u32 FieldIndex, u8 *QType, u8 *AType, Fixed *b_min, Fixed *b_max, u32 *QT13_bits)
{
	switch (FieldIndex) {
	case 1:
		*AType = 7;
		*QType = 11;
		*b_min = FLT2FIX(0);
		*b_max = FIX_MAX;
		return 1;
	case 3:
		*AType = 7;
		*QType = 11;
		*b_min = FLT2FIX(0);
		*b_max = FIX_MAX;
		return 1;
	default:
		return 0;
	}
}



GF_Node *Text_Create()
{
	M_Text *p;
	GF_SAFEALLOC(p, sizeof(M_Text));
	if(!p) return NULL;
	gf_node_setup((GF_Node *)p, TAG_MPEG4_Text);

#ifdef GF_NODE_USE_POINTERS
	((GF_Node *)p)->sgprivate->name = "Text";
	((GF_Node *)p)->sgprivate->node_del = Text_Del;
	((GF_Node *)p)->sgprivate->get_field_count = Text_get_field_count;
	((GF_Node *)p)->sgprivate->get_field = Text_get_field;
#endif


	/*default field values*/
	p->maxExtent = FLT2FIX(0.0);
	return (GF_Node *)p;
}


/*
	TextureCoordinate Node deletion
*/

static void TextureCoordinate_Del(GF_Node *node)
{
	M_TextureCoordinate *p = (M_TextureCoordinate *) node;
	gf_sg_mfvec2f_del(p->point);
	gf_node_free((GF_Node *) p);
}

static const u16 TextureCoordinate_Def2All[] = { 0};
static const u16 TextureCoordinate_In2All[] = { 0};
static const u16 TextureCoordinate_Out2All[] = { 0};
static const u16 TextureCoordinate_Dyn2All[] = { 0};

static u32 TextureCoordinate_get_field_count(GF_Node *node, u8 IndexMode)
{
	switch(IndexMode) {
	case GF_SG_FIELD_CODING_IN: return 1;
	case GF_SG_FIELD_CODING_DEF: return 1;
	case GF_SG_FIELD_CODING_OUT: return 1;
	case GF_SG_FIELD_CODING_DYN: return 1;
	default:
		return 1;
	}
}

static GF_Err TextureCoordinate_get_field_index(GF_Node *n, u32 inField, u8 IndexMode, u32 *allField)
{
	switch(IndexMode) {
	case GF_SG_FIELD_CODING_IN:
		*allField = TextureCoordinate_In2All[inField];
		return GF_OK;
	case GF_SG_FIELD_CODING_DEF:
		*allField = TextureCoordinate_Def2All[inField];
		return GF_OK;
	case GF_SG_FIELD_CODING_OUT:
		*allField = TextureCoordinate_Out2All[inField];
		return GF_OK;
	case GF_SG_FIELD_CODING_DYN:
		*allField = TextureCoordinate_Dyn2All[inField];
		return GF_OK;
	default:
		return GF_BAD_PARAM;
	}
}
static GF_Err TextureCoordinate_get_field(GF_Node *node, GF_FieldInfo *info)
{
	switch (info->fieldIndex) {
	case 0:
		info->name = "point";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_MFVEC2F;
		info->far_ptr = & ((M_TextureCoordinate *) node)->point;
		return GF_OK;
	default:
		return GF_BAD_PARAM;
	}
}

static Bool TextureCoordinate_get_aq_info(GF_Node *n, u32 FieldIndex, u8 *QType, u8 *AType, Fixed *b_min, Fixed *b_max, u32 *QT13_bits)
{
	switch (FieldIndex) {
	case 0:
		*AType = 2;
		*QType = 5;
		*b_min = FIX_MIN;
		*b_max = FIX_MAX;
		return 1;
	default:
		return 0;
	}
}



GF_Node *TextureCoordinate_Create()
{
	M_TextureCoordinate *p;
	GF_SAFEALLOC(p, sizeof(M_TextureCoordinate));
	if(!p) return NULL;
	gf_node_setup((GF_Node *)p, TAG_MPEG4_TextureCoordinate);

#ifdef GF_NODE_USE_POINTERS
	((GF_Node *)p)->sgprivate->name = "TextureCoordinate";
	((GF_Node *)p)->sgprivate->node_del = TextureCoordinate_Del;
	((GF_Node *)p)->sgprivate->get_field_count = TextureCoordinate_get_field_count;
	((GF_Node *)p)->sgprivate->get_field = TextureCoordinate_get_field;
#endif


	/*default field values*/
	return (GF_Node *)p;
}


/*
	TextureTransform Node deletion
*/

static void TextureTransform_Del(GF_Node *node)
{
	M_TextureTransform *p = (M_TextureTransform *) node;
	gf_node_free((GF_Node *) p);
}

static const u16 TextureTransform_Def2All[] = { 0, 1, 2, 3};
static const u16 TextureTransform_In2All[] = { 0, 1, 2, 3};
static const u16 TextureTransform_Out2All[] = { 0, 1, 2, 3};
static const u16 TextureTransform_Dyn2All[] = { 0, 1, 2, 3};

static u32 TextureTransform_get_field_count(GF_Node *node, u8 IndexMode)
{
	switch(IndexMode) {
	case GF_SG_FIELD_CODING_IN: return 4;
	case GF_SG_FIELD_CODING_DEF: return 4;
	case GF_SG_FIELD_CODING_OUT: return 4;
	case GF_SG_FIELD_CODING_DYN: return 4;
	default:
		return 4;
	}
}

static GF_Err TextureTransform_get_field_index(GF_Node *n, u32 inField, u8 IndexMode, u32 *allField)
{
	switch(IndexMode) {
	case GF_SG_FIELD_CODING_IN:
		*allField = TextureTransform_In2All[inField];
		return GF_OK;
	case GF_SG_FIELD_CODING_DEF:
		*allField = TextureTransform_Def2All[inField];
		return GF_OK;
	case GF_SG_FIELD_CODING_OUT:
		*allField = TextureTransform_Out2All[inField];
		return GF_OK;
	case GF_SG_FIELD_CODING_DYN:
		*allField = TextureTransform_Dyn2All[inField];
		return GF_OK;
	default:
		return GF_BAD_PARAM;
	}
}
static GF_Err TextureTransform_get_field(GF_Node *node, GF_FieldInfo *info)
{
	switch (info->fieldIndex) {
	case 0:
		info->name = "center";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFVEC2F;
		info->far_ptr = & ((M_TextureTransform *) node)->center;
		return GF_OK;
	case 1:
		info->name = "rotation";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFFLOAT;
		info->far_ptr = & ((M_TextureTransform *) node)->rotation;
		return GF_OK;
	case 2:
		info->name = "scale";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFVEC2F;
		info->far_ptr = & ((M_TextureTransform *) node)->scale;
		return GF_OK;
	case 3:
		info->name = "translation";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFVEC2F;
		info->far_ptr = & ((M_TextureTransform *) node)->translation;
		return GF_OK;
	default:
		return GF_BAD_PARAM;
	}
}

static Bool TextureTransform_get_aq_info(GF_Node *n, u32 FieldIndex, u8 *QType, u8 *AType, Fixed *b_min, Fixed *b_max, u32 *QT13_bits)
{
	switch (FieldIndex) {
	case 0:
		*AType = 2;
		*QType = 2;
		*b_min = FIX_MIN;
		*b_max = FIX_MAX;
		return 1;
	case 1:
		*AType = 6;
		*QType = 6;
		*b_min = FLT2FIX(0);
		*b_max = FLT2FIX(6.2831853);
		return 1;
	case 2:
		*AType = 12;
		*QType = 7;
		*b_min = FIX_MIN;
		*b_max = FIX_MAX;
		return 1;
	case 3:
		*AType = 2;
		*QType = 2;
		*b_min = FIX_MIN;
		*b_max = FIX_MAX;
		return 1;
	default:
		return 0;
	}
}



GF_Node *TextureTransform_Create()
{
	M_TextureTransform *p;
	GF_SAFEALLOC(p, sizeof(M_TextureTransform));
	if(!p) return NULL;
	gf_node_setup((GF_Node *)p, TAG_MPEG4_TextureTransform);

#ifdef GF_NODE_USE_POINTERS
	((GF_Node *)p)->sgprivate->name = "TextureTransform";
	((GF_Node *)p)->sgprivate->node_del = TextureTransform_Del;
	((GF_Node *)p)->sgprivate->get_field_count = TextureTransform_get_field_count;
	((GF_Node *)p)->sgprivate->get_field = TextureTransform_get_field;
#endif


	/*default field values*/
	p->center.x = FLT2FIX(0);
	p->center.y = FLT2FIX(0);
	p->rotation = FLT2FIX(0);
	p->scale.x = FLT2FIX(1);
	p->scale.y = FLT2FIX(1);
	p->translation.x = FLT2FIX(0);
	p->translation.y = FLT2FIX(0);
	return (GF_Node *)p;
}


/*
	TimeSensor Node deletion
*/

static void TimeSensor_Del(GF_Node *node)
{
	M_TimeSensor *p = (M_TimeSensor *) node;
	gf_node_free((GF_Node *) p);
}

static const u16 TimeSensor_Def2All[] = { 0, 1, 2, 3, 4};
static const u16 TimeSensor_In2All[] = { 0, 1, 2, 3, 4};
static const u16 TimeSensor_Out2All[] = { 0, 1, 2, 3, 4, 5, 6, 7, 8};

static u32 TimeSensor_get_field_count(GF_Node *node, u8 IndexMode)
{
	switch(IndexMode) {
	case GF_SG_FIELD_CODING_IN: return 5;
	case GF_SG_FIELD_CODING_DEF: return 5;
	case GF_SG_FIELD_CODING_OUT: return 9;
	case GF_SG_FIELD_CODING_DYN: return 0;
	default:
		return 9;
	}
}

static GF_Err TimeSensor_get_field_index(GF_Node *n, u32 inField, u8 IndexMode, u32 *allField)
{
	switch(IndexMode) {
	case GF_SG_FIELD_CODING_IN:
		*allField = TimeSensor_In2All[inField];
		return GF_OK;
	case GF_SG_FIELD_CODING_DEF:
		*allField = TimeSensor_Def2All[inField];
		return GF_OK;
	case GF_SG_FIELD_CODING_OUT:
		*allField = TimeSensor_Out2All[inField];
		return GF_OK;
	default:
		return GF_BAD_PARAM;
	}
}
static GF_Err TimeSensor_get_field(GF_Node *node, GF_FieldInfo *info)
{
	switch (info->fieldIndex) {
	case 0:
		info->name = "cycleInterval";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFTIME;
		info->far_ptr = & ((M_TimeSensor *) node)->cycleInterval;
		return GF_OK;
	case 1:
		info->name = "enabled";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFBOOL;
		info->far_ptr = & ((M_TimeSensor *) node)->enabled;
		return GF_OK;
	case 2:
		info->name = "loop";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFBOOL;
		info->far_ptr = & ((M_TimeSensor *) node)->loop;
		return GF_OK;
	case 3:
		info->name = "startTime";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFTIME;
		info->far_ptr = & ((M_TimeSensor *) node)->startTime;
		return GF_OK;
	case 4:
		info->name = "stopTime";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFTIME;
		info->far_ptr = & ((M_TimeSensor *) node)->stopTime;
		return GF_OK;
	case 5:
		info->name = "cycleTime";
		info->eventType = GF_SG_EVENT_OUT;
		info->fieldType = GF_SG_VRML_SFTIME;
		info->far_ptr = & ((M_TimeSensor *) node)->cycleTime;
		return GF_OK;
	case 6:
		info->name = "fraction_changed";
		info->eventType = GF_SG_EVENT_OUT;
		info->fieldType = GF_SG_VRML_SFFLOAT;
		info->far_ptr = & ((M_TimeSensor *) node)->fraction_changed;
		return GF_OK;
	case 7:
		info->name = "isActive";
		info->eventType = GF_SG_EVENT_OUT;
		info->fieldType = GF_SG_VRML_SFBOOL;
		info->far_ptr = & ((M_TimeSensor *) node)->isActive;
		return GF_OK;
	case 8:
		info->name = "time";
		info->eventType = GF_SG_EVENT_OUT;
		info->fieldType = GF_SG_VRML_SFTIME;
		info->far_ptr = & ((M_TimeSensor *) node)->time;
		return GF_OK;
	default:
		return GF_BAD_PARAM;
	}
}

static Bool TimeSensor_get_aq_info(GF_Node *n, u32 FieldIndex, u8 *QType, u8 *AType, Fixed *b_min, Fixed *b_max, u32 *QT13_bits)
{
	switch (FieldIndex) {
	default:
		return 0;
	}
}



GF_Node *TimeSensor_Create()
{
	M_TimeSensor *p;
	GF_SAFEALLOC(p, sizeof(M_TimeSensor));
	if(!p) return NULL;
	gf_node_setup((GF_Node *)p, TAG_MPEG4_TimeSensor);

#ifdef GF_NODE_USE_POINTERS
	((GF_Node *)p)->sgprivate->name = "TimeSensor";
	((GF_Node *)p)->sgprivate->node_del = TimeSensor_Del;
	((GF_Node *)p)->sgprivate->get_field_count = TimeSensor_get_field_count;
	((GF_Node *)p)->sgprivate->get_field = TimeSensor_get_field;
#endif


	/*default field values*/
	p->cycleInterval = 1;
	p->enabled = 1;
	p->startTime = 0;
	p->stopTime = 0;
	return (GF_Node *)p;
}


/*
	TouchSensor Node deletion
*/

static void TouchSensor_Del(GF_Node *node)
{
	M_TouchSensor *p = (M_TouchSensor *) node;
	gf_node_free((GF_Node *) p);
}

static const u16 TouchSensor_Def2All[] = { 0};
static const u16 TouchSensor_In2All[] = { 0};
static const u16 TouchSensor_Out2All[] = { 0, 1, 2, 3, 4, 5, 6};

static u32 TouchSensor_get_field_count(GF_Node *node, u8 IndexMode)
{
	switch(IndexMode) {
	case GF_SG_FIELD_CODING_IN: return 1;
	case GF_SG_FIELD_CODING_DEF: return 1;
	case GF_SG_FIELD_CODING_OUT: return 7;
	case GF_SG_FIELD_CODING_DYN: return 0;
	default:
		return 7;
	}
}

static GF_Err TouchSensor_get_field_index(GF_Node *n, u32 inField, u8 IndexMode, u32 *allField)
{
	switch(IndexMode) {
	case GF_SG_FIELD_CODING_IN:
		*allField = TouchSensor_In2All[inField];
		return GF_OK;
	case GF_SG_FIELD_CODING_DEF:
		*allField = TouchSensor_Def2All[inField];
		return GF_OK;
	case GF_SG_FIELD_CODING_OUT:
		*allField = TouchSensor_Out2All[inField];
		return GF_OK;
	default:
		return GF_BAD_PARAM;
	}
}
static GF_Err TouchSensor_get_field(GF_Node *node, GF_FieldInfo *info)
{
	switch (info->fieldIndex) {
	case 0:
		info->name = "enabled";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFBOOL;
		info->far_ptr = & ((M_TouchSensor *) node)->enabled;
		return GF_OK;
	case 1:
		info->name = "hitNormal_changed";
		info->eventType = GF_SG_EVENT_OUT;
		info->fieldType = GF_SG_VRML_SFVEC3F;
		info->far_ptr = & ((M_TouchSensor *) node)->hitNormal_changed;
		return GF_OK;
	case 2:
		info->name = "hitPoint_changed";
		info->eventType = GF_SG_EVENT_OUT;
		info->fieldType = GF_SG_VRML_SFVEC3F;
		info->far_ptr = & ((M_TouchSensor *) node)->hitPoint_changed;
		return GF_OK;
	case 3:
		info->name = "hitTexCoord_changed";
		info->eventType = GF_SG_EVENT_OUT;
		info->fieldType = GF_SG_VRML_SFVEC2F;
		info->far_ptr = & ((M_TouchSensor *) node)->hitTexCoord_changed;
		return GF_OK;
	case 4:
		info->name = "isActive";
		info->eventType = GF_SG_EVENT_OUT;
		info->fieldType = GF_SG_VRML_SFBOOL;
		info->far_ptr = & ((M_TouchSensor *) node)->isActive;
		return GF_OK;
	case 5:
		info->name = "isOver";
		info->eventType = GF_SG_EVENT_OUT;
		info->fieldType = GF_SG_VRML_SFBOOL;
		info->far_ptr = & ((M_TouchSensor *) node)->isOver;
		return GF_OK;
	case 6:
		info->name = "touchTime";
		info->eventType = GF_SG_EVENT_OUT;
		info->fieldType = GF_SG_VRML_SFTIME;
		info->far_ptr = & ((M_TouchSensor *) node)->touchTime;
		return GF_OK;
	default:
		return GF_BAD_PARAM;
	}
}

static Bool TouchSensor_get_aq_info(GF_Node *n, u32 FieldIndex, u8 *QType, u8 *AType, Fixed *b_min, Fixed *b_max, u32 *QT13_bits)
{
	switch (FieldIndex) {
	default:
		return 0;
	}
}



GF_Node *TouchSensor_Create()
{
	M_TouchSensor *p;
	GF_SAFEALLOC(p, sizeof(M_TouchSensor));
	if(!p) return NULL;
	gf_node_setup((GF_Node *)p, TAG_MPEG4_TouchSensor);

#ifdef GF_NODE_USE_POINTERS
	((GF_Node *)p)->sgprivate->name = "TouchSensor";
	((GF_Node *)p)->sgprivate->node_del = TouchSensor_Del;
	((GF_Node *)p)->sgprivate->get_field_count = TouchSensor_get_field_count;
	((GF_Node *)p)->sgprivate->get_field = TouchSensor_get_field;
#endif


	/*default field values*/
	p->enabled = 1;
	return (GF_Node *)p;
}


/*
	Transform Node deletion
*/

static void Transform_Del(GF_Node *node)
{
	M_Transform *p = (M_Transform *) node;
	gf_sg_vrml_parent_reset((GF_Node *) p);	
	gf_node_free((GF_Node *) p);
}

static const u16 Transform_Def2All[] = { 2, 3, 4, 5, 6, 7};
static const u16 Transform_In2All[] = { 0, 1, 2, 3, 4, 5, 6, 7};
static const u16 Transform_Out2All[] = { 2, 3, 4, 5, 6, 7};
static const u16 Transform_Dyn2All[] = { 2, 4, 5, 6, 7};

static u32 Transform_get_field_count(GF_Node *node, u8 IndexMode)
{
	switch(IndexMode) {
	case GF_SG_FIELD_CODING_IN: return 8;
	case GF_SG_FIELD_CODING_DEF: return 6;
	case GF_SG_FIELD_CODING_OUT: return 6;
	case GF_SG_FIELD_CODING_DYN: return 5;
	default:
		return 8;
	}
}

static GF_Err Transform_get_field_index(GF_Node *n, u32 inField, u8 IndexMode, u32 *allField)
{
	switch(IndexMode) {
	case GF_SG_FIELD_CODING_IN:
		*allField = Transform_In2All[inField];
		return GF_OK;
	case GF_SG_FIELD_CODING_DEF:
		*allField = Transform_Def2All[inField];
		return GF_OK;
	case GF_SG_FIELD_CODING_OUT:
		*allField = Transform_Out2All[inField];
		return GF_OK;
	case GF_SG_FIELD_CODING_DYN:
		*allField = Transform_Dyn2All[inField];
		return GF_OK;
	default:
		return GF_BAD_PARAM;
	}
}
static GF_Err Transform_get_field(GF_Node *node, GF_FieldInfo *info)
{
	switch (info->fieldIndex) {
	case 0:
		info->name = "addChildren";
		info->eventType = GF_SG_EVENT_IN;
		info->on_event_in = ((M_Transform *)node)->on_addChildren;
		info->fieldType = GF_SG_VRML_MFNODE;
		info->NDTtype = NDT_SF3DNode;
		info->far_ptr = & ((M_Transform *)node)->addChildren;
		return GF_OK;
	case 1:
		info->name = "removeChildren";
		info->eventType = GF_SG_EVENT_IN;
		info->on_event_in = ((M_Transform *)node)->on_removeChildren;
		info->fieldType = GF_SG_VRML_MFNODE;
		info->NDTtype = NDT_SF3DNode;
		info->far_ptr = & ((M_Transform *)node)->removeChildren;
		return GF_OK;
	case 2:
		info->name = "center";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFVEC3F;
		info->far_ptr = & ((M_Transform *) node)->center;
		return GF_OK;
	case 3:
		info->name = "children";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_MFNODE;
		info->NDTtype = NDT_SF3DNode;
		info->far_ptr = & ((M_Transform *)node)->children;
		return GF_OK;
	case 4:
		info->name = "rotation";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFROTATION;
		info->far_ptr = & ((M_Transform *) node)->rotation;
		return GF_OK;
	case 5:
		info->name = "scale";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFVEC3F;
		info->far_ptr = & ((M_Transform *) node)->scale;
		return GF_OK;
	case 6:
		info->name = "scaleOrientation";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFROTATION;
		info->far_ptr = & ((M_Transform *) node)->scaleOrientation;
		return GF_OK;
	case 7:
		info->name = "translation";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFVEC3F;
		info->far_ptr = & ((M_Transform *) node)->translation;
		return GF_OK;
	default:
		return GF_BAD_PARAM;
	}
}

static Bool Transform_get_aq_info(GF_Node *n, u32 FieldIndex, u8 *QType, u8 *AType, Fixed *b_min, Fixed *b_max, u32 *QT13_bits)
{
	switch (FieldIndex) {
	case 2:
		*AType = 1;
		*QType = 1;
		*b_min = FIX_MIN;
		*b_max = FIX_MAX;
		return 1;
	case 4:
		*AType = 10;
		*QType = 10;
		return 1;
	case 5:
		*AType = 11;
		*QType = 7;
		*b_min = FLT2FIX(0);
		*b_max = FIX_MAX;
		return 1;
	case 6:
		*AType = 10;
		*QType = 10;
		return 1;
	case 7:
		*AType = 1;
		*QType = 1;
		*b_min = FIX_MIN;
		*b_max = FIX_MAX;
		return 1;
	default:
		return 0;
	}
}



GF_Node *Transform_Create()
{
	M_Transform *p;
	GF_SAFEALLOC(p, sizeof(M_Transform));
	if(!p) return NULL;
	gf_node_setup((GF_Node *)p, TAG_MPEG4_Transform);
	gf_sg_vrml_parent_setup((GF_Node *) p);

#ifdef GF_NODE_USE_POINTERS
	((GF_Node *)p)->sgprivate->name = "Transform";
	((GF_Node *)p)->sgprivate->node_del = Transform_Del;
	((GF_Node *)p)->sgprivate->get_field_count = Transform_get_field_count;
	((GF_Node *)p)->sgprivate->get_field = Transform_get_field;
#endif


	/*default field values*/
	p->center.x = FLT2FIX(0);
	p->center.y = FLT2FIX(0);
	p->center.z = FLT2FIX(0);
	p->rotation.x = FLT2FIX(0);
	p->rotation.y = FLT2FIX(0);
	p->rotation.z = FLT2FIX(1);
	p->rotation.q = FLT2FIX(0);
	p->scale.x = FLT2FIX(1);
	p->scale.y = FLT2FIX(1);
	p->scale.z = FLT2FIX(1);
	p->scaleOrientation.x = FLT2FIX(0);
	p->scaleOrientation.y = FLT2FIX(0);
	p->scaleOrientation.z = FLT2FIX(1);
	p->scaleOrientation.q = FLT2FIX(0);
	p->translation.x = FLT2FIX(0);
	p->translation.y = FLT2FIX(0);
	p->translation.z = FLT2FIX(0);
	return (GF_Node *)p;
}


/*
	Transform2D Node deletion
*/

static void Transform2D_Del(GF_Node *node)
{
	M_Transform2D *p = (M_Transform2D *) node;
	gf_sg_vrml_parent_reset((GF_Node *) p);	
	gf_node_free((GF_Node *) p);
}

static const u16 Transform2D_Def2All[] = { 2, 3, 4, 5, 6, 7};
static const u16 Transform2D_In2All[] = { 0, 1, 2, 3, 4, 5, 6, 7};
static const u16 Transform2D_Out2All[] = { 2, 3, 4, 5, 6, 7};
static const u16 Transform2D_Dyn2All[] = { 3, 4, 5, 6, 7};

static u32 Transform2D_get_field_count(GF_Node *node, u8 IndexMode)
{
	switch(IndexMode) {
	case GF_SG_FIELD_CODING_IN: return 8;
	case GF_SG_FIELD_CODING_DEF: return 6;
	case GF_SG_FIELD_CODING_OUT: return 6;
	case GF_SG_FIELD_CODING_DYN: return 5;
	default:
		return 8;
	}
}

static GF_Err Transform2D_get_field_index(GF_Node *n, u32 inField, u8 IndexMode, u32 *allField)
{
	switch(IndexMode) {
	case GF_SG_FIELD_CODING_IN:
		*allField = Transform2D_In2All[inField];
		return GF_OK;
	case GF_SG_FIELD_CODING_DEF:
		*allField = Transform2D_Def2All[inField];
		return GF_OK;
	case GF_SG_FIELD_CODING_OUT:
		*allField = Transform2D_Out2All[inField];
		return GF_OK;
	case GF_SG_FIELD_CODING_DYN:
		*allField = Transform2D_Dyn2All[inField];
		return GF_OK;
	default:
		return GF_BAD_PARAM;
	}
}
static GF_Err Transform2D_get_field(GF_Node *node, GF_FieldInfo *info)
{
	switch (info->fieldIndex) {
	case 0:
		info->name = "addChildren";
		info->eventType = GF_SG_EVENT_IN;
		info->on_event_in = ((M_Transform2D *)node)->on_addChildren;
		info->fieldType = GF_SG_VRML_MFNODE;
		info->NDTtype = NDT_SF2DNode;
		info->far_ptr = & ((M_Transform2D *)node)->addChildren;
		return GF_OK;
	case 1:
		info->name = "removeChildren";
		info->eventType = GF_SG_EVENT_IN;
		info->on_event_in = ((M_Transform2D *)node)->on_removeChildren;
		info->fieldType = GF_SG_VRML_MFNODE;
		info->NDTtype = NDT_SF2DNode;
		info->far_ptr = & ((M_Transform2D *)node)->removeChildren;
		return GF_OK;
	case 2:
		info->name = "children";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_MFNODE;
		info->NDTtype = NDT_SF2DNode;
		info->far_ptr = & ((M_Transform2D *)node)->children;
		return GF_OK;
	case 3:
		info->name = "center";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFVEC2F;
		info->far_ptr = & ((M_Transform2D *) node)->center;
		return GF_OK;
	case 4:
		info->name = "rotationAngle";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFFLOAT;
		info->far_ptr = & ((M_Transform2D *) node)->rotationAngle;
		return GF_OK;
	case 5:
		info->name = "scale";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFVEC2F;
		info->far_ptr = & ((M_Transform2D *) node)->scale;
		return GF_OK;
	case 6:
		info->name = "scaleOrientation";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFFLOAT;
		info->far_ptr = & ((M_Transform2D *) node)->scaleOrientation;
		return GF_OK;
	case 7:
		info->name = "translation";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFVEC2F;
		info->far_ptr = & ((M_Transform2D *) node)->translation;
		return GF_OK;
	default:
		return GF_BAD_PARAM;
	}
}

static Bool Transform2D_get_aq_info(GF_Node *n, u32 FieldIndex, u8 *QType, u8 *AType, Fixed *b_min, Fixed *b_max, u32 *QT13_bits)
{
	switch (FieldIndex) {
	case 3:
		*AType = 2;
		*QType = 2;
		*b_min = FIX_MIN;
		*b_max = FIX_MAX;
		return 1;
	case 4:
		*AType = 6;
		*QType = 6;
		*b_min = FLT2FIX(0);
		*b_max = FLT2FIX(6.2831853);
		return 1;
	case 5:
		*AType = 12;
		*QType = 7;
		*b_min = FIX_MIN;
		*b_max = FIX_MAX;
		return 1;
	case 6:
		*AType = 6;
		*QType = 6;
		*b_min = FLT2FIX(0);
		*b_max = FLT2FIX(6.2831853);
		return 1;
	case 7:
		*AType = 2;
		*QType = 2;
		*b_min = FIX_MIN;
		*b_max = FIX_MAX;
		return 1;
	default:
		return 0;
	}
}



GF_Node *Transform2D_Create()
{
	M_Transform2D *p;
	GF_SAFEALLOC(p, sizeof(M_Transform2D));
	if(!p) return NULL;
	gf_node_setup((GF_Node *)p, TAG_MPEG4_Transform2D);
	gf_sg_vrml_parent_setup((GF_Node *) p);

#ifdef GF_NODE_USE_POINTERS
	((GF_Node *)p)->sgprivate->name = "Transform2D";
	((GF_Node *)p)->sgprivate->node_del = Transform2D_Del;
	((GF_Node *)p)->sgprivate->get_field_count = Transform2D_get_field_count;
	((GF_Node *)p)->sgprivate->get_field = Transform2D_get_field;
#endif


	/*default field values*/
	p->center.x = FLT2FIX(0);
	p->center.y = FLT2FIX(0);
	p->rotationAngle = FLT2FIX(0);
	p->scale.x = FLT2FIX(1);
	p->scale.y = FLT2FIX(1);
	p->scaleOrientation = FLT2FIX(0);
	p->translation.x = FLT2FIX(0);
	p->translation.y = FLT2FIX(0);
	return (GF_Node *)p;
}


/*
	Valuator Node deletion
*/

static void Valuator_Del(GF_Node *node)
{
	M_Valuator *p = (M_Valuator *) node;
	gf_sg_mfcolor_del(p->inMFColor);
	gf_sg_mffloat_del(p->inMFFloat);
	gf_sg_mfint32_del(p->inMFInt32);
	gf_sg_mfrotation_del(p->inMFRotation);
	gf_sg_sfstring_del(p->inSFString);
	gf_sg_mfstring_del(p->inMFString);
	gf_sg_mfvec2f_del(p->inMFVec2f);
	gf_sg_mfvec3f_del(p->inMFVec3f);
	gf_sg_mfcolor_del(p->outMFColor);
	gf_sg_mffloat_del(p->outMFFloat);
	gf_sg_mfint32_del(p->outMFInt32);
	gf_sg_mfrotation_del(p->outMFRotation);
	gf_sg_sfstring_del(p->outSFString);
	gf_sg_mfstring_del(p->outMFString);
	gf_sg_mfvec2f_del(p->outMFVec2f);
	gf_sg_mfvec3f_del(p->outMFVec3f);
	gf_node_free((GF_Node *) p);
}

static const u16 Valuator_Def2All[] = { 32, 33, 34, 35, 36, 37, 38, 39, 40};
static const u16 Valuator_In2All[] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 32, 33, 34, 35, 36, 37, 38, 39, 40};
static const u16 Valuator_Out2All[] = { 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40};

static u32 Valuator_get_field_count(GF_Node *node, u8 IndexMode)
{
	switch(IndexMode) {
	case GF_SG_FIELD_CODING_IN: return 25;
	case GF_SG_FIELD_CODING_DEF: return 9;
	case GF_SG_FIELD_CODING_OUT: return 25;
	case GF_SG_FIELD_CODING_DYN: return 0;
	default:
		return 41;
	}
}

static GF_Err Valuator_get_field_index(GF_Node *n, u32 inField, u8 IndexMode, u32 *allField)
{
	switch(IndexMode) {
	case GF_SG_FIELD_CODING_IN:
		*allField = Valuator_In2All[inField];
		return GF_OK;
	case GF_SG_FIELD_CODING_DEF:
		*allField = Valuator_Def2All[inField];
		return GF_OK;
	case GF_SG_FIELD_CODING_OUT:
		*allField = Valuator_Out2All[inField];
		return GF_OK;
	default:
		return GF_BAD_PARAM;
	}
}
static GF_Err Valuator_get_field(GF_Node *node, GF_FieldInfo *info)
{
	switch (info->fieldIndex) {
	case 0:
		info->name = "inSFBool";
		info->eventType = GF_SG_EVENT_IN;
		info->on_event_in = ((M_Valuator *)node)->on_inSFBool;
		info->fieldType = GF_SG_VRML_SFBOOL;
		info->far_ptr = & ((M_Valuator *) node)->inSFBool;
		return GF_OK;
	case 1:
		info->name = "inSFColor";
		info->eventType = GF_SG_EVENT_IN;
		info->on_event_in = ((M_Valuator *)node)->on_inSFColor;
		info->fieldType = GF_SG_VRML_SFCOLOR;
		info->far_ptr = & ((M_Valuator *) node)->inSFColor;
		return GF_OK;
	case 2:
		info->name = "inMFColor";
		info->eventType = GF_SG_EVENT_IN;
		info->on_event_in = ((M_Valuator *)node)->on_inMFColor;
		info->fieldType = GF_SG_VRML_MFCOLOR;
		info->far_ptr = & ((M_Valuator *) node)->inMFColor;
		return GF_OK;
	case 3:
		info->name = "inSFFloat";
		info->eventType = GF_SG_EVENT_IN;
		info->on_event_in = ((M_Valuator *)node)->on_inSFFloat;
		info->fieldType = GF_SG_VRML_SFFLOAT;
		info->far_ptr = & ((M_Valuator *) node)->inSFFloat;
		return GF_OK;
	case 4:
		info->name = "inMFFloat";
		info->eventType = GF_SG_EVENT_IN;
		info->on_event_in = ((M_Valuator *)node)->on_inMFFloat;
		info->fieldType = GF_SG_VRML_MFFLOAT;
		info->far_ptr = & ((M_Valuator *) node)->inMFFloat;
		return GF_OK;
	case 5:
		info->name = "inSFInt32";
		info->eventType = GF_SG_EVENT_IN;
		info->on_event_in = ((M_Valuator *)node)->on_inSFInt32;
		info->fieldType = GF_SG_VRML_SFINT32;
		info->far_ptr = & ((M_Valuator *) node)->inSFInt32;
		return GF_OK;
	case 6:
		info->name = "inMFInt32";
		info->eventType = GF_SG_EVENT_IN;
		info->on_event_in = ((M_Valuator *)node)->on_inMFInt32;
		info->fieldType = GF_SG_VRML_MFINT32;
		info->far_ptr = & ((M_Valuator *) node)->inMFInt32;
		return GF_OK;
	case 7:
		info->name = "inSFRotation";
		info->eventType = GF_SG_EVENT_IN;
		info->on_event_in = ((M_Valuator *)node)->on_inSFRotation;
		info->fieldType = GF_SG_VRML_SFROTATION;
		info->far_ptr = & ((M_Valuator *) node)->inSFRotation;
		return GF_OK;
	case 8:
		info->name = "inMFRotation";
		info->eventType = GF_SG_EVENT_IN;
		info->on_event_in = ((M_Valuator *)node)->on_inMFRotation;
		info->fieldType = GF_SG_VRML_MFROTATION;
		info->far_ptr = & ((M_Valuator *) node)->inMFRotation;
		return GF_OK;
	case 9:
		info->name = "inSFString";
		info->eventType = GF_SG_EVENT_IN;
		info->on_event_in = ((M_Valuator *)node)->on_inSFString;
		info->fieldType = GF_SG_VRML_SFSTRING;
		info->far_ptr = & ((M_Valuator *) node)->inSFString;
		return GF_OK;
	case 10:
		info->name = "inMFString";
		info->eventType = GF_SG_EVENT_IN;
		info->on_event_in = ((M_Valuator *)node)->on_inMFString;
		info->fieldType = GF_SG_VRML_MFSTRING;
		info->far_ptr = & ((M_Valuator *) node)->inMFString;
		return GF_OK;
	case 11:
		info->name = "inSFTime";
		info->eventType = GF_SG_EVENT_IN;
		info->on_event_in = ((M_Valuator *)node)->on_inSFTime;
		info->fieldType = GF_SG_VRML_SFTIME;
		info->far_ptr = & ((M_Valuator *) node)->inSFTime;
		return GF_OK;
	case 12:
		info->name = "inSFVec2f";
		info->eventType = GF_SG_EVENT_IN;
		info->on_event_in = ((M_Valuator *)node)->on_inSFVec2f;
		info->fieldType = GF_SG_VRML_SFVEC2F;
		info->far_ptr = & ((M_Valuator *) node)->inSFVec2f;
		return GF_OK;
	case 13:
		info->name = "inMFVec2f";
		info->eventType = GF_SG_EVENT_IN;
		info->on_event_in = ((M_Valuator *)node)->on_inMFVec2f;
		info->fieldType = GF_SG_VRML_MFVEC2F;
		info->far_ptr = & ((M_Valuator *) node)->inMFVec2f;
		return GF_OK;
	case 14:
		info->name = "inSFVec3f";
		info->eventType = GF_SG_EVENT_IN;
		info->on_event_in = ((M_Valuator *)node)->on_inSFVec3f;
		info->fieldType = GF_SG_VRML_SFVEC3F;
		info->far_ptr = & ((M_Valuator *) node)->inSFVec3f;
		return GF_OK;
	case 15:
		info->name = "inMFVec3f";
		info->eventType = GF_SG_EVENT_IN;
		info->on_event_in = ((M_Valuator *)node)->on_inMFVec3f;
		info->fieldType = GF_SG_VRML_MFVEC3F;
		info->far_ptr = & ((M_Valuator *) node)->inMFVec3f;
		return GF_OK;
	case 16:
		info->name = "outSFBool";
		info->eventType = GF_SG_EVENT_OUT;
		info->fieldType = GF_SG_VRML_SFBOOL;
		info->far_ptr = & ((M_Valuator *) node)->outSFBool;
		return GF_OK;
	case 17:
		info->name = "outSFColor";
		info->eventType = GF_SG_EVENT_OUT;
		info->fieldType = GF_SG_VRML_SFCOLOR;
		info->far_ptr = & ((M_Valuator *) node)->outSFColor;
		return GF_OK;
	case 18:
		info->name = "outMFColor";
		info->eventType = GF_SG_EVENT_OUT;
		info->fieldType = GF_SG_VRML_MFCOLOR;
		info->far_ptr = & ((M_Valuator *) node)->outMFColor;
		return GF_OK;
	case 19:
		info->name = "outSFFloat";
		info->eventType = GF_SG_EVENT_OUT;
		info->fieldType = GF_SG_VRML_SFFLOAT;
		info->far_ptr = & ((M_Valuator *) node)->outSFFloat;
		return GF_OK;
	case 20:
		info->name = "outMFFloat";
		info->eventType = GF_SG_EVENT_OUT;
		info->fieldType = GF_SG_VRML_MFFLOAT;
		info->far_ptr = & ((M_Valuator *) node)->outMFFloat;
		return GF_OK;
	case 21:
		info->name = "outSFInt32";
		info->eventType = GF_SG_EVENT_OUT;
		info->fieldType = GF_SG_VRML_SFINT32;
		info->far_ptr = & ((M_Valuator *) node)->outSFInt32;
		return GF_OK;
	case 22:
		info->name = "outMFInt32";
		info->eventType = GF_SG_EVENT_OUT;
		info->fieldType = GF_SG_VRML_MFINT32;
		info->far_ptr = & ((M_Valuator *) node)->outMFInt32;
		return GF_OK;
	case 23:
		info->name = "outSFRotation";
		info->eventType = GF_SG_EVENT_OUT;
		info->fieldType = GF_SG_VRML_SFROTATION;
		info->far_ptr = & ((M_Valuator *) node)->outSFRotation;
		return GF_OK;
	case 24:
		info->name = "outMFRotation";
		info->eventType = GF_SG_EVENT_OUT;
		info->fieldType = GF_SG_VRML_MFROTATION;
		info->far_ptr = & ((M_Valuator *) node)->outMFRotation;
		return GF_OK;
	case 25:
		info->name = "outSFString";
		info->eventType = GF_SG_EVENT_OUT;
		info->fieldType = GF_SG_VRML_SFSTRING;
		info->far_ptr = & ((M_Valuator *) node)->outSFString;
		return GF_OK;
	case 26:
		info->name = "outMFString";
		info->eventType = GF_SG_EVENT_OUT;
		info->fieldType = GF_SG_VRML_MFSTRING;
		info->far_ptr = & ((M_Valuator *) node)->outMFString;
		return GF_OK;
	case 27:
		info->name = "outSFTime";
		info->eventType = GF_SG_EVENT_OUT;
		info->fieldType = GF_SG_VRML_SFTIME;
		info->far_ptr = & ((M_Valuator *) node)->outSFTime;
		return GF_OK;
	case 28:
		info->name = "outSFVec2f";
		info->eventType = GF_SG_EVENT_OUT;
		info->fieldType = GF_SG_VRML_SFVEC2F;
		info->far_ptr = & ((M_Valuator *) node)->outSFVec2f;
		return GF_OK;
	case 29:
		info->name = "outMFVec2f";
		info->eventType = GF_SG_EVENT_OUT;
		info->fieldType = GF_SG_VRML_MFVEC2F;
		info->far_ptr = & ((M_Valuator *) node)->outMFVec2f;
		return GF_OK;
	case 30:
		info->name = "outSFVec3f";
		info->eventType = GF_SG_EVENT_OUT;
		info->fieldType = GF_SG_VRML_SFVEC3F;
		info->far_ptr = & ((M_Valuator *) node)->outSFVec3f;
		return GF_OK;
	case 31:
		info->name = "outMFVec3f";
		info->eventType = GF_SG_EVENT_OUT;
		info->fieldType = GF_SG_VRML_MFVEC3F;
		info->far_ptr = & ((M_Valuator *) node)->outMFVec3f;
		return GF_OK;
	case 32:
		info->name = "Factor1";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFFLOAT;
		info->far_ptr = & ((M_Valuator *) node)->Factor1;
		return GF_OK;
	case 33:
		info->name = "Factor2";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFFLOAT;
		info->far_ptr = & ((M_Valuator *) node)->Factor2;
		return GF_OK;
	case 34:
		info->name = "Factor3";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFFLOAT;
		info->far_ptr = & ((M_Valuator *) node)->Factor3;
		return GF_OK;
	case 35:
		info->name = "Factor4";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFFLOAT;
		info->far_ptr = & ((M_Valuator *) node)->Factor4;
		return GF_OK;
	case 36:
		info->name = "Offset1";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFFLOAT;
		info->far_ptr = & ((M_Valuator *) node)->Offset1;
		return GF_OK;
	case 37:
		info->name = "Offset2";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFFLOAT;
		info->far_ptr = & ((M_Valuator *) node)->Offset2;
		return GF_OK;
	case 38:
		info->name = "Offset3";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFFLOAT;
		info->far_ptr = & ((M_Valuator *) node)->Offset3;
		return GF_OK;
	case 39:
		info->name = "Offset4";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFFLOAT;
		info->far_ptr = & ((M_Valuator *) node)->Offset4;
		return GF_OK;
	case 40:
		info->name = "Sum";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFBOOL;
		info->far_ptr = & ((M_Valuator *) node)->Sum;
		return GF_OK;
	default:
		return GF_BAD_PARAM;
	}
}

static Bool Valuator_get_aq_info(GF_Node *n, u32 FieldIndex, u8 *QType, u8 *AType, Fixed *b_min, Fixed *b_max, u32 *QT13_bits)
{
	switch (FieldIndex) {
	case 32:
		*AType = 0;
		*QType = 0;
		*b_min = FIX_MIN;
		*b_max = FIX_MAX;
		return 1;
	case 33:
		*AType = 0;
		*QType = 0;
		*b_min = FIX_MIN;
		*b_max = FIX_MAX;
		return 1;
	case 34:
		*AType = 0;
		*QType = 0;
		*b_min = FIX_MIN;
		*b_max = FIX_MAX;
		return 1;
	case 35:
		*AType = 0;
		*QType = 0;
		*b_min = FIX_MIN;
		*b_max = FIX_MAX;
		return 1;
	case 36:
		*AType = 0;
		*QType = 0;
		*b_min = FIX_MIN;
		*b_max = FIX_MAX;
		return 1;
	case 37:
		*AType = 0;
		*QType = 0;
		*b_min = FIX_MIN;
		*b_max = FIX_MAX;
		return 1;
	case 38:
		*AType = 0;
		*QType = 0;
		*b_min = FIX_MIN;
		*b_max = FIX_MAX;
		return 1;
	case 39:
		*AType = 0;
		*QType = 0;
		*b_min = FIX_MIN;
		*b_max = FIX_MAX;
		return 1;
	default:
		return 0;
	}
}



GF_Node *Valuator_Create()
{
	M_Valuator *p;
	GF_SAFEALLOC(p, sizeof(M_Valuator));
	if(!p) return NULL;
	gf_node_setup((GF_Node *)p, TAG_MPEG4_Valuator);

#ifdef GF_NODE_USE_POINTERS
	((GF_Node *)p)->sgprivate->name = "Valuator";
	((GF_Node *)p)->sgprivate->node_del = Valuator_Del;
	((GF_Node *)p)->sgprivate->get_field_count = Valuator_get_field_count;
	((GF_Node *)p)->sgprivate->get_field = Valuator_get_field;
#endif


	/*default field values*/
	p->Factor1 = FLT2FIX(1.0);
	p->Factor2 = FLT2FIX(1.0);
	p->Factor3 = FLT2FIX(1.0);
	p->Factor4 = FLT2FIX(1.0);
	p->Offset1 = FLT2FIX(0.0);
	p->Offset2 = FLT2FIX(0.0);
	p->Offset3 = FLT2FIX(0.0);
	p->Offset4 = FLT2FIX(0.0);
	return (GF_Node *)p;
}


/*
	Viewpoint Node deletion
*/

static void Viewpoint_Del(GF_Node *node)
{
	M_Viewpoint *p = (M_Viewpoint *) node;
	gf_sg_sfstring_del(p->description);
	gf_node_free((GF_Node *) p);
}

static const u16 Viewpoint_Def2All[] = { 1, 2, 3, 4, 5};
static const u16 Viewpoint_In2All[] = { 0, 1, 2, 3, 4};
static const u16 Viewpoint_Out2All[] = { 1, 2, 3, 4, 6, 7};
static const u16 Viewpoint_Dyn2All[] = { 1, 3, 4};

static u32 Viewpoint_get_field_count(GF_Node *node, u8 IndexMode)
{
	switch(IndexMode) {
	case GF_SG_FIELD_CODING_IN: return 5;
	case GF_SG_FIELD_CODING_DEF: return 5;
	case GF_SG_FIELD_CODING_OUT: return 6;
	case GF_SG_FIELD_CODING_DYN: return 3;
	default:
		return 8;
	}
}

static GF_Err Viewpoint_get_field_index(GF_Node *n, u32 inField, u8 IndexMode, u32 *allField)
{
	switch(IndexMode) {
	case GF_SG_FIELD_CODING_IN:
		*allField = Viewpoint_In2All[inField];
		return GF_OK;
	case GF_SG_FIELD_CODING_DEF:
		*allField = Viewpoint_Def2All[inField];
		return GF_OK;
	case GF_SG_FIELD_CODING_OUT:
		*allField = Viewpoint_Out2All[inField];
		return GF_OK;
	case GF_SG_FIELD_CODING_DYN:
		*allField = Viewpoint_Dyn2All[inField];
		return GF_OK;
	default:
		return GF_BAD_PARAM;
	}
}
static GF_Err Viewpoint_get_field(GF_Node *node, GF_FieldInfo *info)
{
	switch (info->fieldIndex) {
	case 0:
		info->name = "set_bind";
		info->eventType = GF_SG_EVENT_IN;
		info->on_event_in = ((M_Viewpoint *)node)->on_set_bind;
		info->fieldType = GF_SG_VRML_SFBOOL;
		info->far_ptr = & ((M_Viewpoint *) node)->set_bind;
		return GF_OK;
	case 1:
		info->name = "fieldOfView";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFFLOAT;
		info->far_ptr = & ((M_Viewpoint *) node)->fieldOfView;
		return GF_OK;
	case 2:
		info->name = "jump";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFBOOL;
		info->far_ptr = & ((M_Viewpoint *) node)->jump;
		return GF_OK;
	case 3:
		info->name = "orientation";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFROTATION;
		info->far_ptr = & ((M_Viewpoint *) node)->orientation;
		return GF_OK;
	case 4:
		info->name = "position";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFVEC3F;
		info->far_ptr = & ((M_Viewpoint *) node)->position;
		return GF_OK;
	case 5:
		info->name = "description";
		info->eventType = GF_SG_EVENT_FIELD;
		info->fieldType = GF_SG_VRML_SFSTRING;
		info->far_ptr = & ((M_Viewpoint *) node)->description;
		return GF_OK;
	case 6:
		info->name = "bindTime";
		info->eventType = GF_SG_EVENT_OUT;
		info->fieldType = GF_SG_VRML_SFTIME;
		info->far_ptr = & ((M_Viewpoint *) node)->bindTime;
		return GF_OK;
	case 7:
		info->name = "isBound";
		info->eventType = GF_SG_EVENT_OUT;
		info->fieldType = GF_SG_VRML_SFBOOL;
		info->far_ptr = & ((M_Viewpoint *) node)->isBound;
		return GF_OK;
	default:
		return GF_BAD_PARAM;
	}
}

static Bool Viewpoint_get_aq_info(GF_Node *n, u32 FieldIndex, u8 *QType, u8 *AType, Fixed *b_min, Fixed *b_max, u32 *QT13_bits)
{
	switch (FieldIndex) {
	case 1:
		*AType = 8;
		*QType = 6;
		*b_min = FLT2FIX(0);
		*b_max = FLT2FIX(3.1415927);
		return 1;
	case 3:
		*AType = 10;
		*QType = 10;
		return 1;
	case 4:
		*AType = 1;
		*QType = 1;
		*b_min = FIX_MIN;
		*b_max = FIX_MAX;
		return 1;
	default:
		return 0;
	}
}



GF_Node *Viewpoint_Create()
{
	M_Viewpoint *p;
	GF_SAFEALLOC(p, sizeof(M_Viewpoint));
	if(!p) return NULL;
	gf_node_setup((GF_Node *)p, TAG_MPEG4_Viewpoint);

#ifdef GF_NODE_USE_POINTERS
	((GF_Node *)p)->sgprivate->name = "Viewpoint";
	((GF_Node *)p)->sgprivate->node_del = Viewpoint_Del;
	((GF_Node *)p)->sgprivate->get_field_count = Viewpoint_get_field_count;
	((GF_Node *)p)->sgprivate->get_field = Viewpoint_get_field;
#endif


	/*default field values*/
	p->fieldOfView = FLT2FIX(0.785398);
	p->jump = 1;
	p->orientation.x = FLT2FIX(0);
	p->orientation.y = FLT2FIX(0);
	p->orientation.z = FLT2FIX(1);
	p->orientation.q = FLT2FIX(0);
	p->position.x = FLT2FIX(0);
	p->position.y = FLT2FIX(0);
	p->position.z = FLT2FIX(10);
	return (GF_Node *)p;
}


/*
	VisibilitySensor Node deletion
*/

static void VisibilitySensor_Del(GF_Node *node)
{
	M_VisibilitySensor *p = (M_VisibilitySensor *) node;
	gf_node_free((GF_Node *) p);
}

static const u16 VisibilitySensor_Def2All[] = { 0, 1, 2};
static const u16 VisibilitySensor_In2All[] = { 0, 1, 2};
static const u16 VisibilitySensor_Out2All[] = { 0, 1, 2, 3, 4, 5};
static const u16 VisibilitySensor_Dyn2All[] = { 0, 2};

static u32 VisibilitySensor_get_field_count(GF_Node *node, u8 IndexMode)
{
	switch(IndexMode) {
	case GF_SG_FIELD_CODING_IN: return 3;
	case GF_SG_FIELD_CODING_DEF: return 3;
	case GF_SG_FIELD_CODING_OUT: return 6;
	case GF_SG_FIELD_CODING_DYN: return 2;
	default:
		return 6;
	}
}

static GF_Err VisibilitySensor_get_field_index(GF_Node *n, u32 inField, u8 IndexMode, u32 *allField)
{
	switch(IndexMode) {
	case GF_SG_FIELD_CODING_IN:
		*allField = VisibilitySensor_In2All[inField];
		return GF_OK;
	case GF_SG_FIELD_CODING_DEF:
		*allField = VisibilitySensor_Def2All[inField];
		return GF_OK;
	case GF_SG_FIELD_CODING_OUT:
		*allField = VisibilitySensor_Out2All[inField];
		return GF_OK;
	case GF_SG_FIELD_CODING_DYN:
		*allField = VisibilitySensor_Dyn2All[inField];
		return GF_OK;
	default:
		return GF_BAD_PARAM;
	}
}
static GF_Err VisibilitySensor_get_field(GF_Node *node, GF_FieldInfo *info)
{
	switch (info->fieldIndex) {
	case 0:
		info->name = "center";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFVEC3F;
		info->far_ptr = & ((M_VisibilitySensor *) node)->center;
		return GF_OK;
	case 1:
		info->name = "enabled";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFBOOL;
		info->far_ptr = & ((M_VisibilitySensor *) node)->enabled;
		return GF_OK;
	case 2:
		info->name = "size";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFVEC3F;
		info->far_ptr = & ((M_VisibilitySensor *) node)->size;
		return GF_OK;
	case 3:
		info->name = "enterTime";
		info->eventType = GF_SG_EVENT_OUT;
		info->fieldType = GF_SG_VRML_SFTIME;
		info->far_ptr = & ((M_VisibilitySensor *) node)->enterTime;
		return GF_OK;
	case 4:
		info->name = "exitTime";
		info->eventType = GF_SG_EVENT_OUT;
		info->fieldType = GF_SG_VRML_SFTIME;
		info->far_ptr = & ((M_VisibilitySensor *) node)->exitTime;
		return GF_OK;
	case 5:
		info->name = "isActive";
		info->eventType = GF_SG_EVENT_OUT;
		info->fieldType = GF_SG_VRML_SFBOOL;
		info->far_ptr = & ((M_VisibilitySensor *) node)->isActive;
		return GF_OK;
	default:
		return GF_BAD_PARAM;
	}
}

static Bool VisibilitySensor_get_aq_info(GF_Node *n, u32 FieldIndex, u8 *QType, u8 *AType, Fixed *b_min, Fixed *b_max, u32 *QT13_bits)
{
	switch (FieldIndex) {
	case 0:
		*AType = 1;
		*QType = 1;
		*b_min = FIX_MIN;
		*b_max = FIX_MAX;
		return 1;
	case 2:
		*AType = 11;
		*QType = 11;
		*b_min = FLT2FIX(0);
		*b_max = FIX_MAX;
		return 1;
	default:
		return 0;
	}
}



GF_Node *VisibilitySensor_Create()
{
	M_VisibilitySensor *p;
	GF_SAFEALLOC(p, sizeof(M_VisibilitySensor));
	if(!p) return NULL;
	gf_node_setup((GF_Node *)p, TAG_MPEG4_VisibilitySensor);

#ifdef GF_NODE_USE_POINTERS
	((GF_Node *)p)->sgprivate->name = "VisibilitySensor";
	((GF_Node *)p)->sgprivate->node_del = VisibilitySensor_Del;
	((GF_Node *)p)->sgprivate->get_field_count = VisibilitySensor_get_field_count;
	((GF_Node *)p)->sgprivate->get_field = VisibilitySensor_get_field;
#endif


	/*default field values*/
	p->center.x = FLT2FIX(0);
	p->center.y = FLT2FIX(0);
	p->center.z = FLT2FIX(0);
	p->enabled = 1;
	p->size.x = FLT2FIX(0);
	p->size.y = FLT2FIX(0);
	p->size.z = FLT2FIX(0);
	return (GF_Node *)p;
}


/*
	WorldInfo Node deletion
*/

static void WorldInfo_Del(GF_Node *node)
{
	M_WorldInfo *p = (M_WorldInfo *) node;
	gf_sg_mfstring_del(p->info);
	gf_sg_sfstring_del(p->title);
	gf_node_free((GF_Node *) p);
}

static const u16 WorldInfo_Def2All[] = { 0, 1};

static u32 WorldInfo_get_field_count(GF_Node *node, u8 IndexMode)
{
	switch(IndexMode) {
	case GF_SG_FIELD_CODING_IN: return 0;
	case GF_SG_FIELD_CODING_DEF: return 2;
	case GF_SG_FIELD_CODING_OUT: return 0;
	case GF_SG_FIELD_CODING_DYN: return 0;
	default:
		return 2;
	}
}

static GF_Err WorldInfo_get_field_index(GF_Node *n, u32 inField, u8 IndexMode, u32 *allField)
{
	switch(IndexMode) {
	case GF_SG_FIELD_CODING_DEF:
		*allField = WorldInfo_Def2All[inField];
		return GF_OK;
	default:
		return GF_BAD_PARAM;
	}
}
static GF_Err WorldInfo_get_field(GF_Node *node, GF_FieldInfo *info)
{
	switch (info->fieldIndex) {
	case 0:
		info->name = "info";
		info->eventType = GF_SG_EVENT_FIELD;
		info->fieldType = GF_SG_VRML_MFSTRING;
		info->far_ptr = & ((M_WorldInfo *) node)->info;
		return GF_OK;
	case 1:
		info->name = "title";
		info->eventType = GF_SG_EVENT_FIELD;
		info->fieldType = GF_SG_VRML_SFSTRING;
		info->far_ptr = & ((M_WorldInfo *) node)->title;
		return GF_OK;
	default:
		return GF_BAD_PARAM;
	}
}

static Bool WorldInfo_get_aq_info(GF_Node *n, u32 FieldIndex, u8 *QType, u8 *AType, Fixed *b_min, Fixed *b_max, u32 *QT13_bits)
{
	switch (FieldIndex) {
	default:
		return 0;
	}
}



GF_Node *WorldInfo_Create()
{
	M_WorldInfo *p;
	GF_SAFEALLOC(p, sizeof(M_WorldInfo));
	if(!p) return NULL;
	gf_node_setup((GF_Node *)p, TAG_MPEG4_WorldInfo);

#ifdef GF_NODE_USE_POINTERS
	((GF_Node *)p)->sgprivate->name = "WorldInfo";
	((GF_Node *)p)->sgprivate->node_del = WorldInfo_Del;
	((GF_Node *)p)->sgprivate->get_field_count = WorldInfo_get_field_count;
	((GF_Node *)p)->sgprivate->get_field = WorldInfo_get_field;
#endif


	/*default field values*/
	return (GF_Node *)p;
}


/*
	AcousticMaterial Node deletion
*/

static void AcousticMaterial_Del(GF_Node *node)
{
	M_AcousticMaterial *p = (M_AcousticMaterial *) node;
	gf_sg_mffloat_del(p->reffunc);
	gf_sg_mffloat_del(p->transfunc);
	gf_sg_mffloat_del(p->refFrequency);
	gf_sg_mffloat_del(p->transFrequency);
	gf_node_free((GF_Node *) p);
}

static const u16 AcousticMaterial_Def2All[] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9};
static const u16 AcousticMaterial_In2All[] = { 0, 1, 2, 3, 4, 5};
static const u16 AcousticMaterial_Out2All[] = { 0, 1, 2, 3, 4, 5};
static const u16 AcousticMaterial_Dyn2All[] = { 0, 1, 2, 3, 4, 5};

static u32 AcousticMaterial_get_field_count(GF_Node *node, u8 IndexMode)
{
	switch(IndexMode) {
	case GF_SG_FIELD_CODING_IN: return 6;
	case GF_SG_FIELD_CODING_DEF: return 10;
	case GF_SG_FIELD_CODING_OUT: return 6;
	case GF_SG_FIELD_CODING_DYN: return 6;
	default:
		return 10;
	}
}

static GF_Err AcousticMaterial_get_field_index(GF_Node *n, u32 inField, u8 IndexMode, u32 *allField)
{
	switch(IndexMode) {
	case GF_SG_FIELD_CODING_IN:
		*allField = AcousticMaterial_In2All[inField];
		return GF_OK;
	case GF_SG_FIELD_CODING_DEF:
		*allField = AcousticMaterial_Def2All[inField];
		return GF_OK;
	case GF_SG_FIELD_CODING_OUT:
		*allField = AcousticMaterial_Out2All[inField];
		return GF_OK;
	case GF_SG_FIELD_CODING_DYN:
		*allField = AcousticMaterial_Dyn2All[inField];
		return GF_OK;
	default:
		return GF_BAD_PARAM;
	}
}
static GF_Err AcousticMaterial_get_field(GF_Node *node, GF_FieldInfo *info)
{
	switch (info->fieldIndex) {
	case 0:
		info->name = "ambientIntensity";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFFLOAT;
		info->far_ptr = & ((M_AcousticMaterial *) node)->ambientIntensity;
		return GF_OK;
	case 1:
		info->name = "diffuseColor";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFCOLOR;
		info->far_ptr = & ((M_AcousticMaterial *) node)->diffuseColor;
		return GF_OK;
	case 2:
		info->name = "emissiveColor";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFCOLOR;
		info->far_ptr = & ((M_AcousticMaterial *) node)->emissiveColor;
		return GF_OK;
	case 3:
		info->name = "shininess";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFFLOAT;
		info->far_ptr = & ((M_AcousticMaterial *) node)->shininess;
		return GF_OK;
	case 4:
		info->name = "specularColor";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFCOLOR;
		info->far_ptr = & ((M_AcousticMaterial *) node)->specularColor;
		return GF_OK;
	case 5:
		info->name = "transparency";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFFLOAT;
		info->far_ptr = & ((M_AcousticMaterial *) node)->transparency;
		return GF_OK;
	case 6:
		info->name = "reffunc";
		info->eventType = GF_SG_EVENT_FIELD;
		info->fieldType = GF_SG_VRML_MFFLOAT;
		info->far_ptr = & ((M_AcousticMaterial *) node)->reffunc;
		return GF_OK;
	case 7:
		info->name = "transfunc";
		info->eventType = GF_SG_EVENT_FIELD;
		info->fieldType = GF_SG_VRML_MFFLOAT;
		info->far_ptr = & ((M_AcousticMaterial *) node)->transfunc;
		return GF_OK;
	case 8:
		info->name = "refFrequency";
		info->eventType = GF_SG_EVENT_FIELD;
		info->fieldType = GF_SG_VRML_MFFLOAT;
		info->far_ptr = & ((M_AcousticMaterial *) node)->refFrequency;
		return GF_OK;
	case 9:
		info->name = "transFrequency";
		info->eventType = GF_SG_EVENT_FIELD;
		info->fieldType = GF_SG_VRML_MFFLOAT;
		info->far_ptr = & ((M_AcousticMaterial *) node)->transFrequency;
		return GF_OK;
	default:
		return GF_BAD_PARAM;
	}
}

static Bool AcousticMaterial_get_aq_info(GF_Node *n, u32 FieldIndex, u8 *QType, u8 *AType, Fixed *b_min, Fixed *b_max, u32 *QT13_bits)
{
	switch (FieldIndex) {
	case 0:
		*AType = 8;
		*QType = 4;
		*b_min = FLT2FIX(0);
		*b_max = FLT2FIX(1);
		return 1;
	case 1:
		*AType = 8;
		*QType = 4;
		*b_min = FLT2FIX(0);
		*b_max = FLT2FIX(1);
		return 1;
	case 2:
		*AType = 8;
		*QType = 4;
		*b_min = FLT2FIX(0);
		*b_max = FLT2FIX(1);
		return 1;
	case 3:
		*AType = 8;
		*QType = 4;
		*b_min = FLT2FIX(0);
		*b_max = FLT2FIX(1);
		return 1;
	case 4:
		*AType = 8;
		*QType = 4;
		*b_min = FLT2FIX(0);
		*b_max = FLT2FIX(1);
		return 1;
	case 5:
		*AType = 8;
		*QType = 4;
		*b_min = FLT2FIX(0);
		*b_max = FLT2FIX(1);
		return 1;
	case 6:
		*AType = 0;
		*QType = 0;
		*b_min = FIX_MIN;
		*b_max = FIX_MAX;
		return 1;
	case 7:
		*AType = 0;
		*QType = 0;
		*b_min = FIX_MIN;
		*b_max = FIX_MAX;
		return 1;
	case 8:
		*AType = 0;
		*QType = 0;
		*b_min = FLT2FIX(0);
		*b_max = FIX_MAX;
		return 1;
	case 9:
		*AType = 0;
		*QType = 0;
		*b_min = FLT2FIX(0);
		*b_max = FIX_MAX;
		return 1;
	default:
		return 0;
	}
}



GF_Node *AcousticMaterial_Create()
{
	M_AcousticMaterial *p;
	GF_SAFEALLOC(p, sizeof(M_AcousticMaterial));
	if(!p) return NULL;
	gf_node_setup((GF_Node *)p, TAG_MPEG4_AcousticMaterial);

#ifdef GF_NODE_USE_POINTERS
	((GF_Node *)p)->sgprivate->name = "AcousticMaterial";
	((GF_Node *)p)->sgprivate->node_del = AcousticMaterial_Del;
	((GF_Node *)p)->sgprivate->get_field_count = AcousticMaterial_get_field_count;
	((GF_Node *)p)->sgprivate->get_field = AcousticMaterial_get_field;
#endif


	/*default field values*/
	p->ambientIntensity = FLT2FIX(0.2);
	p->diffuseColor.red = FLT2FIX(0.8);
	p->diffuseColor.green = FLT2FIX(0.8);
	p->diffuseColor.blue = FLT2FIX(0.8);
	p->emissiveColor.red = FLT2FIX(0);
	p->emissiveColor.green = FLT2FIX(0);
	p->emissiveColor.blue = FLT2FIX(0);
	p->shininess = FLT2FIX(0.2);
	p->specularColor.red = FLT2FIX(0);
	p->specularColor.green = FLT2FIX(0);
	p->specularColor.blue = FLT2FIX(0);
	p->transparency = FLT2FIX(0);
	p->reffunc.vals = malloc(sizeof(SFFloat)*1);
	p->reffunc.count = 1;
	p->reffunc.vals[0] = FLT2FIX(0);
	p->transfunc.vals = malloc(sizeof(SFFloat)*1);
	p->transfunc.count = 1;
	p->transfunc.vals[0] = FLT2FIX(1);
	p->refFrequency.vals = malloc(sizeof(SFFloat)*1);
	p->refFrequency.count = 1;
	p->refFrequency.vals[0] = FLT2FIX(0);
	p->transFrequency.vals = malloc(sizeof(SFFloat)*1);
	p->transFrequency.count = 1;
	p->transFrequency.vals[0] = FLT2FIX(0);
	return (GF_Node *)p;
}


/*
	AcousticScene Node deletion
*/

static void AcousticScene_Del(GF_Node *node)
{
	M_AcousticScene *p = (M_AcousticScene *) node;
	gf_sg_mftime_del(p->reverbTime);
	gf_sg_mffloat_del(p->reverbFreq);
	gf_node_free((GF_Node *) p);
}

static const u16 AcousticScene_Def2All[] = { 0, 1, 2, 3, 4, 5};
static const u16 AcousticScene_In2All[] = { 4, 5};
static const u16 AcousticScene_Out2All[] = { 4, 5};
static const u16 AcousticScene_Dyn2All[] = { 4};

static u32 AcousticScene_get_field_count(GF_Node *node, u8 IndexMode)
{
	switch(IndexMode) {
	case GF_SG_FIELD_CODING_IN: return 2;
	case GF_SG_FIELD_CODING_DEF: return 6;
	case GF_SG_FIELD_CODING_OUT: return 2;
	case GF_SG_FIELD_CODING_DYN: return 1;
	default:
		return 6;
	}
}

static GF_Err AcousticScene_get_field_index(GF_Node *n, u32 inField, u8 IndexMode, u32 *allField)
{
	switch(IndexMode) {
	case GF_SG_FIELD_CODING_IN:
		*allField = AcousticScene_In2All[inField];
		return GF_OK;
	case GF_SG_FIELD_CODING_DEF:
		*allField = AcousticScene_Def2All[inField];
		return GF_OK;
	case GF_SG_FIELD_CODING_OUT:
		*allField = AcousticScene_Out2All[inField];
		return GF_OK;
	case GF_SG_FIELD_CODING_DYN:
		*allField = AcousticScene_Dyn2All[inField];
		return GF_OK;
	default:
		return GF_BAD_PARAM;
	}
}
static GF_Err AcousticScene_get_field(GF_Node *node, GF_FieldInfo *info)
{
	switch (info->fieldIndex) {
	case 0:
		info->name = "center";
		info->eventType = GF_SG_EVENT_FIELD;
		info->fieldType = GF_SG_VRML_SFVEC3F;
		info->far_ptr = & ((M_AcousticScene *) node)->center;
		return GF_OK;
	case 1:
		info->name = "Size";
		info->eventType = GF_SG_EVENT_FIELD;
		info->fieldType = GF_SG_VRML_SFVEC3F;
		info->far_ptr = & ((M_AcousticScene *) node)->Size;
		return GF_OK;
	case 2:
		info->name = "reverbTime";
		info->eventType = GF_SG_EVENT_FIELD;
		info->fieldType = GF_SG_VRML_MFTIME;
		info->far_ptr = & ((M_AcousticScene *) node)->reverbTime;
		return GF_OK;
	case 3:
		info->name = "reverbFreq";
		info->eventType = GF_SG_EVENT_FIELD;
		info->fieldType = GF_SG_VRML_MFFLOAT;
		info->far_ptr = & ((M_AcousticScene *) node)->reverbFreq;
		return GF_OK;
	case 4:
		info->name = "reverbLevel";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFFLOAT;
		info->far_ptr = & ((M_AcousticScene *) node)->reverbLevel;
		return GF_OK;
	case 5:
		info->name = "reverbDelay";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFTIME;
		info->far_ptr = & ((M_AcousticScene *) node)->reverbDelay;
		return GF_OK;
	default:
		return GF_BAD_PARAM;
	}
}

static Bool AcousticScene_get_aq_info(GF_Node *n, u32 FieldIndex, u8 *QType, u8 *AType, Fixed *b_min, Fixed *b_max, u32 *QT13_bits)
{
	switch (FieldIndex) {
	case 0:
		*AType = 0;
		*QType = 1;
		*b_min = FIX_MIN;
		*b_max = FIX_MAX;
		return 1;
	case 1:
		*AType = 0;
		*QType = 11;
		*b_min = FIX_MIN;
		*b_max = FIX_MAX;
		return 1;
	case 2:
		*AType = 0;
		*QType = 0;
		*b_min = FLT2FIX(0 );
		*b_max = FIX_MAX;
		return 1;
	case 3:
		*AType = 0;
		*QType = 0;
		*b_min = FLT2FIX(0 );
		*b_max = FIX_MAX;
		return 1;
	case 4:
		*AType = 7;
		*QType = 0;
		*b_min = FLT2FIX(0 );
		*b_max = FIX_MAX;
		return 1;
	case 5:
		*AType = 0;
		*QType = 0;
		*b_min = FLT2FIX(0 );
		*b_max = FIX_MAX;
		return 1;
	default:
		return 0;
	}
}



GF_Node *AcousticScene_Create()
{
	M_AcousticScene *p;
	GF_SAFEALLOC(p, sizeof(M_AcousticScene));
	if(!p) return NULL;
	gf_node_setup((GF_Node *)p, TAG_MPEG4_AcousticScene);

#ifdef GF_NODE_USE_POINTERS
	((GF_Node *)p)->sgprivate->name = "AcousticScene";
	((GF_Node *)p)->sgprivate->node_del = AcousticScene_Del;
	((GF_Node *)p)->sgprivate->get_field_count = AcousticScene_get_field_count;
	((GF_Node *)p)->sgprivate->get_field = AcousticScene_get_field;
#endif


	/*default field values*/
	p->center.x = FLT2FIX(0);
	p->center.y = FLT2FIX(0);
	p->center.z = FLT2FIX(0);
	p->Size.x = FLT2FIX(-1);
	p->Size.y = FLT2FIX(-1);
	p->Size.z = FLT2FIX(-1);
	p->reverbTime.vals = malloc(sizeof(SFTime)*1);
	p->reverbTime.count = 1;
	p->reverbTime.vals[0] = 0;
	p->reverbFreq.vals = malloc(sizeof(SFFloat)*1);
	p->reverbFreq.count = 1;
	p->reverbFreq.vals[0] = FLT2FIX(1000);
	p->reverbLevel = FLT2FIX(0.4);
	p->reverbDelay = 0.5;
	return (GF_Node *)p;
}


/*
	ApplicationWindow Node deletion
*/

static void ApplicationWindow_Del(GF_Node *node)
{
	M_ApplicationWindow *p = (M_ApplicationWindow *) node;
	gf_sg_sfstring_del(p->description);
	gf_sg_mfstring_del(p->parameter);
	gf_sg_mfurl_del(p->url);
	gf_node_free((GF_Node *) p);
}

static const u16 ApplicationWindow_Def2All[] = { 0, 1, 2, 3, 4, 5, 6};
static const u16 ApplicationWindow_In2All[] = { 0, 1, 2, 3, 4, 5, 6};
static const u16 ApplicationWindow_Out2All[] = { 0, 1, 2, 3, 4, 5, 6};
static const u16 ApplicationWindow_Dyn2All[] = { 6};

static u32 ApplicationWindow_get_field_count(GF_Node *node, u8 IndexMode)
{
	switch(IndexMode) {
	case GF_SG_FIELD_CODING_IN: return 7;
	case GF_SG_FIELD_CODING_DEF: return 7;
	case GF_SG_FIELD_CODING_OUT: return 7;
	case GF_SG_FIELD_CODING_DYN: return 1;
	default:
		return 7;
	}
}

static GF_Err ApplicationWindow_get_field_index(GF_Node *n, u32 inField, u8 IndexMode, u32 *allField)
{
	switch(IndexMode) {
	case GF_SG_FIELD_CODING_IN:
		*allField = ApplicationWindow_In2All[inField];
		return GF_OK;
	case GF_SG_FIELD_CODING_DEF:
		*allField = ApplicationWindow_Def2All[inField];
		return GF_OK;
	case GF_SG_FIELD_CODING_OUT:
		*allField = ApplicationWindow_Out2All[inField];
		return GF_OK;
	case GF_SG_FIELD_CODING_DYN:
		*allField = ApplicationWindow_Dyn2All[inField];
		return GF_OK;
	default:
		return GF_BAD_PARAM;
	}
}
static GF_Err ApplicationWindow_get_field(GF_Node *node, GF_FieldInfo *info)
{
	switch (info->fieldIndex) {
	case 0:
		info->name = "isActive";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFBOOL;
		info->far_ptr = & ((M_ApplicationWindow *) node)->isActive;
		return GF_OK;
	case 1:
		info->name = "startTime";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFTIME;
		info->far_ptr = & ((M_ApplicationWindow *) node)->startTime;
		return GF_OK;
	case 2:
		info->name = "stopTime";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFTIME;
		info->far_ptr = & ((M_ApplicationWindow *) node)->stopTime;
		return GF_OK;
	case 3:
		info->name = "description";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFSTRING;
		info->far_ptr = & ((M_ApplicationWindow *) node)->description;
		return GF_OK;
	case 4:
		info->name = "parameter";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_MFSTRING;
		info->far_ptr = & ((M_ApplicationWindow *) node)->parameter;
		return GF_OK;
	case 5:
		info->name = "url";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_MFURL;
		info->far_ptr = & ((M_ApplicationWindow *) node)->url;
		return GF_OK;
	case 6:
		info->name = "size";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFVEC2F;
		info->far_ptr = & ((M_ApplicationWindow *) node)->size;
		return GF_OK;
	default:
		return GF_BAD_PARAM;
	}
}

static Bool ApplicationWindow_get_aq_info(GF_Node *n, u32 FieldIndex, u8 *QType, u8 *AType, Fixed *b_min, Fixed *b_max, u32 *QT13_bits)
{
	switch (FieldIndex) {
	case 1:
		*AType = 0;
		*QType = 0;
		*b_min = FIX_MIN;
		*b_max = FIX_MAX;
		return 1;
	case 2:
		*AType = 0;
		*QType = 0;
		*b_min = FIX_MIN;
		*b_max = FIX_MAX;
		return 1;
	case 6:
		*AType = 12;
		*QType = 12;
		*b_min = FIX_MIN;
		*b_max = FIX_MAX;
		return 1;
	default:
		return 0;
	}
}



GF_Node *ApplicationWindow_Create()
{
	M_ApplicationWindow *p;
	GF_SAFEALLOC(p, sizeof(M_ApplicationWindow));
	if(!p) return NULL;
	gf_node_setup((GF_Node *)p, TAG_MPEG4_ApplicationWindow);

#ifdef GF_NODE_USE_POINTERS
	((GF_Node *)p)->sgprivate->name = "ApplicationWindow";
	((GF_Node *)p)->sgprivate->node_del = ApplicationWindow_Del;
	((GF_Node *)p)->sgprivate->get_field_count = ApplicationWindow_get_field_count;
	((GF_Node *)p)->sgprivate->get_field = ApplicationWindow_get_field;
#endif


	/*default field values*/
	p->startTime = 0;
	p->stopTime = 0;
	p->size.x = FLT2FIX(0);
	p->size.y = FLT2FIX(0);
	return (GF_Node *)p;
}


/*
	DirectiveSound Node deletion
*/

static void DirectiveSound_Del(GF_Node *node)
{
	M_DirectiveSound *p = (M_DirectiveSound *) node;
	gf_node_unregister((GF_Node *) p->source, (GF_Node *) p);	
	gf_node_unregister((GF_Node *) p->perceptualParameters, (GF_Node *) p);	
	gf_sg_mffloat_del(p->directivity);
	gf_sg_mffloat_del(p->angles);
	gf_sg_mffloat_del(p->frequency);
	gf_node_free((GF_Node *) p);
}

static const u16 DirectiveSound_Def2All[] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12};
static const u16 DirectiveSound_In2All[] = { 0, 1, 2, 3, 4, 5, 6};
static const u16 DirectiveSound_Out2All[] = { 0, 1, 2, 3, 4, 5, 6};
static const u16 DirectiveSound_Dyn2All[] = { 0, 1, 2};

static u32 DirectiveSound_get_field_count(GF_Node *node, u8 IndexMode)
{
	switch(IndexMode) {
	case GF_SG_FIELD_CODING_IN: return 7;
	case GF_SG_FIELD_CODING_DEF: return 13;
	case GF_SG_FIELD_CODING_OUT: return 7;
	case GF_SG_FIELD_CODING_DYN: return 3;
	default:
		return 13;
	}
}

static GF_Err DirectiveSound_get_field_index(GF_Node *n, u32 inField, u8 IndexMode, u32 *allField)
{
	switch(IndexMode) {
	case GF_SG_FIELD_CODING_IN:
		*allField = DirectiveSound_In2All[inField];
		return GF_OK;
	case GF_SG_FIELD_CODING_DEF:
		*allField = DirectiveSound_Def2All[inField];
		return GF_OK;
	case GF_SG_FIELD_CODING_OUT:
		*allField = DirectiveSound_Out2All[inField];
		return GF_OK;
	case GF_SG_FIELD_CODING_DYN:
		*allField = DirectiveSound_Dyn2All[inField];
		return GF_OK;
	default:
		return GF_BAD_PARAM;
	}
}
static GF_Err DirectiveSound_get_field(GF_Node *node, GF_FieldInfo *info)
{
	switch (info->fieldIndex) {
	case 0:
		info->name = "direction";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFVEC3F;
		info->far_ptr = & ((M_DirectiveSound *) node)->direction;
		return GF_OK;
	case 1:
		info->name = "intensity";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFFLOAT;
		info->far_ptr = & ((M_DirectiveSound *) node)->intensity;
		return GF_OK;
	case 2:
		info->name = "location";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFVEC3F;
		info->far_ptr = & ((M_DirectiveSound *) node)->location;
		return GF_OK;
	case 3:
		info->name = "source";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFNODE;
		info->NDTtype = NDT_SFAudioNode;
		info->far_ptr = & ((M_DirectiveSound *)node)->source;
		return GF_OK;
	case 4:
		info->name = "perceptualParameters";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFNODE;
		info->NDTtype = NDT_SFPerceptualParameterNode;
		info->far_ptr = & ((M_DirectiveSound *)node)->perceptualParameters;
		return GF_OK;
	case 5:
		info->name = "roomEffect";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFBOOL;
		info->far_ptr = & ((M_DirectiveSound *) node)->roomEffect;
		return GF_OK;
	case 6:
		info->name = "spatialize";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFBOOL;
		info->far_ptr = & ((M_DirectiveSound *) node)->spatialize;
		return GF_OK;
	case 7:
		info->name = "directivity";
		info->eventType = GF_SG_EVENT_FIELD;
		info->fieldType = GF_SG_VRML_MFFLOAT;
		info->far_ptr = & ((M_DirectiveSound *) node)->directivity;
		return GF_OK;
	case 8:
		info->name = "angles";
		info->eventType = GF_SG_EVENT_FIELD;
		info->fieldType = GF_SG_VRML_MFFLOAT;
		info->far_ptr = & ((M_DirectiveSound *) node)->angles;
		return GF_OK;
	case 9:
		info->name = "frequency";
		info->eventType = GF_SG_EVENT_FIELD;
		info->fieldType = GF_SG_VRML_MFFLOAT;
		info->far_ptr = & ((M_DirectiveSound *) node)->frequency;
		return GF_OK;
	case 10:
		info->name = "speedOfSound";
		info->eventType = GF_SG_EVENT_FIELD;
		info->fieldType = GF_SG_VRML_SFFLOAT;
		info->far_ptr = & ((M_DirectiveSound *) node)->speedOfSound;
		return GF_OK;
	case 11:
		info->name = "distance";
		info->eventType = GF_SG_EVENT_FIELD;
		info->fieldType = GF_SG_VRML_SFFLOAT;
		info->far_ptr = & ((M_DirectiveSound *) node)->distance;
		return GF_OK;
	case 12:
		info->name = "useAirabs";
		info->eventType = GF_SG_EVENT_FIELD;
		info->fieldType = GF_SG_VRML_SFBOOL;
		info->far_ptr = & ((M_DirectiveSound *) node)->useAirabs;
		return GF_OK;
	default:
		return GF_BAD_PARAM;
	}
}

static Bool DirectiveSound_get_aq_info(GF_Node *n, u32 FieldIndex, u8 *QType, u8 *AType, Fixed *b_min, Fixed *b_max, u32 *QT13_bits)
{
	switch (FieldIndex) {
	case 0:
		*AType = 9;
		*QType = 9;
		*b_min = FIX_MIN;
		*b_max = FIX_MAX;
		return 1;
	case 1:
		*AType = 7;
		*QType = 0;
		*b_min = FLT2FIX(0);
		*b_max = FIX_MAX;
		return 1;
	case 2:
		*AType = 1;
		*QType = 1;
		*b_min = FIX_MIN;
		*b_max = FIX_MAX;
		return 1;
	case 7:
		*AType = 0;
		*QType = 0;
		*b_min = FIX_MIN;
		*b_max = FIX_MAX;
		return 1;
	case 8:
		*AType = 0;
		*QType = 6;
		*b_min = FLT2FIX(0);
		*b_max = FLT2FIX(3.14159265);
		return 1;
	case 9:
		*AType = 0;
		*QType = 0;
		*b_min = FLT2FIX(0);
		*b_max = FIX_MAX;
		return 1;
	case 10:
		*AType = 0;
		*QType = 1;
		*b_min = FLT2FIX(0);
		*b_max = FIX_MAX;
		return 1;
	case 11:
		*AType = 0;
		*QType = 0;
		*b_min = FLT2FIX(0);
		*b_max = FIX_MAX;
		return 1;
	default:
		return 0;
	}
}



GF_Node *DirectiveSound_Create()
{
	M_DirectiveSound *p;
	GF_SAFEALLOC(p, sizeof(M_DirectiveSound));
	if(!p) return NULL;
	gf_node_setup((GF_Node *)p, TAG_MPEG4_DirectiveSound);

#ifdef GF_NODE_USE_POINTERS
	((GF_Node *)p)->sgprivate->name = "DirectiveSound";
	((GF_Node *)p)->sgprivate->node_del = DirectiveSound_Del;
	((GF_Node *)p)->sgprivate->get_field_count = DirectiveSound_get_field_count;
	((GF_Node *)p)->sgprivate->get_field = DirectiveSound_get_field;
#endif


	/*default field values*/
	p->direction.x = FLT2FIX(0);
	p->direction.y = FLT2FIX(0);
	p->direction.z = FLT2FIX(-1);
	p->intensity = FLT2FIX(1);
	p->location.x = FLT2FIX(0);
	p->location.y = FLT2FIX(0);
	p->location.z = FLT2FIX(0);
	p->spatialize = 1;
	p->directivity.vals = malloc(sizeof(SFFloat)*1);
	p->directivity.count = 1;
	p->directivity.vals[0] = FLT2FIX(1);
	p->angles.vals = malloc(sizeof(SFFloat)*1);
	p->angles.count = 1;
	p->angles.vals[0] = FLT2FIX(1);
	p->speedOfSound = FLT2FIX(340);
	p->distance = FLT2FIX(100);
	return (GF_Node *)p;
}


/*
	Hierarchical3DMesh Node deletion
*/

static void Hierarchical3DMesh_Del(GF_Node *node)
{
	M_Hierarchical3DMesh *p = (M_Hierarchical3DMesh *) node;
	gf_sg_mfurl_del(p->url);
	gf_node_free((GF_Node *) p);
}

static const u16 Hierarchical3DMesh_Def2All[] = { 1, 2};
static const u16 Hierarchical3DMesh_In2All[] = { 0, 1};
static const u16 Hierarchical3DMesh_Out2All[] = { 1, 3};

static u32 Hierarchical3DMesh_get_field_count(GF_Node *node, u8 IndexMode)
{
	switch(IndexMode) {
	case GF_SG_FIELD_CODING_IN: return 2;
	case GF_SG_FIELD_CODING_DEF: return 2;
	case GF_SG_FIELD_CODING_OUT: return 2;
	case GF_SG_FIELD_CODING_DYN: return 0;
	default:
		return 4;
	}
}

static GF_Err Hierarchical3DMesh_get_field_index(GF_Node *n, u32 inField, u8 IndexMode, u32 *allField)
{
	switch(IndexMode) {
	case GF_SG_FIELD_CODING_IN:
		*allField = Hierarchical3DMesh_In2All[inField];
		return GF_OK;
	case GF_SG_FIELD_CODING_DEF:
		*allField = Hierarchical3DMesh_Def2All[inField];
		return GF_OK;
	case GF_SG_FIELD_CODING_OUT:
		*allField = Hierarchical3DMesh_Out2All[inField];
		return GF_OK;
	default:
		return GF_BAD_PARAM;
	}
}
static GF_Err Hierarchical3DMesh_get_field(GF_Node *node, GF_FieldInfo *info)
{
	switch (info->fieldIndex) {
	case 0:
		info->name = "triangleBudget";
		info->eventType = GF_SG_EVENT_IN;
		info->on_event_in = ((M_Hierarchical3DMesh *)node)->on_triangleBudget;
		info->fieldType = GF_SG_VRML_SFINT32;
		info->far_ptr = & ((M_Hierarchical3DMesh *) node)->triangleBudget;
		return GF_OK;
	case 1:
		info->name = "level";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFFLOAT;
		info->far_ptr = & ((M_Hierarchical3DMesh *) node)->level;
		return GF_OK;
	case 2:
		info->name = "url";
		info->eventType = GF_SG_EVENT_FIELD;
		info->fieldType = GF_SG_VRML_MFURL;
		info->far_ptr = & ((M_Hierarchical3DMesh *) node)->url;
		return GF_OK;
	case 3:
		info->name = "doneLoading";
		info->eventType = GF_SG_EVENT_OUT;
		info->fieldType = GF_SG_VRML_SFBOOL;
		info->far_ptr = & ((M_Hierarchical3DMesh *) node)->doneLoading;
		return GF_OK;
	default:
		return GF_BAD_PARAM;
	}
}

static Bool Hierarchical3DMesh_get_aq_info(GF_Node *n, u32 FieldIndex, u8 *QType, u8 *AType, Fixed *b_min, Fixed *b_max, u32 *QT13_bits)
{
	switch (FieldIndex) {
	case 0:
		*AType = 0;
		*QType = 0;
		*b_min = FLT2FIX(-1);
		*b_max = FIX_MAX;
		return 1;
	case 1:
		*AType = 0;
		*QType = 0;
		*b_min = FLT2FIX(-1);
		*b_max = FIX_MAX;
		return 1;
	default:
		return 0;
	}
}



GF_Node *Hierarchical3DMesh_Create()
{
	M_Hierarchical3DMesh *p;
	GF_SAFEALLOC(p, sizeof(M_Hierarchical3DMesh));
	if(!p) return NULL;
	gf_node_setup((GF_Node *)p, TAG_MPEG4_Hierarchical3DMesh);

#ifdef GF_NODE_USE_POINTERS
	((GF_Node *)p)->sgprivate->name = "Hierarchical3DMesh";
	((GF_Node *)p)->sgprivate->node_del = Hierarchical3DMesh_Del;
	((GF_Node *)p)->sgprivate->get_field_count = Hierarchical3DMesh_get_field_count;
	((GF_Node *)p)->sgprivate->get_field = Hierarchical3DMesh_get_field;
#endif


	/*default field values*/
	p->level = FLT2FIX(1);
	return (GF_Node *)p;
}


/*
	MaterialKey Node deletion
*/

static void MaterialKey_Del(GF_Node *node)
{
	M_MaterialKey *p = (M_MaterialKey *) node;
	gf_node_free((GF_Node *) p);
}

static const u16 MaterialKey_Def2All[] = { 0, 1, 2, 3, 4, 5};
static const u16 MaterialKey_In2All[] = { 0, 1, 2, 3, 4, 5};
static const u16 MaterialKey_Out2All[] = { 0, 1, 2, 3, 4, 5};
static const u16 MaterialKey_Dyn2All[] = { 2, 3, 4, 5};

static u32 MaterialKey_get_field_count(GF_Node *node, u8 IndexMode)
{
	switch(IndexMode) {
	case GF_SG_FIELD_CODING_IN: return 6;
	case GF_SG_FIELD_CODING_DEF: return 6;
	case GF_SG_FIELD_CODING_OUT: return 6;
	case GF_SG_FIELD_CODING_DYN: return 4;
	default:
		return 6;
	}
}

static GF_Err MaterialKey_get_field_index(GF_Node *n, u32 inField, u8 IndexMode, u32 *allField)
{
	switch(IndexMode) {
	case GF_SG_FIELD_CODING_IN:
		*allField = MaterialKey_In2All[inField];
		return GF_OK;
	case GF_SG_FIELD_CODING_DEF:
		*allField = MaterialKey_Def2All[inField];
		return GF_OK;
	case GF_SG_FIELD_CODING_OUT:
		*allField = MaterialKey_Out2All[inField];
		return GF_OK;
	case GF_SG_FIELD_CODING_DYN:
		*allField = MaterialKey_Dyn2All[inField];
		return GF_OK;
	default:
		return GF_BAD_PARAM;
	}
}
static GF_Err MaterialKey_get_field(GF_Node *node, GF_FieldInfo *info)
{
	switch (info->fieldIndex) {
	case 0:
		info->name = "isKeyed";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFBOOL;
		info->far_ptr = & ((M_MaterialKey *) node)->isKeyed;
		return GF_OK;
	case 1:
		info->name = "isRGB";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFBOOL;
		info->far_ptr = & ((M_MaterialKey *) node)->isRGB;
		return GF_OK;
	case 2:
		info->name = "keyColor";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFCOLOR;
		info->far_ptr = & ((M_MaterialKey *) node)->keyColor;
		return GF_OK;
	case 3:
		info->name = "lowThreshold";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFFLOAT;
		info->far_ptr = & ((M_MaterialKey *) node)->lowThreshold;
		return GF_OK;
	case 4:
		info->name = "highThreshold";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFFLOAT;
		info->far_ptr = & ((M_MaterialKey *) node)->highThreshold;
		return GF_OK;
	case 5:
		info->name = "transparency";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFFLOAT;
		info->far_ptr = & ((M_MaterialKey *) node)->transparency;
		return GF_OK;
	default:
		return GF_BAD_PARAM;
	}
}

static Bool MaterialKey_get_aq_info(GF_Node *n, u32 FieldIndex, u8 *QType, u8 *AType, Fixed *b_min, Fixed *b_max, u32 *QT13_bits)
{
	switch (FieldIndex) {
	case 2:
		*AType = 4;
		*QType = 4;
		*b_min = FLT2FIX(0);
		*b_max = FLT2FIX(1);
		return 1;
	case 3:
		*AType = 8;
		*QType = 4;
		*b_min = FLT2FIX(0);
		*b_max = FLT2FIX(1);
		return 1;
	case 4:
		*AType = 8;
		*QType = 4;
		*b_min = FLT2FIX(0);
		*b_max = FLT2FIX(1);
		return 1;
	case 5:
		*AType = 8;
		*QType = 4;
		*b_min = FLT2FIX(0);
		*b_max = FLT2FIX(1);
		return 1;
	default:
		return 0;
	}
}



GF_Node *MaterialKey_Create()
{
	M_MaterialKey *p;
	GF_SAFEALLOC(p, sizeof(M_MaterialKey));
	if(!p) return NULL;
	gf_node_setup((GF_Node *)p, TAG_MPEG4_MaterialKey);

#ifdef GF_NODE_USE_POINTERS
	((GF_Node *)p)->sgprivate->name = "MaterialKey";
	((GF_Node *)p)->sgprivate->node_del = MaterialKey_Del;
	((GF_Node *)p)->sgprivate->get_field_count = MaterialKey_get_field_count;
	((GF_Node *)p)->sgprivate->get_field = MaterialKey_get_field;
#endif


	/*default field values*/
	p->isKeyed = 1;
	p->isRGB = 1;
	p->keyColor.red = FLT2FIX(0);
	p->keyColor.green = FLT2FIX(0);
	p->keyColor.blue = FLT2FIX(0);
	p->lowThreshold = FLT2FIX(0);
	p->highThreshold = FLT2FIX(0);
	p->transparency = FLT2FIX(0);
	return (GF_Node *)p;
}


/*
	PerceptualParameters Node deletion
*/

static void PerceptualParameters_Del(GF_Node *node)
{
	M_PerceptualParameters *p = (M_PerceptualParameters *) node;
	gf_sg_mffloat_del(p->omniDirectivity);
	gf_sg_mffloat_del(p->directFilterGains);
	gf_sg_mffloat_del(p->inputFilterGains);
	gf_node_free((GF_Node *) p);
}

static const u16 PerceptualParameters_Def2All[] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18};
static const u16 PerceptualParameters_In2All[] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18};
static const u16 PerceptualParameters_Out2All[] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18};
static const u16 PerceptualParameters_Dyn2All[] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14};

static u32 PerceptualParameters_get_field_count(GF_Node *node, u8 IndexMode)
{
	switch(IndexMode) {
	case GF_SG_FIELD_CODING_IN: return 19;
	case GF_SG_FIELD_CODING_DEF: return 19;
	case GF_SG_FIELD_CODING_OUT: return 19;
	case GF_SG_FIELD_CODING_DYN: return 15;
	default:
		return 19;
	}
}

static GF_Err PerceptualParameters_get_field_index(GF_Node *n, u32 inField, u8 IndexMode, u32 *allField)
{
	switch(IndexMode) {
	case GF_SG_FIELD_CODING_IN:
		*allField = PerceptualParameters_In2All[inField];
		return GF_OK;
	case GF_SG_FIELD_CODING_DEF:
		*allField = PerceptualParameters_Def2All[inField];
		return GF_OK;
	case GF_SG_FIELD_CODING_OUT:
		*allField = PerceptualParameters_Out2All[inField];
		return GF_OK;
	case GF_SG_FIELD_CODING_DYN:
		*allField = PerceptualParameters_Dyn2All[inField];
		return GF_OK;
	default:
		return GF_BAD_PARAM;
	}
}
static GF_Err PerceptualParameters_get_field(GF_Node *node, GF_FieldInfo *info)
{
	switch (info->fieldIndex) {
	case 0:
		info->name = "sourcePresence";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFFLOAT;
		info->far_ptr = & ((M_PerceptualParameters *) node)->sourcePresence;
		return GF_OK;
	case 1:
		info->name = "sourceWarmth";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFFLOAT;
		info->far_ptr = & ((M_PerceptualParameters *) node)->sourceWarmth;
		return GF_OK;
	case 2:
		info->name = "sourceBrilliance";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFFLOAT;
		info->far_ptr = & ((M_PerceptualParameters *) node)->sourceBrilliance;
		return GF_OK;
	case 3:
		info->name = "roomPresence";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFFLOAT;
		info->far_ptr = & ((M_PerceptualParameters *) node)->roomPresence;
		return GF_OK;
	case 4:
		info->name = "runningReverberance";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFFLOAT;
		info->far_ptr = & ((M_PerceptualParameters *) node)->runningReverberance;
		return GF_OK;
	case 5:
		info->name = "envelopment";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFFLOAT;
		info->far_ptr = & ((M_PerceptualParameters *) node)->envelopment;
		return GF_OK;
	case 6:
		info->name = "lateReverberance";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFFLOAT;
		info->far_ptr = & ((M_PerceptualParameters *) node)->lateReverberance;
		return GF_OK;
	case 7:
		info->name = "heavyness";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFFLOAT;
		info->far_ptr = & ((M_PerceptualParameters *) node)->heavyness;
		return GF_OK;
	case 8:
		info->name = "liveness";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFFLOAT;
		info->far_ptr = & ((M_PerceptualParameters *) node)->liveness;
		return GF_OK;
	case 9:
		info->name = "omniDirectivity";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_MFFLOAT;
		info->far_ptr = & ((M_PerceptualParameters *) node)->omniDirectivity;
		return GF_OK;
	case 10:
		info->name = "directFilterGains";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_MFFLOAT;
		info->far_ptr = & ((M_PerceptualParameters *) node)->directFilterGains;
		return GF_OK;
	case 11:
		info->name = "inputFilterGains";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_MFFLOAT;
		info->far_ptr = & ((M_PerceptualParameters *) node)->inputFilterGains;
		return GF_OK;
	case 12:
		info->name = "refDistance";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFFLOAT;
		info->far_ptr = & ((M_PerceptualParameters *) node)->refDistance;
		return GF_OK;
	case 13:
		info->name = "freqLow";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFFLOAT;
		info->far_ptr = & ((M_PerceptualParameters *) node)->freqLow;
		return GF_OK;
	case 14:
		info->name = "freqHigh";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFFLOAT;
		info->far_ptr = & ((M_PerceptualParameters *) node)->freqHigh;
		return GF_OK;
	case 15:
		info->name = "timeLimit1";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFTIME;
		info->far_ptr = & ((M_PerceptualParameters *) node)->timeLimit1;
		return GF_OK;
	case 16:
		info->name = "timeLimit2";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFTIME;
		info->far_ptr = & ((M_PerceptualParameters *) node)->timeLimit2;
		return GF_OK;
	case 17:
		info->name = "timeLimit3";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFTIME;
		info->far_ptr = & ((M_PerceptualParameters *) node)->timeLimit3;
		return GF_OK;
	case 18:
		info->name = "modalDensity";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFTIME;
		info->far_ptr = & ((M_PerceptualParameters *) node)->modalDensity;
		return GF_OK;
	default:
		return GF_BAD_PARAM;
	}
}

static Bool PerceptualParameters_get_aq_info(GF_Node *n, u32 FieldIndex, u8 *QType, u8 *AType, Fixed *b_min, Fixed *b_max, u32 *QT13_bits)
{
	switch (FieldIndex) {
	case 0:
		*AType = 7;
		*QType = 0;
		*b_min = FIX_MIN;
		*b_max = FIX_MAX;
		return 1;
	case 1:
		*AType = 7;
		*QType = 0;
		*b_min = FIX_MIN;
		*b_max = FIX_MAX;
		return 1;
	case 2:
		*AType = 7;
		*QType = 0;
		*b_min = FIX_MIN;
		*b_max = FIX_MAX;
		return 1;
	case 3:
		*AType = 7;
		*QType = 0;
		*b_min = FIX_MIN;
		*b_max = FIX_MAX;
		return 1;
	case 4:
		*AType = 7;
		*QType = 0;
		*b_min = FIX_MIN;
		*b_max = FIX_MAX;
		return 1;
	case 5:
		*AType = 7;
		*QType = 0;
		*b_min = FIX_MIN;
		*b_max = FIX_MAX;
		return 1;
	case 6:
		*AType = 7;
		*QType = 0;
		*b_min = FIX_MIN;
		*b_max = FIX_MAX;
		return 1;
	case 7:
		*AType = 7;
		*QType = 0;
		*b_min = FIX_MIN;
		*b_max = FIX_MAX;
		return 1;
	case 8:
		*AType = 7;
		*QType = 0;
		*b_min = FIX_MIN;
		*b_max = FIX_MAX;
		return 1;
	case 9:
		*AType = 7;
		*QType = 0;
		*b_min = FIX_MIN;
		*b_max = FIX_MAX;
		return 1;
	case 10:
		*AType = 7;
		*QType = 0;
		*b_min = FIX_MIN;
		*b_max = FIX_MAX;
		return 1;
	case 11:
		*AType = 7;
		*QType = 0;
		*b_min = FIX_MIN;
		*b_max = FIX_MAX;
		return 1;
	case 12:
		*AType = 7;
		*QType = 0;
		*b_min = FIX_MIN;
		*b_max = FIX_MAX;
		return 1;
	case 13:
		*AType = 7;
		*QType = 0;
		*b_min = FIX_MIN;
		*b_max = FIX_MAX;
		return 1;
	case 14:
		*AType = 7;
		*QType = 0;
		*b_min = FIX_MIN;
		*b_max = FIX_MAX;
		return 1;
	case 15:
		*AType = 0;
		*QType = 0;
		*b_min = FIX_MIN;
		*b_max = FIX_MAX;
		return 1;
	case 16:
		*AType = 0;
		*QType = 0;
		*b_min = FIX_MIN;
		*b_max = FIX_MAX;
		return 1;
	case 17:
		*AType = 0;
		*QType = 0;
		*b_min = FIX_MIN;
		*b_max = FIX_MAX;
		return 1;
	case 18:
		*AType = 0;
		*QType = 0;
		*b_min = FIX_MIN;
		*b_max = FIX_MAX;
		return 1;
	default:
		return 0;
	}
}



GF_Node *PerceptualParameters_Create()
{
	M_PerceptualParameters *p;
	GF_SAFEALLOC(p, sizeof(M_PerceptualParameters));
	if(!p) return NULL;
	gf_node_setup((GF_Node *)p, TAG_MPEG4_PerceptualParameters);

#ifdef GF_NODE_USE_POINTERS
	((GF_Node *)p)->sgprivate->name = "PerceptualParameters";
	((GF_Node *)p)->sgprivate->node_del = PerceptualParameters_Del;
	((GF_Node *)p)->sgprivate->get_field_count = PerceptualParameters_get_field_count;
	((GF_Node *)p)->sgprivate->get_field = PerceptualParameters_get_field;
#endif


	/*default field values*/
	p->sourcePresence = FLT2FIX(1.0);
	p->sourceWarmth = FLT2FIX(1.0);
	p->sourceBrilliance = FLT2FIX(1.0);
	p->roomPresence = FLT2FIX(1.0);
	p->runningReverberance = FLT2FIX(1.0);
	p->envelopment = FLT2FIX(0.0);
	p->lateReverberance = FLT2FIX(1.0);
	p->heavyness = FLT2FIX(1.0);
	p->liveness = FLT2FIX(1.0);
	p->omniDirectivity.vals = malloc(sizeof(SFFloat)*1);
	p->omniDirectivity.count = 1;
	p->omniDirectivity.vals[0] = FLT2FIX(1.0);
	p->directFilterGains.vals = malloc(sizeof(SFFloat)*3);
	p->directFilterGains.count = 3;
	p->directFilterGains.vals[0] = FLT2FIX(1.0);
	p->directFilterGains.vals[1] = FLT2FIX(1.0);
	p->directFilterGains.vals[2] = FLT2FIX(1.0);
	p->inputFilterGains.vals = malloc(sizeof(SFFloat)*3);
	p->inputFilterGains.count = 3;
	p->inputFilterGains.vals[0] = FLT2FIX(1.0);
	p->inputFilterGains.vals[1] = FLT2FIX(1.0);
	p->inputFilterGains.vals[2] = FLT2FIX(1.0);
	p->refDistance = FLT2FIX(1.0);
	p->freqLow = FLT2FIX(250.0);
	p->freqHigh = FLT2FIX(4000.0);
	p->timeLimit1 = 0.02;
	p->timeLimit2 = 0.04;
	p->timeLimit3 = 0.1;
	p->modalDensity = 0.8;
	return (GF_Node *)p;
}


/*
	TemporalTransform Node deletion
*/

static void TemporalTransform_Del(GF_Node *node)
{
	M_TemporalTransform *p = (M_TemporalTransform *) node;
	gf_sg_vrml_parent_reset((GF_Node *) p);	
	gf_sg_mfurl_del(p->url);
	gf_sg_mfint32_del(p->stretchMode);
	gf_sg_mfint32_del(p->shrinkMode);
	gf_node_free((GF_Node *) p);
}

static const u16 TemporalTransform_Def2All[] = { 2, 3, 4, 5, 6, 7, 8, 9, 10, 11};
static const u16 TemporalTransform_In2All[] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11};
static const u16 TemporalTransform_Out2All[] = { 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12};
static const u16 TemporalTransform_Dyn2All[] = { 7, 8};

static u32 TemporalTransform_get_field_count(GF_Node *node, u8 IndexMode)
{
	switch(IndexMode) {
	case GF_SG_FIELD_CODING_IN: return 12;
	case GF_SG_FIELD_CODING_DEF: return 10;
	case GF_SG_FIELD_CODING_OUT: return 11;
	case GF_SG_FIELD_CODING_DYN: return 2;
	default:
		return 13;
	}
}

static GF_Err TemporalTransform_get_field_index(GF_Node *n, u32 inField, u8 IndexMode, u32 *allField)
{
	switch(IndexMode) {
	case GF_SG_FIELD_CODING_IN:
		*allField = TemporalTransform_In2All[inField];
		return GF_OK;
	case GF_SG_FIELD_CODING_DEF:
		*allField = TemporalTransform_Def2All[inField];
		return GF_OK;
	case GF_SG_FIELD_CODING_OUT:
		*allField = TemporalTransform_Out2All[inField];
		return GF_OK;
	case GF_SG_FIELD_CODING_DYN:
		*allField = TemporalTransform_Dyn2All[inField];
		return GF_OK;
	default:
		return GF_BAD_PARAM;
	}
}
static GF_Err TemporalTransform_get_field(GF_Node *node, GF_FieldInfo *info)
{
	switch (info->fieldIndex) {
	case 0:
		info->name = "addChildren";
		info->eventType = GF_SG_EVENT_IN;
		info->on_event_in = ((M_TemporalTransform *)node)->on_addChildren;
		info->fieldType = GF_SG_VRML_MFNODE;
		info->NDTtype = NDT_SF3DNode;
		info->far_ptr = & ((M_TemporalTransform *)node)->addChildren;
		return GF_OK;
	case 1:
		info->name = "removeChildren";
		info->eventType = GF_SG_EVENT_IN;
		info->on_event_in = ((M_TemporalTransform *)node)->on_removeChildren;
		info->fieldType = GF_SG_VRML_MFNODE;
		info->NDTtype = NDT_SF3DNode;
		info->far_ptr = & ((M_TemporalTransform *)node)->removeChildren;
		return GF_OK;
	case 2:
		info->name = "children";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_MFNODE;
		info->NDTtype = NDT_SF3DNode;
		info->far_ptr = & ((M_TemporalTransform *)node)->children;
		return GF_OK;
	case 3:
		info->name = "url";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_MFURL;
		info->far_ptr = & ((M_TemporalTransform *) node)->url;
		return GF_OK;
	case 4:
		info->name = "startTime";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFTIME;
		info->far_ptr = & ((M_TemporalTransform *) node)->startTime;
		return GF_OK;
	case 5:
		info->name = "optimalDuration";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFTIME;
		info->far_ptr = & ((M_TemporalTransform *) node)->optimalDuration;
		return GF_OK;
	case 6:
		info->name = "active";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFBOOL;
		info->far_ptr = & ((M_TemporalTransform *) node)->active;
		return GF_OK;
	case 7:
		info->name = "speed";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFFLOAT;
		info->far_ptr = & ((M_TemporalTransform *) node)->speed;
		return GF_OK;
	case 8:
		info->name = "scalability";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFVEC2F;
		info->far_ptr = & ((M_TemporalTransform *) node)->scalability;
		return GF_OK;
	case 9:
		info->name = "stretchMode";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_MFINT32;
		info->far_ptr = & ((M_TemporalTransform *) node)->stretchMode;
		return GF_OK;
	case 10:
		info->name = "shrinkMode";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_MFINT32;
		info->far_ptr = & ((M_TemporalTransform *) node)->shrinkMode;
		return GF_OK;
	case 11:
		info->name = "maxDelay";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFTIME;
		info->far_ptr = & ((M_TemporalTransform *) node)->maxDelay;
		return GF_OK;
	case 12:
		info->name = "actualDuration";
		info->eventType = GF_SG_EVENT_OUT;
		info->fieldType = GF_SG_VRML_SFTIME;
		info->far_ptr = & ((M_TemporalTransform *) node)->actualDuration;
		return GF_OK;
	default:
		return GF_BAD_PARAM;
	}
}

static Bool TemporalTransform_get_aq_info(GF_Node *n, u32 FieldIndex, u8 *QType, u8 *AType, Fixed *b_min, Fixed *b_max, u32 *QT13_bits)
{
	switch (FieldIndex) {
	case 7:
		*AType = 7;
		*QType = 0;
		*b_min = FIX_MIN;
		*b_max = FIX_MAX;
		return 1;
	case 8:
		*AType = 12;
		*QType = 12;
		*b_min = FLT2FIX(-1);
		*b_max = FIX_MAX;
		return 1;
	case 9:
		*AType = 0;
		*QType = 13;
		*QT13_bits = 2;
		*b_min = FLT2FIX(0);
		*b_max = FLT2FIX(2);
		return 1;
	case 10:
		*AType = 0;
		*QType = 13;
		*QT13_bits = 1;
		*b_min = FLT2FIX(0);
		*b_max = FLT2FIX(1);
		return 1;
	default:
		return 0;
	}
}



GF_Node *TemporalTransform_Create()
{
	M_TemporalTransform *p;
	GF_SAFEALLOC(p, sizeof(M_TemporalTransform));
	if(!p) return NULL;
	gf_node_setup((GF_Node *)p, TAG_MPEG4_TemporalTransform);
	gf_sg_vrml_parent_setup((GF_Node *) p);

#ifdef GF_NODE_USE_POINTERS
	((GF_Node *)p)->sgprivate->name = "TemporalTransform";
	((GF_Node *)p)->sgprivate->node_del = TemporalTransform_Del;
	((GF_Node *)p)->sgprivate->get_field_count = TemporalTransform_get_field_count;
	((GF_Node *)p)->sgprivate->get_field = TemporalTransform_get_field;
#endif


	/*default field values*/
	p->startTime = -1.0;
	p->optimalDuration = -1.0;
	p->speed = FLT2FIX(1.0);
	p->scalability.x = FLT2FIX(1.0);
	p->scalability.y = FLT2FIX(1.0);
	p->stretchMode.vals = malloc(sizeof(SFInt32)*1);
	p->stretchMode.count = 1;
	p->stretchMode.vals[0] = 0;
	p->shrinkMode.vals = malloc(sizeof(SFInt32)*1);
	p->shrinkMode.count = 1;
	p->shrinkMode.vals[0] = 0;
	p->maxDelay = 0;
	return (GF_Node *)p;
}


/*
	TemporalGroup Node deletion
*/

static void TemporalGroup_Del(GF_Node *node)
{
	M_TemporalGroup *p = (M_TemporalGroup *) node;
	gf_sg_vrml_parent_reset((GF_Node *) p);	
	gf_sg_mffloat_del(p->priority);
	gf_node_free((GF_Node *) p);
}

static const u16 TemporalGroup_Def2All[] = { 2, 3, 4, 5, 6};
static const u16 TemporalGroup_In2All[] = { 0, 1, 2, 6};
static const u16 TemporalGroup_Out2All[] = { 2, 6, 7, 8};

static u32 TemporalGroup_get_field_count(GF_Node *node, u8 IndexMode)
{
	switch(IndexMode) {
	case GF_SG_FIELD_CODING_IN: return 4;
	case GF_SG_FIELD_CODING_DEF: return 5;
	case GF_SG_FIELD_CODING_OUT: return 4;
	case GF_SG_FIELD_CODING_DYN: return 0;
	default:
		return 9;
	}
}

static GF_Err TemporalGroup_get_field_index(GF_Node *n, u32 inField, u8 IndexMode, u32 *allField)
{
	switch(IndexMode) {
	case GF_SG_FIELD_CODING_IN:
		*allField = TemporalGroup_In2All[inField];
		return GF_OK;
	case GF_SG_FIELD_CODING_DEF:
		*allField = TemporalGroup_Def2All[inField];
		return GF_OK;
	case GF_SG_FIELD_CODING_OUT:
		*allField = TemporalGroup_Out2All[inField];
		return GF_OK;
	default:
		return GF_BAD_PARAM;
	}
}
static GF_Err TemporalGroup_get_field(GF_Node *node, GF_FieldInfo *info)
{
	switch (info->fieldIndex) {
	case 0:
		info->name = "addChildren";
		info->eventType = GF_SG_EVENT_IN;
		info->on_event_in = ((M_TemporalGroup *)node)->on_addChildren;
		info->fieldType = GF_SG_VRML_MFNODE;
		info->NDTtype = NDT_SFTemporalNode;
		info->far_ptr = & ((M_TemporalGroup *)node)->addChildren;
		return GF_OK;
	case 1:
		info->name = "removeChildren";
		info->eventType = GF_SG_EVENT_IN;
		info->on_event_in = ((M_TemporalGroup *)node)->on_removeChildren;
		info->fieldType = GF_SG_VRML_MFNODE;
		info->NDTtype = NDT_SFTemporalNode;
		info->far_ptr = & ((M_TemporalGroup *)node)->removeChildren;
		return GF_OK;
	case 2:
		info->name = "children";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_MFNODE;
		info->NDTtype = NDT_SFTemporalNode;
		info->far_ptr = & ((M_TemporalGroup *)node)->children;
		return GF_OK;
	case 3:
		info->name = "costart";
		info->eventType = GF_SG_EVENT_FIELD;
		info->fieldType = GF_SG_VRML_SFBOOL;
		info->far_ptr = & ((M_TemporalGroup *) node)->costart;
		return GF_OK;
	case 4:
		info->name = "coend";
		info->eventType = GF_SG_EVENT_FIELD;
		info->fieldType = GF_SG_VRML_SFBOOL;
		info->far_ptr = & ((M_TemporalGroup *) node)->coend;
		return GF_OK;
	case 5:
		info->name = "meet";
		info->eventType = GF_SG_EVENT_FIELD;
		info->fieldType = GF_SG_VRML_SFBOOL;
		info->far_ptr = & ((M_TemporalGroup *) node)->meet;
		return GF_OK;
	case 6:
		info->name = "priority";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_MFFLOAT;
		info->far_ptr = & ((M_TemporalGroup *) node)->priority;
		return GF_OK;
	case 7:
		info->name = "isActive";
		info->eventType = GF_SG_EVENT_OUT;
		info->fieldType = GF_SG_VRML_SFBOOL;
		info->far_ptr = & ((M_TemporalGroup *) node)->isActive;
		return GF_OK;
	case 8:
		info->name = "activeChild";
		info->eventType = GF_SG_EVENT_OUT;
		info->fieldType = GF_SG_VRML_SFINT32;
		info->far_ptr = & ((M_TemporalGroup *) node)->activeChild;
		return GF_OK;
	default:
		return GF_BAD_PARAM;
	}
}

static Bool TemporalGroup_get_aq_info(GF_Node *n, u32 FieldIndex, u8 *QType, u8 *AType, Fixed *b_min, Fixed *b_max, u32 *QT13_bits)
{
	switch (FieldIndex) {
	case 6:
		*AType = 0;
		*QType = 3;
		*b_min = FLT2FIX(0);
		*b_max = FIX_MAX;
		return 1;
	default:
		return 0;
	}
}



GF_Node *TemporalGroup_Create()
{
	M_TemporalGroup *p;
	GF_SAFEALLOC(p, sizeof(M_TemporalGroup));
	if(!p) return NULL;
	gf_node_setup((GF_Node *)p, TAG_MPEG4_TemporalGroup);
	gf_sg_vrml_parent_setup((GF_Node *) p);

#ifdef GF_NODE_USE_POINTERS
	((GF_Node *)p)->sgprivate->name = "TemporalGroup";
	((GF_Node *)p)->sgprivate->node_del = TemporalGroup_Del;
	((GF_Node *)p)->sgprivate->get_field_count = TemporalGroup_get_field_count;
	((GF_Node *)p)->sgprivate->get_field = TemporalGroup_get_field;
#endif


	/*default field values*/
	p->costart = 1;
	return (GF_Node *)p;
}


/*
	ServerCommand Node deletion
*/

static void ServerCommand_Del(GF_Node *node)
{
	M_ServerCommand *p = (M_ServerCommand *) node;
	gf_sg_mfurl_del(p->url);
	gf_sg_sfstring_del(p->command);
	gf_node_free((GF_Node *) p);
}

static const u16 ServerCommand_Def2All[] = { 1, 2, 3};
static const u16 ServerCommand_In2All[] = { 0, 1, 2, 3};
static const u16 ServerCommand_Out2All[] = { 1, 2, 3};

static u32 ServerCommand_get_field_count(GF_Node *node, u8 IndexMode)
{
	switch(IndexMode) {
	case GF_SG_FIELD_CODING_IN: return 4;
	case GF_SG_FIELD_CODING_DEF: return 3;
	case GF_SG_FIELD_CODING_OUT: return 3;
	case GF_SG_FIELD_CODING_DYN: return 0;
	default:
		return 4;
	}
}

static GF_Err ServerCommand_get_field_index(GF_Node *n, u32 inField, u8 IndexMode, u32 *allField)
{
	switch(IndexMode) {
	case GF_SG_FIELD_CODING_IN:
		*allField = ServerCommand_In2All[inField];
		return GF_OK;
	case GF_SG_FIELD_CODING_DEF:
		*allField = ServerCommand_Def2All[inField];
		return GF_OK;
	case GF_SG_FIELD_CODING_OUT:
		*allField = ServerCommand_Out2All[inField];
		return GF_OK;
	default:
		return GF_BAD_PARAM;
	}
}
static GF_Err ServerCommand_get_field(GF_Node *node, GF_FieldInfo *info)
{
	switch (info->fieldIndex) {
	case 0:
		info->name = "trigger";
		info->eventType = GF_SG_EVENT_IN;
		info->on_event_in = ((M_ServerCommand *)node)->on_trigger;
		info->fieldType = GF_SG_VRML_SFBOOL;
		info->far_ptr = & ((M_ServerCommand *) node)->trigger;
		return GF_OK;
	case 1:
		info->name = "enable";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFBOOL;
		info->far_ptr = & ((M_ServerCommand *) node)->enable;
		return GF_OK;
	case 2:
		info->name = "url";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_MFURL;
		info->far_ptr = & ((M_ServerCommand *) node)->url;
		return GF_OK;
	case 3:
		info->name = "command";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFSTRING;
		info->far_ptr = & ((M_ServerCommand *) node)->command;
		return GF_OK;
	default:
		return GF_BAD_PARAM;
	}
}

static Bool ServerCommand_get_aq_info(GF_Node *n, u32 FieldIndex, u8 *QType, u8 *AType, Fixed *b_min, Fixed *b_max, u32 *QT13_bits)
{
	switch (FieldIndex) {
	default:
		return 0;
	}
}



GF_Node *ServerCommand_Create()
{
	M_ServerCommand *p;
	GF_SAFEALLOC(p, sizeof(M_ServerCommand));
	if(!p) return NULL;
	gf_node_setup((GF_Node *)p, TAG_MPEG4_ServerCommand);

#ifdef GF_NODE_USE_POINTERS
	((GF_Node *)p)->sgprivate->name = "ServerCommand";
	((GF_Node *)p)->sgprivate->node_del = ServerCommand_Del;
	((GF_Node *)p)->sgprivate->get_field_count = ServerCommand_get_field_count;
	((GF_Node *)p)->sgprivate->get_field = ServerCommand_get_field;
#endif


	/*default field values*/
	return (GF_Node *)p;
}


/*
	InputSensor Node deletion
*/

static void InputSensor_Del(GF_Node *node)
{
	M_InputSensor *p = (M_InputSensor *) node;
	gf_sg_sfcommand_del(p->buffer);
	gf_sg_mfurl_del(p->url);
	gf_node_free((GF_Node *) p);
}

static const u16 InputSensor_Def2All[] = { 0, 1, 2};
static const u16 InputSensor_In2All[] = { 0, 1, 2};
static const u16 InputSensor_Out2All[] = { 0, 1, 2, 3};

static u32 InputSensor_get_field_count(GF_Node *node, u8 IndexMode)
{
	switch(IndexMode) {
	case GF_SG_FIELD_CODING_IN: return 3;
	case GF_SG_FIELD_CODING_DEF: return 3;
	case GF_SG_FIELD_CODING_OUT: return 4;
	case GF_SG_FIELD_CODING_DYN: return 0;
	default:
		return 4;
	}
}

static GF_Err InputSensor_get_field_index(GF_Node *n, u32 inField, u8 IndexMode, u32 *allField)
{
	switch(IndexMode) {
	case GF_SG_FIELD_CODING_IN:
		*allField = InputSensor_In2All[inField];
		return GF_OK;
	case GF_SG_FIELD_CODING_DEF:
		*allField = InputSensor_Def2All[inField];
		return GF_OK;
	case GF_SG_FIELD_CODING_OUT:
		*allField = InputSensor_Out2All[inField];
		return GF_OK;
	default:
		return GF_BAD_PARAM;
	}
}
static GF_Err InputSensor_get_field(GF_Node *node, GF_FieldInfo *info)
{
	switch (info->fieldIndex) {
	case 0:
		info->name = "enabled";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFBOOL;
		info->far_ptr = & ((M_InputSensor *) node)->enabled;
		return GF_OK;
	case 1:
		info->name = "buffer";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFCOMMANDBUFFER;
		info->far_ptr = & ((M_InputSensor *) node)->buffer;
		return GF_OK;
	case 2:
		info->name = "url";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_MFURL;
		info->far_ptr = & ((M_InputSensor *) node)->url;
		return GF_OK;
	case 3:
		info->name = "eventTime";
		info->eventType = GF_SG_EVENT_OUT;
		info->fieldType = GF_SG_VRML_SFTIME;
		info->far_ptr = & ((M_InputSensor *) node)->eventTime;
		return GF_OK;
	default:
		return GF_BAD_PARAM;
	}
}

static Bool InputSensor_get_aq_info(GF_Node *n, u32 FieldIndex, u8 *QType, u8 *AType, Fixed *b_min, Fixed *b_max, u32 *QT13_bits)
{
	switch (FieldIndex) {
	default:
		return 0;
	}
}



GF_Node *InputSensor_Create()
{
	M_InputSensor *p;
	GF_SAFEALLOC(p, sizeof(M_InputSensor));
	if(!p) return NULL;
	gf_node_setup((GF_Node *)p, TAG_MPEG4_InputSensor);
	p->buffer.commandList = gf_list_new();	

#ifdef GF_NODE_USE_POINTERS
	((GF_Node *)p)->sgprivate->name = "InputSensor";
	((GF_Node *)p)->sgprivate->node_del = InputSensor_Del;
	((GF_Node *)p)->sgprivate->get_field_count = InputSensor_get_field_count;
	((GF_Node *)p)->sgprivate->get_field = InputSensor_get_field;
#endif


	/*default field values*/
	p->enabled = 1;
	return (GF_Node *)p;
}


/*
	MatteTexture Node deletion
*/

static void MatteTexture_Del(GF_Node *node)
{
	M_MatteTexture *p = (M_MatteTexture *) node;
	gf_node_unregister((GF_Node *) p->surfaceA, (GF_Node *) p);	
	gf_node_unregister((GF_Node *) p->surfaceB, (GF_Node *) p);	
	gf_node_unregister((GF_Node *) p->alphaSurface, (GF_Node *) p);	
	gf_sg_sfstring_del(p->operation);
	gf_sg_mffloat_del(p->parameter);
	gf_node_free((GF_Node *) p);
}

static const u16 MatteTexture_Def2All[] = { 0, 1, 2, 3, 4, 5, 6};
static const u16 MatteTexture_In2All[] = { 3, 5, 6};
static const u16 MatteTexture_Out2All[] = { 3, 5, 6};

static u32 MatteTexture_get_field_count(GF_Node *node, u8 IndexMode)
{
	switch(IndexMode) {
	case GF_SG_FIELD_CODING_IN: return 3;
	case GF_SG_FIELD_CODING_DEF: return 7;
	case GF_SG_FIELD_CODING_OUT: return 3;
	case GF_SG_FIELD_CODING_DYN: return 0;
	default:
		return 7;
	}
}

static GF_Err MatteTexture_get_field_index(GF_Node *n, u32 inField, u8 IndexMode, u32 *allField)
{
	switch(IndexMode) {
	case GF_SG_FIELD_CODING_IN:
		*allField = MatteTexture_In2All[inField];
		return GF_OK;
	case GF_SG_FIELD_CODING_DEF:
		*allField = MatteTexture_Def2All[inField];
		return GF_OK;
	case GF_SG_FIELD_CODING_OUT:
		*allField = MatteTexture_Out2All[inField];
		return GF_OK;
	default:
		return GF_BAD_PARAM;
	}
}
static GF_Err MatteTexture_get_field(GF_Node *node, GF_FieldInfo *info)
{
	switch (info->fieldIndex) {
	case 0:
		info->name = "surfaceA";
		info->eventType = GF_SG_EVENT_FIELD;
		info->fieldType = GF_SG_VRML_SFNODE;
		info->NDTtype = NDT_SFTextureNode;
		info->far_ptr = & ((M_MatteTexture *)node)->surfaceA;
		return GF_OK;
	case 1:
		info->name = "surfaceB";
		info->eventType = GF_SG_EVENT_FIELD;
		info->fieldType = GF_SG_VRML_SFNODE;
		info->NDTtype = NDT_SFTextureNode;
		info->far_ptr = & ((M_MatteTexture *)node)->surfaceB;
		return GF_OK;
	case 2:
		info->name = "alphaSurface";
		info->eventType = GF_SG_EVENT_FIELD;
		info->fieldType = GF_SG_VRML_SFNODE;
		info->NDTtype = NDT_SFTextureNode;
		info->far_ptr = & ((M_MatteTexture *)node)->alphaSurface;
		return GF_OK;
	case 3:
		info->name = "operation";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFSTRING;
		info->far_ptr = & ((M_MatteTexture *) node)->operation;
		return GF_OK;
	case 4:
		info->name = "overwrite";
		info->eventType = GF_SG_EVENT_FIELD;
		info->fieldType = GF_SG_VRML_SFBOOL;
		info->far_ptr = & ((M_MatteTexture *) node)->overwrite;
		return GF_OK;
	case 5:
		info->name = "fraction";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFFLOAT;
		info->far_ptr = & ((M_MatteTexture *) node)->fraction;
		return GF_OK;
	case 6:
		info->name = "parameter";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_MFFLOAT;
		info->far_ptr = & ((M_MatteTexture *) node)->parameter;
		return GF_OK;
	default:
		return GF_BAD_PARAM;
	}
}

static Bool MatteTexture_get_aq_info(GF_Node *n, u32 FieldIndex, u8 *QType, u8 *AType, Fixed *b_min, Fixed *b_max, u32 *QT13_bits)
{
	switch (FieldIndex) {
	default:
		return 0;
	}
}



GF_Node *MatteTexture_Create()
{
	M_MatteTexture *p;
	GF_SAFEALLOC(p, sizeof(M_MatteTexture));
	if(!p) return NULL;
	gf_node_setup((GF_Node *)p, TAG_MPEG4_MatteTexture);

#ifdef GF_NODE_USE_POINTERS
	((GF_Node *)p)->sgprivate->name = "MatteTexture";
	((GF_Node *)p)->sgprivate->node_del = MatteTexture_Del;
	((GF_Node *)p)->sgprivate->get_field_count = MatteTexture_get_field_count;
	((GF_Node *)p)->sgprivate->get_field = MatteTexture_get_field;
#endif


	/*default field values*/
	p->fraction = FLT2FIX(0);
	p->parameter.vals = malloc(sizeof(SFFloat)*1);
	p->parameter.count = 1;
	p->parameter.vals[0] = FLT2FIX(0);
	return (GF_Node *)p;
}


/*
	MediaBuffer Node deletion
*/

static void MediaBuffer_Del(GF_Node *node)
{
	M_MediaBuffer *p = (M_MediaBuffer *) node;
	gf_sg_mfurl_del(p->url);
	gf_node_free((GF_Node *) p);
}

static const u16 MediaBuffer_Def2All[] = { 0, 1, 2, 3, 5};
static const u16 MediaBuffer_In2All[] = { 0, 1, 2, 3, 5};
static const u16 MediaBuffer_Out2All[] = { 0, 1, 2, 3, 4, 5};

static u32 MediaBuffer_get_field_count(GF_Node *node, u8 IndexMode)
{
	switch(IndexMode) {
	case GF_SG_FIELD_CODING_IN: return 5;
	case GF_SG_FIELD_CODING_DEF: return 5;
	case GF_SG_FIELD_CODING_OUT: return 6;
	case GF_SG_FIELD_CODING_DYN: return 0;
	default:
		return 6;
	}
}

static GF_Err MediaBuffer_get_field_index(GF_Node *n, u32 inField, u8 IndexMode, u32 *allField)
{
	switch(IndexMode) {
	case GF_SG_FIELD_CODING_IN:
		*allField = MediaBuffer_In2All[inField];
		return GF_OK;
	case GF_SG_FIELD_CODING_DEF:
		*allField = MediaBuffer_Def2All[inField];
		return GF_OK;
	case GF_SG_FIELD_CODING_OUT:
		*allField = MediaBuffer_Out2All[inField];
		return GF_OK;
	default:
		return GF_BAD_PARAM;
	}
}
static GF_Err MediaBuffer_get_field(GF_Node *node, GF_FieldInfo *info)
{
	switch (info->fieldIndex) {
	case 0:
		info->name = "bufferSize";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFFLOAT;
		info->far_ptr = & ((M_MediaBuffer *) node)->bufferSize;
		return GF_OK;
	case 1:
		info->name = "url";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_MFURL;
		info->far_ptr = & ((M_MediaBuffer *) node)->url;
		return GF_OK;
	case 2:
		info->name = "mediaStartTime";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFTIME;
		info->far_ptr = & ((M_MediaBuffer *) node)->mediaStartTime;
		return GF_OK;
	case 3:
		info->name = "mediaStopTime";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFTIME;
		info->far_ptr = & ((M_MediaBuffer *) node)->mediaStopTime;
		return GF_OK;
	case 4:
		info->name = "isBuffered";
		info->eventType = GF_SG_EVENT_OUT;
		info->fieldType = GF_SG_VRML_SFBOOL;
		info->far_ptr = & ((M_MediaBuffer *) node)->isBuffered;
		return GF_OK;
	case 5:
		info->name = "enabled";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFBOOL;
		info->far_ptr = & ((M_MediaBuffer *) node)->enabled;
		return GF_OK;
	default:
		return GF_BAD_PARAM;
	}
}

static Bool MediaBuffer_get_aq_info(GF_Node *n, u32 FieldIndex, u8 *QType, u8 *AType, Fixed *b_min, Fixed *b_max, u32 *QT13_bits)
{
	switch (FieldIndex) {
	default:
		return 0;
	}
}



GF_Node *MediaBuffer_Create()
{
	M_MediaBuffer *p;
	GF_SAFEALLOC(p, sizeof(M_MediaBuffer));
	if(!p) return NULL;
	gf_node_setup((GF_Node *)p, TAG_MPEG4_MediaBuffer);

#ifdef GF_NODE_USE_POINTERS
	((GF_Node *)p)->sgprivate->name = "MediaBuffer";
	((GF_Node *)p)->sgprivate->node_del = MediaBuffer_Del;
	((GF_Node *)p)->sgprivate->get_field_count = MediaBuffer_get_field_count;
	((GF_Node *)p)->sgprivate->get_field = MediaBuffer_get_field;
#endif


	/*default field values*/
	p->bufferSize = FLT2FIX(0.0);
	p->mediaStartTime = -1;
	p->mediaStopTime = FIX_MAX;
	p->enabled = 1;
	return (GF_Node *)p;
}


/*
	MediaControl Node deletion
*/

static void MediaControl_Del(GF_Node *node)
{
	M_MediaControl *p = (M_MediaControl *) node;
	gf_sg_mfurl_del(p->url);
	gf_node_free((GF_Node *) p);
}

static const u16 MediaControl_Def2All[] = { 0, 1, 2, 3, 4, 5, 6, 7};
static const u16 MediaControl_In2All[] = { 0, 1, 2, 3, 4, 5, 6, 7};
static const u16 MediaControl_Out2All[] = { 0, 1, 2, 3, 4, 5, 6, 7, 8};

static u32 MediaControl_get_field_count(GF_Node *node, u8 IndexMode)
{
	switch(IndexMode) {
	case GF_SG_FIELD_CODING_IN: return 8;
	case GF_SG_FIELD_CODING_DEF: return 8;
	case GF_SG_FIELD_CODING_OUT: return 9;
	case GF_SG_FIELD_CODING_DYN: return 0;
	default:
		return 9;
	}
}

static GF_Err MediaControl_get_field_index(GF_Node *n, u32 inField, u8 IndexMode, u32 *allField)
{
	switch(IndexMode) {
	case GF_SG_FIELD_CODING_IN:
		*allField = MediaControl_In2All[inField];
		return GF_OK;
	case GF_SG_FIELD_CODING_DEF:
		*allField = MediaControl_Def2All[inField];
		return GF_OK;
	case GF_SG_FIELD_CODING_OUT:
		*allField = MediaControl_Out2All[inField];
		return GF_OK;
	default:
		return GF_BAD_PARAM;
	}
}
static GF_Err MediaControl_get_field(GF_Node *node, GF_FieldInfo *info)
{
	switch (info->fieldIndex) {
	case 0:
		info->name = "url";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_MFURL;
		info->far_ptr = & ((M_MediaControl *) node)->url;
		return GF_OK;
	case 1:
		info->name = "mediaStartTime";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFTIME;
		info->far_ptr = & ((M_MediaControl *) node)->mediaStartTime;
		return GF_OK;
	case 2:
		info->name = "mediaStopTime";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFTIME;
		info->far_ptr = & ((M_MediaControl *) node)->mediaStopTime;
		return GF_OK;
	case 3:
		info->name = "mediaSpeed";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFFLOAT;
		info->far_ptr = & ((M_MediaControl *) node)->mediaSpeed;
		return GF_OK;
	case 4:
		info->name = "loop";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFBOOL;
		info->far_ptr = & ((M_MediaControl *) node)->loop;
		return GF_OK;
	case 5:
		info->name = "preRoll";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFBOOL;
		info->far_ptr = & ((M_MediaControl *) node)->preRoll;
		return GF_OK;
	case 6:
		info->name = "mute";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFBOOL;
		info->far_ptr = & ((M_MediaControl *) node)->mute;
		return GF_OK;
	case 7:
		info->name = "enabled";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFBOOL;
		info->far_ptr = & ((M_MediaControl *) node)->enabled;
		return GF_OK;
	case 8:
		info->name = "isPreRolled";
		info->eventType = GF_SG_EVENT_OUT;
		info->fieldType = GF_SG_VRML_SFBOOL;
		info->far_ptr = & ((M_MediaControl *) node)->isPreRolled;
		return GF_OK;
	default:
		return GF_BAD_PARAM;
	}
}

static Bool MediaControl_get_aq_info(GF_Node *n, u32 FieldIndex, u8 *QType, u8 *AType, Fixed *b_min, Fixed *b_max, u32 *QT13_bits)
{
	switch (FieldIndex) {
	default:
		return 0;
	}
}



GF_Node *MediaControl_Create()
{
	M_MediaControl *p;
	GF_SAFEALLOC(p, sizeof(M_MediaControl));
	if(!p) return NULL;
	gf_node_setup((GF_Node *)p, TAG_MPEG4_MediaControl);

#ifdef GF_NODE_USE_POINTERS
	((GF_Node *)p)->sgprivate->name = "MediaControl";
	((GF_Node *)p)->sgprivate->node_del = MediaControl_Del;
	((GF_Node *)p)->sgprivate->get_field_count = MediaControl_get_field_count;
	((GF_Node *)p)->sgprivate->get_field = MediaControl_get_field;
#endif


	/*default field values*/
	p->mediaStartTime = -1;
	p->mediaStopTime = FIX_MAX;
	p->mediaSpeed = FLT2FIX(1.0);
	p->preRoll = 1;
	p->enabled = 1;
	return (GF_Node *)p;
}


/*
	MediaSensor Node deletion
*/

static void MediaSensor_Del(GF_Node *node)
{
	M_MediaSensor *p = (M_MediaSensor *) node;
	gf_sg_mfurl_del(p->url);
	gf_sg_mfstring_del(p->info);
	gf_node_free((GF_Node *) p);
}

static const u16 MediaSensor_Def2All[] = { 0};
static const u16 MediaSensor_In2All[] = { 0};
static const u16 MediaSensor_Out2All[] = { 0, 1, 2, 3, 4, 5};

static u32 MediaSensor_get_field_count(GF_Node *node, u8 IndexMode)
{
	switch(IndexMode) {
	case GF_SG_FIELD_CODING_IN: return 1;
	case GF_SG_FIELD_CODING_DEF: return 1;
	case GF_SG_FIELD_CODING_OUT: return 6;
	case GF_SG_FIELD_CODING_DYN: return 0;
	default:
		return 6;
	}
}

static GF_Err MediaSensor_get_field_index(GF_Node *n, u32 inField, u8 IndexMode, u32 *allField)
{
	switch(IndexMode) {
	case GF_SG_FIELD_CODING_IN:
		*allField = MediaSensor_In2All[inField];
		return GF_OK;
	case GF_SG_FIELD_CODING_DEF:
		*allField = MediaSensor_Def2All[inField];
		return GF_OK;
	case GF_SG_FIELD_CODING_OUT:
		*allField = MediaSensor_Out2All[inField];
		return GF_OK;
	default:
		return GF_BAD_PARAM;
	}
}
static GF_Err MediaSensor_get_field(GF_Node *node, GF_FieldInfo *info)
{
	switch (info->fieldIndex) {
	case 0:
		info->name = "url";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_MFURL;
		info->far_ptr = & ((M_MediaSensor *) node)->url;
		return GF_OK;
	case 1:
		info->name = "mediaCurrentTime";
		info->eventType = GF_SG_EVENT_OUT;
		info->fieldType = GF_SG_VRML_SFTIME;
		info->far_ptr = & ((M_MediaSensor *) node)->mediaCurrentTime;
		return GF_OK;
	case 2:
		info->name = "streamObjectStartTime";
		info->eventType = GF_SG_EVENT_OUT;
		info->fieldType = GF_SG_VRML_SFTIME;
		info->far_ptr = & ((M_MediaSensor *) node)->streamObjectStartTime;
		return GF_OK;
	case 3:
		info->name = "mediaDuration";
		info->eventType = GF_SG_EVENT_OUT;
		info->fieldType = GF_SG_VRML_SFTIME;
		info->far_ptr = & ((M_MediaSensor *) node)->mediaDuration;
		return GF_OK;
	case 4:
		info->name = "isActive";
		info->eventType = GF_SG_EVENT_OUT;
		info->fieldType = GF_SG_VRML_SFBOOL;
		info->far_ptr = & ((M_MediaSensor *) node)->isActive;
		return GF_OK;
	case 5:
		info->name = "info";
		info->eventType = GF_SG_EVENT_OUT;
		info->fieldType = GF_SG_VRML_MFSTRING;
		info->far_ptr = & ((M_MediaSensor *) node)->info;
		return GF_OK;
	default:
		return GF_BAD_PARAM;
	}
}

static Bool MediaSensor_get_aq_info(GF_Node *n, u32 FieldIndex, u8 *QType, u8 *AType, Fixed *b_min, Fixed *b_max, u32 *QT13_bits)
{
	switch (FieldIndex) {
	default:
		return 0;
	}
}



GF_Node *MediaSensor_Create()
{
	M_MediaSensor *p;
	GF_SAFEALLOC(p, sizeof(M_MediaSensor));
	if(!p) return NULL;
	gf_node_setup((GF_Node *)p, TAG_MPEG4_MediaSensor);

#ifdef GF_NODE_USE_POINTERS
	((GF_Node *)p)->sgprivate->name = "MediaSensor";
	((GF_Node *)p)->sgprivate->node_del = MediaSensor_Del;
	((GF_Node *)p)->sgprivate->get_field_count = MediaSensor_get_field_count;
	((GF_Node *)p)->sgprivate->get_field = MediaSensor_get_field;
#endif


	/*default field values*/
	return (GF_Node *)p;
}


/*
	CoordinateInterpolator4D Node deletion
*/

static void CoordinateInterpolator4D_Del(GF_Node *node)
{
	M_CoordinateInterpolator4D *p = (M_CoordinateInterpolator4D *) node;
	gf_sg_mffloat_del(p->key);
	gf_sg_mfvec4f_del(p->keyValue);
	gf_sg_mfvec4f_del(p->value_changed);
	gf_node_free((GF_Node *) p);
}

static const u16 CoordinateInterpolator4D_Def2All[] = { 1, 2};
static const u16 CoordinateInterpolator4D_In2All[] = { 0, 1, 2};
static const u16 CoordinateInterpolator4D_Out2All[] = { 1, 2, 3};

static u32 CoordinateInterpolator4D_get_field_count(GF_Node *node, u8 IndexMode)
{
	switch(IndexMode) {
	case GF_SG_FIELD_CODING_IN: return 3;
	case GF_SG_FIELD_CODING_DEF: return 2;
	case GF_SG_FIELD_CODING_OUT: return 3;
	case GF_SG_FIELD_CODING_DYN: return 0;
	default:
		return 4;
	}
}

static GF_Err CoordinateInterpolator4D_get_field_index(GF_Node *n, u32 inField, u8 IndexMode, u32 *allField)
{
	switch(IndexMode) {
	case GF_SG_FIELD_CODING_IN:
		*allField = CoordinateInterpolator4D_In2All[inField];
		return GF_OK;
	case GF_SG_FIELD_CODING_DEF:
		*allField = CoordinateInterpolator4D_Def2All[inField];
		return GF_OK;
	case GF_SG_FIELD_CODING_OUT:
		*allField = CoordinateInterpolator4D_Out2All[inField];
		return GF_OK;
	default:
		return GF_BAD_PARAM;
	}
}
static GF_Err CoordinateInterpolator4D_get_field(GF_Node *node, GF_FieldInfo *info)
{
	switch (info->fieldIndex) {
	case 0:
		info->name = "set_fraction";
		info->eventType = GF_SG_EVENT_IN;
		info->on_event_in = ((M_CoordinateInterpolator4D *)node)->on_set_fraction;
		info->fieldType = GF_SG_VRML_SFFLOAT;
		info->far_ptr = & ((M_CoordinateInterpolator4D *) node)->set_fraction;
		return GF_OK;
	case 1:
		info->name = "key";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_MFFLOAT;
		info->far_ptr = & ((M_CoordinateInterpolator4D *) node)->key;
		return GF_OK;
	case 2:
		info->name = "keyValue";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_MFVEC4F;
		info->far_ptr = & ((M_CoordinateInterpolator4D *) node)->keyValue;
		return GF_OK;
	case 3:
		info->name = "value_changed";
		info->eventType = GF_SG_EVENT_OUT;
		info->fieldType = GF_SG_VRML_MFVEC4F;
		info->far_ptr = & ((M_CoordinateInterpolator4D *) node)->value_changed;
		return GF_OK;
	default:
		return GF_BAD_PARAM;
	}
}

static Bool CoordinateInterpolator4D_get_aq_info(GF_Node *n, u32 FieldIndex, u8 *QType, u8 *AType, Fixed *b_min, Fixed *b_max, u32 *QT13_bits)
{
	switch (FieldIndex) {
	case 1:
		*AType = 0;
		*QType = 8;
		*b_min = FLT2FIX(0);
		*b_max = FLT2FIX(1);
		return 1;
	case 2:
		*AType = 0;
		*QType = 15;
		*b_min = FIX_MIN;
		*b_max = FIX_MAX;
		return 1;
	default:
		return 0;
	}
}



GF_Node *CoordinateInterpolator4D_Create()
{
	M_CoordinateInterpolator4D *p;
	GF_SAFEALLOC(p, sizeof(M_CoordinateInterpolator4D));
	if(!p) return NULL;
	gf_node_setup((GF_Node *)p, TAG_MPEG4_CoordinateInterpolator4D);

#ifdef GF_NODE_USE_POINTERS
	((GF_Node *)p)->sgprivate->name = "CoordinateInterpolator4D";
	((GF_Node *)p)->sgprivate->node_del = CoordinateInterpolator4D_Del;
	((GF_Node *)p)->sgprivate->get_field_count = CoordinateInterpolator4D_get_field_count;
	((GF_Node *)p)->sgprivate->get_field = CoordinateInterpolator4D_get_field;
#endif


	/*default field values*/
	return (GF_Node *)p;
}


/*
	NonLinearDeformer Node deletion
*/

static void NonLinearDeformer_Del(GF_Node *node)
{
	M_NonLinearDeformer *p = (M_NonLinearDeformer *) node;
	gf_sg_mffloat_del(p->extend);
	gf_node_unregister((GF_Node *) p->geometry, (GF_Node *) p);	
	gf_node_free((GF_Node *) p);
}

static const u16 NonLinearDeformer_Def2All[] = { 0, 1, 2, 3, 4};
static const u16 NonLinearDeformer_In2All[] = { 0, 1, 2, 3, 4};
static const u16 NonLinearDeformer_Out2All[] = { 0, 1, 2, 3, 4};

static u32 NonLinearDeformer_get_field_count(GF_Node *node, u8 IndexMode)
{
	switch(IndexMode) {
	case GF_SG_FIELD_CODING_IN: return 5;
	case GF_SG_FIELD_CODING_DEF: return 5;
	case GF_SG_FIELD_CODING_OUT: return 5;
	case GF_SG_FIELD_CODING_DYN: return 0;
	default:
		return 5;
	}
}

static GF_Err NonLinearDeformer_get_field_index(GF_Node *n, u32 inField, u8 IndexMode, u32 *allField)
{
	switch(IndexMode) {
	case GF_SG_FIELD_CODING_IN:
		*allField = NonLinearDeformer_In2All[inField];
		return GF_OK;
	case GF_SG_FIELD_CODING_DEF:
		*allField = NonLinearDeformer_Def2All[inField];
		return GF_OK;
	case GF_SG_FIELD_CODING_OUT:
		*allField = NonLinearDeformer_Out2All[inField];
		return GF_OK;
	default:
		return GF_BAD_PARAM;
	}
}
static GF_Err NonLinearDeformer_get_field(GF_Node *node, GF_FieldInfo *info)
{
	switch (info->fieldIndex) {
	case 0:
		info->name = "axis";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFVEC3F;
		info->far_ptr = & ((M_NonLinearDeformer *) node)->axis;
		return GF_OK;
	case 1:
		info->name = "extend";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_MFFLOAT;
		info->far_ptr = & ((M_NonLinearDeformer *) node)->extend;
		return GF_OK;
	case 2:
		info->name = "geometry";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFNODE;
		info->NDTtype = NDT_SFGeometryNode;
		info->far_ptr = & ((M_NonLinearDeformer *)node)->geometry;
		return GF_OK;
	case 3:
		info->name = "param";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFFLOAT;
		info->far_ptr = & ((M_NonLinearDeformer *) node)->param;
		return GF_OK;
	case 4:
		info->name = "type";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFINT32;
		info->far_ptr = & ((M_NonLinearDeformer *) node)->type;
		return GF_OK;
	default:
		return GF_BAD_PARAM;
	}
}

static Bool NonLinearDeformer_get_aq_info(GF_Node *n, u32 FieldIndex, u8 *QType, u8 *AType, Fixed *b_min, Fixed *b_max, u32 *QT13_bits)
{
	switch (FieldIndex) {
	default:
		return 0;
	}
}



GF_Node *NonLinearDeformer_Create()
{
	M_NonLinearDeformer *p;
	GF_SAFEALLOC(p, sizeof(M_NonLinearDeformer));
	if(!p) return NULL;
	gf_node_setup((GF_Node *)p, TAG_MPEG4_NonLinearDeformer);

#ifdef GF_NODE_USE_POINTERS
	((GF_Node *)p)->sgprivate->name = "NonLinearDeformer";
	((GF_Node *)p)->sgprivate->node_del = NonLinearDeformer_Del;
	((GF_Node *)p)->sgprivate->get_field_count = NonLinearDeformer_get_field_count;
	((GF_Node *)p)->sgprivate->get_field = NonLinearDeformer_get_field;
#endif


	/*default field values*/
	p->axis.x = FLT2FIX(0);
	p->axis.y = FLT2FIX(0);
	p->axis.z = FLT2FIX(1);
	p->param = FLT2FIX(0);
	p->type = 0;
	return (GF_Node *)p;
}


/*
	PositionAnimator Node deletion
*/

static void PositionAnimator_Del(GF_Node *node)
{
	M_PositionAnimator *p = (M_PositionAnimator *) node;
	gf_sg_mffloat_del(p->key);
	gf_sg_mfrotation_del(p->keyOrientation);
	gf_sg_mfvec2f_del(p->keySpline);
	gf_sg_mfvec3f_del(p->keyValue);
	gf_sg_mffloat_del(p->weight);
	gf_node_free((GF_Node *) p);
}

static const u16 PositionAnimator_Def2All[] = { 1, 2, 3, 4, 5, 6, 7, 8, 9};
static const u16 PositionAnimator_In2All[] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9};
static const u16 PositionAnimator_Out2All[] = { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12};

static u32 PositionAnimator_get_field_count(GF_Node *node, u8 IndexMode)
{
	switch(IndexMode) {
	case GF_SG_FIELD_CODING_IN: return 10;
	case GF_SG_FIELD_CODING_DEF: return 9;
	case GF_SG_FIELD_CODING_OUT: return 12;
	case GF_SG_FIELD_CODING_DYN: return 0;
	default:
		return 13;
	}
}

static GF_Err PositionAnimator_get_field_index(GF_Node *n, u32 inField, u8 IndexMode, u32 *allField)
{
	switch(IndexMode) {
	case GF_SG_FIELD_CODING_IN:
		*allField = PositionAnimator_In2All[inField];
		return GF_OK;
	case GF_SG_FIELD_CODING_DEF:
		*allField = PositionAnimator_Def2All[inField];
		return GF_OK;
	case GF_SG_FIELD_CODING_OUT:
		*allField = PositionAnimator_Out2All[inField];
		return GF_OK;
	default:
		return GF_BAD_PARAM;
	}
}
static GF_Err PositionAnimator_get_field(GF_Node *node, GF_FieldInfo *info)
{
	switch (info->fieldIndex) {
	case 0:
		info->name = "set_fraction";
		info->eventType = GF_SG_EVENT_IN;
		info->on_event_in = ((M_PositionAnimator *)node)->on_set_fraction;
		info->fieldType = GF_SG_VRML_SFFLOAT;
		info->far_ptr = & ((M_PositionAnimator *) node)->set_fraction;
		return GF_OK;
	case 1:
		info->name = "fromTo";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFVEC2F;
		info->far_ptr = & ((M_PositionAnimator *) node)->fromTo;
		return GF_OK;
	case 2:
		info->name = "key";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_MFFLOAT;
		info->far_ptr = & ((M_PositionAnimator *) node)->key;
		return GF_OK;
	case 3:
		info->name = "keyOrientation";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_MFROTATION;
		info->far_ptr = & ((M_PositionAnimator *) node)->keyOrientation;
		return GF_OK;
	case 4:
		info->name = "keyType";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFINT32;
		info->far_ptr = & ((M_PositionAnimator *) node)->keyType;
		return GF_OK;
	case 5:
		info->name = "keySpline";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_MFVEC2F;
		info->far_ptr = & ((M_PositionAnimator *) node)->keySpline;
		return GF_OK;
	case 6:
		info->name = "keyValue";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_MFVEC3F;
		info->far_ptr = & ((M_PositionAnimator *) node)->keyValue;
		return GF_OK;
	case 7:
		info->name = "keyValueType";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFINT32;
		info->far_ptr = & ((M_PositionAnimator *) node)->keyValueType;
		return GF_OK;
	case 8:
		info->name = "offset";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFVEC3F;
		info->far_ptr = & ((M_PositionAnimator *) node)->offset;
		return GF_OK;
	case 9:
		info->name = "weight";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_MFFLOAT;
		info->far_ptr = & ((M_PositionAnimator *) node)->weight;
		return GF_OK;
	case 10:
		info->name = "endValue";
		info->eventType = GF_SG_EVENT_OUT;
		info->fieldType = GF_SG_VRML_SFVEC3F;
		info->far_ptr = & ((M_PositionAnimator *) node)->endValue;
		return GF_OK;
	case 11:
		info->name = "rotation_changed";
		info->eventType = GF_SG_EVENT_OUT;
		info->fieldType = GF_SG_VRML_SFROTATION;
		info->far_ptr = & ((M_PositionAnimator *) node)->rotation_changed;
		return GF_OK;
	case 12:
		info->name = "value_changed";
		info->eventType = GF_SG_EVENT_OUT;
		info->fieldType = GF_SG_VRML_SFVEC3F;
		info->far_ptr = & ((M_PositionAnimator *) node)->value_changed;
		return GF_OK;
	default:
		return GF_BAD_PARAM;
	}
}

static Bool PositionAnimator_get_aq_info(GF_Node *n, u32 FieldIndex, u8 *QType, u8 *AType, Fixed *b_min, Fixed *b_max, u32 *QT13_bits)
{
	switch (FieldIndex) {
	case 1:
		*AType = 0;
		*QType = 8;
		return 1;
	case 2:
		*AType = 0;
		*QType = 8;
		return 1;
	case 5:
		*AType = 0;
		*QType = 8;
		return 1;
	case 6:
		*AType = 0;
		*QType = 4;
		return 1;
	case 8:
		*AType = 0;
		*QType = 1;
		*b_min = FIX_MIN;
		*b_max = FIX_MAX;
		return 1;
	default:
		return 0;
	}
}



GF_Node *PositionAnimator_Create()
{
	M_PositionAnimator *p;
	GF_SAFEALLOC(p, sizeof(M_PositionAnimator));
	if(!p) return NULL;
	gf_node_setup((GF_Node *)p, TAG_MPEG4_PositionAnimator);

#ifdef GF_NODE_USE_POINTERS
	((GF_Node *)p)->sgprivate->name = "PositionAnimator";
	((GF_Node *)p)->sgprivate->node_del = PositionAnimator_Del;
	((GF_Node *)p)->sgprivate->get_field_count = PositionAnimator_get_field_count;
	((GF_Node *)p)->sgprivate->get_field = PositionAnimator_get_field;
#endif


	/*default field values*/
	p->fromTo.x = FLT2FIX(0);
	p->fromTo.y = FLT2FIX(1);
	p->keyType = 0;
	p->keySpline.vals = malloc(sizeof(SFVec2f)*2);
	p->keySpline.count = 2;
	p->keySpline.vals[0].x = FLT2FIX(0);
	p->keySpline.vals[0].y = FLT2FIX(0);
	p->keySpline.vals[1].x = FLT2FIX(1);
	p->keySpline.vals[1].y = FLT2FIX(1);
	p->keyValueType = 0;
	p->offset.x = FLT2FIX(0);
	p->offset.y = FLT2FIX(0);
	p->offset.z = FLT2FIX(0);
	return (GF_Node *)p;
}


/*
	PositionAnimator2D Node deletion
*/

static void PositionAnimator2D_Del(GF_Node *node)
{
	M_PositionAnimator2D *p = (M_PositionAnimator2D *) node;
	gf_sg_mffloat_del(p->key);
	gf_sg_mfvec2f_del(p->keySpline);
	gf_sg_mfvec2f_del(p->keyValue);
	gf_sg_mffloat_del(p->weight);
	gf_node_free((GF_Node *) p);
}

static const u16 PositionAnimator2D_Def2All[] = { 1, 2, 3, 4, 5, 6, 7, 8, 9};
static const u16 PositionAnimator2D_In2All[] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9};
static const u16 PositionAnimator2D_Out2All[] = { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12};

static u32 PositionAnimator2D_get_field_count(GF_Node *node, u8 IndexMode)
{
	switch(IndexMode) {
	case GF_SG_FIELD_CODING_IN: return 10;
	case GF_SG_FIELD_CODING_DEF: return 9;
	case GF_SG_FIELD_CODING_OUT: return 12;
	case GF_SG_FIELD_CODING_DYN: return 0;
	default:
		return 13;
	}
}

static GF_Err PositionAnimator2D_get_field_index(GF_Node *n, u32 inField, u8 IndexMode, u32 *allField)
{
	switch(IndexMode) {
	case GF_SG_FIELD_CODING_IN:
		*allField = PositionAnimator2D_In2All[inField];
		return GF_OK;
	case GF_SG_FIELD_CODING_DEF:
		*allField = PositionAnimator2D_Def2All[inField];
		return GF_OK;
	case GF_SG_FIELD_CODING_OUT:
		*allField = PositionAnimator2D_Out2All[inField];
		return GF_OK;
	default:
		return GF_BAD_PARAM;
	}
}
static GF_Err PositionAnimator2D_get_field(GF_Node *node, GF_FieldInfo *info)
{
	switch (info->fieldIndex) {
	case 0:
		info->name = "set_fraction";
		info->eventType = GF_SG_EVENT_IN;
		info->on_event_in = ((M_PositionAnimator2D *)node)->on_set_fraction;
		info->fieldType = GF_SG_VRML_SFFLOAT;
		info->far_ptr = & ((M_PositionAnimator2D *) node)->set_fraction;
		return GF_OK;
	case 1:
		info->name = "fromTo";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFVEC2F;
		info->far_ptr = & ((M_PositionAnimator2D *) node)->fromTo;
		return GF_OK;
	case 2:
		info->name = "key";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_MFFLOAT;
		info->far_ptr = & ((M_PositionAnimator2D *) node)->key;
		return GF_OK;
	case 3:
		info->name = "keyOrientation";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFINT32;
		info->far_ptr = & ((M_PositionAnimator2D *) node)->keyOrientation;
		return GF_OK;
	case 4:
		info->name = "keyType";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFINT32;
		info->far_ptr = & ((M_PositionAnimator2D *) node)->keyType;
		return GF_OK;
	case 5:
		info->name = "keySpline";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_MFVEC2F;
		info->far_ptr = & ((M_PositionAnimator2D *) node)->keySpline;
		return GF_OK;
	case 6:
		info->name = "keyValue";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_MFVEC2F;
		info->far_ptr = & ((M_PositionAnimator2D *) node)->keyValue;
		return GF_OK;
	case 7:
		info->name = "keyValueType";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFINT32;
		info->far_ptr = & ((M_PositionAnimator2D *) node)->keyValueType;
		return GF_OK;
	case 8:
		info->name = "offset";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFVEC2F;
		info->far_ptr = & ((M_PositionAnimator2D *) node)->offset;
		return GF_OK;
	case 9:
		info->name = "weight";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_MFFLOAT;
		info->far_ptr = & ((M_PositionAnimator2D *) node)->weight;
		return GF_OK;
	case 10:
		info->name = "endValue";
		info->eventType = GF_SG_EVENT_OUT;
		info->fieldType = GF_SG_VRML_SFVEC2F;
		info->far_ptr = & ((M_PositionAnimator2D *) node)->endValue;
		return GF_OK;
	case 11:
		info->name = "rotation_changed";
		info->eventType = GF_SG_EVENT_OUT;
		info->fieldType = GF_SG_VRML_SFFLOAT;
		info->far_ptr = & ((M_PositionAnimator2D *) node)->rotation_changed;
		return GF_OK;
	case 12:
		info->name = "value_changed";
		info->eventType = GF_SG_EVENT_OUT;
		info->fieldType = GF_SG_VRML_SFVEC2F;
		info->far_ptr = & ((M_PositionAnimator2D *) node)->value_changed;
		return GF_OK;
	default:
		return GF_BAD_PARAM;
	}
}

static Bool PositionAnimator2D_get_aq_info(GF_Node *n, u32 FieldIndex, u8 *QType, u8 *AType, Fixed *b_min, Fixed *b_max, u32 *QT13_bits)
{
	switch (FieldIndex) {
	case 1:
		*AType = 0;
		*QType = 8;
		return 1;
	case 2:
		*AType = 0;
		*QType = 8;
		return 1;
	case 5:
		*AType = 0;
		*QType = 8;
		return 1;
	case 6:
		*AType = 0;
		*QType = 4;
		return 1;
	case 8:
		*AType = 0;
		*QType = 2;
		*b_min = FIX_MIN;
		*b_max = FIX_MAX;
		return 1;
	default:
		return 0;
	}
}



GF_Node *PositionAnimator2D_Create()
{
	M_PositionAnimator2D *p;
	GF_SAFEALLOC(p, sizeof(M_PositionAnimator2D));
	if(!p) return NULL;
	gf_node_setup((GF_Node *)p, TAG_MPEG4_PositionAnimator2D);

#ifdef GF_NODE_USE_POINTERS
	((GF_Node *)p)->sgprivate->name = "PositionAnimator2D";
	((GF_Node *)p)->sgprivate->node_del = PositionAnimator2D_Del;
	((GF_Node *)p)->sgprivate->get_field_count = PositionAnimator2D_get_field_count;
	((GF_Node *)p)->sgprivate->get_field = PositionAnimator2D_get_field;
#endif


	/*default field values*/
	p->fromTo.x = FLT2FIX(0);
	p->fromTo.y = FLT2FIX(1);
	p->keyOrientation = 0;
	p->keyType = 0;
	p->keySpline.vals = malloc(sizeof(SFVec2f)*2);
	p->keySpline.count = 2;
	p->keySpline.vals[0].x = FLT2FIX(0);
	p->keySpline.vals[0].y = FLT2FIX(0);
	p->keySpline.vals[1].x = FLT2FIX(1);
	p->keySpline.vals[1].y = FLT2FIX(1);
	p->keyValueType = 0;
	p->offset.x = FLT2FIX(0);
	p->offset.y = FLT2FIX(0);
	return (GF_Node *)p;
}


/*
	PositionInterpolator4D Node deletion
*/

static void PositionInterpolator4D_Del(GF_Node *node)
{
	M_PositionInterpolator4D *p = (M_PositionInterpolator4D *) node;
	gf_sg_mffloat_del(p->key);
	gf_sg_mfvec4f_del(p->keyValue);
	gf_node_free((GF_Node *) p);
}

static const u16 PositionInterpolator4D_Def2All[] = { 1, 2};
static const u16 PositionInterpolator4D_In2All[] = { 0, 1, 2};
static const u16 PositionInterpolator4D_Out2All[] = { 1, 2, 3};

static u32 PositionInterpolator4D_get_field_count(GF_Node *node, u8 IndexMode)
{
	switch(IndexMode) {
	case GF_SG_FIELD_CODING_IN: return 3;
	case GF_SG_FIELD_CODING_DEF: return 2;
	case GF_SG_FIELD_CODING_OUT: return 3;
	case GF_SG_FIELD_CODING_DYN: return 0;
	default:
		return 4;
	}
}

static GF_Err PositionInterpolator4D_get_field_index(GF_Node *n, u32 inField, u8 IndexMode, u32 *allField)
{
	switch(IndexMode) {
	case GF_SG_FIELD_CODING_IN:
		*allField = PositionInterpolator4D_In2All[inField];
		return GF_OK;
	case GF_SG_FIELD_CODING_DEF:
		*allField = PositionInterpolator4D_Def2All[inField];
		return GF_OK;
	case GF_SG_FIELD_CODING_OUT:
		*allField = PositionInterpolator4D_Out2All[inField];
		return GF_OK;
	default:
		return GF_BAD_PARAM;
	}
}
static GF_Err PositionInterpolator4D_get_field(GF_Node *node, GF_FieldInfo *info)
{
	switch (info->fieldIndex) {
	case 0:
		info->name = "set_fraction";
		info->eventType = GF_SG_EVENT_IN;
		info->on_event_in = ((M_PositionInterpolator4D *)node)->on_set_fraction;
		info->fieldType = GF_SG_VRML_SFFLOAT;
		info->far_ptr = & ((M_PositionInterpolator4D *) node)->set_fraction;
		return GF_OK;
	case 1:
		info->name = "key";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_MFFLOAT;
		info->far_ptr = & ((M_PositionInterpolator4D *) node)->key;
		return GF_OK;
	case 2:
		info->name = "keyValue";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_MFVEC4F;
		info->far_ptr = & ((M_PositionInterpolator4D *) node)->keyValue;
		return GF_OK;
	case 3:
		info->name = "value_changed";
		info->eventType = GF_SG_EVENT_OUT;
		info->fieldType = GF_SG_VRML_SFVEC4F;
		info->far_ptr = & ((M_PositionInterpolator4D *) node)->value_changed;
		return GF_OK;
	default:
		return GF_BAD_PARAM;
	}
}

static Bool PositionInterpolator4D_get_aq_info(GF_Node *n, u32 FieldIndex, u8 *QType, u8 *AType, Fixed *b_min, Fixed *b_max, u32 *QT13_bits)
{
	switch (FieldIndex) {
	case 1:
		*AType = 0;
		*QType = 8;
		*b_min = FLT2FIX(0);
		*b_max = FLT2FIX(1);
		return 1;
	case 2:
		*AType = 0;
		*QType = 15;
		*b_min = FIX_MIN;
		*b_max = FIX_MAX;
		return 1;
	default:
		return 0;
	}
}



GF_Node *PositionInterpolator4D_Create()
{
	M_PositionInterpolator4D *p;
	GF_SAFEALLOC(p, sizeof(M_PositionInterpolator4D));
	if(!p) return NULL;
	gf_node_setup((GF_Node *)p, TAG_MPEG4_PositionInterpolator4D);

#ifdef GF_NODE_USE_POINTERS
	((GF_Node *)p)->sgprivate->name = "PositionInterpolator4D";
	((GF_Node *)p)->sgprivate->node_del = PositionInterpolator4D_Del;
	((GF_Node *)p)->sgprivate->get_field_count = PositionInterpolator4D_get_field_count;
	((GF_Node *)p)->sgprivate->get_field = PositionInterpolator4D_get_field;
#endif


	/*default field values*/
	return (GF_Node *)p;
}


/*
	ScalarAnimator Node deletion
*/

static void ScalarAnimator_Del(GF_Node *node)
{
	M_ScalarAnimator *p = (M_ScalarAnimator *) node;
	gf_sg_mffloat_del(p->key);
	gf_sg_mfvec2f_del(p->keySpline);
	gf_sg_mffloat_del(p->keyValue);
	gf_sg_mffloat_del(p->weight);
	gf_node_free((GF_Node *) p);
}

static const u16 ScalarAnimator_Def2All[] = { 1, 2, 3, 4, 5, 6, 7, 8};
static const u16 ScalarAnimator_In2All[] = { 0, 1, 2, 3, 4, 5, 6, 7, 8};
static const u16 ScalarAnimator_Out2All[] = { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10};

static u32 ScalarAnimator_get_field_count(GF_Node *node, u8 IndexMode)
{
	switch(IndexMode) {
	case GF_SG_FIELD_CODING_IN: return 9;
	case GF_SG_FIELD_CODING_DEF: return 8;
	case GF_SG_FIELD_CODING_OUT: return 10;
	case GF_SG_FIELD_CODING_DYN: return 0;
	default:
		return 11;
	}
}

static GF_Err ScalarAnimator_get_field_index(GF_Node *n, u32 inField, u8 IndexMode, u32 *allField)
{
	switch(IndexMode) {
	case GF_SG_FIELD_CODING_IN:
		*allField = ScalarAnimator_In2All[inField];
		return GF_OK;
	case GF_SG_FIELD_CODING_DEF:
		*allField = ScalarAnimator_Def2All[inField];
		return GF_OK;
	case GF_SG_FIELD_CODING_OUT:
		*allField = ScalarAnimator_Out2All[inField];
		return GF_OK;
	default:
		return GF_BAD_PARAM;
	}
}
static GF_Err ScalarAnimator_get_field(GF_Node *node, GF_FieldInfo *info)
{
	switch (info->fieldIndex) {
	case 0:
		info->name = "set_fraction";
		info->eventType = GF_SG_EVENT_IN;
		info->on_event_in = ((M_ScalarAnimator *)node)->on_set_fraction;
		info->fieldType = GF_SG_VRML_SFFLOAT;
		info->far_ptr = & ((M_ScalarAnimator *) node)->set_fraction;
		return GF_OK;
	case 1:
		info->name = "fromTo";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFVEC2F;
		info->far_ptr = & ((M_ScalarAnimator *) node)->fromTo;
		return GF_OK;
	case 2:
		info->name = "key";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_MFFLOAT;
		info->far_ptr = & ((M_ScalarAnimator *) node)->key;
		return GF_OK;
	case 3:
		info->name = "keyType";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFINT32;
		info->far_ptr = & ((M_ScalarAnimator *) node)->keyType;
		return GF_OK;
	case 4:
		info->name = "keySpline";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_MFVEC2F;
		info->far_ptr = & ((M_ScalarAnimator *) node)->keySpline;
		return GF_OK;
	case 5:
		info->name = "keyValue";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_MFFLOAT;
		info->far_ptr = & ((M_ScalarAnimator *) node)->keyValue;
		return GF_OK;
	case 6:
		info->name = "keyValueType";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFINT32;
		info->far_ptr = & ((M_ScalarAnimator *) node)->keyValueType;
		return GF_OK;
	case 7:
		info->name = "offset";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFFLOAT;
		info->far_ptr = & ((M_ScalarAnimator *) node)->offset;
		return GF_OK;
	case 8:
		info->name = "weight";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_MFFLOAT;
		info->far_ptr = & ((M_ScalarAnimator *) node)->weight;
		return GF_OK;
	case 9:
		info->name = "endValue";
		info->eventType = GF_SG_EVENT_OUT;
		info->fieldType = GF_SG_VRML_SFFLOAT;
		info->far_ptr = & ((M_ScalarAnimator *) node)->endValue;
		return GF_OK;
	case 10:
		info->name = "value_changed";
		info->eventType = GF_SG_EVENT_OUT;
		info->fieldType = GF_SG_VRML_SFFLOAT;
		info->far_ptr = & ((M_ScalarAnimator *) node)->value_changed;
		return GF_OK;
	default:
		return GF_BAD_PARAM;
	}
}

static Bool ScalarAnimator_get_aq_info(GF_Node *n, u32 FieldIndex, u8 *QType, u8 *AType, Fixed *b_min, Fixed *b_max, u32 *QT13_bits)
{
	switch (FieldIndex) {
	case 1:
		*AType = 0;
		*QType = 8;
		return 1;
	case 2:
		*AType = 0;
		*QType = 8;
		return 1;
	case 4:
		*AType = 0;
		*QType = 8;
		return 1;
	case 5:
		*AType = 0;
		*QType = 0;
		return 1;
	default:
		return 0;
	}
}



GF_Node *ScalarAnimator_Create()
{
	M_ScalarAnimator *p;
	GF_SAFEALLOC(p, sizeof(M_ScalarAnimator));
	if(!p) return NULL;
	gf_node_setup((GF_Node *)p, TAG_MPEG4_ScalarAnimator);

#ifdef GF_NODE_USE_POINTERS
	((GF_Node *)p)->sgprivate->name = "ScalarAnimator";
	((GF_Node *)p)->sgprivate->node_del = ScalarAnimator_Del;
	((GF_Node *)p)->sgprivate->get_field_count = ScalarAnimator_get_field_count;
	((GF_Node *)p)->sgprivate->get_field = ScalarAnimator_get_field;
#endif


	/*default field values*/
	p->fromTo.x = FLT2FIX(0);
	p->fromTo.y = FLT2FIX(1);
	p->keyType = 0;
	p->keySpline.vals = malloc(sizeof(SFVec2f)*2);
	p->keySpline.count = 2;
	p->keySpline.vals[0].x = FLT2FIX(0);
	p->keySpline.vals[0].y = FLT2FIX(0);
	p->keySpline.vals[1].x = FLT2FIX(1);
	p->keySpline.vals[1].y = FLT2FIX(1);
	p->keyValueType = 0;
	p->offset = FLT2FIX(0);
	return (GF_Node *)p;
}


/*
	Clipper2D Node deletion
*/

static void Clipper2D_Del(GF_Node *node)
{
	M_Clipper2D *p = (M_Clipper2D *) node;
	gf_sg_vrml_parent_reset((GF_Node *) p);	
	gf_node_unregister((GF_Node *) p->geometry, (GF_Node *) p);	
	gf_node_unregister((GF_Node *) p->transform, (GF_Node *) p);	
	gf_node_free((GF_Node *) p);
}

static const u16 Clipper2D_Def2All[] = { 2, 3, 4, 5, 6};
static const u16 Clipper2D_In2All[] = { 0, 1, 2, 3, 4, 5, 6};
static const u16 Clipper2D_Out2All[] = { 2, 3, 4, 5, 6};

static u32 Clipper2D_get_field_count(GF_Node *node, u8 IndexMode)
{
	switch(IndexMode) {
	case GF_SG_FIELD_CODING_IN: return 7;
	case GF_SG_FIELD_CODING_DEF: return 5;
	case GF_SG_FIELD_CODING_OUT: return 5;
	case GF_SG_FIELD_CODING_DYN: return 0;
	default:
		return 7;
	}
}

static GF_Err Clipper2D_get_field_index(GF_Node *n, u32 inField, u8 IndexMode, u32 *allField)
{
	switch(IndexMode) {
	case GF_SG_FIELD_CODING_IN:
		*allField = Clipper2D_In2All[inField];
		return GF_OK;
	case GF_SG_FIELD_CODING_DEF:
		*allField = Clipper2D_Def2All[inField];
		return GF_OK;
	case GF_SG_FIELD_CODING_OUT:
		*allField = Clipper2D_Out2All[inField];
		return GF_OK;
	default:
		return GF_BAD_PARAM;
	}
}
static GF_Err Clipper2D_get_field(GF_Node *node, GF_FieldInfo *info)
{
	switch (info->fieldIndex) {
	case 0:
		info->name = "addChildren";
		info->eventType = GF_SG_EVENT_IN;
		info->on_event_in = ((M_Clipper2D *)node)->on_addChildren;
		info->fieldType = GF_SG_VRML_MFNODE;
		info->NDTtype = NDT_SF2DNode;
		info->far_ptr = & ((M_Clipper2D *)node)->addChildren;
		return GF_OK;
	case 1:
		info->name = "removeChildren";
		info->eventType = GF_SG_EVENT_IN;
		info->on_event_in = ((M_Clipper2D *)node)->on_removeChildren;
		info->fieldType = GF_SG_VRML_MFNODE;
		info->NDTtype = NDT_SF2DNode;
		info->far_ptr = & ((M_Clipper2D *)node)->removeChildren;
		return GF_OK;
	case 2:
		info->name = "children";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_MFNODE;
		info->NDTtype = NDT_SF2DNode;
		info->far_ptr = & ((M_Clipper2D *)node)->children;
		return GF_OK;
	case 3:
		info->name = "geometry";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFNODE;
		info->NDTtype = NDT_SFGeometryNode;
		info->far_ptr = & ((M_Clipper2D *)node)->geometry;
		return GF_OK;
	case 4:
		info->name = "inside";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFBOOL;
		info->far_ptr = & ((M_Clipper2D *) node)->inside;
		return GF_OK;
	case 5:
		info->name = "transform";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFNODE;
		info->NDTtype = NDT_SF2DNode;
		info->far_ptr = & ((M_Clipper2D *)node)->transform;
		return GF_OK;
	case 6:
		info->name = "XOR";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFBOOL;
		info->far_ptr = & ((M_Clipper2D *) node)->XOR;
		return GF_OK;
	default:
		return GF_BAD_PARAM;
	}
}

static Bool Clipper2D_get_aq_info(GF_Node *n, u32 FieldIndex, u8 *QType, u8 *AType, Fixed *b_min, Fixed *b_max, u32 *QT13_bits)
{
	switch (FieldIndex) {
	default:
		return 0;
	}
}



GF_Node *Clipper2D_Create()
{
	M_Clipper2D *p;
	GF_SAFEALLOC(p, sizeof(M_Clipper2D));
	if(!p) return NULL;
	gf_node_setup((GF_Node *)p, TAG_MPEG4_Clipper2D);
	gf_sg_vrml_parent_setup((GF_Node *) p);

#ifdef GF_NODE_USE_POINTERS
	((GF_Node *)p)->sgprivate->name = "Clipper2D";
	((GF_Node *)p)->sgprivate->node_del = Clipper2D_Del;
	((GF_Node *)p)->sgprivate->get_field_count = Clipper2D_get_field_count;
	((GF_Node *)p)->sgprivate->get_field = Clipper2D_get_field;
#endif


	/*default field values*/
	p->inside = 1;
	return (GF_Node *)p;
}


/*
	ColorTransform Node deletion
*/

static void ColorTransform_Del(GF_Node *node)
{
	M_ColorTransform *p = (M_ColorTransform *) node;
	gf_sg_vrml_parent_reset((GF_Node *) p);	
	gf_node_free((GF_Node *) p);
}

static const u16 ColorTransform_Def2All[] = { 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22};
static const u16 ColorTransform_In2All[] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22};
static const u16 ColorTransform_Out2All[] = { 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22};
static const u16 ColorTransform_Dyn2All[] = { 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22};

static u32 ColorTransform_get_field_count(GF_Node *node, u8 IndexMode)
{
	switch(IndexMode) {
	case GF_SG_FIELD_CODING_IN: return 23;
	case GF_SG_FIELD_CODING_DEF: return 21;
	case GF_SG_FIELD_CODING_OUT: return 21;
	case GF_SG_FIELD_CODING_DYN: return 20;
	default:
		return 23;
	}
}

static GF_Err ColorTransform_get_field_index(GF_Node *n, u32 inField, u8 IndexMode, u32 *allField)
{
	switch(IndexMode) {
	case GF_SG_FIELD_CODING_IN:
		*allField = ColorTransform_In2All[inField];
		return GF_OK;
	case GF_SG_FIELD_CODING_DEF:
		*allField = ColorTransform_Def2All[inField];
		return GF_OK;
	case GF_SG_FIELD_CODING_OUT:
		*allField = ColorTransform_Out2All[inField];
		return GF_OK;
	case GF_SG_FIELD_CODING_DYN:
		*allField = ColorTransform_Dyn2All[inField];
		return GF_OK;
	default:
		return GF_BAD_PARAM;
	}
}
static GF_Err ColorTransform_get_field(GF_Node *node, GF_FieldInfo *info)
{
	switch (info->fieldIndex) {
	case 0:
		info->name = "addChildren";
		info->eventType = GF_SG_EVENT_IN;
		info->on_event_in = ((M_ColorTransform *)node)->on_addChildren;
		info->fieldType = GF_SG_VRML_MFNODE;
		info->NDTtype = NDT_SF3DNode;
		info->far_ptr = & ((M_ColorTransform *)node)->addChildren;
		return GF_OK;
	case 1:
		info->name = "removeChildren";
		info->eventType = GF_SG_EVENT_IN;
		info->on_event_in = ((M_ColorTransform *)node)->on_removeChildren;
		info->fieldType = GF_SG_VRML_MFNODE;
		info->NDTtype = NDT_SF3DNode;
		info->far_ptr = & ((M_ColorTransform *)node)->removeChildren;
		return GF_OK;
	case 2:
		info->name = "children";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_MFNODE;
		info->NDTtype = NDT_SF3DNode;
		info->far_ptr = & ((M_ColorTransform *)node)->children;
		return GF_OK;
	case 3:
		info->name = "mrr";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFFLOAT;
		info->far_ptr = & ((M_ColorTransform *) node)->mrr;
		return GF_OK;
	case 4:
		info->name = "mrg";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFFLOAT;
		info->far_ptr = & ((M_ColorTransform *) node)->mrg;
		return GF_OK;
	case 5:
		info->name = "mrb";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFFLOAT;
		info->far_ptr = & ((M_ColorTransform *) node)->mrb;
		return GF_OK;
	case 6:
		info->name = "mra";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFFLOAT;
		info->far_ptr = & ((M_ColorTransform *) node)->mra;
		return GF_OK;
	case 7:
		info->name = "tr";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFFLOAT;
		info->far_ptr = & ((M_ColorTransform *) node)->tr;
		return GF_OK;
	case 8:
		info->name = "mgr";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFFLOAT;
		info->far_ptr = & ((M_ColorTransform *) node)->mgr;
		return GF_OK;
	case 9:
		info->name = "mgg";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFFLOAT;
		info->far_ptr = & ((M_ColorTransform *) node)->mgg;
		return GF_OK;
	case 10:
		info->name = "mgb";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFFLOAT;
		info->far_ptr = & ((M_ColorTransform *) node)->mgb;
		return GF_OK;
	case 11:
		info->name = "mga";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFFLOAT;
		info->far_ptr = & ((M_ColorTransform *) node)->mga;
		return GF_OK;
	case 12:
		info->name = "tg";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFFLOAT;
		info->far_ptr = & ((M_ColorTransform *) node)->tg;
		return GF_OK;
	case 13:
		info->name = "mbr";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFFLOAT;
		info->far_ptr = & ((M_ColorTransform *) node)->mbr;
		return GF_OK;
	case 14:
		info->name = "mbg";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFFLOAT;
		info->far_ptr = & ((M_ColorTransform *) node)->mbg;
		return GF_OK;
	case 15:
		info->name = "mbb";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFFLOAT;
		info->far_ptr = & ((M_ColorTransform *) node)->mbb;
		return GF_OK;
	case 16:
		info->name = "mba";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFFLOAT;
		info->far_ptr = & ((M_ColorTransform *) node)->mba;
		return GF_OK;
	case 17:
		info->name = "tb";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFFLOAT;
		info->far_ptr = & ((M_ColorTransform *) node)->tb;
		return GF_OK;
	case 18:
		info->name = "mar";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFFLOAT;
		info->far_ptr = & ((M_ColorTransform *) node)->mar;
		return GF_OK;
	case 19:
		info->name = "mag";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFFLOAT;
		info->far_ptr = & ((M_ColorTransform *) node)->mag;
		return GF_OK;
	case 20:
		info->name = "mab";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFFLOAT;
		info->far_ptr = & ((M_ColorTransform *) node)->mab;
		return GF_OK;
	case 21:
		info->name = "maa";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFFLOAT;
		info->far_ptr = & ((M_ColorTransform *) node)->maa;
		return GF_OK;
	case 22:
		info->name = "ta";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFFLOAT;
		info->far_ptr = & ((M_ColorTransform *) node)->ta;
		return GF_OK;
	default:
		return GF_BAD_PARAM;
	}
}

static Bool ColorTransform_get_aq_info(GF_Node *n, u32 FieldIndex, u8 *QType, u8 *AType, Fixed *b_min, Fixed *b_max, u32 *QT13_bits)
{
	switch (FieldIndex) {
	case 3:
		*AType = 7;
		*QType = 7;
		*b_min = FIX_MIN;
		*b_max = FIX_MAX;
		return 1;
	case 4:
		*AType = 7;
		*QType = 7;
		*b_min = FIX_MIN;
		*b_max = FIX_MAX;
		return 1;
	case 5:
		*AType = 7;
		*QType = 7;
		*b_min = FIX_MIN;
		*b_max = FIX_MAX;
		return 1;
	case 6:
		*AType = 7;
		*QType = 7;
		*b_min = FIX_MIN;
		*b_max = FIX_MAX;
		return 1;
	case 7:
		*AType = 7;
		*QType = 4;
		*b_min = FIX_MIN;
		*b_max = FIX_MAX;
		return 1;
	case 8:
		*AType = 7;
		*QType = 7;
		*b_min = FIX_MIN;
		*b_max = FIX_MAX;
		return 1;
	case 9:
		*AType = 7;
		*QType = 7;
		*b_min = FIX_MIN;
		*b_max = FIX_MAX;
		return 1;
	case 10:
		*AType = 7;
		*QType = 7;
		*b_min = FIX_MIN;
		*b_max = FIX_MAX;
		return 1;
	case 11:
		*AType = 7;
		*QType = 7;
		*b_min = FIX_MIN;
		*b_max = FIX_MAX;
		return 1;
	case 12:
		*AType = 7;
		*QType = 4;
		*b_min = FIX_MIN;
		*b_max = FIX_MAX;
		return 1;
	case 13:
		*AType = 7;
		*QType = 7;
		*b_min = FIX_MIN;
		*b_max = FIX_MAX;
		return 1;
	case 14:
		*AType = 7;
		*QType = 7;
		*b_min = FIX_MIN;
		*b_max = FIX_MAX;
		return 1;
	case 15:
		*AType = 7;
		*QType = 7;
		*b_min = FIX_MIN;
		*b_max = FIX_MAX;
		return 1;
	case 16:
		*AType = 7;
		*QType = 7;
		*b_min = FIX_MIN;
		*b_max = FIX_MAX;
		return 1;
	case 17:
		*AType = 7;
		*QType = 4;
		*b_min = FIX_MIN;
		*b_max = FIX_MAX;
		return 1;
	case 18:
		*AType = 7;
		*QType = 7;
		*b_min = FIX_MIN;
		*b_max = FIX_MAX;
		return 1;
	case 19:
		*AType = 7;
		*QType = 7;
		*b_min = FIX_MIN;
		*b_max = FIX_MAX;
		return 1;
	case 20:
		*AType = 7;
		*QType = 7;
		*b_min = FIX_MIN;
		*b_max = FIX_MAX;
		return 1;
	case 21:
		*AType = 7;
		*QType = 7;
		*b_min = FIX_MIN;
		*b_max = FIX_MAX;
		return 1;
	case 22:
		*AType = 7;
		*QType = 4;
		*b_min = FIX_MIN;
		*b_max = FIX_MAX;
		return 1;
	default:
		return 0;
	}
}



GF_Node *ColorTransform_Create()
{
	M_ColorTransform *p;
	GF_SAFEALLOC(p, sizeof(M_ColorTransform));
	if(!p) return NULL;
	gf_node_setup((GF_Node *)p, TAG_MPEG4_ColorTransform);
	gf_sg_vrml_parent_setup((GF_Node *) p);

#ifdef GF_NODE_USE_POINTERS
	((GF_Node *)p)->sgprivate->name = "ColorTransform";
	((GF_Node *)p)->sgprivate->node_del = ColorTransform_Del;
	((GF_Node *)p)->sgprivate->get_field_count = ColorTransform_get_field_count;
	((GF_Node *)p)->sgprivate->get_field = ColorTransform_get_field;
#endif


	/*default field values*/
	p->mrr = FLT2FIX(1);
	p->mrg = FLT2FIX(0);
	p->mrb = FLT2FIX(0);
	p->mra = FLT2FIX(0);
	p->tr = FLT2FIX(0);
	p->mgr = FLT2FIX(0);
	p->mgg = FLT2FIX(1);
	p->mgb = FLT2FIX(0);
	p->mga = FLT2FIX(0);
	p->tg = FLT2FIX(0);
	p->mbr = FLT2FIX(0);
	p->mbg = FLT2FIX(0);
	p->mbb = FLT2FIX(1);
	p->mba = FLT2FIX(0);
	p->tb = FLT2FIX(0);
	p->mar = FLT2FIX(0);
	p->mag = FLT2FIX(0);
	p->mab = FLT2FIX(0);
	p->maa = FLT2FIX(1);
	p->ta = FLT2FIX(0);
	return (GF_Node *)p;
}


/*
	Ellipse Node deletion
*/

static void Ellipse_Del(GF_Node *node)
{
	M_Ellipse *p = (M_Ellipse *) node;
	gf_node_free((GF_Node *) p);
}

static const u16 Ellipse_Def2All[] = { 0};
static const u16 Ellipse_In2All[] = { 0};
static const u16 Ellipse_Out2All[] = { 0};
static const u16 Ellipse_Dyn2All[] = { 0};

static u32 Ellipse_get_field_count(GF_Node *node, u8 IndexMode)
{
	switch(IndexMode) {
	case GF_SG_FIELD_CODING_IN: return 1;
	case GF_SG_FIELD_CODING_DEF: return 1;
	case GF_SG_FIELD_CODING_OUT: return 1;
	case GF_SG_FIELD_CODING_DYN: return 1;
	default:
		return 1;
	}
}

static GF_Err Ellipse_get_field_index(GF_Node *n, u32 inField, u8 IndexMode, u32 *allField)
{
	switch(IndexMode) {
	case GF_SG_FIELD_CODING_IN:
		*allField = Ellipse_In2All[inField];
		return GF_OK;
	case GF_SG_FIELD_CODING_DEF:
		*allField = Ellipse_Def2All[inField];
		return GF_OK;
	case GF_SG_FIELD_CODING_OUT:
		*allField = Ellipse_Out2All[inField];
		return GF_OK;
	case GF_SG_FIELD_CODING_DYN:
		*allField = Ellipse_Dyn2All[inField];
		return GF_OK;
	default:
		return GF_BAD_PARAM;
	}
}
static GF_Err Ellipse_get_field(GF_Node *node, GF_FieldInfo *info)
{
	switch (info->fieldIndex) {
	case 0:
		info->name = "radius";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFVEC2F;
		info->far_ptr = & ((M_Ellipse *) node)->radius;
		return GF_OK;
	default:
		return GF_BAD_PARAM;
	}
}

static Bool Ellipse_get_aq_info(GF_Node *n, u32 FieldIndex, u8 *QType, u8 *AType, Fixed *b_min, Fixed *b_max, u32 *QT13_bits)
{
	switch (FieldIndex) {
	case 0:
		*AType = 2;
		*QType = 12;
		*b_min = FLT2FIX(0);
		*b_max = FIX_MAX;
		return 1;
	default:
		return 0;
	}
}



GF_Node *Ellipse_Create()
{
	M_Ellipse *p;
	GF_SAFEALLOC(p, sizeof(M_Ellipse));
	if(!p) return NULL;
	gf_node_setup((GF_Node *)p, TAG_MPEG4_Ellipse);

#ifdef GF_NODE_USE_POINTERS
	((GF_Node *)p)->sgprivate->name = "Ellipse";
	((GF_Node *)p)->sgprivate->node_del = Ellipse_Del;
	((GF_Node *)p)->sgprivate->get_field_count = Ellipse_get_field_count;
	((GF_Node *)p)->sgprivate->get_field = Ellipse_get_field;
#endif


	/*default field values*/
	p->radius.x = FLT2FIX(1);
	p->radius.y = FLT2FIX(1);
	return (GF_Node *)p;
}


/*
	LinearGradient Node deletion
*/

static void LinearGradient_Del(GF_Node *node)
{
	M_LinearGradient *p = (M_LinearGradient *) node;
	gf_sg_mffloat_del(p->key);
	gf_sg_mfcolor_del(p->keyValue);
	gf_sg_mffloat_del(p->opacity);
	gf_node_unregister((GF_Node *) p->transform, (GF_Node *) p);	
	gf_node_free((GF_Node *) p);
}

static const u16 LinearGradient_Def2All[] = { 0, 1, 2, 3, 4, 5, 6};
static const u16 LinearGradient_In2All[] = { 0, 1, 2, 3, 4, 5, 6};
static const u16 LinearGradient_Out2All[] = { 0, 1, 2, 3, 4, 5, 6};
static const u16 LinearGradient_Dyn2All[] = { 0, 5};

static u32 LinearGradient_get_field_count(GF_Node *node, u8 IndexMode)
{
	switch(IndexMode) {
	case GF_SG_FIELD_CODING_IN: return 7;
	case GF_SG_FIELD_CODING_DEF: return 7;
	case GF_SG_FIELD_CODING_OUT: return 7;
	case GF_SG_FIELD_CODING_DYN: return 2;
	default:
		return 7;
	}
}

static GF_Err LinearGradient_get_field_index(GF_Node *n, u32 inField, u8 IndexMode, u32 *allField)
{
	switch(IndexMode) {
	case GF_SG_FIELD_CODING_IN:
		*allField = LinearGradient_In2All[inField];
		return GF_OK;
	case GF_SG_FIELD_CODING_DEF:
		*allField = LinearGradient_Def2All[inField];
		return GF_OK;
	case GF_SG_FIELD_CODING_OUT:
		*allField = LinearGradient_Out2All[inField];
		return GF_OK;
	case GF_SG_FIELD_CODING_DYN:
		*allField = LinearGradient_Dyn2All[inField];
		return GF_OK;
	default:
		return GF_BAD_PARAM;
	}
}
static GF_Err LinearGradient_get_field(GF_Node *node, GF_FieldInfo *info)
{
	switch (info->fieldIndex) {
	case 0:
		info->name = "endPoint";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFVEC2F;
		info->far_ptr = & ((M_LinearGradient *) node)->endPoint;
		return GF_OK;
	case 1:
		info->name = "key";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_MFFLOAT;
		info->far_ptr = & ((M_LinearGradient *) node)->key;
		return GF_OK;
	case 2:
		info->name = "keyValue";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_MFCOLOR;
		info->far_ptr = & ((M_LinearGradient *) node)->keyValue;
		return GF_OK;
	case 3:
		info->name = "opacity";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_MFFLOAT;
		info->far_ptr = & ((M_LinearGradient *) node)->opacity;
		return GF_OK;
	case 4:
		info->name = "spreadMethod";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFINT32;
		info->far_ptr = & ((M_LinearGradient *) node)->spreadMethod;
		return GF_OK;
	case 5:
		info->name = "startPoint";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFVEC2F;
		info->far_ptr = & ((M_LinearGradient *) node)->startPoint;
		return GF_OK;
	case 6:
		info->name = "transform";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFNODE;
		info->NDTtype = NDT_SF3DNode;
		info->far_ptr = & ((M_LinearGradient *)node)->transform;
		return GF_OK;
	default:
		return GF_BAD_PARAM;
	}
}

static Bool LinearGradient_get_aq_info(GF_Node *n, u32 FieldIndex, u8 *QType, u8 *AType, Fixed *b_min, Fixed *b_max, u32 *QT13_bits)
{
	switch (FieldIndex) {
	case 0:
		*AType = 2;
		*QType = 5;
		*b_min = FIX_MIN;
		*b_max = FIX_MAX;
		return 1;
	case 1:
		*AType = 0;
		*QType = 8;
		*b_min = FLT2FIX(0);
		*b_max = FLT2FIX(1);
		return 1;
	case 2:
		*AType = 0;
		*QType = 4;
		*b_min = FLT2FIX(0);
		*b_max = FLT2FIX(1);
		return 1;
	case 3:
		*AType = 0;
		*QType = 7;
		*b_min = FLT2FIX(0);
		*b_max = FLT2FIX(1);
		return 1;
	case 4:
		*AType = 0;
		*QType = 13;
		*QT13_bits = 2;
		*b_min = FLT2FIX(0);
		*b_max = FLT2FIX(2);
		return 1;
	case 5:
		*AType = 2;
		*QType = 5;
		*b_min = FIX_MIN;
		*b_max = FIX_MAX;
		return 1;
	default:
		return 0;
	}
}



GF_Node *LinearGradient_Create()
{
	M_LinearGradient *p;
	GF_SAFEALLOC(p, sizeof(M_LinearGradient));
	if(!p) return NULL;
	gf_node_setup((GF_Node *)p, TAG_MPEG4_LinearGradient);

#ifdef GF_NODE_USE_POINTERS
	((GF_Node *)p)->sgprivate->name = "LinearGradient";
	((GF_Node *)p)->sgprivate->node_del = LinearGradient_Del;
	((GF_Node *)p)->sgprivate->get_field_count = LinearGradient_get_field_count;
	((GF_Node *)p)->sgprivate->get_field = LinearGradient_get_field;
#endif


	/*default field values*/
	p->endPoint.x = FLT2FIX(1);
	p->endPoint.y = FLT2FIX(0);
	p->opacity.vals = malloc(sizeof(SFFloat)*1);
	p->opacity.count = 1;
	p->opacity.vals[0] = FLT2FIX(1);
	p->spreadMethod = 0;
	p->startPoint.x = FLT2FIX(0);
	p->startPoint.y = FLT2FIX(0);
	return (GF_Node *)p;
}


/*
	PathLayout Node deletion
*/

static void PathLayout_Del(GF_Node *node)
{
	M_PathLayout *p = (M_PathLayout *) node;
	gf_sg_vrml_parent_reset((GF_Node *) p);	
	gf_node_unregister((GF_Node *) p->geometry, (GF_Node *) p);	
	gf_sg_mfint32_del(p->alignment);
	gf_node_free((GF_Node *) p);
}

static const u16 PathLayout_Def2All[] = { 2, 3, 4, 5, 6, 7, 8, 9};
static const u16 PathLayout_In2All[] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9};
static const u16 PathLayout_Out2All[] = { 2, 3, 4, 5, 6, 7, 8, 9};
static const u16 PathLayout_Dyn2All[] = { 5, 6};

static u32 PathLayout_get_field_count(GF_Node *node, u8 IndexMode)
{
	switch(IndexMode) {
	case GF_SG_FIELD_CODING_IN: return 10;
	case GF_SG_FIELD_CODING_DEF: return 8;
	case GF_SG_FIELD_CODING_OUT: return 8;
	case GF_SG_FIELD_CODING_DYN: return 2;
	default:
		return 10;
	}
}

static GF_Err PathLayout_get_field_index(GF_Node *n, u32 inField, u8 IndexMode, u32 *allField)
{
	switch(IndexMode) {
	case GF_SG_FIELD_CODING_IN:
		*allField = PathLayout_In2All[inField];
		return GF_OK;
	case GF_SG_FIELD_CODING_DEF:
		*allField = PathLayout_Def2All[inField];
		return GF_OK;
	case GF_SG_FIELD_CODING_OUT:
		*allField = PathLayout_Out2All[inField];
		return GF_OK;
	case GF_SG_FIELD_CODING_DYN:
		*allField = PathLayout_Dyn2All[inField];
		return GF_OK;
	default:
		return GF_BAD_PARAM;
	}
}
static GF_Err PathLayout_get_field(GF_Node *node, GF_FieldInfo *info)
{
	switch (info->fieldIndex) {
	case 0:
		info->name = "addChildren";
		info->eventType = GF_SG_EVENT_IN;
		info->on_event_in = ((M_PathLayout *)node)->on_addChildren;
		info->fieldType = GF_SG_VRML_MFNODE;
		info->NDTtype = NDT_SF2DNode;
		info->far_ptr = & ((M_PathLayout *)node)->addChildren;
		return GF_OK;
	case 1:
		info->name = "removeChildren";
		info->eventType = GF_SG_EVENT_IN;
		info->on_event_in = ((M_PathLayout *)node)->on_removeChildren;
		info->fieldType = GF_SG_VRML_MFNODE;
		info->NDTtype = NDT_SF2DNode;
		info->far_ptr = & ((M_PathLayout *)node)->removeChildren;
		return GF_OK;
	case 2:
		info->name = "children";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_MFNODE;
		info->NDTtype = NDT_SF2DNode;
		info->far_ptr = & ((M_PathLayout *)node)->children;
		return GF_OK;
	case 3:
		info->name = "geometry";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFNODE;
		info->NDTtype = NDT_SFGeometryNode;
		info->far_ptr = & ((M_PathLayout *)node)->geometry;
		return GF_OK;
	case 4:
		info->name = "alignment";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_MFINT32;
		info->far_ptr = & ((M_PathLayout *) node)->alignment;
		return GF_OK;
	case 5:
		info->name = "pathOffset";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFFLOAT;
		info->far_ptr = & ((M_PathLayout *) node)->pathOffset;
		return GF_OK;
	case 6:
		info->name = "spacing";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFFLOAT;
		info->far_ptr = & ((M_PathLayout *) node)->spacing;
		return GF_OK;
	case 7:
		info->name = "reverseLayout";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFBOOL;
		info->far_ptr = & ((M_PathLayout *) node)->reverseLayout;
		return GF_OK;
	case 8:
		info->name = "wrapMode";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFINT32;
		info->far_ptr = & ((M_PathLayout *) node)->wrapMode;
		return GF_OK;
	case 9:
		info->name = "splitText";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFBOOL;
		info->far_ptr = & ((M_PathLayout *) node)->splitText;
		return GF_OK;
	default:
		return GF_BAD_PARAM;
	}
}

static Bool PathLayout_get_aq_info(GF_Node *n, u32 FieldIndex, u8 *QType, u8 *AType, Fixed *b_min, Fixed *b_max, u32 *QT13_bits)
{
	switch (FieldIndex) {
	case 4:
		*AType = 0;
		*QType = 13;
		*QT13_bits = 2;
		*b_min = FLT2FIX(-1);
		*b_max = FLT2FIX(1);
		return 1;
	case 5:
		*AType = 7;
		*QType = 7;
		*b_min = FIX_MIN;
		*b_max = FIX_MAX;
		return 1;
	case 6:
		*AType = 7;
		*QType = 7;
		*b_min = FIX_MIN;
		*b_max = FIX_MAX;
		return 1;
	case 8:
		*AType = 0;
		*QType = 13;
		*QT13_bits = 2;
		*b_min = FLT2FIX(0);
		*b_max = FLT2FIX(2);
		return 1;
	default:
		return 0;
	}
}



GF_Node *PathLayout_Create()
{
	M_PathLayout *p;
	GF_SAFEALLOC(p, sizeof(M_PathLayout));
	if(!p) return NULL;
	gf_node_setup((GF_Node *)p, TAG_MPEG4_PathLayout);
	gf_sg_vrml_parent_setup((GF_Node *) p);

#ifdef GF_NODE_USE_POINTERS
	((GF_Node *)p)->sgprivate->name = "PathLayout";
	((GF_Node *)p)->sgprivate->node_del = PathLayout_Del;
	((GF_Node *)p)->sgprivate->get_field_count = PathLayout_get_field_count;
	((GF_Node *)p)->sgprivate->get_field = PathLayout_get_field;
#endif


	/*default field values*/
	p->alignment.vals = malloc(sizeof(SFInt32)*1);
	p->alignment.count = 1;
	p->alignment.vals[0] = 0;
	p->pathOffset = FLT2FIX(0);
	p->spacing = FLT2FIX(1.0);
	p->wrapMode = 0;
	p->splitText = 1;
	return (GF_Node *)p;
}


/*
	RadialGradient Node deletion
*/

static void RadialGradient_Del(GF_Node *node)
{
	M_RadialGradient *p = (M_RadialGradient *) node;
	gf_sg_mffloat_del(p->key);
	gf_sg_mfcolor_del(p->keyValue);
	gf_sg_mffloat_del(p->opacity);
	gf_node_unregister((GF_Node *) p->transform, (GF_Node *) p);	
	gf_node_free((GF_Node *) p);
}

static const u16 RadialGradient_Def2All[] = { 0, 1, 2, 3, 4, 5, 6, 7};
static const u16 RadialGradient_In2All[] = { 0, 1, 2, 3, 4, 5, 6, 7};
static const u16 RadialGradient_Out2All[] = { 0, 1, 2, 3, 4, 5, 6, 7};
static const u16 RadialGradient_Dyn2All[] = { 0, 1, 5};

static u32 RadialGradient_get_field_count(GF_Node *node, u8 IndexMode)
{
	switch(IndexMode) {
	case GF_SG_FIELD_CODING_IN: return 8;
	case GF_SG_FIELD_CODING_DEF: return 8;
	case GF_SG_FIELD_CODING_OUT: return 8;
	case GF_SG_FIELD_CODING_DYN: return 3;
	default:
		return 8;
	}
}

static GF_Err RadialGradient_get_field_index(GF_Node *n, u32 inField, u8 IndexMode, u32 *allField)
{
	switch(IndexMode) {
	case GF_SG_FIELD_CODING_IN:
		*allField = RadialGradient_In2All[inField];
		return GF_OK;
	case GF_SG_FIELD_CODING_DEF:
		*allField = RadialGradient_Def2All[inField];
		return GF_OK;
	case GF_SG_FIELD_CODING_OUT:
		*allField = RadialGradient_Out2All[inField];
		return GF_OK;
	case GF_SG_FIELD_CODING_DYN:
		*allField = RadialGradient_Dyn2All[inField];
		return GF_OK;
	default:
		return GF_BAD_PARAM;
	}
}
static GF_Err RadialGradient_get_field(GF_Node *node, GF_FieldInfo *info)
{
	switch (info->fieldIndex) {
	case 0:
		info->name = "center";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFVEC2F;
		info->far_ptr = & ((M_RadialGradient *) node)->center;
		return GF_OK;
	case 1:
		info->name = "focalPoint";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFVEC2F;
		info->far_ptr = & ((M_RadialGradient *) node)->focalPoint;
		return GF_OK;
	case 2:
		info->name = "key";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_MFFLOAT;
		info->far_ptr = & ((M_RadialGradient *) node)->key;
		return GF_OK;
	case 3:
		info->name = "keyValue";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_MFCOLOR;
		info->far_ptr = & ((M_RadialGradient *) node)->keyValue;
		return GF_OK;
	case 4:
		info->name = "opacity";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_MFFLOAT;
		info->far_ptr = & ((M_RadialGradient *) node)->opacity;
		return GF_OK;
	case 5:
		info->name = "radius";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFFLOAT;
		info->far_ptr = & ((M_RadialGradient *) node)->radius;
		return GF_OK;
	case 6:
		info->name = "spreadMethod";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFINT32;
		info->far_ptr = & ((M_RadialGradient *) node)->spreadMethod;
		return GF_OK;
	case 7:
		info->name = "transform";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFNODE;
		info->NDTtype = NDT_SF3DNode;
		info->far_ptr = & ((M_RadialGradient *)node)->transform;
		return GF_OK;
	default:
		return GF_BAD_PARAM;
	}
}

static Bool RadialGradient_get_aq_info(GF_Node *n, u32 FieldIndex, u8 *QType, u8 *AType, Fixed *b_min, Fixed *b_max, u32 *QT13_bits)
{
	switch (FieldIndex) {
	case 0:
		*AType = 2;
		*QType = 5;
		*b_min = FIX_MIN;
		*b_max = FIX_MAX;
		return 1;
	case 1:
		*AType = 2;
		*QType = 5;
		*b_min = FIX_MIN;
		*b_max = FIX_MAX;
		return 1;
	case 2:
		*AType = 0;
		*QType = 8;
		*b_min = FLT2FIX(0);
		*b_max = FLT2FIX(1);
		return 1;
	case 3:
		*AType = 0;
		*QType = 4;
		*b_min = FLT2FIX(0);
		*b_max = FLT2FIX(1);
		return 1;
	case 4:
		*AType = 0;
		*QType = 7;
		*b_min = FLT2FIX(0);
		*b_max = FLT2FIX(1);
		return 1;
	case 5:
		*AType = 7;
		*QType = 12;
		*b_min = FLT2FIX(0);
		*b_max = FIX_MAX;
		return 1;
	case 6:
		*AType = 0;
		*QType = 13;
		*QT13_bits = 2;
		*b_min = FLT2FIX(0);
		*b_max = FLT2FIX(2);
		return 1;
	default:
		return 0;
	}
}



GF_Node *RadialGradient_Create()
{
	M_RadialGradient *p;
	GF_SAFEALLOC(p, sizeof(M_RadialGradient));
	if(!p) return NULL;
	gf_node_setup((GF_Node *)p, TAG_MPEG4_RadialGradient);

#ifdef GF_NODE_USE_POINTERS
	((GF_Node *)p)->sgprivate->name = "RadialGradient";
	((GF_Node *)p)->sgprivate->node_del = RadialGradient_Del;
	((GF_Node *)p)->sgprivate->get_field_count = RadialGradient_get_field_count;
	((GF_Node *)p)->sgprivate->get_field = RadialGradient_get_field;
#endif


	/*default field values*/
	p->center.x = FLT2FIX(0.5);
	p->center.y = FLT2FIX(0.5);
	p->focalPoint.x = FLT2FIX(0);
	p->focalPoint.y = FLT2FIX(0);
	p->opacity.vals = malloc(sizeof(SFFloat)*1);
	p->opacity.count = 1;
	p->opacity.vals[0] = FLT2FIX(1);
	p->radius = FLT2FIX(0.5);
	p->spreadMethod = 0;
	return (GF_Node *)p;
}


/*
	TransformMatrix2D Node deletion
*/

static void TransformMatrix2D_Del(GF_Node *node)
{
	M_TransformMatrix2D *p = (M_TransformMatrix2D *) node;
	gf_sg_vrml_parent_reset((GF_Node *) p);	
	gf_node_free((GF_Node *) p);
}

static const u16 TransformMatrix2D_Def2All[] = { 2, 3, 4, 5, 6, 7, 8};
static const u16 TransformMatrix2D_In2All[] = { 0, 1, 2, 3, 4, 5, 6, 7, 8};
static const u16 TransformMatrix2D_Out2All[] = { 2, 3, 4, 5, 6, 7, 8};
static const u16 TransformMatrix2D_Dyn2All[] = { 3, 4, 5, 6, 7, 8};

static u32 TransformMatrix2D_get_field_count(GF_Node *node, u8 IndexMode)
{
	switch(IndexMode) {
	case GF_SG_FIELD_CODING_IN: return 9;
	case GF_SG_FIELD_CODING_DEF: return 7;
	case GF_SG_FIELD_CODING_OUT: return 7;
	case GF_SG_FIELD_CODING_DYN: return 6;
	default:
		return 9;
	}
}

static GF_Err TransformMatrix2D_get_field_index(GF_Node *n, u32 inField, u8 IndexMode, u32 *allField)
{
	switch(IndexMode) {
	case GF_SG_FIELD_CODING_IN:
		*allField = TransformMatrix2D_In2All[inField];
		return GF_OK;
	case GF_SG_FIELD_CODING_DEF:
		*allField = TransformMatrix2D_Def2All[inField];
		return GF_OK;
	case GF_SG_FIELD_CODING_OUT:
		*allField = TransformMatrix2D_Out2All[inField];
		return GF_OK;
	case GF_SG_FIELD_CODING_DYN:
		*allField = TransformMatrix2D_Dyn2All[inField];
		return GF_OK;
	default:
		return GF_BAD_PARAM;
	}
}
static GF_Err TransformMatrix2D_get_field(GF_Node *node, GF_FieldInfo *info)
{
	switch (info->fieldIndex) {
	case 0:
		info->name = "addChildren";
		info->eventType = GF_SG_EVENT_IN;
		info->on_event_in = ((M_TransformMatrix2D *)node)->on_addChildren;
		info->fieldType = GF_SG_VRML_MFNODE;
		info->NDTtype = NDT_SF2DNode;
		info->far_ptr = & ((M_TransformMatrix2D *)node)->addChildren;
		return GF_OK;
	case 1:
		info->name = "removeChildren";
		info->eventType = GF_SG_EVENT_IN;
		info->on_event_in = ((M_TransformMatrix2D *)node)->on_removeChildren;
		info->fieldType = GF_SG_VRML_MFNODE;
		info->NDTtype = NDT_SF2DNode;
		info->far_ptr = & ((M_TransformMatrix2D *)node)->removeChildren;
		return GF_OK;
	case 2:
		info->name = "children";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_MFNODE;
		info->NDTtype = NDT_SF2DNode;
		info->far_ptr = & ((M_TransformMatrix2D *)node)->children;
		return GF_OK;
	case 3:
		info->name = "mxx";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFFLOAT;
		info->far_ptr = & ((M_TransformMatrix2D *) node)->mxx;
		return GF_OK;
	case 4:
		info->name = "mxy";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFFLOAT;
		info->far_ptr = & ((M_TransformMatrix2D *) node)->mxy;
		return GF_OK;
	case 5:
		info->name = "tx";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFFLOAT;
		info->far_ptr = & ((M_TransformMatrix2D *) node)->tx;
		return GF_OK;
	case 6:
		info->name = "myx";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFFLOAT;
		info->far_ptr = & ((M_TransformMatrix2D *) node)->myx;
		return GF_OK;
	case 7:
		info->name = "myy";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFFLOAT;
		info->far_ptr = & ((M_TransformMatrix2D *) node)->myy;
		return GF_OK;
	case 8:
		info->name = "ty";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFFLOAT;
		info->far_ptr = & ((M_TransformMatrix2D *) node)->ty;
		return GF_OK;
	default:
		return GF_BAD_PARAM;
	}
}

static Bool TransformMatrix2D_get_aq_info(GF_Node *n, u32 FieldIndex, u8 *QType, u8 *AType, Fixed *b_min, Fixed *b_max, u32 *QT13_bits)
{
	switch (FieldIndex) {
	case 3:
		*AType = 7;
		*QType = 7;
		*b_min = FIX_MIN;
		*b_max = FIX_MAX;
		return 1;
	case 4:
		*AType = 7;
		*QType = 7;
		*b_min = FIX_MIN;
		*b_max = FIX_MAX;
		return 1;
	case 5:
		*AType = 7;
		*QType = 7;
		*b_min = FIX_MIN;
		*b_max = FIX_MAX;
		return 1;
	case 6:
		*AType = 7;
		*QType = 7;
		*b_min = FIX_MIN;
		*b_max = FIX_MAX;
		return 1;
	case 7:
		*AType = 7;
		*QType = 7;
		*b_min = FIX_MIN;
		*b_max = FIX_MAX;
		return 1;
	case 8:
		*AType = 7;
		*QType = 7;
		*b_min = FIX_MIN;
		*b_max = FIX_MAX;
		return 1;
	default:
		return 0;
	}
}



GF_Node *TransformMatrix2D_Create()
{
	M_TransformMatrix2D *p;
	GF_SAFEALLOC(p, sizeof(M_TransformMatrix2D));
	if(!p) return NULL;
	gf_node_setup((GF_Node *)p, TAG_MPEG4_TransformMatrix2D);
	gf_sg_vrml_parent_setup((GF_Node *) p);

#ifdef GF_NODE_USE_POINTERS
	((GF_Node *)p)->sgprivate->name = "TransformMatrix2D";
	((GF_Node *)p)->sgprivate->node_del = TransformMatrix2D_Del;
	((GF_Node *)p)->sgprivate->get_field_count = TransformMatrix2D_get_field_count;
	((GF_Node *)p)->sgprivate->get_field = TransformMatrix2D_get_field;
#endif


	/*default field values*/
	p->mxx = FLT2FIX(1);
	p->mxy = FLT2FIX(0);
	p->tx = FLT2FIX(0);
	p->myx = FLT2FIX(0);
	p->myy = FLT2FIX(1);
	p->ty = FLT2FIX(0);
	return (GF_Node *)p;
}


/*
	Viewport Node deletion
*/

static void Viewport_Del(GF_Node *node)
{
	M_Viewport *p = (M_Viewport *) node;
	gf_sg_mfint32_del(p->alignment);
	gf_sg_sfstring_del(p->description);
	gf_node_free((GF_Node *) p);
}

static const u16 Viewport_Def2All[] = { 1, 2, 3, 4, 5, 6};
static const u16 Viewport_In2All[] = { 0, 1, 2, 3, 4, 5};
static const u16 Viewport_Out2All[] = { 1, 2, 3, 4, 5, 7, 8};
static const u16 Viewport_Dyn2All[] = { 1, 2, 3};

static u32 Viewport_get_field_count(GF_Node *node, u8 IndexMode)
{
	switch(IndexMode) {
	case GF_SG_FIELD_CODING_IN: return 6;
	case GF_SG_FIELD_CODING_DEF: return 6;
	case GF_SG_FIELD_CODING_OUT: return 7;
	case GF_SG_FIELD_CODING_DYN: return 3;
	default:
		return 9;
	}
}

static GF_Err Viewport_get_field_index(GF_Node *n, u32 inField, u8 IndexMode, u32 *allField)
{
	switch(IndexMode) {
	case GF_SG_FIELD_CODING_IN:
		*allField = Viewport_In2All[inField];
		return GF_OK;
	case GF_SG_FIELD_CODING_DEF:
		*allField = Viewport_Def2All[inField];
		return GF_OK;
	case GF_SG_FIELD_CODING_OUT:
		*allField = Viewport_Out2All[inField];
		return GF_OK;
	case GF_SG_FIELD_CODING_DYN:
		*allField = Viewport_Dyn2All[inField];
		return GF_OK;
	default:
		return GF_BAD_PARAM;
	}
}
static GF_Err Viewport_get_field(GF_Node *node, GF_FieldInfo *info)
{
	switch (info->fieldIndex) {
	case 0:
		info->name = "set_bind";
		info->eventType = GF_SG_EVENT_IN;
		info->on_event_in = ((M_Viewport *)node)->on_set_bind;
		info->fieldType = GF_SG_VRML_SFBOOL;
		info->far_ptr = & ((M_Viewport *) node)->set_bind;
		return GF_OK;
	case 1:
		info->name = "position";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFVEC2F;
		info->far_ptr = & ((M_Viewport *) node)->position;
		return GF_OK;
	case 2:
		info->name = "size";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFVEC2F;
		info->far_ptr = & ((M_Viewport *) node)->size;
		return GF_OK;
	case 3:
		info->name = "orientation";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFFLOAT;
		info->far_ptr = & ((M_Viewport *) node)->orientation;
		return GF_OK;
	case 4:
		info->name = "alignment";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_MFINT32;
		info->far_ptr = & ((M_Viewport *) node)->alignment;
		return GF_OK;
	case 5:
		info->name = "fit";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFINT32;
		info->far_ptr = & ((M_Viewport *) node)->fit;
		return GF_OK;
	case 6:
		info->name = "description";
		info->eventType = GF_SG_EVENT_FIELD;
		info->fieldType = GF_SG_VRML_SFSTRING;
		info->far_ptr = & ((M_Viewport *) node)->description;
		return GF_OK;
	case 7:
		info->name = "bindTime";
		info->eventType = GF_SG_EVENT_OUT;
		info->fieldType = GF_SG_VRML_SFTIME;
		info->far_ptr = & ((M_Viewport *) node)->bindTime;
		return GF_OK;
	case 8:
		info->name = "isBound";
		info->eventType = GF_SG_EVENT_OUT;
		info->fieldType = GF_SG_VRML_SFBOOL;
		info->far_ptr = & ((M_Viewport *) node)->isBound;
		return GF_OK;
	default:
		return GF_BAD_PARAM;
	}
}

static Bool Viewport_get_aq_info(GF_Node *n, u32 FieldIndex, u8 *QType, u8 *AType, Fixed *b_min, Fixed *b_max, u32 *QT13_bits)
{
	switch (FieldIndex) {
	case 1:
		*AType = 1;
		*QType = 1;
		*b_min = FIX_MIN;
		*b_max = FIX_MAX;
		return 1;
	case 2:
		*AType = 12;
		*QType = 12;
		*b_min = FIX_MIN;
		*b_max = FIX_MAX;
		return 1;
	case 3:
		*AType = 6;
		*QType = 6;
		*b_min = FLT2FIX(0);
		*b_max = FLT2FIX(6.2831853);
		return 1;
	case 4:
		*AType = 0;
		*QType = 13;
		*QT13_bits = 3;
		*b_min = FLT2FIX(-1);
		*b_max = FLT2FIX(1);
		return 1;
	case 5:
		*AType = 0;
		*QType = 13;
		*QT13_bits = 3;
		*b_min = FLT2FIX(0);
		*b_max = FLT2FIX(2);
		return 1;
	default:
		return 0;
	}
}



GF_Node *Viewport_Create()
{
	M_Viewport *p;
	GF_SAFEALLOC(p, sizeof(M_Viewport));
	if(!p) return NULL;
	gf_node_setup((GF_Node *)p, TAG_MPEG4_Viewport);

#ifdef GF_NODE_USE_POINTERS
	((GF_Node *)p)->sgprivate->name = "Viewport";
	((GF_Node *)p)->sgprivate->node_del = Viewport_Del;
	((GF_Node *)p)->sgprivate->get_field_count = Viewport_get_field_count;
	((GF_Node *)p)->sgprivate->get_field = Viewport_get_field;
#endif


	/*default field values*/
	p->position.x = FLT2FIX(0);
	p->position.y = FLT2FIX(0);
	p->size.x = FLT2FIX(-1);
	p->size.y = FLT2FIX(-1);
	p->orientation = FLT2FIX(0);
	p->alignment.vals = malloc(sizeof(SFInt32)*1);
	p->alignment.count = 1;
	p->alignment.vals[0] = 0;
	p->fit = 0;
	return (GF_Node *)p;
}


/*
	XCurve2D Node deletion
*/

static void XCurve2D_Del(GF_Node *node)
{
	M_XCurve2D *p = (M_XCurve2D *) node;
	gf_node_unregister((GF_Node *) p->point, (GF_Node *) p);	
	gf_sg_mfint32_del(p->type);
	gf_node_free((GF_Node *) p);
}

static const u16 XCurve2D_Def2All[] = { 0, 1, 2};
static const u16 XCurve2D_In2All[] = { 0, 1, 2};
static const u16 XCurve2D_Out2All[] = { 0, 1, 2};
static const u16 XCurve2D_Dyn2All[] = { 1};

static u32 XCurve2D_get_field_count(GF_Node *node, u8 IndexMode)
{
	switch(IndexMode) {
	case GF_SG_FIELD_CODING_IN: return 3;
	case GF_SG_FIELD_CODING_DEF: return 3;
	case GF_SG_FIELD_CODING_OUT: return 3;
	case GF_SG_FIELD_CODING_DYN: return 1;
	default:
		return 3;
	}
}

static GF_Err XCurve2D_get_field_index(GF_Node *n, u32 inField, u8 IndexMode, u32 *allField)
{
	switch(IndexMode) {
	case GF_SG_FIELD_CODING_IN:
		*allField = XCurve2D_In2All[inField];
		return GF_OK;
	case GF_SG_FIELD_CODING_DEF:
		*allField = XCurve2D_Def2All[inField];
		return GF_OK;
	case GF_SG_FIELD_CODING_OUT:
		*allField = XCurve2D_Out2All[inField];
		return GF_OK;
	case GF_SG_FIELD_CODING_DYN:
		*allField = XCurve2D_Dyn2All[inField];
		return GF_OK;
	default:
		return GF_BAD_PARAM;
	}
}
static GF_Err XCurve2D_get_field(GF_Node *node, GF_FieldInfo *info)
{
	switch (info->fieldIndex) {
	case 0:
		info->name = "point";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFNODE;
		info->NDTtype = NDT_SFCoordinate2DNode;
		info->far_ptr = & ((M_XCurve2D *)node)->point;
		return GF_OK;
	case 1:
		info->name = "fineness";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFFLOAT;
		info->far_ptr = & ((M_XCurve2D *) node)->fineness;
		return GF_OK;
	case 2:
		info->name = "type";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_MFINT32;
		info->far_ptr = & ((M_XCurve2D *) node)->type;
		return GF_OK;
	default:
		return GF_BAD_PARAM;
	}
}

static Bool XCurve2D_get_aq_info(GF_Node *n, u32 FieldIndex, u8 *QType, u8 *AType, Fixed *b_min, Fixed *b_max, u32 *QT13_bits)
{
	switch (FieldIndex) {
	case 1:
		*AType = 7;
		*QType = 0;
		*b_min = FLT2FIX(0);
		*b_max = FLT2FIX(1);
		return 1;
	case 2:
		*AType = 0;
		*QType = 13;
		*QT13_bits = 4;
		*b_min = FLT2FIX(0);
		*b_max = FLT2FIX(15);
		return 1;
	default:
		return 0;
	}
}



GF_Node *XCurve2D_Create()
{
	M_XCurve2D *p;
	GF_SAFEALLOC(p, sizeof(M_XCurve2D));
	if(!p) return NULL;
	gf_node_setup((GF_Node *)p, TAG_MPEG4_XCurve2D);

#ifdef GF_NODE_USE_POINTERS
	((GF_Node *)p)->sgprivate->name = "XCurve2D";
	((GF_Node *)p)->sgprivate->node_del = XCurve2D_Del;
	((GF_Node *)p)->sgprivate->get_field_count = XCurve2D_get_field_count;
	((GF_Node *)p)->sgprivate->get_field = XCurve2D_get_field;
#endif


	/*default field values*/
	p->fineness = FLT2FIX(0.5);
	return (GF_Node *)p;
}


/*
	XFontStyle Node deletion
*/

static void XFontStyle_Del(GF_Node *node)
{
	M_XFontStyle *p = (M_XFontStyle *) node;
	gf_sg_mfstring_del(p->fontName);
	gf_sg_mfstring_del(p->justify);
	gf_sg_sfstring_del(p->language);
	gf_sg_sfstring_del(p->stretch);
	gf_sg_sfstring_del(p->style);
	gf_sg_mfstring_del(p->featureName);
	gf_sg_mfint32_del(p->featureStartOffset);
	gf_sg_mfint32_del(p->featureLength);
	gf_sg_mfint32_del(p->featureValue);
	gf_node_free((GF_Node *) p);
}

static const u16 XFontStyle_Def2All[] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16};
static const u16 XFontStyle_In2All[] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16};
static const u16 XFontStyle_Out2All[] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16};

static u32 XFontStyle_get_field_count(GF_Node *node, u8 IndexMode)
{
	switch(IndexMode) {
	case GF_SG_FIELD_CODING_IN: return 17;
	case GF_SG_FIELD_CODING_DEF: return 17;
	case GF_SG_FIELD_CODING_OUT: return 17;
	case GF_SG_FIELD_CODING_DYN: return 0;
	default:
		return 17;
	}
}

static GF_Err XFontStyle_get_field_index(GF_Node *n, u32 inField, u8 IndexMode, u32 *allField)
{
	switch(IndexMode) {
	case GF_SG_FIELD_CODING_IN:
		*allField = XFontStyle_In2All[inField];
		return GF_OK;
	case GF_SG_FIELD_CODING_DEF:
		*allField = XFontStyle_Def2All[inField];
		return GF_OK;
	case GF_SG_FIELD_CODING_OUT:
		*allField = XFontStyle_Out2All[inField];
		return GF_OK;
	default:
		return GF_BAD_PARAM;
	}
}
static GF_Err XFontStyle_get_field(GF_Node *node, GF_FieldInfo *info)
{
	switch (info->fieldIndex) {
	case 0:
		info->name = "fontName";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_MFSTRING;
		info->far_ptr = & ((M_XFontStyle *) node)->fontName;
		return GF_OK;
	case 1:
		info->name = "horizontal";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFBOOL;
		info->far_ptr = & ((M_XFontStyle *) node)->horizontal;
		return GF_OK;
	case 2:
		info->name = "justify";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_MFSTRING;
		info->far_ptr = & ((M_XFontStyle *) node)->justify;
		return GF_OK;
	case 3:
		info->name = "language";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFSTRING;
		info->far_ptr = & ((M_XFontStyle *) node)->language;
		return GF_OK;
	case 4:
		info->name = "leftToRight";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFBOOL;
		info->far_ptr = & ((M_XFontStyle *) node)->leftToRight;
		return GF_OK;
	case 5:
		info->name = "size";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFFLOAT;
		info->far_ptr = & ((M_XFontStyle *) node)->size;
		return GF_OK;
	case 6:
		info->name = "stretch";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFSTRING;
		info->far_ptr = & ((M_XFontStyle *) node)->stretch;
		return GF_OK;
	case 7:
		info->name = "letterSpacing";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFFLOAT;
		info->far_ptr = & ((M_XFontStyle *) node)->letterSpacing;
		return GF_OK;
	case 8:
		info->name = "wordSpacing";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFFLOAT;
		info->far_ptr = & ((M_XFontStyle *) node)->wordSpacing;
		return GF_OK;
	case 9:
		info->name = "weight";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFINT32;
		info->far_ptr = & ((M_XFontStyle *) node)->weight;
		return GF_OK;
	case 10:
		info->name = "fontKerning";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFBOOL;
		info->far_ptr = & ((M_XFontStyle *) node)->fontKerning;
		return GF_OK;
	case 11:
		info->name = "style";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFSTRING;
		info->far_ptr = & ((M_XFontStyle *) node)->style;
		return GF_OK;
	case 12:
		info->name = "topToBottom";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFBOOL;
		info->far_ptr = & ((M_XFontStyle *) node)->topToBottom;
		return GF_OK;
	case 13:
		info->name = "featureName";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_MFSTRING;
		info->far_ptr = & ((M_XFontStyle *) node)->featureName;
		return GF_OK;
	case 14:
		info->name = "featureStartOffset";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_MFINT32;
		info->far_ptr = & ((M_XFontStyle *) node)->featureStartOffset;
		return GF_OK;
	case 15:
		info->name = "featureLength";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_MFINT32;
		info->far_ptr = & ((M_XFontStyle *) node)->featureLength;
		return GF_OK;
	case 16:
		info->name = "featureValue";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_MFINT32;
		info->far_ptr = & ((M_XFontStyle *) node)->featureValue;
		return GF_OK;
	default:
		return GF_BAD_PARAM;
	}
}

static Bool XFontStyle_get_aq_info(GF_Node *n, u32 FieldIndex, u8 *QType, u8 *AType, Fixed *b_min, Fixed *b_max, u32 *QT13_bits)
{
	switch (FieldIndex) {
	case 5:
		*AType = 0;
		*QType = 11;
		*b_min = FLT2FIX(0);
		*b_max = FIX_MAX;
		return 1;
	case 7:
		*AType = 0;
		*QType = 11;
		*b_min = FLT2FIX(0);
		*b_max = FIX_MAX;
		return 1;
	case 8:
		*AType = 0;
		*QType = 11;
		*b_min = FLT2FIX(0);
		*b_max = FIX_MAX;
		return 1;
	default:
		return 0;
	}
}



GF_Node *XFontStyle_Create()
{
	M_XFontStyle *p;
	GF_SAFEALLOC(p, sizeof(M_XFontStyle));
	if(!p) return NULL;
	gf_node_setup((GF_Node *)p, TAG_MPEG4_XFontStyle);

#ifdef GF_NODE_USE_POINTERS
	((GF_Node *)p)->sgprivate->name = "XFontStyle";
	((GF_Node *)p)->sgprivate->node_del = XFontStyle_Del;
	((GF_Node *)p)->sgprivate->get_field_count = XFontStyle_get_field_count;
	((GF_Node *)p)->sgprivate->get_field = XFontStyle_get_field;
#endif


	/*default field values*/
	p->fontName.vals = malloc(sizeof(SFString)*1);
	p->fontName.count = 1;
	p->fontName.vals[0] = malloc(sizeof(char) * 6);
	strcpy(p->fontName.vals[0], "SERIF");
	p->horizontal = 1;
	p->justify.vals = malloc(sizeof(SFString)*1);
	p->justify.count = 1;
	p->justify.vals[0] = malloc(sizeof(char) * 6);
	strcpy(p->justify.vals[0], "BEGIN");
	p->leftToRight = 1;
	p->size = FLT2FIX(1.0);
	p->stretch.buffer = malloc(sizeof(char) * 7);
	strcpy(p->stretch.buffer, "NORMAL");
	p->letterSpacing = FLT2FIX(0.0);
	p->wordSpacing = FLT2FIX(0.0);
	p->weight = 400;
	p->fontKerning = 1;
	p->style.buffer = malloc(sizeof(char) * 6);
	strcpy(p->style.buffer, "PLAIN");
	p->topToBottom = 1;
	return (GF_Node *)p;
}


/*
	XLineProperties Node deletion
*/

static void XLineProperties_Del(GF_Node *node)
{
	M_XLineProperties *p = (M_XLineProperties *) node;
	gf_sg_mffloat_del(p->dashes);
	gf_node_unregister((GF_Node *) p->texture, (GF_Node *) p);	
	gf_node_unregister((GF_Node *) p->textureTransform, (GF_Node *) p);	
	gf_node_free((GF_Node *) p);
}

static const u16 XLineProperties_Def2All[] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12};
static const u16 XLineProperties_In2All[] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12};
static const u16 XLineProperties_Out2All[] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12};
static const u16 XLineProperties_Dyn2All[] = { 0, 7, 8, 9, 10};

static u32 XLineProperties_get_field_count(GF_Node *node, u8 IndexMode)
{
	switch(IndexMode) {
	case GF_SG_FIELD_CODING_IN: return 13;
	case GF_SG_FIELD_CODING_DEF: return 13;
	case GF_SG_FIELD_CODING_OUT: return 13;
	case GF_SG_FIELD_CODING_DYN: return 5;
	default:
		return 13;
	}
}

static GF_Err XLineProperties_get_field_index(GF_Node *n, u32 inField, u8 IndexMode, u32 *allField)
{
	switch(IndexMode) {
	case GF_SG_FIELD_CODING_IN:
		*allField = XLineProperties_In2All[inField];
		return GF_OK;
	case GF_SG_FIELD_CODING_DEF:
		*allField = XLineProperties_Def2All[inField];
		return GF_OK;
	case GF_SG_FIELD_CODING_OUT:
		*allField = XLineProperties_Out2All[inField];
		return GF_OK;
	case GF_SG_FIELD_CODING_DYN:
		*allField = XLineProperties_Dyn2All[inField];
		return GF_OK;
	default:
		return GF_BAD_PARAM;
	}
}
static GF_Err XLineProperties_get_field(GF_Node *node, GF_FieldInfo *info)
{
	switch (info->fieldIndex) {
	case 0:
		info->name = "lineColor";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFCOLOR;
		info->far_ptr = & ((M_XLineProperties *) node)->lineColor;
		return GF_OK;
	case 1:
		info->name = "lineStyle";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFINT32;
		info->far_ptr = & ((M_XLineProperties *) node)->lineStyle;
		return GF_OK;
	case 2:
		info->name = "isCenterAligned";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFBOOL;
		info->far_ptr = & ((M_XLineProperties *) node)->isCenterAligned;
		return GF_OK;
	case 3:
		info->name = "isScalable";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFBOOL;
		info->far_ptr = & ((M_XLineProperties *) node)->isScalable;
		return GF_OK;
	case 4:
		info->name = "lineCap";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFINT32;
		info->far_ptr = & ((M_XLineProperties *) node)->lineCap;
		return GF_OK;
	case 5:
		info->name = "lineJoin";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFINT32;
		info->far_ptr = & ((M_XLineProperties *) node)->lineJoin;
		return GF_OK;
	case 6:
		info->name = "miterLimit";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFFLOAT;
		info->far_ptr = & ((M_XLineProperties *) node)->miterLimit;
		return GF_OK;
	case 7:
		info->name = "transparency";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFFLOAT;
		info->far_ptr = & ((M_XLineProperties *) node)->transparency;
		return GF_OK;
	case 8:
		info->name = "width";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFFLOAT;
		info->far_ptr = & ((M_XLineProperties *) node)->width;
		return GF_OK;
	case 9:
		info->name = "dashOffset";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFFLOAT;
		info->far_ptr = & ((M_XLineProperties *) node)->dashOffset;
		return GF_OK;
	case 10:
		info->name = "dashes";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_MFFLOAT;
		info->far_ptr = & ((M_XLineProperties *) node)->dashes;
		return GF_OK;
	case 11:
		info->name = "texture";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFNODE;
		info->NDTtype = NDT_SFTextureNode;
		info->far_ptr = & ((M_XLineProperties *)node)->texture;
		return GF_OK;
	case 12:
		info->name = "textureTransform";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFNODE;
		info->NDTtype = NDT_SFTextureTransformNode;
		info->far_ptr = & ((M_XLineProperties *)node)->textureTransform;
		return GF_OK;
	default:
		return GF_BAD_PARAM;
	}
}

static Bool XLineProperties_get_aq_info(GF_Node *n, u32 FieldIndex, u8 *QType, u8 *AType, Fixed *b_min, Fixed *b_max, u32 *QT13_bits)
{
	switch (FieldIndex) {
	case 0:
		*AType = 4;
		*QType = 4;
		*b_min = FLT2FIX(0);
		*b_max = FLT2FIX(1);
		return 1;
	case 1:
		*AType = 0;
		*QType = 13;
		*QT13_bits = 3;
		*b_min = FLT2FIX(0);
		*b_max = FLT2FIX(5);
		return 1;
	case 4:
		*AType = 0;
		*QType = 13;
		*QT13_bits = 3;
		*b_min = FLT2FIX(0);
		*b_max = FLT2FIX(2);
		return 1;
	case 5:
		*AType = 0;
		*QType = 13;
		*QT13_bits = 3;
		*b_min = FLT2FIX(0);
		*b_max = FLT2FIX(2);
		return 1;
	case 6:
		*AType = 0;
		*QType = 12;
		*b_min = FLT2FIX(1);
		*b_max = FIX_MAX;
		return 1;
	case 7:
		*AType = 8;
		*QType = 4;
		*b_min = FLT2FIX(0);
		*b_max = FLT2FIX(1);
		return 1;
	case 8:
		*AType = 7;
		*QType = 12;
		*b_min = FLT2FIX(0);
		*b_max = FIX_MAX;
		return 1;
	case 9:
		*AType = 7;
		*QType = 12;
		*b_min = FLT2FIX(0);
		*b_max = FIX_MAX;
		return 1;
	case 10:
		*AType = 7;
		*QType = 12;
		*b_min = FLT2FIX(0);
		*b_max = FIX_MAX;
		return 1;
	default:
		return 0;
	}
}



GF_Node *XLineProperties_Create()
{
	M_XLineProperties *p;
	GF_SAFEALLOC(p, sizeof(M_XLineProperties));
	if(!p) return NULL;
	gf_node_setup((GF_Node *)p, TAG_MPEG4_XLineProperties);

#ifdef GF_NODE_USE_POINTERS
	((GF_Node *)p)->sgprivate->name = "XLineProperties";
	((GF_Node *)p)->sgprivate->node_del = XLineProperties_Del;
	((GF_Node *)p)->sgprivate->get_field_count = XLineProperties_get_field_count;
	((GF_Node *)p)->sgprivate->get_field = XLineProperties_get_field;
#endif


	/*default field values*/
	p->lineColor.red = FLT2FIX(0);
	p->lineColor.green = FLT2FIX(0);
	p->lineColor.blue = FLT2FIX(0);
	p->lineStyle = 0;
	p->isCenterAligned = 1;
	p->isScalable = 1;
	p->lineCap = 0;
	p->lineJoin = 0;
	p->miterLimit = FLT2FIX(4);
	p->transparency = FLT2FIX(0);
	p->width = FLT2FIX(1);
	p->dashOffset = FLT2FIX(0);
	return (GF_Node *)p;
}




GF_Node *gf_sg_mpeg4_node_new(u32 NodeTag)
{
	switch (NodeTag) {
	case TAG_MPEG4_Anchor:
		return Anchor_Create();
	case TAG_MPEG4_AnimationStream:
		return AnimationStream_Create();
	case TAG_MPEG4_Appearance:
		return Appearance_Create();
	case TAG_MPEG4_AudioBuffer:
		return AudioBuffer_Create();
	case TAG_MPEG4_AudioClip:
		return AudioClip_Create();
	case TAG_MPEG4_AudioDelay:
		return AudioDelay_Create();
	case TAG_MPEG4_AudioFX:
		return AudioFX_Create();
	case TAG_MPEG4_AudioMix:
		return AudioMix_Create();
	case TAG_MPEG4_AudioSource:
		return AudioSource_Create();
	case TAG_MPEG4_AudioSwitch:
		return AudioSwitch_Create();
	case TAG_MPEG4_Background:
		return Background_Create();
	case TAG_MPEG4_Background2D:
		return Background2D_Create();
	case TAG_MPEG4_Billboard:
		return Billboard_Create();
	case TAG_MPEG4_Bitmap:
		return Bitmap_Create();
	case TAG_MPEG4_Box:
		return Box_Create();
	case TAG_MPEG4_Circle:
		return Circle_Create();
	case TAG_MPEG4_Collision:
		return Collision_Create();
	case TAG_MPEG4_Color:
		return Color_Create();
	case TAG_MPEG4_ColorInterpolator:
		return ColorInterpolator_Create();
	case TAG_MPEG4_CompositeTexture2D:
		return CompositeTexture2D_Create();
	case TAG_MPEG4_CompositeTexture3D:
		return CompositeTexture3D_Create();
	case TAG_MPEG4_Conditional:
		return Conditional_Create();
	case TAG_MPEG4_Cone:
		return Cone_Create();
	case TAG_MPEG4_Coordinate:
		return Coordinate_Create();
	case TAG_MPEG4_Coordinate2D:
		return Coordinate2D_Create();
	case TAG_MPEG4_CoordinateInterpolator:
		return CoordinateInterpolator_Create();
	case TAG_MPEG4_CoordinateInterpolator2D:
		return CoordinateInterpolator2D_Create();
	case TAG_MPEG4_Curve2D:
		return Curve2D_Create();
	case TAG_MPEG4_Cylinder:
		return Cylinder_Create();
	case TAG_MPEG4_CylinderSensor:
		return CylinderSensor_Create();
	case TAG_MPEG4_DirectionalLight:
		return DirectionalLight_Create();
	case TAG_MPEG4_DiscSensor:
		return DiscSensor_Create();
	case TAG_MPEG4_ElevationGrid:
		return ElevationGrid_Create();
	case TAG_MPEG4_Extrusion:
		return Extrusion_Create();
	case TAG_MPEG4_Fog:
		return Fog_Create();
	case TAG_MPEG4_FontStyle:
		return FontStyle_Create();
	case TAG_MPEG4_Form:
		return Form_Create();
	case TAG_MPEG4_Group:
		return Group_Create();
	case TAG_MPEG4_ImageTexture:
		return ImageTexture_Create();
	case TAG_MPEG4_IndexedFaceSet:
		return IndexedFaceSet_Create();
	case TAG_MPEG4_IndexedFaceSet2D:
		return IndexedFaceSet2D_Create();
	case TAG_MPEG4_IndexedLineSet:
		return IndexedLineSet_Create();
	case TAG_MPEG4_IndexedLineSet2D:
		return IndexedLineSet2D_Create();
	case TAG_MPEG4_Inline:
		return Inline_Create();
	case TAG_MPEG4_LOD:
		return LOD_Create();
	case TAG_MPEG4_Layer2D:
		return Layer2D_Create();
	case TAG_MPEG4_Layer3D:
		return Layer3D_Create();
	case TAG_MPEG4_Layout:
		return Layout_Create();
	case TAG_MPEG4_LineProperties:
		return LineProperties_Create();
	case TAG_MPEG4_ListeningPoint:
		return ListeningPoint_Create();
	case TAG_MPEG4_Material:
		return Material_Create();
	case TAG_MPEG4_Material2D:
		return Material2D_Create();
	case TAG_MPEG4_MovieTexture:
		return MovieTexture_Create();
	case TAG_MPEG4_NavigationInfo:
		return NavigationInfo_Create();
	case TAG_MPEG4_Normal:
		return Normal_Create();
	case TAG_MPEG4_NormalInterpolator:
		return NormalInterpolator_Create();
	case TAG_MPEG4_OrderedGroup:
		return OrderedGroup_Create();
	case TAG_MPEG4_OrientationInterpolator:
		return OrientationInterpolator_Create();
	case TAG_MPEG4_PixelTexture:
		return PixelTexture_Create();
	case TAG_MPEG4_PlaneSensor:
		return PlaneSensor_Create();
	case TAG_MPEG4_PlaneSensor2D:
		return PlaneSensor2D_Create();
	case TAG_MPEG4_PointLight:
		return PointLight_Create();
	case TAG_MPEG4_PointSet:
		return PointSet_Create();
	case TAG_MPEG4_PointSet2D:
		return PointSet2D_Create();
	case TAG_MPEG4_PositionInterpolator:
		return PositionInterpolator_Create();
	case TAG_MPEG4_PositionInterpolator2D:
		return PositionInterpolator2D_Create();
	case TAG_MPEG4_ProximitySensor2D:
		return ProximitySensor2D_Create();
	case TAG_MPEG4_ProximitySensor:
		return ProximitySensor_Create();
	case TAG_MPEG4_QuantizationParameter:
		return QuantizationParameter_Create();
	case TAG_MPEG4_Rectangle:
		return Rectangle_Create();
	case TAG_MPEG4_ScalarInterpolator:
		return ScalarInterpolator_Create();
	case TAG_MPEG4_Script:
		return Script_Create();
	case TAG_MPEG4_Shape:
		return Shape_Create();
	case TAG_MPEG4_Sound:
		return Sound_Create();
	case TAG_MPEG4_Sound2D:
		return Sound2D_Create();
	case TAG_MPEG4_Sphere:
		return Sphere_Create();
	case TAG_MPEG4_SphereSensor:
		return SphereSensor_Create();
	case TAG_MPEG4_SpotLight:
		return SpotLight_Create();
	case TAG_MPEG4_Switch:
		return Switch_Create();
	case TAG_MPEG4_TermCap:
		return TermCap_Create();
	case TAG_MPEG4_Text:
		return Text_Create();
	case TAG_MPEG4_TextureCoordinate:
		return TextureCoordinate_Create();
	case TAG_MPEG4_TextureTransform:
		return TextureTransform_Create();
	case TAG_MPEG4_TimeSensor:
		return TimeSensor_Create();
	case TAG_MPEG4_TouchSensor:
		return TouchSensor_Create();
	case TAG_MPEG4_Transform:
		return Transform_Create();
	case TAG_MPEG4_Transform2D:
		return Transform2D_Create();
	case TAG_MPEG4_Valuator:
		return Valuator_Create();
	case TAG_MPEG4_Viewpoint:
		return Viewpoint_Create();
	case TAG_MPEG4_VisibilitySensor:
		return VisibilitySensor_Create();
	case TAG_MPEG4_WorldInfo:
		return WorldInfo_Create();
	case TAG_MPEG4_AcousticMaterial:
		return AcousticMaterial_Create();
	case TAG_MPEG4_AcousticScene:
		return AcousticScene_Create();
	case TAG_MPEG4_ApplicationWindow:
		return ApplicationWindow_Create();
	case TAG_MPEG4_DirectiveSound:
		return DirectiveSound_Create();
	case TAG_MPEG4_Hierarchical3DMesh:
		return Hierarchical3DMesh_Create();
	case TAG_MPEG4_MaterialKey:
		return MaterialKey_Create();
	case TAG_MPEG4_PerceptualParameters:
		return PerceptualParameters_Create();
	case TAG_MPEG4_TemporalTransform:
		return TemporalTransform_Create();
	case TAG_MPEG4_TemporalGroup:
		return TemporalGroup_Create();
	case TAG_MPEG4_ServerCommand:
		return ServerCommand_Create();
	case TAG_MPEG4_InputSensor:
		return InputSensor_Create();
	case TAG_MPEG4_MatteTexture:
		return MatteTexture_Create();
	case TAG_MPEG4_MediaBuffer:
		return MediaBuffer_Create();
	case TAG_MPEG4_MediaControl:
		return MediaControl_Create();
	case TAG_MPEG4_MediaSensor:
		return MediaSensor_Create();
	case TAG_MPEG4_CoordinateInterpolator4D:
		return CoordinateInterpolator4D_Create();
	case TAG_MPEG4_NonLinearDeformer:
		return NonLinearDeformer_Create();
	case TAG_MPEG4_PositionAnimator:
		return PositionAnimator_Create();
	case TAG_MPEG4_PositionAnimator2D:
		return PositionAnimator2D_Create();
	case TAG_MPEG4_PositionInterpolator4D:
		return PositionInterpolator4D_Create();
	case TAG_MPEG4_ScalarAnimator:
		return ScalarAnimator_Create();
	case TAG_MPEG4_Clipper2D:
		return Clipper2D_Create();
	case TAG_MPEG4_ColorTransform:
		return ColorTransform_Create();
	case TAG_MPEG4_Ellipse:
		return Ellipse_Create();
	case TAG_MPEG4_LinearGradient:
		return LinearGradient_Create();
	case TAG_MPEG4_PathLayout:
		return PathLayout_Create();
	case TAG_MPEG4_RadialGradient:
		return RadialGradient_Create();
	case TAG_MPEG4_TransformMatrix2D:
		return TransformMatrix2D_Create();
	case TAG_MPEG4_Viewport:
		return Viewport_Create();
	case TAG_MPEG4_XCurve2D:
		return XCurve2D_Create();
	case TAG_MPEG4_XFontStyle:
		return XFontStyle_Create();
	case TAG_MPEG4_XLineProperties:
		return XLineProperties_Create();
	default:
		return NULL;
	}
}

#ifndef GF_NODE_USE_POINTERS
const char *gf_sg_mpeg4_node_get_class_name(u32 NodeTag)
{
	switch (NodeTag) {
	case TAG_MPEG4_Anchor:
		return "Anchor";
	case TAG_MPEG4_AnimationStream:
		return "AnimationStream";
	case TAG_MPEG4_Appearance:
		return "Appearance";
	case TAG_MPEG4_AudioBuffer:
		return "AudioBuffer";
	case TAG_MPEG4_AudioClip:
		return "AudioClip";
	case TAG_MPEG4_AudioDelay:
		return "AudioDelay";
	case TAG_MPEG4_AudioFX:
		return "AudioFX";
	case TAG_MPEG4_AudioMix:
		return "AudioMix";
	case TAG_MPEG4_AudioSource:
		return "AudioSource";
	case TAG_MPEG4_AudioSwitch:
		return "AudioSwitch";
	case TAG_MPEG4_Background:
		return "Background";
	case TAG_MPEG4_Background2D:
		return "Background2D";
	case TAG_MPEG4_Billboard:
		return "Billboard";
	case TAG_MPEG4_Bitmap:
		return "Bitmap";
	case TAG_MPEG4_Box:
		return "Box";
	case TAG_MPEG4_Circle:
		return "Circle";
	case TAG_MPEG4_Collision:
		return "Collision";
	case TAG_MPEG4_Color:
		return "Color";
	case TAG_MPEG4_ColorInterpolator:
		return "ColorInterpolator";
	case TAG_MPEG4_CompositeTexture2D:
		return "CompositeTexture2D";
	case TAG_MPEG4_CompositeTexture3D:
		return "CompositeTexture3D";
	case TAG_MPEG4_Conditional:
		return "Conditional";
	case TAG_MPEG4_Cone:
		return "Cone";
	case TAG_MPEG4_Coordinate:
		return "Coordinate";
	case TAG_MPEG4_Coordinate2D:
		return "Coordinate2D";
	case TAG_MPEG4_CoordinateInterpolator:
		return "CoordinateInterpolator";
	case TAG_MPEG4_CoordinateInterpolator2D:
		return "CoordinateInterpolator2D";
	case TAG_MPEG4_Curve2D:
		return "Curve2D";
	case TAG_MPEG4_Cylinder:
		return "Cylinder";
	case TAG_MPEG4_CylinderSensor:
		return "CylinderSensor";
	case TAG_MPEG4_DirectionalLight:
		return "DirectionalLight";
	case TAG_MPEG4_DiscSensor:
		return "DiscSensor";
	case TAG_MPEG4_ElevationGrid:
		return "ElevationGrid";
	case TAG_MPEG4_Extrusion:
		return "Extrusion";
	case TAG_MPEG4_Fog:
		return "Fog";
	case TAG_MPEG4_FontStyle:
		return "FontStyle";
	case TAG_MPEG4_Form:
		return "Form";
	case TAG_MPEG4_Group:
		return "Group";
	case TAG_MPEG4_ImageTexture:
		return "ImageTexture";
	case TAG_MPEG4_IndexedFaceSet:
		return "IndexedFaceSet";
	case TAG_MPEG4_IndexedFaceSet2D:
		return "IndexedFaceSet2D";
	case TAG_MPEG4_IndexedLineSet:
		return "IndexedLineSet";
	case TAG_MPEG4_IndexedLineSet2D:
		return "IndexedLineSet2D";
	case TAG_MPEG4_Inline:
		return "Inline";
	case TAG_MPEG4_LOD:
		return "LOD";
	case TAG_MPEG4_Layer2D:
		return "Layer2D";
	case TAG_MPEG4_Layer3D:
		return "Layer3D";
	case TAG_MPEG4_Layout:
		return "Layout";
	case TAG_MPEG4_LineProperties:
		return "LineProperties";
	case TAG_MPEG4_ListeningPoint:
		return "ListeningPoint";
	case TAG_MPEG4_Material:
		return "Material";
	case TAG_MPEG4_Material2D:
		return "Material2D";
	case TAG_MPEG4_MovieTexture:
		return "MovieTexture";
	case TAG_MPEG4_NavigationInfo:
		return "NavigationInfo";
	case TAG_MPEG4_Normal:
		return "Normal";
	case TAG_MPEG4_NormalInterpolator:
		return "NormalInterpolator";
	case TAG_MPEG4_OrderedGroup:
		return "OrderedGroup";
	case TAG_MPEG4_OrientationInterpolator:
		return "OrientationInterpolator";
	case TAG_MPEG4_PixelTexture:
		return "PixelTexture";
	case TAG_MPEG4_PlaneSensor:
		return "PlaneSensor";
	case TAG_MPEG4_PlaneSensor2D:
		return "PlaneSensor2D";
	case TAG_MPEG4_PointLight:
		return "PointLight";
	case TAG_MPEG4_PointSet:
		return "PointSet";
	case TAG_MPEG4_PointSet2D:
		return "PointSet2D";
	case TAG_MPEG4_PositionInterpolator:
		return "PositionInterpolator";
	case TAG_MPEG4_PositionInterpolator2D:
		return "PositionInterpolator2D";
	case TAG_MPEG4_ProximitySensor2D:
		return "ProximitySensor2D";
	case TAG_MPEG4_ProximitySensor:
		return "ProximitySensor";
	case TAG_MPEG4_QuantizationParameter:
		return "QuantizationParameter";
	case TAG_MPEG4_Rectangle:
		return "Rectangle";
	case TAG_MPEG4_ScalarInterpolator:
		return "ScalarInterpolator";
	case TAG_MPEG4_Script:
		return "Script";
	case TAG_MPEG4_Shape:
		return "Shape";
	case TAG_MPEG4_Sound:
		return "Sound";
	case TAG_MPEG4_Sound2D:
		return "Sound2D";
	case TAG_MPEG4_Sphere:
		return "Sphere";
	case TAG_MPEG4_SphereSensor:
		return "SphereSensor";
	case TAG_MPEG4_SpotLight:
		return "SpotLight";
	case TAG_MPEG4_Switch:
		return "Switch";
	case TAG_MPEG4_TermCap:
		return "TermCap";
	case TAG_MPEG4_Text:
		return "Text";
	case TAG_MPEG4_TextureCoordinate:
		return "TextureCoordinate";
	case TAG_MPEG4_TextureTransform:
		return "TextureTransform";
	case TAG_MPEG4_TimeSensor:
		return "TimeSensor";
	case TAG_MPEG4_TouchSensor:
		return "TouchSensor";
	case TAG_MPEG4_Transform:
		return "Transform";
	case TAG_MPEG4_Transform2D:
		return "Transform2D";
	case TAG_MPEG4_Valuator:
		return "Valuator";
	case TAG_MPEG4_Viewpoint:
		return "Viewpoint";
	case TAG_MPEG4_VisibilitySensor:
		return "VisibilitySensor";
	case TAG_MPEG4_WorldInfo:
		return "WorldInfo";
	case TAG_MPEG4_AcousticMaterial:
		return "AcousticMaterial";
	case TAG_MPEG4_AcousticScene:
		return "AcousticScene";
	case TAG_MPEG4_ApplicationWindow:
		return "ApplicationWindow";
	case TAG_MPEG4_DirectiveSound:
		return "DirectiveSound";
	case TAG_MPEG4_Hierarchical3DMesh:
		return "Hierarchical3DMesh";
	case TAG_MPEG4_MaterialKey:
		return "MaterialKey";
	case TAG_MPEG4_PerceptualParameters:
		return "PerceptualParameters";
	case TAG_MPEG4_TemporalTransform:
		return "TemporalTransform";
	case TAG_MPEG4_TemporalGroup:
		return "TemporalGroup";
	case TAG_MPEG4_ServerCommand:
		return "ServerCommand";
	case TAG_MPEG4_InputSensor:
		return "InputSensor";
	case TAG_MPEG4_MatteTexture:
		return "MatteTexture";
	case TAG_MPEG4_MediaBuffer:
		return "MediaBuffer";
	case TAG_MPEG4_MediaControl:
		return "MediaControl";
	case TAG_MPEG4_MediaSensor:
		return "MediaSensor";
	case TAG_MPEG4_CoordinateInterpolator4D:
		return "CoordinateInterpolator4D";
	case TAG_MPEG4_NonLinearDeformer:
		return "NonLinearDeformer";
	case TAG_MPEG4_PositionAnimator:
		return "PositionAnimator";
	case TAG_MPEG4_PositionAnimator2D:
		return "PositionAnimator2D";
	case TAG_MPEG4_PositionInterpolator4D:
		return "PositionInterpolator4D";
	case TAG_MPEG4_ScalarAnimator:
		return "ScalarAnimator";
	case TAG_MPEG4_Clipper2D:
		return "Clipper2D";
	case TAG_MPEG4_ColorTransform:
		return "ColorTransform";
	case TAG_MPEG4_Ellipse:
		return "Ellipse";
	case TAG_MPEG4_LinearGradient:
		return "LinearGradient";
	case TAG_MPEG4_PathLayout:
		return "PathLayout";
	case TAG_MPEG4_RadialGradient:
		return "RadialGradient";
	case TAG_MPEG4_TransformMatrix2D:
		return "TransformMatrix2D";
	case TAG_MPEG4_Viewport:
		return "Viewport";
	case TAG_MPEG4_XCurve2D:
		return "XCurve2D";
	case TAG_MPEG4_XFontStyle:
		return "XFontStyle";
	case TAG_MPEG4_XLineProperties:
		return "XLineProperties";
	default:
		return "Unknown Node";
	}
}

void gf_sg_mpeg4_node_del(GF_Node *node)
{
	switch (node->sgprivate->tag) {
	case TAG_MPEG4_Anchor:
		Anchor_Del(node); return;
	case TAG_MPEG4_AnimationStream:
		AnimationStream_Del(node); return;
	case TAG_MPEG4_Appearance:
		Appearance_Del(node); return;
	case TAG_MPEG4_AudioBuffer:
		AudioBuffer_Del(node); return;
	case TAG_MPEG4_AudioClip:
		AudioClip_Del(node); return;
	case TAG_MPEG4_AudioDelay:
		AudioDelay_Del(node); return;
	case TAG_MPEG4_AudioFX:
		AudioFX_Del(node); return;
	case TAG_MPEG4_AudioMix:
		AudioMix_Del(node); return;
	case TAG_MPEG4_AudioSource:
		AudioSource_Del(node); return;
	case TAG_MPEG4_AudioSwitch:
		AudioSwitch_Del(node); return;
	case TAG_MPEG4_Background:
		Background_Del(node); return;
	case TAG_MPEG4_Background2D:
		Background2D_Del(node); return;
	case TAG_MPEG4_Billboard:
		Billboard_Del(node); return;
	case TAG_MPEG4_Bitmap:
		Bitmap_Del(node); return;
	case TAG_MPEG4_Box:
		Box_Del(node); return;
	case TAG_MPEG4_Circle:
		Circle_Del(node); return;
	case TAG_MPEG4_Collision:
		Collision_Del(node); return;
	case TAG_MPEG4_Color:
		Color_Del(node); return;
	case TAG_MPEG4_ColorInterpolator:
		ColorInterpolator_Del(node); return;
	case TAG_MPEG4_CompositeTexture2D:
		CompositeTexture2D_Del(node); return;
	case TAG_MPEG4_CompositeTexture3D:
		CompositeTexture3D_Del(node); return;
	case TAG_MPEG4_Conditional:
		Conditional_Del(node); return;
	case TAG_MPEG4_Cone:
		Cone_Del(node); return;
	case TAG_MPEG4_Coordinate:
		Coordinate_Del(node); return;
	case TAG_MPEG4_Coordinate2D:
		Coordinate2D_Del(node); return;
	case TAG_MPEG4_CoordinateInterpolator:
		CoordinateInterpolator_Del(node); return;
	case TAG_MPEG4_CoordinateInterpolator2D:
		CoordinateInterpolator2D_Del(node); return;
	case TAG_MPEG4_Curve2D:
		Curve2D_Del(node); return;
	case TAG_MPEG4_Cylinder:
		Cylinder_Del(node); return;
	case TAG_MPEG4_CylinderSensor:
		CylinderSensor_Del(node); return;
	case TAG_MPEG4_DirectionalLight:
		DirectionalLight_Del(node); return;
	case TAG_MPEG4_DiscSensor:
		DiscSensor_Del(node); return;
	case TAG_MPEG4_ElevationGrid:
		ElevationGrid_Del(node); return;
	case TAG_MPEG4_Extrusion:
		Extrusion_Del(node); return;
	case TAG_MPEG4_Fog:
		Fog_Del(node); return;
	case TAG_MPEG4_FontStyle:
		FontStyle_Del(node); return;
	case TAG_MPEG4_Form:
		Form_Del(node); return;
	case TAG_MPEG4_Group:
		Group_Del(node); return;
	case TAG_MPEG4_ImageTexture:
		ImageTexture_Del(node); return;
	case TAG_MPEG4_IndexedFaceSet:
		IndexedFaceSet_Del(node); return;
	case TAG_MPEG4_IndexedFaceSet2D:
		IndexedFaceSet2D_Del(node); return;
	case TAG_MPEG4_IndexedLineSet:
		IndexedLineSet_Del(node); return;
	case TAG_MPEG4_IndexedLineSet2D:
		IndexedLineSet2D_Del(node); return;
	case TAG_MPEG4_Inline:
		Inline_Del(node); return;
	case TAG_MPEG4_LOD:
		LOD_Del(node); return;
	case TAG_MPEG4_Layer2D:
		Layer2D_Del(node); return;
	case TAG_MPEG4_Layer3D:
		Layer3D_Del(node); return;
	case TAG_MPEG4_Layout:
		Layout_Del(node); return;
	case TAG_MPEG4_LineProperties:
		LineProperties_Del(node); return;
	case TAG_MPEG4_ListeningPoint:
		ListeningPoint_Del(node); return;
	case TAG_MPEG4_Material:
		Material_Del(node); return;
	case TAG_MPEG4_Material2D:
		Material2D_Del(node); return;
	case TAG_MPEG4_MovieTexture:
		MovieTexture_Del(node); return;
	case TAG_MPEG4_NavigationInfo:
		NavigationInfo_Del(node); return;
	case TAG_MPEG4_Normal:
		Normal_Del(node); return;
	case TAG_MPEG4_NormalInterpolator:
		NormalInterpolator_Del(node); return;
	case TAG_MPEG4_OrderedGroup:
		OrderedGroup_Del(node); return;
	case TAG_MPEG4_OrientationInterpolator:
		OrientationInterpolator_Del(node); return;
	case TAG_MPEG4_PixelTexture:
		PixelTexture_Del(node); return;
	case TAG_MPEG4_PlaneSensor:
		PlaneSensor_Del(node); return;
	case TAG_MPEG4_PlaneSensor2D:
		PlaneSensor2D_Del(node); return;
	case TAG_MPEG4_PointLight:
		PointLight_Del(node); return;
	case TAG_MPEG4_PointSet:
		PointSet_Del(node); return;
	case TAG_MPEG4_PointSet2D:
		PointSet2D_Del(node); return;
	case TAG_MPEG4_PositionInterpolator:
		PositionInterpolator_Del(node); return;
	case TAG_MPEG4_PositionInterpolator2D:
		PositionInterpolator2D_Del(node); return;
	case TAG_MPEG4_ProximitySensor2D:
		ProximitySensor2D_Del(node); return;
	case TAG_MPEG4_ProximitySensor:
		ProximitySensor_Del(node); return;
	case TAG_MPEG4_QuantizationParameter:
		QuantizationParameter_Del(node); return;
	case TAG_MPEG4_Rectangle:
		Rectangle_Del(node); return;
	case TAG_MPEG4_ScalarInterpolator:
		ScalarInterpolator_Del(node); return;
	case TAG_MPEG4_Script:
		Script_Del(node); return;
	case TAG_MPEG4_Shape:
		Shape_Del(node); return;
	case TAG_MPEG4_Sound:
		Sound_Del(node); return;
	case TAG_MPEG4_Sound2D:
		Sound2D_Del(node); return;
	case TAG_MPEG4_Sphere:
		Sphere_Del(node); return;
	case TAG_MPEG4_SphereSensor:
		SphereSensor_Del(node); return;
	case TAG_MPEG4_SpotLight:
		SpotLight_Del(node); return;
	case TAG_MPEG4_Switch:
		Switch_Del(node); return;
	case TAG_MPEG4_TermCap:
		TermCap_Del(node); return;
	case TAG_MPEG4_Text:
		Text_Del(node); return;
	case TAG_MPEG4_TextureCoordinate:
		TextureCoordinate_Del(node); return;
	case TAG_MPEG4_TextureTransform:
		TextureTransform_Del(node); return;
	case TAG_MPEG4_TimeSensor:
		TimeSensor_Del(node); return;
	case TAG_MPEG4_TouchSensor:
		TouchSensor_Del(node); return;
	case TAG_MPEG4_Transform:
		Transform_Del(node); return;
	case TAG_MPEG4_Transform2D:
		Transform2D_Del(node); return;
	case TAG_MPEG4_Valuator:
		Valuator_Del(node); return;
	case TAG_MPEG4_Viewpoint:
		Viewpoint_Del(node); return;
	case TAG_MPEG4_VisibilitySensor:
		VisibilitySensor_Del(node); return;
	case TAG_MPEG4_WorldInfo:
		WorldInfo_Del(node); return;
	case TAG_MPEG4_AcousticMaterial:
		AcousticMaterial_Del(node); return;
	case TAG_MPEG4_AcousticScene:
		AcousticScene_Del(node); return;
	case TAG_MPEG4_ApplicationWindow:
		ApplicationWindow_Del(node); return;
	case TAG_MPEG4_DirectiveSound:
		DirectiveSound_Del(node); return;
	case TAG_MPEG4_Hierarchical3DMesh:
		Hierarchical3DMesh_Del(node); return;
	case TAG_MPEG4_MaterialKey:
		MaterialKey_Del(node); return;
	case TAG_MPEG4_PerceptualParameters:
		PerceptualParameters_Del(node); return;
	case TAG_MPEG4_TemporalTransform:
		TemporalTransform_Del(node); return;
	case TAG_MPEG4_TemporalGroup:
		TemporalGroup_Del(node); return;
	case TAG_MPEG4_ServerCommand:
		ServerCommand_Del(node); return;
	case TAG_MPEG4_InputSensor:
		InputSensor_Del(node); return;
	case TAG_MPEG4_MatteTexture:
		MatteTexture_Del(node); return;
	case TAG_MPEG4_MediaBuffer:
		MediaBuffer_Del(node); return;
	case TAG_MPEG4_MediaControl:
		MediaControl_Del(node); return;
	case TAG_MPEG4_MediaSensor:
		MediaSensor_Del(node); return;
	case TAG_MPEG4_CoordinateInterpolator4D:
		CoordinateInterpolator4D_Del(node); return;
	case TAG_MPEG4_NonLinearDeformer:
		NonLinearDeformer_Del(node); return;
	case TAG_MPEG4_PositionAnimator:
		PositionAnimator_Del(node); return;
	case TAG_MPEG4_PositionAnimator2D:
		PositionAnimator2D_Del(node); return;
	case TAG_MPEG4_PositionInterpolator4D:
		PositionInterpolator4D_Del(node); return;
	case TAG_MPEG4_ScalarAnimator:
		ScalarAnimator_Del(node); return;
	case TAG_MPEG4_Clipper2D:
		Clipper2D_Del(node); return;
	case TAG_MPEG4_ColorTransform:
		ColorTransform_Del(node); return;
	case TAG_MPEG4_Ellipse:
		Ellipse_Del(node); return;
	case TAG_MPEG4_LinearGradient:
		LinearGradient_Del(node); return;
	case TAG_MPEG4_PathLayout:
		PathLayout_Del(node); return;
	case TAG_MPEG4_RadialGradient:
		RadialGradient_Del(node); return;
	case TAG_MPEG4_TransformMatrix2D:
		TransformMatrix2D_Del(node); return;
	case TAG_MPEG4_Viewport:
		Viewport_Del(node); return;
	case TAG_MPEG4_XCurve2D:
		XCurve2D_Del(node); return;
	case TAG_MPEG4_XFontStyle:
		XFontStyle_Del(node); return;
	case TAG_MPEG4_XLineProperties:
		XLineProperties_Del(node); return;
	default:
		return;
	}
}

u32 gf_sg_mpeg4_node_get_field_count(GF_Node *node, u8 code_mode)
{
	switch (node->sgprivate->tag) {
	case TAG_MPEG4_Anchor:return Anchor_get_field_count(node, code_mode);
	case TAG_MPEG4_AnimationStream:return AnimationStream_get_field_count(node, code_mode);
	case TAG_MPEG4_Appearance:return Appearance_get_field_count(node, code_mode);
	case TAG_MPEG4_AudioBuffer:return AudioBuffer_get_field_count(node, code_mode);
	case TAG_MPEG4_AudioClip:return AudioClip_get_field_count(node, code_mode);
	case TAG_MPEG4_AudioDelay:return AudioDelay_get_field_count(node, code_mode);
	case TAG_MPEG4_AudioFX:return AudioFX_get_field_count(node, code_mode);
	case TAG_MPEG4_AudioMix:return AudioMix_get_field_count(node, code_mode);
	case TAG_MPEG4_AudioSource:return AudioSource_get_field_count(node, code_mode);
	case TAG_MPEG4_AudioSwitch:return AudioSwitch_get_field_count(node, code_mode);
	case TAG_MPEG4_Background:return Background_get_field_count(node, code_mode);
	case TAG_MPEG4_Background2D:return Background2D_get_field_count(node, code_mode);
	case TAG_MPEG4_Billboard:return Billboard_get_field_count(node, code_mode);
	case TAG_MPEG4_Bitmap:return Bitmap_get_field_count(node, code_mode);
	case TAG_MPEG4_Box:return Box_get_field_count(node, code_mode);
	case TAG_MPEG4_Circle:return Circle_get_field_count(node, code_mode);
	case TAG_MPEG4_Collision:return Collision_get_field_count(node, code_mode);
	case TAG_MPEG4_Color:return Color_get_field_count(node, code_mode);
	case TAG_MPEG4_ColorInterpolator:return ColorInterpolator_get_field_count(node, code_mode);
	case TAG_MPEG4_CompositeTexture2D:return CompositeTexture2D_get_field_count(node, code_mode);
	case TAG_MPEG4_CompositeTexture3D:return CompositeTexture3D_get_field_count(node, code_mode);
	case TAG_MPEG4_Conditional:return Conditional_get_field_count(node, code_mode);
	case TAG_MPEG4_Cone:return Cone_get_field_count(node, code_mode);
	case TAG_MPEG4_Coordinate:return Coordinate_get_field_count(node, code_mode);
	case TAG_MPEG4_Coordinate2D:return Coordinate2D_get_field_count(node, code_mode);
	case TAG_MPEG4_CoordinateInterpolator:return CoordinateInterpolator_get_field_count(node, code_mode);
	case TAG_MPEG4_CoordinateInterpolator2D:return CoordinateInterpolator2D_get_field_count(node, code_mode);
	case TAG_MPEG4_Curve2D:return Curve2D_get_field_count(node, code_mode);
	case TAG_MPEG4_Cylinder:return Cylinder_get_field_count(node, code_mode);
	case TAG_MPEG4_CylinderSensor:return CylinderSensor_get_field_count(node, code_mode);
	case TAG_MPEG4_DirectionalLight:return DirectionalLight_get_field_count(node, code_mode);
	case TAG_MPEG4_DiscSensor:return DiscSensor_get_field_count(node, code_mode);
	case TAG_MPEG4_ElevationGrid:return ElevationGrid_get_field_count(node, code_mode);
	case TAG_MPEG4_Extrusion:return Extrusion_get_field_count(node, code_mode);
	case TAG_MPEG4_Fog:return Fog_get_field_count(node, code_mode);
	case TAG_MPEG4_FontStyle:return FontStyle_get_field_count(node, code_mode);
	case TAG_MPEG4_Form:return Form_get_field_count(node, code_mode);
	case TAG_MPEG4_Group:return Group_get_field_count(node, code_mode);
	case TAG_MPEG4_ImageTexture:return ImageTexture_get_field_count(node, code_mode);
	case TAG_MPEG4_IndexedFaceSet:return IndexedFaceSet_get_field_count(node, code_mode);
	case TAG_MPEG4_IndexedFaceSet2D:return IndexedFaceSet2D_get_field_count(node, code_mode);
	case TAG_MPEG4_IndexedLineSet:return IndexedLineSet_get_field_count(node, code_mode);
	case TAG_MPEG4_IndexedLineSet2D:return IndexedLineSet2D_get_field_count(node, code_mode);
	case TAG_MPEG4_Inline:return Inline_get_field_count(node, code_mode);
	case TAG_MPEG4_LOD:return LOD_get_field_count(node, code_mode);
	case TAG_MPEG4_Layer2D:return Layer2D_get_field_count(node, code_mode);
	case TAG_MPEG4_Layer3D:return Layer3D_get_field_count(node, code_mode);
	case TAG_MPEG4_Layout:return Layout_get_field_count(node, code_mode);
	case TAG_MPEG4_LineProperties:return LineProperties_get_field_count(node, code_mode);
	case TAG_MPEG4_ListeningPoint:return ListeningPoint_get_field_count(node, code_mode);
	case TAG_MPEG4_Material:return Material_get_field_count(node, code_mode);
	case TAG_MPEG4_Material2D:return Material2D_get_field_count(node, code_mode);
	case TAG_MPEG4_MovieTexture:return MovieTexture_get_field_count(node, code_mode);
	case TAG_MPEG4_NavigationInfo:return NavigationInfo_get_field_count(node, code_mode);
	case TAG_MPEG4_Normal:return Normal_get_field_count(node, code_mode);
	case TAG_MPEG4_NormalInterpolator:return NormalInterpolator_get_field_count(node, code_mode);
	case TAG_MPEG4_OrderedGroup:return OrderedGroup_get_field_count(node, code_mode);
	case TAG_MPEG4_OrientationInterpolator:return OrientationInterpolator_get_field_count(node, code_mode);
	case TAG_MPEG4_PixelTexture:return PixelTexture_get_field_count(node, code_mode);
	case TAG_MPEG4_PlaneSensor:return PlaneSensor_get_field_count(node, code_mode);
	case TAG_MPEG4_PlaneSensor2D:return PlaneSensor2D_get_field_count(node, code_mode);
	case TAG_MPEG4_PointLight:return PointLight_get_field_count(node, code_mode);
	case TAG_MPEG4_PointSet:return PointSet_get_field_count(node, code_mode);
	case TAG_MPEG4_PointSet2D:return PointSet2D_get_field_count(node, code_mode);
	case TAG_MPEG4_PositionInterpolator:return PositionInterpolator_get_field_count(node, code_mode);
	case TAG_MPEG4_PositionInterpolator2D:return PositionInterpolator2D_get_field_count(node, code_mode);
	case TAG_MPEG4_ProximitySensor2D:return ProximitySensor2D_get_field_count(node, code_mode);
	case TAG_MPEG4_ProximitySensor:return ProximitySensor_get_field_count(node, code_mode);
	case TAG_MPEG4_QuantizationParameter:return QuantizationParameter_get_field_count(node, code_mode);
	case TAG_MPEG4_Rectangle:return Rectangle_get_field_count(node, code_mode);
	case TAG_MPEG4_ScalarInterpolator:return ScalarInterpolator_get_field_count(node, code_mode);
	case TAG_MPEG4_Script:return Script_get_field_count(node, code_mode);
	case TAG_MPEG4_Shape:return Shape_get_field_count(node, code_mode);
	case TAG_MPEG4_Sound:return Sound_get_field_count(node, code_mode);
	case TAG_MPEG4_Sound2D:return Sound2D_get_field_count(node, code_mode);
	case TAG_MPEG4_Sphere:return Sphere_get_field_count(node, code_mode);
	case TAG_MPEG4_SphereSensor:return SphereSensor_get_field_count(node, code_mode);
	case TAG_MPEG4_SpotLight:return SpotLight_get_field_count(node, code_mode);
	case TAG_MPEG4_Switch:return Switch_get_field_count(node, code_mode);
	case TAG_MPEG4_TermCap:return TermCap_get_field_count(node, code_mode);
	case TAG_MPEG4_Text:return Text_get_field_count(node, code_mode);
	case TAG_MPEG4_TextureCoordinate:return TextureCoordinate_get_field_count(node, code_mode);
	case TAG_MPEG4_TextureTransform:return TextureTransform_get_field_count(node, code_mode);
	case TAG_MPEG4_TimeSensor:return TimeSensor_get_field_count(node, code_mode);
	case TAG_MPEG4_TouchSensor:return TouchSensor_get_field_count(node, code_mode);
	case TAG_MPEG4_Transform:return Transform_get_field_count(node, code_mode);
	case TAG_MPEG4_Transform2D:return Transform2D_get_field_count(node, code_mode);
	case TAG_MPEG4_Valuator:return Valuator_get_field_count(node, code_mode);
	case TAG_MPEG4_Viewpoint:return Viewpoint_get_field_count(node, code_mode);
	case TAG_MPEG4_VisibilitySensor:return VisibilitySensor_get_field_count(node, code_mode);
	case TAG_MPEG4_WorldInfo:return WorldInfo_get_field_count(node, code_mode);
	case TAG_MPEG4_AcousticMaterial:return AcousticMaterial_get_field_count(node, code_mode);
	case TAG_MPEG4_AcousticScene:return AcousticScene_get_field_count(node, code_mode);
	case TAG_MPEG4_ApplicationWindow:return ApplicationWindow_get_field_count(node, code_mode);
	case TAG_MPEG4_DirectiveSound:return DirectiveSound_get_field_count(node, code_mode);
	case TAG_MPEG4_Hierarchical3DMesh:return Hierarchical3DMesh_get_field_count(node, code_mode);
	case TAG_MPEG4_MaterialKey:return MaterialKey_get_field_count(node, code_mode);
	case TAG_MPEG4_PerceptualParameters:return PerceptualParameters_get_field_count(node, code_mode);
	case TAG_MPEG4_TemporalTransform:return TemporalTransform_get_field_count(node, code_mode);
	case TAG_MPEG4_TemporalGroup:return TemporalGroup_get_field_count(node, code_mode);
	case TAG_MPEG4_ServerCommand:return ServerCommand_get_field_count(node, code_mode);
	case TAG_MPEG4_InputSensor:return InputSensor_get_field_count(node, code_mode);
	case TAG_MPEG4_MatteTexture:return MatteTexture_get_field_count(node, code_mode);
	case TAG_MPEG4_MediaBuffer:return MediaBuffer_get_field_count(node, code_mode);
	case TAG_MPEG4_MediaControl:return MediaControl_get_field_count(node, code_mode);
	case TAG_MPEG4_MediaSensor:return MediaSensor_get_field_count(node, code_mode);
	case TAG_MPEG4_CoordinateInterpolator4D:return CoordinateInterpolator4D_get_field_count(node, code_mode);
	case TAG_MPEG4_NonLinearDeformer:return NonLinearDeformer_get_field_count(node, code_mode);
	case TAG_MPEG4_PositionAnimator:return PositionAnimator_get_field_count(node, code_mode);
	case TAG_MPEG4_PositionAnimator2D:return PositionAnimator2D_get_field_count(node, code_mode);
	case TAG_MPEG4_PositionInterpolator4D:return PositionInterpolator4D_get_field_count(node, code_mode);
	case TAG_MPEG4_ScalarAnimator:return ScalarAnimator_get_field_count(node, code_mode);
	case TAG_MPEG4_Clipper2D:return Clipper2D_get_field_count(node, code_mode);
	case TAG_MPEG4_ColorTransform:return ColorTransform_get_field_count(node, code_mode);
	case TAG_MPEG4_Ellipse:return Ellipse_get_field_count(node, code_mode);
	case TAG_MPEG4_LinearGradient:return LinearGradient_get_field_count(node, code_mode);
	case TAG_MPEG4_PathLayout:return PathLayout_get_field_count(node, code_mode);
	case TAG_MPEG4_RadialGradient:return RadialGradient_get_field_count(node, code_mode);
	case TAG_MPEG4_TransformMatrix2D:return TransformMatrix2D_get_field_count(node, code_mode);
	case TAG_MPEG4_Viewport:return Viewport_get_field_count(node, code_mode);
	case TAG_MPEG4_XCurve2D:return XCurve2D_get_field_count(node, code_mode);
	case TAG_MPEG4_XFontStyle:return XFontStyle_get_field_count(node, code_mode);
	case TAG_MPEG4_XLineProperties:return XLineProperties_get_field_count(node, code_mode);
	default:
		return 0;
	}
}

GF_Err gf_sg_mpeg4_node_get_field(GF_Node *node, GF_FieldInfo *field)
{
	switch (node->sgprivate->tag) {
	case TAG_MPEG4_Anchor: return Anchor_get_field(node, field);
	case TAG_MPEG4_AnimationStream: return AnimationStream_get_field(node, field);
	case TAG_MPEG4_Appearance: return Appearance_get_field(node, field);
	case TAG_MPEG4_AudioBuffer: return AudioBuffer_get_field(node, field);
	case TAG_MPEG4_AudioClip: return AudioClip_get_field(node, field);
	case TAG_MPEG4_AudioDelay: return AudioDelay_get_field(node, field);
	case TAG_MPEG4_AudioFX: return AudioFX_get_field(node, field);
	case TAG_MPEG4_AudioMix: return AudioMix_get_field(node, field);
	case TAG_MPEG4_AudioSource: return AudioSource_get_field(node, field);
	case TAG_MPEG4_AudioSwitch: return AudioSwitch_get_field(node, field);
	case TAG_MPEG4_Background: return Background_get_field(node, field);
	case TAG_MPEG4_Background2D: return Background2D_get_field(node, field);
	case TAG_MPEG4_Billboard: return Billboard_get_field(node, field);
	case TAG_MPEG4_Bitmap: return Bitmap_get_field(node, field);
	case TAG_MPEG4_Box: return Box_get_field(node, field);
	case TAG_MPEG4_Circle: return Circle_get_field(node, field);
	case TAG_MPEG4_Collision: return Collision_get_field(node, field);
	case TAG_MPEG4_Color: return Color_get_field(node, field);
	case TAG_MPEG4_ColorInterpolator: return ColorInterpolator_get_field(node, field);
	case TAG_MPEG4_CompositeTexture2D: return CompositeTexture2D_get_field(node, field);
	case TAG_MPEG4_CompositeTexture3D: return CompositeTexture3D_get_field(node, field);
	case TAG_MPEG4_Conditional: return Conditional_get_field(node, field);
	case TAG_MPEG4_Cone: return Cone_get_field(node, field);
	case TAG_MPEG4_Coordinate: return Coordinate_get_field(node, field);
	case TAG_MPEG4_Coordinate2D: return Coordinate2D_get_field(node, field);
	case TAG_MPEG4_CoordinateInterpolator: return CoordinateInterpolator_get_field(node, field);
	case TAG_MPEG4_CoordinateInterpolator2D: return CoordinateInterpolator2D_get_field(node, field);
	case TAG_MPEG4_Curve2D: return Curve2D_get_field(node, field);
	case TAG_MPEG4_Cylinder: return Cylinder_get_field(node, field);
	case TAG_MPEG4_CylinderSensor: return CylinderSensor_get_field(node, field);
	case TAG_MPEG4_DirectionalLight: return DirectionalLight_get_field(node, field);
	case TAG_MPEG4_DiscSensor: return DiscSensor_get_field(node, field);
	case TAG_MPEG4_ElevationGrid: return ElevationGrid_get_field(node, field);
	case TAG_MPEG4_Extrusion: return Extrusion_get_field(node, field);
	case TAG_MPEG4_Fog: return Fog_get_field(node, field);
	case TAG_MPEG4_FontStyle: return FontStyle_get_field(node, field);
	case TAG_MPEG4_Form: return Form_get_field(node, field);
	case TAG_MPEG4_Group: return Group_get_field(node, field);
	case TAG_MPEG4_ImageTexture: return ImageTexture_get_field(node, field);
	case TAG_MPEG4_IndexedFaceSet: return IndexedFaceSet_get_field(node, field);
	case TAG_MPEG4_IndexedFaceSet2D: return IndexedFaceSet2D_get_field(node, field);
	case TAG_MPEG4_IndexedLineSet: return IndexedLineSet_get_field(node, field);
	case TAG_MPEG4_IndexedLineSet2D: return IndexedLineSet2D_get_field(node, field);
	case TAG_MPEG4_Inline: return Inline_get_field(node, field);
	case TAG_MPEG4_LOD: return LOD_get_field(node, field);
	case TAG_MPEG4_Layer2D: return Layer2D_get_field(node, field);
	case TAG_MPEG4_Layer3D: return Layer3D_get_field(node, field);
	case TAG_MPEG4_Layout: return Layout_get_field(node, field);
	case TAG_MPEG4_LineProperties: return LineProperties_get_field(node, field);
	case TAG_MPEG4_ListeningPoint: return ListeningPoint_get_field(node, field);
	case TAG_MPEG4_Material: return Material_get_field(node, field);
	case TAG_MPEG4_Material2D: return Material2D_get_field(node, field);
	case TAG_MPEG4_MovieTexture: return MovieTexture_get_field(node, field);
	case TAG_MPEG4_NavigationInfo: return NavigationInfo_get_field(node, field);
	case TAG_MPEG4_Normal: return Normal_get_field(node, field);
	case TAG_MPEG4_NormalInterpolator: return NormalInterpolator_get_field(node, field);
	case TAG_MPEG4_OrderedGroup: return OrderedGroup_get_field(node, field);
	case TAG_MPEG4_OrientationInterpolator: return OrientationInterpolator_get_field(node, field);
	case TAG_MPEG4_PixelTexture: return PixelTexture_get_field(node, field);
	case TAG_MPEG4_PlaneSensor: return PlaneSensor_get_field(node, field);
	case TAG_MPEG4_PlaneSensor2D: return PlaneSensor2D_get_field(node, field);
	case TAG_MPEG4_PointLight: return PointLight_get_field(node, field);
	case TAG_MPEG4_PointSet: return PointSet_get_field(node, field);
	case TAG_MPEG4_PointSet2D: return PointSet2D_get_field(node, field);
	case TAG_MPEG4_PositionInterpolator: return PositionInterpolator_get_field(node, field);
	case TAG_MPEG4_PositionInterpolator2D: return PositionInterpolator2D_get_field(node, field);
	case TAG_MPEG4_ProximitySensor2D: return ProximitySensor2D_get_field(node, field);
	case TAG_MPEG4_ProximitySensor: return ProximitySensor_get_field(node, field);
	case TAG_MPEG4_QuantizationParameter: return QuantizationParameter_get_field(node, field);
	case TAG_MPEG4_Rectangle: return Rectangle_get_field(node, field);
	case TAG_MPEG4_ScalarInterpolator: return ScalarInterpolator_get_field(node, field);
	case TAG_MPEG4_Script: return Script_get_field(node, field);
	case TAG_MPEG4_Shape: return Shape_get_field(node, field);
	case TAG_MPEG4_Sound: return Sound_get_field(node, field);
	case TAG_MPEG4_Sound2D: return Sound2D_get_field(node, field);
	case TAG_MPEG4_Sphere: return Sphere_get_field(node, field);
	case TAG_MPEG4_SphereSensor: return SphereSensor_get_field(node, field);
	case TAG_MPEG4_SpotLight: return SpotLight_get_field(node, field);
	case TAG_MPEG4_Switch: return Switch_get_field(node, field);
	case TAG_MPEG4_TermCap: return TermCap_get_field(node, field);
	case TAG_MPEG4_Text: return Text_get_field(node, field);
	case TAG_MPEG4_TextureCoordinate: return TextureCoordinate_get_field(node, field);
	case TAG_MPEG4_TextureTransform: return TextureTransform_get_field(node, field);
	case TAG_MPEG4_TimeSensor: return TimeSensor_get_field(node, field);
	case TAG_MPEG4_TouchSensor: return TouchSensor_get_field(node, field);
	case TAG_MPEG4_Transform: return Transform_get_field(node, field);
	case TAG_MPEG4_Transform2D: return Transform2D_get_field(node, field);
	case TAG_MPEG4_Valuator: return Valuator_get_field(node, field);
	case TAG_MPEG4_Viewpoint: return Viewpoint_get_field(node, field);
	case TAG_MPEG4_VisibilitySensor: return VisibilitySensor_get_field(node, field);
	case TAG_MPEG4_WorldInfo: return WorldInfo_get_field(node, field);
	case TAG_MPEG4_AcousticMaterial: return AcousticMaterial_get_field(node, field);
	case TAG_MPEG4_AcousticScene: return AcousticScene_get_field(node, field);
	case TAG_MPEG4_ApplicationWindow: return ApplicationWindow_get_field(node, field);
	case TAG_MPEG4_DirectiveSound: return DirectiveSound_get_field(node, field);
	case TAG_MPEG4_Hierarchical3DMesh: return Hierarchical3DMesh_get_field(node, field);
	case TAG_MPEG4_MaterialKey: return MaterialKey_get_field(node, field);
	case TAG_MPEG4_PerceptualParameters: return PerceptualParameters_get_field(node, field);
	case TAG_MPEG4_TemporalTransform: return TemporalTransform_get_field(node, field);
	case TAG_MPEG4_TemporalGroup: return TemporalGroup_get_field(node, field);
	case TAG_MPEG4_ServerCommand: return ServerCommand_get_field(node, field);
	case TAG_MPEG4_InputSensor: return InputSensor_get_field(node, field);
	case TAG_MPEG4_MatteTexture: return MatteTexture_get_field(node, field);
	case TAG_MPEG4_MediaBuffer: return MediaBuffer_get_field(node, field);
	case TAG_MPEG4_MediaControl: return MediaControl_get_field(node, field);
	case TAG_MPEG4_MediaSensor: return MediaSensor_get_field(node, field);
	case TAG_MPEG4_CoordinateInterpolator4D: return CoordinateInterpolator4D_get_field(node, field);
	case TAG_MPEG4_NonLinearDeformer: return NonLinearDeformer_get_field(node, field);
	case TAG_MPEG4_PositionAnimator: return PositionAnimator_get_field(node, field);
	case TAG_MPEG4_PositionAnimator2D: return PositionAnimator2D_get_field(node, field);
	case TAG_MPEG4_PositionInterpolator4D: return PositionInterpolator4D_get_field(node, field);
	case TAG_MPEG4_ScalarAnimator: return ScalarAnimator_get_field(node, field);
	case TAG_MPEG4_Clipper2D: return Clipper2D_get_field(node, field);
	case TAG_MPEG4_ColorTransform: return ColorTransform_get_field(node, field);
	case TAG_MPEG4_Ellipse: return Ellipse_get_field(node, field);
	case TAG_MPEG4_LinearGradient: return LinearGradient_get_field(node, field);
	case TAG_MPEG4_PathLayout: return PathLayout_get_field(node, field);
	case TAG_MPEG4_RadialGradient: return RadialGradient_get_field(node, field);
	case TAG_MPEG4_TransformMatrix2D: return TransformMatrix2D_get_field(node, field);
	case TAG_MPEG4_Viewport: return Viewport_get_field(node, field);
	case TAG_MPEG4_XCurve2D: return XCurve2D_get_field(node, field);
	case TAG_MPEG4_XFontStyle: return XFontStyle_get_field(node, field);
	case TAG_MPEG4_XLineProperties: return XLineProperties_get_field(node, field);
	default:
		return GF_BAD_PARAM;
	}
}



#endif
GF_Err gf_sg_mpeg4_node_get_field_index(GF_Node *node, u32 inField, u8 code_mode, u32 *fieldIndex)
{
	switch (node->sgprivate->tag) {
	case TAG_MPEG4_Anchor: return Anchor_get_field_index(node, inField, code_mode, fieldIndex);
	case TAG_MPEG4_AnimationStream: return AnimationStream_get_field_index(node, inField, code_mode, fieldIndex);
	case TAG_MPEG4_Appearance: return Appearance_get_field_index(node, inField, code_mode, fieldIndex);
	case TAG_MPEG4_AudioBuffer: return AudioBuffer_get_field_index(node, inField, code_mode, fieldIndex);
	case TAG_MPEG4_AudioClip: return AudioClip_get_field_index(node, inField, code_mode, fieldIndex);
	case TAG_MPEG4_AudioDelay: return AudioDelay_get_field_index(node, inField, code_mode, fieldIndex);
	case TAG_MPEG4_AudioFX: return AudioFX_get_field_index(node, inField, code_mode, fieldIndex);
	case TAG_MPEG4_AudioMix: return AudioMix_get_field_index(node, inField, code_mode, fieldIndex);
	case TAG_MPEG4_AudioSource: return AudioSource_get_field_index(node, inField, code_mode, fieldIndex);
	case TAG_MPEG4_AudioSwitch: return AudioSwitch_get_field_index(node, inField, code_mode, fieldIndex);
	case TAG_MPEG4_Background: return Background_get_field_index(node, inField, code_mode, fieldIndex);
	case TAG_MPEG4_Background2D: return Background2D_get_field_index(node, inField, code_mode, fieldIndex);
	case TAG_MPEG4_Billboard: return Billboard_get_field_index(node, inField, code_mode, fieldIndex);
	case TAG_MPEG4_Bitmap: return Bitmap_get_field_index(node, inField, code_mode, fieldIndex);
	case TAG_MPEG4_Box: return Box_get_field_index(node, inField, code_mode, fieldIndex);
	case TAG_MPEG4_Circle: return Circle_get_field_index(node, inField, code_mode, fieldIndex);
	case TAG_MPEG4_Collision: return Collision_get_field_index(node, inField, code_mode, fieldIndex);
	case TAG_MPEG4_Color: return Color_get_field_index(node, inField, code_mode, fieldIndex);
	case TAG_MPEG4_ColorInterpolator: return ColorInterpolator_get_field_index(node, inField, code_mode, fieldIndex);
	case TAG_MPEG4_CompositeTexture2D: return CompositeTexture2D_get_field_index(node, inField, code_mode, fieldIndex);
	case TAG_MPEG4_CompositeTexture3D: return CompositeTexture3D_get_field_index(node, inField, code_mode, fieldIndex);
	case TAG_MPEG4_Conditional: return Conditional_get_field_index(node, inField, code_mode, fieldIndex);
	case TAG_MPEG4_Cone: return Cone_get_field_index(node, inField, code_mode, fieldIndex);
	case TAG_MPEG4_Coordinate: return Coordinate_get_field_index(node, inField, code_mode, fieldIndex);
	case TAG_MPEG4_Coordinate2D: return Coordinate2D_get_field_index(node, inField, code_mode, fieldIndex);
	case TAG_MPEG4_CoordinateInterpolator: return CoordinateInterpolator_get_field_index(node, inField, code_mode, fieldIndex);
	case TAG_MPEG4_CoordinateInterpolator2D: return CoordinateInterpolator2D_get_field_index(node, inField, code_mode, fieldIndex);
	case TAG_MPEG4_Curve2D: return Curve2D_get_field_index(node, inField, code_mode, fieldIndex);
	case TAG_MPEG4_Cylinder: return Cylinder_get_field_index(node, inField, code_mode, fieldIndex);
	case TAG_MPEG4_CylinderSensor: return CylinderSensor_get_field_index(node, inField, code_mode, fieldIndex);
	case TAG_MPEG4_DirectionalLight: return DirectionalLight_get_field_index(node, inField, code_mode, fieldIndex);
	case TAG_MPEG4_DiscSensor: return DiscSensor_get_field_index(node, inField, code_mode, fieldIndex);
	case TAG_MPEG4_ElevationGrid: return ElevationGrid_get_field_index(node, inField, code_mode, fieldIndex);
	case TAG_MPEG4_Extrusion: return Extrusion_get_field_index(node, inField, code_mode, fieldIndex);
	case TAG_MPEG4_Fog: return Fog_get_field_index(node, inField, code_mode, fieldIndex);
	case TAG_MPEG4_FontStyle: return FontStyle_get_field_index(node, inField, code_mode, fieldIndex);
	case TAG_MPEG4_Form: return Form_get_field_index(node, inField, code_mode, fieldIndex);
	case TAG_MPEG4_Group: return Group_get_field_index(node, inField, code_mode, fieldIndex);
	case TAG_MPEG4_ImageTexture: return ImageTexture_get_field_index(node, inField, code_mode, fieldIndex);
	case TAG_MPEG4_IndexedFaceSet: return IndexedFaceSet_get_field_index(node, inField, code_mode, fieldIndex);
	case TAG_MPEG4_IndexedFaceSet2D: return IndexedFaceSet2D_get_field_index(node, inField, code_mode, fieldIndex);
	case TAG_MPEG4_IndexedLineSet: return IndexedLineSet_get_field_index(node, inField, code_mode, fieldIndex);
	case TAG_MPEG4_IndexedLineSet2D: return IndexedLineSet2D_get_field_index(node, inField, code_mode, fieldIndex);
	case TAG_MPEG4_Inline: return Inline_get_field_index(node, inField, code_mode, fieldIndex);
	case TAG_MPEG4_LOD: return LOD_get_field_index(node, inField, code_mode, fieldIndex);
	case TAG_MPEG4_Layer2D: return Layer2D_get_field_index(node, inField, code_mode, fieldIndex);
	case TAG_MPEG4_Layer3D: return Layer3D_get_field_index(node, inField, code_mode, fieldIndex);
	case TAG_MPEG4_Layout: return Layout_get_field_index(node, inField, code_mode, fieldIndex);
	case TAG_MPEG4_LineProperties: return LineProperties_get_field_index(node, inField, code_mode, fieldIndex);
	case TAG_MPEG4_ListeningPoint: return ListeningPoint_get_field_index(node, inField, code_mode, fieldIndex);
	case TAG_MPEG4_Material: return Material_get_field_index(node, inField, code_mode, fieldIndex);
	case TAG_MPEG4_Material2D: return Material2D_get_field_index(node, inField, code_mode, fieldIndex);
	case TAG_MPEG4_MovieTexture: return MovieTexture_get_field_index(node, inField, code_mode, fieldIndex);
	case TAG_MPEG4_NavigationInfo: return NavigationInfo_get_field_index(node, inField, code_mode, fieldIndex);
	case TAG_MPEG4_Normal: return Normal_get_field_index(node, inField, code_mode, fieldIndex);
	case TAG_MPEG4_NormalInterpolator: return NormalInterpolator_get_field_index(node, inField, code_mode, fieldIndex);
	case TAG_MPEG4_OrderedGroup: return OrderedGroup_get_field_index(node, inField, code_mode, fieldIndex);
	case TAG_MPEG4_OrientationInterpolator: return OrientationInterpolator_get_field_index(node, inField, code_mode, fieldIndex);
	case TAG_MPEG4_PixelTexture: return PixelTexture_get_field_index(node, inField, code_mode, fieldIndex);
	case TAG_MPEG4_PlaneSensor: return PlaneSensor_get_field_index(node, inField, code_mode, fieldIndex);
	case TAG_MPEG4_PlaneSensor2D: return PlaneSensor2D_get_field_index(node, inField, code_mode, fieldIndex);
	case TAG_MPEG4_PointLight: return PointLight_get_field_index(node, inField, code_mode, fieldIndex);
	case TAG_MPEG4_PointSet: return PointSet_get_field_index(node, inField, code_mode, fieldIndex);
	case TAG_MPEG4_PointSet2D: return PointSet2D_get_field_index(node, inField, code_mode, fieldIndex);
	case TAG_MPEG4_PositionInterpolator: return PositionInterpolator_get_field_index(node, inField, code_mode, fieldIndex);
	case TAG_MPEG4_PositionInterpolator2D: return PositionInterpolator2D_get_field_index(node, inField, code_mode, fieldIndex);
	case TAG_MPEG4_ProximitySensor2D: return ProximitySensor2D_get_field_index(node, inField, code_mode, fieldIndex);
	case TAG_MPEG4_ProximitySensor: return ProximitySensor_get_field_index(node, inField, code_mode, fieldIndex);
	case TAG_MPEG4_QuantizationParameter: return QuantizationParameter_get_field_index(node, inField, code_mode, fieldIndex);
	case TAG_MPEG4_Rectangle: return Rectangle_get_field_index(node, inField, code_mode, fieldIndex);
	case TAG_MPEG4_ScalarInterpolator: return ScalarInterpolator_get_field_index(node, inField, code_mode, fieldIndex);
	case TAG_MPEG4_Script: return Script_get_field_index(node, inField, code_mode, fieldIndex);
	case TAG_MPEG4_Shape: return Shape_get_field_index(node, inField, code_mode, fieldIndex);
	case TAG_MPEG4_Sound: return Sound_get_field_index(node, inField, code_mode, fieldIndex);
	case TAG_MPEG4_Sound2D: return Sound2D_get_field_index(node, inField, code_mode, fieldIndex);
	case TAG_MPEG4_Sphere: return Sphere_get_field_index(node, inField, code_mode, fieldIndex);
	case TAG_MPEG4_SphereSensor: return SphereSensor_get_field_index(node, inField, code_mode, fieldIndex);
	case TAG_MPEG4_SpotLight: return SpotLight_get_field_index(node, inField, code_mode, fieldIndex);
	case TAG_MPEG4_Switch: return Switch_get_field_index(node, inField, code_mode, fieldIndex);
	case TAG_MPEG4_TermCap: return TermCap_get_field_index(node, inField, code_mode, fieldIndex);
	case TAG_MPEG4_Text: return Text_get_field_index(node, inField, code_mode, fieldIndex);
	case TAG_MPEG4_TextureCoordinate: return TextureCoordinate_get_field_index(node, inField, code_mode, fieldIndex);
	case TAG_MPEG4_TextureTransform: return TextureTransform_get_field_index(node, inField, code_mode, fieldIndex);
	case TAG_MPEG4_TimeSensor: return TimeSensor_get_field_index(node, inField, code_mode, fieldIndex);
	case TAG_MPEG4_TouchSensor: return TouchSensor_get_field_index(node, inField, code_mode, fieldIndex);
	case TAG_MPEG4_Transform: return Transform_get_field_index(node, inField, code_mode, fieldIndex);
	case TAG_MPEG4_Transform2D: return Transform2D_get_field_index(node, inField, code_mode, fieldIndex);
	case TAG_MPEG4_Valuator: return Valuator_get_field_index(node, inField, code_mode, fieldIndex);
	case TAG_MPEG4_Viewpoint: return Viewpoint_get_field_index(node, inField, code_mode, fieldIndex);
	case TAG_MPEG4_VisibilitySensor: return VisibilitySensor_get_field_index(node, inField, code_mode, fieldIndex);
	case TAG_MPEG4_WorldInfo: return WorldInfo_get_field_index(node, inField, code_mode, fieldIndex);
	case TAG_MPEG4_AcousticMaterial: return AcousticMaterial_get_field_index(node, inField, code_mode, fieldIndex);
	case TAG_MPEG4_AcousticScene: return AcousticScene_get_field_index(node, inField, code_mode, fieldIndex);
	case TAG_MPEG4_ApplicationWindow: return ApplicationWindow_get_field_index(node, inField, code_mode, fieldIndex);
	case TAG_MPEG4_DirectiveSound: return DirectiveSound_get_field_index(node, inField, code_mode, fieldIndex);
	case TAG_MPEG4_Hierarchical3DMesh: return Hierarchical3DMesh_get_field_index(node, inField, code_mode, fieldIndex);
	case TAG_MPEG4_MaterialKey: return MaterialKey_get_field_index(node, inField, code_mode, fieldIndex);
	case TAG_MPEG4_PerceptualParameters: return PerceptualParameters_get_field_index(node, inField, code_mode, fieldIndex);
	case TAG_MPEG4_TemporalTransform: return TemporalTransform_get_field_index(node, inField, code_mode, fieldIndex);
	case TAG_MPEG4_TemporalGroup: return TemporalGroup_get_field_index(node, inField, code_mode, fieldIndex);
	case TAG_MPEG4_ServerCommand: return ServerCommand_get_field_index(node, inField, code_mode, fieldIndex);
	case TAG_MPEG4_InputSensor: return InputSensor_get_field_index(node, inField, code_mode, fieldIndex);
	case TAG_MPEG4_MatteTexture: return MatteTexture_get_field_index(node, inField, code_mode, fieldIndex);
	case TAG_MPEG4_MediaBuffer: return MediaBuffer_get_field_index(node, inField, code_mode, fieldIndex);
	case TAG_MPEG4_MediaControl: return MediaControl_get_field_index(node, inField, code_mode, fieldIndex);
	case TAG_MPEG4_MediaSensor: return MediaSensor_get_field_index(node, inField, code_mode, fieldIndex);
	case TAG_MPEG4_CoordinateInterpolator4D: return CoordinateInterpolator4D_get_field_index(node, inField, code_mode, fieldIndex);
	case TAG_MPEG4_NonLinearDeformer: return NonLinearDeformer_get_field_index(node, inField, code_mode, fieldIndex);
	case TAG_MPEG4_PositionAnimator: return PositionAnimator_get_field_index(node, inField, code_mode, fieldIndex);
	case TAG_MPEG4_PositionAnimator2D: return PositionAnimator2D_get_field_index(node, inField, code_mode, fieldIndex);
	case TAG_MPEG4_PositionInterpolator4D: return PositionInterpolator4D_get_field_index(node, inField, code_mode, fieldIndex);
	case TAG_MPEG4_ScalarAnimator: return ScalarAnimator_get_field_index(node, inField, code_mode, fieldIndex);
	case TAG_MPEG4_Clipper2D: return Clipper2D_get_field_index(node, inField, code_mode, fieldIndex);
	case TAG_MPEG4_ColorTransform: return ColorTransform_get_field_index(node, inField, code_mode, fieldIndex);
	case TAG_MPEG4_Ellipse: return Ellipse_get_field_index(node, inField, code_mode, fieldIndex);
	case TAG_MPEG4_LinearGradient: return LinearGradient_get_field_index(node, inField, code_mode, fieldIndex);
	case TAG_MPEG4_PathLayout: return PathLayout_get_field_index(node, inField, code_mode, fieldIndex);
	case TAG_MPEG4_RadialGradient: return RadialGradient_get_field_index(node, inField, code_mode, fieldIndex);
	case TAG_MPEG4_TransformMatrix2D: return TransformMatrix2D_get_field_index(node, inField, code_mode, fieldIndex);
	case TAG_MPEG4_Viewport: return Viewport_get_field_index(node, inField, code_mode, fieldIndex);
	case TAG_MPEG4_XCurve2D: return XCurve2D_get_field_index(node, inField, code_mode, fieldIndex);
	case TAG_MPEG4_XFontStyle: return XFontStyle_get_field_index(node, inField, code_mode, fieldIndex);
	case TAG_MPEG4_XLineProperties: return XLineProperties_get_field_index(node, inField, code_mode, fieldIndex);
	default:
		return GF_BAD_PARAM;
	}
}

Bool gf_sg_mpeg4_node_get_aq_info(GF_Node *node, u32 FieldIndex, u8 *QType, u8 *AType, Fixed *b_min, Fixed *b_max, u32 *QT13_bits)
{
	switch (node->sgprivate->tag) {
	case TAG_MPEG4_Anchor: return Anchor_get_aq_info(node, FieldIndex, QType, AType, b_min, b_max, QT13_bits);
	case TAG_MPEG4_AnimationStream: return AnimationStream_get_aq_info(node, FieldIndex, QType, AType, b_min, b_max, QT13_bits);
	case TAG_MPEG4_Appearance: return Appearance_get_aq_info(node, FieldIndex, QType, AType, b_min, b_max, QT13_bits);
	case TAG_MPEG4_AudioBuffer: return AudioBuffer_get_aq_info(node, FieldIndex, QType, AType, b_min, b_max, QT13_bits);
	case TAG_MPEG4_AudioClip: return AudioClip_get_aq_info(node, FieldIndex, QType, AType, b_min, b_max, QT13_bits);
	case TAG_MPEG4_AudioDelay: return AudioDelay_get_aq_info(node, FieldIndex, QType, AType, b_min, b_max, QT13_bits);
	case TAG_MPEG4_AudioFX: return AudioFX_get_aq_info(node, FieldIndex, QType, AType, b_min, b_max, QT13_bits);
	case TAG_MPEG4_AudioMix: return AudioMix_get_aq_info(node, FieldIndex, QType, AType, b_min, b_max, QT13_bits);
	case TAG_MPEG4_AudioSource: return AudioSource_get_aq_info(node, FieldIndex, QType, AType, b_min, b_max, QT13_bits);
	case TAG_MPEG4_AudioSwitch: return AudioSwitch_get_aq_info(node, FieldIndex, QType, AType, b_min, b_max, QT13_bits);
	case TAG_MPEG4_Background: return Background_get_aq_info(node, FieldIndex, QType, AType, b_min, b_max, QT13_bits);
	case TAG_MPEG4_Background2D: return Background2D_get_aq_info(node, FieldIndex, QType, AType, b_min, b_max, QT13_bits);
	case TAG_MPEG4_Billboard: return Billboard_get_aq_info(node, FieldIndex, QType, AType, b_min, b_max, QT13_bits);
	case TAG_MPEG4_Bitmap: return Bitmap_get_aq_info(node, FieldIndex, QType, AType, b_min, b_max, QT13_bits);
	case TAG_MPEG4_Box: return Box_get_aq_info(node, FieldIndex, QType, AType, b_min, b_max, QT13_bits);
	case TAG_MPEG4_Circle: return Circle_get_aq_info(node, FieldIndex, QType, AType, b_min, b_max, QT13_bits);
	case TAG_MPEG4_Collision: return Collision_get_aq_info(node, FieldIndex, QType, AType, b_min, b_max, QT13_bits);
	case TAG_MPEG4_Color: return Color_get_aq_info(node, FieldIndex, QType, AType, b_min, b_max, QT13_bits);
	case TAG_MPEG4_ColorInterpolator: return ColorInterpolator_get_aq_info(node, FieldIndex, QType, AType, b_min, b_max, QT13_bits);
	case TAG_MPEG4_CompositeTexture2D: return CompositeTexture2D_get_aq_info(node, FieldIndex, QType, AType, b_min, b_max, QT13_bits);
	case TAG_MPEG4_CompositeTexture3D: return CompositeTexture3D_get_aq_info(node, FieldIndex, QType, AType, b_min, b_max, QT13_bits);
	case TAG_MPEG4_Conditional: return Conditional_get_aq_info(node, FieldIndex, QType, AType, b_min, b_max, QT13_bits);
	case TAG_MPEG4_Cone: return Cone_get_aq_info(node, FieldIndex, QType, AType, b_min, b_max, QT13_bits);
	case TAG_MPEG4_Coordinate: return Coordinate_get_aq_info(node, FieldIndex, QType, AType, b_min, b_max, QT13_bits);
	case TAG_MPEG4_Coordinate2D: return Coordinate2D_get_aq_info(node, FieldIndex, QType, AType, b_min, b_max, QT13_bits);
	case TAG_MPEG4_CoordinateInterpolator: return CoordinateInterpolator_get_aq_info(node, FieldIndex, QType, AType, b_min, b_max, QT13_bits);
	case TAG_MPEG4_CoordinateInterpolator2D: return CoordinateInterpolator2D_get_aq_info(node, FieldIndex, QType, AType, b_min, b_max, QT13_bits);
	case TAG_MPEG4_Curve2D: return Curve2D_get_aq_info(node, FieldIndex, QType, AType, b_min, b_max, QT13_bits);
	case TAG_MPEG4_Cylinder: return Cylinder_get_aq_info(node, FieldIndex, QType, AType, b_min, b_max, QT13_bits);
	case TAG_MPEG4_CylinderSensor: return CylinderSensor_get_aq_info(node, FieldIndex, QType, AType, b_min, b_max, QT13_bits);
	case TAG_MPEG4_DirectionalLight: return DirectionalLight_get_aq_info(node, FieldIndex, QType, AType, b_min, b_max, QT13_bits);
	case TAG_MPEG4_DiscSensor: return DiscSensor_get_aq_info(node, FieldIndex, QType, AType, b_min, b_max, QT13_bits);
	case TAG_MPEG4_ElevationGrid: return ElevationGrid_get_aq_info(node, FieldIndex, QType, AType, b_min, b_max, QT13_bits);
	case TAG_MPEG4_Extrusion: return Extrusion_get_aq_info(node, FieldIndex, QType, AType, b_min, b_max, QT13_bits);
	case TAG_MPEG4_Fog: return Fog_get_aq_info(node, FieldIndex, QType, AType, b_min, b_max, QT13_bits);
	case TAG_MPEG4_FontStyle: return FontStyle_get_aq_info(node, FieldIndex, QType, AType, b_min, b_max, QT13_bits);
	case TAG_MPEG4_Form: return Form_get_aq_info(node, FieldIndex, QType, AType, b_min, b_max, QT13_bits);
	case TAG_MPEG4_Group: return Group_get_aq_info(node, FieldIndex, QType, AType, b_min, b_max, QT13_bits);
	case TAG_MPEG4_ImageTexture: return ImageTexture_get_aq_info(node, FieldIndex, QType, AType, b_min, b_max, QT13_bits);
	case TAG_MPEG4_IndexedFaceSet: return IndexedFaceSet_get_aq_info(node, FieldIndex, QType, AType, b_min, b_max, QT13_bits);
	case TAG_MPEG4_IndexedFaceSet2D: return IndexedFaceSet2D_get_aq_info(node, FieldIndex, QType, AType, b_min, b_max, QT13_bits);
	case TAG_MPEG4_IndexedLineSet: return IndexedLineSet_get_aq_info(node, FieldIndex, QType, AType, b_min, b_max, QT13_bits);
	case TAG_MPEG4_IndexedLineSet2D: return IndexedLineSet2D_get_aq_info(node, FieldIndex, QType, AType, b_min, b_max, QT13_bits);
	case TAG_MPEG4_Inline: return Inline_get_aq_info(node, FieldIndex, QType, AType, b_min, b_max, QT13_bits);
	case TAG_MPEG4_LOD: return LOD_get_aq_info(node, FieldIndex, QType, AType, b_min, b_max, QT13_bits);
	case TAG_MPEG4_Layer2D: return Layer2D_get_aq_info(node, FieldIndex, QType, AType, b_min, b_max, QT13_bits);
	case TAG_MPEG4_Layer3D: return Layer3D_get_aq_info(node, FieldIndex, QType, AType, b_min, b_max, QT13_bits);
	case TAG_MPEG4_Layout: return Layout_get_aq_info(node, FieldIndex, QType, AType, b_min, b_max, QT13_bits);
	case TAG_MPEG4_LineProperties: return LineProperties_get_aq_info(node, FieldIndex, QType, AType, b_min, b_max, QT13_bits);
	case TAG_MPEG4_ListeningPoint: return ListeningPoint_get_aq_info(node, FieldIndex, QType, AType, b_min, b_max, QT13_bits);
	case TAG_MPEG4_Material: return Material_get_aq_info(node, FieldIndex, QType, AType, b_min, b_max, QT13_bits);
	case TAG_MPEG4_Material2D: return Material2D_get_aq_info(node, FieldIndex, QType, AType, b_min, b_max, QT13_bits);
	case TAG_MPEG4_MovieTexture: return MovieTexture_get_aq_info(node, FieldIndex, QType, AType, b_min, b_max, QT13_bits);
	case TAG_MPEG4_NavigationInfo: return NavigationInfo_get_aq_info(node, FieldIndex, QType, AType, b_min, b_max, QT13_bits);
	case TAG_MPEG4_Normal: return Normal_get_aq_info(node, FieldIndex, QType, AType, b_min, b_max, QT13_bits);
	case TAG_MPEG4_NormalInterpolator: return NormalInterpolator_get_aq_info(node, FieldIndex, QType, AType, b_min, b_max, QT13_bits);
	case TAG_MPEG4_OrderedGroup: return OrderedGroup_get_aq_info(node, FieldIndex, QType, AType, b_min, b_max, QT13_bits);
	case TAG_MPEG4_OrientationInterpolator: return OrientationInterpolator_get_aq_info(node, FieldIndex, QType, AType, b_min, b_max, QT13_bits);
	case TAG_MPEG4_PixelTexture: return PixelTexture_get_aq_info(node, FieldIndex, QType, AType, b_min, b_max, QT13_bits);
	case TAG_MPEG4_PlaneSensor: return PlaneSensor_get_aq_info(node, FieldIndex, QType, AType, b_min, b_max, QT13_bits);
	case TAG_MPEG4_PlaneSensor2D: return PlaneSensor2D_get_aq_info(node, FieldIndex, QType, AType, b_min, b_max, QT13_bits);
	case TAG_MPEG4_PointLight: return PointLight_get_aq_info(node, FieldIndex, QType, AType, b_min, b_max, QT13_bits);
	case TAG_MPEG4_PointSet: return PointSet_get_aq_info(node, FieldIndex, QType, AType, b_min, b_max, QT13_bits);
	case TAG_MPEG4_PointSet2D: return PointSet2D_get_aq_info(node, FieldIndex, QType, AType, b_min, b_max, QT13_bits);
	case TAG_MPEG4_PositionInterpolator: return PositionInterpolator_get_aq_info(node, FieldIndex, QType, AType, b_min, b_max, QT13_bits);
	case TAG_MPEG4_PositionInterpolator2D: return PositionInterpolator2D_get_aq_info(node, FieldIndex, QType, AType, b_min, b_max, QT13_bits);
	case TAG_MPEG4_ProximitySensor2D: return ProximitySensor2D_get_aq_info(node, FieldIndex, QType, AType, b_min, b_max, QT13_bits);
	case TAG_MPEG4_ProximitySensor: return ProximitySensor_get_aq_info(node, FieldIndex, QType, AType, b_min, b_max, QT13_bits);
	case TAG_MPEG4_QuantizationParameter: return QuantizationParameter_get_aq_info(node, FieldIndex, QType, AType, b_min, b_max, QT13_bits);
	case TAG_MPEG4_Rectangle: return Rectangle_get_aq_info(node, FieldIndex, QType, AType, b_min, b_max, QT13_bits);
	case TAG_MPEG4_ScalarInterpolator: return ScalarInterpolator_get_aq_info(node, FieldIndex, QType, AType, b_min, b_max, QT13_bits);
	case TAG_MPEG4_Script: return Script_get_aq_info(node, FieldIndex, QType, AType, b_min, b_max, QT13_bits);
	case TAG_MPEG4_Shape: return Shape_get_aq_info(node, FieldIndex, QType, AType, b_min, b_max, QT13_bits);
	case TAG_MPEG4_Sound: return Sound_get_aq_info(node, FieldIndex, QType, AType, b_min, b_max, QT13_bits);
	case TAG_MPEG4_Sound2D: return Sound2D_get_aq_info(node, FieldIndex, QType, AType, b_min, b_max, QT13_bits);
	case TAG_MPEG4_Sphere: return Sphere_get_aq_info(node, FieldIndex, QType, AType, b_min, b_max, QT13_bits);
	case TAG_MPEG4_SphereSensor: return SphereSensor_get_aq_info(node, FieldIndex, QType, AType, b_min, b_max, QT13_bits);
	case TAG_MPEG4_SpotLight: return SpotLight_get_aq_info(node, FieldIndex, QType, AType, b_min, b_max, QT13_bits);
	case TAG_MPEG4_Switch: return Switch_get_aq_info(node, FieldIndex, QType, AType, b_min, b_max, QT13_bits);
	case TAG_MPEG4_TermCap: return TermCap_get_aq_info(node, FieldIndex, QType, AType, b_min, b_max, QT13_bits);
	case TAG_MPEG4_Text: return Text_get_aq_info(node, FieldIndex, QType, AType, b_min, b_max, QT13_bits);
	case TAG_MPEG4_TextureCoordinate: return TextureCoordinate_get_aq_info(node, FieldIndex, QType, AType, b_min, b_max, QT13_bits);
	case TAG_MPEG4_TextureTransform: return TextureTransform_get_aq_info(node, FieldIndex, QType, AType, b_min, b_max, QT13_bits);
	case TAG_MPEG4_TimeSensor: return TimeSensor_get_aq_info(node, FieldIndex, QType, AType, b_min, b_max, QT13_bits);
	case TAG_MPEG4_TouchSensor: return TouchSensor_get_aq_info(node, FieldIndex, QType, AType, b_min, b_max, QT13_bits);
	case TAG_MPEG4_Transform: return Transform_get_aq_info(node, FieldIndex, QType, AType, b_min, b_max, QT13_bits);
	case TAG_MPEG4_Transform2D: return Transform2D_get_aq_info(node, FieldIndex, QType, AType, b_min, b_max, QT13_bits);
	case TAG_MPEG4_Valuator: return Valuator_get_aq_info(node, FieldIndex, QType, AType, b_min, b_max, QT13_bits);
	case TAG_MPEG4_Viewpoint: return Viewpoint_get_aq_info(node, FieldIndex, QType, AType, b_min, b_max, QT13_bits);
	case TAG_MPEG4_VisibilitySensor: return VisibilitySensor_get_aq_info(node, FieldIndex, QType, AType, b_min, b_max, QT13_bits);
	case TAG_MPEG4_WorldInfo: return WorldInfo_get_aq_info(node, FieldIndex, QType, AType, b_min, b_max, QT13_bits);
	case TAG_MPEG4_AcousticMaterial: return AcousticMaterial_get_aq_info(node, FieldIndex, QType, AType, b_min, b_max, QT13_bits);
	case TAG_MPEG4_AcousticScene: return AcousticScene_get_aq_info(node, FieldIndex, QType, AType, b_min, b_max, QT13_bits);
	case TAG_MPEG4_ApplicationWindow: return ApplicationWindow_get_aq_info(node, FieldIndex, QType, AType, b_min, b_max, QT13_bits);
	case TAG_MPEG4_DirectiveSound: return DirectiveSound_get_aq_info(node, FieldIndex, QType, AType, b_min, b_max, QT13_bits);
	case TAG_MPEG4_Hierarchical3DMesh: return Hierarchical3DMesh_get_aq_info(node, FieldIndex, QType, AType, b_min, b_max, QT13_bits);
	case TAG_MPEG4_MaterialKey: return MaterialKey_get_aq_info(node, FieldIndex, QType, AType, b_min, b_max, QT13_bits);
	case TAG_MPEG4_PerceptualParameters: return PerceptualParameters_get_aq_info(node, FieldIndex, QType, AType, b_min, b_max, QT13_bits);
	case TAG_MPEG4_TemporalTransform: return TemporalTransform_get_aq_info(node, FieldIndex, QType, AType, b_min, b_max, QT13_bits);
	case TAG_MPEG4_TemporalGroup: return TemporalGroup_get_aq_info(node, FieldIndex, QType, AType, b_min, b_max, QT13_bits);
	case TAG_MPEG4_ServerCommand: return ServerCommand_get_aq_info(node, FieldIndex, QType, AType, b_min, b_max, QT13_bits);
	case TAG_MPEG4_InputSensor: return InputSensor_get_aq_info(node, FieldIndex, QType, AType, b_min, b_max, QT13_bits);
	case TAG_MPEG4_MatteTexture: return MatteTexture_get_aq_info(node, FieldIndex, QType, AType, b_min, b_max, QT13_bits);
	case TAG_MPEG4_MediaBuffer: return MediaBuffer_get_aq_info(node, FieldIndex, QType, AType, b_min, b_max, QT13_bits);
	case TAG_MPEG4_MediaControl: return MediaControl_get_aq_info(node, FieldIndex, QType, AType, b_min, b_max, QT13_bits);
	case TAG_MPEG4_MediaSensor: return MediaSensor_get_aq_info(node, FieldIndex, QType, AType, b_min, b_max, QT13_bits);
	case TAG_MPEG4_CoordinateInterpolator4D: return CoordinateInterpolator4D_get_aq_info(node, FieldIndex, QType, AType, b_min, b_max, QT13_bits);
	case TAG_MPEG4_NonLinearDeformer: return NonLinearDeformer_get_aq_info(node, FieldIndex, QType, AType, b_min, b_max, QT13_bits);
	case TAG_MPEG4_PositionAnimator: return PositionAnimator_get_aq_info(node, FieldIndex, QType, AType, b_min, b_max, QT13_bits);
	case TAG_MPEG4_PositionAnimator2D: return PositionAnimator2D_get_aq_info(node, FieldIndex, QType, AType, b_min, b_max, QT13_bits);
	case TAG_MPEG4_PositionInterpolator4D: return PositionInterpolator4D_get_aq_info(node, FieldIndex, QType, AType, b_min, b_max, QT13_bits);
	case TAG_MPEG4_ScalarAnimator: return ScalarAnimator_get_aq_info(node, FieldIndex, QType, AType, b_min, b_max, QT13_bits);
	case TAG_MPEG4_Clipper2D: return Clipper2D_get_aq_info(node, FieldIndex, QType, AType, b_min, b_max, QT13_bits);
	case TAG_MPEG4_ColorTransform: return ColorTransform_get_aq_info(node, FieldIndex, QType, AType, b_min, b_max, QT13_bits);
	case TAG_MPEG4_Ellipse: return Ellipse_get_aq_info(node, FieldIndex, QType, AType, b_min, b_max, QT13_bits);
	case TAG_MPEG4_LinearGradient: return LinearGradient_get_aq_info(node, FieldIndex, QType, AType, b_min, b_max, QT13_bits);
	case TAG_MPEG4_PathLayout: return PathLayout_get_aq_info(node, FieldIndex, QType, AType, b_min, b_max, QT13_bits);
	case TAG_MPEG4_RadialGradient: return RadialGradient_get_aq_info(node, FieldIndex, QType, AType, b_min, b_max, QT13_bits);
	case TAG_MPEG4_TransformMatrix2D: return TransformMatrix2D_get_aq_info(node, FieldIndex, QType, AType, b_min, b_max, QT13_bits);
	case TAG_MPEG4_Viewport: return Viewport_get_aq_info(node, FieldIndex, QType, AType, b_min, b_max, QT13_bits);
	case TAG_MPEG4_XCurve2D: return XCurve2D_get_aq_info(node, FieldIndex, QType, AType, b_min, b_max, QT13_bits);
	case TAG_MPEG4_XFontStyle: return XFontStyle_get_aq_info(node, FieldIndex, QType, AType, b_min, b_max, QT13_bits);
	case TAG_MPEG4_XLineProperties: return XLineProperties_get_aq_info(node, FieldIndex, QType, AType, b_min, b_max, QT13_bits);
	default:
		return 0;
	}
}

u32 gf_sg_mpeg4_node_get_child_ndt(GF_Node *node)
{
	switch (node->sgprivate->tag) {
	case TAG_MPEG4_Anchor: return NDT_SF3DNode;
	case TAG_MPEG4_AudioBuffer: return NDT_SFAudioNode;
	case TAG_MPEG4_AudioDelay: return NDT_SFAudioNode;
	case TAG_MPEG4_AudioFX: return NDT_SFAudioNode;
	case TAG_MPEG4_AudioMix: return NDT_SFAudioNode;
	case TAG_MPEG4_AudioSource: return NDT_SFAudioNode;
	case TAG_MPEG4_AudioSwitch: return NDT_SFAudioNode;
	case TAG_MPEG4_Billboard: return NDT_SF3DNode;
	case TAG_MPEG4_Collision: return NDT_SF3DNode;
	case TAG_MPEG4_CompositeTexture2D: return NDT_SF2DNode;
	case TAG_MPEG4_CompositeTexture3D: return NDT_SF3DNode;
	case TAG_MPEG4_Form: return NDT_SF2DNode;
	case TAG_MPEG4_Group: return NDT_SF3DNode;
	case TAG_MPEG4_LOD: return NDT_SF3DNode;
	case TAG_MPEG4_Layer2D: return NDT_SF2DNode;
	case TAG_MPEG4_Layer3D: return NDT_SF3DNode;
	case TAG_MPEG4_Layout: return NDT_SF2DNode;
	case TAG_MPEG4_OrderedGroup: return NDT_SF3DNode;
	case TAG_MPEG4_Switch: return NDT_SF3DNode;
	case TAG_MPEG4_Transform: return NDT_SF3DNode;
	case TAG_MPEG4_Transform2D: return NDT_SF2DNode;
	case TAG_MPEG4_TemporalTransform: return NDT_SF3DNode;
	case TAG_MPEG4_TemporalGroup: return NDT_SFTemporalNode;
	case TAG_MPEG4_Clipper2D: return NDT_SF2DNode;
	case TAG_MPEG4_ColorTransform: return NDT_SF3DNode;
	case TAG_MPEG4_PathLayout: return NDT_SF2DNode;
	case TAG_MPEG4_TransformMatrix2D: return NDT_SF2DNode;
	default:
		return 0;
	}
}


u32 gf_node_mpeg4_type_by_class_name(const char *node_name)
{
	if(!node_name) return 0;
	if (!stricmp(node_name, "Anchor")) return TAG_MPEG4_Anchor;
	if (!stricmp(node_name, "AnimationStream")) return TAG_MPEG4_AnimationStream;
	if (!stricmp(node_name, "Appearance")) return TAG_MPEG4_Appearance;
	if (!stricmp(node_name, "AudioBuffer")) return TAG_MPEG4_AudioBuffer;
	if (!stricmp(node_name, "AudioClip")) return TAG_MPEG4_AudioClip;
	if (!stricmp(node_name, "AudioDelay")) return TAG_MPEG4_AudioDelay;
	if (!stricmp(node_name, "AudioFX")) return TAG_MPEG4_AudioFX;
	if (!stricmp(node_name, "AudioMix")) return TAG_MPEG4_AudioMix;
	if (!stricmp(node_name, "AudioSource")) return TAG_MPEG4_AudioSource;
	if (!stricmp(node_name, "AudioSwitch")) return TAG_MPEG4_AudioSwitch;
	if (!stricmp(node_name, "Background")) return TAG_MPEG4_Background;
	if (!stricmp(node_name, "Background2D")) return TAG_MPEG4_Background2D;
	if (!stricmp(node_name, "Billboard")) return TAG_MPEG4_Billboard;
	if (!stricmp(node_name, "Bitmap")) return TAG_MPEG4_Bitmap;
	if (!stricmp(node_name, "Box")) return TAG_MPEG4_Box;
	if (!stricmp(node_name, "Circle")) return TAG_MPEG4_Circle;
	if (!stricmp(node_name, "Collision")) return TAG_MPEG4_Collision;
	if (!stricmp(node_name, "Color")) return TAG_MPEG4_Color;
	if (!stricmp(node_name, "ColorInterpolator")) return TAG_MPEG4_ColorInterpolator;
	if (!stricmp(node_name, "CompositeTexture2D")) return TAG_MPEG4_CompositeTexture2D;
	if (!stricmp(node_name, "CompositeTexture3D")) return TAG_MPEG4_CompositeTexture3D;
	if (!stricmp(node_name, "Conditional")) return TAG_MPEG4_Conditional;
	if (!stricmp(node_name, "Cone")) return TAG_MPEG4_Cone;
	if (!stricmp(node_name, "Coordinate")) return TAG_MPEG4_Coordinate;
	if (!stricmp(node_name, "Coordinate2D")) return TAG_MPEG4_Coordinate2D;
	if (!stricmp(node_name, "CoordinateInterpolator")) return TAG_MPEG4_CoordinateInterpolator;
	if (!stricmp(node_name, "CoordinateInterpolator2D")) return TAG_MPEG4_CoordinateInterpolator2D;
	if (!stricmp(node_name, "Curve2D")) return TAG_MPEG4_Curve2D;
	if (!stricmp(node_name, "Cylinder")) return TAG_MPEG4_Cylinder;
	if (!stricmp(node_name, "CylinderSensor")) return TAG_MPEG4_CylinderSensor;
	if (!stricmp(node_name, "DirectionalLight")) return TAG_MPEG4_DirectionalLight;
	if (!stricmp(node_name, "DiscSensor")) return TAG_MPEG4_DiscSensor;
	if (!stricmp(node_name, "ElevationGrid")) return TAG_MPEG4_ElevationGrid;
	if (!stricmp(node_name, "Extrusion")) return TAG_MPEG4_Extrusion;
	if (!stricmp(node_name, "Fog")) return TAG_MPEG4_Fog;
	if (!stricmp(node_name, "FontStyle")) return TAG_MPEG4_FontStyle;
	if (!stricmp(node_name, "Form")) return TAG_MPEG4_Form;
	if (!stricmp(node_name, "Group")) return TAG_MPEG4_Group;
	if (!stricmp(node_name, "ImageTexture")) return TAG_MPEG4_ImageTexture;
	if (!stricmp(node_name, "IndexedFaceSet")) return TAG_MPEG4_IndexedFaceSet;
	if (!stricmp(node_name, "IndexedFaceSet2D")) return TAG_MPEG4_IndexedFaceSet2D;
	if (!stricmp(node_name, "IndexedLineSet")) return TAG_MPEG4_IndexedLineSet;
	if (!stricmp(node_name, "IndexedLineSet2D")) return TAG_MPEG4_IndexedLineSet2D;
	if (!stricmp(node_name, "Inline")) return TAG_MPEG4_Inline;
	if (!stricmp(node_name, "LOD")) return TAG_MPEG4_LOD;
	if (!stricmp(node_name, "Layer2D")) return TAG_MPEG4_Layer2D;
	if (!stricmp(node_name, "Layer3D")) return TAG_MPEG4_Layer3D;
	if (!stricmp(node_name, "Layout")) return TAG_MPEG4_Layout;
	if (!stricmp(node_name, "LineProperties")) return TAG_MPEG4_LineProperties;
	if (!stricmp(node_name, "ListeningPoint")) return TAG_MPEG4_ListeningPoint;
	if (!stricmp(node_name, "Material")) return TAG_MPEG4_Material;
	if (!stricmp(node_name, "Material2D")) return TAG_MPEG4_Material2D;
	if (!stricmp(node_name, "MovieTexture")) return TAG_MPEG4_MovieTexture;
	if (!stricmp(node_name, "NavigationInfo")) return TAG_MPEG4_NavigationInfo;
	if (!stricmp(node_name, "Normal")) return TAG_MPEG4_Normal;
	if (!stricmp(node_name, "NormalInterpolator")) return TAG_MPEG4_NormalInterpolator;
	if (!stricmp(node_name, "OrderedGroup")) return TAG_MPEG4_OrderedGroup;
	if (!stricmp(node_name, "OrientationInterpolator")) return TAG_MPEG4_OrientationInterpolator;
	if (!stricmp(node_name, "PixelTexture")) return TAG_MPEG4_PixelTexture;
	if (!stricmp(node_name, "PlaneSensor")) return TAG_MPEG4_PlaneSensor;
	if (!stricmp(node_name, "PlaneSensor2D")) return TAG_MPEG4_PlaneSensor2D;
	if (!stricmp(node_name, "PointLight")) return TAG_MPEG4_PointLight;
	if (!stricmp(node_name, "PointSet")) return TAG_MPEG4_PointSet;
	if (!stricmp(node_name, "PointSet2D")) return TAG_MPEG4_PointSet2D;
	if (!stricmp(node_name, "PositionInterpolator")) return TAG_MPEG4_PositionInterpolator;
	if (!stricmp(node_name, "PositionInterpolator2D")) return TAG_MPEG4_PositionInterpolator2D;
	if (!stricmp(node_name, "ProximitySensor2D")) return TAG_MPEG4_ProximitySensor2D;
	if (!stricmp(node_name, "ProximitySensor")) return TAG_MPEG4_ProximitySensor;
	if (!stricmp(node_name, "QuantizationParameter")) return TAG_MPEG4_QuantizationParameter;
	if (!stricmp(node_name, "Rectangle")) return TAG_MPEG4_Rectangle;
	if (!stricmp(node_name, "ScalarInterpolator")) return TAG_MPEG4_ScalarInterpolator;
	if (!stricmp(node_name, "Script")) return TAG_MPEG4_Script;
	if (!stricmp(node_name, "Shape")) return TAG_MPEG4_Shape;
	if (!stricmp(node_name, "Sound")) return TAG_MPEG4_Sound;
	if (!stricmp(node_name, "Sound2D")) return TAG_MPEG4_Sound2D;
	if (!stricmp(node_name, "Sphere")) return TAG_MPEG4_Sphere;
	if (!stricmp(node_name, "SphereSensor")) return TAG_MPEG4_SphereSensor;
	if (!stricmp(node_name, "SpotLight")) return TAG_MPEG4_SpotLight;
	if (!stricmp(node_name, "Switch")) return TAG_MPEG4_Switch;
	if (!stricmp(node_name, "TermCap")) return TAG_MPEG4_TermCap;
	if (!stricmp(node_name, "Text")) return TAG_MPEG4_Text;
	if (!stricmp(node_name, "TextureCoordinate")) return TAG_MPEG4_TextureCoordinate;
	if (!stricmp(node_name, "TextureTransform")) return TAG_MPEG4_TextureTransform;
	if (!stricmp(node_name, "TimeSensor")) return TAG_MPEG4_TimeSensor;
	if (!stricmp(node_name, "TouchSensor")) return TAG_MPEG4_TouchSensor;
	if (!stricmp(node_name, "Transform")) return TAG_MPEG4_Transform;
	if (!stricmp(node_name, "Transform2D")) return TAG_MPEG4_Transform2D;
	if (!stricmp(node_name, "Valuator")) return TAG_MPEG4_Valuator;
	if (!stricmp(node_name, "Viewpoint")) return TAG_MPEG4_Viewpoint;
	if (!stricmp(node_name, "VisibilitySensor")) return TAG_MPEG4_VisibilitySensor;
	if (!stricmp(node_name, "WorldInfo")) return TAG_MPEG4_WorldInfo;
	if (!stricmp(node_name, "AcousticMaterial")) return TAG_MPEG4_AcousticMaterial;
	if (!stricmp(node_name, "AcousticScene")) return TAG_MPEG4_AcousticScene;
	if (!stricmp(node_name, "ApplicationWindow")) return TAG_MPEG4_ApplicationWindow;
	if (!stricmp(node_name, "DirectiveSound")) return TAG_MPEG4_DirectiveSound;
	if (!stricmp(node_name, "Hierarchical3DMesh")) return TAG_MPEG4_Hierarchical3DMesh;
	if (!stricmp(node_name, "MaterialKey")) return TAG_MPEG4_MaterialKey;
	if (!stricmp(node_name, "PerceptualParameters")) return TAG_MPEG4_PerceptualParameters;
	if (!stricmp(node_name, "TemporalTransform")) return TAG_MPEG4_TemporalTransform;
	if (!stricmp(node_name, "TemporalGroup")) return TAG_MPEG4_TemporalGroup;
	if (!stricmp(node_name, "ServerCommand")) return TAG_MPEG4_ServerCommand;
	if (!stricmp(node_name, "InputSensor")) return TAG_MPEG4_InputSensor;
	if (!stricmp(node_name, "MatteTexture")) return TAG_MPEG4_MatteTexture;
	if (!stricmp(node_name, "MediaBuffer")) return TAG_MPEG4_MediaBuffer;
	if (!stricmp(node_name, "MediaControl")) return TAG_MPEG4_MediaControl;
	if (!stricmp(node_name, "MediaSensor")) return TAG_MPEG4_MediaSensor;
	if (!stricmp(node_name, "CoordinateInterpolator4D")) return TAG_MPEG4_CoordinateInterpolator4D;
	if (!stricmp(node_name, "NonLinearDeformer")) return TAG_MPEG4_NonLinearDeformer;
	if (!stricmp(node_name, "PositionAnimator")) return TAG_MPEG4_PositionAnimator;
	if (!stricmp(node_name, "PositionAnimator2D")) return TAG_MPEG4_PositionAnimator2D;
	if (!stricmp(node_name, "PositionInterpolator4D")) return TAG_MPEG4_PositionInterpolator4D;
	if (!stricmp(node_name, "ScalarAnimator")) return TAG_MPEG4_ScalarAnimator;
	if (!stricmp(node_name, "Clipper2D")) return TAG_MPEG4_Clipper2D;
	if (!stricmp(node_name, "ColorTransform")) return TAG_MPEG4_ColorTransform;
	if (!stricmp(node_name, "Ellipse")) return TAG_MPEG4_Ellipse;
	if (!stricmp(node_name, "LinearGradient")) return TAG_MPEG4_LinearGradient;
	if (!stricmp(node_name, "PathLayout")) return TAG_MPEG4_PathLayout;
	if (!stricmp(node_name, "RadialGradient")) return TAG_MPEG4_RadialGradient;
	if (!stricmp(node_name, "TransformMatrix2D")) return TAG_MPEG4_TransformMatrix2D;
	if (!stricmp(node_name, "Viewport")) return TAG_MPEG4_Viewport;
	if (!stricmp(node_name, "XCurve2D")) return TAG_MPEG4_XCurve2D;
	if (!stricmp(node_name, "XFontStyle")) return TAG_MPEG4_XFontStyle;
	if (!stricmp(node_name, "XLineProperties")) return TAG_MPEG4_XLineProperties;
	return 0;
}


/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Copyright (c) Jean Le Feuvre 2000-2005
 *					All rights reserved
 *
 *  This file is part of GPAC / X3D Scene Graph sub-project
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
	DO NOT MOFIFY - File generated on GMT Thu Aug 07 11:44:22 2008

	BY X3DGen for GPAC Version 0.4.5-DEV
*/


#include <gpac/nodes_x3d.h>

#include <gpac/internal/scenegraph_dev.h>

/*for NDT tag definitions*/
#include <gpac/nodes_mpeg4.h>

/*
	Anchor Node deletion
*/

static void Anchor_Del(GF_Node *node)
{
	X_Anchor *p = (X_Anchor *) node;
	gf_sg_sfstring_del(p->description);
	gf_sg_mfstring_del(p->parameter);
	gf_sg_mfurl_del(p->url);
	gf_node_unregister((GF_Node *) p->metadata, node);	
	gf_sg_vrml_parent_destroy(node);	
	gf_node_free((GF_Node *)p);
}


static u32 Anchor_get_field_count(GF_Node *node, u8 dummy)
{
	return 7;
}

static GF_Err Anchor_get_field(GF_Node *node, GF_FieldInfo *info)
{
	switch (info->fieldIndex) {
	case 0:
		info->name = "addChildren";
		info->eventType = GF_SG_EVENT_IN;
		info->on_event_in = ((X_Anchor *)node)->on_addChildren;
		info->fieldType = GF_SG_VRML_MFNODE;
		info->NDTtype = NDT_SF3DNode;
		info->far_ptr = & ((X_Anchor *)node)->addChildren;
		return GF_OK;
	case 1:
		info->name = "removeChildren";
		info->eventType = GF_SG_EVENT_IN;
		info->on_event_in = ((X_Anchor *)node)->on_removeChildren;
		info->fieldType = GF_SG_VRML_MFNODE;
		info->NDTtype = NDT_SF3DNode;
		info->far_ptr = & ((X_Anchor *)node)->removeChildren;
		return GF_OK;
	case 2:
		info->name = "children";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_MFNODE;
		info->NDTtype = NDT_SF3DNode;
		info->far_ptr = & ((X_Anchor *)node)->children;
		return GF_OK;
	case 3:
		info->name = "description";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFSTRING;
		info->far_ptr = & ((X_Anchor *) node)->description;
		return GF_OK;
	case 4:
		info->name = "parameter";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_MFSTRING;
		info->far_ptr = & ((X_Anchor *) node)->parameter;
		return GF_OK;
	case 5:
		info->name = "url";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_MFURL;
		info->far_ptr = & ((X_Anchor *) node)->url;
		return GF_OK;
	case 6:
		info->name = "metadata";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFNODE;
		info->NDTtype = NDT_SFMetadataNode;
		info->far_ptr = & ((X_Anchor *)node)->metadata;
		return GF_OK;
	default:
		return GF_BAD_PARAM;
	}
}


static s32 Anchor_get_field_index_by_name(char *name)
{
	if (!strcmp("addChildren", name)) return 0;
	if (!strcmp("removeChildren", name)) return 1;
	if (!strcmp("children", name)) return 2;
	if (!strcmp("description", name)) return 3;
	if (!strcmp("parameter", name)) return 4;
	if (!strcmp("url", name)) return 5;
	if (!strcmp("metadata", name)) return 6;
	return -1;
	}


static GF_Node *Anchor_Create()
{
	X_Anchor *p;
	GF_SAFEALLOC(p, X_Anchor);
	if(!p) return NULL;
	gf_node_setup((GF_Node *)p, TAG_X3D_Anchor);
	gf_sg_vrml_parent_setup((GF_Node *) p);

	/*default field values*/
	return (GF_Node *)p;
}


/*
	Appearance Node deletion
*/

static void Appearance_Del(GF_Node *node)
{
	X_Appearance *p = (X_Appearance *) node;
	gf_node_unregister((GF_Node *) p->material, node);	
	gf_node_unregister((GF_Node *) p->texture, node);	
	gf_node_unregister((GF_Node *) p->textureTransform, node);	
	gf_node_unregister((GF_Node *) p->fillProperties, node);	
	gf_node_unregister((GF_Node *) p->lineProperties, node);	
	gf_node_unregister((GF_Node *) p->metadata, node);	
	gf_node_free((GF_Node *)p);
}


static u32 Appearance_get_field_count(GF_Node *node, u8 dummy)
{
	return 6;
}

static GF_Err Appearance_get_field(GF_Node *node, GF_FieldInfo *info)
{
	switch (info->fieldIndex) {
	case 0:
		info->name = "material";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFNODE;
		info->NDTtype = NDT_SFMaterialNode;
		info->far_ptr = & ((X_Appearance *)node)->material;
		return GF_OK;
	case 1:
		info->name = "texture";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFNODE;
		info->NDTtype = NDT_SFTextureNode;
		info->far_ptr = & ((X_Appearance *)node)->texture;
		return GF_OK;
	case 2:
		info->name = "textureTransform";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFNODE;
		info->NDTtype = NDT_SFTextureTransformNode;
		info->far_ptr = & ((X_Appearance *)node)->textureTransform;
		return GF_OK;
	case 3:
		info->name = "fillProperties";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFNODE;
		info->NDTtype = NDT_SFFillPropertiesNode;
		info->far_ptr = & ((X_Appearance *)node)->fillProperties;
		return GF_OK;
	case 4:
		info->name = "lineProperties";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFNODE;
		info->NDTtype = NDT_SFX3DLinePropertiesNode;
		info->far_ptr = & ((X_Appearance *)node)->lineProperties;
		return GF_OK;
	case 5:
		info->name = "metadata";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFNODE;
		info->NDTtype = NDT_SFMetadataNode;
		info->far_ptr = & ((X_Appearance *)node)->metadata;
		return GF_OK;
	default:
		return GF_BAD_PARAM;
	}
}


static s32 Appearance_get_field_index_by_name(char *name)
{
	if (!strcmp("material", name)) return 0;
	if (!strcmp("texture", name)) return 1;
	if (!strcmp("textureTransform", name)) return 2;
	if (!strcmp("fillProperties", name)) return 3;
	if (!strcmp("lineProperties", name)) return 4;
	if (!strcmp("metadata", name)) return 5;
	return -1;
	}


static GF_Node *Appearance_Create()
{
	X_Appearance *p;
	GF_SAFEALLOC(p, X_Appearance);
	if(!p) return NULL;
	gf_node_setup((GF_Node *)p, TAG_X3D_Appearance);

	/*default field values*/
	return (GF_Node *)p;
}


/*
	Arc2D Node deletion
*/

static void Arc2D_Del(GF_Node *node)
{
	X_Arc2D *p = (X_Arc2D *) node;
	gf_node_unregister((GF_Node *) p->metadata, node);	
	gf_node_free((GF_Node *)p);
}


static u32 Arc2D_get_field_count(GF_Node *node, u8 dummy)
{
	return 4;
}

static GF_Err Arc2D_get_field(GF_Node *node, GF_FieldInfo *info)
{
	switch (info->fieldIndex) {
	case 0:
		info->name = "endAngle";
		info->eventType = GF_SG_EVENT_FIELD;
		info->fieldType = GF_SG_VRML_SFFLOAT;
		info->far_ptr = & ((X_Arc2D *) node)->endAngle;
		return GF_OK;
	case 1:
		info->name = "radius";
		info->eventType = GF_SG_EVENT_FIELD;
		info->fieldType = GF_SG_VRML_SFFLOAT;
		info->far_ptr = & ((X_Arc2D *) node)->radius;
		return GF_OK;
	case 2:
		info->name = "startAngle";
		info->eventType = GF_SG_EVENT_FIELD;
		info->fieldType = GF_SG_VRML_SFFLOAT;
		info->far_ptr = & ((X_Arc2D *) node)->startAngle;
		return GF_OK;
	case 3:
		info->name = "metadata";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFNODE;
		info->NDTtype = NDT_SFMetadataNode;
		info->far_ptr = & ((X_Arc2D *)node)->metadata;
		return GF_OK;
	default:
		return GF_BAD_PARAM;
	}
}


static s32 Arc2D_get_field_index_by_name(char *name)
{
	if (!strcmp("endAngle", name)) return 0;
	if (!strcmp("radius", name)) return 1;
	if (!strcmp("startAngle", name)) return 2;
	if (!strcmp("metadata", name)) return 3;
	return -1;
	}


static GF_Node *Arc2D_Create()
{
	X_Arc2D *p;
	GF_SAFEALLOC(p, X_Arc2D);
	if(!p) return NULL;
	gf_node_setup((GF_Node *)p, TAG_X3D_Arc2D);

	/*default field values*/
	p->endAngle = FLT2FIX(1.5707963);
	p->radius = FLT2FIX(1);
	p->startAngle = FLT2FIX(0);
	return (GF_Node *)p;
}


/*
	ArcClose2D Node deletion
*/

static void ArcClose2D_Del(GF_Node *node)
{
	X_ArcClose2D *p = (X_ArcClose2D *) node;
	gf_sg_sfstring_del(p->closureType);
	gf_node_unregister((GF_Node *) p->metadata, node);	
	gf_node_free((GF_Node *)p);
}


static u32 ArcClose2D_get_field_count(GF_Node *node, u8 dummy)
{
	return 5;
}

static GF_Err ArcClose2D_get_field(GF_Node *node, GF_FieldInfo *info)
{
	switch (info->fieldIndex) {
	case 0:
		info->name = "closureType";
		info->eventType = GF_SG_EVENT_FIELD;
		info->fieldType = GF_SG_VRML_SFSTRING;
		info->far_ptr = & ((X_ArcClose2D *) node)->closureType;
		return GF_OK;
	case 1:
		info->name = "endAngle";
		info->eventType = GF_SG_EVENT_FIELD;
		info->fieldType = GF_SG_VRML_SFFLOAT;
		info->far_ptr = & ((X_ArcClose2D *) node)->endAngle;
		return GF_OK;
	case 2:
		info->name = "radius";
		info->eventType = GF_SG_EVENT_FIELD;
		info->fieldType = GF_SG_VRML_SFFLOAT;
		info->far_ptr = & ((X_ArcClose2D *) node)->radius;
		return GF_OK;
	case 3:
		info->name = "startAngle";
		info->eventType = GF_SG_EVENT_FIELD;
		info->fieldType = GF_SG_VRML_SFFLOAT;
		info->far_ptr = & ((X_ArcClose2D *) node)->startAngle;
		return GF_OK;
	case 4:
		info->name = "metadata";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFNODE;
		info->NDTtype = NDT_SFMetadataNode;
		info->far_ptr = & ((X_ArcClose2D *)node)->metadata;
		return GF_OK;
	default:
		return GF_BAD_PARAM;
	}
}


static s32 ArcClose2D_get_field_index_by_name(char *name)
{
	if (!strcmp("closureType", name)) return 0;
	if (!strcmp("endAngle", name)) return 1;
	if (!strcmp("radius", name)) return 2;
	if (!strcmp("startAngle", name)) return 3;
	if (!strcmp("metadata", name)) return 4;
	return -1;
	}


static GF_Node *ArcClose2D_Create()
{
	X_ArcClose2D *p;
	GF_SAFEALLOC(p, X_ArcClose2D);
	if(!p) return NULL;
	gf_node_setup((GF_Node *)p, TAG_X3D_ArcClose2D);

	/*default field values*/
	p->closureType.buffer = (char*) malloc(sizeof(char) * 4);
	strcpy(p->closureType.buffer, "PIE");
	p->endAngle = FLT2FIX(1.5707963);
	p->radius = FLT2FIX(1);
	p->startAngle = FLT2FIX(0);
	return (GF_Node *)p;
}


/*
	AudioClip Node deletion
*/

static void AudioClip_Del(GF_Node *node)
{
	X_AudioClip *p = (X_AudioClip *) node;
	gf_sg_sfstring_del(p->description);
	gf_sg_mfurl_del(p->url);
	gf_node_unregister((GF_Node *) p->metadata, node);	
	gf_node_free((GF_Node *)p);
}


static u32 AudioClip_get_field_count(GF_Node *node, u8 dummy)
{
	return 13;
}

static GF_Err AudioClip_get_field(GF_Node *node, GF_FieldInfo *info)
{
	switch (info->fieldIndex) {
	case 0:
		info->name = "description";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFSTRING;
		info->far_ptr = & ((X_AudioClip *) node)->description;
		return GF_OK;
	case 1:
		info->name = "loop";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFBOOL;
		info->far_ptr = & ((X_AudioClip *) node)->loop;
		return GF_OK;
	case 2:
		info->name = "pitch";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFFLOAT;
		info->far_ptr = & ((X_AudioClip *) node)->pitch;
		return GF_OK;
	case 3:
		info->name = "startTime";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFTIME;
		info->far_ptr = & ((X_AudioClip *) node)->startTime;
		return GF_OK;
	case 4:
		info->name = "stopTime";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFTIME;
		info->far_ptr = & ((X_AudioClip *) node)->stopTime;
		return GF_OK;
	case 5:
		info->name = "url";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_MFURL;
		info->far_ptr = & ((X_AudioClip *) node)->url;
		return GF_OK;
	case 6:
		info->name = "duration_changed";
		info->eventType = GF_SG_EVENT_OUT;
		info->fieldType = GF_SG_VRML_SFTIME;
		info->far_ptr = & ((X_AudioClip *) node)->duration_changed;
		return GF_OK;
	case 7:
		info->name = "isActive";
		info->eventType = GF_SG_EVENT_OUT;
		info->fieldType = GF_SG_VRML_SFBOOL;
		info->far_ptr = & ((X_AudioClip *) node)->isActive;
		return GF_OK;
	case 8:
		info->name = "metadata";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFNODE;
		info->NDTtype = NDT_SFMetadataNode;
		info->far_ptr = & ((X_AudioClip *)node)->metadata;
		return GF_OK;
	case 9:
		info->name = "pauseTime";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFTIME;
		info->far_ptr = & ((X_AudioClip *) node)->pauseTime;
		return GF_OK;
	case 10:
		info->name = "resumeTime";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFTIME;
		info->far_ptr = & ((X_AudioClip *) node)->resumeTime;
		return GF_OK;
	case 11:
		info->name = "elapsedTime";
		info->eventType = GF_SG_EVENT_OUT;
		info->fieldType = GF_SG_VRML_SFTIME;
		info->far_ptr = & ((X_AudioClip *) node)->elapsedTime;
		return GF_OK;
	case 12:
		info->name = "isPaused";
		info->eventType = GF_SG_EVENT_OUT;
		info->fieldType = GF_SG_VRML_SFBOOL;
		info->far_ptr = & ((X_AudioClip *) node)->isPaused;
		return GF_OK;
	default:
		return GF_BAD_PARAM;
	}
}


static s32 AudioClip_get_field_index_by_name(char *name)
{
	if (!strcmp("description", name)) return 0;
	if (!strcmp("loop", name)) return 1;
	if (!strcmp("pitch", name)) return 2;
	if (!strcmp("startTime", name)) return 3;
	if (!strcmp("stopTime", name)) return 4;
	if (!strcmp("url", name)) return 5;
	if (!strcmp("duration_changed", name)) return 6;
	if (!strcmp("isActive", name)) return 7;
	if (!strcmp("metadata", name)) return 8;
	if (!strcmp("pauseTime", name)) return 9;
	if (!strcmp("resumeTime", name)) return 10;
	if (!strcmp("elapsedTime", name)) return 11;
	if (!strcmp("isPaused", name)) return 12;
	return -1;
	}


static GF_Node *AudioClip_Create()
{
	X_AudioClip *p;
	GF_SAFEALLOC(p, X_AudioClip);
	if(!p) return NULL;
	gf_node_setup((GF_Node *)p, TAG_X3D_AudioClip);

	/*default field values*/
	p->pitch = FLT2FIX(1.0);
	p->startTime = 0;
	p->stopTime = 0;
	p->pauseTime = 0;
	p->resumeTime = 0;
	return (GF_Node *)p;
}


/*
	Background Node deletion
*/

static void Background_Del(GF_Node *node)
{
	X_Background *p = (X_Background *) node;
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
	gf_node_unregister((GF_Node *) p->metadata, node);	
	gf_node_free((GF_Node *)p);
}


static u32 Background_get_field_count(GF_Node *node, u8 dummy)
{
	return 14;
}

static GF_Err Background_get_field(GF_Node *node, GF_FieldInfo *info)
{
	switch (info->fieldIndex) {
	case 0:
		info->name = "set_bind";
		info->eventType = GF_SG_EVENT_IN;
		info->on_event_in = ((X_Background *)node)->on_set_bind;
		info->fieldType = GF_SG_VRML_SFBOOL;
		info->far_ptr = & ((X_Background *) node)->set_bind;
		return GF_OK;
	case 1:
		info->name = "groundAngle";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_MFFLOAT;
		info->far_ptr = & ((X_Background *) node)->groundAngle;
		return GF_OK;
	case 2:
		info->name = "groundColor";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_MFCOLOR;
		info->far_ptr = & ((X_Background *) node)->groundColor;
		return GF_OK;
	case 3:
		info->name = "backUrl";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_MFURL;
		info->far_ptr = & ((X_Background *) node)->backUrl;
		return GF_OK;
	case 4:
		info->name = "bottomUrl";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_MFURL;
		info->far_ptr = & ((X_Background *) node)->bottomUrl;
		return GF_OK;
	case 5:
		info->name = "frontUrl";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_MFURL;
		info->far_ptr = & ((X_Background *) node)->frontUrl;
		return GF_OK;
	case 6:
		info->name = "leftUrl";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_MFURL;
		info->far_ptr = & ((X_Background *) node)->leftUrl;
		return GF_OK;
	case 7:
		info->name = "rightUrl";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_MFURL;
		info->far_ptr = & ((X_Background *) node)->rightUrl;
		return GF_OK;
	case 8:
		info->name = "topUrl";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_MFURL;
		info->far_ptr = & ((X_Background *) node)->topUrl;
		return GF_OK;
	case 9:
		info->name = "skyAngle";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_MFFLOAT;
		info->far_ptr = & ((X_Background *) node)->skyAngle;
		return GF_OK;
	case 10:
		info->name = "skyColor";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_MFCOLOR;
		info->far_ptr = & ((X_Background *) node)->skyColor;
		return GF_OK;
	case 11:
		info->name = "isBound";
		info->eventType = GF_SG_EVENT_OUT;
		info->fieldType = GF_SG_VRML_SFBOOL;
		info->far_ptr = & ((X_Background *) node)->isBound;
		return GF_OK;
	case 12:
		info->name = "metadata";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFNODE;
		info->NDTtype = NDT_SFMetadataNode;
		info->far_ptr = & ((X_Background *)node)->metadata;
		return GF_OK;
	case 13:
		info->name = "bindTime";
		info->eventType = GF_SG_EVENT_OUT;
		info->fieldType = GF_SG_VRML_SFTIME;
		info->far_ptr = & ((X_Background *) node)->bindTime;
		return GF_OK;
	default:
		return GF_BAD_PARAM;
	}
}


static s32 Background_get_field_index_by_name(char *name)
{
	if (!strcmp("set_bind", name)) return 0;
	if (!strcmp("groundAngle", name)) return 1;
	if (!strcmp("groundColor", name)) return 2;
	if (!strcmp("backUrl", name)) return 3;
	if (!strcmp("bottomUrl", name)) return 4;
	if (!strcmp("frontUrl", name)) return 5;
	if (!strcmp("leftUrl", name)) return 6;
	if (!strcmp("rightUrl", name)) return 7;
	if (!strcmp("topUrl", name)) return 8;
	if (!strcmp("skyAngle", name)) return 9;
	if (!strcmp("skyColor", name)) return 10;
	if (!strcmp("isBound", name)) return 11;
	if (!strcmp("metadata", name)) return 12;
	if (!strcmp("bindTime", name)) return 13;
	return -1;
	}


static GF_Node *Background_Create()
{
	X_Background *p;
	GF_SAFEALLOC(p, X_Background);
	if(!p) return NULL;
	gf_node_setup((GF_Node *)p, TAG_X3D_Background);

	/*default field values*/
	p->skyColor.vals = (SFColor*)malloc(sizeof(SFColor)*1);
	p->skyColor.count = 1;
	p->skyColor.vals[0].red = FLT2FIX(0);
	p->skyColor.vals[0].green = FLT2FIX(0);
	p->skyColor.vals[0].blue = FLT2FIX(0);
	return (GF_Node *)p;
}


/*
	Billboard Node deletion
*/

static void Billboard_Del(GF_Node *node)
{
	X_Billboard *p = (X_Billboard *) node;
	gf_node_unregister((GF_Node *) p->metadata, node);	
	gf_sg_vrml_parent_destroy(node);	
	gf_node_free((GF_Node *)p);
}


static u32 Billboard_get_field_count(GF_Node *node, u8 dummy)
{
	return 5;
}

static GF_Err Billboard_get_field(GF_Node *node, GF_FieldInfo *info)
{
	switch (info->fieldIndex) {
	case 0:
		info->name = "addChildren";
		info->eventType = GF_SG_EVENT_IN;
		info->on_event_in = ((X_Billboard *)node)->on_addChildren;
		info->fieldType = GF_SG_VRML_MFNODE;
		info->NDTtype = NDT_SF3DNode;
		info->far_ptr = & ((X_Billboard *)node)->addChildren;
		return GF_OK;
	case 1:
		info->name = "removeChildren";
		info->eventType = GF_SG_EVENT_IN;
		info->on_event_in = ((X_Billboard *)node)->on_removeChildren;
		info->fieldType = GF_SG_VRML_MFNODE;
		info->NDTtype = NDT_SF3DNode;
		info->far_ptr = & ((X_Billboard *)node)->removeChildren;
		return GF_OK;
	case 2:
		info->name = "children";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_MFNODE;
		info->NDTtype = NDT_SF3DNode;
		info->far_ptr = & ((X_Billboard *)node)->children;
		return GF_OK;
	case 3:
		info->name = "axisOfRotation";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFVEC3F;
		info->far_ptr = & ((X_Billboard *) node)->axisOfRotation;
		return GF_OK;
	case 4:
		info->name = "metadata";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFNODE;
		info->NDTtype = NDT_SFMetadataNode;
		info->far_ptr = & ((X_Billboard *)node)->metadata;
		return GF_OK;
	default:
		return GF_BAD_PARAM;
	}
}


static s32 Billboard_get_field_index_by_name(char *name)
{
	if (!strcmp("addChildren", name)) return 0;
	if (!strcmp("removeChildren", name)) return 1;
	if (!strcmp("children", name)) return 2;
	if (!strcmp("axisOfRotation", name)) return 3;
	if (!strcmp("metadata", name)) return 4;
	return -1;
	}


static GF_Node *Billboard_Create()
{
	X_Billboard *p;
	GF_SAFEALLOC(p, X_Billboard);
	if(!p) return NULL;
	gf_node_setup((GF_Node *)p, TAG_X3D_Billboard);
	gf_sg_vrml_parent_setup((GF_Node *) p);

	/*default field values*/
	p->axisOfRotation.x = FLT2FIX(0);
	p->axisOfRotation.y = FLT2FIX(1);
	p->axisOfRotation.z = FLT2FIX(0);
	return (GF_Node *)p;
}


/*
	BooleanFilter Node deletion
*/

static void BooleanFilter_Del(GF_Node *node)
{
	X_BooleanFilter *p = (X_BooleanFilter *) node;
	gf_node_unregister((GF_Node *) p->metadata, node);	
	gf_node_free((GF_Node *)p);
}


static u32 BooleanFilter_get_field_count(GF_Node *node, u8 dummy)
{
	return 5;
}

static GF_Err BooleanFilter_get_field(GF_Node *node, GF_FieldInfo *info)
{
	switch (info->fieldIndex) {
	case 0:
		info->name = "set_boolean";
		info->eventType = GF_SG_EVENT_IN;
		info->on_event_in = ((X_BooleanFilter *)node)->on_set_boolean;
		info->fieldType = GF_SG_VRML_SFBOOL;
		info->far_ptr = & ((X_BooleanFilter *) node)->set_boolean;
		return GF_OK;
	case 1:
		info->name = "inputFalse";
		info->eventType = GF_SG_EVENT_OUT;
		info->fieldType = GF_SG_VRML_SFBOOL;
		info->far_ptr = & ((X_BooleanFilter *) node)->inputFalse;
		return GF_OK;
	case 2:
		info->name = "inputNegate";
		info->eventType = GF_SG_EVENT_OUT;
		info->fieldType = GF_SG_VRML_SFBOOL;
		info->far_ptr = & ((X_BooleanFilter *) node)->inputNegate;
		return GF_OK;
	case 3:
		info->name = "inputTrue";
		info->eventType = GF_SG_EVENT_OUT;
		info->fieldType = GF_SG_VRML_SFBOOL;
		info->far_ptr = & ((X_BooleanFilter *) node)->inputTrue;
		return GF_OK;
	case 4:
		info->name = "metadata";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFNODE;
		info->NDTtype = NDT_SFMetadataNode;
		info->far_ptr = & ((X_BooleanFilter *)node)->metadata;
		return GF_OK;
	default:
		return GF_BAD_PARAM;
	}
}


static s32 BooleanFilter_get_field_index_by_name(char *name)
{
	if (!strcmp("set_boolean", name)) return 0;
	if (!strcmp("inputFalse", name)) return 1;
	if (!strcmp("inputNegate", name)) return 2;
	if (!strcmp("inputTrue", name)) return 3;
	if (!strcmp("metadata", name)) return 4;
	return -1;
	}


static GF_Node *BooleanFilter_Create()
{
	X_BooleanFilter *p;
	GF_SAFEALLOC(p, X_BooleanFilter);
	if(!p) return NULL;
	gf_node_setup((GF_Node *)p, TAG_X3D_BooleanFilter);

	/*default field values*/
	return (GF_Node *)p;
}


/*
	BooleanSequencer Node deletion
*/

static void BooleanSequencer_Del(GF_Node *node)
{
	X_BooleanSequencer *p = (X_BooleanSequencer *) node;
	gf_sg_mffloat_del(p->key);
	gf_sg_mfbool_del(p->keyValue);
	gf_node_unregister((GF_Node *) p->metadata, node);	
	gf_node_free((GF_Node *)p);
}


static u32 BooleanSequencer_get_field_count(GF_Node *node, u8 dummy)
{
	return 7;
}

static GF_Err BooleanSequencer_get_field(GF_Node *node, GF_FieldInfo *info)
{
	switch (info->fieldIndex) {
	case 0:
		info->name = "next";
		info->eventType = GF_SG_EVENT_IN;
		info->on_event_in = ((X_BooleanSequencer *)node)->on_next;
		info->fieldType = GF_SG_VRML_SFBOOL;
		info->far_ptr = & ((X_BooleanSequencer *) node)->next;
		return GF_OK;
	case 1:
		info->name = "previous";
		info->eventType = GF_SG_EVENT_IN;
		info->on_event_in = ((X_BooleanSequencer *)node)->on_previous;
		info->fieldType = GF_SG_VRML_SFBOOL;
		info->far_ptr = & ((X_BooleanSequencer *) node)->previous;
		return GF_OK;
	case 2:
		info->name = "set_fraction";
		info->eventType = GF_SG_EVENT_IN;
		info->on_event_in = ((X_BooleanSequencer *)node)->on_set_fraction;
		info->fieldType = GF_SG_VRML_SFFLOAT;
		info->far_ptr = & ((X_BooleanSequencer *) node)->set_fraction;
		return GF_OK;
	case 3:
		info->name = "key";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_MFFLOAT;
		info->far_ptr = & ((X_BooleanSequencer *) node)->key;
		return GF_OK;
	case 4:
		info->name = "keyValue";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_MFBOOL;
		info->far_ptr = & ((X_BooleanSequencer *) node)->keyValue;
		return GF_OK;
	case 5:
		info->name = "value_changed";
		info->eventType = GF_SG_EVENT_OUT;
		info->fieldType = GF_SG_VRML_SFBOOL;
		info->far_ptr = & ((X_BooleanSequencer *) node)->value_changed;
		return GF_OK;
	case 6:
		info->name = "metadata";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFNODE;
		info->NDTtype = NDT_SFMetadataNode;
		info->far_ptr = & ((X_BooleanSequencer *)node)->metadata;
		return GF_OK;
	default:
		return GF_BAD_PARAM;
	}
}


static s32 BooleanSequencer_get_field_index_by_name(char *name)
{
	if (!strcmp("next", name)) return 0;
	if (!strcmp("previous", name)) return 1;
	if (!strcmp("set_fraction", name)) return 2;
	if (!strcmp("key", name)) return 3;
	if (!strcmp("keyValue", name)) return 4;
	if (!strcmp("value_changed", name)) return 5;
	if (!strcmp("metadata", name)) return 6;
	return -1;
	}


static GF_Node *BooleanSequencer_Create()
{
	X_BooleanSequencer *p;
	GF_SAFEALLOC(p, X_BooleanSequencer);
	if(!p) return NULL;
	gf_node_setup((GF_Node *)p, TAG_X3D_BooleanSequencer);

	/*default field values*/
	return (GF_Node *)p;
}


/*
	BooleanToggle Node deletion
*/

static void BooleanToggle_Del(GF_Node *node)
{
	X_BooleanToggle *p = (X_BooleanToggle *) node;
	gf_node_unregister((GF_Node *) p->metadata, node);	
	gf_node_free((GF_Node *)p);
}


static u32 BooleanToggle_get_field_count(GF_Node *node, u8 dummy)
{
	return 3;
}

static GF_Err BooleanToggle_get_field(GF_Node *node, GF_FieldInfo *info)
{
	switch (info->fieldIndex) {
	case 0:
		info->name = "set_boolean";
		info->eventType = GF_SG_EVENT_IN;
		info->on_event_in = ((X_BooleanToggle *)node)->on_set_boolean;
		info->fieldType = GF_SG_VRML_SFBOOL;
		info->far_ptr = & ((X_BooleanToggle *) node)->set_boolean;
		return GF_OK;
	case 1:
		info->name = "toggle";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFBOOL;
		info->far_ptr = & ((X_BooleanToggle *) node)->toggle;
		return GF_OK;
	case 2:
		info->name = "metadata";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFNODE;
		info->NDTtype = NDT_SFMetadataNode;
		info->far_ptr = & ((X_BooleanToggle *)node)->metadata;
		return GF_OK;
	default:
		return GF_BAD_PARAM;
	}
}


static s32 BooleanToggle_get_field_index_by_name(char *name)
{
	if (!strcmp("set_boolean", name)) return 0;
	if (!strcmp("toggle", name)) return 1;
	if (!strcmp("metadata", name)) return 2;
	return -1;
	}


static GF_Node *BooleanToggle_Create()
{
	X_BooleanToggle *p;
	GF_SAFEALLOC(p, X_BooleanToggle);
	if(!p) return NULL;
	gf_node_setup((GF_Node *)p, TAG_X3D_BooleanToggle);

	/*default field values*/
	return (GF_Node *)p;
}


/*
	BooleanTrigger Node deletion
*/

static void BooleanTrigger_Del(GF_Node *node)
{
	X_BooleanTrigger *p = (X_BooleanTrigger *) node;
	gf_node_unregister((GF_Node *) p->metadata, node);	
	gf_node_free((GF_Node *)p);
}


static u32 BooleanTrigger_get_field_count(GF_Node *node, u8 dummy)
{
	return 3;
}

static GF_Err BooleanTrigger_get_field(GF_Node *node, GF_FieldInfo *info)
{
	switch (info->fieldIndex) {
	case 0:
		info->name = "set_triggerTime";
		info->eventType = GF_SG_EVENT_IN;
		info->on_event_in = ((X_BooleanTrigger *)node)->on_set_triggerTime;
		info->fieldType = GF_SG_VRML_SFTIME;
		info->far_ptr = & ((X_BooleanTrigger *) node)->set_triggerTime;
		return GF_OK;
	case 1:
		info->name = "triggerTrue";
		info->eventType = GF_SG_EVENT_OUT;
		info->fieldType = GF_SG_VRML_SFBOOL;
		info->far_ptr = & ((X_BooleanTrigger *) node)->triggerTrue;
		return GF_OK;
	case 2:
		info->name = "metadata";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFNODE;
		info->NDTtype = NDT_SFMetadataNode;
		info->far_ptr = & ((X_BooleanTrigger *)node)->metadata;
		return GF_OK;
	default:
		return GF_BAD_PARAM;
	}
}


static s32 BooleanTrigger_get_field_index_by_name(char *name)
{
	if (!strcmp("set_triggerTime", name)) return 0;
	if (!strcmp("triggerTrue", name)) return 1;
	if (!strcmp("metadata", name)) return 2;
	return -1;
	}


static GF_Node *BooleanTrigger_Create()
{
	X_BooleanTrigger *p;
	GF_SAFEALLOC(p, X_BooleanTrigger);
	if(!p) return NULL;
	gf_node_setup((GF_Node *)p, TAG_X3D_BooleanTrigger);

	/*default field values*/
	return (GF_Node *)p;
}


/*
	Box Node deletion
*/

static void Box_Del(GF_Node *node)
{
	X_Box *p = (X_Box *) node;
	gf_node_unregister((GF_Node *) p->metadata, node);	
	gf_node_free((GF_Node *)p);
}


static u32 Box_get_field_count(GF_Node *node, u8 dummy)
{
	return 2;
}

static GF_Err Box_get_field(GF_Node *node, GF_FieldInfo *info)
{
	switch (info->fieldIndex) {
	case 0:
		info->name = "size";
		info->eventType = GF_SG_EVENT_FIELD;
		info->fieldType = GF_SG_VRML_SFVEC3F;
		info->far_ptr = & ((X_Box *) node)->size;
		return GF_OK;
	case 1:
		info->name = "metadata";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFNODE;
		info->NDTtype = NDT_SFMetadataNode;
		info->far_ptr = & ((X_Box *)node)->metadata;
		return GF_OK;
	default:
		return GF_BAD_PARAM;
	}
}


static s32 Box_get_field_index_by_name(char *name)
{
	if (!strcmp("size", name)) return 0;
	if (!strcmp("metadata", name)) return 1;
	return -1;
	}


static GF_Node *Box_Create()
{
	X_Box *p;
	GF_SAFEALLOC(p, X_Box);
	if(!p) return NULL;
	gf_node_setup((GF_Node *)p, TAG_X3D_Box);

	/*default field values*/
	p->size.x = FLT2FIX(2);
	p->size.y = FLT2FIX(2);
	p->size.z = FLT2FIX(2);
	return (GF_Node *)p;
}


/*
	Circle2D Node deletion
*/

static void Circle2D_Del(GF_Node *node)
{
	X_Circle2D *p = (X_Circle2D *) node;
	gf_node_unregister((GF_Node *) p->metadata, node);	
	gf_node_free((GF_Node *)p);
}


static u32 Circle2D_get_field_count(GF_Node *node, u8 dummy)
{
	return 2;
}

static GF_Err Circle2D_get_field(GF_Node *node, GF_FieldInfo *info)
{
	switch (info->fieldIndex) {
	case 0:
		info->name = "radius";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFFLOAT;
		info->far_ptr = & ((X_Circle2D *) node)->radius;
		return GF_OK;
	case 1:
		info->name = "metadata";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFNODE;
		info->NDTtype = NDT_SFMetadataNode;
		info->far_ptr = & ((X_Circle2D *)node)->metadata;
		return GF_OK;
	default:
		return GF_BAD_PARAM;
	}
}


static s32 Circle2D_get_field_index_by_name(char *name)
{
	if (!strcmp("radius", name)) return 0;
	if (!strcmp("metadata", name)) return 1;
	return -1;
	}


static GF_Node *Circle2D_Create()
{
	X_Circle2D *p;
	GF_SAFEALLOC(p, X_Circle2D);
	if(!p) return NULL;
	gf_node_setup((GF_Node *)p, TAG_X3D_Circle2D);

	/*default field values*/
	p->radius = FLT2FIX(1);
	return (GF_Node *)p;
}


/*
	Collision Node deletion
*/

static void Collision_Del(GF_Node *node)
{
	X_Collision *p = (X_Collision *) node;
	gf_node_unregister((GF_Node *) p->proxy, node);	
	gf_node_unregister((GF_Node *) p->metadata, node);	
	gf_sg_vrml_parent_destroy(node);	
	gf_node_free((GF_Node *)p);
}


static u32 Collision_get_field_count(GF_Node *node, u8 dummy)
{
	return 8;
}

static GF_Err Collision_get_field(GF_Node *node, GF_FieldInfo *info)
{
	switch (info->fieldIndex) {
	case 0:
		info->name = "addChildren";
		info->eventType = GF_SG_EVENT_IN;
		info->on_event_in = ((X_Collision *)node)->on_addChildren;
		info->fieldType = GF_SG_VRML_MFNODE;
		info->NDTtype = NDT_SF3DNode;
		info->far_ptr = & ((X_Collision *)node)->addChildren;
		return GF_OK;
	case 1:
		info->name = "removeChildren";
		info->eventType = GF_SG_EVENT_IN;
		info->on_event_in = ((X_Collision *)node)->on_removeChildren;
		info->fieldType = GF_SG_VRML_MFNODE;
		info->NDTtype = NDT_SF3DNode;
		info->far_ptr = & ((X_Collision *)node)->removeChildren;
		return GF_OK;
	case 2:
		info->name = "children";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_MFNODE;
		info->NDTtype = NDT_SF3DNode;
		info->far_ptr = & ((X_Collision *)node)->children;
		return GF_OK;
	case 3:
		info->name = "enabled";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFBOOL;
		info->far_ptr = & ((X_Collision *) node)->enabled;
		return GF_OK;
	case 4:
		info->name = "proxy";
		info->eventType = GF_SG_EVENT_FIELD;
		info->fieldType = GF_SG_VRML_SFNODE;
		info->NDTtype = NDT_SF3DNode;
		info->far_ptr = & ((X_Collision *)node)->proxy;
		return GF_OK;
	case 5:
		info->name = "collideTime";
		info->eventType = GF_SG_EVENT_OUT;
		info->fieldType = GF_SG_VRML_SFTIME;
		info->far_ptr = & ((X_Collision *) node)->collideTime;
		return GF_OK;
	case 6:
		info->name = "isActive";
		info->eventType = GF_SG_EVENT_OUT;
		info->fieldType = GF_SG_VRML_SFBOOL;
		info->far_ptr = & ((X_Collision *) node)->isActive;
		return GF_OK;
	case 7:
		info->name = "metadata";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFNODE;
		info->NDTtype = NDT_SFMetadataNode;
		info->far_ptr = & ((X_Collision *)node)->metadata;
		return GF_OK;
	default:
		return GF_BAD_PARAM;
	}
}


static s32 Collision_get_field_index_by_name(char *name)
{
	if (!strcmp("addChildren", name)) return 0;
	if (!strcmp("removeChildren", name)) return 1;
	if (!strcmp("children", name)) return 2;
	if (!strcmp("enabled", name)) return 3;
	if (!strcmp("proxy", name)) return 4;
	if (!strcmp("collideTime", name)) return 5;
	if (!strcmp("isActive", name)) return 6;
	if (!strcmp("metadata", name)) return 7;
	return -1;
	}


static GF_Node *Collision_Create()
{
	X_Collision *p;
	GF_SAFEALLOC(p, X_Collision);
	if(!p) return NULL;
	gf_node_setup((GF_Node *)p, TAG_X3D_Collision);
	gf_sg_vrml_parent_setup((GF_Node *) p);

	/*default field values*/
	p->enabled = 1;
	return (GF_Node *)p;
}


/*
	Color Node deletion
*/

static void Color_Del(GF_Node *node)
{
	X_Color *p = (X_Color *) node;
	gf_sg_mfcolor_del(p->color);
	gf_node_unregister((GF_Node *) p->metadata, node);	
	gf_node_free((GF_Node *)p);
}


static u32 Color_get_field_count(GF_Node *node, u8 dummy)
{
	return 2;
}

static GF_Err Color_get_field(GF_Node *node, GF_FieldInfo *info)
{
	switch (info->fieldIndex) {
	case 0:
		info->name = "color";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_MFCOLOR;
		info->far_ptr = & ((X_Color *) node)->color;
		return GF_OK;
	case 1:
		info->name = "metadata";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFNODE;
		info->NDTtype = NDT_SFMetadataNode;
		info->far_ptr = & ((X_Color *)node)->metadata;
		return GF_OK;
	default:
		return GF_BAD_PARAM;
	}
}


static s32 Color_get_field_index_by_name(char *name)
{
	if (!strcmp("color", name)) return 0;
	if (!strcmp("metadata", name)) return 1;
	return -1;
	}


static GF_Node *Color_Create()
{
	X_Color *p;
	GF_SAFEALLOC(p, X_Color);
	if(!p) return NULL;
	gf_node_setup((GF_Node *)p, TAG_X3D_Color);

	/*default field values*/
	return (GF_Node *)p;
}


/*
	ColorInterpolator Node deletion
*/

static void ColorInterpolator_Del(GF_Node *node)
{
	X_ColorInterpolator *p = (X_ColorInterpolator *) node;
	gf_sg_mffloat_del(p->key);
	gf_sg_mfcolor_del(p->keyValue);
	gf_node_unregister((GF_Node *) p->metadata, node);	
	gf_node_free((GF_Node *)p);
}


static u32 ColorInterpolator_get_field_count(GF_Node *node, u8 dummy)
{
	return 5;
}

static GF_Err ColorInterpolator_get_field(GF_Node *node, GF_FieldInfo *info)
{
	switch (info->fieldIndex) {
	case 0:
		info->name = "set_fraction";
		info->eventType = GF_SG_EVENT_IN;
		info->on_event_in = ((X_ColorInterpolator *)node)->on_set_fraction;
		info->fieldType = GF_SG_VRML_SFFLOAT;
		info->far_ptr = & ((X_ColorInterpolator *) node)->set_fraction;
		return GF_OK;
	case 1:
		info->name = "key";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_MFFLOAT;
		info->far_ptr = & ((X_ColorInterpolator *) node)->key;
		return GF_OK;
	case 2:
		info->name = "keyValue";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_MFCOLOR;
		info->far_ptr = & ((X_ColorInterpolator *) node)->keyValue;
		return GF_OK;
	case 3:
		info->name = "value_changed";
		info->eventType = GF_SG_EVENT_OUT;
		info->fieldType = GF_SG_VRML_SFCOLOR;
		info->far_ptr = & ((X_ColorInterpolator *) node)->value_changed;
		return GF_OK;
	case 4:
		info->name = "metadata";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFNODE;
		info->NDTtype = NDT_SFMetadataNode;
		info->far_ptr = & ((X_ColorInterpolator *)node)->metadata;
		return GF_OK;
	default:
		return GF_BAD_PARAM;
	}
}


static s32 ColorInterpolator_get_field_index_by_name(char *name)
{
	if (!strcmp("set_fraction", name)) return 0;
	if (!strcmp("key", name)) return 1;
	if (!strcmp("keyValue", name)) return 2;
	if (!strcmp("value_changed", name)) return 3;
	if (!strcmp("metadata", name)) return 4;
	return -1;
	}


static GF_Node *ColorInterpolator_Create()
{
	X_ColorInterpolator *p;
	GF_SAFEALLOC(p, X_ColorInterpolator);
	if(!p) return NULL;
	gf_node_setup((GF_Node *)p, TAG_X3D_ColorInterpolator);

	/*default field values*/
	return (GF_Node *)p;
}


/*
	ColorRGBA Node deletion
*/

static void ColorRGBA_Del(GF_Node *node)
{
	X_ColorRGBA *p = (X_ColorRGBA *) node;
	gf_node_unregister((GF_Node *) p->metadata, node);	
	gf_node_free((GF_Node *)p);
}


static u32 ColorRGBA_get_field_count(GF_Node *node, u8 dummy)
{
	return 2;
}

static GF_Err ColorRGBA_get_field(GF_Node *node, GF_FieldInfo *info)
{
	switch (info->fieldIndex) {
	case 0:
		info->name = "color";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_MFCOLORRGBA;
		info->far_ptr = & ((X_ColorRGBA *) node)->color;
		return GF_OK;
	case 1:
		info->name = "metadata";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFNODE;
		info->NDTtype = NDT_SFMetadataNode;
		info->far_ptr = & ((X_ColorRGBA *)node)->metadata;
		return GF_OK;
	default:
		return GF_BAD_PARAM;
	}
}


static s32 ColorRGBA_get_field_index_by_name(char *name)
{
	if (!strcmp("color", name)) return 0;
	if (!strcmp("metadata", name)) return 1;
	return -1;
	}


static GF_Node *ColorRGBA_Create()
{
	X_ColorRGBA *p;
	GF_SAFEALLOC(p, X_ColorRGBA);
	if(!p) return NULL;
	gf_node_setup((GF_Node *)p, TAG_X3D_ColorRGBA);

	/*default field values*/
	return (GF_Node *)p;
}


/*
	Cone Node deletion
*/

static void Cone_Del(GF_Node *node)
{
	X_Cone *p = (X_Cone *) node;
	gf_node_unregister((GF_Node *) p->metadata, node);	
	gf_node_free((GF_Node *)p);
}


static u32 Cone_get_field_count(GF_Node *node, u8 dummy)
{
	return 5;
}

static GF_Err Cone_get_field(GF_Node *node, GF_FieldInfo *info)
{
	switch (info->fieldIndex) {
	case 0:
		info->name = "bottomRadius";
		info->eventType = GF_SG_EVENT_FIELD;
		info->fieldType = GF_SG_VRML_SFFLOAT;
		info->far_ptr = & ((X_Cone *) node)->bottomRadius;
		return GF_OK;
	case 1:
		info->name = "height";
		info->eventType = GF_SG_EVENT_FIELD;
		info->fieldType = GF_SG_VRML_SFFLOAT;
		info->far_ptr = & ((X_Cone *) node)->height;
		return GF_OK;
	case 2:
		info->name = "side";
		info->eventType = GF_SG_EVENT_FIELD;
		info->fieldType = GF_SG_VRML_SFBOOL;
		info->far_ptr = & ((X_Cone *) node)->side;
		return GF_OK;
	case 3:
		info->name = "bottom";
		info->eventType = GF_SG_EVENT_FIELD;
		info->fieldType = GF_SG_VRML_SFBOOL;
		info->far_ptr = & ((X_Cone *) node)->bottom;
		return GF_OK;
	case 4:
		info->name = "metadata";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFNODE;
		info->NDTtype = NDT_SFMetadataNode;
		info->far_ptr = & ((X_Cone *)node)->metadata;
		return GF_OK;
	default:
		return GF_BAD_PARAM;
	}
}


static s32 Cone_get_field_index_by_name(char *name)
{
	if (!strcmp("bottomRadius", name)) return 0;
	if (!strcmp("height", name)) return 1;
	if (!strcmp("side", name)) return 2;
	if (!strcmp("bottom", name)) return 3;
	if (!strcmp("metadata", name)) return 4;
	return -1;
	}


static GF_Node *Cone_Create()
{
	X_Cone *p;
	GF_SAFEALLOC(p, X_Cone);
	if(!p) return NULL;
	gf_node_setup((GF_Node *)p, TAG_X3D_Cone);

	/*default field values*/
	p->bottomRadius = FLT2FIX(1);
	p->height = FLT2FIX(2);
	p->side = 1;
	p->bottom = 1;
	return (GF_Node *)p;
}


/*
	Contour2D Node deletion
*/

static void Contour2D_Del(GF_Node *node)
{
	X_Contour2D *p = (X_Contour2D *) node;
	gf_node_unregister((GF_Node *) p->metadata, node);	
	gf_sg_vrml_parent_destroy(node);	
	gf_node_free((GF_Node *)p);
}


static u32 Contour2D_get_field_count(GF_Node *node, u8 dummy)
{
	return 4;
}

static GF_Err Contour2D_get_field(GF_Node *node, GF_FieldInfo *info)
{
	switch (info->fieldIndex) {
	case 0:
		info->name = "addChildren";
		info->eventType = GF_SG_EVENT_IN;
		info->on_event_in = ((X_Contour2D *)node)->on_addChildren;
		info->fieldType = GF_SG_VRML_MFNODE;
		info->NDTtype = NDT_SFNurbsControlCurveNode;
		info->far_ptr = & ((X_Contour2D *)node)->addChildren;
		return GF_OK;
	case 1:
		info->name = "removeChildren";
		info->eventType = GF_SG_EVENT_IN;
		info->on_event_in = ((X_Contour2D *)node)->on_removeChildren;
		info->fieldType = GF_SG_VRML_MFNODE;
		info->NDTtype = NDT_SFNurbsControlCurveNode;
		info->far_ptr = & ((X_Contour2D *)node)->removeChildren;
		return GF_OK;
	case 2:
		info->name = "children";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_MFNODE;
		info->NDTtype = NDT_SFNurbsControlCurveNode;
		info->far_ptr = & ((X_Contour2D *)node)->children;
		return GF_OK;
	case 3:
		info->name = "metadata";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFNODE;
		info->NDTtype = NDT_SFMetadataNode;
		info->far_ptr = & ((X_Contour2D *)node)->metadata;
		return GF_OK;
	default:
		return GF_BAD_PARAM;
	}
}


static s32 Contour2D_get_field_index_by_name(char *name)
{
	if (!strcmp("addChildren", name)) return 0;
	if (!strcmp("removeChildren", name)) return 1;
	if (!strcmp("children", name)) return 2;
	if (!strcmp("metadata", name)) return 3;
	return -1;
	}


static GF_Node *Contour2D_Create()
{
	X_Contour2D *p;
	GF_SAFEALLOC(p, X_Contour2D);
	if(!p) return NULL;
	gf_node_setup((GF_Node *)p, TAG_X3D_Contour2D);
	gf_sg_vrml_parent_setup((GF_Node *) p);

	/*default field values*/
	return (GF_Node *)p;
}


/*
	ContourPolyline2D Node deletion
*/

static void ContourPolyline2D_Del(GF_Node *node)
{
	X_ContourPolyline2D *p = (X_ContourPolyline2D *) node;
	gf_sg_mfvec2f_del(p->point);
	gf_node_unregister((GF_Node *) p->metadata, node);	
	gf_node_free((GF_Node *)p);
}


static u32 ContourPolyline2D_get_field_count(GF_Node *node, u8 dummy)
{
	return 2;
}

static GF_Err ContourPolyline2D_get_field(GF_Node *node, GF_FieldInfo *info)
{
	switch (info->fieldIndex) {
	case 0:
		info->name = "point";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_MFVEC2F;
		info->far_ptr = & ((X_ContourPolyline2D *) node)->point;
		return GF_OK;
	case 1:
		info->name = "metadata";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFNODE;
		info->NDTtype = NDT_SFMetadataNode;
		info->far_ptr = & ((X_ContourPolyline2D *)node)->metadata;
		return GF_OK;
	default:
		return GF_BAD_PARAM;
	}
}


static s32 ContourPolyline2D_get_field_index_by_name(char *name)
{
	if (!strcmp("point", name)) return 0;
	if (!strcmp("metadata", name)) return 1;
	return -1;
	}


static GF_Node *ContourPolyline2D_Create()
{
	X_ContourPolyline2D *p;
	GF_SAFEALLOC(p, X_ContourPolyline2D);
	if(!p) return NULL;
	gf_node_setup((GF_Node *)p, TAG_X3D_ContourPolyline2D);

	/*default field values*/
	return (GF_Node *)p;
}


/*
	Coordinate Node deletion
*/

static void Coordinate_Del(GF_Node *node)
{
	X_Coordinate *p = (X_Coordinate *) node;
	gf_sg_mfvec3f_del(p->point);
	gf_node_unregister((GF_Node *) p->metadata, node);	
	gf_node_free((GF_Node *)p);
}


static u32 Coordinate_get_field_count(GF_Node *node, u8 dummy)
{
	return 2;
}

static GF_Err Coordinate_get_field(GF_Node *node, GF_FieldInfo *info)
{
	switch (info->fieldIndex) {
	case 0:
		info->name = "point";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_MFVEC3F;
		info->far_ptr = & ((X_Coordinate *) node)->point;
		return GF_OK;
	case 1:
		info->name = "metadata";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFNODE;
		info->NDTtype = NDT_SFMetadataNode;
		info->far_ptr = & ((X_Coordinate *)node)->metadata;
		return GF_OK;
	default:
		return GF_BAD_PARAM;
	}
}


static s32 Coordinate_get_field_index_by_name(char *name)
{
	if (!strcmp("point", name)) return 0;
	if (!strcmp("metadata", name)) return 1;
	return -1;
	}


static GF_Node *Coordinate_Create()
{
	X_Coordinate *p;
	GF_SAFEALLOC(p, X_Coordinate);
	if(!p) return NULL;
	gf_node_setup((GF_Node *)p, TAG_X3D_Coordinate);

	/*default field values*/
	return (GF_Node *)p;
}


/*
	CoordinateDouble Node deletion
*/

static void CoordinateDouble_Del(GF_Node *node)
{
	X_CoordinateDouble *p = (X_CoordinateDouble *) node;
	gf_sg_mfvec3d_del(p->point);
	gf_node_unregister((GF_Node *) p->metadata, node);	
	gf_node_free((GF_Node *)p);
}


static u32 CoordinateDouble_get_field_count(GF_Node *node, u8 dummy)
{
	return 2;
}

static GF_Err CoordinateDouble_get_field(GF_Node *node, GF_FieldInfo *info)
{
	switch (info->fieldIndex) {
	case 0:
		info->name = "point";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_MFVEC3D;
		info->far_ptr = & ((X_CoordinateDouble *) node)->point;
		return GF_OK;
	case 1:
		info->name = "metadata";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFNODE;
		info->NDTtype = NDT_SFMetadataNode;
		info->far_ptr = & ((X_CoordinateDouble *)node)->metadata;
		return GF_OK;
	default:
		return GF_BAD_PARAM;
	}
}


static s32 CoordinateDouble_get_field_index_by_name(char *name)
{
	if (!strcmp("point", name)) return 0;
	if (!strcmp("metadata", name)) return 1;
	return -1;
	}


static GF_Node *CoordinateDouble_Create()
{
	X_CoordinateDouble *p;
	GF_SAFEALLOC(p, X_CoordinateDouble);
	if(!p) return NULL;
	gf_node_setup((GF_Node *)p, TAG_X3D_CoordinateDouble);

	/*default field values*/
	return (GF_Node *)p;
}


/*
	Coordinate2D Node deletion
*/

static void Coordinate2D_Del(GF_Node *node)
{
	X_Coordinate2D *p = (X_Coordinate2D *) node;
	gf_sg_mfvec2f_del(p->point);
	gf_node_unregister((GF_Node *) p->metadata, node);	
	gf_node_free((GF_Node *)p);
}


static u32 Coordinate2D_get_field_count(GF_Node *node, u8 dummy)
{
	return 2;
}

static GF_Err Coordinate2D_get_field(GF_Node *node, GF_FieldInfo *info)
{
	switch (info->fieldIndex) {
	case 0:
		info->name = "point";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_MFVEC2F;
		info->far_ptr = & ((X_Coordinate2D *) node)->point;
		return GF_OK;
	case 1:
		info->name = "metadata";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFNODE;
		info->NDTtype = NDT_SFMetadataNode;
		info->far_ptr = & ((X_Coordinate2D *)node)->metadata;
		return GF_OK;
	default:
		return GF_BAD_PARAM;
	}
}


static s32 Coordinate2D_get_field_index_by_name(char *name)
{
	if (!strcmp("point", name)) return 0;
	if (!strcmp("metadata", name)) return 1;
	return -1;
	}


static GF_Node *Coordinate2D_Create()
{
	X_Coordinate2D *p;
	GF_SAFEALLOC(p, X_Coordinate2D);
	if(!p) return NULL;
	gf_node_setup((GF_Node *)p, TAG_X3D_Coordinate2D);

	/*default field values*/
	return (GF_Node *)p;
}


/*
	CoordinateInterpolator Node deletion
*/

static void CoordinateInterpolator_Del(GF_Node *node)
{
	X_CoordinateInterpolator *p = (X_CoordinateInterpolator *) node;
	gf_sg_mffloat_del(p->key);
	gf_sg_mfvec3f_del(p->keyValue);
	gf_sg_mfvec3f_del(p->value_changed);
	gf_node_unregister((GF_Node *) p->metadata, node);	
	gf_node_free((GF_Node *)p);
}


static u32 CoordinateInterpolator_get_field_count(GF_Node *node, u8 dummy)
{
	return 5;
}

static GF_Err CoordinateInterpolator_get_field(GF_Node *node, GF_FieldInfo *info)
{
	switch (info->fieldIndex) {
	case 0:
		info->name = "set_fraction";
		info->eventType = GF_SG_EVENT_IN;
		info->on_event_in = ((X_CoordinateInterpolator *)node)->on_set_fraction;
		info->fieldType = GF_SG_VRML_SFFLOAT;
		info->far_ptr = & ((X_CoordinateInterpolator *) node)->set_fraction;
		return GF_OK;
	case 1:
		info->name = "key";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_MFFLOAT;
		info->far_ptr = & ((X_CoordinateInterpolator *) node)->key;
		return GF_OK;
	case 2:
		info->name = "keyValue";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_MFVEC3F;
		info->far_ptr = & ((X_CoordinateInterpolator *) node)->keyValue;
		return GF_OK;
	case 3:
		info->name = "value_changed";
		info->eventType = GF_SG_EVENT_OUT;
		info->fieldType = GF_SG_VRML_MFVEC3F;
		info->far_ptr = & ((X_CoordinateInterpolator *) node)->value_changed;
		return GF_OK;
	case 4:
		info->name = "metadata";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFNODE;
		info->NDTtype = NDT_SFMetadataNode;
		info->far_ptr = & ((X_CoordinateInterpolator *)node)->metadata;
		return GF_OK;
	default:
		return GF_BAD_PARAM;
	}
}


static s32 CoordinateInterpolator_get_field_index_by_name(char *name)
{
	if (!strcmp("set_fraction", name)) return 0;
	if (!strcmp("key", name)) return 1;
	if (!strcmp("keyValue", name)) return 2;
	if (!strcmp("value_changed", name)) return 3;
	if (!strcmp("metadata", name)) return 4;
	return -1;
	}


static GF_Node *CoordinateInterpolator_Create()
{
	X_CoordinateInterpolator *p;
	GF_SAFEALLOC(p, X_CoordinateInterpolator);
	if(!p) return NULL;
	gf_node_setup((GF_Node *)p, TAG_X3D_CoordinateInterpolator);

	/*default field values*/
	return (GF_Node *)p;
}


/*
	CoordinateInterpolator2D Node deletion
*/

static void CoordinateInterpolator2D_Del(GF_Node *node)
{
	X_CoordinateInterpolator2D *p = (X_CoordinateInterpolator2D *) node;
	gf_sg_mffloat_del(p->key);
	gf_sg_mfvec2f_del(p->keyValue);
	gf_sg_mfvec2f_del(p->value_changed);
	gf_node_unregister((GF_Node *) p->metadata, node);	
	gf_node_free((GF_Node *)p);
}


static u32 CoordinateInterpolator2D_get_field_count(GF_Node *node, u8 dummy)
{
	return 5;
}

static GF_Err CoordinateInterpolator2D_get_field(GF_Node *node, GF_FieldInfo *info)
{
	switch (info->fieldIndex) {
	case 0:
		info->name = "set_fraction";
		info->eventType = GF_SG_EVENT_IN;
		info->on_event_in = ((X_CoordinateInterpolator2D *)node)->on_set_fraction;
		info->fieldType = GF_SG_VRML_SFFLOAT;
		info->far_ptr = & ((X_CoordinateInterpolator2D *) node)->set_fraction;
		return GF_OK;
	case 1:
		info->name = "key";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_MFFLOAT;
		info->far_ptr = & ((X_CoordinateInterpolator2D *) node)->key;
		return GF_OK;
	case 2:
		info->name = "keyValue";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_MFVEC2F;
		info->far_ptr = & ((X_CoordinateInterpolator2D *) node)->keyValue;
		return GF_OK;
	case 3:
		info->name = "value_changed";
		info->eventType = GF_SG_EVENT_OUT;
		info->fieldType = GF_SG_VRML_MFVEC2F;
		info->far_ptr = & ((X_CoordinateInterpolator2D *) node)->value_changed;
		return GF_OK;
	case 4:
		info->name = "metadata";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFNODE;
		info->NDTtype = NDT_SFMetadataNode;
		info->far_ptr = & ((X_CoordinateInterpolator2D *)node)->metadata;
		return GF_OK;
	default:
		return GF_BAD_PARAM;
	}
}


static s32 CoordinateInterpolator2D_get_field_index_by_name(char *name)
{
	if (!strcmp("set_fraction", name)) return 0;
	if (!strcmp("key", name)) return 1;
	if (!strcmp("keyValue", name)) return 2;
	if (!strcmp("value_changed", name)) return 3;
	if (!strcmp("metadata", name)) return 4;
	return -1;
	}


static GF_Node *CoordinateInterpolator2D_Create()
{
	X_CoordinateInterpolator2D *p;
	GF_SAFEALLOC(p, X_CoordinateInterpolator2D);
	if(!p) return NULL;
	gf_node_setup((GF_Node *)p, TAG_X3D_CoordinateInterpolator2D);

	/*default field values*/
	return (GF_Node *)p;
}


/*
	Cylinder Node deletion
*/

static void Cylinder_Del(GF_Node *node)
{
	X_Cylinder *p = (X_Cylinder *) node;
	gf_node_unregister((GF_Node *) p->metadata, node);	
	gf_node_free((GF_Node *)p);
}


static u32 Cylinder_get_field_count(GF_Node *node, u8 dummy)
{
	return 6;
}

static GF_Err Cylinder_get_field(GF_Node *node, GF_FieldInfo *info)
{
	switch (info->fieldIndex) {
	case 0:
		info->name = "bottom";
		info->eventType = GF_SG_EVENT_FIELD;
		info->fieldType = GF_SG_VRML_SFBOOL;
		info->far_ptr = & ((X_Cylinder *) node)->bottom;
		return GF_OK;
	case 1:
		info->name = "height";
		info->eventType = GF_SG_EVENT_FIELD;
		info->fieldType = GF_SG_VRML_SFFLOAT;
		info->far_ptr = & ((X_Cylinder *) node)->height;
		return GF_OK;
	case 2:
		info->name = "radius";
		info->eventType = GF_SG_EVENT_FIELD;
		info->fieldType = GF_SG_VRML_SFFLOAT;
		info->far_ptr = & ((X_Cylinder *) node)->radius;
		return GF_OK;
	case 3:
		info->name = "side";
		info->eventType = GF_SG_EVENT_FIELD;
		info->fieldType = GF_SG_VRML_SFBOOL;
		info->far_ptr = & ((X_Cylinder *) node)->side;
		return GF_OK;
	case 4:
		info->name = "top";
		info->eventType = GF_SG_EVENT_FIELD;
		info->fieldType = GF_SG_VRML_SFBOOL;
		info->far_ptr = & ((X_Cylinder *) node)->top;
		return GF_OK;
	case 5:
		info->name = "metadata";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFNODE;
		info->NDTtype = NDT_SFMetadataNode;
		info->far_ptr = & ((X_Cylinder *)node)->metadata;
		return GF_OK;
	default:
		return GF_BAD_PARAM;
	}
}


static s32 Cylinder_get_field_index_by_name(char *name)
{
	if (!strcmp("bottom", name)) return 0;
	if (!strcmp("height", name)) return 1;
	if (!strcmp("radius", name)) return 2;
	if (!strcmp("side", name)) return 3;
	if (!strcmp("top", name)) return 4;
	if (!strcmp("metadata", name)) return 5;
	return -1;
	}


static GF_Node *Cylinder_Create()
{
	X_Cylinder *p;
	GF_SAFEALLOC(p, X_Cylinder);
	if(!p) return NULL;
	gf_node_setup((GF_Node *)p, TAG_X3D_Cylinder);

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
	X_CylinderSensor *p = (X_CylinderSensor *) node;
	gf_node_unregister((GF_Node *) p->metadata, node);	
	gf_sg_sfstring_del(p->description);
	gf_node_free((GF_Node *)p);
}


static u32 CylinderSensor_get_field_count(GF_Node *node, u8 dummy)
{
	return 12;
}

static GF_Err CylinderSensor_get_field(GF_Node *node, GF_FieldInfo *info)
{
	switch (info->fieldIndex) {
	case 0:
		info->name = "autoOffset";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFBOOL;
		info->far_ptr = & ((X_CylinderSensor *) node)->autoOffset;
		return GF_OK;
	case 1:
		info->name = "diskAngle";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFFLOAT;
		info->far_ptr = & ((X_CylinderSensor *) node)->diskAngle;
		return GF_OK;
	case 2:
		info->name = "enabled";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFBOOL;
		info->far_ptr = & ((X_CylinderSensor *) node)->enabled;
		return GF_OK;
	case 3:
		info->name = "maxAngle";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFFLOAT;
		info->far_ptr = & ((X_CylinderSensor *) node)->maxAngle;
		return GF_OK;
	case 4:
		info->name = "minAngle";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFFLOAT;
		info->far_ptr = & ((X_CylinderSensor *) node)->minAngle;
		return GF_OK;
	case 5:
		info->name = "offset";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFFLOAT;
		info->far_ptr = & ((X_CylinderSensor *) node)->offset;
		return GF_OK;
	case 6:
		info->name = "isActive";
		info->eventType = GF_SG_EVENT_OUT;
		info->fieldType = GF_SG_VRML_SFBOOL;
		info->far_ptr = & ((X_CylinderSensor *) node)->isActive;
		return GF_OK;
	case 7:
		info->name = "rotation_changed";
		info->eventType = GF_SG_EVENT_OUT;
		info->fieldType = GF_SG_VRML_SFROTATION;
		info->far_ptr = & ((X_CylinderSensor *) node)->rotation_changed;
		return GF_OK;
	case 8:
		info->name = "trackPoint_changed";
		info->eventType = GF_SG_EVENT_OUT;
		info->fieldType = GF_SG_VRML_SFVEC3F;
		info->far_ptr = & ((X_CylinderSensor *) node)->trackPoint_changed;
		return GF_OK;
	case 9:
		info->name = "metadata";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFNODE;
		info->NDTtype = NDT_SFMetadataNode;
		info->far_ptr = & ((X_CylinderSensor *)node)->metadata;
		return GF_OK;
	case 10:
		info->name = "description";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFSTRING;
		info->far_ptr = & ((X_CylinderSensor *) node)->description;
		return GF_OK;
	case 11:
		info->name = "isOver";
		info->eventType = GF_SG_EVENT_OUT;
		info->fieldType = GF_SG_VRML_SFBOOL;
		info->far_ptr = & ((X_CylinderSensor *) node)->isOver;
		return GF_OK;
	default:
		return GF_BAD_PARAM;
	}
}


static s32 CylinderSensor_get_field_index_by_name(char *name)
{
	if (!strcmp("autoOffset", name)) return 0;
	if (!strcmp("diskAngle", name)) return 1;
	if (!strcmp("enabled", name)) return 2;
	if (!strcmp("maxAngle", name)) return 3;
	if (!strcmp("minAngle", name)) return 4;
	if (!strcmp("offset", name)) return 5;
	if (!strcmp("isActive", name)) return 6;
	if (!strcmp("rotation_changed", name)) return 7;
	if (!strcmp("trackPoint_changed", name)) return 8;
	if (!strcmp("metadata", name)) return 9;
	if (!strcmp("description", name)) return 10;
	if (!strcmp("isOver", name)) return 11;
	return -1;
	}


static GF_Node *CylinderSensor_Create()
{
	X_CylinderSensor *p;
	GF_SAFEALLOC(p, X_CylinderSensor);
	if(!p) return NULL;
	gf_node_setup((GF_Node *)p, TAG_X3D_CylinderSensor);

	/*default field values*/
	p->autoOffset = 1;
	p->diskAngle = FLT2FIX(0.2617);
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
	X_DirectionalLight *p = (X_DirectionalLight *) node;
	gf_node_unregister((GF_Node *) p->metadata, node);	
	gf_node_free((GF_Node *)p);
}


static u32 DirectionalLight_get_field_count(GF_Node *node, u8 dummy)
{
	return 6;
}

static GF_Err DirectionalLight_get_field(GF_Node *node, GF_FieldInfo *info)
{
	switch (info->fieldIndex) {
	case 0:
		info->name = "ambientIntensity";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFFLOAT;
		info->far_ptr = & ((X_DirectionalLight *) node)->ambientIntensity;
		return GF_OK;
	case 1:
		info->name = "color";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFCOLOR;
		info->far_ptr = & ((X_DirectionalLight *) node)->color;
		return GF_OK;
	case 2:
		info->name = "direction";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFVEC3F;
		info->far_ptr = & ((X_DirectionalLight *) node)->direction;
		return GF_OK;
	case 3:
		info->name = "intensity";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFFLOAT;
		info->far_ptr = & ((X_DirectionalLight *) node)->intensity;
		return GF_OK;
	case 4:
		info->name = "on";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFBOOL;
		info->far_ptr = & ((X_DirectionalLight *) node)->on;
		return GF_OK;
	case 5:
		info->name = "metadata";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFNODE;
		info->NDTtype = NDT_SFMetadataNode;
		info->far_ptr = & ((X_DirectionalLight *)node)->metadata;
		return GF_OK;
	default:
		return GF_BAD_PARAM;
	}
}


static s32 DirectionalLight_get_field_index_by_name(char *name)
{
	if (!strcmp("ambientIntensity", name)) return 0;
	if (!strcmp("color", name)) return 1;
	if (!strcmp("direction", name)) return 2;
	if (!strcmp("intensity", name)) return 3;
	if (!strcmp("on", name)) return 4;
	if (!strcmp("metadata", name)) return 5;
	return -1;
	}


static GF_Node *DirectionalLight_Create()
{
	X_DirectionalLight *p;
	GF_SAFEALLOC(p, X_DirectionalLight);
	if(!p) return NULL;
	gf_node_setup((GF_Node *)p, TAG_X3D_DirectionalLight);

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
	Disk2D Node deletion
*/

static void Disk2D_Del(GF_Node *node)
{
	X_Disk2D *p = (X_Disk2D *) node;
	gf_node_unregister((GF_Node *) p->metadata, node);	
	gf_node_free((GF_Node *)p);
}


static u32 Disk2D_get_field_count(GF_Node *node, u8 dummy)
{
	return 3;
}

static GF_Err Disk2D_get_field(GF_Node *node, GF_FieldInfo *info)
{
	switch (info->fieldIndex) {
	case 0:
		info->name = "innerRadius";
		info->eventType = GF_SG_EVENT_FIELD;
		info->fieldType = GF_SG_VRML_SFFLOAT;
		info->far_ptr = & ((X_Disk2D *) node)->innerRadius;
		return GF_OK;
	case 1:
		info->name = "outerRadius";
		info->eventType = GF_SG_EVENT_FIELD;
		info->fieldType = GF_SG_VRML_SFFLOAT;
		info->far_ptr = & ((X_Disk2D *) node)->outerRadius;
		return GF_OK;
	case 2:
		info->name = "metadata";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFNODE;
		info->NDTtype = NDT_SFMetadataNode;
		info->far_ptr = & ((X_Disk2D *)node)->metadata;
		return GF_OK;
	default:
		return GF_BAD_PARAM;
	}
}


static s32 Disk2D_get_field_index_by_name(char *name)
{
	if (!strcmp("innerRadius", name)) return 0;
	if (!strcmp("outerRadius", name)) return 1;
	if (!strcmp("metadata", name)) return 2;
	return -1;
	}


static GF_Node *Disk2D_Create()
{
	X_Disk2D *p;
	GF_SAFEALLOC(p, X_Disk2D);
	if(!p) return NULL;
	gf_node_setup((GF_Node *)p, TAG_X3D_Disk2D);

	/*default field values*/
	p->innerRadius = FLT2FIX(0);
	p->outerRadius = FLT2FIX(1);
	return (GF_Node *)p;
}


/*
	ElevationGrid Node deletion
*/

static void ElevationGrid_Del(GF_Node *node)
{
	X_ElevationGrid *p = (X_ElevationGrid *) node;
	gf_sg_mffloat_del(p->set_height);
	gf_node_unregister((GF_Node *) p->color, node);	
	gf_node_unregister((GF_Node *) p->normal, node);	
	gf_node_unregister((GF_Node *) p->texCoord, node);	
	gf_sg_mffloat_del(p->height);
	gf_node_unregister((GF_Node *) p->metadata, node);	
	gf_node_free((GF_Node *)p);
}


static u32 ElevationGrid_get_field_count(GF_Node *node, u8 dummy)
{
	return 15;
}

static GF_Err ElevationGrid_get_field(GF_Node *node, GF_FieldInfo *info)
{
	switch (info->fieldIndex) {
	case 0:
		info->name = "set_height";
		info->eventType = GF_SG_EVENT_IN;
		info->on_event_in = ((X_ElevationGrid *)node)->on_set_height;
		info->fieldType = GF_SG_VRML_MFFLOAT;
		info->far_ptr = & ((X_ElevationGrid *) node)->set_height;
		return GF_OK;
	case 1:
		info->name = "color";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFNODE;
		info->NDTtype = NDT_SFColorNode;
		info->far_ptr = & ((X_ElevationGrid *)node)->color;
		return GF_OK;
	case 2:
		info->name = "normal";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFNODE;
		info->NDTtype = NDT_SFNormalNode;
		info->far_ptr = & ((X_ElevationGrid *)node)->normal;
		return GF_OK;
	case 3:
		info->name = "texCoord";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFNODE;
		info->NDTtype = NDT_SFTextureCoordinateNode;
		info->far_ptr = & ((X_ElevationGrid *)node)->texCoord;
		return GF_OK;
	case 4:
		info->name = "height";
		info->eventType = GF_SG_EVENT_FIELD;
		info->fieldType = GF_SG_VRML_MFFLOAT;
		info->far_ptr = & ((X_ElevationGrid *) node)->height;
		return GF_OK;
	case 5:
		info->name = "ccw";
		info->eventType = GF_SG_EVENT_FIELD;
		info->fieldType = GF_SG_VRML_SFBOOL;
		info->far_ptr = & ((X_ElevationGrid *) node)->ccw;
		return GF_OK;
	case 6:
		info->name = "colorPerVertex";
		info->eventType = GF_SG_EVENT_FIELD;
		info->fieldType = GF_SG_VRML_SFBOOL;
		info->far_ptr = & ((X_ElevationGrid *) node)->colorPerVertex;
		return GF_OK;
	case 7:
		info->name = "creaseAngle";
		info->eventType = GF_SG_EVENT_FIELD;
		info->fieldType = GF_SG_VRML_SFFLOAT;
		info->far_ptr = & ((X_ElevationGrid *) node)->creaseAngle;
		return GF_OK;
	case 8:
		info->name = "normalPerVertex";
		info->eventType = GF_SG_EVENT_FIELD;
		info->fieldType = GF_SG_VRML_SFBOOL;
		info->far_ptr = & ((X_ElevationGrid *) node)->normalPerVertex;
		return GF_OK;
	case 9:
		info->name = "solid";
		info->eventType = GF_SG_EVENT_FIELD;
		info->fieldType = GF_SG_VRML_SFBOOL;
		info->far_ptr = & ((X_ElevationGrid *) node)->solid;
		return GF_OK;
	case 10:
		info->name = "xDimension";
		info->eventType = GF_SG_EVENT_FIELD;
		info->fieldType = GF_SG_VRML_SFINT32;
		info->far_ptr = & ((X_ElevationGrid *) node)->xDimension;
		return GF_OK;
	case 11:
		info->name = "xSpacing";
		info->eventType = GF_SG_EVENT_FIELD;
		info->fieldType = GF_SG_VRML_SFFLOAT;
		info->far_ptr = & ((X_ElevationGrid *) node)->xSpacing;
		return GF_OK;
	case 12:
		info->name = "zDimension";
		info->eventType = GF_SG_EVENT_FIELD;
		info->fieldType = GF_SG_VRML_SFINT32;
		info->far_ptr = & ((X_ElevationGrid *) node)->zDimension;
		return GF_OK;
	case 13:
		info->name = "zSpacing";
		info->eventType = GF_SG_EVENT_FIELD;
		info->fieldType = GF_SG_VRML_SFFLOAT;
		info->far_ptr = & ((X_ElevationGrid *) node)->zSpacing;
		return GF_OK;
	case 14:
		info->name = "metadata";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFNODE;
		info->NDTtype = NDT_SFMetadataNode;
		info->far_ptr = & ((X_ElevationGrid *)node)->metadata;
		return GF_OK;
	default:
		return GF_BAD_PARAM;
	}
}


static s32 ElevationGrid_get_field_index_by_name(char *name)
{
	if (!strcmp("set_height", name)) return 0;
	if (!strcmp("color", name)) return 1;
	if (!strcmp("normal", name)) return 2;
	if (!strcmp("texCoord", name)) return 3;
	if (!strcmp("height", name)) return 4;
	if (!strcmp("ccw", name)) return 5;
	if (!strcmp("colorPerVertex", name)) return 6;
	if (!strcmp("creaseAngle", name)) return 7;
	if (!strcmp("normalPerVertex", name)) return 8;
	if (!strcmp("solid", name)) return 9;
	if (!strcmp("xDimension", name)) return 10;
	if (!strcmp("xSpacing", name)) return 11;
	if (!strcmp("zDimension", name)) return 12;
	if (!strcmp("zSpacing", name)) return 13;
	if (!strcmp("metadata", name)) return 14;
	return -1;
	}


static GF_Node *ElevationGrid_Create()
{
	X_ElevationGrid *p;
	GF_SAFEALLOC(p, X_ElevationGrid);
	if(!p) return NULL;
	gf_node_setup((GF_Node *)p, TAG_X3D_ElevationGrid);

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
	X_Extrusion *p = (X_Extrusion *) node;
	gf_sg_mfvec2f_del(p->set_crossSection);
	gf_sg_mfrotation_del(p->set_orientation);
	gf_sg_mfvec2f_del(p->set_scale);
	gf_sg_mfvec3f_del(p->set_spine);
	gf_sg_mfvec2f_del(p->crossSection);
	gf_sg_mfrotation_del(p->orientation);
	gf_sg_mfvec2f_del(p->scale);
	gf_sg_mfvec3f_del(p->spine);
	gf_node_unregister((GF_Node *) p->metadata, node);	
	gf_node_free((GF_Node *)p);
}


static u32 Extrusion_get_field_count(GF_Node *node, u8 dummy)
{
	return 15;
}

static GF_Err Extrusion_get_field(GF_Node *node, GF_FieldInfo *info)
{
	switch (info->fieldIndex) {
	case 0:
		info->name = "set_crossSection";
		info->eventType = GF_SG_EVENT_IN;
		info->on_event_in = ((X_Extrusion *)node)->on_set_crossSection;
		info->fieldType = GF_SG_VRML_MFVEC2F;
		info->far_ptr = & ((X_Extrusion *) node)->set_crossSection;
		return GF_OK;
	case 1:
		info->name = "set_orientation";
		info->eventType = GF_SG_EVENT_IN;
		info->on_event_in = ((X_Extrusion *)node)->on_set_orientation;
		info->fieldType = GF_SG_VRML_MFROTATION;
		info->far_ptr = & ((X_Extrusion *) node)->set_orientation;
		return GF_OK;
	case 2:
		info->name = "set_scale";
		info->eventType = GF_SG_EVENT_IN;
		info->on_event_in = ((X_Extrusion *)node)->on_set_scale;
		info->fieldType = GF_SG_VRML_MFVEC2F;
		info->far_ptr = & ((X_Extrusion *) node)->set_scale;
		return GF_OK;
	case 3:
		info->name = "set_spine";
		info->eventType = GF_SG_EVENT_IN;
		info->on_event_in = ((X_Extrusion *)node)->on_set_spine;
		info->fieldType = GF_SG_VRML_MFVEC3F;
		info->far_ptr = & ((X_Extrusion *) node)->set_spine;
		return GF_OK;
	case 4:
		info->name = "beginCap";
		info->eventType = GF_SG_EVENT_FIELD;
		info->fieldType = GF_SG_VRML_SFBOOL;
		info->far_ptr = & ((X_Extrusion *) node)->beginCap;
		return GF_OK;
	case 5:
		info->name = "ccw";
		info->eventType = GF_SG_EVENT_FIELD;
		info->fieldType = GF_SG_VRML_SFBOOL;
		info->far_ptr = & ((X_Extrusion *) node)->ccw;
		return GF_OK;
	case 6:
		info->name = "convex";
		info->eventType = GF_SG_EVENT_FIELD;
		info->fieldType = GF_SG_VRML_SFBOOL;
		info->far_ptr = & ((X_Extrusion *) node)->convex;
		return GF_OK;
	case 7:
		info->name = "creaseAngle";
		info->eventType = GF_SG_EVENT_FIELD;
		info->fieldType = GF_SG_VRML_SFFLOAT;
		info->far_ptr = & ((X_Extrusion *) node)->creaseAngle;
		return GF_OK;
	case 8:
		info->name = "crossSection";
		info->eventType = GF_SG_EVENT_FIELD;
		info->fieldType = GF_SG_VRML_MFVEC2F;
		info->far_ptr = & ((X_Extrusion *) node)->crossSection;
		return GF_OK;
	case 9:
		info->name = "endCap";
		info->eventType = GF_SG_EVENT_FIELD;
		info->fieldType = GF_SG_VRML_SFBOOL;
		info->far_ptr = & ((X_Extrusion *) node)->endCap;
		return GF_OK;
	case 10:
		info->name = "orientation";
		info->eventType = GF_SG_EVENT_FIELD;
		info->fieldType = GF_SG_VRML_MFROTATION;
		info->far_ptr = & ((X_Extrusion *) node)->orientation;
		return GF_OK;
	case 11:
		info->name = "scale";
		info->eventType = GF_SG_EVENT_FIELD;
		info->fieldType = GF_SG_VRML_MFVEC2F;
		info->far_ptr = & ((X_Extrusion *) node)->scale;
		return GF_OK;
	case 12:
		info->name = "solid";
		info->eventType = GF_SG_EVENT_FIELD;
		info->fieldType = GF_SG_VRML_SFBOOL;
		info->far_ptr = & ((X_Extrusion *) node)->solid;
		return GF_OK;
	case 13:
		info->name = "spine";
		info->eventType = GF_SG_EVENT_FIELD;
		info->fieldType = GF_SG_VRML_MFVEC3F;
		info->far_ptr = & ((X_Extrusion *) node)->spine;
		return GF_OK;
	case 14:
		info->name = "metadata";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFNODE;
		info->NDTtype = NDT_SFMetadataNode;
		info->far_ptr = & ((X_Extrusion *)node)->metadata;
		return GF_OK;
	default:
		return GF_BAD_PARAM;
	}
}


static s32 Extrusion_get_field_index_by_name(char *name)
{
	if (!strcmp("set_crossSection", name)) return 0;
	if (!strcmp("set_orientation", name)) return 1;
	if (!strcmp("set_scale", name)) return 2;
	if (!strcmp("set_spine", name)) return 3;
	if (!strcmp("beginCap", name)) return 4;
	if (!strcmp("ccw", name)) return 5;
	if (!strcmp("convex", name)) return 6;
	if (!strcmp("creaseAngle", name)) return 7;
	if (!strcmp("crossSection", name)) return 8;
	if (!strcmp("endCap", name)) return 9;
	if (!strcmp("orientation", name)) return 10;
	if (!strcmp("scale", name)) return 11;
	if (!strcmp("solid", name)) return 12;
	if (!strcmp("spine", name)) return 13;
	if (!strcmp("metadata", name)) return 14;
	return -1;
	}


static GF_Node *Extrusion_Create()
{
	X_Extrusion *p;
	GF_SAFEALLOC(p, X_Extrusion);
	if(!p) return NULL;
	gf_node_setup((GF_Node *)p, TAG_X3D_Extrusion);

	/*default field values*/
	p->beginCap = 1;
	p->ccw = 1;
	p->convex = 1;
	p->creaseAngle = FLT2FIX(0.0);
	p->crossSection.vals = (SFVec2f*) malloc(sizeof(SFVec2f)*5);
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
	p->orientation.vals = (GF_Vec4*)malloc(sizeof(GF_Vec4)*1);
	p->orientation.count = 1;
	p->orientation.vals[0].x = FLT2FIX(0);
	p->orientation.vals[0].y = FLT2FIX(0);
	p->orientation.vals[0].z = FLT2FIX(1);
	p->orientation.vals[0].q = FLT2FIX(0);
	p->scale.vals = (SFVec2f*) malloc(sizeof(SFVec2f)*1);
	p->scale.count = 1;
	p->scale.vals[0].x = FLT2FIX(1);
	p->scale.vals[0].y = FLT2FIX(1);
	p->solid = 1;
	p->spine.vals = (SFVec3f*)malloc(sizeof(SFVec3f)*2);
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
	FillProperties Node deletion
*/

static void FillProperties_Del(GF_Node *node)
{
	X_FillProperties *p = (X_FillProperties *) node;
	gf_node_free((GF_Node *)p);
}


static u32 FillProperties_get_field_count(GF_Node *node, u8 dummy)
{
	return 4;
}

static GF_Err FillProperties_get_field(GF_Node *node, GF_FieldInfo *info)
{
	switch (info->fieldIndex) {
	case 0:
		info->name = "filled";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFBOOL;
		info->far_ptr = & ((X_FillProperties *) node)->filled;
		return GF_OK;
	case 1:
		info->name = "hatchColor";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFCOLOR;
		info->far_ptr = & ((X_FillProperties *) node)->hatchColor;
		return GF_OK;
	case 2:
		info->name = "hatched";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFBOOL;
		info->far_ptr = & ((X_FillProperties *) node)->hatched;
		return GF_OK;
	case 3:
		info->name = "hatchStyle";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFINT32;
		info->far_ptr = & ((X_FillProperties *) node)->hatchStyle;
		return GF_OK;
	default:
		return GF_BAD_PARAM;
	}
}


static s32 FillProperties_get_field_index_by_name(char *name)
{
	if (!strcmp("filled", name)) return 0;
	if (!strcmp("hatchColor", name)) return 1;
	if (!strcmp("hatched", name)) return 2;
	if (!strcmp("hatchStyle", name)) return 3;
	return -1;
	}


static GF_Node *FillProperties_Create()
{
	X_FillProperties *p;
	GF_SAFEALLOC(p, X_FillProperties);
	if(!p) return NULL;
	gf_node_setup((GF_Node *)p, TAG_X3D_FillProperties);

	/*default field values*/
	p->filled = 1;
	p->hatchColor.red = FLT2FIX(1);
	p->hatchColor.green = FLT2FIX(1);
	p->hatchColor.blue = FLT2FIX(1);
	p->hatched = 1;
	p->hatchStyle = 1;
	return (GF_Node *)p;
}


/*
	Fog Node deletion
*/

static void Fog_Del(GF_Node *node)
{
	X_Fog *p = (X_Fog *) node;
	gf_sg_sfstring_del(p->fogType);
	gf_node_unregister((GF_Node *) p->metadata, node);	
	gf_node_free((GF_Node *)p);
}


static u32 Fog_get_field_count(GF_Node *node, u8 dummy)
{
	return 7;
}

static GF_Err Fog_get_field(GF_Node *node, GF_FieldInfo *info)
{
	switch (info->fieldIndex) {
	case 0:
		info->name = "color";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFCOLOR;
		info->far_ptr = & ((X_Fog *) node)->color;
		return GF_OK;
	case 1:
		info->name = "fogType";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFSTRING;
		info->far_ptr = & ((X_Fog *) node)->fogType;
		return GF_OK;
	case 2:
		info->name = "visibilityRange";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFFLOAT;
		info->far_ptr = & ((X_Fog *) node)->visibilityRange;
		return GF_OK;
	case 3:
		info->name = "set_bind";
		info->eventType = GF_SG_EVENT_IN;
		info->on_event_in = ((X_Fog *)node)->on_set_bind;
		info->fieldType = GF_SG_VRML_SFBOOL;
		info->far_ptr = & ((X_Fog *) node)->set_bind;
		return GF_OK;
	case 4:
		info->name = "isBound";
		info->eventType = GF_SG_EVENT_OUT;
		info->fieldType = GF_SG_VRML_SFBOOL;
		info->far_ptr = & ((X_Fog *) node)->isBound;
		return GF_OK;
	case 5:
		info->name = "metadata";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFNODE;
		info->NDTtype = NDT_SFMetadataNode;
		info->far_ptr = & ((X_Fog *)node)->metadata;
		return GF_OK;
	case 6:
		info->name = "bindTime";
		info->eventType = GF_SG_EVENT_OUT;
		info->fieldType = GF_SG_VRML_SFTIME;
		info->far_ptr = & ((X_Fog *) node)->bindTime;
		return GF_OK;
	default:
		return GF_BAD_PARAM;
	}
}


static s32 Fog_get_field_index_by_name(char *name)
{
	if (!strcmp("color", name)) return 0;
	if (!strcmp("fogType", name)) return 1;
	if (!strcmp("visibilityRange", name)) return 2;
	if (!strcmp("set_bind", name)) return 3;
	if (!strcmp("isBound", name)) return 4;
	if (!strcmp("metadata", name)) return 5;
	if (!strcmp("bindTime", name)) return 6;
	return -1;
	}


static GF_Node *Fog_Create()
{
	X_Fog *p;
	GF_SAFEALLOC(p, X_Fog);
	if(!p) return NULL;
	gf_node_setup((GF_Node *)p, TAG_X3D_Fog);

	/*default field values*/
	p->color.red = FLT2FIX(1);
	p->color.green = FLT2FIX(1);
	p->color.blue = FLT2FIX(1);
	p->fogType.buffer = (char*) malloc(sizeof(char) * 7);
	strcpy(p->fogType.buffer, "LINEAR");
	p->visibilityRange = FLT2FIX(0);
	return (GF_Node *)p;
}


/*
	FontStyle Node deletion
*/

static void FontStyle_Del(GF_Node *node)
{
	X_FontStyle *p = (X_FontStyle *) node;
	gf_sg_mfstring_del(p->family);
	gf_sg_mfstring_del(p->justify);
	gf_sg_sfstring_del(p->language);
	gf_sg_sfstring_del(p->style);
	gf_node_unregister((GF_Node *) p->metadata, node);	
	gf_node_free((GF_Node *)p);
}


static u32 FontStyle_get_field_count(GF_Node *node, u8 dummy)
{
	return 10;
}

static GF_Err FontStyle_get_field(GF_Node *node, GF_FieldInfo *info)
{
	switch (info->fieldIndex) {
	case 0:
		info->name = "family";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_MFSTRING;
		info->far_ptr = & ((X_FontStyle *) node)->family;
		return GF_OK;
	case 1:
		info->name = "horizontal";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFBOOL;
		info->far_ptr = & ((X_FontStyle *) node)->horizontal;
		return GF_OK;
	case 2:
		info->name = "justify";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_MFSTRING;
		info->far_ptr = & ((X_FontStyle *) node)->justify;
		return GF_OK;
	case 3:
		info->name = "language";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFSTRING;
		info->far_ptr = & ((X_FontStyle *) node)->language;
		return GF_OK;
	case 4:
		info->name = "leftToRight";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFBOOL;
		info->far_ptr = & ((X_FontStyle *) node)->leftToRight;
		return GF_OK;
	case 5:
		info->name = "size";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFFLOAT;
		info->far_ptr = & ((X_FontStyle *) node)->size;
		return GF_OK;
	case 6:
		info->name = "spacing";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFFLOAT;
		info->far_ptr = & ((X_FontStyle *) node)->spacing;
		return GF_OK;
	case 7:
		info->name = "style";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFSTRING;
		info->far_ptr = & ((X_FontStyle *) node)->style;
		return GF_OK;
	case 8:
		info->name = "topToBottom";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFBOOL;
		info->far_ptr = & ((X_FontStyle *) node)->topToBottom;
		return GF_OK;
	case 9:
		info->name = "metadata";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFNODE;
		info->NDTtype = NDT_SFMetadataNode;
		info->far_ptr = & ((X_FontStyle *)node)->metadata;
		return GF_OK;
	default:
		return GF_BAD_PARAM;
	}
}


static s32 FontStyle_get_field_index_by_name(char *name)
{
	if (!strcmp("family", name)) return 0;
	if (!strcmp("horizontal", name)) return 1;
	if (!strcmp("justify", name)) return 2;
	if (!strcmp("language", name)) return 3;
	if (!strcmp("leftToRight", name)) return 4;
	if (!strcmp("size", name)) return 5;
	if (!strcmp("spacing", name)) return 6;
	if (!strcmp("style", name)) return 7;
	if (!strcmp("topToBottom", name)) return 8;
	if (!strcmp("metadata", name)) return 9;
	return -1;
	}


static GF_Node *FontStyle_Create()
{
	X_FontStyle *p;
	GF_SAFEALLOC(p, X_FontStyle);
	if(!p) return NULL;
	gf_node_setup((GF_Node *)p, TAG_X3D_FontStyle);

	/*default field values*/
	p->family.vals = (char**)malloc(sizeof(SFString)*1);
	p->family.count = 1;
	p->family.vals[0] = (char*)malloc(sizeof(char) * 6);
	strcpy(p->family.vals[0], "SERIF");
	p->horizontal = 1;
	p->justify.vals = (char**)malloc(sizeof(SFString)*1);
	p->justify.count = 1;
	p->justify.vals[0] = (char*)malloc(sizeof(char) * 6);
	strcpy(p->justify.vals[0], "BEGIN");
	p->leftToRight = 1;
	p->size = FLT2FIX(1.0);
	p->spacing = FLT2FIX(1.0);
	p->style.buffer = (char*) malloc(sizeof(char) * 6);
	strcpy(p->style.buffer, "PLAIN");
	p->topToBottom = 1;
	return (GF_Node *)p;
}


/*
	Group Node deletion
*/

static void Group_Del(GF_Node *node)
{
	X_Group *p = (X_Group *) node;
	gf_node_unregister((GF_Node *) p->metadata, node);	
	gf_sg_vrml_parent_destroy(node);	
	gf_node_free((GF_Node *)p);
}


static u32 Group_get_field_count(GF_Node *node, u8 dummy)
{
	return 4;
}

static GF_Err Group_get_field(GF_Node *node, GF_FieldInfo *info)
{
	switch (info->fieldIndex) {
	case 0:
		info->name = "addChildren";
		info->eventType = GF_SG_EVENT_IN;
		info->on_event_in = ((X_Group *)node)->on_addChildren;
		info->fieldType = GF_SG_VRML_MFNODE;
		info->NDTtype = NDT_SF3DNode;
		info->far_ptr = & ((X_Group *)node)->addChildren;
		return GF_OK;
	case 1:
		info->name = "removeChildren";
		info->eventType = GF_SG_EVENT_IN;
		info->on_event_in = ((X_Group *)node)->on_removeChildren;
		info->fieldType = GF_SG_VRML_MFNODE;
		info->NDTtype = NDT_SF3DNode;
		info->far_ptr = & ((X_Group *)node)->removeChildren;
		return GF_OK;
	case 2:
		info->name = "children";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_MFNODE;
		info->NDTtype = NDT_SF3DNode;
		info->far_ptr = & ((X_Group *)node)->children;
		return GF_OK;
	case 3:
		info->name = "metadata";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFNODE;
		info->NDTtype = NDT_SFMetadataNode;
		info->far_ptr = & ((X_Group *)node)->metadata;
		return GF_OK;
	default:
		return GF_BAD_PARAM;
	}
}


static s32 Group_get_field_index_by_name(char *name)
{
	if (!strcmp("addChildren", name)) return 0;
	if (!strcmp("removeChildren", name)) return 1;
	if (!strcmp("children", name)) return 2;
	if (!strcmp("metadata", name)) return 3;
	return -1;
	}


static GF_Node *Group_Create()
{
	X_Group *p;
	GF_SAFEALLOC(p, X_Group);
	if(!p) return NULL;
	gf_node_setup((GF_Node *)p, TAG_X3D_Group);
	gf_sg_vrml_parent_setup((GF_Node *) p);

	/*default field values*/
	return (GF_Node *)p;
}


/*
	ImageTexture Node deletion
*/

static void ImageTexture_Del(GF_Node *node)
{
	X_ImageTexture *p = (X_ImageTexture *) node;
	gf_sg_mfurl_del(p->url);
	gf_node_unregister((GF_Node *) p->metadata, node);	
	gf_node_free((GF_Node *)p);
}


static u32 ImageTexture_get_field_count(GF_Node *node, u8 dummy)
{
	return 4;
}

static GF_Err ImageTexture_get_field(GF_Node *node, GF_FieldInfo *info)
{
	switch (info->fieldIndex) {
	case 0:
		info->name = "url";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_MFURL;
		info->far_ptr = & ((X_ImageTexture *) node)->url;
		return GF_OK;
	case 1:
		info->name = "repeatS";
		info->eventType = GF_SG_EVENT_FIELD;
		info->fieldType = GF_SG_VRML_SFBOOL;
		info->far_ptr = & ((X_ImageTexture *) node)->repeatS;
		return GF_OK;
	case 2:
		info->name = "repeatT";
		info->eventType = GF_SG_EVENT_FIELD;
		info->fieldType = GF_SG_VRML_SFBOOL;
		info->far_ptr = & ((X_ImageTexture *) node)->repeatT;
		return GF_OK;
	case 3:
		info->name = "metadata";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFNODE;
		info->NDTtype = NDT_SFMetadataNode;
		info->far_ptr = & ((X_ImageTexture *)node)->metadata;
		return GF_OK;
	default:
		return GF_BAD_PARAM;
	}
}


static s32 ImageTexture_get_field_index_by_name(char *name)
{
	if (!strcmp("url", name)) return 0;
	if (!strcmp("repeatS", name)) return 1;
	if (!strcmp("repeatT", name)) return 2;
	if (!strcmp("metadata", name)) return 3;
	return -1;
	}


static GF_Node *ImageTexture_Create()
{
	X_ImageTexture *p;
	GF_SAFEALLOC(p, X_ImageTexture);
	if(!p) return NULL;
	gf_node_setup((GF_Node *)p, TAG_X3D_ImageTexture);

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
	X_IndexedFaceSet *p = (X_IndexedFaceSet *) node;
	gf_sg_mfint32_del(p->set_colorIndex);
	gf_sg_mfint32_del(p->set_coordIndex);
	gf_sg_mfint32_del(p->set_normalIndex);
	gf_sg_mfint32_del(p->set_texCoordIndex);
	gf_node_unregister((GF_Node *) p->color, node);	
	gf_node_unregister((GF_Node *) p->coord, node);	
	gf_node_unregister((GF_Node *) p->normal, node);	
	gf_node_unregister((GF_Node *) p->texCoord, node);	
	gf_sg_mfint32_del(p->colorIndex);
	gf_sg_mfint32_del(p->coordIndex);
	gf_sg_mfint32_del(p->normalIndex);
	gf_sg_mfint32_del(p->texCoordIndex);
	gf_node_unregister((GF_Node *) p->metadata, node);	
	gf_node_free((GF_Node *)p);
}


static u32 IndexedFaceSet_get_field_count(GF_Node *node, u8 dummy)
{
	return 19;
}

static GF_Err IndexedFaceSet_get_field(GF_Node *node, GF_FieldInfo *info)
{
	switch (info->fieldIndex) {
	case 0:
		info->name = "set_colorIndex";
		info->eventType = GF_SG_EVENT_IN;
		info->on_event_in = ((X_IndexedFaceSet *)node)->on_set_colorIndex;
		info->fieldType = GF_SG_VRML_MFINT32;
		info->far_ptr = & ((X_IndexedFaceSet *) node)->set_colorIndex;
		return GF_OK;
	case 1:
		info->name = "set_coordIndex";
		info->eventType = GF_SG_EVENT_IN;
		info->on_event_in = ((X_IndexedFaceSet *)node)->on_set_coordIndex;
		info->fieldType = GF_SG_VRML_MFINT32;
		info->far_ptr = & ((X_IndexedFaceSet *) node)->set_coordIndex;
		return GF_OK;
	case 2:
		info->name = "set_normalIndex";
		info->eventType = GF_SG_EVENT_IN;
		info->on_event_in = ((X_IndexedFaceSet *)node)->on_set_normalIndex;
		info->fieldType = GF_SG_VRML_MFINT32;
		info->far_ptr = & ((X_IndexedFaceSet *) node)->set_normalIndex;
		return GF_OK;
	case 3:
		info->name = "set_texCoordIndex";
		info->eventType = GF_SG_EVENT_IN;
		info->on_event_in = ((X_IndexedFaceSet *)node)->on_set_texCoordIndex;
		info->fieldType = GF_SG_VRML_MFINT32;
		info->far_ptr = & ((X_IndexedFaceSet *) node)->set_texCoordIndex;
		return GF_OK;
	case 4:
		info->name = "color";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFNODE;
		info->NDTtype = NDT_SFColorNode;
		info->far_ptr = & ((X_IndexedFaceSet *)node)->color;
		return GF_OK;
	case 5:
		info->name = "coord";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFNODE;
		info->NDTtype = NDT_SFCoordinateNode;
		info->far_ptr = & ((X_IndexedFaceSet *)node)->coord;
		return GF_OK;
	case 6:
		info->name = "normal";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFNODE;
		info->NDTtype = NDT_SFNormalNode;
		info->far_ptr = & ((X_IndexedFaceSet *)node)->normal;
		return GF_OK;
	case 7:
		info->name = "texCoord";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFNODE;
		info->NDTtype = NDT_SFTextureCoordinateNode;
		info->far_ptr = & ((X_IndexedFaceSet *)node)->texCoord;
		return GF_OK;
	case 8:
		info->name = "ccw";
		info->eventType = GF_SG_EVENT_FIELD;
		info->fieldType = GF_SG_VRML_SFBOOL;
		info->far_ptr = & ((X_IndexedFaceSet *) node)->ccw;
		return GF_OK;
	case 9:
		info->name = "colorIndex";
		info->eventType = GF_SG_EVENT_FIELD;
		info->fieldType = GF_SG_VRML_MFINT32;
		info->far_ptr = & ((X_IndexedFaceSet *) node)->colorIndex;
		return GF_OK;
	case 10:
		info->name = "colorPerVertex";
		info->eventType = GF_SG_EVENT_FIELD;
		info->fieldType = GF_SG_VRML_SFBOOL;
		info->far_ptr = & ((X_IndexedFaceSet *) node)->colorPerVertex;
		return GF_OK;
	case 11:
		info->name = "convex";
		info->eventType = GF_SG_EVENT_FIELD;
		info->fieldType = GF_SG_VRML_SFBOOL;
		info->far_ptr = & ((X_IndexedFaceSet *) node)->convex;
		return GF_OK;
	case 12:
		info->name = "coordIndex";
		info->eventType = GF_SG_EVENT_FIELD;
		info->fieldType = GF_SG_VRML_MFINT32;
		info->far_ptr = & ((X_IndexedFaceSet *) node)->coordIndex;
		return GF_OK;
	case 13:
		info->name = "creaseAngle";
		info->eventType = GF_SG_EVENT_FIELD;
		info->fieldType = GF_SG_VRML_SFFLOAT;
		info->far_ptr = & ((X_IndexedFaceSet *) node)->creaseAngle;
		return GF_OK;
	case 14:
		info->name = "normalIndex";
		info->eventType = GF_SG_EVENT_FIELD;
		info->fieldType = GF_SG_VRML_MFINT32;
		info->far_ptr = & ((X_IndexedFaceSet *) node)->normalIndex;
		return GF_OK;
	case 15:
		info->name = "normalPerVertex";
		info->eventType = GF_SG_EVENT_FIELD;
		info->fieldType = GF_SG_VRML_SFBOOL;
		info->far_ptr = & ((X_IndexedFaceSet *) node)->normalPerVertex;
		return GF_OK;
	case 16:
		info->name = "solid";
		info->eventType = GF_SG_EVENT_FIELD;
		info->fieldType = GF_SG_VRML_SFBOOL;
		info->far_ptr = & ((X_IndexedFaceSet *) node)->solid;
		return GF_OK;
	case 17:
		info->name = "texCoordIndex";
		info->eventType = GF_SG_EVENT_FIELD;
		info->fieldType = GF_SG_VRML_MFINT32;
		info->far_ptr = & ((X_IndexedFaceSet *) node)->texCoordIndex;
		return GF_OK;
	case 18:
		info->name = "metadata";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFNODE;
		info->NDTtype = NDT_SFMetadataNode;
		info->far_ptr = & ((X_IndexedFaceSet *)node)->metadata;
		return GF_OK;
	default:
		return GF_BAD_PARAM;
	}
}


static s32 IndexedFaceSet_get_field_index_by_name(char *name)
{
	if (!strcmp("set_colorIndex", name)) return 0;
	if (!strcmp("set_coordIndex", name)) return 1;
	if (!strcmp("set_normalIndex", name)) return 2;
	if (!strcmp("set_texCoordIndex", name)) return 3;
	if (!strcmp("color", name)) return 4;
	if (!strcmp("coord", name)) return 5;
	if (!strcmp("normal", name)) return 6;
	if (!strcmp("texCoord", name)) return 7;
	if (!strcmp("ccw", name)) return 8;
	if (!strcmp("colorIndex", name)) return 9;
	if (!strcmp("colorPerVertex", name)) return 10;
	if (!strcmp("convex", name)) return 11;
	if (!strcmp("coordIndex", name)) return 12;
	if (!strcmp("creaseAngle", name)) return 13;
	if (!strcmp("normalIndex", name)) return 14;
	if (!strcmp("normalPerVertex", name)) return 15;
	if (!strcmp("solid", name)) return 16;
	if (!strcmp("texCoordIndex", name)) return 17;
	if (!strcmp("metadata", name)) return 18;
	return -1;
	}


static GF_Node *IndexedFaceSet_Create()
{
	X_IndexedFaceSet *p;
	GF_SAFEALLOC(p, X_IndexedFaceSet);
	if(!p) return NULL;
	gf_node_setup((GF_Node *)p, TAG_X3D_IndexedFaceSet);

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
	IndexedLineSet Node deletion
*/

static void IndexedLineSet_Del(GF_Node *node)
{
	X_IndexedLineSet *p = (X_IndexedLineSet *) node;
	gf_sg_mfint32_del(p->set_colorIndex);
	gf_sg_mfint32_del(p->set_coordIndex);
	gf_node_unregister((GF_Node *) p->color, node);	
	gf_node_unregister((GF_Node *) p->coord, node);	
	gf_sg_mfint32_del(p->colorIndex);
	gf_sg_mfint32_del(p->coordIndex);
	gf_node_unregister((GF_Node *) p->metadata, node);	
	gf_node_free((GF_Node *)p);
}


static u32 IndexedLineSet_get_field_count(GF_Node *node, u8 dummy)
{
	return 8;
}

static GF_Err IndexedLineSet_get_field(GF_Node *node, GF_FieldInfo *info)
{
	switch (info->fieldIndex) {
	case 0:
		info->name = "set_colorIndex";
		info->eventType = GF_SG_EVENT_IN;
		info->on_event_in = ((X_IndexedLineSet *)node)->on_set_colorIndex;
		info->fieldType = GF_SG_VRML_MFINT32;
		info->far_ptr = & ((X_IndexedLineSet *) node)->set_colorIndex;
		return GF_OK;
	case 1:
		info->name = "set_coordIndex";
		info->eventType = GF_SG_EVENT_IN;
		info->on_event_in = ((X_IndexedLineSet *)node)->on_set_coordIndex;
		info->fieldType = GF_SG_VRML_MFINT32;
		info->far_ptr = & ((X_IndexedLineSet *) node)->set_coordIndex;
		return GF_OK;
	case 2:
		info->name = "color";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFNODE;
		info->NDTtype = NDT_SFColorNode;
		info->far_ptr = & ((X_IndexedLineSet *)node)->color;
		return GF_OK;
	case 3:
		info->name = "coord";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFNODE;
		info->NDTtype = NDT_SFCoordinateNode;
		info->far_ptr = & ((X_IndexedLineSet *)node)->coord;
		return GF_OK;
	case 4:
		info->name = "colorIndex";
		info->eventType = GF_SG_EVENT_FIELD;
		info->fieldType = GF_SG_VRML_MFINT32;
		info->far_ptr = & ((X_IndexedLineSet *) node)->colorIndex;
		return GF_OK;
	case 5:
		info->name = "colorPerVertex";
		info->eventType = GF_SG_EVENT_FIELD;
		info->fieldType = GF_SG_VRML_SFBOOL;
		info->far_ptr = & ((X_IndexedLineSet *) node)->colorPerVertex;
		return GF_OK;
	case 6:
		info->name = "coordIndex";
		info->eventType = GF_SG_EVENT_FIELD;
		info->fieldType = GF_SG_VRML_MFINT32;
		info->far_ptr = & ((X_IndexedLineSet *) node)->coordIndex;
		return GF_OK;
	case 7:
		info->name = "metadata";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFNODE;
		info->NDTtype = NDT_SFMetadataNode;
		info->far_ptr = & ((X_IndexedLineSet *)node)->metadata;
		return GF_OK;
	default:
		return GF_BAD_PARAM;
	}
}


static s32 IndexedLineSet_get_field_index_by_name(char *name)
{
	if (!strcmp("set_colorIndex", name)) return 0;
	if (!strcmp("set_coordIndex", name)) return 1;
	if (!strcmp("color", name)) return 2;
	if (!strcmp("coord", name)) return 3;
	if (!strcmp("colorIndex", name)) return 4;
	if (!strcmp("colorPerVertex", name)) return 5;
	if (!strcmp("coordIndex", name)) return 6;
	if (!strcmp("metadata", name)) return 7;
	return -1;
	}


static GF_Node *IndexedLineSet_Create()
{
	X_IndexedLineSet *p;
	GF_SAFEALLOC(p, X_IndexedLineSet);
	if(!p) return NULL;
	gf_node_setup((GF_Node *)p, TAG_X3D_IndexedLineSet);

	/*default field values*/
	p->colorPerVertex = 1;
	return (GF_Node *)p;
}


/*
	IndexedTriangleFanSet Node deletion
*/

static void IndexedTriangleFanSet_Del(GF_Node *node)
{
	X_IndexedTriangleFanSet *p = (X_IndexedTriangleFanSet *) node;
	gf_sg_mfint32_del(p->set_index);
	gf_node_unregister((GF_Node *) p->color, node);	
	gf_node_unregister((GF_Node *) p->coord, node);	
	gf_node_unregister((GF_Node *) p->normal, node);	
	gf_node_unregister((GF_Node *) p->texCoord, node);	
	gf_sg_mfint32_del(p->index);
	gf_node_unregister((GF_Node *) p->metadata, node);	
	gf_node_free((GF_Node *)p);
}


static u32 IndexedTriangleFanSet_get_field_count(GF_Node *node, u8 dummy)
{
	return 11;
}

static GF_Err IndexedTriangleFanSet_get_field(GF_Node *node, GF_FieldInfo *info)
{
	switch (info->fieldIndex) {
	case 0:
		info->name = "set_index";
		info->eventType = GF_SG_EVENT_IN;
		info->on_event_in = ((X_IndexedTriangleFanSet *)node)->on_set_index;
		info->fieldType = GF_SG_VRML_MFINT32;
		info->far_ptr = & ((X_IndexedTriangleFanSet *) node)->set_index;
		return GF_OK;
	case 1:
		info->name = "color";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFNODE;
		info->NDTtype = NDT_SFColorNode;
		info->far_ptr = & ((X_IndexedTriangleFanSet *)node)->color;
		return GF_OK;
	case 2:
		info->name = "coord";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFNODE;
		info->NDTtype = NDT_SFCoordinateNode;
		info->far_ptr = & ((X_IndexedTriangleFanSet *)node)->coord;
		return GF_OK;
	case 3:
		info->name = "normal";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFNODE;
		info->NDTtype = NDT_SFNormalNode;
		info->far_ptr = & ((X_IndexedTriangleFanSet *)node)->normal;
		return GF_OK;
	case 4:
		info->name = "texCoord";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFNODE;
		info->NDTtype = NDT_SFTextureCoordinateNode;
		info->far_ptr = & ((X_IndexedTriangleFanSet *)node)->texCoord;
		return GF_OK;
	case 5:
		info->name = "ccw";
		info->eventType = GF_SG_EVENT_FIELD;
		info->fieldType = GF_SG_VRML_SFBOOL;
		info->far_ptr = & ((X_IndexedTriangleFanSet *) node)->ccw;
		return GF_OK;
	case 6:
		info->name = "colorPerVertex";
		info->eventType = GF_SG_EVENT_FIELD;
		info->fieldType = GF_SG_VRML_SFBOOL;
		info->far_ptr = & ((X_IndexedTriangleFanSet *) node)->colorPerVertex;
		return GF_OK;
	case 7:
		info->name = "normalPerVertex";
		info->eventType = GF_SG_EVENT_FIELD;
		info->fieldType = GF_SG_VRML_SFBOOL;
		info->far_ptr = & ((X_IndexedTriangleFanSet *) node)->normalPerVertex;
		return GF_OK;
	case 8:
		info->name = "solid";
		info->eventType = GF_SG_EVENT_FIELD;
		info->fieldType = GF_SG_VRML_SFBOOL;
		info->far_ptr = & ((X_IndexedTriangleFanSet *) node)->solid;
		return GF_OK;
	case 9:
		info->name = "index";
		info->eventType = GF_SG_EVENT_FIELD;
		info->fieldType = GF_SG_VRML_MFINT32;
		info->far_ptr = & ((X_IndexedTriangleFanSet *) node)->index;
		return GF_OK;
	case 10:
		info->name = "metadata";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFNODE;
		info->NDTtype = NDT_SFMetadataNode;
		info->far_ptr = & ((X_IndexedTriangleFanSet *)node)->metadata;
		return GF_OK;
	default:
		return GF_BAD_PARAM;
	}
}


static s32 IndexedTriangleFanSet_get_field_index_by_name(char *name)
{
	if (!strcmp("set_index", name)) return 0;
	if (!strcmp("color", name)) return 1;
	if (!strcmp("coord", name)) return 2;
	if (!strcmp("normal", name)) return 3;
	if (!strcmp("texCoord", name)) return 4;
	if (!strcmp("ccw", name)) return 5;
	if (!strcmp("colorPerVertex", name)) return 6;
	if (!strcmp("normalPerVertex", name)) return 7;
	if (!strcmp("solid", name)) return 8;
	if (!strcmp("index", name)) return 9;
	if (!strcmp("metadata", name)) return 10;
	return -1;
	}


static GF_Node *IndexedTriangleFanSet_Create()
{
	X_IndexedTriangleFanSet *p;
	GF_SAFEALLOC(p, X_IndexedTriangleFanSet);
	if(!p) return NULL;
	gf_node_setup((GF_Node *)p, TAG_X3D_IndexedTriangleFanSet);

	/*default field values*/
	p->ccw = 1;
	p->colorPerVertex = 1;
	p->normalPerVertex = 1;
	p->solid = 1;
	return (GF_Node *)p;
}


/*
	IndexedTriangleSet Node deletion
*/

static void IndexedTriangleSet_Del(GF_Node *node)
{
	X_IndexedTriangleSet *p = (X_IndexedTriangleSet *) node;
	gf_sg_mfint32_del(p->set_index);
	gf_node_unregister((GF_Node *) p->color, node);	
	gf_node_unregister((GF_Node *) p->coord, node);	
	gf_node_unregister((GF_Node *) p->normal, node);	
	gf_node_unregister((GF_Node *) p->texCoord, node);	
	gf_sg_mfint32_del(p->index);
	gf_node_unregister((GF_Node *) p->metadata, node);	
	gf_node_free((GF_Node *)p);
}


static u32 IndexedTriangleSet_get_field_count(GF_Node *node, u8 dummy)
{
	return 11;
}

static GF_Err IndexedTriangleSet_get_field(GF_Node *node, GF_FieldInfo *info)
{
	switch (info->fieldIndex) {
	case 0:
		info->name = "set_index";
		info->eventType = GF_SG_EVENT_IN;
		info->on_event_in = ((X_IndexedTriangleSet *)node)->on_set_index;
		info->fieldType = GF_SG_VRML_MFINT32;
		info->far_ptr = & ((X_IndexedTriangleSet *) node)->set_index;
		return GF_OK;
	case 1:
		info->name = "color";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFNODE;
		info->NDTtype = NDT_SFColorNode;
		info->far_ptr = & ((X_IndexedTriangleSet *)node)->color;
		return GF_OK;
	case 2:
		info->name = "coord";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFNODE;
		info->NDTtype = NDT_SFCoordinateNode;
		info->far_ptr = & ((X_IndexedTriangleSet *)node)->coord;
		return GF_OK;
	case 3:
		info->name = "normal";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFNODE;
		info->NDTtype = NDT_SFNormalNode;
		info->far_ptr = & ((X_IndexedTriangleSet *)node)->normal;
		return GF_OK;
	case 4:
		info->name = "texCoord";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFNODE;
		info->NDTtype = NDT_SFTextureCoordinateNode;
		info->far_ptr = & ((X_IndexedTriangleSet *)node)->texCoord;
		return GF_OK;
	case 5:
		info->name = "ccw";
		info->eventType = GF_SG_EVENT_FIELD;
		info->fieldType = GF_SG_VRML_SFBOOL;
		info->far_ptr = & ((X_IndexedTriangleSet *) node)->ccw;
		return GF_OK;
	case 6:
		info->name = "colorPerVertex";
		info->eventType = GF_SG_EVENT_FIELD;
		info->fieldType = GF_SG_VRML_SFBOOL;
		info->far_ptr = & ((X_IndexedTriangleSet *) node)->colorPerVertex;
		return GF_OK;
	case 7:
		info->name = "normalPerVertex";
		info->eventType = GF_SG_EVENT_FIELD;
		info->fieldType = GF_SG_VRML_SFBOOL;
		info->far_ptr = & ((X_IndexedTriangleSet *) node)->normalPerVertex;
		return GF_OK;
	case 8:
		info->name = "solid";
		info->eventType = GF_SG_EVENT_FIELD;
		info->fieldType = GF_SG_VRML_SFBOOL;
		info->far_ptr = & ((X_IndexedTriangleSet *) node)->solid;
		return GF_OK;
	case 9:
		info->name = "index";
		info->eventType = GF_SG_EVENT_FIELD;
		info->fieldType = GF_SG_VRML_MFINT32;
		info->far_ptr = & ((X_IndexedTriangleSet *) node)->index;
		return GF_OK;
	case 10:
		info->name = "metadata";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFNODE;
		info->NDTtype = NDT_SFMetadataNode;
		info->far_ptr = & ((X_IndexedTriangleSet *)node)->metadata;
		return GF_OK;
	default:
		return GF_BAD_PARAM;
	}
}


static s32 IndexedTriangleSet_get_field_index_by_name(char *name)
{
	if (!strcmp("set_index", name)) return 0;
	if (!strcmp("color", name)) return 1;
	if (!strcmp("coord", name)) return 2;
	if (!strcmp("normal", name)) return 3;
	if (!strcmp("texCoord", name)) return 4;
	if (!strcmp("ccw", name)) return 5;
	if (!strcmp("colorPerVertex", name)) return 6;
	if (!strcmp("normalPerVertex", name)) return 7;
	if (!strcmp("solid", name)) return 8;
	if (!strcmp("index", name)) return 9;
	if (!strcmp("metadata", name)) return 10;
	return -1;
	}


static GF_Node *IndexedTriangleSet_Create()
{
	X_IndexedTriangleSet *p;
	GF_SAFEALLOC(p, X_IndexedTriangleSet);
	if(!p) return NULL;
	gf_node_setup((GF_Node *)p, TAG_X3D_IndexedTriangleSet);

	/*default field values*/
	p->ccw = 1;
	p->colorPerVertex = 1;
	p->normalPerVertex = 1;
	p->solid = 1;
	return (GF_Node *)p;
}


/*
	IndexedTriangleStripSet Node deletion
*/

static void IndexedTriangleStripSet_Del(GF_Node *node)
{
	X_IndexedTriangleStripSet *p = (X_IndexedTriangleStripSet *) node;
	gf_sg_mfint32_del(p->set_index);
	gf_node_unregister((GF_Node *) p->color, node);	
	gf_node_unregister((GF_Node *) p->coord, node);	
	gf_node_unregister((GF_Node *) p->normal, node);	
	gf_node_unregister((GF_Node *) p->texCoord, node);	
	gf_sg_mfint32_del(p->index);
	gf_node_unregister((GF_Node *) p->metadata, node);	
	gf_node_free((GF_Node *)p);
}


static u32 IndexedTriangleStripSet_get_field_count(GF_Node *node, u8 dummy)
{
	return 11;
}

static GF_Err IndexedTriangleStripSet_get_field(GF_Node *node, GF_FieldInfo *info)
{
	switch (info->fieldIndex) {
	case 0:
		info->name = "set_index";
		info->eventType = GF_SG_EVENT_IN;
		info->on_event_in = ((X_IndexedTriangleStripSet *)node)->on_set_index;
		info->fieldType = GF_SG_VRML_MFINT32;
		info->far_ptr = & ((X_IndexedTriangleStripSet *) node)->set_index;
		return GF_OK;
	case 1:
		info->name = "color";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFNODE;
		info->NDTtype = NDT_SFColorNode;
		info->far_ptr = & ((X_IndexedTriangleStripSet *)node)->color;
		return GF_OK;
	case 2:
		info->name = "coord";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFNODE;
		info->NDTtype = NDT_SFCoordinateNode;
		info->far_ptr = & ((X_IndexedTriangleStripSet *)node)->coord;
		return GF_OK;
	case 3:
		info->name = "creaseAngle";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFFLOAT;
		info->far_ptr = & ((X_IndexedTriangleStripSet *) node)->creaseAngle;
		return GF_OK;
	case 4:
		info->name = "normal";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFNODE;
		info->NDTtype = NDT_SFNormalNode;
		info->far_ptr = & ((X_IndexedTriangleStripSet *)node)->normal;
		return GF_OK;
	case 5:
		info->name = "texCoord";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFNODE;
		info->NDTtype = NDT_SFTextureCoordinateNode;
		info->far_ptr = & ((X_IndexedTriangleStripSet *)node)->texCoord;
		return GF_OK;
	case 6:
		info->name = "ccw";
		info->eventType = GF_SG_EVENT_FIELD;
		info->fieldType = GF_SG_VRML_SFBOOL;
		info->far_ptr = & ((X_IndexedTriangleStripSet *) node)->ccw;
		return GF_OK;
	case 7:
		info->name = "normalPerVertex";
		info->eventType = GF_SG_EVENT_FIELD;
		info->fieldType = GF_SG_VRML_SFBOOL;
		info->far_ptr = & ((X_IndexedTriangleStripSet *) node)->normalPerVertex;
		return GF_OK;
	case 8:
		info->name = "solid";
		info->eventType = GF_SG_EVENT_FIELD;
		info->fieldType = GF_SG_VRML_SFBOOL;
		info->far_ptr = & ((X_IndexedTriangleStripSet *) node)->solid;
		return GF_OK;
	case 9:
		info->name = "index";
		info->eventType = GF_SG_EVENT_FIELD;
		info->fieldType = GF_SG_VRML_MFINT32;
		info->far_ptr = & ((X_IndexedTriangleStripSet *) node)->index;
		return GF_OK;
	case 10:
		info->name = "metadata";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFNODE;
		info->NDTtype = NDT_SFMetadataNode;
		info->far_ptr = & ((X_IndexedTriangleStripSet *)node)->metadata;
		return GF_OK;
	default:
		return GF_BAD_PARAM;
	}
}


static s32 IndexedTriangleStripSet_get_field_index_by_name(char *name)
{
	if (!strcmp("set_index", name)) return 0;
	if (!strcmp("color", name)) return 1;
	if (!strcmp("coord", name)) return 2;
	if (!strcmp("creaseAngle", name)) return 3;
	if (!strcmp("normal", name)) return 4;
	if (!strcmp("texCoord", name)) return 5;
	if (!strcmp("ccw", name)) return 6;
	if (!strcmp("normalPerVertex", name)) return 7;
	if (!strcmp("solid", name)) return 8;
	if (!strcmp("index", name)) return 9;
	if (!strcmp("metadata", name)) return 10;
	return -1;
	}


static GF_Node *IndexedTriangleStripSet_Create()
{
	X_IndexedTriangleStripSet *p;
	GF_SAFEALLOC(p, X_IndexedTriangleStripSet);
	if(!p) return NULL;
	gf_node_setup((GF_Node *)p, TAG_X3D_IndexedTriangleStripSet);

	/*default field values*/
	p->creaseAngle = FLT2FIX(0);
	p->ccw = 1;
	p->normalPerVertex = 1;
	p->solid = 1;
	return (GF_Node *)p;
}


/*
	Inline Node deletion
*/

static void Inline_Del(GF_Node *node)
{
	X_Inline *p = (X_Inline *) node;
	gf_sg_mfurl_del(p->url);
	gf_node_unregister((GF_Node *) p->metadata, node);	
	gf_node_free((GF_Node *)p);
}


static u32 Inline_get_field_count(GF_Node *node, u8 dummy)
{
	return 3;
}

static GF_Err Inline_get_field(GF_Node *node, GF_FieldInfo *info)
{
	switch (info->fieldIndex) {
	case 0:
		info->name = "url";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_MFURL;
		info->far_ptr = & ((X_Inline *) node)->url;
		return GF_OK;
	case 1:
		info->name = "metadata";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFNODE;
		info->NDTtype = NDT_SFMetadataNode;
		info->far_ptr = & ((X_Inline *)node)->metadata;
		return GF_OK;
	case 2:
		info->name = "load";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFBOOL;
		info->far_ptr = & ((X_Inline *) node)->load;
		return GF_OK;
	default:
		return GF_BAD_PARAM;
	}
}


static s32 Inline_get_field_index_by_name(char *name)
{
	if (!strcmp("url", name)) return 0;
	if (!strcmp("metadata", name)) return 1;
	if (!strcmp("load", name)) return 2;
	return -1;
	}


static GF_Node *Inline_Create()
{
	X_Inline *p;
	GF_SAFEALLOC(p, X_Inline);
	if(!p) return NULL;
	gf_node_setup((GF_Node *)p, TAG_X3D_Inline);

	/*default field values*/
	p->load = 1;
	return (GF_Node *)p;
}


/*
	IntegerSequencer Node deletion
*/

static void IntegerSequencer_Del(GF_Node *node)
{
	X_IntegerSequencer *p = (X_IntegerSequencer *) node;
	gf_sg_mffloat_del(p->key);
	gf_sg_mfint32_del(p->keyValue);
	gf_node_unregister((GF_Node *) p->metadata, node);	
	gf_node_free((GF_Node *)p);
}


static u32 IntegerSequencer_get_field_count(GF_Node *node, u8 dummy)
{
	return 7;
}

static GF_Err IntegerSequencer_get_field(GF_Node *node, GF_FieldInfo *info)
{
	switch (info->fieldIndex) {
	case 0:
		info->name = "next";
		info->eventType = GF_SG_EVENT_IN;
		info->on_event_in = ((X_IntegerSequencer *)node)->on_next;
		info->fieldType = GF_SG_VRML_SFBOOL;
		info->far_ptr = & ((X_IntegerSequencer *) node)->next;
		return GF_OK;
	case 1:
		info->name = "previous";
		info->eventType = GF_SG_EVENT_IN;
		info->on_event_in = ((X_IntegerSequencer *)node)->on_previous;
		info->fieldType = GF_SG_VRML_SFBOOL;
		info->far_ptr = & ((X_IntegerSequencer *) node)->previous;
		return GF_OK;
	case 2:
		info->name = "set_fraction";
		info->eventType = GF_SG_EVENT_IN;
		info->on_event_in = ((X_IntegerSequencer *)node)->on_set_fraction;
		info->fieldType = GF_SG_VRML_SFFLOAT;
		info->far_ptr = & ((X_IntegerSequencer *) node)->set_fraction;
		return GF_OK;
	case 3:
		info->name = "key";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_MFFLOAT;
		info->far_ptr = & ((X_IntegerSequencer *) node)->key;
		return GF_OK;
	case 4:
		info->name = "keyValue";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_MFINT32;
		info->far_ptr = & ((X_IntegerSequencer *) node)->keyValue;
		return GF_OK;
	case 5:
		info->name = "value_changed";
		info->eventType = GF_SG_EVENT_OUT;
		info->fieldType = GF_SG_VRML_SFINT32;
		info->far_ptr = & ((X_IntegerSequencer *) node)->value_changed;
		return GF_OK;
	case 6:
		info->name = "metadata";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFNODE;
		info->NDTtype = NDT_SFMetadataNode;
		info->far_ptr = & ((X_IntegerSequencer *)node)->metadata;
		return GF_OK;
	default:
		return GF_BAD_PARAM;
	}
}


static s32 IntegerSequencer_get_field_index_by_name(char *name)
{
	if (!strcmp("next", name)) return 0;
	if (!strcmp("previous", name)) return 1;
	if (!strcmp("set_fraction", name)) return 2;
	if (!strcmp("key", name)) return 3;
	if (!strcmp("keyValue", name)) return 4;
	if (!strcmp("value_changed", name)) return 5;
	if (!strcmp("metadata", name)) return 6;
	return -1;
	}


static GF_Node *IntegerSequencer_Create()
{
	X_IntegerSequencer *p;
	GF_SAFEALLOC(p, X_IntegerSequencer);
	if(!p) return NULL;
	gf_node_setup((GF_Node *)p, TAG_X3D_IntegerSequencer);

	/*default field values*/
	return (GF_Node *)p;
}


/*
	IntegerTrigger Node deletion
*/

static void IntegerTrigger_Del(GF_Node *node)
{
	X_IntegerTrigger *p = (X_IntegerTrigger *) node;
	gf_node_unregister((GF_Node *) p->metadata, node);	
	gf_node_free((GF_Node *)p);
}


static u32 IntegerTrigger_get_field_count(GF_Node *node, u8 dummy)
{
	return 4;
}

static GF_Err IntegerTrigger_get_field(GF_Node *node, GF_FieldInfo *info)
{
	switch (info->fieldIndex) {
	case 0:
		info->name = "set_boolean";
		info->eventType = GF_SG_EVENT_IN;
		info->on_event_in = ((X_IntegerTrigger *)node)->on_set_boolean;
		info->fieldType = GF_SG_VRML_SFBOOL;
		info->far_ptr = & ((X_IntegerTrigger *) node)->set_boolean;
		return GF_OK;
	case 1:
		info->name = "integerKey";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFINT32;
		info->far_ptr = & ((X_IntegerTrigger *) node)->integerKey;
		return GF_OK;
	case 2:
		info->name = "triggerValue";
		info->eventType = GF_SG_EVENT_OUT;
		info->fieldType = GF_SG_VRML_SFINT32;
		info->far_ptr = & ((X_IntegerTrigger *) node)->triggerValue;
		return GF_OK;
	case 3:
		info->name = "metadata";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFNODE;
		info->NDTtype = NDT_SFMetadataNode;
		info->far_ptr = & ((X_IntegerTrigger *)node)->metadata;
		return GF_OK;
	default:
		return GF_BAD_PARAM;
	}
}


static s32 IntegerTrigger_get_field_index_by_name(char *name)
{
	if (!strcmp("set_boolean", name)) return 0;
	if (!strcmp("integerKey", name)) return 1;
	if (!strcmp("triggerValue", name)) return 2;
	if (!strcmp("metadata", name)) return 3;
	return -1;
	}


static GF_Node *IntegerTrigger_Create()
{
	X_IntegerTrigger *p;
	GF_SAFEALLOC(p, X_IntegerTrigger);
	if(!p) return NULL;
	gf_node_setup((GF_Node *)p, TAG_X3D_IntegerTrigger);

	/*default field values*/
	p->integerKey = -1;
	return (GF_Node *)p;
}


/*
	KeySensor Node deletion
*/

static void KeySensor_Del(GF_Node *node)
{
	X_KeySensor *p = (X_KeySensor *) node;
	gf_sg_sfstring_del(p->keyPress);
	gf_sg_sfstring_del(p->keyRelease);
	gf_node_unregister((GF_Node *) p->metadata, node);	
	gf_node_free((GF_Node *)p);
}


static u32 KeySensor_get_field_count(GF_Node *node, u8 dummy)
{
	return 10;
}

static GF_Err KeySensor_get_field(GF_Node *node, GF_FieldInfo *info)
{
	switch (info->fieldIndex) {
	case 0:
		info->name = "enabled";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFBOOL;
		info->far_ptr = & ((X_KeySensor *) node)->enabled;
		return GF_OK;
	case 1:
		info->name = "actionKeyPress";
		info->eventType = GF_SG_EVENT_OUT;
		info->fieldType = GF_SG_VRML_SFINT32;
		info->far_ptr = & ((X_KeySensor *) node)->actionKeyPress;
		return GF_OK;
	case 2:
		info->name = "actionKeyRelease";
		info->eventType = GF_SG_EVENT_OUT;
		info->fieldType = GF_SG_VRML_SFINT32;
		info->far_ptr = & ((X_KeySensor *) node)->actionKeyRelease;
		return GF_OK;
	case 3:
		info->name = "altKey";
		info->eventType = GF_SG_EVENT_OUT;
		info->fieldType = GF_SG_VRML_SFBOOL;
		info->far_ptr = & ((X_KeySensor *) node)->altKey;
		return GF_OK;
	case 4:
		info->name = "controlKey";
		info->eventType = GF_SG_EVENT_OUT;
		info->fieldType = GF_SG_VRML_SFBOOL;
		info->far_ptr = & ((X_KeySensor *) node)->controlKey;
		return GF_OK;
	case 5:
		info->name = "isActive";
		info->eventType = GF_SG_EVENT_OUT;
		info->fieldType = GF_SG_VRML_SFBOOL;
		info->far_ptr = & ((X_KeySensor *) node)->isActive;
		return GF_OK;
	case 6:
		info->name = "keyPress";
		info->eventType = GF_SG_EVENT_OUT;
		info->fieldType = GF_SG_VRML_SFSTRING;
		info->far_ptr = & ((X_KeySensor *) node)->keyPress;
		return GF_OK;
	case 7:
		info->name = "keyRelease";
		info->eventType = GF_SG_EVENT_OUT;
		info->fieldType = GF_SG_VRML_SFSTRING;
		info->far_ptr = & ((X_KeySensor *) node)->keyRelease;
		return GF_OK;
	case 8:
		info->name = "shiftKey";
		info->eventType = GF_SG_EVENT_OUT;
		info->fieldType = GF_SG_VRML_SFBOOL;
		info->far_ptr = & ((X_KeySensor *) node)->shiftKey;
		return GF_OK;
	case 9:
		info->name = "metadata";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFNODE;
		info->NDTtype = NDT_SFMetadataNode;
		info->far_ptr = & ((X_KeySensor *)node)->metadata;
		return GF_OK;
	default:
		return GF_BAD_PARAM;
	}
}


static s32 KeySensor_get_field_index_by_name(char *name)
{
	if (!strcmp("enabled", name)) return 0;
	if (!strcmp("actionKeyPress", name)) return 1;
	if (!strcmp("actionKeyRelease", name)) return 2;
	if (!strcmp("altKey", name)) return 3;
	if (!strcmp("controlKey", name)) return 4;
	if (!strcmp("isActive", name)) return 5;
	if (!strcmp("keyPress", name)) return 6;
	if (!strcmp("keyRelease", name)) return 7;
	if (!strcmp("shiftKey", name)) return 8;
	if (!strcmp("metadata", name)) return 9;
	return -1;
	}


static GF_Node *KeySensor_Create()
{
	X_KeySensor *p;
	GF_SAFEALLOC(p, X_KeySensor);
	if(!p) return NULL;
	gf_node_setup((GF_Node *)p, TAG_X3D_KeySensor);

	/*default field values*/
	p->enabled = 1;
	return (GF_Node *)p;
}


/*
	LineProperties Node deletion
*/

static void LineProperties_Del(GF_Node *node)
{
	X_LineProperties *p = (X_LineProperties *) node;
	gf_node_unregister((GF_Node *) p->metadata, node);	
	gf_node_free((GF_Node *)p);
}


static u32 LineProperties_get_field_count(GF_Node *node, u8 dummy)
{
	return 4;
}

static GF_Err LineProperties_get_field(GF_Node *node, GF_FieldInfo *info)
{
	switch (info->fieldIndex) {
	case 0:
		info->name = "applied";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFBOOL;
		info->far_ptr = & ((X_LineProperties *) node)->applied;
		return GF_OK;
	case 1:
		info->name = "linetype";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFINT32;
		info->far_ptr = & ((X_LineProperties *) node)->linetype;
		return GF_OK;
	case 2:
		info->name = "linewidthScaleFactor";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFFLOAT;
		info->far_ptr = & ((X_LineProperties *) node)->linewidthScaleFactor;
		return GF_OK;
	case 3:
		info->name = "metadata";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFNODE;
		info->NDTtype = NDT_SFMetadataNode;
		info->far_ptr = & ((X_LineProperties *)node)->metadata;
		return GF_OK;
	default:
		return GF_BAD_PARAM;
	}
}


static s32 LineProperties_get_field_index_by_name(char *name)
{
	if (!strcmp("applied", name)) return 0;
	if (!strcmp("linetype", name)) return 1;
	if (!strcmp("linewidthScaleFactor", name)) return 2;
	if (!strcmp("metadata", name)) return 3;
	return -1;
	}


static GF_Node *LineProperties_Create()
{
	X_LineProperties *p;
	GF_SAFEALLOC(p, X_LineProperties);
	if(!p) return NULL;
	gf_node_setup((GF_Node *)p, TAG_X3D_LineProperties);

	/*default field values*/
	p->applied = 1;
	p->linetype = 1;
	p->linewidthScaleFactor = FLT2FIX(0);
	return (GF_Node *)p;
}


/*
	LineSet Node deletion
*/

static void LineSet_Del(GF_Node *node)
{
	X_LineSet *p = (X_LineSet *) node;
	gf_node_unregister((GF_Node *) p->color, node);	
	gf_node_unregister((GF_Node *) p->coord, node);	
	gf_sg_mfint32_del(p->vertexCount);
	gf_node_unregister((GF_Node *) p->metadata, node);	
	gf_node_free((GF_Node *)p);
}


static u32 LineSet_get_field_count(GF_Node *node, u8 dummy)
{
	return 4;
}

static GF_Err LineSet_get_field(GF_Node *node, GF_FieldInfo *info)
{
	switch (info->fieldIndex) {
	case 0:
		info->name = "color";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFNODE;
		info->NDTtype = NDT_SFColorNode;
		info->far_ptr = & ((X_LineSet *)node)->color;
		return GF_OK;
	case 1:
		info->name = "coord";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFNODE;
		info->NDTtype = NDT_SFCoordinateNode;
		info->far_ptr = & ((X_LineSet *)node)->coord;
		return GF_OK;
	case 2:
		info->name = "vertexCount";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_MFINT32;
		info->far_ptr = & ((X_LineSet *) node)->vertexCount;
		return GF_OK;
	case 3:
		info->name = "metadata";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFNODE;
		info->NDTtype = NDT_SFMetadataNode;
		info->far_ptr = & ((X_LineSet *)node)->metadata;
		return GF_OK;
	default:
		return GF_BAD_PARAM;
	}
}


static s32 LineSet_get_field_index_by_name(char *name)
{
	if (!strcmp("color", name)) return 0;
	if (!strcmp("coord", name)) return 1;
	if (!strcmp("vertexCount", name)) return 2;
	if (!strcmp("metadata", name)) return 3;
	return -1;
	}


static GF_Node *LineSet_Create()
{
	X_LineSet *p;
	GF_SAFEALLOC(p, X_LineSet);
	if(!p) return NULL;
	gf_node_setup((GF_Node *)p, TAG_X3D_LineSet);

	/*default field values*/
	return (GF_Node *)p;
}


/*
	LOD Node deletion
*/

static void LOD_Del(GF_Node *node)
{
	X_LOD *p = (X_LOD *) node;
	gf_sg_mffloat_del(p->range);
	gf_node_unregister((GF_Node *) p->metadata, node);	
	gf_sg_vrml_parent_destroy(node);	
	gf_node_free((GF_Node *)p);
}


static u32 LOD_get_field_count(GF_Node *node, u8 dummy)
{
	return 6;
}

static GF_Err LOD_get_field(GF_Node *node, GF_FieldInfo *info)
{
	switch (info->fieldIndex) {
	case 0:
		info->name = "addChildren";
		info->eventType = GF_SG_EVENT_IN;
		info->on_event_in = ((X_LOD *)node)->on_addChildren;
		info->fieldType = GF_SG_VRML_MFNODE;
		info->NDTtype = NDT_SF3DNode;
		info->far_ptr = & ((X_LOD *)node)->addChildren;
		return GF_OK;
	case 1:
		info->name = "removeChildren";
		info->eventType = GF_SG_EVENT_IN;
		info->on_event_in = ((X_LOD *)node)->on_removeChildren;
		info->fieldType = GF_SG_VRML_MFNODE;
		info->NDTtype = NDT_SF3DNode;
		info->far_ptr = & ((X_LOD *)node)->removeChildren;
		return GF_OK;
	case 2:
		info->name = "children";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_MFNODE;
		info->NDTtype = NDT_SF3DNode;
		info->far_ptr = & ((X_LOD *)node)->children;
		return GF_OK;
	case 3:
		info->name = "center";
		info->eventType = GF_SG_EVENT_FIELD;
		info->fieldType = GF_SG_VRML_SFVEC3F;
		info->far_ptr = & ((X_LOD *) node)->center;
		return GF_OK;
	case 4:
		info->name = "range";
		info->eventType = GF_SG_EVENT_FIELD;
		info->fieldType = GF_SG_VRML_MFFLOAT;
		info->far_ptr = & ((X_LOD *) node)->range;
		return GF_OK;
	case 5:
		info->name = "metadata";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFNODE;
		info->NDTtype = NDT_SFMetadataNode;
		info->far_ptr = & ((X_LOD *)node)->metadata;
		return GF_OK;
	default:
		return GF_BAD_PARAM;
	}
}


static s32 LOD_get_field_index_by_name(char *name)
{
	if (!strcmp("addChildren", name)) return 0;
	if (!strcmp("removeChildren", name)) return 1;
	if (!strcmp("children", name)) return 2;
	if (!strcmp("center", name)) return 3;
	if (!strcmp("range", name)) return 4;
	if (!strcmp("metadata", name)) return 5;
	return -1;
	}


static GF_Node *LOD_Create()
{
	X_LOD *p;
	GF_SAFEALLOC(p, X_LOD);
	if(!p) return NULL;
	gf_node_setup((GF_Node *)p, TAG_X3D_LOD);
	gf_sg_vrml_parent_setup((GF_Node *) p);

	/*default field values*/
	p->center.x = FLT2FIX(0);
	p->center.y = FLT2FIX(0);
	p->center.z = FLT2FIX(0);
	return (GF_Node *)p;
}


/*
	Material Node deletion
*/

static void Material_Del(GF_Node *node)
{
	X_Material *p = (X_Material *) node;
	gf_node_unregister((GF_Node *) p->metadata, node);	
	gf_node_free((GF_Node *)p);
}


static u32 Material_get_field_count(GF_Node *node, u8 dummy)
{
	return 7;
}

static GF_Err Material_get_field(GF_Node *node, GF_FieldInfo *info)
{
	switch (info->fieldIndex) {
	case 0:
		info->name = "ambientIntensity";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFFLOAT;
		info->far_ptr = & ((X_Material *) node)->ambientIntensity;
		return GF_OK;
	case 1:
		info->name = "diffuseColor";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFCOLOR;
		info->far_ptr = & ((X_Material *) node)->diffuseColor;
		return GF_OK;
	case 2:
		info->name = "emissiveColor";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFCOLOR;
		info->far_ptr = & ((X_Material *) node)->emissiveColor;
		return GF_OK;
	case 3:
		info->name = "shininess";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFFLOAT;
		info->far_ptr = & ((X_Material *) node)->shininess;
		return GF_OK;
	case 4:
		info->name = "specularColor";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFCOLOR;
		info->far_ptr = & ((X_Material *) node)->specularColor;
		return GF_OK;
	case 5:
		info->name = "transparency";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFFLOAT;
		info->far_ptr = & ((X_Material *) node)->transparency;
		return GF_OK;
	case 6:
		info->name = "metadata";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFNODE;
		info->NDTtype = NDT_SFMetadataNode;
		info->far_ptr = & ((X_Material *)node)->metadata;
		return GF_OK;
	default:
		return GF_BAD_PARAM;
	}
}


static s32 Material_get_field_index_by_name(char *name)
{
	if (!strcmp("ambientIntensity", name)) return 0;
	if (!strcmp("diffuseColor", name)) return 1;
	if (!strcmp("emissiveColor", name)) return 2;
	if (!strcmp("shininess", name)) return 3;
	if (!strcmp("specularColor", name)) return 4;
	if (!strcmp("transparency", name)) return 5;
	if (!strcmp("metadata", name)) return 6;
	return -1;
	}


static GF_Node *Material_Create()
{
	X_Material *p;
	GF_SAFEALLOC(p, X_Material);
	if(!p) return NULL;
	gf_node_setup((GF_Node *)p, TAG_X3D_Material);

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
	MetadataDouble Node deletion
*/

static void MetadataDouble_Del(GF_Node *node)
{
	X_MetadataDouble *p = (X_MetadataDouble *) node;
	gf_sg_sfstring_del(p->name);
	gf_sg_sfstring_del(p->reference);
	gf_sg_mfdouble_del(p->value);
	gf_node_unregister((GF_Node *) p->metadata, node);	
	gf_node_free((GF_Node *)p);
}


static u32 MetadataDouble_get_field_count(GF_Node *node, u8 dummy)
{
	return 4;
}

static GF_Err MetadataDouble_get_field(GF_Node *node, GF_FieldInfo *info)
{
	switch (info->fieldIndex) {
	case 0:
		info->name = "name";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFSTRING;
		info->far_ptr = & ((X_MetadataDouble *) node)->name;
		return GF_OK;
	case 1:
		info->name = "reference";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFSTRING;
		info->far_ptr = & ((X_MetadataDouble *) node)->reference;
		return GF_OK;
	case 2:
		info->name = "value";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_MFDOUBLE;
		info->far_ptr = & ((X_MetadataDouble *) node)->value;
		return GF_OK;
	case 3:
		info->name = "metadata";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFNODE;
		info->NDTtype = NDT_SFMetadataNode;
		info->far_ptr = & ((X_MetadataDouble *)node)->metadata;
		return GF_OK;
	default:
		return GF_BAD_PARAM;
	}
}


static s32 MetadataDouble_get_field_index_by_name(char *name)
{
	if (!strcmp("name", name)) return 0;
	if (!strcmp("reference", name)) return 1;
	if (!strcmp("value", name)) return 2;
	if (!strcmp("metadata", name)) return 3;
	return -1;
	}


static GF_Node *MetadataDouble_Create()
{
	X_MetadataDouble *p;
	GF_SAFEALLOC(p, X_MetadataDouble);
	if(!p) return NULL;
	gf_node_setup((GF_Node *)p, TAG_X3D_MetadataDouble);

	/*default field values*/
	return (GF_Node *)p;
}


/*
	MetadataFloat Node deletion
*/

static void MetadataFloat_Del(GF_Node *node)
{
	X_MetadataFloat *p = (X_MetadataFloat *) node;
	gf_sg_sfstring_del(p->name);
	gf_sg_sfstring_del(p->reference);
	gf_sg_mffloat_del(p->value);
	gf_node_unregister((GF_Node *) p->metadata, node);	
	gf_node_free((GF_Node *)p);
}


static u32 MetadataFloat_get_field_count(GF_Node *node, u8 dummy)
{
	return 4;
}

static GF_Err MetadataFloat_get_field(GF_Node *node, GF_FieldInfo *info)
{
	switch (info->fieldIndex) {
	case 0:
		info->name = "name";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFSTRING;
		info->far_ptr = & ((X_MetadataFloat *) node)->name;
		return GF_OK;
	case 1:
		info->name = "reference";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFSTRING;
		info->far_ptr = & ((X_MetadataFloat *) node)->reference;
		return GF_OK;
	case 2:
		info->name = "value";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_MFFLOAT;
		info->far_ptr = & ((X_MetadataFloat *) node)->value;
		return GF_OK;
	case 3:
		info->name = "metadata";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFNODE;
		info->NDTtype = NDT_SFMetadataNode;
		info->far_ptr = & ((X_MetadataFloat *)node)->metadata;
		return GF_OK;
	default:
		return GF_BAD_PARAM;
	}
}


static s32 MetadataFloat_get_field_index_by_name(char *name)
{
	if (!strcmp("name", name)) return 0;
	if (!strcmp("reference", name)) return 1;
	if (!strcmp("value", name)) return 2;
	if (!strcmp("metadata", name)) return 3;
	return -1;
	}


static GF_Node *MetadataFloat_Create()
{
	X_MetadataFloat *p;
	GF_SAFEALLOC(p, X_MetadataFloat);
	if(!p) return NULL;
	gf_node_setup((GF_Node *)p, TAG_X3D_MetadataFloat);

	/*default field values*/
	return (GF_Node *)p;
}


/*
	MetadataInteger Node deletion
*/

static void MetadataInteger_Del(GF_Node *node)
{
	X_MetadataInteger *p = (X_MetadataInteger *) node;
	gf_sg_sfstring_del(p->name);
	gf_sg_sfstring_del(p->reference);
	gf_sg_mfint32_del(p->value);
	gf_node_unregister((GF_Node *) p->metadata, node);	
	gf_node_free((GF_Node *)p);
}


static u32 MetadataInteger_get_field_count(GF_Node *node, u8 dummy)
{
	return 4;
}

static GF_Err MetadataInteger_get_field(GF_Node *node, GF_FieldInfo *info)
{
	switch (info->fieldIndex) {
	case 0:
		info->name = "name";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFSTRING;
		info->far_ptr = & ((X_MetadataInteger *) node)->name;
		return GF_OK;
	case 1:
		info->name = "reference";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFSTRING;
		info->far_ptr = & ((X_MetadataInteger *) node)->reference;
		return GF_OK;
	case 2:
		info->name = "value";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_MFINT32;
		info->far_ptr = & ((X_MetadataInteger *) node)->value;
		return GF_OK;
	case 3:
		info->name = "metadata";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFNODE;
		info->NDTtype = NDT_SFMetadataNode;
		info->far_ptr = & ((X_MetadataInteger *)node)->metadata;
		return GF_OK;
	default:
		return GF_BAD_PARAM;
	}
}


static s32 MetadataInteger_get_field_index_by_name(char *name)
{
	if (!strcmp("name", name)) return 0;
	if (!strcmp("reference", name)) return 1;
	if (!strcmp("value", name)) return 2;
	if (!strcmp("metadata", name)) return 3;
	return -1;
	}


static GF_Node *MetadataInteger_Create()
{
	X_MetadataInteger *p;
	GF_SAFEALLOC(p, X_MetadataInteger);
	if(!p) return NULL;
	gf_node_setup((GF_Node *)p, TAG_X3D_MetadataInteger);

	/*default field values*/
	return (GF_Node *)p;
}


/*
	MetadataSet Node deletion
*/

static void MetadataSet_Del(GF_Node *node)
{
	X_MetadataSet *p = (X_MetadataSet *) node;
	gf_sg_sfstring_del(p->name);
	gf_sg_sfstring_del(p->reference);
	gf_node_unregister_children(node, p->value);	
	gf_node_unregister((GF_Node *) p->metadata, node);	
	gf_node_free((GF_Node *)p);
}


static u32 MetadataSet_get_field_count(GF_Node *node, u8 dummy)
{
	return 4;
}

static GF_Err MetadataSet_get_field(GF_Node *node, GF_FieldInfo *info)
{
	switch (info->fieldIndex) {
	case 0:
		info->name = "name";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFSTRING;
		info->far_ptr = & ((X_MetadataSet *) node)->name;
		return GF_OK;
	case 1:
		info->name = "reference";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFSTRING;
		info->far_ptr = & ((X_MetadataSet *) node)->reference;
		return GF_OK;
	case 2:
		info->name = "value";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_MFNODE;
		info->NDTtype = NDT_SFMetadataNode;
		info->far_ptr = & ((X_MetadataSet *)node)->value;
		return GF_OK;
	case 3:
		info->name = "metadata";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFNODE;
		info->NDTtype = NDT_SFMetadataNode;
		info->far_ptr = & ((X_MetadataSet *)node)->metadata;
		return GF_OK;
	default:
		return GF_BAD_PARAM;
	}
}


static s32 MetadataSet_get_field_index_by_name(char *name)
{
	if (!strcmp("name", name)) return 0;
	if (!strcmp("reference", name)) return 1;
	if (!strcmp("value", name)) return 2;
	if (!strcmp("metadata", name)) return 3;
	return -1;
	}


static GF_Node *MetadataSet_Create()
{
	X_MetadataSet *p;
	GF_SAFEALLOC(p, X_MetadataSet);
	if(!p) return NULL;
	gf_node_setup((GF_Node *)p, TAG_X3D_MetadataSet);

	/*default field values*/
	return (GF_Node *)p;
}


/*
	MetadataString Node deletion
*/

static void MetadataString_Del(GF_Node *node)
{
	X_MetadataString *p = (X_MetadataString *) node;
	gf_sg_sfstring_del(p->name);
	gf_sg_sfstring_del(p->reference);
	gf_sg_mfstring_del(p->value);
	gf_node_unregister((GF_Node *) p->metadata, node);	
	gf_node_free((GF_Node *)p);
}


static u32 MetadataString_get_field_count(GF_Node *node, u8 dummy)
{
	return 4;
}

static GF_Err MetadataString_get_field(GF_Node *node, GF_FieldInfo *info)
{
	switch (info->fieldIndex) {
	case 0:
		info->name = "name";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFSTRING;
		info->far_ptr = & ((X_MetadataString *) node)->name;
		return GF_OK;
	case 1:
		info->name = "reference";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFSTRING;
		info->far_ptr = & ((X_MetadataString *) node)->reference;
		return GF_OK;
	case 2:
		info->name = "value";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_MFSTRING;
		info->far_ptr = & ((X_MetadataString *) node)->value;
		return GF_OK;
	case 3:
		info->name = "metadata";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFNODE;
		info->NDTtype = NDT_SFMetadataNode;
		info->far_ptr = & ((X_MetadataString *)node)->metadata;
		return GF_OK;
	default:
		return GF_BAD_PARAM;
	}
}


static s32 MetadataString_get_field_index_by_name(char *name)
{
	if (!strcmp("name", name)) return 0;
	if (!strcmp("reference", name)) return 1;
	if (!strcmp("value", name)) return 2;
	if (!strcmp("metadata", name)) return 3;
	return -1;
	}


static GF_Node *MetadataString_Create()
{
	X_MetadataString *p;
	GF_SAFEALLOC(p, X_MetadataString);
	if(!p) return NULL;
	gf_node_setup((GF_Node *)p, TAG_X3D_MetadataString);

	/*default field values*/
	return (GF_Node *)p;
}


/*
	MovieTexture Node deletion
*/

static void MovieTexture_Del(GF_Node *node)
{
	X_MovieTexture *p = (X_MovieTexture *) node;
	gf_sg_mfurl_del(p->url);
	gf_node_unregister((GF_Node *) p->metadata, node);	
	gf_node_free((GF_Node *)p);
}


static u32 MovieTexture_get_field_count(GF_Node *node, u8 dummy)
{
	return 14;
}

static GF_Err MovieTexture_get_field(GF_Node *node, GF_FieldInfo *info)
{
	switch (info->fieldIndex) {
	case 0:
		info->name = "loop";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFBOOL;
		info->far_ptr = & ((X_MovieTexture *) node)->loop;
		return GF_OK;
	case 1:
		info->name = "speed";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFFLOAT;
		info->far_ptr = & ((X_MovieTexture *) node)->speed;
		return GF_OK;
	case 2:
		info->name = "startTime";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFTIME;
		info->far_ptr = & ((X_MovieTexture *) node)->startTime;
		return GF_OK;
	case 3:
		info->name = "stopTime";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFTIME;
		info->far_ptr = & ((X_MovieTexture *) node)->stopTime;
		return GF_OK;
	case 4:
		info->name = "url";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_MFURL;
		info->far_ptr = & ((X_MovieTexture *) node)->url;
		return GF_OK;
	case 5:
		info->name = "repeatS";
		info->eventType = GF_SG_EVENT_FIELD;
		info->fieldType = GF_SG_VRML_SFBOOL;
		info->far_ptr = & ((X_MovieTexture *) node)->repeatS;
		return GF_OK;
	case 6:
		info->name = "repeatT";
		info->eventType = GF_SG_EVENT_FIELD;
		info->fieldType = GF_SG_VRML_SFBOOL;
		info->far_ptr = & ((X_MovieTexture *) node)->repeatT;
		return GF_OK;
	case 7:
		info->name = "duration_changed";
		info->eventType = GF_SG_EVENT_OUT;
		info->fieldType = GF_SG_VRML_SFTIME;
		info->far_ptr = & ((X_MovieTexture *) node)->duration_changed;
		return GF_OK;
	case 8:
		info->name = "isActive";
		info->eventType = GF_SG_EVENT_OUT;
		info->fieldType = GF_SG_VRML_SFBOOL;
		info->far_ptr = & ((X_MovieTexture *) node)->isActive;
		return GF_OK;
	case 9:
		info->name = "metadata";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFNODE;
		info->NDTtype = NDT_SFMetadataNode;
		info->far_ptr = & ((X_MovieTexture *)node)->metadata;
		return GF_OK;
	case 10:
		info->name = "resumeTime";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFTIME;
		info->far_ptr = & ((X_MovieTexture *) node)->resumeTime;
		return GF_OK;
	case 11:
		info->name = "pauseTime";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFTIME;
		info->far_ptr = & ((X_MovieTexture *) node)->pauseTime;
		return GF_OK;
	case 12:
		info->name = "elapsedTime";
		info->eventType = GF_SG_EVENT_OUT;
		info->fieldType = GF_SG_VRML_SFTIME;
		info->far_ptr = & ((X_MovieTexture *) node)->elapsedTime;
		return GF_OK;
	case 13:
		info->name = "isPaused";
		info->eventType = GF_SG_EVENT_OUT;
		info->fieldType = GF_SG_VRML_SFBOOL;
		info->far_ptr = & ((X_MovieTexture *) node)->isPaused;
		return GF_OK;
	default:
		return GF_BAD_PARAM;
	}
}


static s32 MovieTexture_get_field_index_by_name(char *name)
{
	if (!strcmp("loop", name)) return 0;
	if (!strcmp("speed", name)) return 1;
	if (!strcmp("startTime", name)) return 2;
	if (!strcmp("stopTime", name)) return 3;
	if (!strcmp("url", name)) return 4;
	if (!strcmp("repeatS", name)) return 5;
	if (!strcmp("repeatT", name)) return 6;
	if (!strcmp("duration_changed", name)) return 7;
	if (!strcmp("isActive", name)) return 8;
	if (!strcmp("metadata", name)) return 9;
	if (!strcmp("resumeTime", name)) return 10;
	if (!strcmp("pauseTime", name)) return 11;
	if (!strcmp("elapsedTime", name)) return 12;
	if (!strcmp("isPaused", name)) return 13;
	return -1;
	}


static GF_Node *MovieTexture_Create()
{
	X_MovieTexture *p;
	GF_SAFEALLOC(p, X_MovieTexture);
	if(!p) return NULL;
	gf_node_setup((GF_Node *)p, TAG_X3D_MovieTexture);

	/*default field values*/
	p->speed = FLT2FIX(1.0);
	p->startTime = 0;
	p->stopTime = 0;
	p->repeatS = 1;
	p->repeatT = 1;
	p->resumeTime = 0;
	p->pauseTime = 0;
	return (GF_Node *)p;
}


/*
	MultiTexture Node deletion
*/

static void MultiTexture_Del(GF_Node *node)
{
	X_MultiTexture *p = (X_MultiTexture *) node;
	gf_sg_mfstring_del(p->function);
	gf_sg_mfstring_del(p->mode);
	gf_sg_mfstring_del(p->source);
	gf_node_unregister_children(node, p->texture);	
	gf_node_unregister((GF_Node *) p->metadata, node);	
	gf_node_free((GF_Node *)p);
}


static u32 MultiTexture_get_field_count(GF_Node *node, u8 dummy)
{
	return 7;
}

static GF_Err MultiTexture_get_field(GF_Node *node, GF_FieldInfo *info)
{
	switch (info->fieldIndex) {
	case 0:
		info->name = "alpha";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFFLOAT;
		info->far_ptr = & ((X_MultiTexture *) node)->alpha;
		return GF_OK;
	case 1:
		info->name = "color";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFCOLOR;
		info->far_ptr = & ((X_MultiTexture *) node)->color;
		return GF_OK;
	case 2:
		info->name = "function";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_MFSTRING;
		info->far_ptr = & ((X_MultiTexture *) node)->function;
		return GF_OK;
	case 3:
		info->name = "mode";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_MFSTRING;
		info->far_ptr = & ((X_MultiTexture *) node)->mode;
		return GF_OK;
	case 4:
		info->name = "source";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_MFSTRING;
		info->far_ptr = & ((X_MultiTexture *) node)->source;
		return GF_OK;
	case 5:
		info->name = "texture";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_MFNODE;
		info->NDTtype = NDT_SFTextureNode;
		info->far_ptr = & ((X_MultiTexture *)node)->texture;
		return GF_OK;
	case 6:
		info->name = "metadata";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFNODE;
		info->NDTtype = NDT_SFMetadataNode;
		info->far_ptr = & ((X_MultiTexture *)node)->metadata;
		return GF_OK;
	default:
		return GF_BAD_PARAM;
	}
}


static s32 MultiTexture_get_field_index_by_name(char *name)
{
	if (!strcmp("alpha", name)) return 0;
	if (!strcmp("color", name)) return 1;
	if (!strcmp("function", name)) return 2;
	if (!strcmp("mode", name)) return 3;
	if (!strcmp("source", name)) return 4;
	if (!strcmp("texture", name)) return 5;
	if (!strcmp("metadata", name)) return 6;
	return -1;
	}


static GF_Node *MultiTexture_Create()
{
	X_MultiTexture *p;
	GF_SAFEALLOC(p, X_MultiTexture);
	if(!p) return NULL;
	gf_node_setup((GF_Node *)p, TAG_X3D_MultiTexture);

	/*default field values*/
	p->alpha = FLT2FIX(1);
	p->color.red = FLT2FIX(1);
	p->color.green = FLT2FIX(1);
	p->color.blue = FLT2FIX(1);
	return (GF_Node *)p;
}


/*
	MultiTextureCoordinate Node deletion
*/

static void MultiTextureCoordinate_Del(GF_Node *node)
{
	X_MultiTextureCoordinate *p = (X_MultiTextureCoordinate *) node;
	gf_node_unregister_children(node, p->texCoord);	
	gf_node_unregister((GF_Node *) p->metadata, node);	
	gf_node_free((GF_Node *)p);
}


static u32 MultiTextureCoordinate_get_field_count(GF_Node *node, u8 dummy)
{
	return 2;
}

static GF_Err MultiTextureCoordinate_get_field(GF_Node *node, GF_FieldInfo *info)
{
	switch (info->fieldIndex) {
	case 0:
		info->name = "texCoord";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_MFNODE;
		info->NDTtype = NDT_SFTextureCoordinateNode;
		info->far_ptr = & ((X_MultiTextureCoordinate *)node)->texCoord;
		return GF_OK;
	case 1:
		info->name = "metadata";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFNODE;
		info->NDTtype = NDT_SFMetadataNode;
		info->far_ptr = & ((X_MultiTextureCoordinate *)node)->metadata;
		return GF_OK;
	default:
		return GF_BAD_PARAM;
	}
}


static s32 MultiTextureCoordinate_get_field_index_by_name(char *name)
{
	if (!strcmp("texCoord", name)) return 0;
	if (!strcmp("metadata", name)) return 1;
	return -1;
	}


static GF_Node *MultiTextureCoordinate_Create()
{
	X_MultiTextureCoordinate *p;
	GF_SAFEALLOC(p, X_MultiTextureCoordinate);
	if(!p) return NULL;
	gf_node_setup((GF_Node *)p, TAG_X3D_MultiTextureCoordinate);

	/*default field values*/
	return (GF_Node *)p;
}


/*
	MultiTextureTransform Node deletion
*/

static void MultiTextureTransform_Del(GF_Node *node)
{
	X_MultiTextureTransform *p = (X_MultiTextureTransform *) node;
	gf_node_unregister_children(node, p->textureTransform);	
	gf_node_unregister((GF_Node *) p->metadata, node);	
	gf_node_free((GF_Node *)p);
}


static u32 MultiTextureTransform_get_field_count(GF_Node *node, u8 dummy)
{
	return 2;
}

static GF_Err MultiTextureTransform_get_field(GF_Node *node, GF_FieldInfo *info)
{
	switch (info->fieldIndex) {
	case 0:
		info->name = "textureTransform";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_MFNODE;
		info->NDTtype = NDT_SFTextureTransformNode;
		info->far_ptr = & ((X_MultiTextureTransform *)node)->textureTransform;
		return GF_OK;
	case 1:
		info->name = "metadata";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFNODE;
		info->NDTtype = NDT_SFMetadataNode;
		info->far_ptr = & ((X_MultiTextureTransform *)node)->metadata;
		return GF_OK;
	default:
		return GF_BAD_PARAM;
	}
}


static s32 MultiTextureTransform_get_field_index_by_name(char *name)
{
	if (!strcmp("textureTransform", name)) return 0;
	if (!strcmp("metadata", name)) return 1;
	return -1;
	}


static GF_Node *MultiTextureTransform_Create()
{
	X_MultiTextureTransform *p;
	GF_SAFEALLOC(p, X_MultiTextureTransform);
	if(!p) return NULL;
	gf_node_setup((GF_Node *)p, TAG_X3D_MultiTextureTransform);

	/*default field values*/
	return (GF_Node *)p;
}


/*
	NavigationInfo Node deletion
*/

static void NavigationInfo_Del(GF_Node *node)
{
	X_NavigationInfo *p = (X_NavigationInfo *) node;
	gf_sg_mffloat_del(p->avatarSize);
	gf_sg_mfstring_del(p->type);
	gf_node_unregister((GF_Node *) p->metadata, node);	
	gf_sg_mfstring_del(p->transitionType);
	gf_node_free((GF_Node *)p);
}


static u32 NavigationInfo_get_field_count(GF_Node *node, u8 dummy)
{
	return 10;
}

static GF_Err NavigationInfo_get_field(GF_Node *node, GF_FieldInfo *info)
{
	switch (info->fieldIndex) {
	case 0:
		info->name = "set_bind";
		info->eventType = GF_SG_EVENT_IN;
		info->on_event_in = ((X_NavigationInfo *)node)->on_set_bind;
		info->fieldType = GF_SG_VRML_SFBOOL;
		info->far_ptr = & ((X_NavigationInfo *) node)->set_bind;
		return GF_OK;
	case 1:
		info->name = "avatarSize";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_MFFLOAT;
		info->far_ptr = & ((X_NavigationInfo *) node)->avatarSize;
		return GF_OK;
	case 2:
		info->name = "headlight";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFBOOL;
		info->far_ptr = & ((X_NavigationInfo *) node)->headlight;
		return GF_OK;
	case 3:
		info->name = "speed";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFFLOAT;
		info->far_ptr = & ((X_NavigationInfo *) node)->speed;
		return GF_OK;
	case 4:
		info->name = "type";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_MFSTRING;
		info->far_ptr = & ((X_NavigationInfo *) node)->type;
		return GF_OK;
	case 5:
		info->name = "visibilityLimit";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFFLOAT;
		info->far_ptr = & ((X_NavigationInfo *) node)->visibilityLimit;
		return GF_OK;
	case 6:
		info->name = "isBound";
		info->eventType = GF_SG_EVENT_OUT;
		info->fieldType = GF_SG_VRML_SFBOOL;
		info->far_ptr = & ((X_NavigationInfo *) node)->isBound;
		return GF_OK;
	case 7:
		info->name = "metadata";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFNODE;
		info->NDTtype = NDT_SFMetadataNode;
		info->far_ptr = & ((X_NavigationInfo *)node)->metadata;
		return GF_OK;
	case 8:
		info->name = "transitionType";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_MFSTRING;
		info->far_ptr = & ((X_NavigationInfo *) node)->transitionType;
		return GF_OK;
	case 9:
		info->name = "bindTime";
		info->eventType = GF_SG_EVENT_OUT;
		info->fieldType = GF_SG_VRML_SFTIME;
		info->far_ptr = & ((X_NavigationInfo *) node)->bindTime;
		return GF_OK;
	default:
		return GF_BAD_PARAM;
	}
}


static s32 NavigationInfo_get_field_index_by_name(char *name)
{
	if (!strcmp("set_bind", name)) return 0;
	if (!strcmp("avatarSize", name)) return 1;
	if (!strcmp("headlight", name)) return 2;
	if (!strcmp("speed", name)) return 3;
	if (!strcmp("type", name)) return 4;
	if (!strcmp("visibilityLimit", name)) return 5;
	if (!strcmp("isBound", name)) return 6;
	if (!strcmp("metadata", name)) return 7;
	if (!strcmp("transitionType", name)) return 8;
	if (!strcmp("bindTime", name)) return 9;
	return -1;
	}


static GF_Node *NavigationInfo_Create()
{
	X_NavigationInfo *p;
	GF_SAFEALLOC(p, X_NavigationInfo);
	if(!p) return NULL;
	gf_node_setup((GF_Node *)p, TAG_X3D_NavigationInfo);

	/*default field values*/
	p->avatarSize.vals = (SFFloat *)malloc(sizeof(SFFloat)*3);
	p->avatarSize.count = 3;
	p->avatarSize.vals[0] = FLT2FIX(0.25);
	p->avatarSize.vals[1] = FLT2FIX(1.6);
	p->avatarSize.vals[2] = FLT2FIX(0.75);
	p->headlight = 1;
	p->speed = FLT2FIX(1.0);
	p->type.vals = (char**)malloc(sizeof(SFString)*2);
	p->type.count = 2;
	p->type.vals[0] = (char*)malloc(sizeof(char) * 5);
	strcpy(p->type.vals[0], "WALK");
	p->type.vals[1] = (char*)malloc(sizeof(char) * 4);
	strcpy(p->type.vals[1], "ANY");
	p->visibilityLimit = FLT2FIX(0.0);
	p->transitionType.vals = (char**)malloc(sizeof(SFString)*2);
	p->transitionType.count = 2;
	p->transitionType.vals[0] = (char*)malloc(sizeof(char) * 5);
	strcpy(p->transitionType.vals[0], "WALK");
	p->transitionType.vals[1] = (char*)malloc(sizeof(char) * 4);
	strcpy(p->transitionType.vals[1], "ANY");
	return (GF_Node *)p;
}


/*
	Normal Node deletion
*/

static void Normal_Del(GF_Node *node)
{
	X_Normal *p = (X_Normal *) node;
	gf_sg_mfvec3f_del(p->vector);
	gf_node_unregister((GF_Node *) p->metadata, node);	
	gf_node_free((GF_Node *)p);
}


static u32 Normal_get_field_count(GF_Node *node, u8 dummy)
{
	return 2;
}

static GF_Err Normal_get_field(GF_Node *node, GF_FieldInfo *info)
{
	switch (info->fieldIndex) {
	case 0:
		info->name = "vector";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_MFVEC3F;
		info->far_ptr = & ((X_Normal *) node)->vector;
		return GF_OK;
	case 1:
		info->name = "metadata";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFNODE;
		info->NDTtype = NDT_SFMetadataNode;
		info->far_ptr = & ((X_Normal *)node)->metadata;
		return GF_OK;
	default:
		return GF_BAD_PARAM;
	}
}


static s32 Normal_get_field_index_by_name(char *name)
{
	if (!strcmp("vector", name)) return 0;
	if (!strcmp("metadata", name)) return 1;
	return -1;
	}


static GF_Node *Normal_Create()
{
	X_Normal *p;
	GF_SAFEALLOC(p, X_Normal);
	if(!p) return NULL;
	gf_node_setup((GF_Node *)p, TAG_X3D_Normal);

	/*default field values*/
	return (GF_Node *)p;
}


/*
	NormalInterpolator Node deletion
*/

static void NormalInterpolator_Del(GF_Node *node)
{
	X_NormalInterpolator *p = (X_NormalInterpolator *) node;
	gf_sg_mffloat_del(p->key);
	gf_sg_mfvec3f_del(p->keyValue);
	gf_sg_mfvec3f_del(p->value_changed);
	gf_node_unregister((GF_Node *) p->metadata, node);	
	gf_node_free((GF_Node *)p);
}


static u32 NormalInterpolator_get_field_count(GF_Node *node, u8 dummy)
{
	return 5;
}

static GF_Err NormalInterpolator_get_field(GF_Node *node, GF_FieldInfo *info)
{
	switch (info->fieldIndex) {
	case 0:
		info->name = "set_fraction";
		info->eventType = GF_SG_EVENT_IN;
		info->on_event_in = ((X_NormalInterpolator *)node)->on_set_fraction;
		info->fieldType = GF_SG_VRML_SFFLOAT;
		info->far_ptr = & ((X_NormalInterpolator *) node)->set_fraction;
		return GF_OK;
	case 1:
		info->name = "key";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_MFFLOAT;
		info->far_ptr = & ((X_NormalInterpolator *) node)->key;
		return GF_OK;
	case 2:
		info->name = "keyValue";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_MFVEC3F;
		info->far_ptr = & ((X_NormalInterpolator *) node)->keyValue;
		return GF_OK;
	case 3:
		info->name = "value_changed";
		info->eventType = GF_SG_EVENT_OUT;
		info->fieldType = GF_SG_VRML_MFVEC3F;
		info->far_ptr = & ((X_NormalInterpolator *) node)->value_changed;
		return GF_OK;
	case 4:
		info->name = "metadata";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFNODE;
		info->NDTtype = NDT_SFMetadataNode;
		info->far_ptr = & ((X_NormalInterpolator *)node)->metadata;
		return GF_OK;
	default:
		return GF_BAD_PARAM;
	}
}


static s32 NormalInterpolator_get_field_index_by_name(char *name)
{
	if (!strcmp("set_fraction", name)) return 0;
	if (!strcmp("key", name)) return 1;
	if (!strcmp("keyValue", name)) return 2;
	if (!strcmp("value_changed", name)) return 3;
	if (!strcmp("metadata", name)) return 4;
	return -1;
	}


static GF_Node *NormalInterpolator_Create()
{
	X_NormalInterpolator *p;
	GF_SAFEALLOC(p, X_NormalInterpolator);
	if(!p) return NULL;
	gf_node_setup((GF_Node *)p, TAG_X3D_NormalInterpolator);

	/*default field values*/
	return (GF_Node *)p;
}


/*
	OrientationInterpolator Node deletion
*/

static void OrientationInterpolator_Del(GF_Node *node)
{
	X_OrientationInterpolator *p = (X_OrientationInterpolator *) node;
	gf_sg_mffloat_del(p->key);
	gf_sg_mfrotation_del(p->keyValue);
	gf_node_unregister((GF_Node *) p->metadata, node);	
	gf_node_free((GF_Node *)p);
}


static u32 OrientationInterpolator_get_field_count(GF_Node *node, u8 dummy)
{
	return 5;
}

static GF_Err OrientationInterpolator_get_field(GF_Node *node, GF_FieldInfo *info)
{
	switch (info->fieldIndex) {
	case 0:
		info->name = "set_fraction";
		info->eventType = GF_SG_EVENT_IN;
		info->on_event_in = ((X_OrientationInterpolator *)node)->on_set_fraction;
		info->fieldType = GF_SG_VRML_SFFLOAT;
		info->far_ptr = & ((X_OrientationInterpolator *) node)->set_fraction;
		return GF_OK;
	case 1:
		info->name = "key";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_MFFLOAT;
		info->far_ptr = & ((X_OrientationInterpolator *) node)->key;
		return GF_OK;
	case 2:
		info->name = "keyValue";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_MFROTATION;
		info->far_ptr = & ((X_OrientationInterpolator *) node)->keyValue;
		return GF_OK;
	case 3:
		info->name = "value_changed";
		info->eventType = GF_SG_EVENT_OUT;
		info->fieldType = GF_SG_VRML_SFROTATION;
		info->far_ptr = & ((X_OrientationInterpolator *) node)->value_changed;
		return GF_OK;
	case 4:
		info->name = "metadata";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFNODE;
		info->NDTtype = NDT_SFMetadataNode;
		info->far_ptr = & ((X_OrientationInterpolator *)node)->metadata;
		return GF_OK;
	default:
		return GF_BAD_PARAM;
	}
}


static s32 OrientationInterpolator_get_field_index_by_name(char *name)
{
	if (!strcmp("set_fraction", name)) return 0;
	if (!strcmp("key", name)) return 1;
	if (!strcmp("keyValue", name)) return 2;
	if (!strcmp("value_changed", name)) return 3;
	if (!strcmp("metadata", name)) return 4;
	return -1;
	}


static GF_Node *OrientationInterpolator_Create()
{
	X_OrientationInterpolator *p;
	GF_SAFEALLOC(p, X_OrientationInterpolator);
	if(!p) return NULL;
	gf_node_setup((GF_Node *)p, TAG_X3D_OrientationInterpolator);

	/*default field values*/
	return (GF_Node *)p;
}


/*
	PixelTexture Node deletion
*/

static void PixelTexture_Del(GF_Node *node)
{
	X_PixelTexture *p = (X_PixelTexture *) node;
	gf_sg_sfimage_del(p->image);
	gf_node_unregister((GF_Node *) p->metadata, node);	
	gf_node_free((GF_Node *)p);
}


static u32 PixelTexture_get_field_count(GF_Node *node, u8 dummy)
{
	return 4;
}

static GF_Err PixelTexture_get_field(GF_Node *node, GF_FieldInfo *info)
{
	switch (info->fieldIndex) {
	case 0:
		info->name = "image";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFIMAGE;
		info->far_ptr = & ((X_PixelTexture *) node)->image;
		return GF_OK;
	case 1:
		info->name = "repeatS";
		info->eventType = GF_SG_EVENT_FIELD;
		info->fieldType = GF_SG_VRML_SFBOOL;
		info->far_ptr = & ((X_PixelTexture *) node)->repeatS;
		return GF_OK;
	case 2:
		info->name = "repeatT";
		info->eventType = GF_SG_EVENT_FIELD;
		info->fieldType = GF_SG_VRML_SFBOOL;
		info->far_ptr = & ((X_PixelTexture *) node)->repeatT;
		return GF_OK;
	case 3:
		info->name = "metadata";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFNODE;
		info->NDTtype = NDT_SFMetadataNode;
		info->far_ptr = & ((X_PixelTexture *)node)->metadata;
		return GF_OK;
	default:
		return GF_BAD_PARAM;
	}
}


static s32 PixelTexture_get_field_index_by_name(char *name)
{
	if (!strcmp("image", name)) return 0;
	if (!strcmp("repeatS", name)) return 1;
	if (!strcmp("repeatT", name)) return 2;
	if (!strcmp("metadata", name)) return 3;
	return -1;
	}


static GF_Node *PixelTexture_Create()
{
	X_PixelTexture *p;
	GF_SAFEALLOC(p, X_PixelTexture);
	if(!p) return NULL;
	gf_node_setup((GF_Node *)p, TAG_X3D_PixelTexture);

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
	X_PlaneSensor *p = (X_PlaneSensor *) node;
	gf_node_unregister((GF_Node *) p->metadata, node);	
	gf_sg_sfstring_del(p->description);
	gf_node_free((GF_Node *)p);
}


static u32 PlaneSensor_get_field_count(GF_Node *node, u8 dummy)
{
	return 11;
}

static GF_Err PlaneSensor_get_field(GF_Node *node, GF_FieldInfo *info)
{
	switch (info->fieldIndex) {
	case 0:
		info->name = "autoOffset";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFBOOL;
		info->far_ptr = & ((X_PlaneSensor *) node)->autoOffset;
		return GF_OK;
	case 1:
		info->name = "enabled";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFBOOL;
		info->far_ptr = & ((X_PlaneSensor *) node)->enabled;
		return GF_OK;
	case 2:
		info->name = "maxPosition";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFVEC2F;
		info->far_ptr = & ((X_PlaneSensor *) node)->maxPosition;
		return GF_OK;
	case 3:
		info->name = "minPosition";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFVEC2F;
		info->far_ptr = & ((X_PlaneSensor *) node)->minPosition;
		return GF_OK;
	case 4:
		info->name = "offset";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFVEC3F;
		info->far_ptr = & ((X_PlaneSensor *) node)->offset;
		return GF_OK;
	case 5:
		info->name = "isActive";
		info->eventType = GF_SG_EVENT_OUT;
		info->fieldType = GF_SG_VRML_SFBOOL;
		info->far_ptr = & ((X_PlaneSensor *) node)->isActive;
		return GF_OK;
	case 6:
		info->name = "trackPoint_changed";
		info->eventType = GF_SG_EVENT_OUT;
		info->fieldType = GF_SG_VRML_SFVEC3F;
		info->far_ptr = & ((X_PlaneSensor *) node)->trackPoint_changed;
		return GF_OK;
	case 7:
		info->name = "translation_changed";
		info->eventType = GF_SG_EVENT_OUT;
		info->fieldType = GF_SG_VRML_SFVEC3F;
		info->far_ptr = & ((X_PlaneSensor *) node)->translation_changed;
		return GF_OK;
	case 8:
		info->name = "metadata";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFNODE;
		info->NDTtype = NDT_SFMetadataNode;
		info->far_ptr = & ((X_PlaneSensor *)node)->metadata;
		return GF_OK;
	case 9:
		info->name = "description";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFSTRING;
		info->far_ptr = & ((X_PlaneSensor *) node)->description;
		return GF_OK;
	case 10:
		info->name = "isOver";
		info->eventType = GF_SG_EVENT_OUT;
		info->fieldType = GF_SG_VRML_SFBOOL;
		info->far_ptr = & ((X_PlaneSensor *) node)->isOver;
		return GF_OK;
	default:
		return GF_BAD_PARAM;
	}
}


static s32 PlaneSensor_get_field_index_by_name(char *name)
{
	if (!strcmp("autoOffset", name)) return 0;
	if (!strcmp("enabled", name)) return 1;
	if (!strcmp("maxPosition", name)) return 2;
	if (!strcmp("minPosition", name)) return 3;
	if (!strcmp("offset", name)) return 4;
	if (!strcmp("isActive", name)) return 5;
	if (!strcmp("trackPoint_changed", name)) return 6;
	if (!strcmp("translation_changed", name)) return 7;
	if (!strcmp("metadata", name)) return 8;
	if (!strcmp("description", name)) return 9;
	if (!strcmp("isOver", name)) return 10;
	return -1;
	}


static GF_Node *PlaneSensor_Create()
{
	X_PlaneSensor *p;
	GF_SAFEALLOC(p, X_PlaneSensor);
	if(!p) return NULL;
	gf_node_setup((GF_Node *)p, TAG_X3D_PlaneSensor);

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
	PointLight Node deletion
*/

static void PointLight_Del(GF_Node *node)
{
	X_PointLight *p = (X_PointLight *) node;
	gf_node_unregister((GF_Node *) p->metadata, node);	
	gf_node_free((GF_Node *)p);
}


static u32 PointLight_get_field_count(GF_Node *node, u8 dummy)
{
	return 8;
}

static GF_Err PointLight_get_field(GF_Node *node, GF_FieldInfo *info)
{
	switch (info->fieldIndex) {
	case 0:
		info->name = "ambientIntensity";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFFLOAT;
		info->far_ptr = & ((X_PointLight *) node)->ambientIntensity;
		return GF_OK;
	case 1:
		info->name = "attenuation";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFVEC3F;
		info->far_ptr = & ((X_PointLight *) node)->attenuation;
		return GF_OK;
	case 2:
		info->name = "color";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFCOLOR;
		info->far_ptr = & ((X_PointLight *) node)->color;
		return GF_OK;
	case 3:
		info->name = "intensity";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFFLOAT;
		info->far_ptr = & ((X_PointLight *) node)->intensity;
		return GF_OK;
	case 4:
		info->name = "location";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFVEC3F;
		info->far_ptr = & ((X_PointLight *) node)->location;
		return GF_OK;
	case 5:
		info->name = "on";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFBOOL;
		info->far_ptr = & ((X_PointLight *) node)->on;
		return GF_OK;
	case 6:
		info->name = "radius";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFFLOAT;
		info->far_ptr = & ((X_PointLight *) node)->radius;
		return GF_OK;
	case 7:
		info->name = "metadata";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFNODE;
		info->NDTtype = NDT_SFMetadataNode;
		info->far_ptr = & ((X_PointLight *)node)->metadata;
		return GF_OK;
	default:
		return GF_BAD_PARAM;
	}
}


static s32 PointLight_get_field_index_by_name(char *name)
{
	if (!strcmp("ambientIntensity", name)) return 0;
	if (!strcmp("attenuation", name)) return 1;
	if (!strcmp("color", name)) return 2;
	if (!strcmp("intensity", name)) return 3;
	if (!strcmp("location", name)) return 4;
	if (!strcmp("on", name)) return 5;
	if (!strcmp("radius", name)) return 6;
	if (!strcmp("metadata", name)) return 7;
	return -1;
	}


static GF_Node *PointLight_Create()
{
	X_PointLight *p;
	GF_SAFEALLOC(p, X_PointLight);
	if(!p) return NULL;
	gf_node_setup((GF_Node *)p, TAG_X3D_PointLight);

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
	X_PointSet *p = (X_PointSet *) node;
	gf_node_unregister((GF_Node *) p->color, node);	
	gf_node_unregister((GF_Node *) p->coord, node);	
	gf_node_unregister((GF_Node *) p->metadata, node);	
	gf_node_free((GF_Node *)p);
}


static u32 PointSet_get_field_count(GF_Node *node, u8 dummy)
{
	return 3;
}

static GF_Err PointSet_get_field(GF_Node *node, GF_FieldInfo *info)
{
	switch (info->fieldIndex) {
	case 0:
		info->name = "color";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFNODE;
		info->NDTtype = NDT_SFColorNode;
		info->far_ptr = & ((X_PointSet *)node)->color;
		return GF_OK;
	case 1:
		info->name = "coord";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFNODE;
		info->NDTtype = NDT_SFCoordinateNode;
		info->far_ptr = & ((X_PointSet *)node)->coord;
		return GF_OK;
	case 2:
		info->name = "metadata";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFNODE;
		info->NDTtype = NDT_SFMetadataNode;
		info->far_ptr = & ((X_PointSet *)node)->metadata;
		return GF_OK;
	default:
		return GF_BAD_PARAM;
	}
}


static s32 PointSet_get_field_index_by_name(char *name)
{
	if (!strcmp("color", name)) return 0;
	if (!strcmp("coord", name)) return 1;
	if (!strcmp("metadata", name)) return 2;
	return -1;
	}


static GF_Node *PointSet_Create()
{
	X_PointSet *p;
	GF_SAFEALLOC(p, X_PointSet);
	if(!p) return NULL;
	gf_node_setup((GF_Node *)p, TAG_X3D_PointSet);

	/*default field values*/
	return (GF_Node *)p;
}


/*
	Polyline2D Node deletion
*/

static void Polyline2D_Del(GF_Node *node)
{
	X_Polyline2D *p = (X_Polyline2D *) node;
	gf_sg_mfvec2f_del(p->lineSegments);
	gf_node_unregister((GF_Node *) p->metadata, node);	
	gf_node_free((GF_Node *)p);
}


static u32 Polyline2D_get_field_count(GF_Node *node, u8 dummy)
{
	return 2;
}

static GF_Err Polyline2D_get_field(GF_Node *node, GF_FieldInfo *info)
{
	switch (info->fieldIndex) {
	case 0:
		info->name = "lineSegments";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_MFVEC2F;
		info->far_ptr = & ((X_Polyline2D *) node)->lineSegments;
		return GF_OK;
	case 1:
		info->name = "metadata";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFNODE;
		info->NDTtype = NDT_SFMetadataNode;
		info->far_ptr = & ((X_Polyline2D *)node)->metadata;
		return GF_OK;
	default:
		return GF_BAD_PARAM;
	}
}


static s32 Polyline2D_get_field_index_by_name(char *name)
{
	if (!strcmp("lineSegments", name)) return 0;
	if (!strcmp("metadata", name)) return 1;
	return -1;
	}


static GF_Node *Polyline2D_Create()
{
	X_Polyline2D *p;
	GF_SAFEALLOC(p, X_Polyline2D);
	if(!p) return NULL;
	gf_node_setup((GF_Node *)p, TAG_X3D_Polyline2D);

	/*default field values*/
	return (GF_Node *)p;
}


/*
	Polypoint2D Node deletion
*/

static void Polypoint2D_Del(GF_Node *node)
{
	X_Polypoint2D *p = (X_Polypoint2D *) node;
	gf_sg_mfvec2f_del(p->point);
	gf_node_unregister((GF_Node *) p->metadata, node);	
	gf_node_free((GF_Node *)p);
}


static u32 Polypoint2D_get_field_count(GF_Node *node, u8 dummy)
{
	return 2;
}

static GF_Err Polypoint2D_get_field(GF_Node *node, GF_FieldInfo *info)
{
	switch (info->fieldIndex) {
	case 0:
		info->name = "point";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_MFVEC2F;
		info->far_ptr = & ((X_Polypoint2D *) node)->point;
		return GF_OK;
	case 1:
		info->name = "metadata";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFNODE;
		info->NDTtype = NDT_SFMetadataNode;
		info->far_ptr = & ((X_Polypoint2D *)node)->metadata;
		return GF_OK;
	default:
		return GF_BAD_PARAM;
	}
}


static s32 Polypoint2D_get_field_index_by_name(char *name)
{
	if (!strcmp("point", name)) return 0;
	if (!strcmp("metadata", name)) return 1;
	return -1;
	}


static GF_Node *Polypoint2D_Create()
{
	X_Polypoint2D *p;
	GF_SAFEALLOC(p, X_Polypoint2D);
	if(!p) return NULL;
	gf_node_setup((GF_Node *)p, TAG_X3D_Polypoint2D);

	/*default field values*/
	return (GF_Node *)p;
}


/*
	PositionInterpolator Node deletion
*/

static void PositionInterpolator_Del(GF_Node *node)
{
	X_PositionInterpolator *p = (X_PositionInterpolator *) node;
	gf_sg_mffloat_del(p->key);
	gf_sg_mfvec3f_del(p->keyValue);
	gf_node_unregister((GF_Node *) p->metadata, node);	
	gf_node_free((GF_Node *)p);
}


static u32 PositionInterpolator_get_field_count(GF_Node *node, u8 dummy)
{
	return 5;
}

static GF_Err PositionInterpolator_get_field(GF_Node *node, GF_FieldInfo *info)
{
	switch (info->fieldIndex) {
	case 0:
		info->name = "set_fraction";
		info->eventType = GF_SG_EVENT_IN;
		info->on_event_in = ((X_PositionInterpolator *)node)->on_set_fraction;
		info->fieldType = GF_SG_VRML_SFFLOAT;
		info->far_ptr = & ((X_PositionInterpolator *) node)->set_fraction;
		return GF_OK;
	case 1:
		info->name = "key";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_MFFLOAT;
		info->far_ptr = & ((X_PositionInterpolator *) node)->key;
		return GF_OK;
	case 2:
		info->name = "keyValue";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_MFVEC3F;
		info->far_ptr = & ((X_PositionInterpolator *) node)->keyValue;
		return GF_OK;
	case 3:
		info->name = "value_changed";
		info->eventType = GF_SG_EVENT_OUT;
		info->fieldType = GF_SG_VRML_SFVEC3F;
		info->far_ptr = & ((X_PositionInterpolator *) node)->value_changed;
		return GF_OK;
	case 4:
		info->name = "metadata";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFNODE;
		info->NDTtype = NDT_SFMetadataNode;
		info->far_ptr = & ((X_PositionInterpolator *)node)->metadata;
		return GF_OK;
	default:
		return GF_BAD_PARAM;
	}
}


static s32 PositionInterpolator_get_field_index_by_name(char *name)
{
	if (!strcmp("set_fraction", name)) return 0;
	if (!strcmp("key", name)) return 1;
	if (!strcmp("keyValue", name)) return 2;
	if (!strcmp("value_changed", name)) return 3;
	if (!strcmp("metadata", name)) return 4;
	return -1;
	}


static GF_Node *PositionInterpolator_Create()
{
	X_PositionInterpolator *p;
	GF_SAFEALLOC(p, X_PositionInterpolator);
	if(!p) return NULL;
	gf_node_setup((GF_Node *)p, TAG_X3D_PositionInterpolator);

	/*default field values*/
	return (GF_Node *)p;
}


/*
	PositionInterpolator2D Node deletion
*/

static void PositionInterpolator2D_Del(GF_Node *node)
{
	X_PositionInterpolator2D *p = (X_PositionInterpolator2D *) node;
	gf_sg_mffloat_del(p->key);
	gf_sg_mfvec2f_del(p->keyValue);
	gf_node_unregister((GF_Node *) p->metadata, node);	
	gf_node_free((GF_Node *)p);
}


static u32 PositionInterpolator2D_get_field_count(GF_Node *node, u8 dummy)
{
	return 5;
}

static GF_Err PositionInterpolator2D_get_field(GF_Node *node, GF_FieldInfo *info)
{
	switch (info->fieldIndex) {
	case 0:
		info->name = "set_fraction";
		info->eventType = GF_SG_EVENT_IN;
		info->on_event_in = ((X_PositionInterpolator2D *)node)->on_set_fraction;
		info->fieldType = GF_SG_VRML_SFFLOAT;
		info->far_ptr = & ((X_PositionInterpolator2D *) node)->set_fraction;
		return GF_OK;
	case 1:
		info->name = "key";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_MFFLOAT;
		info->far_ptr = & ((X_PositionInterpolator2D *) node)->key;
		return GF_OK;
	case 2:
		info->name = "keyValue";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_MFVEC2F;
		info->far_ptr = & ((X_PositionInterpolator2D *) node)->keyValue;
		return GF_OK;
	case 3:
		info->name = "value_changed";
		info->eventType = GF_SG_EVENT_OUT;
		info->fieldType = GF_SG_VRML_SFVEC2F;
		info->far_ptr = & ((X_PositionInterpolator2D *) node)->value_changed;
		return GF_OK;
	case 4:
		info->name = "metadata";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFNODE;
		info->NDTtype = NDT_SFMetadataNode;
		info->far_ptr = & ((X_PositionInterpolator2D *)node)->metadata;
		return GF_OK;
	default:
		return GF_BAD_PARAM;
	}
}


static s32 PositionInterpolator2D_get_field_index_by_name(char *name)
{
	if (!strcmp("set_fraction", name)) return 0;
	if (!strcmp("key", name)) return 1;
	if (!strcmp("keyValue", name)) return 2;
	if (!strcmp("value_changed", name)) return 3;
	if (!strcmp("metadata", name)) return 4;
	return -1;
	}


static GF_Node *PositionInterpolator2D_Create()
{
	X_PositionInterpolator2D *p;
	GF_SAFEALLOC(p, X_PositionInterpolator2D);
	if(!p) return NULL;
	gf_node_setup((GF_Node *)p, TAG_X3D_PositionInterpolator2D);

	/*default field values*/
	return (GF_Node *)p;
}


/*
	ProximitySensor Node deletion
*/

static void ProximitySensor_Del(GF_Node *node)
{
	X_ProximitySensor *p = (X_ProximitySensor *) node;
	gf_node_unregister((GF_Node *) p->metadata, node);	
	gf_node_free((GF_Node *)p);
}


static u32 ProximitySensor_get_field_count(GF_Node *node, u8 dummy)
{
	return 10;
}

static GF_Err ProximitySensor_get_field(GF_Node *node, GF_FieldInfo *info)
{
	switch (info->fieldIndex) {
	case 0:
		info->name = "center";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFVEC3F;
		info->far_ptr = & ((X_ProximitySensor *) node)->center;
		return GF_OK;
	case 1:
		info->name = "size";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFVEC3F;
		info->far_ptr = & ((X_ProximitySensor *) node)->size;
		return GF_OK;
	case 2:
		info->name = "enabled";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFBOOL;
		info->far_ptr = & ((X_ProximitySensor *) node)->enabled;
		return GF_OK;
	case 3:
		info->name = "isActive";
		info->eventType = GF_SG_EVENT_OUT;
		info->fieldType = GF_SG_VRML_SFBOOL;
		info->far_ptr = & ((X_ProximitySensor *) node)->isActive;
		return GF_OK;
	case 4:
		info->name = "position_changed";
		info->eventType = GF_SG_EVENT_OUT;
		info->fieldType = GF_SG_VRML_SFVEC3F;
		info->far_ptr = & ((X_ProximitySensor *) node)->position_changed;
		return GF_OK;
	case 5:
		info->name = "orientation_changed";
		info->eventType = GF_SG_EVENT_OUT;
		info->fieldType = GF_SG_VRML_SFROTATION;
		info->far_ptr = & ((X_ProximitySensor *) node)->orientation_changed;
		return GF_OK;
	case 6:
		info->name = "enterTime";
		info->eventType = GF_SG_EVENT_OUT;
		info->fieldType = GF_SG_VRML_SFTIME;
		info->far_ptr = & ((X_ProximitySensor *) node)->enterTime;
		return GF_OK;
	case 7:
		info->name = "exitTime";
		info->eventType = GF_SG_EVENT_OUT;
		info->fieldType = GF_SG_VRML_SFTIME;
		info->far_ptr = & ((X_ProximitySensor *) node)->exitTime;
		return GF_OK;
	case 8:
		info->name = "metadata";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFNODE;
		info->NDTtype = NDT_SFMetadataNode;
		info->far_ptr = & ((X_ProximitySensor *)node)->metadata;
		return GF_OK;
	case 9:
		info->name = "centerOfRotation_changed";
		info->eventType = GF_SG_EVENT_OUT;
		info->fieldType = GF_SG_VRML_SFVEC3F;
		info->far_ptr = & ((X_ProximitySensor *) node)->centerOfRotation_changed;
		return GF_OK;
	default:
		return GF_BAD_PARAM;
	}
}


static s32 ProximitySensor_get_field_index_by_name(char *name)
{
	if (!strcmp("center", name)) return 0;
	if (!strcmp("size", name)) return 1;
	if (!strcmp("enabled", name)) return 2;
	if (!strcmp("isActive", name)) return 3;
	if (!strcmp("position_changed", name)) return 4;
	if (!strcmp("orientation_changed", name)) return 5;
	if (!strcmp("enterTime", name)) return 6;
	if (!strcmp("exitTime", name)) return 7;
	if (!strcmp("metadata", name)) return 8;
	if (!strcmp("centerOfRotation_changed", name)) return 9;
	return -1;
	}


static GF_Node *ProximitySensor_Create()
{
	X_ProximitySensor *p;
	GF_SAFEALLOC(p, X_ProximitySensor);
	if(!p) return NULL;
	gf_node_setup((GF_Node *)p, TAG_X3D_ProximitySensor);

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
	Rectangle2D Node deletion
*/

static void Rectangle2D_Del(GF_Node *node)
{
	X_Rectangle2D *p = (X_Rectangle2D *) node;
	gf_node_unregister((GF_Node *) p->metadata, node);	
	gf_node_free((GF_Node *)p);
}


static u32 Rectangle2D_get_field_count(GF_Node *node, u8 dummy)
{
	return 2;
}

static GF_Err Rectangle2D_get_field(GF_Node *node, GF_FieldInfo *info)
{
	switch (info->fieldIndex) {
	case 0:
		info->name = "size";
		info->eventType = GF_SG_EVENT_FIELD;
		info->fieldType = GF_SG_VRML_SFVEC2F;
		info->far_ptr = & ((X_Rectangle2D *) node)->size;
		return GF_OK;
	case 1:
		info->name = "metadata";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFNODE;
		info->NDTtype = NDT_SFMetadataNode;
		info->far_ptr = & ((X_Rectangle2D *)node)->metadata;
		return GF_OK;
	default:
		return GF_BAD_PARAM;
	}
}


static s32 Rectangle2D_get_field_index_by_name(char *name)
{
	if (!strcmp("size", name)) return 0;
	if (!strcmp("metadata", name)) return 1;
	return -1;
	}


static GF_Node *Rectangle2D_Create()
{
	X_Rectangle2D *p;
	GF_SAFEALLOC(p, X_Rectangle2D);
	if(!p) return NULL;
	gf_node_setup((GF_Node *)p, TAG_X3D_Rectangle2D);

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
	X_ScalarInterpolator *p = (X_ScalarInterpolator *) node;
	gf_sg_mffloat_del(p->key);
	gf_sg_mffloat_del(p->keyValue);
	gf_node_unregister((GF_Node *) p->metadata, node);	
	gf_node_free((GF_Node *)p);
}


static u32 ScalarInterpolator_get_field_count(GF_Node *node, u8 dummy)
{
	return 5;
}

static GF_Err ScalarInterpolator_get_field(GF_Node *node, GF_FieldInfo *info)
{
	switch (info->fieldIndex) {
	case 0:
		info->name = "set_fraction";
		info->eventType = GF_SG_EVENT_IN;
		info->on_event_in = ((X_ScalarInterpolator *)node)->on_set_fraction;
		info->fieldType = GF_SG_VRML_SFFLOAT;
		info->far_ptr = & ((X_ScalarInterpolator *) node)->set_fraction;
		return GF_OK;
	case 1:
		info->name = "key";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_MFFLOAT;
		info->far_ptr = & ((X_ScalarInterpolator *) node)->key;
		return GF_OK;
	case 2:
		info->name = "keyValue";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_MFFLOAT;
		info->far_ptr = & ((X_ScalarInterpolator *) node)->keyValue;
		return GF_OK;
	case 3:
		info->name = "value_changed";
		info->eventType = GF_SG_EVENT_OUT;
		info->fieldType = GF_SG_VRML_SFFLOAT;
		info->far_ptr = & ((X_ScalarInterpolator *) node)->value_changed;
		return GF_OK;
	case 4:
		info->name = "metadata";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFNODE;
		info->NDTtype = NDT_SFMetadataNode;
		info->far_ptr = & ((X_ScalarInterpolator *)node)->metadata;
		return GF_OK;
	default:
		return GF_BAD_PARAM;
	}
}


static s32 ScalarInterpolator_get_field_index_by_name(char *name)
{
	if (!strcmp("set_fraction", name)) return 0;
	if (!strcmp("key", name)) return 1;
	if (!strcmp("keyValue", name)) return 2;
	if (!strcmp("value_changed", name)) return 3;
	if (!strcmp("metadata", name)) return 4;
	return -1;
	}


static GF_Node *ScalarInterpolator_Create()
{
	X_ScalarInterpolator *p;
	GF_SAFEALLOC(p, X_ScalarInterpolator);
	if(!p) return NULL;
	gf_node_setup((GF_Node *)p, TAG_X3D_ScalarInterpolator);

	/*default field values*/
	return (GF_Node *)p;
}


/*
	Script Node deletion
*/

static void Script_Del(GF_Node *node)
{
	X_Script *p = (X_Script *) node;
	gf_sg_mfscript_del(p->url);
	gf_node_unregister((GF_Node *) p->metadata, node);	
	gf_node_free((GF_Node *)p);
}


static u32 Script_get_field_count(GF_Node *node, u8 dummy)
{
	return 4;
}

static GF_Err Script_get_field(GF_Node *node, GF_FieldInfo *info)
{
	switch (info->fieldIndex) {
	case 0:
		info->name = "url";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_MFSCRIPT;
		info->far_ptr = & ((X_Script *) node)->url;
		return GF_OK;
	case 1:
		info->name = "directOutput";
		info->eventType = GF_SG_EVENT_FIELD;
		info->fieldType = GF_SG_VRML_SFBOOL;
		info->far_ptr = & ((X_Script *) node)->directOutput;
		return GF_OK;
	case 2:
		info->name = "mustEvaluate";
		info->eventType = GF_SG_EVENT_FIELD;
		info->fieldType = GF_SG_VRML_SFBOOL;
		info->far_ptr = & ((X_Script *) node)->mustEvaluate;
		return GF_OK;
	case 3:
		info->name = "metadata";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFNODE;
		info->NDTtype = NDT_SFMetadataNode;
		info->far_ptr = & ((X_Script *)node)->metadata;
		return GF_OK;
	default:
		return GF_BAD_PARAM;
	}
}


static s32 Script_get_field_index_by_name(char *name)
{
	if (!strcmp("url", name)) return 0;
	if (!strcmp("directOutput", name)) return 1;
	if (!strcmp("mustEvaluate", name)) return 2;
	if (!strcmp("metadata", name)) return 3;
	return -1;
	}


static GF_Node *Script_Create()
{
	X_Script *p;
	GF_SAFEALLOC(p, X_Script);
	if(!p) return NULL;
	gf_node_setup((GF_Node *)p, TAG_X3D_Script);

	/*default field values*/
	return (GF_Node *)p;
}


/*
	Shape Node deletion
*/

static void Shape_Del(GF_Node *node)
{
	X_Shape *p = (X_Shape *) node;
	gf_node_unregister((GF_Node *) p->appearance, node);	
	gf_node_unregister((GF_Node *) p->geometry, node);	
	gf_node_unregister((GF_Node *) p->metadata, node);	
	gf_node_free((GF_Node *)p);
}


static u32 Shape_get_field_count(GF_Node *node, u8 dummy)
{
	return 3;
}

static GF_Err Shape_get_field(GF_Node *node, GF_FieldInfo *info)
{
	switch (info->fieldIndex) {
	case 0:
		info->name = "appearance";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFNODE;
		info->NDTtype = NDT_SFAppearanceNode;
		info->far_ptr = & ((X_Shape *)node)->appearance;
		return GF_OK;
	case 1:
		info->name = "geometry";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFNODE;
		info->NDTtype = NDT_SFGeometryNode;
		info->far_ptr = & ((X_Shape *)node)->geometry;
		return GF_OK;
	case 2:
		info->name = "metadata";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFNODE;
		info->NDTtype = NDT_SFMetadataNode;
		info->far_ptr = & ((X_Shape *)node)->metadata;
		return GF_OK;
	default:
		return GF_BAD_PARAM;
	}
}


static s32 Shape_get_field_index_by_name(char *name)
{
	if (!strcmp("appearance", name)) return 0;
	if (!strcmp("geometry", name)) return 1;
	if (!strcmp("metadata", name)) return 2;
	return -1;
	}


static GF_Node *Shape_Create()
{
	X_Shape *p;
	GF_SAFEALLOC(p, X_Shape);
	if(!p) return NULL;
	gf_node_setup((GF_Node *)p, TAG_X3D_Shape);

	/*default field values*/
	return (GF_Node *)p;
}


/*
	Sound Node deletion
*/

static void Sound_Del(GF_Node *node)
{
	X_Sound *p = (X_Sound *) node;
	gf_node_unregister((GF_Node *) p->source, node);	
	gf_node_unregister((GF_Node *) p->metadata, node);	
	gf_node_free((GF_Node *)p);
}


static u32 Sound_get_field_count(GF_Node *node, u8 dummy)
{
	return 11;
}

static GF_Err Sound_get_field(GF_Node *node, GF_FieldInfo *info)
{
	switch (info->fieldIndex) {
	case 0:
		info->name = "direction";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFVEC3F;
		info->far_ptr = & ((X_Sound *) node)->direction;
		return GF_OK;
	case 1:
		info->name = "intensity";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFFLOAT;
		info->far_ptr = & ((X_Sound *) node)->intensity;
		return GF_OK;
	case 2:
		info->name = "location";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFVEC3F;
		info->far_ptr = & ((X_Sound *) node)->location;
		return GF_OK;
	case 3:
		info->name = "maxBack";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFFLOAT;
		info->far_ptr = & ((X_Sound *) node)->maxBack;
		return GF_OK;
	case 4:
		info->name = "maxFront";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFFLOAT;
		info->far_ptr = & ((X_Sound *) node)->maxFront;
		return GF_OK;
	case 5:
		info->name = "minBack";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFFLOAT;
		info->far_ptr = & ((X_Sound *) node)->minBack;
		return GF_OK;
	case 6:
		info->name = "minFront";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFFLOAT;
		info->far_ptr = & ((X_Sound *) node)->minFront;
		return GF_OK;
	case 7:
		info->name = "priority";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFFLOAT;
		info->far_ptr = & ((X_Sound *) node)->priority;
		return GF_OK;
	case 8:
		info->name = "source";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFNODE;
		info->NDTtype = NDT_SFAudioNode;
		info->far_ptr = & ((X_Sound *)node)->source;
		return GF_OK;
	case 9:
		info->name = "spatialize";
		info->eventType = GF_SG_EVENT_FIELD;
		info->fieldType = GF_SG_VRML_SFBOOL;
		info->far_ptr = & ((X_Sound *) node)->spatialize;
		return GF_OK;
	case 10:
		info->name = "metadata";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFNODE;
		info->NDTtype = NDT_SFMetadataNode;
		info->far_ptr = & ((X_Sound *)node)->metadata;
		return GF_OK;
	default:
		return GF_BAD_PARAM;
	}
}


static s32 Sound_get_field_index_by_name(char *name)
{
	if (!strcmp("direction", name)) return 0;
	if (!strcmp("intensity", name)) return 1;
	if (!strcmp("location", name)) return 2;
	if (!strcmp("maxBack", name)) return 3;
	if (!strcmp("maxFront", name)) return 4;
	if (!strcmp("minBack", name)) return 5;
	if (!strcmp("minFront", name)) return 6;
	if (!strcmp("priority", name)) return 7;
	if (!strcmp("source", name)) return 8;
	if (!strcmp("spatialize", name)) return 9;
	if (!strcmp("metadata", name)) return 10;
	return -1;
	}


static GF_Node *Sound_Create()
{
	X_Sound *p;
	GF_SAFEALLOC(p, X_Sound);
	if(!p) return NULL;
	gf_node_setup((GF_Node *)p, TAG_X3D_Sound);

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
	Sphere Node deletion
*/

static void Sphere_Del(GF_Node *node)
{
	X_Sphere *p = (X_Sphere *) node;
	gf_node_unregister((GF_Node *) p->metadata, node);	
	gf_node_free((GF_Node *)p);
}


static u32 Sphere_get_field_count(GF_Node *node, u8 dummy)
{
	return 2;
}

static GF_Err Sphere_get_field(GF_Node *node, GF_FieldInfo *info)
{
	switch (info->fieldIndex) {
	case 0:
		info->name = "radius";
		info->eventType = GF_SG_EVENT_FIELD;
		info->fieldType = GF_SG_VRML_SFFLOAT;
		info->far_ptr = & ((X_Sphere *) node)->radius;
		return GF_OK;
	case 1:
		info->name = "metadata";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFNODE;
		info->NDTtype = NDT_SFMetadataNode;
		info->far_ptr = & ((X_Sphere *)node)->metadata;
		return GF_OK;
	default:
		return GF_BAD_PARAM;
	}
}


static s32 Sphere_get_field_index_by_name(char *name)
{
	if (!strcmp("radius", name)) return 0;
	if (!strcmp("metadata", name)) return 1;
	return -1;
	}


static GF_Node *Sphere_Create()
{
	X_Sphere *p;
	GF_SAFEALLOC(p, X_Sphere);
	if(!p) return NULL;
	gf_node_setup((GF_Node *)p, TAG_X3D_Sphere);

	/*default field values*/
	p->radius = FLT2FIX(1);
	return (GF_Node *)p;
}


/*
	SphereSensor Node deletion
*/

static void SphereSensor_Del(GF_Node *node)
{
	X_SphereSensor *p = (X_SphereSensor *) node;
	gf_node_unregister((GF_Node *) p->metadata, node);	
	gf_sg_sfstring_del(p->description);
	gf_node_free((GF_Node *)p);
}


static u32 SphereSensor_get_field_count(GF_Node *node, u8 dummy)
{
	return 9;
}

static GF_Err SphereSensor_get_field(GF_Node *node, GF_FieldInfo *info)
{
	switch (info->fieldIndex) {
	case 0:
		info->name = "autoOffset";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFBOOL;
		info->far_ptr = & ((X_SphereSensor *) node)->autoOffset;
		return GF_OK;
	case 1:
		info->name = "enabled";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFBOOL;
		info->far_ptr = & ((X_SphereSensor *) node)->enabled;
		return GF_OK;
	case 2:
		info->name = "offset";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFROTATION;
		info->far_ptr = & ((X_SphereSensor *) node)->offset;
		return GF_OK;
	case 3:
		info->name = "isActive";
		info->eventType = GF_SG_EVENT_OUT;
		info->fieldType = GF_SG_VRML_SFBOOL;
		info->far_ptr = & ((X_SphereSensor *) node)->isActive;
		return GF_OK;
	case 4:
		info->name = "rotation_changed";
		info->eventType = GF_SG_EVENT_OUT;
		info->fieldType = GF_SG_VRML_SFROTATION;
		info->far_ptr = & ((X_SphereSensor *) node)->rotation_changed;
		return GF_OK;
	case 5:
		info->name = "trackPoint_changed";
		info->eventType = GF_SG_EVENT_OUT;
		info->fieldType = GF_SG_VRML_SFVEC3F;
		info->far_ptr = & ((X_SphereSensor *) node)->trackPoint_changed;
		return GF_OK;
	case 6:
		info->name = "metadata";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFNODE;
		info->NDTtype = NDT_SFMetadataNode;
		info->far_ptr = & ((X_SphereSensor *)node)->metadata;
		return GF_OK;
	case 7:
		info->name = "description";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFSTRING;
		info->far_ptr = & ((X_SphereSensor *) node)->description;
		return GF_OK;
	case 8:
		info->name = "isOver";
		info->eventType = GF_SG_EVENT_OUT;
		info->fieldType = GF_SG_VRML_SFBOOL;
		info->far_ptr = & ((X_SphereSensor *) node)->isOver;
		return GF_OK;
	default:
		return GF_BAD_PARAM;
	}
}


static s32 SphereSensor_get_field_index_by_name(char *name)
{
	if (!strcmp("autoOffset", name)) return 0;
	if (!strcmp("enabled", name)) return 1;
	if (!strcmp("offset", name)) return 2;
	if (!strcmp("isActive", name)) return 3;
	if (!strcmp("rotation_changed", name)) return 4;
	if (!strcmp("trackPoint_changed", name)) return 5;
	if (!strcmp("metadata", name)) return 6;
	if (!strcmp("description", name)) return 7;
	if (!strcmp("isOver", name)) return 8;
	return -1;
	}


static GF_Node *SphereSensor_Create()
{
	X_SphereSensor *p;
	GF_SAFEALLOC(p, X_SphereSensor);
	if(!p) return NULL;
	gf_node_setup((GF_Node *)p, TAG_X3D_SphereSensor);

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
	X_SpotLight *p = (X_SpotLight *) node;
	gf_node_unregister((GF_Node *) p->metadata, node);	
	gf_node_free((GF_Node *)p);
}


static u32 SpotLight_get_field_count(GF_Node *node, u8 dummy)
{
	return 11;
}

static GF_Err SpotLight_get_field(GF_Node *node, GF_FieldInfo *info)
{
	switch (info->fieldIndex) {
	case 0:
		info->name = "ambientIntensity";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFFLOAT;
		info->far_ptr = & ((X_SpotLight *) node)->ambientIntensity;
		return GF_OK;
	case 1:
		info->name = "attenuation";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFVEC3F;
		info->far_ptr = & ((X_SpotLight *) node)->attenuation;
		return GF_OK;
	case 2:
		info->name = "beamWidth";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFFLOAT;
		info->far_ptr = & ((X_SpotLight *) node)->beamWidth;
		return GF_OK;
	case 3:
		info->name = "color";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFCOLOR;
		info->far_ptr = & ((X_SpotLight *) node)->color;
		return GF_OK;
	case 4:
		info->name = "cutOffAngle";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFFLOAT;
		info->far_ptr = & ((X_SpotLight *) node)->cutOffAngle;
		return GF_OK;
	case 5:
		info->name = "direction";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFVEC3F;
		info->far_ptr = & ((X_SpotLight *) node)->direction;
		return GF_OK;
	case 6:
		info->name = "intensity";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFFLOAT;
		info->far_ptr = & ((X_SpotLight *) node)->intensity;
		return GF_OK;
	case 7:
		info->name = "location";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFVEC3F;
		info->far_ptr = & ((X_SpotLight *) node)->location;
		return GF_OK;
	case 8:
		info->name = "on";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFBOOL;
		info->far_ptr = & ((X_SpotLight *) node)->on;
		return GF_OK;
	case 9:
		info->name = "radius";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFFLOAT;
		info->far_ptr = & ((X_SpotLight *) node)->radius;
		return GF_OK;
	case 10:
		info->name = "metadata";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFNODE;
		info->NDTtype = NDT_SFMetadataNode;
		info->far_ptr = & ((X_SpotLight *)node)->metadata;
		return GF_OK;
	default:
		return GF_BAD_PARAM;
	}
}


static s32 SpotLight_get_field_index_by_name(char *name)
{
	if (!strcmp("ambientIntensity", name)) return 0;
	if (!strcmp("attenuation", name)) return 1;
	if (!strcmp("beamWidth", name)) return 2;
	if (!strcmp("color", name)) return 3;
	if (!strcmp("cutOffAngle", name)) return 4;
	if (!strcmp("direction", name)) return 5;
	if (!strcmp("intensity", name)) return 6;
	if (!strcmp("location", name)) return 7;
	if (!strcmp("on", name)) return 8;
	if (!strcmp("radius", name)) return 9;
	if (!strcmp("metadata", name)) return 10;
	return -1;
	}


static GF_Node *SpotLight_Create()
{
	X_SpotLight *p;
	GF_SAFEALLOC(p, X_SpotLight);
	if(!p) return NULL;
	gf_node_setup((GF_Node *)p, TAG_X3D_SpotLight);

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
	StaticGroup Node deletion
*/

static void StaticGroup_Del(GF_Node *node)
{
	X_StaticGroup *p = (X_StaticGroup *) node;
	gf_node_unregister((GF_Node *) p->metadata, node);	
	gf_sg_vrml_parent_destroy(node);	
	gf_node_free((GF_Node *)p);
}


static u32 StaticGroup_get_field_count(GF_Node *node, u8 dummy)
{
	return 2;
}

static GF_Err StaticGroup_get_field(GF_Node *node, GF_FieldInfo *info)
{
	switch (info->fieldIndex) {
	case 0:
		info->name = "children";
		info->eventType = GF_SG_EVENT_FIELD;
		info->fieldType = GF_SG_VRML_MFNODE;
		info->NDTtype = NDT_SF3DNode;
		info->far_ptr = & ((X_StaticGroup *)node)->children;
		return GF_OK;
	case 1:
		info->name = "metadata";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFNODE;
		info->NDTtype = NDT_SFMetadataNode;
		info->far_ptr = & ((X_StaticGroup *)node)->metadata;
		return GF_OK;
	default:
		return GF_BAD_PARAM;
	}
}


static s32 StaticGroup_get_field_index_by_name(char *name)
{
	if (!strcmp("children", name)) return 0;
	if (!strcmp("metadata", name)) return 1;
	return -1;
	}


static GF_Node *StaticGroup_Create()
{
	X_StaticGroup *p;
	GF_SAFEALLOC(p, X_StaticGroup);
	if(!p) return NULL;
	gf_node_setup((GF_Node *)p, TAG_X3D_StaticGroup);
	gf_sg_vrml_parent_setup((GF_Node *) p);

	/*default field values*/
	return (GF_Node *)p;
}


/*
	StringSensor Node deletion
*/

static void StringSensor_Del(GF_Node *node)
{
	X_StringSensor *p = (X_StringSensor *) node;
	gf_sg_sfstring_del(p->enteredText);
	gf_sg_sfstring_del(p->finalText);
	gf_node_unregister((GF_Node *) p->metadata, node);	
	gf_node_free((GF_Node *)p);
}


static u32 StringSensor_get_field_count(GF_Node *node, u8 dummy)
{
	return 6;
}

static GF_Err StringSensor_get_field(GF_Node *node, GF_FieldInfo *info)
{
	switch (info->fieldIndex) {
	case 0:
		info->name = "deletionAllowed";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFBOOL;
		info->far_ptr = & ((X_StringSensor *) node)->deletionAllowed;
		return GF_OK;
	case 1:
		info->name = "enabled";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFBOOL;
		info->far_ptr = & ((X_StringSensor *) node)->enabled;
		return GF_OK;
	case 2:
		info->name = "enteredText";
		info->eventType = GF_SG_EVENT_OUT;
		info->fieldType = GF_SG_VRML_SFSTRING;
		info->far_ptr = & ((X_StringSensor *) node)->enteredText;
		return GF_OK;
	case 3:
		info->name = "finalText";
		info->eventType = GF_SG_EVENT_OUT;
		info->fieldType = GF_SG_VRML_SFSTRING;
		info->far_ptr = & ((X_StringSensor *) node)->finalText;
		return GF_OK;
	case 4:
		info->name = "isActive";
		info->eventType = GF_SG_EVENT_OUT;
		info->fieldType = GF_SG_VRML_SFBOOL;
		info->far_ptr = & ((X_StringSensor *) node)->isActive;
		return GF_OK;
	case 5:
		info->name = "metadata";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFNODE;
		info->NDTtype = NDT_SFMetadataNode;
		info->far_ptr = & ((X_StringSensor *)node)->metadata;
		return GF_OK;
	default:
		return GF_BAD_PARAM;
	}
}


static s32 StringSensor_get_field_index_by_name(char *name)
{
	if (!strcmp("deletionAllowed", name)) return 0;
	if (!strcmp("enabled", name)) return 1;
	if (!strcmp("enteredText", name)) return 2;
	if (!strcmp("finalText", name)) return 3;
	if (!strcmp("isActive", name)) return 4;
	if (!strcmp("metadata", name)) return 5;
	return -1;
	}


static GF_Node *StringSensor_Create()
{
	X_StringSensor *p;
	GF_SAFEALLOC(p, X_StringSensor);
	if(!p) return NULL;
	gf_node_setup((GF_Node *)p, TAG_X3D_StringSensor);

	/*default field values*/
	p->deletionAllowed = 1;
	p->enabled = 1;
	return (GF_Node *)p;
}


/*
	Switch Node deletion
*/

static void Switch_Del(GF_Node *node)
{
	X_Switch *p = (X_Switch *) node;
	gf_node_unregister((GF_Node *) p->metadata, node);	
	gf_sg_vrml_parent_destroy(node);	
	gf_node_free((GF_Node *)p);
}


static u32 Switch_get_field_count(GF_Node *node, u8 dummy)
{
	return 5;
}

static GF_Err Switch_get_field(GF_Node *node, GF_FieldInfo *info)
{
	switch (info->fieldIndex) {
	case 0:
		info->name = "addChildren";
		info->eventType = GF_SG_EVENT_IN;
		info->on_event_in = ((X_Switch *)node)->on_addChildren;
		info->fieldType = GF_SG_VRML_MFNODE;
		info->NDTtype = NDT_SF3DNode;
		info->far_ptr = & ((X_Switch *)node)->addChildren;
		return GF_OK;
	case 1:
		info->name = "removeChildren";
		info->eventType = GF_SG_EVENT_IN;
		info->on_event_in = ((X_Switch *)node)->on_removeChildren;
		info->fieldType = GF_SG_VRML_MFNODE;
		info->NDTtype = NDT_SF3DNode;
		info->far_ptr = & ((X_Switch *)node)->removeChildren;
		return GF_OK;
	case 2:
		info->name = "children";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_MFNODE;
		info->NDTtype = NDT_SF3DNode;
		info->far_ptr = & ((X_Switch *)node)->children;
		return GF_OK;
	case 3:
		info->name = "whichChoice";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFINT32;
		info->far_ptr = & ((X_Switch *) node)->whichChoice;
		return GF_OK;
	case 4:
		info->name = "metadata";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFNODE;
		info->NDTtype = NDT_SFMetadataNode;
		info->far_ptr = & ((X_Switch *)node)->metadata;
		return GF_OK;
	default:
		return GF_BAD_PARAM;
	}
}


static s32 Switch_get_field_index_by_name(char *name)
{
	if (!strcmp("addChildren", name)) return 0;
	if (!strcmp("removeChildren", name)) return 1;
	if (!strcmp("children", name)) return 2;
	if (!strcmp("whichChoice", name)) return 3;
	if (!strcmp("metadata", name)) return 4;
	return -1;
	}


static GF_Node *Switch_Create()
{
	X_Switch *p;
	GF_SAFEALLOC(p, X_Switch);
	if(!p) return NULL;
	gf_node_setup((GF_Node *)p, TAG_X3D_Switch);
	gf_sg_vrml_parent_setup((GF_Node *) p);

	/*default field values*/
	p->whichChoice = -1;
	return (GF_Node *)p;
}


/*
	Text Node deletion
*/

static void Text_Del(GF_Node *node)
{
	X_Text *p = (X_Text *) node;
	gf_sg_mfstring_del(p->string);
	gf_sg_mffloat_del(p->length);
	gf_node_unregister((GF_Node *) p->fontStyle, node);	
	gf_node_unregister((GF_Node *) p->metadata, node);	
	gf_node_free((GF_Node *)p);
}


static u32 Text_get_field_count(GF_Node *node, u8 dummy)
{
	return 5;
}

static GF_Err Text_get_field(GF_Node *node, GF_FieldInfo *info)
{
	switch (info->fieldIndex) {
	case 0:
		info->name = "string";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_MFSTRING;
		info->far_ptr = & ((X_Text *) node)->string;
		return GF_OK;
	case 1:
		info->name = "length";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_MFFLOAT;
		info->far_ptr = & ((X_Text *) node)->length;
		return GF_OK;
	case 2:
		info->name = "fontStyle";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFNODE;
		info->NDTtype = NDT_SFFontStyleNode;
		info->far_ptr = & ((X_Text *)node)->fontStyle;
		return GF_OK;
	case 3:
		info->name = "maxExtent";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFFLOAT;
		info->far_ptr = & ((X_Text *) node)->maxExtent;
		return GF_OK;
	case 4:
		info->name = "metadata";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFNODE;
		info->NDTtype = NDT_SFMetadataNode;
		info->far_ptr = & ((X_Text *)node)->metadata;
		return GF_OK;
	default:
		return GF_BAD_PARAM;
	}
}


static s32 Text_get_field_index_by_name(char *name)
{
	if (!strcmp("string", name)) return 0;
	if (!strcmp("length", name)) return 1;
	if (!strcmp("fontStyle", name)) return 2;
	if (!strcmp("maxExtent", name)) return 3;
	if (!strcmp("metadata", name)) return 4;
	return -1;
	}


static GF_Node *Text_Create()
{
	X_Text *p;
	GF_SAFEALLOC(p, X_Text);
	if(!p) return NULL;
	gf_node_setup((GF_Node *)p, TAG_X3D_Text);

	/*default field values*/
	p->maxExtent = FLT2FIX(0.0);
	return (GF_Node *)p;
}


/*
	TextureBackground Node deletion
*/

static void TextureBackground_Del(GF_Node *node)
{
	X_TextureBackground *p = (X_TextureBackground *) node;
	gf_sg_mffloat_del(p->groundAngle);
	gf_sg_mfcolor_del(p->groundColor);
	gf_node_unregister((GF_Node *) p->backTexture, node);	
	gf_node_unregister((GF_Node *) p->bottomTexture, node);	
	gf_node_unregister((GF_Node *) p->frontTexture, node);	
	gf_node_unregister((GF_Node *) p->leftTexture, node);	
	gf_node_unregister((GF_Node *) p->rightTexture, node);	
	gf_node_unregister((GF_Node *) p->topTexture, node);	
	gf_sg_mffloat_del(p->skyAngle);
	gf_sg_mfcolor_del(p->skyColor);
	gf_sg_mffloat_del(p->transparency);
	gf_node_unregister((GF_Node *) p->metadata, node);	
	gf_node_free((GF_Node *)p);
}


static u32 TextureBackground_get_field_count(GF_Node *node, u8 dummy)
{
	return 15;
}

static GF_Err TextureBackground_get_field(GF_Node *node, GF_FieldInfo *info)
{
	switch (info->fieldIndex) {
	case 0:
		info->name = "set_bind";
		info->eventType = GF_SG_EVENT_IN;
		info->on_event_in = ((X_TextureBackground *)node)->on_set_bind;
		info->fieldType = GF_SG_VRML_SFBOOL;
		info->far_ptr = & ((X_TextureBackground *) node)->set_bind;
		return GF_OK;
	case 1:
		info->name = "groundAngle";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_MFFLOAT;
		info->far_ptr = & ((X_TextureBackground *) node)->groundAngle;
		return GF_OK;
	case 2:
		info->name = "groundColor";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_MFCOLOR;
		info->far_ptr = & ((X_TextureBackground *) node)->groundColor;
		return GF_OK;
	case 3:
		info->name = "backTexture";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFNODE;
		info->NDTtype = NDT_SFTextureNode;
		info->far_ptr = & ((X_TextureBackground *)node)->backTexture;
		return GF_OK;
	case 4:
		info->name = "bottomTexture";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFNODE;
		info->NDTtype = NDT_SFTextureNode;
		info->far_ptr = & ((X_TextureBackground *)node)->bottomTexture;
		return GF_OK;
	case 5:
		info->name = "frontTexture";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFNODE;
		info->NDTtype = NDT_SFTextureNode;
		info->far_ptr = & ((X_TextureBackground *)node)->frontTexture;
		return GF_OK;
	case 6:
		info->name = "leftTexture";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFNODE;
		info->NDTtype = NDT_SFTextureNode;
		info->far_ptr = & ((X_TextureBackground *)node)->leftTexture;
		return GF_OK;
	case 7:
		info->name = "rightTexture";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFNODE;
		info->NDTtype = NDT_SFTextureNode;
		info->far_ptr = & ((X_TextureBackground *)node)->rightTexture;
		return GF_OK;
	case 8:
		info->name = "topTexture";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFNODE;
		info->NDTtype = NDT_SFTextureNode;
		info->far_ptr = & ((X_TextureBackground *)node)->topTexture;
		return GF_OK;
	case 9:
		info->name = "skyAngle";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_MFFLOAT;
		info->far_ptr = & ((X_TextureBackground *) node)->skyAngle;
		return GF_OK;
	case 10:
		info->name = "skyColor";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_MFCOLOR;
		info->far_ptr = & ((X_TextureBackground *) node)->skyColor;
		return GF_OK;
	case 11:
		info->name = "transparency";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_MFFLOAT;
		info->far_ptr = & ((X_TextureBackground *) node)->transparency;
		return GF_OK;
	case 12:
		info->name = "bindTime";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFTIME;
		info->far_ptr = & ((X_TextureBackground *) node)->bindTime;
		return GF_OK;
	case 13:
		info->name = "isBound";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFBOOL;
		info->far_ptr = & ((X_TextureBackground *) node)->isBound;
		return GF_OK;
	case 14:
		info->name = "metadata";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFNODE;
		info->NDTtype = NDT_SFMetadataNode;
		info->far_ptr = & ((X_TextureBackground *)node)->metadata;
		return GF_OK;
	default:
		return GF_BAD_PARAM;
	}
}


static s32 TextureBackground_get_field_index_by_name(char *name)
{
	if (!strcmp("set_bind", name)) return 0;
	if (!strcmp("groundAngle", name)) return 1;
	if (!strcmp("groundColor", name)) return 2;
	if (!strcmp("backTexture", name)) return 3;
	if (!strcmp("bottomTexture", name)) return 4;
	if (!strcmp("frontTexture", name)) return 5;
	if (!strcmp("leftTexture", name)) return 6;
	if (!strcmp("rightTexture", name)) return 7;
	if (!strcmp("topTexture", name)) return 8;
	if (!strcmp("skyAngle", name)) return 9;
	if (!strcmp("skyColor", name)) return 10;
	if (!strcmp("transparency", name)) return 11;
	if (!strcmp("bindTime", name)) return 12;
	if (!strcmp("isBound", name)) return 13;
	if (!strcmp("metadata", name)) return 14;
	return -1;
	}


static GF_Node *TextureBackground_Create()
{
	X_TextureBackground *p;
	GF_SAFEALLOC(p, X_TextureBackground);
	if(!p) return NULL;
	gf_node_setup((GF_Node *)p, TAG_X3D_TextureBackground);

	/*default field values*/
	p->skyColor.vals = (SFColor*)malloc(sizeof(SFColor)*1);
	p->skyColor.count = 1;
	p->skyColor.vals[0].red = FLT2FIX(0);
	p->skyColor.vals[0].green = FLT2FIX(0);
	p->skyColor.vals[0].blue = FLT2FIX(0);
	p->transparency.vals = (SFFloat *)malloc(sizeof(SFFloat)*1);
	p->transparency.count = 1;
	p->transparency.vals[0] = FLT2FIX(0);
	return (GF_Node *)p;
}


/*
	TextureCoordinate Node deletion
*/

static void TextureCoordinate_Del(GF_Node *node)
{
	X_TextureCoordinate *p = (X_TextureCoordinate *) node;
	gf_sg_mfvec2f_del(p->point);
	gf_node_unregister((GF_Node *) p->metadata, node);	
	gf_node_free((GF_Node *)p);
}


static u32 TextureCoordinate_get_field_count(GF_Node *node, u8 dummy)
{
	return 2;
}

static GF_Err TextureCoordinate_get_field(GF_Node *node, GF_FieldInfo *info)
{
	switch (info->fieldIndex) {
	case 0:
		info->name = "point";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_MFVEC2F;
		info->far_ptr = & ((X_TextureCoordinate *) node)->point;
		return GF_OK;
	case 1:
		info->name = "metadata";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFNODE;
		info->NDTtype = NDT_SFMetadataNode;
		info->far_ptr = & ((X_TextureCoordinate *)node)->metadata;
		return GF_OK;
	default:
		return GF_BAD_PARAM;
	}
}


static s32 TextureCoordinate_get_field_index_by_name(char *name)
{
	if (!strcmp("point", name)) return 0;
	if (!strcmp("metadata", name)) return 1;
	return -1;
	}


static GF_Node *TextureCoordinate_Create()
{
	X_TextureCoordinate *p;
	GF_SAFEALLOC(p, X_TextureCoordinate);
	if(!p) return NULL;
	gf_node_setup((GF_Node *)p, TAG_X3D_TextureCoordinate);

	/*default field values*/
	return (GF_Node *)p;
}


/*
	TextureCoordinateGenerator Node deletion
*/

static void TextureCoordinateGenerator_Del(GF_Node *node)
{
	X_TextureCoordinateGenerator *p = (X_TextureCoordinateGenerator *) node;
	gf_sg_sfstring_del(p->mode);
	gf_sg_mffloat_del(p->parameter);
	gf_node_unregister((GF_Node *) p->metadata, node);	
	gf_node_free((GF_Node *)p);
}


static u32 TextureCoordinateGenerator_get_field_count(GF_Node *node, u8 dummy)
{
	return 3;
}

static GF_Err TextureCoordinateGenerator_get_field(GF_Node *node, GF_FieldInfo *info)
{
	switch (info->fieldIndex) {
	case 0:
		info->name = "mode";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFSTRING;
		info->far_ptr = & ((X_TextureCoordinateGenerator *) node)->mode;
		return GF_OK;
	case 1:
		info->name = "parameter";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_MFFLOAT;
		info->far_ptr = & ((X_TextureCoordinateGenerator *) node)->parameter;
		return GF_OK;
	case 2:
		info->name = "metadata";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFNODE;
		info->NDTtype = NDT_SFMetadataNode;
		info->far_ptr = & ((X_TextureCoordinateGenerator *)node)->metadata;
		return GF_OK;
	default:
		return GF_BAD_PARAM;
	}
}


static s32 TextureCoordinateGenerator_get_field_index_by_name(char *name)
{
	if (!strcmp("mode", name)) return 0;
	if (!strcmp("parameter", name)) return 1;
	if (!strcmp("metadata", name)) return 2;
	return -1;
	}


static GF_Node *TextureCoordinateGenerator_Create()
{
	X_TextureCoordinateGenerator *p;
	GF_SAFEALLOC(p, X_TextureCoordinateGenerator);
	if(!p) return NULL;
	gf_node_setup((GF_Node *)p, TAG_X3D_TextureCoordinateGenerator);

	/*default field values*/
	p->mode.buffer = (char*) malloc(sizeof(char) * 7);
	strcpy(p->mode.buffer, "SPHERE");
	return (GF_Node *)p;
}


/*
	TextureTransform Node deletion
*/

static void TextureTransform_Del(GF_Node *node)
{
	X_TextureTransform *p = (X_TextureTransform *) node;
	gf_node_unregister((GF_Node *) p->metadata, node);	
	gf_node_free((GF_Node *)p);
}


static u32 TextureTransform_get_field_count(GF_Node *node, u8 dummy)
{
	return 5;
}

static GF_Err TextureTransform_get_field(GF_Node *node, GF_FieldInfo *info)
{
	switch (info->fieldIndex) {
	case 0:
		info->name = "center";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFVEC2F;
		info->far_ptr = & ((X_TextureTransform *) node)->center;
		return GF_OK;
	case 1:
		info->name = "rotation";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFFLOAT;
		info->far_ptr = & ((X_TextureTransform *) node)->rotation;
		return GF_OK;
	case 2:
		info->name = "scale";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFVEC2F;
		info->far_ptr = & ((X_TextureTransform *) node)->scale;
		return GF_OK;
	case 3:
		info->name = "translation";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFVEC2F;
		info->far_ptr = & ((X_TextureTransform *) node)->translation;
		return GF_OK;
	case 4:
		info->name = "metadata";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFNODE;
		info->NDTtype = NDT_SFMetadataNode;
		info->far_ptr = & ((X_TextureTransform *)node)->metadata;
		return GF_OK;
	default:
		return GF_BAD_PARAM;
	}
}


static s32 TextureTransform_get_field_index_by_name(char *name)
{
	if (!strcmp("center", name)) return 0;
	if (!strcmp("rotation", name)) return 1;
	if (!strcmp("scale", name)) return 2;
	if (!strcmp("translation", name)) return 3;
	if (!strcmp("metadata", name)) return 4;
	return -1;
	}


static GF_Node *TextureTransform_Create()
{
	X_TextureTransform *p;
	GF_SAFEALLOC(p, X_TextureTransform);
	if(!p) return NULL;
	gf_node_setup((GF_Node *)p, TAG_X3D_TextureTransform);

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
	X_TimeSensor *p = (X_TimeSensor *) node;
	gf_node_unregister((GF_Node *) p->metadata, node);	
	gf_node_free((GF_Node *)p);
}


static u32 TimeSensor_get_field_count(GF_Node *node, u8 dummy)
{
	return 14;
}

static GF_Err TimeSensor_get_field(GF_Node *node, GF_FieldInfo *info)
{
	switch (info->fieldIndex) {
	case 0:
		info->name = "cycleInterval";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFTIME;
		info->far_ptr = & ((X_TimeSensor *) node)->cycleInterval;
		return GF_OK;
	case 1:
		info->name = "enabled";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFBOOL;
		info->far_ptr = & ((X_TimeSensor *) node)->enabled;
		return GF_OK;
	case 2:
		info->name = "loop";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFBOOL;
		info->far_ptr = & ((X_TimeSensor *) node)->loop;
		return GF_OK;
	case 3:
		info->name = "startTime";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFTIME;
		info->far_ptr = & ((X_TimeSensor *) node)->startTime;
		return GF_OK;
	case 4:
		info->name = "stopTime";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFTIME;
		info->far_ptr = & ((X_TimeSensor *) node)->stopTime;
		return GF_OK;
	case 5:
		info->name = "cycleTime";
		info->eventType = GF_SG_EVENT_OUT;
		info->fieldType = GF_SG_VRML_SFTIME;
		info->far_ptr = & ((X_TimeSensor *) node)->cycleTime;
		return GF_OK;
	case 6:
		info->name = "fraction_changed";
		info->eventType = GF_SG_EVENT_OUT;
		info->fieldType = GF_SG_VRML_SFFLOAT;
		info->far_ptr = & ((X_TimeSensor *) node)->fraction_changed;
		return GF_OK;
	case 7:
		info->name = "isActive";
		info->eventType = GF_SG_EVENT_OUT;
		info->fieldType = GF_SG_VRML_SFBOOL;
		info->far_ptr = & ((X_TimeSensor *) node)->isActive;
		return GF_OK;
	case 8:
		info->name = "time";
		info->eventType = GF_SG_EVENT_OUT;
		info->fieldType = GF_SG_VRML_SFTIME;
		info->far_ptr = & ((X_TimeSensor *) node)->time;
		return GF_OK;
	case 9:
		info->name = "metadata";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFNODE;
		info->NDTtype = NDT_SFMetadataNode;
		info->far_ptr = & ((X_TimeSensor *)node)->metadata;
		return GF_OK;
	case 10:
		info->name = "pauseTime";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFTIME;
		info->far_ptr = & ((X_TimeSensor *) node)->pauseTime;
		return GF_OK;
	case 11:
		info->name = "resumeTime";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFTIME;
		info->far_ptr = & ((X_TimeSensor *) node)->resumeTime;
		return GF_OK;
	case 12:
		info->name = "elapsedTime";
		info->eventType = GF_SG_EVENT_OUT;
		info->fieldType = GF_SG_VRML_SFTIME;
		info->far_ptr = & ((X_TimeSensor *) node)->elapsedTime;
		return GF_OK;
	case 13:
		info->name = "isPaused";
		info->eventType = GF_SG_EVENT_OUT;
		info->fieldType = GF_SG_VRML_SFBOOL;
		info->far_ptr = & ((X_TimeSensor *) node)->isPaused;
		return GF_OK;
	default:
		return GF_BAD_PARAM;
	}
}


static s32 TimeSensor_get_field_index_by_name(char *name)
{
	if (!strcmp("cycleInterval", name)) return 0;
	if (!strcmp("enabled", name)) return 1;
	if (!strcmp("loop", name)) return 2;
	if (!strcmp("startTime", name)) return 3;
	if (!strcmp("stopTime", name)) return 4;
	if (!strcmp("cycleTime", name)) return 5;
	if (!strcmp("fraction_changed", name)) return 6;
	if (!strcmp("isActive", name)) return 7;
	if (!strcmp("time", name)) return 8;
	if (!strcmp("metadata", name)) return 9;
	if (!strcmp("pauseTime", name)) return 10;
	if (!strcmp("resumeTime", name)) return 11;
	if (!strcmp("elapsedTime", name)) return 12;
	if (!strcmp("isPaused", name)) return 13;
	return -1;
	}


static GF_Node *TimeSensor_Create()
{
	X_TimeSensor *p;
	GF_SAFEALLOC(p, X_TimeSensor);
	if(!p) return NULL;
	gf_node_setup((GF_Node *)p, TAG_X3D_TimeSensor);

	/*default field values*/
	p->cycleInterval = 1;
	p->enabled = 1;
	p->startTime = 0;
	p->stopTime = 0;
	p->pauseTime = 0;
	p->resumeTime = 0;
	return (GF_Node *)p;
}


/*
	TimeTrigger Node deletion
*/

static void TimeTrigger_Del(GF_Node *node)
{
	X_TimeTrigger *p = (X_TimeTrigger *) node;
	gf_node_unregister((GF_Node *) p->metadata, node);	
	gf_node_free((GF_Node *)p);
}


static u32 TimeTrigger_get_field_count(GF_Node *node, u8 dummy)
{
	return 3;
}

static GF_Err TimeTrigger_get_field(GF_Node *node, GF_FieldInfo *info)
{
	switch (info->fieldIndex) {
	case 0:
		info->name = "set_boolean";
		info->eventType = GF_SG_EVENT_IN;
		info->on_event_in = ((X_TimeTrigger *)node)->on_set_boolean;
		info->fieldType = GF_SG_VRML_SFBOOL;
		info->far_ptr = & ((X_TimeTrigger *) node)->set_boolean;
		return GF_OK;
	case 1:
		info->name = "triggerTime";
		info->eventType = GF_SG_EVENT_OUT;
		info->fieldType = GF_SG_VRML_SFTIME;
		info->far_ptr = & ((X_TimeTrigger *) node)->triggerTime;
		return GF_OK;
	case 2:
		info->name = "metadata";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFNODE;
		info->NDTtype = NDT_SFMetadataNode;
		info->far_ptr = & ((X_TimeTrigger *)node)->metadata;
		return GF_OK;
	default:
		return GF_BAD_PARAM;
	}
}


static s32 TimeTrigger_get_field_index_by_name(char *name)
{
	if (!strcmp("set_boolean", name)) return 0;
	if (!strcmp("triggerTime", name)) return 1;
	if (!strcmp("metadata", name)) return 2;
	return -1;
	}


static GF_Node *TimeTrigger_Create()
{
	X_TimeTrigger *p;
	GF_SAFEALLOC(p, X_TimeTrigger);
	if(!p) return NULL;
	gf_node_setup((GF_Node *)p, TAG_X3D_TimeTrigger);

	/*default field values*/
	return (GF_Node *)p;
}


/*
	TouchSensor Node deletion
*/

static void TouchSensor_Del(GF_Node *node)
{
	X_TouchSensor *p = (X_TouchSensor *) node;
	gf_node_unregister((GF_Node *) p->metadata, node);	
	gf_sg_sfstring_del(p->description);
	gf_node_free((GF_Node *)p);
}


static u32 TouchSensor_get_field_count(GF_Node *node, u8 dummy)
{
	return 9;
}

static GF_Err TouchSensor_get_field(GF_Node *node, GF_FieldInfo *info)
{
	switch (info->fieldIndex) {
	case 0:
		info->name = "enabled";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFBOOL;
		info->far_ptr = & ((X_TouchSensor *) node)->enabled;
		return GF_OK;
	case 1:
		info->name = "hitNormal_changed";
		info->eventType = GF_SG_EVENT_OUT;
		info->fieldType = GF_SG_VRML_SFVEC3F;
		info->far_ptr = & ((X_TouchSensor *) node)->hitNormal_changed;
		return GF_OK;
	case 2:
		info->name = "hitPoint_changed";
		info->eventType = GF_SG_EVENT_OUT;
		info->fieldType = GF_SG_VRML_SFVEC3F;
		info->far_ptr = & ((X_TouchSensor *) node)->hitPoint_changed;
		return GF_OK;
	case 3:
		info->name = "hitTexCoord_changed";
		info->eventType = GF_SG_EVENT_OUT;
		info->fieldType = GF_SG_VRML_SFVEC2F;
		info->far_ptr = & ((X_TouchSensor *) node)->hitTexCoord_changed;
		return GF_OK;
	case 4:
		info->name = "isActive";
		info->eventType = GF_SG_EVENT_OUT;
		info->fieldType = GF_SG_VRML_SFBOOL;
		info->far_ptr = & ((X_TouchSensor *) node)->isActive;
		return GF_OK;
	case 5:
		info->name = "isOver";
		info->eventType = GF_SG_EVENT_OUT;
		info->fieldType = GF_SG_VRML_SFBOOL;
		info->far_ptr = & ((X_TouchSensor *) node)->isOver;
		return GF_OK;
	case 6:
		info->name = "touchTime";
		info->eventType = GF_SG_EVENT_OUT;
		info->fieldType = GF_SG_VRML_SFTIME;
		info->far_ptr = & ((X_TouchSensor *) node)->touchTime;
		return GF_OK;
	case 7:
		info->name = "metadata";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFNODE;
		info->NDTtype = NDT_SFMetadataNode;
		info->far_ptr = & ((X_TouchSensor *)node)->metadata;
		return GF_OK;
	case 8:
		info->name = "description";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFSTRING;
		info->far_ptr = & ((X_TouchSensor *) node)->description;
		return GF_OK;
	default:
		return GF_BAD_PARAM;
	}
}


static s32 TouchSensor_get_field_index_by_name(char *name)
{
	if (!strcmp("enabled", name)) return 0;
	if (!strcmp("hitNormal_changed", name)) return 1;
	if (!strcmp("hitPoint_changed", name)) return 2;
	if (!strcmp("hitTexCoord_changed", name)) return 3;
	if (!strcmp("isActive", name)) return 4;
	if (!strcmp("isOver", name)) return 5;
	if (!strcmp("touchTime", name)) return 6;
	if (!strcmp("metadata", name)) return 7;
	if (!strcmp("description", name)) return 8;
	return -1;
	}


static GF_Node *TouchSensor_Create()
{
	X_TouchSensor *p;
	GF_SAFEALLOC(p, X_TouchSensor);
	if(!p) return NULL;
	gf_node_setup((GF_Node *)p, TAG_X3D_TouchSensor);

	/*default field values*/
	p->enabled = 1;
	return (GF_Node *)p;
}


/*
	Transform Node deletion
*/

static void Transform_Del(GF_Node *node)
{
	X_Transform *p = (X_Transform *) node;
	gf_node_unregister((GF_Node *) p->metadata, node);	
	gf_sg_vrml_parent_destroy(node);	
	gf_node_free((GF_Node *)p);
}


static u32 Transform_get_field_count(GF_Node *node, u8 dummy)
{
	return 9;
}

static GF_Err Transform_get_field(GF_Node *node, GF_FieldInfo *info)
{
	switch (info->fieldIndex) {
	case 0:
		info->name = "addChildren";
		info->eventType = GF_SG_EVENT_IN;
		info->on_event_in = ((X_Transform *)node)->on_addChildren;
		info->fieldType = GF_SG_VRML_MFNODE;
		info->NDTtype = NDT_SF3DNode;
		info->far_ptr = & ((X_Transform *)node)->addChildren;
		return GF_OK;
	case 1:
		info->name = "removeChildren";
		info->eventType = GF_SG_EVENT_IN;
		info->on_event_in = ((X_Transform *)node)->on_removeChildren;
		info->fieldType = GF_SG_VRML_MFNODE;
		info->NDTtype = NDT_SF3DNode;
		info->far_ptr = & ((X_Transform *)node)->removeChildren;
		return GF_OK;
	case 2:
		info->name = "center";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFVEC3F;
		info->far_ptr = & ((X_Transform *) node)->center;
		return GF_OK;
	case 3:
		info->name = "children";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_MFNODE;
		info->NDTtype = NDT_SF3DNode;
		info->far_ptr = & ((X_Transform *)node)->children;
		return GF_OK;
	case 4:
		info->name = "rotation";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFROTATION;
		info->far_ptr = & ((X_Transform *) node)->rotation;
		return GF_OK;
	case 5:
		info->name = "scale";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFVEC3F;
		info->far_ptr = & ((X_Transform *) node)->scale;
		return GF_OK;
	case 6:
		info->name = "scaleOrientation";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFROTATION;
		info->far_ptr = & ((X_Transform *) node)->scaleOrientation;
		return GF_OK;
	case 7:
		info->name = "translation";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFVEC3F;
		info->far_ptr = & ((X_Transform *) node)->translation;
		return GF_OK;
	case 8:
		info->name = "metadata";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFNODE;
		info->NDTtype = NDT_SFMetadataNode;
		info->far_ptr = & ((X_Transform *)node)->metadata;
		return GF_OK;
	default:
		return GF_BAD_PARAM;
	}
}


static s32 Transform_get_field_index_by_name(char *name)
{
	if (!strcmp("addChildren", name)) return 0;
	if (!strcmp("removeChildren", name)) return 1;
	if (!strcmp("center", name)) return 2;
	if (!strcmp("children", name)) return 3;
	if (!strcmp("rotation", name)) return 4;
	if (!strcmp("scale", name)) return 5;
	if (!strcmp("scaleOrientation", name)) return 6;
	if (!strcmp("translation", name)) return 7;
	if (!strcmp("metadata", name)) return 8;
	return -1;
	}


static GF_Node *Transform_Create()
{
	X_Transform *p;
	GF_SAFEALLOC(p, X_Transform);
	if(!p) return NULL;
	gf_node_setup((GF_Node *)p, TAG_X3D_Transform);
	gf_sg_vrml_parent_setup((GF_Node *) p);

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
	TriangleFanSet Node deletion
*/

static void TriangleFanSet_Del(GF_Node *node)
{
	X_TriangleFanSet *p = (X_TriangleFanSet *) node;
	gf_node_unregister((GF_Node *) p->color, node);	
	gf_node_unregister((GF_Node *) p->coord, node);	
	gf_sg_mfint32_del(p->fanCount);
	gf_node_unregister((GF_Node *) p->normal, node);	
	gf_node_unregister((GF_Node *) p->texCoord, node);	
	gf_node_unregister((GF_Node *) p->metadata, node);	
	gf_node_free((GF_Node *)p);
}


static u32 TriangleFanSet_get_field_count(GF_Node *node, u8 dummy)
{
	return 10;
}

static GF_Err TriangleFanSet_get_field(GF_Node *node, GF_FieldInfo *info)
{
	switch (info->fieldIndex) {
	case 0:
		info->name = "color";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFNODE;
		info->NDTtype = NDT_SFColorNode;
		info->far_ptr = & ((X_TriangleFanSet *)node)->color;
		return GF_OK;
	case 1:
		info->name = "coord";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFNODE;
		info->NDTtype = NDT_SFCoordinateNode;
		info->far_ptr = & ((X_TriangleFanSet *)node)->coord;
		return GF_OK;
	case 2:
		info->name = "fanCount";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_MFINT32;
		info->far_ptr = & ((X_TriangleFanSet *) node)->fanCount;
		return GF_OK;
	case 3:
		info->name = "normal";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFNODE;
		info->NDTtype = NDT_SFNormalNode;
		info->far_ptr = & ((X_TriangleFanSet *)node)->normal;
		return GF_OK;
	case 4:
		info->name = "texCoord";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFNODE;
		info->NDTtype = NDT_SFTextureCoordinateNode;
		info->far_ptr = & ((X_TriangleFanSet *)node)->texCoord;
		return GF_OK;
	case 5:
		info->name = "ccw";
		info->eventType = GF_SG_EVENT_FIELD;
		info->fieldType = GF_SG_VRML_SFBOOL;
		info->far_ptr = & ((X_TriangleFanSet *) node)->ccw;
		return GF_OK;
	case 6:
		info->name = "colorPerVertex";
		info->eventType = GF_SG_EVENT_FIELD;
		info->fieldType = GF_SG_VRML_SFBOOL;
		info->far_ptr = & ((X_TriangleFanSet *) node)->colorPerVertex;
		return GF_OK;
	case 7:
		info->name = "normalPerVertex";
		info->eventType = GF_SG_EVENT_FIELD;
		info->fieldType = GF_SG_VRML_SFBOOL;
		info->far_ptr = & ((X_TriangleFanSet *) node)->normalPerVertex;
		return GF_OK;
	case 8:
		info->name = "solid";
		info->eventType = GF_SG_EVENT_FIELD;
		info->fieldType = GF_SG_VRML_SFBOOL;
		info->far_ptr = & ((X_TriangleFanSet *) node)->solid;
		return GF_OK;
	case 9:
		info->name = "metadata";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFNODE;
		info->NDTtype = NDT_SFMetadataNode;
		info->far_ptr = & ((X_TriangleFanSet *)node)->metadata;
		return GF_OK;
	default:
		return GF_BAD_PARAM;
	}
}


static s32 TriangleFanSet_get_field_index_by_name(char *name)
{
	if (!strcmp("color", name)) return 0;
	if (!strcmp("coord", name)) return 1;
	if (!strcmp("fanCount", name)) return 2;
	if (!strcmp("normal", name)) return 3;
	if (!strcmp("texCoord", name)) return 4;
	if (!strcmp("ccw", name)) return 5;
	if (!strcmp("colorPerVertex", name)) return 6;
	if (!strcmp("normalPerVertex", name)) return 7;
	if (!strcmp("solid", name)) return 8;
	if (!strcmp("metadata", name)) return 9;
	return -1;
	}


static GF_Node *TriangleFanSet_Create()
{
	X_TriangleFanSet *p;
	GF_SAFEALLOC(p, X_TriangleFanSet);
	if(!p) return NULL;
	gf_node_setup((GF_Node *)p, TAG_X3D_TriangleFanSet);

	/*default field values*/
	p->ccw = 1;
	p->colorPerVertex = 1;
	p->normalPerVertex = 1;
	p->solid = 1;
	return (GF_Node *)p;
}


/*
	TriangleSet Node deletion
*/

static void TriangleSet_Del(GF_Node *node)
{
	X_TriangleSet *p = (X_TriangleSet *) node;
	gf_node_unregister((GF_Node *) p->color, node);	
	gf_node_unregister((GF_Node *) p->coord, node);	
	gf_node_unregister((GF_Node *) p->normal, node);	
	gf_node_unregister((GF_Node *) p->texCoord, node);	
	gf_node_unregister((GF_Node *) p->metadata, node);	
	gf_node_free((GF_Node *)p);
}


static u32 TriangleSet_get_field_count(GF_Node *node, u8 dummy)
{
	return 9;
}

static GF_Err TriangleSet_get_field(GF_Node *node, GF_FieldInfo *info)
{
	switch (info->fieldIndex) {
	case 0:
		info->name = "color";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFNODE;
		info->NDTtype = NDT_SFColorNode;
		info->far_ptr = & ((X_TriangleSet *)node)->color;
		return GF_OK;
	case 1:
		info->name = "coord";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFNODE;
		info->NDTtype = NDT_SFCoordinateNode;
		info->far_ptr = & ((X_TriangleSet *)node)->coord;
		return GF_OK;
	case 2:
		info->name = "normal";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFNODE;
		info->NDTtype = NDT_SFNormalNode;
		info->far_ptr = & ((X_TriangleSet *)node)->normal;
		return GF_OK;
	case 3:
		info->name = "texCoord";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFNODE;
		info->NDTtype = NDT_SFTextureCoordinateNode;
		info->far_ptr = & ((X_TriangleSet *)node)->texCoord;
		return GF_OK;
	case 4:
		info->name = "ccw";
		info->eventType = GF_SG_EVENT_FIELD;
		info->fieldType = GF_SG_VRML_SFBOOL;
		info->far_ptr = & ((X_TriangleSet *) node)->ccw;
		return GF_OK;
	case 5:
		info->name = "colorPerVertex";
		info->eventType = GF_SG_EVENT_FIELD;
		info->fieldType = GF_SG_VRML_SFBOOL;
		info->far_ptr = & ((X_TriangleSet *) node)->colorPerVertex;
		return GF_OK;
	case 6:
		info->name = "normalPerVertex";
		info->eventType = GF_SG_EVENT_FIELD;
		info->fieldType = GF_SG_VRML_SFBOOL;
		info->far_ptr = & ((X_TriangleSet *) node)->normalPerVertex;
		return GF_OK;
	case 7:
		info->name = "solid";
		info->eventType = GF_SG_EVENT_FIELD;
		info->fieldType = GF_SG_VRML_SFBOOL;
		info->far_ptr = & ((X_TriangleSet *) node)->solid;
		return GF_OK;
	case 8:
		info->name = "metadata";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFNODE;
		info->NDTtype = NDT_SFMetadataNode;
		info->far_ptr = & ((X_TriangleSet *)node)->metadata;
		return GF_OK;
	default:
		return GF_BAD_PARAM;
	}
}


static s32 TriangleSet_get_field_index_by_name(char *name)
{
	if (!strcmp("color", name)) return 0;
	if (!strcmp("coord", name)) return 1;
	if (!strcmp("normal", name)) return 2;
	if (!strcmp("texCoord", name)) return 3;
	if (!strcmp("ccw", name)) return 4;
	if (!strcmp("colorPerVertex", name)) return 5;
	if (!strcmp("normalPerVertex", name)) return 6;
	if (!strcmp("solid", name)) return 7;
	if (!strcmp("metadata", name)) return 8;
	return -1;
	}


static GF_Node *TriangleSet_Create()
{
	X_TriangleSet *p;
	GF_SAFEALLOC(p, X_TriangleSet);
	if(!p) return NULL;
	gf_node_setup((GF_Node *)p, TAG_X3D_TriangleSet);

	/*default field values*/
	p->ccw = 1;
	p->colorPerVertex = 1;
	p->normalPerVertex = 1;
	p->solid = 1;
	return (GF_Node *)p;
}


/*
	TriangleSet2D Node deletion
*/

static void TriangleSet2D_Del(GF_Node *node)
{
	X_TriangleSet2D *p = (X_TriangleSet2D *) node;
	gf_sg_mfvec2f_del(p->vertices);
	gf_node_unregister((GF_Node *) p->metadata, node);	
	gf_node_free((GF_Node *)p);
}


static u32 TriangleSet2D_get_field_count(GF_Node *node, u8 dummy)
{
	return 2;
}

static GF_Err TriangleSet2D_get_field(GF_Node *node, GF_FieldInfo *info)
{
	switch (info->fieldIndex) {
	case 0:
		info->name = "vertices";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_MFVEC2F;
		info->far_ptr = & ((X_TriangleSet2D *) node)->vertices;
		return GF_OK;
	case 1:
		info->name = "metadata";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFNODE;
		info->NDTtype = NDT_SFMetadataNode;
		info->far_ptr = & ((X_TriangleSet2D *)node)->metadata;
		return GF_OK;
	default:
		return GF_BAD_PARAM;
	}
}


static s32 TriangleSet2D_get_field_index_by_name(char *name)
{
	if (!strcmp("vertices", name)) return 0;
	if (!strcmp("metadata", name)) return 1;
	return -1;
	}


static GF_Node *TriangleSet2D_Create()
{
	X_TriangleSet2D *p;
	GF_SAFEALLOC(p, X_TriangleSet2D);
	if(!p) return NULL;
	gf_node_setup((GF_Node *)p, TAG_X3D_TriangleSet2D);

	/*default field values*/
	return (GF_Node *)p;
}


/*
	TriangleStripSet Node deletion
*/

static void TriangleStripSet_Del(GF_Node *node)
{
	X_TriangleStripSet *p = (X_TriangleStripSet *) node;
	gf_node_unregister((GF_Node *) p->color, node);	
	gf_node_unregister((GF_Node *) p->coord, node);	
	gf_node_unregister((GF_Node *) p->normal, node);	
	gf_sg_mfint32_del(p->stripCount);
	gf_node_unregister((GF_Node *) p->texCoord, node);	
	gf_node_unregister((GF_Node *) p->metadata, node);	
	gf_node_free((GF_Node *)p);
}


static u32 TriangleStripSet_get_field_count(GF_Node *node, u8 dummy)
{
	return 10;
}

static GF_Err TriangleStripSet_get_field(GF_Node *node, GF_FieldInfo *info)
{
	switch (info->fieldIndex) {
	case 0:
		info->name = "color";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFNODE;
		info->NDTtype = NDT_SFColorNode;
		info->far_ptr = & ((X_TriangleStripSet *)node)->color;
		return GF_OK;
	case 1:
		info->name = "coord";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFNODE;
		info->NDTtype = NDT_SFCoordinateNode;
		info->far_ptr = & ((X_TriangleStripSet *)node)->coord;
		return GF_OK;
	case 2:
		info->name = "normal";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFNODE;
		info->NDTtype = NDT_SFNormalNode;
		info->far_ptr = & ((X_TriangleStripSet *)node)->normal;
		return GF_OK;
	case 3:
		info->name = "stripCount";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_MFINT32;
		info->far_ptr = & ((X_TriangleStripSet *) node)->stripCount;
		return GF_OK;
	case 4:
		info->name = "texCoord";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFNODE;
		info->NDTtype = NDT_SFTextureCoordinateNode;
		info->far_ptr = & ((X_TriangleStripSet *)node)->texCoord;
		return GF_OK;
	case 5:
		info->name = "ccw";
		info->eventType = GF_SG_EVENT_FIELD;
		info->fieldType = GF_SG_VRML_SFBOOL;
		info->far_ptr = & ((X_TriangleStripSet *) node)->ccw;
		return GF_OK;
	case 6:
		info->name = "colorPerVertex";
		info->eventType = GF_SG_EVENT_FIELD;
		info->fieldType = GF_SG_VRML_SFBOOL;
		info->far_ptr = & ((X_TriangleStripSet *) node)->colorPerVertex;
		return GF_OK;
	case 7:
		info->name = "normalPerVertex";
		info->eventType = GF_SG_EVENT_FIELD;
		info->fieldType = GF_SG_VRML_SFBOOL;
		info->far_ptr = & ((X_TriangleStripSet *) node)->normalPerVertex;
		return GF_OK;
	case 8:
		info->name = "solid";
		info->eventType = GF_SG_EVENT_FIELD;
		info->fieldType = GF_SG_VRML_SFBOOL;
		info->far_ptr = & ((X_TriangleStripSet *) node)->solid;
		return GF_OK;
	case 9:
		info->name = "metadata";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFNODE;
		info->NDTtype = NDT_SFMetadataNode;
		info->far_ptr = & ((X_TriangleStripSet *)node)->metadata;
		return GF_OK;
	default:
		return GF_BAD_PARAM;
	}
}


static s32 TriangleStripSet_get_field_index_by_name(char *name)
{
	if (!strcmp("color", name)) return 0;
	if (!strcmp("coord", name)) return 1;
	if (!strcmp("normal", name)) return 2;
	if (!strcmp("stripCount", name)) return 3;
	if (!strcmp("texCoord", name)) return 4;
	if (!strcmp("ccw", name)) return 5;
	if (!strcmp("colorPerVertex", name)) return 6;
	if (!strcmp("normalPerVertex", name)) return 7;
	if (!strcmp("solid", name)) return 8;
	if (!strcmp("metadata", name)) return 9;
	return -1;
	}


static GF_Node *TriangleStripSet_Create()
{
	X_TriangleStripSet *p;
	GF_SAFEALLOC(p, X_TriangleStripSet);
	if(!p) return NULL;
	gf_node_setup((GF_Node *)p, TAG_X3D_TriangleStripSet);

	/*default field values*/
	p->ccw = 1;
	p->colorPerVertex = 1;
	p->normalPerVertex = 1;
	p->solid = 1;
	return (GF_Node *)p;
}


/*
	Viewpoint Node deletion
*/

static void Viewpoint_Del(GF_Node *node)
{
	X_Viewpoint *p = (X_Viewpoint *) node;
	gf_sg_sfstring_del(p->description);
	gf_node_unregister((GF_Node *) p->metadata, node);	
	gf_node_free((GF_Node *)p);
}


static u32 Viewpoint_get_field_count(GF_Node *node, u8 dummy)
{
	return 10;
}

static GF_Err Viewpoint_get_field(GF_Node *node, GF_FieldInfo *info)
{
	switch (info->fieldIndex) {
	case 0:
		info->name = "set_bind";
		info->eventType = GF_SG_EVENT_IN;
		info->on_event_in = ((X_Viewpoint *)node)->on_set_bind;
		info->fieldType = GF_SG_VRML_SFBOOL;
		info->far_ptr = & ((X_Viewpoint *) node)->set_bind;
		return GF_OK;
	case 1:
		info->name = "fieldOfView";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFFLOAT;
		info->far_ptr = & ((X_Viewpoint *) node)->fieldOfView;
		return GF_OK;
	case 2:
		info->name = "jump";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFBOOL;
		info->far_ptr = & ((X_Viewpoint *) node)->jump;
		return GF_OK;
	case 3:
		info->name = "orientation";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFROTATION;
		info->far_ptr = & ((X_Viewpoint *) node)->orientation;
		return GF_OK;
	case 4:
		info->name = "position";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFVEC3F;
		info->far_ptr = & ((X_Viewpoint *) node)->position;
		return GF_OK;
	case 5:
		info->name = "description";
		info->eventType = GF_SG_EVENT_FIELD;
		info->fieldType = GF_SG_VRML_SFSTRING;
		info->far_ptr = & ((X_Viewpoint *) node)->description;
		return GF_OK;
	case 6:
		info->name = "bindTime";
		info->eventType = GF_SG_EVENT_OUT;
		info->fieldType = GF_SG_VRML_SFTIME;
		info->far_ptr = & ((X_Viewpoint *) node)->bindTime;
		return GF_OK;
	case 7:
		info->name = "isBound";
		info->eventType = GF_SG_EVENT_OUT;
		info->fieldType = GF_SG_VRML_SFBOOL;
		info->far_ptr = & ((X_Viewpoint *) node)->isBound;
		return GF_OK;
	case 8:
		info->name = "metadata";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFNODE;
		info->NDTtype = NDT_SFMetadataNode;
		info->far_ptr = & ((X_Viewpoint *)node)->metadata;
		return GF_OK;
	case 9:
		info->name = "centerOfRotation";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFVEC3F;
		info->far_ptr = & ((X_Viewpoint *) node)->centerOfRotation;
		return GF_OK;
	default:
		return GF_BAD_PARAM;
	}
}


static s32 Viewpoint_get_field_index_by_name(char *name)
{
	if (!strcmp("set_bind", name)) return 0;
	if (!strcmp("fieldOfView", name)) return 1;
	if (!strcmp("jump", name)) return 2;
	if (!strcmp("orientation", name)) return 3;
	if (!strcmp("position", name)) return 4;
	if (!strcmp("description", name)) return 5;
	if (!strcmp("bindTime", name)) return 6;
	if (!strcmp("isBound", name)) return 7;
	if (!strcmp("metadata", name)) return 8;
	if (!strcmp("centerOfRotation", name)) return 9;
	return -1;
	}


static GF_Node *Viewpoint_Create()
{
	X_Viewpoint *p;
	GF_SAFEALLOC(p, X_Viewpoint);
	if(!p) return NULL;
	gf_node_setup((GF_Node *)p, TAG_X3D_Viewpoint);

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
	p->centerOfRotation.x = FLT2FIX(0);
	p->centerOfRotation.y = FLT2FIX(0);
	p->centerOfRotation.z = FLT2FIX(0);
	return (GF_Node *)p;
}


/*
	VisibilitySensor Node deletion
*/

static void VisibilitySensor_Del(GF_Node *node)
{
	X_VisibilitySensor *p = (X_VisibilitySensor *) node;
	gf_node_unregister((GF_Node *) p->metadata, node);	
	gf_node_free((GF_Node *)p);
}


static u32 VisibilitySensor_get_field_count(GF_Node *node, u8 dummy)
{
	return 7;
}

static GF_Err VisibilitySensor_get_field(GF_Node *node, GF_FieldInfo *info)
{
	switch (info->fieldIndex) {
	case 0:
		info->name = "center";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFVEC3F;
		info->far_ptr = & ((X_VisibilitySensor *) node)->center;
		return GF_OK;
	case 1:
		info->name = "enabled";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFBOOL;
		info->far_ptr = & ((X_VisibilitySensor *) node)->enabled;
		return GF_OK;
	case 2:
		info->name = "size";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFVEC3F;
		info->far_ptr = & ((X_VisibilitySensor *) node)->size;
		return GF_OK;
	case 3:
		info->name = "enterTime";
		info->eventType = GF_SG_EVENT_OUT;
		info->fieldType = GF_SG_VRML_SFTIME;
		info->far_ptr = & ((X_VisibilitySensor *) node)->enterTime;
		return GF_OK;
	case 4:
		info->name = "exitTime";
		info->eventType = GF_SG_EVENT_OUT;
		info->fieldType = GF_SG_VRML_SFTIME;
		info->far_ptr = & ((X_VisibilitySensor *) node)->exitTime;
		return GF_OK;
	case 5:
		info->name = "isActive";
		info->eventType = GF_SG_EVENT_OUT;
		info->fieldType = GF_SG_VRML_SFBOOL;
		info->far_ptr = & ((X_VisibilitySensor *) node)->isActive;
		return GF_OK;
	case 6:
		info->name = "metadata";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFNODE;
		info->NDTtype = NDT_SFMetadataNode;
		info->far_ptr = & ((X_VisibilitySensor *)node)->metadata;
		return GF_OK;
	default:
		return GF_BAD_PARAM;
	}
}


static s32 VisibilitySensor_get_field_index_by_name(char *name)
{
	if (!strcmp("center", name)) return 0;
	if (!strcmp("enabled", name)) return 1;
	if (!strcmp("size", name)) return 2;
	if (!strcmp("enterTime", name)) return 3;
	if (!strcmp("exitTime", name)) return 4;
	if (!strcmp("isActive", name)) return 5;
	if (!strcmp("metadata", name)) return 6;
	return -1;
	}


static GF_Node *VisibilitySensor_Create()
{
	X_VisibilitySensor *p;
	GF_SAFEALLOC(p, X_VisibilitySensor);
	if(!p) return NULL;
	gf_node_setup((GF_Node *)p, TAG_X3D_VisibilitySensor);

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
	X_WorldInfo *p = (X_WorldInfo *) node;
	gf_sg_mfstring_del(p->info);
	gf_sg_sfstring_del(p->title);
	gf_node_unregister((GF_Node *) p->metadata, node);	
	gf_node_free((GF_Node *)p);
}


static u32 WorldInfo_get_field_count(GF_Node *node, u8 dummy)
{
	return 3;
}

static GF_Err WorldInfo_get_field(GF_Node *node, GF_FieldInfo *info)
{
	switch (info->fieldIndex) {
	case 0:
		info->name = "info";
		info->eventType = GF_SG_EVENT_FIELD;
		info->fieldType = GF_SG_VRML_MFSTRING;
		info->far_ptr = & ((X_WorldInfo *) node)->info;
		return GF_OK;
	case 1:
		info->name = "title";
		info->eventType = GF_SG_EVENT_FIELD;
		info->fieldType = GF_SG_VRML_SFSTRING;
		info->far_ptr = & ((X_WorldInfo *) node)->title;
		return GF_OK;
	case 2:
		info->name = "metadata";
		info->eventType = GF_SG_EVENT_EXPOSED_FIELD;
		info->fieldType = GF_SG_VRML_SFNODE;
		info->NDTtype = NDT_SFMetadataNode;
		info->far_ptr = & ((X_WorldInfo *)node)->metadata;
		return GF_OK;
	default:
		return GF_BAD_PARAM;
	}
}


static s32 WorldInfo_get_field_index_by_name(char *name)
{
	if (!strcmp("info", name)) return 0;
	if (!strcmp("title", name)) return 1;
	if (!strcmp("metadata", name)) return 2;
	return -1;
	}


static GF_Node *WorldInfo_Create()
{
	X_WorldInfo *p;
	GF_SAFEALLOC(p, X_WorldInfo);
	if(!p) return NULL;
	gf_node_setup((GF_Node *)p, TAG_X3D_WorldInfo);

	/*default field values*/
	return (GF_Node *)p;
}




GF_Node *gf_sg_x3d_node_new(u32 NodeTag)
{
	switch (NodeTag) {
	case TAG_X3D_Anchor:
		return Anchor_Create();
	case TAG_X3D_Appearance:
		return Appearance_Create();
	case TAG_X3D_Arc2D:
		return Arc2D_Create();
	case TAG_X3D_ArcClose2D:
		return ArcClose2D_Create();
	case TAG_X3D_AudioClip:
		return AudioClip_Create();
	case TAG_X3D_Background:
		return Background_Create();
	case TAG_X3D_Billboard:
		return Billboard_Create();
	case TAG_X3D_BooleanFilter:
		return BooleanFilter_Create();
	case TAG_X3D_BooleanSequencer:
		return BooleanSequencer_Create();
	case TAG_X3D_BooleanToggle:
		return BooleanToggle_Create();
	case TAG_X3D_BooleanTrigger:
		return BooleanTrigger_Create();
	case TAG_X3D_Box:
		return Box_Create();
	case TAG_X3D_Circle2D:
		return Circle2D_Create();
	case TAG_X3D_Collision:
		return Collision_Create();
	case TAG_X3D_Color:
		return Color_Create();
	case TAG_X3D_ColorInterpolator:
		return ColorInterpolator_Create();
	case TAG_X3D_ColorRGBA:
		return ColorRGBA_Create();
	case TAG_X3D_Cone:
		return Cone_Create();
	case TAG_X3D_Contour2D:
		return Contour2D_Create();
	case TAG_X3D_ContourPolyline2D:
		return ContourPolyline2D_Create();
	case TAG_X3D_Coordinate:
		return Coordinate_Create();
	case TAG_X3D_CoordinateDouble:
		return CoordinateDouble_Create();
	case TAG_X3D_Coordinate2D:
		return Coordinate2D_Create();
	case TAG_X3D_CoordinateInterpolator:
		return CoordinateInterpolator_Create();
	case TAG_X3D_CoordinateInterpolator2D:
		return CoordinateInterpolator2D_Create();
	case TAG_X3D_Cylinder:
		return Cylinder_Create();
	case TAG_X3D_CylinderSensor:
		return CylinderSensor_Create();
	case TAG_X3D_DirectionalLight:
		return DirectionalLight_Create();
	case TAG_X3D_Disk2D:
		return Disk2D_Create();
	case TAG_X3D_ElevationGrid:
		return ElevationGrid_Create();
	case TAG_X3D_Extrusion:
		return Extrusion_Create();
	case TAG_X3D_FillProperties:
		return FillProperties_Create();
	case TAG_X3D_Fog:
		return Fog_Create();
	case TAG_X3D_FontStyle:
		return FontStyle_Create();
	case TAG_X3D_Group:
		return Group_Create();
	case TAG_X3D_ImageTexture:
		return ImageTexture_Create();
	case TAG_X3D_IndexedFaceSet:
		return IndexedFaceSet_Create();
	case TAG_X3D_IndexedLineSet:
		return IndexedLineSet_Create();
	case TAG_X3D_IndexedTriangleFanSet:
		return IndexedTriangleFanSet_Create();
	case TAG_X3D_IndexedTriangleSet:
		return IndexedTriangleSet_Create();
	case TAG_X3D_IndexedTriangleStripSet:
		return IndexedTriangleStripSet_Create();
	case TAG_X3D_Inline:
		return Inline_Create();
	case TAG_X3D_IntegerSequencer:
		return IntegerSequencer_Create();
	case TAG_X3D_IntegerTrigger:
		return IntegerTrigger_Create();
	case TAG_X3D_KeySensor:
		return KeySensor_Create();
	case TAG_X3D_LineProperties:
		return LineProperties_Create();
	case TAG_X3D_LineSet:
		return LineSet_Create();
	case TAG_X3D_LOD:
		return LOD_Create();
	case TAG_X3D_Material:
		return Material_Create();
	case TAG_X3D_MetadataDouble:
		return MetadataDouble_Create();
	case TAG_X3D_MetadataFloat:
		return MetadataFloat_Create();
	case TAG_X3D_MetadataInteger:
		return MetadataInteger_Create();
	case TAG_X3D_MetadataSet:
		return MetadataSet_Create();
	case TAG_X3D_MetadataString:
		return MetadataString_Create();
	case TAG_X3D_MovieTexture:
		return MovieTexture_Create();
	case TAG_X3D_MultiTexture:
		return MultiTexture_Create();
	case TAG_X3D_MultiTextureCoordinate:
		return MultiTextureCoordinate_Create();
	case TAG_X3D_MultiTextureTransform:
		return MultiTextureTransform_Create();
	case TAG_X3D_NavigationInfo:
		return NavigationInfo_Create();
	case TAG_X3D_Normal:
		return Normal_Create();
	case TAG_X3D_NormalInterpolator:
		return NormalInterpolator_Create();
	case TAG_X3D_OrientationInterpolator:
		return OrientationInterpolator_Create();
	case TAG_X3D_PixelTexture:
		return PixelTexture_Create();
	case TAG_X3D_PlaneSensor:
		return PlaneSensor_Create();
	case TAG_X3D_PointLight:
		return PointLight_Create();
	case TAG_X3D_PointSet:
		return PointSet_Create();
	case TAG_X3D_Polyline2D:
		return Polyline2D_Create();
	case TAG_X3D_Polypoint2D:
		return Polypoint2D_Create();
	case TAG_X3D_PositionInterpolator:
		return PositionInterpolator_Create();
	case TAG_X3D_PositionInterpolator2D:
		return PositionInterpolator2D_Create();
	case TAG_X3D_ProximitySensor:
		return ProximitySensor_Create();
	case TAG_X3D_Rectangle2D:
		return Rectangle2D_Create();
	case TAG_X3D_ScalarInterpolator:
		return ScalarInterpolator_Create();
	case TAG_X3D_Script:
		return Script_Create();
	case TAG_X3D_Shape:
		return Shape_Create();
	case TAG_X3D_Sound:
		return Sound_Create();
	case TAG_X3D_Sphere:
		return Sphere_Create();
	case TAG_X3D_SphereSensor:
		return SphereSensor_Create();
	case TAG_X3D_SpotLight:
		return SpotLight_Create();
	case TAG_X3D_StaticGroup:
		return StaticGroup_Create();
	case TAG_X3D_StringSensor:
		return StringSensor_Create();
	case TAG_X3D_Switch:
		return Switch_Create();
	case TAG_X3D_Text:
		return Text_Create();
	case TAG_X3D_TextureBackground:
		return TextureBackground_Create();
	case TAG_X3D_TextureCoordinate:
		return TextureCoordinate_Create();
	case TAG_X3D_TextureCoordinateGenerator:
		return TextureCoordinateGenerator_Create();
	case TAG_X3D_TextureTransform:
		return TextureTransform_Create();
	case TAG_X3D_TimeSensor:
		return TimeSensor_Create();
	case TAG_X3D_TimeTrigger:
		return TimeTrigger_Create();
	case TAG_X3D_TouchSensor:
		return TouchSensor_Create();
	case TAG_X3D_Transform:
		return Transform_Create();
	case TAG_X3D_TriangleFanSet:
		return TriangleFanSet_Create();
	case TAG_X3D_TriangleSet:
		return TriangleSet_Create();
	case TAG_X3D_TriangleSet2D:
		return TriangleSet2D_Create();
	case TAG_X3D_TriangleStripSet:
		return TriangleStripSet_Create();
	case TAG_X3D_Viewpoint:
		return Viewpoint_Create();
	case TAG_X3D_VisibilitySensor:
		return VisibilitySensor_Create();
	case TAG_X3D_WorldInfo:
		return WorldInfo_Create();
	default:
		return NULL;
	}
}

const char *gf_sg_x3d_node_get_class_name(u32 NodeTag)
{
	switch (NodeTag) {
	case TAG_X3D_Anchor:
		return "Anchor";
	case TAG_X3D_Appearance:
		return "Appearance";
	case TAG_X3D_Arc2D:
		return "Arc2D";
	case TAG_X3D_ArcClose2D:
		return "ArcClose2D";
	case TAG_X3D_AudioClip:
		return "AudioClip";
	case TAG_X3D_Background:
		return "Background";
	case TAG_X3D_Billboard:
		return "Billboard";
	case TAG_X3D_BooleanFilter:
		return "BooleanFilter";
	case TAG_X3D_BooleanSequencer:
		return "BooleanSequencer";
	case TAG_X3D_BooleanToggle:
		return "BooleanToggle";
	case TAG_X3D_BooleanTrigger:
		return "BooleanTrigger";
	case TAG_X3D_Box:
		return "Box";
	case TAG_X3D_Circle2D:
		return "Circle2D";
	case TAG_X3D_Collision:
		return "Collision";
	case TAG_X3D_Color:
		return "Color";
	case TAG_X3D_ColorInterpolator:
		return "ColorInterpolator";
	case TAG_X3D_ColorRGBA:
		return "ColorRGBA";
	case TAG_X3D_Cone:
		return "Cone";
	case TAG_X3D_Contour2D:
		return "Contour2D";
	case TAG_X3D_ContourPolyline2D:
		return "ContourPolyline2D";
	case TAG_X3D_Coordinate:
		return "Coordinate";
	case TAG_X3D_CoordinateDouble:
		return "CoordinateDouble";
	case TAG_X3D_Coordinate2D:
		return "Coordinate2D";
	case TAG_X3D_CoordinateInterpolator:
		return "CoordinateInterpolator";
	case TAG_X3D_CoordinateInterpolator2D:
		return "CoordinateInterpolator2D";
	case TAG_X3D_Cylinder:
		return "Cylinder";
	case TAG_X3D_CylinderSensor:
		return "CylinderSensor";
	case TAG_X3D_DirectionalLight:
		return "DirectionalLight";
	case TAG_X3D_Disk2D:
		return "Disk2D";
	case TAG_X3D_ElevationGrid:
		return "ElevationGrid";
	case TAG_X3D_Extrusion:
		return "Extrusion";
	case TAG_X3D_FillProperties:
		return "FillProperties";
	case TAG_X3D_Fog:
		return "Fog";
	case TAG_X3D_FontStyle:
		return "FontStyle";
	case TAG_X3D_Group:
		return "Group";
	case TAG_X3D_ImageTexture:
		return "ImageTexture";
	case TAG_X3D_IndexedFaceSet:
		return "IndexedFaceSet";
	case TAG_X3D_IndexedLineSet:
		return "IndexedLineSet";
	case TAG_X3D_IndexedTriangleFanSet:
		return "IndexedTriangleFanSet";
	case TAG_X3D_IndexedTriangleSet:
		return "IndexedTriangleSet";
	case TAG_X3D_IndexedTriangleStripSet:
		return "IndexedTriangleStripSet";
	case TAG_X3D_Inline:
		return "Inline";
	case TAG_X3D_IntegerSequencer:
		return "IntegerSequencer";
	case TAG_X3D_IntegerTrigger:
		return "IntegerTrigger";
	case TAG_X3D_KeySensor:
		return "KeySensor";
	case TAG_X3D_LineProperties:
		return "LineProperties";
	case TAG_X3D_LineSet:
		return "LineSet";
	case TAG_X3D_LOD:
		return "LOD";
	case TAG_X3D_Material:
		return "Material";
	case TAG_X3D_MetadataDouble:
		return "MetadataDouble";
	case TAG_X3D_MetadataFloat:
		return "MetadataFloat";
	case TAG_X3D_MetadataInteger:
		return "MetadataInteger";
	case TAG_X3D_MetadataSet:
		return "MetadataSet";
	case TAG_X3D_MetadataString:
		return "MetadataString";
	case TAG_X3D_MovieTexture:
		return "MovieTexture";
	case TAG_X3D_MultiTexture:
		return "MultiTexture";
	case TAG_X3D_MultiTextureCoordinate:
		return "MultiTextureCoordinate";
	case TAG_X3D_MultiTextureTransform:
		return "MultiTextureTransform";
	case TAG_X3D_NavigationInfo:
		return "NavigationInfo";
	case TAG_X3D_Normal:
		return "Normal";
	case TAG_X3D_NormalInterpolator:
		return "NormalInterpolator";
	case TAG_X3D_OrientationInterpolator:
		return "OrientationInterpolator";
	case TAG_X3D_PixelTexture:
		return "PixelTexture";
	case TAG_X3D_PlaneSensor:
		return "PlaneSensor";
	case TAG_X3D_PointLight:
		return "PointLight";
	case TAG_X3D_PointSet:
		return "PointSet";
	case TAG_X3D_Polyline2D:
		return "Polyline2D";
	case TAG_X3D_Polypoint2D:
		return "Polypoint2D";
	case TAG_X3D_PositionInterpolator:
		return "PositionInterpolator";
	case TAG_X3D_PositionInterpolator2D:
		return "PositionInterpolator2D";
	case TAG_X3D_ProximitySensor:
		return "ProximitySensor";
	case TAG_X3D_Rectangle2D:
		return "Rectangle2D";
	case TAG_X3D_ScalarInterpolator:
		return "ScalarInterpolator";
	case TAG_X3D_Script:
		return "Script";
	case TAG_X3D_Shape:
		return "Shape";
	case TAG_X3D_Sound:
		return "Sound";
	case TAG_X3D_Sphere:
		return "Sphere";
	case TAG_X3D_SphereSensor:
		return "SphereSensor";
	case TAG_X3D_SpotLight:
		return "SpotLight";
	case TAG_X3D_StaticGroup:
		return "StaticGroup";
	case TAG_X3D_StringSensor:
		return "StringSensor";
	case TAG_X3D_Switch:
		return "Switch";
	case TAG_X3D_Text:
		return "Text";
	case TAG_X3D_TextureBackground:
		return "TextureBackground";
	case TAG_X3D_TextureCoordinate:
		return "TextureCoordinate";
	case TAG_X3D_TextureCoordinateGenerator:
		return "TextureCoordinateGenerator";
	case TAG_X3D_TextureTransform:
		return "TextureTransform";
	case TAG_X3D_TimeSensor:
		return "TimeSensor";
	case TAG_X3D_TimeTrigger:
		return "TimeTrigger";
	case TAG_X3D_TouchSensor:
		return "TouchSensor";
	case TAG_X3D_Transform:
		return "Transform";
	case TAG_X3D_TriangleFanSet:
		return "TriangleFanSet";
	case TAG_X3D_TriangleSet:
		return "TriangleSet";
	case TAG_X3D_TriangleSet2D:
		return "TriangleSet2D";
	case TAG_X3D_TriangleStripSet:
		return "TriangleStripSet";
	case TAG_X3D_Viewpoint:
		return "Viewpoint";
	case TAG_X3D_VisibilitySensor:
		return "VisibilitySensor";
	case TAG_X3D_WorldInfo:
		return "WorldInfo";
	default:
		return "Unknown Node";
	}
}

void gf_sg_x3d_node_del(GF_Node *node)
{
	switch (node->sgprivate->tag) {
	case TAG_X3D_Anchor:
		Anchor_Del(node); return;
	case TAG_X3D_Appearance:
		Appearance_Del(node); return;
	case TAG_X3D_Arc2D:
		Arc2D_Del(node); return;
	case TAG_X3D_ArcClose2D:
		ArcClose2D_Del(node); return;
	case TAG_X3D_AudioClip:
		AudioClip_Del(node); return;
	case TAG_X3D_Background:
		Background_Del(node); return;
	case TAG_X3D_Billboard:
		Billboard_Del(node); return;
	case TAG_X3D_BooleanFilter:
		BooleanFilter_Del(node); return;
	case TAG_X3D_BooleanSequencer:
		BooleanSequencer_Del(node); return;
	case TAG_X3D_BooleanToggle:
		BooleanToggle_Del(node); return;
	case TAG_X3D_BooleanTrigger:
		BooleanTrigger_Del(node); return;
	case TAG_X3D_Box:
		Box_Del(node); return;
	case TAG_X3D_Circle2D:
		Circle2D_Del(node); return;
	case TAG_X3D_Collision:
		Collision_Del(node); return;
	case TAG_X3D_Color:
		Color_Del(node); return;
	case TAG_X3D_ColorInterpolator:
		ColorInterpolator_Del(node); return;
	case TAG_X3D_ColorRGBA:
		ColorRGBA_Del(node); return;
	case TAG_X3D_Cone:
		Cone_Del(node); return;
	case TAG_X3D_Contour2D:
		Contour2D_Del(node); return;
	case TAG_X3D_ContourPolyline2D:
		ContourPolyline2D_Del(node); return;
	case TAG_X3D_Coordinate:
		Coordinate_Del(node); return;
	case TAG_X3D_CoordinateDouble:
		CoordinateDouble_Del(node); return;
	case TAG_X3D_Coordinate2D:
		Coordinate2D_Del(node); return;
	case TAG_X3D_CoordinateInterpolator:
		CoordinateInterpolator_Del(node); return;
	case TAG_X3D_CoordinateInterpolator2D:
		CoordinateInterpolator2D_Del(node); return;
	case TAG_X3D_Cylinder:
		Cylinder_Del(node); return;
	case TAG_X3D_CylinderSensor:
		CylinderSensor_Del(node); return;
	case TAG_X3D_DirectionalLight:
		DirectionalLight_Del(node); return;
	case TAG_X3D_Disk2D:
		Disk2D_Del(node); return;
	case TAG_X3D_ElevationGrid:
		ElevationGrid_Del(node); return;
	case TAG_X3D_Extrusion:
		Extrusion_Del(node); return;
	case TAG_X3D_FillProperties:
		FillProperties_Del(node); return;
	case TAG_X3D_Fog:
		Fog_Del(node); return;
	case TAG_X3D_FontStyle:
		FontStyle_Del(node); return;
	case TAG_X3D_Group:
		Group_Del(node); return;
	case TAG_X3D_ImageTexture:
		ImageTexture_Del(node); return;
	case TAG_X3D_IndexedFaceSet:
		IndexedFaceSet_Del(node); return;
	case TAG_X3D_IndexedLineSet:
		IndexedLineSet_Del(node); return;
	case TAG_X3D_IndexedTriangleFanSet:
		IndexedTriangleFanSet_Del(node); return;
	case TAG_X3D_IndexedTriangleSet:
		IndexedTriangleSet_Del(node); return;
	case TAG_X3D_IndexedTriangleStripSet:
		IndexedTriangleStripSet_Del(node); return;
	case TAG_X3D_Inline:
		Inline_Del(node); return;
	case TAG_X3D_IntegerSequencer:
		IntegerSequencer_Del(node); return;
	case TAG_X3D_IntegerTrigger:
		IntegerTrigger_Del(node); return;
	case TAG_X3D_KeySensor:
		KeySensor_Del(node); return;
	case TAG_X3D_LineProperties:
		LineProperties_Del(node); return;
	case TAG_X3D_LineSet:
		LineSet_Del(node); return;
	case TAG_X3D_LOD:
		LOD_Del(node); return;
	case TAG_X3D_Material:
		Material_Del(node); return;
	case TAG_X3D_MetadataDouble:
		MetadataDouble_Del(node); return;
	case TAG_X3D_MetadataFloat:
		MetadataFloat_Del(node); return;
	case TAG_X3D_MetadataInteger:
		MetadataInteger_Del(node); return;
	case TAG_X3D_MetadataSet:
		MetadataSet_Del(node); return;
	case TAG_X3D_MetadataString:
		MetadataString_Del(node); return;
	case TAG_X3D_MovieTexture:
		MovieTexture_Del(node); return;
	case TAG_X3D_MultiTexture:
		MultiTexture_Del(node); return;
	case TAG_X3D_MultiTextureCoordinate:
		MultiTextureCoordinate_Del(node); return;
	case TAG_X3D_MultiTextureTransform:
		MultiTextureTransform_Del(node); return;
	case TAG_X3D_NavigationInfo:
		NavigationInfo_Del(node); return;
	case TAG_X3D_Normal:
		Normal_Del(node); return;
	case TAG_X3D_NormalInterpolator:
		NormalInterpolator_Del(node); return;
	case TAG_X3D_OrientationInterpolator:
		OrientationInterpolator_Del(node); return;
	case TAG_X3D_PixelTexture:
		PixelTexture_Del(node); return;
	case TAG_X3D_PlaneSensor:
		PlaneSensor_Del(node); return;
	case TAG_X3D_PointLight:
		PointLight_Del(node); return;
	case TAG_X3D_PointSet:
		PointSet_Del(node); return;
	case TAG_X3D_Polyline2D:
		Polyline2D_Del(node); return;
	case TAG_X3D_Polypoint2D:
		Polypoint2D_Del(node); return;
	case TAG_X3D_PositionInterpolator:
		PositionInterpolator_Del(node); return;
	case TAG_X3D_PositionInterpolator2D:
		PositionInterpolator2D_Del(node); return;
	case TAG_X3D_ProximitySensor:
		ProximitySensor_Del(node); return;
	case TAG_X3D_Rectangle2D:
		Rectangle2D_Del(node); return;
	case TAG_X3D_ScalarInterpolator:
		ScalarInterpolator_Del(node); return;
	case TAG_X3D_Script:
		Script_Del(node); return;
	case TAG_X3D_Shape:
		Shape_Del(node); return;
	case TAG_X3D_Sound:
		Sound_Del(node); return;
	case TAG_X3D_Sphere:
		Sphere_Del(node); return;
	case TAG_X3D_SphereSensor:
		SphereSensor_Del(node); return;
	case TAG_X3D_SpotLight:
		SpotLight_Del(node); return;
	case TAG_X3D_StaticGroup:
		StaticGroup_Del(node); return;
	case TAG_X3D_StringSensor:
		StringSensor_Del(node); return;
	case TAG_X3D_Switch:
		Switch_Del(node); return;
	case TAG_X3D_Text:
		Text_Del(node); return;
	case TAG_X3D_TextureBackground:
		TextureBackground_Del(node); return;
	case TAG_X3D_TextureCoordinate:
		TextureCoordinate_Del(node); return;
	case TAG_X3D_TextureCoordinateGenerator:
		TextureCoordinateGenerator_Del(node); return;
	case TAG_X3D_TextureTransform:
		TextureTransform_Del(node); return;
	case TAG_X3D_TimeSensor:
		TimeSensor_Del(node); return;
	case TAG_X3D_TimeTrigger:
		TimeTrigger_Del(node); return;
	case TAG_X3D_TouchSensor:
		TouchSensor_Del(node); return;
	case TAG_X3D_Transform:
		Transform_Del(node); return;
	case TAG_X3D_TriangleFanSet:
		TriangleFanSet_Del(node); return;
	case TAG_X3D_TriangleSet:
		TriangleSet_Del(node); return;
	case TAG_X3D_TriangleSet2D:
		TriangleSet2D_Del(node); return;
	case TAG_X3D_TriangleStripSet:
		TriangleStripSet_Del(node); return;
	case TAG_X3D_Viewpoint:
		Viewpoint_Del(node); return;
	case TAG_X3D_VisibilitySensor:
		VisibilitySensor_Del(node); return;
	case TAG_X3D_WorldInfo:
		WorldInfo_Del(node); return;
	default:
		return;
	}
}

u32 gf_sg_x3d_node_get_field_count(GF_Node *node)
{
	switch (node->sgprivate->tag) {
	case TAG_X3D_Anchor:return Anchor_get_field_count(node, 0);
	case TAG_X3D_Appearance:return Appearance_get_field_count(node, 0);
	case TAG_X3D_Arc2D:return Arc2D_get_field_count(node, 0);
	case TAG_X3D_ArcClose2D:return ArcClose2D_get_field_count(node, 0);
	case TAG_X3D_AudioClip:return AudioClip_get_field_count(node, 0);
	case TAG_X3D_Background:return Background_get_field_count(node, 0);
	case TAG_X3D_Billboard:return Billboard_get_field_count(node, 0);
	case TAG_X3D_BooleanFilter:return BooleanFilter_get_field_count(node, 0);
	case TAG_X3D_BooleanSequencer:return BooleanSequencer_get_field_count(node, 0);
	case TAG_X3D_BooleanToggle:return BooleanToggle_get_field_count(node, 0);
	case TAG_X3D_BooleanTrigger:return BooleanTrigger_get_field_count(node, 0);
	case TAG_X3D_Box:return Box_get_field_count(node, 0);
	case TAG_X3D_Circle2D:return Circle2D_get_field_count(node, 0);
	case TAG_X3D_Collision:return Collision_get_field_count(node, 0);
	case TAG_X3D_Color:return Color_get_field_count(node, 0);
	case TAG_X3D_ColorInterpolator:return ColorInterpolator_get_field_count(node, 0);
	case TAG_X3D_ColorRGBA:return ColorRGBA_get_field_count(node, 0);
	case TAG_X3D_Cone:return Cone_get_field_count(node, 0);
	case TAG_X3D_Contour2D:return Contour2D_get_field_count(node, 0);
	case TAG_X3D_ContourPolyline2D:return ContourPolyline2D_get_field_count(node, 0);
	case TAG_X3D_Coordinate:return Coordinate_get_field_count(node, 0);
	case TAG_X3D_CoordinateDouble:return CoordinateDouble_get_field_count(node, 0);
	case TAG_X3D_Coordinate2D:return Coordinate2D_get_field_count(node, 0);
	case TAG_X3D_CoordinateInterpolator:return CoordinateInterpolator_get_field_count(node, 0);
	case TAG_X3D_CoordinateInterpolator2D:return CoordinateInterpolator2D_get_field_count(node, 0);
	case TAG_X3D_Cylinder:return Cylinder_get_field_count(node, 0);
	case TAG_X3D_CylinderSensor:return CylinderSensor_get_field_count(node, 0);
	case TAG_X3D_DirectionalLight:return DirectionalLight_get_field_count(node, 0);
	case TAG_X3D_Disk2D:return Disk2D_get_field_count(node, 0);
	case TAG_X3D_ElevationGrid:return ElevationGrid_get_field_count(node, 0);
	case TAG_X3D_Extrusion:return Extrusion_get_field_count(node, 0);
	case TAG_X3D_FillProperties:return FillProperties_get_field_count(node, 0);
	case TAG_X3D_Fog:return Fog_get_field_count(node, 0);
	case TAG_X3D_FontStyle:return FontStyle_get_field_count(node, 0);
	case TAG_X3D_Group:return Group_get_field_count(node, 0);
	case TAG_X3D_ImageTexture:return ImageTexture_get_field_count(node, 0);
	case TAG_X3D_IndexedFaceSet:return IndexedFaceSet_get_field_count(node, 0);
	case TAG_X3D_IndexedLineSet:return IndexedLineSet_get_field_count(node, 0);
	case TAG_X3D_IndexedTriangleFanSet:return IndexedTriangleFanSet_get_field_count(node, 0);
	case TAG_X3D_IndexedTriangleSet:return IndexedTriangleSet_get_field_count(node, 0);
	case TAG_X3D_IndexedTriangleStripSet:return IndexedTriangleStripSet_get_field_count(node, 0);
	case TAG_X3D_Inline:return Inline_get_field_count(node, 0);
	case TAG_X3D_IntegerSequencer:return IntegerSequencer_get_field_count(node, 0);
	case TAG_X3D_IntegerTrigger:return IntegerTrigger_get_field_count(node, 0);
	case TAG_X3D_KeySensor:return KeySensor_get_field_count(node, 0);
	case TAG_X3D_LineProperties:return LineProperties_get_field_count(node, 0);
	case TAG_X3D_LineSet:return LineSet_get_field_count(node, 0);
	case TAG_X3D_LOD:return LOD_get_field_count(node, 0);
	case TAG_X3D_Material:return Material_get_field_count(node, 0);
	case TAG_X3D_MetadataDouble:return MetadataDouble_get_field_count(node, 0);
	case TAG_X3D_MetadataFloat:return MetadataFloat_get_field_count(node, 0);
	case TAG_X3D_MetadataInteger:return MetadataInteger_get_field_count(node, 0);
	case TAG_X3D_MetadataSet:return MetadataSet_get_field_count(node, 0);
	case TAG_X3D_MetadataString:return MetadataString_get_field_count(node, 0);
	case TAG_X3D_MovieTexture:return MovieTexture_get_field_count(node, 0);
	case TAG_X3D_MultiTexture:return MultiTexture_get_field_count(node, 0);
	case TAG_X3D_MultiTextureCoordinate:return MultiTextureCoordinate_get_field_count(node, 0);
	case TAG_X3D_MultiTextureTransform:return MultiTextureTransform_get_field_count(node, 0);
	case TAG_X3D_NavigationInfo:return NavigationInfo_get_field_count(node, 0);
	case TAG_X3D_Normal:return Normal_get_field_count(node, 0);
	case TAG_X3D_NormalInterpolator:return NormalInterpolator_get_field_count(node, 0);
	case TAG_X3D_OrientationInterpolator:return OrientationInterpolator_get_field_count(node, 0);
	case TAG_X3D_PixelTexture:return PixelTexture_get_field_count(node, 0);
	case TAG_X3D_PlaneSensor:return PlaneSensor_get_field_count(node, 0);
	case TAG_X3D_PointLight:return PointLight_get_field_count(node, 0);
	case TAG_X3D_PointSet:return PointSet_get_field_count(node, 0);
	case TAG_X3D_Polyline2D:return Polyline2D_get_field_count(node, 0);
	case TAG_X3D_Polypoint2D:return Polypoint2D_get_field_count(node, 0);
	case TAG_X3D_PositionInterpolator:return PositionInterpolator_get_field_count(node, 0);
	case TAG_X3D_PositionInterpolator2D:return PositionInterpolator2D_get_field_count(node, 0);
	case TAG_X3D_ProximitySensor:return ProximitySensor_get_field_count(node, 0);
	case TAG_X3D_Rectangle2D:return Rectangle2D_get_field_count(node, 0);
	case TAG_X3D_ScalarInterpolator:return ScalarInterpolator_get_field_count(node, 0);
	case TAG_X3D_Script:return Script_get_field_count(node, 0);
	case TAG_X3D_Shape:return Shape_get_field_count(node, 0);
	case TAG_X3D_Sound:return Sound_get_field_count(node, 0);
	case TAG_X3D_Sphere:return Sphere_get_field_count(node, 0);
	case TAG_X3D_SphereSensor:return SphereSensor_get_field_count(node, 0);
	case TAG_X3D_SpotLight:return SpotLight_get_field_count(node, 0);
	case TAG_X3D_StaticGroup:return StaticGroup_get_field_count(node, 0);
	case TAG_X3D_StringSensor:return StringSensor_get_field_count(node, 0);
	case TAG_X3D_Switch:return Switch_get_field_count(node, 0);
	case TAG_X3D_Text:return Text_get_field_count(node, 0);
	case TAG_X3D_TextureBackground:return TextureBackground_get_field_count(node, 0);
	case TAG_X3D_TextureCoordinate:return TextureCoordinate_get_field_count(node, 0);
	case TAG_X3D_TextureCoordinateGenerator:return TextureCoordinateGenerator_get_field_count(node, 0);
	case TAG_X3D_TextureTransform:return TextureTransform_get_field_count(node, 0);
	case TAG_X3D_TimeSensor:return TimeSensor_get_field_count(node, 0);
	case TAG_X3D_TimeTrigger:return TimeTrigger_get_field_count(node, 0);
	case TAG_X3D_TouchSensor:return TouchSensor_get_field_count(node, 0);
	case TAG_X3D_Transform:return Transform_get_field_count(node, 0);
	case TAG_X3D_TriangleFanSet:return TriangleFanSet_get_field_count(node, 0);
	case TAG_X3D_TriangleSet:return TriangleSet_get_field_count(node, 0);
	case TAG_X3D_TriangleSet2D:return TriangleSet2D_get_field_count(node, 0);
	case TAG_X3D_TriangleStripSet:return TriangleStripSet_get_field_count(node, 0);
	case TAG_X3D_Viewpoint:return Viewpoint_get_field_count(node, 0);
	case TAG_X3D_VisibilitySensor:return VisibilitySensor_get_field_count(node, 0);
	case TAG_X3D_WorldInfo:return WorldInfo_get_field_count(node, 0);
	default:
		return 0;
	}
}

GF_Err gf_sg_x3d_node_get_field(GF_Node *node, GF_FieldInfo *field)
{
	switch (node->sgprivate->tag) {
	case TAG_X3D_Anchor: return Anchor_get_field(node, field);
	case TAG_X3D_Appearance: return Appearance_get_field(node, field);
	case TAG_X3D_Arc2D: return Arc2D_get_field(node, field);
	case TAG_X3D_ArcClose2D: return ArcClose2D_get_field(node, field);
	case TAG_X3D_AudioClip: return AudioClip_get_field(node, field);
	case TAG_X3D_Background: return Background_get_field(node, field);
	case TAG_X3D_Billboard: return Billboard_get_field(node, field);
	case TAG_X3D_BooleanFilter: return BooleanFilter_get_field(node, field);
	case TAG_X3D_BooleanSequencer: return BooleanSequencer_get_field(node, field);
	case TAG_X3D_BooleanToggle: return BooleanToggle_get_field(node, field);
	case TAG_X3D_BooleanTrigger: return BooleanTrigger_get_field(node, field);
	case TAG_X3D_Box: return Box_get_field(node, field);
	case TAG_X3D_Circle2D: return Circle2D_get_field(node, field);
	case TAG_X3D_Collision: return Collision_get_field(node, field);
	case TAG_X3D_Color: return Color_get_field(node, field);
	case TAG_X3D_ColorInterpolator: return ColorInterpolator_get_field(node, field);
	case TAG_X3D_ColorRGBA: return ColorRGBA_get_field(node, field);
	case TAG_X3D_Cone: return Cone_get_field(node, field);
	case TAG_X3D_Contour2D: return Contour2D_get_field(node, field);
	case TAG_X3D_ContourPolyline2D: return ContourPolyline2D_get_field(node, field);
	case TAG_X3D_Coordinate: return Coordinate_get_field(node, field);
	case TAG_X3D_CoordinateDouble: return CoordinateDouble_get_field(node, field);
	case TAG_X3D_Coordinate2D: return Coordinate2D_get_field(node, field);
	case TAG_X3D_CoordinateInterpolator: return CoordinateInterpolator_get_field(node, field);
	case TAG_X3D_CoordinateInterpolator2D: return CoordinateInterpolator2D_get_field(node, field);
	case TAG_X3D_Cylinder: return Cylinder_get_field(node, field);
	case TAG_X3D_CylinderSensor: return CylinderSensor_get_field(node, field);
	case TAG_X3D_DirectionalLight: return DirectionalLight_get_field(node, field);
	case TAG_X3D_Disk2D: return Disk2D_get_field(node, field);
	case TAG_X3D_ElevationGrid: return ElevationGrid_get_field(node, field);
	case TAG_X3D_Extrusion: return Extrusion_get_field(node, field);
	case TAG_X3D_FillProperties: return FillProperties_get_field(node, field);
	case TAG_X3D_Fog: return Fog_get_field(node, field);
	case TAG_X3D_FontStyle: return FontStyle_get_field(node, field);
	case TAG_X3D_Group: return Group_get_field(node, field);
	case TAG_X3D_ImageTexture: return ImageTexture_get_field(node, field);
	case TAG_X3D_IndexedFaceSet: return IndexedFaceSet_get_field(node, field);
	case TAG_X3D_IndexedLineSet: return IndexedLineSet_get_field(node, field);
	case TAG_X3D_IndexedTriangleFanSet: return IndexedTriangleFanSet_get_field(node, field);
	case TAG_X3D_IndexedTriangleSet: return IndexedTriangleSet_get_field(node, field);
	case TAG_X3D_IndexedTriangleStripSet: return IndexedTriangleStripSet_get_field(node, field);
	case TAG_X3D_Inline: return Inline_get_field(node, field);
	case TAG_X3D_IntegerSequencer: return IntegerSequencer_get_field(node, field);
	case TAG_X3D_IntegerTrigger: return IntegerTrigger_get_field(node, field);
	case TAG_X3D_KeySensor: return KeySensor_get_field(node, field);
	case TAG_X3D_LineProperties: return LineProperties_get_field(node, field);
	case TAG_X3D_LineSet: return LineSet_get_field(node, field);
	case TAG_X3D_LOD: return LOD_get_field(node, field);
	case TAG_X3D_Material: return Material_get_field(node, field);
	case TAG_X3D_MetadataDouble: return MetadataDouble_get_field(node, field);
	case TAG_X3D_MetadataFloat: return MetadataFloat_get_field(node, field);
	case TAG_X3D_MetadataInteger: return MetadataInteger_get_field(node, field);
	case TAG_X3D_MetadataSet: return MetadataSet_get_field(node, field);
	case TAG_X3D_MetadataString: return MetadataString_get_field(node, field);
	case TAG_X3D_MovieTexture: return MovieTexture_get_field(node, field);
	case TAG_X3D_MultiTexture: return MultiTexture_get_field(node, field);
	case TAG_X3D_MultiTextureCoordinate: return MultiTextureCoordinate_get_field(node, field);
	case TAG_X3D_MultiTextureTransform: return MultiTextureTransform_get_field(node, field);
	case TAG_X3D_NavigationInfo: return NavigationInfo_get_field(node, field);
	case TAG_X3D_Normal: return Normal_get_field(node, field);
	case TAG_X3D_NormalInterpolator: return NormalInterpolator_get_field(node, field);
	case TAG_X3D_OrientationInterpolator: return OrientationInterpolator_get_field(node, field);
	case TAG_X3D_PixelTexture: return PixelTexture_get_field(node, field);
	case TAG_X3D_PlaneSensor: return PlaneSensor_get_field(node, field);
	case TAG_X3D_PointLight: return PointLight_get_field(node, field);
	case TAG_X3D_PointSet: return PointSet_get_field(node, field);
	case TAG_X3D_Polyline2D: return Polyline2D_get_field(node, field);
	case TAG_X3D_Polypoint2D: return Polypoint2D_get_field(node, field);
	case TAG_X3D_PositionInterpolator: return PositionInterpolator_get_field(node, field);
	case TAG_X3D_PositionInterpolator2D: return PositionInterpolator2D_get_field(node, field);
	case TAG_X3D_ProximitySensor: return ProximitySensor_get_field(node, field);
	case TAG_X3D_Rectangle2D: return Rectangle2D_get_field(node, field);
	case TAG_X3D_ScalarInterpolator: return ScalarInterpolator_get_field(node, field);
	case TAG_X3D_Script: return Script_get_field(node, field);
	case TAG_X3D_Shape: return Shape_get_field(node, field);
	case TAG_X3D_Sound: return Sound_get_field(node, field);
	case TAG_X3D_Sphere: return Sphere_get_field(node, field);
	case TAG_X3D_SphereSensor: return SphereSensor_get_field(node, field);
	case TAG_X3D_SpotLight: return SpotLight_get_field(node, field);
	case TAG_X3D_StaticGroup: return StaticGroup_get_field(node, field);
	case TAG_X3D_StringSensor: return StringSensor_get_field(node, field);
	case TAG_X3D_Switch: return Switch_get_field(node, field);
	case TAG_X3D_Text: return Text_get_field(node, field);
	case TAG_X3D_TextureBackground: return TextureBackground_get_field(node, field);
	case TAG_X3D_TextureCoordinate: return TextureCoordinate_get_field(node, field);
	case TAG_X3D_TextureCoordinateGenerator: return TextureCoordinateGenerator_get_field(node, field);
	case TAG_X3D_TextureTransform: return TextureTransform_get_field(node, field);
	case TAG_X3D_TimeSensor: return TimeSensor_get_field(node, field);
	case TAG_X3D_TimeTrigger: return TimeTrigger_get_field(node, field);
	case TAG_X3D_TouchSensor: return TouchSensor_get_field(node, field);
	case TAG_X3D_Transform: return Transform_get_field(node, field);
	case TAG_X3D_TriangleFanSet: return TriangleFanSet_get_field(node, field);
	case TAG_X3D_TriangleSet: return TriangleSet_get_field(node, field);
	case TAG_X3D_TriangleSet2D: return TriangleSet2D_get_field(node, field);
	case TAG_X3D_TriangleStripSet: return TriangleStripSet_get_field(node, field);
	case TAG_X3D_Viewpoint: return Viewpoint_get_field(node, field);
	case TAG_X3D_VisibilitySensor: return VisibilitySensor_get_field(node, field);
	case TAG_X3D_WorldInfo: return WorldInfo_get_field(node, field);
	default:
		return GF_BAD_PARAM;
	}
}


u32 gf_node_x3d_type_by_class_name(const char *node_name)
{
	if(!node_name) return 0;
	if (!strcmp(node_name, "Anchor")) return TAG_X3D_Anchor;
	if (!strcmp(node_name, "Appearance")) return TAG_X3D_Appearance;
	if (!strcmp(node_name, "Arc2D")) return TAG_X3D_Arc2D;
	if (!strcmp(node_name, "ArcClose2D")) return TAG_X3D_ArcClose2D;
	if (!strcmp(node_name, "AudioClip")) return TAG_X3D_AudioClip;
	if (!strcmp(node_name, "Background")) return TAG_X3D_Background;
	if (!strcmp(node_name, "Billboard")) return TAG_X3D_Billboard;
	if (!strcmp(node_name, "BooleanFilter")) return TAG_X3D_BooleanFilter;
	if (!strcmp(node_name, "BooleanSequencer")) return TAG_X3D_BooleanSequencer;
	if (!strcmp(node_name, "BooleanToggle")) return TAG_X3D_BooleanToggle;
	if (!strcmp(node_name, "BooleanTrigger")) return TAG_X3D_BooleanTrigger;
	if (!strcmp(node_name, "Box")) return TAG_X3D_Box;
	if (!strcmp(node_name, "Circle2D")) return TAG_X3D_Circle2D;
	if (!strcmp(node_name, "Collision")) return TAG_X3D_Collision;
	if (!strcmp(node_name, "Color")) return TAG_X3D_Color;
	if (!strcmp(node_name, "ColorInterpolator")) return TAG_X3D_ColorInterpolator;
	if (!strcmp(node_name, "ColorRGBA")) return TAG_X3D_ColorRGBA;
	if (!strcmp(node_name, "Cone")) return TAG_X3D_Cone;
	if (!strcmp(node_name, "Contour2D")) return TAG_X3D_Contour2D;
	if (!strcmp(node_name, "ContourPolyline2D")) return TAG_X3D_ContourPolyline2D;
	if (!strcmp(node_name, "Coordinate")) return TAG_X3D_Coordinate;
	if (!strcmp(node_name, "CoordinateDouble")) return TAG_X3D_CoordinateDouble;
	if (!strcmp(node_name, "Coordinate2D")) return TAG_X3D_Coordinate2D;
	if (!strcmp(node_name, "CoordinateInterpolator")) return TAG_X3D_CoordinateInterpolator;
	if (!strcmp(node_name, "CoordinateInterpolator2D")) return TAG_X3D_CoordinateInterpolator2D;
	if (!strcmp(node_name, "Cylinder")) return TAG_X3D_Cylinder;
	if (!strcmp(node_name, "CylinderSensor")) return TAG_X3D_CylinderSensor;
	if (!strcmp(node_name, "DirectionalLight")) return TAG_X3D_DirectionalLight;
	if (!strcmp(node_name, "Disk2D")) return TAG_X3D_Disk2D;
	if (!strcmp(node_name, "ElevationGrid")) return TAG_X3D_ElevationGrid;
	if (!strcmp(node_name, "Extrusion")) return TAG_X3D_Extrusion;
	if (!strcmp(node_name, "FillProperties")) return TAG_X3D_FillProperties;
	if (!strcmp(node_name, "Fog")) return TAG_X3D_Fog;
	if (!strcmp(node_name, "FontStyle")) return TAG_X3D_FontStyle;
	if (!strcmp(node_name, "Group")) return TAG_X3D_Group;
	if (!strcmp(node_name, "ImageTexture")) return TAG_X3D_ImageTexture;
	if (!strcmp(node_name, "IndexedFaceSet")) return TAG_X3D_IndexedFaceSet;
	if (!strcmp(node_name, "IndexedLineSet")) return TAG_X3D_IndexedLineSet;
	if (!strcmp(node_name, "IndexedTriangleFanSet")) return TAG_X3D_IndexedTriangleFanSet;
	if (!strcmp(node_name, "IndexedTriangleSet")) return TAG_X3D_IndexedTriangleSet;
	if (!strcmp(node_name, "IndexedTriangleStripSet")) return TAG_X3D_IndexedTriangleStripSet;
	if (!strcmp(node_name, "Inline")) return TAG_X3D_Inline;
	if (!strcmp(node_name, "IntegerSequencer")) return TAG_X3D_IntegerSequencer;
	if (!strcmp(node_name, "IntegerTrigger")) return TAG_X3D_IntegerTrigger;
	if (!strcmp(node_name, "KeySensor")) return TAG_X3D_KeySensor;
	if (!strcmp(node_name, "LineProperties")) return TAG_X3D_LineProperties;
	if (!strcmp(node_name, "LineSet")) return TAG_X3D_LineSet;
	if (!strcmp(node_name, "LOD")) return TAG_X3D_LOD;
	if (!strcmp(node_name, "Material")) return TAG_X3D_Material;
	if (!strcmp(node_name, "MetadataDouble")) return TAG_X3D_MetadataDouble;
	if (!strcmp(node_name, "MetadataFloat")) return TAG_X3D_MetadataFloat;
	if (!strcmp(node_name, "MetadataInteger")) return TAG_X3D_MetadataInteger;
	if (!strcmp(node_name, "MetadataSet")) return TAG_X3D_MetadataSet;
	if (!strcmp(node_name, "MetadataString")) return TAG_X3D_MetadataString;
	if (!strcmp(node_name, "MovieTexture")) return TAG_X3D_MovieTexture;
	if (!strcmp(node_name, "MultiTexture")) return TAG_X3D_MultiTexture;
	if (!strcmp(node_name, "MultiTextureCoordinate")) return TAG_X3D_MultiTextureCoordinate;
	if (!strcmp(node_name, "MultiTextureTransform")) return TAG_X3D_MultiTextureTransform;
	if (!strcmp(node_name, "NavigationInfo")) return TAG_X3D_NavigationInfo;
	if (!strcmp(node_name, "Normal")) return TAG_X3D_Normal;
	if (!strcmp(node_name, "NormalInterpolator")) return TAG_X3D_NormalInterpolator;
	if (!strcmp(node_name, "OrientationInterpolator")) return TAG_X3D_OrientationInterpolator;
	if (!strcmp(node_name, "PixelTexture")) return TAG_X3D_PixelTexture;
	if (!strcmp(node_name, "PlaneSensor")) return TAG_X3D_PlaneSensor;
	if (!strcmp(node_name, "PointLight")) return TAG_X3D_PointLight;
	if (!strcmp(node_name, "PointSet")) return TAG_X3D_PointSet;
	if (!strcmp(node_name, "Polyline2D")) return TAG_X3D_Polyline2D;
	if (!strcmp(node_name, "Polypoint2D")) return TAG_X3D_Polypoint2D;
	if (!strcmp(node_name, "PositionInterpolator")) return TAG_X3D_PositionInterpolator;
	if (!strcmp(node_name, "PositionInterpolator2D")) return TAG_X3D_PositionInterpolator2D;
	if (!strcmp(node_name, "ProximitySensor")) return TAG_X3D_ProximitySensor;
	if (!strcmp(node_name, "Rectangle2D")) return TAG_X3D_Rectangle2D;
	if (!strcmp(node_name, "ScalarInterpolator")) return TAG_X3D_ScalarInterpolator;
	if (!strcmp(node_name, "Script")) return TAG_X3D_Script;
	if (!strcmp(node_name, "Shape")) return TAG_X3D_Shape;
	if (!strcmp(node_name, "Sound")) return TAG_X3D_Sound;
	if (!strcmp(node_name, "Sphere")) return TAG_X3D_Sphere;
	if (!strcmp(node_name, "SphereSensor")) return TAG_X3D_SphereSensor;
	if (!strcmp(node_name, "SpotLight")) return TAG_X3D_SpotLight;
	if (!strcmp(node_name, "StaticGroup")) return TAG_X3D_StaticGroup;
	if (!strcmp(node_name, "StringSensor")) return TAG_X3D_StringSensor;
	if (!strcmp(node_name, "Switch")) return TAG_X3D_Switch;
	if (!strcmp(node_name, "Text")) return TAG_X3D_Text;
	if (!strcmp(node_name, "TextureBackground")) return TAG_X3D_TextureBackground;
	if (!strcmp(node_name, "TextureCoordinate")) return TAG_X3D_TextureCoordinate;
	if (!strcmp(node_name, "TextureCoordinateGenerator")) return TAG_X3D_TextureCoordinateGenerator;
	if (!strcmp(node_name, "TextureTransform")) return TAG_X3D_TextureTransform;
	if (!strcmp(node_name, "TimeSensor")) return TAG_X3D_TimeSensor;
	if (!strcmp(node_name, "TimeTrigger")) return TAG_X3D_TimeTrigger;
	if (!strcmp(node_name, "TouchSensor")) return TAG_X3D_TouchSensor;
	if (!strcmp(node_name, "Transform")) return TAG_X3D_Transform;
	if (!strcmp(node_name, "TriangleFanSet")) return TAG_X3D_TriangleFanSet;
	if (!strcmp(node_name, "TriangleSet")) return TAG_X3D_TriangleSet;
	if (!strcmp(node_name, "TriangleSet2D")) return TAG_X3D_TriangleSet2D;
	if (!strcmp(node_name, "TriangleStripSet")) return TAG_X3D_TriangleStripSet;
	if (!strcmp(node_name, "Viewpoint")) return TAG_X3D_Viewpoint;
	if (!strcmp(node_name, "VisibilitySensor")) return TAG_X3D_VisibilitySensor;
	if (!strcmp(node_name, "WorldInfo")) return TAG_X3D_WorldInfo;
	return 0;
}

s32 gf_sg_x3d_node_get_field_index_by_name(GF_Node *node, char *name)
{
	switch (node->sgprivate->tag) {
	case TAG_X3D_Anchor: return Anchor_get_field_index_by_name(name);
	case TAG_X3D_Appearance: return Appearance_get_field_index_by_name(name);
	case TAG_X3D_Arc2D: return Arc2D_get_field_index_by_name(name);
	case TAG_X3D_ArcClose2D: return ArcClose2D_get_field_index_by_name(name);
	case TAG_X3D_AudioClip: return AudioClip_get_field_index_by_name(name);
	case TAG_X3D_Background: return Background_get_field_index_by_name(name);
	case TAG_X3D_Billboard: return Billboard_get_field_index_by_name(name);
	case TAG_X3D_BooleanFilter: return BooleanFilter_get_field_index_by_name(name);
	case TAG_X3D_BooleanSequencer: return BooleanSequencer_get_field_index_by_name(name);
	case TAG_X3D_BooleanToggle: return BooleanToggle_get_field_index_by_name(name);
	case TAG_X3D_BooleanTrigger: return BooleanTrigger_get_field_index_by_name(name);
	case TAG_X3D_Box: return Box_get_field_index_by_name(name);
	case TAG_X3D_Circle2D: return Circle2D_get_field_index_by_name(name);
	case TAG_X3D_Collision: return Collision_get_field_index_by_name(name);
	case TAG_X3D_Color: return Color_get_field_index_by_name(name);
	case TAG_X3D_ColorInterpolator: return ColorInterpolator_get_field_index_by_name(name);
	case TAG_X3D_ColorRGBA: return ColorRGBA_get_field_index_by_name(name);
	case TAG_X3D_Cone: return Cone_get_field_index_by_name(name);
	case TAG_X3D_Contour2D: return Contour2D_get_field_index_by_name(name);
	case TAG_X3D_ContourPolyline2D: return ContourPolyline2D_get_field_index_by_name(name);
	case TAG_X3D_Coordinate: return Coordinate_get_field_index_by_name(name);
	case TAG_X3D_CoordinateDouble: return CoordinateDouble_get_field_index_by_name(name);
	case TAG_X3D_Coordinate2D: return Coordinate2D_get_field_index_by_name(name);
	case TAG_X3D_CoordinateInterpolator: return CoordinateInterpolator_get_field_index_by_name(name);
	case TAG_X3D_CoordinateInterpolator2D: return CoordinateInterpolator2D_get_field_index_by_name(name);
	case TAG_X3D_Cylinder: return Cylinder_get_field_index_by_name(name);
	case TAG_X3D_CylinderSensor: return CylinderSensor_get_field_index_by_name(name);
	case TAG_X3D_DirectionalLight: return DirectionalLight_get_field_index_by_name(name);
	case TAG_X3D_Disk2D: return Disk2D_get_field_index_by_name(name);
	case TAG_X3D_ElevationGrid: return ElevationGrid_get_field_index_by_name(name);
	case TAG_X3D_Extrusion: return Extrusion_get_field_index_by_name(name);
	case TAG_X3D_FillProperties: return FillProperties_get_field_index_by_name(name);
	case TAG_X3D_Fog: return Fog_get_field_index_by_name(name);
	case TAG_X3D_FontStyle: return FontStyle_get_field_index_by_name(name);
	case TAG_X3D_Group: return Group_get_field_index_by_name(name);
	case TAG_X3D_ImageTexture: return ImageTexture_get_field_index_by_name(name);
	case TAG_X3D_IndexedFaceSet: return IndexedFaceSet_get_field_index_by_name(name);
	case TAG_X3D_IndexedLineSet: return IndexedLineSet_get_field_index_by_name(name);
	case TAG_X3D_IndexedTriangleFanSet: return IndexedTriangleFanSet_get_field_index_by_name(name);
	case TAG_X3D_IndexedTriangleSet: return IndexedTriangleSet_get_field_index_by_name(name);
	case TAG_X3D_IndexedTriangleStripSet: return IndexedTriangleStripSet_get_field_index_by_name(name);
	case TAG_X3D_Inline: return Inline_get_field_index_by_name(name);
	case TAG_X3D_IntegerSequencer: return IntegerSequencer_get_field_index_by_name(name);
	case TAG_X3D_IntegerTrigger: return IntegerTrigger_get_field_index_by_name(name);
	case TAG_X3D_KeySensor: return KeySensor_get_field_index_by_name(name);
	case TAG_X3D_LineProperties: return LineProperties_get_field_index_by_name(name);
	case TAG_X3D_LineSet: return LineSet_get_field_index_by_name(name);
	case TAG_X3D_LOD: return LOD_get_field_index_by_name(name);
	case TAG_X3D_Material: return Material_get_field_index_by_name(name);
	case TAG_X3D_MetadataDouble: return MetadataDouble_get_field_index_by_name(name);
	case TAG_X3D_MetadataFloat: return MetadataFloat_get_field_index_by_name(name);
	case TAG_X3D_MetadataInteger: return MetadataInteger_get_field_index_by_name(name);
	case TAG_X3D_MetadataSet: return MetadataSet_get_field_index_by_name(name);
	case TAG_X3D_MetadataString: return MetadataString_get_field_index_by_name(name);
	case TAG_X3D_MovieTexture: return MovieTexture_get_field_index_by_name(name);
	case TAG_X3D_MultiTexture: return MultiTexture_get_field_index_by_name(name);
	case TAG_X3D_MultiTextureCoordinate: return MultiTextureCoordinate_get_field_index_by_name(name);
	case TAG_X3D_MultiTextureTransform: return MultiTextureTransform_get_field_index_by_name(name);
	case TAG_X3D_NavigationInfo: return NavigationInfo_get_field_index_by_name(name);
	case TAG_X3D_Normal: return Normal_get_field_index_by_name(name);
	case TAG_X3D_NormalInterpolator: return NormalInterpolator_get_field_index_by_name(name);
	case TAG_X3D_OrientationInterpolator: return OrientationInterpolator_get_field_index_by_name(name);
	case TAG_X3D_PixelTexture: return PixelTexture_get_field_index_by_name(name);
	case TAG_X3D_PlaneSensor: return PlaneSensor_get_field_index_by_name(name);
	case TAG_X3D_PointLight: return PointLight_get_field_index_by_name(name);
	case TAG_X3D_PointSet: return PointSet_get_field_index_by_name(name);
	case TAG_X3D_Polyline2D: return Polyline2D_get_field_index_by_name(name);
	case TAG_X3D_Polypoint2D: return Polypoint2D_get_field_index_by_name(name);
	case TAG_X3D_PositionInterpolator: return PositionInterpolator_get_field_index_by_name(name);
	case TAG_X3D_PositionInterpolator2D: return PositionInterpolator2D_get_field_index_by_name(name);
	case TAG_X3D_ProximitySensor: return ProximitySensor_get_field_index_by_name(name);
	case TAG_X3D_Rectangle2D: return Rectangle2D_get_field_index_by_name(name);
	case TAG_X3D_ScalarInterpolator: return ScalarInterpolator_get_field_index_by_name(name);
	case TAG_X3D_Script: return Script_get_field_index_by_name(name);
	case TAG_X3D_Shape: return Shape_get_field_index_by_name(name);
	case TAG_X3D_Sound: return Sound_get_field_index_by_name(name);
	case TAG_X3D_Sphere: return Sphere_get_field_index_by_name(name);
	case TAG_X3D_SphereSensor: return SphereSensor_get_field_index_by_name(name);
	case TAG_X3D_SpotLight: return SpotLight_get_field_index_by_name(name);
	case TAG_X3D_StaticGroup: return StaticGroup_get_field_index_by_name(name);
	case TAG_X3D_StringSensor: return StringSensor_get_field_index_by_name(name);
	case TAG_X3D_Switch: return Switch_get_field_index_by_name(name);
	case TAG_X3D_Text: return Text_get_field_index_by_name(name);
	case TAG_X3D_TextureBackground: return TextureBackground_get_field_index_by_name(name);
	case TAG_X3D_TextureCoordinate: return TextureCoordinate_get_field_index_by_name(name);
	case TAG_X3D_TextureCoordinateGenerator: return TextureCoordinateGenerator_get_field_index_by_name(name);
	case TAG_X3D_TextureTransform: return TextureTransform_get_field_index_by_name(name);
	case TAG_X3D_TimeSensor: return TimeSensor_get_field_index_by_name(name);
	case TAG_X3D_TimeTrigger: return TimeTrigger_get_field_index_by_name(name);
	case TAG_X3D_TouchSensor: return TouchSensor_get_field_index_by_name(name);
	case TAG_X3D_Transform: return Transform_get_field_index_by_name(name);
	case TAG_X3D_TriangleFanSet: return TriangleFanSet_get_field_index_by_name(name);
	case TAG_X3D_TriangleSet: return TriangleSet_get_field_index_by_name(name);
	case TAG_X3D_TriangleSet2D: return TriangleSet2D_get_field_index_by_name(name);
	case TAG_X3D_TriangleStripSet: return TriangleStripSet_get_field_index_by_name(name);
	case TAG_X3D_Viewpoint: return Viewpoint_get_field_index_by_name(name);
	case TAG_X3D_VisibilitySensor: return VisibilitySensor_get_field_index_by_name(name);
	case TAG_X3D_WorldInfo: return WorldInfo_get_field_index_by_name(name);
	default:
		return -1;
	}
}



/* NDT X3D */

#define SFWorldNode_X3D_Count	127
static const u32 SFWorldNode_X3D_TypeToTag[127] = {
 TAG_X3D_Anchor, TAG_X3D_Appearance, TAG_X3D_Arc2D, TAG_X3D_ArcClose2D, TAG_X3D_AudioClip, TAG_X3D_Background, TAG_X3D_Billboard, TAG_X3D_BooleanFilter, TAG_X3D_BooleanSequencer, TAG_X3D_BooleanToggle, TAG_X3D_BooleanTrigger, TAG_X3D_Box, TAG_X3D_Circle2D, TAG_X3D_Collision, TAG_X3D_Color, TAG_X3D_ColorInterpolator, TAG_X3D_ColorRGBA, TAG_X3D_Cone, TAG_X3D_Contour2D, TAG_X3D_ContourPolyline2D, TAG_X3D_Coordinate, TAG_X3D_CoordinateDouble, TAG_X3D_Coordinate2D, TAG_X3D_CoordinateInterpolator, TAG_X3D_CoordinateInterpolator2D, TAG_X3D_Cylinder, TAG_X3D_CylinderSensor, TAG_X3D_DirectionalLight, TAG_X3D_Disk2D, TAG_X3D_ElevationGrid, TAG_X3D_EspduTransform, TAG_X3D_Extrusion, TAG_X3D_FillProperties, TAG_X3D_Fog, TAG_X3D_FontStyle, TAG_X3D_GeoCoordinate, TAG_X3D_GeoElevationGrid, TAG_X3D_GeoLocation, TAG_X3D_GeoLOD, TAG_X3D_GeoMetadata, TAG_X3D_GeoPositionInterpolator, TAG_X3D_GeoTouchSensor, TAG_X3D_GeoViewpoint, TAG_X3D_Group, TAG_X3D_HAnimDisplacer, TAG_X3D_HAnimHumanoid, TAG_X3D_HAnimJoint, TAG_X3D_HAnimSegment, TAG_X3D_HAnimSite, TAG_X3D_ImageTexture, TAG_X3D_IndexedFaceSet, TAG_X3D_IndexedLineSet, TAG_X3D_IndexedTriangleFanSet, TAG_X3D_IndexedTriangleSet, TAG_X3D_IndexedTriangleStripSet, TAG_X3D_Inline, TAG_X3D_IntegerSequencer, TAG_X3D_IntegerTrigger, TAG_X3D_KeySensor, TAG_X3D_LineProperties, TAG_X3D_LineSet, TAG_X3D_LoadSensor, TAG_X3D_LOD, TAG_X3D_Material, TAG_X3D_MetadataDouble, TAG_X3D_MetadataFloat, TAG_X3D_MetadataInteger, TAG_X3D_MetadataSet, TAG_X3D_MetadataString, TAG_X3D_MovieTexture, TAG_X3D_MultiTexture, TAG_X3D_MultiTextureCoordinate, TAG_X3D_MultiTextureTransform, TAG_X3D_NavigationInfo, TAG_X3D_Normal, TAG_X3D_NormalInterpolator, TAG_X3D_NurbsCurve, TAG_X3D_NurbsCurve2D, TAG_X3D_NurbsOrientationInterpolator, TAG_X3D_NurbsPatchSurface, TAG_X3D_NurbsPositionInterpolator, TAG_X3D_NurbsSet, TAG_X3D_NurbsSurfaceInterpolator, TAG_X3D_NurbsSweptSurface, TAG_X3D_NurbsSwungSurface, TAG_X3D_NurbsTextureCoordinate, TAG_X3D_NurbsTrimmedSurface, TAG_X3D_OrientationInterpolator, TAG_X3D_PixelTexture, TAG_X3D_PlaneSensor, TAG_X3D_PointLight, TAG_X3D_PointSet, TAG_X3D_Polyline2D, TAG_X3D_Polypoint2D, TAG_X3D_PositionInterpolator, TAG_X3D_PositionInterpolator2D, TAG_X3D_ProximitySensor, TAG_X3D_ReceiverPdu, TAG_X3D_Rectangle2D, TAG_X3D_ScalarInterpolator, TAG_X3D_Script, TAG_X3D_Shape, TAG_X3D_SignalPdu, TAG_X3D_Sound, TAG_X3D_Sphere, TAG_X3D_SphereSensor, TAG_X3D_SpotLight, TAG_X3D_StaticGroup, TAG_X3D_StringSensor, TAG_X3D_Switch, TAG_X3D_Text, TAG_X3D_TextureBackground, TAG_X3D_TextureCoordinate, TAG_X3D_TextureCoordinateGenerator, TAG_X3D_TextureTransform, TAG_X3D_TimeSensor, TAG_X3D_TimeTrigger, TAG_X3D_TouchSensor, TAG_X3D_Transform, TAG_X3D_TransmitterPdu, TAG_X3D_TriangleFanSet, TAG_X3D_TriangleSet, TAG_X3D_TriangleSet2D, TAG_X3D_TriangleStripSet, TAG_X3D_Viewpoint, TAG_X3D_VisibilitySensor, TAG_X3D_WorldInfo
};

#define SF3DNode_X3D_Count	60
static const u32 SF3DNode_X3D_TypeToTag[60] = {
 TAG_X3D_Anchor, TAG_X3D_Background, TAG_X3D_Billboard, TAG_X3D_BooleanFilter, TAG_X3D_BooleanSequencer, TAG_X3D_BooleanToggle, TAG_X3D_BooleanTrigger, TAG_X3D_Collision, TAG_X3D_ColorInterpolator, TAG_X3D_CoordinateInterpolator, TAG_X3D_CoordinateInterpolator2D, TAG_X3D_CylinderSensor, TAG_X3D_DirectionalLight, TAG_X3D_EspduTransform, TAG_X3D_Fog, TAG_X3D_GeoLocation, TAG_X3D_GeoLOD, TAG_X3D_GeoMetadata, TAG_X3D_GeoPositionInterpolator, TAG_X3D_GeoTouchSensor, TAG_X3D_GeoViewpoint, TAG_X3D_Group, TAG_X3D_HAnimHumanoid, TAG_X3D_Inline, TAG_X3D_IntegerSequencer, TAG_X3D_IntegerTrigger, TAG_X3D_KeySensor, TAG_X3D_LOD, TAG_X3D_NavigationInfo, TAG_X3D_NormalInterpolator, TAG_X3D_NurbsOrientationInterpolator, TAG_X3D_NurbsPositionInterpolator, TAG_X3D_NurbsSet, TAG_X3D_NurbsSurfaceInterpolator, TAG_X3D_OrientationInterpolator, TAG_X3D_PlaneSensor, TAG_X3D_PointLight, TAG_X3D_PositionInterpolator, TAG_X3D_PositionInterpolator2D, TAG_X3D_ProximitySensor, TAG_X3D_ReceiverPdu, TAG_X3D_ScalarInterpolator, TAG_X3D_Script, TAG_X3D_Shape, TAG_X3D_SignalPdu, TAG_X3D_Sound, TAG_X3D_SphereSensor, TAG_X3D_SpotLight, TAG_X3D_StaticGroup, TAG_X3D_StringSensor, TAG_X3D_Switch, TAG_X3D_TextureBackground, TAG_X3D_TimeSensor, TAG_X3D_TimeTrigger, TAG_X3D_TouchSensor, TAG_X3D_Transform, TAG_X3D_TransmitterPdu, TAG_X3D_Viewpoint, TAG_X3D_VisibilitySensor, TAG_X3D_WorldInfo
};

#define SF2DNode_X3D_Count	34
static const u32 SF2DNode_X3D_TypeToTag[34] = {
 TAG_X3D_Anchor, TAG_X3D_BooleanFilter, TAG_X3D_BooleanSequencer, TAG_X3D_BooleanToggle, TAG_X3D_BooleanTrigger, TAG_X3D_ColorInterpolator, TAG_X3D_CoordinateInterpolator2D, TAG_X3D_EspduTransform, TAG_X3D_GeoMetadata, TAG_X3D_GeoTouchSensor, TAG_X3D_Group, TAG_X3D_Inline, TAG_X3D_IntegerSequencer, TAG_X3D_IntegerTrigger, TAG_X3D_KeySensor, TAG_X3D_LOD, TAG_X3D_NurbsOrientationInterpolator, TAG_X3D_NurbsPositionInterpolator, TAG_X3D_NurbsSet, TAG_X3D_NurbsSurfaceInterpolator, TAG_X3D_PositionInterpolator2D, TAG_X3D_ReceiverPdu, TAG_X3D_ScalarInterpolator, TAG_X3D_Script, TAG_X3D_Shape, TAG_X3D_SignalPdu, TAG_X3D_StaticGroup, TAG_X3D_StringSensor, TAG_X3D_Switch, TAG_X3D_TimeSensor, TAG_X3D_TimeTrigger, TAG_X3D_TouchSensor, TAG_X3D_TransmitterPdu, TAG_X3D_WorldInfo
};

#define SFAppearanceNode_X3D_Count	1
static const u32 SFAppearanceNode_X3D_TypeToTag[1] = {
 TAG_X3D_Appearance
};

#define SFGeometryNode_X3D_Count	31
static const u32 SFGeometryNode_X3D_TypeToTag[31] = {
 TAG_X3D_Arc2D, TAG_X3D_ArcClose2D, TAG_X3D_Box, TAG_X3D_Circle2D, TAG_X3D_Cone, TAG_X3D_Cylinder, TAG_X3D_Disk2D, TAG_X3D_ElevationGrid, TAG_X3D_Extrusion, TAG_X3D_GeoElevationGrid, TAG_X3D_IndexedFaceSet, TAG_X3D_IndexedLineSet, TAG_X3D_IndexedTriangleFanSet, TAG_X3D_IndexedTriangleSet, TAG_X3D_IndexedTriangleStripSet, TAG_X3D_LineSet, TAG_X3D_NurbsCurve, TAG_X3D_NurbsPatchSurface, TAG_X3D_NurbsSweptSurface, TAG_X3D_NurbsSwungSurface, TAG_X3D_NurbsTrimmedSurface, TAG_X3D_PointSet, TAG_X3D_Polyline2D, TAG_X3D_Polypoint2D, TAG_X3D_Rectangle2D, TAG_X3D_Sphere, TAG_X3D_Text, TAG_X3D_TriangleFanSet, TAG_X3D_TriangleSet, TAG_X3D_TriangleSet2D, TAG_X3D_TriangleStripSet
};

#define SFAudioNode_X3D_Count	1
static const u32 SFAudioNode_X3D_TypeToTag[1] = {
 TAG_X3D_AudioClip
};

#define SFStreamingNode_X3D_Count	4
static const u32 SFStreamingNode_X3D_TypeToTag[4] = {
 TAG_X3D_AudioClip, TAG_X3D_Inline, TAG_X3D_LoadSensor, TAG_X3D_MovieTexture
};

#define SFBackground3DNode_X3D_Count	2
static const u32 SFBackground3DNode_X3D_TypeToTag[2] = {
 TAG_X3D_Background, TAG_X3D_TextureBackground
};

#define SFColorNode_X3D_Count	2
static const u32 SFColorNode_X3D_TypeToTag[2] = {
 TAG_X3D_Color, TAG_X3D_ColorRGBA
};

#define SFNurbsControlCurveNode_X3D_Count	3
static const u32 SFNurbsControlCurveNode_X3D_TypeToTag[3] = {
 TAG_X3D_Contour2D, TAG_X3D_ContourPolyline2D, TAG_X3D_NurbsCurve2D
};

#define SFCoordinateNode_X3D_Count	3
static const u32 SFCoordinateNode_X3D_TypeToTag[3] = {
 TAG_X3D_Coordinate, TAG_X3D_CoordinateDouble, TAG_X3D_GeoCoordinate
};

#define SFCoordinate2DNode_X3D_Count	1
static const u32 SFCoordinate2DNode_X3D_TypeToTag[1] = {
 TAG_X3D_Coordinate2D
};

#define SFFillPropertiesNode_X3D_Count	1
static const u32 SFFillPropertiesNode_X3D_TypeToTag[1] = {
 TAG_X3D_FillProperties
};

#define SFFogNode_X3D_Count	1
static const u32 SFFogNode_X3D_TypeToTag[1] = {
 TAG_X3D_Fog
};

#define SFFontStyleNode_X3D_Count	1
static const u32 SFFontStyleNode_X3D_TypeToTag[1] = {
 TAG_X3D_FontStyle
};

#define SFGeoOriginNode_X3D_Count	1
static const u32 SFGeoOriginNode_X3D_TypeToTag[1] = {
 TAG_X3D_GeoOrigin
};

#define SFViewpointNode_X3D_Count	2
static const u32 SFViewpointNode_X3D_TypeToTag[2] = {
 TAG_X3D_GeoViewpoint, TAG_X3D_Viewpoint
};

#define SFTopNode_X3D_Count	1
static const u32 SFTopNode_X3D_TypeToTag[1] = {
 TAG_X3D_Group
};

#define SFHAnimDisplacerNode_X3D_Count	1
static const u32 SFHAnimDisplacerNode_X3D_TypeToTag[1] = {
 TAG_X3D_HAnimDisplacer
};

#define SFHAnimNode_X3D_Count	3
static const u32 SFHAnimNode_X3D_TypeToTag[3] = {
 TAG_X3D_HAnimJoint, TAG_X3D_HAnimSegment, TAG_X3D_HAnimSite
};

#define SFTextureNode_X3D_Count	4
static const u32 SFTextureNode_X3D_TypeToTag[4] = {
 TAG_X3D_ImageTexture, TAG_X3D_MovieTexture, TAG_X3D_MultiTexture, TAG_X3D_PixelTexture
};

#define SFX3DLinePropertiesNode_X3D_Count	1
static const u32 SFX3DLinePropertiesNode_X3D_TypeToTag[1] = {
 TAG_X3D_LineProperties
};

#define SFMaterialNode_X3D_Count	1
static const u32 SFMaterialNode_X3D_TypeToTag[1] = {
 TAG_X3D_Material
};

#define SFMetadataNode_X3D_Count	5
static const u32 SFMetadataNode_X3D_TypeToTag[5] = {
 TAG_X3D_MetadataDouble, TAG_X3D_MetadataFloat, TAG_X3D_MetadataInteger, TAG_X3D_MetadataSet, TAG_X3D_MetadataString
};

#define SFTextureCoordinateNode_X3D_Count	4
static const u32 SFTextureCoordinateNode_X3D_TypeToTag[4] = {
 TAG_X3D_MultiTextureCoordinate, TAG_X3D_NurbsTextureCoordinate, TAG_X3D_TextureCoordinate, TAG_X3D_TextureCoordinateGenerator
};

#define SFTextureTransformNode_X3D_Count	2
static const u32 SFTextureTransformNode_X3D_TypeToTag[2] = {
 TAG_X3D_MultiTextureTransform, TAG_X3D_TextureTransform
};

#define SFNavigationInfoNode_X3D_Count	1
static const u32 SFNavigationInfoNode_X3D_TypeToTag[1] = {
 TAG_X3D_NavigationInfo
};

#define SFNormalNode_X3D_Count	1
static const u32 SFNormalNode_X3D_TypeToTag[1] = {
 TAG_X3D_Normal
};

#define SFNurbsCurveNode_X3D_Count	1
static const u32 SFNurbsCurveNode_X3D_TypeToTag[1] = {
 TAG_X3D_NurbsCurve
};

#define SFNurbsSurfaceNode_X3D_Count	4
static const u32 SFNurbsSurfaceNode_X3D_TypeToTag[4] = {
 TAG_X3D_NurbsPatchSurface, TAG_X3D_NurbsSweptSurface, TAG_X3D_NurbsSwungSurface, TAG_X3D_NurbsTrimmedSurface
};




Bool gf_x3d_get_node_type(u32 NDT_Tag, u32 NodeTag)
{
	const u32 *types;
	u32 count, i;
	if (!NodeTag) return 0;
	types = NULL; count = 0;
	switch (NDT_Tag) {
	case NDT_SFWorldNode:
		types = SFWorldNode_X3D_TypeToTag; count = SFWorldNode_X3D_Count; break;
	case NDT_SF3DNode:
		types = SF3DNode_X3D_TypeToTag; count = SF3DNode_X3D_Count; break;
	case NDT_SF2DNode:
		types = SF2DNode_X3D_TypeToTag; count = SF2DNode_X3D_Count; break;
	case NDT_SFAppearanceNode:
		types = SFAppearanceNode_X3D_TypeToTag; count = SFAppearanceNode_X3D_Count; break;
	case NDT_SFGeometryNode:
		types = SFGeometryNode_X3D_TypeToTag; count = SFGeometryNode_X3D_Count; break;
	case NDT_SFAudioNode:
		types = SFAudioNode_X3D_TypeToTag; count = SFAudioNode_X3D_Count; break;
	case NDT_SFStreamingNode:
		types = SFStreamingNode_X3D_TypeToTag; count = SFStreamingNode_X3D_Count; break;
	case NDT_SFBackground3DNode:
		types = SFBackground3DNode_X3D_TypeToTag; count = SFBackground3DNode_X3D_Count; break;
	case NDT_SFColorNode:
		types = SFColorNode_X3D_TypeToTag; count = SFColorNode_X3D_Count; break;
	case NDT_SFNurbsControlCurveNode:
		types = SFNurbsControlCurveNode_X3D_TypeToTag; count = SFNurbsControlCurveNode_X3D_Count; break;
	case NDT_SFCoordinateNode:
		types = SFCoordinateNode_X3D_TypeToTag; count = SFCoordinateNode_X3D_Count; break;
	case NDT_SFCoordinate2DNode:
		types = SFCoordinate2DNode_X3D_TypeToTag; count = SFCoordinate2DNode_X3D_Count; break;
	case NDT_SFFillPropertiesNode:
		types = SFFillPropertiesNode_X3D_TypeToTag; count = SFFillPropertiesNode_X3D_Count; break;
	case NDT_SFFogNode:
		types = SFFogNode_X3D_TypeToTag; count = SFFogNode_X3D_Count; break;
	case NDT_SFFontStyleNode:
		types = SFFontStyleNode_X3D_TypeToTag; count = SFFontStyleNode_X3D_Count; break;
	case NDT_SFGeoOriginNode:
		types = SFGeoOriginNode_X3D_TypeToTag; count = SFGeoOriginNode_X3D_Count; break;
	case NDT_SFViewpointNode:
		types = SFViewpointNode_X3D_TypeToTag; count = SFViewpointNode_X3D_Count; break;
	case NDT_SFTopNode:
		types = SFTopNode_X3D_TypeToTag; count = SFTopNode_X3D_Count; break;
	case NDT_SFHAnimDisplacerNode:
		types = SFHAnimDisplacerNode_X3D_TypeToTag; count = SFHAnimDisplacerNode_X3D_Count; break;
	case NDT_SFHAnimNode:
		types = SFHAnimNode_X3D_TypeToTag; count = SFHAnimNode_X3D_Count; break;
	case NDT_SFTextureNode:
		types = SFTextureNode_X3D_TypeToTag; count = SFTextureNode_X3D_Count; break;
	case NDT_SFX3DLinePropertiesNode:
		types = SFX3DLinePropertiesNode_X3D_TypeToTag; count = SFX3DLinePropertiesNode_X3D_Count; break;
	case NDT_SFMaterialNode:
		types = SFMaterialNode_X3D_TypeToTag; count = SFMaterialNode_X3D_Count; break;
	case NDT_SFMetadataNode:
		types = SFMetadataNode_X3D_TypeToTag; count = SFMetadataNode_X3D_Count; break;
	case NDT_SFTextureCoordinateNode:
		types = SFTextureCoordinateNode_X3D_TypeToTag; count = SFTextureCoordinateNode_X3D_Count; break;
	case NDT_SFTextureTransformNode:
		types = SFTextureTransformNode_X3D_TypeToTag; count = SFTextureTransformNode_X3D_Count; break;
	case NDT_SFNavigationInfoNode:
		types = SFNavigationInfoNode_X3D_TypeToTag; count = SFNavigationInfoNode_X3D_Count; break;
	case NDT_SFNormalNode:
		types = SFNormalNode_X3D_TypeToTag; count = SFNormalNode_X3D_Count; break;
	case NDT_SFNurbsCurveNode:
		types = SFNurbsCurveNode_X3D_TypeToTag; count = SFNurbsCurveNode_X3D_Count; break;
	case NDT_SFNurbsSurfaceNode:
		types = SFNurbsSurfaceNode_X3D_TypeToTag; count = SFNurbsSurfaceNode_X3D_Count; break;
	default:
		return 0;
	}
	for(i=0; i<count; i++) { if (types[i]==NodeTag) return 1;}
	return 0;
}

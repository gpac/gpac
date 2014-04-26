/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2000-2012
 *					All rights reserved
 *
 *  This file is part of GPAC / BIFS codec sub-project
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
	DO NOT MOFIFY - File generated on GMT Tue Nov 08 09:10:57 2011

	BY MPEG4Gen for GPAC Version 0.5.0
*/



#include <gpac/internal/bifs_tables.h>


#ifndef GPAC_DISABLE_BIFS


u32 ALL_GetNodeType(const u32 *table, const u32 count, u32 NodeTag, u32 Version)
{
	u32 i = 0;
	while (i<count) {
		if (table[i] == NodeTag) goto found;
		i++;
	}
	return 0;
found:
	if (Version == 2) return i+2;
	return i+1;
}




u32 NDT_V1_GetNodeTag(u32 Context_NDT_Tag, u32 NodeType)
{
	if (!NodeType) return 0;
	/* adjust according to the table version */
	/* v1: 0 reserved for extensions */
	NodeType -= 1;
	switch (Context_NDT_Tag) {
	case NDT_SFWorldNode:
		if (NodeType >= SFWorldNode_V1_Count) return 0;
		return SFWorldNode_V1_TypeToTag[NodeType];
	case NDT_SF3DNode:
		if (NodeType >= SF3DNode_V1_Count) return 0;
		return SF3DNode_V1_TypeToTag[NodeType];
	case NDT_SF2DNode:
		if (NodeType >= SF2DNode_V1_Count) return 0;
		return SF2DNode_V1_TypeToTag[NodeType];
	case NDT_SFStreamingNode:
		if (NodeType >= SFStreamingNode_V1_Count) return 0;
		return SFStreamingNode_V1_TypeToTag[NodeType];
	case NDT_SFAppearanceNode:
		if (NodeType >= SFAppearanceNode_V1_Count) return 0;
		return SFAppearanceNode_V1_TypeToTag[NodeType];
	case NDT_SFAudioNode:
		if (NodeType >= SFAudioNode_V1_Count) return 0;
		return SFAudioNode_V1_TypeToTag[NodeType];
	case NDT_SFBackground3DNode:
		if (NodeType >= SFBackground3DNode_V1_Count) return 0;
		return SFBackground3DNode_V1_TypeToTag[NodeType];
	case NDT_SFBackground2DNode:
		if (NodeType >= SFBackground2DNode_V1_Count) return 0;
		return SFBackground2DNode_V1_TypeToTag[NodeType];
	case NDT_SFGeometryNode:
		if (NodeType >= SFGeometryNode_V1_Count) return 0;
		return SFGeometryNode_V1_TypeToTag[NodeType];
	case NDT_SFColorNode:
		if (NodeType >= SFColorNode_V1_Count) return 0;
		return SFColorNode_V1_TypeToTag[NodeType];
	case NDT_SFTextureNode:
		if (NodeType >= SFTextureNode_V1_Count) return 0;
		return SFTextureNode_V1_TypeToTag[NodeType];
	case NDT_SFCoordinateNode:
		if (NodeType >= SFCoordinateNode_V1_Count) return 0;
		return SFCoordinateNode_V1_TypeToTag[NodeType];
	case NDT_SFCoordinate2DNode:
		if (NodeType >= SFCoordinate2DNode_V1_Count) return 0;
		return SFCoordinate2DNode_V1_TypeToTag[NodeType];
	case NDT_SFExpressionNode:
		if (NodeType >= SFExpressionNode_V1_Count) return 0;
		return SFExpressionNode_V1_TypeToTag[NodeType];
	case NDT_SFFaceDefMeshNode:
		if (NodeType >= SFFaceDefMeshNode_V1_Count) return 0;
		return SFFaceDefMeshNode_V1_TypeToTag[NodeType];
	case NDT_SFFaceDefTablesNode:
		if (NodeType >= SFFaceDefTablesNode_V1_Count) return 0;
		return SFFaceDefTablesNode_V1_TypeToTag[NodeType];
	case NDT_SFFaceDefTransformNode:
		if (NodeType >= SFFaceDefTransformNode_V1_Count) return 0;
		return SFFaceDefTransformNode_V1_TypeToTag[NodeType];
	case NDT_SFFAPNode:
		if (NodeType >= SFFAPNode_V1_Count) return 0;
		return SFFAPNode_V1_TypeToTag[NodeType];
	case NDT_SFFDPNode:
		if (NodeType >= SFFDPNode_V1_Count) return 0;
		return SFFDPNode_V1_TypeToTag[NodeType];
	case NDT_SFFITNode:
		if (NodeType >= SFFITNode_V1_Count) return 0;
		return SFFITNode_V1_TypeToTag[NodeType];
	case NDT_SFFogNode:
		if (NodeType >= SFFogNode_V1_Count) return 0;
		return SFFogNode_V1_TypeToTag[NodeType];
	case NDT_SFFontStyleNode:
		if (NodeType >= SFFontStyleNode_V1_Count) return 0;
		return SFFontStyleNode_V1_TypeToTag[NodeType];
	case NDT_SFTopNode:
		if (NodeType >= SFTopNode_V1_Count) return 0;
		return SFTopNode_V1_TypeToTag[NodeType];
	case NDT_SFLinePropertiesNode:
		if (NodeType >= SFLinePropertiesNode_V1_Count) return 0;
		return SFLinePropertiesNode_V1_TypeToTag[NodeType];
	case NDT_SFMaterialNode:
		if (NodeType >= SFMaterialNode_V1_Count) return 0;
		return SFMaterialNode_V1_TypeToTag[NodeType];
	case NDT_SFNavigationInfoNode:
		if (NodeType >= SFNavigationInfoNode_V1_Count) return 0;
		return SFNavigationInfoNode_V1_TypeToTag[NodeType];
	case NDT_SFNormalNode:
		if (NodeType >= SFNormalNode_V1_Count) return 0;
		return SFNormalNode_V1_TypeToTag[NodeType];
	case NDT_SFTextureCoordinateNode:
		if (NodeType >= SFTextureCoordinateNode_V1_Count) return 0;
		return SFTextureCoordinateNode_V1_TypeToTag[NodeType];
	case NDT_SFTextureTransformNode:
		if (NodeType >= SFTextureTransformNode_V1_Count) return 0;
		return SFTextureTransformNode_V1_TypeToTag[NodeType];
	case NDT_SFViewpointNode:
		if (NodeType >= SFViewpointNode_V1_Count) return 0;
		return SFViewpointNode_V1_TypeToTag[NodeType];
	case NDT_SFVisemeNode:
		if (NodeType >= SFVisemeNode_V1_Count) return 0;
		return SFVisemeNode_V1_TypeToTag[NodeType];
	default:
		return 0;
	}
}


u32 NDT_V1_GetNumBits(u32 NDT_Tag)
{
	switch (NDT_Tag) {
	case NDT_SFWorldNode:
		return SFWorldNode_V1_NUMBITS;
	case NDT_SF3DNode:
		return SF3DNode_V1_NUMBITS;
	case NDT_SF2DNode:
		return SF2DNode_V1_NUMBITS;
	case NDT_SFStreamingNode:
		return SFStreamingNode_V1_NUMBITS;
	case NDT_SFAppearanceNode:
		return SFAppearanceNode_V1_NUMBITS;
	case NDT_SFAudioNode:
		return SFAudioNode_V1_NUMBITS;
	case NDT_SFBackground3DNode:
		return SFBackground3DNode_V1_NUMBITS;
	case NDT_SFBackground2DNode:
		return SFBackground2DNode_V1_NUMBITS;
	case NDT_SFGeometryNode:
		return SFGeometryNode_V1_NUMBITS;
	case NDT_SFColorNode:
		return SFColorNode_V1_NUMBITS;
	case NDT_SFTextureNode:
		return SFTextureNode_V1_NUMBITS;
	case NDT_SFCoordinateNode:
		return SFCoordinateNode_V1_NUMBITS;
	case NDT_SFCoordinate2DNode:
		return SFCoordinate2DNode_V1_NUMBITS;
	case NDT_SFExpressionNode:
		return SFExpressionNode_V1_NUMBITS;
	case NDT_SFFaceDefMeshNode:
		return SFFaceDefMeshNode_V1_NUMBITS;
	case NDT_SFFaceDefTablesNode:
		return SFFaceDefTablesNode_V1_NUMBITS;
	case NDT_SFFaceDefTransformNode:
		return SFFaceDefTransformNode_V1_NUMBITS;
	case NDT_SFFAPNode:
		return SFFAPNode_V1_NUMBITS;
	case NDT_SFFDPNode:
		return SFFDPNode_V1_NUMBITS;
	case NDT_SFFITNode:
		return SFFITNode_V1_NUMBITS;
	case NDT_SFFogNode:
		return SFFogNode_V1_NUMBITS;
	case NDT_SFFontStyleNode:
		return SFFontStyleNode_V1_NUMBITS;
	case NDT_SFTopNode:
		return SFTopNode_V1_NUMBITS;
	case NDT_SFLinePropertiesNode:
		return SFLinePropertiesNode_V1_NUMBITS;
	case NDT_SFMaterialNode:
		return SFMaterialNode_V1_NUMBITS;
	case NDT_SFNavigationInfoNode:
		return SFNavigationInfoNode_V1_NUMBITS;
	case NDT_SFNormalNode:
		return SFNormalNode_V1_NUMBITS;
	case NDT_SFTextureCoordinateNode:
		return SFTextureCoordinateNode_V1_NUMBITS;
	case NDT_SFTextureTransformNode:
		return SFTextureTransformNode_V1_NUMBITS;
	case NDT_SFViewpointNode:
		return SFViewpointNode_V1_NUMBITS;
	case NDT_SFVisemeNode:
		return SFVisemeNode_V1_NUMBITS;
	default:
		return 0;
	}
}

u32 NDT_V1_GetNodeType(u32 NDT_Tag, u32 NodeTag)
{
	if(!NDT_Tag || !NodeTag) return 0;
	switch(NDT_Tag) {
	case NDT_SFWorldNode:
		return ALL_GetNodeType(SFWorldNode_V1_TypeToTag, SFWorldNode_V1_Count, NodeTag, GF_BIFS_V1);
	case NDT_SF3DNode:
		return ALL_GetNodeType(SF3DNode_V1_TypeToTag, SF3DNode_V1_Count, NodeTag, GF_BIFS_V1);
	case NDT_SF2DNode:
		return ALL_GetNodeType(SF2DNode_V1_TypeToTag, SF2DNode_V1_Count, NodeTag, GF_BIFS_V1);
	case NDT_SFStreamingNode:
		return ALL_GetNodeType(SFStreamingNode_V1_TypeToTag, SFStreamingNode_V1_Count, NodeTag, GF_BIFS_V1);
	case NDT_SFAppearanceNode:
		return ALL_GetNodeType(SFAppearanceNode_V1_TypeToTag, SFAppearanceNode_V1_Count, NodeTag, GF_BIFS_V1);
	case NDT_SFAudioNode:
		return ALL_GetNodeType(SFAudioNode_V1_TypeToTag, SFAudioNode_V1_Count, NodeTag, GF_BIFS_V1);
	case NDT_SFBackground3DNode:
		return ALL_GetNodeType(SFBackground3DNode_V1_TypeToTag, SFBackground3DNode_V1_Count, NodeTag, GF_BIFS_V1);
	case NDT_SFBackground2DNode:
		return ALL_GetNodeType(SFBackground2DNode_V1_TypeToTag, SFBackground2DNode_V1_Count, NodeTag, GF_BIFS_V1);
	case NDT_SFGeometryNode:
		return ALL_GetNodeType(SFGeometryNode_V1_TypeToTag, SFGeometryNode_V1_Count, NodeTag, GF_BIFS_V1);
	case NDT_SFColorNode:
		return ALL_GetNodeType(SFColorNode_V1_TypeToTag, SFColorNode_V1_Count, NodeTag, GF_BIFS_V1);
	case NDT_SFTextureNode:
		return ALL_GetNodeType(SFTextureNode_V1_TypeToTag, SFTextureNode_V1_Count, NodeTag, GF_BIFS_V1);
	case NDT_SFCoordinateNode:
		return ALL_GetNodeType(SFCoordinateNode_V1_TypeToTag, SFCoordinateNode_V1_Count, NodeTag, GF_BIFS_V1);
	case NDT_SFCoordinate2DNode:
		return ALL_GetNodeType(SFCoordinate2DNode_V1_TypeToTag, SFCoordinate2DNode_V1_Count, NodeTag, GF_BIFS_V1);
	case NDT_SFExpressionNode:
		return ALL_GetNodeType(SFExpressionNode_V1_TypeToTag, SFExpressionNode_V1_Count, NodeTag, GF_BIFS_V1);
	case NDT_SFFaceDefMeshNode:
		return ALL_GetNodeType(SFFaceDefMeshNode_V1_TypeToTag, SFFaceDefMeshNode_V1_Count, NodeTag, GF_BIFS_V1);
	case NDT_SFFaceDefTablesNode:
		return ALL_GetNodeType(SFFaceDefTablesNode_V1_TypeToTag, SFFaceDefTablesNode_V1_Count, NodeTag, GF_BIFS_V1);
	case NDT_SFFaceDefTransformNode:
		return ALL_GetNodeType(SFFaceDefTransformNode_V1_TypeToTag, SFFaceDefTransformNode_V1_Count, NodeTag, GF_BIFS_V1);
	case NDT_SFFAPNode:
		return ALL_GetNodeType(SFFAPNode_V1_TypeToTag, SFFAPNode_V1_Count, NodeTag, GF_BIFS_V1);
	case NDT_SFFDPNode:
		return ALL_GetNodeType(SFFDPNode_V1_TypeToTag, SFFDPNode_V1_Count, NodeTag, GF_BIFS_V1);
	case NDT_SFFITNode:
		return ALL_GetNodeType(SFFITNode_V1_TypeToTag, SFFITNode_V1_Count, NodeTag, GF_BIFS_V1);
	case NDT_SFFogNode:
		return ALL_GetNodeType(SFFogNode_V1_TypeToTag, SFFogNode_V1_Count, NodeTag, GF_BIFS_V1);
	case NDT_SFFontStyleNode:
		return ALL_GetNodeType(SFFontStyleNode_V1_TypeToTag, SFFontStyleNode_V1_Count, NodeTag, GF_BIFS_V1);
	case NDT_SFTopNode:
		return ALL_GetNodeType(SFTopNode_V1_TypeToTag, SFTopNode_V1_Count, NodeTag, GF_BIFS_V1);
	case NDT_SFLinePropertiesNode:
		return ALL_GetNodeType(SFLinePropertiesNode_V1_TypeToTag, SFLinePropertiesNode_V1_Count, NodeTag, GF_BIFS_V1);
	case NDT_SFMaterialNode:
		return ALL_GetNodeType(SFMaterialNode_V1_TypeToTag, SFMaterialNode_V1_Count, NodeTag, GF_BIFS_V1);
	case NDT_SFNavigationInfoNode:
		return ALL_GetNodeType(SFNavigationInfoNode_V1_TypeToTag, SFNavigationInfoNode_V1_Count, NodeTag, GF_BIFS_V1);
	case NDT_SFNormalNode:
		return ALL_GetNodeType(SFNormalNode_V1_TypeToTag, SFNormalNode_V1_Count, NodeTag, GF_BIFS_V1);
	case NDT_SFTextureCoordinateNode:
		return ALL_GetNodeType(SFTextureCoordinateNode_V1_TypeToTag, SFTextureCoordinateNode_V1_Count, NodeTag, GF_BIFS_V1);
	case NDT_SFTextureTransformNode:
		return ALL_GetNodeType(SFTextureTransformNode_V1_TypeToTag, SFTextureTransformNode_V1_Count, NodeTag, GF_BIFS_V1);
	case NDT_SFViewpointNode:
		return ALL_GetNodeType(SFViewpointNode_V1_TypeToTag, SFViewpointNode_V1_Count, NodeTag, GF_BIFS_V1);
	case NDT_SFVisemeNode:
		return ALL_GetNodeType(SFVisemeNode_V1_TypeToTag, SFVisemeNode_V1_Count, NodeTag, GF_BIFS_V1);
	default:
		return 0;
	}
}




u32 NDT_V2_GetNodeTag(u32 Context_NDT_Tag, u32 NodeType)
{
	if (!NodeType) return 0;
	/* adjust according to the table version */
	/* v2: 0 reserved for extensions, 1 reserved for protos */
	if (NodeType == 1) return 0;
	NodeType -= 2;
	switch (Context_NDT_Tag) {
	case NDT_SFWorldNode:
		if (NodeType >= SFWorldNode_V2_Count) return 0;
		return SFWorldNode_V2_TypeToTag[NodeType];
	case NDT_SF3DNode:
		if (NodeType >= SF3DNode_V2_Count) return 0;
		return SF3DNode_V2_TypeToTag[NodeType];
	case NDT_SF2DNode:
		if (NodeType >= SF2DNode_V2_Count) return 0;
		return SF2DNode_V2_TypeToTag[NodeType];
	case NDT_SFGeometryNode:
		if (NodeType >= SFGeometryNode_V2_Count) return 0;
		return SFGeometryNode_V2_TypeToTag[NodeType];
	case NDT_SFMaterialNode:
		if (NodeType >= SFMaterialNode_V2_Count) return 0;
		return SFMaterialNode_V2_TypeToTag[NodeType];
	case NDT_SFBAPNode:
		if (NodeType >= SFBAPNode_V2_Count) return 0;
		return SFBAPNode_V2_TypeToTag[NodeType];
	case NDT_SFBDPNode:
		if (NodeType >= SFBDPNode_V2_Count) return 0;
		return SFBDPNode_V2_TypeToTag[NodeType];
	case NDT_SFBodyDefTableNode:
		if (NodeType >= SFBodyDefTableNode_V2_Count) return 0;
		return SFBodyDefTableNode_V2_TypeToTag[NodeType];
	case NDT_SFBodySegmentConnectionHintNode:
		if (NodeType >= SFBodySegmentConnectionHintNode_V2_Count) return 0;
		return SFBodySegmentConnectionHintNode_V2_TypeToTag[NodeType];
	case NDT_SFPerceptualParameterNode:
		if (NodeType >= SFPerceptualParameterNode_V2_Count) return 0;
		return SFPerceptualParameterNode_V2_TypeToTag[NodeType];
	default:
		return 0;
	}
}


u32 NDT_V2_GetNumBits(u32 NDT_Tag)
{
	switch (NDT_Tag) {
	case NDT_SFWorldNode:
		return SFWorldNode_V2_NUMBITS;
	case NDT_SF3DNode:
		return SF3DNode_V2_NUMBITS;
	case NDT_SF2DNode:
		return SF2DNode_V2_NUMBITS;
	case NDT_SFGeometryNode:
		return SFGeometryNode_V2_NUMBITS;
	case NDT_SFMaterialNode:
		return SFMaterialNode_V2_NUMBITS;
	case NDT_SFBAPNode:
		return SFBAPNode_V2_NUMBITS;
	case NDT_SFBDPNode:
		return SFBDPNode_V2_NUMBITS;
	case NDT_SFBodyDefTableNode:
		return SFBodyDefTableNode_V2_NUMBITS;
	case NDT_SFBodySegmentConnectionHintNode:
		return SFBodySegmentConnectionHintNode_V2_NUMBITS;
	case NDT_SFPerceptualParameterNode:
		return SFPerceptualParameterNode_V2_NUMBITS;
	default:
		return 1;
	}
}

u32 NDT_V2_GetNodeType(u32 NDT_Tag, u32 NodeTag)
{
	if(!NDT_Tag || !NodeTag) return 0;
	switch(NDT_Tag) {
	case NDT_SFWorldNode:
		return ALL_GetNodeType(SFWorldNode_V2_TypeToTag, SFWorldNode_V2_Count, NodeTag, GF_BIFS_V2);
	case NDT_SF3DNode:
		return ALL_GetNodeType(SF3DNode_V2_TypeToTag, SF3DNode_V2_Count, NodeTag, GF_BIFS_V2);
	case NDT_SF2DNode:
		return ALL_GetNodeType(SF2DNode_V2_TypeToTag, SF2DNode_V2_Count, NodeTag, GF_BIFS_V2);
	case NDT_SFGeometryNode:
		return ALL_GetNodeType(SFGeometryNode_V2_TypeToTag, SFGeometryNode_V2_Count, NodeTag, GF_BIFS_V2);
	case NDT_SFMaterialNode:
		return ALL_GetNodeType(SFMaterialNode_V2_TypeToTag, SFMaterialNode_V2_Count, NodeTag, GF_BIFS_V2);
	case NDT_SFBAPNode:
		return ALL_GetNodeType(SFBAPNode_V2_TypeToTag, SFBAPNode_V2_Count, NodeTag, GF_BIFS_V2);
	case NDT_SFBDPNode:
		return ALL_GetNodeType(SFBDPNode_V2_TypeToTag, SFBDPNode_V2_Count, NodeTag, GF_BIFS_V2);
	case NDT_SFBodyDefTableNode:
		return ALL_GetNodeType(SFBodyDefTableNode_V2_TypeToTag, SFBodyDefTableNode_V2_Count, NodeTag, GF_BIFS_V2);
	case NDT_SFBodySegmentConnectionHintNode:
		return ALL_GetNodeType(SFBodySegmentConnectionHintNode_V2_TypeToTag, SFBodySegmentConnectionHintNode_V2_Count, NodeTag, GF_BIFS_V2);
	case NDT_SFPerceptualParameterNode:
		return ALL_GetNodeType(SFPerceptualParameterNode_V2_TypeToTag, SFPerceptualParameterNode_V2_Count, NodeTag, GF_BIFS_V2);
	default:
		return 0;
	}
}




u32 NDT_V3_GetNodeTag(u32 Context_NDT_Tag, u32 NodeType)
{
	if (!NodeType) return 0;
	/* adjust according to the table version */
	/* v3: 0 reserved for extensions */
	NodeType -= 1;
	switch (Context_NDT_Tag) {
	case NDT_SFWorldNode:
		if (NodeType >= SFWorldNode_V3_Count) return 0;
		return SFWorldNode_V3_TypeToTag[NodeType];
	case NDT_SF3DNode:
		if (NodeType >= SF3DNode_V3_Count) return 0;
		return SF3DNode_V3_TypeToTag[NodeType];
	case NDT_SF2DNode:
		if (NodeType >= SF2DNode_V3_Count) return 0;
		return SF2DNode_V3_TypeToTag[NodeType];
	case NDT_SFTemporalNode:
		if (NodeType >= SFTemporalNode_V3_Count) return 0;
		return SFTemporalNode_V3_TypeToTag[NodeType];
	default:
		return 0;
	}
}


u32 NDT_V3_GetNumBits(u32 NDT_Tag)
{
	switch (NDT_Tag) {
	case NDT_SFWorldNode:
		return SFWorldNode_V3_NUMBITS;
	case NDT_SF3DNode:
		return SF3DNode_V3_NUMBITS;
	case NDT_SF2DNode:
		return SF2DNode_V3_NUMBITS;
	case NDT_SFTemporalNode:
		return SFTemporalNode_V3_NUMBITS;
	default:
		return 0;
	}
}

u32 NDT_V3_GetNodeType(u32 NDT_Tag, u32 NodeTag)
{
	if(!NDT_Tag || !NodeTag) return 0;
	switch(NDT_Tag) {
	case NDT_SFWorldNode:
		return ALL_GetNodeType(SFWorldNode_V3_TypeToTag, SFWorldNode_V3_Count, NodeTag, GF_BIFS_V3);
	case NDT_SF3DNode:
		return ALL_GetNodeType(SF3DNode_V3_TypeToTag, SF3DNode_V3_Count, NodeTag, GF_BIFS_V3);
	case NDT_SF2DNode:
		return ALL_GetNodeType(SF2DNode_V3_TypeToTag, SF2DNode_V3_Count, NodeTag, GF_BIFS_V3);
	case NDT_SFTemporalNode:
		return ALL_GetNodeType(SFTemporalNode_V3_TypeToTag, SFTemporalNode_V3_Count, NodeTag, GF_BIFS_V3);
	default:
		return 0;
	}
}




u32 NDT_V4_GetNodeTag(u32 Context_NDT_Tag, u32 NodeType)
{
	if (!NodeType) return 0;
	/* adjust according to the table version */
	/* v4: 0 reserved for extensions */
	NodeType -= 1;
	switch (Context_NDT_Tag) {
	case NDT_SFWorldNode:
		if (NodeType >= SFWorldNode_V4_Count) return 0;
		return SFWorldNode_V4_TypeToTag[NodeType];
	case NDT_SF3DNode:
		if (NodeType >= SF3DNode_V4_Count) return 0;
		return SF3DNode_V4_TypeToTag[NodeType];
	case NDT_SF2DNode:
		if (NodeType >= SF2DNode_V4_Count) return 0;
		return SF2DNode_V4_TypeToTag[NodeType];
	case NDT_SFTextureNode:
		if (NodeType >= SFTextureNode_V4_Count) return 0;
		return SFTextureNode_V4_TypeToTag[NodeType];
	default:
		return 0;
	}
}


u32 NDT_V4_GetNumBits(u32 NDT_Tag)
{
	switch (NDT_Tag) {
	case NDT_SFWorldNode:
		return SFWorldNode_V4_NUMBITS;
	case NDT_SF3DNode:
		return SF3DNode_V4_NUMBITS;
	case NDT_SF2DNode:
		return SF2DNode_V4_NUMBITS;
	case NDT_SFTextureNode:
		return SFTextureNode_V4_NUMBITS;
	default:
		return 0;
	}
}

u32 NDT_V4_GetNodeType(u32 NDT_Tag, u32 NodeTag)
{
	if(!NDT_Tag || !NodeTag) return 0;
	switch(NDT_Tag) {
	case NDT_SFWorldNode:
		return ALL_GetNodeType(SFWorldNode_V4_TypeToTag, SFWorldNode_V4_Count, NodeTag, GF_BIFS_V4);
	case NDT_SF3DNode:
		return ALL_GetNodeType(SF3DNode_V4_TypeToTag, SF3DNode_V4_Count, NodeTag, GF_BIFS_V4);
	case NDT_SF2DNode:
		return ALL_GetNodeType(SF2DNode_V4_TypeToTag, SF2DNode_V4_Count, NodeTag, GF_BIFS_V4);
	case NDT_SFTextureNode:
		return ALL_GetNodeType(SFTextureNode_V4_TypeToTag, SFTextureNode_V4_Count, NodeTag, GF_BIFS_V4);
	default:
		return 0;
	}
}




u32 NDT_V5_GetNodeTag(u32 Context_NDT_Tag, u32 NodeType)
{
	if (!NodeType) return 0;
	/* adjust according to the table version */
	/* v5: 0 reserved for extensions */
	NodeType -= 1;
	switch (Context_NDT_Tag) {
	case NDT_SFWorldNode:
		if (NodeType >= SFWorldNode_V5_Count) return 0;
		return SFWorldNode_V5_TypeToTag[NodeType];
	case NDT_SF3DNode:
		if (NodeType >= SF3DNode_V5_Count) return 0;
		return SF3DNode_V5_TypeToTag[NodeType];
	case NDT_SF2DNode:
		if (NodeType >= SF2DNode_V5_Count) return 0;
		return SF2DNode_V5_TypeToTag[NodeType];
	case NDT_SFAppearanceNode:
		if (NodeType >= SFAppearanceNode_V5_Count) return 0;
		return SFAppearanceNode_V5_TypeToTag[NodeType];
	case NDT_SFGeometryNode:
		if (NodeType >= SFGeometryNode_V5_Count) return 0;
		return SFGeometryNode_V5_TypeToTag[NodeType];
	case NDT_SFTextureNode:
		if (NodeType >= SFTextureNode_V5_Count) return 0;
		return SFTextureNode_V5_TypeToTag[NodeType];
	case NDT_SFDepthImageNode:
		if (NodeType >= SFDepthImageNode_V5_Count) return 0;
		return SFDepthImageNode_V5_TypeToTag[NodeType];
	case NDT_SFBlendListNode:
		if (NodeType >= SFBlendListNode_V5_Count) return 0;
		return SFBlendListNode_V5_TypeToTag[NodeType];
	case NDT_SFFrameListNode:
		if (NodeType >= SFFrameListNode_V5_Count) return 0;
		return SFFrameListNode_V5_TypeToTag[NodeType];
	case NDT_SFLightMapNode:
		if (NodeType >= SFLightMapNode_V5_Count) return 0;
		return SFLightMapNode_V5_TypeToTag[NodeType];
	case NDT_SFSurfaceMapNode:
		if (NodeType >= SFSurfaceMapNode_V5_Count) return 0;
		return SFSurfaceMapNode_V5_TypeToTag[NodeType];
	case NDT_SFViewMapNode:
		if (NodeType >= SFViewMapNode_V5_Count) return 0;
		return SFViewMapNode_V5_TypeToTag[NodeType];
	case NDT_SFParticleInitializerNode:
		if (NodeType >= SFParticleInitializerNode_V5_Count) return 0;
		return SFParticleInitializerNode_V5_TypeToTag[NodeType];
	case NDT_SFInfluenceNode:
		if (NodeType >= SFInfluenceNode_V5_Count) return 0;
		return SFInfluenceNode_V5_TypeToTag[NodeType];
	case NDT_SFDepthTextureNode:
		if (NodeType >= SFDepthTextureNode_V5_Count) return 0;
		return SFDepthTextureNode_V5_TypeToTag[NodeType];
	case NDT_SFSBBoneNode:
		if (NodeType >= SFSBBoneNode_V5_Count) return 0;
		return SFSBBoneNode_V5_TypeToTag[NodeType];
	case NDT_SFSBMuscleNode:
		if (NodeType >= SFSBMuscleNode_V5_Count) return 0;
		return SFSBMuscleNode_V5_TypeToTag[NodeType];
	case NDT_SFSBSegmentNode:
		if (NodeType >= SFSBSegmentNode_V5_Count) return 0;
		return SFSBSegmentNode_V5_TypeToTag[NodeType];
	case NDT_SFSBSiteNode:
		if (NodeType >= SFSBSiteNode_V5_Count) return 0;
		return SFSBSiteNode_V5_TypeToTag[NodeType];
	case NDT_SFBaseMeshNode:
		if (NodeType >= SFBaseMeshNode_V5_Count) return 0;
		return SFBaseMeshNode_V5_TypeToTag[NodeType];
	case NDT_SFSubdivSurfaceSectorNode:
		if (NodeType >= SFSubdivSurfaceSectorNode_V5_Count) return 0;
		return SFSubdivSurfaceSectorNode_V5_TypeToTag[NodeType];
	default:
		return 0;
	}
}


u32 NDT_V5_GetNumBits(u32 NDT_Tag)
{
	switch (NDT_Tag) {
	case NDT_SFWorldNode:
		return SFWorldNode_V5_NUMBITS;
	case NDT_SF3DNode:
		return SF3DNode_V5_NUMBITS;
	case NDT_SF2DNode:
		return SF2DNode_V5_NUMBITS;
	case NDT_SFAppearanceNode:
		return SFAppearanceNode_V5_NUMBITS;
	case NDT_SFGeometryNode:
		return SFGeometryNode_V5_NUMBITS;
	case NDT_SFTextureNode:
		return SFTextureNode_V5_NUMBITS;
	case NDT_SFDepthImageNode:
		return SFDepthImageNode_V5_NUMBITS;
	case NDT_SFBlendListNode:
		return SFBlendListNode_V5_NUMBITS;
	case NDT_SFFrameListNode:
		return SFFrameListNode_V5_NUMBITS;
	case NDT_SFLightMapNode:
		return SFLightMapNode_V5_NUMBITS;
	case NDT_SFSurfaceMapNode:
		return SFSurfaceMapNode_V5_NUMBITS;
	case NDT_SFViewMapNode:
		return SFViewMapNode_V5_NUMBITS;
	case NDT_SFParticleInitializerNode:
		return SFParticleInitializerNode_V5_NUMBITS;
	case NDT_SFInfluenceNode:
		return SFInfluenceNode_V5_NUMBITS;
	case NDT_SFDepthTextureNode:
		return SFDepthTextureNode_V5_NUMBITS;
	case NDT_SFSBBoneNode:
		return SFSBBoneNode_V5_NUMBITS;
	case NDT_SFSBMuscleNode:
		return SFSBMuscleNode_V5_NUMBITS;
	case NDT_SFSBSegmentNode:
		return SFSBSegmentNode_V5_NUMBITS;
	case NDT_SFSBSiteNode:
		return SFSBSiteNode_V5_NUMBITS;
	case NDT_SFBaseMeshNode:
		return SFBaseMeshNode_V5_NUMBITS;
	case NDT_SFSubdivSurfaceSectorNode:
		return SFSubdivSurfaceSectorNode_V5_NUMBITS;
	default:
		return 0;
	}
}

u32 NDT_V5_GetNodeType(u32 NDT_Tag, u32 NodeTag)
{
	if(!NDT_Tag || !NodeTag) return 0;
	switch(NDT_Tag) {
	case NDT_SFWorldNode:
		return ALL_GetNodeType(SFWorldNode_V5_TypeToTag, SFWorldNode_V5_Count, NodeTag, GF_BIFS_V5);
	case NDT_SF3DNode:
		return ALL_GetNodeType(SF3DNode_V5_TypeToTag, SF3DNode_V5_Count, NodeTag, GF_BIFS_V5);
	case NDT_SF2DNode:
		return ALL_GetNodeType(SF2DNode_V5_TypeToTag, SF2DNode_V5_Count, NodeTag, GF_BIFS_V5);
	case NDT_SFAppearanceNode:
		return ALL_GetNodeType(SFAppearanceNode_V5_TypeToTag, SFAppearanceNode_V5_Count, NodeTag, GF_BIFS_V5);
	case NDT_SFGeometryNode:
		return ALL_GetNodeType(SFGeometryNode_V5_TypeToTag, SFGeometryNode_V5_Count, NodeTag, GF_BIFS_V5);
	case NDT_SFTextureNode:
		return ALL_GetNodeType(SFTextureNode_V5_TypeToTag, SFTextureNode_V5_Count, NodeTag, GF_BIFS_V5);
	case NDT_SFDepthImageNode:
		return ALL_GetNodeType(SFDepthImageNode_V5_TypeToTag, SFDepthImageNode_V5_Count, NodeTag, GF_BIFS_V5);
	case NDT_SFBlendListNode:
		return ALL_GetNodeType(SFBlendListNode_V5_TypeToTag, SFBlendListNode_V5_Count, NodeTag, GF_BIFS_V5);
	case NDT_SFFrameListNode:
		return ALL_GetNodeType(SFFrameListNode_V5_TypeToTag, SFFrameListNode_V5_Count, NodeTag, GF_BIFS_V5);
	case NDT_SFLightMapNode:
		return ALL_GetNodeType(SFLightMapNode_V5_TypeToTag, SFLightMapNode_V5_Count, NodeTag, GF_BIFS_V5);
	case NDT_SFSurfaceMapNode:
		return ALL_GetNodeType(SFSurfaceMapNode_V5_TypeToTag, SFSurfaceMapNode_V5_Count, NodeTag, GF_BIFS_V5);
	case NDT_SFViewMapNode:
		return ALL_GetNodeType(SFViewMapNode_V5_TypeToTag, SFViewMapNode_V5_Count, NodeTag, GF_BIFS_V5);
	case NDT_SFParticleInitializerNode:
		return ALL_GetNodeType(SFParticleInitializerNode_V5_TypeToTag, SFParticleInitializerNode_V5_Count, NodeTag, GF_BIFS_V5);
	case NDT_SFInfluenceNode:
		return ALL_GetNodeType(SFInfluenceNode_V5_TypeToTag, SFInfluenceNode_V5_Count, NodeTag, GF_BIFS_V5);
	case NDT_SFDepthTextureNode:
		return ALL_GetNodeType(SFDepthTextureNode_V5_TypeToTag, SFDepthTextureNode_V5_Count, NodeTag, GF_BIFS_V5);
	case NDT_SFSBBoneNode:
		return ALL_GetNodeType(SFSBBoneNode_V5_TypeToTag, SFSBBoneNode_V5_Count, NodeTag, GF_BIFS_V5);
	case NDT_SFSBMuscleNode:
		return ALL_GetNodeType(SFSBMuscleNode_V5_TypeToTag, SFSBMuscleNode_V5_Count, NodeTag, GF_BIFS_V5);
	case NDT_SFSBSegmentNode:
		return ALL_GetNodeType(SFSBSegmentNode_V5_TypeToTag, SFSBSegmentNode_V5_Count, NodeTag, GF_BIFS_V5);
	case NDT_SFSBSiteNode:
		return ALL_GetNodeType(SFSBSiteNode_V5_TypeToTag, SFSBSiteNode_V5_Count, NodeTag, GF_BIFS_V5);
	case NDT_SFBaseMeshNode:
		return ALL_GetNodeType(SFBaseMeshNode_V5_TypeToTag, SFBaseMeshNode_V5_Count, NodeTag, GF_BIFS_V5);
	case NDT_SFSubdivSurfaceSectorNode:
		return ALL_GetNodeType(SFSubdivSurfaceSectorNode_V5_TypeToTag, SFSubdivSurfaceSectorNode_V5_Count, NodeTag, GF_BIFS_V5);
	default:
		return 0;
	}
}




u32 NDT_V6_GetNodeTag(u32 Context_NDT_Tag, u32 NodeType)
{
	if (!NodeType) return 0;
	/* adjust according to the table version */
	/* v6: 0 reserved for extensions */
	NodeType -= 1;
	switch (Context_NDT_Tag) {
	case NDT_SFWorldNode:
		if (NodeType >= SFWorldNode_V6_Count) return 0;
		return SFWorldNode_V6_TypeToTag[NodeType];
	case NDT_SF3DNode:
		if (NodeType >= SF3DNode_V6_Count) return 0;
		return SF3DNode_V6_TypeToTag[NodeType];
	case NDT_SF2DNode:
		if (NodeType >= SF2DNode_V6_Count) return 0;
		return SF2DNode_V6_TypeToTag[NodeType];
	case NDT_SFGeometryNode:
		if (NodeType >= SFGeometryNode_V6_Count) return 0;
		return SFGeometryNode_V6_TypeToTag[NodeType];
	case NDT_SFTextureNode:
		if (NodeType >= SFTextureNode_V6_Count) return 0;
		return SFTextureNode_V6_TypeToTag[NodeType];
	case NDT_SFFontStyleNode:
		if (NodeType >= SFFontStyleNode_V6_Count) return 0;
		return SFFontStyleNode_V6_TypeToTag[NodeType];
	case NDT_SFLinePropertiesNode:
		if (NodeType >= SFLinePropertiesNode_V6_Count) return 0;
		return SFLinePropertiesNode_V6_TypeToTag[NodeType];
	case NDT_SFTextureTransformNode:
		if (NodeType >= SFTextureTransformNode_V6_Count) return 0;
		return SFTextureTransformNode_V6_TypeToTag[NodeType];
	case NDT_SFViewportNode:
		if (NodeType >= SFViewportNode_V6_Count) return 0;
		return SFViewportNode_V6_TypeToTag[NodeType];
	default:
		return 0;
	}
}


u32 NDT_V6_GetNumBits(u32 NDT_Tag)
{
	switch (NDT_Tag) {
	case NDT_SFWorldNode:
		return SFWorldNode_V6_NUMBITS;
	case NDT_SF3DNode:
		return SF3DNode_V6_NUMBITS;
	case NDT_SF2DNode:
		return SF2DNode_V6_NUMBITS;
	case NDT_SFGeometryNode:
		return SFGeometryNode_V6_NUMBITS;
	case NDT_SFTextureNode:
		return SFTextureNode_V6_NUMBITS;
	case NDT_SFFontStyleNode:
		return SFFontStyleNode_V6_NUMBITS;
	case NDT_SFLinePropertiesNode:
		return SFLinePropertiesNode_V6_NUMBITS;
	case NDT_SFTextureTransformNode:
		return SFTextureTransformNode_V6_NUMBITS;
	case NDT_SFViewportNode:
		return SFViewportNode_V6_NUMBITS;
	default:
		return 0;
	}
}

u32 NDT_V6_GetNodeType(u32 NDT_Tag, u32 NodeTag)
{
	if(!NDT_Tag || !NodeTag) return 0;
	switch(NDT_Tag) {
	case NDT_SFWorldNode:
		return ALL_GetNodeType(SFWorldNode_V6_TypeToTag, SFWorldNode_V6_Count, NodeTag, GF_BIFS_V6);
	case NDT_SF3DNode:
		return ALL_GetNodeType(SF3DNode_V6_TypeToTag, SF3DNode_V6_Count, NodeTag, GF_BIFS_V6);
	case NDT_SF2DNode:
		return ALL_GetNodeType(SF2DNode_V6_TypeToTag, SF2DNode_V6_Count, NodeTag, GF_BIFS_V6);
	case NDT_SFGeometryNode:
		return ALL_GetNodeType(SFGeometryNode_V6_TypeToTag, SFGeometryNode_V6_Count, NodeTag, GF_BIFS_V6);
	case NDT_SFTextureNode:
		return ALL_GetNodeType(SFTextureNode_V6_TypeToTag, SFTextureNode_V6_Count, NodeTag, GF_BIFS_V6);
	case NDT_SFFontStyleNode:
		return ALL_GetNodeType(SFFontStyleNode_V6_TypeToTag, SFFontStyleNode_V6_Count, NodeTag, GF_BIFS_V6);
	case NDT_SFLinePropertiesNode:
		return ALL_GetNodeType(SFLinePropertiesNode_V6_TypeToTag, SFLinePropertiesNode_V6_Count, NodeTag, GF_BIFS_V6);
	case NDT_SFTextureTransformNode:
		return ALL_GetNodeType(SFTextureTransformNode_V6_TypeToTag, SFTextureTransformNode_V6_Count, NodeTag, GF_BIFS_V6);
	case NDT_SFViewportNode:
		return ALL_GetNodeType(SFViewportNode_V6_TypeToTag, SFViewportNode_V6_Count, NodeTag, GF_BIFS_V6);
	default:
		return 0;
	}
}




u32 NDT_V7_GetNodeTag(u32 Context_NDT_Tag, u32 NodeType)
{
	if (!NodeType) return 0;
	/* adjust according to the table version */
	/* v7: 0 reserved for extensions */
	NodeType -= 1;
	switch (Context_NDT_Tag) {
	case NDT_SFWorldNode:
		if (NodeType >= SFWorldNode_V7_Count) return 0;
		return SFWorldNode_V7_TypeToTag[NodeType];
	case NDT_SF3DNode:
		if (NodeType >= SF3DNode_V7_Count) return 0;
		return SF3DNode_V7_TypeToTag[NodeType];
	case NDT_SF2DNode:
		if (NodeType >= SF2DNode_V7_Count) return 0;
		return SF2DNode_V7_TypeToTag[NodeType];
	case NDT_SFAudioNode:
		if (NodeType >= SFAudioNode_V7_Count) return 0;
		return SFAudioNode_V7_TypeToTag[NodeType];
	case NDT_SFTextureNode:
		if (NodeType >= SFTextureNode_V7_Count) return 0;
		return SFTextureNode_V7_TypeToTag[NodeType];
	case NDT_SFDepthImageNode:
		if (NodeType >= SFDepthImageNode_V7_Count) return 0;
		return SFDepthImageNode_V7_TypeToTag[NodeType];
	case NDT_SFDepthTextureNode:
		if (NodeType >= SFDepthTextureNode_V7_Count) return 0;
		return SFDepthTextureNode_V7_TypeToTag[NodeType];
	default:
		return 0;
	}
}


u32 NDT_V7_GetNumBits(u32 NDT_Tag)
{
	switch (NDT_Tag) {
	case NDT_SFWorldNode:
		return SFWorldNode_V7_NUMBITS;
	case NDT_SF3DNode:
		return SF3DNode_V7_NUMBITS;
	case NDT_SF2DNode:
		return SF2DNode_V7_NUMBITS;
	case NDT_SFAudioNode:
		return SFAudioNode_V7_NUMBITS;
	case NDT_SFTextureNode:
		return SFTextureNode_V7_NUMBITS;
	case NDT_SFDepthImageNode:
		return SFDepthImageNode_V7_NUMBITS;
	case NDT_SFDepthTextureNode:
		return SFDepthTextureNode_V7_NUMBITS;
	default:
		return 0;
	}
}

u32 NDT_V7_GetNodeType(u32 NDT_Tag, u32 NodeTag)
{
	if(!NDT_Tag || !NodeTag) return 0;
	switch(NDT_Tag) {
	case NDT_SFWorldNode:
		return ALL_GetNodeType(SFWorldNode_V7_TypeToTag, SFWorldNode_V7_Count, NodeTag, GF_BIFS_V7);
	case NDT_SF3DNode:
		return ALL_GetNodeType(SF3DNode_V7_TypeToTag, SF3DNode_V7_Count, NodeTag, GF_BIFS_V7);
	case NDT_SF2DNode:
		return ALL_GetNodeType(SF2DNode_V7_TypeToTag, SF2DNode_V7_Count, NodeTag, GF_BIFS_V7);
	case NDT_SFAudioNode:
		return ALL_GetNodeType(SFAudioNode_V7_TypeToTag, SFAudioNode_V7_Count, NodeTag, GF_BIFS_V7);
	case NDT_SFTextureNode:
		return ALL_GetNodeType(SFTextureNode_V7_TypeToTag, SFTextureNode_V7_Count, NodeTag, GF_BIFS_V7);
	case NDT_SFDepthImageNode:
		return ALL_GetNodeType(SFDepthImageNode_V7_TypeToTag, SFDepthImageNode_V7_Count, NodeTag, GF_BIFS_V7);
	case NDT_SFDepthTextureNode:
		return ALL_GetNodeType(SFDepthTextureNode_V7_TypeToTag, SFDepthTextureNode_V7_Count, NodeTag, GF_BIFS_V7);
	default:
		return 0;
	}
}




u32 NDT_V8_GetNodeTag(u32 Context_NDT_Tag, u32 NodeType)
{
	if (!NodeType) return 0;
	/* adjust according to the table version */
	/* v8: 0 reserved for extensions */
	NodeType -= 1;
	switch (Context_NDT_Tag) {
	case NDT_SFWorldNode:
		if (NodeType >= SFWorldNode_V8_Count) return 0;
		return SFWorldNode_V8_TypeToTag[NodeType];
	case NDT_SF3DNode:
		if (NodeType >= SF3DNode_V8_Count) return 0;
		return SF3DNode_V8_TypeToTag[NodeType];
	case NDT_SF2DNode:
		if (NodeType >= SF2DNode_V8_Count) return 0;
		return SF2DNode_V8_TypeToTag[NodeType];
	case NDT_SFMusicScoreNode:
		if (NodeType >= SFMusicScoreNode_V8_Count) return 0;
		return SFMusicScoreNode_V8_TypeToTag[NodeType];
	default:
		return 0;
	}
}


u32 NDT_V8_GetNumBits(u32 NDT_Tag)
{
	switch (NDT_Tag) {
	case NDT_SFWorldNode:
		return SFWorldNode_V8_NUMBITS;
	case NDT_SF3DNode:
		return SF3DNode_V8_NUMBITS;
	case NDT_SF2DNode:
		return SF2DNode_V8_NUMBITS;
	case NDT_SFMusicScoreNode:
		return SFMusicScoreNode_V8_NUMBITS;
	default:
		return 0;
	}
}

u32 NDT_V8_GetNodeType(u32 NDT_Tag, u32 NodeTag)
{
	if(!NDT_Tag || !NodeTag) return 0;
	switch(NDT_Tag) {
	case NDT_SFWorldNode:
		return ALL_GetNodeType(SFWorldNode_V8_TypeToTag, SFWorldNode_V8_Count, NodeTag, GF_BIFS_V8);
	case NDT_SF3DNode:
		return ALL_GetNodeType(SF3DNode_V8_TypeToTag, SF3DNode_V8_Count, NodeTag, GF_BIFS_V8);
	case NDT_SF2DNode:
		return ALL_GetNodeType(SF2DNode_V8_TypeToTag, SF2DNode_V8_Count, NodeTag, GF_BIFS_V8);
	case NDT_SFMusicScoreNode:
		return ALL_GetNodeType(SFMusicScoreNode_V8_TypeToTag, SFMusicScoreNode_V8_Count, NodeTag, GF_BIFS_V8);
	default:
		return 0;
	}
}




u32 NDT_V9_GetNodeTag(u32 Context_NDT_Tag, u32 NodeType)
{
	if (!NodeType) return 0;
	/* adjust according to the table version */
	/* v9: 0 reserved for extensions */
	NodeType -= 1;
	switch (Context_NDT_Tag) {
	case NDT_SFWorldNode:
		if (NodeType >= SFWorldNode_V9_Count) return 0;
		return SFWorldNode_V9_TypeToTag[NodeType];
	case NDT_SF3DNode:
		if (NodeType >= SF3DNode_V9_Count) return 0;
		return SF3DNode_V9_TypeToTag[NodeType];
	case NDT_SFGeometryNode:
		if (NodeType >= SFGeometryNode_V9_Count) return 0;
		return SFGeometryNode_V9_TypeToTag[NodeType];
	default:
		return 0;
	}
}


u32 NDT_V9_GetNumBits(u32 NDT_Tag)
{
	switch (NDT_Tag) {
	case NDT_SFWorldNode:
		return SFWorldNode_V9_NUMBITS;
	case NDT_SF3DNode:
		return SF3DNode_V9_NUMBITS;
	case NDT_SFGeometryNode:
		return SFGeometryNode_V9_NUMBITS;
	default:
		return 0;
	}
}

u32 NDT_V9_GetNodeType(u32 NDT_Tag, u32 NodeTag)
{
	if(!NDT_Tag || !NodeTag) return 0;
	switch(NDT_Tag) {
	case NDT_SFWorldNode:
		return ALL_GetNodeType(SFWorldNode_V9_TypeToTag, SFWorldNode_V9_Count, NodeTag, GF_BIFS_V9);
	case NDT_SF3DNode:
		return ALL_GetNodeType(SF3DNode_V9_TypeToTag, SF3DNode_V9_Count, NodeTag, GF_BIFS_V9);
	case NDT_SFGeometryNode:
		return ALL_GetNodeType(SFGeometryNode_V9_TypeToTag, SFGeometryNode_V9_Count, NodeTag, GF_BIFS_V9);
	default:
		return 0;
	}
}




u32 NDT_V10_GetNodeTag(u32 Context_NDT_Tag, u32 NodeType)
{
	if (!NodeType) return 0;
	/* adjust according to the table version */
	/* v10: 0 reserved for extensions */
	NodeType -= 1;
	switch (Context_NDT_Tag) {
	case NDT_SFWorldNode:
		if (NodeType >= SFWorldNode_V10_Count) return 0;
		return SFWorldNode_V10_TypeToTag[NodeType];
	case NDT_SF3DNode:
		if (NodeType >= SF3DNode_V10_Count) return 0;
		return SF3DNode_V10_TypeToTag[NodeType];
	case NDT_SF2DNode:
		if (NodeType >= SF2DNode_V10_Count) return 0;
		return SF2DNode_V10_TypeToTag[NodeType];
	case NDT_SFTextureNode:
		if (NodeType >= SFTextureNode_V10_Count) return 0;
		return SFTextureNode_V10_TypeToTag[NodeType];
	default:
		return 0;
	}
}


u32 NDT_V10_GetNumBits(u32 NDT_Tag)
{
	switch (NDT_Tag) {
	case NDT_SFWorldNode:
		return SFWorldNode_V10_NUMBITS;
	case NDT_SF3DNode:
		return SF3DNode_V10_NUMBITS;
	case NDT_SF2DNode:
		return SF2DNode_V10_NUMBITS;
	case NDT_SFTextureNode:
		return SFTextureNode_V10_NUMBITS;
	default:
		return 0;
	}
}

u32 NDT_V10_GetNodeType(u32 NDT_Tag, u32 NodeTag)
{
	if(!NDT_Tag || !NodeTag) return 0;
	switch(NDT_Tag) {
	case NDT_SFWorldNode:
		return ALL_GetNodeType(SFWorldNode_V10_TypeToTag, SFWorldNode_V10_Count, NodeTag, GF_BIFS_V10);
	case NDT_SF3DNode:
		return ALL_GetNodeType(SF3DNode_V10_TypeToTag, SF3DNode_V10_Count, NodeTag, GF_BIFS_V10);
	case NDT_SF2DNode:
		return ALL_GetNodeType(SF2DNode_V10_TypeToTag, SF2DNode_V10_Count, NodeTag, GF_BIFS_V10);
	case NDT_SFTextureNode:
		return ALL_GetNodeType(SFTextureNode_V10_TypeToTag, SFTextureNode_V10_Count, NodeTag, GF_BIFS_V10);
	default:
		return 0;
	}
}



u32 gf_bifs_ndt_get_node_type(u32 NDT_Tag, u32 NodeType, u32 Version)
{
	switch (Version) {
	case GF_BIFS_V1:
		return NDT_V1_GetNodeTag(NDT_Tag, NodeType);
	case GF_BIFS_V2:
		return NDT_V2_GetNodeTag(NDT_Tag, NodeType);
	case GF_BIFS_V3:
		return NDT_V3_GetNodeTag(NDT_Tag, NodeType);
	case GF_BIFS_V4:
		return NDT_V4_GetNodeTag(NDT_Tag, NodeType);
	case GF_BIFS_V5:
		return NDT_V5_GetNodeTag(NDT_Tag, NodeType);
	case GF_BIFS_V6:
		return NDT_V6_GetNodeTag(NDT_Tag, NodeType);
	case GF_BIFS_V7:
		return NDT_V7_GetNodeTag(NDT_Tag, NodeType);
	case GF_BIFS_V8:
		return NDT_V8_GetNodeTag(NDT_Tag, NodeType);
	case GF_BIFS_V9:
		return NDT_V9_GetNodeTag(NDT_Tag, NodeType);
	case GF_BIFS_V10:
		return NDT_V10_GetNodeTag(NDT_Tag, NodeType);
	default:
		return 0;
	}
}

u32 gf_bifs_get_ndt_bits(u32 NDT_Tag, u32 Version)
{
	switch (Version) {
	case GF_BIFS_V1:
		return NDT_V1_GetNumBits(NDT_Tag);
	case GF_BIFS_V2:
		return NDT_V2_GetNumBits(NDT_Tag);
	case GF_BIFS_V3:
		return NDT_V3_GetNumBits(NDT_Tag);
	case GF_BIFS_V4:
		return NDT_V4_GetNumBits(NDT_Tag);
	case GF_BIFS_V5:
		return NDT_V5_GetNumBits(NDT_Tag);
	case GF_BIFS_V6:
		return NDT_V6_GetNumBits(NDT_Tag);
	case GF_BIFS_V7:
		return NDT_V7_GetNumBits(NDT_Tag);
	case GF_BIFS_V8:
		return NDT_V8_GetNumBits(NDT_Tag);
	case GF_BIFS_V9:
		return NDT_V9_GetNumBits(NDT_Tag);
	case GF_BIFS_V10:
		return NDT_V10_GetNumBits(NDT_Tag);
	default:
		return 0;
	}
}

u32 gf_bifs_get_node_type(u32 NDT_Tag, u32 NodeTag, u32 Version)
{
	switch (Version) {
	case GF_BIFS_V1:
		return NDT_V1_GetNodeType(NDT_Tag, NodeTag);
	case GF_BIFS_V2:
		return NDT_V2_GetNodeType(NDT_Tag, NodeTag);
	case GF_BIFS_V3:
		return NDT_V3_GetNodeType(NDT_Tag, NodeTag);
	case GF_BIFS_V4:
		return NDT_V4_GetNodeType(NDT_Tag, NodeTag);
	case GF_BIFS_V5:
		return NDT_V5_GetNodeType(NDT_Tag, NodeTag);
	case GF_BIFS_V6:
		return NDT_V6_GetNodeType(NDT_Tag, NodeTag);
	case GF_BIFS_V7:
		return NDT_V7_GetNodeType(NDT_Tag, NodeTag);
	case GF_BIFS_V8:
		return NDT_V8_GetNodeType(NDT_Tag, NodeTag);
	case GF_BIFS_V9:
		return NDT_V9_GetNodeType(NDT_Tag, NodeTag);
	case GF_BIFS_V10:
		return NDT_V10_GetNodeType(NDT_Tag, NodeTag);
	default:
		return 0;
	}
}
u32 GetChildrenNDT(GF_Node *node)
{
	if (!node) return 0;
	switch (gf_node_get_tag(node)) {
	case TAG_MPEG4_Anchor:
		return NDT_SF3DNode;
	case TAG_MPEG4_AudioBuffer:
		return NDT_SFAudioNode;
	case TAG_MPEG4_AudioDelay:
		return NDT_SFAudioNode;
	case TAG_MPEG4_AudioFX:
		return NDT_SFAudioNode;
	case TAG_MPEG4_AudioMix:
		return NDT_SFAudioNode;
	case TAG_MPEG4_AudioSource:
		return NDT_SFAudioNode;
	case TAG_MPEG4_AudioSwitch:
		return NDT_SFAudioNode;
	case TAG_MPEG4_Billboard:
		return NDT_SF3DNode;
	case TAG_MPEG4_Collision:
		return NDT_SF3DNode;
	case TAG_MPEG4_CompositeTexture2D:
		return NDT_SF2DNode;
	case TAG_MPEG4_CompositeTexture3D:
		return NDT_SF3DNode;
	case TAG_MPEG4_Form:
		return NDT_SF2DNode;
	case TAG_MPEG4_Group:
		return NDT_SF3DNode;
	case TAG_MPEG4_Layer2D:
		return NDT_SF2DNode;
	case TAG_MPEG4_Layer3D:
		return NDT_SF3DNode;
	case TAG_MPEG4_Layout:
		return NDT_SF2DNode;
	case TAG_MPEG4_OrderedGroup:
		return NDT_SF3DNode;
	case TAG_MPEG4_Transform:
		return NDT_SF3DNode;
	case TAG_MPEG4_Transform2D:
		return NDT_SF2DNode;
	case TAG_MPEG4_TemporalTransform:
		return NDT_SF3DNode;
	case TAG_MPEG4_TemporalGroup:
		return NDT_SFTemporalNode;
	case TAG_MPEG4_FFD:
		return NDT_SF3DNode;
	case TAG_MPEG4_SBBone:
		return NDT_SF3DNode;
	case TAG_MPEG4_SBSegment:
		return NDT_SF3DNode;
	case TAG_MPEG4_SBSite:
		return NDT_SF3DNode;
	case TAG_MPEG4_Clipper2D:
		return NDT_SF2DNode;
	case TAG_MPEG4_ColorTransform:
		return NDT_SF3DNode;
	case TAG_MPEG4_PathLayout:
		return NDT_SF2DNode;
	case TAG_MPEG4_TransformMatrix2D:
		return NDT_SF2DNode;
	case TAG_MPEG4_AdvancedAudioBuffer:
		return NDT_SFAudioNode;
	case TAG_MPEG4_AudioChannelConfig:
		return NDT_SFAudioNode;
	case TAG_MPEG4_Transform3DAudio:
		return NDT_SF3DNode;
	case TAG_MPEG4_FootPrintSetNode:
		return NDT_SFGeometryNode;
	case TAG_MPEG4_Shadow:
		return NDT_SF3DNode;
	case TAG_MPEG4_SpacePartition:
		return NDT_SF3DNode;
	default:
		return 0;
	}
}



#endif /*GPAC_DISABLE_BIFS*/


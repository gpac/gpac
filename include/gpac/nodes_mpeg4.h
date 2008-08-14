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
	DO NOT MOFIFY - File generated on GMT Thu Aug 07 11:43:26 2008

	BY MPEG4Gen for GPAC Version 0.4.5-DEV
*/

#ifndef _nodes_mpeg4_H
#define _nodes_mpeg4_H

#ifdef __cplusplus
extern "C" {
#endif

#include <gpac/scenegraph_vrml.h>



enum {
	TAG_MPEG4_Anchor = GF_NODE_RANGE_FIRST_MPEG4,
	TAG_MPEG4_AnimationStream,
	TAG_MPEG4_Appearance,
	TAG_MPEG4_AudioBuffer,
	TAG_MPEG4_AudioClip,
	TAG_MPEG4_AudioDelay,
	TAG_MPEG4_AudioFX,
	TAG_MPEG4_AudioMix,
	TAG_MPEG4_AudioSource,
	TAG_MPEG4_AudioSwitch,
	TAG_MPEG4_Background,
	TAG_MPEG4_Background2D,
	TAG_MPEG4_Billboard,
	TAG_MPEG4_Bitmap,
	TAG_MPEG4_Box,
	TAG_MPEG4_Circle,
	TAG_MPEG4_Collision,
	TAG_MPEG4_Color,
	TAG_MPEG4_ColorInterpolator,
	TAG_MPEG4_CompositeTexture2D,
	TAG_MPEG4_CompositeTexture3D,
	TAG_MPEG4_Conditional,
	TAG_MPEG4_Cone,
	TAG_MPEG4_Coordinate,
	TAG_MPEG4_Coordinate2D,
	TAG_MPEG4_CoordinateInterpolator,
	TAG_MPEG4_CoordinateInterpolator2D,
	TAG_MPEG4_Curve2D,
	TAG_MPEG4_Cylinder,
	TAG_MPEG4_CylinderSensor,
	TAG_MPEG4_DirectionalLight,
	TAG_MPEG4_DiscSensor,
	TAG_MPEG4_ElevationGrid,
	TAG_MPEG4_Expression,
	TAG_MPEG4_Extrusion,
	TAG_MPEG4_Face,
	TAG_MPEG4_FaceDefMesh,
	TAG_MPEG4_FaceDefTables,
	TAG_MPEG4_FaceDefTransform,
	TAG_MPEG4_FAP,
	TAG_MPEG4_FDP,
	TAG_MPEG4_FIT,
	TAG_MPEG4_Fog,
	TAG_MPEG4_FontStyle,
	TAG_MPEG4_Form,
	TAG_MPEG4_Group,
	TAG_MPEG4_ImageTexture,
	TAG_MPEG4_IndexedFaceSet,
	TAG_MPEG4_IndexedFaceSet2D,
	TAG_MPEG4_IndexedLineSet,
	TAG_MPEG4_IndexedLineSet2D,
	TAG_MPEG4_Inline,
	TAG_MPEG4_LOD,
	TAG_MPEG4_Layer2D,
	TAG_MPEG4_Layer3D,
	TAG_MPEG4_Layout,
	TAG_MPEG4_LineProperties,
	TAG_MPEG4_ListeningPoint,
	TAG_MPEG4_Material,
	TAG_MPEG4_Material2D,
	TAG_MPEG4_MovieTexture,
	TAG_MPEG4_NavigationInfo,
	TAG_MPEG4_Normal,
	TAG_MPEG4_NormalInterpolator,
	TAG_MPEG4_OrderedGroup,
	TAG_MPEG4_OrientationInterpolator,
	TAG_MPEG4_PixelTexture,
	TAG_MPEG4_PlaneSensor,
	TAG_MPEG4_PlaneSensor2D,
	TAG_MPEG4_PointLight,
	TAG_MPEG4_PointSet,
	TAG_MPEG4_PointSet2D,
	TAG_MPEG4_PositionInterpolator,
	TAG_MPEG4_PositionInterpolator2D,
	TAG_MPEG4_ProximitySensor2D,
	TAG_MPEG4_ProximitySensor,
	TAG_MPEG4_QuantizationParameter,
	TAG_MPEG4_Rectangle,
	TAG_MPEG4_ScalarInterpolator,
	TAG_MPEG4_Script,
	TAG_MPEG4_Shape,
	TAG_MPEG4_Sound,
	TAG_MPEG4_Sound2D,
	TAG_MPEG4_Sphere,
	TAG_MPEG4_SphereSensor,
	TAG_MPEG4_SpotLight,
	TAG_MPEG4_Switch,
	TAG_MPEG4_TermCap,
	TAG_MPEG4_Text,
	TAG_MPEG4_TextureCoordinate,
	TAG_MPEG4_TextureTransform,
	TAG_MPEG4_TimeSensor,
	TAG_MPEG4_TouchSensor,
	TAG_MPEG4_Transform,
	TAG_MPEG4_Transform2D,
	TAG_MPEG4_Valuator,
	TAG_MPEG4_Viewpoint,
	TAG_MPEG4_VisibilitySensor,
	TAG_MPEG4_Viseme,
	TAG_MPEG4_WorldInfo,
	TAG_MPEG4_AcousticMaterial,
	TAG_MPEG4_AcousticScene,
	TAG_MPEG4_ApplicationWindow,
	TAG_MPEG4_BAP,
	TAG_MPEG4_BDP,
	TAG_MPEG4_Body,
	TAG_MPEG4_BodyDefTable,
	TAG_MPEG4_BodySegmentConnectionHint,
	TAG_MPEG4_DirectiveSound,
	TAG_MPEG4_Hierarchical3DMesh,
	TAG_MPEG4_MaterialKey,
	TAG_MPEG4_PerceptualParameters,
	TAG_MPEG4_TemporalTransform,
	TAG_MPEG4_TemporalGroup,
	TAG_MPEG4_ServerCommand,
	TAG_MPEG4_InputSensor,
	TAG_MPEG4_MatteTexture,
	TAG_MPEG4_MediaBuffer,
	TAG_MPEG4_MediaControl,
	TAG_MPEG4_MediaSensor,
	TAG_MPEG4_BitWrapper,
	TAG_MPEG4_CoordinateInterpolator4D,
	TAG_MPEG4_DepthImage,
	TAG_MPEG4_FFD,
	TAG_MPEG4_Implicit,
	TAG_MPEG4_XXLFM_Appearance,
	TAG_MPEG4_XXLFM_BlendList,
	TAG_MPEG4_XXLFM_FrameList,
	TAG_MPEG4_XXLFM_LightMap,
	TAG_MPEG4_XXLFM_SurfaceMapList,
	TAG_MPEG4_XXLFM_ViewMapList,
	TAG_MPEG4_MeshGrid,
	TAG_MPEG4_NonLinearDeformer,
	TAG_MPEG4_NurbsCurve,
	TAG_MPEG4_NurbsCurve2D,
	TAG_MPEG4_NurbsSurface,
	TAG_MPEG4_OctreeImage,
	TAG_MPEG4_XXParticles,
	TAG_MPEG4_XXParticleInitBox,
	TAG_MPEG4_XXPlanarObstacle,
	TAG_MPEG4_XXPointAttractor,
	TAG_MPEG4_PointTexture,
	TAG_MPEG4_PositionAnimator,
	TAG_MPEG4_PositionAnimator2D,
	TAG_MPEG4_PositionInterpolator4D,
	TAG_MPEG4_ProceduralTexture,
	TAG_MPEG4_Quadric,
	TAG_MPEG4_SBBone,
	TAG_MPEG4_SBMuscle,
	TAG_MPEG4_SBSegment,
	TAG_MPEG4_SBSite,
	TAG_MPEG4_SBSkinnedModel,
	TAG_MPEG4_SBVCAnimation,
	TAG_MPEG4_ScalarAnimator,
	TAG_MPEG4_SimpleTexture,
	TAG_MPEG4_SolidRep,
	TAG_MPEG4_SubdivisionSurface,
	TAG_MPEG4_SubdivSurfaceSector,
	TAG_MPEG4_WaveletSubdivisionSurface,
	TAG_MPEG4_Clipper2D,
	TAG_MPEG4_ColorTransform,
	TAG_MPEG4_Ellipse,
	TAG_MPEG4_LinearGradient,
	TAG_MPEG4_PathLayout,
	TAG_MPEG4_RadialGradient,
	TAG_MPEG4_SynthesizedTexture,
	TAG_MPEG4_TransformMatrix2D,
	TAG_MPEG4_Viewport,
	TAG_MPEG4_XCurve2D,
	TAG_MPEG4_XFontStyle,
	TAG_MPEG4_XLineProperties,
	TAG_LastImplementedMPEG4
};

typedef struct _tagAnchor
{
	BASE_NODE
	VRML_CHILDREN
	SFString description;	/*exposedField*/
	MFString parameter;	/*exposedField*/
	MFURL url;	/*exposedField*/
	SFBool activate;	/*eventIn*/
	void (*on_activate)(GF_Node *pThis);	/*eventInHandler*/
} M_Anchor;


typedef struct _tagAnimationStream
{
	BASE_NODE
	SFBool loop;	/*exposedField*/
	SFFloat speed;	/*exposedField*/
	SFTime startTime;	/*exposedField*/
	SFTime stopTime;	/*exposedField*/
	MFURL url;	/*exposedField*/
	SFTime duration_changed;	/*eventOut*/
	SFBool isActive;	/*eventOut*/
} M_AnimationStream;


typedef struct _tagAppearance
{
	BASE_NODE
	GF_Node *material;	/*exposedField*/
	GF_Node *texture;	/*exposedField*/
	GF_Node *textureTransform;	/*exposedField*/
} M_Appearance;


typedef struct _tagAudioBuffer
{
	BASE_NODE
	SFBool loop;	/*exposedField*/
	SFFloat pitch;	/*exposedField*/
	SFTime startTime;	/*exposedField*/
	SFTime stopTime;	/*exposedField*/
	GF_ChildNodeItem *children;	/*exposedField*/
	SFInt32 numChan;	/*exposedField*/
	MFInt32 phaseGroup;	/*exposedField*/
	SFFloat length;	/*exposedField*/
	SFTime duration_changed;	/*eventOut*/
	SFBool isActive;	/*eventOut*/
} M_AudioBuffer;


typedef struct _tagAudioClip
{
	BASE_NODE
	SFString description;	/*exposedField*/
	SFBool loop;	/*exposedField*/
	SFFloat pitch;	/*exposedField*/
	SFTime startTime;	/*exposedField*/
	SFTime stopTime;	/*exposedField*/
	MFURL url;	/*exposedField*/
	SFTime duration_changed;	/*eventOut*/
	SFBool isActive;	/*eventOut*/
} M_AudioClip;


typedef struct _tagAudioDelay
{
	BASE_NODE
	VRML_CHILDREN
	SFTime delay;	/*exposedField*/
	SFInt32 numChan;	/*field*/
	MFInt32 phaseGroup;	/*field*/
} M_AudioDelay;


typedef struct _tagAudioFX
{
	BASE_NODE
	VRML_CHILDREN
	SFString orch;	/*exposedField*/
	SFString score;	/*exposedField*/
	MFFloat params;	/*exposedField*/
	SFInt32 numChan;	/*field*/
	MFInt32 phaseGroup;	/*field*/
} M_AudioFX;


typedef struct _tagAudioMix
{
	BASE_NODE
	VRML_CHILDREN
	SFInt32 numInputs;	/*exposedField*/
	MFFloat matrix;	/*exposedField*/
	SFInt32 numChan;	/*field*/
	MFInt32 phaseGroup;	/*field*/
} M_AudioMix;


typedef struct _tagAudioSource
{
	BASE_NODE
	VRML_CHILDREN
	MFURL url;	/*exposedField*/
	SFFloat pitch;	/*exposedField*/
	SFFloat speed;	/*exposedField*/
	SFTime startTime;	/*exposedField*/
	SFTime stopTime;	/*exposedField*/
	SFInt32 numChan;	/*field*/
	MFInt32 phaseGroup;	/*field*/
} M_AudioSource;


typedef struct _tagAudioSwitch
{
	BASE_NODE
	VRML_CHILDREN
	MFInt32 whichChoice;	/*exposedField*/
	SFInt32 numChan;	/*field*/
	MFInt32 phaseGroup;	/*field*/
} M_AudioSwitch;


typedef struct _tagBackground
{
	BASE_NODE
	SFBool set_bind;	/*eventIn*/
	void (*on_set_bind)(GF_Node *pThis);	/*eventInHandler*/
	MFFloat groundAngle;	/*exposedField*/
	MFColor groundColor;	/*exposedField*/
	MFURL backUrl;	/*exposedField*/
	MFURL bottomUrl;	/*exposedField*/
	MFURL frontUrl;	/*exposedField*/
	MFURL leftUrl;	/*exposedField*/
	MFURL rightUrl;	/*exposedField*/
	MFURL topUrl;	/*exposedField*/
	MFFloat skyAngle;	/*exposedField*/
	MFColor skyColor;	/*exposedField*/
	SFBool isBound;	/*eventOut*/
} M_Background;


typedef struct _tagBackground2D
{
	BASE_NODE
	SFBool set_bind;	/*eventIn*/
	void (*on_set_bind)(GF_Node *pThis);	/*eventInHandler*/
	SFColor backColor;	/*exposedField*/
	MFURL url;	/*exposedField*/
	SFBool isBound;	/*eventOut*/
} M_Background2D;


typedef struct _tagBillboard
{
	BASE_NODE
	VRML_CHILDREN
	SFVec3f axisOfRotation;	/*exposedField*/
} M_Billboard;


typedef struct _tagBitmap
{
	BASE_NODE
	SFVec2f scale;	/*exposedField*/
} M_Bitmap;


typedef struct _tagBox
{
	BASE_NODE
	SFVec3f size;	/*field*/
} M_Box;


typedef struct _tagCircle
{
	BASE_NODE
	SFFloat radius;	/*exposedField*/
} M_Circle;


typedef struct _tagCollision
{
	BASE_NODE
	VRML_CHILDREN
	SFBool collide;	/*exposedField*/
	GF_Node *proxy;	/*field*/
	SFTime collideTime;	/*eventOut*/
} M_Collision;


typedef struct _tagColor
{
	BASE_NODE
	MFColor color;	/*exposedField*/
} M_Color;


typedef struct _tagColorInterpolator
{
	BASE_NODE
	SFFloat set_fraction;	/*eventIn*/
	void (*on_set_fraction)(GF_Node *pThis);	/*eventInHandler*/
	MFFloat key;	/*exposedField*/
	MFColor keyValue;	/*exposedField*/
	SFColor value_changed;	/*eventOut*/
} M_ColorInterpolator;


typedef struct _tagCompositeTexture2D
{
	BASE_NODE
	VRML_CHILDREN
	SFInt32 pixelWidth;	/*exposedField*/
	SFInt32 pixelHeight;	/*exposedField*/
	GF_Node *background;	/*exposedField*/
	GF_Node *viewport;	/*exposedField*/
	SFInt32 repeatSandT;	/*field*/
} M_CompositeTexture2D;


typedef struct _tagCompositeTexture3D
{
	BASE_NODE
	VRML_CHILDREN
	SFInt32 pixelWidth;	/*exposedField*/
	SFInt32 pixelHeight;	/*exposedField*/
	GF_Node *background;	/*exposedField*/
	GF_Node *fog;	/*exposedField*/
	GF_Node *navigationInfo;	/*exposedField*/
	GF_Node *viewpoint;	/*exposedField*/
	SFBool repeatS;	/*field*/
	SFBool repeatT;	/*field*/
} M_CompositeTexture3D;


typedef struct _tagConditional
{
	BASE_NODE
	SFBool activate;	/*eventIn*/
	void (*on_activate)(GF_Node *pThis);	/*eventInHandler*/
	SFBool reverseActivate;	/*eventIn*/
	void (*on_reverseActivate)(GF_Node *pThis);	/*eventInHandler*/
	SFCommandBuffer buffer;	/*exposedField*/
	SFBool isActive;	/*eventOut*/
} M_Conditional;


typedef struct _tagCone
{
	BASE_NODE
	SFFloat bottomRadius;	/*field*/
	SFFloat height;	/*field*/
	SFBool side;	/*field*/
	SFBool bottom;	/*field*/
} M_Cone;


typedef struct _tagCoordinate
{
	BASE_NODE
	MFVec3f point;	/*exposedField*/
} M_Coordinate;


typedef struct _tagCoordinate2D
{
	BASE_NODE
	MFVec2f point;	/*exposedField*/
} M_Coordinate2D;


typedef struct _tagCoordinateInterpolator
{
	BASE_NODE
	SFFloat set_fraction;	/*eventIn*/
	void (*on_set_fraction)(GF_Node *pThis);	/*eventInHandler*/
	MFFloat key;	/*exposedField*/
	MFVec3f keyValue;	/*exposedField*/
	MFVec3f value_changed;	/*eventOut*/
} M_CoordinateInterpolator;


typedef struct _tagCoordinateInterpolator2D
{
	BASE_NODE
	SFFloat set_fraction;	/*eventIn*/
	void (*on_set_fraction)(GF_Node *pThis);	/*eventInHandler*/
	MFFloat key;	/*exposedField*/
	MFVec2f keyValue;	/*exposedField*/
	MFVec2f value_changed;	/*eventOut*/
} M_CoordinateInterpolator2D;


typedef struct _tagCurve2D
{
	BASE_NODE
	GF_Node *point;	/*exposedField*/
	SFFloat fineness;	/*exposedField*/
	MFInt32 type;	/*exposedField*/
} M_Curve2D;


typedef struct _tagCylinder
{
	BASE_NODE
	SFBool bottom;	/*field*/
	SFFloat height;	/*field*/
	SFFloat radius;	/*field*/
	SFBool side;	/*field*/
	SFBool top;	/*field*/
} M_Cylinder;


typedef struct _tagCylinderSensor
{
	BASE_NODE
	SFBool autoOffset;	/*exposedField*/
	SFFloat diskAngle;	/*exposedField*/
	SFBool enabled;	/*exposedField*/
	SFFloat maxAngle;	/*exposedField*/
	SFFloat minAngle;	/*exposedField*/
	SFFloat offset;	/*exposedField*/
	SFBool isActive;	/*eventOut*/
	SFRotation rotation_changed;	/*eventOut*/
	SFVec3f trackPoint_changed;	/*eventOut*/
} M_CylinderSensor;


typedef struct _tagDirectionalLight
{
	BASE_NODE
	SFFloat ambientIntensity;	/*exposedField*/
	SFColor color;	/*exposedField*/
	SFVec3f direction;	/*exposedField*/
	SFFloat intensity;	/*exposedField*/
	SFBool on;	/*exposedField*/
} M_DirectionalLight;


typedef struct _tagDiscSensor
{
	BASE_NODE
	SFBool autoOffset;	/*exposedField*/
	SFBool enabled;	/*exposedField*/
	SFFloat maxAngle;	/*exposedField*/
	SFFloat minAngle;	/*exposedField*/
	SFFloat offset;	/*exposedField*/
	SFBool isActive;	/*eventOut*/
	SFFloat rotation_changed;	/*eventOut*/
	SFVec2f trackPoint_changed;	/*eventOut*/
} M_DiscSensor;


typedef struct _tagElevationGrid
{
	BASE_NODE
	MFFloat set_height;	/*eventIn*/
	void (*on_set_height)(GF_Node *pThis);	/*eventInHandler*/
	GF_Node *color;	/*exposedField*/
	GF_Node *normal;	/*exposedField*/
	GF_Node *texCoord;	/*exposedField*/
	MFFloat height;	/*field*/
	SFBool ccw;	/*field*/
	SFBool colorPerVertex;	/*field*/
	SFFloat creaseAngle;	/*field*/
	SFBool normalPerVertex;	/*field*/
	SFBool solid;	/*field*/
	SFInt32 xDimension;	/*field*/
	SFFloat xSpacing;	/*field*/
	SFInt32 zDimension;	/*field*/
	SFFloat zSpacing;	/*field*/
} M_ElevationGrid;


typedef struct _tagExtrusion
{
	BASE_NODE
	MFVec2f set_crossSection;	/*eventIn*/
	void (*on_set_crossSection)(GF_Node *pThis);	/*eventInHandler*/
	MFRotation set_orientation;	/*eventIn*/
	void (*on_set_orientation)(GF_Node *pThis);	/*eventInHandler*/
	MFVec2f set_scale;	/*eventIn*/
	void (*on_set_scale)(GF_Node *pThis);	/*eventInHandler*/
	MFVec3f set_spine;	/*eventIn*/
	void (*on_set_spine)(GF_Node *pThis);	/*eventInHandler*/
	SFBool beginCap;	/*field*/
	SFBool ccw;	/*field*/
	SFBool convex;	/*field*/
	SFFloat creaseAngle;	/*field*/
	MFVec2f crossSection;	/*field*/
	SFBool endCap;	/*field*/
	MFRotation orientation;	/*field*/
	MFVec2f scale;	/*field*/
	SFBool solid;	/*field*/
	MFVec3f spine;	/*field*/
} M_Extrusion;


typedef struct _tagFog
{
	BASE_NODE
	SFColor color;	/*exposedField*/
	SFString fogType;	/*exposedField*/
	SFFloat visibilityRange;	/*exposedField*/
	SFBool set_bind;	/*eventIn*/
	void (*on_set_bind)(GF_Node *pThis);	/*eventInHandler*/
	SFBool isBound;	/*eventOut*/
} M_Fog;


typedef struct _tagFontStyle
{
	BASE_NODE
	MFString family;	/*exposedField*/
	SFBool horizontal;	/*exposedField*/
	MFString justify;	/*exposedField*/
	SFString language;	/*exposedField*/
	SFBool leftToRight;	/*exposedField*/
	SFFloat size;	/*exposedField*/
	SFFloat spacing;	/*exposedField*/
	SFString style;	/*exposedField*/
	SFBool topToBottom;	/*exposedField*/
} M_FontStyle;


typedef struct _tagForm
{
	BASE_NODE
	VRML_CHILDREN
	SFVec2f size;	/*exposedField*/
	MFInt32 groups;	/*exposedField*/
	MFString constraints;	/*exposedField*/
	MFInt32 groupsIndex;	/*exposedField*/
} M_Form;


typedef struct _tagGroup
{
	BASE_NODE
	VRML_CHILDREN
} M_Group;


typedef struct _tagImageTexture
{
	BASE_NODE
	MFURL url;	/*exposedField*/
	SFBool repeatS;	/*field*/
	SFBool repeatT;	/*field*/
} M_ImageTexture;


typedef struct _tagIndexedFaceSet
{
	BASE_NODE
	MFInt32 set_colorIndex;	/*eventIn*/
	void (*on_set_colorIndex)(GF_Node *pThis);	/*eventInHandler*/
	MFInt32 set_coordIndex;	/*eventIn*/
	void (*on_set_coordIndex)(GF_Node *pThis);	/*eventInHandler*/
	MFInt32 set_normalIndex;	/*eventIn*/
	void (*on_set_normalIndex)(GF_Node *pThis);	/*eventInHandler*/
	MFInt32 set_texCoordIndex;	/*eventIn*/
	void (*on_set_texCoordIndex)(GF_Node *pThis);	/*eventInHandler*/
	GF_Node *color;	/*exposedField*/
	GF_Node *coord;	/*exposedField*/
	GF_Node *normal;	/*exposedField*/
	GF_Node *texCoord;	/*exposedField*/
	SFBool ccw;	/*field*/
	MFInt32 colorIndex;	/*field*/
	SFBool colorPerVertex;	/*field*/
	SFBool convex;	/*field*/
	MFInt32 coordIndex;	/*field*/
	SFFloat creaseAngle;	/*field*/
	MFInt32 normalIndex;	/*field*/
	SFBool normalPerVertex;	/*field*/
	SFBool solid;	/*field*/
	MFInt32 texCoordIndex;	/*field*/
} M_IndexedFaceSet;


typedef struct _tagIndexedFaceSet2D
{
	BASE_NODE
	MFInt32 set_colorIndex;	/*eventIn*/
	void (*on_set_colorIndex)(GF_Node *pThis);	/*eventInHandler*/
	MFInt32 set_coordIndex;	/*eventIn*/
	void (*on_set_coordIndex)(GF_Node *pThis);	/*eventInHandler*/
	MFInt32 set_texCoordIndex;	/*eventIn*/
	void (*on_set_texCoordIndex)(GF_Node *pThis);	/*eventInHandler*/
	GF_Node *color;	/*exposedField*/
	GF_Node *coord;	/*exposedField*/
	GF_Node *texCoord;	/*exposedField*/
	MFInt32 colorIndex;	/*field*/
	SFBool colorPerVertex;	/*field*/
	SFBool convex;	/*field*/
	MFInt32 coordIndex;	/*field*/
	MFInt32 texCoordIndex;	/*field*/
} M_IndexedFaceSet2D;


typedef struct _tagIndexedLineSet
{
	BASE_NODE
	MFInt32 set_colorIndex;	/*eventIn*/
	void (*on_set_colorIndex)(GF_Node *pThis);	/*eventInHandler*/
	MFInt32 set_coordIndex;	/*eventIn*/
	void (*on_set_coordIndex)(GF_Node *pThis);	/*eventInHandler*/
	GF_Node *color;	/*exposedField*/
	GF_Node *coord;	/*exposedField*/
	MFInt32 colorIndex;	/*field*/
	SFBool colorPerVertex;	/*field*/
	MFInt32 coordIndex;	/*field*/
} M_IndexedLineSet;


typedef struct _tagIndexedLineSet2D
{
	BASE_NODE
	MFInt32 set_colorIndex;	/*eventIn*/
	void (*on_set_colorIndex)(GF_Node *pThis);	/*eventInHandler*/
	MFInt32 set_coordIndex;	/*eventIn*/
	void (*on_set_coordIndex)(GF_Node *pThis);	/*eventInHandler*/
	GF_Node *color;	/*exposedField*/
	GF_Node *coord;	/*exposedField*/
	MFInt32 colorIndex;	/*field*/
	SFBool colorPerVertex;	/*field*/
	MFInt32 coordIndex;	/*field*/
} M_IndexedLineSet2D;


typedef struct _tagInline
{
	BASE_NODE
	MFURL url;	/*exposedField*/
} M_Inline;


typedef struct _tagLOD
{
	BASE_NODE
	GF_ChildNodeItem *level;	/*exposedField*/
	SFVec3f center;	/*field*/
	MFFloat range;	/*field*/
} M_LOD;


typedef struct _tagLayer2D
{
	BASE_NODE
	VRML_CHILDREN
	SFVec2f size;	/*exposedField*/
	GF_Node *background;	/*exposedField*/
	GF_Node *viewport;	/*exposedField*/
} M_Layer2D;


typedef struct _tagLayer3D
{
	BASE_NODE
	VRML_CHILDREN
	SFVec2f size;	/*exposedField*/
	GF_Node *background;	/*exposedField*/
	GF_Node *fog;	/*exposedField*/
	GF_Node *navigationInfo;	/*exposedField*/
	GF_Node *viewpoint;	/*exposedField*/
} M_Layer3D;


typedef struct _tagLayout
{
	BASE_NODE
	VRML_CHILDREN
	SFBool wrap;	/*exposedField*/
	SFVec2f size;	/*exposedField*/
	SFBool horizontal;	/*exposedField*/
	MFString justify;	/*exposedField*/
	SFBool leftToRight;	/*exposedField*/
	SFBool topToBottom;	/*exposedField*/
	SFFloat spacing;	/*exposedField*/
	SFBool smoothScroll;	/*exposedField*/
	SFBool loop;	/*exposedField*/
	SFBool scrollVertical;	/*exposedField*/
	SFFloat scrollRate;	/*exposedField*/
	SFInt32 scrollMode;	/*exposedField*/
} M_Layout;


typedef struct _tagLineProperties
{
	BASE_NODE
	SFColor lineColor;	/*exposedField*/
	SFInt32 lineStyle;	/*exposedField*/
	SFFloat width;	/*exposedField*/
} M_LineProperties;


typedef struct _tagListeningPoint
{
	BASE_NODE
	SFBool set_bind;	/*eventIn*/
	void (*on_set_bind)(GF_Node *pThis);	/*eventInHandler*/
	SFBool jump;	/*exposedField*/
	SFRotation orientation;	/*exposedField*/
	SFVec3f position;	/*exposedField*/
	SFString description;	/*field*/
	SFTime bindTime;	/*eventOut*/
	SFBool isBound;	/*eventOut*/
} M_ListeningPoint;


typedef struct _tagMaterial
{
	BASE_NODE
	SFFloat ambientIntensity;	/*exposedField*/
	SFColor diffuseColor;	/*exposedField*/
	SFColor emissiveColor;	/*exposedField*/
	SFFloat shininess;	/*exposedField*/
	SFColor specularColor;	/*exposedField*/
	SFFloat transparency;	/*exposedField*/
} M_Material;


typedef struct _tagMaterial2D
{
	BASE_NODE
	SFColor emissiveColor;	/*exposedField*/
	SFBool filled;	/*exposedField*/
	GF_Node *lineProps;	/*exposedField*/
	SFFloat transparency;	/*exposedField*/
} M_Material2D;


typedef struct _tagMovieTexture
{
	BASE_NODE
	SFBool loop;	/*exposedField*/
	SFFloat speed;	/*exposedField*/
	SFTime startTime;	/*exposedField*/
	SFTime stopTime;	/*exposedField*/
	MFURL url;	/*exposedField*/
	SFBool repeatS;	/*field*/
	SFBool repeatT;	/*field*/
	SFTime duration_changed;	/*eventOut*/
	SFBool isActive;	/*eventOut*/
} M_MovieTexture;


typedef struct _tagNavigationInfo
{
	BASE_NODE
	SFBool set_bind;	/*eventIn*/
	void (*on_set_bind)(GF_Node *pThis);	/*eventInHandler*/
	MFFloat avatarSize;	/*exposedField*/
	SFBool headlight;	/*exposedField*/
	SFFloat speed;	/*exposedField*/
	MFString type;	/*exposedField*/
	SFFloat visibilityLimit;	/*exposedField*/
	SFBool isBound;	/*eventOut*/
} M_NavigationInfo;


typedef struct _tagNormal
{
	BASE_NODE
	MFVec3f vector;	/*exposedField*/
} M_Normal;


typedef struct _tagNormalInterpolator
{
	BASE_NODE
	SFFloat set_fraction;	/*eventIn*/
	void (*on_set_fraction)(GF_Node *pThis);	/*eventInHandler*/
	MFFloat key;	/*exposedField*/
	MFVec3f keyValue;	/*exposedField*/
	MFVec3f value_changed;	/*eventOut*/
} M_NormalInterpolator;


typedef struct _tagOrderedGroup
{
	BASE_NODE
	VRML_CHILDREN
	MFFloat order;	/*exposedField*/
} M_OrderedGroup;


typedef struct _tagOrientationInterpolator
{
	BASE_NODE
	SFFloat set_fraction;	/*eventIn*/
	void (*on_set_fraction)(GF_Node *pThis);	/*eventInHandler*/
	MFFloat key;	/*exposedField*/
	MFRotation keyValue;	/*exposedField*/
	SFRotation value_changed;	/*eventOut*/
} M_OrientationInterpolator;


typedef struct _tagPixelTexture
{
	BASE_NODE
	SFImage image;	/*exposedField*/
	SFBool repeatS;	/*field*/
	SFBool repeatT;	/*field*/
} M_PixelTexture;


typedef struct _tagPlaneSensor
{
	BASE_NODE
	SFBool autoOffset;	/*exposedField*/
	SFBool enabled;	/*exposedField*/
	SFVec2f maxPosition;	/*exposedField*/
	SFVec2f minPosition;	/*exposedField*/
	SFVec3f offset;	/*exposedField*/
	SFBool isActive;	/*eventOut*/
	SFVec3f trackPoint_changed;	/*eventOut*/
	SFVec3f translation_changed;	/*eventOut*/
} M_PlaneSensor;


typedef struct _tagPlaneSensor2D
{
	BASE_NODE
	SFBool autoOffset;	/*exposedField*/
	SFBool enabled;	/*exposedField*/
	SFVec2f maxPosition;	/*exposedField*/
	SFVec2f minPosition;	/*exposedField*/
	SFVec2f offset;	/*exposedField*/
	SFBool isActive;	/*eventOut*/
	SFVec2f trackPoint_changed;	/*eventOut*/
	SFVec2f translation_changed;	/*eventOut*/
} M_PlaneSensor2D;


typedef struct _tagPointLight
{
	BASE_NODE
	SFFloat ambientIntensity;	/*exposedField*/
	SFVec3f attenuation;	/*exposedField*/
	SFColor color;	/*exposedField*/
	SFFloat intensity;	/*exposedField*/
	SFVec3f location;	/*exposedField*/
	SFBool on;	/*exposedField*/
	SFFloat radius;	/*exposedField*/
} M_PointLight;


typedef struct _tagPointSet
{
	BASE_NODE
	GF_Node *color;	/*exposedField*/
	GF_Node *coord;	/*exposedField*/
} M_PointSet;


typedef struct _tagPointSet2D
{
	BASE_NODE
	GF_Node *color;	/*exposedField*/
	GF_Node *coord;	/*exposedField*/
} M_PointSet2D;


typedef struct _tagPositionInterpolator
{
	BASE_NODE
	SFFloat set_fraction;	/*eventIn*/
	void (*on_set_fraction)(GF_Node *pThis);	/*eventInHandler*/
	MFFloat key;	/*exposedField*/
	MFVec3f keyValue;	/*exposedField*/
	SFVec3f value_changed;	/*eventOut*/
} M_PositionInterpolator;


typedef struct _tagPositionInterpolator2D
{
	BASE_NODE
	SFFloat set_fraction;	/*eventIn*/
	void (*on_set_fraction)(GF_Node *pThis);	/*eventInHandler*/
	MFFloat key;	/*exposedField*/
	MFVec2f keyValue;	/*exposedField*/
	SFVec2f value_changed;	/*eventOut*/
} M_PositionInterpolator2D;


typedef struct _tagProximitySensor2D
{
	BASE_NODE
	SFVec2f center;	/*exposedField*/
	SFVec2f size;	/*exposedField*/
	SFBool enabled;	/*exposedField*/
	SFBool isActive;	/*eventOut*/
	SFVec2f position_changed;	/*eventOut*/
	SFFloat orientation_changed;	/*eventOut*/
	SFTime enterTime;	/*eventOut*/
	SFTime exitTime;	/*eventOut*/
} M_ProximitySensor2D;


typedef struct _tagProximitySensor
{
	BASE_NODE
	SFVec3f center;	/*exposedField*/
	SFVec3f size;	/*exposedField*/
	SFBool enabled;	/*exposedField*/
	SFBool isActive;	/*eventOut*/
	SFVec3f position_changed;	/*eventOut*/
	SFRotation orientation_changed;	/*eventOut*/
	SFTime enterTime;	/*eventOut*/
	SFTime exitTime;	/*eventOut*/
} M_ProximitySensor;


typedef struct _tagQuantizationParameter
{
	BASE_NODE
	SFBool isLocal;	/*field*/
	SFBool position3DQuant;	/*field*/
	SFVec3f position3DMin;	/*field*/
	SFVec3f position3DMax;	/*field*/
	SFInt32 position3DNbBits;	/*field*/
	SFBool position2DQuant;	/*field*/
	SFVec2f position2DMin;	/*field*/
	SFVec2f position2DMax;	/*field*/
	SFInt32 position2DNbBits;	/*field*/
	SFBool drawOrderQuant;	/*field*/
	SFFloat drawOrderMin;	/*field*/
	SFFloat drawOrderMax;	/*field*/
	SFInt32 drawOrderNbBits;	/*field*/
	SFBool colorQuant;	/*field*/
	SFFloat colorMin;	/*field*/
	SFFloat colorMax;	/*field*/
	SFInt32 colorNbBits;	/*field*/
	SFBool textureCoordinateQuant;	/*field*/
	SFFloat textureCoordinateMin;	/*field*/
	SFFloat textureCoordinateMax;	/*field*/
	SFInt32 textureCoordinateNbBits;	/*field*/
	SFBool angleQuant;	/*field*/
	SFFloat angleMin;	/*field*/
	SFFloat angleMax;	/*field*/
	SFInt32 angleNbBits;	/*field*/
	SFBool scaleQuant;	/*field*/
	SFFloat scaleMin;	/*field*/
	SFFloat scaleMax;	/*field*/
	SFInt32 scaleNbBits;	/*field*/
	SFBool keyQuant;	/*field*/
	SFFloat keyMin;	/*field*/
	SFFloat keyMax;	/*field*/
	SFInt32 keyNbBits;	/*field*/
	SFBool normalQuant;	/*field*/
	SFInt32 normalNbBits;	/*field*/
	SFBool sizeQuant;	/*field*/
	SFFloat sizeMin;	/*field*/
	SFFloat sizeMax;	/*field*/
	SFInt32 sizeNbBits;	/*field*/
	SFBool useEfficientCoding;	/*field*/
} M_QuantizationParameter;


typedef struct _tagRectangle
{
	BASE_NODE
	SFVec2f size;	/*exposedField*/
} M_Rectangle;


typedef struct _tagScalarInterpolator
{
	BASE_NODE
	SFFloat set_fraction;	/*eventIn*/
	void (*on_set_fraction)(GF_Node *pThis);	/*eventInHandler*/
	MFFloat key;	/*exposedField*/
	MFFloat keyValue;	/*exposedField*/
	SFFloat value_changed;	/*eventOut*/
} M_ScalarInterpolator;


typedef struct _tagScript
{
	BASE_NODE
	MFScript url;	/*exposedField*/
	SFBool directOutput;	/*field*/
	SFBool mustEvaluate;	/*field*/
} M_Script;


typedef struct _tagShape
{
	BASE_NODE
	GF_Node *appearance;	/*exposedField*/
	GF_Node *geometry;	/*exposedField*/
} M_Shape;


typedef struct _tagSound
{
	BASE_NODE
	SFVec3f direction;	/*exposedField*/
	SFFloat intensity;	/*exposedField*/
	SFVec3f location;	/*exposedField*/
	SFFloat maxBack;	/*exposedField*/
	SFFloat maxFront;	/*exposedField*/
	SFFloat minBack;	/*exposedField*/
	SFFloat minFront;	/*exposedField*/
	SFFloat priority;	/*exposedField*/
	GF_Node *source;	/*exposedField*/
	SFBool spatialize;	/*field*/
} M_Sound;


typedef struct _tagSound2D
{
	BASE_NODE
	SFFloat intensity;	/*exposedField*/
	SFVec2f location;	/*exposedField*/
	GF_Node *source;	/*exposedField*/
	SFBool spatialize;	/*field*/
} M_Sound2D;


typedef struct _tagSphere
{
	BASE_NODE
	SFFloat radius;	/*field*/
} M_Sphere;


typedef struct _tagSphereSensor
{
	BASE_NODE
	SFBool autoOffset;	/*exposedField*/
	SFBool enabled;	/*exposedField*/
	SFRotation offset;	/*exposedField*/
	SFBool isActive;	/*eventOut*/
	SFRotation rotation_changed;	/*eventOut*/
	SFVec3f trackPoint_changed;	/*eventOut*/
} M_SphereSensor;


typedef struct _tagSpotLight
{
	BASE_NODE
	SFFloat ambientIntensity;	/*exposedField*/
	SFVec3f attenuation;	/*exposedField*/
	SFFloat beamWidth;	/*exposedField*/
	SFColor color;	/*exposedField*/
	SFFloat cutOffAngle;	/*exposedField*/
	SFVec3f direction;	/*exposedField*/
	SFFloat intensity;	/*exposedField*/
	SFVec3f location;	/*exposedField*/
	SFBool on;	/*exposedField*/
	SFFloat radius;	/*exposedField*/
} M_SpotLight;


typedef struct _tagSwitch
{
	BASE_NODE
	GF_ChildNodeItem *choice;	/*exposedField*/
	SFInt32 whichChoice;	/*exposedField*/
} M_Switch;


typedef struct _tagTermCap
{
	BASE_NODE
	SFTime evaluate;	/*eventIn*/
	void (*on_evaluate)(GF_Node *pThis);	/*eventInHandler*/
	SFInt32 capability;	/*exposedField*/
	SFInt32 value;	/*eventOut*/
} M_TermCap;


typedef struct _tagText
{
	BASE_NODE
	MFString string;	/*exposedField*/
	MFFloat length;	/*exposedField*/
	GF_Node *fontStyle;	/*exposedField*/
	SFFloat maxExtent;	/*exposedField*/
} M_Text;


typedef struct _tagTextureCoordinate
{
	BASE_NODE
	MFVec2f point;	/*exposedField*/
} M_TextureCoordinate;


typedef struct _tagTextureTransform
{
	BASE_NODE
	SFVec2f center;	/*exposedField*/
	SFFloat rotation;	/*exposedField*/
	SFVec2f scale;	/*exposedField*/
	SFVec2f translation;	/*exposedField*/
} M_TextureTransform;


typedef struct _tagTimeSensor
{
	BASE_NODE
	SFTime cycleInterval;	/*exposedField*/
	SFBool enabled;	/*exposedField*/
	SFBool loop;	/*exposedField*/
	SFTime startTime;	/*exposedField*/
	SFTime stopTime;	/*exposedField*/
	SFTime cycleTime;	/*eventOut*/
	SFFloat fraction_changed;	/*eventOut*/
	SFBool isActive;	/*eventOut*/
	SFTime time;	/*eventOut*/
} M_TimeSensor;


typedef struct _tagTouchSensor
{
	BASE_NODE
	SFBool enabled;	/*exposedField*/
	SFVec3f hitNormal_changed;	/*eventOut*/
	SFVec3f hitPoint_changed;	/*eventOut*/
	SFVec2f hitTexCoord_changed;	/*eventOut*/
	SFBool isActive;	/*eventOut*/
	SFBool isOver;	/*eventOut*/
	SFTime touchTime;	/*eventOut*/
} M_TouchSensor;


typedef struct _tagTransform
{
	BASE_NODE
	VRML_CHILDREN
	SFVec3f center;	/*exposedField*/
	SFRotation rotation;	/*exposedField*/
	SFVec3f scale;	/*exposedField*/
	SFRotation scaleOrientation;	/*exposedField*/
	SFVec3f translation;	/*exposedField*/
} M_Transform;


typedef struct _tagTransform2D
{
	BASE_NODE
	VRML_CHILDREN
	SFVec2f center;	/*exposedField*/
	SFFloat rotationAngle;	/*exposedField*/
	SFVec2f scale;	/*exposedField*/
	SFFloat scaleOrientation;	/*exposedField*/
	SFVec2f translation;	/*exposedField*/
} M_Transform2D;


typedef struct _tagValuator
{
	BASE_NODE
	SFBool inSFBool;	/*eventIn*/
	void (*on_inSFBool)(GF_Node *pThis);	/*eventInHandler*/
	SFColor inSFColor;	/*eventIn*/
	void (*on_inSFColor)(GF_Node *pThis);	/*eventInHandler*/
	MFColor inMFColor;	/*eventIn*/
	void (*on_inMFColor)(GF_Node *pThis);	/*eventInHandler*/
	SFFloat inSFFloat;	/*eventIn*/
	void (*on_inSFFloat)(GF_Node *pThis);	/*eventInHandler*/
	MFFloat inMFFloat;	/*eventIn*/
	void (*on_inMFFloat)(GF_Node *pThis);	/*eventInHandler*/
	SFInt32 inSFInt32;	/*eventIn*/
	void (*on_inSFInt32)(GF_Node *pThis);	/*eventInHandler*/
	MFInt32 inMFInt32;	/*eventIn*/
	void (*on_inMFInt32)(GF_Node *pThis);	/*eventInHandler*/
	SFRotation inSFRotation;	/*eventIn*/
	void (*on_inSFRotation)(GF_Node *pThis);	/*eventInHandler*/
	MFRotation inMFRotation;	/*eventIn*/
	void (*on_inMFRotation)(GF_Node *pThis);	/*eventInHandler*/
	SFString inSFString;	/*eventIn*/
	void (*on_inSFString)(GF_Node *pThis);	/*eventInHandler*/
	MFString inMFString;	/*eventIn*/
	void (*on_inMFString)(GF_Node *pThis);	/*eventInHandler*/
	SFTime inSFTime;	/*eventIn*/
	void (*on_inSFTime)(GF_Node *pThis);	/*eventInHandler*/
	SFVec2f inSFVec2f;	/*eventIn*/
	void (*on_inSFVec2f)(GF_Node *pThis);	/*eventInHandler*/
	MFVec2f inMFVec2f;	/*eventIn*/
	void (*on_inMFVec2f)(GF_Node *pThis);	/*eventInHandler*/
	SFVec3f inSFVec3f;	/*eventIn*/
	void (*on_inSFVec3f)(GF_Node *pThis);	/*eventInHandler*/
	MFVec3f inMFVec3f;	/*eventIn*/
	void (*on_inMFVec3f)(GF_Node *pThis);	/*eventInHandler*/
	SFBool outSFBool;	/*eventOut*/
	SFColor outSFColor;	/*eventOut*/
	MFColor outMFColor;	/*eventOut*/
	SFFloat outSFFloat;	/*eventOut*/
	MFFloat outMFFloat;	/*eventOut*/
	SFInt32 outSFInt32;	/*eventOut*/
	MFInt32 outMFInt32;	/*eventOut*/
	SFRotation outSFRotation;	/*eventOut*/
	MFRotation outMFRotation;	/*eventOut*/
	SFString outSFString;	/*eventOut*/
	MFString outMFString;	/*eventOut*/
	SFTime outSFTime;	/*eventOut*/
	SFVec2f outSFVec2f;	/*eventOut*/
	MFVec2f outMFVec2f;	/*eventOut*/
	SFVec3f outSFVec3f;	/*eventOut*/
	MFVec3f outMFVec3f;	/*eventOut*/
	SFFloat Factor1;	/*exposedField*/
	SFFloat Factor2;	/*exposedField*/
	SFFloat Factor3;	/*exposedField*/
	SFFloat Factor4;	/*exposedField*/
	SFFloat Offset1;	/*exposedField*/
	SFFloat Offset2;	/*exposedField*/
	SFFloat Offset3;	/*exposedField*/
	SFFloat Offset4;	/*exposedField*/
	SFBool Sum;	/*exposedField*/
} M_Valuator;


typedef struct _tagViewpoint
{
	BASE_NODE
	SFBool set_bind;	/*eventIn*/
	void (*on_set_bind)(GF_Node *pThis);	/*eventInHandler*/
	SFFloat fieldOfView;	/*exposedField*/
	SFBool jump;	/*exposedField*/
	SFRotation orientation;	/*exposedField*/
	SFVec3f position;	/*exposedField*/
	SFString description;	/*field*/
	SFTime bindTime;	/*eventOut*/
	SFBool isBound;	/*eventOut*/
} M_Viewpoint;


typedef struct _tagVisibilitySensor
{
	BASE_NODE
	SFVec3f center;	/*exposedField*/
	SFBool enabled;	/*exposedField*/
	SFVec3f size;	/*exposedField*/
	SFTime enterTime;	/*eventOut*/
	SFTime exitTime;	/*eventOut*/
	SFBool isActive;	/*eventOut*/
} M_VisibilitySensor;


typedef struct _tagWorldInfo
{
	BASE_NODE
	MFString info;	/*field*/
	SFString title;	/*field*/
} M_WorldInfo;


typedef struct _tagAcousticMaterial
{
	BASE_NODE
	SFFloat ambientIntensity;	/*exposedField*/
	SFColor diffuseColor;	/*exposedField*/
	SFColor emissiveColor;	/*exposedField*/
	SFFloat shininess;	/*exposedField*/
	SFColor specularColor;	/*exposedField*/
	SFFloat transparency;	/*exposedField*/
	MFFloat reffunc;	/*field*/
	MFFloat transfunc;	/*field*/
	MFFloat refFrequency;	/*field*/
	MFFloat transFrequency;	/*field*/
} M_AcousticMaterial;


typedef struct _tagAcousticScene
{
	BASE_NODE
	SFVec3f center;	/*field*/
	SFVec3f Size;	/*field*/
	MFTime reverbTime;	/*field*/
	MFFloat reverbFreq;	/*field*/
	SFFloat reverbLevel;	/*exposedField*/
	SFTime reverbDelay;	/*exposedField*/
} M_AcousticScene;


typedef struct _tagApplicationWindow
{
	BASE_NODE
	SFBool isActive;	/*exposedField*/
	SFTime startTime;	/*exposedField*/
	SFTime stopTime;	/*exposedField*/
	SFString description;	/*exposedField*/
	MFString parameter;	/*exposedField*/
	MFURL url;	/*exposedField*/
	SFVec2f size;	/*exposedField*/
} M_ApplicationWindow;


typedef struct _tagDirectiveSound
{
	BASE_NODE
	SFVec3f direction;	/*exposedField*/
	SFFloat intensity;	/*exposedField*/
	SFVec3f location;	/*exposedField*/
	GF_Node *source;	/*exposedField*/
	GF_Node *perceptualParameters;	/*exposedField*/
	SFBool roomEffect;	/*exposedField*/
	SFBool spatialize;	/*exposedField*/
	MFFloat directivity;	/*field*/
	MFFloat angles;	/*field*/
	MFFloat frequency;	/*field*/
	SFFloat speedOfSound;	/*field*/
	SFFloat distance;	/*field*/
	SFBool useAirabs;	/*field*/
} M_DirectiveSound;


typedef struct _tagHierarchical3DMesh
{
	BASE_NODE
	SFInt32 triangleBudget;	/*eventIn*/
	void (*on_triangleBudget)(GF_Node *pThis);	/*eventInHandler*/
	SFFloat level;	/*exposedField*/
	MFURL url;	/*field*/
	SFBool doneLoading;	/*eventOut*/
} M_Hierarchical3DMesh;


typedef struct _tagMaterialKey
{
	BASE_NODE
	SFBool isKeyed;	/*exposedField*/
	SFBool isRGB;	/*exposedField*/
	SFColor keyColor;	/*exposedField*/
	SFFloat lowThreshold;	/*exposedField*/
	SFFloat highThreshold;	/*exposedField*/
	SFFloat transparency;	/*exposedField*/
} M_MaterialKey;


typedef struct _tagPerceptualParameters
{
	BASE_NODE
	SFFloat sourcePresence;	/*exposedField*/
	SFFloat sourceWarmth;	/*exposedField*/
	SFFloat sourceBrilliance;	/*exposedField*/
	SFFloat roomPresence;	/*exposedField*/
	SFFloat runningReverberance;	/*exposedField*/
	SFFloat envelopment;	/*exposedField*/
	SFFloat lateReverberance;	/*exposedField*/
	SFFloat heavyness;	/*exposedField*/
	SFFloat liveness;	/*exposedField*/
	MFFloat omniDirectivity;	/*exposedField*/
	MFFloat directFilterGains;	/*exposedField*/
	MFFloat inputFilterGains;	/*exposedField*/
	SFFloat refDistance;	/*exposedField*/
	SFFloat freqLow;	/*exposedField*/
	SFFloat freqHigh;	/*exposedField*/
	SFTime timeLimit1;	/*exposedField*/
	SFTime timeLimit2;	/*exposedField*/
	SFTime timeLimit3;	/*exposedField*/
	SFTime modalDensity;	/*exposedField*/
} M_PerceptualParameters;


typedef struct _tagTemporalTransform
{
	BASE_NODE
	VRML_CHILDREN
	MFURL url;	/*exposedField*/
	SFTime startTime;	/*exposedField*/
	SFTime optimalDuration;	/*exposedField*/
	SFBool active;	/*exposedField*/
	SFFloat speed;	/*exposedField*/
	SFVec2f scalability;	/*exposedField*/
	MFInt32 stretchMode;	/*exposedField*/
	MFInt32 shrinkMode;	/*exposedField*/
	SFTime maxDelay;	/*exposedField*/
	SFTime actualDuration;	/*eventOut*/
} M_TemporalTransform;


typedef struct _tagTemporalGroup
{
	BASE_NODE
	VRML_CHILDREN
	SFBool costart;	/*field*/
	SFBool coend;	/*field*/
	SFBool meet;	/*field*/
	MFFloat priority;	/*exposedField*/
	SFBool isActive;	/*eventOut*/
	SFInt32 activeChild;	/*eventOut*/
} M_TemporalGroup;


typedef struct _tagServerCommand
{
	BASE_NODE
	SFBool trigger;	/*eventIn*/
	void (*on_trigger)(GF_Node *pThis);	/*eventInHandler*/
	SFBool enable;	/*exposedField*/
	MFURL url;	/*exposedField*/
	SFString command;	/*exposedField*/
} M_ServerCommand;


typedef struct _tagInputSensor
{
	BASE_NODE
	SFBool enabled;	/*exposedField*/
	SFCommandBuffer buffer;	/*exposedField*/
	MFURL url;	/*exposedField*/
	SFTime eventTime;	/*eventOut*/
} M_InputSensor;


typedef struct _tagMatteTexture
{
	BASE_NODE
	GF_Node *surfaceA;	/*field*/
	GF_Node *surfaceB;	/*field*/
	GF_Node *alphaSurface;	/*field*/
	SFString operation;	/*exposedField*/
	SFBool overwrite;	/*field*/
	SFFloat fraction;	/*exposedField*/
	MFFloat parameter;	/*exposedField*/
} M_MatteTexture;


typedef struct _tagMediaBuffer
{
	BASE_NODE
	SFFloat bufferSize;	/*exposedField*/
	MFURL url;	/*exposedField*/
	SFTime mediaStartTime;	/*exposedField*/
	SFTime mediaStopTime;	/*exposedField*/
	SFBool isBuffered;	/*eventOut*/
	SFBool enabled;	/*exposedField*/
} M_MediaBuffer;


typedef struct _tagMediaControl
{
	BASE_NODE
	MFURL url;	/*exposedField*/
	SFTime mediaStartTime;	/*exposedField*/
	SFTime mediaStopTime;	/*exposedField*/
	SFFloat mediaSpeed;	/*exposedField*/
	SFBool loop;	/*exposedField*/
	SFBool preRoll;	/*exposedField*/
	SFBool mute;	/*exposedField*/
	SFBool enabled;	/*exposedField*/
	SFBool isPreRolled;	/*eventOut*/
} M_MediaControl;


typedef struct _tagMediaSensor
{
	BASE_NODE
	MFURL url;	/*exposedField*/
	SFTime mediaCurrentTime;	/*eventOut*/
	SFTime streamObjectStartTime;	/*eventOut*/
	SFTime mediaDuration;	/*eventOut*/
	SFBool isActive;	/*eventOut*/
	MFString info;	/*eventOut*/
} M_MediaSensor;


typedef struct _tagCoordinateInterpolator4D
{
	BASE_NODE
	SFFloat set_fraction;	/*eventIn*/
	void (*on_set_fraction)(GF_Node *pThis);	/*eventInHandler*/
	MFFloat key;	/*exposedField*/
	MFVec4f keyValue;	/*exposedField*/
	MFVec4f value_changed;	/*eventOut*/
} M_CoordinateInterpolator4D;


typedef struct _tagNonLinearDeformer
{
	BASE_NODE
	SFVec3f axis;	/*exposedField*/
	MFFloat extend;	/*exposedField*/
	GF_Node *geometry;	/*exposedField*/
	SFFloat param;	/*exposedField*/
	SFInt32 type;	/*exposedField*/
} M_NonLinearDeformer;


typedef struct _tagPositionAnimator
{
	BASE_NODE
	SFFloat set_fraction;	/*eventIn*/
	void (*on_set_fraction)(GF_Node *pThis);	/*eventInHandler*/
	SFVec2f fromTo;	/*exposedField*/
	MFFloat key;	/*exposedField*/
	MFRotation keyOrientation;	/*exposedField*/
	SFInt32 keyType;	/*exposedField*/
	MFVec2f keySpline;	/*exposedField*/
	MFVec3f keyValue;	/*exposedField*/
	SFInt32 keyValueType;	/*exposedField*/
	SFVec3f offset;	/*exposedField*/
	MFFloat weight;	/*exposedField*/
	SFVec3f endValue;	/*eventOut*/
	SFRotation rotation_changed;	/*eventOut*/
	SFVec3f value_changed;	/*eventOut*/
} M_PositionAnimator;


typedef struct _tagPositionAnimator2D
{
	BASE_NODE
	SFFloat set_fraction;	/*eventIn*/
	void (*on_set_fraction)(GF_Node *pThis);	/*eventInHandler*/
	SFVec2f fromTo;	/*exposedField*/
	MFFloat key;	/*exposedField*/
	SFInt32 keyOrientation;	/*exposedField*/
	SFInt32 keyType;	/*exposedField*/
	MFVec2f keySpline;	/*exposedField*/
	MFVec2f keyValue;	/*exposedField*/
	SFInt32 keyValueType;	/*exposedField*/
	SFVec2f offset;	/*exposedField*/
	MFFloat weight;	/*exposedField*/
	SFVec2f endValue;	/*eventOut*/
	SFFloat rotation_changed;	/*eventOut*/
	SFVec2f value_changed;	/*eventOut*/
} M_PositionAnimator2D;


typedef struct _tagPositionInterpolator4D
{
	BASE_NODE
	SFFloat set_fraction;	/*eventIn*/
	void (*on_set_fraction)(GF_Node *pThis);	/*eventInHandler*/
	MFFloat key;	/*exposedField*/
	MFVec4f keyValue;	/*exposedField*/
	SFVec4f value_changed;	/*eventOut*/
} M_PositionInterpolator4D;


typedef struct _tagScalarAnimator
{
	BASE_NODE
	SFFloat set_fraction;	/*eventIn*/
	void (*on_set_fraction)(GF_Node *pThis);	/*eventInHandler*/
	SFVec2f fromTo;	/*exposedField*/
	MFFloat key;	/*exposedField*/
	SFInt32 keyType;	/*exposedField*/
	MFVec2f keySpline;	/*exposedField*/
	MFFloat keyValue;	/*exposedField*/
	SFInt32 keyValueType;	/*exposedField*/
	SFFloat offset;	/*exposedField*/
	MFFloat weight;	/*exposedField*/
	SFFloat endValue;	/*eventOut*/
	SFFloat value_changed;	/*eventOut*/
} M_ScalarAnimator;


typedef struct _tagClipper2D
{
	BASE_NODE
	VRML_CHILDREN
	GF_Node *geometry;	/*exposedField*/
	SFBool inside;	/*exposedField*/
	GF_Node *transform;	/*exposedField*/
	SFBool XOR;	/*exposedField*/
} M_Clipper2D;


typedef struct _tagColorTransform
{
	BASE_NODE
	VRML_CHILDREN
	SFFloat mrr;	/*exposedField*/
	SFFloat mrg;	/*exposedField*/
	SFFloat mrb;	/*exposedField*/
	SFFloat mra;	/*exposedField*/
	SFFloat tr;	/*exposedField*/
	SFFloat mgr;	/*exposedField*/
	SFFloat mgg;	/*exposedField*/
	SFFloat mgb;	/*exposedField*/
	SFFloat mga;	/*exposedField*/
	SFFloat tg;	/*exposedField*/
	SFFloat mbr;	/*exposedField*/
	SFFloat mbg;	/*exposedField*/
	SFFloat mbb;	/*exposedField*/
	SFFloat mba;	/*exposedField*/
	SFFloat tb;	/*exposedField*/
	SFFloat mar;	/*exposedField*/
	SFFloat mag;	/*exposedField*/
	SFFloat mab;	/*exposedField*/
	SFFloat maa;	/*exposedField*/
	SFFloat ta;	/*exposedField*/
} M_ColorTransform;


typedef struct _tagEllipse
{
	BASE_NODE
	SFVec2f radius;	/*exposedField*/
} M_Ellipse;


typedef struct _tagLinearGradient
{
	BASE_NODE
	SFVec2f endPoint;	/*exposedField*/
	MFFloat key;	/*exposedField*/
	MFColor keyValue;	/*exposedField*/
	MFFloat opacity;	/*exposedField*/
	SFInt32 spreadMethod;	/*exposedField*/
	SFVec2f startPoint;	/*exposedField*/
	GF_Node *transform;	/*exposedField*/
} M_LinearGradient;


typedef struct _tagPathLayout
{
	BASE_NODE
	VRML_CHILDREN
	GF_Node *geometry;	/*exposedField*/
	MFInt32 alignment;	/*exposedField*/
	SFFloat pathOffset;	/*exposedField*/
	SFFloat spacing;	/*exposedField*/
	SFBool reverseLayout;	/*exposedField*/
	SFInt32 wrapMode;	/*exposedField*/
	SFBool splitText;	/*exposedField*/
} M_PathLayout;


typedef struct _tagRadialGradient
{
	BASE_NODE
	SFVec2f center;	/*exposedField*/
	SFVec2f focalPoint;	/*exposedField*/
	MFFloat key;	/*exposedField*/
	MFColor keyValue;	/*exposedField*/
	MFFloat opacity;	/*exposedField*/
	SFFloat radius;	/*exposedField*/
	SFInt32 spreadMethod;	/*exposedField*/
	GF_Node *transform;	/*exposedField*/
} M_RadialGradient;


typedef struct _tagTransformMatrix2D
{
	BASE_NODE
	VRML_CHILDREN
	SFFloat mxx;	/*exposedField*/
	SFFloat mxy;	/*exposedField*/
	SFFloat tx;	/*exposedField*/
	SFFloat myx;	/*exposedField*/
	SFFloat myy;	/*exposedField*/
	SFFloat ty;	/*exposedField*/
} M_TransformMatrix2D;


typedef struct _tagViewport
{
	BASE_NODE
	SFBool set_bind;	/*eventIn*/
	void (*on_set_bind)(GF_Node *pThis);	/*eventInHandler*/
	SFVec2f position;	/*exposedField*/
	SFVec2f size;	/*exposedField*/
	SFFloat orientation;	/*exposedField*/
	MFInt32 alignment;	/*exposedField*/
	SFInt32 fit;	/*exposedField*/
	SFString description;	/*field*/
	SFTime bindTime;	/*eventOut*/
	SFBool isBound;	/*eventOut*/
} M_Viewport;


typedef struct _tagXCurve2D
{
	BASE_NODE
	GF_Node *point;	/*exposedField*/
	SFFloat fineness;	/*exposedField*/
	MFInt32 type;	/*exposedField*/
} M_XCurve2D;


typedef struct _tagXFontStyle
{
	BASE_NODE
	MFString fontName;	/*exposedField*/
	SFBool horizontal;	/*exposedField*/
	MFString justify;	/*exposedField*/
	SFString language;	/*exposedField*/
	SFBool leftToRight;	/*exposedField*/
	SFFloat size;	/*exposedField*/
	SFString stretch;	/*exposedField*/
	SFFloat letterSpacing;	/*exposedField*/
	SFFloat wordSpacing;	/*exposedField*/
	SFInt32 weight;	/*exposedField*/
	SFBool fontKerning;	/*exposedField*/
	SFString style;	/*exposedField*/
	SFBool topToBottom;	/*exposedField*/
	MFString featureName;	/*exposedField*/
	MFInt32 featureStartOffset;	/*exposedField*/
	MFInt32 featureLength;	/*exposedField*/
	MFInt32 featureValue;	/*exposedField*/
} M_XFontStyle;


typedef struct _tagXLineProperties
{
	BASE_NODE
	SFColor lineColor;	/*exposedField*/
	SFInt32 lineStyle;	/*exposedField*/
	SFBool isCenterAligned;	/*exposedField*/
	SFBool isScalable;	/*exposedField*/
	SFInt32 lineCap;	/*exposedField*/
	SFInt32 lineJoin;	/*exposedField*/
	SFFloat miterLimit;	/*exposedField*/
	SFFloat transparency;	/*exposedField*/
	SFFloat width;	/*exposedField*/
	SFFloat dashOffset;	/*exposedField*/
	MFFloat dashes;	/*exposedField*/
	GF_Node *texture;	/*exposedField*/
	GF_Node *textureTransform;	/*exposedField*/
} M_XLineProperties;


/*NodeDataType tags*/
enum {
	NDT_SFWorldNode = 1,
	NDT_SF3DNode,
	NDT_SF2DNode,
	NDT_SFStreamingNode,
	NDT_SFAppearanceNode,
	NDT_SFAudioNode,
	NDT_SFBackground3DNode,
	NDT_SFBackground2DNode,
	NDT_SFGeometryNode,
	NDT_SFColorNode,
	NDT_SFTextureNode,
	NDT_SFCoordinateNode,
	NDT_SFCoordinate2DNode,
	NDT_SFExpressionNode,
	NDT_SFFaceDefMeshNode,
	NDT_SFFaceDefTablesNode,
	NDT_SFFaceDefTransformNode,
	NDT_SFFAPNode,
	NDT_SFFDPNode,
	NDT_SFFITNode,
	NDT_SFFogNode,
	NDT_SFFontStyleNode,
	NDT_SFTopNode,
	NDT_SFLinePropertiesNode,
	NDT_SFMaterialNode,
	NDT_SFNavigationInfoNode,
	NDT_SFNormalNode,
	NDT_SFTextureCoordinateNode,
	NDT_SFTextureTransformNode,
	NDT_SFViewpointNode,
	NDT_SFVisemeNode,
	NDT_SFViewportNode,
	NDT_SFBAPNode,
	NDT_SFBDPNode,
	NDT_SFBodyDefTableNode,
	NDT_SFBodySegmentConnectionHintNode,
	NDT_SFPerceptualParameterNode,
	NDT_SFTemporalNode,
	NDT_SFDepthImageNode,
	NDT_SFBlendListNode,
	NDT_SFFrameListNode,
	NDT_SFLightMapNode,
	NDT_SFSurfaceMapNode,
	NDT_SFViewMapNode,
	NDT_SFParticleInitializerNode,
	NDT_SFInfluenceNode,
	NDT_SFDepthTextureNode,
	NDT_SFSBBoneNode,
	NDT_SFSBMuscleNode,
	NDT_SFSBSegmentNode,
	NDT_SFSBSiteNode,
	NDT_SFBaseMeshNode,
	NDT_SFSubdivSurfaceSectorNode
};

/*All BIFS versions handled*/
#define GF_BIFS_NUM_VERSION		6

enum {
	GF_BIFS_V1 = 1,
	GF_BIFS_V2,
	GF_BIFS_V3,
	GF_BIFS_V4,
	GF_BIFS_V5,
	GF_BIFS_V6,
	GF_BIFS_LAST_VERSION = GF_BIFS_V6
};



#ifdef __cplusplus
}
#endif



#endif		/*_nodes_mpeg4_H*/


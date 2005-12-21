/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Copyright (c) Cyril Concolato 2004
 *					All rights reserved
 *
 *  This file is part of GPAC / SVG Scene Graph sub-project
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

#ifndef _GF_SG_SVG_H_
#define _GF_SG_SVG_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <gpac/scenegraph.h>
#include <gpac/path2d.h>

/* SVG attribute types */
enum {
	SVG_Unknown_datatype					= 0,

	SVG_Boolean_datatype					= 1,
	
	SVG_Color_datatype						= 2,
	SVG_Paint_datatype						= 3,
	SVG_SVGColor_datatype = SVG_Paint_datatype,

	/* keyword enum types */
	SVG_FillRule_datatype					= 5,
	SVG_Clip_datatype = SVG_FillRule_datatype,
	SVG_StrokeLineJoin_datatype				= 6,
	SVG_StrokeLineCap_datatype				= 7,
	SVG_FontStyle_datatype					= 8,
	SVG_FontWeight_datatype					= 9,
	SVG_FontVariant_datatype				= 10,
	SVG_TextAnchor_datatype					= 11,
	SVG_TransformType_datatype				= 12, 
	SVG_Display_datatype					= 13, 
	SVG_Visibility_datatype					= 14,
	SVG_Overflow_datatype					= 15,
	SVG_ZoomAndPan_datatype					= 16,
	SVG_DisplayAlign_datatype				= 17,
	SVG_PointerEvents_datatype				= 18,
	SVG_RenderingHint_datatype				= 19,
	SVG_VectorEffect_datatype				= 20,
	SVG_PlaybackOrder_datatype				= 21,
	SVG_TimelineBegin_datatype				= 22,
	XML_Space_datatype						= 23,
	XMLEV_Propagate_datatype				= 24,
	XMLEV_DefaultAction_datatype			= 25,
	XMLEV_Phase_datatype					= 26,
	SMIL_SyncBehavior_datatype				= 27,
	SMIL_SyncTolerance_datatype				= 28,
	SMIL_AttributeType_datatype				= 29,
	SMIL_CalcMode_datatype					= 30,
	SMIL_Additive_datatype					= 31,
	SMIL_Accumulate_datatype				= 32,
	SMIL_Restart_datatype					= 33,
	SMIL_Fill_datatype						= 34,	
	SVG_GradientUnit_datatype				= 81,
	SVG_InitialVisibility_datatype			= 82,
	SVG_FocusHighlight_datatype				= 83,
	SVG_Overlay_datatype					= 86,
	SVG_TransformBehavior_datatype			= 87,

	/* SVG Number */
	SVG_Number_datatype						= 78,
	SVG_NumberOrPercentage_datatype			= 80,
	SVG_Opacity_datatype					= 35,
	SVG_StrokeMiterLimit_datatype			= 36,
	SVG_FontSize_datatype					= 37,
	SVG_StrokeDashOffset_datatype			= 38,
	SVG_AudioLevel_datatype					= 39,
	SVG_LineIncrement_datatype				= 40,
	SVG_StrokeWidth_datatype				= 41,
	SVG_Length_datatype						= 42,
	SVG_Coordinate_datatype					= 43,
	SVG_Rotate_datatype						= 84,

	/* List of */
	SVG_Numbers_datatype					= 79,
	SVG_Points_datatype						= 45,
	SVG_Coordinates_datatype				= 46,
	SVG_FeatureList_datatype				= 47,
	SVG_ExtensionList_datatype				= 48,
	SVG_FormatList_datatype					= 49,
	SVG_FontList_datatype					= 50,
	SVG_ListOfIRI_datatype					= 51,
	SVG_LanguageIDs_datatype				= 52,
	SMIL_KeyTimes_datatype					= 53,
	SMIL_KeySplines_datatype				= 54,
	SMIL_KeyPoints_datatype					= 55,
	SMIL_Times_datatype						= 56,

	SVG_PathData_datatype					= 57,

	SVG_FontFamily_datatype					= 58,

	/* animated (untyped) value */
	SMIL_AnimateValue_datatype				= 59,
	SMIL_AnimateValues_datatype				= 60,

	SVG_ID_datatype							= 61,
	SVG_IRI_datatype						= 62,

	SVG_Matrix_datatype						= 44,

	/* string types */
	SVG_String_datatype						= 63,

	SMIL_Duration_datatype					= 64,
	SMIL_RepeatCount_datatype				= 65,
	SMIL_AttributeName_datatype				= 66,

	SVG_StrokeDashArray_datatype			= 67,
	SVG_PreserveAspectRatio_datatype		= 68,
	SVG_ViewBox_datatype					= 69,
	SVG_GradientOffset_datatype				= 70,
	SVG_Focus_datatype						= 71,
	SVG_XML_Name_datatype					= 85,
	SVG_XSLT_QName_datatype					= 72,
	SVG_Clock_datatype						= 73,

	SVG_Motion_datatype						= 74, /* required for animateMotion */

	SVG_TextContent_datatype				= 4,

	SVG_ContentType_datatype				= 75,
	SVG_LinkTarget_datatype					= 76,
	SVG_LanguageID_datatype					= 77,
};

/* Reusing BIFS structure:
	fieldIndex:		index of the attribute in the element
	fieldType:		attribute data type as in the above enumeration
	name:			attribute name
	far_ptr:		pointer to the actual data

	NDTType:		unused in SVG
	eventType:		unused in SVG
	on_event_in:	unused in SVG */
typedef GF_FieldInfo SVGAttributeInfo;

/* Definition of SVG base data types */
typedef u8 *DOM_String;
typedef DOM_String SVG_String;
typedef DOM_String SVG_ContentType;
typedef DOM_String SVG_LanguageID;
typedef DOM_String SVG_TextContent;

/* types not yet handled properly, i.e. for the moment using strings */
typedef DOM_String SVG_Focus;
typedef DOM_String SVG_ID;
typedef DOM_String SVG_LinkTarget;
typedef DOM_String SVG_GradientOffset;

/*DOM event type*/
typedef u32 SVG_XSLT_QName;
typedef u32 SVG_XML_Name;

typedef Double SVG_Clock;

typedef GF_List *SVG_Numbers;
typedef GF_List	*SVG_FeatureList;
typedef GF_List	*SVG_ExtensionList;
typedef GF_List	*SVG_FormatList;
typedef GF_List	*SVG_ListOfIRI;
typedef GF_List	*SVG_LanguageIDs;
typedef GF_List	*SVG_FontList;

/* SMIL Anim types */
typedef struct {
	char *name;
	u32 type;
	/* used in case type is SVG_Matrix_datatype */
	u32 transform_type;

	void *field_ptr;
} SMIL_AttributeName;

enum {
	SMIL_TIME_UNSPECIFIED   = 0,
	SMIL_TIME_CLOCK			= 1,
	SMIL_TIME_WALLCLOCK		= 2,
	SMIL_TIME_EVENT			= 3,
	SMIL_TIME_INDEFINITE	= 4
};


typedef struct {
	/* Type of timing value*/
	u8 type;
	/* in case of syncbase, event, repeat value: this is the pointer to the source of the event */
	GF_Node *element; 
	/* parent animation*/
	GF_Node *owner_animation; 
	/* id of the element before resolution of the pointer to the element */
	char *element_id; 

	/* the animation event, the type of syncbase, ... */
	u8 event;
	/*
	0: this SMIL time is static
	1: this SMIL time is the prototype of a dynamic SMIL time (triggered by mouse event, access key)
	2: this SMIL time is a resolved value of a proto SMIL time, result of element.event and should be deleted whenever 
	appropriate*/
	u8 dynamic_type;
	/* either keyCode when event is accessKey or repeatCount when event is repeat */
	u32 parameter; 

	Double clock;
} SMIL_Time;

typedef GF_List * SMIL_Times;

enum {
	SMIL_DURATION_UNSPECIFIED = 0,
	SMIL_DURATION_INDEFINITE  = 1,
	SMIL_DURATION_DEFINED	  = 2,
	SMIL_DURATION_MEDIA		  = 3
};
typedef struct {
	u8 type;
	Double clock_value;
} SMIL_Duration;


enum {
	SMIL_RESTART_ALWAYS = 0,
	SMIL_RESTART_NEVER,
	SMIL_RESTART_WHENNOTACTIVE,
};
typedef u8 SMIL_Restart;

enum {
	SMIL_FILL_REMOVE = 0,
	SMIL_FILL_FREEZE
};
typedef u8 SMIL_Fill;

enum {
	SMIL_REPEATCOUNT_UNSPECIFIED = 0,
	SMIL_REPEATCOUNT_INDEFINITE  = 1,
	SMIL_REPEATCOUNT_DEFINED	 = 2
};
typedef struct {
	u8 type;
	Fixed count;
} SMIL_RepeatCount;

typedef struct {
	u8 type;
	u8 transform_type;
	void *value;
} SMIL_AnimateValue;

typedef struct {
	u8 type;
	u8 transform_type;
	GF_List *values;
} SMIL_AnimateValues;

typedef GF_List * SMIL_KeyTimes;
typedef GF_List * SMIL_KeyPoints;

/* Fixed between 0 and 1 */
typedef GF_List * SMIL_KeySplines;

enum {
	SMIL_ADDITIVE_REPLACE = 0,
	SMIL_ADDITIVE_SUM
}; 
typedef u8 SMIL_Additive;

enum {
	SMIL_ACCUMULATE_NONE = 0,
	SMIL_ACCUMULATE_SUM
}; 
typedef u8 SMIL_Accumulate;

enum {
	SMIL_CALCMODE_LINEAR = 0,
	SMIL_CALCMODE_DISCRETE,
	SMIL_CALCMODE_PACED,
	SMIL_CALCMODE_SPLINE
};
typedef u8 SMIL_CalcMode;
/* end of SMIL Anim types */


enum {
	SVG_IRI_IRI = 0,
	SVG_IRI_ELEMENTID
};
typedef struct {
	u8 type;
	u8 *iri;
	struct _svg_element *target;
} SVG_IRI;

enum {
	SVG_FONTFAMILY_INHERIT,
	SVG_FONTFAMILY_VALUE
};

typedef struct {
	u8 type;
	SVG_String value;
} SVG_FontFamily;

enum {
	SVG_FONTSTYLE_INHERIT = 0,
	SVG_FONTSTYLE_NORMAL = 1,
	SVG_FONTSTYLE_ITALIC = 2,  
	SVG_FONTSTYLE_OBLIQUE = 3
}; 
typedef u8 SVG_FontStyle;

enum {
	SVG_PATHCOMMAND_M = 0,
	SVG_PATHCOMMAND_L = 1,
	SVG_PATHCOMMAND_C = 2,
	SVG_PATHCOMMAND_S = 3,
	SVG_PATHCOMMAND_Q = 4,
	SVG_PATHCOMMAND_T = 5,
	SVG_PATHCOMMAND_A = 7,
	SVG_PATHCOMMAND_Z = 6
};

typedef struct {
	GF_List *commands;
	GF_List *points;
} SVG_PathData;

typedef struct {
	Fixed x, y;
} SVG_Point;
typedef GF_List * SVG_Points;

typedef struct {
	Fixed x, y, angle;
} SVG_Point_Angle;

typedef struct {
	Fixed x, y, width, height;
} SVG_Rect;

typedef SVG_Rect SVG_ViewBox;

typedef Bool SVG_Boolean;

/*WARNING - THESE ARE PATH FLAGS, CHECK IF WORKING*/
enum {
	SVG_FILLRULE_EVENODD= 0,
	SVG_FILLRULE_NONZERO,
	SVG_FILLRULE_INHERIT
};
typedef u8 SVG_FillRule, SVG_Clip;
	
enum {
	SVG_STROKELINEJOIN_MITER = GF_LINE_JOIN_MITER,
	SVG_STROKELINEJOIN_ROUND = GF_LINE_JOIN_ROUND,
	SVG_STROKELINEJOIN_BEVEL = GF_LINE_JOIN_BEVEL,
	SVG_STROKELINEJOIN_INHERIT = 100
};
typedef u8 SVG_StrokeLineJoin;

/* Warning: GPAC naming is not the same as SVG naming for line cap Flat = butt and Butt = square*/
enum {
	SVG_STROKELINECAP_BUTT = GF_LINE_CAP_FLAT,
	SVG_STROKELINECAP_ROUND = GF_LINE_CAP_ROUND,
	SVG_STROKELINECAP_SQUARE = GF_LINE_CAP_SQUARE,
	SVG_STROKELINECAP_INHERIT = 100
};
typedef u8 SVG_StrokeLineCap;

enum {
	SVG_OVERFLOW_INHERIT	= 0,
	SVG_OVERFLOW_VISIBLE	= 1,
	SVG_OVERFLOW_HIDDEN		= 2,
	SVG_OVERFLOW_SCROLL		= 3,
	SVG_OVERFLOW_AUTO		= 4
};
typedef u8 SVG_Overflow;

enum {
	SVG_COLOR_RGBCOLOR		= 0,
	SVG_COLOR_INHERIT		= 1,
	SVG_COLOR_CURRENTCOLOR	= 2
};

typedef struct {
	u8 type;
	Fixed red, green, blue;
} SVG_Color;

enum {
	SVG_PAINT_NONE		= 0,
	SVG_PAINT_COLOR		= 1,
	SVG_PAINT_URI		= 2,
	SVG_PAINT_INHERIT	= 3
};

typedef struct {
	u8 type;
	SVG_Color color;
	DOM_String uri;
} SVG_Paint, SVG_SVGColor;

enum {
	SVG_NUMBER_UNKNOWN		= 0,
	SVG_NUMBER_VALUE		= 1,
	SVG_NUMBER_PERCENTAGE	= 2,
	SVG_NUMBER_EMS			= 3,
	SVG_NUMBER_EXS			= 4,
	SVG_NUMBER_PX			= 5,
	SVG_NUMBER_CM			= 6,
	SVG_NUMBER_MM			= 7,
	SVG_NUMBER_IN			= 8,
	SVG_NUMBER_PT			= 9,
	SVG_NUMBER_PC			= 10,
	SVG_NUMBER_INHERIT		= 11,
	SVG_NUMBER_AUTO			= 12,
	SVG_NUMBER_AUTO_REVERSE	= 13
};

typedef struct {
	u8 type;
	Fixed value;
} SVG_Number, 
  SVG_Opacity, 
  SVG_StrokeMiterLimit, 
  SVG_FontSize, 
  SVG_StrokeDashOffset,
  SVG_AudioLevel,
  SVG_LineIncrement,
  SVG_Length, 
  SVG_Coordinate, 
  SVG_StrokeWidth,
  SVG_NumberOrPercentage,
  SVG_Rotate;

typedef GF_List * SVG_Coordinates;

typedef GF_Matrix2D SVG_Matrix;

enum {
	SVG_TRANSFORM_MATRIX	= 0,
	SVG_TRANSFORM_TRANSLATE = 1,
	SVG_TRANSFORM_SCALE		= 2,
	SVG_TRANSFORM_ROTATE	= 3,
	SVG_TRANSFORM_SKEWX		= 4,
	SVG_TRANSFORM_SKEWY		= 5,
	SVG_TRANSFORM_UNKNOWN	= 6
};

typedef u8 SVG_TransformType; 
typedef GF_List * SVG_TransformList;

enum {
	SVG_FONTWEIGHT_INHERIT,
	SVG_FONTWEIGHT_NORMAL,
	SVG_FONTWEIGHT_BOLD, 
	SVG_FONTWEIGHT_BOLDER, 
	SVG_FONTWEIGHT_LIGHTER, 
	SVG_FONTWEIGHT_100, 
	SVG_FONTWEIGHT_200,
	SVG_FONTWEIGHT_300, 
	SVG_FONTWEIGHT_400,
	SVG_FONTWEIGHT_500,
	SVG_FONTWEIGHT_600,
	SVG_FONTWEIGHT_700,
	SVG_FONTWEIGHT_800,
	SVG_FONTWEIGHT_900
};
typedef u8 SVG_FontWeight;

enum {
	SVG_FONTVARIANT_INHERIT		= 0,
	SVG_FONTVARIANT_NORMAL		= 1,
	SVG_FONTVARIANT_SMALLCAPS	= 2
};
typedef u8 SVG_FontVariant;

enum {
	SVG_VISIBILITY_INHERIT	= 0,
	SVG_VISIBILITY_VISIBLE  = 1,
	SVG_VISIBILITY_HIDDEN   = 2,
	SVG_VISIBILITY_COLLAPSE = 3
};
typedef u8 SVG_Visibility;

enum {
	SVG_DISPLAY_INHERIT = 0,
	SVG_DISPLAY_NONE    = 1,
	SVG_DISPLAY_INLINE  = 2,
	SVG_DISPLAY_BLOCK,
	SVG_DISPLAY_LIST_ITEM,
	SVG_DISPLAY_RUN_IN,
	SVG_DISPLAY_COMPACT,
	SVG_DISPLAY_MARKER,
	SVG_DISPLAY_TABLE,
	SVG_DISPLAY_INLINE_TABLE,
	SVG_DISPLAY_TABLE_ROW_GROUP,
	SVG_DISPLAY_TABLE_HEADER_GROUP,
	SVG_DISPLAY_TABLE_FOOTER_GROUP,
	SVG_DISPLAY_TABLE_ROW,
	SVG_DISPLAY_TABLE_COLUMN_GROUP,
	SVG_DISPLAY_TABLE_COLUMN,
	SVG_DISPLAY_TABLE_CELL,
	SVG_DISPLAY_TABLE_CAPTION
};
typedef u8 SVG_Display;

enum {
	SVG_DISPLAYALIGN_INHERIT	= 0,
	SVG_DISPLAYALIGN_AUTO		= 1,
	SVG_DISPLAYALIGN_AFTER		= 2,
	SVG_DISPLAYALIGN_BEFORE		= 3,
	SVG_DISPLAYALIGN_CENTER		= 4
};
typedef u8 SVG_DisplayAlign;

enum {
	SVG_STROKEDASHARRAY_NONE	= 0,
	SVG_STROKEDASHARRAY_INHERIT = 1,
	SVG_STROKEDASHARRAY_ARRAY	= 2
};

typedef struct {
	u32 count;
	Fixed* vals;
} Array;

typedef struct {
	u8 type;
	Array array;
} SVG_StrokeDashArray;

enum {
	SVG_TEXTANCHOR_START,
	SVG_TEXTANCHOR_MIDDLE,
	SVG_TEXTANCHOR_END,
	SVG_TEXTANCHOR_INHERIT
};
typedef u8 SVG_TextAnchor;

enum {
	SVG_ANGLETYPE_UNKNOWN = 0,
	SVG_ANGLETYPE_UNSPECIFIED = 1,
	SVG_ANGLETYPE_DEG = 2,
	SVG_ANGLETYPE_RAD = 3,
	SVG_ANGLETYPE_GRAD = 4
};

enum {
	SVG_UNIT_TYPE_UNKNOWN = 0,
	SVG_UNIT_TYPE_USERSPACEONUSE = 1,
	SVG_UNIT_TYPE_OBJECTBOUNDINGBOX = 2
};

enum {
	// Alignment Types
	SVG_PRESERVEASPECTRATIO_NONE = 1,
	SVG_PRESERVEASPECTRATIO_XMINYMIN = 2,
	SVG_PRESERVEASPECTRATIO_XMIDYMIN = 3,
	SVG_PRESERVEASPECTRATIO_XMAXYMIN = 4,
	SVG_PRESERVEASPECTRATIO_XMINYMID = 5,
	SVG_PRESERVEASPECTRATIO_XMIDYMID = 0, //default
	SVG_PRESERVEASPECTRATIO_XMAXYMID = 6,
	SVG_PRESERVEASPECTRATIO_XMINYMAX = 7,
	SVG_PRESERVEASPECTRATIO_XMIDYMAX = 8,
	SVG_PRESERVEASPECTRATIO_XMAXYMAX = 9
};

enum {
	// Meet_or_slice Types
	SVG_MEETORSLICE_MEET  = 0,
	SVG_MEETORSLICE_SLICE = 1
};

typedef struct {
	Bool defer;
	u8 align;
	u8 meetOrSlice;
} SVG_PreserveAspectRatio; 

enum {
	SVG_ZOOMANDPAN_MAGNIFY = 0,
	SVG_ZOOMANDPAN_DISABLE = 1
};

typedef u8 SVG_ZoomAndPan;

enum {
	LENGTHADJUST_UNKNOWN   = 0,
	LENGTHADJUST_SPACING     = 1,
	LENGTHADJUST_SPACINGANDGLYPHS     = 2
};

enum {
    // textPath Method Types
	TEXTPATH_METHODTYPE_UNKNOWN   = 0,
	TEXTPATH_METHODTYPE_ALIGN     = 1,
	TEXTPATH_METHODTYPE_STRETCH     = 2
};
enum {
    // textPath Spacing Types
	TEXTPATH_SPACINGTYPE_UNKNOWN   = 0,
	TEXTPATH_SPACINGTYPE_AUTO     = 1,
	TEXTPATH_SPACINGTYPE_EXACT     = 2
};

enum {
    // Marker Unit Types
	SVG_MARKERUNITS_UNKNOWN        = 0,
	SVG_MARKERUNITS_USERSPACEONUSE = 1,
	SVG_MARKERUNITS_STROKEWIDTH    = 2
};
enum {
    // Marker Orientation Types
	SVG_MARKER_ORIENT_UNKNOWN      = 0,
	SVG_MARKER_ORIENT_AUTO         = 1,
	SVG_MARKER_ORIENT_ANGLE        = 2
};

enum {
    // Spread Method Types
	SVG_SPREADMETHOD_UNKNOWN = 0,
	SVG_SPREADMETHOD_PAD     = 1,
	SVG_SPREADMETHOD_REFLECT = 2,
	SVG_SPREADMETHOD_REPEAT  = 3
};

enum {
	SVG_POINTEREVENTS_INHERIT			= 0,
	SVG_POINTEREVENTS_ALL				= 1,
	SVG_POINTEREVENTS_FILL				= 2,
	SVG_POINTEREVENTS_NONE				= 3,
	SVG_POINTEREVENTS_PAINTED			= 4,
	SVG_POINTEREVENTS_STROKE			= 5,
	SVG_POINTEREVENTS_VISIBLE			= 6,
	SVG_POINTEREVENTS_VISIBLEFILL		= 7,
	SVG_POINTEREVENTS_VISIBLEPAINTED	= 8,
	SVG_POINTEREVENTS_VISIBLESTROKE		= 9
};
typedef u8 SVG_PointerEvents;

enum {
	SVG_RENDERINGHINT_INHERIT				= 0,
	SVG_RENDERINGHINT_AUTO					= 1,
	SVG_RENDERINGHINT_OPTIMIZEQUALITY		= 2,
	SVG_RENDERINGHINT_OPTIMIZESPEED			= 3,
	SVG_RENDERINGHINT_OPTIMIZELEGIBILITY	= 4,
	SVG_RENDERINGHINT_CRISPEDGES			= 5,
	SVG_RENDERINGHINT_GEOMETRICPRECISION	= 6,

};
typedef u8 SVG_RenderingHint;

enum {
	SVG_VECTOREFFECT_INHERIT			= 0,
	SVG_VECTOREFFECT_NONE				= 1,
	SVG_VECTOREFFECT_NONSCALINGSTROKE	= 2,
};
typedef u8 SVG_VectorEffect;

enum {
	XMLEVENT_PROPAGATE_CONTINUE = 0,
	XMLEVENT_PROPAGATE_STOP		= 1
};
typedef u8 XMLEV_Propagate;

enum {
	XMLEVENT_DEFAULTACTION_CANCEL	= 0,
	XMLEVENT_DEFAULTACTION_PERFORM	= 1
};
typedef u8 XMLEV_DefaultAction;

enum {
	XMLEVENT_PHASE_DEFAULT	= 0,
	XMLEVENT_PHASE_CAPTURE	= 1
};
typedef u8 XMLEV_Phase;

enum {
	SMIL_SYNCBEHAVIOR_INHERIT		= 0,
	/*LASeR order*/
	SMIL_SYNCBEHAVIOR_CANSLIP,
	SMIL_SYNCBEHAVIOR_DEFAULT,
	SMIL_SYNCBEHAVIOR_INDEPENDENT,
	SMIL_SYNCBEHAVIOR_LOCKED,
};
typedef u8 SMIL_SyncBehavior;

enum {
	SMIL_SYNCTOLERANCE_INHERIT		= 0,
	SMIL_SYNCTOLERANCE_DEFAULT		= 1,
	SMIL_SYNCTOLERANCE_VALUE		= 2
};

typedef struct {
	u8 type;
	SVG_Clock value;
} SMIL_SyncTolerance;

enum {
	SMIL_ATTRIBUTETYPE_AUTO	= 0,
	SMIL_ATTRIBUTETYPE_XML	= 1,
	SMIL_ATTRIBUTETYPE_CSS	= 2
};
typedef u8 SMIL_AttributeType;

enum {
	SVG_PLAYBACKORDER_ALL			= 0,
	SVG_PLAYBACKORDER_FORWARDONLY	= 1,
};
typedef u8 SVG_PlaybackOrder;

enum {
	SVG_TIMELINEBEGIN_ONSTART		= 0,
	SVG_TIMELINEBEGIN_ONLOAD		= 1
};
typedef u8 SVG_TimelineBegin;

enum {
	XML_SPACE_DEFAULT		= 0,
	XML_SPACE_PRESERVE		= 1
};
typedef u8 XML_Space;


typedef enum {
	SVG_GRADIENTUNITS_OBJECT = 0,
	SVG_GRADIENTUNITS_USER = 1
} SVG_GradientUnit;

typedef enum {
	SVG_FOCUSHIGHLIGHT_AUTO = 0,
	SVG_FOCUSHIGHLIGHT_NONE	= 1
} SVG_FocusHighlight;

typedef enum {
	SVG_INITIALVISIBILTY_WHENSTARTED = 0,
	SVG_INITIALVISIBILTY_ALWAYS		 = 1
} SVG_InitialVisibility;

typedef enum {
	SVG_TRANSFORMBEHAVIOR_GEOMETRIC = 0,
	SVG_TRANSFORMBEHAVIOR_PINNED	= 1,
	SVG_TRANSFORMBEHAVIOR_PINNED90	= 2,
	SVG_TRANSFORMBEHAVIOR_PINNED180 = 3,
	SVG_TRANSFORMBEHAVIOR_PINNED270 = 4
} SVG_TransformBehavior;

typedef enum {
	SVG_OVERLAY_NONE = 0,
	SVG_OVERLAY_TOP	 = 1
} SVG_Overlay;

/**************************************************
 *  SVG's styling properties (see 6.1 in REC 1.1) *
 *************************************************/

typedef struct {
	/* Tiny 1.2 properties, alphabetically sorted */
	SVG_Paint					*color;
	SVG_Paint					*fill; 
	SVG_Paint					*stroke;
	SVG_Paint					*solid_color;
	SVG_Paint					*stop_color;
	SVG_Paint					*viewport_fill;
	SVG_Opacity					*fill_opacity;
	SVG_Opacity					*solid_opacity;
	SVG_Opacity					*stop_opacity;
	SVG_Opacity					*stroke_opacity;
	SVG_Opacity					*viewport_fill_opacity;
	SVG_AudioLevel				*audio_level;
	SVG_RenderingHint			*color_rendering;
	SVG_RenderingHint			*image_rendering;
	SVG_RenderingHint			*shape_rendering;
	SVG_RenderingHint			*text_rendering;
	SVG_Display					*display; 
	SVG_DisplayAlign			*display_align;
	SVG_FillRule				*fill_rule; 
	SVG_FontFamily				*font_family;
	SVG_FontSize				*font_size;
	SVG_FontStyle				*font_style; 
	SVG_FontWeight				*font_weight; 
	SVG_LineIncrement			*line_increment;
	SVG_PointerEvents			*pointer_events;
	SVG_StrokeDashArray			*stroke_dasharray;
	SVG_StrokeDashOffset		*stroke_dashoffset;
	SVG_StrokeLineCap			*stroke_linecap; 
	SVG_StrokeLineJoin			*stroke_linejoin; 
	SVG_StrokeMiterLimit		*stroke_miterlimit; 
	SVG_StrokeWidth				*stroke_width;
	SVG_TextAnchor				*text_anchor;
	SVG_VectorEffect			*vector_effect;
	SVG_Visibility				*visibility;

	/* Not a property in Tiny 1.2 because only on video but a property in Full 1.1 */
	SVG_Opacity					*opacity;

	SVG_Overflow				*overflow; 


	/* Full 1.1 props, i.e. not implemented */
/*
	SVG_FontVariant *font_variant; 
	SVG_String *font;
	SVG_String *font_size_adjust;
	SVG_String *font_stretch;
	SVG_String *direction;
	SVG_String *letter_spacing;
	SVG_String *text_decoration;
	SVG_String *unicode_bidi;
	SVG_String *word_spacing;
	SVG_String *clip; 
	SVG_String *cursor;
	SVG_String *clip_path;
	SVG_String *clip_rule;
	SVG_String *mask;
	SVG_String *enable_background;
	SVG_String *filter;
	SVG_String *flood_color;
	SVG_String *flood_opacity;
	SVG_String *lighting_color;
	SVG_String *color_interpolation;
	SVG_String *color_interpolation_filters;
	SVG_String *color_profile;
	SVG_String *marker;
	SVG_String *marker_end;
	SVG_String *marker_mid;
	SVG_String *marker_start;
	SVG_String *alignment_baseline;
	SVG_String *baseline_shift;
	SVG_String *dominant_baseline;
	SVG_String *glyph_orientation_horizontal;
	SVG_String *glyph_orientation_vertical;
	SVG_String *kerning;
	SVG_String *writing_mode;
*/
} SVGPropertiesPointers;

typedef struct {
	SVG_Paint					color;
	SVG_Paint					fill; 
	SVG_Paint					stroke;
	SVG_Paint					solid_color;
	SVG_Paint					stop_color;
	SVG_Paint					viewport_fill;
	SVG_Opacity					fill_opacity;
	SVG_Opacity					solid_opacity;
	SVG_Opacity					stop_opacity;
	SVG_Opacity					stroke_opacity;
	SVG_Opacity					viewport_fill_opacity;
	SVG_AudioLevel				audio_level;
	SVG_RenderingHint			color_rendering;
	SVG_RenderingHint			image_rendering;
	SVG_RenderingHint			shape_rendering;
	SVG_RenderingHint			text_rendering;
	SVG_Display					display; 
	SVG_DisplayAlign			display_align;
	SVG_FillRule				fill_rule; 
	SVG_FontFamily				font_family;
	SVG_FontSize				font_size;
	SVG_FontStyle				font_style; 
	SVG_FontWeight				font_weight; 
	SVG_LineIncrement			line_increment;
	SVG_PointerEvents			pointer_events;
	SVG_StrokeDashArray			stroke_dasharray;
	SVG_StrokeDashOffset		stroke_dashoffset;
	SVG_StrokeLineCap			stroke_linecap; 
	SVG_StrokeLineJoin			stroke_linejoin; 
	SVG_StrokeMiterLimit		stroke_miterlimit; 
	SVG_StrokeWidth				stroke_width;
	SVG_TextAnchor				text_anchor;
	SVG_VectorEffect			vector_effect;
	SVG_Visibility				visibility;

	/* Not a property in Tiny 1.2 because only on video but a property in Full 1.1 */
	SVG_Opacity					opacity;

	SVG_Overflow				overflow; 


	/* Full 1.1 props, i.e. not implemented */
/*
	SVG_FontVariant *font_variant; 
	SVG_String *font;
	SVG_String *font_size_adjust;
	SVG_String *font_stretch;
	SVG_String *direction;
	SVG_String *letter_spacing;
	SVG_String *text_decoration;
	SVG_String *unicode_bidi;
	SVG_String *word_spacing;
	SVG_String *clip; 
	SVG_String *cursor;
	SVG_String *clip_path;
	SVG_String *clip_rule;
	SVG_String *mask;
	SVG_String *enable_background;
	SVG_String *filter;
	SVG_String *flood_color;
	SVG_String *flood_opacity;
	SVG_String *lighting_color;
	SVG_String *color_interpolation;
	SVG_String *color_interpolation_filters;
	SVG_String *color_profile;
	SVG_String *marker;
	SVG_String *marker_end;
	SVG_String *marker_mid;
	SVG_String *marker_start;
	SVG_String *alignment_baseline;
	SVG_String *baseline_shift;
	SVG_String *dominant_baseline;
	SVG_String *glyph_orientation_horizontal;
	SVG_String *glyph_orientation_vertical;
	SVG_String *kerning;
	SVG_String *writing_mode;
*/
} SVGProperties;

typedef struct {
/*	SVG_ID id / xml_id; are actually nodeID in the sgprivate structure */
	SVG_IRI base;
	SVG_LanguageID lang;
	XML_Space space;
	SVG_String _class;
	SVG_Boolean eRR;
} XMLCoreAttributes;

typedef struct {
	SVG_FocusHighlight focusHighlight;
	Bool focusable;
	SVG_Focus nav_next, nav_prev; 
	SVG_Focus nav_down, nav_down_left, nav_down_right;
	SVG_Focus nav_left, nav_right;
	SVG_Focus nav_up, nav_up_left, nav_up_right;
} SVGFocusAttributes;

typedef struct {
	SVG_IRI href;
	SVG_ContentType type;
	SVG_String title;
	SVG_IRI arcrole; 
	SVG_IRI role;
	SVG_String show;
	SVG_String actuate;
} XLinkAttributes;

typedef struct {
	SMIL_Times begin, end;
	SMIL_Duration dur;
	SMIL_RepeatCount repeatCount;
	SMIL_Duration repeatDur;
	SMIL_Restart restart;
	SMIL_Fill fill;
	SMIL_Duration max;
	SMIL_Duration min;
	struct _smil_timing_rti *runtime; /* contains values for runtime handling of the SMIL timing */
} SMILTimingAttributes;

typedef struct {
	SMIL_SyncBehavior syncBehavior, syncBehaviorDefault;
	SMIL_SyncTolerance syncTolerance, syncToleranceDefault;
	SVG_Boolean syncMaster;
} SMILSyncAttributes;

typedef struct {
	SMIL_AttributeName attributeName; 
	SMIL_AttributeType attributeType;
	SMIL_AnimateValue to, by, from;
	SMIL_AnimateValues values;
	SMIL_CalcMode calcMode;
	SMIL_Accumulate accumulate;
	SMIL_Additive additive;
	SMIL_KeySplines keySplines;
	SMIL_KeyTimes keyTimes;
	SVG_TransformType type;
} SMILAnimationAttributes;

typedef struct {
	SVG_ListOfIRI requiredExtensions;
	SVG_ListOfIRI requiredFeatures;
	SVG_FontList requiredFonts;
	SVG_FormatList requiredFormats;
	SVG_LanguageIDs systemLanguage;
} SVGConditionalAttributes;

#define BASE_SVG_ELEMENT \
	BASE_NODE \
	CHILDREN \
	SVG_String textContent; \
	XMLCoreAttributes *core; \
	SVGProperties *properties; \
	SVGConditionalAttributes *conditional; \
	SVGFocusAttributes *focus; \
	SMILTimingAttributes *timing; \
	SMILAnimationAttributes *anim; \
	SMILSyncAttributes *sync; \
	XLinkAttributes *xlink;

typedef struct _svg_element {
	BASE_SVG_ELEMENT
} SVGElement;

typedef struct _svg_transformable_element {
	BASE_SVG_ELEMENT
	SVG_Matrix transform;
} SVGTransformableElement;

/*************************************
 * Generic SVG element functions     *
 *************************************/

/*the exported functions used by the scene graph*/
u32			gf_svg_get_tag_by_name		(const char *element_name);
u32			gf_svg_get_attribute_count	(GF_Node *);
GF_Err		gf_svg_get_attribute_info	(GF_Node *node, GF_FieldInfo *info);
const char *gf_svg_get_element_name		(u32 tag);

void		gf_svg_element_del			(SVGElement *elt);
void		gf_svg_reset_base_element	(SVGElement *p);

SVGElement *gf_svg_create_node			(u32 ElementTag);
/*shortcut to gf_node_new + gf_node_init*/
SVGElement *gf_svg_new_node				(GF_SceneGraph *inScene, u32 tag);

void gf_svg_init_conditional	(SVGElement *p);
void gf_svg_init_anim			(SVGElement *p);
void gf_svg_init_sync			(SVGElement *p);
void gf_svg_init_timing			(SVGElement *p);
void gf_svg_init_xlink			(SVGElement *p);
void gf_svg_init_focus			(SVGElement *p);
void gf_svg_init_properties		(SVGElement *p);
void gf_svg_init_core			(SVGElement *p);

/* reset functions for SVG types */
void gf_svg_reset_path			(SVG_PathData path);
void gf_svg_reset_iri			(SVGElement *p, SVG_IRI*iri);

/* delete functions for SVG types */
void gf_svg_delete_paint		(SVG_Paint *paint);
void gf_smil_delete_times		(GF_List *l);
void gf_svg_delete_points		(GF_List *l);
void gf_svg_delete_coordinates	(GF_List *l);
/*for keyTimes, keyPoints and keySplines*/
void gf_smil_delete_key_types	(GF_List *l);

void gf_svg_properties_init_pointers(SVGPropertiesPointers *svg_props);
void gf_svg_properties_reset_pointers(SVGPropertiesPointers *svg_props);
void gf_svg_properties_apply(SVGPropertiesPointers *render_svg_props, SVGProperties *current_svg_props);


/* basic DOM event handling
DO NOT CHANGE THEIR POSITION IN THE LIST, USED TO SPEED UP USER INPUT EVENTS
*/

enum {
	/*DOM MouseEvents*/
	SVG_DOM_EVT_CLICK, 
	SVG_DOM_EVT_MOUSEUP, 
	SVG_DOM_EVT_MOUSEDOWN, 
	SVG_DOM_EVT_MOUSEOVER, 
	SVG_DOM_EVT_MOUSEOUT, 
	SVG_DOM_EVT_MOUSEMOVE, 
	/*KEY EVENTS*/
	SVG_DOM_EVT_KEYUP,
	SVG_DOM_EVT_KEYDOWN,
	SVG_DOM_EVT_KEYPRESS,
	SVG_DOM_EVT_LONGKEYPRESS,
	/*DOM UIEvents*/
	SVG_DOM_EVT_FOCUSIN,
	SVG_DOM_EVT_FOCUSOUT, 
	SVG_DOM_EVT_ACTIVATE, 
	/*SVG (HTML) Events*/
	SVG_DOM_EVT_LOAD, 
	SVG_DOM_EVT_UNLOAD,
	SVG_DOM_EVT_ABORT, 
	SVG_DOM_EVT_ERROR, 
	SVG_DOM_EVT_RESIZE, 
	SVG_DOM_EVT_SCROLL, 
	SVG_DOM_EVT_ZOOM,
	SVG_DOM_EVT_BEGIN,
	SVG_DOM_EVT_END,
	SVG_DOM_EVT_REPEAT,

	SVG_DOM_EVT_TEXTINPUT,
	
	/*DOM MutationEvents - NOT SUPPORTED YET*/
	SVG_DOM_EVT_TREE_MODIFIED,
	SVG_DOM_EVT_NODE_INSERTED,
	SVG_DOM_EVT_NODE_REMOVED,
	SVG_DOM_EVT_NODE_INSERTED_DOC,
	SVG_DOM_EVT_NODE_REMOVED_DOC,
	SVG_DOM_EVT_ATTR_MODIFIED,
	SVG_DOM_EVT_CHAR_DATA_MODIFIED,

	SVG_DOM_EVT_UNKNOWN,
};

u32 svg_dom_event_by_name(const char *name);
const char *gf_dom_event_name(u32 type);


typedef struct
{
	/*event type*/
	u32 type;
	/*event phase type, READ-ONLY
	0: at target, 1: bubbling, 2: capturing , 3: canceled
	*/
	u8 event_phase;
	u8 bubbles;
	u8 cancelable;
	/*output only - indicates UI events (mouse) have been detected*/
	u8 has_ui_events;
	GF_Node *target;
	GF_Node *currentTarget;
	Double timestamp;
	/*UIEvent extension. For mouse extensions, stores button type. For key event, the key code*/
	u32 detail;
	/*MouseEvent extension*/
	s32 screenX, screenY;
	s32 clientX, clientY;
	Bool ctrl_key, shift_key, alt_key, meta_key;
	GF_Node *relatedTarget;
	/*Zoom event*/
	GF_Rect screen_rect;
	GF_Point2D prev_translate, new_translate;
	Fixed prev_scale, new_scale;
} GF_DOM_Event;

Bool gf_sg_fire_dom_event(GF_Node *node, GF_DOM_Event *event);

/*listener are simply nodes added to the node events list. 
THIS SHALL NOT BE USED WITH VRML-BASED GRAPHS: either one uses listeners or one uses routes
the listener node is NOT registered, it is the user responsability to delete it from its parent
@listener is a listenerElement (XML event)
*/
GF_Err gf_node_listener_add(GF_Node *node, GF_Node *listener);
GF_Err gf_node_listener_del(GF_Node *node, GF_Node *listener);
u32 gf_node_listener_count(GF_Node *node);
GF_Node *gf_node_listener_get(GF_Node *node, u32 i);

/* animations */
GF_Err gf_node_animation_add(GF_Node *node, void *animation);
GF_Err gf_node_animation_del(GF_Node *node);
u32 gf_node_animation_count(GF_Node *node);
void *gf_node_animation_get(GF_Node *node, u32 i);

void *svg_create_value_from_attributetype(u32 attribute_type, u8 transform_type);
GF_Err svg_parse_attribute(SVGElement *elt, GF_FieldInfo *info, char *attribute_content, u8 anim_value_type, u8 transform_type);
void smil_parse_attributename(SVGElement *animation_element, char *value_string);
void svg_parse_style(SVGElement *elt, char *style);
GF_Err svg_dump_attribute(SVGElement *elt, GF_FieldInfo *info, char *attValue);
Bool svg_store_embedded_data(SVG_IRI *iri, const char *iri_data, const char *cache_dir, const char *base_filename);

/* a == b */
Bool svg_attributes_equal(GF_FieldInfo *a, GF_FieldInfo *b);

/* c = coef * a + (1 - coef) * b */
GF_Err svg_attributes_interpolate(GF_FieldInfo *a, GF_FieldInfo *b, GF_FieldInfo *c, Fixed coef, Bool clamp);

/* a = b */
GF_Err svg_attributes_copy(GF_FieldInfo *a, GF_FieldInfo *b, Bool clamp);

/* c = alpha * a + beta * b */
GF_Err svg_attributes_muladd(Fixed alpha, GF_FieldInfo *a, Fixed beta, GF_FieldInfo *b, GF_FieldInfo *c, Bool clamp);

/* c = a + b */
GF_Err svg_attributes_add(GF_FieldInfo *a, GF_FieldInfo *b, GF_FieldInfo *c, Bool clamp);

Bool gf_svg_is_property_inherited(GF_FieldInfo *a);
Bool gf_svg_is_current_color(GF_FieldInfo *a);
void *gf_svg_get_property_pointer(SVGPropertiesPointers *p, SVGElement *elt, void *orig_prop_ptr);
void gf_svg_attributes_copy_computed_value(GF_FieldInfo *out, GF_FieldInfo *in, SVGElement*elt, void *orig_dom_ptr, SVGPropertiesPointers *inherited_props);
void gf_svg_attributes_smart_copy(GF_FieldInfo *out, GF_FieldInfo *in, GF_FieldInfo *prop, GF_FieldInfo *current_color);
void gf_svg_attributes_pointer_update(GF_FieldInfo *a, GF_FieldInfo *prop, GF_FieldInfo *current_color);

void gf_svg_delete_attribute_value(u32 type, void *value);

/*creates a default listener/handler for the given event on the given node, and return the 
handler element to allow for handler function override*/
struct _tagSVGhandlerElement *gf_sg_dom_create_listener(GF_Node *node, u32 eventType);



/* SMIL Timing structures */
/* status of an SMIL timed element */ 
enum {
	SMIL_STATUS_STARTUP = 0,
	SMIL_STATUS_WAITING_TO_BEGIN,
	SMIL_STATUS_ACTIVE,
	SMIL_STATUS_END_INTERVAL,
	SMIL_STATUS_POST_ACTIVE,
	SMIL_STATUS_FROZEN,
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

typedef struct _smil_timing_rti
{
	SVGElement *timed_elt;

	/* SMIL element life-cycle status */
	u8 status;
	u32 cycle_number;
	u32 first_frozen;

	/* List of possible intervals for activation of the element */
	GF_List *intervals;
	s32	current_interval_index;
	SMIL_Interval *current_interval;

	/* is called only when the timed element is active */
	void (*activation)(struct _smil_timing_rti *rti, Fixed normalized_simple_time);

	/* is called (possibly many times) when the timed element is frozen */
	void (*freeze)(struct _smil_timing_rti *rti, Fixed normalized_simple_time);

	/* is called (only once) when the timed element is restored */
	void (*restore)(struct _smil_timing_rti *rti, Fixed normalized_simple_time);

} SMIL_Timing_RTI;

void gf_smil_timing_init_runtime_info(SVGElement *timed_elt);
void gf_smil_timing_delete_runtime_info(SVGElement *timed_elt);
Fixed gf_smil_timing_get_normalized_simple_time(SMIL_Timing_RTI *rti, Double scene_time);
void gf_smil_timing_notify_time(SMIL_Timing_RTI *rti, Double scene_time);

/* SMIL Animation Structures */
/* This structure is used per animated attribute,
   it contains all the animations applying to the same attribute,
   and the presentation value passed from one animation to the next one */
typedef struct {
	GF_FieldInfo presentation_value;
	GF_FieldInfo saved_dom_value;
	/*original location of the DOM attribute in the elt structure used for fast comparison of SVG 
	properties when animating from/to/by/values/... inherited values*/
	void *orig_dom_ptr;
	GF_FieldInfo current_color_value;
	GF_List *anims;
} SMIL_AttributeAnimations;

/* This structure is per animation element, 
   it holds the result of the animation and 
   some info to make animation computation faster */
typedef struct {
	SMIL_AttributeAnimations *owner;

	/* animation element */
	SVGElement *anim_elt;

	/* result of the animation */
	GF_FieldInfo interpolated_value;

	/* last value of the animation, used in accumulation phase */
	GF_FieldInfo last_specified_value;

	/* temporary value needed when the type of 
	   the key values is different from the target attribute type */
	GF_FieldInfo tmp_value;

	s32 previous_key_index;
	Fixed previous_coef;
	u32 last_keytime_index;

	/* needed ? */
	Bool target_value_changed;

	GF_Path *path;
	u8 rotate;
	GF_PathIterator *path_iterator;
	Fixed length;

} SMIL_Anim_RTI;

void gf_smil_anim_init_node(GF_Node *node);
void gf_smil_anim_init_runtime_info(SVGElement *e);
void gf_smil_anim_delete_runtime_info(SMIL_Anim_RTI *rai);
void gf_smil_anim_delete_animations(SVGElement *e);

void gf_path_init_from_svg(GF_Path *path, GF_List *commands, GF_List *points);

void gf_svg_register_iri(GF_SceneGraph *sg, SVG_IRI *iri);

#ifdef __cplusplus
}
#endif

#endif	//_GF_SG_SVG_H_

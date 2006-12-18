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

	/* keyword enum types */
	SVG_FillRule_datatype					= 1,
//	SVG_Clip_datatype = SVG_FillRule_datatype,
	SVG_StrokeLineJoin_datatype				= 2,
	SVG_StrokeLineCap_datatype				= 3,
	SVG_FontStyle_datatype					= 4,
	SVG_FontWeight_datatype					= 5,
	SVG_FontVariant_datatype				= 6,
	SVG_TextAnchor_datatype					= 7,
	SVG_TransformType_datatype				= 8, 
	SVG_Display_datatype					= 9, 
	SVG_Visibility_datatype					= 10,
	SVG_Overflow_datatype					= 11,
	SVG_ZoomAndPan_datatype					= 12,
	SVG_DisplayAlign_datatype				= 13,
	SVG_PointerEvents_datatype				= 14,
	SVG_RenderingHint_datatype				= 15,
	SVG_VectorEffect_datatype				= 16,
	SVG_PlaybackOrder_datatype				= 17,
	SVG_TimelineBegin_datatype				= 18,
	XML_Space_datatype						= 19,
	XMLEV_Propagate_datatype				= 20,
	XMLEV_DefaultAction_datatype			= 21,
	XMLEV_Phase_datatype					= 22,
	SMIL_SyncBehavior_datatype				= 23,
	SMIL_SyncTolerance_datatype				= 24,
	SMIL_AttributeType_datatype				= 25,
	SMIL_CalcMode_datatype					= 26,
	SMIL_Additive_datatype					= 27,
	SMIL_Accumulate_datatype				= 28,
	SMIL_Restart_datatype					= 29,
	SMIL_Fill_datatype						= 30,	
	SVG_GradientUnit_datatype				= 31,
	SVG_InitialVisibility_datatype			= 32,
	SVG_FocusHighlight_datatype				= 33,
	SVG_Overlay_datatype					= 34,
	SVG_TransformBehavior_datatype			= 35,
	SVG_SpreadMethod_datatype				= 36,
	SVG_TextAlign_datatype					= 37,

	/* SVG Number */
	SVG_Number_datatype						= 50,
//	SVG_NumberOrPercentage_datatype			= 51,
//	SVG_Opacity_datatype					= 52,
//	SVG_StrokeMiterLimit_datatype			= 53,
	SVG_FontSize_datatype					= 54,
//	SVG_StrokeDashOffset_datatype			= 55,
//	SVG_AudioLevel_datatype					= 56,
//	SVG_LineIncrement_datatype				= 57,
//	SVG_StrokeWidth_datatype				= 58,
	SVG_Length_datatype						= 59,
	SVG_Coordinate_datatype					= 60,
	SVG_Rotate_datatype						= 61,

	/* List of */
	SVG_Numbers_datatype					= 70,
	SVG_Points_datatype						= 71,
	SVG_Coordinates_datatype				= 72,
	SVG_FeatureList_datatype				= 73,
	SVG_ExtensionList_datatype				= 74,
	SVG_FormatList_datatype					= 75,
	SVG_FontList_datatype					= 76,
	SVG_ListOfIRI_datatype					= 77,
	SVG_LanguageIDs_datatype				= 78,
	SMIL_KeyTimes_datatype					= 79,
	SMIL_KeySplines_datatype				= 80,
	SMIL_KeyPoints_datatype					= 81,
	SMIL_Times_datatype						= 82,

	/* animated (untyped) value */
	SMIL_AnimateValue_datatype				= 85,
	SMIL_AnimateValues_datatype				= 86,
	
	SMIL_Duration_datatype					= 87,
	SMIL_RepeatCount_datatype				= 88,
	SMIL_AttributeName_datatype				= 89,

	/*all other types*/
	SVG_Boolean_datatype					= 90,
	SVG_Color_datatype						= 91,
	SVG_Paint_datatype						= 92,
	SVG_SVGColor_datatype = SVG_Paint_datatype,
	SVG_PathData_datatype					= 93,
	SVG_FontFamily_datatype					= 94,
	SVG_ID_datatype							= 95,
	SVG_IRI_datatype						= 96,
	SVG_Matrix_datatype						= 97,
	SVG_Motion_datatype						= 98, /* required for animateMotion */
	SVG_StrokeDashArray_datatype			= 99,
	SVG_PreserveAspectRatio_datatype		= 100,
	SVG_ViewBox_datatype					= 101,
	SVG_GradientOffset_datatype				= 102,
	SVG_Focus_datatype						= 103,
	SVG_Clock_datatype						= 104,
	SVG_String_datatype						= 105,
	SVG_ContentType_datatype				= 106,
	SVG_LanguageID_datatype					= 107,
	XMLEV_Event_datatype					= 130,

	/*LASeR types*/
	LASeR_Choice_datatype					= 135,
	LASeR_Size_datatype						= 136,
	LASeR_TimeAttribute_datatype			= 137
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
typedef char *DOM_String;
typedef DOM_String SVG_String;
typedef DOM_String SVG_ContentType;
typedef DOM_String SVG_LanguageID;
typedef DOM_String SVG_TextContent;

/* types not yet handled properly, i.e. for the moment using strings */
typedef DOM_String SVG_ID;
typedef DOM_String SVG_LinkTarget;
typedef DOM_String SVG_GradientOffset;

typedef Double SVG_Clock;

typedef GF_List *SVG_Numbers;
typedef GF_List *SVG_Coordinates;
typedef GF_List	*SVG_FeatureList;
typedef GF_List	*SVG_ExtensionList;
typedef GF_List	*SVG_FormatList;
typedef GF_List	*SVG_ListOfIRI;
typedef GF_List	*SVG_LanguageIDs;
typedef GF_List	*SVG_FontList;
typedef GF_List *SVG_TransformList;
typedef GF_List *SVG_Points;
typedef GF_List *SMIL_Times;
typedef GF_List *SMIL_KeyTimes;
typedef GF_List *SMIL_KeyPoints;
/* Fixed between 0 and 1 */
typedef GF_List *SMIL_KeySplines;

/* SMIL Anim types */
typedef struct {
	char *name;
	u32 type;
	/* used in case type is SVG_Matrix_datatype, 
	we need to precise the transformation type (see SVG_TRANSFORM_TRANSLATE...) */
	u32 transform_type;

	void *field_ptr;
} SMIL_AttributeName;

enum {
	/*unspecified time*/
	GF_SMIL_TIME_UNSPECIFIED   = 0,
	/*clock time*/
	GF_SMIL_TIME_CLOCK			= 1,
	/*wallclock time*/
	GF_SMIL_TIME_WALLCLOCK		= 2,
	/*resolved time of an event, discarded when restarting animation.*/
	GF_SMIL_TIME_EVENT_RESLOVED	= 3,
	/*event time*/
	GF_SMIL_TIME_EVENT			= 4,
	/*indefinite time*/
	GF_SMIL_TIME_INDEFINITE		= 5
};

#define GF_SMIL_TIME_IS_CLOCK(v) (v<=GF_SMIL_TIME_EVENT_RESLOVED)
#define GF_SMIL_TIME_IS_SPECIFIED_CLOCK(v) ((v) && (v<=GF_SMIL_TIME_EVENT_RESLOVED) )

typedef struct
{
	u32 type;
	/*for accessKey and mouse button, or repeatCount when the event is a SMIL repeat */
	u32 parameter;
} XMLEV_Event;

typedef struct {
	/* Type of timing value*/
	u8 type;
	/* in case of syncbase, event, repeat value: this is the pointer to the source of the event */
	GF_Node *element; 
	/* id of the element before resolution of the pointer to the element */
	char *element_id; 

	/* event type and parameter */
	XMLEV_Event event; 
	/*clock offset (absolute or relative to event)*/
	Double clock;
} SMIL_Time;

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
	SVG_IRI_ELEMENTID = 0,
	SVG_IRI_IRI = 1
};
typedef struct {
	u8 type;
	char *iri;
	struct _svg_element *target;
} SVG_IRI;

enum
{
	SVG_FOCUS_SELF = 0, 
	SVG_FOCUS_AUTO, 
	SVG_FOCUS_IRI, 
};

typedef struct
{
	u8 type;
	SVG_IRI target;
} SVG_Focus;

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
	SVG_FONTSTYLE_ITALIC = 1,  
	SVG_FONTSTYLE_NORMAL = 2,
	SVG_FONTSTYLE_OBLIQUE = 3
}; 
typedef u8 SVG_FontStyle;

/*the values are chosen to match LASeR code points*/
enum {
	SVG_PATHCOMMAND_M = 3,
	SVG_PATHCOMMAND_L = 2,
	SVG_PATHCOMMAND_C = 0,
	SVG_PATHCOMMAND_S = 5,
	SVG_PATHCOMMAND_Q = 4,
	SVG_PATHCOMMAND_T = 6,
	SVG_PATHCOMMAND_A = 20,
	SVG_PATHCOMMAND_Z = 8
};

#define USE_GF_PATH 1

#if USE_GF_PATH
typedef GF_Path SVG_PathData;
#else
typedef struct {
	GF_List *commands;
	GF_List *points;
} SVG_PathData;
#endif

typedef struct {
	Fixed x, y;
} SVG_Point;

typedef struct {
	Fixed x, y, angle;
} SVG_Point_Angle;

typedef struct {
	Bool is_set;
	Fixed x, y, width, height;
} SVG_ViewBox;

typedef Bool SVG_Boolean;

/*WARNING - THESE ARE PATH FLAGS, CHECK IF WORKING*/
enum {
	SVG_FILLRULE_EVENODD= 0,
	SVG_FILLRULE_NONZERO,
	SVG_FILLRULE_INHERIT
};
typedef u8 SVG_FillRule;
	
enum {
	SVG_STROKELINEJOIN_MITER = GF_LINE_JOIN_MITER_SVG,
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
	SVG_COLOR_RGBCOLOR = 0,
	SVG_COLOR_INHERIT,
	SVG_COLOR_CURRENTCOLOR,
	SVG_COLOR_ACTIVE_BORDER, /*Active window border*/
	SVG_COLOR_ACTIVE_CAPTION, /*Active window caption. */
	SVG_COLOR_APP_WORKSPACE, /*Background color of multiple document interface. */
	SVG_COLOR_BACKGROUND, /*Desktop background. */
	SVG_COLOR_BUTTON_FACE, /* Face color for three-dimensional display elements. */
	SVG_COLOR_BUTTON_HIGHLIGHT, /* Dark shadow for three-dimensional display elements (for edges facing away from the light source). */
	SVG_COLOR_BUTTON_SHADOW, /* Shadow color for three-dimensional display elements. */
	SVG_COLOR_BUTTON_TEXT, /*Text on push buttons. */
	SVG_COLOR_CAPTION_TEXT, /* Text in caption, size box, and scrollbar arrow box. */
	SVG_COLOR_GRAY_TEXT, /* Disabled ('grayed') text. */
	SVG_COLOR_HIGHLIGHT, /* Item(s) selected in a control. */
	SVG_COLOR_HIGHLIGHT_TEXT, /*Text of item(s) selected in a control. */
	SVG_COLOR_INACTIVE_BORDER, /* Inactive window border. */
	SVG_COLOR_INACTIVE_CAPTION, /* Inactive window caption. */
	SVG_COLOR_INACTIVE_CAPTION_TEXT, /*Color of text in an inactive caption. */
	SVG_COLOR_INFO_BACKGROUND, /* Background color for tooltip controls. */
	SVG_COLOR_INFO_TEXT,  /*Text color for tooltip controls. */
	SVG_COLOR_MENU, /*Menu background. */
	SVG_COLOR_MENU_TEXT, /* Text in menus. */
	SVG_COLOR_SCROLLBAR, /* Scroll bar gray area. */
	SVG_COLOR_3D_DARK_SHADOW, /* Dark shadow for three-dimensional display elements. */
	SVG_COLOR_3D_FACE, /* Face color for three-dimensional display elements. */
	SVG_COLOR_3D_HIGHLIGHT, /* Highlight color for three-dimensional display elements. */
	SVG_COLOR_3D_LIGHT_SHADOW, /* Light color for three-dimensional display elements (for edges facing the light source). */
	SVG_COLOR_3D_SHADOW, /* Dark shadow for three-dimensional display elements. */
	SVG_COLOR_WINDOW, /* Window background. */
	SVG_COLOR_WINDOW_FRAME, /* Window frame. */
	SVG_COLOR_WINDOW_TEXT /* Text in windows.*/
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
	SVG_IRI iri;
} SVG_Paint, SVG_SVGColor;

enum {
	SVG_NUMBER_VALUE		= 0,
	SVG_NUMBER_PERCENTAGE	= 1,
	SVG_NUMBER_EMS			= 2,
	SVG_NUMBER_EXS			= 3,
	SVG_NUMBER_PX			= 4,
	SVG_NUMBER_CM			= 5,
	SVG_NUMBER_MM			= 6,
	SVG_NUMBER_IN			= 7,
	SVG_NUMBER_PT			= 8,
	SVG_NUMBER_PC			= 9,
	SVG_NUMBER_INHERIT		= 10,
	SVG_NUMBER_AUTO			= 11,
	SVG_NUMBER_AUTO_REVERSE	= 12
};

typedef struct {
	u8 type;
	Fixed value;
} SVG_Number, 
  SVG_FontSize, 
  SVG_Length, 
  SVG_Coordinate, 
  SVG_Rotate;

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

enum {
	SVG_FONTWEIGHT_100 = 0, 
	SVG_FONTWEIGHT_200,
	SVG_FONTWEIGHT_300, 
	SVG_FONTWEIGHT_400,
	SVG_FONTWEIGHT_500,
	SVG_FONTWEIGHT_600,
	SVG_FONTWEIGHT_700,
	SVG_FONTWEIGHT_800,
	SVG_FONTWEIGHT_900,
	SVG_FONTWEIGHT_BOLD, 
	SVG_FONTWEIGHT_BOLDER, 
	SVG_FONTWEIGHT_INHERIT,
	SVG_FONTWEIGHT_LIGHTER, 
	SVG_FONTWEIGHT_NORMAL
};
typedef u8 SVG_FontWeight;

enum {
	SVG_FONTVARIANT_INHERIT		= 0,
	SVG_FONTVARIANT_NORMAL		= 1,
	SVG_FONTVARIANT_SMALLCAPS	= 2
};
typedef u8 SVG_FontVariant;

enum {
	SVG_VISIBILITY_HIDDEN   = 0,
	SVG_VISIBILITY_INHERIT	= 1,
	SVG_VISIBILITY_VISIBLE  = 2,
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
	SVG_TEXTALIGN_INHERIT	= 0,
	SVG_TEXTALIGN_START		= 1,
	SVG_TEXTALIGN_CENTER	= 2,
	SVG_TEXTALIGN_END		= 3
};
typedef u8 SVG_TextAlign;

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
	SVG_TEXTANCHOR_END,
	SVG_TEXTANCHOR_INHERIT,
	SVG_TEXTANCHOR_MIDDLE,
	SVG_TEXTANCHOR_START
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
	XMLEVENT_DEFAULTACTION_PERFORM	= 0,
	XMLEVENT_DEFAULTACTION_CANCEL
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
	SVG_OVERLAY_TOP,
	SVG_OVERLAY_FULLSCREEN,
} SVG_Overlay;


enum {
	SVG_SPREAD_PAD = 0,
	SVG_SPREAD_REFLECT,
	SVG_SPREAD_REPEAT,
};
typedef u8 SVG_SpreadMethod;

typedef enum {
	LASeR_CHOICE_ALL   = 0,
	LASeR_CHOICE_NONE  = 1,
	LASeR_CHOICE_N	   = 2
} LASeR_Choice_enum;

typedef struct {
	u32 type;
	u32 choice_index;
} LASeR_Choice;

typedef struct {
	Bool enabled;
	Fixed width, height;
} LASeR_Size;

enum {
	LASeR_TIMEATTRIBUTE_BEGIN = 0,
	LASeR_TIMEATTRIBUTE_END
};
typedef u8 LASeR_TimeAttribute;

/**************************************************
 *  SVG's styling properties (see 6.1 in REC 1.1) *
 *************************************************/

typedef struct {
	/* Tiny 1.2 properties*/
	SVG_Paint					*color;
	SVG_Paint					*fill; 
	SVG_Paint					*stroke;
	SVG_Paint					*solid_color;
	SVG_Paint					*stop_color;
	SVG_Paint					*viewport_fill;

	SVG_Number					*fill_opacity;
	SVG_Number					*solid_opacity;
	SVG_Number					*stop_opacity;
	SVG_Number					*stroke_opacity;
	SVG_Number					*viewport_fill_opacity;
	SVG_Number					*opacity; /* Restricted property in Tiny 1.2 */

	SVG_Number					*audio_level;

	SVG_RenderingHint			*color_rendering;
	SVG_RenderingHint			*image_rendering;
	SVG_RenderingHint			*shape_rendering;
	SVG_RenderingHint			*text_rendering;

	SVG_Display					*display; 
	SVG_Visibility				*visibility;
	SVG_Overflow				*overflow; /* Restricted property in Tiny 1.2 */
	
	SVG_FontFamily				*font_family;
	SVG_FontSize				*font_size;
	SVG_FontStyle				*font_style; 
	SVG_FontWeight				*font_weight; 
	SVG_FontVariant				*font_variant; 
	SVG_Number					*line_increment;	
	SVG_TextAnchor				*text_anchor;
	SVG_DisplayAlign			*display_align;
	SVG_TextAlign				*text_align;

	SVG_PointerEvents			*pointer_events;
	
	SVG_FillRule				*fill_rule; 
	
	SVG_StrokeDashArray			*stroke_dasharray;
	SVG_Length					*stroke_dashoffset;
	SVG_StrokeLineCap			*stroke_linecap; 
	SVG_StrokeLineJoin			*stroke_linejoin; 
	SVG_Number					*stroke_miterlimit; 
	SVG_Length					*stroke_width;
	SVG_VectorEffect			*vector_effect;
	
	/* Full 1.1 props, i.e. not implemented */
/*
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

	SVG_Number					fill_opacity;
	SVG_Number					solid_opacity;
	SVG_Number					stop_opacity;
	SVG_Number					stroke_opacity;
	SVG_Number					viewport_fill_opacity;
	SVG_Number					opacity; /* Restricted property in Tiny 1.2 */

	SVG_Number  				audio_level;

	SVG_RenderingHint			color_rendering;
	SVG_RenderingHint			image_rendering;
	SVG_RenderingHint			shape_rendering;
	SVG_RenderingHint			text_rendering;

	SVG_Display					display; 
	SVG_Visibility				visibility;
	SVG_Overflow				overflow; /* Restricted property in Tiny 1.2 */

	SVG_FontFamily				font_family;
	SVG_FontSize				font_size;
	SVG_FontStyle				font_style; 
	SVG_FontWeight				font_weight; 
	SVG_FontVariant				font_variant; 
	SVG_Number					line_increment;
	SVG_TextAnchor				text_anchor;
	SVG_DisplayAlign			display_align;
	SVG_TextAlign				text_align;

	SVG_PointerEvents			pointer_events;

	SVG_FillRule				fill_rule; 

	SVG_StrokeDashArray			stroke_dasharray;
	SVG_Length					stroke_dashoffset;
	SVG_StrokeLineCap			stroke_linecap; 
	SVG_StrokeLineJoin			stroke_linejoin; 
	SVG_Number					stroke_miterlimit; 
	SVG_Length					stroke_width;
	SVG_VectorEffect			vector_effect;	

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
	SVG_Clock clipBegin, clipEnd;
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
	SVG_String syncReference;
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
	SVG_Boolean lsr_enabled;
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

#define TRANSFORMABLE_SVG_ELEMENT	\
	BASE_SVG_ELEMENT	\
	Bool is_ref_transform;	\
	SVG_Matrix transform;	\
	SVG_Matrix *motionTransform;


typedef struct _svg_transformable_element {
	TRANSFORMABLE_SVG_ELEMENT
} SVGTransformableElement;

typedef struct
{
	char *data;
	u32 data_size;
	GF_List *com_list;
	void (*exec_command_list)(void *script);
} SVGCommandBuffer;

/*************************************
 * Generic SVG element functions     *
 *************************************/

/*the exported functions used by the scene graph*/
u32 gf_node_svg_type_by_class_name(const char *element_name);

void gf_svg_properties_init_pointers(SVGPropertiesPointers *svg_props);
void gf_svg_properties_reset_pointers(SVGPropertiesPointers *svg_props);

void gf_svg_apply_inheritance(SVGElement *elt, SVGPropertiesPointers *render_svg_props);
void gf_svg_apply_animations(GF_Node *node, SVGPropertiesPointers *render_svg_props);

Bool is_svg_animation_tag(u32 tag);
Bool gf_svg_is_element_transformable(u32 tag);

void *gf_svg_create_attribute_value(u32 attribute_type, u8 transform_type);
void gf_svg_delete_attribute_value(u32 type, void *value, GF_SceneGraph *sg);
/* a == b */
Bool gf_svg_attributes_equal(GF_FieldInfo *a, GF_FieldInfo *b);
/* a = b */
GF_Err gf_svg_attributes_copy(GF_FieldInfo *a, GF_FieldInfo *b, Bool clamp);
/* c = a + b */
GF_Err gf_svg_attributes_add(GF_FieldInfo *a, GF_FieldInfo *b, GF_FieldInfo *c, Bool clamp);
Bool gf_svg_attribute_is_interpolatable(u32 type) ;
/* c = coef * a + (1 - coef) * b */
GF_Err gf_svg_attributes_interpolate(GF_FieldInfo *a, GF_FieldInfo *b, GF_FieldInfo *c, Fixed coef, Bool clamp);
/* c = alpha * a + beta * b */
GF_Err gf_svg_attributes_muladd(Fixed alpha, GF_FieldInfo *a, Fixed beta, GF_FieldInfo *b, GF_FieldInfo *c, Bool clamp);



GF_Err gf_svg_parse_attribute(SVGElement *elt, GF_FieldInfo *info, char *attribute_content, u8 anim_value_type, u8 transform_type);
void gf_svg_parse_style(SVGElement *elt, char *style);
GF_Err gf_svg_dump_attribute(SVGElement *elt, GF_FieldInfo *info, char *attValue);
GF_Err gf_svg_dump_attribute_indexed(SVGElement *elt, GF_FieldInfo *info, char *attValue);

Bool gf_svg_store_embedded_data(SVG_IRI *iri, const char *cache_dir, const char *base_filename);
void gf_svg_path_build(GF_Path *path, GF_List *commands, GF_List *points);
void gf_svg_register_iri(GF_SceneGraph *sg, SVG_IRI *iri);
void gf_svg_unregister_iri(GF_SceneGraph *sg, SVG_IRI *iri);

GF_Err gf_svg_parse_element_id(SVGElement *elt, const char *nodename, Bool warning_if_defined);


/* 
	basic DOM event handling
DO NOT CHANGE THEIR POSITION IN THE LIST, USED TO SPEED UP USER INPUT EVENTS
*/

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
	/*key flags*/
	u32 key_flags;
	/*key hardware code*/
	u32 key_hw_code;
	GF_Node *relatedTarget;
	/*Zoom event*/
	GF_Rect screen_rect;
	GF_Point2D prev_translate, new_translate;
	Fixed prev_scale, new_scale;
	/* CPU */
	u32 cpu_percentage;
	/* Battery */
	Bool onBattery;
	u32 batteryState, batteryLevel;
} GF_DOM_Event;


u32 gf_dom_event_type_by_name(const char *name);
const char *gf_dom_event_get_name(u32 type);
Bool gf_dom_event_fire(GF_Node *node, GF_Node *parent_use, GF_DOM_Event *event);

/*listener are simply nodes added to the node events list. 
THIS SHALL NOT BE USED WITH VRML-BASED GRAPHS: either one uses listeners or one uses routes
the listener node is NOT registered, it is the user responsability to delete it from its parent
@listener is a listenerElement (XML event)
*/
GF_Err gf_dom_listener_add(GF_Node *node, GF_Node *listener);
GF_Err gf_dom_listener_del(GF_Node *node, GF_Node *listener);
u32 gf_dom_listener_count(GF_Node *node);
struct _tagSVGlistenerElement *gf_dom_listener_get(GF_Node *node, u32 i);

/*creates a default listener/handler for the given event on the given node, and return the 
handler element to allow for handler function override*/
struct _tagSVGhandlerElement *gf_dom_listener_build(GF_Node *node, XMLEV_Event event);

Bool gf_sg_notify_smil_timed_elements(GF_SceneGraph *sg);

const char *gf_dom_get_key_name(u32 key_identifier);

#ifdef __cplusplus
}
#endif

#endif	//_GF_SG_SVG_H_

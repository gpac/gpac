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
	SVG_Clip_datatype = SVG_FillRule_datatype,
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

	/* SVG Number */
	SVG_Number_datatype						= 50,
	SVG_NumberOrPercentage_datatype			= 51,
	SVG_Opacity_datatype					= 52,
	SVG_StrokeMiterLimit_datatype			= 53,
	SVG_FontSize_datatype					= 54,
	SVG_StrokeDashOffset_datatype			= 55,
	SVG_AudioLevel_datatype					= 56,
	SVG_LineIncrement_datatype				= 57,
	SVG_StrokeWidth_datatype				= 58,
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
typedef u8 *DOM_String;
typedef DOM_String SVG_String;
typedef DOM_String SVG_ContentType;
typedef DOM_String SVG_LanguageID;
typedef DOM_String SVG_TextContent;

/* types not yet handled properly, i.e. for the moment using strings */
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
	SVG_FONTSTYLE_NORMAL = 1,
	SVG_FONTSTYLE_ITALIC = 2,  
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
typedef u8 SVG_FillRule, SVG_Clip;
	
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
	DOM_String uri;
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
	LASeR_CHOICE_CLIP  = 1,
	LASeR_CHOICE_DELTA = 2,
	LASeR_CHOICE_NONE  = 3,
	LASeR_CHOICE_N	   = 4
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

	SVG_Opacity					*fill_opacity;
	SVG_Opacity					*solid_opacity;
	SVG_Opacity					*stop_opacity;
	SVG_Opacity					*stroke_opacity;
	SVG_Opacity					*viewport_fill_opacity;
	SVG_Opacity					*opacity; /* Restricted property in Tiny 1.2 */

	SVG_AudioLevel				*audio_level;

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
	SVG_LineIncrement			*line_increment;	
	SVG_TextAnchor				*text_anchor;
	SVG_DisplayAlign			*display_align;
	
	SVG_PointerEvents			*pointer_events;
	
	SVG_FillRule				*fill_rule; 
	
	SVG_StrokeDashArray			*stroke_dasharray;
	SVG_StrokeDashOffset		*stroke_dashoffset;
	SVG_StrokeLineCap			*stroke_linecap; 
	SVG_StrokeLineJoin			*stroke_linejoin; 
	SVG_StrokeMiterLimit		*stroke_miterlimit; 
	SVG_StrokeWidth				*stroke_width;
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

	SVG_Opacity					fill_opacity;
	SVG_Opacity					solid_opacity;
	SVG_Opacity					stop_opacity;
	SVG_Opacity					stroke_opacity;
	SVG_Opacity					viewport_fill_opacity;
	SVG_Opacity					opacity; /* Restricted property in Tiny 1.2 */

	SVG_AudioLevel				audio_level;

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
	SVG_LineIncrement			line_increment;
	SVG_TextAnchor				text_anchor;
	SVG_DisplayAlign			display_align;

	SVG_PointerEvents			pointer_events;

	SVG_FillRule				fill_rule; 

	SVG_StrokeDashArray			stroke_dasharray;
	SVG_StrokeDashOffset		stroke_dashoffset;
	SVG_StrokeLineCap			stroke_linecap; 
	SVG_StrokeLineJoin			stroke_linejoin; 
	SVG_StrokeMiterLimit		stroke_miterlimit; 
	SVG_StrokeWidth				stroke_width;
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

typedef struct _svg_transformable_element {
	BASE_SVG_ELEMENT
	SVG_Matrix transform;
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

/* Updates the passed SVG Styling Properties Pointers with the properties of the given SVG element:
	1- applies inheritance whenever needed.
	2- applies any running animation on the element
*/
void gf_svg_apply_inheritance_and_animation(GF_Node *node, SVGPropertiesPointers *render_svg_props);


void *gf_svg_create_attribute_value(u32 attribute_type, u8 transform_type);
void gf_svg_delete_attribute_value(u32 type, void *value, GF_SceneGraph *sg);
/* a == b */
Bool gf_svg_attributes_equal(GF_FieldInfo *a, GF_FieldInfo *b);
/* a = b */
GF_Err gf_svg_attributes_copy(GF_FieldInfo *a, GF_FieldInfo *b, Bool clamp);
/* c = a + b */
GF_Err gf_svg_attributes_add(GF_FieldInfo *a, GF_FieldInfo *b, GF_FieldInfo *c, Bool clamp);
/* c = coef * a + (1 - coef) * b */
GF_Err gf_svg_attributes_interpolate(GF_FieldInfo *a, GF_FieldInfo *b, GF_FieldInfo *c, Fixed coef, Bool clamp);
/* c = alpha * a + beta * b */
GF_Err gf_svg_attributes_muladd(Fixed alpha, GF_FieldInfo *a, Fixed beta, GF_FieldInfo *b, GF_FieldInfo *c, Bool clamp);



GF_Err gf_svg_parse_attribute(SVGElement *elt, GF_FieldInfo *info, char *attribute_content, u8 anim_value_type, u8 transform_type);
void gf_svg_parse_style(SVGElement *elt, char *style);
GF_Err gf_svg_dump_attribute(SVGElement *elt, GF_FieldInfo *info, char *attValue);
Bool gf_svg_store_embedded_data(SVG_IRI *iri, const char *cache_dir, const char *base_filename);
void gf_svg_path_build(GF_Path *path, GF_List *commands, GF_List *points);
void gf_svg_register_iri(GF_SceneGraph *sg, SVG_IRI *iri);
void gf_svg_unregister_iri(GF_SceneGraph *sg, SVG_IRI *iri);


/* 
	basic DOM event handling
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

enum {
	DOM_KEY_UNIDENTIFIED = 0, 
	DOM_KEY_ACCEPT = 1, /* "Accept"    The Accept (Commit) key.*/
	DOM_KEY_AGAIN, /* "Again"  The Again key.*/
	DOM_KEY_ALLCANDIDATES, /* "AllCandidates"    The All Candidates key.*/
	DOM_KEY_ALPHANUM, /*"Alphanumeric"    The Alphanumeric key.*/
	DOM_KEY_ALT, /*"Alt"    The Alt (Menu) key.*/
	DOM_KEY_ALTGRAPH, /*"AltGraph"    The Alt-Graph key.*/
	DOM_KEY_APPS, /*"Apps"    The Application key.*/
	DOM_KEY_ATTN, /*"Attn"    The ATTN key.*/
	DOM_KEY_BROWSERBACK, /*"BrowserBack"    The Browser Back key.*/
	DOM_KEY_BROWSERFAVORITES, /*"BrowserFavorites"    The Browser Favorites key.*/
	DOM_KEY_BROWSERFORWARD, /*"BrowserForward"    The Browser Forward key.*/
	DOM_KEY_BROWSERHOME, /*"BrowserHome"    The Browser Home key.*/
	DOM_KEY_BROWSERREFRESH, /*"BrowserRefresh"    The Browser Refresh key.*/
	DOM_KEY_BROWSERSEARCH, /*"BrowserSearch"    The Browser Search key.*/
	DOM_KEY_BROWSERSTOP, /*"BrowserStop"    The Browser Stop key.*/
	DOM_KEY_CAPSLOCK, /*"CapsLock"    The Caps Lock (Capital) key.*/
	DOM_KEY_CLEAR, /*"Clear"    The Clear key.*/
	DOM_KEY_CODEINPUT, /*"CodeInput"    The Code Input key.*/
	DOM_KEY_COMPOSE, /*"Compose"    The Compose key.*/
	DOM_KEY_CONTROL, /*"Control"    The Control (Ctrl) key.*/
	DOM_KEY_CRSEL, /*"Crsel"    The Crsel key.*/
	DOM_KEY_CONVERT, /*"Convert"    The Convert key.*/
	DOM_KEY_COPY, /*"Copy"    The Copy key.*/
	DOM_KEY_CUT, /*"Cut"    The Cut key.*/
	DOM_KEY_DOWN, /*"Down"    The Down Arrow key.*/
	DOM_KEY_END, /*"End"    The End key.*/
	DOM_KEY_ENTER, /*"Enter"    The Enter key.
                   Note: This key identifier is also used for the Return (Macintosh numpad) key.*/
	DOM_KEY_ERASEEOF, /*"EraseEof"    The Erase EOF key.*/
	DOM_KEY_EXECUTE, /*"Execute"    The Execute key.*/
	DOM_KEY_EXSEL, /*"Exsel"    The Exsel key.*/
	DOM_KEY_F1, /*"F1"    The F1 key.*/
	DOM_KEY_F2, /*"F2"    The F2 key.*/
	DOM_KEY_F3, /*"F3"    The F3 key.*/
	DOM_KEY_F4, /*"F4"    The F4 key.*/
	DOM_KEY_F5, /*"F5"    The F5 key.*/
	DOM_KEY_F6, /*"F6"    The F6 key.*/
	DOM_KEY_F7, /*"F7"    The F7 key.*/
	DOM_KEY_F8, /*"F8"    The F8 key.*/
	DOM_KEY_F9, /*"F9"    The F9 key.*/
	DOM_KEY_F10, /*"F10"    The F10 key.*/
	DOM_KEY_F11, /*"F11"    The F11 key.*/
	DOM_KEY_F12, /*"F12"    The F12 key.*/
	DOM_KEY_F13, /*"F13"    The F13 key.*/
	DOM_KEY_F14, /*"F14"    The F14 key.*/
	DOM_KEY_F15, /*"F15"    The F15 key.*/
	DOM_KEY_F16, /*"F16"    The F16 key.*/
	DOM_KEY_F17, /*"F17"    The F17 key.*/
	DOM_KEY_F18, /*"F18"    The F18 key.*/
	DOM_KEY_F19, /*"F19"    The F19 key.*/
	DOM_KEY_F20, /*"F20"    The F20 key.*/
	DOM_KEY_F21, /*"F21"    The F21 key.*/
	DOM_KEY_F22, /*"F22"    The F22 key.*/
	DOM_KEY_F23, /*"F23"    The F23 key.*/
	DOM_KEY_F24, /*"F24"    The F24 key.*/
	DOM_KEY_FINALMODE, /*"FinalMode"    The Final Mode (Final) key used on some asian keyboards.*/
	DOM_KEY_FIND, /*"Find"    The Find key.*/
	DOM_KEY_FULLWIDTH, /*"FullWidth"    The Full-Width Characters key.*/
	DOM_KEY_HALFWIDTH, /*"HalfWidth"    The Half-Width Characters key.*/
	DOM_KEY_HANGULMODE, /*"HangulMode"    The Hangul (Korean characters) Mode key.*/
	DOM_KEY_HANJAMODE, /*"HanjaMode"    The Hanja (Korean characters) Mode key.*/
	DOM_KEY_HELP, /*"Help"    The Help key.*/
	DOM_KEY_HIRAGANA, /*"Hiragana"    The Hiragana (Japanese Kana characters) key.*/
	DOM_KEY_HOME, /*"Home"    The Home key.*/
	DOM_KEY_INSERT, /*"Insert"    The Insert (Ins) key.*/
	DOM_KEY_JAPANESEHIRAGANA, /*"JapaneseHiragana"    The Japanese-Hiragana key.*/
	DOM_KEY_JAPANESEKATAKANA, /*"JapaneseKatakana"    The Japanese-Katakana key.*/
	DOM_KEY_JAPANESEROMAJI, /*"JapaneseRomaji"    The Japanese-Romaji key.*/
	DOM_KEY_JUNJAMODE, /*"JunjaMode"    The Junja Mode key.*/
	DOM_KEY_KANAMODE, /*"KanaMode"    The Kana Mode (Kana Lock) key.*/
	DOM_KEY_KANJIMODE, /*"KanjiMode"    The Kanji (Japanese name for ideographic characters of Chinese origin) Mode key.*/
	DOM_KEY_KATAKANA, /*"Katakana"    The Katakana (Japanese Kana characters) key.*/
	DOM_KEY_LAUNCHAPPLICATION1, /*"LaunchApplication1"    The Start Application One key.*/
	DOM_KEY_LAUNCHAPPLICATION2, /*"LaunchApplication2"    The Start Application Two key.*/
	DOM_KEY_LAUNCHMAIL, /*"LaunchMail"    The Start Mail key.*/
	DOM_KEY_LEFT, /*"Left"    The Left Arrow key.*/
	DOM_KEY_META, /*"Meta"    The Meta key.*/
	DOM_KEY_MEDIANEXTTRACK, /*"MediaNextTrack"    The Media Next Track key.*/
	DOM_KEY_MEDIAPLAYPAUSE, /*"MediaPlayPause"    The Media Play Pause key.*/
	DOM_KEY_MEDIAPREVIOUSTRACK, /*"MediaPreviousTrack"    The Media Previous Track key.*/
	DOM_KEY_MEDIASTOP, /*"MediaStop"    The Media Stok key.*/
	DOM_KEY_MODECHANGE, /*"ModeChange"    The Mode Change key.*/
	DOM_KEY_NONCONVERT, /*"Nonconvert"    The Nonconvert (Don't Convert) key.*/
	DOM_KEY_NUMLOCK, /*"NumLock"    The Num Lock key.*/
	DOM_KEY_PAGEDOWN, /*"PageDown"    The Page Down (Next) key.*/
	DOM_KEY_PAGEUP, /*"PageUp"    The Page Up key.*/
	DOM_KEY_PASTE, /*"Paste"    The Paste key.*/
	DOM_KEY_PAUSE, /*"Pause"    The Pause key.*/
	DOM_KEY_PLAY, /*"Play"    The Play key.*/
	DOM_KEY_PREVIOUSCANDIDATE, /*"PreviousCandidate"    The Previous Candidate function key.*/
	DOM_KEY_PRINTSCREEN, /*"PrintScreen"    The Print Screen (PrintScrn, SnapShot) key.*/
	DOM_KEY_PROCESS, /*"Process"    The Process key.*/
	DOM_KEY_PROPS, /*"Props"    The Props key.*/
	DOM_KEY_RIGHT, /*"Right"    The Right Arrow key.*/
	DOM_KEY_ROMANCHARACTERS, /*"RomanCharacters"    The Roman Characters function key.*/
	DOM_KEY_SCROLL, /*"Scroll"    The Scroll Lock key.*/
	DOM_KEY_SELECT, /*"Select"    The Select key.*/
	DOM_KEY_SELECTMEDIA, /*"SelectMedia"    The Select Media key.*/
	DOM_KEY_SHIFT, /*"Shift"    The Shift key.*/
	DOM_KEY_STOP, /*"Stop"    The Stop key.*/
	DOM_KEY_UP, /*"Up"    The Up Arrow key.*/
	DOM_KEY_UNDO, /*"Undo"    The Undo key.*/
	DOM_KEY_VOLUMEDOWN, /*"VolumeDown"    The Volume Down key.*/
	DOM_KEY_VOLUMEMUTE, /*"VolumeMute"    The Volume Mute key.*/
	DOM_KEY_VOLUMEUP, /*"VolumeUp"    The Volume Up key.*/
	DOM_KEY_WIN, /*"Win"    The Windows Logo key.*/
	DOM_KEY_ZOOM, /*"Zoom"    The Zoom key.*/
	DOM_KEY_BACKSPACE, /*"U+0008"    The Backspace (Back) key.*/
	DOM_KEY_TAB, /*"U+0009"    The Horizontal Tabulation (Tab) key.*/
	DOM_KEY_CANCEL, /*"U+0018"    The Cancel key.*/
	DOM_KEY_ESCAPE, /*"U+001B"    The Escape (Esc) key.*/
	DOM_KEY_SPACE, /*"U+0020"    The Space (Spacebar) key.*/
	DOM_KEY_EXCLAMATION, /*"U+0021"    The Exclamation Mark (Factorial, Bang) key (!).*/
	DOM_KEY_QUOTATION, /*"U+0022"    The Quotation Mark (Quote Double) key (").*/
	DOM_KEY_NUMBER, /*"U+0023"    The Number Sign (Pound Sign, Hash, Crosshatch, Octothorpe) key (#).*/
	DOM_KEY_DOLLAR, /*"U+0024"    The Dollar Sign (milreis, escudo) key ($).*/
	DOM_KEY_AMPERSAND, /*"U+0026"    The Ampersand key (&).*/
	DOM_KEY_APOSTROPHE, /*"U+0027"    The Apostrophe (Apostrophe-Quote, APL Quote) key (').*/
	DOM_KEY_LEFTPARENTHESIS, /*"U+0028"    The Left Parenthesis (Opening Parenthesis) key (().*/
	DOM_KEY_RIGHTPARENTHESIS, /*"U+0029"    The Right Parenthesis (Closing Parenthesis) key ()).*/
	DOM_KEY_STAR, /*"U+002A"    The Asterix (Star) key (*).*/
	DOM_KEY_PLUS, /*"U+002B"    The Plus Sign (Plus) key (+).*/
	DOM_KEY_COMMA, /*"U+002C"    The Comma (decimal separator) sign key (,).*/
	DOM_KEY_HYPHEN, /*"U+002D"    The Hyphen-minus (hyphen or minus sign) key (-).*/
	DOM_KEY_FULLSTOP, /*"U+002E"    The Full Stop (period, dot, decimal point) key (.).*/
	DOM_KEY_SLASH, /*"U+002F"    The Solidus (slash, virgule, shilling) key (/).*/
	DOM_KEY_0, /*"U+0030"    The Digit Zero key (0).*/
	DOM_KEY_1, /*"U+0031"    The Digit One key (1).*/
	DOM_KEY_2, /*"U+0032"    The Digit Two key (2).*/
	DOM_KEY_3, /*"U+0033"    The Digit Three key (3).*/
	DOM_KEY_4, /*"U+0034"    The Digit Four key (4).*/
	DOM_KEY_5, /*"U+0035"    The Digit Five key (5).*/
	DOM_KEY_6, /*"U+0036"    The Digit Six key (6).*/
	DOM_KEY_7, /*"U+0037"    The Digit Seven key (7).*/
	DOM_KEY_8, /*"U+0038"    The Digit Eight key (8).*/
	DOM_KEY_9, /*"U+0039"    The Digit Nine key (9).*/
	DOM_KEY_COLON, /*"U+003A"    The Colon key (:).*/
	DOM_KEY_SEMICOLON, /*"U+003B"    The Semicolon key (;).*/
	DOM_KEY_LESSTHAN, /*"U+003C"    The Less-Than Sign key (<).*/
	DOM_KEY_EQUALS, /*"U+003D"    The Equals Sign key (=).*/
	DOM_KEY_GREATERTHAN, /*"U+003E"    The Greater-Than Sign key (>).*/
	DOM_KEY_QUESTION, /*"U+003F"    The Question Mark key (?).*/
	DOM_KEY_AT, /*"U+0040"    The Commercial At (@) key.*/
	DOM_KEY_A, /*"U+0041"    The Latin Capital Letter A key (A).*/
	DOM_KEY_B, /*"U+0042"    The Latin Capital Letter B key (B).*/
	DOM_KEY_C, /*"U+0043"    The Latin Capital Letter C key (C).*/
	DOM_KEY_D, /*"U+0044"    The Latin Capital Letter D key (D).*/
	DOM_KEY_E, /*"U+0045"    The Latin Capital Letter E key (E).*/
	DOM_KEY_F, /*"U+0046"    The Latin Capital Letter F key (F).*/
	DOM_KEY_G, /*"U+0047"    The Latin Capital Letter G key (G).*/
	DOM_KEY_H, /*"U+0048"    The Latin Capital Letter H key (H).*/
	DOM_KEY_I, /*"U+0049"    The Latin Capital Letter I key (I).*/
	DOM_KEY_J, /*"U+004A"    The Latin Capital Letter J key (J).*/
	DOM_KEY_K, /*"U+004B"    The Latin Capital Letter K key (K).*/
	DOM_KEY_L, /*"U+004C"    The Latin Capital Letter L key (L).*/
	DOM_KEY_M, /*"U+004D"    The Latin Capital Letter M key (M).*/
	DOM_KEY_N, /*"U+004E"    The Latin Capital Letter N key (N).*/
	DOM_KEY_O, /*"U+004F"    The Latin Capital Letter O key (O).*/
	DOM_KEY_P, /*"U+0050"    The Latin Capital Letter P key (P).*/
	DOM_KEY_Q, /*"U+0051"    The Latin Capital Letter Q key (Q).*/
	DOM_KEY_R, /*"U+0052"    The Latin Capital Letter R key (R).*/
	DOM_KEY_S, /*"U+0053"    The Latin Capital Letter S key (S).*/
	DOM_KEY_T, /*"U+0054"    The Latin Capital Letter T key (T).*/
	DOM_KEY_U, /*"U+0055"    The Latin Capital Letter U key (U).*/
	DOM_KEY_V, /*"U+0056"    The Latin Capital Letter V key (V).*/
	DOM_KEY_W, /*"U+0057"    The Latin Capital Letter W key (W).*/
	DOM_KEY_X, /*"U+0058"    The Latin Capital Letter X key (X).*/
	DOM_KEY_Y, /*"U+0059"    The Latin Capital Letter Y key (Y).*/
	DOM_KEY_Z, /*"U+005A"    The Latin Capital Letter Z key (Z).*/
	DOM_KEY_LEFTSQUAREBRACKET, /*"U+005B"    The Left Square Bracket (Opening Square Bracket) key ([).*/
	DOM_KEY_BACKSLASH, /*"U+005C"    The Reverse Solidus (Backslash) key (\).*/
	DOM_KEY_RIGHTSQUAREBRACKET, /*"U+005D"    The Right Square Bracket (Closing Square Bracket) key (]).*/
	DOM_KEY_CIRCUM, /*"U+005E"    The Circumflex Accent key (^).*/
	DOM_KEY_UNDERSCORE, /*"U+005F"    The Low Sign (Spacing Underscore, Underscore) key (_).*/
	DOM_KEY_GRAVEACCENT, /*"U+0060"    The Grave Accent (Back Quote) key (`).*/
	DOM_KEY_LEFTCURLYBRACKET, /*"U+007B"    The Left Curly Bracket (Opening Curly Bracket, Opening Brace, Brace Left) key ({).*/
	DOM_KEY_PIPE, /*"U+007C"    The Vertical Line (Vertical Bar, Pipe) key (|).*/
	DOM_KEY_RIGHTCURLYBRACKET, /*"U+007D"    The Right Curly Bracket (Closing Curly Bracket, Closing Brace, Brace Right) key (}).*/
	DOM_KEY_DEL, /*"U+007F"    The Delete (Del) Key.*/
	DOM_KEY_INVERTEXCLAMATION, /*"U+00A1"    The Inverted Exclamation Mark key (¡).*/
	DOM_KEY_DEADGRAVE, /*"U+0300"    The Combining Grave Accent (Greek Varia, Dead Grave) key.*/
	DOM_KEY_DEADEACUTE, /*"U+0301"    The Combining Acute Accent (Stress Mark, Greek Oxia, Tonos, Dead Eacute) key.*/
	DOM_KEY_DEADCIRCUM, /*"U+0302"    The Combining Circumflex Accent (Hat, Dead Circumflex) key.*/
	DOM_KEY_DEADTILDE, /*"U+0303"    The Combining Tilde (Dead Tilde) key.*/
	DOM_KEY_DEADMACRON, /*"U+0304"    The Combining Macron (Long, Dead Macron) key.*/
	DOM_KEY_DEADBREVE, /*"U+0306"    The Combining Breve (Short, Dead Breve) key.*/
	DOM_KEY_DEADABOVEDOT, /*"U+0307"    The Combining Dot Above (Derivative, Dead Above Dot) key.*/
	DOM_KEY_DEADDIARESIS, /*"U+0308"    The Combining Diaeresis (Double Dot Abode, Umlaut, Greek Dialytika, Double Derivative, Dead Diaeresis) key.*/
	DOM_KEY_DEADRINGABOVE, /*"U+030A"    The Combining Ring Above (Dead Above Ring) key.*/
	DOM_KEY_DEADDOUBLEACUTE, /*"U+030B"    The Combining Double Acute Accent (Dead Doubleacute) key.*/
	DOM_KEY_DEADCARON, /*"U+030C"    The Combining Caron (Hacek, V Above, Dead Caron) key.*/
	DOM_KEY_DEADCEDILLA, /*"U+0327"    The Combining Cedilla (Dead Cedilla) key.*/
	DOM_KEY_DEADOGONEK, /*"U+0328"    The Combining Ogonek (Nasal Hook, Dead Ogonek) key.*/
	DOM_KEY_DEADIOTA, /*"U+0345"    The Combining Greek Ypogegrammeni (Greek Non-Spacing Iota Below, Iota Subscript, Dead Iota) key.*/
	DOM_KEY_EURO, /*"U+20AC"    The Euro Currency Sign key (€).*/
	DOM_KEY_DEADVOICESOUND, /*"U+3099"    The Combining Katakana-Hiragana Voiced Sound Mark (Dead Voiced Sound) key.*/
	DOM_KEY_DEADSEMIVOICESOUND, /*"U+309A"    The Combining Katakana-Hiragana Semi-Voiced Sound Mark (Dead Semivoiced Sound) key. */
};

#ifdef __cplusplus
}
#endif

#endif	//_GF_SG_SVG_H_

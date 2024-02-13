/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2000-2024
 *					All rights reserved
 *
 *  This file is part of GPAC / MPEG-4 Object Descriptor sub-project
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

#ifndef _GF_MPEG4_ODF_H_
#define _GF_MPEG4_ODF_H_

#ifdef __cplusplus
extern "C" {
#endif

/*!
\file <gpac/mpeg4_odf.h>
\brief MPEG-4 Object Descriptor Framework.
 */

/*!
\addtogroup mpeg4sys_grp MPEG-4 Systems
\brief MPEG-4 Systems.
 */

/*!
\addtogroup odf_grp MPEG-4 OD
\ingroup mpeg4sys_grp
\brief MPEG-4 Object Descriptor Framework

This section documents the MPEG-4 OD, OCI and IPMPX functions of the GPAC framework.
@{
 */

#include <gpac/list.h>
#include <gpac/bitstream.h>
#include <gpac/sync_layer.h>

/*! Tags for MPEG-4 systems descriptors */
enum
{
	GF_ODF_OD_TAG			= 0x01,
	GF_ODF_IOD_TAG			= 0x02,
	GF_ODF_ESD_TAG			= 0x03,
	GF_ODF_DCD_TAG			= 0x04,
	GF_ODF_DSI_TAG			= 0x05,
	GF_ODF_SLC_TAG			= 0x06,
	GF_ODF_CI_TAG			= 0x07,
	GF_ODF_SCI_TAG			= 0x08,
	GF_ODF_IPI_PTR_TAG		= 0x09,
	GF_ODF_IPMP_PTR_TAG		= 0x0A,
	GF_ODF_IPMP_TAG			= 0x0B,
	GF_ODF_QOS_TAG			= 0x0C,
	GF_ODF_REG_TAG			= 0x0D,

	/*FILE FORMAT RESERVED IDs - NEVER CREATE / USE THESE DESCRIPTORS*/
	GF_ODF_ESD_INC_TAG		= 0x0E,
	GF_ODF_ESD_REF_TAG		= 0x0F,
	GF_ODF_ISOM_IOD_TAG		= 0x10,
	GF_ODF_ISOM_OD_TAG		= 0x11,
	GF_ODF_ISOM_IPI_PTR_TAG	= 0x12,
	/*END FILE FORMAT RESERVED*/

	GF_ODF_EXT_PL_TAG		= 0x13,
	GF_ODF_PL_IDX_TAG		= 0x14,

	GF_ODF_ISO_BEGIN_TAG	= 0x15,
	GF_ODF_ISO_END_TAG		= 0x3F,

	GF_ODF_CC_TAG			= 0x40,
	GF_ODF_KW_TAG			= 0x41,
	GF_ODF_RATING_TAG		= 0x42,
	GF_ODF_LANG_TAG			= 0x43,
	GF_ODF_SHORT_TEXT_TAG	= 0x44,
	GF_ODF_TEXT_TAG			= 0x45,
	GF_ODF_CC_NAME_TAG		= 0x46,
	GF_ODF_CC_DATE_TAG		= 0x47,
	GF_ODF_OCI_NAME_TAG		= 0x48,
	GF_ODF_OCI_DATE_TAG		= 0x49,
	GF_ODF_SMPTE_TAG		= 0x4A,

	GF_ODF_SEGMENT_TAG		= 0x4B,
	GF_ODF_MEDIATIME_TAG	= 0x4C,

	GF_ODF_IPMP_TL_TAG		= 0x60,
	GF_ODF_IPMP_TOOL_TAG	= 0x61,

	GF_ODF_ISO_RES_BEGIN_TAG	= 0x62,
	GF_ODF_ISO_RES_END_TAG		= 0xBF,

	GF_ODF_USER_BEGIN_TAG	= 0xC0,

	/*! internal descriptor for mux input description*/
	GF_ODF_MUXINFO_TAG		= GF_ODF_USER_BEGIN_TAG,
	/*! internal descriptor for bifs config input description*/
	GF_ODF_BIFS_CFG_TAG		= GF_ODF_USER_BEGIN_TAG + 1,
	/*! internal descriptor for UI config input description*/
	GF_ODF_UI_CFG_TAG		= GF_ODF_USER_BEGIN_TAG + 2,
	/*! internal descriptor for TextConfig description*/
	GF_ODF_TEXT_CFG_TAG		= GF_ODF_USER_BEGIN_TAG + 3,
	/*! internal descriptor for Text/TX3G description*/
	GF_ODF_TX3G_TAG			= GF_ODF_USER_BEGIN_TAG + 4,
	/*! internal descriptor for BIFS_anim input description*/
	GF_ODF_ELEM_MASK_TAG	= GF_ODF_USER_BEGIN_TAG + 5,
	/*! internal descriptor for LASeR config input description*/
	GF_ODF_LASER_CFG_TAG	= GF_ODF_USER_BEGIN_TAG + 6,
	/*! internal descriptor for subtitle stream description*/
	GF_ODF_GEN_SUB_CFG_TAG	= GF_ODF_USER_BEGIN_TAG + 7,

	GF_ODF_USER_END_TAG		= 0xFE,

	GF_ODF_OCI_BEGIN_TAG	= 0x40,
	GF_ODF_OCI_END_TAG		= (GF_ODF_ISO_RES_BEGIN_TAG - 1),

	GF_ODF_EXT_BEGIN_TAG	= 0x80,
	GF_ODF_EXT_END_TAG		= 0xFE,


	/*descriptor for aucilary video data*/
	GF_ODF_AUX_VIDEO_DATA	= GF_ODF_EXT_BEGIN_TAG + 1,
	GF_ODF_GPAC_LANG	= GF_ODF_EXT_BEGIN_TAG + 2
};


/***************************************
			Descriptors
***************************************/
/*! macro defining a base descriptor*/
#define BASE_DESCRIPTOR \
		u8 tag;


/*!	base descriptor used as base type in many function.*/
typedef struct
{
	BASE_DESCRIPTOR
} GF_Descriptor;


/*!	default descriptor.
	\note The decoderSpecificInfo is used as a default desc with tag 0x05 */
typedef struct
{
	BASE_DESCRIPTOR
	u32 dataLength;
	u8 *data;
} GF_DefaultDescriptor;

/*! Object Descriptor*/
typedef struct
{
	BASE_DESCRIPTOR
	GF_List *ipmp_tools;
} GF_IPMP_ToolList;

/*! ObjectDescriptor*/
typedef struct
{
	BASE_DESCRIPTOR
	u16 objectDescriptorID;
	char *URLString;
	GF_List *ESDescriptors;
	GF_List *OCIDescriptors;
	/*includes BOTH IPMP_DescriptorPointer (IPMP & IPMPX) and GF_IPMP_Descriptor (IPMPX only)*/
	GF_List *IPMP_Descriptors;
	GF_List *extensionDescriptors;
	/*MPEG-2 (or other service mux formats) service ID*/
	u32 ServiceID;
	/*for ROUTE, instructs client to keep OD alive even though URL string is set*/
	Bool RedirectOnly;
	/*set to true for fake remote ODs in BT/XMT (remote ODs created for OD with ESD with MuxInfo)*/
	Bool fake_remote;

} GF_ObjectDescriptor;

/*! GF_InitialObjectDescriptor - WARNING: even though the bitstream IOD is not
a bit extension of OD, internally it is a real overclass of OD
we usually typecast IOD to OD when flags are not needed !!!*/
typedef struct
{
	BASE_DESCRIPTOR
	u16 objectDescriptorID;
	char *URLString;
	GF_List *ESDescriptors;
	GF_List *OCIDescriptors;
	/*includes BOTH IPMP_DescriptorPointer (IPMP & IPMPX) and GF_IPMP_Descriptor (IPMPX only)*/
	GF_List *IPMP_Descriptors;
	GF_List *extensionDescriptors;
	/*MPEG-2 (or other service mux formats) service ID*/
	u16 ServiceID;
	/*for ROUTE, instructs client to keep OD alive even though URL string is set*/
	Bool RedirectOnly;
	/*set to true for fake remote ODs in BT/XMT (remote ODs created for OD with ESD with MuxInfo)*/
	Bool fake_remote;

	/*IOD extensions*/
	u8 inlineProfileFlag;
	u8 OD_profileAndLevel;
	u8 scene_profileAndLevel;
	u8 audio_profileAndLevel;
	u8 visual_profileAndLevel;
	u8 graphics_profileAndLevel;

	GF_IPMP_ToolList *IPMPToolList;
} GF_InitialObjectDescriptor;

/*! File Format Object Descriptor*/
typedef struct
{
	BASE_DESCRIPTOR
	u16 objectDescriptorID;
	char *URLString;
	GF_List *ES_ID_RefDescriptors;
	GF_List *OCIDescriptors;
	GF_List *IPMP_Descriptors;
	GF_List *extensionDescriptors;
	GF_List *ES_ID_IncDescriptors;

	/*MPEG-2 (or other service mux formats) service ID*/
	u32 ServiceID;
	/*for ROUTE, instructs client to keep OD alive even though URL string is set*/
	Bool RedirectOnly;
	/*set to true for fake remote ODs in BT/XMT (remote ODs created for OD with ESD with MuxInfo)*/
	Bool fake_remote;
} GF_IsomObjectDescriptor;

/*! File Format Initial Object Descriptor - same remark as IOD*/
typedef struct
{
	BASE_DESCRIPTOR
	u16 objectDescriptorID;
	char *URLString;
	GF_List *ES_ID_RefDescriptors;
	GF_List *OCIDescriptors;
	GF_List *IPMP_Descriptors;
	GF_List *extensionDescriptors;
	GF_List *ES_ID_IncDescriptors;

	u8 inlineProfileFlag;
	u8 OD_profileAndLevel;
	u8 scene_profileAndLevel;
	u8 audio_profileAndLevel;
	u8 visual_profileAndLevel;
	u8 graphics_profileAndLevel;

	GF_IPMP_ToolList *IPMPToolList;
} GF_IsomInitialObjectDescriptor;


/*! File Format ES Descriptor for IOD*/
typedef struct {
	BASE_DESCRIPTOR
	u32 trackID;
} GF_ES_ID_Inc;

/*! File Format ES Descriptor for OD*/
typedef struct {
	BASE_DESCRIPTOR
	u16 trackRef;
} GF_ES_ID_Ref;

/*! Decoder config Descriptor*/
typedef struct
{
	BASE_DESCRIPTOR
	/*coded on 8 bit, but we use 32 bits for internal signaling in GPAC to enable usage of 4CC*/
	u32 objectTypeIndication;
	u8 streamType;
	u8 upstream;
	u32 bufferSizeDB;
	u32 maxBitrate;
	u32 avgBitrate;
	GF_DefaultDescriptor *decoderSpecificInfo;

	/*placeholder for RVC decoder config if any*/
	u16 predefined_rvc_config;
	GF_DefaultDescriptor *rvc_config;

	GF_List *profileLevelIndicationIndexDescriptor;
	/*pass through data for some modules*/
	void *udta;
} GF_DecoderConfig;


/*! Content Identification Descriptor*/
typedef struct {
	BASE_DESCRIPTOR
	u8 compatibility;
	u8 protectedContent;
	u8 contentTypeFlag;
	u8 contentIdentifierFlag;
	u8 contentType;
	u8 contentIdentifierType;
	/*international code string*/
	char *contentIdentifier;
} GF_CIDesc;

/*! Supplementary Content Identification Descriptor)*/
typedef struct {
	BASE_DESCRIPTOR
	u32 languageCode;
	char *supplContentIdentifierTitle;
	char *supplContentIdentifierValue;
} GF_SCIDesc;

/*! IPI (Intelectual Property Identification) Descriptor Pointer*/
typedef struct {
	BASE_DESCRIPTOR
	u16 IPI_ES_Id;
} GF_IPIPtr;

/*! IPMP Descriptor Pointer*/
typedef struct {
	BASE_DESCRIPTOR
	u8 IPMP_DescriptorID;
	u16 IPMP_DescriptorIDEx;
	u16 IPMP_ES_ID;
} GF_IPMPPtr;

/*! IPMPX control points*/
enum
{
	/*no control point*/
	IPMP_CP_NONE = 0,
	/*control point between DB and decoder*/
	IPMP_CP_DB = 1,
	/*control point between decoder and CB*/
	IPMP_CP_CB = 2,
	/*control point between CB and render*/
	IPMP_CP_CM = 3,
	/*control point in BIFS tree (???)*/
	IPMP_CP_BIFS = 4
	               /*the rest is reserved or forbidden(0xFF)*/
};

/*! IPMPX base classe*/
#define GF_IPMPX_BASE	\
	u8 tag;	\
	u8 version;	\
	u32 dataID;	\

/*! IPMPX base object used for type casting in many function*/
typedef struct
{
	GF_IPMPX_BASE
} GF_GF_IPMPX_Base;

/*! IPMP descriptor*/
typedef struct {
	BASE_DESCRIPTOR
	u8 IPMP_DescriptorID;
	u16 IPMPS_Type;
	/*if IPMPS_Type=0, NULL-terminated URL, else if IPMPS_Type is not IPMPX, opaque data*/
	u8 *opaque_data;
	/*if IPMPS_Type=0, irrelevant (strlen(URL)), else if IPMPS_Type is not IPMPX, opaque data size*/
	u32 opaque_data_size;

	/*IPMPX specific*/
	u16 IPMP_DescriptorIDEx;
	bin128 IPMP_ToolID;
	u8 control_point;
	u8 cp_sequence_code;
	GF_List *ipmpx_data;
} GF_IPMP_Descriptor;


/*! IPMPX max number of tools*/
#define MAX_IPMP_ALT_TOOLS	20

/*! IPMPX Tool*/
typedef struct
{
	BASE_DESCRIPTOR
	bin128 IPMP_ToolID;
	/*if set, this is an alternate tool*/
	u32 num_alternate;
	bin128 specificToolID[MAX_IPMP_ALT_TOOLS];

	struct _tagIPMPXParamDesc *toolParamDesc;
	char *tool_url;
} GF_IPMP_Tool;


/*! Elementary Mask of Bifs Config - parsing only */
typedef struct {
	BASE_DESCRIPTOR
	u32 node_id;			/* referenced nodeID */
	char *node_name;		/* referenced node name */
} GF_ElementaryMask;

/*! BIFSConfig - parsing only, STORED IN ESD:DCD:DSI*/
typedef struct __tag_bifs_config
{
	BASE_DESCRIPTOR
	u32 version;
	u16 nodeIDbits;
	u16 routeIDbits;
	u16 protoIDbits;
	Bool pixelMetrics;
	u16 pixelWidth, pixelHeight;
	/*BIFS-Anim stuff*/
	Bool randomAccess;
	GF_List *elementaryMasks;
	/*internal extensions for encoding*/
	Bool useNames;
} GF_BIFSConfig;

/*! flags for text style*/
enum
{
	/*! normal*/
	GF_TXT_STYLE_NORMAL = 0,
	/*! bold*/
	GF_TXT_STYLE_BOLD = 1,
	/*! italic*/
	GF_TXT_STYLE_ITALIC = 1<<1,
	/*! underlined*/
	GF_TXT_STYLE_UNDERLINED = 1<<2,
	/*! strikethrough - not 3GPP/QT defined, GPAC only*/
	GF_TXT_STYLE_STRIKETHROUGH = 1<<3,
};

/*! text style record*/
typedef struct
{
	u16 startCharOffset;
	u16 endCharOffset;
	u16 fontID;
	u8 style_flags;
	u8 font_size;
	/*ARGB*/
	u32 text_color;
} GF_StyleRecord;

/*! font record for text*/
typedef struct
{
	u16 fontID;
	char *fontName;
} GF_FontRecord;

/*! positioning record for text*/
typedef struct
{
	s16 top, left, bottom, right;
} GF_BoxRecord;

/*! scroll flags for text*/
enum
{
	GF_TXT_SCROLL_CREDITS = 0,
	GF_TXT_SCROLL_MARQUEE = 1,
	GF_TXT_SCROLL_DOWN = 2,
	GF_TXT_SCROLL_RIGHT = 3
};

/*! display flags for text*/
enum
{
	GF_TXT_SCROLL_IN = 0x00000020,
	GF_TXT_SCROLL_OUT = 0x00000040,
	/*use one of the scroll flags, eg GF_TXT_SCROLL_DIRECTION | GF_TXT_SCROLL_CREDITS*/
	GF_TXT_SCROLL_DIRECTION = 0x00000180,
	GF_TXT_KARAOKE	= 0x00000800,
	GF_TXT_VERTICAL = 0x00020000,
	GF_TXT_FILL_REGION = 0x00040000,

	GF_TXT_NO_SCALE = 0x2,
	GF_TXT_MOVIE_BACK_COLOR = 0x8,
	GF_TXT_CONTINUOUS_SCROLL = 0x200,
	GF_TXT_DROP_SHADOW = 0x1000,
	GF_TXT_FILL_ANTIALIAS = 0x2000,
	GF_TXT_SOME_SAMPLES_FORCED = 0x40000000,
	GF_TXT_ALL_SAMPLES_FORCED = 0x80000000,
};

/*! Text sample description descriptor (eg mostly a copy of ISOBMF sample entry)*/
typedef struct
{
	/*this is defined as a descriptor for parsing*/
	BASE_DESCRIPTOR

	u32 displayFlags;
	/*left, top: 0 -  centered: 1 - bottom, right: -1*/
	s8 horiz_justif, vert_justif;
	/*ARGB*/
	u32 back_color;
	GF_BoxRecord default_pos;
	GF_StyleRecord	default_style;

	u32 font_count;
	GF_FontRecord *fonts;

	/*unused in isomedia but needed for streamingText*/
	u8 sample_index;
} GF_TextSampleDescriptor;

/*! Text stream descriptor, internal only*/
typedef struct
{
	BASE_DESCRIPTOR
	/*only 0x10 shall be used for 3GP text stream*/
	u8 Base3GPPFormat;
	/*only 0x10 shall be used for StreamingText*/
	u8 MPEGExtendedFormat;
	/*only 0x10 shall be used for StreamingText (base profile, base level)*/
	u8 profileLevel;
	u32 timescale;
	/*0 forbidden, 1: out-of-band desc only, 2: in-band desc only, 3: both*/
	u8 sampleDescriptionFlags;
	/*More negative layer values are towards the viewer*/
	s16 layer;
	/*text track width & height*/
	u16 text_width;
	u16 text_height;
	/*compatible 3GP formats, same coding as 3GPPBaseFormat*/
	u8 nb_compatible_formats;
	u8 compatible_formats[20];
	/*defined in isomedia.h*/
	GF_List *sample_descriptions;

	/*if true info below are valid (cf 3GPP for their meaning)*/
	Bool has_vid_info;
	u16 video_width;
	u16 video_height;
	s16 horiz_offset;
	s16 vert_offset;
} GF_TextConfig;

/*! generic subtitle sample description descriptor*/
typedef struct
{
	/*this is defined as a descriptor for parsing*/
	BASE_DESCRIPTOR

	/*unused in isomedia but needed for streamingText*/
	u8 sample_index;
} GF_GenericSubtitleSampleDescriptor;

/*! generic subtitle descriptor*/
typedef struct
{
	BASE_DESCRIPTOR
	u32 timescale;
	/*More negative layer values are towards the viewer*/
	s16 layer;
	/*text track width & height*/
	u16 text_width;
	u16 text_height;
	/*defined in isomedia.h*/
	GF_List *sample_descriptions;

	/*if true info below are valid (cf 3GPP for their meaning)*/
	Bool has_vid_info;
	u16 video_width;
	u16 video_height;
	s16 horiz_offset;
	s16 vert_offset;
} GF_GenericSubtitleConfig;


/*! MuxInfo descriptor - parsing only, stored in ESD:extDescr*/
typedef struct {
	BASE_DESCRIPTOR
	/*input location*/
	char *file_name;
	/*input groupID for interleaving*/
	u32 GroupID;
	/*input stream format (not required, guessed from file_name)*/
	char *streamFormat;
	/*time offset in ms from first TS (appends an edit list in mp4)*/
	s32 startTime;

	/*media length to import in ms (from 0)*/
	u32 duration;

	/*SRT/SUB import extensions - only support for text and italic style*/
	char *textNode;
	char *fontNode;

	/*video and SUB import*/
	Double frame_rate;

	/*same as importer flags, cf media.h*/
	u32 import_flags;

	/*indicates input file shall be destryed - used during SWF import*/
	Bool delete_file;

	/*carousel configuration*/
	u32 carousel_period_plus_one;
	u16 aggregate_on_esid;

	/*original source URL*/
	char *src_url;
} GF_MuxInfo;

/*! UI config descriptor for InputSensor streams*/
typedef struct
{
	BASE_DESCRIPTOR
	/*input type*/
	char *deviceName;
	/*string sensor terminaison (validation) char*/
	char termChar;
	/*string sensor deletion char*/
	char delChar;
	/*device-specific data*/
	u8 *ui_data;
	u32 ui_data_length;
} GF_UIConfig;

/*! LASERConfig - parsing only, STORED IN ESD:DCD:DSI*/
typedef struct __tag_laser_config
{
	BASE_DESCRIPTOR
	u8 profile;
	u8 level;
	u8 pointsCodec;
	u8 pathComponents;
	u8 fullRequestHost;
	u16 time_resolution;
	u8 colorComponentBits;
	s8 resolution;
	u8 coord_bits;
	u8 scale_bits_minus_coord_bits;
	u8 newSceneIndicator;
	u8 extensionIDBits;

	/*the rest of the structure is never coded, only used for the config of GPAC...*/
	Bool force_string_ids;/*forces all nodes to be defined with string IDs*/
} GF_LASERConfig;


/*! QoS Tags */
enum
{
	QoSMaxDelayTag = 0x01,
	QoSPrefMaxDelayTag = 0x02,
	QoSLossProbTag = 0x03,
	QoSMaxGapLossTag = 0x04,
	QoSMaxAUSizeTag = 0x41,
	QoSAvgAUSizeTag = 0x42,
	QoSMaxAURateTag = 0x43
};

/*! QoS Descriptor*/
typedef struct {
	BASE_DESCRIPTOR
	u8 predefined;
	GF_List *QoS_Qualifiers;
} GF_QoS_Descriptor;


/*! macro defining a base QOS qualifier*/
#define QOS_BASE_QUALIFIER \
	u8 tag;	\
	u32 size;

/*! QoS Default Qualifier*/
typedef struct {
	QOS_BASE_QUALIFIER
} GF_QoS_Default;

/*! QoS Max Delay Qualifier*/
typedef struct {
	QOS_BASE_QUALIFIER
	u32 MaxDelay;
} GF_QoS_MaxDelay;

/*! QoS preferred Max Delay Qualifier*/
typedef struct {
	QOS_BASE_QUALIFIER
	u32 PrefMaxDelay;
} GF_QoS_PrefMaxDelay;

/*! QoS loss probability Qualifier*/
typedef struct {
	QOS_BASE_QUALIFIER
	Float LossProb;
} GF_QoS_LossProb;

/*! QoS Max Gap Loss Qualifier*/
typedef struct {
	QOS_BASE_QUALIFIER
	u32 MaxGapLoss;
} GF_QoS_MaxGapLoss;

/*! QoS Max AU Size Qualifier*/
typedef struct {
	QOS_BASE_QUALIFIER
	u32 MaxAUSize;
} GF_QoS_MaxAUSize;

/*! QoS Average AU Size Qualifier*/
typedef struct {
	QOS_BASE_QUALIFIER
	u32 AvgAUSize;
} GF_QoS_AvgAUSize;

/*! QoS AU rate Qualifier*/
typedef struct {
	QOS_BASE_QUALIFIER
	u32 MaxAURate;
} GF_QoS_MaxAURate;

/*! QoS private  Qualifier*/
typedef struct {
	QOS_BASE_QUALIFIER
	/*! max size class is  2^28 - 1*/
	u32 DataLength;
	u8 *Data;
} GF_QoS_Private;


/*! Registration Descriptor*/
typedef struct {
	BASE_DESCRIPTOR
	u32 formatIdentifier;
	u32 dataLength;
	u8 *additionalIdentificationInfo;
} GF_Registration;

/*! Language Descriptor*/
typedef struct {
	BASE_DESCRIPTOR
	u32 langCode;
	char *full_lang_code;
} GF_Language;

/*! Elementary Stream Descriptor*/
typedef struct
{
	BASE_DESCRIPTOR
	u16 ESID;
	u16 OCRESID;
	u16 dependsOnESID;
	u8 streamPriority;
	char *URLString;
	GF_DecoderConfig *decoderConfig;
	GF_SLConfig *slConfig;
	GF_IPIPtr *ipiPtr;
	GF_QoS_Descriptor *qos;
	GF_Registration *RegDescriptor;
	/*! 0 or 1 lang desc*/
	GF_Language *langDesc;

	GF_List *IPIDataSet;
	GF_List *IPMPDescriptorPointers;
	GF_List *extensionDescriptors;

	//GPAC internals

	/*! 1 if this stream has scalable layers, 0 otherwise (GPAC internals)*/
	Bool has_scalable_layers;
	/*! service URL (GPAC internals)*/
	const char *service_url;
} GF_ESD;


/*! Auxiliary Video Data Descriptor*/
typedef struct {
	BASE_DESCRIPTOR
	u32 aux_video_type;
	u32 position_offset_h;
	u32 position_offset_v;
	u32 knear;
	u32 kfar;
	u32 parallax_zero;
	u32 parallax_scale;
	u32 dref;
	u32 wref;
} GF_AuxVideoDescriptor;

/*! Content Classification Descriptor*/
typedef struct {
	BASE_DESCRIPTOR
	u32 classificationEntity;
	u16 classificationTable;
	u32 dataLength;
	char *contentClassificationData;
} GF_CCDescriptor;


/*! this structure is used in GF_KeyWord*/
typedef struct {
	char *keyWord;
} GF_KeyWordItem;

/*! Key Word Descriptor*/
typedef struct {
	BASE_DESCRIPTOR
	u32 languageCode;
	u8 isUTF8;
	GF_List *keyWordsList;
} GF_KeyWord;

/*! Rating Descriptor*/
typedef struct {
	BASE_DESCRIPTOR
	u32 ratingEntity;
	u16 ratingCriteria;
	u32 infoLength;
	char *ratingInfo;
} GF_Rating;


/*! Short Textual Descriptor*/
typedef struct {
	BASE_DESCRIPTOR
	u32 langCode;
	u8 isUTF8;
	char *eventName;
	char *eventText;
} GF_ShortTextual;


/*! this structure is used in GF_ExpandedTextual*/
typedef struct {
	char *text;
} GF_ETD_ItemText;

/*! Expanded Textual Descriptor*/
typedef struct {
	BASE_DESCRIPTOR
	u32 langCode;
	u8 isUTF8;
	GF_List *itemDescriptionList;
	GF_List *itemTextList;
	char *NonItemText;
} GF_ExpandedTextual;

/*! this structure is used in GF_CC_Name*/
typedef struct {
	u32 langCode;
	u8 isUTF8;
	char *contentCreatorName;
} GF_ContentCreatorInfo;

/*! Content Creator Name GF_Descriptor
\note The desctructor will delete all the items in the list
(GF_ContentCreatorInfo items) */
typedef struct {
	BASE_DESCRIPTOR
	GF_List *ContentCreators;
} GF_CC_Name;

/*! Content Creation Date Descriptor*/
typedef struct {
	BASE_DESCRIPTOR
	char contentCreationDate[5];
} GF_CC_Date;


/*! this structure is used in GF_OCICreators*/
typedef struct {
	u32 langCode;
	u8 isUTF8;
	char *OCICreatorName;
} GF_OCICreator_item;

/*! OCI Creator Name Descriptor*/
typedef struct {
	BASE_DESCRIPTOR
	GF_List *OCICreators;
} GF_OCICreators;

/*! OCI Creation Date Descriptor*/
typedef struct {
	BASE_DESCRIPTOR
	char OCICreationDate[5];
} GF_OCI_Data;


/*! this structure is used in GF_SMPTECamera*/
typedef struct {
	u8 paramID;
	u32 param;
} GF_SmpteParam;

/*! Smpte Camera Position Descriptor*/
typedef struct {
	BASE_DESCRIPTOR
	u8 cameraID;
	GF_List *ParamList;
} GF_SMPTECamera;


/*! Extension Profile Level Descriptor*/
typedef struct {
	BASE_DESCRIPTOR
	u8 profileLevelIndicationIndex;
	u8 ODProfileLevelIndication;
	u8 SceneProfileLevelIndication;
	u8 AudioProfileLevelIndication;
	u8 VisualProfileLevelIndication;
	u8 GraphicsProfileLevelIndication;
	u8 MPEGJProfileLevelIndication;
} GF_PLExt;

/*! Profile Level Indication Index Descriptor*/
typedef struct {
	BASE_DESCRIPTOR
	u8 profileLevelIndicationIndex;
} GF_PL_IDX;

/*! used for storing NALU-based parameter set in AVC/HEVC/VVC configuration record*/
typedef struct
{
	/*! size of nal*/
	u16 size;
	/*! nal data*/
	u8 *data;
	/*! ID of param set, used by some importers but not written in file*/
	s32 id;
	/*! CRC of nal, used by some importers but not written in file*/
	u32 crc;
} GF_NALUFFParam;

/*! pre v1.1 naming of NALU config record*/
typedef GF_NALUFFParam GF_AVCConfigSlot;

/*! AVC config record - not a real MPEG-4 descriptor
*/
typedef struct
{
	u8 configurationVersion;
	u8 AVCProfileIndication;
	u8 profile_compatibility;
	u8 AVCLevelIndication;
	u8 nal_unit_size;

	GF_List *sequenceParameterSets;
	GF_List *pictureParameterSets;

	/*for SVC*/
	u8 complete_representation;

	/*for high profiles*/
	u8 chroma_format;
	u8 luma_bit_depth;
	u8 chroma_bit_depth;
	/*may be NULL*/
	GF_List *sequenceParameterSetExtensions;

	Bool write_annex_b;
} GF_AVCConfig;



/*! used for storing HEVC SPS/PPS/VPS/SEI*/
typedef struct
{
	u8 type;
	u8 array_completeness;
	GF_List *nalus;
} GF_NALUFFParamArray;

/*! pre v1.1 naming of NALU param array*/
typedef GF_NALUFFParamArray GF_HEVCParamArray;

/*! HEVC config record - not a real MPEG-4 descriptor*/
typedef struct
{
	u8 configurationVersion;
	u8 profile_space;
	u8 tier_flag;
	u8 profile_idc;
	u32 general_profile_compatibility_flags;
	u8 progressive_source_flag;
	u8 interlaced_source_flag;
	u8 non_packed_constraint_flag;
	u8 frame_only_constraint_flag;
	/*only lowest 44 bits used*/
	u64 constraint_indicator_flags;
	u8 level_idc;
	u16 min_spatial_segmentation_idc;

	u8 parallelismType;
	u8 chromaFormat;
	u8 luma_bit_depth;
	u8 chroma_bit_depth;
	u16 avgFrameRate;
	u8 constantFrameRate;
	u8 numTemporalLayers;
	u8 temporalIdNested;

	u8 nal_unit_size;

	GF_List *param_array;
	//used in LHVC config
	Bool complete_representation;

	//following are internal to libgpac and NEVER serialized

	//set by libisomedia at import/export/parsing time to differentiate between lhcC and hvcC time
	Bool is_lhvc;

	Bool write_annex_b;

} GF_HEVCConfig;



/*! VVC config record - not a real MPEG-4 descriptor*/
typedef struct
{
	u8 general_profile_idc;
	u8 general_tier_flag;
	u8 general_sub_profile_idc;
	u8 num_constraint_info;
	u8 *general_constraint_info;
	u8 general_level_idc;

	u8 ptl_sublayer_present_mask;
	u8 sublayer_level_idc[8];

	u8 chroma_format;
	u8 bit_depth;
	u16 avgFrameRate;
	u8 constantFrameRate;
	u8 numTemporalLayers;
	u16 maxPictureWidth, maxPictureHeight;

	Bool ptl_present, ptl_frame_only_constraint, ptl_multilayer_enabled;
	u8 num_sub_profiles;
	u32 *sub_profiles_idc;

	u16 ols_idx;
	u8 nal_unit_size;

	GF_List *param_array;

	Bool write_annex_b;
} GF_VVCConfig;


/*! used for storing AV1 OBUs*/
typedef struct
{
	u64 obu_length;
	int obu_type; /*ObuType*/
	u8 *obu;
} GF_AV1_OBUArrayEntry;

/*! AV1 config record - not a real MPEG-4 descriptor*/
typedef struct
{
	Bool marker;
	u8 version; /*7 bits*/
	u8 seq_profile; /*3 bits*/
	u8 seq_level_idx_0; /*5 bits*/
	Bool seq_tier_0;
	Bool high_bitdepth;
	Bool twelve_bit;
	Bool monochrome;
	Bool chroma_subsampling_x;
	Bool chroma_subsampling_y;
	u8 chroma_sample_position; /*2 bits*/

	Bool initial_presentation_delay_present;
	u8 initial_presentation_delay_minus_one; /*4 bits*/
	GF_List *obu_array; /*GF_AV1_OBUArrayEntry*/
} GF_AV1Config;

/*! max number of reference frames for VP9*/
#define VP9_NUM_REF_FRAMES	8

/*! VP8-9 config vpcC */
typedef struct
{
	u8	profile;
	u8	level;

	u8   bit_depth; /*4 bits*/
	u8   chroma_subsampling; /*3 bits*/
	Bool video_fullRange_flag;

	u8  colour_primaries;
	u8  transfer_characteristics;
	u8  matrix_coefficients;

	/* MUST be 0 for VP8 and VP9 */
	u16 codec_initdata_size;
	u8* codec_initdata;

	/* parsing state information - not used for vpcC*/
	int RefFrameWidth[VP9_NUM_REF_FRAMES];
	int RefFrameHeight[VP9_NUM_REF_FRAMES];
} GF_VPConfig;

/*! DolbyVision config dvcC/dvvC */
typedef struct {
	u8 dv_version_major;
	u8 dv_version_minor;
	u8 dv_profile; //7 bits
	u8 dv_level;   //6 bits
	Bool rpu_present_flag;
	Bool el_present_flag;
	Bool bl_present_flag;
	u8 dv_bl_signal_compatibility_id; //4 bits
	//const unsigned int (28) reserved = 0;
	//const unsigned int (32)[4] reserved = 0;

	//internal, force dvhe or dvh1 signaling
	u8 force_dv;
} GF_DOVIDecoderConfigurationRecord;

/*! Media Segment Descriptor used for Media Control Extensions*/
typedef struct
{
	BASE_DESCRIPTOR
	Double startTime;
	Double Duration;
	char *SegmentName;
} GF_Segment;

/*! Media Time Descriptor used for Media Control Extensions*/
typedef struct
{
	BASE_DESCRIPTOR
	Double mediaTimeStamp;
} GF_MediaTime;


/*! MPEG-4 SYSTEMS OD Commands Tags */
enum
{
	GF_ODF_OD_UPDATE_TAG					= 0x01,
	GF_ODF_OD_REMOVE_TAG					= 0x02,
	GF_ODF_ESD_UPDATE_TAG					= 0x03,
	GF_ODF_ESD_REMOVE_TAG					= 0x04,
	GF_ODF_IPMP_UPDATE_TAG					= 0x05,
	GF_ODF_IPMP_REMOVE_TAG					= 0x06,

	/*file format reserved*/
	GF_ODF_ESD_REMOVE_REF_TAG				= 0x07,

	GF_ODF_COM_ISO_BEGIN_TAG		= 0x0D,
	GF_ODF_COM_ISO_END_TAG		= 0xBF,

	GF_ODF_COM_USER_BEGIN_TAG	= 0xC0,
	GF_ODF_COM_USER_END_TAG		= 0xFE
};

/*! macro defining a base OD command*/
#define BASE_OD_COMMAND \
	u8 tag;

/*! MPEG-4 SYSTEMS OD - (abstract) base command. */
typedef struct {
	BASE_OD_COMMAND
} GF_ODCom;

/*! MPEG-4 SYSTEMS OD - default command*/
typedef struct {
	BASE_OD_COMMAND
	u32 dataSize;
	u8 *data;
} GF_BaseODCom;

/*! MPEG-4 SYSTEMS OD - Object Descriptor Update
NB: the list can contain OD or IOD, except internally in the File Format (only MP4_OD)*/
typedef struct
{
	BASE_OD_COMMAND
	GF_List *objectDescriptors;
} GF_ODUpdate;

/*! MPEG-4 SYSTEMS OD - Object Descriptor Remove*/
typedef struct
{
	BASE_OD_COMMAND
	u32 NbODs;
	u16 *OD_ID;
} GF_ODRemove;

/*! MPEG-4 SYSTEMS OD - Elementary Stream Descriptor Update*/
typedef struct
{
	BASE_OD_COMMAND
	u16 ODID;
	GF_List *ESDescriptors;
} GF_ESDUpdate;

/*! MPEG-4 SYSTEMS OD - Elementary Stream Descriptor Remove*/
typedef struct {
	BASE_OD_COMMAND
	u16 ODID;
	u32 NbESDs;
	u16 *ES_ID;
} GF_ESDRemove;

/*! MPEG-4 SYSTEMS OD - IPMP Descriptor Update*/
typedef struct {
	BASE_OD_COMMAND
	GF_List *IPMPDescList;
} GF_IPMPUpdate;

/*! MPEG-4 SYSTEMS OD - IPMP Descriptor Remove*/
typedef struct {
	BASE_OD_COMMAND
	u32 NbIPMPDs;
	/*now this is bad: only IPMPv1 descriptors can be removed at run tim...*/
	u8 *IPMPDescID;
} GF_IPMPRemove;






/*! MPEG-4 SYSTEMS OD - OD API */

/*! OD CODEC object - just a simple reader/writer*/
typedef struct tagODCoDec
{
	GF_BitStream *bs;
	GF_List *CommandList;
} GF_ODCodec;


/*! OD codec construction
\return new codec object*/
GF_ODCodec *gf_odf_codec_new();
/*! OD codec destruction
\param codec OD codec to destroy
 */
void gf_odf_codec_del(GF_ODCodec *codec);
/*! add a command to the codec command list.
\param codec target codec
\param command command to add
\return error if any
 */
GF_Err gf_odf_codec_add_com(GF_ODCodec *codec, GF_ODCom *command);
/*! encode the current command list.
\param codec target codec
\param cleanup_type specifies what to do with the command after encoding. The following values are accepted:
	0: commands are removed from the list but not destroyed
	1: commands are removed from the list and destroyed
	2: commands are kept in the list and not destroyed
\return error if any
*/
GF_Err gf_odf_codec_encode(GF_ODCodec *codec, u32 cleanup_type);
/*! get the encoded AU.
\param codec target codec
\param outAU output buffer allocated by the codec, user is responsible of freeing the allocated space
\param au_length size of the AU (allocated buffer)
\return error if any
 */
GF_Err gf_odf_codec_get_au(GF_ODCodec *codec, u8 **outAU, u32 *au_length);
/*! set the encoded AU to the codec
\param codec target codec
\param au target AU to decode
\param au_length size in bytes of the AU to decode
\return error if any
 */
GF_Err gf_odf_codec_set_au(GF_ODCodec *codec, const u8 *au, u32 au_length);
/*! decode the previously set-up AU
\param codec target codec
\return error if any
 */
GF_Err gf_odf_codec_decode(GF_ODCodec *codec);
/*! get the first OD command in the list. Once called, the command is removed
from the command list. Return NULL when commandList is empty
\param codec target codec
\return deocded command or NULL
 */
GF_ODCom *gf_odf_codec_get_com(GF_ODCodec *codec);

/*! apply a command to the codec command list. Command is duplicated if needed
This is used for state maintenance and RAP generation.
\param codec target codec
\param command the command to apply
\return error if any
 */
GF_Err gf_odf_codec_apply_com(GF_ODCodec *codec, GF_ODCom *command);


/*! MPEG-4 SYSTEMS OD Command Creation
\param tag type of command to create
\return the created command or NULL
 */
GF_ODCom *gf_odf_com_new(u8 tag);
/*! MPEG-4 SYSTEMS OD Command Destruction
\param com the command to delete. Pointer is set back to NULL
\return error if any
 */
GF_Err gf_odf_com_del(GF_ODCom **com);


/************************************************************
		Descriptors Functions
************************************************************/

/*! Descriptors Creation
\param tag type of descriptor to create
\return created descriptor or NULL
 */
GF_Descriptor *gf_odf_desc_new(u8 tag);
/*! Descriptors Destruction
\param desc the descriptor to destroy
 */
void gf_odf_desc_del(GF_Descriptor *desc);

/*! helper for building a preformatted GF_ESD with decoderConfig, decoderSpecificInfo with no data and
SLConfig descriptor with predefined
\param sl_predefined type of predefined sl config
\return the ESD created
*/
GF_ESD *gf_odf_desc_esd_new(u32 sl_predefined);

/*! special function for authoring - convert DSI to BIFSConfig
\param dsi BIFS decoder specific info
\param codecid BIFS codecid/object type indication
\return decoded BIFS Config descriptor - It is the caller responsibility of freeing it
 */
GF_BIFSConfig *gf_odf_get_bifs_config(GF_DefaultDescriptor *dsi, u32 codecid);
/*! special function for authoring - convert DSI to LASERConfig
\param dsi LASER decoder specific info
\param cfg the LASER config object to be filled
\return error if any
 */
GF_Err gf_odf_get_laser_config(GF_DefaultDescriptor *dsi, GF_LASERConfig *cfg);
/*! special function for authoring - convert DSI to TextConfig
\param data TEXT decoder config block
\param data_len TEXT decoder config block size
\param codecid TEXT codecid/object type indication
\param cfg the text config object to be filled
\return error if any
 */
GF_Err gf_odf_get_text_config(u8 *data, u32 data_len, u32 codecid, GF_TextConfig *cfg);
/*! converts UIConfig to dsi - does not destroy input descr but does create output one
\param cfg the UI config object
\param out_dsi the decoder specific info created. It is the caller responsibility of freeing it
\return error if any
 */
GF_Err gf_odf_encode_ui_config(GF_UIConfig *cfg, GF_DefaultDescriptor **out_dsi);

/*! AVC config constructor
\return the created AVC config*/
GF_AVCConfig *gf_odf_avc_cfg_new();
/*! AVC config destructor
\param cfg the AVC config to destroy*/
void gf_odf_avc_cfg_del(GF_AVCConfig *cfg);
/*! gets GF_AVCConfig from MPEG-4 DSI
\param dsi encoded AVC decoder specific info
\param dsi_size encoded AVC decoder specific info size
\return the decoded AVC config
 */
GF_AVCConfig *gf_odf_avc_cfg_read(u8 *dsi, u32 dsi_size);
/*! writes GF_AVCConfig
\param cfg the AVC config to encode
\param outData encoded dsi buffer - it is the caller responsibility to free this
\param outSize  encoded dsi buffer size
\return error if any
 */
GF_Err gf_odf_avc_cfg_write(GF_AVCConfig *cfg, u8 **outData, u32 *outSize);

/*! writes GF_AVCConfig to bitstream
\param cfg the AVC config to encode
\param bs bitstream in WRITE mode
\return error if any
 */
GF_Err gf_odf_avc_cfg_write_bs(GF_AVCConfig *cfg, GF_BitStream *bs);

/*! writes GF_TextSampleDescriptor
\param cfg the text config to encode
\param outData address of output buffer (internal alloc, user to free it)
\param outSize size of the allocated output
\return error if any
 */
GF_Err gf_odf_tx3g_write(GF_TextSampleDescriptor *cfg, u8 **outData, u32 *outSize);

/*! gets GF_TextSampleDescriptor from buffer
\param dsi encoded text sample description
\param dsi_size size of encoded description
\return the decoded GF_TextSampleDescriptor config
 */
GF_TextSampleDescriptor *gf_odf_tx3g_read(u8 *dsi, u32 dsi_size);

/*! HEVC config constructor
\return the created HEVC config*/
GF_HEVCConfig *gf_odf_hevc_cfg_new();

/*! HEVC config destructor
\param cfg the HEVC config to destroy*/
void gf_odf_hevc_cfg_del(GF_HEVCConfig *cfg);

/*! writes GF_HEVCConfig as MPEG-4 DSI in a bitstream object
\param cfg the HEVC config to encode
\param bs output bitstream object in which the config is written
\return error if any
 */
GF_Err gf_odf_hevc_cfg_write_bs(GF_HEVCConfig *cfg, GF_BitStream *bs);

/*! writes GF_HEVCConfig as MPEG-4 DSI
\param cfg the HEVC config to encode
\param outData encoded dsi buffer - it is the caller responsibility to free this
\param outSize  encoded dsi buffer size
\return error if any
 */
GF_Err gf_odf_hevc_cfg_write(GF_HEVCConfig *cfg, u8 **outData, u32 *outSize);

/*! gets GF_HEVCConfig from bitstream MPEG-4 DSI
\param bs bitstream containing the encoded HEVC decoder specific info
\param is_lhvc if GF_TRUE, indicates if the dsi is LHVC
\return the decoded HEVC config
 */
GF_HEVCConfig *gf_odf_hevc_cfg_read_bs(GF_BitStream *bs, Bool is_lhvc);

/*! gets GF_HEVCConfig from MPEG-4 DSI
\param dsi encoded HEVC decoder specific info
\param dsi_size encoded HEVC decoder specific info size
\param is_lhvc if GF_TRUE, indicates if the dsi is LHVC
\return the decoded HEVC config
 */
GF_HEVCConfig *gf_odf_hevc_cfg_read(u8 *dsi, u32 dsi_size, Bool is_lhvc);


/*! VVC config constructor
\return the created VVC config*/
GF_VVCConfig *gf_odf_vvc_cfg_new();

/*! VVC config destructor
\param cfg the VVC config to destroy*/
void gf_odf_vvc_cfg_del(GF_VVCConfig *cfg);

/*! writes GF_VVCConfig as MPEG-4 DSI in a bitstream object
\param cfg the VVC config to encode
\param bs output bitstream object in which the config is written
\return error if any
 */
GF_Err gf_odf_vvc_cfg_write_bs(GF_VVCConfig *cfg, GF_BitStream *bs);

/*! writes GF_VVCConfig as MPEG-4 DSI
\param cfg the VVC config to encode
\param outData encoded dsi buffer - it is the caller responsibility to free this
\param outSize  encoded dsi buffer size
\return error if any
 */
GF_Err gf_odf_vvc_cfg_write(GF_VVCConfig *cfg, u8 **outData, u32 *outSize);

/*! gets GF_VVCConfig from bitstream MPEG-4 DSI
\param bs bitstream containing the encoded VVC decoder specific info
\return the decoded VVC config
 */
GF_VVCConfig *gf_odf_vvc_cfg_read_bs(GF_BitStream *bs);

/*! gets GF_VVCConfig from MPEG-4 DSI
\param dsi encoded VVC decoder specific info
\param dsi_size encoded VVC decoder specific info size
\return the decoded VVC config
 */
GF_VVCConfig *gf_odf_vvc_cfg_read(u8 *dsi, u32 dsi_size);

/*! AV1 config constructor
\return the created AV1 config*/
GF_AV1Config *gf_odf_av1_cfg_new();

/*! AV1 config destructor
\param cfg the AV1 config to destroy*/
void gf_odf_av1_cfg_del(GF_AV1Config *cfg);

/*! writes AV1 config to buffer
\param cfg the AV1 config to write
\param outData set to an allocated encoded buffer - it is the caller responsibility to free this
\param outSize set to the encoded dsi buffer size
\return error if any
*/
GF_Err gf_odf_av1_cfg_write(GF_AV1Config *cfg, u8 **outData, u32 *outSize);

/*! writes AV1 config to bitstream
\param cfg the AV1 config to write
\param bs bitstream containing the encoded AV1 decoder specific info
\return error code if any
*/
GF_Err gf_odf_av1_cfg_write_bs(GF_AV1Config *cfg, GF_BitStream *bs);

/*! gets AV1 config from bitstream
\param bs bitstream containing the encoded AV1 decoder specific info
\return the decoded AV1 config
*/
GF_AV1Config *gf_odf_av1_cfg_read_bs(GF_BitStream *bs);

/*! gets AV1 config to bitstream
\param bs bitstream containing the encoded AV1 decoder specific info
\param size size of encoded structure in the bitstream. A value of 0 means "until the end", equivalent to gf_odf_av1_cfg_read_bs
\return the decoded AV1 config
*/
GF_AV1Config *gf_odf_av1_cfg_read_bs_size(GF_BitStream *bs, u32 size);

/*! gets AV1 config from buffer
\param dsi encoded AV1 config
\param dsi_size size of encoded AV1 config
\return the decoded AV1 config
*/
GF_AV1Config *gf_odf_av1_cfg_read(u8 *dsi, u32 dsi_size);


/*! creates a VPx (VP8, VP9) descriptor
\return a newly allocated descriptor
*/
GF_VPConfig *gf_odf_vp_cfg_new();
/*! destroys an VPx config
\param cfg the VPx config to destroy*/
void gf_odf_vp_cfg_del(GF_VPConfig *cfg);
/*! writes VPx config to bitstream
\param cfg the VPx config to write
\param bs bitstream containing the encoded VPx decoder specific info
\param is_v0 if GF_TRUE, this is a version 0 config
\return error code if any
*/
GF_Err gf_odf_vp_cfg_write_bs(GF_VPConfig *cfg, GF_BitStream *bs, Bool is_v0);
/*! writes VPx config to buffer
\param cfg the VPx config to write
\param outData set to an allocated encoded buffer - it is the caller responsibility to free this
\param outSize set to the encoded buffer size
\param is_v0 if GF_TRUE, this is a version 0 config
\return error if any
*/
GF_Err gf_odf_vp_cfg_write(GF_VPConfig *cfg, u8 **outData, u32 *outSize, Bool is_v0);
/*! gets VPx config to bitstream
\param bs bitstream containing the encoded VPx decoder specific info
\param is_v0 if GF_TRUE, this is a version 0 config
\return the decoded VPx config
*/
GF_VPConfig *gf_odf_vp_cfg_read_bs(GF_BitStream *bs, Bool is_v0);
/*! gets VPx config from buffer
\param dsi encoded VPx config
\param dsi_size size of encoded VPx config
\return the decoded VPx config
*/
GF_VPConfig *gf_odf_vp_cfg_read(u8 *dsi, u32 dsi_size);

/*! gets DolbyVision config to bitstream
\param bs bitstream containing the encoded VPx decoder specific info
\return the decoded DolbyVision config
*/
GF_DOVIDecoderConfigurationRecord *gf_odf_dovi_cfg_read_bs(GF_BitStream *bs);
/*! writes DolbyVision config to buffer
\param cfg the DolbyVision config to write
\param bs the bitstream object in which to write the config
\return error if any
*/
GF_Err gf_odf_dovi_cfg_write_bs(GF_DOVIDecoderConfigurationRecord *cfg, GF_BitStream *bs);
/*! destroys a DolbyVision config
\param cfg the DolbyVision config to destroy*/
void gf_odf_dovi_cfg_del(GF_DOVIDecoderConfigurationRecord *cfg);


/*! AC-3 and E-AC3 stream info */
typedef struct __ec3_stream
{
	/*! AC3 fs code*/
	u8 fscod;
	/*! AC3 bsid code*/
	u8 bsid;
	/*! AC3 bs mode*/
	u8 bsmod;
	/*! AC3 ac mode*/
	u8 acmod;
	/*! LF on*/
	u8 lfon;
	/*! asvc mode, only for EC3*/
	u8 asvc;
	/*! number of channels, including lfe and surround channels */
	u8 channels;
	/*! number of surround channels */
	u8 surround_channels;
	/*! number of dependent substreams, only for EC3*/
	u8 nb_dep_sub;
	/*! channel locations for dependent substreams, only for EC3*/
	u16 chan_loc;
} GF_AC3StreamInfo;

/*! AC3 config record  - see dolby specs ETSI TS 102 366 */
typedef struct __ac3_config
{
	/*! streams info - for AC3, always the first*/
	GF_AC3StreamInfo streams[8];
	/*! number of independent streams :
		1 for AC3
		max 8 for EC3
	*/
	u8 nb_streams;
	/*! indicates if frame is ec3*/
	u8 is_ec3;
	/*! if AC3 this is the bitrate code
	,	 otherwise cumulated data rate of EAC3 streams in kbps
	*/
	u16 brcode;
	/*! sample rate - all additional streams shall have the same sample rate as first independent stream in EC3*/
	u32 sample_rate;
	/*! size of the complete frame*/
	u32 framesize;
	/*! atmos EC3 flag*/
	u8 atmos_ec3_ext;
	/*! atmos complexity index*/
	u8 complexity_index_type;
} GF_AC3Config;


/*! writes Dolby AC3/EAC3 config to buffer
\param cfg the Dolby AC3 config to write
\param bs the bitstream object in which to write the config
\return error if any
*/
GF_Err gf_odf_ac3_cfg_write_bs(GF_AC3Config *cfg, GF_BitStream *bs);
/*! writes Dolby AC3/EAC3 config to buffer
\param cfg the Dolby AC3 config to write
\param data set to created output buffer, must be freed by caller
\param size set to created output buffer size
\return error if any
*/
GF_Err gf_odf_ac3_cfg_write(GF_AC3Config *cfg, u8 **data, u32 *size);


/*! parses an AC3/EC3 sample description
\param dsi the encoded config
\param dsi_len the encoded config size
\param is_ec3 indicates that the encoded config is for an EC3 track
\param cfg the AC3/EC3 config to fill
\return Error if any
*/
GF_Err gf_odf_ac3_config_parse(u8 *dsi, u32 dsi_len, Bool is_ec3, GF_AC3Config *cfg);

/*! parses an AC3/EC3 sample description from bitstream
\param bs the bitstream object
\param is_ec3 indicates that the encoded config is for an EC3 track
\param cfg the AC3/EC3 config to fill
\return Error if any
*/
GF_Err gf_odf_ac3_config_parse_bs(GF_BitStream *bs, Bool is_ec3, GF_AC3Config *cfg);



/*! Opus decoder config*/
typedef struct
{
	//! version (should be 1)
	u8 version;
	//!same value as the *Output Channel Count* field in the identification header defined in Ogg Opus [3]
	u8 OutputChannelCount;
	//!The value of the PreSkip field shall be at least 80 milliseconds' worth of PCM samples even when removing any number of Opus samples which may or may not contain the priming samples. The PreSkip field is not used for discarding the priming samples at the whole playback at all since it is informative only, and that task falls on the Edit List Box.
	u16 PreSkip;
	//! The InputSampleRate field shall be set to the same value as the *Input Sample Rate* field in the identification header defined in Ogg Opus
	u32 InputSampleRate;
	//! The OutputGain field shall be set to the same value as the *Output Gain* field in the identification header define in Ogg Opus [3]. Note that the value is stored as 8.8 fixed-point.
	s16 OutputGain;
	//! The ChannelMappingFamily field shall be set to the same value as the *Channel Mapping Family* field in the identification header defined in Ogg Opus [3]. Note that the value 255 may be used for an alternative to map channels by ISO Base Media native mapping. The details are described in 4.5.1.
	u8 ChannelMappingFamily;

	//! The StreamCount field shall be set to the same value as the *Stream Count* field in the identification header defined in Ogg Opus [3].
	u8 StreamCount;
	//! The CoupledCount field shall be set to the same value as the *Coupled Count* field in the identification header defined in Ogg Opus [3].
	u8 CoupledCount;
	//! The ChannelMapping field shall be set to the same octet string as *Channel Mapping* field in the identi- fication header defined in Ogg Opus [3].
	u8 ChannelMapping[255];
} GF_OpusConfig;


/*! writes Opus  config to buffer
\param cfg the Opus config to write
\param bs the bitstream object in which to write the config
\return error if any
*/
GF_Err gf_odf_opus_cfg_write_bs(GF_OpusConfig *cfg, GF_BitStream *bs);

/*! writes Opus config to buffer
\param cfg the Opus config to write
\param data set to created output buffer, must be freed by caller
\param size set to created output buffer size
\return error if any
*/
GF_Err gf_odf_opus_cfg_write(GF_OpusConfig *cfg, u8 **data, u32 *size);

/*! parses an Opus sample description
\param dsi the encoded config
\param dsi_len the encoded config size
\param cfg the Opus config to fill
\return Error if any
*/
GF_Err gf_odf_opus_cfg_parse(u8 *dsi, u32 dsi_len, GF_OpusConfig *cfg);

/*! parses an Opus sample description from bitstream
\param bs the bitstream object
\param cfg the Opus config to fill
\return Error if any
*/
GF_Err gf_odf_opus_cfg_parse_bs(GF_BitStream *bs, GF_OpusConfig *cfg);

/*! destroy the descriptors in a list but not the list
\param descList descriptor list to destroy
\return error if any
 */
GF_Err gf_odf_desc_list_del(GF_List *descList);

/*! use this function to decode a standalone descriptor
the raw descriptor MUST be formatted with tag and size field!!!
a new desc is created and you must delete it when done
\param raw_desc encoded descriptor to decode
\param descSize size of descriptor to decode
\param outDesc output decoded descriptor - it is the caller responsibility to free this
\return error if any
*/
GF_Err gf_odf_desc_read(u8 *raw_desc, u32 descSize, GF_Descriptor **outDesc);

/*! use this function to encode a standalone descriptor
the desc will be formatted with tag and size field
the output buffer is allocated and you must delete it when done
\param desc descriptor to encode
\param outEncDesc output encoded descriptor - it is the caller responsibility to free this
\param outSize size of encoded descriptor
\return error if any
 */
GF_Err gf_odf_desc_write(GF_Descriptor *desc, u8 **outEncDesc, u32 *outSize);

/*! use this function to encode a standalone descriptor in a bitstream object
the desc will be formatted with tag and size field
\param desc descriptor to encode
\param bs the bitstream object in write mode
\return error if any
 */
GF_Err gf_odf_desc_write_bs(GF_Descriptor *desc, GF_BitStream *bs);

/*! use this function to get the size of a standalone descriptor (including tag and size fields)
\param desc descriptor to encode
\return 0 if error or encoded desc size otherwise*/
u32 gf_odf_desc_size(GF_Descriptor *desc);

/*! duplicate descriptors
\param inDesc descriptor to copy
\param outDesc copied descriptor - it is the caller responsibility to free this
\return error if any
 */
GF_Err gf_odf_desc_copy(GF_Descriptor *inDesc, GF_Descriptor **outDesc);

/*! Adds a descriptor to a parent one. Handles internally what desc can be added to another desc
and adds it. NO DUPLICATION of the descriptor, so
once a desc is added to its parent, destroying the parent WILL DESTROY
this descriptor
\param parentDesc parent descriptor
\param newDesc descriptor to add to parent
\return error if any
 */
GF_Err gf_odf_desc_add_desc(GF_Descriptor *parentDesc, GF_Descriptor *newDesc);

/*! Since IPMP V2, we introduce a new set of functions to read / write a list of descriptors
that have no containers (a bit like an OD command, but for descriptors)
This is useful for IPMPv2 DecoderSpecificInfo which contains a set of IPMP_Declarators
As it could be used for other purposes we keep it generic
you must create the list yourself, the functions just encode/decode from/to the list*/

/*! uncompress an encoded list of descriptors. You must pass an empty GF_List structure
to know exactly what was in the buffer
\param raw_list encoded list of descriptors
\param raw_size size of the encoded list of descriptors
\param descList list in which the decoded descriptors will be placed
\return error if any
*/
GF_Err gf_odf_desc_list_read(u8 *raw_list, u32 raw_size, GF_List *descList);

/*! compress all descriptors in the list into a single buffer. The buffer is allocated
by the lib and must be destroyed by your app
you must pass (outEncList != NULL  && *outEncList == NULL)
\param descList list of descriptors to be encoded
\param outEncList buffer of encoded descriptors
\param outSize size of buffer of encoded descriptors
\return error if any
 */
GF_Err gf_odf_desc_list_write(GF_List *descList, u8 **outEncList, u32 *outSize);

/*! returns size of encoded desc list
\param descList list of descriptors to be encoded
\param outSize size of buffer of encoded descriptors
\return error if any
 */
GF_Err gf_odf_desc_list_size(GF_List *descList, u32 *outSize);

/*! DTS audio configuration*/
typedef struct
{
	u32 SamplingFrequency;
	u32 MaxBitrate;
	u32 AvgBitrate;
	u8 SampleDepth;
	u8 FrameDuration;
	u8 StreamConstruction;
	u8 CoreLFEPresent;
	u8 CoreLayout;
	u16 CoreSize;
	u8 StereoDownmix;
	u8 RepresentationType;
	u16 ChannelLayout;
	u8 MultiAssetFlag;
	u8 LBRDurationMod;
} GF_DTSConfig;


/*! UDTS audio configuration*/
typedef struct
{
	u8 DecoderProfileCode;
	u8 FrameDurationCode;
	u8 MaxPayloadCode;
	u8 NumPresentationsCode;
	u32 ChannelMask;
	u8 BaseSamplingFrequencyCode;
	u8 SampleRateMod;
	u8 RepresentationType;
	u8 StreamIndex;
	u8 ExpansionBoxPresent;
	u8 IDTagPresent[32];
	u8 *PresentationIDTagData;
	u16 PresentationIDTagDataSize;
	u8 *ExpansionBoxData;
	u32 ExpansionBoxDataSize;
} GF_UDTSConfig;


//! @cond Doxygen_Suppress

#ifndef GPAC_MINIMAL_ODF


/************************************************************
		QoS Qualifiers Functions
************************************************************/

/*! QoS Qualifiers constructor
\param tag tag of QoS descriptor to create
\return created QoS descriptor
 */
GF_QoS_Default *gf_odf_qos_new(u8 tag);
/*! QoS Qualifiers destructor
\param qos descriptor to destroy. The pointer is set back to NULL upon destruction
\return error if any
 */
GF_Err gf_odf_qos_del(GF_QoS_Default **qos);

/*! READ/WRITE functions: QoS qualifiers are special descriptors but follow the same rules as descriptors.
therefore, use gf_odf_desc_read and gf_odf_desc_write for QoS*/

/*! Adds a QoS qualificator to a parent QoS descriptor
\param desc parent QoS descriptor
\param qualif  QoS qualificator
\return error if any
 */
GF_Err gf_odf_qos_add_qualif(GF_QoS_Descriptor *desc, GF_QoS_Default *qualif);



/*
	OCI Stream AU is a list of OCI event (like OD AU is a list of OD commands)
*/

typedef struct __tag_oci_event OCIEvent;

OCIEvent *gf_oci_event_new(u16 EventID);
void gf_oci_event_del(OCIEvent *event);

GF_Err gf_oci_event_set_start_time(OCIEvent *event, u8 Hours, u8 Minutes, u8 Seconds, u8 HundredSeconds, u8 IsAbsoluteTime);
GF_Err gf_oci_event_set_duration(OCIEvent *event, u8 Hours, u8 Minutes, u8 Seconds, u8 HundredSeconds);
GF_Err gf_oci_event_add_desc(OCIEvent *event, GF_Descriptor *oci_desc);

GF_Err gf_oci_event_get_id(OCIEvent *event, u16 *ID);
GF_Err gf_oci_event_get_start_time(OCIEvent *event, u8 *Hours, u8 *Minutes, u8 *Seconds, u8 *HundredSeconds, u8 *IsAbsoluteTime);
GF_Err gf_oci_event_get_duration(OCIEvent *event, u8 *Hours, u8 *Minutes, u8 *Seconds, u8 *HundredSeconds);
u32 gf_oci_event_get_desc_count(OCIEvent *event);
GF_Descriptor *gf_oci_event_get_desc(OCIEvent *event, u32 DescIndex);
GF_Err gf_oci_event_rem_desc(OCIEvent *event, u32 DescIndex);



typedef struct __tag_oci_codec OCICodec;

/*construction / destruction
IsEncoder specifies an OCI Event encoder
version is for future extensions, and only 0x01 is valid for now*/
OCICodec *gf_oci_codec_new(u8 IsEncoder, u8 Version);
void gf_oci_codec_del(OCICodec *codec);

/*				ENCODER FUNCTIONS
add a command to the codec event list.
The event WILL BE DESTROYED BY THE CODEC after encoding*/
GF_Err gf_oci_codec_add_event(OCICodec *codec, OCIEvent *event);

/*encode AU. The memory allocation is done in place
WARNING: once this function called, the codec event List is empty
and events destroyed
you must set *outAU = NULL*/
GF_Err gf_oci_codec_encode(OCICodec *codec, u8 **outAU, u32 *au_length);



/*Decoder: decode the previously set-up AU
the input buffer is cleared once decoded*/
GF_Err gf_oci_codec_decode(OCICodec *codec, u8 *au, u32 au_length);

/*get the first OCI Event in the list. Once called, the event is removed
from the event list. Return NULL when the event List is empty
you MUST delete events */
OCIEvent *gf_oci_codec_get_event(OCICodec *codec);

/*! Dumps an OCI event
\param ev OCI event to dump
\param trace destination file for dumping
\param indent number of spaces to use as base index
\param XMTDump if GF_TRUE dumpos as XMT, otherwise as BT
 */
GF_Err gf_oci_dump_event(OCIEvent *ev, FILE *trace, u32 indent, Bool XMTDump);
/*! Dumps an OCI AU
\param version version of the OCI stream
\param au OCI AU to dump
\param au_length size of the OCI AU to dump
\param trace destination file for dumping
\param indent number of spaces to use as base index
\param XMTDump if GF_TRUE dumpos as XMT, otherwise as BT
 */
GF_Err gf_oci_dump_au(u8 version, u8 *au, u32 au_length, FILE *trace, u32 indent, Bool XMTDump);

#endif /*GPAC_MINIMAL_ODF*/

//! @endcond

#ifndef GPAC_DISABLE_OD_DUMP

/*! Dumps an OD AU
\param com OD command to dump
\param trace destination file for dumping
\param indent number of spaces to use as base index
\param XMTDump if GF_TRUE dumpos as XMT, otherwise as BT
\return error if any
 */
GF_Err gf_odf_dump_com(GF_ODCom *com, FILE *trace, u32 indent, Bool XMTDump);
/*! Dumps an OD Descriptor
\param desc descriptor to dump
\param trace destination file for dumping
\param indent number of spaces to use as base index
\param XMTDump if GF_TRUE dumpos as XMT, otherwise as BT
\return error if any
 */
GF_Err gf_odf_dump_desc(GF_Descriptor *desc, FILE *trace, u32 indent, Bool XMTDump);
/*! Dumps an OD Descriptor
\param commandList descriptor list to dump
\param trace destination file for dumping
\param indent number of spaces to use as base index
\param XMTDump if GF_TRUE dumpos as XMT, otherwise as BT
\return error if any
 */
GF_Err gf_odf_dump_com_list(GF_List *commandList, FILE *trace, u32 indent, Bool XMTDump);

#endif /*GPAC_DISABLE_OD_DUMP*/


/*! Gets descriptor tag by name
\param descName target descriptor name
\return descriptor tag or 0 if error
 */
u32 gf_odf_get_tag_by_name(char *descName);

/*! field type for OD/QoS/IPMPX/etc*/
typedef enum
{
	/*! regular type*/
	GF_ODF_FT_DEFAULT = 0,
	/*! single descriptor type*/
	GF_ODF_FT_OD = 1,
	/*! descriptor list type*/
	GF_ODF_FT_OD_LIST = 2,
	/*! IPMP Data type*/
	GF_ODF_FT_IPMPX = 3,
	/*! IPMP Data list type*/
	GF_ODF_FT_IPMPX_LIST = 4,
	/*! IPMP ByteArray type*/
	GF_ODF_FT_IPMPX_BA = 5,
	/*! IPMP ByteArray list type*/
	GF_ODF_FT_IPMPX_BA_LIST = 6
} GF_ODF_FieldType;
/*! Gets ODF field type by name
\param desc target descriptor
\param fieldName  descriptor field name
\return the descriptor field type
*/
GF_ODF_FieldType gf_odf_get_field_type(GF_Descriptor *desc, char *fieldName);

/*! Set non-descriptor field value - value string shall be presented without ' or " characters
\param desc target descriptor
\param fieldName descriptor field name
\param val field value to parse
\return error if any
 */
GF_Err gf_odf_set_field(GF_Descriptor *desc, char *fieldName, char *val);

#ifndef GPAC_MINIMAL_ODF

//! @cond Doxygen_Suppress


/*
	IPMPX extensions - IPMP Data only (messages are not supported yet)
*/

/*! IPMPX base buffer object*/
typedef struct
{
	u32 length;
	u8 *data;
} GF_IPMPX_ByteArray;

#define GF_IPMPX_AUTH_DESC	\
	u8 tag;	\

/*! IPMPX authentication descriptor*/
typedef struct
{
	GF_IPMPX_AUTH_DESC
} GF_IPMPX_Authentication;

enum
{
	GF_IPMPX_AUTH_Forbidden_Tag = 0x00,
	GF_IPMPX_AUTH_AlgorithmDescr_Tag = 0x01,
	GF_IPMPX_AUTH_KeyDescr_Tag = 0x02
};

/*! IPMPX authentication key descriptor*/
typedef struct
{
	GF_IPMPX_AUTH_DESC
	u8 *keyBody;
	u32 keyBodyLength;
} GF_IPMPX_AUTH_KeyDescriptor;

/*! IPMPX authentication algorithm descriptor*/
typedef struct
{
	GF_IPMPX_AUTH_DESC
	/*used if no specAlgoID*/
	u16 regAlgoID;
	GF_IPMPX_ByteArray *specAlgoID;
	GF_IPMPX_ByteArray *OpaqueData;
} GF_IPMPX_AUTH_AlgorithmDescriptor;


/*! IPMPX data message types*/
enum
{
	GF_IPMPX_OPAQUE_DATA_TAG = 0x01,
	GF_IPMPX_AUDIO_WM_INIT_TAG = 0x02,
	GF_IPMPX_VIDEO_WM_INIT_TAG = 0x03,
	GF_IPMPX_SEL_DEC_INIT_TAG = 0x04,
	GF_IPMPX_KEY_DATA_TAG = 0x05,
	GF_IPMPX_AUDIO_WM_SEND_TAG = 0x06,
	GF_IPMPX_VIDEO_WM_SEND_TAG = 0x07,
	GF_IPMPX_RIGHTS_DATA_TAG = 0x08,
	GF_IPMPX_SECURE_CONTAINER_TAG = 0x09,
	GF_IPMPX_ADD_TOOL_LISTENER_TAG = 0x0A,
	GF_IPMPX_REMOVE_TOOL_LISTENER_TAG = 0x0B,
	GF_IPMPX_INIT_AUTHENTICATION_TAG = 0x0C,
	GF_IPMPX_MUTUAL_AUTHENTICATION_TAG = 0x0D,
	GF_IPMPX_USER_QUERY_TAG = 0x0E,
	GF_IPMPX_USER_RESPONSE_TAG = 0x0F,
	GF_IPMPX_PARAMETRIC_DESCRIPTION_TAG = 0x10,
	GF_IPMPX_PARAMETRIC_CAPS_QUERY_TAG = 0x11,
	GF_IPMPX_PARAMETRIC_CAPS_RESPONSE_TAG = 0x12,
	/*NO ASSOCIATED STRUCTURE*/
	GF_IPMPX_GET_TOOLS_TAG = 0x13,
	GF_IPMPX_GET_TOOLS_RESPONSE_TAG = 0x14,
	GF_IPMPX_GET_TOOL_CONTEXT_TAG = 0x15,
	GF_IPMPX_GET_TOOL_CONTEXT_RESPONSE_TAG = 0x16,
	GF_IPMPX_CONNECT_TOOL_TAG = 0x17,
	GF_IPMPX_DISCONNECT_TOOL_TAG = 0x18,
	GF_IPMPX_NOTIFY_TOOL_EVENT_TAG = 0x19,
	GF_IPMPX_CAN_PROCESS_TAG = 0x1A,
	GF_IPMPX_TRUST_SECURITY_METADATA_TAG = 0x1B,
	GF_IPMPX_TOOL_API_CONFIG_TAG = 0x1C,

	/*ISMA*/
	GF_IPMPX_ISMACRYP_TAG = 0xD0,

	/*intern ones for parsing (not real datas)*/
	GF_IPMPX_TRUSTED_TOOL_TAG = 0xA1,
	GF_IPMPX_TRUST_SPECIFICATION_TAG = 0xA2,
	/*emulate algo descriptors as base IPMP classes for parsing...*/
	GF_IPMPX_ALGORITHM_DESCRIPTOR_TAG = 0xA3,
	GF_IPMPX_KEY_DESCRIPTOR_TAG = 0xA4,
	GF_IPMPX_PARAM_DESCRIPTOR_ITEM_TAG = 0xA5,
	GF_IPMPX_SEL_ENC_BUFFER_TAG = 0xA6,
	GF_IPMPX_SEL_ENC_FIELD_TAG = 0xA7
};

typedef char GF_IPMPX_Date[5];


#define GF_IPMPX_DATA_BASE	\
	u8 tag;	\
	u8 Version;	\
	u8 dataID;	\

/*! Base IPMPX data*/
typedef struct
{
	GF_IPMPX_DATA_BASE
} GF_IPMPX_Data;

/*! IPMPX Init Authentiactaion data*/
typedef struct
{
	GF_IPMPX_DATA_BASE
	u32 Context;
	u8 AuthType;
} GF_IPMPX_InitAuthentication;

/*! IPMPX Trust Specification data
NOT a real DATA, only used as data for parsing
 */
typedef struct
{
	GF_IPMPX_DATA_BASE
	GF_IPMPX_Date startDate;
	u8 attackerProfile;
	u32 trustedDuration;
	GF_IPMPX_ByteArray	*CCTrustMetadata;
} GF_IPMPX_TrustSpecification;

/*! IPMPX Trusted Tool data
 NOT a real DATA, only used as data for parsing
 */
typedef struct
{
	GF_IPMPX_DATA_BASE
	bin128 toolID;
	GF_IPMPX_Date AuditDate;
	GF_List *trustSpecifications;
} GF_IPMPX_TrustedTool;

/*! IPMPX Trust Security Metadata data
*/
typedef struct _ipmpx_TrustSecurityMetadata
{
	GF_IPMPX_DATA_BASE
	GF_List *TrustedTools;
} GF_IPMPX_TrustSecurityMetadata;


/*! IPMPX Mutual Authentication data
*/
typedef struct
{
	GF_IPMPX_DATA_BASE
	Bool failedNegotiation;

	GF_List *candidateAlgorithms;
	GF_List *agreedAlgorithms;
	GF_IPMPX_ByteArray *AuthenticationData;

	/*inclAuthCodes will be set if any of the members is set (cf spec...)*/
	u32 certType;
	/*GF_IPMPX_ByteArray list*/
	GF_List *certificates;
	GF_IPMPX_AUTH_KeyDescriptor *publicKey;
	GF_IPMPX_ByteArray *opaque;
	GF_IPMPX_TrustSecurityMetadata *trustData;
	GF_IPMPX_ByteArray *authCodes;
} GF_IPMPX_MutualAuthentication;

/*! IPMPX Secure Container data
*/
typedef struct
{
	GF_IPMPX_DATA_BASE
	/*if set MAC is part of the encrypted data*/
	Bool isMACEncrypted;

	GF_IPMPX_ByteArray *encryptedData;
	GF_IPMPX_Data *protectedMsg;
	GF_IPMPX_ByteArray *MAC;
} GF_IPMPX_SecureContainer;

/*! IPMPX Tool Response container
*/
typedef struct
{
	GF_IPMPX_DATA_BASE
	GF_List *ipmp_tools;
} GF_IPMPX_GetToolsResponse;

/*! IPMPX Parametric Description Item data
*/
typedef struct
{
	GF_IPMPX_DATA_BASE
	GF_IPMPX_ByteArray	*main_class;
	GF_IPMPX_ByteArray	*subClass;
	GF_IPMPX_ByteArray	*typeData;
	GF_IPMPX_ByteArray	*type;
	GF_IPMPX_ByteArray	*addedData;
} GF_IPMPX_ParametricDescriptionItem;

/*! IPMPX Parametric Description data
 */
typedef struct _tagIPMPXParamDesc
{
	GF_IPMPX_DATA_BASE
	GF_IPMPX_ByteArray *descriptionComment;
	u8 majorVersion;
	u8 minorVersion;
	/*list of GF_IPMPX_ParametricDescriptionItem*/
	GF_List *descriptions;
} GF_IPMPX_ParametricDescription;

/*! IPMPX Tool Capability Query data
*/
typedef struct
{
	GF_IPMPX_DATA_BASE
	GF_IPMPX_ParametricDescription *description;
} GF_IPMPX_ToolParamCapabilitiesQuery;

/*! IPMPX Tool Capability Response data
 */
typedef struct
{
	GF_IPMPX_DATA_BASE
	Bool capabilitiesSupported;
} GF_IPMPX_ToolParamCapabilitiesResponse;


/*! IPMPX Connected Tool data
 */
typedef struct
{
	GF_IPMPX_DATA_BASE
	GF_IPMP_Descriptor *toolDescriptor;
} GF_IPMPX_ConnectTool;

/*! IPMPX Disconnected Tool data
 */
typedef struct
{
	GF_IPMPX_DATA_BASE
	u32 IPMP_ToolContextID;
} GF_IPMPX_DisconnectTool;


/*! IPMPX Tool Context ID query data
 */
typedef struct
{
	GF_IPMPX_DATA_BASE
	u8 scope;
	u16 IPMP_DescriptorIDEx;
} GF_IPMPX_GetToolContext;


/*! IPMPX Get Tool response data
 */
typedef struct
{
	GF_IPMPX_DATA_BASE
	u16 OD_ID;
	u16 ESD_ID;
	u32 IPMP_ToolContextID;
} GF_IPMPX_GetToolContextResponse;

/*! GF_IPMPX_LISTEN_Types*/
typedef enum
{
	GF_IPMPX_LISTEN_CONNECTED = 0x00,
	GF_IPMPX_LISTEN_CONNECTIONFAILED = 0x01,
	GF_IPMPX_LISTEN_DISCONNECTED = 0x02,
	GF_IPMPX_LISTEN_DISCONNECTIONFAILED = 0x03,
	GF_IPMPX_LISTEN_WATERMARKDETECTED = 0x04
} GF_IPMPX_ListenType;

/*! IPMPX Add Tool Listener data
 */
typedef struct
{
	GF_IPMPX_DATA_BASE
	u8 scope;
	/*events to listen to*/
	u8 eventTypeCount;
	GF_IPMPX_ListenType eventType[10];
} GF_IPMPX_AddToolNotificationListener;

/*! IPMPX Remove Tool Listener data
 */
typedef struct
{
	GF_IPMPX_DATA_BASE
	u8 eventTypeCount;
	GF_IPMPX_ListenType eventType[10];
} GF_IPMPX_RemoveToolNotificationListener;

/*! IPMPX Tool Notify Event data
 */
typedef struct
{
	GF_IPMPX_DATA_BASE
	u16 OD_ID;
	u16 ESD_ID;
	u8 eventType;
	u32 IPMP_ToolContextID;
} GF_IPMPX_NotifyToolEvent;

/*! IPMPX Can Process data
 */
typedef struct
{
	GF_IPMPX_DATA_BASE
	Bool canProcess;
} GF_IPMPX_CanProcess;

/*! IPMPX Opaque Data container data
 */
typedef struct
{
	GF_IPMPX_DATA_BASE
	GF_IPMPX_ByteArray *opaqueData;
} GF_IPMPX_OpaqueData;


/*! IPMPX Key data
 */
typedef struct
{
	GF_IPMPX_DATA_BASE
	GF_IPMPX_ByteArray *keyBody;
	/*flags meaning
	hasStartDTS = 1;
	hasStartPacketID = 1<<1;
	hasExpireDTS = 1<<2;
	hasExpirePacketID = 1<<3
	*/
	u32 flags;

	u64 startDTS;
	u32 startPacketID;
	u64 expireDTS;
	u32 expirePacketID;
	GF_IPMPX_ByteArray *OpaqueData;
} GF_IPMPX_KeyData;

/*! IPMPX Rights data
 */
typedef struct
{
	GF_IPMPX_DATA_BASE
	GF_IPMPX_ByteArray *rightsInfo;
} GF_IPMPX_RightsData;


/*! IPMPX Selective Encryption Buffer data
	 not a real GF_IPMPX_Data in spec, but emulated as if for parsing
 */
typedef struct
{
	GF_IPMPX_DATA_BASE
	bin128 cipher_Id;
	u8 syncBoundary;
	/*block mode if stream cypher info is NULL*/
	u8 mode;
	u16 blockSize;
	u16 keySize;
	GF_IPMPX_ByteArray *Stream_Cipher_Specific_Init_Info;
} GF_IPMPX_SelEncBuffer;

/*! IPMPX Selective Encryption Field data
not a real GF_IPMPX_Data in spec, but emulated as if for parsing
 */
typedef struct
{
	GF_IPMPX_DATA_BASE
	u8 field_Id;
	u8 field_Scope;
	u8 buf;

	u16 mappingTableSize;
	u16 *mappingTable;
	GF_IPMPX_ByteArray *shuffleSpecificInfo;
} GF_IPMPX_SelEncField;


/*! IPMPX mediaTypeExtension*/
enum
{
	GF_IPMPX_SE_MT_ISO_IEC = 0x00,
	GF_IPMPX_SE_MT_ITU = 0x01
	                     /*the rest is reserved or forbidden*/
};

/*! IPMPX compliance*/
enum
{
	GF_IPMPX_SE_COMP_FULLY = 0x00,
	GF_IPMPX_SE_COMP_VIDEO_PACKETS = 0x01,
	GF_IPMPX_SE_COMP_VIDEO_VOP = 0x02,
	GF_IPMPX_SE_COMP_VIDEO_NONE = 0x03,
	GF_IPMPX_SE_COMP_VIDEO_GOB = 0x04,
	/*0x05-2F	ISO Reserved for video*/
	GF_IPMPX_SE_COMP_AAC_DF = 0x30,
	GF_IPMPX_SE_COMP_AAC_NONE = 0x31
	                            /*
	                            0x32 -  0x5F	ISO Reserved for audio
	                            0x60 - 0xCF	ISO Reserved
	                            0xD0 - 0xFE	User Defined
	                            0xFF	Forbidden
	                            */
};

/*! IPMPX syncBoundary*/
enum
{
	GF_IPMPX_SE_SYNC_VID7EO_PACKETS = 0x00,
	GF_IPMPX_SE_SYNC_VIDEO_VOP = 0x01,
	GF_IPMPX_SE_SYNC_VIDEO_GOV = 0x02,
	/*0x03-2F	ISO Reserved for video,*/
	GF_IPMPX_SE_SYNC_AAC_DF = 0x30
	                          /*0x31 -  0x5F	ISO Reserved for audio
	                          0x60 - 0xCF	ISO Reserved
	                          0xD0 - 0xFE	User Defined
	                          0xFF	Forbidden
	                          */
};

/*! IPMPX field_Id for selective encryption*/
enum
{
	GF_IPMPX_SE_FID_VIDEO_MV = 0x00,
	GF_IPMPX_SE_FID_VIDEO_DC = 0x01,
	GF_IPMPX_SE_FID_VIDEO_DCT_SIGN = 0x02,
	GF_IPMPX_SE_FID_VIDEO_DQUANT = 0x03,
	GF_IPMPX_SE_FID_VIDEO_DCT_COEF = 0x04,
	GF_IPMPX_SE_FID_VIDEO_ALL = 0x05,
	/*0x06-2F	ISO Reserved for video*/
	GF_IPMPX_SE_FID_AAC_SIGN = 0x30,
	GF_IPMPX_SE_FID_AAC_CODEWORDS = 0x31,
	GF_IPMPX_SE_FID_AAC_SCALE = 0x32
	                            /*0x32 -  0x5F	ISO Reserved for audio
	                            0x60 - 0xCF	ISO Reserved
	                            0xD0 - 0xFE	User Defined
	                            0xFF	Forbidden*/
};


/*! IPMPX Selective Encryption Init data
 */
typedef struct
{
	GF_IPMPX_DATA_BASE
	u8 mediaTypeExtension;
	u8 mediaTypeIndication;
	u8 profileLevelIndication;
	u8 compliance;

	GF_List *SelEncBuffer;

	GF_List *SelEncFields;

	u16 RLE_DataLength;
	u16 *RLE_Data;
} GF_IPMPX_SelectiveDecryptionInit;


/*! IPMPX watermark init ops*/
enum
{
	GF_IPMPX_WM_INSERT = 0,
	GF_IPMPX_WM_EXTRACT = 1,
	GF_IPMPX_WM_REMARK = 2,
	GF_IPMPX_WM_DETECT_COMPRESSION = 3
};

/*! IPMPX Watermark Init data
used for both audio and video WM init
 */
typedef struct
{
	GF_IPMPX_DATA_BASE
	/*
	for audio: PCM defined (0x01) and all audio objectTypeIndications
	for video: YUV defined (0x01) and all visual objectTypeIndications
	*/
	u8 inputFormat;
	u8 requiredOp;

	/*valid for audio WM, inputFormat=0x01*/
	u8 nChannels;
	u8 bitPerSample;
	u32 frequency;

	/*valid for video WM, inputFormat=0x01*/
	u16 frame_horizontal_size;
	u16 frame_vertical_size;
	u8 chroma_format;

	u32 wmPayloadLen;
	u8 *wmPayload;

	u16 wmRecipientId;

	u32 opaqueDataSize;
	u8 *opaqueData;
} GF_IPMPX_WatermarkingInit;



/*! IPMPX Watermark status*/
enum
{
	GF_IPMPX_WM_PAYLOAD = 0,
	GF_IPMPX_WM_NOPAYLOAD = 1,
	GF_IPMPX_WM_NONE = 2,
	GF_IPMPX_WM_UNKNOWN = 3
};

/*! IPMPX compression status*/
enum
{
	GF_IPMPX_WM_COMPRESSION = 0,
	GF_IPMPX_WM_NO_COMPRESSION = 1,
	GF_IPMPX_WM_COMPRESSION_UNKNOWN = 2
};

/*! IPMPX Send Watermark data
 */
typedef struct
{
	GF_IPMPX_DATA_BASE
	u8 wm_status;
	u8 compression_status;
	/*if payload is set, status is FORCED to AUDIO_GF_IPMPX_WM_PAYLOAD*/
	GF_IPMPX_ByteArray *payload;
	GF_IPMPX_ByteArray *opaqueData;
} GF_IPMPX_SendWatermark;


/*! IPMPX Tool API config data
 */
typedef struct
{
	GF_IPMPX_DATA_BASE
	/*GPAC only supports non-0 IDs*/
	u32 Instantiation_API_ID;
	u32 Messaging_API_ID;
	GF_IPMPX_ByteArray *opaqueData;
} GF_IPMPX_ToolAPI_Config;

/*! IPMPX ISMACryp data
*/
typedef struct
{
	GF_IPMPX_DATA_BASE
	u8 cryptoSuite;
	u8 IV_length;
	Bool use_selective_encryption;
	u8 key_indicator_length;
} GF_IPMPX_ISMACryp;


/* constructor */
GF_IPMPX_Data *gf_ipmpx_data_new(u8 tag);
/* destructor */
void gf_ipmpx_data_del(GF_IPMPX_Data *p);

/* parse from bitstream */
GF_Err gf_ipmpx_data_parse(GF_BitStream *bs, GF_IPMPX_Data **out_data);
/*! get IPMP_Data contained size (eg without tag & sizeofinstance)*/
u32 gf_ipmpx_data_size(GF_IPMPX_Data *p);
/*! get fulml IPMP_Data encoded size (eg with tag & sizeofinstance)*/
u32 gf_ipmpx_data_full_size(GF_IPMPX_Data *p);
/*! writes IPMP_Data to buffer*/
GF_Err gf_ipmpx_data_write(GF_BitStream *bs, GF_IPMPX_Data *_p);

/*! returns GF_IPMPX_Tag based on name*/
u8 gf_ipmpx_get_tag(char *dataName);
/*! return values: cf above */
u32 gf_ipmpx_get_field_type(GF_IPMPX_Data *p, char *fieldName);
GF_Err gf_ipmpx_set_field(GF_IPMPX_Data *desc, char *fieldName, char *val);
/*! assign subdata*/
GF_Err gf_ipmpx_set_sub_data(GF_IPMPX_Data *desc, char *fieldName, GF_IPMPX_Data *subdesc);
/*! assign bytearray*/
GF_Err gf_ipmpx_set_byte_array(GF_IPMPX_Data *p, char *field, char *str);

/*! ipmpx dumper*/
GF_Err gf_ipmpx_dump_data(GF_IPMPX_Data *_p, FILE *trace, u32 indent, Bool XMTDump);

//! @endcond

#endif /*GPAC_MINIMAL_ODF*/

/*! @} */

#ifdef __cplusplus
}
#endif

#endif	/*_GF_MPEG4_ODF_H_*/

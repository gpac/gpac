/*
 * Copyright (c) TELECOM ParisTech 2011
 */

#ifndef _GF_AIT_H_
#define _GF_AIT_H_

#ifdef __cplusplus
extern "C" {
#endif

/*!
\file <gpac/ait.h>
\brief Specific extensions for handling AIT in MPEG-2 TS.
 */
	
#include <gpac/mpegts.h>
#include <gpac/dsmcc.h>
#include <string.h>
#include <gpac/bitstream.h>


#ifndef GPAC_DISABLE_MPEG2TS


#define AIT_SECTION_LENGTH_MAX 1021
#define APPLICATION_TYPE_HTTP_APPLICATION 16
#define DSMCC_SECTION_LENGTH_MAX 4093

/*! AIT descriptor tags*/
typedef enum {
	APPLICATION_DESCRIPTOR = 0x00,
	APPLICATION_NAME_DESCRIPTOR = 0x01,
	TRANSPORT_PROTOCOL_DESCRIPTOR = 0x02,
	SIMPLE_APPLICATION_LOCATION_DESCRIPTOR = 0x15,
	APPLICATION_USAGE_DESCRIPTOR = 0x16,
	APPLICATION_BOUNDARY_DESCRIPTOR = 0x17,
} DESCRIPTOR_TAG;

/*! AIT Application Control Code*/
enum ApplicationControlCode {
	AUTOSTART =				0x01,
	PRESENT = 				0x02,
	DESTROY =				0x03,
	KILL =					0x04,
	PREFETCH =				0x05,
	REMOTE = 				0x06,
	DISABLED =				0x07,
	PLAYBACK_AUTOSTART =	0x08
};

/*! AIT Transport Type*/
enum TransportType {
	BROADCAST =		0x01,
	BROADBAND = 	0x03
};

/*! AIT table*/
typedef struct
{
	u32 pid;
	u32 service_id;
	u8 table_id;
	Bool section_syntax_indicator;
	u16 section_length;
	Bool test_application_flag;
	u16 application_type;
	u8 version_number;
	Bool current_next_indicator;
	u8 section_number;
	u8 last_section_number;
	u16 common_descriptors_length;
	GF_List * common_descriptors;
	u16 application_loop_length;
	GF_List * application_decoded;
	u32 CRC_32;

} GF_M2TS_AIT;

/*! AIT elementary stream / section filter*/
typedef struct
{
	ABSTRACT_ES
	GF_M2TS_SectionFilter *sec;

} GF_M2TS_AIT_CARRY;


/*! AIT single application descriptor*/
typedef struct
{
	u32 organisation_id;
	u16 application_id;
	u8 application_control_code;
	u16 application_descriptors_loop_length;
	GF_List * application_descriptors;
	u8 application_descriptors_id[50];
	u8 index_app_desc_id;

} GF_M2TS_AIT_APPLICATION_DECODE;


/*! AIT protocol identifier*/
typedef enum {
	FUTURE_USE = 0x00,
	CAROUSEL = 0x01,
	RESERVED = 0x02,
	TRANSPORT_HTTP = 0x03,
	DVB_USE = 0x04,
	TO_REGISTER = 0x100,
} PROTOCOL_ID;

/*! Application descriptor*/
typedef struct
{
	u8 descriptor_tag;
	u8 descriptor_length;
	u8 application_profiles_length;
	u16 application_profile;
	u8 version_major;
	u8 version_minor;
	u8 version_micro;
	Bool service_bound_flag;
	u8 visibility;
	u8 application_priority;
	u8 transport_protocol_label[5];

} GF_M2TS_APPLICATION_DESCRIPTOR;

/*! Application usage descriptor*/
typedef struct
{
	u8 descriptor_tag;
	u8 descriptor_length;
	u8 usage_type;

} GF_M2TS_APPLICATION_USAGE;

/*! Application location descriptor*/
typedef struct
{
	u8 descriptor_tag;
	u8 descriptor_length;
	char* initial_path_bytes;

} GF_M2TS_SIMPLE_APPLICATION_LOCATION;

/*! Application object carousel selector*/
typedef struct
{
	Bool remote_connection;
	u16 original_network_id;
	u16 transport_stream_id;
	u16 service_id;
	u8 component_tag;

} GF_M2TS_OBJECT_CAROUSEL_SELECTOR_BYTE;

/*! Application HTTP transport descriptor*/
typedef struct {
	u8 URL_extension_length;
	char* URL_extension_byte;

} GF_M2TS_TRANSPORT_HTTP_URL_EXTENTION;

/*! Application HTTP selector*/
typedef struct
{
	u8 URL_base_length;
	char* URL_base_byte;
	u8 URL_extension_count;
	GF_M2TS_TRANSPORT_HTTP_URL_EXTENTION* URL_extentions;

} GF_M2TS_TRANSPORT_HTTP_SELECTOR_BYTE;

/*! Transport Protocol descriptor*/
typedef struct
{
	u8 descriptor_tag;
	u8 descriptor_length;
	u16 protocol_id;
	u8 transport_protocol_label;
	void* selector_byte;

} GF_M2TS_TRANSPORT_PROTOCOL_DESCRIPTOR;

/*! Application Name descriptor*/
typedef struct
{
	u8 descriptor_tag;
	u8 descriptor_length;
	u32 ISO_639_language_code;
	u8 application_name_length;
	char* application_name_char;

} GF_M2TS_APPLICATION_NAME_DESCRIPTOR;

/*! Application boundary extension info */
typedef struct
{
	u8 boundary_extension_length;
	char* boundary_extension_byte;

} GF_M2TS_APPLICATION_BOUNDARY_EXTENSION_INFO;

/*! Application boundary descriptor*/
typedef struct
{
	u8 descriptor_tag;
	u8 descriptor_length;
	u8 boundary_extension_count;
	GF_M2TS_APPLICATION_BOUNDARY_EXTENSION_INFO* boundary_extension_info;

} GF_M2TS_APPLICATION_BOUNDARY_DESCRIPTOR;

/*! AIT implementation object*/
typedef struct
{
	u32 application_id;
	u8 application_control_code;

	u8 priority;
	u16 application_profile;

	/* Transport mode - 1 Broadcast - 3 Broadband */
	Bool broadcast;
	Bool broadband;
	char* http_url;
	char* carousel_url;
	Bool url_received;

	/* Carousel */
	u32 carousel_pid;
	u32 component_tag;


	char* appli_name;

} GF_M2TS_AIT_APPLICATION;

/*! AIT Channel Info object*/
typedef struct
{
	u32 service_id;
	u32 version_number;
	u32 ait_pid;
	u32 nb_application;
	GF_List *Application;

} GF_M2TS_CHANNEL_APPLICATION_INFO;

/*! AIT section callback used by MPEG-2 TS parser*/
void on_ait_section(GF_M2TS_Demuxer *ts, u32 evt_type, void *par);
/*! creates a new AIT section*/
GF_M2TS_ES *gf_ait_section_new(u32 service_id);
/*! gets the application info for the given service ID*/
GF_M2TS_CHANNEL_APPLICATION_INFO* gf_m2ts_get_channel_application_info(GF_List* ChannelAppList, u32 ait_service_id);
/*! frees the given application info*/
void  gf_m2ts_delete_channel_application_info(GF_M2TS_CHANNEL_APPLICATION_INFO* ChannelApp);

#endif /*GPAC_DISABLE_MPEG2TS*/


#ifdef __cplusplus
}
#endif

#endif	//_GF_AIT_H_


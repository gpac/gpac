/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Telecom Paristech
 *    Copyright (c)2006-200X ENST - All rights reserved
 *
 *  This file is part of GPAC / MPEG2-TS sub-project
 *
 *  GPAC is gf_free software; you can redistribute it and/or modify
 *  it under the terms of the GNU Lesser General Public License as published by
 *  the gf_free Software Foundation; either version 2, or (at your option)
 *  any later version.
 *   
 *  GPAC is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Lesser General Public License for more details.
 *   
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; see the file COPYING.  If not, write to
 *  the gf_free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA. 
 *
 */


#ifndef _GF_CAROUSSEL_H_
#define _GF_CAROUSSEL_H_

#include <gpac/mpegts.h>
#include <string.h>
#include <gpac/bitstream.h>

#define AIT_SECTION_LENGTH_MAX 1021
#define APPLICATION_TYPE_HTTP_APPLICATION 16
#define DSMCC_SECTION_LENGTH_MAX 4093

typedef enum {
	APPLICATION_DESCRIPTOR = 0x00,
	APPLICATION_NAME_DESCRIPTOR = 0x01,
	TRANSPORT_PROTOCOL_DESCRIPTOR = 0x02,
	SIMPLE_APPLICATION_LOCATION_DESCRIPTOR = 0x15,
	APPLICATION_USAGE_DESCRIPTOR = 0x16,
	APPLICATION_BOUNDARY_DESCRIPTOR = 0x17,
} DESCRIPTOR_TAG;

typedef enum{
	DOWNLOAD_INFO_REQUEST =					0x1001,
	DOWNLOAD_INFO_REPONSE_INDICATION =		0x1002,
	DOWNLOAD_DATA_BLOCK =					0x1003,
	DOWNLOAD_DATA_REQUEST =					0x1004,
	DOWNLOAD_DATA_CANCEL =					0x1005,
	DOWNLOAD_SERVER_INITIATE =				0x1006
}DSMCC_DOWNLOAD_MESSAGE_ID;

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
	GF_List * application;
	u32 CRC_32;

} GF_M2TS_AIT;


typedef struct
{
	ABSTRACT_ES
	GF_M2TS_SectionFilter *sec;
	u32 service_id;	

} GF_M2TS_AIT_CARRY;


typedef struct
{
	u32 organisation_id;
	u16 application_id;
	u8 application_control_code;
	u16 application_descriptors_loop_length;
	GF_List * application_descriptors;
	u8 application_descriptors_id[50];
	u8 index_app_desc_id;

}
GF_M2TS_AIT_APPLICATION;


typedef enum {
	FUTURE_USE = 0x00,
	CAROUSEL = 0x01,
	RESERVED = 0x02,
	TRANSPORT_HTTP = 0x03,
	DVB_USE = 0x04,
	TO_REGISTER = 0x100,
} PROTOCOL_ID;

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

typedef struct
{
	u8 descriptor_tag;
	u8 descriptor_length;
	u8 usage_type;

} GF_M2TS_APPLICATION_USAGE;

typedef struct
{
	u8 descriptor_tag;
	u8 descriptor_length;
	char* initial_path_bytes;

}GF_M2TS_SIMPLE_APPLICATION_LOCATION;

typedef struct
{
	Bool remote_connection;
	u16 original_network_id;
	u16 transport_stream_id;
	u16 service_id;
	u8 component_tag;

} GF_M2TS_OBJECT_CAROUSEL_SELECTOR_BYTE;

typedef struct{

	u8 URL_extension_length;
	char* URL_extension_byte;

}GF_M2TS_TRANSPORT_HTTP_URL_EXTENTION;

typedef struct
{
	u8 URL_base_length;
	char* URL_base_byte;
	u8 URL_extension_count;
	GF_M2TS_TRANSPORT_HTTP_URL_EXTENTION* URL_extentions;	

} GF_M2TS_TRANSPORT_HTTP_SELECTOR_BYTE;

typedef struct
{
	u8 descriptor_tag;
	u8 descriptor_length;
	u16 protocol_id;
	u8 transport_protocol_label;
	void* selector_byte;

} GF_M2TS_TRANSPORT_PROTOCOL_DESCRIPTOR;

typedef struct
{
	u8 descriptor_tag;
	u8 descriptor_length;
	u32 ISO_639_language_code;
	u8 application_name_length;
	char* application_name_char;

} GF_M2TS_APPLICATION_NAME_DESCRIPTOR;

typedef struct
{
	u8 boundary_extension_length;
	char* boundary_extension_byte;

} GF_M2TS_APPLICATION_BOUNDARY_EXTENSION_INFO;

typedef struct
{
	u8 descriptor_tag;
	u8 descriptor_length;
	u8 boundary_extension_count;
	GF_M2TS_APPLICATION_BOUNDARY_EXTENSION_INFO* boundary_extension_info;

} GF_M2TS_APPLICATION_BOUNDARY_DESCRIPTOR;


 

GF_Err gf_m2ts_process_ait(GF_M2TS_AIT *es, char  *data, u32 data_size, u32 table_id);
void on_ait_section(GF_M2TS_Demuxer *ts, u32 evt_type, void *par);
GF_M2TS_ES *gf_ait_section_new(u32 service_id);
void gf_ait_destroy(GF_M2TS_AIT* ait);
void gf_ait_application_destroy(GF_M2TS_AIT_APPLICATION* application);

/********************************************************** CAROUSEL PART ************************************************************************************/

typedef struct
{
	u32 moduleId;
	u32 downloadId;
	u32 version_number;	
}GF_M2TS_DSMCC_PROCESSED;

typedef struct
{
	u32 moduleId;
	u32 version_number;
	u32 size;
	u32 downloadId;
	char* buffer;
	u32 byte_sift;
	u16 section_number;
	u16 last_section_number;
	Bool processed;
}GF_M2TS_DSMCC_MODULE;


typedef struct
{	
	GF_List* dsmcc_modules;
	GF_M2TS_DSMCC_PROCESSED processed[255];
}GF_M2TS_DSMCC_OVERLORD;

typedef struct
{
	u8 table_id;
	u8 section_syntax_indicator;
	u8 private_indicator;
	u16 dsmcc_section_length;
	u16 table_id_extension;
	u8 version_number;
	u8 current_next_indicator;
	u8 section_number;
	u8 last_section_number; 
	void* DSMCC_Extension;
	u32 checksum;
	u32 CRC_32;
}GF_M2TS_DSMCC_SECTION;

typedef struct
{
	u8 adaptationType;
	char* adaptationDataByte;
}
GF_M2TS_DSMCC_ADAPTATION_HEADER;

typedef struct
{
	u8 protocolDiscriminator;
	u8 dsmccType;
	u16 messageId;
	/* dsmccMessageHeader mode */
	u32 transactionId;
	/* dsmccDownloadDataHeader */
	u32 downloadId;
	u8 reserved;
	u8 adaptationLength;
	u16 messageLength;
	/* added not in the spec */
	u8 header_length;
	GF_M2TS_DSMCC_ADAPTATION_HEADER* DsmccAdaptationHeader;

}GF_M2TS_DSMCC_MESSAGE_DATA_HEADER;

/* DOWNLOAD_DATA_MESSAGE */
typedef struct
{
	u8 protocolDiscriminator;
	u8 dsmccType;
	u16 messageId;
	u32 downloadId;
	u8 reserved;
	u8 adaptationLength;
	u16 messageLength;
	GF_M2TS_DSMCC_ADAPTATION_HEADER* DsmccAdaptationHeader;

}GF_M2TS_DSMCC_DOWNLOAD_DATA_HEADER;

typedef struct
{	
	u8 subDescriptorType;
	u8 subDescriptorLength;	
	char *additionalInformation;
	
}GF_M2TS_DSMCC_SUBDESCRIPTOR;

typedef struct
{
	u8 descriptorType;
	u8 descriptorLength;
	u8 specifierType;
	u32 specifierData;
	u16 model;
	u16 version;
	u8 subDescriptorCount;
	GF_M2TS_DSMCC_SUBDESCRIPTOR* SubDescriptor;

}GF_M2TS_DSMCC_DESCRIPTOR;

typedef struct
{
	u16 compatibilityDescriptorLength;
	u16 descriptorCount;
	GF_M2TS_DSMCC_DESCRIPTOR* Descriptor;
}GF_M2TS_DSMCC_COMPATIBILITY_DESCRIPTOR;

typedef struct
{	
	u8 bufferSize;
	u8 maximumBlockSize;
	GF_M2TS_DSMCC_COMPATIBILITY_DESCRIPTOR Compatibility_Descr;
	u8 privateDataLength;
	char* privateDataByte;
}GF_M2TS_DSMCC_DOWNLOAD_INFO_REQUEST;

typedef struct
{
  u16 moduleId;
  u32 moduleSize;
  u8 moduleVersion;
  u8 moduleInfoLength;
  char* moduleInfoByte;
}GF_M2TS_DSMCC_INFO_MODULES;

typedef struct
{  
  u32 downloadId;
  u16 blockSize;
  u8 windowSize;
  u8 ackPeriod;
  u32 tCDownloadWindow;
  u32 tCDownloadScenario;
  GF_M2TS_DSMCC_COMPATIBILITY_DESCRIPTOR CompatibilityDescr;
  u16 numberOfModules;
  GF_M2TS_DSMCC_INFO_MODULES* Modules;
  u16 privateDataLength;     
  char* privateDataByte;
}GF_M2TS_DSMCC_DOWNLOAD_INFO_RESP_INDIC;

typedef struct
{  
  u8 moduleId;
  u8 moduleVersion;
  u8 reserved;
  u8 blockNumber;
  char* blockDataByte;
  /*added not in the spec */
  u32 dataBlocksize;
}GF_M2TS_DSMCC_DOWNLOAD_DATA_BLOCK;

typedef struct
{ 
  u16 moduleId;
  u16 blockNumber;
  u8 downloadReason;
}GF_M2TS_DSMCC_DOWNLOAD_DATA_REQUEST_MESSAGE;

typedef struct
{  
  u32 downloadId;
  u16 moduleId;
  u16 blockNumber;
  u8 downloadCancelReason;
  u8 reserved;
  u16 privateDataLength;
  char* privateDataByte;
}GF_M2TS_DSMCC_DOWNLOAD_CANCEL;

typedef struct
{ 
  u8 serverId[20];
  GF_M2TS_DSMCC_COMPATIBILITY_DESCRIPTOR CompatibilityDescr;
  u16 privateDataLength;
  char* privateDataByte;
}GF_M2TS_DSMCC_DOWNLOAD_SERVER_INIT;

typedef struct
{
	GF_M2TS_DSMCC_MESSAGE_DATA_HEADER DownloadDataHeader;
	void* dataMessagePayload;
}GF_M2TS_DSMCC_DOWNLOAD_DATA_MESSAGE;

/* DESCRIPTOR LIST */

typedef struct
{
	u8 descriptorTag;
	u8 descriptorLength;
	u8 postDiscontinuityIndicator;
	u8 contentId;
	u8 STC_Reference[5];
	u8 NPT_Reference[5];
	u16 scaleNumerator;
	u16 scaleDenominator;
}GF_M2TS_DSMCC_NPT_REFERENCE_DESCRIPTOR;


typedef struct
{
	u8 descriptorTag;
	u8 descriptorLength;
	void* descriptor;
}GF_M2TS_DSMCC_STREAM_DESCRIPTOR;

GF_Err gf_m2ts_process_dsmcc(GF_M2TS_Demuxer* ts,GF_M2TS_DSMCC_SECTION *dsmcc, char  *data, u32 data_size, u32 table_id);
GF_M2TS_DSMCC_OVERLORD* gf_m2ts_init_dsmcc_overlord();

#endif	//_GF_CAROUSSEL_H_


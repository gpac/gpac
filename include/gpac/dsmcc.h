/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jonathan Sillan
 *			Copyright (c) Telecom ParisTech 2011-2012
 *					All rights reserved
 *
 *  This file is part of GPAC / common tools sub-project
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


#ifndef _GF_DSMCC_H_
#define _GF_DSMCC_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <gpac/mpegts.h>
#include <string.h>
#include <gpac/bitstream.h>


#ifndef GPAC_DISABLE_MPEG2TS


#define DSMCC_SECTION_LENGTH_MAX 4093

typedef enum{
	DOWNLOAD_INFO_REQUEST =					0x1001,
	DOWNLOAD_INFO_REPONSE_INDICATION =		0x1002,
	DOWNLOAD_DATA_BLOCK =					0x1003,
	DOWNLOAD_DATA_REQUEST =					0x1004,
	DOWNLOAD_DATA_CANCEL =					0x1005,
	DOWNLOAD_SERVER_INITIATE =				0x1006
}DSMCC_DOWNLOAD_MESSAGE_ID;

typedef enum{
	TAG_BIOP =				0x49534F06,
	TAG_LITE_OPTIONS =		0x49534F05
}DSMCC_DOWNLOAD_PROFILE_ID_TAG;

typedef enum{
	CACHING_PRIORITY_DESCRIPTOR		=		0x71,
	CONTENT_TYPE_DESCRIPTOR			=		0x72,
	COMPRESSED_MODULE_DESCRIPTOR	=		0x09
}DSMCC_BIOP_DESCRIPTOR;

typedef struct{
	u8 descriptor_tag;
	u8 descriptor_length;
	u32 carousel_id;
	u8 FormatID;
	char *private_data_byte;
	u8 ModuleVersion;
	u8 ModuleId;
	u16 BlockSize;
	u32 ModuleSize;
	u8 CompressionMethod;
	u32 OriginalSize;
	u8 TimeOut;
	u8 ObjectKeyLength;
	char* ObjectKeyData;
}GF_M2TS_CAROUSEL_INDENTIFIER_DESCRIPTOR;

typedef struct
{
	u32 moduleId;
	u32 downloadId;
	u32 version_number;
	Bool done;
}GF_M2TS_DSMCC_PROCESSED;

typedef struct
{
	/* Module identifier */
	u32 moduleId;
	/* Version number of the module */
	u32 version_number;
	/* size in byte of the module */
	u32 size;
	/* Download identifier */
	u32 downloadId;
	/* buffer of data */
	char* buffer;
	/* byte shifting in the buffer of data */
	u32 byte_sift;
	/* last section number processed */
	u16 section_number;
	/* the last section number of the module */
	u16 last_section_number;
	/* size in byte of each block in the module */
	u32 block_size;
	/* Checks if the module has been processed */
	/* 1 if yes */
	/* 0 otherwise */
	Bool processed;
	/* Checks if the module's data are zipped */
	/* 1 if yes */
	/* 0 otherwise */
	Bool Gzip;
	/* Size of the module's data after uncompression */
	u32 original_size;
}GF_M2TS_DSMCC_MODULE;

typedef struct
{
	/* table id : identifier for dsmcc message type */
	u8 table_id;
	/* indicates the presence of CRC 32 */
	u8 section_syntax_indicator;
	u8 private_indicator;
	/* length in byte of the dsmcc section */
	u16 dsmcc_section_length;
	/* linked with the moduleId if carried by the section */
	u16 table_id_extension;
	/* version number linked with the Data block if carried by the section */
	u8 version_number;
	u8 current_next_indicator;
	/* section number of the data block if carried by the section */
	u8 section_number;
	/* last section number of the data block if carried by the section */
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
	u32 bufferSize;
	u16 maximumBlockSize;
	GF_M2TS_DSMCC_COMPATIBILITY_DESCRIPTOR CompatibilityDescr;
	u16 privateDataLength;
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
	GF_M2TS_DSMCC_INFO_MODULES Modules;
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
	u32 GroupId;
	u32 GroupSize;
	GF_M2TS_DSMCC_COMPATIBILITY_DESCRIPTOR CompatibilityDescr;
	u16 GroupInfoLength;
	char* groupInfoByte;
}GF_M2TS_DSMCC_INFO_GROUP;

typedef struct
{
	u16 NumberOfGroups;
	GF_M2TS_DSMCC_INFO_GROUP* InfoGroup; 
	u16 PrivateDataLength;
	char* privateDataByte;

}GF_M2TS_DSMCC_GROUP_INFO_INDICATION;

typedef struct
{ 
	u8 serverId[20];
	GF_M2TS_DSMCC_COMPATIBILITY_DESCRIPTOR CompatibilityDescr;
	u16 privateDataLength;
	char* privateDataByte;
	GF_M2TS_DSMCC_GROUP_INFO_INDICATION* GroupInfoIndic;
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

/* OBJECT CAROUSEL */
typedef struct{
	u16 id;
	u16 use;
	u16 assocTag;
	u8 selector_length;
	char* selector_data;
	u16 selector_type;
	u32 transactionId;
	u32 timeout;
}GF_M2TS_DSMCC_BIOP_TAPS;

typedef struct{
	u8 AFI;
	u8 type;
	u32 carouselId;
	u8 specifierType;
	u32 specifierData;
	u16 transport_stream_id; 
	u16 original_network_id;
	u16 service_id;
	u32 reserved;
}GF_M2TS_DSMCC_SERVICE_DOMAIN;

typedef struct{
	u32 componentId_tag;
	u8 component_data_length;
	u32 carouselId;
	u16 moduleId;
	u8 version_major;
	u8 version_minor;
	u8 objectKey_length;
	u32 objectKey_data;
}GF_M2TS_DSMCC_BIOP_OBJECT_LOCATION;

typedef struct{
	u32 componentId_tag;
	u8 component_data_length;
	u8 taps_count;
	GF_M2TS_DSMCC_BIOP_TAPS* Taps;
	char* additional_tap_byte;
}GF_M2TS_DSMCC_BIOP_CONN_BINDER;

typedef struct{
	GF_M2TS_DSMCC_BIOP_OBJECT_LOCATION ObjectLocation;
	GF_M2TS_DSMCC_BIOP_CONN_BINDER ConnBinder;
}GF_M2TS_DSMCC_BIOP_PROFILE_BODY;

typedef struct{
	u32 id_length;
	char* id_data;
	u32 kind_length;
	char* kind_data;
}GF_M2TS_DSMCC_BIOP_NAME_COMPONENT;

typedef struct{
	u32 componentId_tag;
	u8 component_data_length;
	u8 serviceDomain_length;
	GF_M2TS_DSMCC_SERVICE_DOMAIN serviceDomain_data;
	u32 nameComponents_count;
	GF_M2TS_DSMCC_BIOP_NAME_COMPONENT* NameComponent;
	u32 initialContext_length;
	char* InitialContext_data_byte;
}GF_M2TS_DSMCC_BIOP_SERVICE_LOCATION;

typedef struct{
	u32 componentId_tag;
	u8 component_data_length;
	char* component_data_byte;
}GF_M2TS_DSMCC_BIOP_LITE_COMPONENT;

typedef struct{
	u32 profileId_tag;
	u32 profile_data_length;
	u8 profile_data_byte_order;
	u8 lite_component_count;

	GF_M2TS_DSMCC_BIOP_PROFILE_BODY* BIOPProfileBody;
	GF_M2TS_DSMCC_BIOP_SERVICE_LOCATION* ServiceLocation;
	GF_M2TS_DSMCC_BIOP_LITE_COMPONENT* LiteComponent;

}GF_M2TS_DSMCC_BIOP_TAGGED_PROFILE;

typedef struct{
	u32 type_id_length;
	char* type_id_byte;
	u32 taggedProfiles_count;
	GF_List* taggedProfile;
}GF_M2TS_DSMCC_IOR;

typedef struct{
	u32 moduleTimeOut;
	u32 blockTimeOut;
	u32 minBlockTime;
	u8 taps_count;
	GF_M2TS_DSMCC_BIOP_TAPS* Taps;
	u8 userInfoLength;
	u8* userInfo_data;
	GF_List* descriptor;

	u8 compression_method;	
	u8 transparency_level; 
}GF_M2TS_DSMCC_BIOP_MODULE_INFO;

typedef struct{
	u32 context_id;
	u16 context_data_length;
	char* context_data_byte;
}GF_M2TS_DSMCC_SERVICE_CONTEXT;

typedef struct{
	GF_M2TS_DSMCC_IOR IOR;
	u8 downloadTaps_count;
	GF_M2TS_DSMCC_BIOP_TAPS* Taps;
	u8 serviceContextList_count;
	GF_M2TS_DSMCC_SERVICE_CONTEXT* ServiceContext;
	u16 userInfoLength;
	char* userInfo_data;
}GF_M2TS_DSMCC_SERVICE_GATEWAY_INFO;


/* DESCRIPTORS */
typedef struct{
	u8 descriptor_tag;
	u8 descriptor_length;
	u8 priority_value;
	u8 transparency_level;
}GF_M2TS_DSMCC_BIOP_CACHING_PRIORITY_DESCRIPTOR;

typedef struct{
	u8 descriptor_tag;
	u8 descriptor_length;
	u8 compression_method;
	u32 original_size;
}GF_M2TS_DSMCC_BIOP_COMPRESSED_MODULE_DESCRIPTOR;

typedef struct{
	u8 descriptor_tag;
	u8 descriptor_length;							
	char* content_type_data_byte;
}GF_M2TS_DSMCC_BIOP_CONTENT_TYPE_DESRIPTOR;


typedef struct{
	/* "BIOP" */
	u32 magic;
	u8 biop_version_major;
	u8 biop_version_minor;
	u8 byte_order;
	u8 message_type;
	/* size in byte of the whole object carousel */
	u32 message_size;
	u8 objectKey_length;
	/* witness the kind of object carousel the item is */
	/* fil for a file */
	/* dir for a directory */
	/* srg for the ServiceGateway */
	/* str for Stream Message */
	/* ste for Stream Event Message */
	u32 objectKey_data;
	u32 objectKind_length;
	/* The number that identifies the object in the module */
	char* objectKind_data;
	u16 objectInfo_length;
}GF_M2TS_DSMCC_BIOP_HEADER;

typedef struct{
	GF_M2TS_DSMCC_BIOP_HEADER* Header;
	u64 ContentSize;
	GF_List* descriptor;
	u8 serviceContextList_count;
	GF_M2TS_DSMCC_SERVICE_CONTEXT* ServiceContext;
	u32 messageBody_length;
	/* size in byte of the data of the file */
	u32 content_length;
	/* data a the file */
	char* content_byte;
}GF_M2TS_DSMCC_BIOP_FILE;

typedef struct{
	/* Name */
	u8 nameComponents_count;
	/* There is must be only one nameComponent */
	u8 id_length;
	/* the name of the item */
	char * id_data;
	u8 kind_length;
	/* the kind of the item */
	/* fil for a file */
	/* dir for a directory */
	/* srg for the ServiceGateway */
	/* str for Stream Message */
	/* ste for Stream Event Message */
	char* kind_data;
	/* 1 if the item a file or a stream */
	/* 0 if the item si a directory */
	u8 BindingType;
	GF_M2TS_DSMCC_IOR IOR;
	u16 objectInfo_length;
	u64 ContentSize;
	GF_List* descriptor;
}GF_M2TS_DSMCC_BIOP_NAME;

typedef struct{
	GF_M2TS_DSMCC_BIOP_HEADER* Header;
	char* objectInfo_data;
	u8 serviceContextList_count;
	GF_M2TS_DSMCC_SERVICE_CONTEXT* ServiceContext;
	/* Length is byte of the message */
	u32 messageBody_length;
	/* Number of the item */
	u16 bindings_count;
	/* List of the item in the directory */
	GF_M2TS_DSMCC_BIOP_NAME* Name;		
}GF_M2TS_DSMCC_BIOP_DIRECTORY;

typedef struct{
	u8 aDescription_length; 				
	char* aDescription_bytes;
	u32 duration_aSeconds;
	u32 duration_aMicroseconds;
	u8 audio;	 
	u8 video;	 
	u8 data;
}GF_M2TS_DSMCC_STREAM_INFO;

typedef struct{
	GF_M2TS_DSMCC_BIOP_HEADER* Header;			
	GF_M2TS_DSMCC_STREAM_INFO Info;
	char* objectInfo_byte;
	u8 serviceContextList_count; 				
	GF_M2TS_DSMCC_SERVICE_CONTEXT* ServiceContext;
	u32 messageBody_length; 
	u8 taps_count;
	GF_M2TS_DSMCC_BIOP_TAPS* Taps; 				
}GF_M2TS_DSMCC_BIOP_STREAM_MESSAGE;

typedef struct{					
	u8 eventName_length; 				
	char* eventName_data_byte;
}GF_M2TS_DSMCC_BIOP_EVENT_LIST;

typedef struct{
	GF_M2TS_DSMCC_BIOP_HEADER* Header; 
	GF_M2TS_DSMCC_STREAM_INFO Info;				
	u16 eventNames_count;
	GF_M2TS_DSMCC_BIOP_EVENT_LIST* EventList;
	char* objectInfo_byte;
	u8 serviceContextList_count;				
	GF_M2TS_DSMCC_SERVICE_CONTEXT* ServiceContext;
	u32 messageBody_length; 	 
	u8 taps_count;
	GF_M2TS_DSMCC_BIOP_TAPS* Taps;		
	u8 eventIds_count;  				
	u16* eventId;
}GF_M2TS_DSMCC_BIOP_STREAM_EVENT;

/*Define the base element for extracted dsmcc element*/
#define GF_M2TS_DSMCC_ELEMENT		\
			u32 moduleId; \
			u32 downloadId; \
			u32 version_number; \
			u32 objectKey_data; \
			char* name; \
			void* parent;

typedef struct
{	
	GF_M2TS_DSMCC_ELEMENT
	/*Path to the file */
	char* Path;
}GF_M2TS_DSMCC_FILE;

typedef struct
{	GF_M2TS_DSMCC_ELEMENT
	/* List of files in the directory*/
	GF_List* File;
	/* List of directories of the directory*/
	GF_List* Dir;
	/*Path to the directory */
	char* Path;
}GF_M2TS_DSMCC_DIR;

typedef struct
{	GF_M2TS_DSMCC_ELEMENT
	/* Number of process directories */
	u8 nb_processed_dir;
	/* Service Id of the data carousel*/
	u32 service_id;
	/* List of files of the root of the file system*/
	GF_List* File;
	/* List of directories of the root of the file system*/
	GF_List* Dir;
}GF_M2TS_DSMCC_SERVICE_GATEWAY;

typedef struct
{	
	/* List that carries the modules to process */
	GF_List* dsmcc_modules;
	/* List of processed module */
	GF_M2TS_DSMCC_PROCESSED processed[512];	
	/*Check if the ServiceGateway has been recovered*/
	/* 1 ServiceGateway received */
	/* 0 otherwise */
	Bool Got_ServiceGateway;
	/* ServiceGateway Structure */
	GF_M2TS_DSMCC_SERVICE_GATEWAY* ServiceGateway;
	/* u32 transactionId for DownloadInfoIndicator versioning */
	u32 transactionId;
	/* List that carries modules that have been received before inti - TO DO */
	GF_List* Unprocessed_module;
	/* Service ID that carries this carousel */
	u32 service_id;
	/* Path of the root directory of the file system */
	char* root_dir;
	/*Check if the index.html (root of the application) has been recovered*/
	/* 1 index.html received received */
	/* 0 otherwise */
	Bool get_index;
	/* Number of the application that uses the carousel*/
	u32 application_id;
} GF_M2TS_DSMCC_OVERLORD;

void on_dsmcc_section(GF_M2TS_Demuxer *ts, u32 evt_type, void *par);
GF_Err gf_m2ts_process_dsmcc(GF_M2TS_DSMCC_OVERLORD* dsmcc_overlord,GF_M2TS_DSMCC_SECTION *dsmcc, char  *data, u32 data_size, u32 table_id);
GF_M2TS_DSMCC_OVERLORD* gf_m2ts_init_dsmcc_overlord(u32 service_id);
GF_M2TS_DSMCC_OVERLORD* gf_m2ts_get_dmscc_overlord(GF_List* Dsmcc_controller,u32 service_id);
void gf_m2ts_delete_dsmcc_overlord(GF_M2TS_DSMCC_OVERLORD* dsmcc_overlord);

#endif /*GPAC_DISABLE_MPEG2TS*/

#ifdef __cplusplus
}
#endif

#endif	//_GF_CAROUSSEL_H_


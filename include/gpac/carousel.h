/* 
 * Copyright (c) TELECOM ParisTech 2011
 */

#ifndef _GF_CAROUSSEL_H_
#define _GF_CAROUSSEL_H_

#ifndef GPAC_DISABLE_MPEG2TS

#ifdef __cplusplus
extern "C" {
#endif

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

typedef enum{
	TAG_BIOP =				0x49534F06,
	TAG_LITE_OPTIONS =		0x49534F05
}DSMCC_DOWNLOAD_PROFILE_ID_TAG;

typedef enum{
	CACHING_PRIORITY_DESCRIPTOR		=		0x71,
	CONTENT_TYPE_DESCRIPTOR			=		0x72,
	COMPRESSED_MODULE_DESCRIPTOR	=		0x09
}DSMCC_BIOP_DESCRIPTOR;

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
	u32 moduleId;
	u32 version_number;
	u32 size;
	u32 downloadId;
	char* buffer;
	u32 byte_sift;
	u16 section_number;
	u16 last_section_number;
	u32 block_size;
	Bool processed;
	Bool Gzip;
	u32 original_size;
}GF_M2TS_DSMCC_MODULE;

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
	u32 magic;
	u8 biop_version_major;
	u8 biop_version_minor;
	u8 byte_order;
	u8 message_type;
	u32 message_size;
	u8 objectKey_length;
	u32 objectKey_data;
	u32 objectKind_length;
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
	u32 content_length;
	char* content_byte;
}GF_M2TS_DSMCC_BIOP_FILE;

typedef struct{
	/* Name */
	u8 nameComponents_count;
	/* There is must be only one nameComponent */
	u8 id_length;
	char * id_data;
	u8 kind_length;
	char* kind_data;
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
	u32 messageBody_length;
	u16 bindings_count;
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
	char* Path;
}GF_M2TS_DSMCC_FILE;

typedef struct
{	GF_M2TS_DSMCC_ELEMENT
	GF_List* File;
	GF_List* Dir;
	char* Path;
}GF_M2TS_DSMCC_DIR;

typedef struct
{	GF_M2TS_DSMCC_ELEMENT
	u8 nb_processed_dir;
	GF_List* File;
	GF_List* Dir;
}GF_M2TS_DSMCC_SERVICE_GATEWAY;

typedef struct
{	
	GF_List* dsmcc_modules;
	GF_M2TS_DSMCC_PROCESSED processed[512];	
	Bool Got_ServiceGateway;
	GF_M2TS_DSMCC_SERVICE_GATEWAY* ServiceGateway;
	GF_List* Unprocessed_module;
}GF_M2TS_DSMCC_OVERLORD;


GF_Err gf_m2ts_process_dsmcc(GF_M2TS_Demuxer* ts,GF_M2TS_DSMCC_SECTION *dsmcc, char  *data, u32 data_size, u32 table_id);
GF_M2TS_DSMCC_OVERLORD* gf_m2ts_init_dsmcc_overlord();

#ifdef __cplusplus
}
#endif
#endif

#endif	//_GF_CAROUSSEL_H_


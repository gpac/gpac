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

#include <gpac/carousel.h>

#ifndef GPAC_DISABLE_MPEG2TS

/* static functions */
static GF_Err dsmcc_download_data_validation(GF_M2TS_DSMCC_DOWNLOAD_DATA_BLOCK* DownloadDataBlock,GF_M2TS_DSMCC_MODULE* dsmcc_module,u32 downloadId);
static GF_Err gf_m2ts_dsmcc_process_compatibility_descriptor(GF_M2TS_DSMCC_COMPATIBILITY_DESCRIPTOR *CompatibilityDesc, char* data,GF_BitStream *bs,u32* data_shift);
static GF_Err gf_m2ts_dsmcc_process_message_header(GF_M2TS_DSMCC_MESSAGE_DATA_HEADER *MessageHeader, char* data,GF_BitStream *bs,u32* data_shift,u32 mode);
static GF_Err gf_m2ts_dsmcc_download_data(GF_M2TS_DSMCC_SECTION *dsmcc, char  *data, GF_BitStream *bs,u32* data_shift);
static GF_Err gf_m2ts_dsmcc_section_delete(GF_M2TS_DSMCC_SECTION *dsmcc);
static GF_Err gf_m2ts_dsmcc_delete_message_header(GF_M2TS_DSMCC_MESSAGE_DATA_HEADER *MessageHeader);
static GF_Err gf_m2ts_dsmcc_delete_compatibility_descriptor(GF_M2TS_DSMCC_COMPATIBILITY_DESCRIPTOR *CompatibilityDesc);
static GF_Err gf_m2ts_extract_info(GF_M2TS_Demuxer *ts,GF_M2TS_DSMCC_SECTION *dsmcc);
static GF_Err dsmcc_module_delete(GF_M2TS_DSMCC_MODULE* dsmcc_module);
static GF_Err dsmcc_module_state(GF_M2TS_DSMCC_OVERLORD* dsmcc_overlord,u32 moduleId,u32 moduleVersion);
static GF_Err dsmcc_create_module_validation(GF_M2TS_DSMCC_INFO_MODULES* InfoModules, u32 downloadId, GF_M2TS_DSMCC_OVERLORD* dsmcc_overlord,u32 nb_module);
static GF_Err dsmcc_module_complete(GF_M2TS_DSMCC_OVERLORD* dsmcc_overlord,GF_M2TS_DSMCC_MODULE* dsmcc_module,u32 moduleIndex);


GF_M2TS_ES *gf_ait_section_new(u32 service_id)
{
	GF_M2TS_ES *es;
	GF_M2TS_AIT_CARRY *ses;
	GF_SAFEALLOC(ses, GF_M2TS_AIT_CARRY);
	es = (GF_M2TS_ES *)ses;
	es->flags = GF_M2TS_ES_IS_SECTION;
	ses->service_id = service_id;
	return es;
}


void on_ait_section(GF_M2TS_Demuxer *ts, u32 evt_type, void *par) 
{
	GF_M2TS_SL_PCK *pck = (GF_M2TS_SL_PCK *)par;
	char *data;
	u32 u32_data_size;
	u32 u32_table_id;

	if (evt_type == GF_M2TS_EVT_AIT_FOUND) {
		GF_M2TS_AIT* ait;
		GF_M2TS_AIT_CARRY* ait_carry = (GF_M2TS_AIT_CARRY*)pck->stream;
		GF_SAFEALLOC(ait, GF_M2TS_AIT);
		data = pck->data;
		u32_data_size = pck->data_len;
		u32_table_id = data[0];		
		ait->pid = ait_carry->pid;
		ait->service_id = ait_carry->service_id;
		gf_m2ts_process_ait(ait, data, u32_data_size, u32_table_id);

	}
}

GF_Err gf_m2ts_process_ait(GF_M2TS_AIT *ait, char  *data, u32 data_size, u32 table_id)
{

	GF_BitStream *bs;
	u8 temp_descriptor_tag;
	u32 data_shift, app_desc_data_shift, ait_app_data_shift;
	u32 nb_of_protocol;

	data_shift = 0;
	temp_descriptor_tag = 0;
	ait_app_data_shift = 0;
	bs = gf_bs_new(data,data_size,GF_BITSTREAM_READ);

	ait->common_descriptors = gf_list_new();
	if(ait->common_descriptors == NULL){
		GF_LOG(GF_LOG_INFO, GF_LOG_CONTAINER, ("[Process AIT] Error during common_descriptors list initialization, abording processing \n"));
		return GF_BAD_PARAM;
	}
	ait->application = gf_list_new();
	if(ait->application == NULL){
		GF_LOG(GF_LOG_INFO, GF_LOG_CONTAINER, ("[Process AIT] Error during application list initialization, abording processing \n"));
		return GF_BAD_PARAM;
	}

	ait->table_id = gf_bs_read_int(bs,8);	
	ait->section_syntax_indicator = gf_bs_read_int(bs,1);
	gf_bs_read_int(bs,3);
	ait->section_length = gf_bs_read_int(bs,12);	
	if( (data[1] & 0x0C) != 0){
		GF_LOG(GF_LOG_INFO, GF_LOG_CONTAINER, ("[Process AIT] section length is not correct abroding \n"));
	}else if( ait->section_length > AIT_SECTION_LENGTH_MAX){
		GF_LOG(GF_LOG_INFO, GF_LOG_CONTAINER, ("[Process AIT] section length should not exceed 1021. Wrong section, abording processing \n"));
	}
	ait->test_application_flag = gf_bs_read_int(bs,1);
	if(ait->test_application_flag == 1){
		GF_LOG(GF_LOG_INFO, GF_LOG_CONTAINER, ("[Process AIT] test application flag is at 1. API transmitted for testing, abording processing \n"));
		return GF_BAD_PARAM;
	}
	ait->application_type = gf_bs_read_int(bs,15);	
	if(ait->application_type != APPLICATION_TYPE_HTTP_APPLICATION){
		GF_LOG(GF_LOG_INFO, GF_LOG_CONTAINER, ("[Process AIT] application type should 0x10. Wrong section, abording processing \n"));
		return GF_BAD_PARAM;
	}
	gf_bs_read_int(bs,2);
	ait->version_number = gf_bs_read_int(bs,5);

	ait->current_next_indicator = gf_bs_read_int(bs,1);	
	if(!ait->current_next_indicator){
		GF_LOG(GF_LOG_INFO, GF_LOG_CONTAINER, ("[Process AIT] current next indicator should be at 1 \n"));
		return GF_BAD_PARAM;
	}
	ait->section_number = gf_bs_read_int(bs,8);
	ait->last_section_number = gf_bs_read_int(bs,8);	
	gf_bs_read_int(bs,4);/* bit shifting */
	ait->common_descriptors_length = gf_bs_read_int(bs,12);	
	gf_bs_read_int(bs,(unsigned int)ait->common_descriptors_length/8);
	gf_bs_read_int(bs,4);/* bit shifting */	
	ait->application_loop_length = gf_bs_read_int(bs,12);

	data_shift = (u32)(gf_bs_get_position(bs)) + ait->common_descriptors_length/8;

	while (ait_app_data_shift < ait->application_loop_length) {

		GF_M2TS_AIT_APPLICATION* application;
		GF_SAFEALLOC(application,GF_M2TS_AIT_APPLICATION);
		application->application_descriptors = gf_list_new();
		application->index_app_desc_id = 0;

		/* application loop */
		application->organisation_id = gf_bs_read_int(bs,32);	
		application->application_id= gf_bs_read_int(bs,16);
		application->application_control_code= gf_bs_read_int(bs,8);
		gf_bs_read_int(bs,4);/* bit shifting */
		application->application_descriptors_loop_length= gf_bs_read_int(bs,12);		

		ait_app_data_shift += 9;
		app_desc_data_shift = 0;
		nb_of_protocol = 0;

		while (app_desc_data_shift< application->application_descriptors_loop_length) {
			temp_descriptor_tag = gf_bs_read_int(bs,8);
			switch (temp_descriptor_tag) {
				case APPLICATION_DESCRIPTOR:
				{
 					u64 pre_processing_pos;
 					u8 i;
 					GF_M2TS_APPLICATION_DESCRIPTOR *application_descriptor;
 					GF_SAFEALLOC(application_descriptor, GF_M2TS_APPLICATION_DESCRIPTOR);
 					application_descriptor->descriptor_tag = temp_descriptor_tag;
 					application->application_descriptors_id[application->index_app_desc_id] = temp_descriptor_tag;
 					application->index_app_desc_id++;
 					application_descriptor->descriptor_length = gf_bs_read_int(bs,8);
					pre_processing_pos = gf_bs_get_position(bs);
					application_descriptor->application_profiles_length = gf_bs_read_int(bs,8);
					application_descriptor->application_profile = gf_bs_read_int(bs,16);
					application_descriptor->version_major = gf_bs_read_int(bs,8);
					application_descriptor->version_minor = gf_bs_read_int(bs,8);
					application_descriptor->version_micro = gf_bs_read_int(bs,8);
					application_descriptor->service_bound_flag = gf_bs_read_int(bs,1);
					application_descriptor->visibility = gf_bs_read_int(bs,2);
					gf_bs_read_int(bs,5); /*bit shift*/
					application_descriptor->application_priority = gf_bs_read_int(bs,8);
					if (nb_of_protocol > 0) {
						for (i=0; i<nb_of_protocol; i++) {						
							application_descriptor->transport_protocol_label[i] = gf_bs_read_int(bs,8);
						}
					} else {
						application_descriptor->transport_protocol_label[0] = gf_bs_read_int(bs,8);
					}
					if (pre_processing_pos+application_descriptor->descriptor_length != gf_bs_get_position(bs) || application_descriptor->application_profile == 2 /* PVR feature */) {
						GF_LOG(GF_LOG_INFO, GF_LOG_CONTAINER, ("[Process AIT] Descriptor data processed length error. Difference between byte shifting %d and descriptor length %d \n",(gf_bs_get_position(bs) -  pre_processing_pos),application_descriptor->descriptor_length));
						gf_free(application_descriptor);
						return GF_BAD_PARAM;
					}
 					gf_list_add(application->application_descriptors,application_descriptor);
					app_desc_data_shift += (2+ application_descriptor->descriptor_length);
 					break;
				}
				case APPLICATION_NAME_DESCRIPTOR:
				{
 					u64 pre_processing_pos;
 					GF_M2TS_APPLICATION_NAME_DESCRIPTOR* name_descriptor;
 					GF_SAFEALLOC(name_descriptor, GF_M2TS_APPLICATION_NAME_DESCRIPTOR);
 					application->application_descriptors_id[application->index_app_desc_id] = temp_descriptor_tag;
 					application->index_app_desc_id++;
 					name_descriptor->descriptor_tag = temp_descriptor_tag;
 					name_descriptor->descriptor_length = gf_bs_read_int(bs,8);
 					pre_processing_pos = gf_bs_get_position(bs);
 					name_descriptor->ISO_639_language_code = gf_bs_read_int(bs,24);
 					name_descriptor->application_name_length = gf_bs_read_int(bs,8);
 					name_descriptor->application_name_char = (char*) gf_calloc(name_descriptor->application_name_length+1,sizeof(char));
 					gf_bs_read_data(bs,name_descriptor->application_name_char,name_descriptor->application_name_length);
 					name_descriptor->application_name_char[name_descriptor->application_name_length] = 0 ;
 					if (pre_processing_pos+name_descriptor->descriptor_length != gf_bs_get_position(bs)) {
 						GF_LOG(GF_LOG_INFO, GF_LOG_CONTAINER, ("[Process AIT] Descriptor data processed length error. Difference between byte shifting %d and descriptor length %d \n",(gf_bs_get_position(bs) -  pre_processing_pos),name_descriptor->descriptor_length));
						gf_free(name_descriptor->application_name_char);
						gf_free(name_descriptor);
						return GF_BAD_PARAM;
 					}
 					gf_list_add(application->application_descriptors,name_descriptor);
 					app_desc_data_shift += (2+ name_descriptor->descriptor_length);
 					break;
				}
				case TRANSPORT_PROTOCOL_DESCRIPTOR:
				{
					u64 pre_processing_pos;
					GF_M2TS_TRANSPORT_PROTOCOL_DESCRIPTOR* protocol_descriptor;
					GF_SAFEALLOC(protocol_descriptor, GF_M2TS_TRANSPORT_PROTOCOL_DESCRIPTOR);
					application->application_descriptors_id[application->index_app_desc_id] = temp_descriptor_tag;
					application->index_app_desc_id++;
					nb_of_protocol++;
					protocol_descriptor->descriptor_tag = temp_descriptor_tag;
					protocol_descriptor->descriptor_length = gf_bs_read_int(bs,8);
					pre_processing_pos = gf_bs_get_position(bs);
					protocol_descriptor->protocol_id = gf_bs_read_int(bs,16);
					protocol_descriptor->transport_protocol_label = gf_bs_read_int(bs,8);
					switch (protocol_descriptor->protocol_id) {
						case CAROUSEL:
						{
							GF_M2TS_OBJECT_CAROUSEL_SELECTOR_BYTE* Carousel_selector_byte;
							GF_SAFEALLOC(Carousel_selector_byte, GF_M2TS_OBJECT_CAROUSEL_SELECTOR_BYTE);
							Carousel_selector_byte->remote_connection = gf_bs_read_int(bs,1);
							gf_bs_read_int(bs,7); /* bit shifting */
							if (Carousel_selector_byte->remote_connection) {
								Carousel_selector_byte->original_network_id = gf_bs_read_int(bs,16);
								Carousel_selector_byte->transport_stream_id = gf_bs_read_int(bs,16);
								Carousel_selector_byte->service_id = gf_bs_read_int(bs,16);
							}
							Carousel_selector_byte->component_tag = gf_bs_read_int(bs,8);
							protocol_descriptor->selector_byte = Carousel_selector_byte;
							break;
						}
						case TRANSPORT_HTTP:
						{					
							u32 i;								
							GF_M2TS_TRANSPORT_HTTP_SELECTOR_BYTE* Transport_http_selector_byte;
							GF_SAFEALLOC(Transport_http_selector_byte, GF_M2TS_TRANSPORT_HTTP_SELECTOR_BYTE);					
							Transport_http_selector_byte->URL_base_length = gf_bs_read_int(bs,8);
							//printf("Transport_http_selector_byte->URL_base_length %d \n",Transport_http_selector_byte->URL_base_length);
							Transport_http_selector_byte->URL_base_byte = (char*)gf_calloc(Transport_http_selector_byte->URL_base_length+1,sizeof(char));
							gf_bs_read_data(bs,Transport_http_selector_byte->URL_base_byte ,(u32)(Transport_http_selector_byte->URL_base_length));
							Transport_http_selector_byte->URL_base_byte[Transport_http_selector_byte->URL_base_length] = 0;
							Transport_http_selector_byte->URL_extension_count = gf_bs_read_int(bs,8);
							if (Transport_http_selector_byte->URL_extension_count) {
								Transport_http_selector_byte->URL_extentions = (GF_M2TS_TRANSPORT_HTTP_URL_EXTENTION*) gf_calloc(Transport_http_selector_byte->URL_extension_count,sizeof(GF_M2TS_TRANSPORT_HTTP_URL_EXTENTION));
								for (i=0; i < Transport_http_selector_byte->URL_extension_count;i++) {
									Transport_http_selector_byte->URL_extentions[i].URL_extension_length = gf_bs_read_int(bs,8);
									Transport_http_selector_byte->URL_extentions[i].URL_extension_byte = (char*)gf_calloc(Transport_http_selector_byte->URL_extentions[i].URL_extension_length+1,sizeof(char));
									gf_bs_read_data(bs,Transport_http_selector_byte->URL_extentions[i].URL_extension_byte,(u32)(Transport_http_selector_byte->URL_extentions[i].URL_extension_length));
									Transport_http_selector_byte->URL_extentions[i].URL_extension_byte[Transport_http_selector_byte->URL_extentions[i].URL_extension_length] = 0;
								}
							}
							protocol_descriptor->selector_byte = Transport_http_selector_byte;
							break;
						}
						default:
						{
							GF_LOG(GF_LOG_INFO, GF_LOG_CONTAINER, ("[Process AIT] Protocol ID %d unsupported, ignoring the selector byte \n",protocol_descriptor->protocol_id));
						}
					}
					if (pre_processing_pos+protocol_descriptor->descriptor_length != gf_bs_get_position(bs)) {
						GF_LOG(GF_LOG_INFO, GF_LOG_CONTAINER, ("[Process AIT] Descriptor data processed length error. Difference between byte shifting %d and descriptor length %d \n",(gf_bs_get_position(bs) -  pre_processing_pos),protocol_descriptor->descriptor_length));
						if (protocol_descriptor->protocol_id == CAROUSEL) {						
							GF_M2TS_OBJECT_CAROUSEL_SELECTOR_BYTE* Carousel_selector_byte = (GF_M2TS_OBJECT_CAROUSEL_SELECTOR_BYTE*) protocol_descriptor->selector_byte;								
							gf_free(Carousel_selector_byte);								
						} else if(protocol_descriptor->protocol_id ==TRANSPORT_HTTP) {
							u32 i;
							GF_M2TS_TRANSPORT_HTTP_SELECTOR_BYTE* Transport_http_selector_byte = (GF_M2TS_TRANSPORT_HTTP_SELECTOR_BYTE*) protocol_descriptor->selector_byte;								
							gf_free(Transport_http_selector_byte->URL_base_byte);
							for(i = 0; i < Transport_http_selector_byte->URL_extension_count;i++){										
								gf_free(Transport_http_selector_byte->URL_extentions[i].URL_extension_byte);
							}
							gf_free(Transport_http_selector_byte);						
						}
						gf_free(protocol_descriptor);
						return GF_BAD_PARAM;
					}
					gf_list_add(application->application_descriptors,protocol_descriptor);					
					app_desc_data_shift += (2+ protocol_descriptor->descriptor_length);
					break;
				}
				case SIMPLE_APPLICATION_LOCATION_DESCRIPTOR:
				{
					u64 pre_processing_pos;					
					GF_M2TS_SIMPLE_APPLICATION_LOCATION* Simple_application_location;
					GF_SAFEALLOC(Simple_application_location, GF_M2TS_SIMPLE_APPLICATION_LOCATION);
					application->application_descriptors_id[application->index_app_desc_id] = temp_descriptor_tag;
					application->index_app_desc_id++;
					Simple_application_location->descriptor_tag = temp_descriptor_tag;
					Simple_application_location->descriptor_length = gf_bs_read_int(bs,8);
					pre_processing_pos = gf_bs_get_position(bs);
					Simple_application_location->initial_path_bytes = (char*)gf_calloc(Simple_application_location->descriptor_length+1,sizeof(char));
					gf_bs_read_data(bs,Simple_application_location->initial_path_bytes ,(u32)(Simple_application_location->descriptor_length));
					Simple_application_location->initial_path_bytes[Simple_application_location->descriptor_length] = 0;
					if (pre_processing_pos+Simple_application_location->descriptor_length != gf_bs_get_position(bs)) {
						GF_LOG(GF_LOG_INFO, GF_LOG_CONTAINER, ("[Process AIT] Descriptor data processed length error. Difference between byte shifting %d and descriptor length %d \n",(gf_bs_get_position(bs) -  pre_processing_pos),Simple_application_location->descriptor_length));
						gf_free(Simple_application_location->initial_path_bytes);
						gf_free(Simple_application_location);
						return GF_BAD_PARAM;
					}
					gf_list_add(application->application_descriptors,Simple_application_location);
					app_desc_data_shift += (2+ Simple_application_location->descriptor_length);
					break;
				}
				case APPLICATION_USAGE_DESCRIPTOR:
				{
					u64 pre_processing_pos;
					GF_M2TS_APPLICATION_USAGE* Application_usage;
					GF_SAFEALLOC(Application_usage, GF_M2TS_APPLICATION_USAGE);
					application->application_descriptors_id[application->index_app_desc_id] = temp_descriptor_tag;
					application->index_app_desc_id++;
					Application_usage->descriptor_tag = temp_descriptor_tag;
					Application_usage->descriptor_length = gf_bs_read_int(bs,8);
					pre_processing_pos = gf_bs_get_position(bs);
					Application_usage->usage_type = gf_bs_read_int(bs,8);
					if (pre_processing_pos+Application_usage->descriptor_length != gf_bs_get_position(bs)) {
						GF_LOG(GF_LOG_INFO, GF_LOG_CONTAINER, ("[Process AIT] Descriptor data processed length error. Difference between byte shifting %d and descriptor length %d \n",(gf_bs_get_position(bs) -  pre_processing_pos),Application_usage->descriptor_length));
						gf_free(Application_usage);
						return GF_BAD_PARAM;
					}
					gf_list_add(application->application_descriptors,Application_usage);
					app_desc_data_shift += (2+ Application_usage->descriptor_length);
					break;
				}
				case APPLICATION_BOUNDARY_DESCRIPTOR:
				{
					u64 pre_processing_pos;
					u32 i;
					GF_M2TS_APPLICATION_BOUNDARY_DESCRIPTOR* boundary_descriptor;
					GF_SAFEALLOC(boundary_descriptor, GF_M2TS_APPLICATION_BOUNDARY_DESCRIPTOR);
					application->application_descriptors_id[application->index_app_desc_id] = temp_descriptor_tag;
					application->index_app_desc_id++;
					boundary_descriptor->descriptor_tag = temp_descriptor_tag;
					boundary_descriptor->descriptor_length = gf_bs_read_int(bs,8);
					pre_processing_pos = gf_bs_get_position(bs);
					boundary_descriptor->boundary_extension_count = gf_bs_read_int(bs,8);
					if (boundary_descriptor->boundary_extension_count > 0) {
						boundary_descriptor->boundary_extension_info = (GF_M2TS_APPLICATION_BOUNDARY_EXTENSION_INFO*)gf_calloc(boundary_descriptor->boundary_extension_count,sizeof(GF_M2TS_APPLICATION_BOUNDARY_EXTENSION_INFO));
						for (i=0;i<boundary_descriptor->boundary_extension_count;i++) {
							boundary_descriptor->boundary_extension_info[i].boundary_extension_length = gf_bs_read_int(bs,8);
							if (boundary_descriptor->boundary_extension_info[i].boundary_extension_length > 0) {
								boundary_descriptor->boundary_extension_info[i].boundary_extension_byte = (char*)gf_calloc(boundary_descriptor->boundary_extension_info[i].boundary_extension_length+1,sizeof(char));
								gf_bs_read_data(bs,boundary_descriptor->boundary_extension_info[i].boundary_extension_byte ,(u8)(boundary_descriptor->boundary_extension_info[i].boundary_extension_length));
								boundary_descriptor->boundary_extension_info[i].boundary_extension_byte[boundary_descriptor->boundary_extension_info[i].boundary_extension_length] = 0;
							}
						}
					}
					if (pre_processing_pos+boundary_descriptor->descriptor_length != gf_bs_get_position(bs)) {
						GF_LOG(GF_LOG_INFO, GF_LOG_CONTAINER, ("[Process AIT] Descriptor data processed length error. Difference between byte shifting %d and descriptor length %d \n",(gf_bs_get_position(bs) -  pre_processing_pos),boundary_descriptor->descriptor_length));
						if (boundary_descriptor->boundary_extension_count > 0) {
							u32 i;
							for (i=0;i<boundary_descriptor->boundary_extension_count;i++) {
								if (boundary_descriptor->boundary_extension_info[i].boundary_extension_length > 0) {
									gf_free(boundary_descriptor->boundary_extension_info[i].boundary_extension_byte);
								}
							}
							gf_free(boundary_descriptor->boundary_extension_info);
						}
						gf_free(boundary_descriptor);
						return GF_BAD_PARAM;
					}
					gf_list_add(application->application_descriptors,boundary_descriptor);
					app_desc_data_shift += (2+ boundary_descriptor->descriptor_length);
					break;					
				}
				default:
				{
					u8 length;
					GF_LOG(GF_LOG_INFO, GF_LOG_CONTAINER, ("[Process AIT] Descriptor tag %d unknown, ignoring the descriptor \n",temp_descriptor_tag));
					/* get descriptor's length */
					length = gf_bs_read_int(bs,8);
					/* bit shifting (eq descriptor's length)*/
					gf_bs_read_int(bs,8*length);					
				}

			}

		}
		ait_app_data_shift += application->application_descriptors_loop_length; 
		gf_list_add(ait->application,application);		
	}

	data_shift +=ait->application_loop_length;
	ait->CRC_32 = gf_bs_read_int(bs,32);
	data_shift += 4;


	if (data_shift != data_size) {
		GF_LOG(GF_LOG_INFO, GF_LOG_CONTAINER, ("[Process AIT] AIT processed length error. Difference between byte shifting %d and data size %d \n",data_shift,data_size));
		return GF_BAD_PARAM;
	}

	return GF_OK;
}

void gf_ait_destroy(GF_M2TS_AIT* ait)
{
	u32 common_descr_numb, app_numb;
	
	/* delete de Elementary Stream part of the AIT structure */ 	
	common_descr_numb = 0;
	app_numb = 0;
	
	/* delete the common descriptors */ 
	common_descr_numb = gf_list_count(ait->common_descriptors);
	while(common_descr_numb != 0){
		//TODO
	};
	gf_list_del(ait->common_descriptors);
	
	/* delete the applications and their descriptors */
	app_numb = gf_list_count(ait->application);
	while(app_numb != 0){
		GF_M2TS_AIT_APPLICATION* application = gf_list_get(ait->application,0);
		gf_ait_application_destroy(application);
		gf_list_rem(ait->application,0);
		app_numb = gf_list_count(ait->application);
	}
	gf_list_del(ait->application);
	gf_free(ait);
}

void gf_ait_application_destroy(GF_M2TS_AIT_APPLICATION* application)
{
		
	u32 app_desc_num,i,app_descr_index;
	  
	app_desc_num = app_descr_index = i = 0; 
	app_desc_num = gf_list_count(application->application_descriptors);
	  
	while (app_desc_num != 0) {
		u32 descr_tag;
		descr_tag = application->application_descriptors_id[i];
		printf("descr_tag %d\n", descr_tag);
		switch(descr_tag) {
			case APPLICATION_DESCRIPTOR:
			{
				GF_M2TS_APPLICATION_DESCRIPTOR* application_descriptor = (GF_M2TS_APPLICATION_DESCRIPTOR*)gf_list_get(application->application_descriptors,0);
				gf_free(application_descriptor);  
				break;
			}
			case APPLICATION_NAME_DESCRIPTOR:
			{
				GF_M2TS_APPLICATION_NAME_DESCRIPTOR* name_descriptor = (GF_M2TS_APPLICATION_NAME_DESCRIPTOR*)gf_list_get(application->application_descriptors,0);
				gf_free(name_descriptor->application_name_char);
				gf_free(name_descriptor);
				break;
			}
			case TRANSPORT_PROTOCOL_DESCRIPTOR:
			{
				GF_M2TS_TRANSPORT_PROTOCOL_DESCRIPTOR* protocol_descriptor = (GF_M2TS_TRANSPORT_PROTOCOL_DESCRIPTOR*)gf_list_get(application->application_descriptors,0);  					
				switch (protocol_descriptor->protocol_id) {
					case CAROUSEL:
					{
						GF_M2TS_OBJECT_CAROUSEL_SELECTOR_BYTE* Carousel_selector_byte = (GF_M2TS_OBJECT_CAROUSEL_SELECTOR_BYTE*)protocol_descriptor->selector_byte;
						gf_free(Carousel_selector_byte);
						break;
					}
					case TRANSPORT_HTTP:
					{					
						u32 i;								
						GF_M2TS_TRANSPORT_HTTP_SELECTOR_BYTE* Transport_http_selector_byte = (GF_M2TS_TRANSPORT_HTTP_SELECTOR_BYTE*)protocol_descriptor->selector_byte;
						gf_free(Transport_http_selector_byte->URL_base_byte);
						if (Transport_http_selector_byte->URL_extension_count) {
							for (i=0; i < Transport_http_selector_byte->URL_extension_count;i++) { 										
								gf_free(Transport_http_selector_byte->URL_extentions[i].URL_extension_byte);
							}
							gf_free(Transport_http_selector_byte->URL_extentions);
						}
						gf_free(Transport_http_selector_byte);
						break;
					}
					default:
					{
						GF_LOG(GF_LOG_INFO, GF_LOG_CONTAINER, ("[Process AIT] Protocol ID %d unsupported, ignoring the selector byte \n",protocol_descriptor->protocol_id));
					}
				}					
				gf_free(protocol_descriptor);					
				break;
			}
			case SIMPLE_APPLICATION_LOCATION_DESCRIPTOR:
			{			
				GF_M2TS_SIMPLE_APPLICATION_LOCATION* Simple_application_location = (GF_M2TS_SIMPLE_APPLICATION_LOCATION*)gf_list_get(application->application_descriptors,0);
				gf_free(Simple_application_location->initial_path_bytes);
				gf_free(Simple_application_location);
				break;
			}
			case APPLICATION_USAGE_DESCRIPTOR:
			{
				GF_M2TS_APPLICATION_USAGE* Application_usage = (GF_M2TS_APPLICATION_USAGE*)gf_list_get(application->application_descriptors,0);
				gf_free(Application_usage);
				break;
			}
			case APPLICATION_BOUNDARY_DESCRIPTOR:
			{
				u32 i;
				GF_M2TS_APPLICATION_BOUNDARY_DESCRIPTOR* boundary_descriptor = (GF_M2TS_APPLICATION_BOUNDARY_DESCRIPTOR*)gf_list_get(application->application_descriptors, 0);
				if (boundary_descriptor->boundary_extension_count > 0) {
					for (i=0; i<boundary_descriptor->boundary_extension_count; i++) {
						if (boundary_descriptor->boundary_extension_info[i].boundary_extension_length > 0) {
							gf_free(boundary_descriptor->boundary_extension_info[i].boundary_extension_byte);
						}							
					}
					gf_free(boundary_descriptor->boundary_extension_info);
				}
				gf_free(boundary_descriptor);
				break;					
			}
			default:
			{
			}
		}
		gf_list_rem(application->application_descriptors,0);
		app_desc_num = gf_list_count(application->application_descriptors);
		i++;
	}
	gf_list_del(application->application_descriptors);
	gf_free(application);
}

/* DSMCC */

void on_dsmcc_section(GF_M2TS_Demuxer *ts, u32 evt_type, void *par) 
{
	GF_M2TS_SL_PCK *pck = (GF_M2TS_SL_PCK *)par;
	char *data;
	u32 u32_data_size;
	u32 u32_table_id;

	if (ts->dsmcc_controler == NULL) {
		ts->dsmcc_controler = gf_m2ts_init_dsmcc_overlord();	
	}

	if (evt_type == GF_M2TS_EVT_DSMCC_FOUND) {
		GF_Err e;
		GF_M2TS_DSMCC_SECTION* dsmcc;
		data = pck->data;
		u32_data_size = pck->data_len;
		u32_table_id = data[0];	
		GF_SAFEALLOC(dsmcc,GF_M2TS_DSMCC_SECTION);

		e = gf_m2ts_process_dsmcc(ts,dsmcc,data,u32_data_size,u32_table_id);	
		assert(e == GF_OK);
	}
}

GF_M2TS_DSMCC_OVERLORD* gf_m2ts_init_dsmcc_overlord() {
	GF_M2TS_DSMCC_OVERLORD* dsmcc_controler;
	GF_SAFEALLOC(dsmcc_controler,GF_M2TS_DSMCC_OVERLORD);	
	dsmcc_controler->dsmcc_modules = gf_list_new();
	return dsmcc_controler;
}

GF_Err gf_m2ts_process_dsmcc(GF_M2TS_Demuxer* ts,GF_M2TS_DSMCC_SECTION *dsmcc, char  *data, u32 data_size, u32 table_id)
{
	GF_BitStream *bs;
	GF_M2TS_DSMCC_OVERLORD* dsmcc_overlord;
	u32 data_shift,reserved_test;

	data_shift = 0;
	dsmcc_overlord = (GF_M2TS_DSMCC_OVERLORD*)ts->dsmcc_controler;
	//first_section = *first_section_received;
	bs = gf_bs_new(data,data_size,GF_BITSTREAM_READ);

	dsmcc->table_id = gf_bs_read_int(bs,8);	
	dsmcc->section_syntax_indicator = gf_bs_read_int(bs,1);
	dsmcc->private_indicator = gf_bs_read_int(bs,1);
	reserved_test = gf_bs_read_int(bs,2);
	if (reserved_test != 3) {
		GF_LOG(GF_LOG_INFO, GF_LOG_CONTAINER, ("[Process DSMCC] test reserved flag is not at 3. Corrupted section, abording processing\n"));
		return GF_BAD_PARAM;
	}
	dsmcc->dsmcc_section_length = gf_bs_read_int(bs,12);
	if (dsmcc->dsmcc_section_length > DSMCC_SECTION_LENGTH_MAX) {
		GF_LOG(GF_LOG_INFO, GF_LOG_CONTAINER, ("[Process DSMCC] section length should not exceed 4096. Wrong section, abording processing \n"));
		return GF_BAD_PARAM;
	}
	dsmcc->table_id_extension = gf_bs_read_int(bs,16);

	/*bit shifting */
	gf_bs_read_int(bs,2);

	dsmcc->version_number = gf_bs_read_int(bs,5);
	if(dsmcc->version_number != 0 &&(dsmcc->table_id == GF_M2TS_TABLE_ID_DSM_CC_ENCAPSULATED_DATA || dsmcc->table_id == GF_M2TS_TABLE_ID_DSM_CC_UN_MESSAGE)){
		GF_LOG(GF_LOG_INFO, GF_LOG_CONTAINER, ("[Process DSMCC] Version number should be 0 for Encapsulated Data or UN Message, abording processing \n"));
		return GF_BAD_PARAM;
	}

	dsmcc->current_next_indicator = gf_bs_read_int(bs,1);	
	if (!dsmcc->current_next_indicator) {
		GF_LOG(GF_LOG_INFO, GF_LOG_CONTAINER, ("[Process DSMCC] current next indicator should be at 1 \n"));
		return GF_BAD_PARAM;
	}
	dsmcc->section_number = gf_bs_read_int(bs,8);
	/*if(dsmcc->section_number >0 && ((*first_section_received) == 0)){
		GF_LOG(GF_LOG_INFO, GF_LOG_CONTAINER, ("[Process DSMCC] Wainting for the first section \n"));
		return GF_BAD_PARAM;
	}
	*first_section_received = 1;*/
	dsmcc->last_section_number = gf_bs_read_int(bs,8);	
	printf("\nsection_number %d last_section_number %d\n",dsmcc->section_number,dsmcc->last_section_number);
	printf("dsmcc->table_id %d \n",dsmcc->table_id);
	switch (dsmcc->table_id) {
		case GF_M2TS_TABLE_ID_DSM_CC_ENCAPSULATED_DATA:
		{
			data_shift = (u32)(gf_bs_get_position(bs));
			break;
		}
		case GF_M2TS_TABLE_ID_DSM_CC_UN_MESSAGE:			
		case GF_M2TS_TABLE_ID_DSM_CC_DOWNLOAD_DATA_MESSAGE:
		{
			data_shift = (u32)(gf_bs_get_position(bs));
			if(!dsmcc_module_state(dsmcc_overlord,dsmcc->table_id_extension,dsmcc->version_number)){
				gf_m2ts_dsmcc_download_data(dsmcc,data,bs,&data_shift);
			}
			break;
		}
		case GF_M2TS_TABLE_ID_DSM_CC_STREAM_DESCRIPTION:
		{
			data_shift = (u32)(gf_bs_get_position(bs));
			break;
		}
		default:
		{
			GF_LOG(GF_LOG_INFO, GF_LOG_CONTAINER, ("[Process DSMCC] Unknown DSMCC Section Type \n"));		
			break;
		}
	}

	if (dsmcc->section_syntax_indicator == 0) {
		dsmcc->checksum = gf_bs_read_int(bs,32);	
	} else {
		dsmcc->CRC_32= gf_bs_read_int(bs,32);
	}

	data_shift = (u32)(gf_bs_get_position(bs));

	/*if(data_shift != dsmcc->dsmcc_section_length){.
		GF_LOG(GF_LOG_INFO, GF_LOG_CONTAINER, ("[Process AIT] AIT processed length error. Difference between byte shifting %d and data size %d \n",data_shift,data_size));
		return GF_BAD_PARAM;
	}*/

	gf_m2ts_extract_info(ts,dsmcc);
	gf_m2ts_dsmcc_section_delete(dsmcc);

	return GF_OK;
}

static GF_Err gf_m2ts_dsmcc_download_data(GF_M2TS_DSMCC_SECTION *dsmcc, char  *data, GF_BitStream *bs,u32* data_shift)
{
	GF_M2TS_DSMCC_DOWNLOAD_DATA_MESSAGE* DataMessage;	

	GF_SAFEALLOC(DataMessage,GF_M2TS_DSMCC_DOWNLOAD_DATA_MESSAGE);

	/* Header */
	gf_m2ts_dsmcc_process_message_header(&DataMessage->DownloadDataHeader,data,bs,data_shift,1);

	printf("DataMessage->DownloadDataHeader.messageId %d \n",DataMessage->DownloadDataHeader.messageId);

	switch (DataMessage->DownloadDataHeader.messageId)
	{
		case DOWNLOAD_INFO_REPONSE_INDICATION:
		{
			u32 i;
			GF_M2TS_DSMCC_DOWNLOAD_INFO_RESP_INDIC* DownloadInfoIndication;
			GF_SAFEALLOC(DownloadInfoIndication,GF_M2TS_DSMCC_DOWNLOAD_INFO_RESP_INDIC);
			DataMessage->dataMessagePayload = DownloadInfoIndication;

			/* Payload */
			DownloadInfoIndication->downloadId = gf_bs_read_int(bs,32);
			DownloadInfoIndication->blockSize = gf_bs_read_int(bs,16);
			DownloadInfoIndication->windowSize = gf_bs_read_int(bs,8);
			DownloadInfoIndication->ackPeriod = gf_bs_read_int(bs,8);
			DownloadInfoIndication->tCDownloadWindow = gf_bs_read_int(bs,32);
			DownloadInfoIndication->tCDownloadScenario = gf_bs_read_int(bs,32);
			/* Compatibility Descr */
			gf_m2ts_dsmcc_process_compatibility_descriptor(&DownloadInfoIndication->CompatibilityDescr,data,bs,data_shift);
			DownloadInfoIndication->numberOfModules = gf_bs_read_int(bs,16);
			DownloadInfoIndication->Modules = (GF_M2TS_DSMCC_INFO_MODULES*)gf_calloc(DownloadInfoIndication->numberOfModules,sizeof(GF_M2TS_DSMCC_INFO_MODULES));
			for (i=0; i<DownloadInfoIndication->numberOfModules; i++) {
				DownloadInfoIndication->Modules[i].moduleId = gf_bs_read_int(bs,16);
				DownloadInfoIndication->Modules[i].moduleSize = gf_bs_read_int(bs,32);
				DownloadInfoIndication->Modules[i].moduleVersion = gf_bs_read_int(bs,8);
				DownloadInfoIndication->Modules[i].moduleInfoLength = gf_bs_read_int(bs,8);				
				DownloadInfoIndication->Modules[i].moduleInfoByte = (char*)gf_calloc(DownloadInfoIndication->Modules[i].moduleInfoLength,sizeof(char));	
				gf_bs_read_data(bs,DownloadInfoIndication->Modules[i].moduleInfoByte,(u8)(DownloadInfoIndication->Modules[i].moduleInfoLength));
			}

			DownloadInfoIndication->privateDataLength = gf_bs_read_int(bs,16);    
			DownloadInfoIndication->privateDataByte = (char*)gf_calloc(DownloadInfoIndication->privateDataLength,sizeof(char));
			gf_bs_read_data(bs,DownloadInfoIndication->privateDataByte,(u8)(DownloadInfoIndication->privateDataLength));
			
			break;
		}
		case DOWNLOAD_DATA_BLOCK:
		{				
			GF_M2TS_DSMCC_DOWNLOAD_DATA_BLOCK* DownloadDataBlock;
			GF_SAFEALLOC(DownloadDataBlock,GF_M2TS_DSMCC_DOWNLOAD_DATA_BLOCK);
			DataMessage->dataMessagePayload = DownloadDataBlock;				
			DownloadDataBlock->moduleId = gf_bs_read_int(bs,16);
			printf("DownloadDataBlock->moduleId %d \n",DownloadDataBlock->moduleId);
			DownloadDataBlock->moduleVersion = gf_bs_read_int(bs,8); 
			DownloadDataBlock->reserved = gf_bs_read_int(bs,8);
			if (DownloadDataBlock->reserved != 0xFF) {
				GF_LOG(GF_LOG_INFO, GF_LOG_CONTAINER, ("[Process DSMCC] DataHeader reserved slot does not have the correct value, abording the processing \n"));
				return GF_BAD_PARAM;
			}
			DownloadDataBlock->blockNumber = gf_bs_read_int(bs,16);
			printf("DownloadDataBlock->blockNumber %d \n\n",DownloadDataBlock->blockNumber);
			DownloadDataBlock->dataBlocksize = DataMessage->DownloadDataHeader.messageLength - DataMessage->DownloadDataHeader.adaptationLength - 6; /* the first 6 bytes of the Data Block message */
			DownloadDataBlock->blockDataByte = (char*)gf_calloc(DownloadDataBlock->dataBlocksize,sizeof(char));
			gf_bs_read_data(bs,DownloadDataBlock->blockDataByte,(u8)(DownloadDataBlock->dataBlocksize));
			break;
		}
		case DOWNLOAD_DATA_REQUEST:
		{
			GF_M2TS_DSMCC_DOWNLOAD_DATA_REQUEST_MESSAGE* DownloadDataRequest;
			GF_SAFEALLOC(DownloadDataRequest,GF_M2TS_DSMCC_DOWNLOAD_DATA_REQUEST_MESSAGE);
			DataMessage->dataMessagePayload = DownloadDataRequest;	
			DownloadDataRequest->moduleId = gf_bs_read_int(bs,16);
			DownloadDataRequest->blockNumber = gf_bs_read_int(bs,16);
			DownloadDataRequest->downloadReason  = gf_bs_read_int(bs,8);
			break;
		}
		case DOWNLOAD_DATA_CANCEL:
		{
			GF_M2TS_DSMCC_DOWNLOAD_CANCEL* DownloadCancel;
			GF_SAFEALLOC(DownloadCancel,GF_M2TS_DSMCC_DOWNLOAD_CANCEL);
			DataMessage->dataMessagePayload = DownloadCancel;	
			DownloadCancel->downloadId = gf_bs_read_int(bs,32);
			DownloadCancel->moduleId = gf_bs_read_int(bs,16);
			DownloadCancel->blockNumber = gf_bs_read_int(bs,16);
			DownloadCancel->downloadCancelReason = gf_bs_read_int(bs,8);
			DownloadCancel->reserved = gf_bs_read_int(bs,8);
			DownloadCancel->privateDataLength = gf_bs_read_int(bs,16);
			if (DownloadCancel->privateDataLength){
				DownloadCancel->privateDataByte = (char*)gf_calloc(DownloadCancel->privateDataLength,sizeof(char));
				gf_bs_read_data(bs,DownloadCancel->privateDataByte,(u8)(DownloadCancel->privateDataLength));
			}
			break;
		}
		case DOWNLOAD_SERVER_INITIATE:
		{
			GF_M2TS_DSMCC_DOWNLOAD_SERVER_INIT* DownloadServerInit;
			GF_SAFEALLOC(DownloadServerInit,GF_M2TS_DSMCC_DOWNLOAD_SERVER_INIT);
			DataMessage->dataMessagePayload = DownloadServerInit;
			gf_bs_read_data(bs,DownloadServerInit->serverId,20);			
			gf_m2ts_dsmcc_process_compatibility_descriptor(&DownloadServerInit->CompatibilityDescr,data,bs,data_shift);
			break;
		}
		default:
		{
			GF_LOG(GF_LOG_INFO, GF_LOG_CONTAINER, ("[Process DSMCC] Unknown dataMessagePayload Type \n"));
			break;
		}
	}

	dsmcc->DSMCC_Extension = DataMessage;

	return GF_OK;
}
	
				


static GF_Err gf_m2ts_dsmcc_process_message_header(GF_M2TS_DSMCC_MESSAGE_DATA_HEADER *MessageHeader, char* data,GF_BitStream *bs,u32* data_shift,u32 mode)
{
	u32 byte_shift;

	byte_shift = *data_shift;
	
	MessageHeader->protocolDiscriminator = gf_bs_read_int(bs,8);
	if (MessageHeader->protocolDiscriminator != 0x11) {
		GF_LOG(GF_LOG_INFO, GF_LOG_CONTAINER, ("[Process DSMCC] DataHeader Protocol Discriminator slot does not have the correct value, abording the processing \n"));
		return GF_BAD_PARAM;
	}
	MessageHeader->dsmccType = gf_bs_read_int(bs,8);
	MessageHeader->messageId = gf_bs_read_int(bs,16);
	/* mode 0 for Message Header - 1 for Download Data header */
	if (mode == 0) {
		MessageHeader->transactionId = gf_bs_read_int(bs,32);
	} else if (mode == 1) {
		MessageHeader->downloadId = gf_bs_read_int(bs,32);
	}
	printf("MessageHeader->downloadId %d \n",MessageHeader->downloadId);
	MessageHeader->reserved = gf_bs_read_int(bs,8);
	if (MessageHeader->reserved != 0xFF) {
		GF_LOG(GF_LOG_INFO, GF_LOG_CONTAINER, ("[Process DSMCC] DataHeader reserved slot does not have the correct value, abording the processing \n"));
		return GF_BAD_PARAM;
	}
	MessageHeader->adaptationLength = gf_bs_read_int(bs,8);
	MessageHeader->header_length = ((u32)(gf_bs_get_position(bs)) - byte_shift);
	MessageHeader->messageLength = gf_bs_read_int(bs,16);

	if (MessageHeader->adaptationLength > 0) {
		MessageHeader->DsmccAdaptationHeader = (GF_M2TS_DSMCC_ADAPTATION_HEADER*)gf_calloc(1, sizeof(GF_M2TS_DSMCC_ADAPTATION_HEADER));
		MessageHeader->DsmccAdaptationHeader->adaptationType = gf_bs_read_int(bs,8);
		MessageHeader->DsmccAdaptationHeader->adaptationDataByte = (char*)gf_calloc(MessageHeader->adaptationLength-1, sizeof(char));
		gf_bs_read_data(bs, MessageHeader->DsmccAdaptationHeader->adaptationDataByte, (u8)(MessageHeader->adaptationLength));
	}

	*data_shift = (u32)(gf_bs_get_position(bs));

	return GF_OK;
}

static GF_Err gf_m2ts_dsmcc_process_compatibility_descriptor(GF_M2TS_DSMCC_COMPATIBILITY_DESCRIPTOR *CompatibilityDesc, char* data,GF_BitStream *bs,u32* data_shift)
{
	u32 i,j;

	CompatibilityDesc->compatibilityDescriptorLength = gf_bs_read_int(bs,16);

	if (CompatibilityDesc->compatibilityDescriptorLength) {
		CompatibilityDesc->descriptorCount = gf_bs_read_int(bs,16);
		if (CompatibilityDesc->descriptorCount) {
			CompatibilityDesc->Descriptor = (GF_M2TS_DSMCC_DESCRIPTOR*) gf_calloc(CompatibilityDesc->descriptorCount, sizeof(GF_M2TS_DSMCC_DESCRIPTOR));
			for (i=0; i<CompatibilityDesc->descriptorCount; i++) {
				CompatibilityDesc->Descriptor[i].descriptorType = gf_bs_read_int(bs,8);
				CompatibilityDesc->Descriptor[i].descriptorLength = gf_bs_read_int(bs,8);
				CompatibilityDesc->Descriptor[i].specifierType = gf_bs_read_int(bs,8);
				CompatibilityDesc->Descriptor[i].specifierData = gf_bs_read_int(bs,21);
				CompatibilityDesc->Descriptor[i].model = gf_bs_read_int(bs,16);
				CompatibilityDesc->Descriptor[i].version = gf_bs_read_int(bs,16);
				CompatibilityDesc->Descriptor[i].subDescriptorCount = gf_bs_read_int(bs,8);
				if (CompatibilityDesc->Descriptor[i].subDescriptorCount) {
					CompatibilityDesc->Descriptor[i].SubDescriptor = (GF_M2TS_DSMCC_SUBDESCRIPTOR*)gf_calloc(CompatibilityDesc->Descriptor[i].subDescriptorCount,sizeof(GF_M2TS_DSMCC_SUBDESCRIPTOR));
					for (j=0; j>CompatibilityDesc->Descriptor[i].subDescriptorCount; j++) {
						CompatibilityDesc->Descriptor[i].SubDescriptor[j].subDescriptorType = gf_bs_read_int(bs,8);
						CompatibilityDesc->Descriptor[i].SubDescriptor[j].subDescriptorLength = gf_bs_read_int(bs,8);
						if (CompatibilityDesc->Descriptor[i].SubDescriptor[j].subDescriptorLength) {
							CompatibilityDesc->Descriptor[i].SubDescriptor[j].additionalInformation = (char*)gf_calloc(CompatibilityDesc->Descriptor[i].SubDescriptor[j].subDescriptorLength,sizeof(char));
							gf_bs_read_data(bs,CompatibilityDesc->Descriptor[i].SubDescriptor[j].additionalInformation,(u8)(CompatibilityDesc->Descriptor[i].SubDescriptor[j].subDescriptorLength));
						}
					}
				}
			}
		}
	}
	
	*data_shift = (u32)(gf_bs_get_position(bs));

	return GF_OK;
}

static GF_Err gf_m2ts_extract_info(GF_M2TS_Demuxer *ts,GF_M2TS_DSMCC_SECTION *dsmcc)
{	
	
	GF_M2TS_DSMCC_OVERLORD* dsmcc_overlord = (GF_M2TS_DSMCC_OVERLORD*)ts->dsmcc_controler;
	switch(dsmcc->table_id) {
		case GF_M2TS_TABLE_ID_DSM_CC_ENCAPSULATED_DATA:
		{
			break;
		}
		case GF_M2TS_TABLE_ID_DSM_CC_UN_MESSAGE:		
		case GF_M2TS_TABLE_ID_DSM_CC_DOWNLOAD_DATA_MESSAGE:
		{
			GF_M2TS_DSMCC_DOWNLOAD_DATA_MESSAGE* DataMessage = (GF_M2TS_DSMCC_DOWNLOAD_DATA_MESSAGE*)dsmcc->DSMCC_Extension;

			switch( DataMessage->DownloadDataHeader.messageId )
			{
				case DOWNLOAD_INFO_REPONSE_INDICATION:
				{
					u32 i, nb_modules;							
					GF_M2TS_DSMCC_DOWNLOAD_INFO_RESP_INDIC* DownloadInfoIndication = (GF_M2TS_DSMCC_DOWNLOAD_INFO_RESP_INDIC*)DataMessage->dataMessagePayload;
					nb_modules = gf_list_count(dsmcc_overlord->dsmcc_modules);
					for (i=0; i<DownloadInfoIndication->numberOfModules; i++) {
						if (!dsmcc_create_module_validation(&DownloadInfoIndication->Modules[i],DownloadInfoIndication->downloadId,dsmcc_overlord,nb_modules)) {
							u32 module_index;
							GF_M2TS_DSMCC_MODULE* dsmcc_module;
							GF_SAFEALLOC(dsmcc_module,GF_M2TS_DSMCC_MODULE);
							dsmcc_module->downloadId = DownloadInfoIndication->downloadId;
							dsmcc_module->moduleId = DownloadInfoIndication->Modules[i].moduleId;
							dsmcc_module->size = DownloadInfoIndication->Modules[i].moduleSize;
							dsmcc_module->version_number = DownloadInfoIndication->Modules[i].moduleVersion;
							dsmcc_module->buffer = (char*)gf_calloc(dsmcc_module->size,sizeof(char));
							module_index = gf_list_count(dsmcc_overlord->dsmcc_modules);
							dsmcc_overlord->processed[module_index].moduleId = dsmcc_module->moduleId;
							dsmcc_overlord->processed[module_index].downloadId = dsmcc_module->downloadId;
							dsmcc_overlord->processed[module_index].version_number = dsmcc_module->version_number;
							gf_list_add(dsmcc_overlord->dsmcc_modules,dsmcc_module);
						}
					}
					break;
				}
				case DOWNLOAD_DATA_BLOCK:
				{				
					u32 modules_count,i;
					GF_M2TS_DSMCC_DOWNLOAD_DATA_BLOCK* DownloadDataBlock = (GF_M2TS_DSMCC_DOWNLOAD_DATA_BLOCK*)DataMessage->dataMessagePayload;
					modules_count = gf_list_count(dsmcc_overlord->dsmcc_modules);
					for (i=0; i<modules_count; i++) {
						GF_M2TS_DSMCC_MODULE* dsmcc_module = gf_list_get(dsmcc_overlord->dsmcc_modules,i);
						/* Test if the data are compatible with the module configuration */
						if (!dsmcc_download_data_validation(DownloadDataBlock,dsmcc_module,DataMessage->DownloadDataHeader.downloadId)) {
							memcpy(dsmcc_module->buffer+dsmcc_module->byte_sift,DownloadDataBlock->blockDataByte,DownloadDataBlock->dataBlocksize*sizeof(char));
							dsmcc_module->byte_sift += DownloadDataBlock->dataBlocksize;									
							dsmcc_module->last_section_number = dsmcc->last_section_number;
							dsmcc_module->section_number++;
							if (dsmcc_module->section_number == (dsmcc_module->last_section_number+1)){
								dsmcc_module_complete(dsmcc_overlord,dsmcc_module,i);
							}
							break;
						}
					}							
					break;
				}
				case DOWNLOAD_DATA_REQUEST:
				{
					GF_LOG(GF_LOG_WARNING, GF_LOG_CONTAINER, ("[Process DSMCC] Unimplemented DOWNLOAD_DATA_REQUEST type\n"));
					assert(0);
					//GF_M2TS_DSMCC_DOWNLOAD_DATA_REQUEST_MESSAGE* DownloadDataRequest = (GF_M2TS_DSMCC_DOWNLOAD_DATA_REQUEST_MESSAGE*)DataMessage->dataMessagePayload;
					break;
				}
				case DOWNLOAD_DATA_CANCEL:
				{
					GF_LOG(GF_LOG_WARNING, GF_LOG_CONTAINER, ("[Process DSMCC] Unimplemented DOWNLOAD_DATA_CANCEL type\n"));
					assert(0);
					//GF_M2TS_DSMCC_DOWNLOAD_CANCEL* DownloadCancel = (GF_M2TS_DSMCC_DOWNLOAD_CANCEL*)DataMessage->dataMessagePayload;						
					break;
				}
				case DOWNLOAD_SERVER_INITIATE:
				{
					GF_LOG(GF_LOG_WARNING, GF_LOG_CONTAINER, ("[Process DSMCC] Unimplemented DOWNLOAD_SERVER_INITIATE type\n"));
					assert(0);
					//GF_M2TS_DSMCC_DOWNLOAD_SERVER_INIT* DownloadServerInit = (GF_M2TS_DSMCC_DOWNLOAD_SERVER_INIT*)DataMessage->dataMessagePayload;								
					break;
				}
				default:
				{
					GF_LOG(GF_LOG_INFO, GF_LOG_CONTAINER, ("[Process DSMCC] Unknown dataMessagePayload Type \n"));
					break;
				}
			} 			
			break;
		}
		case GF_M2TS_TABLE_ID_DSM_CC_STREAM_DESCRIPTION:
		{
			break;
		}
		default:
		{
			GF_LOG(GF_LOG_INFO, GF_LOG_CONTAINER, ("[Process DSMCC] Unknown DSMCC Section Type \n"));		
			break;
		}
	}
	return GF_OK;
}

static GF_Err dsmcc_create_module_validation(GF_M2TS_DSMCC_INFO_MODULES* InfoModules, u32 downloadId, GF_M2TS_DSMCC_OVERLORD* dsmcc_overlord,u32 nb_module){

	u32 i;
	for (i=0; i<nb_module; i++) {
		GF_M2TS_DSMCC_PROCESSED dsmcc_process = dsmcc_overlord->processed[i];
		if ((InfoModules->moduleId == dsmcc_process.moduleId) && (downloadId == dsmcc_process.downloadId)) {
			if (InfoModules->moduleVersion <= dsmcc_process.version_number) {
				GF_LOG(GF_LOG_INFO, GF_LOG_CONTAINER, ("[Process DSMCC] Module already intialized \n"));
				return GF_BAD_PARAM;
			} else if (InfoModules->moduleVersion > dsmcc_process.version_number) {
				GF_M2TS_DSMCC_MODULE* dsmcc_module = (GF_M2TS_DSMCC_MODULE*)gf_list_get(dsmcc_overlord->dsmcc_modules,i);
				dsmcc_module_delete(dsmcc_module);
				gf_list_rem(dsmcc_overlord->dsmcc_modules,i);
				dsmcc_process.version_number = InfoModules->moduleVersion;
				dsmcc_process.done = 0;
				return GF_OK;
			}
		}
	}
	return  GF_OK;
}

static GF_Err dsmcc_module_state(GF_M2TS_DSMCC_OVERLORD* dsmcc_overlord,u32 moduleId,u32 moduleVersion){

	u32 i,nb_module;
	nb_module = gf_list_count(dsmcc_overlord->dsmcc_modules);
	/* This test comes from the fact that the moduleVersion only borrow the least 5 significant bits of the moduleVersion conveys in DownloadDataBlock */
	/* If the moduleVersion is eq to 0x1F, it does not tell if it is clearly 0x1F or a superior value. So in this case it is better to process the data */
	/* If the moduleVersion is eq to 0x0, it means that the data conveys a DownloadDataResponse, so it has to be processed */
	if (moduleVersion != 0 || moduleVersion < 0x1F) {
		for (i=0; i<nb_module; i++) {
			GF_M2TS_DSMCC_PROCESSED dsmcc_process = dsmcc_overlord->processed[i];
			if ((moduleId == dsmcc_process.moduleId) && moduleVersion == dsmcc_process.version_number) {
				if (dsmcc_process.done) {
					GF_LOG(GF_LOG_INFO, GF_LOG_CONTAINER, ("[Process DSMCC] Module already processed \n"));
					return GF_BAD_PARAM;
				} else {
					return GF_OK;	
				}
			}
		}
	}
	return  GF_OK;
}

static GF_Err dsmcc_download_data_validation(GF_M2TS_DSMCC_DOWNLOAD_DATA_BLOCK* DownloadDataBlock,GF_M2TS_DSMCC_MODULE* dsmcc_module,u32 downloadId)
{
		if ((dsmcc_module->moduleId == DownloadDataBlock->moduleId) && (dsmcc_module->section_number == DownloadDataBlock->blockNumber) &&
			dsmcc_module->version_number == DownloadDataBlock->moduleVersion && (dsmcc_module->downloadId == downloadId)) {
			return GF_OK;
		}
		return  GF_BAD_PARAM;
}

static GF_Err dsmcc_module_complete(GF_M2TS_DSMCC_OVERLORD* dsmcc_overlord,GF_M2TS_DSMCC_MODULE* dsmcc_module,u32 moduleIndex)
{
	u32 i,nb_module;
	nb_module = gf_list_count(dsmcc_overlord->dsmcc_modules);
	for (i=0; i<nb_module; i++) {
		GF_M2TS_DSMCC_PROCESSED dsmcc_process = dsmcc_overlord->processed[i];
		if ((dsmcc_module->moduleId == dsmcc_process.moduleId) && dsmcc_module->version_number == dsmcc_process.version_number && dsmcc_module->downloadId == dsmcc_process.downloadId) {
			dsmcc_process.done = 1;
			/*process buffer*/
			dsmcc_module_delete(dsmcc_module);
			gf_list_rem(dsmcc_overlord->dsmcc_modules,moduleIndex);
		}
	}
	
	return  GF_OK;
}

/* Delete structure of the DSMCC data processing */

static GF_Err gf_m2ts_dsmcc_delete_compatibility_descriptor(GF_M2TS_DSMCC_COMPATIBILITY_DESCRIPTOR *CompatibilityDesc)
{
	u32 i,j;	
	if (CompatibilityDesc->compatibilityDescriptorLength) {
		if (CompatibilityDesc->descriptorCount) {
			for (i=0; i<CompatibilityDesc->descriptorCount; i++) {
				if (CompatibilityDesc->Descriptor[i].subDescriptorCount) {
					for (j=0; j>CompatibilityDesc->Descriptor[i].subDescriptorCount; j++) {
						if (CompatibilityDesc->Descriptor[i].SubDescriptor[j].subDescriptorLength) {
							gf_free(CompatibilityDesc->Descriptor[i].SubDescriptor[j].additionalInformation);
						}
					}
					gf_free(CompatibilityDesc->Descriptor[i].SubDescriptor);
				}
			}
			gf_free(CompatibilityDesc->Descriptor);
		}
	}
	return GF_OK;
}

static GF_Err gf_m2ts_dsmcc_delete_message_header(GF_M2TS_DSMCC_MESSAGE_DATA_HEADER *MessageHeader)
{	
	if (MessageHeader->adaptationLength > 0) {		
		gf_free(MessageHeader->DsmccAdaptationHeader->adaptationDataByte);
		gf_free(MessageHeader->DsmccAdaptationHeader);
	}
	return GF_OK;
}

static GF_Err gf_m2ts_dsmcc_section_delete(GF_M2TS_DSMCC_SECTION *dsmcc)
{
	GF_M2TS_DSMCC_DOWNLOAD_DATA_MESSAGE* DataMessage = (GF_M2TS_DSMCC_DOWNLOAD_DATA_MESSAGE*)dsmcc->DSMCC_Extension;	

	switch (DataMessage->DownloadDataHeader.messageId)
	{
		case DOWNLOAD_INFO_REPONSE_INDICATION:
		{
			u32 i;
			GF_M2TS_DSMCC_DOWNLOAD_INFO_RESP_INDIC* DownloadInfoIndication = (GF_M2TS_DSMCC_DOWNLOAD_INFO_RESP_INDIC*)DataMessage->dataMessagePayload;

			/* Compatibility Descr */
			gf_m2ts_dsmcc_delete_compatibility_descriptor(&DownloadInfoIndication->CompatibilityDescr);
			if (DownloadInfoIndication->numberOfModules) {
				for( i=0; i<DownloadInfoIndication->numberOfModules;i++) {
					gf_free(DownloadInfoIndication->Modules[i].moduleInfoByte);
				}
				gf_free(DownloadInfoIndication->Modules);
			}
			if (DownloadInfoIndication->privateDataLength) {
				gf_free(DownloadInfoIndication->privateDataByte);
			}
			gf_free(DownloadInfoIndication);
			break;
		}
		case DOWNLOAD_DATA_BLOCK:
		{
			GF_M2TS_DSMCC_DOWNLOAD_DATA_BLOCK* DownloadDataBlock = (GF_M2TS_DSMCC_DOWNLOAD_DATA_BLOCK*)DataMessage->dataMessagePayload;
			if (DownloadDataBlock->dataBlocksize) {
				gf_free(DownloadDataBlock->blockDataByte);
			}
			gf_free(DownloadDataBlock);
			break;
		}
		case DOWNLOAD_DATA_REQUEST:
		{
			GF_M2TS_DSMCC_DOWNLOAD_DATA_REQUEST_MESSAGE* DownloadDataRequest = (GF_M2TS_DSMCC_DOWNLOAD_DATA_REQUEST_MESSAGE*)DataMessage->dataMessagePayload;
			gf_free(DownloadDataRequest);
			break;
		}
		case DOWNLOAD_DATA_CANCEL:
		{
			GF_M2TS_DSMCC_DOWNLOAD_CANCEL* DownloadCancel = (GF_M2TS_DSMCC_DOWNLOAD_CANCEL*)DataMessage->dataMessagePayload;			
			if (DownloadCancel->privateDataLength) {
				gf_free(DownloadCancel->privateDataByte);
			}
			gf_free(DownloadCancel);
			break;
		}
		case DOWNLOAD_SERVER_INITIATE:
		{
			GF_M2TS_DSMCC_DOWNLOAD_SERVER_INIT* DownloadServerInit = (GF_M2TS_DSMCC_DOWNLOAD_SERVER_INIT*)DataMessage->dataMessagePayload;
			gf_m2ts_dsmcc_delete_compatibility_descriptor(&DownloadServerInit->CompatibilityDescr);
			if (DownloadServerInit->privateDataLength) {
				gf_free(DownloadServerInit->privateDataByte);
			}
			gf_free(DownloadServerInit);
			break;
		}
		default:
		{
			GF_LOG(GF_LOG_INFO, GF_LOG_CONTAINER, ("[Process DSMCC] Unknown dataMessagePayload Type \n"));
			break;
		}
	} 

	/* Header */
	gf_m2ts_dsmcc_delete_message_header(&DataMessage->DownloadDataHeader);
	gf_free(DataMessage);
	gf_free(dsmcc);
	dsmcc = NULL;
	return GF_OK;
}


static GF_Err dsmcc_module_delete(GF_M2TS_DSMCC_MODULE* dsmcc_module){

	gf_free(dsmcc_module->buffer);
	gf_free(dsmcc_module);
	dsmcc_module = NULL;
	return  GF_OK;

}

#endif //GPAC_DISABLE_MPEG2TS


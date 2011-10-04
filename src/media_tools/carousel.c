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


GF_M2TS_ES *gf_ait_section_new(u32 service_id)
{
	GF_M2TS_ES *es;

	GF_M2TS_AIT_CARRY *ses;
	GF_SAFEALLOC(ses, GF_M2TS_AIT_CARRY);
	GF_SAFEALLOC(ses->ait, GF_M2TS_AIT);
	es = (GF_M2TS_ES *)ses;
	es->flags = GF_M2TS_ES_IS_SECTION;
	ses->ait->service_id = service_id;
	return es;
}


void on_ait_section(GF_M2TS_Demuxer *ts, u32 evt_type, void *par) 
{
	GF_M2TS_SL_PCK *pck = (GF_M2TS_SL_PCK *)par;
	char *data;
	u32 u32_data_size;
	u32 u32_table_id;

	if (evt_type == GF_M2TS_EVT_AIT_FOUND) {
		data = pck->data;
		u32_data_size = pck->data_len;
		u32_table_id = data[0];
		GF_M2TS_AIT_CARRY* ait_carry = (GF_M2TS_AIT_CARRY*)pck->stream;
		ait_carry->ait->pid = ait_carry->pid;
		gf_m2ts_process_ait(ait_carry->ait, data, u32_data_size, u32_table_id);

	}
}

GF_Err gf_m2ts_process_ait(GF_M2TS_AIT *ait, char  *data, u32 data_size, u32 table_id)
{

	GF_BitStream *bs;
	u8 temp_descriptor_tag;
	u32 data_shift, app_desc_data_shift, ait_app_data_shift;
	u32 nb_of_ait;
	u32 nb_of_protocol;

	data_shift = 0;
	temp_descriptor_tag = 0;
	nb_of_ait = 0;
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

	while(ait_app_data_shift<ait->application_loop_length){

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

		while(app_desc_data_shift< application->application_descriptors_loop_length){
			temp_descriptor_tag = gf_bs_read_int(bs,8);
			switch(temp_descriptor_tag){
			case APPLICATION_DESCRIPTOR:
				{
 					u64 pre_processing_pos;
 					u8 i;
 					GF_M2TS_APPLICATION_DESCRIPTOR* application_descriptor;
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
					if(nb_of_protocol > 0){
						for(i=0;i<nb_of_protocol;i++){						
							application_descriptor->transport_protocol_label[i] = gf_bs_read_int(bs,8);
						}
					}else{
						application_descriptor->transport_protocol_label[0] = gf_bs_read_int(bs,8);
					}
					if(pre_processing_pos+application_descriptor->descriptor_length != gf_bs_get_position(bs) || application_descriptor->application_profile == 2 /* PVR feature */){
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
 					if(pre_processing_pos+name_descriptor->descriptor_length != gf_bs_get_position(bs)){
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
					switch(protocol_descriptor->protocol_id){
						case CAROUSEL:
							{
								GF_M2TS_OBJECT_CAROUSEL_SELECTOR_BYTE* Carousel_selector_byte;
								GF_SAFEALLOC(Carousel_selector_byte, GF_M2TS_OBJECT_CAROUSEL_SELECTOR_BYTE);
								Carousel_selector_byte->remote_connection = gf_bs_read_int(bs,1);
								gf_bs_read_int(bs,7); /* bit shifting */
								if(Carousel_selector_byte->remote_connection){
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
								Transport_http_selector_byte->URL_base_byte = (char*)gf_calloc(Transport_http_selector_byte->URL_base_length,sizeof(char));
								gf_bs_read_data(bs,Transport_http_selector_byte->URL_base_byte ,(u32)(Transport_http_selector_byte->URL_base_length));
								Transport_http_selector_byte->URL_base_byte[Transport_http_selector_byte->URL_base_length] = 0;
								Transport_http_selector_byte->URL_extension_count = gf_bs_read_int(bs,8);
								if(Transport_http_selector_byte->URL_extension_count){
									Transport_http_selector_byte->URL_extentions = (GF_M2TS_TRANSPORT_HTTP_URL_EXTENTION*) gf_calloc(Transport_http_selector_byte->URL_extension_count,sizeof(GF_M2TS_TRANSPORT_HTTP_URL_EXTENTION));
									for(i = 0; i < Transport_http_selector_byte->URL_extension_count;i++){
										Transport_http_selector_byte->URL_extentions[i].URL_extension_length = gf_bs_read_int(bs,8);
										Transport_http_selector_byte->URL_extentions[i].URL_extension_byte = (char*)gf_calloc(Transport_http_selector_byte->URL_extentions[i].URL_extension_length,sizeof(char));
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
					if(pre_processing_pos+protocol_descriptor->descriptor_length != gf_bs_get_position(bs)){
						GF_LOG(GF_LOG_INFO, GF_LOG_CONTAINER, ("[Process AIT] Descriptor data processed length error. Difference between byte shifting %d and descriptor length %d \n",(gf_bs_get_position(bs) -  pre_processing_pos),protocol_descriptor->descriptor_length));
						if(protocol_descriptor->protocol_id == CAROUSEL){						
							GF_M2TS_OBJECT_CAROUSEL_SELECTOR_BYTE* Carousel_selector_byte = (GF_M2TS_OBJECT_CAROUSEL_SELECTOR_BYTE*) protocol_descriptor->selector_byte;								
							gf_free(Carousel_selector_byte);								
						}else if(protocol_descriptor->protocol_id ==TRANSPORT_HTTP){
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
					Simple_application_location->initial_path_bytes = (char*)gf_calloc(Simple_application_location->descriptor_length,sizeof(char));
					gf_bs_read_data(bs,Simple_application_location->initial_path_bytes ,(u32)(Simple_application_location->descriptor_length));
					Simple_application_location->initial_path_bytes[Simple_application_location->descriptor_length] = 0;
					if(pre_processing_pos+Simple_application_location->descriptor_length != gf_bs_get_position(bs)){						
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
					if(pre_processing_pos+Application_usage->descriptor_length != gf_bs_get_position(bs)){
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
					if(boundary_descriptor->boundary_extension_count > 0){
						boundary_descriptor->boundary_extension_info = (GF_M2TS_APPLICATION_BOUNDARY_EXTENSION_INFO*)gf_calloc(boundary_descriptor->boundary_extension_count,sizeof(GF_M2TS_APPLICATION_BOUNDARY_EXTENSION_INFO));
						for(i=0;i<boundary_descriptor->boundary_extension_count;i++){
							boundary_descriptor->boundary_extension_info[i].boundary_extension_length = gf_bs_read_int(bs,8);
							if(boundary_descriptor->boundary_extension_info[i].boundary_extension_length > 0){
								boundary_descriptor->boundary_extension_info[i].boundary_extension_byte = (char*)gf_calloc(boundary_descriptor->boundary_extension_info[i].boundary_extension_length,sizeof(char));
								gf_bs_read_data(bs,boundary_descriptor->boundary_extension_info[i].boundary_extension_byte ,(u8)(boundary_descriptor->boundary_extension_info[i].boundary_extension_length));
								boundary_descriptor->boundary_extension_info[i].boundary_extension_byte[boundary_descriptor->boundary_extension_info[i].boundary_extension_length] = 0;
							}
						}
					}
					if(pre_processing_pos+boundary_descriptor->descriptor_length != gf_bs_get_position(bs)){
						GF_LOG(GF_LOG_INFO, GF_LOG_CONTAINER, ("[Process AIT] Descriptor data processed length error. Difference between byte shifting %d and descriptor length %d \n",(gf_bs_get_position(bs) -  pre_processing_pos),boundary_descriptor->descriptor_length));
						if(boundary_descriptor->boundary_extension_count > 0){
							u32 i;
							for(i=0;i<boundary_descriptor->boundary_extension_count;i++){							
								if(boundary_descriptor->boundary_extension_info[i].boundary_extension_length > 0){
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


	if(data_shift != data_size){
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
	//~ gf_free(ait);
	
	
}

void gf_ait_application_destroy(GF_M2TS_AIT_APPLICATION* application)
{
		
	  u32 app_desc_num,i,app_descr_index;
 	  
 	  app_desc_num = app_descr_index = i = 0; 
 	  app_desc_num = gf_list_count(application->application_descriptors);
 	  
 	  while(app_desc_num != 0){
 	    u32 descr_tag;
 	    descr_tag = application->application_descriptors_id[i];
 	    printf("descr_tag %d\n",descr_tag);
 	    switch(descr_tag){
 			case APPLICATION_DESCRIPTOR:
 				{
 					GF_M2TS_APPLICATION_DESCRIPTOR* application_descriptor = (GF_M2TS_APPLICATION_DESCRIPTOR*)gf_list_get(application->application_descriptors,0);
  					gf_free(application_descriptor);  
  					break;
 				}
 			case APPLICATION_NAME_DESCRIPTOR:
 				{
  					u64 pre_processing_pos;
  					GF_M2TS_APPLICATION_NAME_DESCRIPTOR* name_descriptor = (GF_M2TS_APPLICATION_NAME_DESCRIPTOR*)gf_list_get(application->application_descriptors,0);
  					gf_free(name_descriptor->application_name_char);
 					gf_free(name_descriptor);
  					break;
 				}
 			case TRANSPORT_PROTOCOL_DESCRIPTOR:
 				{
 					u64 pre_processing_pos;
 					GF_M2TS_TRANSPORT_PROTOCOL_DESCRIPTOR* protocol_descriptor = (GF_M2TS_TRANSPORT_PROTOCOL_DESCRIPTOR*)gf_list_get(application->application_descriptors,0);
  					
 
 					switch(protocol_descriptor->protocol_id){
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
 								if(Transport_http_selector_byte->URL_extension_count){
 									for(i = 0; i < Transport_http_selector_byte->URL_extension_count;i++){ 										
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
				
					u32 i,j;
					GF_M2TS_APPLICATION_BOUNDARY_DESCRIPTOR* boundary_descriptor = (GF_M2TS_APPLICATION_BOUNDARY_DESCRIPTOR*)gf_list_get(application->application_descriptors,0);
  									
					for(i = 0;i<boundary_descriptor->boundary_extension_count;i++);
					if(boundary_descriptor->boundary_extension_count > 0){
						for(i=0;i<boundary_descriptor->boundary_extension_count;i++){						
							if(boundary_descriptor->boundary_extension_info[i].boundary_extension_length > 0){
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

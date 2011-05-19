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


GF_M2TS_ES *gf_ait_section_new()
{
	GF_M2TS_ES *es;

	GF_M2TS_AIT *ses;
	GF_SAFEALLOC(ses, GF_M2TS_AIT);
	es = (GF_M2TS_ES *)ses;
	es->flags = GF_M2TS_ES_IS_SECTION;
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
			
		gf_m2ts_process_ait(ts, (GF_M2TS_AIT*)pck->stream, data, u32_data_size, u32_table_id);
		
	}
}

GF_Err gf_m2ts_process_ait(GF_M2TS_Demuxer *ts, GF_M2TS_AIT *ait, char  *data, u32 data_size, u32 table_id)
{
	
	GF_BitStream *bs;
	u8 temp_descriptor_tag;
	u32 data_shift, app_desc_data_shift;
	u32 nb_of_ait;

	data_shift = 0;
	temp_descriptor_tag = 0;
	nb_of_ait = 0;
	bs = gf_bs_new(data,data_size,GF_BITSTREAM_READ);
	
	ait->common_descriptors = gf_list_new();
	ait->application_descriptors = gf_list_new();

	
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
	ait->application_type = gf_bs_read_int(bs,15);	
	if(ait->application_type != APPLICATION_TYPE_HTTP_APPLICATION){
		GF_LOG(GF_LOG_INFO, GF_LOG_CONTAINER, ("[Process AIT] application type should 0x10. Wrong section, abording processing \n"));
	}
	gf_bs_read_int(bs,2);
	ait->version_number = gf_bs_read_int(bs,5);
	
	ait->current_next_indicator = gf_bs_read_int(bs,1);	
	if(!ait->current_next_indicator){
		GF_LOG(GF_LOG_INFO, GF_LOG_CONTAINER, ("[Process AIT] current next indicator should be at 1 \n"));
	}
	ait->section_number = gf_bs_read_int(bs,8);
	ait->last_section_number = gf_bs_read_int(bs,8);	
	gf_bs_read_int(bs,4);/* bit shifting */
	ait->common_descriptors_length = gf_bs_read_int(bs,12);	
	gf_bs_read_int(bs,(unsigned int)ait->common_descriptors_length/8);
	gf_bs_read_int(bs,4);/* bit shifting */	
	ait->application_loop_length = gf_bs_read_int(bs,12);
	
	/* application loop */
	ait->organisation_id = gf_bs_read_int(bs,32);	
	ait->application_id= gf_bs_read_int(bs,16);
	ait->application_control_code= gf_bs_read_int(bs,8);
	gf_bs_read_int(bs,4);/* bit shifting */
	ait->application_descriptors_loop_length= gf_bs_read_int(bs,12);

	data_shift = (u32)(gf_bs_get_position(bs)) + ait->common_descriptors_length/8;

	app_desc_data_shift = 0;

	while(app_desc_data_shift< ait->application_descriptors_loop_length){
		temp_descriptor_tag = gf_bs_read_int(bs,8);
		switch(temp_descriptor_tag){
			case APPLICATION_DESCRIPTOR:
				{
					u64 pre_processing_pos;
					GF_M2TS_APPLICATION_DESCRIPTOR* application_descriptor;
					GF_SAFEALLOC(application_descriptor, GF_M2TS_APPLICATION_DESCRIPTOR);
					application_descriptor->descriptor_tag = temp_descriptor_tag;
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
					application_descriptor->transport_protocol_label = gf_bs_read_int(bs,8);
					if(pre_processing_pos+application_descriptor->descriptor_length != gf_bs_get_position(bs)){
						GF_LOG(GF_LOG_INFO, GF_LOG_CONTAINER, ("[Process AIT] Descriptor data processed length error. Difference between byte shifting %d and descriptor length %d \n",(gf_bs_get_position(bs) -  pre_processing_pos),application_descriptor->descriptor_length));
					}
					gf_list_add(ait->application_descriptors,application_descriptor);
					app_desc_data_shift += (2+ application_descriptor->descriptor_length);
					break;
				}
			case APPLICATION_NAME_DESCRIPTOR:
				{
					u64 pre_processing_pos;
					GF_M2TS_APPLICATION_NAME_DESCRIPTOR* name_descriptor;
					GF_SAFEALLOC(name_descriptor, GF_M2TS_APPLICATION_NAME_DESCRIPTOR);
					name_descriptor->descriptor_tag = temp_descriptor_tag;
					name_descriptor->descriptor_length = gf_bs_read_int(bs,8);
					pre_processing_pos = gf_bs_get_position(bs);
					name_descriptor->ISO_639_language_code = gf_bs_read_int(bs,24);
					name_descriptor->application_name_length = gf_bs_read_int(bs,8);
					name_descriptor->application_name_char = (char*) gf_calloc(name_descriptor->application_name_length,sizeof(char));
					gf_bs_read_data(bs,name_descriptor->application_name_char,name_descriptor->application_name_length);
					name_descriptor->application_name_char[name_descriptor->application_name_length] = 0 ;
					if(pre_processing_pos+name_descriptor->descriptor_length != gf_bs_get_position(bs)){
						GF_LOG(GF_LOG_INFO, GF_LOG_CONTAINER, ("[Process AIT] Descriptor data processed length error. Difference between byte shifting %d and descriptor length %d \n",(gf_bs_get_position(bs) -  pre_processing_pos),name_descriptor->descriptor_length));
					}
					gf_list_add(ait->application_descriptors,name_descriptor);
					app_desc_data_shift += (2+ name_descriptor->descriptor_length);
					break;
				}
			case TRANSPORT_PROTOCOL_DESCRIPTOR:
				{
					u64 pre_processing_pos;
					GF_M2TS_TRANSPORT_PROTOCOL_DESCRIPTOR* protocol_descriptor;
					GF_SAFEALLOC(protocol_descriptor, GF_M2TS_TRANSPORT_PROTOCOL_DESCRIPTOR);
					protocol_descriptor->descriptor_tag = temp_descriptor_tag;
					protocol_descriptor->descriptor_length = gf_bs_read_int(bs,8);
					pre_processing_pos = gf_bs_get_position(bs);
					protocol_descriptor->protocol_id = gf_bs_read_int(bs,16);
					protocol_descriptor->transport_protocol_label = gf_bs_read_int(bs,8);
					printf("protocol_descriptor->protocol_id %d \n",protocol_descriptor->protocol_id);
								
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
					}
					gf_list_add(ait->application_descriptors,protocol_descriptor);
					app_desc_data_shift += (2+ protocol_descriptor->descriptor_length);
					break;
				}
			case SIMPLE_APPLICATION_LOCATION_DESCRIPTOR:
					{
					u64 pre_processing_pos;
					GF_M2TS_SIMPLE_APPLICATION_LOCATION* Simple_application_location;
					GF_SAFEALLOC(Simple_application_location, GF_M2TS_SIMPLE_APPLICATION_LOCATION);
					Simple_application_location->descriptor_tag = temp_descriptor_tag;
					Simple_application_location->descriptor_length = gf_bs_read_int(bs,8);
					pre_processing_pos = gf_bs_get_position(bs);
					Simple_application_location->initial_path_bytes = (char*)gf_calloc(Simple_application_location->descriptor_length,sizeof(char));
					gf_bs_read_data(bs,Simple_application_location->initial_path_bytes ,(u32)(Simple_application_location->descriptor_length));
					Simple_application_location->initial_path_bytes[Simple_application_location->descriptor_length] = 0;
					if(pre_processing_pos+Simple_application_location->descriptor_length != gf_bs_get_position(bs)){
						GF_LOG(GF_LOG_INFO, GF_LOG_CONTAINER, ("[Process AIT] Descriptor data processed length error. Difference between byte shifting %d and descriptor length %d \n",(gf_bs_get_position(bs) -  pre_processing_pos),Simple_application_location->descriptor_length));
					}
					gf_list_add(ait->application_descriptors,Simple_application_location);
					app_desc_data_shift += (2+ Simple_application_location->descriptor_length);
					break;
				}
			case APPLICATION_USAGE_DESCRIPTOR:
				{
					u64 pre_processing_pos;
					GF_M2TS_APPLICATION_USAGE* Application_usage;
					GF_SAFEALLOC(Application_usage, GF_M2TS_APPLICATION_USAGE);
					Application_usage->descriptor_tag = temp_descriptor_tag;
					Application_usage->descriptor_length = gf_bs_read_int(bs,8);
					pre_processing_pos = gf_bs_get_position(bs);
					Application_usage->usage_type = gf_bs_read_int(bs,8);
					if(pre_processing_pos+Application_usage->descriptor_length != gf_bs_get_position(bs)){
						GF_LOG(GF_LOG_INFO, GF_LOG_CONTAINER, ("[Process AIT] Descriptor data processed length error. Difference between byte shifting %d and descriptor length %d \n",(gf_bs_get_position(bs) -  pre_processing_pos),Application_usage->descriptor_length));
					}
					gf_list_add(ait->application_descriptors,Application_usage);
					app_desc_data_shift += (2+ Application_usage->descriptor_length);
					break;
				}
				

		default:
			{
				GF_LOG(GF_LOG_INFO, GF_LOG_CONTAINER, ("[Process AIT] Descriptor tag %d unknown, ignoring the descriptor \n",temp_descriptor_tag));
			}

		}

	}

	data_shift +=ait->application_descriptors_loop_length;
	ait->CRC_32 = gf_bs_read_int(bs,32);
	data_shift += 4;

	if(data_shift != data_size){
		GF_LOG(GF_LOG_INFO, GF_LOG_CONTAINER, ("[Process AIT] AIT processed length error. Difference between byte shifting %d and data size %d \n",data_shift,data_size));
	}

	return GF_OK;
}

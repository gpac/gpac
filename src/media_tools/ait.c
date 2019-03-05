/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jonathan Sillan
 *			Copyright (c) Telecom ParisTech 2011-2012
 *					All rights reserved
 *
 *  This file is part of GPAC / media tools sub-project
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

#include <gpac/ait.h>

#ifdef GPAC_ENABLE_DSMCC

/* static functions */
static Bool check_ait_already_received(GF_List* ChannelAppList,u32 pid,char* data);
static void gf_ait_application_decode_destroy(GF_M2TS_AIT_APPLICATION_DECODE* application_decode);
static GF_Err gf_m2ts_decode_ait(GF_M2TS_AIT *ait, char  *data, u32 data_size, u32 table_id);
static Bool gf_m2ts_is_dmscc_app(GF_M2TS_CHANNEL_APPLICATION_INFO* ChanAppInfo);
static void gf_m2ts_free_ait_application(GF_M2TS_AIT_APPLICATION* application);
static void gf_ait_destroy(GF_M2TS_AIT* ait);
static void gf_m2ts_process_ait(GF_M2TS_Demuxer *ts, GF_M2TS_AIT* ait);

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
		data = pck->data;

		if(!check_ait_already_received(ts->ChannelAppList,ait_carry->pid,data)) {
			GF_SAFEALLOC(ait, GF_M2TS_AIT);
			u32_data_size = pck->data_len;
			u32_table_id = data[0];
			ait->pid = ait_carry->pid;
			ait->service_id = ait_carry->service_id;
			gf_m2ts_decode_ait(ait, data, u32_data_size, u32_table_id);
			gf_m2ts_process_ait(ts,ait);
			gf_ait_destroy(ait);
			if (ts->on_event) ts->on_event(ts, GF_M2TS_EVT_AIT_FOUND, pck->stream);
		}

	}
}

static GF_Err gf_m2ts_decode_ait(GF_M2TS_AIT *ait, char  *data, u32 data_size, u32 table_id)
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
	if(ait->common_descriptors == NULL) {
		GF_LOG(GF_LOG_INFO, GF_LOG_CONTAINER, ("[Process AIT] Error during common_descriptors list initialization, abording processing \n"));
		return GF_CORRUPTED_DATA;
	}
	ait->application_decoded = gf_list_new();
	if(ait->application_decoded == NULL) {
		GF_LOG(GF_LOG_INFO, GF_LOG_CONTAINER, ("[Process AIT] Error during application list initialization, abording processing \n"));
		return GF_CORRUPTED_DATA;
	}

	ait->table_id = gf_bs_read_int(bs,8);
	ait->section_syntax_indicator = gf_bs_read_int(bs,1);
	gf_bs_read_int(bs,3);
	ait->section_length = gf_bs_read_int(bs,12);
	if( (data[1] & 0x0C) != 0) {
		GF_LOG(GF_LOG_INFO, GF_LOG_CONTAINER, ("[Process AIT] section length is not correct abroding \n"));
	} else if( ait->section_length > AIT_SECTION_LENGTH_MAX) {
		GF_LOG(GF_LOG_INFO, GF_LOG_CONTAINER, ("[Process AIT] section length should not exceed 1021. Wrong section, abording processing \n"));
	}
	ait->test_application_flag = gf_bs_read_int(bs,1);
	if(ait->test_application_flag == 1) {
		GF_LOG(GF_LOG_INFO, GF_LOG_CONTAINER, ("[Process AIT] test application flag is at 1. API transmitted for testing, abording processing \n"));
		return GF_CORRUPTED_DATA;
	}
	ait->application_type = gf_bs_read_int(bs,15);
	if(ait->application_type != APPLICATION_TYPE_HTTP_APPLICATION) {
		GF_LOG(GF_LOG_INFO, GF_LOG_CONTAINER, ("[Process AIT] application type should 0x10. Wrong section, abording processing \n"));
		return GF_CORRUPTED_DATA;
	}
	gf_bs_read_int(bs,2);
	ait->version_number = gf_bs_read_int(bs,5);

	ait->current_next_indicator = gf_bs_read_int(bs,1);
	if(!ait->current_next_indicator) {
		GF_LOG(GF_LOG_INFO, GF_LOG_CONTAINER, ("[Process AIT] current next indicator should be at 1 \n"));
		return GF_CORRUPTED_DATA;
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

		GF_M2TS_AIT_APPLICATION_DECODE* application;
		GF_SAFEALLOC(application,GF_M2TS_AIT_APPLICATION_DECODE);
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
					return GF_CORRUPTED_DATA;
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
					return GF_CORRUPTED_DATA;
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
					//fprintf(stderr, "Transport_http_selector_byte->URL_base_length %d \n",Transport_http_selector_byte->URL_base_length);
					Transport_http_selector_byte->URL_base_byte = (char*)gf_calloc(Transport_http_selector_byte->URL_base_length+1,sizeof(char));
					gf_bs_read_data(bs,Transport_http_selector_byte->URL_base_byte ,(u32)(Transport_http_selector_byte->URL_base_length));
					Transport_http_selector_byte->URL_base_byte[Transport_http_selector_byte->URL_base_length] = 0;
					Transport_http_selector_byte->URL_extension_count = gf_bs_read_int(bs,8);
					if (Transport_http_selector_byte->URL_extension_count) {
						Transport_http_selector_byte->URL_extentions = (GF_M2TS_TRANSPORT_HTTP_URL_EXTENTION*) gf_calloc(Transport_http_selector_byte->URL_extension_count,sizeof(GF_M2TS_TRANSPORT_HTTP_URL_EXTENTION));
						for (i=0; i < Transport_http_selector_byte->URL_extension_count; i++) {
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
						for(i = 0; i < Transport_http_selector_byte->URL_extension_count; i++) {
							gf_free(Transport_http_selector_byte->URL_extentions[i].URL_extension_byte);
						}
						gf_free(Transport_http_selector_byte);
					}
					gf_free(protocol_descriptor);
					return GF_CORRUPTED_DATA;
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
					return GF_CORRUPTED_DATA;
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
					return GF_CORRUPTED_DATA;
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
					for (i=0; i<boundary_descriptor->boundary_extension_count; i++) {
						boundary_descriptor->boundary_extension_info[i].boundary_extension_length = gf_bs_read_int(bs,8);
						if (boundary_descriptor->boundary_extension_info[i].boundary_extension_length > 0) {
							boundary_descriptor->boundary_extension_info[i].boundary_extension_byte = (char*)gf_calloc(boundary_descriptor->boundary_extension_info[i].boundary_extension_length+1,sizeof(char));
							gf_bs_read_data(bs,boundary_descriptor->boundary_extension_info[i].boundary_extension_byte ,(u32)(boundary_descriptor->boundary_extension_info[i].boundary_extension_length));
							boundary_descriptor->boundary_extension_info[i].boundary_extension_byte[boundary_descriptor->boundary_extension_info[i].boundary_extension_length] = 0;
						}
					}
				}
				if (pre_processing_pos+boundary_descriptor->descriptor_length != gf_bs_get_position(bs)) {
					GF_LOG(GF_LOG_INFO, GF_LOG_CONTAINER, ("[Process AIT] Descriptor data processed length error. Difference between byte shifting %d and descriptor length %d \n",(gf_bs_get_position(bs) -  pre_processing_pos),boundary_descriptor->descriptor_length));
					if (boundary_descriptor->boundary_extension_count > 0) {
						u32 i;
						for (i=0; i<boundary_descriptor->boundary_extension_count; i++) {
							if (boundary_descriptor->boundary_extension_info[i].boundary_extension_length > 0) {
								gf_free(boundary_descriptor->boundary_extension_info[i].boundary_extension_byte);
							}
						}
						gf_free(boundary_descriptor->boundary_extension_info);
					}
					gf_free(boundary_descriptor);
					return GF_CORRUPTED_DATA;
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
		gf_list_add(ait->application_decoded,application);
	}

	data_shift +=ait->application_loop_length;
	ait->CRC_32 = gf_bs_read_int(bs,32);
	data_shift += 4;


	if (data_shift != data_size) {
		GF_LOG(GF_LOG_INFO, GF_LOG_CONTAINER, ("[Process AIT] AIT processed length error. Difference between byte shifting %d and data size %d \n",data_shift,data_size));
		return GF_CORRUPTED_DATA;
	}

	return GF_OK;
}

static void gf_m2ts_process_ait(GF_M2TS_Demuxer *ts, GF_M2TS_AIT* ait) {

	u32 nb_app_desc, k, desc_id, nb_of_app, j;
	GF_M2TS_CHANNEL_APPLICATION_INFO* ChanAppInfo;
	char *url_base, *url_appli_path;

	nb_of_app = gf_list_count(ait->application_decoded);
	url_base = NULL;
	url_appli_path = NULL;
	j=0;

	/* Link the AIT and the channel */
	ChanAppInfo = gf_m2ts_get_channel_application_info(ts->ChannelAppList,ait->service_id);

	if(!ChanAppInfo) {
		GF_SAFEALLOC(ChanAppInfo,GF_M2TS_CHANNEL_APPLICATION_INFO);
		ChanAppInfo->service_id = ait->service_id;
		ChanAppInfo->Application = gf_list_new();
		ChanAppInfo->ait_pid = ait->pid;
		gf_list_add(ts->ChannelAppList,ChanAppInfo);
	}

	ChanAppInfo->nb_application = nb_of_app;

	GF_LOG(GF_LOG_DEBUG, GF_LOG_CONTAINER, ("[DSMCC] ChanAppInfo->nb_application %d \n",ChanAppInfo->nb_application));

	for(j=0 ; j<nb_of_app ; j++) {
		GF_M2TS_AIT_APPLICATION* Application;
		GF_M2TS_AIT_APPLICATION_DECODE* application_decoded;
		GF_SAFEALLOC(Application,GF_M2TS_AIT_APPLICATION);
		gf_list_add(ChanAppInfo->Application,Application);
		Application->http_url = NULL;
		Application->carousel_url = NULL;

		application_decoded = (GF_M2TS_AIT_APPLICATION_DECODE*)gf_list_get(ait->application_decoded,j);
		nb_app_desc = gf_list_count(application_decoded->application_descriptors);
		Application->application_control_code = application_decoded->application_control_code;
		Application->application_id = application_decoded->application_id;
		Application->broadband = 0;
		Application->broadcast = 0;
		ChanAppInfo->version_number = ait->version_number;

		k=0;
		for(k=0 ; k<nb_app_desc ; k++) {
			desc_id = application_decoded->application_descriptors_id[k];
			switch(desc_id) {
			case APPLICATION_DESCRIPTOR:
			{
				GF_M2TS_APPLICATION_DESCRIPTOR* application_descriptor = (GF_M2TS_APPLICATION_DESCRIPTOR*)gf_list_get(application_decoded->application_descriptors,k);
				Application->priority = application_descriptor->application_priority;
				Application->application_profile = application_descriptor->application_profile;

				break;
			}
			case APPLICATION_NAME_DESCRIPTOR:
			{
				GF_M2TS_APPLICATION_NAME_DESCRIPTOR* name_descriptor = (GF_M2TS_APPLICATION_NAME_DESCRIPTOR*)gf_list_get(application_decoded->application_descriptors,k);
				Application->appli_name = gf_strdup(name_descriptor->application_name_char);
				break;
			}
			case TRANSPORT_PROTOCOL_DESCRIPTOR:
			{
				GF_M2TS_TRANSPORT_PROTOCOL_DESCRIPTOR* protocol_descriptor = (GF_M2TS_TRANSPORT_PROTOCOL_DESCRIPTOR*)gf_list_get(application_decoded->application_descriptors,k);

				switch(protocol_descriptor->protocol_id) {
				case CAROUSEL:
				{
					GF_Err e;
					GF_M2TS_OBJECT_CAROUSEL_SELECTOR_BYTE* Carousel_selector_byte = (GF_M2TS_OBJECT_CAROUSEL_SELECTOR_BYTE*)protocol_descriptor->selector_byte;
					if(ts->process_dmscc) {
						GF_M2TS_DSMCC_OVERLORD* dsmcc_overlord = gf_m2ts_get_dmscc_overlord(ts->dsmcc_controler,ait->service_id);
						Application->broadcast = 1;
						if(!dsmcc_overlord) {
							char app_url[1024], char_service_id[128];

							memset(&app_url,0,1024*sizeof(char));
							memset(&char_service_id,0,128*sizeof(char));

							dsmcc_overlord = gf_m2ts_init_dsmcc_overlord(ait->service_id);
							dsmcc_overlord->application_id = Application->application_id;
							sprintf(char_service_id,"%d",dsmcc_overlord->service_id);
							dsmcc_overlord->root_dir = (char*)gf_calloc(strlen(ts->dsmcc_root_dir)+2+strlen(char_service_id),sizeof(char));
							sprintf(dsmcc_overlord->root_dir,"%s%c%s",ts->dsmcc_root_dir,GF_PATH_SEPARATOR,char_service_id);
							e = gf_mkdir(dsmcc_overlord->root_dir);
							if(e) {
								GF_LOG(GF_LOG_INFO, GF_LOG_CONTAINER, ("[Process DSMCC] Error during the creation of the directory %s \n",dsmcc_overlord->root_dir));
								if(e == GF_BAD_PARAM) {
									gf_cleanup_dir(dsmcc_overlord->root_dir);
								}
							}
							sprintf(app_url,"%s%cindex.html",dsmcc_overlord->root_dir,GF_PATH_SEPARATOR);
							Application->carousel_url = gf_strdup(app_url);
							gf_list_add(ts->dsmcc_controler,dsmcc_overlord);
						}
					}
					if(Carousel_selector_byte->remote_connection) {
						Application->carousel_pid = Carousel_selector_byte->service_id;
					}
					Application->component_tag = Carousel_selector_byte->component_tag;

					break;
				}
				case TRANSPORT_HTTP:
				{
					GF_M2TS_TRANSPORT_HTTP_SELECTOR_BYTE* Transport_http_selector_byte = (GF_M2TS_TRANSPORT_HTTP_SELECTOR_BYTE*)protocol_descriptor->selector_byte;
					url_base = Transport_http_selector_byte->URL_base_byte;
					Application->broadband = 1;
				}
				default:
				{
				}
				}
				break;
			}
			case SIMPLE_APPLICATION_LOCATION_DESCRIPTOR:
			{
				GF_M2TS_SIMPLE_APPLICATION_LOCATION* Simple_application_location = (GF_M2TS_SIMPLE_APPLICATION_LOCATION*)gf_list_get(application_decoded->application_descriptors,k);
				url_appli_path = Simple_application_location->initial_path_bytes;
				break;
			}
			case APPLICATION_USAGE_DESCRIPTOR:
			{
				break;
			}
			default:
			{
				GF_LOG(GF_LOG_DEBUG, GF_LOG_MODULE, ("[Process AIT] Descriptor tag %d unknown, ignoring the descriptor \n",desc_id));
			}
			}

			if((url_base != NULL) && ( url_appli_path != NULL)) {
				u32 url_length = (strlen(url_base)+strlen(url_appli_path));
				Application->http_url = (char*)gf_calloc(url_length + 1,sizeof(char));
				sprintf(Application->http_url,"%s%s",url_base,url_appli_path);
				Application->http_url[url_length]=0;
				Application->url_received = 1;
				url_base = NULL;
				url_appli_path = NULL;
				GF_LOG(GF_LOG_INFO, GF_LOG_MODULE, ("[Process AIT] SERVICE ID %d  URL %s  \n",ChanAppInfo->service_id,Application->http_url));
			}

		}
	}
	if(ts->process_dmscc) {
		GF_M2TS_DSMCC_OVERLORD* dsmcc_overlord = gf_m2ts_get_dmscc_overlord(ts->dsmcc_controler,ait->service_id);

		if(dsmcc_overlord && !gf_m2ts_is_dmscc_app(ChanAppInfo)) {
			gf_cleanup_dir(dsmcc_overlord->root_dir);
			gf_rmdir(dsmcc_overlord->root_dir);
			gf_m2ts_delete_dsmcc_overlord(dsmcc_overlord);

		}
	}
}

static Bool gf_m2ts_is_dmscc_app(GF_M2TS_CHANNEL_APPLICATION_INFO* ChanAppInfo) {

	u32 nb_app,i;

	nb_app = gf_list_count(ChanAppInfo->Application);
	for(i=0; i<nb_app; i++) {
		GF_M2TS_AIT_APPLICATION* Application = (GF_M2TS_AIT_APPLICATION*)gf_list_get(ChanAppInfo->Application,i);
		if(Application->broadband) {
			return 1;
		}
	}
	return 0;
}

GF_EXPORT
GF_M2TS_CHANNEL_APPLICATION_INFO* gf_m2ts_get_channel_application_info(GF_List* ChannelAppList, u32 ait_service_id) {
	u32 i,nb_chanapp;

	nb_chanapp = gf_list_count(ChannelAppList);

	for(i = 0; i < nb_chanapp; i++) {
		GF_M2TS_CHANNEL_APPLICATION_INFO* ChanAppInfo = (GF_M2TS_CHANNEL_APPLICATION_INFO*)gf_list_get(ChannelAppList,i);
		if(ChanAppInfo->service_id == ait_service_id) {
			return ChanAppInfo;
		}
	}

	return NULL;

}

static Bool check_ait_already_received(GF_List* ChannelAppList,u32 pid,char* data)
{
	u32 nb_of_ait;
	u32 version_number;

	nb_of_ait = 0;

	nb_of_ait = gf_list_count(ChannelAppList);

	if(nb_of_ait) {
		u32 i;
		for(i = 0; i < nb_of_ait; i++) {
			GF_M2TS_CHANNEL_APPLICATION_INFO* ChanAppInfo = (GF_M2TS_CHANNEL_APPLICATION_INFO*) gf_list_get(ChannelAppList,i);
			if(ChanAppInfo->ait_pid == pid) {
				version_number = (data[5] &0x3E)>>1;
				GF_LOG(GF_LOG_DEBUG, GF_LOG_MODULE, ("[Process AIT] AIT ait_list_version number %d, ait->version_number %d \n",ChanAppInfo->version_number,version_number));
				if(ChanAppInfo->version_number >= version_number) {
					GF_LOG(GF_LOG_DEBUG, GF_LOG_MODULE, ("[Process AIT] AIT already received, abording processing \n"));
					return 1;
				} else {
					while(ChanAppInfo->nb_application != 0) {
						GF_M2TS_AIT_APPLICATION* Application = gf_list_get(ChanAppInfo->Application,0);
						gf_m2ts_free_ait_application(Application);
						gf_list_rem(ChanAppInfo->Application,0);
						ChanAppInfo->nb_application = gf_list_count(ChanAppInfo->Application);
					}
					GF_LOG(GF_LOG_DEBUG, GF_LOG_MODULE, ("[Process AIT] Discard old applications - New AIT received \n"));
					return 0;
				}
			}
		}
	} else {
		GF_LOG(GF_LOG_DEBUG, GF_LOG_MODULE, ("[Process AIT] No AIT received \n"));
	}

	return 0;
}

static void gf_ait_destroy(GF_M2TS_AIT* ait)
{
	u32 common_descr_numb, app_numb;

	/* delete de Elementary Stream part of the AIT structure */
	common_descr_numb = 0;
	app_numb = 0;

	/* delete the common descriptors */
	common_descr_numb = gf_list_count(ait->common_descriptors);
	while(common_descr_numb != 0) {
		//TODO
	};
	gf_list_del(ait->common_descriptors);

	/* delete the applications and their descriptors */
	app_numb = gf_list_count(ait->application_decoded);
	while(app_numb != 0) {
		GF_M2TS_AIT_APPLICATION_DECODE* application = gf_list_get(ait->application_decoded,0);
		gf_ait_application_decode_destroy(application);
		gf_list_rem(ait->application_decoded,0);
		app_numb = gf_list_count(ait->application_decoded);
	}
	gf_list_del(ait->application_decoded);
	gf_free(ait);
}

static void gf_ait_application_decode_destroy(GF_M2TS_AIT_APPLICATION_DECODE* application_decode)
{

	u32 app_desc_num,i,app_descr_index;

	app_desc_num = app_descr_index = i = 0;
	app_desc_num = gf_list_count(application_decode->application_descriptors);

	while (app_desc_num != 0) {
		u32 descr_tag;
		descr_tag = application_decode->application_descriptors_id[i];
		GF_LOG(GF_LOG_DEBUG, GF_LOG_CONTAINER, ("[DSMCC] descr_tag %d\n", descr_tag));
		switch(descr_tag) {
		case APPLICATION_DESCRIPTOR:
		{
			GF_M2TS_APPLICATION_DESCRIPTOR* application_descriptor = (GF_M2TS_APPLICATION_DESCRIPTOR*)gf_list_get(application_decode->application_descriptors,0);
			gf_free(application_descriptor);
			break;
		}
		case APPLICATION_NAME_DESCRIPTOR:
		{
			GF_M2TS_APPLICATION_NAME_DESCRIPTOR* name_descriptor = (GF_M2TS_APPLICATION_NAME_DESCRIPTOR*)gf_list_get(application_decode->application_descriptors,0);
			gf_free(name_descriptor->application_name_char);
			gf_free(name_descriptor);
			break;
		}
		case TRANSPORT_PROTOCOL_DESCRIPTOR:
		{
			GF_M2TS_TRANSPORT_PROTOCOL_DESCRIPTOR* protocol_descriptor = (GF_M2TS_TRANSPORT_PROTOCOL_DESCRIPTOR*)gf_list_get(application_decode->application_descriptors,0);
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
					for (i=0; i < Transport_http_selector_byte->URL_extension_count; i++) {
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
			GF_M2TS_SIMPLE_APPLICATION_LOCATION* Simple_application_location = (GF_M2TS_SIMPLE_APPLICATION_LOCATION*)gf_list_get(application_decode->application_descriptors,0);
			gf_free(Simple_application_location->initial_path_bytes);
			gf_free(Simple_application_location);
			break;
		}
		case APPLICATION_USAGE_DESCRIPTOR:
		{
			GF_M2TS_APPLICATION_USAGE* Application_usage = (GF_M2TS_APPLICATION_USAGE*)gf_list_get(application_decode->application_descriptors,0);
			gf_free(Application_usage);
			break;
		}
		case APPLICATION_BOUNDARY_DESCRIPTOR:
		{
			u32 i;
			GF_M2TS_APPLICATION_BOUNDARY_DESCRIPTOR* boundary_descriptor = (GF_M2TS_APPLICATION_BOUNDARY_DESCRIPTOR*)gf_list_get(application_decode->application_descriptors, 0);
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
		gf_list_rem(application_decode->application_descriptors,0);
		app_desc_num = gf_list_count(application_decode->application_descriptors);
		i++;
	}
	gf_list_del(application_decode->application_descriptors);
	gf_free(application_decode);
}

static void gf_m2ts_free_ait_application(GF_M2TS_AIT_APPLICATION* Application) {

	if(Application->http_url) {
		gf_free(Application->http_url);
	}
	if(Application->carousel_url) {
		gf_free(Application->carousel_url);
	}

	if(Application->appli_name) {
		gf_free(Application->appli_name);
	}
	gf_free(Application);
}

void  gf_m2ts_delete_channel_application_info(GF_M2TS_CHANNEL_APPLICATION_INFO* ChannelApp) {

	while(gf_list_count(ChannelApp->Application)) {
		GF_M2TS_AIT_APPLICATION* Application = (GF_M2TS_AIT_APPLICATION*)gf_list_get(ChannelApp->Application,0);
		gf_m2ts_free_ait_application(Application);
		gf_list_rem(ChannelApp->Application,0);
	}

	gf_list_del(ChannelApp->Application);
	gf_free(ChannelApp);
}

#endif //GPAC_ENABLE_DSMCC



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


#include <gpac/dsmcc.h>

#ifdef GPAC_ENABLE_DSMCC

/* DSMCC */


/* static functions */
static GF_Err dsmcc_download_data_validation(GF_M2TS_DSMCC_OVERLORD* dsmcc_overlord,GF_M2TS_DSMCC_DOWNLOAD_DATA_BLOCK* DownloadDataBlock,GF_M2TS_DSMCC_MODULE* dsmcc_module,u32 downloadId);
static GF_Err gf_m2ts_dsmcc_process_compatibility_descriptor(GF_M2TS_DSMCC_COMPATIBILITY_DESCRIPTOR *CompatibilityDesc, char* data,GF_BitStream *bs,u32* data_shift);
static GF_Err gf_m2ts_dsmcc_process_message_header(GF_M2TS_DSMCC_MESSAGE_DATA_HEADER *MessageHeader, char* data,GF_BitStream *bs,u32* data_shift,u32 mode);
static GF_Err gf_m2ts_dsmcc_download_data(GF_M2TS_DSMCC_OVERLORD *dsmcc_overlord,GF_M2TS_DSMCC_SECTION *dsmcc, char  *data, GF_BitStream *bs,u32* data_shift);
static GF_Err gf_m2ts_dsmcc_section_delete(GF_M2TS_DSMCC_SECTION *dsmcc);
static GF_Err gf_m2ts_dsmcc_delete_message_header(GF_M2TS_DSMCC_MESSAGE_DATA_HEADER *MessageHeader);
static GF_Err gf_m2ts_dsmcc_delete_compatibility_descriptor(GF_M2TS_DSMCC_COMPATIBILITY_DESCRIPTOR *CompatibilityDesc);


static GF_Err dsmcc_module_delete(GF_M2TS_DSMCC_MODULE* dsmcc_module);
//static GF_Err dsmcc_module_state(GF_M2TS_DSMCC_OVERLORD* dsmcc_overlord,u32 moduleId,u32 moduleVersion);
static GF_Err dsmcc_create_module_validation(GF_M2TS_DSMCC_INFO_MODULES* InfoModules, u32 downloadId, GF_M2TS_DSMCC_OVERLORD* dsmcc_overlord,u32 nb_module);
static GF_Err dsmcc_module_complete(GF_M2TS_DSMCC_OVERLORD* dsmcc_overlord,GF_M2TS_DSMCC_MODULE* dsmcc_module,u32 moduleIndex);
static GF_Err dsmcc_get_biop_module_info(GF_M2TS_DSMCC_MODULE* dsmcc_module,char* data,u8 data_size);
static GF_M2TS_DSMCC_MODULE* dsmcc_create_module(GF_M2TS_DSMCC_OVERLORD* dsmcc_overlord, GF_M2TS_DSMCC_DOWNLOAD_INFO_RESP_INDIC* DownloadInfoIndication);

/* BIOP */
static GF_Err dsmcc_process_biop_data(GF_M2TS_DSMCC_OVERLORD* dsmcc_overlord,GF_M2TS_DSMCC_MODULE* dsmcc_module,char* data,u32 data_size);
static GF_M2TS_DSMCC_BIOP_HEADER* dsmcc_process_biop_header(GF_BitStream* bs);
static GF_Err dsmcc_process_biop_file(GF_BitStream* bs,GF_M2TS_DSMCC_BIOP_HEADER* BIOP_Header,GF_M2TS_DSMCC_OVERLORD*dsmcc_overlord,u16 moduleId,u32 downloadId);
static GF_Err dsmcc_process_biop_directory(GF_BitStream* bs,GF_M2TS_DSMCC_BIOP_HEADER* BIOP_Header,GF_M2TS_DSMCC_OVERLORD*dsmcc_overlord,Bool IsServiceGateway);
static GF_Err dsmcc_process_biop_stream_event(GF_BitStream* bs,GF_M2TS_DSMCC_BIOP_HEADER* BIOP_Header,GF_M2TS_DSMCC_SERVICE_GATEWAY* ServiceGateway);
static GF_Err dsmcc_process_biop_stream_message(GF_BitStream* bs,GF_M2TS_DSMCC_BIOP_HEADER* BIOP_Header,GF_M2TS_DSMCC_SERVICE_GATEWAY* ServiceGateway);

/* Tools */
static void dsmcc_biop_get_context(GF_BitStream* bs,GF_M2TS_DSMCC_SERVICE_CONTEXT* Context,u32 serviceContextList_count);
static void dsmcc_biop_descriptor(GF_BitStream* bs,GF_List* list,u32 size);
static GF_Err dsmcc_biop_get_ior(GF_BitStream* bs,GF_M2TS_DSMCC_IOR* IOR);
static GF_M2TS_DSMCC_DIR* dsmcc_get_directory(GF_List* List, u32 objectKey_data);
static GF_M2TS_DSMCC_FILE* dsmcc_get_file(GF_List* ServiceGatewayList,u16 moduleId,u32 downloadId,u32 objectKey_data);
static GF_Err dsmcc_check_element_validation(GF_List* List,char* Parent_name,GF_M2TS_DSMCC_BIOP_NAME Name);
static char* dsmcc_get_file_namepath(GF_M2TS_DSMCC_DIR* Dir,char* name);

/* Free */
static void dsmcc_free_biop_context(GF_M2TS_DSMCC_SERVICE_CONTEXT* Context,u32 serviceContextList_count);
static void dsmcc_free_biop_name(GF_M2TS_DSMCC_BIOP_NAME* Name, u32 nb_name);
static void dsmcc_free_biop_descriptor(GF_List* list);
static void dsmcc_free_biop_ior(GF_M2TS_DSMCC_IOR* IOR);
static void dsmcc_free_biop_header(GF_M2TS_DSMCC_BIOP_HEADER* BIOP_Header);
static void dsmcc_free_biop_directory(GF_M2TS_DSMCC_BIOP_DIRECTORY* BIOP_Directory);
static void dsmcc_free_biop_file(GF_M2TS_DSMCC_BIOP_FILE* BIOP_File);
static void dsmcc_free_biop_stream_event(GF_M2TS_DSMCC_BIOP_STREAM_EVENT* BIOP_StreamEvent);
static void dsmcc_free_biop_stream_message(GF_M2TS_DSMCC_BIOP_STREAM_MESSAGE* BIOP_StreamMessage);

GF_EXPORT
GF_M2TS_DSMCC_OVERLORD* gf_m2ts_init_dsmcc_overlord(u32 service_id) {
	GF_M2TS_DSMCC_OVERLORD* dsmcc_overlord;
	GF_SAFEALLOC(dsmcc_overlord,GF_M2TS_DSMCC_OVERLORD);
	dsmcc_overlord->dsmcc_modules = gf_list_new();
	dsmcc_overlord->service_id = service_id;
	return dsmcc_overlord;
}

GF_EXPORT
GF_M2TS_DSMCC_OVERLORD* gf_m2ts_get_dmscc_overlord(GF_List* Dsmcc_controller,u32 service_id)
{
	u16 nb_dsmcc,i;

	nb_dsmcc = gf_list_count(Dsmcc_controller);

	if(!nb_dsmcc) {
		return NULL;
	} else {
		for(i =0; i<nb_dsmcc; i++) {
			GF_M2TS_DSMCC_OVERLORD* dsmcc_overlord = (GF_M2TS_DSMCC_OVERLORD*)gf_list_get(Dsmcc_controller,i);
			if(dsmcc_overlord->service_id == service_id) {
				return dsmcc_overlord;
			}
		}
	}
	return NULL;
}

void on_dsmcc_section(GF_M2TS_Demuxer *ts, u32 evt_type, void *par)
{
	GF_M2TS_SL_PCK *pck = (GF_M2TS_SL_PCK *)par;
	char *data;
	u32 u32_data_size;
	u32 u32_table_id;
	GF_M2TS_DSMCC_OVERLORD* dsmcc_overlord;

	dsmcc_overlord = gf_m2ts_get_dmscc_overlord(ts->dsmcc_controler,pck->stream->service_id);

	if (dsmcc_overlord && evt_type == GF_M2TS_EVT_DSMCC_FOUND) {
		GF_Err e;
		GF_M2TS_DSMCC_SECTION* dsmcc;
		data = pck->data;
		u32_data_size = pck->data_len;
		u32_table_id = data[0];
		GF_SAFEALLOC(dsmcc,GF_M2TS_DSMCC_SECTION);

		e = gf_m2ts_process_dsmcc(dsmcc_overlord,dsmcc,data,u32_data_size,u32_table_id);
		if (ts->on_event) ts->on_event(ts, GF_M2TS_EVT_DSMCC_FOUND, pck);
		assert(e == GF_OK);
	}
}

GF_EXPORT
GF_Err gf_m2ts_process_dsmcc(GF_M2TS_DSMCC_OVERLORD* dsmcc_overlord,GF_M2TS_DSMCC_SECTION *dsmcc, char  *data, u32 data_size, u32 table_id)
{
	GF_BitStream *bs;
	u32 data_shift,reserved_test;

	data_shift = 0;
	//first_section = *first_section_received;
	bs = gf_bs_new(data,data_size,GF_BITSTREAM_READ);

	dsmcc->table_id = gf_bs_read_int(bs,8);
	dsmcc->section_syntax_indicator = gf_bs_read_int(bs,1);
	dsmcc->private_indicator = gf_bs_read_int(bs,1);
	reserved_test = gf_bs_read_int(bs,2);
	if (reserved_test != 3) {
		GF_LOG(GF_LOG_INFO, GF_LOG_CONTAINER, ("[Process DSMCC] test reserved flag is not at 3. Corrupted section, abording processing\n"));
		return GF_CORRUPTED_DATA;
	}
	dsmcc->dsmcc_section_length = gf_bs_read_int(bs,12);
	if (dsmcc->dsmcc_section_length > DSMCC_SECTION_LENGTH_MAX) {
		GF_LOG(GF_LOG_INFO, GF_LOG_CONTAINER, ("[Process DSMCC] section length should not exceed 4096. Wrong section, abording processing \n"));
		return GF_CORRUPTED_DATA;
	}
	dsmcc->table_id_extension = gf_bs_read_int(bs,16);

	/*bit shifting */
	gf_bs_read_int(bs,2);

	dsmcc->version_number = gf_bs_read_int(bs,5);
	if(dsmcc->version_number != 0 &&(dsmcc->table_id == GF_M2TS_TABLE_ID_DSM_CC_ENCAPSULATED_DATA || dsmcc->table_id == GF_M2TS_TABLE_ID_DSM_CC_UN_MESSAGE)) {
		GF_LOG(GF_LOG_INFO, GF_LOG_CONTAINER, ("[Process DSMCC] Version number should be 0 for Encapsulated Data or UN Message, abording processing \n"));
		return GF_CORRUPTED_DATA;
	}

	dsmcc->current_next_indicator = gf_bs_read_int(bs,1);
	if (!dsmcc->current_next_indicator) {
		GF_LOG(GF_LOG_INFO, GF_LOG_CONTAINER, ("[Process DSMCC] current next indicator should be at 1 \n"));
		return GF_CORRUPTED_DATA;
	}
	dsmcc->section_number = gf_bs_read_int(bs,8);
	/*if(dsmcc->section_number >0 && ((*first_section_received) == 0)){
	GF_LOG(GF_LOG_INFO, GF_LOG_CONTAINER, ("[Process DSMCC] Wainting for the first section \n"));
	return GF_CORRUPTED_DATA;
	}
	*first_section_received = 1;*/
	dsmcc->last_section_number = gf_bs_read_int(bs,8);
	//fprintf(stderr, "\nsection_number %d last_section_number %d\n",dsmcc->section_number,dsmcc->last_section_number);
	//fprintf(stderr, "dsmcc->table_id %d \n",dsmcc->table_id);
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
		gf_m2ts_dsmcc_download_data(dsmcc_overlord,dsmcc,data,bs,&data_shift);
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
	return GF_CORRUPTED_DATA;
	}*/

	//gf_m2ts_dsmcc_extract_info(ts,dsmcc);
	gf_m2ts_dsmcc_section_delete(dsmcc);

	return GF_OK;
}

static GF_Err gf_m2ts_dsmcc_download_data(GF_M2TS_DSMCC_OVERLORD *dsmcc_overlord,GF_M2TS_DSMCC_SECTION *dsmcc, char  *data, GF_BitStream *bs,u32* data_shift)
{
	GF_M2TS_DSMCC_DOWNLOAD_DATA_MESSAGE* DataMessage;
	GF_SAFEALLOC(DataMessage,GF_M2TS_DSMCC_DOWNLOAD_DATA_MESSAGE);

	/* Header */
	gf_m2ts_dsmcc_process_message_header(&DataMessage->DownloadDataHeader,data,bs,data_shift,1);
	dsmcc->DSMCC_Extension = DataMessage;

	//fprintf(stderr, "DataMessage->DownloadDataHeader.messageId %d \n",DataMessage->DownloadDataHeader.messageId);

	switch (DataMessage->DownloadDataHeader.messageId)
	{
	case DOWNLOAD_INFO_REQUEST:
	{
		GF_M2TS_DSMCC_DOWNLOAD_INFO_REQUEST*  DownloadInfoRequest;
		GF_SAFEALLOC(DownloadInfoRequest,GF_M2TS_DSMCC_DOWNLOAD_INFO_REQUEST);
		DataMessage->dataMessagePayload = DownloadInfoRequest;

		/* Payload */
		DownloadInfoRequest->bufferSize = gf_bs_read_int(bs,32);
		DownloadInfoRequest->maximumBlockSize = gf_bs_read_int(bs,16);
		gf_m2ts_dsmcc_process_compatibility_descriptor(&DownloadInfoRequest->CompatibilityDescr,data,bs,data_shift);
		DownloadInfoRequest->privateDataLength = gf_bs_read_int(bs,16);
		DownloadInfoRequest->privateDataByte = (char*)gf_calloc(DownloadInfoRequest->privateDataLength,sizeof(char));
		gf_bs_read_data(bs,DownloadInfoRequest->privateDataByte,(u32)(DownloadInfoRequest->privateDataLength));
		break;
	}
	case DOWNLOAD_INFO_REPONSE_INDICATION:
	{
		u32 i,nb_modules;
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
		/* Versioning of the DownloadInfoIndication is made by the field transactionId (here known as downloadId) */
		if(DataMessage->DownloadDataHeader.downloadId > dsmcc_overlord->transactionId) {
			dsmcc_overlord->transactionId = DataMessage->DownloadDataHeader.downloadId;
			nb_modules = gf_list_count(dsmcc_overlord->dsmcc_modules);
			for(i = 0; i<DownloadInfoIndication->numberOfModules; i++) {
				DownloadInfoIndication->Modules.moduleId = gf_bs_read_int(bs,16);
				DownloadInfoIndication->Modules.moduleSize = gf_bs_read_int(bs,32);
				DownloadInfoIndication->Modules.moduleVersion = gf_bs_read_int(bs,8);
				DownloadInfoIndication->Modules.moduleInfoLength = gf_bs_read_int(bs,8);
				DownloadInfoIndication->Modules.moduleInfoByte = (char*)gf_calloc(DownloadInfoIndication->Modules.moduleInfoLength,sizeof(char));
				gf_bs_read_data(bs,DownloadInfoIndication->Modules.moduleInfoByte,(u32)(DownloadInfoIndication->Modules.moduleInfoLength));
				if(!dsmcc_create_module_validation(&DownloadInfoIndication->Modules,DownloadInfoIndication->downloadId,dsmcc_overlord,nb_modules)) {
					/* Creation of the module */
					GF_M2TS_DSMCC_MODULE*dsmcc_module = dsmcc_create_module(dsmcc_overlord,DownloadInfoIndication);
					if(DownloadInfoIndication->Modules.moduleInfoLength) {
						GF_Err e;
						e = dsmcc_get_biop_module_info(dsmcc_module,DownloadInfoIndication->Modules.moduleInfoByte,DownloadInfoIndication->Modules.moduleInfoLength);
						if(e) {
							GF_LOG(GF_LOG_INFO, GF_LOG_CONTAINER, ("[Process DSMCC] Error in BIOP Module Info for module %d, abording the processing \n",dsmcc_module->moduleId));
							gf_free(DownloadInfoIndication->Modules.moduleInfoByte);
							DownloadInfoIndication->Modules.moduleInfoByte = NULL;
							return GF_CORRUPTED_DATA;
						}
					}
				}
				gf_free(DownloadInfoIndication->Modules.moduleInfoByte);
				DownloadInfoIndication->Modules.moduleInfoByte = NULL;
			}
			DownloadInfoIndication->privateDataLength = gf_bs_read_int(bs,16);
			DownloadInfoIndication->privateDataByte = (char*)gf_calloc(DownloadInfoIndication->privateDataLength,sizeof(char));
			gf_bs_read_data(bs,DownloadInfoIndication->privateDataByte,(u32)(DownloadInfoIndication->privateDataLength));
		}
		break;
	}
	case DOWNLOAD_DATA_BLOCK:
	{
		u32 modules_count, i;
		GF_M2TS_DSMCC_DOWNLOAD_DATA_BLOCK* DownloadDataBlock;
		GF_SAFEALLOC(DownloadDataBlock,GF_M2TS_DSMCC_DOWNLOAD_DATA_BLOCK);
		DataMessage->dataMessagePayload = DownloadDataBlock;
		modules_count = 0;
		modules_count = gf_list_count(dsmcc_overlord->dsmcc_modules);

		if(!modules_count) {
			GF_LOG(GF_LOG_INFO, GF_LOG_CONTAINER, ("[Process DSMCC] Download Information Indicator has not been received yet, waiting before processing data block \n"));
			break;
		}


		DownloadDataBlock->moduleId = gf_bs_read_int(bs,16);
		//fprintf(stderr, "DownloadDataBlock->moduleId %d \n",DownloadDataBlock->moduleId);
		DownloadDataBlock->moduleVersion = gf_bs_read_int(bs,8);
		DownloadDataBlock->reserved = gf_bs_read_int(bs,8);
		if(DownloadDataBlock->reserved != 0xFF) {
			GF_LOG(GF_LOG_INFO, GF_LOG_CONTAINER, ("[Process DSMCC] DataHeader reserved slot does not have the correct value, abording the processing \n"));
			return GF_CORRUPTED_DATA;
		}
		DownloadDataBlock->blockNumber = gf_bs_read_int(bs,16);

		for(i=0; i<modules_count; i++) {
			GF_M2TS_DSMCC_MODULE* dsmcc_module = gf_list_get(dsmcc_overlord->dsmcc_modules,i);
			/* Test if the data are compatible with the module configuration */
			if(!dsmcc_download_data_validation(dsmcc_overlord,DownloadDataBlock,dsmcc_module,DataMessage->DownloadDataHeader.downloadId)) {
				//fprintf(stderr, "DownloadDataBlock->blockNumber %d \n\n",DownloadDataBlock->blockNumber);
				DownloadDataBlock->dataBlocksize = (DataMessage->DownloadDataHeader.messageLength - 6);
				if(dsmcc_module->block_size < DownloadDataBlock->dataBlocksize) {
					GF_LOG(GF_LOG_INFO, GF_LOG_CONTAINER, ("[Process DSMCC] Error block_size should be >= to DownloadDataBlock->dataBlocksize , abording the processing \n"));
					return GF_CORRUPTED_DATA;
				}
				DownloadDataBlock->blockDataByte = (char*)gf_calloc(DownloadDataBlock->dataBlocksize,sizeof(char));
				*data_shift = (u32)(gf_bs_get_position(bs));
				gf_bs_read_data(bs,DownloadDataBlock->blockDataByte,DownloadDataBlock->dataBlocksize);
				memcpy(dsmcc_module->buffer+dsmcc_module->byte_sift,DownloadDataBlock->blockDataByte,DownloadDataBlock->dataBlocksize*sizeof(char));
				dsmcc_module->byte_sift += DownloadDataBlock->dataBlocksize;
				dsmcc_module->last_section_number = dsmcc->last_section_number;
				dsmcc_module->section_number++;
				if(dsmcc_module->section_number == (dsmcc_module->last_section_number+1)) {
					dsmcc_module_complete(dsmcc_overlord,dsmcc_module,i);
				}
				break;
			}
		}

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
		if(DownloadCancel->privateDataLength) {
			DownloadCancel->privateDataByte = (char*)gf_calloc(DownloadCancel->privateDataLength,sizeof(char));
			gf_bs_read_data(bs,DownloadCancel->privateDataByte,(u32)(DownloadCancel->privateDataLength));
		}
		break;

	}
	case DOWNLOAD_SERVER_INITIATE:
	{
		GF_Err e;
		GF_M2TS_DSMCC_DOWNLOAD_SERVER_INIT* DownloadServerInit;
		GF_SAFEALLOC(DownloadServerInit,GF_M2TS_DSMCC_DOWNLOAD_SERVER_INIT);
		DataMessage->dataMessagePayload = DownloadServerInit;
		gf_bs_read_data(bs,DownloadServerInit->serverId,20);
		gf_m2ts_dsmcc_process_compatibility_descriptor(&DownloadServerInit->CompatibilityDescr,data,bs,data_shift);
		DownloadServerInit->privateDataLength = gf_bs_read_int(bs,16);
		if(DownloadServerInit->privateDataLength) {

			u32 i;

			GF_M2TS_DSMCC_SERVICE_GATEWAY_INFO* ServiceGateWayInfo;
			GF_SAFEALLOC(ServiceGateWayInfo,GF_M2TS_DSMCC_SERVICE_GATEWAY_INFO);

			/* IOR */
			e = dsmcc_biop_get_ior(bs,&ServiceGateWayInfo->IOR);
			if(e) {
				GF_LOG(GF_LOG_INFO, GF_LOG_CONTAINER, ("[Process DSMCC] Corrupted IOR, abording the processing \n"));
				gf_free(ServiceGateWayInfo);
				return GF_CORRUPTED_DATA;
			}
			ServiceGateWayInfo->downloadTaps_count = gf_bs_read_int(bs,8);
			ServiceGateWayInfo->Taps = (GF_M2TS_DSMCC_BIOP_TAPS*)gf_calloc(ServiceGateWayInfo->downloadTaps_count,sizeof(GF_M2TS_DSMCC_BIOP_TAPS));
			for(i=0; i<ServiceGateWayInfo->downloadTaps_count; i++) {
				ServiceGateWayInfo->Taps[i].id = gf_bs_read_int(bs,16);
				ServiceGateWayInfo->Taps[i].use = gf_bs_read_int(bs,16);
				ServiceGateWayInfo->Taps[i].assocTag = gf_bs_read_int(bs,16);
				ServiceGateWayInfo->Taps[i].selector_length = gf_bs_read_int(bs,8);
				ServiceGateWayInfo->Taps[i].selector_type = gf_bs_read_int(bs,16);
				ServiceGateWayInfo->Taps[i].transactionId = gf_bs_read_int(bs,32);
				ServiceGateWayInfo->Taps[i].timeout = gf_bs_read_int(bs,32);
			}
			ServiceGateWayInfo->serviceContextList_count = gf_bs_read_int(bs,8);
			ServiceGateWayInfo->ServiceContext = (GF_M2TS_DSMCC_SERVICE_CONTEXT*)gf_calloc(ServiceGateWayInfo->serviceContextList_count,sizeof(GF_M2TS_DSMCC_SERVICE_CONTEXT));
			dsmcc_biop_get_context(bs,ServiceGateWayInfo->ServiceContext,ServiceGateWayInfo->serviceContextList_count);
			ServiceGateWayInfo->userInfoLength = gf_bs_read_int(bs,16);
			if(ServiceGateWayInfo->userInfoLength != 0) {
				ServiceGateWayInfo->userInfo_data = (char*)gf_calloc(ServiceGateWayInfo->userInfoLength,sizeof(char));
				gf_bs_read_data(bs,ServiceGateWayInfo->userInfo_data,(u32)(ServiceGateWayInfo->userInfoLength));
			}

			if(!dsmcc_overlord->ServiceGateway && gf_list_count(ServiceGateWayInfo->IOR.taggedProfile)) {
				GF_M2TS_DSMCC_BIOP_TAGGED_PROFILE* taggedProfile = (GF_M2TS_DSMCC_BIOP_TAGGED_PROFILE*)gf_list_get(ServiceGateWayInfo->IOR.taggedProfile,0);
				dsmcc_overlord->ServiceGateway = (GF_M2TS_DSMCC_SERVICE_GATEWAY*)gf_calloc(1,sizeof(GF_M2TS_DSMCC_SERVICE_GATEWAY));
				dsmcc_overlord->ServiceGateway->downloadId = taggedProfile->BIOPProfileBody->ObjectLocation.carouselId;
				dsmcc_overlord->ServiceGateway->moduleId = taggedProfile->BIOPProfileBody->ObjectLocation.moduleId;
				dsmcc_overlord->ServiceGateway->service_id = dsmcc_overlord->service_id;
				dsmcc_overlord->ServiceGateway->File = gf_list_new();
				dsmcc_overlord->ServiceGateway->Dir = gf_list_new();
			}
		}

		break;
	}

	default:
	{
		GF_LOG(GF_LOG_INFO, GF_LOG_CONTAINER, ("[Process DSMCC] Unknown dataMessagePayload Type \n"));
		break;
	}
	}

	//dsmcc->DSMCC_Extension = DataMessage;

	return GF_OK;
}




static GF_Err gf_m2ts_dsmcc_process_message_header(GF_M2TS_DSMCC_MESSAGE_DATA_HEADER *MessageHeader, char* data,GF_BitStream *bs,u32* data_shift,u32 mode)
{
	u32 byte_shift;

	byte_shift = *data_shift;

	MessageHeader->protocolDiscriminator = gf_bs_read_int(bs,8);
	if (MessageHeader->protocolDiscriminator != 0x11) {
		GF_LOG(GF_LOG_INFO, GF_LOG_CONTAINER, ("[Process DSMCC] DataHeader Protocol Discriminator slot does not have the correct value, abording the processing \n"));
		return GF_CORRUPTED_DATA;
	}
	MessageHeader->dsmccType = gf_bs_read_int(bs,8);
	MessageHeader->messageId = gf_bs_read_int(bs,16);
	/* mode 0 for Message Header - 1 for Download Data header */
	if (mode == 0) {
		MessageHeader->transactionId = gf_bs_read_int(bs,32);
	} else if (mode == 1) {
		MessageHeader->downloadId = gf_bs_read_int(bs,32);
	}
	//fprintf(stderr, "MessageHeader->downloadId %d \n",MessageHeader->downloadId);
	MessageHeader->reserved = gf_bs_read_int(bs,8);
	if (MessageHeader->reserved != 0xFF) {
		GF_LOG(GF_LOG_INFO, GF_LOG_CONTAINER, ("[Process DSMCC] DataHeader reserved slot does not have the correct value, abording the processing \n"));
		return GF_CORRUPTED_DATA;
	}
	MessageHeader->adaptationLength = gf_bs_read_int(bs,8);
	MessageHeader->header_length = ((u32)(gf_bs_get_position(bs)) - byte_shift);
	MessageHeader->messageLength = gf_bs_read_int(bs,16);

	if (MessageHeader->adaptationLength > 0) {
		MessageHeader->DsmccAdaptationHeader = (GF_M2TS_DSMCC_ADAPTATION_HEADER*)gf_calloc(1, sizeof(GF_M2TS_DSMCC_ADAPTATION_HEADER));
		MessageHeader->DsmccAdaptationHeader->adaptationType = gf_bs_read_int(bs,8);

		MessageHeader->DsmccAdaptationHeader->adaptationDataByte = (char*)gf_calloc(MessageHeader->adaptationLength-1,sizeof(char));
		gf_bs_read_data(bs,MessageHeader->DsmccAdaptationHeader->adaptationDataByte,(u32)(MessageHeader->adaptationLength));

	}

	*data_shift = (u32)(gf_bs_get_position(bs));

	return GF_OK;
}

static GF_Err gf_m2ts_dsmcc_process_compatibility_descriptor(GF_M2TS_DSMCC_COMPATIBILITY_DESCRIPTOR *CompatibilityDesc, char* data,GF_BitStream *bs,u32* data_shift)
{
	u32 i,j,byte_shift;

	byte_shift = (u32)(gf_bs_get_position(bs));

	CompatibilityDesc->compatibilityDescriptorLength = gf_bs_read_int(bs,16);

	if(CompatibilityDesc->compatibilityDescriptorLength) {
		CompatibilityDesc->descriptorCount = gf_bs_read_int(bs,16);
		if(CompatibilityDesc->descriptorCount) {
			CompatibilityDesc->Descriptor = (GF_M2TS_DSMCC_DESCRIPTOR*)gf_calloc(CompatibilityDesc->descriptorCount,sizeof(GF_M2TS_DSMCC_DESCRIPTOR));
			for(i=0; i<CompatibilityDesc->descriptorCount; i++) {
				CompatibilityDesc->Descriptor[i].descriptorType = gf_bs_read_int(bs,8);
				CompatibilityDesc->Descriptor[i].descriptorLength = gf_bs_read_int(bs,8);
				CompatibilityDesc->Descriptor[i].specifierType = gf_bs_read_int(bs,8);
				CompatibilityDesc->Descriptor[i].specifierData = gf_bs_read_int(bs,21);
				CompatibilityDesc->Descriptor[i].model = gf_bs_read_int(bs,16);
				CompatibilityDesc->Descriptor[i].version = gf_bs_read_int(bs,16);
				CompatibilityDesc->Descriptor[i].subDescriptorCount = gf_bs_read_int(bs,8);
				if(CompatibilityDesc->Descriptor[i].subDescriptorCount) {
					CompatibilityDesc->Descriptor[i].SubDescriptor  = (GF_M2TS_DSMCC_SUBDESCRIPTOR*)gf_calloc(CompatibilityDesc->Descriptor[i].subDescriptorCount,sizeof(GF_M2TS_DSMCC_SUBDESCRIPTOR));
					for(j=0; j>CompatibilityDesc->Descriptor[i].subDescriptorCount; j++) {
						CompatibilityDesc->Descriptor[i].SubDescriptor[j].subDescriptorType = gf_bs_read_int(bs,8);
						CompatibilityDesc->Descriptor[i].SubDescriptor[j].subDescriptorLength = gf_bs_read_int(bs,8);
						if(CompatibilityDesc->Descriptor[i].SubDescriptor[j].subDescriptorLength) {
							CompatibilityDesc->Descriptor[i].SubDescriptor[j].additionalInformation = (char*)gf_calloc(CompatibilityDesc->Descriptor[i].SubDescriptor[j].subDescriptorLength,sizeof(char));
							gf_bs_read_data(bs,CompatibilityDesc->Descriptor[i].SubDescriptor[j].additionalInformation,(u32)(CompatibilityDesc->Descriptor[i].SubDescriptor[j].subDescriptorLength));
						}

					}
				}
			}
		}
	}

	*data_shift = (u32)(gf_bs_get_position(bs));
	if(*data_shift != byte_shift+2+CompatibilityDesc->compatibilityDescriptorLength) {
		GF_LOG(GF_LOG_INFO, GF_LOG_CONTAINER, ("[Process DSMCC] Descriptor length not respected, difference between %d and %d \n",(*data_shift - byte_shift),2+CompatibilityDesc->compatibilityDescriptorLength));
		return GF_CORRUPTED_DATA;
	}

	return GF_OK;
}

static GF_Err dsmcc_create_module_validation(GF_M2TS_DSMCC_INFO_MODULES* InfoModules, u32 downloadId, GF_M2TS_DSMCC_OVERLORD* dsmcc_overlord,u32 nb_module) {

	u32 i;
	for (i=0; i<nb_module; i++) {
		GF_M2TS_DSMCC_PROCESSED dsmcc_process = dsmcc_overlord->processed[i];
		if ((InfoModules->moduleId == dsmcc_process.moduleId) && (downloadId == dsmcc_process.downloadId)) {
			if (InfoModules->moduleVersion <= dsmcc_process.version_number) {
				GF_LOG(GF_LOG_INFO, GF_LOG_CONTAINER, ("[Process DSMCC] Module already intialized \n"));
				return GF_CORRUPTED_DATA;
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

//static GF_Err dsmcc_module_state(GF_M2TS_DSMCC_OVERLORD* dsmcc_overlord,u32 moduleId,u32 moduleVersion){
//
//	u32 i,nb_module;
//	nb_module = gf_list_count(dsmcc_overlord->dsmcc_modules);
//	/* This test comes from the fact that the moduleVersion only borrow the least 5 significant bits of the moduleVersion conveys in DownloadDataBlock */
//	/* If the moduleVersion is eq to 0x1F, it does not tell if it is clearly 0x1F or a superior value. So in this case it is better to process the data */
//	/* If the moduleVersion is eq to 0x0, it means that the data conveys a DownloadDataResponse, so it has to be processed */
//	if (moduleVersion != 0 || moduleVersion < 0x1F) {
//		for (i=0; i<nb_module; i++) {
//			GF_M2TS_DSMCC_PROCESSED dsmcc_process = dsmcc_overlord->processed[i];
//			if ((moduleId == dsmcc_process.moduleId) && (moduleVersion <= dsmcc_process.version_number)) {
//				if (dsmcc_process.done) {
//					GF_LOG(GF_LOG_INFO, GF_LOG_CONTAINER, ("[Process DSMCC] Module already processed \n"));
//					return GF_CORRUPTED_DATA;
//				} else {
//					return GF_OK;
//				}
//			}
//		}
//	}
//	return  GF_OK;
//}

static GF_Err dsmcc_download_data_validation(GF_M2TS_DSMCC_OVERLORD* dsmcc_overlord,GF_M2TS_DSMCC_DOWNLOAD_DATA_BLOCK* DownloadDataBlock,GF_M2TS_DSMCC_MODULE* dsmcc_module,u32 downloadId)
{
	/* It checks if the module Id is eq to the SGW's module Id if Got_ServiceGateway is null (means that the SWG has not been processed yet
		If then Got_ServiceGateway = 1, all the module are processed */
	if(dsmcc_overlord->ServiceGateway) {
		if ((dsmcc_overlord->Got_ServiceGateway || dsmcc_module->moduleId == dsmcc_overlord->ServiceGateway->moduleId)&&
		        ((dsmcc_module->moduleId == DownloadDataBlock->moduleId) && (dsmcc_module->section_number == DownloadDataBlock->blockNumber) &&
		         (dsmcc_module->downloadId == downloadId) && (dsmcc_module->version_number == DownloadDataBlock->moduleVersion))) {
			return GF_OK;
		}
	}

	return GF_CORRUPTED_DATA;
}

static GF_Err dsmcc_module_complete(GF_M2TS_DSMCC_OVERLORD* dsmcc_overlord,GF_M2TS_DSMCC_MODULE* dsmcc_module,u32 moduleIndex)
{
	u32 i,nb_module;
	GF_Err e;
	e = GF_OK;
	nb_module = gf_list_count(dsmcc_overlord->dsmcc_modules);
	for (i=0; i<nb_module; i++) {
		GF_M2TS_DSMCC_PROCESSED dsmcc_process = dsmcc_overlord->processed[i];
		if ((dsmcc_module->moduleId == dsmcc_process.moduleId) && dsmcc_module->version_number == dsmcc_process.version_number && dsmcc_module->downloadId == dsmcc_process.downloadId) {
			/*process buffer*/
			if(dsmcc_module->Gzip) {
				u32 uncomp_size;
				char* uncompressed_data;

				gf_gz_decompress_payload(dsmcc_module->buffer,dsmcc_module->byte_sift,&uncompressed_data, &uncomp_size);
				//dsmcc_process_biop_data(dsmcc_overlord,dsmcc_module,uncompressed_data,dsmcc_module->original_size);

				if(dsmcc_module->original_size == uncomp_size) {
					e = dsmcc_process_biop_data(dsmcc_overlord,dsmcc_module,uncompressed_data,dsmcc_module->original_size);
				} else {
					GF_LOG(GF_LOG_INFO, GF_LOG_CONTAINER, ("[Process DSMCC] Buffer size is not equal to the module size. Flushing the data \n"));
					gf_free(dsmcc_module->buffer);
					dsmcc_module->buffer = NULL;
					dsmcc_module->buffer = (char*)gf_calloc(dsmcc_module->size,sizeof(char));
					dsmcc_module->section_number = 0;
					return GF_CORRUPTED_DATA;
				}
			} else {
				e = dsmcc_process_biop_data(dsmcc_overlord,dsmcc_module,dsmcc_module->buffer,dsmcc_module->size);
			}
			if(e) {
				GF_LOG(GF_LOG_INFO, GF_LOG_CONTAINER, ("[Process DSMCC] Error during the processing of the module data. Flushing the data \n"));
				gf_free(dsmcc_module->buffer);
				dsmcc_module->buffer = NULL;
				dsmcc_module->buffer = (char*)gf_calloc(dsmcc_module->size,sizeof(char));
				dsmcc_module->section_number = 0;
				return GF_CORRUPTED_DATA;
			} else {
				dsmcc_process.done = 1;
				dsmcc_module_delete(dsmcc_module);
				gf_list_rem(dsmcc_overlord->dsmcc_modules,moduleIndex);
			}
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

	if(!DataMessage) {
		return GF_OK;
	}

	switch (DataMessage->DownloadDataHeader.messageId)
	{
	case DOWNLOAD_INFO_REQUEST:
	{
		GF_M2TS_DSMCC_DOWNLOAD_INFO_REQUEST*  DownloadInfoRequest = (GF_M2TS_DSMCC_DOWNLOAD_INFO_REQUEST*)DataMessage->dataMessagePayload;
		if(DownloadInfoRequest->privateDataLength) {
			gf_free(DownloadInfoRequest->privateDataByte);
		}
		gf_free(DownloadInfoRequest);
		break;
	}
	case DOWNLOAD_INFO_REPONSE_INDICATION:
	{
		GF_M2TS_DSMCC_DOWNLOAD_INFO_RESP_INDIC* DownloadInfoIndication = (GF_M2TS_DSMCC_DOWNLOAD_INFO_RESP_INDIC*)DataMessage->dataMessagePayload;

		/* Compatibility Descr */
		gf_m2ts_dsmcc_delete_compatibility_descriptor(&DownloadInfoIndication->CompatibilityDescr);

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


static GF_Err dsmcc_module_delete(GF_M2TS_DSMCC_MODULE* dsmcc_module) {

	gf_free(dsmcc_module->buffer);
	gf_free(dsmcc_module);
	dsmcc_module = NULL;
	return  GF_OK;
}

/* BIOP MESSAGE */

static GF_Err dsmcc_get_biop_module_info(GF_M2TS_DSMCC_MODULE* dsmcc_module,char* data,u8 data_size) {

	GF_M2TS_DSMCC_BIOP_MODULE_INFO* BIOP_ModuleInfo;
	GF_BitStream *bs;
	u8 i;


	bs = gf_bs_new(data,data_size,GF_BITSTREAM_READ);
	GF_SAFEALLOC(BIOP_ModuleInfo,GF_M2TS_DSMCC_BIOP_MODULE_INFO);
	BIOP_ModuleInfo->descriptor = gf_list_new();

	BIOP_ModuleInfo->moduleTimeOut = gf_bs_read_int(bs,32);
	BIOP_ModuleInfo->blockTimeOut = gf_bs_read_int(bs,32);
	BIOP_ModuleInfo->minBlockTime = gf_bs_read_int(bs,32);
	BIOP_ModuleInfo->taps_count = gf_bs_read_int(bs,8);
	if(!BIOP_ModuleInfo->taps_count) {
		GF_LOG(GF_LOG_INFO, GF_LOG_CONTAINER, ("[Process DSMCC] Corrupted payload for BIOP Module Info \n"));
		gf_list_del(BIOP_ModuleInfo->descriptor);
		gf_free(BIOP_ModuleInfo);
		return GF_CORRUPTED_DATA;
	}

	BIOP_ModuleInfo->Taps = (GF_M2TS_DSMCC_BIOP_TAPS*)gf_calloc(BIOP_ModuleInfo->taps_count,sizeof(GF_M2TS_DSMCC_BIOP_TAPS));
	for(i = 0; i < BIOP_ModuleInfo->taps_count; i++) {
		BIOP_ModuleInfo->Taps[i].id = gf_bs_read_int(bs,16);
		BIOP_ModuleInfo->Taps[i].use = gf_bs_read_int(bs,16);
		BIOP_ModuleInfo->Taps[i].assocTag = gf_bs_read_int(bs,16);
		BIOP_ModuleInfo->Taps[i].selector_length = gf_bs_read_int(bs,8);
		if(BIOP_ModuleInfo->Taps[i].selector_length) {
			BIOP_ModuleInfo->Taps[i].selector_data = (char*)gf_calloc(BIOP_ModuleInfo->Taps[i].selector_length,sizeof(char));
			gf_bs_read_data(bs,BIOP_ModuleInfo->Taps[i].selector_data,(u8)(BIOP_ModuleInfo->Taps[i].selector_length));
		}
		if(i == 0 && (BIOP_ModuleInfo->Taps[i].id != 0x00 || BIOP_ModuleInfo->Taps[i].use != 0x17 || BIOP_ModuleInfo->Taps[i].selector_length != 0x00)) {
			GF_LOG(GF_LOG_INFO, GF_LOG_CONTAINER, ("[Process DSMCC] Corrupted payload for BIOP Module Info \n"));
			gf_free(BIOP_ModuleInfo->Taps);
			gf_list_del(BIOP_ModuleInfo->descriptor);
			gf_free(BIOP_ModuleInfo);
			return GF_CORRUPTED_DATA;
		}
	}
	BIOP_ModuleInfo->userInfoLength = gf_bs_read_int(bs,8);
	if(BIOP_ModuleInfo->userInfoLength) {

		u32 nb_desc,j;
		u8* descr_tag;

		dsmcc_biop_descriptor(bs,BIOP_ModuleInfo->descriptor,(u32)(BIOP_ModuleInfo->userInfoLength));

		nb_desc = gf_list_count(BIOP_ModuleInfo->descriptor);
		j = 0;
		while(j<nb_desc) {

			/* get the descriptor tag */
			descr_tag = (u8*)gf_list_get(BIOP_ModuleInfo->descriptor,j);

			switch(*descr_tag) {

			case CACHING_PRIORITY_DESCRIPTOR:
			{
				//GF_M2TS_DSMCC_BIOP_CACHING_PRIORITY_DESCRIPTOR* CachingPriorityDescr = (GF_M2TS_DSMCC_BIOP_CACHING_PRIORITY_DESCRIPTOR*)gf_list_get(BIOP_ModuleInfo->descriptor,j);
				break;
			}
			case COMPRESSED_MODULE_DESCRIPTOR:
			{
				u8 comp_meth;
				GF_M2TS_DSMCC_BIOP_COMPRESSED_MODULE_DESCRIPTOR* CompModuleDescr = (GF_M2TS_DSMCC_BIOP_COMPRESSED_MODULE_DESCRIPTOR*)gf_list_get(BIOP_ModuleInfo->descriptor,j);
				/*if CompModuleDescr->compression_method least significant nibble is eq to 0x08, the terminal shall support the Deflate compression algorithm (GZIP)*/
				comp_meth = (CompModuleDescr->compression_method &0x0F);
				if(comp_meth == 0x08) {
					dsmcc_module->Gzip = 1;
				}
				dsmcc_module->original_size = CompModuleDescr->original_size;
				break;

			}
			default:
			{
				GF_LOG(GF_LOG_INFO, GF_LOG_CONTAINER, ("[Process DSMCC] Unsupported descriptor Type \n"));
				break;
			}
			}
			j++;
		}
	}

	dsmcc_free_biop_descriptor(BIOP_ModuleInfo->descriptor);

	for(i = 0; i < BIOP_ModuleInfo->taps_count; i++) {
		if(BIOP_ModuleInfo->Taps[i].selector_length) {
			gf_free(BIOP_ModuleInfo->Taps[i].selector_data);
		}
	}
	gf_free(BIOP_ModuleInfo->Taps);
	gf_free(BIOP_ModuleInfo);

	return GF_OK;
}

static GF_Err dsmcc_process_biop_data(GF_M2TS_DSMCC_OVERLORD* dsmcc_overlord,GF_M2TS_DSMCC_MODULE* dsmcc_module,char* data,u32 data_size) {

	GF_BitStream *bs;
	GF_Err e;
	Bool Error;
	u32 byte_shift;
	GF_M2TS_DSMCC_BIOP_HEADER* BIOP_Header;
	GF_M2TS_DSMCC_SERVICE_GATEWAY* ServiceGateway = dsmcc_overlord->ServiceGateway;

	e = GF_OK;
	Error = 0;

	bs = gf_bs_new(data,data_size,GF_BITSTREAM_READ);

	byte_shift = (u32)(gf_bs_get_position(bs));

	while(byte_shift < data_size) {

		BIOP_Header = dsmcc_process_biop_header(bs);

		if(BIOP_Header) {
			if(!strcmp(BIOP_Header->objectKind_data,"fil")) {
				e = dsmcc_process_biop_file(bs,BIOP_Header,dsmcc_overlord,dsmcc_module->moduleId,dsmcc_module->downloadId);
			} else if(!strcmp(BIOP_Header->objectKind_data,"dir")) {
				e = dsmcc_process_biop_directory(bs,BIOP_Header,dsmcc_overlord,0);
			} else if(!strcmp(BIOP_Header->objectKind_data,"srg")) {
				e = dsmcc_process_biop_directory(bs,BIOP_Header,dsmcc_overlord,1);
				if(e == GF_OK) {
					dsmcc_overlord->Got_ServiceGateway = 1;
				}
			} else if(!strcmp(BIOP_Header->objectKind_data,"str")) {
				dsmcc_process_biop_stream_message(bs,BIOP_Header,ServiceGateway);
			} else if(!strcmp(BIOP_Header->objectKind_data,"ste")) {
				dsmcc_process_biop_stream_event(bs,BIOP_Header,ServiceGateway);
			} else {
				GF_LOG(GF_LOG_INFO, GF_LOG_CONTAINER, ("[Process DSMCC] Unsupported BIOP Message, abording process \n"));
			}
		}
		byte_shift = (u32)(gf_bs_get_position(bs));
		if((e || !BIOP_Header) && (byte_shift < data_size)) {
			/* Error inside the data. Read next byte until a new "BIOP" is found or data_size is reached */
			gf_bs_read_int(bs,8);
			Error = 1;
		}
		if(BIOP_Header) {
			dsmcc_free_biop_header(BIOP_Header);
		}

	}

	if(Error) {
		return GF_CORRUPTED_DATA;
	}

	return GF_OK;
}

static GF_M2TS_DSMCC_BIOP_HEADER* dsmcc_process_biop_header(GF_BitStream* bs) {

	GF_M2TS_DSMCC_BIOP_HEADER* BIOP_Header;
	GF_SAFEALLOC(BIOP_Header,GF_M2TS_DSMCC_BIOP_HEADER);

	BIOP_Header->magic = gf_bs_read_int(bs,32);
	if(BIOP_Header->magic != 0x42494F50) {
		GF_LOG(GF_LOG_INFO, GF_LOG_CONTAINER, ("[Process DSMCC] Wrong BIOP Header, abording process \n"));
		return NULL;
	}
	BIOP_Header->biop_version_major = gf_bs_read_int(bs,8);
	BIOP_Header->biop_version_minor = gf_bs_read_int(bs,8);
	BIOP_Header->byte_order = gf_bs_read_int(bs,8);
	BIOP_Header->message_type = gf_bs_read_int(bs,8);
	BIOP_Header->message_size = gf_bs_read_int(bs,32);
	BIOP_Header->objectKey_length = gf_bs_read_int(bs,8);
	if(BIOP_Header->objectKey_length) {
		BIOP_Header->objectKey_data = gf_bs_read_int(bs,BIOP_Header->objectKey_length*8);
	}
	BIOP_Header->objectKind_length = gf_bs_read_int(bs,32);
	if(BIOP_Header->objectKind_length) {
		BIOP_Header->objectKind_data = (char*)gf_calloc(BIOP_Header->objectKind_length,sizeof(char));
		gf_bs_read_data(bs,BIOP_Header->objectKind_data,(u32)(BIOP_Header->objectKind_length));
	}
	BIOP_Header->objectInfo_length = gf_bs_read_int(bs,16);

	return BIOP_Header;
}

static GF_Err dsmcc_process_biop_file(GF_BitStream* bs,GF_M2TS_DSMCC_BIOP_HEADER* BIOP_Header,GF_M2TS_DSMCC_OVERLORD*dsmcc_overlord,u16 moduleId,u32 downloadId) {

	u32 nb_desc,descr_size;
	GF_M2TS_DSMCC_BIOP_FILE* BIOP_File;
	GF_M2TS_DSMCC_SERVICE_GATEWAY* ServiceGateway;
	GF_M2TS_DSMCC_FILE* File;
	FILE* pFile;
	u8* descr_tag;

	GF_SAFEALLOC(BIOP_File,GF_M2TS_DSMCC_BIOP_FILE);

	ServiceGateway = dsmcc_overlord->ServiceGateway;

	BIOP_File->Header = BIOP_Header;
	BIOP_File->ContentSize = gf_bs_read_int(bs,64);

	descr_size = BIOP_File->Header->objectInfo_length-8;

	if(descr_size) {
		dsmcc_biop_descriptor(bs,BIOP_File->descriptor,descr_size);
	}

	gf_bs_read_int(bs,(u32)(descr_size));

	nb_desc = gf_list_count(BIOP_File->descriptor);

	while(nb_desc) {

		/* get the descriptor tag */
		descr_tag = (u8*)gf_list_get(BIOP_File->descriptor,0);

		switch(*descr_tag) {
		case CONTENT_TYPE_DESCRIPTOR:
		{
			//GF_M2TS_DSMCC_BIOP_CONTENT_TYPE_DESRIPTOR* ContentTypeDescr = (GF_M2TS_DSMCC_BIOP_CONTENT_TYPE_DESRIPTOR*)gf_list_get(BIOP_File->descriptor,0);
		}
		default:
		{
			GF_LOG(GF_LOG_INFO, GF_LOG_CONTAINER, ("[Process DSMCC] Unsupported descriptor Type \n"));
			break;
		}
		}
		nb_desc--;
	}

	BIOP_File->serviceContextList_count = gf_bs_read_int(bs,8);
	if(BIOP_File->serviceContextList_count) {
		BIOP_File->ServiceContext = (GF_M2TS_DSMCC_SERVICE_CONTEXT*)gf_calloc(BIOP_File->serviceContextList_count,sizeof(GF_M2TS_DSMCC_SERVICE_CONTEXT));
		dsmcc_biop_get_context(bs,BIOP_File->ServiceContext,BIOP_File->serviceContextList_count);
	}
	BIOP_File->messageBody_length = gf_bs_read_int(bs,32);
	BIOP_File->content_length = gf_bs_read_int(bs,32);
	if(BIOP_File->content_length) {
		BIOP_File->content_byte = (char*)gf_calloc(BIOP_File->content_length,sizeof(char));
		gf_bs_read_data(bs,BIOP_File->content_byte,(u32)(BIOP_File->content_length));

	}
	GF_LOG(GF_LOG_INFO, GF_LOG_CONTAINER, ("module_Id %d \n",moduleId));
	File = dsmcc_get_file(ServiceGateway->File,moduleId,downloadId,BIOP_File->Header->objectKey_data);
	if(File) {
		GF_LOG(GF_LOG_INFO, GF_LOG_CONTAINER, ("Fichier: %s module_Id %d place :%d \n",File->Path,moduleId,File->objectKey_data));
		pFile = gf_fopen(File->Path,"wb");
		if (pFile!=NULL) {
			gf_fwrite(BIOP_File->content_byte,1,BIOP_File->content_length ,pFile);
			gf_fclose(pFile);
			GF_LOG(GF_LOG_INFO, GF_LOG_CONTAINER, ("Fichier créé \n\n"));
			if(!strcmp(File->name,"index.html")) {
				dsmcc_overlord->get_index = 1;
			}
		}
	} else {
		GF_LOG(GF_LOG_WARNING, GF_LOG_CONTAINER, ("[DSMCC] File vould not be created\n"));
	}

	dsmcc_free_biop_file(BIOP_File);

	return GF_OK;
}

static GF_Err dsmcc_process_biop_directory(GF_BitStream* bs,GF_M2TS_DSMCC_BIOP_HEADER* BIOP_Header,GF_M2TS_DSMCC_OVERLORD*dsmcc_overlord,Bool IsServiceGateway) {

	GF_M2TS_DSMCC_BIOP_DIRECTORY* BIOP_Directory;
	GF_M2TS_DSMCC_DIR* Dir;
	u32 i;
	GF_M2TS_DSMCC_SERVICE_GATEWAY* ServiceGateway;

	GF_SAFEALLOC(BIOP_Directory,GF_M2TS_DSMCC_BIOP_DIRECTORY);

	ServiceGateway = dsmcc_overlord->ServiceGateway;

	/* Get the Header */
	BIOP_Directory->Header = BIOP_Header;

	if(BIOP_Directory->Header->objectInfo_length != 0x0) {
		GF_LOG(GF_LOG_INFO, GF_LOG_CONTAINER, ("[Process DSMCC] ObjectInfo_length value is not correct \n"));
		return GF_CORRUPTED_DATA;
	}

	if(IsServiceGateway) {
		/* create a "dir" struct with no parent */
		Dir = (GF_M2TS_DSMCC_DIR*)ServiceGateway;
		ServiceGateway->objectKey_data = BIOP_Directory->Header->objectKey_data;
		ServiceGateway->parent = NULL;
		ServiceGateway->name = (char*)gf_strdup(dsmcc_overlord->root_dir);
	} else {
		/* get the dir related to the payload */
		Dir = dsmcc_get_directory(ServiceGateway->Dir,BIOP_Directory->Header->objectKey_data);
	}

	BIOP_Directory->serviceContextList_count = gf_bs_read_int(bs,8);
	if(BIOP_Directory->serviceContextList_count) {
		BIOP_Directory->ServiceContext = (GF_M2TS_DSMCC_SERVICE_CONTEXT*)gf_calloc(BIOP_Directory->serviceContextList_count,sizeof(GF_M2TS_DSMCC_SERVICE_CONTEXT));
		dsmcc_biop_get_context(bs,BIOP_Directory->ServiceContext,BIOP_Directory->serviceContextList_count);
	}
	BIOP_Directory->messageBody_length = gf_bs_read_int(bs,32);
	BIOP_Directory->bindings_count = gf_bs_read_int(bs,16);
	BIOP_Directory->Name = (GF_M2TS_DSMCC_BIOP_NAME*)gf_calloc(BIOP_Directory->bindings_count,sizeof(GF_M2TS_DSMCC_BIOP_NAME));

	/* Get the linked files */
	for(i = 0; i<BIOP_Directory->bindings_count; i++) {
		u32 descr_length,nb_desc,j;
		u8* descr_tag;
		GF_Err e;

		BIOP_Directory->Name[i].nameComponents_count = gf_bs_read_int(bs,8);
		BIOP_Directory->Name[i].id_length = gf_bs_read_int(bs,8);
		if(BIOP_Directory->Name[i].id_length) {
			BIOP_Directory->Name[i].id_data = (char*)gf_calloc(BIOP_Directory->Name[i].id_length,sizeof(char));
			gf_bs_read_data(bs,BIOP_Directory->Name[i].id_data,(u32)(BIOP_Directory->Name[i].id_length));
		}
		BIOP_Directory->Name[i].kind_length = gf_bs_read_int(bs,8);
		if(BIOP_Directory->Name[i].kind_length > 0x04) {
			GF_LOG(GF_LOG_INFO, GF_LOG_CONTAINER, ("[Process DSMCC] kind_length value is not valid\n"));
			return GF_CORRUPTED_DATA;
		}
		if(BIOP_Directory->Name[i].kind_length) {
			BIOP_Directory->Name[i].kind_data = (char*)gf_calloc(BIOP_Directory->Name[i].kind_length,sizeof(char));
			gf_bs_read_data(bs,BIOP_Directory->Name[i].kind_data,(u32)(BIOP_Directory->Name[i].kind_length));
		}

		BIOP_Directory->Name[i].BindingType = gf_bs_read_int(bs,8);
		/* IOR */
		e = dsmcc_biop_get_ior(bs,&BIOP_Directory->Name[i].IOR);
		if(e) {
			GF_LOG(GF_LOG_INFO, GF_LOG_CONTAINER, ("[Process DSMCC] Error in IOR processing\n"));
			return GF_CORRUPTED_DATA;
		}

		BIOP_Directory->Name[i].objectInfo_length = gf_bs_read_int(bs,16);
		if(!strcmp(BIOP_Directory->Name[i].kind_data,"fil")) {
			BIOP_Directory->Name[i].ContentSize = gf_bs_read_int(bs,64);
			descr_length = BIOP_Directory->Name[i].objectInfo_length - 8;
		} else {
			descr_length = BIOP_Directory->Name[i].objectInfo_length;
		}

		if(descr_length) {
			BIOP_Directory->Name[i].descriptor = gf_list_new();
			dsmcc_biop_descriptor(bs,BIOP_Directory->Name[i].descriptor,descr_length);
		}

		gf_bs_read_int(bs,(u32)(descr_length));

		nb_desc = gf_list_count(BIOP_Directory->Name[i].descriptor);
		j = 0;
		while(j<nb_desc) {
			/* get the descriptor tag */
			descr_tag = (u8*)gf_list_get(BIOP_Directory->Name[i].descriptor ,0);

			switch(*descr_tag) {
			case CONTENT_TYPE_DESCRIPTOR:
			{
				//GF_M2TS_DSMCC_BIOP_CONTENT_TYPE_DESRIPTOR* ContentTypeDescr = (GF_M2TS_DSMCC_BIOP_CONTENT_TYPE_DESRIPTOR*)gf_list_get(BIOP_Directory->Name[i].descriptor ,j);
			}
			default:
			{
				GF_LOG(GF_LOG_INFO, GF_LOG_CONTAINER, ("[Process DSMCC] Unsupported descriptor Type \n"));
				break;
			}
			}
			j++;
		}

		if(!strcmp(BIOP_Directory->Name[i].kind_data,"dir")) {
			if(!dsmcc_check_element_validation(ServiceGateway->Dir,ServiceGateway->name,BIOP_Directory->Name[i])) {
				GF_Err e;
				GF_M2TS_DSMCC_DIR* Directory;
				GF_M2TS_DSMCC_BIOP_TAGGED_PROFILE* taggedProfile = (GF_M2TS_DSMCC_BIOP_TAGGED_PROFILE*)gf_list_get(BIOP_Directory->Name[i].IOR.taggedProfile,0);
				GF_SAFEALLOC(Directory,GF_M2TS_DSMCC_DIR);
				Directory->name = (char*)gf_strdup(BIOP_Directory->Name[i].id_data);
				Directory->File = gf_list_new();
				Directory->objectKey_data = taggedProfile->BIOPProfileBody->ObjectLocation.objectKey_data;
				Directory->downloadId = taggedProfile->BIOPProfileBody->ObjectLocation.carouselId;
				Directory->moduleId = taggedProfile->BIOPProfileBody->ObjectLocation.moduleId;
				Directory->parent = Dir;
				Directory->Path = dsmcc_get_file_namepath(Dir,Directory->name);
				e = gf_mkdir(Directory->Path);
				if(e) {
					GF_LOG(GF_LOG_INFO, GF_LOG_CONTAINER, ("[Process DSMCC] Error during the creation of the directory %s \n",Directory->Path));
				}
				gf_list_add(ServiceGateway->Dir,Directory);
			}
		} else if(!strcmp(BIOP_Directory->Name[i].kind_data,"fil")) {
			if(!dsmcc_check_element_validation(ServiceGateway->File,ServiceGateway->name,BIOP_Directory->Name[i])) {
				GF_M2TS_DSMCC_FILE* File;
				GF_M2TS_DSMCC_BIOP_TAGGED_PROFILE* taggedProfile = (GF_M2TS_DSMCC_BIOP_TAGGED_PROFILE*)gf_list_get(BIOP_Directory->Name[i].IOR.taggedProfile,0);
				GF_SAFEALLOC(File,GF_M2TS_DSMCC_FILE);
				File->name = (char*)gf_strdup(BIOP_Directory->Name[i].id_data);
				File->objectKey_data = taggedProfile->BIOPProfileBody->ObjectLocation.objectKey_data;
				File->downloadId = taggedProfile->BIOPProfileBody->ObjectLocation.carouselId;
				File->moduleId = taggedProfile->BIOPProfileBody->ObjectLocation.moduleId;
				File->parent = Dir;
				File->Path = dsmcc_get_file_namepath(Dir,File->name);
				gf_list_add(ServiceGateway->File,File);
			}
		}
	}
	dsmcc_free_biop_directory(BIOP_Directory);

	return GF_OK;
}

static GF_Err dsmcc_process_biop_stream_event(GF_BitStream* bs,GF_M2TS_DSMCC_BIOP_HEADER* BIOP_Header,GF_M2TS_DSMCC_SERVICE_GATEWAY* ServiceGateway) {

	u32 i;
	GF_M2TS_DSMCC_BIOP_STREAM_EVENT* BIOP_StreamEvent;
	//GF_M2TS_DSMCC_FILE* File;
	u32 eventdata;

	GF_SAFEALLOC(BIOP_StreamEvent,GF_M2TS_DSMCC_BIOP_STREAM_EVENT);

	eventdata = 0;

	BIOP_StreamEvent->Header = BIOP_Header;
	/* Get Info */
	BIOP_StreamEvent->Info.aDescription_length = gf_bs_read_int(bs,8);
	if(BIOP_StreamEvent->Info.aDescription_length) {
		BIOP_StreamEvent->Info.aDescription_bytes = (char*)gf_calloc(BIOP_StreamEvent->Info.aDescription_length,sizeof(char));
		gf_bs_read_data(bs,BIOP_StreamEvent->Info.aDescription_bytes,(u8)(BIOP_StreamEvent->Info.aDescription_length));
	}
	BIOP_StreamEvent->Info.duration_aSeconds = gf_bs_read_int(bs,32);
	BIOP_StreamEvent->Info.duration_aMicroseconds = gf_bs_read_int(bs,32);
	BIOP_StreamEvent->Info.audio = gf_bs_read_int(bs,8);
	BIOP_StreamEvent->Info.video = gf_bs_read_int(bs,8);
	BIOP_StreamEvent->Info.data = gf_bs_read_int(bs,8);

	/* Event List */
	BIOP_StreamEvent->eventNames_count = gf_bs_read_int(bs,16);
	if(BIOP_StreamEvent->eventNames_count) {
		BIOP_StreamEvent->EventList = (GF_M2TS_DSMCC_BIOP_EVENT_LIST*)gf_calloc(BIOP_StreamEvent->eventNames_count,sizeof(GF_M2TS_DSMCC_BIOP_EVENT_LIST));
		for(i=0; i<BIOP_StreamEvent->eventNames_count; i++) {
			BIOP_StreamEvent->EventList[i].eventName_length = gf_bs_read_int(bs,8);
			eventdata += BIOP_StreamEvent->EventList[i].eventName_length;
			if(BIOP_StreamEvent->EventList[i].eventName_length) {
				BIOP_StreamEvent->EventList[i].eventName_data_byte = (char*)gf_calloc(BIOP_StreamEvent->EventList[i].eventName_length,sizeof(char));
				gf_bs_read_data(bs,BIOP_StreamEvent->EventList[i].eventName_data_byte,(u8)(BIOP_StreamEvent->EventList[i].eventName_length));
			}
		}
	}

	BIOP_StreamEvent->serviceContextList_count = gf_bs_read_int(bs,8);
	if (BIOP_StreamEvent->serviceContextList_count) {
		BIOP_StreamEvent->ServiceContext = (GF_M2TS_DSMCC_SERVICE_CONTEXT*)gf_calloc(BIOP_StreamEvent->serviceContextList_count,sizeof(GF_M2TS_DSMCC_SERVICE_CONTEXT));
		dsmcc_biop_get_context(bs,BIOP_StreamEvent->ServiceContext,BIOP_StreamEvent->serviceContextList_count);
	}

	BIOP_StreamEvent->messageBody_length = gf_bs_read_int(bs,32);
	BIOP_StreamEvent->taps_count = gf_bs_read_int(bs,8);
	BIOP_StreamEvent->Taps = (GF_M2TS_DSMCC_BIOP_TAPS*)gf_calloc(BIOP_StreamEvent->taps_count,sizeof(GF_M2TS_DSMCC_BIOP_TAPS));
	for (i=0; i<BIOP_StreamEvent->taps_count; i++) {
		BIOP_StreamEvent->Taps[i].id = gf_bs_read_int(bs,16);
		BIOP_StreamEvent->Taps[i].use = gf_bs_read_int(bs,16);
		BIOP_StreamEvent->Taps[i].assocTag = gf_bs_read_int(bs,16);
		BIOP_StreamEvent->Taps[i].selector_length = gf_bs_read_int(bs,8);
		if(BIOP_StreamEvent->Taps[i].selector_length != 0x00) {
			GF_LOG(GF_LOG_INFO, GF_LOG_CONTAINER, ("[Process DSMCC] Error in Stream Event : selector_length has a wrong value, abording the processing \n"));
			return GF_CORRUPTED_DATA;
		}
	}
	BIOP_StreamEvent->eventIds_count = gf_bs_read_int(bs,8);
	if(BIOP_StreamEvent->eventIds_count != BIOP_StreamEvent->eventNames_count) {
		GF_LOG(GF_LOG_INFO, GF_LOG_CONTAINER, ("[Process DSMCC] Error in Stream Event : eventIds_count has a wrong value, abording the processing \n"));
		return GF_CORRUPTED_DATA;
	} else if(BIOP_StreamEvent->eventIds_count) {
		BIOP_StreamEvent->eventId = (u16*)gf_calloc(BIOP_StreamEvent->eventIds_count,sizeof(u16));
		for(i =0; i<BIOP_StreamEvent->eventIds_count; i++) {
			BIOP_StreamEvent->eventId[i] = gf_bs_read_int(bs,16);
		}
	}

	dsmcc_free_biop_stream_event(BIOP_StreamEvent);

	return GF_OK;
}

static GF_Err dsmcc_process_biop_stream_message(GF_BitStream* bs,GF_M2TS_DSMCC_BIOP_HEADER* BIOP_Header,GF_M2TS_DSMCC_SERVICE_GATEWAY* ServiceGateway) {

	u32 i;
	GF_M2TS_DSMCC_BIOP_STREAM_MESSAGE* BIOP_StreamMessage;

	GF_SAFEALLOC(BIOP_StreamMessage,GF_M2TS_DSMCC_BIOP_STREAM_MESSAGE);

	BIOP_StreamMessage->Header = BIOP_Header;
	/* Get Info */
	BIOP_StreamMessage->Info.aDescription_length = gf_bs_read_int(bs,8);
	if(BIOP_StreamMessage->Info.aDescription_length) {
		BIOP_StreamMessage->Info.aDescription_bytes = (char*)gf_calloc(BIOP_StreamMessage->Info.aDescription_length,sizeof(char));
		gf_bs_read_data(bs,BIOP_StreamMessage->Info.aDescription_bytes,(u8)(BIOP_StreamMessage->Info.aDescription_length));
	}
	BIOP_StreamMessage->Info.duration_aSeconds = gf_bs_read_int(bs,32);
	BIOP_StreamMessage->Info.duration_aMicroseconds = gf_bs_read_int(bs,32);
	BIOP_StreamMessage->Info.audio = gf_bs_read_int(bs,8);
	BIOP_StreamMessage->Info.video = gf_bs_read_int(bs,8);
	BIOP_StreamMessage->Info.data = gf_bs_read_int(bs,8);

	BIOP_StreamMessage->serviceContextList_count = gf_bs_read_int(bs,8);
	if(BIOP_StreamMessage->serviceContextList_count) {
		BIOP_StreamMessage->ServiceContext = (GF_M2TS_DSMCC_SERVICE_CONTEXT*)gf_calloc(BIOP_StreamMessage->serviceContextList_count,sizeof(GF_M2TS_DSMCC_SERVICE_CONTEXT));
		dsmcc_biop_get_context(bs,BIOP_StreamMessage->ServiceContext,BIOP_StreamMessage->serviceContextList_count);
	}

	BIOP_StreamMessage->messageBody_length = gf_bs_read_int(bs,32);
	BIOP_StreamMessage->taps_count = gf_bs_read_int(bs,8);
	BIOP_StreamMessage->Taps = (GF_M2TS_DSMCC_BIOP_TAPS*)gf_calloc(BIOP_StreamMessage->taps_count,sizeof(GF_M2TS_DSMCC_BIOP_TAPS));
	for(i=0; i<BIOP_StreamMessage->taps_count; i++) {
		BIOP_StreamMessage->Taps[i].id = gf_bs_read_int(bs,16);
		BIOP_StreamMessage->Taps[i].use = gf_bs_read_int(bs,16);
		BIOP_StreamMessage->Taps[i].assocTag = gf_bs_read_int(bs,16);
		BIOP_StreamMessage->Taps[i].selector_length = gf_bs_read_int(bs,8);
		if(BIOP_StreamMessage->Taps[i].selector_length != 0x00) {
			GF_LOG(GF_LOG_INFO, GF_LOG_CONTAINER, ("[Process DSMCC] Error in Stream Event : selector_length has a wrong value, abording the processing \n"));
			return GF_CORRUPTED_DATA;
		}
	}

	dsmcc_free_biop_stream_message(BIOP_StreamMessage);

	return GF_OK;
}


/* Tools */

static GF_M2TS_DSMCC_MODULE* dsmcc_create_module(GF_M2TS_DSMCC_OVERLORD* dsmcc_overlord, GF_M2TS_DSMCC_DOWNLOAD_INFO_RESP_INDIC* DownloadInfoIndication)
{
	u32 module_index;
	GF_M2TS_DSMCC_MODULE* dsmcc_module;
	GF_SAFEALLOC(dsmcc_module,GF_M2TS_DSMCC_MODULE);
	dsmcc_module->downloadId = DownloadInfoIndication->downloadId;
	dsmcc_module->moduleId = DownloadInfoIndication->Modules.moduleId;
	dsmcc_module->size = DownloadInfoIndication->Modules.moduleSize;
	dsmcc_module->version_number = DownloadInfoIndication->Modules.moduleVersion;
	dsmcc_module->block_size = DownloadInfoIndication->blockSize;
	dsmcc_module->buffer = (char*)gf_calloc(dsmcc_module->size,sizeof(char));
	module_index = gf_list_count(dsmcc_overlord->dsmcc_modules);
	dsmcc_overlord->processed[module_index].moduleId = dsmcc_module->moduleId;
	dsmcc_overlord->processed[module_index].downloadId = dsmcc_module->downloadId;
	dsmcc_overlord->processed[module_index].version_number = dsmcc_module->version_number;
	gf_list_add(dsmcc_overlord->dsmcc_modules,dsmcc_module);

	return dsmcc_module;
}

static void dsmcc_biop_descriptor(GF_BitStream* bs,GF_List* list,u32 size) {
	u8 descr_tag;
	u32 data_shift,start_pos;

	start_pos = (u32)(gf_bs_get_position(bs));
	data_shift = 0;

	while(size > data_shift) {
		descr_tag = gf_bs_read_int(bs,8);

		switch(descr_tag) {

		case CACHING_PRIORITY_DESCRIPTOR:
		{
			GF_M2TS_DSMCC_BIOP_CACHING_PRIORITY_DESCRIPTOR* CachingPriorityDescr;
			GF_SAFEALLOC(CachingPriorityDescr,GF_M2TS_DSMCC_BIOP_CACHING_PRIORITY_DESCRIPTOR);
			CachingPriorityDescr->descriptor_tag = descr_tag;
			CachingPriorityDescr->descriptor_length = gf_bs_read_int(bs,8);
			CachingPriorityDescr->priority_value = gf_bs_read_int(bs,8);
			CachingPriorityDescr->transparency_level = gf_bs_read_int(bs,8);
			gf_list_add(list,CachingPriorityDescr);
			break;
		}
		case COMPRESSED_MODULE_DESCRIPTOR:
		{
			GF_M2TS_DSMCC_BIOP_COMPRESSED_MODULE_DESCRIPTOR* CompModuleDescr;
			GF_SAFEALLOC(CompModuleDescr,GF_M2TS_DSMCC_BIOP_COMPRESSED_MODULE_DESCRIPTOR);
			CompModuleDescr->descriptor_tag = descr_tag;
			CompModuleDescr->descriptor_length = gf_bs_read_int(bs,8);
			CompModuleDescr->compression_method = gf_bs_read_int(bs,8);
			/* if CompModuleDescr->compression_method least significant nibble is eq to 0x08, the terminal shall support the Deflate compression algorithm (GZIP) */
			CompModuleDescr->original_size = gf_bs_read_int(bs,32);
			gf_list_add(list,CompModuleDescr);
			break;

		}
		case CONTENT_TYPE_DESCRIPTOR:
		{
			GF_M2TS_DSMCC_BIOP_CONTENT_TYPE_DESRIPTOR* ContentTypeDescr;
			GF_SAFEALLOC(ContentTypeDescr,GF_M2TS_DSMCC_BIOP_CONTENT_TYPE_DESRIPTOR);
			ContentTypeDescr->descriptor_tag = descr_tag;
			ContentTypeDescr->descriptor_length = gf_bs_read_int(bs,8);
			ContentTypeDescr->content_type_data_byte = (char*)gf_calloc(ContentTypeDescr->descriptor_length,sizeof(char));
			gf_bs_read_data(bs,ContentTypeDescr->content_type_data_byte,(u32)(ContentTypeDescr->descriptor_length));
			gf_list_add(list,ContentTypeDescr);
		}
		default:
		{
			u32 size;
			GF_LOG(GF_LOG_INFO, GF_LOG_CONTAINER, ("[Process DSMCC] Unsupported Descriptor Type \n"));
			/* byte shift - in descriptors length is on thr second byte*/
			size = gf_bs_read_int(bs,8);
			gf_bs_read_int(bs,8*size);
			break;
		}
		}
		data_shift = (u32)(gf_bs_get_position(bs)) - start_pos;
	}
}

static void dsmcc_biop_get_context(GF_BitStream* bs,GF_M2TS_DSMCC_SERVICE_CONTEXT* Context,u32 serviceContextList_count)
{
	u32 i;

	for(i=0; i<serviceContextList_count; i++) {
		Context[i].context_id = gf_bs_read_int(bs,32);
		Context[i].context_data_length = gf_bs_read_int(bs,16);
		if(Context[i].context_data_length != 0) {
			Context[i].context_data_byte = (char*)gf_calloc(Context[i].context_data_length ,sizeof(char));
			gf_bs_read_data(bs,Context[i].context_data_byte,(u32)(Context[i].context_data_length ));
		}
	}
}

static GF_Err dsmcc_biop_get_ior(GF_BitStream* bs,GF_M2TS_DSMCC_IOR* IOR)
{
	u32 i,j,left_lite_component;
	/* IOR */
	left_lite_component = 0;
	IOR->type_id_length = gf_bs_read_int(bs,32);
	if(IOR->type_id_length > 0xA) {
		//return GF_CORRUPTED_DATA;
	}
	IOR->type_id_byte = (char*)gf_calloc(IOR->type_id_length,sizeof(char));
	gf_bs_read_data(bs,IOR->type_id_byte,(u32)(IOR->type_id_length));
	IOR->taggedProfiles_count = gf_bs_read_int(bs,32);
	IOR->taggedProfile = gf_list_new();
	for(i = 0; i < IOR->taggedProfiles_count; i++) {
		GF_M2TS_DSMCC_BIOP_TAGGED_PROFILE* taggedProfile;
		GF_SAFEALLOC(taggedProfile,GF_M2TS_DSMCC_BIOP_TAGGED_PROFILE);
		gf_list_add(IOR->taggedProfile,taggedProfile);
		taggedProfile->profileId_tag = gf_bs_read_int(bs,32);
		taggedProfile->profile_data_length = gf_bs_read_int(bs,32);
		taggedProfile->profile_data_byte_order = gf_bs_read_int(bs,8);
		if(taggedProfile->profile_data_byte_order != 0x00) {
			GF_LOG(GF_LOG_INFO, GF_LOG_CONTAINER, ("[Process DSMCC] Error in IOR processing : profile data byte order has a wrong value, abording the processing \n"));
			return GF_CORRUPTED_DATA;
		}
		taggedProfile->lite_component_count = gf_bs_read_int(bs,8);
		left_lite_component = taggedProfile->lite_component_count;
		switch(taggedProfile->profileId_tag) {
		case TAG_BIOP:
		{
			/* Object Location */
			u32 j;
			GF_SAFEALLOC(taggedProfile->BIOPProfileBody,GF_M2TS_DSMCC_BIOP_PROFILE_BODY);
			taggedProfile->BIOPProfileBody->ObjectLocation.componentId_tag = gf_bs_read_int(bs,32);
			if(taggedProfile->BIOPProfileBody->ObjectLocation.componentId_tag != 0x49534F50) {
				GF_LOG(GF_LOG_INFO, GF_LOG_CONTAINER, ("[Process DSMCC] Error in Profile Body : component tag has a wrong value, abording the processing \n"));
				return GF_CORRUPTED_DATA;
			}
			taggedProfile->BIOPProfileBody->ObjectLocation.component_data_length = gf_bs_read_int(bs,8);
			taggedProfile->BIOPProfileBody->ObjectLocation.carouselId = gf_bs_read_int(bs,32);
			taggedProfile->BIOPProfileBody->ObjectLocation.moduleId = gf_bs_read_int(bs,16);
			taggedProfile->BIOPProfileBody->ObjectLocation.version_major = gf_bs_read_int(bs,8);
			if(taggedProfile->BIOPProfileBody->ObjectLocation.version_major != 0x01) {
				GF_LOG(GF_LOG_INFO, GF_LOG_CONTAINER, ("[Process DSMCC] Error in Profile Body : version major has a wrong value, abording the processing \n"));
				return GF_CORRUPTED_DATA;
			}
			taggedProfile->BIOPProfileBody->ObjectLocation.version_minor = gf_bs_read_int(bs,8);
			if(taggedProfile->BIOPProfileBody->ObjectLocation.version_minor != 0x00) {
				GF_LOG(GF_LOG_INFO, GF_LOG_CONTAINER, ("[Process DSMCC] Error in Profile Body : version minor has a wrong value, abording the processing \n"));
				return GF_CORRUPTED_DATA;
			}
			taggedProfile->BIOPProfileBody->ObjectLocation.objectKey_length = gf_bs_read_int(bs,8);
			if(taggedProfile->BIOPProfileBody->ObjectLocation.objectKey_length) {
				taggedProfile->BIOPProfileBody->ObjectLocation.objectKey_data = gf_bs_read_int(bs,taggedProfile->BIOPProfileBody->ObjectLocation.objectKey_length*8);
			}
			/* ConnBinder */

			taggedProfile->BIOPProfileBody->ConnBinder.componentId_tag = gf_bs_read_int(bs,32);
			if(taggedProfile->BIOPProfileBody->ConnBinder.componentId_tag != 0x49534F40) {
				GF_LOG(GF_LOG_INFO, GF_LOG_CONTAINER, ("[Process DSMCC] Error in ConnBinder : componentId tag has a wrong value, abording the processing \n"));
				return GF_CORRUPTED_DATA;
			}
			taggedProfile->BIOPProfileBody->ConnBinder.component_data_length = gf_bs_read_int(bs,8);
			taggedProfile->BIOPProfileBody->ConnBinder.taps_count = gf_bs_read_int(bs,8);
			taggedProfile->BIOPProfileBody->ConnBinder.Taps = (GF_M2TS_DSMCC_BIOP_TAPS*)gf_calloc(taggedProfile->BIOPProfileBody->ConnBinder.taps_count,sizeof(GF_M2TS_DSMCC_BIOP_TAPS));
			for(j=0; j<taggedProfile->BIOPProfileBody->ConnBinder.taps_count; j++) {
				taggedProfile->BIOPProfileBody->ConnBinder.Taps[i].id = gf_bs_read_int(bs,16);
				if(taggedProfile->BIOPProfileBody->ConnBinder.Taps[i].id != 0x00) {
					GF_LOG(GF_LOG_INFO, GF_LOG_CONTAINER, ("[Process DSMCC] Error in ConnBinder : id has a wrong value, abording the processing \n"));
					return GF_CORRUPTED_DATA;
				}
				taggedProfile->BIOPProfileBody->ConnBinder.Taps[i].use = gf_bs_read_int(bs,16);
				if(taggedProfile->BIOPProfileBody->ConnBinder.Taps[i].use != 0x016) {
					GF_LOG(GF_LOG_INFO, GF_LOG_CONTAINER, ("[Process DSMCC] Error in ConnBinder : use has a wrong value, abording the processing \n"));
					return GF_CORRUPTED_DATA;
				}
				taggedProfile->BIOPProfileBody->ConnBinder.Taps[i].assocTag = gf_bs_read_int(bs,16);
				taggedProfile->BIOPProfileBody->ConnBinder.Taps[i].selector_length = gf_bs_read_int(bs,8);
				if(taggedProfile->BIOPProfileBody->ConnBinder.Taps[i].selector_length != 0x0A) {
					GF_LOG(GF_LOG_INFO, GF_LOG_CONTAINER, ("[Process DSMCC] Error in ConnBinder : selector_length has a wrong value, abording the processing \n"));
					return GF_CORRUPTED_DATA;
				}
				taggedProfile->BIOPProfileBody->ConnBinder.Taps[i].selector_type = gf_bs_read_int(bs,16);
				if(taggedProfile->BIOPProfileBody->ConnBinder.Taps[i].selector_type != 0x001) {
					GF_LOG(GF_LOG_INFO, GF_LOG_CONTAINER, ("[Process DSMCC] Error in ConnBinder : selector_type has a wrong value, abording the processing \n"));
					return GF_CORRUPTED_DATA;
				}
				taggedProfile->BIOPProfileBody->ConnBinder.Taps[i].transactionId = gf_bs_read_int(bs,32);
				taggedProfile->BIOPProfileBody->ConnBinder.Taps[i].timeout = gf_bs_read_int(bs,32);
			}
			if(taggedProfile->BIOPProfileBody->ConnBinder.component_data_length-18 != 0) {
				taggedProfile->BIOPProfileBody->ConnBinder.additional_tap_byte = (char*)gf_calloc(taggedProfile->BIOPProfileBody->ConnBinder.component_data_length-18,sizeof(char));
				gf_bs_read_data(bs,taggedProfile->BIOPProfileBody->ConnBinder.additional_tap_byte,(u32)(taggedProfile->BIOPProfileBody->ConnBinder.component_data_length-18));
			}
			left_lite_component = left_lite_component - 2;
			break;
		}
		case TAG_LITE_OPTIONS:
		{
			/* Service Location */
			u32 j;
			GF_SAFEALLOC(taggedProfile->ServiceLocation,GF_M2TS_DSMCC_BIOP_SERVICE_LOCATION);
			taggedProfile->ServiceLocation->componentId_tag = gf_bs_read_int(bs,32);
			if(taggedProfile->ServiceLocation->componentId_tag != 0x49534F46) {
				GF_LOG(GF_LOG_INFO, GF_LOG_CONTAINER, ("[Process DSMCC] Error in Service Location : component tag has a wrong value, abording the processing \n"));
				return GF_CORRUPTED_DATA;
			}
			taggedProfile->ServiceLocation->component_data_length = gf_bs_read_int(bs,8);
			taggedProfile->ServiceLocation->serviceDomain_length = gf_bs_read_int(bs,8);
			if(taggedProfile->ServiceLocation->serviceDomain_length != 0x14) {
				GF_LOG(GF_LOG_INFO, GF_LOG_CONTAINER, ("[Process DSMCC] Error in Service Location : Service Domain Length tag has a wrong value, abording the processing \n"));
				return GF_CORRUPTED_DATA;
			}
			taggedProfile->ServiceLocation->serviceDomain_data.AFI = gf_bs_read_int(bs,8);
			taggedProfile->ServiceLocation->serviceDomain_data.type = gf_bs_read_int(bs,8);
			taggedProfile->ServiceLocation->serviceDomain_data.carouselId = gf_bs_read_int(bs,32);
			taggedProfile->ServiceLocation->serviceDomain_data.specifierType = gf_bs_read_int(bs,8);
			taggedProfile->ServiceLocation->serviceDomain_data.specifierData = gf_bs_read_int(bs,24);
			taggedProfile->ServiceLocation->serviceDomain_data.transport_stream_id = gf_bs_read_int(bs,16);
			taggedProfile->ServiceLocation->serviceDomain_data.original_network_id = gf_bs_read_int(bs,16);
			taggedProfile->ServiceLocation->serviceDomain_data.service_id = gf_bs_read_int(bs,16);
			taggedProfile->ServiceLocation->serviceDomain_data.reserved = gf_bs_read_int(bs,32);
			if(taggedProfile->ServiceLocation->serviceDomain_data.reserved != 0xFFFFFFFF) {
				GF_LOG(GF_LOG_INFO, GF_LOG_CONTAINER, ("[Process DSMCC] Error in Service Domain Data : reserved has a wrong value, abording the processing \n"));
				return GF_CORRUPTED_DATA;
			}
			taggedProfile->ServiceLocation->nameComponents_count = gf_bs_read_int(bs,32);
			taggedProfile->ServiceLocation->NameComponent = (GF_M2TS_DSMCC_BIOP_NAME_COMPONENT*)gf_calloc(taggedProfile->ServiceLocation->nameComponents_count,sizeof(GF_M2TS_DSMCC_BIOP_NAME_COMPONENT));
			for(j = 0; j < taggedProfile->ServiceLocation->nameComponents_count; j++) {
				taggedProfile->ServiceLocation->NameComponent[j].id_length = gf_bs_read_int(bs,32);
				if(taggedProfile->ServiceLocation->NameComponent[j].id_length != 0) {
					taggedProfile->ServiceLocation->NameComponent[j].id_data = (char*)gf_calloc(taggedProfile->ServiceLocation->NameComponent[j].id_length,sizeof(char));
					gf_bs_read_data(bs,taggedProfile->ServiceLocation->NameComponent[i].id_data,(u32)(taggedProfile->ServiceLocation->NameComponent[j].id_length));
				}
				taggedProfile->ServiceLocation->NameComponent[j].kind_length = gf_bs_read_int(bs,32);
				if(taggedProfile->ServiceLocation->NameComponent[j].kind_length != 0) {
					taggedProfile->ServiceLocation->NameComponent[j].kind_data = (char*)gf_calloc(taggedProfile->ServiceLocation->NameComponent[j].kind_length,sizeof(char));
					gf_bs_read_data(bs,taggedProfile->ServiceLocation->NameComponent[j].kind_data,(u32)(taggedProfile->ServiceLocation->NameComponent[j].kind_length));
				}
			}
			taggedProfile->ServiceLocation->initialContext_length = gf_bs_read_int(bs,8);
			if(taggedProfile->ServiceLocation->initialContext_length != 0) {
				taggedProfile->ServiceLocation->InitialContext_data_byte = (char*)gf_calloc(taggedProfile->ServiceLocation->initialContext_length,sizeof(char));
				gf_bs_read_data(bs,taggedProfile->ServiceLocation->InitialContext_data_byte,(u32)(taggedProfile->ServiceLocation->initialContext_length));
			}
			left_lite_component = left_lite_component - 1;
			break;
		}
		default:
		{
			GF_LOG(GF_LOG_INFO, GF_LOG_CONTAINER, ("[Process DSMCC] Unsupported Service gateway profile tag \n"));
			break;
		}
		}
		taggedProfile->LiteComponent = (GF_M2TS_DSMCC_BIOP_LITE_COMPONENT*)gf_calloc(left_lite_component,sizeof(GF_M2TS_DSMCC_BIOP_LITE_COMPONENT));
		for(j = 0; j<left_lite_component ; j++) {
			taggedProfile->LiteComponent[j].componentId_tag = gf_bs_read_int(bs,32);
			taggedProfile->LiteComponent[j].component_data_length = gf_bs_read_int(bs,8);
			if(taggedProfile->LiteComponent[j].component_data_length != 0) {
				taggedProfile->LiteComponent[j].component_data_byte = (char*)gf_calloc(taggedProfile->ServiceLocation->initialContext_length,sizeof(char));
				gf_bs_read_data(bs,taggedProfile->LiteComponent[j].component_data_byte,(u32)(taggedProfile->LiteComponent[j].component_data_length));
			}
		}

	}
	return GF_OK;
}

static GF_Err dsmcc_check_element_validation(GF_List* List,char* Parent_name,GF_M2TS_DSMCC_BIOP_NAME Name)
{
	u32 nb_elt,i;

	GF_M2TS_DSMCC_BIOP_TAGGED_PROFILE* taggedProfile = (GF_M2TS_DSMCC_BIOP_TAGGED_PROFILE*)gf_list_get(Name.IOR.taggedProfile,0);
	nb_elt = gf_list_count(List);

	for(i = 0; i < nb_elt; i++) {
		GF_M2TS_DSMCC_FILE* File = (GF_M2TS_DSMCC_FILE*)gf_list_get(List,i);
		if(!strcmp(File->name,Name.id_data) && !strcmp(File->parent,Parent_name) &&
		        (File->objectKey_data == taggedProfile->BIOPProfileBody->ObjectLocation.objectKey_data) && File->moduleId == taggedProfile->BIOPProfileBody->ObjectLocation.moduleId) {
			/* File or Dir already received */
			return GF_BAD_PARAM;
		}
	}

	return GF_OK;
}

static GF_M2TS_DSMCC_DIR* dsmcc_get_directory(GF_List* List, u32 objectKey_data) {
	u32 nb_elt,i;

	nb_elt = gf_list_count(List);

	for(i = 0; i<nb_elt; i++) {
		GF_M2TS_DSMCC_DIR* TmpDir;
		GF_M2TS_DSMCC_DIR* Dir = (GF_M2TS_DSMCC_DIR*)gf_list_get(List,i);
		if(Dir->objectKey_data == objectKey_data) {
			return Dir;
		}
		if(gf_list_count(Dir->Dir)) {
			TmpDir = dsmcc_get_directory(Dir->Dir,objectKey_data);
			if(TmpDir) {
				return TmpDir;
			}
		}
	}
	return NULL;
}

static GF_M2TS_DSMCC_FILE* dsmcc_get_file(GF_List* ServiceGateway_List, u16 moduleId,u32 downloadId,u32 objectKey_data) {
	u32 nb_elt,i;
	nb_elt = gf_list_count(ServiceGateway_List);
	for(i = 0; i<nb_elt; i++) {
		GF_M2TS_DSMCC_FILE* File = (GF_M2TS_DSMCC_FILE*)gf_list_get(ServiceGateway_List,i);
		if((File->objectKey_data == objectKey_data) && File->moduleId == moduleId && File->downloadId == downloadId) {
			return File;
		}
	}
	return NULL;
}

static char* dsmcc_get_file_namepath(GF_M2TS_DSMCC_DIR* Dir,char* name) {
	char* Path;
	GF_M2TS_DSMCC_DIR* directory = Dir;

	Path = gf_calloc(512,sizeof(char));
	sprintf(Path,"%s%c%s",directory->name,GF_PATH_SEPARATOR,name);
	while(directory->parent != NULL) {
		GF_M2TS_DSMCC_DIR* tempdir;
		char* tempPath = gf_strdup(Path);
		tempdir = (GF_M2TS_DSMCC_DIR*)directory->parent;
		sprintf(Path,"%s%c%s",tempdir->name,GF_PATH_SEPARATOR,tempPath);
		gf_free(tempPath);
		directory = tempdir;
	}
	return Path;
}


/* Free struct */

static void dsmcc_delete_module(GF_M2TS_DSMCC_MODULE* module)
{
	if(module->buffer) {
		gf_free(module->buffer);
	}

	gf_free(module);
}

static void dmscc_delete_file(GF_M2TS_DSMCC_FILE* File) {

	if(File->name) {
		gf_free(File->name);
	}
	if(File->Path) {
		gf_free(File->Path);
	}
}

static void dmscc_delete_dir(GF_M2TS_DSMCC_DIR* Dir) {

	u32 nb_file, nb_dir,i;

	if(Dir->name) {
		gf_free(Dir->name);
	}
	if(Dir->Path) {
		gf_free(Dir->Path);
	}

	nb_file = gf_list_count(Dir->File);
	for(i=0; i<nb_file; i++) {
		GF_M2TS_DSMCC_FILE* File = (GF_M2TS_DSMCC_FILE*)gf_list_get(Dir->File,i);
		dmscc_delete_file(File);
	}
	gf_list_del(Dir->File);

	nb_dir = gf_list_count(Dir->Dir);
	for(i=0; i<nb_dir; i++) {
		GF_M2TS_DSMCC_DIR* Sub_Dir = (GF_M2TS_DSMCC_DIR*)gf_list_get(Dir->Dir,i);
		dmscc_delete_dir(Sub_Dir);
	}
	gf_list_del(Dir->Dir);
	gf_free(Dir);
}

static void dmscc_delete_servicegateway(GF_M2TS_DSMCC_SERVICE_GATEWAY* Servicegateway) {

	u32 nb_file, nb_dir,i;

	if(Servicegateway->name) {
		gf_free(Servicegateway->name);
	}
	nb_file = gf_list_count(Servicegateway->File);
	for(i=0; i<nb_file; i++) {
		GF_M2TS_DSMCC_FILE* File = (GF_M2TS_DSMCC_FILE*)gf_list_get(Servicegateway->File,i);
		dmscc_delete_file(File);
	}
	gf_list_del(Servicegateway->File);

	nb_dir = gf_list_count(Servicegateway->Dir);
	for(i=0; i<nb_dir; i++) {
		GF_M2TS_DSMCC_DIR* Dir = (GF_M2TS_DSMCC_DIR*)gf_list_get(Servicegateway->Dir,i);
		dmscc_delete_dir(Dir);
	}
	gf_list_del(Servicegateway->Dir);
	gf_free(Servicegateway);
}

void gf_m2ts_delete_dsmcc_overlord(GF_M2TS_DSMCC_OVERLORD* dsmcc_overlord)
{
	u16 nb_module,i;

	nb_module = gf_list_count(dsmcc_overlord->dsmcc_modules);
	for(i=0; i<nb_module; i++) {
		GF_M2TS_DSMCC_MODULE* module = (GF_M2TS_DSMCC_MODULE*)gf_list_get(dsmcc_overlord->dsmcc_modules,i);
		dsmcc_delete_module(module);
	}
	gf_list_del(dsmcc_overlord->dsmcc_modules);

	nb_module = gf_list_count(dsmcc_overlord->Unprocessed_module);
	for(i=0; i<nb_module; i++) {
		GF_M2TS_DSMCC_MODULE* module = (GF_M2TS_DSMCC_MODULE*)gf_list_get(dsmcc_overlord->Unprocessed_module,i);
		dsmcc_delete_module(module);
	}
	gf_list_del(dsmcc_overlord->dsmcc_modules);
	dmscc_delete_servicegateway(dsmcc_overlord->ServiceGateway);

	if(dsmcc_overlord->root_dir) {
		gf_free(dsmcc_overlord->root_dir);
	}

	gf_free(dsmcc_overlord);
}

static void dsmcc_free_biop_header(GF_M2TS_DSMCC_BIOP_HEADER* BIOP_Header) {

	gf_free(BIOP_Header->objectKind_data);
	gf_free(BIOP_Header);
}

static void dsmcc_free_biop_directory(GF_M2TS_DSMCC_BIOP_DIRECTORY* BIOP_Directory) {

	gf_free(BIOP_Directory->objectInfo_data);
	dsmcc_free_biop_context(BIOP_Directory->ServiceContext,BIOP_Directory->serviceContextList_count);
	dsmcc_free_biop_name(BIOP_Directory->Name,BIOP_Directory->bindings_count);
	gf_free(BIOP_Directory);
}

static void dsmcc_free_biop_file(GF_M2TS_DSMCC_BIOP_FILE* BIOP_File) {

	dsmcc_free_biop_descriptor(BIOP_File->descriptor);
	dsmcc_free_biop_context(BIOP_File->ServiceContext,BIOP_File->serviceContextList_count);
	if(BIOP_File->content_length) {
		gf_free(BIOP_File->content_byte);
	}
	gf_free(BIOP_File);
}

static void dsmcc_free_biop_stream_event(GF_M2TS_DSMCC_BIOP_STREAM_EVENT* BIOP_StreamEvent) {

	u32 i;

	if(BIOP_StreamEvent->Info.aDescription_length) {
		gf_free(BIOP_StreamEvent->Info.aDescription_bytes);
	}

	if(BIOP_StreamEvent->objectInfo_byte) {
		gf_free(BIOP_StreamEvent->objectInfo_byte);
	}

	if(BIOP_StreamEvent->eventNames_count) {
		for(i=0; i<BIOP_StreamEvent->eventNames_count; i++) {
			if(BIOP_StreamEvent->EventList[i].eventName_length) {
				gf_free(BIOP_StreamEvent->EventList[i].eventName_data_byte);
			}
		}
		gf_free(BIOP_StreamEvent->EventList);
	}

	if(BIOP_StreamEvent->taps_count) {
		gf_free(BIOP_StreamEvent->Taps);
	}

	dsmcc_free_biop_context(BIOP_StreamEvent->ServiceContext,BIOP_StreamEvent->serviceContextList_count);

	if(BIOP_StreamEvent->eventId) {
		gf_free(BIOP_StreamEvent->eventId);
	}
}

static void dsmcc_free_biop_stream_message(GF_M2TS_DSMCC_BIOP_STREAM_MESSAGE* BIOP_StreamMessage) {

	if(BIOP_StreamMessage->Info.aDescription_length) {
		gf_free(BIOP_StreamMessage->Info.aDescription_bytes);
	}

	if(BIOP_StreamMessage->objectInfo_byte) {
		gf_free(BIOP_StreamMessage->objectInfo_byte);
	}

	if(BIOP_StreamMessage->taps_count) {
		gf_free(BIOP_StreamMessage->Taps);
	}

	dsmcc_free_biop_context(BIOP_StreamMessage->ServiceContext,BIOP_StreamMessage->serviceContextList_count);
}

static void dsmcc_free_biop_ior(GF_M2TS_DSMCC_IOR* IOR)
{
	u32 i,left_lite_component;

	if(IOR->type_id_length) {
		gf_free(IOR->type_id_byte);
	}
	for(i = 0; i < IOR->taggedProfiles_count; i++) {
		GF_M2TS_DSMCC_BIOP_TAGGED_PROFILE* taggedProfile = gf_list_get(IOR->taggedProfile,0);
		left_lite_component = taggedProfile->lite_component_count;

		switch(taggedProfile->profileId_tag) {

		case TAG_BIOP:
		{
			/* Object Location */
			GF_SAFEALLOC(taggedProfile->BIOPProfileBody,GF_M2TS_DSMCC_BIOP_PROFILE_BODY);

			gf_free(taggedProfile->BIOPProfileBody->ConnBinder.Taps);
			if(taggedProfile->BIOPProfileBody->ConnBinder.component_data_length-18 != 0) {
				gf_free(taggedProfile->BIOPProfileBody->ConnBinder.additional_tap_byte);
			}
			gf_free(taggedProfile->BIOPProfileBody);
			left_lite_component = left_lite_component - 2;
			break;
		}
		case TAG_LITE_OPTIONS:
		{
			/* Service Location */
			u32 j;

			for(j = 0; j < taggedProfile->ServiceLocation->nameComponents_count; j++) {

				if(taggedProfile->ServiceLocation->NameComponent[j].id_length != 0) {
					gf_free(taggedProfile->ServiceLocation->NameComponent[j].id_data);
				}
				if(taggedProfile->ServiceLocation->NameComponent[j].kind_length != 0) {
					gf_free(taggedProfile->ServiceLocation->NameComponent[j].kind_data);
				}
			}
			gf_free(taggedProfile->ServiceLocation->NameComponent);

			if(taggedProfile->ServiceLocation->initialContext_length != 0) {
				gf_free(taggedProfile->ServiceLocation->InitialContext_data_byte);
			}
			left_lite_component = left_lite_component - 1;
			break;
		}
		}

		for(i = 0; i<left_lite_component ; i++) {
			if(taggedProfile->LiteComponent[i].component_data_length != 0) {
				gf_free(taggedProfile->LiteComponent[i].component_data_byte);
			}

		}
		gf_free(taggedProfile->LiteComponent);
		gf_list_rem(IOR->taggedProfile,0);
		gf_free(taggedProfile);
	}
	gf_list_del(IOR->taggedProfile);
}

static void dsmcc_free_biop_descriptor(GF_List* list)
{
	u8* descr_tag;

	while(gf_list_count(list)) {

		descr_tag = (u8*)gf_list_get(list,0);

		switch(*descr_tag) {

		case CACHING_PRIORITY_DESCRIPTOR:
		{
			GF_M2TS_DSMCC_BIOP_CACHING_PRIORITY_DESCRIPTOR* CachingPriorityDescr = (GF_M2TS_DSMCC_BIOP_CACHING_PRIORITY_DESCRIPTOR*)gf_list_get(list,0);
			gf_list_rem(list,0);
			gf_free(CachingPriorityDescr);
			break;
		}
		case COMPRESSED_MODULE_DESCRIPTOR:
		{
			GF_M2TS_DSMCC_BIOP_COMPRESSED_MODULE_DESCRIPTOR* CompModuleDescr = (GF_M2TS_DSMCC_BIOP_COMPRESSED_MODULE_DESCRIPTOR*)gf_list_get(list,0);
			gf_list_rem(list,0);
			gf_free(CompModuleDescr);
			break;

		}
		case CONTENT_TYPE_DESCRIPTOR:
		{
			GF_M2TS_DSMCC_BIOP_CONTENT_TYPE_DESRIPTOR* ContentTypeDescr = (GF_M2TS_DSMCC_BIOP_CONTENT_TYPE_DESRIPTOR*)gf_list_get(list,0);
			gf_list_rem(list,0);
			if(ContentTypeDescr->descriptor_length) {
				gf_free(ContentTypeDescr->content_type_data_byte);
			}
			gf_free(ContentTypeDescr);
			break;
		}
		default:
		{

			break;
		}
		}

	}
	gf_list_del(list);
}


static void dsmcc_free_biop_name(GF_M2TS_DSMCC_BIOP_NAME* Name, u32 nb_name) {
	u32 i;

	for(i =0; i<nb_name; i++) {

		if(Name[i].id_length) {
			gf_free(Name[i].id_data);
		}
		if(Name[i].kind_length) {
			gf_free(Name[i].kind_data);
		}
		/* IOR */
		dsmcc_free_biop_ior(&Name[i].IOR);
		dsmcc_free_biop_descriptor(Name[i].descriptor);
	}
}

static void dsmcc_free_biop_context(GF_M2TS_DSMCC_SERVICE_CONTEXT* Context,u32 serviceContextList_count) {
	u32 i;

	for(i=0; i<serviceContextList_count; i++) {
		if(Context[i].context_data_length != 0) {
			gf_free(Context[i].context_data_byte);
		}
	}
	gf_free(Context);
}

#endif //GPAC_ENABLE_DSMCC

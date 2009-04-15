#include "RTP_serv_generator.h"

extern GF_Err SampleCallBack(void *calling_object, char *data, u32 size, u64 ts);

PNC_CallbackData * PNC_Init_SceneGenerator(GF_RTPChannel * p_chan, GF_RTPHeader * p_hdr, char * default_scene, u16 socketPort) 
{
	GF_Err e;
	PNC_CallbackData * data = malloc(sizeof(PNC_CallbackData));
	int * i ;

	data->chan = p_chan;
	data->hdr=p_hdr;

	/* Loading the initial scene as the encoding context */
	data->codec = gf_beng_init(data, default_scene);
	if (data->codec) {
		fprintf(stderr, "Cannot create BIFS Engine from %s\n", default_scene);
		free(data);
		return NULL;
	}
	data->socket = gf_sk_new(GF_SOCK_TYPE_UDP); 
	e = gf_sk_bind(data->socket, "localhost", (u16) socketPort, NULL, 0, 0);
	e |= gf_sk_set_block_mode(data->socket,0);
	if (e) {
		fprintf(stderr, "Cannot bind UDP socket to port %d (%s)\n", socketPort, gf_error_to_string(e));
		gf_sk_del(data->socket);
		free(data);
		return NULL;
	}
	data->extension = malloc(sizeof(PNC_CallbackExt));
	((PNC_CallbackExt * )data->extension)->i = 0;
	((PNC_CallbackExt * )data->extension)->lastTS = 0;
	i = & ((PNC_CallbackExt * )data->extension)->i;

	return data;
}

void PNC_SendInitScene(PNC_CallbackData * data){
	  
	data->RAP = 1; 
	data->SAUN_inc = 1;
	gf_beng_encode_context(data->codec, SampleCallBack);
}

void PNC_Close_SceneGenerator(PNC_CallbackData * data) {
	if (data->extension) free(data->extension);
	gf_beng_terminate(data->codec);
	gf_rtp_del(data->chan);
	PNC_ClosePacketizer(data);
	free(data);
}

/* Callback function called when encoding of BT is done */
GF_Err SampleCallBack(void *calling_object, char *au, u32 size, u64 ts)
{
	PNC_CallbackData *data = (PNC_CallbackData *)calling_object;
	/* call the packetizer to create RTP packets */
	PNC_ProcessData(data, au, size, ts); 
	return GF_OK;
}

static int PNC_int_isFinished(char * bsBuffer, u32 bsBufferSize) {
	char * buff = malloc(sizeof(char)*bsBufferSize+5);
	char * sstr;
	int retour=(int)PNC_RET_RTP_STREAM_NOOP;
	
	*buff = 0;
	
	if (! buff)      printf("ERREUR 1 %s %d \n", __FILE__, __LINE__);
	if (! bsBuffer)  printf("ERREUR 2 %s %d \n", __FILE__, __LINE__);
	
	memcpy(buff, bsBuffer, bsBufferSize);
	buff[bsBufferSize]=0;
	if ( *buff != '\0') 
	{
		/* we are looking for the prefix of streaming directives */
		sstr = strstr(buff, "#_RTP_STREAM_"); 		
		if (sstr) {
			sstr+=13; 
			if (strcmp(sstr, PNC_STR_RTP_STREAM_SEND_CRITICAL)==0) retour= (int)PNC_RET_RTP_STREAM_SEND_CRITICAL;
			if (strcmp(sstr, PNC_STR_RTP_STREAM_SEND)==0)          retour= (int)PNC_RET_RTP_STREAM_SEND;
			if (strcmp(sstr, PNC_STR_RTP_STREAM_RAP)==0)           retour= (int)PNC_RET_RTP_STREAM_RAP;
			if (strcmp(sstr, PNC_STR_RTP_STREAM_RAP_RESET)==0)     retour= (int)PNC_RET_RTP_STREAM_RAP_RESET;
			free(buff); 
			return retour;
		}
	}
	free(buff);
	return (int)PNC_RET_RTP_STREAM_NOOP;
}

GF_Err PNC_processBIFSGenerator(PNC_CallbackData * data) {
	unsigned char buffer[66000];
	u32 byteRead=0;
	GF_BitStream * bs; 
	unsigned char *bsBuffer;
	u32 bsSize=0; 
	int retour=0;
	GF_Err e;

	bs = gf_bs_new(NULL, 0,GF_BITSTREAM_WRITE);

	/* store BIFS data until we receive a streaming directive */
	while (!(retour = PNC_int_isFinished((char *) buffer, byteRead))) {
		e = gf_sk_receive(data->socket, buffer, 66000, 0, & byteRead);
		switch (e) {
			case GF_IP_NETWORK_EMPTY:
				gf_bs_del(bs);
				return GF_OK;
			case GF_OK:
	  			break;
			default:
	  			fprintf(stderr, "Socket error while receiving BIFS data %s\n", gf_error_to_string(e));
				gf_bs_del(bs);
				return e;
		}
		gf_bs_write_data(bs, buffer, byteRead);
	}

	/* we need to force a null terminated string */
	gf_bs_write_data(bs, (unsigned char *) "\0", 1);
	gf_bs_get_content(bs,  &bsBuffer, &bsSize); 

	// locking mutex
	gf_mx_p(data->carrousel_mutex);
	fprintf(stdout, "BIFS data received with streaming directive: ");
	switch (retour) {
		case PNC_RET_RTP_STREAM_SEND:
			fprintf(stdout, "RTP STREAM SEND\n");
			e = gf_beng_encode_from_string(data->codec, (char *) bsBuffer, SampleCallBack);
			fprintf(stdout, "beng result %d\n",e);
			break;
	
		case PNC_RET_RTP_STREAM_SEND_CRITICAL:
			fprintf(stdout, "RTP STREAM SEND CRITICAL\n");
			data->SAUN_inc = 1;
			gf_beng_encode_from_string(data->codec, (char *) bsBuffer, SampleCallBack);
			break;
	
		case PNC_RET_RTP_STREAM_RAP:
			fprintf(stdout, "RTP STREAM RAP\n");
			data->RAP = 1;
			data->RAPsent++;
			gf_beng_aggregate_context(data->codec);
			gf_beng_encode_context(data->codec, SampleCallBack);
			break;
		case PNC_RET_RTP_STREAM_RAP_RESET:
			fprintf(stdout, "RTP STREAM RAP\n");
			data->RAP = 1; 
			data->RAPsent++;
			data->SAUN_inc = 1;
			gf_beng_aggregate_context(data->codec);
			gf_beng_encode_context(data->codec, SampleCallBack);
			break;
	}
	// unlocking mutex
	gf_mx_v(data->carrousel_mutex);
	return GF_OK;
}

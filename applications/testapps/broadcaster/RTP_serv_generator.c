#include <assert.h>
#include <string.h>
#include <errno.h>

#include "RTP_serv_generator.h"
#include "debug.h"

/* Callback function called when encoding of BT is done */
GF_Err SampleCallBack(void *calling_object, u16 ESID, char *au, u32 size, u64 ts)
{
	PNC_CallbackData *data = (PNC_CallbackData *)calling_object;
	/* call the packetizer to create RTP packets */
	PNC_ProcessData(data, au, size, ts);
	return GF_OK;
}

GF_Err (*MySampleCallBack)(void *, u16, char *data, u32 size, u64 ts) = &SampleCallBack;

PNC_CallbackData *PNC_Init_SceneGenerator(GF_RTPChannel *p_chan, GF_RTPHeader *p_hdr, char *default_scene,
        u32 socketType, u16 socketPort, int debug)
{
	GF_Err e;
	PNC_CallbackData *data = gf_malloc(sizeof(PNC_CallbackData));
	int *i;
	data->chan = p_chan;
	data->hdr = p_hdr;
	data->debug = debug;
	memset( (void*) (data->buffer), '\0', RECV_BUFFER_SIZE_FOR_COMMANDS);
	data->bufferPosition = 0;
	/* Loading the initial scene as the encoding context */
	data->codec = gf_seng_init((void*)data, default_scene);
	if (!data->codec) {
		fprintf(stderr, "Cannot create BIFS Engine from %s\n", default_scene);
		gf_free(data);
		return NULL;
	}
	data->server_socket = NULL;
	data->socket = NULL;

	if (socketType == GF_SOCK_TYPE_TCP)
	{
		data->server_socket = gf_sk_new(socketType);
		e = gf_sk_bind(data->server_socket, NULL, (u16) socketPort, NULL, 0, 0);
		if (e)
			fprintf(stderr, "Failed to bind : %s\n", gf_error_to_string(e));
		e |= gf_sk_listen(data->server_socket, 1);
		if (e)
			fprintf(stderr, "Failed to listen : %s\n", gf_error_to_string(e));
		e |= gf_sk_set_block_mode(data->server_socket, 0);
		if (e)
			fprintf(stderr, "Failed to set block mode : %s\n", gf_error_to_string(e));
		e |= gf_sk_server_mode(data->server_socket, 0);
		if (e)
			fprintf(stderr, "Failed to set server mode : %s\n", gf_error_to_string(e));
	} else {
		data->socket = gf_sk_new(socketType);
		e = gf_sk_bind(data->socket, NULL, (u16) socketPort, NULL, 0, 0);
	}
	/*
	char buffIp[1024];
	u16 port = 0;
	u32 socket_type = 0;
	e |= gf_sk_get_local_ip(data->socket, buffIp);
	e |= gf_sk_get_local_info(data->socket, &port, &socket_type);
	dprintf(DEBUG_RTP_serv_generator, "RTS_serv_generator %s:%d %s\n",
		buffIp, port, socket_type==GF_SOCK_TYPE_UDP?"UDP":"TCP", e==GF_OK?"OK":"ERROR");
	*/
	if (e) {
		fprintf(stderr, "Cannot bind socket to port %d (%s)\n", socketPort, gf_error_to_string(e));
		if (data->socket)
			gf_sk_del(data->socket);
		if (data->server_socket)
			gf_sk_del(data->server_socket);
		gf_free(data);
		return NULL;
	}
	data->extension = gf_malloc(sizeof(PNC_CallbackExt));
	((PNC_CallbackExt * )data->extension)->i = 0;
	((PNC_CallbackExt * )data->extension)->lastTS = 0;
	i = &((PNC_CallbackExt*)data->extension)->i;
	return data;
}

void PNC_SendInitScene(PNC_CallbackData * data)
{
	data->RAP = 1;
	data->SAUN_inc = 1;
	gf_seng_encode_context(data->codec, MySampleCallBack);
}

void PNC_Close_SceneGenerator(PNC_CallbackData * data)
{
	if (data->extension) gf_free(data->extension);
	gf_seng_terminate(data->codec);
	gf_rtp_del(data->chan);
	PNC_ClosePacketizer(data);
	gf_free(data);
}


/**
 * Finds the command directive if any
 */
static int findCommand(const char * buffer, int searchFrom)
{
	char * sstr;
	assert( buffer );
	assert( searchFrom >= 0);
	/** We may have received #RTP_STREAM_ directive before the last update */
	if (searchFrom < 30) {
		searchFrom = 0;
	} else {
		searchFrom-= 30;
	}
	sstr = strstr(&(buffer[searchFrom]), "#_RTP_STREAM_");
	if (sstr) {
		return (sstr - buffer);
	}
	return -1;
}

static GF_Err processSend(PNC_CallbackData * data, char * bsBuffer)
{
	GF_Err error;
	assert( data );
	assert( bsBuffer );
	assert( data->codec );
	dprintf(DEBUG_RTP_serv_generator, "RTP STREAM SEND\n");
	gf_mx_p(data->carrousel_mutex);
	error = gf_seng_encode_from_string(data->codec, 0, 0, bsBuffer, MySampleCallBack);
	gf_mx_v(data->carrousel_mutex);
	gf_free( bsBuffer );
	return error;
}

static GF_Err processRapReset(PNC_CallbackData * data, char * bsBuffer)
{
	GF_Err error;
	dprintf(DEBUG_RTP_serv_generator, "RTP STREAM RAP RESET\n");
	gf_mx_p(data->carrousel_mutex);
	data->RAP = 1;
	data->RAPsent++;
	data->SAUN_inc = 1;
	error = gf_seng_aggregate_context(data->codec, 0);
	if (error == GF_OK)
		error = gf_seng_encode_context(data->codec, MySampleCallBack);
	gf_mx_v(data->carrousel_mutex);
	gf_free( bsBuffer );
	return error;
}

static GF_Err processRap(PNC_CallbackData * data, char * bsBuffer)
{
	GF_Err error;
	dprintf(DEBUG_RTP_serv_generator, "RTP STREAM RAP\n");
	gf_mx_p(data->carrousel_mutex);
	data->SAUN_inc = 1;
	data->RAP = 1;
	data->RAPsent++;
	error = gf_seng_aggregate_context(data->codec, 0);
	if (GF_OK == error)
		error = gf_seng_encode_context(data->codec, MySampleCallBack);
	gf_mx_v(data->carrousel_mutex);
	gf_free( bsBuffer );
	return error;
}

static GF_Err processSendCritical(PNC_CallbackData * data, char * bsBuffer)
{
	GF_Err error;
	dprintf(DEBUG_RTP_serv_generator, "RTP STREAM SEND CRITICAL\n");
	gf_mx_p(data->carrousel_mutex);
	data->SAUN_inc = 1;
	error = gf_seng_encode_from_string(data->codec, 0, 0, bsBuffer, MySampleCallBack);
	gf_mx_v(data->carrousel_mutex);
	gf_free( bsBuffer );
	return error;
}

/**
 * Allocates a new buffer for output and copy everything in it;
 * then copy off data from newStart to upToPosition.
 */
static char * eat_buffer_to_bs(char * data, int newStart, int upToPosition, int dataFullSize)
{
	char * newBuffer;

	/* Sanity checks */
	assert(data);
	assert(newStart >= 0);
	assert(upToPosition >= 0);
	assert(dataFullSize > 0);
	assert(newStart < upToPosition);
	data[upToPosition] = '\0';
	newBuffer = NULL;

	/*new length + '\0'*/
	assert(dataFullSize >= upToPosition-newStart+2);
	newBuffer = (char*)gf_malloc(dataFullSize);
	memcpy(newBuffer, data, dataFullSize);
	memcpy(data, newBuffer+newStart, upToPosition-newStart+1);
	data[upToPosition-newStart+1]='\0';
	dprintf(DEBUG_RTP_serv_generator, "Generated : '%s'\n", newBuffer);
	return newBuffer;
}

GF_Err PNC_processBIFSGenerator(PNC_CallbackData * data)
{
	const int tmpBufferSize = 2048;
	char *tmpBuffer = (char*)malloc(tmpBufferSize);
	int byteRead=0;

	char *bsBuffer;
	int retour=0;
	GF_Err e;

	if (data->server_socket)
	{
		data->socket = NULL;
		e = gf_sk_accept(data->server_socket, &(data->socket));
		if (e) {
			free(tmpBuffer);
			return GF_OK;
		} else {
			dprintf(DEBUG_RTP_serv_generator, "New TCP client connected !\n");
		}
	}

	do
	{
		if (data->socket == NULL)
			return GF_OK;
		e = gf_sk_receive(data->socket, tmpBuffer, tmpBufferSize, 0, & byteRead);
		switch (e) {
		case GF_IP_NETWORK_EMPTY:
			e = GF_OK;
			break;
		case GF_OK:
			if (byteRead > 0) {
				dprintf(DEBUG_RTP_serv_generator, "Received %d bytes\n", byteRead);
				/* We copy data in buffer */
				memcpy( &(data->buffer[data->bufferPosition]), tmpBuffer, byteRead );
				data->buffer[data->bufferPosition + byteRead] = '\0';
				retour = findCommand( data->buffer, data->bufferPosition);
				data->bufferPosition += byteRead;
				if (retour >= 0) {
					/** OK, it means we found a command ! */
					if (strncmp(&(data->buffer[retour+13]),
					            "SEND_CRITICAL", 13)==0) {
						bsBuffer = eat_buffer_to_bs( data->buffer, retour, retour + 26, RECV_BUFFER_SIZE_FOR_COMMANDS);
						data->bufferPosition = 0;
						return processSendCritical(data, bsBuffer);
					}
					if (strncmp(&(data->buffer[retour+13]), "SEND", 4)==0) {
						bsBuffer = eat_buffer_to_bs( data->buffer, retour, retour + 17, RECV_BUFFER_SIZE_FOR_COMMANDS);
						data->bufferPosition = 0;
						return processSend(data, bsBuffer);
					}
					if (strncmp(&(data->buffer[retour+13]), "RAP", 3)==0) {
						bsBuffer = eat_buffer_to_bs( data->buffer, retour, retour + 16, RECV_BUFFER_SIZE_FOR_COMMANDS);
						data->bufferPosition = 0;
						return processRap(data, bsBuffer);
					}
					if (strncmp(&(data->buffer[retour+13]), "RAP_RESET", 9)==0) {
						bsBuffer = eat_buffer_to_bs( data->buffer, retour, retour + 22, RECV_BUFFER_SIZE_FOR_COMMANDS);
						data->bufferPosition = 0;
						return processRapReset(data, bsBuffer);
					}
					/** If we are here, it means probably we did not received fully the command */
					break;
				}
			}
			/* No bytes were received */
			break;
		default:
			fprintf(stderr, "Socket error while receiving BIFS data %s\n", gf_error_to_string(e));
			if (data->socket != NULL) {
				gf_sk_del(data->socket);
				data->socket = NULL;
			}
			return e;
		}

	} while (e == GF_OK);

	return GF_OK;
}

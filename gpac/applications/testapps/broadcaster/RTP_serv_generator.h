#ifndef _RTP_SERV_GENERATOR_H_
#define _RTP_SERV_GENERATOR_H_
#include <stdlib.h>

#include <gpac/ietf.h>
#include <gpac/network.h> // sockets
#include <gpac/internal/media_dev.h>
#include <gpac/thread.h>
#include <gpac/scene_engine.h>

#include <time.h>
#define RECV_BUFFER_SIZE_FOR_COMMANDS 262144


/*callback type (allows reentrance)*/
typedef struct tmp_PNC_CallbackData {
	GF_RTPChannel *chan;
	GF_RTPHeader *hdr;
	char * formattedPacket;
	int formattedPacketLength;
	GP_RTPPacketizer *rtpBuilder;
	GF_SceneEngine *codec;

	/* socket on which updates are received */
	GF_Socket *socket;
	GF_Socket *server_socket;
	/* socket on which bitrate feedback is sent */
	GF_Socket *feedback_socket;

	void *extension;

	/* indication that the Access Unit is a RAP */
	int RAP;
	/* RAP counter */
	int RAPsent;
	/* indication that the Access Unit Sequence Number should be increased */
	int SAUN_inc;

	GF_Mutex *carrousel_mutex;
	char buffer[RECV_BUFFER_SIZE_FOR_COMMANDS];
	int bufferPosition;
	int debug;
} PNC_CallbackData;



#define RTP_SERV_GENERATOR_DEBUG 0x4

typedef struct tmp_PNC_CallbackExt {
	int i;
	int lastTS;
} PNC_CallbackExt;


/*exports*/
extern GF_Err PNC_RAP(PNC_CallbackData *data);
extern PNC_CallbackData* PNC_Init_SceneGenerator(GF_RTPChannel *p_chan, GF_RTPHeader *p_hdr, char *default_scene,
        u32 socketType, u16 socketPort, int debug);
extern GF_Err PNC_processBIFSGenerator(PNC_CallbackData*);
extern void PNC_Close_SceneGenerator(PNC_CallbackData*);

extern void PNC_SendInitScene(PNC_CallbackData * data);

#include "RTP_serv_packetizer.h"

#endif
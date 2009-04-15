#include <stdlib.h>
#if 0
#include <unistd.h>
#endif 
#include <gpac/ietf.h>
#include <gpac/network.h> // Pour les sockets
#include <gpac/internal/media_dev.h>
#include <gpac/thread.h>

#include "RTP_serv_packetizer.h"
#include <time.h>
#ifndef __RTP_SERV_CLOCK
#define __RTP_SERV_CLOCK


#define  PNC_RET_RTP_STREAM_NOOP 0
#define  PNC_RET_RTP_STREAM_SEND 1
#define  PNC_RET_RTP_STREAM_SEND_CRITICAL 2
#define  PNC_RET_RTP_STREAM_RAP 3
#define  PNC_RET_RTP_STREAM_RAP_RESET 4

#define  PNC_STR_RTP_STREAM_SEND "SEND\n"
#define  PNC_STR_RTP_STREAM_SEND_CRITICAL "SEND_CRITICAL\n"
#define  PNC_STR_RTP_STREAM_RAP "RAP\n"
#define  PNC_STR_RTP_STREAM_RAP_RESET "RAP_RESET\n"

/*Le type passe pour le callback (permet la reentrance)*/
typedef struct tmp_PNC_CallbackData {
  GF_RTPChannel * chan;
  GF_RTPHeader * hdr;
  char * formatedPacket;
  int formatedPacketLength;
  GP_RTPPacketizer *rtpBuilder;
  void * codec;
  
  /* socket on which updates are received */
  GF_Socket *socket; 
  /* socket on which bitrate feedback is sent */
  GF_Socket *feedback_socket;	

  void * extension;

  /* indication that the Access Unit is a RAP */
  int RAP;
  /* RAP counter */
  int RAPsent;
  /* indication that the Access Unit Sequence Number should be increased */
  int SAUN_inc; 

  GF_Mutex *carrousel_mutex;
} PNC_CallbackData;


typedef struct tmp_PNC_CallbackExt {
  int i;
  int lastTS;
} PNC_CallbackExt;


/*Les fonctions exportees*/
extern GF_Err PNC_RAP(PNC_CallbackData * data);
extern PNC_CallbackData*  PNC_Init_SceneGenerator(GF_RTPChannel * p_chan, GF_RTPHeader * p_hdr, char * default_scene, u16 socketPort);
extern GF_Err PNC_processBIFSGenerator(PNC_CallbackData*);
extern void PNC_Close_SceneGenerator(PNC_CallbackData*);

extern void PNC_SendInitScene(PNC_CallbackData * data);
  

#endif

#ifndef __RTP_SERV_SENDER
#define __RTP_SERV_SENDER

#include <gpac/ietf.h> /// For GF_Err ...
#include "RTP_serv_generator.h"

extern void test_RTP_serv_send();

extern GF_Err PNC_InitRTP(GF_RTPChannel **chan, char *dest, int port, unsigned short mtu_size);
extern GF_Err PNC_SendRTP(PNC_CallbackData *data, char *payload, int payloadSize);
extern GF_Err PNC_CloseRTP(GF_RTPChannel *chan);


#endif

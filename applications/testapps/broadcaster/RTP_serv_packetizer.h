#ifndef __RTP_SERV_PACKETISER
#define __RTP_SERV_PACKETISER


#include <gpac/ietf.h>
#include "gpac/bifsengine.h" // Pour M4Sample
#include "RTP_serv_generator.h"

/*Les fonctions exportees*/
void PNC_InitPacketiser(PNC_CallbackData * data, char *sdp_fmt);
GF_Err PNC_ProcessData(PNC_CallbackData * data, char *au, u32 size, u64 ts);
void PNC_ClosePacketizer(PNC_CallbackData *data);


#endif

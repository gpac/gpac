#ifndef _BROADCASTER_DEBUG_H
#define _BROADCASTER_DEBUG_H

typedef enum _debugMode {
	DEBUG_broadcaster = 1,
	DEBUG_RTP_serv_generator = 2,
	DEBUG_RTP_serv_packetizer =4,
	DEBUG_RTP_serv_sender = 8,
	DEBUG_sdp_generator = 16
} debugMode;

void setDebugMode(int mode);

int getDebugMode();

void dprintf(debugMode debug, const char *msg, ...);

#endif

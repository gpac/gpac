#include "RTP_serv_sender.h"
#include <gpac/internal/ietf_dev.h>
#include <gpac/ietf.h>


GF_Err PNC_InitRTP(GF_RTPChannel **chan, char * dest, int port){
	GF_Err res;
	GF_RTSPTransport tr;

	*chan = gf_rtp_new();
	printf("[carrousel] : RTP_SetupPorts=%d\n", gf_rtp_set_ports(*chan, 0));  

	tr.destination = dest;
	tr.IsUnicast = gf_sk_is_multicast_address(dest) ? 0 : 1;
	tr.Profile="RTP/AVP";//RTSP_PROFILE_RTP_AVP;
	tr.IsRecord = 0;
	tr.Append = 0;
	tr.source = "0.0.0.0";
	tr.SSRC=rand();

	tr.port_first        = port;
	tr.port_last         = port+1;
	if (tr.IsUnicast) {
		tr.client_port_first = port;
		tr.client_port_last  = port+1;
	} else {
		tr.source = dest;
	}
	res = gf_rtp_setup_transport(*chan, &tr, dest);
	printf("[carrousel] : RTP_SetupTransport=%d\n", res);
	if (res !=0) return res;

	res = gf_rtp_initialize(*chan, 0, 1, 1500, 0, 0, NULL);
	printf("[carrousel] : RTP_Initialize=%d\n", res);
	if (res !=0) return res;
	return GF_OK;
}


GF_Err PNC_SendRTP(PNC_CallbackData * data, char * payload, int payloadSize){
  	GF_Err e;
	unsigned char feedback_buffer[250];	// buffer pour envoyer le nombre de byte envoyé
  
	if (!data->hdr->TimeStamp) 
		data->hdr->TimeStamp = ((PNC_CallbackExt * )data->extension)->lastTS;
  	
	((PNC_CallbackExt * )data->extension)->lastTS = data->hdr->TimeStamp;
	
	e = gf_rtp_send_packet(data->chan, data->hdr, 0, 0, payload, payloadSize);

  	fprintf(stdout, " SendPacket : %d, TimeStamp RTP = %d\n", e, data->hdr->TimeStamp);
	
	// sending feedback bytes
	memset(feedback_buffer, 0, sizeof(feedback_buffer));
	sprintf((char *) feedback_buffer, "DataSent=%d\nRAPsent=%d\n", payloadSize, data->RAPsent);
	// fprintf(stdout, "DataSent=%d\nRAPsent=%d\n", payloadSize, data->RAPsent);
	e = gf_sk_send(data->feedback_socket, feedback_buffer, strlen((char *) feedback_buffer));
  	fprintf(stdout, "[carrousel] : sent feedback data %d byte, return %d\n", payloadSize, e);

  	return GF_OK;
}

GF_Err PNC_CloseRTP(GF_RTPChannel *chan){ 
	gf_rtp_del(chan);
	return GF_OK;
}

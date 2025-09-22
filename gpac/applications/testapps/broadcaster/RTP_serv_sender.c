#include "RTP_serv_sender.h"
#include <gpac/internal/ietf_dev.h>
#include <gpac/ietf.h>
#include "debug.h"


GF_Err PNC_InitRTP(GF_RTPChannel **chan, char *dest, int port, unsigned short mtu_size)
{
	GF_Err res;
	GF_RTSPTransport tr;

	*chan = gf_rtp_new();
	res = gf_rtp_set_ports(*chan, 0);
	if (res) {
		fprintf(stderr, "Cannot set RTP ports: %s\n", gf_error_to_string(res));
		gf_rtp_del(*chan);
		return res;
	}

	tr.destination = dest;
	tr.IsUnicast = gf_sk_is_multicast_address(dest) ? 0 : 1;
	tr.Profile="RTP/AVP";//RTSP_PROFILE_RTP_AVP;
	tr.IsRecord = 0;
	tr.Append = 0;
	tr.source = "0.0.0.0";
	tr.SSRC=rand();

	tr.port_first		= port;
	tr.port_last		 = port+1;
	if (tr.IsUnicast) {
		tr.client_port_first = port;
		tr.client_port_last = port+1;
	} else {
		tr.source = dest;
		tr.client_port_first = 0;
		tr.client_port_last  = 0;
	}

	res = gf_rtp_setup_transport(*chan, &tr, dest);
	if (res) {
		fprintf(stderr, "Cannot setup RTP transport %s\n", gf_error_to_string(res));
		gf_rtp_del(*chan);
		return res;
	}

	res = gf_rtp_initialize(*chan, 0, 1, mtu_size, 0, 0, NULL);
	if (res) {
		fprintf(stderr, "Cannot initialize RTP transport %s\n", gf_error_to_string(res));
		gf_rtp_del(*chan);
		return res;
	}
	return GF_OK;
}


GF_Err PNC_SendRTP(PNC_CallbackData *data, char *payload, int payloadSize)
{
	GF_Err e;
	unsigned char feedback_buffer[250];

	if (!data->hdr->TimeStamp)
		data->hdr->TimeStamp = ((PNC_CallbackExt * )data->extension)->lastTS;

	((PNC_CallbackExt * )data->extension)->lastTS = data->hdr->TimeStamp;

	e = gf_rtp_send_packet(data->chan, data->hdr, payload, payloadSize, 0);
	dprintf(DEBUG_RTP_serv_sender, "SendPacket : %d, TimeStamp RTP = %d, sz= %d\n",
	        e, data->hdr->TimeStamp, payloadSize);

	// sending feedback bytes
	memset(feedback_buffer, 0, sizeof(feedback_buffer));
	sprintf((char *) feedback_buffer, "DataSent=%d\nRAPsent=%d\n", payloadSize, data->RAPsent);
	e = gf_sk_send(data->feedback_socket, feedback_buffer, strlen((char *) feedback_buffer));
	dprintf(DEBUG_RTP_serv_packetizer, "Sent feedback data %d byte, return %d\n", payloadSize, e);

	return GF_OK;
}

GF_Err PNC_CloseRTP(GF_RTPChannel *chan)
{
	gf_rtp_del(chan);
	return GF_OK;
}

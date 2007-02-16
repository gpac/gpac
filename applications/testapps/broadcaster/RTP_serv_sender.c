#include "RTP_serv_sender.h"
#include <gpac/internal/ietf_dev.h>
#include <gpac/ietf.h>

GF_Err my_RTP_Initialize(GF_RTPChannel *ch, u32 UDPBufferSize, Bool IsSource, u32 PathMTU, u32 ReorederingSize, u32 MaxReorderDelay)
{
  GF_Err e;

  if (IsSource && !PathMTU) return GF_BAD_PARAM;

  if (ch->rtp) gf_sk_del(ch->rtp);
  if (ch->rtcp) gf_sk_del(ch->rtcp);
  if (ch->po) gf_rtp_reorderer_del(ch->po);

  ch->CurrentTime = 0;
  ch->rtp_time = 0;

  //create sockets for RTP/AVP profile only
  if (ch->net_info.Profile &&
      ( !stricmp(ch->net_info.Profile, GF_RTSP_PROFILE_RTP_AVP) || !stricmp(ch->net_info.Profile, "RTP/AVP/UDP"))
      ) {
    //destination MUST be specified for unicast
    if (IsSource && ch->net_info.IsUnicast && !ch->net_info.destination) return GF_BAD_PARAM;

    //
    //    RTP
    //
    ch->rtp = gf_sk_new(GF_SOCK_TYPE_UDP);
    if (!ch->rtp) return GF_IP_NETWORK_FAILURE;
    if (ch->net_info.IsUnicast) {
      //if client, bind and connect the socket

      if (!IsSource) {

	e = gf_sk_bind(ch->rtp, ch->net_info.client_port_first, ch->net_info.source, ch->net_info.port_first, GF_SOCK_REUSE_PORT);
	if (e) return e;
      }
      //else bind and set remote destination
      else {

	e = gf_sk_bind(ch->rtp, ch->net_info.port_first, ch->net_info.destination, ch->net_info.client_port_first, GF_SOCK_REUSE_PORT);
	if (e) return e;
      }
    } else {
      //Bind to multicast (auto-join the group).
      //we do not bind the socket if this is a source-only channel because some servers
      //don't like that on local loop ...
      e = gf_sk_setup_multicast(ch->rtp, ch->net_info.source, ch->net_info.port_first, ch->net_info.TTL, (IsSource==2), NULL);
      if (e) return e;

      //destination is used for multicast interface addressing - TO DO

    }

    if (UDPBufferSize) gf_sk_set_buffer_size(ch->rtp, IsSource, UDPBufferSize);

    if (IsSource) {
      if (ch->send_buffer) free(ch->send_buffer);
      ch->send_buffer = malloc(sizeof(char) * PathMTU);
      ch->send_buffer_size = PathMTU;

    }


    //Create re-ordering queue for UDP only, and recieve
    if (ReorederingSize && !IsSource) {
      if (!MaxReorderDelay) MaxReorderDelay = 200;
      ch->po = gf_rtp_reorderer_new(ReorederingSize, MaxReorderDelay);

    }

    //
    //      RTCP
    //
    ch->rtcp = gf_sk_new(GF_SOCK_TYPE_UDP);
    if (!ch->rtcp) return GF_IP_NETWORK_FAILURE;
    if (ch->net_info.IsUnicast) {
      if (!IsSource) {

	e = gf_sk_bind(ch->rtcp, ch->net_info.client_port_last, ch->net_info.source, ch->net_info.port_last, GF_SOCK_REUSE_PORT);
	if (e) return e;
      } else {
	e = gf_sk_bind(ch->rtcp, ch->net_info.port_last, ch->net_info.destination, ch->net_info.client_port_last, GF_SOCK_REUSE_PORT); /// PATCH
	if (e) return e;
      }
    } else {
      //Bind to multicast (auto-join the group)
      e = gf_sk_setup_multicast(ch->rtcp, ch->net_info.source, ch->net_info.port_last, ch->net_info.TTL, (IsSource==2), NULL);
      if (e) return e;
      //destination is used for multicast interface addressing - TO DO
    }
  }

  //format CNAME if not done yet
  if (!ch->CName) {
    //this is the real CName setup
    if (!ch->rtp) {
      ch->CName = strdup("mpeg4rtp");
    } else {
      char name[GF_MAX_IP_NAME_LEN];
      s32 start;
      //gf_get_user_name(name, 1024);
	  strcpy(name, "gpac");
      if (strlen(name)) strcat(name, "@");
      start = strlen(name);
      //get host IP or loopback if error
      if (gf_sk_get_local_ip(ch->rtp, name+start) != GF_OK) strcpy(name+start, "127.0.0.1");
      ch->CName = strdup(name);
    }
  }

  return GF_OK;
}

GF_Err PNC_InitRTP(GF_RTPChannel **chan, char * dest, int port){
	GF_Err res;
	GF_RTSPTransport tr;

	*chan = gf_rtp_new();
	printf("[carrousel] : RTP_SetupPorts=%d\n", gf_rtp_set_ports(*chan));  

	tr.IsUnicast=1;
	tr.Profile="RTP/AVP";//RTSP_PROFILE_RTP_AVP;
	tr.destination = dest;
	tr.source = "0.0.0.0";
	tr.IsRecord = 0;
	tr.Append = 0;
	tr.SSRC=rand();

	tr.client_port_first = port;   //RTP
	tr.client_port_last  = port+1; //RTCP (not used a priori)
	tr.port_first        = port; //RTP other end 
	tr.port_last         = port+1; //RTCP other end (not used a priori)

	res = gf_rtp_setup_transport(*chan, &tr, dest);
	printf("[carrousel] : RTP_SetupTransport=%d\n", res);
	if (res !=0) return res;

	res = gf_rtp_initialize(*chan, 0, 1, 1500, 0, 0, NULL);
	//res = my_RTP_Initialize(*chan, 0, 1, 1500, 0, 0);
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

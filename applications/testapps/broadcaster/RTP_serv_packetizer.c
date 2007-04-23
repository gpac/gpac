/*
RTP Server
Projet MIX

Generation des paquets RTP
*/


/* Ici on va prendre un buffer et le préparer pour l'envoyer, cad le passer
   a RTPBuilder pour qu'il fasse les entetes, qu'il splitte
   Ensuite on concatene l'entete avec le contenu et on envoie.

*/



#include "RTP_serv_packetizer.h"
#include "RTP_serv_sender.h"

#include <gpac/ietf.h>
#include <gpac/mpeg4_odf.h>
#include <gpac/internal/media_dev.h>


void  OnNewPacket(void *cbk, GF_RTPHeader *header){
  // 

  // Ici on remet le packet courant à vide.
  ((PNC_CallbackData *)cbk)->formatedPacketLength = 0; 

}
void  OnPacketDone(void *cbk, GF_RTPHeader *header) {
  // 
  printf("OK Paquet généré  ->>>");
  //Ici on peut proceder à l'envoi du packet courant.
  

  PNC_SendRTP(((PNC_CallbackData *)cbk),  ((PNC_CallbackData *)cbk)->formatedPacket,  ((PNC_CallbackData *)cbk)->formatedPacketLength);

  ((PNC_CallbackData *)cbk)->formatedPacketLength = 0; 
  
}

void OnData(void *cbk, char *data, u32 data_size, Bool is_head){
  
  memcpy( ((PNC_CallbackData *)cbk)->formatedPacket+((PNC_CallbackData *)cbk)->formatedPacketLength, data, data_size);
  ((PNC_CallbackData *)cbk)->formatedPacketLength+=data_size;
  
  // Ici on concatene ce qu'on vient de recevoir au packet courant.
  
}

void PNC_InitPacketiser(PNC_CallbackData * data, char *sdp_fmt){

  // char sdp_fmt[5000];
  GP_RTPPacketizer *p;
  GF_SLConfig sl;
  memset(&sl, 0, sizeof(sl));
  sl.useTimestampsFlag = 1;
  sl.useRandomAccessPointFlag = 1;
  sl.timestampResolution = 1000;
  sl.AUSeqNumLength = 16; // Peut etre descendu a 7 pour gagner 1 octet ...
  // PIPOCANAJA
  

  p = gf_rtp_builder_new(GF_RTP_PAYT_MPEG4,
				   &sl,
				   GP_RTP_PCK_SIGNAL_RAP | GP_RTP_PCK_SIGNAL_AU_IDX,
				   data,
				   OnNewPacket,
				   OnPacketDone,
				   NULL,
				   OnData);
  
  //  M4RTP_InitBuilder(p, 96, 1488, 0, 3, 1, 1, 0, 0, 0, 0, 0, 0, NULL);
  gf_rtp_builder_init(p, 96, 1470, 0, 3, 1, 1, 0, 0, 0, 0, 0, 0, NULL);
 
  gf_rtp_builder_format_sdp(p, "mpeg4-generic", sdp_fmt, NULL, 0);

  // fprintf(stdout, "%s\n", sdp_fmt);
  p->rtp_header.Version=2;
  p->rtp_header.SSRC=rand();//RandomNumber identifiant la source
  
  data->hdr=& p->rtp_header;
  data->rtpBuilder=p;
  data->formatedPacket = malloc(2000); //buffer pour les paquets
  data->formatedPacketLength = 0;
}

void PNC_ClosePacketizer(PNC_CallbackData *data)
{
	free(data->formatedPacket);
	gf_rtp_builder_del(data->rtpBuilder);
}

GF_Err PNC_ProcessData(PNC_CallbackData * data, char *au, u32 size, u64 ts) {
  /* Prepare le paquet en mettant à jour le hdr qui contiendra le numero de sequence courant,
     le timestamp correct ...*/
  /*PacketID represente le numero du paquet dans le cas d'une fragmentation en plusieurs paquet RTP
    d'une data*/
  
  // ici on met le timestamp pour que les modifications arrivent bien a etre affiches
  data->hdr->TimeStamp = (u32) gf_sys_clock(); // ts; 
  
  //Dans samp->IsRAP
  //     /*Random Access Point flag - 1 is regular RAP (read/write) , 2 is SyncShadow (read mode only)*/
  //     u8 IsRAP;
  
  if (! data->rtpBuilder) {
    printf(" data->rtpBuilder Null\n\n");exit(-1);}
  
  // ici on met le timestamp pour que les modifications arrivent bien a etre affiches
  data->rtpBuilder->sl_header.compositionTimeStamp = (u32) gf_sys_clock(); // ts;

   data->rtpBuilder->sl_header.randomAccessPointFlag = data->RAP;
  
  if (data->SAUN_inc)  // Si on a demandé d'incrémenter, on incrémente :)
    data->rtpBuilder->sl_header.AU_sequenceNumber ++ ;
  
  data->RAP=0;
  data->SAUN_inc=0;

  data->rtpBuilder->sl_header.paddingBits = 0;


  gf_rtp_builder_process(data->rtpBuilder, au, size, 1, size, 0, 0);

#if 0

  while (remaining_length >  RTP_packet_overflow) {
    data->hdr->SequenceNumber++;
    
    PNC_SendRTP(data, current_data,  RTP_packet_overflow);
    
    current_data     +=  RTP_packet_overflow;
    remaining_length -=  RTP_packet_overflow;
  }

#endif
  


  //  data->hdr->SequenceNumber++;
  //PNC_SendRTP(data, current_data, remaining_length);
  
  return GF_OK;
}

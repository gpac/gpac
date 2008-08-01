/*
	RTP Server
	Projet MIX
	Module de generation de l'horloge
*/

#include "RTP_serv_generator.h"

/*Inutile car plus de variables globales*/
#define VERIFY_INITIALIZED if (!initialized) { printf("Error : SampleCallBack Called before initialization\n"); exit(255);}
int initialized = 0; // Il n'y a plus de variables globales donc ce flag est inutile.

char in_context[100];
char update[1000];// = "REPLACE M.emissiveColor BY 1 1 0";
char timed_update[1000];

extern GF_Err SampleCallBack(void *calling_object, char *data, u32 size, u64 ts);

PNC_CallbackData * PNC_Init_SceneGenerator(GF_RTPChannel * p_chan, GF_RTPHeader * p_hdr, char * default_scene, int socketPort) {
	PNC_CallbackData * data = malloc(sizeof(PNC_CallbackData));
	  int * i ;
	  
	  data->chan = p_chan;
	  data->hdr=p_hdr;
	  
	  /*Charger la scene initiale == le contexte*/
	  data->codec= gf_beng_init(data, default_scene);
	  fprintf(stdout, "[carrousel] : command socket: listening on port %d\n", socketPort);
	  data->socket = gf_sk_new(GF_SOCK_TYPE_UDP); // on cree le socket UDP de commande
	  fprintf(stdout, "[carrousel] : socket bind: %d\n", gf_sk_bind(data->socket, NULL, socketPort, NULL, 0, 0));
	  // fprintf(stdout, "[carrousel] : socket connect: %d\n", gf_sk_connect(data->socket, "127.0.0.1", socketPort, NULL));
	  // fprintf(stdout, "[carrousel] : socket listen: %d\n", gf_sk_listen(data->socket, 1)); 
	  fprintf(stdout, "[carrousel] : gf_sk_set_block_mode: %d\n", gf_sk_set_block_mode(data->socket,0)); 
	
	  data->extension = malloc(sizeof(PNC_CallbackExt));
	  ((PNC_CallbackExt * )data->extension)->i = 0;
	  ((PNC_CallbackExt * )data->extension)->lastTS = 0;
	  i = & ((PNC_CallbackExt * )data->extension)->i;
	  
	  initialized = 1; // ici on est initialisé (flag)
	
	  return data;
}

void PNC_SendInitScene(PNC_CallbackData * data){
	  
	// on peut gerer l'update de la scene par defaut.	
	/// TODO: Il va falloir gerer le mecanisme Carousel
	data->RAP=1; //On demande que RAP soit positionné dans le SL
	data->SAUN_inc=1; //On demande que SAUN soit incrementé car 
	//on ne pourra pas faire les prochaines mises a jour sans ...
	fprintf(stdout, "[gpaclib] : ");
	gf_beng_encode_context(data->codec, SampleCallBack);
}

void PNC_Close_SceneGenerator(PNC_CallbackData * data) {
	// terminer
	if (data->extension) free(data->extension);
	gf_beng_terminate(data->codec);
	gf_rtp_del(data->chan);
	PNC_ClosePacketizer(data);
	free(data);
}

/* fonction callcak appellee quand la compilation du BT est faite */
GF_Err SampleCallBack(void *calling_object, char *au, u32 size, u64 ts)
{
	PNC_CallbackData * data ;
	VERIFY_INITIALIZED;

	data = (PNC_CallbackData *) calling_object;
	
	///*the sample object*/
	////typedef struct tagM4Sample
	//{
	//      /*data size*/
	//        u32 dataLength;
	//        /*data with padding if requested*/
	//       char *data;
	//      /*decoding time*/
	//     u32 DTS;
	//     /*relative offset for composition if needed*/
	//     u32 CTS_Offset;
	//     /*Random Access Point flag - 1 is regular RAP (read/write) , 2 is SyncShadow (read mode only)*/
	//     u8 IsRAP;
	//} M4Sample;
	PNC_ProcessData(data, au, size, ts); // on passe la main au packetizer ...
	
	return GF_OK;
}

GF_Err PNC_RAP(PNC_CallbackData * data) {
	/// envoyer le RAP initial;
	data->RAP = 1;
	gf_beng_aggregate_context(data->codec);
	gf_beng_encode_context(data->codec, SampleCallBack);
	data->RAP = 0;
	return GF_OK;
}

int PNC_int_isFinished(char * bsBuffer, u32 bsBufferSize) {
	/* Fonction interne, sorte de parseur de directives
	* Non exportée dans le .h   */
	char * buff = malloc(sizeof(char)*bsBufferSize+5);
	char * sstr;
	int retour=(int)PNC_RET_RTP_STREAM_NOOP;
	
	*buff = 0;
	
	if (! buff)      printf("ERREUR 1 %s %d \n", __FILE__, __LINE__);
	if (! bsBuffer)  printf("ERREUR 2 %s %d \n", __FILE__, __LINE__);
	
	memcpy(buff, bsBuffer, bsBufferSize);
	buff[bsBufferSize]=0;
	if ( *buff != '\0') 
	{
		sstr = strstr(buff, "#_RTP_STREAM_"); // On cherche le prefixe des directives ...
		
		if (sstr) 
		{
			//On en tient une ;)
			sstr+=13; //On mange le prefixe
			//      printf("strStr $%s$   $%s$ %d\n",sstr ,PNC_STR_RTP_STREAM_RAP, strcmp(sstr, PNC_STR_RTP_STREAM_RAP) );
			if (strcmp(sstr, PNC_STR_RTP_STREAM_SEND_CRITICAL)==0) retour= (int)PNC_RET_RTP_STREAM_SEND_CRITICAL;
			
			if (strcmp(sstr, PNC_STR_RTP_STREAM_SEND)==0)          retour= (int)PNC_RET_RTP_STREAM_SEND;
			if (strcmp(sstr, PNC_STR_RTP_STREAM_RAP)==0)           retour= (int)PNC_RET_RTP_STREAM_RAP;
			if (strcmp(sstr, PNC_STR_RTP_STREAM_RAP_RESET)==0)     retour= (int)PNC_RET_RTP_STREAM_RAP_RESET;
			
			free(buff); return retour;
		}
	}
	free(buff);
	return (int)PNC_RET_RTP_STREAM_NOOP;
}

GF_Err PNC_processBIFSGenerator(PNC_CallbackData * data) {
	unsigned char buffer[66000];
	u32 byteRead=0;
	GF_BitStream * bs = gf_bs_new(NULL, 0,GF_BITSTREAM_WRITE); // dernier champ = mode ?!? no doc
	unsigned char *bsBuffer;
	// char bsCommande[66000];
	// struct timespec heure;
	u32 bsSize=0; 
	int retour=0;
	GF_Err e;
	/* ici, nous allons attendre (bloquant) sur le socket, et quand on recoit
	* des données, on les range. quand on recoit un ordre, on l'execute sur les données.
	*/
	
	while (!(retour = PNC_int_isFinished((char *) buffer, byteRead))) 
	{
		 e = gf_sk_receive(data->socket, buffer, 66000, 0, & byteRead);
		/* le receive semble pas bloquant alors que l'API l'annonce */
		switch (e) 
		{
			case GF_IP_NETWORK_EMPTY:
				gf_bs_del(bs);
				return GF_OK;
			case GF_OK:
	  			break;
			default:
	  			fprintf(stdout, "[carrousel] : erreur de socket : %d\n", e);
				gf_bs_del(bs);
				return e;
		}
		// if (byteRead > 0) printf("Octets recus: %d \n", byteRead);
		gf_bs_write_data(bs, buffer, byteRead);
	}

	gf_bs_write_data(bs, (unsigned char *) "\0", 1); // on met le terminateur de chaine
	gf_bs_get_content(bs,  &bsBuffer, &bsSize); 
	//update=malloc(sizeof(char) * (bsSize + 20));
	//trash = clock_gettime(CLOCK_REALTIME, &heure);
	//offset = heure.tv_sec*1000 + heure.tv_nsec/1000/1000;
	//sprintf(update, "AT %d {%s}", offset, bsBuffer);
	//printf("%f\n",(float)offset/1000.0);
	/* les données sont pretes*/
	
	// mutex
	while(gf_mx_try_lock(data->carrousel_mutex) == 0)
	{ 
		gf_sleep(1); 
	}
	
	fprintf(stdout, "[carrousel] : received -> ");
	switch (retour) 
	{
		case PNC_RET_RTP_STREAM_SEND:
			fprintf(stdout, "RTP STREAM SEND\n");
			/* on encode la chaine, qui est bien NULL Terminated */
			// sprintf(bsCommande, "AT 1{%s}", bsBuffer);
			// fprintf(stdout, "COMMANDE : %s\n", bsCommande);
			fprintf(stdout, "[gpaclib] : ");
			e = gf_beng_encode_from_string(data->codec, (char *) bsBuffer, SampleCallBack);
			fprintf(stdout, "beng result %d\n",e);
			break;
	
		case PNC_RET_RTP_STREAM_SEND_CRITICAL:
			fprintf(stdout, "RTP STREAM SEND CRITICAL\n");
			// sprintf(bsCommande, "AT 1{%s}", bsBuffer);
			// fprintf(stdout, "COMMANDE : %s\n", bsCommande);
			data->SAUN_inc=1;
			fprintf(stdout, "[gpaclib] : ");
			gf_beng_encode_from_string(data->codec, (char *) bsBuffer, SampleCallBack);
			break;
	
		case PNC_RET_RTP_STREAM_RAP:
			data->RAP=1;
			data->RAPsent++;
			fprintf(stdout, "RTP STREAM RAP\n");
			fprintf(stdout, "[gpaclib] : ");
			gf_beng_aggregate_context(data->codec);
			gf_beng_encode_context(data->codec, SampleCallBack);
			break;
		case PNC_RET_RTP_STREAM_RAP_RESET:
			data->RAP=1; //On demande que RAP soit positionné dans le SL
			data->RAPsent++;
			fprintf(stdout, "RTP STREAM RAP\n");
			data->SAUN_inc=1; // On demande l'augmentation du SAUN
			fprintf(stdout, "[gpaclib] : ");
			gf_beng_aggregate_context(data->codec);
			gf_beng_encode_context(data->codec, SampleCallBack);
			break;
	}
	// unlocking mutex
	gf_mx_v(data->carrousel_mutex);
	return GF_OK;
}

#include "sdp_generator.h"

int sdp_generator(PNC_CallbackData * data, char *ip_dest, char *sdp_fmt)
{
	GF_BifsEngine *codec;
	GF_ESD *esd;
	u32 size,size64;
	char *buffer;
	char buf64[5000];
	FILE *fp;
	int ret;
	char temp[5000];
	u16 port;
	u32 socket_type;
		
	// fonctions necessaires pour recuperer les informations necessaires a la construction du fichier
	gf_sk_get_local_info(data->chan->rtp, &port, &socket_type);

	// fprintf(stdout, "%s --------- %d\n", ip_adresse, port);
	fp = fopen("broadcaster.sdp", "w+");
	if(fp == NULL)
	{
		fprintf(stdout, "[broadcaster] : erreur, probleme a ouvrir file temp pour scene initiale\n");
		exit(1);
	}
	// ecriture du fichier SDP
	ret = fwrite("v=0\n", 1, 4, fp);
	
	sprintf(temp, "o=GpacBroadcaster 3326096807 1117107880000 IN IP%d %s\n", gf_net_is_ipv6(ip_dest) ? 6 : 4, ip_dest);
	ret = fwrite(temp, 1, strlen(temp), fp);
	
	ret = fwrite("s=MPEG4Broadcaster\n", 1, 19, fp);
	
	sprintf(temp, "c=IN IP%d %s\n", gf_net_is_ipv6(ip_dest) ? 6 : 4, ip_dest);
	ret = fwrite(temp, 1, strlen(temp), fp);
	
	ret = fwrite("t=0 0\n", 1, 6, fp);
	
	// GF_BIFSEngine
	codec = (GF_BifsEngine *) data->codec;
	buffer = NULL;
	size = 0;
	gf_odf_desc_write((GF_Descriptor *) codec->ctx->root_od, &buffer, &size);
	esd = gf_list_get(codec->ctx->root_od->ESDescriptors, 0);
	
	//encode in Base64 the iod
	size64 = gf_base64_encode((unsigned char *) buffer, size, (unsigned char *) buf64, 2000);
	// size64 = gf_base64_encode(buffer, size, buf64, 2000);
	buf64[size64] = 0;
	free(buffer);

	// fprintf(stdout, "a=mpeg4-iod:\"data:application/mpeg4-iod;base64,%s\"\n", buf64);
	sprintf(temp, "a=mpeg4-iod:\"data:application/mpeg4-iod;base64,%s\"\n", buf64);
	ret = fwrite(temp, 1, strlen(temp), fp);
	
	sprintf(temp, "m=application %d RTP/AVP 96\n", port);
	ret = fwrite(temp, 1, strlen(temp), fp);
	
	ret = fwrite("a=rtpmap:96 mpeg4-generic/1000\n", 1, 31, fp);

	sprintf(temp, "a=mpeg4-esid:%d\n", esd->ESID);
	ret = fwrite(temp, 1, strlen(temp), fp);
	
	// fprintf(stdout, "%s\n", sdp_fmt);
	sprintf(temp, "%s\n", sdp_fmt);
	ret = fwrite(temp, 1, strlen(temp), fp);
	
	fclose(fp);
	fprintf(stdout, "[sdp generator] : fichier SDP generater in broadcaster.sdp\n");
	return GF_OK;
}

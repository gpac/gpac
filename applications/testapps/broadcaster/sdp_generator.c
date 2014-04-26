#include "sdp_generator.h"
#include "debug.h"

int sdp_generator(PNC_CallbackData *data, char *ip_dest, char *sdp_fmt)
{
	GF_SceneEngine *codec;
	GF_ESD *esd = NULL;
	u32 size,size64;
	char *buffer;
	char buf64[5000];
	FILE *fp;
	int ret;
	char temp[5000];
	u16 port;
	u32 socket_type;

	gf_sk_get_local_info(data->chan->rtp, &port, &socket_type);

	fp = fopen("broadcaster.sdp", "w+");
	if(fp == NULL) {
		fprintf(stderr, "Cannot open SDP file broadcaster.sdp\n");
		exit(1);
	}

	ret = gf_fwrite("v=0\n", 1, 4, fp);
	sprintf(temp, "o=GpacBroadcaster 3326096807 1117107880000 IN IP%d %s\n", gf_net_is_ipv6(ip_dest) ? 6 : 4, ip_dest);
	ret = gf_fwrite(temp, 1, strlen(temp), fp);

	ret = gf_fwrite("s=MPEG4Broadcaster\n", 1, 19, fp);

	sprintf(temp, "c=IN IP%d %s\n", gf_net_is_ipv6(ip_dest) ? 6 : 4, ip_dest);
	ret = gf_fwrite(temp, 1, strlen(temp), fp);

	ret = gf_fwrite("t=0 0\n", 1, 6, fp);

	codec = (GF_SceneEngine *) data->codec;
	if (codec) {
		buffer = NULL;
		size = 0;
		gf_odf_desc_write((GF_Descriptor *) codec->ctx->root_od, &buffer, &size);
		esd = gf_list_get(codec->ctx->root_od->ESDescriptors, 0);

		size64 = gf_base64_encode((unsigned char *) buffer, size, (unsigned char *) buf64, 2000);
		buf64[size64] = 0;
		free(buffer);

		sprintf(temp, "a=mpeg4-iod:\"data:application/mpeg4-iod;base64,%s\"\n", buf64);
		ret = gf_fwrite(temp, 1, strlen(temp), fp);
	}

	sprintf(temp, "m=application %d RTP/AVP 96\n", port);
	ret = gf_fwrite(temp, 1, strlen(temp), fp);

	ret = gf_fwrite("a=rtpmap:96 mpeg4-generic/1000\n", 1, 31, fp);

	if (esd) {
		sprintf(temp, "a=mpeg4-esid:%d\n", esd->ESID);
		ret = gf_fwrite(temp, 1, strlen(temp), fp);
	}

	sprintf(temp, "%s\n", sdp_fmt);
	ret = gf_fwrite(temp, 1, strlen(temp), fp);
	fflush(fp);
	fclose(fp);
	dprintf(DEBUG_sdp_generator, "SDP file generated in broadcaster.sdp\n");
	return GF_OK;
}

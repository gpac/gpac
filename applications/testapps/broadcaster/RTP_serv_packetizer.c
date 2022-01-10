#include "RTP_serv_packetizer.h"
#include "RTP_serv_sender.h"
#include <assert.h>

#include <gpac/ietf.h>
#include <gpac/mpeg4_odf.h>
#include <gpac/internal/media_dev.h>

#define MAX_PACKET_SIZE 2000

#include "debug.h"


void OnNewPacket(void *cbk, GF_RTPHeader *header)
{
	((PNC_CallbackData *)cbk)->formattedPacketLength = 0;
}

void OnPacketDone(void *cbk, GF_RTPHeader *header)
{
	PNC_CallbackData *data = (PNC_CallbackData *)cbk;
	dprintf(DEBUG_RTP_serv_packetizer, "RTP Packet done\n");
	PNC_SendRTP(data, ((PNC_CallbackData *)cbk)->formattedPacket, ((PNC_CallbackData *)cbk)->formattedPacketLength);
	((PNC_CallbackData *)cbk)->formattedPacketLength = 0;
}

void OnData(void *cbk, char *data, u32 data_size, Bool is_head)
{
	memcpy(((PNC_CallbackData *)cbk)->formattedPacket+((PNC_CallbackData *)cbk)->formattedPacketLength, data, data_size);
	((PNC_CallbackData *)cbk)->formattedPacketLength += data_size;
}

void PNC_InitPacketiser(PNC_CallbackData * data, char *sdp_fmt, unsigned short mtu_size)
{
	GP_RTPPacketizer *p;
	GF_SLConfig sl;
	memset(&sl, 0, sizeof(sl));
	sl.useTimestampsFlag = 1;
	sl.useRandomAccessPointFlag = 1;
	sl.timestampResolution = 1000;
	sl.AUSeqNumLength = 16;

	p = gf_rtp_builder_new(GF_RTP_PAYT_MPEG4,
	                       &sl,
	                       GP_RTP_PCK_SIGNAL_RAP | GP_RTP_PCK_SIGNAL_AU_IDX,
	                       data,
	                       OnNewPacket,
	                       OnPacketDone,
	                       NULL,
	                       OnData);
	if (!p) {
		fprintf(stderr, "Cannot create RTP builder \n");
		return;
	}

	/* Mtu size - 20 = payload max size */
	mtu_size-=20;
	gf_rtp_builder_init(p, 96, mtu_size, 0, 3, 1, 1, 0, 0, 0, 0, 0, 0, NULL);
	gf_rtp_builder_format_sdp(p, "mpeg4-generic", sdp_fmt, NULL, 0);
	p->rtp_header.Version=2;
	p->rtp_header.SSRC=rand();
	data->hdr=& p->rtp_header;
	data->rtpBuilder=p;
	data->formattedPacket = gf_malloc(MAX_PACKET_SIZE);
	data->formattedPacketLength = 0;
}

void PNC_ClosePacketizer(PNC_CallbackData *data)
{
	gf_free(data->formattedPacket);
	gf_rtp_builder_del(data->rtpBuilder);
}

GF_Err PNC_ProcessData(PNC_CallbackData * data, char *au, u32 size, u64 ts)
{
	assert( data );
	assert( au );
	/* We need to set a TS different every time */
	data->hdr->TimeStamp = (u32) gf_sys_clock();
	data->rtpBuilder->sl_header.compositionTimeStamp = (u32) gf_sys_clock();
	data->rtpBuilder->sl_header.randomAccessPointFlag = data->RAP;
	if (data->SAUN_inc) data->rtpBuilder->sl_header.AU_sequenceNumber++;

	/* reset input data config */
	data->RAP=0;
	data->SAUN_inc=0;

	data->rtpBuilder->sl_header.paddingBits = 0;
	gf_rtp_builder_process(data->rtpBuilder, au, size, 1, size, 0, 0);

	return GF_OK;
}

/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Copyright (c) Cyril Concolato / Jean Le Feuvre 2000-2005
 *					All rights reserved
 *
 *  This file is part of GPAC / mp4 simple streamer application
 *
 *  GPAC is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU Lesser General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your option)
 *  any later version.
 *   
 *  GPAC is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Lesser General Public License for more details.
 *   
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; see the file COPYING.  If not, write to
 *  the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA. 
 *
 */

#include <gpac/constants.h>
#include <gpac/isomedia.h>
#include <gpac/ietf.h>
#include <gpac/internal/ietf_dev.h>

typedef struct {

	GF_RTPChannel *channel; 
	GP_RTPPacketizer *packetizer;

	/* The current packet being formed */
	GF_RTPHeader header;
	char *payload;
	u32 payload_len;
	u32 payload_valid_len;
} RTP_Caller;

static void OnNewPacket(void *cbk, GF_RTPHeader *header)
{
	RTP_Caller *caller = cbk;
	if (!header) return;
	memcpy(&caller->header, header, sizeof(GF_RTPHeader));
}

static void OnPacketDone(void *cbk, GF_RTPHeader *header) 
{
	RTP_Caller *caller = cbk;
	gf_rtp_send_packet(caller->channel, (header?header:&caller->header), 0, 0, caller->payload, caller->payload_len);
	caller->payload_valid_len = 0;
}

static void OnData(void *cbk, char *data, u32 data_size, Bool is_head) 
{
	RTP_Caller *caller = cbk;
	if (!data ||!data_size) return;

	if (!caller->payload_len) {
		caller->payload = malloc(data_size);
		memcpy(caller->payload, data, data_size);
		caller->payload_len = caller->payload_valid_len = data_size;
	} else {
		if (caller->payload_valid_len + data_size > caller->payload_len) {
			caller->payload = realloc(caller->payload, caller->payload_valid_len + data_size);
			caller->payload_len = caller->payload_valid_len + data_size;
		} 
		if (!is_head) {
			memcpy(caller->payload+caller->payload_valid_len, data, data_size);
		} else {
			memmove(caller->payload+data_size, caller->payload, caller->payload_valid_len);
			memcpy(caller->payload, data, data_size);
		}
		caller->payload_valid_len += data_size;
	}
}

GF_Err rtp_init_packetizer(RTP_Caller *caller)
{
	char sdp_fmt[5000];
	caller->packetizer = gf_rtp_builder_new(GP_RTP_PAYT_MPEG4, NULL, GP_RTP_PCK_SIGNAL_RAP, caller, OnNewPacket, OnPacketDone, NULL, OnData);
	gf_rtp_builder_init(caller->packetizer, 96, 1500, 0, 
		/*stream type*/GF_STREAM_SCENE, /*objecttypeindication */1, 
		0, 0, 0, 0, 0, 0, 0, NULL); 
	
	/* regarder hint_track.c pour créer un SDP entier */
	gf_rtp_builder_format_sdp(caller->packetizer, "mpeg4-generic", sdp_fmt, NULL, 0);
	fprintf(stdout, "%s\n", sdp_fmt);

	caller->packetizer->rtp_header.Version = 2;
	caller->packetizer->rtp_header.SSRC = rand();//RandomNumber identifiant la source

	return GF_OK;
}

GF_Err rtp_init_channel(GF_RTPChannel **chan, char * dest, int port){
	GF_RTSPTransport tr;
	GF_Err res;

	*chan = gf_rtp_new();
	gf_rtp_set_ports(*chan);

	tr.IsUnicast=1;
	tr.Profile="RTP/AVP";//RTSP_PROFILE_RTP_AVP;
	tr.destination = dest;
	tr.source = "0.0.0.0";
	tr.IsRecord = 0;
	tr.Append = 0;
	tr.SSRC = rand();
	
	tr.client_port_first = (*chan)->net_info.client_port_first; //RTP
	tr.client_port_last  = (*chan)->net_info.client_port_last; //RTCP (not used a priori)

	tr.port_first        = port;   //RTP other end 
	tr.port_last         = port+1; //RTCP other end (not used a priori)

	res = gf_rtp_setup_transport(*chan, &tr, dest);
	if (res !=0) return res;

	gf_rtp_initialize(*chan, 0, 1, 1500, 0, 0, NULL);
	if (res !=0) return res;
	return GF_OK;
}

void usage() 
{
	fprintf(stdout, "usage: file.mp4 IP port\n");
}

int main(int argc, char **argv)
{
	RTP_Caller caller;
	GF_ISOFile *mp4;
	GF_Err e = GF_OK;
	u32 j, track_number = 1;

	memset(&caller, 0, sizeof(RTP_Caller));

	fprintf(stdout, "mp4 simple streamer\n");

	if (argc < 4) {
		usage();
		return -1;
	}

	mp4 = gf_isom_open(argv[1], GF_ISOM_OPEN_READ, NULL);
	if (!mp4) {
		fprintf(stderr, "Could not open input MP4 file\n");
		return GF_IO_ERR;
	} else {
		u32 count;
		u32 port = atoi(argv[3]);
		char *ip_address = argv[2];
		e = rtp_init_channel(&caller.channel, ip_address, port);
		if (e) {
			fprintf(stderr, "Could not initialize RTP Channel\n");
		}

		e = rtp_init_packetizer(&caller);
				
		count= gf_isom_get_sample_count(mp4, track_number);
		for (j=0; j<count; j++) {
			u32 duration, descIndex, timescale;
			u64 ts;
			GF_ISOSample *samp;
			
			timescale = gf_isom_get_media_timescale(mp4, track_number);		
			
			samp = gf_isom_get_sample(mp4, track_number, j+1, &descIndex);
			duration = gf_isom_get_sample_duration(mp4, track_number, j+1);
			
			ts = (u64) (caller.packetizer->sl_config.timestampResolution / timescale * (s64) (samp->DTS+samp->CTS_Offset));
			caller.packetizer->sl_header.compositionTimeStamp = ts;
			
			ts = (u64) (caller.packetizer->sl_config.timestampResolution / timescale * (s64)(samp->DTS));
			caller.packetizer->sl_header.decodingTimeStamp = ts;
			
			caller.packetizer->sl_header.randomAccessPointFlag = samp->IsRAP;
						
			gf_rtp_builder_process(caller.packetizer, samp->data, samp->dataLength, 1, samp->dataLength, duration, descIndex);
			gf_isom_sample_del(&samp);
		}
	}

	free(caller.payload);
	gf_rtp_del(caller.channel);
	gf_rtp_builder_del(caller.packetizer);
	if (mp4) gf_isom_close(mp4);

	return e;
}

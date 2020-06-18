/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Romain Bouqueau
 *			Copyright (c) Romain Bouqueau - GPAC Licensing 2016
 *					All rights reserved
 *
 *  This file is part of GPAC / sample m2ts streamer over UDP application
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
 
#include <gpac/mpegts.h>

#define MPEGTS_PKT_SIZE 188
//TODO #define NUM_TS_PKT_PER_UDP 7
#define QUEUE_SIZE 1024 /*max num pck between two PCRs, otherwise can block*/
#define TTL 64

typedef struct {
	GF_List *pck;
	GF_Semaphore *sema;
	GF_Socket *sk;
	char *buffer;
	volatile u64 data_read_idx, data_write_idx;
	u64 pcr, first_pcr;
	volatile Bool done;
} UDPSender;

void on_m2ts_event(GF_M2TS_Demuxer *ts, u32 evt_type, void *par)
{
	GF_M2TS_PES_PCK *pck;
	UDPSender *sender = ts->user;
	switch (evt_type) {
	case GF_M2TS_EVT_PES_PCR:
		pck = (GF_M2TS_PES_PCK*)par;
		if (sender->first_pcr == (u64)-1) {
			sender->first_pcr = pck->stream->program->last_pcr_value;
		}
		sender->pcr = pck->stream->program->last_pcr_value;
		break;
	}
}

u32 sender_thread_proc(void *param) {
	char *data = NULL;
	UDPSender *sender = param;
	s32 time_to_wait_in_ms = 0;
	while (!sender->done) {
		gf_sema_wait(sender->sema);
		time_to_wait_in_ms = (s32)((sender->pcr - sender->first_pcr) / 27000 - gf_sys_clock());
		if (time_to_wait_in_ms > 0) {
			gf_sleep(time_to_wait_in_ms);
		} else if (time_to_wait_in_ms < -300) {
			fprintf(stderr, "sender late from %d ms\n", -time_to_wait_in_ms);
		}
		data = sender->buffer + MPEGTS_PKT_SIZE * (sender->data_read_idx % QUEUE_SIZE);
		gf_sk_send(sender->sk, data, MPEGTS_PKT_SIZE); //TODO: *= NUM_TS_PKT_PER_UDP
		sender->data_read_idx++;
	}

	return 0;
}

int main(int argc, char **argv)
{
	u32 size = 0;
	u64 fsize = 0, fdone = 0;
	FILE *src = NULL;
	GF_M2TS_Demuxer *ts = NULL;
	GF_Thread *th = NULL;
	u16 port = 0;
	UDPSender sender;

	if (argc != 4) {
		fprintf(stderr, "Usage  : %s ts_file ip_addr   port\n", argv[0]);
		fprintf(stderr, "Example: %s file.ts 224.0.0.1 1234\n", argv[0]);
		return 1;
	}
	port = atoi(argv[3]);
	fprintf(stdout, "Detected: %s ts_file %s %s %u\n", argv[0], argv[1], argv[2], (u32)port);

	gf_sys_init(GF_FALSE);
	memset(&sender, 0, sizeof(UDPSender));
	sender.first_pcr = (u64)-1;
	src = gf_fopen(argv[1], "rb");
	if (!src) {
		fprintf(stderr, "Couldn't open input file %s\n", argv[1]);
		goto exit;
	}
	th = gf_th_new("UDP sender");
	sender.sema = gf_sema_new(QUEUE_SIZE, 0);
	gf_th_set_priority(th, GF_THREAD_PRIORITY_REALTIME);
	ts = gf_m2ts_demux_new();
	ts->on_event = on_m2ts_event;
	ts->user = &sender;
	sender.sk = gf_sk_new(GF_SOCK_TYPE_UDP);
	if (gf_sk_is_multicast_address(argv[2])) {
		gf_sk_setup_multicast(sender.sk, argv[2], port, TTL, 0, NULL);
	} else {
		gf_sk_bind(sender.sk, NULL, port, argv[2], port, GF_SOCK_REUSE_PORT);
	}
	sender.buffer = gf_malloc(MPEGTS_PKT_SIZE*QUEUE_SIZE);

	gf_fseek(src, 0, SEEK_END);
	fsize = gf_ftell(src);
	gf_fseek(src, 0, SEEK_SET);
	gf_th_run(th, sender_thread_proc, &sender);
	while (!feof(src)) {
		if (sender.data_write_idx - sender.data_read_idx < QUEUE_SIZE) {
			char *data = sender.buffer + MPEGTS_PKT_SIZE * (sender.data_write_idx % QUEUE_SIZE);
			size = (u32)gf_fread(data, 1, MPEGTS_PKT_SIZE, src);
			if (size<MPEGTS_PKT_SIZE) break;
			gf_m2ts_process_data(ts, data, size);
			sender.data_write_idx++;
			gf_sema_notify(sender.sema, 1);

			fdone += size;
			gf_set_progress("MPEG-2 TS Processing", fdone, fsize);
		} else {
			gf_sleep(10);
		}
	}
	sender.done = GF_TRUE;
	gf_set_progress("MPEG-2 TS Processing", fsize, fsize);

exit:
	if(src) gf_fclose(src);
	gf_m2ts_demux_del(ts);
	gf_th_del(th);
	gf_sema_del(sender.sema);
	gf_sk_del(sender.sk);
	gf_sys_close();
	gf_free(sender.buffer);
	return 0;
}

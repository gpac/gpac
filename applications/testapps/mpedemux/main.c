/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2007-2012
 *					All rights reserved
 *
 *  This file is part of GPAC / mpedemux application
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

typedef struct
{
	FILE *ts_file;
	GF_M2TS_Demuxer *ts_demux;
} MPEDemux;


static void mpedemux_on_event(GF_M2TS_Demuxer *ts, u32 evt_type, void *param)
{
	MPEDemux *mpedemux= (MPEDemux *) ts->user;
	switch (evt_type) {
	case GF_M2TS_EVT_PAT_FOUND:
		/* called when the first PAT is fully parsed */
		break;
	case GF_M2TS_EVT_SDT_FOUND:
		/* called when the first SDT is fully parsed */
		break;
	case GF_M2TS_EVT_PMT_FOUND:
		/* called when a first PMT is fully parsed */
		break;
	case GF_M2TS_EVT_INT_FOUND:
		/* called when a first INT is fully parsed */
		/* TODO: create socket for each target in the IP platform */
		break;
	case GF_M2TS_EVT_PAT_UPDATE:
	case GF_M2TS_EVT_PMT_UPDATE:
	case GF_M2TS_EVT_SDT_UPDATE:
		/* called when a new version of the table is parsed */
		break;
	case GF_M2TS_EVT_PES_PCK:
		/* called when a PES packet is parsed */
		break;
	case GF_M2TS_EVT_SL_PCK:
		/* called when an MPEG-4 SL-packet is parsed */
		break;
	case GF_M2TS_EVT_IP_DATAGRAM:
		/* called when an IP packet is parsed
			TODO: send this packet on the right socket */
		break;
	}
}

static void usage()
{
	fprintf(stdout, "mpedemux input.ts\n");
}

int main(int argc, char **argv)
{
	u8 data[188];
	u32 size;
	MPEDemux *mpedemux;

	if (argc < 2) {
		usage();
		return GF_OK;
	}

	GF_SAFEALLOC(mpedemux, MPEDemux);
	mpedemux->ts_demux = gf_m2ts_demux_new();
	mpedemux->ts_demux->on_event = mpedemux_on_event;
	mpedemux->ts_demux->user = mpedemux;

	mpedemux->ts_file = fopen(argv[1], "rb");

	while (1) {
		/*read chunks by chunks*/
		size = fread(data, 1, 188, mpedemux->ts_file);
		if (!size) break;
		/*process chunk*/
		gf_m2ts_process_data(mpedemux->ts_demux, data, size);
	}

	gf_m2ts_demux_del(mpedemux->ts_demux);
	gf_free(mpedemux);
	return GF_OK;
}

#include <gpac/mpegts.h>

u32 dump_pid = 130;
FILE *dest = NULL;
Bool has_seen_pat = 0;

void on_m2ts_event(GF_M2TS_Demuxer *ts, u32 evt_type, void *par)
{
	GF_M2TS_PES_PCK *pck;
	switch (evt_type) {
	case GF_M2TS_EVT_PAT_FOUND:
		fprintf(stdout, "Service connected (PAT found)\n");
		break;
	case GF_M2TS_EVT_PAT_REPEAT:
		has_seen_pat = 1;
		break;
	case GF_M2TS_EVT_PAT_UPDATE:
		fprintf(stdout, "Service connected (PAT found)\n");
		break;
	case GF_M2TS_EVT_PMT_FOUND:
		fprintf(stdout, "Program list found - %d streams\n", gf_list_count( ((GF_M2TS_Program*)par)->streams) );
		break;
	case GF_M2TS_EVT_PMT_UPDATE:
		fprintf(stdout, "Program list updated - %d streams\n", gf_list_count( ((GF_M2TS_Program*)par)->streams) );
		break;
	case GF_M2TS_EVT_SDT_FOUND:
		fprintf(stdout, "Program Description found - %d desc\n", gf_list_count(ts->SDTs) );
		break;
	case GF_M2TS_EVT_SDT_UPDATE:
		fprintf(stdout, "Program Description updated - %d desc\n", gf_list_count(ts->SDTs) );
		break;
	case GF_M2TS_EVT_PES_PCK:
		pck = par;
		if (dest && (dump_pid == pck->stream->pid)) {
			gf_fwrite(pck->data, pck->data_len, 1, dest);
		}

		//fprintf(stdout, "PES(%d): DTS "LLD" PTS" LLD" RAP %d size %d\n", pck->stream->pid, pck->DTS, pck->PTS, pck->rap, pck->data_len);
		break;
	}
}

int main(int argc, char **argv)
{
	char data[188];
	u32 size, fsize, fdone;
	GF_M2TS_Demuxer *ts;

	FILE *src = fopen(argv[1], "rb");
	ts = gf_m2ts_demux_new();
	ts->on_event = on_m2ts_event;

	fseek(src, 0, SEEK_END);
	fsize = ftell(src);
	fseek(src, 0, SEEK_SET);
	fdone = 0;

	while (!feof(src)) {
		size = fread(data, 1, 188, src);
		if (size<188) break;

		gf_m2ts_process_data(ts, data, size);
		if (has_seen_pat) break;
	}

	dest = fopen("pes.mp3", "wb");
	gf_m2ts_reset_parsers(ts);
	gf_f64_seek(src, 0, SEEK_SET);
	fdone = 0;
	while (!feof(src)) {
		size = fread(data, 1, 188, src);
		if (size<188) break;

		gf_m2ts_process_data(ts, data, size);

		fdone += size;
		gf_set_progress("MPEG-2 TS Parsing", fdone, fsize);
	}
	gf_set_progress("MPEG-2 TS Parsing", fsize, fsize);

	fclose(src);
	gf_m2ts_demux_del(ts);
	if (dest) fclose(dest);
	return 0;
}




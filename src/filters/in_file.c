/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2017-2022
 *					All rights reserved
 *
 *  This file is part of GPAC / generic FILE input filter
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


#include <gpac/filters.h>
#include <gpac/constants.h>

enum{
	FILE_RAND_NONE=0,
	FILE_RAND_ANY,
	FILE_RAND_SC_ANY,
	FILE_RAND_SC_AVC,
	FILE_RAND_SC_HEVC,
	FILE_RAND_SC_AV1
};

typedef struct
{
	//options
	char *src;
	char *ext, *mime;
	u32 block_size;
	GF_Fraction64 range;

	//only one output pid declared
	GF_FilterPid *pid;

	FILE *file;
	u64 file_size;
	u64 file_pos, end_pos;
	Bool is_end, pck_out;
	Bool is_null;
	Bool full_file_only;
	Bool do_reconfigure;
	char *block;
	u32 is_random;
	Bool cached_set;
	Bool no_failure;
} GF_FileInCtx;


static GF_Err filein_initialize(GF_Filter *filter)
{
	GF_FileInCtx *ctx = (GF_FileInCtx *) gf_filter_get_udta(filter);
	FILE *old_file = NULL;
	char *ext_start = NULL;
	char *frag_par = NULL;
	char *cgi_par = NULL;
	char *src, *path;
	const char *prev_url=NULL;

	if (!ctx || !ctx->src) return GF_BAD_PARAM;

	if (!strcmp(ctx->src, "null")) {
		ctx->pid = gf_filter_pid_new(filter);
		gf_filter_pid_set_property(ctx->pid, GF_PROP_PID_STREAM_TYPE, &PROP_UINT(GF_STREAM_FILE));
		gf_filter_pid_set_eos(ctx->pid);
		ctx->is_end = GF_TRUE;
		return GF_OK;
	}
	if (!strcmp(ctx->src, "rand") || !strcmp(ctx->src, "randsc")) {
		gf_rand_init(GF_FALSE);
		ctx->is_random = FILE_RAND_ANY;
		if (!strcmp(ctx->src, "randsc")) {
			ctx->is_random = FILE_RAND_SC_ANY;
			if (ctx->mime) {
				if (!strcmp(ctx->mime, "video/avc")) ctx->is_random = FILE_RAND_SC_AVC;
				if (!strcmp(ctx->mime, "video/hevc")) ctx->is_random = FILE_RAND_SC_HEVC;
				if (!strcmp(ctx->mime, "video/av1")) ctx->is_random = FILE_RAND_SC_AV1;
			}
		}

		if (!ctx->block_size) ctx->block_size = 5000;
		while (ctx->block_size % 4) ctx->block_size++;
		ctx->block = gf_malloc(ctx->block_size +1);
		return GF_OK;
	}



	if (strnicmp(ctx->src, "file:/", 6) && strnicmp(ctx->src, "gfio:/", 6) && strstr(ctx->src, "://"))  {
		gf_filter_setup_failure(filter, GF_NOT_SUPPORTED);
		return GF_NOT_SUPPORTED;
	}
	path = strstr(ctx->src, "://");
	if (path) path += 3;
	if (path && strstr(path, "://")) {
		ctx->is_end = GF_TRUE;
		return gf_filter_pid_raw_new(filter, path, path, NULL, NULL, NULL, 0, GF_TRUE, &ctx->pid);
	}

	//local file

	//strip any fragment identifer
	ext_start = gf_file_ext_start(ctx->src);
	frag_par = strchr(ext_start ? ext_start : ctx->src, '#');
	if (frag_par) frag_par[0] = 0;
	cgi_par = strchr(ctx->src, '?');
	if (cgi_par) cgi_par[0] = 0;

	src = (char *) ctx->src;
	if (!strnicmp(ctx->src, "file://", 7)) src += 7;
	else if (!strnicmp(ctx->src, "file:", 5)) src += 5;

	//for gfio, do not close file until we open the new one
	if (ctx->do_reconfigure) {
		old_file = ctx->file;
		ctx->file = NULL;
		if (gf_fileio_check(old_file))
			prev_url = gf_fileio_url((GF_FileIO *)old_file);
	}

	if (!ctx->file) {
		ctx->file = gf_fopen_ex(src, prev_url, "rb", GF_FALSE);
	}

	if (old_file) {
		gf_fclose(old_file);
	}

	if (!ctx->file) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_MMIO, ("[FileIn] Failed to open %s\n", src));

		if (frag_par) frag_par[0] = '#';
		if (cgi_par) cgi_par[0] = '?';

		if (ctx->no_failure) {
			gf_filter_notification_failure(filter, GF_URL_ERROR, GF_FALSE);
			ctx->is_end = GF_TRUE;
			return GF_OK;
		}

		gf_filter_setup_failure(filter, GF_URL_ERROR);
#ifdef GPAC_ENABLE_COVERAGE
		if (gf_sys_is_cov_mode() && !strcmp(src, "blob"))
			return GF_OK;
#endif
		return GF_URL_ERROR;
	}
	GF_LOG(GF_LOG_INFO, GF_LOG_MMIO, ("[FileIn] opening %s\n", src));
	ctx->file_size = gf_fsize(ctx->file);

	ctx->cached_set = GF_FALSE;
	ctx->full_file_only = GF_FALSE;

	if (ctx->do_reconfigure && gf_fileio_check(ctx->file)) {
		GF_FileIO *gfio = (GF_FileIO *)ctx->file;
		gf_free(ctx->src);
		ctx->src = gf_strdup(gf_fileio_url(gfio));
	}


	ctx->file_pos = ctx->range.num;
	if (ctx->range.den) {
		ctx->end_pos = ctx->range.den;
		if (ctx->end_pos>ctx->file_size) {
			ctx->range.den = ctx->end_pos = ctx->file_size;
		}
	}
	gf_fseek(ctx->file, ctx->file_pos, SEEK_SET);
	ctx->is_end = GF_FALSE;

	if (frag_par) frag_par[0] = '#';
	if (cgi_par) cgi_par[0] = '?';

	if (!ctx->block) {
		if (!ctx->block_size) {
			if (ctx->file_size>500000000) ctx->block_size = 1000000;
			else ctx->block_size = 5000;
		}
		ctx->block = gf_malloc(ctx->block_size +1);
	}
	return GF_OK;
}

static void filein_finalize(GF_Filter *filter)
{
	GF_FileInCtx *ctx = (GF_FileInCtx *) gf_filter_get_udta(filter);

	if (ctx->file) gf_fclose(ctx->file);
	if (ctx->block) gf_free(ctx->block);
}

static GF_FilterProbeScore filein_probe_url(const char *url, const char *mime_type)
{
	char *ext_start = NULL;
	char *frag_par = NULL;
	char *cgi_par = NULL;
	char *src = (char *) url;
	Bool res;

	if (!strcmp(url, "-") || !strcmp(url, "stdin")) return GF_FPROBE_NOT_SUPPORTED;

	if (!strnicmp(url, "file://", 7)) src += 7;
	else if (!strnicmp(url, "file:", 5)) src += 5;

	if (!strcmp(url, "null")) return GF_FPROBE_SUPPORTED;
	if (!strcmp(url, "rand")) return GF_FPROBE_SUPPORTED;
	if (!strcmp(url, "randsc")) return GF_FPROBE_SUPPORTED;
	if (!strncmp(src, "isobmff://", 10)) return GF_FPROBE_SUPPORTED;
	if (!strncmp(url, "gfio://", 7)) {
		GF_FileIO *gfio = gf_fileio_from_url(url);
		if (gfio && gf_fileio_read_mode(gfio))
			return GF_FPROBE_SUPPORTED;
		return GF_FPROBE_NOT_SUPPORTED;
	}
	if (strstr(src, "://"))
		return GF_FPROBE_NOT_SUPPORTED;


	//strip any fragment identifer
	ext_start = gf_file_ext_start(url);
	frag_par = strchr(ext_start ? ext_start : url, '#');
	if (frag_par) frag_par[0] = 0;
	cgi_par = strchr(url, '?');
	if (cgi_par) cgi_par[0] = 0;

	res = gf_file_exists(src);

	if (frag_par) frag_par[0] = '#';
	if (cgi_par) cgi_par[0] = '?';

	return res ? GF_FPROBE_SUPPORTED : GF_FPROBE_NOT_SUPPORTED;
}

static Bool filein_process_event(GF_Filter *filter, const GF_FilterEvent *evt)
{
	GF_FileInCtx *ctx = (GF_FileInCtx *) gf_filter_get_udta(filter);

	if (evt->base.on_pid && (evt->base.on_pid != ctx->pid))
		return GF_FALSE;

	switch (evt->base.type) {
	case GF_FEVT_PLAY:
	case GF_FEVT_PLAY_HINT:
		ctx->full_file_only = evt->play.full_file_only;
		return GF_TRUE;
	case GF_FEVT_STOP:
		//stop sending data
		ctx->is_end = GF_TRUE;
		gf_filter_pid_set_eos(ctx->pid);
		return GF_TRUE;
	case GF_FEVT_SOURCE_SEEK:
		if (ctx->is_random)
			return GF_TRUE;
		if (ctx->file_size && (evt->seek.start_offset >= ctx->file_size)) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_MMIO, ("[FileIn] Seek request outside of file %s range ("LLU" vs size "LLU")\n", ctx->src, evt->seek.start_offset, ctx->file_size));
			return GF_TRUE;
		}

		GF_LOG(GF_LOG_INFO, GF_LOG_MMIO, ("[FileIn] Asked to seek source to range "LLU"-"LLU"\n", evt->seek.start_offset, evt->seek.end_offset));
		ctx->is_end = GF_FALSE;

		if (gf_fileio_check(ctx->file)) {
			ctx->cached_set = GF_FALSE;
		}

		if (ctx->file) {
			int res = gf_fseek(ctx->file, evt->seek.start_offset, SEEK_SET);
			if (res) {
				GF_LOG(GF_LOG_ERROR, GF_LOG_MMIO, ("[FileIn] Seek on file failed: %d\n", res));
				return GF_TRUE;
			}
		}

		ctx->file_pos = evt->seek.start_offset;
		ctx->end_pos = evt->seek.end_offset;
		if (ctx->end_pos>ctx->file_size) ctx->end_pos = ctx->file_size;
		ctx->range.num = evt->seek.start_offset;
		ctx->range.den = ctx->end_pos;
		if (evt->seek.hint_block_size > ctx->block_size) {
			ctx->block_size = evt->seek.hint_block_size;
			ctx->block = gf_realloc(ctx->block, ctx->block_size+1);
		}
		return GF_TRUE;
	case GF_FEVT_SOURCE_SWITCH:
		if (ctx->is_random)
			return GF_TRUE;
		GF_LOG(GF_LOG_INFO, GF_LOG_MMIO, ("[FileIn] Asked to switch source to %s (range "LLU"-"LLU")\n", evt->seek.source_switch ? evt->seek.source_switch : "self", evt->seek.start_offset, evt->seek.end_offset));
		assert(ctx->is_end);
		ctx->range.num = evt->seek.start_offset;
		ctx->range.den = evt->seek.end_offset;
		if (evt->seek.source_switch) {
			if (strcmp(evt->seek.source_switch, ctx->src)) {
				gf_free(ctx->src);
				ctx->src = gf_strdup(evt->seek.source_switch);
			}
			ctx->do_reconfigure = GF_TRUE;
		}
		//don't send a setup failure on source switch (this would destroy ourselves which we don't want in DASH)
		ctx->no_failure = GF_TRUE;
		filein_initialize(filter);
		gf_filter_post_process_task(filter);
		break;
	case GF_FEVT_FILE_DELETE:
		if (ctx->is_end && !strcmp(evt->file_del.url, "__gpac_self__")) {
			if (ctx->file) {
				gf_fclose(ctx->file);
				ctx->file = NULL;
			}
			gf_file_delete(ctx->src);
		}
		break;

	default:
		break;
	}
	return GF_FALSE;
}


static void filein_pck_destructor(GF_Filter *filter, GF_FilterPid *pid, GF_FilterPacket *pck)
{
	GF_FileInCtx *ctx = (GF_FileInCtx *) gf_filter_get_udta(filter);
	ctx->pck_out = GF_FALSE;
	//ready to process again
	gf_filter_post_process_task(filter);
}

static GF_Err filein_process(GF_Filter *filter)
{
	GF_Err e;
	u32 nb_read, to_read;
	u64 lto_read;
	GF_FilterPacket *pck;
	GF_FileInCtx *ctx = (GF_FileInCtx *) gf_filter_get_udta(filter);

	if (ctx->is_end)
		return GF_EOS;

	//until packet is released we return EOS (no processing), and ask for processing again upon release
	if (ctx->pck_out)
		return GF_EOS;
	if (ctx->pid && gf_filter_pid_would_block(ctx->pid)) {
		return GF_OK;
	}

	if (ctx->full_file_only && ctx->pid && !ctx->do_reconfigure && ctx->cached_set) {
		pck = gf_filter_pck_new_shared(ctx->pid, ctx->block, 0, filein_pck_destructor);
		if (!pck) return GF_OUT_OF_MEM;

		ctx->is_end = GF_TRUE;
		gf_filter_pck_set_framing(pck, ctx->file_pos ? GF_FALSE : GF_TRUE, ctx->is_end);
		gf_filter_pck_set_sap(pck, GF_FILTER_SAP_1);
		ctx->pck_out = GF_TRUE;
		gf_filter_pck_send(pck);
		gf_filter_pid_set_eos(ctx->pid);

		if (ctx->file_size && gf_filter_reporting_enabled(filter)) {
			char szStatus[1024], *szSrc;
			szSrc = gf_file_basename(ctx->src);

			sprintf(szStatus, "%s: EOS (dispatch canceled after "LLD" b, file size "LLD" b)", szSrc, (s64) ctx->file_pos, (s64) ctx->file_size);
			gf_filter_update_status(filter, 10000, szStatus);
		}
		return GF_OK;
	}
	if (ctx->is_random) {
		u32 i;
		if (ctx->is_random>=FILE_RAND_SC_ANY) {
			for (i=0; i<ctx->block_size; i+= 4) {
				u32 val = gf_rand();

				if (i+4>=ctx->block_size) {
					* ((u32 *) (ctx->block + i)) = val;
					continue;
				}
				if (val % 100) {
					* ((u32 *) (ctx->block + i)) = val;
					continue;
				}
				if (ctx->is_random==FILE_RAND_SC_AVC) {
					u32 rand_high = val>>24;
					* ((u32 *) (ctx->block + i)) = 0x00000001;
					i += 4;
					val &= 0x00FFFFFF;
					rand_high = rand_high%31;
					rand_high <<= 24;
					val |= rand_high;
					* ((u32 *) (ctx->block + i)) = val;
				} else if (ctx->is_random==FILE_RAND_SC_HEVC) {
					u32 rand_high = val>>16;
					* ((u32 *) (ctx->block + i)) = 0x00000001;
					i += 4;
					val &= 0x0000FFFF;
					rand_high = rand_high % 63;
					rand_high <<= 8;
					rand_high |= 1; //end of layerid (=0) and temporal sublayer (=1)
					rand_high <<= 16;
					val |= rand_high;
					* ((u32 *) (ctx->block + i)) = val;
				}
				else {
					val &= 0x000001FF;
					* ((u32 *) (ctx->block + i)) = val;
				}
			}
		} else {
			for (i=0; i<ctx->block_size; i+= 4) {
				* ((u32 *) (ctx->block + i)) = gf_rand();
			}
		}
		ctx->block[ctx->block_size]=0;

		if (!ctx->pid) {
			e = gf_filter_pid_raw_new(filter, ctx->src, ctx->src, ctx->mime, ctx->ext, ctx->block, ctx->block_size, GF_TRUE,  &ctx->pid);
			if (e) return e;
		}
		pck = gf_filter_pck_new_shared(ctx->pid, ctx->block, ctx->block_size, filein_pck_destructor);
		if (!pck) return GF_OUT_OF_MEM;

		gf_filter_pck_set_framing(pck, ctx->file_pos ? GF_FALSE : GF_TRUE, GF_FALSE);
		gf_filter_pck_set_sap(pck, GF_FILTER_SAP_1);
		ctx->file_pos += ctx->block_size;

		ctx->pck_out = GF_TRUE;
		gf_filter_pck_send(pck);
		return GF_OK;
	}

	//compute size to read as u64 (large file)
	if (ctx->end_pos > ctx->file_pos)
		lto_read = ctx->end_pos - ctx->file_pos;
	else if (ctx->file_size)
		lto_read = ctx->file_size - ctx->file_pos;
	else
		lto_read = ctx->block_size;

	//and clamp based on blocksize as u32
	if (lto_read > (u64) ctx->block_size)
		to_read = (u64) ctx->block_size;
	else
		to_read = (u32) lto_read;

	nb_read = (u32) gf_fread(ctx->block, to_read, ctx->file);
	if (!nb_read)
		ctx->file_size = ctx->file_pos;

	ctx->block[nb_read] = 0;
	if (!ctx->pid || ctx->do_reconfigure) {
		//quick hack for ID3v2: if detected, increase block size to have the full id3v2 + some frames in the initial block
		//to avoid relying on file extension for demux
		if (!ctx->pid && (nb_read>10)
			&& (ctx->block[0] == 'I' && ctx->block[1] == 'D' && ctx->block[2] == '3')
		) {
			u32 tag_size = ((ctx->block[9] & 0x7f) + ((ctx->block[8] & 0x7f) << 7) + ((ctx->block[7] & 0x7f) << 14) + ((ctx->block[6] & 0x7f) << 21));
			if (tag_size > nb_read) {
				u32 probe_size = tag_size + ctx->block_size;
				if (probe_size>ctx->file_size)
					probe_size = (u32) ctx->file_size;

				ctx->block_size = probe_size;
				ctx->block = gf_realloc(ctx->block, ctx->block_size+1);
				nb_read += (u32) gf_fread(ctx->block + nb_read, probe_size-nb_read, ctx->file);
			}
		}

		ctx->do_reconfigure = GF_FALSE;
		e = gf_filter_pid_raw_new(filter, ctx->src, ctx->src, ctx->mime, ctx->ext, ctx->block, nb_read, GF_TRUE, &ctx->pid);
		if (e) return e;

		if (!gf_fileio_check(ctx->file)) {
			gf_filter_pid_set_property(ctx->pid, GF_PROP_PID_FILE_CACHED, &PROP_BOOL(GF_TRUE) );

			gf_filter_pid_set_property(ctx->pid, GF_PROP_PID_DOWN_SIZE, ctx->file_size ? &PROP_LONGUINT(ctx->file_size) : NULL);

			ctx->cached_set = GF_TRUE;
		}

		if (ctx->range.num || ctx->range.den)
			gf_filter_pid_set_property(ctx->pid, GF_PROP_PID_FILE_RANGE, &PROP_FRAC64(ctx->range) );
	}

	//GFIO wrapper, gets stats and update
	if (!ctx->cached_set) {
		u64 bdone, btotal;
		Bool fcached;
		u32 bytes_per_sec;
		if (gf_fileio_get_stats((GF_FileIO *)ctx->file, &bdone, &btotal, &fcached, &bytes_per_sec)) {
			if (fcached) {
				gf_filter_pid_set_property(ctx->pid, GF_PROP_PID_FILE_CACHED, &PROP_BOOL(fcached) );
				ctx->cached_set = GF_TRUE;
			}

			gf_filter_pid_set_info(ctx->pid, GF_PROP_PID_DOWN_SIZE, &PROP_LONGUINT(btotal) );
			gf_filter_pid_set_info(ctx->pid, GF_PROP_PID_DOWN_BYTES, &PROP_LONGUINT(bdone) );
			gf_filter_pid_set_info(ctx->pid, GF_PROP_PID_DOWN_RATE, &PROP_UINT(bytes_per_sec*8) );
		}
	}

	pck = gf_filter_pck_new_shared(ctx->pid, ctx->block, nb_read, filein_pck_destructor);
	if (!pck) return GF_OUT_OF_MEM;

	gf_filter_pck_set_byte_offset(pck, ctx->file_pos);

	if (ctx->file_size && (ctx->file_pos + nb_read == ctx->file_size)) {
		ctx->is_end = GF_TRUE;
		gf_filter_pid_set_info(ctx->pid, GF_PROP_PID_DOWN_BYTES, &PROP_LONGUINT(ctx->file_size) );
	} else if (ctx->end_pos && (ctx->file_pos + nb_read == ctx->end_pos)) {
		ctx->is_end = GF_TRUE;
		gf_filter_pid_set_info(ctx->pid, GF_PROP_PID_DOWN_BYTES, &PROP_LONGUINT(ctx->range.den - ctx->range.num) );
	} else {
		if (nb_read < to_read) {
			Bool is_eof;
			GF_LOG(GF_LOG_WARNING, GF_LOG_MMIO, ("[FileIn] Asked to read %d but got only %d\n", to_read, nb_read));

			is_eof = gf_feof(ctx->file);

			if (is_eof) {
				gf_filter_pid_set_info(ctx->pid, GF_PROP_PID_DOWN_BYTES, &PROP_LONGUINT(ctx->file_size) );
				ctx->is_end = GF_TRUE;
				GF_LOG(GF_LOG_ERROR, GF_LOG_MMIO, ("[FileIn] IO error EOF found after reading %d bytes but file %s size is %d\n", ctx->file_pos+nb_read, ctx->src, ctx->file_size));
			}
		} else {
			gf_filter_pid_set_info(ctx->pid, GF_PROP_PID_DOWN_BYTES, &PROP_LONGUINT(ctx->file_pos) );
		}
	}
	gf_filter_pck_set_framing(pck, ctx->file_pos ? GF_FALSE : GF_TRUE, ctx->is_end);
	gf_filter_pck_set_sap(pck, GF_FILTER_SAP_1);
	ctx->file_pos += nb_read;

	ctx->pck_out = GF_TRUE;
	gf_filter_pck_send(pck);

	if (ctx->file_size && gf_filter_reporting_enabled(filter)) {
		char szStatus[1024], *szSrc;
		szSrc = gf_file_basename(ctx->src);

		sprintf(szStatus, "%s: % 16"LLD_SUF" /% 16"LLD_SUF" (%02.02f)", szSrc, (s64) ctx->file_pos, (s64) ctx->file_size, ((Double)ctx->file_pos*100.0)/ctx->file_size);
		gf_filter_update_status(filter, (u32) (ctx->file_pos*10000/ctx->file_size), szStatus);
	}

	if (ctx->is_end) {
		gf_filter_pid_set_eos(ctx->pid);
		return GF_EOS;
	}
	return ctx->pck_out ? GF_EOS : GF_OK;
}



#define OFFS(_n)	#_n, offsetof(GF_FileInCtx, _n)

static const GF_FilterArgs FileInArgs[] =
{
	{ OFFS(src), "location of source file", GF_PROP_NAME, NULL, NULL, 0},
	{ OFFS(block_size), "block size used to read file. 0 means 5000 if file less than 500m, 1M otherwise", GF_PROP_UINT, "0", NULL, GF_FS_ARG_HINT_ADVANCED},
	{ OFFS(range), "byte range", GF_PROP_FRACTION64, "0-0", NULL, 0},
	{ OFFS(ext), "override file extension", GF_PROP_NAME, NULL, NULL, 0},
	{ OFFS(mime), "set file mime type", GF_PROP_NAME, NULL, NULL, 0},
	{0}
};

static const GF_FilterCapability FileInCaps[] =
{
	CAP_UINT(GF_CAPS_OUTPUT,  GF_PROP_PID_STREAM_TYPE, GF_STREAM_FILE),
};

GF_FilterRegister FileInRegister = {
	.name = "fin",
	GF_FS_SET_DESCRIPTION("File input")
	GF_FS_SET_HELP("This filter dispatch raw blocks from input file into a filter chain.\n"
	"Block size can be adjusted using [-block_size]().\n"
	"Content format can be forced through [-mime]() and file extension can be changed through [-ext]().\n"
	"Note: Unless disabled at session level (see [-no-probe](CORE) ), file extensions are usually ignored and format probing is done on the first data block.\n"
	"The special file name `null` is used for creating a file with no data, needed by some filters such as [dasher](dasher).\n"
	"The special file name `rand` is used to generate random data.\n"
	"The special file name `randsc` is used to generate random data with `0x000001` start-code prefix.\n"
	"\n"
	"The filter handles both files and GF_FileIO objects as input URL.\n"
	)
	.private_size = sizeof(GF_FileInCtx),
	.args = FileInArgs,
	.initialize = filein_initialize,
	.flags = GF_FS_REG_FORCE_REMUX,
	SETCAPS(FileInCaps),
	.finalize = filein_finalize,
	.process = filein_process,
	.process_event = filein_process_event,
	.probe_url = filein_probe_url
};


const GF_FilterRegister *filein_register(GF_FilterSession *session)
{
	return &FileInRegister;
}


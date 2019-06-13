/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2017-2018
 *					All rights reserved
 *
 *  This file is part of GPAC / generic FILE output filter
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
#include <gpac/xml.h>

typedef struct
{
	//options
	Double start, speed;
	char *dst, *mime, *fext;
	Bool append, dynext, cat, ow;

	//only one input pid
	GF_FilterPid *pid;

	FILE *file;
	Bool is_std;
	u64 nb_write;

	GF_FilterCapability in_caps[2];
	char szExt[10];
	char szFileName[GF_MAX_PATH];

	Bool patch_blocks;
	Bool is_null;
	GF_Err is_error;
} GF_FileOutCtx;


static GF_Err fileout_open_close(GF_FileOutCtx *ctx, const char *filename, const char *ext, u32 file_idx, Bool explicit_overwrite)
{
	char szName[GF_MAX_PATH], szFinalName[GF_MAX_PATH];
	if (ctx->file && !ctx->is_std) {
		gf_fclose(ctx->file);
	}
	ctx->file = NULL;
	if (!filename)
		return GF_OK;

	if (!strcmp(filename, "std")) ctx->is_std = GF_TRUE;
	else if (!strcmp(filename, "stdout")) ctx->is_std = GF_TRUE;
	else ctx->is_std = GF_FALSE;

	if (ctx->is_std) {
		ctx->file = stdout;
	} else {
		Bool append = ctx->append;
		if (ctx->dynext) {
			const char *has_ext = gf_file_ext_start(filename);

			strcpy(szName, filename);
			if (!has_ext && ext) {
				strcat(szName, ".");
				strcat(szName, ext);
			}
		} else {
			strcpy(szName, filename);
		}
		gf_filter_pid_resolve_file_template(ctx->pid, szName, szFinalName, file_idx, NULL);

		if (!gf_file_exists(szFinalName)) append = GF_FALSE;

		if (!ctx->ow && gf_file_exists(szFinalName) && !append) {
			char szRes[20];
			s32 res;

			fprintf(stderr, "File %s already exist - override (y/n/a) ?:", szFinalName);
			res = scanf("%20s", szRes);
			if (!res || (szRes[0] == 'n') || (szRes[0] == 'N')) {
				ctx->is_error = GF_IO_ERR;;
				return GF_IO_ERR;
			}
			if ((szRes[0] == 'a') || (szRes[0] == 'A')) ctx->ow = GF_TRUE;
		}

		ctx->file = gf_fopen(szFinalName, append ? "a+b" : "w+b");

		if (!strcmp(szFinalName, ctx->szFileName) && !ctx->append && ctx->nb_write && !explicit_overwrite) {
			GF_LOG(GF_LOG_WARNING, GF_LOG_MMIO, ("[FileOut] re-opening in write mode output file %s, content overwrite\n", filename));
		}
		strcpy(ctx->szFileName, szFinalName);
	}
	ctx->nb_write = 0;
	if (!ctx->file) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_MMIO, ("[FileOut] cannot open output file %s\n", ctx->szFileName));
		ctx->is_error = GF_IO_ERR;;
		return GF_IO_ERR;
	}
	return GF_OK;
}

static void fileout_setup_file(GF_FileOutCtx *ctx, Bool explicit_overwrite)
{
	const GF_PropertyValue *p, *ext;
	p = gf_filter_pid_get_property(ctx->pid, GF_PROP_PID_OUTPATH);
	ext = gf_filter_pid_get_property(ctx->pid, GF_PROP_PID_FILE_EXT);

	if (p && p->value.string) {
		fileout_open_close(ctx, p->value.string, (ext && ctx->dynext) ? ext->value.string : NULL, 0, explicit_overwrite);
	} else if (ctx->dynext) {
		p = gf_filter_pid_get_property(ctx->pid, GF_PROP_PCK_FILENUM);
		if (!p) {
			if (ext && ext->value.string) {
				fileout_open_close(ctx, ctx->dst, ext->value.string, 0, explicit_overwrite);
			}
		}
	} else {
		fileout_open_close(ctx, ctx->dst, NULL, 0, explicit_overwrite);
	}
}
static GF_Err fileout_configure_pid(GF_Filter *filter, GF_FilterPid *pid, Bool is_remove)
{
	const GF_PropertyValue *p;
	GF_FileOutCtx *ctx = (GF_FileOutCtx *) gf_filter_get_udta(filter);
	if (is_remove) {
		ctx->pid = NULL;
		fileout_open_close(ctx, NULL, NULL, 0, GF_FALSE);
		return GF_OK;
	}
	gf_filter_pid_check_caps(pid);

	if (!ctx->pid) {
		GF_FilterEvent evt;
		gf_filter_pid_init_play_event(pid, &evt, ctx->start, ctx->speed, "FileOut");
		gf_filter_pid_send_event(pid, &evt);
	}
	ctx->pid = pid;

	p = gf_filter_pid_get_property(pid, GF_PROP_PID_DISABLE_PROGRESSIVE);
	if (p && p->value.boolean) ctx->patch_blocks = GF_TRUE;

	return GF_OK;
}

static GF_Err fileout_initialize(GF_Filter *filter)
{
	char *ext;
	GF_FileOutCtx *ctx = (GF_FileOutCtx *) gf_filter_get_udta(filter);

	if (!ctx || !ctx->dst) return GF_OK;

	if (strnicmp(ctx->dst, "file:/", 6) && strstr(ctx->dst, "://"))  {
		gf_filter_setup_failure(filter, GF_NOT_SUPPORTED);
		return GF_NOT_SUPPORTED;
	}
	if (!stricmp(ctx->dst, "null") ) {
		ctx->is_null = GF_TRUE;
		//null and no format specified, we accept any kind
		if (!ctx->fext) {
			ctx->in_caps[0].code = GF_PROP_PID_STREAM_TYPE;
			ctx->in_caps[0].val = PROP_UINT(GF_STREAM_UNKNOWN);
			ctx->in_caps[0].flags = GF_CAPS_INPUT_EXCLUDED;
			gf_filter_override_caps(filter, ctx->in_caps, 1);
			return GF_OK;
		}
	}
	if (ctx->dynext) return GF_OK;

	if (ctx->fext) ext = ctx->fext;
	else {
		ext = strrchr(ctx->dst, '.');
		if (!ext) ext = ".*";
		ext += 1;
	}

	if (!ext && !ctx->mime) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_MMIO, ("[FileOut] No extension provided nor mime type for output file %s, cannot infer format\n", ctx->dst));
		return GF_NOT_SUPPORTED;
	}
	//static cap, streamtype = file
	ctx->in_caps[0].code = GF_PROP_PID_STREAM_TYPE;
	ctx->in_caps[0].val = PROP_UINT(GF_STREAM_FILE);
	ctx->in_caps[0].flags = GF_CAPS_INPUT_STATIC;

	if (ctx->mime) {
		ctx->in_caps[1].code = GF_PROP_PID_MIME;
		ctx->in_caps[1].val = PROP_NAME( ctx->mime );
		ctx->in_caps[1].flags = GF_CAPS_INPUT;
	} else {
		strncpy(ctx->szExt, ext, 9);
		strlwr(ctx->szExt);
		ctx->in_caps[1].code = GF_PROP_PID_FILE_EXT;
		ctx->in_caps[1].val = PROP_NAME( ctx->szExt );
		ctx->in_caps[1].flags = GF_CAPS_INPUT;
	}
	gf_filter_override_caps(filter, ctx->in_caps, 2);
	return GF_OK;
}

static void fileout_finalize(GF_Filter *filter)
{
	GF_FileOutCtx *ctx = (GF_FileOutCtx *) gf_filter_get_udta(filter);
	fileout_open_close(ctx, NULL, NULL, 0, GF_FALSE);
}

static GF_Err fileout_process(GF_Filter *filter)
{
	GF_FilterPacket *pck;
	const GF_PropertyValue *fname, *fext, *fnum, *p;
	Bool start, end;
	const char *pck_data;
	u32 pck_size, nb_write;
	u32 dep_flags;
	GF_FileOutCtx *ctx = (GF_FileOutCtx *) gf_filter_get_udta(filter);

	if (ctx->is_error) {
		GF_Err e = ctx->is_error;
		if (e != GF_EOS) {
			gf_filter_pid_set_discard(ctx->pid, GF_TRUE);
			ctx->is_error = GF_EOS;
		}
		return e;
	}

	pck = gf_filter_pid_get_packet(ctx->pid);
	if (!pck) {
		if (gf_filter_pid_is_eos(ctx->pid)) {
			if (gf_filter_reporting_enabled(filter)) {
				char szStatus[1024];
				snprintf(szStatus, 1024, "%s: done - wrote "LLU" bytes", gf_file_basename(ctx->szFileName), ctx->nb_write);
				gf_filter_update_status(filter, 10000, szStatus);
			}
			fileout_open_close(ctx, NULL, NULL, 0, GF_FALSE);
			return GF_EOS;
		}
		return GF_OK;
	}

	if (ctx->is_null) {
		gf_filter_pid_drop_packet(ctx->pid);
		return fileout_process(filter);
		return GF_OK;
	}

	gf_filter_pck_get_framing(pck, &start, &end);
	dep_flags = gf_filter_pck_get_dependency_flags(pck);
	//redundant packet, do not store
	if ((dep_flags & 0x3) == 1) {
		gf_filter_pid_drop_packet(ctx->pid);
		return GF_OK;
	}

	if (ctx->file && start && ctx->cat)
		start = GF_FALSE;

	if (start) {
		Bool explicit_overwrite = GF_FALSE;
		const char *name = NULL;
		fname = fext = NULL;
		//file num increased per packet, open new file
		fnum = gf_filter_pck_get_property(pck, GF_PROP_PCK_FILENUM);
		if (fnum) {
			fname = gf_filter_pid_get_property(ctx->pid, GF_PROP_PID_OUTPATH);
			fext = gf_filter_pid_get_property(ctx->pid, GF_PROP_PID_FILE_EXT);
			if (!fname) name = ctx->dst;
		}
		//filename change at packet start, open new file
		if (!fname) fname = gf_filter_pck_get_property(pck, GF_PROP_PCK_FILENAME);
		if (!fname) fname = gf_filter_pck_get_property(pck, GF_PROP_PID_OUTPATH);
		if (!fext) fext = gf_filter_pck_get_property(pck, GF_PROP_PID_FILE_EXT);
		if (fname) name = fname->value.string;

		if (end && gf_filter_pck_get_seek_flag(pck))
			explicit_overwrite = GF_TRUE;

		if (name) {
			fileout_open_close(ctx, name, fext ? fext->value.string : NULL, fnum ? fnum->value.uint : 0, explicit_overwrite);
		} else if (!ctx->file) {
			fileout_setup_file(ctx, explicit_overwrite);
		}
	}

	pck_data = gf_filter_pck_get_data(pck, &pck_size);
	if (ctx->file) {
		GF_FilterFrameInterface *hwf = gf_filter_pck_get_frame_interface(pck);
		if (pck_data) {
			if (ctx->patch_blocks && gf_filter_pck_get_seek_flag(pck)) {
				u64 bo = gf_filter_pck_get_byte_offset(pck);
				if (ctx->is_std) {
					GF_LOG(GF_LOG_ERROR, GF_LOG_AUDIO, ("[FileOut] Cannot patch file, output is stdout\n"));
				} else if (bo==GF_FILTER_NO_BO) {
					GF_LOG(GF_LOG_ERROR, GF_LOG_AUDIO, ("[FileOut] Cannot patch file, wrong byte offset\n"));
				} else {
					u64 pos = gf_ftell(ctx->file);
					gf_fseek(ctx->file, bo, SEEK_SET);
					nb_write = (u32) fwrite(pck_data, 1, pck_size, ctx->file);
					if (nb_write!=pck_size) {
						GF_LOG(GF_LOG_ERROR, GF_LOG_MMIO, ("[FileOut] Write error, wrote %d bytes but had %d to write\n", nb_write, pck_size));
					}
					gf_fseek(ctx->file, pos, SEEK_SET);
				}
			} else {
				nb_write = (u32) fwrite(pck_data, 1, pck_size, ctx->file);
				if (nb_write!=pck_size) {
					GF_LOG(GF_LOG_ERROR, GF_LOG_MMIO, ("[FileOut] Write error, wrote %d bytes but had %d to write\n", nb_write, pck_size));
				}
				ctx->nb_write += nb_write;
			}
		} else if (hwf) {
			u32 w, h, stride, stride_uv, pf;
			u32 nb_planes, uv_height;
			p = gf_filter_pid_get_property(ctx->pid, GF_PROP_PID_WIDTH);
			w = p ? p->value.uint : 0;
			p = gf_filter_pid_get_property(ctx->pid, GF_PROP_PID_HEIGHT);
			h = p ? p->value.uint : 0;
			p = gf_filter_pid_get_property(ctx->pid, GF_PROP_PID_PIXFMT);
			pf = p ? p->value.uint : 0;

			stride = stride_uv = 0;

			if (gf_pixel_get_size_info(pf, w, h, NULL, &stride, &stride_uv, &nb_planes, &uv_height) == GF_TRUE) {
				u32 i;
				for (i=0; i<nb_planes; i++) {
					u32 j, write_h, lsize;
					const u8 *out_ptr;
					u32 out_stride = i ? stride_uv : stride;
					GF_Err e = hwf->get_plane(hwf, i, &out_ptr, &out_stride);
					if (e) {
						GF_LOG(GF_LOG_ERROR, GF_LOG_MMIO, ("[FileOut] Failed to fetch plane data from hardware frame, cannot write\n"));
						break;
					}
					if (i) {
						write_h = uv_height;
						lsize = stride_uv;
					} else {
						write_h = h;
						lsize = stride;
					}
					for (j=0; j<write_h; j++) {
						nb_write = (u32) fwrite(out_ptr, 1, lsize, ctx->file);
						if (nb_write!=lsize) {
							GF_LOG(GF_LOG_ERROR, GF_LOG_MMIO, ("[FileOut] Write error, wrote %d bytes but had %d to write\n", nb_write, lsize));
						}
						ctx->nb_write += nb_write;
						out_ptr += out_stride;
					}

				}
			}
		} else {
			GF_LOG(GF_LOG_WARNING, GF_LOG_MMIO, ("[FileOut] No data associated with packet, cannot write\n"));
		}
	} else {
		GF_LOG(GF_LOG_ERROR, GF_LOG_MMIO, ("[FileOut] output file handle is not opened, discarding %d bytes\n", pck_size));
	}
	gf_filter_pid_drop_packet(ctx->pid);
	if (end && !ctx->cat) {
		fileout_open_close(ctx, NULL, NULL, 0, GF_FALSE);
	}
	if (gf_filter_reporting_enabled(filter)) {
		char szStatus[1024];
		snprintf(szStatus, 1024, "%s: wrote % 16"LLD_SUF" bytes", gf_file_basename(ctx->szFileName), (s64) ctx->nb_write);
		gf_filter_update_status(filter, -1, szStatus);
	}
	return GF_OK;
}

static GF_FilterProbeScore fileout_probe_url(const char *url, const char *mime)
{
	if (strstr(url, "://")) {
		if (!strnicmp(url, "file://", 7)) return GF_FPROBE_NOT_SUPPORTED;
	}
	return GF_FPROBE_MAYBE_SUPPORTED;
}


#define OFFS(_n)	#_n, offsetof(GF_FileOutCtx, _n)

static const GF_FilterArgs FileOutArgs[] =
{
	{ OFFS(dst), "location of destination file - see filter help ", GF_PROP_NAME, NULL, NULL, 0},
	{ OFFS(append), "open in append mode", GF_PROP_BOOL, "false", NULL, GF_FS_ARG_HINT_ADVANCED},
	{ OFFS(dynext), "indicate the file extension is set by filter chain, not dst", GF_PROP_BOOL, "false", NULL, GF_FS_ARG_HINT_ADVANCED},
	{ OFFS(start), "set playback start offset. Negative value means percent of media dur with -1 <=> dur", GF_PROP_DOUBLE, "0.0", NULL, 0},
	{ OFFS(speed), "set playback speed when vsync is on. If speed is negative and start is 0, start is set to -1", GF_PROP_DOUBLE, "1.0", NULL, GF_FS_ARG_HINT_EXPERT},
	{ OFFS(fext), "set extension for graph resolution, regardless of file extension", GF_PROP_NAME, NULL, NULL, GF_FS_ARG_HINT_ADVANCED},
	{ OFFS(cat), "cat each file of input pid rather than creating one file per filename", GF_PROP_BOOL, "false", NULL, GF_FS_ARG_HINT_ADVANCED},
	{ OFFS(ow), "overwrite output if existing", GF_PROP_BOOL, "true", NULL, 0},
	{0}
};

static const GF_FilterCapability FileOutCaps[] =
{
	CAP_UINT(GF_CAPS_INPUT,GF_PROP_PID_STREAM_TYPE, GF_STREAM_FILE),
	CAP_STRING(GF_CAPS_INPUT,GF_PROP_PID_MIME, "*"),
	{0},
	CAP_UINT(GF_CAPS_INPUT,GF_PROP_PID_STREAM_TYPE, GF_STREAM_FILE),
	CAP_STRING(GF_CAPS_INPUT,GF_PROP_PID_FILE_EXT, "*"),
};


GF_FilterRegister FileOutRegister = {
	.name = "fout",
	GF_FS_SET_DESCRIPTION("File output")
	GF_FS_SET_HELP("The file output filter can work as a null sink when its destination is \"null\", dropping all input packets. In this case it accepts ANY type of input pid, not just file ones.\n"\
	"In regular mode, the filter will dump to file incomming packets (stream type file), starting a new file for each packet having a __frame_start__ flag set, unless operating in [-cat]() mode.\n"\
	"The ouput file name can use gpac templating mechanism, see gpac help.")
	.private_size = sizeof(GF_FileOutCtx),
	.args = FileOutArgs,
	SETCAPS(FileOutCaps),
	.probe_url = fileout_probe_url,
	.initialize = fileout_initialize,
	.finalize = fileout_finalize,
	.configure_pid = fileout_configure_pid,
	.process = fileout_process
};


const GF_FilterRegister *fileout_register(GF_FilterSession *session)
{
	return &FileOutRegister;
}


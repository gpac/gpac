/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2018
 *					All rights reserved
 *
 *  This file is part of GPAC / generic pipe output filter
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

#ifdef WIN32

#else

#include <fcntl.h>
#include <unistd.h>

#ifndef __BEOS__
#include <errno.h>
#endif

#endif



typedef struct
{
	//options
	Double start, speed;
	char *dst, *mime, *ext;
	Bool dynext, mkp;


	//only one input pid
	GF_FilterPid *pid;

#ifdef WIN32
#else
	int fd;
#endif

	GF_FilterCapability in_caps[2];
	char szExt[10];
	char szFileName[GF_MAX_PATH];
	Bool owns_pipe;

} GF_PipeOutCtx;

const char *gf_errno_str(int errnoval);


static GF_Err pipeout_open_close(GF_PipeOutCtx *ctx, const char *filename, const char *ext, u32 file_idx, Bool explicit_overwrite)
{
	char szName[GF_MAX_PATH], szFinalName[GF_MAX_PATH];

	if (!filename) {
#ifdef WIN32
#else
		if (ctx->fd>=0) close(ctx->fd);
		ctx->fd = -1;
#endif
		return GF_OK;
	}

	if (!strncmp(filename, "pipe://", 7)) filename+=7;

	if (ctx->dynext) {
		Bool has_ext = (strchr(filename, '.')==NULL) ? GF_FALSE : GF_TRUE;

		strcpy(szName, filename);
		if (!has_ext && ext) {
			strcat(szName, ".");
			strcat(szName, ext);
		}
	} else {
		strcpy(szName, filename);
	}
	gf_filter_pid_resolve_file_template(ctx->pid, szName, szFinalName, file_idx);

	if (!strcmp(szFinalName, ctx->szFileName) 
#ifdef WIN32
#else
		&& ctx->fd>=0
#endif
		) return GF_OK;

#ifdef WIN32
#else
	if (ctx->fd>=0) close(ctx->fd);
	ctx->fd = -1;
#endif

	if (!gf_file_exists(szFinalName) && ctx->mkp) {
#ifdef WIN32
#elif defined(GPAC_CONFIG_DARWIN)
		mknod(szFinalName, S_IFIFO | 0666, 0);
#else
		mkfifo(szFinalName, 0666);
#endif
		ctx->owns_pipe = GF_TRUE;
	}

#ifdef WIN32
#else
	ctx->fd = open(szFinalName, O_WRONLY );

	if (ctx->fd<0) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_MMIO, ("[PipeOut] cannot open output pipe %s: %s\n", ctx->szFileName, gf_errno_str(errno)));
		return GF_IO_ERR;
	}
#endif

	strcpy(ctx->szFileName, szFinalName);
	return GF_OK;
}

static void pipeout_setup_file(GF_PipeOutCtx *ctx, Bool explicit_overwrite)
{
	const GF_PropertyValue *p;
	p = gf_filter_pid_get_property(ctx->pid, GF_PROP_PID_OUTPATH);
	if (p && p->value.string) {
		pipeout_open_close(ctx, p->value.string, NULL, 0, explicit_overwrite);
	} else if (ctx->dynext) {
		p = gf_filter_pid_get_property(ctx->pid, GF_PROP_PCK_FILENUM);
		if (!p) {
			p = gf_filter_pid_get_property(ctx->pid, GF_PROP_PID_FILE_EXT);
			if (p && p->value.string) {
				pipeout_open_close(ctx, ctx->dst, p->value.string, 0, explicit_overwrite);
			}
		}
	} else {
		pipeout_open_close(ctx, ctx->dst, NULL, 0, explicit_overwrite);
	}
}
static GF_Err pipeout_configure_pid(GF_Filter *filter, GF_FilterPid *pid, Bool is_remove)
{
	const GF_PropertyValue *p;
	GF_PipeOutCtx *ctx = (GF_PipeOutCtx *) gf_filter_get_udta(filter);
	if (is_remove) {
		ctx->pid = NULL;
		pipeout_open_close(ctx, NULL, NULL, 0, GF_FALSE);
		return GF_OK;
	}
	gf_filter_pid_check_caps(pid);

	if (!ctx->pid) {
		GF_FilterEvent evt;
		gf_filter_init_play_event(pid, &evt, ctx->start, ctx->speed, "PipeOut");
		gf_filter_pid_send_event(pid, &evt);
	}
	ctx->pid = pid;

	p = gf_filter_pid_get_property(pid, GF_PROP_PID_DISABLE_PROGRESSIVE);
	if (p && p->value.boolean) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_MMIO, ("[PipeOut] Block patching is currently not supported by pipe output\n"));
		return GF_NOT_SUPPORTED;
	}
	return GF_OK;
}

static GF_Err pipeout_initialize(GF_Filter *filter)
{
	char *ext;
	GF_PipeOutCtx *ctx = (GF_PipeOutCtx *) gf_filter_get_udta(filter);

	if (!ctx || !ctx->dst) return GF_OK;

	if (strnicmp(ctx->dst, "pipe://", 7) && strstr(ctx->dst, "://"))  {
		gf_filter_setup_failure(filter, GF_NOT_SUPPORTED);
		return GF_NOT_SUPPORTED;
	}
	if (ctx->dynext) return GF_OK;

	if (ctx->ext) ext = ctx->ext;
	else {
		ext = strrchr(ctx->dst, '.');
		if (ext) ext++;
	}

#ifdef WIN32
#else
	ctx->fd = -1;
#endif
	if (!ext && !ctx->mime) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_MMIO, ("[PipeOut] No extension provided nor mime type for output file %s, cannot infer format\n", ctx->dst));
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

static void pipeout_finalize(GF_Filter *filter)
{
	GF_PipeOutCtx *ctx = (GF_PipeOutCtx *) gf_filter_get_udta(filter);
	pipeout_open_close(ctx, NULL, NULL, 0, GF_FALSE);

	if (ctx->owns_pipe)
		gf_delete_file(ctx->szFileName);
}

static GF_Err pipeout_process(GF_Filter *filter)
{
	GF_FilterPacket *pck;
	const GF_PropertyValue *fname, *fext, *fnum, *p;
	Bool start, end;
	const char *pck_data;
	u32 pck_size;
	s32 nb_write;
	GF_PipeOutCtx *ctx = (GF_PipeOutCtx *) gf_filter_get_udta(filter);

	pck = gf_filter_pid_get_packet(ctx->pid);
	if (!pck) {
		if (gf_filter_pid_is_eos(ctx->pid)) {
			pipeout_open_close(ctx, NULL, NULL, 0, GF_FALSE);
			return GF_EOS;
		}
		return GF_OK;
	}

	gf_filter_pck_get_framing(pck, &start, &end);

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
			pipeout_open_close(ctx, name, fext ? fext->value.string : NULL, fnum ? fnum->value.uint : 0, explicit_overwrite);
		} else if (
#ifdef WIN32
			1
#else
			ctx->fd<0
#endif
			) {
			pipeout_setup_file(ctx, explicit_overwrite);
		}
	}

	pck_data = gf_filter_pck_get_data(pck, &pck_size);
	if (
#ifdef WIN32
		0
#else
		ctx->fd>=0
#endif
		) {
		GF_FilterHWFrame *hwf = gf_filter_pck_get_hw_frame(pck);
		if (pck_data) {
#ifdef WIN32
#else
			nb_write = (s32) write(ctx->fd, pck_data, pck_size);
#endif
			if (nb_write != pck_size) {
				GF_LOG(GF_LOG_ERROR, GF_LOG_MMIO, ("[PipeOut] Write error, wrote %d bytes but had %u to write: %s\n", nb_write, pck_size, gf_errno_str(errno) ));
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
			p = gf_filter_pid_get_property(ctx->pid, GF_PROP_PID_STRIDE);
			stride = p ? p->value.uint : 0;
			p = gf_filter_pid_get_property(ctx->pid, GF_PROP_PID_STRIDE_UV);
			stride_uv = p ? p->value.uint : 0;

			if (gf_pixel_get_size_info(pf, w, h, NULL, &stride, &stride_uv, &nb_planes, &uv_height) == GF_TRUE) {
				u32 i, bpp = gf_pixel_get_bytes_per_pixel(pf);
				for (i=0; i<nb_planes; i++) {
					u32 j, write_h, lsize;
					const u8 *out_ptr;
					u32 out_stride = i ? stride_uv : stride;
					GF_Err e = hwf->get_plane(hwf, i, &out_ptr, &out_stride);
					if (e) {
						GF_LOG(GF_LOG_ERROR, GF_LOG_MMIO, ("[PipeOut] Failed to fetch plane data from hardware frame, cannot write\n"));
						break;
					}
					write_h = h;
					if (i) write_h = uv_height;
					lsize = bpp * (i ? stride : stride_uv);
					for (j=0; j<write_h; j++) {
#ifdef WIN32
#else
						nb_write = (s32) write(ctx->fd, out_ptr, lsize);
#endif
						if (nb_write != lsize) {
							GF_LOG(GF_LOG_ERROR, GF_LOG_MMIO, ("[PipeOut] Write error, wrote %d bytes but had %u to write: %s\n", nb_write, lsize, gf_errno_str(errno) ));
						}
						out_ptr += out_stride;
					}
				}
			}
		} else {
			GF_LOG(GF_LOG_WARNING, GF_LOG_MMIO, ("[PipeOut] No data associated with packet, cannot write\n"));
		}
	} else {
		GF_LOG(GF_LOG_ERROR, GF_LOG_MMIO, ("[PipeOut] output file handle is not opened, discarding %d bytes\n", pck_size));
	}
	gf_filter_pid_drop_packet(ctx->pid);
	if (end) {
		pipeout_open_close(ctx, NULL, NULL, 0, GF_FALSE);
	}
	return GF_OK;
}

static GF_FilterProbeScore pipeout_probe_url(const char *url, const char *mime)
{
	if (!strnicmp(url, "pipe://", 7)) return GF_FPROBE_SUPPORTED;
	if (!strnicmp(url, "pipe:", 5)) return GF_FPROBE_SUPPORTED;
	return GF_FPROBE_NOT_SUPPORTED;
}


#define OFFS(_n)	#_n, offsetof(GF_PipeOutCtx, _n)

static const GF_FilterArgs PipeOutArgs[] =
{
	{ OFFS(dst), "location of destination file", GF_PROP_NAME, NULL, NULL, GF_FALSE},
	{ OFFS(ext), "indicates file extension of pipe data", GF_PROP_STRING, NULL, NULL, GF_FALSE},
	{ OFFS(mime), "indicates mime type of pipe data", GF_PROP_STRING, NULL, NULL, GF_FALSE},
	{ OFFS(dynext), "indicates the file extension is set by filter chain, not dst", GF_PROP_BOOL, "false", NULL, GF_FALSE},
	{ OFFS(start), "sets playback start offset, [-1, 0] means percent of media dur, eg -1 == dur", GF_PROP_DOUBLE, "0.0", NULL, GF_FALSE},
	{ OFFS(speed), "sets playback speed", GF_PROP_DOUBLE, "1.0", NULL, GF_FALSE},
	{0}
};

static const GF_FilterCapability PipeOutCaps[] =
{
	CAP_UINT(GF_CAPS_INPUT,GF_PROP_PID_STREAM_TYPE, GF_STREAM_FILE),
	CAP_STRING(GF_CAPS_INPUT,GF_PROP_PID_MIME, "*"),
	{0},
	CAP_UINT(GF_CAPS_INPUT,GF_PROP_PID_STREAM_TYPE, GF_STREAM_FILE),
	CAP_STRING(GF_CAPS_INPUT,GF_PROP_PID_FILE_EXT, "*"),
};


GF_FilterRegister PipeOutRegister = {
	.name = "pout",
	.description = "pipe output",
	.comment = "This filter handles generic output pipes (mono-directionnal) in blocking mode only.\n"\
		"Output pipes don't currently support non blocking mode\n"\
		"The assoicated protocol scheme is pipe:// when loaded as a generic output (eg, -o pipe://URL where URL is a relative or absolute pipe name)\n"\
		"Data format of the pipe must currently be specified using extension (either in filename or through ext option) or mime type\n"\
		"The pipe name indicated in dst can use template mechanisms from gpac, e.g. dst=pipe_$ServiceID$\n",
	.private_size = sizeof(GF_PipeOutCtx),
	.args = PipeOutArgs,
	SETCAPS(PipeOutCaps),
	.probe_url = pipeout_probe_url,
	.initialize = pipeout_initialize,
	.finalize = pipeout_finalize,
	.configure_pid = pipeout_configure_pid,
	.process = pipeout_process
};


const GF_FilterRegister *pipeout_register(GF_FilterSession *session)
{
	return &PipeOutRegister;
}


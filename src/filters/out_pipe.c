/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2018-2023
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

#ifndef GPAC_DISABLE_POUT

#ifdef WIN32

#include <windows.h>

#else

#include <fcntl.h>
#include <unistd.h>

#if defined(GPAC_CONFIG_LINUX) || defined(GPAC_CONFIG_EMSCRIPTEN)
#include <sys/types.h>
#include <sys/stat.h>
#endif

#ifndef __BEOS__
#include <errno.h>
#endif

#endif



typedef struct
{
	//options
	Double start, speed;
	char *dst, *mime, *ext;
	Bool dynext, mkp, ka, marker, force_close;
	u32 block_size;


	//only one input pid
	GF_FilterPid *pid;

#ifdef WIN32
	HANDLE pipe;
#else
	int fd;
#endif

	GF_FilterCapability in_caps[2];
	char szExt[10];
	char *szFileName;
	Bool owns_pipe;

} GF_PipeOutCtx;

const char *gf_errno_str(int errnoval);


static GF_Err pipeout_open_close(GF_PipeOutCtx *ctx, const char *filename, const char *ext, u32 file_idx, Bool explicit_overwrite)
{
	GF_Err e = GF_OK;
	char szName[GF_MAX_PATH], szFinalName[GF_MAX_PATH];

	if (!filename) {
#ifdef WIN32
		if (ctx->pipe != INVALID_HANDLE_VALUE) CloseHandle(ctx->pipe);
		ctx->pipe = INVALID_HANDLE_VALUE;
#else
		if (ctx->fd>=0) close(ctx->fd);
		ctx->fd = -1;
#endif
		ctx->force_close = GF_FALSE;
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
	gf_filter_pid_resolve_file_template(ctx->pid, szName, szFinalName, file_idx, NULL);

	if (ctx->szFileName && !strcmp(szFinalName, ctx->szFileName) 
#ifdef WIN32
		&& (ctx->pipe != INVALID_HANDLE_VALUE)
#else
		&& ctx->fd>=0
#endif
		) return GF_OK;

#ifdef WIN32
	if (ctx->pipe != INVALID_HANDLE_VALUE) CloseHandle(ctx->pipe);
	ctx->pipe = INVALID_HANDLE_VALUE;
	char szNamedPipe[GF_MAX_PATH];
	if (!strncmp(szFinalName, "\\\\", 2)) {
		strcpy(szNamedPipe, szFinalName);
	}
	else {
		strcpy(szNamedPipe, "\\\\.\\pipe\\gpac\\");
		strcat(szNamedPipe, szFinalName);
	}
	if (strchr(szFinalName, '/')) {
		u32 i, len = (u32)strlen(szNamedPipe);
		for (i = 0; i < len; i++) {
			if (szNamedPipe[i] == '/')
				szNamedPipe[i] = '\\';
		}
	}

	if (WaitNamedPipeA(szNamedPipe, 1) == FALSE) {
		if (!ctx->mkp) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_MMIO, ("[PipeOut] Failed to open %s: %d\n", szNamedPipe, GetLastError()));
			e = GF_URL_ERROR;
		}
		else {
			ctx->pipe = CreateNamedPipe(szNamedPipe, PIPE_ACCESS_OUTBOUND | FILE_FLAG_FIRST_PIPE_INSTANCE,
				PIPE_TYPE_BYTE | PIPE_WAIT,
				10,
				ctx->block_size,
				ctx->block_size,
				0,
				NULL);

			if (ctx->pipe == INVALID_HANDLE_VALUE) {
				GF_LOG(GF_LOG_ERROR, GF_LOG_MMIO, ("[PipeOut] Failed to create named pipe %s: %d\n", szNamedPipe, GetLastError()));
				e = GF_IO_ERR;
			}
			else {
				GF_LOG(GF_LOG_WARNING, GF_LOG_MMIO, ("[PipeOut] Waiting for client connection for %s, blocking\n", szNamedPipe));
				if (!ConnectNamedPipe(ctx->pipe, NULL) && (GetLastError() != ERROR_PIPE_CONNECTED)) {
					GF_LOG(GF_LOG_ERROR, GF_LOG_MMIO, ("[PipeOut] Failed to connect named pipe %s: %d\n", szNamedPipe, GetLastError()));
					e = GF_IO_ERR;
					CloseHandle(ctx->pipe);
					ctx->pipe = INVALID_HANDLE_VALUE;
				}
				else {
					ctx->owns_pipe = GF_TRUE;
				}
			}
		}
	}
	else {
		ctx->pipe = CreateFile(szNamedPipe, GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);
		if (ctx->pipe == INVALID_HANDLE_VALUE) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_MMIO, ("[PipeOut] Failed to open %s: %d\n", szNamedPipe, GetLastError()));
			e = GF_URL_ERROR;
		}
	}

#else
	if (ctx->fd>=0) close(ctx->fd);
	ctx->fd = -1;

	if (!gf_file_exists(szFinalName) && ctx->mkp) {
#ifdef GPAC_CONFIG_DARWIN
		mknod(szFinalName, S_IFIFO | 0666, 0);
#else
		mkfifo(szFinalName, 0666);
#endif
		ctx->owns_pipe = GF_TRUE;
	}

	ctx->fd = open(szFinalName, O_WRONLY );

	if (ctx->fd<0) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_MMIO, ("[PipeOut] Cannot open output pipe %s: %s\n", szFinalName, gf_errno_str(errno)));
		e = ctx->owns_pipe ? GF_IO_ERR : GF_URL_ERROR;
	}
#endif
	if (e) {
		return e;
	}
	if (ctx->szFileName) gf_free(ctx->szFileName);
	ctx->szFileName = gf_strdup(szFinalName);
	return GF_OK;
}

static GF_Err pipeout_setup_file(GF_PipeOutCtx *ctx, Bool explicit_overwrite)
{
	const GF_PropertyValue *p;
	p = gf_filter_pid_get_property(ctx->pid, GF_PROP_PID_OUTPATH);
	if (p && p->value.string) {
		return pipeout_open_close(ctx, p->value.string, NULL, 0, explicit_overwrite);
	} else if (ctx->dynext) {
		p = gf_filter_pid_get_property(ctx->pid, GF_PROP_PCK_FILENUM);
		if (!p) {
			p = gf_filter_pid_get_property(ctx->pid, GF_PROP_PID_FILE_EXT);
			if (p && p->value.string) {
				return pipeout_open_close(ctx, ctx->dst, p->value.string, 0, explicit_overwrite);
			}
		}
	} else {
		return pipeout_open_close(ctx, ctx->dst, NULL, 0, explicit_overwrite);
	}
	return GF_BAD_PARAM;
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
		gf_filter_pid_init_play_event(pid, &evt, ctx->start, ctx->speed, "PipeOut");
		gf_filter_pid_send_event(pid, &evt);
	}
	ctx->pid = pid;

	p = gf_filter_pid_get_property(pid, GF_PROP_PID_DISABLE_PROGRESSIVE);
	if (p && p->value.uint) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_MMIO, ("[PipeOut] Block patching is not supported by pipe output\n"));
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
		ext = gf_file_ext_start(ctx->dst);
		if (ext) ext++;
	}
	ctx->force_close = GF_TRUE;

#ifdef WIN32
	ctx->pipe = INVALID_HANDLE_VALUE;
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
		ctx->szExt[9] = 0;
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

	//pipe was not open, do an open/close - this allows signaling broken pipe when we had an error before opening
	if (ctx->force_close) {
		pipeout_open_close(ctx, ctx->dst, NULL, 0, GF_FALSE);
	}

	pipeout_open_close(ctx, NULL, NULL, 0, GF_FALSE);

	if (ctx->szFileName) {
		if (ctx->owns_pipe)
			gf_file_delete(ctx->szFileName);
		gf_free(ctx->szFileName);
	}
}

#define PIPE_FLUSH_MARKER	"GPACPIF"
static void pout_write_marker(GF_PipeOutCtx *ctx)
{
	if (ctx->marker) {
		u32 nb_write;
#ifdef WIN32
		if (! WriteFile(ctx->pipe, PIPE_FLUSH_MARKER, 8, (LPDWORD) &nb_write, NULL)) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_MMIO, ("[PipeOut] Failed to write marker: error %d\n", GetLastError() ));
			return;
		}
#else
		nb_write = (s32) write(ctx->fd, PIPE_FLUSH_MARKER, 8);
		if (nb_write != 8) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_MMIO, ("[PipeOut] Failed to write marker: %s\n", gf_errno_str(errno)));
			return;
		}
#endif
		GF_LOG(GF_LOG_INFO, GF_LOG_MMIO, ("[PipeOut] Wrote flush marker\n"));
	} else {
		GF_LOG(GF_LOG_DEBUG, GF_LOG_MMIO, ("[PipeOut] Got flush marker (write disabled)\n"));
	}
}

static GF_Err pipeout_process(GF_Filter *filter)
{
	GF_FilterPacket *pck;
	const GF_PropertyValue *fname, *p;
	Bool start, end, broken=GF_FALSE;
	GF_Err e = GF_OK;
	const char *pck_data;
	u32 pck_size;
	s32 nb_write;
	GF_PipeOutCtx *ctx = (GF_PipeOutCtx *) gf_filter_get_udta(filter);

	pck = gf_filter_pid_get_packet(ctx->pid);
	if (!pck) {
		if (gf_filter_pid_is_eos(ctx->pid)) {
			if (gf_filter_pid_is_flush_eos(ctx->pid)) {
				pout_write_marker(ctx);
			} else {
				pipeout_open_close(ctx, NULL, NULL, 0, GF_FALSE);
				return GF_EOS;
			}
		}
		return GF_OK;
	}

	gf_filter_pck_get_framing(pck, &start, &end);

	if (start) {
		const GF_PropertyValue *fext, *fnum;

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
			ctx->pipe==INVALID_HANDLE_VALUE
#else
			ctx->fd<0
#endif
			) {
			GF_Err e = pipeout_setup_file(ctx, explicit_overwrite);
			if (e) {
				gf_filter_setup_failure(filter, e);
				return e;
			}
		}
	}

	pck_data = gf_filter_pck_get_data(pck, &pck_size);
	if (
#ifdef WIN32
		ctx->pipe != INVALID_HANDLE_VALUE
#else
		ctx->fd>=0
#endif
		) {
		GF_FilterFrameInterface *hwf = gf_filter_pck_get_frame_interface(pck);
		if (pck_data) {
#ifdef WIN32
			if (! WriteFile(ctx->pipe, pck_data, pck_size, (LPDWORD) &nb_write, NULL)) {
				nb_write = 0;
				GF_LOG(GF_LOG_ERROR, GF_LOG_MMIO, ("[PipeOut] Write error, wrote %d bytes but had %u to write: error %d\n", nb_write, pck_size, GetLastError() ));

				if (GetLastError() == ERROR_BROKEN_PIPE)
					broken = GF_TRUE;
			}
#else
			nb_write = (s32) write(ctx->fd, pck_data, pck_size);
			if (nb_write != pck_size) {
				GF_LOG(GF_LOG_ERROR, GF_LOG_MMIO, ("[PipeOut] Write error, wrote %d bytes but had %u to write: %s\n", nb_write, pck_size, gf_errno_str(errno)));
				if (errno == EPIPE)
					broken = GF_TRUE;
			}
#endif
		} else if (hwf) {
			u32 w, h, stride, stride_uv, pf;
			u32 nb_planes, uv_height;
			p = gf_filter_pid_get_property(ctx->pid, GF_PROP_PID_WIDTH);
			w = p ? p->value.uint : 0;
			p = gf_filter_pid_get_property(ctx->pid, GF_PROP_PID_HEIGHT);
			h = p ? p->value.uint : 0;
			p = gf_filter_pid_get_property(ctx->pid, GF_PROP_PID_PIXFMT);
			pf = p ? p->value.uint : 0;

			//get stride/stride_uv with no padding
			stride = stride_uv = 0;
			if (gf_pixel_get_size_info(pf, w, h, NULL, &stride, &stride_uv, &nb_planes, &uv_height) == GF_TRUE) {
				u32 i;
				for (i=0; i<nb_planes; i++) {
					u32 j, write_h, lsize;
					const u8 *out_ptr;
					u32 out_stride = i ? stride_uv : stride;
					GF_Err e = hwf->get_plane(hwf, i, &out_ptr, &out_stride);
					if (e) {
						GF_LOG(GF_LOG_ERROR, GF_LOG_MMIO, ("[PipeOut] Failed to fetch plane data from hardware frame, cannot write\n"));
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
#ifdef WIN32
						if (!WriteFile(ctx->pipe, out_ptr, lsize, (LPDWORD) &nb_write, NULL)) {
							nb_write = 0;
							GF_LOG(GF_LOG_ERROR, GF_LOG_MMIO, ("[PipeOut] Write error, wrote %d bytes but had %u to write: %d\n", nb_write, pck_size, GetLastError()));

							if (GetLastError() == ERROR_BROKEN_PIPE)
								broken = GF_TRUE;
						}
#else
						nb_write = (s32) write(ctx->fd, out_ptr, lsize);
						if (nb_write != lsize) {
							GF_LOG(GF_LOG_ERROR, GF_LOG_MMIO, ("[PipeOut] Write error, wrote %d bytes but had %u to write: %s\n", nb_write, lsize, gf_errno_str(errno)));
							if (errno == EPIPE)
								broken = GF_TRUE;
						}
#endif
						out_ptr += out_stride;
					}
				}
			}
		} else {
			GF_LOG(GF_LOG_WARNING, GF_LOG_MMIO, ("[PipeOut] No data associated with packet, cannot write\n"));
		}
	} else {
		GF_LOG(GF_LOG_ERROR, GF_LOG_MMIO, ("[PipeOut] Output file handle is not opened, discarding %d bytes\n", pck_size));
	}
	gf_filter_pid_drop_packet(ctx->pid);
	GF_LOG(GF_LOG_DEBUG, GF_LOG_MMIO, ("[PipeOut] Wrote packet %d bytes\n", pck_size));

	if (broken && !ctx->ka) {
		//abort and send stop
		gf_filter_pid_set_discard(ctx->pid, GF_TRUE);
		GF_FilterEvent evt;
		GF_FEVT_INIT(evt, GF_FEVT_STOP, ctx->pid);
		gf_filter_pid_send_event(ctx->pid, &evt);
		end = GF_TRUE;
		e = GF_IO_ERR;
	}

	if (end) {
		pipeout_open_close(ctx, NULL, NULL, 0, GF_FALSE);
	}
	return e;
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
	{ OFFS(dst), "name of destination pipe", GF_PROP_NAME, NULL, NULL, 0},
	{ OFFS(ext), "indicate file extension of pipe data", GF_PROP_STRING, NULL, NULL, 0},
	{ OFFS(mime), "indicate mime type of pipe data", GF_PROP_STRING, NULL, NULL, 0},
	{ OFFS(dynext), "indicate the file extension is set by filter chain, not dst", GF_PROP_BOOL, "false", NULL, GF_FS_ARG_HINT_ADVANCED},
	{ OFFS(start), "set playback start offset. A negative value means percent of media duration with -1 equal to duration", GF_PROP_DOUBLE, "0.0", NULL, 0},
	{ OFFS(speed), "set playback speed. If negative and start is 0, start is set to -1", GF_PROP_DOUBLE, "1.0", NULL, 0},
	{ OFFS(mkp), "create pipe if not found", GF_PROP_BOOL, "false", NULL, 0 },
	{ OFFS(block_size), "buffer size used to write to pipe, Windows only", GF_PROP_UINT, "5000", NULL, GF_FS_ARG_HINT_ADVANCED },
	{ OFFS(ka), "keep pipe alive when broken pipe is detected", GF_PROP_BOOL, "false", NULL, GF_FS_ARG_HINT_ADVANCED},
	{ OFFS(marker), "inject marker upon pipeline flush events", GF_PROP_BOOL, "false", NULL, GF_FS_ARG_HINT_ADVANCED},
	{0}
};

static const GF_FilterCapability PipeOutCaps[] =
{
	CAP_UINT(GF_CAPS_INPUT,GF_PROP_PID_STREAM_TYPE, GF_STREAM_FILE),
	CAP_STRING(GF_CAPS_INPUT,GF_PROP_PID_FILE_EXT, "*"),
	CAP_STRING(GF_CAPS_INPUT,GF_PROP_PID_MIME, "*"),
};


GF_FilterRegister PipeOutRegister = {
	.name = "pout",
	GF_FS_SET_DESCRIPTION("pipe output")
	GF_FS_SET_HELP("This filter handles generic output pipes (mono-directional) in blocking mode only.\n"
		"Warning: Output pipes do not currently support non blocking mode.\n"
		"The associated protocol scheme is `pipe://` when loaded as a generic output (e.g. -o `pipe://URL` where URL is a relative or absolute pipe name).\n"
		"Data format of the pipe **shall** be specified using extension (either in filename or through [-ext]() option) or MIME type through [-mime]()\n"
		"The pipe name indicated in [-dst]() can use template mechanisms from gpac, e.g. `dst=pipe_$ServiceID$`\n"
		"\n"
		"On Windows hosts, the default pipe prefix is `\\\\.\\pipe\\gpac\\` if no prefix is set \n"
		"`dst=mypipe` resolves in `\\\\.\\pipe\\gpac\\mypipe`\n"
		"`dst=\\\\.\\pipe\\myapp\\mypipe` resolves in `\\\\.\\pipe\\myapp\\mypipe\n"
		"Any destination name starting with `\\\\` is used as is, with `\\` translated in `/`\n"
		"\n"
		"The pipe input can create the pipe if not found using [-mkp](). On windows hosts, this will create a pipe server.\n"
		"On non windows hosts, the created pipe will delete the pipe file upon filter destruction."
		"\n"
		"The pipe can be kept alive after a broken pipe is detected using [-ka](). This is typically used when clients crash/exits and resumes.\n"
		"When a keep-alive pipe is broken, input data is discarded and the filter will keep trashing data as fast as possible.\n"
		"It is therefore recommended to use this mode with real-time inputs (use a [reframer](reframer) if needed)."
		"\n"
		"If [-marker]() is set, the string `GPACPIF` (8 bytes including 0-terminator) will be written to the pipe at each detected pipeline flush.\n"
		"Pipeline flushing is currently triggered by DASH segment end or ISOBMF fragment end.\n"
	"")
	.private_size = sizeof(GF_PipeOutCtx),
	.args = PipeOutArgs,
	SETCAPS(PipeOutCaps),
	.probe_url = pipeout_probe_url,
	.initialize = pipeout_initialize,
	.finalize = pipeout_finalize,
	.configure_pid = pipeout_configure_pid,
	.process = pipeout_process,
	.flags = GF_FS_REG_TEMP_INIT
};


const GF_FilterRegister *pout_register(GF_FilterSession *session)
{
	if (gf_opts_get_bool("temp", "get_proto_schemes")) {
		gf_opts_set_key("temp_out_proto", PipeOutRegister.name, "pipe");
	}
	return &PipeOutRegister;
}
#else
const GF_FilterRegister *pout_register(GF_FilterSession *session)
{
	return NULL;
}
#endif // GPAC_DISABLE_POUT


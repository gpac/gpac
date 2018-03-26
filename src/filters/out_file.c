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
	char *dst;
	Bool append, dynext;

	//only one output pid declared for now
	GF_FilterPid *pid;

	FILE *file;
	Bool is_std;
	u32 nb_write;
} GF_FileOutCtx;

static GF_Err fileout_open_close(GF_FileOutCtx *ctx, const char *filename, const char *ext, u32 file_idx)
{
	Bool had_file = GF_FALSE;
	if (ctx->file && !ctx->is_std) {
		had_file = GF_TRUE;
		gf_fclose(ctx->file);
	}
	ctx->file = NULL;
	if (!filename) return GF_OK;

	if (!strcmp(filename, "std")) ctx->is_std = GF_TRUE;
	else if (!strcmp(filename, "stdout")) ctx->is_std = GF_TRUE;
	else ctx->is_std = GF_FALSE;

	if (ctx->is_std) {
		ctx->file = stdout;
	} else {
		if (ctx->dynext) {
			Bool has_ext = (strchr(filename, '.')==NULL) ? GF_FALSE : GF_TRUE;
			char szName[GF_MAX_PATH];

			if (file_idx && (strstr(filename, "\%d") || strstr(filename, "\%0") ) ) {
				sprintf(szName, filename, file_idx);
				had_file = GF_FALSE;
			} else if (file_idx) {
				sprintf(szName, "%s_%d", filename, file_idx);
				had_file = GF_FALSE;
			} else {
				strcpy(szName, filename);
			}
			if (!has_ext && ext) {
				strcat(szName, ".");
				strcat(szName, ext);
			}

			ctx->file = gf_fopen(szName, ctx->append ? "a+" : "w");
		} else {
			ctx->file = gf_fopen(filename, ctx->append ? "a+" : "w");
		}
		if (had_file && !ctx->append && ctx->nb_write) {
			GF_LOG(GF_LOG_WARNING, GF_LOG_MMIO, ("[FileOut] re-opening in write mode output file %s, content overwrite\n", filename));

		}
	}
	ctx->nb_write = 0;
	if (!ctx->file) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_MMIO, ("[FileOut] cannot open output file %s\n", filename));
		return GF_IO_ERR;
	}
	return GF_OK;
}

static GF_Err fileout_configure_pid(GF_Filter *filter, GF_FilterPid *pid, Bool is_remove)
{
	const GF_PropertyValue *p;
	GF_FileOutCtx *ctx = (GF_FileOutCtx *) gf_filter_get_udta(filter);
	if (is_remove) {
		ctx->pid = NULL;
		fileout_open_close(ctx, NULL, NULL, 0);
	}
	gf_filter_pid_check_caps(pid);

	p = gf_filter_pid_get_property(pid, GF_PROP_PID_OUTPATH);
	if (p && p->value.string) {
		fileout_open_close(ctx, p->value.string, NULL, 0);
	} else {
		p = gf_filter_pid_get_property(pid, GF_PROP_PCK_FILENUM);
		if (!p) {
			p = gf_filter_pid_get_property(pid, GF_PROP_PID_FILE_EXT);
			if (p && p->value.string) {
				fileout_open_close(ctx, ctx->dst, p->value.string, 0);
			}
		}
	}
	if (!ctx->pid) {
		GF_FilterEvent evt;
		GF_FEVT_INIT(evt, GF_FEVT_PLAY, pid);
		gf_filter_pid_send_event(pid, &evt);
	}
	ctx->pid = pid;
	return GF_OK;
}

static GF_Err fileout_initialize(GF_Filter *filter)
{
	GF_FileOutCtx *ctx = (GF_FileOutCtx *) gf_filter_get_udta(filter);

	if (!ctx || !ctx->dst) return GF_OK;

	if (strnicmp(ctx->dst, "file:/", 6) && strstr(ctx->dst, "://"))  {
		gf_filter_setup_failure(filter, GF_NOT_SUPPORTED);
		return GF_NOT_SUPPORTED;
	}
	if (ctx->dynext) return GF_OK;
	fileout_open_close(ctx, ctx->dst, NULL, 0);
	return GF_OK;
}

static void fileout_finalize(GF_Filter *filter)
{
	GF_FileOutCtx *ctx = (GF_FileOutCtx *) gf_filter_get_udta(filter);
	fileout_open_close(ctx, NULL, NULL, 0);
}

static GF_Err fileout_process(GF_Filter *filter)
{
	GF_FilterPacket *pck;
	const GF_PropertyValue *fname, *fext, *fnum, *p;
	Bool start, end;
	const char *pck_data;
	u32 pck_size, nb_write;
	GF_FileOutCtx *ctx = (GF_FileOutCtx *) gf_filter_get_udta(filter);

	pck = gf_filter_pid_get_packet(ctx->pid);
	if (!pck) {
		if (gf_filter_pid_is_eos(ctx->pid)) {
			fileout_open_close(ctx, NULL, NULL, 0);
			return GF_EOS;
		}
		return GF_OK;
	}

	gf_filter_pck_get_framing(pck, &start, &end);

	if (start) {
		const char *name = NULL;
		fname = fext = NULL;
		//file num increased per packet, open new file
		fnum = gf_filter_pck_get_property(pck, GF_PROP_PCK_FILENUM);
		if (fnum) {
			fname = gf_filter_pid_get_property(ctx->pid, GF_PROP_PID_OUTPATH);
			fext = gf_filter_pid_get_property(ctx->pid, GF_PROP_PID_FILE_EXT);
			if (!fname) name = ctx->dst;
		}
		if (!fname) fname = gf_filter_pck_get_property(pck, GF_PROP_PID_OUTPATH);
		if (!fext) fext = gf_filter_pck_get_property(pck, GF_PROP_PID_FILE_EXT);
		if (fname) name = fname->value.string;

		if (name) {
			fileout_open_close(ctx, name, fext ? fext->value.string : NULL, fnum ? fnum->value.uint : 0);
		}
	}

	pck_data = gf_filter_pck_get_data(pck, &pck_size);
	if (ctx->file) {
		GF_FilterHWFrame *hwf = gf_filter_pck_get_hw_frame(pck);
		if (pck_data) {
			nb_write = fwrite(pck_data, 1, pck_size, ctx->file);
			if (nb_write!=pck_size) {
				GF_LOG(GF_LOG_ERROR, GF_LOG_MMIO, ("[FileOut] Write error, wrote %d bytes but had %d to write\n", nb_write, pck_size));
			}
			ctx->nb_write += nb_write;
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
						GF_LOG(GF_LOG_ERROR, GF_LOG_MMIO, ("[FileOut] Failed to fetch plane data from hardware frame, cannot write\n"));
						break;
					}
					write_h = h;
					if (i) write_h = uv_height;
					lsize = bpp * (i ? stride : stride_uv);
					for (j=0; j<write_h; j++) {
						nb_write = fwrite(out_ptr, 1, lsize, ctx->file);
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
	if (end) {
		fileout_open_close(ctx, NULL, NULL, 0);
	}
	return GF_OK;
}



#define OFFS(_n)	#_n, offsetof(GF_FileOutCtx, _n)

static const GF_FilterArgs FileOutArgs[] =
{
	{ OFFS(dst), "location of source content", GF_PROP_NAME, NULL, NULL, GF_FALSE},
	{ OFFS(append), "open in append mode", GF_PROP_BOOL, "false", NULL, GF_FALSE},
	{ OFFS(dynext), "indicates the file extension is set by filter chain, not dst", GF_PROP_BOOL, "false", NULL, GF_FALSE},
	{}
};

static const GF_FilterCapability FileOutInputs[] =
{
	CAP_INC_UINT(GF_PROP_PID_STREAM_TYPE, GF_STREAM_FILE),
};


GF_FilterRegister FileOutRegister = {
	.name = "fileout",
	.description = "Generic File Output",
	.private_size = sizeof(GF_FileOutCtx),
	.args = FileOutArgs,
	INCAPS(FileOutInputs),
	.initialize = fileout_initialize,
	.finalize = fileout_finalize,
	.configure_pid = fileout_configure_pid,
	.process = fileout_process
};


const GF_FilterRegister *fileout_register(GF_FilterSession *session)
{
	return &FileOutRegister;
}


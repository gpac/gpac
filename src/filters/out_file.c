/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2017-2022
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

enum
{
	FOUT_CAT_NONE = 0,
	FOUT_CAT_AUTO,
	FOUT_CAT_ALL
};

typedef struct
{
	//options
	Double start, speed;
	char *dst, *mime, *ext;
	Bool append, dynext, ow, redund, noinitraw;
	u32 cat;
	u32 mvbk;

	//only one input pid
	GF_FilterPid *pid;

	FILE *file;
	Bool is_std;
	u64 nb_write;
	Bool use_templates;
	GF_FilterCapability in_caps[2];
	char szExt[10];
	char szFileName[GF_MAX_PATH];

	Bool patch_blocks;
	Bool is_null;
	GF_Err error;
	u32 dash_mode;
	u64 offset_at_seg_start;
	const char *original_url;
	GF_FileIO *gfio_ref;

	FILE *hls_chunk;
} GF_FileOutCtx;

#ifdef WIN32
#include <io.h>
#include <fcntl.h>
#endif //WIN32

static void fileout_close_hls_chunk(GF_FileOutCtx *ctx, Bool final_flush)
{
	if (!ctx->hls_chunk) return;
	gf_fclose(ctx->hls_chunk);
	ctx->hls_chunk = NULL;
}

static GF_Err fileout_open_close(GF_FileOutCtx *ctx, const char *filename, const char *ext, u32 file_idx, Bool explicit_overwrite, char *file_suffix)
{
	if (ctx->file && !ctx->is_std) {
		GF_LOG(GF_LOG_INFO, GF_LOG_MMIO, ("[FileOut] closing output file %s\n", ctx->szFileName));
		gf_fclose(ctx->file);

		fileout_close_hls_chunk(ctx, GF_FALSE);
	}
	ctx->file = NULL;

	if (!filename)
		return GF_OK;

	if (!strcmp(filename, "std")) ctx->is_std = GF_TRUE;
	else if (!strcmp(filename, "stdout")) ctx->is_std = GF_TRUE;
	else ctx->is_std = GF_FALSE;

	if (ctx->is_std) {
		ctx->file = stdout;
#ifdef WIN32
		_setmode(_fileno(stdout), _O_BINARY);
#endif

	} else {
		char szFinalName[GF_MAX_PATH];
		Bool append = ctx->append;
		const char *url = filename;

		if (!strncmp(filename, "gfio://", 7))
			url = gf_fileio_translate_url(filename);

		if (ctx->dynext) {
			const char *has_ext = gf_file_ext_start(url);

			strcpy(szFinalName, url);
			if (!has_ext && ext) {
				strcat(szFinalName, ".");
				strcat(szFinalName, ext);
			}
		} else {
			strcpy(szFinalName, url);
		}

		if (ctx->use_templates) {
			GF_Err e;
			char szName[GF_MAX_PATH];
			assert(ctx->dst);
			if (!strcmp(filename, ctx->dst)) {
				strcpy(szName, szFinalName);
				e = gf_filter_pid_resolve_file_template(ctx->pid, szName, szFinalName, file_idx, file_suffix);
			} else {
				char szFileName[GF_MAX_PATH];
				strcpy(szFileName, szFinalName);
				strcpy(szName, ctx->dst);
				e = gf_filter_pid_resolve_file_template_ex(ctx->pid, szName, szFinalName, file_idx, file_suffix, szFileName);
			}
			if (e) {
				return ctx->error = e;
			}
		}

		if (!gf_file_exists(szFinalName)) append = GF_FALSE;

		if (!strcmp(szFinalName, ctx->szFileName) && (ctx->cat==FOUT_CAT_AUTO))
			append = GF_TRUE;

		if (!ctx->ow && gf_file_exists(szFinalName) && !append) {
			char szRes[21];
			s32 res;

			fprintf(stderr, "File %s already exist - override (y/n/a) ?:", szFinalName);
			res = scanf("%20s", szRes);
			if (!res || (szRes[0] == 'n') || (szRes[0] == 'N')) {
				return ctx->error = GF_IO_ERR;
			}
			if ((szRes[0] == 'a') || (szRes[0] == 'A')) ctx->ow = GF_TRUE;
		}

		GF_LOG(GF_LOG_INFO, GF_LOG_MMIO, ("[FileOut] opening output file %s\n", szFinalName));
		ctx->file = gf_fopen_ex(szFinalName, ctx->original_url, append ? "a+b" : "w+b", GF_FALSE);

		if (!strcmp(szFinalName, ctx->szFileName) && !append && ctx->nb_write && !explicit_overwrite) {
			GF_LOG(GF_LOG_WARNING, GF_LOG_MMIO, ("[FileOut] re-opening in write mode output file %s, content overwrite (use `cat` option to enable append)\n", szFinalName));
		}
		strcpy(ctx->szFileName, szFinalName);
	}
	ctx->nb_write = 0;
	if (!ctx->file) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_MMIO, ("[FileOut] cannot open output file %s\n", ctx->szFileName));
		return ctx->error = GF_IO_ERR;;
	}

	return GF_OK;
}

static void fileout_setup_file(GF_FileOutCtx *ctx, Bool explicit_overwrite)
{
	const char *dst = ctx->dst;
	const GF_PropertyValue *p, *ext;

	p = gf_filter_pid_get_property(ctx->pid, GF_PROP_PID_OUTPATH);
	ext = gf_filter_pid_get_property(ctx->pid, GF_PROP_PID_FILE_EXT);

	if (p && p->value.string) {
		fileout_open_close(ctx, p->value.string, (ext && ctx->dynext) ? ext->value.string : NULL, 0, explicit_overwrite, NULL);
		return;
	}
	if (!dst) {
		p = gf_filter_pid_get_property(ctx->pid, GF_PROP_PID_FILEPATH);
		if (p && p->value.string) {
			dst = p->value.string;
			char *sep = strstr(dst, "://");
			if (sep) {
				dst = strchr(sep+3, '/');
				if (!dst) return;
			} else {
				if (!strncmp(dst, "./", 2)) dst+= 2;
				else if (!strncmp(dst, ".\\", 2)) dst+= 2;
				else if (!strncmp(dst, "../", 3)) dst+= 3;
				else if (!strncmp(dst, "..\\", 3)) dst+= 3;
			}
		}
	}
	if (ctx->dynext) {
		p = gf_filter_pid_get_property(ctx->pid, GF_PROP_PCK_FILENUM);
		if (!p) {
			if (ext && ext->value.string) {
				fileout_open_close(ctx, dst, ext->value.string, 0, explicit_overwrite, NULL);
			}
		}
	} else if (ctx->dst) {
		fileout_open_close(ctx, ctx->dst, NULL, 0, explicit_overwrite, NULL);
	} else {
		p = gf_filter_pid_get_property(ctx->pid, GF_PROP_PID_FILEPATH);
		if (!p) p = gf_filter_pid_get_property(ctx->pid, GF_PROP_PID_URL);
		if (p && p->value.string)
			fileout_open_close(ctx, p->value.string, NULL, 0, explicit_overwrite, NULL);
	}
}
static GF_Err fileout_configure_pid(GF_Filter *filter, GF_FilterPid *pid, Bool is_remove)
{
	const GF_PropertyValue *p;
	GF_FileOutCtx *ctx = (GF_FileOutCtx *) gf_filter_get_udta(filter);
	if (is_remove) {
		ctx->pid = NULL;
		fileout_open_close(ctx, NULL, NULL, 0, GF_FALSE, NULL);
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
	if (p && p->value.uint) ctx->patch_blocks = GF_TRUE;

	p = gf_filter_pid_get_property(pid, GF_PROP_PID_DASH_MODE);
	if (p && p->value.uint) ctx->dash_mode = 1;

	ctx->error = GF_OK;
	return GF_OK;
}

static GF_Err fileout_initialize(GF_Filter *filter)
{
	char *ext=NULL, *sep;
	const char *dst;
	GF_FileOutCtx *ctx = (GF_FileOutCtx *) gf_filter_get_udta(filter);

	if (!ctx || !ctx->dst) return GF_OK;

	if (!ctx->mvbk)
		ctx->mvbk = 1;

	if (strnicmp(ctx->dst, "file:/", 6) && strnicmp(ctx->dst, "gfio:/", 6) && strstr(ctx->dst, "://"))  {
		gf_filter_setup_failure(filter, GF_NOT_SUPPORTED);
		return GF_NOT_SUPPORTED;
	}
	if (!stricmp(ctx->dst, "null") ) {
		ctx->is_null = GF_TRUE;
		//null and no format specified, we accept any kind
		if (!ctx->ext) {
			ctx->in_caps[0].code = GF_PROP_PID_STREAM_TYPE;
			ctx->in_caps[0].val = PROP_UINT(GF_STREAM_UNKNOWN);
			ctx->in_caps[0].flags = GF_CAPS_INPUT_EXCLUDED;
			gf_filter_override_caps(filter, ctx->in_caps, 1);
			return GF_OK;
		}
	}
	if (!strncmp(ctx->dst, "gfio://", 7)) {
		GF_Err e;
		ctx->gfio_ref = gf_fileio_open_url(gf_fileio_from_url(ctx->dst), NULL, "ref", &e);
		if (!ctx->gfio_ref) {
			gf_filter_setup_failure(filter, e);
			return e;
		}
		dst = gf_fileio_translate_url(ctx->dst);
		ctx->original_url = ctx->dst;
	} else {
		dst = ctx->dst;
	}

	sep = dst ? strchr(dst, '$') : NULL;
	if (sep) {
		sep = strchr(sep+1, '$');
		if (sep) ctx->use_templates = GF_TRUE;
	}

	if (ctx->dynext) return GF_OK;

	if (ctx->ext) ext = ctx->ext;
	else if (dst) {
		ext = gf_file_ext_start(dst);
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
		ctx->szExt[9] = 0;
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
	GF_Err e;
	GF_FileOutCtx *ctx = (GF_FileOutCtx *) gf_filter_get_udta(filter);

	fileout_close_hls_chunk(ctx, GF_TRUE);

	fileout_open_close(ctx, NULL, NULL, 0, GF_FALSE, NULL);
	if (ctx->gfio_ref)
		gf_fileio_open_url((GF_FileIO *)ctx->gfio_ref, NULL, "unref", &e);
}

static GF_Err fileout_process(GF_Filter *filter)
{
	GF_FilterPacket *pck;
	const GF_PropertyValue *fname, *p;
	Bool start, end;
	const u8 *pck_data;
	u32 pck_size, nb_write;
	GF_FileOutCtx *ctx = (GF_FileOutCtx *) gf_filter_get_udta(filter);

	if (ctx->error)
		return ctx->error;

	pck = gf_filter_pid_get_packet(ctx->pid);
	if (!pck) {
		if (gf_filter_pid_is_eos(ctx->pid)) {
			if (gf_filter_reporting_enabled(filter)) {
				char szStatus[1024];
				snprintf(szStatus, 1024, "%s: done - wrote "LLU" bytes", gf_file_basename(ctx->szFileName), ctx->nb_write);
				gf_filter_update_status(filter, 10000, szStatus);
			}

			if (ctx->dash_mode && ctx->file) {
				GF_FilterEvent evt;
				GF_FEVT_INIT(evt, GF_FEVT_SEGMENT_SIZE, ctx->pid);
				evt.seg_size.seg_url = NULL;

				if (ctx->dash_mode==1) {
					evt.seg_size.is_init = 1;
					ctx->dash_mode = 2;
					evt.seg_size.media_range_start = 0;
					evt.seg_size.media_range_end = 0;
					gf_filter_pid_send_event(ctx->pid, &evt);
				} else {
					evt.seg_size.is_init = 0;
					evt.seg_size.media_range_start = ctx->offset_at_seg_start;
					evt.seg_size.media_range_end = gf_ftell(ctx->file)-1;
					gf_filter_pid_send_event(ctx->pid, &evt);
				}
			}
			fileout_open_close(ctx, NULL, NULL, 0, GF_FALSE, NULL);
			return GF_EOS;
		}
		return GF_OK;
	}

	gf_filter_pck_get_framing(pck, &start, &end);
	if (!ctx->redund && !ctx->dash_mode) {
		u32 dep_flags = gf_filter_pck_get_dependency_flags(pck);
		//redundant packet, do not store
		if ((dep_flags & 0x3) == 1) {
			gf_filter_pid_drop_packet(ctx->pid);
			return GF_OK;
		}
	}

	if (ctx->is_null) {
		if (start) {
			u32 fnum=0;
			const char *filename=NULL;
			p = gf_filter_pck_get_property(pck, GF_PROP_PCK_FILENUM);
			if (p) fnum = p->value.uint;
			p = gf_filter_pid_get_property(ctx->pid, GF_PROP_PID_URL);
			if (!p) p = gf_filter_pid_get_property(ctx->pid, GF_PROP_PID_OUTPATH);
			if (!p) p = gf_filter_pck_get_property(pck, GF_PROP_PCK_FILENAME);
			if (p) filename = p->value.string;
			if (filename) {
				strcpy(ctx->szFileName, filename);
			} else {
				sprintf(ctx->szFileName, "%d", fnum);
			}
			GF_LOG(GF_LOG_INFO, GF_LOG_MMIO, ("[FileOut] null open (file name is %s)\n", ctx->szFileName));
		}
		if (end) {
			GF_LOG(GF_LOG_INFO, GF_LOG_MMIO, ("[FileOut] null close (file name was %s)\n", ctx->szFileName));
		}
		gf_filter_pid_drop_packet(ctx->pid);
		return fileout_process(filter);
	}

	if (ctx->file && start && (ctx->cat==FOUT_CAT_ALL))
		start = GF_FALSE;

	if (ctx->dash_mode) {
		p = gf_filter_pck_get_property(pck, GF_PROP_PCK_FILENUM);
		if (p) {
			GF_FilterEvent evt;

			GF_FEVT_INIT(evt, GF_FEVT_SEGMENT_SIZE, ctx->pid);
			evt.seg_size.seg_url = NULL;

			if (ctx->dash_mode==1) {
				evt.seg_size.is_init = 1;
				ctx->dash_mode = 2;
				evt.seg_size.media_range_start = 0;
				evt.seg_size.media_range_end = 0;
				gf_filter_pid_send_event(ctx->pid, &evt);
			} else {
				evt.seg_size.is_init = 0;
				evt.seg_size.media_range_start = ctx->offset_at_seg_start;
				evt.seg_size.media_range_end = gf_ftell(ctx->file)-1;
				ctx->offset_at_seg_start = evt.seg_size.media_range_end+1;
				gf_filter_pid_send_event(ctx->pid, &evt);
			}
			if ( gf_filter_pck_get_property(pck, GF_PROP_PCK_FILENAME))
				start = GF_TRUE;
		}
	}

	if (start) {
		const GF_PropertyValue *ext, *fnum, *fsuf;
		Bool explicit_overwrite = GF_FALSE;
		const char *name = NULL;
		fname = ext = NULL;
		//file num increased per packet, open new file
		fnum = gf_filter_pck_get_property(pck, GF_PROP_PCK_FILENUM);
		if (fnum) {
			fname = gf_filter_pid_get_property(ctx->pid, GF_PROP_PID_OUTPATH);
			ext = gf_filter_pid_get_property(ctx->pid, GF_PROP_PID_FILE_EXT);
			if (!fname) name = ctx->dst;
		}
		//filename change at packet start, open new file
		if (!fname) fname = gf_filter_pck_get_property(pck, GF_PROP_PCK_FILENAME);
		if (!fname) fname = gf_filter_pck_get_property(pck, GF_PROP_PID_OUTPATH);
		if (!ext) ext = gf_filter_pck_get_property(pck, GF_PROP_PID_FILE_EXT);
		if (fname) name = fname->value.string;

		fsuf = gf_filter_pck_get_property(pck, GF_PROP_PCK_FILESUF);

		if (end && gf_filter_pck_get_seek_flag(pck))
			explicit_overwrite = GF_TRUE;

		if (name) {
			fileout_open_close(ctx, name, ext ? ext->value.string : NULL, fnum ? fnum->value.uint : 0, explicit_overwrite, fsuf ? fsuf->value.string : NULL);
		} else if (!ctx->file && !ctx->noinitraw) {
			fileout_setup_file(ctx, explicit_overwrite);
		}
	}

	p = gf_filter_pck_get_property(pck, GF_PROP_PCK_HLS_FRAG_NUM);
	if (p) {
		char szHLSChunk[GF_MAX_PATH+21];
		snprintf(szHLSChunk, GF_MAX_PATH+20, "%s.%d", ctx->szFileName, p->value.uint);
		if (ctx->hls_chunk) gf_fclose(ctx->hls_chunk);
		ctx->hls_chunk = gf_fopen_ex(szHLSChunk, ctx->original_url, "w+b", GF_FALSE);
	}

	pck_data = gf_filter_pck_get_data(pck, &pck_size);
	if (ctx->file) {
		GF_FilterFrameInterface *hwf = gf_filter_pck_get_frame_interface(pck);
		if (pck_data) {
			if (ctx->patch_blocks && gf_filter_pck_get_seek_flag(pck)) {
				u64 bo = gf_filter_pck_get_byte_offset(pck);
				if (ctx->is_std) {
					GF_LOG(GF_LOG_ERROR, GF_LOG_MMIO, ("[FileOut] Cannot patch file, output is stdout\n"));
				} else if (bo==GF_FILTER_NO_BO) {
					GF_LOG(GF_LOG_ERROR, GF_LOG_MMIO, ("[FileOut] Cannot patch file, wrong byte offset\n"));
				} else {
					u32 ilaced = gf_filter_pck_get_interlaced(pck);
					u64 pos = ctx->nb_write;
					if (ctx->file) {
						assert( ctx->nb_write == gf_ftell(ctx->file) );
					}

					//we are inserting a block: write dummy bytes at end and move bytes
					if (ilaced) {
						u8 *block;
						u64 cur_r, cur_w;
						nb_write = (u32) gf_fwrite(pck_data, pck_size, ctx->file);

						if (nb_write!=pck_size) {
							GF_LOG(GF_LOG_ERROR, GF_LOG_MMIO, ("[FileOut] Write error, wrote %d bytes but had %d to write\n", nb_write, pck_size));
						}
						cur_w = gf_ftell(ctx->file);

						gf_fseek(ctx->file, pos, SEEK_SET);

						cur_r = pos;
						pos = cur_w;
						block = gf_malloc(ctx->mvbk);
						if (!block) {
							GF_LOG(GF_LOG_ERROR, GF_LOG_MMIO, ("[FileOut] unable to allocate block of %d bytes\n", ctx->mvbk));
						} else {
							while (cur_r > bo) {
								u32 move_bytes = ctx->mvbk;
								if (cur_r - bo < move_bytes)
									move_bytes = (u32) (cur_r - bo);

								gf_fseek(ctx->file, cur_r - move_bytes, SEEK_SET);
								nb_write = (u32) gf_fread(block, (size_t) move_bytes, ctx->file);

								if (nb_write!=move_bytes) {
									GF_LOG(GF_LOG_ERROR, GF_LOG_MMIO, ("[FileOut] Read error, got %d bytes but had %d to read\n", nb_write, move_bytes));
								}

								gf_fseek(ctx->file, cur_w - move_bytes, SEEK_SET);
								nb_write = (u32) gf_fwrite(block, (size_t) move_bytes, ctx->file);

								if (nb_write!=move_bytes) {
									GF_LOG(GF_LOG_ERROR, GF_LOG_MMIO, ("[FileOut] Write error, wrote %d bytes but had %d to write\n", nb_write, move_bytes));
								}
								cur_r -= move_bytes;
								cur_w -= move_bytes;
							}
							gf_free(block);
						}
					}

					gf_fseek(ctx->file, bo, SEEK_SET);
					nb_write = (u32) gf_fwrite(pck_data, pck_size, ctx->file);
					gf_fseek(ctx->file, pos, SEEK_SET);

					if (nb_write!=pck_size) {
						GF_LOG(GF_LOG_ERROR, GF_LOG_MMIO, ("[FileOut] Write error, wrote %d bytes but had %d to write\n", nb_write, pck_size));
					}
				}
			} else {
				nb_write = (u32) gf_fwrite(pck_data, pck_size, ctx->file);
				if (nb_write!=pck_size) {
					GF_LOG(GF_LOG_ERROR, GF_LOG_MMIO, ("[FileOut] Write error, wrote %d bytes but had %d to write\n", nb_write, pck_size));
				}
				ctx->nb_write += nb_write;

				if (ctx->hls_chunk) {
					nb_write = (u32) gf_fwrite(pck_data, pck_size, ctx->hls_chunk);
					if (nb_write!=pck_size) {
						GF_LOG(GF_LOG_ERROR, GF_LOG_MMIO, ("[FileOut] Write error, wrote %d bytes but had %d to write\n", nb_write, pck_size));
					}
				}
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
						nb_write = (u32) gf_fwrite(out_ptr, lsize, ctx->file);
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
		fileout_open_close(ctx, NULL, NULL, 0, GF_FALSE, NULL);
	}
	if (gf_filter_reporting_enabled(filter)) {
		char szStatus[1024];
		snprintf(szStatus, 1024, "%s: wrote % 16"LLD_SUF" bytes", gf_file_basename(ctx->szFileName), (s64) ctx->nb_write);
		gf_filter_update_status(filter, -1, szStatus);
	}
	return GF_OK;
}

static Bool fileout_process_event(GF_Filter *filter, const GF_FilterEvent *evt)
{
	if (evt->base.type==GF_FEVT_FILE_DELETE) {
		GF_FileOutCtx *ctx = (GF_FileOutCtx *) gf_filter_get_udta(filter);
		if (ctx->is_null) {
			GF_LOG(GF_LOG_INFO, GF_LOG_MMIO, ("[FileOut] null delete (file name was %s)\n", evt->file_del.url));
		} else {
			gf_file_delete(evt->file_del.url);
		}
		return GF_TRUE;
	}
	return GF_FALSE;
}
static GF_FilterProbeScore fileout_probe_url(const char *url, const char *mime)
{
	if (strstr(url, "://")) {

		if (!strnicmp(url, "file://", 7)) return GF_FPROBE_MAYBE_SUPPORTED;
		if (!strnicmp(url, "gfio://", 7)) {
			if (!gf_fileio_write_mode(gf_fileio_from_url(url)))
				return GF_FPROBE_NOT_SUPPORTED;
			return GF_FPROBE_MAYBE_SUPPORTED;
		}
		return GF_FPROBE_NOT_SUPPORTED;
	}
	return GF_FPROBE_MAYBE_SUPPORTED;
}


#define OFFS(_n)	#_n, offsetof(GF_FileOutCtx, _n)

static const GF_FilterArgs FileOutArgs[] =
{
	{ OFFS(dst), "location of destination file - see filter help ", GF_PROP_NAME, NULL, NULL, 0},
	{ OFFS(append), "open in append mode", GF_PROP_BOOL, "false", NULL, GF_FS_ARG_HINT_ADVANCED},
	{ OFFS(dynext), "indicate the file extension is set by filter chain, not dst", GF_PROP_BOOL, "false", NULL, GF_FS_ARG_HINT_ADVANCED},
	{ OFFS(start), "set playback start offset. Negative value means percent of media duration with -1 equal to duration", GF_PROP_DOUBLE, "0.0", NULL, 0},
	{ OFFS(speed), "set playback speed when vsync is on. If speed is negative and start is 0, start is set to -1", GF_PROP_DOUBLE, "1.0", NULL, 0},
	{ OFFS(ext), "set extension for graph resolution, regardless of file extension", GF_PROP_NAME, NULL, NULL, GF_FS_ARG_HINT_ADVANCED},
	{ OFFS(mime), "set mime type for graph resolution", GF_PROP_NAME, NULL, NULL, GF_FS_ARG_HINT_EXPERT},
	{ OFFS(cat), "cat each file of input pid rather than creating one file per filename\n"
			"- none: never cat files\n"
			"- auto: only cat if files have same names\n"
			"- all: always cat regardless of file names"
	, GF_PROP_UINT, "none", "none|auto|all", GF_FS_ARG_HINT_ADVANCED},
	{ OFFS(ow), "overwrite output if existing", GF_PROP_BOOL, "true", NULL, 0},
	{ OFFS(mvbk), "block size used when moving parts of the file around in patch mode", GF_PROP_UINT, "8192", NULL, 0},
	{ OFFS(redund), "keep redundant packet in output file", GF_PROP_BOOL, "false", NULL, 0},
	{ OFFS(noinitraw), "do not produce initial segment", GF_PROP_BOOL, "false", NULL, GF_FS_ARG_HINT_HIDE},

	{0}
};

static const GF_FilterCapability FileOutCaps[] =
{
	CAP_UINT(GF_CAPS_INPUT,GF_PROP_PID_STREAM_TYPE, GF_STREAM_FILE),
	CAP_STRING(GF_CAPS_INPUT,GF_PROP_PID_FILE_EXT, "*"),
};


GF_FilterRegister FileOutRegister = {
	.name = "fout",
	GF_FS_SET_DESCRIPTION("File output")
	GF_FS_SET_HELP("The file output filter is used to write output to disk, and does not produce any output PID.\n"
		"It can work as a null sink when its destination is `null`, dropping all input packets. In this case it accepts ANY type of input pid, not just file ones.\n"
		"In regular mode, the filter only accept pid of type file. It will dump to file incomming packets (stream type file), starting a new file for each packet having a __frame_start__ flag set, unless operating in [-cat]() mode.\n"
		"If the output file name is `std` or `stdout`, writes to stdout.\n"
		"The output file name can use gpac templating mechanism, see `gpac -h doc`."
		"The filter watches the property `FileNumber` on incoming packets to create new files.\n"
	)
	.private_size = sizeof(GF_FileOutCtx),
	.args = FileOutArgs,
	.flags = GF_FS_REG_FORCE_REMUX,
	SETCAPS(FileOutCaps),
	.probe_url = fileout_probe_url,
	.initialize = fileout_initialize,
	.finalize = fileout_finalize,
	.configure_pid = fileout_configure_pid,
	.process = fileout_process,
	.process_event = fileout_process_event
};


const GF_FilterRegister *fileout_register(GF_FilterSession *session)
{
	return &FileOutRegister;
}


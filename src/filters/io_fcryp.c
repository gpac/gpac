/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2021-2022
 *					All rights reserved
 *
 *  This file is part of GPAC / file crypt/decrypt for full segment encryption filter
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
#include <gpac/download.h>
#include <gpac/crypt.h>
#include <gpac/network.h>

enum
{
	KEY_STATE_NONE=0,
	KEY_STATE_CHANGED,
	KEY_STATE_DOWNLOADING,
	KEY_STATE_SET_IV,
};

typedef struct
{
	Bool do_crypt;
	bin128 key;
	bin128 iv;
} KeyInfo;


typedef struct
{
	//options
	char *src, *dst;
	Bool fullfile;

	GF_FilterPid *ipid, *opid;
	GF_Filter *for_filter;

	u32 reload_key_state;
	char *key_url;
	bin128 IV;

	GF_DownloadSession *key_sess;
	GF_Err in_error;

	u8 key_data[20];
	u32 key_size;
	GF_Crypt *crypt;

	u8 store[16];
	u32 remain;

	bin128 last_key;
	Bool use_key;
	Bool file_done;
	GF_List *keys;
} GF_CryptFileCtx;

static GF_Err cryptfile_configure_pid(GF_Filter *filter, GF_FilterPid *pid, Bool is_remove)
{
	GF_CryptFileCtx *ctx = (GF_CryptFileCtx *) gf_filter_get_udta(filter);

	if (is_remove) {
		if (ctx->opid) gf_filter_pid_set_eos(ctx->opid);
		return GF_OK;
	}
	if (!ctx->opid)
		ctx->opid = gf_filter_pid_new(filter);
	ctx->ipid = pid;
	gf_filter_pid_copy_properties(ctx->opid, pid);
	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_ORIG_STREAM_TYPE, NULL);
	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_STREAM_TYPE, &PROP_UINT(GF_STREAM_FILE));
	//the output file is no longer cached
	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_FILE_CACHED, NULL);
	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_FILEPATH, NULL);

	//if no mime assume M2TS
	if (gf_filter_pid_get_property(pid, GF_PROP_PID_MIME) == NULL) {
		gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_MIME, &PROP_NAME("video/mpeg-2"));
	}

	if (ctx->fullfile)
		gf_filter_pid_set_framing_mode(pid, GF_TRUE);
	return GF_OK;
}

const char *gf_filter_get_src_args(GF_Filter *filter);

static Bool cryptfile_on_filter_setup_error(GF_Filter *failed_filter, void *udta, GF_Err err)
{
	GF_Filter *f = (GF_Filter *)udta;
	if (!udta) return GF_FALSE;
	//forward failure, but do not send a setup failure (gf_filter_setup_failure) which would remove this filter
	//we let the final user (dashdmx) decide what to do
	gf_filter_notification_failure(f, err, GF_FALSE);
	return GF_FALSE;
}

static GF_Err cryptfin_initialize(GF_Filter *filter)
{
	GF_Err e;
	const char *args;
	GF_CryptFileCtx *ctx = (GF_CryptFileCtx *) gf_filter_get_udta(filter);
	if (!ctx || !ctx->src) return GF_BAD_PARAM;

	if (strncmp(ctx->src, "gcryp://", 8)) return GF_BAD_PARAM;
	//get complete args of filter, strip anything up to (including) gcryp://
	args = gf_filter_get_src_args(filter);
	if (args) args = strstr(args, "gcryp://");
	if (args) args += 8;
	else args = ctx->src+8;

#ifdef GPAC_ENABLE_COVERAGE
	if (gf_sys_is_cov_mode()) {
		cryptfile_on_filter_setup_error(NULL, NULL, GF_OK);
	}
#endif

	ctx->for_filter = gf_filter_connect_source(filter, args, NULL, GF_FALSE, &e);
	if (e) return e;
	gf_filter_set_setup_failure_callback(filter, ctx->for_filter, cryptfile_on_filter_setup_error, filter);
	return gf_filter_set_source(filter, ctx->for_filter, NULL);
}

static void cryptfile_finalize(GF_Filter *filter)
{
	GF_CryptFileCtx *ctx = (GF_CryptFileCtx *) gf_filter_get_udta(filter);
	if (ctx->crypt) gf_crypt_close(ctx->crypt);
	if (ctx->key_url) gf_free(ctx->key_url);

	if (ctx->keys) {
		while (gf_list_count(ctx->keys)) {
			KeyInfo *ki = gf_list_pop_front(ctx->keys);
			gf_free(ki);
		}
		gf_list_del(ctx->keys);
	}
}

static GF_FilterProbeScore cryptfile_probe_url(const char *url, const char *mime_type)
{
	if (!strnicmp(url, "gcryp://", 8)) return GF_FPROBE_SUPPORTED;
	return GF_FPROBE_NOT_SUPPORTED;
}

static void cryptfile_set_key(GF_CryptFileCtx *ctx)
{
	if (!ctx->crypt) {
		ctx->crypt = gf_crypt_open(GF_AES_128, GF_CBC);
		if (!ctx->crypt) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_DASH, ("[CryptFile] Failed to allocat decryptor\n"))
			ctx->in_error = GF_OUT_OF_MEM;
			return;
		}
		ctx->in_error = gf_crypt_init(ctx->crypt, ctx->key_data, ctx->IV);
	} else {
		ctx->in_error = gf_crypt_set_key(ctx->crypt, ctx->key_data);
		if (!ctx->in_error)
			ctx->in_error = gf_crypt_set_IV(ctx->crypt, ctx->IV, 16);
	}
	ctx->reload_key_state = KEY_STATE_NONE;
}

static GF_Err cryptfin_process(GF_Filter *filter)
{
	GF_Err e;
	u32 size, osize, unused;
	const u8 *data;
	Bool start, end;
	u8 *output, pad;
	GF_FilterPacket *pck, *pck_out;
	GF_CryptFileCtx *ctx = (GF_CryptFileCtx *) gf_filter_get_udta(filter);

	if (ctx->in_error) return ctx->in_error;

	if (!ctx->ipid) return GF_OK;
	pck = gf_filter_pid_get_packet(ctx->ipid);
	if (!pck) {
		if (gf_filter_pid_is_eos(ctx->ipid)) {
			gf_filter_pid_set_eos(ctx->opid);
			return GF_EOS;
		}
		return GF_OK;
	}

	if (!ctx->key_url) {
		gf_filter_pck_forward(pck, ctx->opid);
		gf_filter_pid_drop_packet(ctx->ipid);
		return GF_OK;
	}

	if (ctx->reload_key_state) {
		if (ctx->reload_key_state==KEY_STATE_CHANGED) {
			if (!ctx->key_sess) {
				GF_DownloadManager *dm = gf_filter_get_download_manager(filter);
				ctx->key_sess = gf_dm_sess_new(dm, ctx->key_url, GF_NETIO_SESSION_NOT_THREADED | GF_NETIO_SESSION_NOT_CACHED, NULL, NULL, &e);
			} else {
				e = gf_dm_sess_setup_from_url(ctx->key_sess, ctx->key_url, GF_TRUE);
			}
			if (e) {
				GF_LOG(GF_LOG_ERROR, GF_LOG_DASH, ("[CryptFile] Failed to setup download session for key %s: %s\n", ctx->key_url, gf_error_to_string(e)))
				return ctx->in_error = e;
			}
			ctx->reload_key_state = KEY_STATE_DOWNLOADING;
		}
		if (ctx->reload_key_state==KEY_STATE_DOWNLOADING) {
			u32 nb_read;
			e = gf_dm_sess_fetch_data(ctx->key_sess, ctx->key_data + ctx->key_size, 100-ctx->key_size, &nb_read);
			ctx->key_size += nb_read;
			if (ctx->key_size > 16) {
				GF_LOG(GF_LOG_ERROR, GF_LOG_DASH, ("[CryptFile] Invalid key size, greater than 16 bytes\n"))
				return ctx->in_error = GF_SERVICE_ERROR;
			}

			if ((e<0) && (e != GF_IP_NETWORK_EMPTY)) {
				ctx->in_error = e;
				ctx->reload_key_state = KEY_STATE_NONE;
				GF_LOG(GF_LOG_ERROR, GF_LOG_DASH, ("[CryptFile] Failed to download key %s: %s\n", ctx->key_url, gf_error_to_string(e)))
				return e;
			}
			if (e == GF_EOS) {
				cryptfile_set_key(ctx);
				if (ctx->in_error) return ctx->in_error;
			} else {
				return GF_OK;
			}
		}
		if (ctx->reload_key_state == KEY_STATE_SET_IV) {
			gf_crypt_set_IV(ctx->crypt, ctx->IV, 16);
			ctx->reload_key_state = KEY_STATE_NONE;
		}
	}

	if (ctx->fullfile) {

		gf_filter_pck_get_data(pck, &size);
		pck_out = gf_filter_pck_new_clone(ctx->opid, pck, &output);
		if (!pck_out) return GF_OUT_OF_MEM;

		e = gf_crypt_decrypt(ctx->crypt, output, size);
		pad = output[size-1];
		if (!pad || (pad>16)) {
			GF_LOG(GF_LOG_WARNING, GF_LOG_DASH, ("[CryptFile] Invalid PKCS7 padding %d, should be in range [1,16]\n", pad))
			if (pad) pad = 16;
		}
		size -= pad;
		gf_filter_pck_truncate(pck_out, size);
		gf_filter_pck_send(pck_out);

		gf_filter_pid_drop_packet(ctx->ipid);
		return e;
	}
	data = gf_filter_pck_get_data(pck, &size);
	gf_filter_pck_get_framing(pck, &start, &end);

	osize = size + ctx->remain;
	unused = 0;
	if (!end) {
		while (osize % 16) {
			osize --;
			unused++;
		}
	}
	pck_out = gf_filter_pck_new_alloc(ctx->opid, osize, &output);
	if (!pck_out) return GF_OUT_OF_MEM;

	if (ctx->remain)
		memcpy(output, ctx->store, ctx->remain);
	memcpy(output+ctx->remain, data, osize - ctx->remain);
	e = gf_crypt_decrypt(ctx->crypt, output, osize);
	gf_filter_pck_merge_properties(pck, pck_out);
	gf_filter_pck_set_framing(pck_out, start, end);

	if (unused) {
		memcpy(ctx->store, data + size - unused, unused);
	}
	ctx->remain = unused;
	if (end) {
		pad = output[osize-1];
		if (!pad || (pad>16)) {
			GF_LOG(GF_LOG_WARNING, GF_LOG_DASH, ("[CryptFile] Invalid PKCS7 padding %d, should be in range [1,16]\n", pad))
			if (pad) pad = 16;
		}
		osize -= pad;
		gf_filter_pck_truncate(pck_out, osize);

		gf_filter_pid_set_info(ctx->opid, GF_PROP_PID_FILE_CACHED, &PROP_BOOL(GF_TRUE) );
	} else if (start) {
		gf_filter_pid_set_info(ctx->opid, GF_PROP_PID_FILE_CACHED, &PROP_BOOL(GF_FALSE) );
	}
	gf_filter_pck_send(pck_out);
	gf_filter_pid_drop_packet(ctx->ipid);
	return e;
}


#define OFFS(_n)	#_n, offsetof(GF_CryptFileCtx, _n)

static const GF_FilterArgs CryptFinArgs[] =
{
	{ OFFS(src), "location of source file", GF_PROP_NAME, NULL, NULL, 0},
	{ OFFS(fullfile), "reassemble full file before decryption", GF_PROP_BOOL, "false", NULL, GF_ARG_HINT_ADVANCED},
	{0}
};

static const GF_FilterCapability CryptFileCaps[] =
{
	CAP_UINT(GF_CAPS_INPUT, GF_PROP_PID_STREAM_TYPE, GF_STREAM_FILE),
	CAP_UINT(GF_CAPS_OUTPUT, GF_PROP_PID_STREAM_TYPE, GF_STREAM_FILE),
};

GF_FilterRegister CryptFinRegister = {
	.name = "cryptin",
	GF_FS_SET_DESCRIPTION("CryptFile input")
	GF_FS_SET_HELP("This filter dispatch raw blocks from encrypted files with AES 128 CBC in PKCS7 to clear input files\n"
	"\n"
	"The filter is automatically loaded by the DASH/HLS demultiplexer and should not be explicitly loaded by your application.\n"
	"\n"
	"The filter accepts URL with scheme `gcryp://URL`, where `URL` is the URL to decrypt.\n"
	"\n"
	"The filter can process http(s) and local file key URLs (setup through HLS manifest), and expects a full key (16 bytes) as result of resource fetching.\n"
	)
	.private_size = sizeof(GF_CryptFileCtx),
	.args = CryptFinArgs,
	.initialize = cryptfin_initialize,
	//GF_FS_REG_ACT_AS_SOURCE needed to prevent GF_FEVT_SOURCE_SWITCH to be canceled
	.flags = GF_FS_REG_EXPLICIT_ONLY | GF_FS_REG_ACT_AS_SOURCE,
	SETCAPS(CryptFileCaps),
	.finalize = cryptfile_finalize,
	.configure_pid = cryptfile_configure_pid,
	.process = cryptfin_process,
	.probe_url = cryptfile_probe_url
};


const GF_FilterRegister *cryptfin_register(GF_FilterSession *session)
{
	return &CryptFinRegister;
}

void gf_cryptfin_set_kms(GF_Filter *filter, const char *key_url, bin128 key_IV)
{
	GF_CryptFileCtx *ctx;
	if (!gf_filter_is_instance_of(filter, &CryptFinRegister))
		return;
	ctx = (GF_CryptFileCtx *) gf_filter_get_udta(filter);

	//copy IV
	memcpy(ctx->IV, key_IV, sizeof(bin128));
	//switch key if needed IV
	if (ctx->key_url && key_url && !strcmp(ctx->key_url, key_url)) {
		ctx->reload_key_state = KEY_STATE_SET_IV;
	} else {
		if (ctx->key_url) gf_free(ctx->key_url);

		if (!key_url) {
			ctx->key_url = NULL;
			return;
		}
		ctx->key_url = gf_strdup(key_url);
		if (!ctx->key_url) {
			ctx->in_error = GF_OUT_OF_MEM;
			return;
		}
		ctx->reload_key_state = KEY_STATE_CHANGED;
		GF_LOG(GF_LOG_DEBUG, GF_LOG_DASH, ("[CryptFile] Switching key to %s\n", key_url))

		if (!strncmp(key_url, "urn:gpac:keys:value:", 20)) {
			u32 i;
			bin128 key_data;
			key_url += 20;
			if (!strncmp(key_url, "0x", 2)) key_url += 2;
			i = (u32) strlen(key_url);
			if (i != 32) {
				GF_LOG(GF_LOG_ERROR, GF_LOG_DASH, ("[CryptFile] key %s not found\n", key_url))
				ctx->in_error = GF_BAD_PARAM;
				return;
			}
			for (i=0; i<16; i++) {
				char szV[3];
				u32 v;
				szV[0] = key_url[2*i];
				szV[1] = key_url[2*i + 1];
				szV[2] = 0;
				sscanf(szV, "%X", &v);
				key_data[i] = v;
			}
			memcpy(ctx->key_data, key_data, sizeof(bin128));
			cryptfile_set_key(ctx);
		}
		//key is local, activate right away
		else if (gf_url_is_local(key_url)) {
			FILE *fkey = gf_fopen(key_url, "rb");
			if (!fkey) {
				ctx->in_error = GF_URL_ERROR;
				GF_LOG(GF_LOG_ERROR, GF_LOG_DASH, ("[CryptFile] key %s not found\n", key_url))
			} else {
				u32 read = (u32) gf_fread(ctx->key_data, 16, fkey);
				if (read != 16) {
					GF_LOG(GF_LOG_ERROR, GF_LOG_DASH, ("[CryptFile] key %s too short, expecting 16 bytes got %d\n", key_url, read))
					ctx->in_error = GF_BAD_PARAM;
				} else {
					cryptfile_set_key(ctx);
				}
				gf_fclose(fkey);
			}
		}
	}
}


void gf_filter_mirror_forced_caps(GF_Filter *filter, GF_Filter *dst_filter);

static GF_Err cryptfout_initialize(GF_Filter *filter)
{
	GF_Err e;
	const char *args;
	GF_CryptFileCtx *ctx = (GF_CryptFileCtx *) gf_filter_get_udta(filter);
	if (!ctx || !ctx->dst) return GF_BAD_PARAM;

	if (strncmp(ctx->dst, "gcryp://", 8)) return GF_BAD_PARAM;
	ctx->keys = gf_list_new();
	if (!ctx->keys) return GF_OUT_OF_MEM;

	//get complete args of filter, strip anything up to (including) gcryp://
	args = gf_filter_get_src_args(filter);
	if (args) args = strstr(args, "gcryp://");
	if (args) args += 8;
	else args = ctx->dst+8;

	ctx->for_filter = gf_filter_connect_destination(filter, args, &e);
	if (e) return e;
	gf_filter_set_source(ctx->for_filter, filter, NULL);
	//use same forced cap as the solved destination
	gf_filter_mirror_forced_caps(filter, ctx->for_filter);
	ctx->file_done = GF_TRUE;
	return GF_OK;
}

static GF_Err cryptfout_process(GF_Filter *filter)
{
	const u8 *data;
	u8 *output;
	GF_Err e;
	Bool start, end;
	u32 size, osize, pad, i, unused;
	GF_FilterPacket *pck_out;
	GF_CryptFileCtx *ctx = (GF_CryptFileCtx *) gf_filter_get_udta(filter);
	GF_FilterPacket *pck_in = gf_filter_pid_get_packet(ctx->ipid);

	if (ctx->in_error)
		return ctx->in_error;

	if (!pck_in) {
		if (gf_filter_pid_is_eos(ctx->ipid)) {
			if (!ctx->remain && ctx->file_done) {
				gf_filter_pid_set_eos(ctx->opid);
				return GF_EOS;
			}
			start = GF_TRUE;
		} else {
			return GF_OK;
		}
	} else {
		gf_filter_pck_get_framing(pck_in, &start, &end);
	}

	if (start) {
		KeyInfo *ki = gf_list_pop_front(ctx->keys);

		if (ctx->remain || !ctx->file_done) {
			pad = 16 - (ctx->remain % 16);
			osize = ctx->remain + pad;
			pck_out = gf_filter_pck_new_alloc(ctx->opid, osize, &output);
			if (!pck_out) {
				gf_list_insert(ctx->keys, ki, 0);
				return GF_OUT_OF_MEM;
			}

			if (ctx->remain)
				memcpy(output, ctx->store, ctx->remain);

			for (i=0; i<pad; i++) {
				output[ctx->remain + i] = pad;
			}
			gf_crypt_encrypt(ctx->crypt, output, osize);

			gf_filter_pck_set_framing(pck_out, GF_FALSE, GF_TRUE);
			ctx->remain = 0;
			ctx->file_done = GF_TRUE;
			gf_filter_pck_send(pck_out);
		}
		if (!pck_in) {
			gf_free(ki);
			return GF_OK;
		}
		ctx->file_done = GF_FALSE;

		ctx->use_key = GF_TRUE;
		e = GF_OK;
		if (!ki || !ki->do_crypt) {
			ctx->use_key = GF_FALSE;
			ctx->file_done = GF_TRUE;
		} else if (!ctx->crypt) {
			ctx->crypt = gf_crypt_open(GF_AES_128, GF_CBC);
			if (!ctx->crypt) {
				gf_free(ki);
				return ctx->in_error = GF_OUT_OF_MEM;
			}
			e = gf_crypt_init(ctx->crypt, ki->key, ki->iv);
			memcpy(ctx->last_key, ki->key, sizeof(bin128));
		} else {
			if (memcmp(ctx->last_key, ki->key, sizeof(bin128))) {
				e = gf_crypt_set_key(ctx->crypt, ki->key);
				memcpy(ctx->last_key, ki->key, sizeof(bin128));
			}
			if (!e)
				e = gf_crypt_set_IV(ctx->crypt, ki->iv, 16);
		}
		gf_free(ki);
		if (e) return ctx->in_error = e;
	}

	if (!ctx->use_key) {
		gf_filter_pck_forward(pck_in, ctx->opid);
		gf_filter_pid_drop_packet(ctx->ipid);
		return GF_OK;
	}

	if (ctx->fullfile) {
		data = gf_filter_pck_get_data(pck_in, &size);
		pad = 16 - (size % 16);
		pck_out = gf_filter_pck_new_alloc(ctx->opid, size+pad, &output);
		if (!pck_out) return GF_OUT_OF_MEM;

		memcpy(output, data, size);
		for (i=0; i<pad; i++) {
			output[size+i] = (u8) pad;
		}
		gf_crypt_encrypt(ctx->crypt, output, size+pad);

		gf_filter_pck_merge_properties(pck_in, pck_out);
		gf_filter_pck_send(pck_out);
		gf_filter_pid_drop_packet(ctx->ipid);
		ctx->file_done = GF_TRUE;
		return GF_OK;
	}

	data = gf_filter_pck_get_data(pck_in, &size);

	osize = size + ctx->remain;
	unused = pad = 0;
	if (end) {
		pad = 16 - (osize % 16);
		osize += pad;
		ctx->file_done = GF_TRUE;
	} else {
		unused = osize % 16;
		osize -= unused;
	}

	pck_out = gf_filter_pck_new_alloc(ctx->opid, osize, &output);
	if (!pck_out) return GF_OUT_OF_MEM;
	
	if (ctx->remain)
		memcpy(output, ctx->store, ctx->remain);
	memcpy(output+ctx->remain, data, osize - ctx->remain - pad);
	for (i=0; i<pad; i++) {
		output[osize + i - pad] = pad;
	}
	e = gf_crypt_encrypt(ctx->crypt, output, osize);

	gf_filter_pck_merge_properties(pck_in, pck_out);
	gf_filter_pck_set_framing(pck_out, start, end);

	if (unused) {
		memcpy(ctx->store, data + size - unused, unused);
	}
	ctx->remain = unused;
	gf_filter_pck_send(pck_out);
	gf_filter_pid_drop_packet(ctx->ipid);
	return e;
}

static const GF_FilterArgs CryptFoutArgs[] =
{
	{ OFFS(dst), "location of source file", GF_PROP_NAME, NULL, NULL, 0},
	{ OFFS(fullfile), "reassemble full file before decryption", GF_PROP_BOOL, "false", NULL, GF_ARG_HINT_ADVANCED},
	{0}
};

GF_FilterRegister CryptFoutRegister = {
	.name = "cryptout",
	GF_FS_SET_DESCRIPTION("CryptFile output")
	GF_FS_SET_HELP("This filter dispatch raw blocks from clear input files to encrypted files with AES 128 CBC in PKCS7\n"
	"\n"
	"The filter is automatically loaded by the DASH/HLS multiplexer and should not be explicitly loaded by your application.\n"
	"\n"
	"The filter accepts URL with scheme `gcryp://URL`, where `URL` is the URL to encrypt.")
	.private_size = sizeof(GF_CryptFileCtx),
	.args = CryptFoutArgs,
	.initialize = cryptfout_initialize,
	.flags = GF_FS_REG_EXPLICIT_ONLY,
	SETCAPS(CryptFileCaps),
	.finalize = cryptfile_finalize,
	.configure_pid = cryptfile_configure_pid,
	.process = cryptfout_process,
	.probe_url = cryptfile_probe_url
};


const GF_FilterRegister *cryptfout_register(GF_FilterSession *session)
{
	return &CryptFoutRegister;
}

GF_Err gf_cryptfout_push_key(GF_Filter *filter, bin128 *key, bin128 *IV)
{
	KeyInfo *key_info;
	GF_CryptFileCtx *ctx;
	if (!gf_filter_is_instance_of(filter, &CryptFoutRegister))
		return GF_BAD_PARAM;

	ctx = (GF_CryptFileCtx *) gf_filter_get_udta(filter);
	GF_SAFEALLOC(key_info, KeyInfo);
	if (!key_info) return GF_OUT_OF_MEM;

	if (key && IV) {
		memcpy(key_info->key, *key, sizeof(bin128));
		memcpy(key_info->iv, *IV, sizeof(bin128));
		key_info->do_crypt = GF_TRUE;
	}
	return gf_list_add(ctx->keys, key_info);
}

/*
*			GPAC - Multimedia Framework C SDK
*
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2000-2019
*					All rights reserved
*
*  This file is part of GPAC / openjpeg2k decoder filter
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

#ifdef GPAC_HAS_JP2

//we MUST set OPJ_STATIC before including openjpeg.h
#if !defined(__GNUC__) && (defined(_WIN32_WCE) || defined (WIN32))
#  define OPJ_STATIC
#endif

#include <gpac/constants.h>
#include <gpac/isomedia.h>

#include <openjpeg.h>

#ifdef OPJ_PROFILE_NONE
#define OPENJP2	1

# if !defined(__GNUC__) && (defined(_WIN32_WCE) || defined (WIN32))
#  if defined(_DEBUG)
#   pragma comment(lib, "libopenjp2d")
#  else
#   pragma comment(lib, "libopenjp2")
#  endif
# endif

#else

#define OPENJP2	0

# if !defined(__GNUC__) && (defined(_WIN32_WCE) || defined (WIN32))
#  if defined(_DEBUG)
#   pragma comment(lib, "LibOpenJPEGd")
#  else
#   pragma comment(lib, "LibOpenJPEG")
#  endif
# endif

#endif

typedef struct
{
	GF_FilterPid *ipid, *opid;
	u32 cfg_crc;
	/*no support for scalability with JPEG (progressive JPEG to test)*/
	u32 bpp, nb_comp, width, height, out_size, pixel_format, dsi_size;
	char *dsi;
} GF_J2KCtx;


static GF_Err j2kdec_configure_pid(GF_Filter *filter, GF_FilterPid *pid, Bool is_remove)
{
	const GF_PropertyValue *p;
	GF_BitStream *bs;
	GF_J2KCtx *ctx = gf_filter_get_udta(filter);

	if (is_remove) {
		if (ctx->opid) gf_filter_pid_remove(ctx->opid);
		ctx->opid = NULL;
		ctx->ipid = NULL;
		return GF_OK;
	}
	if (! gf_filter_pid_check_caps(pid))
		return GF_NOT_SUPPORTED;

	p = gf_filter_pid_get_property(pid, GF_PROP_PID_DECODER_CONFIG);
	if (p && p->value.data.ptr && p->value.data.size) {
		u32 d4cc;
		Bool dsi_ok=GF_FALSE;
		u32 ex_crc = gf_crc_32(p->value.data.ptr, p->value.data.size);
		if (ctx->cfg_crc == ex_crc) {
			return GF_OK;
		}
		ctx->cfg_crc = ex_crc;

		//old gpac version used to store only the payload of ihdr in the dsi, but the new version exposes the entire jp2h (without box size and type) as dsi
		//so handle both on read
		d4cc = 0;
		if (p->value.data.size>8)
			d4cc = GF_4CC(p->value.data.ptr[4], p->value.data.ptr[5], p->value.data.ptr[6], p->value.data.ptr[7]);

		bs = gf_bs_new(p->value.data.ptr, p->value.data.size, GF_BITSTREAM_READ);
		if ((d4cc==GF_4CC('i','h','d','r')) || (d4cc==GF_4CC('c','o','l','r'))) {
			while (gf_bs_available(bs)) {
				u32 bsize = gf_bs_read_u32(bs);
				u32 btype = gf_bs_read_u32(bs);
				if (btype==GF_4CC('i','h','d','r')) {
					dsi_ok=GF_TRUE;
					break;
				}
				gf_bs_skip_bytes(bs, bsize-8);
			}
		} else {
			dsi_ok=GF_TRUE;
		}
		if (dsi_ok) {
			ctx->height = gf_bs_read_u32(bs);
			ctx->width = gf_bs_read_u32(bs);
			ctx->nb_comp = gf_bs_read_u16(bs);
			ctx->bpp = 1 + gf_bs_read_u8(bs);
		}
		gf_bs_del(bs);

		if (!dsi_ok) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_CODEC, ("[OpenJPEG] Broken decoder config in j2k stream, cannot decode\n"));
			return GF_NON_COMPLIANT_BITSTREAM;
		}

		ctx->out_size = ctx->width * ctx->height * ctx->nb_comp /* * ctx->bpp / 8 */;

		switch (ctx->nb_comp) {
		case 1:
			ctx->pixel_format = GF_PIXEL_GREYSCALE;
			break;
		case 2:
			ctx->pixel_format = GF_PIXEL_ALPHAGREY;
			break;
		case 3:
			ctx->pixel_format = GF_PIXEL_RGB;
			break;
		case 4:
			ctx->pixel_format = GF_PIXEL_RGBA;
			break;
		default:
			return GF_NOT_SUPPORTED;
		}
	} else {
		p = gf_filter_pid_get_property(pid, GF_PROP_PID_WIDTH);
		if (p) ctx->width = p->value.uint;
		p = gf_filter_pid_get_property(pid, GF_PROP_PID_HEIGHT);
		if (p) ctx->height = p->value.uint;
		ctx->nb_comp = 0;
		ctx->pixel_format = 0;
	}

	ctx->ipid = pid;
	if (!ctx->opid) {
		ctx->opid = gf_filter_pid_new(filter);
		gf_filter_pid_set_framing_mode(ctx->ipid, GF_TRUE);
	}
	//copy properties at init or reconfig
	gf_filter_pid_copy_properties(ctx->opid, ctx->ipid);
	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_CODECID, &PROP_UINT(GF_CODECID_RAW) );

	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_WIDTH, &PROP_UINT(ctx->width) );
	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_HEIGHT, &PROP_UINT(ctx->height) );
	if (ctx->pixel_format) {
		gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_STRIDE, &PROP_UINT( (ctx->pixel_format == GF_PIXEL_YUV) ? ctx->width : ctx->width * ctx->nb_comp) );
		gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_PIXFMT, &PROP_UINT(ctx->pixel_format) );
	}
	return GF_OK;
}

/**
sample error callback expecting a FILE* client object
*/
void error_callback(const char *msg, void *client_data)
{
	if (msg) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_CODEC, ("[OpenJPEG] Error %s", msg));
	} else {
		GF_LOG(GF_LOG_DEBUG, GF_LOG_CODEC, ("[OpenJPEG] coverage test\n"));
	}
}
/**
sample warning callback expecting a FILE* client object
*/
void warning_callback(const char *msg, void *client_data)
{
	if (msg) {
		GF_LOG(GF_LOG_WARNING, GF_LOG_CODEC, ("[OpenJPEG] Warning %s", msg));
	} else {
		GF_LOG(GF_LOG_DEBUG, GF_LOG_CODEC, ("[OpenJPEG] coverage test\n"));
	}
}
/**
sample debug callback expecting no client object
*/
void info_callback(const char *msg, void *client_data)
{
	if (msg) {
		GF_LOG(GF_LOG_INFO, GF_LOG_CODEC, ("[OpenJPEG] Info %s", msg));
	} else {
		GF_LOG(GF_LOG_DEBUG, GF_LOG_CODEC, ("[OpenJPEG] coverage test\n"));
	}
}

/*
* Divide an integer by a power of 2 and round upwards.
*
* a divided by 2^b
*/
static int int_ceildivpow2(int a, int b) {
	return (a + (1 << b) - 1) >> b;
}

#if OPENJP2
typedef struct
{
	char *data;
	u32 len, pos;
} OJP2Frame;

static OPJ_SIZE_T j2kdec_stream_read(void *out_buffer, OPJ_SIZE_T nb_bytes, void *user_data)
{
    OJP2Frame *frame = user_data;
    u32 remain;
    if (frame->pos == frame->len) return (OPJ_SIZE_T)-1;
    remain = frame->len - frame->pos;
    if (nb_bytes > remain) nb_bytes = remain;
    memcpy(out_buffer, frame->data + frame->pos, nb_bytes);
    frame->pos += (u32) nb_bytes;
    return nb_bytes;
}

static OPJ_OFF_T j2kdec_stream_skip(OPJ_OFF_T nb_bytes, void *user_data)
{
    OJP2Frame *frame = user_data;
    if (nb_bytes < 0) {
        if (frame->pos == 0) return (OPJ_SIZE_T)-1;
        if (nb_bytes + (s32) frame->pos < 0) {
            nb_bytes = -frame->pos;
        }
    } else {
        u32 remain;
        if (frame->pos == frame->len) {
            return (OPJ_SIZE_T)-1;
        }
        remain = frame->len - frame->pos;
        if (nb_bytes > remain) {
            nb_bytes = remain;
        }
    }
    frame->pos += (u32) nb_bytes;
    return nb_bytes;
}

static OPJ_BOOL j2kdec_stream_seek(OPJ_OFF_T nb_bytes, void *user_data)
{
    OJP2Frame *frame = user_data;
    if (nb_bytes < 0 || nb_bytes > frame->pos) return OPJ_FALSE;
    frame->pos = (u32)nb_bytes;
    return OPJ_TRUE;
}
#endif


static GF_Err j2kdec_process(GF_Filter *filter)
{
	u32 i, w, wr, h, hr, wh, size, pf;
	u8 *data, *buffer;
	opj_dparameters_t parameters;	/* decompression parameters */
#if OPENJP2
	s32 res;
	opj_codec_t *codec = NULL;
	opj_stream_t * stream = NULL;
	OJP2Frame ojp2frame;
#else
	opj_event_mgr_t event_mgr;		/* event manager */
	opj_dinfo_t* dinfo = NULL;	/* handle to a decompressor */
	opj_cio_t *cio = NULL;
	opj_codestream_info_t cinfo;
#endif
	opj_image_t *image = NULL;
	u32 start_offset=0;
	GF_J2KCtx *ctx = gf_filter_get_udta(filter);
	GF_FilterPacket *pck, *pck_dst;
	Bool changed = GF_FALSE;
	pck = gf_filter_pid_get_packet(ctx->ipid);
	if (!pck) {
		if (gf_filter_pid_is_eos(ctx->ipid))
			gf_filter_pid_set_eos(ctx->opid);
		return GF_OK;
	}
	data = (char *) gf_filter_pck_get_data(pck, &size);
	if (!data) {
		gf_filter_pid_drop_packet(ctx->ipid);
		return GF_IO_ERR;
	}

	if (size>=8) {
		if ((data[4]=='j') && (data[5]=='p') && (data[6]=='2') && (data[7]=='c'))
			start_offset = 8;
	}

	/* set decoding parameters to default values */
	opj_set_default_decoder_parameters(&parameters);

#if OPENJP2
	codec = opj_create_decompress(OPJ_CODEC_J2K);
	if (codec) res = 1;
	else res=0;

	if (res) res = opj_set_info_handler(codec, info_callback, NULL);
	if (res) res = opj_set_warning_handler(codec, warning_callback, NULL);
	if (res) res = opj_set_error_handler(codec, error_callback, NULL);

	if (res) res = opj_setup_decoder(codec, &parameters);

	stream = opj_stream_default_create(OPJ_STREAM_READ);
    opj_stream_set_read_function(stream, j2kdec_stream_read);
    opj_stream_set_skip_function(stream, j2kdec_stream_skip);
    opj_stream_set_seek_function(stream, j2kdec_stream_seek);
    ojp2frame.data = data+start_offset;
    ojp2frame.len = size-start_offset;
    ojp2frame.pos = 0;
    if (res) opj_stream_set_user_data(stream, &ojp2frame, NULL);
    if (res) opj_stream_set_user_data_length(stream, ojp2frame.len);

	if (res) res = opj_read_header(stream, codec, &image);
	if (res) res = opj_set_decode_area(codec, image, 0, 0, image->x1, image->y1);
	if (res) res = opj_decode(codec, stream, image);

#else

	/* get a decoder handle for raw J2K frames*/
	dinfo = opj_create_decompress(CODEC_J2K);

	/* configure the event callbacks (not required) */
	memset(&event_mgr, 0, sizeof(opj_event_mgr_t));
	event_mgr.error_handler = error_callback;
	event_mgr.warning_handler = warning_callback;
	event_mgr.info_handler = info_callback;

	/* catch events using our callbacks and give a local context */
	opj_set_event_mgr((opj_common_ptr)dinfo, &event_mgr, stderr);

	/* setup the decoder decoding parameters using the current image and user parameters */
	opj_setup_decoder(dinfo, &parameters);

	cio = opj_cio_open((opj_common_ptr)dinfo, data+start_offset, size-start_offset);
	/* decode the stream and fill the image structure */
	image = opj_decode_with_info(dinfo, cio, &cinfo);
#endif

	if (!image) {
#if OPENJP2
		opj_stream_destroy(stream);
		opj_destroy_codec(codec);
#else
		opj_destroy_decompress(dinfo);
		opj_cio_close(cio);
#endif
		gf_filter_pid_drop_packet(ctx->ipid);
		return GF_IO_ERR;
	}

#if OPENJP2
	ctx->nb_comp = image->numcomps;
	w = image->x1;
	h = image->y1;
#else
	ctx->nb_comp = cinfo.numcomps;
	w = cinfo.image_w;
	h = cinfo.image_h;
#endif
	ctx->bpp = ctx->nb_comp * 8;
	ctx->out_size = ctx->width * ctx->height * ctx->nb_comp /* * ctx->bpp / 8 */;

	switch (ctx->nb_comp) {
	case 1:
		pf = GF_PIXEL_GREYSCALE;
		break;
	case 2:
		pf = GF_PIXEL_ALPHAGREY;
		break;
	case 3:
		pf = GF_PIXEL_RGB;
		break;
	case 4:
		pf = GF_PIXEL_RGBA;
		break;
	default:
		gf_filter_pid_drop_packet(ctx->ipid);
		return GF_NOT_SUPPORTED;
	}
	if ((image->comps[0].w==2*image->comps[1].w)
		&& (image->comps[1].w==image->comps[2].w)
		&& (image->comps[0].h==2*image->comps[1].h)
		&& (image->comps[1].h==image->comps[2].h)) {
		pf = GF_PIXEL_YUV;
		ctx->out_size = 3*ctx->width*ctx->height/2;
		changed = GF_TRUE;
	}
	if (ctx->pixel_format!=pf) {
		gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_PIXFMT, &PROP_UINT(pf) );
		ctx->pixel_format = pf;
		changed = GF_TRUE;
	}
	if (ctx->width!=w) {
		gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_WIDTH, &PROP_UINT(w) );
		ctx->width = w;
		changed = GF_TRUE;
	}
	if (ctx->height!=h) {
		gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_HEIGHT, &PROP_UINT(h) );
		ctx->height = h;
		changed = GF_TRUE;
	}
	if (changed) {
		gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_STRIDE, &PROP_UINT( (ctx->pixel_format == GF_PIXEL_YUV) ? ctx->width : ctx->width * ctx->nb_comp) );
	}

#if OPENJP2
	opj_end_decompress(codec, stream);
	opj_stream_destroy(stream);
	stream = NULL;
	opj_destroy_codec(codec);
	codec = NULL;
#else
	/* close the byte stream */
	opj_cio_close(cio);
	cio = NULL;

	/* gf_free( remaining structures */
	if(dinfo) {
		opj_destroy_decompress(dinfo);
		dinfo = NULL;
	}
#endif

	pck_dst = gf_filter_pck_new_alloc(ctx->opid, ctx->out_size, &buffer);

	w = image->comps[0].w;
	wr = int_ceildivpow2(image->comps[0].w, image->comps[0].factor);
	h = image->comps[0].h;
	hr = int_ceildivpow2(image->comps[0].h, image->comps[0].factor);
	wh = wr*hr;

	if (ctx->nb_comp==1) {
		if ((w==wr) && (h==hr)) {
			for (i=0; i<wh; i++) {
				buffer[i] = image->comps[0].data[i];
			}
		} else {
			for (i=0; i<wh; i++) {
				buffer[i] = image->comps[0].data[w * hr - ((i) / (wr) + 1) * w + (i) % (wr)];
			}
		}
	}
	else if (ctx->nb_comp==3) {

		if ((image->comps[0].w==2*image->comps[1].w) && (image->comps[1].w==image->comps[2].w)
		        && (image->comps[0].h==2*image->comps[1].h) && (image->comps[1].h==image->comps[2].h)) {

			if ((w==wr) && (h==hr)) {
				for (i=0; i<wh; i++) {
					*buffer = image->comps[0].data[i];
					buffer++;
				}
//				w = image->comps[1].w;
				wr = int_ceildivpow2(image->comps[1].w, image->comps[1].factor);
//				h = image->comps[1].h;
				hr = int_ceildivpow2(image->comps[1].h, image->comps[1].factor);
				wh = wr*hr;
				for (i=0; i<wh; i++) {
					*buffer = image->comps[1].data[i];
					buffer++;
				}
				for (i=0; i<wh; i++) {
					*buffer = image->comps[2].data[i];
					buffer++;
				}
			} else {
				for (i=0; i<wh; i++) {
					*buffer = image->comps[0].data[w * hr - ((i) / (wr) + 1) * w + (i) % (wr)];
				}
				w = image->comps[1].w;
				wr = int_ceildivpow2(image->comps[1].w, image->comps[1].factor);
//				h = image->comps[1].h;
				hr = int_ceildivpow2(image->comps[1].h, image->comps[1].factor);
				wh = wr*hr;
				for (i=0; i<wh; i++) {
					*buffer = image->comps[1].data[w * hr - ((i) / (wr) + 1) * w + (i) % (wr)];
				}
				for (i=0; i<wh; i++) {
					*buffer = image->comps[2].data[w * hr - ((i) / (wr) + 1) * w + (i) % (wr)];
				}
			}


		} else if ((image->comps[0].w==image->comps[1].w) && (image->comps[1].w==image->comps[2].w)
		           && (image->comps[0].h==image->comps[1].h) && (image->comps[1].h==image->comps[2].h)) {

			if ((w==wr) && (h==hr)) {
				for (i=0; i<wh; i++) {
					u32 idx = 3*i;
					buffer[idx] = image->comps[0].data[i];
					buffer[idx+1] = image->comps[1].data[i];
					buffer[idx+2] = image->comps[2].data[i];
				}
			} else {
				for (i=0; i<wh; i++) {
					u32 idx = 3*i;
					buffer[idx] = image->comps[0].data[w * hr - ((i) / (wr) + 1) * w + (i) % (wr)];
					buffer[idx+1] = image->comps[1].data[w * hr - ((i) / (wr) + 1) * w + (i) % (wr)];
					buffer[idx+2] = image->comps[2].data[w * hr - ((i) / (wr) + 1) * w + (i) % (wr)];
				}
			}
		}
	}
	else if (ctx->nb_comp==4) {
		if ((image->comps[0].w==image->comps[1].w) && (image->comps[1].w==image->comps[2].w) && (image->comps[2].w==image->comps[3].w)
		        && (image->comps[0].h==image->comps[1].h) && (image->comps[1].h==image->comps[2].h) && (image->comps[2].h==image->comps[3].h)) {

			if ((w==wr) && (h==hr)) {
				for (i=0; i<wh; i++) {
					u32 idx = 4*i;
					buffer[idx] = image->comps[0].data[i];
					buffer[idx+1] = image->comps[1].data[i];
					buffer[idx+2] = image->comps[2].data[i];
					buffer[idx+3] = image->comps[3].data[i];
				}
			} else {
				for (i=0; i<wh; i++) {
					u32 idx = 4*i;
					buffer[idx] = image->comps[0].data[w * hr - ((i) / (wr) + 1) * w + (i) % (wr)];
					buffer[idx+1] = image->comps[1].data[w * hr - ((i) / (wr) + 1) * w + (i) % (wr)];
					buffer[idx+2] = image->comps[2].data[w * hr - ((i) / (wr) + 1) * w + (i) % (wr)];
					buffer[idx+3] = image->comps[3].data[w * hr - ((i) / (wr) + 1) * w + (i) % (wr)];
				}
			}
		}
	}

	opj_image_destroy(image);
	image = NULL;

	if (gf_filter_pck_get_seek_flag(pck)) {
		gf_filter_pck_discard(pck_dst);
	} else {
		gf_filter_pck_merge_properties(pck, pck_dst);
		gf_filter_pck_send(pck_dst);
	}
	gf_filter_pid_drop_packet(ctx->ipid);
	return GF_OK;
}

static GF_Err j2kdec_initialize(GF_Filter *filter)
{
#ifdef GPAC_ENABLE_COVERAGE
	if (gf_sys_is_test_mode()) {
		error_callback(NULL, NULL);
		warning_callback(NULL, NULL);
		info_callback(NULL, NULL);
	}
#endif
	return GF_OK;
}

static const GF_FilterCapability J2KCaps[] =
{
	CAP_UINT(GF_CAPS_INPUT,GF_PROP_PID_STREAM_TYPE, GF_STREAM_VISUAL),
	CAP_UINT(GF_CAPS_INPUT,GF_PROP_PID_CODECID, GF_CODECID_J2K),
	CAP_BOOL(GF_CAPS_INPUT_EXCLUDED, GF_PROP_PID_UNFRAMED, GF_TRUE),
	CAP_UINT(GF_CAPS_OUTPUT, GF_PROP_PID_STREAM_TYPE, GF_STREAM_VISUAL),
	CAP_UINT(GF_CAPS_OUTPUT, GF_PROP_PID_CODECID, GF_CODECID_RAW),
};

GF_FilterRegister J2KRegister = {
	.name = "j2kdec",
#ifdef OPENJPEG_VERSION
	.version = ""OPENJPEG_VERSION,
#elif OPENJP2
	.version = "2.x",
#else
	.version = "1.x",
#endif
	GF_FS_SET_DESCRIPTION("OpenJPEG2000 decoder")
	GF_FS_SET_HELP("This filter decodes JPEG2000 streams through OpenJPEG2000 library.")
	.private_size = sizeof(GF_J2KCtx),
	.priority = 1,
	SETCAPS(J2KCaps),
	.initialize = j2kdec_initialize,
	.configure_pid = j2kdec_configure_pid,
	.process = j2kdec_process,
};

#endif

const GF_FilterRegister *j2kdec_register(GF_FilterSession *session)
{
#ifdef GPAC_HAS_JP2
	return &J2KRegister;
#else
	return NULL;
#endif
}


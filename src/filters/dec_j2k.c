/*
*			GPAC - Multimedia Framework C SDK
*
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2000-2017
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
#include <gpac/constants.h>

#ifdef GPAC_HAS_JP2

#if !defined(__GNUC__)
# if defined(_WIN32_WCE) || defined (WIN32)
#  if defined(_DEBUG)
#   pragma comment(lib, "LibOpenJPEGd")
#  else
#   pragma comment(lib, "LibOpenJPEG")
#  endif
# endif
#endif

#include <openjpeg.h>
#include <gpac/isomedia.h>

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
		u32 ex_crc = gf_crc_32(p->value.data.ptr, p->value.data.size);
		if (ctx->cfg_crc == ex_crc) {
			return GF_OK;
		}
		ctx->cfg_crc = ex_crc;
		bs = gf_bs_new(p->value.data.ptr, p->value.data.size, GF_BITSTREAM_READ);
		ctx->height = gf_bs_read_u32(bs);
		ctx->width = gf_bs_read_u32(bs);
		ctx->nb_comp = gf_bs_read_u16(bs);
		ctx->bpp = 1 + gf_bs_read_u8(bs);
		ctx->out_size = ctx->width * ctx->height * ctx->nb_comp /* * ctx->bpp / 8 */;
		gf_bs_del(bs);

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
	GF_LOG(GF_LOG_ERROR, GF_LOG_CODEC, ("[OpenJPEG] Error %s", msg));
}
/**
sample warning callback expecting a FILE* client object
*/
void warning_callback(const char *msg, void *client_data)
{
	GF_LOG(GF_LOG_WARNING, GF_LOG_CODEC, ("[OpenJPEG] Warning %s", msg));
}
/**
sample debug callback expecting no client object
*/
void info_callback(const char *msg, void *client_data)
{
	GF_LOG(GF_LOG_INFO, GF_LOG_CODEC, ("[OpenJPEG] Info %s", msg));
}

/*
* Divide an integer by a power of 2 and round upwards.
*
* a divided by 2^b
*/
static int int_ceildivpow2(int a, int b) {
	return (a + (1 << b) - 1) >> b;
}

static GF_Err j2kdec_process(GF_Filter *filter)
{
	u32 i, w, wr, h, hr, wh, size, pf;
	char *data, *buffer;
	opj_dparameters_t parameters;	/* decompression parameters */
	opj_event_mgr_t event_mgr;		/* event manager */
	opj_dinfo_t* dinfo = NULL;	/* handle to a decompressor */
	opj_cio_t *cio = NULL;
	opj_image_t *image = NULL;
	opj_codestream_info_t cinfo;
	GF_J2KCtx *ctx = gf_filter_get_udta(filter);
	GF_FilterPacket *pck, *pck_dst;

	pck = gf_filter_pid_get_packet(ctx->ipid);
	if (!pck) {
		if (gf_filter_pid_is_eos(ctx->ipid))
			gf_filter_pid_set_eos(ctx->opid);
		return GF_OK;
	}
	data = (char *) gf_filter_pck_get_data(pck, &size);

	/* configure the event callbacks (not required) */
	memset(&event_mgr, 0, sizeof(opj_event_mgr_t));
	event_mgr.error_handler = error_callback;
	event_mgr.warning_handler = warning_callback;
	event_mgr.info_handler = info_callback;

	/* set decoding parameters to default values */
	opj_set_default_decoder_parameters(&parameters);

	/* get a decoder handle */
	dinfo = opj_create_decompress(CODEC_JP2);

	/* catch events using our callbacks and give a local context */
	opj_set_event_mgr((opj_common_ptr)dinfo, &event_mgr, stderr);

	/* setup the decoder decoding parameters using the current image and user parameters */
	opj_setup_decoder(dinfo, &parameters);

	cio = opj_cio_open((opj_common_ptr)dinfo, data, size);
	/* decode the stream and fill the image structure */
	image = opj_decode_with_info(dinfo, cio, &cinfo);

	if (!image) {
		opj_destroy_decompress(dinfo);
		opj_cio_close(cio);
		return GF_IO_ERR;
	}

	ctx->nb_comp = cinfo.numcomps;
	w = cinfo.image_w;
	h = cinfo.image_h;
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
		return GF_NOT_SUPPORTED;
	}
	if ((image->comps[0].w==2*image->comps[1].w)
		&& (image->comps[1].w==image->comps[2].w)
		&& (image->comps[0].h==2*image->comps[1].h)
		&& (image->comps[1].h==image->comps[2].h)) {
		pf = GF_PIXEL_YUV;
		ctx->out_size = 3*ctx->width*ctx->height/2;
	}
	if (ctx->pixel_format!=pf) {
		gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_PIXFMT, &PROP_UINT(pf) );
		ctx->pixel_format = pf;
	}
	if (ctx->width!=w) {
		gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_WIDTH, &PROP_UINT(w) );
		ctx->width = w;
	}
	if (ctx->height!=h) {
		gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_HEIGHT, &PROP_UINT(h) );
		ctx->height = h;
	}
	/* close the byte stream */
	opj_cio_close(cio);
	cio = NULL;

	/* gf_free( remaining structures */
	if(dinfo) {
		opj_destroy_decompress(dinfo);
		dinfo = NULL;
	}

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
	.description = "OpenJPEG2000 decoder v"OPENJPEG_VERSION,
#else
	.description = "OpenJPEG2000 decoder",
#endif
	.private_size = sizeof(GF_J2KCtx),
	SETCAPS(J2KCaps),
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


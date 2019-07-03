/*
*			GPAC - Multimedia Framework C SDK
*
*			Authors: Romain Bouqueau
*			Copyright (c) 
*                  Romain Bouqueau @ GPAC Licensing
*			        Jean Le Feuvre
*					All rights reserved
*
*  This file is part of GPAC / Dektec SDI video output filter
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

#ifdef GPAC_HAS_DTAPI

//#define FAKE_DT_API

#ifndef FAKE_DT_API
#include <DTAPI.h>
#endif

#include <gpac/constants.h>
#include <gpac/color.h>
#include <gpac/thread.h>

#if defined(WIN32) && !defined(__GNUC__)
# include <intrin.h>
#else
#  include <emmintrin.h>
#endif


typedef struct {
	struct _dtout_ctx *ctx;
	GF_FilterPid *video; //null if audio callback
	GF_FilterPid *audio; //null if video callback
} DtCbkCtx;

typedef struct _dtout_ctx
{
	//opts
	s32 bus, slot;
	GF_Fraction fps;
	Bool clip;
	u32 port;
	Double start;

	u32 width, height, pix_fmt, stride, stride_uv, uv_height, nb_planes;
	GF_Fraction framerate;
	Bool is_sending, is_configured, is_10b, is_eos;

#ifndef FAKE_DT_API
	DtMxProcess *dt_matrix;
	DtDevice *dt_device;
	DtCbkCtx audio_cbk, video_cbk;
#endif

	s64 frameNum;
	u64 init_clock, last_frame_time;

	u32 frame_dur, frame_scale;
	Bool needs_reconfigure;
} GF_DTOutCtx;

#ifndef FAKE_DT_API

static void OnNewFrameAudio(DtMxData *pData, const DtCbkCtx *ctx) {
	DTAPI_RESULT  dr;

	// Must have a valid frame
	DtMxRowData &OurRow = pData->m_Rows[0];
	if (OurRow.m_CurFrame->m_Status != DT_FRMSTATUS_OK) {
		return; // Frame is not valid so we have to assume the frame buffers are unusable
	}

	// Get frame audio data
	DtMxAudioData&  AudioData = OurRow.m_CurFrame->m_Audio;

	// Init channel status word for all (valid) audio channels
	dr = AudioData.InitChannelStatus();
}

static void OnNewFrameVideo(DtMxData *pData, const DtCbkCtx  *cbck)
{
	u64 now = gf_sys_clock_high_res();
	DtMxRowData&  OurRow = pData->m_Rows[0];
	GF_DTOutCtx *ctx = cbck->ctx;
	GF_FilterPacket *pck;
	
	if (!ctx->is_configured || ctx->is_eos) return;
	if (pData->m_NumSkippedFrames > 0) {
		GF_LOG(GF_LOG_INFO, GF_LOG_MMIO, ("[DekTecOut] [%lld] #skipped frames=%d\n", pData->m_Frame, pData->m_NumSkippedFrames));
	}

	if (!ctx->init_clock) {
		ctx->init_clock = ctx->last_frame_time = now;
	}
	GF_LOG(GF_LOG_DEBUG, GF_LOG_MMIO, ("[DekTecOut] output frame %d at %d ms - %d since last\n", ctx->frameNum, (u32)(now - ctx->init_clock) / 1000, now - ctx->last_frame_time));

	ctx->last_frame_time = now;

	if (!cbck->video) {
		GF_LOG(GF_LOG_WARNING, GF_LOG_MMIO, ("[DekTecOut] no connected pid !\n"));
		return;
	}

	// Must have a valid frame
	if (OurRow.m_CurFrame->m_Status != DT_FRMSTATUS_OK)
	{
		GF_LOG(GF_LOG_ERROR, GF_LOG_MMIO, ("[DekTecOut] Not valid frame 4k\n"));
		return; // Frame is not valid so we have to assume the frame buffers are unusable
	}
	if (pData->m_NumSkippedFrames > 0) {
		GF_LOG(GF_LOG_WARNING, GF_LOG_MMIO, ("[DekTecOut] [%lld] #skipped frames=%d\n", pData->m_Frame, pData->m_NumSkippedFrames));
	}

	pck = gf_filter_pid_get_packet(cbck->video);
	if (!pck) {
		if (gf_filter_pid_is_eos(cbck->video)) {
			ctx->is_eos = GF_TRUE;
		} else {
			GF_LOG(GF_LOG_WARNING, GF_LOG_MMIO, ("[DekTecOut] no packet ready !\n"));
		}
		return;
	}
	ctx->is_eos = GF_FALSE;

	u8 *pY, *pU, *pV;
	u32 pck_size = 0;
	u32 stride = ctx->stride, stride_uv= ctx->stride_uv;
	pY = pU = pV = NULL;
	pY = (u8 *) gf_filter_pck_get_data(pck, &pck_size);
	if (!pY) {
		GF_FilterFrameInterface *hwframe = gf_filter_pck_get_frame_interface(pck);
		if (hwframe) {
			hwframe->get_plane(hwframe, 0, (const u8 **) &pY, &stride);
			if (ctx->nb_planes>1)
				hwframe->get_plane(hwframe, 1, (const u8 **) &pU, &stride_uv);
			if (ctx->nb_planes>2)
				hwframe->get_plane(hwframe, 2, (const u8 **) &pV, &stride_uv);
		}
	}
	if (!pY) {
		gf_filter_pid_drop_packet(cbck->video);
		GF_LOG(GF_LOG_WARNING, GF_LOG_MMIO, ("[DekTecOut] No data associated with packet\n"));
		return;
	}
	if (!pU && !pV) {
		if (ctx->nb_planes>1)
			pU = pY + stride * ctx->height;
		if (ctx->nb_planes>2)
			pV = pU + stride_uv * ctx->uv_height;
	}

	//TODO - move all this into color convertion code in libgpac !

	DtMxVideoBuf&  VideoBuf = OurRow.m_CurFrame->m_Video[0];
	u8* pDstY = VideoBuf.m_Planes[0].m_pBuf;
	u8* pDstU = VideoBuf.m_Planes[1].m_pBuf;
	u8* pDstV = VideoBuf.m_Planes[2].m_pBuf;

	if (ctx->is_10b) {
		u32 nb_pix = 2 * ctx->width*ctx->height;
		int lineC = 2 * ctx->width / 2;
		unsigned char *pSrcY = pY;
		unsigned char *pSrcU = pU;
		unsigned char *pSrcV = pV;

		if (ctx->clip) {
			const u16 nYRangeMin = 4;
			const u16 nYRangeMax = 1019;
			const __m128i mMin = _mm_set1_epi16(nYRangeMin);
			const __m128i mMax = _mm_set1_epi16(nYRangeMax);

			for (u32 i = 0; i < nb_pix; i += 16) {
				_mm_storeu_si128((__m128i *)&pDstY[i], _mm_max_epi16(mMin, _mm_min_epi16(mMax, _mm_loadu_si128((__m128i const *)&pSrcY[i]))));
			}

			for (u32 h = 0; h < ctx->height / 2; h++) {
				for (u32 i = 0; i < ctx->width; i += 16) {
					_mm_storeu_si128((__m128i *)&pDstU[i], _mm_max_epi16(mMin, _mm_min_epi16(mMax, _mm_loadu_si128((__m128i const *)&pSrcU[i]))));
				}
				//420->422: copy over each U and V line
				memcpy(pDstU + lineC, pDstU, lineC);
				pSrcU += lineC;
				pDstU += 2 * lineC;
			}

			//NV12 formats
			if (!pSrcV) pSrcV = pU + 1;

			for (u32 h = 0; h < ctx->height / 2; h++) {
				for (u32 i = 0; i < ctx->width; i += 16) {
					_mm_storeu_si128((__m128i *)&pDstV[i], _mm_max_epi16(mMin, _mm_min_epi16(mMax, _mm_loadu_si128((__m128i const *)&pSrcV[i]))));
				}
				memcpy(pDstV + lineC, pDstV, lineC);
				pSrcV += lineC;
				pDstV += 2 * lineC;
			}
		}
		else {
			memcpy(pDstY, pY, stride * ctx->height);

			//420->422: copy over each U and V line
			for (u32 h = 0; h < ctx->height / 2; h++) {
				memcpy(pDstU, pSrcU, lineC);
				memcpy(pDstU + lineC, pSrcU, lineC);
				pSrcU += lineC;
				pDstU += 2 * lineC;
			}
			for (u32 h = 0; h < ctx->height / 2; h++) {
				memcpy(pDstV, pSrcV, lineC);
				memcpy(pDstV + lineC, pSrcV, lineC);
				pSrcV += lineC;
				pDstV += 2 * lineC;
			}
		}
	}
	else {
		const int lineC = ctx->width / 2;
		u32 nb_pix = ctx->width*ctx->height;
		unsigned char *pSrcY = pY;
		unsigned char *pSrcU = pU;
		unsigned char *pSrcV = pV;

		const u16 nYRangeMin = 4;
		const u16 nYRangeMax = 1019;

		for (u32 i = 0; i < nb_pix; i++) {
			u16 srcy = ((u16)(*pSrcY)) << 2;
			if (ctx->clip) {
				if (srcy < nYRangeMin)      srcy = nYRangeMin;
				else if (srcy > nYRangeMax) srcy = nYRangeMax;
			}
			*(short *)pDstY = srcy;

			pSrcY++;
			pDstY += sizeof(short); //char type but short buffer
		}

		u32 hw = ctx->width / 2;
		u32 hh = ctx->height / 2;

		for (u32 i = 0; i < hh; i++) {
			//420->422: copy over each U and V line
			u8 *p_dst_u = pDstU + sizeof(short) * lineC * i * 2;
			u8 *p_dst_v = pDstV + sizeof(short) * lineC * i * 2;
			u8 *p_src_u = NULL;
			u8 *p_src_v = NULL;
			u8 *op_dst_u = p_dst_u;
			u8 *op_dst_v = p_dst_v;

			//NV12 formats
			if (!pV) {
				p_src_u = pSrcU + 2 * ctx->stride_uv * i;
				p_src_v = p_src_u + 1;
			}
			else {
				p_src_u = pSrcU + ctx->stride_uv * i;
				p_src_v = pSrcV + ctx->stride_uv * i;
			}

			for (u32 j = 0; j < hw; j++) {
				u16 srcu = ((u16)(*p_src_u)) << 2;
				u16 srcv = ((u16)(*p_src_v)) << 2;

				if (ctx->clip) {
					if (srcu < nYRangeMin)      srcu = nYRangeMin;
					else if (srcu > nYRangeMax) srcu = nYRangeMax;

					if (srcv < nYRangeMin)      srcv = nYRangeMin;
					else if (srcv > nYRangeMax) srcv = nYRangeMax;
				}

				*(short *)p_dst_u = srcu;
				p_src_u += 2;
				p_dst_u += sizeof(short); //char type but short buffer

				*(short *)p_dst_v = srcv;
				p_src_v += 2;
				p_dst_v += sizeof(short); //char type but short buffer
			}
			memcpy(op_dst_u + lineC * sizeof(short), op_dst_u, lineC * sizeof(short));
			memcpy(op_dst_v + lineC * sizeof(short), op_dst_v, lineC * sizeof(short));
		}
	}

	GF_LOG(GF_LOG_DEBUG, GF_LOG_MMIO, ("[DekTecOut] wrote YUV data in "LLU" us\n", gf_sys_clock_high_res() - now));
	gf_filter_pid_drop_packet(cbck->video);
	ctx->frameNum++;
}

static void OnNewFrame(DtMxData *pData, void *pOpaque) {
	const DtCbkCtx *ctx = ((const DtCbkCtx*)pOpaque);
	if (ctx->video)
		OnNewFrameVideo(pData, ctx);
	else if (ctx->audio)
		OnNewFrameAudio(pData, ctx);
}

static void dtout_disconnect(GF_DTOutCtx *ctx)
{
	GF_LOG(GF_LOG_DEBUG, GF_LOG_MMIO, ("[DekTecOut] Disconnecting\n"));
	DtMxProcess *Matrix = ctx->dt_matrix;
	ctx->is_configured = GF_FALSE;

	DTAPI_RESULT res = Matrix->Stop();
	if (res != DTAPI_OK)
		GF_LOG(GF_LOG_ERROR, GF_LOG_MMIO, ("[DekTecOut] Can't stop transmission: %s\n", DtapiResult2Str(res)));

	Matrix->Reset();
	ctx->dt_device->Detach();
	ctx->is_sending = GF_FALSE;
}


static GF_Err dtout_configure_pid(GF_Filter *filter, GF_FilterPid *pid, Bool is_remove)
{
	DTAPI_RESULT res;
	int port = 1;
	Bool send_play = GF_FALSE;
	Bool is4K = GF_FALSE;
	Bool is_audio = GF_FALSE;
	GF_DTOutCtx *ctx = (GF_DTOutCtx*)gf_filter_get_udta(filter);
	const GF_PropertyValue *p;
	DtDevice *dt_device = ctx->dt_device;

	if (is_remove) {
		if (ctx->audio_cbk.audio == pid) {
			ctx->audio_cbk.audio = NULL;
		}
		else if (ctx->video_cbk.video == pid) {
			ctx->video_cbk.video = NULL;
		}
		if (ctx->is_sending && !ctx->audio_cbk.audio && !ctx->video_cbk.video) {
			dtout_disconnect(ctx);
		}
		return GF_OK;
	}

	p = gf_filter_pid_get_property(pid, GF_PROP_PID_STREAM_TYPE);
	if (!p) return GF_NOT_SUPPORTED;
	if (p->value.uint == GF_STREAM_VISUAL) {
		if (!ctx->video_cbk.video) {
			ctx->video_cbk.video = pid;
			send_play = GF_TRUE;
		}
		else if (ctx->video_cbk.video != pid) return GF_REQUIRES_NEW_INSTANCE;
	}
	else if (p->value.uint == GF_STREAM_AUDIO) {
		if (!ctx->audio_cbk.audio) {
			ctx->audio_cbk.audio = pid;
			send_play = GF_TRUE;
		}
		else if (ctx->audio_cbk.audio != pid) return GF_REQUIRES_NEW_INSTANCE;
		is_audio=GF_TRUE;
	}
	else {
		return GF_NOT_SUPPORTED;
	}

	p = gf_filter_pid_get_property(pid, GF_PROP_PID_CODECID);
	if (!p || (p->value.uint != GF_CODECID_RAW)) return GF_NOT_SUPPORTED;


	if (send_play) {
		GF_FilterEvent fevt;
		//set a minimum buffer (although we don't buffer)
		GF_FEVT_INIT(fevt, GF_FEVT_BUFFER_REQ, pid);
		fevt.buffer_req.max_buffer_us = 200000;
		gf_filter_pid_send_event(pid, &fevt);

		gf_filter_pid_init_play_event(pid, &fevt, ctx->start, 1.0, "DekTecOut");
		gf_filter_pid_send_event(pid, &fevt);
	}

	if (is_audio) {
		//todo
		return GF_NOT_SUPPORTED;
	}

	u32 width = 0;
	u32 height = 0;
	u32 pix_fmt = 0;
	u32 stride = 0;
	u32 stride_uv = 0;
	p = gf_filter_pid_get_property(pid, GF_PROP_PID_WIDTH);
	if (p) width = p->value.uint;
	p = gf_filter_pid_get_property(pid, GF_PROP_PID_HEIGHT);
	if (p) height = p->value.uint;
	p = gf_filter_pid_get_property(pid, GF_PROP_PID_PIXFMT);
	if (p) pix_fmt = p->value.uint;
	p = gf_filter_pid_get_property(pid, GF_PROP_PID_STRIDE);
	if (p) stride = p->value.uint;
	p = gf_filter_pid_get_property(pid, GF_PROP_PID_STRIDE_UV);
	if (p) stride_uv = p->value.uint;

	//not configured yet
	if (!width || !height || !pix_fmt) return GF_OK;

	Bool is_10_bits = GF_FALSE;
	switch (pix_fmt) {
	case GF_PIXEL_YUV:
		break;
	case GF_PIXEL_YUV_10:
		is_10_bits = GF_TRUE;
		break;
	case GF_PIXEL_YUV422:
		break;
	case GF_PIXEL_YUV422_10:
		is_10_bits = GF_TRUE;
		break;
	case GF_PIXEL_YUV444:
		break;
	case GF_PIXEL_YUV444_10:
		is_10_bits = GF_TRUE;
		break;
	case GF_PIXEL_NV12:
	case GF_PIXEL_NV21:
		break;
	case GF_PIXEL_NV12_10:
	case GF_PIXEL_NV21_10:
		is_10_bits = GF_TRUE;
		break;
	default:
		GF_PropertyValue val;
		val.type = GF_PROP_UINT;
		val.value.uint = GF_PIXEL_YUV;
		gf_filter_pid_negociate_property(pid, GF_PROP_PID_PIXFMT, &val);
		return GF_OK;
	}
	if (!stride) stride = width * (is_10_bits ? 2 : 1);
	if (!stride_uv) stride_uv = stride / 2;

	GF_Fraction fps = { 30, 1 };
	p = gf_filter_pid_get_property(pid, GF_PROP_PID_FPS);
	if (p) fps = p->value.frac;

	if ((ctx->width == width) && (ctx->height == height) && (ctx->pix_fmt == pix_fmt) && (ctx->framerate.num * fps.den == ctx->framerate.den * fps.num) && (ctx->stride==stride) && ctx->is_configured)
		return GF_OK;

	ctx->width = width;
	ctx->height = height;
	ctx->pix_fmt = pix_fmt;
	ctx->framerate = fps;
	ctx->is_10b = is_10_bits;
	ctx->stride = stride;
	ctx->stride_uv = stride_uv;

	gf_pixel_get_size_info((GF_PixelFormat) pix_fmt, width, height, NULL, &ctx->stride, &ctx->stride_uv, &ctx->nb_planes, &ctx->uv_height);

	int dFormat = -1, linkStd = -1;

	GF_LOG(GF_LOG_INFO, GF_LOG_MMIO, ("[DekTecOut] Reconfigure to %dx%d @ %g FPS %d/%d bits src - SDI clipping: %s\n", width, height, fps.num, fps.den, is_10_bits ? "10" : "8", ctx->clip ? "yes" : "no"));
	if (ctx->is_configured) {
		DtMxProcess *Matrix = ctx->dt_matrix;
		if (ctx->is_sending) {
			res = Matrix->Stop();
			if (res != DTAPI_OK) {
				GF_LOG(GF_LOG_ERROR, GF_LOG_MMIO, ("[DekTecOut] Can't stop transmission: %s\n", DtapiResult2Str(res)));
			}
		}
		Matrix->Reset();
		ctx->is_configured = GF_FALSE;
		ctx->is_sending = GF_FALSE;
	}

	if (fps.num == 30 * fps.den) {
		ctx->frame_scale = 30;
		ctx->frame_dur = 1;
		if ((ctx->width == 1280) && (ctx->height == 720)) {
			dFormat = DTAPI_IOCONFIG_720P30;
		}
		else if ((ctx->width == 1920) && (ctx->height == 1080)) {
			dFormat = DTAPI_IOCONFIG_1080P30;
		}
		else if ((ctx->width == 3840) && (ctx->height == 2160)) {
			dFormat = DTAPI_VIDSTD_2160P30;
		}
	}
	else if (fps.num == 50 * fps.den) {
		ctx->frame_scale = 50;
		ctx->frame_dur = 1;
		if ((ctx->width == 1280) && (ctx->height == 720)) {
			dFormat = DTAPI_IOCONFIG_720P50;
		}
		else if ((ctx->width == 1920) && (ctx->height == 1080)) {
			dFormat = DTAPI_IOCONFIG_1080P50;
		}
		else if ((ctx->width == 3840) && (ctx->height == 2160)) {
			dFormat = DTAPI_VIDSTD_2160P50;
		}
	}
	else if (fps.num == 60 * fps.den) {
		ctx->frame_scale = 60;
		ctx->frame_dur = 1;
		if ((ctx->width == 1280) && (ctx->height == 720)) {
			dFormat = DTAPI_IOCONFIG_720P60;
		}
		else if ((ctx->width == 1920) && (ctx->height == 1080)) {
			dFormat = DTAPI_IOCONFIG_1080P60;
		}
		else if ((ctx->width == 3840) && (ctx->height == 2160)) {
			dFormat = DTAPI_VIDSTD_2160P60;
		}
		GF_LOG(GF_LOG_WARNING, GF_LOG_MMIO, ("[DekTecOut] Dealing with 60 fps video is not stable, use with caution.\n"));
	}
	else if (fps.num*100 == 5994 * fps.den) {
		ctx->frame_scale = 3000;
		ctx->frame_dur = 1001;

		if ((ctx->width == 1280) && (ctx->height == 720)) {
			dFormat = DTAPI_IOCONFIG_720P59_94;
		}
		else if ((ctx->width == 1920) && (ctx->height == 1080)) {
			dFormat = DTAPI_IOCONFIG_1080P59_94;
		}
		else if ((ctx->width == 3840) && (ctx->height == 2160)) {
			dFormat = DTAPI_VIDSTD_2160P59_94;
		}
	}
	else if (fps.num == 24 * fps.den) {
		ctx->frame_scale = 24;
		ctx->frame_dur = 1;
		if ((ctx->width == 1280) && (ctx->height == 720)) {
			dFormat = DTAPI_IOCONFIG_720P24;
		}
		else if ((ctx->width == 1920) && (ctx->height == 1080)) {
			dFormat = DTAPI_IOCONFIG_1080P24;
		}
		else if ((ctx->width == 3840) && (ctx->height == 2160)) {
			dFormat = DTAPI_VIDSTD_2160P24;
		}
	}
	else if (fps.num == 25 * fps.den) {
		ctx->frame_scale = 25;
		ctx->frame_dur = 1;
		if ((ctx->width == 1280) && (ctx->height == 720)) {
			dFormat = DTAPI_IOCONFIG_720P25;
		}
		else if ((ctx->width == 1920) && (ctx->height == 1080)) {
			dFormat = DTAPI_IOCONFIG_1080P25;
		}
		else if ((ctx->width == 3840) && (ctx->height == 2160)) {
			dFormat = DTAPI_VIDSTD_2160P25;
		}
	}

	if (dFormat == -1) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_MMIO, ("[DekTecOut] Unsupported frame rate %d/%d / resolution %dx%d\n", fps.num, fps.den, ctx->width, ctx->height));
		return GF_FILTER_NOT_SUPPORTED;
	}

	if ((ctx->width == 3840) && (ctx->height == 2160)) {
		is4K = GF_TRUE;
		linkStd = DTAPI_VIDLNK_4K_SMPTE425B;
	}

	port = ctx->port ? ctx->port : 1;

	DtMxPort mxPort;
	if (!is4K)
	{
		res = dt_device->SetIoConfig(port, DTAPI_IOCONFIG_IODIR, DTAPI_IOCONFIG_OUTPUT, DTAPI_IOCONFIG_OUTPUT);
		if (res != DTAPI_OK) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_MMIO, ("[DekTecOut] Can't set I/O config for the device: %s\n", DtapiResult2Str(res)));
			return GF_BAD_PARAM;
		}

		int IoStdValue = -1, IoStdSubValue = -1;
		res = DtapiVidStd2IoStd(dFormat, IoStdValue, IoStdSubValue);
		if (res != DTAPI_OK) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_MMIO, ("[DekTecOut] Unknown VidStd: %s\n", DtapiResult2Str(res)));
			return GF_BAD_PARAM;
		}

		res = dt_device->SetIoConfig(port, DTAPI_IOCONFIG_IOSTD, IoStdValue, IoStdSubValue);
		if (res != DTAPI_OK) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_MMIO, ("[DekTecOut] Can't set I/O config: %s\n", DtapiResult2Str(res)));
			return GF_BAD_PARAM;
		}
		res = mxPort.AddPhysicalPort(dt_device, port);
		if (res != DTAPI_OK) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_MMIO, ("[DekTecOut] Failed to add port %d to logical port", port));
			return GF_BAD_PARAM;
		}
	}
	else
	{
		DtMxPort mxPort4k(dFormat, linkStd);
		for (int i = 0; i < 4; i++)
		{
			res = dt_device->SetIoConfig(i + 1, DTAPI_IOCONFIG_IODIR, DTAPI_IOCONFIG_OUTPUT, DTAPI_IOCONFIG_OUTPUT);
			if (res != DTAPI_OK) {
				GF_LOG(GF_LOG_ERROR, GF_LOG_MMIO, ("[DekTecOut] Can't set I/O config for the device: %s\n", DtapiResult2Str(res)));
				return GF_BAD_PARAM;
			}
		}

		int IoStdValue = -1, IoStdSubValue = -1;
		res = DtapiVidStd2IoStd(dFormat, linkStd, IoStdValue, IoStdSubValue);
		if (res != DTAPI_OK) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_MMIO, ("[DekTecOut] Unknown VidStd: %s\n", DtapiResult2Str(res)));
			return GF_BAD_PARAM;
		}

		for (int i = 0; i < 4; i++)
		{
			res = dt_device->SetIoConfig(i + 1, DTAPI_IOCONFIG_IOSTD, IoStdValue, IoStdSubValue);
			if (res != DTAPI_OK) {
				GF_LOG(GF_LOG_ERROR, GF_LOG_MMIO, ("[DekTecOut] Can't set I/O config: %s\n", DtapiResult2Str(res)));
				return GF_BAD_PARAM;
			}
		}
		for (int i = 0; i < 4; i++)
		{
			res = mxPort4k.AddPhysicalPort(dt_device, i + 1);
			if (res != DTAPI_OK) {
				GF_LOG(GF_LOG_ERROR, GF_LOG_MMIO, ("[DekTecOut] Failed to add port %d to logical 4k port", (i + 1)));
				return GF_BAD_PARAM;
			}
		}

		DtIoConfig  Cfg;
		std::vector<DtIoConfig>  IoConfigs;
		DtHwFuncDesc*  pHwf = dt_device->m_pHwf;
		for (int i = 0; i < dt_device->m_pHwf->m_DvcDesc.m_NumPorts; i++, pHwf++)
		{
			// Check if the port has GENNREF capability
			if ((pHwf->m_Flags & DTAPI_CAP_GENREF) == 0)
				continue;   // No GENREF cap

			Cfg.m_Port = pHwf->m_Port;
			Cfg.m_Group = DTAPI_IOCONFIG_GENREF;

			// Check if this is the internal GENREF (has DTAPI_CAP_VIRTUAL)?
			if ((pHwf->m_Flags & DTAPI_CAP_VIRTUAL) != 0)
				Cfg.m_Value = DTAPI_IOCONFIG_TRUE;   // The internal => enable as GENREF
			else
				Cfg.m_Value = DTAPI_IOCONFIG_FALSE;  // Not the internal => disable as GENREF
			Cfg.m_SubValue = -1;
			Cfg.m_ParXtra[0] = Cfg.m_ParXtra[1] = -1;
			// Add to list
			IoConfigs.push_back(Cfg);

			// Set the IO-standard for 'THE' GENREF port
			if (Cfg.m_Value != DTAPI_IOCONFIG_TRUE)
				continue;
			Cfg.m_Group = DTAPI_IOCONFIG_IOSTD;
			// Set a standard with matching frame rate
			DtVidStdInfo  VidInfo;
			res = ::DtapiGetVidStdInfo(dFormat, linkStd, VidInfo);
			Cfg.m_Value = DTAPI_IOCONFIG_HDSDI;
			switch ((int)VidInfo.m_Fps)
			{
			case 23: Cfg.m_SubValue = DTAPI_IOCONFIG_720P23_98; break;
			case 24: Cfg.m_SubValue = DTAPI_IOCONFIG_720P24; break;
			case 25: Cfg.m_SubValue = DTAPI_IOCONFIG_720P25; break;
			case 29: Cfg.m_SubValue = DTAPI_IOCONFIG_720P29_97; break;
			case 30: Cfg.m_SubValue = DTAPI_IOCONFIG_720P30; break;
			case 50: Cfg.m_SubValue = DTAPI_IOCONFIG_720P50; break;
			case 59: Cfg.m_SubValue = DTAPI_IOCONFIG_720P59_94; break;
			case 60: Cfg.m_SubValue = DTAPI_IOCONFIG_720P60; break;
			}
			Cfg.m_ParXtra[0] = Cfg.m_ParXtra[1] = -1;
			// Add to list
			IoConfigs.push_back(Cfg);
		}
		res = dt_device->SetIoConfig(IoConfigs.data(), (int)IoConfigs.size());
		if (res != DTAPI_OK) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_MMIO, ("[DekTecOut] Failed to apply genref\n"));
			return GF_BAD_PARAM;
		}
		mxPort = mxPort4k;
	}

	res = ctx->dt_matrix->AttachRowToOutput(0, mxPort);
	if (res != DTAPI_OK) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_MMIO, ("[DekTecOut] Can't attach to port %d: %s\n", port, DtapiResult2Str(res)));
		return GF_BAD_PARAM;
	}

	/* --- Set row configuration --- */
	DtMxRowConfig  RowConfig;
	RowConfig.m_Enable = true;
	RowConfig.m_RowSize = 1;       // Keep a history of one frame

	// Enable the video
	RowConfig.m_VideoEnable = true;
	//todo, make this configurable !
	RowConfig.m_Video.m_PixelFormat = DT_PXFMT_YUV422P_16B;  // Use a planar format (16-bit)
	RowConfig.m_Video.m_StartLine1 = RowConfig.m_Video.m_StartLine2 = 1;
	RowConfig.m_Video.m_NumLines1 = RowConfig.m_Video.m_NumLines2 = -1;
	RowConfig.m_Video.m_Scaling = DTAPI_SCALING_OFF;   // No scaling for this row
	RowConfig.m_Video.m_LineAlignment = 8; // Want an 8-byte alignment

	// Enable audio
	RowConfig.m_AudioEnable = true;
	// We will add four channels (2x stereo)
	for (int i = 0; i < 4; i++)
	{
		DtMxAudioConfig  AudioConfig;
		AudioConfig.m_Index = i;
		AudioConfig.m_DeEmbed = false;  // Nothing to de-embed for an output only row
		AudioConfig.m_OutputMode = DT_OUTPUT_MODE_ADD;  // We want to add audio samples
		AudioConfig.m_Format = DT_AUDIO_SAMPLE_PCM;     // We will add PCM samples

		// Add to config list
		RowConfig.m_Audio.push_back(AudioConfig);
	}

	// Apply the row configuration
	res = ctx->dt_matrix->SetRowConfig(0, RowConfig);
	if (res != DTAPI_OK) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_MMIO, ("[DekTecOut] Failed to set row 0 config"));
		return GF_BAD_PARAM;
	}

	/* --- Final Configuration --- */
	// Determine the default recommended end-to-end delay for this configuration

	int  DefEnd2EndDelay = 0;
	double CbFrames = 0.0;    // Time available for the call-back method
	res = ctx->dt_matrix->GetDefEndToEndDelay(DefEnd2EndDelay, CbFrames);
	GF_LOG(GF_LOG_INFO, GF_LOG_MMIO, ("[DekTecOut]  Default end-to-end delay = %d Frames\n", DefEnd2EndDelay));
	GF_LOG(GF_LOG_INFO, GF_LOG_MMIO, ("[DekTecOut]  Time-for-Callback = %.1f Frame Period\n", CbFrames));
	// Apply the default end-to-end delay
	res = ctx->dt_matrix->SetEndToEndDelay(DefEnd2EndDelay);
	if (res != DTAPI_OK) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_MMIO, ("[DekTecOut] Failed to set the end-to-end delay (delay=%d)", DefEnd2EndDelay));
		return GF_BAD_PARAM;
	}

	res = ctx->dt_matrix->AddMatrixCbFunc(OnNewFrame, &ctx->audio_cbk);
	if (res != DTAPI_OK) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_MMIO, ("[DekTecOut] Failed register audio callback function"));
		return GF_BAD_PARAM;
	}
	res = ctx->dt_matrix->AddMatrixCbFunc(OnNewFrame, &ctx->video_cbk);
	if (res != DTAPI_OK) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_MMIO, ("[DekTecOut] Failed register video callback function"));
		return GF_BAD_PARAM;
	}

	ctx->frameNum = 1;
	ctx->is_configured = GF_TRUE;
	// Start transmission right now - this function is called during the blit(), and the object clock is initialized
	//after the blit, starting now will reduce the number of skipped frames at the dektec startup
	res = ctx->dt_matrix->Start();
	if (res != DTAPI_OK) {
		GF_LOG(GF_LOG_WARNING, GF_LOG_MMIO, ("[DekTecOut] Can't start transmission: %s.\n", DtapiResult2Str(res)));
	}
	else {
		GF_LOG(GF_LOG_INFO, GF_LOG_MMIO, ("[DekTecOut] Transmission started.\n"));
		ctx->is_sending = GF_TRUE;
	}
	return GF_OK;
}

static GF_Err dtout_initialize(GF_Filter *filter)
{
	GF_DTOutCtx *ctx = (GF_DTOutCtx*)gf_filter_get_udta(filter);
	DTAPI_RESULT res;

	ctx->dt_device = new DtDevice;
	ctx->dt_matrix = new DtMxProcess;
	if (!ctx->dt_matrix || !ctx->dt_device) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_MMIO, ("[DekTecOut] DTA API couldn't be initialized.\n"));
		if (ctx->dt_device) delete ctx->dt_device;
		if (ctx->dt_matrix) delete ctx->dt_matrix;
		ctx->dt_device = NULL;
		ctx->dt_matrix = NULL;
		return GF_IO_ERR;
	}

	ctx->audio_cbk.ctx = ctx;
	ctx->video_cbk.ctx = ctx;

	if ((ctx->bus >= 0) && (ctx->slot >= 0)) {
		res = ctx->dt_device->AttachToSlot(ctx->bus, ctx->slot);
		if (res != DTAPI_OK) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_MMIO, ("[DekTecOut] No DTA found on PIC bus %d slot %d: %s - trying enumerating cards\n", ctx->bus, ctx->slot, DtapiResult2Str(res)));
		}
		else {
			return GF_OK;
		}
	}

	res = ctx->dt_device->AttachToType(2174);
	if (res != DTAPI_OK) res = ctx->dt_device->AttachToType(2154);

	if (res != DTAPI_OK) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_MMIO, ("[DekTecOut] No DTA 2174 or 2154 in system: %s\n", DtapiResult2Str(res)));
		return GF_BAD_PARAM;
	}
	return GF_OK;
}

static void dtout_finalize(GF_Filter *filter)
{
	GF_DTOutCtx *ctx = (GF_DTOutCtx*)gf_filter_get_udta(filter);

	if (ctx->is_sending) {
		dtout_disconnect(ctx);
	}
	delete(ctx->dt_matrix);
	delete(ctx->dt_device);
}

#endif //FAKE_DT_API

static GF_Err dtout_process(GF_Filter *filter)
{
	GF_DTOutCtx *ctx = (GF_DTOutCtx*)gf_filter_get_udta(filter);
	if (!ctx->is_configured && !ctx->is_sending) {
#ifndef FAKE_DT_API
		if (ctx->audio_cbk.audio) gf_filter_pid_get_packet(ctx->audio_cbk.audio);
		if (ctx->video_cbk.video) gf_filter_pid_get_packet(ctx->video_cbk.video);
#endif
	}
	if (ctx->is_eos)
		return GF_EOS;
	gf_filter_ask_rt_reschedule(filter, 200000);
	return GF_OK;
}



#define OFFS(_n)	#_n, offsetof(GF_DTOutCtx, _n)

static const GF_FilterArgs DTOutArgs[] =
{
	{ OFFS(bus), "PCI bus number - if not set, device discovery is used", GF_PROP_SINT, "-1", NULL, GF_FS_ARG_HINT_EXPERT},
	{ OFFS(slot), "PCI bus number - if not set, device discovery is used", GF_PROP_SINT, "-1", NULL, GF_FS_ARG_HINT_EXPERT },
	{ OFFS(fps), "default FPS to use if input stream fps cannot be detected", GF_PROP_FRACTION, "30/1", NULL, GF_FS_ARG_HINT_ADVANCED },
	{ OFFS(clip), "clip YUV data to valid SDI range, slower", GF_PROP_BOOL, "false", NULL, GF_FS_ARG_HINT_ADVANCED },
	{ OFFS(port), "set sdi output port of card", GF_PROP_UINT, "1", NULL, GF_FS_ARG_HINT_ADVANCED },
	{ OFFS(start), "set playback start offset, [-1, 0] means percent of media dur, eg -1 == dur", GF_PROP_DOUBLE, "0.0", NULL, GF_FS_ARG_HINT_NORMAL },
	{ 0 }
};

static GF_FilterCapability DTOutCaps[3];

GF_FilterRegister DTOutRegister;

extern "C" {

GPAC_MODULE_EXPORT
GF_FilterRegister *RegisterFilter(GF_FilterSession *session)
{
	memset(DTOutCaps, 0, sizeof(DTOutCaps));
	DTOutCaps[0].code = GF_PROP_PID_STREAM_TYPE;
	DTOutCaps[0].flags = GF_CAPS_INPUT;
	DTOutCaps[0].val.type = GF_PROP_UINT;
	DTOutCaps[0].val.value.uint = GF_STREAM_VISUAL;

	DTOutCaps[1].code = GF_PROP_PID_STREAM_TYPE;
	DTOutCaps[1].flags = GF_CAPS_INPUT;
	DTOutCaps[1].val.type = GF_PROP_UINT;
	DTOutCaps[1].val.value.uint = GF_STREAM_AUDIO;

	DTOutCaps[2].code = GF_PROP_PID_CODECID;
	DTOutCaps[2].flags = GF_CAPS_INPUT;
	DTOutCaps[2].val.type = GF_PROP_UINT;
	DTOutCaps[2].val.value.uint = GF_CODECID_RAW;

	memset(&DTOutRegister, 0, sizeof(GF_FilterRegister));
	DTOutRegister.name = "dtout";
#ifndef GPAC_DISABLE_DOC
	DTOutRegister.description = "DekTec SDIOut";
	DTOutRegister.help = "This filter provides SDI output to be used with __DTA 2174__ or __DTA 2154__ cards.";
#endif
	DTOutRegister.private_size = sizeof(GF_DTOutCtx);
	DTOutRegister.args = DTOutArgs;
	DTOutRegister.caps = DTOutCaps;
	DTOutRegister.nb_caps = 3;
#ifndef FAKE_DT_API
	DTOutRegister.initialize = dtout_initialize;
	DTOutRegister.finalize = dtout_finalize;
	DTOutRegister.configure_pid = dtout_configure_pid;
#endif
	DTOutRegister.process = dtout_process;


	return &DTOutRegister;
}

}

#endif //GPAC_HAS_DTAPI

 

/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Romain Bouqueau
 *			Copyright (c) 
 *                  Romain Bouqueau @ GPAC Licensing
 *			        Jean Le Feuvre
 *					All rights reserved
 *
 *  This file is part of GPAC / Dektec SDI video render module
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

//#define _DTAPI_DISABLE_AUTO_LINK
#include <DTAPI.h>

extern "C" {

/*driver interfaces*/
#include <gpac/modules/video_out.h>
#include <gpac/modules/audio_out.h>
#include <gpac/constants.h>
#include <gpac/setup.h>
#include <gpac/color.h>
#include <gpac/thread.h>

typedef struct
{
	unsigned char *pixels_YUV_source;

	u32 width, height;
	DtMxProcess *m_Matrix;

	DtDevice *dvc;
	Bool is_sending, is_configured, is_10b, clip_sdi;

	s64 frameNum;
	FILE *pFile;
	Bool is_in_flush;
	Bool frame_transfered;
	
	s64 lastFrameNum;
	u64 init_clock, last_frame_time;

	u32 frame_dur, frame_scale;
	Bool needs_reconfigure;
	u32 force_width, force_height;
} DtContext;

static void OnNewFrameVideo(DtMxData* pData, void* pOpaque)
{
	u64 now = gf_sys_clock_high_res();
	DtMxRowData&  OurRow = pData->m_Rows[0];
	GF_VideoOutput *dr = (GF_VideoOutput *)pOpaque;
	DtContext *dtc = (DtContext*)dr->opaque;
	if (!dtc->is_configured) return;

	if (!dtc->init_clock)  {
		dtc->init_clock = dtc->last_frame_time = now;
	}
	GF_LOG(GF_LOG_DEBUG, GF_LOG_MMIO, ("[DekTecOut] output frame %d at %d ms - %d since last\n", dtc->frameNum, (u32) (now - dtc->init_clock)/1000,  now-dtc->last_frame_time));
	
	dtc->last_frame_time = now;

	if (dtc->lastFrameNum == dtc->frameNum) {
		GF_LOG(GF_LOG_INFO, GF_LOG_MMIO, ("[DekTec Out] Skipping same frame %d - compositor in flush %d\n", dtc->frameNum, dtc->is_in_flush));
		dtc->frame_transfered = GF_TRUE;
		return;
	}

	dtc->lastFrameNum = dtc->frameNum;

	// Must have a valid frame
	if (OurRow.m_CurFrame->m_Status != DT_FRMSTATUS_OK)
	{
		GF_LOG(GF_LOG_ERROR, GF_LOG_MMIO, ("[DekTecOut] Not valid frame 4k\n"));
		dtc->frame_transfered = GF_TRUE;
		exit(0);
		return; // Frame is not valid so we have to assume the frame buffers are unusable
	}
	if (pData->m_NumSkippedFrames>0) {
		GF_Event evt;
		evt.type = GF_EVENT_SYNC_LOST;
		evt.sync_loss.sync_loss_ms = (u32) ( (u64) pData->m_NumSkippedFrames * dtc->frame_dur * 1000) / dtc->frame_scale;
		dr->on_event(dr->evt_cbk_hdl, &evt);
		GF_LOG(GF_LOG_INFO, GF_LOG_MMIO, ("[DekTecOut] [%lld] #skipped frames=%d\n", pData->m_Frame, pData->m_NumSkippedFrames));
	}

	DtMxVideoBuf&  VideoBuf = OurRow.m_CurFrame->m_Video[0];
	unsigned char* pDstY = VideoBuf.m_Planes[0].m_pBuf;
	unsigned char* pDstU = VideoBuf.m_Planes[1].m_pBuf;
	unsigned char* pDstV = VideoBuf.m_Planes[2].m_pBuf;

	if (dtc->is_10b) {
		u32 nb_pix = 2*dtc->width*dtc->height;
		int lineC = 2*dtc->width / 2;
		unsigned char *pSrcY = dtc->pixels_YUV_source;
		unsigned char *pSrcU = dtc->pixels_YUV_source + 2 * dtc->width*dtc->height;
		unsigned char *pSrcV = pSrcU + (2 * dtc->width*dtc->height / 4);

		if (dtc->clip_sdi) {
			u16 nYRangeMin = 4;
			u16 nYRangeMax = 1019;

			__m128i mMin = _mm_set1_epi16(nYRangeMin);
			__m128i mMax = _mm_set1_epi16(nYRangeMax);

			for (u32 i = 0; i < nb_pix; i += 16) {
				_mm_storeu_si128((__m128i *)&pDstY[i], _mm_max_epi16(mMin, _mm_min_epi16(mMax, _mm_loadu_si128((__m128i const *)&pSrcY[i]))));
			}

			for (u32 h = 0; h < dtc->height/2; h++) {
				for (u32 i = 0; i < dtc->width; i += 16) {
					_mm_storeu_si128((__m128i *)&pDstU[i], _mm_max_epi16(mMin, _mm_min_epi16(mMax, _mm_loadu_si128((__m128i const *)&pSrcU[i]))));
					_mm_storeu_si128((__m128i *)&pDstV[i], _mm_max_epi16(mMin, _mm_min_epi16(mMax, _mm_loadu_si128((__m128i const *)&pSrcV[i]))));
				}
				//420->422: copy over each U and V line
				memcpy(pDstU+lineC, pDstU, lineC);
				memcpy(pDstV+lineC, pDstV, lineC);

				pSrcU += lineC;
				pSrcV += lineC;
				pDstU += 2 * lineC;
				pDstV += 2 * lineC;
			}
		} else {
			memcpy(pDstY, dtc->pixels_YUV_source, 2 * dtc->width*dtc->height);
	
			//420->422: copy over each U and V line
			for (u32 h = 0; h < dtc->height/2; h++) {
				memcpy(pDstU, pSrcU, lineC);
				memcpy(pDstV, pSrcV, lineC);
				memcpy(pDstU+lineC, pSrcU, lineC);
				memcpy(pDstV+lineC, pSrcV, lineC);
				pSrcU += lineC;
				pSrcV += lineC;
				pDstU += 2 * lineC;
				pDstV += 2 * lineC;
			}
		}
	} else {
		u32 nb_pix = dtc->width*dtc->height;
		int lineC = dtc->width / 2;
		unsigned char *pSrcY = dtc->pixels_YUV_source;
		unsigned char *pSrcU = dtc->pixels_YUV_source + dtc->width * dtc->height;
		unsigned char *pSrcV = pSrcU + (dtc->width*dtc->height / 4);

		for (u32 i=0; i<nb_pix; i++) {
			u16 srcy = ((u16) (*pSrcY)) << 2;
			if (dtc->clip_sdi) {
				if (srcy<4) srcy = 4;
				else if (srcy>1019) srcy = 1019;
			}
			*(short *)pDstY = srcy;

			pSrcY++;
			pDstY+=2;//char type but short buffer
		}

		nb_pix = dtc->width*dtc->height/4;
		for (u32 i=0; i<nb_pix; i++) {
			u16 srcu = ((u16) (*pSrcU) ) << 2;
			u16 srcv = ((u16) (*pSrcV) ) << 2;

			if (dtc->clip_sdi) {
				if (srcu<4) srcu = 4;
				else if (srcu>1019) srcu = 1019;

				if (srcv<4) srcv = 4;
				else if (srcv>1019) srcv = 1019;
			}

			*(short *)pDstU = srcu;
			*(short *)(pDstU+lineC) = srcu;

			pSrcU++;
			pDstU+=2;//char type but short buffer

			*(short *)pDstV = srcv;
			*(short *)(pDstV+lineC) = srcv;

			pSrcV++;
			pDstV+=2;//char type but short buffer
		}

	}

	GF_LOG(GF_LOG_DEBUG, GF_LOG_MMIO, ("[DekTecOut] wrote YUV data in "LLU" us\n", gf_sys_clock_high_res() - now));
	dtc->frame_transfered = GF_TRUE;
}


static GF_Err Dektec_Flush(GF_VideoOutput *dr, GF_Window *dest)
{
	DtContext *dtc = (DtContext*)dr->opaque;
	//DtFrameBuffer *dtf = dtc->dtf;
	DtMxProcess *Matrix = dtc->m_Matrix;

	if (!dtc->is_configured || !dtc->is_sending) return GF_OK;

	dtc->is_in_flush = GF_TRUE;

	dtc->frameNum++;
	
	//wait for vsync, eg wait for buffer to be read by the card
	dtc->frame_transfered = GF_FALSE;
	while (!dtc->frame_transfered) {
		gf_sleep(0);
	}
	dtc->is_in_flush = GF_FALSE;
	return GF_OK;
}

static GF_Err Dektec_LockBackBuffer(GF_VideoOutput *dr, GF_VideoSurface *vi, Bool do_lock)
{
	DtContext *dtc = (DtContext*)dr->opaque;

	if (do_lock) {
		return GF_NOT_SUPPORTED;
	}
	return GF_OK;
}

static void Dektec_Shutdown(GF_VideoOutput *dr)
{
	GF_LOG(GF_LOG_DEBUG, GF_LOG_MMIO, ("[DekTecOut] Dektec_Shutdown\n"));
	DtContext *dtc = (DtContext*)dr->opaque;
	DtMxProcess *Matrix = dtc->m_Matrix;
	dtc->is_configured = GF_FALSE;

	DTAPI_RESULT res = Matrix->Stop();
	if ( res != DTAPI_OK)
		GF_LOG(GF_LOG_ERROR, GF_LOG_MMIO, ("[DekTecOut] Can't stop transmission: %s\n", DtapiResult2Str(res)));

	Matrix->Reset();
	dtc->dvc->Detach();
	dtc->is_sending = GF_FALSE;
}

static GF_Err Dektec_ProcessEvent(GF_VideoOutput *dr, GF_Event *evt)
{
	//DtFrameBuffer *dtf = (DtFrameBuffer*)(((DtContext*)dr->opaque)->dtf);
	DtContext *dtc = (DtContext*)dr->opaque;

	if (evt) {
		switch (evt->type) {
		case GF_EVENT_VIDEO_SETUP:
			GF_LOG(GF_LOG_DEBUG, GF_LOG_MMIO, ("[DekTecOut] resize (%ux%u) received.\n", evt->size.width, evt->size.height));
			dtc->needs_reconfigure = GF_TRUE;
			return GF_OK;
		}
	}
	return GF_OK;
}

GF_Err Dektec_Setup(GF_VideoOutput *dr, void *os_handle, void *os_display, u32 init_flags)
{
	const char *opt;
	DTAPI_RESULT res;
	DtContext *dtc = (DtContext*)dr->opaque;
	DtDevice *dvc = dtc->dvc;

	opt = gf_modules_get_option((GF_BaseInterface *)dr, "DektecVideo", "CardSlot");
	if (opt) {
		int PciBusNumber, SlotNumber;
		sscanf(opt, "%d:%d", &PciBusNumber, &SlotNumber);

		res = dvc->AttachToSlot(PciBusNumber, SlotNumber);
		if ( res != DTAPI_OK) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_MMIO, ("[DekTecOut] No DTA found on PIC bus %d slot %d: %s - trying enumerating cards\n", PciBusNumber, SlotNumber, DtapiResult2Str(res) ));
		} else {
			return GF_OK;
		}
	}
	dtc->clip_sdi = GF_TRUE;
	opt = gf_modules_get_option((GF_BaseInterface *)dr, "DektecVideo", "ClipSDI");
	if (!opt) 
		gf_modules_set_option((GF_BaseInterface *)dr, "DektecVideo", "ClipSDI", "yes");
	else if (!strcmp(opt, "no"))
		dtc->clip_sdi = GF_FALSE;

	res = dvc->AttachToType(2174);
	if ( res != DTAPI_OK) res = dvc->AttachToType(2154);

	if ( res != DTAPI_OK) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_MMIO, ("[DekTecOut] No DTA 2174 or 2154 in system: %s\n", DtapiResult2Str(res)));
		return GF_BAD_PARAM;
	}
	opt = gf_modules_get_option((GF_BaseInterface *)dr, "DektecVideo", "ForceRes");
	if (!opt) {
		gf_modules_set_option((GF_BaseInterface *)dr, "DektecVideo", "ForceRes", "no");
		dtc->force_width = dtc->force_height = 0;
	} else if (sscanf(opt, "%ux%u", &dtc->force_width, &dtc->force_height) == 2) {
		GF_LOG(GF_LOG_INFO, GF_LOG_MMIO, ("[DekTecOut] Using forced resolution %ux%u\n", dtc->force_width, dtc->force_height));
	} else {
		dtc->force_width = dtc->force_height = 0;
	}
	return GF_OK;
}

GF_Err Dektec_Configure(GF_VideoOutput *dr, u32 width, u32 height, Bool is_10_bits)
{
	DTAPI_RESULT res;
	int port = 1;
	const char *opt;
	Bool is4K = GF_FALSE;
	DtContext *dtc = (DtContext*)dr->opaque;
	DtDevice *dvc = dtc->dvc;

	const char* sopt = gf_modules_get_option((GF_BaseInterface *)dr, "Video", "FrameRate");
	if (!sopt) sopt = gf_modules_get_option((GF_BaseInterface *)dr, "Compositor", "FrameRate");
	Double frameRate = 30.0;
	int dFormat = -1, linkStd = -1;

	if (sopt) frameRate = atof(sopt);

	GF_LOG(GF_LOG_INFO, GF_LOG_MMIO, ("[DekTecOut] Reconfigure to %dx%d @ %g FPS %s bits src - SDI clipping: %s\n", width, height, frameRate, is_10_bits ? "10" : "8", dtc->clip_sdi ? "yes" : "no"));
	if (dtc->is_configured) {
		DtMxProcess *Matrix = dtc->m_Matrix;
		if (dtc->is_sending) {
			res = Matrix->Stop();
			if ( res != DTAPI_OK) {
				GF_LOG(GF_LOG_ERROR, GF_LOG_MMIO, ("[DekTecOut] Can't stop transmission: %s\n", DtapiResult2Str(res)));
			}
		}
		Matrix->Reset();
		dtc->is_configured = GF_FALSE;
		dtc->is_sending = GF_FALSE;
	}

	dtc->width = width;
	dtc->height = height;
	dtc->is_10b = is_10_bits;

	if (frameRate == 30.0) {
		dtc->frame_scale = 30;
		dtc->frame_dur = 1;
		if ((dtc->width==1280) && (dtc->height==720)) {
			dFormat = DTAPI_IOCONFIG_720P30;
		} else if ((dtc->width==1920) && (dtc->height==1080)) {
			dFormat = DTAPI_IOCONFIG_1080P30;
		} else if ((dtc->width==3840) && (dtc->height==2160)) {
			dFormat = DTAPI_VIDSTD_2160P30;
		}
	} else if (frameRate == 50.0) {
		dtc->frame_scale = 50;
		dtc->frame_dur = 1;
		if ((dtc->width==1280) && (dtc->height==720)) {
			dFormat = DTAPI_IOCONFIG_720P50;
		} else if ((dtc->width==1920) && (dtc->height==1080)) {
			dFormat = DTAPI_IOCONFIG_1080P50;
		} else if ((dtc->width==3840) && (dtc->height==2160)) {
			dFormat = DTAPI_VIDSTD_2160P50;
		}
	} else if (frameRate == 60.0) {
		dtc->frame_scale = 60;
		dtc->frame_dur = 1;
		if ((dtc->width==1280) && (dtc->height==720)) {
			dFormat = DTAPI_IOCONFIG_720P60;
		} else if ((dtc->width==1920) && (dtc->height==1080)) {
			dFormat = DTAPI_IOCONFIG_1080P60;
		} else if ((dtc->width==3840) && (dtc->height==2160)) {
			dFormat = DTAPI_VIDSTD_2160P60;
		}
	} else if (frameRate == 59.94) {
		dtc->frame_scale = 3000;
		dtc->frame_dur = 1001;

		if ((dtc->width==1280) && (dtc->height==720)) {
			dFormat = DTAPI_IOCONFIG_720P59_94;
		} else if ((dtc->width==1920) && (dtc->height==1080)) {
			dFormat = DTAPI_IOCONFIG_1080P59_94;
		} else if ((dtc->width==3840) && (dtc->height==2160)) {
			dFormat = DTAPI_VIDSTD_2160P59_94;
		}
	} else if (frameRate == 24.0) {
		dtc->frame_scale = 24;
		dtc->frame_dur = 1;
		if ((dtc->width==1280) && (dtc->height==720)) {
			dFormat = DTAPI_IOCONFIG_720P24;
		} else if ((dtc->width==1920) && (dtc->height==1080)) {
			dFormat = DTAPI_IOCONFIG_1080P24;
		} else if ((dtc->width==3840) && (dtc->height==2160)) {
			dFormat = DTAPI_VIDSTD_2160P24;
		}
	}

	if (dFormat==-1) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_MMIO, ("[DekTecOut] Unsupported frame rate %f / resolution %dx%d\n", frameRate, dtc->width, dtc->height));
		return GF_BAD_PARAM;
	}

	if ((dtc->width==3840) && (dtc->height==2160)) {
		is4K = GF_TRUE;
		linkStd = DTAPI_VIDLNK_4K_SMPTE425B;
	}

	if (frameRate == 60.0)
		GF_LOG(GF_LOG_ERROR, GF_LOG_MMIO, ("[DekTecOut] Dealing with 60 fps video is not stable, use with caution.\n"));

	opt = gf_modules_get_option((GF_BaseInterface *)dr, "DektecVideo", "SDIOutput");
	if (opt) {
		port = atoi(opt);
		GF_LOG(GF_LOG_INFO, GF_LOG_MMIO, ("[DekTecOut] Using port %d (%s)\n", port, opt));
	} else {
		GF_LOG(GF_LOG_INFO, GF_LOG_MMIO, ("[DekTecOut] No port specified, using default port: %d\n", port));
	}

	DtMxPort mxPort;
	if (!is4K)
	{
		res = dvc->SetIoConfig(port, DTAPI_IOCONFIG_IODIR, DTAPI_IOCONFIG_OUTPUT, DTAPI_IOCONFIG_OUTPUT);
		if ( res != DTAPI_OK) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_MMIO, ("[DekTecOut] Can't set I/O config for the device: %s\n", DtapiResult2Str(res)));
			return GF_BAD_PARAM;
		}

		int IoStdValue = -1, IoStdSubValue = -1;
		res = DtapiVidStd2IoStd(dFormat, IoStdValue, IoStdSubValue);
		if ( res != DTAPI_OK) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_MMIO, ("[DekTecOut] Unknown VidStd: %s\n", DtapiResult2Str(res)));
			return GF_BAD_PARAM;
		}

		res = dvc->SetIoConfig(port, DTAPI_IOCONFIG_IOSTD, IoStdValue, IoStdSubValue);
		if ( res != DTAPI_OK) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_MMIO, ("[DekTecOut] Can't set I/O config: %s\n", DtapiResult2Str(res)));
			return GF_BAD_PARAM;
		}
		res = mxPort.AddPhysicalPort(dvc, port);
		if (res != DTAPI_OK){
			GF_LOG(GF_LOG_ERROR, GF_LOG_MMIO, ("[DekTecOut] Failed to add port %d to logical port", port));
			return GF_BAD_PARAM;
		}
	}
	else
	{
		DtMxPort mxPort4k(dFormat, linkStd);
		for (int i = 0; i < 4; i++)
		{
			res = dvc->SetIoConfig(i+1, DTAPI_IOCONFIG_IODIR, DTAPI_IOCONFIG_OUTPUT, DTAPI_IOCONFIG_OUTPUT);
			if ( res != DTAPI_OK) {
				GF_LOG(GF_LOG_ERROR, GF_LOG_MMIO, ("[DekTecOut] Can't set I/O config for the device: %s\n", DtapiResult2Str(res)));
				return GF_BAD_PARAM;
			}
		}

		int IoStdValue = -1, IoStdSubValue = -1;
		res = DtapiVidStd2IoStd(dFormat, linkStd, IoStdValue, IoStdSubValue);
		if ( res != DTAPI_OK) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_MMIO, ("[DekTecOut] Unknown VidStd: %s\n", DtapiResult2Str(res)));
			return GF_BAD_PARAM;
		}

		for (int i = 0; i < 4; i++)
		{
			res = dvc->SetIoConfig(i+1, DTAPI_IOCONFIG_IOSTD, IoStdValue, IoStdSubValue);
			if ( res != DTAPI_OK) {
				GF_LOG(GF_LOG_ERROR, GF_LOG_MMIO, ("[DekTecOut] Can't set I/O config: %s\n", DtapiResult2Str(res)));
				return GF_BAD_PARAM;
			}
		}
		for (int i = 0; i < 4; i++)
		{
			res = mxPort4k.AddPhysicalPort(dvc, i + 1);
			if (res != DTAPI_OK){
				GF_LOG(GF_LOG_ERROR, GF_LOG_MMIO, ("[DekTecOut] Failed to add port %d to logical 4k port", (i + 1)));
				return GF_BAD_PARAM;
			}
		}
		
		DtIoConfig  Cfg;
		std::vector<DtIoConfig>  IoConfigs;
		DtHwFuncDesc*  pHwf = dvc->m_pHwf;
		for (int i = 0; i<dvc->m_pHwf->m_DvcDesc.m_NumPorts; i++, pHwf++)
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
		res = dvc->SetIoConfig(IoConfigs.data(), (int)IoConfigs.size());
		if (res != DTAPI_OK){
			GF_LOG(GF_LOG_ERROR, GF_LOG_MMIO, ("[DekTecOut] Failed to apply genref\n"));
			return GF_BAD_PARAM;
		}
		mxPort = mxPort4k;
	}

	res = dtc->m_Matrix->AttachRowToOutput(0, mxPort);
	if ( res != DTAPI_OK) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_MMIO, ("[DekTecOut] Can't attach to port %d: %s\n", port, DtapiResult2Str(res)));
		return GF_BAD_PARAM;
	}

	/* --- Set row configuration --- */
	DtMxRowConfig  RowConfig;
	RowConfig.m_Enable = true;
	RowConfig.m_RowSize = 1;       // Keep a history of one frame

	// Enable the video
	RowConfig.m_VideoEnable = true;
	RowConfig.m_Video.m_PixelFormat = DT_PXFMT_YUV422P_16B;  // Use a planar format (16-bit)
	RowConfig.m_Video.m_StartLine1 = RowConfig.m_Video.m_StartLine2 = 1;
	RowConfig.m_Video.m_NumLines1 = RowConfig.m_Video.m_NumLines2 = -1;
	RowConfig.m_Video.m_Scaling = DTAPI_SCALING_OFF;   // No scaling for this row
	RowConfig.m_Video.m_LineAlignment = 8; // Want an 8-byte alignment

	// Enable audio
	RowConfig.m_AudioEnable = false;
	// We will add four channels (2x stereo)
	for (int i = 0; i<4; i++)
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
	res = dtc->m_Matrix->SetRowConfig(0, RowConfig);
	if (res != DTAPI_OK){
		GF_LOG(GF_LOG_ERROR, GF_LOG_MMIO, ("[DekTecOut] Failed to set row 0 config"));
		return GF_BAD_PARAM;
	}

	/* --- Final Configuration --- */
	// Determine the default recommended end-to-end delay for this configuration

	int  DefEnd2EndDelay = 0;
	double CbFrames = 0.0;    // Time available for the call-back method
	res = dtc->m_Matrix->GetDefEndToEndDelay(DefEnd2EndDelay, CbFrames);
	GF_LOG(GF_LOG_INFO, GF_LOG_MMIO, ("[DekTecOut]  Default end-to-end delay = %d Frames\n", DefEnd2EndDelay));
	GF_LOG(GF_LOG_INFO, GF_LOG_MMIO, ("[DekTecOut]  Time-for-Callback = %.1f Frame Period\n", CbFrames));
	// Apply the default end-to-end delay
	res = dtc->m_Matrix->SetEndToEndDelay(DefEnd2EndDelay);
	if (res != DTAPI_OK){
		GF_LOG(GF_LOG_ERROR, GF_LOG_MMIO, ("[DekTecOut] Failed to set the end-to-end delay (delay=%d)", DefEnd2EndDelay));
		return GF_BAD_PARAM;
	}

	// Register our callbacks
	////m_ContextAudio.m_pPlayer = this;
	////m_ContextAudio.m_Type = CallbackContext::GEN_AUDIO;
	//res = dtc->m_Matrix->AddMatrixCbFunc(OnNewFrameAudio, dr);
	//if (res != DTAPI_OK){
	//	GF_LOG(GF_LOG_ERROR, GF_LOG_MMIO, ("[DekTecOut] Failed register audio callback function"));
	//	return GF_BAD_PARAM;
	//}

	//m_ContextVideo.m_pPlayer = this;
	//m_ContextVideo.m_Type = CallbackContext::GEN_VIDEO;
	res = dtc->m_Matrix->AddMatrixCbFunc(OnNewFrameVideo, dr);
	if (res != DTAPI_OK){
		GF_LOG(GF_LOG_ERROR, GF_LOG_MMIO, ("[DekTecOut] Failed register video callback function"));
		return GF_BAD_PARAM;
	}

	dtc->frameNum = -1;
	dtc->lastFrameNum = -1;
	dtc->is_configured = GF_TRUE;
	// Start transmission right now - this function is called during the blit(), and the object clock is initialized
	//after the blit, starting now will reduce the number of skipped frames at the dektec startup
	res = dtc->m_Matrix->Start();
	if ( res != DTAPI_OK) {
		GF_LOG(GF_LOG_WARNING, GF_LOG_MMIO, ("[DekTecOut] Can't start transmission: %s.\n", DtapiResult2Str(res)));
	} else {
		GF_LOG(GF_LOG_INFO, GF_LOG_MMIO, ("[DekTecOut] Transmission started.\n"));
		dtc->is_sending = GF_TRUE;
	}
	return GF_OK;
}

static GF_Err Dektec_Blit(GF_VideoOutput *dr, GF_VideoSurface *video_src, GF_Window *src_wnd, GF_Window *dst_wnd, u32 overlay_type)
{
	DtContext *dtc = (DtContext*)dr->opaque;
	u32 w = video_src->width;
	u32 h = video_src->height;

	if (dtc->needs_reconfigure) {
		if ((w==1280) && (h==720)) {
		} else if ((w==1920) && (h==1080)) {
		} else if ((w==3840) && (h==2160)) {
		} else {
			return GF_NOT_SUPPORTED;
		}
		if (dtc->force_width && dtc->force_height) {
			if ((w != dtc->force_width) || (h != dtc->force_height)) {
				return GF_NOT_SUPPORTED;
			}
		}
		dtc->needs_reconfigure = GF_FALSE;
		Dektec_Configure(dr, w, h, (video_src->pixel_format==GF_PIXEL_YV12_10) ? GF_TRUE : GF_FALSE);
	} else if ((w!=dtc->width) || (h != dtc->height)) {
		return GF_NOT_SUPPORTED;
	}

	dtc->pixels_YUV_source = (unsigned char*)video_src->video_buffer;
	return GF_OK;
}


GF_VideoOutput *NewDektecVideoOutput()
{
	GF_VideoOutput *driv = (GF_VideoOutput *) gf_malloc(sizeof(GF_VideoOutput));
	memset(driv, 0, sizeof(GF_VideoOutput));
	GF_REGISTER_MODULE_INTERFACE(driv, GF_VIDEO_OUTPUT_INTERFACE, "Dektec Video Output", "gpac distribution")

	DtDevice *dvc = new DtDevice;
	//DtFrameBuffer *dtf = new DtFrameBuffer;
	DtMxProcess *Matrix = new DtMxProcess;
	//if (!dtf || !dvc) {
	if (!Matrix || !dvc) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_MMIO, ("[DekTecOut] DTA API couldn't be initialized.\n"));
		delete dvc;
		//delete dtf;
		delete Matrix;
		gf_free(driv);
		return NULL;
	}

	DtContext *dtc = new DtContext;
	memset(dtc, 0, sizeof(DtContext));
	dtc->dvc = dvc;
	//dtc->dtf = dtf;
	dtc->m_Matrix = Matrix;
	driv->opaque = (void*)dtc;

	driv->Flush = Dektec_Flush;
	driv->LockBackBuffer = Dektec_LockBackBuffer;
	driv->Setup = Dektec_Setup;
	driv->Shutdown = Dektec_Shutdown;
	driv->ProcessEvent = Dektec_ProcessEvent;
	driv->Blit = Dektec_Blit;
	driv->hw_caps |= GF_VIDEO_HW_HAS_YUV_OVERLAY | GF_VIDEO_HW_HAS_YUV;
	driv->max_screen_bpp = 10;

	return driv;
}

void DeleteDektecVideoOutput(void *ifce)
{
	GF_VideoOutput *driv = (GF_VideoOutput *) ifce;
	DtContext *dtc = (DtContext*)driv->opaque;

	//delete(dtc->dtf);
	delete(dtc->m_Matrix);
	delete(dtc->dvc);
	delete(dtc);
	gf_free(driv);
}


#ifndef GPAC_STANDALONE_RENDER_2D

/*interface query*/
GPAC_MODULE_EXPORT
	const u32 *QueryInterfaces()
{
	static u32 si [] = {
		GF_VIDEO_OUTPUT_INTERFACE,
		0
	};
	return si;
}

/*interface create*/
GPAC_MODULE_EXPORT
	GF_BaseInterface *LoadInterface(u32 InterfaceType)
{
	if (InterfaceType == GF_VIDEO_OUTPUT_INTERFACE) return (GF_BaseInterface *) NewDektecVideoOutput();
	return NULL;
}

/*interface destroy*/
GPAC_MODULE_EXPORT
	void ShutdownInterface(GF_BaseInterface *ifce)
{
	switch (ifce->InterfaceType) {
	case GF_VIDEO_OUTPUT_INTERFACE:
		DeleteDektecVideoOutput((GF_VideoOutput *)ifce);
		break;
	}
}


GPAC_MODULE_STATIC_DECLARATION( dektec_out )

#endif

} /* extern "C" */

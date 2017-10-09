/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2010-2017
 *					All rights reserved
 *
 *  This file is part of GPAC / OpenSVC Decoder filter
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

#ifdef GPAC_HAS_OPENSVC

#include <gpac/avparse.h>
#include <gpac/constants.h>
#include <gpac/internal/media_dev.h>


#if (defined(WIN32) || defined(_WIN32_WCE)) && !defined(__GNUC__)
#  pragma comment(lib, "OpenSVCDecoder")
#endif

#include <OpenSVCDecoder/SVCDecoder_ietr_api.h>

#define SVC_MAX_STREAMS 3

typedef struct
{
	GF_FilterPid *ipid;
	u32 cfg_crc;
	u32 dep_id;
} GF_SVCStream;

typedef struct{
	u64 cts;
	u32 duration;
	u8 sap_type;
	u8 seek_flag;
} OSVCDecFrameInfo;

typedef struct
{
	GF_FilterPid *opid;
	GF_SVCStream streams[SVC_MAX_STREAMS];
	u32 nb_streams;
	u32 width, stride, height, out_size, pixel_ar;

	u32 nalu_size_length;
	Bool init_layer_set;
	Bool state_found;

	/*OpenSVC things*/
	void *codec;
	int CurrentDqId;
	int MaxDqId;
	int DqIdTable[8];
	int TemporalId;
	int TemporalCom;

	OSVCDecFrameInfo *frame_infos;
	u32 frame_infos_alloc, frame_infos_size;

} GF_OSVCDecCtx;

static GF_Err osvcdec_configure_pid(GF_Filter *filter, GF_FilterPid *pid, Bool is_remove)
{
	Bool found=GF_TRUE;
	const GF_PropertyValue *p;
	GF_M4VDecSpecInfo dsi;
	GF_Err e;
	u32 i, count, dep_id=0, cfg_crc=0;
	s32 res;
	OPENSVCFRAME Picture;
	int Layer[4];
	GF_OSVCDecCtx *ctx = (GF_OSVCDecCtx*) gf_filter_get_udta(filter);

	if (is_remove) {
		if (ctx->streams[0].ipid == pid) {
			memset(ctx->streams, 0, SVC_MAX_STREAMS*sizeof(GF_SVCStream));
			if (ctx->opid) gf_filter_pid_remove(ctx->opid);
			ctx->opid = NULL;
			ctx->nb_streams=0;
			if (ctx->codec) SVCDecoder_close(ctx->codec);
			ctx->codec = NULL;
			return GF_OK;
		} else {
			for (i=0; i<ctx->nb_streams; i++) {
				if (ctx->streams[i].ipid == pid) {
					ctx->streams[i].ipid = NULL;
					ctx->streams[i].cfg_crc = 0;
					memmove(&ctx->streams[i], &ctx->streams[i+1], sizeof(GF_SVCStream)*(ctx->nb_streams-1));
					ctx->nb_streams--;
					return GF_OK;
				}
			}
		}
		return GF_OK;
	}
	if (! gf_filter_pid_check_caps(pid))
		return GF_NOT_SUPPORTED;

	p = gf_filter_pid_get_property(pid, GF_PROP_PID_DEPENDENCY_ID);
	if (p) dep_id = p->value.uint;

	p = gf_filter_pid_get_property(pid, GF_PROP_PID_DECODER_CONFIG);
	if (p && p->value.data && p->data_len) {
		cfg_crc = gf_crc_32(p->value.data, p->data_len);
		for (i=0; i<ctx->nb_streams; i++) {
			if ((ctx->streams[i].ipid == pid) && (ctx->streams[i].cfg_crc == cfg_crc)) return GF_OK;
		}
	}

	found = GF_FALSE;
	for (i=0; i<ctx->nb_streams; i++) {
		if (!ctx->streams[i].ipid == pid) {
			ctx->streams[i].cfg_crc = cfg_crc;
			found = GF_TRUE;
		}
	}
	if (!found) {
		if (ctx->nb_streams==SVC_MAX_STREAMS) {
			return GF_NOT_SUPPORTED;
		}
		ctx->streams[ctx->nb_streams].ipid = pid;
		ctx->streams[ctx->nb_streams].cfg_crc = cfg_crc;
		ctx->streams[ctx->nb_streams].dep_id = dep_id;
		gf_filter_pid_set_framing_mode(pid, GF_TRUE);
		ctx->nb_streams++;
	}

	if (p && p->value.data) {
		GF_AVCConfig *cfg = gf_odf_avc_cfg_read(p->value.data, p->data_len);
		if (!cfg) return GF_NON_COMPLIANT_BITSTREAM;
		if (!dep_id) {
			ctx->nalu_size_length = cfg->nal_unit_size;
			if (SVCDecoder_init(&ctx->codec) == SVC_STATUS_ERROR) return GF_IO_ERR;
		}

		/*decode all NALUs*/
		count = gf_list_count(cfg->sequenceParameterSets);
		SetCommandLayer(Layer, 255, 0, &res, 0);//bufindex can be reset without pb
		for (i=0; i<count; i++) {
			u32 w=0, h=0, sid;
			s32 par_n=0, par_d=0;
			GF_AVCConfigSlot *slc = (GF_AVCConfigSlot*)gf_list_get(cfg->sequenceParameterSets, i);

#ifndef GPAC_DISABLE_AV_PARSERS
			gf_avc_get_sps_info(slc->data, slc->size, &sid, &w, &h, &par_n, &par_d);
#endif
			/*by default use the base layer*/
			if (!i) {
				if ((ctx->width<w) || (ctx->height<h)) {
					ctx->width = w;
					ctx->height = h;
					if ( ((s32)par_n>0) && ((s32)par_d>0) )
						ctx->pixel_ar = (par_n<<16) || par_d;
				}
			}
			res = decodeNAL(ctx->codec, (unsigned char *) slc->data, slc->size, &Picture, Layer);
			if (res<0) {
				GF_LOG(GF_LOG_ERROR, GF_LOG_CODEC, ("[SVC Decoder] Error decoding SPS %d\n", res));
			}
			GF_LOG(GF_LOG_DEBUG, GF_LOG_CODEC, ("[SVC Decoder] Attach: SPS id=\"%d\" code=\"%d\" size=\"%d\"\n", slc->id, slc->data[0] & 0x1F, slc->size));
		}

		count = gf_list_count(cfg->pictureParameterSets);
		for (i=0; i<count; i++) {
			u32 sps_id, pps_id;
			GF_AVCConfigSlot *slc = (GF_AVCConfigSlot*)gf_list_get(cfg->pictureParameterSets, i);
			gf_avc_get_pps_info(slc->data, slc->size, &pps_id, &sps_id);
			res = decodeNAL(ctx->codec, (unsigned char *) slc->data, slc->size, &Picture, Layer);
			if (res<0) {
				GF_LOG(GF_LOG_ERROR, GF_LOG_CODEC, ("[SVC Decoder] Error decoding PPS %d\n", res));
			}
			GF_LOG(GF_LOG_DEBUG, GF_LOG_CODEC, ("[SVC Decoder] Attach: PPS id=\"%d\" code=\"%d\" size=\"%d\" sps_id=\"%d\"\n", pps_id, slc->data[0] & 0x1F, slc->size, sps_id));
		}
		ctx->state_found = GF_TRUE;
		gf_odf_avc_cfg_del(cfg);
	} else {
		if (ctx->nalu_size_length) {
			return GF_NOT_SUPPORTED;
		}
		ctx->nalu_size_length = 0;
		if (!dep_id) {
			if (SVCDecoder_init(&ctx->codec) == SVC_STATUS_ERROR) return GF_IO_ERR;
		}
		ctx->pixel_ar = (1<<16) || 1;
	}
	ctx->stride = ctx->width + 32;
	ctx->CurrentDqId = ctx->MaxDqId = 0;
	ctx->out_size = ctx->stride * ctx->height * 3 / 2;

	if (!ctx->opid) {
		ctx->opid = gf_filter_pid_new(filter);

		gf_filter_pid_copy_properties(ctx->opid, ctx->streams[0].ipid);
		gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_OTI, &PROP_UINT(GPAC_OTI_RAW_MEDIA_STREAM) );
	}
	if (ctx->width) {
		gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_WIDTH, &PROP_UINT(ctx->width) );
		gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_HEIGHT, &PROP_UINT(ctx->height) );
		gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_STRIDE, &PROP_UINT(ctx->stride) );
		if (ctx->pixel_ar)
			gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_PAR, &PROP_UINT(ctx->pixel_ar) );
		gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_PIXFMT, &PROP_UINT(GF_PIXEL_YV12) );
	}
	return GF_OK;
}


static Bool osvcdec_process_event(GF_Filter *filter, GF_FilterEvent *fevt)
{
	GF_OSVCDecCtx *ctx = (GF_OSVCDecCtx*) gf_filter_get_udta(filter);

	if (fevt->base.type == GF_FEVT_QUALITY_SWITCH) {
		if (fevt->quality_switch.up) {
			if (ctx->CurrentDqId < ctx->MaxDqId)
				// set layer up (command=1)
				UpdateLayer( ctx->DqIdTable, &ctx->CurrentDqId, &ctx->TemporalCom, &ctx->TemporalId, ctx->MaxDqId, 1 );
		} else {
			if (ctx->CurrentDqId > 0)
				// set layer down (command=0)
				UpdateLayer( ctx->DqIdTable, &ctx->CurrentDqId, &ctx->TemporalCom, &ctx->TemporalId, ctx->MaxDqId, 0 );
		}
		//todo: we should get the set of pids active and trigger the switch up/down based on that
		//rather than not canceling the event
		return GF_FALSE;
	}
	return GF_FALSE;
}

static void osvcdec_drop_frameinfo(GF_OSVCDecCtx *ctx)
{
	if (ctx->frame_infos_size) {
		ctx->frame_infos_size--;
		memmove(&ctx->frame_infos[0], &ctx->frame_infos[1], sizeof(OSVCDecFrameInfo)*ctx->frame_infos_size);
	}
}

static GF_Err osvcdec_process(GF_Filter *filter)
{
	s32 got_pic;
	u64 min_dts = GF_FILTER_NO_TS;
	u64 min_cts = GF_FILTER_NO_TS;
	OPENSVCFRAME pic;
	int Layer[4];
	u32 i, idx, nalu_size, sc_size, nb_eos=0;
	u8 *ptr;
	u32 data_size;
	char *data;
	Bool has_pic = GF_FALSE;
	GF_OSVCDecCtx *ctx = (GF_OSVCDecCtx*) gf_filter_get_udta(filter);
	GF_FilterPacket *dst_pck;
	GF_FilterPacket *pck_ref=NULL;
	u32 curMaxDqId = ctx->MaxDqId;

	for (idx=0; idx<ctx->nb_streams; idx++) {
		u64 dts, cts;
		GF_FilterPacket *pck = gf_filter_pid_get_packet(ctx->streams[idx].ipid);
		if (!pck) {
			if (gf_filter_pid_is_eos(ctx->streams[idx].ipid)) nb_eos++;
			continue;
		}
		dts = gf_filter_pck_get_dts(pck);
		cts = gf_filter_pck_get_cts(pck);
		if (min_dts > dts) {
			min_dts = dts;
			pck_ref = pck;
		}
		if (min_cts > cts) {
			min_cts = cts;
			if (!pck_ref) pck_ref = pck;
		}
	}
	if (nb_eos == ctx->nb_streams) {
		gf_filter_pid_set_eos(ctx->opid);
		return GF_OK;
	}
	if (!pck_ref) return GF_OK;

	if (ctx->frame_infos_size==ctx->frame_infos_alloc) {
		ctx->frame_infos_alloc += 10;
		ctx->frame_infos = gf_realloc(ctx->frame_infos, sizeof(OSVCDecFrameInfo)*ctx->frame_infos_alloc);
	}
	min_cts = gf_filter_pck_get_cts(pck_ref);
	for (i=0; i<ctx->frame_infos_size; i++) {
		if (ctx->frame_infos[i].cts > min_cts) {
			memmove(&ctx->frame_infos[i+1], &ctx->frame_infos[i], sizeof(OSVCDecFrameInfo) * (ctx->frame_infos_size-i));
			ctx->frame_infos[i].cts = min_cts;
			ctx->frame_infos[i].duration = gf_filter_pck_get_duration(pck_ref);
			ctx->frame_infos[i].sap_type = gf_filter_pck_get_sap(pck_ref);
			ctx->frame_infos[i].seek_flag = gf_filter_pck_get_seek_flag(pck_ref);
			break;
		}
	}
	if (i==ctx->frame_infos_size) {
		ctx->frame_infos[i].cts = min_cts;
		ctx->frame_infos[i].duration = gf_filter_pck_get_duration(pck_ref);
		ctx->frame_infos[i].sap_type = gf_filter_pck_get_sap(pck_ref);
		ctx->frame_infos[i].seek_flag = gf_filter_pck_get_seek_flag(pck_ref);
	}
	ctx->frame_infos_size++;


	for (idx=0; idx<ctx->nb_streams; idx++) {
		u64 dts, cts;
		u32 sps_id, pps_id;

		GF_FilterPacket *pck = gf_filter_pid_get_packet(ctx->streams[idx].ipid);
		if (!pck) continue;

		dts = gf_filter_pck_get_dts(pck);
		cts = gf_filter_pck_get_cts(pck);
		if (min_dts != GF_FILTER_NO_TS) {
			if (min_dts != dts) continue;
		} else if (min_cts != cts) {
			continue;
		}
		data = gf_filter_pck_get_data(pck, &data_size);

		ctx->MaxDqId = GetDqIdMax((unsigned char *) data, data_size, ctx->nalu_size_length, ctx->DqIdTable, ctx->nalu_size_length ? 1 : 0);
		if (!ctx->init_layer_set) {
			//AVC stream in a h264 file
			if (ctx->MaxDqId == -1)
				ctx->MaxDqId = 0;

			ctx->CurrentDqId = ctx->MaxDqId;
			ctx->init_layer_set = GF_TRUE;
		}
		if (curMaxDqId != ctx->MaxDqId)
			ctx->CurrentDqId = ctx->MaxDqId;

		/*decode only current layer*/
		SetCommandLayer(Layer, ctx->MaxDqId, ctx->CurrentDqId, &ctx->TemporalCom, ctx->TemporalId);

		got_pic = 0;
		nalu_size = 0;
		ptr = (u8 *) data;
		sc_size = 0;

		if (!ctx->nalu_size_length) {
			u32 size = gf_media_nalu_next_start_code((u8 *) data, data_size, &sc_size);
			if (sc_size) {
				ptr += size+sc_size;
				assert(data_size >= size+sc_size);
				data_size -= size+sc_size;
			} else {
				/*no annex-B start-code found, discard */
				gf_filter_pid_drop_packet(ctx->streams[i].ipid);
				return GF_OK;
			}
		}

		while (data_size) {
			if (ctx->nalu_size_length) {
				for (i=0; i<ctx->nalu_size_length; i++) {
					nalu_size = (nalu_size<<8) + ptr[i];
				}
				ptr += ctx->nalu_size_length;
			} else {
				nalu_size = gf_media_nalu_next_start_code(ptr, data_size, &sc_size);
			}
#ifndef GPAC_DISABLE_LOG
			if (gf_log_tool_level_on(GF_LOG_CODEC, GF_LOG_DEBUG)) {
				switch (ptr[0] & 0x1F) {
				case GF_AVC_NALU_SEQ_PARAM:
				case GF_AVC_NALU_SVC_SUBSEQ_PARAM:
					gf_avc_get_sps_info((char *)ptr, nalu_size, &sps_id, NULL, NULL, NULL, NULL);
					GF_LOG(GF_LOG_DEBUG, GF_LOG_CODEC, ("[SVCDec] PID %s: SPS id=\"%d\" code=\"%d\" size=\"%d\"\n", gf_filter_pid_get_name(ctx->streams[idx].ipid), sps_id, ptr[0] & 0x1F, nalu_size));
					break;
				case GF_AVC_NALU_PIC_PARAM:
					gf_avc_get_pps_info((char *)ptr, nalu_size, &pps_id, &sps_id);
					GF_LOG(GF_LOG_DEBUG, GF_LOG_CODEC, ("[SVC Decoder] PID %s: PPS id=\"%d\" code=\"%d\" size=\"%d\" sps_id=\"%d\"\n", gf_filter_pid_get_name(ctx->streams[idx].ipid), pps_id, ptr[0] & 0x1F, nalu_size, sps_id));
					break;
				default:
					GF_LOG(GF_LOG_DEBUG, GF_LOG_CODEC, ("[SVC Decoder] PID %s: NALU code=\"%d\" size=\"%d\"\n", gf_filter_pid_get_name(ctx->streams[idx].ipid), ptr[0] & 0x1F, nalu_size));
				}
			}
#endif
			if (!ctx->state_found) {
				u8 nal_type = (ptr[0] & 0x1F) ;
				switch (nal_type) {
				case GF_AVC_NALU_SEQ_PARAM:
				case GF_AVC_NALU_PIC_PARAM:
					if (! ctx->streams[idx].dep_id)
						ctx->state_found = GF_TRUE;
					break;
				}
			}

			if (ctx->state_found) {
				if (!got_pic)
					got_pic = decodeNAL(ctx->codec, ptr, nalu_size, &pic, Layer);
				else
					decodeNAL(ctx->codec, ptr, nalu_size, &pic, Layer);
			}

			ptr += nalu_size;
			if (ctx->nalu_size_length) {
				if (data_size < nalu_size + ctx->nalu_size_length) break;
				data_size -= nalu_size + ctx->nalu_size_length;
			} else {
				if (!sc_size || (data_size < nalu_size + sc_size)) break;
				data_size -= nalu_size + sc_size;
				ptr += sc_size;
			}
		}
		gf_filter_pid_drop_packet(ctx->streams[idx].ipid);

		if (got_pic) has_pic = GF_TRUE;
	}
	if (!has_pic) return GF_OK;

	if (/*(curMaxDqId != ctx->MaxDqId) || */ (pic.Width != ctx->width) || (pic.Height!=ctx->height)) {
		GF_LOG(GF_LOG_INFO, GF_LOG_CODEC, ("[SVC Decoder] Resizing from %dx%d to %dx%d\n", ctx->width, ctx->height, pic.Width, pic.Height ));
		ctx->width = pic.Width;
		ctx->stride = pic.Width + 32;
		ctx->height = pic.Height;
		ctx->out_size = ctx->stride * ctx->height * 3 / 2;

		gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_WIDTH, &PROP_UINT(ctx->width) );
		gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_HEIGHT, &PROP_UINT(ctx->height) );
		gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_STRIDE, &PROP_UINT(ctx->stride) );
		if (ctx->pixel_ar)
			gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_PAR, &PROP_UINT(ctx->pixel_ar) );
	}

	if (ctx->frame_infos[0].seek_flag) {
		osvcdec_drop_frameinfo(ctx);
		return GF_OK;
	}

	dst_pck = gf_filter_pck_new_alloc(ctx->opid, ctx->out_size, &data);
	memcpy(data, pic.pY[0], ctx->stride*ctx->height);
	memcpy(data + ctx->stride * ctx->height, pic.pU[0], ctx->stride*ctx->height/4);
	memcpy(data + 5*ctx->stride * ctx->height/4, pic.pV[0], ctx->stride*ctx->height/4);

	gf_filter_pck_set_cts(dst_pck, ctx->frame_infos[0].cts);
	gf_filter_pck_set_sap(dst_pck, ctx->frame_infos[0].sap_type);
	gf_filter_pck_set_duration(dst_pck, ctx->frame_infos[0].duration);

	gf_filter_pck_send(dst_pck);

	osvcdec_drop_frameinfo(ctx);
	return GF_OK;
}

static void osvcdec_finalize(GF_Filter *filter)
{
	GF_OSVCDecCtx *ctx = (GF_OSVCDecCtx*) gf_filter_get_udta(filter);
	if (ctx->codec) SVCDecoder_close(ctx->codec);
	if (ctx->frame_infos) gf_free(ctx->frame_infos);
}

static const GF_FilterCapability OSVCDecInputs[] =
{
	CAP_INC_UINT(GF_PROP_PID_STREAM_TYPE, GF_STREAM_VISUAL),
	CAP_EXC_BOOL(GF_PROP_PID_UNFRAMED, GF_TRUE),
	CAP_INC_UINT(GF_PROP_PID_OTI, GPAC_OTI_VIDEO_AVC),
	CAP_INC_UINT(GF_PROP_PID_OTI, GPAC_OTI_VIDEO_SVC),
};

static const GF_FilterCapability OSVCDecOutputs[] =
{
	CAP_INC_UINT(GF_PROP_PID_STREAM_TYPE, GF_STREAM_VISUAL),
	CAP_INC_UINT(GF_PROP_PID_OTI, GPAC_OTI_RAW_MEDIA_STREAM),
};

GF_FilterRegister OSVCDecRegister = {
	.name = "osvcdec",
	.description = "OpenSVC decoder",
	.private_size = sizeof(GF_OSVCDecCtx),
	INCAPS(OSVCDecInputs),
	OUTCAPS(OSVCDecOutputs),
	.finalize = osvcdec_finalize,
	.configure_pid = osvcdec_configure_pid,
	.process = osvcdec_process,
	.process_event = osvcdec_process_event,
	.max_extra_pids = (SVC_MAX_STREAMS-1)
};

#endif //GPAC_HAS_OPENSVC

const GF_FilterRegister *osvcdec_register(GF_FilterSession *session)
{
#ifdef GPAC_HAS_OPENSVC
	return &OSVCDecRegister;
#else
	return NULL;
#endif
}

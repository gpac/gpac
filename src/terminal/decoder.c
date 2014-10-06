/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2000-2012
 *					All rights reserved
 *
 *  This file is part of GPAC / Media terminal sub-project
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



#include <gpac/internal/terminal_dev.h>
#include <gpac/internal/compositor_dev.h>
#include <gpac/constants.h>
#include <gpac/crypt.h>
#include "media_memory.h"
#include "media_control.h"
#include "input_sensor.h"

GF_Err Codec_Load(GF_Codec *codec, GF_ESD *esd, u32 PL);
GF_Err gf_codec_process_raw_media_pull(GF_Codec *codec, u32 TimeAvailable);

GF_Codec *gf_codec_new(GF_ObjectManager *odm, GF_ESD *base_layer, s32 PL, GF_Err *e)
{
	GF_Codec *tmp;

	//this is an addon, we must check if it's scalable stream or not ...
	//if so, do not create any new codec
	if (odm->parentscene && odm->parentscene->root_od->addon) {
		switch (base_layer->decoderConfig->objectTypeIndication) {
		case GPAC_OTI_VIDEO_SHVC:
		case GPAC_OTI_VIDEO_SVC:
			odm->scalable_addon = 1;
			odm->parentscene->root_od->addon->addon_type = GF_ADDON_TYPE_SCALABLE;
			*e = GF_OK;
			//fixme - we need a way to signal dependencies accross services!!
			base_layer->dependsOnESID = 0xFFFF;
			return NULL;
		default:
			break;
		}
	}

	GF_SAFEALLOC(tmp, GF_Codec);
	if (! tmp) {
		*e = GF_OUT_OF_MEM;
		return NULL;
	}
	tmp->odm = odm;

	if (PL<0) PL = 0xFF;
	*e = Codec_Load(tmp, base_layer, PL);

	if (*e) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_CODEC, ("[Codec] ODM%d ES%d: Cannot find decoder for stream type %s\n", odm->OD->objectDescriptorID, base_layer->ESID, gf_esd_get_textual_description(base_layer) ));
		gf_free(tmp);
		return NULL;
	}
	/*remember codec type*/
	tmp->type = base_layer->decoderConfig->streamType;
	tmp->oti = base_layer->decoderConfig->objectTypeIndication;
	tmp->inChannels = gf_list_new();
	tmp->Status = GF_ESM_CODEC_STOP;

	if (tmp->type==GF_STREAM_PRIVATE_MEDIA) tmp->type = GF_STREAM_VISUAL;

	if (tmp->type==GF_STREAM_VISUAL) {
		GF_CodecCapability cap;
		cap.CapCode = GF_CODEC_DISPLAY_BPP;
		cap.cap.valueInt = odm->term->compositor->video_out->max_screen_bpp;
		gf_codec_set_capability(tmp, cap);
	}
	tmp->Priority = base_layer->streamPriority ? base_layer->streamPriority : 1;

	GF_LOG(GF_LOG_INFO, GF_LOG_CODEC, ("[Codec] Found decoder %s for stream type %s\n", tmp->decio ? tmp->decio->module_name : "RAW", gf_esd_get_textual_description(base_layer) ));
	return tmp;
}

GF_Codec *gf_codec_use_codec(GF_Codec *codec, GF_ObjectManager *odm)
{
	GF_Codec *tmp;
	if (!codec->decio) return NULL;
	GF_SAFEALLOC(tmp, GF_Codec);
	tmp->type = codec->type;
	tmp->inChannels = gf_list_new();
	tmp->Status = GF_ESM_CODEC_STOP;
	tmp->odm = odm;
	tmp->flags = codec->flags | GF_ESM_CODEC_IS_USE;
	tmp->decio = codec->decio;
	tmp->process = codec->process;
	return tmp;
}

GF_Err gf_codec_add_channel(GF_Codec *codec, GF_Channel *ch)
{
	GF_Err e;
	GF_NetworkCommand com;
	GF_Channel *a_ch;
	u32 CUsize, i;
	GF_CodecCapability cap;
	u32 min, max;



	/*only for valid codecs (eg not OCR)*/
	if (codec->decio) {
		com.get_dsi.dsi = NULL;
		if (ch->esd->decoderConfig->upstream) codec->flags |= GF_ESM_CODEC_HAS_UPSTREAM;
		/*For objects declared in OD stream, override with network DSI if any*/
		if (ch->service && !(ch->odm->flags & GF_ODM_NOT_IN_OD_STREAM) ) {
			com.command_type = GF_NET_CHAN_GET_DSI;
			com.base.on_channel = ch;
			e = gf_term_service_command(ch->service, &com);
			if (!e && com.get_dsi.dsi) {
				if (ch->esd->decoderConfig->decoderSpecificInfo->data) gf_free(ch->esd->decoderConfig->decoderSpecificInfo->data);
				ch->esd->decoderConfig->decoderSpecificInfo->data = com.get_dsi.dsi;
				ch->esd->decoderConfig->decoderSpecificInfo->dataLength = com.get_dsi.dsi_len;
			}
		}
		GF_LOG(GF_LOG_DEBUG, GF_LOG_CODEC, ("[Codec] Attaching stream %d to codec %s\n", ch->esd->ESID, codec->decio->module_name));

		if (codec->odm->term->bench_mode==2) {
			e = GF_OK;
		} else {
			/*lock the channel before setup in case we are using direct_decode */
			gf_mx_p(ch->mx);
			e = codec->decio->AttachStream(codec->decio, ch->esd);
			gf_mx_v(ch->mx);
		}

		if (ch->esd->decoderConfig && ch->esd->decoderConfig->rvc_config) {
			gf_odf_desc_del((GF_Descriptor *)ch->esd->decoderConfig->rvc_config);
			ch->esd->decoderConfig->rvc_config = NULL;
		}

		if (e) {
			GF_LOG(GF_LOG_DEBUG, GF_LOG_CODEC, ("[Codec] Attach Stream failed %s\n", gf_error_to_string(e) ));
			return e;
		}

		/*ask codec for desired output capacity - note this may be 0 if stream is not yet configured*/
		cap.CapCode = GF_CODEC_OUTPUT_SIZE;
		gf_codec_get_capability(codec, &cap);
		if (codec->CB && (cap.cap.valueInt != codec->CB->UnitSize)) {
			gf_cm_del(codec->CB);
			codec->CB = NULL;
		}
		CUsize = cap.cap.valueInt;

		/*get desired amount of units and minimal fullness (used for scheduling)*/
		switch(codec->type) {
		case GF_STREAM_VISUAL:
			//this works but we need at least double buffering of textures on the GPU which we don't have now
			if ( gf_sc_use_raw_texture(codec->odm->term->compositor)) {
				cap.CapCode = GF_CODEC_DIRECT_OUTPUT;
				gf_codec_get_capability(codec, &cap);
				if (cap.cap.valueBool) {
					cap.CapCode = GF_CODEC_DIRECT_OUTPUT;
					if ((gf_codec_set_capability(codec, cap)==GF_OK) && (((GF_MediaDecoder*)codec->decio)->GetOutputBuffer != NULL))
						codec->direct_vout = GF_TRUE;
				}
			}

		case GF_STREAM_AUDIO:
			cap.CapCode = GF_CODEC_BUFFER_MIN;
			cap.cap.valueInt = 1;
			gf_codec_get_capability(codec, &cap);
			min = cap.cap.valueInt;
			cap.CapCode = GF_CODEC_BUFFER_MAX;
			cap.cap.valueInt = 1;
			gf_codec_get_capability(codec, &cap);
			max = cap.cap.valueInt;
			//this may happen in sysetm bench mode since we dont configure codecs
			if (!max) max = 1;
			break;
		case GF_STREAM_ND_SUBPIC:
			max = 1;
			min = 0;
			break;
		default:
			min = max = 0;
			break;
		}
		if ((codec->type==GF_STREAM_AUDIO) && (max<2)) max = 2;

		/*setup CB*/
		if (!codec->CB && max) {
			Bool no_alloc = GF_FALSE;
			if (codec->flags & GF_ESM_CODEC_IS_RAW_MEDIA) {
				max = 1;
				/*create a semaphore in non-notified stage*/
				codec->odm->raw_frame_sema = gf_sema_new(1, 0);
				no_alloc = 1;
			}
			else if (codec->direct_vout) {
				max = 1;
				no_alloc = 1;
			}
			//very low latency
			else if (ch->MaxBuffer<=300) {
//				max /= MAX(max/2, 2);
			}

			GF_LOG(GF_LOG_DEBUG, GF_LOG_CODEC, ("[ODM] Creating composition buffer for codec %s - %d units %d bytes each\n", codec->decio->module_name, max, CUsize));

			codec->CB = gf_cm_new(CUsize, max, no_alloc);
			codec->CB->Min = min;
			codec->CB->odm = codec->odm;
		}

		if (codec->CB) {
			/*check re-ordering - set by default on all codecs*/
			codec->is_reordering = 1;
			cap.CapCode = GF_CODEC_REORDER;
			if (gf_codec_get_capability(codec, &cap) == GF_OK)
				codec->is_reordering = cap.cap.valueInt;

			codec->trusted_cts = 0;
			cap.CapCode = GF_CODEC_TRUSTED_CTS;
			if (gf_codec_get_capability(codec, &cap) == GF_OK)
				codec->trusted_cts = cap.cap.valueInt;

		}

		if (codec->flags & GF_ESM_CODEC_IS_RAW_MEDIA) {
			ch->is_raw_channel = 1;
		}

		/*setup net channel config*/
		if (ch->service) {
			memset(&com, 0, sizeof(GF_NetworkCommand));
			com.command_type = GF_NET_CHAN_CONFIG;
			com.base.on_channel = ch;

			com.cfg.priority = ch->esd->streamPriority;
			assert( ch->clock );
			com.cfg.sync_id = ch->clock->clockID;
			memcpy(&com.cfg.sl_config, ch->esd->slConfig, sizeof(GF_SLConfig));
			/*get the frame duration if audio (used by some network stack)*/
			if (ch->odm->codec && (ch->odm->codec->type==GF_STREAM_AUDIO) ) {
				cap.CapCode = GF_CODEC_SAMPLERATE;
				gf_codec_get_capability(ch->odm->codec, &cap);
				com.cfg.sample_rate = cap.cap.valueInt;
				cap.CapCode = GF_CODEC_CU_DURATION;
				gf_codec_get_capability(ch->odm->codec, &cap);
				com.cfg.frame_duration = cap.cap.valueInt;
			}
			gf_term_service_command(ch->service, &com);

			ch->carousel_type = GF_ESM_CAROUSEL_NONE;
			if (com.cfg.use_m2ts_sections) {
				ch->carousel_type = GF_ESM_CAROUSEL_MPEG2;
			} else {
				switch (ch->esd->decoderConfig->streamType) {
				case GF_STREAM_OD:
				case GF_STREAM_SCENE:
					ch->carousel_type = ch->esd->slConfig->AUSeqNumLength ? GF_ESM_CAROUSEL_MPEG4 : GF_ESM_CAROUSEL_NONE;
					break;
				}
			}

		}
	} else if (codec->flags & GF_ESM_CODEC_IS_RAW_MEDIA) {
		cap.CapCode = GF_CODEC_OUTPUT_SIZE;
		gf_codec_get_capability(codec, &cap);
		if (codec->CB && (cap.cap.valueInt != codec->CB->UnitSize)) {
			gf_cm_del(codec->CB);
			codec->CB = NULL;
		}
		CUsize = cap.cap.valueInt;
		/*create a semaphore in non-notified stage*/
		codec->odm->raw_frame_sema = gf_sema_new(1, 0);

		codec->CB = gf_cm_new(CUsize, 1, 1);
		codec->CB->Min = 0;
		codec->CB->odm = codec->odm;
		ch->is_raw_channel = 1;
		if (gf_es_owns_clock(ch))
			ch->is_raw_channel = 2;

		if (ch->is_pulling) {
			codec->process = gf_codec_process_raw_media_pull;
		}
	}

	/*assign the first base layer as the codec clock by default, or current channel clock if no clock set
	Also assign codec priority here*/
	if (!ch->esd->dependsOnESID || !codec->ck) {
		codec->ck = ch->clock;
		/*insert base layer first - note we are sure this is a stream of the same type
		as the codec (other streams - OCI, MPEG7, MPEGJ - are not added that way)*/
		return gf_list_insert(codec->inChannels, ch, 0);
	}
	else {
		/*make sure all channels are in order*/
		i=0;
		while ((a_ch = (GF_Channel*)gf_list_enum(codec->inChannels, &i))) {
			if (ch->esd->dependsOnESID == a_ch->esd->ESID) {
				return gf_list_insert(codec->inChannels, ch, i);
			}
			if (a_ch->esd->dependsOnESID == ch->esd->ESID) {
				return gf_list_insert(codec->inChannels, ch, i-1);
			}
		}
		/*by default append*/
		return gf_list_add(codec->inChannels, ch);
	}
}

Bool gf_codec_is_scene_or_image(GF_Codec *codec)
{
	if (!codec) return GF_TRUE;
	if (!codec->CB) return GF_TRUE;
	if (codec->CB->Capacity>1 || codec->CB->no_allocation) return GF_FALSE;
	return GF_TRUE;
}

Bool gf_codec_remove_channel(GF_Codec *codec, struct _es_channel *ch)
{
	s32 i;
	assert( codec );
	assert( codec->inChannels);
	assert(ch);
	i = gf_list_find(codec->inChannels, ch);
	if (i>=0) {
		if (codec->decio) {
			if (codec->odm->term->bench_mode!=2) {
				codec->decio->DetachStream(codec->decio, ch->esd->ESID);
			}
		}
		gf_list_rem(codec->inChannels, (u32) i);
		return 1;
	}
	return 0;
}


static void codec_update_stats(GF_Codec *codec, u32 dataLength, u64 dec_time, u32 DTS, Bool is_rap)
{
	codec->total_dec_time += dec_time;
	codec->last_frame_time = gf_sys_clock();
	if (!codec->nb_dec_frames) {
		codec->first_frame_time = codec->last_frame_time;
		codec->min_frame_dur = (u32) -1;
	}

	codec->nb_dec_frames++;
	if (is_rap) {
		codec->nb_iframes ++;
		if (dec_time>codec->max_iframes_time) codec->max_iframes_time = (u32) dec_time;
		codec->total_iframes_time += dec_time;
	}

	if (dec_time>codec->max_dec_time) codec->max_dec_time = (u32) dec_time;


	if (DTS < codec->min_frame_dur + codec->last_unit_dts ) {
		//might happen with some AVI with ffmpeg ...
		if (DTS > codec->last_unit_dts)
			codec->min_frame_dur = DTS - codec->last_unit_dts;
	}

	if (dataLength) {
		if (!codec->cur_bit_size || (codec->stat_start > DTS)) {
			codec->stat_start = DTS;
			codec->cur_bit_size = 8*dataLength;
		} else {
			if (codec->last_stat_start + 2000 <= DTS) {
				codec->avg_bit_rate = (u32) (codec->cur_bit_size * (1000.0 / (DTS-codec->last_stat_start) ) );
				if (codec->avg_bit_rate > codec->max_bit_rate) codec->max_bit_rate = codec->avg_bit_rate;
				codec->last_stat_start = DTS;
				codec->cur_bit_size = 0;
			}
			codec->cur_bit_size += 8*dataLength;
		}
	}
}

static void MediaDecoder_GetNextAU(GF_Codec *codec, GF_Channel **activeChannel, GF_DBUnit **nextAU)
{
	GF_Channel *ch;
	GF_DBUnit *AU;
	GF_List *src_channels = codec->inChannels;
	GF_ObjectManager *current_odm = codec->odm;
	u32 count, curCTS, i, stream_state;
	Bool scalable_check = 0;
	s32 cts_diff;

	*nextAU = NULL;
	*activeChannel = NULL;
	curCTS = 0;

browse_scalable:

	count = gf_list_count(src_channels);
	if (!count) return;

	/*browse from base to top layer*/
	for (i=0; i<count; i++) {
		ch = (GF_Channel*)gf_list_get(src_channels, i);

		if ((codec->type==GF_STREAM_OCR) && ch->IsClockInit) {
			/*check duration - we assume that scalable OCR streams are just pure nonsense...*/
			if (ch->is_pulling && codec->odm->duration) {
				if (gf_clock_time(codec->ck) > codec->odm->duration)
					gf_es_on_eos(ch);
			}
			return;
		}
refetch_AU:
		stream_state = ch->stream_state;
		AU = gf_es_get_au(ch);
		if (!AU) {
			if (scalable_check==1) {
				if (*nextAU && ((*nextAU)->flags & GF_DB_AU_REAGGREGATED)) {
					scalable_check=2;
				} else {
					GF_LOG(GF_LOG_INFO, GF_LOG_CODEC, ("Warning, No AU in enhancement layer for this AU\n"));
				}
			}
			if (! (*activeChannel)) *activeChannel = ch;
			/*we have received the first AU of enhencement layer, wait for a moment for aggregation
			if (ch->AU_buffer_first && (ch->esd->dependsOnESID == (*activeChannel)->esd->ESID)) {
				*nextAU = NULL;
				return;
			}*/
			continue;
		}

		cts_diff = AU->CTS;
		cts_diff -= curCTS;
		if (cts_diff < 0) cts_diff = -cts_diff;

		/*aggregate all AUs with the same timestamp on the base AU and delete the upper layers)*/
		if (! *nextAU) {
			if (ch->esd->dependsOnESID) {
				//gf_es_drop_au(ch);
				continue;
			}
			GF_LOG(GF_LOG_DEBUG, GF_LOG_CODEC, ("[%s] ODM%d#CH%d (%s) AU DTS %d (size %d) selected as first layer (CTS %d)\n", codec->decio->module_name, codec->odm->OD->objectDescriptorID, ch->esd->ESID, ch->odm->net_service->url, AU->DTS, AU->dataLength, AU->CTS));
			*nextAU = AU;
			*activeChannel = ch;
			curCTS = AU->CTS;
		}
		//we allow for +/- 1ms drift due to timestamp rounding when converting to milliseconds units
		else if (cts_diff<=1) {
			GF_DBUnit *baseAU = *nextAU;
			assert(baseAU);
			if ((*activeChannel)->is_pulling && !(baseAU->flags & GF_DB_AU_REAGGREGATED)) {
				char *base_au = baseAU->data;
				baseAU->data = gf_malloc(baseAU->dataLength + AU->dataLength);
				memcpy(baseAU->data, base_au, baseAU->dataLength);
				memcpy(baseAU->data + baseAU->dataLength , AU->data, AU->dataLength);
			} else {
				baseAU->data = gf_realloc(baseAU->data, baseAU->dataLength + AU->dataLength);
				memcpy(baseAU->data + baseAU->dataLength , AU->data, AU->dataLength);
			}
			baseAU->dataLength += AU->dataLength;
			GF_LOG(GF_LOG_DEBUG, GF_LOG_CODEC, ("[%s] ODM%d#CH%d (%s) AU DTS %d reaggregated on base layer %d\n", codec->decio->module_name, codec->odm->OD->objectDescriptorID, ch->esd->ESID, ch->odm->net_service->url, AU->DTS, (*activeChannel)->esd->ESID));
			gf_es_drop_au(ch);
			ch->first_au_fetched = 1;
			scalable_check = 2;
			(*nextAU)->flags |= GF_DB_AU_REAGGREGATED;
		}
		//not the same TS for base and enhancement - either temporal scalability is used or we had a frame loss
		else {
			//we cannot rely on DTS - to check if this is temporal scalability, check next CTS
			if (ch->recompute_dts) {
				Bool au_match_base_ts = GF_FALSE;
				GF_DBUnit *next_unit = AU->next;
				while (next_unit) {
					//SNR/spatial scalable found later: we need to start from the enhancement layer
					if (next_unit->CTS==curCTS) {
						au_match_base_ts = GF_TRUE;
						break;
					}
					next_unit = next_unit->next;
				}
				//no AU found with the same CTS as the current base, we likely had a drop in the enhancement - aggregate from base
				if (!au_match_base_ts) {
				}
				// AU found with the same CTS as the current base, we either had a drop on the base or some temporal scalability - aggregate from current channel.
				else {
					//we cannot tell whether this is a loss or temporal scalable, don't attempt to discard the AU
					GF_LOG(GF_LOG_DEBUG, GF_LOG_CODEC, ("[%s] ODM%d#CH%d (%s) AU CTS %d doesn't have the same CTS as the base (%d)- selected as first layer\n", codec->decio->module_name, codec->odm->OD->objectDescriptorID, ch->esd->ESID, ch->odm->net_service->url, AU->CTS, (*nextAU)->CTS));
					*nextAU = AU;
					*activeChannel = ch;
					curCTS = AU->CTS;
				}
			}
			//we can rely on DTS - if DTS is earlier on the enhencement, this is a loss or temporal scalability
			else if (AU->DTS < (*nextAU)->DTS) {
				//Sample with the same DTS of this AU has been decoded. This is a loss, we need to drop it and re-fetch this channel
				if ((AU->DTS <= codec->last_unit_dts)
				        //we also prevent detecting temporal scalability until at least one frame from the base has been decoded
				        || !codec->first_frame_processed) {
					GF_LOG(GF_LOG_DEBUG, GF_LOG_CODEC, ("[%s] ODM%d#CH%d %s AU DTS %d but base DTS %d: loss detected - re-fetch channel\n", codec->decio->module_name, codec->odm->OD->objectDescriptorID, ch->esd->ESID, ch->odm->net_service->url, AU->DTS, (*nextAU)->DTS));
					gf_es_drop_au(ch);
					//restore stream state in case we got a RAP this time but we discard the AU, we need to wait again for the next RAP with the right timing
					ch->stream_state = stream_state;

					goto refetch_AU;
				}
				//This is a temporal scalability so we re-aggregate from the enhencement
				else {
					GF_LOG(GF_LOG_DEBUG, GF_LOG_CODEC, ("[%s] ODM%d#CH%d (%s) AU DTS %d selected as first layer (CTS %d)\n", codec->decio->module_name, codec->odm->OD->objectDescriptorID, ch->esd->ESID, ch->odm->net_service->url, AU->DTS, AU->CTS));
					*nextAU = AU;
					*activeChannel = ch;
					curCTS = AU->CTS;
				}
			} else {
				if ((*nextAU)->flags & GF_DB_AU_REAGGREGATED) {
					scalable_check = 2;
				} else {
					GF_LOG(GF_LOG_INFO, GF_LOG_CODEC, ("AU in enhancement layer DTS %d - CTS %d too early for this AU\n", AU->DTS, AU->CTS));
				}
			}
		}
	}
	//scalable addon, browse channels in scalable object
	if (current_odm->upper_layer_odm) {
		if (*nextAU) {
			gf_scene_check_addon_restart(current_odm->upper_layer_odm->parentscene->root_od->addon, (*nextAU)->CTS, (*nextAU)->DTS);
		}
		current_odm = current_odm->upper_layer_odm;
		src_channels = current_odm->channels;
		scalable_check = 1;
		goto browse_scalable;
	}

	if (scalable_check==1) {
		GF_LOG(GF_LOG_INFO, GF_LOG_CODEC, ("Warning, could not find enhancement layer for this AU\n"));
	}

	if (codec->is_reordering && *nextAU && codec->first_frame_dispatched) {
		if ((*activeChannel)->esd->slConfig->no_dts_signaling) {
			u32 CTS = (*nextAU)->CTS;
			/*reordering !!*/
			u32 prev_ts_diff;
			u32 diff = 0;
			if (codec->recomputed_cts && (codec->recomputed_cts > (*nextAU)->CTS)) {
				diff = codec->recomputed_cts - CTS;
			}

			prev_ts_diff = (CTS > codec->last_unit_cts) ? (CTS - codec->last_unit_cts) : (codec->last_unit_cts - CTS);
			if (!diff) diff = prev_ts_diff;
			else if (prev_ts_diff && (prev_ts_diff < diff) ) diff = prev_ts_diff;

			if (!codec->min_au_duration || (diff < codec->min_au_duration))
				codec->min_au_duration = diff;
		} else {
			codec->min_au_duration = 0;
			/*FIXME - we're breaking sync (couple of frames delay)*/
			(*nextAU)->CTS = (*nextAU)->DTS;
		}
	}

}

/*scalable browsing of input channels: find the AU with the lowest DTS on all input channels*/
static void Decoder_GetNextAU(GF_Codec *codec, GF_Channel **activeChannel, GF_DBUnit **nextAU)
{
	GF_Channel *ch;
	GF_DBUnit *AU;
	u32 count, minDTS, i;
	count = gf_list_count(codec->inChannels);
	*nextAU = NULL;
	*activeChannel = NULL;

	if (!count) return;

	minDTS = 0;
	/*reverse browsing to make sure we fill enhancement before base layer*/
	for (i=count; i>0; i--) {
		ch = (GF_Channel*)gf_list_get(codec->inChannels, i-1);

		if ((codec->type==GF_STREAM_OCR) && ch->IsClockInit) {
			/*check duration - we assume that scalable OCR streams are just pure nonsense...*/
			if (ch->is_pulling && codec->odm->duration) {
				if (gf_clock_time(codec->ck) > codec->odm->duration)
					gf_es_on_eos(ch);
			}
			return;
		}

		AU = gf_es_get_au(ch);
		if (!AU) {
			if (! (*activeChannel)) *activeChannel = ch;
			continue;
		}

		/*we use <= to make sure we first fetch data on base layer if same
		DTS (which is possible in spatial scalability)*/
		if (!minDTS || (AU->DTS == minDTS)) {
			minDTS = AU->DTS;
			*activeChannel = ch;
			*nextAU = AU;
		}
	}
}


static GF_Err SystemCodec_Process(GF_Codec *codec, u32 TimeAvailable)
{
	GF_DBUnit *AU;
	GF_Channel *ch;
	u32 obj_time, mm_level, au_time, cts;
	u64 now;
	GF_Scene *scene_locked;
	Bool check_next_unit;
	GF_SceneDecoder *sdec = (GF_SceneDecoder *)codec->decio;
	GF_Err e = GF_OK;

	scene_locked = NULL;

	/*for resync, if needed - the logic behind this is that there is no composition memory on sytems codecs so
	"frame dropping" is done by preventing the compositor from redrawing after an update and decoding following AU
	so that the compositor is always woken up once all late systems AUs are decoded. This flag is overriden when
	seeking*/
	check_next_unit = (codec->odm->term->flags & GF_TERM_DROP_LATE_FRAMES) ? 1 : 0;

check_unit:

	/*muting systems codec means we don't decode until mute is off - likely there will be visible however
	there is no other way to decode system AUs without modifying the content, which is what mute is about on visual...*/
	if (codec->Muted) goto exit;

	/*fetch next AU in DTS order for this codec*/
	Decoder_GetNextAU(codec, &ch, &AU);

	/*no active channel return*/
	if (!AU || !ch) {
		/*if the codec is in EOS state, move to STOP*/
		if (codec->Status == GF_ESM_CODEC_EOS) {
			GF_CodecCapability cap;
			cap.CapCode = GF_CODEC_MEDIA_NOT_OVER;
			cap.cap.valueInt = 0;
			sdec->GetCapabilities(codec->decio, &cap);
			if (!cap.cap.valueInt) {
				gf_term_stop_codec(codec, 0);
				gf_odm_signal_eos(ch->odm);
				if ((codec->type==GF_STREAM_OD) && (codec->nb_dec_frames==1)) {
					/*this is just by safety, since seeking is only allowed when a single clock is present
					in the scene*/
					if (gf_list_count(codec->odm->net_service->Clocks)==1)
						codec->odm->subscene->static_media_ressources=1;
				}
			}
		}
		goto exit;
	}

#ifndef GPAC_DISABLE_VRML
	if (ch && ch->odm->media_ctrl && !ch->odm->media_ctrl->media_speed)
		goto exit;
#endif

	/*get the object time*/
	obj_time = gf_clock_time(codec->ck);

	/*clock is not started*/
//	if (ch->first_au_fetched && !gf_clock_is_started(ch->clock)) goto exit;

	/*check timing based on the input channel and main FPS*/
	if (AU->DTS > obj_time) {
		gf_sc_set_system_pending_frame(ch->odm->term->compositor, 1);
		goto exit;
	}
	gf_sc_set_system_pending_frame(ch->odm->term->compositor, 0);

	cts = AU->CTS;
	/*in cases where no CTS was set for the BIFS (which may be interpreted as "now", although not compliant), use the object clock*/
	if (AU->flags & GF_DB_AU_NO_TIMESTAMPS) au_time = obj_time;
	/*case where CTS is in the past (carousels) */
	else if (AU->flags & GF_DB_AU_CTS_IN_PAST) {
		au_time = - (s32) AU->CTS;
		cts = AU->DTS;
	}
	/*regular case, SFTime will be from CTS (since DTS == CTS)*/
	else au_time = AU->DTS;

	/*check seeking and update timing - do NOT use the base layer, since BIFS streams may depend on other
	streams not on the same clock*/
	if (codec->last_unit_cts == cts) {
		/*seeking for systems is done by not releasing the graph until seek is done*/
		check_next_unit = 1;
		mm_level = GF_CODEC_LEVEL_SEEK;

	}
	/*set system stream timing*/
	else {
		codec->last_unit_cts = AU->CTS;
		/*we're droping the frame*/
		if (scene_locked) codec->nb_droped ++;
		mm_level = GF_CODEC_LEVEL_NORMAL;
	}

	/*lock scene*/
	if (!scene_locked) {
		scene_locked = codec->odm->subscene ? codec->odm->subscene : codec->odm->parentscene;
		if (!gf_mx_try_lock(scene_locked->root_od->term->compositor->mx))
			return GF_OK;
		/*if terminal is paused, force step-mode: it won't hurt in regular pause/play and ensures proper frame dumping*/
		if (codec->odm->term->play_state) codec->odm->term->compositor->step_mode=1;
	}

	/*current media time for system objects is the clock time, since the media is likely to have random
	updates in time*/
	codec->odm->media_current_time = obj_time	 - codec->ck->init_time;

	now = gf_sys_clock_high_res();
	if (codec->odm->term->bench_mode==2) {
		e = GF_OK;
	} else {
		e = sdec->ProcessData(sdec, AU->data, AU->dataLength, ch->esd->ESID, au_time, mm_level);
	}
	now = gf_sys_clock_high_res() - now;

	GF_LOG(GF_LOG_DEBUG, GF_LOG_CODEC, ("[%s] ODM%d#CH%d at %d decoded AU TS %d in "LLU" us\n", sdec->module_name, codec->odm->OD->objectDescriptorID, ch->esd->ESID, obj_time, AU->CTS, now));

	codec_update_stats(codec, AU->dataLength, now, AU->DTS, (AU->flags & GF_DB_AU_RAP));
	codec->prev_au_size = AU->dataLength;

	/*destroy this AU*/
	gf_es_drop_au(ch);

	if (e) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_CODEC, ("[SysDec] Codec %s AU CTS %d Decode error %s\n", sdec->module_name , cts, gf_error_to_string(e) ));
		if (e<0) ch->stream_state = 2;
		goto exit;
	}

	/*in broadcast mode, generate a scene if none is available*/
	if (codec->ck->no_time_ctrl) {
		GF_Scene *scene = codec->odm->subscene ? codec->odm->subscene : codec->odm->parentscene;

		/*static OD resources (embedded in ESD) in broadcast mode, reset time*/
		if (codec->flags & GF_ESM_CODEC_IS_STATIC_OD) gf_clock_reset(codec->ck);
		/*generate a temp scene if none is in place*/
		if (scene->graph_attached != 1) {
			Bool prev_dyn = scene->is_dynamic_scene;
			scene->is_dynamic_scene = 1;
			gf_scene_regenerate(scene);
			scene->graph_attached = 2;
			scene->is_dynamic_scene = prev_dyn;
			GF_LOG(GF_LOG_INFO, GF_LOG_CODEC, ("[Decoder] Got OD resources before scene - generating temporary scene\n"));
		}
	}

	/*if no release restart*/
	if (check_next_unit) goto check_unit;

exit:
	if (scene_locked) {
		gf_mx_v(scene_locked->root_od->term->compositor->mx);
	}
	return e;
}



/*special handling of decoders not using ESM*/
static GF_Err PrivateScene_Process(GF_Codec *codec, u32 TimeAvailable)
{
	u64 now;
	GF_Channel *ch;
	GF_Scene *scene_locked;
	GF_SceneDecoder *sdec = (GF_SceneDecoder *)codec->decio;
	GF_Err e = GF_OK;

	/*muting systems codec means we don't decode until mute is off - likely there will be visible however
	there is no other way to decode system AUs without modifying the content, which is what mute is about on visual...*/
	if (codec->Muted) return GF_OK;

	if (codec->Status == GF_ESM_CODEC_EOS) {
		gf_term_stop_codec(codec, 0);
		return GF_OK;
	}

	scene_locked = codec->odm->subscene ? codec->odm->subscene : codec->odm->parentscene;

	ch = (GF_Channel*)gf_list_get(codec->inChannels, 0);
	if (!ch) return GF_OK;
	/*init channel clock*/
	if (!ch->IsClockInit) {
		Bool started;
		/*signal seek*/
		if (!gf_mx_try_lock(scene_locked->root_od->term->compositor->mx)) return GF_OK;
		gf_es_init_dummy(ch);
		if (codec->odm->term->bench_mode != 2) {
			sdec->ProcessData(sdec, NULL, 0, ch->esd->ESID, -1, GF_CODEC_LEVEL_NORMAL);
		}
		gf_mx_v(scene_locked->root_od->term->compositor->mx);
		started = gf_clock_is_started(ch->clock);
		/*let's be nice to the scene loader (that usually involves quite some parsing), pause clock while
		parsing*/
		gf_clock_pause(ch->clock);
		codec->last_unit_dts = 0;
		if (!started) return GF_OK;
	}

	codec->odm->media_current_time = codec->last_unit_cts = gf_clock_time(codec->ck);
	codec->odm->media_current_time += codec->ck->media_time_at_init;
	codec->odm->media_current_time -= codec->ck->init_time;

	/*lock scene*/
	GF_LOG(GF_LOG_DEBUG, GF_LOG_CODEC, ("[PrivateDec] Codec %s Processing at %d\n", sdec->module_name, codec->last_unit_cts));

	if (!gf_mx_try_lock(scene_locked->root_od->term->compositor->mx)) return GF_OK;

	now = gf_sys_clock_high_res();
	if (codec->odm->term->bench_mode == 2) {
		e = GF_OK;
	} else {
		e = sdec->ProcessData(sdec, NULL, 0, ch->esd->ESID, codec->last_unit_cts, GF_CODEC_LEVEL_NORMAL);
	}
	now = gf_sys_clock_high_res() - now;
	codec->last_unit_dts ++;
	/*resume on error*/
	if (e && (codec->last_unit_dts<2) ) {
		gf_clock_resume(ch->clock);
		codec->last_unit_dts = 2;
	}
	/*resume clock on 2nd decode (we assume parsing is done in 2 steps, one for first frame display, one for complete parse)*/
	else if (codec->last_unit_dts==2) {
		gf_clock_resume(ch->clock);
	}

	codec_update_stats(codec, 0, now, codec->last_unit_cts, 0);

	gf_mx_v(scene_locked->root_od->term->compositor->mx);

	if (e==GF_EOS) {
		/*first end of stream, evaluate duration*/
		//if (!codec->odm->duration) gf_odm_set_duration(codec->odm, ch, codec->odm->current_time);
		gf_es_on_eos(ch);
		return GF_OK;
	}
	return e;
}
/*Get a pointer to the next CU buffer*/
static GFINLINE GF_Err LockCompositionUnit(GF_Codec *dec, u32 CU_TS, GF_CMUnit **cu, u32 *cu_size)
{
	if (!dec->CB) return GF_BAD_PARAM;

	*cu = gf_cm_lock_input(dec->CB, CU_TS, dec->is_reordering);
	if (! *cu ) return GF_OUT_OF_MEM;
	*cu_size = dec->CB->UnitSize;
	return GF_OK;
}


static GFINLINE GF_Err UnlockCompositionUnit(GF_Codec *dec, GF_CMUnit *CU, u32 cu_size)
{
	if (dec->is_reordering && !dec->trusted_cts) {
		/*first dispatch from decoder, store CTS*/
		if (!dec->first_frame_dispatched) {
			dec->recomputed_cts = CU->TS;
			dec->first_frame_dispatched = 1;
			GF_LOG(GF_LOG_DEBUG, GF_LOG_CODEC, ("[%s] ODM%d reordering mode - first frame dispatch - CTS %d - min TS diff %d\n", dec->decio->module_name, dec->odm->OD->objectDescriptorID, dec->recomputed_cts, dec->min_au_duration));
		} else if (dec->min_au_duration) {
			dec->recomputed_cts += dec->min_au_duration;
			GF_LOG(GF_LOG_DEBUG, GF_LOG_CODEC, ("[%s] ODM%d reordering mode - original CTS %d recomputed CTS %d - min TS diff %d\n", dec->decio->module_name, dec->odm->OD->objectDescriptorID, CU->TS, dec->recomputed_cts, dec->min_au_duration));
			CU->TS = dec->recomputed_cts;
		}
	}
	/*unlock the CB*/
	gf_cm_unlock_input(dec->CB, CU, cu_size, dec->is_reordering);
	return GF_OK;
}


GF_Err gf_codec_resize_composition_buffer(GF_Codec *dec, u32 NewSize)
{
	if (!dec || !dec->CB) return GF_BAD_PARAM;

	/*update config*/
	gf_mo_update_caps(dec->odm->mo);

	/*bytes per sec not available: either video or audio not configured*/
	if (!dec->bytes_per_sec) {
		if (NewSize && (NewSize != dec->CB->UnitSize) ) {
			GF_LOG(GF_LOG_DEBUG, GF_LOG_CODEC, ("[ODM] Resizing composition buffer for codec %s - %d bytes per unit\n", dec->decio->module_name, NewSize));
			gf_cm_resize(dec->CB, NewSize);
		}
	}
	/*audio: make sure we have enough data in CM to entirely fill the HW audio buffer...*/
	else {
		u32 unit_size, audio_buf_len, unit_count;
		GF_CodecCapability cap;
		unit_size = NewSize;
		/*a bit ugly, make some extra provision for speed >1. this is the drawback of working with pre-allocated memory
		for composition, we may get into cases where there will never be enough data for high speeds...
		FIXME - WE WILL NEED TO MOVE TO DYNAMIC CU BLOCKS IN ORDER TO SUPPORT ANY SPEED, BUT WHAT IS THE IMPACT
		FOR LOW RESOURCES DEVICES ??*/
//		audio_buf_len = 1000;
		audio_buf_len = 200;

		cap.CapCode = GF_CODEC_BUFFER_MAX;
		gf_codec_get_capability(dec, &cap);
		unit_count = cap.cap.valueInt;
		/*at least 2 units for dec and render ...*/
		if (unit_count<2) unit_count = 2;
		while (unit_size*unit_count*1000 < dec->bytes_per_sec*audio_buf_len) unit_count++;
#ifdef __SYMBIAN32__
		/*FIXME - symbian tests*/
		unit_count = 10;
#endif
		gf_cm_reinit(dec->CB, unit_size, unit_count);
		dec->CB->Min = unit_count/3;
		if (!dec->CB->Min) dec->CB->Min = 1;
	}

	//reset bitrate compute
	dec->cur_bit_size = 0;
	dec->last_stat_start = 0;

	//if dynamic scene, set size
	if ((dec->type==GF_STREAM_VISUAL) && dec->odm->parentscene->is_dynamic_scene) {
		gf_scene_force_size_to_video(dec->odm->parentscene, dec->odm->mo);
	}
	return GF_OK;
}

static GF_Err MediaCodec_Process(GF_Codec *codec, u32 TimeAvailable)
{
	GF_CMUnit *CU;
	GF_DBUnit *AU;
	GF_Channel *ch, *prev_ch;
	Bool drop_late_frames = 0;
	u64 now, entryTime;
	u32 mmlevel, cts;
	u32 first, obj_time, unit_size;
	GF_MediaDecoder *mdec = (GF_MediaDecoder*)codec->decio;
	GF_Err e = GF_OK;
	CU = NULL;

	/*if video codec muted don't decode (try to saves ressources)
	if audio codec muted we dispatch to keep sync in place*/
	if (codec->Muted && (codec->type==GF_STREAM_VISUAL) ) return GF_OK;

	//cannot output frame, do nothing (we force a channel query before for pull mode)
	if (codec->CB->Capacity == codec->CB->UnitCount) {
		if (codec->Status==GF_ESM_CODEC_PAUSE) {
			gf_term_stop_codec(codec, 1);
		}
		if (codec->CB->UnitCount > 1) return GF_OK;
		else if (codec->direct_vout) return GF_OK;
	}

	entryTime = gf_sys_clock_high_res();

	if (!codec->odm->term->bench_mode && (codec->odm->term->flags & GF_TERM_DROP_LATE_FRAMES))
		drop_late_frames = 1;


	/*fetch next AU in DTS order for this codec*/
	MediaDecoder_GetNextAU(codec, &ch, &AU);

	/*no active channel return*/
	if (!AU || !ch) {
		/*if the codec is in EOS state, assume we're done*/
		if (codec->Status == GF_ESM_CODEC_EOS) {
			/*if codec is reordering, try to flush it*/
			if (codec->is_reordering) {
				if ( LockCompositionUnit(codec, codec->last_unit_cts+1, &CU, &unit_size) == GF_OUT_OF_MEM)
					return GF_OK;
				assert( CU );
				unit_size = 0;
				if (codec->odm->term->bench_mode != 2) {
					e = mdec->ProcessData(mdec, NULL, 0, 0, &CU->TS, CU->data, &unit_size, 0, 0);
					if (e==GF_OK) {
						e = UnlockCompositionUnit(codec, CU, unit_size);
						if (unit_size) return GF_OK;
					}
				}
			}
			gf_term_stop_codec(codec, 0);
			if (codec->CB) {
				gf_cm_set_eos(codec->CB);
			}
		}
		/*if no data, and channel not buffering, ABORT CB buffer (data timeout or EOS not detectable)*/
		else if (ch && !ch->is_pulling && !ch->BufferOn && !ch->last_au_was_seek)
			gf_cm_abort_buffering(codec->CB);

		//GF_LOG(GF_LOG_DEBUG, GF_LOG_CODEC, ("[%s] ODM%d: No data in decoding buffer\n", codec->decio->module_name, codec->odm->OD->objectDescriptorID));
		return GF_OK;
	}

	/*get the object time*/
	obj_time = gf_clock_time(codec->ck);

	/*Media Time for media codecs is updated in the CB*/

	if (!codec->CB) {
		gf_es_drop_au(ch);
		return GF_BAD_PARAM;
	}
	/*we are still flushing our CB - keep the current pending AU and wait for CB resize*/
	if (codec->force_cb_resize) {
		/*we cannot destroy the CB here, as we don't know who is using its output*/
		return GF_OK;
	}

	/*image codecs*/
	if (!codec->CB->no_allocation && (codec->CB->Capacity == 1)) {
		/*a SHA signature is computed for each AU. This avoids decoding/recompositing when identical (for instance streaming a carousel)*/
		u8 new_unit_signature[20];
		gf_sha1_csum((u8*)AU->data, AU->dataLength, new_unit_signature);
		if (!memcmp(codec->last_unit_signature, new_unit_signature, sizeof(new_unit_signature))) {
			codec->nb_repeted_frames++;
			gf_es_drop_au(ch);
			return GF_OK;
		}

		/*usually only one image is tolerated in the stream, but just in case force reset of CB*/
		if (codec->CB->UnitCount && (obj_time>=AU->CTS)) {
			gf_mx_p(codec->odm->mx);
			codec->CB->output->dataLength = 0;
			codec->CB->UnitCount = 0;
			gf_mx_v(codec->odm->mx);
		}

		/*CB is already full*/
		if (codec->CB->UnitCount)
			return GF_OK;

		codec->nb_repeted_frames = 0;
		memcpy(codec->last_unit_signature, new_unit_signature, sizeof(new_unit_signature));

	}


	if (codec->ck->speed != codec->check_speed) {
		//decrease in speed
		if (ABS(codec->check_speed) > ABS(codec->ck->speed)) {
			codec->decode_only_rap = 0;
			codec->drop_modulo = 0;
			codec->drop_count = 0;
		}
		codec->check_speed = codec->ck->speed;
		codec->consecutive_late_frames = 0;
		codec->consecutive_ontime_frames = 0;

		if (codec->type==GF_STREAM_AUDIO) {
			if (ABS(codec->ck->speed) >8) {
				codec->decode_only_rap = 2;
				GF_LOG(GF_LOG_WARNING, GF_LOG_CODEC, ("[%s] Speed %g too hight for audio decoder/renderer - skipping decode\n", codec->decio->module_name, FIX2FLT(codec->ck->speed)));
			}
		} else {
			//dynamically decided based on late frames found
		}
	}

	TimeAvailable*=1000;
	/*try to refill the full buffer*/
	first = 1;
	while (codec->CB->Capacity > codec->CB->UnitCount) {
		Bool force_skip = 0;

		if (codec->decode_only_rap) {
			//check whether we have to skip decoding this frame
			if (AU->flags & GF_DB_AU_RAP) {
				if (codec->decode_only_rap==2) {
					if (AU->CTS > obj_time + 500)
						return GF_OK;
					force_skip = 1;
				} else if (codec->drop_modulo) {
					codec->drop_count ++;
					if (codec->drop_count >= codec->drop_modulo) {
						codec->drop_count = 0;
					} else {
						force_skip = 1;
					}
				}
			} else {
				force_skip = 1;
			}
		}

		/*set media processing level*/
		ch->last_au_was_seek = 0;
		mmlevel = GF_CODEC_LEVEL_NORMAL;
		/*SEEK: if the last frame had the same TS, we are seeking. Ask the codec to drop*/
		if (!ch->skip_sl && codec->last_unit_cts && (codec->last_unit_cts == AU->CTS) && !ch->esd->dependsOnESID) {
			mmlevel = GF_CODEC_LEVEL_SEEK;
			ch->last_au_was_seek = 1;
			/*object clock is paused by media control or terminal is paused: exact frame seek*/
			if (
#ifndef GPAC_DISABLE_VRML
			    (codec->ck->mc && codec->ck->mc->paused) ||
#endif
			    (codec->odm->term->play_state)
			) {
				gf_cm_rewind_input(codec->CB);
				mmlevel = GF_CODEC_LEVEL_NORMAL;
				/*force staying in step-mode*/
				codec->odm->term->compositor->step_mode=1;
			}
		}
		/*only perform drop in normal playback*/
		else if (!force_skip && (codec->ck->speed >= 0) && (codec->CB->Status == CB_PLAY)) {
			/*extremely late, set the level to drop
			 NOTE: the 100 ms safety gard is to avoid discarding audio*/
			if (!ch->skip_sl && (AU->CTS + (codec->is_reordering ? 1000 : 100) < obj_time) ) {
				mmlevel = GF_CODEC_LEVEL_DROP;
				GF_LOG(GF_LOG_INFO, GF_LOG_CODEC, ("[%s] ODM%d: frame too late (CTS %d vs time %d) - using drop level\n", codec->decio->module_name, codec->odm->OD->objectDescriptorID, AU->CTS, obj_time));

//above 1 out of this threshold frames only shown, move to I-frame only mode. 50 = 1 frame shown out of 2 GOPs at 25hz
#define IFRAME_MODE_THRESHOLD	50

				codec->consecutive_late_frames++;
				codec->consecutive_ontime_frames = 0;
				if (codec->check_speed > 1) {
					Double speed = (Double) FIX2FLT(codec->ck->speed);
					u32 nb_check_frames = codec->decode_only_rap ? 5 : 30;

					if (codec->consecutive_late_frames >= nb_check_frames) {
						if (!codec->decode_only_rap) {
							codec->drop_modulo += 2;
							codec->drop_count = 0;

							GF_LOG(GF_LOG_INFO, GF_LOG_CODEC, ("[%s] ODM%d: %d consecutive late frames at speed %g - increasing frame drop modulo to %d\n", codec->decio->module_name, codec->odm->OD->objectDescriptorID, codec->consecutive_late_frames, speed, codec->drop_modulo));
							if (codec->drop_modulo > IFRAME_MODE_THRESHOLD) {

								GF_LOG(GF_LOG_INFO, GF_LOG_CODEC, ("[%s] ODM%d: %d consecutive late frames at speed %g with drop modulo %d - moving to I-frame only decoding\n", codec->decio->module_name, codec->odm->OD->objectDescriptorID, codec->consecutive_late_frames, speed, codec->drop_modulo));
								codec->drop_modulo = 0;
								codec->drop_count = 0;
								codec->decode_only_rap = 1;
							}
						} else {
							codec->drop_modulo += 2;
							codec->drop_count = 0;

							GF_LOG(GF_LOG_INFO, GF_LOG_CODEC, ("[%s] ODM%d: %d consecutive late frames at speed %g in I-frame only mode - decoding only one I-frame out of %d\n", codec->decio->module_name, codec->odm->OD->objectDescriptorID, codec->consecutive_late_frames, speed, codec->drop_modulo));
						}
						codec->consecutive_late_frames = 0;
					}
					//very late even in I-frame mode, drop it
					else if (codec->decode_only_rap) {
						if (obj_time - AU->CTS > 2000) {
							force_skip = 1;
						}
					}
				}

				if (ch->resync_drift && (AU->CTS + ch->resync_drift < obj_time)) {
					ch->clock->StartTime += (obj_time - AU->CTS);
					GF_LOG(GF_LOG_WARNING, GF_LOG_CODEC, ("[%s] ODM%d: decoder too slow on OCR stream - rewinding clock of %d ms\n", codec->decio->module_name, codec->odm->OD->objectDescriptorID, obj_time - AU->CTS));
					obj_time = gf_clock_time(codec->ck);
				}
			}
			/*we are late according to the media manager*/
			else if (codec->PriorityBoost) {
				mmlevel = GF_CODEC_LEVEL_VERY_LATE;
				codec->consecutive_late_frames = 0;
				codec->consecutive_ontime_frames ++;
			}
			/*otherwise we must have an idea of the load in order to set the right level
			use the composition buffer for that, only on the first frame*/
			else {
				codec->consecutive_late_frames = 0;
				codec->consecutive_ontime_frames ++;
				if (first) {
					//if the CB is almost empty set to very late
					if (codec->CB->UnitCount <= codec->CB->Min+1) {
						mmlevel = GF_CODEC_LEVEL_VERY_LATE;
					} else if (codec->CB->UnitCount * 2 <= codec->CB->Capacity) {
						mmlevel = GF_CODEC_LEVEL_LATE;
					}
					first = 0;
				}
			}

			if (codec->check_speed > 1) {
				u32 nb_check_frames = codec->decode_only_rap ? 5 : 30;
				//try to decrease the drop rate, but never switch back from I-frame mode only for a given speed
				if (codec->consecutive_ontime_frames > nb_check_frames) {
					if (codec->drop_modulo) {
						codec->drop_modulo -= 2;
						codec->drop_count = 0;
						GF_LOG(GF_LOG_INFO, GF_LOG_CODEC, ("[%s] ODM%d: %d consecutive on-time frames - Decreasing drop modulo %d (I-frame only mode %d)\n", codec->decio->module_name, codec->odm->OD->objectDescriptorID, codec->consecutive_ontime_frames, codec->drop_modulo, codec->decode_only_rap));
						codec->consecutive_late_frames = 0;
						codec->consecutive_ontime_frames = 0;
					}
				}
			}
		}

		if (ch->skip_sl) {
			if (codec->bytes_per_sec) {
				AU->CTS = codec->last_unit_cts + ch->ts_offset + codec->cur_audio_bytes * 1000 / codec->bytes_per_sec;
			} else if (codec->fps) {
				AU->CTS = codec->last_unit_cts + ch->ts_offset + (u32) (codec->cur_video_frames * 1000 / codec->fps);
			}
		}
		if ( LockCompositionUnit(codec, AU->CTS, &CU, &unit_size) == GF_OUT_OF_MEM) {
			GF_LOG(GF_LOG_DEBUG, GF_LOG_CODEC, ("[%s] Exit decode loop because no more space in composition buffer\n", codec->decio->module_name ));
			return GF_OK;
		}


scalable_retry:

		now = gf_sys_clock_high_res();

		assert( CU );
		if (!CU->data && unit_size && !codec->CB->no_allocation) {
			e = GF_OUT_OF_MEM;
		} else if (codec->odm->term->bench_mode==2) {
			unit_size = 0;
			gf_cm_abort_buffering(codec->CB);
		} else if (force_skip) {
			unit_size = 0;
			GF_LOG(GF_LOG_DEBUG, GF_LOG_CODEC, ("[%s] ODM%d: force drop requested in fast playback for AU CTS %d\n", codec->decio->module_name, codec->odm->OD->objectDescriptorID, AU->CTS));
		} else {
			GF_LOG(GF_LOG_DEBUG, GF_LOG_CODEC, ("[%s] ODM%d ES%d at %d decoding frame DTS %d CTS %d size %d (%d in channels)\n", codec->decio->module_name, codec->odm->OD->objectDescriptorID, ch->esd->ESID, gf_clock_real_time(ch->clock), AU->DTS, AU->CTS, AU->dataLength, ch->AU_Count));
			e = mdec->ProcessData(mdec, AU->data, AU->dataLength, ch->esd->ESID, &CU->TS, CU->data, &unit_size, AU->PaddingBits, mmlevel);
		}
		now = gf_sys_clock_high_res() - now;
		if (codec->Status == GF_ESM_CODEC_STOP) {
			GF_LOG(GF_LOG_DEBUG, GF_LOG_CODEC, ("[%s] Exit decode loop because codec has been stopped\n", codec->decio->module_name));
			return GF_OK;
		}
		/*input is too small, resize composition memory*/
		switch (e) {
		case GF_BUFFER_TOO_SMALL:
			/*release but no dispatch*/
			UnlockCompositionUnit(codec, CU, 0);

			/*if we have pending media, wait untill CB is completely flushed by the compositor - this shoud be fixed by avoiding to destroy the CB ... */
			if (codec->CB->LastRenderedTS) {
				GF_LOG(GF_LOG_INFO, GF_LOG_CODEC, ("[%s] ODM%d ES%d: Resize output buffer requested\n", codec->decio->module_name, codec->odm->OD->objectDescriptorID, ch->esd->ESID));
				codec->force_cb_resize = unit_size;
				return GF_OK;
			}
			GF_LOG(GF_LOG_INFO, GF_LOG_CODEC, ("[%s] ODM%d ES%d: Resizing output buffer %d -> %d\n", codec->decio->module_name, codec->odm->OD->objectDescriptorID, ch->esd->ESID, codec->CB->UnitSize, unit_size));
			gf_codec_resize_composition_buffer(codec, unit_size);
			continue;

		/*this happens a lot when using non-MPEG-4 streams (ex: ffmpeg demuxer)*/
		case GF_PACKED_FRAMES:
			/*in seek do dispatch output otherwise we will only see the I-frame preceding the seek point*/
			if (mmlevel == GF_CODEC_LEVEL_DROP) {
				if (drop_late_frames) {
					unit_size = 0;
					codec->nb_droped++;
				} else
					ch->clock->last_TS_rendered = codec->CB->LastRenderedTS;
			}
			e = UnlockCompositionUnit(codec, CU, unit_size);

			GF_LOG(GF_LOG_DEBUG, GF_LOG_CODEC, ("[%s] ODM%d ES%d at %d decoded packed frame TS %d in "LLU" us\n", codec->decio->module_name, codec->odm->OD->objectDescriptorID, ch->esd->ESID, gf_clock_real_time(ch->clock), AU->CTS, now));
			if (ch->skip_sl) {
				if (codec->bytes_per_sec) {
					codec->cur_audio_bytes += unit_size;
				} else if (codec->fps && unit_size) {
					codec->cur_video_frames += 1;
				}
			} else {
				u32 deltaTS = 0;
				if (codec->bytes_per_sec) {
					deltaTS = unit_size * 1000 / codec->bytes_per_sec;
				} /*else if (0 && codec->fps && unit_size) {
					deltaTS = (u32) (1000.0f / codec->fps);
				} */else {
					deltaTS = (AU->DTS - codec->last_unit_dts);
				}
				AU->CTS += deltaTS;
			}
			codec_update_stats(codec, 0, now, 0, 0);
			continue;

		/*for all cases below, don't release the composition buffer until we are sure we are not
		processing a scalable stream*/
		case GF_OK:
			if (unit_size) {
				GF_LOG(GF_LOG_DEBUG, GF_LOG_CODEC, ("[%s] ODM%d ES%d at %d decoded frame TS %d in "LLU" us (DTS %d - size %d) - %d in CB\n", codec->decio->module_name, codec->odm->OD->objectDescriptorID, ch->esd->ESID, gf_clock_real_time(ch->clock), AU->CTS, now, AU->DTS, AU->dataLength, codec->CB->UnitCount + 1));

				if (codec->direct_vout) {
					e = mdec->GetOutputBuffer(mdec, ch->esd->ESID, &codec->CB->pY, &codec->CB->pU, &codec->CB->pV);
					if (e==GF_OK) {
						gf_sc_set_video_pending_frame(codec->odm->term->compositor);
					}
				}
			}
#if 0
			/*if no size and the decoder is not using the composition memory - if the object is in intitial buffering resume it!!*/
			else if ( (!codec->CB->UnitSize && !codec->CB->Capacity) && (codec->CB->Status == CB_BUFFER)) {
				codec->nb_dispatch_skipped++;
				if (codec->nb_dispatch_skipped==codec->CB->UnitCount)
					gf_cm_abort_buffering(codec->CB);
			}
#endif

			codec_update_stats(codec, AU->dataLength, now, AU->DTS, (AU->flags & GF_DB_AU_RAP));
			if (ch->skip_sl) {
				if (codec->bytes_per_sec) {
					codec->cur_audio_bytes += unit_size;
					while (codec->cur_audio_bytes>codec->bytes_per_sec) {
						codec->cur_audio_bytes -= codec->bytes_per_sec;
						codec->last_unit_cts += 1000;
					}
				} else if (codec->fps && unit_size) {
					codec->cur_video_frames += 1;
				}
			}
#ifndef GPAC_DISABLE_LOGS
			if (codec->odm->flags & GF_ODM_PREFETCH) {
				GF_LOG(GF_LOG_INFO, GF_LOG_CODEC, ("[%s] ODM%d ES%d At %d decoding frame TS %d in prefetch mode\n", codec->decio->module_name, codec->odm->OD->objectDescriptorID, ch->esd->ESID, gf_clock_real_time(ch->clock), AU->CTS));
			}
#endif
			break;
		default:
			unit_size = 0;
			/*error - if the object is in intitial buffering resume it!!*/
			gf_cm_abort_buffering(codec->CB);
			GF_LOG(GF_LOG_INFO, GF_LOG_CODEC, ("[%s] ODM%d ES%d At %d (frame TS %d - "LLU" us ): decoded error %s\n", codec->decio->module_name, codec->odm->OD->objectDescriptorID, ch->esd->ESID, gf_clock_real_time(ch->clock), AU->CTS, now, gf_error_to_string(e) ));
			e = GF_OK;
			break;
		}

		codec->last_unit_dts = AU->DTS;
		/*remember base layer timing*/
		if (!ch->esd->dependsOnESID && !ch->skip_sl) {
			codec->last_unit_cts = AU->CTS;
			codec->first_frame_processed = 1;
		}

#ifndef GPAC_DISABLE_LOG
		if (unit_size) {
			if (ch->is_pulling) {
				GF_LOG(GF_LOG_DEBUG, GF_LOG_MEDIA, ("[%s] at %d decoded frame CTS %d in "LLU" us\n", codec->decio->module_name, gf_clock_real_time(ch->clock), AU->CTS, now));
			} else {
				GF_LOG(GF_LOG_DEBUG, GF_LOG_MEDIA, ("[%s] at %d decoded frame CTS %d in "LLU" us - %d AU in channel\n", codec->decio->module_name, gf_clock_real_time(ch->clock), AU->CTS, now, ch->AU_Count));
			}
		}
#endif

		/*store current CTS*/
		cts = AU->CTS;
		prev_ch = ch;

		gf_es_drop_au(ch);
		AU = NULL;

		if (e) {
			UnlockCompositionUnit(codec, CU, unit_size);
			GF_LOG(GF_LOG_DEBUG, GF_LOG_CODEC, ("[%s] Exit decode loop because error %s\n", codec->decio->module_name, gf_error_to_string(e) ));
			return e;
		}

		MediaDecoder_GetNextAU(codec, &ch, &AU);
		/*same CTS: same output, likely scalable stream so don't release the CB*/
		if (AU && (AU->CTS == cts) && (ch != prev_ch) ) {
			unit_size = codec->CB->UnitSize;
			goto scalable_retry;
		}

		/*in seek don't dispatch any output*/
		if (mmlevel >= GF_CODEC_LEVEL_DROP) {
			if (drop_late_frames || (mmlevel == GF_CODEC_LEVEL_SEEK) ) {
				unit_size = 0;
				if (drop_late_frames) codec->nb_droped++;
			} else
				ch->clock->last_TS_rendered = codec->CB->LastRenderedTS;
		} else {
			ch->clock->last_TS_rendered = 0;
		}


		if (!codec->decode_only_rap && codec->drop_modulo) {
			codec->drop_count++;
			if (codec->drop_count==codec->drop_modulo) {
				codec->drop_count = 0;
			} else {
				unit_size = 0;
			}
		}

		UnlockCompositionUnit(codec, CU, unit_size);
		if (!ch || !AU) {
			GF_LOG(GF_LOG_DEBUG, GF_LOG_CODEC, ("[%s] Exit decode loop because no more input data\n", codec->decio->module_name));
			return GF_OK;
		}
		if (force_skip) continue;

		now = gf_sys_clock_high_res() - entryTime;
		/*escape from decoding loop only if above critical limit - this is to avoid starvation on audio*/
		if (!ch->esd->dependsOnESID && (codec->CB->UnitCount > codec->CB->Min)) {
			if (now >= TimeAvailable) {
				GF_LOG(GF_LOG_DEBUG, GF_LOG_CODEC, ("[%s] Exit decode loop because time is up: %d vs %d us available\n", codec->decio->module_name, now, TimeAvailable));
				return GF_OK;
			}
		} else if (now >= TimeAvailable) {
			GF_LOG(GF_LOG_DEBUG, GF_LOG_CODEC, ("[%s] Exit decode loop because running for too long: %d vs %d us available\n", codec->decio->module_name, now, TimeAvailable));
			return GF_OK;
		} else if (codec->odm->term->bench_mode) {
			return GF_OK;
		}
	}
	return GF_OK;
}


GF_Err gf_codec_process_private_media(GF_Codec *codec, u32 TimeAvailable)
{

	if (codec->ck && codec->ck->Paused) {
		u32 i;
		for (i=0; i<gf_list_count(codec->odm->channels); i++) {
			GF_Channel *ch = gf_list_get(codec->odm->channels, i);
			if (ch->BufferOn) {
				ch->BufferOn = 0;
				gf_clock_buffer_off(ch->clock);
			}
		}
		if (codec->CB)
			gf_cm_abort_buffering(codec->CB);
	}
	return GF_OK;
}


GF_Err gf_codec_process_raw_media_pull(GF_Codec *codec, u32 TimeAvailable)
{
	GF_Channel *ch;
	GF_DBUnit *db;
	if (codec->ck && codec->ck->Paused) {
		u32 i;
		for (i=0; i<gf_list_count(codec->odm->channels); i++) {
			ch = gf_list_get(codec->odm->channels, i);
			if (ch->BufferOn) {
				ch->BufferOn = 0;
				gf_clock_buffer_off(ch->clock);
			}
		}
		if (codec->CB)
			gf_cm_abort_buffering(codec->CB);
	}

	/*this will pull the next AU from the service */
	Decoder_GetNextAU(codec, &ch, &db);
	if (!db) return GF_OK;

	/*dispatch raw media - this call is blocking untill the cu has been consumed */
	gf_es_dispatch_raw_media_au(ch, db->data, db->dataLength, db->CTS);

	/*release AU from service*/
	gf_term_channel_release_sl_packet(ch->service, ch);
	return GF_OK;
}

GF_Err gf_codec_process_ocr(GF_Codec *codec, u32 TimeAvailable)
{
	/*OCR: needed for OCR in pull mode (dummy streams used to sync various sources)*/
	GF_DBUnit *AU;
	GF_Channel *ch;
	/*fetch next AU on OCR (empty AUs)*/
	Decoder_GetNextAU(codec, &ch, &AU);

	/*no active channel return*/
	if (!AU || !ch) {
		/*if the codec is in EOS state, move to STOP*/
		if (codec->Status == GF_ESM_CODEC_EOS) {
			gf_term_stop_codec(codec, 0);
#ifndef GPAC_DISABLE_VRML
			/*if a mediacontrol is ruling this OCR*/
			if (codec->odm->media_ctrl && codec->odm->media_ctrl->control->loop) mediacontrol_restart(codec->odm);
#endif
		}
	}
	return GF_OK;
}

GF_Err gf_codec_process(GF_Codec *codec, u32 TimeAvailable)
{
	if (codec->Status == GF_ESM_CODEC_STOP) return GF_OK;
#ifndef GPAC_DISABLE_VRML
	codec->Muted = (codec->odm->media_ctrl && codec->odm->media_ctrl->control->mute) ? 1 : 0;
#else
	codec->Muted = 0;
#endif

	return codec->process(codec, TimeAvailable);
}


GF_Err gf_codec_get_capability(GF_Codec *codec, GF_CodecCapability *cap)
{
	cap->cap.valueInt = 0;
	if (codec->decio) {
		return codec->decio->GetCapabilities(codec->decio, cap);
	}

	if (codec->flags & GF_ESM_CODEC_IS_RAW_MEDIA) {
		GF_BitStream *bs;
		u32 pf, w, h, stride=0, out_size, sr, nb_ch, bpp, ch_cfg, is_flipped = 0;
		GF_Channel *ch = gf_list_get(codec->odm->channels, 0);
		if (!ch || !ch->esd->decoderConfig->decoderSpecificInfo || !ch->esd->decoderConfig->decoderSpecificInfo->data) return 0;
		bs = gf_bs_new(ch->esd->decoderConfig->decoderSpecificInfo->data, ch->esd->decoderConfig->decoderSpecificInfo->dataLength, GF_BITSTREAM_READ);

		pf = w = h = sr = nb_ch = bpp = ch_cfg = 0;
		if (codec->type==GF_STREAM_VISUAL) {
			pf = gf_bs_read_u32(bs);
			w = gf_bs_read_u16(bs);
			h = gf_bs_read_u16(bs);
			out_size = gf_bs_read_u32(bs);
			stride = gf_bs_read_u32(bs);
			is_flipped = gf_bs_read_u8(bs);
		} else {
			sr = gf_bs_read_u32(bs);
			nb_ch = gf_bs_read_u16(bs);
			bpp = gf_bs_read_u16(bs);
			out_size = gf_bs_read_u32(bs);
			ch_cfg = gf_bs_read_u32(bs);
		}
		gf_bs_del(bs);
		switch (cap->CapCode) {
		case GF_CODEC_WIDTH:
			cap->cap.valueInt = w;
			return GF_OK;
		case GF_CODEC_HEIGHT:
			cap->cap.valueInt = h;
			return GF_OK;
		case GF_CODEC_STRIDE:
			cap->cap.valueInt = stride;
			return GF_OK;
		case GF_CODEC_PIXEL_FORMAT:
			cap->cap.valueInt = pf;
			return GF_OK;
		case GF_CODEC_FLIP:
			cap->cap.valueInt = is_flipped;
			return GF_OK;
		case GF_CODEC_OUTPUT_SIZE:
			cap->cap.valueInt = out_size;
			return GF_OK;
		case GF_CODEC_SAMPLERATE:
			cap->cap.valueInt = sr;
			return GF_OK;
		case GF_CODEC_NB_CHAN:
			cap->cap.valueInt = nb_ch;
			return GF_OK;
		case GF_CODEC_BITS_PER_SAMPLE:
			cap->cap.valueInt = bpp;
			return GF_OK;
		case GF_CODEC_CHANNEL_CONFIG:
			cap->cap.valueInt = ch_cfg;
			return GF_OK;
		case GF_CODEC_PAR:
			cap->cap.valueInt = 0;
			return GF_OK;
		case GF_CODEC_PADDING_BYTES:
			cap->cap.valueInt = 0;
			return GF_OK;
		case GF_CODEC_RESILIENT:
			cap->cap.valueInt = 1;
			return GF_OK;
		}
	}
	return GF_BAD_PARAM;
}

GF_Err gf_codec_set_capability(GF_Codec *codec, GF_CodecCapability cap)
{
	if (!codec->decio) return GF_OK;
	return codec->decio->SetCapabilities(codec->decio, cap);
}


void gf_codec_set_status(GF_Codec *codec, u32 Status)
{
	if (!codec) return;

	if (Status == GF_ESM_CODEC_PAUSE) {
		//stay in PLAY mode if we have a composition buffer, so that the "pause" state still output an initial picture
		codec->Status = codec->CB ? GF_ESM_CODEC_PAUSE : GF_ESM_CODEC_STOP;
	}
	else if (Status == GF_ESM_CODEC_BUFFER) codec->Status = GF_ESM_CODEC_PLAY;
	else if (Status == GF_ESM_CODEC_PLAY) {
		codec->last_unit_cts = 0;
		codec->prev_au_size = 0;
		codec->Status = Status;
		codec->last_stat_start = codec->cur_bit_size = codec->max_bit_rate = codec->avg_bit_rate = 0;
		codec->nb_dec_frames = 0;
		codec->nb_iframes = 0;
		codec->max_iframes_time = 0;
		codec->total_iframes_time = 0;
		codec->total_dec_time = 0;
		codec->max_dec_time = 0;
		codec->cur_audio_bytes = codec->cur_video_frames = 0;
		codec->nb_droped = 0;
		codec->nb_repeted_frames = 0;
		codec->recomputed_cts = 0;
		codec->first_frame_dispatched = 0;
		codec->first_frame_processed = 0;
		codec->nb_dispatch_skipped = 0;
		memset(codec->last_unit_signature, 0, sizeof(codec->last_unit_signature));
	}
	else 
		codec->Status = Status;

	if (!codec->CB) return;

	/*notify CB*/
	switch (Status) {
	case GF_ESM_CODEC_PLAY:
		gf_cm_set_status(codec->CB, CB_PLAY);
		return;
	case GF_ESM_CODEC_PAUSE:
		gf_cm_set_status(codec->CB, CB_PAUSE);
		return;
	case GF_ESM_CODEC_STOP:
		gf_cm_set_status(codec->CB, CB_STOP);
		return;
	case GF_ESM_CODEC_EOS:
		/*do NOT notify CB yet, wait till last AU is decoded*/
		return;
	case GF_ESM_CODEC_BUFFER:
	default:
		return;
	}
}

static GF_Err Codec_LoadModule(GF_Codec *codec, GF_ESD *esd, u32 PL)
{
	char szPrefDec[500];
	const char *sOpt;
	GF_BaseDecoder *ifce, *dec_ifce;
	u32 i, plugCount;
	u32 ifce_type;
	u32 dec_confidence;
	GF_Terminal *term = codec->odm->term;

	switch (esd->decoderConfig->streamType) {
	case GF_STREAM_AUDIO:
	case GF_STREAM_VISUAL:
	case GF_STREAM_ND_SUBPIC:
		ifce_type = GF_MEDIA_DECODER_INTERFACE;
		codec->process = MediaCodec_Process;
		break;
	case GF_STREAM_PRIVATE_MEDIA:
		ifce_type = GF_PRIVATE_MEDIA_DECODER_INTERFACE;
		codec->process = gf_codec_process_private_media;
		break;
	case GF_STREAM_PRIVATE_SCENE:
		ifce_type = GF_SCENE_DECODER_INTERFACE;
		codec->process = PrivateScene_Process;
		break;
	default:
		ifce_type = GF_SCENE_DECODER_INTERFACE;
		codec->process = SystemCodec_Process;
		if (esd->decoderConfig->objectTypeIndication==GPAC_OTI_SCENE_AFX) {
			ifce_type = GF_NODE_DECODER_INTERFACE;
		}
		break;
	}

	/*a bit dirty, if FFMPEG is used for demuxer load it for decoder too*/
	if (0 && !stricmp(codec->odm->net_service->ifce->module_name, "FFMPEG demuxer")) {
		sOpt = "FFMPEG decoder";
	} else {
		/*use user-defined module if any*/
		sOpt = NULL;
		switch (esd->decoderConfig->streamType) {
		case GF_STREAM_VISUAL:
			if ((esd->decoderConfig->objectTypeIndication==GPAC_OTI_IMAGE_JPEG) || (esd->decoderConfig->objectTypeIndication==GPAC_OTI_IMAGE_PNG))
				sOpt = gf_cfg_get_key(term->user->config, "Systems", "DefImageDec");
			else
				sOpt = gf_cfg_get_key(term->user->config, "Systems", "DefVideoDec");
			break;
		case GF_STREAM_AUDIO:
			sOpt = gf_cfg_get_key(term->user->config, "Systems", "DefAudioDec");
			break;
		default:
			break;
		}
	}

	dec_confidence = 0;
	ifce = NULL;

	if (sOpt) {
		ifce = (GF_BaseDecoder *) gf_modules_load_interface_by_name(term->user->modules, sOpt, ifce_type);
		if (ifce) {
			if (ifce->CanHandleStream) {
				dec_confidence = ifce->CanHandleStream(ifce, esd->decoderConfig->streamType, esd, PL);
				if (dec_confidence==GF_CODEC_SUPPORTED) {
					codec->decio = ifce;
					return GF_OK;
				}
				if (dec_confidence==GF_CODEC_NOT_SUPPORTED) {
					gf_modules_close_interface((GF_BaseInterface *) ifce);
					ifce = NULL;
				}
			} else {
				gf_modules_close_interface((GF_BaseInterface *) ifce);
			}
		}
	}

	dec_ifce = ifce;
	/*prefered codec module per streamType/objectType from config*/
	sprintf(szPrefDec, "codec_%02X_%02X", esd->decoderConfig->streamType, esd->decoderConfig->objectTypeIndication);
	sOpt = gf_cfg_get_key(term->user->config, "Systems", szPrefDec);

	//little hack - FFMPEG has some weird issues with MPEG1/2 audio on some systems, try to default to MAD if present
	if (!sOpt && (esd->decoderConfig->streamType==GF_STREAM_AUDIO)) {
		switch (esd->decoderConfig->objectTypeIndication) {
		case GPAC_OTI_AUDIO_MPEG2_PART3:
		case GPAC_OTI_AUDIO_MPEG1:
			sOpt = "MAD Decoder";
			gf_cfg_set_key(term->user->config, "Systems", szPrefDec, sOpt);
		}
	}

	if (sOpt) {
		ifce = (GF_BaseDecoder *) gf_modules_load_interface_by_name(term->user->modules, sOpt, ifce_type);
		if (ifce) {
			if (ifce->CanHandleStream) {
				u32 conf = ifce->CanHandleStream(ifce, esd->decoderConfig->streamType, esd, PL);
				if ((conf!=GF_CODEC_NOT_SUPPORTED) && (conf>=dec_confidence)) {
					/*switch*/
					if (dec_ifce) gf_modules_close_interface((GF_BaseInterface *) dec_ifce);
					dec_confidence = conf;
					dec_ifce = ifce;
					ifce = NULL;
					if (dec_confidence==GF_CODEC_SUPPORTED) {
						codec->decio = dec_ifce;
						return GF_OK;
					}
				}
			}
			if (ifce)
				gf_modules_close_interface((GF_BaseInterface *) ifce);
		}
	}
	/*not found, check all modules*/
	plugCount = gf_modules_get_count(term->user->modules);
	for (i = 0; i < plugCount ; i++) {
		ifce = (GF_BaseDecoder *) gf_modules_load_interface(term->user->modules, i, ifce_type);
		if (!ifce) continue;
		if (ifce->CanHandleStream) {
			u32 conf = ifce->CanHandleStream(ifce, esd->decoderConfig->streamType, esd, PL);

			if ((conf!=GF_CODEC_NOT_SUPPORTED) && (conf>dec_confidence)) {
				/*switch*/
				if (dec_ifce) gf_modules_close_interface((GF_BaseInterface *) dec_ifce);
				dec_confidence = conf;
				dec_ifce = ifce;
				ifce = NULL;
			}
		}
		if (ifce)
			gf_modules_close_interface((GF_BaseInterface *) ifce);
	}

	if (dec_ifce) {
		codec->decio = dec_ifce;
		sprintf(szPrefDec, "codec_%02X_%02X", esd->decoderConfig->streamType, esd->decoderConfig->objectTypeIndication);
		gf_cfg_set_key(term->user->config, "Systems", szPrefDec, dec_ifce->module_name);
		return GF_OK;
	}

	return GF_CODEC_NOT_FOUND;
}

GF_Err Codec_Load(GF_Codec *codec, GF_ESD *esd, u32 PL)
{
	switch (esd->decoderConfig->streamType) {
	/*OCR has no codec, just a channel*/
	case GF_STREAM_OCR:
		codec->decio = NULL;
		codec->process = gf_codec_process_ocr;
		return GF_OK;
#ifndef GPAC_DISABLE_VRML
	/*InteractionStream */
	case GF_STREAM_INTERACT:
		codec->decio = (GF_BaseDecoder *) gf_isdec_new(esd, PL);
		assert(codec->decio->InterfaceType == GF_SCENE_DECODER_INTERFACE);
		codec->process = SystemCodec_Process;
		return GF_OK;
#endif

	/*load decoder module*/
	case GF_STREAM_VISUAL:
	case GF_STREAM_AUDIO:
		if (!esd->decoderConfig->objectTypeIndication)
			return GF_NON_COMPLIANT_BITSTREAM;

		if (esd->decoderConfig->objectTypeIndication==GPAC_OTI_RAW_MEDIA_STREAM) {
			codec->flags |= GF_ESM_CODEC_IS_RAW_MEDIA;
			codec->process = gf_codec_process_private_media;
			return GF_OK;
		}
	default:
		return Codec_LoadModule(codec, esd, PL);
	}
}


void gf_codec_del(GF_Codec *codec)
{
	if (!codec || !codec->inChannels)
		return;
	if (gf_list_count(codec->inChannels)) return;

	if (!(codec->flags & GF_ESM_CODEC_IS_USE)) {
		switch (codec->type) {
			/*input sensor streams are handled internally for now*/
#ifndef GPAC_DISABLE_VRML
		case GF_STREAM_INTERACT:
			gf_mx_p(codec->odm->term->net_mx);
			gf_isdec_del(codec->decio);
			gf_list_del_item(codec->odm->term->input_streams, codec);
			gf_mx_v(codec->odm->term->net_mx);
			break;
#endif
		default:
			if (codec->decio)
				gf_modules_close_interface((GF_BaseInterface *) codec->decio);
			break;
		}
	}
	if (codec->CB) gf_cm_del(codec->CB);
	codec->CB = NULL;
	if (codec->inChannels) gf_list_del(codec->inChannels);
	codec->inChannels = NULL;
	gf_free(codec);
}



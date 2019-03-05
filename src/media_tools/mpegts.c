/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2005-2012
 *
 *  This file is part of GPAC / MPEG2-TS sub-project
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

#include <gpac/mpegts.h>


#ifndef GPAC_DISABLE_MPEG2TS

#include <string.h>
#include <gpac/constants.h>
#include <gpac/internal/media_dev.h>
#include <gpac/download.h>


#ifndef GPAC_DISABLE_STREAMING
#include <gpac/internal/ietf_dev.h>
#endif


#ifdef GPAC_CONFIG_LINUX
#include <unistd.h>
#endif

//#define FORCE_DISABLE_MPEG4SL_OVER_MPEG2TS

#ifdef GPAC_ENABLE_MPE
#include <gpac/dvb_mpe.h>
#endif

#ifdef GPAC_ENABLE_DSMCC
#include <gpac/ait.h>
#endif

#define DEBUG_TS_PACKET 0

GF_EXPORT
const char *gf_m2ts_get_stream_name(u32 streamType)
{
	switch (streamType) {
	case GF_M2TS_VIDEO_MPEG1:
		return "MPEG-1 Video";
	case GF_M2TS_VIDEO_MPEG2:
		return "MPEG-2 Video";
	case GF_M2TS_AUDIO_MPEG1:
		return "MPEG-1 Audio";
	case GF_M2TS_AUDIO_MPEG2:
		return "MPEG-2 Audio";
	case GF_M2TS_PRIVATE_SECTION:
		return "Private Section";
	case GF_M2TS_PRIVATE_DATA:
		return "Private Data";
	case GF_M2TS_AUDIO_AAC:
		return "AAC Audio";
	case GF_M2TS_VIDEO_MPEG4:
		return "MPEG-4 Video";
	case GF_M2TS_VIDEO_H264:
		return "MPEG-4/H264 Video";
	case GF_M2TS_VIDEO_SVC:
		return "H264-SVC Video";
	case GF_M2TS_VIDEO_HEVC:
		return "HEVC Video";
	case GF_M2TS_VIDEO_SHVC:
		return "SHVC Video";
	case GF_M2TS_VIDEO_SHVC_TEMPORAL:
		return "SHVC Video Temporal Sublayer";
	case GF_M2TS_VIDEO_MHVC:
		return "MHVC Video";
	case GF_M2TS_VIDEO_MHVC_TEMPORAL:
		return "MHVC Video Temporal Sublayer";

	case GF_M2TS_AUDIO_AC3:
		return "Dolby AC3 Audio";
	case GF_M2TS_AUDIO_DTS:
		return "Dolby DTS Audio";
	case GF_M2TS_SUBTITLE_DVB:
		return "DVB Subtitle";
	case GF_M2TS_SYSTEMS_MPEG4_PES:
		return "MPEG-4 SL (PES)";
	case GF_M2TS_SYSTEMS_MPEG4_SECTIONS:
		return "MPEG-4 SL (Section)";
	case GF_M2TS_MPE_SECTIONS:
		return "MPE (Section)";

	case GF_M2TS_METADATA_PES:
		return "Metadata (PES)";
	case GF_M2TS_METADATA_ID3_HLS:
		return "ID3/HLS Metadata (PES)";

	default:
		return "Unknown";
	}
}

static void gf_m2ts_estimate_duration(GF_M2TS_Demuxer *ts, u64 PCR, u16 pcr_pid)
{
	u64 file_size = 0;
//	if (ts->duration>0) return;

	if (ts->file || ts->file_size) {
		file_size = ts->file_size;
	} else if (ts->dnload) {
		u32 size;
		gf_dm_sess_get_stats(ts->dnload, NULL, NULL, &size, NULL, NULL, NULL);
		file_size = size;
	} else {
		return;
	}

	if (!ts->first_pcr_found) {
		ts->first_pcr_found = PCR;
		ts->pcr_pid = pcr_pid;
		ts->nb_pck_at_pcr = ts->nb_pck;
	} else if (ts->pcr_pid == pcr_pid) {
		if (PCR < ts->first_pcr_found) {
			ts->first_pcr_found = PCR;
			ts->pcr_pid = pcr_pid;
			ts->nb_pck_at_pcr = ts->nb_pck;
		} else if (PCR - ts->first_pcr_found > 2*27000000) {
			Bool changed = GF_FALSE;
			Double pck_dur = (Double) (PCR-ts->first_pcr_found);
			pck_dur /= (ts->nb_pck - ts->nb_pck_at_pcr);
			pck_dur /= 27000000;

			pck_dur *= file_size;
			pck_dur /= 188.0;
			if ((u32) ts->duration != (u32) pck_dur) {
				ts->duration = pck_dur;
				changed = GF_TRUE;
			}

			ts->first_pcr_found = PCR;
			ts->pcr_pid = pcr_pid;
			ts->nb_pck_at_pcr = ts->nb_pck;

			GF_LOG(GF_LOG_DEBUG, GF_LOG_CONTAINER, ("[MPEG-2 TS] Estimated duration based on instant bitrate: %g sec\n", ts->duration));

			if (ts->on_event && changed) {
				GF_M2TS_PES_PCK pck;
				memset(&pck, 0, sizeof(GF_M2TS_PES_PCK));
				pck.PTS = (u64) (ts->duration*1000);
				ts->on_event(ts, GF_M2TS_EVT_DURATION_ESTIMATED, &pck);
			}
		}
	}
}

static u32 gf_m2ts_reframe_default(GF_M2TS_Demuxer *ts, GF_M2TS_PES *pes, Bool same_pts, unsigned char *data, u32 data_len, GF_M2TS_PESHeader *pes_hdr)
{
	GF_M2TS_PES_PCK pck;
	pck.flags = 0;
	if (pes->rap) pck.flags |= GF_M2TS_PES_PCK_RAP;
	if (!same_pts) pck.flags |= GF_M2TS_PES_PCK_AU_START;
	pck.DTS = pes->DTS;
	pck.PTS = pes->PTS;
	pck.data = (char *)data;
	pck.data_len = data_len;
	pck.stream = pes;
	ts->on_event(ts, GF_M2TS_EVT_PES_PCK, &pck);
	/*we consumed all data*/
	return 0;
}

static u32 gf_m2ts_reframe_reset(GF_M2TS_Demuxer *ts, GF_M2TS_PES *pes, Bool same_pts, unsigned char *data, u32 data_len, GF_M2TS_PESHeader *pes_hdr)
{
	if (pes->pck_data) {
		gf_free(pes->pck_data);
		pes->pck_data = NULL;
	}
	pes->pck_data_len = pes->pck_alloc_len = 0;
	if (pes->prev_data) {
		gf_free(pes->prev_data);
		pes->prev_data = NULL;
	}
	pes->prev_data_len = 0;
	pes->pes_len = 0;
	pes->prev_PTS = 0;
	pes->reframe = NULL;
	pes->cc = -1;
	pes->temi_tc_desc_len = 0;
	return 0;
}


static u32 gf_m2ts_reframe_nalu_video(GF_M2TS_Demuxer *ts, GF_M2TS_PES *pes, Bool same_pts, unsigned char *data, u32 data_len, GF_M2TS_PESHeader *pes_hdr, Bool is_hevc)
{
	Bool au_start_in_pes=0;
	Bool prev_is_au_delim=0;
	Bool force_new_au=0;
	u32 start_code_found = 0;
	Bool short_start_code = 0;
	Bool esc_code_found = 0;
	u32 nal_type, sc_pos = 0;
	u32 first_nal_offset_in_pck = 0;
	Bool full_au_pes_mode = 0;
	u8 *au_start = NULL;
	GF_M2TS_PES_PCK pck;

	if (!same_pts)
		force_new_au = 1;

	/*dispatch frame*/
	pck.stream = pes;
	pck.DTS = pes->DTS;
	pck.PTS = pes->PTS;
	pck.flags = 0;

	while (sc_pos<data_len) {
		/* u32 sctype=0;*/
		unsigned char *start = (unsigned char *)memchr(data+sc_pos, 0, data_len-sc_pos);
		if (!start) break;
		sc_pos = (u32) (start - data);
		/*not enough space to test for start code, don't check it*/
		if (data_len - sc_pos < 5)
			break;

		/*0x00000001 start code*/
		if (!start[1] && !start[2] && (start[3]==1)) {
			short_start_code = 0;
		}
		/*0xXX000001 start code*/
		else if (!start[1] && (start[2]==1) && sc_pos && (data[sc_pos-1]!=0) ) {
			short_start_code = 1;
		}
		/*0x000000 escape code*/
		else if (!start[1] && !start[2]) {
			esc_code_found=1;
			if (!start_code_found) {
				sc_pos ++;
				continue;
			}
		} else {
			esc_code_found=0;
			sc_pos ++;
			continue;
		}

		if (!start_code_found) {
			if (sc_pos) {
				if (!esc_code_found) {
					pck.data = (char *)data;
					pck.data_len = sc_pos;
					pck.flags = 0;
					ts->on_event(ts, GF_M2TS_EVT_PES_PCK, &pck);
				}

				data += sc_pos;
				first_nal_offset_in_pck += sc_pos;
				data_len -= sc_pos;
			}
			start_code_found = short_start_code ? 2 : 1;
			sc_pos=1;
		} else {
			Bool is_short_start_code = (start_code_found==2) ? GF_TRUE : GF_FALSE;

			if (!full_au_pes_mode && (start_code_found==2)) {
				pck.data = (char *)data-1;
				pck.data[0]=0;
				pck.data_len = sc_pos+1;
			} else {
				pck.data = (char *)data;
				pck.data_len = sc_pos;
			}
			//copy start code type for next nal
			start_code_found = short_start_code ? 2 : 1;

			if (is_hevc) {
#ifndef GPAC_DISABLE_HEVC
				nal_type = (pck.data[ is_short_start_code ? 3 : 4 ] & 0x7E) >> 1;

				/*check for SPS and update stream info*/
#ifndef GPAC_DISABLE_AV_PARSERS
				if (!pes->vid_w && (nal_type==GF_HEVC_NALU_SEQ_PARAM)) {
					HEVCState hevc;
					s32 idx;
					memset(&hevc, 0, sizeof(HEVCState));
					hevc.sps_active_idx = -1;
					idx = gf_media_hevc_read_sps((char *)data+4, sc_pos-4, &hevc);

					if (idx>=0) {
						pes->vid_w = hevc.sps[idx].width;
						pes->vid_h = hevc.sps[idx].height;
					}
				}
#endif
				/*check AU start type - if this is an LHVC PID and the first nal is the first byte of the PES payload, consider this is an AU start*/
				if ((nal_type==GF_HEVC_NALU_ACCESS_UNIT) || (pes->depends_on_pid && !first_nal_offset_in_pck)) {
					if (!prev_is_au_delim) {
						//this was not a one AU per PES config, dispatch
						if (au_start) {
							pck.data = (char *)au_start;
							pck.data_len = (u32) (data - au_start);
							ts->on_event(ts, GF_M2TS_EVT_PES_PCK, &pck);
							au_start = NULL;
							full_au_pes_mode = 0;
							if (is_short_start_code) {
								pck.data = (char *)data-1;
								pck.data[0]=0;
								pck.data_len = sc_pos+1;
							} else {
								pck.data = (char *)data;
								pck.data_len = sc_pos;
							}
						}

						if (au_start_in_pes) {
							/*FIXME - we should check the AVC framerate to update the timing ...*/
							pck.DTS += 3000;
							pck.PTS += 3000;
							//						GF_LOG(GF_LOG_INFO, GF_LOG_CONTAINER, ("[MPEG-2 TS] PID%d: Two AVC AUs start in this PES packet - cannot recompute non-first AU timing\n", pes->pid));
						}

						pck.flags = GF_M2TS_PES_PCK_AU_START;
						force_new_au = 0;
						au_start_in_pes = 1;
						if (pes_hdr->data_alignment && !first_nal_offset_in_pck && !pes->single_nal_mode) {
							full_au_pes_mode = GF_TRUE;
							au_start = (u8 *) pck.data;
						} else {
							ts->on_event(ts, GF_M2TS_EVT_PES_PCK, &pck);
						}
						prev_is_au_delim=1;
					}
				} else if ((nal_type>=GF_HEVC_NALU_SLICE_BLA_W_LP) && (nal_type<=GF_HEVC_NALU_SLICE_CRA)) {
					if (!full_au_pes_mode) {
						pck.flags = GF_M2TS_PES_PCK_RAP;
						ts->on_event(ts, GF_M2TS_EVT_PES_PCK, &pck);
					} else {
						pck.flags |= GF_M2TS_PES_PCK_RAP;
					}
					prev_is_au_delim=0;
				}
				else
#endif //GPAC_DISABLE_HEVC
				{
					if (!full_au_pes_mode) {
						pck.flags = 0;
						ts->on_event(ts, GF_M2TS_EVT_PES_PCK, &pck);
					}
					prev_is_au_delim=0;
				}
			} else {
				nal_type = pck.data[is_short_start_code ? 3 : 4] & 0x1F;

				/*check for SPS and update stream info*/
#ifndef GPAC_DISABLE_AV_PARSERS
				if ((nal_type==GF_AVC_NALU_SEQ_PARAM) || (nal_type==GF_AVC_NALU_SVC_SUBSEQ_PARAM)) {
					AVCState avc;
					s32 idx;
					memset(&avc, 0, sizeof(AVCState));
					avc.sps_active_idx = -1;
					idx = gf_media_avc_read_sps((char *)data+4, sc_pos-4, &avc, 0, NULL);

					if ((idx>=0) && (pes->vid_w < avc.sps[idx].width) && (pes->vid_h < avc.sps[idx].height)) {
						pes->vid_w = avc.sps[idx].width;
						pes->vid_h = avc.sps[idx].height;
					}
				}
#endif
				/*check AU start type*/
				if ((nal_type==GF_AVC_NALU_ACCESS_UNIT) || (nal_type==GF_AVC_NALU_VDRD) ) {
					if (!prev_is_au_delim) {

						//this was not a one AU per PES config, dispatch
						if (au_start) {
							pck.data = (char *)au_start;
							pck.data_len = (u32) (data - au_start);
							ts->on_event(ts, GF_M2TS_EVT_PES_PCK, &pck);
							au_start = NULL;
							full_au_pes_mode = 0;
							if (is_short_start_code) {
								pck.data = (char *)data-1;
								pck.data[0]=0;
								pck.data_len = sc_pos+1;
							} else {
								pck.data = (char *)data;
								pck.data_len = sc_pos;
							}
						}

						if (au_start_in_pes) {
							/*FIXME - we should check the AVC framerate to update the timing ...*/
							pck.DTS += 3000;
							pck.PTS += 3000;
							//						GF_LOG(GF_LOG_INFO, GF_LOG_CONTAINER, ("[MPEG-2 TS] PID%d: Two AVC AUs start in this PES packet - cannot recompute non-first AU timing\n", pes->pid));
						}
						pck.flags = GF_M2TS_PES_PCK_AU_START;
						force_new_au = 0;
						au_start_in_pes = 1;
						if (pes_hdr->data_alignment && !first_nal_offset_in_pck && !pes->single_nal_mode) {
							full_au_pes_mode = GF_TRUE;
							au_start = (u8 *) pck.data;
						} else {
							ts->on_event(ts, GF_M2TS_EVT_PES_PCK, &pck);
						}
						prev_is_au_delim=1;
					}
				} else {
					if (!full_au_pes_mode) {
						pck.flags = (nal_type==GF_AVC_NALU_IDR_SLICE) ? GF_M2TS_PES_PCK_RAP : 0;
						ts->on_event(ts, GF_M2TS_EVT_PES_PCK, &pck);
					} else {
						if (nal_type==GF_AVC_NALU_IDR_SLICE) pck.flags |= GF_M2TS_PES_PCK_RAP;
					}
					prev_is_au_delim=0;
				}
			}

			data += sc_pos;
			data_len -= sc_pos;
			first_nal_offset_in_pck += sc_pos;
			sc_pos = 0;

			if (esc_code_found) {
				esc_code_found=0;
				start_code_found=0;
			} else {
				sc_pos = 1;
			}
		}
	}
	/*we did not consume all data*/
	if (!start_code_found) {
		u32 min_size = is_hevc ? 6 : 5;

		if (au_start) {
			pck.data = (char *)au_start;
			pck.data_len = (u32) (data - au_start);
			ts->on_event(ts, GF_M2TS_EVT_PES_PCK, &pck);
			au_start = NULL;
		}

		/*if not enough data to locate start code, store it*/
		if (data_len < min_size )
			return data_len;
		/*otherwise this is the middle of a frame, let's dispatch it*/
	}

	if (au_start) {
		u8 sc_end = ((data[0]==0) && (data[1]==0) && (data[2]==1)) ? 3 : 4;
		if (is_hevc) {
#ifndef GPAC_DISABLE_HEVC
			nal_type = (data[sc_end] & 0x7E) >> 1;
			if ((nal_type>=GF_HEVC_NALU_SLICE_BLA_W_LP) && (nal_type<=GF_HEVC_NALU_SLICE_CRA)) {
				pck.flags |= GF_M2TS_PES_PCK_RAP;
			}
#endif
		} else {
			nal_type = data[sc_end] & 0x1F;
			if (nal_type==GF_AVC_NALU_IDR_SLICE) pck.flags |= GF_M2TS_PES_PCK_RAP;
		}

		pck.data = (char *)au_start;
		pck.data_len = (u32) (data - au_start) + data_len;
		ts->on_event(ts, GF_M2TS_EVT_PES_PCK, &pck);
		return 0;
	}

	if (data_len) {
		pck.flags = 0;
		pck.data = (char *)data;
		pck.data_len = data_len;
		if (start_code_found) {
			if (start_code_found==2) {
				pck.data = (char *)data-1;
				pck.data[0]=0;
				pck.data_len = data_len+1;
			}
			if (is_hevc) {
#ifndef GPAC_DISABLE_HEVC
				nal_type = (pck.data[4] & 0x7E) >> 1;
				if (nal_type==GF_HEVC_NALU_ACCESS_UNIT) {
					pck.flags = GF_M2TS_PES_PCK_AU_START;
				} else if ((nal_type==GF_HEVC_NALU_SLICE_IDR_W_DLP) || (nal_type==GF_HEVC_NALU_SLICE_IDR_N_LP)) {
					pck.flags = GF_M2TS_PES_PCK_RAP;
				} else if (nal_type==GF_HEVC_NALU_FILLER_DATA) {
					return 0;
				}
#endif
			} else {
				nal_type = pck.data[4] & 0x1F;
				if (nal_type==GF_AVC_NALU_ACCESS_UNIT) {
					pck.flags = GF_M2TS_PES_PCK_AU_START;
				} else if (nal_type==GF_AVC_NALU_IDR_SLICE) {
					pck.flags = GF_M2TS_PES_PCK_RAP;
				}
			}
		}
		if (force_new_au) {
			pck.flags |= GF_M2TS_PES_PCK_AU_START;
			//force_new_au = 0;
		}
		ts->on_event(ts, GF_M2TS_EVT_PES_PCK, &pck);
	}
	/*we consumed all data*/
	return 0;
}

static u32 gf_m2ts_reframe_avc_h264(GF_M2TS_Demuxer *ts, GF_M2TS_PES *pes, Bool same_pts, unsigned char *data, u32 data_len, GF_M2TS_PESHeader *pes_hdr)
{
	return gf_m2ts_reframe_nalu_video(ts, pes, same_pts, data, data_len, pes_hdr, 0);
}

static u32 gf_m2ts_reframe_hevc(GF_M2TS_Demuxer *ts, GF_M2TS_PES *pes, Bool same_pts, unsigned char *data, u32 data_len, GF_M2TS_PESHeader *pes_hdr)
{
	return gf_m2ts_reframe_nalu_video(ts, pes, same_pts, data, data_len, pes_hdr, 1);
}

static u32 gf_m2ts_reframe_mpeg_video(GF_M2TS_Demuxer *ts, GF_M2TS_PES *pes, Bool same_pts, unsigned char *data, u32 data_len, GF_M2TS_PESHeader *pes_hdr)
{
	u32 sc_pos = 0;
	u32 to_send = data_len;
	GF_M2TS_PES_PCK pck;

	/*dispatch frame*/
	pck.stream = pes;
	pck.DTS = pes->DTS;
	pck.PTS = pes->PTS;
	pck.flags = 0;

	while (sc_pos+4<data_len) {
		unsigned char *start = (unsigned char*)memchr(data+sc_pos, 0, data_len-sc_pos);
		if (!start) break;
		sc_pos = (u32) (start - (unsigned char*)data);

		/*found picture or sequence start_code*/
		if (!start[1] && (start[2]==0x01)) {
			if (!start[3] || (start[3]==0xb3) || (start[3]==0xb8)) {
				Bool new_au;
				if (sc_pos) {
					pck.data = (char *)data;
					pck.data_len = sc_pos;
					ts->on_event(ts, GF_M2TS_EVT_PES_PCK, &pck);
					pck.flags = 0;
					data += sc_pos;
					data_len -= sc_pos;
					to_send -= sc_pos;
					sc_pos = 0;
				}
				new_au = 1;
				/*if prev was GOP/SEQ start, this is not a new AU*/
				if (pes->frame_state)
					new_au = 0;
				pes->frame_state = data[3];
				if (new_au) {
					pck.flags = GF_M2TS_PES_PCK_AU_START;
					if (pes->rap)
						pck.flags |= GF_M2TS_PES_PCK_RAP;
				}

				if (!pes->vid_h && (pes->frame_state==0xb3)) {
					u32 den, num;
					unsigned char *p = data+4;
					pes->vid_w = (p[0] << 4) | ((p[1] >> 4) & 0xf);
					pes->vid_h = ((p[1] & 0xf) << 8) | p[2];
					pes->vid_par = (p[3] >> 4) & 0xf;

					switch (pes->vid_par) {
					case 2:
						num = 4;
						den = 3;
						break;
					case 3:
						num = 16;
						den = 9;
						break;
					case 4:
						num = 221;
						den = 100;
						break;
					default:
						pes->vid_par = 0;
						den = num = 0;
						break;
					}
					if (den)
						pes->vid_par = ((pes->vid_h/den)<<16) | (pes->vid_w/num);
					break;
				}
				if (pes->frame_state==0x00) {
					switch ((data[5] >> 3) & 0x7) {
					case 1:
						pck.flags |= GF_M2TS_PES_PCK_I_FRAME;
						break;
					case 2:
						pck.flags |= GF_M2TS_PES_PCK_P_FRAME;
						break;
					case 3:
						pck.flags |= GF_M2TS_PES_PCK_B_FRAME;
						break;
					}
				}
			}
			sc_pos+=3;
		}
		sc_pos++;
	}
	pck.data = (char *)data;
	pck.data_len = data_len;
	ts->on_event(ts, GF_M2TS_EVT_PES_PCK, &pck);
	/*we consumed all data*/
	return 0;
}

#ifndef GPAC_DISABLE_AV_PARSERS

typedef struct
{
	Bool is_mp2, no_crc;
	u32 profile, sr_idx, nb_ch, frame_size;
} ADTSHeader;

static u32 gf_m2ts_reframe_aac_adts(GF_M2TS_Demuxer *ts, GF_M2TS_PES *pes, Bool same_pts, unsigned char *data, u32 data_len, GF_M2TS_PESHeader *pes_hdr)
{
	ADTSHeader hdr;
	u32 sc_pos = 0;
	u32 start = 0;
	u32 hdr_size = 0;
	u64 PTS;
	Bool first = 1;
	Bool garbage_bytes = 0;
	GF_M2TS_PES_PCK pck;

	/*dispatch frame*/
	PTS = pes->PTS;
	pck.stream = pes;
	pck.DTS = pes->DTS;
	pck.PTS = PTS;
	pck.flags = 0;

	if (pes->frame_state && ((pes->frame_state==data_len) || (((pes->frame_state+1<data_len)) && (data[pes->frame_state]==0xFF) && ((data[pes->frame_state+1] & 0xF0) == 0xF0)))) {
		assert(pes->frame_state<=data_len);
		/*dispatch frame*/
		pck.stream = pes;
		pck.DTS = PTS;
		pck.PTS = PTS;
		pck.flags = 0;
		pck.data = (char *)data;
		pck.data_len = pes->frame_state;
		ts->on_event(ts, GF_M2TS_EVT_PES_PCK, &pck);
		first = 0;
		start = sc_pos = pes->frame_state;
	}
	pes->frame_state = 0;

	/*fixme - we need to test this with more ADTS sources were PES framing is on any boundaries*/
	while (sc_pos+2<data_len) {
		u32 size;
		GF_BitStream *bs;
		/*look for sync marker 0xFFF0 on 12 bits*/
		if ((data[sc_pos]!=0xFF) || ((data[sc_pos+1] & 0xF0) != 0xF0)) {
			sc_pos++;
			continue;
		}

		if (garbage_bytes) {
			garbage_bytes = 0;
		}
		/*flush any pending data*/
		else if (start < sc_pos) {
			/*dispatch frame*/
			pck.stream = pes;
			pck.DTS = PTS;
			pck.PTS = PTS;
			pck.flags = 0;
			pck.data = (char *)data+start;
			pck.data_len = sc_pos-start;
			ts->on_event(ts, GF_M2TS_EVT_PES_PCK, &pck);
			if (pes->frame_state == pck.data_len) {
				/*consider we are sync*/
				first = 0;
			} else {
				first = 1;
			}
			pes->frame_state = 0;
		}
		/*not enough data to parse the frame header*/
		if (sc_pos + 7 >= data_len) {
			pes->frame_state = 0;
			pes->prev_PTS = PTS;
			return data_len-sc_pos;
		}

		bs = gf_bs_new((char *)data + sc_pos + 1, 9, GF_BITSTREAM_READ);
		gf_bs_read_int(bs, 4);
		hdr.is_mp2 = gf_bs_read_int(bs, 1);
		gf_bs_read_int(bs, 2);
		hdr.no_crc = gf_bs_read_int(bs, 1);

		hdr.profile = 1 + gf_bs_read_int(bs, 2);
		hdr.sr_idx = gf_bs_read_int(bs, 4);
		gf_bs_read_int(bs, 1);
		hdr.nb_ch = gf_bs_read_int(bs, 3);
		gf_bs_read_int(bs, 4);
		hdr.frame_size = gf_bs_read_int(bs, 13);
		gf_bs_read_int(bs, 11);
		gf_bs_read_int(bs, 2);
		if (!hdr.no_crc) gf_bs_read_int(bs, 16);
		hdr_size = hdr.no_crc ? 7 : 9;

		gf_bs_del(bs);

		/*make sure we are sync if we have more data following*/
		if (sc_pos + hdr.frame_size < data_len) {
			if ((hdr.frame_size < hdr_size) || (data[sc_pos + hdr.frame_size]!=0xFF) || ((data[sc_pos+hdr.frame_size+1] & 0xF0) != 0xF0)) {
				sc_pos++;
				continue;
			}
		} else if (first && (hdr.frame_size + sc_pos > data_len)) {
			sc_pos++;
			continue;
		}

		if (pes->aud_sr != GF_M4ASampleRates[hdr.sr_idx]) {
			GF_M4ADecSpecInfo cfg;
			pck.stream = pes;
			memset(&cfg, 0, sizeof(GF_M4ADecSpecInfo));
			cfg.base_object_type = hdr.profile;
			cfg.base_sr = GF_M4ASampleRates[hdr.sr_idx];


			if (!cfg.base_sr) {
				sc_pos++;
				garbage_bytes = 1;
				continue;
			}

			pes->aud_sr = cfg.base_sr;
			pes->aud_nb_ch = cfg.nb_chan = hdr.nb_ch;
			cfg.sbr_object_type = 0;
			gf_m4a_write_config(&cfg, &pck.data, &pck.data_len);
			ts->on_event(ts, GF_M2TS_EVT_AAC_CFG, &pck);
			gf_free(pck.data);
			pes->aud_aac_sr_idx = cfg.base_sr_index;
			pes->aud_sr = cfg.base_sr;
			pes->aud_nb_ch = cfg.nb_chan;
			pes->aud_aac_obj_type = hdr.profile;
		}

		/*dispatch frame*/
		pck.stream = pes;
		if (first && pes->prev_PTS) {
			pck.DTS = pck.PTS = pes->prev_PTS;
		} else {
			pck.DTS = pck.PTS = PTS;
		}
		pck.flags = GF_M2TS_PES_PCK_AU_START | GF_M2TS_PES_PCK_RAP;
		pck.data = (char *)data + sc_pos + hdr_size;
		pck.data_len = hdr.frame_size - hdr_size;

		if (pck.data_len > data_len - sc_pos - hdr_size) {
			assert(pck.data_len - (data_len - sc_pos - hdr_size) > 0);
			/*remember how much we have to send*/
			pes->frame_state = pck.data_len - (data_len - sc_pos - hdr_size);
			assert((s32) pes->frame_state > 0);
			pck.data_len = data_len - sc_pos - hdr_size;
		}

		ts->on_event(ts, GF_M2TS_EVT_PES_PCK, &pck);
		sc_pos += pck.data_len + hdr_size;
		start = sc_pos;

		if (first && pes->prev_PTS) {
			pes->prev_PTS = 0;
		}
		/*update PTS in case we don't get any update*/
		else if (pes->aud_sr) {
			size = 1024*90000/pes->aud_sr;
			PTS += size;
		}
		first = 0;
	}
	/*did we consumed all data ?*/
	if (data_len==sc_pos) return 0;
	pes->prev_PTS = PTS;
	return data_len - sc_pos;
}

static u32 gf_m2ts_reframe_aac_latm(GF_M2TS_Demuxer *ts, GF_M2TS_PES *pes, Bool same_pts, unsigned char *data, u32 data_len, GF_M2TS_PESHeader *pes_hdr)
{
	u32 sc_pos = 0;
	u32 start = 0;
	GF_M2TS_PES_PCK pck;

	/*dispatch frame*/
	pck.stream = pes;
	pck.DTS = pes->DTS;
	pck.PTS = pes->PTS;
	pck.flags = 0;


	if (data_len + pes->reassemble_len > pes->reassemble_alloc) {
		pes->reassemble_alloc = data_len + pes->reassemble_len;
		pes->reassemble_buf = gf_realloc(pes->reassemble_buf, sizeof(char)*pes->reassemble_alloc);
	}
	memcpy(pes->reassemble_buf + pes->reassemble_len, data, data_len);
	pes->reassemble_len += data_len;
	data_len = pes->reassemble_len;
	data = pes->reassemble_buf;


	/*fixme - we need to test this with more LATM sources were PES framing is on any boundaries*/
	while (sc_pos+2<data_len) {
		u32 size;
		u32 amux_len;
		GF_BitStream *bs;
		/*look for sync marker 0x2B7 on 11 bits*/
		if ((data[sc_pos]!=0x56) || ((data[sc_pos+1] & 0xE0) != 0xE0)) {
			sc_pos++;
			continue;
		}

		/*flush any pending data*/
		if (start < sc_pos) {
			assert(data_len >= sc_pos);
			memmove(pes->reassemble_buf, pes->reassemble_buf + sc_pos, data_len - sc_pos);
			pes->reassemble_len -= sc_pos;
			sc_pos = 0;
			data_len = pes->reassemble_len;
			data = pes->reassemble_buf;
		}

		/*found a start code*/
		amux_len = data[sc_pos+1] & 0x1F;
		amux_len <<= 8;
		amux_len |= data[sc_pos+2];
		if (!amux_len) {
			sc_pos+=3;
			assert(data_len >= sc_pos);
			memmove(pes->reassemble_buf, pes->reassemble_buf + sc_pos, data_len - sc_pos);
			pes->reassemble_len -= sc_pos;
			sc_pos = 0;
			data_len = pes->reassemble_len;
			data = pes->reassemble_buf;
			continue;
		}


		//we cannot check sync wait for next frame
		if (data_len < sc_pos+4+amux_len) {
			if (pes->aud_sr) {
				u32 tsinc = 1024*90000/pes->aud_sr;
				pes->PTS += tsinc;
			}
			return 0;
		}

		//we are not sync, trash what is before the start code and continue
		if ((data[sc_pos+3+amux_len] != 0x56) || ((data[sc_pos+3+amux_len + 1] & 0xE0) != 0xE0)) {
			sc_pos+=2;
			assert(data_len >= sc_pos);
			memmove(pes->reassemble_buf , pes->reassemble_buf+ sc_pos, data_len - sc_pos);
			pes->reassemble_len -= sc_pos;
			sc_pos = 0;
			data_len = pes->reassemble_len;
			data = pes->reassemble_buf;
			continue;
		}


		bs = gf_bs_new((char *)data+sc_pos+3, amux_len, GF_BITSTREAM_READ);

		/*use same stream mux*/
		if (!gf_bs_read_int(bs, 1)) {
			Bool amux_version, amux_versionA;

			amux_version = gf_bs_read_int(bs, 1);
			amux_versionA = 0;
			if (amux_version) amux_versionA = gf_bs_read_int(bs, 1);
			if (!amux_versionA) {
				u32 i, allStreamsSameTimeFraming, numProgram;
				if (amux_version) gf_latm_get_value(bs);

				allStreamsSameTimeFraming = gf_bs_read_int(bs, 1);
				/*numSubFrames = */gf_bs_read_int(bs, 6);
				numProgram = gf_bs_read_int(bs, 4);
				for (i=0; i<=numProgram; i++) {
					u32 j, num_lay;
					num_lay = gf_bs_read_int(bs, 3);
					for (j=0; j<=num_lay; j++) {
						GF_M4ADecSpecInfo cfg;
						u32 frameLengthType;
						Bool same_cfg = 0;
						if (i || j) same_cfg = gf_bs_read_int(bs, 1);

						if (!same_cfg) {
							if (amux_version==1) gf_latm_get_value(bs);
							gf_m4a_parse_config(bs, &cfg, 0);

							if (!pes->aud_sr) {
								pck.stream = pes;
								pes->aud_aac_sr_idx = cfg.base_sr_index;
								pes->aud_sr = cfg.base_sr;
								pes->aud_nb_ch = cfg.nb_chan;
								pes->aud_aac_obj_type = cfg.base_object_type;

								gf_m4a_write_config(&cfg, &pck.data, &pck.data_len);
								ts->on_event(ts, GF_M2TS_EVT_AAC_CFG, &pck);
								gf_free(pck.data);
							}
						}
						frameLengthType = gf_bs_read_int(bs, 3);
						if (!frameLengthType) {
							/*latmBufferFullness = */gf_bs_read_int(bs, 8);
							if (!allStreamsSameTimeFraming) {
							}
						} else {
							/*not supported*/
						}
					}

				}
				/*other data present*/
				if (gf_bs_read_int(bs, 1)) {
//					u32 k = 0;
				}
				/*CRCcheck present*/
				if (gf_bs_read_int(bs, 1)) {
				}
			}


		}

		/*we have a cfg, read data - we only handle single stream multiplex in LATM/LOAS*/
		if (pes->aud_sr) {
			size = 0;
			while (1) {
				u32 tmp = gf_bs_read_int(bs, 8);
				size += tmp;
				if (tmp!=255) break;
			}
			if (size>pes->buf_len) {
				pes->buf_len = size;
				pes->buf = gf_realloc(pes->buf, sizeof(char)*pes->buf_len);
			}
			gf_bs_read_data(bs, (char *)pes->buf, size);

			/*dispatch frame*/
			pck.stream = pes;
			pck.DTS = pes->PTS;
			pck.PTS = pes->PTS;
			pck.flags = GF_M2TS_PES_PCK_AU_START | GF_M2TS_PES_PCK_RAP;
			pck.data = (char *)pes->buf;
			pck.data_len = size;

			ts->on_event(ts, GF_M2TS_EVT_PES_PCK, &pck);
		}
		gf_bs_del(bs);

		//trash dispatched AU, including 3 bytes header
		sc_pos += amux_len+3;
		assert(data_len >= sc_pos);
		memmove(pes->reassemble_buf, pes->reassemble_buf + sc_pos, data_len - sc_pos);
		pes->reassemble_len -= sc_pos;
		sc_pos = 0;
		data_len = pes->reassemble_len;
		data = pes->reassemble_buf;

		if ((data_len>3) && pes->aud_sr) {
			/*update PTS in case we don't get any update*/
			size = 1024*90000/pes->aud_sr;
			pes->PTS += size;
		}
	}
	/*we consumed all data*/
	return 0;
}
#endif


#ifndef GPAC_DISABLE_AV_PARSERS
static u32 gf_m2ts_reframe_mpeg_audio(GF_M2TS_Demuxer *ts, GF_M2TS_PES *pes, Bool same_pts, unsigned char *data, u32 data_len, GF_M2TS_PESHeader *pes_hdr)
{
	GF_M2TS_PES_PCK pck;
	u32 pos, frame_size, remain;
	u64 PTS;
	u32 next_hdr = 0;

	pck.flags = GF_M2TS_PES_PCK_RAP;
	pck.stream = pes;
	remain = pes->frame_state;
	PTS = pes->PTS;

	/*check we don't find a sync point within what we should flush - this happens with looping TS streams and other nasty scenarii*/
	next_hdr = gf_mp3_get_next_header_mem((char *)data, data_len, &pos);
	if (next_hdr && remain && (pos<remain) ) {
		//only resync if the frame header matches our current config, otherwise this is likely an emulated header
		if ((pes->aud_sr == gf_mp3_sampling_rate(pes->frame_state) )
		        && (pes->aud_nb_ch == gf_mp3_num_channels(pes->frame_state))
		   ) {
			remain = pos;
		}
	}

	if (remain) {
		/*dispatch end of prev frame*/
		pck.DTS = pck.PTS = PTS;
		pck.data = (char *)data;
		pck.data_len = (remain>data_len) ? data_len : remain;
		ts->on_event(ts, GF_M2TS_EVT_PES_PCK, &pck);
		if (remain>data_len) {
			pes->frame_state = remain - data_len;
			/*we consumed all data*/
			return 0;
		}
		data += remain;
		data_len -= remain;
	}

	pes->frame_state = gf_mp3_get_next_header_mem((char *)data, data_len, &pos);
	if (!pes->frame_state) {
		/*we did not consumed all data*/
		return data_len;
	}
	assert((pes->frame_state & 0xffe00000) == 0xffe00000);

	if (!pes->aud_sr || !pes->aud_nb_ch) {
		pes->aud_sr = gf_mp3_sampling_rate(pes->frame_state);
		pes->aud_nb_ch = gf_mp3_num_channels(pes->frame_state);
	}
	pck.flags = GF_M2TS_PES_PCK_RAP | GF_M2TS_PES_PCK_AU_START;

	frame_size = gf_mp3_frame_size(pes->frame_state);
	while (frame_size <= data_len) {
		/*dispatch frame*/
		pck.DTS = pck.PTS = PTS;
		pck.data = (char *)data;
		pck.data_len = frame_size;
		ts->on_event(ts, GF_M2TS_EVT_PES_PCK, &pck);

		PTS += gf_mp3_window_size(pes->frame_state)*90000/gf_mp3_sampling_rate(pes->frame_state);
		/*move frame*/
		data += frame_size;
		data_len -= frame_size;
		if (!data_len) break;
		pes->frame_state = gf_mp3_get_next_header_mem((char *)data, data_len, &pos);
		/*resync (ID3 or error)*/
		if (!pes->frame_state) {
			/*we did not consumed all data*/
			return data_len;
		}
		/*resync (ID3 or error)*/
		if (pos) {
			data_len -= pos;
			data += pos;
		}
		frame_size = gf_mp3_frame_size(pes->frame_state);
	}
	if (data_len) {
		pck.DTS = pck.PTS = PTS;
		pck.data = (char *)data;
		pck.data_len = data_len;
		ts->on_event(ts, GF_M2TS_EVT_PES_PCK, &pck);
		/*update PTS in case we don't get any update*/
		pes->PTS += gf_mp3_window_size(pes->frame_state)*90000/gf_mp3_sampling_rate(pes->frame_state);
		pes->frame_state = frame_size - data_len;
	} else  {
		pes->frame_state = 0;
	}
	/*we consumed all data*/
	return 0;
}

#endif /*GPAC_DISABLE_AV_PARSERS*/


#ifndef GPAC_DISABLE_AV_PARSERS
static u32 gf_m2ts_reframe_ac3(GF_M2TS_Demuxer *ts, GF_M2TS_PES *pes, Bool same_pts, unsigned char *data, u32 data_len, GF_M2TS_PESHeader *pes_hdr)
{
	GF_M2TS_PES_PCK pck;
	pck.flags = 0;
	if (pes->rap) pck.flags |= GF_M2TS_PES_PCK_RAP;
	if (!same_pts) pck.flags |= GF_M2TS_PES_PCK_AU_START;
	pck.DTS = pes->DTS;
	pck.PTS = pes->PTS;

	if (!pes->aud_sr) {
		GF_AC3Header hdr;
		u32 pos;
		if (gf_ac3_parser(data, data_len, &pos, &hdr, GF_TRUE)) {
			pes->aud_sr = hdr.sample_rate;
			pes->aud_nb_ch = hdr.channels;
		}
	}

	pck.data = (char *)data;
	pck.data_len = data_len;
	pck.stream = pes;
	ts->on_event(ts, GF_M2TS_EVT_PES_PCK, &pck);
	/*we consumed all data*/
	return 0;
}

static u32 gf_m2ts_reframe_ec3(GF_M2TS_Demuxer *ts, GF_M2TS_PES *pes, Bool same_pts, unsigned char *data, u32 data_len, GF_M2TS_PESHeader *pes_hdr)
{
	GF_M2TS_PES_PCK pck;
	pck.flags = 0;
	if (pes->rap) pck.flags |= GF_M2TS_PES_PCK_RAP;
	if (!same_pts) pck.flags |= GF_M2TS_PES_PCK_AU_START;
	pck.DTS = pes->DTS;
	pck.PTS = pes->PTS;

	if (!pes->aud_sr) {
		GF_AC3Header hdr;
		GF_BitStream *bs = gf_bs_new((const char *) data, data_len, GF_BITSTREAM_READ);
		if (gf_eac3_parser_bs(bs, &hdr, GF_TRUE)) {
			pes->aud_sr = hdr.sample_rate;
			pes->aud_nb_ch = hdr.channels;
		}
		gf_bs_del(bs);
	}

	pck.data = (char *)data;
	pck.data_len = data_len;
	pck.stream = pes;
	ts->on_event(ts, GF_M2TS_EVT_PES_PCK, &pck);
	/*we consumed all data*/
	return 0;
}

#endif /*GPAC_DISABLE_AV_PARSERS*/

static void add_text(char **buffer, u32 *size, u32 *pos, char *msg, u32 msg_len)
{
	if (!msg || !buffer || ! *buffer) return;

	if (*pos+msg_len>*size) {
		*size = *pos+msg_len-*size+256;
		*buffer = (char *)gf_realloc(*buffer, *size);
	}
	strncpy((*buffer)+(*pos), msg, msg_len);
	*pos += msg_len;
}

static GF_Err id3_parse_tag(char *data, u32 length, char **output, u32 *output_size, u32 *output_pos)
{
	u32 size;
	u32 pos = 0;
	/* ID3VVFFFFSIZE = 13bytes
	     * ID3 string
	 * VV = Version
	     * F = Flags
	     * SIZE = 32bits size with first Most Significant bit set to 0 -> 28 bits
	 * Size starts AFTER this header, meaning we have to add 10 bytes
	 */
	if (data[pos] == 'I' && data[pos+1] == 'D' && data[pos+2] == '3') {
		//u16 version = (data[pos+3]<<8)+data[pos+4];
		//Bool unsync_flag = ((data[pos+5]>>7 & 0x1) ? GF_TRUE: GF_FALSE);
		Bool extended_header_flag = ((data[pos+5]>>6 & 0x1) ? GF_TRUE: GF_FALSE);
		//Bool experimental_flag = ((data[pos+5]>>5 & 0x1) ? GF_TRUE: GF_FALSE);
		if (data[pos+5] & 0x1F) {
			return GF_NOT_SUPPORTED;
		} else {
			size = 10 + ((data[pos+9] & 0x7f) + ((data[pos+8] & 0x7f) << 7) + ((data[pos+7] & 0x7f) << 14) + ((data[pos+6] & 0x7f) << 21));
		}
		pos += 10;

		if (extended_header_flag) {
			u32 extended_size = 4 + ((data[pos]) + ((data[pos+1]) << 8) + ((data[pos+2]) << 16) + ((data[pos+3]) << 24));
			pos += extended_size;
		}

		while (pos < size) {
			GF_ID3v2FrameType type;
			u32 frame_size, frame_pos;
			Bool compression_flag, encryption_flag/*, tag_alter_preservation_flag, file_alter_preservation_flag, readonly_flag, grouping_flag*/;
			frame_pos = pos;
			/* parsing a frame */
			type = (GF_ID3v2FrameType)(((data[pos+3]) + ((data[pos+2]) << 8) + ((data[pos+1]) << 16) + ((data[pos]) << 24)));
			pos+=4;
			frame_size = 10 + ((data[pos+3] & 0x7f) + ((data[pos+2] & 0x7f) << 7) + ((data[pos+1] & 0x7f) << 14) + ((data[pos] & 0x7f) << 21));
			pos+=4;
			//tag_alter_preservation_flag = ((data[pos]>>7 & 0x1) ? GF_TRUE: GF_FALSE);
			//file_alter_preservation_flag = ((data[pos]>>6 & 0x1) ? GF_TRUE: GF_FALSE);
			//readonly_flag = ((data[pos]>>5 & 0x1) ? GF_TRUE: GF_FALSE);
			pos++;
			compression_flag = ((data[pos]>>7 & 0x1) ? GF_TRUE: GF_FALSE);
			encryption_flag = ((data[pos]>>6 & 0x1) ? GF_TRUE: GF_FALSE);
			//grouping_flag = ((data[pos]>>5 & 0x1) ? GF_TRUE: GF_FALSE);
			if (compression_flag || encryption_flag) {
				/* unsupported, skip */
				pos = frame_pos + frame_size;
			} else {
				switch (type) {
				case ID3V2_FRAME_TXXX:
					add_text(output, output_size, output_pos, data+pos+3, frame_size-10-3);
					pos = frame_pos + frame_size;
					break;
				default:
					/* unsupported, skip */
					pos = frame_pos + frame_size;
				}
			}
		}

		return GF_OK;
	} else {
		return GF_NOT_SUPPORTED;
	}
}

static u32 gf_m2ts_reframe_id3_pes(GF_M2TS_Demuxer *ts, GF_M2TS_PES *pes, Bool same_pts, unsigned char *data, u32 data_len, GF_M2TS_PESHeader *pes_hdr)
{
	char frame_header[256];
	char *output_text = NULL;
	u32 output_len = 0;
	u32 pos = 0;
	GF_M2TS_PES_PCK pck;
	pck.flags = 0;
	if (pes->rap) pck.flags |= GF_M2TS_PES_PCK_RAP;
	if (!same_pts) pck.flags |= GF_M2TS_PES_PCK_AU_START;
	pck.DTS = pes->DTS;
	pck.PTS = pes->PTS;
	sprintf(frame_header, LLU" --> NEXT\n", pes->PTS);
	add_text(&output_text, &output_len, &pos, frame_header, (u32)strlen(frame_header));
	id3_parse_tag((char *)data, data_len, &output_text, &output_len, &pos);
	add_text(&output_text, &output_len, &pos, "\n\n", 2);
	pck.data = (char *)output_text;
	pck.data_len = pos;
	pck.stream = pes;
	ts->on_event(ts, GF_M2TS_EVT_PES_PCK, &pck);
	gf_free(output_text);
	/*we consumed all data*/
	return 0;
}

static u32 gf_m2ts_sync(GF_M2TS_Demuxer *ts, Bool simple_check)
{
	u32 i=0;
	/*if first byte is sync assume we're sync*/
	if (simple_check && (ts->buffer[i]==0x47)) return 0;

	while (i<ts->buffer_size) {
		if (i+188>ts->buffer_size) return ts->buffer_size;
		if ((ts->buffer[i]==0x47) && (ts->buffer[i+188]==0x47))
			break;
		if ((ts->buffer[i]==0x47) && (ts->buffer[i+192]==0x47)) {
			ts->prefix_present = 1;
			break;
		}
		i++;
	}
	if (i) {
		GF_LOG(GF_LOG_DEBUG, GF_LOG_CONTAINER, ("[MPEG-2 TS] re-sync skipped %d bytes\n", i) );
	}
	return i;
}

GF_EXPORT
Bool gf_m2ts_crc32_check(char *data, u32 len)
{
	u32 crc = gf_crc_32(data, len);
	u32 crc_val = GF_4CC((u8) data[len], (u8) data[len+1], (u8) data[len+2], (u8) data[len+3]);
	return (crc==crc_val) ? GF_TRUE : GF_FALSE;
}


static GF_M2TS_SectionFilter *gf_m2ts_section_filter_new(gf_m2ts_section_callback process_section_callback, Bool process_individual)
{
	GF_M2TS_SectionFilter *sec;
	GF_SAFEALLOC(sec, GF_M2TS_SectionFilter);
	if (!sec) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[MPEG-2 TS] gf_m2ts_section_filter_new : OUT OF MEMORY\n"));
		return NULL;
	}
	sec->cc = -1;
	sec->process_section = process_section_callback;
	sec->process_individual = process_individual;
	return sec;
}

static void gf_m2ts_reset_sections(GF_List *sections)
{
	u32 count;
	GF_M2TS_Section *section;
	//GF_LOG(GF_LOG_DEBUG, GF_LOG_CONTAINER, ("[MPEG-2 TS] Deleting sections\n"));

	count = gf_list_count(sections);
	while (count) {
		section = gf_list_get(sections, 0);
		gf_list_rem(sections, 0);
		if (section->data) gf_free(section->data);
		gf_free(section);
		count--;
	}
}

static void gf_m2ts_section_filter_reset(GF_M2TS_SectionFilter *sf)
{
	if (sf->section) {
		gf_free(sf->section);
		sf->section = NULL;
	}
	while (sf->table) {
		GF_M2TS_Table *t = sf->table;
		sf->table = t->next;
		gf_m2ts_reset_sections(t->sections);
		gf_list_del(t->sections);
		gf_free(t);
	}
	sf->cc = -1;
	sf->length = sf->received = 0;
	sf->demux_restarted = 1;

}
static void gf_m2ts_section_filter_del(GF_M2TS_SectionFilter *sf)
{
	gf_m2ts_section_filter_reset(sf);
	gf_free(sf);
}

GF_EXPORT
void gf_m2ts_es_del(GF_M2TS_ES *es, GF_M2TS_Demuxer *ts)
{
	gf_list_del_item(es->program->streams, es);

	if (es->flags & GF_M2TS_ES_IS_SECTION) {
		GF_M2TS_SECTION_ES *ses = (GF_M2TS_SECTION_ES *)es;
		if (ses->sec) gf_m2ts_section_filter_del(ses->sec);

#ifdef GPAC_ENABLE_MPE
		if (es->flags & GF_M2TS_ES_IS_MPE)
			gf_dvb_mpe_section_del(es);
#endif

	} else if (es->pid!=es->program->pmt_pid) {
		GF_M2TS_PES *pes = (GF_M2TS_PES *)es;

		if ((pes->flags & GF_M2TS_INHERIT_PCR) && ts->ess[es->program->pcr_pid]==es)
			ts->ess[es->program->pcr_pid] = NULL;

		if (pes->pck_data) gf_free(pes->pck_data);
		if (pes->prev_data) gf_free(pes->prev_data);
		if (pes->buf) gf_free(pes->buf);
		if (pes->reassemble_buf) gf_free(pes->reassemble_buf);
		if (pes->temi_tc_desc) gf_free(pes->temi_tc_desc);
	}
	if (es->slcfg) gf_free(es->slcfg);
	gf_free(es);
}

static void gf_m2ts_reset_sdt(GF_M2TS_Demuxer *ts)
{
	while (gf_list_count(ts->SDTs)) {
		GF_M2TS_SDT *sdt = (GF_M2TS_SDT *)gf_list_last(ts->SDTs);
		gf_list_rem_last(ts->SDTs);
		if (sdt->provider) gf_free(sdt->provider);
		if (sdt->service) gf_free(sdt->service);
		gf_free(sdt);
	}
}

GF_EXPORT
GF_M2TS_SDT *gf_m2ts_get_sdt_info(GF_M2TS_Demuxer *ts, u32 program_id)
{
	u32 i;
	for (i=0; i<gf_list_count(ts->SDTs); i++) {
		GF_M2TS_SDT *sdt = (GF_M2TS_SDT *)gf_list_get(ts->SDTs, i);
		if (sdt->service_id==program_id) return sdt;
	}
	return NULL;
}

static void gf_m2ts_section_complete(GF_M2TS_Demuxer *ts, GF_M2TS_SectionFilter *sec, GF_M2TS_SECTION_ES *ses)
{
	//seek mode, only process PAT and PMT
	if (ts->start_range && (sec->section[0] != GF_M2TS_TABLE_ID_PAT) && (sec->section[0] != GF_M2TS_TABLE_ID_PMT)) {
		/*clean-up (including broken sections)*/
		if (sec->section) gf_free(sec->section);
		sec->section = NULL;
		sec->length = sec->received = 0;
		return;
	}

	if (!sec->process_section) {
		if ((ts->on_event && (sec->section[0]==GF_M2TS_TABLE_ID_AIT)) ) {
#ifdef GPAC_ENABLE_DSMCC
			GF_M2TS_SL_PCK pck;
			pck.data_len = sec->length;
			pck.data = sec->section;
			pck.stream = (GF_M2TS_ES *)ses;
			//ts->on_event(ts, GF_M2TS_EVT_AIT_FOUND, &pck);
			on_ait_section(ts, GF_M2TS_EVT_AIT_FOUND, &pck);
#endif
		} else if ((ts->on_event && (sec->section[0]==GF_M2TS_TABLE_ID_DSM_CC_ENCAPSULATED_DATA	|| sec->section[0]==GF_M2TS_TABLE_ID_DSM_CC_UN_MESSAGE ||
		                             sec->section[0]==GF_M2TS_TABLE_ID_DSM_CC_DOWNLOAD_DATA_MESSAGE || sec->section[0]==GF_M2TS_TABLE_ID_DSM_CC_STREAM_DESCRIPTION || sec->section[0]==GF_M2TS_TABLE_ID_DSM_CC_PRIVATE)) ) {

#ifdef GPAC_ENABLE_DSMCC
			GF_M2TS_SL_PCK pck;
			pck.data_len = sec->length;
			pck.data = sec->section;
			pck.stream = (GF_M2TS_ES *)ses;
			on_dsmcc_section(ts,GF_M2TS_EVT_DSMCC_FOUND,&pck);
			//ts->on_event(ts, GF_M2TS_EVT_DSMCC_FOUND, &pck);
#endif
		}
#ifdef GPAC_ENABLE_MPE
		else if (ts->on_mpe_event && ((ses && (ses->flags & GF_M2TS_EVT_DVB_MPE)) || (sec->section[0]==GF_M2TS_TABLE_ID_INT)) ) {
			GF_M2TS_SL_PCK pck;
			pck.data_len = sec->length;
			pck.data = sec->section;
			pck.stream = (GF_M2TS_ES *)ses;
			ts->on_mpe_event(ts, GF_M2TS_EVT_DVB_MPE, &pck);
		}
#endif
		else if (ts->on_event) {
			GF_M2TS_SL_PCK pck;
			pck.data_len = sec->length;
			pck.data = sec->section;
			pck.stream = (GF_M2TS_ES *)ses;
			ts->on_event(ts, GF_M2TS_EVT_DVB_GENERAL, &pck);
		}
	} else {
		Bool has_syntax_indicator;
		u8 table_id;
		u16 extended_table_id;
		u32 status, section_start, i;
		GF_M2TS_Table *t, *prev_t;
		unsigned char *data;
		Bool section_valid = 0;

		status = 0;
		/*parse header*/
		data = (u8 *)sec->section;

		/*look for proper table*/
		table_id = data[0];

		if (ts->on_event) {
			switch (table_id) {
			case GF_M2TS_TABLE_ID_PAT:
			case GF_M2TS_TABLE_ID_SDT_ACTUAL:
			case GF_M2TS_TABLE_ID_PMT:
			case GF_M2TS_TABLE_ID_NIT_ACTUAL:
			case GF_M2TS_TABLE_ID_TDT:
			case GF_M2TS_TABLE_ID_TOT:
			{
				GF_M2TS_SL_PCK pck;
				pck.data_len = sec->length;
				pck.data = sec->section;
				pck.stream = (GF_M2TS_ES *)ses;
				ts->on_event(ts, GF_M2TS_EVT_DVB_GENERAL, &pck);
			}
			}
		}

		has_syntax_indicator = (data[1] & 0x80) ? 1 : 0;
		if (has_syntax_indicator) {
			extended_table_id = (data[3]<<8) | data[4];
		} else {
			extended_table_id = 0;
		}

		prev_t = NULL;
		t = sec->table;
		while (t) {
			if ((t->table_id==table_id) && (t->ex_table_id == extended_table_id)) break;
			prev_t = t;
			t = t->next;
		}

		/*create table*/
		if (!t) {
			GF_SAFEALLOC(t, GF_M2TS_Table);
			if (!t) {
				GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[MPEG-2 TS] Fail to alloc table %d %d\n", table_id, extended_table_id));
				return;
			}
			GF_LOG(GF_LOG_DEBUG, GF_LOG_CONTAINER, ("[MPEG-2 TS] Creating table %d %d\n", table_id, extended_table_id));
			t->table_id = table_id;
			t->ex_table_id = extended_table_id;
			t->last_version_number = 0xFF;
			t->sections = gf_list_new();
			if (prev_t) prev_t->next = t;
			else sec->table = t;
		}

		if (has_syntax_indicator) {
			if (sec->length < 4) {
				GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[MPEG-2 TS] corrupted section length %d less than CRC \n", sec->length));
			} else {
				/*remove crc32*/
				sec->length -= 4;
				if (gf_m2ts_crc32_check((char *)data, sec->length)) {
					s32 cur_sec_num;
					t->version_number = (data[5] >> 1) & 0x1f;
					if (t->last_section_number && t->section_number && (t->version_number != t->last_version_number)) {
						GF_LOG(GF_LOG_WARNING, GF_LOG_CONTAINER, ("[MPEG-2 TS] table transmission interrupted: previous table (v=%d) %d/%d sections - new table (v=%d) %d/%d sections\n", t->last_version_number, t->section_number, t->last_section_number, t->version_number, data[6] + 1, data[7] + 1) );
						gf_m2ts_reset_sections(t->sections);
						t->section_number = 0;
					}

					t->current_next_indicator = (data[5] & 0x1) ? 1 : 0;
					/*add one to section numbers to detect if we missed or not the first section in the table*/
					cur_sec_num = data[6] + 1;
					t->last_section_number = data[7] + 1;
					section_start = 8;
					/*we missed something*/
					if (!sec->process_individual && t->section_number + 1 != cur_sec_num) {
						/* TODO - Check how to handle sections when the first complete section does
						   not have its sec num 0 */
						section_valid = 0;
						if (t->is_init) {
							GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[MPEG-2 TS] corrupted table (lost section %d)\n", cur_sec_num ? cur_sec_num-1 : 31) );
						}
					} else {
						section_valid = 1;
						t->section_number = cur_sec_num;
					}
				} else {
					GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[MPEG-2 TS] corrupted section (CRC32 failed)\n"));
				}
			}
		} else {
			section_valid = 1;
			section_start = 3;
		}
		/*process section*/
		if (section_valid) {
			GF_M2TS_Section *section;

			GF_SAFEALLOC(section, GF_M2TS_Section);
			if (!section) {
				GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[MPEG-2 TS] Fail to create section\n"));
				return;
			}
			section->data_size = sec->length - section_start;
			section->data = (unsigned char*)gf_malloc(sizeof(unsigned char)*section->data_size);
			memcpy(section->data, sec->section + section_start, sizeof(unsigned char)*section->data_size);
			gf_list_add(t->sections, section);

			if (t->section_number == 1) {
				status |= GF_M2TS_TABLE_START;
				if (t->last_version_number == t->version_number) {
					t->is_repeat = 1;
				} else {
					t->is_repeat = 0;
				}
				/*only update version number in the first section of the table*/
				t->last_version_number = t->version_number;
			}

			if (t->is_init) {
				if (t->is_repeat) {
					status |=  GF_M2TS_TABLE_REPEAT;
				} else {
					status |=  GF_M2TS_TABLE_UPDATE;
				}
			} else {
				status |=  GF_M2TS_TABLE_FOUND;
			}

			if (t->last_section_number == t->section_number) {
				u32 table_size;

				status |= GF_M2TS_TABLE_END;

				table_size = 0;
				for (i=0; i<gf_list_count(t->sections); i++) {
					GF_M2TS_Section *section = gf_list_get(t->sections, i);
					table_size += section->data_size;
				}
				if (t->is_repeat) {
					if (t->table_size != table_size) {
						status |= GF_M2TS_TABLE_UPDATE;
						GF_LOG(GF_LOG_WARNING, GF_LOG_CONTAINER, ("[MPEG-2 TS] Repeated section found with different sizes (old table %d bytes, new table %d bytes)\n", t->table_size, table_size) );

						t->table_size = table_size;
					}
				} else {
					t->table_size = table_size;
				}

				t->is_init = 1;
				/*reset section number*/
				t->section_number = 0;

				t->is_repeat = 0;

			}

			if (sec->process_individual) {
				/*send each section of the table and not the aggregated table*/
				sec->process_section(ts, ses, t->sections, t->table_id, t->ex_table_id, t->version_number, (u8) (t->last_section_number - 1), status);
				gf_m2ts_reset_sections(t->sections);
			} else {
				if (status&GF_M2TS_TABLE_END) {
					sec->process_section(ts, ses, t->sections, t->table_id, t->ex_table_id, t->version_number, (u8) (t->last_section_number - 1), status);
					gf_m2ts_reset_sections(t->sections);
				}
			}

		} else {
			sec->cc = -1;
			t->section_number = 0;
		}
	}
	/*clean-up (including broken sections)*/
	if (sec->section) gf_free(sec->section);
	sec->section = NULL;
	sec->length = sec->received = 0;
}

static Bool gf_m2ts_is_long_section(u8 table_id)
{
	switch (table_id) {
	case GF_M2TS_TABLE_ID_MPEG4_BIFS:
	case GF_M2TS_TABLE_ID_MPEG4_OD:
	case GF_M2TS_TABLE_ID_INT:
	case GF_M2TS_TABLE_ID_EIT_ACTUAL_PF:
	case GF_M2TS_TABLE_ID_EIT_OTHER_PF:
	case GF_M2TS_TABLE_ID_ST:
	case GF_M2TS_TABLE_ID_SIT:
	case GF_M2TS_TABLE_ID_DSM_CC_PRIVATE:
	case GF_M2TS_TABLE_ID_MPE_FEC:
	case GF_M2TS_TABLE_ID_DSM_CC_DOWNLOAD_DATA_MESSAGE:
	case GF_M2TS_TABLE_ID_DSM_CC_UN_MESSAGE:
		return 1;
	default:
		if (table_id >= GF_M2TS_TABLE_ID_EIT_SCHEDULE_MIN && table_id <= GF_M2TS_TABLE_ID_EIT_SCHEDULE_MAX)
			return 1;
		else
			return 0;
	}
}

static u32 gf_m2ts_get_section_length(char byte0, char byte1, char byte2)
{
	u32 length;
	if (gf_m2ts_is_long_section(byte0)) {
		length = 3 + ( ((byte1<<8) | (byte2&0xff)) & 0xfff );
	} else {
		length = 3 + ( ((byte1<<8) | (byte2&0xff)) & 0x3ff );
	}
	return length;
}

static void gf_m2ts_gather_section(GF_M2TS_Demuxer *ts, GF_M2TS_SectionFilter *sec, GF_M2TS_SECTION_ES *ses, GF_M2TS_Header *hdr, unsigned char *data, u32 data_size)
{
	u32 payload_size = data_size;
	u8 expect_cc = (sec->cc<0) ? hdr->continuity_counter : (sec->cc + 1) & 0xf;
	Bool disc = (expect_cc == hdr->continuity_counter) ? 0 : 1;
	sec->cc = expect_cc;

	/*may happen if hdr->adaptation_field=2 no payload in TS packet*/
	if (!data_size) return;

	if (hdr->payload_start) {
		u32 ptr_field;

		ptr_field = data[0];
		if (ptr_field+1>data_size) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[MPEG-2 TS] Invalid section start (@ptr_field=%d, @data_size=%d)\n", ptr_field, data_size) );
			return;
		}

		/*end of previous section*/
		if (!sec->length && sec->received) {
			/* the length of the section could not be determined from the previous TS packet because we had only 1 or 2 bytes */
			if (sec->received == 1)
				sec->length = gf_m2ts_get_section_length(sec->section[0], data[1], data[2]);
			else /* (sec->received == 2)  */
				sec->length = gf_m2ts_get_section_length(sec->section[0], sec->section[1], data[1]);
			sec->section = (char*)gf_realloc(sec->section, sizeof(char)*sec->length);
		}

		if (sec->length && sec->received + ptr_field >= sec->length) {
			u32 len = sec->length - sec->received;
			memcpy(sec->section + sec->received, data+1, sizeof(char)*len);
			sec->received += len;
			if (ptr_field > len)
				GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[MPEG-2 TS] Invalid pointer field (@ptr_field=%d, @remaining=%d)\n", ptr_field, len) );
			gf_m2ts_section_complete(ts, sec, ses);
		}
		data += ptr_field+1;
		data_size -= ptr_field+1;
		payload_size -= ptr_field+1;

aggregated_section:

		if (sec->section) gf_free(sec->section);
		sec->length = sec->received = 0;
		sec->section = (char*)gf_malloc(sizeof(char)*data_size);
		memcpy(sec->section, data, sizeof(char)*data_size);
		sec->received = data_size;
	} else if (disc) {
		if (sec->section) gf_free(sec->section);
		sec->section = NULL;
		sec->received = sec->length = 0;
		return;
	} else if (!sec->section) {
		return;
	} else {
		if (sec->length && sec->received+data_size > sec->length)
			data_size = sec->length - sec->received;

		if (sec->length) {
			memcpy(sec->section + sec->received, data, sizeof(char)*data_size);
		} else {
			sec->section = (char*)gf_realloc(sec->section, sizeof(char)*(sec->received+data_size));
			memcpy(sec->section + sec->received, data, sizeof(char)*data_size);
		}
		sec->received += data_size;
	}
	/*alloc final buffer*/
	if (!sec->length && (sec->received >= 3)) {
		sec->length = gf_m2ts_get_section_length(sec->section[0], sec->section[1], sec->section[2]);
		sec->section = (char*)gf_realloc(sec->section, sizeof(char)*sec->length);

		if (sec->received > sec->length) {
			data_size -= sec->received - sec->length;
			sec->received = sec->length;
		}
	}
	if (!sec->length || sec->received < sec->length) return;

	/*OK done*/
	gf_m2ts_section_complete(ts, sec, ses);

	if (payload_size > data_size) {
		data += data_size;
		/* detect padding after previous section */
		if (data[0] != 0xFF) {
			data_size = payload_size - data_size;
			payload_size = data_size;
			goto aggregated_section;
		}
	}
}

static void gf_m2ts_process_sdt(GF_M2TS_Demuxer *ts, GF_M2TS_SECTION_ES *ses, GF_List *sections, u8 table_id, u16 ex_table_id, u8 version_number, u8 last_section_number, u32 status)
{
	u32 pos, evt_type;
	u32 nb_sections;
	u32 data_size;
	unsigned char *data;
	GF_M2TS_Section *section;

	/*wait for the last section */
	if (!(status&GF_M2TS_TABLE_END)) return;

	/*skip if already received*/
	if (status&GF_M2TS_TABLE_REPEAT) {
		if (ts->on_event) ts->on_event(ts, GF_M2TS_EVT_SDT_REPEAT, NULL);
		return;
	}

	if (table_id != GF_M2TS_TABLE_ID_SDT_ACTUAL) {
		return;
	}

	gf_m2ts_reset_sdt(ts);

	nb_sections = gf_list_count(sections);
	if (nb_sections > 1) {
		GF_LOG(GF_LOG_WARNING, GF_LOG_CONTAINER, ("[MPEG-2 TS] SDT on multiple sections not supported\n"));
	}

	section = (GF_M2TS_Section *)gf_list_get(sections, 0);
	data = section->data;
	data_size = section->data_size;

	//orig_net_id = (data[0] << 8) | data[1];
	pos = 3;
	while (pos < data_size) {
		GF_M2TS_SDT *sdt;
		u32 descs_size, d_pos, ulen;

		GF_SAFEALLOC(sdt, GF_M2TS_SDT);
		if (!sdt) {
			GF_LOG(GF_LOG_WARNING, GF_LOG_CONTAINER, ("[MPEG-2 TS] Fail to create SDT\n"));
			return;
		}
		gf_list_add(ts->SDTs, sdt);

		sdt->service_id = (data[pos]<<8) + data[pos+1];
		sdt->EIT_schedule = (data[pos+2] & 0x2) ? 1 : 0;
		sdt->EIT_present_following = (data[pos+2] & 0x1);
		sdt->running_status = (data[pos+3]>>5) & 0x7;
		sdt->free_CA_mode = (data[pos+3]>>4) & 0x1;
		descs_size = ((data[pos+3]&0xf)<<8) | data[pos+4];
		pos += 5;

		d_pos = 0;
		while (d_pos < descs_size) {
			u8 d_tag = data[pos+d_pos];
			u8 d_len = data[pos+d_pos+1];

			switch (d_tag) {
			case GF_M2TS_DVB_SERVICE_DESCRIPTOR:
				if (sdt->provider) gf_free(sdt->provider);
				sdt->provider = NULL;
				if (sdt->service) gf_free(sdt->service);
				sdt->service = NULL;

				d_pos+=2;
				sdt->service_type = data[pos+d_pos];
				ulen = data[pos+d_pos+1];
				d_pos += 2;
				sdt->provider = (char*)gf_malloc(sizeof(char)*(ulen+1));
				memcpy(sdt->provider, data+pos+d_pos, sizeof(char)*ulen);
				sdt->provider[ulen] = 0;
				d_pos += ulen;

				ulen = data[pos+d_pos];
				d_pos += 1;
				sdt->service = (char*)gf_malloc(sizeof(char)*(ulen+1));
				memcpy(sdt->service, data+pos+d_pos, sizeof(char)*ulen);
				sdt->service[ulen] = 0;
				d_pos += ulen;
				break;

			default:
				GF_LOG(GF_LOG_DEBUG, GF_LOG_CONTAINER, ("[MPEG-2 TS] Skipping descriptor (0x%x) not supported\n", d_tag));
				d_pos += d_len;
				if (d_len == 0) d_pos = descs_size;
				break;
			}
		}
		pos += descs_size;
	}
	evt_type = GF_M2TS_EVT_SDT_FOUND;
	if (ts->on_event) ts->on_event(ts, evt_type, NULL);
}

static void gf_m2ts_process_mpeg4section(GF_M2TS_Demuxer *ts, GF_M2TS_SECTION_ES *es, GF_List *sections, u8 table_id, u16 ex_table_id, u8 version_number, u8 last_section_number, u32 status)
{
	GF_M2TS_SL_PCK sl_pck;
	u32 nb_sections, i;
	GF_M2TS_Section *section;

	/*skip if already received*/
	if (status & GF_M2TS_TABLE_REPEAT)
		if (!(es->flags & GF_M2TS_ES_SEND_REPEATED_SECTIONS))
			return;

	GF_LOG(GF_LOG_DEBUG, GF_LOG_CONTAINER, ("[MPEG-2 TS] Sections for PID %d\n", es->pid) );
	/*send all sections (eg SL-packets)*/
	nb_sections = gf_list_count(sections);
	for (i=0; i<nb_sections; i++) {
		section = (GF_M2TS_Section *)gf_list_get(sections, i);
		sl_pck.data = (char *)section->data;
		sl_pck.data_len = section->data_size;
		sl_pck.stream = (GF_M2TS_ES *)es;
		sl_pck.version_number = version_number;
		if (ts->on_event) ts->on_event(ts, GF_M2TS_EVT_SL_PCK, &sl_pck);
	}
}

static void gf_m2ts_process_nit(GF_M2TS_Demuxer *ts, GF_M2TS_SECTION_ES *nit_es, GF_List *sections, u8 table_id, u16 ex_table_id, u8 version_number, u8 last_section_number, u32 status)
{
	GF_LOG(GF_LOG_DEBUG, GF_LOG_CONTAINER, ("[MPEG-2 TS] NIT table processing (not yet implemented)"));
}

/* decodes an Modified Julian Date (MJD) into a Co-ordinated Universal Time (UTC)
See annex C of DVB-SI ETSI EN 300468 */
static void dvb_decode_mjd_date(u32 date, u16 *year, u8 *month, u8 *day)
{
	u32 yp, mp, k;
	yp = (u32)((date - 15078.2)/365.25);
	mp = (u32)((date - 14956.1 - (u32)(yp * 365.25))/30.6001);
	if (day) *day = (u32)(date - 14956 - (u32)(yp * 365.25) - (u32)(mp * 30.6001));
	if (mp == 14 || mp == 15) k = 1;
	else k = 0;
	if (year) *year = yp + k + 1900;
	if (month) *month = mp - 1 - k*12;
}


static void gf_m2ts_process_tdt_tot(GF_M2TS_Demuxer *ts, GF_M2TS_SECTION_ES *tdt_tot_es, GF_List *sections, u8 table_id, u16 ex_table_id, u8 version_number, u8 last_section_number, u32 status)
{
	unsigned char *data;
	u32 data_size, nb_sections;
	GF_M2TS_Section *section;
	GF_M2TS_TDT_TOT *time_table;
	const char *table_name;

	/*wait for the last section */
	if ( !(status & GF_M2TS_TABLE_END) )
		return;

	switch (table_id) {
	case GF_M2TS_TABLE_ID_TDT:
		table_name = "TDT";
		break;
	case GF_M2TS_TABLE_ID_TOT:
		table_name = "TOT";
		break;
	default:
		GF_LOG(GF_LOG_DEBUG, GF_LOG_CONTAINER, ("[MPEG-2 TS] Unimplemented table_id %u for PID %u\n", table_id, GF_M2TS_PID_TDT_TOT_ST));
		return;
	}

	nb_sections = gf_list_count(sections);
	if (nb_sections > 1) {
		GF_LOG(GF_LOG_WARNING, GF_LOG_CONTAINER, ("[MPEG-2 TS] %s on multiple sections not supported\n", table_name));
	}

	section = (GF_M2TS_Section *)gf_list_get(sections, 0);
	data = section->data;
	data_size = section->data_size;

	/*TOT only contains 40 bits of UTC_time; TDT add descriptors and a CRC*/
	if ((table_id==GF_M2TS_TABLE_ID_TDT) && (data_size != 5)) {
		GF_LOG(GF_LOG_WARNING, GF_LOG_CONTAINER, ("[MPEG-2 TS] Corrupted TDT size\n", table_name));
	}
	GF_SAFEALLOC(time_table, GF_M2TS_TDT_TOT);
	if (!time_table) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[MPEG-2 TS] Fail to alloc DVB time table\n"));
		return;
	}

	/*UTC_time - see annex C of DVB-SI ETSI EN 300468*/
	dvb_decode_mjd_date(data[0]*256 + data[1], &(time_table->year), &(time_table->month), &(time_table->day));
	time_table->hour   = 10*((data[2]&0xf0)>>4) + (data[2]&0x0f);
	time_table->minute = 10*((data[3]&0xf0)>>4) + (data[3]&0x0f);
	time_table->second = 10*((data[4]&0xf0)>>4) + (data[4]&0x0f);
	assert(time_table->hour<24 && time_table->minute<60 && time_table->second<60);
	GF_LOG(GF_LOG_DEBUG, GF_LOG_CONTAINER, ("[MPEG-2 TS] Stream UTC time is %u/%02u/%02u %02u:%02u:%02u\n", time_table->year, time_table->month, time_table->day, time_table->hour, time_table->minute, time_table->second));

	switch (table_id) {
	case GF_M2TS_TABLE_ID_TDT:
		if (ts->TDT_time) gf_free(ts->TDT_time);
		ts->TDT_time = time_table;
		if (ts->on_event) ts->on_event(ts, GF_M2TS_EVT_TDT, time_table);
		break;
	case GF_M2TS_TABLE_ID_TOT:
#if 0
	{
		u32 pos, loop_len;
		loop_len = ((data[5]&0x0f) << 8) | (data[6] & 0xff);
		data += 7;
		pos = 0;
		while (pos < loop_len) {
			u8 tag = data[pos];
			pos += 2;
			if (tag == GF_M2TS_DVB_LOCAL_TIME_OFFSET_DESCRIPTOR) {
				char tmp_time[10];
				u16 offset_hours, offset_minutes;
				now->country_code[0] = data[pos];
				now->country_code[1] = data[pos+1];
				now->country_code[2] = data[pos+2];
				now->country_region_id = data[pos+3]>>2;

				sprintf(tmp_time, "%02x", data[pos+4]);
				offset_hours = atoi(tmp_time);
				sprintf(tmp_time, "%02x", data[pos+5]);
				offset_minutes = atoi(tmp_time);
				now->local_time_offset_seconds = (offset_hours * 60 + offset_minutes) * 60;
				if (data[pos+3] & 1) now->local_time_offset_seconds *= -1;

				dvb_decode_mjd_to_unix_time(data+pos+6, &now->unix_next_toc);

				sprintf(tmp_time, "%02x", data[pos+11]);
				offset_hours = atoi(tmp_time);
				sprintf(tmp_time, "%02x", data[pos+12]);
				offset_minutes = atoi(tmp_time);
				now->next_time_offset_seconds = (offset_hours * 60 + offset_minutes) * 60;
				if (data[pos+3] & 1) now->next_time_offset_seconds *= -1;
				pos+= 13;
			}
		}
		/*TODO: check lengths are ok*/
		if (ts->on_event) ts->on_event(ts, GF_M2TS_EVT_TOT, time_table);
	}
#endif
	/*check CRC32*/
	if (ts->tdt_tot->length<4) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[MPEG-2 TS] corrupted %s table (less than 4 bytes but CRC32 should be present\n", table_name));
		goto error_exit;
	}
	if (!gf_m2ts_crc32_check(ts->tdt_tot->section, ts->tdt_tot->length-4)) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[MPEG-2 TS] corrupted %s table (CRC32 failed)\n", table_name));
		goto error_exit;
	}
	if (ts->TDT_time) gf_free(ts->TDT_time);
	ts->TDT_time = time_table;
	if (ts->on_event) ts->on_event(ts, GF_M2TS_EVT_TOT, time_table);
	break;
	default:
		assert(0);
		goto error_exit;
	}

	return; /*success*/

error_exit:
	gf_free(time_table);
	return;
}

static GF_M2TS_MetadataPointerDescriptor *gf_m2ts_read_metadata_pointer_descriptor(GF_BitStream *bs, u32 length)
{
	u32 size;
	GF_M2TS_MetadataPointerDescriptor *d;
	GF_SAFEALLOC(d, GF_M2TS_MetadataPointerDescriptor);
	if (!d) return NULL;
	d->application_format = gf_bs_read_u16(bs);
	size = 2;
	if (d->application_format == 0xFFFF) {
		d->application_format_identifier = gf_bs_read_u32(bs);
		size += 4;
	}
	d->format = gf_bs_read_u8(bs);
	size += 1;
	if (d->format == 0xFF) {
		d->format_identifier = gf_bs_read_u32(bs);
		size += 4;
	}
	d->service_id = gf_bs_read_u8(bs);
	d->locator_record_flag = (gf_bs_read_int(bs, 1) ? GF_TRUE : GF_FALSE);
	d->carriage_flag = (enum metadata_carriage)gf_bs_read_int(bs, 2);
	gf_bs_read_int(bs, 5); /*reserved */
	size += 2;
	if (d->locator_record_flag) {
		d->locator_length = gf_bs_read_u8(bs);
		d->locator_data = (char *)gf_malloc(d->locator_length);
		size += 1 + d->locator_length;
		gf_bs_read_data(bs, d->locator_data, d->locator_length);
	}
	if (d->carriage_flag != 3) {
		d->program_number = gf_bs_read_u16(bs);
		size += 2;
	}
	if (d->carriage_flag == 1) {
		d->ts_location = gf_bs_read_u16(bs);
		d->ts_id = gf_bs_read_u16(bs);
		size += 4;
	}
	if (length-size > 0) {
		d->data_size = length-size;
		d->data = (char *)gf_malloc(d->data_size);
		gf_bs_read_data(bs, d->data, d->data_size);
	}
	return d;
}

void gf_m2ts_metadata_pointer_descriptor_del(GF_M2TS_MetadataPointerDescriptor *metapd)
{
	if (metapd) {
		if (metapd->locator_data) gf_free(metapd->locator_data);
		if (metapd->data) gf_free(metapd->data);
		gf_free(metapd);
	}
}

static GF_M2TS_MetadataDescriptor *gf_m2ts_read_metadata_descriptor(GF_BitStream *bs, u32 length)
{
	u32 size;
	GF_M2TS_MetadataDescriptor *d;
	GF_SAFEALLOC(d, GF_M2TS_MetadataDescriptor);
	if (!d) return NULL;
	d->application_format = gf_bs_read_u16(bs);
	size = 2;
	if (d->application_format == 0xFFFF) {
		d->application_format_identifier = gf_bs_read_u32(bs);
		size += 4;
	}
	d->format = gf_bs_read_u8(bs);
	size += 1;
	if (d->format == 0xFF) {
		d->format_identifier = gf_bs_read_u32(bs);
		size += 4;
	}
	d->service_id = gf_bs_read_u8(bs);
	d->decoder_config_flags = gf_bs_read_int(bs, 3);
	d->dsmcc_flag = (gf_bs_read_int(bs, 1) ? GF_TRUE : GF_FALSE);
	gf_bs_read_int(bs, 4); /* reserved */
	size += 2;
	if (d->dsmcc_flag) {
		d->service_id_record_length = gf_bs_read_u8(bs);
		d->service_id_record = (char *)gf_malloc(d->service_id_record_length);
		size += 1 + d->service_id_record_length;
		gf_bs_read_data(bs, d->service_id_record, d->service_id_record_length);
	}
	if (d->decoder_config_flags == 1) {
		d->decoder_config_length = gf_bs_read_u8(bs);
		d->decoder_config = (char *)gf_malloc(d->decoder_config_length);
		size += 1 + d->decoder_config_length;
		gf_bs_read_data(bs, d->decoder_config, d->decoder_config_length);
	}
	if (d->decoder_config_flags == 3) {
		d->decoder_config_id_length = gf_bs_read_u8(bs);
		d->decoder_config_id = (char *)gf_malloc(d->decoder_config_id_length);
		size += 1 + d->decoder_config_id_length;
		gf_bs_read_data(bs, d->decoder_config_id, d->decoder_config_id_length);
	}
	if (d->decoder_config_flags == 4) {
		d->decoder_config_service_id = gf_bs_read_u8(bs);
		size++;
	}
	return d;
}

void gf_m2ts_metadata_descriptor_del(GF_M2TS_MetadataDescriptor *metad)
{
	if (metad) {
		if (metad->service_id_record) gf_free(metad->service_id_record);
		if (metad->decoder_config) gf_free(metad->decoder_config);
		if (metad->decoder_config_id) gf_free(metad->decoder_config_id);
		gf_free(metad);
	}
}

static void gf_m2ts_process_pmt(GF_M2TS_Demuxer *ts, GF_M2TS_SECTION_ES *pmt, GF_List *sections, u8 table_id, u16 ex_table_id, u8 version_number, u8 last_section_number, u32 status)
{
	u32 info_length, pos, desc_len, evt_type, nb_es,i;
	u32 nb_sections;
	u32 data_size;
	u32 nb_hevc, nb_hevc_temp, nb_shvc, nb_shvc_temp, nb_mhvc, nb_mhvc_temp;
	unsigned char *data;
	GF_M2TS_Section *section;
	GF_Err e = GF_OK;

	/*wait for the last section */
	if (!(status&GF_M2TS_TABLE_END)) return;

	nb_es = 0;

	/*skip if already received but no update detected (eg same data) */
	if ((status&GF_M2TS_TABLE_REPEAT) && !(status&GF_M2TS_TABLE_UPDATE))  {
		if (ts->on_event) ts->on_event(ts, GF_M2TS_EVT_PMT_REPEAT, pmt->program);
		return;
	}

	if (pmt->sec->demux_restarted) {
		pmt->sec->demux_restarted = 0;
		return;
	}
	GF_LOG(GF_LOG_DEBUG, GF_LOG_CONTAINER, ("[MPEG-2 TS] PMT Found or updated\n"));

	nb_sections = gf_list_count(sections);
	if (nb_sections > 1) {
		GF_LOG(GF_LOG_WARNING, GF_LOG_CONTAINER, ("PMT on multiple sections not supported\n"));
	}

	section = (GF_M2TS_Section *)gf_list_get(sections, 0);
	data = section->data;
	data_size = section->data_size;

	pmt->program->pcr_pid = ((data[0] & 0x1f) << 8) | data[1];

	info_length = ((data[2]&0xf)<<8) | data[3];
	if (info_length != 0) {
		/* ...Read Descriptors ... */
		u8 tag, len;
		u32 first_loop_len = 0;
		tag = data[4];
		len = data[5];
		while (info_length > first_loop_len) {
#ifndef FORCE_DISABLE_MPEG4SL_OVER_MPEG2TS
			if (tag == GF_M2TS_MPEG4_IOD_DESCRIPTOR) {
				u32 size;
				GF_BitStream *iod_bs;
				iod_bs = gf_bs_new((char *)data+8, len-2, GF_BITSTREAM_READ);
				if (pmt->program->pmt_iod) gf_odf_desc_del((GF_Descriptor *)pmt->program->pmt_iod);
				e = gf_odf_parse_descriptor(iod_bs , (GF_Descriptor **) &pmt->program->pmt_iod, &size);
				gf_bs_del(iod_bs );
				if (e==GF_OK) {
					/*remember program number for service/program selection*/
					if (pmt->program->pmt_iod) pmt->program->pmt_iod->ServiceID = pmt->program->number;
					/*if empty IOD (freebox case), discard it and use dynamic declaration of object*/
					if (!gf_list_count(pmt->program->pmt_iod->ESDescriptors)) {
						gf_odf_desc_del((GF_Descriptor *)pmt->program->pmt_iod);
						pmt->program->pmt_iod = NULL;
					}
				}
			} else if (tag == GF_M2TS_METADATA_POINTER_DESCRIPTOR) {
				GF_BitStream *metadatapd_bs;
				GF_M2TS_MetadataPointerDescriptor *metapd;
				metadatapd_bs = gf_bs_new((char *)data+6, len, GF_BITSTREAM_READ);
				metapd = gf_m2ts_read_metadata_pointer_descriptor(metadatapd_bs, len);
				gf_bs_del(metadatapd_bs);
				if (metapd->application_format_identifier == GF_MEDIA_TYPE_ID3 &&
				        metapd->format_identifier == GF_MEDIA_TYPE_ID3 &&
				        metapd->carriage_flag == METADATA_CARRIAGE_SAME_TS) {
					/*HLS ID3 Metadata */
					pmt->program->metadata_pointer_descriptor = metapd;
				} else {
					/* don't know what to do with it for now, delete */
					gf_m2ts_metadata_pointer_descriptor_del(metapd);
				}
			} else {
#else
			{
#endif
				GF_LOG(GF_LOG_DEBUG, GF_LOG_CONTAINER, ("[MPEG-2 TS] Skipping descriptor (0x%x) and others not supported\n", tag));
			}
			first_loop_len += 2 + len;
		}
	}
	if (data_size <= 4 + info_length) return;
	data += 4 + info_length;
	data_size -= 4 + info_length;
	pos = 0;

	/* count de number of program related PMT received */
	for(i=0; i<gf_list_count(ts->programs); i++) {
		GF_M2TS_Program *prog = (GF_M2TS_Program *)gf_list_get(ts->programs,i);
		if(prog->pmt_pid == pmt->pid) {
			break;
		}
	}

	nb_hevc = nb_hevc_temp = nb_shvc = nb_shvc_temp = nb_mhvc = nb_mhvc_temp = 0;
	while (pos<data_size) {
		GF_M2TS_PES *pes = NULL;
		GF_M2TS_SECTION_ES *ses = NULL;
		GF_M2TS_ES *es = NULL;
		Bool inherit_pcr = 0;
		u32 pid, stream_type, reg_desc_format;

		stream_type = data[0];
		pid = ((data[1] & 0x1f) << 8) | data[2];
		desc_len = ((data[3] & 0xf) << 8) | data[4];

		GF_LOG(GF_LOG_DEBUG, GF_LOG_CONTAINER, ("stream_type :%d \n",stream_type));
		switch (stream_type) {

		/* PES */
		case GF_M2TS_VIDEO_MPEG1:
		case GF_M2TS_VIDEO_MPEG2:
		case GF_M2TS_VIDEO_DCII:
		case GF_M2TS_VIDEO_MPEG4:
		case GF_M2TS_SYSTEMS_MPEG4_PES:
		case GF_M2TS_VIDEO_H264:
		case GF_M2TS_VIDEO_SVC:
		case GF_M2TS_VIDEO_MVCD:
		case GF_M2TS_VIDEO_HEVC:
		case GF_M2TS_VIDEO_HEVC_MCTS:
		case GF_M2TS_VIDEO_HEVC_TEMPORAL:
		case GF_M2TS_VIDEO_SHVC:
		case GF_M2TS_VIDEO_SHVC_TEMPORAL:
		case GF_M2TS_VIDEO_MHVC:
		case GF_M2TS_VIDEO_MHVC_TEMPORAL:
			inherit_pcr = 1;
		case GF_M2TS_AUDIO_MPEG1:
		case GF_M2TS_AUDIO_MPEG2:
		case GF_M2TS_AUDIO_AAC:
		case GF_M2TS_AUDIO_LATM_AAC:
		case GF_M2TS_AUDIO_AC3:
		case GF_M2TS_AUDIO_DTS:
		case GF_M2TS_MHAS_MAIN:
		case GF_M2TS_MHAS_AUX:
		case GF_M2TS_SUBTITLE_DVB:
		case GF_M2TS_METADATA_PES:
			GF_SAFEALLOC(pes, GF_M2TS_PES);
			if (!pes) {
				GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[MPEG2TS] Failed to allocate ES for pid %d\n", pid));
				return;
			}
			pes->cc = -1;
			pes->flags = GF_M2TS_ES_IS_PES;
			if (inherit_pcr)
				pes->flags |= GF_M2TS_INHERIT_PCR;
			es = (GF_M2TS_ES *)pes;
			break;
		case GF_M2TS_PRIVATE_DATA:
			GF_SAFEALLOC(pes, GF_M2TS_PES);
			if (!pes) {
				GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[MPEG2TS] Failed to allocate ES for pid %d\n", pid));
				return;
			}
			pes->cc = -1;
			pes->flags = GF_M2TS_ES_IS_PES;
			es = (GF_M2TS_ES *)pes;
			break;
		/* Sections */
		case GF_M2TS_SYSTEMS_MPEG4_SECTIONS:
			GF_SAFEALLOC(ses, GF_M2TS_SECTION_ES);
			if (!ses) {
				GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[MPEG2TS] Failed to allocate ES for pid %d\n", pid));
				return;
			}
			es = (GF_M2TS_ES *)ses;
			es->flags |= GF_M2TS_ES_IS_SECTION;
			/* carriage of ISO_IEC_14496 data in sections */
			if (stream_type == GF_M2TS_SYSTEMS_MPEG4_SECTIONS) {
				/*MPEG-4 sections need to be fully checked: if one section is lost, this means we lost
				one SL packet in the AU so we must wait for the complete section again*/
				ses->sec = gf_m2ts_section_filter_new(gf_m2ts_process_mpeg4section, 0);
				/*create OD container*/
				if (!pmt->program->additional_ods) {
					pmt->program->additional_ods = gf_list_new();
					ts->has_4on2 = 1;
				}
			}
			break;

		case GF_M2TS_13818_6_ANNEX_A:
		case GF_M2TS_13818_6_ANNEX_B:
		case GF_M2TS_13818_6_ANNEX_C:
		case GF_M2TS_13818_6_ANNEX_D:
		case GF_M2TS_PRIVATE_SECTION:
		case GF_M2TS_QUALITY_SEC:
		case GF_M2TS_MORE_SEC:
			GF_SAFEALLOC(ses, GF_M2TS_SECTION_ES);
			if (!ses) {
				GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[MPEG2TS] Failed to allocate ES for pid %d\n", pid));
				return;
			}
			es = (GF_M2TS_ES *)ses;
			es->flags |= GF_M2TS_ES_IS_SECTION;
			es->pid = pid;
			es->service_id = pmt->program->number;
			if (stream_type == GF_M2TS_PRIVATE_SECTION) {
				GF_LOG(GF_LOG_INFO, GF_LOG_CONTAINER, ("AIT sections on pid %d\n", pid));
			} else if (stream_type == GF_M2TS_QUALITY_SEC) {
				GF_LOG(GF_LOG_INFO, GF_LOG_CONTAINER, ("Quality metadata sections on pid %d\n", pid));
			} else if (stream_type == GF_M2TS_MORE_SEC) {
				GF_LOG(GF_LOG_INFO, GF_LOG_CONTAINER, ("MORE sections on pid %d\n", pid));
			} else {
				GF_LOG(GF_LOG_INFO, GF_LOG_CONTAINER, ("stream type DSM CC user private sections on pid %d \n", pid));
			}
			/* NULL means: trigger the call to on_event with DVB_GENERAL type and the raw section as payload */
			ses->sec = gf_m2ts_section_filter_new(NULL, 1);
			//ses->sec->service_id = pmt->program->number;
			break;

		case GF_M2TS_MPE_SECTIONS:
			if (! ts->prefix_present) {
				GF_LOG(GF_LOG_INFO, GF_LOG_CONTAINER, ("stream type MPE found : pid = %d \n", pid));
#ifdef GPAC_ENABLE_MPE
				es = gf_dvb_mpe_section_new();
				if (es->flags & GF_M2TS_ES_IS_SECTION) {
					/* NULL means: trigger the call to on_event with DVB_GENERAL type and the raw section as payload */
					((GF_M2TS_SECTION_ES*)es)->sec = gf_m2ts_section_filter_new(NULL, 1);
				}
#endif
				break;
			}

		default:
			GF_LOG(GF_LOG_WARNING, GF_LOG_CONTAINER, ("[MPEG-2 TS] Stream type (0x%x) for PID %d not supported\n", stream_type, pid ) );
			//GF_LOG(/*GF_LOG_WARNING*/GF_LOG_ERROR, GF_LOG_CONTAINER, ("[MPEG-2 TS] Stream type (0x%x) for PID %d not supported\n", stream_type, pid ) );
			break;
		}

		if (es) {
			es->stream_type = (stream_type==GF_M2TS_PRIVATE_DATA) ? 0 : stream_type;
			es->program = pmt->program;
			es->pid = pid;
			es->component_tag = -1;
		}

		pos += 5;
		data += 5;

		while (desc_len) {
			u8 tag = data[0];
			u32 len = data[1];
			if (es) {
				switch (tag) {
				case GF_M2TS_ISO_639_LANGUAGE_DESCRIPTOR:
					pes->lang = GF_4CC(' ', data[2], data[3], data[4]);
					break;
				case GF_M2TS_MPEG4_SL_DESCRIPTOR:
#ifdef FORCE_DISABLE_MPEG4SL_OVER_MPEG2TS
					es->mpeg4_es_id = es->pid;
#else
					es->mpeg4_es_id = ((data[2] & 0x1f) << 8) | data[3];
#endif
					es->flags |= GF_M2TS_ES_IS_SL;
					break;
				case GF_M2TS_REGISTRATION_DESCRIPTOR:
					reg_desc_format = GF_4CC(data[2], data[3], data[4], data[5]);
					/*cf http://www.smpte-ra.org/mpegreg/mpegreg.html*/
					switch (reg_desc_format) {
					case GF_MEDIA_STREAM_AC3:
						es->stream_type = GF_M2TS_AUDIO_AC3;
						break;
					case GF_MEDIA_STREAM_VC1:
						es->stream_type = GF_M2TS_VIDEO_VC1;
						break;
					default:
						GF_LOG(GF_LOG_INFO, GF_LOG_CONTAINER, ("Unknown registration descriptor %s\n", gf_4cc_to_str(reg_desc_format) ));
						break;
					}
					break;
				case GF_M2TS_DVB_EAC3_DESCRIPTOR:
					es->stream_type = GF_M2TS_AUDIO_EC3;
					break;
				case GF_M2TS_DVB_DATA_BROADCAST_ID_DESCRIPTOR:
				{
					u32 id = data[2]<<8 | data[3];
					if ((id == 0xB) && ses && !ses->sec) {
						ses->sec = gf_m2ts_section_filter_new(NULL, 1);
					}
				}
				break;
				case GF_M2TS_DVB_SUBTITLING_DESCRIPTOR:
					pes->sub.language[0] = data[2];
					pes->sub.language[1] = data[3];
					pes->sub.language[2] = data[4];
					pes->sub.type = data[5];
					pes->sub.composition_page_id = (data[6]<<8) | data[7];
					pes->sub.ancillary_page_id = (data[8]<<8) | data[9];

					es->stream_type = GF_M2TS_DVB_SUBTITLE;
					break;
				case GF_M2TS_DVB_STREAM_IDENTIFIER_DESCRIPTOR:
				{
					es->component_tag = data[2];
					GF_LOG(GF_LOG_DEBUG, GF_LOG_CONTAINER, ("Component Tag: %d on Program %d\n", es->component_tag, es->program->number));
				}
				break;
				case GF_M2TS_DVB_TELETEXT_DESCRIPTOR:
					es->stream_type = GF_M2TS_DVB_TELETEXT;
					break;
				case GF_M2TS_DVB_VBI_DATA_DESCRIPTOR:
					es->stream_type = GF_M2TS_DVB_VBI;
					break;
				case GF_M2TS_HIERARCHY_DESCRIPTOR:
					if (pes) {
						u8 hierarchy_embedded_layer_index;
						GF_BitStream *hbs = gf_bs_new((const char *)data, data_size, GF_BITSTREAM_READ);
						/*u32 skip = */gf_bs_read_int(hbs, 16);
						/*u8 res1 = */gf_bs_read_int(hbs, 1);
						/*u8 temp_scal = */gf_bs_read_int(hbs, 1);
						/*u8 spatial_scal = */gf_bs_read_int(hbs, 1);
						/*u8 quality_scal = */gf_bs_read_int(hbs, 1);
						/*u8 hierarchy_type = */gf_bs_read_int(hbs, 4);
						/*u8 res2 = */gf_bs_read_int(hbs, 2);
						/*u8 hierarchy_layer_index = */gf_bs_read_int(hbs, 6);
						/*u8 tref_not_present = */gf_bs_read_int(hbs, 1);
						/*u8 res3 = */gf_bs_read_int(hbs, 1);
						hierarchy_embedded_layer_index = gf_bs_read_int(hbs, 6);
						/*u8 res4 = */gf_bs_read_int(hbs, 2);
						/*u8 hierarchy_channel = */gf_bs_read_int(hbs, 6);
						gf_bs_del(hbs);

						pes->depends_on_pid = 1+hierarchy_embedded_layer_index;
					}
					break;
				case GF_M2TS_METADATA_DESCRIPTOR:
				{
					GF_BitStream *metadatad_bs;
					GF_M2TS_MetadataDescriptor *metad;
					metadatad_bs = gf_bs_new((char *)data+2, len, GF_BITSTREAM_READ);
					metad = gf_m2ts_read_metadata_descriptor(metadatad_bs, len);
					gf_bs_del(metadatad_bs);
					if (metad->application_format_identifier == GF_MEDIA_TYPE_ID3 &&
					        metad->format_identifier == GF_MEDIA_TYPE_ID3) {
						/*HLS ID3 Metadata */
						if (pes) {
							pes->metadata_descriptor = metad;
							pes->stream_type = GF_M2TS_METADATA_ID3_HLS;
						}
					} else {
						/* don't know what to do with it for now, delete */
						gf_m2ts_metadata_descriptor_del(metad);
					}
				}
				break;

				default:
					GF_LOG(GF_LOG_DEBUG, GF_LOG_CONTAINER, ("[MPEG-2 TS] skipping descriptor (0x%x) not supported\n", tag));
					break;
				}
			}

			data += len+2;
			pos += len+2;
			if (desc_len < len+2) {
				GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[MPEG-2 TS] Invalid PMT es descriptor size for PID %d\n", pid ) );
				break;
			}
			desc_len-=len+2;
		}

		if (es && !es->stream_type) {
			gf_free(es);
			es = NULL;
			GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[MPEG-2 TS] Private Stream type (0x%x) for PID %d not supported\n", stream_type, pid ) );
		}

		if (!es) continue;

		if (ts->ess[pid]) {
			//this is component reuse across programs, overwrite the previously declared stream ...
			if (status & GF_M2TS_TABLE_FOUND) {
				GF_LOG(GF_LOG_INFO, GF_LOG_CONTAINER, ("[MPEG-2 TS] PID %d reused across programs %d and %d, not completely supported\n", pid, ts->ess[pid]->program->number, es->program->number ) );

				//add stream to program but don't reassign the pid table until the stream is playing (>GF_M2TS_PES_FRAMING_SKIP)
				gf_list_add(pmt->program->streams, es);
				if (!(es->flags & GF_M2TS_ES_IS_SECTION) ) gf_m2ts_set_pes_framing(pes, GF_M2TS_PES_FRAMING_SKIP);

				nb_es++;
				//skip assignment below
				es = NULL;
			}
			/*watchout for pmt update - FIXME this likely won't work in most cases*/
			else {

				GF_M2TS_ES *o_es = ts->ess[es->pid];

				if ((o_es->stream_type == es->stream_type)
				        && ((o_es->flags & GF_M2TS_ES_STATIC_FLAGS_MASK) == (es->flags & GF_M2TS_ES_STATIC_FLAGS_MASK))
				        && (o_es->mpeg4_es_id == es->mpeg4_es_id)
				        && ((o_es->flags & GF_M2TS_ES_IS_SECTION) || ((GF_M2TS_PES *)o_es)->lang == ((GF_M2TS_PES *)es)->lang)
				   ) {
					gf_free(es);
					es = NULL;
				} else {
					gf_m2ts_es_del(o_es, ts);
					ts->ess[es->pid] = NULL;
				}
			}
		}

		if (es) {
			ts->ess[es->pid] = es;
			gf_list_add(pmt->program->streams, es);
			if (!(es->flags & GF_M2TS_ES_IS_SECTION) ) gf_m2ts_set_pes_framing(pes, GF_M2TS_PES_FRAMING_SKIP);

			nb_es++;
		}

		if (es->stream_type == GF_M2TS_VIDEO_HEVC) nb_hevc++;
		else if (es->stream_type == GF_M2TS_VIDEO_HEVC_TEMPORAL) nb_hevc_temp++;
		else if (es->stream_type == GF_M2TS_VIDEO_SHVC) nb_shvc++;
		else if (es->stream_type == GF_M2TS_VIDEO_SHVC_TEMPORAL) nb_shvc_temp++;
		else if (es->stream_type == GF_M2TS_VIDEO_MHVC) nb_mhvc++;
		else if (es->stream_type == GF_M2TS_VIDEO_MHVC_TEMPORAL) nb_mhvc_temp++;
	}

	//Table 2-139, implied hierarchy indexes
	if (nb_hevc_temp + nb_shvc + nb_shvc_temp + nb_mhvc+ nb_mhvc_temp) {
		for (i=0; i<gf_list_count(pmt->program->streams); i++) {
			GF_M2TS_PES *es = (GF_M2TS_PES *)gf_list_get(pmt->program->streams, i);
			if ( !(es->flags & GF_M2TS_ES_IS_PES)) continue;
			if (es->depends_on_pid) continue;

			switch (es->stream_type) {
			case GF_M2TS_VIDEO_HEVC_TEMPORAL:
				es->depends_on_pid = 1;
				break;
			case GF_M2TS_VIDEO_SHVC:
				if (!nb_hevc_temp) es->depends_on_pid = 1;
				else es->depends_on_pid = 2;
				break;
			case GF_M2TS_VIDEO_SHVC_TEMPORAL:
				es->depends_on_pid = 3;
				break;
			case GF_M2TS_VIDEO_MHVC:
				if (!nb_hevc_temp) es->depends_on_pid = 1;
				else es->depends_on_pid = 2;
				break;
			case GF_M2TS_VIDEO_MHVC_TEMPORAL:
				if (!nb_hevc_temp) es->depends_on_pid = 2;
				else es->depends_on_pid = 3;
				break;
			}
		}
	}

	if (nb_es) {
		u32 i;

		//translate hierarchy descriptors indexes into PIDs - check whether the PMT-index rules are the same for HEVC
		for (i=0; i<gf_list_count(pmt->program->streams); i++) {
			GF_M2TS_PES *an_es = NULL;
			GF_M2TS_PES *es = (GF_M2TS_PES *)gf_list_get(pmt->program->streams, i);
			if ( !(es->flags & GF_M2TS_ES_IS_PES)) continue;
			if (!es->depends_on_pid) continue;

			//fixeme we are not always assured that hierarchy_layer_index matches the stream index...
			//+1 is because our first stream is the PMT
			an_es =  (GF_M2TS_PES *)gf_list_get(pmt->program->streams, es->depends_on_pid);
			if (an_es)
				es->depends_on_pid = an_es->pid;
		}

		evt_type = (status&GF_M2TS_TABLE_FOUND) ? GF_M2TS_EVT_PMT_FOUND : GF_M2TS_EVT_PMT_UPDATE;
		if (ts->on_event) ts->on_event(ts, evt_type, pmt->program);
	} else {
		/* if we found no new ES it's simply a repeat of the PMT */
		if (ts->on_event) ts->on_event(ts, GF_M2TS_EVT_PMT_REPEAT, pmt->program);
	}
}

static void gf_m2ts_process_pat(GF_M2TS_Demuxer *ts, GF_M2TS_SECTION_ES *ses, GF_List *sections, u8 table_id, u16 ex_table_id, u8 version_number, u8 last_section_number, u32 status)
{
	GF_M2TS_Program *prog;
	GF_M2TS_SECTION_ES *pmt;
	u32 i, nb_progs, evt_type;
	u32 nb_sections;
	u32 data_size;
	unsigned char *data;
	GF_M2TS_Section *section;

	/*wait for the last section */
	if (!(status&GF_M2TS_TABLE_END)) return;

	/*skip if already received*/
	if (status&GF_M2TS_TABLE_REPEAT) {
		if (ts->on_event) ts->on_event(ts, GF_M2TS_EVT_PAT_REPEAT, NULL);
		return;
	}

	nb_sections = gf_list_count(sections);
	if (nb_sections > 1) {
		GF_LOG(GF_LOG_WARNING, GF_LOG_CONTAINER, ("PAT on multiple sections not supported\n"));
	}

	section = (GF_M2TS_Section *)gf_list_get(sections, 0);
	data = section->data;
	data_size = section->data_size;

	if (!(status&GF_M2TS_TABLE_UPDATE) && gf_list_count(ts->programs)) {
		if (ts->pat->demux_restarted) {
			ts->pat->demux_restarted = 0;
		} else {
			GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("Multiple different PAT on single TS found, ignoring new PAT declaration (table id %d - extended table id %d)\n", table_id, ex_table_id));
		}
		return;
	}
	nb_progs = data_size / 4;

	for (i=0; i<nb_progs; i++) {
		u16 number, pid;
		number = (data[0]<<8) | data[1];
		pid = (data[2]&0x1f)<<8 | data[3];
		data += 4;
		if (number==0) {
			if (!ts->nit) {
				ts->nit = gf_m2ts_section_filter_new(gf_m2ts_process_nit, 0);
			}
		} else {
			GF_SAFEALLOC(prog, GF_M2TS_Program);
			if (!prog) {
				GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("Fail to allocate program for pid %d\n", pid));
				return;
			}
			prog->streams = gf_list_new();
			prog->pmt_pid = pid;
			prog->number = number;
			prog->ts = ts;
			gf_list_add(ts->programs, prog);
			GF_SAFEALLOC(pmt, GF_M2TS_SECTION_ES);
			if (!pmt) {
				GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("Fail to allocate pmt filter for pid %d\n", pid));
				return;
			}
			pmt->flags = GF_M2TS_ES_IS_SECTION;
			gf_list_add(prog->streams, pmt);
			pmt->pid = prog->pmt_pid;
			pmt->program = prog;
			ts->ess[pmt->pid] = (GF_M2TS_ES *)pmt;
			pmt->sec = gf_m2ts_section_filter_new(gf_m2ts_process_pmt, 0);
		}
	}

	evt_type = (status&GF_M2TS_TABLE_UPDATE) ? GF_M2TS_EVT_PAT_UPDATE : GF_M2TS_EVT_PAT_FOUND;
	if (ts->on_event) ts->on_event(ts, evt_type, NULL);
}

static void gf_m2ts_process_cat(GF_M2TS_Demuxer *ts, GF_M2TS_SECTION_ES *ses, GF_List *sections, u8 table_id, u16 ex_table_id, u8 version_number, u8 last_section_number, u32 status)
{
	u32 evt_type;
	/*
		GF_M2TS_Program *prog;
		GF_M2TS_SECTION_ES *pmt;
		u32 i, nb_progs;
		u32 nb_sections;
		u32 data_size;
		unsigned char *data;
		GF_M2TS_Section *section;
	*/

	/*wait for the last section */
	if (!(status&GF_M2TS_TABLE_END)) return;

	/*skip if already received*/
	if (status&GF_M2TS_TABLE_REPEAT) {
		if (ts->on_event) ts->on_event(ts, GF_M2TS_EVT_CAT_REPEAT, NULL);
		return;
	}
	/*
		nb_sections = gf_list_count(sections);
		if (nb_sections > 1) {
			GF_LOG(GF_LOG_WARNING, GF_LOG_CONTAINER, ("CAT on multiple sections not supported\n"));
		}

		section = (GF_M2TS_Section *)gf_list_get(sections, 0);
		data = section->data;
		data_size = section->data_size;

		nb_progs = data_size / 4;

		for (i=0; i<nb_progs; i++) {
			u16 number, pid;
			number = (data[0]<<8) | data[1];
			pid = (data[2]&0x1f)<<8 | data[3];
			data += 4;
			if (number==0) {
				if (!ts->nit) {
					ts->nit = gf_m2ts_section_filter_new(gf_m2ts_process_nit, 0);
				}
			} else {
				GF_SAFEALLOC(prog, GF_M2TS_Program);
				prog->streams = gf_list_new();
				prog->pmt_pid = pid;
				prog->number = number;
				gf_list_add(ts->programs, prog);
				GF_SAFEALLOC(pmt, GF_M2TS_SECTION_ES);
				pmt->flags = GF_M2TS_ES_IS_SECTION;
				gf_list_add(prog->streams, pmt);
				pmt->pid = prog->pmt_pid;
				pmt->program = prog;
				ts->ess[pmt->pid] = (GF_M2TS_ES *)pmt;
				pmt->sec = gf_m2ts_section_filter_new(gf_m2ts_process_pmt, 0);
			}
		}
	*/

	evt_type = (status&GF_M2TS_TABLE_UPDATE) ? GF_M2TS_EVT_CAT_UPDATE : GF_M2TS_EVT_CAT_FOUND;
	if (ts->on_event) ts->on_event(ts, evt_type, NULL);
}

u64 gf_m2ts_get_pts(unsigned char *data)
{
	u64 pts;
	u32 val;
	pts = (u64)((data[0] >> 1) & 0x07) << 30;
	val = (data[1] << 8) | data[2];
	pts |= (u64)(val >> 1) << 15;
	val = (data[3] << 8) | data[4];
	pts |= (u64)(val >> 1);
	return pts;
}

void gf_m2ts_pes_header(GF_M2TS_PES *pes, unsigned char *data, u32 data_size, GF_M2TS_PESHeader *pesh)
{
	u32 has_pts, has_dts;
	u32 len_check;
	memset(pesh, 0, sizeof(GF_M2TS_PESHeader));

	len_check = 0;

	pesh->id = data[0];
	pesh->pck_len = (data[1]<<8) | data[2];
	/*
		2bits
		scrambling_control		= gf_bs_read_int(bs,2);
		priority				= gf_bs_read_int(bs,1);
	*/
	pesh->data_alignment = (data[3] & 0x4) ? 1 : 0;
	/*
		copyright				= gf_bs_read_int(bs,1);
		original				= gf_bs_read_int(bs,1);
	*/
	has_pts = (data[4]&0x80);
	has_dts = has_pts ? (data[4]&0x40) : 0;
	/*
		ESCR_flag				= gf_bs_read_int(bs,1);
		ES_rate_flag			= gf_bs_read_int(bs,1);
		DSM_flag				= gf_bs_read_int(bs,1);
		additional_copy_flag	= gf_bs_read_int(bs,1);
		prev_crc_flag			= gf_bs_read_int(bs,1);
		extension_flag			= gf_bs_read_int(bs,1);
	*/

	pesh->hdr_data_len = data[5];

	data += 6;
	if (has_pts) {
		pesh->PTS = gf_m2ts_get_pts(data);
		data+=5;
		len_check += 5;
	}
	if (has_dts) {
		pesh->DTS = gf_m2ts_get_pts(data);
		//data+=5;
		len_check += 5;
	} else {
		pesh->DTS = pesh->PTS;
	}
	if (len_check < pesh->hdr_data_len) {
		GF_LOG(GF_LOG_DEBUG, GF_LOG_CONTAINER, ("[MPEG-2 TS] PID %d Skipping %d bytes in pes header\n", pes->pid, pesh->hdr_data_len - len_check));
	} else if (len_check > pesh->hdr_data_len) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[MPEG-2 TS] PID %d Wrong pes_header_data_length field %d bytes - read %d\n", pes->pid, pesh->hdr_data_len, len_check));
	}

	if ((pesh->PTS<90000) && ((s32)pesh->DTS<0)) {
		GF_LOG(GF_LOG_WARNING, GF_LOG_CONTAINER, ("[MPEG-2 TS] PID %d Wrong DTS %d negative for PTS %d - forcing to 0\n", pes->pid, pesh->DTS, pesh->PTS));
		pesh->DTS=0;
	}
}

static void gf_m2ts_store_temi(GF_M2TS_Demuxer *ts, GF_M2TS_PES *pes)
{
	GF_BitStream *bs = gf_bs_new(pes->temi_tc_desc, pes->temi_tc_desc_len, GF_BITSTREAM_READ);
	u32 has_timestamp = gf_bs_read_int(bs, 2);
	Bool has_ntp = (Bool) gf_bs_read_int(bs, 1);
	/*u32 has_ptp = */gf_bs_read_int(bs, 1);
	/*u32 has_timecode = */gf_bs_read_int(bs, 2);

	memset(&pes->temi_tc, 0, sizeof(GF_M2TS_TemiTimecodeDescriptor));
	pes->temi_tc.force_reload = gf_bs_read_int(bs, 1);
	pes->temi_tc.is_paused = gf_bs_read_int(bs, 1);
	pes->temi_tc.is_discontinuity = gf_bs_read_int(bs, 1);
	gf_bs_read_int(bs, 7);
	pes->temi_tc.timeline_id = gf_bs_read_int(bs, 8);
	if (has_timestamp) {
		pes->temi_tc.media_timescale = gf_bs_read_u32(bs);
		if (has_timestamp==2)
			pes->temi_tc.media_timestamp = gf_bs_read_u64(bs);
		else
			pes->temi_tc.media_timestamp = gf_bs_read_u32(bs);
	}
	if (has_ntp) {
		pes->temi_tc.ntp = gf_bs_read_u64(bs);
	}
	gf_bs_del(bs);
	pes->temi_tc_desc_len = 0;
	pes->temi_pending = 1;
}

void gf_m2ts_flush_pes(GF_M2TS_Demuxer *ts, GF_M2TS_PES *pes)
{
	GF_M2TS_PESHeader pesh;
	if (!ts) return;

	/*we need at least a full, valid start code !!*/
	if ((pes->pck_data_len >= 4) && !pes->pck_data[0] && !pes->pck_data[1] && (pes->pck_data[2] == 0x1)) {
		u32 len;
		u32 stream_id = pes->pck_data[3] | 0x100;
		Bool same_pts = GF_FALSE;

		if ((stream_id >= 0x1c0 && stream_id <= 0x1df) ||
			(stream_id >= 0x1e0 && stream_id <= 0x1ef) ||
			(stream_id == 0x1bd) ||
			(stream_id == 0x10d) ||
			/*SL-packetized*/
			((u8)pes->pck_data[3] == 0xfa)
			) {
		}
		else {
			GF_LOG(GF_LOG_DEBUG, GF_LOG_CONTAINER, ("[MPEG-2 TS] PES %d: unknown stream ID %08X\n", pes->pid, stream_id));
		}


		/*OK read header*/
		gf_m2ts_pes_header(pes, pes->pck_data + 3, pes->pck_data_len - 3, &pesh);

		/*send PES timing*/
		if (ts->notify_pes_timing) {
			GF_M2TS_PES_PCK pck;
			memset(&pck, 0, sizeof(GF_M2TS_PES_PCK));
			pck.PTS = pesh.PTS;
			pck.DTS = pesh.DTS;
			pck.stream = pes;
			if (pes->rap) pck.flags |= GF_M2TS_PES_PCK_RAP;
			pes->pes_end_packet_number = ts->pck_number;
			if (ts->on_event) ts->on_event(ts, GF_M2TS_EVT_PES_TIMING, &pck);
		}
		GF_LOG(GF_LOG_DEBUG, GF_LOG_CONTAINER, ("[MPEG-2 TS] PID %d Got PES header DTS %d PTS %d\n", pes->pid, pesh.DTS, pesh.PTS));

		if (pesh.PTS) {
			if (pesh.PTS == pes->PTS) {
				same_pts = GF_TRUE;
				GF_LOG(GF_LOG_WARNING, GF_LOG_CONTAINER, ("[MPEG-2 TS] PID %d - same PTS "LLU" for two consecutive PES packets \n", pes->pid, pes->PTS));
			}
#ifndef GPAC_DISABLE_LOG
			/*FIXME - this test should only be done for non bi-directionnally coded media
			else if (pesh.PTS < pes->PTS) {
				GF_LOG(GF_LOG_WARNING, GF_LOG_CONTAINER, ("[MPEG-2 TS] PID %d - PTS "LLU" less than previous packet PTS "LLU"\n", pes->pid, pesh.PTS, pes->PTS) );
			}
			*/
#endif

			pes->PTS = pesh.PTS;
#ifndef GPAC_DISABLE_LOG
			{
				if (pes->DTS && (pesh.DTS == pes->DTS)) {
					GF_LOG(GF_LOG_WARNING, GF_LOG_CONTAINER, ("[MPEG-2 TS] PID %d - same DTS "LLU" for two consecutive PES packets \n", pes->pid, pes->DTS));
				}
				if (pesh.DTS < pes->DTS) {
					GF_LOG(GF_LOG_WARNING, GF_LOG_CONTAINER, ("[MPEG-2 TS] PID %d - DTS "LLU" less than previous DTS "LLU"\n", pes->pid, pesh.DTS, pes->DTS));
				}
			}
#endif
			pes->DTS = pesh.DTS;
		}
		/*no PTSs were coded, same time*/
		else if (!pesh.hdr_data_len) {
			same_pts = GF_TRUE;
		}


		/*3-byte start-code + 6 bytes header + hdr extensions*/
		len = 9 + pesh.hdr_data_len;

		if ((u8) pes->pck_data[3]==0xfa) {
			GF_M2TS_SL_PCK sl_pck;

			GF_LOG(GF_LOG_DEBUG, GF_LOG_CONTAINER, ("[MPEG-2 TS] SL Packet in PES for %d - ES ID %d\n", pes->pid, pes->mpeg4_es_id));

			if (pes->pck_data_len > len) {
				sl_pck.data = (char *)pes->pck_data + len;
				sl_pck.data_len = pes->pck_data_len - len;
				sl_pck.stream = (GF_M2TS_ES *)pes;
				if (ts->on_event) ts->on_event(ts, GF_M2TS_EVT_SL_PCK, &sl_pck);
			} else {
				GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[MPEG-2 TS] Bad SL Packet size: (%d indicated < %d header)\n", pes->pid, pes->pck_data_len, len));
			}
		} else if (pes->reframe) {
			u32 remain = 0;
			u32 offset = len;

			if (pesh.pck_len && (pesh.pck_len-3-pesh.hdr_data_len != pes->pck_data_len-len)) {
				GF_LOG(GF_LOG_WARNING, GF_LOG_CONTAINER, ("[MPEG-2 TS] PID %d PES payload size %d but received %d bytes\n", pes->pid, (u32) ( pesh.pck_len-3-pesh.hdr_data_len), pes->pck_data_len-len));
			}
			//copy over the remaining of previous PES payload before start of this PES payload
			if (pes->prev_data_len) {
				if (pes->prev_data_len < len) {
					offset = len - pes->prev_data_len;
					memcpy(pes->pck_data + offset, pes->prev_data, pes->prev_data_len);
				} else {
					GF_LOG(GF_LOG_WARNING, GF_LOG_CONTAINER, ("[MPEG-2 TS] PID %d PES reassembly buffer overflow (%d bytes not processed from previous PES) - discarding prev data\n", pes->pid, pes->prev_data_len ));
				}
			}

			if (!pes->temi_pending && pes->temi_tc_desc_len) {
				gf_m2ts_store_temi(ts, pes);
			}

			if (pes->temi_pending) {
				pes->temi_pending = 0;
				pes->temi_tc.pes_pts = pes->PTS;
				if (ts->on_event)
					ts->on_event(ts, GF_M2TS_EVT_TEMI_TIMECODE, &pes->temi_tc);
			}

			if (! ts->start_range)
				remain = pes->reframe(ts, pes, same_pts, pes->pck_data+offset, pes->pck_data_len-offset, &pesh);

			//CLEANUP alloc stuff
			if (pes->prev_data) gf_free(pes->prev_data);
			pes->prev_data = NULL;
			pes->prev_data_len = 0;
			if (remain) {
				pes->prev_data = gf_malloc(sizeof(char)*remain);
				assert(pes->pck_data_len >= remain);
				memcpy(pes->prev_data, pes->pck_data + pes->pck_data_len - remain, remain);
				pes->prev_data_len = remain;
			}
		}
	} else if (pes->pck_data_len) {
		GF_LOG(GF_LOG_WARNING, GF_LOG_CONTAINER, ("[MPEG-2 TS] PES %d: Bad PES Header, discarding packet (maybe stream is encrypted ?)\n", pes->pid));
	}
	pes->pck_data_len = 0;
	pes->pes_len = 0;
	pes->rap = 0;
}

static void gf_m2ts_process_pes(GF_M2TS_Demuxer *ts, GF_M2TS_PES *pes, GF_M2TS_Header *hdr, unsigned char *data, u32 data_size, GF_M2TS_AdaptationField *paf)
{
	u8 expect_cc;
	Bool disc=0;
	Bool flush_pes = 0;

	/*duplicated packet, NOT A DISCONTINUITY, we should discard the packet - however we may encounter this configuration in DASH at segment boundaries.
	If payload start is set, ignore duplication*/
	if (hdr->continuity_counter==pes->cc) {
		if (!hdr->payload_start || (hdr->adaptation_field!=3) ) {
			GF_LOG(GF_LOG_INFO, GF_LOG_CONTAINER, ("[MPEG-2 TS] PES %d: Duplicated Packet found (CC %d) - skipping\n", pes->pid, pes->cc));
			return;
		}
	} else {
		expect_cc = (pes->cc<0) ? hdr->continuity_counter : (pes->cc + 1) & 0xf;
		if (expect_cc != hdr->continuity_counter)
			disc = 1;
	}
	pes->cc = hdr->continuity_counter;

	if (disc) {
		if (pes->flags & GF_M2TS_ES_IGNORE_NEXT_DISCONTINUITY) {
			pes->flags &= ~GF_M2TS_ES_IGNORE_NEXT_DISCONTINUITY;
			disc = 0;
		}
		if (disc) {
			if (hdr->payload_start) {
				if (pes->pck_data_len) {
					GF_LOG(GF_LOG_WARNING, GF_LOG_CONTAINER, ("[MPEG-2 TS] PES %d: Packet discontinuity (%d expected - got %d) - may have lost end of previous PES\n", pes->pid, expect_cc, hdr->continuity_counter));
				}
			} else {
				if (pes->pck_data_len) {
					GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[MPEG-2 TS] PES %d: Packet discontinuity (%d expected - got %d) - trashing PES packet\n", pes->pid, expect_cc, hdr->continuity_counter));
				}
				pes->pck_data_len = 0;
				pes->pes_len = 0;
				pes->cc = -1;
				return;
			}
		}
	}

	if (!pes->reframe) return;

	if (hdr->payload_start) {
		flush_pes = 1;
		pes->pes_start_packet_number = ts->pck_number;
		pes->before_last_pcr_value = pes->program->before_last_pcr_value;
		pes->before_last_pcr_value_pck_number = pes->program->before_last_pcr_value_pck_number;
		pes->last_pcr_value = pes->program->last_pcr_value;
		pes->last_pcr_value_pck_number = pes->program->last_pcr_value_pck_number;
	} else if (pes->pes_len && (pes->pck_data_len + data_size == pes->pes_len + 6)) {
		/* 6 = startcode+stream_id+length*/
		/*reassemble pes*/
		if (pes->pck_data_len + data_size > pes->pck_alloc_len) {
			pes->pck_alloc_len = pes->pck_data_len + data_size;
			pes->pck_data = (u8*)gf_realloc(pes->pck_data, pes->pck_alloc_len);
		}
		memcpy(pes->pck_data+pes->pck_data_len, data, data_size);
		pes->pck_data_len += data_size;
		/*force discard*/
		data_size = 0;
		flush_pes = 1;
	}

	/*PES first fragment: flush previous packet*/
	if (flush_pes && pes->pck_data_len) {
		gf_m2ts_flush_pes(ts, pes);
		if (!data_size) return;
	}
	/*we need to wait for first packet of PES*/
	if (!pes->pck_data_len && !hdr->payload_start) {
		GF_LOG(GF_LOG_DEBUG, GF_LOG_CONTAINER, ("[MPEG-2 TS] PID %d: Waiting for PES header, trashing data\n", hdr->pid));
		return;
	}
	/*reassemble*/
	if (pes->pck_data_len + data_size > pes->pck_alloc_len ) {
		pes->pck_alloc_len = pes->pck_data_len + data_size;
		pes->pck_data = (u8*)gf_realloc(pes->pck_data, pes->pck_alloc_len);
	}
	memcpy(pes->pck_data + pes->pck_data_len, data, data_size);
	pes->pck_data_len += data_size;

	if (paf && paf->random_access_indicator) pes->rap = 1;
	if (hdr->payload_start && !pes->pes_len && (pes->pck_data_len>=6)) {
		pes->pes_len = (pes->pck_data[4]<<8) | pes->pck_data[5];
		GF_LOG(GF_LOG_DEBUG, GF_LOG_CONTAINER, ("[MPEG-2 TS] PID %d: Got PES packet len %d\n", pes->pid, pes->pes_len));

		if (pes->pes_len + 6 == pes->pck_data_len) {
			gf_m2ts_flush_pes(ts, pes);
		}
	}
}


static void gf_m2ts_get_adaptation_field(GF_M2TS_Demuxer *ts, GF_M2TS_AdaptationField *paf, unsigned char *data, u32 size, u32 pid)
{
	unsigned char *af_extension;
	paf->discontinuity_indicator = (data[0] & 0x80) ? 1 : 0;
	paf->random_access_indicator = (data[0] & 0x40) ? 1 : 0;
	paf->priority_indicator = (data[0] & 0x20) ? 1 : 0;
	paf->PCR_flag = (data[0] & 0x10) ? 1 : 0;
	paf->OPCR_flag = (data[0] & 0x8) ? 1 : 0;
	paf->splicing_point_flag = (data[0] & 0x4) ? 1 : 0;
	paf->transport_private_data_flag = (data[0] & 0x2) ? 1 : 0;
	paf->adaptation_field_extension_flag = (data[0] & 0x1) ? 1 : 0;

	af_extension = data + 1;
	if (paf->PCR_flag == 1) {
		u32 base = (data[1] << 24) | (data[2] << 16) | (data[3] << 8) | data[4];
		u64 PCR = (u64) base;
		paf->PCR_base = (PCR << 1) | (data[5] >> 7);
		paf->PCR_ext = ((data[5] & 1) << 8) | data[6];
		af_extension += 6;
	}

	if (paf->adaptation_field_extension_flag) {
		u32 afext_bytes;
		Bool ltw_flag, pwr_flag, seamless_flag, af_desc_not_present;
		if (paf->OPCR_flag) {
			af_extension += 6;
		}
		if (paf->splicing_point_flag) {
			af_extension += 1;
		}
		if (paf->transport_private_data_flag) {
			u32 priv_bytes = af_extension[0];
			af_extension += 1 + priv_bytes;
		}

		afext_bytes = af_extension[0];
		ltw_flag = af_extension[1] & 0x80 ? 1 : 0;
		pwr_flag = af_extension[1] & 0x40 ? 1 : 0;
		seamless_flag = af_extension[1] & 0x20 ? 1 : 0;
		af_desc_not_present = af_extension[1] & 0x10 ? 1 : 0;
		af_extension += 2;
		if (!afext_bytes) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[MPEG-2 TS] PID %d: Bad Adaptation Extension found\n", pid));
			return;
		}
		afext_bytes-=1;
		if (ltw_flag) {
			af_extension += 2;
			if (afext_bytes<2) {
				GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[MPEG-2 TS] PID %d: Bad Adaptation Extension found\n", pid));
				return;
			}
			afext_bytes-=2;
		}
		if (pwr_flag) {
			af_extension += 3;
			if (afext_bytes<3) {
				GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[MPEG-2 TS] PID %d: Bad Adaptation Extension found\n", pid));
				return;
			}
			afext_bytes-=3;
		}
		if (seamless_flag) {
			af_extension += 3;
			if (afext_bytes<3) {
				GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[MPEG-2 TS] PID %d: Bad Adaptation Extension found\n", pid));
				return;
			}
			afext_bytes-=3;
		}

		if (! af_desc_not_present) {
			while (afext_bytes) {
				GF_BitStream *bs;
				char *desc;
				u8 desc_tag = af_extension[0];
				u8 desc_len = af_extension[1];
				if (!desc_len || (u32) desc_len+2 > afext_bytes) {
					GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[MPEG-2 TS] PID %d: Bad Adaptation Descriptor found (tag %d) size is %d but only %d bytes available\n", pid, desc_tag, desc_len, afext_bytes));
					break;
				}
				desc = (char *) af_extension+2;

				bs = gf_bs_new(desc, desc_len, GF_BITSTREAM_READ);
				switch (desc_tag) {
				case GF_M2TS_AFDESC_LOCATION_DESCRIPTOR:
				{
					Bool use_base_temi_url;
					char URL[255];
					GF_M2TS_TemiLocationDescriptor temi_loc;
					memset(&temi_loc, 0, sizeof(GF_M2TS_TemiLocationDescriptor) );
					temi_loc.reload_external = gf_bs_read_int(bs, 1);
					temi_loc.is_announce = gf_bs_read_int(bs, 1);
					temi_loc.is_splicing = gf_bs_read_int(bs, 1);
					use_base_temi_url = gf_bs_read_int(bs, 1);
					gf_bs_read_int(bs, 5); //reserved
					temi_loc.timeline_id = gf_bs_read_int(bs, 7);
					if (!use_base_temi_url) {
						char *_url = URL;
						u8 scheme = gf_bs_read_int(bs, 8);
						u8 url_len = gf_bs_read_int(bs, 8);
						switch (scheme) {
						case 1:
							strcpy(URL, "http://");
							_url = URL+7;
							break;
						case 2:
							strcpy(URL, "https://");
							_url = URL+8;
							break;
						}
						gf_bs_read_data(bs, _url, url_len);
						_url[url_len] = 0;
					}
					temi_loc.external_URL = URL;

					GF_LOG(GF_LOG_INFO, GF_LOG_CONTAINER, ("[MPEG-2 TS] PID %d AF Location descriptor found - URL %s\n", pid, URL));
					if (ts->on_event) ts->on_event(ts, GF_M2TS_EVT_TEMI_LOCATION, &temi_loc);
				}
				break;
				case GF_M2TS_AFDESC_TIMELINE_DESCRIPTOR:
					if (ts->ess[pid] && (ts->ess[pid]->flags & GF_M2TS_ES_IS_PES)) {
						GF_M2TS_PES *pes = (GF_M2TS_PES *) ts->ess[pid];

						if (pes->temi_tc_desc_len)
							gf_m2ts_store_temi(ts, pes);

						if (pes->temi_tc_desc_alloc_size < desc_len) {
							pes->temi_tc_desc = gf_realloc(pes->temi_tc_desc, desc_len);
							pes->temi_tc_desc_alloc_size = desc_len;
						}
						memcpy(pes->temi_tc_desc, desc, desc_len);
						pes->temi_tc_desc_len = desc_len;

						GF_LOG(GF_LOG_DEBUG, GF_LOG_CONTAINER, ("[MPEG-2 TS] PID %d AF Timeline descriptor found\n", pid));
					}
					break;
				}
				gf_bs_del(bs);

				af_extension += 2+desc_len;
				afext_bytes -= 2+desc_len;
			}

		}
	}

	GF_LOG(GF_LOG_DEBUG, GF_LOG_CONTAINER, ("[MPEG-2 TS] PID %d: Adaptation Field found: Discontinuity %d - RAP %d - PCR: "LLD"\n", pid, paf->discontinuity_indicator, paf->random_access_indicator, paf->PCR_flag ? paf->PCR_base * 300 + paf->PCR_ext : 0));
}

static GF_Err gf_m2ts_process_packet(GF_M2TS_Demuxer *ts, unsigned char *data)
{
	GF_M2TS_ES *es;
	GF_M2TS_Header hdr;
	GF_M2TS_AdaptationField af, *paf;
	u32 payload_size, af_size;
	u32 pos = 0;

	ts->pck_number++;

	/* read TS packet header*/
	hdr.sync = data[0];
	if (hdr.sync != 0x47) {
		GF_LOG(GF_LOG_WARNING, GF_LOG_CONTAINER, ("[MPEG-2 TS] TS Packet %d does not start with sync marker\n", ts->pck_number));
		return GF_CORRUPTED_DATA;
	}
	hdr.error = (data[1] & 0x80) ? 1 : 0;
	hdr.payload_start = (data[1] & 0x40) ? 1 : 0;
	hdr.priority = (data[1] & 0x20) ? 1 : 0;
	hdr.pid = ( (data[1]&0x1f) << 8) | data[2];
	hdr.scrambling_ctrl = (data[3] >> 6) & 0x3;
	hdr.adaptation_field = (data[3] >> 4) & 0x3;
	hdr.continuity_counter = data[3] & 0xf;

	if (hdr.error) {
		GF_LOG(GF_LOG_WARNING, GF_LOG_CONTAINER, ("[MPEG-2 TS] TS Packet %d has error (PID could be %d)\n", ts->pck_number, hdr.pid));
		return GF_CORRUPTED_DATA;
	}
//#if DEBUG_TS_PACKET
	GF_LOG(GF_LOG_DEBUG, GF_LOG_CONTAINER, ("[MPEG-2 TS] TS Packet %d PID %d CC %d Encrypted %d\n", ts->pck_number, hdr.pid, hdr.continuity_counter, hdr.scrambling_ctrl));
//#endif

	if (hdr.scrambling_ctrl) {
		//TODO add decyphering
		GF_LOG(GF_LOG_WARNING, GF_LOG_CONTAINER, ("[MPEG-2 TS] TS Packet %d is scrambled - not supported\n", ts->pck_number, hdr.pid));
		return GF_NOT_SUPPORTED;
	}

	paf = NULL;
	payload_size = 184;
	pos = 4;
	switch (hdr.adaptation_field) {
	/*adaptation+data*/
	case 3:
		af_size = data[4];
		if (af_size>183) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[MPEG-2 TS] TS Packet %d AF field larger than 183 !\n", ts->pck_number));
			//error
			return GF_CORRUPTED_DATA;
		}
		paf = &af;
		memset(paf, 0, sizeof(GF_M2TS_AdaptationField));
		//this will stop you when processing invalid (yet existing) mpeg2ts streams in debug
		assert( af_size<=183);
		if (af_size>183)
			GF_LOG(GF_LOG_WARNING, GF_LOG_CONTAINER, ("[MPEG-2 TS] TS Packet %d Detected wrong adaption field size %u when control value is 3\n", ts->pck_number, af_size));
		if (af_size) gf_m2ts_get_adaptation_field(ts, paf, data+5, af_size, hdr.pid);
		pos += 1+af_size;
		payload_size = 183 - af_size;
		break;
	/*adaptation only - still process in case of PCR*/
	case 2:
		af_size = data[4];
		if (af_size != 183) {
			GF_LOG(GF_LOG_WARNING, GF_LOG_CONTAINER, ("[MPEG-2 TS] TS Packet %d AF size is %d when it must be 183 for AF type 2\n", ts->pck_number, af_size));
			return GF_CORRUPTED_DATA;
		}
		paf = &af;
		memset(paf, 0, sizeof(GF_M2TS_AdaptationField));
		gf_m2ts_get_adaptation_field(ts, paf, data+5, af_size, hdr.pid);
		payload_size = 0;
		/*no payload and no PCR, return*/
		if (!paf->PCR_flag)
			return GF_OK;
		break;
	/*reserved*/
	case 0:
		return GF_OK;
	default:
		break;
	}
	data += pos;

	/*PAT*/
	if (hdr.pid == GF_M2TS_PID_PAT) {
		gf_m2ts_gather_section(ts, ts->pat, NULL, &hdr, data, payload_size);
		return GF_OK;
	} else if (hdr.pid == GF_M2TS_PID_CAT) {
		gf_m2ts_gather_section(ts, ts->cat, NULL, &hdr, data, payload_size);
		return GF_OK;
	}

	es = ts->ess[hdr.pid];
	if (paf && paf->PCR_flag) {
		if (!es) {
			u32 i, j;
			for(i=0; i<gf_list_count(ts->programs); i++) {
				GF_M2TS_PES *first_pes = NULL;
				GF_M2TS_Program *program = (GF_M2TS_Program *)gf_list_get(ts->programs,i);
				if(program->pcr_pid != hdr.pid) continue;
				for (j=0; j<gf_list_count(program->streams); j++) {
					GF_M2TS_PES *pes = (GF_M2TS_PES *) gf_list_get(program->streams, j);
					if (pes->flags & GF_M2TS_INHERIT_PCR) {
						ts->ess[hdr.pid] = (GF_M2TS_ES *) pes;
						pes->flags |= GF_M2TS_FAKE_PCR;
						break;
					}
					if (pes->flags & GF_M2TS_ES_IS_PES) {
						first_pes = pes;
					}
				}
				//non found, use the first media stream as a PCR destination - Q: is it legal to have PCR only streams not declared in PMT ?
				if (!es && first_pes) {
					es = (GF_M2TS_ES *) first_pes;
					first_pes->flags |= GF_M2TS_FAKE_PCR;
				}
				break;
			}
			if (!es)
				es = ts->ess[hdr.pid];
		}
		if (es) {
			GF_M2TS_PES_PCK pck;
			s64 prev_diff_in_us;
			Bool discontinuity;
			s32 cc = -1;

			if (es->flags & GF_M2TS_FAKE_PCR) {
				cc = es->program->pcr_cc;
				es->program->pcr_cc = hdr.continuity_counter;
			}
			else if (es->flags & GF_M2TS_ES_IS_PES) cc = ((GF_M2TS_PES*)es)->cc;
			else if (((GF_M2TS_SECTION_ES*)es)->sec) cc = ((GF_M2TS_SECTION_ES*)es)->sec->cc;

			discontinuity = paf->discontinuity_indicator;
			if ((cc>=0) && es->program->before_last_pcr_value) {
				//no increment of CC if AF only packet
				if (hdr.adaptation_field == 2) {
					if (hdr.continuity_counter != cc) {
						discontinuity = GF_TRUE;
					}
				} else if (hdr.continuity_counter != ((cc + 1) & 0xF)) {
					discontinuity = GF_TRUE;
				}
			}

			memset(&pck, 0, sizeof(GF_M2TS_PES_PCK));
			prev_diff_in_us = (s64) (es->program->last_pcr_value /27- es->program->before_last_pcr_value/27);
			es->program->before_last_pcr_value = es->program->last_pcr_value;
			es->program->before_last_pcr_value_pck_number = es->program->last_pcr_value_pck_number;
			es->program->last_pcr_value_pck_number = ts->pck_number;
			es->program->last_pcr_value = paf->PCR_base * 300 + paf->PCR_ext;
			if (!es->program->last_pcr_value) es->program->last_pcr_value =  1;

			GF_LOG(GF_LOG_DEBUG, GF_LOG_CONTAINER, ("[MPEG-2 TS] PID %d PCR found "LLU" ("LLU" at 90kHz) - PCR diff is %d us\n", hdr.pid, es->program->last_pcr_value, es->program->last_pcr_value/300, (s32) (es->program->last_pcr_value - es->program->before_last_pcr_value)/27 ));

			pck.PTS = es->program->last_pcr_value;
			pck.stream = (GF_M2TS_PES *)es;

			//try to ignore all discontinuities that are less than 200 ms (seen in some HLS setup ...)
			if (discontinuity) {
				s64 diff_in_us = (s64) (es->program->last_pcr_value - es->program->before_last_pcr_value) / 27;
				u64 diff = ABS(diff_in_us - prev_diff_in_us);

				if ((diff_in_us<0) && (diff_in_us >= -200000)) {
					GF_LOG(GF_LOG_WARNING, GF_LOG_CONTAINER, ("[MPEG-2 TS] PID %d new PCR, with discontinuity signaled, is less than previously received PCR (diff %d us) but not too large, trying to ignore discontinuity\n", hdr.pid, diff_in_us));
				}

				//ignore PCR discontinuity indicator if PCR found is larger than previously received PCR and diffence between PCR before and after discontinuity indicator is smaller than 50ms
				else if ((diff_in_us > 0) && (diff < 200000)) {
					GF_LOG(GF_LOG_DEBUG, GF_LOG_CONTAINER, ("[MPEG-2 TS] PID %d PCR discontinuity signaled but diff is small (diff %d us - PCR diff %d vs prev PCR diff %d) - ignore it\n", hdr.pid, diff, diff_in_us, prev_diff_in_us));
				} else {
					GF_LOG(GF_LOG_DEBUG, GF_LOG_CONTAINER, ("[MPEG-2 TS] PID %d PCR discontinuity signaled (diff %d us - PCR diff %d vs prev PCR diff %d)\n", hdr.pid, diff, diff_in_us, prev_diff_in_us));
					pck.flags = GF_M2TS_PES_PCK_DISCONTINUITY;
				}
			}
			else if ( (es->program->last_pcr_value < es->program->before_last_pcr_value) ) {
				s64 diff_in_us = (s64) (es->program->last_pcr_value - es->program->before_last_pcr_value) / 27;
				//if less than 200 ms before PCR loop at the last PCR, this is a PCR loop
				if (GF_M2TS_MAX_PCR - es->program->before_last_pcr_value < 5400000 /*2*2700000*/) {
					GF_LOG(GF_LOG_INFO, GF_LOG_CONTAINER, ("[MPEG-2 TS] PID %d PCR loop found from "LLU" to "LLU" \n", hdr.pid, es->program->before_last_pcr_value, es->program->last_pcr_value));
				} else if ((diff_in_us<0) && (diff_in_us >= -200000)) {
					GF_LOG(GF_LOG_WARNING, GF_LOG_CONTAINER, ("[MPEG-2 TS] PID %d new PCR, without discontinuity signaled, is less than previously received PCR (diff %d us) but not too large, trying to ignore discontinuity\n", hdr.pid, diff_in_us));
				} else {
					GF_LOG(GF_LOG_WARNING, GF_LOG_CONTAINER, ("[MPEG-2 TS] PID %d PCR found "LLU" is less than previously received PCR "LLU" (PCR diff %g sec) but no discontinuity signaled\n", hdr.pid, es->program->last_pcr_value, es->program->before_last_pcr_value, (GF_M2TS_MAX_PCR - es->program->before_last_pcr_value + es->program->last_pcr_value) / 27000000.0));

					pck.flags = GF_M2TS_PES_PCK_DISCONTINUITY;
				}
			}

			if (pck.flags & GF_M2TS_PES_PCK_DISCONTINUITY) {
				gf_m2ts_reset_parsers_for_program(ts, es->program);
			}

			if (ts->on_event) {
				gf_m2ts_estimate_duration(ts, es->program->last_pcr_value, hdr.pid);
				ts->on_event(ts, GF_M2TS_EVT_PES_PCR, &pck);
			}
		}
	}

	/*check for DVB reserved PIDs*/
	if (!es) {
		if (hdr.pid == GF_M2TS_PID_SDT_BAT_ST) {
			gf_m2ts_gather_section(ts, ts->sdt, NULL, &hdr, data, payload_size);
			return GF_OK;
		} else if (hdr.pid == GF_M2TS_PID_NIT_ST) {
			/*ignore them, unused at application level*/
			gf_m2ts_gather_section(ts, ts->nit, NULL, &hdr, data, payload_size);
			return GF_OK;
		} else if (hdr.pid == GF_M2TS_PID_EIT_ST_CIT) {
			/* ignore EIT messages for the moment */
			gf_m2ts_gather_section(ts, ts->eit, NULL, &hdr, data, payload_size);
			return GF_OK;
		} else if (hdr.pid == GF_M2TS_PID_TDT_TOT_ST) {
			gf_m2ts_gather_section(ts, ts->tdt_tot, NULL, &hdr, data, payload_size);
		} else {
			/* ignore packet */
		}
	} else if (es->flags & GF_M2TS_ES_IS_SECTION) { 	/* The stream uses sections to carry its payload */
		GF_M2TS_SECTION_ES *ses = (GF_M2TS_SECTION_ES *)es;
		if (ses->sec) gf_m2ts_gather_section(ts, ses->sec, ses, &hdr, data, payload_size);
	} else {
		GF_M2TS_PES *pes = (GF_M2TS_PES *)es;
		/* regular stream using PES packets */
		if (pes->reframe && payload_size) gf_m2ts_process_pes(ts, pes, &hdr, data, payload_size, paf);
	}

	return GF_OK;
}

GF_EXPORT
GF_Err gf_m2ts_process_data(GF_M2TS_Demuxer *ts, char *data, u32 data_size)
{
	GF_Err e;
	u32 pos, pck_size;
	Bool is_align = 1;
	if (ts->buffer) {
		if (ts->alloc_size < ts->buffer_size+data_size) {
			ts->alloc_size = ts->buffer_size+data_size;
			ts->buffer = (char*)gf_realloc(ts->buffer, sizeof(char)*ts->alloc_size);
		}
		memcpy(ts->buffer + ts->buffer_size, data, sizeof(char)*data_size);
		ts->buffer_size += data_size;
		is_align = 0;
	} else {
		ts->buffer = data;
		ts->buffer_size = data_size;
	}

	/*sync input data*/
	pos = gf_m2ts_sync(ts, is_align);
	if (pos==ts->buffer_size) {
		if (is_align) {
			ts->buffer = (char*)gf_malloc(sizeof(char)*data_size);
			memcpy(ts->buffer, data, sizeof(char)*data_size);
			ts->alloc_size = ts->buffer_size = data_size;
		}
		return GF_OK;
	}
	pck_size = ts->prefix_present ? 192 : 188;
	e=GF_OK;
	for (;;) {
		/*wait for a complete packet*/
		if (ts->buffer_size < pos  + pck_size) {
			ts->buffer_size -= pos;
			if (!ts->buffer_size) {
				if (!is_align) gf_free(ts->buffer);
				ts->buffer = NULL;
				return e;
			}
			if (is_align) {
				data = ts->buffer+pos;
				ts->buffer = (char*)gf_malloc(sizeof(char)*ts->buffer_size);
				memcpy(ts->buffer, data, sizeof(char)*ts->buffer_size);
				ts->alloc_size = ts->buffer_size;
			} else {
				memmove(ts->buffer, ts->buffer + pos, sizeof(char)*ts->buffer_size);
			}
			return e;
		}
		/*process*/
		e |= gf_m2ts_process_packet(ts, (unsigned char *)ts->buffer+pos);
		pos += pck_size;

		if (ts->abort_parsing) {
			if (!is_align) gf_free(ts->buffer);
			ts->buffer = NULL;
			return e;
		}
	}
	return e;
}

GF_EXPORT
GF_ESD *gf_m2ts_get_esd(GF_M2TS_ES *es)
{
	GF_ESD *esd;
	u32 k, esd_count;

	esd = NULL;
	if (es->program->pmt_iod && es->program->pmt_iod->ESDescriptors) {
		esd_count = gf_list_count(es->program->pmt_iod->ESDescriptors);
		for (k = 0; k < esd_count; k++) {
			GF_ESD *esd_tmp = (GF_ESD *)gf_list_get(es->program->pmt_iod->ESDescriptors, k);
			if (esd_tmp->ESID != es->mpeg4_es_id) continue;
			esd = esd_tmp;
			break;
		}
	}

	if (!esd && es->program->additional_ods) {
		u32 od_count, od_index;
		od_count = gf_list_count(es->program->additional_ods);
		for (od_index = 0; od_index < od_count; od_index++) {
			GF_ObjectDescriptor *od = (GF_ObjectDescriptor *)gf_list_get(es->program->additional_ods, od_index);
			esd_count = gf_list_count(od->ESDescriptors);
			for (k = 0; k < esd_count; k++) {
				GF_ESD *esd_tmp = (GF_ESD *)gf_list_get(od->ESDescriptors, k);
				if (esd_tmp->ESID != es->mpeg4_es_id) continue;
				esd = esd_tmp;
				break;
			}
		}
	}

	return esd;
}

void gf_m2ts_set_segment_switch(GF_M2TS_Demuxer *ts)
{
	u32 i;
	for (i=0; i<GF_M2TS_MAX_STREAMS; i++) {
		GF_M2TS_ES *es = (GF_M2TS_ES *) ts->ess[i];
		if (!es) continue;
		es->flags |= GF_M2TS_ES_IGNORE_NEXT_DISCONTINUITY;
	}
}

GF_EXPORT
void gf_m2ts_reset_parsers_for_program(GF_M2TS_Demuxer *ts, GF_M2TS_Program *prog)
{
	u32 i;

	for (i=0; i<GF_M2TS_MAX_STREAMS; i++) {
		GF_M2TS_ES *es = (GF_M2TS_ES *) ts->ess[i];
		if (!es) continue;
		if (prog && (es->program != prog) ) continue;

		if (es->flags & GF_M2TS_ES_IS_SECTION) {
			GF_M2TS_SECTION_ES *ses = (GF_M2TS_SECTION_ES *)es;
			gf_m2ts_section_filter_reset(ses->sec);
		} else {
			GF_M2TS_PES *pes = (GF_M2TS_PES *)es;
			if (!pes || (pes->pid==pes->program->pmt_pid)) continue;
			pes->cc = -1;
			pes->frame_state = 0;
			pes->pck_data_len = 0;
			if (pes->prev_data) gf_free(pes->prev_data);
			pes->prev_data = NULL;
			pes->prev_data_len = 0;
			pes->PTS = pes->DTS = 0;
//			pes->prev_PTS = 0;
//			pes->first_dts = 0;
			pes->pes_len = pes->pes_end_packet_number = pes->pes_start_packet_number = 0;
			if (pes->buf) gf_free(pes->buf);
			pes->buf = NULL;
			if (pes->temi_tc_desc) gf_free(pes->temi_tc_desc);
			pes->temi_tc_desc = NULL;
			pes->temi_tc_desc_len = pes->temi_tc_desc_alloc_size = 0;

			pes->before_last_pcr_value = pes->before_last_pcr_value_pck_number = 0;
			pes->last_pcr_value = pes->last_pcr_value_pck_number = 0;
			if (pes->program->pcr_pid==pes->pid) {
				pes->program->last_pcr_value = pes->program->last_pcr_value_pck_number = 0;
				pes->program->before_last_pcr_value = pes->program->before_last_pcr_value_pck_number = 0;
			}
		}
	}
}

GF_EXPORT
void gf_m2ts_reset_parsers(GF_M2TS_Demuxer *ts)
{
	gf_m2ts_reset_parsers_for_program(ts, NULL);

	ts->pck_number = 0;

	gf_m2ts_section_filter_reset(ts->cat);
	gf_m2ts_section_filter_reset(ts->pat);
	gf_m2ts_section_filter_reset(ts->sdt);
	gf_m2ts_section_filter_reset(ts->nit);
	gf_m2ts_section_filter_reset(ts->eit);
	gf_m2ts_section_filter_reset(ts->tdt_tot);

}

static void gf_m2ts_process_section_discard(GF_M2TS_Demuxer *ts, GF_M2TS_SECTION_ES *es, GF_List *sections, u8 table_id, u16 ex_table_id, u8 version_number, u8 last_section_number, u32 status)
{
}

GF_EXPORT
u32 gf_m2ts_pes_get_framing_mode(GF_M2TS_PES *pes)
{
	if (pes->flags & GF_M2TS_ES_IS_SECTION) {
		if (pes->flags & GF_M2TS_ES_IS_SL) {
			if ( ((GF_M2TS_SECTION_ES *)pes)->sec->process_section == gf_m2ts_process_section_discard)
				return GF_M2TS_PES_FRAMING_DEFAULT;

		}
		return GF_M2TS_PES_FRAMING_SKIP_NO_RESET;
	}

	if (!pes->reframe ) return GF_M2TS_PES_FRAMING_SKIP_NO_RESET;
	if (pes->reframe == gf_m2ts_reframe_default) return GF_M2TS_PES_FRAMING_RAW;
	if (pes->reframe == gf_m2ts_reframe_reset) return GF_M2TS_PES_FRAMING_SKIP;
	if (pes->single_nal_mode) return GF_M2TS_PES_FRAMING_DEFAULT_NAL;
	return GF_M2TS_PES_FRAMING_DEFAULT;
}

GF_EXPORT
GF_Err gf_m2ts_set_pes_framing(GF_M2TS_PES *pes, u32 mode)
{
	if (!pes) return GF_BAD_PARAM;

	GF_LOG(GF_LOG_DEBUG, GF_LOG_CONTAINER, ("[MPEG-2 TS] Setting pes framing mode of PID %d to %d\n", pes->pid, mode) );
	/*ignore request for section PIDs*/
	if (pes->flags & GF_M2TS_ES_IS_SECTION) {
		if (pes->flags & GF_M2TS_ES_IS_SL) {
			if (mode==GF_M2TS_PES_FRAMING_DEFAULT) {
				((GF_M2TS_SECTION_ES *)pes)->sec->process_section = gf_m2ts_process_mpeg4section;
			} else {
				((GF_M2TS_SECTION_ES *)pes)->sec->process_section = gf_m2ts_process_section_discard;
			}
		}
		return GF_OK;
	}

	if (pes->pid==pes->program->pmt_pid) return GF_BAD_PARAM;

	//if component reuse, disable previous pes
	if ((mode > GF_M2TS_PES_FRAMING_SKIP) && (pes->program->ts->ess[pes->pid] != (GF_M2TS_ES *) pes)) {
		GF_M2TS_PES *o_pes = (GF_M2TS_PES *) pes->program->ts->ess[pes->pid];
		if (o_pes->flags & GF_M2TS_ES_IS_PES)
			gf_m2ts_set_pes_framing(o_pes, GF_M2TS_PES_FRAMING_SKIP);

		GF_LOG(GF_LOG_INFO, GF_LOG_CONTAINER, ("[MPEG-2 TS] Reassinging PID %d from program %d to program %d\n", pes->pid, o_pes->program->number, pes->program->number) );
		pes->program->ts->ess[pes->pid] = (GF_M2TS_ES *) pes;
	}

	switch (mode) {
	case GF_M2TS_PES_FRAMING_RAW:
		pes->reframe = gf_m2ts_reframe_default;
		break;
	case GF_M2TS_PES_FRAMING_SKIP:
		pes->reframe = gf_m2ts_reframe_reset;
		break;
	case GF_M2TS_PES_FRAMING_SKIP_NO_RESET:
		pes->reframe = NULL;
		break;
	case GF_M2TS_PES_FRAMING_DEFAULT_NAL:
	case GF_M2TS_PES_FRAMING_DEFAULT:
	default:
		switch (pes->stream_type) {
		case GF_M2TS_VIDEO_MPEG1:
		case GF_M2TS_VIDEO_MPEG2:
			pes->reframe = gf_m2ts_reframe_mpeg_video;
			break;
		case GF_M2TS_VIDEO_H264:
		case GF_M2TS_VIDEO_SVC:
			pes->reframe = gf_m2ts_reframe_avc_h264;
			pes->single_nal_mode = (mode==GF_M2TS_PES_FRAMING_DEFAULT_NAL) ? 1 : 0;
			break;
		case GF_M2TS_VIDEO_HEVC:
		case GF_M2TS_VIDEO_HEVC_TEMPORAL:
		case GF_M2TS_VIDEO_HEVC_MCTS:
		case GF_M2TS_VIDEO_SHVC:
		case GF_M2TS_VIDEO_SHVC_TEMPORAL:
		case GF_M2TS_VIDEO_MHVC:
		case GF_M2TS_VIDEO_MHVC_TEMPORAL:
			pes->reframe = gf_m2ts_reframe_hevc;
			pes->single_nal_mode = (mode==GF_M2TS_PES_FRAMING_DEFAULT_NAL) ? 1 : 0;
			break;
#ifndef GPAC_DISABLE_AV_PARSERS
		case GF_M2TS_AUDIO_MPEG1:
		case GF_M2TS_AUDIO_MPEG2:
			pes->reframe = gf_m2ts_reframe_mpeg_audio;
			break;
		case GF_M2TS_AUDIO_AAC:
			pes->reframe = gf_m2ts_reframe_aac_adts;
			break;
		case GF_M2TS_AUDIO_LATM_AAC:
			pes->reframe = gf_m2ts_reframe_aac_latm;
			break;
		case GF_M2TS_AUDIO_AC3:
			pes->reframe = gf_m2ts_reframe_ac3;
			break;
		case GF_M2TS_AUDIO_EC3:
			pes->reframe = gf_m2ts_reframe_ec3;
			break;
#endif

		case GF_M2TS_PRIVATE_DATA:
			/* TODO: handle DVB subtitle streams */
			break;
		case GF_M2TS_METADATA_ID3_HLS:
			pes->reframe = gf_m2ts_reframe_id3_pes;
			break;
		default:
			pes->reframe = gf_m2ts_reframe_default;
			break;
		}
		break;
	}
	return GF_OK;
}

GF_EXPORT
GF_M2TS_Demuxer *gf_m2ts_demux_new()
{
	GF_M2TS_Demuxer *ts;

	GF_SAFEALLOC(ts, GF_M2TS_Demuxer);
	if (!ts) return NULL;
	ts->programs = gf_list_new();
	ts->SDTs = gf_list_new();

	ts->pat = gf_m2ts_section_filter_new(gf_m2ts_process_pat, 0);
	ts->cat = gf_m2ts_section_filter_new(gf_m2ts_process_cat, 0);
	ts->sdt = gf_m2ts_section_filter_new(gf_m2ts_process_sdt, 1);
	ts->nit = gf_m2ts_section_filter_new(gf_m2ts_process_nit, 0);
	ts->eit = gf_m2ts_section_filter_new(NULL/*gf_m2ts_process_eit*/, 1);
	ts->tdt_tot = gf_m2ts_section_filter_new(gf_m2ts_process_tdt_tot, 1);

#ifdef GPAC_ENABLE_MPE
	gf_dvb_mpe_init(ts);
#endif

	ts->requested_progs = gf_list_new();
	ts->requested_pids = gf_list_new();
	ts->demux_and_play = 0;
	ts->nb_prog_pmt_received = 0;
	ts->ChannelAppList = gf_list_new();
	ts->udp_buffer_size = GF_M2TS_UDP_BUFFER_SIZE;

	return ts;
}

GF_EXPORT
void gf_m2ts_abort_parsing(GF_M2TS_Demuxer *ts, Bool force_reset_pes)
{
	u32 i, j, count, count2;

	if (force_reset_pes) {
		count = gf_list_count(ts->programs);
		for (i=0; i<count; i++) {
			GF_M2TS_Program *prog = (GF_M2TS_Program *)gf_list_get(ts->programs, i);
			count2 = gf_list_count(prog->streams);
			for (j=0; j<count2; j++) {
				GF_M2TS_PES *pes = (GF_M2TS_PES *)gf_list_get(prog->streams, j);
				if (pes)
					pes->pck_data_len = 0;
			}
		}
	}
	ts->abort_parsing = GF_TRUE;
}

GF_EXPORT
void gf_m2ts_demux_dmscc_init(GF_M2TS_Demuxer *ts) {

	char* temp_dir;
	u32 length;
	GF_Err e;

	ts->dsmcc_controler = gf_list_new();
	ts->process_dmscc = 1;

	temp_dir = gf_get_default_cache_directory();
	length = (u32) strlen(temp_dir);
	if(temp_dir[length-1] == GF_PATH_SEPARATOR) {
		temp_dir[length-1] = 0;
	}

	ts->dsmcc_root_dir = (char*)gf_calloc(strlen(temp_dir)+strlen("CarouselData")+2,sizeof(char));
	sprintf(ts->dsmcc_root_dir,"%s%cCarouselData",temp_dir,GF_PATH_SEPARATOR);
	e = gf_mkdir(ts->dsmcc_root_dir);
	if(e) {
		GF_LOG(GF_LOG_INFO, GF_LOG_CONTAINER, ("[Process DSMCC] Error during the creation of the directory %s \n",ts->dsmcc_root_dir));
	}

}

GF_EXPORT
void gf_m2ts_demux_del(GF_M2TS_Demuxer *ts)
{
	u32 i;
	if (ts->pat) gf_m2ts_section_filter_del(ts->pat);
	if (ts->cat) gf_m2ts_section_filter_del(ts->cat);
	if (ts->sdt) gf_m2ts_section_filter_del(ts->sdt);
	if (ts->nit) gf_m2ts_section_filter_del(ts->nit);
	if (ts->eit) gf_m2ts_section_filter_del(ts->eit);
	if (ts->tdt_tot) gf_m2ts_section_filter_del(ts->tdt_tot);

	for (i=0; i<GF_M2TS_MAX_STREAMS; i++) {
		//bacause of pure PCR streams, en ES might be reassigned on 2 PIDs, one for the ES and one for the PCR
		if (ts->ess[i] && (ts->ess[i]->pid==i)) gf_m2ts_es_del(ts->ess[i], ts);
	}
	if (ts->buffer) gf_free(ts->buffer);
	while (gf_list_count(ts->programs)) {
		GF_M2TS_Program *p = (GF_M2TS_Program *)gf_list_last(ts->programs);
		gf_list_rem_last(ts->programs);
		gf_list_del(p->streams);
		/*reset OD list*/
		if (p->additional_ods) {
			gf_odf_desc_list_del(p->additional_ods);
			gf_list_del(p->additional_ods);
		}
		if (p->pmt_iod) gf_odf_desc_del((GF_Descriptor *)p->pmt_iod);
		gf_free(p);
	}
	gf_list_del(ts->programs);

	if (ts->TDT_time) gf_free(ts->TDT_time);
	gf_m2ts_reset_sdt(ts);
	if (ts->tdt_tot)
		gf_list_del(ts->SDTs);

#ifdef GPAC_ENABLE_MPE
	gf_dvb_mpe_shutdown(ts);
#endif

	if(gf_list_count(ts->dsmcc_controler)) {
#ifdef GPAC_ENABLE_DSMCC
		GF_M2TS_DSMCC_OVERLORD* dsmcc_overlord = (GF_M2TS_DSMCC_OVERLORD*)gf_list_get(ts->dsmcc_controler,0);
		gf_cleanup_dir(dsmcc_overlord->root_dir);
		gf_rmdir(dsmcc_overlord->root_dir);
		gf_m2ts_delete_dsmcc_overlord(dsmcc_overlord);
		if(ts->dsmcc_root_dir) {
			gf_free(ts->dsmcc_root_dir);
		}
#endif
	}

	while(gf_list_count(ts->ChannelAppList)) {
#ifdef GPAC_ENABLE_DSMCC
		GF_M2TS_CHANNEL_APPLICATION_INFO* ChanAppInfo = (GF_M2TS_CHANNEL_APPLICATION_INFO*)gf_list_get(ts->ChannelAppList,0);
		gf_m2ts_delete_channel_application_info(ChanAppInfo);
		gf_list_rem(ts->ChannelAppList,0);
#endif
	}
	gf_list_del(ts->ChannelAppList);

	if (ts->requested_progs) {
		assert(!gf_list_count(ts->requested_progs));
		gf_list_del(ts->requested_progs);
	}

	if (ts->requested_pids) {
		assert(!gf_list_count(ts->requested_pids));
		gf_list_del(ts->requested_pids);
	}

	if (ts->th)
		gf_th_del(ts->th);

	if (ts->socket_url) gf_free(ts->socket_url);
	gf_free(ts);
}

void gf_m2ts_print_info(GF_M2TS_Demuxer *ts)
{
#ifdef GPAC_ENABLE_MPE
	gf_m2ts_print_mpe_info(ts);
#endif
}

GF_EXPORT
void gf_m2ts_pause_demux(GF_M2TS_Demuxer *ts, Bool do_pause)
{
	if (ts) ts->paused = do_pause;
}

GF_EXPORT
GF_Err gf_m2ts_demux_file(GF_M2TS_Demuxer *ts, const char *fileName, u64 start_byterange, u64 end_byterange, u32 refresh_type, Bool signal_end_of_stream)
{
	u32 i;
	GF_Err e;
	u32 size;
	u64 read;
	GF_BitStream *bs = NULL;
	FILE *f = NULL;

	//force EOS signaing
	if (!fileName) {
		if (!signal_end_of_stream) return GF_BAD_PARAM;
		ts->pos_in_stream = 0;
	}
	else if (fileName && !strnicmp(fileName, "gmem://", 7)) {
		void *mem_address;
		u32 remain;
		if (sscanf(fileName, "gmem://%d@%p", &size, &mem_address) != 2) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[M2TSDemux] Cannot open next file %s\n", fileName));
			return GF_URL_ERROR;
		}
		if (refresh_type==0)
			ts->pos_in_stream = 0;

		//resume where we left
		mem_address = (u8 *) mem_address + ts->pos_in_stream;
		size -= (u32) ts->pos_in_stream;

		remain = (size % 188);
		size -= remain;

		/*process chunk*/
		ts->abort_parsing = GF_FALSE;
		/*e = */gf_m2ts_process_data(ts, mem_address, size);

		if (refresh_type==2)
			ts->pos_in_stream = 0;
		else
			ts->pos_in_stream += size;

	} else if (fileName) {
		char data[188000];

		f = gf_fopen(fileName, "rb");

		if (!f) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[M2TSDemux] Cannot open next file %s\n", fileName));
			return GF_URL_ERROR;
		}
		bs = gf_bs_from_file(f, GF_BITSTREAM_READ);

		if (refresh_type) {
			if (ts->pos_in_stream) {
				gf_bs_seek(bs, ts->pos_in_stream);
			}
		} else {
			if (start_byterange) {
				gf_bs_seek(bs, start_byterange);
			}
		}
		read = 0;
		ts->abort_parsing = GF_FALSE;
		while (1) {
			u32 to_read = 188000;
			Bool done = 0;
			u64 avail = gf_bs_available(bs);

			if (avail < 188)
				break;

			if (end_byterange && (read + to_read > end_byterange)) {
				to_read = (u32) (end_byterange - read);
				done = 1;
			}
			if (to_read > avail) {
				u32 skip = avail % 188;
				to_read = (u32) (avail - skip);
				done = 1;
			}
			size = gf_bs_read_data(bs, data, to_read);
			if (size) {
				e = gf_m2ts_process_data(ts, data, size);
				if (e) {
					GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[M2TSDemux] Error %s while demuxing %s\n", gf_error_to_string(e), fileName));
				}
			}
			read += size;
			if (done) break;
			if (ts->abort_parsing)
				break;
		}

		if ((refresh_type==2) || !gf_bs_available(bs)) {
			ts->pos_in_stream = 0;
		} else {
			ts->pos_in_stream = gf_bs_get_position(bs);
		}

		gf_bs_del(bs);
		gf_fclose(f);
		ts->abort_parsing = GF_FALSE;
	}

	if (signal_end_of_stream && !ts->pos_in_stream) {
		for (i=0; i<GF_M2TS_MAX_STREAMS; i++) {
			if (ts->ess[i]) {
				if (ts->ess[i]->flags & GF_M2TS_ES_IS_PES) {
					gf_m2ts_flush_pes(ts, (GF_M2TS_PES *) ts->ess[i]);
					ts->on_event(ts, GF_M2TS_EVT_EOS, (GF_M2TS_PES *) ts->ess[i]);
				}
			}
		}
	}
	return GF_OK;
}


static u32 gf_m2ts_demuxer_run(void *_p)
{
	u32 i;
	GF_Err e;
	u32 size;
	GF_M2TS_Demuxer *ts = _p;
	char *data = gf_malloc(ts->udp_buffer_size);

	gf_m2ts_reset_parsers(ts);
	ts->abort_parsing = GF_FALSE;

	//recreate the socket if needed
	if (ts->socket_url && !ts->sock) {
		gf_m2ts_get_socket(ts->socket_url, ts->network_type, ts->udp_buffer_size, &ts->sock);
	}

#ifdef GPAC_HAS_LINUX_DVB
	if (ts->tuner) {
		s32 ts_size;
		// in case of DVB
		while (ts->run_state) {
			if (ts->paused) {
				gf_sleep(1);
				continue;
			}

			ts_size = read(ts->tuner->ts_fd, data, ts->udp_buffer_size);
			if (ts_size>0) gf_m2ts_process_data(ts, data, (u32) ts_size);
		}
	} else
#endif
		if (ts->sock) {
#ifndef GPAC_DISABLE_STREAMING
			u16 seq_num;
			GF_RTPReorder *ch = NULL;
#endif
			u32 nb_empty=0;
			Bool first_run, is_rtp;
			FILE *record_to = NULL;
			if (ts->record_to)
				record_to = gf_fopen(ts->record_to, "wb");

			first_run = 1;
			is_rtp = 0;
			while (ts->run_state) {
				if (ts->paused) {
					gf_sleep(1);
					continue;
				}
				size = 0;
				/*m2ts chunks by chunks*/
				e = gf_sk_receive(ts->sock, data, ts->udp_buffer_size, 0, &size);
				if (!size || e) {
					nb_empty++;
					if (nb_empty==1000) {
						gf_sleep(1);
						nb_empty=0;
					}
					continue;
				}
				if (first_run) {
					first_run = 0;
					/*FIXME: we assume only simple RTP packaging (no CSRC nor extensions)*/
					if ((data[0] != 0x47) && ((data[1] & 0x7F) == 33) ) {
						is_rtp = 1;
#ifndef GPAC_DISABLE_STREAMING
						ch = gf_rtp_reorderer_new(100, 500);
#endif
					}
				}
				/*process chunk*/
				if (is_rtp) {
#ifndef GPAC_DISABLE_STREAMING
					char *pck;
					seq_num = ((data[2] << 8) & 0xFF00) | (data[3] & 0xFF);
					gf_rtp_reorderer_add(ch, (void *) data, size, seq_num);

					pck = (char *) gf_rtp_reorderer_get(ch, &size);
					if (pck) {
						gf_m2ts_process_data(ts, pck+12, size-12);
						if (record_to)
							fwrite(data+12, size-12, 1, record_to);
						gf_free(pck);
					}
#else
					gf_m2ts_process_data(ts, data+12, size-12);
					if (record_to)
						fwrite(data+12, size-12, 1, record_to);
#endif

				} else {
					gf_m2ts_process_data(ts, data, size);
					if (record_to)
						fwrite(data, size, 1, record_to);
				}
			}
			if (record_to)
				gf_fclose(record_to);

#ifndef GPAC_DISABLE_STREAMING
			if (ch)
				gf_rtp_reorderer_del(ch);
#endif

			if (ts->sock && !ts->sock_is_delegate) gf_sk_del(ts->sock);
			ts->sock = NULL;
		} else if (ts->dnload) {
			while (ts->run_state) {
				gf_dm_sess_process(ts->dnload);
				gf_sleep(1);
			}
		} else {
			u32 pos = 0;
			GF_BitStream *ts_bs = NULL;

			if (ts->file)
				ts_bs = gf_bs_from_file(ts->file, GF_BITSTREAM_READ);
			else
				ts_bs = gf_bs_new(ts->ts_data_chunk, ts->ts_data_chunk_size, GF_BITSTREAM_READ);

			gf_bs_seek(ts_bs, 0);

			while (ts->run_state && gf_bs_available(ts_bs) && !ts->force_file_refresh) {

				if (ts->paused) {
					gf_sleep(1);
					continue;
				}

				if (ts->start_range && ts->duration) {
					Double perc = ts->start_range / (1000 * ts->duration);
					pos = (u32) (s64) (perc * ts->file_size);
					/*align to TS packet size*/
					pos/=188;
					pos*=188;

					if (pos>=ts->file_size) {
						pos = 0;
					}
					ts->start_range = 0;
					gf_bs_seek(ts_bs, pos);
				}

				/*m2ts chunks by chunks*/
				size = gf_bs_read_data(ts_bs, data, 188);
				if (!size && (ts->loop_demux == 1)) {
					gf_bs_seek(ts_bs, pos);
					GF_LOG(GF_LOG_INFO, GF_LOG_CONTAINER, ("[M2TSDemux] Loop \n"));
					size = gf_bs_read_data(ts_bs, data, 188);
				}
				if (!size) break;
				if (size != 188) {
					GF_LOG(GF_LOG_WARNING, GF_LOG_CONTAINER, ("[M2TS In] %u bytes read from file instead of 188.\n", size));
				}
				/*process chunk*/
				gf_m2ts_process_data(ts, data, size);

				ts->nb_pck++;

				if (!gf_bs_available(ts_bs) && ts->loop_demux == 1) {
					gf_bs_seek(ts_bs, pos);
					GF_LOG(GF_LOG_INFO, GF_LOG_CONTAINER, ("[M2TSDemux] Loop \n"));
					gf_sleep(3000);
				}
			}
			ts->force_file_refresh = 0;

			if (ts_bs) {
				/*pos = (u32) gf_bs_get_position(ts_bs);*/
				gf_bs_del(ts_bs);
				ts_bs = NULL;
			}
		}

	for (i=0; i<GF_M2TS_MAX_STREAMS; i++) {
		if (ts->ess[i]) {
			if (ts->ess[i]->flags & GF_M2TS_ES_IS_PES) {
				gf_m2ts_flush_pes(ts, (GF_M2TS_PES *) ts->ess[i]);
				ts->on_event(ts, GF_M2TS_EVT_EOS, (GF_M2TS_PES *) ts->ess[i]);
			}
		}
	}
	GF_LOG(GF_LOG_INFO, GF_LOG_CONTAINER, ("[M2TSDemux] EOS reached\n"));

	ts->run_state = 2;
	gf_free(data);
	return 0;
}


GF_EXPORT
GF_Err gf_m2ts_get_socket(const char *url, const char *mcast_ifce_or_mobileip, u32 buf_size, GF_Socket **out_socket)
{
	char *str;
	u16 port;
	GF_Err e;
	u32 sock_type = 0;

	*out_socket=NULL;

	if (!strnicmp(url, "udp://", 6) || !strnicmp(url, "mpegts-udp://", 13)) {
		sock_type = GF_SOCK_TYPE_UDP;
	} else if (!strnicmp(url, "mpegts-tcp://", 13) ) {
		sock_type = GF_SOCK_TYPE_TCP;
	} else {
		return GF_NOT_SUPPORTED;
	}

	url = strchr(url, ':');
	url += 3;

	*out_socket = gf_sk_new(sock_type);
	if (! (*out_socket) ) {
		return GF_IO_ERR;
	}

	/*setup port and src*/
	port = 1234;
	str = strrchr(url, ':');
	/*take care of IPv6 address*/
	if (str && strchr(str, ']')) str = strchr(url, ':');
	if (str) {
		port = atoi(str+1);
		str[0] = 0;
	}

	/*do we have a source ?*/
	if (strlen(url) && strcmp(url, "localhost") ) {
		if (gf_sk_is_multicast_address(url)) {
			e = gf_sk_setup_multicast(*out_socket, url, port, 0, 0, (char*)mcast_ifce_or_mobileip);
		} else {
			e = gf_sk_bind(*out_socket, (char*)mcast_ifce_or_mobileip, port, url, 0, GF_SOCK_REUSE_PORT);
		}
		if (e) {
			gf_sk_del(*out_socket);
			*out_socket = NULL;
			return e;
		}
	}
	if (str) str[0] = ':';

	gf_sk_set_buffer_size(*out_socket, 0, buf_size);
	gf_sk_set_block_mode(*out_socket, 0);
	return GF_OK;
}

static GF_Err gf_m2ts_demuxer_setup_live(GF_M2TS_Demuxer *ts, char *url)
{
	if (!strnicmp(url, "mpegts-sk://", 12)) {
		sscanf(url, "mpegts-sk://%p", &ts->sock);
		ts->sock_is_delegate = GF_TRUE;
	} else {
		GF_Err e;
		e = gf_m2ts_get_socket(url, ts->network_type, ts->udp_buffer_size, &ts->sock);
		if (e) return e;
	}

	if (ts->socket_url) gf_free(ts->socket_url);
	ts->socket_url = gf_strdup(url);

	//gf_th_set_priority(ts->th, GF_THREAD_PRIORITY_HIGHEST);
	return gf_m2ts_demuxer_play(ts);

}

#ifdef GPAC_HAS_LINUX_DVB

static GF_Err gf_dvb_tune(GF_Tuner *tuner, const char *url, const char *chan_path) {
	struct dmx_pes_filter_params pesFilterParams;
	struct dvb_frontend_parameters frp;
	int demux1, front1;
	FILE *chanfile;
	char line[255], chan_name_t[255];
	char freq_str[255], inv[255], bw[255], lcr[255], hier[255], cr[255],
	     mod[255], transm[255], gi[255], apid_str[255], vpid_str[255];
	const char *chan_conf = ":%255[^:]:%255[^:]:%255[^:]:%255[^:]:%255[^:]:%255[^:]:%255[^:]:%255[^:]:%255[^:]:%255[^:]:%255[^:]:";
	char *chan_name;
	char *tmp;
	char frontend_name[100], demux_name[100], dvr_name[100];
	u32 adapter_num;

	chanfile = gf_fopen(chan_path, "r");
	if (!chanfile) return GF_BAD_PARAM;

	chan_name = (char *) url+6; // 6 = strlen("dvb://")

	// support for multiple frontends
	tmp = strchr(chan_name, '@');
	if (tmp) {
		adapter_num = atoi(tmp+1);
		tmp[0] = 0;
	} else {
		adapter_num = 0;
	}
	GF_LOG(GF_LOG_DEBUG, GF_LOG_CONTAINER, ("Channel name %s\n", chan_name));

	while(!feof(chanfile)) {
		if ( fgets(line, 255, chanfile) != NULL) {
			if (line[0]=='#') continue;
			if (line[0]=='\r') continue;
			if (line[0]=='\n') continue;

			strncpy(chan_name_t, line, index(line, ':')-line);
			if (strncmp(chan_name,chan_name_t,strlen(chan_name))==0) {
				sscanf(strstr(line,":"), chan_conf, freq_str, inv, bw, lcr, cr, mod, transm, gi, hier, apid_str, vpid_str);
				tuner->freq = (uint32_t) atoi(freq_str);
				tuner->apid = (uint16_t) atoi(apid_str);
				tuner->vpid = (uint16_t) atoi(vpid_str);
				//Inversion
				if(! strcmp(inv, "INVERSION_ON")) tuner->specInv = INVERSION_ON;
				else if(! strcmp(inv, "INVERSION_OFF")) tuner->specInv = INVERSION_OFF;
				else tuner->specInv = INVERSION_AUTO;
				//LP Code Rate
				if(! strcmp(lcr, "FEC_1_2")) tuner->LP_CodeRate =FEC_1_2;
				else if(! strcmp(lcr, "FEC_2_3")) tuner->LP_CodeRate =FEC_2_3;
				else if(! strcmp(lcr, "FEC_3_4")) tuner->LP_CodeRate =FEC_3_4;
				else if(! strcmp(lcr, "FEC_4_5")) tuner->LP_CodeRate =FEC_4_5;
				else if(! strcmp(lcr, "FEC_6_7")) tuner->LP_CodeRate =FEC_6_7;
				else if(! strcmp(lcr, "FEC_8_9")) tuner->LP_CodeRate =FEC_8_9;
				else if(! strcmp(lcr, "FEC_5_6")) tuner->LP_CodeRate =FEC_5_6;
				else if(! strcmp(lcr, "FEC_7_8")) tuner->LP_CodeRate =FEC_7_8;
				else if(! strcmp(lcr, "FEC_NONE")) tuner->LP_CodeRate =FEC_NONE;
				else tuner->LP_CodeRate =FEC_AUTO;
				//HP Code Rate
				if(! strcmp(cr, "FEC_1_2")) tuner->HP_CodeRate =FEC_1_2;
				else if(! strcmp(cr, "FEC_2_3")) tuner->HP_CodeRate =FEC_2_3;
				else if(! strcmp(cr, "FEC_3_4")) tuner->HP_CodeRate =FEC_3_4;
				else if(! strcmp(cr, "FEC_4_5")) tuner->HP_CodeRate =FEC_4_5;
				else if(! strcmp(cr, "FEC_6_7")) tuner->HP_CodeRate =FEC_6_7;
				else if(! strcmp(cr, "FEC_8_9")) tuner->HP_CodeRate =FEC_8_9;
				else if(! strcmp(cr, "FEC_5_6")) tuner->HP_CodeRate =FEC_5_6;
				else if(! strcmp(cr, "FEC_7_8")) tuner->HP_CodeRate =FEC_7_8;
				else if(! strcmp(cr, "FEC_NONE")) tuner->HP_CodeRate =FEC_NONE;
				else tuner->HP_CodeRate =FEC_AUTO;
				//Modulation
				if(! strcmp(mod, "QAM_128")) tuner->modulation = QAM_128;
				else if(! strcmp(mod, "QAM_256")) tuner->modulation = QAM_256;
				else if(! strcmp(mod, "QAM_64")) tuner->modulation = QAM_64;
				else if(! strcmp(mod, "QAM_32")) tuner->modulation = QAM_32;
				else if(! strcmp(mod, "QAM_16")) tuner->modulation = QAM_16;
				//Bandwidth
				if(! strcmp(bw, "BANDWIDTH_6_MHZ")) tuner->bandwidth = BANDWIDTH_6_MHZ;
				else if(! strcmp(bw, "BANDWIDTH_7_MHZ")) tuner->bandwidth = BANDWIDTH_7_MHZ;
				else if(! strcmp(bw, "BANDWIDTH_8_MHZ")) tuner->bandwidth = BANDWIDTH_8_MHZ;
				//Transmission Mode
				if(! strcmp(transm, "TRANSMISSION_MODE_2K")) tuner->TransmissionMode = TRANSMISSION_MODE_2K;
				else if(! strcmp(transm, "TRANSMISSION_MODE_8K")) tuner->TransmissionMode = TRANSMISSION_MODE_8K;
				//Guard Interval
				if(! strcmp(gi, "GUARD_INTERVAL_1_32")) tuner->guardInterval = GUARD_INTERVAL_1_32;
				else if(! strcmp(gi, "GUARD_INTERVAL_1_16")) tuner->guardInterval = GUARD_INTERVAL_1_16;
				else if(! strcmp(gi, "GUARD_INTERVAL_1_8")) tuner->guardInterval = GUARD_INTERVAL_1_8;
				else tuner->guardInterval = GUARD_INTERVAL_1_4;
				//Hierarchy
				if(! strcmp(hier, "HIERARCHY_1")) tuner->hierarchy = HIERARCHY_1;
				else if(! strcmp(hier, "HIERARCHY_2")) tuner->hierarchy = HIERARCHY_2;
				else if(! strcmp(hier, "HIERARCHY_4")) tuner->hierarchy = HIERARCHY_4;
				else if(! strcmp(hier, "HIERARCHY_AUTO")) tuner->hierarchy = HIERARCHY_AUTO;
				else tuner->hierarchy = HIERARCHY_NONE;

				break;
			}
		}
	}
	gf_fclose(chanfile);

	sprintf(frontend_name, "/dev/dvb/adapter%d/frontend0", adapter_num);
	sprintf(demux_name, "/dev/dvb/adapter%d/demux0", adapter_num);
	sprintf(dvr_name, "/dev/dvb/adapter%d/dvr0", adapter_num);

	// Open frontend
	if((front1 = open(frontend_name,O_RDWR|O_NONBLOCK)) < 0) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("Cannot open frontend %s.\n", frontend_name));
		return GF_IO_ERR;
	} else {
		GF_LOG(GF_LOG_DEBUG, GF_LOG_CONTAINER, ("Frontend %s opened.\n", frontend_name));
	}
	// Open demuxes
	if ((demux1=open(demux_name, O_RDWR|O_NONBLOCK)) < 0) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("Cannot open demux %s\n", demux_name));
		return GF_IO_ERR;
	} else {
		GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("Demux %s opened.\n", demux_name));
	}
	// Set FrontendParameters - DVB-T
	frp.frequency = tuner->freq;
	frp.inversion = tuner->specInv;
	frp.u.ofdm.bandwidth = tuner->bandwidth;
	frp.u.ofdm.code_rate_HP = tuner->HP_CodeRate;
	frp.u.ofdm.code_rate_LP = tuner->LP_CodeRate;
	frp.u.ofdm.constellation = tuner->modulation;
	frp.u.ofdm.transmission_mode = tuner->TransmissionMode;
	frp.u.ofdm.guard_interval = tuner->guardInterval;
	frp.u.ofdm.hierarchy_information = tuner->hierarchy;
	// Set frontend
	if (ioctl(front1, FE_SET_FRONTEND, &frp) < 0) {
		return GF_IO_ERR;
	}
	// Set dumex
	pesFilterParams.pid      = 0x2000;				// Linux-DVB API take PID=2000 for FULL/RAW TS flag
	pesFilterParams.input    = DMX_IN_FRONTEND;
	pesFilterParams.output   = DMX_OUT_TS_TAP;
	pesFilterParams.pes_type = DMX_PES_OTHER;
	pesFilterParams.flags    = DMX_IMMEDIATE_START;
	if (ioctl(demux1, DMX_SET_PES_FILTER, &pesFilterParams) < 0) {
		return GF_IO_ERR;
	}
	/* The following code differs from mplayer and alike because the device is opened in blocking mode */
	if ((tuner->ts_fd = open(dvr_name, O_RDONLY/*|O_NONBLOCK*/)) < 0) {
		return GF_IO_ERR;
	}
	return GF_OK;
}

GF_EXPORT
u32 gf_dvb_get_freq_from_url(const char *channels_config_path, const char *url)
{
	FILE *channels_config_file;
	char line[255], *tmp, *channel_name;

	u32 freq;

	/* get rid of trailing @ */
	tmp = strchr(url, '@');
	if (tmp) tmp[0] = 0;

	channel_name = (char *)url+6;

	channels_config_file = gf_fopen(channels_config_path, "r");
	if (!channels_config_file) return GF_BAD_PARAM;

	freq = 0;
	while(!feof(channels_config_file)) {
		if ( fgets(line, 255, channels_config_file) != NULL) {
			if (line[0]=='#') continue;
			if (line[0]=='\r') continue;
			if (line[0]=='\n') continue;

			tmp = strchr(line, ':');
			tmp[0] = 0;
			if (!strcmp(line, channel_name)) {
				char *tmp2;
				tmp++;
				tmp2 = strchr(tmp, ':');
				if (tmp2) tmp2[0] = 0;
				freq = (u32)atoi(tmp);
				break;
			}
		}
	}
	return freq;
}

GF_Err gf_m2ts_demuxer_setup_dvb(GF_M2TS_Demuxer *ts, const char *url)
{
	GF_Err e = GF_OK;

	if (! ts->dvb_channels_conf_path) return GF_BAD_PARAM;

	if (strnicmp(url, "dvb://", 6)) return GF_NOT_SUPPORTED;

	if (!ts->tuner) GF_SAFEALLOC(ts->tuner, GF_Tuner);

	if (ts->tuner->freq != 0 && ts->tuner->freq == gf_dvb_get_freq_from_url(ts->dvb_channels_conf_path, url)) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[M2TSDemux] Tuner already tuned to that frequency\n"));
		return GF_OK;
	}

	e = gf_dvb_tune(ts->tuner, url, ts->dvb_channels_conf_path);
	if (e) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[M2TSDemux] Unable to tune to frequency\n"));
		return GF_SERVICE_ERROR;
	}

	return  gf_m2ts_demuxer_play(ts);
}

#endif

static GF_Err gf_m2ts_demuxer_setup_file(GF_M2TS_Demuxer *ts, char *url)
{
	if (url) {
		if (ts->file && !strcmp(ts->filename, url)) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[TSDemux] TS file already being processed: %s\n", url));
			return GF_IO_ERR;
		}
		strcpy(ts->filename, url);

		if (!strncmp(url, "gmem://", 7)) {
			u32 size;
			void *mem_address;
			if (sscanf(url, "gmem://%d@%p", &size, &mem_address) != 2)
				return GF_IO_ERR;
			ts->ts_data_chunk_size = size;
			ts->ts_data_chunk = mem_address;
		} else {

			ts->file = gf_fopen(url, "rb");
			if (!ts->file) {
				GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[TSDemux] Could not open TS file: %s\n", url));
				return GF_IO_ERR;
			}
			strcpy(ts->filename, url);

			gf_fseek(ts->file, 0, SEEK_END);
			ts->file_size = gf_ftell(ts->file);
			gf_fseek(ts->file, 0, SEEK_SET);
		}
	}

	/* reinitialization for seek */
	ts->end_range = ts->start_range = 0;
	return  gf_m2ts_demuxer_play(ts);

}

GF_EXPORT
GF_Err gf_m2ts_demuxer_setup(GF_M2TS_Demuxer *ts, const char *url, Bool loop)
{
	char szURL[2048];
	char *frag;
	ts->duration = 0;

	if(loop == 1) {
		ts->loop_demux = 1;
		GF_LOG(GF_LOG_INFO, GF_LOG_CONTAINER, ("Loop Mode activated \n"));
	}

	if (!url) {
		return gf_m2ts_demuxer_setup_file(ts, NULL);
	}

	strcpy(szURL, url);
	frag = strrchr(szURL, '#');
	if (frag) frag[0] = 0;

	if (!strnicmp(url, "udp://", 6)
	        || !strnicmp(url, "mpegts-udp://", 13)
	        || !strnicmp(url, "mpegts-tcp://", 13)
	        || !strnicmp(url, "mpegts-sk://", 12)
	   ) {
		return gf_m2ts_demuxer_setup_live(ts, (char *) szURL);
	}
#ifdef GPAC_HAS_LINUX_DVB
	else if (!strnicmp(url, "dvb://", 6)) {
		return gf_m2ts_demuxer_setup_dvb(ts, (char *) szURL);
	}
#endif
	else {
		return gf_m2ts_demuxer_setup_file(ts, (char *) szURL);
	}

	return GF_NOT_SUPPORTED;
}

GF_EXPORT
GF_Err gf_m2ts_demuxer_close(GF_M2TS_Demuxer *ts)
{
	GF_LOG(GF_LOG_DEBUG, GF_LOG_CONTAINER, ("[TSDemux] Destroying demuxer\n"));
	if (ts->th) {
		if (ts->run_state == 1) {
			ts->run_state = 0;
			while (ts->run_state!=2) gf_sleep(0);
		}
		gf_th_del(ts->th);
		ts->th = NULL;
	}

	if (ts->file) gf_fclose(ts->file);
	ts->file = NULL;
	ts->ts_data_chunk = NULL;

	return GF_OK;
}

GF_EXPORT
GF_Err gf_m2ts_demuxer_play(GF_M2TS_Demuxer *ts)
{

	/*set the state variable outside the TS thread. If inside, we may get called for shutdown before the TS thread has started
	and we would overwrite the run_state when entering the TS thread, which would make the thread run forever and the stop() wait forever*/
	ts->run_state = 1;
	if(ts->th) {
		/*start playing for tune-in*/
		return gf_th_run(ts->th, gf_m2ts_demuxer_run, ts);
	} else {
		return gf_m2ts_demuxer_run(ts);
	}

}

#define M2TS_PROBE_SIZE	188000
GF_EXPORT
Bool gf_m2ts_probe_file(const char *fileName)
{
	char buf[M2TS_PROBE_SIZE];
	GF_Err e;
	u32 size;
	FILE *t;
	GF_M2TS_Demuxer *ts;

	if (!strncmp(fileName, "gmem://", 7)) {
		u8 *mem_address;
		if (sscanf(fileName, "gmem://%d@%p", &size, &mem_address) != 2) {
			return GF_FALSE;
		}
		if (size>M2TS_PROBE_SIZE) size = M2TS_PROBE_SIZE;
		memcpy(buf, mem_address, size);
	} else {
		t = gf_fopen(fileName, "rb");
		if (!t) return 0;
		size = (u32) fread(buf, 1, M2TS_PROBE_SIZE, t);
		gf_fclose(t);
		if ((s32) size <= 0) return 0;
	}
	ts = gf_m2ts_demux_new();
	e = gf_m2ts_process_data(ts, buf, size);
	if (!ts->pck_number) e = GF_BAD_PARAM;
	gf_m2ts_demux_del(ts);
	if (e) return 0;
	return 1;
}

static void rewrite_pts_dts(unsigned char *ptr, u64 TS)
{
	ptr[0] &= 0xf1;
	ptr[0] |= (unsigned char)((TS&0x1c0000000ULL)>>29);
	ptr[1]  = (unsigned char)((TS&0x03fc00000ULL)>>22);
	ptr[2] &= 0x1;
	ptr[2] |= (unsigned char)((TS&0x0003f8000ULL)>>14);
	ptr[3]  = (unsigned char)((TS&0x000007f80ULL)>>7);
	ptr[4] &= 0x1;
	ptr[4] |= (unsigned char)((TS&0x00000007fULL)<<1);

	assert(((u64)(ptr[0]&0xe)<<29) + ((u64)ptr[1]<<22) + ((u64)(ptr[2]&0xfe)<<14) + ((u64)ptr[3]<<7) + ((ptr[4]&0xfe)>>1) == TS);
}

#define ADJUST_TIMESTAMP(_TS) \
	if (_TS < (u64) -ts_shift) _TS = pcr_mod + _TS + ts_shift; \
	else _TS = _TS + ts_shift; \
	while (_TS > pcr_mod) _TS -= pcr_mod; \

GF_EXPORT
GF_Err gf_m2ts_restamp(char *buffer, u32 size, s64 ts_shift, u8 *is_pes)
{
	u32 done = 0;
	u64 pcr_mod;
//	if (!ts_shift) return GF_OK;

	pcr_mod = 0x80000000;
	pcr_mod*=4;
	while (done + 188 <= size) {
		u8 *pesh;
		u8 *pck;
		u64 pcr_base=0, pcr_ext=0;
		u16 pid;
		u8 adaptation_field, adaptation_field_length;

		pck = (u8*) buffer+done;
		if (pck[0]!=0x47) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[M2TS Restamp] Invalid sync byte %X\n", pck[0]));
			return GF_NON_COMPLIANT_BITSTREAM;
		}
		pid = ((pck[1] & 0x1f) <<8 ) + pck[2];

		adaptation_field_length = 0;
		adaptation_field = (pck[3] >> 4) & 0x3;
		if ((adaptation_field==2) || (adaptation_field==3)) {
			adaptation_field_length = pck[4];
			if ( pck[5]&0x10 /*PCR_flag*/) {
				pcr_base = (((u64)pck[6])<<25) + (pck[7]<<17) + (pck[8]<<9) + (pck[9]<<1) + (pck[10]>>7);
				pcr_ext  = ((pck[10]&1)<<8) + pck[11];

				ADJUST_TIMESTAMP(pcr_base);

				pck[6]  = (unsigned char)(0xff&(pcr_base>>25));
				pck[7]  = (unsigned char)(0xff&(pcr_base>>17));
				pck[8]  = (unsigned char)(0xff&(pcr_base>>9));
				pck[9]  = (unsigned char)(0xff&(pcr_base>>1));
				pck[10] = (unsigned char)(((0x1&pcr_base)<<7) | 0x7e | ((0x100&pcr_ext)>>8));
				if (pcr_ext != ((pck[10]&1)<<8) + pck[11]) {
					GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[M2TS Restamp] Sanity check failed for PCR restamping\n"));
					return GF_IO_ERR;
				}
				pck[11] = (unsigned char)(0xff&pcr_ext);
			}
			/*add adaptation_field_length field*/
			adaptation_field_length++;
		}
		if (!is_pes[pid] || !(pck[1]&0x40)) {
			done+=188;
			continue;
		}

		pesh = &pck[4+adaptation_field_length];

		if ((pesh[0]==0x00) && (pesh[1]==0x00) && (pesh[2]==0x01)) {
			Bool has_pts, has_dts;
			if ((pesh[6]&0xc0)!=0x80) {
				done+=188;
				continue;
			}
			has_pts = (pesh[7]&0x80);
			has_dts = has_pts ? (pesh[7]&0x40) : 0;

			if (has_pts) {
				u64 PTS;
				if (((pesh[9]&0xe0)>>4)!=0x2) {
					GF_LOG(GF_LOG_WARNING, GF_LOG_CONTAINER, ("[M2TS Restamp] PID %4d: Wrong PES header, PTS decoding: '0010' expected\n", pid));
					done+=188;
					continue;
				}

				PTS = gf_m2ts_get_pts(pesh + 9);
				ADJUST_TIMESTAMP(PTS);
				rewrite_pts_dts(pesh+9, PTS);
			}

			if (has_dts) {
				u64 DTS = gf_m2ts_get_pts(pesh + 14);
				ADJUST_TIMESTAMP(DTS);
				rewrite_pts_dts(pesh+14, DTS);
			}
		} else {
			GF_LOG(GF_LOG_WARNING, GF_LOG_CONTAINER, ("[M2TS Restamp] PID %4d: Wrong PES not beginning with start code\n", pid));
		}
		done+=188;
	}
	return GF_OK;
}

#endif /*GPAC_DISABLE_MPEG2TS*/

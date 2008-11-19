/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Walid B.H - Jean Le Feuvre
 *    Copyright (c)2006-200X ENST - All rights reserved
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
#include <gpac/constants.h>
#include <gpac/internal/media_dev.h>
#include <gpac/math.h>

#define DEBUG_TS_PACKET 0

const char *gf_m2ts_get_stream_name(u32 streamType)
{
	switch (streamType) {
	case GF_M2TS_VIDEO_MPEG1: return "MPEG-1 Video";
	case GF_M2TS_VIDEO_MPEG2: return "MPEG-2 Video";
	case GF_M2TS_AUDIO_MPEG1: return "MPEG-1 Audio";
	case GF_M2TS_AUDIO_MPEG2: return "MPEG-2 Audio";
	case GF_M2TS_PRIVATE_SECTION: return "Private Section";
	case GF_M2TS_PRIVATE_DATA: return "Private Data";
	case GF_M2TS_AUDIO_AAC: return "AAC Audio";
	case GF_M2TS_VIDEO_MPEG4: return "MPEG-4 Video";
	case GF_M2TS_VIDEO_H264: return "MPEG-4/H264 Video";

	case GF_M2TS_AUDIO_AC3: return "Dolby AC3 Audio";
	case GF_M2TS_AUDIO_DTS: return "Dolby DTS Audio";
	case GF_M2TS_SUBTITLE_DVB: return "DVB Subtitle";
	case GF_M2TS_SYSTEMS_MPEG4_PES: return "MPEG-4 SL (PES)";
	case GF_M2TS_SYSTEMS_MPEG4_SECTIONS: return "MPEG-4 SL (Section)";
	default: return "Unknown";
	}
}

static void gf_m2ts_reframe_default(GF_M2TS_Demuxer *ts, GF_M2TS_PES *pes, u64 DTS, u64 PTS, unsigned char *data, u32 data_len)
{
	GF_M2TS_PES_PCK pck;
	pck.flags = 0;
	if (pes->rap) pck.flags |= GF_M2TS_PES_PCK_RAP;

	if (PTS) {
		pes->PTS = PTS;
		/*backup DTS for start detection*/
		PTS = pes->DTS;
		if (DTS) pes->DTS = DTS;
		else pes->DTS = PTS;
		if (!PTS || (PTS != pes->DTS)) pck.flags = GF_M2TS_PES_PCK_AU_START;
	}
	pck.DTS = pes->DTS;
	pck.PTS = pes->PTS;
	pck.data = data;
	pck.data_len = data_len;
	pck.stream = pes;
	ts->on_event(ts, GF_M2TS_EVT_PES_PCK, &pck);
}


static void gf_m2ts_reframe_avc_h264(GF_M2TS_Demuxer *ts, GF_M2TS_PES *pes, u64 DTS, u64 PTS, unsigned char *data, u32 data_len)
{
	Bool start_code_found = 0;
	u32 nal_type, sc_pos = 0;
	GF_M2TS_PES_PCK pck;

	if (PTS) {
		pes->PTS = PTS;
		if (DTS) pes->DTS = DTS;
		else pes->DTS = PTS;
	}

	/*dispatch frame*/
	pck.stream = pes;
	pck.DTS = pes->DTS;
	pck.PTS = pes->PTS;
	pck.flags = 0;

	while (sc_pos<data_len) {
		unsigned char *start = (unsigned char *)memchr(data+sc_pos, 0, data_len-sc_pos);
		if (!start) break;
		sc_pos = start - data;

		if (start[1] || start[2] || (start[3]!=1)) {
			sc_pos ++;
			continue;
		}

		if (!start_code_found) {
			if (sc_pos) {
				pck.data = data;
				pck.data_len = sc_pos;
				pck.flags = 0;
				ts->on_event(ts, GF_M2TS_EVT_PES_PCK, &pck);
				data += sc_pos;
				data_len -= sc_pos;
				sc_pos = 0;
			}
			start_code_found = 1;
		} else {
			pck.data = data;
			pck.data_len = sc_pos;

			nal_type = pck.data[4] & 0x1F;

			/*check for SPS and update stream info*/
			if (!pes->vid_w && (nal_type==GF_AVC_NALU_SEQ_PARAM)) {
				AVCState avc;
				s32 idx;
				GF_BitStream *bs = gf_bs_new(data+5, sc_pos-5, GF_BITSTREAM_READ);
				memset(&avc, 0, sizeof(AVCState));
				idx = AVC_ReadSeqInfo(bs, &avc, NULL);
				gf_bs_del(bs);

				if (idx>=0) {
					pes->vid_w = avc.sps[idx].width;
					pes->vid_h = avc.sps[idx].height;
				}
			}

			/*check AU start type*/
			if (nal_type==GF_AVC_NALU_ACCESS_UNIT) pck.flags = GF_M2TS_PES_PCK_AU_START;
			else pck.flags = 0;
			ts->on_event(ts, GF_M2TS_EVT_PES_PCK, &pck);

			data += sc_pos;
			data_len -= sc_pos;
			sc_pos = 0;
		}
		sc_pos++;
	}
	if (data_len) {
		pck.flags = 0;
		if (start_code_found) {
			pck.data = data;
			pck.data_len = data_len;
			nal_type = pck.data[4] & 0x1F;
			if (nal_type==GF_AVC_NALU_ACCESS_UNIT) pck.flags = GF_M2TS_PES_PCK_AU_START;
		} else {
			pck.data = data;
			pck.data_len = data_len;
		}
		ts->on_event(ts, GF_M2TS_EVT_PES_PCK, &pck);
	}
}

void gf_m2ts_reframe_mpeg_video(GF_M2TS_Demuxer *ts, GF_M2TS_PES *pes, u64 DTS, u64 PTS, unsigned char *data, u32 data_len)
{
	u32 sc_pos = 0;
	u32 to_send = data_len;
	GF_M2TS_PES_PCK pck;

	if (PTS) {
		pes->PTS = PTS;
		if (DTS) pes->DTS = DTS;
		else pes->DTS = PTS;
	}
	/*dispatch frame*/
	pck.stream = pes;
	pck.DTS = pes->DTS;
	pck.PTS = pes->PTS;
	pck.flags = 0;

	while (sc_pos+4<data_len) {
		unsigned char *start = (unsigned char*)memchr(data+sc_pos, 0, data_len-sc_pos);
		if (!start) break;
		sc_pos = start - (unsigned char*)data;

		/*found picture or sequence start_code*/
		if (!start[1] && (start[2]==0x01)) {
			if (!start[3] || (start[3]==0xb3) || (start[3]==0xb8)) {
				Bool new_au;
				if (sc_pos) {
					pck.data = data;
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
					default: pes->vid_par = 0; break;
					case 2: num = 4; den = 3; break;
					case 3: num = 16; den = 9; break;
					case 4: num = 221; den = 100; break;
					}
					if (den)
						pes->vid_par = ((pes->vid_h/den)<<16) | (pes->vid_w/num); break;
				}
				if (pes->frame_state==0x00) {
					switch ((data[5] >> 3) & 0x7) {
					case 1: pck.flags |= GF_M2TS_PES_PCK_I_FRAME; break;
					case 2: pck.flags |= GF_M2TS_PES_PCK_P_FRAME; break;
					case 3: pck.flags |= GF_M2TS_PES_PCK_B_FRAME; break;
					}
				}
			}
			sc_pos+=3;
		}
		sc_pos++;
	}
	pck.data = data;
	pck.data_len = data_len;
	ts->on_event(ts, GF_M2TS_EVT_PES_PCK, &pck);
}

static u32 latm_get_value(GF_BitStream *bs)
{
	u32 i, tmp, value = 0;
	u32 bytesForValue = gf_bs_read_int(bs, 2);
	for (i=0; i <= bytesForValue; i++) {
		value <<= 8;
		tmp = gf_bs_read_int(bs, 8);
		value += tmp;
	}
	return value;
}

void gf_m2ts_reframe_aac_latm(GF_M2TS_Demuxer *ts, GF_M2TS_PES *pes, u64 DTS, u64 PTS, unsigned char *data, u32 data_len)
{
	u32 sc_pos = 0;
	u32 start = 0;
	GF_M2TS_PES_PCK pck;

	if (PTS) {
		pes->PTS = PTS;
		if (DTS) pes->DTS = DTS;
		else pes->DTS = PTS;
	}
	/*dispatch frame*/
	pck.stream = pes;
	pck.DTS = pes->DTS;
	pck.PTS = pes->PTS;
	pck.flags = 0;

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
			/*...*/

		}

		/*found a start code*/
		amux_len = data[sc_pos+1] & 0x1F;
		amux_len <<= 8;
		amux_len |= data[sc_pos+2];

		bs = gf_bs_new(data+sc_pos+3, amux_len, GF_BITSTREAM_READ);

		/*use same stream mux*/
		if (!gf_bs_read_int(bs, 1)) {
			Bool amux_version, amux_versionA;
			
			amux_version = gf_bs_read_int(bs, 1);
			amux_versionA = 0;
			if (amux_version) amux_versionA = gf_bs_read_int(bs, 1);
			if (!amux_versionA) {
				u32 i, allStreamsSameTimeFraming, numSubFrames, numProgram;
				if (amux_version) latm_get_value(bs);

				allStreamsSameTimeFraming = gf_bs_read_int(bs, 1);
				numSubFrames = gf_bs_read_int(bs, 6);
				numProgram = gf_bs_read_int(bs, 4);
				for (i=0; i<=numProgram; i++) {
					u32 j, num_lay;
					num_lay = gf_bs_read_int(bs, 3);
					for (j=0;j<=num_lay; j++) {
						GF_M4ADecSpecInfo cfg;
						u32 frameLengthType, latmBufferFullness;
						Bool same_cfg = 0;
						if (i || j) same_cfg = gf_bs_read_int(bs, 1);

						if (!same_cfg) {
							if (amux_version==1) latm_get_value(bs);
							gf_m4a_parse_config(bs, &cfg, 0);

							if (!pes->aud_sr) {
								pck.stream = pes;
								gf_m4a_write_config(&cfg, &pck.data, &pck.data_len);
								ts->on_event(ts, GF_M2TS_EVT_AAC_CFG, &pck);
								free(pck.data);
								pes->aud_sr = cfg.base_sr;
								pes->aud_nb_ch = cfg.nb_chan;
							}
						}
						frameLengthType = gf_bs_read_int(bs, 3);
						if (!frameLengthType) {
							latmBufferFullness = gf_bs_read_int(bs, 8);
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
				pes->buf = realloc(pes->buf, sizeof(char)*pes->buf_len);
			}
			gf_bs_read_data(bs, pes->buf, size);

			/*dispatch frame*/
			pck.stream = pes;
			pck.DTS = pes->PTS;
			pck.PTS = pes->PTS;
			pck.flags = GF_M2TS_PES_PCK_AU_START | GF_M2TS_PES_PCK_RAP;
			pck.data = pes->buf;
			pck.data_len = size;

			ts->on_event(ts, GF_M2TS_EVT_PES_PCK, &pck);

			/*update PTS in case we don't get any update*/
			size = 1024*90000/pes->aud_sr;
			pes->PTS += size;
		}
		gf_bs_del(bs);

		/*parse amux*/
		sc_pos += amux_len+3;
		start = sc_pos;

	}
}

static void gf_m2ts_reframe_mpeg_audio(GF_M2TS_Demuxer *ts, GF_M2TS_PES *pes, u64 DTS, u64 PTS, unsigned char *data, u32 data_len)
{
	GF_M2TS_PES_PCK pck;
	u32 pos, frame_size, remain;

	pck.flags = GF_M2TS_PES_PCK_RAP;
	pck.stream = pes;
	remain = pes->frame_state;

	pes->frame_state = gf_mp3_get_next_header_mem(data, data_len, &pos);
	if (!pes->frame_state) {
		if (remain) {
			/*dispatch end of prev frame*/
			pck.DTS = pck.PTS = pes->PTS;
			pck.data = data;
			pck.data_len = (remain>data_len) ? data_len : remain;
			ts->on_event(ts, GF_M2TS_EVT_PES_PCK, &pck);
			if (remain>data_len) pes->frame_state = remain - data_len;
		}
		return;
	}
	assert((pes->frame_state & 0xffe00000) == 0xffe00000);
	/*resync*/
	if (pos) {
		if (remain) {
			/*sync error!!*/
			if (remain>pos) {
				GF_LOG(GF_LOG_DEBUG, GF_LOG_CONTAINER, ("[MPEG-2 TS] sync error - start code @ %d - remaining from last frame %d\n", pos, remain) );
				remain = pos;
			}
			/*dispatch end of prev frame*/
			pck.DTS = pck.PTS = pes->PTS;
			pck.data = data;
			pck.data_len = remain;
			ts->on_event(ts, GF_M2TS_EVT_PES_PCK, &pck);
		}
		data += pos;
		data_len -= pos;
	}
	if (!pes->PTS) {
		pes->aud_sr = gf_mp3_sampling_rate(pes->frame_state);
		pes->aud_nb_ch = gf_mp3_num_channels(pes->frame_state);
	}
	/*we may get a PTS for aither the previous or the current frame*/
	if (PTS>=pes->PTS) pes->PTS = PTS;
	pck.flags = GF_M2TS_PES_PCK_RAP | GF_M2TS_PES_PCK_AU_START;

	frame_size = gf_mp3_frame_size(pes->frame_state);
	while (frame_size <= data_len) {
		/*dispatch frame*/
		pck.DTS = pck.PTS = pes->PTS;
		pck.data = data;
		pck.data_len = frame_size;
		ts->on_event(ts, GF_M2TS_EVT_PES_PCK, &pck);

		pes->PTS += gf_mp3_window_size(pes->frame_state)*90000/gf_mp3_sampling_rate(pes->frame_state);
		/*move frame*/
		data += frame_size;
		data_len -= frame_size;
		if (!data_len) break;
		pes->frame_state = gf_mp3_get_next_header_mem(data, data_len, &pos);
		/*resync (ID3 or error)*/
		if (!pes->frame_state) {
			data_len = 0;
			break;
		}
		/*resync (ID3 or error)*/
		if (pos) { 
			data_len -= pos;
			data += pos;
		}
		frame_size = gf_mp3_frame_size(pes->frame_state);
	}
	if (data_len) {
		pck.DTS = pck.PTS = pes->PTS;
		pck.data = data;
		pck.data_len = data_len;
		ts->on_event(ts, GF_M2TS_EVT_PES_PCK, &pck);
		/*update PTS in case we don't get any update*/
		pes->PTS += gf_mp3_window_size(pes->frame_state)*90000/gf_mp3_sampling_rate(pes->frame_state);
		pes->frame_state = frame_size - data_len;
	} else {
		pes->frame_state = 0;
	}
}

static u32 gf_m2ts_sync(GF_M2TS_Demuxer *ts, Bool simple_check)
{
	u32 i=0;
	/*if first byte is sync assume we're sync*/
	if (simple_check && (ts->buffer[i]==0x47)) return 0;

	while (i<ts->buffer_size) {
		if (i+188>ts->buffer_size) return ts->buffer_size;
		if ((ts->buffer[i]==0x47) && (ts->buffer[i+188]==0x47)) break;
		i++;
	}
	if (i) {
		GF_LOG(GF_LOG_DEBUG, GF_LOG_CONTAINER, ("[MPEG-2 TS] re-sync skipped %d bytes\n", i) );
	}
	return i;
}

Bool gf_m2ts_crc32_check(char *data, u32 len)
{
    u32 crc = gf_crc_32(data, len);
	u32 crc_val = GF_4CC((u8) data[len], (u8) data[len+1], (u8) data[len+2], (u8) data[len+3]);
	return (crc==crc_val) ? 1 : 0;
}


static GF_M2TS_SectionFilter *gf_m2ts_section_filter_new(gf_m2ts_section_callback process_section_callback, Bool process_individual)
{
	GF_M2TS_SectionFilter *sec;
	GF_SAFEALLOC(sec, GF_M2TS_SectionFilter);
	sec->cc = -1;
	sec->process_section = process_section_callback;
	sec->process_individual = process_individual;
	return sec;
}

static void gf_m2ts_reset_sections(GF_List *sections) 
{
	u32 count;
	GF_M2TS_Section *section;

	count = gf_list_count(sections);
	while (count) {
		section = gf_list_get(sections, 0);
		gf_list_rem(sections, 0);
		if (section->data) free(section->data);	
		free(section);
		count--;
	}
}

static void gf_m2ts_section_filter_del(GF_M2TS_SectionFilter *sf)
{
	if (sf->section) free(sf->section);
	while (sf->table) {
		GF_M2TS_Table *t = sf->table;
		sf->table = t->next;
		gf_m2ts_reset_sections(t->sections);
		gf_list_del(t->sections);
		free(t);
	}
	free(sf);
}

void gf_m2ts_es_del(GF_M2TS_ES *es)
{
	gf_list_del_item(es->program->streams, es);
	if (es->flags & GF_M2TS_ES_IS_SECTION) {
		GF_M2TS_SECTION_ES *ses = (GF_M2TS_SECTION_ES *)es;
		if (ses->sec) gf_m2ts_section_filter_del(ses->sec);
	} else if (es->pid!=es->program->pmt_pid) {
		GF_M2TS_PES *pes = (GF_M2TS_PES *)es;
		if (pes->data) free(pes->data);
		if (pes->buf) free(pes->buf);
	}
	free(es);
}

static void gf_m2ts_reset_sdt(GF_M2TS_Demuxer *ts)
{
	while (gf_list_count(ts->SDTs)) {
		GF_M2TS_SDT *sdt = (GF_M2TS_SDT *)gf_list_last(ts->SDTs);
		gf_list_rem_last(ts->SDTs);
		if (sdt->provider) free(sdt->provider);
		if (sdt->service) free(sdt->service);
		free(sdt);
	}
}

static void gf_m2ts_section_complete(GF_M2TS_Demuxer *ts, GF_M2TS_SectionFilter *sec, GF_M2TS_SECTION_ES *ses)
{
	if (! sec->process_section && !sec->had_error) {
		if (ts->on_event) {
			GF_M2TS_SL_PCK pck;
			pck.data_len = sec->length;
			pck.data = (unsigned char*)malloc(sizeof(unsigned char)*pck.data_len);
			memcpy(pck.data, sec->section, sizeof(unsigned char)*pck.data_len);
			//pck.data[pck.data_len] = 0;
			pck.stream = (GF_M2TS_ES *)ses;
			ts->on_event(ts, GF_M2TS_EVT_DVB_GENERAL, &pck);
			free(pck.data);
		}
	} else {
		Bool has_syntax_indicator;
		u8 table_id;
		u16 extended_table_id;
		u32 status, section_start;
		GF_M2TS_Table *t, *prev_t;
		unsigned char *data;
		Bool section_valid = 0;

		status = 0;
		/*parse header*/
		data = sec->section;

		/*look for proper table*/
		table_id = data[0];

		if ((table_id == GF_M2TS_TABLE_ID_PMT || table_id == GF_M2TS_TABLE_ID_NIT_ACTUAL) && ts->on_event) {
			GF_M2TS_SL_PCK pck;
			pck.data_len = sec->length;
			pck.data = (unsigned char*)malloc(sizeof(unsigned char)*pck.data_len);
			memcpy(pck.data, sec->section, sizeof(unsigned char)*pck.data_len);
			//pck.data[pck.data_len] = 0;
			pck.stream = (GF_M2TS_ES *)ses;
			ts->on_event(ts, GF_M2TS_EVT_DVB_GENERAL, &pck);
			free(pck.data);
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
			t->table_id = table_id;
			t->ex_table_id = extended_table_id;
			t->sections = gf_list_new();
			if (prev_t) prev_t->next = t;
			else sec->table = t;
		}

		if (has_syntax_indicator) {
			/*remove crc32*/
			sec->length -= 4;
			if (gf_m2ts_crc32_check(data, sec->length)) {
				s32 cur_sec_num;
				t->version_number = (data[5] >> 1) & 0x1f;
				t->current_next_indicator = (data[5] & 0x1) ? 1 : 0;
				/*add one to section numbers to detect if we missed or not the first section in the table*/
				cur_sec_num = data[6] + 1;	
				t->last_section_number = data[7] + 1;
				section_start = 8;
				section_valid = 1;
				/*we missed something*/
				if (!sec->process_individual && t->section_number + 1 != cur_sec_num) {
					/* TODO - Check how to handle sections when the first complete section does 
					   not have its sec num 0 */
					section_valid = 0;
					GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[MPEG-2 TS] corrupted table (lost section %d)\n", cur_sec_num ? cur_sec_num-1 : 31) );
				}
				t->section_number = cur_sec_num;
			} else {
				GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[MPEG-2 TS] corrupted section (CRC32 failed)\n"));
			}
		} else if (!sec->had_error) {
			section_valid = 1;
			section_start = 3;
		}
		/*process section*/
		if (section_valid) {
			GF_M2TS_Section *section;

			GF_SAFEALLOC(section, GF_M2TS_Section);
			section->data_size = sec->length - section_start;
			section->data = (unsigned char*)malloc(sizeof(unsigned char)*section->data_size);
			memcpy(section->data, sec->section + section_start, sizeof(unsigned char)*section->data_size);
			gf_list_add(t->sections, section);

			if (t->section_number == 1) status |= GF_M2TS_TABLE_START;		

			if (t->is_init) {
				if (t->last_version_number == t->version_number) {
					status |=  GF_M2TS_TABLE_REPEAT;
				} else {
					status |=  GF_M2TS_TABLE_UPDATE;
				}		
			} else {
				status |=  GF_M2TS_TABLE_FOUND;
			}

			t->last_version_number = t->version_number;

			if (t->last_section_number == t->section_number) {
				status |= GF_M2TS_TABLE_END;
				t->is_init = 1;
				/*reset section number*/
				t->section_number = 0;
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
	if (sec->section) free(sec->section);
	sec->section = NULL;
	sec->length = sec->received = 0;
}

static Bool gf_m2ts_is_long_section(u8 table_id)
{
	switch (table_id) {
	case GF_M2TS_TABLE_ID_MPEG4_BIFS:
	case GF_M2TS_TABLE_ID_MPEG4_OD:
	case GF_M2TS_TABLE_ID_EIT_ACTUAL_PF:
	case GF_M2TS_TABLE_ID_EIT_OTHER_PF:
	case GF_M2TS_TABLE_ID_ST:
	case GF_M2TS_TABLE_ID_SIT:
		return 1;
	default:
		if (table_id >= GF_M2TS_TABLE_ID_EIT_SCHEDULE_MIN && table_id <= GF_M2TS_TABLE_ID_EIT_SCHEDULE_MAX) 
			return 1;
		else 
			return 0;
	}
}

static void gf_m2ts_gather_section(GF_M2TS_Demuxer *ts, GF_M2TS_SectionFilter *sec, GF_M2TS_SECTION_ES *ses, GF_M2TS_Header *hdr, unsigned char *data, u32 data_size)
{
	u32 payload_size = data_size;
	u8 expect_cc = (sec->cc<0) ? hdr->continuity_counter : (sec->cc + 1) & 0xf;
	Bool disc = (expect_cc == hdr->continuity_counter) ? 0 : 1;
	sec->cc = expect_cc;

	if (hdr->error || 
		(hdr->adaptation_field==2)) /* 2 = no payload in TS packet */
		return; 

	if (hdr->payload_start) {
		u32 ptr_field;

		ptr_field = data[0];
		if (ptr_field+1>data_size) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[MPEG-2 TS] Invalid section start (@ptr_field=%d, @data_size=%d)\n", ptr_field, data_size) );
			return;
		}

		/*end of previous section*/
		if (sec->length && sec->received + ptr_field >= sec->length) {
			memcpy(sec->section + sec->received, data, sizeof(char)*ptr_field);
			sec->received += ptr_field;
			gf_m2ts_section_complete(ts, sec, ses);
		}
		data += ptr_field+1;
		data_size -= ptr_field+1;
		payload_size -= ptr_field+1;

aggregated_section:

		if (sec->section) free(sec->section);
		sec->length = sec->received = 0;
		sec->section = (char*)malloc(sizeof(char)*data_size);
		memcpy(sec->section, data, sizeof(char)*data_size);
		sec->received = data_size;
		sec->had_error = 0;
	} else if (disc || hdr->error) {
		if (sec->section) free(sec->section);
		sec->section = NULL;
		sec->received = sec->length = 0;
		return;
	} else if (!sec->section) {
		return;
	} else {
		if (sec->received+data_size > sec->length) 
			data_size = sec->length - sec->received;

		if (sec->length) {
			memcpy(sec->section + sec->received, data, sizeof(char)*data_size);
		} else {
			sec->section = (char*)realloc(sec->section, sizeof(char)*(sec->received+data_size));
			memcpy(sec->section + sec->received, data, sizeof(char)*data_size);
		}
		sec->received += data_size;
	}
	if (hdr->error) 
		sec->had_error = 1;

	/*alloc final buffer*/
	if (!sec->length && (sec->received >= 3)) {
		if (gf_m2ts_is_long_section(sec->section[0])) {
			sec->length = 3 + ( ((sec->section[1]<<8) | (sec->section[2]&0xff)) & 0xfff );
		} else {
			sec->length = 3 + ( ((sec->section[1]<<8) | (sec->section[2]&0xff)) & 0x3ff );
		}
		sec->section = (char*)realloc(sec->section, sizeof(char)*sec->length);

		if (sec->received > sec->length) {
			data_size -= sec->received - sec->length;
			sec->received = sec->length;
		}
	}
	if (sec->received < sec->length) return;

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

static void gf_m2ts_process_mpeg4section(GF_M2TS_Demuxer *ts, GF_M2TS_SECTION_ES *es, GF_List *sections, u8 table_id, u16 ex_table_id, u8 version_number, u8 last_section_number, u32 status)
{
	GF_M2TS_SL_PCK sl_pck;
	u32 nb_sections, i;
	GF_M2TS_Section *section;

	/*skip if already received*/
	if (status & GF_M2TS_TABLE_REPEAT) 
		return;

	GF_LOG(GF_LOG_DEBUG, GF_LOG_CONTAINER, ("[MPEG-2 TS] Sections for PID %d\n", es->pid) );
	/*send all sections (eg SL-packets)*/
	nb_sections = gf_list_count(sections);
	for (i=0; i<nb_sections; i++) {
		section = (GF_M2TS_Section *)gf_list_get(sections, i);
		sl_pck.data = section->data;
		sl_pck.data_len = section->data_size;
		sl_pck.stream = (GF_M2TS_ES *)es;
		if (ts->on_event) ts->on_event(ts, GF_M2TS_EVT_SL_PCK, &sl_pck);
	}
}

GF_EXPORT 
void gf_m2ts_decode_mjd_date(u32 date, u32 *year, u32 *month, u32 *day)
{
	u32 yp, mp, k;
	yp = (u32)floor((date - 15078.2)/365.25);
	mp = (u32)floor((date - 14956.1 - floor(yp * 365.25))/30.6001);
	*day = (u32)(date - 14956 - floor(yp * 365.25) - floor(mp * 30.6001));		
	if (mp == 14 || mp == 15) k = 1;
	else k = 0;
	*year = yp + k + 1900;
	*month = mp - 1 - k*12;
}

static void gf_m2ts_process_int(GF_M2TS_Demuxer *ts, GF_M2TS_SECTION_ES *ip_not_table, GF_List *sections, u8 table_id, u16 ex_table_id, u8 version_number, u8 last_section_number, u32 status)
{
	fprintf(stdout, "Processing IP/MAC Notification table (PID %d) %s\n", ip_not_table->pid, (status&GF_M2TS_TABLE_REPEAT)?"repeated":"");
}

#if 0
static void gf_m2ts_process_mpe(GF_M2TS_Demuxer *ts, GF_M2TS_SECTION_ES *mpe, GF_List *sections, u8 table_id, u16 ex_table_id, u8 version_number, u8 last_section_number, u32 status)
{
	fprintf(stdout, "Processing MPE Datagram (PID %d)\n", mpe->pid);
}
#endif

static void gf_m2ts_process_nit(GF_M2TS_Demuxer *ts, GF_M2TS_SECTION_ES *nit_es, GF_List *sections, u8 table_id, u16 ex_table_id, u8 version_number, u8 last_section_number, u32 status)
{
	
	
}

static void gf_m2ts_process_pmt(GF_M2TS_Demuxer *ts, GF_M2TS_SECTION_ES *pmt, GF_List *sections, u8 table_id, u16 ex_table_id, u8 version_number, u8 last_section_number, u32 status)
{
	u32 info_length, pos, desc_len, evt_type, nb_es;
	u32 nb_sections;
	u32 data_size;
	unsigned char *data;
	GF_M2TS_Section *section;

	/*wait for the last section */
	if (!(status&GF_M2TS_TABLE_END)) return;

	nb_es = 0;

	/*skip if already received*/
	if (status&GF_M2TS_TABLE_REPEAT) {
		if (ts->on_event) ts->on_event(ts, GF_M2TS_EVT_PMT_REPEAT, pmt->program);
		return;
	}
	
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
			if (tag == GF_M2TS_MPEG4_IOD_DESCRIPTOR) { 
				u8 scope, label;				
				u32 size;
				GF_BitStream *iod_bs;
				scope = data[6];
				label = data[7];
				iod_bs = gf_bs_new(data+8, len-2, GF_BITSTREAM_READ);
				if (pmt->program->pmt_iod) gf_odf_desc_del((GF_Descriptor *)pmt->program->pmt_iod);
				gf_odf_parse_descriptor(iod_bs , (GF_Descriptor **) &pmt->program->pmt_iod, &size);
				gf_bs_del(iod_bs );
			} else {
				GF_LOG(GF_LOG_DEBUG, GF_LOG_CONTAINER, ("[MPEG-2 TS] Skipping descriptor (0x%x) and others not supported\n", tag));
			}
			first_loop_len += 2 + len;
		}
	}
	if (data_size <= 4 + info_length) return;
	data += 4 + info_length;
	data_size -= 4 + info_length;
	pos = 0;

	while (pos<data_size) {
		GF_M2TS_PES *pes = NULL;
		GF_M2TS_SECTION_ES *ses = NULL;
		GF_M2TS_ES *es = NULL;
		u32 pid, stream_type;
		
		stream_type = data[0];
		pid = ((data[1] & 0x1f) << 8) | data[2];
		desc_len = ((data[3] & 0xf) << 8) | data[4];

		switch (stream_type) {
			/* PES */
			case GF_M2TS_VIDEO_MPEG1: 
			case GF_M2TS_VIDEO_MPEG2:
			case GF_M2TS_AUDIO_MPEG1:
			case GF_M2TS_AUDIO_MPEG2:
			case GF_M2TS_AUDIO_AAC:
			case GF_M2TS_AUDIO_LATM_AAC:
			case GF_M2TS_VIDEO_MPEG4:
			case GF_M2TS_SYSTEMS_MPEG4_PES:
			case GF_M2TS_VIDEO_H264:
			case GF_M2TS_AUDIO_AC3:
			case GF_M2TS_AUDIO_DTS:
			case GF_M2TS_SUBTITLE_DVB:
			case GF_M2TS_PRIVATE_DATA:
				GF_SAFEALLOC(pes, GF_M2TS_PES);
				es = (GF_M2TS_ES *)pes;
				break;
			/* Sections */
			case GF_M2TS_SYSTEMS_MPEG4_SECTIONS:
				GF_SAFEALLOC(ses, GF_M2TS_SECTION_ES);
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

			case GF_M2TS_PRIVATE_SECTION:
			default:
				GF_LOG(GF_LOG_WARNING, GF_LOG_CONTAINER, ("[MPEG-2 TS] Stream type (0x%x) for PID %d not supported\n", stream_type, pid ) );
				break;
		}

		if (es) {
			es->stream_type = stream_type; 
			es->program = pmt->program;
			es->pid = pid;
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
					es->mpeg4_es_id = ((data[2] & 0x1f) << 8) | data[3];
					es->flags |= GF_M2TS_ES_IS_SL;
					break;
				case GF_M2TS_DVB_DATA_BROADCAST_ID_DESCRIPTOR: 
					 {
				 		u32 id = data[2]<<8 | data[3];
						if (id == 0xB) {
					 		ses->sec = gf_m2ts_section_filter_new(NULL, 1);
					 		gf_list_add(ts->ip_mac_not_tables, es);
						}
					 }
					break;
				case GF_M2TS_DVB_SUBTITLING_DESCRIPTOR:
					{
						pes->sub.language[0] = data[2];
						pes->sub.language[1] = data[3];
						pes->sub.language[2] = data[4];
						pes->sub.type = data[5];
						pes->sub.composition_page_id = (data[6]<<8) | data[7];
						pes->sub.ancillary_page_id = (data[8]<<8) | data[9];						
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
				GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[MPEG-2 TS] Invalid PMT es descriptor size for PID %d\n", pes->pid ) );
				break;
			}
			desc_len-=len+2;
		}

		if (!es) continue;

		/*watchout for pmt update*/
		if (/*(status==GF_M2TS_TABLE_UPDATE) && */ts->ess[pid]) {
			GF_M2TS_ES *o_es = ts->ess[es->pid];

			if ((o_es->stream_type == es->stream_type) 
				&& ((o_es->flags & GF_M2TS_ES_STATIC_FLAGS_MASK) == (es->flags & GF_M2TS_ES_STATIC_FLAGS_MASK))
				&& (o_es->mpeg4_es_id == es->mpeg4_es_id)
				&& ((o_es->flags & GF_M2TS_ES_IS_SECTION) || ((GF_M2TS_PES *)o_es)->lang == ((GF_M2TS_PES *)es)->lang)
			) {
				free(es);
				es = NULL;

			}
		}

		if (es) {
			es->stream_type = stream_type; 
			es->program = pmt->program;
			es->pid = pid;
			ts->ess[es->pid] = es;
			gf_list_add(pmt->program->streams, es);

			if (!(es->flags & GF_M2TS_ES_IS_SECTION) ) gf_m2ts_set_pes_framing(pes, GF_M2TS_PES_FRAMING_DEFAULT);

			nb_es++;
		}
	}
	if (nb_es) {
		evt_type = (status&GF_M2TS_TABLE_FOUND) ? GF_M2TS_EVT_PMT_FOUND : GF_M2TS_EVT_PMT_UPDATE;
		if (ts->on_event) ts->on_event(ts, evt_type, pmt->program);
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
		GF_LOG(GF_LOG_WARNING, GF_LOG_CONTAINER, ("PMT on multiple sections not supported\n"));
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

	evt_type = (status&GF_M2TS_TABLE_UPDATE) ? GF_M2TS_EVT_PAT_UPDATE : GF_M2TS_EVT_PAT_FOUND;
	if (ts->on_event) ts->on_event(ts, evt_type, NULL);
}


static GFINLINE u64 gf_m2ts_get_pts(unsigned char *data)
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

static void gf_m2ts_pes_header(GF_M2TS_PES *pes, unsigned char *data, u32 data_size, GF_M2TS_PESHeader *pesh)
{
	u32 has_pts, has_dts, te;
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
	te = data[4];
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
		data+=5;
		len_check += 5;
	}
	if (len_check < pesh->hdr_data_len) {		
		GF_LOG(GF_LOG_DEBUG, GF_LOG_CONTAINER, ("[MPEG-2 TS] PID %d Skipping %d bytes in pes header\n", pes->pid, pesh->hdr_data_len - len_check));
	} else if (len_check > pesh->hdr_data_len) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[MPEG-2 TS] PID %d Wrong pes_header_data_length field %d bytes - read %d\n", pes->pid, pesh->hdr_data_len, len_check));
	}
}


static void gf_m2ts_process_pes(GF_M2TS_Demuxer *ts, GF_M2TS_PES *pes, GF_M2TS_Header *hdr, unsigned char *data, u32 data_size, GF_M2TS_AdaptationField *paf)
{
	Bool flush_pes = 0;
	
	if (hdr->error) {
		if (pes->data) {
			free(pes->data);
			pes->data = NULL;
		}
		pes->data_len = 0;
		pes->pes_len = 0;
		return;
	}
	if (!pes->reframe) return;

	if (hdr->payload_start) {
		flush_pes = 1;
	} else if (pes->pes_len && (pes->data_len + data_size  == pes->pes_len + 6)) { 
		/* 6 = startcode+stream_id+length*/
		/*reassemble pes*/
		if (pes->data) pes->data = (char*)realloc(pes->data, pes->data_len+data_size);
		else pes->data = (char*)malloc(data_size);
		memcpy(pes->data+pes->data_len, data, data_size);
		pes->data_len += data_size;
		/*force discard*/
		data_size = 0;
		flush_pes = 1;
	}

	/*PES first fragment: flush previous packet*/
	if (flush_pes && pes->data) {
		GF_M2TS_PESHeader pesh;
	if (pes->pid==25)
		pes->pid=25;

		/*we need at least a full, valid start code !!*/
		if ((pes->data_len >= 4) && !pes->data[0] && !pes->data[1] && (pes->data[2]==0x1)) {
			u32 len;
            u32 stream_id = pes->data[3] | 0x100;
            if ((stream_id >= 0x1c0 && stream_id <= 0x1df) ||
                  (stream_id >= 0x1e0 && stream_id <= 0x1ef) ||
                  (stream_id == 0x1bd)) {

				/*OK read header*/
				gf_m2ts_pes_header(pes, pes->data+3, pes->data_len-3, &pesh);

				/*3-byte start-code + 6 bytes header + hdr extensions*/
				len = 9 + pesh.hdr_data_len;
				pes->reframe(ts, pes, pesh.DTS, pesh.PTS, pes->data+len, pes->data_len-len);
			} 
			/*SL-packetized stream*/
			else if ((u8) pes->data[3]==0xfa) {
				GF_M2TS_SL_PCK sl_pck;
		if (pes->pid==25)
			pes->pid=25;
				/*read header*/
				gf_m2ts_pes_header(pes, pes->data+3, pes->data_len-3, &pesh);

				/*3-byte start-code + 6 bytes header + hdr extensions*/
				len = 9 + pesh.hdr_data_len;

				GF_LOG(GF_LOG_DEBUG, GF_LOG_CONTAINER, ("[MPEG-2 TS] SL Packet in PES for %d - ES ID %d\n", pes->pid, pes->mpeg4_es_id));
				if (pes->data_len > len) {
					sl_pck.data = pes->data + len;
					sl_pck.data_len = pes->data_len - len;
					sl_pck.stream = (GF_M2TS_ES *)pes;
					if (ts->on_event) ts->on_event(ts, GF_M2TS_EVT_SL_PCK, &sl_pck);
				} else {
					GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[MPEG-2 TS] Bad SL Packet size: (%d indicated < %d header)\n", pes->pid, pes->data_len, len));
				}
			}
		}
		if (pes->data) {
			free(pes->data);
			pes->data = NULL;
			pes->data_len = 0;
			pes->pes_len = 0;
		}
		pes->rap = 0;
		if (!data_size) return;
	}

	/*reassemble*/
	if (pes->data) pes->data = (char*)realloc(pes->data, pes->data_len+data_size);
	else pes->data = (char*)malloc(data_size);
	memcpy(pes->data+pes->data_len, data, data_size);
	pes->data_len += data_size;

	if (paf && paf->random_access_indicator) pes->rap = 1; 
	if (!pes->pes_len && (pes->data_len>=6)) pes->pes_len = (pes->data[4]<<8) | pes->data[5];
}


static void gf_m2ts_get_adaptation_field(GF_M2TS_Demuxer *ts, GF_M2TS_AdaptationField *paf, unsigned char *data, u32 size)
{
	paf->discontinuity_indicator = (data[0] & 0x80) ? 1 : 0;
	paf->random_access_indicator = (data[0] & 0x40) ? 1 : 0;
	paf->priority_indicator = (data[0] & 0x20) ? 1 : 0;
	paf->PCR_flag = (data[0] & 0x10) ? 1 : 0;
	paf->OPCR_flag = (data[0] & 0x8) ? 1 : 0;
	paf->splicing_point_flag = (data[0] & 0x4) ? 1 : 0;
	paf->transport_private_data_flag = (data[0] & 0x2) ? 1 : 0;
	paf->adaptation_field_extension_flag = (data[0] & 0x1) ? 1 : 0;

	if (paf->PCR_flag == 1){
		u32 base = (data[1] << 24) | (data[2] << 16) | (data[3] << 8) | data[4];
		u64 PCR = (u64) base;
		paf->PCR_base = (PCR << 1) | (data[5] >> 7);
		paf->PCR_ext = ((data[5] & 1) << 8) | data[6];
	}

#if 0
	if (paf->OPCR_flag == 1){
		u32 base = (data[7] << 24) | (data[8] << 16) | (data[9] << 8) | data[10];
		u64 PCR = (u64) base;
		paf->PCR_base = (PCR << 1) | (data[11] >> 7);
		paf->PCR_ext = ((data[11] & 1) << 8) | data[12];
	}
#endif	
}

static void gf_m2ts_process_packet(GF_M2TS_Demuxer *ts, unsigned char *data)
{
	GF_M2TS_ES *es;
	GF_M2TS_Header hdr;
	GF_M2TS_AdaptationField af, *paf;
	u32 payload_size, af_size;
	u32 pos = 0;
	
	/* read TS packet header*/
	hdr.sync = data[0];
	hdr.error = (data[1] & 0x80) ? 1 : 0;
	hdr.payload_start = (data[1] & 0x40) ? 1 : 0;
	hdr.priority = (data[1] & 0x20) ? 1 : 0;
	hdr.pid = ( (data[1]&0x1f) << 8) | data[2];
	hdr.scrambling_ctrl = (data[3] >> 6) & 0x3;
	hdr.adaptation_field = (data[3] >> 4) & 0x3;
	hdr.continuity_counter = data[3] & 0xf;

#if DEBUG_TS_PACKET
	GF_LOG(GF_LOG_DEBUG, GF_LOG_CONTAINER, ("[MPEG-2 TS] Packet PID %d\n", hdr.pid));
#endif

	paf = NULL;
	payload_size = 184;
	pos = 4;
	switch (hdr.adaptation_field) {
	/*adaptation+data*/
	case 3: 
		af_size = data[4];
		if (af_size>183) {
			//error
			return;
		}
		paf = &af;
		memset(paf, 0, sizeof(GF_M2TS_AdaptationField));
		gf_m2ts_get_adaptation_field(ts, paf, data+5, af_size);
		pos += 1+af_size;
		payload_size = 183 - af_size;
		break;
	/*adaptation only - still process in cas of PCR*/
	case 2: 
		af_size = data[4];
		if (af_size>183) {
			//error
			return;
		}
		paf = &af;
		memset(paf, 0, sizeof(GF_M2TS_AdaptationField));
		gf_m2ts_get_adaptation_field(ts, paf, data+5, af_size);
		payload_size = 0;
		break;
	/*reserved*/
	case 0:
		return;
	default:
		break;
	}
	data += pos;

	/*PAT*/
	if (hdr.pid == GF_M2TS_PID_PAT) {
		gf_m2ts_gather_section(ts, ts->pat, NULL, &hdr, data, payload_size);
		return;
	} else {
		es = ts->ess[hdr.pid];

		/*check for DVB reserved PIDs*/
		if (!es) {
			if (hdr.pid == GF_M2TS_PID_SDT_BAT_ST) {
				gf_m2ts_gather_section(ts, ts->sdt, NULL, &hdr, data, payload_size);
				return;
			} else if (hdr.pid == GF_M2TS_PID_NIT_ST) {
				/*ignore them, unused at application level*/
				if (!hdr.error) gf_m2ts_gather_section(ts, ts->nit, NULL, &hdr, data, payload_size);
				return;
			} else if (hdr.pid == GF_M2TS_PID_EIT_ST_CIT) {
				/* ignore EIT messages for the moment */
				gf_m2ts_gather_section(ts, ts->eit, NULL, &hdr, data, payload_size); 
				return;
			} else if (hdr.pid == GF_M2TS_PID_TDT_TOT_ST) {
				gf_m2ts_gather_section(ts, ts->tdt_tot_st, NULL, &hdr, data, payload_size); 
			}
		} else if (es->flags & GF_M2TS_ES_IS_SECTION) { 	/* The stream uses sections to carry its payload */
			GF_M2TS_SECTION_ES *ses = (GF_M2TS_SECTION_ES *)es;
			if (ses->sec) gf_m2ts_gather_section(ts, ses->sec, ses, &hdr, data, payload_size);
		} else {
			/* regular stream using PES packets */
			/* let the pes reassembler decide if packets with error shall be discarded*/
			gf_m2ts_process_pes(ts, (GF_M2TS_PES *)es, &hdr, data, payload_size, paf);
		}
	}
	if (paf && paf->PCR_flag) {
		GF_M2TS_PES_PCK pck;
		memset(&pck, 0, sizeof(GF_M2TS_PES_PCK));
		pck.PTS = paf->PCR_base * 300 + paf->PCR_ext; // ???
		pck.stream = (GF_M2TS_PES *)es;
		if (ts->on_event) ts->on_event(ts, GF_M2TS_EVT_PES_PCR, &pck);
	}
	return;
}

GF_Err gf_m2ts_process_data(GF_M2TS_Demuxer *ts, char *data, u32 data_size)
{
	u32 pos;
	Bool is_align = 1;
	if (ts->buffer) {
		if (ts->alloc_size < ts->buffer_size+data_size) {
			ts->alloc_size = ts->buffer_size+data_size;
			ts->buffer = (char*)realloc(ts->buffer, sizeof(char)*ts->alloc_size);
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
			ts->buffer = (char*)malloc(sizeof(char)*data_size);
			memcpy(ts->buffer, data, sizeof(char)*data_size);
			ts->alloc_size = ts->buffer_size = data_size;
		}
		return GF_OK;
	}
	for (;;) {
		/*wait for a complete packet*/
		if (ts->buffer_size - pos < 188) {
			ts->buffer_size -= pos;
			if (!ts->buffer_size) {
				if (!is_align) free(ts->buffer);
				ts->buffer = NULL;
				return GF_OK;
			}
			if (is_align) {
				data = ts->buffer+pos;
				ts->buffer = (char*)malloc(sizeof(char)*ts->buffer_size);
				memcpy(ts->buffer, data, sizeof(char)*ts->buffer_size);
				ts->alloc_size = ts->buffer_size;
			} else {
				memmove(ts->buffer, ts->buffer + pos, sizeof(char)*ts->buffer_size);
			}
			return GF_OK;
		}
		/*process*/
		gf_m2ts_process_packet(ts, ts->buffer+pos);
		pos += 188;
	} 
	return GF_OK;
}

void gf_m2ts_reset_parsers(GF_M2TS_Demuxer *ts)
{
	u32 i;
	for (i=0; i<GF_M2TS_MAX_STREAMS; i++) {
		GF_M2TS_ES *es = (GF_M2TS_ES *) ts->ess[i];
		if (!es) continue;

		if (es->flags & GF_M2TS_ES_IS_SECTION) {
			GF_M2TS_SECTION_ES *ses = (GF_M2TS_SECTION_ES *)es;
			ses->sec->cc = -1;
			ses->sec->length = 0;
			ses->sec->received = 0;
			free(ses->sec->section);
			ses->sec->section = NULL;
			while (ses->sec->table) {
				GF_M2TS_Table *t = ses->sec->table;
				ses->sec->table = t->next;
				gf_m2ts_reset_sections(t->sections);
				gf_list_del(t->sections);
				free(t);
			}
		} else {
			GF_M2TS_PES *pes = (GF_M2TS_PES *)es;
			if (!pes || (pes->pid==pes->program->pmt_pid)) continue;
			pes->frame_state = 0;
			if (pes->data) free(pes->data);
			pes->data = NULL;
			pes->data_len = 0;
			pes->PTS = pes->DTS = 0;
		}
//		free(es);
//		ts->ess[i] = NULL;
	}
/*
	if (ts->pat) {
		ts->pat->cc = -1;
		ts->pat->length = 0;
		ts->pat->received = 0;
		free(ts->pat->section);
		while (ts->pat->table) {
			GF_M2TS_Table *t = ts->pat->table;
			ts->pat->table = t->next;
			if (t->data) free(t->data);
			free(t);
		}
	}
*/
}

static void gf_m2ts_reframe_skip(GF_M2TS_Demuxer *ts, GF_M2TS_PES *pes, u64 DTS, u64 PTS, unsigned char *data, u32 data_len)
{
	u32 res;
	res = 0;
}

GF_Err gf_m2ts_set_pes_framing(GF_M2TS_PES *pes, u32 mode)
{
	/*ignore request for section PIDs*/
	if (pes->flags & GF_M2TS_ES_IS_SECTION) return GF_OK;

	if (pes->pid==pes->program->pmt_pid) return GF_BAD_PARAM;

	if (mode==GF_M2TS_PES_FRAMING_RAW) {
		pes->reframe = gf_m2ts_reframe_default;
		return GF_OK;
	} else if (mode==GF_M2TS_PES_FRAMING_SKIP) {
		pes->reframe = gf_m2ts_reframe_skip;
		return GF_OK;
	} else { // mode==GF_M2TS_PES_FRAMING_DEFAULT
		switch (pes->stream_type) {
		case GF_M2TS_VIDEO_MPEG1: 
		case GF_M2TS_VIDEO_MPEG2:
			pes->reframe = gf_m2ts_reframe_mpeg_video;
			break;
		case GF_M2TS_AUDIO_MPEG1:
		case GF_M2TS_AUDIO_MPEG2:
			pes->reframe = gf_m2ts_reframe_mpeg_audio;
			break;
		case GF_M2TS_VIDEO_H264:
			pes->reframe = gf_m2ts_reframe_avc_h264;
			break;
		case GF_M2TS_AUDIO_LATM_AAC:
			pes->reframe = gf_m2ts_reframe_aac_latm;
			break;
		case GF_M2TS_PRIVATE_DATA:
			/* TODO: handle DVB subtitle streams */
		default:
			pes->reframe = gf_m2ts_reframe_default;
			break;
		}
		return GF_OK;
	}
}

GF_M2TS_Demuxer *gf_m2ts_demux_new()
{
	GF_M2TS_Demuxer *ts;
	GF_SAFEALLOC(ts, GF_M2TS_Demuxer);
	ts->programs = gf_list_new();
	ts->SDTs = gf_list_new();

	ts->pat = gf_m2ts_section_filter_new(gf_m2ts_process_pat, 0);
	ts->sdt = gf_m2ts_section_filter_new(NULL/*gf_m2ts_process_sdt*/, 1);
	ts->nit = gf_m2ts_section_filter_new(gf_m2ts_process_nit, 0);
	ts->eit = gf_m2ts_section_filter_new(NULL/*gf_m2ts_process_eit*/, 1);
	ts->tdt_tot_st = gf_m2ts_section_filter_new(NULL/*gf_m2ts_process_tdt_tot_st*/, 1);
	return ts;
}

void gf_m2ts_demux_del(GF_M2TS_Demuxer *ts)
{
	u32 i;
	if (ts->pat) gf_m2ts_section_filter_del(ts->pat);
	if (ts->sdt) gf_m2ts_section_filter_del(ts->sdt);
	if (ts->nit) gf_m2ts_section_filter_del(ts->nit);
	if (ts->eit) gf_m2ts_section_filter_del(ts->eit);
	if (ts->tdt_tot_st) gf_m2ts_section_filter_del(ts->tdt_tot_st);

	for (i=0; i<8192; i++) {
		if (ts->ess[i]) gf_m2ts_es_del(ts->ess[i]);
	}
	if (ts->buffer) free(ts->buffer);
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
		free(p);
	}
	gf_list_del(ts->programs);

	gf_m2ts_reset_sdt(ts);
	gf_list_del(ts->SDTs);

	free(ts);
}

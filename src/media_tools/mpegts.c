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
#include <gpac/avparse.h>


typedef struct
{
	u8 sync;
	u8 error;
	u8 payload_start;
	u8 priority;
	u16 pid;
	u8 scrambling_ctrl;
	u8 adaptation_field;
	u8 continuity_counter;
} GF_M2TS_Header;

typedef struct
{
	u32 discontinuity_indicator;
	u32 random_access_indicator;
	u32 priority_indicator;

	u32 PCR_flag;
	u64 PCR_base, PCR_ext;

	u32 OPCR_flag;
	u64 OPCR_base, OPCR_ext;

	u32 splicing_point_flag;
	u32 transport_private_data_flag;
	u32 adaptation_field_extension_flag;
/*	
	u32 splice_countdown;
	u32 transport_private_data_length;
	u32 adaptation_field_extension_length;
	u32 ltw_flag;
	u32 piecewise_rate_flag;
	u32 seamless_splice_flag;
	u32 ltw_valid_flag;
	u32 ltw_offset;
	u32 piecewise_rate;
	u32 splice_type;
	u32 DTS_next_AU;
*/
} GF_M2TS_AdaptationField;

typedef struct 
{
	u8 id;
	u16 pck_len;
	u8 data_alignment;
	u64 PTS, DTS;
	u8 hdr_data_len;
} GF_M2TS_PESHeader;


static GF_Err gf_m2ts_report(GF_M2TS_Demuxer *ts, GF_Err e, char *format, ...)
{
	va_list args;
	va_start(args, format);
	if (0) {
		char szMsg[2048];
		vsprintf(szMsg, format, args);
		//parser->load->OnMessage(parser->load->cbk, szMsgFull, e);
	} else {
		vfprintf(stdout, format, args);
		fprintf(stdout, "\n");
	}
	va_end(args);
	return e;
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

static void gf_m2ts_reframe_mpeg_video(GF_M2TS_Demuxer *ts, GF_M2TS_PES *pes, u64 DTS, u64 PTS, unsigned char *data, u32 data_len)
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

	while (sc_pos<data_len) {
		unsigned char *start = memchr(data+sc_pos, 0, data_len-sc_pos);
		if (!start) break;
		sc_pos = start - data;
		/*found picture or sequence start_code*/
		if (!start[1] && (start[2]==1)) {
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
					if (pes->rap) pck.flags |= GF_M2TS_PES_PCK_RAP;
				}

				if (!pes->vid_h && (pes->frame_state==0xb3)) {
					unsigned char *p = data+4;
					pes->vid_w = (p[0] << 4) | ((p[1] >> 4) & 0xf);
					pes->vid_h = ((p[1] & 0xf) << 8) | p[2];
					pes->vid_par = (p[3] >> 4) & 0xf;
					switch (pes->vid_par) {
					default: pes->vid_par = 0; break;
					case 2: pes->vid_par = (4<<16) | 3; break;
					case 3: pes->vid_par = (16<<16) | 9; break;
					case 4: pes->vid_par = (2<<16) | 21; break;
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

	/*resync*/
	if (pos) {
		if (remain) {
			/*sync error!!*/
			if (remain>pos) remain = pos;
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
	if (i) gf_m2ts_report(ts, GF_OK, "re-sync skiped %d bytes", i);
	return i;
}

/* crc32 from ffmpeg*/
static const u32 m2ts_crc_table[256] = {
	0x00000000, 0x04c11db7, 0x09823b6e, 0x0d4326d9, 0x130476dc, 0x17c56b6b,
	0x1a864db2, 0x1e475005, 0x2608edb8, 0x22c9f00f, 0x2f8ad6d6, 0x2b4bcb61,
	0x350c9b64, 0x31cd86d3, 0x3c8ea00a, 0x384fbdbd, 0x4c11db70, 0x48d0c6c7,
	0x4593e01e, 0x4152fda9, 0x5f15adac, 0x5bd4b01b, 0x569796c2, 0x52568b75,
	0x6a1936c8, 0x6ed82b7f, 0x639b0da6, 0x675a1011, 0x791d4014, 0x7ddc5da3,
	0x709f7b7a, 0x745e66cd, 0x9823b6e0, 0x9ce2ab57, 0x91a18d8e, 0x95609039,
	0x8b27c03c, 0x8fe6dd8b, 0x82a5fb52, 0x8664e6e5, 0xbe2b5b58, 0xbaea46ef,
	0xb7a96036, 0xb3687d81, 0xad2f2d84, 0xa9ee3033, 0xa4ad16ea, 0xa06c0b5d,
	0xd4326d90, 0xd0f37027, 0xddb056fe, 0xd9714b49, 0xc7361b4c, 0xc3f706fb,
	0xceb42022, 0xca753d95, 0xf23a8028, 0xf6fb9d9f, 0xfbb8bb46, 0xff79a6f1,
	0xe13ef6f4, 0xe5ffeb43, 0xe8bccd9a, 0xec7dd02d, 0x34867077, 0x30476dc0,
	0x3d044b19, 0x39c556ae, 0x278206ab, 0x23431b1c, 0x2e003dc5, 0x2ac12072,
	0x128e9dcf, 0x164f8078, 0x1b0ca6a1, 0x1fcdbb16, 0x018aeb13, 0x054bf6a4,
	0x0808d07d, 0x0cc9cdca, 0x7897ab07, 0x7c56b6b0, 0x71159069, 0x75d48dde,
	0x6b93dddb, 0x6f52c06c, 0x6211e6b5, 0x66d0fb02, 0x5e9f46bf, 0x5a5e5b08,
	0x571d7dd1, 0x53dc6066, 0x4d9b3063, 0x495a2dd4, 0x44190b0d, 0x40d816ba,
	0xaca5c697, 0xa864db20, 0xa527fdf9, 0xa1e6e04e, 0xbfa1b04b, 0xbb60adfc,
	0xb6238b25, 0xb2e29692, 0x8aad2b2f, 0x8e6c3698, 0x832f1041, 0x87ee0df6,
	0x99a95df3, 0x9d684044, 0x902b669d, 0x94ea7b2a, 0xe0b41de7, 0xe4750050,
	0xe9362689, 0xedf73b3e, 0xf3b06b3b, 0xf771768c, 0xfa325055, 0xfef34de2,
	0xc6bcf05f, 0xc27dede8, 0xcf3ecb31, 0xcbffd686, 0xd5b88683, 0xd1799b34,
	0xdc3abded, 0xd8fba05a, 0x690ce0ee, 0x6dcdfd59, 0x608edb80, 0x644fc637,
	0x7a089632, 0x7ec98b85, 0x738aad5c, 0x774bb0eb, 0x4f040d56, 0x4bc510e1,
	0x46863638, 0x42472b8f, 0x5c007b8a, 0x58c1663d, 0x558240e4, 0x51435d53,
	0x251d3b9e, 0x21dc2629, 0x2c9f00f0, 0x285e1d47, 0x36194d42, 0x32d850f5,
	0x3f9b762c, 0x3b5a6b9b, 0x0315d626, 0x07d4cb91, 0x0a97ed48, 0x0e56f0ff,
	0x1011a0fa, 0x14d0bd4d, 0x19939b94, 0x1d528623, 0xf12f560e, 0xf5ee4bb9,
	0xf8ad6d60, 0xfc6c70d7, 0xe22b20d2, 0xe6ea3d65, 0xeba91bbc, 0xef68060b,
	0xd727bbb6, 0xd3e6a601, 0xdea580d8, 0xda649d6f, 0xc423cd6a, 0xc0e2d0dd,
	0xcda1f604, 0xc960ebb3, 0xbd3e8d7e, 0xb9ff90c9, 0xb4bcb610, 0xb07daba7,
	0xae3afba2, 0xaafbe615, 0xa7b8c0cc, 0xa379dd7b, 0x9b3660c6, 0x9ff77d71,
	0x92b45ba8, 0x9675461f, 0x8832161a, 0x8cf30bad, 0x81b02d74, 0x857130c3,
	0x5d8a9099, 0x594b8d2e, 0x5408abf7, 0x50c9b640, 0x4e8ee645, 0x4a4ffbf2,
	0x470cdd2b, 0x43cdc09c, 0x7b827d21, 0x7f436096, 0x7200464f, 0x76c15bf8,
	0x68860bfd, 0x6c47164a, 0x61043093, 0x65c52d24, 0x119b4be9, 0x155a565e,
	0x18197087, 0x1cd86d30, 0x029f3d35, 0x065e2082, 0x0b1d065b, 0x0fdc1bec,
	0x3793a651, 0x3352bbe6, 0x3e119d3f, 0x3ad08088, 0x2497d08d, 0x2056cd3a,
	0x2d15ebe3, 0x29d4f654, 0xc5a92679, 0xc1683bce, 0xcc2b1d17, 0xc8ea00a0,
	0xd6ad50a5, 0xd26c4d12, 0xdf2f6bcb, 0xdbee767c, 0xe3a1cbc1, 0xe760d676,
	0xea23f0af, 0xeee2ed18, 0xf0a5bd1d, 0xf464a0aa, 0xf9278673, 0xfde69bc4,
	0x89b8fd09, 0x8d79e0be, 0x803ac667, 0x84fbdbd0, 0x9abc8bd5, 0x9e7d9662,
	0x933eb0bb, 0x97ffad0c, 0xafb010b1, 0xab710d06, 0xa6322bdf, 0xa2f33668,
	0xbcb4666d, 0xb8757bda, 0xb5365d03, 0xb1f740b4
};

u32 gf_m2ts_crc32(unsigned char *data, u32 len)
{
    register u32 i;
    u32 crc = 0xffffffff;
    
    for (i=0; i<len; i++)
        crc = (crc << 8) ^ m2ts_crc_table[((crc >> 24) ^ *data++) & 0xff];
    
    return crc;
}

Bool gf_m2ts_crc32_check(unsigned char *data, u32 len)
{
    u32 crc = gf_m2ts_crc32(data, len);
	u32 crc_val = GF_4CC(data[len], data[len+1], data[len+2], data[len+3]);
	return (crc==crc_val) ? 1 : 0;
}


void gf_m2ts_es_del(GF_M2TS_ES *es)
{
	gf_list_del_item(es->program->streams, es);
	if (es->program->pmt_pid==es->pid) {
		GF_M2TS_PMT *pmt = (GF_M2TS_PMT *)es;
		if (pmt->sec) {
			if (pmt->sec->section) free(pmt->sec->section);
			free(pmt->sec);
		}
	} else {
		GF_M2TS_PES *pes = (GF_M2TS_PES *)es;
		if (pes->data) free(pes->data);
	}
	free(es);
}

static GF_M2TS_Section *gf_m2ts_section_filter_new()
{
	GF_M2TS_Section *sec;
	GF_SAFEALLOC(sec, sizeof(GF_M2TS_Section));
	sec->cc = -1;
	return sec;
}

static void gf_m2ts_section_filter_del(GF_M2TS_Section *sf)
{
	if (sf->section) free(sf->section);
	free(sf);
}

static void gf_m2ts_reset_sdt(GF_M2TS_Demuxer *ts)
{
	while (gf_list_count(ts->SDTs)) {
		GF_M2TS_SDT *sdt = gf_list_last(ts->SDTs);
		gf_list_rem_last(ts->SDTs);
		if (sdt->provider) free(sdt->provider);
		if (sdt->service) free(sdt->service);
		free(sdt);
	}
}

static Bool gf_m2ts_gather_section(GF_M2TS_Demuxer *ts, GF_M2TS_Section *sec, GF_M2TS_Header *hdr, unsigned char *data, u32 data_size)
{
	u8 expect_cc = (sec->cc<0) ? hdr->continuity_counter : (sec->cc + 1) & 0xf;
	Bool disc = (expect_cc == hdr->continuity_counter) ? 0 : 1;
	sec->cc = expect_cc;

	if (hdr->adaptation_field==2) return 0;

	if (hdr->payload_start) {
		u32 ptr_field = data[0];
		if (ptr_field+1>data_size) 
			//error
			return data_size;
		data += ptr_field+1;
		data_size -= ptr_field+1;

		sec->section_len = sec->section_recv = 0;
		if (sec->section) free(sec->section);
		sec->section = malloc(sizeof(char)*data_size);
		memcpy(sec->section, data, sizeof(char)*data_size);
		sec->section_recv = data_size;
	} else if (disc || hdr->error) {
		if (sec->section) free(sec->section);
		sec->section = NULL;
		sec->section_recv = sec->section_len = 0;
		return 0;
	} else if (!sec->section) {
		return 0;
	} else {
		if (sec->section_len) {
			memcpy(sec->section + sec->section_recv, data, sizeof(char)*data_size);
		} else {
			sec->section = realloc(sec->section, sizeof(char)*(sec->section_recv+data_size));
			memcpy(sec->section + sec->section_recv, data, sizeof(char)*data_size);
		}
		sec->section_recv += data_size;
	}

	/*alloc final buffer*/
	if (!sec->section_len && (sec->section_recv >= 3)) {
		sec->section_len = 3 + ((sec->section[1]<<8) | sec->section[2]) & 0x3ff;
		sec->section = realloc(sec->section, sizeof(char)*sec->section_len);
	}
	if (sec->section_recv < sec->section_len) return 0;

	/*parse header*/
	data = sec->section;
	sec->table_id = data[0]; 
	sec->syntax_indicator = (data[1] & 0x80) ? 1 : 0;
	/* pas->section_len = ((data[1]<<8) | data[2]) & 0x3ff; */
	if (sec->syntax_indicator) {
		/*remove crc32*/
		sec->section_len -= 4;
		if ( gf_m2ts_crc32_check(data, sec->section_len)) {
			sec->sec_id = (data[3]<<8) | data[4];
			sec->version_number = (data[5] >> 1) & 0x1f;
			sec->current_next_indicator = (data[5] & 0x1) ? 1 : 0;
			sec->section_number = data[6];
			sec->last_section_number = data[7];
			sec->start = 8;
			return 1;
		} else {
			gf_m2ts_report(ts, GF_OK, "Corrupted section (CRC32 failed)\n");
		}
	} else {
		sec->start = 3;
		return 1;
	}
	/*broken section*/
	free(sec->section);
	memset(sec, 0, sizeof(GF_M2TS_Section));
	sec->cc = -1;
	return 0;
}

static void gf_m2ts_process_pat(GF_M2TS_Demuxer *ts, GF_M2TS_Header *hdr, unsigned char *data, u32 data_size)
{
	GF_M2TS_Program *prog;
	GF_M2TS_PMT *pmt;
	u32 i, nb_progs, evt_type;
	if (!ts->pas) ts->pas = gf_m2ts_section_filter_new();

	if (!gf_m2ts_gather_section(ts, ts->pas, hdr, data, data_size)) return;

	/*skip if already received*/
	if (ts->pas->section_init && (ts->pas->section_number == ts->pas->last_section_number)) {
		if (ts->on_event) ts->on_event(ts, GF_M2TS_EVT_PAT_REPEAT, NULL);
		return;
	}

	nb_progs = (ts->pas->section_len - ts->pas->start) / 4;
	data = ts->pas->section + ts->pas->start;

	for (i=0; i<nb_progs; i++) {
		GF_SAFEALLOC(prog, sizeof(GF_M2TS_Program));
		prog->streams = gf_list_new();
		gf_list_add(ts->programs, prog);
		prog->number = (data[0]<<8) | data[1];
		prog->pmt_pid = (data[2]&0x1f)<<8 | data[3];
		data += 4;
		GF_SAFEALLOC(pmt, sizeof(GF_M2TS_PMT));
		gf_list_add(prog->streams, pmt);
		pmt->pid = prog->pmt_pid;
		pmt->program = prog;
		ts->ess[pmt->pid] = (GF_M2TS_ES *)pmt;
		pmt->sec = gf_m2ts_section_filter_new();
	}

	evt_type = ts->pas->section_init ? GF_M2TS_EVT_PAT_UPDATE : GF_M2TS_EVT_PAT_FOUND;
	ts->pas->section_init = 1;
	if (ts->on_event) ts->on_event(ts, evt_type, NULL);
}

static void gf_m2ts_process_sdt(GF_M2TS_Demuxer *ts, GF_M2TS_Header *hdr, unsigned char *data, u32 data_size)
{
	u32 orig_net_id, pos, evt_type;
	if (!ts->sdt) ts->sdt = gf_m2ts_section_filter_new();
	if (!gf_m2ts_gather_section(ts, ts->sdt, hdr, data, data_size)) return;

	/*skip if already received*/
	if (ts->sdt->section_init && (ts->sdt->section_number == ts->sdt->last_section_number)) {
		if (ts->on_event) ts->on_event(ts, GF_M2TS_EVT_SDT_UPDATE, NULL);
		return;
	}

	/*reset service desc*/
	gf_m2ts_reset_sdt(ts);

	data = ts->sdt->section + ts->sdt->start;
	orig_net_id = (data[0] << 8) | data[1];
	pos = 3;
	while (ts->sdt->start + pos < ts->sdt->section_len) {
		GF_M2TS_SDT *sdt;
		u32 descs_size, d_pos, ulen;
		
		GF_SAFEALLOC(sdt, sizeof(GF_M2TS_SDT));
		gf_list_add(ts->SDTs, sdt);

		sdt->service_id = (data[pos]<<8) + data[pos+1];
		sdt->EIT_schedule = (data[pos+2] & 0x2) ? 1 : 0;
		sdt->EIT_present_following = (data[pos+2] & 0x1);
		sdt->running_status = (data[pos+3]>>5) & 0x7;
		sdt->free_CA_mode = (data[pos+3]>>4) & 0x1;
		descs_size = ((data[pos+3]&0xf)<<8) | data[pos+4];
		pos += 5;

		d_pos = 0;
		while (d_pos<descs_size) {
			u8 d_tag = data[pos+d_pos];
			u8 d_len = data[pos+d_pos+1];

			switch (d_tag) {
			/*service_descriptor */
			case 0x48: 
				if (sdt->provider) free(sdt->provider);
				sdt->provider = NULL;
				if (sdt->service) free(sdt->service);
				sdt->service = NULL;
				
				d_pos+=2;
				sdt->service_type = data[pos+d_pos];
				ulen = data[pos+d_pos+1];
				d_pos += 2;
				sdt->provider = malloc(sizeof(char)*(ulen+1));
				memcpy(sdt->provider, data+pos+d_pos, sizeof(char)*ulen);
				sdt->provider[ulen] = 0;
				d_pos += ulen;

				ulen = data[pos+d_pos];
				d_pos += 1;
				sdt->service = malloc(sizeof(char)*(ulen+1));
				memcpy(sdt->service, data+pos+d_pos, sizeof(char)*ulen);
				sdt->service[ulen] = 0;
				d_pos += ulen;
				break;

			default:
				d_pos += d_len;
				break;
			}
		}
		pos += descs_size;
	}
	evt_type = GF_M2TS_EVT_SDT_FOUND;
	ts->sdt->section_init = 1;
	if (ts->on_event) ts->on_event(ts, evt_type, NULL);
}

static void gf_m2ts_process_pmt(GF_M2TS_Demuxer *ts, GF_M2TS_PMT *pmt, GF_M2TS_Header *hdr, unsigned char *data, u32 data_size)
{
	GF_M2TS_PES *pes;
	u32 info_length, pos, desc_len, evt_type;
	if (!pmt->sec) pmt->sec = gf_m2ts_section_filter_new();

	if (!gf_m2ts_gather_section(ts, pmt->sec, hdr, data, data_size)) return;

	/*skip if already received*/
	if (pmt->sec->section_init && (pmt->sec->section_number == pmt->sec->last_section_number)) {
		if (ts->on_event) ts->on_event(ts, GF_M2TS_EVT_PMT_UPDATE, pmt->program);
		return;
	}

	if (pmt->sec->sec_id != pmt->program->number) return;

	data = pmt->sec->section + pmt->sec->start;
	pmt->program->pcr_pid = ((data[0] & 0x1f) << 8) | data[1];

	/*skip descriptor - !! IOD should be somewhere around here !! */
	info_length = ((data[2]&0xf)<<8) | data[3];
	if (pmt->sec->start + 4 + info_length > pmt->sec->section_len) return;
	data = pmt->sec->section + pmt->sec->start + 4 + info_length;
	pos = pmt->sec->start + 4 + info_length;

	while (pos<pmt->sec->section_len) {
		u32 d_pos;
		GF_SAFEALLOC(pes, sizeof(GF_M2TS_PES));
		pes->program = pmt->program;
		gf_list_add(pmt->program->streams, pes);
		pes->stream_type = data[0];
		pes->pid = ((data[1] & 0x1f) << 8) | data[2];
		ts->ess[pes->pid] = (GF_M2TS_ES *) pes;
		desc_len = ((data[3] & 0xf) << 8) | data[4];
		pos += 5;
		data += 5;

		d_pos = 0;
		while (d_pos<desc_len) {
			u8 d_tag = data[d_pos];
			u8 d_len = data[d_pos+1];
			d_pos+=2;
			switch (d_tag) {
			case 0x0a:
				pes->lang = GF_4CC(' ', data[d_pos], data[d_pos+1], data[d_pos+2]);
				break;
			default:
				break;
			}
			d_pos += d_len;
		}
		pos += desc_len;
		data += desc_len;
		gf_m2ts_set_pes_framing(pes, GF_M2TS_PES_FRAMING_DEFAULT);
	}
	evt_type = GF_M2TS_EVT_PMT_FOUND;
	pmt->sec->section_init = 1;
	if (ts->on_event) ts->on_event(ts, evt_type, pmt->program);
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

static void gf_m2ts_pes_header(unsigned char *data, u32 data_size, GF_M2TS_PESHeader *pesh)
{
	Bool has_pts, has_dts;
	memset(pesh, 0, sizeof(GF_M2TS_PESHeader));

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
	has_dts = (data[4]&0x40);
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
	}
	if (has_dts) {
		pesh->DTS = gf_m2ts_get_pts(data);
		data+=5;
	}
}


static void gf_m2ts_process_pes(GF_M2TS_Demuxer *ts, GF_M2TS_PES *pes, GF_M2TS_Header *hdr, unsigned char *data, u32 data_size, GF_M2TS_AdaptationField *paf)
{
	if (paf && paf->PCR_flag) {
		GF_M2TS_PES_PCK pck;
		memset(&pck, 0, sizeof(GF_M2TS_PES_PCK));
		pck.PTS = paf->PCR_base * 300 + paf->PCR_ext;
		pck.stream = pes;
		if (ts->on_event) ts->on_event(ts, GF_M2TS_EVT_PES_PCR, &pck);
	}

	if (!pes->reframe) return;

	/*PES first fragment: flush previous packet*/
	if (hdr->payload_start && pes->data) {
		GF_M2TS_PESHeader pesh;

		/*we need at least a full start code !!*/
		assert(pes->data_len >= 4);

		/*check start_code */
		if (!pes->data[0] && !pes->data[1] && (pes->data[2]==0x1)) {
            u32 var = pes->data[3] | 0x100;
            if ((var >= 0x1c0 && var <= 0x1df) ||
                  (var >= 0x1e0 && var <= 0x1ef) ||
                  (var == 0x1bd)) {

				/*OK read header*/
				gf_m2ts_pes_header(pes->data+3, pes->data_len-3, &pesh);

				/*remember first DTS we found on this program - used by media importers*/
//				if (!pes->program->first_dts) pes->program->first_dts = pesh.DTS ? pesh.DTS : pesh.PTS;

				/*3-byte start-code + 6 bytes header + hdr extensions*/
				var = 9 + pesh.hdr_data_len;
				pes->reframe(ts, pes, pesh.DTS, pesh.PTS, pes->data+var, pes->data_len-var);
			}
		}
		if (pes->data) {
			free(pes->data);
			pes->data = NULL;
			pes->data_len = 0;
		}
		pes->rap = 0;
	}

	/*reassemble*/
	pes->data = realloc(pes->data, pes->data_len+data_size);
	memcpy(pes->data+pes->data_len, data, data_size);
	pes->data_len += data_size;

	if (paf && paf->random_access_indicator) pes->rap = 1; 
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
		u64 PCR = (data[1] << 24) | (data[2] << 16) | (data[3] << 8) | data[4];
		paf->PCR_base = (PCR << 1) | (data[5] >> 7);
		paf->PCR_ext = ((data[5] & 1) << 8) | data[6];
	}

#if 0
	if (paf->OPCR_flag == 1){
		u64 PCR = (data[7] << 24) | (data[8] << 16) | (data[9] << 8) | data[10];
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
	/*process header*/
	hdr.sync = data[0];
	hdr.error = (data[1] & 0x80) ? 1 : 0;
	hdr.payload_start = (data[1] & 0x40) ? 1 : 0;
	hdr.priority = (data[1] & 0x20) ? 1 : 0;
	hdr.pid = ( (data[1]&0x1f) << 8) | data[2];
	hdr.scrambling_ctrl = (data[3] >> 6) & 0x3;
	hdr.adaptation_field = (data[3] >> 4) & 0x3;
	hdr.continuity_counter = data[3] & 0xf;

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
	/*reserved*/
	case 0:
		return;
	default:
		break;
	}
	data += pos;

	/*PAT*/
	if (hdr.pid == 0) {
		if (!hdr.error) gf_m2ts_process_pat(ts, &hdr, data, payload_size);
		return;
	}
	/*SDT*/
	else if (hdr.pid == 0x0011) {
		if (!hdr.error) gf_m2ts_process_sdt(ts, &hdr, data, payload_size);
		return;
	}
	/*NIT*/
	if (hdr.pid == 0x0010) {
		/*ignore them, unused at application level*/
		return;
	}

	es = ts->ess[hdr.pid];
	if (!es) return;
	/*pmt*/
	if (es->program->pmt_pid==hdr.pid) {
		if (!hdr.error) gf_m2ts_process_pmt(ts, (GF_M2TS_PMT *)es, &hdr, data, payload_size);
	}
	/*regular es - let the pes reassembler decide if packets with error shall be discarded*/
	else {
		gf_m2ts_process_pes(ts, (GF_M2TS_PES *)es, &hdr, data, payload_size, paf);
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
			ts->buffer = realloc(ts->buffer, sizeof(char)*ts->alloc_size);
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
			ts->buffer = malloc(sizeof(char)*data_size);
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
				ts->buffer = malloc(sizeof(char)*ts->buffer_size);
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
		GF_M2TS_PES *pes = (GF_M2TS_PES *) ts->ess[i];
		if (!pes || (pes->pid==pes->program->pmt_pid)) continue;
		pes->frame_state = 0;
		if (pes->data) free(pes->data);
		pes->data = NULL;
		pes->data_len = 0;
		pes->PTS = pes->DTS = 0;
		pes->program->first_dts = 0;
	}
}

GF_Err gf_m2ts_set_pes_framing(GF_M2TS_PES *pes, u32 mode)
{
	if (pes->pid==pes->program->pmt_pid) return GF_BAD_PARAM;
	if (mode==GF_M2TS_PES_FRAMING_RAW) {
		pes->reframe = gf_m2ts_reframe_default;
		return GF_OK;
	}
	if (mode==GF_M2TS_PES_FRAMING_SKIP) {
		pes->reframe = NULL;
		return GF_OK;
	}
	switch (pes->stream_type) {
	case 1: case 2:
		pes->reframe = gf_m2ts_reframe_mpeg_video;
		break;
	case 3: case 4:
		pes->reframe = gf_m2ts_reframe_mpeg_audio;
		break;
	default:
		pes->reframe = gf_m2ts_reframe_default;
		break;
	}
	return GF_OK;
}

GF_M2TS_Demuxer *gf_m2ts_demux_new()
{
	GF_M2TS_Demuxer *ts;
	GF_SAFEALLOC(ts, sizeof(GF_M2TS_Demuxer));
	ts->programs = gf_list_new();
	ts->SDTs = gf_list_new();
	return ts;
}

void gf_m2ts_demux_del(GF_M2TS_Demuxer *ts)
{
	u32 i;
	if (ts->pas) gf_m2ts_section_filter_del(ts->pas);
	if (ts->sdt) gf_m2ts_section_filter_del(ts->sdt);
	if (ts->nit) gf_m2ts_section_filter_del(ts->nit);

	for (i=0; i<8192; i++) {
		if (ts->ess[i]) gf_m2ts_es_del(ts->ess[i]);
	}
	if (ts->buffer) free(ts->buffer);
	while (gf_list_count(ts->programs)) {
		GF_M2TS_Program *p = gf_list_last(ts->programs);
		gf_list_rem_last(ts->programs);
		gf_list_del(p->streams);
		free(p);
	}
	gf_list_del(ts->programs);

	gf_m2ts_reset_sdt(ts);
	gf_list_del(ts->SDTs);

	free(ts);
}

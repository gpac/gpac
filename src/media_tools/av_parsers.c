/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Copyright (c) Jean Le Feuvre 2000-2005 
 *					All rights reserved
 *
 *  This file is part of GPAC / Media Tools sub-project
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

#include <gpac/internal/media_dev.h>
#include <gpac/internal/ogg.h>
#include <gpac/constants.h>
#include <gpac/math.h>

/*
	MPEG-4 video (14496-2)
*/

#define M4V_VO_START_CODE					0x00
#define M4V_VOL_START_CODE					0x20
#define M4V_VOP_START_CODE					0xB6
#define M4V_VISOBJ_START_CODE				0xB5
#define M4V_VOS_START_CODE					0xB0
#define M4V_GOV_START_CODE					0xB3
#define M4V_UDTA_START_CODE					0xB2


#define M2V_PIC_START_CODE					0x00
#define M2V_SEQ_START_CODE					0xB3
#define M2V_EXT_START_CODE					0xB5
#define M2V_GOP_START_CODE					0xB8

struct __tag_m4v_parser
{
	GF_BitStream *bs;
	Bool mpeg12;
	u32 current_object_type;
	u32 current_object_start;
	u32 tc_dec, prev_tc_dec, tc_disp, prev_tc_disp;
};

GF_EXPORT
GF_M4VParser *gf_m4v_parser_new(char *data, u32 data_size, Bool mpeg12video)
{
	GF_M4VParser *tmp;
	if (!data || !data_size) return NULL;
	GF_SAFEALLOC(tmp, GF_M4VParser);
	tmp->bs = gf_bs_new(data, data_size, GF_BITSTREAM_READ);
	tmp->mpeg12 = mpeg12video;
	return tmp;
}

GF_M4VParser *gf_m4v_parser_bs_new(GF_BitStream *bs, Bool mpeg12video)
{
	GF_M4VParser *tmp;
	GF_SAFEALLOC(tmp, GF_M4VParser);
	tmp->bs = bs;
	tmp->mpeg12 = mpeg12video;
	return tmp;
}

GF_EXPORT
void gf_m4v_parser_del(GF_M4VParser *m4v)
{
	gf_bs_del(m4v->bs);
	free(m4v);
}


#define M4V_CACHE_SIZE		4096
s32 M4V_LoadObject(GF_M4VParser *m4v)
{
	u32 v, bpos, found;
	char m4v_cache[M4V_CACHE_SIZE];
	u64 end, cache_start, load_size;
	if (!m4v) return 0;
	bpos = 0;
	found = 0;
	load_size = 0;
	end = 0;
	cache_start = 0;
	v = 0xffffffff;
	while (!end) {
		/*refill cache*/
		if (bpos == (u32) load_size) {
			if (!gf_bs_available(m4v->bs)) break;
			load_size = gf_bs_available(m4v->bs);
			if (load_size>M4V_CACHE_SIZE) load_size=M4V_CACHE_SIZE;
			bpos = 0;
			cache_start = gf_bs_get_position(m4v->bs);
			gf_bs_read_data(m4v->bs, m4v_cache, (u32) load_size);
		}
		v = ( (v<<8) & 0xFFFFFF00) | ((u8) m4v_cache[bpos]);
		bpos++;
		if ((v & 0xFFFFFF00) == 0x00000100) {
			end = cache_start+bpos-4;
			found = 1;
			break;
		}
	}
	if (!found) return -1;
	m4v->current_object_start = (u32) end;
	gf_bs_seek(m4v->bs, end+3);
	m4v->current_object_type = gf_bs_read_u8(m4v->bs);
	return (s32) m4v->current_object_type;
}

GF_EXPORT
const char *gf_m4v_get_profile_name(u8 video_pl)
{
	switch (video_pl) {
	case 0x00: return "Reserved (0x00) Profile";
	case 0x01: return "Simple Profile @ Level 1";
	case 0x02: return "Simple Profile @ Level 2";
	case 0x03: return "Simple Profile @ Level 3";
	case 0x08: return "Simple Profile @ Level 0";
	case 0x10: return "Simple Scalable Profile @ Level 0";
	case 0x11: return "Simple Scalable Profile @ Level 1";
	case 0x12: return "Simple Scalable Profile @ Level 2";
	case 0x15: return "AVC/H264 Profile";
	case 0x21: return "Core Profile @ Level 1";
	case 0x22: return "Core Profile @ Level 2";
	case 0x32: return "Main Profile @ Level 2";
	case 0x33: return "Main Profile @ Level 3";
	case 0x34: return "Main Profile @ Level 4";
	case 0x42: return "N-bit Profile @ Level 2";
	case 0x51: return "Scalable Texture Profile @ Level 1";
	case 0x61: return "Simple Face Animation Profile @ Level 1";
	case 0x62: return "Simple Face Animation Profile @ Level 2";
	case 0x63: return "Simple FBA Profile @ Level 1";
	case 0x64: return "Simple FBA Profile @ Level 2";
	case 0x71: return "Basic Animated Texture Profile @ Level 1";
	case 0x72: return "Basic Animated Texture Profile @ Level 2";
	case 0x81: return "Hybrid Profile @ Level 1";
	case 0x82: return "Hybrid Profile @ Level 2";
	case 0x91: return "Advanced Real Time Simple Profile @ Level 1";
	case 0x92: return "Advanced Real Time Simple Profile @ Level 2";
	case 0x93: return "Advanced Real Time Simple Profile @ Level 3";
	case 0x94: return "Advanced Real Time Simple Profile @ Level 4";
	case 0xA1: return "Core Scalable Profile @ Level1";
	case 0xA2: return "Core Scalable Profile @ Level2";
	case 0xA3: return "Core Scalable Profile @ Level3";
	case 0xB1: return "Advanced Coding Efficiency Profile @ Level 1";
	case 0xB2: return "Advanced Coding Efficiency Profile @ Level 2";
	case 0xB3: return "Advanced Coding Efficiency Profile @ Level 3";
	case 0xB4: return "Advanced Coding Efficiency Profile @ Level 4";
	case 0xC1: return "Advanced Core Profile @ Level 1";
	case 0xC2: return "Advanced Core Profile @ Level 2";
	case 0xD1: return "Advanced Scalable Texture @ Level1";
	case 0xD2: return "Advanced Scalable Texture @ Level2";
	case 0xE1: return "Simple Studio Profile @ Level 1";
	case 0xE2: return "Simple Studio Profile @ Level 2";
	case 0xE3: return "Simple Studio Profile @ Level 3";
	case 0xE4: return "Simple Studio Profile @ Level 4";
	case 0xE5: return "Core Studio Profile @ Level 1";
	case 0xE6: return "Core Studio Profile @ Level 2";
	case 0xE7: return "Core Studio Profile @ Level 3";
	case 0xE8: return "Core Studio Profile @ Level 4";
	case 0xF0: return "Advanced Simple Profile @ Level 0";
	case 0xF1: return "Advanced Simple Profile @ Level 1";
	case 0xF2: return "Advanced Simple Profile @ Level 2";
	case 0xF3: return "Advanced Simple Profile @ Level 3";
	case 0xF4: return "Advanced Simple Profile @ Level 4";
	case 0xF5: return "Advanced Simple Profile @ Level 5";
	case 0xF7: return "Advanced Simple Profile @ Level 3b";
	case 0xF8: return "Fine Granularity Scalable Profile @ Level 0";
	case 0xF9: return "Fine Granularity Scalable Profile @ Level 1";
	case 0xFA: return "Fine Granularity Scalable Profile @ Level 2";
	case 0xFB: return "Fine Granularity Scalable Profile @ Level 3";
	case 0xFC: return "Fine Granularity Scalable Profile @ Level 4";
	case 0xFD: return "Fine Granularity Scalable Profile @ Level 5";
	case 0xFE: return "Not part of MPEG-4 Visual profiles";
	case 0xFF: return "No visual capability required";
	default: return "ISO Reserved Profile";
	}
}

GF_EXPORT
void gf_m4v_rewrite_pl(char **o_data, u32 *o_dataLen, u8 PL)
{
	u32 pos = 0;
	unsigned char *data = (unsigned char *)*o_data;
	u32 dataLen = *o_dataLen;

	while (pos+4<dataLen) {
		if (!data[pos] && !data[pos+1] && (data[pos+2]==0x01) && (data[pos+3]==M4V_VOS_START_CODE)) {
			data[pos+4] = PL;
			return;
		}
		pos ++;
	}
	/*emulate VOS at beggining*/
	(*o_data) = (char *)malloc(sizeof(char)*(dataLen+5));
	(*o_data)[0] = 0;
	(*o_data)[1] = 0;
	(*o_data)[2] = 1;
	(*o_data)[3] = (char) M4V_VOS_START_CODE;
	(*o_data)[4] = PL;
	memcpy( (*o_data + 5), data, sizeof(char)*dataLen);
	free(data);
	(*o_dataLen) = dataLen + 5;
}

static GF_Err M4V_Reset(GF_M4VParser *m4v, u32 start)
{
	gf_bs_seek(m4v->bs, start);
	m4v->current_object_start = start;
	m4v->current_object_type = 0;
	return GF_OK;
}


static GF_Err gf_m4v_parse_config_mpeg12(GF_M4VParser *m4v, GF_M4VDecSpecInfo *dsi)
{
	char p[4];
	u32 ext_type;
	s32 o_type;
	u8 go, par;

	if (!m4v || !dsi) return GF_BAD_PARAM;

	memset(dsi, 0, sizeof(GF_M4VDecSpecInfo));
	dsi->VideoPL = 0;

	go = 1;
	while (go) {
		o_type = M4V_LoadObject(m4v);
		switch (o_type) {
		case M2V_SEQ_START_CODE:
			dsi->RAP_stream = 1;
			gf_bs_read_data(m4v->bs, p, 4);
			dsi->width = (p[0] << 4) | ((p[1] >> 4) & 0xf);
			dsi->height = ((p[1] & 0xf) << 8) | p[2];

			dsi->VideoPL = 0x6A;
			par = (p[3] >> 4) & 0xf;
			switch (par) {
			case 2: dsi->par_num = dsi->height/3; dsi->par_den = dsi->width/4; break;
			case 3: dsi->par_num = dsi->height/9; dsi->par_den = dsi->width/16; break;
			case 4: dsi->par_num = dsi->height/2; dsi->par_den = dsi->width/21; break;
			default: dsi->par_den = dsi->par_num = 0; break;
			}
			switch (p[3] & 0xf) {
			case 0: break;
			case 1: dsi->fps = 24000.0/1001.0; break;
			case 2: dsi->fps = 24.0; break;
			case 3: dsi->fps = 25.0; break;
			case 4: dsi->fps = 30000.0/1001.0; break;
			case 5: dsi->fps = 30.0; break;
			case 6: dsi->fps = 50.0; break;
			case 7: dsi->fps = ((60.0*1000.0)/1001.0); break;
			case 8: dsi->fps = 60.0; break;
			case 9: dsi->fps = 1; break;
			case 10: dsi->fps = 5; break;
			case 11: dsi->fps = 10; break;
			case 12: dsi->fps = 12; break;
			case 13: dsi->fps = 15; break;
			}
			break;
		case M2V_EXT_START_CODE:
			gf_bs_read_data(m4v->bs, p, 4);
			ext_type = ((p[0] >> 4) & 0xf);
			if (ext_type == 1) {
			  dsi->VideoPL = 0x65;
			  dsi->height = ((p[1] & 0x1) << 13) | ((p[2] & 0x80) << 5) | (dsi->height & 0x0fff);
			  dsi->width = (((p[2] >> 5) & 0x3) << 12) | (dsi->width & 0x0fff);
			}
			break;
		case M2V_PIC_START_CODE:
			if (dsi->width) go = 0;
			break;
		default:
			break;
		/*EOS*/
		case -1:
			go = 0;
			m4v->current_object_start = (u32) gf_bs_get_position(m4v->bs);
			break;
		}
	}
	M4V_Reset(m4v, 0);
	return GF_OK;
}


static const struct { u32 w, h; } m4v_sar[6] = { { 0,   0 }, { 1,   1 }, { 12, 11 }, { 10, 11 }, { 16, 11 }, { 40, 33 } };

static u8 m4v_get_sar_idx(u32 w, u32 h)
{
	u32 i;
	for (i=0; i<6; i++) {
		if ((m4v_sar[i].w==w) && (m4v_sar[i].h==h)) return i;
	}
	return 0xF;
}

static GF_Err gf_m4v_parse_config_mpeg4(GF_M4VParser *m4v, GF_M4VDecSpecInfo *dsi)
{
	s32 o_type;
	u8 go, verid, par;
	s32 clock_rate;

	if (!m4v || !dsi) return GF_BAD_PARAM;

	memset(dsi, 0, sizeof(GF_M4VDecSpecInfo));

	go = 1;
	while (go) {
		o_type = M4V_LoadObject(m4v);
		switch (o_type) {
		/*vosh*/
		case M4V_VOS_START_CODE:
			dsi->VideoPL = (u8) gf_bs_read_u8(m4v->bs);
			break;

		case M4V_VOL_START_CODE:
			verid = 0;
			dsi->RAP_stream = gf_bs_read_int(m4v->bs, 1);
			dsi->objectType = gf_bs_read_int(m4v->bs, 8);
			if (gf_bs_read_int(m4v->bs, 1)) {
				verid = gf_bs_read_int(m4v->bs, 4);
				gf_bs_read_int(m4v->bs, 3);
			}
			par = gf_bs_read_int(m4v->bs, 4);
			if (par == 0xF) {
				dsi->par_num = gf_bs_read_int(m4v->bs, 8);
				dsi->par_den = gf_bs_read_int(m4v->bs, 8);
			} else if (par<6) {
				dsi->par_num = m4v_sar[par].w;
				dsi->par_den = m4v_sar[par].h;
			}
			if (gf_bs_read_int(m4v->bs, 1)) {
				gf_bs_read_int(m4v->bs, 3);
				if (gf_bs_read_int(m4v->bs, 1)) gf_bs_read_int(m4v->bs, 79);
			}
			dsi->has_shape = gf_bs_read_int(m4v->bs, 2);
			if (dsi->has_shape && (verid!=1) ) gf_bs_read_int(m4v->bs, 4);
			gf_bs_read_int(m4v->bs, 1);
			/*clock rate*/
			dsi->clock_rate = gf_bs_read_int(m4v->bs, 16);
			/*marker*/
			gf_bs_read_int(m4v->bs, 1);

			clock_rate = dsi->clock_rate-1;
			if (clock_rate >= 65536) clock_rate = 65535;
			if (clock_rate > 0) {
				for (dsi->NumBitsTimeIncrement = 1; dsi->NumBitsTimeIncrement < 16; dsi->NumBitsTimeIncrement++)	{	
					if (clock_rate == 1) break;
					clock_rate = (clock_rate >> 1);
				}
			} else {
				/*fix from vivien for divX*/
				dsi->NumBitsTimeIncrement = 1;
			}			
			/*fixed FPS stream*/
			dsi->time_increment = 0;
			if (gf_bs_read_int(m4v->bs, 1)) {
				dsi->time_increment = gf_bs_read_int(m4v->bs, dsi->NumBitsTimeIncrement);
			}
			if (!dsi->has_shape) {
				gf_bs_read_int(m4v->bs, 1);
				dsi->width = gf_bs_read_int(m4v->bs, 13);
				gf_bs_read_int(m4v->bs, 1);
				dsi->height = gf_bs_read_int(m4v->bs, 13);
			} else {
				dsi->width = dsi->height = 0;
			}
			/*shape will be done later*/
			gf_bs_align(m4v->bs);
			break;

		case M4V_VOP_START_CODE:
		case M4V_GOV_START_CODE:
			go = 0;
			break;
		/*EOS*/
		case -1:
			go = 0;
			m4v->current_object_start = (u32) gf_bs_get_position(m4v->bs);
			break;
		/*don't interest us*/
		case M4V_UDTA_START_CODE:
		default:
			break;
		}
	}
	return GF_OK;
}

GF_EXPORT
GF_Err gf_m4v_parse_config(GF_M4VParser *m4v, GF_M4VDecSpecInfo *dsi)
{
	if (m4v->mpeg12) {
		return gf_m4v_parse_config_mpeg12(m4v, dsi);
	} else {
		return gf_m4v_parse_config_mpeg4(m4v, dsi);
	}
}

static GF_Err gf_m4v_parse_frame_mpeg12(GF_M4VParser *m4v, GF_M4VDecSpecInfo dsi, u8 *frame_type, u32 *time_inc, u32 *size, u32 *start, Bool *is_coded)
{
	u8 go, hasVOP, firstObj, val;
	s32 o_type;

	if (!m4v || !size || !start || !frame_type) return GF_BAD_PARAM;

	*size = 0;
	firstObj = 1;
	hasVOP = 0;
	*is_coded = 0;
	m4v->current_object_type = (u32) -1;
	*frame_type = 0;

	M4V_Reset(m4v, m4v->current_object_start);
	go = 1;
	
	while (go) {
		o_type = M4V_LoadObject(m4v);
		switch (o_type) {
		case M2V_PIC_START_CODE:
			/*done*/
			if (hasVOP) {
				go = 0;
				break;
			}
			if (firstObj) {
				*start = m4v->current_object_start;
				firstObj = 0;
			}
			hasVOP = 1;
			*is_coded = 1;

			val = gf_bs_read_u8(m4v->bs);
			val = gf_bs_read_u8(m4v->bs);
			*frame_type = ( (val >> 3) & 0x7 ) - 1;
			break;
		case M2V_GOP_START_CODE:
			if (firstObj) {
				*start = m4v->current_object_start;
				firstObj = 0;
			}
			if (hasVOP) go = 0;
			break;

		case M2V_SEQ_START_CODE:
			if (firstObj) {
				*start = m4v->current_object_start;
				firstObj = 0;
			}
			if (hasVOP) {
				go = 0;
				break;
			}

			/**/
			break;

		default:
			break;

		case -1:
			*size = (u32) gf_bs_get_position(m4v->bs) - *start;
			return GF_EOS;
		}
	}
	*size = m4v->current_object_start - *start;
	return GF_OK;
}

static GF_Err gf_m4v_parse_frame_mpeg4(GF_M4VParser *m4v, GF_M4VDecSpecInfo dsi, u8 *frame_type, u32 *time_inc, u32 *size, u32 *start, Bool *is_coded)
{
	u8 go, hasVOP, firstObj, secs;
	s32 o_type;
	u32 vop_inc = 0;

	if (!m4v || !size || !start || !frame_type) return GF_BAD_PARAM;

	*size = 0;
	firstObj = 1;
	hasVOP = 0;
	*is_coded = 0;
	m4v->current_object_type = (u32) -1;
	*frame_type = 0;

	M4V_Reset(m4v, m4v->current_object_start);
	go = 1;
	while (go) {
		o_type = M4V_LoadObject(m4v);
		switch (o_type) {
		case M4V_VOP_START_CODE:
			/*done*/
			if (hasVOP) {
				go = 0;
				break;
			}
			if (firstObj) {
				*start = m4v->current_object_start;
				firstObj = 0;
			}
			hasVOP = 1;

			/*coding type*/
			*frame_type = gf_bs_read_int(m4v->bs, 2);
			/*modulo time base*/
			secs = 0;
			while (gf_bs_read_int(m4v->bs, 1) != 0)
				secs ++;
			/*no support for B frames in parsing*/
			secs += (dsi.enh_layer || *frame_type!=2) ? m4v->tc_dec : m4v->tc_disp;
			/*marker*/
			gf_bs_read_int(m4v->bs, 1);
			/*vop_time_inc*/
			if (dsi.NumBitsTimeIncrement)
				vop_inc = gf_bs_read_int(m4v->bs, dsi.NumBitsTimeIncrement);
			
			m4v->prev_tc_dec = m4v->tc_dec;
			m4v->prev_tc_disp = m4v->tc_disp;
			if (dsi.enh_layer || *frame_type!=2) {
				m4v->tc_disp = m4v->tc_dec;
				m4v->tc_dec = secs;
			}
			*time_inc = secs * dsi.clock_rate + vop_inc;
			/*marker*/
			gf_bs_read_int(m4v->bs, 1);
			/*coded*/
			*is_coded = gf_bs_read_int(m4v->bs, 1);
			gf_bs_align(m4v->bs);
			break;
		case M4V_GOV_START_CODE:
			if (firstObj) {
				*start = m4v->current_object_start;
				firstObj = 0;
			}
			if (hasVOP) go = 0;
			break;

		case M4V_VOS_START_CODE:
		case M4V_VOL_START_CODE:
			if (hasVOP) {
				go = 0;
			} else if (firstObj) {
				*start = m4v->current_object_start;
				firstObj = 0;
			}
			break;

		case M4V_VO_START_CODE:
		default:
			break;

		case -1:
			*size = (u32) gf_bs_get_position(m4v->bs) - *start;
			return GF_EOS;
		}
	}
	*size = m4v->current_object_start - *start;
	return GF_OK;
}

GF_EXPORT
GF_Err gf_m4v_parse_frame(GF_M4VParser *m4v, GF_M4VDecSpecInfo dsi, u8 *frame_type, u32 *time_inc, u32 *size, u32 *start, Bool *is_coded)
{
	if (m4v->mpeg12) {
		return gf_m4v_parse_frame_mpeg12(m4v, dsi, frame_type, time_inc, size, start, is_coded);
	} else {
		return gf_m4v_parse_frame_mpeg4(m4v, dsi, frame_type, time_inc, size, start, is_coded);
	}
}

GF_Err gf_m4v_rewrite_par(char **o_data, u32 *o_dataLen, s32 par_n, s32 par_d)
{
	u32 start, end, size;
	GF_BitStream *mod;
	GF_M4VParser *m4v;
	Bool go = 1;

	m4v = gf_m4v_parser_new(*o_data, *o_dataLen, 0);
	mod = gf_bs_new(NULL, 0, GF_BITSTREAM_WRITE);

	end = start = 0;
	while (go) {
		u32 type = M4V_LoadObject(m4v);

		end = (u32) gf_bs_get_position(m4v->bs) - 4;
		size = end - start;
		/*store previous object*/
		if (size) {
			if (size) gf_bs_write_data(mod, *o_data + start, size);
			start = end;
		}

		switch (type) {
		case M4V_VOL_START_CODE:
			gf_bs_write_int(mod, 0, 8);
			gf_bs_write_int(mod, 0, 8);
			gf_bs_write_int(mod, 1, 8);
			gf_bs_write_int(mod, M4V_VOL_START_CODE, 8);
			gf_bs_write_int(mod, gf_bs_read_int(m4v->bs, 1), 1);
			gf_bs_write_int(mod, gf_bs_read_int(m4v->bs, 8), 8);
			start = gf_bs_read_int(m4v->bs, 1);
			gf_bs_write_int(mod, start, 1);
			if (start) {
				gf_bs_write_int(mod, gf_bs_read_int(m4v->bs, 7), 7);
			}
			start = gf_bs_read_int(m4v->bs, 4);
			if (start == 0xF) {
				gf_bs_read_int(m4v->bs, 8);
				gf_bs_read_int(m4v->bs, 8);
			}
			if ((par_n>=0) && (par_d>=0)) {
				u8 par = m4v_get_sar_idx(par_n, par_d);
				gf_bs_write_int(mod, par, 4);
				if (par==0xF) {
					gf_bs_write_int(mod, par_n, 8);
					gf_bs_write_int(mod, par_d, 8);
				}
			} else {
				gf_bs_write_int(mod, 0x0, 4);
			}
		case -1:
			go = 0;
			break;
		default:
			break;
		}
	}
	while (gf_bs_bits_available(m4v->bs)) {
		u32 b = gf_bs_read_int(m4v->bs, 1);
		gf_bs_write_int(mod, b, 1);
	}
	
	gf_m4v_parser_del(m4v);
	free(*o_data);
	gf_bs_get_content(mod, o_data, o_dataLen);
	gf_bs_del(mod);
	return GF_OK;
}

GF_EXPORT
u32 gf_m4v_get_object_start(GF_M4VParser *m4v)
{
	return m4v->current_object_start;
}

GF_EXPORT
Bool gf_m4v_is_valid_object_type(GF_M4VParser *m4v)
{
	return ((s32) m4v->current_object_type==-1) ? 0 : 1;
}


GF_EXPORT
GF_Err gf_m4v_get_config(char *rawdsi, u32 rawdsi_size, GF_M4VDecSpecInfo *dsi)
{
	GF_Err e;
	GF_M4VParser *vparse;
	if (!rawdsi || !rawdsi_size) return GF_NON_COMPLIANT_BITSTREAM;
	vparse = gf_m4v_parser_new(rawdsi, rawdsi_size, 0);
	e = gf_m4v_parse_config(vparse, dsi);
	gf_m4v_parser_del(vparse);
	return e;
}

/*
	AAC parser
*/

GF_EXPORT
const char *gf_m4a_object_type_name(u32 objectType)
{
	switch (objectType) {
	case 0: return "Reserved";
	case 1: return "AAC Main";
	case 2: return "AAC LC";
	case 3: return "AAC SSR";
	case 4: return "AAC LTP";
	case 5: return "SBR";
	case 6: return "AAC Scalable";
	case 7: return "TwinVQ";
	case 8: return "CELP";
	case 9: return "HVXC";
	case 10: return "Reserved";
	case 11: return "Reserved";
	case 12: return "TTSI";
	case 13: return "Main synthetic";
	case 14: return "Wavetable synthesis";
	case 15: return "General MIDI";
	case 16: return "Algorithmic Synthesis and Audio FX";
	case 17: return "ER AAC LC";
	case 18: return "Reserved";
	case 19: return "ER AAC LTP";
	case 20: return "ER AAC scalable";
	case 21: return "ER TwinVQ";
	case 22: return "ER BSAC";
	case 23: return "ER AAC LD";
	case 24: return "ER CELP";
	case 25: return "ER HVXC";
	case 26: return "ER HILN";
	case 27: return "ER Parametric";
	case 28: return "SSC";
	case 29: return "ParametricStereo";
	case 30: return "(Reserved)";
	case 31: return "(Reserved)";
	case 32: return "Layer-1";
	case 33: return "Layer-2";
	case 34: return "Layer-3";
	case 35: return "DST";
	case 36: return "ALS";
	default: return "Unknown";
	}
}

GF_EXPORT
const char *gf_m4a_get_profile_name(u8 audio_pl)
{
	switch (audio_pl) {
	case 0x00: return "ISO Reserved (0x00)";
	case 0x01: return "Main Audio Profile @ Level 1";
	case 0x02: return "Main Audio Profile @ Level 2";
	case 0x03: return "Main Audio Profile @ Level 3";
	case 0x04: return "Main Audio Profile @ Level 4";
	case 0x05: return "Scalable Audio Profile @ Level 1";
	case 0x06: return "Scalable Audio Profile @ Level 2";
	case 0x07: return "Scalable Audio Profile @ Level 3";
	case 0x08: return "Scalable Audio Profile @ Level 4";
	case 0x09: return "Speech Audio Profile @ Level 1";
	case 0x0A: return "Speech Audio Profile @ Level 2";
	case 0x0B: return "Synthetic Audio Profile @ Level 1";
	case 0x0C: return "Synthetic Audio Profile @ Level 2";
	case 0x0D: return "Synthetic Audio Profile @ Level 3";
	case 0x0E: return "High Quality Audio Profile @ Level 1";
	case 0x0F: return "High Quality Audio Profile @ Level 2";
	case 0x10: return "High Quality Audio Profile @ Level 3";
	case 0x11: return "High Quality Audio Profile @ Level 4";
	case 0x12: return "High Quality Audio Profile @ Level 5";
	case 0x13: return "High Quality Audio Profile @ Level 6";
	case 0x14: return "High Quality Audio Profile @ Level 7";
	case 0x15: return "High Quality Audio Profile @ Level 8";
	case 0x16: return "Low Delay Audio Profile @ Level 1";
	case 0x17: return "Low Delay Audio Profile @ Level 2";
	case 0x18: return "Low Delay Audio Profile @ Level 3";
	case 0x19: return "Low Delay Audio Profile @ Level 4";
	case 0x1A: return "Low Delay Audio Profile @ Level 5";
	case 0x1B: return "Low Delay Audio Profile @ Level 6";
	case 0x1C: return "Low Delay Audio Profile @ Level 7";
	case 0x1D: return "Low Delay Audio Profile @ Level 8";
	case 0x1E: return "Natural Audio Profile @ Level 1";
	case 0x1F: return "Natural Audio Profile @ Level 2";
	case 0x20: return "Natural Audio Profile @ Level 3";
	case 0x21: return "Natural Audio Profile @ Level 4";
	case 0x22: return "Mobile Audio Internetworking Profile @ Level 1";
	case 0x23: return "Mobile Audio Internetworking Profile @ Level 2";
	case 0x24: return "Mobile Audio Internetworking Profile @ Level 3";
	case 0x25: return "Mobile Audio Internetworking Profile @ Level 4";
	case 0x26: return "Mobile Audio Internetworking Profile @ Level 5";
	case 0x27: return "Mobile Audio Internetworking Profile @ Level 6";
	case 0x28: return "AAC Profile @ Level 1";
	case 0x29: return "AAC Profile @ Level 2";
	case 0x2A: return "AAC Profile @ Level 4";
	case 0x2B: return "AAC Profile @ Level 5";
	case 0x2C: return "High Efficiency AAC Profile @ Level 2";
	case 0x2D: return "High Efficiency AAC Profile @ Level 3";
	case 0x2E: return "High Efficiency AAC Profile @ Level 4";
	case 0x2F: return "High Efficiency AAC Profile @ Level 5";
	case 0xFE: return "Not part of MPEG-4 audio profiles";
	case 0xFF: return "No audio capability required";
	default: return "ISO Reserved / User Private";
	}
}

u32 gf_m4a_get_profile(GF_M4ADecSpecInfo *cfg)
{
	switch (cfg->base_object_type) {
	case 2: /*AAC LC*/
		if (cfg->nb_chan<=2) return (cfg->base_sr<=24000) ? 0x28 : 0x29; /*LC@L1 or LC@L2*/
		return (cfg->base_sr<=48000) ? 0x2A : 0x2B; /*LC@L4 or LC@L5*/
	case 5: /*HE-AAC*/
		if (cfg->nb_chan<=2) return (cfg->base_sr<=24000) ? 0x2C : 0x2D; /*HE@L2 or HE@L3*/
		return (cfg->base_sr<=48000) ? 0x2E : 0x2F; /*HE@L4 or HE@L5*/
	/*default to HQ*/
	default:
		if (cfg->nb_chan<=2) return (cfg->base_sr<24000) ? 0x0E : 0x0F; /*HQ@L1 or HQ@L2*/
		return 0x10; /*HQ@L3*/
	}
}



GF_EXPORT
GF_Err gf_m4a_parse_config(GF_BitStream *bs, GF_M4ADecSpecInfo *cfg, Bool size_known)
{
	memset(cfg, 0, sizeof(GF_M4ADecSpecInfo));
	cfg->base_object_type = gf_bs_read_int(bs, 5);
	/*extended object type*/
	if (cfg->base_object_type==31) {
		cfg->base_object_type = 32 + gf_bs_read_int(bs, 6);
	}
	cfg->base_sr_index = gf_bs_read_int(bs, 4);
	if (cfg->base_sr_index == 0x0F) {
		cfg->base_sr = gf_bs_read_int(bs, 24);
	} else {
		cfg->base_sr = GF_M4ASampleRates[cfg->base_sr_index];
	}
	cfg->nb_chan = gf_bs_read_int(bs, 4);
	/*this is 7+1 channels*/
	if (cfg->nb_chan==7) cfg->nb_chan=8;

	if (cfg->base_object_type==5) {
		cfg->has_sbr = 1;
		cfg->sbr_sr_index = gf_bs_read_int(bs, 4);
		if (cfg->sbr_sr_index == 0x0F) {
			cfg->sbr_sr = gf_bs_read_int(bs, 24);
		} else {
			cfg->sbr_sr = GF_M4ASampleRates[cfg->sbr_sr_index];
		}
		cfg->sbr_object_type = gf_bs_read_int(bs, 5);
	}

	/*object cfg*/
	switch (cfg->base_object_type) {
	case 1:
	case 2:
	case 3:
	case 4:
	case 6:
	case 7:
	case 17:
	case 19:
	case 20:
	case 21:
	case 22:
	case 23:
	{
		Bool fl_flag, ext_flag;
		u32 delay;
		/*frame length flag*/
		fl_flag = gf_bs_read_int(bs, 1);
		/*depends on core coder*/
		delay = 0;
		if (gf_bs_read_int(bs, 1))
			delay = gf_bs_read_int(bs, 14);
		ext_flag = gf_bs_read_int(bs, 1);
		if (!cfg->nb_chan) {
		}
		if ((cfg->base_object_type == 6) || (cfg->base_object_type == 20)) {
			gf_bs_read_int(bs, 3);
		}
		if (ext_flag) {
			if (cfg->base_object_type == 22) {
				gf_bs_read_int(bs, 5);
				gf_bs_read_int(bs, 11);
			}
			if ((cfg->base_object_type == 17) 
				|| (cfg->base_object_type == 19) 
				|| (cfg->base_object_type == 20) 
				|| (cfg->base_object_type == 23) 
			) {
				gf_bs_read_int(bs, 1);
				gf_bs_read_int(bs, 1);
				gf_bs_read_int(bs, 1);
			}
			ext_flag = gf_bs_read_int(bs, 1);
		}
	}
		break;
	}
	/*ER cfg*/
	switch (cfg->base_object_type) {
	case 17:
	case 19:
	case 20:
	case 21:
	case 22:
	case 23:
	case 24:
	case 25:
	case 26:
	case 27:
	{
		u32 epConfig = gf_bs_read_int(bs, 2);
		if ((epConfig == 2) || (epConfig == 3) ) {
		}
		if (epConfig == 3) {
			gf_bs_read_int(bs, 1);
		}
	}
		break;
	}

	if (size_known && (cfg->base_object_type != 5) && gf_bs_available(bs)>=2) {
		u32 sync = gf_bs_peek_bits(bs, 11, 0);
		if (sync==0x2b7) {
			gf_bs_read_int(bs, 11);
			cfg->sbr_object_type = gf_bs_read_int(bs, 5);
			cfg->has_sbr = gf_bs_read_int(bs, 1);
			if (cfg->has_sbr) {
				cfg->sbr_sr_index = gf_bs_read_int(bs, 4);
				if (cfg->sbr_sr_index == 0x0F) {
					cfg->sbr_sr = gf_bs_read_int(bs, 24);
				} else {
					cfg->sbr_sr = GF_M4ASampleRates[cfg->sbr_sr_index];
				}
			}
		}
	}
	cfg->audioPL = gf_m4a_get_profile(cfg);
	return GF_OK;
}

GF_EXPORT
GF_Err gf_m4a_get_config(char *dsi, u32 dsi_size, GF_M4ADecSpecInfo *cfg)
{
	GF_BitStream *bs;
	if (!dsi || !dsi_size || (dsi_size<2) ) return GF_NON_COMPLIANT_BITSTREAM;
	bs = gf_bs_new(dsi, dsi_size, GF_BITSTREAM_READ);
	gf_m4a_parse_config(bs, cfg, 1);
	gf_bs_del(bs);
	return GF_OK;
}


GF_EXPORT
GF_Err gf_m4a_write_config(GF_M4ADecSpecInfo *cfg, char **dsi, u32 *dsi_size)
{
	GF_BitStream *bs = gf_bs_new(NULL, 0, GF_BITSTREAM_WRITE);

	/*extended object type*/
	if (cfg->base_object_type>=32) {
		gf_bs_write_int(bs, 31, 5);
		gf_bs_write_int(bs, cfg->base_object_type-32, 6);
	} else {
		gf_bs_write_int(bs, cfg->base_object_type, 5);
	}
	gf_bs_write_int(bs, cfg->base_sr_index, 4);
	if (cfg->base_sr_index == 0x0F) {
		gf_bs_write_int(bs, cfg->base_sr, 24);
	}
	if (cfg->nb_chan == 8) {
		gf_bs_write_int(bs, 7, 4);
	} else {
		gf_bs_write_int(bs, cfg->nb_chan, 4);
	}

	if (cfg->base_object_type==5) {
		cfg->has_sbr = 1;
		gf_bs_write_int(bs, cfg->sbr_sr_index, 4);
		if (cfg->sbr_sr_index == 0x0F) {
			gf_bs_write_int(bs, cfg->sbr_sr, 24);
		}
		gf_bs_write_int(bs, cfg->sbr_object_type, 5);
	}

	/*object cfg*/
	switch (cfg->base_object_type) {
	case 1:
	case 2:
	case 3:
	case 4:
	case 6:
	case 7:
	case 17:
	case 19:
	case 20:
	case 21:
	case 22:
	case 23:
	{
		/*frame length flag*/
		gf_bs_write_int(bs, 0, 1);
		/*depends on core coder*/
		gf_bs_write_int(bs, 0, 1);
		/*ext flag*/
		gf_bs_write_int(bs, 0, 1);
		if ((cfg->base_object_type == 6) || (cfg->base_object_type == 20)) {
			gf_bs_write_int(bs, 0, 3);
		}
	}
		break;
	}
	/*ER cfg - not supported*/

	/*implicit sbr - not used yet*/
	if (0 && (cfg->base_object_type != 5) ) {
		gf_bs_write_int(bs, 0x2b7, 11);
		cfg->sbr_object_type = gf_bs_read_int(bs, 5);
		cfg->has_sbr = gf_bs_read_int(bs, 1);
		if (cfg->has_sbr) {
			cfg->sbr_sr_index = gf_bs_read_int(bs, 4);
			if (cfg->sbr_sr_index == 0x0F) {
				cfg->sbr_sr = gf_bs_read_int(bs, 24);
			} else {
				cfg->sbr_sr = GF_M4ASampleRates[cfg->sbr_sr_index];
			}
		}
	}
	gf_bs_get_content(bs, dsi, dsi_size);
	gf_bs_del(bs);
	return GF_OK;
}


/*		
	MP3 parser
	credits go to CISCO - MPEG4IP
*/
GF_EXPORT
u8 gf_mp3_version(u32 hdr)
{
	return ((hdr >> 19) & 0x3); 
}

static u8 MP3_GetLayerV(u32 hdr)
{
	return ((hdr >> 17) & 0x3); 
}

GF_EXPORT
u8 gf_mp3_layer(u32 hdr)
{
	return 4 - (((hdr >> 17) & 0x3));
}

GF_EXPORT
u8 gf_mp3_num_channels(u32 hdr)
{
	if (((hdr >> 6) & 0x3) == 3) return 1;
	return 2;
}


static GFINLINE u32 get_MP3SamplingRates(u32 version, u32 idx) 
{
	switch (version) {
	case 0:
		switch (idx) {
		case 0: return 11025;
		case 1: return 12000;
		case 2: return 8000;
		default: return 0;
		}
		break;
	case 1:
		return 0;
	case 2:
		switch (idx) {
		case 0: return 22050;
		case 1: return 24000;
		case 2: return 16000;
		default: return 0;
		}
		break;
	case 3:
		switch (idx) {
		case 0: return 44100;
		case 1: return 48000;
		case 2: return 32000;
		default: return 0;
		}
		break;
	}
	return 0;
}

GF_EXPORT
u16 gf_mp3_sampling_rate(u32 hdr)
{
	/* extract the necessary fields from the MP3 header */
	u8 version = gf_mp3_version(hdr);
	u8 sampleRateIndex = (hdr >> 10) & 0x3;
	return get_MP3SamplingRates(version, sampleRateIndex);
}

GF_EXPORT
u16 gf_mp3_window_size(u32 hdr)
{
	u8 version = gf_mp3_version(hdr);
	u8 layer = MP3_GetLayerV(hdr);

	if (layer == 1) {
		if (version == 3) return 1152;
		return 576;
	}
	if (layer == 2) return 1152;
	return 384;
}

GF_EXPORT
u8 gf_mp3_object_type_indication(u32 hdr)
{
	switch (gf_mp3_version(hdr)) {
	case 3:
		return 0x6B;
		break;
	case 2:
	case 0:
		return 0x69;
	default:
		return 0x00;
	}
}

GF_EXPORT
const char *gf_mp3_version_name(u32 hdr)
{
	u32 v = gf_mp3_version(hdr);
	switch (v) {
	case 0: return "MPEG-2.5";
	case 1: return "Reserved";
	case 2: return "MPEG-2";
	case 3: return "MPEG-1";
	default: return "Unknown";
	}
}

static GFINLINE u32 get_MP3BitRates(u32 idx1, u32 idx2) 
{
	switch (idx1) {
	/* MPEG-1, Layer III */
	case 0:
		switch (idx2) {
		case 0: return 32;
		case 1: return 40;
		case 2: return 48;
		case 3: return 56;
		case 4: return 64;
		case 5: return 80;
		case 6: return 96;
		case 7: return 112;
		case 8: return 128;
		case 9: return 160;
		case 10: return 192;
		case 11: return 224;
		case 12: return 256;
		case 13: return 320;
		default: return 0;
		}
		break;
	/* MPEG-1, Layer II */
	case 1:
		switch (idx2) {
		case 0: return 32;
		case 1: return 48;
		case 2: return 56;
		case 3: return 64;
		case 4: return 80;
		case 5: return 96;
		case 6: return 112;
		case 7: return 128;
		case 8: return 160;
		case 9: return 192;
		case 10: return 224;
		case 11: return 256;
		case 12: return 320;
		case 13: return 384;
		default: return 0;
		}
		break;
	/* MPEG-1, Layer I */
	case 2:
		switch (idx2) {
		case 0: return 32;
		case 1: return 64;
		case 2: return 96;
		case 3: return 128;
		case 4: return 160;
		case 5: return 192;
		case 6: return 224;
		case 7: return 256;
		case 8: return 288;
		case 9: return 320;
		case 10: return 352;
		case 11: return 384;
		case 12: return 416;
		case 13: return 448;
		default: return 0;
		}
		break;
	/* MPEG-2 or 2.5, Layer II or III */
	case 3:
		switch (idx2) {
		case 0: return 8;
		case 1: return 16;
		case 2: return 24;
		case 3: return 32;
		case 4: return 40;
		case 5: return 48;
		case 6: return 56;
		case 7: return 64;
		case 8: return 80;
		case 9: return 96;
		case 10: return 112;
		case 11: return 128;
		case 12: return 144;
		case 13: return 160;
		default: return 0;
		}
		break;
	/* MPEG-2 or 2.5, Layer I */
	case 4:
		switch (idx2) {
		case 0: return 32;
		case 1: return 48;
		case 2: return 56;
		case 3: return 64;
		case 4: return 80;
		case 5: return 96;
		case 6: return 112;
		case 7: return 128;
		case 8: return 144;
		case 9: return 160;
		case 10: return 176;
		case 11: return 192;
		case 12: return 224;
		case 13: return 256;
		default: return 0;
		}
		break;
	default:
		return 0;
	}
}


GF_EXPORT
u16 gf_mp3_frame_size(u32 hdr)
{
	u32 val;
	u8 bitRateIndex1;
	u8 version = gf_mp3_version(hdr);
	u8 layer = MP3_GetLayerV(hdr);
	u8 bitRateIndex2 = (hdr >> 12) & 0xF;
	u8 sampleRateIndex = (hdr >> 10) & 0x3;
	Bool isPadded = (hdr >> 9) & 0x1;
	u32 frameSize = 0;

	if (version == 3) {
		bitRateIndex1 = layer - 1;
	} else {
		if (layer == 3) {
			bitRateIndex1 = 4;
		} else {
			bitRateIndex1 = 3;
		}
	}

	/* compute frame size */
	val = get_MP3SamplingRates(version, sampleRateIndex);
	if (!(version & 1)) val <<= 1;
	if (!val) return 0;
	frameSize = 144 * 1000 * get_MP3BitRates(bitRateIndex1, bitRateIndex2-1);
	frameSize /= val;

	if (isPadded) {
		if (layer == 3) {
			frameSize += 4;
		} else {
			frameSize++;
		}
	}
	return (u16) frameSize;
}


u16 gf_mp3_bit_rate(u32 hdr)
{
	u8 bitRateIndex1;
	u8 version = gf_mp3_version(hdr);
	u8 layer = MP3_GetLayerV(hdr);
	u8 bitRateIndex2 = (hdr >> 12) & 0xF;
	if (version == 3) {
		bitRateIndex1 = layer - 1;
	} else {
		if (layer == 3) {
			bitRateIndex1 = 4;
		} else {
			bitRateIndex1 = 3;
		}
	}

	/* compute frame size */
	return get_MP3BitRates(bitRateIndex1, bitRateIndex2-1);
}


GF_EXPORT
u32 gf_mp3_get_next_header(FILE* in)
{
	u8 b, state = 0;
	u32 dropped = 0;
	unsigned char bytes[4];
	bytes[0] = bytes[1] = bytes[2] = bytes[3] = 0;

	while (1) {
		if (fread(&b, 1, 1, in) == 0) return 0;

		if (state==3) {
			bytes[state] = b;
			return GF_4CC(bytes[0], bytes[1], bytes[2], bytes[3]);
		}
		if (state==2) {
			if (((b & 0xF0) == 0) || ((b & 0xF0) == 0xF0) || ((b & 0x0C) == 0x0C)) {
				if (bytes[1] == 0xFF) state = 1; 
				else state = 0; 
			} else {
				bytes[state] = b;
				state = 3;
			}
		}
		if (state==1) {
			if (((b & 0xE0) == 0xE0) && ((b & 0x18) != 0x08) && ((b & 0x06) != 0)) {
				bytes[state] = b;
				state = 2;
			} else {
				state = 0;
			}
		}

		if (state==0) {
			if (b == 0xFF) {
				bytes[state] = b;
				state = 1;
			} else {
				if ((dropped == 0) && ((b & 0xE0) == 0xE0) && ((b & 0x18) != 0x08) && ((b & 0x06) != 0)) {
					bytes[0] = (u8) 0xFF;
					bytes[1] = b;
					state = 2;
				} else {
					dropped++;
				}
			}
		}
	}
	return 0;
}

GF_EXPORT
u32 gf_mp3_get_next_header_mem(char *buffer, u32 size, u32 *pos)
{
	u32 cur;
	u8 b, state = 0;
	u32 dropped = 0;
	unsigned char bytes[4];
	bytes[0] = bytes[1] = bytes[2] = bytes[3] = 0;

	cur = 0;
	*pos = 0;
	while (cur<size) {
		b = (u8) buffer[cur];
		cur++;

		if (state==3) {
			u32 val;
			bytes[state] = b; 
			val = GF_4CC(bytes[0], bytes[1], bytes[2], bytes[3]);
			if (gf_mp3_frame_size(val)) {
				*pos = dropped;
				return val;
			}
			state = 0;
			dropped = cur;
		}
		if (state==2) {
			if (((b & 0xF0) == 0) || ((b & 0xF0) == 0xF0) || ((b & 0x0C) == 0x0C)) {
				if (bytes[1] == 0xFF) {
					state = 1; 
					dropped+=1;
				} else {
					state = 0;
					dropped = cur;
				}
			} else {
				bytes[state] = b;
				state = 3;
			}
		}
		if (state==1) {
			if (((b & 0xE0) == 0xE0) && ((b & 0x18) != 0x08) && ((b & 0x06) != 0)) {
				bytes[state] = b;
				state = 2;
			} else {
				state = 0;
				dropped = cur;
			}
		}

		if (state==0) {
			if (b == 0xFF) {
				bytes[state] = b;
				state = 1;
			} else {
				dropped++;
			}
		}
	}
	return 0;
}

#ifndef GPAC_READ_ONLY

/*
	MPEG-4 AVC/H264 parser

  Inspired from ffmpeg (LGPL) and MPEG4IP (MPL)

*/

static u8 avc_golomb_bits[256] = {
	8, 7, 6, 6, 5, 5, 5, 5, 4, 4, 4, 4, 4, 4, 4, 4, 3, 
	3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 2, 2, 
	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 
	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 1, 1, 1, 1, 
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 
	1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
	0
};

static u32 avc_get_ue(GF_BitStream *bs)
{
	u8 coded;
	u32 bits, read;
	bits = 0;
	while (1) {
		read = gf_bs_peek_bits(bs, 8, 0);
		if (read) break;
		gf_bs_read_int(bs, 8);
		bits += 8;
	}
	coded = avc_golomb_bits[read];
	gf_bs_read_int(bs, coded);
	bits += coded;
	return gf_bs_read_int(bs, bits + 1) - 1;
}

static s32 avc_get_se(GF_BitStream *bs) 
{
	u32 v = avc_get_ue(bs);
	if ((v & 0x1) == 0) return (s32) (0 - (v>>1));
	return (v + 1) >> 1;
}

u32 AVC_IsStartCode(GF_BitStream *bs)
{
	u8 s1, s2, s3, s4;
	Bool is_sc = 0;
	u64 pos = gf_bs_get_position(bs);
	s1 = gf_bs_read_int(bs, 8);
	s2 = gf_bs_read_int(bs, 8);
	if (!s1 && !s2) {
		s3 = gf_bs_read_int(bs, 8);
		if (s3==0x01) is_sc = 3;
		else if (!s3) {
			s4 = gf_bs_read_int(bs, 8);
			if (s4==0x01) is_sc = 4;
		}
	}
	gf_bs_seek(bs, pos+is_sc);
	return is_sc;
}

/*read that amount of data at each IO access rather than fetching byte by byte...*/
#define AVC_CACHE_SIZE	4096
u32 AVC_NextStartCode(GF_BitStream *bs)
{
	u32 v, bpos;
	char avc_cache[AVC_CACHE_SIZE];
	u64 end, cache_start, load_size;
	u64 start = gf_bs_get_position(bs);
	if (start<3) return 0;
	
	load_size = 0;
	bpos = 0;
	cache_start = 0;
	end = 0;
	v = 0xffffffff;
	while (!end) {
		/*refill cache*/
		if (bpos == (u32) load_size) {
			if (!gf_bs_available(bs)) break;
			load_size = gf_bs_available(bs);
			if (load_size>AVC_CACHE_SIZE) load_size=AVC_CACHE_SIZE;
			bpos = 0;
			cache_start = gf_bs_get_position(bs);
			gf_bs_read_data(bs, avc_cache, (u32) load_size);
		}
		v = ( (v<<8) & 0xFFFFFF00) | ((u32) avc_cache[bpos]);
		bpos++;
		if (v == 0x00000001) end = cache_start+bpos-4;
		else if ( (v & 0x00FFFFFF) == 0x00000001) end = cache_start+bpos-3;
	}
	gf_bs_seek(bs, start);
	if (!end) end = gf_bs_get_size(bs);
	return (u32) (end-start);
}


Bool AVC_NALUIsSlice(u8 type)
{
	return ((type >= GF_AVC_NALU_NON_IDR_SLICE) && (type <= GF_AVC_NALU_IDR_SLICE)) ? 1 : 0;
}

Bool AVC_SliceIsIDR(AVCState *avc) 
{
  if (avc->sei.recovery_point.valid)
  {
	  avc->sei.recovery_point.valid = 0;
	  return 1;
  }
  if (avc->s_info.nal_unit_type != GF_AVC_NALU_IDR_SLICE) return 0;
  switch (avc->s_info.slice_type) {
  case GF_AVC_TYPE_I:
  case GF_AVC_TYPE2_I:
  case GF_AVC_TYPE_SI:
  case GF_AVC_TYPE2_SI:
	  return 1;
  default:
	  return 0;
  }
}

static const struct { u32 w, h; } avc_sar[14] =
{
    { 0,   0 }, { 1,   1 }, { 12, 11 }, { 10, 11 },
    { 16, 11 }, { 40, 33 }, { 24, 11 }, { 20, 11 },
    { 32, 11 }, { 80, 33 }, { 18, 11 }, { 15, 11 },
    { 64, 33 }, { 160,99 },
};

s32 AVC_ReadSeqInfo(GF_BitStream *bs, AVCState *avc, u32 *vui_flag_pos)
{
	AVC_SPS *sps;
	s32 mb_width, mb_height;
	u32 sps_id, profile_idc, level_idc, pcomp, i, chroma_format_idc, cl, cr, ct, cb;
	
	if (vui_flag_pos) *vui_flag_pos = 0;

	profile_idc = gf_bs_read_int(bs, 8);
	pcomp = gf_bs_read_int(bs, 8);
	level_idc = gf_bs_read_int(bs, 8);
    sps_id = avc_get_ue(bs);

	sps = &avc->sps[sps_id];
	if (!sps->status) sps->status = 1;

	/*High Profile cfg stuff*/	
	switch (profile_idc) {
	case 100:
	case 110:
	case 122:
	case 144:
		chroma_format_idc = avc_get_ue(bs);
		if (chroma_format_idc == 3) /*residual_colour_transform_flag = */ gf_bs_read_int(bs, 1);
		/*bit_depth_luma_minus8 = */ avc_get_ue(bs);
		/*bit_depth_chroma_minus8 = */ avc_get_ue(bs);
		/*qpprime_y_zero_transform_bypass_flag = */ gf_bs_read_int(bs, 1);
		/*seq_scaling_matrix_present_flag*/
		if (gf_bs_read_int(bs, 1)) {
			u32 k;
			for (k=0; k<8; k++) {
				if (gf_bs_read_int(bs, 1)) {
					u32 z, last = 8, next = 8;
					u32 sl = k<6 ? 16 : 64;
					for (z=0; z<sl; z++) {
						if (next) {
							s32 delta = avc_get_se(bs);
							next = (last + delta + 256) % 256;
						}
						last = next ? next : last;
					}
				}
			}
		}
		break;
	}	
	
	sps->profile_idc = profile_idc;
	sps->level_idc = level_idc;
	sps->prof_compat = pcomp;
    sps->log2_max_frame_num = avc_get_ue(bs) + 4;
    sps->poc_type = avc_get_ue(bs);

    if (sps->poc_type == 0) {
        sps->log2_max_poc_lsb = avc_get_ue(bs) + 4;
    } else if(sps->poc_type == 1) {
        sps->delta_pic_order_always_zero_flag = gf_bs_read_int(bs, 1);
        sps->offset_for_non_ref_pic = avc_get_se(bs);
        sps->offset_for_top_to_bottom_field = avc_get_se(bs);
        sps->poc_cycle_length = avc_get_ue(bs);
        for(i=0; i<sps->poc_cycle_length; i++) sps->offset_for_ref_frame[i] = avc_get_se(bs);
    }
    if (sps->poc_type > 2) return -1;
    avc_get_ue(bs); /*ref_frame_count*/
    gf_bs_read_int(bs, 1); /*gaps_in_frame_num_allowed_flag*/
    mb_width = avc_get_ue(bs) + 1;
    mb_height= avc_get_ue(bs) + 1;

    sps->frame_mbs_only_flag = gf_bs_read_int(bs, 1);

    sps->width = mb_width * 16;
    sps->height = (2-sps->frame_mbs_only_flag) * mb_height * 16;

	/*mb_adaptive_frame_field_flag*/
	if (!sps->frame_mbs_only_flag) gf_bs_read_int(bs, 1);
    gf_bs_read_int(bs, 1); /*direct_8x8_inference_flag*/
	cl = cr = ct = cb = 0;
    if (gf_bs_read_int(bs, 1)) /*crop*/ {
        cl = avc_get_ue(bs); /*crop_left*/
        cr = avc_get_ue(bs); /*crop_right*/
        ct = avc_get_ue(bs); /*crop_top*/
        cb = avc_get_ue(bs); /*crop_bottom*/

		sps->width = 16*mb_width - 2*(cl + cr);
		sps->height -= (2-sps->frame_mbs_only_flag)*2*(ct + cb);
    }

	if (vui_flag_pos) {
		*vui_flag_pos = (u32) gf_bs_get_bit_offset(bs);
	}

    /*vui_parameters_present_flag*/
    if (gf_bs_read_int(bs, 1)) {
		/*aspect_ratio_info_present_flag*/
		if (gf_bs_read_int(bs, 1)) {
			s32 aspect_ratio_idc = gf_bs_read_int(bs, 8);
			if (aspect_ratio_idc == 255) {
				sps->par_num = gf_bs_read_int(bs, 16); /*AR num*/
				sps->par_den = gf_bs_read_int(bs, 16); /*AR den*/
			} else if (aspect_ratio_idc<14) {
				sps->par_num = avc_sar[aspect_ratio_idc].w;
				sps->par_den = avc_sar[aspect_ratio_idc].h;
			}
		}
		if(gf_bs_read_int(bs, 1))       /* overscan_info_present_flag */
			gf_bs_read_int(bs, 1);      /* overscan_appropriate_flag */

		if (gf_bs_read_int(bs, 1)){      /* video_signal_type_present_flag */
			gf_bs_read_int(bs, 3);    /* video_format */
			gf_bs_read_int(bs, 1);      /* video_full_range_flag */
			if (gf_bs_read_int(bs, 1)){  /* colour_description_present_flag */
				gf_bs_read_int(bs, 8); /* colour_primaries */
				gf_bs_read_int(bs, 8); /* transfer_characteristics */
				gf_bs_read_int(bs, 8); /* matrix_coefficients */
			}
		}

		if (gf_bs_read_int(bs, 1)) {      /* chroma_location_info_present_flag */
			avc_get_ue(bs);  /* chroma_sample_location_type_top_field */
			avc_get_ue(bs);  /* chroma_sample_location_type_bottom_field */
		}

		sps->timing_info_present_flag = gf_bs_read_int(bs, 1);
		if (sps->timing_info_present_flag) {
			sps->num_units_in_tick = gf_bs_read_int(bs, 32);
			sps->time_scale = gf_bs_read_int(bs, 32);
			sps->fixed_frame_rate_flag = gf_bs_read_int(bs, 1);
		}
	}
    return sps_id;
}

s32 AVC_ReadPictParamSet(GF_BitStream *bs, AVCState *avc)
{
    s32 pps_id = avc_get_ue(bs);
    AVC_PPS *pps= &avc->pps[pps_id];
   
	if (!pps->status) pps->status = 1;
    pps->sps_id= avc_get_ue(bs);
    /*pps->cabac = */gf_bs_read_int(bs, 1);
    /*pps->pic_order_present= */gf_bs_read_int(bs, 1);
    pps->slice_group_count= avc_get_ue(bs) + 1;
    if (pps->slice_group_count > 1 ) /*pps->mb_slice_group_map_type = */avc_get_ue(bs);
    /*pps->ref_count[0]= */avc_get_ue(bs) /*+ 1*/;
    /*pps->ref_count[1]= */avc_get_ue(bs) /*+ 1*/;
    /*
	if ((pps->ref_count[0] > 32) || (pps->ref_count[1] > 32)) return -1;
	*/
    
    /*pps->weighted_pred = */gf_bs_read_int(bs, 1);
    /*pps->weighted_bipred_idc = */gf_bs_read_int(bs, 2);
    /*pps->init_qp = */avc_get_se(bs) /*+ 26*/;
    /*pps->init_qs= */avc_get_se(bs) /*+ 26*/;
    /*pps->chroma_qp_index_offset = */avc_get_se(bs);
    /*pps->deblocking_filter_parameters_present = */gf_bs_read_int(bs, 1);
    /*pps->constrained_intra_pred = */gf_bs_read_int(bs, 1);
    pps->redundant_pic_cnt_present = gf_bs_read_int(bs, 1);
    return pps_id;
}

static s32 avc_parse_slice(GF_BitStream *bs, AVCState *avc, AVCSliceInfo *si) 
{
    s32 first_mb_in_slice, pps_id;

    /*s->current_picture.reference= h->nal_ref_idc != 0;*/
    first_mb_in_slice = avc_get_ue(bs);
    si->slice_type = avc_get_ue(bs);
    if (si->slice_type > 9) return -1;

    pps_id = avc_get_ue(bs);
    if (pps_id>255) return -1;
    si->pps = &avc->pps[pps_id];
    if (!si->pps->slice_group_count) return -2;
    si->sps = &avc->sps[si->pps->sps_id];
    if (!si->sps->log2_max_frame_num) return -2;

    si->frame_num = gf_bs_read_int(bs, si->sps->log2_max_frame_num);

	si->field_pic_flag = 0;
	si->bottom_field_flag = 0;
    if (si->sps->frame_mbs_only_flag) {
        /*s->picture_structure= PICT_FRAME;*/
    } else {
		si->field_pic_flag = gf_bs_read_int(bs, 1);
        if (si->field_pic_flag)
			si->bottom_field_flag = gf_bs_read_int(bs, 1);
    }
	if (si->nal_unit_type==GF_AVC_NALU_IDR_SLICE)
		si->idr_pic_id = avc_get_ue(bs);
   
    if (si->sps->poc_type==0) {
		si->poc_lsb = gf_bs_read_int(bs, si->sps->log2_max_poc_lsb);
		if (si->pps->pic_order_present && !si->field_pic_flag) {
			si->delta_poc_bottom = avc_get_se(bs);
		}
	} else if ((si->sps->poc_type==1) && !si->sps->delta_pic_order_always_zero_flag) {
        si->delta_poc[0] = avc_get_se(bs);
        if ((si->pps->pic_order_present==1) && !si->field_pic_flag)
            si->delta_poc[1] = avc_get_se(bs);
    }
    if (si->pps->redundant_pic_cnt_present) {
        si->redundant_pic_cnt = avc_get_ue(bs);
    }
	return 0;
}

static s32 avc_parse_recovery_point_sei(GF_BitStream *bs, AVCState *avc) 
{
	AVCSeiRecoveryPoint *rp = &avc->sei.recovery_point;

	rp->frame_cnt = avc_get_ue(bs);
	rp->exact_match_flag = gf_bs_read_int(bs, 1);
	rp->broken_link_flag = gf_bs_read_int(bs, 1);
	rp->changing_slice_group_idc = gf_bs_read_int(bs, 2);
	rp->valid = 1;

	return 0;
}


static void avc_compute_poc(AVCSliceInfo *si)
{
	enum {
		AVC_PIC_FRAME,
		AVC_PIC_FIELD_TOP,
		AVC_PIC_FIELD_BOTTOM,
	} pic_type;
	s32 field_poc[2] = {0,0};
	s32 max_frame_num = 1 << (si->sps->log2_max_frame_num);

	/* picture type */
	if (si->sps->frame_mbs_only_flag || !si->field_pic_flag) pic_type = AVC_PIC_FRAME;
	else if (si->bottom_field_flag) pic_type = AVC_PIC_FIELD_BOTTOM;
	else pic_type = AVC_PIC_FIELD_TOP;

	/* frame_num_offset */
	if (si->nal_unit_type == GF_AVC_NALU_IDR_SLICE) {
		si->poc_lsb_prev = 0;
		si->poc_msb_prev = 0;
		si->frame_num_offset = 0;
	} else {
		if (si->frame_num < si->frame_num_prev)
			si->frame_num_offset = si->frame_num_offset_prev + max_frame_num;
		else
			si->frame_num_offset = si->frame_num_offset_prev;
	}
	
	if (si->sps->poc_type==0) {
		const u32 max_poc_lsb = 1 << (si->sps->log2_max_poc_lsb);
		
		if ((si->poc_lsb < si->poc_lsb_prev) &&
		(si->poc_lsb_prev - si->poc_lsb >= max_poc_lsb / 2) )
			si->poc_msb = si->poc_msb_prev + max_poc_lsb;
		else if ((si->poc_lsb > si->poc_lsb_prev) &&
		(si->poc_lsb - si->poc_lsb_prev > max_poc_lsb / 2))
			si->poc_msb = si->poc_msb_prev - max_poc_lsb;
		else
			si->poc_msb = si->poc_msb_prev;

		field_poc[0] = field_poc[1] = si->poc_msb + si->poc_lsb;
		if (pic_type == AVC_PIC_FRAME) field_poc[1] += si->delta_poc_bottom;
	} else if (si->sps->poc_type==1) {
		u32 i;
		s32 abs_frame_num, expected_delta_per_poc_cycle, expected_poc;

		if (si->sps->poc_cycle_length)
			abs_frame_num = si->frame_num_offset + si->frame_num;
		else
			abs_frame_num = 0;

		if (!si->nal_ref_idc && (abs_frame_num > 0)) abs_frame_num--;

		expected_delta_per_poc_cycle = 0;
		for (i=0; i < si->sps->poc_cycle_length; i++)
			expected_delta_per_poc_cycle += si->sps->offset_for_ref_frame[i];

		if (abs_frame_num > 0) {
			const u32 poc_cycle_cnt = ( abs_frame_num - 1 ) / si->sps->poc_cycle_length;
			const u32 frame_num_in_poc_cycle = ( abs_frame_num - 1 ) % si->sps->poc_cycle_length;

			expected_poc = poc_cycle_cnt * expected_delta_per_poc_cycle;
			for (i = 0; i<=frame_num_in_poc_cycle; i++)
				expected_poc += si->sps->offset_for_ref_frame[i];
		} else {
			expected_poc = 0;
		}

		if (!si->nal_ref_idc) expected_poc += si->sps->offset_for_non_ref_pic;

		field_poc[0] = expected_poc + si->delta_poc[0];
		field_poc[1] = field_poc[0] + si->sps->offset_for_top_to_bottom_field;
		if (pic_type == AVC_PIC_FRAME) field_poc[1] += si->delta_poc[1];
	} else if (si->sps->poc_type== 2) {
		int poc;
		if (si->nal_unit_type == GF_AVC_NALU_IDR_SLICE) {
			poc = 0;
		} else {
			const int abs_frame_num = si->frame_num_offset + si->frame_num;
			poc = 2 * abs_frame_num;
			if (!si->nal_ref_idc) poc -= 1;
		}
		field_poc[0] = poc;
		field_poc[1] = poc;
	}
	if (pic_type == AVC_PIC_FRAME)
		si->poc = MIN(field_poc[0], field_poc[1] );
	else if (pic_type == AVC_PIC_FIELD_TOP)
		si->poc = field_poc[0];
	else
		si->poc = field_poc[1];
}

s32 AVC_ParseNALU(GF_BitStream *bs, u32 nal_hdr, AVCState *avc)
{
	u8 temp;
	s32 slice, ret;
	AVCSliceInfo n_state;

	slice = 0;
	memcpy(&n_state, &avc->s_info, sizeof(AVCSliceInfo));
	temp = n_state.nal_unit_type = nal_hdr & 0x1F;
	n_state.nal_ref_idc = (nal_hdr>>5) & 0x3;
	
	ret = 0;
	switch (temp) {
	case GF_AVC_NALU_ACCESS_UNIT:
	case GF_AVC_NALU_END_OF_SEQ:
	case GF_AVC_NALU_END_OF_STREAM:
		ret = 1;
		break;
	case GF_AVC_NALU_NON_IDR_SLICE:
	case GF_AVC_NALU_DP_A_SLICE:
	case GF_AVC_NALU_DP_B_SLICE:
	case GF_AVC_NALU_DP_C_SLICE:
	case GF_AVC_NALU_IDR_SLICE:
		slice = 1;
		/* slice buffer - read the info and compare.*/
		ret = avc_parse_slice(bs, avc, &n_state);
		if (ret<0) return ret;
		ret = 0;
		if ((avc->s_info.nal_unit_type > GF_AVC_NALU_IDR_SLICE)
			|| (avc->s_info.nal_unit_type < GF_AVC_NALU_NON_IDR_SLICE)) {
			break;
		}
		if (avc->s_info.frame_num != n_state.frame_num) { ret = 1; break; }

		if (avc->s_info.field_pic_flag != n_state.field_pic_flag) { ret = 1; break; }
		if ((avc->s_info.nal_ref_idc != n_state.nal_ref_idc) &&
			(!avc->s_info.nal_ref_idc || !n_state.nal_ref_idc)) {
			ret = 1;
			break;
		}
		assert(avc->s_info.sps);

		if (avc->s_info.sps->poc_type == n_state.sps->poc_type) {
			if (!avc->s_info.sps->poc_type) {
				if (!n_state.bottom_field_flag && (avc->s_info.poc_lsb != n_state.poc_lsb)) { 
					ret = 1; 
					break;
				}
				if (avc->s_info.delta_poc_bottom != n_state.delta_poc_bottom) {
					ret = 1;
					break;
				}
			} else if (avc->s_info.sps->poc_type==1) {
				if (avc->s_info.delta_poc[0] != n_state.delta_poc[0]) {
					ret =1;
					break;
				}
				if (avc->s_info.delta_poc[1] != n_state.delta_poc[1]) {
					ret = 1;
					break;
				}
			}
		}
		if ((avc->s_info.nal_unit_type == GF_AVC_NALU_IDR_SLICE) 
		&& (n_state.nal_unit_type == GF_AVC_NALU_IDR_SLICE)) {
			if (avc->s_info.idr_pic_id != n_state.idr_pic_id) {
				ret = 1;
				break;
			}
		}
		break;
	case GF_AVC_NALU_SEQ_PARAM:
	case GF_AVC_NALU_PIC_PARAM:
		return 0;
	default:
		if (avc->s_info.nal_unit_type <= GF_AVC_NALU_IDR_SLICE) ret = 1;
		else ret = 0;
		break;
	} 

	/* save _prev values */
	if (ret && avc->s_info.sps) {
		n_state.frame_num_offset_prev = avc->s_info.frame_num_offset;
		if ((avc->s_info.sps->poc_type != 2) || (avc->s_info.nal_ref_idc != 0))
			n_state.frame_num_prev = avc->s_info.frame_num;
		if (avc->s_info.nal_ref_idc) {
			n_state.poc_lsb_prev = avc->s_info.poc_lsb;
			n_state.poc_msb_prev = avc->s_info.poc_msb;
		}
	}
	if (slice) avc_compute_poc(&n_state);
	memcpy(&avc->s_info, &n_state, sizeof(AVCSliceInfo));
	return ret;
}

u32 AVC_ReformatSEI_NALU(char *buffer, u32 nal_size, AVCState *avc)
{
	u32 ptype, psize, hdr, written, start, var, num_zero, size_fix, i;
	char *new_buffer;
	GF_BitStream *bs;
	hdr = buffer[0];
	if ((hdr & 0x1F) != GF_AVC_NALU_SEI) return 0;
	
	bs = gf_bs_new(buffer, nal_size, GF_BITSTREAM_READ);
	gf_bs_read_int(bs, 8);

	new_buffer = (char*)malloc(sizeof(char)*nal_size);
	new_buffer[0] = (char) hdr;
	written = 1;

	/*parse SEI*/
	while (gf_bs_available(bs)) {
		Bool do_copy;
		ptype = 0;
		while (gf_bs_peek_bits(bs, 8, 0)==0xFF) {
			gf_bs_read_int(bs, 8);
			ptype += 255;
		}
		ptype += gf_bs_read_int(bs, 8);
		psize = 0;
		while (gf_bs_peek_bits(bs, 8, 0)==0xFF) {
			gf_bs_read_int(bs, 8);
			psize += 255;
		}
		psize += gf_bs_read_int(bs, 8);

		start = (u32) gf_bs_get_position(bs);
		do_copy = 1;
		switch (ptype) {
		/*remove SEI messages forbidden in MP4*/
		case 3: /*filler data*/
		case 10: /*sub_seq info*/
		case 11: /*sub_seq_layer char*/
		case 12: /*sub_seq char*/
			do_copy = 0;
			break;
		case 5: /*user unregistered */
		{
			u8 prev = buffer[start+2+psize];
			buffer[start+2+psize] = 0;
			GF_LOG(GF_LOG_DEBUG, GF_LOG_CODING, ("[avc-h264] SEI user message %s\n", buffer+start+2)); 
			buffer[start+2+psize] = prev;
		}
			break;
		
		case 6: /*recovery point*/
			{
				GF_BitStream *rp_bs = gf_bs_new(buffer + start, psize, GF_BITSTREAM_READ);
				avc_parse_recovery_point_sei(rp_bs, avc);
				gf_bs_del(rp_bs);
			}
			break;

		case 0: /*buffering period*/
		case 1: /*pic_timing*/
		case 2: /*pan scan rect*/
		case 4: /*user registered ITU t35*/

		case 7: /*def_rec_pic_marking_repetition*/
		case 8: /*spare_pic*/
		case 9: /*scene info*/
		case 13: /*full frame freeze*/
		case 14: /*full frame freeze release*/
		case 15: /*full frame snapshot*/
		case 16: /*progressive refinement segment start*/
		case 17: /*progressive refinement segment end*/
		case 18: /*motion constrained slice group*/
		default:/*reserved*/
			break;
		}

		/*fix payload size due to emulation prevention bytes*/
		size_fix = 0;
		num_zero = 0;
		if (psize%255 == 0) num_zero = 1;

		for (i = 0; i < psize + size_fix; i++)
		{
			if (!buffer[start + i])
				num_zero++;
			else
			{
				if ((num_zero == 2) && (buffer[start + i] == 3))
					size_fix++;
				num_zero = 0;
			}
		}

		if (do_copy) {
			var = ptype;
			while (var>=255) { new_buffer[written] = (char) 0xff; written++; var-=255;}
			new_buffer[written] = (char) var; written++;
			var = psize;
			while (var>=255) { new_buffer[written] = (char) 0xff; written++; var-=255;}
			new_buffer[written] = (char) var; written++;
			memcpy(new_buffer+written, buffer+start, sizeof(char)* (psize + size_fix));
			written += psize + size_fix;
		}

		gf_bs_skip_bytes(bs, (u64) (psize + size_fix));
		gf_bs_align(bs);
		if (gf_bs_available(bs)<2) {
			if (gf_bs_peek_bits(bs, 8, 0)==0x80) {
				new_buffer[written] = (char) 0x80;
				written += 1;
				break;
			}
		}
	}
	gf_bs_del(bs);
	assert(written<=nal_size);
	if (written) memcpy(buffer, new_buffer, sizeof(char)*written);
	free(new_buffer);
	/*if only hdr written ignore*/
	return (written>1) ? written : 0;
}

static u8 avc_get_sar_idx(u32 w, u32 h)
{
	u32 i;
	for (i=0; i<14; i++) {
		if ((avc_sar[i].w==w) && (avc_sar[i].h==h)) return i;
	}
	return 0xFF;
}

GF_Err AVC_ChangePAR(GF_AVCConfig *avcc, s32 ar_n, s32 ar_d)
{
	GF_BitStream *orig, *mod;
	AVCState avc;
	u32 i, bit_offset, flag;
	s32 idx;
	GF_AVCConfigSlot *slc;

	memset(&avc, 0, sizeof(AVCState));

	i=0;
	while ((slc = (GF_AVCConfigSlot *)gf_list_enum(avcc->sequenceParameterSets, &i))) {
		orig = gf_bs_new(slc->data, slc->size, GF_BITSTREAM_READ);
		/*skip NALU type*/
		gf_bs_read_int(orig, 8);
		idx = AVC_ReadSeqInfo(orig, &avc, &bit_offset);
		if (idx<0) {
			gf_bs_del(orig);
			continue;
		}
		mod = gf_bs_new(NULL, 0, GF_BITSTREAM_WRITE);
		gf_bs_seek(orig, 0);

		/*copy over till vui flag*/
		while (bit_offset) {
			flag = gf_bs_read_int(orig, 1);
			gf_bs_write_int(mod, flag, 1);
			bit_offset--;
		}
		/*check VUI*/
		flag = gf_bs_read_int(orig, 1);
		gf_bs_write_int(mod, 1, 1);
		if (flag) {
			/*aspect_ratio_info_present_flag*/
			if (gf_bs_read_int(orig, 1)) {
				s32 aspect_ratio_idc = gf_bs_read_int(orig, 8);
				if (aspect_ratio_idc == 255) {
					gf_bs_read_int(orig, 16); /*AR num*/
					gf_bs_read_int(orig, 16); /*AR den*/
				}
			}
		}
		if ((ar_d<0) || (ar_n<0)) {
			/*no AR signaled*/
			gf_bs_write_int(mod, 0, 1);
		} else {
			u32 sarx;
			gf_bs_write_int(mod, 1, 1);
			sarx = avc_get_sar_idx((u32) ar_n, (u32) ar_d);
			gf_bs_write_int(mod, sarx, 8);
			if (sarx==0xFF) {
				gf_bs_write_int(mod, ar_n, 16);
				gf_bs_write_int(mod, ar_d, 16);
			}
		}
		/*no VUI in input bitstream, set all vui flags to 0*/
		if (!flag) {
			gf_bs_write_int(mod, 0, 1);     /*overscan_info_present_flag */
			gf_bs_write_int(mod, 0, 1);     /*video_signal_type_present_flag */
			gf_bs_write_int(mod, 0, 1);     /*chroma_location_info_present_flag */
			gf_bs_write_int(mod, 0, 1);		/*timing_info_present_flag*/
			gf_bs_write_int(mod, 0, 1);		/*nal_hrd_parameters_present*/
			gf_bs_write_int(mod, 0, 1);		/*vcl_hrd_parameters_present*/
			gf_bs_write_int(mod, 0, 1);		/*pic_struct_present*/
			gf_bs_write_int(mod, 0, 1);		/*bitstream_restriction*/
		}

		/*finally copy over remaining*/
		while (gf_bs_bits_available(orig)) {
			flag = gf_bs_read_int(orig, 1);
			gf_bs_write_int(mod, flag, 1);
		}
		gf_bs_del(orig);
		free(slc->data);
		slc->data = NULL;
		gf_bs_get_content(mod, (char **) &slc->data, &flag);
		slc->size = flag;
		gf_bs_del(mod);
	}
	return GF_OK;
}

GF_EXPORT
GF_Err gf_avc_get_sps_info(char *sps_data, u32 sps_size, u32 *width, u32 *height, s32 *par_n, s32 *par_d)
{
	GF_BitStream *bs;
	AVCState avc;
	s32 idx;
	memset(&avc, 0, sizeof(AVCState));

	bs = gf_bs_new(sps_data, sps_size, GF_BITSTREAM_READ);
	/*skip NALU type*/
	gf_bs_read_int(bs, 8);
	idx = AVC_ReadSeqInfo(bs, &avc, NULL);
	gf_bs_del(bs);
	if (idx<0) return GF_NON_COMPLIANT_BITSTREAM;

	if (width) *width = avc.sps[idx].width;
	if (height) *height = avc.sps[idx].height;
	if (par_n) *par_n = avc.sps[idx].par_num ? avc.sps[idx].par_num : (u32) -1;
	if (par_d) *par_d = avc.sps[idx].par_den ? avc.sps[idx].par_den : (u32) -1;
	return GF_OK;
}

#endif

GF_EXPORT
const char *gf_avc_get_profile_name(u8 video_prof)
{
	switch (video_prof) {
	case 0x42: return "Baseline";
	case 0x4D: return "Main";
	case 0x58: return "Extended";
	case 0x64: return "High";
	case 0x6E: return "High 10";
	case 0x7A: return "High 4:2:2";
	case 0x90: return "High 4:4:4";
	default: return "Unknown";
	}
}

static u32 AC3_FindSyncCode(u8 *buf, u32 buflen)
{
	u32 end = buflen - 6;
	u32 offset = 0;
	while (offset <= end) {
		if (buf[offset] == 0x0b && buf[offset + 1] == 0x77) {
			return offset;
		}
		offset++;
	}
	return buflen;
}


static Bool AC3_FindSyncCodeBS(GF_BitStream *bs)
{
	u8 b1;
	u32 pos = (u32) gf_bs_get_position(bs);
	u32 end = (u32) gf_bs_get_size(bs) - 6;

	pos += 1;
	b1 = gf_bs_read_u8(bs);
	while (pos <= end) {
		u8 b2 = gf_bs_read_u8(bs);
		if ((b1 == 0x0b) && (b2==0x77)) {
			gf_bs_seek(bs, pos-1);
			return 1;
		}
		pos++;
	}
	return 0;
}

static const u32 ac3_sizecod_to_bitrate[] = {
  32000, 40000, 48000, 56000, 64000, 80000, 96000,
  112000, 128000, 160000, 192000, 224000, 256000,
  320000, 384000, 448000, 512000, 576000, 640000
};

static const u32 ac3_sizecod2_to_framesize[] = {
  96, 120, 144, 168, 192, 240, 288, 336, 384, 480, 576, 672,
  768, 960, 1152, 1344, 1536, 1728, 1920
};

static const u32 ac3_sizecod1_to_framesize[] = {
  69, 87, 104, 121, 139, 174, 208, 243, 278, 348, 417, 487,
  557, 696, 835, 975, 1114, 1253, 1393
};
static const u32 ac3_sizecod0_to_framesize[] = {
  64, 80, 96, 112, 128, 160, 192, 224, 256, 320, 384, 448, 
  512, 640, 768, 896, 1024, 1152, 1280
};

static const u32 ac3_mod_to_chans[] = {
  2, 1, 2, 3, 3, 4, 4, 5
};

Bool gf_ac3_parser(u8 *buf, u32 buflen, u32 *pos, GF_AC3Header *hdr, Bool full_parse)
{
	u32 fscod, frmsizecod, bsid, ac3_mod, freq, framesize;
	if (buflen < 6) return 0;
	(*pos) = AC3_FindSyncCode(buf, buflen);
	if (*pos >= buflen) return 0;

	buf += (*pos);
	fscod = (buf[4] >> 6) & 0x3;
	frmsizecod = (buf[4] & 0x3f);
	bsid = (buf[5] >> 3) & 0x1f;
	ac3_mod = (buf[6] >> 5) & 0x7;
	if (bsid >= 12) return 0;

	if (full_parse && hdr) memset(hdr, 0, sizeof(GF_AC3Header));
	
	if (hdr) {
		hdr->bitrate = ac3_sizecod_to_bitrate[frmsizecod / 2];
		if (bsid > 8) hdr->bitrate = hdr->bitrate >> (bsid - 8);
	}
	switch (fscod) {
	case 0:
		freq = 48000;
		framesize = ac3_sizecod0_to_framesize[frmsizecod / 2] * 2;
		break;
	case 1:
		freq = 44100;
		framesize = (ac3_sizecod1_to_framesize[frmsizecod / 2] + (frmsizecod & 0x1)) * 2;
		break;
	case 2:
		freq = 32000;
		framesize = ac3_sizecod2_to_framesize[frmsizecod / 2] * 2;
		break;
	default:
	    return 0;
	}
	if (hdr) {
		u16 maskbit, b67;
		hdr->sample_rate = freq;
		hdr->framesize = framesize;

		hdr->channels = ac3_mod_to_chans[ac3_mod];
		maskbit = 0x100;
		if ((ac3_mod & 0x1) && (ac3_mod != 1)) maskbit >>= 2;
		if (ac3_mod & 0x4) maskbit >>= 2;
		if (ac3_mod == 0x2) maskbit += 2;
		b67 = (buf[6] << 8) | buf[7];
		if ((b67 & maskbit) != 0) hdr->channels += 1;
	}
	return 1;
}


Bool gf_ac3_parser_bs(GF_BitStream *bs, GF_AC3Header *hdr, Bool full_parse)
{
	u32 fscod, frmsizecod, bsid, ac3_mod, freq, framesize, pos, bsmod;
	if (!hdr || (gf_bs_available(bs) < 6)) return 0;
	if (!AC3_FindSyncCodeBS(bs)) return 0;

	pos = (u32) gf_bs_get_position(bs);
	gf_bs_read_u32(bs);
	fscod = gf_bs_read_int(bs, 2);
	frmsizecod = gf_bs_read_int(bs, 6);
	bsid = gf_bs_read_int(bs, 5);
	bsmod = gf_bs_read_int(bs, 3);
	ac3_mod = gf_bs_read_int(bs, 3);

	if (bsid >= 12) return 0;

	//memset(hdr, 0, sizeof(GF_AC3Header));
	
	hdr->bitrate = ac3_sizecod_to_bitrate[frmsizecod / 2];
	if (bsid > 8) hdr->bitrate = hdr->bitrate >> (bsid - 8);

	switch (fscod) {
	case 0:
		freq = 48000;
		framesize = ac3_sizecod0_to_framesize[frmsizecod / 2] * 2;
		break;
	case 1:
		freq = 44100;
		framesize = (ac3_sizecod1_to_framesize[frmsizecod / 2] + (frmsizecod & 0x1)) * 2;
		break;
	case 2:
		freq = 32000;
		framesize = ac3_sizecod2_to_framesize[frmsizecod / 2] * 2;
		break;
	default:
	    return 0;
	}
	hdr->sample_rate = freq;
	hdr->framesize = framesize;

	if (full_parse) {
		hdr->bsid = bsid;
		hdr->bsmod = bsmod;
		hdr->acmod = ac3_mod;
		hdr->lfon = 0;
		hdr->fscod = fscod;
		hdr->brcode = hdr->bitrate/1000;
	}

	hdr->channels = ac3_mod_to_chans[ac3_mod];
	if ((ac3_mod & 0x1) && (ac3_mod != 1)) gf_bs_read_int(bs, 2);
	if (ac3_mod & 0x4) gf_bs_read_int(bs, 2);
	if (ac3_mod == 0x2) gf_bs_read_int(bs, 2);
	/*LFEon*/
	if (gf_bs_read_int(bs, 1)) {
		hdr->channels += 1;
		hdr->lfon = 1;
	}


	gf_bs_seek(bs, pos);
	return 1;
}



/*
	Vorbis parser
*/

static u32 vorbis_book_maptype1_quantvals(u32 entries, u32 dim)
{
	u32 vals = (u32) floor(pow(entries, 1.0/dim));
	while(1) {
		u32 acc=1;
		u32 acc1=1;
		u32 i;
		for (i=0;i<dim;i++) {
			acc*=vals;
			acc1*=vals+1;
		}
		if(acc<=entries && acc1>entries) return (vals);
		else { 
			if (acc>entries) vals--;
			else vals++;
		}
	}
}

u32 _ilog_(u32 v)
{
	u32 ret=0;
	while(v) {
		ret++;
		v>>=1;
	}
	return(ret);
}

static u32 ilog(u32 v)
{
	u32 ret=0; 
	if(v) --v;
	while(v) {
		ret++;
		v>>=1;
	}
	return (ret);
}

static u32 icount(u32 v)
{
	u32 ret=0;
	while(v) {
		ret += v&1;
		v>>=1;
	}
	return(ret);
}


GF_EXPORT
Bool gf_vorbis_parse_header(GF_VorbisParser *vp, char *data, u32 data_len)
{
	u32 pack_type, i, j, k, times, nb_part, nb_books, nb_modes;
	char szNAME[8];
	oggpack_buffer opb;

	oggpack_readinit(&opb, (u8*)data, data_len);
	pack_type = oggpack_read(&opb, 8);
	i=0;
	while (i<6) { szNAME[i] = oggpack_read(&opb, 8); i++;}
	szNAME[i] = 0;
	if (strcmp(szNAME, "vorbis")) return vp->is_init = 0;

	switch (pack_type) {
	case 0x01:
		vp->version = oggpack_read(&opb, 32);
		if (vp->version!=0) return 0;
		vp->channels = oggpack_read(&opb, 8);
		vp->sample_rate = oggpack_read(&opb, 32);
		vp->max_r = oggpack_read(&opb, 32);
		vp->avg_r = oggpack_read(&opb, 32);
		vp->low_r = oggpack_read(&opb, 32);

		vp->min_block = 1<<oggpack_read(&opb, 4);
		vp->max_block = 1<<oggpack_read(&opb, 4);
		if (vp->sample_rate < 1) return vp->is_init = 0;
		if (vp->channels < 1) return vp->is_init = 0;
		if (vp->min_block<8) return vp->is_init = 0;
		if (vp->max_block < vp->min_block) return vp->is_init = 0;
		if (oggpack_read(&opb, 1) != 1) return vp->is_init = 0;
		vp->is_init = 1;
		return 1;
	case 0x03:
		/*trash comments*/
		vp->is_init ++;
		return 1;
	case 0x05:
		/*need at least bitstream header to make sure we're parsing the right thing*/
		if (!vp->is_init) return 0;
		break;
	default:
		vp->is_init = 0;
		return 0;
	}
	/*OK parse codebook*/
	nb_books = oggpack_read(&opb, 8) + 1;
	/*skip vorbis static books*/
	for (i=0; i<nb_books; i++) {
		u32 j, map_type, qb, qq;
		u32 entries, dim;
		oggpack_read(&opb, 24);
		dim = oggpack_read(&opb, 16);
		entries = oggpack_read(&opb, 24);
		if ( (s32) entries < 0) entries = 0;
		if (oggpack_read(&opb, 1) == 0) {
			if (oggpack_read(&opb, 1)) {
				for (j=0; j<entries; j++) {
					if (oggpack_read(&opb, 1)) {
						oggpack_read(&opb, 5);
					}
				}
			} else {
				for (j=0; j<entries; j++) 
					oggpack_read(&opb, 5);
			}
		} else {
			oggpack_read(&opb, 5);
			for (j=0; j<entries;) {
				u32 num = oggpack_read(&opb, _ilog_(entries-j));
				for (k=0; k<num && j<entries; k++, j++) {
				}
			}
		}
		switch ((map_type=oggpack_read(&opb, 4))) {
		case 0: break;
		case 1:
		case 2:
			oggpack_read(&opb, 32);
			oggpack_read(&opb, 32);
			qq = oggpack_read(&opb, 4)+1;
			oggpack_read(&opb, 1);
			if (map_type==1) qb = vorbis_book_maptype1_quantvals(entries, dim);
			else if (map_type==2) qb = entries * dim;
			else qb = 0;
			for (j=0;j<qb;j++) oggpack_read(&opb, qq);
			break;
		}
	}
	times = oggpack_read(&opb, 6)+1;
	for (i=0;i<times;i++) oggpack_read(&opb, 16);
	times = oggpack_read(&opb, 6)+1;
	for (i=0;i<times;i++) {
		u32 type = oggpack_read(&opb, 16);
		if (type) {
			u32 *parts, *class_dims, count, rangebits;
			u32 max_class = 0;
			nb_part = oggpack_read(&opb, 5);
			parts = (u32*)malloc(sizeof(u32) * nb_part);
			for (j=0;j<nb_part;j++) {
				parts[j] = oggpack_read(&opb, 4);
				if (max_class<parts[j]) max_class = parts[j];
			}
			class_dims = (u32*)malloc(sizeof(u32) * (max_class+1));
			for (j=0; j<max_class+1;j++) {
				u32 class_sub;
				class_dims[j] = oggpack_read(&opb, 3) + 1;
				class_sub = oggpack_read(&opb, 2);
				if (class_sub) oggpack_read(&opb, 8);
				for (k=0; k < (u32) (1<<class_sub); k++) oggpack_read(&opb, 8);
			}
			oggpack_read(&opb, 2);
			rangebits=oggpack_read(&opb, 4);
			count = 0;
			for (j=0,k=0;j<nb_part;j++) {
				count+=class_dims[parts[j]];
				for (;k<count;k++) oggpack_read(&opb, rangebits);
			}
			free(parts);
			free(class_dims);
		} else {
			u32 j, nb_books;
			oggpack_read(&opb, 8+16+16+6+8);
			nb_books = oggpack_read(&opb, 4)+1;
			for (j=0; j<nb_books; j++) oggpack_read(&opb, 8);
		}
	}
	times = oggpack_read(&opb, 6)+1;
	for (i=0;i<times;i++) {
		u32 acc = 0;
		oggpack_read(&opb, 16);/*type*/
		oggpack_read(&opb, 24);
		oggpack_read(&opb,24);
		oggpack_read(&opb,24);
		nb_part = oggpack_read(&opb, 6)+1;
		oggpack_read(&opb, 8);
		for (j=0; j<nb_part;j++) {
			u32 cascade = oggpack_read(&opb, 3);
			if (oggpack_read(&opb, 1)) cascade |= (oggpack_read(&opb, 5)<<3);
			acc += icount(cascade);
		}
		for (j=0;j<acc;j++) oggpack_read(&opb, 8);
	}
	times = oggpack_read(&opb, 6)+1;
	for (i=0; i<times; i++) {
		u32 sub_maps = 1;
		oggpack_read(&opb, 16);
		if (oggpack_read(&opb, 1)) sub_maps = oggpack_read(&opb, 4)+1;
		if (oggpack_read(&opb, 1)) {
			u32 nb_steps = oggpack_read(&opb, 8)+1;
			for (j=0;j<nb_steps;j++) {
				oggpack_read(&opb, ilog(vp->channels));
				oggpack_read(&opb, ilog(vp->channels));
			}
		}
		oggpack_read(&opb, 2);
		if (sub_maps>1) {
			for(j=0; j<vp->channels; j++) oggpack_read(&opb, 4);
		}
		for (j=0;j<sub_maps;j++) {
			oggpack_read(&opb, 8);
			oggpack_read(&opb, 8);
			oggpack_read(&opb, 8);
		}
	}
	nb_modes = oggpack_read(&opb, 6)+1;
	for (i=0; i<nb_modes; i++) {
		vp->mode_flag[i] = oggpack_read(&opb, 1);
		oggpack_read(&opb, 16);
		oggpack_read(&opb, 16);
		oggpack_read(&opb, 8);
	}

	vp->modebits = 0;
	j = nb_modes;
	while(j>1) {
		vp->modebits++;
		j>>=1;
	}
	return 1;
}

GF_EXPORT
u32 gf_vorbis_check_frame(GF_VorbisParser *vp, char *data, u32 data_length)
{
	s32 block_size;
	oggpack_buffer opb;
	if (!vp->is_init) return 0;
	oggpack_readinit(&opb, (unsigned char*)data, data_length);
	/*not audio*/
	if (oggpack_read(&opb, 1) !=0) return 0;
	block_size = oggpack_read(&opb, vp->modebits);
	if (block_size == -1) return 0;
	return ((vp->mode_flag[block_size]) ? vp->max_block : vp->min_block) / (2);
}

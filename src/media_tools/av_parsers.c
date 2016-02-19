/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre, Romain Bouqueau, Cyril Concolato
 *			Copyright (c) Telecom ParisTech 2000-2012
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
#include <gpac/constants.h>
#include <gpac/mpeg4_odf.h>

#ifndef GPAC_DISABLE_OGG
#include <gpac/internal/ogg.h>
#include <gpac/maths.h>
#endif

static const struct {
	u32 w, h;
} std_par[ ] =
{
	{ 4, 3}, {3, 2}, {16, 9}, {5, 3}, {5, 4}, {8, 5}, {2, 1},
	{0, 0},
};

GF_EXPORT
void gf_media_reduce_aspect_ratio(u32 *width, u32 *height)
{
	u32 i=0;
	u32 w = *width;
	u32 h = *height;
	while (std_par[i].w) {
		if (std_par[i].w * h == std_par[i].h * w) {
			*width = std_par[i].w;
			*height = std_par[i].h;
			return;
		}
		i++;
	}
}

GF_EXPORT
void gf_media_get_reduced_frame_rate(u32 *timescale, u32 *sample_dur)
{
	u32 res;
	if (! *sample_dur) return;
	res = *timescale / *sample_dur;
	if (res **sample_dur == *timescale) {
		*timescale = res;
		*sample_dur = 1;
	}
}

GF_EXPORT
const char *gf_m4v_get_profile_name(u8 video_pl)
{
	switch (video_pl) {
	case 0x00:
		return "Reserved (0x00) Profile";
	case 0x01:
		return "Simple Profile @ Level 1";
	case 0x02:
		return "Simple Profile @ Level 2";
	case 0x03:
		return "Simple Profile @ Level 3";
	case 0x08:
		return "Simple Profile @ Level 0";
	case 0x10:
		return "Simple Scalable Profile @ Level 0";
	case 0x11:
		return "Simple Scalable Profile @ Level 1";
	case 0x12:
		return "Simple Scalable Profile @ Level 2";
	case 0x21:
		return "Core Profile @ Level 1";
	case 0x22:
		return "Core Profile @ Level 2";
	case 0x32:
		return "Main Profile @ Level 2";
	case 0x33:
		return "Main Profile @ Level 3";
	case 0x34:
		return "Main Profile @ Level 4";
	case 0x42:
		return "N-bit Profile @ Level 2";
	case 0x51:
		return "Scalable Texture Profile @ Level 1";
	case 0x61:
		return "Simple Face Animation Profile @ Level 1";
	case 0x62:
		return "Simple Face Animation Profile @ Level 2";
	case 0x63:
		return "Simple FBA Profile @ Level 1";
	case 0x64:
		return "Simple FBA Profile @ Level 2";
	case 0x71:
		return "Basic Animated Texture Profile @ Level 1";
	case 0x72:
		return "Basic Animated Texture Profile @ Level 2";
	case 0x7F:
		return "AVC/H264 Profile";
	case 0x81:
		return "Hybrid Profile @ Level 1";
	case 0x82:
		return "Hybrid Profile @ Level 2";
	case 0x91:
		return "Advanced Real Time Simple Profile @ Level 1";
	case 0x92:
		return "Advanced Real Time Simple Profile @ Level 2";
	case 0x93:
		return "Advanced Real Time Simple Profile @ Level 3";
	case 0x94:
		return "Advanced Real Time Simple Profile @ Level 4";
	case 0xA1:
		return "Core Scalable Profile @ Level1";
	case 0xA2:
		return "Core Scalable Profile @ Level2";
	case 0xA3:
		return "Core Scalable Profile @ Level3";
	case 0xB1:
		return "Advanced Coding Efficiency Profile @ Level 1";
	case 0xB2:
		return "Advanced Coding Efficiency Profile @ Level 2";
	case 0xB3:
		return "Advanced Coding Efficiency Profile @ Level 3";
	case 0xB4:
		return "Advanced Coding Efficiency Profile @ Level 4";
	case 0xC1:
		return "Advanced Core Profile @ Level 1";
	case 0xC2:
		return "Advanced Core Profile @ Level 2";
	case 0xD1:
		return "Advanced Scalable Texture @ Level1";
	case 0xD2:
		return "Advanced Scalable Texture @ Level2";
	case 0xE1:
		return "Simple Studio Profile @ Level 1";
	case 0xE2:
		return "Simple Studio Profile @ Level 2";
	case 0xE3:
		return "Simple Studio Profile @ Level 3";
	case 0xE4:
		return "Simple Studio Profile @ Level 4";
	case 0xE5:
		return "Core Studio Profile @ Level 1";
	case 0xE6:
		return "Core Studio Profile @ Level 2";
	case 0xE7:
		return "Core Studio Profile @ Level 3";
	case 0xE8:
		return "Core Studio Profile @ Level 4";
	case 0xF0:
		return "Advanced Simple Profile @ Level 0";
	case 0xF1:
		return "Advanced Simple Profile @ Level 1";
	case 0xF2:
		return "Advanced Simple Profile @ Level 2";
	case 0xF3:
		return "Advanced Simple Profile @ Level 3";
	case 0xF4:
		return "Advanced Simple Profile @ Level 4";
	case 0xF5:
		return "Advanced Simple Profile @ Level 5";
	case 0xF7:
		return "Advanced Simple Profile @ Level 3b";
	case 0xF8:
		return "Fine Granularity Scalable Profile @ Level 0";
	case 0xF9:
		return "Fine Granularity Scalable Profile @ Level 1";
	case 0xFA:
		return "Fine Granularity Scalable Profile @ Level 2";
	case 0xFB:
		return "Fine Granularity Scalable Profile @ Level 3";
	case 0xFC:
		return "Fine Granularity Scalable Profile @ Level 4";
	case 0xFD:
		return "Fine Granularity Scalable Profile @ Level 5";
	case 0xFE:
		return "Not part of MPEG-4 Visual profiles";
	case 0xFF:
		return "No visual capability required";
	default:
		return "ISO Reserved Profile";
	}
}


#ifndef GPAC_DISABLE_AV_PARSERS

#define MPEG12_START_CODE_PREFIX		0x000001
#define MPEG12_PICTURE_START_CODE		0x00000100
#define MPEG12_SLICE_MIN_START			0x00000101
#define MPEG12_SLICE_MAX_START			0x000001af
#define MPEG12_USER_DATA_START_CODE		0x000001b2
#define MPEG12_SEQUENCE_START_CODE		0x000001b3
#define MPEG12_SEQUENCE_ERR_START_CODE	0x000001b4
#define MPEG12_EXT_START_CODE			0x000001b5
#define MPEG12_SEQUENCE_END_START_CODE	0x000001b7
#define MPEG12_GOP_START_CODE			0x000001b8

s32 gf_mv12_next_start_code(unsigned char *pbuffer, u32 buflen, u32 *optr, u32 *scode)
{
	u32 value;
	u32 offset;

	if (buflen < 4) return -1;
	for (offset = 0; offset < buflen - 3; offset++, pbuffer++) {
#ifdef GPAC_BIG_ENDIAN
		value = *(u32 *)pbuffer >> 8;
#else
		value = (pbuffer[0] << 16) | (pbuffer[1] << 8) | (pbuffer[2] << 0);
#endif

		if (value == MPEG12_START_CODE_PREFIX) {
			*optr = offset;
			*scode = (value << 8) | pbuffer[3];
			return 0;
		}
	}
	return -1;
}

s32 gf_mv12_next_slice_start(unsigned char *pbuffer, u32 startoffset, u32 buflen, u32 *slice_offset)
{
	u32 slicestart, code;
	while (gf_mv12_next_start_code(pbuffer + startoffset, buflen - startoffset, &slicestart, &code) >= 0) {
		if ((code >= MPEG12_SLICE_MIN_START) && (code <= MPEG12_SLICE_MAX_START)) {
			*slice_offset = slicestart + startoffset;
			return 0;
		}
		startoffset += slicestart + 4;
	}
	return -1;
}


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
	u64 current_object_start;
	u32 tc_dec, prev_tc_dec, tc_disp, prev_tc_disp;
};

GF_EXPORT
GF_M4VParser *gf_m4v_parser_new(char *data, u64 data_size, Bool mpeg12video)
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
	gf_free(m4v);
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
	m4v->current_object_start = end;
	gf_bs_seek(m4v->bs, end+3);
	m4v->current_object_type = gf_bs_read_u8(m4v->bs);
	return (s32) m4v->current_object_type;
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
	(*o_data) = (char *)gf_malloc(sizeof(char)*(dataLen+5));
	(*o_data)[0] = 0;
	(*o_data)[1] = 0;
	(*o_data)[2] = 1;
	(*o_data)[3] = (char) M4V_VOS_START_CODE;
	(*o_data)[4] = PL;
	memcpy( (*o_data + 5), data, sizeof(char)*dataLen);
	gf_free(data);
	(*o_dataLen) = dataLen + 5;
}

static GF_Err M4V_Reset(GF_M4VParser *m4v, u64 start)
{
	gf_bs_seek(m4v->bs, start);
	assert(start < 1<<31);
	m4v->current_object_start = (u32) start;
	m4v->current_object_type = 0;
	return GF_OK;
}


static GF_Err gf_m4v_parse_config_mpeg12(GF_M4VParser *m4v, GF_M4VDecSpecInfo *dsi)
{
	unsigned char p[4];
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
			gf_bs_read_data(m4v->bs,  (char *) p, 4);
			dsi->width = (p[0] << 4) | ((p[1] >> 4) & 0xf);
			dsi->height = ((p[1] & 0xf) << 8) | p[2];

			dsi->VideoPL = GPAC_OTI_VIDEO_MPEG1;
			par = (p[3] >> 4) & 0xf;
			switch (par) {
			case 2:
				dsi->par_num = dsi->height/3;
				dsi->par_den = dsi->width/4;
				break;
			case 3:
				dsi->par_num = dsi->height/9;
				dsi->par_den = dsi->width/16;
				break;
			case 4:
				dsi->par_num = dsi->height/2;
				dsi->par_den = dsi->width/21;
				break;
			default:
				dsi->par_den = dsi->par_num = 0;
				break;
			}
			switch (p[3] & 0xf) {
			case 0:
				break;
			case 1:
				dsi->fps = 24000.0/1001.0;
				break;
			case 2:
				dsi->fps = 24.0;
				break;
			case 3:
				dsi->fps = 25.0;
				break;
			case 4:
				dsi->fps = 30000.0/1001.0;
				break;
			case 5:
				dsi->fps = 30.0;
				break;
			case 6:
				dsi->fps = 50.0;
				break;
			case 7:
				dsi->fps = ((60.0*1000.0)/1001.0);
				break;
			case 8:
				dsi->fps = 60.0;
				break;
			case 9:
				dsi->fps = 1;
				break;
			case 10:
				dsi->fps = 5;
				break;
			case 11:
				dsi->fps = 10;
				break;
			case 12:
				dsi->fps = 12;
				break;
			case 13:
				dsi->fps = 15;
				break;
			}
			break;
		case M2V_EXT_START_CODE:
			gf_bs_read_data(m4v->bs,  (char *) p, 4);
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
			m4v->current_object_start = gf_bs_get_position(m4v->bs);
			break;
		}
	}
	M4V_Reset(m4v, 0);
	return GF_OK;
}


static const struct {
	u32 w, h;
} m4v_sar[6] = { { 0,   0 }, { 1,   1 }, { 12, 11 }, { 10, 11 }, { 16, 11 }, { 40, 33 } };

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
			m4v->current_object_start = gf_bs_get_position(m4v->bs);
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

static GF_Err gf_m4v_parse_frame_mpeg12(GF_M4VParser *m4v, GF_M4VDecSpecInfo dsi, u8 *frame_type, u32 *time_inc, u64 *size, u64 *start, Bool *is_coded)
{
	u8 go, hasVOP, firstObj, val;
	s32 o_type;

	if (!m4v || !size || !start || !frame_type) return GF_BAD_PARAM;

	*size = 0;
	firstObj = 1;
	hasVOP = 0;
	*is_coded = GF_FALSE;
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
			*size = gf_bs_get_position(m4v->bs) - *start;
			return GF_EOS;
		}
	}
	*size = m4v->current_object_start - *start;
	return GF_OK;
}

static GF_Err gf_m4v_parse_frame_mpeg4(GF_M4VParser *m4v, GF_M4VDecSpecInfo dsi, u8 *frame_type, u32 *time_inc, u64 *size, u64 *start, Bool *is_coded)
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
			*size = gf_bs_get_position(m4v->bs) - *start;
			return GF_EOS;
		}
	}
	*size = m4v->current_object_start - *start;
	return GF_OK;
}

GF_EXPORT
GF_Err gf_m4v_parse_frame(GF_M4VParser *m4v, GF_M4VDecSpecInfo dsi, u8 *frame_type, u32 *time_inc, u64 *size, u64 *start, Bool *is_coded)
{
	if (m4v->mpeg12) {
		return gf_m4v_parse_frame_mpeg12(m4v, dsi, frame_type, time_inc, size, start, is_coded);
	} else {
		return gf_m4v_parse_frame_mpeg4(m4v, dsi, frame_type, time_inc, size, start, is_coded);
	}
}

GF_Err gf_m4v_rewrite_par(char **o_data, u32 *o_dataLen, s32 par_n, s32 par_d)
{
	u64 start, end, size;
	GF_BitStream *mod;
	GF_M4VParser *m4v;
	Bool go = 1;

	m4v = gf_m4v_parser_new(*o_data, *o_dataLen, 0);
	mod = gf_bs_new(NULL, 0, GF_BITSTREAM_WRITE);

	end = start = 0;
	while (go) {
		u32 type = M4V_LoadObject(m4v);

		end = gf_bs_get_position(m4v->bs) - 4;
		size = end - start;
		/*store previous object*/
		if (size) {
			assert (size < 1<<31);
			if (size) gf_bs_write_data(mod, *o_data + start, (u32) size);
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
			gf_bs_write_int(mod, (u32) start, 1);
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
	gf_free(*o_data);
	gf_bs_get_content(mod, o_data, o_dataLen);
	gf_bs_del(mod);
	return GF_OK;
}

GF_EXPORT
u64 gf_m4v_get_object_start(GF_M4VParser *m4v)
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
#endif


/*
	AAC parser
*/

GF_EXPORT
const char *gf_m4a_object_type_name(u32 objectType)
{
	switch (objectType) {
	case 0:
		return "MPEG-4 Audio Reserved";
	case 1:
		return "MPEG-4 Audio AAC Main";
	case 2:
		return "MPEG-4 Audio AAC LC";
	case 3:
		return "MPEG-4 Audio AAC SSR";
	case 4:
		return "MPEG-4 Audio AAC LTP";
	case 5:
		return "MPEG-4 Audio SBR";
	case 6:
		return "MPEG-4 Audio AAC Scalable";
	case 7:
		return "MPEG-4 Audio TwinVQ";
	case 8:
		return "MPEG-4 Audio CELP";
	case 9:
		return "MPEG-4 Audio HVXC";
	case 10:
		return "MPEG-4 Audio Reserved";
	case 11:
		return "MPEG-4 Audio Reserved";
	case 12:
		return "MPEG-4 Audio TTSI";
	case 13:
		return "MPEG-4 Audio Main synthetic";
	case 14:
		return "MPEG-4 Audio Wavetable synthesis";
	case 15:
		return "MPEG-4 Audio General MIDI";
	case 16:
		return "MPEG-4 Audio Algorithmic Synthesis and Audio FX";
	case 17:
		return "MPEG-4 Audio ER AAC LC";
	case 18:
		return "MPEG-4 Audio Reserved";
	case 19:
		return "MPEG-4 Audio ER AAC LTP";
	case 20:
		return "MPEG-4 Audio ER AAC scalable";
	case 21:
		return "MPEG-4 Audio ER TwinVQ";
	case 22:
		return "MPEG-4 Audio ER BSAC";
	case 23:
		return "MPEG-4 Audio ER AAC LD";
	case 24:
		return "MPEG-4 Audio ER CELP";
	case 25:
		return "MPEG-4 Audio ER HVXC";
	case 26:
		return "MPEG-4 Audio ER HILN";
	case 27:
		return "MPEG-4 Audio ER Parametric";
	case 28:
		return "MPEG-4 Audio SSC";
	case 29:
		return "MPEG-4 Audio ParametricStereo";
	case 30:
		return "MPEG-4 Audio Reserved";
	case 31:
		return "MPEG-4 Audio Reserved";
	case 32:
		return "MPEG-1 Audio Layer-1";
	case 33:
		return "MPEG-1 Audio Layer-2";
	case 34:
		return "MPEG-1 Audio Layer-3";
	case 35:
		return "MPEG-4 Audio DST";
	case 36:
		return "MPEG-4 Audio ALS";
	default:
		return "MPEG-4 Audio Unknown";
	}
}

GF_EXPORT
const char *gf_m4a_get_profile_name(u8 audio_pl)
{
	switch (audio_pl) {
	case 0x00:
		return "ISO Reserved (0x00)";
	case 0x01:
		return "Main Audio Profile @ Level 1";
	case 0x02:
		return "Main Audio Profile @ Level 2";
	case 0x03:
		return "Main Audio Profile @ Level 3";
	case 0x04:
		return "Main Audio Profile @ Level 4";
	case 0x05:
		return "Scalable Audio Profile @ Level 1";
	case 0x06:
		return "Scalable Audio Profile @ Level 2";
	case 0x07:
		return "Scalable Audio Profile @ Level 3";
	case 0x08:
		return "Scalable Audio Profile @ Level 4";
	case 0x09:
		return "Speech Audio Profile @ Level 1";
	case 0x0A:
		return "Speech Audio Profile @ Level 2";
	case 0x0B:
		return "Synthetic Audio Profile @ Level 1";
	case 0x0C:
		return "Synthetic Audio Profile @ Level 2";
	case 0x0D:
		return "Synthetic Audio Profile @ Level 3";
	case 0x0E:
		return "High Quality Audio Profile @ Level 1";
	case 0x0F:
		return "High Quality Audio Profile @ Level 2";
	case 0x10:
		return "High Quality Audio Profile @ Level 3";
	case 0x11:
		return "High Quality Audio Profile @ Level 4";
	case 0x12:
		return "High Quality Audio Profile @ Level 5";
	case 0x13:
		return "High Quality Audio Profile @ Level 6";
	case 0x14:
		return "High Quality Audio Profile @ Level 7";
	case 0x15:
		return "High Quality Audio Profile @ Level 8";
	case 0x16:
		return "Low Delay Audio Profile @ Level 1";
	case 0x17:
		return "Low Delay Audio Profile @ Level 2";
	case 0x18:
		return "Low Delay Audio Profile @ Level 3";
	case 0x19:
		return "Low Delay Audio Profile @ Level 4";
	case 0x1A:
		return "Low Delay Audio Profile @ Level 5";
	case 0x1B:
		return "Low Delay Audio Profile @ Level 6";
	case 0x1C:
		return "Low Delay Audio Profile @ Level 7";
	case 0x1D:
		return "Low Delay Audio Profile @ Level 8";
	case 0x1E:
		return "Natural Audio Profile @ Level 1";
	case 0x1F:
		return "Natural Audio Profile @ Level 2";
	case 0x20:
		return "Natural Audio Profile @ Level 3";
	case 0x21:
		return "Natural Audio Profile @ Level 4";
	case 0x22:
		return "Mobile Audio Internetworking Profile @ Level 1";
	case 0x23:
		return "Mobile Audio Internetworking Profile @ Level 2";
	case 0x24:
		return "Mobile Audio Internetworking Profile @ Level 3";
	case 0x25:
		return "Mobile Audio Internetworking Profile @ Level 4";
	case 0x26:
		return "Mobile Audio Internetworking Profile @ Level 5";
	case 0x27:
		return "Mobile Audio Internetworking Profile @ Level 6";
	case 0x28:
		return "AAC Profile @ Level 1";
	case 0x29:
		return "AAC Profile @ Level 2";
	case 0x2A:
		return "AAC Profile @ Level 4";
	case 0x2B:
		return "AAC Profile @ Level 5";
	case 0x2C:
		return "High Efficiency AAC Profile @ Level 2";
	case 0x2D:
		return "High Efficiency AAC Profile @ Level 3";
	case 0x2E:
		return "High Efficiency AAC Profile @ Level 4";
	case 0x2F:
		return "High Efficiency AAC Profile @ Level 5";
	case 0x30:
		return "High Efficiency AAC v2 Profile @ Level 2";
	case 0x31:
		return "High Efficiency AAC v2 Profile @ Level 3";
	case 0x32:
		return "High Efficiency AAC v2 Profile @ Level 4";
	case 0x33:
		return "High Efficiency AAC v2 Profile @ Level 5";
	case 0x34:
		return "Low Delay AAC Profile";
	case 0x35:
		return "Baseline MPEG Surround Profile @ Level 1";
	case 0x36:
		return "Baseline MPEG Surround Profile @ Level 2";
	case 0x37:
		return "Baseline MPEG Surround Profile @ Level 3";
	case 0x38:
		return "Baseline MPEG Surround Profile @ Level 4";
	case 0x39:
		return "Baseline MPEG Surround Profile @ Level 5";
	case 0x3A:
		return "Baseline MPEG Surround Profile @ Level 6";

	case 0x50:
		return "AAC Profile @ Level 6";
	case 0x51:
		return "AAC Profile @ Level 7";
	case 0x52:
		return "High Efficiency AAC Profile @ Level 6";
	case 0x53:
		return "High Efficiency AAC Profile @ Level 7";
	case 0x54:
		return "High Efficiency AAC v2 Profile @ Level 6";
	case 0x55:
		return "High Efficiency AAC v2 Profile @ Level 7";
	case 0x56:
		return "Extended High Efficiency AAC Profile @ Level 6";
	case 0x57:
		return "Extended High Efficiency AAC Profile @ Level 7";

	case 0xFE:
		return "Not part of MPEG-4 audio profiles";
	case 0xFF:
		return "No audio capability required";
	default:
		return "ISO Reserved / User Private";
	}
}

#ifndef GPAC_DISABLE_AV_PARSERS

GF_EXPORT
u32 gf_m4a_get_profile(GF_M4ADecSpecInfo *cfg)
{
	switch (cfg->base_object_type) {
	case 2: /*AAC LC*/
		if (cfg->nb_chan<=2)
			return (cfg->base_sr<=24000) ? 0x28 : 0x29; /*LC@L1 or LC@L2*/
		if (cfg->nb_chan<=5)
			return (cfg->base_sr<=48000) ? 0x2A : 0x2B; /*LC@L4 or LC@L5*/
		return (cfg->base_sr<=48000) ? 0x50 : 0x51; /*LC@L4 or LC@L5*/
	case 5: /*HE-AAC - SBR*/
		if (cfg->nb_chan<=2)
			return (cfg->base_sr<=24000) ? 0x2C : 0x2D; /*HE@L2 or HE@L3*/
		if (cfg->nb_chan<=5)
			return (cfg->base_sr<=48000) ? 0x2E : 0x2F; /*HE@L4 or HE@L5*/
		return (cfg->base_sr<=48000) ? 0x52 : 0x53; /*HE@L6 or HE@L7*/
	case 29: /*HE-AACv2 - SBR+PS*/
		if (cfg->nb_chan<=2)
			return (cfg->base_sr<=24000) ? 0x30 : 0x31; /*HE-AACv2@L2 or HE-AACv2@L3*/
		if (cfg->nb_chan<=5)
			return (cfg->base_sr<=48000) ? 0x32 : 0x33; /*HE-AACv2@L4 or HE-AACv2@L5*/
		return (cfg->base_sr<=48000) ? 0x54 : 0x55; /*HE-AACv2@L6 or HE-AACv2@L7*/
	/*default to HQ*/
	default:
		if (cfg->nb_chan<=2) return (cfg->base_sr<24000) ? 0x0E : 0x0F; /*HQ@L1 or HQ@L2*/
		return 0x10; /*HQ@L3*/
	}
}



GF_EXPORT
GF_Err gf_m4a_parse_config(GF_BitStream *bs, GF_M4ADecSpecInfo *cfg, Bool size_known)
{
	u32 channel_configuration = 0;
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

	channel_configuration = gf_bs_read_int(bs, 4);

	if (channel_configuration) {
		cfg->nb_chan = GF_M4ANumChannels[channel_configuration-1];
	}

	if (cfg->base_object_type==5 || cfg->base_object_type==29) {
		if (cfg->base_object_type==29) {
			cfg->has_ps = 1;
			cfg->nb_chan = 1;
		}
		cfg->has_sbr = GF_TRUE;
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
		Bool ext_flag;
		/*frame length flag*/
		/*fl_flag = */gf_bs_read_int(bs, 1);
		/*depends on core coder*/
		if (gf_bs_read_int(bs, 1))
			/*delay = */gf_bs_read_int(bs, 14);
		ext_flag = gf_bs_read_int(bs, 1);

		if (! channel_configuration) {
			u32 i;
			cfg->program_config_element_present = 1;
			cfg->element_instance_tag = gf_bs_read_int(bs, 4);
			cfg->object_type = gf_bs_read_int(bs, 2);
			cfg->sampling_frequency_index = gf_bs_read_int(bs, 4);
			cfg->num_front_channel_elements = gf_bs_read_int(bs, 4);
			cfg->num_side_channel_elements = gf_bs_read_int(bs, 4);
			cfg->num_back_channel_elements = gf_bs_read_int(bs, 4);
			cfg->num_lfe_channel_elements = gf_bs_read_int(bs, 2);
			cfg->num_assoc_data_elements = gf_bs_read_int(bs, 3);
			cfg->num_valid_cc_elements = gf_bs_read_int(bs, 4);
			cfg-> mono_mixdown_present = (Bool) gf_bs_read_int(bs, 1);
			if (cfg->mono_mixdown_present) {
				cfg->mono_mixdown_element_number = gf_bs_read_int(bs, 4);
			}
			cfg->stereo_mixdown_present = gf_bs_read_int(bs, 1);
			if (cfg->stereo_mixdown_present) {
				cfg->stereo_mixdown_element_number = gf_bs_read_int(bs, 4);
			}
			cfg->matrix_mixdown_idx_present = gf_bs_read_int(bs, 1);
			if (cfg->matrix_mixdown_idx_present) {
				cfg->matrix_mixdown_idx = gf_bs_read_int(bs, 2);
				cfg->pseudo_surround_enable = gf_bs_read_int(bs, 1);
			}
			for (i = 0; i < cfg->num_front_channel_elements; i++) {
				cfg->front_element_is_cpe[i] = gf_bs_read_int(bs, 1);
				cfg->front_element_tag_select[i] = gf_bs_read_int(bs, 4);
			}
			for (i = 0; i < cfg->num_side_channel_elements; i++) {
				cfg->side_element_is_cpe[i] = gf_bs_read_int(bs, 1);
				cfg->side_element_tag_select[i] = gf_bs_read_int(bs, 4);
			}
			for (i = 0; i < cfg->num_back_channel_elements; i++) {
				cfg->back_element_is_cpe[i] = gf_bs_read_int(bs, 1);
				cfg->back_element_tag_select[i] = gf_bs_read_int(bs, 4);
			}
			for (i = 0; i < cfg->num_lfe_channel_elements; i++) {
				cfg->lfe_element_tag_select[i] = gf_bs_read_int(bs, 4);
			}
			for ( i = 0; i < cfg->num_assoc_data_elements; i++) {
				cfg->assoc_data_element_tag_select[i] = gf_bs_read_int(bs, 4);
			}

			for (i = 0; i < cfg->num_valid_cc_elements; i++) {
				cfg->cc_element_is_ind_sw[i] = gf_bs_read_int(bs, 1);
				cfg->valid_cc_element_tag_select[i] = gf_bs_read_int(bs, 4);
			}
			gf_bs_align(bs);
			cfg->comment_field_bytes = gf_bs_read_int(bs, 8);
			gf_bs_read_data(bs, (char *) cfg->comments, cfg->comment_field_bytes);

			cfg->nb_chan = cfg->num_front_channel_elements + cfg->num_back_channel_elements + cfg->num_side_channel_elements + cfg->num_lfe_channel_elements;
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

	if (size_known && (cfg->base_object_type != 5) && (cfg->base_object_type != 29) ) {
		while (gf_bs_available(bs)>=2) {
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
			} else if (sync == 0x548) {
				gf_bs_read_int(bs, 11);
				cfg->has_ps = gf_bs_read_int(bs, 1);
				if (cfg->has_ps)
					cfg->nb_chan = 1;
			} else {
				break;
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

u32 gf_latm_get_value(GF_BitStream *bs)
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

GF_EXPORT
u32 gf_m4a_get_channel_cfg(u32 nb_chan)
{
	u32 i, count = sizeof(GF_M4ANumChannels)/sizeof(u32);
	for (i=0; i<count; i++) {
		if (GF_M4ANumChannels[i] == nb_chan) return i+1;
	}
	return 0;
}

GF_EXPORT
GF_Err gf_m4a_write_config_bs(GF_BitStream *bs, GF_M4ADecSpecInfo *cfg)
{
	if (!cfg->base_sr_index) {
		if (!cfg->base_sr) return GF_BAD_PARAM;
		while (GF_M4ASampleRates[cfg->base_sr_index]) {
			if (GF_M4ASampleRates[cfg->base_sr_index]==cfg->base_sr)
				break;
			cfg->base_sr_index++;
		}
	}
	if (cfg->sbr_sr && !cfg->sbr_sr_index) {
		while (GF_M4ASampleRates[cfg->sbr_sr_index]) {
			if (GF_M4ASampleRates[cfg->sbr_sr_index]==cfg->sbr_sr)
				break;
			cfg->sbr_sr_index++;
		}
	}
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

	if (cfg->program_config_element_present) {
		gf_bs_write_int(bs, 0, 4);
	} else {
		gf_bs_write_int(bs, gf_m4a_get_channel_cfg( cfg->nb_chan) , 4);
	}

	if (cfg->base_object_type==5 || cfg->base_object_type==29) {
		if (cfg->base_object_type == 29) {
			cfg->has_ps = 1;
			cfg->nb_chan = 1;
		}
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

		if (cfg->program_config_element_present) {
			u32 i;
			gf_bs_write_int(bs, cfg->element_instance_tag, 4);
			gf_bs_write_int(bs, cfg->object_type, 2);
			gf_bs_write_int(bs, cfg->sampling_frequency_index, 4);
			gf_bs_write_int(bs, cfg->num_front_channel_elements, 4);
			gf_bs_write_int(bs, cfg->num_side_channel_elements, 4);
			gf_bs_write_int(bs, cfg->num_back_channel_elements, 4);
			gf_bs_write_int(bs, cfg->num_lfe_channel_elements, 2);
			gf_bs_write_int(bs, cfg->num_assoc_data_elements, 3);
			gf_bs_write_int(bs, cfg->num_valid_cc_elements, 4);
			gf_bs_write_int(bs, cfg-> mono_mixdown_present, 1);
			if (cfg->mono_mixdown_present) {
				gf_bs_write_int(bs, cfg->mono_mixdown_element_number, 4);
			}
			gf_bs_write_int(bs, cfg->stereo_mixdown_present, 1);
			if (cfg->stereo_mixdown_present) {
				gf_bs_write_int(bs, cfg->stereo_mixdown_element_number, 4);
			}
			gf_bs_write_int(bs, cfg->matrix_mixdown_idx_present, 1);
			if (cfg->matrix_mixdown_idx_present) {
				gf_bs_write_int(bs, cfg->matrix_mixdown_idx, 2);
				gf_bs_write_int(bs, cfg->pseudo_surround_enable, 1);
			}
			for (i = 0; i < cfg->num_front_channel_elements; i++) {
				gf_bs_write_int(bs, cfg->front_element_is_cpe[i], 1);
				gf_bs_write_int(bs, cfg->front_element_tag_select[i], 4);
			}
			for (i = 0; i < cfg->num_side_channel_elements; i++) {
				gf_bs_write_int(bs, cfg->side_element_is_cpe[i], 1);
				gf_bs_write_int(bs, cfg->side_element_tag_select[i], 4);
			}
			for (i = 0; i < cfg->num_back_channel_elements; i++) {
				gf_bs_write_int(bs, cfg->back_element_is_cpe[i], 1);
				gf_bs_write_int(bs, cfg->back_element_tag_select[i], 4);
			}
			for (i = 0; i < cfg->num_lfe_channel_elements; i++) {
				gf_bs_write_int(bs, cfg->lfe_element_tag_select[i], 4);
			}
			for ( i = 0; i < cfg->num_assoc_data_elements; i++) {
				gf_bs_write_int(bs, cfg->assoc_data_element_tag_select[i], 4);
			}

			for (i = 0; i < cfg->num_valid_cc_elements; i++) {
				gf_bs_write_int(bs, cfg->cc_element_is_ind_sw[i], 1);
				gf_bs_write_int(bs, cfg->valid_cc_element_tag_select[i], 4);
			}
			gf_bs_align(bs);
			gf_bs_write_int(bs, cfg->comment_field_bytes, 8);
			gf_bs_write_data(bs, (char *) cfg->comments, cfg->comment_field_bytes);
		}

		if ((cfg->base_object_type == 6) || (cfg->base_object_type == 20)) {
			gf_bs_write_int(bs, 0, 3);
		}
	}
	break;
	}
	/*ER cfg - not supported*/

	/*implicit sbr - not used yet*/
	if (0 && (cfg->base_object_type != 5) && (cfg->base_object_type != 29) ) {
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
	return GF_OK;
}

GF_EXPORT
GF_Err gf_m4a_write_config(GF_M4ADecSpecInfo *cfg, char **dsi, u32 *dsi_size)
{
	GF_BitStream *bs = gf_bs_new(NULL, 0, GF_BITSTREAM_WRITE);
	gf_m4a_write_config_bs(bs, cfg);
	gf_bs_get_content(bs, dsi, dsi_size);
	gf_bs_del(bs);
	return GF_OK;
}

#endif /*GPAC_DISABLE_AV_PARSERS*/

GF_EXPORT
u8 gf_mp3_version(u32 hdr)
{
	return ((hdr >> 19) & 0x3);
}

GF_EXPORT
const char *gf_mp3_version_name(u32 hdr)
{
	u32 v = gf_mp3_version(hdr);
	switch (v) {
	case 0:
		return "MPEG-2.5";
	case 1:
		return "Reserved";
	case 2:
		return "MPEG-2";
	case 3:
		return "MPEG-1";
	default:
		return "Unknown";
	}
}

#ifndef GPAC_DISABLE_AV_PARSERS


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

GF_EXPORT
u16 gf_mp3_sampling_rate(u32 hdr)
{
	u16 res;
	/* extract the necessary fields from the MP3 header */
	u8 version = gf_mp3_version(hdr);
	u8 sampleRateIndex = (hdr >> 10) & 0x3;

	switch (sampleRateIndex) {
	case 0:
		res = 44100;
		break;
	case 1:
		res = 48000;
		break;
	case 2:
		res = 32000;
		break;
	default:
		GF_LOG(GF_LOG_ERROR, GF_LOG_CODING, ("[MPEG-1/2 Audio] Samplerate index not valid\n"));
		return 0;
	}
	/*reserved or MPEG-1*/
	if (version & 1) return res;

	/*MPEG-2*/
	res /= 2;
	/*MPEG-2.5*/
	if (version == 0) res /= 2;
	return res;
}

GF_EXPORT
u16 gf_mp3_window_size(u32 hdr)
{
	u8 version = gf_mp3_version(hdr);
	u8 layer = gf_mp3_layer(hdr);

	if (layer == 3) {
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
		return GPAC_OTI_AUDIO_MPEG1;
	case 2:
	case 0:
		return GPAC_OTI_AUDIO_MPEG2_PART3;
	default:
		return 0x00;
	}
}

/*aligned bitrate parsing with libMAD*/

static
u32 const bitrate_table[5][15] = {
	/* MPEG-1 */
	{	0,  32000,  64000,  96000, 128000, 160000, 192000, 224000,  /* Layer I   */
		256000, 288000, 320000, 352000, 384000, 416000, 448000
	},
	{	0,  32000,  48000,  56000,  64000,  80000,  96000, 112000,  /* Layer II  */
		128000, 160000, 192000, 224000, 256000, 320000, 384000
	},
	{	0,  32000,  40000,  48000,  56000,  64000,  80000,  96000,  /* Layer III */
		112000, 128000, 160000, 192000, 224000, 256000, 320000
	},

	/* MPEG-2 LSF */
	{	0,  32000,  48000,  56000,  64000,  80000,  96000, 112000,  /* Layer I   */
		128000, 144000, 160000, 176000, 192000, 224000, 256000
	},
	{	0,   8000,  16000,  24000,  32000,  40000,  48000,  56000,  /* Layers    */
		64000,  80000,  96000, 112000, 128000, 144000, 160000
	} /* II & III  */
};


u32 gf_mp3_bit_rate(u32 hdr)
{
	u8 version = gf_mp3_version(hdr);
	u8 layer = gf_mp3_layer(hdr);
	u8 bitRateIndex = (hdr >> 12) & 0xF;

	if (bitRateIndex == 15) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_CODING, ("[MPEG-1/2 Audio] Bitrate index not valid\n"));
		return 0;
	}

	/*MPEG-1*/
	if (version & 1)
		return bitrate_table[layer - 1][bitRateIndex];
	/*MPEG-2/2.5*/
	else
		return bitrate_table[3 + (layer >> 1)][bitRateIndex];
}



GF_EXPORT
u16 gf_mp3_frame_size(u32 hdr)
{
	u8 version = gf_mp3_version(hdr);
	u8 layer = gf_mp3_layer(hdr);
	u32 pad = ( (hdr >> 9) & 0x1) ? 1 : 0;
	u32 bitrate = gf_mp3_bit_rate(hdr);
	u32 samplerate = gf_mp3_sampling_rate(hdr);

	u32 frameSize = 0;
	if (!samplerate || !bitrate) return 0;

	if (layer==1) {
		frameSize = (( 12 * bitrate / samplerate) + pad) * 4;
	} else {
		u32 slots_per_frame = 144;
		if ((layer == 3) && !(version & 1)) slots_per_frame = 72;
		frameSize = (slots_per_frame * bitrate / samplerate) + pad;
	}
	return (u16) frameSize;
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
u32 gf_mp3_get_next_header_mem(const char *buffer, u32 size, u32 *pos)
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

#endif /*GPAC_DISABLE_AV_PARSERS*/



GF_EXPORT
const char *gf_avc_get_profile_name(u8 video_prof)
{
	switch (video_prof) {
	case 0x42:
		return "Baseline";
	case 0x4D:
		return "Main";
	case 0x53:
		return "Scalable Baseline";
	case 0x56:
		return "Scalable High";
	case 0x58:
		return "Extended";
	case 0x64:
		return "High";
	case 0x6E:
		return "High 10";
	case 0x7A:
		return "High 4:2:2";
	case 0x90:
		return "High 4:4:4";
	default:
		return "Unknown";
	}
}

GF_EXPORT
const char *gf_hevc_get_profile_name(u8 video_prof)
{
	switch (video_prof) {
	case 0x01:
		return "Main";
	case 0x02:
		return "Main 10";
	case 0x03:
		return "Main Still Picture";
	default:
		return "Unknown";
	}
}

#ifndef GPAC_DISABLE_AV_PARSERS


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

static u32 bs_get_ue(GF_BitStream *bs)
{
	u8 coded;
	u32 bits = 0, read = 0;
	while (1) {
		read = gf_bs_peek_bits(bs, 8, 0);
		if (read) break;
		//check whether we still have bits once the peek is done since we may have less than 8 bits available
		if (!gf_bs_available(bs)) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_CODING, ("[AVC/HEVC] Not enough bits in bitstream !!\n"));
			return 0;
		}
		gf_bs_read_int(bs, 8);
		bits += 8;
	}
	coded = avc_golomb_bits[read];
	gf_bs_read_int(bs, coded);
	bits += coded;
	return gf_bs_read_int(bs, bits + 1) - 1;
}

static s32 bs_get_se(GF_BitStream *bs)
{
	u32 v = bs_get_ue(bs);
	if ((v & 0x1) == 0) return (s32) (0 - (v>>1));
	return (v + 1) >> 1;
}

u32 gf_media_nalu_is_start_code(GF_BitStream *bs)
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

static u32 gf_media_nalu_locate_start_code_bs(GF_BitStream *bs, Bool locate_trailing)
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
		if (locate_trailing) {
			if ( (v & 0x00FFFFFF) == 0x00000000) end = cache_start+bpos-3;
		}

		if (v == 0x00000001) end = cache_start+bpos-4;
		else if ( (v & 0x00FFFFFF) == 0x00000001) end = cache_start+bpos-3;
	}
	gf_bs_seek(bs, start);
	if (!end) end = gf_bs_get_size(bs);
	return (u32) (end-start);
}

GF_EXPORT
u32 gf_media_nalu_next_start_code_bs(GF_BitStream *bs)
{
	return gf_media_nalu_locate_start_code_bs(bs, 0);
}

GF_EXPORT
u32 gf_media_nalu_payload_end_bs(GF_BitStream *bs)
{
	return gf_media_nalu_locate_start_code_bs(bs, 1);
}

GF_EXPORT
u32 gf_media_nalu_next_start_code(const u8 *data, u32 data_len, u32 *sc_size)
{
	u32 v, bpos;
	u32 end;

	bpos = 0;
	end = 0;
	v = 0xffffffff;
	while (!end) {
		/*refill cache*/
		if (bpos == (u32) data_len)
			break;

		v = ( (v<<8) & 0xFFFFFF00) | ((u32) data[bpos]);
		bpos++;
		if (v == 0x00000001) {
			end = bpos-4;
			*sc_size = 4;
			return end;
		}
		else if ( (v & 0x00FFFFFF) == 0x00000001) {
			end = bpos-3;
			*sc_size = 3;
			return end;
		}
	}
	if (!end) end = data_len;
	return (u32) (end);
}

Bool gf_media_avc_slice_is_intra(AVCState *avc)
{
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

Bool gf_media_avc_slice_is_IDR(AVCState *avc)
{
	if (avc->sei.recovery_point.valid)
	{
		avc->sei.recovery_point.valid = 0;
		return 1;
	}
	if (avc->s_info.nal_unit_type != GF_AVC_NALU_IDR_SLICE)
		return 0;
	return gf_media_avc_slice_is_intra(avc);
}

static const struct {
	u32 w, h;
} avc_sar[14] =
{
	{ 0,   0 }, { 1,   1 }, { 12, 11 }, { 10, 11 },
	{ 16, 11 }, { 40, 33 }, { 24, 11 }, { 20, 11 },
	{ 32, 11 }, { 80, 33 }, { 18, 11 }, { 15, 11 },
	{ 64, 33 }, { 160,99 },
};


/*ISO 14496-10 (N11084) E.1.2*/
static void avc_parse_hrd_parameters(GF_BitStream *bs, AVC_HRD *hrd)
{
	int i, cpb_cnt_minus1;

	cpb_cnt_minus1 = bs_get_ue(bs);		/*cpb_cnt_minus1*/
	if (cpb_cnt_minus1 > 31)
		GF_LOG(GF_LOG_WARNING, GF_LOG_CODING, ("[avc-h264] invalid cpb_cnt_minus1 value: %d (expected in [0;31])\n", cpb_cnt_minus1));
	gf_bs_read_int(bs, 4);				/*bit_rate_scale*/
	gf_bs_read_int(bs, 4);				/*cpb_size_scale*/

	/*for( SchedSelIdx = 0; SchedSelIdx <= cpb_cnt_minus1; SchedSelIdx++ ) {*/
	for (i=0; i<=cpb_cnt_minus1; i++) {
		bs_get_ue(bs);					/*bit_rate_value_minus1[ SchedSelIdx ]*/
		bs_get_ue(bs);					/*cpb_size_value_minus1[ SchedSelIdx ]*/
		gf_bs_read_int(bs, 1);			/*cbr_flag[ SchedSelIdx ]*/
	}
	gf_bs_read_int(bs, 5);											/*initial_cpb_removal_delay_length_minus1*/
	hrd->cpb_removal_delay_length_minus1 = gf_bs_read_int(bs, 5);	/*cpb_removal_delay_length_minus1*/
	hrd->dpb_output_delay_length_minus1  = gf_bs_read_int(bs, 5);	/*dpb_output_delay_length_minus1*/
	hrd->time_offset_length = gf_bs_read_int(bs, 5);				/*time_offset_length*/

	return;
}

/*returns the nal_size without emulation prevention bytes*/
static u32 avc_emulation_bytes_add_count(char *buffer, u32 nal_size)
{
	u32 i = 0, emulation_bytes_count = 0;
	u8 num_zero = 0;

	while (i < nal_size) {
		/*ISO 14496-10: "Within the NAL unit, any four-byte sequence that starts with 0x000003
		other than the following sequences shall not occur at any byte-aligned position:
		\96 0x00000300
		\96 0x00000301
		\96 0x00000302
		\96 0x00000303"
		*/
		if (num_zero == 2 && buffer[i] < 0x04) {
			/*emulation code found*/
			num_zero = 0;
			emulation_bytes_count++;
			if (!buffer[i])
				num_zero = 1;
		} else {
			if (!buffer[i])
				num_zero++;
			else
				num_zero = 0;
		}
		i++;
	}
	return emulation_bytes_count;
}

static u32 avc_add_emulation_bytes(const char *buffer_src, char *buffer_dst, u32 nal_size)
{

	u32 i = 0, emulation_bytes_count = 0;
	u8 num_zero = 0;

	while (i < nal_size) {
		/*ISO 14496-10: "Within the NAL unit, any four-byte sequence that starts with 0x000003
		other than the following sequences shall not occur at any byte-aligned position:
		0x00000300
		0x00000301
		0x00000302
		0x00000303"
		*/
		if (num_zero == 2 && buffer_src[i] < 0x04) {
			/*add emulation code*/
			num_zero = 0;
			buffer_dst[i+emulation_bytes_count] = 0x03;
			emulation_bytes_count++;
			if (!buffer_src[i])
				num_zero = 1;
		} else {
			if (!buffer_src[i])
				num_zero++;
			else
				num_zero = 0;
		}
		buffer_dst[i+emulation_bytes_count] = buffer_src[i];
		i++;
	}
	return nal_size+emulation_bytes_count;
}
#ifdef GPAC_UNUSED_FUNC
/*returns the nal_size without emulation prevention bytes*/
static u32 avc_emulation_bytes_remove_count(unsigned char *buffer, u32 nal_size)
{
	u32 i = 0, emulation_bytes_count = 0;
	u8 num_zero = 0;

	while (i < nal_size)
	{
		/*ISO 14496-10: "Within the NAL unit, any four-byte sequence that starts with 0x000003
		  other than the following sequences shall not occur at any byte-aligned position:
		  \96 0x00000300
		  \96 0x00000301
		  \96 0x00000302
		  \96 0x00000303"
		*/
		if (num_zero == 2
		        && buffer[i] == 0x03
		        && i+1 < nal_size /*next byte is readable*/
		        && buffer[i+1] < 0x04)
		{
			/*emulation code found*/
			num_zero = 0;
			emulation_bytes_count++;
			i++;
		}

		if (!buffer[i])
			num_zero++;
		else
			num_zero = 0;

		i++;
	}

	return emulation_bytes_count;
}
#endif /*GPAC_UNUSED_FUNC*/

/*nal_size is updated to allow better error detection*/
static u32 avc_remove_emulation_bytes(const char *buffer_src, char *buffer_dst, u32 nal_size)
{
	u32 i = 0, emulation_bytes_count = 0;
	u8 num_zero = 0;

	while (i < nal_size)
	{
		/*ISO 14496-10: "Within the NAL unit, any four-byte sequence that starts with 0x000003
		  other than the following sequences shall not occur at any byte-aligned position:
		  0x00000300
		  0x00000301
		  0x00000302
		  0x00000303"
		*/
		if (num_zero == 2
		        && buffer_src[i] == 0x03
		        && i+1 < nal_size /*next byte is readable*/
		        && buffer_src[i+1] < 0x04)
		{
			/*emulation code found*/
			num_zero = 0;
			emulation_bytes_count++;
			i++;
		}

		buffer_dst[i-emulation_bytes_count] = buffer_src[i];

		if (!buffer_src[i])
			num_zero++;
		else
			num_zero = 0;

		i++;
	}

	return nal_size-emulation_bytes_count;
}

GF_EXPORT
s32 gf_media_avc_read_sps(const char *sps_data, u32 sps_size, AVCState *avc, u32 subseq_sps, u32 *vui_flag_pos)
{
	AVC_SPS *sps;
	u32 ChromaArrayType = 0;
	s32 mb_width, mb_height, sps_id = -1;
	u32 profile_idc, level_idc, pcomp, i, chroma_format_idc, cl, cr, ct, cb, luma_bd, chroma_bd;
	u8 separate_colour_plane_flag;
	GF_BitStream *bs;
	char *sps_data_without_emulation_bytes = NULL;
	u32 sps_data_without_emulation_bytes_size = 0;

	/*SPS still contains emulation bytes*/
	sps_data_without_emulation_bytes = gf_malloc(sps_size*sizeof(char));
	sps_data_without_emulation_bytes_size = avc_remove_emulation_bytes(sps_data, sps_data_without_emulation_bytes, sps_size);
	bs = gf_bs_new(sps_data_without_emulation_bytes, sps_data_without_emulation_bytes_size, GF_BITSTREAM_READ);
	if (!bs) {
		sps_id = -1;
		goto exit;
	}
	if (vui_flag_pos) *vui_flag_pos = 0;

	/*nal hdr*/ gf_bs_read_int(bs, 8);

	profile_idc = gf_bs_read_int(bs, 8);

	pcomp = gf_bs_read_int(bs, 8);
	/*sanity checks*/
	if (pcomp & 0x3)
		goto exit;

	level_idc = gf_bs_read_int(bs, 8);

	/*SubsetSps is used to be sure that AVC SPS are not going to be scratched
	by subset SPS. According to the SVC standard, subset SPS can have the same sps_id
	than its base layer, but it does not refer to the same SPS. */
	sps_id = bs_get_ue(bs) + GF_SVC_SSPS_ID_SHIFT * subseq_sps;
	if (sps_id >=32) {
		sps_id = -1;
		goto exit;
	}

	chroma_format_idc = luma_bd = chroma_bd = 0;
	sps = &avc->sps[sps_id];
	sps->state |= subseq_sps ? AVC_SUBSPS_PARSED : AVC_SPS_PARSED;

	/*High Profile and SVC*/
	switch (profile_idc) {
	case 100:
	case 110:
	case 122:
	case 244:
	case 44:
		/*sanity checks: note1 from 7.4.2.1.1 of iso/iec 14496-10-N11084*/
		if (pcomp & 0xE0)
			goto exit;
	case 83:
	case 86:
	case 118:
	case 128:
		chroma_format_idc = bs_get_ue(bs);
		ChromaArrayType = chroma_format_idc;
		if (chroma_format_idc == 3) {
			separate_colour_plane_flag = gf_bs_read_int(bs, 1);
			/*
			Depending on the value of separate_colour_plane_flag, the value of the variable ChromaArrayType is assigned as follows.
			\96	If separate_colour_plane_flag is equal to 0, ChromaArrayType is set equal to chroma_format_idc.
			\96	Otherwise (separate_colour_plane_flag is equal to 1), ChromaArrayType is set equal to 0.
			*/
			if (separate_colour_plane_flag) ChromaArrayType = 0;
		}
		luma_bd = bs_get_ue(bs);
		chroma_bd = bs_get_ue(bs);
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
							s32 delta = bs_get_se(bs);
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
	sps->log2_max_frame_num = bs_get_ue(bs) + 4;
	sps->poc_type = bs_get_ue(bs);
	sps->chroma_format = chroma_format_idc;
	sps->luma_bit_depth_m8 = luma_bd;
	sps->chroma_bit_depth_m8 = chroma_bd;

	if (sps->poc_type == 0) {
		sps->log2_max_poc_lsb = bs_get_ue(bs) + 4;
	} else if(sps->poc_type == 1) {
		sps->delta_pic_order_always_zero_flag = gf_bs_read_int(bs, 1);
		sps->offset_for_non_ref_pic = bs_get_se(bs);
		sps->offset_for_top_to_bottom_field = bs_get_se(bs);
		sps->poc_cycle_length = bs_get_ue(bs);
		for(i=0; i<sps->poc_cycle_length; i++) sps->offset_for_ref_frame[i] = bs_get_se(bs);
	}
	if (sps->poc_type > 2) {
		sps_id = -1;
		goto exit;
	}
	bs_get_ue(bs); /*ref_frame_count*/
	gf_bs_read_int(bs, 1); /*gaps_in_frame_num_allowed_flag*/
	mb_width = bs_get_ue(bs) + 1;
	mb_height= bs_get_ue(bs) + 1;

	sps->frame_mbs_only_flag = gf_bs_read_int(bs, 1);

	sps->width = mb_width * 16;
	sps->height = (2-sps->frame_mbs_only_flag) * mb_height * 16;

	/*mb_adaptive_frame_field_flag*/
	if (!sps->frame_mbs_only_flag) gf_bs_read_int(bs, 1);
	gf_bs_read_int(bs, 1); /*direct_8x8_inference_flag*/
	cl = cr = ct = cb = 0;
	if (gf_bs_read_int(bs, 1)) { /*crop*/
		int CropUnitX, CropUnitY, SubWidthC = -1, SubHeightC = -1;

		if (chroma_format_idc == 1) {
			SubWidthC = 2, SubHeightC = 2;
		} else if (chroma_format_idc == 2) {
			SubWidthC = 2, SubHeightC = 1;
		} else if ((chroma_format_idc == 3) && (separate_colour_plane_flag == 0)) {
			SubWidthC = 1, SubHeightC = 1;
		}

		if (ChromaArrayType == 0) {
			assert(SubWidthC==-1);
			CropUnitX = 1;
			CropUnitY = 2-sps->frame_mbs_only_flag;
		} else {
			CropUnitX = SubWidthC;
			CropUnitY = SubHeightC * (2-sps->frame_mbs_only_flag);
		}

		cl = bs_get_ue(bs); /*crop_left*/
		cr = bs_get_ue(bs); /*crop_right*/
		ct = bs_get_ue(bs); /*crop_top*/
		cb = bs_get_ue(bs); /*crop_bottom*/

		sps->width = 16*mb_width - CropUnitX * (cl + cr);
		sps->height -= (2-sps->frame_mbs_only_flag) * CropUnitY * (ct + cb);
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
				sps->vui.par_num = gf_bs_read_int(bs, 16); /*AR num*/
				sps->vui.par_den = gf_bs_read_int(bs, 16); /*AR den*/
			} else if (aspect_ratio_idc<14) {
				sps->vui.par_num = avc_sar[aspect_ratio_idc].w;
				sps->vui.par_den = avc_sar[aspect_ratio_idc].h;
			}
		}
		if(gf_bs_read_int(bs, 1))		/* overscan_info_present_flag */
			gf_bs_read_int(bs, 1);		/* overscan_appropriate_flag */

		if (gf_bs_read_int(bs, 1)) {		/* video_signal_type_present_flag */
			gf_bs_read_int(bs, 3);		/* video_format */
			gf_bs_read_int(bs, 1);		/* video_full_range_flag */
			if (gf_bs_read_int(bs, 1)) { /* colour_description_present_flag */
				gf_bs_read_int(bs, 8);  /* colour_primaries */
				gf_bs_read_int(bs, 8);  /* transfer_characteristics */
				gf_bs_read_int(bs, 8);  /* matrix_coefficients */
			}
		}

		if (gf_bs_read_int(bs, 1)) {	/* chroma_location_info_present_flag */
			bs_get_ue(bs);				/* chroma_sample_location_type_top_field */
			bs_get_ue(bs);				/* chroma_sample_location_type_bottom_field */
		}

		sps->vui.timing_info_present_flag = gf_bs_read_int(bs, 1);
		if (sps->vui.timing_info_present_flag) {
			sps->vui.num_units_in_tick = gf_bs_read_int(bs, 32);
			sps->vui.time_scale = gf_bs_read_int(bs, 32);
			sps->vui.fixed_frame_rate_flag = gf_bs_read_int(bs, 1);
		}

		sps->vui.nal_hrd_parameters_present_flag = gf_bs_read_int(bs, 1);
		if (sps->vui.nal_hrd_parameters_present_flag)
			avc_parse_hrd_parameters(bs, &sps->vui.hrd);

		sps->vui.vcl_hrd_parameters_present_flag = gf_bs_read_int(bs, 1);
		if (sps->vui.vcl_hrd_parameters_present_flag)
			avc_parse_hrd_parameters(bs, &sps->vui.hrd);

		if (sps->vui.nal_hrd_parameters_present_flag || sps->vui.vcl_hrd_parameters_present_flag)
			gf_bs_read_int(bs, 1); /*low_delay_hrd_flag*/

		sps->vui.pic_struct_present_flag = gf_bs_read_int(bs, 1);
	}
	/*end of seq_parameter_set_data*/

	if (subseq_sps) {
		if ((profile_idc==83) || (profile_idc==86)) {
			u8 extended_spatial_scalability_idc;
			/*parsing seq_parameter_set_svc_extension*/

			/*inter_layer_deblocking_filter_control_present_flag=*/	gf_bs_read_int(bs, 1);
			extended_spatial_scalability_idc = gf_bs_read_int(bs, 2);
			if (ChromaArrayType == 1 || ChromaArrayType == 2) {
				/*chroma_phase_x_plus1_flag*/ gf_bs_read_int(bs, 1);
			}
			if( ChromaArrayType  ==  1 ) {
				/*chroma_phase_y_plus1*/ gf_bs_read_int(bs, 2);
			}
			if (extended_spatial_scalability_idc == 1) {
				if( ChromaArrayType > 0 ) {
					/*seq_ref_layer_chroma_phase_x_plus1_flag*/gf_bs_read_int(bs, 1);
					/*seq_ref_layer_chroma_phase_y_plus1*/gf_bs_read_int(bs, 2);
				}
				/*seq_scaled_ref_layer_left_offset*/ bs_get_se(bs);
				/*seq_scaled_ref_layer_top_offset*/bs_get_se(bs);
				/*seq_scaled_ref_layer_right_offset*/bs_get_se(bs);
				/*seq_scaled_ref_layer_bottom_offset*/bs_get_se(bs);
			}
			if (/*seq_tcoeff_level_prediction_flag*/gf_bs_read_int(bs, 1)) {
				/*adaptive_tcoeff_level_prediction_flag*/ gf_bs_read_int(bs, 1);
			}
			/*slice_header_restriction_flag*/gf_bs_read_int(bs, 1);

			/*svc_vui_parameters_present*/
			if (gf_bs_read_int(bs, 1)) {
				u32 i, vui_ext_num_entries_minus1;
				vui_ext_num_entries_minus1 = bs_get_ue(bs);

				for (i=0; i <= vui_ext_num_entries_minus1; i++) {
					u8 vui_ext_nal_hrd_parameters_present_flag, vui_ext_vcl_hrd_parameters_present_flag, vui_ext_timing_info_present_flag;
					/*u8 vui_ext_dependency_id =*/ gf_bs_read_int(bs, 3);
					/*u8 vui_ext_quality_id =*/ gf_bs_read_int(bs, 4);
					/*u8 vui_ext_temporal_id =*/ gf_bs_read_int(bs, 3);
					vui_ext_timing_info_present_flag = gf_bs_read_int(bs, 1);
					if (vui_ext_timing_info_present_flag) {
						/*u32 vui_ext_num_units_in_tick = */gf_bs_read_int(bs, 32);
						/*u32 vui_ext_time_scale = */gf_bs_read_int(bs, 32);
						/*u8 vui_ext_fixed_frame_rate_flag = */gf_bs_read_int(bs, 1);
					}
					vui_ext_nal_hrd_parameters_present_flag = gf_bs_read_int(bs, 1);
					if (vui_ext_nal_hrd_parameters_present_flag) {
						//hrd_parameters( )
					}
					vui_ext_vcl_hrd_parameters_present_flag = gf_bs_read_int(bs, 1);
					if (vui_ext_vcl_hrd_parameters_present_flag) {
						//hrd_parameters( )
					}
					if ( vui_ext_nal_hrd_parameters_present_flag ||  vui_ext_vcl_hrd_parameters_present_flag) {
						/*vui_ext_low_delay_hrd_flag*/gf_bs_read_int(bs, 1);
					}
					/*vui_ext_pic_struct_present_flag*/gf_bs_read_int(bs, 1);
				}
			}
		}
		else if ((profile_idc==118) || (profile_idc==128)) {
			GF_LOG(GF_LOG_WARNING, GF_LOG_CODING, ("[avc-h264] MVC not supported - skipping parsing end of Subset SPS\n"));
			goto exit;
		}

		if (gf_bs_read_int(bs, 1)) {
			GF_LOG(GF_LOG_WARNING, GF_LOG_CODING, ("[avc-h264] skipping parsing end of Subset SPS (additional_extension2)\n"));
			goto exit;
		}
	}

exit:
	gf_bs_del(bs);
	gf_free(sps_data_without_emulation_bytes);
	return sps_id;
}

GF_EXPORT
s32 gf_media_avc_read_pps(const char *pps_data, u32 pps_size, AVCState *avc)
{
	GF_BitStream *bs;
	char *pps_data_without_emulation_bytes = NULL;
	u32 pps_data_without_emulation_bytes_size = 0;
	s32 pps_id;
	AVC_PPS *pps;

	/*PPS still contains emulation bytes*/
	pps_data_without_emulation_bytes = gf_malloc(pps_size*sizeof(char));
	pps_data_without_emulation_bytes_size = avc_remove_emulation_bytes(pps_data, pps_data_without_emulation_bytes, pps_size);
	bs = gf_bs_new(pps_data_without_emulation_bytes, pps_data_without_emulation_bytes_size, GF_BITSTREAM_READ);
	if (!bs) {
		pps_id = -1;
		goto exit;
	}
	/*nal hdr*/gf_bs_read_u8(bs);


	pps_id = bs_get_ue(bs);
	if (pps_id>=255) {
		pps_id = -1;
		goto exit;
	}
	pps = &avc->pps[pps_id];

	if (!pps->status) pps->status = 1;
	pps->sps_id = bs_get_ue(bs);
	if (pps->sps_id >= 32) {
		pps->sps_id = 0;
		pps_id = -1;
		goto exit;
	}
	/*sps_id may be refer to regular SPS or subseq sps, depending on the coded slice refering to the pps*/
	if (!avc->sps[pps->sps_id].state && !avc->sps[pps->sps_id + GF_SVC_SSPS_ID_SHIFT].state) {
		pps_id = -1;
		goto exit;
	}
	avc->sps_active_idx = pps->sps_id; /*set active sps*/
	/*pps->cabac = */gf_bs_read_int(bs, 1);
	pps->pic_order_present= gf_bs_read_int(bs, 1);
	pps->slice_group_count= bs_get_ue(bs) + 1;
	if (pps->slice_group_count > 1 ) /*pps->mb_slice_group_map_type = */bs_get_ue(bs);
	/*pps->ref_count[0]= */bs_get_ue(bs) /*+ 1*/;
	/*pps->ref_count[1]= */bs_get_ue(bs) /*+ 1*/;
	/*
	if ((pps->ref_count[0] > 32) || (pps->ref_count[1] > 32)) goto exit;
	*/

	/*pps->weighted_pred = */gf_bs_read_int(bs, 1);
	/*pps->weighted_bipred_idc = */gf_bs_read_int(bs, 2);
	/*pps->init_qp = */bs_get_se(bs) /*+ 26*/;
	/*pps->init_qs= */bs_get_se(bs) /*+ 26*/;
	/*pps->chroma_qp_index_offset = */bs_get_se(bs);
	/*pps->deblocking_filter_parameters_present = */gf_bs_read_int(bs, 1);
	/*pps->constrained_intra_pred = */gf_bs_read_int(bs, 1);
	pps->redundant_pic_cnt_present = gf_bs_read_int(bs, 1);

exit:
	gf_bs_del(bs);
	gf_free(pps_data_without_emulation_bytes);
	return pps_id;
}

s32 gf_media_avc_read_sps_ext(const char *spse_data, u32 spse_size)
{
	GF_BitStream *bs;
	char *spse_data_without_emulation_bytes = NULL;
	u32 spse_data_without_emulation_bytes_size = 0;
	s32 sps_id;

	/*PPS still contains emulation bytes*/
	spse_data_without_emulation_bytes = gf_malloc(spse_size*sizeof(char));
	spse_data_without_emulation_bytes_size = avc_remove_emulation_bytes(spse_data, spse_data_without_emulation_bytes, spse_size);
	bs = gf_bs_new(spse_data_without_emulation_bytes, spse_data_without_emulation_bytes_size, GF_BITSTREAM_READ);

	/*nal header*/gf_bs_read_u8(bs);

	sps_id = bs_get_ue(bs);

	gf_bs_del(bs);
	gf_free(spse_data_without_emulation_bytes);
	return sps_id;
}

static s32 SVC_ReadNal_header_extension(GF_BitStream *bs, SVC_NALUHeader *NalHeader)
{
	gf_bs_read_int(bs, 1); //reserved_one_bits
	NalHeader->idr_pic_flag = gf_bs_read_int(bs, 1); //idr_flag
	NalHeader->priority_id = gf_bs_read_int(bs, 6); //priority_id
	gf_bs_read_int(bs, 1); //no_inter_layer_pred_flag
	NalHeader->dependency_id = gf_bs_read_int(bs, 3); //DependencyId
	NalHeader->quality_id = gf_bs_read_int(bs, 4); //quality_id
	NalHeader->temporal_id = gf_bs_read_int(bs, 3); //temporal_id
	gf_bs_read_int(bs, 1); //use_ref_base_pic_flag
	gf_bs_read_int(bs, 1); //discardable_flag
	gf_bs_read_int(bs, 1); //output_flag
	gf_bs_read_int(bs, 2); //reserved_three_2bits
	return 1;
}

static s32 avc_parse_slice(GF_BitStream *bs, AVCState *avc, Bool svc_idr_flag, AVCSliceInfo *si)
{
	s32 pps_id;

	/*s->current_picture.reference= h->nal_ref_idc != 0;*/
	/*first_mb_in_slice = */bs_get_ue(bs);
	si->slice_type = bs_get_ue(bs);
	if (si->slice_type > 9) return -1;

	pps_id = bs_get_ue(bs);
	if (pps_id>255) return -1;
	si->pps = &avc->pps[pps_id];
	if (!si->pps->slice_group_count) return -2;
	si->sps = &avc->sps[si->pps->sps_id];
	if (!si->sps->log2_max_frame_num) return -2;

	si->frame_num = gf_bs_read_int(bs, si->sps->log2_max_frame_num);

	si->field_pic_flag = 0;
	si->bottom_field_flag = 0;
	if (!si->sps->frame_mbs_only_flag) {
		si->field_pic_flag = gf_bs_read_int(bs, 1);
		if (si->field_pic_flag)
			si->bottom_field_flag = gf_bs_read_int(bs, 1);
	}
	if ((si->nal_unit_type==GF_AVC_NALU_IDR_SLICE) || svc_idr_flag)
		si->idr_pic_id = bs_get_ue(bs);

	if (si->sps->poc_type==0) {
		si->poc_lsb = gf_bs_read_int(bs, si->sps->log2_max_poc_lsb);
		if (si->pps->pic_order_present && !si->field_pic_flag) {
			si->delta_poc_bottom = bs_get_se(bs);
		}
	} else if ((si->sps->poc_type==1) && !si->sps->delta_pic_order_always_zero_flag) {
		si->delta_poc[0] = bs_get_se(bs);
		if ((si->pps->pic_order_present==1) && !si->field_pic_flag)
			si->delta_poc[1] = bs_get_se(bs);
	}
	if (si->pps->redundant_pic_cnt_present) {
		si->redundant_pic_cnt = bs_get_ue(bs);
	}
	return 0;
}


static s32 svc_parse_slice(GF_BitStream *bs, AVCState *avc, AVCSliceInfo *si)
{
	s32 pps_id;

	/*s->current_picture.reference= h->nal_ref_idc != 0;*/
	/*first_mb_in_slice = */bs_get_ue(bs);
	si->slice_type = bs_get_ue(bs);
	if (si->slice_type > 9) return -1;

	pps_id = bs_get_ue(bs);
	if (pps_id>255)
		return -1;
	si->pps = &avc->pps[pps_id];
	si->pps->id = pps_id;
	if (!si->pps->slice_group_count)
		return -2;
	si->sps = &avc->sps[si->pps->sps_id + GF_SVC_SSPS_ID_SHIFT];
	if (!si->sps->log2_max_frame_num)
		return -2;

	si->frame_num = gf_bs_read_int(bs, si->sps->log2_max_frame_num);

	si->field_pic_flag = 0;
	if (si->sps->frame_mbs_only_flag) {
		/*s->picture_structure= PICT_FRAME;*/
	} else {
		si->field_pic_flag = gf_bs_read_int(bs, 1);
		if (si->field_pic_flag) si->bottom_field_flag = gf_bs_read_int(bs, 1);
	}
	if (si->nal_unit_type == GF_AVC_NALU_IDR_SLICE || si ->NalHeader.idr_pic_flag)
		si->idr_pic_id = bs_get_ue(bs);

	if (si->sps->poc_type==0) {
		si->poc_lsb = gf_bs_read_int(bs, si->sps->log2_max_poc_lsb);
		if (si->pps->pic_order_present && !si->field_pic_flag) {
			si->delta_poc_bottom = bs_get_se(bs);
		}
	} else if ((si->sps->poc_type==1) && !si->sps->delta_pic_order_always_zero_flag) {
		si->delta_poc[0] = bs_get_se(bs);
		if ((si->pps->pic_order_present==1) && !si->field_pic_flag)
			si->delta_poc[1] = bs_get_se(bs);
	}
	if (si->pps->redundant_pic_cnt_present) {
		si->redundant_pic_cnt = bs_get_ue(bs);
	}
	return 0;
}


static s32 avc_parse_recovery_point_sei(GF_BitStream *bs, AVCState *avc)
{
	AVCSeiRecoveryPoint *rp = &avc->sei.recovery_point;

	rp->frame_cnt = bs_get_ue(bs);
	rp->exact_match_flag = gf_bs_read_int(bs, 1);
	rp->broken_link_flag = gf_bs_read_int(bs, 1);
	rp->changing_slice_group_idc = gf_bs_read_int(bs, 2);
	rp->valid = 1;

	return 0;
}

/*for interpretation see ISO 14496-10 N.11084, table D-1*/
static s32 avc_parse_pic_timing_sei(GF_BitStream *bs, AVCState *avc)
{
	int i;
	int sps_id = avc->sps_active_idx;
	const char NumClockTS[] = {1, 1, 1, 2, 2, 3, 3, 2, 3};
	AVCSeiPicTiming *pt = &avc->sei.pic_timing;

	if (sps_id < 0) {
		/*sps_active_idx equals -1 when no sps has been detected. In this case SEI should not be decoded.*/
		assert(0);
		return 1;
	}
	if (avc->sps[sps_id].vui.nal_hrd_parameters_present_flag || avc->sps[sps_id].vui.vcl_hrd_parameters_present_flag) { /*CpbDpbDelaysPresentFlag, see 14496-10(2003) E.11*/
		gf_bs_read_int(bs, 1+avc->sps[sps_id].vui.hrd.cpb_removal_delay_length_minus1); /*cpb_removal_delay*/
		gf_bs_read_int(bs, 1+avc->sps[sps_id].vui.hrd.dpb_output_delay_length_minus1);  /*dpb_output_delay*/
	}

	/*ISO 14496-10 (2003), D.8.2: we need to get pic_struct in order to know if we display top field first or bottom field first*/
	if (avc->sps[sps_id].vui.pic_struct_present_flag) {
		pt->pic_struct = gf_bs_read_int(bs, 4);
		if (pt->pic_struct > 8) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_CODING, ("[avc-h264] invalid pic_struct value %d\n", pt->pic_struct));
			return 1;
		}

		for (i=0; i<NumClockTS[pt->pic_struct]; i++) {
			if (gf_bs_read_int(bs, 1)) {/*clock_timestamp_flag[i]*/
				Bool full_timestamp_flag;
				gf_bs_read_int(bs, 2);						/*ct_type*/
				gf_bs_read_int(bs, 1);						/*nuit_field_based_flag*/
				gf_bs_read_int(bs, 5);						/*counting_type*/
				full_timestamp_flag = gf_bs_read_int(bs, 1);/*full_timestamp_flag*/
				gf_bs_read_int(bs, 1);						/*discontinuity_flag*/
				gf_bs_read_int(bs, 1);						/*cnt_dropped_flag*/
				gf_bs_read_int(bs, 8);						/*n_frames*/
				if (full_timestamp_flag) {
					gf_bs_read_int(bs, 6);					/*seconds_value*/
					gf_bs_read_int(bs, 6);					/*minutes_value*/
					gf_bs_read_int(bs, 5);					/*hours_value*/
				} else {
					if (gf_bs_read_int(bs, 1)) {			/*seconds_flag*/
						gf_bs_read_int(bs, 6);				/*seconds_value*/
						if (gf_bs_read_int(bs, 1)) {		/*minutes_flag*/
							gf_bs_read_int(bs, 6);			/*minutes_value*/
							if (gf_bs_read_int(bs, 1)) {	/*hours_flag*/
								gf_bs_read_int(bs, 5);		/*hours_value*/
							}
						}
					}
					if (avc->sps[sps_id].vui.hrd.time_offset_length > 0)
						gf_bs_read_int(bs, avc->sps[sps_id].vui.hrd.time_offset_length);	/*time_offset*/
				}
			}
		}
	}

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

	/*ISO 14496-10 N.11084 8.2.1.1*/
	if (si->sps->poc_type==0)
	{
		const u32 max_poc_lsb = 1 << (si->sps->log2_max_poc_lsb);

		/*ISO 14496-10 N.11084 eq (8-3)*/
		if ((si->poc_lsb < si->poc_lsb_prev) &&
		        (si->poc_lsb_prev - si->poc_lsb >= max_poc_lsb / 2) )
			si->poc_msb = si->poc_msb_prev + max_poc_lsb;
		else if ((si->poc_lsb > si->poc_lsb_prev) &&
		         (si->poc_lsb - si->poc_lsb_prev > max_poc_lsb / 2))
			si->poc_msb = si->poc_msb_prev - max_poc_lsb;
		else
			si->poc_msb = si->poc_msb_prev;

		/*ISO 14496-10 N.11084 eq (8-4)*/
		if (pic_type != AVC_PIC_FIELD_BOTTOM)
			field_poc[0] = si->poc_msb + si->poc_lsb;

		/*ISO 14496-10 N.11084 eq (8-5)*/
		if (pic_type != AVC_PIC_FIELD_TOP) {
			if (!si->field_pic_flag)
				field_poc[1] = field_poc[0] + si->delta_poc_bottom;
			else
				field_poc[1] = si->poc_msb + si->poc_lsb;
		}
	}
	/*ISO 14496-10 N.11084 8.2.1.2*/
	else if (si->sps->poc_type==1)
	{
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
	}
	/*ISO 14496-10 N.11084 8.2.1.3*/
	else if (si->sps->poc_type== 2)
	{
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

	/*ISO 14496-10 N.11084 eq (8-1)*/
	if (pic_type == AVC_PIC_FRAME)
		si->poc = MIN(field_poc[0], field_poc[1] );
	else if (pic_type == AVC_PIC_FIELD_TOP)
		si->poc = field_poc[0];
	else
		si->poc = field_poc[1];
}

GF_EXPORT
s32 gf_media_avc_parse_nalu(GF_BitStream *bs, u32 nal_hdr, AVCState *avc)
{
	u8 idr_flag;
	s32 slice, ret;
	AVCSliceInfo n_state;

	slice = 0;
	memcpy(&n_state, &avc->s_info, sizeof(AVCSliceInfo));
	n_state.nal_unit_type = nal_hdr & 0x1F;
	n_state.nal_ref_idc = (nal_hdr>>5) & 0x3;

	idr_flag = 0;
	ret = 0;
	switch (n_state.nal_unit_type) {
	case GF_AVC_NALU_ACCESS_UNIT:
	case GF_AVC_NALU_END_OF_SEQ:
	case GF_AVC_NALU_END_OF_STREAM:
		ret = 1;
		break;

	case GF_AVC_NALU_SVC_SLICE:
		SVC_ReadNal_header_extension(bs, &n_state.NalHeader);
		slice = 1;
		// slice buffer - read the info and compare.
		ret = svc_parse_slice(bs, avc, &n_state);
		if (avc->s_info.nal_ref_idc) {
			n_state.poc_lsb_prev = avc->s_info.poc_lsb;
			n_state.poc_msb_prev = avc->s_info.poc_msb;
		}
		if (slice)
			avc_compute_poc(&n_state);

		if (avc->s_info.poc != n_state.poc) {
			memcpy(&avc -> s_info, &n_state, sizeof(AVCSliceInfo));
			return 1;
		}
		memcpy(&avc -> s_info, &n_state, sizeof(AVCSliceInfo));
		return 0;

	case GF_AVC_NALU_SVC_PREFIX_NALU:
		SVC_ReadNal_header_extension(bs, &n_state.NalHeader);
		return 0;

	case GF_AVC_NALU_NON_IDR_SLICE:
	case GF_AVC_NALU_DP_A_SLICE:
	case GF_AVC_NALU_DP_B_SLICE:
	case GF_AVC_NALU_DP_C_SLICE:
	case GF_AVC_NALU_IDR_SLICE:
		slice = 1;
		/* slice buffer - read the info and compare.*/
		ret = avc_parse_slice(bs, avc, idr_flag, &n_state);
		if (ret<0) return ret;
		ret = 0;
		if (
		    ((avc->s_info.nal_unit_type > GF_AVC_NALU_IDR_SLICE) || (avc->s_info.nal_unit_type < GF_AVC_NALU_NON_IDR_SLICE))
		    && (avc->s_info.nal_unit_type != GF_AVC_NALU_SVC_SLICE)
		) {
			break;
		}
		if (avc->s_info.frame_num != n_state.frame_num) {
			ret = 1;
			break;
		}

		if (avc->s_info.field_pic_flag != n_state.field_pic_flag) {
			ret = 1;
			break;
		}
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

		if (n_state.nal_unit_type == GF_AVC_NALU_IDR_SLICE)  {
			if (avc->s_info.nal_unit_type != GF_AVC_NALU_IDR_SLICE) { /*IdrPicFlag differs in value*/
				ret = 1;
				break;
			}
			else if (avc->s_info.idr_pic_id != n_state.idr_pic_id) { /*both IDR and idr_pic_id differs*/
				ret = 1;
				break;
			}
		}
		break;
	case GF_AVC_NALU_SEQ_PARAM:
	case GF_AVC_NALU_PIC_PARAM:
	case GF_AVC_NALU_SVC_SUBSEQ_PARAM:
	case GF_AVC_NALU_FILLER_DATA:
		return 0;
	default:
		if (avc->s_info.nal_unit_type <= GF_AVC_NALU_IDR_SLICE) ret = 1;
		//To detect change of AU when multiple sps and pps in stream
		else if ((nal_hdr & 0x1F) == GF_AVC_NALU_SEI && avc -> s_info .nal_unit_type == GF_AVC_NALU_SVC_SLICE)
			ret = 1;
		else if ((nal_hdr & 0x1F) == GF_AVC_NALU_SEQ_PARAM && avc -> s_info .nal_unit_type == GF_AVC_NALU_SVC_SLICE)
			ret = 1;
		else
			ret = 0;
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

u32 gf_media_avc_reformat_sei(char *buffer, u32 nal_size, AVCState *avc)
{
	u32 ptype, psize, hdr, written, var;
	u64 start;
	char *new_buffer;
	GF_BitStream *bs;
	char *sei_without_emulation_bytes = NULL;
	u32 sei_without_emulation_bytes_size = 0;

	hdr = buffer[0];
	if ((hdr & 0x1F) != GF_AVC_NALU_SEI) return 0;

	/*PPS still contains emulation bytes*/
	sei_without_emulation_bytes = gf_malloc(nal_size + 1/*for SEI null string termination*/);
	sei_without_emulation_bytes_size = avc_remove_emulation_bytes(buffer, sei_without_emulation_bytes, nal_size);

	bs = gf_bs_new(sei_without_emulation_bytes, sei_without_emulation_bytes_size, GF_BITSTREAM_READ);
	gf_bs_read_int(bs, 8);

	new_buffer = (char*)gf_malloc(sizeof(char)*nal_size);
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

		start = gf_bs_get_position(bs);

		do_copy = 1;

		if (start+psize >= nal_size) {
			if (written == 1) written = 0;
			GF_LOG(GF_LOG_WARNING, GF_LOG_CODING, ("[avc-h264] SEI user message type %d size error (%d but %d remain), skiping %sSEI message\n", ptype, psize, nal_size-start, written ? "end of " : ""));
			break;
		}

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
			char prev;
			prev = sei_without_emulation_bytes[start+psize+1];
			sei_without_emulation_bytes[start+psize+1] = 0;
			GF_LOG(GF_LOG_DEBUG, GF_LOG_CODING, ("[avc-h264] SEI user message %s\n", sei_without_emulation_bytes+start+16));
			sei_without_emulation_bytes[start+psize+1] = prev;
		}
		break;

		case 6: /*recovery point*/
		{
			GF_BitStream *rp_bs = gf_bs_new(sei_without_emulation_bytes + start, psize, GF_BITSTREAM_READ);
			avc_parse_recovery_point_sei(rp_bs, avc);
			gf_bs_del(rp_bs);
		}
		break;

		case 1: /*pic_timing*/
		{
			GF_BitStream *pt_bs = gf_bs_new(sei_without_emulation_bytes + start, psize, GF_BITSTREAM_READ);
			avc_parse_pic_timing_sei(pt_bs, avc);
			gf_bs_del(pt_bs);
		}
		break;

		case 0: /*buffering period*/
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
			break;
		default: /*reserved*/
			do_copy = 0;
			break;
		}

		if (do_copy) {
			var = ptype;
			while (var>=255) {
				new_buffer[written] = (char) 0xff;
				written++;
				var-=255;
			}
			new_buffer[written] = (char) var;
			written++;
			var = psize;
			while (var>=255) {
				new_buffer[written] = (char) 0xff;
				written++;
				var-=255;
			}
			new_buffer[written] = (char) var;
			written++;
			memcpy(new_buffer+written, sei_without_emulation_bytes+start, sizeof(char) * psize);
			written += psize;
		}

		gf_bs_skip_bytes(bs, (u64) psize);
		gf_bs_align(bs);
		if (gf_bs_available(bs)<=2) {
			if (gf_bs_peek_bits(bs, 8, 0)==0x80) {
				new_buffer[written] = (char) 0x80;
				written += 1;
			}
			break;
		}
	}
	gf_bs_del(bs);
	gf_free(sei_without_emulation_bytes);

	if (written) {
		var = avc_emulation_bytes_add_count(new_buffer, written);
		if (var) {
			if (written+var<=nal_size) {
				written = avc_add_emulation_bytes(new_buffer, buffer, written);
			} else {
				written = 0;
			}
		} else {
			if (written<=nal_size) {
				memcpy(buffer, new_buffer, sizeof(char)*written);
			} else {
				written = 0;
			}
		}
	}
	gf_free(new_buffer);

	/*if only hdr written ignore*/
	return (written>1) ? written : 0;
}

#ifndef GPAC_DISABLE_ISOM

static u8 avc_get_sar_idx(u32 w, u32 h)
{
	u32 i;
	for (i=0; i<14; i++) {
		if ((avc_sar[i].w==w) && (avc_sar[i].h==h)) return i;
	}
	return 0xFF;
}

GF_Err gf_media_avc_change_par(GF_AVCConfig *avcc, s32 ar_n, s32 ar_d)
{
	GF_BitStream *orig, *mod;
	AVCState avc;
	u32 i, bit_offset, flag;
	s32 idx;
	GF_AVCConfigSlot *slc;
	orig = NULL;

	memset(&avc, 0, sizeof(AVCState));
	avc.sps_active_idx = -1;

	i=0;
	while ((slc = (GF_AVCConfigSlot *)gf_list_enum(avcc->sequenceParameterSets, &i))) {
		char *no_emulation_buf = NULL;
		u32 no_emulation_buf_size = 0, emulation_bytes = 0;
		idx = gf_media_avc_read_sps(slc->data, slc->size, &avc, 0, &bit_offset);
		if (idx<0) {
			if ( orig )
				gf_bs_del(orig);
			continue;
		}

		/*SPS still contains emulation bytes*/
		no_emulation_buf = gf_malloc((slc->size-1)*sizeof(char));
		no_emulation_buf_size = avc_remove_emulation_bytes(slc->data+1, no_emulation_buf, slc->size-1);

		orig = gf_bs_new(no_emulation_buf, no_emulation_buf_size, GF_BITSTREAM_READ);
		gf_bs_read_data(orig, no_emulation_buf, no_emulation_buf_size);
		gf_bs_seek(orig, 0);
		mod = gf_bs_new(NULL, 0, GF_BITSTREAM_WRITE);

		/*copy over till vui flag*/
		assert(bit_offset>=8);
		while (bit_offset-8/*bit_offset doesn't take care of the first byte (NALU type)*/) {
			flag = gf_bs_read_int(orig, 1);
			gf_bs_write_int(mod, flag, 1);
			bit_offset--;
		}
		/*check VUI*/
		flag = gf_bs_read_int(orig, 1);
		gf_bs_write_int(mod, 1, 1); /*vui_parameters_present_flag*/
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
			gf_bs_write_int(mod, 0, 1);		/*overscan_info_present_flag */
			gf_bs_write_int(mod, 0, 1);		/*video_signal_type_present_flag */
			gf_bs_write_int(mod, 0, 1);		/*chroma_location_info_present_flag */
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
		orig = NULL;
		gf_free(no_emulation_buf);

		/*set anti-emulation*/
		gf_bs_get_content(mod, (char **) &no_emulation_buf, &flag);
		emulation_bytes = avc_emulation_bytes_add_count(no_emulation_buf, flag);
		if (flag+emulation_bytes+1>slc->size)
			slc->data = (char*)gf_realloc(slc->data, flag+emulation_bytes+1);
		slc->size = avc_add_emulation_bytes(no_emulation_buf, slc->data+1, flag)+1;

		gf_bs_del(mod);
		gf_free(no_emulation_buf);
	}
	return GF_OK;
}
#endif

GF_EXPORT
GF_Err gf_avc_get_sps_info(char *sps_data, u32 sps_size, u32 *sps_id, u32 *width, u32 *height, s32 *par_n, s32 *par_d)
{
	AVCState avc;
	s32 idx;
	memset(&avc, 0, sizeof(AVCState));
	avc.sps_active_idx = -1;

	idx = gf_media_avc_read_sps(sps_data, sps_size, &avc, 0, NULL);
	if (idx<0) {
		return GF_NON_COMPLIANT_BITSTREAM;
	}
	if (sps_id) *sps_id = idx;

	if (width) *width = avc.sps[idx].width;
	if (height) *height = avc.sps[idx].height;
	if (par_n) *par_n = avc.sps[idx].vui.par_num ? avc.sps[idx].vui.par_num : (u32) -1;
	if (par_d) *par_d = avc.sps[idx].vui.par_den ? avc.sps[idx].vui.par_den : (u32) -1;

	return GF_OK;
}

GF_EXPORT
GF_Err gf_avc_get_pps_info(char *pps_data, u32 pps_size, u32 *pps_id, u32 *sps_id)
{
	GF_BitStream *bs;
	char *pps_data_without_emulation_bytes = NULL;
	u32 pps_data_without_emulation_bytes_size = 0;
	GF_Err e = GF_OK;

	/*PPS still contains emulation bytes*/
	pps_data_without_emulation_bytes = gf_malloc(pps_size*sizeof(char));
	pps_data_without_emulation_bytes_size = avc_remove_emulation_bytes(pps_data, pps_data_without_emulation_bytes, pps_size);
	bs = gf_bs_new(pps_data_without_emulation_bytes, pps_data_without_emulation_bytes_size, GF_BITSTREAM_READ);
	if (!bs) {
		e = GF_NON_COMPLIANT_BITSTREAM;
		goto exit;
	}
	/*nal hdr*/ gf_bs_read_int(bs, 8);

	*pps_id = bs_get_ue(bs);
	*sps_id = bs_get_ue(bs);

exit:
	gf_bs_del(bs);
	gf_free(pps_data_without_emulation_bytes);
	return e;
}



#ifndef GPAC_DISABLE_HEVC

/**********
HEVC parsing
**********/

Bool gf_media_hevc_slice_is_intra(HEVCState *hevc)
{
	switch (hevc->s_info.nal_unit_type) {
	case GF_HEVC_NALU_SLICE_BLA_W_LP:
	case GF_HEVC_NALU_SLICE_BLA_W_DLP:
	case GF_HEVC_NALU_SLICE_BLA_N_LP:
	case GF_HEVC_NALU_SLICE_IDR_W_DLP:
	case GF_HEVC_NALU_SLICE_IDR_N_LP:
	case GF_HEVC_NALU_SLICE_CRA:
		return GF_TRUE;
	default:
		return GF_FALSE;
	}
}

Bool gf_media_hevc_slice_is_IDR(HEVCState *hevc)
{
	if (hevc->sei.recovery_point.valid)
	{
		hevc->sei.recovery_point.valid = 0;
		return GF_TRUE;
	}
	switch (hevc->s_info.nal_unit_type) {
	case GF_HEVC_NALU_SLICE_IDR_W_DLP:
	case GF_HEVC_NALU_SLICE_IDR_N_LP:
		return GF_TRUE;
	default:
		return GF_FALSE;
	}
}

static Bool parse_short_term_ref_pic_set(GF_BitStream *bs, HEVC_SPS *sps, u32 idx_rps)
{
	u32 i;
	Bool inter_ref_pic_set_prediction_flag = 0;
	if (idx_rps != 0)
		inter_ref_pic_set_prediction_flag = gf_bs_read_int(bs, 1);

	if (inter_ref_pic_set_prediction_flag ) {
		HEVC_ReferencePictureSets *ref_ps, *rps;
		u32 delta_idx_minus1 = 0;
		u32 ref_idx;
		u32 delta_rps_sign;
		u32 abs_delta_rps_minus1, nb_ref_pics;
		s32 deltaRPS;
		u32 k = 0, k0 = 0, k1 = 0;
		if (idx_rps == sps->num_short_term_ref_pic_sets)
			delta_idx_minus1 = bs_get_ue(bs);

		assert(delta_idx_minus1 <= idx_rps - 1);
		ref_idx = idx_rps - 1 - delta_idx_minus1;
		delta_rps_sign = gf_bs_read_int(bs, 1);
		abs_delta_rps_minus1 = bs_get_ue(bs);
		deltaRPS = (1 - (delta_rps_sign<<1)) * (abs_delta_rps_minus1 + 1);

		rps = &sps->rps[idx_rps];
		ref_ps = &sps->rps[ref_idx];
		nb_ref_pics = ref_ps->num_negative_pics + ref_ps->num_positive_pics;
		for (i=0; i<=nb_ref_pics; i++) {
			s32 ref_idc;
			s32 used_by_curr_pic_flag = gf_bs_read_int(bs, 1);
			ref_idc = used_by_curr_pic_flag ? 1 : 0;
			if ( !used_by_curr_pic_flag ) {
				used_by_curr_pic_flag = gf_bs_read_int(bs, 1);
				ref_idc = used_by_curr_pic_flag << 1;
			}
			if ((ref_idc==1) || (ref_idc== 2)) {
				s32 deltaPOC = deltaRPS;
				if (i < nb_ref_pics)
					deltaPOC += ref_ps->delta_poc[i];

				rps->delta_poc[k] = deltaPOC;

				if (deltaPOC < 0)  k0++;
				else k1++;

				k++;
			}
		}
		rps->num_negative_pics = k0;
		rps->num_positive_pics = k1;
	} else {
		s32 prev = 0, poc = 0;
		sps->rps[idx_rps].num_negative_pics = bs_get_ue(bs);
		sps->rps[idx_rps].num_positive_pics = bs_get_ue(bs);
		for (i=0; i<sps->rps[idx_rps].num_negative_pics; i++) {
			u32 delta_poc_s0_minus1 = bs_get_ue(bs);
			poc = prev - delta_poc_s0_minus1 - 1;
			prev = poc;
			sps->rps[idx_rps].delta_poc[i] = poc;
			/*used_by_curr_pic_s1_flag[ i ] = */gf_bs_read_int(bs, 1);
		}
		for (i=0; i<sps->rps[idx_rps].num_positive_pics; i++) {
			u32 delta_poc_s1_minus1 = bs_get_ue(bs);
			poc = prev + delta_poc_s1_minus1 + 1;
			prev = poc;
			sps->rps[idx_rps].delta_poc[i] = poc;
			/*used_by_curr_pic_s1_flag[ i ] = */gf_bs_read_int(bs, 1);
		}
	}
	return 1;
}

#define PARSE_FULL_HEADER	0
s32 hevc_parse_slice_segment(GF_BitStream *bs, HEVCState *hevc, HEVCSliceInfo *si)
{
#if PARSE_FULL_HEADER
	u32 i, j;
#endif
	HEVC_PPS *pps;
	HEVC_SPS *sps;
	s32 pps_id;
	Bool RapPicFlag = GF_FALSE;
	Bool IDRPicFlag = GF_FALSE;

	si->first_slice_segment_in_pic_flag = gf_bs_read_int(bs, 1);

	switch (si->nal_unit_type) {
	case GF_HEVC_NALU_SLICE_IDR_W_DLP:
	case GF_HEVC_NALU_SLICE_IDR_N_LP:
		IDRPicFlag = GF_TRUE;
		RapPicFlag = GF_TRUE;
		break;
	case GF_HEVC_NALU_SLICE_BLA_W_LP:
	case GF_HEVC_NALU_SLICE_BLA_W_DLP:
	case GF_HEVC_NALU_SLICE_BLA_N_LP:
	case GF_HEVC_NALU_SLICE_CRA:
		RapPicFlag = GF_TRUE;
		break;
	}

	if (RapPicFlag) {
		/*Bool no_output_of_prior_pics_flag = */gf_bs_read_int(bs, 1);
	}

	pps_id = bs_get_ue(bs);
	if (pps_id>=64) return -1;

	pps = &hevc->pps[pps_id];
	sps = &hevc->sps[pps->sps_id];
	si->sps = sps;
	si->pps = pps;

	if (!si->first_slice_segment_in_pic_flag && pps->dependent_slice_segments_enabled_flag) {
		si->dependent_slice_segment_flag = gf_bs_read_int(bs, 1);
	} else {
		si->dependent_slice_segment_flag = GF_FALSE;
	}

	if (!si->first_slice_segment_in_pic_flag) {
		si->slice_segment_address = gf_bs_read_int(bs, sps->bitsSliceSegmentAddress);
	} else {
		si->slice_segment_address = 0;
	}

	if( !si->dependent_slice_segment_flag ) {
#if PARSE_FULL_HEADER
		Bool deblocking_filter_override_flag=0;
		Bool slice_temporal_mvp_enabled_flag = 0;
		Bool slice_sao_luma_flag=0;
		Bool slice_sao_chroma_flag=0;
		Bool slice_deblocking_filter_disabled_flag=0;
#endif

		//"slice_reserved_undetermined_flag[]"
		gf_bs_read_int(bs, pps->num_extra_slice_header_bits);

		si->slice_type = bs_get_ue(bs);

		if(pps->output_flag_present_flag)
			/*pic_output_flag = */gf_bs_read_int(bs, 1);

		if (sps->separate_colour_plane_flag == 1)
			/*colour_plane_id = */gf_bs_read_int(bs, 2);

		if (IDRPicFlag) {
			si->poc_lsb = 0;
		} else {
			si->poc_lsb = gf_bs_read_int(bs, sps->log2_max_pic_order_cnt_lsb);

#if !PARSE_FULL_HEADER
		}
	}
#else
			if (/*short_term_ref_pic_set_sps_flag =*/gf_bs_read_int(bs, 1) == 0) {
				Bool ret = parse_short_term_ref_pic_set(bs, sps, sps->num_short_term_ref_pic_sets );
				if (!ret) return 0;
			} else if( sps->num_short_term_ref_pic_sets > 1 ) {
				u32 numbits = 0;
				s32 short_term_ref_pic_set_idx;
				while ( (u32) (1 << numbits) < sps->num_short_term_ref_pic_sets)
					numbits++;
				if (numbits > 0)
					short_term_ref_pic_set_idx = gf_bs_read_int(bs, numbits);
				else
					short_term_ref_pic_set_idx = 0;
			}
			if (sps->long_term_ref_pics_present_flag ) {
				u8 DeltaPocMsbCycleLt[32];
				u32 num_long_term_sps = 0;
				u32 num_long_term_pics = 0;
				if (sps->num_long_term_ref_pic_sps > 0 ) {
					num_long_term_sps = bs_get_ue(bs);
				}
				num_long_term_pics = bs_get_ue(bs);

				for (i = 0; i < num_long_term_sps + num_long_term_pics; i++ ) {
					if( i < num_long_term_sps ) {
						u8 lt_idx_sps = 0;
						if (sps->num_long_term_ref_pic_sps > 1)
							lt_idx_sps = gf_bs_read_int(bs, gf_get_bit_size(sps->num_long_term_ref_pic_sps) );
					} else {
						/*PocLsbLt[ i ] = */ gf_bs_read_int(bs, sps->log2_max_pic_order_cnt_lsb);
						/*UsedByCurrPicLt[ i ] = */ gf_bs_read_int(bs, 1);
					}
					if (/*delta_poc_msb_present_flag[ i ] = */ gf_bs_read_int(bs, 1) ) {
						if( i == 0 || i == num_long_term_sps )
							DeltaPocMsbCycleLt[i] = bs_get_ue(bs);
						else
							DeltaPocMsbCycleLt[i] = bs_get_ue(bs) + DeltaPocMsbCycleLt[i-1];
					}
				}
			}
			if (sps->temporal_mvp_enable_flag)
				slice_temporal_mvp_enabled_flag = gf_bs_read_int(bs, 1);
		}
		if (sps->sample_adaptive_offset_enabled_flag) {
			slice_sao_luma_flag = gf_bs_read_int(bs, 1);
			slice_sao_chroma_flag = gf_bs_read_int(bs, 1);
		}

		if (si->slice_type == GF_HEVC_TYPE_P || si->slice_type == GF_HEVC_TYPE_B) {
			//u32 NumPocTotalCurr;
			u32 num_ref_idx_l0_active, num_ref_idx_l1_active;

			num_ref_idx_l0_active = pps->num_ref_idx_l0_default_active;
			num_ref_idx_l1_active = 0;
			if (si->slice_type == GF_HEVC_TYPE_B)
				num_ref_idx_l1_active = pps->num_ref_idx_l1_default_active;

			if ( /*num_ref_idx_active_override_flag =*/gf_bs_read_int(bs, 1) ) {
				num_ref_idx_l0_active = 1 + bs_get_ue(bs);
				if (si->slice_type == GF_HEVC_TYPE_B)
					num_ref_idx_l1_active = 1 + bs_get_ue(bs);
			}

//            if (pps->lists_modification_present_flag && NumPocTotalCurr > 1) {
			if (pps->lists_modification_present_flag ) {
				GF_LOG(GF_LOG_INFO, GF_LOG_CODING, ("[hehv] ref_pic_lists_modification( ) not implemented\n"));
				return 0;
			}

			if (si->slice_type == GF_HEVC_TYPE_B)
				/*mvd_l1_zero_flag=*/gf_bs_read_int(bs, 1);
			if (pps->cabac_init_present_flag)
				/*cabac_init_flag=*/gf_bs_read_int(bs, 1);

			if (slice_temporal_mvp_enabled_flag) {
				Bool collocated_from_l0_flag = 0;
				if (si->slice_type == GF_HEVC_TYPE_B)
					collocated_from_l0_flag = gf_bs_read_int(bs, 1);

				if ( (collocated_from_l0_flag && num_ref_idx_l0_active-1 > 0 )
				        || ( !collocated_from_l0_flag && num_ref_idx_l1_active-1 > 0 )
				   ) {
					/*collocated_ref_idx=*/bs_get_ue(bs);
				}
			}

			if ( (pps->weighted_pred_flag && si->slice_type == GF_HEVC_TYPE_P )
			        || ( pps->weighted_bipred_flag && si->slice_type == GF_HEVC_TYPE_B)
			   ) {
				GF_LOG(GF_LOG_INFO, GF_LOG_CODING, ("[hehv] pred_weight_table not implemented\n"));
				return 0;
			}
			/*five_minus_max_num_merge_cand=*/bs_get_ue(bs);
		}
		/*slice_qp_delta = */bs_get_se(bs);
		if( pps->slice_chroma_qp_offsets_present_flag ) {
			/*slice_cb_qp_offset=*/bs_get_se(bs);
			/*slice_cr_qp_offset=*/bs_get_se(bs);
		}
		if ( pps->deblocking_filter_override_enabled_flag ) {
			deblocking_filter_override_flag = gf_bs_read_int(bs, 1);
		}

		if (deblocking_filter_override_flag) {
			slice_deblocking_filter_disabled_flag = gf_bs_read_int(bs, 1);
			if ( !slice_deblocking_filter_disabled_flag) {
				/*slice_beta_offset_div2=*/ bs_get_se(bs);
				/*slice_tc_offset_div2=*/bs_get_se(bs);
			}
		}
		if( pps->loop_filter_across_slices_enabled_flag
		        && ( slice_sao_luma_flag || slice_sao_chroma_flag || !slice_deblocking_filter_disabled_flag )
		  ) {
			/*slice_loop_filter_across_slices_enabled_flag = */gf_bs_read_int(bs, 1);
		}
	}


	if (pps->tiles_enabled_flag || pps->entropy_coding_sync_enabled_flag ) {
		u32 num_entry_point_offsets = bs_get_ue(bs);
		if ( num_entry_point_offsets > 0) {
			u32 offset = bs_get_ue(bs) + 1;
			u32 segments = offset >> 4;
			s32 remain = (offset & 15);

			for (i=0; i<num_entry_point_offsets; i++) {
				u32 res = 0;
				for (j=0; j<segments; j++) {
					res <<= 16;
					res += gf_bs_read_int(bs, 16);
				}
				if (remain) {
					res <<= remain;
					res += gf_bs_read_int(bs, remain);
				}
				// entry_point_offset = val + 1; // +1; // +1 to get the size
			}
		}
	}
#endif //PARSE_FULL_HEADER
	return 0;
}

static void hevc_compute_poc(HEVCSliceInfo *si)
{
	u32 max_poc_lsb = 1 << (si->sps->log2_max_pic_order_cnt_lsb);

	/*POC reset for IDR frames, NOT for CRA*/
	switch (si->nal_unit_type) {
	case GF_HEVC_NALU_SLICE_IDR_W_DLP:
	case GF_HEVC_NALU_SLICE_IDR_N_LP:
		si->poc_lsb_prev = 0;
		si->poc_msb_prev = 0;
		break;
	}

	if ((si->poc_lsb < si->poc_lsb_prev) && (	si->poc_lsb_prev - si->poc_lsb >= max_poc_lsb / 2) )
		si->poc_msb = si->poc_msb_prev + max_poc_lsb;
	else if ((si->poc_lsb > si->poc_lsb_prev) && (si->poc_lsb - si->poc_lsb_prev > max_poc_lsb / 2))
		si->poc_msb = si->poc_msb_prev - max_poc_lsb;
	else
		si->poc_msb = si->poc_msb_prev;

	switch (si->nal_unit_type) {
	case GF_HEVC_NALU_SLICE_BLA_W_LP:
	case GF_HEVC_NALU_SLICE_BLA_W_DLP:
	case GF_HEVC_NALU_SLICE_BLA_N_LP:
		si->poc_msb  = 0;
		break;
	}
	si->poc = si->poc_msb + si->poc_lsb;
}

void profile_tier_level(GF_BitStream *bs, Bool ProfilePresentFlag, u8 MaxNumSubLayersMinus1, HEVC_ProfileTierLevel *ptl)
{
	u32 i;
	if (ProfilePresentFlag) {
		ptl->profile_space = gf_bs_read_int(bs, 2);
		ptl->tier_flag = gf_bs_read_int(bs, 1);
		ptl->profile_idc = gf_bs_read_int(bs, 5);

		ptl->profile_compatibility_flag = gf_bs_read_int(bs, 32);

		ptl->general_progressive_source_flag = gf_bs_read_int(bs, 1);
		ptl->general_interlaced_source_flag = gf_bs_read_int(bs, 1);
		ptl->general_non_packed_constraint_flag = gf_bs_read_int(bs, 1);
		ptl->general_frame_only_constraint_flag = gf_bs_read_int(bs, 1);
		ptl->general_reserved_44bits = gf_bs_read_long_int(bs, 44);
	}
	ptl->level_idc = gf_bs_read_int(bs, 8);
	for (i=0; i<MaxNumSubLayersMinus1; i++) {
		ptl->sub_ptl[i].profile_present_flag = gf_bs_read_int(bs, 1);
		ptl->sub_ptl[i].level_present_flag = gf_bs_read_int(bs, 1);
	}
	if (MaxNumSubLayersMinus1>0) {
		for (i=MaxNumSubLayersMinus1; i<8; i++) {
			/*reserved_zero_2bits*/gf_bs_read_int(bs, 2);
		}
	}

	for (i=0; i<MaxNumSubLayersMinus1; i++) {
		if (ProfilePresentFlag && ptl->sub_ptl[i].profile_present_flag) {
			ptl->sub_ptl[i].profile_space = gf_bs_read_int(bs, 2);
			ptl->sub_ptl[i].tier_flag = gf_bs_read_int(bs, 1);
			ptl->sub_ptl[i].profile_idc = gf_bs_read_int(bs, 5);
			ptl->sub_ptl[i].profile_compatibility_flag = gf_bs_read_int(bs, 32);
			/*sub_layer_reserved_zero_16bits*/gf_bs_read_int(bs, 16);
		}
		if (ptl->sub_ptl[i].level_present_flag)
			ptl->sub_ptl[i].level_idc = gf_bs_read_int(bs, 8);
	}
}

static u32 scalability_type_to_idx(HEVC_VPS *vps, u32 scalability_type)
{
	u32 idx = 0, type;
	for (type=0; type < scalability_type; type++) {
		idx += (vps->scalability_mask[type] ? 1 : 0 );
	}
	return idx;
}

#define SHVC_VIEW_ORDER_INDEX  1
#define SHVC_SCALABILITY_INDEX	2

static u32 shvc_get_scalability_id(HEVC_VPS *vps, u32 layer_id_in_vps, u32 scalability_type )
{
	u32 idx;
	if (!vps->scalability_mask[scalability_type]) return 0;
	idx = scalability_type_to_idx(vps, scalability_type);
	return vps->dimension_id[layer_id_in_vps][idx];
}

static u32 shvc_get_view_index(HEVC_VPS *vps, u32 id)
{
	return shvc_get_scalability_id(vps, vps->layer_id_in_vps[id], SHVC_VIEW_ORDER_INDEX);
}

static u32 shvc_get_num_views(HEVC_VPS *vps)
{
	u32 numViews = 1, i;
	for (i=0; i<vps->max_layers; i++ ) {
		u32 layer_id = vps->layer_id_in_nuh[i];
		if (i>0 && ( shvc_get_view_index( vps, layer_id) != shvc_get_scalability_id( vps, i-1, SHVC_VIEW_ORDER_INDEX) )) {
			numViews++;
		}
	}
	return numViews;
}

static void shvc_parse_rep_format(HEVC_RepFormat *fmt, GF_BitStream *bs)
{
	u8 chroma_bitdepth_present_flag = gf_bs_read_int(bs, 1);
	fmt->pic_width_luma_samples = gf_bs_read_int(bs, 16);
	fmt->pic_height_luma_samples = gf_bs_read_int(bs, 16);
	if (chroma_bitdepth_present_flag) {
		fmt->chroma_format_idc = gf_bs_read_int(bs, 2);

		if (fmt->chroma_format_idc == 3)
			fmt->separate_colour_plane_flag = gf_bs_read_int(bs, 1);
		fmt->bit_depth_luma = 1 + gf_bs_read_int(bs, 4);
		fmt->bit_depth_chroma = 1 + gf_bs_read_int(bs, 4);
	}
}

static void hevc_parse_vps_extension(HEVC_VPS *vps, GF_BitStream *bs)
{
	u8 splitting_flag, vps_nuh_layer_id_present_flag, view_id_len;
	u32 i, j, NumScalabilityTypes, num_profile_tier_level, num_add_output_layer_sets, NumOutputLayerSets;
	u8 dimension_id_len[62];
	u8 direct_dependency_flag[62][62];
	u8 /*avc_base_layer_flag, */vps_number_layer_sets, /*default_one_target_output_layer_flag, */rep_format_idx_present_flag;

	/*avc_base_layer_flag = */gf_bs_read_int(bs, 1);
	splitting_flag = gf_bs_read_int(bs, 1);
	NumScalabilityTypes =0;
	for (i=0; i<16; i++) {
		vps->scalability_mask[i] = gf_bs_read_int(bs, 1);
		NumScalabilityTypes += vps->scalability_mask[i];
	}
	dimension_id_len[0] = 0;
	for (i=0; i<(NumScalabilityTypes - splitting_flag); i++) {
		dimension_id_len[i] = 1 + gf_bs_read_int(bs, 3);
	}

	vps->layer_id_in_nuh[0] = 0;
	vps->layer_id_in_vps[0] = 0;
	vps_nuh_layer_id_present_flag = gf_bs_read_int(bs, 1);
	for (i=1; i<vps->max_layers; i++) {
		if (vps_nuh_layer_id_present_flag) {
			vps->layer_id_in_nuh[i] = gf_bs_read_int(bs, 6);
		} else {
			vps->layer_id_in_nuh[i] = i;
		}
		vps->layer_id_in_vps[vps->layer_id_in_nuh[i]] = i;

		if (!splitting_flag) {
			for (j=0; j<NumScalabilityTypes; j++) {
				vps->dimension_id[i][j] = gf_bs_read_int(bs, dimension_id_len[j]);
			}
		}
	}

	view_id_len = gf_bs_read_int(bs, 4);
	for( i = 0; i < shvc_get_num_views(vps); i++ ) {
		/*m_viewIdVal[i] = */ gf_bs_read_int(bs, view_id_len + 1);
	}

	for (i=1; i<vps->max_layers; i++) {
		for (j=0; j<i; j++) {
			direct_dependency_flag[i][j] = gf_bs_read_int(bs, 1);
		}
	}

	if (/*vps_sub_layers_max_minus1_present_flag*/gf_bs_read_int(bs, 1)) {
		for (i=0; i < vps->max_layers - 1; i++) {
			/*sub_layers_vps_max_minus1[ i ]*/gf_bs_read_int(bs, 3);
		}
	}

	if (/*max_tid_ref_present_flag = */gf_bs_read_int(bs, 1)) {
		for (i=0; i<vps->max_layers ; i++) {
			for (j= i+1; j < vps->max_layers; j++) {
				if (direct_dependency_flag[j][i])
					/*max_tid_il_ref_pics_plus1[ i ][ j ]*/gf_bs_read_int(bs, 3);
			}
		}
	}
	/*all_ref_layers_active_flag*/gf_bs_read_int(bs, 1);

	vps_number_layer_sets = 1+gf_bs_read_int(bs, 10);
	num_profile_tier_level = 1+gf_bs_read_int(bs, 6);

	for (i=1; i < num_profile_tier_level; i++) {
		Bool vps_profile_present_flag = gf_bs_read_int(bs, 1);
		if (!vps_profile_present_flag) {
			/*vps->profile_ref[i] = */gf_bs_read_int(bs, 6);
		}
		profile_tier_level(bs, vps_profile_present_flag, vps->max_sub_layers-1, &vps->ext_ptl[i-1] );
	}
	NumOutputLayerSets = vps_number_layer_sets;
	if (/*more_output_layer_sets_than_default_flag */gf_bs_read_int(bs, 1)) {
		num_add_output_layer_sets = gf_bs_read_int(bs, 10)+1;
		NumOutputLayerSets += num_add_output_layer_sets;
	}

	/*default_one_target_output_layer_flag = 0;*/
	if (NumOutputLayerSets > 1) {
		/*default_one_target_output_layer_flag = */gf_bs_read_int(bs, 1);
	}
	vps->profile_level_tier_idx[0] = 0;
	for (i=1; i<NumOutputLayerSets; i++) {
		u32 nb_bits;
		//don't warn untiol we fix the entire syntax to last version of the spec
		if( i > vps->num_layer_sets - 1) {
//			GF_LOG(GF_LOG_ERROR, GF_LOG_CODING, ("[HEVC] VPS Extensions: not supported number of layers\n"));
		}
		nb_bits = 1;
		while ((u32) (1 << nb_bits) < num_profile_tier_level) {
			nb_bits++;
		}
		vps->profile_level_tier_idx[i] = gf_bs_read_int(bs, nb_bits);
	}

	if (vps->max_layers - 1 > 0 )
		/*alt_output_layer_flag*/gf_bs_read_int(bs, 1);

	rep_format_idx_present_flag = gf_bs_read_int(bs, 1);
	if (rep_format_idx_present_flag ) {
		vps->num_rep_formats = 1 + gf_bs_read_int(bs, 8);
	} else {
		vps->num_rep_formats = vps->max_layers;
	}
	for (i=0; i<vps->num_rep_formats; i++) {
		shvc_parse_rep_format(&vps->rep_formats[i], bs);
	}
	vps->rep_format_idx[0] = 0;
	for (i=1; i<vps->max_layers; i++) {
		if (rep_format_idx_present_flag) {
			if (vps->num_rep_formats > 1) {
				vps->rep_format_idx[i] = gf_bs_read_int(bs, 8);
			} else {
				vps->rep_format_idx[i] = 0;
			}
		} else {
			vps->rep_format_idx[i] = i;
		}
	}
	//TODO - we don't use the rest ...

}

GF_EXPORT
s32 gf_media_hevc_read_vps(char *data, u32 size, HEVCState *hevc)
{
	GF_BitStream *bs;
	u8 vps_sub_layer_ordering_info_present_flag, vps_extension_flag;
	char *data_without_emulation_bytes = NULL;
	u32 data_without_emulation_bytes_size = 0;
	u32 i, j;
	s32 vps_id = -1;
	HEVC_VPS *vps;

	/*still contains emulation bytes*/
	data_without_emulation_bytes = gf_malloc(size*sizeof(char));
	data_without_emulation_bytes_size = avc_remove_emulation_bytes(data, data_without_emulation_bytes, size);
	bs = gf_bs_new(data_without_emulation_bytes, data_without_emulation_bytes_size, GF_BITSTREAM_READ);
	if (!bs) goto exit;

	gf_bs_read_u16(bs);

	vps_id = gf_bs_read_int(bs, 4);

	if (vps_id>=16) goto exit;

	vps = &hevc->vps[vps_id];
	if (!vps->state) {
		vps->id = vps_id;
		vps->state = 1;
	}

	/* vps_reserved_three_2bits = */ gf_bs_read_int(bs, 2);
	vps->max_layers = 1 + gf_bs_read_int(bs, 6);
	vps->max_sub_layers = gf_bs_read_int(bs, 3) + 1;
	vps->temporal_id_nesting = gf_bs_read_int(bs, 1);
	/* vps_reserved_ffff_16bits = */ gf_bs_read_int(bs, 16);
	profile_tier_level(bs, 1, vps->max_sub_layers-1, &vps->ptl);

	vps_sub_layer_ordering_info_present_flag = gf_bs_read_int(bs, 1);
	for (i=(vps_sub_layer_ordering_info_present_flag ? 0 : vps->max_sub_layers - 1); i < vps->max_sub_layers; i++) {
		/*vps_max_dec_pic_buffering_minus1[i] = */bs_get_ue(bs);
		/*vps_max_num_reorder_pics[i] = */bs_get_ue(bs);
		/*vps_max_latency_increase_plus1[i] = */bs_get_ue(bs);
	}
	vps->max_layer_id = gf_bs_read_int(bs, 6);
	vps->num_layer_sets = bs_get_ue(bs) + 1;
	for (i=1; i < vps->num_layer_sets; i++) {
		for (j=0; j <= vps->max_layer_id; j++) {
			/*layer_id_included_flag[ i ][ j ]*/gf_bs_read_int(bs, 1);
		}
	}
	if (/*vps_timing_info_present_flag*/gf_bs_read_int(bs, 1)) {
		u32 vps_num_hrd_parameters;
		/*u32 vps_num_units_in_tick = */gf_bs_read_int(bs, 32);
		/*u32 vps_time_scale = */gf_bs_read_int(bs, 32);
		if (/*vps_poc_proportional_to_timing_flag*/gf_bs_read_int(bs, 1)) {
			/*vps_num_ticks_poc_diff_one_minus1*/bs_get_ue(bs);
		}
		vps_num_hrd_parameters = bs_get_ue(bs);
		for( i = 0; i < vps_num_hrd_parameters; i++ ) {
			//Bool cprms_present_flag=1;
			/*hrd_layer_set_idx[ i ] = */bs_get_ue(bs);
			if (i>0)
				/*cprms_present_flag = */gf_bs_read_int(bs, 1) ;
			// hevc_parse_hrd_parameters(cprms_present_flag, vps->max_sub_layers - 1);
		}
	}
	vps_extension_flag = gf_bs_read_int(bs, 1);
	if (vps_extension_flag ) {
		gf_bs_align(bs);
		hevc_parse_vps_extension(vps, bs);
		vps_extension_flag = gf_bs_read_int(bs, 1);
	}

exit:
	gf_bs_del(bs);
	gf_free(data_without_emulation_bytes);
	return vps_id;
}

static const struct {
	u32 w, h;
} hevc_sar[17] =
{
	{ 0,   0 }, { 1,   1 }, { 12, 11 }, { 10, 11 },
	{ 16, 11 }, { 40, 33 }, { 24, 11 }, { 20, 11 },
	{ 32, 11 }, { 80, 33 }, { 18, 11 }, { 15, 11 },
	{ 64, 33 }, { 160,99 }, { 4,3}, { 3,2}, { 2,1}
};

static s32 gf_media_hevc_read_sps_ex(char *data, u32 size, HEVCState *hevc, u32 *vui_flag_pos)
{
	GF_BitStream *bs;
	char *data_without_emulation_bytes = NULL;
	u32 data_without_emulation_bytes_size = 0;
	s32 vps_id, sps_id = -1;
	u8 max_sub_layers_minus1, update_rep_format_flag, flag;
	u8 layer_id/*, temporal_id, sps_rep_format_idx*/;
	Bool scaling_list_enable_flag;
	u32 i, nb_CTUs, depth;
	u32 log2_diff_max_min_luma_coding_block_size;
	u32 log2_min_transform_block_size, log2_min_luma_coding_block_size;
	Bool sps_sub_layer_ordering_info_present_flag;
	HEVC_SPS *sps;
	HEVC_VPS *vps;
	HEVC_ProfileTierLevel ptl;

	if (vui_flag_pos) *vui_flag_pos = 0;

	/*still contains emulation bytes*/
	data_without_emulation_bytes = gf_malloc(size*sizeof(char));
	data_without_emulation_bytes_size = avc_remove_emulation_bytes(data, data_without_emulation_bytes, size);
	bs = gf_bs_new(data_without_emulation_bytes, data_without_emulation_bytes_size, GF_BITSTREAM_READ);
	if (!bs) goto exit;

	gf_bs_read_int(bs, 7);
	layer_id = gf_bs_read_int(bs, 6);
	/*temporal_id = */gf_bs_read_int(bs, 3);
	vps_id = gf_bs_read_int(bs, 4);
	if (vps_id>=16) {
		sps_id = -1;
		goto exit;
	}

	memset(&ptl, 0, sizeof(ptl));
	max_sub_layers_minus1 = 0;
	if (layer_id == 0) {
		max_sub_layers_minus1 = gf_bs_read_int(bs, 3);
		/*temporal_id_nesting_flag = */gf_bs_read_int(bs, 1);
		profile_tier_level(bs, 1, max_sub_layers_minus1, &ptl);
	}

	sps_id = bs_get_ue(bs);
	if (sps_id>=16) {
		sps_id = -1;
		goto exit;
	}

	sps = &hevc->sps[sps_id];
	if (!sps->state) {
		sps->state = 1;
		sps->id = sps_id;
		sps->vps_id = vps_id;
	}
	sps->ptl = ptl;
	vps = &hevc->vps[vps_id];

	//sps_rep_format_idx = 0;
	update_rep_format_flag = 0;
	if (layer_id > 0) {
		update_rep_format_flag = gf_bs_read_int(bs, 1);
		if (update_rep_format_flag) {
			sps->rep_format_idx = gf_bs_read_int(bs, 8);
		} else {
			sps->rep_format_idx = vps->rep_format_idx[layer_id];
		}
		sps->width = vps->rep_formats[sps->rep_format_idx].pic_width_luma_samples;
		sps->height = vps->rep_formats[sps->rep_format_idx].pic_height_luma_samples;
		sps->chroma_format_idc = vps->rep_formats[sps->rep_format_idx].chroma_format_idc;
		sps->bit_depth_luma = vps->rep_formats[sps->rep_format_idx].bit_depth_luma;
		sps->bit_depth_chroma = vps->rep_formats[sps->rep_format_idx].bit_depth_chroma;
		sps->separate_colour_plane_flag = vps->rep_formats[sps->rep_format_idx].separate_colour_plane_flag;

		//TODO this is crude ...
		sps->ptl = vps->ext_ptl[0];
	} else {
		sps->chroma_format_idc = bs_get_ue(bs);
		if (sps->chroma_format_idc==3)
			sps->separate_colour_plane_flag = gf_bs_read_int(bs, 1);
		sps->width = bs_get_ue(bs);
		sps->height = bs_get_ue(bs);
	}

	if (gf_bs_read_int(bs, 1)) {
		u32 SubWidthC, SubHeightC;
		SubWidthC = SubHeightC = 1;
		if (sps->chroma_format_idc==1) {
			SubWidthC = SubHeightC = 2;
		}
		else if (sps->chroma_format_idc==2) {
			SubWidthC = 2;
			SubHeightC = 1;
		} else {
			SubWidthC = SubHeightC = 1;
		}

		sps->cw_left = bs_get_ue(bs);
		sps->cw_right = bs_get_ue(bs);
		sps->cw_top = bs_get_ue(bs);
		sps->cw_bottom = bs_get_ue(bs);

		sps->width -= SubWidthC * (sps->cw_left + sps->cw_right);
		sps->height -= SubHeightC * (sps->cw_top + sps->cw_bottom);
	}
	if (layer_id == 0) {
		sps->bit_depth_luma = 8 + bs_get_ue(bs);
		sps->bit_depth_chroma = 8 + bs_get_ue(bs);
	}

	sps->log2_max_pic_order_cnt_lsb = 4 + bs_get_ue(bs);

	if (layer_id == 0) {
		sps_sub_layer_ordering_info_present_flag = gf_bs_read_int(bs, 1);
		for(i=sps_sub_layer_ordering_info_present_flag ? 0 : max_sub_layers_minus1; i<=max_sub_layers_minus1; i++) {
			/*max_dec_pic_buffering = */ bs_get_ue(bs);
			/*num_reorder_pics = */ bs_get_ue(bs);
			/*max_latency_increase = */ bs_get_ue(bs);
		}
	}

	log2_min_luma_coding_block_size = 3 + bs_get_ue(bs);
	log2_diff_max_min_luma_coding_block_size = bs_get_ue(bs);
	sps->max_CU_width = ( 1<<(log2_min_luma_coding_block_size + log2_diff_max_min_luma_coding_block_size) );
	sps->max_CU_height = ( 1<<(log2_min_luma_coding_block_size + log2_diff_max_min_luma_coding_block_size) );

	log2_min_transform_block_size = 2 + bs_get_ue(bs);
	/*log2_max_transform_block_size = log2_min_transform_block_size  + */bs_get_ue(bs);

	depth = 0;
	/*u32 max_transform_hierarchy_depth_inter = */bs_get_ue(bs);
	/*u32 max_transform_hierarchy_depth_intra = */bs_get_ue(bs);
	while( (u32) ( sps->max_CU_width >> log2_diff_max_min_luma_coding_block_size ) > (u32) ( 1 << ( log2_min_transform_block_size + depth )  ) )
	{
		depth++;
	}
	sps->max_CU_depth = log2_diff_max_min_luma_coding_block_size + depth;

	nb_CTUs = ((sps->width + sps->max_CU_width -1) / sps->max_CU_width) * ((sps->height + sps->max_CU_height-1) / sps->max_CU_height);
	sps->bitsSliceSegmentAddress = 0;
	while (nb_CTUs > (u32) (1 << sps->bitsSliceSegmentAddress)) {
		sps->bitsSliceSegmentAddress++;
	}

	scaling_list_enable_flag = gf_bs_read_int(bs, 1);
	if (scaling_list_enable_flag) {
		Bool sps_infer_scaling_list_flag = 0;
		/*u8 sps_scaling_list_ref_layer_id = 0;*/
		if (layer_id>0) {
			sps_infer_scaling_list_flag = gf_bs_read_int(bs, 1);
		}

		if (sps_infer_scaling_list_flag) {
			/*sps_scaling_list_ref_layer_id = */gf_bs_read_int(bs, 6);
		} else {
			if (/*sps_scaling_list_data_present_flag=*/gf_bs_read_int(bs, 1) ) {
				//scaling_list_data( )
			}
		}
	}
	/*asymmetric_motion_partitions_enabled_flag= */ gf_bs_read_int(bs, 1);
	sps->sample_adaptive_offset_enabled_flag = gf_bs_read_int(bs, 1);
	if (/*pcm_enabled_flag= */ gf_bs_read_int(bs, 1) ) {
		/*pcm_sample_bit_depth_luma_minus1=*/gf_bs_read_int(bs, 4);
		/*pcm_sample_bit_depth_chroma_minus1=*/gf_bs_read_int(bs, 4);
		/*log2_min_pcm_luma_coding_block_size_minus3= */ bs_get_ue(bs);
		/*log2_diff_max_min_pcm_luma_coding_block_size = */ bs_get_ue(bs);
		/*pcm_loop_filter_disable_flag=*/gf_bs_read_int(bs, 1);
	}
	sps->num_short_term_ref_pic_sets = bs_get_ue(bs);
	for (i=0; i<sps->num_short_term_ref_pic_sets; i++) {
		Bool ret = parse_short_term_ref_pic_set(bs, sps, i);
		/*cannot parse short_term_ref_pic_set, skip VUI parsing*/
		if (!ret) goto exit;
	}
	if ( (sps->long_term_ref_pics_present_flag = gf_bs_read_int(bs, 1)) ) {
		sps->num_long_term_ref_pic_sps = bs_get_ue(bs);
		for (i=0; i<sps->num_long_term_ref_pic_sps; i++) {
			/*lt_ref_pic_poc_lsb_sps=*/gf_bs_read_int(bs, sps->log2_max_pic_order_cnt_lsb);
			/*used_by_curr_pic_lt_sps_flag*/gf_bs_read_int(bs, 1);
		}
	}
	sps->temporal_mvp_enable_flag = gf_bs_read_int(bs, 1);
	/*strong_intra_smoothing_enable_flag*/gf_bs_read_int(bs, 1);

	if (vui_flag_pos)
		*vui_flag_pos = (u32) gf_bs_get_bit_offset(bs);

	/*fixme - move to latest syntax*/
	if (layer_id>0) goto exit;

	if (/*vui_parameters_present_flag*/gf_bs_read_int(bs, 1)) {

		sps->aspect_ratio_info_present_flag = gf_bs_read_int(bs, 1);
		if (sps->aspect_ratio_info_present_flag) {
			sps->sar_idc = gf_bs_read_int(bs, 8);
			if (sps->sar_idc == 255) {
				sps->sar_width = gf_bs_read_int(bs, 16);
				sps->sar_height = gf_bs_read_int(bs, 16);
			} else if (sps->sar_idc<17) {
				sps->sar_width = hevc_sar[sps->sar_idc].w;
				sps->sar_height = hevc_sar[sps->sar_idc].h;
			}
		}

		if (/*overscan_info_present = */ gf_bs_read_int(bs, 1))
			/*overscan_appropriate = */ gf_bs_read_int(bs, 1);

		/*video_signal_type_present_flag = */flag = gf_bs_read_int(bs, 1);
		if (flag) {
			/*video_format = */gf_bs_read_int(bs, 3);
			/*video_full_range_flag = */gf_bs_read_int(bs, 1);
			if (/*colour_description_present_flag = */gf_bs_read_int(bs, 1)) {
				/*colour_primaries = */ gf_bs_read_int(bs, 8);
				/* transfer_characteristic = */ gf_bs_read_int(bs, 8);
				/* matrix_coeffs = */ gf_bs_read_int(bs, 8);
			}
		}

		if (/*chroma_loc_info_present_flag = */ gf_bs_read_int(bs, 1)) {
			/*chroma_sample_loc_type_top_field = */ bs_get_ue(bs);
			/*chroma_sample_loc_type_bottom_field = */bs_get_ue(bs);
		}

		/*neutra_chroma_indication_flag = */gf_bs_read_int(bs, 1);
		/*field_seq_flag = */gf_bs_read_int(bs, 1);
		/*frame_field_info_present_flag = */gf_bs_read_int(bs, 1);

		if (/*default_display_window_flag=*/gf_bs_read_int(bs, 1)) {
			/*left_offset = */bs_get_ue(bs);
			/*right_offset = */bs_get_ue(bs);
			/*top_offset = */bs_get_ue(bs);
			/*bottom_offset = */bs_get_ue(bs);
		}

		sps->has_timing_info = gf_bs_read_int(bs, 1);
		if (sps->has_timing_info ) {
			sps->num_units_in_tick = gf_bs_read_int(bs, 32);
			sps->time_scale = gf_bs_read_int(bs, 32);
			sps->poc_proportional_to_timing_flag = gf_bs_read_int(bs, 1);
			if (sps->poc_proportional_to_timing_flag)
				sps->num_ticks_poc_diff_one_minus1 = bs_get_ue(bs);
			if (/*hrd_parameters_present_flag=*/gf_bs_read_int(bs, 1) ) {
				goto exit;
//				GF_LOG(GF_LOG_INFO, GF_LOG_CODING, ("[HEVC] HRD param parsing not implemented\n"));
			}
		}

		if (/*bitstream_restriction_flag=*/gf_bs_read_int(bs, 1)) {
			/*tiles_fixed_structure_flag = */gf_bs_read_int(bs, 1);
			/*motion_vectors_over_pic_boundaries_flag = */gf_bs_read_int(bs, 1);
			/*restricted_ref_pic_lists_flag = */gf_bs_read_int(bs, 1);
			/*min_spatial_segmentation_idc = */bs_get_ue(bs);
			/*max_bytes_per_pic_denom = */bs_get_ue(bs);
			/*max_bits_per_min_cu_denom = */bs_get_ue(bs);
			/*log2_max_mv_length_horizontal = */bs_get_ue(bs);
			/*log2_max_mv_length_vertical = */bs_get_ue(bs);
		}
	}

	if (/*sps_extension_flag*/gf_bs_read_int(bs, 1)) {
		while (gf_bs_available(bs)) {
			/*sps_extension_data_flag */ gf_bs_read_int(bs, 1);
		}
	}

exit:
	gf_bs_del(bs);
	gf_free(data_without_emulation_bytes);
	return sps_id;
}


GF_EXPORT
s32 gf_media_hevc_read_sps(char *data, u32 size, HEVCState *hevc)
{
	return gf_media_hevc_read_sps_ex(data, size, hevc, NULL);
}


GF_EXPORT
s32 gf_media_hevc_read_pps(char *data, u32 size, HEVCState *hevc)
{
	u32 i;
	GF_BitStream *bs;
	char *data_without_emulation_bytes = NULL;
	u32 data_without_emulation_bytes_size = 0;
	s32 pps_id = -1;
	HEVC_PPS *pps;

	/*still contains emulation bytes*/
	data_without_emulation_bytes = gf_malloc(size*sizeof(char));
	data_without_emulation_bytes_size = avc_remove_emulation_bytes(data, data_without_emulation_bytes, size);
	bs = gf_bs_new(data_without_emulation_bytes, data_without_emulation_bytes_size, GF_BITSTREAM_READ);
	if (!bs) goto exit;

	gf_bs_read_u16(bs);

	pps_id = bs_get_ue(bs);

	if (pps_id>=64) goto exit;
	pps = &hevc->pps[pps_id];

	if (!pps->state) {
		pps->id = pps_id;
		pps->state = 1;
	}
	pps->sps_id = bs_get_ue(bs);
	hevc->sps_active_idx = pps->sps_id; /*set active sps*/
	pps->dependent_slice_segments_enabled_flag = gf_bs_read_int(bs, 1);

	pps->output_flag_present_flag = gf_bs_read_int(bs, 1);
	pps->num_extra_slice_header_bits = gf_bs_read_int(bs, 3);
	/*sign_data_hiding_flag = */gf_bs_read_int(bs, 1);
	pps->cabac_init_present_flag = gf_bs_read_int(bs, 1);
	pps->num_ref_idx_l0_default_active = 1 + bs_get_ue(bs);
	pps->num_ref_idx_l1_default_active = 1 + bs_get_ue(bs);
	/*pic_init_qp_minus26 = */bs_get_se(bs);
	/*constrained_intra_pred_flag = */gf_bs_read_int(bs, 1);
	/*transform_skip_enabled_flag = */gf_bs_read_int(bs, 1);
	if (/*cu_qp_delta_enabled_flag = */gf_bs_read_int(bs, 1) )
		/*diff_cu_qp_delta_depth = */bs_get_ue(bs);

	/*pic_cb_qp_offset = */bs_get_se(bs);
	/*pic_cr_qp_offset = */bs_get_se(bs);
	pps->slice_chroma_qp_offsets_present_flag = gf_bs_read_int(bs, 1);
	pps->weighted_pred_flag = gf_bs_read_int(bs, 1);
	pps->weighted_bipred_flag = gf_bs_read_int(bs, 1);
	/*transquant_bypass_enable_flag = */gf_bs_read_int(bs, 1);
	pps->tiles_enabled_flag = gf_bs_read_int(bs, 1);
	pps->entropy_coding_sync_enabled_flag = gf_bs_read_int(bs, 1);
	if (pps->tiles_enabled_flag) {
		pps->num_tile_columns = 1 + bs_get_ue(bs);
		pps->num_tile_rows = 1 + bs_get_ue(bs);
		pps->uniform_spacing_flag = gf_bs_read_int(bs, 1);
		if (!pps->uniform_spacing_flag ) {
			for (i=0; i<pps->num_tile_columns-1; i++) {
				pps->column_width[i] = 1 + bs_get_ue(bs);
			}
			for (i=0; i<pps->num_tile_rows-1; i++) {
				pps->row_height[i] = 1+bs_get_ue(bs);
			}
		}
		pps->loop_filter_across_tiles_enabled_flag = gf_bs_read_int(bs, 1);
	}
	pps->loop_filter_across_slices_enabled_flag = gf_bs_read_int(bs, 1);
	if( /*deblocking_filter_control_present_flag = */gf_bs_read_int(bs, 1)  ) {
		pps->deblocking_filter_override_enabled_flag = gf_bs_read_int(bs, 1);
		if (! /*pic_disable_deblocking_filter_flag= */gf_bs_read_int(bs, 1) ) {
			/*beta_offset_div2 = */bs_get_se(bs);
			/*tc_offset_div2 = */bs_get_se(bs);
		}
	}
	if (/*pic_scaling_list_data_present_flag	= */gf_bs_read_int(bs, 1) ) {
		//scaling_list_data( )
		GF_LOG(GF_LOG_WARNING, GF_LOG_CODING, ("[HEVC] Parsing of scaling_list_data is not yet supported, stopping scaning of PPS (slice_segment_header_extension_present won't be checked ...\n"));
		goto exit;
	}
	pps->lists_modification_present_flag = gf_bs_read_int(bs, 1);
	/*log2_parallel_merge_level_minus2 = */bs_get_ue(bs);
	pps->slice_segment_header_extension_present_flag = gf_bs_read_int(bs, 1);
	if ( /*pps_extension_flag= */gf_bs_read_int(bs, 1) ) {
		while (gf_bs_available(bs) ) {
			/*pps_extension_data_flag */ gf_bs_read_int(bs, 1);
		}
	}

exit:
	gf_bs_del(bs);
	gf_free(data_without_emulation_bytes);
	return pps_id;
}


GF_EXPORT
s32 gf_media_hevc_parse_nalu(GF_BitStream *bs, HEVCState *hevc, u8 *nal_unit_type, u8 *temporal_id, u8 *layer_id)
{
	u32 reserved;
	s32 slice, ret;
	HEVCSliceInfo n_state;

	slice = 0;
	memcpy(&n_state, &hevc->s_info, sizeof(HEVCSliceInfo));

	reserved = gf_bs_read_int(bs, 1);
	if (reserved) return -1;

	*nal_unit_type = n_state.nal_unit_type = gf_bs_read_int(bs, 6);
	*layer_id = gf_bs_read_int(bs, 6);
	*temporal_id = n_state.temporal_id = gf_bs_read_int(bs, 3);
	if (! (*temporal_id))
		return -1;

	*temporal_id -= 1;

	ret = 0;
	switch (n_state.nal_unit_type) {
	case GF_HEVC_NALU_ACCESS_UNIT:
	case GF_HEVC_NALU_END_OF_SEQ:
	case GF_HEVC_NALU_END_OF_STREAM:
		ret = 1;
		break;


	/*slice_layer_rbsp*/
//	case GF_HEVC_NALU_SLICE_STSA_N:
//	case GF_HEVC_NALU_SLICE_STSA_R:
//	case GF_HEVC_NALU_SLICE_RADL_N:
//	case GF_HEVC_NALU_SLICE_RADL_R:
//	case GF_HEVC_NALU_SLICE_RASL_N:
//	case GF_HEVC_NALU_SLICE_RASL_R:
//		break;

	/*slice_segment_layer_rbsp*/
	case GF_HEVC_NALU_SLICE_TRAIL_N:
	case GF_HEVC_NALU_SLICE_TRAIL_R:
	case GF_HEVC_NALU_SLICE_TSA_N:
	case GF_HEVC_NALU_SLICE_TSA_R:
	case GF_HEVC_NALU_SLICE_STSA_N:
	case GF_HEVC_NALU_SLICE_STSA_R:

	case GF_HEVC_NALU_SLICE_BLA_W_LP:
	case GF_HEVC_NALU_SLICE_BLA_W_DLP:
	case GF_HEVC_NALU_SLICE_BLA_N_LP:
	case GF_HEVC_NALU_SLICE_IDR_W_DLP:
	case GF_HEVC_NALU_SLICE_IDR_N_LP:
	case GF_HEVC_NALU_SLICE_CRA:
	case GF_HEVC_NALU_SLICE_RADL_N:
	case GF_HEVC_NALU_SLICE_RADL_R:
	case GF_HEVC_NALU_SLICE_RASL_N:
	case GF_HEVC_NALU_SLICE_RASL_R:
		slice = 1;
		//fixme with latest SHVC syntax
		if (*layer_id) return 0;

		/* slice - read the info and compare.*/
		ret = hevc_parse_slice_segment(bs, hevc, &n_state);
		if (ret<0) return ret;

		hevc_compute_poc(&n_state);

		ret = 0;

		if (hevc->s_info.poc != n_state.poc) {
			ret=1;
			break;
		}
		break;
	case GF_HEVC_NALU_SEQ_PARAM:
	case GF_HEVC_NALU_PIC_PARAM:
	case GF_HEVC_NALU_VID_PARAM:
		return 0;
	default:
		break;
	}

	/* save _prev values */
	if (ret && hevc->s_info.sps) {
		n_state.frame_num_offset_prev = hevc->s_info.frame_num_offset;
		n_state.frame_num_prev = hevc->s_info.frame_num;

		n_state.poc_lsb_prev = hevc->s_info.poc_lsb;
		n_state.poc_msb_prev = hevc->s_info.poc_msb;
	}
	if (slice) hevc_compute_poc(&n_state);
	memcpy(&hevc->s_info, &n_state, sizeof(HEVCSliceInfo));
	return ret;
}

static u8 hevc_get_sar_idx(u32 w, u32 h)
{
	u32 i;
	for (i=0; i<14; i++) {
		if ((avc_sar[i].w==w) && (avc_sar[i].h==h)) return i;
	}
	return 0xFF;
}

GF_Err gf_media_hevc_change_par(GF_HEVCConfig *hvcc, s32 ar_n, s32 ar_d)
{
	GF_BitStream *orig, *mod;
	HEVCState hevc;
	u32 i, bit_offset, flag, nal_hdr_size;
	s32 idx;
	GF_HEVCParamArray *spss;
	GF_AVCConfigSlot *slc;
	orig = NULL;

	memset(&hevc, 0, sizeof(HEVCState));
	hevc.sps_active_idx = -1;

	nal_hdr_size=2;

	i=0;
	spss = NULL;
	while ((spss = (GF_HEVCParamArray *)gf_list_enum(hvcc->param_array, &i))) {
		if (spss->type==GF_HEVC_NALU_SEQ_PARAM)
			break;
		spss = NULL;
	}
	if (!spss) return GF_NON_COMPLIANT_BITSTREAM;

	i=0;
	while ((slc = (GF_AVCConfigSlot *)gf_list_enum(spss->nalus, &i))) {
		char *no_emulation_buf = NULL;
		u32 no_emulation_buf_size = 0, emulation_bytes = 0;
		idx = gf_media_hevc_read_sps_ex(slc->data, slc->size, &hevc, &bit_offset);
		if (idx<0) {
			if ( orig )
				gf_bs_del(orig);
			continue;
		}

		/*SPS still contains emulation bytes*/
		no_emulation_buf = gf_malloc((slc->size - nal_hdr_size)*sizeof(char));
		no_emulation_buf_size = avc_remove_emulation_bytes(slc->data + nal_hdr_size, no_emulation_buf, slc->size - nal_hdr_size);

		orig = gf_bs_new(no_emulation_buf, no_emulation_buf_size, GF_BITSTREAM_READ);
		gf_bs_read_data(orig, no_emulation_buf, no_emulation_buf_size);
		gf_bs_seek(orig, 0);
		mod = gf_bs_new(NULL, 0, GF_BITSTREAM_WRITE);

		/*copy over till vui flag*/
		assert(bit_offset >= 8 * nal_hdr_size);
		while (bit_offset - 8 * nal_hdr_size/*bit_offset doesn't take care of the first two byte (NALU hdr)*/) {
			flag = gf_bs_read_int(orig, 1);
			gf_bs_write_int(mod, flag, 1);
			bit_offset--;
		}
		/*check VUI*/
		flag = gf_bs_read_int(orig, 1);
		gf_bs_write_int(mod, 1, 1); /*vui_parameters_present_flag*/
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
			sarx = hevc_get_sar_idx((u32) ar_n, (u32) ar_d);
			gf_bs_write_int(mod, sarx, 8);
			if (sarx==0xFF) {
				gf_bs_write_int(mod, ar_n, 16);
				gf_bs_write_int(mod, ar_d, 16);
			}
		}
		/*no VUI in input bitstream, set all vui flags to 0*/
		if (!flag) {
			gf_bs_write_int(mod, 0, 1);		/*overscan_info_present_flag */
			gf_bs_write_int(mod, 0, 1);		/*video_signal_type_present_flag */
			gf_bs_write_int(mod, 0, 1);		/*chroma_location_info_present_flag */

			gf_bs_write_int(mod, 0, 1); /*neutra_chroma_indication_flag */;
			gf_bs_write_int(mod, 0, 1); /*field_seq_flag */;
			gf_bs_write_int(mod, 0, 1); /*frame_field_info_present_flag*/;
			gf_bs_write_int(mod, 0, 1); /*default_display_window_flag*/;

			gf_bs_write_int(mod, 0, 1);		/*timing_info_present_flag*/
			gf_bs_write_int(mod, 0, 1);		/*bitstream_restriction*/
		}

		/*finally copy over remaining*/
		while (gf_bs_bits_available(orig)) {
			flag = gf_bs_read_int(orig, 1);
			gf_bs_write_int(mod, flag, 1);
		}
		gf_bs_del(orig);
		orig = NULL;
		gf_free(no_emulation_buf);

		/*set anti-emulation*/
		gf_bs_get_content(mod, (char **) &no_emulation_buf, &no_emulation_buf_size);
		emulation_bytes = avc_emulation_bytes_add_count(no_emulation_buf, no_emulation_buf_size);
		if (no_emulation_buf_size + emulation_bytes + nal_hdr_size > slc->size)
			slc->data = (char*)gf_realloc(slc->data, no_emulation_buf_size + emulation_bytes + nal_hdr_size);

		slc->size = avc_add_emulation_bytes(no_emulation_buf, slc->data + nal_hdr_size, no_emulation_buf_size) + nal_hdr_size;

		gf_bs_del(mod);
		gf_free(no_emulation_buf);
	}
	return GF_OK;
}

GF_EXPORT
GF_Err gf_hevc_get_sps_info_with_state(HEVCState *hevc, char *sps_data, u32 sps_size, u32 *sps_id, u32 *width, u32 *height, s32 *par_n, s32 *par_d)
{
	s32 idx;
	idx = gf_media_hevc_read_sps(sps_data, sps_size, hevc);
	if (idx<0) {
		return GF_NON_COMPLIANT_BITSTREAM;
	}
	if (sps_id) *sps_id = idx;

	if (width) *width = hevc->sps[idx].width;
	if (height) *height = hevc->sps[idx].height;
	if (par_n) *par_n = hevc->sps[idx].aspect_ratio_info_present_flag ? hevc->sps[idx].sar_width : (u32) -1;
	if (par_d) *par_d = hevc->sps[idx].aspect_ratio_info_present_flag ? hevc->sps[idx].sar_height : (u32) -1;
	return GF_OK;
}

GF_EXPORT
GF_Err gf_hevc_get_sps_info(char *sps_data, u32 sps_size, u32 *sps_id, u32 *width, u32 *height, s32 *par_n, s32 *par_d)
{
	HEVCState hevc;
	memset(&hevc, 0, sizeof(HEVCState));
	hevc.sps_active_idx = -1;
	return gf_hevc_get_sps_info_with_state(&hevc, sps_data, sps_size, sps_id, width, height, par_n, par_d);
}

#endif //GPAC_DISABLE_HEVC

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
	u64 pos = gf_bs_get_position(bs);
	u64 end = gf_bs_get_size(bs) - 6;

	pos += 1;
	b1 = gf_bs_read_u8(bs);
	while (pos <= end) {
		u8 b2 = gf_bs_read_u8(bs);
		if ((b1 == 0x0b) && (b2==0x77)) {
			gf_bs_seek(bs, pos-1);
			return GF_TRUE;
		}
		pos++;
		b1 = b2;
	}
	return GF_FALSE;
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

GF_EXPORT
u32 gf_ac3_get_channels(u32 acmod)
{
	u32 nb_ch;
	nb_ch = ac3_mod_to_chans[acmod];
	return nb_ch;
}

GF_EXPORT
u32 gf_ac3_get_bitrate(u32 brcode)
{
	return ac3_sizecod_to_bitrate[brcode];
}

Bool gf_ac3_parser(u8 *buf, u32 buflen, u32 *pos, GF_AC3Header *hdr, Bool full_parse)
{
	GF_BitStream *bs;
	Bool ret;

	if (buflen < 6) return GF_FALSE;
	(*pos) = AC3_FindSyncCode(buf, buflen);
	if (*pos >= buflen) return GF_FALSE;

	bs = gf_bs_new((const char*)(buf+*pos), buflen, GF_BITSTREAM_READ);
	ret = gf_ac3_parser_bs(bs, hdr, full_parse);
	gf_bs_del(bs);

	return ret;
}

GF_EXPORT
Bool gf_ac3_parser_bs(GF_BitStream *bs, GF_AC3Header *hdr, Bool full_parse)
{
	u32 fscod, frmsizecod, bsid, ac3_mod, freq, framesize, bsmod, syncword;
	u64 pos;
	if (!hdr || (gf_bs_available(bs) < 6)) return GF_FALSE;
	if (!AC3_FindSyncCodeBS(bs)) return GF_FALSE;

	pos = gf_bs_get_position(bs);

	syncword = gf_bs_read_u16(bs);
	if (syncword != 0x0B77) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_CODING, ("[AC3] Wrong sync word detected (0x%X - expecting 0x0B77).\n", syncword));
		return GF_FALSE;
	}
	gf_bs_read_u16(bs); //crc1
	fscod = gf_bs_read_int(bs, 2);
	frmsizecod = gf_bs_read_int(bs, 6);
	bsid = gf_bs_read_int(bs, 5);
	bsmod = gf_bs_read_int(bs, 3);
	ac3_mod = gf_bs_read_int(bs, 3);

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
		return GF_FALSE;
	}
	hdr->sample_rate = freq;
	hdr->framesize = framesize;

	if (full_parse) {
		hdr->bsid = bsid;
		hdr->bsmod = bsmod;
		hdr->acmod = ac3_mod;
		hdr->lfon = 0;
		hdr->fscod = fscod;
		hdr->brcode = frmsizecod / 2;
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

	return GF_TRUE;
}

GF_EXPORT
Bool gf_eac3_parser_bs(GF_BitStream *bs, GF_AC3Header *hdr, Bool full_parse)
{
	u32 fscod, bsid, ac3_mod, freq, framesize, syncword, substreamid, lfon, channels, numblkscod;
	u64 pos;

restart:
	if (!hdr || (gf_bs_available(bs) < 6))
		return GF_FALSE;
	if (!AC3_FindSyncCodeBS(bs))
		return GF_FALSE;

	pos = gf_bs_get_position(bs);
	framesize = 0;
	numblkscod = 0;

block:
	syncword = gf_bs_read_u16(bs);
	if (syncword != 0x0B77) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_CODING, ("[E-AC3] Wrong sync word detected (0x%X - expecting 0x0B77).\n", syncword));
		return GF_FALSE;
	}

	gf_bs_read_int(bs, 2); //strmtyp
	substreamid = gf_bs_read_int(bs, 3);
	framesize += gf_bs_read_int(bs, 11);
	fscod = gf_bs_read_int(bs, 2);
	if (fscod == 0x3) {
		fscod = gf_bs_read_int(bs, 2);
		numblkscod += 6;
	} else {
		numblkscod += gf_bs_read_int(bs, 2);
	}
	assert(numblkscod <= 9);

	if ((hdr->substreams >> substreamid) & 0x1) {
		if (!substreamid) {
			hdr->framesize = framesize;

			if (numblkscod < 6) { //we need 6 blocks to make a sample
				gf_bs_seek(bs, pos+2*framesize);
				if ((gf_bs_available(bs) < 6) || !AC3_FindSyncCodeBS(bs))
					return GF_FALSE;
				goto block;
			}

			gf_bs_seek(bs, pos);
			return GF_TRUE;
		} else {
			GF_LOG(GF_LOG_INFO, GF_LOG_CODING, ("[E-AC3] Detected sample in substream id=%u. Skipping.\n", substreamid));
			gf_bs_seek(bs, pos+framesize);
			goto restart;
		}
	}
	hdr->substreams |= (1 << substreamid);

	switch (fscod) {
	case 0:
		freq = 48000;
		break;
	case 1:
		freq = 44100;
		break;
	case 2:
		freq = 32000;
		break;
	default:
		return GF_FALSE;
	}

	ac3_mod = gf_bs_read_int(bs, 3);
	lfon = gf_bs_read_int(bs, 1);
	bsid = gf_bs_read_int(bs, 5);
	if (!substreamid && (bsid!=16/*E-AC3*/))
		return GF_FALSE;

	channels = ac3_mod_to_chans[ac3_mod];
	if (lfon)
		channels += 1;

	if (substreamid) {
		GF_LOG(GF_LOG_WARNING, GF_LOG_CODING, ("[E-AC3] Detected additional %u channels in substream id=%u - may not be handled correctly. Skipping.\n", channels, substreamid));
		gf_bs_seek(bs, pos+framesize);
		goto restart;
	} else {
		hdr->bitrate = 0;
		hdr->sample_rate = freq;
		hdr->framesize = framesize;
		hdr->lfon = lfon;
		hdr->channels = channels;
		if (full_parse) {
			hdr->bsid = bsid;
			hdr->bsmod = 0;
			hdr->acmod = ac3_mod;
			hdr->fscod = fscod;
			hdr->brcode = 0;
		}
	}

	if (numblkscod < 6) { //we need 6 blocks to make a sample
		gf_bs_seek(bs, pos+2*framesize);
		if ((gf_bs_available(bs) < 6) || !AC3_FindSyncCodeBS(bs))
			return GF_FALSE;
		goto block;
	}

	gf_bs_seek(bs, pos);

	return GF_TRUE;
}

#endif /*GPAC_DISABLE_AV_PARSERS*/


#if !defined(GPAC_DISABLE_AV_PARSERS) && !defined (GPAC_DISABLE_OGG)

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
		for (i=0; i<dim; i++) {
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
	while (i<6) {
		szNAME[i] = oggpack_read(&opb, 8);
		i++;
	}
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
		case 0:
			break;
		case 1:
		case 2:
			oggpack_read(&opb, 32);
			oggpack_read(&opb, 32);
			qq = oggpack_read(&opb, 4)+1;
			oggpack_read(&opb, 1);
			if (map_type==1) qb = vorbis_book_maptype1_quantvals(entries, dim);
			else if (map_type==2) qb = entries * dim;
			else qb = 0;
			for (j=0; j<qb; j++) oggpack_read(&opb, qq);
			break;
		}
	}
	times = oggpack_read(&opb, 6)+1;
	for (i=0; i<times; i++) oggpack_read(&opb, 16);
	times = oggpack_read(&opb, 6)+1;
	for (i=0; i<times; i++) {
		u32 type = oggpack_read(&opb, 16);
		if (type) {
			u32 *parts, *class_dims, count, rangebits;
			u32 max_class = 0;
			nb_part = oggpack_read(&opb, 5);
			parts = (u32*)gf_malloc(sizeof(u32) * nb_part);
			for (j=0; j<nb_part; j++) {
				parts[j] = oggpack_read(&opb, 4);
				if (max_class<parts[j]) max_class = parts[j];
			}
			class_dims = (u32*)gf_malloc(sizeof(u32) * (max_class+1));
			for (j=0; j<max_class+1; j++) {
				u32 class_sub;
				class_dims[j] = oggpack_read(&opb, 3) + 1;
				class_sub = oggpack_read(&opb, 2);
				if (class_sub) oggpack_read(&opb, 8);
				for (k=0; k < (u32) (1<<class_sub); k++) oggpack_read(&opb, 8);
			}
			oggpack_read(&opb, 2);
			rangebits=oggpack_read(&opb, 4);
			count = 0;
			for (j=0,k=0; j<nb_part; j++) {
				count+=class_dims[parts[j]];
				for (; k<count; k++) oggpack_read(&opb, rangebits);
			}
			gf_free(parts);
			gf_free(class_dims);
		} else {
			u32 j, nb_books;
			oggpack_read(&opb, 8+16+16+6+8);
			nb_books = oggpack_read(&opb, 4)+1;
			for (j=0; j<nb_books; j++) oggpack_read(&opb, 8);
		}
	}
	times = oggpack_read(&opb, 6)+1;
	for (i=0; i<times; i++) {
		u32 acc = 0;
		oggpack_read(&opb, 16);/*type*/
		oggpack_read(&opb, 24);
		oggpack_read(&opb,24);
		oggpack_read(&opb,24);
		nb_part = oggpack_read(&opb, 6)+1;
		oggpack_read(&opb, 8);
		for (j=0; j<nb_part; j++) {
			u32 cascade = oggpack_read(&opb, 3);
			if (oggpack_read(&opb, 1)) cascade |= (oggpack_read(&opb, 5)<<3);
			acc += icount(cascade);
		}
		for (j=0; j<acc; j++) oggpack_read(&opb, 8);
	}
	times = oggpack_read(&opb, 6)+1;
	for (i=0; i<times; i++) {
		u32 sub_maps = 1;
		oggpack_read(&opb, 16);
		if (oggpack_read(&opb, 1)) sub_maps = oggpack_read(&opb, 4)+1;
		if (oggpack_read(&opb, 1)) {
			u32 nb_steps = oggpack_read(&opb, 8)+1;
			for (j=0; j<nb_steps; j++) {
				oggpack_read(&opb, ilog(vp->channels));
				oggpack_read(&opb, ilog(vp->channels));
			}
		}
		oggpack_read(&opb, 2);
		if (sub_maps>1) {
			for(j=0; j<vp->channels; j++) oggpack_read(&opb, 4);
		}
		for (j=0; j<sub_maps; j++) {
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

#endif /*!defined(GPAC_DISABLE_AV_PARSERS) && !defined (GPAC_DISABLE_OGG)*/

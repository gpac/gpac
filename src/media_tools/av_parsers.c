/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre, Romain Bouqueau, Cyril Concolato
 *			Copyright (c) Telecom ParisTech 2000-2023
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
#include <gpac/maths.h>
#include <gpac/avparse.h>

#ifndef GPAC_DISABLE_OGG
#include <gpac/internal/ogg.h>
#endif

//uncomment/define globally to remove all bitstream parsing logging from code (this will break inspect mode analyze=bs)
//#define GPAC_DISABLE_AVPARSE_LOGS

#ifndef GPAC_DISABLE_AVPARSE_LOGS
void gf_bs_log_idx(GF_BitStream *bs, u32 nBits, const char *fname, s64 val, s32 idx1, s32 idx2, s32 idx3);

#define gf_bs_log(_bs, _nBits, _fname, _val) gf_bs_log_idx(_bs, _nBits, _fname, _val, -1, -1, -1)

static u32 gf_bs_read_int_log_idx3(GF_BitStream *bs, u32 nBits, const char *fname, s32 idx1, s32 idx2, s32 idx3)
{
	u32 val = gf_bs_read_int(bs, nBits);
	gf_bs_log_idx(bs, nBits, fname, val, idx1, idx2, idx3);
	return val;
}

#define gf_bs_read_int_log(_bs, _nBits, _fname) gf_bs_read_int_log_idx3(_bs, _nBits, _fname, -1, -1, -1)
#define gf_bs_read_int_log_idx(_bs, _nBits, _fname, _idx) gf_bs_read_int_log_idx3(_bs, _nBits, _fname, _idx, -1, -1)
#define gf_bs_read_int_log_idx2(_bs, _nBits, _fname, _idx1, _idx2) gf_bs_read_int_log_idx3(_bs, _nBits, _fname, (s32) _idx1, (s32) _idx2, -1)


#else

#define gf_bs_log(_bs, _nBits, _fname, _val)
#define gf_bs_log_idx(_bs, _nBits, _fname, _val, _idx1, _idx2, _idx3)

#define gf_bs_read_int_log(_bs, _nbb, _f) gf_bs_read_int(_bs, _nbb)
#define gf_bs_read_int_log_idx(_bs, _nbb, _f, _idx) gf_bs_read_int(_bs, _nbb)
#define gf_bs_read_int_log_idx2(_bs, _nbb, _f, _idx1, _idx2) gf_bs_read_int(_bs, _nbb)
#define gf_bs_read_int_log_idx3(_bs, _nbb, _f, _idx1, _idx2, _idx3) gf_bs_read_int(_bs, _nbb)

#endif




static const struct {
	u32 w, h;
} std_par[] =
{
	{ 4, 3}, {3, 2}, {16, 9}, {5, 3}, {5, 4}, {8, 5}, {2, 1}, {1, 1},
	{0, 0},
};

GF_EXPORT
void gf_media_reduce_aspect_ratio(u32 *width, u32 *height)
{
	u32 i = 0;
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
	//not standard one, reduce (brute force)
	i = 2;
	while (1) {
		if ((w <= i) || (h <= i))
			return;

		if (!(w % i) && !(h % i)) {
			*width = w / i;
			*height = h / i;
		}
		i += 1;
	}
}

GF_EXPORT
void gf_media_get_reduced_frame_rate(u32 *timescale, u32 *sample_dur)
{
	u32 res;
	if (!*sample_dur) return;
	res = *timescale / *sample_dur;
	if (res * (*sample_dur) == *timescale) {
		*timescale = res;
		*sample_dur = 1;
	}
	else if ((double)((u64)*timescale * 1001 - (u64)(res + 1) * (u64)*sample_dur * 1000) / ((u64)(res + 1) * (u64)*sample_dur * 1000) < 0.001) {
		*timescale = (res + 1) * 1000;
		*sample_dur = 1001;
	}
}

struct __m4v_profile
{
	u32 value;
	const char *name;
} M4VProfiles[] = {
	{0x00, "Reserved (0x00) Profile"},
	{0x01, "Simple Profile @ Level 1"},
	{0x02, "Simple Profile @ Level 2"},
	{0x03, "Simple Profile @ Level 3"},
	{0x08, "Simple Profile @ Level 0"},
	{0x10, "Simple Scalable Profile @ Level 0"},
	{0x11, "Simple Scalable Profile @ Level 1"},
	{0x12, "Simple Scalable Profile @ Level 2"},
	{0x21, "Core Profile @ Level 1"},
	{0x22, "Core Profile @ Level 2"},
	{0x32, "Main Profile @ Level 2"},
	{0x33, "Main Profile @ Level 3"},
	{0x34, "Main Profile @ Level 4"},
	{0x42, "N-bit Profile @ Level 2"},
	{0x51, "Scalable Texture Profile @ Level 1"},
	{0x61, "Simple Face Animation Profile @ Level 1"},
	{0x62, "Simple Face Animation Profile @ Level 2"},
	{0x63, "Simple FBA Profile @ Level 1"},
	{0x64, "Simple FBA Profile @ Level 2"},
	{0x71, "Basic Animated Texture Profile @ Level 1"},
	{0x72, "Basic Animated Texture Profile @ Level 2"},
	{0x7F, "AVC/H264 Profile"},
	{0x81, "Hybrid Profile @ Level 1"},
	{0x82, "Hybrid Profile @ Level 2"},
	{0x91, "Advanced Real Time Simple Profile @ Level 1"},
	{0x92, "Advanced Real Time Simple Profile @ Level 2"},
	{0x93, "Advanced Real Time Simple Profile @ Level 3"},
	{0x94, "Advanced Real Time Simple Profile @ Level 4"},
	{0xA1, "Core Scalable Profile @ Level1"},
	{0xA2, "Core Scalable Profile @ Level2"},
	{0xA3, "Core Scalable Profile @ Level3"},
	{0xB1, "Advanced Coding Efficiency Profile @ Level 1"},
	{0xB2, "Advanced Coding Efficiency Profile @ Level 2"},
	{0xB3, "Advanced Coding Efficiency Profile @ Level 3"},
	{0xB4, "Advanced Coding Efficiency Profile @ Level 4"},
	{0xC1, "Advanced Core Profile @ Level 1"},
	{0xC2, "Advanced Core Profile @ Level 2"},
	{0xD1, "Advanced Scalable Texture @ Level1"},
	{0xD2, "Advanced Scalable Texture @ Level2"},
	{0xE1, "Simple Studio Profile @ Level 1"},
	{0xE2, "Simple Studio Profile @ Level 2"},
	{0xE3, "Simple Studio Profile @ Level 3"},
	{0xE4, "Simple Studio Profile @ Level 4"},
	{0xE5, "Core Studio Profile @ Level 1"},
	{0xE6, "Core Studio Profile @ Level 2"},
	{0xE7, "Core Studio Profile @ Level 3"},
	{0xE8, "Core Studio Profile @ Level 4"},
	{0xF0, "Advanced Simple Profile @ Level 0"},
	{0xF1, "Advanced Simple Profile @ Level 1"},
	{0xF2, "Advanced Simple Profile @ Level 2"},
	{0xF3, "Advanced Simple Profile @ Level 3"},
	{0xF4, "Advanced Simple Profile @ Level 4"},
	{0xF5, "Advanced Simple Profile @ Level 5"},
	{0xF7, "Advanced Simple Profile @ Level 3b"},
	{0xF8, "Fine Granularity Scalable Profile @ Level 0"},
	{0xF9, "Fine Granularity Scalable Profile @ Level 1"},
	{0xFA, "Fine Granularity Scalable Profile @ Level 2"},
	{0xFB, "Fine Granularity Scalable Profile @ Level 3"},
	{0xFC, "Fine Granularity Scalable Profile @ Level 4"},
	{0xFD, "Fine Granularity Scalable Profile @ Level 5"},
	{0xFE, "Not part of MPEG-4 Visual profiles"},
	{0xFF, "No visual capability required"}
};

GF_EXPORT
const char *gf_m4v_get_profile_name(u8 video_pl)
{
	u32 i, count = GF_ARRAY_LENGTH(M4VProfiles);
	for (i=0; i<count; i++) {
		if ((u32)video_pl == M4VProfiles[i].value)
			return M4VProfiles[i].name;
	}
	return "ISO Reserved Profile";
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

struct __tag_m4v_parser
{
	GF_BitStream *bs;
	Bool mpeg12, step_mode;
	u32 current_object_type;
	u32 force_next_obj_type;
	u64 current_object_start;
	u32 tc_dec, prev_tc_dec, tc_disp, prev_tc_disp;
};

GF_EXPORT
GF_M4VParser *gf_m4v_parser_new(u8 *data, u64 data_size, Bool mpeg12video)
{
	GF_M4VParser *tmp;
	if (!data || !data_size) return NULL;
	GF_SAFEALLOC(tmp, GF_M4VParser);
	if (!tmp) return NULL;
	tmp->bs = gf_bs_new(data, data_size, GF_BITSTREAM_READ);
	tmp->mpeg12 = mpeg12video;
	return tmp;
}

GF_M4VParser *gf_m4v_parser_bs_new(GF_BitStream *bs, Bool mpeg12video)
{
	GF_M4VParser *tmp;
	GF_SAFEALLOC(tmp, GF_M4VParser);
	if (!tmp) return NULL;
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

GF_EXPORT
void gf_m4v_parser_del_no_bs(GF_M4VParser *m4v)
{
	gf_free(m4v);
}

GF_EXPORT
void gf_m4v_parser_set_inspect(GF_M4VParser *m4v)
{
	if (m4v) m4v->step_mode = 1;
}
GF_EXPORT
u32 gf_m4v_parser_get_obj_type(GF_M4VParser *m4v)
{
	if (m4v) return m4v->current_object_type;
	return 0;
}

#define M4V_CACHE_SIZE		4096
s32 M4V_LoadObject(GF_M4VParser *m4v)
{
	u32 v, bpos, found;
	char m4v_cache[M4V_CACHE_SIZE];
	u64 end, cache_start, load_size;
	if (!m4v) return 0;
	if (m4v->force_next_obj_type) {
		m4v->current_object_type = m4v->force_next_obj_type - 1;
		m4v->force_next_obj_type = 0;
		return (s32)m4v->current_object_type;
	}

	bpos = 0;
	found = 0;
	load_size = 0;
	end = 0;
	cache_start = 0;
	v = 0xffffffff;
	while (!end) {
		/*refill cache*/
		if (bpos == (u32)load_size) {
			if (!gf_bs_available(m4v->bs)) break;
			load_size = gf_bs_available(m4v->bs);
			if (load_size > M4V_CACHE_SIZE) load_size = M4V_CACHE_SIZE;
			bpos = 0;
			cache_start = gf_bs_get_position(m4v->bs);
			gf_bs_read_data(m4v->bs, m4v_cache, (u32)load_size);
		}
		v = ((v << 8) & 0xFFFFFF00) | ((u8)m4v_cache[bpos]);
		bpos++;
		if ((v & 0xFFFFFF00) == 0x00000100) {
			end = cache_start + bpos - 4;
			found = 1;
			break;
		}
	}
	if (!found) return -1;
	m4v->current_object_start = end;
	gf_bs_seek(m4v->bs, end + 3);
	m4v->current_object_type = gf_bs_read_u8(m4v->bs);
	return (s32)m4v->current_object_type;
}


GF_EXPORT
void gf_m4v_rewrite_pl(u8 **o_data, u32 *o_dataLen, u8 PL)
{
	u32 pos = 0;
	unsigned char *data = (unsigned char *)*o_data;
	u32 dataLen = *o_dataLen;

	while (pos + 4 < dataLen) {
		if (!data[pos] && !data[pos + 1] && (data[pos + 2] == 0x01) && (data[pos + 3] == M4V_VOS_START_CODE)) {
			data[pos + 4] = PL;
			return;
		}
		pos++;
	}
	/*emulate VOS at beggining*/
	(*o_data) = (char *)gf_malloc(sizeof(char)*(dataLen + 5));
	(*o_data)[0] = 0;
	(*o_data)[1] = 0;
	(*o_data)[2] = 1;
	(*o_data)[3] = (char)M4V_VOS_START_CODE;
	(*o_data)[4] = PL;
	memcpy((*o_data + 5), data, sizeof(char)*dataLen);
	gf_free(data);
	(*o_dataLen) = dataLen + 5;
}

static GF_Err M4V_Reset(GF_M4VParser *m4v, u64 start)
{
	gf_bs_seek(m4v->bs, start);

	gf_fatal_assert(start < (u64)1<<31);
	m4v->current_object_start = (u32)start;
	m4v->current_object_type = 0;
	return GF_OK;
}

void gf_m4v_parser_reset(GF_M4VParser *m4v, u8 sc_type)
{
	m4v->current_object_start = 0;
	m4v->current_object_type = 0;
	m4v->force_next_obj_type = sc_type;
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
			gf_bs_read_data(m4v->bs, (char *)p, 4);
			dsi->width = (p[0] << 4) | ((p[1] >> 4) & 0xf);
			dsi->height = ((p[1] & 0xf) << 8) | p[2];

			dsi->VideoPL = GF_CODECID_MPEG1;
			par = (p[3] >> 4) & 0xf;
			switch (par) {
			case 2:
				dsi->par_num = dsi->height / 3;
				dsi->par_den = dsi->width / 4;
				break;
			case 3:
				dsi->par_num = dsi->height / 9;
				dsi->par_den = dsi->width / 16;
				break;
			case 4:
				dsi->par_num = dsi->height / 2;
				dsi->par_den = dsi->width / 21;
				break;
			default:
				dsi->par_den = dsi->par_num = 0;
				break;
			}
			switch (p[3] & 0xf) {
			case 0:
				break;
			case 1:
				dsi->fps = 24000.0 / 1001.0;
				break;
			case 2:
				dsi->fps = 24.0;
				break;
			case 3:
				dsi->fps = 25.0;
				break;
			case 4:
				dsi->fps = 30000.0 / 1001.0;
				break;
			case 5:
				dsi->fps = 30.0;
				break;
			case 6:
				dsi->fps = 50.0;
				break;
			case 7:
				dsi->fps = ((60.0*1000.0) / 1001.0);
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
			gf_bs_read_data(m4v->bs, (char *)p, 4);
			ext_type = ((p[0] >> 4) & 0xf);
			if (ext_type == 1) {
				dsi->VideoPL = (p[0]&0xf) | ((p[1] >> 4) & 0xf);
				dsi->progresive = (p[1] & 0x8) ? 1 : 0;
				dsi->chroma_fmt = (p[1]>>1) & 0x3;
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
	return GF_OK;
}


static const struct {
	u32 w, h;
} m4v_sar[6] = { { 0,   0 }, { 1,   1 }, { 12, 11 }, { 10, 11 }, { 16, 11 }, { 40, 33 } };

static u8 m4v_get_sar_idx(u32 w, u32 h)
{
	u32 i;
	for (i = 0; i < 6; i++) {
		if ((m4v_sar[i].w == w) && (m4v_sar[i].h == h)) return i;
	}
	return 0xF;
}

static void gf_m4v_parse_vol(GF_M4VParser *m4v, GF_M4VDecSpecInfo *dsi)
{
	u8 verid, par;
	s32 clock_rate;
	u8 vpl = dsi->VideoPL;

	memset(dsi, 0, sizeof(GF_M4VDecSpecInfo));
	dsi->VideoPL = vpl;

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
		dsi->chroma_fmt = gf_bs_read_int(m4v->bs, 2);
		gf_bs_read_int(m4v->bs, 1);
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
		gf_bs_read_int(m4v->bs, 1); //marker bit
		dsi->width = gf_bs_read_int(m4v->bs, 13);
		gf_bs_read_int(m4v->bs, 1); //marker bit
		dsi->height = gf_bs_read_int(m4v->bs, 13);
		gf_bs_read_int(m4v->bs, 1); //marker bit
		dsi->progresive = !gf_bs_read_int(m4v->bs, 1);
	} else {
		dsi->width = dsi->height = 0;
	}
	gf_bs_align(m4v->bs);
}

static GF_Err gf_m4v_parse_config_mpeg4(GF_M4VParser *m4v, GF_M4VDecSpecInfo *dsi)
{
	s32 o_type;
	u8 go;

	if (!m4v || !dsi) return GF_BAD_PARAM;

	memset(dsi, 0, sizeof(GF_M4VDecSpecInfo));

	go = 1;
	while (go) {
		o_type = M4V_LoadObject(m4v);
		switch (o_type) {
			/*vosh*/
		case M4V_VOS_START_CODE:
			dsi->VideoPL = (u8)gf_bs_read_u8(m4v->bs);
			break;

		case M4V_VOL_START_CODE:
			gf_m4v_parse_vol(m4v, dsi);
			/*shape will be done later*/
			gf_bs_align(m4v->bs);
			break;

		case M4V_VOP_START_CODE:
		case M4V_GOV_START_CODE:
			go = 0;
			break;
			/*EOS*/
		case -1:
			m4v->current_object_start = gf_bs_get_position(m4v->bs);
			return GF_EOS;
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
	}
	else {
		return gf_m4v_parse_config_mpeg4(m4v, dsi);
	}
}

static GF_Err gf_m4v_parse_frame_mpeg12(GF_M4VParser *m4v, GF_M4VDecSpecInfo *dsi, u8 *frame_type, u32 *time_inc, u64 *size, u64 *start, Bool *is_coded)
{
	u8 go, hasVOP, firstObj, val;
	s32 o_type;

	if (!m4v || !size || !start || !frame_type) return GF_BAD_PARAM;

	*size = 0;
	firstObj = 1;
	hasVOP = 0;
	*is_coded = GF_FALSE;
	*frame_type = 0;

	if (!m4v->step_mode)
		M4V_Reset(m4v, m4v->current_object_start);

	m4v->current_object_type = (u32)-1;
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

			/*val = */gf_bs_read_u8(m4v->bs);
			val = gf_bs_read_u8(m4v->bs);
			*frame_type = ((val >> 3) & 0x7);
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
		if (m4v->step_mode)
			return GF_OK;
	}
	*size = m4v->current_object_start - *start;
	return GF_OK;
}

static GF_Err gf_m4v_parse_frame_mpeg4(GF_M4VParser *m4v, GF_M4VDecSpecInfo *dsi, u8 *frame_type, u32 *time_inc, u64 *size, u64 *start, Bool *is_coded)
{
	u8 go, hasVOP, firstObj, secs;
	s32 o_type;
	u32 vop_inc = 0;

	if (!m4v || !size || !start || !frame_type) return GF_BAD_PARAM;

	*size = 0;
	firstObj = 1;
	hasVOP = 0;
	*is_coded = 0;
	m4v->current_object_type = (u32)-1;
	*frame_type = 0;
	*start = 0;

	if (!m4v->step_mode)
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
			*frame_type = 1 + gf_bs_read_int(m4v->bs, 2);
			/*modulo time base*/
			secs = 0;
			while (gf_bs_read_int(m4v->bs, 1) != 0)
				secs++;
			/*no support for B frames in parsing*/
			secs += (dsi->enh_layer || *frame_type!=2) ? m4v->tc_dec : m4v->tc_disp;
			/*marker*/
			gf_bs_read_int(m4v->bs, 1);
			/*vop_time_inc*/
			if (dsi->NumBitsTimeIncrement)
				vop_inc = gf_bs_read_int(m4v->bs, dsi->NumBitsTimeIncrement);

			m4v->prev_tc_dec = m4v->tc_dec;
			m4v->prev_tc_disp = m4v->tc_disp;
			if (dsi->enh_layer || *frame_type!=2) {
				m4v->tc_disp = m4v->tc_dec;
				m4v->tc_dec = secs;
			}
			*time_inc = secs * dsi->clock_rate + vop_inc;
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

		case M4V_VOL_START_CODE:
			if (m4v->step_mode)
				gf_m4v_parse_vol(m4v, dsi);
		case M4V_VOS_START_CODE:
			if (hasVOP) {
				go = 0;
			}
			else if (firstObj) {
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
		if (m4v->step_mode)
			return GF_OK;
	}
	if (m4v->current_object_start < *start) return GF_NON_COMPLIANT_BITSTREAM;
	*size = m4v->current_object_start - *start;
	return GF_OK;
}

GF_EXPORT
GF_Err gf_m4v_parse_frame(GF_M4VParser *m4v, GF_M4VDecSpecInfo *dsi, u8 *frame_type, u32 *time_inc, u64 *size, u64 *start, Bool *is_coded)
{
	if (m4v->mpeg12) {
		return gf_m4v_parse_frame_mpeg12(m4v, dsi, frame_type, time_inc, size, start, is_coded);
	}
	else {
		return gf_m4v_parse_frame_mpeg4(m4v, dsi, frame_type, time_inc, size, start, is_coded);
	}
}

GF_Err gf_m4v_rewrite_par(u8 **o_data, u32 *o_dataLen, s32 par_n, s32 par_d)
{
	u64 start, end, size;
	GF_BitStream *mod;
	GF_M4VParser *m4v;
	Bool go = 1;

	m4v = gf_m4v_parser_new(*o_data, *o_dataLen, 0);
	mod = gf_bs_new(NULL, 0, GF_BITSTREAM_WRITE);

	start = 0;
	while (go) {
		u32 type = M4V_LoadObject(m4v);

		end = gf_bs_get_position(m4v->bs) - 4;
		size = end - start;
		/*store previous object*/
		if (size) {
			assert (size < (u64)1<<31);
			gf_bs_write_data(mod, *o_data + start, (u32)size);
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
			gf_bs_write_int(mod, (u32)start, 1);
			if (start) {
				gf_bs_write_int(mod, gf_bs_read_int(m4v->bs, 7), 7);
			}
			start = gf_bs_read_int(m4v->bs, 4);
			if (start == 0xF) {
				gf_bs_read_int(m4v->bs, 8);
				gf_bs_read_int(m4v->bs, 8);
			}
			if ((par_n >= 0) && (par_d >= 0)) {
				u8 par = m4v_get_sar_idx(par_n, par_d);
				gf_bs_write_int(mod, par, 4);
				if (par == 0xF) {
					gf_bs_write_int(mod, par_n, 8);
					gf_bs_write_int(mod, par_d, 8);
				}
			}
			else {
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

#if 0 //unused
Bool gf_m4v_is_valid_object_type(GF_M4VParser *m4v)
{
	return ((s32)m4v->current_object_type == -1) ? 0 : 1;
}
#endif


GF_EXPORT
GF_Err gf_m4v_get_config(u8 *rawdsi, u32 rawdsi_size, GF_M4VDecSpecInfo *dsi)
{
	GF_Err e;
	GF_M4VParser *vparse;
	if (!rawdsi || !rawdsi_size) return GF_NON_COMPLIANT_BITSTREAM;
	vparse = gf_m4v_parser_new(rawdsi, rawdsi_size, 0);
	e = gf_m4v_parse_config(vparse, dsi);
	dsi->next_object_start = (u32)vparse->current_object_start;
	gf_m4v_parser_del(vparse);
	return e < 0 ? e : GF_OK;
}

GF_EXPORT
GF_Err gf_mpegv12_get_config(u8 *rawdsi, u32 rawdsi_size, GF_M4VDecSpecInfo *dsi)
{
	GF_Err e;
	GF_M4VParser *vparse;
	if (!rawdsi || !rawdsi_size) return GF_NON_COMPLIANT_BITSTREAM;
	vparse = gf_m4v_parser_new(rawdsi, rawdsi_size, GF_TRUE);
	e = gf_m4v_parse_config(vparse, dsi);
	dsi->next_object_start = (u32)vparse->current_object_start;
	gf_m4v_parser_del(vparse);
	return e;
}

#endif


/*
	AAC parser
*/

struct __m4a_oti
{
	u32 type;
	const char *name;
} M4AObjectTypes[] = {
	{0, "MPEG-4 Audio Reserved"},
	{1, "MPEG-4 Audio AAC Main"},
	{2, "MPEG-4 Audio AAC LC"},
	{3, "MPEG-4 Audio AAC SSR"},
	{4, "MPEG-4 Audio AAC LTP"},
	{5, "MPEG-4 Audio SBR"},
	{6, "MPEG-4 Audio AAC Scalable"},
	{7, "MPEG-4 Audio TwinVQ"},
	{8, "MPEG-4 Audio CELP"},
	{9, "MPEG-4 Audio HVXC"},
	{10, "MPEG-4 Audio Reserved"},
	{11, "MPEG-4 Audio Reserved"},
	{12, "MPEG-4 Audio TTSI"},
	{13, "MPEG-4 Audio Main synthetic"},
	{14, "MPEG-4 Audio Wavetable synthesis"},
	{15, "MPEG-4 Audio General MIDI"},
	{16, "MPEG-4 Audio Algorithmic Synthesis and Audio FX"},
	{17, "MPEG-4 Audio ER AAC LC"},
	{18, "MPEG-4 Audio Reserved"},
	{19, "MPEG-4 Audio ER AAC LTP"},
	{20, "MPEG-4 Audio ER AAC scalable"},
	{21, "MPEG-4 Audio ER TwinVQ"},
	{22, "MPEG-4 Audio ER BSAC"},
	{23, "MPEG-4 Audio ER AAC LD"},
	{24, "MPEG-4 Audio ER CELP"},
	{25, "MPEG-4 Audio ER HVXC"},
	{26, "MPEG-4 Audio ER HILN"},
	{27, "MPEG-4 Audio ER Parametric"},
	{28, "MPEG-4 Audio SSC"},
	{29, "MPEG-4 Audio ParametricStereo"},
	{30, "MPEG-4 Audio Reserved"},
	{31, "MPEG-4 Audio Reserved"},
	{32, "MPEG-1 Audio Layer-1"},
	{33, "MPEG-1 Audio Layer-2"},
	{34, "MPEG-1 Audio Layer-3"},
	{35, "MPEG-4 Audio DST"},
	{36, "MPEG-4 Audio ALS"},
	{37, "MPEG-4 Audio SLS"},
	{42, "MPEG Audio xHE-AAC"},
};

GF_EXPORT
const char *gf_m4a_object_type_name(u32 objectType)
{
	u32 i, count = GF_ARRAY_LENGTH(M4AObjectTypes);
	for (i=0; i<count; i++) {
		if (objectType==M4AObjectTypes[i].type)
			return M4AObjectTypes[i].name;
	}
	return "MPEG-4 Audio Unknown";
}

struct __m4a_profile
{
	u32 value;
	const char *name;
} M4AProfiles[] = {
	{0x00, "ISO Reserved (0x00)"},
	{0x01, "Main Audio Profile @ Level 1"},
	{0x02, "Main Audio Profile @ Level 2"},
	{0x03, "Main Audio Profile @ Level 3"},
	{0x04, "Main Audio Profile @ Level 4"},
	{0x05, "Scalable Audio Profile @ Level 1"},
	{0x06, "Scalable Audio Profile @ Level 2"},
	{0x07, "Scalable Audio Profile @ Level 3"},
	{0x08, "Scalable Audio Profile @ Level 4"},
	{0x09, "Speech Audio Profile @ Level 1"},
	{0x0A, "Speech Audio Profile @ Level 2"},
	{0x0B, "Synthetic Audio Profile @ Level 1"},
	{0x0C, "Synthetic Audio Profile @ Level 2"},
	{0x0D, "Synthetic Audio Profile @ Level 3"},
	{0x0E, "High Quality Audio Profile @ Level 1"},
	{0x0F, "High Quality Audio Profile @ Level 2"},
	{0x10, "High Quality Audio Profile @ Level 3"},
	{0x11, "High Quality Audio Profile @ Level 4"},
	{0x12, "High Quality Audio Profile @ Level 5"},
	{0x13, "High Quality Audio Profile @ Level 6"},
	{0x14, "High Quality Audio Profile @ Level 7"},
	{0x15, "High Quality Audio Profile @ Level 8"},
	{0x16, "Low Delay Audio Profile @ Level 1"},
	{0x17, "Low Delay Audio Profile @ Level 2"},
	{0x18, "Low Delay Audio Profile @ Level 3"},
	{0x19, "Low Delay Audio Profile @ Level 4"},
	{0x1A, "Low Delay Audio Profile @ Level 5"},
	{0x1B, "Low Delay Audio Profile @ Level 6"},
	{0x1C, "Low Delay Audio Profile @ Level 7"},
	{0x1D, "Low Delay Audio Profile @ Level 8"},
	{0x1E, "Natural Audio Profile @ Level 1"},
	{0x1F, "Natural Audio Profile @ Level 2"},
	{0x20, "Natural Audio Profile @ Level 3"},
	{0x21, "Natural Audio Profile @ Level 4"},
	{0x22, "Mobile Audio Internetworking Profile @ Level 1"},
	{0x23, "Mobile Audio Internetworking Profile @ Level 2"},
	{0x24, "Mobile Audio Internetworking Profile @ Level 3"},
	{0x25, "Mobile Audio Internetworking Profile @ Level 4"},
	{0x26, "Mobile Audio Internetworking Profile @ Level 5"},
	{0x27, "Mobile Audio Internetworking Profile @ Level 6"},
	{0x28, "AAC Profile @ Level 1"},
	{0x29, "AAC Profile @ Level 2"},
	{0x2A, "AAC Profile @ Level 4"},
	{0x2B, "AAC Profile @ Level 5"},
	{0x2C, "High Efficiency AAC Profile @ Level 2"},
	{0x2D, "High Efficiency AAC Profile @ Level 3"},
	{0x2E, "High Efficiency AAC Profile @ Level 4"},
	{0x2F, "High Efficiency AAC Profile @ Level 5"},
	{0x30, "High Efficiency AAC v2 Profile @ Level 2"},
	{0x31, "High Efficiency AAC v2 Profile @ Level 3"},
	{0x32, "High Efficiency AAC v2 Profile @ Level 4"},
	{0x33, "High Efficiency AAC v2 Profile @ Level 5"},
	{0x34, "Low Delay AAC Profile"},
	{0x35, "Baseline MPEG Surround Profile @ Level 1"},
	{0x36, "Baseline MPEG Surround Profile @ Level 2"},
	{0x37, "Baseline MPEG Surround Profile @ Level 3"},
	{0x38, "Baseline MPEG Surround Profile @ Level 4"},
	{0x39, "Baseline MPEG Surround Profile @ Level 5"},
	{0x3A, "Baseline MPEG Surround Profile @ Level 6"},
	{0x3B, "High Definition AAC Profile @ Level 1"},
	{0x3C, "ALS Simple Profile @ Level 1"},
	{0x50, "AAC Profile @ Level 6"},
	{0x51, "AAC Profile @ Level 7"},
	{0x52, "High Efficiency AAC Profile @ Level 6"},
	{0x53, "High Efficiency AAC Profile @ Level 7"},
	{0x54, "High Efficiency AAC v2 Profile @ Level 6"},
	{0x55, "High Efficiency AAC v2 Profile @ Level 7"},
	{0x56, "Extended High Efficiency AAC Profile @ Level 6"},
	{0x57, "Extended High Efficiency AAC Profile @ Level 7"},
	{0xFE, "Not part of MPEG-4 audio profiles"},
	{0xFF, "No audio capability required"}
};

GF_EXPORT
const char *gf_m4a_get_profile_name(u8 audio_pl)
{
	u32 i, count = GF_ARRAY_LENGTH(M4AProfiles);
	for (i=0; i<count; i++) {
		if ((u32) audio_pl==M4AProfiles[i].value)
			return M4AProfiles[i].name;
	}
	return "ISO Reserved / User Private";
}

#ifndef GPAC_DISABLE_AV_PARSERS

GF_EXPORT
u32 gf_m4a_get_profile(GF_M4ADecSpecInfo *cfg)
{
	switch (cfg->base_object_type) {
	case 2: /*AAC LC*/
		if (cfg->nb_chan <= 2)
			return (cfg->base_sr <= 24000) ? 0x28 : 0x29; /*LC@L1 or LC@L2*/
		if (cfg->nb_chan <= 5)
			return (cfg->base_sr <= 48000) ? 0x2A : 0x2B; /*LC@L4 or LC@L5*/
		return (cfg->base_sr <= 48000) ? 0x50 : 0x51; /*LC@L4 or LC@L5*/
	case 5: /*HE-AAC - SBR*/
		if (cfg->nb_chan <= 2)
			return (cfg->base_sr <= 24000) ? 0x2C : 0x2D; /*HE@L2 or HE@L3*/
		if (cfg->nb_chan <= 5)
			return (cfg->base_sr <= 48000) ? 0x2E : 0x2F; /*HE@L4 or HE@L5*/
		return (cfg->base_sr <= 48000) ? 0x52 : 0x53; /*HE@L6 or HE@L7*/
	case 29: /*HE-AACv2 - SBR+PS*/
		if (cfg->nb_chan <= 2)
			return (cfg->base_sr <= 24000) ? 0x30 : 0x31; /*HE-AACv2@L2 or HE-AACv2@L3*/
		if (cfg->nb_chan <= 5)
			return (cfg->base_sr <= 48000) ? 0x32 : 0x33; /*HE-AACv2@L4 or HE-AACv2@L5*/
		return (cfg->base_sr <= 48000) ? 0x54 : 0x55; /*HE-AACv2@L6 or HE-AACv2@L7*/
	/*default to HQ*/
	default:
		if (cfg->nb_chan <= 2) return (cfg->base_sr < 24000) ? 0x0E : 0x0F; /*HQ@L1 or HQ@L2*/
		return 0x10; /*HQ@L3*/
	}
}

GF_EXPORT
GF_Err gf_m4a_parse_program_config_element(GF_BitStream *bs, GF_M4ADecSpecInfo *cfg)
{
	u32 i;

	cfg->program_config_element_present = 1;
	cfg->cpe_channels = 0;

	cfg->element_instance_tag = gf_bs_read_int_log(bs, 4, "element_instance_tag");
	cfg->object_type = gf_bs_read_int_log(bs, 2, "object_type");
	cfg->sampling_frequency_index = gf_bs_read_int_log(bs, 4, "sampling_frequency_index");
	cfg->num_front_channel_elements = gf_bs_read_int_log(bs, 4, "num_front_channel_elements");
	cfg->num_side_channel_elements = gf_bs_read_int_log(bs, 4, "num_side_channel_elements");
	cfg->num_back_channel_elements = gf_bs_read_int_log(bs, 4, "num_back_channel_elements");
	cfg->num_lfe_channel_elements = gf_bs_read_int_log(bs, 2, "num_lfe_channel_elements");
	cfg->num_assoc_data_elements = gf_bs_read_int_log(bs, 3, "num_assoc_data_elements");
	cfg->num_valid_cc_elements = gf_bs_read_int_log(bs, 4, "num_valid_cc_elements");
	cfg->mono_mixdown_present = (Bool)gf_bs_read_int_log(bs, 1, "mono_mixdown_present");
	if (cfg->mono_mixdown_present) {
		cfg->mono_mixdown_element_number = gf_bs_read_int_log(bs, 4, "mono_mixdown_element_number");
	}
	cfg->stereo_mixdown_present = gf_bs_read_int_log(bs, 1, "stereo_mixdown_present");
	if (cfg->stereo_mixdown_present) {
		cfg->stereo_mixdown_element_number = gf_bs_read_int_log(bs, 4, "stereo_mixdown_element_number");
	}
	cfg->matrix_mixdown_idx_present = gf_bs_read_int_log(bs, 1, "matrix_mixdown_idx_present");
	if (cfg->matrix_mixdown_idx_present) {
		cfg->matrix_mixdown_idx = gf_bs_read_int_log(bs, 2, "matrix_mixdown_idx");
		cfg->pseudo_surround_enable = gf_bs_read_int_log(bs, 1, "pseudo_surround_enable");
	}
	for (i = 0; i < cfg->num_front_channel_elements; i++) {
		cfg->front_element_is_cpe[i] = gf_bs_read_int_log_idx(bs, 1, "front_element_is_cpe", i);
		cfg->front_element_tag_select[i] = gf_bs_read_int_log_idx(bs, 4, "front_element_tag_select", i);
		if (cfg->front_element_is_cpe[i]) cfg->cpe_channels++;
	}
	for (i = 0; i < cfg->num_side_channel_elements; i++) {
		cfg->side_element_is_cpe[i] = gf_bs_read_int_log_idx(bs, 1, "side_element_is_cpe", i);
		cfg->side_element_tag_select[i] = gf_bs_read_int_log_idx(bs, 4, "side_element_tag_select", i);
		if (cfg->side_element_is_cpe[i]) cfg->cpe_channels++;
	}
	for (i = 0; i < cfg->num_back_channel_elements; i++) {
		cfg->back_element_is_cpe[i] = gf_bs_read_int_log_idx(bs, 1, "back_element_is_cpe", i);
		cfg->back_element_tag_select[i] = gf_bs_read_int_log_idx(bs, 4, "back_element_tag_select", i);
		if (cfg->back_element_is_cpe[i]) cfg->cpe_channels++;
	}
	for (i = 0; i < cfg->num_lfe_channel_elements; i++) {
		cfg->lfe_element_tag_select[i] = gf_bs_read_int_log_idx(bs, 4, "lfe_element_tag_select", i);
	}
	for (i = 0; i < cfg->num_assoc_data_elements; i++) {
		cfg->assoc_data_element_tag_select[i] = gf_bs_read_int_log_idx(bs, 4, "assoc_data_element_tag_select", i);
	}

	for (i = 0; i < cfg->num_valid_cc_elements; i++) {
		cfg->cc_element_is_ind_sw[i] = gf_bs_read_int_log_idx(bs, 1, "cc_element_is_ind_sw", i);
		cfg->valid_cc_element_tag_select[i] = gf_bs_read_int_log_idx(bs, 4, "valid_cc_element_tag_select", i);
	}
	gf_bs_align(bs);
	cfg->comment_field_bytes = gf_bs_read_int_log(bs, 8, "comment_field_bytes");
	gf_bs_read_data(bs, (char *)cfg->comments, cfg->comment_field_bytes);

	cfg->nb_chan = cfg->num_front_channel_elements + cfg->num_back_channel_elements + cfg->num_side_channel_elements + cfg->num_lfe_channel_elements;
	cfg->nb_chan += cfg->cpe_channels;

	return GF_OK;
}

GF_EXPORT
GF_Err gf_m4a_parse_config(GF_BitStream *bs, GF_M4ADecSpecInfo *cfg, Bool size_known)
{
	u32 audio_obj_type;
	memset(cfg, 0, sizeof(GF_M4ADecSpecInfo));
	cfg->base_object_type = gf_bs_read_int_log(bs, 5, "base_object_type");
	/*extended object type*/
	if (cfg->base_object_type == 31) {
		cfg->base_object_type = 32 + gf_bs_read_int_log(bs, 6, "extended_base_object_type");
	}
	cfg->base_sr_index = gf_bs_read_int_log(bs, 4, "base_samplerate_index");
	if (cfg->base_sr_index == 0x0F) {
		cfg->base_sr = gf_bs_read_int_log(bs, 24, "base_samplerate");
	}
	else {
		cfg->base_sr = GF_M4ASampleRates[cfg->base_sr_index];
	}

	cfg->chan_cfg = gf_bs_read_int_log(bs, 4, "channel_configuration");
	if (cfg->chan_cfg) {
		cfg->nb_chan = GF_M4ANumChannels[cfg->chan_cfg - 1];
	}

	audio_obj_type = cfg->base_object_type;
	if (cfg->base_object_type == 5 || cfg->base_object_type == 29) {
		if (cfg->base_object_type == 29) {
			cfg->has_ps = 1;
			cfg->nb_chan = 1;
		}
		cfg->has_sbr = GF_TRUE;
		cfg->sbr_sr_index = gf_bs_read_int_log(bs, 4, "sbr_samplerate_index");
		if (cfg->sbr_sr_index == 0x0F) {
			cfg->sbr_sr = gf_bs_read_int_log(bs, 24, "sbr_samplerate");
		}
		else {
			cfg->sbr_sr = GF_M4ASampleRates[cfg->sbr_sr_index];
		}
		cfg->sbr_object_type = gf_bs_read_int_log(bs, 5, "sbr_object_type");
		if (cfg->sbr_object_type==31)
			cfg->sbr_object_type = 32 + gf_bs_read_int_log(bs, 6, "audioObjectTypeExt");
		audio_obj_type = cfg->sbr_object_type;
		if (cfg->sbr_object_type==22) {
			/*ext_chan_cfg = */gf_bs_read_int_log(bs, 4, "channel_configuration");
		}
	}

	/*object cfg*/
	switch (audio_obj_type) {
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
	case 42:
	{
		Bool ext_flag;
		gf_bs_read_int_log(bs, 1, "frame_length_flag");
		if (gf_bs_read_int_log(bs, 1, "depends_on_core_coder"))
			gf_bs_read_int_log(bs, 14, "delay");
		ext_flag = gf_bs_read_int_log(bs, 1, "extension_flag");

		if (!cfg->chan_cfg) {
			gf_m4a_parse_program_config_element(bs, cfg);
		}

		if ((cfg->base_object_type == 6) || (cfg->base_object_type == 20)) {
			gf_bs_read_int_log(bs, 3, "layerN");
		}
		if (ext_flag) {
			if (cfg->base_object_type == 22) {
				gf_bs_read_int_log(bs, 5, "numOfSubFrame");
				gf_bs_read_int_log(bs, 11, "layer_length");
			}
			if ((cfg->base_object_type == 17)
				|| (cfg->base_object_type == 19)
				|| (cfg->base_object_type == 20)
				|| (cfg->base_object_type == 23)
			) {
				gf_bs_read_int_log(bs, 1, "aacSectionDataResilienceFlag");
				gf_bs_read_int_log(bs, 1, "aacScalefactorDataResilienceFlag");
				gf_bs_read_int_log(bs, 1, "aacSpectralDataResilienceFlag");
			}
			gf_bs_read_int_log(bs, 1, "extensionFlag3");
		}
	}
	break;
	}
	/*ER cfg*/
	switch (audio_obj_type) {
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
		u32 epConfig = gf_bs_read_int_log(bs, 2, "epConfig");
		if ((epConfig == 2) || (epConfig == 3)) {
		}
		if (epConfig == 3) {
			gf_bs_read_int_log(bs, 1, "directMapping");
		}
	}
	break;
	}

	if (size_known && (cfg->base_object_type != 5) && (cfg->base_object_type != 29)) {
		while (gf_bs_available(bs) >= 2) {
			u32 sync = gf_bs_peek_bits(bs, 11, 0);
			if (sync == 0x2b7) {
				gf_bs_read_int_log(bs, 11, "syncExtensionType");
				cfg->sbr_object_type = gf_bs_read_int_log(bs, 5, "extensionAudioObjectType ");
				cfg->has_sbr = gf_bs_read_int_log(bs, 1, "sbrPresentFlag");
				if (cfg->has_sbr) {
					cfg->sbr_sr_index = gf_bs_read_int_log(bs, 4, "extensionSamplingFrequencyIndex");
					if (cfg->sbr_sr_index == 0x0F) {
						cfg->sbr_sr = gf_bs_read_int_log(bs, 24, "extensionSamplingFrequency");
					}
					else {
						cfg->sbr_sr = GF_M4ASampleRates[cfg->sbr_sr_index];
					}
				}
			}
			else if (sync == 0x548) {
				gf_bs_read_int_log(bs, 11, "syncExtensionType");
				cfg->has_ps = gf_bs_read_int_log(bs, 1, "hasParametricStereo");
				if (cfg->has_ps)
					cfg->nb_chan = 1;
			}
			else {
				break;
			}
		}
	}
	cfg->audioPL = gf_m4a_get_profile(cfg);
	return GF_OK;
}

GF_EXPORT
GF_Err gf_m4a_get_config(u8 *dsi, u32 dsi_size, GF_M4ADecSpecInfo *cfg)
{
	GF_BitStream *bs;
	if (!dsi || !dsi_size || (dsi_size < 2)) return GF_NON_COMPLIANT_BITSTREAM;
	bs = gf_bs_new(dsi, dsi_size, GF_BITSTREAM_READ);
	gf_m4a_parse_config(bs, cfg, GF_TRUE);
	gf_bs_del(bs);
	return GF_OK;
}

u32 gf_latm_get_value(GF_BitStream *bs)
{
	u32 i, tmp, value = 0;
	u32 bytesForValue = gf_bs_read_int(bs, 2);
	for (i = 0; i <= bytesForValue; i++) {
		value <<= 8;
		tmp = gf_bs_read_int(bs, 8);
		value += tmp;
	}
	return value;
}

GF_EXPORT
u32 gf_m4a_get_channel_cfg(u32 nb_chan)
{
	u32 i, count = sizeof(GF_M4ANumChannels) / sizeof(u32);
	for (i = 0; i < count; i++) {
		if (GF_M4ANumChannels[i] == nb_chan) return i + 1;
	}
	return 0;
}

GF_EXPORT
GF_Err gf_m4a_write_program_config_element_bs(GF_BitStream *bs, GF_M4ADecSpecInfo *cfg)
{
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
	gf_bs_write_int(bs, cfg->mono_mixdown_present, 1);
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
	for (i = 0; i < cfg->num_assoc_data_elements; i++) {
		gf_bs_write_int(bs, cfg->assoc_data_element_tag_select[i], 4);
	}

	for (i = 0; i < cfg->num_valid_cc_elements; i++) {
		gf_bs_write_int(bs, cfg->cc_element_is_ind_sw[i], 1);
		gf_bs_write_int(bs, cfg->valid_cc_element_tag_select[i], 4);
	}
	gf_bs_align(bs);
	gf_bs_write_int(bs, cfg->comment_field_bytes, 8);
	gf_bs_write_data(bs, (char *)cfg->comments, cfg->comment_field_bytes);
	return GF_OK;
}

GF_EXPORT
GF_Err gf_m4a_write_config_bs(GF_BitStream *bs, GF_M4ADecSpecInfo *cfg)
{
	if (!cfg->base_sr_index) {
		if (!cfg->base_sr) return GF_BAD_PARAM;
		while (GF_M4ASampleRates[cfg->base_sr_index]) {
			if (GF_M4ASampleRates[cfg->base_sr_index] == cfg->base_sr)
				break;
			cfg->base_sr_index++;
		}
	}
	if (cfg->sbr_sr && !cfg->sbr_sr_index) {
		while (GF_M4ASampleRates[cfg->sbr_sr_index]) {
			if (GF_M4ASampleRates[cfg->sbr_sr_index] == cfg->sbr_sr)
				break;
			cfg->sbr_sr_index++;
		}
	}
	/*extended object type*/
	if (cfg->base_object_type >= 32) {
		gf_bs_write_int(bs, 31, 5);
		gf_bs_write_int(bs, cfg->base_object_type - 32, 6);
	}
	else {
		gf_bs_write_int(bs, cfg->base_object_type, 5);
	}
	gf_bs_write_int(bs, cfg->base_sr_index, 4);
	if (cfg->base_sr_index == 0x0F) {
		gf_bs_write_int(bs, cfg->base_sr, 24);
	}

	if (cfg->program_config_element_present) {
		gf_bs_write_int(bs, 0, 4);
	} else {
		cfg->chan_cfg = gf_m4a_get_channel_cfg(cfg->nb_chan);
		if (!cfg->chan_cfg) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_CODING, ("[AAC] Cannot write decoder config, ProgramConfigElement is missing and channel configuration is not a predefined one !\n"));
			return GF_BAD_PARAM;
		}
		gf_bs_write_int(bs, cfg->chan_cfg, 4);
	}

	if (cfg->base_object_type == 5 || cfg->base_object_type == 29) {
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
	case 42:
	{
		/*frame length flag*/
		gf_bs_write_int(bs, 0, 1);
		/*depends on core coder*/
		gf_bs_write_int(bs, 0, 1);
		/*ext flag*/
		gf_bs_write_int(bs, 0, 1);

		if (cfg->program_config_element_present) {
			gf_m4a_write_program_config_element_bs(bs, cfg);
		}

		if ((cfg->base_object_type == 6) || (cfg->base_object_type == 20)) {
			gf_bs_write_int(bs, 0, 3);
		}
	}
	break;
	}
	/*ER cfg - not supported*/

	/*implicit sbr/ps signaling not written here, cf reframe_adts*/
	return GF_OK;
}

GF_EXPORT
GF_Err gf_m4a_write_config(GF_M4ADecSpecInfo *cfg, u8 **dsi, u32 *dsi_size)
{
	GF_BitStream *bs = gf_bs_new(NULL, 0, GF_BITSTREAM_WRITE);
	gf_m4a_write_config_bs(bs, cfg);
	gf_bs_get_content(bs, dsi, dsi_size);
	gf_bs_del(bs);
	return GF_OK;
}


/*AV1 parsing*/

static u32 av1_read_ns(GF_BitStream *bs, u32 n, const char *fname)
{
	u32 v, res;
	Bool extra_bit;
	int w = (u32)(log(n) / log(2)) + 1;
	u32 m = (1 << w) - n;
	gf_assert(w < 32);
	v = gf_bs_read_int(bs, w - 1);
	if (v < m) {
		if (fname) {
			gf_bs_log(bs, w-1, fname, v);
		}
		return v;
	}
	extra_bit = gf_bs_read_int(bs, 1);
	res = (v << 1) - m + extra_bit;
	if (fname) {
		gf_bs_log(bs, w, fname, res);
	}
	return res;
}

static void av1_color_config(GF_BitStream *bs, AV1State *state)
{
	state->config->high_bitdepth = gf_bs_read_int_log(bs, 1, "high_bitdepth");
	state->bit_depth = 8;
	if (state->config->seq_profile == 2 && state->config->high_bitdepth) {
		state->config->twelve_bit = gf_bs_read_int_log(bs, 1, "twelve_bit");
		state->bit_depth = state->config->twelve_bit ? 12 : 10;
	}
	else if (state->config->seq_profile <= 2) {
		state->bit_depth = state->config->high_bitdepth ? 10 : 8;
	}

	state->config->monochrome = GF_FALSE;
	if (state->config->seq_profile == 1) {
		state->config->monochrome = GF_FALSE;
	}
	else {
		state->config->monochrome = gf_bs_read_int_log(bs, 1, "monochrome");
	}
	/*NumPlanes = mono_chrome ? 1 : 3;*/
	state->color_description_present_flag = gf_bs_read_int_log(bs, 1, "color_description_present_flag");
	if (state->color_description_present_flag) {
		state->color_primaries = gf_bs_read_int_log(bs, 8, "color_primaries");
		state->transfer_characteristics = gf_bs_read_int_log(bs, 8, "transfer_characteristics");
		state->matrix_coefficients = gf_bs_read_int_log(bs, 8, "matrix_coefficients");
	}
	else {
		state->color_primaries = 2/*CP_UNSPECIFIED*/;
		state->transfer_characteristics = 2/*TC_UNSPECIFIED*/;
		state->matrix_coefficients = 2/*MC_UNSPECIFIED*/;
	}
	if (state->config->monochrome) {
		state->color_range = gf_bs_read_int_log(bs, 1, "color_range");
		state->config->chroma_subsampling_x = GF_TRUE;
		state->config->chroma_subsampling_y = GF_TRUE;
		state->config->chroma_sample_position = 0/*CSP_UNKNOWN*/;
		state->separate_uv_delta_q = 0;
		return;
	}
	else if (state->color_primaries == 0/*CP_BT_709*/ &&
		state->transfer_characteristics == 13/*TC_SRGB*/ &&
		state->matrix_coefficients == 0/*MC_IDENTITY*/) {
		state->color_range = GF_TRUE;
		state->config->chroma_subsampling_x = GF_FALSE;
		state->config->chroma_subsampling_y = GF_FALSE;
	}
	else {
		state->config->chroma_subsampling_x = GF_FALSE;
		state->config->chroma_subsampling_y = GF_FALSE;

		state->color_range = gf_bs_read_int_log(bs, 1, "color_range");
		if (state->config->seq_profile == 0) {
			state->config->chroma_subsampling_x = GF_TRUE;
			state->config->chroma_subsampling_y = GF_TRUE;
		}
		else if (state->config->seq_profile == 1) {
			state->config->chroma_subsampling_x = GF_FALSE;
			state->config->chroma_subsampling_y = GF_FALSE;
		}
		else {
			if (state->bit_depth == 12) {
				state->config->chroma_subsampling_x = gf_bs_read_int_log(bs, 1, "chroma_subsampling_x");
				if (state->config->chroma_subsampling_x)
					state->config->chroma_subsampling_y = gf_bs_read_int_log(bs, 1, "chroma_subsampling_y");
				else
					state->config->chroma_subsampling_y = GF_FALSE;
			}
			else {
				state->config->chroma_subsampling_x = GF_TRUE;
				state->config->chroma_subsampling_y = GF_FALSE;
			}
		}
		if (state->config->chroma_subsampling_x && state->config->chroma_subsampling_y) {
			state->config->chroma_sample_position = gf_bs_read_int_log(bs, 2, "chroma_sample_position");
		}
	}
	state->separate_uv_delta_q = gf_bs_read_int_log(bs, 1, "separate_uv_delta_q");
}


static u32 av1_uvlc(GF_BitStream *bs, const char *fname)
{
	u32 res;
	u8 leadingZeros = 0;
	while (1) {
		Bool done = gf_bs_read_int(bs, 1);
		if (done)
			break;
		leadingZeros++;
		//avoid calls to bs_available or bs_is_overflow - see #2698 for poc
		if (leadingZeros >= 32) {
			return 0xFFFFFFFF;
		}
	}
	res = gf_bs_read_int(bs, leadingZeros) + ((u32)1 << leadingZeros) - 1;
	gf_bs_log(bs, 2*leadingZeros, fname, res);
	return res;
}

static void timing_info(GF_BitStream *bs, AV1State *state) {
	u32 time_scale = 0;
	u32 num_units_in_display_tick = gf_bs_read_int_log(bs, 32, "num_units_in_display_tick");
	if (num_units_in_display_tick == 0) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_CODING, ("[AV1] num_units_in_display_tick must be greater than 0.\n"));
	}
	time_scale = gf_bs_read_int_log(bs, 32, "time_scale");
	if (time_scale == 0) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_CODING, ("[AV1] time_scale must be greater than 0.\n"));
	}
	state->equal_picture_interval = gf_bs_read_int_log(bs, 1, "equal_picture_interval");
	if (state->equal_picture_interval) {
		u32 num_ticks_per_picture_minus_1 = av1_uvlc(bs, "num_ticks_per_picture_minus_1");
		state->tb_num = time_scale;
		state->tb_den = (num_ticks_per_picture_minus_1 + 1)*num_units_in_display_tick;
	}
	else {
		GF_LOG(GF_LOG_ERROR, GF_LOG_CODING, ("[AV1] VFR not supported.\n"));
		//TODO: upload num_units_in_display_tick (eq. to the POC in H264), compute delta between frames, set it as dts_inc in gf_import_aom_av1()
	}
}

static void decoder_model_info(AV1State *state, GF_BitStream *bs) {
	state->buffer_delay_length = 1 + gf_bs_read_int_log(bs, 5, "buffer_delay_length_minus1");
	gf_bs_read_int_log(bs, 32, "num_units_in_decoding_tick");
	state->buffer_removal_time_length = gf_bs_read_int_log(bs, 5, "buffer_removal_time_length");
	state->frame_presentation_time_length = 1 + gf_bs_read_int_log(bs, 5, "frame_presentation_time_length_minus1");
}

static void operating_parameters_info(GF_BitStream *bs, const u8 idx, const u8 buffer_delay_length_minus_1) {
	const u8 n = buffer_delay_length_minus_1 + 1;
	gf_bs_read_int_log(bs, n, "decoder_buffer_delay");
	gf_bs_read_int_log(bs, n, "encoder_buffer_delay");
	gf_bs_read_int_log(bs, 1, "low_delay_mode_flag");
}

static void av1_parse_sequence_header_obu(GF_BitStream *bs, AV1State *state)
{
	u8 buffer_delay_length_minus_1 = 0;
	state->frame_state.seen_seq_header = GF_TRUE;
	state->config->seq_profile = gf_bs_read_int_log(bs, 3, "seq_profile");
	state->still_picture = gf_bs_read_int_log(bs, 1, "still_picture");
	state->reduced_still_picture_header = gf_bs_read_int_log(bs, 1, "reduced_still_picture_header");
	if (state->reduced_still_picture_header) {
		//timing_info_present_flag = GF_FALSE;
		//initial_display_delay_present_flag = GF_FALSE;
		state->operating_points_count = 1;
		state->config->seq_level_idx_0 = gf_bs_read_int_log(bs, 5, "seq_level_idx_0");
	}
	else {
		u8 i = 0;
		Bool initial_display_delay_present_flag;
		Bool timing_info_present_flag = gf_bs_read_int_log(bs, 1, "timing_info_present_flag");
		if (timing_info_present_flag) {
			timing_info(bs, state);
			state->decoder_model_info_present_flag = gf_bs_read_int_log(bs, 1, "decoder_model_info_present_flag");
			if (state->decoder_model_info_present_flag) {
				decoder_model_info(state, bs);
			}
		}
		else {
			state->decoder_model_info_present_flag = GF_FALSE;
		}
		initial_display_delay_present_flag = gf_bs_read_int_log(bs, 1, "initial_display_delay_present_flag");
		state->operating_points_count = 1 + gf_bs_read_int_log(bs, 5, "operating_points_count_minus1");
		for (i = 0; i < state->operating_points_count; i++) {
			u8 seq_level_idx_i, seq_tier = 0;

			state->operating_point_idc[i] = gf_bs_read_int_log_idx(bs, 12, "operating_point_idc", i);

			seq_level_idx_i = gf_bs_read_int_log_idx(bs, 5, "seq_level_idx", i);
			if (i == 0) state->config->seq_level_idx_0 = seq_level_idx_i;

			if (seq_level_idx_i > 7) {
				seq_tier = gf_bs_read_int_log_idx(bs, 1, "seq_tier", i);
			}
			if (i == 0) state->config->seq_tier_0 = seq_tier;

			if (state->decoder_model_info_present_flag) {
				state->decoder_model_present_for_this_op[i] = gf_bs_read_int_log_idx(bs, 1, "decoder_model_present_for_this_op", i);
				if (state->decoder_model_present_for_this_op[i]) {
					operating_parameters_info(bs, i, buffer_delay_length_minus_1);
				}
			}
			else {
				state->decoder_model_present_for_this_op[i] = 0;
			}
			if (initial_display_delay_present_flag) {
				state->initial_display_delay_present_for_this_op[i] = gf_bs_read_int_log_idx(bs, 1, "initial_display_delay_present_for_this_op", i);
				if (state->initial_display_delay_present_for_this_op[i]) {
					state->initial_display_delay_minus1_for_this_op[i] = gf_bs_read_int_log_idx(bs, 4, "initial_display_delay_minus1", i);
					// the procedure to compute the initial_presentation_delay (IPD) measured in ISOBMFF samples
					// from the initial_display_delay (IDD) measured in decoded frames is a bit complex,
					// and cannot be computed until samples have been formed
					// except when the number of operating points is 1 (usual, non scalable case)
					// and for the special case where IDD = 1 (low delay encodes)
					// because that first frame is necessarily contained in the first sample, so IPD = 1
					if (state->operating_points_count == 1
						&& state->initial_display_delay_minus1_for_this_op[i] == 0) {
						state->config->initial_presentation_delay_present = GF_TRUE;
						state->config->initial_presentation_delay_minus_one = 0;
					}
				}
			}
		}
	}

	//operatingPoint = av1_choose_operating_point(bs);
	state->OperatingPointIdc = 0;//TODO: operating_point_idc[operatingPoint];

	state->frame_width_bits_minus_1 = gf_bs_read_int_log(bs, 4, "frame_width_bits_minus1");
	state->frame_height_bits_minus_1 = gf_bs_read_int_log(bs, 4, "frame_height_bits_minus1");
	state->width = gf_bs_read_int_log(bs, state->frame_width_bits_minus_1 + 1, "width_minus1") + 1;
	state->height = gf_bs_read_int_log(bs, state->frame_height_bits_minus_1 + 1, "height_minus1") + 1;
	state->sequence_width = state->width;
	state->sequence_height = state->height;
	state->frame_id_numbers_present_flag = GF_FALSE;
	if (!state->reduced_still_picture_header) {
		state->frame_id_numbers_present_flag = gf_bs_read_int_log(bs, 1, "frame_id_numbers_present_flag");
	}
	if (state->frame_id_numbers_present_flag) {
		state->delta_frame_id_length_minus_2 = gf_bs_read_int_log(bs, 4, "delta_frame_id_length_minus2");
		state->additional_frame_id_length_minus_1 = gf_bs_read_int_log(bs, 3, "additional_frame_id_length_minus1");
	}
	state->use_128x128_superblock = gf_bs_read_int_log(bs, 1, "use_128x128_superblock");
	gf_bs_read_int_log(bs, 1, "enable_filter_intra");
	gf_bs_read_int_log(bs, 1, "enable_intra_edge_filter");
	if (state->reduced_still_picture_header) {
		/*enable_interintra_compound = 0;
		enable_masked_compound = 0;
		enable_dual_filter = 0;
		enable_jnt_comp = 0;
		enable_ref_frame_mvs = 0;*/
		state->enable_warped_motion = 0;
		state->enable_order_hint = GF_FALSE;
		state->OrderHintBits = 0;
		state->seq_force_integer_mv = 2/*SELECT_INTEGER_MV*/;
		state->seq_force_screen_content_tools = 2/*SELECT_SCREEN_CONTENT_TOOLS*/;
	}
	else {
		Bool seq_choose_screen_content_tools;
		gf_bs_read_int_log(bs, 1, "enable_interintra_compound");
		gf_bs_read_int_log(bs, 1, "enable_masked_compound");
		state->enable_warped_motion = gf_bs_read_int_log(bs, 1, "enable_warped_motion");
		gf_bs_read_int_log(bs, 1, "enable_dual_filter");
		state->enable_order_hint = gf_bs_read_int_log(bs, 1, "enable_order_hint");
		if (state->enable_order_hint) {
			gf_bs_read_int_log(bs, 1, "enable_jnt_comp");
			state->enable_ref_frame_mvs = gf_bs_read_int_log(bs, 1, "enable_ref_frame_mvs");
		}
		else {
			/*enable_jnt_comp =  0*/;
			/*enable_ref_frame_mvs = 0*/;
		}
		seq_choose_screen_content_tools = gf_bs_read_int_log(bs, 1, "seq_choose_screen_content_tools");
		state->seq_force_screen_content_tools = 0;
		if (seq_choose_screen_content_tools) {
			state->seq_force_screen_content_tools = 2/*SELECT_SCREEN_CONTENT_TOOLS*/;
		}
		else {
			state->seq_force_screen_content_tools = gf_bs_read_int_log(bs, 1, "seq_force_screen_content_tools");
		}

		state->seq_force_integer_mv = 0;
		if (state->seq_force_screen_content_tools > 0) {
			const Bool seq_choose_integer_mv = gf_bs_read_int_log(bs, 1, "seq_choose_integer_mv");
			if (seq_choose_integer_mv) {
				state->seq_force_integer_mv = 2/*SELECT_INTEGER_MV*/;
			}
			else {
				state->seq_force_integer_mv = gf_bs_read_int_log(bs, 1, "seq_force_integer_mv");
			}
		}
		else {
			state->seq_force_integer_mv = 2/*SELECT_INTEGER_MV*/;
		}
		if (state->enable_order_hint) {
			u8 order_hint_bits_minus_1 = gf_bs_read_int_log(bs, 3, "order_hint_bits_minus1");
			state->OrderHintBits = order_hint_bits_minus_1 + 1;
		}
		else {
			state->OrderHintBits = 0;
		}
	}

	state->enable_superres = gf_bs_read_int_log(bs, 1, "enable_superres");
	state->enable_cdef = gf_bs_read_int_log(bs, 1, "enable_cdef");
	state->enable_restoration = gf_bs_read_int_log(bs, 1, "enable_restoration");
	av1_color_config(bs, state);
	state->film_grain_params_present = gf_bs_read_int_log(bs, 1, "film_grain_params_present");
}



#define IVF_FILE_HEADER_SIZE 32

Bool gf_media_probe_ivf(GF_BitStream *bs)
{
	u32 dw = 0;
	if (gf_bs_available(bs) < IVF_FILE_HEADER_SIZE) return GF_FALSE;

	dw = gf_bs_peek_bits(bs, 32, 0);
	if (dw != GF_4CC('D', 'K', 'I', 'F')) {
		return GF_FALSE;
	}
	return GF_TRUE;
}

GF_Err gf_media_parse_ivf_file_header(GF_BitStream *bs, u32 *width, u32 *height, u32 *codec_fourcc, u32 *timebase_num, u32 *timebase_den, u32 *num_frames)
{
	u32 dw = 0;

	if (!width || !height || !codec_fourcc || !timebase_den || !timebase_num || !num_frames) {
		gf_assert(0);
		return GF_BAD_PARAM;
	}

	if (gf_bs_available(bs) < IVF_FILE_HEADER_SIZE) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_CODING, ("[IVF] Not enough bytes available ("LLU").\n", gf_bs_available(bs)));
		return GF_NON_COMPLIANT_BITSTREAM;
	}

	dw = gf_bs_read_u32(bs);
	if (dw != GF_4CC('D', 'K', 'I', 'F')) {
		GF_LOG(GF_LOG_INFO, GF_LOG_CODING, ("[IVF] Invalid signature\n"));
		return GF_NON_COMPLIANT_BITSTREAM;
	}

	dw = gf_bs_read_u16_le(bs);
	if (dw != 0) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_CODING, ("[IVF] Wrong IVF version. 0 expected, got %u\n", dw));
		return GF_NON_COMPLIANT_BITSTREAM;
	}

	dw = gf_bs_read_u16_le(bs); //length of header in bytes
	if (dw != IVF_FILE_HEADER_SIZE) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_CODING, ("[IVF] Wrong IVF header length. Expected 32 bytes, got %u\n", dw));
		return GF_NON_COMPLIANT_BITSTREAM;
	}

	*codec_fourcc = gf_bs_read_u32(bs);

	*width = gf_bs_read_u16_le(bs);
	*height = gf_bs_read_u16_le(bs);

	*timebase_num = gf_bs_read_u32_le(bs);
	*timebase_den = gf_bs_read_u32_le(bs);

	*num_frames = gf_bs_read_u32_le(bs);
	gf_bs_read_u32_le(bs); //skip unused

	return GF_OK;
}

GF_Err gf_media_parse_ivf_frame_header(GF_BitStream *bs, u64 *frame_size, u64 *pts)
{
	if (!frame_size) return GF_BAD_PARAM;
	if (gf_bs_available(bs) < 12)
		return GF_BUFFER_TOO_SMALL;

	*frame_size = gf_bs_read_u32_le(bs);
	if (*frame_size > 256 * 1024 * 1024) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_CODING, ("[IVF] Wrong frame size %u\n", *frame_size));
		*frame_size = 0;
		return GF_NON_COMPLIANT_BITSTREAM;
	}

	*pts = gf_bs_read_u64_le(bs);

	return GF_OK;
}

GF_Err gf_vp9_parse_superframe(GF_BitStream *bs, u64 ivf_frame_size, u32 *num_frames_in_superframe, u32 frame_sizes[VP9_MAX_FRAMES_IN_SUPERFRAME], u32 *superframe_index_size)
{
	u32 byte, bytes_per_framesize;
	u64 pos = gf_bs_get_position(bs), i = 0;
	GF_Err e;

	gf_assert(bs && num_frames_in_superframe);

	/*initialize like there is no superframe*/
	memset(frame_sizes, 0, VP9_MAX_FRAMES_IN_SUPERFRAME * sizeof(frame_sizes[0]));
	*num_frames_in_superframe = 1;
	frame_sizes[0] = (u32)ivf_frame_size;
	*superframe_index_size = 0;

	e = gf_bs_seek(bs, pos + ivf_frame_size - 1);
	if (e) return e;

	gf_bs_mark_overflow(bs, GF_TRUE);

	byte = gf_bs_read_u8(bs);
	if ((byte & 0xe0) != 0xc0)
		goto exit; /*no superframe*/

	bytes_per_framesize = 1 + ((byte & 0x18) >> 3);
	*num_frames_in_superframe = (u32)(1 + (byte & 0x7));

	/*superframe_index()*/
	*superframe_index_size = 2 + bytes_per_framesize * *num_frames_in_superframe;
	gf_bs_seek(bs, pos + ivf_frame_size - *superframe_index_size);
	byte = gf_bs_read_u8(bs);
	if ((byte & 0xe0) != 0xc0) {
		e = GF_NON_COMPLIANT_BITSTREAM;
		goto exit; /*no superframe*/
	}
	frame_sizes[0] = 0;
	for (i = 0; i < *num_frames_in_superframe; ++i) {
		gf_bs_read_data(bs, (char*)(frame_sizes + i), bytes_per_framesize);
	}

exit:
	gf_bs_seek(bs, pos);

	if (gf_bs_is_overflow(bs)) e = GF_NON_COMPLIANT_BITSTREAM;

	return e;
}


static Bool vp9_frame_sync_code(GF_BitStream *bs)
{
	u8 val = gf_bs_read_int_log(bs, 8, "syncbyte1");
	if (val != 0x49)
		return GF_FALSE;

	val = gf_bs_read_int_log(bs, 8, "syncbyte2");
	if (val != 0x83)
		return GF_FALSE;

	val = gf_bs_read_int_log(bs, 8, "syncbyte3");
	if (val != 0x42)
		return GF_FALSE;

	return GF_TRUE;
}

typedef enum {
	CS_UNKNOWN = 0,
	CS_BT_601 = 1,
	CS_BT_709 = 2,
	CS_SMPTE_170 = 3,
	CS_SMPTE_240 = 4,
	CS_BT_2020 = 5,
	CS_RESERVED = 6,
	CS_RGB = 7,
} VP9_color_space;

static const int VP9_CS_to_23001_8_colour_primaries[] = { 2/*unspecified*/, 5, 1, 6, 7, 9, -1/*reserved*/, 1 };
static const int VP9_CS_to_23001_8_transfer_characteristics[] = { 2/*unspecified*/, 5, 1, 6, 7, 9, -1/*reserved*/, 13 };
static const int VP9_CS_to_23001_8_matrix_coefficients[] = { 2/*unspecified*/, 6, 1, -1, -1, 9, -1/*reserved*/, 0 };

static GF_Err vp9_color_config(GF_BitStream *bs, GF_VPConfig *vp9_cfg)
{
	VP9_color_space color_space;

	if (vp9_cfg->profile >= 2) {
		Bool ten_or_twelve_bit = gf_bs_read_int_log(bs, 1, "ten_or_twelve_bit");
		vp9_cfg->bit_depth = ten_or_twelve_bit ? 12 : 10;
	}
	else {
		vp9_cfg->bit_depth = 8;
	}

	color_space = gf_bs_read_int_log(bs, 3, "color_space");
	vp9_cfg->colour_primaries = VP9_CS_to_23001_8_colour_primaries[color_space];
	vp9_cfg->transfer_characteristics = VP9_CS_to_23001_8_transfer_characteristics[color_space];
	vp9_cfg->matrix_coefficients = VP9_CS_to_23001_8_matrix_coefficients[color_space];
	if (color_space != CS_RGB) {
		vp9_cfg->video_fullRange_flag = gf_bs_read_int_log(bs, 1, "video_fullRange_flag");
		if (vp9_cfg->profile == 1 || vp9_cfg->profile == 3) {
			u8 subsampling_x, subsampling_y, subsampling_xy_to_chroma_subsampling[2][2] = { {3, 0}, {2, 0} };
			subsampling_x = gf_bs_read_int_log(bs, 1, "subsampling_x");
			subsampling_y = gf_bs_read_int_log(bs, 1, "subsampling_x");
			vp9_cfg->chroma_subsampling = subsampling_xy_to_chroma_subsampling[subsampling_x][subsampling_y];
			Bool reserved_zero = gf_bs_read_int_log(bs, 1, "reserved_zero");
			if (reserved_zero) {
				GF_LOG(GF_LOG_ERROR, GF_LOG_CODING, ("[VP9] color config reserved zero (1) is not zero.\n"));
				return GF_NON_COMPLIANT_BITSTREAM;
			}
		}
		else {
			vp9_cfg->chroma_subsampling = 0;
		}
	}
	else {
		vp9_cfg->video_fullRange_flag = GF_TRUE;
		if (vp9_cfg->profile == 1 || vp9_cfg->profile == 3) {
			vp9_cfg->chroma_subsampling = 3;
			Bool reserved_zero = gf_bs_read_int_log(bs, 1, "reserved_zero");
			if (reserved_zero) {
				GF_LOG(GF_LOG_ERROR, GF_LOG_CODING, ("[VP9] color config reserved zero (2) is not zero.\n"));
				return GF_NON_COMPLIANT_BITSTREAM;
			}
		}
	}

	return GF_OK;
}

static void vp9_compute_image_size(int FrameWidth, int FrameHeight, int *Sb64Cols, int *Sb64Rows)
{
	int MiCols = (FrameWidth + 7) >> 3;
	int MiRows = (FrameHeight + 7) >> 3;
	*Sb64Cols = (MiCols + 7) >> 3;
	*Sb64Rows = (MiRows + 7) >> 3;
}

static void vp9_frame_size(GF_BitStream *bs, int *FrameWidth, int *FrameHeight, int *Sb64Cols, int *Sb64Rows)
{
	int frame_width_minus_1 = gf_bs_read_int_log(bs, 16, "frame_width_minus_1");
	int frame_height_minus_1 = gf_bs_read_int_log(bs, 16, "frame_height_minus_1");
	if (frame_width_minus_1 + 1 != *FrameWidth || frame_height_minus_1 + 1 != *FrameHeight) {
		if (*FrameWidth || *FrameHeight)
			GF_LOG(GF_LOG_WARNING, GF_LOG_CODING, ("[VP9] inconsistent frame dimensions: previous was %dx%d, new one is %dx%d.\n", *FrameWidth, *FrameHeight, frame_width_minus_1 + 1, frame_height_minus_1 + 1));
	}
	*FrameWidth = frame_width_minus_1 + 1;
	*FrameHeight = frame_height_minus_1 + 1;
	vp9_compute_image_size(*FrameWidth, *FrameHeight, Sb64Cols, Sb64Rows);
}

static void vp9_render_size(GF_BitStream *bs, int FrameWidth, int FrameHeight, int *renderWidth, int *renderHeight)
{
	Bool render_and_frame_size_different = gf_bs_read_int_log(bs, 1, "render_and_frame_size_different");
	if (render_and_frame_size_different == 1) {
		int render_width_minus_1 = gf_bs_read_int_log(bs, 16, "render_width_minus_1");
		int render_height_minus_1 = gf_bs_read_int_log(bs, 16, "render_height_minus_1");
		*renderWidth = render_width_minus_1 + 1;
		*renderHeight = render_height_minus_1 + 1;
	}
	else {
		*renderWidth = FrameWidth;
		*renderHeight = FrameHeight;
	}
}

static s64 vp9_s(GF_BitStream *bs, int n, const char *fname, u32 idx) {
	s64 value = gf_bs_read_int(bs, n);
	Bool sign = gf_bs_read_int(bs, 1);
	if (sign) value = -value;
	gf_bs_log_idx(bs, n+1, fname, value, idx, -1, -1);
	return value;
}

static void vp9_loop_filter_params(GF_BitStream *bs)
{
	/*loop_filter_level = */gf_bs_read_int_log(bs, 6, "loop_filter_level");
	/*loop_filter_sharpness = */gf_bs_read_int_log(bs, 3, "loop_filter_sharpness");
	Bool loop_filter_delta_enabled = gf_bs_read_int_log(bs, 1, "loop_filter_delta_enabled");
	if (loop_filter_delta_enabled == 1) {
		Bool loop_filter_delta_update = gf_bs_read_int_log(bs, 1, "loop_filter_delta_update");
		if (loop_filter_delta_update == GF_TRUE) {
			int i;
			for (i = 0; i < 4; i++) {
				Bool update_ref_delta = gf_bs_read_int_log_idx(bs, 1, "update_ref_delta", i);
				if (update_ref_delta == GF_TRUE)
					vp9_s(bs, 6, "loop_filter_ref_deltas", i);
			}
			for (i = 0; i < 2; i++) {
				Bool update_mode_delta = gf_bs_read_int_log_idx(bs, 1, "update_mode_delta", i);
				if (update_mode_delta == GF_TRUE)
					vp9_s(bs, 6, "loop_filter_mode_deltas", i);
			}
		}
	}
}

static void vp9_delta_q(GF_BitStream *bs) {
	Bool delta_coded = gf_bs_read_int_log(bs, 1, "delta_coded");
	if (delta_coded) {
		gf_bs_read_int_log(bs, 4, "delta_q");
	}
}

static void vp9_quantization_params(GF_BitStream *bs)
{
	/*base_q_idx = */gf_bs_read_int_log(bs, 8, "base_q_idx");
	vp9_delta_q(bs); // delta_q_y_dc
	vp9_delta_q(bs); // delta_q_uv_dc
	vp9_delta_q(bs); // delta_q_uv_ac
}

#define VP9_MAX_SEGMENTS 8
#define VP9_SEG_LVL_MAX 4
static const int segmentation_feature_bits[VP9_SEG_LVL_MAX] = { 8, 6, 2, 0 };
static const int segmentation_feature_signed[VP9_SEG_LVL_MAX] = { 1, 1, 0, 0 };

#define VP9_MIN_TILE_WIDTH_B64 4
#define VP9_MAX_TILE_WIDTH_B64 64

static void vp9_read_prob(GF_BitStream *bs)
{
	Bool prob_coded = gf_bs_read_int_log(bs, 1, "prob_coded");
	if (prob_coded) {
		gf_bs_read_int_log(bs, 8, "prob");
	}
}

static void vp9_segmentation_params(GF_BitStream *bs)
{
	Bool segmentation_enabled = gf_bs_read_int_log(bs, 1, "segmentation_enabled");
	if (segmentation_enabled == 1) {
		int i;
		Bool segmentation_update_map = gf_bs_read_int_log(bs, 1, "segmentation_update_map");
		if (segmentation_update_map) {
			for (i = 0; i < 7; i++) {
				vp9_read_prob(bs);
			}
			Bool segmentation_temporal_update = gf_bs_read_int_log(bs, 1, "segmentation_temporal_update");
			for (i = 0; i < 3; i++) {
				if (segmentation_temporal_update) {
					vp9_read_prob(bs);
				}
			}
		}
		Bool segmentation_update_data = gf_bs_read_int_log(bs, 1, "segmentation_update_data");
		if (segmentation_update_data == 1) {
			/*segmentation_abs_or_delta_update =*/ gf_bs_read_int_log(bs, 1, "segmentation_abs_or_delta_update");
			for (i = 0; i < VP9_MAX_SEGMENTS; i++) {
				int j;
				for (j = 0; j < VP9_SEG_LVL_MAX; j++) {
					/*feature_value = 0*/
					Bool feature_enabled = gf_bs_read_int_log(bs, 1, "feature_enabled");
					/*FeatureEnabled[i][j] = feature_enabled*/
					if (feature_enabled) {
						int bits_to_read = segmentation_feature_bits[j];
						/*feature_value =*/ gf_bs_read_int_log(bs, bits_to_read, "feature_value");
						if (segmentation_feature_signed[j] == 1) {
							/*Bool feature_sign = */gf_bs_read_int_log(bs, 1, "feature_sign");
							/*if (feature_sign == 1)
								feature_value *= -1*/
						}
					}
					/*FeatureData[i][j] = feature_value*/
				}
			}
		}
	}
}

static int calc_min_log2_tile_cols(int Sb64Cols) {
	int minLog2 = 0;
	while ((VP9_MAX_TILE_WIDTH_B64 << minLog2) < Sb64Cols)
		minLog2++;

	return minLog2;
}

static int calc_max_log2_tile_cols(int Sb64Cols) {
	int maxLog2 = 1;
	while ((Sb64Cols >> maxLog2) >= VP9_MIN_TILE_WIDTH_B64)
		maxLog2++;

	return maxLog2 - 1;
}

static void vp9_tile_info(GF_BitStream *bs, int Sb64Cols)
{
	Bool tile_rows_log2;
	int minLog2TileCols = calc_min_log2_tile_cols(Sb64Cols);
	int maxLog2TileCols = calc_max_log2_tile_cols(Sb64Cols);
	int tile_cols_log2 = minLog2TileCols;
	while (tile_cols_log2 < maxLog2TileCols) {
		Bool increment_tile_cols_log2 = gf_bs_read_int_log(bs, 1, "increment_tile_cols_log2");
		if (increment_tile_cols_log2)
			tile_cols_log2++;
		else
			break;
	}
	tile_rows_log2 = gf_bs_read_int_log(bs, 1, "tile_rows_log2");
	if (tile_rows_log2) {
		/*Bool increment_tile_rows_log2 = */gf_bs_read_int_log(bs, 1, "increment_tile_rows_log2");
		//tile_rows_log2 += increment_tile_rows_log2;
	}
}

static void vp9_frame_size_with_refs(GF_BitStream *bs, u8 refresh_frame_flags, u8 * ref_frame_idx, int * RefFrameWidth, int *RefFrameHeight,
	int *FrameWidth, int *FrameHeight, int *RenderWidth, int *RenderHeight, int *Sb64Cols, int *Sb64Rows)
{
	Bool found_ref;
	int i;
	for (i = 0; i < 3; i++) {
		found_ref = gf_bs_read_int_log(bs, 1, "found_ref");
		if (found_ref) {
			*FrameWidth  = RefFrameWidth [ref_frame_idx[i]];
			*FrameHeight = RefFrameHeight[ref_frame_idx[i]];
			break;
		}
	}
	if (found_ref == 0) {
		vp9_frame_size(bs, FrameWidth, FrameHeight, Sb64Cols, Sb64Rows);
	}
	else {
		vp9_compute_image_size(*FrameWidth, *FrameHeight, Sb64Cols, Sb64Rows);
	}

	vp9_render_size(bs, *FrameWidth, *FrameHeight, RenderWidth, RenderHeight);
}

static void vp9_read_interpolation_filter(GF_BitStream *bs)
{
	Bool is_filter_switchable = gf_bs_read_int_log(bs, 1, "is_filter_switchable");
	if (!is_filter_switchable) {
		/*raw_interpolation_filter = */gf_bs_read_int_log(bs, 2, "raw_interpolation_filter");
	}
}


#define VP9_KEY_FRAME 0

GF_Err gf_vp9_parse_sample(GF_BitStream *bs, GF_VPConfig *vp9_cfg, Bool *key_frame, u32 *FrameWidth, u32 *FrameHeight, u32 *renderWidth, u32 *renderHeight)
{
	Bool FrameIsIntra = GF_FALSE, profile_low_bit, profile_high_bit, show_existing_frame = GF_FALSE, frame_type = GF_FALSE, show_frame = GF_FALSE, error_resilient_mode = GF_FALSE;
	/*u8 frame_context_idx = 0, reset_frame_context = 0, frame_marker = 0*/;
	int Sb64Cols = 0, Sb64Rows = 0, i;
	u8 refresh_frame_flags = 0;

	gf_assert(bs && key_frame);

	/*uncompressed header*/
	/*frame_marker = */gf_bs_read_int_log(bs, 2, "frame_marker");
	profile_low_bit = gf_bs_read_int_log(bs, 1, "profile_low_bit");
	profile_high_bit = gf_bs_read_int_log(bs, 1, "profile_high_bit");
	vp9_cfg->profile = (profile_high_bit << 1) + profile_low_bit;
	if (vp9_cfg->profile == 3) {
		Bool reserved_zero = gf_bs_read_int_log(bs, 1, "reserved_zero");
		if (reserved_zero) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_CODING, ("[VP9] uncompressed header reserved zero is not zero.\n"));
			return GF_NON_COMPLIANT_BITSTREAM;
		}
	}

	show_existing_frame = gf_bs_read_int_log(bs, 1, "show_existing_frame");
	if (show_existing_frame == GF_TRUE) {
		/*frame_to_show_map_idx = */gf_bs_read_int_log(bs, 3, "frame_to_show_map_idx");
		return GF_OK;
	}

	frame_type = gf_bs_read_int_log(bs, 1, "frame_type");
	show_frame = gf_bs_read_int_log(bs, 1, "show_frame");
	error_resilient_mode = gf_bs_read_int_log(bs, 1, "error_resilient_mode");
	if (frame_type == VP9_KEY_FRAME) {
		if (!vp9_frame_sync_code(bs))
			return GF_NON_COMPLIANT_BITSTREAM;
		if (vp9_color_config(bs, vp9_cfg) != GF_OK)
			return GF_NON_COMPLIANT_BITSTREAM;
		vp9_frame_size(bs, FrameWidth, FrameHeight, &Sb64Cols, &Sb64Rows);
		vp9_render_size(bs, *FrameWidth, *FrameHeight, renderWidth, renderHeight);
		refresh_frame_flags = 0xFF;
		*key_frame = GF_TRUE;
		FrameIsIntra = GF_TRUE;
	}
	else {
		Bool intra_only = GF_FALSE;
		*key_frame = GF_FALSE;

		if (show_frame == GF_FALSE) {
			intra_only = gf_bs_read_int_log(bs, 1, "intra_only");
		}
		FrameIsIntra = intra_only;

		if (error_resilient_mode == GF_FALSE) {
			/*reset_frame_context = */gf_bs_read_int_log(bs, 2, "reset_frame_context");
		}

		if (intra_only == GF_TRUE) {
			if (!vp9_frame_sync_code(bs))
				return GF_NON_COMPLIANT_BITSTREAM;

			if (vp9_cfg->profile > 0) {
				if (vp9_color_config(bs, vp9_cfg) != GF_OK)
					return GF_NON_COMPLIANT_BITSTREAM;
			}
			else {
				u8 color_space = CS_BT_601;
				vp9_cfg->colour_primaries = VP9_CS_to_23001_8_colour_primaries[color_space];
				vp9_cfg->transfer_characteristics = VP9_CS_to_23001_8_transfer_characteristics[color_space];
				vp9_cfg->matrix_coefficients = VP9_CS_to_23001_8_matrix_coefficients[color_space];
				vp9_cfg->chroma_subsampling = 0;
				vp9_cfg->bit_depth = 8;
			}
			refresh_frame_flags = gf_bs_read_int_log(bs, 8, "refresh_frame_flags");
			vp9_frame_size(bs, FrameWidth, FrameHeight, &Sb64Cols, &Sb64Rows);
			vp9_render_size(bs, *FrameWidth, *FrameHeight, renderWidth, renderHeight);
		}
		else {
			refresh_frame_flags = gf_bs_read_int_log(bs, 8, "refresh_frame_flags");
			u8 ref_frame_idx[3];
			for (i = 0; i < 3; i++) {
				ref_frame_idx[i] = gf_bs_read_int_log_idx(bs, 3, "ref_frame_idx", i);
				/*ref_frame_sign_bias[LAST_FRAME + i] = */gf_bs_read_int_log_idx(bs, 1, "ref_frame_sign_bias", i);
			}
			vp9_frame_size_with_refs(bs, refresh_frame_flags, ref_frame_idx, vp9_cfg->RefFrameWidth, vp9_cfg->RefFrameHeight, FrameWidth, FrameHeight, renderWidth, renderHeight, &Sb64Cols, &Sb64Rows);
			/*allow_high_precision_mv = */gf_bs_read_int_log(bs, 1, "allow_high_precision_mv");
			vp9_read_interpolation_filter(bs);
		}
	}

	if (error_resilient_mode == 0) {
		/*refresh_frame_context = */gf_bs_read_int_log(bs, 1, "refresh_frame_context");
		/*frame_parallel_decoding_mode = */gf_bs_read_int_log(bs, 1, "frame_parallel_decoding_mode");
	}

	/*frame_context_idx = */gf_bs_read_int_log(bs, 2, "frame_context_idx");
	if (FrameIsIntra || error_resilient_mode) {
		/*setup_past_independence + save_probs ...*/
		//frame_context_idx = 0;
	}

	vp9_loop_filter_params(bs);
	vp9_quantization_params(bs);
	vp9_segmentation_params(bs);
	vp9_tile_info(bs, Sb64Cols);

	/*header_size_in_bytes = */gf_bs_read_int_log(bs, 16, "header_size_in_bytes");

	/*Reference frame update process (8.10 - partial)*/
	for (i = 0; i < VP9_NUM_REF_FRAMES; i++) {
		if ((refresh_frame_flags >> i) & 1) {
			vp9_cfg->RefFrameWidth[i] = *FrameWidth;
			vp9_cfg->RefFrameHeight[i] = *FrameHeight;
		}
	}

	return GF_OK;
}

GF_EXPORT
GF_Err gf_av1_parse_obu_header(GF_BitStream *bs, ObuType *obu_type, Bool *obu_extension_flag, Bool *obu_has_size_field, u8 *temporal_id, u8 *spatial_id)
{
	Bool forbidden = gf_bs_read_int(bs, 1);
	if (forbidden) {
		return GF_NON_COMPLIANT_BITSTREAM;
	}

	*obu_type = gf_bs_read_int(bs, 4);
	*obu_extension_flag = gf_bs_read_int(bs, 1);
	*obu_has_size_field = gf_bs_read_int(bs, 1);
	if (gf_bs_read_int(bs, 1) /*obu_reserved_1bit*/) {
		return GF_NON_COMPLIANT_BITSTREAM;
	}
	if (*obu_extension_flag) {
		*temporal_id = gf_bs_read_int(bs, 3);
		*spatial_id = gf_bs_read_int(bs, 2);
		/*extension_header_reserved_3bits = */gf_bs_read_int(bs, 3);
	}

	return GF_OK;
}

#endif // GPAC_DISABLE_AV_PARSERS

GF_EXPORT
const char *gf_av1_get_obu_name(ObuType obu_type)
{
	switch (obu_type) {
	case OBU_SEQUENCE_HEADER: return "seq_header";
	case OBU_TEMPORAL_DELIMITER: return "delimiter";
	case OBU_FRAME_HEADER: return "frame_header";
	case OBU_TILE_GROUP: return "tile_group";
	case OBU_METADATA: return "metadata";
	case OBU_FRAME: return "frame";
	case OBU_REDUNDANT_FRAME_HEADER: return "redundant_frame_header";
	case OBU_TILE_LIST: return "tile_list";
	case OBU_PADDING: return "padding";
	case OBU_RESERVED_0:
	case OBU_RESERVED_9:
	case OBU_RESERVED_10:
	case OBU_RESERVED_11:
	case OBU_RESERVED_12:
	case OBU_RESERVED_13:
	case OBU_RESERVED_14:
		return "reserved";
	default: return "unknown";
	}
}

Bool av1_is_obu_header(ObuType obu_type) {
	switch (obu_type) {
	case OBU_SEQUENCE_HEADER:
	case OBU_METADATA:
		// TODO add check based on the metadata type
		return GF_TRUE;
	default:
		return GF_FALSE;
	}
}

#ifndef GPAC_DISABLE_AV_PARSERS

static Bool av1_is_obu_frame(AV1State *state, ObuType obu_type)
{
	switch (obu_type) {
	case OBU_PADDING:
	case OBU_REDUNDANT_FRAME_HEADER:
		return GF_FALSE;
	case OBU_TEMPORAL_DELIMITER:
		return state->keep_temporal_delim ? GF_TRUE : GF_FALSE;
	default:
		return GF_TRUE;
	}
}

GF_EXPORT
u64 gf_av1_leb128_read(GF_BitStream *bs, u8 *opt_Leb128Bytes) {
	u64 value = 0;
	u8 Leb128Bytes = 0, i = 0;
	for (i = 0; i < 8; i++) {
		u8 leb128_byte = gf_bs_read_u8(bs);
		value |= ( ((u64) (leb128_byte & 0x7f)) << (i * 7));
		Leb128Bytes += 1;
		if (!(leb128_byte & 0x80)) {
			break;
		}
	}

	if (opt_Leb128Bytes) {
		*opt_Leb128Bytes = Leb128Bytes;
	}
	return value;
}

u32 gf_av1_leb128_size(u64 value)
{
	u32 gf_av1_leb128_size = 0;
	do {
		++gf_av1_leb128_size;
	} while ((value >>= 7) != 0);

	return gf_av1_leb128_size;
}

u64 gf_av1_leb128_write(GF_BitStream *bs, u64 value)
{
	u32 i, leb_size = gf_av1_leb128_size(value);
	for (i = 0; i < leb_size; ++i) {
		u8 byte = value & 0x7f;
		value >>= 7;
		if (value != 0) byte |= 0x80; //more bytes follow
		gf_bs_write_u8(bs, byte);
	}

	return leb_size;
}

#define OBU_BLOCK_SIZE 4096
static void av1_add_obu_internal(GF_BitStream *bs, u64 pos, u64 obu_length, ObuType obu_type, GF_List **obu_list, AV1State *state)
{
	char block[OBU_BLOCK_SIZE];
	Bool has_size_field = 0, obu_extension_flag = 0;
	u8 temporal_id, spatial_id;
	GF_AV1_OBUArrayEntry *a = NULL;

	if (state && state->mem_mode) {
		if (!state->bs) {
			state->bs = gf_bs_new(NULL, 0, GF_BITSTREAM_WRITE);
		} else {
			gf_bs_reassign_buffer(state->bs, state->frame_obus, state->frame_obus_alloc);
			//make sure we don't attempt at freeing this buffer while assigned to the bitstream - cf gf_av1_reset_state
			state->frame_obus = NULL;
		}
	}
	else {
		GF_SAFEALLOC(a, GF_AV1_OBUArrayEntry);
		if (!a) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_CODING, ("[AV1] Failed to allocate OBU\n"));
			return;
		}
	}

	gf_bs_seek(bs, pos);
	gf_av1_parse_obu_header(bs, &obu_type, &obu_extension_flag, &has_size_field, &temporal_id, &spatial_id);
	gf_bs_seek(bs, pos);

	if (has_size_field) {
		if (a) {
			a->obu = gf_malloc((size_t)obu_length);
			gf_bs_read_data(bs, a->obu, (u32)obu_length);
			a->obu_length = obu_length;
		}
		else {
			u32 remain = (u32)obu_length;
			while (remain) {
				u32 block_size = OBU_BLOCK_SIZE;
				if (block_size > remain) block_size = remain;
				gf_bs_read_data(bs, block, block_size);
				gf_bs_write_data(state->bs, block, block_size);
				remain -= block_size;
			}
			return;
		}
	}
	else {
		u8 i, hdr_size = obu_extension_flag ? 2 : 1;
		const u32 leb_size = (u32)gf_av1_leb128_size(obu_length);
		const u64 obu_size = obu_length - hdr_size;

		if (a) {
			a->obu = gf_malloc((size_t)obu_length + leb_size);
			a->obu_length = obu_length + leb_size;
			for (i = 0; i < hdr_size; ++i) {
				a->obu[i] = gf_bs_read_u8(bs);
				/*add size field flag*/
				if (i == 0) a->obu[0] |= 0x02;
			}
			{
				u32 out_size = 0;
				u8 *output = NULL;
				GF_BitStream *bsLeb128 = gf_bs_new(NULL, 0, GF_BITSTREAM_WRITE);
				/*write size field*/
				gf_av1_leb128_write(bsLeb128, obu_size);
				gf_assert(gf_bs_get_position(bsLeb128) == leb_size);
				gf_bs_get_content(bsLeb128, &output, &out_size);
				gf_bs_del(bsLeb128);
				memcpy(a->obu + hdr_size, output, out_size);
				gf_free(output);
			}
			gf_bs_read_data(bs, a->obu + hdr_size + leb_size, (u32)(obu_size));
			gf_assert(gf_bs_get_position(bs) == pos + obu_length);
		}
		else {
			u32 remain;
			for (i = 0; i < hdr_size; ++i) {
				u8 hdr_b = gf_bs_read_u8(bs);
				if (i == 0) hdr_b |= 0x02; /*add size field flag*/
				gf_bs_write_u8(state->bs, hdr_b);
			}
			/*add size field */
			gf_av1_leb128_write(state->bs, obu_size);
			remain = (u32)obu_length - hdr_size;
			while (remain) {
				u32 block_size = OBU_BLOCK_SIZE;
				if (block_size > remain) block_size = remain;
				gf_bs_read_data(bs, block, block_size);
				gf_bs_write_data(state->bs, block, block_size);
				remain -= block_size;
			}
			gf_assert(gf_bs_get_position(bs) == pos + obu_length);
			return;
		}
	}
	if (!obu_list) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_CODING, ("[AV1] internal error, no OBU list cannot add\n"));
		gf_free(a->obu);
		gf_free(a);
		return;
	}
	a->obu_type = obu_type;
	if (! *obu_list)
		*obu_list = gf_list_new();
	gf_list_add(*obu_list, a);
}

static void av1_populate_state_from_obu(GF_BitStream *bs, u64 pos, u64 obu_length, ObuType obu_type, AV1State *state)
{
	if (av1_is_obu_header(obu_type)) {
		av1_add_obu_internal(bs, pos, obu_length, obu_type, &state->frame_state.header_obus, NULL);
	}
	if (!state->skip_frames && av1_is_obu_frame(state, obu_type)) {
		if (!state->mem_mode) {
			av1_add_obu_internal(bs, pos, obu_length, obu_type, &state->frame_state.frame_obus, NULL);
		}
		else {
			av1_add_obu_internal(bs, pos, obu_length, obu_type, NULL, state);
		}
	}
}

GF_Err aom_av1_parse_temporal_unit_from_section5(GF_BitStream *bs, AV1State *state)
{
	if (!state) return GF_BAD_PARAM;
	state->has_temporal_delim = 0;
	Bool first_obu = GF_TRUE;

	while (1) {
		GF_Err e;
		if (!gf_bs_available(bs))
			return state->unframed ? GF_BUFFER_TOO_SMALL : GF_OK;

		u64 pos = gf_bs_get_position(bs), obu_size = 0;

		e = gf_av1_parse_obu(bs, &state->obu_type, &obu_size, NULL, state);
		if (e)
			return e;

		if (obu_size != gf_bs_get_position(bs) - pos) {
			GF_LOG(GF_LOG_WARNING, GF_LOG_CODING, ("[AV1] OBU (Section 5) frame size "LLU" different from consumed bytes "LLU".\n", obu_size, gf_bs_get_position(bs) - pos));
			return GF_NON_COMPLIANT_BITSTREAM;
		}

		if (state->obu_type == OBU_TEMPORAL_DELIMITER) {
			if (!first_obu) {
				// seek back
				gf_bs_seek(bs, pos);
				break;
			}
			state->has_temporal_delim = 1;
		}
		first_obu = GF_FALSE;

		GF_LOG(GF_LOG_DEBUG, GF_LOG_CODING, ("[AV1] Section5 OBU detected (size "LLU")\n", obu_size));
		av1_populate_state_from_obu(bs, pos, obu_size, state->obu_type, state);
	}

	return GF_OK;
}

Bool gf_media_aom_probe_annexb(GF_BitStream *bs)
{
	Bool res = GF_TRUE;
	u64 init_pos = gf_bs_get_position(bs);
	u64 sz = gf_av1_leb128_read(bs, NULL);
	if (!sz) res = GF_FALSE;
	while (sz > 0) {
		u8 Leb128Bytes = 0;
		u64 frame_unit_size = gf_av1_leb128_read(bs, &Leb128Bytes);

		if (!frame_unit_size) {
			res = GF_FALSE;
			break;
		}

		if (sz < Leb128Bytes + frame_unit_size) {
			res = GF_FALSE;
			break;
		}
		sz -= Leb128Bytes + frame_unit_size;

		while (frame_unit_size > 0) {
			ObuType obu_type;
			u64 pos, obu_length = gf_av1_leb128_read(bs, &Leb128Bytes);
			if (frame_unit_size < Leb128Bytes + obu_length) {
				res = GF_FALSE;
				break;
			}
			pos = gf_bs_get_position(bs);
			frame_unit_size -= Leb128Bytes;

			u8 tid, sid;
			Bool extflag, has_size;
			GF_Err e = gf_av1_parse_obu_header(bs, &obu_type, &extflag, &has_size, &tid, &sid);
			if (e) {
				res = GF_FALSE;
				break;
			}

			if (has_size) {
				obu_length = (u32)gf_av1_leb128_read(bs, NULL);
			}
			else {
				if (obu_length >= 1 + extflag) {
					obu_length = obu_length - 1 - extflag;
				}
				else {
					res = GF_FALSE;
					break;
				}
			}
			u32 hdr_size = (u32)(gf_bs_get_position(bs) - pos);
			obu_length += hdr_size;

			if (frame_unit_size < obu_length) {
				res = GF_FALSE;
				break;
			}
			frame_unit_size -= obu_length;
			gf_bs_skip_bytes(bs, obu_length - hdr_size);
		}
		if (!res) break;
	}
	gf_bs_seek(bs, init_pos);
	return res;
}

GF_Err aom_av1_parse_temporal_unit_from_annexb(GF_BitStream *bs, AV1State *state)
{
	GF_Err e;
	u64 tupos;
	u64 tusize, sz;
	if (!bs || !state) return GF_BAD_PARAM;

	gf_bs_mark_overflow(bs, GF_TRUE);
	tusize = sz = gf_av1_leb128_read(bs, NULL);
	tupos = gf_bs_get_position(bs);
	if (!sz) {
		GF_LOG(GF_LOG_INFO, GF_LOG_CODING, ("[AV1] temporal unit size is 0, likely not annex B\n"));
		return GF_NON_COMPLIANT_BITSTREAM;
	}

	GF_LOG(GF_LOG_DEBUG, GF_LOG_CODING, ("[AV1] Annex B temporal unit detected (size "LLU") ***** \n", sz));
	while (sz > 0) {
		u8 Leb128Bytes = 0;
		u64 frame_unit_size = gf_av1_leb128_read(bs, &Leb128Bytes);

		if (gf_bs_is_overflow(bs)) {
			return GF_BUFFER_TOO_SMALL;
		}
		if (sz < Leb128Bytes + frame_unit_size) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_CODING, ("[AV1] Annex B sz("LLU") < Leb128Bytes("LLU") + frame_unit_size("LLU")\n", sz, Leb128Bytes, frame_unit_size));
			return GF_NON_COMPLIANT_BITSTREAM;
		}
		GF_LOG(GF_LOG_DEBUG, GF_LOG_CODING, ("[AV1] Annex B frame unit detected (size "LLU")\n", frame_unit_size));
		sz -= Leb128Bytes + frame_unit_size;

		while (frame_unit_size > 0) {
			u64 pos, obu_size = gf_av1_leb128_read(bs, &Leb128Bytes);

			if (gf_bs_is_overflow(bs)) {
				return GF_BUFFER_TOO_SMALL;
			}
			if (frame_unit_size < Leb128Bytes + obu_size) {
				GF_LOG(GF_LOG_ERROR, GF_LOG_CODING, ("[AV1] Annex B frame_unit_size("LLU") < Leb128Bytes("LLU") + obu_length("LLU")\n", frame_unit_size, Leb128Bytes, obu_size));
				return GF_NON_COMPLIANT_BITSTREAM;
			}
			GF_LOG(GF_LOG_DEBUG, GF_LOG_CODING, ("[AV1] Annex B OBU detected (size "LLU")\n", obu_size));
			pos = gf_bs_get_position(bs);
			frame_unit_size -= Leb128Bytes;

			e = gf_av1_parse_obu(bs, &state->obu_type, &obu_size, NULL, state);
			if (e) return e;

			if (obu_size != gf_bs_get_position(bs) - pos) {
				GF_LOG(GF_LOG_WARNING, GF_LOG_CODING, ("[AV1] Annex B frame size "LLU" different from consumed bytes "LLU".\n", obu_size, gf_bs_get_position(bs) - pos));
				return GF_NON_COMPLIANT_BITSTREAM;
			}

			av1_populate_state_from_obu(bs, pos, obu_size, state->obu_type, state);
			if (frame_unit_size < obu_size) {
				GF_LOG(GF_LOG_ERROR, GF_LOG_CODING, ("[AV1] Annex B frame_unit_size("LLU") < OBU size ("LLU")\n", frame_unit_size, obu_size));
				return GF_NON_COMPLIANT_BITSTREAM;
			}
			frame_unit_size -= obu_size;
		}
	}
	gf_assert(sz == 0);
	if (tusize != gf_bs_get_position(bs) - tupos) {
		GF_LOG(GF_LOG_WARNING, GF_LOG_CODING, ("[AV1] Annex B TU size "LLU" different from consumed bytes "LLU".\n", tusize, gf_bs_get_position(bs) - tupos));
		return GF_NON_COMPLIANT_BITSTREAM;
	}
	return GF_OK;
}

GF_Err aom_av1_parse_temporal_unit_from_ivf(GF_BitStream *bs, AV1State *state)
{
	u64 frame_size, pts_ignored;
	GF_Err e;
	if (gf_bs_available(bs)<12) return GF_EOS;
	e = gf_media_parse_ivf_frame_header(bs, &frame_size, &pts_ignored);
	if (e) return e;
	GF_LOG(GF_LOG_DEBUG, GF_LOG_CODING, ("[AV1] IVF frame detected (size "LLU")\n", frame_size));

	if (gf_bs_available(bs) < frame_size) return GF_EOS;

	while (frame_size > 0) {
		u64 obu_size = 0, pos = gf_bs_get_position(bs);

		e = gf_av1_parse_obu(bs, &state->obu_type, &obu_size, NULL, state);
		if (e != GF_OK)
			return e;

		if (obu_size != gf_bs_get_position(bs) - pos) {
			GF_LOG(GF_LOG_WARNING, GF_LOG_CODING, ("[AV1] IVF frame size "LLU" different from consumed bytes "LLU".\n", obu_size, gf_bs_get_position(bs) - pos));
			return GF_NON_COMPLIANT_BITSTREAM;
		}

		av1_populate_state_from_obu(bs, pos, obu_size, state->obu_type, state);

		frame_size -= obu_size;
	}
	return GF_OK;
}

#define AV1_NUM_REF_FRAMES 8
#define AV1_ALL_FRAMES ((1 << AV1_NUM_REF_FRAMES) - 1)

#define AV1_SUPERRES_DENOM_MIN 9
#define AV1_SUPERRES_DENOM_BITS 3
#define AV1_SUPERRES_NUM 8

#define AV1_REFS_PER_FRAME 7
#define AV1_PRIMARY_REF_NONE 7

#define MAX_TILE_WIDTH 4096
#define MAX_TILE_AREA (4096 * 2304)

static u32 aom_av1_tile_log2(u32 blkSize, u32 target)
{
	u32 k;
	for (k = 0; (blkSize << k) < target; k++) {
	}
	return k;
}

static u64 aom_av1_le(GF_BitStream *bs, u32 n, const char *name) {
	u32 i = 0;
	u64 t = 0;
	for (i = 0; i < n; i++) {
		u32 byte = gf_bs_read_int(bs, 8);
		t += (byte << (i * 8));
	}
	gf_bs_log(bs, n*8, name, t);
	return t;
}


static void av1_parse_tile_info(GF_BitStream *bs, AV1State *state)
{
	u32 i;
	u32 MiCols = 2 * ((state->width + 7) >> 3);
	u32 MiRows = 2 * ((state->height + 7) >> 3);
	u32 sbCols = state->use_128x128_superblock ? ((MiCols + 31) >> 5) : ((MiCols + 15) >> 4);
	u32 sbRows = state->use_128x128_superblock ? ((MiRows + 31) >> 5) : ((MiRows + 15) >> 4);
	u32 sbShift = state->use_128x128_superblock ? 5 : 4;
	u32 sbSize = sbShift + 2;
	u32 maxTileWidthSb = MAX_TILE_WIDTH >> sbSize;
	u32 maxTileAreaSb = MAX_TILE_AREA >> (2 * sbSize);
	u32 minLog2tileCols = aom_av1_tile_log2(maxTileWidthSb, sbCols);
	u32 maxLog2tileCols = aom_av1_tile_log2(1, MIN(sbCols, AV1_MAX_TILE_COLS));
	u32 maxLog2tileRows = aom_av1_tile_log2(1, MIN(sbRows, AV1_MAX_TILE_ROWS));
	u32 minLog2Tiles = MAX(minLog2tileCols, aom_av1_tile_log2(maxTileAreaSb, sbRows * sbCols));
	Bool uniform_tile_spacing_flag = gf_bs_read_int_log(bs, 1, "uniform_tile_spacing_flag");
	if (uniform_tile_spacing_flag) {
		u32 startSb, tileWidthSb, tileHeightSb, minLog2tileRows;
		state->tileColsLog2 = minLog2tileCols;
		while (state->tileColsLog2 < maxLog2tileCols) {
			Bool increment_tile_cols_log2 = gf_bs_read_int_log(bs, 1, "increment_tile_cols_log2");
			if (increment_tile_cols_log2 == 1)
				state->tileColsLog2++;
			else
				break;
		}

		tileWidthSb = (sbCols + (1 << state->tileColsLog2) - 1) >> state->tileColsLog2;
		i = 0;
		for (startSb = 0; startSb < sbCols; startSb += tileWidthSb) {
			i += 1;
		}
		state->tileCols = i;
		minLog2tileRows = MAX((int)(minLog2Tiles - state->tileColsLog2), 0);
		state->tileRowsLog2 = minLog2tileRows;
		while (state->tileRowsLog2 < maxLog2tileRows) {
			Bool increment_tile_rows_log2 = gf_bs_read_int_log(bs, 1, "increment_tile_rows_log2");
			if (increment_tile_rows_log2 == 1)
				state->tileRowsLog2++;
			else
				break;
		}

		tileHeightSb = (sbRows + (1 << state->tileRowsLog2) - 1) >> state->tileRowsLog2;
		i = 0;
		for (startSb = 0; startSb < sbRows; startSb += tileHeightSb) {
			i += 1;
		}
		state->tileRows = i;
	}
	else {
		u32 startSb, maxTileHeightSb, widestTileSb;
		widestTileSb = 0;
		startSb = 0;
		for (i = 0; startSb < sbCols; i++) {
			u32 maxWidth = MIN((int)(sbCols - startSb), maxTileWidthSb);
			u32 width_in_sbs_minus_1 = av1_read_ns(bs, maxWidth, "width_in_sbs_minus_1");
			u32 sizeSb = width_in_sbs_minus_1 + 1;
			widestTileSb = MAX(sizeSb, widestTileSb);
			startSb += sizeSb;
		}
		if (!widestTileSb) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_CODING, ("[AV1] widest tile is 0, broken bitstream\n"));
			return;
		}
		state->tileCols = i;
		state->tileColsLog2 = aom_av1_tile_log2(1, state->tileCols);

		if (minLog2Tiles > 0)
			maxTileAreaSb = (sbRows * sbCols) >> (minLog2Tiles + 1);
		else
			maxTileAreaSb = sbRows * sbCols;
		maxTileHeightSb = MAX(maxTileAreaSb / widestTileSb, 1);

		startSb = 0;
		for (i = 0; startSb < sbRows; i++) {
			u32 maxHeight = MIN((int)(sbRows - startSb), maxTileHeightSb);
			u32 height_in_sbs_minus_1 = av1_read_ns(bs, maxHeight, "height_in_sbs_minus_1");
			u32 sizeSb = height_in_sbs_minus_1 + 1;
			startSb += sizeSb;
		}

		state->tileRows = i;
		state->tileRowsLog2 = aom_av1_tile_log2(1, state->tileRows);
	}
	if (state->tileColsLog2 > 0 || state->tileRowsLog2 > 0) {
		gf_bs_read_int_log(bs, state->tileRowsLog2 + state->tileColsLog2, "context_update_tile_id");
		state->tile_size_bytes = gf_bs_read_int_log(bs, 2, "tile_size_bytes_minus1") + 1;
	}
}

static void superres_params(GF_BitStream *bs, AV1State *state)
{
	u32 SuperresDenom;
	Bool use_superres;

	if (state->enable_superres) {
		use_superres = gf_bs_read_int_log(bs, 1, "use_superres");
	}
	else {
		use_superres = GF_FALSE;
	}
	if (use_superres) {
		u8 coded_denom = gf_bs_read_int_log(bs, AV1_SUPERRES_DENOM_BITS, "coded_denom");
		SuperresDenom = coded_denom + AV1_SUPERRES_DENOM_MIN;
	}
	else {
		SuperresDenom = AV1_SUPERRES_NUM;
	}
	state->UpscaledWidth = state->width;
	state->width = (state->UpscaledWidth * AV1_SUPERRES_NUM + (SuperresDenom / 2)) / SuperresDenom;
}

static void av1_frame_size(GF_BitStream *bs, AV1State *state, Bool frame_size_override_flag)
{
	if (frame_size_override_flag) {
		u32 frame_width_minus_1, frame_height_minus_1;
		u8 n = state->frame_width_bits_minus_1 + 1;
		frame_width_minus_1 = gf_bs_read_int_log(bs, n, "frame_width_minus_1");
		n = state->frame_height_bits_minus_1 + 1;
		frame_height_minus_1 = gf_bs_read_int_log(bs, n, "frame_height_minus_1");
		state->width = frame_width_minus_1 + 1;
		state->height = frame_height_minus_1 + 1;
	} else {
		state->width = state->sequence_width;
		state->height = state->sequence_height;
	}
	superres_params(bs, state);
	//compute_image_size(); //no bits
}

static void av1_render_size(GF_BitStream *bs)
{
	Bool render_and_frame_size_different = gf_bs_read_int_log(bs, 1, "render_and_frame_size_different_flag");
	if (render_and_frame_size_different == GF_TRUE) {
		gf_bs_read_int_log(bs, 16, "render_width_minus_1");
		gf_bs_read_int_log(bs, 16, "render_height_minus_1");
		//RenderWidth = render_width_minus_1 + 1;
		//RenderHeight = render_height_minus_1 + 1;
	}
	else {
		//RenderWidth = UpscaledWidth;
		//RenderHeight = FrameHeight;
	}
}

static void read_interpolation_filter(GF_BitStream *bs)
{
	Bool is_filter_switchable = gf_bs_read_int_log(bs, 1, "is_filter_switchable");
	if (!is_filter_switchable) {
		/*interpolation_filter =*/ gf_bs_read_int_log(bs, 2, "interpolation_filter");
	}
}

static void frame_size_with_refs(GF_BitStream *bs, AV1State *state, Bool frame_size_override_flag, s8 *ref_frame_idx)
{
	Bool found_ref = GF_FALSE;
	u32 i = 0;
	for (i = 0; i < AV1_REFS_PER_FRAME; i++) {
		found_ref = gf_bs_read_int_log_idx(bs, 1, "found_ref", i);
		if (found_ref == 1) {
			state->UpscaledWidth = state->RefUpscaledWidth[ref_frame_idx[i]];
			state->width = state->UpscaledWidth;
			state->height = state->RefFrameHeight[ref_frame_idx[i]];
			break;
		}
	}
	if (found_ref == 0) {
		av1_frame_size(bs, state, frame_size_override_flag);
		av1_render_size(bs);
	}
	else {
		superres_params(bs, state);
		//compute_image_size();
	}
}

static s32 av1_delta_q(GF_BitStream *bs, const char *name_flag, const char *name)
{
	Bool delta_coded = gf_bs_read_int_log(bs, 1, name_flag);
	s32 delta_q = 0;
	if (delta_coded) {
		u32 signMask = 1 << (7 - 1);
		delta_q = gf_bs_read_int_log(bs, 7, name);
		if (delta_q & signMask)
			delta_q = delta_q - 2 * signMask;
	}
	return delta_q;
}

static u8 Segmentation_Feature_Bits[] = { 8,6,6,6,6,3,0,0 };
static u8 Segmentation_Feature_Signed[] = { 1, 1, 1, 1, 1, 0, 0, 0 };

static u8 av1_get_qindex(Bool ignoreDeltaQ, u32 segmentId, u32 base_q_idx, u32 delta_q_present, u32 CurrentQIndex, Bool segmentation_enabled, u8 *features_SEG_LVL_ALT_Q_enabled, s32 *features_SEG_LVL_ALT_Q)
{
	//If seg_feature_active_idx( segmentId, SEG_LVL_ALT_Q ) is equal to 1 the following ordered steps apply:
	if (segmentation_enabled && features_SEG_LVL_ALT_Q_enabled[segmentId]) {
		//Set the variable data equal to FeatureData[ segmentId ][ SEG_LVL_ALT_Q ].
		s32 data = features_SEG_LVL_ALT_Q[segmentId];
		s32 qindex = base_q_idx + data;
		//If ignoreDeltaQ is equal to 0 and delta_q_present is equal to 1, set qindex equal to CurrentQIndex + data.
		if ((ignoreDeltaQ == 0) && (delta_q_present == 1)) qindex = CurrentQIndex + data;
		//Return Clip3( 0, 255, qindex ).
		if (qindex < 0) return 0;
		else if (qindex > 255) return 255;
		else return (u8)qindex;
	}
	//Otherwise, if ignoreDeltaQ is equal to 0 and delta_q_present is equal to 1, return CurrentQIndex.
	if ((ignoreDeltaQ == 0) && (delta_q_present == 1)) return CurrentQIndex;
	//otherwise
	return base_q_idx;
}

enum {
	AV1_RESTORE_NONE = 0,
	AV1_RESTORE_SWITCHABLE,
	AV1_RESTORE_WIENER,
	AV1_RESTORE_SGRPROJ
};

#define AV1_GMC_IDENTITY  0
#define AV1_GMC_TRANSLATION 1
#define AV1_GMC_ROTZOOM 2
#define AV1_GMC_AFFINE 3

#define AV1_LAST_FRAME 1
#define AV1_LAST2_FRAME 2
#define AV1_LAST3_FRAME 3
#define AV1_GOLDEN_FRAME 4
#define AV1_BWDREF_FRAME 5
#define AV1_ALTREF2_FRAME 6
#define AV1_ALTREF_FRAME 7

#define GM_ABS_ALPHA_BITS 12
#define GM_ALPHA_PREC_BITS 15
#define GM_ABS_TRANS_ONLY_BITS 9
#define GM_TRANS_ONLY_PREC_BITS 3
#define GM_ABS_TRANS_BITS 12
#define GM_TRANS_PREC_BITS 6
#define WARPEDMODEL_PREC_BITS 16


static u32 av1_decode_subexp(GF_BitStream *bs, s32 numSyms)
{
	s32 i = 0;
	s32 mk = 0;
	s32 k = 3;
	while (1) {
		s32 b2 = i ? k + i - 1 : k;
		s32 a = 1 << b2;
		if (numSyms <= mk + 3 * a) {
			s32 subexp_final_bits = av1_read_ns(bs, numSyms - mk, NULL);
			return subexp_final_bits + mk;
		}
		else {
			s32 subexp_more_bits = gf_bs_read_int(bs, 1);
			if (subexp_more_bits) {
				i++;
				mk += a;
			}
			else {
				s32 subexp_bits = gf_bs_read_int(bs, b2);
				return subexp_bits + mk;
			}
		}
	}
}

static GFINLINE s32 inverse_recenter(s32 r, u32 v)
{
	if ((s64)v > (s64)(2 * r))
		return v;
	else if (v & 1)
		return r - ((v + 1) >> 1);
	else
		return r + (v >> 1);
}

static s32 av1_decode_unsigned_subexp_with_ref(GF_BitStream *bs, s32 mx, s32 r)
{
	u32 v = av1_decode_subexp(bs, mx);
	if ((r < 0) && (-(-r << 1) <= mx)) {
		return inverse_recenter(r, v);
	}
	else if ((r << 1) <= mx) {
		return inverse_recenter(r, v);
	}
	else {
		return mx - 1 - inverse_recenter(mx - 1 - r, v);
	}
}
static s16 av1_decode_signed_subexp_with_ref(GF_BitStream *bs, s32 low, s32 high, s32 r)
{
	s16 x = av1_decode_unsigned_subexp_with_ref(bs, high - low, r - low);
	return x + low;
}

static void av1_read_global_param(AV1State *state, GF_BitStream *bs, u8 type, u8 ref, u8 idx)
{
	u8 absBits = GM_ABS_ALPHA_BITS;
	u8 precBits = GM_ALPHA_PREC_BITS;
	if (idx < 2) {
		if (type == AV1_GMC_TRANSLATION) {
			absBits = GM_ABS_TRANS_ONLY_BITS - (!state->frame_state.allow_high_precision_mv ? 1 : 0);
			precBits = GM_TRANS_ONLY_PREC_BITS - (!state->frame_state.allow_high_precision_mv ? 1 : 0);
		}
		else {
			absBits = GM_ABS_TRANS_BITS;
			precBits = GM_TRANS_PREC_BITS;
		}
	}
	s32 precDiff = WARPEDMODEL_PREC_BITS - precBits;
	s32 round = (idx % 3) == 2 ? (1 << WARPEDMODEL_PREC_BITS) : 0;
	s32 sub = (idx % 3) == 2 ? (1 << precBits) : 0;
	s32 mx = (1 << absBits);
	s32 r = (state->PrevGmParams.coefs[ref][idx] >> precDiff) - sub;
	s32 val = av1_decode_signed_subexp_with_ref(bs, -mx, mx + 1, r);

	if (val < 0) {
		val = -val;
		state->GmParams.coefs[ref][idx] = (-(val << precDiff) + round);
	}
	else {
		state->GmParams.coefs[ref][idx] = (val << precDiff) + round;
	}
}

static s32 av1_get_relative_dist(s32 a, s32 b, AV1State *state)
{
	if (!state->enable_order_hint)
		return 0;
	s32 diff = a - b;
	s32 m = 1 << (state->OrderHintBits - 1);
	diff = (diff & (m - 1)) - (diff & m);
	return diff;
}

static void av1_setup_past_independence(AV1State *state)
{
	u32 ref, i;
	for (ref = AV1_LAST_FRAME; ref <= AV1_ALTREF_FRAME; ref++) {
		for (i = 0; i <= 5; i++) {
			state->PrevGmParams.coefs[ref][i] = ((i % 3 == 2) ? 1 << WARPEDMODEL_PREC_BITS : 0);
		}
	}
}

static void av1_load_previous(AV1State *state, u8 primary_ref_frame, s8 *ref_frame_idx)
{
	s8 prevFrame = ref_frame_idx[primary_ref_frame];
	if (prevFrame < 0) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_CODING, ("[AV1] load_previous: prevFrame reference index %d is invalid\n", prevFrame));
	}
	else {
		state->PrevGmParams = state->SavedGmParams[prevFrame];
		// load_loop_filter_params( prevFrame )
		// load_segmentation_params( prevFrame )
	}
}

static void av1_decode_frame_wrapup(AV1State *state)
{
	u32 i;
	for (i = 0; i < AV1_NUM_REF_FRAMES; i++) {
		if ((state->frame_state.refresh_frame_flags >> i) & 1) {
			state->RefOrderHint[i] = state->frame_state.order_hint;
			state->SavedGmParams[i] = state->GmParams;
			state->RefFrameType[i] = state->frame_state.frame_type;
			state->RefUpscaledWidth[i] = state->UpscaledWidth;
			state->RefFrameHeight[i] = state->height;
		}
	}
	state->frame_state.seen_frame_header = GF_FALSE;
	//Otherwise (show_existing_frame is equal to 1), if frame_type is equal to KEY_FRAME, the reference frame loading process as specified in section 7.21 is invoked
	if ((state->frame_state.show_existing_frame) && (state->frame_state.frame_type == AV1_KEY_FRAME)) {
		state->frame_state.order_hint = state->RefOrderHint[state->frame_state.frame_to_show_map_idx];
		//OrderHints[ j + LAST_FRAME ] is set equal to SavedOrderHints[state->frame_to_show_map_idx ][ j + LAST_FRAME ] for j = 0..REFS_PER_FRAME-1.

		//gm_params[ ref ][ j ] is set equal to SavedGmParams[ frame_to_show_map_idx ][ ref ][ j ] for ref = LAST_FRAME..ALTREF_FRAME, for j = 0..5.
		state->GmParams = state->SavedGmParams[state->frame_state.frame_to_show_map_idx];

	}
}

static s32 find_latest_forward(u32 curFrameHint, u8 *shiftedOrderHints, u8 *usedFrame)
{
	u32 i;
	s32 ref = -1;
	s32 latestOrderHint = 0;
	for (i = 0; i < AV1_NUM_REF_FRAMES; i++) {
		s32 hint = shiftedOrderHints[i];
		if (!usedFrame[i] && ((u32)hint < curFrameHint) && (ref < 0 || hint >= latestOrderHint)) {
			ref = i;
			latestOrderHint = hint;
		}
	}
	return ref;
}

//see 7.8 of AV1 spec
static void av1_set_frame_refs(AV1State *state, u8 last_frame_idx, u8 gold_frame_idx, s8 *ref_frame_idx)
{
	u32 i;
	u8 usedFrame[AV1_NUM_REF_FRAMES];
	u8 shiftedOrderHints[AV1_NUM_REF_FRAMES];

	for (i = 0; i < AV1_REFS_PER_FRAME; i++)
		ref_frame_idx[i] = -1;

	ref_frame_idx[AV1_LAST_FRAME - AV1_LAST_FRAME] = last_frame_idx;
	ref_frame_idx[AV1_GOLDEN_FRAME - AV1_LAST_FRAME] = gold_frame_idx;

	for (i = 0; i < AV1_NUM_REF_FRAMES; i++) {
		usedFrame[i] = 0;
	}

	usedFrame[last_frame_idx] = 1;
	usedFrame[gold_frame_idx] = 1;
	u32 curFrameHint = 1 << (state->OrderHintBits - 1);

	for (i = 0; i < AV1_NUM_REF_FRAMES; i++) {
		shiftedOrderHints[i] = curFrameHint + av1_get_relative_dist(state->RefOrderHint[i], state->frame_state.order_hint, state);
	}

	u8 lastOrderHint = shiftedOrderHints[last_frame_idx];
	u8 goldOrderHint = shiftedOrderHints[gold_frame_idx];

	//It is a requirement of bitstream conformance that lastOrderHint is strictly less than curFrameHint.
	if (lastOrderHint >= curFrameHint) {
		GF_LOG(GF_LOG_WARNING, GF_LOG_CODING, ("[AV1] non conformant bitstream detected while setting up frame refs: lastOrderHint(%d) shall be stricly less than curFrameHint(%d)\n", lastOrderHint, curFrameHint));
	}
	//It is a requirement of bitstream conformance that goldOrderHint is strictly less than curFrameHint.
	if (goldOrderHint >= curFrameHint) {
		GF_LOG(GF_LOG_WARNING, GF_LOG_CODING, ("[AV1] non conformant bitstream detected while setting up frame refs: goldOrderHint(%d) shall be stricly less than curFrameHint(%d)\n", lastOrderHint, curFrameHint));
	}

	//find_latest_backward() {
	s32 ref = -1;
	s32 latestOrderHint = 0;
	for (i = 0; i < AV1_NUM_REF_FRAMES; i++) {
		s32 hint = shiftedOrderHints[i];
		if (!usedFrame[i] && ((u32)hint >= curFrameHint) && (ref < 0 || hint >= latestOrderHint)) {
			ref = i;
			latestOrderHint = hint;
		}
	}
	if (ref >= 0) {
		ref_frame_idx[AV1_ALTREF_FRAME - AV1_LAST_FRAME] = ref;
		usedFrame[ref] = 1;
	}
	//find_earliest_backward() for BWDREF_FRAME
	ref = -1;
	s32 earliestOrderHint = 0;
	for (i = 0; i < AV1_NUM_REF_FRAMES; i++) {
		s32 hint = shiftedOrderHints[i];
		if (!usedFrame[i] && ((u32)hint >= curFrameHint) && (ref < 0 || hint < earliestOrderHint)) {
			ref = i;
			earliestOrderHint = hint;
		}
	}
	if (ref >= 0) {
		ref_frame_idx[AV1_BWDREF_FRAME - AV1_LAST_FRAME] = ref;
		usedFrame[ref] = 1;
	}

	//find_earliest_backward() for ALTREF2_FRAME
	ref = -1;
	earliestOrderHint = 0;
	for (i = 0; i < AV1_NUM_REF_FRAMES; i++) {
		s32 hint = shiftedOrderHints[i];
		if (!usedFrame[i] && ((u32)hint >= curFrameHint) && (ref < 0 || hint < earliestOrderHint)) {
			ref = i;
			earliestOrderHint = hint;
		}
	}
	if (ref >= 0) {
		ref_frame_idx[AV1_ALTREF2_FRAME - AV1_LAST_FRAME] = ref;
		usedFrame[ref] = 1;
	}

	//The remaining references are set to be forward references in anti-chronological order as follows:

	const u8 Ref_Frame_List[AV1_REFS_PER_FRAME - 2] = {
		AV1_LAST2_FRAME, AV1_LAST3_FRAME, AV1_BWDREF_FRAME, AV1_ALTREF2_FRAME, AV1_ALTREF_FRAME
	};

	for (i = 0; i < AV1_REFS_PER_FRAME - 2; i++) {
		u8 refFrame = Ref_Frame_List[i];
		if (ref_frame_idx[refFrame - AV1_LAST_FRAME] < 0) {
			s32 last_ref = find_latest_forward(curFrameHint, shiftedOrderHints, usedFrame);
			if (last_ref >= 0) {
				ref_frame_idx[refFrame - AV1_LAST_FRAME] = last_ref;
				usedFrame[last_ref] = 1;
			}
		}
	}
	//Finally, any remaining references are set to the reference frame with smallest output order as follows:
	ref = -1;
	for (i = 0; i < AV1_NUM_REF_FRAMES; i++) {
		s32 hint = shiftedOrderHints[i];
		if (ref < 0 || hint < earliestOrderHint) {
			ref = i;
			earliestOrderHint = hint;
		}
	}
	for (i = 0; i < AV1_REFS_PER_FRAME; i++) {
		if (ref_frame_idx[i] < 0) {
			ref_frame_idx[i] = ref;
		}
	}
}


static void av1_parse_uncompressed_header(GF_BitStream *bs, AV1State *state)
{
	Bool error_resilient_mode = GF_FALSE, allow_screen_content_tools = GF_FALSE, force_integer_mv = GF_FALSE;
	Bool /*use_ref_frame_mvs = GF_FALSE,*/ FrameIsIntra = GF_FALSE, frame_size_override_flag = GF_FALSE;
	Bool disable_cdf_update = GF_FALSE;
	u8 showable_frame;
	u8 primary_ref_frame;
	u16 idLen = 0;
	u32 idx;
	s8 ref_frame_idx[AV1_REFS_PER_FRAME];
	AV1StateFrame *frame_state = &state->frame_state;

	if (state->frame_id_numbers_present_flag) {
		idLen = (state->additional_frame_id_length_minus_1 + state->delta_frame_id_length_minus_2 + 3);
	}
	frame_state->refresh_frame_flags = 0;

	showable_frame = 0;
	if (state->reduced_still_picture_header) {
		frame_state->key_frame = GF_TRUE;
		FrameIsIntra = GF_TRUE;
		frame_state->frame_type = AV1_KEY_FRAME;
		frame_state->show_frame = GF_TRUE;
		frame_state->show_existing_frame = 0;
	}
	else {
		frame_state->show_existing_frame = gf_bs_read_int_log(bs, 1, "show_existing_frame");
		if (frame_state->show_existing_frame == GF_TRUE) {
			frame_state->frame_to_show_map_idx = gf_bs_read_int_log(bs, 3, "frame_to_show_map_idx");
			frame_state->frame_type = state->RefFrameType[frame_state->frame_to_show_map_idx];

			if (state->decoder_model_info_present_flag && !state->equal_picture_interval) {
				gf_bs_read_int_log(bs, state->frame_presentation_time_length, "frame_presentation_time");
			}

			frame_state->refresh_frame_flags = 0;
			if (state->frame_id_numbers_present_flag) {
				gf_bs_read_int_log(bs, idLen, "display_frame_id");
			}
			if (frame_state->frame_type == AV1_KEY_FRAME) {
				frame_state->refresh_frame_flags = AV1_ALL_FRAMES;
			}
			/*
			if (film_grain_params_present) {
				load_grain_params(frame_to_show_map_idx)
			}*/
			return;
		}
		frame_state->frame_type = gf_bs_read_int_log(bs, 2, "frame_type");
		FrameIsIntra = (frame_state->frame_type == AV1_INTRA_ONLY_FRAME || frame_state->frame_type == AV1_KEY_FRAME);
		frame_state->show_frame = gf_bs_read_int_log(bs, 1, "show_frame");
		if (frame_state->is_first_frame) {
			frame_state->key_frame = frame_state->seen_seq_header && frame_state->show_frame && frame_state->frame_type == AV1_KEY_FRAME && frame_state->seen_frame_header;
		}
		if (frame_state->show_frame && state->decoder_model_info_present_flag && !state->equal_picture_interval) {
			gf_bs_read_int_log(bs, state->frame_presentation_time_length, "frame_presentation_time");
		}
		if (frame_state->show_frame) {
			showable_frame = frame_state->frame_type != AV1_KEY_FRAME;

		}
		else {
			showable_frame = gf_bs_read_int_log(bs, 1, "showable_frame");
		}
		if (frame_state->frame_type == AV1_SWITCH_FRAME || (frame_state->frame_type == AV1_KEY_FRAME && frame_state->show_frame))
			error_resilient_mode = GF_TRUE;
		else
			error_resilient_mode = gf_bs_read_int_log(bs, 1, "error_resilient_mode");
	}

	if ((frame_state->frame_type == AV1_KEY_FRAME) && frame_state->show_frame) {
		u32 i;
		for (i = 0; i < AV1_NUM_REF_FRAMES; i++) {
			state->RefValid[i] = 0;
			state->RefOrderHint[i] = 0;
		}
		for (i = 0; i < AV1_REFS_PER_FRAME; i++) {
			state->OrderHints[AV1_LAST_FRAME + i] = 0;
		}
	}

	disable_cdf_update = gf_bs_read_int_log(bs, 1, "disable_cdf_update");
	if (state->seq_force_screen_content_tools == 2/*SELECT_SCREEN_CONTENT_TOOLS*/) {
		allow_screen_content_tools = gf_bs_read_int_log(bs, 1, "allow_screen_content_tools");
	}
	else {
		allow_screen_content_tools = state->seq_force_screen_content_tools;
	}
	if (allow_screen_content_tools) {
		if (state->seq_force_integer_mv == 2/*SELECT_INTEGER_MV*/) {
			force_integer_mv = gf_bs_read_int_log(bs, 1, "force_integer_mv");
		}
		else {
			force_integer_mv = state->seq_force_integer_mv;
		}
	}
	else {
		force_integer_mv = 0;
	}
	if (FrameIsIntra) {
		force_integer_mv = 1;
	}
	if (state->frame_id_numbers_present_flag) {
		gf_bs_read_int_log(bs, idLen, "current_frame_id");
	}
	if (frame_state->frame_type == AV1_SWITCH_FRAME)
		frame_size_override_flag = GF_TRUE;
	else if (state->reduced_still_picture_header)
		frame_size_override_flag = GF_FALSE;
	else
		frame_size_override_flag = gf_bs_read_int_log(bs, 1, "frame_size_override_flag");

	frame_state->order_hint = gf_bs_read_int_log(bs, state->OrderHintBits, "order_hint");
	if (FrameIsIntra || error_resilient_mode) {
		primary_ref_frame = AV1_PRIMARY_REF_NONE;
	}
	else {
		primary_ref_frame = gf_bs_read_int_log(bs, 3, "primary_ref_frame");
	}

	if (state->decoder_model_info_present_flag) {
		u8 buffer_removal_time_present_flag = gf_bs_read_int_log(bs, 1, "buffer_removal_time_present_flag");
		if (buffer_removal_time_present_flag) {
			u32 opNum;
			for (opNum = 0; opNum < state->operating_points_count; opNum++) {
				if (state->decoder_model_present_for_this_op[opNum]) {
					u8 opPtIdc = state->operating_point_idc[opNum];
					u8 inTemporalLayer = (opPtIdc >> state->temporal_id) & 1;
					u8 inSpatialLayer = (opPtIdc >> (state->spatial_id + 8)) & 1;
					if (opPtIdc == 0 || (inTemporalLayer && inSpatialLayer)) {
						gf_bs_read_int_log_idx(bs, state->buffer_removal_time_length, "buffer_removal_time", opNum);
					}
				}
			}
		}
	}

	if (frame_state->frame_type == AV1_SWITCH_FRAME || (frame_state->frame_type == AV1_KEY_FRAME && frame_state->show_frame)) {
		frame_state->refresh_frame_flags = AV1_ALL_FRAMES;
	}
	else {
		frame_state->refresh_frame_flags = gf_bs_read_int_log(bs, 8, "refresh_frame_flags");
	}
	if (!FrameIsIntra || frame_state->refresh_frame_flags != AV1_ALL_FRAMES) {
		if (error_resilient_mode && state->enable_order_hint) {
			u32 i = 0;
			for (i = 0; i < AV1_NUM_REF_FRAMES; i++) {
				u8 ref_order_hint = gf_bs_read_int_log_idx(bs, state->OrderHintBits, "ref_order_hint", i);
				if (ref_order_hint != state->RefOrderHint[i]) {
					state->RefValid[i] = 0;
				}
				state->RefOrderHint[i] = ref_order_hint;
			}
		}
	}

	u8 allow_intrabc = 0;
	if (frame_state->frame_type == AV1_KEY_FRAME) {
		av1_frame_size(bs, state, frame_size_override_flag);
		av1_render_size(bs);
		if (allow_screen_content_tools && state->UpscaledWidth == state->width) {
			allow_intrabc = gf_bs_read_int_log(bs, 1, "allow_intrabc");
		}
	}
	else {
		if (frame_state->frame_type == AV1_INTRA_ONLY_FRAME) {
			av1_frame_size(bs, state, frame_size_override_flag);
			av1_render_size(bs);
			if (allow_screen_content_tools && state->UpscaledWidth == state->width) {
				allow_intrabc = gf_bs_read_int_log(bs, 1, "allow_intrabc");
			}
		}
		else {
			u32 i = 0;
			Bool frame_refs_short_signaling = GF_FALSE;
			if (state->enable_order_hint) {
				frame_refs_short_signaling = gf_bs_read_int_log(bs, 1, "frame_refs_short_signaling");
				if (frame_refs_short_signaling) {
					u8 last_frame_idx = gf_bs_read_int_log(bs, 3, "last_frame_idx");
					u8 gold_frame_idx = gf_bs_read_int_log(bs, 3, "gold_frame_idx");
					av1_set_frame_refs(state, last_frame_idx, gold_frame_idx, ref_frame_idx);
				}
			}
			for (i = 0; i < AV1_REFS_PER_FRAME; i++) {
				if (!frame_refs_short_signaling)
					ref_frame_idx[i] = gf_bs_read_int_log_idx(bs, 3, "ref_frame_idx", i);

				if (state->frame_id_numbers_present_flag) {
					u32 n = state->delta_frame_id_length_minus_2 + 2;
					/*delta_frame_id_minus_1 =*/ gf_bs_read_int_log_idx(bs, n, "delta_frame_id_minus1", i);
					//DeltaFrameId = delta_frame_id_minus_1 + 1;
					//expectedFrameId[i] = ((current_frame_id + (1 << idLen) - DeltaFrameId) % (1 << idLen));
				}
			}
			if (frame_size_override_flag && !error_resilient_mode) {
				frame_size_with_refs(bs, state, frame_size_override_flag, ref_frame_idx);
			}
			else {
				av1_frame_size(bs, state, frame_size_override_flag);
				av1_render_size(bs);
			}
			frame_state->allow_high_precision_mv = 0;
			if (!force_integer_mv) {
				frame_state->allow_high_precision_mv = gf_bs_read_int_log(bs, 1, "allow_high_precision_mv");
			}

			read_interpolation_filter(bs);

			gf_bs_read_int_log(bs, 1, "is_motion_mode_switchable");
			if (!(error_resilient_mode || !state->enable_ref_frame_mvs)) {
				gf_bs_read_int_log(bs, 1, "use_ref_frame_mvs");
			}
		}
	}

	if (!FrameIsIntra) {
		u32 i;
		for (i = 0; i < AV1_REFS_PER_FRAME; i++) {
			u8 refFrame = AV1_LAST_FRAME + i;
			u8 ridx = ref_frame_idx[i];
			if (ridx >= 0) {
				u8 hint = state->RefOrderHint[ridx];
				state->OrderHints[refFrame] = hint;
				/*			if ( !enable_order_hint ) {
								RefFrameSignBias[ refFrame ] = 0;
							} else {
								RefFrameSignBias[ refFrame ] = get_relative_dist( hint, OrderHint) > 0;
							}
				*/
			}

		}
	}

	if (!(state->reduced_still_picture_header || disable_cdf_update))
		gf_bs_read_int_log(bs, 1, "disable_frame_end_update_cdf");

	if (primary_ref_frame == AV1_PRIMARY_REF_NONE) {
		//init_non_coeff_cdfs();
		av1_setup_past_independence(state);
	}
	else {
		//load_cdfs(ref_frame_idx[primary_ref_frame]);
		av1_load_previous(state, primary_ref_frame, ref_frame_idx);
	}

	av1_parse_tile_info(bs, state);
	//quantization_params( ):
	u8 base_q_idx = gf_bs_read_int_log(bs, 8, "base_q_idx");
	s32 DeltaQUDc = 0;
	s32 DeltaQUAc = 0;
	s32 DeltaQVDc = 0;
	s32 DeltaQVAc = 0;
	s32 DeltaQYDc = av1_delta_q(bs, "DeltaQYDc_coded", "DeltaQYDc");
	if (!state->config->monochrome) {
		u8 diff_uv_delta = 0;
		if (state->separate_uv_delta_q)
			diff_uv_delta = gf_bs_read_int_log(bs, 1, "diff_uv_delta");

		DeltaQUDc = av1_delta_q(bs, "DeltaQUDc_coded", "DeltaQUDc");
		DeltaQUAc = av1_delta_q(bs, "DeltaQUAc_coded", "DeltaQUAc");
		if (diff_uv_delta) {
			DeltaQVDc = av1_delta_q(bs, "DeltaQVDc_coded", "DeltaQVDc");
			DeltaQVAc = av1_delta_q(bs, "DeltaQVAc_coded", "DeltaQVAc");
		}
	}
	if (gf_bs_read_int_log(bs, 1, "using_qmatrix")) {
		gf_bs_read_int_log(bs, 4, "qm_y");
		gf_bs_read_int_log(bs, 4, "qm_u");
		if (state->separate_uv_delta_q) {
			gf_bs_read_int_log(bs, 4, "qm_v");
		}
	}

	u8 seg_features_SEG_LVL_ALT_Q_enabled[8] = { 0,0,0,0,0,0,0,0 };
	s32 seg_features_SEG_LVL_ALT_Q[8] = { 0,0,0,0,0,0,0,0 };

	//segmentation_params( ):
	u8 segmentation_enabled = gf_bs_read_int_log(bs, 1, "segmentation_enabled");
	if (segmentation_enabled) {
		/*u8 segmentation_temporal_update = 0;*/
		u8 segmentation_update_data = 1;
		if (primary_ref_frame != AV1_PRIMARY_REF_NONE) {
			u8 segmentation_update_map = gf_bs_read_int_log(bs, 1, "segmentation_update_map");
			if (segmentation_update_map == 1)
				gf_bs_read_int_log(bs, 1, "segmentation_temporal_update");
			segmentation_update_data = gf_bs_read_int_log(bs, 1, "segmentation_update_data");
		}
		if (segmentation_update_data == 1) {
			u32 i, j;
			for (i = 0; i < 8/*=MAX_SEGMENTS*/; i++) {
				for (j = 0; j < 8 /*=SEG_LVL_MAX*/; j++) {
					if (/*feature_enabled = */gf_bs_read_int_log_idx2(bs, 1, "feature_enabled", i, j) == 1) {
						s32 val;
						u32 bitsToRead = Segmentation_Feature_Bits[j];
						//this is SEG_LVL_ALT_Q
						if (!j) seg_features_SEG_LVL_ALT_Q_enabled[i] = 1;

						if (Segmentation_Feature_Signed[j] == 1) {
							val = gf_bs_read_int_log_idx2(bs, 1 + bitsToRead, "signed_feature_value", i, j);
						}
						else {
							val = gf_bs_read_int_log_idx2(bs, bitsToRead, "feature_value", i, j);
						}
						if (!j) seg_features_SEG_LVL_ALT_Q[i] = val;
					}
				}
			}
			//ignore all init steps
		}

	}

	//delta_q_params():
	/*u8 delta_q_res = 0;*/
	u8 delta_q_present = 0;
	if (base_q_idx > 0) {
		delta_q_present = gf_bs_read_int_log(bs, 1, "delta_q_present");
	}
	if (delta_q_present) {
		gf_bs_read_int_log(bs, 2, "delta_q_res");
	}

	//delta_lf_params():
	u8 delta_lf_present = 0;
	/*u8 delta_lf_res = 0;
	u8 delta_lf_multi = 0;*/
	if (delta_q_present) {
		if (!allow_intrabc) {
			delta_lf_present = gf_bs_read_int_log(bs, 1, "delta_lf_present");
		}
		if (delta_lf_present) {
			gf_bs_read_int_log(bs, 2, "delta_lf_res");
			gf_bs_read_int_log(bs, 1, "delta_lf_multi");
		}
	}

	//init lossless stuff!
	u8 CodedLossless = 1;
	for (idx = 0; idx < 8; idx++) {
		u8 qindex = av1_get_qindex(GF_TRUE, idx, base_q_idx, delta_q_present, 0/*CurrentQIndex always ignored at this level of parsin*/, segmentation_enabled, seg_features_SEG_LVL_ALT_Q_enabled, seg_features_SEG_LVL_ALT_Q);
		Bool LosslessArray = (qindex == 0) && (DeltaQYDc == 0) && (DeltaQUAc == 0) && (DeltaQUDc == 0) && (DeltaQVAc == 0) && (DeltaQVDc == 0);
		if (!LosslessArray)
			CodedLossless = 0;
	}
	Bool AllLossless = CodedLossless && (state->width == state->UpscaledWidth);

	//loop_filter_params():
	if (!CodedLossless && !allow_intrabc) {
		u8 loop_filter_level_0 = gf_bs_read_int_log(bs, 6, "loop_filter_level_0");
		u8 loop_filter_level_1 = gf_bs_read_int_log(bs, 6, "loop_filter_level_1");
		if (!state->config->monochrome) {
			if (loop_filter_level_0 || loop_filter_level_1) {
				gf_bs_read_int_log(bs, 6, "loop_filter_level_2");
				gf_bs_read_int_log(bs, 6, "loop_filter_level_3");
			}
		}
		gf_bs_read_int_log(bs, 3, "loop_filter_sharpness");
		u8 loop_filter_delta_enabled = gf_bs_read_int_log(bs, 1, "loop_filter_delta_enabled");
		if (loop_filter_delta_enabled == 1) {
			u8 loop_filter_delta_update = gf_bs_read_int_log(bs, 1, "loop_filter_delta_update");
			if (loop_filter_delta_update) {
				u32 i;
				for (i = 0; i < 8/*TOTAL_REFS_PER_FRAME*/; i++) {
					u8 update_ref_delta = gf_bs_read_int_log_idx(bs, 1, "update_ref_delta", i);
					if (update_ref_delta == 1) {
						gf_bs_read_int_log_idx(bs, 1 + 6, "loop_filter_ref_deltas", i);
					}
				}
				for (i = 0; i < 2; i++) {
					u8 update_mode_delta = gf_bs_read_int_log_idx(bs, 1, "update_mode_delta", i);
					if (update_mode_delta) {
						gf_bs_read_int_log_idx(bs, 1 + 6, "loop_filter_mode_deltas", i);
					}
				}
			}
		}
	}
	//cdef_params( ):
	if (!CodedLossless && !allow_intrabc && state->enable_cdef) {
		gf_bs_read_int_log(bs, 2, "cdef_damping_minus_3");
		u8 cdef_bits = gf_bs_read_int_log(bs, 2, "cdef_bits");
		u32 i, num_cd = 1 << cdef_bits;
		for (i = 0; i < num_cd; i++) {
			gf_bs_read_int_log_idx(bs, 4, "cdef_y_pri_strength", i);
			gf_bs_read_int_log_idx(bs, 2, "cdef_y_sec_strength", i);
			if (!state->config->monochrome) {
				gf_bs_read_int_log_idx(bs, 4, "cdef_uv_pri_strength", i);
				gf_bs_read_int_log_idx(bs, 2, "cdef_uv_sec_strength", i);
			}
		}
	}

	//lr_params( ) :
	if (!AllLossless && !allow_intrabc && state->enable_restoration) {
		u32 i, nb_planes = state->config->monochrome ? 1 : 3;
		u8 UsesLr = 0;
		u8 usesChromaLr = 0;
		for (i = 0; i < nb_planes; i++) {
			u8 lr_type = gf_bs_read_int_log_idx(bs, 2, "lr_type", i);
			//FrameRestorationType[i] = Remap_Lr_Type[lr_type]
			if (lr_type != AV1_RESTORE_NONE) {
				UsesLr = 1;
				if (i > 0) {
					usesChromaLr = 1;
				}
			}
		}
		if (UsesLr) {
			if (state->use_128x128_superblock) {
				gf_bs_read_int_log(bs, 1, "lr_unit_shift_minus_1");
			}
			else {
				u8 lr_unit_shift = gf_bs_read_int_log(bs, 1, "lr_unit_shift");
				if (lr_unit_shift) {
					gf_bs_read_int_log(bs, 1, "lr_unit_extra_shift");
					//lr_unit_shift += lr_unit_extra_shift;
				}
			}
			if (state->config->chroma_subsampling_x && state->config->chroma_subsampling_y && usesChromaLr) {
				gf_bs_read_int_log(bs, 1, "lr_uv_shift");
			}
		}
	}
	//read_tx_mode():
	if (CodedLossless == 1) {
	}
	else {
		gf_bs_read_int_log(bs, 1, "tx_mode_select");
	}

	//frame_reference_mode( ):
	u8 reference_select = 0;
	if (FrameIsIntra) {
	}
	else {
		reference_select = gf_bs_read_int_log(bs, 1, "reference_select");
	}

	//skip_mode_params( ):
	u8 skipModeAllowed = 0;
	if (FrameIsIntra || !reference_select || !state->enable_order_hint) {
	}
	else {
		u32 i;
		s32 forwardIdx = -1;
		s32 backwardIdx = -1;
		s32 forwardHint = 0;
		s32 backwardHint = 0;
		for (i = 0; i < AV1_REFS_PER_FRAME; i++) {
			u8 refHint = state->RefOrderHint[ref_frame_idx[i]];
			if (av1_get_relative_dist(refHint, frame_state->order_hint, state) < 0) {
				if (forwardIdx < 0 || av1_get_relative_dist(refHint, forwardHint, state) > 0) {
					forwardIdx = i;
					forwardHint = refHint;
				}
			}
			else if (av1_get_relative_dist(refHint, frame_state->order_hint, state) > 0) {
				if (backwardIdx < 0 || av1_get_relative_dist(refHint, backwardHint, state) < 0) {
					backwardIdx = i;
					backwardHint = refHint;
				}
			}
		}
		if (forwardIdx < 0) {
			skipModeAllowed = 0;
		}
		else if (backwardIdx >= 0) {
			skipModeAllowed = 1;
			//SkipModeFrame[0] = AV1_LAST_FRAME + MIN(forwardIdx, backwardIdx);
			//SkipModeFrame[1] = AV1_LAST_FRAME + MAX(forwardIdx, backwardIdx);
		}
		else {
			s32 secondForwardIdx = -1;
			s32 secondForwardHint = 0;
			for (i = 0; i < AV1_REFS_PER_FRAME; i++) {
				u8 refHint = state->RefOrderHint[ref_frame_idx[i]];
				if (av1_get_relative_dist(refHint, forwardHint, state) < 0) {
					if (secondForwardIdx < 0 || av1_get_relative_dist(refHint, secondForwardHint, state) > 0) {
						secondForwardIdx = i;
						secondForwardHint = refHint;
					}
				}
			}
			if (secondForwardIdx < 0) {
				skipModeAllowed = 0;
			}
			else {
				skipModeAllowed = 1;
				//SkipModeFrame[ 0 ] = LAST_FRAME + Min(forwardIdx, secondForwardIdx)
				//SkipModeFrame[ 1 ] = LAST_FRAME + Max(forwardIdx, secondForwardIdx)
			}
		}
	}
	if (skipModeAllowed) {
		gf_bs_read_int_log(bs, 1, "skip_mode_present");
	}


	if (FrameIsIntra || error_resilient_mode || !state->enable_warped_motion) {

	}
	else {
		gf_bs_read_int_log(bs, 1, "allow_warped_motion");
	}

	gf_bs_read_int_log(bs, 1, "reduced_tx");

	//global_motion_params( )
	u32 ref;
	for (ref = AV1_LAST_FRAME; ref <= AV1_ALTREF_FRAME; ref++) {
		u32 i;
		for (i = 0; i < 6; i++) {
			state->GmParams.coefs[ref][i] = ((i % 3 == 2) ? 1 << WARPEDMODEL_PREC_BITS : 0);
		}
	}
	if (!FrameIsIntra) {
		u32 refs;
		for (refs = AV1_LAST_FRAME; refs <= AV1_ALTREF_FRAME; refs++) {
			u8 type = AV1_GMC_IDENTITY;
			Bool is_global = gf_bs_read_int_log_idx(bs, 1, "is_global", refs);
			if (is_global) {
				Bool is_rot_zoom = gf_bs_read_int_log_idx(bs, 1, "is_rot_zoom", refs);
				if (is_rot_zoom) {
					type = AV1_GMC_ROTZOOM;
				}
				else {
					Bool is_trans = gf_bs_read_int_log_idx(bs, 1, "is_translation", refs);
					type = is_trans ? AV1_GMC_TRANSLATION : AV1_GMC_AFFINE;

				}
			}

			if (type >= AV1_GMC_ROTZOOM) {
				av1_read_global_param(state, bs, type, refs, 2);
				av1_read_global_param(state, bs, type, refs, 3);
				if (type == AV1_GMC_AFFINE) {
					av1_read_global_param(state, bs, type, refs, 4);
					av1_read_global_param(state, bs, type, refs, 5);
				}
				else {
					state->GmParams.coefs[refs][4] = -state->GmParams.coefs[refs][3];
					state->GmParams.coefs[refs][5] = state->GmParams.coefs[refs][2];

				}
			}
			if (type >= AV1_GMC_TRANSLATION) {
				av1_read_global_param(state, bs, type, refs, 0);
				av1_read_global_param(state, bs, type, refs, 1);
			}
		}
	}

	//film_grain_params()
	if (!state->film_grain_params_present || (!state->frame_state.show_frame && !showable_frame)) {
	}
	else {
		u8 apply_grain = gf_bs_read_int_log(bs, 1, "apply_grain");
		if (apply_grain) {
			gf_bs_read_int_log(bs, 16, "grain_seed");
			u8 update_grain = 1;
			if (state->frame_state.frame_type == AV1_INTER_FRAME) {
				update_grain = gf_bs_read_int_log(bs, 1, "update_grain");
			}
			if (!update_grain) {
				gf_bs_read_int_log(bs, 3, "film_grain_params_ref_idx");
			}
			else {
				u32 i, num_y_points = gf_bs_read_int_log(bs, 4, "num_y_points");
				for (i = 0; i < num_y_points; i++) {
					gf_bs_read_int_log_idx(bs, 8, "point_y_value", i);
					gf_bs_read_int_log_idx(bs, 8, "point_y_scaling", i);
				}
				u8 chroma_scaling_from_luma = 0;
				if (!state->config->monochrome)
					chroma_scaling_from_luma = gf_bs_read_int_log(bs, 1, "chroma_scaling_from_luma");

				u8 num_cb_points = 0;
				u8 num_cr_points = 0;
				if (state->config->monochrome || chroma_scaling_from_luma ||
					((state->config->chroma_subsampling_x == 1) && (state->config->chroma_subsampling_y == 1) && (num_y_points == 0))
					) {
				}
				else {
					num_cb_points = gf_bs_read_int_log(bs, 4, "num_cb_points");
					for (i = 0; i < num_cb_points; i++) {
						gf_bs_read_int_log_idx(bs, 8, "point_cb_value", i);
						gf_bs_read_int_log_idx(bs, 8, "point_cb_scaling", i);
					}
					num_cr_points = gf_bs_read_int_log(bs, 4, "num_cr_points");
					for (i = 0; i < num_cr_points; i++) {
						gf_bs_read_int_log_idx(bs, 8, "point_cr_value", i);
						gf_bs_read_int_log_idx(bs, 8, "point_cr_scaling", i);
					}
				}
				gf_bs_read_int_log(bs, 2, "grain_scaling_minus_8");
				u8 ar_coeff_lag = gf_bs_read_int_log(bs, 2, "ar_coeff_lag");
				u16 numPosLuma = 2 * ar_coeff_lag * (ar_coeff_lag + 1);
				u16 numPosChroma = numPosLuma;
				if (num_y_points) {
					numPosChroma = numPosLuma + 1;
					for (i = 0; i < numPosLuma; i++) {
						gf_bs_read_int_log_idx(bs, 8, "ar_coeffs_y_plus_128", i);
					}
				}
				if (chroma_scaling_from_luma || num_cb_points) {
					for (i = 0; i < numPosChroma; i++) {
						gf_bs_read_int_log_idx(bs, 8, "ar_coeffs_cb_plus_128", i);
					}
				}
				if (chroma_scaling_from_luma || num_cr_points) {
					for (i = 0; i < numPosChroma; i++) {
						gf_bs_read_int_log_idx(bs, 8, "ar_coeffs_cr_plus_128", i);
					}
				}
				gf_bs_read_int_log(bs, 2, "ar_coeff_shift_minus_6");
				gf_bs_read_int_log(bs, 2, "grain_scale_shift");
				if (num_cb_points) {
					gf_bs_read_int_log(bs, 8, "cb_mult");
					gf_bs_read_int_log(bs, 8, "cb_luma_mult");
					gf_bs_read_int_log(bs, 9, "cb_offset");
				}
				if (num_cr_points) {
					gf_bs_read_int_log(bs, 8, "cr_mult");
					gf_bs_read_int_log(bs, 8, "cr_luma_mult");
					gf_bs_read_int_log(bs, 9, "cr_offset");
				}
				gf_bs_read_int_log(bs, 1, "overlap_flag");
				gf_bs_read_int_log(bs, 1, "clip_to_restricted_range");
			}
		}
	}

	//end of uncompressed header !!
}

GF_EXPORT
void gf_av1_init_state(AV1State *state)
{
	if (!state) return;
	memset(state, 0, sizeof(AV1State));
	state->color_primaries = 2;
	state->transfer_characteristics = 2;
	state->matrix_coefficients = 2;
}

GF_EXPORT
void gf_av1_reset_state(AV1State *state, Bool is_destroy)
{
	GF_List *l1, *l2;

	if (state->frame_state.header_obus) {
		while (gf_list_count(state->frame_state.header_obus)) {
			GF_AV1_OBUArrayEntry *a = (GF_AV1_OBUArrayEntry*)gf_list_pop_back(state->frame_state.header_obus);
			if (a->obu) gf_free(a->obu);
			gf_free(a);
		}
	}

	if (state->frame_state.frame_obus) {
		while (gf_list_count(state->frame_state.frame_obus)) {
			GF_AV1_OBUArrayEntry *a = (GF_AV1_OBUArrayEntry*)gf_list_pop_back(state->frame_state.frame_obus);
			if (a->obu) gf_free(a->obu);
			gf_free(a);
		}
	}
	l1 = state->frame_state.frame_obus;
	l2 = state->frame_state.header_obus;
	memset(&state->frame_state, 0, sizeof(AV1StateFrame));
	state->frame_state.is_first_frame = GF_TRUE;

	if (is_destroy) {
		gf_list_del(l1);
		gf_list_del(l2);
		if (state->bs) {
			//cf issues #1893 and #2604:
			//state->frame_obus is either:
			//- NULL, in which case there is a valid buffer in bs, freed by bs_del
			//- not NULL, in which case the internal buffer of bs is NULL and we must free the buffer

			if (state->frame_obus) {
				gf_free(state->frame_obus);
				state->frame_obus = NULL;
				state->frame_obus_alloc = 0;
			}
			gf_bs_del(state->bs);
			state->bs = NULL;
		}
	}
	else {
		state->frame_state.frame_obus = l1;
		state->frame_state.header_obus = l2;
		if (state->bs)
			gf_bs_seek(state->bs, 0);
	}
}

static GF_Err av1_parse_tile_group(GF_BitStream *bs, AV1State *state, u64 obu_start, u64 obu_size)
{
	u32 TileNum, tg_start = 0, tg_end = 0;
	u32 numTiles = state->tileCols * state->tileRows;
	Bool tile_start_and_end_present_flag = GF_FALSE;
	GF_Err e = GF_OK;
	if (numTiles > 1)
		tile_start_and_end_present_flag = gf_bs_read_int_log(bs, 1, "tile_start_and_end_present_flag");

	if (numTiles == 1 || !tile_start_and_end_present_flag) {
		tg_start = 0;
		tg_end = numTiles - 1;
		/*state->frame_state.tg[0].start_idx = 0;
		state->frame_state.tg[0].end_idx = numTiles - 1;*/
	}
	else {
		u32 tileBits = state->tileColsLog2 + state->tileRowsLog2;
		/*state->frame_state.tg[state->frame_state.tg_idx].start_idx*/ tg_start = gf_bs_read_int_log(bs, tileBits, "tg_start");
		/*state->frame_state.tg[state->frame_state.tg_idx].end_idx*/ tg_end = gf_bs_read_int_log(bs, tileBits, "tg_end");
	}
	/*state->frame_state.tg_idx++;*/

	gf_bs_align(bs);

	if (tg_end >= GF_ARRAY_LENGTH(state->frame_state.tiles))
		return GF_NON_COMPLIANT_BITSTREAM;

	state->frame_state.nb_tiles_in_obu = 0;
	for (TileNum = tg_start; TileNum <= tg_end; TileNum++) {
		u32 tile_start_offset, tile_size;
		/*u32 tileRow = TileNum / state->tileCols;
		u32 tileCol = TileNum % state->tileCols;*/
		Bool lastTile = TileNum == tg_end;
		u64 pos = gf_bs_get_position(bs);
		if (lastTile) {
			tile_start_offset = (u32)(pos - obu_start);
			tile_size = (u32)(obu_size - (pos - obu_start));
		}
		else {
			u64 tile_size_minus_1 = aom_av1_le(bs, state->tile_size_bytes, "tile_size_minus_1");
			pos = gf_bs_get_position(bs);
			tile_start_offset = (u32)(pos - obu_start);
			tile_size = (u32)(tile_size_minus_1 + 1/* + state->tile_size_bytes*/);
		}


		if (tile_start_offset + tile_size > obu_size) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_CODING, ("[AV1] Error parsing tile group, tile %d start %d + size %d exceeds OBU length %d\n", TileNum, tile_start_offset, tile_size, obu_size));
			e = GF_NON_COMPLIANT_BITSTREAM;
			break;
		}

		state->frame_state.tiles[state->frame_state.nb_tiles_in_obu].obu_start_offset = tile_start_offset;
		state->frame_state.tiles[state->frame_state.nb_tiles_in_obu].size = tile_size;
		gf_bs_skip_bytes(bs, tile_size);
		state->frame_state.nb_tiles_in_obu++;
	}
	if (tg_end == numTiles - 1) {
		av1_decode_frame_wrapup(state);
	}
	return e;
}

static void av1_parse_frame_header(GF_BitStream *bs, AV1State *state)
{
	AV1StateFrame *frame_state = &state->frame_state;
	if (frame_state->seen_frame_header == GF_FALSE) {
		u64 pos = gf_bs_get_position(bs);
		state->frame_state.show_existing_frame = GF_FALSE;
		frame_state->seen_frame_header = GF_TRUE;
		av1_parse_uncompressed_header(bs, state);
		state->frame_state.is_first_frame = GF_FALSE;
		state->frame_state.uncompressed_header_bytes = (u32) (gf_bs_get_position(bs) - pos);

		if (state->frame_state.show_existing_frame) {
			av1_decode_frame_wrapup(state);
			frame_state->seen_frame_header = GF_FALSE;
		}
		else {
			//TileNum = 0;
			frame_state->seen_frame_header = GF_TRUE;
		}
	}
}

static GF_Err av1_parse_frame(GF_BitStream *bs, AV1State *state, u64 obu_start, u64 obu_size)
{
	av1_parse_frame_header(bs, state);
	//byte alignment
    {
        u32 nbBits = gf_bs_align(bs);
        gf_bs_log_idx(bs, nbBits, "alignment", 0, -1, -1, -1);
    }
	return av1_parse_tile_group(bs, state, obu_start, obu_size);
}

static void av1_parse_obu_metadata(AV1State *state, GF_BitStream *bs)
{
	u32 metadata_type = (u32)gf_av1_leb128_read(bs, NULL);

	switch (metadata_type) {
	case OBU_METADATA_TYPE_ITUT_T35:
		break;
	case OBU_METADATA_TYPE_HDR_CLL:
		gf_bs_read_data(bs, state->clli_data, 4);
		state->clli_valid = 1;
		break;
	case OBU_METADATA_TYPE_HDR_MDCV:
		gf_bs_read_data(bs, state->mdcv_data, 24);
		state->mdcv_valid = 1;
		break;
	default:
		break;
	}
}

GF_EXPORT
GF_Err gf_av1_parse_obu(GF_BitStream *bs, ObuType *obu_type, u64 *obu_size, u32 *obu_hdr_size, AV1State *state)
{
	GF_Err e = GF_OK;
	u32 i, hdr_size;
	u64 pos = gf_bs_get_position(bs);

	if (!bs || !obu_type || !state)
		return GF_BAD_PARAM;

	gf_bs_mark_overflow(bs, GF_TRUE);

	state->obu_extension_flag = state->obu_has_size_field = 0;
	state->temporal_id = state->spatial_id = 0;
	state->frame_state.uncompressed_header_bytes = 0;
	e = gf_av1_parse_obu_header(bs, obu_type, &state->obu_extension_flag, &state->obu_has_size_field, &state->temporal_id, &state->spatial_id);
	if (gf_bs_is_overflow(bs)) e = GF_NON_COMPLIANT_BITSTREAM;
	if (e)
		return e;

	//at this point obu_size is either zero or the size of the containing buffer (likely the Temporal Unit)
	if (state->obu_has_size_field) {
		*obu_size = (u32)gf_av1_leb128_read(bs, NULL);
	}
	else {
		if (*obu_size >= 1 + state->obu_extension_flag) {
			*obu_size = *obu_size - 1 - state->obu_extension_flag;
		}
		else {
			GF_LOG(state->config ? GF_LOG_WARNING : GF_LOG_DEBUG, GF_LOG_CODING, ("[AV1] computed OBU size "LLD" (input value = "LLU"). Skipping.\n", *obu_size - 1 - state->obu_extension_flag, *obu_size));
			return GF_NON_COMPLIANT_BITSTREAM;
		}
	}
	hdr_size = (u32)(gf_bs_get_position(bs) - pos);
	if (gf_bs_is_overflow(bs) || (gf_bs_available(bs) < *obu_size) ) {
		gf_bs_seek(bs, pos);
		return GF_BUFFER_TOO_SMALL;
	}
	//gpac's internal obu_size includes the header + the payload
	*obu_size += hdr_size;
	if (obu_hdr_size) *obu_hdr_size = hdr_size;

	if (*obu_type != OBU_SEQUENCE_HEADER && *obu_type != OBU_TEMPORAL_DELIMITER &&
		state->OperatingPointIdc != 0 && state->obu_extension_flag == 1)
	{
		u32 inTemporalLayer = (state->OperatingPointIdc >> state->temporal_id) & 1;
		u32 inSpatialLayer = (state->OperatingPointIdc >> (state->spatial_id + 8)) & 1;
		if (!inTemporalLayer || !inSpatialLayer) {
			*obu_type = -1;
			gf_bs_seek(bs, pos + *obu_size);
			return GF_OK;
		}
	}

	e = GF_OK;

	/* for AVIF a1lx */
	for (i = state->spatial_id; i < 4; i++) {
		state->layer_size[i] = (u32) (pos + *obu_size);
	}

	switch (*obu_type) {
	case OBU_SEQUENCE_HEADER:
		av1_parse_sequence_header_obu(bs, state);
		if (gf_bs_is_overflow(bs) || (gf_bs_get_position(bs) > pos + *obu_size)) {
			GF_LOG(GF_LOG_WARNING, GF_LOG_CODING, ("[AV1] Sequence header parsing consumed too many bytes !\n"));
			e = GF_NON_COMPLIANT_BITSTREAM;
		}
		gf_bs_seek(bs, pos + *obu_size);
		break;

	case OBU_METADATA:
		av1_parse_obu_metadata(state, bs);
		gf_bs_seek(bs, pos + *obu_size);
		if (gf_bs_is_overflow(bs)) e = GF_NON_COMPLIANT_BITSTREAM;
		break;

	case OBU_FRAME_HEADER:
	case OBU_REDUNDANT_FRAME_HEADER:
		if (state->config) {
			av1_parse_frame_header(bs, state);
			if (gf_bs_is_overflow(bs) || (gf_bs_get_position(bs) > pos + *obu_size)) {
				GF_LOG(GF_LOG_WARNING, GF_LOG_CODING, ("[AV1] Frame header parsing consumed too many bytes !\n"));
				e = GF_NON_COMPLIANT_BITSTREAM;
			}
		}
		gf_bs_seek(bs, pos + *obu_size);
		break;
	case OBU_FRAME:
		e = av1_parse_frame(bs, state, pos, *obu_size);
		if (gf_bs_is_overflow(bs) || (gf_bs_get_position(bs) != pos + *obu_size)) {
			GF_LOG(GF_LOG_WARNING, GF_LOG_CODING, ("[AV1] Frame parsing did not consume the right number of bytes !\n"));
			e = GF_NON_COMPLIANT_BITSTREAM;
		}
		gf_bs_seek(bs, pos + *obu_size);
		break;
	case OBU_TILE_GROUP:
		if (state->config) {
			e = av1_parse_tile_group(bs, state, pos, *obu_size);
			if (gf_bs_is_overflow(bs) || (gf_bs_get_position(bs) != pos + *obu_size)) {
				GF_LOG(GF_LOG_WARNING, GF_LOG_CODING, ("[AV1] Tile group parsing did not consume the right number of bytes !\n"));
				e = GF_NON_COMPLIANT_BITSTREAM;
			}
		}
		gf_bs_seek(bs, pos + *obu_size);
		break;
	case OBU_TEMPORAL_DELIMITER:
		state->frame_state.seen_frame_header = GF_FALSE;
		state->clli_valid = state->mdcv_valid = 0;
	case OBU_PADDING:
		gf_bs_seek(bs, pos + *obu_size);
		break;
	default:
		GF_LOG(GF_LOG_WARNING, GF_LOG_CODING, ("[AV1] unknown OBU type %u (size "LLU"). Skipping.\n", *obu_type, *obu_size));
		gf_bs_seek(bs, pos + *obu_size);
		break;
	}
	return e;
}


GF_EXPORT
GF_Err gf_media_prores_parse_bs(GF_BitStream *bs, GF_ProResFrameInfo *prores_frame)
{
	u32 i, j;
	u64 start, pos;
	memset(prores_frame, 0, sizeof(GF_ProResFrameInfo));

	start = gf_bs_get_position(bs);
	if (gf_bs_available(bs) < 10)
		return GF_BUFFER_TOO_SMALL;

	prores_frame->frame_size = gf_bs_read_u32(bs);
	prores_frame->frame_identifier = gf_bs_read_u32(bs);
	if (prores_frame->frame_identifier != GF_4CC('i','c','p','f')) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_CODING, ("[ProRes] Invalid frame identifier, expected \"icpf\" got \"%s\"\n", gf_4cc_to_str(prores_frame->frame_identifier) ));
		gf_bs_seek(bs, start);
		return GF_NON_COMPLIANT_BITSTREAM;
	}
	/*parse frame header*/
	pos = gf_bs_get_position(bs);
	prores_frame->frame_hdr_size = gf_bs_read_u16(bs);
	if (gf_bs_available(bs) + 2 < prores_frame->frame_hdr_size) {
		gf_bs_seek(bs, start);
		return GF_BUFFER_TOO_SMALL;
	}
	gf_bs_read_u8(bs);
	prores_frame->version = gf_bs_read_u8(bs);
	prores_frame->encoder_id = gf_bs_read_u32(bs);
	prores_frame->width = gf_bs_read_u16(bs);
	prores_frame->height = gf_bs_read_u16(bs);
	prores_frame->chroma_format = gf_bs_read_int(bs, 2);
	gf_bs_read_int(bs, 2);
	prores_frame->interlaced_mode = gf_bs_read_int(bs, 2);
	gf_bs_read_int(bs, 2);
	prores_frame->aspect_ratio_information = gf_bs_read_int(bs, 4);
	prores_frame->framerate_code = gf_bs_read_int(bs, 4);
	prores_frame->color_primaries = gf_bs_read_u8(bs);
	prores_frame->transfer_characteristics = gf_bs_read_u8(bs);
	prores_frame->matrix_coefficients = gf_bs_read_u8(bs);
	gf_bs_read_int(bs, 4);
	prores_frame->alpha_channel_type = gf_bs_read_int(bs, 4);
	gf_bs_read_int(bs, 14);
	prores_frame->load_luma_quant_matrix = gf_bs_read_int(bs, 1);
	prores_frame->load_chroma_quant_matrix = gf_bs_read_int(bs, 1);
	if (prores_frame->load_luma_quant_matrix) {
		for (i=0; i<8; i++) {
			for (j=0; j<8; j++) {
				prores_frame->luma_quant_matrix[i][j] = gf_bs_read_u8(bs);
			}
		}
	}
	if (prores_frame->load_chroma_quant_matrix) {
		for (i=0; i<8; i++) {
			for (j=0; j<8; j++) {
				prores_frame->chroma_quant_matrix[i][j] = gf_bs_read_u8(bs);
			}
		}
	}
	pos = gf_bs_get_position(bs) - pos;
	if (pos != prores_frame->frame_hdr_size) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_CODING, ("[ProRes] Invalid frame header size, expected %d got %d\n", prores_frame->frame_hdr_size, (u32) pos));
		gf_bs_seek(bs, start);
		return GF_NON_COMPLIANT_BITSTREAM;
	}
	prores_frame->nb_pic = ((prores_frame->interlaced_mode==1) || (prores_frame->interlaced_mode==2)) ? 2 : 1;
	gf_bs_seek(bs, start);

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
		return GF_CODECID_MPEG_AUDIO;
	case 2:
	case 0:
		return GF_CODECID_MPEG2_PART3;
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
	u32 lidx;
	/*MPEG-1*/
	if (version & 1) {
		if (!layer) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_CODING, ("[MPEG-1/2 Audio] layer index not valid\n"));
			return 0;
		}
		lidx = layer - 1;
	}
	/*MPEG-2/2.5*/
	else {
		lidx = 3 + (layer >> 1);
	}
	if (lidx>4) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_CODING, ("[MPEG-1/2 Audio] layer index not valid\n"));
		return 0;
	}
	if (bitRateIndex>14) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_CODING, ("[MPEG-1/2 Audio] Bitrate index not valid\n"));
		return 0;
	}
	return bitrate_table[lidx][bitRateIndex];
}



GF_EXPORT
u16 gf_mp3_frame_size(u32 hdr)
{
	u8 version = gf_mp3_version(hdr);
	u8 layer = gf_mp3_layer(hdr);
	u32 pad = ((hdr >> 9) & 0x1) ? 1 : 0;
	u32 bitrate = gf_mp3_bit_rate(hdr);
	u32 samplerate = gf_mp3_sampling_rate(hdr);

	u32 frameSize = 0;
	if (!samplerate || !bitrate) return 0;

	if (layer == 1) {
		frameSize = ((12 * bitrate / samplerate) + pad) * 4;
	}
	else {
		u32 slots_per_frame = 144;
		if ((layer == 3) && !(version & 1)) slots_per_frame = 72;
		frameSize = (slots_per_frame * bitrate / samplerate) + pad;
	}
	return (u16)frameSize;
}


GF_EXPORT
u32 gf_mp3_get_next_header(FILE* in)
{
	u8 b, state = 0;
	u32 dropped = 0;
	unsigned char bytes[4];
	bytes[0] = bytes[1] = bytes[2] = bytes[3] = 0;

	while (1) {
		if (gf_fread(&b, 1, in) == 0) return 0;

		if (state == 3) {
			bytes[state] = b;
			return GF_4CC((u32)bytes[0], bytes[1], bytes[2], bytes[3]);
		}
		if (state == 2) {
			if (((b & 0xF0) == 0) || ((b & 0xF0) == 0xF0) || ((b & 0x0C) == 0x0C)) {
				if (bytes[1] == 0xFF) state = 1;
				else state = 0;
			}
			else {
				bytes[state] = b;
				state = 3;
			}
		}
		if (state == 1) {
			if (((b & 0xE0) == 0xE0) && ((b & 0x18) != 0x08) && ((b & 0x06) != 0)) {
				bytes[state] = b;
				state = 2;
			}
			else {
				state = 0;
			}
		}

		if (state == 0) {
			if (b == 0xFF) {
				bytes[state] = b;
				state = 1;
			}
			else {
				if ((dropped == 0) && ((b & 0xE0) == 0xE0) && ((b & 0x18) != 0x08) && ((b & 0x06) != 0)) {
					bytes[0] = (u8)0xFF;
					bytes[1] = b;
					state = 2;
				}
				else {
					dropped++;
				}
			}
		}
	}
	return 0;
}

GF_EXPORT
u32 gf_mp3_get_next_header_mem(const u8 *buffer, u32 size, u32 *pos)
{
	u32 cur;
	u8 b, state = 0;
	u32 dropped = 0;
	unsigned char bytes[4];
	bytes[0] = bytes[1] = bytes[2] = bytes[3] = 0;

	cur = 0;
	*pos = 0;
	while (cur < size) {
		b = (u8)buffer[cur];
		cur++;

		if (state == 3) {
			u32 val;
			bytes[state] = b;
			val = GF_4CC((u32)bytes[0], bytes[1], bytes[2], bytes[3]);
			if (gf_mp3_frame_size(val)) {
				*pos = dropped;
				return val;
			}
			state = 0;
			dropped = cur;
		}
		if (state == 2) {
			if (((b & 0xF0) == 0) || ((b & 0xF0) == 0xF0) || ((b & 0x0C) == 0x0C)) {
				if (bytes[1] == 0xFF) {
					state = 1;
					dropped += 1;
				}
				else {
					state = 0;
					dropped = cur;
				}
			}
			else {
				bytes[state] = b;
				state = 3;
			}
		}
		if (state == 1) {
			if (((b & 0xE0) == 0xE0) && ((b & 0x18) != 0x08) && ((b & 0x06) != 0)) {
				bytes[state] = b;
				state = 2;
			}
			else {
				state = 0;
				dropped = cur;
			}
		}

		if (state == 0) {
			if (b == 0xFF) {
				bytes[state] = b;
				state = 1;
			}
			else {
				dropped++;
			}
		}
	}
	return 0;
}

#endif /*GPAC_DISABLE_AV_PARSERS*/

Bool gf_avcc_use_extensions(u8 profile_idc)
{
	switch (profile_idc) {
	case 66:
	case 77:
	case 88:
		return GF_FALSE;
	default:
		return GF_TRUE;
	}
}
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
	case 0xF4:
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
GF_EXPORT
const char *gf_avc_hevc_get_chroma_format_name(u8 chroma_format)
{
	switch (chroma_format) {
	case 1:
		return "YUV 4:2:0";
	case 2:
		return "YUV 4:2:2";
	case 3:
		return "YUV 4:4:4";
	default:
		return "Unknown";
	}
}

u32 gf_bs_read_ue_log_idx3(GF_BitStream *bs, const char *fname, s32 idx1, s32 idx2, s32 idx3)
{
	u32 val=0, code;
	s32 nb_lead = -1;
	u32 bits = 0;
	for (code=0; !code; nb_lead++) {
		if (nb_lead>=32) {
			break;
		}
		code = gf_bs_read_int(bs, 1);
		bits++;
	}

	if (nb_lead>=32) {
		if (gf_bs_is_overflow(bs)<2) {
			//gf_bs_read_int keeps returning 0 on EOS, so if no more bits available, rbsp was truncated otherwise code is broken in rbsp)
			//we only test once nb_lead>=32 to avoid testing at each bit read
			if (!gf_bs_available(bs)) {
				GF_LOG(GF_LOG_ERROR, GF_LOG_CODING, ("[Core] exp-golomb read failed, not enough bits in bitstream !\n"));
			} else {
				GF_LOG(GF_LOG_ERROR, GF_LOG_CODING, ("[Core] corrupted exp-golomb code, %d leading zeros, max 31 allowed !\n", nb_lead));
			}
			gf_bs_mark_overflow(bs, GF_FALSE);
		}
		return 0;
	}

	if (nb_lead) {
		u32 leads=1;
		val = gf_bs_read_int(bs, nb_lead);
		leads <<= nb_lead;
		leads -= 1;
		val += leads;
//		val += (1 << nb_lead) - 1;
		bits += nb_lead;
	}

	if (fname) {
		gf_bs_log_idx(bs, bits, fname, val, idx1, idx2, idx3);
	}
	return val;
}

#define gf_bs_read_ue_log_idx2(_bs, _fname, _idx1, _idx2) gf_bs_read_ue_log_idx3(_bs, _fname, (s32) _idx1, (s32) _idx2, -1)
#define gf_bs_read_ue_log_idx(_bs, _fname, _idx) gf_bs_read_ue_log_idx3(_bs, _fname, (s32) _idx, -1, -1)
#define gf_bs_read_ue_log(_bs, _fname) gf_bs_read_ue_log_idx3(_bs, _fname, -1, -1, -1)


u32 gf_bs_read_ue(GF_BitStream *bs)
{
	return gf_bs_read_ue_log(bs, NULL);
}

s32 gf_bs_read_se(GF_BitStream *bs)
{
	u32 v = gf_bs_read_ue(bs);
	if ((v & 0x1) == 0) return (s32)(0 - (v >> 1));
	return (v + 1) >> 1;
}

s32 gf_bs_read_se_log_idx2(GF_BitStream *bs, const char *fname, s32 idx1, s32 idx2)
{
	s32 res = gf_bs_read_se(bs);
	if (fname)
		gf_bs_log_idx(bs, -1, fname, res, idx1, idx2, -1);
	return res;
}
#define gf_bs_read_se_log_idx(_bs, _fname, _idx) gf_bs_read_se_log_idx2(_bs, _fname, (s32) _idx, -1)
#define gf_bs_read_se_log(_bs, _fname) gf_bs_read_se_log_idx2(_bs, _fname, -1, -1)



void gf_bs_write_ue(GF_BitStream *bs, u32 num) {
	s32 length = 1;
	s32 temp = ++num;

	while (temp != 1) {
		temp >>= 1;
		length += 2;
	}

	gf_bs_write_int(bs, 0, length >> 1);
	gf_bs_write_int(bs, num, (length + 1) >> 1);
}

static u32 gf_get_ue_nb_bits(u32 num) {
	s32 length = 1;
	s32 temp = ++num;

	while (temp != 1) {
		temp >>= 1;
		length += 2;
	}

	return (length >> 1) + ( (length + 1) >> 1);
}

void gf_bs_write_se(GF_BitStream *bs, s32 num)
{
	u32 v;
	if (num <= 0)
		v = (-1 * num) << 1;
	else
		v = (num << 1) - 1;

	gf_bs_write_ue(bs, v);
}

#ifndef GPAC_DISABLE_AV_PARSERS

GF_EXPORT
u32 gf_media_nalu_next_start_code(const u8 *data, u32 data_len, u32 *sc_size)
{
	u32 avail = data_len;
	const u8 *cur = data;

	while (cur) {
		u32 v, bpos;
		u8 *next_zero = memchr(cur, 0, avail);
		if (!next_zero) return data_len;

		v = 0xffffff00;
		bpos = (u32)(next_zero - data) + 1;
		while (1) {
			u8 cval;
			if (bpos == (u32)data_len)
				return data_len;

			cval = data[bpos];
			v = ((v << 8) & 0xFFFFFF00) | ((u32)cval);
			bpos++;
			if (v == 0x00000001) {
				*sc_size = 4;
				return bpos - 4;
			}
			else if ((v & 0x00FFFFFF) == 0x00000001) {
				*sc_size = 3;
				return bpos - 3;
			}
			if (cval)
				break;
		}
		if (bpos >= data_len)
			break;
		cur = data + bpos;
		avail = data_len - bpos;
	}
	return data_len;
}

Bool gf_avc_slice_is_intra(AVCState *avc)
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

#if 0 //unused
Bool gf_avc_slice_is_IDR(AVCState *avc)
{
	if (avc->sei.recovery_point.valid)
	{
		avc->sei.recovery_point.valid = 0;
		return 1;
	}
	if (avc->s_info.nal_unit_type != GF_AVC_NALU_IDR_SLICE)
		return 0;
	return gf_avc_slice_is_intra(avc);
}
#endif

static const struct  {
	u32 w, h;
} avc_hevc_sar[] = {
	{ 0,   0 }, { 1,   1 }, { 12, 11 }, { 10, 11 },
	{ 16, 11 }, { 40, 33 }, { 24, 11 }, { 20, 11 },
	{ 32, 11 }, { 80, 33 }, { 18, 11 }, { 15, 11 },
	{ 64, 33 }, { 160,99 }, {  4,  3 }, {  3,  2 },
	{  2,  1 }
};


/*ISO 14496-10 (N11084) E.1.2*/
static s32 avc_parse_hrd_parameters(GF_BitStream *bs, AVC_HRD *hrd)
{
	int i, cpb_cnt_minus1;

	cpb_cnt_minus1 = gf_bs_read_ue_log(bs, "cpb_cnt_minus1");
	if (cpb_cnt_minus1 > 31) {
		GF_LOG(GF_LOG_WARNING, GF_LOG_CODING, ("[avc-h264] invalid cpb_cnt_minus1 value: %d (expected in [0;31])\n", cpb_cnt_minus1));
		return -1;
	}
	gf_bs_read_int_log(bs, 4, "bit_rate_scale");
	gf_bs_read_int_log(bs, 4, "cpb_size_scale");

	/*for( SchedSelIdx = 0; SchedSelIdx <= cpb_cnt_minus1; SchedSelIdx++ ) {*/
	for (i = 0; i <= cpb_cnt_minus1; i++) {
		gf_bs_read_ue_log_idx(bs, "bit_rate_value_minus1", i);
		gf_bs_read_ue_log_idx(bs, "cpb_size_value_minus1", i);
		gf_bs_read_int_log_idx(bs, 1, "cbr_flag", i);
	}
	gf_bs_read_int_log(bs, 5, "initial_cpb_removal_delay_length_minus1");
	hrd->cpb_removal_delay_length_minus1 = gf_bs_read_int_log(bs, 5, "cpb_removal_delay_length_minus1");
	hrd->dpb_output_delay_length_minus1 = gf_bs_read_int_log(bs, 5, "dpb_output_delay_length_minus1");
	hrd->time_offset_length = gf_bs_read_int_log(bs, 5, "time_offset_length");
	return 0;
}

/*returns the nal_size without emulation prevention bytes*/
u32 gf_media_nalu_emulation_bytes_add_count(u8 *buffer, u32 nal_size)
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
		if (num_zero == 2 && (u8)buffer[i] < 0x04) {
			/*emulation code found*/
			num_zero = 0;
			emulation_bytes_count++;
			if (!buffer[i])
				num_zero = 1;
		}
		else {
			if (!buffer[i])
				num_zero++;
			else
				num_zero = 0;
		}
		i++;
	}
	return emulation_bytes_count;
}

u32 gf_media_nalu_add_emulation_bytes(const u8 *buffer_src, u8 *buffer_dst, u32 nal_size)
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
		if (num_zero == 2 && (u8)buffer_src[i] < 0x04) {
			/*add emulation code*/
			num_zero = 0;
			buffer_dst[i + emulation_bytes_count] = 0x03;
			emulation_bytes_count++;
			if (!buffer_src[i])
				num_zero = 1;
		}
		else {
			if (!buffer_src[i])
				num_zero++;
			else
				num_zero = 0;
		}
		buffer_dst[i + emulation_bytes_count] = buffer_src[i];
		i++;
	}
	return nal_size + emulation_bytes_count;
}

/*returns the nal_size without emulation prevention bytes*/
u32 gf_media_nalu_emulation_bytes_remove_count(const u8 *buffer, u32 nal_size)
{
	u32 i = 0, emulation_bytes_count = 0;
	u8 num_zero = 0;
	if (!buffer || !nal_size) return 0;

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
			&& i + 1 < nal_size /*next byte is readable*/
			&& (u8)buffer[i + 1] < 0x04)
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

/*nal_size is updated to allow better error detection*/
GF_EXPORT
u32 gf_media_nalu_remove_emulation_bytes(const u8 *buffer_src, u8 *buffer_dst, u32 nal_size)
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
			&& i + 1 < nal_size /*next byte is readable*/
			&& (u8)buffer_src[i + 1] < 0x04)
		{
			/*emulation code found*/
			num_zero = 0;
			emulation_bytes_count++;
			i++;
		}

		buffer_dst[i - emulation_bytes_count] = buffer_src[i];

		if (!buffer_src[i])
			num_zero++;
		else
			num_zero = 0;

		i++;
	}

	return nal_size - emulation_bytes_count;
}

#define AVC_SPS_BROKEN {\
	memset(sps, 0, sizeof(AVC_SPS)); \
	return -1;\
	}

static s32 gf_avc_read_sps_bs_internal(GF_BitStream *bs, AVCState *avc, u32 subseq_sps, u32 *vui_flag_pos, u32 nal_hdr)
{
	AVC_SPS *sps;
	s32 mb_width, mb_height, sps_id = -1;
	u32 profile_idc, level_idc, pcomp, i, chroma_format_idc, cl = 0, cr = 0, ct = 0, cb = 0, luma_bd, chroma_bd;
	u8 separate_colour_plane_flag = 0;

	if (!vui_flag_pos) {
		gf_bs_enable_emulation_byte_removal(bs, GF_TRUE);
	}

	if (!bs) {
		return -1;
	}

	if (!nal_hdr) {
		gf_bs_read_int_log(bs, 1, "forbidden_zero_bit");
		gf_bs_read_int_log(bs, 2, "nal_ref_idc");
		gf_bs_read_int_log(bs, 5, "nal_unit_type");
	}
	profile_idc = gf_bs_read_int_log(bs, 8, "profile_idc");

	pcomp = gf_bs_read_int_log(bs, 8, "profile_compatibility");
	/*sanity checks*/
	if (pcomp & 0x3)
		return -1;

	level_idc = gf_bs_read_int_log(bs, 8, "level_idc");

	/*SubsetSps is used to be sure that AVC SPS are not going to be scratched
	by subset SPS. According to the SVC standard, subset SPS can have the same sps_id
	than its base layer, but it does not refer to the same SPS. */
	sps_id = gf_bs_read_ue_log(bs, "sps_id") + GF_SVC_SSPS_ID_SHIFT * subseq_sps;
	if ((sps_id < 0) || (sps_id >= 32)) {
		return -1;
	}

	luma_bd = chroma_bd = 0;
	sps = &avc->sps[sps_id];
	chroma_format_idc = sps->ChromaArrayType = 1;
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
			AVC_SPS_BROKEN
	case 83:
	case 86:
	case 118:
	case 128:
		chroma_format_idc = gf_bs_read_ue_log(bs, "chroma_format_idc");
		sps->ChromaArrayType = chroma_format_idc;
		if (chroma_format_idc == 3) {
			separate_colour_plane_flag = gf_bs_read_int_log(bs, 1, "separate_colour_plane_flag");
			/*
			Depending on the value of separate_colour_plane_flag, the value of the variable ChromaArrayType is assigned as follows.
			\96	If separate_colour_plane_flag is equal to 0, ChromaArrayType is set equal to chroma_format_idc.
			\96	Otherwise (separate_colour_plane_flag is equal to 1), ChromaArrayType is set equal to 0.
			*/
			if (separate_colour_plane_flag) sps->ChromaArrayType = 0;
		}
		luma_bd = gf_bs_read_ue_log(bs, "luma_bit_depth");
		chroma_bd = gf_bs_read_ue_log(bs, "chroma_bit_depth");
		/*qpprime_y_zero_transform_bypass_flag = */ gf_bs_read_int_log(bs, 1, "qpprime_y_zero_transform_bypass_flag");
		/*seq_scaling_matrix_present_flag*/
		if (gf_bs_read_int_log(bs, 1, "seq_scaling_matrix_present_flag")) {
			u32 k;
			for (k = 0; k < 8; k++) {
				if (gf_bs_read_int_log_idx(bs, 1, "seq_scaling_list_present_flag", k)) {
					u32 z, last = 8, next = 8;
					u32 sl = k < 6 ? 16 : 64;
					for (z = 0; z < sl; z++) {
						if (next) {
							s32 delta = gf_bs_read_se(bs);
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
	sps->log2_max_frame_num = gf_bs_read_ue_log(bs, "log2_max_frame_num") + 4;
	if (sps->log2_max_frame_num>16) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_CODING, ("[avc-h264] invalid SPS: log2_max_frame_num_minus4 shall be less than 12, but is %d\n", sps->log2_max_frame_num-4));
		AVC_SPS_BROKEN
	}
	sps->poc_type = gf_bs_read_ue_log(bs, "poc_type");
	if (sps->poc_type>2) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_CODING, ("[avc-h264] invalid SPS: pic_order_cnt_type shall be less than 2, but is %d\n", sps->poc_type));
		AVC_SPS_BROKEN
	}
	sps->chroma_format = chroma_format_idc;
	sps->luma_bit_depth_m8 = luma_bd;
	sps->chroma_bit_depth_m8 = chroma_bd;

	if (sps->poc_type == 0) {
		sps->log2_max_poc_lsb = gf_bs_read_ue_log(bs, "log2_max_poc_lsb") + 4;
		//log2_max_poc_lsb shall be in the range of 0 to 12, inclusive
		if (sps->log2_max_poc_lsb>16) {
			AVC_SPS_BROKEN
		}
	}
	else if (sps->poc_type == 1) {
		sps->delta_pic_order_always_zero_flag = gf_bs_read_int_log(bs, 1, "delta_pic_order_always_zero_flag");
		sps->offset_for_non_ref_pic = gf_bs_read_se_log(bs, "offset_for_non_ref_pic");
		sps->offset_for_top_to_bottom_field = gf_bs_read_se_log(bs, "offset_for_top_to_bottom_field");
		sps->poc_cycle_length = gf_bs_read_ue_log(bs, "poc_cycle_length");
		if (sps->poc_cycle_length > GF_ARRAY_LENGTH(sps->offset_for_ref_frame)) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_CODING, ("[avc-h264] offset_for_ref_frame overflow from poc_cycle_length\n"));
			AVC_SPS_BROKEN
		}
		for (i = 0; i < sps->poc_cycle_length; i++)
			sps->offset_for_ref_frame[i] = gf_bs_read_se_log_idx(bs, "offset_for_ref_frame", i);
	}
	if (sps->poc_type > 2) {
		AVC_SPS_BROKEN
	}
	sps->max_num_ref_frames = gf_bs_read_ue_log(bs, "max_num_ref_frames");
	sps->gaps_in_frame_num_value_allowed_flag = gf_bs_read_int_log(bs, 1, "gaps_in_frame_num_value_allowed_flag");
	mb_width = gf_bs_read_ue_log(bs, "pic_width_in_mbs_minus1") + 1;
	mb_height = gf_bs_read_ue_log(bs, "pic_height_in_map_units_minus1") + 1;
	//5.1 level max frame size in MBs is 36864, we set our limit at 16k x 16x pixels (eg 1 M MBs) for fuzzed stream detection
	if ( (u64) mb_width * (u64) mb_height > 1000000) {
		AVC_SPS_BROKEN
	}

	sps->frame_mbs_only_flag = gf_bs_read_int_log(bs, 1, "frame_mbs_only_flag");

	sps->width = mb_width * 16;
	sps->height = (2 - sps->frame_mbs_only_flag) * mb_height * 16;

	if (!sps->frame_mbs_only_flag) sps->mb_adaptive_frame_field_flag = gf_bs_read_int_log(bs, 1, "mb_adaptive_frame_field_flag");
	gf_bs_read_int_log(bs, 1, "direct_8x8_inference_flag");

	if (gf_bs_read_int_log(bs, 1, "frame_cropping_flag")) {
		int CropUnitX, CropUnitY, SubWidthC = -1, SubHeightC = -1;

		if (chroma_format_idc == 1) {
			SubWidthC = 2; SubHeightC = 2;
		}
		else if (chroma_format_idc == 2) {
			SubWidthC = 2; SubHeightC = 1;
		}
		else if ((chroma_format_idc == 3) && (separate_colour_plane_flag == 0)) {
			SubWidthC = 1; SubHeightC = 1;
		}

		if (sps->ChromaArrayType == 0) {
			gf_assert(SubWidthC == -1);
			CropUnitX = 1;
			CropUnitY = 2 - sps->frame_mbs_only_flag;
		}
		else {
			CropUnitX = SubWidthC;
			CropUnitY = SubHeightC * (2 - sps->frame_mbs_only_flag);
		}

		cl = gf_bs_read_ue_log(bs, "frame_crop_left_offset");
		cr = gf_bs_read_ue_log(bs, "frame_crop_right_offset");
		ct = gf_bs_read_ue_log(bs, "frame_crop_top_offset");
		cb = gf_bs_read_ue_log(bs, "frame_crop_bottom_offset");

		sps->width -= CropUnitX * (cl + cr);
		sps->height -= CropUnitY * (ct + cb);
		cl *= CropUnitX;
		cr *= CropUnitX;
		ct *= CropUnitY;
		cb *= CropUnitY;
	}
	sps->crop.left = cl;
	sps->crop.right = cr;
	sps->crop.top = ct;
	sps->crop.bottom = cb;

	if (vui_flag_pos) {
		*vui_flag_pos = (u32)gf_bs_get_bit_offset(bs);
	}
	/*vui_parameters_present_flag*/
	sps->vui_parameters_present_flag = gf_bs_read_int_log(bs, 1, "vui_parameters_present_flag");
	if (sps->vui_parameters_present_flag) {
		sps->vui.aspect_ratio_info_present_flag = gf_bs_read_int_log(bs, 1, "aspect_ratio_info_present_flag");
		if (sps->vui.aspect_ratio_info_present_flag) {
			s32 aspect_ratio_idc = gf_bs_read_int_log(bs, 8, "aspect_ratio_idc");
			if (aspect_ratio_idc == 255) {
				sps->vui.par_num = gf_bs_read_int_log(bs, 16, "aspect_ratio_num");
				sps->vui.par_den = gf_bs_read_int_log(bs, 16, "aspect_ratio_den");
			}
			else if (aspect_ratio_idc < GF_ARRAY_LENGTH(avc_hevc_sar) ) {
				sps->vui.par_num = avc_hevc_sar[aspect_ratio_idc].w;
				sps->vui.par_den = avc_hevc_sar[aspect_ratio_idc].h;
			}
			else {
				GF_LOG(GF_LOG_WARNING, GF_LOG_CODING, ("[avc-h264] Unknown aspect_ratio_idc: your video may have a wrong aspect ratio. Contact the GPAC team!\n"));
			}
		}
		sps->vui.overscan_info_present_flag = gf_bs_read_int_log(bs, 1, "overscan_info_present_flag");
		if (sps->vui.overscan_info_present_flag)
			gf_bs_read_int_log(bs, 1, "overscan_appropriate_flag");

		/* default values */
		sps->vui.video_format = 5;
		sps->vui.colour_primaries = 2;
		sps->vui.transfer_characteristics = 2;
		sps->vui.matrix_coefficients = 2;
		//When the chroma_sample_loc_type_top_field and chroma_sample_loc_type_bottom_field are not present, the values of chroma_sample_loc_type_top_field and chroma_sample_loc_type_bottom_field shall be inferred to be equal to 0.
		sps->vui.chroma_sample_loc_type_top_field = sps->vui.chroma_sample_loc_type_bottom_field = 0;

		/* now read values if possible */
		sps->vui.video_signal_type_present_flag = gf_bs_read_int_log(bs, 1, "video_signal_type_present_flag");
		if (sps->vui.video_signal_type_present_flag) {
			sps->vui.video_format = gf_bs_read_int_log(bs, 3, "video_format");
			sps->vui.video_full_range_flag = gf_bs_read_int_log(bs, 1, "video_full_range_flag");
			sps->vui.colour_description_present_flag = gf_bs_read_int_log(bs, 1, "colour_description_present_flag");
			if (sps->vui.colour_description_present_flag) {
				sps->vui.colour_primaries = gf_bs_read_int_log(bs, 8, "colour_primaries");
				sps->vui.transfer_characteristics = gf_bs_read_int_log(bs, 8, "transfer_characteristics");
				sps->vui.matrix_coefficients = gf_bs_read_int_log(bs, 8, "matrix_coefficients");
			}
		}

		sps->vui.chroma_location_info_present_flag = gf_bs_read_int_log(bs, 1, "chroma_location_info_present_flag");
		if (sps->vui.chroma_location_info_present_flag) {
			sps->vui.chroma_sample_loc_type_top_field = gf_bs_read_ue_log(bs, "chroma_sample_location_type_top_field");
			sps->vui.chroma_sample_loc_type_bottom_field = gf_bs_read_ue_log(bs, "chroma_sample_location_type_bottom_field");
		}

		sps->vui.timing_info_present_flag = gf_bs_read_int_log(bs, 1, "timing_info_present_flag");
		if (sps->vui.timing_info_present_flag) {
			sps->vui.num_units_in_tick = gf_bs_read_int_log(bs, 32, "num_units_in_tick");
			sps->vui.time_scale = gf_bs_read_int_log(bs, 32, "time_scale");
			sps->vui.fixed_frame_rate_flag = gf_bs_read_int_log(bs, 1, "fixed_frame_rate_flag");
		}

		sps->vui.nal_hrd_parameters_present_flag = gf_bs_read_int_log(bs, 1, "nal_hrd_parameters_present_flag");
		if (sps->vui.nal_hrd_parameters_present_flag)
			if (avc_parse_hrd_parameters(bs, &sps->vui.hrd)<0)
				AVC_SPS_BROKEN

		sps->vui.vcl_hrd_parameters_present_flag = gf_bs_read_int_log(bs, 1, "vcl_hrd_parameters_present_flag");
		if (sps->vui.vcl_hrd_parameters_present_flag)
			if (avc_parse_hrd_parameters(bs, &sps->vui.hrd)<0)
				AVC_SPS_BROKEN

		if (sps->vui.nal_hrd_parameters_present_flag || sps->vui.vcl_hrd_parameters_present_flag)
			sps->vui.low_delay_hrd_flag = gf_bs_read_int_log(bs, 1, "low_delay_hrd_flag");

		sps->vui.pic_struct_present_flag = gf_bs_read_int_log(bs, 1, "pic_struct_present_flag");
	}
	/*end of seq_parameter_set_data*/

	if (subseq_sps) {
		if ((profile_idc == 83) || (profile_idc == 86)) {
			u8 extended_spatial_scalability_idc;
			/*parsing seq_parameter_set_svc_extension*/

			gf_bs_read_int_log(bs, 1, "inter_layer_deblocking_filter_control_present_flag");
			extended_spatial_scalability_idc = gf_bs_read_int_log(bs, 2, "extended_spatial_scalability_idc");
			if (sps->ChromaArrayType == 1 || sps->ChromaArrayType == 2) {
				gf_bs_read_int_log(bs, 1, "chroma_phase_x_plus1_flag");
			}
			if (sps->ChromaArrayType == 1) {
				gf_bs_read_int_log(bs, 2, "chroma_phase_y_plus1");
			}
			if (extended_spatial_scalability_idc == 1) {
				if (sps->ChromaArrayType > 0) {
					gf_bs_read_int_log(bs, 1, "seq_ref_layer_chroma_phase_x_plus1_flag");
					gf_bs_read_int_log(bs, 2, "seq_ref_layer_chroma_phase_y_plus1");
				}
				gf_bs_read_se_log(bs, "seq_scaled_ref_layer_left_offset");
				gf_bs_read_se_log(bs, "seq_scaled_ref_layer_top_offset");
				gf_bs_read_se_log(bs, "seq_scaled_ref_layer_right_offset");
				gf_bs_read_se_log(bs, "seq_scaled_ref_layer_bottom_offset");
			}
			if (gf_bs_read_int_log(bs, 1, "seq_tcoeff_level_prediction_flag")) {
				gf_bs_read_int_log(bs, 1, "adaptive_tcoeff_level_prediction_flag");
			}
			gf_bs_read_int_log(bs, 1, "slice_header_restriction_flag");

			if (gf_bs_read_int_log(bs, 1, "svc_vui_parameters_present")) {
				u32 vui_ext_num_entries_minus1 = gf_bs_read_ue_log(bs, "vui_ext_num_entries_minus1");

				for (i = 0; i <= vui_ext_num_entries_minus1; i++) {
					u8 vui_ext_nal_hrd_parameters_present_flag, vui_ext_vcl_hrd_parameters_present_flag, vui_ext_timing_info_present_flag;
					gf_bs_read_int_log(bs, 3, "vui_ext_dependency_id");
					gf_bs_read_int_log(bs, 4, "vui_ext_quality_id");
					gf_bs_read_int_log(bs, 3, "vui_ext_temporal_id");
					vui_ext_timing_info_present_flag = gf_bs_read_int_log(bs, 1, "vui_ext_timing_info_present_flag");
					if (vui_ext_timing_info_present_flag) {
						gf_bs_read_int_log(bs, 32, "vui_ext_num_units_in_tick");
						gf_bs_read_int_log(bs, 32, "vui_ext_time_scale");
						gf_bs_read_int_log(bs, 1, "vui_ext_fixed_frame_rate_flag");
					}
					vui_ext_nal_hrd_parameters_present_flag = gf_bs_read_int_log(bs, 1, "vui_ext_nal_hrd_parameters_present_flag");
					if (vui_ext_nal_hrd_parameters_present_flag) {
						//hrd_parameters( )
					}
					vui_ext_vcl_hrd_parameters_present_flag = gf_bs_read_int_log(bs, 1, "vui_ext_vcl_hrd_parameters_present_flag");
					if (vui_ext_vcl_hrd_parameters_present_flag) {
						//hrd_parameters( )
					}
					if (vui_ext_nal_hrd_parameters_present_flag || vui_ext_vcl_hrd_parameters_present_flag) {
						gf_bs_read_int_log(bs, 1, "vui_ext_low_delay_hrd_flag");
					}
					gf_bs_read_int_log(bs, 1, "vui_ext_pic_struct_present_flag");
				}
			}
		}
		else if ((profile_idc == 118) || (profile_idc == 128)) {
			GF_LOG(GF_LOG_INFO, GF_LOG_CODING, ("[avc-h264] MVC parsing not implemented - skipping parsing end of Subset SPS\n"));
			return sps_id;
		}

		if (gf_bs_read_int_log(bs, 1, "additional_extension2")) {
			GF_LOG(GF_LOG_WARNING, GF_LOG_CODING, ("[avc-h264] skipping parsing end of Subset SPS (additional_extension2)\n"));
			return sps_id;
		}
	}
	if (gf_bs_is_overflow(bs))
		AVC_SPS_BROKEN
	return sps_id;
}

GF_EXPORT
s32 gf_avc_read_sps_bs(GF_BitStream *bs, AVCState *avc, u32 subseq_sps, u32 *vui_flag_pos)
{
	return gf_avc_read_sps_bs_internal(bs, avc, subseq_sps, vui_flag_pos, 0);
}

GF_EXPORT
s32 gf_avc_read_sps(const u8 *sps_data, u32 sps_size, AVCState *avc, u32 subseq_sps, u32 *vui_flag_pos)
{
	s32 sps_id = -1;
	GF_BitStream *bs;
	char *sps_data_without_emulation_bytes = NULL;
	u32 sps_data_without_emulation_bytes_size = 0;

	if (vui_flag_pos) {
		/*SPS still contains emulation bytes*/
		sps_data_without_emulation_bytes = gf_malloc(sps_size * sizeof(char));
		sps_data_without_emulation_bytes_size = gf_media_nalu_remove_emulation_bytes(sps_data, sps_data_without_emulation_bytes, sps_size);
		bs = gf_bs_new(sps_data_without_emulation_bytes, sps_data_without_emulation_bytes_size, GF_BITSTREAM_READ);

		*vui_flag_pos = 0;
	}
	else {
		bs = gf_bs_new(sps_data, sps_size, GF_BITSTREAM_READ);
	}

	if (!bs) {
		sps_id = -1;
		goto exit;
	}

	sps_id = gf_avc_read_sps_bs(bs, avc, subseq_sps, vui_flag_pos);

exit:
	gf_bs_del(bs);
	if (sps_data_without_emulation_bytes) gf_free(sps_data_without_emulation_bytes);
	return sps_id;
}

#define AVC_PPS_BROKEN {\
	memset(pps, 0, sizeof(AVC_PPS)); \
	return -1;\
	}

static s32 gf_avc_read_pps_bs_internal(GF_BitStream *bs, AVCState *avc, u32 nal_hdr)
{
	s32 pps_id;
	AVC_PPS *pps;

	gf_bs_enable_emulation_byte_removal(bs, GF_TRUE);

	if (!nal_hdr) {
		gf_bs_read_int_log(bs, 1, "forbidden_zero_bit");
		gf_bs_read_int_log(bs, 2, "nal_ref_idc");
		gf_bs_read_int_log(bs, 5, "nal_unit_type");
	}
	pps_id = gf_bs_read_ue_log(bs, "pps_id");
	if ((pps_id<0) || (pps_id >= 255)) {
		return -1;
	}
	pps = &avc->pps[pps_id];
	pps->id = pps_id;

	if (!pps->status) pps->status = 1;
	pps->sps_id = gf_bs_read_ue_log(bs, "sps_id");
	if ((pps->sps_id<0) || (pps->sps_id >= 32)) {
		AVC_PPS_BROKEN
	}
	/*sps_id may be refer to regular SPS or subseq sps, depending on the coded slice referring to the pps*/
	if (!avc->sps[pps->sps_id].state
		&& ((pps->sps_id + GF_SVC_SSPS_ID_SHIFT<32) && !avc->sps[pps->sps_id + GF_SVC_SSPS_ID_SHIFT].state)
	) {
		AVC_PPS_BROKEN
	}
	avc->pps_active_idx = pps->id; /*set active sps*/
	avc->sps_active_idx = pps->sps_id; /*set active sps*/
	pps->entropy_coding_mode_flag = gf_bs_read_int_log(bs, 1, "entropy_coding_mode_flag");
	pps->pic_order_present = gf_bs_read_int_log(bs, 1, "pic_order_present");
	pps->slice_group_count = gf_bs_read_ue_log(bs, "slice_group_count_minus1") + 1;
	if (pps->slice_group_count > 1) {
		u32 iGroup;
		pps->mb_slice_group_map_type = gf_bs_read_ue_log(bs, "mb_slice_group_map_type");
		if (pps->mb_slice_group_map_type == 0) {
			for (iGroup = 0; iGroup <= pps->slice_group_count - 1; iGroup++)
				gf_bs_read_ue_log_idx(bs, "run_length_minus1", iGroup);
		}
		else if (pps->mb_slice_group_map_type == 2) {
			for (iGroup = 0; iGroup < pps->slice_group_count - 1; iGroup++) {
				gf_bs_read_ue_log_idx(bs, "top_left", iGroup);
				gf_bs_read_ue_log_idx(bs, "bottom_right", iGroup);
			}
		}
		else if (pps->mb_slice_group_map_type == 3 || pps->mb_slice_group_map_type == 4 || pps->mb_slice_group_map_type == 5) {
			gf_bs_read_int_log(bs, 1, "slice_group_change_direction_flag");
			gf_bs_read_ue_log(bs, "slice_group_change_rate_minus1");
		}
		else if (pps->mb_slice_group_map_type == 6) {
			u32 i;
			pps->pic_size_in_map_units_minus1 = gf_bs_read_ue_log(bs, "pic_size_in_map_units_minus1");
			for (i = 0; i <= pps->pic_size_in_map_units_minus1; i++) {
				gf_bs_read_int_log_idx(bs, (u32)ceil(log(pps->slice_group_count) / log(2)), "slice_group_id", i);
			}
		}
	}
	pps->num_ref_idx_l0_default_active_minus1 = gf_bs_read_ue_log(bs, "num_ref_idx_l0_default_active_minus1");
	pps->num_ref_idx_l1_default_active_minus1 = gf_bs_read_ue_log(bs, "num_ref_idx_l1_default_active_minus1");

	/*
	if ((pps->ref_count[0] > 32) || (pps->ref_count[1] > 32)) goto exit;
	*/

	pps->weighted_pred_flag = gf_bs_read_int_log(bs, 1, "weighted_pred_flag");
	gf_bs_read_int_log(bs, 2, "weighted_bipred_idc");
	gf_bs_read_se_log(bs, "init_qp_minus26");
	gf_bs_read_se_log(bs, "init_qs_minus26");
	gf_bs_read_se_log(bs, "chroma_qp_index_offset");
	pps->deblocking_filter_control_present_flag = gf_bs_read_int_log(bs, 1, "deblocking_filter_control_present_flag");
	gf_bs_read_int_log(bs, 1, "constrained_intra_pred");
	pps->redundant_pic_cnt_present = gf_bs_read_int_log(bs, 1, "redundant_pic_cnt_present");

	if (gf_bs_is_overflow(bs))
		AVC_PPS_BROKEN
	return pps_id;
}

GF_EXPORT
s32 gf_avc_read_pps_bs(GF_BitStream *bs, AVCState *avc)
{
	return gf_avc_read_pps_bs_internal(bs, avc, 0);
}

GF_EXPORT
s32 gf_avc_read_pps(const u8 *pps_data, u32 pps_size, AVCState *avc)
{
	GF_BitStream *bs;
	s32 pps_id;

	/*PPS still contains emulation bytes*/
	bs = gf_bs_new(pps_data, pps_size, GF_BITSTREAM_READ);
	if (!bs) {
		return -1;
	}
	gf_bs_enable_emulation_byte_removal(bs, GF_TRUE);
	pps_id = gf_avc_read_pps_bs(bs, avc);
	gf_bs_del(bs);
	return pps_id;
}

#if 0 //unused

s32 gf_avc_read_sps_ext(const char *spse_data, u32 spse_size)
{
	GF_BitStream *bs;
	s32 sps_id;

	bs = gf_bs_new(spse_data, spse_size, GF_BITSTREAM_READ);
	sps_id = gf_avc_read_sps_ext_bs(bs);

	gf_bs_del(bs);
	return sps_id;
}
#endif

static s32 SVC_ReadNal_header_extension(GF_BitStream *bs, SVC_NALUHeader *NalHeader)
{
	gf_bs_read_int_log(bs, 1, "reserved_one_bit");
	NalHeader->idr_pic_flag = gf_bs_read_int_log(bs, 1, "idr_flag");
	NalHeader->priority_id = gf_bs_read_int_log(bs, 6, "priority_id");
	gf_bs_read_int_log(bs, 1, "no_inter_layer_pred_flag");
	NalHeader->dependency_id = gf_bs_read_int_log(bs, 3, "DependencyId");
	NalHeader->quality_id = gf_bs_read_int_log(bs, 4, "quality_id");
	NalHeader->temporal_id = gf_bs_read_int_log(bs, 3, "temporal_id");
	gf_bs_read_int_log(bs, 1, "use_ref_base_pic_flag");
	gf_bs_read_int_log(bs, 1, "discardable_flag");
	gf_bs_read_int_log(bs, 1, "output_flag");
	gf_bs_read_int_log(bs, 2, "reserved_three_2bits");
	return 1;
}

static void ref_pic_list_modification(GF_BitStream *bs, u32 slice_type) {
	if (slice_type % 5 != 2 && slice_type % 5 != 4) {
		if (gf_bs_read_int_log(bs, 1, "ref_pic_list_modification_flag_l0")) {
			u32 idx=0, modification_of_pic_nums_idc;
			do {
				modification_of_pic_nums_idc = gf_bs_read_ue_log_idx(bs, "modification_of_pic_nums_idc", idx);
				if (modification_of_pic_nums_idc == 0 || modification_of_pic_nums_idc == 1) {
					gf_bs_read_ue_log_idx(bs, "abs_diff_pic_num_minus1", idx);
				}
				else if (modification_of_pic_nums_idc == 2) {
					gf_bs_read_ue_log_idx(bs, "long_term_pic_num", idx);
				}
				idx++;
			} while ((modification_of_pic_nums_idc != 3) && gf_bs_available(bs));
		}
	}
	if (slice_type % 5 == 1) {
		if (gf_bs_read_int_log(bs, 1, "ref_pic_list_modification_flag_l1")) {
			u32 idx=0, modification_of_pic_nums_idc;
			do {
				modification_of_pic_nums_idc = gf_bs_read_ue_log_idx(bs, "modification_of_pic_nums_idc", idx);
				if (modification_of_pic_nums_idc == 0 || modification_of_pic_nums_idc == 1) {
					gf_bs_read_ue_log_idx(bs, "abs_diff_pic_num_minus1", idx);
				}
				else if (modification_of_pic_nums_idc == 2) {
					gf_bs_read_ue_log_idx(bs, "long_term_pic_num", idx);
				}
				idx++;
			} while ((modification_of_pic_nums_idc != 3) && gf_bs_available(bs));
		}
	}
}

static void avc_pred_weight_table(GF_BitStream *bs, u32 slice_type, u32 ChromaArrayType, u32 num_ref_idx_l0_active_minus1, u32 num_ref_idx_l1_active_minus1) {
	u32 i, j;
	gf_bs_read_ue_log(bs, "luma_log2_weight_denom");
	if (ChromaArrayType != 0) {
		gf_bs_read_ue_log(bs, "chroma_log2_weight_denom");
	}
	for (i = 0; i <= num_ref_idx_l0_active_minus1; i++) {
		if (gf_bs_read_int_log_idx(bs, 1, "luma_weight_l0_flag", i)) {
			gf_bs_read_se_log_idx(bs, "luma_weight_l0", i);
			gf_bs_read_se_log_idx(bs, "luma_offset_l0", i);
		}
		if (ChromaArrayType != 0) {
			if (gf_bs_read_int_log_idx(bs, 1, "chroma_weight_l0_flag", i))
				for (j = 0; j < 2; j++) {
					gf_bs_read_se_log_idx2(bs, "chroma_weight_l0", i, j);
					gf_bs_read_se_log_idx2(bs, "chroma_offset_l0", i, j);
				}
		}
	}
	if (slice_type % 5 == 1) {
		for (i = 0; i <= num_ref_idx_l1_active_minus1; i++) {
			if (gf_bs_read_int_log_idx(bs, 1, "luma_weight_l1_flag", i)) {
				gf_bs_read_se_log_idx(bs, "luma_weight_l1", i);
				gf_bs_read_se_log_idx(bs, "luma_offset_l1", i);
			}
			if (ChromaArrayType != 0) {
				if (gf_bs_read_int_log_idx(bs, 1, "chroma_weight_l1_flag", i)) {
					for (j = 0; j < 2; j++) {
						gf_bs_read_se_log_idx2(bs, "chroma_weight_l1", i, j);
						gf_bs_read_se_log_idx2(bs, "chroma_offset_l1", i, j);
					}
				}
			}
		}
	}
}

static void dec_ref_pic_marking(GF_BitStream *bs, Bool IdrPicFlag) {
	if (IdrPicFlag) {
		gf_bs_read_int_log(bs, 1, "no_output_of_prior_pics_flag");
		gf_bs_read_int_log(bs, 1, "long_term_reference_flag");
	}
	else {
		if (gf_bs_read_int_log(bs, 1, "adaptive_ref_pic_marking_mode_flag")) {
			u32 idx=0, memory_management_control_operation;
			do {
				memory_management_control_operation = gf_bs_read_ue_log_idx(bs, "memory_management_control_operation", idx);
				if (memory_management_control_operation == 1 || memory_management_control_operation == 3)
					gf_bs_read_ue_log_idx(bs, "difference_of_pic_nums_minus1", idx);
				if (memory_management_control_operation == 2)
					gf_bs_read_ue_log_idx(bs, "long_term_pic_num", idx);
				if (memory_management_control_operation == 3 || memory_management_control_operation == 6)
					gf_bs_read_ue_log_idx(bs, "long_term_frame_idx", idx);
				if (memory_management_control_operation == 4)
					gf_bs_read_ue_log_idx(bs, "max_long_term_frame_idx_plus1", idx);
				idx++;
			} while (memory_management_control_operation != 0);
		}
	}
}

static s32 avc_parse_slice(GF_BitStream *bs, AVCState *avc, Bool svc_idr_flag, AVCSliceInfo *si)
{
	s32 pps_id, num_ref_idx_l0_active_minus1 = 0, num_ref_idx_l1_active_minus1 = 0;

	/*s->current_picture.reference= h->nal_ref_idc != 0;*/
	gf_bs_read_ue_log(bs, "first_mb_in_slice");
	si->slice_type = gf_bs_read_ue_log(bs, "slice_type");
	if (si->slice_type > 9) return -1;

	pps_id = gf_bs_read_ue_log(bs, "pps_id");
	if ((pps_id<0) || (pps_id >= 255)) return -1;
	si->pps = &avc->pps[pps_id];
	if (!si->pps->slice_group_count) return -2;
	if (si->pps->sps_id>=32) return -1;
	si->sps = &avc->sps[si->pps->sps_id];
	if (!si->sps->log2_max_frame_num) return -2;
	avc->sps_active_idx = si->pps->sps_id;
	avc->pps_active_idx = pps_id;

	si->frame_num = gf_bs_read_int_log(bs, si->sps->log2_max_frame_num, "frame_num");

	si->field_pic_flag = 0;
	si->bottom_field_flag = 0;
	if (!si->sps->frame_mbs_only_flag) {
		si->field_pic_flag = gf_bs_read_int_log(bs, 1, "field_pic_flag");
		if (si->field_pic_flag)
			si->bottom_field_flag = gf_bs_read_int_log(bs, 1, "bottom_field_flag");
	}

	if ((si->nal_unit_type == GF_AVC_NALU_IDR_SLICE) || svc_idr_flag)
		si->idr_pic_id = gf_bs_read_ue_log(bs, "idr_pic_id");

	if (si->sps->poc_type == 0) {
		si->poc_lsb = gf_bs_read_int_log(bs, si->sps->log2_max_poc_lsb, "poc_lsb");
		if (si->pps->pic_order_present && !si->field_pic_flag) {
			si->delta_poc_bottom = gf_bs_read_se_log(bs, "delta_poc_bottom");
		}
	}
	else if ((si->sps->poc_type == 1) && !si->sps->delta_pic_order_always_zero_flag) {
		si->delta_poc[0] = gf_bs_read_se_log(bs, "delta_poc0");
		if ((si->pps->pic_order_present == 1) && !si->field_pic_flag)
			si->delta_poc[1] = gf_bs_read_se_log(bs, "delta_poc1");
	}

	if (si->pps->redundant_pic_cnt_present) {
		si->redundant_pic_cnt = gf_bs_read_ue_log(bs, "redundant_pic_cnt");
	}

	if (si->slice_type % 5 == GF_AVC_TYPE_B) {
		gf_bs_read_int_log(bs, 1, "direct_spatial_mv_pred_flag");
	}

	num_ref_idx_l0_active_minus1 = si->pps->num_ref_idx_l0_default_active_minus1;
	num_ref_idx_l1_active_minus1 = si->pps->num_ref_idx_l1_default_active_minus1;

	if (si->slice_type % 5 == GF_AVC_TYPE_P || si->slice_type % 5 == GF_AVC_TYPE_SP || si->slice_type % 5 == GF_AVC_TYPE_B) {
		Bool num_ref_idx_active_override_flag = gf_bs_read_int_log(bs, 1, "num_ref_idx_active_override_flag");
		if (num_ref_idx_active_override_flag) {
			num_ref_idx_l0_active_minus1 = gf_bs_read_ue_log(bs, "num_ref_idx_l0_active_minus1");
			if (si->slice_type % 5 == GF_AVC_TYPE_B) {
				num_ref_idx_l1_active_minus1 = gf_bs_read_ue_log(bs, "num_ref_idx_l1_active_minus1");
			}
		}
	}

	if (si->nal_unit_type == 20 || si->nal_unit_type == 21) {
		//ref_pic_list_mvc_modification(); /* specified in Annex H */
		GF_LOG(GF_LOG_ERROR, GF_LOG_CODING, ("[avc-h264] unimplemented ref_pic_list_mvc_modification() in slide header\n"));
		gf_assert(0);
		return -1;
	}
	else {
		ref_pic_list_modification(bs, si->slice_type);
	}

	if ((si->pps->weighted_pred_flag && (si->slice_type % 5 == GF_AVC_TYPE_P || si->slice_type % 5 == GF_AVC_TYPE_SP))
		|| (si->pps->weighted_bipred_idc == 1 && si->slice_type % 5 == GF_AVC_TYPE_B)) {
		avc_pred_weight_table(bs, si->slice_type, si->sps->ChromaArrayType, num_ref_idx_l0_active_minus1, num_ref_idx_l1_active_minus1);
	}

	if (si->nal_ref_idc != 0) {
		dec_ref_pic_marking(bs, (si->nal_unit_type == GF_AVC_NALU_IDR_SLICE));
	}

	if (si->pps->entropy_coding_mode_flag && si->slice_type % 5 != GF_AVC_TYPE_I && si->slice_type % 5 != GF_AVC_TYPE_SI) {
		gf_bs_read_ue_log(bs, "cabac_init_idc");
	}

	/*slice_qp_delta = */gf_bs_read_se(bs);
	if (si->slice_type % 5 == GF_AVC_TYPE_SP || si->slice_type % 5 == GF_AVC_TYPE_SI) {
		if (si->slice_type % 5 == GF_AVC_TYPE_SP) {
			gf_bs_read_int_log(bs, 1, "sp_for_switch_flag");
		}
		gf_bs_read_se_log(bs, "slice_qs_delta");
	}

	if (si->pps->deblocking_filter_control_present_flag) {
		if (gf_bs_read_ue_log(bs, "disable_deblocking_filter_idc") != 1) {
			gf_bs_read_se_log(bs, "slice_alpha_c0_offset_div2");
			gf_bs_read_se_log(bs, "slice_beta_offset_div2");
		}
	}

	if (si->pps->slice_group_count > 1 && si->pps->mb_slice_group_map_type >= 3 && si->pps->mb_slice_group_map_type <= 5) {
		gf_bs_read_int_log(bs, (u32)ceil(log1p((si->pps->pic_size_in_map_units_minus1 + 1) / (si->pps->slice_group_change_rate_minus1 + 1) ) / log(2)), "slice_group_change_cycle");
	}
	return 0;
}


static s32 svc_parse_slice(GF_BitStream *bs, AVCState *avc, AVCSliceInfo *si)
{
	s32 pps_id;

	/*s->current_picture.reference= h->nal_ref_idc != 0;*/
	gf_bs_read_ue_log(bs, "first_mb_in_slice");
	si->slice_type = gf_bs_read_ue_log(bs, "slice_type");
	if (si->slice_type > 9) return -1;

	pps_id = gf_bs_read_ue_log(bs, "pps_id");
	if ((pps_id<0) || (pps_id >= 255))
		return -1;
	si->pps = &avc->pps[pps_id];
	si->pps->id = pps_id;
	if (!si->pps->slice_group_count)
		return -2;
	if (si->pps->sps_id + GF_SVC_SSPS_ID_SHIFT>=32)
		return -1;
	si->sps = &avc->sps[si->pps->sps_id + GF_SVC_SSPS_ID_SHIFT];
	if (!si->sps->log2_max_frame_num)
		return -2;

	si->frame_num = gf_bs_read_int_log(bs, si->sps->log2_max_frame_num, "frame_num");

	si->field_pic_flag = 0;
	if (si->sps->frame_mbs_only_flag) {
		/*s->picture_structure= PICT_FRAME;*/
	}
	else {
		si->field_pic_flag = gf_bs_read_int_log(bs, 1, "field_pic_flag");
		if (si->field_pic_flag) si->bottom_field_flag = gf_bs_read_int_log(bs, 1, "bottom_field_flag");
	}
	if (si->nal_unit_type == GF_AVC_NALU_IDR_SLICE || si->svc_nalhdr.idr_pic_flag)
		si->idr_pic_id = gf_bs_read_ue_log(bs, "idr_pic_id");

	if (si->sps->poc_type == 0) {
		si->poc_lsb = gf_bs_read_int_log(bs, si->sps->log2_max_poc_lsb, "poc_lsb");
		if (si->pps->pic_order_present && !si->field_pic_flag) {
			si->delta_poc_bottom = gf_bs_read_se_log(bs, "delta_poc_bottom");
		}
	}
	else if ((si->sps->poc_type == 1) && !si->sps->delta_pic_order_always_zero_flag) {
		si->delta_poc[0] = gf_bs_read_se_log(bs, "delta_poc0");
		if ((si->pps->pic_order_present == 1) && !si->field_pic_flag)
			si->delta_poc[1] = gf_bs_read_se_log(bs, "delta_poc1");
	}
	if (si->pps->redundant_pic_cnt_present) {
		si->redundant_pic_cnt = gf_bs_read_ue_log(bs, "redundant_pic_cnt");
	}
	return 0;
}


static s32 avc_parse_recovery_point_sei(GF_BitStream *bs, AVCState *avc)
{
	AVCSeiRecoveryPoint *rp = &avc->sei.recovery_point;

	rp->frame_cnt = gf_bs_read_ue_log(bs, "frame_cnt");
	rp->exact_match_flag = gf_bs_read_int_log(bs, 1, "exact_match_flag");
	rp->broken_link_flag = gf_bs_read_int_log(bs, 1, "broken_link_flag");
	rp->changing_slice_group_idc = gf_bs_read_int_log(bs, 2, "changing_slice_group_idc");
	rp->valid = 1;

	return 0;
}

/*for interpretation see ISO 14496-10 N.11084, table D-1*/
static s32 avc_parse_pic_timing_sei(GF_BitStream *bs, AVCState *avc)
{
	int sps_id = avc->sps_active_idx;
	const char NumClockTS[] = { 1, 1, 1, 2, 2, 3, 3, 2, 3 };
	AVCSeiPicTiming *pt = &avc->sei.pic_timing;

	if (sps_id < 0) {
		/*sps_active_idx equals -1 when no sps has been detected. In this case SEI should not be decoded.*/
		gf_assert(0);
		return 1;
	}
	if (avc->sps[sps_id].vui.nal_hrd_parameters_present_flag || avc->sps[sps_id].vui.vcl_hrd_parameters_present_flag) { /*CpbDpbDelaysPresentFlag, see 14496-10(2003) E.11*/
		gf_bs_read_int_log(bs, 1 + avc->sps[sps_id].vui.hrd.cpb_removal_delay_length_minus1, "cpb_removal_delay_minus1");
		gf_bs_read_int_log(bs, 1 + avc->sps[sps_id].vui.hrd.dpb_output_delay_length_minus1, "dpb_output_delay_minus1");
	}

	/*ISO 14496-10 (2003), D.8.2: we need to get pic_struct in order to know if we display top field first or bottom field first*/
	if (avc->sps[sps_id].vui.pic_struct_present_flag) {
		int i;
		pt->pic_struct = gf_bs_read_int_log(bs, 4, "pic_struct");
		if (pt->pic_struct > 8) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_CODING, ("[avc-h264] invalid pic_struct value %d\n", pt->pic_struct));
			return 1;
		}

		for (i = 0; i < NumClockTS[pt->pic_struct]; i++) {
			if (gf_bs_read_int_log_idx(bs, 1, "clock_timestamp_flag", i)) {
				Bool full_timestamp_flag;
				gf_bs_read_int_log_idx(bs, 2, "ct_type", i);
				gf_bs_read_int_log_idx(bs, 1, "nuit_field_based_flag", i);
				gf_bs_read_int_log_idx(bs, 5, "counting_type", i);
				full_timestamp_flag = gf_bs_read_int_log_idx(bs, 1, "full_timestamp_flag", i);
				gf_bs_read_int_log_idx(bs, 1, "discontinuity_flag", i);
				gf_bs_read_int_log_idx(bs, 1, "cnt_dropped_flag", i);
				gf_bs_read_int_log_idx(bs, 8, "n_frames", i);
				if (full_timestamp_flag) {
					gf_bs_read_int_log_idx(bs, 6, "seconds_value", i);
					gf_bs_read_int_log_idx(bs, 6, "minutes_value", i);
					gf_bs_read_int_log_idx(bs, 5, "hours_value", i);
				}
				else {
					if (gf_bs_read_int_log_idx(bs, 1, "seconds_flag", i)) {
						gf_bs_read_int_log_idx(bs, 6, "seconds_value", i);
						if (gf_bs_read_int_log_idx(bs, 1, "minutes_flag", i)) {
							gf_bs_read_int_log_idx(bs, 6, "minutes_value", i);
							if (gf_bs_read_int_log_idx(bs, 1, "hours_flag", i)) {
								gf_bs_read_int_log_idx(bs, 5, "hours_value", i);
							}
						}
					}
					if (avc->sps[sps_id].vui.hrd.time_offset_length > 0)
						gf_bs_read_int_log_idx(bs, avc->sps[sps_id].vui.hrd.time_offset_length, "time_offset", i);
				}
			}
		}
	}

	return 0;
}


static void avc_parse_itu_t_t35_sei(GF_BitStream* bs, AVCSeiItuTT35DolbyVision *dovi)
{
	u8 itu_t_t35_country_code = gf_bs_read_u8(bs);
	u16 terminal_provider_code = gf_bs_read_u16(bs);
	u32 user_id = gf_bs_read_u32(bs);
	u8 data_type_code = gf_bs_read_u8(bs);
	if (itu_t_t35_country_code == 0xB5 && terminal_provider_code == 0x31 && user_id == 0x47413934 && (data_type_code == 0x8 || data_type_code == 0x9)) {
		dovi->rpu_flag = GF_TRUE;
	}
}

static void avc_compute_poc(AVCSliceInfo *si)
{
	enum {
		AVC_PIC_FRAME,
		AVC_PIC_FIELD_TOP,
		AVC_PIC_FIELD_BOTTOM,
	} pic_type;
	s32 field_poc[2] = { 0,0 };
	s32 max_frame_num;

	if (!si->sps) return;

	max_frame_num = 1 << (si->sps->log2_max_frame_num);

	/* picture type */
	if (si->sps->frame_mbs_only_flag || !si->field_pic_flag) pic_type = AVC_PIC_FRAME;
	else if (si->bottom_field_flag) pic_type = AVC_PIC_FIELD_BOTTOM;
	else pic_type = AVC_PIC_FIELD_TOP;

	/* frame_num_offset */
	if (si->nal_unit_type == GF_AVC_NALU_IDR_SLICE) {
		si->poc_lsb_prev = 0;
		si->poc_msb_prev = 0;
		si->frame_num_offset = 0;
		si->frame_num_offset_prev = 0;
		si->frame_num_prev = 0;
	}
	else {
		if (si->frame_num < si->frame_num_prev)
			si->frame_num_offset = si->frame_num_offset_prev + max_frame_num;
		else
			si->frame_num_offset = si->frame_num_offset_prev;
	}

	/*ISO 14496-10 N.11084 8.2.1.1*/
	if (si->sps->poc_type == 0)
	{
		const u32 max_poc_lsb = 1 << (si->sps->log2_max_poc_lsb);

		/*ISO 14496-10 N.11084 eq (8-3)*/
		if ((si->poc_lsb < si->poc_lsb_prev) &&
			(si->poc_lsb_prev - si->poc_lsb >= max_poc_lsb / 2))
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
	else if (si->sps->poc_type == 1)
	{
		u32 i;
		s32 abs_frame_num, expected_delta_per_poc_cycle, expected_poc;

		if (si->sps->poc_cycle_length)
			abs_frame_num = si->frame_num_offset + si->frame_num;
		else
			abs_frame_num = 0;

		if (!si->nal_ref_idc && (abs_frame_num > 0)) abs_frame_num--;

		expected_delta_per_poc_cycle = 0;
		for (i = 0; i < si->sps->poc_cycle_length; i++)
			expected_delta_per_poc_cycle += si->sps->offset_for_ref_frame[i];

		if (abs_frame_num > 0) {
			const u32 poc_cycle_cnt = (abs_frame_num - 1) / si->sps->poc_cycle_length;
			const u32 frame_num_in_poc_cycle = (abs_frame_num - 1) % si->sps->poc_cycle_length;

			expected_poc = poc_cycle_cnt * expected_delta_per_poc_cycle;
			for (i = 0; i <= frame_num_in_poc_cycle; i++)
				expected_poc += si->sps->offset_for_ref_frame[i];
		}
		else {
			expected_poc = 0;
		}

		if (!si->nal_ref_idc) expected_poc += si->sps->offset_for_non_ref_pic;

		field_poc[0] = expected_poc + si->delta_poc[0];
		field_poc[1] = field_poc[0] + si->sps->offset_for_top_to_bottom_field;
		if (pic_type == AVC_PIC_FRAME) field_poc[1] += si->delta_poc[1];
	}
	/*ISO 14496-10 N.11084 8.2.1.3*/
	else if (si->sps->poc_type == 2)
	{
		int poc;
		if (si->nal_unit_type == GF_AVC_NALU_IDR_SLICE) {
			poc = 0;
		}
		else {
			const int abs_frame_num = si->frame_num_offset + si->frame_num;
			poc = 2 * abs_frame_num;
			if (!si->nal_ref_idc) poc -= 1;
		}
		field_poc[0] = poc;
		field_poc[1] = poc;
	}

	/*ISO 14496-10 N.11084 eq (8-1)*/
	if (pic_type == AVC_PIC_FRAME)
		si->poc = MIN(field_poc[0], field_poc[1]);
	else if (pic_type == AVC_PIC_FIELD_TOP)
		si->poc = field_poc[0];
	else
		si->poc = field_poc[1];
}

GF_EXPORT
s32 gf_avc_parse_nalu(GF_BitStream *bs, AVCState *avc)
{
	u8 idr_flag;
	s32 slice, ret;
	u32 nal_hdr;
	AVCSliceInfo n_state;

	gf_bs_enable_emulation_byte_removal(bs, GF_TRUE);
	if (!gf_bs_available(bs)) return -1;
	gf_bs_mark_overflow(bs, GF_TRUE);

	nal_hdr = gf_bs_read_u8(bs);

	slice = 0;
	memcpy(&n_state, &avc->s_info, sizeof(AVCSliceInfo));
	avc->last_nal_type_parsed = n_state.nal_unit_type = nal_hdr & 0x1F;
	n_state.nal_ref_idc = (nal_hdr >> 5) & 0x3;

	idr_flag = 0;

	switch (n_state.nal_unit_type) {
	case GF_AVC_NALU_ACCESS_UNIT:
	case GF_AVC_NALU_END_OF_SEQ:
	case GF_AVC_NALU_END_OF_STREAM:
		ret = 1;
		break;

	case GF_AVC_NALU_SVC_SLICE:
		SVC_ReadNal_header_extension(bs, &n_state.svc_nalhdr);
		if (gf_bs_is_overflow(bs)) return -1;

		// slice buffer - read the info and compare.
		/*ret = */svc_parse_slice(bs, avc, &n_state);
		if (avc->s_info.nal_ref_idc) {
			n_state.poc_lsb_prev = avc->s_info.poc_lsb;
			n_state.poc_msb_prev = avc->s_info.poc_msb;
		}
		avc_compute_poc(&n_state);

		if (avc->s_info.poc != n_state.poc) {
			memcpy(&avc->s_info, &n_state, sizeof(AVCSliceInfo));
			return 1;
		}
		memcpy(&avc->s_info, &n_state, sizeof(AVCSliceInfo));
		return 0;

	case GF_AVC_NALU_SVC_PREFIX_NALU:
		SVC_ReadNal_header_extension(bs, &avc->s_info.svc_nalhdr);
		if (gf_bs_is_overflow(bs)) return -1;
		return 0;

	case GF_AVC_NALU_IDR_SLICE:
	case GF_AVC_NALU_NON_IDR_SLICE:
	case GF_AVC_NALU_DP_A_SLICE:
	case GF_AVC_NALU_DP_B_SLICE:
	case GF_AVC_NALU_DP_C_SLICE:
		slice = 1;
		/* slice buffer - read the info and compare.*/
		ret = avc_parse_slice(bs, avc, idr_flag, &n_state);
		if (gf_bs_is_overflow(bs)) ret = -1;
		if (ret < 0) return ret;
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
		if (!avc->s_info.sps)
			return -1;

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
			}
			else if (avc->s_info.sps->poc_type == 1) {
				if (avc->s_info.delta_poc[0] != n_state.delta_poc[0]) {
					ret = 1;
					break;
				}
				if (avc->s_info.delta_poc[1] != n_state.delta_poc[1]) {
					ret = 1;
					break;
				}
			}
		}

		if (n_state.nal_unit_type == GF_AVC_NALU_IDR_SLICE) {
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
		avc->last_ps_idx = gf_avc_read_sps_bs_internal(bs, avc, 0, NULL, nal_hdr);
		if (gf_bs_is_overflow(bs)) return -1;
		if (avc->last_ps_idx < 0) return -1;
		avc->last_sps_idx = avc->last_ps_idx;
		return 0;

	case GF_AVC_NALU_PIC_PARAM:
		avc->last_ps_idx = gf_avc_read_pps_bs_internal(bs, avc, nal_hdr);
		if (gf_bs_is_overflow(bs)) return -1;
		if (avc->last_ps_idx < 0) return -1;
		return 0;
	case GF_AVC_NALU_SVC_SUBSEQ_PARAM:
		avc->last_ps_idx = gf_avc_read_sps_bs_internal(bs, avc, 1, NULL, nal_hdr);
		if (gf_bs_is_overflow(bs)) return -1;
		if (avc->last_ps_idx < 0) return -1;
		return 0;
	case GF_AVC_NALU_SEQ_PARAM_EXT:
		avc->last_ps_idx = (s32) gf_bs_read_ue(bs);
		if (gf_bs_is_overflow(bs)) return -1;
		if (avc->last_ps_idx < 0) return -1;
		return 0;

	case GF_AVC_NALU_SEI:
	case GF_AVC_NALU_FILLER_DATA:
		return 0;

	default:
		if (avc->s_info.nal_unit_type <= GF_AVC_NALU_IDR_SLICE) ret = 1;
		//To detect change of AU when multiple sps and pps in stream
		else if ((nal_hdr & 0x1F) == GF_AVC_NALU_SEI && avc->s_info.nal_unit_type == GF_AVC_NALU_SVC_SLICE)
			ret = 1;
		else if ((nal_hdr & 0x1F) == GF_AVC_NALU_SEQ_PARAM && avc->s_info.nal_unit_type == GF_AVC_NALU_SVC_SLICE)
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
	if (slice)
		avc_compute_poc(&n_state);
	memcpy(&avc->s_info, &n_state, sizeof(AVCSliceInfo));
	return ret;
}


u32 gf_avc_reformat_sei(u8 *buffer, u32 nal_size, Bool isobmf_rewrite, AVCState *avc)
{
	u32 ptype, psize, hdr, var;
	u32 start;
	GF_BitStream *bs;
	GF_BitStream *bs_dest = NULL;
	u8 nhdr;
	Bool sei_removed = GF_FALSE;
	char store;

	hdr = buffer[0];
	if ((hdr & 0x1F) != GF_AVC_NALU_SEI) return 0;

	if (isobmf_rewrite) bs_dest = gf_bs_new(NULL, 0, GF_BITSTREAM_WRITE);

	bs = gf_bs_new(buffer, nal_size, GF_BITSTREAM_READ);
	gf_bs_enable_emulation_byte_removal(bs, GF_TRUE);

	nhdr = gf_bs_read_int(bs, 8);
	if (bs_dest) gf_bs_write_int(bs_dest, nhdr, 8);

	/*parse SEI*/
	while (gf_bs_available(bs)) {
		Bool do_copy;
		ptype = 0;
		while (1) {
			u8 v = gf_bs_read_int(bs, 8);
			ptype += v;
			if (v != 0xFF) break;
		}

		psize = 0;
		while (1) {
			u8 v = gf_bs_read_int(bs, 8);
			psize += v;
			if (v != 0xFF) break;
		}

		start = (u32)gf_bs_get_position(bs);

		do_copy = 1;

		if (start + psize >= nal_size) {
			GF_LOG(GF_LOG_WARNING, GF_LOG_CODING, ("[avc-h264] SEI user message type %d size error (%d but %d remain), keeping full SEI untouched\n", ptype, psize, nal_size - start));
			if (bs_dest) gf_bs_del(bs_dest);
			gf_bs_del(bs);
			return nal_size;
		}
		switch (ptype) {
			/*remove SEI messages forbidden in MP4*/
		case 3: /*filler data*/
		case 10: /*sub_seq info*/
		case 11: /*sub_seq_layer char*/
		case 12: /*sub_seq char*/
			do_copy = 0;
			sei_removed = GF_TRUE;
			break;
		case 5: /*user unregistered */
			store = buffer[start + psize];
			buffer[start + psize] = 0;
			GF_LOG(GF_LOG_DEBUG, GF_LOG_CODING, ("[avc-h264] SEI user message %s\n", buffer + start + 16));
			buffer[start + psize] = store;
			break;

		case 6: /*recovery point*/
			avc_parse_recovery_point_sei(bs, avc);
			break;

		case 1: /*pic_timing*/
			avc_parse_pic_timing_sei(bs, avc);
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
		default: /*add all unknown SEIs*/
			break;
		}

		if (do_copy && bs_dest) {
			var = ptype;
			while (var >= 255) {
				gf_bs_write_int(bs_dest, 0xFF, 8);
				var -= 255;
			}
			gf_bs_write_int(bs_dest, var, 8);

			var = psize;
			while (var >= 255) {
				gf_bs_write_int(bs_dest, 0xFF, 8);
				var -= 255;
			}
			gf_bs_write_int(bs_dest, var, 8);
			gf_bs_seek(bs, start);

			//bs_read_data does not skip EPB, read byte per byte
			var = psize;
			while (var) {
				gf_bs_write_u8(bs_dest, gf_bs_read_u8(bs));
				var--;
			}
		}
		else {
			gf_bs_seek(bs, start);

			//bs_skip_bytes does not skip EPB, skip byte per byte
			while (psize) {
				gf_bs_read_u8(bs);
				psize--;
			}
		}

		if (gf_bs_available(bs) <= 2) {
			var = gf_bs_read_int(bs, 8);
			if (var != 0x80) {
				GF_LOG(GF_LOG_WARNING, GF_LOG_CODING, ("[avc-h264] SEI user message has less than 2 bytes remaining but no end of sei found, keeping full SEI untouched\n"));
				if (bs_dest) gf_bs_del(bs_dest);
				gf_bs_del(bs);
				return nal_size;
			}
			if (bs_dest) gf_bs_write_int(bs_dest, 0x80, 8);
			break;
		}
	}
	gf_bs_del(bs);
	//we cannot compare final size and original size since original may have EPB and final does not yet have them
	if (bs_dest && sei_removed) {
		u8 *dst_no_epb = NULL;
		u32 dst_no_epb_size = 0;
		gf_bs_get_content(bs_dest, &dst_no_epb, &dst_no_epb_size);
		if (dst_no_epb) {
			u32 nb_bytes_add = gf_media_nalu_emulation_bytes_add_count(dst_no_epb, dst_no_epb_size);
			//if result fits into source buffer, reformat
			//otherwise ignore and return source (happens in some fuzzing cases, cf issue 1903)
			if (dst_no_epb_size + nb_bytes_add <= nal_size)
				nal_size = gf_media_nalu_add_emulation_bytes(dst_no_epb, buffer, dst_no_epb_size);

			gf_free(dst_no_epb);
		}
	}
	if (bs_dest) gf_bs_del(bs_dest);
	return nal_size;
}

static u8 avc_hevc_get_sar_idx(u32 w, u32 h)
{
	u32 i, count = GF_ARRAY_LENGTH(avc_hevc_sar);
	for (i = 0; i < count; i++) {
		if ((avc_hevc_sar[i].w == w) && (avc_hevc_sar[i].h == h))
			return i;
	}
	return 0xFF;
}

static void sub_layer_hrd_parameters(GF_BitStream *bs, int subLayerId, u32 cpb_cnt, Bool sub_pic_hrd_params_present_flag, u32 idx1, u32 idx2)
{
	u32 i;
	if (!gf_bs_available(bs)) return;

	for (i = 0; i <= cpb_cnt; i++) {
		gf_bs_read_ue_log_idx3(bs, "bit_rate_value_minus1", idx1, idx2, i);
		gf_bs_read_ue_log_idx3(bs, "cpb_size_value_minus1", idx1, idx2, i);
		if (sub_pic_hrd_params_present_flag) {
			gf_bs_read_ue_log_idx3(bs, "cpb_size_du_value_minus1", idx1, idx2, i);
			gf_bs_read_ue_log_idx3(bs, "bit_rate_du_value_minus1", idx1, idx2, i);
		}
		gf_bs_read_int_log_idx3(bs, 1, "cbr_flag", idx1, idx2, i);
	}
}

static void hevc_parse_hrd_parameters(GF_BitStream *bs, Bool commonInfPresentFlag, int maxNumSubLayersMinus1, u32 idx)
{
	int i;
	Bool nal_hrd_parameters_present_flag = GF_FALSE;
	Bool vcl_hrd_parameters_present_flag = GF_FALSE;
	Bool sub_pic_hrd_params_present_flag = GF_FALSE;

	if (commonInfPresentFlag) {
		nal_hrd_parameters_present_flag = gf_bs_read_int_log_idx(bs, 1, "nal_hrd_parameters_present_flag", idx);
		vcl_hrd_parameters_present_flag = gf_bs_read_int_log_idx(bs, 1, "vcl_hrd_parameters_present_flag", idx);
		if (nal_hrd_parameters_present_flag || vcl_hrd_parameters_present_flag) {
			sub_pic_hrd_params_present_flag = gf_bs_read_int_log_idx(bs, 1, "sub_pic_hrd_params_present_flag", idx);
			if (sub_pic_hrd_params_present_flag) {
				gf_bs_read_int_log_idx(bs, 8, "tick_divisor_minus2", idx);
				gf_bs_read_int_log_idx(bs, 5, "du_cpb_removal_delay_increment_length_minus1", idx);
				gf_bs_read_int_log_idx(bs, 1, "sub_pic_cpb_params_in_pic_timing_sei_flag", idx);
				gf_bs_read_int_log_idx(bs, 5, "dpb_output_delay_du_length_minus1", idx);
			}
			gf_bs_read_int_log_idx(bs, 4, "bit_rate_scale", idx);
			gf_bs_read_int_log_idx(bs, 4, "cpb_size_scale", idx);
			if (sub_pic_hrd_params_present_flag) {
				gf_bs_read_int_log_idx(bs, 4, "cpb_size_du_scale", idx);
			}
			gf_bs_read_int_log_idx(bs, 5, "initial_cpb_removal_delay_length_minus1", idx);
			gf_bs_read_int_log_idx(bs, 5, "au_cpb_removal_delay_length_minus1", idx);
			gf_bs_read_int_log_idx(bs, 5, "dpb_output_delay_length_minus1", idx);
		}
	}
	for (i = 0; i <= maxNumSubLayersMinus1; i++) {
		Bool fixed_pic_rate_general_flag_i = gf_bs_read_int_log_idx(bs, 1, "fixed_pic_rate_general_flag", idx);
		Bool fixed_pic_rate_within_cvs_flag_i = GF_TRUE;
		Bool low_delay_hrd_flag_i = GF_FALSE;
		u32 cpb_cnt_minus1_i = 0;
		if (!fixed_pic_rate_general_flag_i) {
			fixed_pic_rate_within_cvs_flag_i = gf_bs_read_int_log_idx(bs, 1, "fixed_pic_rate_within_cvs_flag", idx);
		}
		if (fixed_pic_rate_within_cvs_flag_i)
			gf_bs_read_ue_log_idx(bs, "elemental_duration_in_tc_minus1", idx);
		else
			low_delay_hrd_flag_i = gf_bs_read_int_log_idx(bs, 1, "low_delay_hrd_flag", idx);
		if (!low_delay_hrd_flag_i) {
			cpb_cnt_minus1_i = gf_bs_read_ue_log_idx(bs, "cpb_cnt_minus1", idx);
		}
		if (nal_hrd_parameters_present_flag) {
			sub_layer_hrd_parameters(bs, i, cpb_cnt_minus1_i, sub_pic_hrd_params_present_flag, idx, i);
		}
		if (vcl_hrd_parameters_present_flag) {
			sub_layer_hrd_parameters(bs, i, cpb_cnt_minus1_i, sub_pic_hrd_params_present_flag, idx, i);
		}
	}
}

static void avc_hevc_vvc_rewrite_vui(GF_VUIInfo *vui_info, GF_BitStream *orig, GF_BitStream *mod, GF_CodecID codec)
{
	/* VUI present flag*/
	Bool vui_present_flag = gf_bs_read_int(orig, 1);

	/*setup default values*/
	Bool aspect_ratio_info_present_flag = 0;
	s32 aspect_ratio_idc = -1;
	u32 ar_n=0, ar_d=0;
	Bool overscan_info_present_flag = 0;
	u32 overscan_info = 0;
	u32 video_signal_type_present_flag = 0;
	u32 video_format = 5;
	u32 video_full_range_flag = 0;
	u32 colour_description_present_flag = 0;
	u32 colour_primaries = 2;
	u32 transfer_characteristics = 2;
	u32 matrix_coefficients = 2;
	//HEVC
	Bool neutral_chroma_indication_flag = GF_FALSE;
	Bool field_seq_flag = GF_FALSE;
	Bool frame_field_info_present_flag = GF_FALSE;
	Bool default_display_window_flag = GF_FALSE;
	u32 def_disp_win_left_offset = 0;
	u32 def_disp_win_right_offset = 0;
	u32 def_disp_win_top_offset = 0;
	u32 def_disp_win_bottom_offset = 0;
	//AVC & HEVC
	Bool timing_info_present_flag = GF_FALSE;
	u32 num_units_in_tick = 0;
	u32 time_scale = 0;
	//AVC
	Bool fixed_frame_rate_flag=GF_FALSE;
	//HEVC
	Bool poc_proportional_to_timing_flag = GF_FALSE;
	u32 vui_num_ticks_poc_diff_one_minus1 = 0;
	//VVC
	Bool progressive_source_flag = 1;
	Bool interlaced_source_flag = 0;
	Bool non_packed_constraint_flag = 0;
	Bool non_projected_constraint_flag = 0;
	Bool aspect_ratio_constant_flag = 1;
	u32 vui_start_pos = 0;
	u32 orig_vvc_payload_size = 0;
	Bool vui_chroma_loc_info_present_flag = 0;
	u32 chroma_loc1=0, chroma_loc2 = 0;
	u32 final_vvc_payload_size = 8; //4 first bits + 4 flags (ar, overscan and colour desc, chroma pos)
	u32 mod_vui_start_pos = 0;

	//if VUI is present, read all SAR and overscan values
	if (vui_present_flag) { /* VUI found in input bitstream */
		if (codec == GF_CODECID_VVC) {
			//align
			orig_vvc_payload_size = 8 * ( 1 + gf_bs_read_ue(orig) );
			gf_bs_align(orig);
			vui_start_pos = gf_bs_get_bit_offset(orig);

			progressive_source_flag = gf_bs_read_int(orig, 1);
			interlaced_source_flag = gf_bs_read_int(orig, 1);
			non_packed_constraint_flag = gf_bs_read_int(orig, 1);
			non_projected_constraint_flag = gf_bs_read_int(orig, 1);
		}
		aspect_ratio_info_present_flag = gf_bs_read_int(orig, 1);

		if (aspect_ratio_info_present_flag) {
			if (codec == GF_CODECID_VVC) {
				aspect_ratio_constant_flag = gf_bs_read_int(orig, 1);
			}
			aspect_ratio_idc = gf_bs_read_int(orig, 8); /*aspect_ratio_idc*/
			if (aspect_ratio_idc == 255) {
				ar_n = gf_bs_read_int(orig, 16); /*sar_width*/
				ar_d = gf_bs_read_int(orig, 16); /*sar_height*/
			}
		}

		/*overscan_info_present_flag */
		overscan_info_present_flag = gf_bs_read_int(orig, 1);
		if(overscan_info_present_flag) {
			overscan_info = gf_bs_read_int(orig, 1);
		}

		/* read all video signal related flags first */
		video_signal_type_present_flag = gf_bs_read_int(orig, 1);

		if (video_signal_type_present_flag) {
			if (codec != GF_CODECID_VVC) {
				video_format = gf_bs_read_int(orig, 3);
				video_full_range_flag = gf_bs_read_int(orig, 1);
				colour_description_present_flag = gf_bs_read_int(orig, 1);
			} else {
				colour_description_present_flag = 1;
			}

			if (colour_description_present_flag) {
				colour_primaries = gf_bs_read_int(orig, 8);
				transfer_characteristics = gf_bs_read_int(orig, 8);
				matrix_coefficients = gf_bs_read_int(orig, 8);
				if (codec == GF_CODECID_VVC) {
					video_full_range_flag = gf_bs_read_int(orig, 1);
				}
			}
		}

		if (codec == GF_CODECID_VVC) {
			vui_chroma_loc_info_present_flag = gf_bs_read_int(orig, 1);
			if (vui_chroma_loc_info_present_flag) {
				if (progressive_source_flag && !interlaced_source_flag) {
					chroma_loc1 = gf_bs_read_ue(orig);
				} else {
					chroma_loc1 = gf_bs_read_ue(orig);
					chroma_loc2 = gf_bs_read_ue(orig);
				}
			}
			//LAST bit read for VVC
		} else { //AVC, HEVC
			vui_chroma_loc_info_present_flag = gf_bs_read_int(orig, 1);
			if (vui_chroma_loc_info_present_flag) {
				chroma_loc1 = gf_bs_read_ue(orig); //chroma_sample_loc_type_top_field
				chroma_loc2 = gf_bs_read_ue(orig); //chroma_sample_loc_type_bottom_field
			}

			if (codec == GF_CODECID_HEVC) {
				neutral_chroma_indication_flag = gf_bs_read_int(orig, 1);
				field_seq_flag = gf_bs_read_int(orig, 1);
				frame_field_info_present_flag = gf_bs_read_int(orig, 1);
				default_display_window_flag = gf_bs_read_int(orig, 1);
				if (default_display_window_flag) {
					def_disp_win_left_offset = gf_bs_read_ue(orig);
					def_disp_win_right_offset = gf_bs_read_ue(orig);
					def_disp_win_top_offset = gf_bs_read_ue(orig);
					def_disp_win_bottom_offset = gf_bs_read_ue(orig);
				}
			}

			timing_info_present_flag = gf_bs_read_int(orig, 1);
			if (timing_info_present_flag) {
				num_units_in_tick = gf_bs_read_int(orig, 32);
				time_scale = gf_bs_read_int(orig, 32);
				if (codec == GF_CODECID_AVC) {
					fixed_frame_rate_flag = gf_bs_read_int(orig, 1);

					//LAST bit read for AVC
				} else if (codec == GF_CODECID_HEVC) {
					poc_proportional_to_timing_flag = gf_bs_read_int(orig, 1);
					if (poc_proportional_to_timing_flag)
						/*vui_num_ticks_poc_diff_one_minus1 = */gf_bs_read_ue(orig);
					if (/*vui_hrd_parameters_present_flag = */gf_bs_read_int(orig, 1))
						hevc_parse_hrd_parameters(orig, GF_TRUE, 0/*assumes max_sub_layers_minus1=0*/, 0);

					//LAST bit read for HEVC
				}
			}
		}
	}

	//recompute values
	//no change
	if ((vui_info->ar_num<0) || (vui_info->ar_den<0)) {
	}
	//remove par
	else if ((vui_info->ar_num==0) || (vui_info->ar_den==0)) {
		aspect_ratio_info_present_flag = 0;
	}
	//set par
	else {
		aspect_ratio_info_present_flag = 1;
	}

	//add par size
	if (aspect_ratio_info_present_flag) {
		ar_n = vui_info->ar_num;
		ar_d = vui_info->ar_den;
		aspect_ratio_idc = avc_hevc_get_sar_idx((u32) ar_n, (u32) ar_d);
		if (codec == GF_CODECID_VVC) {
			final_vvc_payload_size += 9;
			if (aspect_ratio_idc==0xFF)
				final_vvc_payload_size += 32;
		}
	}

	if (vui_info->remove_video_info) {
		video_signal_type_present_flag = 0;
	}
	/* correct the values of each flags */
	else if ((vui_info->fullrange==0) && (vui_info->video_format==5) && (vui_info->color_prim==2) && (vui_info->color_tfc==2) && (vui_info->color_matrix==2)) {
		video_signal_type_present_flag = 0; /* all default, nothing to write*/
	} else {
		video_signal_type_present_flag = 1;
		video_format = (vui_info->video_format < 0) ? video_format : vui_info->video_format;
		video_full_range_flag = (vui_info->fullrange < 0) ? video_full_range_flag : vui_info->fullrange;
		if ((vui_info->color_prim==2) && (vui_info->color_tfc==2) && (vui_info->color_matrix==2)) {
			colour_description_present_flag = 0;
		} else {
			colour_description_present_flag = 1;
			colour_primaries = (vui_info->color_prim < 0) ? colour_primaries : vui_info->color_prim;
			transfer_characteristics = (vui_info->color_tfc < 0) ? transfer_characteristics : vui_info->color_tfc;
			matrix_coefficients = (vui_info->color_matrix < 0) ? matrix_coefficients : vui_info->color_matrix;
		}
		if ((colour_primaries==2) && (transfer_characteristics==2) && (matrix_coefficients==2)) {
			colour_description_present_flag = 0;
			if ((video_format==5) && (video_full_range_flag==0))
				video_signal_type_present_flag = 0;
		}

		if (codec == GF_CODECID_VVC) {
			if (!video_full_range_flag && !colour_description_present_flag) {
				video_signal_type_present_flag = 0;
			} else {
				final_vvc_payload_size += 25;
			}
		}
	}

	if ((codec == GF_CODECID_VVC) && vui_chroma_loc_info_present_flag) {
		if (progressive_source_flag && !interlaced_source_flag) {
			final_vvc_payload_size += gf_get_ue_nb_bits(chroma_loc1);
		} else {
			final_vvc_payload_size += gf_get_ue_nb_bits(chroma_loc1);
			final_vvc_payload_size += gf_get_ue_nb_bits(chroma_loc2);
		}
	}
	//remove VUI timing
	if (vui_info->remove_vui_timing_info)
		timing_info_present_flag = 0;

	//always rewrite VUI
	gf_bs_write_int(mod, 1, 1);
	if (codec == GF_CODECID_VVC) {
		while (final_vvc_payload_size%8)
			final_vvc_payload_size++;
		final_vvc_payload_size/=8;

		gf_bs_write_ue(mod, final_vvc_payload_size-1);
		gf_bs_align(mod);
		mod_vui_start_pos = gf_bs_get_bit_offset(mod);
		final_vvc_payload_size *= 8;

		gf_bs_write_int(mod, progressive_source_flag, 1);
		gf_bs_write_int(mod, interlaced_source_flag, 1);
		gf_bs_write_int(mod, non_packed_constraint_flag, 1);
		gf_bs_write_int(mod, non_projected_constraint_flag, 1);
	}

	gf_bs_write_int(mod, aspect_ratio_info_present_flag, 1);
	if (aspect_ratio_info_present_flag) {
		if (codec == GF_CODECID_VVC)
			gf_bs_write_int(mod, aspect_ratio_constant_flag, 1);

		gf_bs_write_int(mod, aspect_ratio_idc, 8);
		if (aspect_ratio_idc == 255) {
			gf_bs_write_int(mod, ar_n, 16);
			gf_bs_write_int(mod, ar_d, 16);
		}
		if (vui_info->update) {
			vui_info->ar_num = ar_n;
			vui_info->ar_den = ar_d;
		}
	}
	gf_bs_write_int(mod, overscan_info_present_flag, 1);
	if (overscan_info_present_flag) {
		gf_bs_write_int(mod, overscan_info, 1);
	}

	gf_bs_write_int(mod, video_signal_type_present_flag, 1);
	if (video_signal_type_present_flag) {
		if (codec != GF_CODECID_VVC) {
			gf_bs_write_int(mod, video_format, 3);
			gf_bs_write_int(mod, video_full_range_flag, 1);
			gf_bs_write_int(mod, colour_description_present_flag, 1);
		} else {
			colour_description_present_flag = 1;
		}

		if (colour_description_present_flag) {
			gf_bs_write_int(mod, colour_primaries, 8);
			gf_bs_write_int(mod, transfer_characteristics, 8);
			gf_bs_write_int(mod, matrix_coefficients, 8);
			if (codec == GF_CODECID_VVC)
				gf_bs_write_int(mod, video_full_range_flag, 1);
		}

		if (vui_info->update) {
			vui_info->video_format = video_format;
			vui_info->fullrange = video_full_range_flag;
			if (colour_description_present_flag) {
				vui_info->color_prim = colour_primaries;
				vui_info->color_tfc = transfer_characteristics;
				vui_info->color_matrix = matrix_coefficients;
			}
		}
	}

	if (codec == GF_CODECID_VVC) {
		//write vui_chroma_loc_info_present_flag
		gf_bs_write_int(mod, vui_chroma_loc_info_present_flag, 1);
		if (vui_chroma_loc_info_present_flag) {
			if (progressive_source_flag && !interlaced_source_flag) {
				gf_bs_write_ue(mod, chroma_loc1);
			} else {
				gf_bs_write_ue(mod, chroma_loc1);
				gf_bs_write_ue(mod, chroma_loc2);
			}
		}
		//we don't copy over vui extension (they're not supposed to be present), but we must parse them
		if (vui_present_flag) {
			//we are byte aligned for vui_paramaters
			Bool more_data_in_payload = GF_TRUE;
			vui_start_pos = gf_bs_get_bit_offset(orig) - vui_start_pos;
			if (gf_bs_is_align(orig) && (vui_start_pos == orig_vvc_payload_size))
				more_data_in_payload = GF_FALSE;

			u32 nb_bits = orig_vvc_payload_size - vui_start_pos;
			if (more_data_in_payload) {
				if (nb_bits<8) {
					u32 val = gf_bs_peek_bits(orig, nb_bits, 0);
					u32 bit_pos = 1<<(nb_bits-1);
					if (val == bit_pos)
						more_data_in_payload = 0;
				}
			}
			if (more_data_in_payload) {
				while (nb_bits) {
					nb_bits--;
					gf_bs_read_int(orig, 1); //vui_reserved_payload_extension_data
					//load next 32 bits, if only 1 at MSB and 0 afterwards, done
					if (nb_bits<32) {
						u32 val = gf_bs_peek_bits(orig, nb_bits, 0);
						u32 bit_pos = 1<<(nb_bits-1);
						if (val == bit_pos)
							break;
					}
				}

				//then byte alignment of vui_payload
				gf_bs_read_int(orig, 1); //vui_payload_bit_equal_to_one
				gf_bs_align(orig);
			}
		}
		mod_vui_start_pos = gf_bs_get_bit_offset(mod) - mod_vui_start_pos;
		//check if we need explicit align
		if (!gf_bs_is_align(mod) || (mod_vui_start_pos != final_vvc_payload_size)) {
			gf_bs_write_int(mod, 1, 1); //vui_payload_bit_equal_to_one
			gf_bs_align(mod);
		}
		//VVC done
		return;
	}

	//AVC and HEVC
	gf_bs_write_int(mod, vui_chroma_loc_info_present_flag, 1);
	if (vui_chroma_loc_info_present_flag) {
		gf_bs_write_ue(mod, chroma_loc1); //chroma_sample_loc_type_top_field
		gf_bs_write_ue(mod, chroma_loc2); //chroma_sample_loc_type_bottom_field
	}

	if (codec == GF_CODECID_HEVC) {
		gf_bs_write_int(mod, neutral_chroma_indication_flag, 1);
		gf_bs_write_int(mod, field_seq_flag, 1);
		gf_bs_write_int(mod, frame_field_info_present_flag, 1);
		gf_bs_write_int(mod, default_display_window_flag, 1);
		if (default_display_window_flag) {
			 gf_bs_write_ue(mod, def_disp_win_left_offset);
			 gf_bs_write_ue(mod, def_disp_win_right_offset);
			 gf_bs_write_ue(mod, def_disp_win_top_offset);
			 gf_bs_write_ue(mod, def_disp_win_bottom_offset);
		}
	}

	gf_bs_write_int(mod, timing_info_present_flag, 1);
	if (timing_info_present_flag) {
		gf_bs_write_int(mod, num_units_in_tick, 32);
		gf_bs_write_int(mod, time_scale, 32);
		if (codec == GF_CODECID_AVC) {
			gf_bs_write_int(mod, fixed_frame_rate_flag, 1);
		} else if (codec == GF_CODECID_HEVC) {
			gf_bs_write_int(mod, poc_proportional_to_timing_flag, 1);
			if (poc_proportional_to_timing_flag)
				gf_bs_write_ue(mod, vui_num_ticks_poc_diff_one_minus1);
		}
	}

	/*no VUI in input bitstream but we just inserted one, set all remaining vui flags to 0*/
	if (!vui_present_flag) {
		if (codec == GF_CODECID_AVC) {
			gf_bs_write_int(mod, 0, 1);		/*nal_hrd_parameters_present*/
			gf_bs_write_int(mod, 0, 1);		/*vcl_hrd_parameters_present*/
			gf_bs_write_int(mod, 0, 1);		/*pic_struct_present*/
			gf_bs_write_int(mod, 0, 1);		/*bitstream_restriction*/
		} else if (codec == GF_CODECID_HEVC) {
			if (timing_info_present_flag) {
				gf_bs_write_int(mod, 0, 1);		/*vui_hrd_parameters_present_flag*/
			}
			gf_bs_write_int(mod, 0, 1);		/*bitstream_restriction*/
		}
	}
	/*otherwise we copy over the bits from the input bitstream*/
}

GF_Err gf_avc_change_vui(GF_AVCConfig *avcc, GF_VUIInfo *vui_info)
{
	AVCState avc;
	u32 i, bit_offset, flag;
	s32 idx;
	GF_AVCConfigSlot *slc;

	if (!avcc)
		return GF_NON_COMPLIANT_BITSTREAM;

	memset(&avc, 0, sizeof(AVCState));
	avc.sps_active_idx = -1;

	i=0;
	while ((slc = (GF_AVCConfigSlot *)gf_list_enum(avcc->sequenceParameterSets, &i))) {
		GF_BitStream *orig, *mod;
		u8 *no_emulation_buf = NULL;
		u32 no_emulation_buf_size = 0, emulation_bytes = 0;
		idx = gf_avc_read_sps(slc->data, slc->size, &avc, 0, &bit_offset);
		if (idx<0) {
			continue;
		}

		/*SPS still contains emulation bytes*/
		no_emulation_buf = gf_malloc((slc->size - 1) * sizeof(char));
		no_emulation_buf_size = gf_media_nalu_remove_emulation_bytes(slc->data + 1, no_emulation_buf, slc->size - 1);

		orig = gf_bs_new(no_emulation_buf, no_emulation_buf_size, GF_BITSTREAM_READ);
		gf_bs_read_data(orig, no_emulation_buf, no_emulation_buf_size);
		gf_bs_seek(orig, 0);
		mod = gf_bs_new(NULL, 0, GF_BITSTREAM_WRITE);

		/*copy over till vui flag*/
		while (bit_offset > 8/*bit_offset doesn't take care of the first byte (NALU type)*/) {
			flag = gf_bs_read_int(orig, 1);
			gf_bs_write_int(mod, flag, 1);
			bit_offset--;
		}

		avc_hevc_vvc_rewrite_vui(vui_info, orig, mod, GF_CODECID_AVC);

		/*finally copy over remaining*/
		while (gf_bs_bits_available(orig)) {
			flag = gf_bs_read_int(orig, 1);
			gf_bs_write_int(mod, flag, 1);
		}
		gf_bs_del(orig);
		orig = NULL;
		gf_free(no_emulation_buf);

		/*set anti-emulation*/
		gf_bs_get_content(mod, &no_emulation_buf, &flag);
		emulation_bytes = gf_media_nalu_emulation_bytes_add_count(no_emulation_buf, flag);
		if (flag+emulation_bytes+1>slc->size)
			slc->data = (char*)gf_realloc(slc->data, flag+emulation_bytes+1);
		slc->size = gf_media_nalu_add_emulation_bytes(no_emulation_buf, slc->data + 1, flag) + 1;

		gf_bs_del(mod);
		gf_free(no_emulation_buf);
	}
	return GF_OK;
}

GF_EXPORT
GF_Err gf_avc_change_par(GF_AVCConfig *avcc, s32 ar_n, s32 ar_d)
{
	GF_VUIInfo vuii;
	memset(&vuii, 0, sizeof(GF_VUIInfo));
	vuii.ar_num = ar_n;
	vuii.ar_den = ar_d;
	vuii.fullrange = -1;
	vuii.video_format = -1;
	vuii.color_prim = -1;
	vuii.color_tfc = -1;
	vuii.color_matrix = -1;
	return gf_avc_change_vui(avcc, &vuii);
}

GF_EXPORT
GF_Err gf_avc_change_color(GF_AVCConfig *avcc, s32 fullrange, s32 vidformat, s32 colorprim, s32 transfer, s32 colmatrix)
{
	GF_VUIInfo vuii;
	memset(&vuii, 0, sizeof(GF_VUIInfo));
	vuii.ar_num = -1;
	vuii.ar_den = -1;
	vuii.fullrange = fullrange;
	vuii.video_format = vidformat;
	vuii.color_prim = colorprim;
	vuii.color_tfc = transfer;
	vuii.color_matrix = colmatrix;
	return gf_avc_change_vui(avcc, &vuii);
}


GF_EXPORT
GF_Err gf_avc_get_sps_info(u8 *sps_data, u32 sps_size, u32 *sps_id, u32 *width, u32 *height, s32 *par_n, s32 *par_d)
{
	AVCState avc;
	s32 idx;
	memset(&avc, 0, sizeof(AVCState));
	avc.sps_active_idx = -1;

	idx = gf_avc_read_sps(sps_data, sps_size, &avc, 0, NULL);
	if (idx < 0) {
		return GF_NON_COMPLIANT_BITSTREAM;
	}
	if (sps_id) *sps_id = idx;

	if (width) *width = avc.sps[idx].width;
	if (height) *height = avc.sps[idx].height;
	if (par_n) *par_n = avc.sps[idx].vui.par_num ? avc.sps[idx].vui.par_num : (u32)-1;
	if (par_d) *par_d = avc.sps[idx].vui.par_den ? avc.sps[idx].vui.par_den : (u32)-1;

	return GF_OK;
}

GF_EXPORT
GF_Err gf_avc_get_pps_info(u8 *pps_data, u32 pps_size, u32 *pps_id, u32 *sps_id)
{
	GF_BitStream *bs;
	GF_Err e = GF_OK;

	bs = gf_bs_new(pps_data, pps_size, GF_BITSTREAM_READ);
	if (!bs) {
		e = GF_NON_COMPLIANT_BITSTREAM;
		goto exit;
	}
	gf_bs_enable_emulation_byte_removal(bs, GF_TRUE);
	/*nal hdr*/ gf_bs_read_int(bs, 8);

	*pps_id = gf_bs_read_ue(bs);
	*sps_id = gf_bs_read_ue(bs);

exit:
	gf_bs_del(bs);
	return e;
}

/**********
HEVC parsing
**********/

Bool gf_hevc_slice_is_intra(HEVCState *hevc)
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

Bool gf_hevc_slice_is_IDR(HEVCState *hevc)
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

static Bool hevc_parse_short_term_ref_pic_set(GF_BitStream *bs, HEVC_SPS *sps, u32 idx_rps)
{
	u32 i;
	Bool inter_ref_pic_set_prediction_flag = 0;
	if (idx_rps != 0)
		inter_ref_pic_set_prediction_flag = gf_bs_read_int_log_idx(bs, 1, "inter_ref_pic_set_prediction_flag", idx_rps);

	if (inter_ref_pic_set_prediction_flag) {
		HEVC_ReferencePictureSets *ref_ps, *rps;
		u32 delta_idx_minus1 = 0;
		u32 ref_idx;
		u32 delta_rps_sign;
		u32 abs_delta_rps_minus1, nb_ref_pics;
		s32 deltaRPS;
		u32 k = 0, k0 = 0, k1 = 0;
		if (idx_rps == sps->num_short_term_ref_pic_sets)
			delta_idx_minus1 = gf_bs_read_ue_log_idx(bs, "delta_idx_minus1", idx_rps);

		if (delta_idx_minus1 > idx_rps - 1)
			return GF_FALSE;

		ref_idx = idx_rps - 1 - delta_idx_minus1;
		delta_rps_sign = gf_bs_read_int_log_idx(bs, 1, "delta_rps_sign", idx_rps);
		abs_delta_rps_minus1 = gf_bs_read_ue_log_idx(bs, "abs_delta_rps_minus1", idx_rps);
		deltaRPS = (1 - (delta_rps_sign << 1)) * (abs_delta_rps_minus1 + 1);

		rps = &sps->rps[idx_rps];
		ref_ps = &sps->rps[ref_idx];
		nb_ref_pics = ref_ps->num_negative_pics + ref_ps->num_positive_pics;
		for (i = 0; i <= nb_ref_pics; i++) {
			s32 ref_idc;
			s32 used_by_curr_pic_flag = gf_bs_read_int_log_idx2(bs, 1, "used_by_curr_pic_flag", idx_rps, i);
			ref_idc = used_by_curr_pic_flag ? 1 : 0;
			if (!used_by_curr_pic_flag) {
				used_by_curr_pic_flag = gf_bs_read_int_log_idx2(bs, 1, "used_by_curr_pic_flag", idx_rps, i);
				ref_idc = used_by_curr_pic_flag << 1;
			}
			if ((ref_idc == 1) || (ref_idc == 2)) {
				s32 deltaPOC = deltaRPS;
				if ((i < nb_ref_pics) && (i<16)) {
					if ((ref_ps->delta_poc[i] > 0 && deltaPOC > GF_INT_MAX - ref_ps->delta_poc[i]) ||
						(ref_ps->delta_poc[i] < 0 && deltaPOC < GF_INT_MIN - ref_ps->delta_poc[i]) )

						return GF_FALSE;

					deltaPOC += ref_ps->delta_poc[i];
				}
				if (k<16)
					rps->delta_poc[k] = deltaPOC;

				if (deltaPOC < 0)  k0++;
				else k1++;

				k++;
			}
		}
		rps->num_negative_pics = k0;
		rps->num_positive_pics = k1;
	}
	else {
		s32 prev = 0, poc;
		sps->rps[idx_rps].num_negative_pics = gf_bs_read_ue_log_idx(bs, "num_negative_pics", idx_rps);
		sps->rps[idx_rps].num_positive_pics = gf_bs_read_ue_log_idx(bs, "num_positive_pics", idx_rps);
		if (sps->rps[idx_rps].num_negative_pics > 16)
			return GF_FALSE;
		if (sps->rps[idx_rps].num_positive_pics > 16)
			return GF_FALSE;
		for (i = 0; i < sps->rps[idx_rps].num_negative_pics; i++) {
			u32 delta_poc_s0_minus1 = gf_bs_read_ue_log_idx2(bs, "delta_poc_s0_minus1", idx_rps, i);
			poc = prev - delta_poc_s0_minus1 - 1;
			prev = poc;
			sps->rps[idx_rps].delta_poc[i] = poc;
			gf_bs_read_int_log_idx2(bs, 1, "used_by_curr_pic_s0_flag", idx_rps, i);
		}
		for (i = 0; i < sps->rps[idx_rps].num_positive_pics; i++) {
			u32 delta_poc_s1_minus1 = gf_bs_read_ue_log_idx2(bs, "delta_poc_s1_minus1" , idx_rps, i);
			poc = prev + delta_poc_s1_minus1 + 1;
			prev = poc;
			sps->rps[idx_rps].delta_poc[i] = poc;
			gf_bs_read_int_log_idx2(bs, 1, "used_by_curr_pic_s1_flag", idx_rps, i);
		}
	}
	return GF_TRUE;
}

void hevc_pred_weight_table(GF_BitStream *bs, HEVCState *hevc, HEVCSliceInfo *si, HEVC_PPS *pps, HEVC_SPS *sps, u32 num_ref_idx_l0_active, u32 num_ref_idx_l1_active)
{
	u32 i, num_ref_idx;
	Bool first_pass = GF_TRUE;
	u8 luma_weights[20], chroma_weights[20];
	u32 ChromaArrayType = sps->separate_colour_plane_flag ? 0 : sps->chroma_format_idc;

	num_ref_idx = num_ref_idx_l0_active < 20 ? num_ref_idx_l0_active : 19 ;

	gf_bs_read_ue_log(bs, "luma_log2_weight_denom");
	if (ChromaArrayType != 0)
		gf_bs_read_se_log(bs, "delta_chroma_log2_weight_denom");

parse_weights:
	for (i = 0; i < num_ref_idx; i++) {
		luma_weights[i] = gf_bs_read_int_log_idx(bs, 1, "luma_weights", i);
		//inferred to be 0 if not present
		chroma_weights[i] = 0;
	}
	if (ChromaArrayType != 0) {
		for (i = 0; i < num_ref_idx; i++) {
			chroma_weights[i] = gf_bs_read_int_log_idx(bs, 1, "chroma_weights", i);
		}
	}
	for (i = 0; i < num_ref_idx; i++) {
		if (luma_weights[i]) {
			gf_bs_read_se_log_idx(bs, "delta_luma_weight_l0", i);
			gf_bs_read_se_log_idx(bs, "luma_offset_l0", i);
		}
		if (chroma_weights[i]) {
			gf_bs_read_se_log_idx(bs, "delta_chroma_weight_l0_0", i);
			gf_bs_read_se_log_idx(bs, "delta_chroma_offset_l0_0", i);

			gf_bs_read_se_log_idx(bs, "delta_chroma_weight_l0_1", i);
			gf_bs_read_se_log_idx(bs, "delta_chroma_offset_l0_1", i);
		}
	}

	if (si->slice_type == GF_HEVC_SLICE_TYPE_B) {
		if (!first_pass) return;
		first_pass = GF_FALSE;
		num_ref_idx = num_ref_idx_l1_active < 20 ? num_ref_idx_l1_active : 19 ;
		goto parse_weights;
	}
}

static
Bool ref_pic_lists_modification(GF_BitStream *bs, u32 slice_type, u32 num_ref_idx_l0_active, u32 num_ref_idx_l1_active)
{
	//u32 i;
	Bool ref_pic_list_modification_flag_l0 = gf_bs_read_int_log(bs, 1, "ref_pic_list_modification_flag_l0");
	if (ref_pic_list_modification_flag_l0) {
		/*for (i=0; i<num_ref_idx_l0_active; i++) {
			list_entry_l0[i] = *//*gf_bs_read_int(bs, (u32)ceil(log(getNumPicTotalCurr())/log(2)));
		}*/
		return GF_FALSE;
	}
	if (slice_type == GF_HEVC_SLICE_TYPE_B) {
		Bool ref_pic_list_modification_flag_l1 = gf_bs_read_int_log(bs, 1, "ref_pic_list_modification_flag_l1");
		if (ref_pic_list_modification_flag_l1) {
			/*for (i=0; i<num_ref_idx_l1_active; i++) {
				list_entry_l1[i] = *//*gf_bs_read_int(bs, (u32)ceil(log(getNumPicTotalCurr()) / log(2)));
			}*/
			return GF_FALSE;
		}
	}

	return GF_TRUE;
}

static
s32 hevc_parse_slice_segment(GF_BitStream *bs, HEVCState *hevc, HEVCSliceInfo *si)
{
	u32 i, j;
	u32 num_ref_idx_l0_active = 0, num_ref_idx_l1_active = 0;
	HEVC_PPS *pps;
	HEVC_SPS *sps;
	s32 pps_id;
	Bool RapPicFlag = GF_FALSE;
	Bool IDRPicFlag = GF_FALSE;

	si->first_slice_segment_in_pic_flag = gf_bs_read_int_log(bs, 1, "first_slice_segment_in_pic_flag");

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
		gf_bs_read_int_log(bs, 1, "no_output_of_prior_pics_flag");
	}

	pps_id = gf_bs_read_ue_log(bs, "pps_id");
	if ((pps_id<0) || (pps_id >= 64))
		return -1;

	pps = &hevc->pps[pps_id];
	sps = &hevc->sps[pps->sps_id];
	si->sps = sps;
	si->pps = pps;

	if (!si->first_slice_segment_in_pic_flag && pps->dependent_slice_segments_enabled_flag) {
		si->dependent_slice_segment_flag = gf_bs_read_int_log(bs, 1, "dependent_slice_segment_flag");
	}
	else {
		si->dependent_slice_segment_flag = GF_FALSE;
	}

	if (!si->first_slice_segment_in_pic_flag) {
		si->slice_segment_address = gf_bs_read_int_log(bs, sps->bitsSliceSegmentAddress, "slice_segment_address");
	}
	else {
		si->slice_segment_address = 0;
	}

	if (!si->dependent_slice_segment_flag) {
		Bool deblocking_filter_override_flag = 0;
		Bool slice_temporal_mvp_enabled_flag = 0;
		Bool slice_sao_luma_flag = 0;
		Bool slice_sao_chroma_flag = 0;
		Bool slice_deblocking_filter_disabled_flag = 0;

		//"slice_reserved_undetermined_flag[]"
		gf_bs_read_int_log(bs, pps->num_extra_slice_header_bits, "slice_reserved_undetermined_flag");

		si->slice_type = gf_bs_read_ue_log(bs, "slice_type");

		if (pps->output_flag_present_flag)
			gf_bs_read_int_log(bs, 1, "pic_output_flag");

		if (sps->separate_colour_plane_flag == 1)
			gf_bs_read_int_log(bs, 2, "colour_plane_id");

		if (IDRPicFlag) {
			si->poc_lsb = 0;

			//if not asked to parse full header, abort since we know the poc
			if (!hevc->full_slice_header_parse) return 0;

		}
		else {
			si->poc_lsb = gf_bs_read_int_log(bs, sps->log2_max_pic_order_cnt_lsb, "poc_lsb");

			//if not asked to parse full header, abort once we have the poc
			if (!hevc->full_slice_header_parse) return 0;

			if (gf_bs_read_int_log(bs, 1, "short_term_ref_pic_set_sps_flag") == 0) {
				Bool ret = hevc_parse_short_term_ref_pic_set(bs, sps, sps->num_short_term_ref_pic_sets);
				if (!ret)
					return -1;
			}
			else if (sps->num_short_term_ref_pic_sets > 1) {
				u32 numbits = 0;

				while ((u32)(1 << numbits) < sps->num_short_term_ref_pic_sets)
					numbits++;
				if (numbits > 0)
					gf_bs_read_int_log(bs, numbits, "short_term_ref_pic_set_idx");
				/*else
					short_term_ref_pic_set_idx = 0;*/
			}
			if (sps->long_term_ref_pics_present_flag) {
				u8 DeltaPocMsbCycleLt[32];
				u32 num_long_term_sps = 0;
				u32 num_long_term_pics = 0;

				memset(DeltaPocMsbCycleLt, 0, sizeof(u8) * 32);

				if (sps->num_long_term_ref_pic_sps > 0) {
					num_long_term_sps = gf_bs_read_ue_log(bs, "num_long_term_sps");
				}
				num_long_term_pics = gf_bs_read_ue_log(bs, "num_long_term_pics");
				if (num_long_term_sps+num_long_term_pics>32) return -1;

				for (i = 0; i < num_long_term_sps + num_long_term_pics; i++) {
					if (i < num_long_term_sps) {
						if (sps->num_long_term_ref_pic_sps > 1)
							gf_bs_read_int_log_idx(bs, gf_get_bit_size(sps->num_long_term_ref_pic_sps), "lt_idx_sps", i);
					}
					else {
						gf_bs_read_int_log_idx(bs, sps->log2_max_pic_order_cnt_lsb, "PocLsbLt", i);
						gf_bs_read_int_log_idx(bs, 1, "UsedByCurrPicLt", i);
					}
					if (gf_bs_read_int_log_idx(bs, 1, "delta_poc_msb_present_flag", i)) {
						if (i == 0 || i == num_long_term_sps)
							DeltaPocMsbCycleLt[i] = gf_bs_read_ue_log_idx(bs, "DeltaPocMsbCycleLt", i);
						else
							DeltaPocMsbCycleLt[i] = gf_bs_read_ue_log_idx(bs, "DeltaPocMsbCycleLt", i) + DeltaPocMsbCycleLt[i - 1];
					}
				}
			}
			if (sps->temporal_mvp_enable_flag)
				slice_temporal_mvp_enabled_flag = gf_bs_read_int_log(bs, 1, "slice_temporal_mvp_enabled_flag");
		}
		if (sps->sample_adaptive_offset_enabled_flag) {
			u32 ChromaArrayType = sps->separate_colour_plane_flag ? 0 : sps->chroma_format_idc;
			slice_sao_luma_flag = gf_bs_read_int_log(bs, 1, "slice_sao_luma_flag");
			if (ChromaArrayType != 0)
				slice_sao_chroma_flag = gf_bs_read_int_log(bs, 1, "slice_sao_chroma_flag");
		}

		if (si->slice_type == GF_HEVC_SLICE_TYPE_P || si->slice_type == GF_HEVC_SLICE_TYPE_B) {
			//u32 NumPocTotalCurr;
			num_ref_idx_l0_active = pps->num_ref_idx_l0_default_active;
			num_ref_idx_l1_active = 0;
			if (si->slice_type == GF_HEVC_SLICE_TYPE_B)
				num_ref_idx_l1_active = pps->num_ref_idx_l1_default_active;

			if (gf_bs_read_int_log(bs, 1, "num_ref_idx_active_override_flag")) {
				num_ref_idx_l0_active = 1 + gf_bs_read_ue_log(bs, "num_ref_idx_l0_active");
				if (si->slice_type == GF_HEVC_SLICE_TYPE_B)
					num_ref_idx_l1_active = 1 + gf_bs_read_ue_log(bs, "num_ref_idx_l1_active");
			}

			if (pps->lists_modification_present_flag /*TODO: && NumPicTotalCurr > 1*/) {
				if (!ref_pic_lists_modification(bs, si->slice_type, num_ref_idx_l0_active, num_ref_idx_l1_active)) {
					GF_LOG(GF_LOG_WARNING, GF_LOG_CODING, ("[hevc] ref_pic_lists_modification( ) not implemented\n"));
					return -1;
				}
			}

			if (si->slice_type == GF_HEVC_SLICE_TYPE_B)
				gf_bs_read_int_log(bs, 1, "mvd_l1_zero_flag");
			if (pps->cabac_init_present_flag)
				gf_bs_read_int_log(bs, 1, "cabac_init_flag");

			if (slice_temporal_mvp_enabled_flag) {
				// When collocated_from_l0_flag is not present, it is inferred to be equal to 1.
				Bool collocated_from_l0_flag = 1;
				if (si->slice_type == GF_HEVC_SLICE_TYPE_B)
					collocated_from_l0_flag = gf_bs_read_int_log(bs, 1, "collocated_from_l0_flag");

				if ((collocated_from_l0_flag && (num_ref_idx_l0_active > 1))
					|| (!collocated_from_l0_flag && (num_ref_idx_l1_active > 1))
				) {
					gf_bs_read_ue_log(bs, "collocated_ref_idx");
				}
			}

			if ((pps->weighted_pred_flag && si->slice_type == GF_HEVC_SLICE_TYPE_P)
				|| (pps->weighted_bipred_flag && si->slice_type == GF_HEVC_SLICE_TYPE_B)
				) {
				hevc_pred_weight_table(bs, hevc, si, pps, sps, num_ref_idx_l0_active, num_ref_idx_l1_active);
			}
			gf_bs_read_ue_log(bs, "five_minus_max_num_merge_cand");
		}
		si->slice_qp_delta_start_bits = (s32) (gf_bs_get_position(bs) - 1) * 8 + gf_bs_get_bit_position(bs);
		si->slice_qp_delta = gf_bs_read_se_log(bs, "slice_qp_delta");

		if (pps->slice_chroma_qp_offsets_present_flag) {
			gf_bs_read_se_log(bs, "slice_cb_qp_offset");
			gf_bs_read_se_log(bs, "slice_cr_qp_offset");
		}
		if (pps->deblocking_filter_override_enabled_flag) {
			deblocking_filter_override_flag = gf_bs_read_int_log(bs, 1, "deblocking_filter_override_flag");
		}

		if (deblocking_filter_override_flag) {
			slice_deblocking_filter_disabled_flag = gf_bs_read_int_log(bs, 1, "slice_deblocking_filter_disabled_flag");
			if (!slice_deblocking_filter_disabled_flag) {
				gf_bs_read_se_log(bs, "slice_beta_offset_div2");
				gf_bs_read_se_log(bs, "slice_tc_offset_div2");
			}
		}
		if (pps->loop_filter_across_slices_enabled_flag
			&& (slice_sao_luma_flag || slice_sao_chroma_flag || !slice_deblocking_filter_disabled_flag)
		) {
			gf_bs_read_int_log(bs, 1, "slice_loop_filter_across_slices_enabled_flag");
		}
	}
	//dependent slice segment
	else {
		//if not asked to parse full header, abort
		if (!hevc->full_slice_header_parse) return 0;
	}

	si->entry_point_start_bits = ((u32)gf_bs_get_position(bs) - 1) * 8 + gf_bs_get_bit_position(bs);

	if (pps->tiles_enabled_flag || pps->entropy_coding_sync_enabled_flag) {
		u32 num_entry_point_offsets = gf_bs_read_ue_log(bs, "num_entry_point_offsets");
		if (num_entry_point_offsets > 0) {
			u32 offset = gf_bs_read_ue_log(bs, "offset") + 1;
			u32 segments = offset >> 4;
			s32 remain = (offset & 15);

			for (i = 0; i < num_entry_point_offsets; i++) {
				//u32 res = 0;
				for (j = 0; j < segments; j++) {
					//res <<= 16;
					/*res +=*/ gf_bs_read_int(bs, 16);
				}
				if (remain) {
					//res <<= remain;
					/* res += */ gf_bs_read_int(bs, remain);
				}
				// entry_point_offset = val + 1; // +1; // +1 to get the size
			}
		}
	}

	if (pps->slice_segment_header_extension_present_flag) {
		u32 size_ext = gf_bs_read_ue_log(bs, "size_ext");
		while (size_ext) {
			gf_bs_read_int(bs, 8);
			size_ext--;
		}
	}

	si->header_size_bits = (gf_bs_get_position(bs) - 1) * 8 + gf_bs_get_bit_position(bs); // av_parser.c modified on 16 jan. 2019

	if (gf_bs_read_int_log(bs, 1, "byte_align") == 0) {
		GF_LOG(GF_LOG_WARNING, GF_LOG_CODING, ("Error parsing slice header: byte_align not found at end of header !\n"));
	}

	gf_bs_align(bs);
	si->payload_start_offset = (s32)gf_bs_get_position(bs);
	return 0;
}

static void gf_hevc_vvc_parse_sei(char *buffer, u32 nal_size, HEVCState *hevc, VVCState *vvc)
{
	u32 ptype, psize, hdr, i;
	u8 *dst_ptr;
	u64 start;
	GF_BitStream *bs;

	hdr = buffer[0];
	if (((hdr & 0x7e) >> 1) != GF_HEVC_NALU_SEI_PREFIX) return;

	bs = gf_bs_new(buffer, nal_size, GF_BITSTREAM_READ);
	gf_bs_enable_emulation_byte_removal(bs, GF_TRUE);

	gf_bs_read_int(bs, 16);

	/*parse SEI*/
	while (gf_bs_available(bs)) {
		u32 consumed, nb_zeros;
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
		if (start+psize >= nal_size) {
			GF_LOG(GF_LOG_WARNING, GF_LOG_CODING, ("[%s] SEI user message type %d size error (%d but %d remain), skipping SEI message\n", hevc ? "HEVC" : "VVC", ptype, psize, nal_size-start));
			break;
		}

		nb_zeros = gf_bs_get_emulation_byte_removed(bs);

		switch (ptype) {
		case 4: /*user registered ITU-T T35*/
			if (hevc) {
				avc_parse_itu_t_t35_sei(bs, &hevc->sei.dovi);
			}
			break;
		//clli
		case 144:
			dst_ptr = hevc ? hevc->clli_data : vvc->clli_data;
			//do not use read data due to possible EPB
			for (i=0; i<4; i++)
				dst_ptr[i] = gf_bs_read_u8(bs);

			if (hevc) {
				hevc->clli_valid = 1;
			} else {
				vvc->clli_valid = 1;
			}
			break;
		//mdcv
		case 137:
			dst_ptr = hevc ? hevc->mdcv_data : vvc->mdcv_data;
			//do not use read data due to possible EPB
			for (i=0; i<24; i++)
				dst_ptr[i] = gf_bs_read_u8(bs);

			if (hevc) {
				hevc->mdcv_valid = 1;
			} else {
				vvc->mdcv_valid = 1;
			}
			break;
		default:
			break;
		}
		nb_zeros = gf_bs_get_emulation_byte_removed(bs) - nb_zeros;

		gf_bs_align(bs);
		consumed = (u32) (gf_bs_get_position(bs) - start);
		consumed -= nb_zeros;
		psize-=consumed;
		//do not use skip bytes due to possible EPB
		while (psize) {
			gf_bs_read_u8(bs);
			psize--;
		}
		if (gf_bs_available(bs) <= 2)
			break;
	}
	gf_bs_del(bs);
}

void gf_hevc_parse_sei(char *buffer, u32 nal_size, HEVCState *hevc)
{
	gf_hevc_vvc_parse_sei(buffer, nal_size, hevc, NULL);
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

	if ((si->poc_lsb < si->poc_lsb_prev) && (si->poc_lsb_prev - si->poc_lsb >= max_poc_lsb / 2))
		si->poc_msb = si->poc_msb_prev + max_poc_lsb;
	else if ((si->poc_lsb > si->poc_lsb_prev) && (si->poc_lsb - si->poc_lsb_prev > max_poc_lsb / 2))
		si->poc_msb = si->poc_msb_prev - max_poc_lsb;
	else
		si->poc_msb = si->poc_msb_prev;

	switch (si->nal_unit_type) {
	case GF_HEVC_NALU_SLICE_BLA_W_LP:
	case GF_HEVC_NALU_SLICE_BLA_W_DLP:
	case GF_HEVC_NALU_SLICE_BLA_N_LP:
		si->poc_msb = 0;
		break;
	}
	si->poc = si->poc_msb + si->poc_lsb;
}


static Bool hevc_parse_nal_header(GF_BitStream *bs, u8 *nal_unit_type, u8 *temporal_id, u8 *layer_id)
{
	u32 val;
	val = gf_bs_read_int_log(bs, 1, "forbidden_zero");
	if (val) return GF_FALSE;

	val = gf_bs_read_int_log(bs, 6, "nuh_type");
	if (nal_unit_type) *nal_unit_type = val;

	val = gf_bs_read_int_log(bs, 6, "layerID");
	if (layer_id) *layer_id = val;

	val = gf_bs_read_int_log(bs, 3, "temporalID");
	if (!val)
		return GF_FALSE;
	val -= 1;
	if (temporal_id) *temporal_id = val;
	return GF_TRUE;
}


void hevc_profile_tier_level(GF_BitStream *bs, Bool ProfilePresentFlag, u8 MaxNumSubLayersMinus1, HEVC_ProfileTierLevel *ptl, u32 idx)
{
	u32 i;
	if (ProfilePresentFlag) {
		ptl->profile_space = gf_bs_read_int_log_idx(bs, 2, "profile_space", idx);
		ptl->tier_flag = gf_bs_read_int_log_idx(bs, 1, "tier_flag", idx);
		ptl->profile_idc = gf_bs_read_int_log_idx(bs, 5, "profile_idc", idx);

		ptl->profile_compatibility_flag = gf_bs_read_int_log_idx(bs, 32, "profile_compatibility_flag", idx);

		ptl->general_progressive_source_flag = gf_bs_read_int_log_idx(bs, 1, "general_progressive_source_flag", idx);
		ptl->general_interlaced_source_flag = gf_bs_read_int_log_idx(bs, 1, "general_interlaced_source_flag", idx);
		ptl->general_non_packed_constraint_flag = gf_bs_read_int_log_idx(bs, 1, "general_non_packed_constraint_flag", idx);
		ptl->general_frame_only_constraint_flag = gf_bs_read_int_log_idx(bs, 1, "general_frame_only_constraint_flag", idx);
		ptl->general_reserved_44bits = gf_bs_read_long_int(bs, 44);
	}
	ptl->level_idc = gf_bs_read_int_log(bs, 8, "level_idc");
	for (i = 0; i < MaxNumSubLayersMinus1; i++) {
		ptl->sub_ptl[i].profile_present_flag = gf_bs_read_int_log_idx2(bs, 1, "profile_present_flag", idx, i);
		ptl->sub_ptl[i].level_present_flag = gf_bs_read_int_log_idx2(bs, 1, "level_present_flag", idx, i);
	}
	if (MaxNumSubLayersMinus1 > 0) {
		for (i = MaxNumSubLayersMinus1; i < 8; i++) {
			/*reserved_zero_2bits*/gf_bs_read_int(bs, 2);
		}
	}

	for (i = 0; i < MaxNumSubLayersMinus1; i++) {
		if (ptl->sub_ptl[i].profile_present_flag) {
			ptl->sub_ptl[i].profile_space = gf_bs_read_int_log_idx2(bs, 2, "sublayer_profile_space", idx, i);
			ptl->sub_ptl[i].tier_flag = gf_bs_read_int_log_idx2(bs, 1, "sublayer_tier_flag", idx, i);
			ptl->sub_ptl[i].profile_idc = gf_bs_read_int_log_idx2(bs, 5, "sublayer_profile_idc", idx, i);
			ptl->sub_ptl[i].profile_compatibility_flag = gf_bs_read_int_log_idx2(bs, 32, "sublayer_profile_compatibility_flag", idx, i);
			/*ptl->sub_ptl[i].progressive_source_flag =*/ gf_bs_read_int_log_idx2(bs, 1, "sublayer_progressive_source_flag", idx, i);
			/*ptl->sub_ptl[i].interlaced_source_flag =*/ gf_bs_read_int_log_idx2(bs, 1, "sublayer_interlaced_source_flag", idx, i);
			/*ptl->sub_ptl[i].non_packed_constraint_flag =*/ gf_bs_read_int_log_idx2(bs, 1, "sublayer_non_packed_constraint_flag", idx, i);
			/*ptl->sub_ptl[i].frame_only_constraint_flag =*/ gf_bs_read_int_log_idx2(bs, 1, "sublayer_frame_only_constraint_flag", idx, i);
			/*ptl->sub_ptl[i].reserved_44bits =*/ gf_bs_read_long_int(bs, 44);
		}
		if (ptl->sub_ptl[i].level_present_flag)
			ptl->sub_ptl[i].level_idc = gf_bs_read_int_log_idx2(bs, 8, "sublayer_level_idc", idx, i);
	}
}

static u32 scalability_type_to_idx(HEVC_VPS *vps, u32 scalability_type)
{
	u32 idx = 0, type;
	for (type = 0; type < scalability_type; type++) {
		idx += (vps->scalability_mask[type] ? 1 : 0);
	}
	return idx;
}

#define LHVC_VIEW_ORDER_INDEX  1
#define LHVC_SCALABILITY_INDEX	2

static u32 lhvc_get_scalability_id(HEVC_VPS *vps, u32 layer_id_in_vps, u32 scalability_type)
{
	u32 idx;
	if (!vps->scalability_mask[scalability_type]) return 0;
	idx = scalability_type_to_idx(vps, scalability_type);
	return vps->dimension_id[layer_id_in_vps][idx];
}

static u32 lhvc_get_view_index(HEVC_VPS *vps, u32 id)
{
	return lhvc_get_scalability_id(vps, vps->layer_id_in_vps[id], LHVC_VIEW_ORDER_INDEX);
}

static u32 lhvc_get_num_views(HEVC_VPS *vps)
{
	u32 numViews = 1, i;
	for (i = 0; i < vps->max_layers; i++) {
		u32 layer_id = vps->layer_id_in_nuh[i];
		if (i > 0 && (lhvc_get_view_index(vps, layer_id) != lhvc_get_scalability_id(vps, i - 1, LHVC_VIEW_ORDER_INDEX))) {
			numViews++;
		}
	}
	return numViews;
}

static void lhvc_parse_rep_format(HEVC_RepFormat *fmt, GF_BitStream *bs, u32 idx)
{
	u8 chroma_bitdepth_present_flag;
	fmt->pic_width_luma_samples = gf_bs_read_int_log_idx(bs, 16, "pic_width_luma_samples", idx);
	fmt->pic_height_luma_samples = gf_bs_read_int_log_idx(bs, 16, "pic_height_luma_samples", idx);
	chroma_bitdepth_present_flag = gf_bs_read_int_log_idx(bs, 1, "chroma_bitdepth_present_flag", idx);
	if (chroma_bitdepth_present_flag) {
		fmt->chroma_format_idc = gf_bs_read_int_log_idx(bs, 2, "chroma_format_idc", idx);

		if (fmt->chroma_format_idc == 3)
			fmt->separate_colour_plane_flag = gf_bs_read_int_log_idx(bs, 1, "separate_colour_plane_flag", idx);
		fmt->bit_depth_luma = 8 + gf_bs_read_int_log_idx(bs, 4, "bit_depth_luma_minus8", idx);
		fmt->bit_depth_chroma = 8 + gf_bs_read_int_log_idx(bs, 4, "bit_depth_chroma_minus8", idx);
	}
	if (gf_bs_read_int_log_idx(bs, 1, "conformance_window_vps_flag", idx)) {
		gf_bs_read_ue_log_idx(bs, "conf_win_vps_left_offset", idx);
		gf_bs_read_ue_log_idx(bs, "conf_win_vps_right_offset", idx);
		gf_bs_read_ue_log_idx(bs, "conf_win_vps_top_offset", idx);
		gf_bs_read_ue_log_idx(bs, "conf_win_vps_bottom_offset", idx);
	}
}


static Bool hevc_parse_vps_extension(HEVC_VPS *vps, GF_BitStream *bs)
{
	u8 splitting_flag, vps_nuh_layer_id_present_flag, view_id_len;
	u32 i, j, num_scalability_types, num_add_olss, num_add_layer_set, num_indepentdent_layers, nb_bits, default_output_layer_idc = 0;
	u8 dimension_id_len[16], dim_bit_offset[16];
	u8 /*avc_base_layer_flag, */NumLayerSets, /*default_one_target_output_layer_flag, */rep_format_idx_present_flag, ols_ids_to_ls_idx;
	u8 layer_set_idx_for_ols_minus1[MAX_LHVC_LAYERS];
	u8 nb_output_layers_in_output_layer_set[MAX_LHVC_LAYERS + 1];
	u8 ols_highest_output_layer_id[MAX_LHVC_LAYERS + 1];

	u32 k, d, r, p, iNuhLId, jNuhLId;
	u8 num_direct_ref_layers[64], num_pred_layers[64], num_layers_in_tree_partition[MAX_LHVC_LAYERS];
	u8 dependency_flag[MAX_LHVC_LAYERS][MAX_LHVC_LAYERS], id_pred_layers[64][MAX_LHVC_LAYERS];
	//	u8 num_ref_layers[64];
	//	u8 tree_partition_layer_id[MAX_LHVC_LAYERS][MAX_LHVC_LAYERS];
	//	u8 id_ref_layers[64][MAX_LHVC_LAYERS];
	//	u8 id_direct_ref_layers[64][MAX_LHVC_LAYERS];
	u8 layer_id_in_list_flag[64];
	Bool OutputLayerFlag[MAX_LHVC_LAYERS][MAX_LHVC_LAYERS];

	vps->vps_extension_found = 1;
	if ((vps->max_layers > 1) && vps->base_layer_internal_flag)
		hevc_profile_tier_level(bs, 0, vps->max_sub_layers - 1, &vps->ext_ptl[0], 0);

	splitting_flag = gf_bs_read_int_log(bs, 1, "splitting_flag");
	num_scalability_types = 0;
	for (i = 0; i < 16; i++) {
		vps->scalability_mask[i] = gf_bs_read_int_log_idx(bs, 1, "scalability_mask", i);
		num_scalability_types += vps->scalability_mask[i];
	}
	if (num_scalability_types >= 16) {
		num_scalability_types = 16;
	}
	dimension_id_len[0] = 0;
	if (num_scalability_types) {
		for (i = 0; i < (num_scalability_types - splitting_flag); i++) {
			dimension_id_len[i] = 1 + gf_bs_read_int_log_idx(bs, 3, "dimension_id_len_minus1", i);
		}

		if (splitting_flag) {
			u32 num_bits=0;
			for (i = 0; i < num_scalability_types-1; i++) {
				dim_bit_offset[i] = 0;
				num_bits+=dimension_id_len[i];
				for (j = 0; j < i; j++)
					dim_bit_offset[i] += dimension_id_len[j];
			}
			if (num_bits>=6) {
				GF_LOG(GF_LOG_ERROR, GF_LOG_CODING, ("[HEVC] Too many bits defined for dimension IDs (%d vs 5 max)\n", num_bits));
				return GF_FALSE;
			}
			dimension_id_len[num_scalability_types - 1] = 6 - num_bits; //1 + (5 - dim_bit_offset[num_scalability_types - 1]);
			dim_bit_offset[num_scalability_types - 1] = 6;
		}
	}

	vps_nuh_layer_id_present_flag = gf_bs_read_int_log(bs, 1, "vps_nuh_layer_id_present_flag");
	vps->layer_id_in_nuh[0] = 0;
	vps->layer_id_in_vps[0] = 0;
	for (i = 1; i < vps->max_layers; i++) {
		if (vps_nuh_layer_id_present_flag) {
			vps->layer_id_in_nuh[i] = gf_bs_read_int_log_idx(bs, 6, "layer_id_in_nuh", i);
		}
		else {
			vps->layer_id_in_nuh[i] = i;
		}
		if (vps->layer_id_in_nuh[i] >= MAX_LHVC_LAYERS) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_CODING, ("[HEVC] %d layers in VPS ext but only %d supported in GPAC\n", 1+vps->layer_id_in_nuh[i], MAX_LHVC_LAYERS));
			vps->layer_id_in_nuh[i] = 0;
			return GF_FALSE;
		}
		vps->layer_id_in_vps[vps->layer_id_in_nuh[i]] = i;

		if (!splitting_flag) {
			for (j = 0; j < num_scalability_types; j++) {
				vps->dimension_id[i][j] = gf_bs_read_int_log_idx2(bs, dimension_id_len[j], "dimension_id", i, j);
			}
		}
	}

	if (splitting_flag) {
		if (num_scalability_types==16)
			return GF_FALSE;
		for (i = 0; i < vps->max_layers; i++)
			for (j = 0; j < num_scalability_types; j++) {
				if (dim_bit_offset[j + 1] <= 31)
					vps->dimension_id[i][j] = ((vps->layer_id_in_nuh[i] & ((1 << dim_bit_offset[j + 1]) - 1)) >> dim_bit_offset[j]);
				else
					return GF_FALSE;
			}
	}
	else {
		for (j = 0; j < num_scalability_types; j++)
			vps->dimension_id[0][j] = 0;
	}

	view_id_len = gf_bs_read_int_log(bs, 4, "view_id_len");
	if (view_id_len > 0) {
		for (i = 0; i < lhvc_get_num_views(vps); i++) {
			gf_bs_read_int_log_idx(bs, view_id_len, "view_id_val", i);
		}
	}

	for (i = 1; i < vps->max_layers; i++) {
		for (j = 0; j < i; j++) {
			vps->direct_dependency_flag[i][j] = gf_bs_read_int_log_idx(bs, 1, "direct_dependency_flag", i);
		}
	}

	//we do the test on MAX_LHVC_LAYERS and break in the loop to avoid a wrong GCC 4.8 warning on array bounds
	for (i = 0; i < MAX_LHVC_LAYERS; i++) {
		if (i >= vps->max_layers) break;
		for (j = 0; j < vps->max_layers; j++) {
			dependency_flag[i][j] = vps->direct_dependency_flag[i][j];
			for (k = 0; k < i; k++)
				if (vps->direct_dependency_flag[i][k] && vps->direct_dependency_flag[k][j])
					dependency_flag[i][j] = 1;
		}
	}

	for (i = 0; i < vps->max_layers; i++) {
		iNuhLId = vps->layer_id_in_nuh[i];
		d = r = p = 0;
		for (j = 0; j < vps->max_layers; j++) {
			jNuhLId = vps->layer_id_in_nuh[j];
			if (vps->direct_dependency_flag[i][j]) {
				//				id_direct_ref_layers[iNuhLId][d] = jNuhLId;
				d++;
			}
			if (dependency_flag[i][j]) {
				//				id_ref_layers[iNuhLId][r] = jNuhLId;
				r++;
			}

			if (dependency_flag[j][i])
				id_pred_layers[iNuhLId][p++] = jNuhLId;
		}
		num_direct_ref_layers[iNuhLId] = d;
		//		num_ref_layers[iNuhLId] = r;
		num_pred_layers[iNuhLId] = p;
	}

	memset(layer_id_in_list_flag, 0, 64 * sizeof(u8));
	k = 0; //num_indepentdent_layers
	for (i = 0; i < vps->max_layers; i++) {
		iNuhLId = vps->layer_id_in_nuh[i];
		if (!num_direct_ref_layers[iNuhLId]) {
			u32 h = 1;
			//tree_partition_layer_id[k][0] = iNuhLId;
			for (j = 0; j < num_pred_layers[iNuhLId]; j++) {
				u32 predLId = id_pred_layers[iNuhLId][j];
				if (!layer_id_in_list_flag[predLId]) {
					//tree_partition_layer_id[k][h++] = predLId;
					layer_id_in_list_flag[predLId] = 1;
				}
			}
			num_layers_in_tree_partition[k++] = h;
		}
	}
	num_indepentdent_layers = k;

	num_add_layer_set = 0;
	if (num_indepentdent_layers > 1)
		num_add_layer_set = gf_bs_read_ue_log(bs, "num_add_layer_set");

	for (i = 0; i < num_add_layer_set; i++)
		for (j = 1; j < num_indepentdent_layers; j++) {
			nb_bits = 1;
			while ((1 << nb_bits) < (num_layers_in_tree_partition[j] + 1))
				nb_bits++;
			gf_bs_read_int_log_idx2(bs, nb_bits, "highest_layer_idx_plus1", i, j);
		}


	if (gf_bs_read_int_log(bs, 1, "vps_sub_layers_max_minus1_present_flag")) {
		for (i = 0; i < vps->max_layers; i++) {
			gf_bs_read_int_log_idx(bs, 3, "sub_layers_vps_max_minus1", i);
		}
	}

	if (gf_bs_read_int_log(bs, 1, "max_tid_ref_present_flag")) {
		for (i = 0; i < (vps->max_layers - 1); i++) {
			for (j = i + 1; j < vps->max_layers; j++) {
				if (vps->direct_dependency_flag[j][i])
					gf_bs_read_int_log_idx2(bs, 3, "max_tid_il_ref_pics_plus1", i, j);
			}
		}
	}
	gf_bs_read_int_log(bs, 1, "default_ref_layers_active_flag");

	vps->num_profile_tier_level = 1 + gf_bs_read_ue_log(bs, "num_profile_tier_level");
	if (vps->num_profile_tier_level > MAX_LHVC_LAYERS) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_CODING, ("[HEVC] Wrong number of PTLs in VPS %d\n", vps->num_profile_tier_level));
		vps->num_profile_tier_level = 1;
		return GF_FALSE;
	}

	for (i = vps->base_layer_internal_flag ? 2 : 1; i < vps->num_profile_tier_level; i++) {
		Bool vps_profile_present_flag = gf_bs_read_int_log_idx(bs, 1, "vps_profile_present_flag", i);
		hevc_profile_tier_level(bs, vps_profile_present_flag, vps->max_sub_layers - 1, &vps->ext_ptl[i - 1], i-1);
	}

	NumLayerSets = vps->num_layer_sets + num_add_layer_set;
	num_add_olss = 0;

	if (NumLayerSets > 1) {
		num_add_olss = gf_bs_read_ue_log(bs, "num_add_olss");
		default_output_layer_idc = gf_bs_read_int_log(bs, 2, "default_output_layer_idc");
		default_output_layer_idc = default_output_layer_idc < 2 ? default_output_layer_idc : 2;
	}
	vps->num_output_layer_sets = num_add_olss + NumLayerSets;

	if (vps->num_output_layer_sets > MAX_LHVC_LAYERS) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_CODING, ("[HEVC] Wrong number of output layer sets in VPS %d, max %d supported\n", vps->num_output_layer_sets, MAX_LHVC_LAYERS));
		vps->num_output_layer_sets = 1;
		return GF_FALSE;
	}

	layer_set_idx_for_ols_minus1[0] = 1;
	vps->output_layer_flag[0][0] = 1;

	for (i = 0; i < vps->num_output_layer_sets; i++) {
		if ((NumLayerSets > 2) && (i >= NumLayerSets)) {
			nb_bits = 1;
			while ((1 << nb_bits) < (NumLayerSets - 1))
				nb_bits++;
			layer_set_idx_for_ols_minus1[i] = gf_bs_read_int_log_idx(bs, nb_bits, "layer_set_idx_for_ols_minus1", i);
		}
		else
			layer_set_idx_for_ols_minus1[i] = 0;
		ols_ids_to_ls_idx = i < NumLayerSets ? i : layer_set_idx_for_ols_minus1[i] + 1;

		if ((i > (vps->num_layer_sets - 1)) || (default_output_layer_idc == 2)) {
			for (j = 0; j < vps->num_layers_in_id_list[ols_ids_to_ls_idx]; j++)
				vps->output_layer_flag[i][j] = gf_bs_read_int_log_idx2(bs, 1, "output_layer_flag", i, j);
		}

		if ((default_output_layer_idc == 0) || (default_output_layer_idc == 1)) {
			for (j = 0; j < vps->num_layers_in_id_list[ols_ids_to_ls_idx]; j++) {
				if ((default_output_layer_idc == 0) || (vps->LayerSetLayerIdList[i][j] == vps->LayerSetLayerIdListMax[i]))
					OutputLayerFlag[i][j] = GF_TRUE;
				else
					OutputLayerFlag[i][j] = GF_FALSE;
			}
		}

		for (j = 0; j < vps->num_layers_in_id_list[ols_ids_to_ls_idx]; j++) {
			if (OutputLayerFlag[i][j]) {
				u32 curLayerID;
				vps->necessary_layers_flag[i][j] = GF_TRUE;
				curLayerID = vps->LayerSetLayerIdList[i][j];
				for (k = 0; k < j; k++) {
					u32 refLayerId = vps->LayerSetLayerIdList[i][k];
					if (dependency_flag[vps->layer_id_in_vps[curLayerID]][vps->layer_id_in_vps[refLayerId]])
						vps->necessary_layers_flag[i][k] = GF_TRUE;
				}
			}
		}
		vps->num_necessary_layers[i] = 0;
		for (j = 0; j < vps->num_layers_in_id_list[ols_ids_to_ls_idx]; j++) {
			if (vps->necessary_layers_flag[i][j])
				vps->num_necessary_layers[i] += 1;
		}

		if (i == 0) {
			if (vps->base_layer_internal_flag) {
				if (vps->max_layers > 1)
					vps->profile_tier_level_idx[0][0] = 1;
				else
					vps->profile_tier_level_idx[0][0] = 0;
			}
			continue;
		}
		nb_bits = 1;
		while ((u32)(1 << nb_bits) < vps->num_profile_tier_level)
			nb_bits++;
		for (j = 0; j < vps->num_layers_in_id_list[ols_ids_to_ls_idx]; j++)
			if (vps->necessary_layers_flag[i][j] && vps->num_profile_tier_level)
				vps->profile_tier_level_idx[i][j] = gf_bs_read_int_log_idx2(bs, nb_bits, "profile_tier_level_idx", i, j);
			else
				vps->profile_tier_level_idx[i][j] = 0;


		nb_output_layers_in_output_layer_set[i] = 0;
		for (j = 0; j < vps->num_layers_in_id_list[ols_ids_to_ls_idx]; j++) {
			nb_output_layers_in_output_layer_set[i] += OutputLayerFlag[i][j];
			if (OutputLayerFlag[i][j]) {
				ols_highest_output_layer_id[i] = vps->LayerSetLayerIdList[ols_ids_to_ls_idx][j];
			}
		}
		if (nb_output_layers_in_output_layer_set[i] == 1 && ols_highest_output_layer_id[i] > 0)
			vps->alt_output_layer_flag[i] = gf_bs_read_int_log_idx(bs, 1, "alt_output_layer_flag", i);
	}

	vps->num_rep_formats = 1 + gf_bs_read_ue_log(bs, "num_rep_formats_minus1");
	if (vps->num_rep_formats > 16) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_CODING, ("[HEVC] Wrong number of rep formats in VPS %d\n", vps->num_rep_formats));
		vps->num_rep_formats = 0;
		return GF_FALSE;
	}

	for (i = 0; i < vps->num_rep_formats; i++) {
		lhvc_parse_rep_format(&vps->rep_formats[i], bs, i);
	}
	if (vps->num_rep_formats > 1)
		rep_format_idx_present_flag = gf_bs_read_int_log(bs, 1, "rep_format_idx_present_flag");
	else
		rep_format_idx_present_flag = 0;

	vps->rep_format_idx[0] = 0;
	nb_bits = 1;
	while ((u32)(1 << nb_bits) < vps->num_rep_formats)
		nb_bits++;
	for (i = vps->base_layer_internal_flag ? 1 : 0; i < vps->max_layers; i++) {
		if (rep_format_idx_present_flag) {
			vps->rep_format_idx[i] = gf_bs_read_int_log_idx(bs, nb_bits, "rep_format_idx", i);
		}
		else {
			vps->rep_format_idx[i] = i < vps->num_rep_formats - 1 ? i : vps->num_rep_formats - 1;
		}
	}
	//TODO - we don't use the rest ...

	return GF_TRUE;
}

#define HEVC_VPS_BROKEN {\
	memset(vps, 0, sizeof(HEVC_VPS)); \
	return -1;\
	}

static s32 gf_hevc_read_vps_bs_internal(GF_BitStream *bs, HEVCState *hevc, Bool stop_at_vps_ext)
{
	u8 vps_sub_layer_ordering_info_present_flag, vps_extension_flag;
	u32 i, j;
	s32 vps_id;
	HEVC_VPS *vps;
	u8 layer_id_included_flag[MAX_LHVC_LAYERS][64];

	//nalu header already parsed
	vps_id = gf_bs_read_int_log(bs, 4, "vps_id");

	if ((vps_id<0) || (vps_id >= 16)) return -1;

	vps = &hevc->vps[vps_id];
	vps->bit_pos_vps_extensions = -1;
	if (!vps->state) {
		vps->id = vps_id;
		vps->state = 1;
	}

	vps->base_layer_internal_flag = gf_bs_read_int_log(bs, 1, "base_layer_internal_flag");
	vps->base_layer_available_flag = gf_bs_read_int_log(bs, 1, "base_layer_available_flag");
	vps->max_layers = 1 + gf_bs_read_int_log(bs, 6, "max_layers_minus1");
	if (vps->max_layers > MAX_LHVC_LAYERS) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_CODING, ("[HEVC] %d layers in VPS but only %d supported in GPAC\n", vps->max_layers, MAX_LHVC_LAYERS));
		HEVC_VPS_BROKEN
	}
	vps->max_sub_layers = gf_bs_read_int_log(bs, 3, "max_sub_layers_minus1") + 1;
	vps->temporal_id_nesting = gf_bs_read_int_log(bs, 1, "temporal_id_nesting");
	gf_bs_read_int_log(bs, 16, "vps_reserved_ffff_16bits");
	hevc_profile_tier_level(bs, 1, vps->max_sub_layers - 1, &vps->ptl, 0);

	vps_sub_layer_ordering_info_present_flag = gf_bs_read_int_log(bs, 1, "vps_sub_layer_ordering_info_present_flag");
	for (i = (vps_sub_layer_ordering_info_present_flag ? 0 : vps->max_sub_layers - 1); i < vps->max_sub_layers; i++) {
		gf_bs_read_ue_log_idx(bs, "vps_max_dec_pic_buffering_minus1", i);
		gf_bs_read_ue_log_idx(bs, "vps_max_num_reorder_pics", i);
		gf_bs_read_ue_log_idx(bs, "vps_max_latency_increase_plus1", i);
	}
	vps->max_layer_id = gf_bs_read_int_log(bs, 6, "max_layer_id");
	if (vps->max_layer_id >= MAX_LHVC_LAYERS) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_CODING, ("[HEVC] VPS max layer ID %u but GPAC only supports %u\n", vps->max_layer_id, MAX_LHVC_LAYERS));
		HEVC_VPS_BROKEN
	}
	vps->num_layer_sets = gf_bs_read_ue_log(bs, "num_layer_sets_minus1") + 1;
	if (vps->num_layer_sets > MAX_LHVC_LAYERS) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_CODING, ("[HEVC] Wrong number of layer sets in VPS %d\n", vps->num_layer_sets));
		HEVC_VPS_BROKEN
	}
	for (i = 1; i < vps->num_layer_sets; i++) {
		for (j = 0; j <= vps->max_layer_id; j++) {
			layer_id_included_flag[i][j] = gf_bs_read_int_log_idx2(bs, 1, "layer_id_included_flag", i, j);
		}
	}
	vps->num_layers_in_id_list[0] = 1;
	for (i = 1; i < vps->num_layer_sets; i++) {
		u32 n, m;
		n = 0;
		for (m = 0; m <= vps->max_layer_id; m++) {
			if (layer_id_included_flag[i][m]) {
				vps->LayerSetLayerIdList[i][n++] = m;
				if (vps->LayerSetLayerIdListMax[i] < m)
					vps->LayerSetLayerIdListMax[i] = m;
			}
		}
		vps->num_layers_in_id_list[i] = n;
	}
	if (gf_bs_read_int_log(bs, 1, "vps_timing_info_present_flag")) {
		u32 vps_num_hrd_parameters;
		gf_bs_read_int_log(bs, 32, "vps_num_units_in_tick");
		gf_bs_read_int_log(bs, 32, "vps_time_scale");
		if (gf_bs_read_int_log(bs, 1, "vps_poc_proportional_to_timing_flag")) {
			gf_bs_read_ue_log(bs, "vps_num_ticks_poc_diff_one_minus1");
		}
		vps_num_hrd_parameters = gf_bs_read_ue_log(bs, "vps_num_hrd_parameters");
		for (i = 0; i < vps_num_hrd_parameters; i++) {
			Bool cprms_present_flag = GF_TRUE;
			gf_bs_read_ue_log_idx(bs, "hrd_layer_set_idx", i);
			if (i > 0)
				cprms_present_flag = gf_bs_read_int_log(bs, 1, "cprms_present_flag");
			hevc_parse_hrd_parameters(bs, cprms_present_flag, vps->max_sub_layers - 1, i);
		}
	}
	if (stop_at_vps_ext) {
		return vps_id;
	}

	vps_extension_flag = gf_bs_read_int_log(bs, 1, "vps_extension_flag");
	if (vps_extension_flag) {
		Bool res;
		gf_bs_align(bs);
		res = hevc_parse_vps_extension(vps, bs);
		if (res != GF_TRUE) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_CODING, ("[HEVC] Failed to parse VPS extensions\n"));
			HEVC_VPS_BROKEN
		}
		if (gf_bs_read_int_log(bs, 1, "vps_extension2_flag")) {
#if 0
			while (gf_bs_available(bs)) {
				/*vps_extension_data_flag */ gf_bs_read_int(bs, 1);
			}
#endif

		}
	}
	return vps_id;
}

GF_EXPORT
s32 gf_hevc_read_vps_ex(u8 *data, u32 *size, HEVCState *hevc, Bool remove_extensions)
{
	GF_BitStream *bs;
	char *data_without_emulation_bytes = NULL;
	u32 data_without_emulation_bytes_size = 0;
	s32 vps_id = -1;

	/*still contains emulation bytes*/
	data_without_emulation_bytes_size = remove_extensions ? gf_media_nalu_emulation_bytes_remove_count(data, (*size)) : 0;
	if (!data_without_emulation_bytes_size) {
		bs = gf_bs_new(data, (*size), GF_BITSTREAM_READ);
		gf_bs_enable_emulation_byte_removal(bs, GF_TRUE);
	}
	//when removing VPS ext, we have to get the full buffer without emulation prevention bytes becuase we do a bit-by-bit copy of the vps
	else {
		data_without_emulation_bytes = gf_malloc((*size) * sizeof(char));
		data_without_emulation_bytes_size = gf_media_nalu_remove_emulation_bytes(data, data_without_emulation_bytes, (*size));
		bs = gf_bs_new(data_without_emulation_bytes, data_without_emulation_bytes_size, GF_BITSTREAM_READ);
	}
	if (!bs) goto exit;


	if (!hevc_parse_nal_header(bs, NULL, NULL, NULL)) goto exit;

	vps_id = gf_hevc_read_vps_bs_internal(bs, hevc, remove_extensions);
	if (vps_id < 0) goto exit;

	if (remove_extensions) {
		u8 *new_vps;
		u32 new_vps_size, emulation_bytes;
		u32 bit_pos = gf_bs_get_bit_offset(bs);
		GF_BitStream *w_bs = gf_bs_new(NULL, 0, GF_BITSTREAM_WRITE);
		gf_bs_seek(bs, 0);
		gf_bs_write_u8(w_bs, gf_bs_read_u8(bs) );
		gf_bs_write_u8(w_bs, gf_bs_read_u8(bs) );
		gf_bs_write_u8(w_bs, gf_bs_read_u8(bs) );
		gf_bs_write_u8(w_bs, gf_bs_read_u8(bs) );
		gf_bs_write_u16(w_bs, gf_bs_read_u16(bs) );
		bit_pos -= 48;
		while (bit_pos) {
			u32 v = gf_bs_read_int(bs, 1);
			gf_bs_write_int(w_bs, v, 1);
			bit_pos--;
		}
		/*vps extension flag*/
		gf_bs_write_int(w_bs, 0, 1);
		new_vps = NULL;
		gf_bs_get_content(w_bs, &new_vps, &new_vps_size);
		gf_bs_del(w_bs);

		emulation_bytes = gf_media_nalu_emulation_bytes_add_count(new_vps, new_vps_size);
		if (emulation_bytes + new_vps_size > *size) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_CODING, ("Buffer too small to rewrite VPS - skipping rewrite\n"));
		}
		else {
			*size = gf_media_nalu_add_emulation_bytes(new_vps, data, new_vps_size);
		}
		if (new_vps)
			gf_free(new_vps);
	}

exit:
	if (bs)
		gf_bs_del(bs);
	if (data_without_emulation_bytes) gf_free(data_without_emulation_bytes);
	return vps_id;
}

GF_EXPORT
s32 gf_hevc_read_vps(u8 *data, u32 size, HEVCState *hevc)
{
	return gf_hevc_read_vps_ex(data, &size, hevc, GF_FALSE);
}

GF_EXPORT
s32 gf_hevc_read_vps_bs(GF_BitStream *bs, HEVCState *hevc)
{
	if (!bs || !hevc) return -1;
	gf_bs_enable_emulation_byte_removal(bs, GF_TRUE);
	if (!hevc_parse_nal_header(bs, NULL, NULL, NULL)) return -1;
	return gf_hevc_read_vps_bs_internal(bs, hevc, GF_FALSE);
}

static void hevc_scaling_list_data(GF_BitStream *bs)
{
	u32 i, sizeId, matrixId;
	for (sizeId = 0; sizeId < 4; sizeId++) {
		for (matrixId = 0; matrixId < 6; matrixId += (sizeId == 3) ? 3 : 1) {
			u32 idx = sizeId*100 + 10*matrixId;
			u32 scaling_list_pred_mode_flag_sizeId_matrixId = gf_bs_read_int_log_idx(bs, 1, "scaling_list_pred_mode_flag_sizeId_matrixId", idx);
			if (!scaling_list_pred_mode_flag_sizeId_matrixId) {
				gf_bs_read_ue_log_idx(bs, "scaling_list_pred_matrix_id_delta", idx);
			}
			else {
				//u32 nextCoef = 8;
				u32 coefNum = MIN(64, (1 << (4 + (sizeId << 1))));
				if (sizeId > 1) {
					gf_bs_read_se_log_idx(bs, "scaling_list_dc_coef_minus8", idx);
				}
				for (i = 0; i < coefNum; i++) {
					gf_bs_read_se_log_idx2(bs, "scaling_list_delta_coef", idx, i);
				}
			}
		}
	}
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

#define HEVC_SPS_BROKEN {\
	memset(sps, 0, sizeof(HEVC_SPS)); \
	return -1;\
	}

static s32 gf_hevc_read_sps_bs_internal(GF_BitStream *bs, HEVCState *hevc, u8 layer_id, u32 *vui_flag_pos)
{
	s32 vps_id, sps_id = -1;
	u32 i, nb_CTUs, depth;
	HEVC_SPS *sps;
	HEVC_VPS *vps;
	HEVC_ProfileTierLevel ptl;
	Bool multiLayerExtSpsFlag;
	u8 sps_ext_or_max_sub_layers_minus1, max_sub_layers_minus1;

	if (vui_flag_pos) *vui_flag_pos = 0;

	//nalu header already parsed
	vps_id = gf_bs_read_int_log(bs, 4, "vps_id");
	if ((vps_id<0) || (vps_id >= 16)) {
		return -1;
	}
	memset(&ptl, 0, sizeof(ptl));
	max_sub_layers_minus1 = 0;
	sps_ext_or_max_sub_layers_minus1 = 0;
	if (layer_id == 0)
		max_sub_layers_minus1 = gf_bs_read_int_log(bs, 3, "max_sub_layers_minus1");
	else
		sps_ext_or_max_sub_layers_minus1 = gf_bs_read_int_log(bs, 3, "sps_ext_or_max_sub_layers_minus1");
	multiLayerExtSpsFlag = (layer_id != 0) && (sps_ext_or_max_sub_layers_minus1 == 7);
	if (!multiLayerExtSpsFlag) {
		gf_bs_read_int_log(bs, 1, "temporal_id_nesting_flag");
		hevc_profile_tier_level(bs, 1, max_sub_layers_minus1, &ptl, 0);
	}

	sps_id = gf_bs_read_ue_log(bs, "sps_id");
	if ((sps_id < 0) || (sps_id >= 16)) {
		return -1;
	}
	if (!hevc) return sps_id;

	sps = &hevc->sps[sps_id];
	if (!sps->state) {
		sps->state = 1;
		sps->id = sps_id;
		sps->vps_id = vps_id;
	}
	sps->ptl = ptl;
	vps = &hevc->vps[vps_id];
	sps->max_sub_layers_minus1 = max_sub_layers_minus1;
	sps->sps_ext_or_max_sub_layers_minus1 = sps_ext_or_max_sub_layers_minus1;

	/* default values */
	sps->colour_primaries = 2;
	sps->transfer_characteristic = 2;
	sps->matrix_coeffs = 2;

	//sps_rep_format_idx = 0;
	if (multiLayerExtSpsFlag) {
		sps->update_rep_format_flag = gf_bs_read_int_log(bs, 1, "update_rep_format_flag");
		if (sps->update_rep_format_flag) {
			sps->rep_format_idx = gf_bs_read_int_log(bs, 8, "rep_format_idx");
		} else {
			if (layer_id<MAX_LHVC_LAYERS) {
				u32 idx = vps->layer_id_in_vps[layer_id];
				if (idx<=15)
					sps->rep_format_idx = vps->rep_format_idx[idx];
			}
		}
		if (sps->rep_format_idx>15) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_CODING, ("[HEVC] Invalid rep_format_idx index %d\n", sps->rep_format_idx));
			HEVC_SPS_BROKEN
		}
		sps->width = vps->rep_formats[sps->rep_format_idx].pic_width_luma_samples;
		sps->height = vps->rep_formats[sps->rep_format_idx].pic_height_luma_samples;
		sps->chroma_format_idc = vps->rep_formats[sps->rep_format_idx].chroma_format_idc;
		sps->bit_depth_luma = vps->rep_formats[sps->rep_format_idx].bit_depth_luma;
		sps->bit_depth_chroma = vps->rep_formats[sps->rep_format_idx].bit_depth_chroma;
		sps->separate_colour_plane_flag = vps->rep_formats[sps->rep_format_idx].separate_colour_plane_flag;

		//TODO this is crude ...
		sps->ptl = vps->ext_ptl[0];
	}
	else {
		sps->chroma_format_idc = gf_bs_read_ue_log(bs, "chroma_format_idc");
		if (sps->chroma_format_idc == 3)
			sps->separate_colour_plane_flag = gf_bs_read_int_log(bs, 1, "separate_colour_plane_flag");
		sps->width = gf_bs_read_ue_log(bs, "width");
		sps->height = gf_bs_read_ue_log(bs, "height");
		if ((sps->cw_flag = gf_bs_read_int_log(bs, 1, "conformance_window_flag"))) {
			u32 SubWidthC, SubHeightC;

			if (sps->chroma_format_idc == 1) {
				SubWidthC = SubHeightC = 2;
			}
			else if (sps->chroma_format_idc == 2) {
				SubWidthC = 2;
				SubHeightC = 1;
			}
			else {
				SubWidthC = SubHeightC = 1;
			}

			sps->cw_left = gf_bs_read_ue_log(bs, "conformance_window_left");
			sps->cw_right = gf_bs_read_ue_log(bs, "conformance_window_right");
			sps->cw_top = gf_bs_read_ue_log(bs, "conformance_window_top");
			sps->cw_bottom = gf_bs_read_ue_log(bs, "conformance_window_bottom");

			sps->width -= SubWidthC * (sps->cw_left + sps->cw_right);
			sps->height -= SubHeightC * (sps->cw_top + sps->cw_bottom);
		}
		sps->bit_depth_luma = 8 + gf_bs_read_ue_log(bs, "bit_depth_luma_minus8");
		sps->bit_depth_chroma = 8 + gf_bs_read_ue_log(bs, "bit_depth_chroma_minus8");
	}

	sps->log2_max_pic_order_cnt_lsb = 4 + gf_bs_read_ue_log(bs, "log2_max_pic_order_cnt_lsb_minus4");
	if (sps->log2_max_pic_order_cnt_lsb>16) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_CODING, ("[HEVC] Invalid log2_max_pic_order_cnt_lsb_minus4 %d, max shall be 12\n", sps->log2_max_pic_order_cnt_lsb-4));
		HEVC_SPS_BROKEN
	}

	if (!multiLayerExtSpsFlag) {
		sps->sub_layer_ordering_info_present_flag = gf_bs_read_int_log(bs, 1, "sub_layer_ordering_info_present_flag");
		for (i = sps->sub_layer_ordering_info_present_flag ? 0 : sps->max_sub_layers_minus1; i <= sps->max_sub_layers_minus1; i++) {
			gf_bs_read_ue_log_idx(bs, "max_dec_pic_buffering", i);
			gf_bs_read_ue_log_idx(bs, "num_reorder_pics", i);
			gf_bs_read_ue_log_idx(bs, "max_latency_increase", i);
		}
	}

	sps->log2_min_luma_coding_block_size = 3 + gf_bs_read_ue_log(bs, "log2_min_luma_coding_block_size_minus3");
	sps->log2_diff_max_min_luma_coding_block_size = gf_bs_read_ue_log(bs, "log2_diff_max_min_luma_coding_block_size");
	//we allow more than in max profile, but make sure we don't overflow max CU W/H compute below
	if (sps->log2_min_luma_coding_block_size + sps->log2_diff_max_min_luma_coding_block_size >= 30) {
		HEVC_SPS_BROKEN
	}
	sps->max_CU_width = (1 << (sps->log2_min_luma_coding_block_size + sps->log2_diff_max_min_luma_coding_block_size));
	sps->max_CU_height = (1 << (sps->log2_min_luma_coding_block_size + sps->log2_diff_max_min_luma_coding_block_size));

	sps->log2_min_transform_block_size = 2 + gf_bs_read_ue_log(bs, "log2_min_transform_block_size_minus2");
	sps->log2_max_transform_block_size = sps->log2_min_transform_block_size  + gf_bs_read_ue_log(bs, "log2_max_transform_block_size");
	//The CVS shall not contain data that result in MinTbLog2SizeY greater than or equal to MinCbLog2SizeY
	if (sps->log2_min_transform_block_size/*MinTbLog2SizeY*/ >= sps->log2_min_luma_coding_block_size/*MinCbLog2SizeY*/)
		HEVC_SPS_BROKEN
	//The CVS shall not contain data that result in MaxTbLog2SizeY greater than Min( CtbLog2SizeY, 5 ).
	if (sps->log2_max_transform_block_size /*MaxTbLog2SizeY*/ > MIN( sps->log2_min_luma_coding_block_size+ sps->log2_diff_max_min_luma_coding_block_size /*CtbLog2SizeY*/, 5 ))
		HEVC_SPS_BROKEN

	depth = 0;
	sps->max_transform_hierarchy_depth_inter = gf_bs_read_ue_log(bs, "max_transform_hierarchy_depth_inter");
	sps->max_transform_hierarchy_depth_intra = gf_bs_read_ue_log(bs, "max_transform_hierarchy_depth_intra");
	while ((u32)(sps->max_CU_width >> sps->log2_diff_max_min_luma_coding_block_size) > (u32)(1 << (sps->log2_min_transform_block_size + depth)))
	{
		depth++;
	}
	sps->max_CU_depth = sps->log2_diff_max_min_luma_coding_block_size + depth;

	nb_CTUs = ((sps->width + sps->max_CU_width - 1) / sps->max_CU_width) * ((sps->height + sps->max_CU_height - 1) / sps->max_CU_height);
	sps->bitsSliceSegmentAddress = 0;
	while (nb_CTUs > (u32)(1 << sps->bitsSliceSegmentAddress)) {
		sps->bitsSliceSegmentAddress++;
		if (sps->bitsSliceSegmentAddress==31)
			HEVC_SPS_BROKEN
	}

	sps->scaling_list_enable_flag = gf_bs_read_int_log(bs, 1, "scaling_list_enable_flag");
	if (sps->scaling_list_enable_flag) {
		sps->infer_scaling_list_flag = 0;
		sps->scaling_list_ref_layer_id = 0;
		if (multiLayerExtSpsFlag) {
			sps->infer_scaling_list_flag = gf_bs_read_int_log(bs, 1, "infer_scaling_list_flag");
		}
		if (sps->infer_scaling_list_flag) {
			sps->scaling_list_ref_layer_id = gf_bs_read_int_log(bs, 6, "scaling_list_ref_layer_id");
		}
		else {
			sps->scaling_list_data_present_flag = gf_bs_read_int_log(bs, 1, "scaling_list_data_present_flag");
			if (sps->scaling_list_data_present_flag) {
				hevc_scaling_list_data(bs);
			}
		}
	}
	sps->asymmetric_motion_partitions_enabled_flag = gf_bs_read_int_log(bs, 1, "asymmetric_motion_partitions_enabled_flag");
	sps->sample_adaptive_offset_enabled_flag = gf_bs_read_int_log(bs, 1, "sample_adaptive_offset_enabled_flag");
	if ( (sps->pcm_enabled_flag = gf_bs_read_int_log(bs, 1, "pcm_enabled_flag")) ) {
		sps->pcm_sample_bit_depth_luma_minus1 = gf_bs_read_int_log(bs, 4, "pcm_sample_bit_depth_luma_minus1");
		sps->pcm_sample_bit_depth_chroma_minus1 = gf_bs_read_int_log(bs, 4, "pcm_sample_bit_depth_chroma_minus1");
		sps->log2_min_pcm_luma_coding_block_size_minus3 = gf_bs_read_ue_log(bs, "log2_min_pcm_luma_coding_block_size_minus3");
		sps->log2_diff_max_min_pcm_luma_coding_block_size = gf_bs_read_ue_log(bs, "log2_diff_max_min_pcm_luma_coding_block_size");
		sps->pcm_loop_filter_disable_flag = gf_bs_read_int_log(bs, 1, "pcm_loop_filter_disable_flag");
	}
	sps->num_short_term_ref_pic_sets = gf_bs_read_ue_log(bs, "num_short_term_ref_pic_sets");
	if (sps->num_short_term_ref_pic_sets > 64) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_CODING, ("[HEVC] Invalid number of short term reference picture sets %d\n", sps->num_short_term_ref_pic_sets));
		HEVC_SPS_BROKEN
	}

	for (i = 0; i < sps->num_short_term_ref_pic_sets; i++) {
		Bool ret = hevc_parse_short_term_ref_pic_set(bs, sps, i);
		/*cannot parse short_term_ref_pic_set, skip VUI parsing*/
		if (!ret) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_CODING, ("[HEVC] Invalid short_term_ref_pic_set\n"));
			HEVC_SPS_BROKEN
		}
	}
	sps->long_term_ref_pics_present_flag = gf_bs_read_int_log(bs, 1, "long_term_ref_pics_present_flag");
	if (sps->long_term_ref_pics_present_flag) {
		sps->num_long_term_ref_pic_sps = gf_bs_read_ue_log(bs, "num_long_term_ref_pic_sps");
		for (i = 0; i < sps->num_long_term_ref_pic_sps; i++) {
			gf_bs_read_int_log_idx(bs, sps->log2_max_pic_order_cnt_lsb, "lt_ref_pic_poc_lsb_sps", i);
			gf_bs_read_int_log_idx(bs, 1, "used_by_curr_pic_lt_sps_flag", i);
		}
	}
	sps->temporal_mvp_enable_flag = gf_bs_read_int_log(bs, 1, "temporal_mvp_enable_flag");
	sps->strong_intra_smoothing_enable_flag = gf_bs_read_int_log(bs, 1, "strong_intra_smoothing_enable_flag");

	if (vui_flag_pos)
		*vui_flag_pos = (u32)gf_bs_get_bit_offset(bs);

	if ((sps->vui_parameters_present_flag = gf_bs_read_int_log(bs, 1, "vui_parameters_present_flag")) ) {
		sps->aspect_ratio_info_present_flag = gf_bs_read_int_log(bs, 1, "aspect_ratio_info_present_flag");
		if (sps->aspect_ratio_info_present_flag) {
			sps->sar_idc = gf_bs_read_int_log(bs, 8, "aspect_ratio_idc");
			if (sps->sar_idc == 255) {
				sps->sar_width = gf_bs_read_int_log(bs, 16, "aspect_ratio_width");
				sps->sar_height = gf_bs_read_int_log(bs, 16, "aspect_ratio_height");
			}
			else if (sps->sar_idc < 17) {
				sps->sar_width = hevc_sar[sps->sar_idc].w;
				sps->sar_height = hevc_sar[sps->sar_idc].h;
			}
		}

		if ((sps->overscan_info_present = gf_bs_read_int_log(bs, 1, "overscan_info_present")))
			sps->overscan_appropriate = gf_bs_read_int_log(bs, 1, "overscan_appropriate");

		sps->video_signal_type_present_flag = gf_bs_read_int_log(bs, 1, "video_signal_type_present_flag");
		if (sps->video_signal_type_present_flag) {
			sps->video_format = gf_bs_read_int_log(bs, 3, "video_format");
			sps->video_full_range_flag = gf_bs_read_int_log(bs, 1, "video_full_range_flag");
			if ((sps->colour_description_present_flag = gf_bs_read_int_log(bs, 1, "colour_description_present_flag"))) {
				sps->colour_primaries = gf_bs_read_int_log(bs, 8, "colour_primaries");
				sps->transfer_characteristic = gf_bs_read_int_log(bs, 8, "transfer_characteristic");
				sps->matrix_coeffs = gf_bs_read_int_log(bs, 8, "matrix_coefficients");
			}
		}

		if ((sps->chroma_loc_info_present_flag = gf_bs_read_int_log(bs, 1, "chroma_loc_info_present_flag"))) {
			sps->chroma_sample_loc_type_top_field = gf_bs_read_ue_log(bs, "chroma_sample_loc_type_top_field");
			sps->chroma_sample_loc_type_bottom_field = gf_bs_read_ue_log(bs, "chroma_sample_loc_type_bottom_field");
		}

		sps->neutral_chroma_indication_flag = gf_bs_read_int_log(bs, 1, "neutral_chroma_indication_flag");
		sps->field_seq_flag = gf_bs_read_int_log(bs, 1, "field_seq_flag");
		sps->frame_field_info_present_flag = gf_bs_read_int_log(bs, 1, "frame_field_info_present_flag");

		if ((sps->default_display_window_flag = gf_bs_read_int_log(bs, 1, "default_display_window_flag"))) {
			sps->left_offset = gf_bs_read_ue_log(bs, "display_window_left_offset");
			sps->right_offset = gf_bs_read_ue_log(bs, "display_window_right_offset");
			sps->top_offset = gf_bs_read_ue_log(bs, "display_window_top_offset");
			sps->bottom_offset = gf_bs_read_ue_log(bs, "display_window_bottom_offset");
		}

		sps->has_timing_info = gf_bs_read_int_log(bs, 1, "has_timing_info");
		if (sps->has_timing_info) {
			sps->num_units_in_tick = gf_bs_read_int_log(bs, 32, "num_units_in_tick");
			sps->time_scale = gf_bs_read_int_log(bs, 32, "time_scale");
			sps->poc_proportional_to_timing_flag = gf_bs_read_int_log(bs, 1, "poc_proportional_to_timing_flag");
			if (sps->poc_proportional_to_timing_flag)
				sps->num_ticks_poc_diff_one_minus1 = gf_bs_read_ue_log(bs, "num_ticks_poc_diff_one_minus1");
			if ((sps->hrd_parameters_present_flag = gf_bs_read_int_log(bs, 1, "hrd_parameters_present_flag"))) {
				//				GF_LOG(GF_LOG_INFO, GF_LOG_CODING, ("[HEVC] HRD param parsing not implemented\n"));
				return sps_id;
			}
		}

		if (gf_bs_read_int_log(bs, 1, "bitstream_restriction_flag")) {
			gf_bs_read_int_log(bs, 1, "tiles_fixed_structure_flag");
			gf_bs_read_int_log(bs, 1, "motion_vectors_over_pic_boundaries_flag");
			gf_bs_read_int_log(bs, 1, "restricted_ref_pic_lists_flag");
			gf_bs_read_ue_log(bs, "min_spatial_segmentation_idc");
			gf_bs_read_ue_log(bs, "max_bytes_per_pic_denom");
			gf_bs_read_ue_log(bs, "max_bits_per_min_cu_denom");
			gf_bs_read_ue_log(bs, "log2_max_mv_length_horizontal");
			gf_bs_read_ue_log(bs, "log2_max_mv_length_vertical");
		}
	}

	if (gf_bs_read_int_log(bs, 1, "sps_extension_flag")) {
#if 0
		while (gf_bs_available(bs)) {
			/*sps_extension_data_flag */ gf_bs_read_int(bs, 1);
		}
#endif

	}
	if (gf_bs_is_overflow(bs))
		HEVC_SPS_BROKEN
	return sps_id;
}

GF_EXPORT
s32 gf_hevc_read_sps_ex(char *data, u32 size, HEVCState *hevc, u32 *vui_flag_pos)
{
	GF_BitStream *bs;
	s32 sps_id = -1;
	u8 layer_id;

	if (vui_flag_pos) *vui_flag_pos = 0;

	bs = gf_bs_new(data, size, GF_BITSTREAM_READ);
	if (!bs) goto exit;
	gf_bs_enable_emulation_byte_removal(bs, GF_TRUE);

	if (!hevc_parse_nal_header(bs, NULL, NULL, &layer_id)) goto exit;
	sps_id = gf_hevc_read_sps_bs_internal(bs, hevc, layer_id, vui_flag_pos);

exit:
	if (bs) gf_bs_del(bs);
	return sps_id;
}

GF_EXPORT
s32 gf_hevc_read_sps(u8 *data, u32 size, HEVCState *hevc)
{
	return gf_hevc_read_sps_ex(data, size, hevc, NULL);
}

GF_EXPORT
s32 gf_hevc_read_sps_bs(GF_BitStream *bs, HEVCState *hevc)
{
	u8 layer_id;
	if (!bs || !hevc) return -1;
	gf_bs_enable_emulation_byte_removal(bs, GF_TRUE);
	if (!hevc_parse_nal_header(bs, NULL, NULL, &layer_id)) return -1;
	return gf_hevc_read_sps_bs_internal(bs, hevc, layer_id, NULL);
}

#define HEVC_PPS_BROKEN {\
	memset(pps, 0, sizeof(HEVC_PPS)); \
	return -1;\
	}

static s32 gf_hevc_read_pps_bs_internal(GF_BitStream *bs, HEVCState *hevc)
{
	u32 i;
	s32 pps_id;
	HEVC_PPS *pps;

	//NAL header already read
	pps_id = gf_bs_read_ue_log(bs, "pps_id");

	if ((pps_id < 0) || (pps_id >= 64)) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_CODING, ("[HEVC] wrong PPS ID %d in PPS\n", pps_id));
		return -1;
	}
	pps = &hevc->pps[pps_id];

	if (!pps->state) {
		pps->id = pps_id;
		pps->state = 1;
	}
	pps->sps_id = gf_bs_read_ue_log(bs, "sps_id");
	if (((s32)pps->sps_id<0) || (pps->sps_id >= 16)) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_CODING, ("[HEVC] wrong SPS ID %d in PPS\n", pps->sps_id));
		HEVC_PPS_BROKEN
	}
	hevc->sps_active_idx = pps->sps_id; /*set active sps*/
	pps->dependent_slice_segments_enabled_flag = gf_bs_read_int_log(bs, 1, "dependent_slice_segments_enabled_flag");

	pps->output_flag_present_flag = gf_bs_read_int_log(bs, 1, "output_flag_present_flag");
	pps->num_extra_slice_header_bits = gf_bs_read_int_log(bs, 3, "num_extra_slice_header_bits");
	pps->sign_data_hiding_flag = gf_bs_read_int_log(bs, 1, "sign_data_hiding_flag");
	pps->cabac_init_present_flag = gf_bs_read_int_log(bs, 1, "cabac_init_present_flag");
	pps->num_ref_idx_l0_default_active = 1 + gf_bs_read_ue_log(bs, "num_ref_idx_l0_default_active");
	pps->num_ref_idx_l1_default_active = 1 + gf_bs_read_ue_log(bs, "num_ref_idx_l1_default_active");
	pps->pic_init_qp_minus26 = gf_bs_read_se_log(bs, "pic_init_qp_minus26");
	pps->constrained_intra_pred_flag = gf_bs_read_int_log(bs, 1, "constrained_intra_pred_flag");
	pps->transform_skip_enabled_flag = gf_bs_read_int_log(bs, 1, "transform_skip_enabled_flag");
	if ((pps->cu_qp_delta_enabled_flag = gf_bs_read_int_log(bs, 1, "cu_qp_delta_enabled_flag")))
		pps->diff_cu_qp_delta_depth = gf_bs_read_ue_log(bs, "diff_cu_qp_delta_depth");

	pps->pic_cb_qp_offset = gf_bs_read_se_log(bs, "pic_cb_qp_offset");
	pps->pic_cr_qp_offset = gf_bs_read_se_log(bs, "pic_cr_qp_offset");
	pps->slice_chroma_qp_offsets_present_flag = gf_bs_read_int_log(bs, 1, "slice_chroma_qp_offsets_present_flag");
	pps->weighted_pred_flag = gf_bs_read_int_log(bs, 1, "weighted_pred_flag");
	pps->weighted_bipred_flag = gf_bs_read_int_log(bs, 1, "weighted_bipred_flag");
	pps->transquant_bypass_enable_flag = gf_bs_read_int_log(bs, 1, "transquant_bypass_enable_flag");
	pps->tiles_enabled_flag = gf_bs_read_int_log(bs, 1, "tiles_enabled_flag");
	pps->entropy_coding_sync_enabled_flag = gf_bs_read_int_log(bs, 1, "entropy_coding_sync_enabled_flag");
	if (pps->tiles_enabled_flag) {
		pps->num_tile_columns = 1 + gf_bs_read_ue_log(bs, "num_tile_columns_minus1");
		if (pps->num_tile_columns > 22) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_CODING, ("[HEVC] Invalid num_tile_columns %u\n", pps->num_tile_columns));
			HEVC_PPS_BROKEN
		}
		pps->num_tile_rows = 1 + gf_bs_read_ue_log(bs, "num_tile_rows_minus1");
		if (pps->num_tile_rows > 20) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_CODING, ("[HEVC] Invalid num_tile_rows %u\n", pps->num_tile_rows));
			HEVC_PPS_BROKEN
		}
		pps->uniform_spacing_flag = gf_bs_read_int_log(bs, 1, "uniform_spacing_flag");
		if (!pps->uniform_spacing_flag) {
			for (i = 0; i < pps->num_tile_columns - 1; i++) {
				pps->column_width[i] = 1 + gf_bs_read_ue_log_idx(bs, "column_width_minus1", i);
			}
			for (i = 0; i < pps->num_tile_rows - 1; i++) {
				pps->row_height[i] = 1 + gf_bs_read_ue_log_idx(bs, "row_height_minus1", i);
			}
		}
		pps->loop_filter_across_tiles_enabled_flag = gf_bs_read_int_log(bs, 1, "loop_filter_across_tiles_enabled_flag");
	}
	pps->loop_filter_across_slices_enabled_flag = gf_bs_read_int_log(bs, 1, "loop_filter_across_slices_enabled_flag");
	if ((pps->deblocking_filter_control_present_flag = gf_bs_read_int_log(bs, 1, "deblocking_filter_control_present_flag"))) {
		pps->deblocking_filter_override_enabled_flag = gf_bs_read_int_log(bs, 1, "deblocking_filter_override_enabled_flag");
		if (! (pps->pic_disable_deblocking_filter_flag = gf_bs_read_int_log(bs, 1, "pic_disable_deblocking_filter_flag"))) {
			pps->beta_offset_div2 = gf_bs_read_se_log(bs, "beta_offset_div2");
			pps->tc_offset_div2 = gf_bs_read_se_log(bs, "tc_offset_div2");
		}
	}
	if ((pps->pic_scaling_list_data_present_flag = gf_bs_read_int_log(bs, 1, "pic_scaling_list_data_present_flag"))) {
		hevc_scaling_list_data(bs);
	}
	pps->lists_modification_present_flag = gf_bs_read_int_log(bs, 1, "lists_modification_present_flag");
	pps->log2_parallel_merge_level_minus2 = gf_bs_read_ue_log(bs, "log2_parallel_merge_level_minus2");
	pps->slice_segment_header_extension_present_flag = gf_bs_read_int_log(bs, 1, "slice_segment_header_extension_present_flag");
	if (gf_bs_read_int_log(bs, 1, "pps_extension_flag")) {
#if 0
		while (gf_bs_available(bs)) {
			/*pps_extension_data_flag */ gf_bs_read_int(bs, 1);
		}
#endif

	}

	if (gf_bs_is_overflow(bs))
		HEVC_PPS_BROKEN
	return pps_id;
}


GF_EXPORT
s32 gf_hevc_read_pps(u8 *data, u32 size, HEVCState *hevc)
{
	GF_BitStream *bs;
	s32 pps_id = -1;

	bs = gf_bs_new(data, size, GF_BITSTREAM_READ);
	if (!bs) goto exit;
	gf_bs_enable_emulation_byte_removal(bs, GF_TRUE);

	if (!hevc_parse_nal_header(bs, NULL, NULL, NULL)) goto exit;

	pps_id = gf_hevc_read_pps_bs_internal(bs, hevc);

exit:
	if (bs) gf_bs_del(bs);
	return pps_id;
}

GF_EXPORT
s32 gf_hevc_read_pps_bs(GF_BitStream *bs, HEVCState *hevc)
{
	if (!bs || !hevc) return -1;
	gf_bs_enable_emulation_byte_removal(bs, GF_TRUE);
	if (!hevc_parse_nal_header(bs, NULL, NULL, NULL)) return -1;
	return gf_hevc_read_pps_bs_internal(bs, hevc);
}

GF_EXPORT
s32 gf_hevc_parse_nalu_bs(GF_BitStream *bs, HEVCState *hevc, u8 *nal_unit_type, u8 *temporal_id, u8 *layer_id)
{
	Bool is_slice = GF_FALSE;
	s32 ret = -1;
	HEVCSliceInfo n_state;

	gf_bs_enable_emulation_byte_removal(bs, GF_TRUE);
	if (gf_bs_available(bs)<2) return -1;

	gf_bs_mark_overflow(bs, GF_TRUE);

	memcpy(&n_state, &hevc->s_info, sizeof(HEVCSliceInfo));
	if (!hevc_parse_nal_header(bs, nal_unit_type, temporal_id, layer_id)) return -1;

	n_state.nal_unit_type = *nal_unit_type;

	switch (n_state.nal_unit_type) {
	case GF_HEVC_NALU_ACCESS_UNIT:
	case GF_HEVC_NALU_END_OF_SEQ:
	case GF_HEVC_NALU_END_OF_STREAM:
		ret = 1;
		break;

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
		is_slice = GF_TRUE;
		/* slice - read the info and compare.*/
		ret = hevc_parse_slice_segment(bs, hevc, &n_state);
		if (ret < 0) return ret;

		hevc_compute_poc(&n_state);

		ret = 0;

		if (hevc->s_info.poc != n_state.poc) {
			ret = 1;
			break;
		}
		if (n_state.first_slice_segment_in_pic_flag) {
			if (!(*layer_id) || (n_state.prev_layer_id_plus1 && ((*layer_id) <= n_state.prev_layer_id_plus1 - 1))) {
				ret = 1;
				break;
			}
		}
		break;
	case GF_HEVC_NALU_SEQ_PARAM:
		hevc->last_parsed_sps_id = gf_hevc_read_sps_bs_internal(bs, hevc, *layer_id, NULL);
		ret = (hevc->last_parsed_sps_id>=0) ? 0 : -1;
		break;
	case GF_HEVC_NALU_PIC_PARAM:
		hevc->last_parsed_pps_id = gf_hevc_read_pps_bs_internal(bs, hevc);
		ret = (hevc->last_parsed_pps_id>=0) ? 0 : -1;
		break;
	case GF_HEVC_NALU_VID_PARAM:
		hevc->last_parsed_vps_id = gf_hevc_read_vps_bs_internal(bs, hevc, GF_FALSE);
		ret = (hevc->last_parsed_vps_id>=0) ? 0 : -1;
		break;
	default:
		ret = 0;
		break;
	}

	if (gf_bs_is_overflow(bs))
		ret = -1;

	/* save _prev values */
	if ((ret>0) && hevc->s_info.sps) {
		n_state.frame_num_offset_prev = hevc->s_info.frame_num_offset;
		n_state.frame_num_prev = hevc->s_info.frame_num;

		n_state.poc_lsb_prev = hevc->s_info.poc_lsb;
		n_state.poc_msb_prev = hevc->s_info.poc_msb;
		if (is_slice)
			n_state.prev_layer_id_plus1 = *layer_id + 1;
	}
	if (is_slice) hevc_compute_poc(&n_state);
	memcpy(&hevc->s_info, &n_state, sizeof(HEVCSliceInfo));

	return ret;
}

GF_EXPORT
s32 gf_hevc_parse_nalu(u8 *data, u32 size, HEVCState *hevc, u8 *nal_unit_type, u8 *temporal_id, u8 *layer_id)
{
	GF_BitStream *bs = NULL;
	s32 ret = -1;

	if (size<2) return -1;

	if (!hevc) {
		if (nal_unit_type) (*nal_unit_type) = (data[0] & 0x7E) >> 1;
		if (layer_id) {
			u8 id = data[0] & 1;
			id <<= 5;
			id |= (data[1] >> 3) & 0x1F;
			(*layer_id) = id;
		}
		if (temporal_id) (*temporal_id) = (data[1] & 0x7);
		return -1;
	}

	bs = gf_bs_new(data, size, GF_BITSTREAM_READ);
	if (!bs) return -1;
	gf_bs_enable_emulation_byte_removal(bs, GF_TRUE);

	ret = gf_hevc_parse_nalu_bs(bs, hevc, nal_unit_type, temporal_id, layer_id);

	gf_bs_del(bs);
	return ret;
}

GF_EXPORT
GF_Err gf_hevc_change_vui(GF_HEVCConfig *hvcc, GF_VUIInfo *vui_info)
{
	GF_BitStream *orig, *mod;
	HEVCState hevc;
	u32 i, bit_offset, flag;
	s32 idx;
	GF_NALUFFParamArray *spss;
	GF_NALUFFParam *slc;
	orig = NULL;

	memset(&hevc, 0, sizeof(HEVCState));
	hevc.sps_active_idx = -1;

	i = 0;
	spss = NULL;
	while ((spss = (GF_NALUFFParamArray *)gf_list_enum(hvcc->param_array, &i))) {
		if (spss->type == GF_HEVC_NALU_SEQ_PARAM)
			break;
		spss = NULL;
	}
	if (!spss) return GF_NON_COMPLIANT_BITSTREAM;

	i = 0;
	while ((slc = (GF_NALUFFParam *)gf_list_enum(spss->nalus, &i))) {
		u8 *no_emulation_buf;
		u32 no_emulation_buf_size, emulation_bytes;

		/*SPS may still contains emulation bytes*/
		no_emulation_buf = gf_malloc((slc->size) * sizeof(char));
		no_emulation_buf_size = gf_media_nalu_remove_emulation_bytes(slc->data, no_emulation_buf, slc->size);

		idx = gf_hevc_read_sps_ex(no_emulation_buf, no_emulation_buf_size, &hevc, &bit_offset);
		if (idx < 0) {
			if (orig)
				gf_bs_del(orig);
			gf_free(no_emulation_buf);
			continue;
		}

		orig = gf_bs_new(no_emulation_buf, no_emulation_buf_size, GF_BITSTREAM_READ);
		mod = gf_bs_new(NULL, 0, GF_BITSTREAM_WRITE);

		/*copy over till vui flag*/
		while (bit_offset) {
			flag = gf_bs_read_int(orig, 1);
			gf_bs_write_int(mod, flag, 1);
			bit_offset--;
		}

		avc_hevc_vvc_rewrite_vui(vui_info, orig, mod, GF_CODECID_HEVC);

		/*finally copy over remaining*/
		while (gf_bs_bits_available(orig)) {
			flag = gf_bs_read_int(orig, 1);
			gf_bs_write_int(mod, flag, 1);
		}
		gf_bs_del(orig);
		orig = NULL;
		gf_free(no_emulation_buf);

		/*set anti-emulation*/
		gf_bs_get_content(mod, &no_emulation_buf, &no_emulation_buf_size);
		emulation_bytes = gf_media_nalu_emulation_bytes_add_count(no_emulation_buf, no_emulation_buf_size);
		if (no_emulation_buf_size + emulation_bytes > slc->size)
			slc->data = (char*)gf_realloc(slc->data, no_emulation_buf_size + emulation_bytes);

		slc->size = gf_media_nalu_add_emulation_bytes(no_emulation_buf, slc->data, no_emulation_buf_size);

		gf_bs_del(mod);
		gf_free(no_emulation_buf);
	}
	return GF_OK;
}


GF_EXPORT
GF_Err gf_hevc_change_par(GF_HEVCConfig *hvcc, s32 ar_n, s32 ar_d)
{
	GF_VUIInfo vuii;
	memset(&vuii, 0, sizeof(GF_VUIInfo));
	vuii.ar_num = ar_n;
	vuii.ar_den = ar_d;
	vuii.fullrange = -1;
	vuii.video_format = -1;
	vuii.color_prim = -1;
	vuii.color_tfc = -1;
	vuii.color_matrix = -1;
	return gf_hevc_change_vui(hvcc, &vuii);
}

GF_EXPORT
GF_Err gf_hevc_change_color(GF_HEVCConfig *hvcc, s32 fullrange, s32 vidformat, s32 colorprim, s32 transfer, s32 colmatrix)
{
	GF_VUIInfo vuii;
	memset(&vuii, 0, sizeof(GF_VUIInfo));
	vuii.ar_num = -1;
	vuii.ar_den = -1;
	vuii.fullrange = fullrange;
	vuii.video_format = vidformat;
	vuii.color_prim = colorprim;
	vuii.color_tfc = transfer;
	vuii.color_matrix = colmatrix;
	return gf_hevc_change_vui(hvcc, &vuii);
}


GF_EXPORT
GF_Err gf_hevc_get_sps_info_with_state(HEVCState *hevc, u8 *sps_data, u32 sps_size, u32 *sps_id, u32 *width, u32 *height, s32 *par_n, s32 *par_d)
{
	s32 idx;
	idx = gf_hevc_read_sps(sps_data, sps_size, hevc);
	if (idx < 0) {
		return GF_NON_COMPLIANT_BITSTREAM;
	}
	if (sps_id) *sps_id = idx;

	if (width) *width = hevc->sps[idx].width;
	if (height) *height = hevc->sps[idx].height;
	if (par_n) *par_n = hevc->sps[idx].aspect_ratio_info_present_flag ? hevc->sps[idx].sar_width : (u32)-1;
	if (par_d) *par_d = hevc->sps[idx].aspect_ratio_info_present_flag ? hevc->sps[idx].sar_height : (u32)-1;
	return GF_OK;
}

GF_EXPORT
GF_Err gf_hevc_get_sps_info(u8 *sps_data, u32 sps_size, u32 *sps_id, u32 *width, u32 *height, s32 *par_n, s32 *par_d)
{
	HEVCState hevc;
	memset(&hevc, 0, sizeof(HEVCState));
	hevc.sps_active_idx = -1;
	return gf_hevc_get_sps_info_with_state(&hevc, sps_data, sps_size, sps_id, width, height, par_n, par_d);
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
	if (gf_bs_available(bs)<6) return GF_FALSE;
	u64 pos = gf_bs_get_position(bs);
	u64 end = gf_bs_get_size(bs);

	pos += 1;
	b1 = gf_bs_read_u8(bs);
	while (pos + 1 <= end) {
		u8 b2 = gf_bs_read_u8(bs);
		if ((b1 == 0x0b) && (b2 == 0x77)) {
			gf_bs_seek(bs, pos - 1);
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

static const u32 ac3_mod_to_total_chans[] = {
	2, 1, 2, 3, 3, 4, 4, 5
};

static const u32 ac3_mod_to_surround_chans[] = {
	0, 0, 0, 0, 1, 1, 2, 2
};

GF_EXPORT
u32 gf_ac3_get_total_channels(u32 acmod)
{
	u32 nb_ch;
	nb_ch = ac3_mod_to_total_chans[acmod];
	return nb_ch;
}

GF_EXPORT
u32 gf_ac3_get_surround_channels(u32 acmod)
{
	u32 nb_ch;
	nb_ch = ac3_mod_to_surround_chans[acmod];
	return nb_ch;
}

GF_EXPORT
u32 gf_ac3_get_bitrate(u32 brcode)
{
	return ac3_sizecod_to_bitrate[brcode];
}

Bool gf_ac3_parser(u8 *buf, u32 buflen, u32 *pos, GF_AC3Config *hdr, Bool full_parse)
{
	GF_BitStream *bs;
	Bool ret;

	if (buflen < 6) return GF_FALSE;
	(*pos) = AC3_FindSyncCode(buf, buflen);
	if (*pos >= buflen) return GF_FALSE;

	bs = gf_bs_new((const char*)(buf + *pos), buflen, GF_BITSTREAM_READ);
	ret = gf_ac3_parser_bs(bs, hdr, full_parse);
	gf_bs_del(bs);

	return ret;
}

GF_EXPORT
Bool gf_ac3_parser_bs(GF_BitStream *bs, GF_AC3Config *hdr, Bool full_parse)
{
	u32 fscod, frmsizecod, bsid, ac3_mod, freq, framesize, bsmod, syncword;
	u64 pos;
	if (!hdr || !AC3_FindSyncCodeBS(bs)) return GF_FALSE;

	memset(hdr, 0, sizeof(GF_AC3Header));
	pos = gf_bs_get_position(bs);

	syncword = gf_bs_read_u16(bs);
	if (syncword != 0x0B77) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_CODING, ("[AC3] Wrong sync word detected (0x%X - expecting 0x0B77).\n", syncword));
		return GF_FALSE;
	}
	gf_bs_read_int_log(bs, 16, "crc1");
	fscod = gf_bs_read_int_log(bs, 2, "fscod");
	frmsizecod = gf_bs_read_int_log(bs, 6, "frmsizecod");
	bsid = gf_bs_read_int_log(bs, 5, "bsid");
	bsmod = gf_bs_read_int_log(bs, 3, "bsmod");
	ac3_mod = gf_bs_read_int_log(bs, 3, "ac3_mod");

	if (frmsizecod >= 2 * sizeof(ac3_sizecod_to_bitrate) / sizeof(u32))
		return GF_FALSE;

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
	hdr->nb_streams = 1;

	if (full_parse) {
		hdr->streams[0].bsid = bsid;
		hdr->streams[0].bsmod = bsmod;
		hdr->streams[0].acmod = ac3_mod;
		hdr->streams[0].lfon = 0;
		hdr->streams[0].fscod = fscod;
		hdr->brcode = frmsizecod / 2;
	}
	if (ac3_mod >= 2 * sizeof(ac3_mod_to_total_chans) / sizeof(u32))
		return GF_FALSE;

	hdr->streams[0].channels = ac3_mod_to_total_chans[ac3_mod];
	hdr->streams[0].surround_channels = ac3_mod_to_surround_chans[ac3_mod];
	if ((ac3_mod & 0x1) && (ac3_mod != 1)) gf_bs_read_int_log(bs, 2, "cmixlev");
	if (ac3_mod & 0x4) gf_bs_read_int_log(bs, 2, "surmixlev");
	if (ac3_mod == 0x2) gf_bs_read_int_log(bs, 2, "dsurmod");

	if (gf_bs_read_int_log(bs, 1, "lfeon")) {
		hdr->streams[0].channels += 1;
		hdr->streams[0].lfon = 1;
	}

	gf_bs_seek(bs, pos);

	return GF_TRUE;
}

GF_EXPORT
u32 gf_eac3_get_chan_loc_count(u32 chan_loc)
{
	u32 nb_ch=0;
	if (chan_loc & 1) nb_ch+=2; //Lc/Rc pair
	if (chan_loc & (1<<1)) nb_ch+=2; //Lrs/Rrs pair
	if (chan_loc & (1<<2)) nb_ch+=1; //Cs
	if (chan_loc & (1<<3)) nb_ch+=1; //Ts
	if (chan_loc & (1<<4)) nb_ch+=2; //Lsd/Rsd pair
	if (chan_loc & (1<<5)) nb_ch+=2; //Lw/Rw pair
	if (chan_loc & (1<<6)) nb_ch+=2; //Lvh/Rvh pair
	if (chan_loc & (1<<7)) nb_ch+=1; //Cvh
	if (chan_loc & (1<<8)) nb_ch+=1; //LFE2
	return nb_ch;
}

static u32 eac3_chanmap_to_chan_loc(u32 chan_map)
{
	u32 chan_loc = 0;
	if (chan_map & (1<<10)) chan_loc |= (1);
	if (chan_map & (1<<9)) chan_loc |= (1<<1);
	if (chan_map & (1<<8)) chan_loc |= (1<<2);
	if (chan_map & (1<<7)) chan_loc |= (1<<3);
	if (chan_map & (1<<6)) chan_loc |= (1<<4);
	if (chan_map & (1<<5)) chan_loc |= (1<<5);
	if (chan_map & (1<<4)) chan_loc |= (1<<6);
	if (chan_map & (1<<3)) chan_loc |= (1<<7);
	//Lts/Rts pair (LSB 2) is not exposed in chan_loc
	if (chan_map & (1<<1)) chan_loc |= (1<<8);
	return chan_loc;
}

static void eac3_update_channels(GF_AC3Config *hdr)
{
	u32 i;
	for (i=0; i<hdr->nb_streams; i++) {
		u32 nb_ch = ac3_mod_to_total_chans[hdr->streams[i].acmod];
		if (hdr->streams[i].nb_dep_sub) {
			hdr->streams[i].chan_loc = eac3_chanmap_to_chan_loc(hdr->streams[i].chan_loc);
			nb_ch += gf_eac3_get_chan_loc_count(hdr->streams[i].chan_loc);
		}
		if (hdr->streams[i].lfon) nb_ch++;
		hdr->streams[i].channels = nb_ch;
		hdr->streams[i].surround_channels = ac3_mod_to_surround_chans[hdr->streams[i].acmod];
	}
}

static Bool gf_eac3_parser_internal(GF_BitStream *bs, GF_AC3Config *hdr, Bool full_parse)
{
	u32 fscod, bsid, acmod, freq, framesize, syncword, substreamid, lfon, numblkscod, strmtyp, frmsiz, bsmod;
	u64 pos, hdr_pos;
	u16 chanmap;
	Bool main_indep_found = GF_FALSE;
	s32 cur_main_id = -1;
	u32 nb_blocks_main;
	u16 main_substreams; //bit-mask of independent channels found so far
	static u32 numblks[4] = {1, 2, 3, 6};

	if (!hdr || !AC3_FindSyncCodeBS(bs))
		return GF_FALSE;

retry_frame:
	pos = gf_bs_get_position(bs);
	framesize = 0;
	numblkscod = 0;
	bsmod = 0;
	nb_blocks_main = 0;
	main_substreams = 0;
	memset(hdr, 0, sizeof(GF_AC3Config));

next_block:
	hdr_pos = gf_bs_get_position(bs);

	bsid = gf_bs_peek_bits(bs, 5, 5);
	//"If an AC-3 bit stream is present in the Enhanced AC-3 bit stream, then the AC-3 bit stream shall be treated
	//as an independent substream assigned substream ID 0."
	if (bsid<=8) {
		GF_AC3Header ac3h;
		//we are done
		if (main_indep_found) {
			eac3_update_channels(hdr);
			gf_bs_seek(bs, pos);
			return GF_TRUE;
		}
		if (!gf_ac3_parser_bs(bs, &ac3h, GF_TRUE)) {
			gf_bs_seek(bs, pos);
			return GF_FALSE;
		}
		hdr->streams[0] = ac3h.streams[0];
		hdr->nb_streams = 1;
		hdr->sample_rate = ac3h.sample_rate;
		main_substreams |= 1;
		hdr->framesize = ac3h.framesize;
		nb_blocks_main = 6;
		hdr->brcode = gf_ac3_get_bitrate(ac3h.brcode)/1000;

		gf_bs_skip_bytes(bs, ac3h.framesize);
		if (!AC3_FindSyncCodeBS(bs)) {
			gf_bs_seek(bs, pos);
			return GF_FALSE;
		}
		main_indep_found = GF_TRUE;
		cur_main_id = 0;
		goto next_block;
	}
	//corrupted frame, trash
	if ((bsid<10) || (bsid>16)) {
		gf_bs_skip_bytes(bs, 1);//we are still at the startcode
		if (!AC3_FindSyncCodeBS(bs)) {
			gf_bs_seek(bs, pos);
			return GF_FALSE;
		}
		goto next_block;
	}
	syncword = gf_bs_read_u16(bs);
	if (syncword != 0x0B77) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_CODING, ("[E-AC3] Wrong sync word detected (0x%X - expecting 0x0B77).\n", syncword));
		return GF_FALSE;
	}

	hdr->is_ec3 = 1;
	strmtyp = gf_bs_read_int_log(bs, 2, "strmtyp");
	substreamid = gf_bs_read_int_log(bs, 3, "substreamid");

	//independent stream
	if (strmtyp!=0x1) {
		//all blocks gathered and we have seen this substreamid, done with whole frame
		if ( (nb_blocks_main>=6) && ( (main_substreams >> substreamid) & 0x1)) {
			eac3_update_channels(hdr);
			gf_bs_seek(bs, pos);
			return GF_TRUE;
		}
		//main independent: "All Enhanced AC-3 bit streams shall contain an independent substream assigned substream ID 0.
		// The independent substream assigned substream ID 0 shall be the first substream present in the bit stream."
		if (!substreamid)
			main_indep_found = 1;
		if (cur_main_id != substreamid)
			nb_blocks_main=0;
		cur_main_id = substreamid;
	}

	//trash everything until we find first indep stream
	if (!main_indep_found) {
		gf_bs_align(bs);
		if (!AC3_FindSyncCodeBS(bs)) {
			gf_bs_seek(bs, pos);
			return GF_FALSE;
		}
		goto retry_frame;
	}

	frmsiz = gf_bs_read_int_log(bs, 11, "frmsiz");
	framesize = 2 * (1 + frmsiz);
	fscod = gf_bs_read_int_log(bs, 2, "fscod");
	if (fscod == 0x3) {
		fscod = gf_bs_read_int_log(bs, 2, "fscod2");
		numblkscod = 3;
	} else {
		numblkscod = gf_bs_read_int_log(bs, 2, "numblkscod");
	}

	//remember our independent substreams
	if (strmtyp!=0x1) {
		main_substreams |= (1 << substreamid);
	}

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
		//do not sync
		gf_bs_align(bs);
		return GF_FALSE;
	}

	acmod = gf_bs_read_int_log(bs, 3, "acmod");
	lfon = gf_bs_read_int_log(bs, 1, "lfon");
	bsid = gf_bs_read_int_log(bs, 5, "bsid");

	gf_bs_read_int_log(bs, 5, "dialnorm");
	if (gf_bs_read_int_log(bs, 1, "compre")) {
		gf_bs_read_int_log(bs, 8, "compr");
	}
	if (acmod==0) {
		gf_bs_read_int_log(bs, 5, "dialnorm2");
		if (gf_bs_read_int_log(bs, 1, "compr2e")) {
			gf_bs_read_int_log(bs, 8, "compr2");
		}
	}
	chanmap = 0;
	if (strmtyp==0x1) {
		if (gf_bs_read_int_log(bs, 1, "chanmape")) {
			chanmap = gf_bs_read_int_log(bs, 16, "chanmap");
		}
	}

	hdr->sample_rate = freq;
	hdr->framesize += framesize;
	if (strmtyp != 1) {
		gf_assert(cur_main_id == substreamid);
		hdr->streams[substreamid].lfon = lfon;
		hdr->streams[substreamid].bsid = bsid;
		hdr->streams[substreamid].bsmod = bsmod;
		hdr->streams[substreamid].acmod = acmod;
		hdr->streams[substreamid].fscod = fscod;
		hdr->brcode = 0;
		if (hdr->nb_streams<8)
			hdr->nb_streams++;
	}
	//dependent stream, record max substream ID of dep and store chan map
	//"Dependent substreams are assigned substream ID's 0 to 7, which shall be assigned sequentially according to the order the dependent substreams are present in the bit stream. "
	else {
		hdr->streams[cur_main_id].nb_dep_sub = 1+substreamid;
		hdr->streams[cur_main_id].chan_loc |= chanmap;
	}

	//not clear if this is only for the independent streams - spec says "The value is the sum of the data rates of all the substreams"
	hdr->brcode += ((frmsiz+1) * freq) / (numblks[numblkscod]*16) / 1000;

	//start of header only, we are done - chan info might be wrong
	if (!full_parse) {
		eac3_update_channels(hdr);
		gf_bs_seek(bs, pos);
		return GF_TRUE;
	}

	//mix metadata
	if (gf_bs_read_int(bs, 1)) {
		if (acmod > 0x2) gf_bs_read_int(bs, 2);
		if ((acmod & 0x1) && (acmod > 0x2)) gf_bs_read_int(bs, 6);
		if (acmod & 0x4) gf_bs_read_int(bs, 6);
		if (lfon) {
			if (gf_bs_read_int(bs, 1))
				gf_bs_read_int(bs, 5);
		}
		if (strmtyp == 0) {
			//pgmscle
			if (gf_bs_read_int(bs, 1))
				gf_bs_read_int(bs, 6);
			if (acmod==0) {
				//pgmscl2e
				if (gf_bs_read_int(bs, 1))
					gf_bs_read_int(bs, 6);
			}
			//extpgmscle
			if (gf_bs_read_int(bs, 1))
				gf_bs_read_int(bs, 6);
			u8 mixdef = gf_bs_read_int(bs, 2);
			if (mixdef == 0x1) {
				gf_bs_read_int(bs, 5);
			} else if (mixdef == 0x2) {
				gf_bs_read_int(bs, 12);
			} else if (mixdef == 0x3) {
				u32 mixdeflen = gf_bs_read_int(bs, 5);
				mixdeflen = 8 * (mixdeflen + 2);
				while (mixdeflen) {
					gf_bs_read_int(bs, 1);
					mixdeflen--;
				}
			}
			if (acmod < 0x2) {
				//paninfoe
				if (gf_bs_read_int(bs, 1))
					gf_bs_read_int(bs, 14);
				if (acmod == 0) {
					//paninfo2e
					if (gf_bs_read_int(bs, 1))
						gf_bs_read_int(bs, 14);
				}

			}
			//frmmixcfginfoe
			if (gf_bs_read_int(bs, 1)) {
				if (numblkscod == 0x0) {
					gf_bs_read_int(bs, 5);
				} else {
					u32 i, nb_blocks = numblks[numblkscod];
					for (i=0; i<nb_blocks; i++) {
						if (gf_bs_read_int(bs, 1))
							gf_bs_read_int(bs, 5);
					}
				}
			}
		}
	}
	//info metadata
	if (gf_bs_read_int(bs, 1)) {
		gf_bs_read_int(bs, 5);
		if (acmod == 0x2) gf_bs_read_int(bs, 4);
		if (acmod >= 0x6) gf_bs_read_int(bs, 2);
		//audprodie
		if (gf_bs_read_int(bs, 1)) gf_bs_read_int(bs, 8);
		if (acmod == 0x0) {
			//audprodi2e
			if (gf_bs_read_int(bs, 1)) gf_bs_read_int(bs, 8);
		}
		if (fscod < 0x3)  gf_bs_read_int(bs, 1);
	}
	if ((strmtyp == 0) && (numblkscod != 0x3)) gf_bs_read_int(bs, 1);
	if (strmtyp == 0x2) {
		u32 blkid=0;
		if (numblkscod == 0x3) blkid=1;
		else blkid = gf_bs_read_int(bs, 1);
		if (blkid) gf_bs_read_int(bs, 6);
	}
	u8 addbsie = gf_bs_read_int(bs, 1);
	if (addbsie) {
		u32 addbsil = gf_bs_read_int(bs, 6) + 1;
		//we only use the first 2 bytes - cf 8.3 of ETSI 103 420 V1.2.1
		if (addbsil>=2) {
			gf_bs_read_int(bs, 7);
			if (gf_bs_read_int(bs, 1)) {
				hdr->atmos_ec3_ext = 1;
				hdr->complexity_index_type = gf_bs_read_int(bs, 8);
			}
		}
	}

	//remember numbers of block for main
	if (strmtyp!=0x1) {
		nb_blocks_main += numblks[numblkscod];
	}

	if (gf_bs_seek(bs, hdr_pos + framesize) != GF_OK) {
		gf_bs_seek(bs, pos);
		return GF_FALSE;
	}

	if (!AC3_FindSyncCodeBS(bs)) {
		gf_bs_seek(bs, pos);
		return GF_FALSE;
	}
	//we go to next block even if we have 6 of main (to check deps)
	goto next_block;
}

GF_EXPORT
Bool gf_eac3_parser_bs(GF_BitStream *bs, GF_AC3Config *hdr, Bool full_parse)
{
	return gf_eac3_parser_internal(bs, hdr, full_parse);
}

GF_EXPORT
Bool gf_eac3_parser(u8 *buf, u32 buflen, u32 *pos, GF_AC3Config *hdr, Bool full_parse)
{
	GF_BitStream *bs;
	Bool ret;

	if (buflen < 6) return GF_FALSE;
	(*pos) = AC3_FindSyncCode(buf, buflen);
	if (*pos >= buflen) return GF_FALSE;

	bs = gf_bs_new((const char*)(buf + *pos), buflen, GF_BITSTREAM_READ);
	ret = gf_eac3_parser_internal(bs, hdr, full_parse);
	gf_bs_del(bs);
	return ret;
}

#endif /*GPAC_DISABLE_AV_PARSERS*/

u32 gf_id3_read_size(GF_BitStream *bs)
{
	u32 size = 0;
	gf_bs_read_int(bs, 1);
	size |= gf_bs_read_int(bs, 7);
	size<<=7;
	gf_bs_read_int(bs, 1);
	size |= gf_bs_read_int(bs, 7);
	size<<=7;
	gf_bs_read_int(bs, 1);
	size |= gf_bs_read_int(bs, 7);
	size<<=7;
	gf_bs_read_int(bs, 1);
	size |= gf_bs_read_int(bs, 7);
	return size;
}


#if !defined(GPAC_DISABLE_AV_PARSERS) && !defined (GPAC_DISABLE_OGG)

/*
	Vorbis parser
*/

static u32 vorbis_book_maptype1_quantvals(u32 entries, u32 dim)
{
	u32 vals = (u32)floor(pow(entries, 1.0 / dim));
	while (1) {
		u32 acc = 1;
		u32 acc1 = 1;
		u32 i;
		for (i = 0; i < dim; i++) {
			acc *= vals;
			acc1 *= vals + 1;
		}
		if (acc <= entries && acc1 > entries) return (vals);
		else {
			if (acc > entries) vals--;
			else vals++;
		}
	}
}

static u32 ilog(u32 v, Bool dec)
{
	u32 ret = 0;
	if (dec && v) --v;
	while (v) {
		ret++;
		v >>= 1;
	}
	return (ret);
}

static u32 icount(u32 v)
{
	u32 ret = 0;
	while (v) {
		ret += v & 1;
		v >>= 1;
	}
	return(ret);
}


GF_EXPORT
Bool gf_vorbis_parse_header(GF_VorbisParser *vp, u8 *data, u32 data_len)
{
	u32 pack_type, i, j, k, times, nb_part, nb_books, nb_modes;
	u32 l;
	char szNAME[8];
	oggpack_buffer opb;

	oggpack_readinit(&opb, (u8*)data, data_len);
	pack_type = oggpack_read(&opb, 8);
	i = 0;
	while (i < 6) {
		szNAME[i] = oggpack_read(&opb, 8);
		i++;
	}
	szNAME[i] = 0;
	if (strcmp(szNAME, "vorbis")) {
		return GF_FALSE;
	}

	switch (pack_type) {
	case 0x01:
		vp->version = oggpack_read(&opb, 32);
		if (vp->version != 0) {
			return GF_FALSE;
		}
		vp->channels = oggpack_read(&opb, 8);
		vp->sample_rate = oggpack_read(&opb, 32);
		vp->max_r = oggpack_read(&opb, 32);
		vp->avg_r = oggpack_read(&opb, 32);
		vp->low_r = oggpack_read(&opb, 32);

		vp->min_block = 1<<oggpack_read(&opb, 4);
		vp->max_block = 1<<oggpack_read(&opb, 4);
		if (vp->sample_rate < 1 || vp->channels < 1 || vp->min_block < 8 || vp->max_block < vp->min_block
		    || oggpack_read(&opb, 1) != 1) {
			return GF_FALSE;
		}
		vp->nb_init=1;
		return GF_TRUE;

	case 0x03:
		/*trash comments*/
		vp->nb_init++;
		return GF_TRUE;
	case 0x05:
		/*need at least bitstream header to make sure we're parsing the right thing*/
		if (!vp->nb_init) return GF_FALSE;
		break;
	default:
		return GF_FALSE;
	}
	/*OK parse codebook*/
	nb_books = oggpack_read(&opb, 8) + 1;
	/*skip vorbis static books*/
	for (i = 0; i < nb_books; i++) {
		u32 map_type, qb, qq;
		u32 entries, dim;
		oggpack_read(&opb, 24);
		dim = oggpack_read(&opb, 16);
		entries = oggpack_read(&opb, 24);
		if ((s32)entries < 0) entries = 0;
		if (oggpack_read(&opb, 1) == 0) {
			if (oggpack_read(&opb, 1)) {
				for (j = 0; j < entries; j++) {
					if (oggpack_read(&opb, 1)) {
						oggpack_read(&opb, 5);
					}
				}
			}
			else {
				for (j = 0; j < entries; j++)
					oggpack_read(&opb, 5);
			}
		}
		else {
			oggpack_read(&opb, 5);
			for (j = 0; j < entries;) {
				u32 num = oggpack_read(&opb, ilog(entries - j, GF_FALSE));
				for (k = 0; k < num && j < entries; k++, j++) {
				}
			}
		}
		switch ((map_type = oggpack_read(&opb, 4))) {
		case 0:
			break;
		case 1:
		case 2:
			oggpack_read(&opb, 32);
			oggpack_read(&opb, 32);
			qq = oggpack_read(&opb, 4) + 1;
			oggpack_read(&opb, 1);
			if (map_type == 1) qb = vorbis_book_maptype1_quantvals(entries, dim);
			else if (map_type == 2) qb = entries * dim;
			else qb = 0;
			for (j = 0; j < qb; j++) oggpack_read(&opb, qq);
			break;
		}
	}
	times = oggpack_read(&opb, 6) + 1;
	for (i = 0; i < times; i++) oggpack_read(&opb, 16);
	times = oggpack_read(&opb, 6) + 1;
	for (i = 0; i < times; i++) {
		u32 type = oggpack_read(&opb, 16);
		if (type) {
			u32 *parts, *class_dims, count, rangebits;
			u32 max_class = 0;
			nb_part = oggpack_read(&opb, 5);
			parts = (u32*)gf_malloc(sizeof(u32) * nb_part);
			for (j = 0; j < nb_part; j++) {
				parts[j] = oggpack_read(&opb, 4);
				if (max_class < parts[j]) max_class = parts[j];
			}
			class_dims = (u32*)gf_malloc(sizeof(u32) * (max_class + 1));
			for (j = 0; j < max_class + 1; j++) {
				u32 class_sub;
				class_dims[j] = oggpack_read(&opb, 3) + 1;
				class_sub = oggpack_read(&opb, 2);
				if (class_sub) oggpack_read(&opb, 8);
				for (k = 0; k < (u32)(1 << class_sub); k++) oggpack_read(&opb, 8);
			}
			oggpack_read(&opb, 2);
			rangebits = oggpack_read(&opb, 4);
			count = 0;
			for (j = 0, k = 0; j < nb_part; j++) {
				count += class_dims[parts[j]];
				for (; k < count; k++) oggpack_read(&opb, rangebits);
			}
			gf_free(parts);
			gf_free(class_dims);
		}
		else {
			oggpack_read(&opb, 8);
			oggpack_read(&opb, 16);
			oggpack_read(&opb, 16);
			oggpack_read(&opb, 6);
			oggpack_read(&opb, 8);
			nb_books = oggpack_read(&opb, 4) + 1;
			for (j = 0; j < nb_books; j++)
				oggpack_read(&opb, 8);
		}
	}
	times = oggpack_read(&opb, 6) + 1;
	for (i = 0; i < times; i++) {
		u32 acc = 0;
		oggpack_read(&opb, 16);/*type*/
		oggpack_read(&opb, 24);
		oggpack_read(&opb, 24);
		oggpack_read(&opb, 24);
		nb_part = oggpack_read(&opb, 6) + 1;
		oggpack_read(&opb, 8);
		for (j = 0; j < nb_part; j++) {
			u32 cascade = oggpack_read(&opb, 3);
			if (oggpack_read(&opb, 1)) cascade |= (oggpack_read(&opb, 5) << 3);
			acc += icount(cascade);
		}
		for (j = 0; j < acc; j++) oggpack_read(&opb, 8);
	}
	times = oggpack_read(&opb, 6) + 1;
	for (i = 0; i < times; i++) {
		u32 sub_maps = 1;
		oggpack_read(&opb, 16);
		if (oggpack_read(&opb, 1)) sub_maps = oggpack_read(&opb, 4) + 1;
		if (oggpack_read(&opb, 1)) {
			u32 nb_steps = oggpack_read(&opb, 8) + 1;
			for (j = 0; j < nb_steps; j++) {
				oggpack_read(&opb, ilog(vp->channels, GF_TRUE));
				oggpack_read(&opb, ilog(vp->channels, GF_TRUE));
			}
		}
		oggpack_read(&opb, 2);
		if (sub_maps>1) {
			for(l=0; l<vp->channels; l++)
				oggpack_read(&opb, 4);
		}
		for (j = 0; j < sub_maps; j++) {
			oggpack_read(&opb, 8);
			oggpack_read(&opb, 8);
			oggpack_read(&opb, 8);
		}
	}
	nb_modes = oggpack_read(&opb, 6) + 1;
	for (i = 0; i < nb_modes; i++) {
		vp->mode_flag[i] = oggpack_read(&opb, 1);
		oggpack_read(&opb, 16);
		oggpack_read(&opb, 16);
		oggpack_read(&opb, 8);
	}

	vp->modebits = 0;
	j = nb_modes;
	while (j > 1) {
		vp->modebits++;
		j >>= 1;
	}

	return GF_TRUE;
}

GF_EXPORT
u32 gf_vorbis_check_frame(GF_VorbisParser *vp, u8 *data, u32 data_length)
{
	s32 block_size;
	oggpack_buffer opb;
	if (!vp) return 0;
	oggpack_readinit(&opb, (unsigned char*)data, data_length);
	/*not audio*/
	if (oggpack_read(&opb, 1) != 0) return 0;
	block_size = oggpack_read(&opb, vp->modebits);
	if (block_size == -1) return 0;
	return ((vp->mode_flag[block_size]) ? vp->max_block : vp->min_block) / (2);
}


#endif /*!defined(GPAC_DISABLE_AV_PARSERS) && !defined (GPAC_DISABLE_OGG)*/


#if !defined(GPAC_DISABLE_AV_PARSERS)

/*call with vorbis header packets - initializes the parser on success, leave it to NULL otherwise
returns 1 if success, 0 if error.*/
Bool gf_opus_parse_header(GF_OpusConfig *ocfg, u8 *data, u32 data_len)
{
	char tag[9];
	GF_BitStream *bs = gf_bs_new(data, data_len, GF_BITSTREAM_READ);
	gf_bs_read_data(bs, tag, 8);
	tag[8]=0;

	if (memcmp(data, "OpusHead", sizeof(char)*8)) {
		gf_bs_del(bs);
		return GF_FALSE;
	}
	/*Identification Header*/
	ocfg->version = gf_bs_read_u8(bs); /*version*/
	if (ocfg->version != 1) {
		gf_bs_del(bs);
		GF_LOG(GF_LOG_ERROR, GF_LOG_CODING, ("[Opus] Unsupported version %d\n", ocfg->version));
		return GF_FALSE;
	}
	ocfg->OutputChannelCount = gf_bs_read_u8(bs);
	ocfg->PreSkip = gf_bs_read_u16_le(bs);
	ocfg->InputSampleRate = gf_bs_read_u32_le(bs);
	ocfg->OutputGain = gf_bs_read_u16_le(bs);
	ocfg->ChannelMappingFamily = gf_bs_read_u8(bs);
	if (ocfg->ChannelMappingFamily != 0) {
		ocfg->StreamCount = gf_bs_read_u8(bs);
		ocfg->CoupledCount = gf_bs_read_u8(bs);
		gf_bs_read_data(bs, (char *) ocfg->ChannelMapping, ocfg->OutputChannelCount);
	}
	gf_bs_del(bs);
	return GF_TRUE;
}

/*returns 0 if init error or not an opus frame, otherwise returns the number of audio samples
in this frame*/
u32 gf_opus_check_frame(GF_OpusConfig *ocfg, u8 *data, u32 data_length)
{
	u32 block_size;

	if (!data || !data_length) return 0;
	if (!memcmp(data, "OpusHead", sizeof(char)*8)) return 0;
	if (!memcmp(data, "OpusTags", sizeof(char)*8)) return 0;

	/*consider the whole packet as Ogg packets and ISOBMFF samples for Opus are framed similarly*/
	static const int OpusFrameDurIn48k[] = { 480, 960, 1920, 2880, 480, 960, 1920, 2880, 480, 960, 1920, 2880,
		480, 960, 480, 960,
		120, 240, 480, 960, 120, 240, 480, 960, 120, 240, 480, 960, 120, 240, 480, 960,
	};
	int TOC_config = (data[0] & 0xf8) >> 3;
	//int s = (data[0] & 0x04) >> 2;
	block_size = OpusFrameDurIn48k[TOC_config];

	int c = data[0] & 0x03;
	if (c == 1 || c == 2) {
		block_size *= 2;
	} else if (c == 3) {
		/*unknown number of frames*/
		int num_frames = data[1] & 0x3f;
		block_size *= num_frames;
	}
	return block_size;
}

/* return nb bytes read */
static u8 gf_opus_read_length(u8 *data, u32 data_length, u32 offset, u16 *read_length) {
    if (!data || !data_length || !read_length) {
        GF_LOG(GF_LOG_ERROR, GF_LOG_CODING, ("Cannot read Opus length value\n"));
        return 0;
    }
    if (offset >= data_length) {
        GF_LOG(GF_LOG_ERROR, GF_LOG_CODING, ("Not enough bytes to read Opus length\n"));
        return 0;
    }
    if (data[offset] < 252) {
        *read_length = data[offset];
        return 1;
    } else {
        if (offset+1 >= data_length) {
            GF_LOG(GF_LOG_ERROR, GF_LOG_CODING, ("Not enough bytes to read 2-byte Opus length\n"));
            return 0;
        }
        *read_length = data[offset+1]*4+data[offset];
        return 2;
    }
}

GF_EXPORT
u8 gf_opus_parse_packet_header(u8 *data, u32 data_length, Bool self_delimited, GF_OpusPacketHeader *header)
{
    u32 i;
    u32 nb_read_bytes;
    if (!data || !data_length)
        return 0;
    if (!header)
        return 0;
    if (data_length>=8 && !memcmp(data, "OpusHead", sizeof(char)*8))
        return 0;
    if (data_length>=8 && !memcmp(data, "OpusTags", sizeof(char)*8))
        return 0;

    GF_LOG(GF_LOG_DEBUG, GF_LOG_CODING, ("Processing Opus packet, self: %d, size %d\n", self_delimited, data_length));

    if (data_length < 1) {
        GF_LOG(GF_LOG_ERROR, GF_LOG_CODING, ("Opus packet size must be at least one to parse TOC byte\n"));
        return 0;
    }
    memset(header, 0, sizeof(GF_OpusPacketHeader));
    header->self_delimited = self_delimited;
    header->TOC_config = (data[0] & 0xf8) >> 3;
    header->TOC_stereo = (data[0] & 0x4) >> 2;
    header->TOC_code = data[0] & 0x03;
    header->size = 1;
    if (header->TOC_code == 0) {
        header->nb_frames = 1;
        if (self_delimited) {
            nb_read_bytes = gf_opus_read_length(data, data_length, header->size, &header->self_delimited_length);
            if (nb_read_bytes) {
                header->size += nb_read_bytes;
            } else {
                GF_LOG(GF_LOG_ERROR, GF_LOG_CODING, ("Could not read self delimited length in Opus packet code 0\n"));
                return 0;
            }
//            0                   1                   2                   3
//            0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
//           +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//           | config  |s|0|0| N1 (1-2 bytes):                               |
//           +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+                               |
//           |               Compressed frame 1 (N1 bytes)...                :
//           :                                                               |
//           |                                                               |
//           +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
            header->frame_lengths[0] = header->self_delimited_length;
        } else {
//            0                   1                   2                   3
//            0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
//           +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//           | config  |s|0|0|                                               |
//           +-+-+-+-+-+-+-+-+                                               |
//           |                    Compressed frame 1 (N-1 bytes)...          :
//           :                                                               |
//           |                                                               |
//           +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
            header->frame_lengths[0] = data_length - header->size;
        }
        header->packet_size = header->size + header->frame_lengths[0];
    } else if (header->TOC_code == 1) {
        header->nb_frames = 2;
        if (self_delimited) {
            nb_read_bytes = gf_opus_read_length(data, data_length, header->size, &header->self_delimited_length);
            if (nb_read_bytes) {
                header->size += nb_read_bytes;
            } else {
                GF_LOG(GF_LOG_ERROR, GF_LOG_CODING, ("Could not read self delimited length in Opus packet code 1\n"));
                return 0;
            }
//            0                   1                   2                   3
//            0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
//           +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//           | config  |s|0|1| N1 (1-2 bytes):                               |
//           +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+                               :
//           |               Compressed frame 1 (N1 bytes)...                |
//           :                               +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//           |                               |                               |
//           +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+                               :
//           |               Compressed frame 2 (N1 bytes)...                |
//           :                                               +-+-+-+-+-+-+-+-+
//           |                                               |
//           +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
            header->frame_lengths[0] = header->self_delimited_length;
            header->frame_lengths[1] = header->self_delimited_length;
        } else {
//            0                   1                   2                   3
//            0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
//           +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//           | config  |s|0|1|                                               |
//           +-+-+-+-+-+-+-+-+                                               :
//           |             Compressed frame 1 ((N-1)/2 bytes)...             |
//           :                               +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//           |                               |                               |
//           +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+                               :
//           |             Compressed frame 2 ((N-1)/2 bytes)...             |
//           :                                               +-+-+-+-+-+-+-+-+
//           |                                               |
//           +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
            if ((data_length-header->size) % 2) {
                GF_LOG(GF_LOG_ERROR, GF_LOG_CODING, ("Size of non-self-delimited Opus packet with code 2 must be even but is %d\n",data_length-header->size));
                return 0;
            }
            header->frame_lengths[0] = (data_length-header->size)/2;
            header->frame_lengths[1] = (data_length-header->size)/2;
        }
        header->packet_size = header->size + header->frame_lengths[0] + header->frame_lengths[1];
    } else if (header->TOC_code == 2) {
        header->nb_frames = 2;
        if (self_delimited) {
            nb_read_bytes = gf_opus_read_length(data, data_length, header->size, &header->self_delimited_length);
            if (nb_read_bytes) {
                header->size += nb_read_bytes;
            } else {
                GF_LOG(GF_LOG_ERROR, GF_LOG_CODING, ("Could not read self delimited length in Opus packet code 2\n"));
                return 0;
            }
        }
        nb_read_bytes = gf_opus_read_length(data, data_length, header->size, &header->code2_frame_length);
        if (nb_read_bytes) {
            header->size += nb_read_bytes;
        } else {
            GF_LOG(GF_LOG_ERROR, GF_LOG_CODING, ("Could not read frame length in Opus packet code 2\n"));
            return 0;
        }
        if (self_delimited) {
//            0                   1                   2                   3
//            0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
//           +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//           | config  |s|1|0| N1 (1-2 bytes): N2 (1-2 bytes :               |
//           +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+               :
//           |               Compressed frame 1 (N1 bytes)...                |
//           :                               +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//           |                               |                               |
//           +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+                               |
//           |               Compressed frame 2 (N2 bytes)...                :
//           :                                                               |
//           |                                                               |
//           +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
            header->frame_lengths[0] = header->self_delimited_length;
            header->frame_lengths[1] = header->code2_frame_length;
        } else {
//            0                   1                   2                   3
//            0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
//           +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//           | config  |s|1|0| N1 (1-2 bytes):                               |
//           +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+                               :
//           |               Compressed frame 1 (N1 bytes)...                |
//           :                               +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//           |                               |                               |
//           +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+                               |
//           |                     Compressed frame 2...                     :
//           :                                                               |
//           |                                                               |
//           +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
            header->frame_lengths[0] = header->code2_frame_length;
            header->frame_lengths[1] = data_length - header->size - header->code2_frame_length;
        }
        header->packet_size = header->size + header->frame_lengths[0] + header->frame_lengths[1];
    } else if (header->TOC_code == 3) {
        u32 sum = 0;
        if (data_length <= header->size) {
            GF_LOG(GF_LOG_ERROR, GF_LOG_CODING, ("Not enough data to parse TOC code 3 data\n"));
            return 0;
        }
        header->code3_vbr = (data[header->size] & 0x80) >> 7;
        header->code3_padding = (data[header->size] & 0x40) >> 6;
        header->nb_frames = data[header->size] & 0x3f;
        header->size++;
        if (header->code3_padding) {
            if (data_length <= header->size) {
                GF_LOG(GF_LOG_ERROR, GF_LOG_CODING, ("Not enough data to parse TOC code 3 padding length\n"));
                return 0;
            }
            if (data[header->size] == 255) {
                header->code3_padding_length = 254 + data[header->size+1];
                header->size += 2;
            } else {
                header->code3_padding_length = data[header->size];
                header->size++;
            }
        } else {
            header->code3_padding_length = 0;
        }
        if (self_delimited) {
            nb_read_bytes = gf_opus_read_length(data, data_length, header->size, &header->self_delimited_length);
            if (nb_read_bytes) {
                header->size += nb_read_bytes;
            } else {
                GF_LOG(GF_LOG_ERROR, GF_LOG_CODING, ("Could not read self delimited length in Opus packet code 3\n"));
                return 0;
            }
        }
        if (header->code3_vbr) {
            u32 max;
            u32 min;
            if (self_delimited) {
//                0                   1                   2                   3
//                0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
//               +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//               | config  |s|1|1|1|p|     M     | Padding length (Optional)     :
//               +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//               : N1 (1-2 bytes):     ...       :     N[M-1]    |     N[M]      :
//               +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//               |                                                               |
//               :               Compressed frame 1 (N1 bytes)...                :
//               |                                                               |
//               +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//               |                                                               |
//               :               Compressed frame 2 (N2 bytes)...                :
//               |                                                               |
//               +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//               |                                                               |
//               :                              ...                              :
//               |                                                               |
//               +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//               |                                                               |
//               :              Compressed frame M (N[M] bytes)...               :
//               |                                                               |
//               +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//               :                  Opus Padding (Optional)...                   |
//               +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
                header->frame_lengths[0] = header->self_delimited_length;
                min = 1;
                max = header->nb_frames;
                sum += header->frame_lengths[0];
            } else {
//                0                   1                   2                   3
//                0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
//               +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//               | config  |s|1|1|1|p|     M     | Padding length (Optional)     :
//               +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//               : N1 (1-2 bytes): N2 (1-2 bytes):     ...       :     N[M-1]    |
//               +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//               |                                                               |
//               :               Compressed frame 1 (N1 bytes)...                :
//               |                                                               |
//               +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//               |                                                               |
//               :               Compressed frame 2 (N2 bytes)...                :
//               |                                                               |
//               +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//               |                                                               |
//               :                              ...                              :
//               |                                                               |
//               +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//               |                                                               |
//               :                     Compressed frame M...                     :
//               |                                                               |
//               +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//               :                  Opus Padding (Optional)...                   |
//               +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
                min = 0;
                max = header->nb_frames-1;
            }
            for (i = min; i < max; i++) {
                if (data_length <= header->size) {
                    GF_LOG(GF_LOG_ERROR, GF_LOG_CODING, ("Not enough data to parse TOC code 3 length\n"));
                    return 0;
                }
                nb_read_bytes = gf_opus_read_length(data, data_length, header->size, &(header->frame_lengths[i]));
                if (nb_read_bytes) {
                    header->size += nb_read_bytes;
                } else {
                    GF_LOG(GF_LOG_ERROR, GF_LOG_CODING, ("Could not read frame length in Opus packet code 3\n"));
                    return 0;
                }
                sum += header->frame_lengths[i];
            }
            if (!self_delimited) {
                header->frame_lengths[header->nb_frames-1] = data_length - header->size - header->code3_padding_length - sum;
                sum += header->frame_lengths[header->nb_frames-1];
            }
        } else {
            u32 cbr_length;
            if (self_delimited) {
//                0                   1                   2                   3
//                0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
//               +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//               | config  |s|1|1|0|p|     M     | Pad len (Opt) : N1 (1-2 bytes):
//               +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//               |                                                               |
//               :               Compressed frame 1 (N1 bytes)...                :
//               |                                                               |
//               +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//               |                                                               |
//               :               Compressed frame 2 (N1 bytes)...                :
//               |                                                               |
//               +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//               |                                                               |
//               :                              ...                              :
//               |                                                               |
//               +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//               |                                                               |
//               :               Compressed frame M (N1 bytes)...                :
//               |                                                               |
//               +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//               :                  Opus Padding (Optional)...                   |
//               +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
                cbr_length = header->self_delimited_length;
            } else {
//                0                   1                   2                   3
//                0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
//               +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//               | config  |s|1|1|0|p|     M     |  Padding length (Optional)    :
//               +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//               |                                                               |
//               :               Compressed frame 1 (R/M bytes)...               :
//               |                                                               |
//               +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//               |                                                               |
//               :               Compressed frame 2 (R/M bytes)...               :
//               |                                                               |
//               +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//               |                                                               |
//               :                              ...                              :
//               |                                                               |
//               +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//               |                                                               |
//               :               Compressed frame M (R/M bytes)...               :
//               |                                                               |
//               +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//               :                  Opus Padding (Optional)...                   |
//               +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
                if ((data_length - header->size - header->code3_padding_length) % header->nb_frames) {
                    GF_LOG(GF_LOG_ERROR, GF_LOG_CODING, ("Sum of frame lengths is not a multiple of the number of frames\n"));
                    return 0;
                }
                cbr_length = (data_length - header->size - header->code3_padding_length)/header->nb_frames;
            }
            for (i = 0; i < header->nb_frames; i++) {
                header->frame_lengths[i] = cbr_length;
                sum += header->frame_lengths[i];
            }
        }
        header->packet_size = header->size + header->code3_padding_length + sum;
    }
    return 1;
}

u64 gf_mpegh_escaped_value(GF_BitStream *bs, u32 nBits1, u32 nBits2, u32 nBits3)
{
	u64 value = gf_bs_read_int(bs, nBits1);
	if (value == (1<<nBits1)-1) {
		u32 vadd = gf_bs_read_int(bs, nBits2);
		value += vadd;
		if (vadd == (1<<nBits2)-1) {
			vadd = gf_bs_read_int(bs, nBits3);
			value += vadd;
		}
	}
	return value;
}

GF_EXPORT
s32 gf_mpegh_get_mhas_pl(u8 *ptr, u32 size, u64 *ch_layout)
{
	s32 PL = -1;
	GF_BitStream *bs;
	u32 i;
	s32 sync_pos=-1;

	if (!ptr || !size) return 0;

	for (i=0; i<size-3; i++) {
		if ((ptr[i]==0xC0) && (ptr[i+1]== 0x01) && (ptr[i+2]==0xA5)) {
			sync_pos = i;
			break;
		}
	}
	if (sync_pos<0) return 0;
	if (ch_layout) *ch_layout = 0;
	bs = gf_bs_new(ptr, size, GF_BITSTREAM_READ);
	gf_bs_skip_bytes(bs, sync_pos);

	while (gf_bs_available(bs)) {
		u32 type = (u32) gf_mpegh_escaped_value(bs, 3, 8, 8);
		/*u64 label = */gf_mpegh_escaped_value(bs, 2, 8, 32);
		u64 mh_size = gf_mpegh_escaped_value(bs, 11, 24, 24);
		if (mh_size > gf_bs_available(bs))
			break;
		//MHAS config
		if (type==1) {
			PL = gf_bs_read_int(bs, 8);
			if (ch_layout) {
				u32 idx = gf_bs_read_int(bs, 5);
				if (idx==0x1f)
					gf_bs_read_int(bs, 24);
				/*idx = */gf_bs_read_int(bs, 3);
				gf_bs_read_int(bs, 1);
				gf_bs_read_int(bs, 1);

				//speaker config
				idx = gf_bs_read_int(bs, 2);
				if (idx == 0) {
					*ch_layout = gf_audio_fmt_get_layout_from_cicp( gf_bs_read_int(bs, 6) );
				}
			}
			break;
		}
		gf_bs_skip_bytes(bs, mh_size);
	}
	gf_bs_del(bs);
	return PL;
}

GF_EXPORT
void gf_vvc_parse_sei(char *buffer, u32 nal_size, VVCState *vvc)
{
	gf_hevc_vvc_parse_sei(buffer, nal_size, NULL, vvc);
}

static Bool vvc_parse_nal_header(GF_BitStream *bs, u8 *nal_unit_type, u8 *temporal_id, u8 *layer_id)
{
	u32 val;
	val = gf_bs_read_int_log(bs, 1, "forbidden_zero");
	if (val) return GF_FALSE;
	val = gf_bs_read_int_log(bs, 1, "reserved_zero");
	if (val) return GF_FALSE;

	val = gf_bs_read_int_log(bs, 6, "layerID");
	if (layer_id) *layer_id = val;

	val = gf_bs_read_int_log(bs, 5, "nuh_type");
	if (nal_unit_type) *nal_unit_type = val;

	val = gf_bs_read_int_log(bs, 3, "temporalID");
	if (!val) return GF_FALSE;
	val -= 1;
	if (temporal_id) *temporal_id = val;
	return GF_TRUE;
}

static void vvc_profile_tier_level(GF_BitStream *bs, VVC_ProfileTierLevel *ptl, u32 idx)
{
	u32 i;
	if (ptl->pt_present) {
		ptl->general_profile_idc = gf_bs_read_int_log_idx(bs, 7, "general_profile_idc", idx);
		ptl->general_tier_flag = gf_bs_read_int_log_idx(bs, 1, "general_tier_flag", idx);
	}
	ptl->general_level_idc = gf_bs_read_int_log_idx(bs, 8, "general_level_idc", idx);
	ptl->frame_only_constraint = gf_bs_read_int_log_idx(bs, 1, "frame_only_constraint", idx);
	ptl->multilayer_enabled = gf_bs_read_int_log_idx(bs, 1, "multilayer_enabled", idx);
	//general constraints info - max size if 1 + 81 + 8 + 255
	if (ptl->pt_present) {
		//		general_constraints_info
		ptl->gci_present = gf_bs_read_int_log_idx(bs, 1, "gci_present", idx);
		if (ptl->gci_present) {
			u8 res;
			ptl->gci[0] = 0x80;
			ptl->gci[0] |= gf_bs_read_int(bs, 7);
			//71 buts till reserved, so 71-7 = 64bits = 8 bytes till reserved
			gf_bs_read_data(bs, ptl->gci+1, 8);
			ptl->gci[10] = gf_bs_read_int(bs, 2)<<6;
			//skip extensions
			ptl->gci[11] = 0;
			res = gf_bs_read_int(bs, 8);
			gf_bs_read_int(bs, res);
		}
		gf_bs_align(bs);
	}
	for (i=ptl->ptl_max_tid; i>0; i--) {
		ptl->sub_ptl[i-1].level_present_flag = gf_bs_read_int_log_idx2(bs, 1, "level_present_flag", idx, i);
	}
	gf_bs_align(bs);
	for (i=ptl->ptl_max_tid; i>0; i--) {
		if (ptl->sub_ptl[i-1].level_present_flag)
			ptl->sub_ptl[i-1].sublayer_level_idc = gf_bs_read_int_log_idx2(bs, 8, "sublayer_level_idc", idx, i);
	}
	if (ptl->pt_present) {
		ptl->num_sub_profiles = gf_bs_read_int_log_idx(bs, 8, "num_sub_profiles", idx);
		for (i=0; i<ptl->num_sub_profiles; i++) {
			ptl->sub_profile_idc[i] = gf_bs_read_int_log_idx2(bs, 32, "sub_profile_idc", idx, i);
		}
	}
}

#define VVC_VPS_BROKEN {\
	memset(vps, 0, sizeof(VVC_VPS)); \
	return -1;\
	}

static s32 gf_vvc_read_vps_bs_internal(GF_BitStream *bs, VVCState *vvc, Bool stop_at_vps_ext)
{
	u32 i, j;
	s32 vps_id;
	VVC_VPS *vps;
	Bool vps_default_ptl_dpb_hrd_max_tid_flag=0;

	//nalu header already parsed
	vps_id = gf_bs_read_int_log(bs, 4, "vps_id");
	if ((vps_id<0) || (vps_id >= 16)) return -1;
	if (!vps_id) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_CODING, ("[VVC] VPS ID 0 is forbidden\n"));
		return -1;
	}
	vps = &vvc->vps[vps_id];
	if (!vps->state) {
		vps->id = vps_id;
		vps->state = 1;
	}
	vps->max_layers = 1 + gf_bs_read_int_log(bs, 6, "max_layers");
	if (vps->max_layers > VVC_MAX_LAYERS) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_CODING, ("[VVC] %d layers in VPS but only %d supported in GPAC\n", vps->max_layers, VVC_MAX_LAYERS));
		VVC_VPS_BROKEN
	}
	vps->max_sub_layers = gf_bs_read_int_log(bs, 3, "max_sub_layers_minus1") + 1;

	if ((vps->max_layers>1) && (vps->max_sub_layers>1))
		vps_default_ptl_dpb_hrd_max_tid_flag = gf_bs_read_int_log(bs, 1, "vps_default_ptl_dpb_hrd_max_tid_flag");

	if (vps->max_layers>1)
		vps->all_layers_independent = gf_bs_read_int_log(bs, 1, "all_layers_independent");

	for (i=0; i<vps->max_layers; i++) {
		u32 layer_id = gf_bs_read_int_log_idx(bs, 6, "layer_id", i);
		if (layer_id>vps->max_layer_id) vps->max_layer_id = layer_id;
		if (i && !vps->all_layers_independent) {
			Bool layer_indep = gf_bs_read_int_log_idx(bs, 1, "layer_independent", i);
			if (!layer_indep) {
				Bool vps_max_tid_ref_present_flag = gf_bs_read_int_log_idx(bs, 1, "vps_max_tid_ref_present_flag", i);
				for (j=0; j<i; j++) {
					Bool vps_direct_ref_layer_flag = gf_bs_read_int_log_idx2(bs, 1, "vps_direct_ref_layer_flag", i, j);
					if (vps_max_tid_ref_present_flag && vps_direct_ref_layer_flag) {
						gf_bs_read_int_log_idx2(bs, 3, "vps_max_tid_il_ref_pics_plus1", i, j);
					}
				}
			}
		}
	}
	vps->num_ptl = 1;
	if (vps->max_layers > 1) {
		if (vps->all_layers_independent) {
			vps->each_layer_is_ols = gf_bs_read_int_log(bs, 1, "each_layer_is_ols");
		}
		if (!vps->each_layer_is_ols) {
			u32 vps_ols_mode_idc = 2;
			if (!vps->all_layers_independent) {
				vps_ols_mode_idc = gf_bs_read_int_log(bs, 2, "vps_ols_mode_idc");
			}
			if (vps_ols_mode_idc==2) {
				u8 vps_num_output_layer_sets = 2 + gf_bs_read_int_log(bs, 8, "vps_num_output_layer_sets_minus2");
				for (i=0; i<vps_num_output_layer_sets; i++) {
					for (j=0; j<vps->max_layers; j++) {
						gf_bs_read_int_log_idx2(bs, 1, "vps_ols_output_layer_flag", i, j);
					}
				}
			}
		}
		vps->num_ptl = 1 + gf_bs_read_int_log(bs, 8, "num_ptl_minus1");
	}
	vps->ptl[0].pt_present = 1;
	for (i=0; i<vps->num_ptl; i++) {
		if (i)
			vps->ptl[i].pt_present = gf_bs_read_int_log_idx(bs, 1, "pt_present", i);
		if (!vps_default_ptl_dpb_hrd_max_tid_flag)
			vps->ptl[i].ptl_max_tid = gf_bs_read_int_log_idx(bs, 3, "ptl_max_tid", i);
		else
			vps->ptl[i].ptl_max_tid = vps->max_sub_layers - 1;
	}
	//align
	gf_bs_align(bs);

	for (i=0; i<vps->num_ptl; i++) {
		vvc_profile_tier_level(bs, &vps->ptl[i], i);
	}

	//TODO, parse multilayer stuff


	if (gf_bs_is_overflow(bs))
		VVC_VPS_BROKEN
	return vps_id;
}

static s32 vvc_parse_ref_pic_list_struct(GF_BitStream *bs, VVC_SPS *sps, u32 listIdx, u32 rplsIdx, VVC_RefPicList *rpl)
{
	u32 i;
	memset(rpl, 0, sizeof(VVC_RefPicList));
	rpl->num_ref_entries = gf_bs_read_ue_log_idx2(bs, "num_ref_entries", listIdx, rplsIdx);
	if (rpl->num_ref_entries>=VVC_MAX_REF_PICS) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_CODING, ("[VVC] num_ref_entries %d exceeds maximum allowed value %d\n", rpl->num_ref_entries, VVC_MAX_REF_PICS));
		return -1;
	}

	rpl->ltrp_in_header_flag = 1;
	if (sps->long_term_ref_pics_flag
		&& rplsIdx < sps->num_ref_pic_lists[listIdx]
		&& (rpl->num_ref_entries > 0)
	) {
		rpl->ltrp_in_header_flag = gf_bs_read_int_log_idx2(bs, 1, "ltrp_in_header_flag", listIdx, rplsIdx);
	}
	for (i=0; i < rpl->num_ref_entries; i++) {
		Bool inter_layer_ref_pic_flag = 0;
		if (sps->inter_layer_prediction_enabled_flag) {
			inter_layer_ref_pic_flag = gf_bs_read_int_log_idx3(bs, 1, "inter_layer_ref_pic_flag", listIdx, rplsIdx, i);
		}
		if (!inter_layer_ref_pic_flag) {
			u32 AbsDeltaPocSt;
			Bool st_ref_pic_flag = 1;
			if (sps->long_term_ref_pics_flag) {
				st_ref_pic_flag = gf_bs_read_int_log_idx3(bs, 1, "st_ref_pic_flag", listIdx, rplsIdx, i);
			}
			if (st_ref_pic_flag) {
				u32 abs_delta_poc_st = gf_bs_read_ue_log_idx3(bs, "abs_delta_poc_st", listIdx, rplsIdx, i);

				if ((sps->weighted_pred_flag || sps->weighted_bipred_flag) && (i!=0)) {
					AbsDeltaPocSt = abs_delta_poc_st;
				} else {
					AbsDeltaPocSt = abs_delta_poc_st + 1;
				}
				if (AbsDeltaPocSt>0) {
					gf_bs_read_int_log_idx3(bs, 1, "strp_entry_sign_flag", listIdx, rplsIdx, i);
				}
				rpl->nb_short_term_pictures++;
				rpl->ref_pic_type[i] = VVC_RPL_ST;
			} else if( !rpl->ltrp_in_header_flag) {
				gf_bs_read_int_log_idx3(bs, sps->log2_max_poc_lsb, "rpls_poc_lsb_lt", listIdx, rplsIdx, i);
				rpl->nb_long_term_pictures++;
				rpl->ref_pic_type[i] = VVC_RPL_LT;
			}
		} else {
			gf_bs_read_ue_log_idx3(bs, "ilrp_idx", listIdx, rplsIdx, i);
			rpl->nb_inter_layer_pictures ++;
			rpl->ref_pic_type[i] = VVC_RPL_IL;
		}
	}
	return 0;
}

static void vvc_parse_general_timing_hrd_parameters(GF_BitStream *bs, VVC_SPS *sps, VVC_VPS *vps, Bool *general_nal_hrd_params_present_flag, Bool *general_vcl_hrd_params_present_flag, Bool *general_du_hrd_params_present_flag, u32 *hrd_cpb_cnt_minus1)
{
	sps->has_timing_info = 1;
	sps->num_units_in_tick = gf_bs_read_int_log(bs, 32, "num_units_in_tick");
	sps->time_scale = gf_bs_read_int_log(bs, 32, "timescale");
	*general_du_hrd_params_present_flag = GF_FALSE;
	*general_nal_hrd_params_present_flag = gf_bs_read_int_log(bs, 1, "general_nal_hrd_params_present_flag");
	*general_vcl_hrd_params_present_flag = gf_bs_read_int_log(bs, 1, "general_vcl_hrd_params_present_flag");
	if (*general_nal_hrd_params_present_flag || *general_vcl_hrd_params_present_flag) {
		gf_bs_read_int_log(bs, 1, "general_same_pic_timing_in_all_ols_flag");
		*general_du_hrd_params_present_flag = gf_bs_read_int_log(bs, 1, "general_du_hrd_params_present_flag");
		if (*general_du_hrd_params_present_flag)
			gf_bs_read_int_log(bs, 8, "tick_divisor_minus2");
		gf_bs_read_int_log(bs, 4, "bit_rate_scale");
		gf_bs_read_int_log(bs, 4, "cpb_size_scale");
		if (*general_du_hrd_params_present_flag)
			gf_bs_read_int_log(bs, 4, "cpb_size_du_scale");
		*hrd_cpb_cnt_minus1 = gf_bs_read_ue_log(bs, "hrd_cpb_cnt_minus1");
	}
}

static void vvc_parse_sublayer_hrd_parameters(GF_BitStream *bs, u32 subLayerId, Bool general_du_hrd_params_present_flag, u32 hrd_cpb_cnt_minus1)
{
	u32 j;
	for (j=0; j <= hrd_cpb_cnt_minus1; j++) {
		gf_bs_read_ue_log_idx2(bs, "bit_rate_value_minus1", subLayerId, j);
		gf_bs_read_ue_log_idx2(bs, "cpb_size_value_minus1", subLayerId, j);
		if( general_du_hrd_params_present_flag ) {
			gf_bs_read_ue_log_idx2(bs, "cpb_size_du_value_minus1", subLayerId, j);
			gf_bs_read_ue_log_idx2(bs, "bit_rate_du_value_minus1", subLayerId, j);
		}
		gf_bs_read_int_log_idx2(bs, 1, "cbr_flag", subLayerId, j);
	}
}

static void vvc_parse_ols_timing_hrd_parameters(GF_BitStream *bs, u32 firstSubLayer, u32 MaxSubLayersVal, Bool general_nal_hrd_params_present_flag, Bool general_vcl_hrd_params_present_flag, Bool general_du_hrd_params_present_flag, u32 hrd_cpb_cnt_minus1)
{
	u32 i;
	for (i=firstSubLayer; i<=MaxSubLayersVal; i++) {
		Bool fixed_pic_rate_within_cvs_flag = GF_TRUE;
		if (! gf_bs_read_int_log_idx(bs, 1, "fixed_pic_rate_general_flag", i)) {
			fixed_pic_rate_within_cvs_flag = gf_bs_read_int_log_idx(bs, 1, "fixed_pic_rate_within_cvs_flag", i);
		}
		if (fixed_pic_rate_within_cvs_flag) {
			gf_bs_read_ue_log(bs, "elemental_duration_in_tc_minus1");
		} else if ( (general_nal_hrd_params_present_flag || general_vcl_hrd_params_present_flag)
			&& (hrd_cpb_cnt_minus1 ==0)
		) {
			gf_bs_read_int_log_idx(bs, 1, "low_delay_hrd_flag", i);
		}
		if (general_nal_hrd_params_present_flag) {
			vvc_parse_sublayer_hrd_parameters(bs, i, general_du_hrd_params_present_flag, hrd_cpb_cnt_minus1);
		}
		if (general_vcl_hrd_params_present_flag) {
			vvc_parse_sublayer_hrd_parameters(bs, i, general_du_hrd_params_present_flag, hrd_cpb_cnt_minus1);
		}
	}
}

#define VVC_SPS_BROKEN {\
	memset(sps, 0, sizeof(VVC_SPS)); \
	return -1;\
	}

static u32 vvc_ceillog2(u32 val)
{
	u32 maxBits=0;
	while (val > (u32)(1 << maxBits)) {
		maxBits++;
	}
	return maxBits;
}

static s32 gf_vvc_read_sps_bs_internal(GF_BitStream *bs, VVCState *vvc, u8 layer_id, u32 *vui_flag_pos)
{
	s32 vps_id, sps_id;
	u32 i, CtbSizeY;
	VVC_SPS *sps;
	u8 sps_ptl_dpb_hrd_params_present_flag;

	if (vui_flag_pos) *vui_flag_pos = 0;

	sps_id = gf_bs_read_int_log(bs, 4, "sps_id");
	if ((sps_id<0) || (sps_id >= 16)) {
		return -1;
	}
	vps_id = gf_bs_read_int_log(bs, 4, "vps_id");
	if ((vps_id<0) || (vps_id >= 16)) {
		return -1;
	}
	if (!vps_id && !vvc->vps[0].state) {
		vvc->vps[0].state = 1;
		vvc->vps[0].num_ptl = 1;
		vvc->vps[0].max_layers = 1;
		vvc->vps[0].all_layers_independent = 1;
	}

	sps = &vvc->sps[sps_id];
	if (!sps->state) {
		sps->state = 1;
		sps->id = sps_id;
		sps->vps_id = vps_id;
	}
	sps->max_sublayers = 1 + gf_bs_read_int_log(bs, 3, "max_sublayers_minus1");
	sps->chroma_format_idc = gf_bs_read_int_log(bs, 2, "chroma_format_idc");
	sps->log2_ctu_size = 5 + gf_bs_read_int_log(bs, 2, "log2_ctu_size_minus5");
	CtbSizeY = 1<<sps->log2_ctu_size;

	sps_ptl_dpb_hrd_params_present_flag = gf_bs_read_int_log(bs, 1, "sps_ptl_dpb_hrd_params_present_flag");
	if (sps_ptl_dpb_hrd_params_present_flag) {
		VVC_ProfileTierLevel ptl, *p_ptl;
		if (sps->vps_id) {
			p_ptl = &ptl;
		} else {
			p_ptl = &vvc->vps[0].ptl[0];
		}
		memset(p_ptl, 0, sizeof(VVC_ProfileTierLevel));
		p_ptl->pt_present = 1;
		p_ptl->ptl_max_tid = sps->max_sublayers-1;
		vvc_profile_tier_level(bs, p_ptl, 0);
	}
	sps->gdr_enabled = gf_bs_read_int_log(bs, 1, "gdr_enabled");
	sps->ref_pic_resampling = gf_bs_read_int_log(bs, 1, "ref_pic_resampling");
	if (sps->ref_pic_resampling)
		sps->res_change_in_clvs = gf_bs_read_int_log(bs, 1, "res_change_in_clvs");
	sps->width = gf_bs_read_ue_log(bs, "width");
	sps->height = gf_bs_read_ue_log(bs, "height");
	sps->conf_window = gf_bs_read_int_log(bs, 1, "conformance_window_present_flag");
	if (sps->conf_window) {
		u32 SubWidthC, SubHeightC;
		sps->cw_left = gf_bs_read_ue_log(bs, "conformance_window_left");
		sps->cw_right = gf_bs_read_ue_log(bs, "conformance_window_right");
		sps->cw_top = gf_bs_read_ue_log(bs, "conformance_window_top");
		sps->cw_bottom = gf_bs_read_ue_log(bs, "conformance_window_bottom");


		if (sps->chroma_format_idc == 1) {
			SubWidthC = SubHeightC = 2;
		} else if (sps->chroma_format_idc == 2) {
			SubWidthC = 2;
			SubHeightC = 1;
		} else {
			SubWidthC = SubHeightC = 1;
		}
		sps->width -= SubWidthC * (sps->cw_left + sps->cw_right);
		sps->height -= SubHeightC * (sps->cw_top + sps->cw_bottom);
	}

	sps->subpic_info_present = gf_bs_read_int_log(bs, 1, "subpic_info_present");
	if (sps->subpic_info_present) {
		sps->nb_subpics = 1 + gf_bs_read_ue_log(bs, "nb_subpics_minus1");
		if (sps->nb_subpics>1000) VVC_SPS_BROKEN
		if (sps->nb_subpics>VVC_MAX_SUBPIC) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_CODING, ("[VVC] Maximum subpics supported by GPAC is %u, got %u\n", VVC_MAX_SUBPIC, sps->nb_subpics));
			VVC_SPS_BROKEN
		}

		if (sps->nb_subpics>1) {
			u32 tmpWidthBits, tmpHeightBits;
			sps->independent_subpic_flags = gf_bs_read_int_log(bs, 1, "independent_subpic_flags");
			sps->subpic_same_size = gf_bs_read_int_log(bs, 1, "subpic_same_size");

			tmpWidthBits = vvc_ceillog2((sps->width + CtbSizeY-1) / CtbSizeY);
			tmpHeightBits = vvc_ceillog2((sps->height + CtbSizeY-1) / CtbSizeY);

			for (i=0; i<sps->nb_subpics; i++) {
				VVC_SubpicInfo *sp = &sps->subpics[i];
				sp->id = i;
				if( !sps->subpic_same_size || !i) {
					if (i && (sps->width > CtbSizeY))
						sp->x = gf_bs_read_int_log(bs, tmpWidthBits, "subpic_ctu_top_left_x");
					if (i && (sps->height > CtbSizeY))
						sp->y = gf_bs_read_int_log(bs, tmpHeightBits, "subpic_ctu_top_left_y");
					if ((i+1 < sps->nb_subpics) && (sps->width > CtbSizeY))
						sp->w = 1 + gf_bs_read_int_log(bs, tmpWidthBits, "subpic_width_minus1");
					if ((i+1 < sps->nb_subpics) && (sps->height > CtbSizeY))
						sp->h = 1 + gf_bs_read_int_log(bs, tmpHeightBits, "subpic_height_minus1");
				} else {
					sp->w = sps->subpics[0].w;
					sp->h = sps->subpics[0].h;
				}
				if (!sps->independent_subpic_flags) {
					gf_bs_read_int_log(bs, 1, "subpic_treated_as_pic_flag");
					gf_bs_read_int_log(bs, 1, "loop_filter_across_subpic_enabled_flag");
				}
			}
			sps->subpicid_len = gf_bs_read_ue_log(bs, "subpic_id_len_minus1") + 1;
			sps->subpicid_mapping_explicit = gf_bs_read_int_log(bs, 1, "subpic_id_mapping_explicitly_signalled_flag");
			if (sps->subpicid_mapping_explicit) {
				sps->subpicid_mapping_present = gf_bs_read_int_log(bs, 1, "subpic_id_mapping_present_flag");
				if (sps->subpicid_mapping_present) {
					for (i=0; i<sps->nb_subpics; i++) {
						VVC_SubpicInfo *sp = &sps->subpics[i];
						sp->id = gf_bs_read_int_log_idx(bs, sps->subpicid_len, "subpic_id", i);
					}
				}
			}
		}
	} else {
		sps->nb_subpics = 1;
	}
	sps->bitdepth = gf_bs_read_ue_log(bs, "bitdepth_minus8") + 8;
	sps->entropy_coding_sync_enabled_flag = gf_bs_read_int_log(bs, 1, "entropy_coding_sync_enabled_flag");
	sps->entry_point_offsets_present_flag = gf_bs_read_int_log(bs, 1, "entry_point_offsets_present_flag");
	sps->log2_max_poc_lsb = 4 + gf_bs_read_int_log(bs, 4, "log2_max_poc_lsb_minus4");
	if ((sps->poc_msb_cycle_flag = gf_bs_read_int_log(bs, 1, "poc_msb_cycle_flag")))
		sps->poc_msb_cycle_len = 1 + gf_bs_read_ue_log(bs, "poc_msb_cycle_len_minus1");

	u8 sps_num_extra_ph_bits = 8 * gf_bs_read_int_log(bs, 2, "sps_num_extra_ph_bytes");
	for (i=0; i<sps_num_extra_ph_bits; i++) {
		if (gf_bs_read_int_log_idx(bs, 1, "extra_ph_bit_present_flag", 1))
			sps->ph_num_extra_bits++;
	}
	u8 sps_num_extra_sh_bits = 8 * gf_bs_read_int_log(bs, 2, "num_extra_sh_bytes");
	for (i=0; i<sps_num_extra_sh_bits; i++) {
		if (gf_bs_read_int_log_idx(bs, 1, "extra_sh_bit_present_flag", i))
			sps->sh_num_extra_bits++;
	}

	if (sps_ptl_dpb_hrd_params_present_flag) {
		u8 sps_sublayer_dpb_params_flag = 0;
		if (sps->max_sublayers>1) {
			sps_sublayer_dpb_params_flag = gf_bs_read_int_log(bs, 1, "sps_sublayer_dpb_params_flag");
		}
		for (i=(sps_sublayer_dpb_params_flag ? 0 : sps->max_sublayers-1); i < sps->max_sublayers; i++ ) {
			gf_bs_read_ue_log_idx(bs, "dpb_max_dec_pic_buffering_minus1", i);
			gf_bs_read_ue_log_idx(bs, "dpb_max_num_reorder_pics", i);
			gf_bs_read_ue_log_idx(bs, "dpb_max_latency_increase_plus1", i);
		}
	}
	gf_bs_read_ue_log(bs, "sps_log2_min_luma_coding_block_size_minus2");
	sps->partition_constraints_override_enabled_flag = gf_bs_read_int_log(bs, 1, "sps_partition_constraints_override_enabled_flag");
	gf_bs_read_ue_log(bs, "sps_log2_min_luma_coding_block_size_minus2");
	u8 sps_max_mtt_hierarchy_depth_intra_slice_luma = gf_bs_read_ue_log(bs, "sps_max_mtt_hierarchy_depth_intra_slice_luma");
	if (sps_max_mtt_hierarchy_depth_intra_slice_luma != 0) {
		gf_bs_read_ue_log(bs, "sps_log2_diff_max_bt_min_qt_intra_slice_luma");
		gf_bs_read_ue_log(bs, "sps_log2_diff_max_tt_min_qt_intra_slice_luma");
	}
	u8 sps_qtbtt_dual_tree_intra_flag = 0;
	if (sps->chroma_format_idc) {
		sps_qtbtt_dual_tree_intra_flag = gf_bs_read_int_log(bs, 1, "sps_qtbtt_dual_tree_intra_flag");
	}
	if (sps_qtbtt_dual_tree_intra_flag) {
		gf_bs_read_ue_log(bs, "sps_log2_diff_min_qt_min_cb_intra_slice_chroma");
		u8 sps_max_mtt_hierarchy_depth_intra_slice_chroma = gf_bs_read_ue_log(bs, "sps_max_mtt_hierarchy_depth_intra_slice_chroma");
		if( sps_max_mtt_hierarchy_depth_intra_slice_chroma != 0) {
			gf_bs_read_ue_log(bs, "sps_log2_diff_max_bt_min_qt_intra_slice_chroma");
			gf_bs_read_ue_log(bs, "sps_log2_diff_max_tt_min_qt_intra_slice_chroma");
		}
	}

	gf_bs_read_ue_log(bs, "sps_log2_diff_min_qt_min_cb_inter_slice");
	u8 sps_max_mtt_hierarchy_depth_inter_slice = gf_bs_read_ue_log(bs, "sps_max_mtt_hierarchy_depth_inter_slice");
	if (sps_max_mtt_hierarchy_depth_inter_slice != 0) {
		gf_bs_read_ue_log(bs, "sps_log2_diff_max_bt_min_qt_inter_slice");
		gf_bs_read_ue_log(bs, "sps_log2_diff_max_tt_min_qt_inter_slice");
	}
	u8 max_luma_transform_size_64_flag = 0;
	if (CtbSizeY > 32) {
		max_luma_transform_size_64_flag = gf_bs_read_int_log(bs, 1, "sps_max_luma_transform_size_64_flag");
	}
	sps->transform_skip_enabled_flag = gf_bs_read_int_log(bs, 1, "sps_transform_skip_enabled_flag");

	if (sps->transform_skip_enabled_flag) {
		gf_bs_read_ue_log(bs, "sps_log2_transform_skip_max_size_minus2");
		gf_bs_read_int_log(bs, 1, "sps_bdpcm_enabled_flag");
	}
	if (gf_bs_read_int_log(bs, 1, "sps_mts_enabled_flag")) {
		gf_bs_read_int_log(bs, 1, "sps_explicit_mts_intra_enabled_flag");
		gf_bs_read_int_log(bs, 1, "sps_explicit_mts_inter_enabled_flag");
	}
	Bool lfnst_enabled_flag = gf_bs_read_int_log(bs, 1, "sps_lfnst_enabled_flag");
	sps->joint_cbcr_enabled_flag = 0;
	if (sps->chroma_format_idc) {
		sps->joint_cbcr_enabled_flag = gf_bs_read_int_log(bs, 1, "sps_joint_cbcr_enabled_flag");
		u8 sps_same_qp_table_for_chroma_flag = gf_bs_read_int_log(bs, 1, "sps_same_qp_table_for_chroma_flag");
		u32 numQpTables = sps_same_qp_table_for_chroma_flag ? 1 : (sps->joint_cbcr_enabled_flag ? 3 : 2);
		for (i=0; i<numQpTables; i++) {
			gf_bs_read_se_log_idx(bs, "sps_qp_table_start_minus26", i);
			u32 j, sps_num_points_in_qp_table = 1 + gf_bs_read_ue_log_idx(bs, "sps_num_points_in_qp_table_minus1", i);
			for (j=0; j<sps_num_points_in_qp_table; j++) {
				gf_bs_read_ue_log_idx2(bs, "sps_delta_qp_in_val_minus1", i, j);
				gf_bs_read_ue_log_idx2(bs, "sps_delta_qp_diff_val", i, j);
			}
		}
	}
	sps->sao_enabled_flag = gf_bs_read_int_log(bs, 1, "sps_sao_enabled_flag");
	sps->alf_enabled_flag = gf_bs_read_int_log(bs, 1, "sps_alf_enabled_flag");
	if (sps->alf_enabled_flag && sps->chroma_format_idc) {
		sps->ccalf_enabled_flag = gf_bs_read_int_log(bs, 1, "sps_ccalf_enabled_flag");
	}
	sps->lmcs_enabled_flag = gf_bs_read_int_log(bs, 1, "sps_lmcs_enabled_flag");
	sps->weighted_pred_flag = gf_bs_read_int_log(bs, 1, "sps_weighted_pred_flag");
	sps->weighted_bipred_flag = gf_bs_read_int_log(bs, 1, "sps_weighted_bipred_flag");
	sps->long_term_ref_pics_flag = gf_bs_read_int_log(bs, 1, "sps_long_term_ref_pics_flag");
	if (sps->vps_id>0)
		sps->inter_layer_prediction_enabled_flag = gf_bs_read_int_log(bs, 1, "sps_inter_layer_prediction_enabled_flag");
	sps->idr_rpl_present_flag = gf_bs_read_int_log(bs, 1, "sps_idr_rpl_present_flag");
	u32 sps_rpl1_same_as_rpl0 = gf_bs_read_int_log(bs, 1, "sps_rpl1_same_as_rpl0_flag") ? 1: 2;
	for (i=0; i<sps_rpl1_same_as_rpl0; i++) {
		u32 j;
		sps->num_ref_pic_lists[i] = gf_bs_read_ue_log_idx(bs, "sps_num_ref_pic_lists", i);
		if (sps->num_ref_pic_lists[i] > 64) VVC_SPS_BROKEN

		for (j=0; j<sps->num_ref_pic_lists[i]; j++) {
			s32 res = vvc_parse_ref_pic_list_struct(bs, sps, i, j, &sps->rps[i][j]);
			if (res<0) VVC_SPS_BROKEN
		}
	}
	gf_bs_read_int_log(bs, 1, "sps_ref_wraparound_enabled_flag");
	sps->temporal_mvp_enabled_flag = gf_bs_read_int_log(bs, 1, "sps_temporal_mvp_enabled_flag");
	if (sps->temporal_mvp_enabled_flag) {
		gf_bs_read_int_log(bs, 1, "sps_sbtmvp_enabled_flag");
	}
	Bool amvr_enabled = gf_bs_read_int_log(bs, 1, "sps_amvr_enabled_flag");
	sps->bdof_control_present_in_ph_flag = 0;
	if (gf_bs_read_int_log(bs, 1, "sps_bdof_enabled_flag")) {
		sps->bdof_control_present_in_ph_flag = gf_bs_read_int_log(bs, 1, "sps_bdof_control_present_in_ph_flag");
	}
	gf_bs_read_int_log(bs, 1, "sps_smvd_enabled_flag");
	sps->dmvr_control_present_in_ph_flag = 0;
	if (gf_bs_read_int_log(bs, 1, "sps_dmvr_enabled_flag")) {
		sps->dmvr_control_present_in_ph_flag = gf_bs_read_int_log(bs, 1, "sps_dmvr_control_present_in_ph_flag");
	}
	sps->mmvd_fullpel_only_enabled_flag = 0;
	if (gf_bs_read_int_log(bs, 1, "sps_mmvd_enabled_flag")) {
		sps->mmvd_fullpel_only_enabled_flag = gf_bs_read_int_log(bs, 1, "sps_mmvd_fullpel_only_enabled_flag");
	}
	u32 MaxNumMergeCand  = 6 - gf_bs_read_ue_log(bs, "sps_six_minus_max_num_merge_cand");

	sps->prof_control_present_in_ph_flag = 0;
	gf_bs_read_int_log(bs, 1, "sps_sbt_enabled_flag");
	if (gf_bs_read_int_log(bs, 1, "sps_affine_enabled_flag")) {
		gf_bs_read_ue_log(bs, "sps_five_minus_max_num_subblock_merge_cand");
		gf_bs_read_int_log(bs, 1, "sps_6param_affine_enabled_flag");
		if (amvr_enabled) {
			gf_bs_read_int_log(bs, 1, "sps_affine_amvr_enabled_flag");
		}
		if (gf_bs_read_int_log(bs, 1, "sps_affine_prof_enabled_flag")) {
			sps->prof_control_present_in_ph_flag = gf_bs_read_int_log(bs, 1, "sps_prof_control_present_in_ph_flag");
		}
	}

	gf_bs_read_int_log(bs, 1, "sps_bcw_enabled_flag");
	gf_bs_read_int_log(bs, 1, "sps_ciip_enabled_flag");
	if (MaxNumMergeCand >= 2) {
		Bool gpm_enabled_flag = gf_bs_read_int_log(bs, 1, "sps_gpm_enabled_flag");
		if (gpm_enabled_flag && (MaxNumMergeCand >= 3)) {
			gf_bs_read_ue_log(bs, "sps_max_num_merge_cand_minus_max_num_gpm_cand");
		}
	}
	gf_bs_read_ue_log(bs, "sps_log2_parallel_merge_level_minus2");

	gf_bs_read_int_log(bs, 1, "sps_isp_enabled_flag");
	gf_bs_read_int_log(bs, 1, "sps_mrl_enabled_flag");
	gf_bs_read_int_log(bs, 1, "sps_mip_enabled_flag");
	if (sps->chroma_format_idc != 0) {
		gf_bs_read_int_log(bs, 1, "sps_cclm_enabled_flag");
	}
	if (sps->chroma_format_idc == 1) {
		gf_bs_read_int_log(bs, 1, "sps_chroma_horizontal_collocated_flag");
		gf_bs_read_int_log(bs, 1, "sps_chroma_vertical_collocated_flag");
	}
	Bool act_enabled_flag = GF_FALSE;
	Bool palette_enabled_flag = gf_bs_read_int_log(bs, 1, "sps_palette_enabled_flag");
	if ((sps->chroma_format_idc == 3) && !max_luma_transform_size_64_flag) {
		act_enabled_flag = gf_bs_read_int_log(bs, 1, "sps_act_enabled_flag");
	}
	if (sps->transform_skip_enabled_flag || palette_enabled_flag) {
		gf_bs_read_ue_log(bs, "sps_min_qp_prime_ts");
	}
	if (gf_bs_read_int_log(bs, 1, "sps_ibc_enabled_flag")) {
		gf_bs_read_ue_log(bs, "sps_six_minus_max_num_ibc_merge_cand");
	}
	if (gf_bs_read_int_log(bs, 1, "sps_ladf_enabled_flag")) {
		u32 num_ladf_intervals_minus2 = gf_bs_read_int_log(bs, 2, "sps_num_ladf_intervals_minus2");
		gf_bs_read_se_log(bs, "sps_ladf_lowest_interval_qp_offset");
		for (i=0; i<num_ladf_intervals_minus2+1; i++) {
			gf_bs_read_se_log_idx(bs, "sps_ladf_qp_offset", i);
			gf_bs_read_ue_log_idx(bs, "sps_ladf_delta_threshold_minus1", i);
		}
	}
	sps->explicit_scaling_list_enabled_flag = gf_bs_read_int_log(bs, 1, "sps_explicit_scaling_list_enabled_flag");
	if (lfnst_enabled_flag && sps->explicit_scaling_list_enabled_flag) {
		gf_bs_read_int_log(bs, 1, "sps_scaling_matrix_for_lfnst_disabled_flag");
	}
	Bool scaling_matrix_for_alternative_colour_space_disabled_flag = 0;
	if (act_enabled_flag && sps->explicit_scaling_list_enabled_flag) {
		scaling_matrix_for_alternative_colour_space_disabled_flag = gf_bs_read_int_log(bs, 1, "sps_scaling_matrix_for_alternative_colour_space_disabled_flag");
	}
	if (scaling_matrix_for_alternative_colour_space_disabled_flag) {
		gf_bs_read_int_log(bs, 1, "sps_scaling_matrix_designated_colour_space_flag");
	}
	sps->dep_quant_enabled_flag = gf_bs_read_int_log(bs, 1, "sps_dep_quant_enabled_flag");
	sps->sign_data_hiding_enabled_flag = gf_bs_read_int_log(bs, 1, "sps_sign_data_hiding_enabled_flag");
	sps->virtual_boundaries_enabled_flag = gf_bs_read_int_log(bs, 1, "sps_virtual_boundaries_enabled_flag");
	if (sps->virtual_boundaries_enabled_flag) {
		sps->virtual_boundaries_present_flag = gf_bs_read_int_log(bs, 1, "sps_virtual_boundaries_present_flag");
		if (sps->virtual_boundaries_present_flag) {
			u32 num_virtual_boundaries = gf_bs_read_ue_log(bs, "sps_num_ver_virtual_boundaries");
			for (i=0; i<num_virtual_boundaries; i++) {
				gf_bs_read_ue_log_idx(bs, "sps_virtual_boundary_pos_x_minus1", i);
			}
			num_virtual_boundaries = gf_bs_read_ue_log(bs, "sps_num_hor_virtual_boundaries");
			for (i=0; i<num_virtual_boundaries; i++) {
				gf_bs_read_ue_log_idx(bs, "sps_virtual_boundary_pos_y_minus1", i);
			}
		}
	}

	if (sps_ptl_dpb_hrd_params_present_flag) {
		if (gf_bs_read_int_log(bs, 1, "sps_timing_hrd_params_present_flag")) {
			Bool general_nal_hrd_params_present_flag, general_vcl_hrd_params_present_flag, general_du_hrd_params_present_flag;
			u32 hrd_cpb_cnt_minus1=0;
			u32 sublayer_cpb_params_present_flag = 0;
			vvc_parse_general_timing_hrd_parameters(bs, sps, NULL, &general_nal_hrd_params_present_flag, &general_vcl_hrd_params_present_flag, &general_du_hrd_params_present_flag, &hrd_cpb_cnt_minus1);
			if (sps->max_sublayers > 1) {
				sublayer_cpb_params_present_flag = gf_bs_read_int_log(bs, 1, "sps_sublayer_cpb_params_present_flag");
			}
			u32 firstSubLayer = sublayer_cpb_params_present_flag ? 0 : sps->max_sublayers - 1;
			vvc_parse_ols_timing_hrd_parameters(bs, firstSubLayer, sps->max_sublayers-1, general_nal_hrd_params_present_flag, general_vcl_hrd_params_present_flag, general_du_hrd_params_present_flag, hrd_cpb_cnt_minus1);

		}
	}

	gf_bs_read_int_log(bs, 1, "sps_field_seq_flag");
	if (vui_flag_pos) {
		*vui_flag_pos = (u32)gf_bs_get_bit_offset(bs);
	}
	//all this to get to VUI !!!
	if (gf_bs_read_int_log(bs, 1, "sps_vui_parameters_present_flag")) {
		gf_bs_read_ue_log(bs, "sps_vui_payload_size_minus1");
		while (!gf_bs_is_align(bs)) {
			gf_bs_read_int_log(bs, 1, "sps_vui_alignment_zero_bit");
		}
		//vui parameters
		Bool vui_progressive_source_flag = gf_bs_read_int_log(bs, 1, "vui_progressive_source_flag");
		Bool vui_interlaced_source_flag = gf_bs_read_int_log(bs, 1, "vui_interlaced_source_flag");
		gf_bs_read_int_log(bs, 1, "vui_non_packed_constraint_flag");
		gf_bs_read_int_log(bs, 1, "vui_non_projected_constraint_flag");
		sps->aspect_ratio_info_present_flag = gf_bs_read_int_log(bs, 1, "vui_aspect_ratio_info_present_flag");
		if (sps->aspect_ratio_info_present_flag) {
			gf_bs_read_int_log(bs, 1, "vui_aspect_ratio_constant_flag");
			sps->sar_idc = gf_bs_read_int_log(bs, 8, "vui_aspect_ratio_idc");
			if (sps->sar_idc== 0xFF) {
				sps->sar_width = gf_bs_read_int_log(bs, 16, "vui_sar_width");
				sps->sar_height = gf_bs_read_int_log(bs, 16, "vui_sar_height");
			}
		}
		sps->overscan_info_present_flag = gf_bs_read_int_log(bs, 1, "vui_overscan_info_present_flag");
		if (sps->overscan_info_present_flag) {
			gf_bs_read_int_log(bs, 1, "vui_overscan_appropriate_flag");
		}
		sps->colour_description_present_flag = gf_bs_read_int_log(bs, 1, "vui_colour_description_present_flag");
		if (sps->colour_description_present_flag) {
			sps->colour_primaries = gf_bs_read_int_log(bs, 8, "vui_colour_primaries");
			sps->transfer_characteristics = gf_bs_read_int_log(bs, 8, "vui_transfer_characteristics");
			sps->matrix_coefficients = gf_bs_read_int_log(bs, 8, "vui_matrix_coeffs");
			sps->video_full_range_flag = gf_bs_read_int_log(bs, 1, "vui_full_range_flag");
		}
		if (gf_bs_read_int_log(bs, 1, " vui_chroma_loc_info_present_flag")) {
			if (vui_progressive_source_flag && !vui_interlaced_source_flag) {
				gf_bs_read_ue_log(bs, "vui_chroma_sample_loc_type_frame");
			} else {
				gf_bs_read_ue_log(bs, "vui_chroma_sample_loc_type_top_field");
				gf_bs_read_ue_log(bs, "vui_chroma_sample_loc_type_bottom_field");
			}
		}
		//WE DON'T PARSE vui_payload_bit_equal_to_one because we dont parse the rest (sps extensions)
		//if needed, see rewrite_vui code
	}

	if (gf_bs_is_overflow(bs))
		VVC_SPS_BROKEN
	return sps_id;
}

#define VVC_PPS_BROKEN {\
	memset(pps, 0, sizeof(VVC_PPS)); \
	return -1;\
	}


static s32 gf_vvc_read_pps_bs_internal(GF_BitStream *bs, VVCState *vvc)
{
	u32 i;
	s32 pps_id;
	VVC_PPS *pps;

	//NAL header already read
	pps_id = gf_bs_read_int_log(bs, 6, "pps_id");

	if ((pps_id < 0) || (pps_id >= 64)) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_CODING, ("[VVC] wrong PPS ID %d in PPS\n", pps_id));
		return -1;
	}
	pps = &vvc->pps[pps_id];

	if (!pps->state) {
		pps->id = pps_id;
		pps->state = 1;
	}
	pps->sps_id = gf_bs_read_int_log(bs, 4, "sps_id");
	if (((s32)pps->sps_id<0) || (pps->sps_id >= 16)) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_CODING, ("[VVC] wrong SPS ID %d in PPS\n", pps->sps_id));
		VVC_PPS_BROKEN
	}
	vvc->sps_active_idx = pps->sps_id; /*set active sps*/
	pps->mixed_nal_types = gf_bs_read_int_log(bs, 1, "mixed_nal_types");
	pps->width = gf_bs_read_ue_log(bs, "width");
	pps->height = gf_bs_read_ue_log(bs, "height");
	pps->conf_window = gf_bs_read_int_log(bs, 1, "conformance_window_flag");
	if (pps->conf_window) {
		pps->cw_left = gf_bs_read_ue_log(bs, "conf_win_left_offset");
		pps->cw_right = gf_bs_read_ue_log(bs, "conf_win_right_offset");
		pps->cw_top = gf_bs_read_ue_log(bs, "conf_win_top_offset");
		pps->cw_bottom = gf_bs_read_ue_log(bs, "conf_win_bottom_offset");
	}
	//scaling window
	if (gf_bs_read_int_log(bs, 1, "scaling_window_explicit_signaling_flag")) {
		gf_bs_read_se_log(bs, "scaling_win_left_offset");
		gf_bs_read_se_log(bs, "scaling_win_right_offset");
		gf_bs_read_se_log(bs, "scaling_win_top_offset");
		gf_bs_read_se_log(bs, "scaling_win_bottom_offset");
	}
	pps->output_flag_present_flag = gf_bs_read_int_log(bs, 1, "output_flag_present_flag");
	pps->no_pic_partition_flag = gf_bs_read_int_log(bs, 1, "no_pic_partition_flag");
	pps->subpic_id_mapping_present_flag = gf_bs_read_int_log(bs, 1, "subpic_id_mapping_present_flag");

	VVC_SPS *sps = &vvc->sps[pps->sps_id];
	memcpy(pps->subpics, sps->subpics, sizeof(VVC_SubpicInfo)*sps->nb_subpics);

	u32 pps_num_subpics = 1;
	if (pps->subpic_id_mapping_present_flag) {
		u32 pps_subpic_id_len;
		if (!pps->no_pic_partition_flag) {
			pps_num_subpics = 1+gf_bs_read_ue_log(bs, "pps_num_subpics_minus1");
			if (pps_num_subpics != sps->nb_subpics) {
				GF_LOG(GF_LOG_ERROR, GF_LOG_CODING, ("[VVC] wrong number of subpictures %u in PPS vs %u in SPS\n", pps_num_subpics, sps->nb_subpics));
				VVC_PPS_BROKEN
			}
		}
		pps_subpic_id_len = 1 + gf_bs_read_ue(bs);
		for (i=0; i<pps_num_subpics; i++) {
			VVC_SubpicInfo *sp = &pps->subpics[i];
			sp->id = gf_bs_read_int_log_idx(bs, pps_subpic_id_len, "subpic_id", i);
		}
	}
	pps->single_slice_per_subpic_flag = 1;
	pps->num_slices_in_pic = 1;

	pps->num_tiles_in_pic = 0;
	if (!pps->no_pic_partition_flag) {
		u32 ctu_size = 5 + gf_bs_read_int_log(bs, 2, "pps_log2_ctu_size_minus5");
		u32 num_exp_tile_columns = 1 + gf_bs_read_ue_log(bs, "num_exp_tile_columns_minus1");
		u32 num_exp_tile_rows = 1 + gf_bs_read_ue_log(bs, "num_exp_tile_rows_minus1");

		if (num_exp_tile_columns > VVC_MAX_TILE_COLS) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_CODING, ("[VVC] wrong num tile columns %d in PPS\n", num_exp_tile_columns));
			VVC_PPS_BROKEN
		}
		if (num_exp_tile_rows > VVC_MAX_TILE_ROWS) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_CODING, ("[VVC] wrong num tile rows %d in PPS\n", num_exp_tile_rows));
			VVC_PPS_BROKEN
		}

		ctu_size = 1<<ctu_size;
		pps->pic_width_in_ctbsY = pps->width / ctu_size;
		if (pps->pic_width_in_ctbsY * ctu_size < pps->width) pps->pic_width_in_ctbsY++;
		pps->pic_height_in_ctbsY = pps->height / ctu_size;
		if (pps->pic_height_in_ctbsY * ctu_size < pps->height) pps->pic_height_in_ctbsY++;

		u32 nb_ctb_left = pps->pic_width_in_ctbsY;
		pps->num_tile_cols=0;
		u32 nb_ctb_last=0;
		for (i=0; i<num_exp_tile_columns; i++) {
			u32 nb_ctb_width = 1 + gf_bs_read_ue_log_idx(bs, "tile_column_width_minus1", i);
			if (nb_ctb_left < nb_ctb_width) {
				VVC_PPS_BROKEN
			}
			nb_ctb_left -= nb_ctb_width;
			pps->tile_cols_width_ctb[i] = nb_ctb_width;
			nb_ctb_last = nb_ctb_width;
			pps->num_tile_cols++;
			if (pps->num_tile_cols > VVC_MAX_TILE_COLS) {
				VVC_PPS_BROKEN
			}
		}
		u32 uni_size_ctb = nb_ctb_last;
		while (nb_ctb_left >= uni_size_ctb) {
			nb_ctb_left -= uni_size_ctb;
			if (pps->num_tile_cols >= VVC_MAX_TILE_COLS) {
				VVC_PPS_BROKEN
			}
			pps->tile_cols_width_ctb[pps->num_tile_cols] = uni_size_ctb;
			pps->num_tile_cols++;
		}
		if (nb_ctb_left>0) {
			if (pps->num_tile_cols >= VVC_MAX_TILE_COLS) {
				VVC_PPS_BROKEN
			}
			pps->tile_cols_width_ctb[pps->num_tile_cols] = nb_ctb_left;
			pps->num_tile_cols++;
		}

		nb_ctb_left = pps->pic_height_in_ctbsY;
		nb_ctb_last=0;
		pps->num_tile_rows=0;
		for (i=0; i<num_exp_tile_rows; i++) {
			u32 nb_ctb_height = 1 + gf_bs_read_ue_log_idx(bs, "tile_row_height_minus1", i);
			if (nb_ctb_left < nb_ctb_height) {
				VVC_PPS_BROKEN
			}
			nb_ctb_left -= nb_ctb_height;
			pps->tile_rows_height_ctb[i] = nb_ctb_height;
			pps->num_tile_rows++;
			nb_ctb_last = nb_ctb_height;
			if (pps->num_tile_rows > VVC_MAX_TILE_ROWS) {
				VVC_PPS_BROKEN
			}
		}
		uni_size_ctb = nb_ctb_last;
		while (nb_ctb_left >= uni_size_ctb) {
			nb_ctb_left -= uni_size_ctb;
			if (pps->num_tile_rows >= VVC_MAX_TILE_ROWS) {
				VVC_PPS_BROKEN
			}
			pps->tile_rows_height_ctb[pps->num_tile_rows] = uni_size_ctb;
			pps->num_tile_rows++;
		}
		if (nb_ctb_left>0) {
			if (pps->num_tile_rows >= VVC_MAX_TILE_ROWS) {
				VVC_PPS_BROKEN
			}
			pps->tile_rows_height_ctb[pps->num_tile_rows] = nb_ctb_left;
			pps->num_tile_rows++;
		}

		pps->num_tiles_in_pic = pps->num_tile_cols * pps->num_tile_rows;
		pps->slice_address_len = vvc_ceillog2(pps->num_tiles_in_pic);
		if (pps->num_tiles_in_pic > 1) {
			gf_bs_read_int_log(bs, 1, "pps_loop_filter_across_tiles_enabled_flag");
			pps->rect_slice_flag = gf_bs_read_int_log(bs, 1, "pps_rect_slice_flag");
		}

		if (pps->rect_slice_flag) {
			pps->single_slice_per_subpic_flag = gf_bs_read_int_log(bs, 1, "pps_single_slice_per_subpic_flag");
			pps->num_slices_in_pic = pps_num_subpics;
		}

		if (pps->rect_slice_flag && !pps->single_slice_per_subpic_flag) {
			pps->num_slices_in_pic = 1 + gf_bs_read_ue_log(bs, "pps_num_slices_in_pic_minus1");
			u8 tile_idx_delta_present_flag = 0;
			if (pps->num_slices_in_pic > 1) {
				tile_idx_delta_present_flag = gf_bs_read_int_log(bs, 1, "pps_tile_idx_delta_present_flag");
			}
			for (i=0; i<pps->num_slices_in_pic-1; i++) {
				//TODO FIXME we assume single slice per tile
				u32 SliceTopLeftTileIdx=0;
				u32 RowHeightVal=0;


				u32 slice_width_in_tiles_minus1=0;
				u32 slice_height_in_tiles_minus1=0;
				if (SliceTopLeftTileIdx % pps->num_tile_cols != pps->num_tile_cols - 1) {
					slice_width_in_tiles_minus1 = gf_bs_read_ue_log_idx(bs, "pps_slice_width_in_tiles_minus1", i);
				}

				if ( (SliceTopLeftTileIdx / pps->num_tile_cols != pps->num_tile_rows - 1)
					&& (tile_idx_delta_present_flag || (SliceTopLeftTileIdx % pps->num_tile_cols == 0) )
				) {
					slice_height_in_tiles_minus1 = gf_bs_read_ue_log_idx(bs, "pps_slice_height_in_tiles_minus1", i);
				}

				if (!slice_width_in_tiles_minus1 && !slice_height_in_tiles_minus1 && (RowHeightVal > 1 )
				) {
					u32 j, num_exp_slices_in_tile = gf_bs_read_ue_log_idx(bs, "pps_num_exp_slices_in_tile", i);
					for (j=0; j<num_exp_slices_in_tile; j++) {
						gf_bs_read_ue_log_idx2 (bs, "pps_exp_slice_height_in_ctus_minus1", i, j);
					}
					//TODO FIXME i += NumSlicesInTile[ i ] − 1
				}

				if (tile_idx_delta_present_flag && (i < pps->num_slices_in_pic)) {
					gf_bs_read_se_log_idx(bs, "pps_tile_idx_delta_val", i);
				}

			}
		}
		if (!pps->rect_slice_flag || pps->single_slice_per_subpic_flag || (pps->num_slices_in_pic > 1)) {
			gf_bs_read_int_log(bs, 1, "pps_loop_filter_across_slices_enabled_flag");
		}
	}

	//update subpic info - TODO, for now we assume single slice per subpic
	if (sps->nb_subpics>1) {
		for (i=0; i<pps_num_subpics; i++) {
			VVC_SubpicInfo *sp = &pps->subpics[i];
			sp->num_slices=1;
		}
	}

	pps->cabac_init_present_flag = gf_bs_read_int_log(bs, 1, "pps_cabac_init_present_flag");
	for (i=0; i<2; i++) {
		pps->num_ref_idx_default_active[i] = 1 + gf_bs_read_ue_log_idx(bs, "pps_num_ref_idx_default_active_minus1", i);
	}
	pps->rpl1_idx_present_flag = gf_bs_read_int_log(bs, 1, "pps_rpl1_idx_present_flag");
	pps->weighted_pred_flag = gf_bs_read_int_log(bs, 1, "pps_weighted_pred_flag");
	pps->weighted_bipred_flag = gf_bs_read_int_log(bs, 1, "pps_weighted_bipred_flag");
	if (gf_bs_read_int_log(bs, 1, "pps_ref_wraparound_enabled_flag")) {
		gf_bs_read_ue_log(bs, "pps_pic_width_minus_wraparound_offset");
	}
	gf_bs_read_se_log(bs, "pps_init_qp_minus26");
	pps->cu_qp_delta_enabled_flag = gf_bs_read_int_log(bs, 1, "pps_cu_qp_delta_enabled_flag");
	pps->slice_chroma_qp_offsets_present_flag = 0;
	pps->chroma_tool_offsets_present_flag = gf_bs_read_int_log(bs, 1, "pps_chroma_tool_offsets_present_flag");
	if (pps->chroma_tool_offsets_present_flag) {
		gf_bs_read_se_log(bs, "pps_cb_qp_offset");
		gf_bs_read_se_log(bs, "pps_cr_qp_offset");
		u8 joint_cbcr_qp_offset_present_flag = gf_bs_read_int_log(bs, 1, "pps_joint_cbcr_qp_offset_present_flag");
		if (joint_cbcr_qp_offset_present_flag) {
			gf_bs_read_se_log(bs, "pps_joint_cbcr_qp_offset_value");
		}
		pps->slice_chroma_qp_offsets_present_flag = gf_bs_read_int_log(bs, 1, "pps_slice_chroma_qp_offsets_present_flag");
		pps->cu_chroma_qp_offset_list_enabled_flag = gf_bs_read_int_log(bs, 1, "pps_cu_chroma_qp_offset_list_enabled_flag");
		if (pps->cu_chroma_qp_offset_list_enabled_flag) {
			u32 pps_chroma_qp_offset_list_len = 1 + gf_bs_read_ue_log(bs, "pps_chroma_qp_offset_list_len_minus1");
			for (i=0; i<pps_chroma_qp_offset_list_len; i++) {
				gf_bs_read_se_log_idx(bs, "pps_cb_qp_offset_list", i);
				gf_bs_read_se_log_idx(bs, "pps_cr_qp_offset_list", i);
				if (joint_cbcr_qp_offset_present_flag) {
					gf_bs_read_se_log_idx(bs, "pps_joint_cbcr_qp_offset_list", i);
				}
			}
		}
	}

	pps->dbf_info_in_ph_flag = 0;
	pps->deblocking_filter_disabled_flag = 0;
	pps->deblocking_filter_override_enabled_flag = 0;
	if (gf_bs_read_int_log(bs, 1, "pps_deblocking_filter_control_present_flag")) {
		pps->deblocking_filter_override_enabled_flag = gf_bs_read_int_log(bs, 1, "pps_deblocking_filter_override_enabled_flag");
		pps->deblocking_filter_disabled_flag = gf_bs_read_int_log(bs, 1, "pps_deblocking_filter_disabled_flag");

		if (!pps->no_pic_partition_flag && pps->deblocking_filter_override_enabled_flag) {
			pps->dbf_info_in_ph_flag = gf_bs_read_int_log(bs, 1, "pps_dbf_info_in_ph_flag");
		}
		if (!pps->deblocking_filter_disabled_flag) {
			gf_bs_read_se_log(bs, "pps_luma_beta_offset_div2");
			gf_bs_read_se_log(bs, "pps_luma_tc_offset_div2");
			if (pps->chroma_tool_offsets_present_flag) {
				gf_bs_read_se_log(bs, "pps_cb_beta_offset_div2");
				gf_bs_read_se_log(bs, "pps_cb_tc_offset_div2");
				gf_bs_read_se_log(bs, "pps_cr_beta_offset_div2");
				gf_bs_read_se_log(bs, "pps_cr_tc_offset_div2");
			}
		}
	}
	pps->wp_info_in_ph_flag = 0;
	pps->qp_delta_info_in_ph_flag = 0;
	pps->sao_info_in_ph_flag = 0;
	if (!pps->no_pic_partition_flag) {
		pps->rpl_info_in_ph_flag = gf_bs_read_int_log(bs, 1, "pps_rpl_info_in_ph_flag");
		pps->sao_info_in_ph_flag = gf_bs_read_int_log(bs, 1, "pps_sao_info_in_ph_flag");
		pps->alf_info_in_ph_flag = gf_bs_read_int_log(bs, 1, "pps_alf_info_in_ph_flag");
		if ( (pps->weighted_pred_flag || pps->weighted_bipred_flag) && pps->rpl_info_in_ph_flag) {
			pps->wp_info_in_ph_flag = gf_bs_read_int_log(bs, 1, "pps_wp_info_in_ph_flag");
		}
		pps->qp_delta_info_in_ph_flag = gf_bs_read_int_log(bs, 1, "pps_qp_delta_info_in_ph_flag");
	}
	pps->picture_header_extension_present_flag = gf_bs_read_int_log(bs, 1, "pps_picture_header_extension_present_flag");
	pps->slice_header_extension_present_flag = gf_bs_read_int_log(bs, 1, "pps_slice_header_extension_present_flag");

	//TODO
	if (gf_bs_read_int_log(bs, 1, "pps_extension_flag")) {
		//while ( more_rbsp_data()) bit(1);
	}
	//rbsp_trailing_bits()

	if (gf_bs_is_overflow(bs))
		VVC_PPS_BROKEN
	return pps_id;
}


static s32 vvc_parse_ref_pic_lists(GF_BitStream *bs, VVCSliceInfo *si, Bool is_pic_header)
{
	u32 i;
	s32 *p_rpl_idx = is_pic_header ? &si->ph_rpl_idx[0] : &si->rpl_idx[0];

	u8 rpl_sps_flag_prev=0;
	for (i=0; i<2; i++) {
		VVC_RefPicList *rpl=NULL;
		u32 j;
		u8 rpl_sps_flag=0;
		u32 rpl_idx = 0;
		if ((si->sps->num_ref_pic_lists[i]>0) && (!i || si->pps->rpl1_idx_present_flag)) {
			rpl_sps_flag = gf_bs_read_int_log_idx(bs, 1, "rpl_sps_flag", i);
		}
		/*
		When rpl_sps_flag[ i ] is not present, it is inferred as follows:
		⎯ If sps_num_ref_pic_lists[ i ] is equal to 0, the value of rpl_sps_flag[ i ] is inferred to be equal to 0.
		⎯ Otherwise (sps_num_ref_pic_lists[ i ] is greater than 0), when pps_rpl1_idx_present_flag is equal to 0 and i is equal to 1, the value of rpl_sps_flag[ 1 ] is inferred to be equal to rpl_sps_flag[ 0 ].
		*/
		else {
			if (si->sps->num_ref_pic_lists[i]==0) {
				rpl_sps_flag = 0;
			} else {
				rpl_sps_flag = rpl_sps_flag_prev;
			}
		}
		rpl_sps_flag_prev = rpl_sps_flag;
		if (is_pic_header) {
			rpl = &si->ph_rpl[i];
		} else {
			rpl = &si->rpl[i];
		}

		if (rpl_sps_flag) {
			if ((si->sps->num_ref_pic_lists[i]>1) && (!i || si->pps->rpl1_idx_present_flag)) {
				u32 nb_bits =  vvc_ceillog2(si->sps->num_ref_pic_lists[i]);
				rpl_idx = gf_bs_read_int_log_idx(bs, nb_bits, "rpl_idx", i);
			}
			else if (si->sps->num_ref_pic_lists[i] == 1) {
				rpl_idx = 0;
			} else {
				gf_assert(p_rpl_idx[0] != -1);
				rpl_idx = p_rpl_idx[0];
			}
			p_rpl_idx[i] = rpl_idx;
			if (rpl_idx>=VVC_MAX_REF_PICS) {
				GF_LOG(GF_LOG_WARNING, GF_LOG_CODING, ("[VVC] Picture header RplIdx %d greater than max allowed %d\n", rpl_idx, VVC_MAX_REF_PICS));
				return -1;
			}

			memcpy(rpl, &si->sps->rps[i][rpl_idx], sizeof(VVC_RefPicList));

		} else {
			s32 res = vvc_parse_ref_pic_list_struct(bs, si->sps, i, si->sps->num_ref_pic_lists[i], rpl);
			if (res<0) return res;
			p_rpl_idx[i] = -1;
		}

		if (rpl->nb_long_term_pictures) {
			for (j=0; j<rpl->num_ref_entries; j++) {
				if (rpl->ref_pic_type[j] != VVC_RPL_LT) continue;

				if (rpl->ltrp_in_header_flag) {
					gf_bs_read_int_log_idx2(bs, si->sps->log2_max_poc_lsb, "poc_lsb_lt", i, j);
				}
				if (gf_bs_read_int_log_idx2(bs, 1, "delta_poc_msb_cycle_present_flag", i, j)) {
					gf_bs_read_ue_log_idx2(bs, "delta_poc_msb_cycle_lt", i, j);
				}
			}
		}
	}
	return 0;
}

static s32 vvc_pred_weight_table(GF_BitStream *bs, VVCState *vvc, VVCSliceInfo *si, VVC_PPS *pps, VVC_SPS *sps, u32 *num_ref_idx_active)
{
	u32 i, num_weights;
	u8 weights[VVC_MAX_REF_PICS];
	gf_bs_read_ue_log(bs, "luma_log2_weight_denom");
	if (sps->chroma_format_idc) {
		gf_bs_read_se_log(bs, "delta_chroma_log2_weight_denom");
	}
	if (pps->wp_info_in_ph_flag) {
		num_weights = gf_bs_read_ue_log(bs, "num_l0_weights");
	} else {
		num_weights = num_ref_idx_active[0];
	}
	if (num_weights>VVC_MAX_REF_PICS) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_CODING, ("[VVC] num weights L0 %d greater than max allowed %d\n", num_weights, VVC_MAX_REF_PICS));
		return -1;
	}

	memset(weights, 0, sizeof(u8)*VVC_MAX_REF_PICS);
	for (i=0; i<num_weights; i++) {
		if (gf_bs_read_int_log_idx(bs, 1, "luma_weight_l0_flag", i))
			weights[i] |= 1;
	}
	if (sps->chroma_format_idc) {
		for (i=0; i<num_weights; i++) {
			if (gf_bs_read_int_log_idx(bs, 1, "chroma_weight_l0_flag", i))
				weights[i] |= 2;
		}
	}
	for (i=0; i<num_weights; i++) {
		if (weights[i] & 1) {
			gf_bs_read_se_log_idx(bs, "delta_luma_weight_l0", i);
			gf_bs_read_se_log_idx(bs, "luma_offset_l0", i);
		}
		if (weights[i] & 2) {
			gf_bs_read_se_log_idx2(bs, "delta_chroma_weight_l0", i, 0);
			gf_bs_read_se_log_idx2(bs, "delta_chroma_offset_l0", i, 0);
			gf_bs_read_se_log_idx2(bs, "delta_chroma_weight_l0", i, 1);
			gf_bs_read_se_log_idx2(bs, "delta_chroma_offset_l0", i, 1);
		}
	}
	num_weights = 0;
	if (pps->weighted_bipred_flag && pps->wp_info_in_ph_flag && (si->ph_rpl[1].num_ref_entries > 0)) {
		num_weights = gf_bs_read_ue_log(bs, "num_l1_weights");
	}
	if (!num_weights) return 0;

	if (num_weights>VVC_MAX_REF_PICS) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_CODING, ("[VVC] num weights L1 %d greater than max allowed %d\n", num_weights, VVC_MAX_REF_PICS));
		return -1;
	}

	memset(weights, 0, sizeof(u8)*VVC_MAX_REF_PICS);
	for (i=0; i<num_weights; i++) {
		if (gf_bs_read_int_log_idx(bs, 1, "luma_weight_l1_flag", i))
			weights[i] |= 1;
	}
	if (sps->chroma_format_idc) {
		for (i=0; i<num_weights; i++) {
			if (gf_bs_read_int_log_idx(bs, 1, "chroma_weight_l1_flag", i))
				weights[i] |= 2;
		}
	}
	for (i=0; i<num_weights; i++) {
		if (weights[i] & 1) {
			gf_bs_read_se_log_idx(bs, "delta_luma_weight_l1", i);
			gf_bs_read_se_log_idx(bs, "luma_offset_l1", i);
		}
		if (weights[i] & 2) {
			gf_bs_read_se_log_idx2(bs, "delta_chroma_weight_l1", i, 0);
			gf_bs_read_se_log_idx2(bs, "delta_chroma_offset_l1", i, 0);
			gf_bs_read_se_log_idx2(bs, "delta_chroma_weight_l1", i, 1);
			gf_bs_read_se_log_idx2(bs, "delta_chroma_offset_l1", i, 1);
		}
	}
	return 0;
}

static s32 vvc_parse_picture_header(GF_BitStream *bs, VVCState *vvc, VVCSliceInfo *si)
{
	s32 pps_id;

	si->irap_or_gdr_pic = gf_bs_read_int_log(bs, 1, "irap_or_gdr_pic");
	si->non_ref_pic = gf_bs_read_int_log(bs, 1, "non_ref_pic");
	if (si->irap_or_gdr_pic)
		si->gdr_pic = gf_bs_read_int_log(bs, 1, "gdr_pic");

	si->intra_slice_allowed_flag = 1;
	if ((si->inter_slice_allowed_flag = gf_bs_read_int_log(bs, 1, "inter_slice_allowed_flag")))
		si->intra_slice_allowed_flag = gf_bs_read_int_log(bs, 1, "intra_slice_allowed_flag");

	pps_id = gf_bs_read_ue_log(bs, "pps_id");
	if ((pps_id<0) || (pps_id >= 64))
		return -1;
	si->pps = &vvc->pps[pps_id];
	si->sps = &vvc->sps[si->pps->sps_id];
	si->poc_lsb = gf_bs_read_int_log(bs, si->sps->log2_max_poc_lsb, "poc_lsb");

	si->recovery_point_valid = 0;
	si->gdr_recovery_count = 0;
	if (si->gdr_pic) {
		si->recovery_point_valid = 1;
		si->gdr_recovery_count = gf_bs_read_ue_log(bs, "gdr_recovery_count");
	}
	gf_bs_read_int_log(bs, si->sps->ph_num_extra_bits, "ph_extra_bits");

	if (si->sps->poc_msb_cycle_flag) {
		if ( (si->poc_msb_cycle_present_flag = gf_bs_read_int_log(bs, 1, "poc_msb_cycle_present_flag"))) {
			si->poc_msb_cycle = gf_bs_read_int_log(bs, si->sps->poc_msb_cycle_len, "poc_msb_cycle");
		}
	}

	if (si->sps->alf_enabled_flag && si->pps->alf_info_in_ph_flag ) {
		if (gf_bs_read_int_log(bs, 1, "ph_alf_enabled_flag")) {
			u32 i, nb_aps_id = gf_bs_read_int_log(bs, 3, "ph_num_alf_aps_ids_luma");
			for (i=0; i<nb_aps_id; i++) {
				gf_bs_read_int_log_idx(bs, 3, "ph_alf_aps_id_luma", i);
			}
			u8 alf_cb_enabled_flag = 0, alf_cr_enabled_flag=0;
			if (si->sps->chroma_format_idc) {
				alf_cb_enabled_flag = gf_bs_read_int_log(bs, 1, "ph_alf_cb_enabled_flag");
				alf_cr_enabled_flag = gf_bs_read_int_log(bs, 1, "ph_alf_cr_enabled_flag");
			}
			if (alf_cb_enabled_flag || alf_cr_enabled_flag) {
				gf_bs_read_int_log(bs, 3, "ph_alf_aps_id_chroma");
			}
			if (si->sps->ccalf_enabled_flag ) {
				if (gf_bs_read_int_log(bs, 1, "ph_alf_cc_cb_enabled_flag")) {
					gf_bs_read_int_log(bs, 3, "ph_alf_cc_cb_aps_id");
				}
				if (gf_bs_read_int_log(bs, 1, "ph_alf_cc_cr_enabled_flag")) {
					gf_bs_read_int_log(bs, 3, "ph_alf_cc_cr_aps_id");
				}
			}
		}
	}
	si->lmcs_enabled_flag = 0;
	if (si->sps->lmcs_enabled_flag) {
		si->lmcs_enabled_flag = gf_bs_read_int_log(bs, 1, "ph_lmcs_enabled_flag");
		if (si->lmcs_enabled_flag) {
			gf_bs_read_int_log(bs, 2, "ph_lmcs_aps_id");
			if (si->sps->chroma_format_idc) {
				gf_bs_read_int_log(bs, 1, "ph_chroma_residual_scale_flag");
			}
		}
	}
	si->explicit_scaling_list_enabled_flag = 0;
	if (si->sps->explicit_scaling_list_enabled_flag) {
		si->explicit_scaling_list_enabled_flag = gf_bs_read_int_log(bs, 1, "ph_explicit_scaling_list_enabled_flag");
		if (si->explicit_scaling_list_enabled_flag) {
			gf_bs_read_int_log(bs, 3, "ph_scaling_list_aps_id");
		}
	}
	if (si->sps->virtual_boundaries_enabled_flag && !si->sps->virtual_boundaries_present_flag) {
		if (gf_bs_read_int_log(bs, 1, "ph_virtual_boundaries_present_flag")) {
			u32 i, nb_virt_boundaries = gf_bs_read_ue_log(bs, "ph_num_ver_virtual_boundaries");
			for (i=0; i<nb_virt_boundaries; i++) {
				gf_bs_read_ue_log_idx(bs, "ph_virtual_boundary_pos_x_minus1", i);
			}
			nb_virt_boundaries = gf_bs_read_ue_log(bs, "ph_num_hor_virtual_boundaries");
			for (i=0; i<nb_virt_boundaries; i++) {
				gf_bs_read_ue_log_idx(bs, "ph_virtual_boundary_pos_x_minus1", i);
			}
		}
	}
	if (si->pps->output_flag_present_flag && !si->non_ref_pic) {
		gf_bs_read_int_log(bs, 1, "ph_pic_output_flag");
	}
	if (si->pps->rpl_info_in_ph_flag) {
		s32 res = vvc_parse_ref_pic_lists(bs, si, GF_TRUE);
		if (res<0) return res;
	}
	u8 partition_constraints_override_flag = 0;
	if (si->sps->partition_constraints_override_enabled_flag) {
		partition_constraints_override_flag = gf_bs_read_int_log(bs, 1, "ph_partition_constraints_override_flag");
	}
	if (si->intra_slice_allowed_flag) {
		if (partition_constraints_override_flag) {
			gf_bs_read_ue_log(bs, "ph_log2_diff_min_qt_min_cb_inter_slice");
			u32 max_mtt_hierarchy_depth_inter_slice = gf_bs_read_ue_log(bs, "ph_max_mtt_hierarchy_depth_inter_slice");
			if (max_mtt_hierarchy_depth_inter_slice) {
				gf_bs_read_ue_log(bs, "ph_log2_diff_max_bt_min_qt_inter_slice");
				gf_bs_read_ue_log(bs, "ph_log2_diff_max_tt_min_qt_inter_slice");
			}
		}
		if (si->pps->cu_qp_delta_enabled_flag) {
			gf_bs_read_ue_log(bs, "ph_cu_qp_delta_subdiv_inter_slice");
		}
		if (si->pps->cu_chroma_qp_offset_list_enabled_flag) {
			gf_bs_read_ue_log(bs, "ph_cu_chroma_qp_offset_subdiv_intra_slice");
		}
	}
	si->temporal_mvp_enabled_flag = 0;
	if (si->inter_slice_allowed_flag) {
		if (partition_constraints_override_flag) {
			gf_bs_read_ue_log(bs, "ph_log2_diff_min_qt_min_cb_inter_slice");
			u32 max_mtt_hierarchy_depth_inter_slice = gf_bs_read_ue_log(bs, "ph_max_mtt_hierarchy_depth_inter_slice");
			if (max_mtt_hierarchy_depth_inter_slice) {
				gf_bs_read_ue_log(bs, "ph_log2_diff_max_bt_min_qt_inter_slice");
				gf_bs_read_ue_log(bs, "ph_log2_diff_max_tt_min_qt_inter_slice");
			}
		}
		if (si->pps->cu_qp_delta_enabled_flag) {
			gf_bs_read_ue_log(bs, "ph_cu_qp_delta_subdiv_inter_slice");
		}
		if (si->pps->cu_chroma_qp_offset_list_enabled_flag) {
			gf_bs_read_ue_log(bs, "ph_cu_chroma_qp_offset_subdiv_inter_slice");
		}
		if (si->sps->temporal_mvp_enabled_flag) {
			si->temporal_mvp_enabled_flag = gf_bs_read_int_log(bs, 1, "ph_temporal_mvp_enabled_flag");
			if (si->temporal_mvp_enabled_flag && si->pps->rpl_info_in_ph_flag) {
				u8 collocated_from_l0_flag = 1;
				if (si->ph_rpl[1].num_ref_entries>0)
					collocated_from_l0_flag = gf_bs_read_int_log(bs, 1, "ph_collocated_from_l0_flag");

				if ( (collocated_from_l0_flag && si->ph_rpl[0].num_ref_entries > 1)
					|| (!collocated_from_l0_flag && si->ph_rpl[1].num_ref_entries > 1)
				) {
					gf_bs_read_ue_log(bs, "ph_collocated_ref_idx");
				}
			}
		}
		if (si->sps->mmvd_fullpel_only_enabled_flag) {
			gf_bs_read_int_log(bs, 1, "ph_mmvd_fullpel_only_flag");
		}
		u8 presenceFlag = 0;
		if (!si->pps->rpl_info_in_ph_flag) {
			presenceFlag = 1;
		}
		else if (si->ph_rpl[1].num_ref_entries > 0) {
			presenceFlag = 1;
		}
		if (presenceFlag) {
			gf_bs_read_int_log(bs, 1, "ph_mvd_l1_zero_flag");
			if (si->sps->bdof_control_present_in_ph_flag)
				gf_bs_read_int_log(bs, 1, "ph_bdof_disabled_flag");
			if (si->sps->dmvr_control_present_in_ph_flag)
				gf_bs_read_int_log(bs, 1, "ph_dmvr_disabled_flag");
		}
		if (si->sps->prof_control_present_in_ph_flag)
			gf_bs_read_int_log(bs, 1, "ph_prof_disabled_flag");

		if ( (si->pps->weighted_pred_flag || si->pps->weighted_bipred_flag) && si->pps->wp_info_in_ph_flag) {
			s32 res = vvc_pred_weight_table(bs, vvc, si, si->pps, si->sps, NULL);
			if (res<0) return res;
		}
	}
	if (si->pps->qp_delta_info_in_ph_flag) {
		gf_bs_read_se_log(bs, "ph_qp_delta");
	}
	if (si->sps->joint_cbcr_enabled_flag) {
		gf_bs_read_int_log(bs, 1, "ph_joint_cbcr_sign_flag");
	}
	if (si->sps->sao_enabled_flag && si->pps->sao_info_in_ph_flag) {
		gf_bs_read_int_log(bs, 1, "ph_sao_luma_enabled_flag");
		if (si->sps->chroma_format_idc)
			gf_bs_read_int_log(bs, 1, "ph_sao_chroma_enabled_flag");
	}
	if (si->pps->dbf_info_in_ph_flag) {
		if (gf_bs_read_int_log(bs, 1, "ph_deblocking_params_present_flag")) {
			//defaults to 0
			u8 deblocking_filter_disabled_flag = 0;

			if (!si->pps->deblocking_filter_disabled_flag) {
				deblocking_filter_disabled_flag = gf_bs_read_int_log(bs, 1, "deblocking_filter_disabled_flag");
			}
			if (!deblocking_filter_disabled_flag) {
				gf_bs_read_se_log(bs, "ph_luma_beta_offset_div2");
				gf_bs_read_se_log(bs, "ph_luma_tc_offset_div2");
				if (si->pps->chroma_tool_offsets_present_flag) {
					gf_bs_read_se_log(bs, "ph_cb_beta_offset_div2");
					gf_bs_read_se_log(bs, "ph_cb_tc_offset_div2");
					gf_bs_read_se_log(bs, "ph_cr_beta_offset_div2");
					gf_bs_read_se_log(bs, "ph_cr_tc_offset_div2");
				}
			}
		}
	}
	if (si->pps->picture_header_extension_present_flag) {
		u32 i=0, num_ext = gf_bs_read_ue_log(bs, "ph_extension_length");
		while (i<num_ext) {
			gf_bs_read_int_log_idx(bs, 8, "ph_extension_data_byte", i);
			i++;
		}
	}
	return 0;
}

static s32 vvc_get_ctb_info_in_slice(VVCSliceInfo *si, u32 sh_slice_address, u32 sh_num_tiles_in_slice, s32 ctu_index)
{
	if (si->pps->rect_slice_flag) {
/*
	TODO
		u32 picLevelSliceIdx = sh_slice_address;
		for (j=0; j<CurrSubpicIdx; j++) {
			picLevelSliceIdx += NumSlicesInSubpic[j];

		u32 NumCtusInCurrSlice = NumCtusInSlice[picLevelSliceIdx];
		for (i=0; i<NumCtusInCurrSlice; i++)
			CtbAddrInCurrSlice[i] = CtbAddrInSlice[ picLevelSliceIdx ][ i ]
*/

	} else {
		u32 i, tidx, NumCtusInCurrSlice = 0;

		for (tidx=sh_slice_address; tidx < sh_slice_address + sh_num_tiles_in_slice; tidx++) {
			u32 ctb_y, ctb_x;
			u32 tileX = tidx % si->pps->num_tile_cols;
			u32 tileY = tidx / si->pps->num_tile_cols;
			u32 min_ctbx=0;
			u32 max_ctbx=0;
			u32 min_ctby=0;
			u32 max_ctby=0;

			if (tileY>=VVC_MAX_TILE_ROWS) return -1;
			if (tileX>=VVC_MAX_TILE_COLS) return -1;

			for (i=0; i<tileY; i++) min_ctby += si->pps->tile_rows_height_ctb[i];
			max_ctby = min_ctby + si->pps->tile_rows_height_ctb[i];

			for (i=0; i<tileX; i++) min_ctbx += si->pps->tile_cols_width_ctb[i];
			max_ctbx = min_ctbx + si->pps->tile_cols_width_ctb[i];


			for (ctb_y=min_ctby; ctb_y < max_ctby; ctb_y++) {
				for (ctb_x=min_ctbx; ctb_x < max_ctbx; ctb_x++) {
					if (ctu_index>=0) {
						if (ctu_index == NumCtusInCurrSlice)
							return ctb_y * si->pps->pic_width_in_ctbsY + ctb_x;
					}
					NumCtusInCurrSlice++;
				}
			}
		}
		if (ctu_index>=0) return -1;
		return NumCtusInCurrSlice;
	}
	return -1;
}

static u32 vvc_ctb_to_tile_row_bd(VVCSliceInfo *si, u32 ctb_addr_y)
{
	u32 i, tile_y = 0;
	u32 tile_row_bd_val = 0;

	for (i=0; i <= si->pps->pic_height_in_ctbsY; i++) {
		if (i == tile_row_bd_val + si->pps->tile_rows_height_ctb[tile_y]) {
			tile_row_bd_val += si->pps->tile_rows_height_ctb[tile_y];
			tile_y++;
		}
		if (ctb_addr_y == i) return tile_row_bd_val;
	}
	return 0;
}

static u32 vvc_ctb_to_tile_col_bd(VVCSliceInfo *si, u32 ctb_addr_x)
{
	u32 i, tile_x = 0;
	u32 tile_col_bd_val = 0;
	for (i=0; i <= si->pps->pic_width_in_ctbsY; i++) {
		if (i == tile_col_bd_val + si->pps->tile_cols_width_ctb[tile_x] ) {
			tile_col_bd_val += si->pps->tile_cols_width_ctb[tile_x];
			tile_x++;
		}
		if (ctb_addr_x == i) return tile_col_bd_val;
	}
	return 0;
}

static u32 vvc_get_num_entry_points(VVCSliceInfo *si, u32 sh_slice_address, u32 sh_num_tiles_in_slice)
{
	if (!si->sps->entry_point_offsets_present_flag) return 0;

	u32 nb_entry_points = 0;
	u32 prev_ctb_addr_y=0;
	u32 prev_ctb_to_tile_row_bd, prev_ctb_to_tile_col_bd;
	s32 i;
	s32 nb_ctus_in_slice = vvc_get_ctb_info_in_slice(si, sh_slice_address, sh_num_tiles_in_slice, -1);
	if (nb_ctus_in_slice<0) return 0;

	for (i=0; i < nb_ctus_in_slice; i++ ) {
		s32 addr;
		u32 ctb_addr_x, ctb_addr_y;
		u32 ctb_to_tile_row_bd, ctb_to_tile_col_bd;

		addr = vvc_get_ctb_info_in_slice(si, sh_slice_address, sh_num_tiles_in_slice, i);
		if (addr<0) return 0;
		ctb_addr_x = (u32) ( addr % si->pps->pic_width_in_ctbsY );
		ctb_addr_y = (u32) ( addr / si->pps->pic_width_in_ctbsY );

		ctb_to_tile_row_bd = vvc_ctb_to_tile_row_bd(si, ctb_addr_y);
		ctb_to_tile_col_bd = vvc_ctb_to_tile_col_bd(si, ctb_addr_x);

		if (i) {

			if ( ctb_to_tile_row_bd != prev_ctb_to_tile_row_bd
				|| ctb_to_tile_col_bd != prev_ctb_to_tile_col_bd
				|| ((ctb_addr_y != prev_ctb_addr_y) && si->sps->entropy_coding_sync_enabled_flag)
			) {
				nb_entry_points++;
			}

		}

		prev_ctb_addr_y = ctb_addr_y;
		prev_ctb_to_tile_row_bd = ctb_to_tile_row_bd;
		prev_ctb_to_tile_col_bd = ctb_to_tile_col_bd;
	}
	return nb_entry_points;
}

static s32 vvc_parse_slice(GF_BitStream *bs, VVCState *vvc, VVCSliceInfo *si)
{
	u32 i, subpic_id=0, slice_address=0, num_tiles_in_slice=1;

	si->picture_header_in_slice_header_flag = gf_bs_read_int_log(bs, 1, "picture_header_in_slice_header_flag");
	if (si->picture_header_in_slice_header_flag) {
		s32 res = vvc_parse_picture_header(bs, vvc, si);
		if (res<0) return res;
	}
	if (!si->sps) return -1;
	if (!si->pps) return -1;
	si->slice_type = GF_VVC_SLICE_TYPE_I;
	if (si->sps->subpic_info_present) {
		subpic_id = gf_bs_read_int_log(bs, si->sps->subpicid_len, "subpic_id");
	}

	if (si->pps->rect_slice_flag) {
		if ((si->sps->nb_subpics==1) && (si->pps->num_slices_in_pic<=1)) {

		} else {
			if (!vvc->parse_mode) {
				return 0;
			}
			VVC_SubpicInfo *sp = NULL;
			for (i=0;i<si->sps->nb_subpics; i++) {
				if (si->pps->subpics[i].id==subpic_id) {
					sp = &si->pps->subpics[i];
					break;
				}
			}
			if (!sp) {
				GF_LOG(GF_LOG_ERROR, GF_LOG_CODING, ("[VVC] sub-picture with id %u not found\n", subpic_id));
				return -1;
			}
			if (sp->num_slices > 1 ) {
				u32 shbits = vvc_ceillog2(sp->num_slices);
				slice_address = gf_bs_read_int_log(bs, shbits, "sh_slice_address");
			}
		}
	} else {
		if (si->pps->num_tiles_in_pic > 1) {
			slice_address = gf_bs_read_int_log(bs, si->pps->slice_address_len, "sh_slice_address");
		}
	}

	gf_bs_read_int_log(bs, si->sps->sh_num_extra_bits, "num_extra_bits");

	if (!si->pps->rect_slice_flag && (si->pps->num_tiles_in_pic - slice_address > 1)) {
		num_tiles_in_slice = 1 + gf_bs_read_ue_log(bs, "sh_num_tiles_in_slice_minus1");
		if (num_tiles_in_slice > si->pps->num_tiles_in_pic) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_CODING, ("[VVC] Invalid number of tiles in slice %u, should be less than %u\n", num_tiles_in_slice, si->pps->num_tiles_in_pic));
			num_tiles_in_slice = si->pps->num_tiles_in_pic;
		}
	}

	if (si->inter_slice_allowed_flag )
		si->slice_type = gf_bs_read_ue_log(bs, "slice_type");

	if (!vvc->parse_mode) return 0;

	switch (si->nal_unit_type) {
	case GF_VVC_NALU_SLICE_IDR_W_RADL:
	case GF_VVC_NALU_SLICE_IDR_N_LP:
	case GF_VVC_NALU_SLICE_CRA:
	case GF_VVC_NALU_SLICE_GDR:
		gf_bs_read_int_log(bs, 1, "sh_no_output_of_prior_pics_flag");
		break;
	}
	if (si->sps->alf_enabled_flag && !si->pps->alf_info_in_ph_flag) {
		if (gf_bs_read_int_log(bs, 1, "sh_alf_enabled_flag")) {
			u32 nb_vals = gf_bs_read_int_log(bs, 3, "sh_num_alf_aps_ids_luma");
			for (i=0; i<nb_vals; i++) {
				gf_bs_read_int_log(bs, 3, "sh_alf_aps_id_luma");
			}
			u8 sh_alf_cb_enabled_flag = 0;
			u8 sh_alf_cr_enabled_flag = 0;
			if (si->sps->chroma_format_idc) {
				sh_alf_cb_enabled_flag = gf_bs_read_int_log(bs, 1, "sh_alf_cb_enabled_flag");
				sh_alf_cr_enabled_flag = gf_bs_read_int_log(bs, 1, "sh_alf_cr_enabled_flag");
			}
			if (sh_alf_cb_enabled_flag || sh_alf_cr_enabled_flag) {
				gf_bs_read_int_log(bs, 3, "sh_alf_aps_id_chroma");
			}
			if (si->sps->ccalf_enabled_flag) {
				if (gf_bs_read_int_log(bs, 1, "sh_alf_cc_cb_enabled_flag")) {
					gf_bs_read_int_log(bs, 3, "sh_alf_cc_cb_aps_id");
				}
				if (gf_bs_read_int_log(bs, 1, "sh_alf_cc_cr_enabled_flag")) {
					gf_bs_read_int_log(bs, 3, "sh_alf_cc_cr_aps_id");
				}
			}
		}
	}
	if (si->lmcs_enabled_flag && !si->picture_header_in_slice_header_flag) {
		gf_bs_read_int_log(bs, 1, "sh_lmcs_used_flag");
	}
	if (si->explicit_scaling_list_enabled_flag && !si->picture_header_in_slice_header_flag) {
		gf_bs_read_int_log(bs, 3, "sh_explicit_scaling_list_used_flag");
	}

	if (si->pps->rpl_info_in_ph_flag) {
		si->rpl[0] = si->ph_rpl[0];
		si->rpl[1] = si->ph_rpl[1];
	} else {
		memset(&si->rpl[0], 0, sizeof(VVC_RefPicList));
		memset(&si->rpl[1], 0, sizeof(VVC_RefPicList));
	}

	if (!si->pps->rpl_info_in_ph_flag
		&& ( ( (si->nal_unit_type != GF_VVC_NALU_SLICE_IDR_N_LP) && (si->nal_unit_type != GF_VVC_NALU_SLICE_IDR_W_RADL) ) || si->sps->idr_rpl_present_flag)
	) {
		s32 res = vvc_parse_ref_pic_lists(bs, si, GF_FALSE);
		if (res<0) return (vvc->parse_mode==1) ? res : 0;
	}

	u32 num_ref_idx_active[2] = {0, 0};

	if (
		((si->slice_type != GF_VVC_SLICE_TYPE_I) && (si->rpl[0].num_ref_entries > 1) )
		|| ((si->slice_type == GF_VVC_SLICE_TYPE_B) && (si->rpl[1].num_ref_entries > 1))
	) {
		if (gf_bs_read_int_log(bs, 1, "sh_num_ref_idx_active_override_flag")) {
			//L0
			u32 nb_active = 0;
			if (si->rpl[0].num_ref_entries>1) {
				nb_active = 1 + gf_bs_read_ue_log_idx(bs, "sh_num_ref_idx_active_minus1", 0);
			}
			num_ref_idx_active[0] = nb_active;
			//L1
			if (si->slice_type == GF_VVC_SLICE_TYPE_B) {
				nb_active = 0;
				if (si->rpl[1].num_ref_entries>1) {
					nb_active = 1 + gf_bs_read_ue_log_idx(bs, "sh_num_ref_idx_active_minus1", 1);
				}
				num_ref_idx_active[1] = nb_active;
			} else {
				num_ref_idx_active[1] = 0;
			}
		} else {
			if (si->rpl[0].num_ref_entries >= si->pps->num_ref_idx_default_active[0]) {
				num_ref_idx_active[0] = si->pps->num_ref_idx_default_active[0];
			} else {
				num_ref_idx_active[0] = si->rpl[0].num_ref_entries;
			}
			if (si->slice_type == GF_VVC_SLICE_TYPE_B) {
				if (si->rpl[1].num_ref_entries >= si->pps->num_ref_idx_default_active[1]) {
					num_ref_idx_active[1] = si->pps->num_ref_idx_default_active[1];
				} else {
					num_ref_idx_active[1] = si->rpl[1].num_ref_entries;
				}
			} else {
				num_ref_idx_active[1] = 0;
			}
		}
	} else {
        num_ref_idx_active[0] = (si->slice_type == GF_VVC_SLICE_TYPE_I) ? 0 : 1;
        num_ref_idx_active[1] = (si->slice_type == GF_VVC_SLICE_TYPE_B) ? 1 : 0;
	}

	if (si->slice_type != GF_VVC_SLICE_TYPE_I) {
		if (si->pps->cabac_init_present_flag)
			gf_bs_read_int_log(bs, 1, "sh_cabac_init_flag");

		if (si->temporal_mvp_enabled_flag && !si->pps->rpl_info_in_ph_flag) {
			u8 collocated_from_l0_flag = 0;
			if (si->slice_type == GF_VVC_SLICE_TYPE_B) {
				collocated_from_l0_flag = gf_bs_read_int_log(bs, 1, "sh_collocated_from_l0_flag");
			}
			if ( (collocated_from_l0_flag && (num_ref_idx_active[0] > 1))
				|| (!collocated_from_l0_flag && (num_ref_idx_active[1] > 1))
			) {
				gf_bs_read_ue_log(bs, "sh_collocated_ref_idx");
			}
		}
		if (!si->pps->wp_info_in_ph_flag
			&& (
				(si->pps->weighted_pred_flag && (si->slice_type == GF_VVC_SLICE_TYPE_P) )
				|| (si->pps->weighted_bipred_flag && (si->slice_type == GF_VVC_SLICE_TYPE_B))
			)
		) {
			s32 res = vvc_pred_weight_table(bs, vvc, si, si->pps, si->sps, num_ref_idx_active);
			if (res<0) return (vvc->parse_mode==1) ? res : 0;
		}
	}
	if (!si->pps->qp_delta_info_in_ph_flag) {
		gf_bs_read_se_log(bs, "sh_qp_delta");
	}
	if (si->pps->slice_chroma_qp_offsets_present_flag) {
		gf_bs_read_se_log(bs, "sh_cb_qp_offset");
		gf_bs_read_se_log(bs, "sh_cr_qp_offset");
		if (si->sps->joint_cbcr_enabled_flag)
			gf_bs_read_se_log(bs, "sh_joint_cbcr_qp_offset");
	}
	if (si->pps->cu_chroma_qp_offset_list_enabled_flag)
		gf_bs_read_int_log(bs, 1, "sh_cu_chroma_qp_offset_enabled_flag");
	if (si->sps->sao_enabled_flag && !si->pps->sao_info_in_ph_flag) {
		gf_bs_read_int_log(bs, 1, "sh_sao_luma_used_flag");
		if (si->sps->chroma_format_idc)
			gf_bs_read_int_log(bs, 1, "sh_sao_chroma_used_flag");
	}

	if (si->pps->deblocking_filter_override_enabled_flag && !si->pps->dbf_info_in_ph_flag) {
		if (gf_bs_read_int_log(bs, 1, "sh_deblocking_params_present_flag")) {
			u8 deblocking_params_present_flag=0;
			if (!si->pps->deblocking_filter_disabled_flag) {
				deblocking_params_present_flag = gf_bs_read_int_log(bs, 1, "sh_deblocking_filter_disabled_flag");
			}
			if (deblocking_params_present_flag) {
				gf_bs_read_se_log(bs, "sh_luma_beta_offset_div2");
				gf_bs_read_se_log(bs, "sh_luma_tc_offset_div2");
				if (si->pps->chroma_tool_offsets_present_flag) {
					gf_bs_read_se_log(bs, "sh_cb_beta_offset_div2");
					gf_bs_read_se_log(bs, "sh_cb_tc_offset_div2");
					gf_bs_read_se_log(bs, "sh_cr_beta_offset_div2");
					gf_bs_read_se_log(bs, "sh_cr_tc_offset_div2");
				}
			}
		}
	}

	u8 dep_quant_used_flag = 0;
	if (si->sps->dep_quant_enabled_flag) {
		dep_quant_used_flag = gf_bs_read_int_log(bs, 1, "sh_dep_quant_used_flag");
	}
	u8 sign_data_hiding_used_flag = 0;
	if (si->sps->sign_data_hiding_enabled_flag && !dep_quant_used_flag) {
		sign_data_hiding_used_flag = gf_bs_read_int_log(bs, 1, "sh_sign_data_hiding_used_flag");
	}
	u8 ts_residual_coding_disabled_flag = 0;
	if (si->sps->transform_skip_enabled_flag && !dep_quant_used_flag && !sign_data_hiding_used_flag) {
		ts_residual_coding_disabled_flag = gf_bs_read_int_log(bs, 1, "sh_ts_residual_coding_disabled_flag");
	}
	if (!ts_residual_coding_disabled_flag && si->sps->ts_residual_coding_rice_present_in_sh_flag) {
		gf_bs_read_int_log(bs, 3, "sh_ts_residual_coding_rice_idx_minus1");
	}
	if (si->sps->reverse_last_sig_coeff_enabled_flag) {
		gf_bs_read_int_log(bs, 1, "sh_reverse_last_sig_coeff_flag");
	}
	if (si->pps->slice_header_extension_present_flag) {
		u32 j=0, slice_header_extension_length = gf_bs_read_ue_log(bs, "sh_slice_header_extension_length");
		while (j<slice_header_extension_length) {
			gf_bs_read_int_log_idx(bs, 8, "sh_slice_header_extension_data_byte", j);
			j++;
		}
	}

	if (!si->pps->rect_slice_flag && !si->pps->num_tile_cols && !si->pps->num_tile_rows) {
	} else {
		if (si->sps->entry_point_offsets_present_flag && si->pps->rect_slice_flag) {
			if (vvc->parse_mode==1) {
				GF_LOG(GF_LOG_WARNING, GF_LOG_CODING, ("[VVC] Entry point offsets parsing for sub-picture not yet implemented, wrong slice header size estimation (result might be non-compliant) - patch welcome\n"));

				gf_bs_align(bs);
				si->payload_start_offset = (u32) gf_bs_get_position(bs);
				return -2;
			}
			GF_LOG(GF_LOG_INFO, GF_LOG_CODING, ("[VVC] Entry point offsets parsing for sub-picture not yet implemented, aborting slice header parsing - patch welcome\n"));
			return 0;
		}

		u32 nb_entry_points = vvc_get_num_entry_points(si, slice_address, num_tiles_in_slice);
		if (nb_entry_points) {
			u32 nb_bits = 1 + gf_bs_read_ue_log(bs, "sh_entry_offset_len_minus1");
			for (i=0; i<nb_entry_points; i++) {
				gf_bs_read_int_log_idx(bs, nb_bits, "sh_entry_point_offset_minus1", i);
			}
		}
	}

	u8 align_bit = gf_bs_read_int(bs, 1);
	if (align_bit != 1) {
		GF_LOG(GF_LOG_WARNING, GF_LOG_CODING, ("[VVC] Align bit at end of slice header not set to 1 !\n"));
		//return error only for strict mdoe
		if (vvc->parse_mode==1) {
			return -1;
		}
	}
	gf_bs_align(bs);
	//end of slice header
	si->payload_start_offset = (u32) gf_bs_get_position(bs);
	return 0;
}

static void vvc_compute_poc(VVCSliceInfo *si, Bool poc_reset)
{
	u32 max_poc_lsb = 1 << (si->sps->log2_max_poc_lsb);

	if (si->poc_msb_cycle_present_flag) {
		si->poc_msb = si->poc_msb_cycle * max_poc_lsb;
	} else if (poc_reset) {
		si->poc_msb = 0;
	} else {
		if ((si->poc_lsb < si->poc_lsb_prev) && (si->poc_lsb_prev - si->poc_lsb >= max_poc_lsb / 2))
			si->poc_msb = si->poc_msb_prev + max_poc_lsb;
		else if ((si->poc_lsb > si->poc_lsb_prev) && (si->poc_lsb - si->poc_lsb_prev > max_poc_lsb / 2))
			si->poc_msb = si->poc_msb_prev - max_poc_lsb;
		else
			si->poc_msb = si->poc_msb_prev;
	}

	si->poc = si->poc_msb + si->poc_lsb;
}


GF_EXPORT
s32 gf_vvc_parse_nalu_bs(GF_BitStream *bs, VVCState *vvc, u8 *nal_unit_type, u8 *temporal_id, u8 *layer_id)
{
	Bool is_slice = GF_FALSE;
	s32 ret = -1;
	Bool poc_reset = GF_FALSE;
	VVCSliceInfo n_state;

	gf_bs_enable_emulation_byte_removal(bs, GF_TRUE);
	if (gf_bs_available(bs)<2) return -1;
	gf_bs_mark_overflow(bs, GF_TRUE);

	memcpy(&n_state, &vvc->s_info, sizeof(VVCSliceInfo));
	if (!vvc_parse_nal_header(bs, nal_unit_type, temporal_id, layer_id)) return -1;

	n_state.nal_unit_type = *nal_unit_type;

	switch (n_state.nal_unit_type) {
	case GF_VVC_NALU_ACCESS_UNIT:
	case GF_VVC_NALU_END_OF_SEQ:
	case GF_VVC_NALU_END_OF_STREAM:
		//don't restore slice info, we don't have any change and n_state.poc_lsb / n_state.poc_msb is not valid
		vvc->s_info.nal_unit_type = n_state.nal_unit_type;
		return 1;

	case GF_VVC_NALU_SLICE_IDR_W_RADL:
	case GF_VVC_NALU_SLICE_IDR_N_LP:
		poc_reset = GF_TRUE;
	case GF_VVC_NALU_SLICE_TRAIL:
	case GF_VVC_NALU_SLICE_STSA:
	case GF_VVC_NALU_SLICE_RADL:
	case GF_VVC_NALU_SLICE_RASL:
	case GF_VVC_NALU_SLICE_CRA:
	case GF_VVC_NALU_SLICE_GDR:
		/* slice - read the info and compare.*/
		ret = vvc_parse_slice(bs, vvc, &n_state);
		if (ret < 0) {
			memcpy(&vvc->s_info, &n_state, sizeof(VVCSliceInfo));
			return ret;
		}

		ret = 0;
		if (n_state.compute_poc_defer || n_state.picture_header_in_slice_header_flag) {
			is_slice = GF_TRUE;
			n_state.compute_poc_defer = 0;

			vvc_compute_poc(&n_state, poc_reset);
			if (vvc->s_info.poc != n_state.poc) {
				ret = 1;
				break;
			}
			if (!(*layer_id) || (n_state.prev_layer_id_plus1 && ((*layer_id) <= n_state.prev_layer_id_plus1 - 1))) {
				ret = 1;
				break;
			}
		}
		break;

	case GF_VVC_NALU_PIC_HEADER:
		if (vvc_parse_picture_header(bs, vvc, &n_state)<0) {
			ret = -1;
			break;
		}
		is_slice = GF_TRUE;

		//we cannot compute poc until we know the first picture unit type, since IDR will reset poc count
		//and irap_or_gdr_pic=0 does not prevent IDR from following
		n_state.compute_poc_defer = 1;

		if (!(*layer_id) || (n_state.prev_layer_id_plus1 && ((*layer_id) <= n_state.prev_layer_id_plus1 - 1))) {
			ret = 1;
		}
		break;
	case GF_VVC_NALU_SEQ_PARAM:
		vvc->last_parsed_sps_id = gf_vvc_read_sps_bs_internal(bs, vvc, *layer_id, NULL);
		ret = (vvc->last_parsed_sps_id>=0) ? 0 : -1;
		break;
	case GF_VVC_NALU_PIC_PARAM:
		vvc->last_parsed_pps_id = gf_vvc_read_pps_bs_internal(bs, vvc);
		ret = (vvc->last_parsed_pps_id>=0) ? 0 : -1;
		break;
	case GF_VVC_NALU_VID_PARAM:
		vvc->last_parsed_vps_id = gf_vvc_read_vps_bs_internal(bs, vvc, GF_FALSE);
		ret = (vvc->last_parsed_vps_id>=0) ? 0 : -1;
		break;
	case GF_VVC_NALU_DEC_PARAM:
		ret = 0;
		break;
	case GF_VVC_NALU_APS_PREFIX:
		//we use the mix aps type + aps id (first 8 bits) as unique identifier
		vvc->last_parsed_aps_id = gf_bs_read_int_log(bs, 8, "aps_id");
		ret = 0;
		break;
	default:
		ret = 0;
		break;
	}
	if (gf_bs_is_overflow(bs)) ret = -1;

	/* save current POC lsb/msb to prev values */
	if ((ret>0) && vvc->s_info.sps) {
		if (!n_state.compute_poc_defer) {
			n_state.poc_lsb_prev = n_state.poc_lsb;
			n_state.poc_msb_prev = n_state.poc_msb;
		}
		if (is_slice)
			n_state.prev_layer_id_plus1 = *layer_id + 1;
	}

	memcpy(&vvc->s_info, &n_state, sizeof(VVCSliceInfo));

	return ret;
}

GF_EXPORT
s32 gf_vvc_parse_nalu(u8 *data, u32 size, VVCState *vvc, u8 *nal_unit_type, u8 *temporal_id, u8 *layer_id)
{
	GF_BitStream *bs = NULL;
	s32 ret;

	if (size<2) return -1;

	if (!vvc) {
		if (nal_unit_type) (*nal_unit_type) = data[1] >> 3;
		if (layer_id) (*layer_id) = data[0] & 0x3f;
		if (temporal_id) (*temporal_id) = (data[1] & 0x7);
		return -1;
	}
	bs = gf_bs_new(data, size, GF_BITSTREAM_READ);
	if (!bs) return -1;
	gf_bs_enable_emulation_byte_removal(bs, GF_TRUE);

	ret = gf_vvc_parse_nalu_bs(bs, vvc, nal_unit_type, temporal_id, layer_id);
	gf_bs_del(bs);
	return ret;
}

Bool gf_vvc_slice_is_ref(VVCState *vvc)
{
	if (!vvc->s_info.irap_or_gdr_pic) {
		return GF_FALSE;
	}
	if (vvc->s_info.gdr_pic) {
		if (vvc->s_info.recovery_point_valid) {
			vvc->s_info.recovery_point_valid = 0;
			return GF_TRUE;
		}
		return GF_FALSE;
	}
	return GF_TRUE;
}


GF_EXPORT
GF_Err gf_vvc_change_vui(GF_VVCConfig *vvcc, GF_VUIInfo *vui_info)
{
	GF_BitStream *orig, *mod;
	VVCState *vvc;
	u32 i, bit_offset, flag;
	s32 idx;
	GF_NALUFFParamArray *spss;
	GF_NALUFFParam *slc;
	orig = NULL;

	GF_SAFEALLOC(vvc, VVCState);
	if (!vvc) return GF_OUT_OF_MEM;
	vvc->sps_active_idx = -1;

	i = 0;
	spss = NULL;
	while ((spss = (GF_NALUFFParamArray *)gf_list_enum(vvcc->param_array, &i))) {
		if (spss->type == GF_VVC_NALU_SEQ_PARAM)
			break;
		spss = NULL;
	}
	if (!spss) {
		gf_free(vvc);
		return GF_NON_COMPLIANT_BITSTREAM;
	}

	i = 0;
	while ((slc = (GF_NALUFFParam *)gf_list_enum(spss->nalus, &i))) {
		u8 *no_emulation_buf;
		u32 no_emulation_buf_size, emulation_bytes;
		u8 nal_unit_type, temporal_id, layer_id;

		/*SPS may still contains emulation bytes*/
		no_emulation_buf = gf_malloc((slc->size) * sizeof(char));
		no_emulation_buf_size = gf_media_nalu_remove_emulation_bytes(slc->data, no_emulation_buf, slc->size);

		orig = gf_bs_new(no_emulation_buf, no_emulation_buf_size, GF_BITSTREAM_READ);
		vvc_parse_nal_header(orig, &nal_unit_type, &temporal_id, &layer_id);
		idx = gf_vvc_read_sps_bs_internal(orig, vvc, layer_id, &bit_offset);

		if (idx < 0) {
			if (orig)
				gf_bs_del(orig);
			gf_free(no_emulation_buf);
			gf_bs_del(orig);
			continue;
		}

		gf_bs_seek(orig, 0);
		mod = gf_bs_new(NULL, 0, GF_BITSTREAM_WRITE);

		/*copy over till vui flag*/
		while (bit_offset) {
			flag = gf_bs_read_int(orig, 1);
			gf_bs_write_int(mod, flag, 1);
			bit_offset--;
		}

		avc_hevc_vvc_rewrite_vui(vui_info, orig, mod, GF_CODECID_VVC);

		/*finally copy over remaining*/
		while (gf_bs_bits_available(orig)) {
			flag = gf_bs_read_int(orig, 1);
			gf_bs_write_int(mod, flag, 1);
		}
		gf_bs_del(orig);
		orig = NULL;
		gf_free(no_emulation_buf);

		/*set anti-emulation*/
		gf_bs_get_content(mod, &no_emulation_buf, &no_emulation_buf_size);
		emulation_bytes = gf_media_nalu_emulation_bytes_add_count(no_emulation_buf, no_emulation_buf_size);
		if (no_emulation_buf_size + emulation_bytes > slc->size)
			slc->data = (char*)gf_realloc(slc->data, no_emulation_buf_size + emulation_bytes);

		slc->size = gf_media_nalu_add_emulation_bytes(no_emulation_buf, slc->data, no_emulation_buf_size);

		gf_bs_del(mod);
		gf_free(no_emulation_buf);
	}
	gf_free(vvc);
	return GF_OK;
}


GF_EXPORT
GF_Err gf_vvc_change_par(GF_VVCConfig *vvcc, s32 ar_n, s32 ar_d)
{
	GF_VUIInfo vuii;
	memset(&vuii, 0, sizeof(GF_VUIInfo));
	vuii.ar_num = ar_n;
	vuii.ar_den = ar_d;
	vuii.fullrange = -1;
	vuii.video_format = -1;
	vuii.color_prim = -1;
	vuii.color_tfc = -1;
	vuii.color_matrix = -1;
	return gf_vvc_change_vui(vvcc, &vuii);
}

GF_EXPORT
GF_Err gf_vvc_change_color(GF_VVCConfig *vvcc, s32 fullrange, s32 vidformat, s32 colorprim, s32 transfer, s32 colmatrix)
{
	GF_VUIInfo vuii;
	memset(&vuii, 0, sizeof(GF_VUIInfo));
	vuii.ar_num = -1;
	vuii.ar_den = -1;
	vuii.fullrange = fullrange;
	vuii.video_format = vidformat;
	vuii.color_prim = colorprim;
	vuii.color_tfc = transfer;
	vuii.color_matrix = colmatrix;
	return gf_vvc_change_vui(vvcc, &vuii);
}


GF_EXPORT
GF_Err gf_vvc_get_sps_info(u8 *sps_data, u32 sps_size, u32 *sps_id, u32 *width, u32 *height, s32 *par_n, s32 *par_d)
{
	VVCState *vvc;
	s32 idx;
	GF_SAFEALLOC(vvc, VVCState);
	if (!vvc) return GF_OUT_OF_MEM;
	vvc->sps_active_idx = -1;

	GF_BitStream *bs = gf_bs_new(sps_data, sps_size, GF_BITSTREAM_READ);
	gf_bs_enable_emulation_byte_removal(bs, GF_TRUE);

	u8 nal_unit_type, temporal_id, layer_id;

	vvc_parse_nal_header(bs, &nal_unit_type, &temporal_id, &layer_id);
	idx = gf_vvc_read_sps_bs_internal(bs, vvc, layer_id, NULL);
	gf_bs_del(bs);
	if (idx < 0) {
		gf_free(vvc);
		return GF_NON_COMPLIANT_BITSTREAM;
	}

	if (sps_id) *sps_id = idx;

	if (width) *width = vvc->sps[idx].width;
	if (height) *height = vvc->sps[idx].height;
	if (par_n) *par_n = vvc->sps[idx].aspect_ratio_info_present_flag ? vvc->sps[idx].sar_width : (u32)-1;
	if (par_d) *par_d = vvc->sps[idx].aspect_ratio_info_present_flag ? vvc->sps[idx].sar_height : (u32)-1;
	gf_free(vvc);
	return GF_OK;
}


GF_EXPORT
const char *gf_vvc_get_profile_name(u8 video_prof)
{
	switch (video_prof) {
	case 1:
		return "Main 10";
	case 65:
		return "Main 10 Still Picture";
	case 17:
		return "Multilayer Main 10";
	case 2:
		return "Main 12";
	case 10:
		return "Main 12 Intra";
	case 66:
		return "Main 12 Still Picture";
	case 34:
		return "Main 12 4:4:4";
	case 42:
		return "Main 12 4:4:4 Intra";
	case 98:
		return "Main 12 4:4:4 Still Picture";
	case 36:
		return "Main 16 4:4:4";
	case 44:
		return "Main 16 4:4:4 Intra";
	case 100:
		return "Main 16 4:4:4 Still Picture";
	default:
		return "Unknown";
	}
}


GF_Err gf_media_vc1_seq_header_to_dsi(const u8 *seq_hdr, u32 seq_hdr_len, u8 **dsi, u32 *dsi_size)
{
	GF_BitStream *bs;
	u8 level=0, interlace=0;
	u8 profile=12;
	u8 *sqhdr = memchr(seq_hdr+1, 0x0F, seq_hdr_len);
	if (sqhdr) {
		u32 skip = (u32) (sqhdr - seq_hdr - 3);
		seq_hdr+=skip;
		seq_hdr_len-=skip;
		bs = gf_bs_new(seq_hdr+4, seq_hdr_len-4, GF_BITSTREAM_READ);
		profile = gf_bs_read_int(bs, 2);
		if (profile==3) {
			level = gf_bs_read_int(bs, 3);
			/*cfmt*/gf_bs_read_int(bs, 2);
			/*fps*/gf_bs_read_int(bs, 3);
			/*btrt*/gf_bs_read_int(bs, 5);
			gf_bs_read_int(bs, 1);
			/*mw*/gf_bs_read_int(bs, 12);
			/*mh*/gf_bs_read_int(bs, 12);
			/*bcast*/gf_bs_read_int(bs, 1);
			interlace = gf_bs_read_int(bs, 1);
		}
		gf_bs_del(bs);
	}
	*dsi_size = seq_hdr_len+7;
	*dsi = gf_malloc(seq_hdr_len+7);
	if (! (*dsi) ) return  GF_OUT_OF_MEM;

	bs = gf_bs_new(*dsi, *dsi_size, GF_BITSTREAM_WRITE);
	gf_bs_write_int(bs, 12, 4); //profile
	gf_bs_write_int(bs, level, 3); //level
	gf_bs_write_int(bs, 0, 1); //reserved
	gf_bs_write_int(bs, level, 3); //level
	gf_bs_write_int(bs, 0, 1); //cbr
	gf_bs_write_int(bs, 0, 6); //reserved
	gf_bs_write_int(bs, !interlace, 1); //no interlace
	gf_bs_write_int(bs, 1, 1); //no multiple seq
	gf_bs_write_int(bs, 1, 1); //no multiple entry
	gf_bs_write_int(bs, 1, 1); //no slice present
	gf_bs_write_int(bs, 0, 1); //no b-frames
	gf_bs_write_int(bs, 0, 1); //reserved
	gf_bs_write_u32(bs, 0xFFFFFFFF); //framerate
	gf_bs_write_data(bs, seq_hdr, seq_hdr_len); //VOS
	gf_bs_del(bs);
	return GF_OK;
}

void gf_hevc_parse_ps(GF_HEVCConfig* hevccfg, HEVCState* hevc, u32 nal_type)
{
	u32 i, j;
	if (!hevccfg) return;

	for (i = 0; i < gf_list_count(hevccfg->param_array); i++) {
		GF_NALUFFParamArray* ar = gf_list_get(hevccfg->param_array, i);
		if (ar->type != nal_type) continue;
		for (j = 0; j < gf_list_count(ar->nalus); j++) {
			u8 ntype, tid, lid;
			GF_NALUFFParam* sl = gf_list_get(ar->nalus, j);
			gf_hevc_parse_nalu(sl->data, sl->size, hevc, &ntype, &tid, &lid);
		}
	}
}

void gf_vvc_parse_ps(GF_VVCConfig* vvccfg, VVCState* vvc, u32 nal_type)
{
	u32 i, j;
	if (!vvccfg) return;

	for (i = 0; i < gf_list_count(vvccfg->param_array); i++) {
		GF_NALUFFParamArray* ar = gf_list_get(vvccfg->param_array, i);
		if (ar->type != nal_type) continue;
		for (j = 0; j < gf_list_count(ar->nalus); j++) {
			u8 ntype, tid, lid;
			GF_NALUFFParam* sl = gf_list_get(ar->nalus, j);
			gf_vvc_parse_nalu(sl->data, sl->size, vvc, &ntype, &tid, &lid);
		}
	}
}

#endif /*GPAC_DISABLE_AV_PARSERS*/

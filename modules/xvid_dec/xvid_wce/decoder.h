/*****************************************************************************
 *
 *  XVID MPEG-4 VIDEO CODEC
 *  - Decoder related header -
 *
 *  Copyright(C) 2002-2003 Peter Ross <pross@xvid.org>
 *
 *  This program is free software ; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation ; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY ; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program ; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
 *
 * $Id: decoder.h,v 1.1.1.1 2005-07-13 14:36:13 jeanlf Exp $
 *
 ****************************************************************************/

#ifndef _DECODER_H_
#define _DECODER_H_

#include "xvid.h"
#include "portab.h"
#include "global.h"
#include "image.h"
#include "vlc_codes.h"
#include "Interpolate8x8.h"

/*****************************************************************************
 * Structures
 ****************************************************************************/

/* complexity estimation toggles */
struct ESTIMATION {
int method;

int opaque;
int transparent;
int intra_cae;
int inter_cae;
int no_update;
int upsampling;

int intra_blocks;
int inter_blocks;
int inter4v_blocks;
int gmc_blocks;
int not_coded_blocks;

int dct_coefs;
int dct_lines;
int vlc_symbols;
int vlc_bits;

int apm;
int npm;
int interpolate_mc_q;
int forw_back_mc_q;
int halfpel2;
int halfpel4;

int sadct;
int quarterpel;

ESTIMATION() {
	MemSet(this, 0, sizeof(ESTIMATION));
}
};

//----------------------------

struct S_decoder {
private:
#ifdef PROFILE
mutable C_profiler &prof;
#endif

int Resize();

//----------------------------
void SetEdges(IMAGE &img) const;

//----------------------------
void I_Frame(Bitstream * bs, bool reduced_resolution, int quant, int intra_dc_threshold);
void P_Frame(Bitstream * bs, int rounding, bool reduced_resolution, int quant, int fcode, int intra_dc_threshold, const WARPPOINTS *const gmc_warp);
void MBIntra(MACROBLOCK *pMB, dword x_pos, dword y_pos, dword acpred_flag, dword cbp,
             Bitstream *bs, dword quant, dword intra_dc_threshold, dword bound, bool reduced_resolution);

void mb_decode(const dword cbp, Bitstream * bs, byte * pY_Cur, byte * pU_Cur, byte * pV_Cur, bool reduced_resolution, const MACROBLOCK * pMB);

void DecodeInterMacroBlock(const MACROBLOCK *pMB, dword x_pos, dword y_pos, dword cbp, Bitstream *bs,
                           bool rounding, bool reduced_resolution, int ref);

void mbgmc(MACROBLOCK *pMB, dword x_pos, dword y_pos, dword fcode, dword cbp, Bitstream *bs, bool rounding);

void BFrameInterpolateMBInter(const IMAGE &forward, const IMAGE &backward, const MACROBLOCK *pMB, dword x_pos, dword y_pos, Bitstream *bs, int direct);

void B_Frame(Bitstream * bs, int quant, int fcode_forward, int fcode_backward);
void GetMotionVector(Bitstream *bs, int x, int y, int k, VECTOR *ret_mv, int fcode, int bound);
//void Output(IMAGE *img, xvid_dec_frame_t *frame, xvid_dec_stats_t *stats, int coding_type);

int read_video_packet_header(Bitstream *bs, const int addbits, int *quant, int *fcode_forward, int *fcode_backward, int *intra_dc_threshold);
void read_vol_complexity_estimation_header(Bitstream * bs);
int BitstreamReadHeaders(Bitstream * bs, bool &rounding, bool *reduced_resolution, dword *quant, dword *fcode_forward,
                         dword *fcode_backward, dword *intra_dc_threshold, WARPPOINTS *gmc_warp);
void read_vop_complexity_estimation_header(Bitstream * bs, int coding_type);

int get_coeff(Bitstream *bs, int *run, int *last, int intra, int short_video_header);
void get_intra_block(Bitstream * bs, int *block, int direction, int coeff);
void get_inter_block(Bitstream * bs, int *block, int direction);

void idct_int32_init();
void InverseDiscreteCosineTransform(int *block) const;

friend int decoder_create(xvid_dec_create_t *create);

int Init(xvid_dec_create_t *create);

#ifdef PROFILE
void interpolate8x8_quarterpel(byte *cur, byte *refn, byte *refh, byte *refv, byte *refhv, dword x, dword y, int dx, int dy, dword stride, bool rounding) {
	::interpolate8x8_quarterpel(cur, refn, refh, refv, refhv, x, y, dx, dy, stride, rounding);
}
void interpolate16x16_switch(byte *cur, const byte *refn, dword x, dword y, int dx, int dy, dword stride, bool rounding) {
	interpolate8x8_switch(cur, refn, x,   y,   dx, dy, stride, rounding);
	interpolate8x8_switch(cur, refn, x+8, y,   dx, dy, stride, rounding);
	interpolate8x8_switch(cur, refn, x,   y+8, dx, dy, stride, rounding);
	interpolate8x8_switch(cur, refn, x+8, y+8, dx, dy, stride, rounding);
}
void interpolate8x8_switch(byte *cur, const byte *refn, dword x, dword y, int dx, int dy, dword stride, bool rounding) {
	const byte *src = refn + ((y + (dy>>1)) * stride + x + (dx>>1));
	byte *dst = cur + (y * stride + x);

	int code = ((dx & 1) << 1) + (dy & 1);
	//PROF_S(PROF_4+MIN(3, code));
	switch(code) {
	case 0:
		transfer8x8_copy(dst, src, stride);
		break;
	case 1:
		interpolate8x8_halfpel_v(dst, src, stride, rounding);
		break;
	case 2:
		interpolate8x8_halfpel_h(dst, src, stride, rounding);
		break;
	default:
		interpolate8x8_halfpel_hv(dst, src, stride, rounding);
		break;
	}
	//PROF_E(PROF_4+MIN(3, code));
}
#endif
public:
S_decoder(xvid_dec_create_t *create);
~S_decoder();
//vol bitstream
int time_inc_resolution;
int fixed_time_inc;
dword time_inc_bits;

dword shape;
dword quant_bits;
dword quant_type;
dword *mpeg_quant_matrices;
int quarterpel;
int complexity_estimation_disable;
ESTIMATION estimation;

dword top_field_first;
dword alternate_vertical_scan;

int aspect_ratio;
int par_width;
int par_height;

int sprite_enable;
int sprite_warping_points;
int sprite_warping_accuracy;
int sprite_brightness_change;

bool interlacing;
bool newpred_enable;
bool reduced_resolution_enable;

/* The bitstream version if it's a XviD stream */
int bs_version;

/* image */

dword width;
dword height;
dword edged_width;
dword edged_height;

IMAGE cur;
IMAGE refn[2];          /* 0   -- last I or P VOP */

/* 1   -- first I or P */
IMAGE tmp;     /* bframe interpolation, and post processing tmp buffer */
IMAGE qtmp;    /* quarter pel tmp buffer */

/* macroblock */

dword mb_width;
dword mb_height;
MACROBLOCK *mbs;

//for B-frame & low_delay==0
// XXX: should move frame based stuff into a DECODER_FRAMEINFO struct
MACROBLOCK *last_mbs;         /* last MB */
int last_coding_type;           /* last coding type value */
int frames;            /* total frame number */
VECTOR p_fmv, p_bmv;    /* pred forward & backward motion vector */
int64_t time;           /* for record time */
int64_t time_base;
int64_t last_time_base;
int64_t last_non_b_time;
dword time_pp;
dword time_bp;
bool fixed_dimensions;
bool scalability;
bool low_delay;            //low_delay flage (1 means no B_VOP)
bool low_delay_default;    //default value for low_delay flag
bool last_reduced_resolution; //last reduced_resolution value
bool packed_mode;          //bframes packed bitstream? (1 = yes)

//for GMC: central place for all parameters

IMAGE gmc;     /* gmc tmp buffer, remove for blockbased compensation */
GMC_DATA gmc_data;
NEW_GMC_DATA new_gmc_data;

/*
#ifdef XVID_CSP_SLICE
xvid_image_t* out_frm;                //This is used for slice rendering
#endif
*/

struct REVERSE_EVENT {
	byte len;
	EVENT event;
};
//decoding tables
REVERSE_EVENT DCT3D[2][4096];
VLC coeff_VLC[2][2][64][64];

typedef int t_clip_val;
t_clip_val iclip[1024];            //clipping table

void init_vlc_tables();
int Decode(xvid_dec_frame_t * frame, xvid_dec_stats_t * stats);
};

/*****************************************************************************
 * Decoder prototypes
 ****************************************************************************/

void init_decoder(dword cpu_flags);

int decoder_create(xvid_dec_create_t * param);


#endif

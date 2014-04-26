/*****************************************************************************
 *
 *  XVID MPEG-4 VIDEO CODEC
 *  - Global definitions  -
 *
 *  Copyright(C) 2002 Michael Militzer <isibaar@xvid.org>
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
 * $Id: global.h,v 1.1.1.1 2005-07-13 14:36:14 jeanlf Exp $
 *
 ****************************************************************************/

#ifndef _GLOBAL_H_
#define _GLOBAL_H_

#include "xvid.h"
#include "portab.h"

//----------------------------

void MemSet(void *dst, byte c, dword len);
int MemCmp(const void *mem1, const void *mem2, dword len);
void MemCpy(void *dst, const void *src, dword len);

template<class T>
inline void Swap(T &l, T &r) {
T tmp = r;
r = l;
l = tmp;
}

#ifndef __SYMBIAN32__

enum TLeave { ELeave };
void *operator new(size_t sz, TLeave);
inline void operator delete(void *vp, TLeave) {
	operator delete(vp);
}

#endif

//----------------------------
// Fatal error - display message, and exit program immediately.
void Fatal(const char *msg, dword code = 0);

//----------------------------
#ifndef assert

#ifdef NDEBUG
#define assert(exp) ((void)0)
#else

#define assert(exp) if(!(exp)){\
   Fatal(#exp, __LINE__); }

#endif
#endif

//----------------------------
/* --- macroblock modes --- */

#define MODE_INTER      0
#define MODE_INTER_Q 1
#define MODE_INTER4V 2
#define  MODE_INTRA     3
#define MODE_INTRA_Q 4
#define MODE_NOT_CODED  16
#define MODE_NOT_CODED_GMC 17

/* --- bframe specific --- */

#define MODE_DIRECT        0
#define MODE_INTERPOLATE   1
#define MODE_BACKWARD      2
#define MODE_FORWARD    3
#define MODE_DIRECT_NONE_MV   4
#define MODE_DIRECT_NO4V   5


/*
 * vop coding types
 * intra, prediction, backward, sprite, not_coded
 */
#define I_VOP  0
#define P_VOP  1
#define B_VOP  2
#define S_VOP  3
#define N_VOP  4

//----------------------------
// convert mpeg-4 coding type i/p/b/s_VOP to XVID_TYPE_xxx
inline int coding2type(int coding_type) {
	return coding_type + 1;
}

//----------------------------
// convert XVID_TYPE_xxx to bitstream coding type i/p/b/s_VOP
inline int type2coding(int xvid_type) {
	return xvid_type - 1;
}


typedef struct
{
	int x;
	int y;
}
VECTOR;



typedef struct
{
	VECTOR duv[3];
}
WARPPOINTS;

/* save all warping parameters for GMC once and for all, instead of
   recalculating for every block. This is needed for encoding&decoding
   When switching to incremental calculations, this will get much shorter
*/

/* we don't include WARPPOINTS wp   here, but in FRAMEINFO itself */

struct GMC_DATA {
	int num_wp;    /* [input]: 0=none, 1=translation, 2,3 = warping */
	/* a value of -1 means: "structure not initialized!" */
	int s;               /* [input]: calc is done with 1/s pel resolution */

	int W;
	int H;

	int ss;
	int smask;
	int sigma;

	int r;
	int rho;

	int i0s;
	int j0s;
	int i1s;
	int j1s;
	int i2s;
	int j2s;

	int i1ss;
	int j1ss;
	int i2ss;
	int j2ss;

	int alpha;
	int beta;
	int Ws;
	int Hs;

	int dxF, dyF, dxG, dyG;
	int Fo, Go;
	int cFo, cGo;

	GMC_DATA() {
		MemSet(this, 0, sizeof(GMC_DATA));
	}
};

struct NEW_GMC_DATA {
	/*  0=none, 1=translation, 2,3 = warping
	*  a value of -1 means: "structure not initialized!" */
	int num_wp;

	/* {0,1,2,3}  =>   {1/2,1/4,1/8,1/16} pel */
	int accuracy;

	/* sprite size * 16 */
	int sW, sH;

	/* gradient, calculated from warp points */
	int dU[2], dV[2], Uo, Vo, Uco, Vco;

	void (*predict_16x16)(const NEW_GMC_DATA * const This,
	                      byte *dst, const byte *src,
	                      int dststride, int srcstride, int x, int y, int rounding);
	void (*predict_8x8)  (const NEW_GMC_DATA * const This,
	                      byte *uDst, const byte *uSrc,
	                      byte *vDst, const byte *vSrc,
	                      int dststride, int srcstride, int x, int y, int rounding);
	void (*get_average_mv)(const NEW_GMC_DATA * const Dsp, VECTOR * const mv,
	                       int x, int y, int qpel);

	NEW_GMC_DATA() {
		MemSet(this, 0, sizeof(NEW_GMC_DATA));
	}
};

//----------------------------

struct IMAGE: public C_xvid_image {
private:
	void draw_num(const int stride, const int height, const char * font, const int x, const int y);
public:
	void Print(int edged_width, int height, int x, int y, const char *fmt);
	void Swap(IMAGE *image2);
	void Copy(const IMAGE * image2, dword edged_width, dword height);
	void Clear(int width, int height, int edged_width, int y, int u, int v);
	void deblock_rrv(int edged_width, const struct MACROBLOCK * mbs, int mb_width, int mb_height, int mb_stride, int block, int flags);

	inline void Null() {
		y = u = v = 0;
	}
};

//----------------------------

struct Bitstream {
	dword bufa;
	dword bufb;
	dword buf;
	dword pos;                //bit position in currently cached 2 dwords (0-31)
	dword *tail;
	dword *start;
	dword length;
	dword initpos;

	void Init(const void *bitstream, dword length);
	dword ShowBits(dword bits);
	void get_matrix(byte *matrix);
	void Skip(dword bits);

//----------------------------
// number of bits to next byte alignment
	inline dword NumBitsToByteAlign() const {
		dword n = (32 - pos) & 7;
		return n == 0 ? 8 : n;
	}
	dword ShowBitsFromByteAlign(int bits);

//----------------------------
// move forward to the next byte boundary
	void ByteAlign() {
		dword remainder = pos & 7;
		if (remainder) {
			Skip(8 - remainder);
		}
	}

//----------------------------
// bitstream length (unit bits)
	inline dword Pos() const {
		return((dword)(8*((dword)tail - (dword)start) + pos - initpos));
	}

	dword GetBits(const dword n);

//----------------------------
// read single bit from bitstream
	inline dword GetBit() {
		return GetBits(1);
	}

//----------------------------

	int GetMcbpcInter();
	int GetCbpy(int intra);
	int GetMoveVector(int fcode);

//----------------------------

	int check_resync_marker(int addbits);
	int bs_get_spritetrajectory();
	int get_mcbpc_intra();
	int get_dc_dif(dword dc_size);
	int get_dc_size_lum();
	int get_dc_size_chrom();
};


#define MBPRED_SIZE  15


struct MACROBLOCK {
	/* decoder/encoder */
	VECTOR mvs[4];

	int pred_values[6][MBPRED_SIZE];
	int acpred_directions[6];

	int mode;
	int quant;              /* absolute quant */

	int field_dct;
	int field_pred;
	int field_for_top;
	int field_for_bot;

	/* encoder specific */

	VECTOR mv16;
	VECTOR pmvs[4];
	VECTOR qmvs[4];            /* mvs in quarter pixel resolution */

	int sad8[4];        /* SAD values for inter4v-VECTORs */
	int sad16;          /* SAD value for inter-VECTOR */

	int dquant;
	int cbp;

	/* bframe stuff */

	VECTOR b_mvs[4];
	VECTOR b_qmvs[4];

	int mb_type;

	/*
	 * stuff for block based ME (needed for Qpel ME)
	 * backup of last integer ME vectors/sad
	 */

	VECTOR amv; /* average motion vectors from GMC  */
	int mcsel;

	/* This structure has become way to big! What to do? Split it up?   */

};

//----------------------------

inline dword log2bin(dword value) {
	int n = 0;

	while (value) {
		value >>= 1;
		n++;
	}
	return n;
}

//----------------------------

/* useful macros */

#define MIN(X, Y) ((X)<(Y)?(X):(Y))
#define MAX(X, Y) ((X)>(Y)?(X):(Y))
#define ABS(X)    (((X)>0)?(X):-(X))
#define SIGN(X)   (((X)>0)?1:-1)
#define CLIP(X,AMIN,AMAX)   (((X)<(AMIN)) ? (AMIN) : ((X)>(AMAX)) ? (AMAX) : (X))

#endif                     /* _GLOBAL_H_ */

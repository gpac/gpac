/*****************************************************************************
 *
 *  XVID MPEG-4 VIDEO CODEC
 *  - GMC interpolation module -
 *
 *  Copyright(C) 2002-2003 Pascal Massimino <skal@planet-d.net>
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
 * $Id: gmc.cpp,v 1.1.1.1 2005-07-13 14:36:14 jeanlf Exp $
 *
 ****************************************************************************/

#include "portab.h"
#include "global.h"
#include "gmc.h"

/* ************************************************************
 * Pts = 2 or 3
 *
 * Warning! *src is the global frame pointer (that is: address
 * of pixel 0,0), not the macroblock one.
 * Conversely, *dst is the macroblock top-left address.
 */
static void Predict_16x16_C(const NEW_GMC_DATA * const This, byte *dst, const byte *src, int dststride, int srcstride, int x, int y, int rounding) {

	const int W = This->sW;
	const int H = This->sH;
	const int rho = 3 - This->accuracy;
	const int Rounder = ( (1<<7) - (rounding<<(2*rho)) ) << 16;

	const int dUx = This->dU[0];
	const int dVx = This->dV[0];
	const int dUy = This->dU[1];
	const int dVy = This->dV[1];

	int Uo = This->Uo + 16*(dUy*y + dUx*x);
	int Vo = This->Vo + 16*(dVy*y + dVx*x);

	int i, j;

	dst += 16;
	for (j=16; j>0; --j) {
		int U = Uo, V = Vo;
		Uo += dUy;
		Vo += dVy;
		for (i=-16; i<0; ++i) {
			unsigned int f0, f1, ri = 16, rj = 16;
			int Offset;
			int u = ( U >> 16 ) << rho;
			int v = ( V >> 16 ) << rho;

			U += dUx;
			V += dVx;

			if (u > 0 && u <= W) {
				ri = MTab[u&15];
				Offset = u>>4;
			}
			else if (u > W) Offset = W>>4;
			else Offset = -1;

			if (v > 0 && v <= H) {
				rj = MTab[v&15];
				Offset += (v>>4)*srcstride;
			}
			else if (v > H) Offset += (H>>4)*srcstride;
			else Offset -= srcstride;

			f0 = src[Offset + 0];
			f0 |= src[Offset + 1] << 16;
			f1 = src[Offset + srcstride + 0];
			f1 |= src[Offset + srcstride + 1] << 16;
			f0 = (ri*f0)>>16;
			f1 = (ri*f1) & 0x0fff0000;
			f0 |= f1;
			f0 = (rj*f0 + Rounder) >> 24;

			dst[i] = (byte)f0;
		}
		dst += dststride;
	}
}

//----------------------------

static void Predict_8x8_C(const NEW_GMC_DATA * const This, byte *uDst, const byte *uSrc,
                          byte *vDst, const byte *vSrc, int dststride, int srcstride, int x, int y, int rounding) {

	const int W  = This->sW >> 1;
	const int H  = This->sH >> 1;
	const int rho = 3-This->accuracy;
	const int Rounder = ( 128 - (rounding<<(2*rho)) ) << 16;

	const int dUx = This->dU[0];
	const int dVx = This->dV[0];
	const int dUy = This->dU[1];
	const int dVy = This->dV[1];

	int Uo = This->Uco + 8*(dUy*y + dUx*x);
	int Vo = This->Vco + 8*(dVy*y + dVx*x);

	int i, j;

	uDst += 8;
	vDst += 8;
	for (j=8; j>0; --j) {
		int U = Uo, V = Vo;
		Uo += dUy;
		Vo += dVy;

		for (i=-8; i<0; ++i) {
			int Offset;
			dword f0, f1, ri, rj;
			int u, v;

			u = ( U >> 16 ) << rho;
			v = ( V >> 16 ) << rho;
			U += dUx;
			V += dVx;

			if (u > 0 && u <= W) {
				ri = MTab[u&15];
				Offset = u>>4;
			} else {
				ri = 16;
				if (u>W) Offset = W>>4;
				else Offset = -1;
			}

			if (v > 0 && v <= H) {
				rj = MTab[v&15];
				Offset += (v>>4)*srcstride;
			} else {
				rj = 16;
				if (v>H) Offset += (H>>4)*srcstride;
				else Offset -= srcstride;
			}

			f0 = uSrc[Offset + 0];
			f0 |= uSrc[Offset + 1] << 16;
			f1 = uSrc[Offset + srcstride + 0];
			f1 |= uSrc[Offset + srcstride + 1] << 16;
			f0 = (ri*f0)>>16;
			f1 = (ri*f1) & 0x0fff0000;
			f0 |= f1;
			f0 = (rj*f0 + Rounder) >> 24;

			uDst[i] = (byte)f0;

			f0 = vSrc[Offset + 0];
			f0 |= vSrc[Offset + 1] << 16;
			f1 = vSrc[Offset + srcstride + 0];
			f1 |= vSrc[Offset + srcstride + 1] << 16;
			f0 = (ri*f0)>>16;
			f1 = (ri*f1) & 0x0fff0000;
			f0 |= f1;
			f0 = (rj*f0 + Rounder) >> 24;

			vDst[i] = (byte)f0;
		}
		uDst += dststride;
		vDst += dststride;
	}
}

//----------------------------

static void get_average_mv_C(const NEW_GMC_DATA * const Dsp, VECTOR * const mv, int x, int y, int qpel) {

	int i, j;
	int vx = 0, vy = 0;
	int uo = Dsp->Uo + 16*(Dsp->dU[1]*y + Dsp->dU[0]*x);
	int vo = Dsp->Vo + 16*(Dsp->dV[1]*y + Dsp->dV[0]*x);
	for (j=16; j>0; --j)
	{
		int U, V;
		U = uo;
		uo += Dsp->dU[1];
		V = vo;
		vo += Dsp->dV[1];
		for (i=16; i>0; --i)
		{
			int u,v;
			u = U >> 16;
			U += Dsp->dU[0];
			vx += u;
			v = V >> 16;
			V += Dsp->dV[0];
			vy += v;
		}
	}
	vx -= (256*x+120) << (5+Dsp->accuracy);   /* 120 = 15*16/2 */
	vy -= (256*y+120) << (5+Dsp->accuracy);

	mv->x = RSHIFT( vx, 8+Dsp->accuracy - qpel );
	mv->y = RSHIFT( vy, 8+Dsp->accuracy - qpel );
}

//----------------------------
/* ************************************************************
 * simplified version for 1 warp point
 */
static void Predict_1pt_16x16_C(const NEW_GMC_DATA * const This, byte *Dst, const byte *Src, int dststride, int srcstride, int x, int y, int rounding) {

	const int W  = This->sW;
	const int H  = This->sH;
	const int rho = 3-This->accuracy;
	const int Rounder = ( 128 - (rounding<<(2*rho)) ) << 16;


	int uo = This->Uo + (x<<8);    /* ((16*x)<<4) */
	int vo = This->Vo + (y<<8);
	const dword ri = MTab[uo & 15];
	const dword rj = MTab[vo & 15];
	int i, j;

	int Offset;
	if ((dword)vo<=(dword)H) Offset  = (vo>>4)*srcstride;
	else if (vo>H)           Offset  = ( H>>4)*srcstride;
	else                     Offset  =-16*srcstride;
	if ((dword)uo<=(dword)W) Offset += (uo>>4);
	else if (uo>W)           Offset += ( W>>4);
	else                     Offset -= 16;

	Dst += 16;

	for(j=16; j>0; --j, Offset+=srcstride-16)
	{
		for(i=-16; i<0; ++i, ++Offset)
		{
			dword f0, f1;
			f0 = Src[ Offset     +0 ];
			f0 |= Src[ Offset    +1 ] << 16;
			f1 = Src[ Offset+srcstride +0 ];
			f1 |= Src[ Offset+srcstride +1 ] << 16;
			f0 = (ri*f0)>>16;
			f1 = (ri*f1) & 0x0fff0000;
			f0 |= f1;
			f0 = ( rj*f0 + Rounder ) >> 24;
			Dst[i] = (byte)f0;
		}
		Dst += dststride;
	}
}

//----------------------------

static void Predict_1pt_8x8_C(const NEW_GMC_DATA * const This, byte *uDst, const byte *uSrc,
                              byte *vDst, const byte *vSrc, int dststride, int srcstride, int x, int y, int rounding) {

	const int W  = This->sW >> 1;
	const int H  = This->sH >> 1;
	const int rho = 3-This->accuracy;
	const int Rounder = ( 128 - (rounding<<(2*rho)) ) << 16;

	int uo = This->Uco + (x<<7);
	int vo = This->Vco + (y<<7);
	const dword rri = MTab[uo & 15];
	const dword rrj = MTab[vo & 15];
	int i, j;

	int Offset;
	if ((dword)vo<=(dword)H) Offset = (vo>>4)*srcstride;
	else if (vo>H) Offset = ( H>>4)*srcstride;
	else Offset =-8*srcstride;
	if ((dword)uo<=(dword)W) Offset += (uo>>4);
	else if (uo>W) Offset += (W>>4);
	else Offset -= 8;

	uDst += 8;
	vDst += 8;
	for(j=8; j>0; --j, Offset+=srcstride-8)
	{
		for(i=-8; i<0; ++i, Offset++)
		{
			dword f0, f1;
			f0 = uSrc[ Offset + 0 ];
			f0 |= uSrc[ Offset + 1 ] << 16;
			f1 = uSrc[ Offset + srcstride + 0 ];
			f1 |= uSrc[ Offset + srcstride + 1 ] << 16;
			f0 = (rri*f0)>>16;
			f1 = (rri*f1) & 0x0fff0000;
			f0 |= f1;
			f0 = ( rrj*f0 + Rounder ) >> 24;
			uDst[i] = (byte)f0;

			f0 = vSrc[ Offset + 0 ];
			f0 |= vSrc[ Offset + 1 ] << 16;
			f1 = vSrc[ Offset + srcstride + 0 ];
			f1 |= vSrc[ Offset + srcstride + 1 ] << 16;
			f0 = (rri*f0)>>16;
			f1 = (rri*f1) & 0x0fff0000;
			f0 |= f1;
			f0 = ( rrj*f0 + Rounder ) >> 24;
			vDst[i] = (byte)f0;
		}
		uDst += dststride;
		vDst += dststride;
	}
}

//----------------------------

static void get_average_mv_1pt_C(const NEW_GMC_DATA *const Dsp, VECTOR * const mv, int x, int y, int qpel) {

	mv->x = RSHIFT(Dsp->Uo<<qpel, 3);
	mv->y = RSHIFT(Dsp->Vo<<qpel, 3);
}

//----------------------------

void generate_GMCparameters(int nb_pts, int accuracy, const WARPPOINTS *pts, int width, int height, NEW_GMC_DATA *gmc) {

	gmc->sW = width   << 4;
	gmc->sH = height << 4;
	gmc->accuracy = accuracy;
	gmc->num_wp = nb_pts;

	//reduce the number of points, if possible
	if(nb_pts<3 || (pts->duv[2].x==-pts->duv[1].y && pts->duv[2].y==pts->duv[1].x)) {
		if(nb_pts<2 || (pts->duv[1].x==0 && pts->duv[1].y==0)) {
			if(nb_pts<1 || (pts->duv[0].x==0 && pts->duv[0].y==0)) {
				nb_pts = 0;
			} else
				nb_pts = 1;
		} else
			nb_pts = 2;
	} else
		nb_pts = 3;

	//now, nb_pts stores the actual number of points required for interpolation
	if(nb_pts<=1) {
		if(nb_pts==1) {
			/* store as 4b fixed point */
			gmc->Uo = pts->duv[0].x << accuracy;
			gmc->Vo = pts->duv[0].y << accuracy;
			gmc->Uco = ((pts->duv[0].x>>1) | (pts->duv[0].x&1)) << accuracy;   /* DIV2RND() */
			gmc->Vco = ((pts->duv[0].y>>1) | (pts->duv[0].y&1)) << accuracy;   /* DIV2RND() */
		} else {  /* zero points?! */
			gmc->Uo  = gmc->Vo   = 0;
			gmc->Uco = gmc->Vco = 0;
		}

		gmc->predict_16x16   = Predict_1pt_16x16_C;
		gmc->predict_8x8  = Predict_1pt_8x8_C;
		gmc->get_average_mv = get_average_mv_1pt_C;
	} else {     /* 2 or 3 points */
		const int rho   = 3 - accuracy;  /* = {3,2,1,0} for Acc={0,1,2,3} */
		int Alpha = log2bin(width-1);
		int Ws = 1 << Alpha;

		gmc->dU[0] = 16*Ws + RDIV( 8*Ws*pts->duv[1].x, width );   /* dU/dx */
		gmc->dV[0] =       RDIV( 8*Ws*pts->duv[1].y, width );  /* dV/dx */

		/*  disabled, because possibly buggy? */

#if 0
		if (nb_pts==2) {
			gmc->dU[1] = -gmc->dV[0];  /* -Sin */
			gmc->dV[1] =   gmc->dU[0] ;   /* Cos */
		}
		else
#endif
		{
			const int Beta = log2bin(height-1);
			const int Hs = 1<<Beta;
			gmc->dU[1] =       RDIV( 8*Hs*pts->duv[2].x, height );    /* dU/dy */
			gmc->dV[1] = 16*Hs + RDIV( 8*Hs*pts->duv[2].y, height );  /* dV/dy */
			if (Beta>Alpha) {
				gmc->dU[0] <<= (Beta-Alpha);
				gmc->dV[0] <<= (Beta-Alpha);
				Alpha = Beta;
				Ws = Hs;
			}
			else {
				gmc->dU[1] <<= Alpha - Beta;
				gmc->dV[1] <<= Alpha - Beta;
			}
		}
		/* upscale to 16b fixed-point */
		gmc->dU[0] <<= (16-Alpha - rho);
		gmc->dU[1] <<= (16-Alpha - rho);
		gmc->dV[0] <<= (16-Alpha - rho);
		gmc->dV[1] <<= (16-Alpha - rho);

		gmc->Uo  = ( pts->duv[0].x  <<(16+ accuracy)) + (1<<15);
		gmc->Vo  = ( pts->duv[0].y  <<(16+ accuracy)) + (1<<15);
		gmc->Uco = ((pts->duv[0].x-1)<<(17+ accuracy)) + (1<<17);
		gmc->Vco = ((pts->duv[0].y-1)<<(17+ accuracy)) + (1<<17);
		gmc->Uco = (gmc->Uco + gmc->dU[0] + gmc->dU[1])>>2;
		gmc->Vco = (gmc->Vco + gmc->dV[0] + gmc->dV[1])>>2;

		gmc->predict_16x16   = Predict_16x16_C;
		gmc->predict_8x8  = Predict_8x8_C;
		gmc->get_average_mv = get_average_mv_C;
	}
}

//----------------------------
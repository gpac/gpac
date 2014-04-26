/*****************************************************************************
 *
 *  XVID MPEG-4 VIDEO CODEC
 *  - GMC interpolation module header -
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
 * $Id: gmc.h,v 1.1.1.1 2005-07-13 14:36:14 jeanlf Exp $
 *
 ****************************************************************************/

#include "portab.h"
#include "global.h"


#define RDIV(a,b) (((a)>0 ? (a) + ((b)>>1) : (a) - ((b)>>1))/(b))
#define RSHIFT(a,b) ( (a)>0 ? ((a) + (1<<((b)-1)))>>(b) : ((a) + (1<<((b)-1))-1)>>(b))

#define MLT(i)  (((16-(i))<<16) + (i))
static const dword MTab[16] = {
	MLT( 0), MLT( 1), MLT( 2), MLT( 3), MLT( 4), MLT( 5), MLT( 6), MLT( 7),
	MLT( 8), MLT( 9), MLT(10), MLT(11), MLT(12), MLT(13), MLT(14), MLT(15)
};
#undef MLT


/* *************************************************************
 * Warning! It's Accuracy being passed, not 'resolution'!
 */
void generate_GMCparameters(int nb_pts, int accuracy, const WARPPOINTS *pts, int width, int height, NEW_GMC_DATA *gmc);


/*****************************************************************************
 *
 *  XVID MPEG-4 VIDEO CODEC
 *  - Bitstream reader/writer inlined functions and constants-
 *
 *  Copyright (C) 2001-2003 Peter Ross <pross@xvid.org>
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
 * $Id: bitstream.h,v 1.1.1.1 2005-07-13 14:36:12 jeanlf Exp $
 *
 ****************************************************************************/

#ifndef _BITSTREAM_H_
#define _BITSTREAM_H_

#include "portab.h"
#include "decoder.h"


/*****************************************************************************
 * Constants
 ****************************************************************************/

/* comment any #defs we dont use */

#define VIDOBJ_START_CODE		0x00000100	/* ..0x0000011f  */
#define VIDOBJLAY_START_CODE	0x00000120	/* ..0x0000012f */
#define VISOBJSEQ_START_CODE	0x000001b0
#define VISOBJSEQ_STOP_CODE		0x000001b1	/* ??? */
#define USERDATA_START_CODE		0x000001b2
#define GRPOFVOP_START_CODE		0x000001b3
/*#define VIDSESERR_ERROR_CODE  0x000001b4 */
#define VISOBJ_START_CODE		0x000001b5
#define VOP_START_CODE			0x000001b6
/*#define STUFFING_START_CODE	0x000001c3 */


#define VISOBJ_TYPE_VIDEO				1
/*#define VISOBJ_TYPE_STILLTEXTURE      2 */
/*#define VISOBJ_TYPE_MESH              3 */
/*#define VISOBJ_TYPE_FBA               4 */
/*#define VISOBJ_TYPE_3DMESH            5 */


#define VIDOBJLAY_TYPE_SIMPLE			1
/*#define VIDOBJLAY_TYPE_SIMPLE_SCALABLE    2 */
/*#define VIDOBJLAY_TYPE_CORE				3 */
/*#define VIDOBJLAY_TYPE_MAIN				4 */
/*#define VIDOBJLAY_TYPE_NBIT				5 */
/*#define VIDOBJLAY_TYPE_ANIM_TEXT			6 */
/*#define VIDOBJLAY_TYPE_ANIM_MESH			7 */
/*#define VIDOBJLAY_TYPE_SIMPLE_FACE		8 */
/*#define VIDOBJLAY_TYPE_STILL_SCALABLE		9 */
#define VIDOBJLAY_TYPE_ART_SIMPLE		10
/*#define VIDOBJLAY_TYPE_CORE_SCALABLE		11 */
/*#define VIDOBJLAY_TYPE_ACE				12 */
/*#define VIDOBJLAY_TYPE_ADVANCED_SCALABLE_TEXTURE 13 */
/*#define VIDOBJLAY_TYPE_SIMPLE_FBA			14 */
/*#define VIDEOJLAY_TYPE_SIMPLE_STUDIO    15*/
/*#define VIDEOJLAY_TYPE_CORE_STUDIO      16*/
#define VIDOBJLAY_TYPE_ASP              17
/*#define VIDOBJLAY_TYPE_FGS              18*/


/*#define VIDOBJLAY_AR_SQUARE           1 */
/*#define VIDOBJLAY_AR_625TYPE_43       2 */
/*#define VIDOBJLAY_AR_525TYPE_43       3 */
/*#define VIDOBJLAY_AR_625TYPE_169      8 */
/*#define VIDOBJLAY_AR_525TYPE_169      9 */
#define VIDOBJLAY_AR_EXTPAR				15


#define VIDOBJLAY_SHAPE_RECTANGULAR		0
#define VIDOBJLAY_SHAPE_BINARY			1
#define VIDOBJLAY_SHAPE_BINARY_ONLY		2
#define VIDOBJLAY_SHAPE_GRAYSCALE		3


#define SPRITE_NONE		0
#define SPRITE_STATIC	1
#define SPRITE_GMC		2



#define READ_MARKER()	bs->Skip(1)
//#define WRITE_MARKER()	BitstreamPutBit(bs, 1)

/* vop coding types  */
/* intra, prediction, backward, sprite, not_coded */
#define I_VOP	0
#define P_VOP	1
#define B_VOP	2
#define S_VOP	3
#define N_VOP	4

/* resync-specific */
#define NUMBITS_VP_RESYNC_MARKER  17
#define RESYNC_MARKER 1

extern const dword scan_tables[3][64];


#endif /* _BITSTREAM_H_ */

/*****************************************************************************
 *
 *  XVID MPEG-4 VIDEO CODEC
 *  - Bitstream reader/writer -
 *
 *  Copyright (C) 2001-2003 Peter Ross <pross@xvid.org>
 *                     2003 Cristoph Lampert <gruel@web.de>
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
 * $Id: bitstream.cpp,v 1.1.1.1 2005-07-13 14:36:12 jeanlf Exp $
 *
 ****************************************************************************/

#include "bitstream.h"
#include "quant_matrix.h"

#ifdef WIN32
#include <stdio.h>
#endif

//----------------------------

inline dword ByteSwap(dword a) {
	return (a<<24) | ((a&0xff00) << 8) | ((a&0xff0000) >> 8) | (a>>24);
}

//----------------------------

void Bitstream::Init(const void *bitstream, dword _length) {

	dword adjbitstream = (dword)bitstream;

	/*
	 * Start the stream on a dword boundary, by rounding down to the
	 * previous dword and skipping the intervening bytes.
	 */
	long bitpos = ((sizeof(dword)-1) & (long)bitstream);
	adjbitstream = adjbitstream - bitpos;
	start = tail = (dword *) adjbitstream;

	if(_length) {
		dword tmp = *start;
#ifndef ARCH_IS_BIG_ENDIAN
		tmp = ByteSwap(tmp);
#endif
		bufa = tmp;
		if(_length>4) {
			tmp = *(start + 1);
#ifndef ARCH_IS_BIG_ENDIAN
			tmp = ByteSwap(tmp);
#endif
			bufb = tmp;
		} else
			bufb = 0;
	} else
		bufa = bufb = 0;

	buf = 0;
	pos = initpos = bitpos*8;
	length = _length;
}

//---------------------------

/* reads n bits from bitstream without changing the stream pos */

dword Bitstream::ShowBits(dword bits) {

	int nbit = (bits + pos) - 32;
	if(nbit > 0)
		return ((bufa & (0xffffffff >> pos)) << nbit) | (bufb >> (32 - nbit));
	return (bufa & (0xffffffff >> pos)) >> (32 - pos - bits);
}

//---------------------------
// skip n bits forward in bitstream
void Bitstream::Skip(dword bits) {

	pos += bits;
	if(pos >= 32) {
		dword tmp;

		bufa = bufb;
		tmp = *((dword *) tail + 2);
#ifndef ARCH_IS_BIG_ENDIAN
		tmp = ByteSwap(tmp);
#endif
		bufb = tmp;
		tail++;
		pos -= 32;
	}
}

//---------------------------
/* show nbits from next byte alignment */
dword Bitstream::ShowBitsFromByteAlign(int bits) {

	int bspos = pos + NumBitsToByteAlign();
	int nbit = (bits + bspos) - 32;

	if (bspos >= 32) {
		return bufb >> (32 - nbit);
	} else   if (nbit > 0) {
		return ((bufa & (0xffffffff >> bspos)) << nbit) | (bufb >> (32 - nbit));
	} else {
		return (bufa & (0xffffffff >> bspos)) >> (32 - bspos - bits);
	}

}

//---------------------------
/* read n bits from bitstream */
dword Bitstream::GetBits(dword n) {
	dword ret = ShowBits(n);
	Skip(n);
	return ret;
}

//---------------------------

static const dword intra_dc_threshold_table[] = {
	32,                     /* never use */
	13,
	15,
	17,
	19,
	21,
	23,
	1,
};

//----------------------------

void Bitstream::get_matrix(byte * matrix) {

	int i = 0;
	int last, value = 0;

	do {
		last = value;
		value = GetBits(8);
		matrix[scan_tables[0][i++]] = value;
	}
	while (value != 0 && i < 64);
	i--;    /* fix little bug at coeff not full */

	while (i < 64) {
		matrix[scan_tables[0][i++]] = last;
	}
}

//----------------------------
/*
 * for PVOP addbits == fcode - 1
 * for BVOP addbits == max(fcode,bcode) - 1
 * returns mbpos
 */
int S_decoder::read_video_packet_header(Bitstream *bs, const int addbits, int *quant, int *fcode_forward, int *fcode_backward, int *intra_dc_threshold) {

	int startcode_bits = NUMBITS_VP_RESYNC_MARKER + addbits;
	int mbnum_bits = log2bin(mb_width *  mb_height - 1);
	int mbnum;
	int hec = 0;

	bs->Skip(bs->NumBitsToByteAlign());
	bs->Skip(startcode_bits);

	DPRINTF(XVID_DEBUG_STARTCODE, "<video_packet_header>\n");

	if (shape != VIDOBJLAY_SHAPE_RECTANGULAR)
	{
		hec = bs->GetBit();     /* header_extension_code */
		if (hec && !(sprite_enable == SPRITE_STATIC /* && current_coding_type = I_VOP */))
		{
			bs->Skip(13);        /* vop_width */
			READ_MARKER();
			bs->Skip(13);        /* vop_height */
			READ_MARKER();
			bs->Skip(13);        /* vop_horizontal_mc_spatial_ref */
			READ_MARKER();
			bs->Skip(13);        /* vop_vertical_mc_spatial_ref */
			READ_MARKER();
		}
	}

	mbnum = bs->GetBits(mbnum_bits);    /* macroblock_number */
	DPRINTF(XVID_DEBUG_HEADER, "mbnum %i\n", mbnum);

	if (shape != VIDOBJLAY_SHAPE_BINARY_ONLY)
	{
		*quant = bs->GetBits(quant_bits);   /* quant_scale */
		DPRINTF(XVID_DEBUG_HEADER, "quant %i\n", *quant);
	}

	if (shape == VIDOBJLAY_SHAPE_RECTANGULAR)
		hec = bs->GetBit();     /* header_extension_code */


	DPRINTF(XVID_DEBUG_HEADER, "header_extension_code %i\n", hec);
	if (hec)
	{
		int time_base;
		int time_increment = 0;
		int coding_type;

		for (time_base=0; bs->GetBit()!=0; time_base++);      /* modulo_time_base */
		READ_MARKER();
		if (time_inc_bits)
			time_increment = (bs->GetBits(time_inc_bits));  /* vop_time_increment */
		READ_MARKER();
		//DPRINTF(XVID_DEBUG_HEADER,"time %i:%i\n", time_base, time_increment);

		coding_type = bs->GetBits(2);
		DPRINTF(XVID_DEBUG_HEADER,"coding_type %i\n", coding_type);

		if (shape != VIDOBJLAY_SHAPE_RECTANGULAR)
		{
			bs->Skip(1);   /* change_conv_ratio_disable */
			if (coding_type != I_VOP)
				bs->Skip(1);   /* vop_shape_coding_type */
		}

		if (shape != VIDOBJLAY_SHAPE_BINARY_ONLY)
		{
			*intra_dc_threshold = intra_dc_threshold_table[bs->GetBits(3)];

			if (sprite_enable == SPRITE_GMC && coding_type == S_VOP &&
			        sprite_warping_points > 0)
			{
				/* TODO: sprite trajectory */
			}
			if(reduced_resolution_enable && shape == VIDOBJLAY_SHAPE_RECTANGULAR && (coding_type == P_VOP || coding_type == I_VOP)) {
				bs->Skip(1); /* XXX: vop_reduced_resolution */
			}

			if (coding_type != I_VOP && fcode_forward)
			{
				*fcode_forward = bs->GetBits(3);
				DPRINTF(XVID_DEBUG_HEADER,"fcode_forward %i\n", *fcode_forward);
			}

			if (coding_type == B_VOP && fcode_backward)
			{
				*fcode_backward = bs->GetBits(3);
				DPRINTF(XVID_DEBUG_HEADER,"fcode_backward %i\n", *fcode_backward);
			}
		}

	}

	if (newpred_enable) {
		int vop_id;
		int vop_id_for_prediction;

		vop_id = bs->GetBits(MIN(time_inc_bits + 3, 15));
		DPRINTF(XVID_DEBUG_HEADER, "vop_id %i\n", vop_id);
		if (bs->GetBit()) /* vop_id_for_prediction_indication */
		{
			vop_id_for_prediction = bs->GetBits(MIN(time_inc_bits + 3, 15));
			DPRINTF(XVID_DEBUG_HEADER, "vop_id_for_prediction %i\n", vop_id_for_prediction);
		}
		READ_MARKER();
	}

	return mbnum;
}

//----------------------------
/* vol estimation header */
void S_decoder::read_vol_complexity_estimation_header(Bitstream * bs) {

	ESTIMATION * e = &estimation;

	e->method = bs->GetBits(2);   /* estimation_method */
	DPRINTF(XVID_DEBUG_HEADER,"+ complexity_estimation_header; method=%i\n", e->method);

	if (e->method == 0 || e->method == 1) {
		if(!bs->GetBit()) {
			//shape_complexity_estimation_disable
			e->opaque = bs->GetBit();     /* opaque */
			e->transparent = bs->GetBit();      /* transparent */
			e->intra_cae = bs->GetBit();     /* intra_cae */
			e->inter_cae = bs->GetBit();     /* inter_cae */
			e->no_update = bs->GetBit();     /* no_update */
			e->upsampling = bs->GetBit();    /* upsampling */
		}

		if (!bs->GetBit())   /* texture_complexity_estimation_set_1_disable */
		{
			e->intra_blocks = bs->GetBit();     /* intra_blocks */
			e->inter_blocks = bs->GetBit();     /* inter_blocks */
			e->inter4v_blocks = bs->GetBit();      /* inter4v_blocks */
			e->not_coded_blocks = bs->GetBit();    /* not_coded_blocks */
		}
	}

	READ_MARKER();

	if (!bs->GetBit()) {
		//texture_complexity_estimation_set_2_disable
		e->dct_coefs = bs->GetBit();     /* dct_coefs */
		e->dct_lines = bs->GetBit();     /* dct_lines */
		e->vlc_symbols = bs->GetBit();      /* vlc_symbols */
		e->vlc_bits = bs->GetBit();      /* vlc_bits */
	}

	if (!bs->GetBit()) {
		//motion_compensation_complexity_disable
		e->apm = bs->GetBit();     /* apm */
		e->npm = bs->GetBit();     /* npm */
		e->interpolate_mc_q = bs->GetBit();    /* interpolate_mc_q */
		e->forw_back_mc_q = bs->GetBit();      /* forw_back_mc_q */
		e->halfpel2 = bs->GetBit();      /* halfpel2 */
		e->halfpel4 = bs->GetBit();      /* halfpel4 */
	}

	READ_MARKER();

	if (e->method == 1) {
		if (!bs->GetBit()) {
			//version2_complexity_estimation_disable
			e->sadct = bs->GetBit();      /* sadct */
			e->quarterpel = bs->GetBit();    /* quarterpel */
		}
	}
}

//----------------------------
/* vop estimation header */
void S_decoder::read_vop_complexity_estimation_header(Bitstream * bs, int coding_type) {

	ESTIMATION * e = &estimation;

	if (e->method == 0 || e->method == 1)
	{
		if (coding_type == I_VOP) {
			if (e->opaque)    bs->Skip(8);   /* dcecs_opaque */
			if (e->transparent) bs->Skip(8); /* */
			if (e->intra_cae) bs->Skip(8);   /* */
			if (e->inter_cae) bs->Skip(8);   /* */
			if (e->no_update) bs->Skip(8);   /* */
			if (e->upsampling)   bs->Skip(8);   /* */
			if (e->intra_blocks) bs->Skip(8);   /* */
			if (e->not_coded_blocks) bs->Skip(8);  /* */
			if (e->dct_coefs) bs->Skip(8);   /* */
			if (e->dct_lines) bs->Skip(8);   /* */
			if (e->vlc_symbols) bs->Skip(8); /* */
			if (e->vlc_bits)  bs->Skip(8);   /* */
			if (e->sadct)     bs->Skip(8);   /* */
		}

		if (coding_type == P_VOP) {
			if (e->opaque) bs->Skip(8);      /* */
			if (e->transparent) bs->Skip(8); /* */
			if (e->intra_cae) bs->Skip(8);   /* */
			if (e->inter_cae) bs->Skip(8);   /* */
			if (e->no_update) bs->Skip(8);   /* */
			if (e->upsampling) bs->Skip(8);  /* */
			if (e->intra_blocks) bs->Skip(8);   /* */
			if (e->not_coded_blocks)   bs->Skip(8);   /* */
			if (e->dct_coefs) bs->Skip(8);   /* */
			if (e->dct_lines) bs->Skip(8);   /* */
			if (e->vlc_symbols) bs->Skip(8); /* */
			if (e->vlc_bits)  bs->Skip(8);   /* */
			if (e->inter_blocks) bs->Skip(8);   /* */
			if (e->inter4v_blocks) bs->Skip(8); /* */
			if (e->apm)       bs->Skip(8);   /* */
			if (e->npm)       bs->Skip(8);   /* */
			if (e->forw_back_mc_q) bs->Skip(8); /* */
			if (e->halfpel2)  bs->Skip(8);   /* */
			if (e->halfpel4)  bs->Skip(8);   /* */
			if (e->sadct)     bs->Skip(8);   /* */
			if (e->quarterpel)   bs->Skip(8);   /* */
		}
		if (coding_type == B_VOP) {
			if (e->opaque)    bs->Skip(8);   /* */
			if (e->transparent)  bs->Skip(8);   /* */
			if (e->intra_cae) bs->Skip(8);   /* */
			if (e->inter_cae) bs->Skip(8);   /* */
			if (e->no_update) bs->Skip(8);   /* */
			if (e->upsampling)   bs->Skip(8);   /* */
			if (e->intra_blocks) bs->Skip(8);   /* */
			if (e->not_coded_blocks) bs->Skip(8);  /* */
			if (e->dct_coefs) bs->Skip(8);   /* */
			if (e->dct_lines) bs->Skip(8);   /* */
			if (e->vlc_symbols)  bs->Skip(8);   /* */
			if (e->vlc_bits)  bs->Skip(8);   /* */
			if (e->inter_blocks) bs->Skip(8);   /* */
			if (e->inter4v_blocks) bs->Skip(8); /* */
			if (e->apm)       bs->Skip(8);   /* */
			if (e->npm)       bs->Skip(8);   /* */
			if (e->forw_back_mc_q) bs->Skip(8); /* */
			if (e->halfpel2)  bs->Skip(8);   /* */
			if (e->halfpel4)  bs->Skip(8);   /* */
			if (e->interpolate_mc_q) bs->Skip(8);  /* */
			if (e->sadct)     bs->Skip(8);   /* */
			if (e->quarterpel)   bs->Skip(8);   /* */
		}

		if (coding_type == S_VOP && sprite_enable == SPRITE_STATIC) {
			if (e->intra_blocks) bs->Skip(8);   /* */
			if (e->not_coded_blocks) bs->Skip(8);  /* */
			if (e->dct_coefs) bs->Skip(8);   /* */
			if (e->dct_lines) bs->Skip(8);   /* */
			if (e->vlc_symbols)  bs->Skip(8);   /* */
			if (e->vlc_bits)  bs->Skip(8);   /* */
			if (e->inter_blocks) bs->Skip(8);   /* */
			if (e->inter4v_blocks)  bs->Skip(8);   /* */
			if (e->apm)       bs->Skip(8);   /* */
			if (e->npm)       bs->Skip(8);   /* */
			if (e->forw_back_mc_q)  bs->Skip(8);   /* */
			if (e->halfpel2)  bs->Skip(8);   /* */
			if (e->halfpel4)  bs->Skip(8);   /* */
			if (e->interpolate_mc_q) bs->Skip(8);  /* */
		}
	}
}

//----------------------------
// decode headers
// returns coding_type, or -1 if error

#define VIDOBJ_START_CODE_MASK      0x0000001f
#define VIDOBJLAY_START_CODE_MASK   0x0000000f

int S_decoder::BitstreamReadHeaders(Bitstream * bs, bool &rounding, bool *reduced_resolution, dword *quant, dword *fcode_forward,
                                    dword *fcode_backward, dword *intra_dc_threshold, WARPPOINTS *gmc_warp) {

	dword vol_ver_id;
	dword coding_type;
	dword start_code;
	dword time_incr = 0;
	int time_increment = 0;
	int resize = 0;

	while((bs->Pos() >> 3) < bs->length) {
		bs->ByteAlign();
		start_code = bs->ShowBits(32);

		switch(start_code) {
		case VISOBJSEQ_START_CODE:
		{
			DPRINTF(XVID_DEBUG_STARTCODE, "<visual_object_sequence>\n");

			bs->Skip(32);     //visual_object_sequence_start_code
			int profile = bs->GetBits(8);  //profile_and_level_indication

			DPRINTF(XVID_DEBUG_HEADER, "profile_and_level_indication %i\n", profile);
		}
		break;
		case VISOBJSEQ_STOP_CODE:
		{
			//visual_object_sequence_stop_code
			bs->Skip(32);
			DPRINTF(XVID_DEBUG_STARTCODE, "</visual_object_sequence>\n");

		}
		break;
		case VISOBJ_START_CODE:
		{
			DPRINTF(XVID_DEBUG_STARTCODE, "<visual_object>\n");

			bs->Skip(32);  //visual_object_start_code

			int visobj_ver_id;
			//is_visual_object_identified
			if(bs->GetBit()) {
				visobj_ver_id = bs->GetBits(4);  /* visual_object_ver_id */
				DPRINTF(XVID_DEBUG_HEADER,"visobj_ver_id %i\n", visobj_ver_id);
				bs->Skip(3);   //visual_object_priority
			} else {
				visobj_ver_id = 1;
			}

			if(bs->ShowBits(4) != VISOBJ_TYPE_VIDEO) {
				//visual_object_type
				DPRINTF(XVID_DEBUG_ERROR, "visual_object_type != video\n");
				return -1;
			}
			bs->Skip(4);
			//video_signal_type

			if(bs->GetBit()) {
				DPRINTF(XVID_DEBUG_HEADER,"+ video_signal_type\n");
				bs->Skip(3);   /* video_format */
				bs->Skip(1);   /* video_range */
				if (bs->GetBit()) /* color_description */
				{
					DPRINTF(XVID_DEBUG_HEADER,"+ color_description");
					bs->Skip(8);   /* color_primaries */
					bs->Skip(8);   /* transfer_characteristics */
					bs->Skip(8);   /* matrix_coefficients */
				}
			}
		}
		break;
		case GRPOFVOP_START_CODE:
		{
			DPRINTF(XVID_DEBUG_STARTCODE, "<group_of_vop>\n");

			bs->Skip(32);
			{
				int hours, minutes, seconds;

				hours = bs->GetBits(5);
				minutes = bs->GetBits(6);
				READ_MARKER();
				seconds = bs->GetBits(6);
				//DPRINTF(XVID_DEBUG_HEADER, "time %ih%im%is\n", hours,minutes,seconds);
			}
			bs->Skip(1);   /* closed_gov */
			bs->Skip(1);   /* broken_link */
		}
		break;
		case VOP_START_CODE:
		{

			DPRINTF(XVID_DEBUG_STARTCODE, "<vop>\n");

			bs->Skip(32);  /* vop_start_code */

			coding_type = bs->GetBits(2); /* vop_coding_type */
			DPRINTF(XVID_DEBUG_HEADER, "coding_type %i\n", coding_type);

			/*********************** for decode B-frame time ***********************/
			while (bs->GetBit() != 0)  /* time_base */
				time_incr++;

			READ_MARKER();

			if (time_inc_bits) {
				time_increment = (bs->GetBits(time_inc_bits));  /* vop_time_increment */
			}

			DPRINTF(XVID_DEBUG_HEADER, "time_base %i\n", time_incr);
			DPRINTF(XVID_DEBUG_HEADER, "time_increment %i\n", time_increment);

			//DPRINTF(XVID_DEBUG_TIMECODE, "%c %i:%i\n", coding_type == I_VOP ? 'I' : coding_type == P_VOP ? 'P' : coding_type == B_VOP ? 'B' : 'S', time_incr, time_increment);
			if (coding_type != B_VOP) {
				last_time_base = time_base;
				time_base += time_incr;
				time = time_increment;

#if 0
				time_base * time_inc_resolution +
				time_increment;
#endif
				time_pp = (dword)
				          (time_inc_resolution + time - last_non_b_time)%time_inc_resolution;
				last_non_b_time = time;
			} else {
				time = time_increment;
#if 0
				(last_time_base +
				 time_incr) * time_inc_resolution + time_increment;
#endif
				time_bp = (dword)
				          (time_inc_resolution + last_non_b_time - time)%time_inc_resolution;
			}
			DPRINTF(XVID_DEBUG_HEADER,"time_pp=%i\n", time_pp);
			DPRINTF(XVID_DEBUG_HEADER,"time_bp=%i\n", time_bp);

			READ_MARKER();

			if (!bs->GetBit())   /* vop_coded */
			{
				DPRINTF(XVID_DEBUG_HEADER, "vop_coded==false\n");
				return N_VOP;
			}

			if (newpred_enable) {
				int vop_id;
				int vop_id_for_prediction;

				vop_id = bs->GetBits(MIN(time_inc_bits + 3, 15));
				DPRINTF(XVID_DEBUG_HEADER, "vop_id %i\n", vop_id);
				if (bs->GetBit()) /* vop_id_for_prediction_indication */
				{
					vop_id_for_prediction = bs->GetBits(MIN(time_inc_bits + 3, 15));
					DPRINTF(XVID_DEBUG_HEADER, "vop_id_for_prediction %i\n", vop_id_for_prediction);
				}
				READ_MARKER();
			}

			//fix a little bug by MinChen <chenm002@163.com>
			if((shape != VIDOBJLAY_SHAPE_BINARY_ONLY) &&
			        ((coding_type == P_VOP) || (coding_type == S_VOP && sprite_enable == SPRITE_GMC))) {
				rounding = !!bs->GetBit();  //rounding_type
				DPRINTF(XVID_DEBUG_HEADER, "rounding %i\n", rounding);
			}

			if(reduced_resolution_enable && shape == VIDOBJLAY_SHAPE_RECTANGULAR && (coding_type == P_VOP || coding_type == I_VOP)) {

				*reduced_resolution = !!bs->GetBit();
				DPRINTF(XVID_DEBUG_HEADER, "reduced_resolution %i\n", *reduced_resolution);
			} else {
				*reduced_resolution = false;
			}

			if (shape != VIDOBJLAY_SHAPE_RECTANGULAR) {
				if(!(sprite_enable == SPRITE_STATIC && coding_type == I_VOP)) {

					dword width, height;
					dword horiz_mc_ref, vert_mc_ref;

					width = bs->GetBits(13);
					READ_MARKER();
					height = bs->GetBits(13);
					READ_MARKER();
					horiz_mc_ref = bs->GetBits(13);
					READ_MARKER();
					vert_mc_ref = bs->GetBits(13);
					READ_MARKER();

					DPRINTF(XVID_DEBUG_HEADER, "width %i\n", width);
					DPRINTF(XVID_DEBUG_HEADER, "height %i\n", height);
					DPRINTF(XVID_DEBUG_HEADER, "horiz_mc_ref %i\n", horiz_mc_ref);
					DPRINTF(XVID_DEBUG_HEADER, "vert_mc_ref %i\n", vert_mc_ref);
				}

				bs->Skip(1);   /* change_conv_ratio_disable */
				if (bs->GetBit()) /* vop_constant_alpha */
				{
					bs->Skip(8);   /* vop_constant_alpha_value */
				}
			}

			if (shape != VIDOBJLAY_SHAPE_BINARY_ONLY) {

				if (!complexity_estimation_disable)
				{
					read_vop_complexity_estimation_header(bs, coding_type);
				}

				/* intra_dc_vlc_threshold */
				*intra_dc_threshold =
				    intra_dc_threshold_table[bs->GetBits(3)];

				top_field_first = 0;
				alternate_vertical_scan = 0;

				if(interlacing) {
					top_field_first = bs->GetBit();
					DPRINTF(XVID_DEBUG_HEADER, "interlace top_field_first %i\n", top_field_first);
					alternate_vertical_scan = bs->GetBit();
					DPRINTF(XVID_DEBUG_HEADER, "interlace alternate_vertical_scan %i\n", alternate_vertical_scan);

				}
			}

			if ((sprite_enable == SPRITE_STATIC || sprite_enable== SPRITE_GMC) && coding_type == S_VOP) {

				int i;

				for (i = 0 ; i < sprite_warping_points; i++)
				{
					int length;
					int x = 0, y = 0;

					/* sprite code borowed from ffmpeg; thx Michael Niedermayer <michaelni@gmx.at> */
					length = bs->bs_get_spritetrajectory();
					if(length) {
						x= bs->GetBits(length);
						if ((x >> (length - 1)) == 0) /* if MSB not set it is negative*/
							x = - (x ^ ((1 << length) - 1));
					}
					READ_MARKER();

					length = bs->bs_get_spritetrajectory();
					if(length) {
						y = bs->GetBits(length);
						if ((y >> (length - 1)) == 0) /* if MSB not set it is negative*/
							y = - (y ^ ((1 << length) - 1));
					}
					READ_MARKER();

					gmc_warp->duv[i].x = x;
					gmc_warp->duv[i].y = y;

					//DPRINTF(XVID_DEBUG_HEADER,"sprite_warping_point[%i] xy=(%i,%i)\n", i, x, y);
				}

				if (sprite_brightness_change)
				{
					/* XXX: brightness_change_factor() */
				}
				if (sprite_enable == SPRITE_STATIC)
				{
					/* XXX: todo */
				}

			}

			if ((*quant = bs->GetBits(quant_bits)) < 1)  /* vop_quant */
				*quant = 1;
			DPRINTF(XVID_DEBUG_HEADER, "quant %i\n", *quant);

			if (coding_type != I_VOP) {
				*fcode_forward = bs->GetBits(3); /* fcode_forward */
				DPRINTF(XVID_DEBUG_HEADER, "fcode_forward %i\n", *fcode_forward);
			}

			if (coding_type == B_VOP) {
				*fcode_backward = bs->GetBits(3);   /* fcode_backward */
				DPRINTF(XVID_DEBUG_HEADER, "fcode_backward %i\n", *fcode_backward);
			}
			if(!scalability) {
				if ((shape != VIDOBJLAY_SHAPE_RECTANGULAR) &&
				        (coding_type != I_VOP)) {
					bs->Skip(1);   /* vop_shape_coding_type */
				}
			}
			return coding_type;

		}
		break;
		case USERDATA_START_CODE:
		{
			char tmp[256];
			int i;

			bs->Skip(32);  /* user_data_start_code */

			tmp[0] = bs->ShowBits(8);

			for(i = 1; i < 256; i++) {
				tmp[i] = (bs->ShowBits(16) & 0xFF);

				if(tmp[i] == 0)
					break;

				bs->Skip(8);
			}

			//DPRINTF(XVID_DEBUG_STARTCODE, "<user_data>: %s\n", tmp);

			/* read xvid bitstream version */
#ifdef WIN32
			{
				char packed;
				int version, build;
				if(MemCmp(tmp, "XviD", 4) == 0) {
					sscanf(tmp, "XviD%d", &bs_version);
					DPRINTF(XVID_DEBUG_HEADER, "xvid bitstream version=%i", bs_version);
				}

				/* divx detection */
				i = sscanf(tmp, "DivX%dBuild%d%c", &version, &build, &packed);
				if (i < 2)
					i = sscanf(tmp, "DivX%db%d%c", &version, &build, &packed);
				if (i >= 2) {
					packed_mode = (i == 3 && packed == 'p');
					//DPRINTF(XVID_DEBUG_HEADER, "divx version=%i, build=%i packed=%i\n", version, build, packed_mode);
				}
			}
#endif
		}
		break;
		default:
			switch(start_code & ~VIDOBJ_START_CODE_MASK) {
			case VIDOBJ_START_CODE:
			{

				DPRINTF(XVID_DEBUG_STARTCODE, "<video_object>\n");
				DPRINTF(XVID_DEBUG_HEADER, "vo id %i\n", start_code & VIDOBJ_START_CODE_MASK);
				//video_object_start_code
				bs->Skip(32);
			}
			break;
			case VIDOBJLAY_START_CODE:
			{

				DPRINTF(XVID_DEBUG_STARTCODE, "<video_object_layer>\n");
				DPRINTF(XVID_DEBUG_HEADER, "vol id %i\n", start_code & VIDOBJLAY_START_CODE_MASK);

				bs->Skip(32);  //video_object_layer_start_code
				bs->Skip(1);   //random_accessible_vol

				bs->Skip(8);   //video_object_type_indication

				if(bs->GetBit()) {
					//is_object_layer_identifier
					DPRINTF(XVID_DEBUG_HEADER, "+ is_object_layer_identifier\n");
					vol_ver_id = bs->GetBits(4);  /* video_object_layer_verid */
					DPRINTF(XVID_DEBUG_HEADER,"ver_id %i\n", vol_ver_id);
					bs->Skip(3);   /* video_object_layer_priority */
				} else {
					vol_ver_id = 1;
				}

				aspect_ratio = bs->GetBits(4);

				if(aspect_ratio == VIDOBJLAY_AR_EXTPAR) {
					//aspect_ratio_info
					DPRINTF(XVID_DEBUG_HEADER, "+ aspect_ratio_info\n");
					par_width = bs->GetBits(8);   /* par_width */
					par_height = bs->GetBits(8);  /* par_height */
				}

				if(bs->GetBit()) {
					//vol_control_parameters
					DPRINTF(XVID_DEBUG_HEADER, "+ vol_control_parameters\n");
					bs->Skip(2);   /* chroma_format */
					low_delay = (bs->GetBit()!=0);
					DPRINTF(XVID_DEBUG_HEADER, "low_delay %i\n", (int)low_delay);
					if (bs->GetBit()) /* vbv_parameters */
					{
						unsigned int bitrate;
						unsigned int buffer_size;
						unsigned int occupancy;

						DPRINTF(XVID_DEBUG_HEADER,"+ vbv_parameters\n");

						bitrate = bs->GetBits(15) << 15; /* first_half_bit_rate */
						READ_MARKER();
						bitrate |= bs->GetBits(15);      /* latter_half_bit_rate */
						READ_MARKER();

						buffer_size = bs->GetBits(15) << 3; /* first_half_vbv_buffer_size */
						READ_MARKER();
						buffer_size |= bs->GetBits(3);      /* latter_half_vbv_buffer_size */

						occupancy = bs->GetBits(11) << 15;  /* first_half_vbv_occupancy */
						READ_MARKER();
						occupancy |= bs->GetBits(15); /* latter_half_vbv_occupancy */
						READ_MARKER();

						DPRINTF(XVID_DEBUG_HEADER,"bitrate %d (unit=400 bps)\n", bitrate);
						DPRINTF(XVID_DEBUG_HEADER,"buffer_size %d (unit=16384 bits)\n", buffer_size);
						DPRINTF(XVID_DEBUG_HEADER,"occupancy %d (unit=64 bits)\n", occupancy);
					}
				} else {
					low_delay = low_delay_default;
				}

				//video_object_layer_shape
				shape = bs->GetBits(2);

				DPRINTF(XVID_DEBUG_HEADER, "shape %i\n", shape);
				if(shape != VIDOBJLAY_SHAPE_RECTANGULAR) {
					DPRINTF(XVID_DEBUG_ERROR,"non-rectangular shapes are not supported\n");
				}

				if(shape == VIDOBJLAY_SHAPE_GRAYSCALE && vol_ver_id != 1) {
					//video_object_layer_shape_extension
					bs->Skip(4);
				}

				READ_MARKER();

				/********************** for decode B-frame time ***********************/
				time_inc_resolution = bs->GetBits(16); /* vop_time_increment_resolution */
				DPRINTF(XVID_DEBUG_HEADER,"vop_time_increment_resolution %i\n", time_inc_resolution);

#if 0
				time_inc_resolution--;
#endif

				if(time_inc_resolution > 0) {
					time_inc_bits = log2bin(time_inc_resolution-1);
				} else {
#if 0
					time_inc_bits = 0;
#endif
					/* for "old" xvid compatibility, set time_inc_bits = 1 */
					time_inc_bits = 1;
				}

				READ_MARKER();

				if(bs->GetBit()) {
					//fixed_vop_rate
					DPRINTF(XVID_DEBUG_HEADER, "+ fixed_vop_rate\n");
					bs->Skip(time_inc_bits);   //fixed_vop_time_increment
				}

				if(shape != VIDOBJLAY_SHAPE_BINARY_ONLY) {

					if(shape == VIDOBJLAY_SHAPE_RECTANGULAR) {
						dword _width, _height;

						READ_MARKER();
						_width = bs->GetBits(13);  //video_object_layer_width
						READ_MARKER();
						_height = bs->GetBits(13); //video_object_layer_height
						READ_MARKER();

						DPRINTF(XVID_DEBUG_HEADER, "width %i\n", _width);
						DPRINTF(XVID_DEBUG_HEADER, "height %i\n", _height);

						if(width != _width || height != _height) {
							if(fixed_dimensions) {
								DPRINTF(XVID_DEBUG_ERROR, "decoder width/height does not match bitstream\n");
								return -1;
							}
							resize = 1;
							width = _width;
							height = _height;
						}
					}

					interlacing = !!bs->GetBit();
					DPRINTF(XVID_DEBUG_HEADER, "interlacing %i\n", interlacing);

					if(!bs->GetBit()) {
						//obmc_disable
						DPRINTF(XVID_DEBUG_ERROR, "obmc_disabled==false not supported\n");
						/* TODO */
						/* fucking divx4.02 has this enabled */
					}

					sprite_enable = bs->GetBits((vol_ver_id == 1 ? 1 : 2));

					if(sprite_enable == SPRITE_STATIC || sprite_enable == SPRITE_GMC) {
						int low_latency_sprite_enable;

						if(sprite_enable != SPRITE_GMC) {
							int sprite_width;
							int sprite_height;
							int sprite_left_coord;
							int sprite_top_coord;
							sprite_width = bs->GetBits(13);     /* sprite_width */
							READ_MARKER();
							sprite_height = bs->GetBits(13); /* sprite_height */
							READ_MARKER();
							sprite_left_coord = bs->GetBits(13);   /* sprite_left_coordinate */
							READ_MARKER();
							sprite_top_coord = bs->GetBits(13); /* sprite_top_coordinate */
							READ_MARKER();
						}
						sprite_warping_points = bs->GetBits(6);      /* no_of_sprite_warping_points */
						sprite_warping_accuracy = bs->GetBits(2);    /* sprite_warping_accuracy */
						sprite_brightness_change = bs->GetBits(1);      /* brightness_change */
						if (sprite_enable != SPRITE_GMC)
						{
							low_latency_sprite_enable = bs->GetBits(1);     /* low_latency_sprite_enable */
						}
					}

					if(vol_ver_id != 1 && shape != VIDOBJLAY_SHAPE_RECTANGULAR) {
						//sadct_disable
						bs->Skip(1);
					}

					if(bs->GetBit()) {
						//not_8_bit
						DPRINTF(XVID_DEBUG_HEADER, "not_8_bit==true (ignored)\n");
						quant_bits = bs->GetBits(4);  /* quant_precision */
						bs->Skip(4);   /* bits_per_pixel */
					} else {
						quant_bits = 5;
					}

					if(shape == VIDOBJLAY_SHAPE_GRAYSCALE) {
						bs->Skip(1);   /* no_gray_quant_update */
						bs->Skip(1);   /* composition_method */
						bs->Skip(1);   /* linear_composition */
					}

					quant_type = bs->GetBit();
					DPRINTF(XVID_DEBUG_HEADER, "quant_type %i\n", quant_type);

					if(quant_type) {
						if (bs->GetBit()) //load_intra_quant_mat
						{
							byte matrix[64];

							DPRINTF(XVID_DEBUG_HEADER, "load_intra_quant_mat\n");

							bs->get_matrix(matrix);
							set_intra_matrix(mpeg_quant_matrices, matrix);
						} else
							set_intra_matrix(mpeg_quant_matrices, get_default_intra_matrix());

						if (bs->GetBit()) /* load_inter_quant_mat */
						{
							byte matrix[64];

							DPRINTF(XVID_DEBUG_HEADER, "load_inter_quant_mat\n");

							bs->get_matrix(matrix);
							set_inter_matrix(mpeg_quant_matrices, matrix);
						} else
							set_inter_matrix(mpeg_quant_matrices, get_default_inter_matrix());

						if (shape == VIDOBJLAY_SHAPE_GRAYSCALE) {
							DPRINTF(XVID_DEBUG_ERROR, "greyscale matrix not supported\n");
							return -1;
						}

					}

					if(vol_ver_id != 1) {
						quarterpel = bs->GetBit();
						DPRINTF(XVID_DEBUG_HEADER,"quarterpel %i\n", quarterpel);
					} else
						quarterpel = 0;


					complexity_estimation_disable = bs->GetBit();
					if(!complexity_estimation_disable) {
						read_vol_complexity_estimation_header(bs);
					}

					bs->Skip(1);   //resync_marker_disable

					if(bs->GetBit()) {
						//data_partitioned
						DPRINTF(XVID_DEBUG_ERROR, "data_partitioned not supported\n");
						bs->Skip(1);   /* reversible_vlc */
					}

					if(vol_ver_id != 1) {
						newpred_enable = !!bs->GetBit();
						if (newpred_enable)  /* newpred_enable */
						{
							DPRINTF(XVID_DEBUG_HEADER, "+ newpred_enable\n");
							bs->Skip(2);   /* requested_upstream_message_type */
							bs->Skip(1);   /* newpred_segment_type */
						}
						reduced_resolution_enable = !!bs->GetBit(); //reduced_resolution_vop_enable
						DPRINTF(XVID_DEBUG_HEADER, "reduced_resolution_enable %i\n", reduced_resolution_enable);
					} else {
						newpred_enable = false;
						reduced_resolution_enable = false;
					}

					scalability = (bs->GetBit()!=0);
					if(scalability) {
						DPRINTF(XVID_DEBUG_ERROR, "scalability not supported\n");
						bs->Skip(1);   /* hierarchy_type */
						bs->Skip(4);   /* ref_layer_id */
						bs->Skip(1);   /* ref_layer_sampling_direc */
						bs->Skip(5);   /* hor_sampling_factor_n */
						bs->Skip(5);   /* hor_sampling_factor_m */
						bs->Skip(5);   /* vert_sampling_factor_n */
						bs->Skip(5);   /* vert_sampling_factor_m */
						bs->Skip(1);   /* enhancement_type */
						if(shape == VIDOBJLAY_SHAPE_BINARY /* && hierarchy_type==0 */) {
							bs->Skip(1);   /* use_ref_shape */
							bs->Skip(1);   /* use_ref_texture */
							bs->Skip(5);   /* shape_hor_sampling_factor_n */
							bs->Skip(5);   /* shape_hor_sampling_factor_m */
							bs->Skip(5);   /* shape_vert_sampling_factor_n */
							bs->Skip(5);   /* shape_vert_sampling_factor_m */
						}
						return -1;
					}
				} else {
					//shape == BINARY_ONLY
					if(vol_ver_id != 1) {
						scalability = (bs->GetBit()!=0);
						if(scalability) {
							DPRINTF(XVID_DEBUG_ERROR, "scalability not supported\n");
							bs->Skip(4);   /* ref_layer_id */
							bs->Skip(5);   /* hor_sampling_factor_n */
							bs->Skip(5);   /* hor_sampling_factor_m */
							bs->Skip(5);   /* vert_sampling_factor_n */
							bs->Skip(5);   /* vert_sampling_factor_m */
							return -1;
						}
					}
					bs->Skip(1);   /* resync_marker_disable */

				}

				return (resize ? -3 : -2);   //VOL

			}
			break;
			default:
			{
				//start_code == ?
				//if(bs->ShowBits(24) == 0x000001)
				if((start_code&0x00ffffff) == 0x000001) {
					DPRINTF(XVID_DEBUG_STARTCODE, "<unknown: %x>\n", bs->ShowBits(32));
				}
				bs->Skip(8);
			}
			}
		}
	}

#if 0
	DPRINTF("*** WARNING: no vop_start_code found");
#endif
	return -1;                 //ignore it
}

//----------------------------


/*****************************************************************************
 *
 *  XVID MPEG-4 VIDEO CODEC
 *  - MB coding -
 *
 *  Copyright (C) 2002 Michael Militzer <isibaar@xvid.org>
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
 * $Id: mbcoding.cpp,v 1.1.1.1 2005-07-13 14:36:15 jeanlf Exp $
 *
 ****************************************************************************/

#include "portab.h"
#include "global.h"
#include "bitstream.h"
#include "vlc_codes.h"

//----------------------------
/*****************************************************************************
 * VLC tables and other constant arrays
 ****************************************************************************/

struct VLC_TABLE {
	VLC vlc;
	EVENT event;
};

static const VLC_TABLE coeff_tab[2][102] = {
	/* intra = 0 */
	{
		{{ 2,  2}, {0, 0, 1}},
		{{15,  4}, {0, 0, 2}},
		{{21,  6}, {0, 0, 3}},
		{{23,  7}, {0, 0, 4}},
		{{31,  8}, {0, 0, 5}},
		{{37,  9}, {0, 0, 6}},
		{{36,  9}, {0, 0, 7}},
		{{33, 10}, {0, 0, 8}},
		{{32, 10}, {0, 0, 9}},
		{{ 7, 11}, {0, 0, 10}},
		{{ 6, 11}, {0, 0, 11}},
		{{32, 11}, {0, 0, 12}},
		{{ 6,  3}, {0, 1, 1}},
		{{20,  6}, {0, 1, 2}},
		{{30,  8}, {0, 1, 3}},
		{{15, 10}, {0, 1, 4}},
		{{33, 11}, {0, 1, 5}},
		{{80, 12}, {0, 1, 6}},
		{{14,  4}, {0, 2, 1}},
		{{29,  8}, {0, 2, 2}},
		{{14, 10}, {0, 2, 3}},
		{{81, 12}, {0, 2, 4}},
		{{13,  5}, {0, 3, 1}},
		{{35,  9}, {0, 3, 2}},
		{{13, 10}, {0, 3, 3}},
		{{12,  5}, {0, 4, 1}},
		{{34,  9}, {0, 4, 2}},
		{{82, 12}, {0, 4, 3}},
		{{11,  5}, {0, 5, 1}},
		{{12, 10}, {0, 5, 2}},
		{{83, 12}, {0, 5, 3}},
		{{19,  6}, {0, 6, 1}},
		{{11, 10}, {0, 6, 2}},
		{{84, 12}, {0, 6, 3}},
		{{18,  6}, {0, 7, 1}},
		{{10, 10}, {0, 7, 2}},
		{{17,  6}, {0, 8, 1}},
		{{ 9, 10}, {0, 8, 2}},
		{{16,  6}, {0, 9, 1}},
		{{ 8, 10}, {0, 9, 2}},
		{{22,  7}, {0, 10, 1}},
		{{85, 12}, {0, 10, 2}},
		{{21,  7}, {0, 11, 1}},
		{{20,  7}, {0, 12, 1}},
		{{28,  8}, {0, 13, 1}},
		{{27,  8}, {0, 14, 1}},
		{{33,  9}, {0, 15, 1}},
		{{32,  9}, {0, 16, 1}},
		{{31,  9}, {0, 17, 1}},
		{{30,  9}, {0, 18, 1}},
		{{29,  9}, {0, 19, 1}},
		{{28,  9}, {0, 20, 1}},
		{{27,  9}, {0, 21, 1}},
		{{26,  9}, {0, 22, 1}},
		{{34, 11}, {0, 23, 1}},
		{{35, 11}, {0, 24, 1}},
		{{86, 12}, {0, 25, 1}},
		{{87, 12}, {0, 26, 1}},
		{{ 7,  4}, {1, 0, 1}},
		{{25,  9}, {1, 0, 2}},
		{{ 5, 11}, {1, 0, 3}},
		{{15,  6}, {1, 1, 1}},
		{{ 4, 11}, {1, 1, 2}},
		{{14,  6}, {1, 2, 1}},
		{{13,  6}, {1, 3, 1}},
		{{12,  6}, {1, 4, 1}},
		{{19,  7}, {1, 5, 1}},
		{{18,  7}, {1, 6, 1}},
		{{17,  7}, {1, 7, 1}},
		{{16,  7}, {1, 8, 1}},
		{{26,  8}, {1, 9, 1}},
		{{25,  8}, {1, 10, 1}},
		{{24,  8}, {1, 11, 1}},
		{{23,  8}, {1, 12, 1}},
		{{22,  8}, {1, 13, 1}},
		{{21,  8}, {1, 14, 1}},
		{{20,  8}, {1, 15, 1}},
		{{19,  8}, {1, 16, 1}},
		{{24,  9}, {1, 17, 1}},
		{{23,  9}, {1, 18, 1}},
		{{22,  9}, {1, 19, 1}},
		{{21,  9}, {1, 20, 1}},
		{{20,  9}, {1, 21, 1}},
		{{19,  9}, {1, 22, 1}},
		{{18,  9}, {1, 23, 1}},
		{{17,  9}, {1, 24, 1}},
		{{ 7, 10}, {1, 25, 1}},
		{{ 6, 10}, {1, 26, 1}},
		{{ 5, 10}, {1, 27, 1}},
		{{ 4, 10}, {1, 28, 1}},
		{{36, 11}, {1, 29, 1}},
		{{37, 11}, {1, 30, 1}},
		{{38, 11}, {1, 31, 1}},
		{{39, 11}, {1, 32, 1}},
		{{88, 12}, {1, 33, 1}},
		{{89, 12}, {1, 34, 1}},
		{{90, 12}, {1, 35, 1}},
		{{91, 12}, {1, 36, 1}},
		{{92, 12}, {1, 37, 1}},
		{{93, 12}, {1, 38, 1}},
		{{94, 12}, {1, 39, 1}},
		{{95, 12}, {1, 40, 1}}
	},
	/* intra = 1 */
	{
		{{ 2,  2}, {0, 0, 1}},
		{{15,  4}, {0, 0, 3}},
		{{21,  6}, {0, 0, 6}},
		{{23,  7}, {0, 0, 9}},
		{{31,  8}, {0, 0, 10}},
		{{37,  9}, {0, 0, 13}},
		{{36,  9}, {0, 0, 14}},
		{{33, 10}, {0, 0, 17}},
		{{32, 10}, {0, 0, 18}},
		{{ 7, 11}, {0, 0, 21}},
		{{ 6, 11}, {0, 0, 22}},
		{{32, 11}, {0, 0, 23}},
		{{ 6,  3}, {0, 0, 2}},
		{{20,  6}, {0, 1, 2}},
		{{30,  8}, {0, 0, 11}},
		{{15, 10}, {0, 0, 19}},
		{{33, 11}, {0, 0, 24}},
		{{80, 12}, {0, 0, 25}},
		{{14,  4}, {0, 1, 1}},
		{{29,  8}, {0, 0, 12}},
		{{14, 10}, {0, 0, 20}},
		{{81, 12}, {0, 0, 26}},
		{{13,  5}, {0, 0, 4}},
		{{35,  9}, {0, 0, 15}},
		{{13, 10}, {0, 1, 7}},
		{{12,  5}, {0, 0, 5}},
		{{34,  9}, {0, 4, 2}},
		{{82, 12}, {0, 0, 27}},
		{{11,  5}, {0, 2, 1}},
		{{12, 10}, {0, 2, 4}},
		{{83, 12}, {0, 1, 9}},
		{{19,  6}, {0, 0, 7}},
		{{11, 10}, {0, 3, 4}},
		{{84, 12}, {0, 6, 3}},
		{{18,  6}, {0, 0, 8}},
		{{10, 10}, {0, 4, 3}},
		{{17,  6}, {0, 3, 1}},
		{{ 9, 10}, {0, 8, 2}},
		{{16,  6}, {0, 4, 1}},
		{{ 8, 10}, {0, 5, 3}},
		{{22,  7}, {0, 1, 3}},
		{{85, 12}, {0, 1, 10}},
		{{21,  7}, {0, 2, 2}},
		{{20,  7}, {0, 7, 1}},
		{{28,  8}, {0, 1, 4}},
		{{27,  8}, {0, 3, 2}},
		{{33,  9}, {0, 0, 16}},
		{{32,  9}, {0, 1, 5}},
		{{31,  9}, {0, 1, 6}},
		{{30,  9}, {0, 2, 3}},
		{{29,  9}, {0, 3, 3}},
		{{28,  9}, {0, 5, 2}},
		{{27,  9}, {0, 6, 2}},
		{{26,  9}, {0, 7, 2}},
		{{34, 11}, {0, 1, 8}},
		{{35, 11}, {0, 9, 2}},
		{{86, 12}, {0, 2, 5}},
		{{87, 12}, {0, 7, 3}},
		{{ 7,  4}, {1, 0, 1}},
		{{25,  9}, {0, 11, 1}},
		{{ 5, 11}, {1, 0, 6}},
		{{15,  6}, {1, 1, 1}},
		{{ 4, 11}, {1, 0, 7}},
		{{14,  6}, {1, 2, 1}},
		{{13,  6}, {0, 5, 1}},
		{{12,  6}, {1, 0, 2}},
		{{19,  7}, {1, 5, 1}},
		{{18,  7}, {0, 6, 1}},
		{{17,  7}, {1, 3, 1}},
		{{16,  7}, {1, 4, 1}},
		{{26,  8}, {1, 9, 1}},
		{{25,  8}, {0, 8, 1}},
		{{24,  8}, {0, 9, 1}},
		{{23,  8}, {0, 10, 1}},
		{{22,  8}, {1, 0, 3}},
		{{21,  8}, {1, 6, 1}},
		{{20,  8}, {1, 7, 1}},
		{{19,  8}, {1, 8, 1}},
		{{24,  9}, {0, 12, 1}},
		{{23,  9}, {1, 0, 4}},
		{{22,  9}, {1, 1, 2}},
		{{21,  9}, {1, 10, 1}},
		{{20,  9}, {1, 11, 1}},
		{{19,  9}, {1, 12, 1}},
		{{18,  9}, {1, 13, 1}},
		{{17,  9}, {1, 14, 1}},
		{{ 7, 10}, {0, 13, 1}},
		{{ 6, 10}, {1, 0, 5}},
		{{ 5, 10}, {1, 1, 3}},
		{{ 4, 10}, {1, 2, 2}},
		{{36, 11}, {1, 3, 2}},
		{{37, 11}, {1, 4, 2}},
		{{38, 11}, {1, 15, 1}},
		{{39, 11}, {1, 16, 1}},
		{{88, 12}, {0, 14, 1}},
		{{89, 12}, {1, 0, 8}},
		{{90, 12}, {1, 5, 2}},
		{{91, 12}, {1, 6, 2}},
		{{92, 12}, {1, 17, 1}},
		{{93, 12}, {1, 18, 1}},
		{{94, 12}, {1, 19, 1}},
		{{95, 12}, {1, 20, 1}}
	}
};

//----------------------------
/* constants taken from momusys/vm_common/inlcude/max_level.h */
static const byte max_level[2][2][64] = {
	{
		/* intra = 0, last = 0 */
		{
			12, 6, 4, 3, 3, 3, 3, 2,
			2, 2, 2, 1, 1, 1, 1, 1,
			1, 1, 1, 1, 1, 1, 1, 1,
			1, 1, 1, 0, 0, 0, 0, 0,
			0, 0, 0, 0, 0, 0, 0, 0,
			0, 0, 0, 0, 0, 0, 0, 0,
			0, 0, 0, 0, 0, 0, 0, 0,
			0, 0, 0, 0, 0, 0, 0, 0
		},
		/* intra = 0, last = 1 */
		{
			3, 2, 1, 1, 1, 1, 1, 1,
			1, 1, 1, 1, 1, 1, 1, 1,
			1, 1, 1, 1, 1, 1, 1, 1,
			1, 1, 1, 1, 1, 1, 1, 1,
			1, 1, 1, 1, 1, 1, 1, 1,
			1, 0, 0, 0, 0, 0, 0, 0,
			0, 0, 0, 0, 0, 0, 0, 0,
			0, 0, 0, 0, 0, 0, 0, 0
		}
	},
	{
		/* intra = 1, last = 0 */
		{
			27, 10, 5, 4, 3, 3, 3, 3,
			2, 2, 1, 1, 1, 1, 1, 0,
			0, 0, 0, 0, 0, 0, 0, 0,
			0, 0, 0, 0, 0, 0, 0, 0,
			0, 0, 0, 0, 0, 0, 0, 0,
			0, 0, 0, 0, 0, 0, 0, 0,
			0, 0, 0, 0, 0, 0, 0, 0,
			0, 0, 0, 0, 0, 0, 0, 0
		},
		/* intra = 1, last = 1 */
		{
			8, 3, 2, 2, 2, 2, 2, 1,
			1, 1, 1, 1, 1, 1, 1, 1,
			1, 1, 1, 1, 1, 0, 0, 0,
			0, 0, 0, 0, 0, 0, 0, 0,
			0, 0, 0, 0, 0, 0, 0, 0,
			0, 0, 0, 0, 0, 0, 0, 0,
			0, 0, 0, 0, 0, 0, 0, 0,
			0, 0, 0, 0, 0, 0, 0, 0
		}
	}
};

//----------------------------

static const byte max_run[2][2][64] = {
	{
		/* intra = 0, last = 0 */
		{
			0, 26, 10, 6, 2, 1, 1, 0,
			0, 0, 0, 0, 0, 0, 0, 0,
			0, 0, 0, 0, 0, 0, 0, 0,
			0, 0, 0, 0, 0, 0, 0, 0,
			0, 0, 0, 0, 0, 0, 0, 0,
			0, 0, 0, 0, 0, 0, 0, 0,
			0, 0, 0, 0, 0, 0, 0, 0,
			0, 0, 0, 0, 0, 0, 0, 0,
		},
		/* intra = 0, last = 1 */
		{
			0, 40, 1, 0, 0, 0, 0, 0,
			0, 0, 0, 0, 0, 0, 0, 0,
			0, 0, 0, 0, 0, 0, 0, 0,
			0, 0, 0, 0, 0, 0, 0, 0,
			0, 0, 0, 0, 0, 0, 0, 0,
			0, 0, 0, 0, 0, 0, 0, 0,
			0, 0, 0, 0, 0, 0, 0, 0,
			0, 0, 0, 0, 0, 0, 0, 0,
		}
	},
	{
		/* intra = 1, last = 0 */
		{
			0, 14, 9, 7, 3, 2, 1, 1,
			1, 1, 1, 0, 0, 0, 0, 0,
			0, 0, 0, 0, 0, 0, 0, 0,
			0, 0, 0, 0, 0, 0, 0, 0,
			0, 0, 0, 0, 0, 0, 0, 0,
			0, 0, 0, 0, 0, 0, 0, 0,
			0, 0, 0, 0, 0, 0, 0, 0,
			0, 0, 0, 0, 0, 0, 0, 0,
		},
		/* intra = 1, last = 1 */
		{
			0, 20, 6, 1, 0, 0, 0, 0,
			0, 0, 0, 0, 0, 0, 0, 0,
			0, 0, 0, 0, 0, 0, 0, 0,
			0, 0, 0, 0, 0, 0, 0, 0,
			0, 0, 0, 0, 0, 0, 0, 0,
			0, 0, 0, 0, 0, 0, 0, 0,
			0, 0, 0, 0, 0, 0, 0, 0,
			0, 0, 0, 0, 0, 0, 0, 0,
		}
	}
};

/******************************************************************
 * encoder tables                                                 *
 ******************************************************************/

static const VLC sprite_trajectory_len[15] = {
	{ 0x00 , 2},
	{ 0x02 , 3}, { 0x03, 3}, { 0x04, 3}, { 0x05, 3}, { 0x06, 3},
	{ 0x0E , 4}, { 0x1E, 5}, { 0x3E, 6}, { 0x7E, 7}, { 0xFE, 8},
	{ 0x1FE, 9}, {0x3FE,10}, {0x7FE,11}, {0xFFE,12}
};


/******************************************************************
 * decoder tables                                                 *
 ******************************************************************/

static const VLC mcbpc_intra_table[64] = {
	{-1, 0}, {20, 6}, {36, 6}, {52, 6}, {4, 4},  {4, 4},  {4, 4},  {4, 4},
	{19, 3}, {19, 3}, {19, 3}, {19, 3}, {19, 3}, {19, 3}, {19, 3}, {19, 3},
	{35, 3}, {35, 3}, {35, 3}, {35, 3}, {35, 3}, {35, 3}, {35, 3}, {35, 3},
	{51, 3}, {51, 3}, {51, 3}, {51, 3}, {51, 3}, {51, 3}, {51, 3}, {51, 3},
	{3, 1},  {3, 1},  {3, 1},  {3, 1},  {3, 1},  {3, 1},  {3, 1},  {3, 1},
	{3, 1},  {3, 1},  {3, 1},  {3, 1},  {3, 1},  {3, 1},  {3, 1},  {3, 1},
	{3, 1},  {3, 1},  {3, 1},  {3, 1},  {3, 1},  {3, 1},  {3, 1},  {3, 1},
	{3, 1},  {3, 1},  {3, 1},  {3, 1},  {3, 1},  {3, 1},  {3, 1},  {3, 1}
};

//----------------------------

static const VLC mcbpc_inter_table[257] = {
	{VLC_ERROR, 0}, {255, 9}, {52, 9}, {36, 9}, {20, 9}, {49, 9}, {35, 8}, {35, 8},
	{19, 8}, {19, 8}, {50, 8}, {50, 8}, {51, 7}, {51, 7}, {51, 7}, {51, 7},
	{34, 7}, {34, 7}, {34, 7}, {34, 7}, {18, 7}, {18, 7}, {18, 7}, {18, 7},
	{33, 7}, {33, 7}, {33, 7}, {33, 7}, {17, 7}, {17, 7}, {17, 7}, {17, 7},
	{4, 6}, {4, 6}, {4, 6}, {4, 6}, {4, 6}, {4, 6}, {4, 6}, {4, 6},
	{48, 6}, {48, 6}, {48, 6}, {48, 6}, {48, 6}, {48, 6}, {48, 6}, {48, 6},
	{3, 5}, {3, 5}, {3, 5}, {3, 5}, {3, 5}, {3, 5}, {3, 5}, {3, 5},
	{3, 5}, {3, 5}, {3, 5}, {3, 5}, {3, 5}, {3, 5}, {3, 5}, {3, 5},
	{32, 4}, {32, 4}, {32, 4}, {32, 4}, {32, 4}, {32, 4}, {32, 4}, {32, 4},
	{32, 4}, {32, 4}, {32, 4}, {32, 4}, {32, 4}, {32, 4}, {32, 4}, {32, 4},
	{32, 4}, {32, 4}, {32, 4}, {32, 4}, {32, 4}, {32, 4}, {32, 4}, {32, 4},
	{32, 4}, {32, 4}, {32, 4}, {32, 4}, {32, 4}, {32, 4}, {32, 4}, {32, 4},
	{16, 4}, {16, 4}, {16, 4}, {16, 4}, {16, 4}, {16, 4}, {16, 4}, {16, 4},
	{16, 4}, {16, 4}, {16, 4}, {16, 4}, {16, 4}, {16, 4}, {16, 4}, {16, 4},
	{16, 4}, {16, 4}, {16, 4}, {16, 4}, {16, 4}, {16, 4}, {16, 4}, {16, 4},
	{16, 4}, {16, 4}, {16, 4}, {16, 4}, {16, 4}, {16, 4}, {16, 4}, {16, 4},
	{2, 3}, {2, 3}, {2, 3}, {2, 3}, {2, 3}, {2, 3}, {2, 3}, {2, 3},
	{2, 3}, {2, 3}, {2, 3}, {2, 3}, {2, 3}, {2, 3}, {2, 3}, {2, 3},
	{2, 3}, {2, 3}, {2, 3}, {2, 3}, {2, 3}, {2, 3}, {2, 3}, {2, 3},
	{2, 3}, {2, 3}, {2, 3}, {2, 3}, {2, 3}, {2, 3}, {2, 3}, {2, 3},
	{2, 3}, {2, 3}, {2, 3}, {2, 3}, {2, 3}, {2, 3}, {2, 3}, {2, 3},
	{2, 3}, {2, 3}, {2, 3}, {2, 3}, {2, 3}, {2, 3}, {2, 3}, {2, 3},
	{2, 3}, {2, 3}, {2, 3}, {2, 3}, {2, 3}, {2, 3}, {2, 3}, {2, 3},
	{2, 3}, {2, 3}, {2, 3}, {2, 3}, {2, 3}, {2, 3}, {2, 3}, {2, 3},
	{1, 3}, {1, 3}, {1, 3}, {1, 3}, {1, 3}, {1, 3}, {1, 3}, {1, 3},
	{1, 3}, {1, 3}, {1, 3}, {1, 3}, {1, 3}, {1, 3}, {1, 3}, {1, 3},
	{1, 3}, {1, 3}, {1, 3}, {1, 3}, {1, 3}, {1, 3}, {1, 3}, {1, 3},
	{1, 3}, {1, 3}, {1, 3}, {1, 3}, {1, 3}, {1, 3}, {1, 3}, {1, 3},
	{1, 3}, {1, 3}, {1, 3}, {1, 3}, {1, 3}, {1, 3}, {1, 3}, {1, 3},
	{1, 3}, {1, 3}, {1, 3}, {1, 3}, {1, 3}, {1, 3}, {1, 3}, {1, 3},
	{1, 3}, {1, 3}, {1, 3}, {1, 3}, {1, 3}, {1, 3}, {1, 3}, {1, 3},
	{1, 3}, {1, 3}, {1, 3}, {1, 3}, {1, 3}, {1, 3}, {1, 3}, {1, 3},
	{0, 1}
};

//----------------------------

static const VLC cbpy_table[64] = {
	{-1, 0}, {-1, 0}, {6, 6},  {9, 6},  {8, 5},  {8, 5},  {4, 5},  {4, 5},
	{2, 5},  {2, 5},  {1, 5},  {1, 5},  {0, 4},  {0, 4},  {0, 4},  {0, 4},
	{12, 4}, {12, 4}, {12, 4}, {12, 4}, {10, 4}, {10, 4}, {10, 4}, {10, 4},
	{14, 4}, {14, 4}, {14, 4}, {14, 4}, {5, 4},  {5, 4},  {5, 4},  {5, 4},
	{13, 4}, {13, 4}, {13, 4}, {13, 4}, {3, 4},  {3, 4},  {3, 4},  {3, 4},
	{11, 4}, {11, 4}, {11, 4}, {11, 4}, {7, 4},  {7, 4},  {7, 4},  {7, 4},
	{15, 2}, {15, 2}, {15, 2}, {15, 2}, {15, 2}, {15, 2}, {15, 2}, {15, 2},
	{15, 2}, {15, 2}, {15, 2}, {15, 2}, {15, 2}, {15, 2}, {15, 2}, {15, 2}
};

//----------------------------

static const VLC TMNMVtab0[] = {
	{3, 4}, {-3, 4}, {2, 3}, {2, 3}, {-2, 3}, {-2, 3}, {1, 2},
	{1, 2}, {1, 2}, {1, 2}, {-1, 2}, {-1, 2}, {-1, 2}, {-1, 2}
};

//----------------------------

static const VLC TMNMVtab1[] = {
	{12, 10}, {-12, 10}, {11, 10}, {-11, 10},
	{10, 9}, {10, 9}, {-10, 9}, {-10, 9},
	{9, 9}, {9, 9}, {-9, 9}, {-9, 9},
	{8, 9}, {8, 9}, {-8, 9}, {-8, 9},
	{7, 7}, {7, 7}, {7, 7}, {7, 7},
	{7, 7}, {7, 7}, {7, 7}, {7, 7},
	{-7, 7}, {-7, 7}, {-7, 7}, {-7, 7},
	{-7, 7}, {-7, 7}, {-7, 7}, {-7, 7},
	{6, 7}, {6, 7}, {6, 7}, {6, 7},
	{6, 7}, {6, 7}, {6, 7}, {6, 7},
	{-6, 7}, {-6, 7}, {-6, 7}, {-6, 7},
	{-6, 7}, {-6, 7}, {-6, 7}, {-6, 7},
	{5, 7}, {5, 7}, {5, 7}, {5, 7},
	{5, 7}, {5, 7}, {5, 7}, {5, 7},
	{-5, 7}, {-5, 7}, {-5, 7}, {-5, 7},
	{-5, 7}, {-5, 7}, {-5, 7}, {-5, 7},
	{4, 6}, {4, 6}, {4, 6}, {4, 6},
	{4, 6}, {4, 6}, {4, 6}, {4, 6},
	{4, 6}, {4, 6}, {4, 6}, {4, 6},
	{4, 6}, {4, 6}, {4, 6}, {4, 6},
	{-4, 6}, {-4, 6}, {-4, 6}, {-4, 6},
	{-4, 6}, {-4, 6}, {-4, 6}, {-4, 6},
	{-4, 6}, {-4, 6}, {-4, 6}, {-4, 6},
	{-4, 6}, {-4, 6}, {-4, 6}, {-4, 6}
};

//----------------------------

static const VLC TMNMVtab2[] = {
	{32, 12}, {-32, 12}, {31, 12}, {-31, 12},
	{30, 11}, {30, 11}, {-30, 11}, {-30, 11},
	{29, 11}, {29, 11}, {-29, 11}, {-29, 11},
	{28, 11}, {28, 11}, {-28, 11}, {-28, 11},
	{27, 11}, {27, 11}, {-27, 11}, {-27, 11},
	{26, 11}, {26, 11}, {-26, 11}, {-26, 11},
	{25, 11}, {25, 11}, {-25, 11}, {-25, 11},
	{24, 10}, {24, 10}, {24, 10}, {24, 10},
	{-24, 10}, {-24, 10}, {-24, 10}, {-24, 10},
	{23, 10}, {23, 10}, {23, 10}, {23, 10},
	{-23, 10}, {-23, 10}, {-23, 10}, {-23, 10},
	{22, 10}, {22, 10}, {22, 10}, {22, 10},
	{-22, 10}, {-22, 10}, {-22, 10}, {-22, 10},
	{21, 10}, {21, 10}, {21, 10}, {21, 10},
	{-21, 10}, {-21, 10}, {-21, 10}, {-21, 10},
	{20, 10}, {20, 10}, {20, 10}, {20, 10},
	{-20, 10}, {-20, 10}, {-20, 10}, {-20, 10},
	{19, 10}, {19, 10}, {19, 10}, {19, 10},
	{-19, 10}, {-19, 10}, {-19, 10}, {-19, 10},
	{18, 10}, {18, 10}, {18, 10}, {18, 10},
	{-18, 10}, {-18, 10}, {-18, 10}, {-18, 10},
	{17, 10}, {17, 10}, {17, 10}, {17, 10},
	{-17, 10}, {-17, 10}, {-17, 10}, {-17, 10},
	{16, 10}, {16, 10}, {16, 10}, {16, 10},
	{-16, 10}, {-16, 10}, {-16, 10}, {-16, 10},
	{15, 10}, {15, 10}, {15, 10}, {15, 10},
	{-15, 10}, {-15, 10}, {-15, 10}, {-15, 10},
	{14, 10}, {14, 10}, {14, 10}, {14, 10},
	{-14, 10}, {-14, 10}, {-14, 10}, {-14, 10},
	{13, 10}, {13, 10}, {13, 10}, {13, 10},
	{-13, 10}, {-13, 10}, {-13, 10}, {-13, 10}
};

//----------------------------

static const VLC dc_lum_tab[] = {
	{0, 0}, {4, 3}, {3, 3}, {0, 3},
	{2, 2}, {2, 2}, {1, 2}, {1, 2},
};

//---------------------------
#define LEVELOFFSET 32

int Bitstream::bs_get_spritetrajectory() {

	for(int i = 0; i < 12; i++) {
		if((int)ShowBits(sprite_trajectory_len[i].len) == sprite_trajectory_len[i].code) {
			Skip(sprite_trajectory_len[i].len);
			return i;
		}
	}
	return -1;
}

//----------------------------

void S_decoder::init_vlc_tables() {

	dword i, j, intra, last, run,  run_esc, level, level_esc, escape, escape_len, offset;

	for (intra = 0; intra < 2; intra++)
		for (i = 0; i < 4096; i++)
			DCT3D[intra][i].event.level = 0;

	for (intra = 0; intra < 2; intra++) {
		for (last = 0; last < 2; last++) {
			for (run = 0; run < 63 + last; run++) {
				for (level = 0; level < (dword)(32 << intra); level++) {
					offset = !intra * LEVELOFFSET;
					coeff_VLC[intra][last][level + offset][run].len = 128;
				}
			}
		}
	}

	for (intra = 0; intra < 2; intra++) {
		for (i = 0; i < 102; i++) {
			offset = !intra * LEVELOFFSET;

			for (j = 0; j < (dword)(1 << (12 - coeff_tab[intra][i].vlc.len)); j++) {
				DCT3D[intra][(coeff_tab[intra][i].vlc.code << (12 - coeff_tab[intra][i].vlc.len)) | j].len    = coeff_tab[intra][i].vlc.len;
				DCT3D[intra][(coeff_tab[intra][i].vlc.code << (12 - coeff_tab[intra][i].vlc.len)) | j].event = coeff_tab[intra][i].event;
			}

			coeff_VLC[intra][coeff_tab[intra][i].event.last][coeff_tab[intra][i].event.level + offset][coeff_tab[intra][i].event.run].code
			    = coeff_tab[intra][i].vlc.code << 1;
			coeff_VLC[intra][coeff_tab[intra][i].event.last][coeff_tab[intra][i].event.level + offset][coeff_tab[intra][i].event.run].len
			    = coeff_tab[intra][i].vlc.len + 1;

			if (!intra) {
				coeff_VLC[intra][coeff_tab[intra][i].event.last][offset - coeff_tab[intra][i].event.level][coeff_tab[intra][i].event.run].code
				    = (coeff_tab[intra][i].vlc.code << 1) | 1;
				coeff_VLC[intra][coeff_tab[intra][i].event.last][offset - coeff_tab[intra][i].event.level][coeff_tab[intra][i].event.run].len
				    = coeff_tab[intra][i].vlc.len + 1;
			}
		}
	}

	for (intra = 0; intra < 2; intra++) {
		for (last = 0; last < 2; last++) {
			for (run = 0; run < 63 + last; run++) {
				for (level = 1; level < (dword)(32 << intra); level++) {

					if (level <= max_level[intra][last][run] && run <= max_run[intra][last][level])
						continue;

					offset = !intra * LEVELOFFSET;
					level_esc = level - max_level[intra][last][run];
					run_esc = run - 1 - max_run[intra][last][level];

					if (level_esc <= max_level[intra][last][run] && run <= max_run[intra][last][level_esc]) {
						escape     = ESCAPE1;
						escape_len = 7 + 1;
						run_esc    = run;
					} else {
						if (run_esc <= max_run[intra][last][level] && level <= max_level[intra][last][run_esc]) {
							escape     = ESCAPE2;
							escape_len = 7 + 2;
							level_esc  = level;
						} else {
							if (!intra) {
								coeff_VLC[intra][last][level + offset][run].code
								    = (ESCAPE3 << 21) | (last << 20) | (run << 14) | (1 << 13) | ((level & 0xfff) << 1) | 1;
								coeff_VLC[intra][last][level + offset][run].len = 30;
								coeff_VLC[intra][last][offset - level][run].code
								    = (ESCAPE3 << 21) | (last << 20) | (run << 14) | (1 << 13) | ((-(int)level & 0xfff) << 1) | 1;
								coeff_VLC[intra][last][offset - level][run].len = 30;
							}
							continue;
						}
					}

					coeff_VLC[intra][last][level + offset][run].code
					    = (escape << coeff_VLC[intra][last][level_esc + offset][run_esc].len)
					      |  coeff_VLC[intra][last][level_esc + offset][run_esc].code;
					coeff_VLC[intra][last][level + offset][run].len
					    = coeff_VLC[intra][last][level_esc + offset][run_esc].len + escape_len;

					if (!intra) {
						coeff_VLC[intra][last][offset - level][run].code
						    = (escape << coeff_VLC[intra][last][level_esc + offset][run_esc].len)
						      |  coeff_VLC[intra][last][level_esc + offset][run_esc].code | 1;
						coeff_VLC[intra][last][offset - level][run].len
						    = coeff_VLC[intra][last][level_esc + offset][run_esc].len + escape_len;
					}
				}

				if (!intra) {
					coeff_VLC[intra][last][0][run].code
					    = (ESCAPE3 << 21) | (last << 20) | (run << 14) | (1 << 13) | ((-32 & 0xfff) << 1) | 1;
					coeff_VLC[intra][last][0][run].len = 30;
				}
			}
		}
	}

	/* init sprite_trajectory tables
	 * even if GMC is not specified (it might be used later...) */
	/*
	{
	   dword k;
	   int l;
	sprite_trajectory_code[0+16384].code = 0;
	sprite_trajectory_code[0+16384].len = 0;
	for (k=0;k<14;k++) {
	   int limit = (1<<k);

	   for (l=-(2*limit-1); l <= -limit; l++) {
	      sprite_trajectory_code[l+16384].code = (2*limit-1)+l;
	      sprite_trajectory_code[l+16384].len = k+1;
	   }

	   for (l=limit; l<= 2*limit-1; l++) {
	      sprite_trajectory_code[l+16384].code = l;
	      sprite_trajectory_code[l+16384].len = k+1;
	   }
	}
	}
	*/
}


/***************************************************************
 * decoding stuff starts here                                  *
 ***************************************************************/


/*
 * for IVOP addbits == 0
 * for PVOP addbits == fcode - 1
 * for BVOP addbits == max(fcode,bcode) - 1
 * returns true or false
 */
int Bitstream::check_resync_marker(int addbits) {

	dword nbitsresyncmarker = NUMBITS_VP_RESYNC_MARKER + addbits;
	dword nbits = NumBitsToByteAlign();
	dword code = ShowBits(nbits);

	if(code == (((dword)1 << (nbits - 1)) - 1)) {
		return ShowBitsFromByteAlign(nbitsresyncmarker) == RESYNC_MARKER;
	}
	return 0;
}

//----------------------------

int Bitstream::get_mcbpc_intra() {

	dword index;

	index = ShowBits(9);
	index >>= 3;

	Skip(mcbpc_intra_table[index].len);

	return mcbpc_intra_table[index].code;

}

//----------------------------

int Bitstream::GetMcbpcInter() {

	dword index = MIN(ShowBits(9), 256);
	Skip(mcbpc_inter_table[index].len);

	return mcbpc_inter_table[index].code;
}

//----------------------------

int Bitstream::GetCbpy(int intra) {

	dword index = ShowBits(6);
	Skip(cbpy_table[index].len);
	int cbpy = cbpy_table[index].code;
	if(!intra)
		cbpy = 15 - cbpy;
	return cbpy;
}

//----------------------------

static int get_mv_data(Bitstream * bs) {

	if(bs->GetBit())
		return 0;

	dword index = bs->ShowBits(12);

	if(index >= 512) {
		index = (index >> 8) - 2;
		bs->Skip(TMNMVtab0[index].len);
		return TMNMVtab0[index].code;
	}

	if (index >= 128) {
		index = (index >> 2) - 32;
		bs->Skip(TMNMVtab1[index].len);
		return TMNMVtab1[index].code;
	}

	index -= 4;

	bs->Skip(TMNMVtab2[index].len);
	return TMNMVtab2[index].code;
}

//----------------------------

int Bitstream::GetMoveVector(int fcode) {

	int scale_fac = 1 << (fcode - 1);
	int data = get_mv_data(this);

	if(scale_fac == 1 || data == 0)
		return data;

	int res = GetBits(fcode - 1);
	int mv = ((ABS(data) - 1) * scale_fac) + res + 1;

	return data < 0 ? -mv : mv;
}

//----------------------------

int Bitstream::get_dc_dif(dword dc_size) {

	int code = GetBits(dc_size);
	int msb = code >> (dc_size - 1);

	if (msb == 0)
		return (-1 * (code ^ ((1 << dc_size) - 1)));

	return code;

}

//----------------------------

int Bitstream::get_dc_size_lum() {

	int code = ShowBits(11);

	for(int i = 11; i > 3; i--) {
		if (code == 1) {
			Skip(i);
			return i + 1;
		}
		code >>= 1;
	}

	Skip(dc_lum_tab[code].len);
	return dc_lum_tab[code].code;

}

//----------------------------

int Bitstream::get_dc_size_chrom() {

	dword code, i;

	code = ShowBits(12);

	for (i = 12; i > 2; i--) {
		if (code == 1) {
			Skip(i);
			return i;
		}
		code >>= 1;
	}

	return 3 - GetBits(2);

}

//----------------------------

int S_decoder::get_coeff(Bitstream *bs, int *run, int *last, int intra, int short_video_header) {

	dword mode;
	int level;
	REVERSE_EVENT *reverse_event;

	if(short_video_header)    /* inter-VLCs will be used for both intra and inter blocks */
		intra = 0;

	if(bs->ShowBits(7) != ESCAPE) {
		reverse_event = &DCT3D[intra][bs->ShowBits(12)];

		if((level = reverse_event->event.level) == 0)
			goto error;

		*last = reverse_event->event.last;
		*run  = reverse_event->event.run;

		bs->Skip(reverse_event->len);

		return bs->GetBits(1) ? -level : level;
	}

	bs->Skip(7);

	if(short_video_header) {
		//escape mode 4 - H.263 type, only used if short_video_header = 1
		*last = bs->GetBit();
		*run = bs->GetBits(6);
		level = bs->GetBits(8);
		if (level == 0 || level == 128)
			DPRINTF(XVID_DEBUG_ERROR, "Illegal LEVEL for ESCAPE mode 4: %d\n", level);
		return (level << 24) >> 24;
	}

	mode = bs->ShowBits(2);
	if(mode < 3) {
		bs->Skip((mode == 2) ? 2 : 1);

		reverse_event = &DCT3D[intra][bs->ShowBits(12)];

		if((level = reverse_event->event.level) == 0)
			goto error;
		*last = reverse_event->event.last;
		*run  = reverse_event->event.run;
		bs->Skip(reverse_event->len);

		if(mode < 2)        /* first escape mode, level is offset */
			level += max_level[intra][*last][*run];
		else              /* second escape mode, run is offset */
			*run += max_run[intra][*last][level] + 1;
		return bs->GetBits(1) ? -level : level;
	}
	//third escape mode - fixed length codes
	bs->Skip(2);
	*last = bs->GetBits(1);
	*run = bs->GetBits(6);
	bs->Skip(1);      //marker
	level = bs->GetBits(12);
	bs->Skip(1);      //marker

	return (level << 20) >> 20;

error:
	*run = VLC_ERROR;
	return 0;
}

//----------------------------

void S_decoder::get_intra_block(Bitstream *bs, int *block, int direction, int coeff) {

	const dword *scan = scan_tables[direction];
	int last;
	do {
		int run;
		int level = get_coeff(bs, &run, &last, 1, 0);
		if(run == -1) {
			DPRINTF(XVID_DEBUG_ERROR,"fatal: invalid run");
			break;
		}
		coeff += run;
		block[scan[coeff]] = level;

		//DPRINTF(XVID_DEBUG_COEFF,"block[%i] %i\n", scan[coeff], level);

		if(level < -2047 || level > 2047) {
			DPRINTF(XVID_DEBUG_ERROR,"warning: intra_overflow %i\n", level);
		}
		coeff++;
	} while(!last);
}

//----------------------------

void S_decoder::get_inter_block(Bitstream * bs, int *block, int direction) {

	const dword *scan = scan_tables[direction];
	int p = 0;
	int last;
	do {
		int run;
		int level = get_coeff(bs, &run, &last, 0, 0);
		if(run == -1) {
			DPRINTF(XVID_DEBUG_ERROR,"fatal: invalid run");
			break;
		}
		p += run;
		block[scan[p]] = level;

		//DPRINTF(XVID_DEBUG_COEFF,"block[%i] %i\n", scan[p], level);

		if(level < -2047 || level > 2047) {
			DPRINTF(XVID_DEBUG_ERROR,"warning: inter overflow %i\n", level);
		}
		p++;
	} while(!last);
}

//----------------------------


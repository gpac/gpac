/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Romain Bouqueau
 *			Copyright (c) Motion Spell 2026
 *					All rights reserved
 *
 *  This file is part of GPAC / AV1 layer splitter/aggregator filters
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

#ifndef GPAC_BS_AV1_H
#define GPAC_BS_AV1_H

typedef struct{
	u32 size, type, spatial_id, temporal_id;
} BSAV1OBU;

//inspect low-overhead OBU input buffer to a BSAV1OBU struct
//note: framed AV1 use the low-overhead OBU format with a size field OBU so we only
//      inspect the framing (enh layers dont need to be decodable without their deps)
static GF_Err bs_av1_obu_inspect(const u8 *data, u32 size, BSAV1OBU *obu)
{
	u32 pos = 1, i;
	u64 payload = 0;
	if (!size || (data[0] & 0x81) || !(data[0] & 2))
		return GF_NON_COMPLIANT_BITSTREAM;

	obu->type = (data[0] >> 3) & 15;
	obu->spatial_id = obu->temporal_id = 0;
	if (data[0] & 4) {
		if (size < 2)
			return GF_NON_COMPLIANT_BITSTREAM;

		obu->temporal_id = data[1] >> 5;
		obu->spatial_id = (data[1] >> 3) & 3;
		pos++;
	}

	for (i=0; i<8; i++) {
		u8 byte;
		if (pos == size)
			return GF_NON_COMPLIANT_BITSTREAM;
		byte = data[pos++];
		payload |= (u64) (byte & 127) << (7*i);
		if (!(byte & 128)) {
			if (payload > size-pos)
			return GF_NON_COMPLIANT_BITSTREAM;
			obu->size = pos + (u32) payload;
			return GF_OK;
		}
	}
	return GF_NON_COMPLIANT_BITSTREAM;
}

#endif

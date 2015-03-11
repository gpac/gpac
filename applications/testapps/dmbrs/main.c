/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2000-2012
 *					All rights reserved
 *
 *  This file is part of GPAC / DMB application
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
#include <gpac/mpegts.h>

void save_ts(char *filename, unsigned char *data)
{
	FILE *ts_out = gf_fopen(filename,"a+b");
	gf_fwrite(data, 1, 188, ts_out);
	gf_fclose(ts_out);
}
void save_rs_0(char *filename, unsigned char *data)
{
	FILE *rs_out = gf_fopen(filename,"a+b");
	gf_fwrite(data, 1, 204, rs_out);
	gf_fclose(rs_out);
}

void RS_Interleaver(GF_BitStream *bs, char *out_name)
{
	u8 *tmp;
	u8 ts0[204], ts1[204], ts2[204], ts3[204], ts4[204], ts5[204], ts6[204], ts7[204], ts8[204], ts9[204], ts10[204], ts11[204];
	u8 *ts[12] = { ts0, ts1, ts2, ts3, ts4, ts5, ts6, ts7, ts8, ts9, ts10, ts11 };
	u8 rs[204];
	u32 i;
	u64 bs_data;
	u32 k;

	memset(ts[0], 0xFF, 204);
	ts[0][0] = 0x47;
	memset(ts[1], 0xFF, 204);
	ts[1][0] = 0x47;
	memset(ts[2], 0xFF, 204);
	ts[2][0] = 0x47;
	memset(ts[3], 0xFF, 204);
	ts[3][0] = 0x47;
	memset(ts[4], 0xFF, 204);
	ts[4][0] = 0x47;
	memset(ts[5], 0xFF, 204);
	ts[5][0] = 0x47;
	memset(ts[6], 0xFF, 204);
	ts[6][0] = 0x47;
	memset(ts[7], 0xFF, 204);
	ts[7][0] = 0x47;
	memset(ts[8], 0xFF, 204);
	ts[8][0] = 0x47;
	memset(ts[9], 0xFF, 204);
	ts[9][0] = 0x47;
	memset(ts[10], 0xFF, 204);
	ts[10][0] = 0x47;

	k = 11;
	bs_data = gf_bs_available(bs);
	while (bs_data > 188 || k > 0) {

		gf_bs_read_data(bs, ts[11], 188);
		if (bs_data == 0) {
			memset(ts[11], 0xFF, 204);
			ts[11][0] = 0x47;
			k--;
		}
		bs_data = gf_bs_available(bs);

		for (i=0; i<(17*12); i+=12) { // 1 paquet RS

			rs[i]    = ts[11][i];
			if (rs[0] != 0x47) {
				printf ("error ts sync byte");
			}
			rs[i+1]  = ts[10][i+1];
			rs[i+2]	 = ts[9][i+2];
			rs[i+3]  = ts[8][i+3];
			rs[i+4]  = ts[7][i+4];
			rs[i+5]  = ts[6][i+5];
			rs[i+6]  = ts[5][i+6];
			rs[i+7]  = ts[4][i+7];
			rs[i+8]  = ts[3][i+8];
			rs[i+9]  = ts[2][i+9];
			rs[i+10] = ts[1][i+10];
			rs[i+11] = ts[0][i+11];
		}

		if (rs[0] != 0x47) {
			printf("error in output TS\n");
		} else {
			save_rs_0(out_name, rs);
		}

		tmp = ts[0];
		ts[0] = ts[1];
		ts[1] = ts[2];
		ts[2] = ts[3];
		ts[3] = ts[4];
		ts[4] = ts[5];
		ts[5] = ts[6];
		ts[6] = ts[7];
		ts[7] = ts[8];
		ts[8] = ts[9];
		ts[9] = ts[10];
		ts[10] = ts[11];
		ts[11] = tmp;
	}
}

void RS_Deinterleaver(GF_BitStream *bs, char *out_name)
{
	u8 rs0[204], rs1[204], rs2[204], rs3[204], rs4[204], rs5[204], rs6[204], rs7[204], rs8[204], rs9[204], rs10[204], rs11[204];
	u8 *rs[12] = { rs0, rs1, rs2, rs3, rs4, rs5, rs6, rs7, rs8, rs9, rs10, rs11 };
	u8 *tmp;
	u8 buf[204];
	u32 i;
	u64 bs_data;
	u32 k = 0;

	memset(rs[0], 0, 204);
	memset(rs[1], 0, 204);
	memset(rs[2], 0, 204);
	memset(rs[3], 0, 204);
	memset(rs[4], 0, 204);
	memset(rs[5], 0, 204);
	memset(rs[6], 0, 204);
	memset(rs[7], 0, 204);
	memset(rs[8], 0, 204);
	memset(rs[9], 0, 204);
	memset(rs[10], 0, 204);
	memset(rs[11], 0, 204);

	bs_data = gf_bs_available(bs);
	while (bs_data > 204) {
		u64 pos;
		k++;
//		printf("TS Packet Number: %d\r", k);

		pos = gf_bs_get_position(bs);
		gf_bs_read_data(bs, buf, 204);
		bs_data = gf_bs_available(bs);

		while ((buf[0] != 0x47) && (bs_data > 0)) {
			printf("error in input TS %d\n", k);
			//return;
			pos++;
			gf_bs_seek(bs, pos);
			gf_bs_read_data(bs, buf, 204);
			bs_data = gf_bs_available(bs);
		}

		for (i=0; i<(17*12); i+=12) { // 1 paquet
			rs[0][i]     = buf[i];
			rs[1][i+1]   = buf[i+1];
			rs[2][i+2]	 = buf[i+2];
			rs[3][i+3]   = buf[i+3];
			rs[4][i+4]   = buf[i+4];
			rs[5][i+5]   = buf[i+5];
			rs[6][i+6]   = buf[i+6];
			rs[7][i+7]   = buf[i+7];
			rs[8][i+8]   = buf[i+8];
			rs[9][i+9]   = buf[i+9];
			rs[10][i+10] = buf[i+10];
			rs[11][i+11] = buf[i+11];
		}
		if (k >= 12) {
			if (rs[11][0] != 0x47) {
				printf("error in output TS\n");
			} else {
				save_ts(out_name, rs[11]);
			}
		}
		tmp = rs[11];
		rs[11] = rs[10];
		rs[10] = rs[9];
		rs[9] = rs[8];
		rs[8] = rs[7];
		rs[7] = rs[6];
		rs[6] = rs[5];
		rs[5] = rs[4];
		rs[4] = rs[3];
		rs[3] = rs[2];
		rs[2] = rs[1];
		rs[1] = rs[0];
		rs[0] = tmp;
	}
}

void main(int argc, char **argv)
{
	FILE *in;
	GF_BitStream *bs;

	/* generation d'un TS aléatoire */
	/*
		if ((in=gf_fopen(argv[1], "wb")) == NULL) {
			printf( "Impossible d'ouvrir %s en lecture.\n", argv[1]);
		}
		{
			char buffer[188];
			u32 j, i, nb_packets = 300;
			for (i = 0; i < nb_packets; i++) {
				buffer[0] = 0x47;
				for (j = 1; j <188; j++) {
					buffer[j] = rand();//j;
				}
				gf_fwrite(buffer, 1, 188, in);
			}
		}
		gf_fclose(in);
		if ((in=gf_fopen(argv[1], "rb")) == NULL) {
			printf( "Impossible d'ouvrir %s en lecture.\n", argv[1]);
		}

		bs = gf_bs_from_file(in, GF_BITSTREAM_READ);
		if (bs == NULL) return;

		RS_Interleaver(bs, argv[2]);
		gf_fclose(in);
		gf_bs_del(bs);
	*/


	if ((in=gf_fopen(argv[1], "rb")) == NULL) {
		printf( "Impossible d'ouvrir %s en lecture.\n", argv[1]);
	}

	bs = gf_bs_from_file(in, GF_BITSTREAM_READ);
	if (bs == NULL) return;

	RS_Deinterleaver(bs, argv[2]);
	gf_fclose(in);
	gf_bs_del(bs);

}
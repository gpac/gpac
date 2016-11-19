/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2000-2012
 *					All rights reserved
 *
 *  This file is part of GPAC / command-line mp4 toolbox
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

#include <stdlib.h>
#include <stdio.h>
#include <gpac/bitstream.h>
#include <gpac/internal/media_dev.h>


int main (int argc, char **argv)
{
	FILE* file = NULL;
	HEVCState hevc; // = {.sps_active_idx=-1};
 	u8 nal_unit_type, temporal_id, layer_id;

	if(2 == argc){
		file = fopen(argv[1],"rb");
                if(file != NULL){
                        GF_BitStream *bs = NULL;
                        bs = gf_bs_from_file(file, GF_BITSTREAM_READ);
	       		if(bs != NULL){
				gf_media_hevc_parse_nalu(bs, &hevc, &nal_unit_type, &temporal_id, &layer_id);
		                int i = 0;
     				printf("====%p=====\n",&hevc);
				printf("width:	%d\n",hevc.sps[0].width);
				for(i=0;i<64;i++){
  					printf("====%d=====\n",i);
	      				printf("sps_active_idx:	%d\n",hevc.sps_active_idx);
	       				printf("id:	%d\n",hevc.pps[i].id);
					printf("sps_id:	%d\n",hevc.pps[i].sps_id);
					printf("tiles_enabled_flag:	%d\n",hevc.pps[i].tiles_enabled_flag);
	     				printf("uniform_spacing_flag:	%d\n",hevc.pps[i].uniform_spacing_flag);
					printf("num_tile_columns:	%d\n",hevc.pps[i].num_tile_columns);
					printf("num_tile_rows:	%d\n",hevc.pps[i].num_tile_rows);
					printf("state:	%d\n",hevc.pps[i].state);
					printf("%d\n",nal_unit_type);
					//printf("%d\n",temporal_id);
				}
		        }
                }
	}
	return 0;
}

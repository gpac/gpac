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




void printVPS(char *buffer, u32 bz, HEVCState* hevc){
	gf_media_hevc_read_vps(buffer,bz, hevc);
	printf("===VPS===\n");
	int i;
	for(i=0;i<2;i++){
  		printf("====%d=====\n",i);
	       	printf("id:	%d\n",(*hevc).vps[i].id);
		printf("num_profile_tier_level:	%d\n",(*hevc).vps[i].num_profile_tier_level);
		}
}


void printSPS(char *buffer, u32 bz, HEVCState* hevc){
	gf_media_hevc_read_sps(buffer,bz, hevc);
	printf("===SPS==%d===\n",(*hevc).sps_active_idx);
	int i;
	for(i=0;i<2;i++){
  		printf("====%d=====\n",i);
		printf("id:	%d\n",(*hevc).sps[i].id);
	       	printf("width:	%d\n",(*hevc).sps[i].width);
		printf("height:	%d\n",(*hevc).sps[i].height);
		printf("cw_flag:	%d\n",(*hevc).sps[i].cw_flag);
		}
}

void printPPS(char *buffer, u32 bz, HEVCState* hevc){
	gf_media_hevc_read_pps(buffer,bz, hevc);
	int i = 0;
	printf("===PPS===\n");
  		printf("id:	%d\n",(*hevc).pps[i].id);
	     	printf("column_width:	%d\n",(*hevc).pps[i].column_width[i]);
	     	printf("uniform_spacing_flag:	%d\n",(*hevc).pps[i].uniform_spacing_flag);
	     	printf("dependent_slice_segments_enabled_flag:	%d\n",(*hevc).pps[i].dependent_slice_segments_enabled_flag);
		printf("num_tile_columns:	%d\n",(*hevc).pps[i].num_tile_columns);
		printf("num_tile_rows:	%d\n",(*hevc).pps[i].num_tile_rows);
		printf("num_profile_tier_level:	%d\n",(*hevc).vps[i].num_profile_tier_level);
		printf("height:	%d\n",(*hevc).sps[i].height);
		
}

void bs_set_ue(GF_BitStream *bs,s32 num) {
    s32 length = 1;
    s32 temp = ++num;

    while (temp != 1) {
        temp >>= 1;
        length += 2;
    }
    
    gf_bs_write_int(bs, 0, length >> 1);
    gf_bs_write_int(bs, num, (length+1) >> 1);
}

/*void printSliceInfo(HEVCState hevc){
	printf("===SSH===\n");
	int i;
	for(i=0;i<2;i++){
  		printf("id:	%d\n",hevc.pps[i].id);
		printf("tiles_enabled_flag:	%d\n",hevc.pps[i].tiles_enabled_flag);
	     	printf("uniform_spacing_flag:	%d\n",hevc.pps[i].uniform_spacing_flag);
		printf("dependent_slice_segments_enabled_flag:	%d\n",hevc.pps[i].dependent_slice_segments_enabled_flag);
		printf("num_tile_columns:	%d\n",hevc.pps[i].num_tile_columns);
		printf("num_tile_rows:	%d\n",hevc.pps[i].num_tile_rows);
		}
}*/


u64 size_of_file(FILE *file)
{
	long cur = ftell(file);
	fseek(file, 0L, SEEK_END);
	u64 size = (u64) ftell(file);
	fseek(file, cur, SEEK_SET);
	return size;
}


int main (int argc, char **argv)
{
	printf("===begin====\n");
	FILE* file = NULL;
	if(2 == argc){
		file = fopen(argv[1],"rb");
                if(file != NULL){
			//print_nalu(file);
			GF_BitStream *bs = NULL; //==The whole bitstream
		        bs = gf_bs_from_file(file, GF_BITSTREAM_READ);
			char *buffer =NULL; //==pointer to hold chunk of NAL data
			u32 nal_start_code;
			u32 nal_length;
			int is_nal = 0;
			
			if(bs != NULL){
				HEVCState hevc;
				u8 nal_unit_type =1, temporal_id, layer_id;
				int nal_num=0;
				while( gf_bs_get_size(bs)!= gf_bs_get_position(bs))
				{
					nal_length = gf_media_nalu_next_start_code_bs(bs);
					gf_bs_skip_bytes(bs, nal_length);
					nal_start_code = gf_bs_read_int(bs, 24);
					if(0x000001 == nal_start_code){
						is_nal = 1;
					}
					else{
						if(0x000000 == nal_start_code && 1==gf_bs_read_int(bs, 8)){
							is_nal = 1;
						}
						
						else{
							is_nal = 0;
						}
					}

					if(is_nal)
					{
						nal_length = gf_media_nalu_next_start_code_bs(bs);
						printf("\n\n");
						if(0==nal_length)
							nal_length = gf_bs_get_size(bs) - gf_bs_get_position(bs);
						buffer = malloc(sizeof(char)*nal_length);
						gf_bs_read_data(bs,buffer,nal_length);
						GF_BitStream *bs_tmp = gf_bs_new(buffer, nal_length, GF_BITSTREAM_READ);
						gf_media_hevc_parse_nalu(bs_tmp, &hevc, &nal_unit_type, &temporal_id, &layer_id);
						nal_num++;	
						printf("nal_number: \t%d\t%d\n ",nal_num,nal_unit_type);
						switch(nal_unit_type){
							case 32:
								printVPS(buffer,nal_length,&hevc);
								break;
							case 33:
								printSPS(buffer,nal_length,&hevc);
								break;
							case 34:
								printPPS(buffer,nal_length,&hevc);
								break;
							case 19:
							case 20:
							case 1:
								printf("===Slice===\n");
								printf("Slice adresse\t:%d\n",hevc.s_info.slice_segment_address);
								printf("first_slice_segment_in_pic_flag\t:%d\n",hevc.s_info.first_slice_segment_in_pic_flag);
								printf("frame_num\t:%d\n",hevc.s_info.frame_num);
								break;
							default:
								printf("nal_unit_type not managed\n\n");
								break;

						}

					}
					else{
						printf("6\n");
						printf("==%lu==\n",(gf_bs_get_size(bs) - gf_bs_get_position(bs)));
						printf("s==%lu==\n",gf_bs_get_size(bs));
						printf("p==%lu==\n", gf_bs_get_position(bs));
						gf_bs_skip_bytes(bs, gf_bs_get_size(bs) - gf_bs_get_position(bs));	
					}
				}
			}

                }
	}
	return 0;
}


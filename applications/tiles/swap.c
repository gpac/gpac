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


static u8 avc_golomb_bits[256] = {
	8, 7, 6, 6, 5, 5, 5, 5, 4, 4, 4, 4, 4, 4, 4, 4, 3,
	3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 2, 2,
	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 1, 1, 1, 1,
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
	1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0
};

static u32 bs_get_ue(GF_BitStream *bs)
{
	u8 coded;
	u32 bits = 0, read = 0;
	while (1) {
		read = gf_bs_peek_bits(bs, 8, 0);
		if (read) break;
		//check whether we still have bits once the peek is done since we may have less than 8 bits available
		if (!gf_bs_available(bs)) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_CODING, ("[AVC/HEVC] Not enough bits in bitstream !!\n"));
			return 0;
		}
		gf_bs_read_int(bs, 8);
		bits += 8;
	}
	coded = avc_golomb_bits[read];
	gf_bs_read_int(bs, coded);
	bits += coded;
	return gf_bs_read_int(bs, bits + 1) - 1;
}

static s32 bs_get_se(GF_BitStream *bs)
{
	u32 v = bs_get_ue(bs);
	if ((v & 0x1) == 0) return (s32) (0 - (v>>1));
	return (v + 1) >> 1;
}

void bs_set_ue(GF_BitStream *bs,s32 num) 
{
    	s32 length = 1;
    	s32 temp = ++num;

    	while (temp != 1) 
	{
        	temp >>= 1;
        	length += 2;
    	}
    
    	gf_bs_write_int(bs, 0, length >> 1);
    	gf_bs_write_int(bs, num, (length+1) >> 1);
}

void rewrite_slice_address(u32 new_address, char *in_slice, u32 in_slice_length, char **out_slice, u32 *out_slice_length, HEVCState* hevc) 
{
	u64 length_no_use = 0;
	GF_BitStream *bs_ori = gf_bs_new(in_slice, in_slice_length+1, GF_BITSTREAM_READ);
	GF_BitStream *bs_rw = gf_bs_new(NULL, length_no_use, GF_BITSTREAM_WRITE);
	u32 first_slice_segment_in_pic_flag;
	u32 dependent_slice_segment_flag;
	int address_ori;	

	/*HEVCState hevc;
	HEVCSliceInfo si;
	u8 nal_unit_type, temporal_id, layer_id;
	gf_media_hevc_parse_nalu(bs_rw, &hevc, &nal_unit_type, &temporal_id, &layer_id);*/


	u32 F = gf_bs_read_int(bs_ori, 1);			 //nal_unit_header
	gf_bs_write_int(bs_rw, F, 1);

	u32 nal_unit_type = gf_bs_read_int(bs_ori, 6);
	gf_bs_write_int(bs_rw, nal_unit_type, 6);
	u32 rest_nalu_header = gf_bs_read_int(bs_ori, 9);
	gf_bs_write_int(bs_rw, rest_nalu_header, 9);


	first_slice_segment_in_pic_flag = gf_bs_read_int(bs_ori, 1);    //first_slice_segment_in_pic_flag
	if (new_address == 0)  					
	{	
		gf_bs_write_int(bs_rw, 1, 1);
	}
	else
	{
		gf_bs_write_int(bs_rw, 0, 1);	
	}
	
 
	if (nal_unit_type >= 16 && nal_unit_type <= 23)                 //no_output_of_prior_pics_flag
	{
		u32 no_output_of_prior_pics_flag = gf_bs_read_int(bs_ori, 1);    
		gf_bs_write_int(bs_rw, no_output_of_prior_pics_flag, 1);
	}
	else;

	u32 pps_id = bs_get_ue(bs_ori);					 //pps_id
	bs_set_ue(bs_rw, pps_id);

	HEVC_PPS *pps;
	HEVC_SPS *sps;
	pps = &hevc->pps[pps_id];
	sps = &hevc->sps[pps->sps_id];

	if (!first_slice_segment_in_pic_flag && pps->dependent_slice_segments_enabled_flag) //dependent_slice_segment_flag READ
	{ 
		dependent_slice_segment_flag = gf_bs_read_int(bs_ori, 1);
	}
	else;
	if (!first_slice_segment_in_pic_flag) 						    //slice_segment_address READ
	{
		address_ori = gf_bs_read_int(bs_ori, sps->bitsSliceSegmentAddress);
	}
	else 	//original slice segment address = 0
	{
		address_ori = gf_bs_read_int(bs_ori, 1);
	}

	if (new_address) //new address != 0  						    //slice_segment_address WRITE
	{
		gf_bs_write_int(bs_rw, new_address, sps->bitsSliceSegmentAddress);
	}
	else //new_address = 0
	{
		gf_bs_write_int(bs_rw, new_address, 1);
	}

	/*PicSizeInCtbsY = PicWidthInCtbsY * PicHeightInCtbsY;
	gf_bs_read_int(bs_ori, ceil(Log2( PicSizeInCtbsY))	*/
	
	//******
	while (gf_bs_get_size(bs_ori)*8 != gf_bs_get_position(bs_ori)*8+gf_bs_get_bit_position(bs_ori)) //Rest contents copying
	{
		u32 rest_contents = gf_bs_read_int(bs_ori, 1);
		gf_bs_write_int(bs_rw, rest_contents, 1);
	}

	gf_bs_align(bs_rw);						//align
	*out_slice_length = gf_bs_get_position(bs_rw);			//Write to new buffer
	*out_slice = malloc(gf_bs_get_size(bs_rw)*8);
	gf_bs_seek(bs_rw, 0);
	gf_bs_read_data(bs_rw, *out_slice, *out_slice_length);
} 

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
	FILE* file_output = NULL;
	if(2 == argc){
		file = fopen(argv[1], "rb");
		file_output = fopen("/home/ubuntu/gpac/applications/tiles/output_film.hvc", "wb");
                if(file != NULL){
			//print_nalu(file);
			u64 length_no_use = 0;
			GF_BitStream *bs = gf_bs_from_file(file, GF_BITSTREAM_READ);; //==The whole bitstream
			GF_BitStream *bs_swap = gf_bs_new(NULL, length_no_use, GF_BITSTREAM_WRITE);
			char *buffer = NULL; //==pointer to hold chunk of NAL data
			char *buffer_swap;
			u32 nal_start_code;
			u32 nal_length;
			u32 nal_length_swap;
			int is_nal = 0;
			int slice_address[4] = {0, 30, 1020, 1050};
			
			if (bs != NULL){
				HEVCState hevc;
				u8 nal_unit_type =1, temporal_id, layer_id;
				int nal_num=0;
				while(gf_bs_get_size(bs)!= gf_bs_get_position(bs))
				{
					nal_length = gf_media_nalu_next_start_code_bs(bs);
					gf_bs_skip_bytes(bs, nal_length);
					nal_start_code = gf_bs_read_int(bs, 24);
					if(0x000001 == nal_start_code){
						is_nal = 1;
						gf_bs_write_int(bs_swap, 0x000001, 24);
					}
					else{
						if(0x000000 == nal_start_code && 1==gf_bs_read_int(bs, 8)){
							is_nal = 1;
							gf_bs_write_int(bs_swap, 0x00000001, 32);
						}
						
						else{
							is_nal = 0;
						}
					}

					if(is_nal)
					{
						nal_length = gf_media_nalu_next_start_code_bs(bs);
						//printf("\n\n");
						if(0==nal_length)
							nal_length = gf_bs_get_size(bs) - gf_bs_get_position(bs);
						buffer = malloc(sizeof(char)*nal_length);
						gf_bs_read_data(bs,buffer,nal_length);
						GF_BitStream *bs_tmp = gf_bs_new(buffer, nal_length+1, GF_BITSTREAM_READ);
						gf_media_hevc_parse_nalu(bs_tmp, &hevc, &nal_unit_type, &temporal_id, &layer_id);
						nal_num++;	
						//printf("nal_number: \t%d\t%d\n ",nal_num,nal_unit_type);
						switch (nal_unit_type){
							case 32:
								printVPS(buffer, nal_length, &hevc);
								gf_bs_write_data(bs_swap, buffer, nal_length);
								break;
							case 33:
								printSPS(buffer, nal_length, &hevc);
								gf_bs_write_data(bs_swap, buffer, nal_length);
								break;
							case 34:
								printPPS(buffer, nal_length, &hevc);
								gf_bs_write_data(bs_swap, buffer, nal_length);
								break;
							default:
								//printf("===Slice===\n");
								//printf("Slice adresse\t:%d\n",hevc.s_info.slice_segment_address);
								//printf("first_slice_segment_in_pic_flag\t:%d\n",hevc.s_info.first_slice_segment_in_pic_flag);
								//printf("frame_num\t:%d\n",hevc.s_info.frame_num);
								if (hevc.s_info.slice_segment_address == slice_address[0])
								{
									rewrite_slice_address(slice_address[1], buffer, nal_length, &buffer_swap, &nal_length_swap, &hevc);
									gf_bs_write_data(bs_swap, buffer_swap, nal_length_swap);
									//printf("I/O nal length: %d %d\n", nal_length, nal_length_swap);
									free(buffer_swap);
								}	
								else if (hevc.s_info.slice_segment_address == slice_address[1])
								{					
									rewrite_slice_address(slice_address[0], buffer, nal_length, &buffer_swap, &nal_length_swap, &hevc);
									gf_bs_write_data(bs_swap, buffer_swap, nal_length_swap);
									free(buffer_swap);
								}
								else //no swap
								{
									gf_bs_write_data(bs_swap, buffer, nal_length);
								}
								//gf_bs_write_data(bs_swap, buffer, nal_length);
								//printf("input pos: %d\n", gf_bs_get_position(bs)*8+gf_bs_get_bit_position(bs));
								//printf("output pos: %d\n", gf_bs_get_position(bs_swap)*8+gf_bs_get_bit_position(bs_swap));	
								break;
							//default:
								//printf("nal_unit_type not managed\n\n");
								//gf_bs_write_data(bs_swap, buffer, nal_length);
								//break;

						}
						//free(buffer);
						//free(*buffer_swap);
						
					}
					else{
						printf("6\n");
						printf("==%lu==\n",(gf_bs_get_size(bs) - gf_bs_get_position(bs)));
						printf("s==%lu==\n",gf_bs_get_size(bs));
						printf("p==%lu==\n", gf_bs_get_position(bs));
						gf_bs_skip_bytes(bs, gf_bs_get_size(bs) - gf_bs_get_position(bs));	
					}
				}
				printf("input bs size: %d\n", gf_bs_get_size(bs));
				printf("output bs size: %d\n", gf_bs_get_size(bs_swap));
				gf_bs_seek(bs_swap, 0);				
				buffer = malloc (gf_bs_get_size(bs_swap)*8);
				gf_bs_read_data(bs_swap, buffer, gf_bs_get_size(bs_swap));

				fwrite(buffer, gf_bs_get_size(bs), 1, file_output);
				//fputs(buffer, file_output);
				fclose(file_output);
			}

                }
	}
	return 0;	
}


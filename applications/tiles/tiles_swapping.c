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

/*u32 get_slice_segment_header_length(GF_BitStream *bs, HEVCState *hevc)
{
	HEVCSliceInfo si;
	u32 header_length = hevc_parse_slice_segment(bs, hevc, &si)
	
}*/



void rewrite_slice_address(u32 new_address, char *in_slice, u32 in_slice_length, char **out_slice, u32 *out_slice_length, HEVCState* hevc) 
{
	GF_BitStream *bs_ori = gf_bs_new(in_slice, in_slice_length, GF_BITSTREAM_READ);
	GF_BitStream *bs_rw = gf_bs_new(NULL, 0, GF_BITSTREAM_WRITE);
	u32 first_slice_segment_in_pic_flag;
	u32 dependent_slice_segment_flag;
	int address_ori;

	gf_bs_skip_bytes(bs_ori, 2);
	HEVCSliceInfo si;
	hevc_parse_slice_segment(bs_ori, hevc, &si);
	u32 header_end = (gf_bs_get_position(bs_ori)-1)*8+gf_bs_get_bit_position(bs_ori);

	gf_bs_seek(bs_ori, 0);

	//nal_unit_header			 
	gf_bs_write_int(bs_rw, gf_bs_read_int(bs_ori, 1), 1);
	u32 nal_unit_type = gf_bs_read_int(bs_ori, 6);
	gf_bs_write_int(bs_rw, nal_unit_type, 6);
	gf_bs_write_int(bs_rw, gf_bs_read_int(bs_ori, 9), 9);
	

	first_slice_segment_in_pic_flag = gf_bs_read_int(bs_ori, 1);    //first_slice_segment_in_pic_flag
	if (new_address == 0)  						
		gf_bs_write_int(bs_rw, 1, 1);
	else
		gf_bs_write_int(bs_rw, 0, 1);
	

	if (nal_unit_type >= 16 && nal_unit_type <= 23)                 //no_output_of_prior_pics_flag  
		gf_bs_write_int(bs_rw, gf_bs_read_int(bs_ori, 1), 1);
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
	else
	{
		dependent_slice_segment_flag = GF_FALSE;
	}
	if (!first_slice_segment_in_pic_flag) 						    //slice_segment_address READ
	{
		address_ori = gf_bs_read_int(bs_ori, sps->bitsSliceSegmentAddress);
	}
	else; 	//original slice segment address = 0

	if (new_address != 0) //new address != 0  						    
	{
		if (pps->dependent_slice_segments_enabled_flag)				    //dependent_slice_segment_flag WRITE
		{
			gf_bs_write_int(bs_rw, 0, 1);
		}						    
		gf_bs_write_int(bs_rw, new_address, sps->bitsSliceSegmentAddress);	    //slice_segment_address WRITE
	}
	else; //new_address = 0
	
	printf("ori_pos:%llu   rw_pos:%llu\n", gf_bs_get_position(bs_ori)*8+gf_bs_get_bit_position(bs_ori), gf_bs_get_position(bs_rw)*8+gf_bs_get_bit_position(bs_rw));
	

	while (header_end != (gf_bs_get_position(bs_ori)-1)*8+gf_bs_get_bit_position(bs_ori)) //Copy till the end of the header
	{
		gf_bs_write_int(bs_rw, gf_bs_read_int(bs_ori, 1), 1);
	}	
	//if (nal_unit_type == 1)
	//	printf("address pos:%llu\n", gf_bs_get_position(bs_rw)*8+gf_bs_get_bit_position(bs_rw));
	gf_bs_align(bs_rw);					//align
	gf_bs_align(bs_ori);     				//skip the padding 0s in original bitstream
	//printf("in_type:%d add:%d hd:%d\n",nal_unit_type,new_address,header_end);

	while (gf_bs_get_size(bs_ori)*8 != (gf_bs_get_position(bs_ori)-1)*8+gf_bs_get_bit_position(bs_ori)) //Rest contents copying
	{
		//printf("size: %llu %llu\n", gf_bs_get_size(bs_ori)*8, (gf_bs_get_position(bs_ori)-1)*8+gf_bs_get_bit_position(bs_ori));
		gf_bs_write_int(bs_rw, gf_bs_read_int(bs_ori, 1), 1);
	}
	
	

	//gf_bs_align(bs_rw);						//align
	gf_bs_truncate(bs_rw);
	//printf("input bs size: %llu\n", gf_bs_get_size(bs_ori));
	//printf("output bs size: %llu\n", gf_bs_get_size(bs_rw));
	//printf("input pos:%lld\n", (gf_bs_get_position(bs_ori)-1)*8+gf_bs_get_bit_position(bs_ori));
	//printf("output pos:%lld\n", (gf_bs_get_position(bs_rw)-1)*8+gf_bs_get_bit_position(bs_rw));
	*out_slice_length = 0;
	*out_slice = NULL;
	gf_bs_get_content(bs_rw, out_slice, out_slice_length);
	//printf("output buffer size: %d\n", *out_slice_length);

} 

void parse_and_print_VPS(char *buffer, u32 nal_length, HEVCState* hevc)
{
	int i = gf_media_hevc_read_vps(buffer, nal_length, hevc);
	printf("vps id:\t%d\n", i);
	printf("num_profile_tier_level:\t%d\n",(*hevc).vps[i].num_profile_tier_level);
}


void parse_and_print_SPS(char *buffer, u32 nal_length, HEVCState* hevc)
{	
	int i = gf_media_hevc_read_sps(buffer, nal_length, hevc);
	printf("sps id:\t%d\n", i);
       	printf("width:\t%d\n",(*hevc).sps[i].width);
	printf("height:\t%d\n",(*hevc).sps[i].height);
	printf("cw_flag:\t%d\n",(*hevc).sps[i].cw_flag);
	printf("max_CU_width:\t%d\n",(*hevc).sps[i].max_CU_width);
	printf("max_CU_height:\t%d\n",(*hevc).sps[i].max_CU_height);
	printf("sar_width:\t%d\n",(*hevc).sps[i].sar_width);
	printf("sar_height:\t%d\n",(*hevc).sps[i].sar_height);
}

void parse_and_print_PPS(char *buffer, u32 nal_length, HEVCState* hevc, int *tile_num)
{	
	int i = gf_media_hevc_read_pps(buffer, nal_length, hevc);
 	printf("pps id:\t%d\n", i);
    	printf("column_width:\t%d\n",(*hevc).pps[i].column_width[i]);
     	printf("uniform_spacing_flag:\t%d\n",(*hevc).pps[i].uniform_spacing_flag);
     	printf("dependent_slice_segments_enabled_flag:\t%d\n",(*hevc).pps[i].dependent_slice_segments_enabled_flag);
	printf("num_tile_columns:\t%d\n",(*hevc).pps[i].num_tile_columns);
	printf("num_tile_rows:\t%d\n",(*hevc).pps[i].num_tile_rows);
	printf("num_profile_tier_level:\t%d\n",(*hevc).vps[i].num_profile_tier_level);
	printf("height:\t%d\n",(*hevc).sps[i].height);
	*tile_num = (*hevc).pps[i].num_tile_columns * (*hevc).pps[i].num_tile_rows;
}

u64 size_of_file(FILE *file)
{
	long cur = ftell(file);
	fseek(file, 0L, SEEK_END);
	u64 size = (u64) ftell(file);
	fseek(file, cur, SEEK_SET);
	return size;
}

static u32 hevc_get_tile_id(HEVCState *hevc, u32 *tile_x, u32 *tile_y, u32 *tile_width, u32 *tile_height)
{
	HEVCSliceInfo *si = &hevc->s_info;
	u32 i, tbX, tbY, PicWidthInCtbsY, PicHeightInCtbsY, tileX, tileY, oX, oY, val;

	PicWidthInCtbsY = si->sps->width / si->sps->max_CU_width;
	if (PicWidthInCtbsY * si->sps->max_CU_width < si->sps->width) PicWidthInCtbsY++;
	PicHeightInCtbsY = si->sps->height / si->sps->max_CU_width;
	if (PicHeightInCtbsY * si->sps->max_CU_width < si->sps->height) PicHeightInCtbsY++;

	tbX = si->slice_segment_address % PicWidthInCtbsY;
	tbY = si->slice_segment_address / PicWidthInCtbsY;

	tileX = tileY = 0;
	oX = oY = 0;
	for (i=0; i < si->pps->num_tile_columns; i++) {
		if (si->pps->uniform_spacing_flag) {
			val = (i+1)*PicWidthInCtbsY / si->pps->num_tile_columns - (i)*PicWidthInCtbsY / si->pps->num_tile_columns;
		} else {
			if (i<si->pps->num_tile_columns-1) {
				val = si->pps->column_width[i];
			} else {
				val = (PicWidthInCtbsY - si->pps->column_width[i-1]);
			}
		}
		*tile_x = oX;
		*tile_width = val;

		if (oX >= tbX) break;
		oX += val;
		tileX++;
	}
	for (i=0; i<si->pps->num_tile_rows; i++) {
		if (si->pps->uniform_spacing_flag) {
			val = (i+1)*PicHeightInCtbsY / si->pps->num_tile_rows - (i)*PicHeightInCtbsY / si->pps->num_tile_rows;
		} else {
			if (i<si->pps->num_tile_rows-1) {
				val = si->pps->row_height[i];
			} else {
				val = (PicHeightInCtbsY - si->pps->row_height[i-1]);
			}
		}
		*tile_y = oY;
		*tile_height = val;

		if (oY >= tbY) break;
		oY += val;
		tileY++;
	}
	*tile_x = *tile_x * si->sps->max_CU_width;
	*tile_y = *tile_y * si->sps->max_CU_width;
	*tile_width = *tile_width * si->sps->max_CU_width;
	*tile_height = *tile_height * si->sps->max_CU_width;

	if (*tile_x + *tile_width > si->sps->width)
		*tile_width = si->sps->width - *tile_x;
	if (*tile_y + *tile_height > si->sps->height)
		*tile_height = si->sps->height - *tile_y;

	return tileX + tileY * si->pps->num_tile_columns;
}

void slice_address_calculation(HEVCState *hevc, u32 *address, u32 tile_x, u32 tile_y)
{
	HEVCSliceInfo *si = &hevc->s_info;
	u32 PicWidthInCtbsY, PicHeightInCtbsY;
	
	PicWidthInCtbsY = si->sps->width / si->sps->max_CU_width;
	if (PicWidthInCtbsY * si->sps->max_CU_width < si->sps->width) PicWidthInCtbsY++;

	*address = tile_x / si->sps->max_CU_width + (tile_y / si->sps->max_CU_width) * PicWidthInCtbsY;
}

int main (int argc, char **argv)
{
	printf("===begin====\n");
	FILE* file = NULL;
	FILE* file_output = NULL;
	int tile_1 = atoi(argv[2])-1;
	int tile_2 = atoi(argv[3])-1;
	if(5 == argc)
	{
		file = fopen(argv[1], "rb");
		file_output = fopen(argv[4], "wb");
                if(file != NULL)
		{
			GF_BitStream *bs = gf_bs_from_file(file, GF_BITSTREAM_READ);; //==The whole bitstream
			
			if (bs != NULL)
			{
				GF_BitStream *bs_swap = gf_bs_from_file(file_output, GF_BITSTREAM_WRITE);
				HEVCState hevc;
				u8 nal_unit_type, temporal_id, layer_id;
				int nal_num=0;
				int slice_num=0;
				int first_slice_num=0;
				int slice_info_scan_finish = 0;
				char *buffer_reorder[100];
				u32 buffer_reorder_length[100];
				u32 buffer_reorder_sc[100][2];
				char *buffer = NULL; //==pointer to hold chunk of NAL data
				char *buffer_swap;
				u32 nal_start_code;
				int nal_start_code_length;
				u32 nal_length;
				u32 nal_length_swap;
				int is_nal = 0;
				int vps_num = 0;
				int sps_num = 0;
				int pps_num = 0;
				int tile_num = 0;
				int slice_address[100] = {0};
				int pos_rec = 0;
				int tile_info_check = 0;
				int tiles_width[100] = {0};
				int tiles_height[100] = {0};
				u32 tile_x, tile_y, tile_width, tile_height;
				
				while(gf_bs_get_size(bs) != gf_bs_get_position(bs) && tile_info_check == 0)
				{
					nal_length = gf_media_nalu_next_start_code_bs(bs);
					gf_bs_skip_bytes(bs, nal_length);
					nal_start_code = gf_bs_read_int(bs, 24);
					if(0x000001 == nal_start_code)
					{
						is_nal = 1;
						nal_start_code_length = 24;
					}
					else
					{
						if(0x000000 == nal_start_code && 1==gf_bs_read_int(bs, 8))
						{
							is_nal = 1;
							nal_start_code = 0x00000001;
							nal_start_code_length = 32;
						}
						
						else
						{
							is_nal = 0;
						}
					}

					if(is_nal)
					{
						nal_length = gf_media_nalu_next_start_code_bs(bs);
						if(nal_length == 0)
						{
							nal_length = gf_bs_get_size(bs) - gf_bs_get_position(bs);
						}
						buffer = malloc(sizeof(char)*nal_length);
						gf_bs_read_data(bs,buffer,nal_length);
						GF_BitStream *bs_tmp = gf_bs_new(buffer, nal_length, GF_BITSTREAM_READ);
						//printf("size bs buf bstmp:%d %lld %lld\n",sizeof(char)*nal_length, gf_bs_get_position(bs), gf_bs_get_size(bs_tmp));
						gf_media_hevc_parse_nalu(bs_tmp, &hevc, &nal_unit_type, &temporal_id, &layer_id);
						nal_num++;
						//printf("%d ", nal_unit_type);	
						switch (nal_unit_type)
						{
							case 32:
								vps_num++;
								printf("===VPS #%d===\n", vps_num);
								parse_and_print_VPS(buffer, nal_length, &hevc);
								gf_bs_write_int(bs_swap, nal_start_code, nal_start_code_length);
								gf_bs_write_data(bs_swap, buffer, nal_length);
								if (pos_rec < gf_bs_get_position(bs))
									pos_rec = gf_bs_get_position(bs);
								break;
							case 33:
								sps_num++;
								printf("===SPS #%d===\n", sps_num);
								parse_and_print_SPS(buffer, nal_length, &hevc);
								gf_bs_write_int(bs_swap, nal_start_code, nal_start_code_length);
								gf_bs_write_data(bs_swap, buffer, nal_length);
								if (pos_rec < gf_bs_get_position(bs))
									pos_rec = gf_bs_get_position(bs);
								break;
							case 34:
								pps_num++;
								printf("===PPS #%d===\n", pps_num);
								parse_and_print_PPS(buffer, nal_length, &hevc, &tile_num);
								gf_bs_write_int(bs_swap, nal_start_code, nal_start_code_length);
								gf_bs_write_data(bs_swap, buffer, nal_length);
								if (tile_1+1 > tile_num || tile_2+1 > tile_num)
								{
									tile_info_check = 1;
								}
								if (pos_rec < gf_bs_get_position(bs))
									pos_rec = gf_bs_get_position(bs);
								break;
							default:
								if (nal_unit_type <= 21)  //Slice
								{
									//printf("===Slice===\n");
									//printf("Slice adresse\t:%d\n",hevc.s_info.slice_segment_address);
									//printf("first_slice_segment_in_pic_flag\t:%d\n",hevc.s_info.first_slice_segment_in_pic_flag);
									//printf("frame_num\t:%d\n",hevc.s_info.frame_num);

									int tile_id = hevc_get_tile_id(&hevc, &tile_x, &tile_y, &tile_width, &tile_height);
									//printf("x,y,w,h: %d %d %d %d\n", tile_x, tile_y, tile_width, tile_height);
									tiles_width[tile_id] = tile_width;
									tiles_height[tile_id] = tile_height;

									if (!slice_info_scan_finish)
									{
										slice_address_calculation(&hevc, &slice_address[tile_id], tile_x, tile_y);
										if (slice_address[tile_id] != hevc.s_info.slice_segment_address)
											printf("Slice address calculation wrong!\n");
										if (tile_id == tile_num-1)
										{
											slice_info_scan_finish = 1;	
											gf_bs_seek(bs, pos_rec);
											if (tiles_width[tile_1] != tiles_width[tile_2] || tiles_height[tile_1] != tiles_height[tile_2])
											{
												tile_info_check = 2;
											}		
										}
									}
									else
									{												
										if (slice_address[tile_id] == slice_address[tile_1])
										{
											buffer_reorder_sc[tile_2][0] = nal_start_code;
											buffer_reorder_sc[tile_2][1] = nal_start_code_length;
											rewrite_slice_address(slice_address[tile_2], buffer, nal_length, &buffer_swap, &nal_length_swap, &hevc);
											//printf("output bs size2: %d\n", nal_length_swap);
											//if(nal_length_swap!=nal_length) fprintf(stderr, "ERROR in size\n");
											buffer_reorder_length[tile_2] = sizeof(char)*nal_length_swap;										
											buffer_reorder[tile_2] = malloc(sizeof(char)*nal_length_swap);										
											memcpy(buffer_reorder[tile_2], buffer_swap, sizeof(char)*nal_length_swap);
											free(buffer_swap);
										}	
										else if (slice_address[tile_id] == slice_address[tile_2])
										{			
											buffer_reorder_sc[tile_1][0] = nal_start_code;
											buffer_reorder_sc[tile_1][1] = nal_start_code_length;		
											rewrite_slice_address(slice_address[tile_1], buffer, nal_length, &buffer_swap, &nal_length_swap, &hevc);
											//printf("output bs size2: %d\n", nal_length_swap);
											//if(nal_length_swap!=nal_length) fprintf(stderr, "ERROR in size\n");
											buffer_reorder_length[tile_1] = sizeof(char)*nal_length_swap;										
											buffer_reorder[tile_1] = malloc(sizeof(char)*nal_length_swap);										
											memcpy(buffer_reorder[tile_1], buffer_swap, sizeof(char)*nal_length_swap);
											free(buffer_swap);							
										}
										else //no swap
										{
											int slice_num;
											int i=0;
											for (i; i<tile_num; i++)
											{
												if (slice_address[tile_id] == slice_address[i])
													slice_num = i;
												else;
											}
											buffer_reorder_sc[slice_num][0] = nal_start_code;
											buffer_reorder_sc[slice_num][1] = nal_start_code_length;
											buffer_reorder_length[slice_num] = sizeof(char)*nal_length;
											buffer_reorder[slice_num] = malloc(sizeof(char)*nal_length);										
											memcpy(buffer_reorder[slice_num], buffer, sizeof(char)*nal_length);
										}

										if (tile_id == tile_num-1)  //re-ordering
										{
											int i=0;
											for (i; i<tile_num; i++)
											{
												gf_bs_write_int(bs_swap, buffer_reorder_sc[i][0], buffer_reorder_sc[i][1]);
												gf_bs_write_data(bs_swap, buffer_reorder[i], buffer_reorder_length[i]);
											}
										
											int j=0;										
											for (j; j<tile_num; j++)
											{
												free(buffer_reorder[j]);
											}
										}
										else;

										if (hevc.s_info.first_slice_segment_in_pic_flag)
											first_slice_num++;
										else;
										slice_num++;
										//printf("input pos: %d\n", gf_bs_get_position(bs)*8+gf_bs_get_bit_position(bs));
										//printf("output pos: %d\n", gf_bs_get_position(bs_swap)*8+gf_bs_get_bit_position(bs_swap));	
									}								
								} 
								else 		//Not slice
								{
									gf_bs_write_int(bs_swap, nal_start_code, nal_start_code_length);
									gf_bs_write_data(bs_swap, buffer, nal_length);
									if (pos_rec < gf_bs_get_position(bs) && !slice_info_scan_finish)
										pos_rec = gf_bs_get_position(bs);
								}
								break;
						}
						free(buffer);
						//free(*buffer_swap);
						
					}
					else;
				}
				printf("======Log======\n");
				if (tile_info_check == 0)
				{
					printf("Tile num: %d\n", tile_num);
					printf("Slice address:");
					int i = 0;			
					for (i; i<tile_num; i++)
					{
						printf(" %d", slice_address[i]);
					}
					printf("\n");
					printf("input bs size: %llu\n", gf_bs_get_size(bs));
					printf("output bs size: %llu\n", gf_bs_get_size(bs_swap));
					printf("slice number: %d\n", slice_num); 
					printf("first slice number: %d\n", first_slice_num); 
					printf("slice number/frame: %d\n", slice_num/first_slice_num);
					printf("Success!\n"); 
				}
				else if (tile_info_check == 1)
				{
					printf("Requested tile id doesn't exist!\n");
					if (tile_num == 0)
						printf("This file has no tile!\n");
					else
						printf("This file has only %d tile(s)\n", tile_num);
				}
				else if (tile_info_check == 2)
				{
					printf("Tile sizes are different, can't swap!\n");
					printf("Tile 1: %dx%d, Tile 2: %dx%d\n", tiles_width[tile_1], tiles_height[tile_1], tiles_width[tile_2], tiles_height[tile_2]);
				}
				else;

				fclose(file_output);
			}
			else
				printf("Bitstream reading fail!\n");
                }
		else
			printf("File reading fail!\n");
	}
	return 0;	
}

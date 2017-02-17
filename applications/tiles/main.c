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
#include <math.h>
#include "helper.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>


// Extract the tiles from a tiled video
void extract_tiles (int argc, char **argv)
{
    FILE* file = NULL;
    int i,j;
    if(3 <= argc)
    {
        file = fopen(argv[2],"rb");
        if(file != NULL)
        {
            GF_BitStream *bs;
            bs = gf_bs_from_file(file, GF_BITSTREAM_READ);
            if(bs != NULL)
            {
                char start_data; //Store the first before start code in the bitstream
                char *buffer_swap=NULL; //buffer for swap operation
                char *buffer =NULL; //buffer to store one NAL data
                char *VPS_buffer=NULL; //Store the VPS
                char *SPS_buffer=NULL; //Store the SPS
                char *PPS_buffer=NULL; //Store the PPS
                u32 start_length=0, buffer_swap_length=0, buffer_length=0, VPS_length=0, SPS_length=0, PPS_length=0; 
                u32 nal_start_code;
                int is_nal;
                u32 x, y, pps_id;
		HEVCState hevc;
		u8 nal_unit_type, temporal_id, layer_id;
                
                /**array of bitstream: Each one point to one tile bistream*/
		GF_BitStream *bs_tiles[10][10]; 
                /**array of file: Each one point to one file where one tile bistream will be saved*/
                FILE *files[10][10];
		while( gf_bs_get_size(bs)!= gf_bs_get_position(bs))
		{
                    start_length = gf_media_nalu_next_start_code_bs(bs);
                    gf_bs_read_data(bs,start_data,start_data);
                    //gf_bs_skip_bytes(bs, start_length);
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
			buffer_length = gf_media_nalu_next_start_code_bs(bs);
                        if(0==buffer_length)
                            buffer_length = gf_bs_get_size(bs) - gf_bs_get_position(bs);

			buffer = malloc(sizeof(char)*buffer_length);
			gf_bs_read_data(bs,buffer,buffer_length);
			GF_BitStream *bs_buffer = gf_bs_new(buffer, buffer_length, GF_BITSTREAM_READ);
			gf_media_hevc_parse_nalu(bs_buffer, &hevc, &nal_unit_type, &temporal_id, &layer_id);
                        gf_bs_del(bs_buffer);
                        switch(nal_unit_type){
                            case 32:
                                VPS_buffer = buffer;
                                VPS_length = buffer_length;
                                gf_media_hevc_read_vps(VPS_buffer,VPS_length, &hevc);
                                buffer = NULL;
                                break;
                            case 33:
                                SPS_buffer = buffer;
                                SPS_length = buffer_length;
                                gf_media_hevc_read_sps(SPS_buffer,SPS_length, &hevc);
                                buffer = NULL;
                                break;
                            case 34:
                                PPS_buffer = buffer;
                                PPS_length = buffer_length;
                                pps_id = gf_media_hevc_read_pps(PPS_buffer,PPS_length, &hevc);
                                buffer = NULL;
                                break;
                            case 21:
                            case 19:
                            case 18:
                            case 17:
                            case 16:
                            case 9:
                            case 8:
                            case 7:
                            case 6:
                            case 5:
                            case 4:
                            case 2:
                            case 0:
                            case 1:
                                if(hevc.s_info.pps->tiles_enabled_flag)
                                {
                                    //u32 l=0;
                                    get_2D_cordinates_of_slices(&x, &y, &hevc);
                                    printf("(x,y):\t(%u,%u)",x,y);
                                    if(bs_tiles[x][y]== NULL) // Check if it is the first slice of tile
                                    {
                                        char tile_name[100];
                                        /** argv[3] is the directory where the tiles will be saved*/
                                        sprintf(tile_name, "%s/tile_%u_%u_.hvc",argv[3], x, y);
                                        files[x][y] = fopen(tile_name,"wb");
                                        //create bitstream
                                        bs_tiles[x][y] = gf_bs_from_file(files[x][y],GF_BITSTREAM_WRITE);
                                        if(start_length)
                                            gf_bs_write_data(bs_tiles[x][y],start_data, start_length);
                                        //Copy the VPS
                                        gf_bs_write_int(bs_tiles[x][y], 1, 32);
                                        gf_bs_write_data(bs_tiles[x][y],VPS_buffer, VPS_length);
                                        gf_bs_write_int(bs_tiles[x][y], 1, 32);
                                        
                                        //rewrite and copy the SPS
                                        rewrite_SPS(SPS_buffer, SPS_length, &buffer_swap, &buffer_swap_length, &hevc);
                                        printf("\nSPS (in, out)=(%u,%u)\n", SPS_length,buffer_swap_length);
                                        gf_bs_write_data(bs_tiles[x][y],buffer_swap, buffer_swap_length);
                                        free(buffer_swap);
                                        buffer_swap = NULL;
                                        buffer_swap_length = 0;
                                        
                                        //rewrite and copy the PPS
                                        gf_bs_write_int(bs_tiles[x][y], 1, 32);
                                        rewrite_PPS(PPS_buffer,PPS_length, &buffer_swap, &buffer_swap_length);
                                        gf_bs_write_data(bs_tiles[x][y],buffer_swap, buffer_swap_length);
                                        free(buffer_swap);
                                        buffer_swap = NULL;
                                        //l += (start_length+VPS_length+SPS_length+buffer_swap_length+12);
                                        buffer_swap_length = 0;
                                        
                                        //gf_bs_write_data(bs_tiles[x][y],PPS_buffer, PPS_length);
                                    }
                                    
                                    //rewrite and copy the Slice
                                    gf_bs_write_int(bs_tiles[x][y], 1, 32);
                                    //rewrite_slice_address(0, buffer, buffer_length, &buffer_swap, &buffer_swap_length, &hevc);
                                    //gf_bs_write_data(bs_tiles[x][y], buffer_swap, buffer_swap_length);
                                    //free(buffer_swap);
                                    //buffer_swap = NULL;
                                    //buffer_swap_length = 0;
                                    gf_bs_write_data(bs_tiles[x][y], buffer, buffer_length);
                                    free(buffer);
                                    buffer = NULL;
                                    //l += (4+buffer_length);
                                    //printf("\tposition:(B,b)=(%lu,%u)\t%lu\t%u\n",gf_bs_get_position(bs_tiles[x][y]),gf_bs_get_bit_position(bs_tiles[x][y]),l,buffer_length);
                                    buffer_length = 0;
                               }
                                else{
                                    printf("\n Make sure you video is tiled. \n");
                                }
                                
                                break;
                            default:
                                for(i=0;i<hevc.pps[pps_id].num_tile_columns;i++)
                                {
                                    for(j=0;j<hevc.pps[pps_id].num_tile_rows;j++)
                                    {
                                        if(bs_tiles[i][j])
                                        {
                                            gf_bs_write_int(bs_tiles[i][j], 1, 32);
                                            gf_bs_write_data(bs_tiles[i][j], buffer, buffer_length);
                                            buffer = NULL;
                                            buffer_length = 0;
                                        }
                                    }
                                }
                                break;
			}
                    }else{
                        printf("\n Your file is corrupted. \n");
                        goto exit;
                    }
                }
                for(i=0;i<hevc.s_info.pps->num_tile_columns;i++)
                {
                    for(j=0;j<hevc.s_info.pps->num_tile_rows;j++)
                    {
                        gf_bs_del(bs_tiles[i][j]);
                        fclose(files[i][j]);
                    }
                }		
            }else{
                printf("\n Sorry try again: our program failed to open the bitsream. \n");
                goto exit;
            }
            //fclose(file);
        }else
        {
            printf("\n Make sure you file path is correct. \n");
            goto exit;
        }
        
    }
    exit:
        fclose(file);           
}

void bs_info(int argc, char **argv)
{
    FILE* file = NULL;
    if(3 <= argc)
    {
        file = fopen(argv[2],"rb");
        if(file != NULL)
        {
            GF_BitStream *bs;
            bs = gf_bs_from_file(file, GF_BITSTREAM_READ);
            if(bs != NULL)
            {
                char *buffer =NULL;
                u32 buffer_length; 
                u32 nal_start_code;
                int is_nal;
                u32 x, y;
		HEVCState hevc;
		u8 nal_unit_type, temporal_id, layer_id;
		int nal_num=0;
		while( gf_bs_get_size(bs)!= gf_bs_get_position(bs))
		{
                    buffer_length = gf_media_nalu_next_start_code_bs(bs);
                    gf_bs_skip_bytes(bs, buffer_length);
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
			buffer_length = gf_media_nalu_next_start_code_bs(bs);
                        if(0==buffer_length)
                            buffer_length = gf_bs_get_size(bs) - gf_bs_get_position(bs);

			buffer = malloc(sizeof(char)*buffer_length);
			gf_bs_read_data(bs,buffer,buffer_length);
			GF_BitStream *bs_buffer = gf_bs_new(buffer, buffer_length, GF_BITSTREAM_READ);
			gf_media_hevc_parse_nalu(bs_buffer, &hevc, &nal_unit_type, &temporal_id, &layer_id);
                        gf_bs_del(bs_buffer);
			nal_num++;
                        switch(nal_unit_type){
                            case 32:
                                parse_print_VPS(buffer,buffer_length, &hevc);
                                break;
                            case 33:
                                parse_print_SPS(buffer,buffer_length, &hevc);
                                break;
                            case 34:
                                parse_print_PPS(buffer,buffer_length, &hevc);
                                break;
                            case 21:
                            case 19:
                            case 18:
                            case 17:
                            case 16:
                            case 9:
                            case 8:
                            case 7:
                            case 6:
                            case 5:
                            case 4:
                            case 2:
                            case 0:
                            case 1:
                                get_size_of_slice(&x, &y, &hevc);
                                printf("\n slice: (width,height) = (%u,%u) \t adress: %u", x, y, hevc.s_info.slice_segment_address);
                                get_2D_cordinates_of_slices(&x, &y, &hevc);printf("\n slice: (wx,hy) = (%u,%u) \t adress: %u", x, y, hevc.s_info.slice_segment_address);
                                break;
                            default:
                                break;
			}
                    }
                    else{
                        printf("\n Your file is corrupted. \n");
                        goto exit;
                    }
                }
                printf("\n\nnumber of NAL:\t %d",nal_num);
            }else{
                printf("\n Sorry try again: our program failed to open the bitsream. \n");
                goto exit;
            }
            //fclose(file);
        }else
        {
            printf("\n Make sure you file path is correct. \n");
            goto exit;
        }
    }
    exit:
        fclose(file);                        
}

void test()
{
    char data[12];
    char *dest;
    sprintf(data,"tester");
    GF_BitStream *bs_in = gf_bs_new(data, 6, GF_BITSTREAM_READ);
    GF_BitStream *bs_out = gf_bs_new(NULL, 0, GF_BITSTREAM_WRITE);
    printf("size:  %lu\n\n",gf_bs_get_size(bs_in));
    printf("r_bit_pos:  %u\t",gf_bs_get_bit_position(bs_in));
    printf("w_bit_pos:  %u\t",gf_bs_get_bit_position(bs_out));
    printf("r_byte_pos:  %lu\t",gf_bs_get_position(bs_in));
    printf("w_byte_pos:  %lu\t",gf_bs_get_position(bs_out));
    gf_bs_write_int(bs_out,gf_bs_read_int(bs_in,1),1);
    int i= 0;
    for(i=0;i<20;i++){
        printf("\n");
        printf("r_bit_pos:  %u\t",gf_bs_get_bit_position(bs_in));
        printf("w_bit_pos:  %u\t",gf_bs_get_bit_position(bs_out));
        printf("r_byte_pos:  %lu\t",gf_bs_get_position(bs_in));
        printf("w_byte_pos:  %lu\t",gf_bs_get_position(bs_out));
        gf_bs_write_int(bs_out,gf_bs_read_int(bs_in,1),1);
    }
}
int main (int argc, char **argv)
{
    if(argc >= 2)
    {
        if(0==strcmp(argv[1],"--extract") || 0==strcmp(argv[1],"-x"))
        {
            struct stat st = {0};
            if (stat(argv[3], &st) == -1)
                mkdir(argv[3], 0777);
            extract_tiles(argc, argv);
        }  
        else if(0==strcmp(argv[1],"--info") || 0==strcmp(argv[1],"-i"))
            bs_info(argc, argv);
        else
        {
            printf(" \tThis commands allows to extract the for a given tiled bitstream\n");
            printf(" \tType ./TILES -x filename dir_where_to_save_the_extract_tiles\n");
            printf(" \tType ./TILES --extract filename dir_where_to_save_the_extract_tiles\n");
            
            printf(" \tThis commands allows to get info from some bitstream \n");
            printf(" \tType ./TILES -i filename\n");
            printf(" \tType ./TILES --info filename \n");
        }
    }
    else
        {
            printf(" \tThis commands allows to extract the for a given tiled bitstream\n");
            printf(" \tType ./TILES -x filename dir_where_to_save_the_extract_tiles\n");
            printf(" \tType ./TILES --extract filename dir_where_to_save_the_extract_tiles\n");
            
            printf(" \tThis commands allows to get info from some bitstream \n");
            printf(" \tType ./TILES -i filename\n");
            printf(" \tType ./TILES --info filename \n");
        }
    //test();
    return 0;
}

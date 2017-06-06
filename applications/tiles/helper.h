/*
 * To change this license header, choose License Headers in Project Properties.
 * To change this template file, choose Tools | Templates
 * and open the template in the editor.
 */

/* 
 * File:   helper.h
 * Author: dadja
 *
 * Created on January 31, 2017, 2:00 AM
 */

#ifndef HELPER_H
#define HELPER_H

#ifdef __cplusplus
extern "C" {
#endif
    
typedef struct {
    u32 x;
    u32 y;
} Point;

u32 parse_print_VPS(char *buffer, u32 bz, HEVCState* hevc);
u32 parse_print_SPS(char *buffer, u32 bz, HEVCState* hevc);
u32 parse_print_PPS(char *buffer, u32 bz, HEVCState* hevc);

u64 size_of_file(FILE *file);
    

void bs_set_ue(GF_BitStream *bs,u32 num);
void bs_set_se(GF_BitStream *bs,s32 num);

u32 new_address(int x,int y,int num_CTU_height[],int num_CTU_width[],int num_CTU_width_tot);
void get_2D_cordinates_of_slices(u32 *x, u32 *y, HEVCState *hevc);
void get_size_of_slice(u32 *w, u32 *h, HEVCState *hevc);

//void rewrite_VPS(char *in_VPS, u32 in_VPS_length, char **out_VPS, u32 *out_VPS_length, HEVCState* hevc);
void rewrite_SPS(char *in_SPS, u32 in_SPS_length, char **out_SPS, u32 *out_SPS_length, HEVCState *hevc);
void rewrite_PPS(char *in_PPS, u32 in_PPS_length, char **out_PPS, u32 *out_PPS_length);
void rewrite_SPS_cb(u32 pic_width, u32 pic_height,char *in_SPS, u32 in_SPS_length, char **out_SPS, u32 *out_SPS_length, HEVCState *hevc);
void rewrite_PPS_cb(u32 num_tile_columns_minus1,u32 num_tile_rows_minus1,u32 uniform_spacing_flag,u32 column_width_minus1[],u32 row_height_minus1[],char *in_PPS, u32 in_PPS_length, char **out_PPS, u32 *out_PPS_length);
void rewrite_slice_address(u32 new_address, char *in_slice, u32 in_slice_length, char **out_slice, u32 *out_slice_length, HEVCState* hevc);


#ifdef __cplusplus
}
#endif

#endif /* HELPER_H */


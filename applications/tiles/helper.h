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

void get_2D_cordinates_of_slices(u32 *x, u32 *y, HEVCState *hevc);
void get_size_of_slice(u32 *w, u32 *h, HEVCState *hevc);

//void rewrite_VPS(char *in_VPS, u32 in_VPS_length, char **out_VPS, u32 *out_VPS_length, HEVCState* hevc);
void rewrite_SPS(char *in_SPS, u32 in_SPS_length, char **out_SPS, u32 *out_SPS_length, HEVCState *hevc);
void rewrite_PPS(char *in_PPS, u32 in_PPS_length, char **out_PPS, u32 *out_PPS_length);


#ifdef __cplusplus
}
#endif

#endif /* HELPER_H */


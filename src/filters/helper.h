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

#include <gpac/constants.h>
#include <gpac/internal/media_dev.h>

	typedef struct {
		u32 x;
		u32 y;
	} Point;

	u32 parse_print_VPS(char *buffer, u32 bz, HEVCState* hevc);
	u32 parse_print_SPS(char *buffer, u32 bz, HEVCState* hevc);
	u32 parse_print_PPS(char *buffer, u32 bz, HEVCState* hevc);

	void bs_set_ue(GF_BitStream *bs, u32 num);
	void bs_set_se(GF_BitStream *bs, s32 num);

	u32 new_address(int x, int y, u32 num_CTU_height[], u32 num_CTU_width[], int num_CTU_width_tot, u32 sliceAdresse);
	u32 hevc_get_tile_id(HEVCState *hevc, u32 *tile_x, u32 *tile_y, u32 *tile_width, u32 *tile_height);
	void slice_address_calculation(HEVCState *hevc, u32 *address, u32 tile_x, u32 tile_y);
	u32 get_newSliceAddress_and_tilesCordinates(u32 *x, u32 *y, HEVCState *hevc);
	void get_size_of_tile(HEVCState hevc, u32 index_row, u32 index_col, u32 pps_id, u32 *width, u32 *height, u32 *tx, u32 *ty);

	void write_profile_tier_level(GF_BitStream *bs_in, GF_BitStream *bs_out, Bool ProfilePresentFlag, u8 MaxNumSubLayersMinus1);

	//void rewrite_VPS(char *in_VPS, u32 in_VPS_length, char **out_VPS, u32 *out_VPS_length, HEVCState* hevc);
	void rewrite_SPS(char *in_SPS, u32 in_SPS_length, u32 width, u32 height, HEVCState *hevc, char **out_SPS, u32 *out_SPS_length);
	void rewrite_PPS(Bool extract, char *in_PPS, u32 in_PPS_length, char **out_PPS, u32 *out_PPS_length, u32 num_tile_columns_minus1, u32 num_tile_rows_minus1, u32 uniform_spacing_flag, u32 column_width_minus1[], u32 row_height_minus1[]);
	void rewrite_slice_address(s32 new_address, char *in_slice, u32 in_slice_length, char **out_slice, u32 *out_slice_length, HEVCState* hevc, u32 final_width, u32 final_height);

#ifdef __cplusplus
}
#endif

#endif /* HELPER_H */




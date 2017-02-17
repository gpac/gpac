
#include <stdlib.h>
#include <stdio.h>
#include <gpac/bitstream.h>
#include <gpac/internal/media_dev.h>
#include <math.h>
#include "helper.h"

/*functions for the visualization of non-VCL NAL*/
u32 parse_print_VPS(char *buffer, u32 bz, HEVCState* hevc){
	u32 i = gf_media_hevc_read_vps(buffer,bz, hevc);
	printf("=== Visualization of VPS with id: %d ===\n",(*hevc).vps[i].id);
	printf("num_profile_tier_level:	%hhu\n",(*hevc).vps[i].num_profile_tier_level);
        return i;
}

u32 parse_print_SPS(char *buffer, u32 bz, HEVCState* hevc){
	u32 i = gf_media_hevc_read_sps(buffer,bz, hevc);
	printf("=== Visualization of SPS with id: %d ===\n",(*hevc).sps[i].id);
       	printf("width:	%u\n",(*hevc).sps[i].width);
	printf("height:	%u\n",(*hevc).sps[i].height);
        printf("pic_width_in_luma_samples:	%u\n",(*hevc).sps[i].max_CU_width);
        printf("pic_heigth_in_luma_samples:	%u\n",(*hevc).sps[i].max_CU_height);
        printf("cw_left:	%u\n",(*hevc).sps[i].cw_left);
        printf("cw_right:	%u\n",(*hevc).sps[i].cw_right);
        printf("cw_top:	%u\n",(*hevc).sps[i].cw_top);
        printf("cw_bottom:	%u\n",(*hevc).sps[i].cw_bottom);
        return i;
}

u32 parse_print_PPS(char *buffer, u32 bz, HEVCState* hevc){
	u32 i = gf_media_hevc_read_pps(buffer,bz, hevc);
	printf("=== Visualization of PPS with id: %d ===\n",(*hevc).pps[i].id);
     	printf("tiles_enabled_flag:	%u\n",(*hevc).pps[i].tiles_enabled_flag);
     	printf("uniform_spacing_flag:	%d\n",(*hevc).pps[i].uniform_spacing_flag);
	printf("num_tile_columns:	%u\n",(*hevc).pps[i].num_tile_columns);
	printf("num_tile_rows:	%u\n",(*hevc).pps[i].num_tile_rows);
        return i;
}

u64 size_of_file(FILE *file)
{
	long cur = ftell(file);
	fseek(file, 0L, SEEK_END);
	u64 size = (u64) ftell(file);
	fseek(file, cur, SEEK_SET);
	return size;
}

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

static u32 avc_remove_emulation_bytes(const char *buffer_src, char *buffer_dst, u32 nal_size)
{
	u32 i = 0, emulation_bytes_count = 0;
	u8 num_zero = 0;

	while (i < nal_size)
	{
		/*ISO 14496-10: "Within the NAL unit, any four-byte sequence that starts with 0x000003
		  other than the following sequences shall not occur at any byte-aligned position:
		  0x00000300
		  0x00000301
		  0x00000302
		  0x00000303"
		*/
		if (num_zero == 2
		        && buffer_src[i] == 0x03
		        && i+1 < nal_size /*next byte is readable*/
		        && buffer_src[i+1] < 0x04)
		{
			/*emulation code found*/
			num_zero = 0;
			emulation_bytes_count++;
			i++;
		}

		buffer_dst[i-emulation_bytes_count] = buffer_src[i];

		if (!buffer_src[i])
			num_zero++;
		else
			num_zero = 0;

		i++;
	}

	return nal_size-emulation_bytes_count;
}

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

void bs_set_ue(GF_BitStream *bs, u32 num) {
    s32 length = 1;
    s32 temp = ++num;

    while (temp != 1) {
        temp >>= 1;
        length += 2;
    }
    
    gf_bs_write_int(bs, 0, length >> 1);
    gf_bs_write_int(bs, num, (length+1) >> 1);
}

void bs_set_se(GF_BitStream *bs,s32 num)
{	u32 v;
	if(num <=0)
		v = (-1*num)<<1;
	else
		v = (num << 1) - 1;

	bs_set_ue(bs,v);
}

//Transform slice 1d address into 2D address
void get_2D_cordinates_of_slices(u32 *x, u32 *y, HEVCState *hevc)
{
    *x = 0;
    *y = 0;
    u32 number_of_CTU_per_width = (hevc->s_info.sps->width + hevc->s_info.sps->max_CU_width -1) / hevc->s_info.sps->max_CU_width;
    
    u32 index_x = hevc->s_info.slice_segment_address % number_of_CTU_per_width;
    u32 index_y = hevc->s_info.slice_segment_address /number_of_CTU_per_width;
    
    if(hevc->s_info.pps->tiles_enabled_flag)
    {
        if(hevc->s_info.pps->uniform_spacing_flag)
        {
            *x = ((index_x+1) * hevc->s_info.sps->max_CU_width)/(hevc->s_info.sps->width/(hevc->s_info.pps->num_tile_columns));
            *y = ((index_y+1) * hevc->s_info.sps->max_CU_height)/(hevc->s_info.sps->height/(hevc->s_info.pps->num_tile_rows));
        }   
        else
        {
            u32 cum_width=0;
            u32 cum_heigth=0, i;
            for(i=0;i < hevc->s_info.pps->num_tile_columns;i++)
            {
                if(index_x == cum_width)
                {
                    *x = i;
                    break;
                }
                cum_width += hevc->s_info.pps->column_width[i];
            }
            for(i=0;i < hevc->s_info.pps->num_tile_rows;i++)
            {
                if(index_y == cum_heigth)
                {
                    *y = i;
                    break;
                } 
                cum_heigth += hevc->s_info.pps->row_height[i]; 
            } 
        }
    }
}


//get the width and the height of the tile in pixel
void get_size_of_slice(u32 *width, u32 *height, HEVCState *hevc)
{
    u32 number_of_CTU_on_width = (hevc->s_info.sps->width + hevc->s_info.sps->max_CU_width -1) / hevc->s_info.sps->max_CU_width;
    u32 number_of_CTU_on_height = (hevc->s_info.sps->height + hevc->s_info.sps->max_CU_height -1) / hevc->s_info.sps->max_CU_height;
    
    u32 index_x = hevc->s_info.slice_segment_address % number_of_CTU_on_width;
    u32 index_y = hevc->s_info.slice_segment_address /number_of_CTU_on_width;
    
    if(hevc->s_info.pps->tiles_enabled_flag)
    {
        if(hevc->s_info.pps->uniform_spacing_flag)
        {
            u32 w = number_of_CTU_on_width/hevc->s_info.pps->num_tile_columns;
            if((number_of_CTU_on_width % hevc->s_info.pps->num_tile_columns) != 0)
                w +=1;
            *width = w * hevc->s_info.sps->max_CU_width;
            u32 h = number_of_CTU_on_height/hevc->s_info.pps->num_tile_rows;
            if((number_of_CTU_on_height % hevc->s_info.pps->num_tile_rows) != 0)
                h +=1;
            *height = h * hevc->s_info.sps->max_CU_height;
        }   
        else
        {
            u32 cum_width=0;
            u32 cum_heigth=0, i;
            for(i=0;i< hevc->s_info.pps->num_tile_columns;i++)
            {
                if(index_x == cum_width)
                {
                    *width = hevc->s_info.pps->column_width[i] * hevc->s_info.sps->max_CU_width;
                    break;
                }
                cum_width += hevc->s_info.pps->column_width[i];
            }
            for(i=0;i< hevc->s_info.pps->num_tile_rows;i++)
            {
                if(index_y == cum_heigth)
                {
                    *height = hevc->s_info.pps->row_height[i] * hevc->s_info.sps->max_CU_height; 
                    break;
                } 
                cum_heigth += hevc->s_info.pps->row_height[i]; 
            } 
        }    
    }
}

//rewrite the profile and level
void write_profile_tier_level(GF_BitStream *bs_in, GF_BitStream *bs_out , Bool ProfilePresentFlag, u8 MaxNumSubLayersMinus1)
{
    u8 j;
    Bool sub_layer_profile_present_flag[8], sub_layer_level_present_flag[8];
    if (ProfilePresentFlag) {
        gf_bs_write_int(bs_out, gf_bs_read_int(bs_in, 8), 8);
        gf_bs_write_long_int(bs_out, gf_bs_read_long_int(bs_in, 32), 32);
        gf_bs_write_int(bs_out, gf_bs_read_int(bs_in, 4), 4);
        gf_bs_write_long_int(bs_out, gf_bs_read_long_int(bs_in, 44), 44);
    }
    gf_bs_write_int(bs_out, gf_bs_read_int(bs_in, 8), 8);
    
    for (j=0; j < MaxNumSubLayersMinus1; j++) {
        sub_layer_profile_present_flag[j] = gf_bs_read_int(bs_in, 1);
        gf_bs_write_int(bs_out, sub_layer_profile_present_flag[j], 1);
        sub_layer_level_present_flag[j] = gf_bs_read_int(bs_in, 1);
        gf_bs_write_int(bs_out,sub_layer_level_present_flag[j], 1);
    }
    if (MaxNumSubLayersMinus1 > 0)
        for (j=MaxNumSubLayersMinus1; j < 8; j++) 
            gf_bs_write_int(bs_out, gf_bs_read_int(bs_in, 2), 2);
    

    for (j=0; j<MaxNumSubLayersMinus1; j++) {
        if (sub_layer_profile_present_flag[j]) {
            gf_bs_write_int(bs_out, gf_bs_read_int(bs_in, 8), 8);
            gf_bs_write_int(bs_out, gf_bs_read_int(bs_in, 32), 32);
            gf_bs_write_int(bs_out, gf_bs_read_int(bs_in, 4), 4);
            gf_bs_write_long_int(bs_out, gf_bs_read_long_int(bs_in, 44), 44);
        }
        if( sub_layer_level_present_flag[j])
            gf_bs_write_int(bs_out, gf_bs_read_int(bs_in, 8), 8);
    }
}

void rewrite_SPS(char *in_SPS, u32 in_SPS_length, char **out_SPS, u32 *out_SPS_length, HEVCState *hevc)
{
    GF_BitStream *bs_in, *bs_out;
    u64 length_no_use = 0;
    char *data_without_emulation_bytes = NULL;
    u32 data_without_emulation_bytes_size = 0, sps_ext_or_max_sub_layers_minus1;
    u8 max_sub_layers_minus1=0, layer_id;
    Bool conformance_window_flag, multiLayerExtSpsFlag;;
    u32 chroma_format_idc, width, height;
    
    data_without_emulation_bytes = gf_malloc(in_SPS_length*sizeof(char));
    data_without_emulation_bytes_size = avc_remove_emulation_bytes(in_SPS, data_without_emulation_bytes, in_SPS_length);
    bs_in = gf_bs_new(data_without_emulation_bytes, data_without_emulation_bytes_size, GF_BITSTREAM_READ);
    bs_out = gf_bs_new(NULL, length_no_use, GF_BITSTREAM_WRITE);
    if (!bs_in) goto exit;
    
    //copy NAL Header
    gf_bs_write_int(bs_out, gf_bs_read_int(bs_in, 7), 7); 
    layer_id = gf_bs_read_int(bs_in, 6);
    gf_bs_write_int(bs_out, layer_id, 6);
    gf_bs_write_int(bs_out, gf_bs_read_int(bs_in, 3), 3);
    
    gf_bs_write_int(bs_out, gf_bs_read_int(bs_in, 4), 4); //copy vps_id
    
    if (layer_id == 0)
    {
        max_sub_layers_minus1 = gf_bs_read_int(bs_in, 3);
        gf_bs_write_int(bs_out, max_sub_layers_minus1, 3);
    }  
    else
    {
        sps_ext_or_max_sub_layers_minus1 = gf_bs_read_int(bs_in, 3);
        gf_bs_write_int(bs_out, sps_ext_or_max_sub_layers_minus1, 3);
    }
    multiLayerExtSpsFlag = (layer_id != 0) && (sps_ext_or_max_sub_layers_minus1 == 7);
    if (!multiLayerExtSpsFlag) {		
        gf_bs_write_int(bs_out, gf_bs_read_int(bs_in, 1), 1);
        write_profile_tier_level(bs_in, bs_out, 1, max_sub_layers_minus1);
    }
    
    bs_set_ue(bs_out,bs_get_ue(bs_in)); //copy sps_id
    
    if (multiLayerExtSpsFlag) {
        u8 update_rep_format_flag = gf_bs_read_int(bs_in, 1);
        gf_bs_write_int(bs_out, update_rep_format_flag, 1);
        if (update_rep_format_flag) {
            gf_bs_write_int(bs_out, gf_bs_read_int(bs_in, 8), 8);
        }
    }else {
        chroma_format_idc = bs_get_ue(bs_in);
        bs_set_ue(bs_out,chroma_format_idc);
        if(chroma_format_idc==3)
            gf_bs_write_int(bs_out, gf_bs_read_int(bs_in, 1), 1); // copy separate_colour_plane_flag
        bs_get_ue(bs_in); //skip width bits in input bitstream
	bs_get_ue(bs_in); //skip height bits in input bitstream
        get_size_of_slice(&width, &height, hevc); // get the width and height of the tile
        
        //Copy the new width and height in output bitstream
        bs_set_ue(bs_out,width);
        bs_set_ue(bs_out,height);
        
        //Get rid of the bit conformance_window_flag
        conformance_window_flag = gf_bs_read_int(bs_in, 1);
        //put the new conformance flag to zero
        gf_bs_write_int(bs_out, 0, 1);
        
        //Skip the bits related to conformance_window_offset
        if(conformance_window_flag)
        {
            bs_get_ue(bs_in);
            bs_get_ue(bs_in);
            bs_get_ue(bs_in);
            bs_get_ue(bs_in);
        }
    }
    
    //Copy the rest of the bitstream
    if(gf_bs_get_bit_position(bs_in) != 8)
        gf_bs_write_int(bs_out, gf_bs_read_int(bs_in, 8-gf_bs_get_bit_position(bs_in)), 8-gf_bs_get_bit_position(bs_in));
    while (gf_bs_get_size(bs_in) != gf_bs_get_position(bs_in)) //Rest contents copying
    {
        gf_bs_write_int(bs_out, gf_bs_read_int(bs_in, 8), 8);      
    }


    gf_bs_align(bs_out);						//align

    *out_SPS_length = 0;
    *out_SPS = NULL;
    gf_bs_get_content(bs_out, out_SPS, out_SPS_length);

exit:
	gf_bs_del(bs_in);
	gf_bs_del(bs_out);
	gf_free(data_without_emulation_bytes);
}

void rewrite_PPS(char *in_PPS, u32 in_PPS_length, char **out_PPS, u32 *out_PPS_length)
{
    u64 length_no_use = 0;
    u8 cu_qp_delta_enabled_flag, tiles_enabled_flag;
    
    GF_BitStream *bs_in = gf_bs_new(in_PPS, in_PPS_length, GF_BITSTREAM_READ);
    GF_BitStream *bs_out = gf_bs_new(NULL, length_no_use, GF_BITSTREAM_WRITE);
    if (!bs_in) goto exit;
    
    //Read and write NAL header bits
    gf_bs_write_int(bs_out, gf_bs_read_int(bs_in, 16), 16);
    bs_set_ue(bs_out, bs_get_ue(bs_in)); //pps_pic_parameter_set_id
    bs_set_ue(bs_out, bs_get_ue(bs_in)); //pps_seq_parameter_set_id
    gf_bs_write_int(bs_out, gf_bs_read_int(bs_in, 7), 7); //from dependent_slice_segments_enabled_flag to cabac_init_present_flag
    bs_set_ue(bs_out, bs_get_ue(bs_in)); //num_ref_idx_l0_default_active_minus1
    bs_set_ue(bs_out, bs_get_ue(bs_in)); //num_ref_idx_l1_default_active_minus1
    bs_set_se(bs_out, bs_get_se(bs_in)); //init_qp_minus26
    gf_bs_write_int(bs_out, gf_bs_read_int(bs_in, 2), 2); //from constrained_intra_pred_flag to transform_skip_enabled_flag
    cu_qp_delta_enabled_flag = gf_bs_read_int(bs_in, 1); //cu_qp_delta_enabled_flag
    gf_bs_write_int(bs_out, cu_qp_delta_enabled_flag, 1); //
    if(cu_qp_delta_enabled_flag) 
        bs_set_ue(bs_out, bs_get_ue(bs_in)); // diff_cu_qp_delta_depth
    bs_set_se(bs_out, bs_get_se(bs_in)); // pps_cb_qp_offset
    bs_set_se(bs_out, bs_get_se(bs_in)); // pps_cr_qp_offset
    gf_bs_write_int(bs_out, gf_bs_read_int(bs_in, 4), 4); // from pps_slice_chroma_qp_offsets_present_flag to transquant_bypass_enabled_flag
    tiles_enabled_flag = gf_bs_read_int(bs_in, 1); // tiles_enabled_flag
    gf_bs_write_int(bs_out, 0, 1);
    gf_bs_write_int(bs_out, gf_bs_read_int(bs_in, 1), 1); // entropy_coding_sync_enabled_flag
    if(tiles_enabled_flag)
     {
        u32 num_tile_columns_minus1 = bs_get_ue(bs_in);
        u32 num_tile_rows_minus1 = bs_get_ue(bs_in);
        u8 uniform_spacing_flag =  gf_bs_read_int(bs_in, 1);
        if(!uniform_spacing_flag)
        {
            u32 i;
            for(i=0;i<num_tile_columns_minus1;i++)
                bs_get_ue(bs_in);
            for(i=0;i<num_tile_rows_minus1;i++)
                bs_get_ue(bs_in);
        } 
        gf_bs_read_int(bs_in, 1);
    }
    
    //copy and write the rest of the bits
    if(gf_bs_get_bit_position(bs_in) != 8)
        gf_bs_write_int(bs_out, gf_bs_read_int(bs_in, 8-gf_bs_get_bit_position(bs_in)), 8-gf_bs_get_bit_position(bs_in));
    while (gf_bs_get_size(bs_in) != gf_bs_get_position(bs_in)) //Rest contents copying
    {
        gf_bs_write_int(bs_out, gf_bs_read_int(bs_in, 8), 8);      
    }
    
    gf_bs_align(bs_out);						//align

    *out_PPS_length = 0;
    *out_PPS = NULL;
    gf_bs_get_content(bs_out, out_PPS, out_PPS_length);
    
    exit:
	gf_bs_del(bs_in);
	gf_bs_del(bs_out);
}

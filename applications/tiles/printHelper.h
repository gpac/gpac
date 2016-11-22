
#include <stdlib.h>
#include <stdio.h>
#include <gpac/bitstream.h>
#include <gpac/internal/media_dev.h>
void print_nalu(int argc, char **argv)
{
	FILE* file = NULL;
	HEVCState hevc; // = {.sps_active_idx=-1};
 	u8 nal_unit_type =1, temporal_id, layer_id;

	if(2 == argc){
		file = fopen(argv[1],"rb");
                if(file != NULL){
                        GF_BitStream *bs = NULL;
                        bs = gf_bs_from_file(file, GF_BITSTREAM_READ);
	       		if(bs != NULL){
				gf_media_hevc_parse_nalu(bs, &hevc, &nal_unit_type, &temporal_id, &layer_id);
		                int i = 0;
     				printf("====%p=====\n",&hevc);
				printf("nal_unit_type: \t%d\n",nal_unit_type);
				printf("temporal_id: \t%d\n",temporal_id);
				printf("layer_id: \t%d\n",layer_id);
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
}

void printPPS(FILE *file){

	char* buffer = NULL;
	HEVCState hevc; // = {.sps_active_idx=-1};
 	u8 nal_unit_type, temporal_id, layer_id;

	u32 bz = (u32)fseek(file, 0L, SEEK_END);
	printf("size \t%d\n",bz);
	if(bz != -1){
		buffer = malloc(sizeof(char) * (bz + 1));
		size_t rst = fread(buffer, sizeof(char), (bz+1), file);
		gf_media_hevc_read_pps(buffer,(bz + 1), &hevc);
		int i = 0;
     		printf("=====%p==pps===\n",&hevc);
		for(i=0;i<2;i++){
  			printf("====%d=====\n",i);
	      		//printf("sps_active_idx:	%d\n",hevc.sps_active_idx);
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
  		fclose(file);
	}		

}

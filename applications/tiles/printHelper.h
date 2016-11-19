
#include <stdlib.h>
#include <stdio.h>
#include <gpac/bitstream.h>
#include <gpac/internal/media_dev.h>

void printPPS(FILE *file){

	char* buffer = NULL;
	HEVCState hevc; // = {.sps_active_idx=-1};
 	u8 nal_unit_type, temporal_id, layer_id;

	long bz = fseek(file, 0L, SEEK_END);
	if(bz != -1){
		buffer = malloc(sizeof(char) * (bz + 1));
		size_t rst = fread(buffer, sizeof(char), (bz+1), file);
		gf_media_hevc_read_pps(buffer,(bz + 1), &hevc);
		int i = 0;
     		printf("====%p==pps===\n",&hevc);
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

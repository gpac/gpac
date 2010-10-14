#include "playlist.h"

const char * files[] = {
	"tests/spec_simple_playlist.m3u8",
	"tests/relative_playlist.m3u8",
	"tests/direct8.m3u8",
	"tests/spec_sliding_window_playlist.m3u8",
	"tests/spec_encrypted_media.m3u8",
	"tests/spec_variant_playlist.m3u8",
	"tests/spec_multiple_files.m3u8",
	 NULL
};

int main(int argc, char ** argv){
	int i = 0;
	while (files[i] != NULL){
		VariantPlaylist * pl = NULL;
		printf("Testing %s...\n", files[i]);
		if (parse_root_playlist(files[i], &pl, "tests/")){
			printf("**** Error while reading playlist %s !\n", files[i]);
		}
		i++;
		printf("DONE, now dumping...\n\n");
		variant_playlist_dump(pl);
		printf("DONE, now deleting...\n\n");
		variant_playlist_del( pl );
	}
	printf("Finished\n");
	return 0;
}

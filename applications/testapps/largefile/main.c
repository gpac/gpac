#include <gpac/isomedia.h>

void PrintUsage()
{
	fprintf(stdout, 
		"Usage: largefile [options]\n"
		"Option is one of:\n"
		"-flat        test file writing in flat mode (moov at end)\n"
		"-inter       test file writing in interleaved mode (moov at begin)\n"
		"-size size   specifies target media size in GB. Default is 5.0 GB\n"
		""
		);
}
#define TEST_FILE_NAME	"largefile.mp4"

int main(int argc, char **argv)
{
	GF_ISOFile *movie;
	GF_ESD *esd;
	GF_Err e;
	Double gb_size = 5.0;
	u8 store_mode;
	u32 track, di, i, nb_samp;
	GF_ISOSample *samp;

	store_mode = GF_ISOM_OPEN_WRITE;
	for (i=1; i<argc; i++) {
		if (!strcmp(argv[i], "-flat")) store_mode = GF_ISOM_OPEN_WRITE;
		else if (!strcmp(argv[i], "-inter")) store_mode = GF_ISOM_WRITE_EDIT;
		else if (!strcmp(argv[i], "-size") && (i+1<argc)) {
			gb_size = atof(argv[i+1]);
			i++;
		}
		else if (!strcmp(argv[i], "-h")) {
			PrintUsage();
			return 0;
		}
	}

	nb_samp = (u32) (gb_size*1024);
	fprintf(stdout, "Creating test file %s - %g GBytes - %d samples - %s mode\n", TEST_FILE_NAME, gb_size, nb_samp, (store_mode == GF_ISOM_OPEN_WRITE) ? "Flat" : "Interleaved");
	
	movie = gf_isom_open(TEST_FILE_NAME, store_mode, NULL);
	if (!movie) {
		fprintf(stdout, "Error creating file: %s\n", gf_error_to_string(gf_isom_last_error(NULL)));
		return 1;
	}

	track = gf_isom_new_track(movie, 1, GF_ISOM_MEDIA_VISUAL, 25);
	esd = gf_odf_desc_esd_new(2);
	esd->decoderConfig->streamType = 4;
	gf_isom_new_mpeg4_description(movie, track, esd, NULL, NULL, &di);

	samp = gf_isom_sample_new();
	samp->dataLength = 1024*1024;
	samp->data = malloc(sizeof(char)*samp->dataLength);
  memset(samp->data, 0, sizeof(char)*samp->dataLength);
  
	for (i=0; i<nb_samp; i++) {
		if (samp->DTS % 25) samp->IsRAP = 0;
		else samp->IsRAP = 1;
		e = gf_isom_add_sample(movie, track, di, samp);
		samp->DTS += 1;

		fprintf(stdout, "Writing sample %d / %d \r", i+1, nb_samp);
		if (e) break;
	}
	gf_isom_sample_del(&samp);

	if (e) {
		fprintf(stdout, "\nError writing sample %d\n", i);
		gf_isom_delete(movie);
		return 1;
	}
	
	fprintf(stdout, "\nDone writing samples\n");
	e = gf_isom_close(movie);
	if (e) {
		fprintf(stdout, "Error writing file\n");
		return 1;
	}
	return 0;
}



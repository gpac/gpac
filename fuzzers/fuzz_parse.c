#include <stdio.h>
#include <unistd.h>

#include <gpac/internal/isomedia_dev.h>
#include <gpac/constants.h>

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    char filename[256];
    sprintf(filename, "/tmp/libfuzzer.%d", getpid());

    FILE *fp = fopen(filename, "wb");
    if (!fp) {
        return 0;
    }
    fwrite(data, size, 1, fp);
    fclose(fp);

    GF_ISOFile *movie = NULL;
    movie = gf_isom_open_file(filename, GF_ISOM_OPEN_READ_DUMP, NULL);
    if (movie != NULL) {
        gf_isom_close(movie);
    }
    unlink(filename);
    return 0;
}

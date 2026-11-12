#include "tests.h"
#include <gpac/filters.h>
#include <gpac/isomedia.h>

#if !defined(GPAC_DISABLE_ISOM) && !defined(GPAC_DISABLE_ISOM_WRITE) && !defined(GPAC_DISABLE_CRYPTO)
#define UT_CDCRYPT

#define CENC_NB_SAMPLES 4
#define CENC_SAMPLE_SIZE 64
#define CENC_KID "0x279926496a7f5d25da69f2b3b2799a7f"
#define CENC_KEY "0x5544694d47473326622665665a396b36"

static void cenc_sample_data(u32 idx, u8 *data)
{
	u32 i;
	for (i=0; i<CENC_SAMPLE_SIZE; i++) data[i] = (u8) (idx*CENC_SAMPLE_SIZE + i);
}

static GF_Err cenc_write_clear(const char *path)
{
	GF_GenericSampleDescription desc = {0};
	u32 i, track, desc_index;
	u8 data[CENC_SAMPLE_SIZE];
	GF_ISOFile *file = gf_isom_open(path, GF_ISOM_OPEN_WRITE, NULL);
	if (!file) return GF_IO_ERR;
	track = gf_isom_new_track(file, 0, GF_ISOM_MEDIA_VISUAL, 1000);
	gf_isom_set_track_enabled(file, track, GF_TRUE);
	desc.codec_tag = GF_4CC('t', 'e', 's', 't');
	GF_Err e = gf_isom_new_generic_sample_description(file, track, NULL, NULL, &desc, &desc_index);
	for (i=0; !e && (i<CENC_NB_SAMPLES); i++) {
		GF_ISOSample samp = {0};
		cenc_sample_data(i, data);
		samp.data = data;
		samp.dataLength = CENC_SAMPLE_SIZE;
		samp.DTS = i*40;
		samp.IsRAP = RAP;
		e = gf_isom_add_sample(file, track, desc_index, &samp);
	}
	if (e) {
		gf_isom_delete(file);
		return e;
	}
	return gf_isom_close(file);
}

//src -> filter -> dst, the way `gpac -i src filter -o dst` links them
static GF_Err cenc_run(const char *src, const char *filter, const char *dst)
{
	GF_Err e = GF_OK;
	GF_FilterSession *fs = gf_fs_new_defaults(0);
	if (!fs) return GF_OUT_OF_MEM;
	GF_Filter *f_src = gf_fs_load_source(fs, src, NULL, NULL, &e);
	GF_Filter *f = e ? NULL : gf_fs_load_filter(fs, filter, &e);
	GF_Filter *f_dst = e ? NULL : gf_fs_load_destination(fs, dst, NULL, NULL, &e);
	if (!e) e = gf_filter_set_source(f, f_src, NULL);
	if (!e) e = gf_filter_set_source(f_dst, f, NULL);
	if (!e) {
		e = gf_fs_run(fs);
		if (e == GF_EOS) e = GF_OK;
	}
	if (!e) e = gf_fs_get_last_connect_error(fs);
	if (!e) e = gf_fs_get_last_process_error(fs);
	gf_fs_del(fs);
	return e;
}

static Bool cenc_is_clear(const char *path)
{
	u32 i;
	u8 data[CENC_SAMPLE_SIZE];
	Bool ok = GF_TRUE;
	GF_ISOFile *file = gf_isom_open(path, GF_ISOM_OPEN_READ, NULL);
	if (!file) return GF_FALSE;
	if (gf_isom_get_sample_count(file, 1) != CENC_NB_SAMPLES) ok = GF_FALSE;
	for (i=0; ok && (i<CENC_NB_SAMPLES); i++) {
		u32 di;
		GF_ISOSample *samp = gf_isom_get_sample(file, 1, i+1, &di);
		cenc_sample_data(i, data);
		if (!samp || (samp->dataLength != CENC_SAMPLE_SIZE) || memcmp(samp->data, data, CENC_SAMPLE_SIZE)) ok = GF_FALSE;
		if (samp) gf_isom_sample_del(&samp);
	}
	gf_isom_delete(file);
	return ok;
}

static void cenc_write_cfile(const char *path, Bool with_pssh)
{
	FILE *f = gf_fopen(path, "w");
	if (!f) return;
	gf_fprintf(f, "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<GPACDRM type=\"CENC AES-CTR\">\n");
	if (with_pssh)
		gf_fprintf(f, "<DRMInfo type=\"pssh\" version=\"1\"><BS ID128=\"EDEF8BA979D64ACEA3C827DCD51D21ED\"/><BS bits=\"32\" value=\"1\"/><BS ID128=\"" CENC_KID "\"/></DRMInfo>\n");
	gf_fprintf(f, "<CrypTrack trackID=\"1\" IsEncrypted=\"1\" IV_size=\"16\" first_IV=\"0x0a610676cb88f302d10ac8bc66e039ed\" saiSavedBox=\"senc\">\n"
		"<key KID=\"" CENC_KID "\" value=\"" CENC_KEY "\"/>\n</CrypTrack>\n</GPACDRM>\n");
	gf_fclose(f);
}

#endif

unittest(cdcrypt_kids_keys)
{
#ifdef UT_CDCRYPT
	const char *clear = "ut-cdcrypt-clear.mp4", *enc = "ut-cdcrypt-enc.mp4", *dec = "ut-cdcrypt-dec.mp4", *cfile = "ut-cdcrypt-drm.xml";
	u32 with_pssh;

	assert_equal(cenc_write_clear(clear), GF_OK, "%d");
	assert_true(cenc_is_clear(clear));

	// keys given only as kids/keys must decrypt like the same keys given in a crypt file,
	// whether or not the file carries a pssh box
	for (with_pssh=0; with_pssh<2; with_pssh++) {
		cenc_write_cfile(cfile, with_pssh);
		assert_equal(cenc_run(clear, "cecrypt:cfile=ut-cdcrypt-drm.xml", enc), GF_OK, "%d");
		assert_true(!cenc_is_clear(enc));

		// control: keys from the crypt file
		assert_equal(cenc_run(enc, "cdcrypt:cfile=ut-cdcrypt-drm.xml", dec), GF_OK, "%d");
		assert_true(cenc_is_clear(dec));

		assert_equal(cenc_run(enc, "cdcrypt:kids=" CENC_KID ":keys=" CENC_KEY, dec), GF_OK, "%d");
		assert_true(cenc_is_clear(dec));
	}

	gf_file_delete(clear);
	gf_file_delete(enc);
	gf_file_delete(dec);
	gf_file_delete(cfile);
#endif
}

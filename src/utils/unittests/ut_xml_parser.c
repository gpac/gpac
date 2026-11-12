#include "tests.h"
#include <gpac/xml.h>

unittest(xml_translate_xml_string)
{
	char *xml_translate_xml_string(char *str);
    char *str= xml_translate_xml_string("&amp;");
    assert_equal_str(str, "&");
    gf_free(str);
}

unittest(xml_get_root_type_file)
{
	const char *path = "ut-xml-root.xml";
	const char *xml = "<GPACDRM/>";
	//file reader consumes a 4-byte BOM probe then 4096-byte chunks
	const u32 sizes[] = {10, 4099, 4100, 4101, 8193};
	char data[8193];
	u32 i;
	memset(data, ' ', sizeof(data));
	memcpy(data, xml, strlen(xml));
	for (i=0; i<GF_ARRAY_LENGTH(sizes); i++) {
		GF_Err e;
		char *root;
		FILE *f = gf_fopen(path, "wb");
		assert_not_null(f);
		if (!f) return;
		assert_equal(gf_fwrite(data, sizes[i], f), (size_t) sizes[i], "%zu");
		gf_fclose(f);
		root = gf_xml_get_root_type(path, &e);
		assert_equal(e, GF_OK, "%d");
		assert_true(root && !strcmp(root, "GPACDRM"));
		if (root) gf_free(root);
	}
	gf_file_delete(path);
}

unittest(xml_get_root_type_file_errors)
{
	const char *path = "ut-xml-root-errors.xml";
	const char *inputs[] = { "\t", "<GPACDRM key=unquoted/>" };
	u32 i;
	GF_Err e;
	char *root;
	for (i=0; i<GF_ARRAY_LENGTH(inputs); i++) {
		FILE *f = gf_fopen(path, "wb");
		assert_not_null(f);
		if (!f) return;
		assert_equal(gf_fwrite(inputs[i], strlen(inputs[i]), f), strlen(inputs[i]), "%zu");
		gf_fclose(f);
		root = gf_xml_get_root_type(path, &e);
		assert_true(root == NULL);
		if (i==0) assert_equal(e, GF_EOS, "%d");
		else assert_true(e<0);
		if (root) gf_free(root);
	}
	gf_file_delete(path);
	root = gf_xml_get_root_type(path, &e);
	assert_true(root == NULL);
	assert_equal(e, GF_URL_ERROR, "%d");
	if (root) gf_free(root);
}

static GF_Err parse_blob(const u8 *data, u32 size, char **root_name)
{
	GF_Blob blob = {0};
	blob.data = (u8 *) data;
	blob.size = size;
	char *url = gf_blob_register(&blob);
	GF_DOMParser *dom = gf_xml_dom_new();
	GF_Err e = gf_xml_dom_parse(dom, url, NULL, NULL);
	GF_XMLNode *root = gf_xml_dom_get_root(dom);
	*root_name = (root && root->name) ? gf_strdup(root->name) : NULL;
	gf_xml_dom_del(dom);
	gf_blob_unregister(&blob);
	gf_free(url);
	return e;
}

unittest(xml_dom_parse_gzip_blob)
{
#ifndef GPAC_DISABLE_ZLIB
	const char *xml = "<?xml version=\"1.0\"?>\n<MPD type=\"dynamic\"><Period id=\"1\"/></MPD>\n";
	u32 len = (u32) strlen(xml);
	char *root_name;

	// control: the plain document
	assert_equal(parse_blob((const u8 *) xml, len, &root_name), GF_OK, "%d");
	assert_true(root_name && !strcmp(root_name, "MPD"));
	if (root_name) gf_free(root_name);

	// the same document gzip-compressed, as an HTTP body sent with Content-Encoding: gzip
	u8 *gz = NULL;
	u32 gz_size = 0;
	assert_equal(gf_gz_compress_payload_ex((u8 **) &xml, len, &gz_size, 0, GF_FALSE, &gz, GF_TRUE), GF_OK, "%d");
	assert_true(gz && (gz[0] == 0x1f) && (gz[1] == 0x8b));
	assert_equal(parse_blob(gz, gz_size, &root_name), GF_OK, "%d");
	assert_true(root_name && !strcmp(root_name, "MPD"));
	if (root_name) gf_free(root_name);
	gf_free(gz);
#else
	GF_LOG(GF_LOG_WARNING, GF_LOG_CORE, ("gzip test skipped because GPAC_DISABLE_ZLIB is defined") );
#endif // GPAC_DISABLE_ZLIB
}

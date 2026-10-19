#include "tests.h"
#include <gpac/xml.h>

char *xml_translate_xml_string(char *str);
unittest(xml_translate_xml_string)
{
    char *str= xml_translate_xml_string("&amp;");
    assert_equal_str(str, "&");
    gf_free(str);
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
}

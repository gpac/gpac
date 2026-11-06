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

unittest(xml_dom_parse_string_unterminated_utf16)
{
	// UTF-16LE BOM + "A" units with NO aligned 0x0000 terminator: the byte
	// string's single trailing NUL sits at an odd offset, so the u16 view has
	// no terminator within the buffer and must be rejected, not over-read.
	char *buf = (char *) gf_malloc(20);
	memset(buf, 'A', 19);
	buf[0] = (char) 0xFF; buf[1] = (char) 0xFE;
	buf[19] = 0;

	GF_DOMParser *dom = gf_xml_dom_new();
	GF_Err e = gf_xml_dom_parse_string(dom, buf);
	assert_equal(e, GF_CORRUPTED_DATA, "%d");
	gf_xml_dom_del(dom);
	gf_free(buf);

}

unittest(xml_dom_parse_utf16_blob)
{
	// control: a properly 0x0000-terminated UTF-16LE document still parses
	// through the sized buffer path (file/blob), where the real extent is known
	static const u8 u16doc[] = {
		0xFF, 0xFE, '<', 0, 'a', 0, '/', 0, '>', 0, 0, 0
	};
	char *root_name;
	assert_equal(parse_blob(u16doc, sizeof(u16doc), &root_name), GF_OK, "%d");
	assert_true(root_name && !strcmp(root_name, "a"));
	if (root_name) gf_free(root_name);
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

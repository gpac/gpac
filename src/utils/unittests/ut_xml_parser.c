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
	assert_equal(e, GF_NOT_SUPPORTED, "%d");
	gf_xml_dom_del(dom);
	gf_free(buf);

}

unittest(xml_dom_parse_string_rejects_utf16)
{
	static char u16doc[] = {
		(char) 0xFF, (char) 0xFE, '<', 0, 'a', 0, '/', 0, '>', 0, 0, 0
	};
	GF_DOMParser *dom = gf_xml_dom_new();
	GF_Err e = gf_xml_dom_parse_string(dom, u16doc);
	assert_equal(e, GF_NOT_SUPPORTED, "%d");
	gf_xml_dom_del(dom);
}

unittest(xml_dom_parse_string_len_utf16le)
{
	static const u8 u16doc[] = {
		0xFF, 0xFE, '<', 0, 'a', 0, '/', 0, '>', 0
	};
	GF_DOMParser *dom = gf_xml_dom_new();
	GF_Err e = gf_xml_dom_parse_string_len(dom, u16doc, sizeof(u16doc));
	GF_XMLNode *root = gf_xml_dom_get_root(dom);
	assert_equal(e, GF_OK, "%d");
	assert_true(root && !strcmp(root->name, "a"));
	gf_xml_dom_del(dom);
}

unittest(xml_dom_parse_string_len_utf16be)
{
	static const u8 u16doc[] = {
		0xFE, 0xFF, 0, '<', 0, 'a', 0, '/', 0, '>'
	};
	GF_DOMParser *dom = gf_xml_dom_new();
	GF_Err e = gf_xml_dom_parse_string_len(dom, u16doc, sizeof(u16doc));
	GF_XMLNode *root = gf_xml_dom_get_root(dom);
	assert_equal(e, GF_OK, "%d");
	assert_true(root && !strcmp(root->name, "a"));
	gf_xml_dom_del(dom);
}

unittest(xml_dom_parse_string_len_utf8)
{
	static const char xml[] = "<a/>";
	GF_DOMParser *dom = gf_xml_dom_new();
	GF_Err e = gf_xml_dom_parse_string_len(dom, xml, sizeof(xml) - 1);
	GF_XMLNode *root = gf_xml_dom_get_root(dom);
	assert_equal(e, GF_OK, "%d");
	assert_true(root && !strcmp(root->name, "a"));
	gf_xml_dom_del(dom);
}

unittest(xml_dom_parse_string_len_trailing_terminators)
{
	static const char xml[] = "<a/>";
	static const u8 u16doc[] = {
		0xFF, 0xFE, '<', 0, 'a', 0, '/', 0, '>', 0, 0, 0
	};
	GF_DOMParser *dom = gf_xml_dom_new();
	GF_Err e = gf_xml_dom_parse_string_len(dom, xml, sizeof(xml));
	GF_XMLNode *root = gf_xml_dom_get_root(dom);
	assert_equal(e, GF_OK, "%d");
	assert_true(root && !strcmp(root->name, "a"));
	gf_xml_dom_del(dom);

	dom = gf_xml_dom_new();
	e = gf_xml_dom_parse_string_len(dom, u16doc, sizeof(u16doc));
	root = gf_xml_dom_get_root(dom);
	assert_equal(e, GF_OK, "%d");
	assert_true(root && !strcmp(root->name, "a"));
	gf_xml_dom_del(dom);
}

unittest(xml_dom_parse_string_len_odd_utf16)
{
	static const u8 u16doc[] = {0xFF, 0xFE, '<'};
	GF_DOMParser *dom = gf_xml_dom_new();
	GF_Err e = gf_xml_dom_parse_string_len(dom, u16doc, sizeof(u16doc));
	assert_equal(e, GF_CORRUPTED_DATA, "%d");
	gf_xml_dom_del(dom);
}

unittest(xml_sax_legacy_rejects_utf16)
{
	static const unsigned char legacy_bom[] = {0xFF, 0xFE, 0};
	static const unsigned char bom[] = {0xFF, 0xFE};
	static const unsigned char body[] = {'<', 0, 'a', 0, '/', 0, '>', 0};
	GF_SAXParser *parser = gf_xml_sax_new(NULL, NULL, NULL, NULL);
	assert_equal(gf_xml_sax_init(parser, (unsigned char *) legacy_bom), GF_NOT_SUPPORTED, "%d");
	assert_equal(gf_xml_sax_init_len(parser, bom, sizeof(bom)), GF_OK, "%d");
	assert_equal(gf_xml_sax_parse(parser, body), GF_NOT_SUPPORTED, "%d");
	gf_xml_sax_del(parser);
}

unittest(xml_sax_len_trailing_terminators)
{
	static const unsigned char utf8[] = "<a/>";
	static const unsigned char bom[] = {0xFF, 0xFE};
	static const unsigned char utf16[] = {'<', 0, 'a', 0, '/', 0, '>', 0, 0, 0};
	GF_SAXParser *parser = gf_xml_sax_new(NULL, NULL, NULL, NULL);
	assert_equal(gf_xml_sax_init_len(parser, utf8, sizeof(utf8)), GF_OK, "%d");
	gf_xml_sax_del(parser);

	parser = gf_xml_sax_new(NULL, NULL, NULL, NULL);
	assert_equal(gf_xml_sax_init_len(parser, bom, sizeof(bom)), GF_OK, "%d");
	assert_equal(gf_xml_sax_parse_len(parser, utf16, sizeof(utf16)), GF_OK, "%d");
	gf_xml_sax_del(parser);
}

unittest(xml_sax_parse_len_utf16_chunks)
{
	static const unsigned char first[] = {0xFF};
	static const unsigned char second[] = {0xFE, '<'};
	static const unsigned char third[] = {0, 'a', 0, '/', 0, '>', 0};
	GF_SAXParser *parser = gf_xml_sax_new(NULL, NULL, NULL, NULL);
	assert_equal(gf_xml_sax_init_len(parser, first, sizeof(first)), GF_OK, "%d");
	assert_equal(gf_xml_sax_parse_len(parser, second, sizeof(second)), GF_OK, "%d");
	assert_equal(gf_xml_sax_parse_len(parser, third, sizeof(third)), GF_OK, "%d");
	gf_xml_sax_del(parser);
}

unittest(xml_sax_utf16_surrogate_pair_across_chunks)
{
	static const unsigned char bom[] = {0xFF, 0xFE};
	static const unsigned char first[] = {'<', 0, 'a', 0, '>', 0, 0x3D, 0xD8};
	static const unsigned char second[] = {0x00, 0xDE, '<', 0, '/', 0, 'a', 0, '>', 0};
	GF_SAXParser *parser = gf_xml_sax_new(NULL, NULL, NULL, NULL);
	assert_equal(gf_xml_sax_init_len(parser, bom, sizeof(bom)), GF_OK, "%d");
	assert_equal(gf_xml_sax_parse_len(parser, first, sizeof(first)), GF_OK, "%d");
	assert_equal(gf_xml_sax_parse_len(parser, second, sizeof(second)), GF_OK, "%d");
	gf_xml_sax_del(parser);
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

unittest(xml_dom_parse_unterminated_blob)
{
	// raw blob with no NUL byte: must not read past the blob end
	static const u8 junk[] = "<?5=\"2.???b?P>M&";
	char *root_name;
	parse_blob(junk, sizeof(junk)-1, &root_name);
	if (root_name) gf_free(root_name);
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

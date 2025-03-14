#include <gpac/internal/id3.h>

// First 36 bytes of a Nielsen ID3 tag: "ID3\x04\x00 \x00\x00\x02\x05PRIV\x00\x00\x01{\x00\x00www.nielsen.com/"
static const u32 NIELSEN_ID3_TAG_PREFIX_LEN = 36;
static const u8 NIELSEN_ID3_TAG_PREFIX[] = {0x49, 0x44, 0x33, 0x04, 0x00, 0x20,
                                            0x00, 0x00, 0x02, 0x05, 0x50, 0x52,
                                            0x49, 0x56, 0x00, 0x00, 0x01, 0x7B,
                                            0x00, 0x00, 0x77, 0x77, 0x77, 0x2E,
                                            0x6E, 0x69, 0x65, 0x6C, 0x73, 0x65,
                                            0x6E, 0x2E, 0x63, 0x6F, 0x6D, 0x2F};

static const char *ID3_PROP_SCHEME_URI = "https://aomedia.org/emsg/ID3";
static const char *ID3_PROP_VALUE_URI_NIELSEN = "www.nielsen.com:id3:v1";
static const char *ID3_PROP_VALUE_URI_DEFAULT = "https://aomedia.org/emsg/ID3";

GF_Err gf_id3_tag_new(GF_ID3_TAG *tag, u32 timescale, u64 pts, u8 *data, u32 data_length)
{
    if (!tag) {
        return GF_BAD_PARAM;
    }

    if (data == NULL || data_length == 0) {
        return GF_BAD_PARAM;
    }

    tag->timescale = timescale;
    tag->pts = pts;
    tag->scheme_uri = gf_strdup(ID3_PROP_SCHEME_URI);
    tag->scheme_uri_length = (u32) strlen(ID3_PROP_SCHEME_URI) + 1; // plus null character

    // test if the data is a Nielsen ID3 tag
    if ((data_length >= NIELSEN_ID3_TAG_PREFIX_LEN) && !memcmp(data, NIELSEN_ID3_TAG_PREFIX, NIELSEN_ID3_TAG_PREFIX_LEN))
    {
        tag->value_uri = gf_strdup(ID3_PROP_VALUE_URI_NIELSEN);
    }
    else
    {
        tag->value_uri = gf_strdup(ID3_PROP_VALUE_URI_DEFAULT);
    }

    tag->value_uri_length = (u32) strlen(tag->value_uri) + 1; // plus null character

    tag->data_length = data_length;
    tag->data = gf_malloc(data_length);
    memcpy(tag->data, data, data_length);

    return GF_OK;
}

void gf_id3_tag_free(GF_ID3_TAG *tag)
{
    if (!tag) {
        return;
    }

    if (tag->scheme_uri) {
        gf_free(tag->scheme_uri);
        tag->scheme_uri = NULL;
    }

    if (tag->value_uri) {
        gf_free(tag->value_uri);
        tag->value_uri = NULL;
    }

    if (tag->data) {
        gf_free(tag->data);
        tag->data = NULL;
    }
}

GF_Err gf_id3_to_bitstream(GF_ID3_TAG *tag, GF_BitStream *bs)
{
    if (!tag || !bs) {
        return GF_BAD_PARAM;
    }

    gf_bs_write_u32(bs, tag->timescale);
    gf_bs_write_u64(bs, tag->pts);

    gf_bs_write_u32(bs, tag->scheme_uri_length);
    u32 bytes_read = gf_bs_write_data(bs, (const u8 *)tag->scheme_uri, tag->scheme_uri_length);
    if (bytes_read != tag->scheme_uri_length) {
        return GF_IO_ERR;
    }

    gf_bs_write_u32(bs, tag->value_uri_length);
    bytes_read = gf_bs_write_data(bs, (const u8 *)tag->value_uri, tag->value_uri_length);
    if (bytes_read != tag->value_uri_length) {
        return GF_IO_ERR;
    }

    gf_bs_write_u32(bs, tag->data_length);
    bytes_read = gf_bs_write_data(bs, tag->data, tag->data_length);
    if (bytes_read != tag->data_length) {
        return GF_IO_ERR;
    }

    return GF_OK;
}

GF_Err gf_id3_list_to_bitstream(GF_List *tag_list, GF_BitStream *bs) {

    GF_Err err = GF_OK;

    // first, write the number of tags
    u32 id3_count = gf_list_count(tag_list);
    gf_bs_write_u32(bs, id3_count);

    for (u32 i = 0; i < id3_count; ++i) {
        GF_ID3_TAG *tag = gf_list_get(tag_list, i);
        err = gf_id3_to_bitstream(tag, bs);
        if (err != GF_OK) {
            return err;
        }
    }

    return err;
}

GF_Err gf_id3_from_bitstream(GF_ID3_TAG *tag, GF_BitStream *bs)
{
    if (!tag || !bs) {
        return GF_BAD_PARAM;
    }

    tag->timescale = gf_bs_read_u32(bs);
    tag->pts = gf_bs_read_u64(bs);

    tag->scheme_uri_length = gf_bs_read_u32(bs);
    tag->scheme_uri = gf_malloc(tag->scheme_uri_length);
    u32 bytes_read = gf_bs_read_data(bs, tag->scheme_uri, tag->scheme_uri_length);
    if (bytes_read != tag->scheme_uri_length) {
        return GF_IO_ERR;
    }

    tag->value_uri_length = gf_bs_read_u32(bs);
    tag->value_uri = gf_malloc(tag->value_uri_length);
    bytes_read = gf_bs_read_data(bs, tag->value_uri, tag->value_uri_length);
    if (bytes_read != tag->value_uri_length) {
        return GF_IO_ERR;
    }

    tag->data_length = gf_bs_read_u32(bs);
    tag->data = gf_malloc(tag->data_length);
    bytes_read = gf_bs_read_data(bs, tag->data, tag->data_length);
    if (bytes_read != tag->data_length) {
        return GF_IO_ERR;
    }

    return GF_OK;
}

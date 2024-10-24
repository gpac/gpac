
#ifndef _GF_ID3_H_
#define _GF_ID3_H_
#ifdef __cplusplus
extern "C" {
#endif

#include <gpac/bitstream.h>
#include <gpac/list.h>

typedef struct {
	u32 timescale;
	u64 pts;
	u32 scheme_uri_length;
	char* scheme_uri;
	u32 value_uri_length;
	char* value_uri;
	u32 data_length;
	u8* data;
} GF_ID3_TAG;

GF_Err gf_id3_tag_new(GF_ID3_TAG *tag, u32 timescale, u64 pts, u8 *data, u32 data_length);
void gf_id3_tag_free(GF_ID3_TAG *tag);
GF_Err gf_id3_to_bitstream(GF_ID3_TAG *tag, GF_BitStream *bs);
GF_Err gf_id3_list_to_bitstream(GF_List *tag_list, GF_BitStream *bs);
GF_Err gf_id3_from_bitstream(GF_ID3_TAG *tag, GF_BitStream *bs);

#ifdef __cplusplus
}
#endif

#endif // _GF_ID3_H_
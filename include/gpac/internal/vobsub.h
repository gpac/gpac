#ifndef _GF_VOBSUB_H_
#define _GF_VOBSUB_H_

#include <gpac/tools.h>

#ifdef __cplusplus
extern "C" {
#endif

#define VOBSUBIDXVER 7

typedef struct _tag_vobsub_pos
{
	u64  filepos;
	u64  start;
	u64  stop;
} vobsub_pos;

typedef struct _tag_vobsub_lang
{
	u32      id;
	char    *name;
	GF_List *subpos;
} vobsub_lang;

typedef struct _tag_vobsub_file
{
	u32         width;
	u32         height;
	u8          palette[16][4];
	u32         num_langs;
	vobsub_lang langs[32];
} vobsub_file;

GF_Err vobsub_read_idx(FILE *file, vobsub_file *vobsub, int *version);
void   vobsub_free(vobsub_file *vobsub);

#ifdef __cplusplus
}
#endif

#endif /* _GF_VOBSUB_H_ */

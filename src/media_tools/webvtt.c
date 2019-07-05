/*
 *          GPAC - Multimedia Framework C SDK
 *
 *          Authors: Cyril Concolato
 *          Copyright (c) Telecom ParisTech 2000-2012
 *                  All rights reserved
 *
 *  This file is part of GPAC / ISO Media File Format sub-project
 *
 *  GPAC is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU Lesser General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your option)
 *  any later version.
 *
 *  GPAC is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; see the file COPYING.  If not, write to
 *  the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 */

#include <gpac/list.h>
#include <gpac/internal/isomedia_dev.h>
#include <gpac/internal/media_dev.h>
#include <gpac/webvtt.h>
#include <gpac/constants.h>

#ifndef GPAC_DISABLE_VTT

struct _webvtt_sample
{
	u64 start;
	u64 end;
	GF_List *cues;
};


#ifndef GPAC_DISABLE_ISOM

typedef struct
{
	GF_ISOM_BOX
	GF_StringBox *id;
	GF_StringBox *time;
	GF_StringBox *settings;
	GF_StringBox *payload;
} GF_VTTCueBox;

GF_Box *boxstring_box_new() {
	//type is assigned by caller
	ISOM_DECL_BOX_ALLOC(GF_StringBox, 0);
	return (GF_Box *)tmp;
}

GF_Box *boxstring_new_with_data(u32 type, const char *string, GF_List **parent)
{
	GF_Box *a = NULL;

	switch (type) {
	case GF_ISOM_BOX_TYPE_VTTC_CONFIG:
	case GF_ISOM_BOX_TYPE_CTIM:
	case GF_ISOM_BOX_TYPE_IDEN:
	case GF_ISOM_BOX_TYPE_STTG:
	case GF_ISOM_BOX_TYPE_PAYL:
	case GF_ISOM_BOX_TYPE_VTTA:
		if (string) {
			/* remove trailing spaces; spec. \r, \n; skip if empty */
			size_t len = strlen(string);
			char const* last = string + len-1;
			while (len && isspace(*last--))
				--len;

			if (!len) break;
			if (parent) {
				a = gf_isom_box_new_parent(parent, type);
			} else {
				a = gf_isom_box_new(type);
			}
			if (a) {
				char* str = ((GF_StringBox *)a)->string = gf_malloc(len + 1);
				memcpy(str, string, len);
				str[len] = '\0';
			}
		}
		break;
	default:
		GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[iso file] Box type %s is not a boxstring, cannot initialize with data\n", gf_4cc_to_str(type) ));

		break;
	}
	return a;
}

GF_Box *vtcu_box_new()
{
	ISOM_DECL_BOX_ALLOC(GF_VTTCueBox, GF_ISOM_BOX_TYPE_VTCC_CUE);
	return (GF_Box *)tmp;
}

GF_Box *vtte_box_new() {
	ISOM_DECL_BOX_ALLOC(GF_Box, GF_ISOM_BOX_TYPE_VTTE);
	return (GF_Box *)tmp;
}

GF_Box *wvtt_box_new()
{
	ISOM_DECL_BOX_ALLOC(GF_WebVTTSampleEntryBox, GF_ISOM_BOX_TYPE_WVTT);
	return (GF_Box *)tmp;
}

void boxstring_box_del(GF_Box *s)
{
	GF_StringBox *box = (GF_StringBox *)s;
	if (box->string) gf_free(box->string);
	gf_free(box);
}

void vtcu_box_del(GF_Box *s)
{
	gf_free(s);
}

void vtte_box_del(GF_Box *s)
{
	gf_free(s);
}

void wvtt_box_del(GF_Box *s)
{
	gf_free(s);
}

GF_Err boxstring_box_read(GF_Box *s, GF_BitStream *bs)
{
	GF_StringBox *box = (GF_StringBox *)s;
	box->string = (char *)gf_malloc((u32)(s->size+1));
	gf_bs_read_data(bs, box->string, (u32)(s->size));
	box->string[(u32)(s->size)] = 0;
	return GF_OK;
}

static GF_Err vtcu_Add(GF_Box *s, GF_Box *box)
{
	GF_VTTCueBox *cuebox = (GF_VTTCueBox *)s;
	switch(box->type) {
	case GF_ISOM_BOX_TYPE_CTIM:
		cuebox->time = (GF_StringBox *)box;
		break;
	case GF_ISOM_BOX_TYPE_IDEN:
		cuebox->id = (GF_StringBox *)box;
		break;
	case GF_ISOM_BOX_TYPE_STTG:
		cuebox->settings = (GF_StringBox *)box;
		break;
	case GF_ISOM_BOX_TYPE_PAYL:
		cuebox->payload = (GF_StringBox *)box;
		break;
	}
	return GF_OK;
}

GF_Err vtcu_box_read(GF_Box *s, GF_BitStream *bs)
{
	return gf_isom_box_array_read(s, bs, vtcu_Add);
}

GF_Err vtte_box_read(GF_Box *s, GF_BitStream *bs)
{
	return gf_isom_box_array_read(s, bs, NULL);
}

static GF_Err wvtt_Add(GF_Box *s, GF_Box *box)
{
	GF_WebVTTSampleEntryBox *wvtt = (GF_WebVTTSampleEntryBox *)s;
	switch(box->type) {
	case GF_ISOM_BOX_TYPE_VTTC_CONFIG:
		wvtt->config = (GF_StringBox *)box;
		break;
	}
	return GF_OK;
}

GF_Err wvtt_box_read(GF_Box *s, GF_BitStream *bs)
{
	GF_Err e;
	GF_WebVTTSampleEntryBox *wvtt = (GF_WebVTTSampleEntryBox *)s;
	e = gf_isom_base_sample_entry_read((GF_SampleEntryBox *)wvtt, bs);
	if (e) return e;

	wvtt->size -= 8;
	return gf_isom_box_array_read(s, bs, wvtt_Add);
}

#ifndef GPAC_DISABLE_ISOM_WRITE
GF_Err boxstring_box_write(GF_Box *s, GF_BitStream *bs)
{
	GF_Err e;
	GF_StringBox *box = (GF_StringBox *)s;
	e = gf_isom_box_write_header(s, bs);
	if (e) return e;
	if (box->string) {
		gf_bs_write_data(bs, box->string, (u32)(box->size-8));
	}
	return e;
}

GF_Err vtcu_box_write(GF_Box *s, GF_BitStream *bs)
{
	return gf_isom_box_write_header(s, bs);
}

GF_Err vtte_box_write(GF_Box *s, GF_BitStream *bs)
{
	GF_Err e;
	e = gf_isom_box_write_header(s, bs);
	return e;
}

GF_Err wvtt_box_write(GF_Box *s, GF_BitStream *bs)
{
	GF_Err e;
	GF_WebVTTSampleEntryBox *wvtt = (GF_WebVTTSampleEntryBox *)s;
	e = gf_isom_box_write_header(s, bs);
	gf_bs_write_data(bs, wvtt->reserved, 6);
	gf_bs_write_u16(bs, wvtt->dataReferenceIndex);
	return e;
}

GF_Err boxstring_box_size(GF_Box *s)
{
	GF_StringBox *box = (GF_StringBox *)s;
    if (box->string)
        box->size += strlen(box->string);
	return GF_OK;
}

GF_Err vtcu_box_size(GF_Box *s)
{
	u32 pos=0;
	GF_VTTCueBox *cuebox = (GF_VTTCueBox *)s;
	gf_isom_check_position(s, (GF_Box*)cuebox->id, &pos);
	gf_isom_check_position(s, (GF_Box*)cuebox->time, &pos);
	gf_isom_check_position(s, (GF_Box*)cuebox->settings, &pos);
	gf_isom_check_position(s, (GF_Box*)cuebox->payload, &pos);
	return GF_OK;
}

GF_Err vtte_box_size(GF_Box *s)
{
	return GF_OK;
}

GF_Err wvtt_box_size(GF_Box *s)
{
	u32 pos=0;
	GF_WebVTTSampleEntryBox *wvtt = (GF_WebVTTSampleEntryBox *)s;
	s->size += 8; // reserved and dataReferenceIndex
	gf_isom_check_position(s, (GF_Box *)wvtt->config, &pos);
	return GF_OK;
}

static GF_Err webvtt_write_cue(GF_BitStream *bs, GF_WebVTTCue *cue)
{
	GF_Err e;
	GF_VTTCueBox *cuebox;
	if (!cue) return GF_OK;

	cuebox = (GF_VTTCueBox *)gf_isom_box_new(GF_ISOM_BOX_TYPE_VTCC_CUE);

	if (cue->id) {
		cuebox->id = (GF_StringBox *)boxstring_new_with_data(GF_ISOM_BOX_TYPE_IDEN, cue->id, &cuebox->child_boxes);
	}
	if (cue->settings) {
		cuebox->settings = (GF_StringBox *)boxstring_new_with_data(GF_ISOM_BOX_TYPE_STTG, cue->settings, &cuebox->child_boxes);
	}
	if (cue->text) {
		cuebox->payload = (GF_StringBox *)boxstring_new_with_data(GF_ISOM_BOX_TYPE_PAYL, cue->text, &cuebox->child_boxes);
	}
	/* TODO: check if a time box should be written */
	e = gf_isom_box_size((GF_Box *)cuebox);
	if (!e) e = gf_isom_box_write((GF_Box *)cuebox, bs);

	gf_isom_box_del((GF_Box *)cuebox);
	return e;
}

GF_ISOSample *gf_isom_webvtt_to_sample(void *s)
{
	GF_Err e = GF_OK;
	GF_ISOSample *res;
	GF_BitStream *bs;
	u32 i;
	GF_WebVTTCue *cue;
	GF_WebVTTSample *samp = (GF_WebVTTSample *)s;
	if (!samp) return NULL;

	bs = gf_bs_new(NULL, 0, GF_BITSTREAM_WRITE);

	if (gf_list_count(samp->cues)) {
		i=0;
		while ((cue = (GF_WebVTTCue *)gf_list_enum(samp->cues, &i))) {
			e = webvtt_write_cue(bs, cue);
			if (e) break;
		}
		if (e) {
			gf_bs_del(bs);
			return NULL;
		}
	} else {
		GF_Box *cuebox = (GF_Box *)gf_isom_box_new(GF_ISOM_BOX_TYPE_VTTE);
		e = gf_isom_box_size((GF_Box *)cuebox);
		if (!e) e = gf_isom_box_write((GF_Box *)cuebox, bs);
		gf_isom_box_del((GF_Box *)cuebox);
		if (e) {
			gf_bs_del(bs);
			return NULL;
		}
	}
	res = gf_isom_sample_new();
	if (!res) {
		gf_bs_del(bs);
		return NULL;
	}
	gf_bs_get_content(bs, &res->data, &res->dataLength);
	gf_bs_del(bs);
	res->IsRAP = RAP;
	return res;
}
#endif /*GPAC_DISABLE_ISOM_WRITE*/

#ifndef GPAC_DISABLE_ISOM_DUMP

GF_Err boxstring_box_dump(GF_Box *a, FILE * trace)
{
	char *szName;
	GF_StringBox *sbox = (GF_StringBox *)a;
	switch (a->type) {
	case GF_ISOM_BOX_TYPE_VTTC_CONFIG:
		szName = "WebVTTConfigurationBox";
		break;
	case GF_ISOM_BOX_TYPE_CTIM:
		szName = "CueTimeBox";
		break;
	case GF_ISOM_BOX_TYPE_IDEN:
		szName = "CueIDBox";
		break;
	case GF_ISOM_BOX_TYPE_STTG:
		szName = "CueSettingsBox";
		break;
	case GF_ISOM_BOX_TYPE_PAYL:
		szName = "CuePayloadBox";
		break;
	case GF_ISOM_BOX_TYPE_VTTA:
		szName = "VTTAdditionalCueBox";
		break;
	default:
		szName = "StringBox";
		break;
	}
	gf_isom_box_dump_start(a, szName, trace);
	fprintf(trace, "><![CDATA[\n");
	if (sbox->string)
		fprintf(trace, "%s", sbox->string);
	fprintf(trace, "\n]]>");
	gf_isom_box_dump_done(szName, a, trace);
	return GF_OK;
}

GF_Err vtcu_box_dump(GF_Box *a, FILE * trace)
{
	gf_isom_box_dump_start(a, "WebVTTCueBox", trace);
	fprintf(trace, ">\n");
	gf_isom_box_dump_done("WebVTTCueBox", a, trace);
	return GF_OK;
}

GF_Err vtte_box_dump(GF_Box *a, FILE * trace)
{
	gf_isom_box_dump_start(a, "WebVTTEmptyCueBox", trace);
	fprintf(trace, ">\n");
	gf_isom_box_dump_done("WebVTTEmptyCueBox", a, trace);
	return GF_OK;
}

GF_Err wvtt_box_dump(GF_Box *a, FILE * trace)
{
	gf_isom_box_dump_start(a, "WebVTTSampleEntryBox", trace);
	fprintf(trace, ">\n");
	gf_isom_box_dump_done("WebVTTSampleEntryBox", a, trace);
	return GF_OK;
}
#endif /* GPAC_DISABLE_ISOM_DUMP */

#endif /*GPAC_DISABLE_ISOM*/

typedef enum {
	WEBVTT_PARSER_STATE_WAITING_SIGNATURE,
	WEBVTT_PARSER_STATE_WAITING_HEADER,
	WEBVTT_PARSER_STATE_WAITING_CUE,
	WEBVTT_PARSER_STATE_WAITING_CUE_TIMESTAMP,
	WEBVTT_PARSER_STATE_WAITING_CUE_PAYLOAD
} GF_WebVTTParserState;

struct _webvtt_parser {
	GF_WebVTTParserState state;
	Bool is_srt, suspend, is_eof;

	/* List of non-overlapping GF_WebVTTSample */
	GF_List *samples;

	FILE *vtt_in;
	s32 unicode_type;

	u64  last_duration;
	void *user;
	GF_Err (*report_message)(void *, GF_Err, char *, const char *);
	void (*on_header_parsed)(void *, const char *);
	void (*on_sample_parsed)(void *, GF_WebVTTSample *);
	void (*on_cue_read)(void *, GF_WebVTTCue *);
};

#ifndef GPAC_DISABLE_MEDIA_IMPORT

static Bool gf_webvtt_timestamp_is_zero(GF_WebVTTTimestamp *ts)
{
	return (ts->hour == 0 && ts->min == 0 && ts->sec == 0 && ts->ms == 0) ? GF_TRUE : GF_FALSE;
}
static Bool gf_webvtt_timestamp_greater(GF_WebVTTTimestamp *ts1, GF_WebVTTTimestamp *ts2)
{
	u64 t_ts1 = (60 * 60 * ts1->hour + 60 * ts1->min + ts1->sec) * 1000 + ts1->ms;
	u64 t_ts2 = (60 * 60 * ts2->hour + 60 * ts2->min + ts2->sec) * 1000 + ts2->ms;
	return (t_ts1 >= t_ts2) ? GF_TRUE : GF_FALSE;
}


/* mark the overlapped cue in the previous sample as split */
/* duplicate the cue, mark it as split and adjust its timing */
/* adjust the end of the overlapped cue in the previous sample */
static GF_WebVTTCue *gf_webvtt_cue_split_at(GF_WebVTTCue *cue, GF_WebVTTTimestamp *time)
{
	GF_WebVTTCue *dup_cue;

	cue->split = GF_TRUE;
	cue->orig_start = cue->start;
	cue->orig_end = cue->end;

	GF_SAFEALLOC(dup_cue, GF_WebVTTCue);
	if (!dup_cue) return NULL;
	dup_cue->split = GF_TRUE;
	if (time) dup_cue->start = *time;
	dup_cue->end = cue->end;
	dup_cue->orig_start = cue->orig_start;
	dup_cue->orig_end = cue->orig_end;
	dup_cue->id = gf_strdup((cue->id ? cue->id : ""));
	dup_cue->settings = gf_strdup((cue->settings ? cue->settings : ""));
	dup_cue->text = gf_strdup((cue->text ? cue->text : ""));

	if (time) cue->end = *time;
	return dup_cue;
}

static GF_Err gf_webvtt_cue_add_property(GF_WebVTTCue *cue, GF_WebVTTCuePropertyType type, char *text_data, u32 text_len)
{
	char **prop = NULL;
	u32 len;
	if (!cue) return GF_BAD_PARAM;
	if (!text_len) return GF_OK;
	switch(type)
	{
	case WEBVTT_ID:
		prop = &cue->id;
		break;
	case WEBVTT_SETTINGS:
		prop = &cue->settings;
		break;
	case WEBVTT_PAYLOAD:
		prop = &cue->text;
		break;
	case WEBVTT_POSTCUE_TEXT:
		prop = &cue->post_text;
		break;
	case WEBVTT_PRECUE_TEXT:
		prop = &cue->pre_text;
		break;
	}
	if (*prop) {
		len = (u32) strlen(*prop);
		*prop = (char*)gf_realloc(*prop, sizeof(char) * (len + text_len + 1) );
		strcpy(*prop + len, text_data);
	} else {
		*prop = gf_strdup(text_data);
	}
	return GF_OK;
}

static GF_WebVTTCue *gf_webvtt_cue_new()
{
	GF_WebVTTCue *cue;
	GF_SAFEALLOC(cue, GF_WebVTTCue);
	return cue;
}

GF_EXPORT
void gf_webvtt_cue_del(GF_WebVTTCue * cue)
{
	if (!cue) return;
	if (cue->id) gf_free(cue->id);
	if (cue->settings) gf_free(cue->settings);
	if (cue->text) gf_free(cue->text);
	if (cue->pre_text) gf_free(cue->pre_text);
	if (cue->post_text) gf_free(cue->post_text);
	gf_free(cue);
}

static GF_WebVTTSample *gf_webvtt_sample_new()
{
	GF_WebVTTSample *samp;
	GF_SAFEALLOC(samp, GF_WebVTTSample);
	if (!samp) return NULL;
	samp->cues = gf_list_new();
	return samp;
}

u64 gf_webvtt_sample_get_start(GF_WebVTTSample * samp)
{
	return samp->start;
}
u64 gf_webvtt_sample_get_end(GF_WebVTTSample * samp)
{
	return samp->end;
}

void gf_webvtt_sample_del(GF_WebVTTSample * samp)
{
	while (gf_list_count(samp->cues)) {
		GF_WebVTTCue *cue = (GF_WebVTTCue *)gf_list_get(samp->cues, 0);
		gf_list_rem(samp->cues, 0);
		gf_webvtt_cue_del(cue);
	}
	gf_list_del(samp->cues);
	gf_free(samp);
}

GF_WebVTTParser *gf_webvtt_parser_new()
{
	GF_WebVTTParser *parser;
	GF_SAFEALLOC(parser, GF_WebVTTParser);
	if (!parser) return NULL;
	parser->samples = gf_list_new();
	return parser;
}

extern s32 gf_text_get_utf_type(FILE *in_src);

GF_Err gf_webvtt_parser_init(GF_WebVTTParser *parser, FILE *vtt_file, s32 unicode_type, Bool is_srt,
                             void *user, GF_Err (*report_message)(void *, GF_Err, char *, const char *),
                             void (*on_sample_parsed)(void *, GF_WebVTTSample *),
                             void (*on_header_parsed)(void *, const char *))
{
	if (!parser) return GF_BAD_PARAM;
	parser->state = WEBVTT_PARSER_STATE_WAITING_SIGNATURE;

#ifdef GPAC_ENABLE_COVERAGE
	if (gf_sys_is_test_mode()) {
		gf_webvtt_parser_restart(parser);
	}
#endif

	parser->is_srt = is_srt;
	if (is_srt)
		parser->state = WEBVTT_PARSER_STATE_WAITING_CUE;

	parser->vtt_in = vtt_file;
	parser->unicode_type = unicode_type;

	parser->user = user;
	parser->report_message = report_message;
	parser->on_sample_parsed = on_sample_parsed;
	parser->on_header_parsed = on_header_parsed;
	return GF_OK;
}

void gf_webvtt_parser_suspend(GF_WebVTTParser *vttparser)
{
	vttparser->suspend = GF_TRUE;
}

void gf_webvtt_parser_restart(GF_WebVTTParser *parser)
{
	if (!parser->vtt_in) return;

	gf_fseek(parser->vtt_in, 0, SEEK_SET);
	parser->last_duration = 0;
	while (gf_list_count(parser->samples)) {
		gf_webvtt_sample_del((GF_WebVTTSample *)gf_list_get(parser->samples, 0));
		gf_list_rem(parser->samples, 0);
	}
	parser->state = WEBVTT_PARSER_STATE_WAITING_SIGNATURE;
}

void gf_webvtt_parser_reset(GF_WebVTTParser *parser)
{
	if (!parser) return;
	while (gf_list_count(parser->samples)) {
		gf_webvtt_sample_del((GF_WebVTTSample *)gf_list_get(parser->samples, 0));
		gf_list_rem(parser->samples, 0);
	}
	parser->last_duration = 0;
	parser->on_header_parsed = NULL;
	parser->on_sample_parsed = NULL;
	parser->report_message = NULL;
	parser->state = WEBVTT_PARSER_STATE_WAITING_SIGNATURE;
	parser->unicode_type = 0;
	parser->user = NULL;
	parser->vtt_in = NULL;
}

void gf_webvtt_parser_del(GF_WebVTTParser *parser)
{
	if (parser) {
		gf_webvtt_parser_reset(parser);
		gf_list_del(parser->samples);
		gf_free(parser);
	}
}

#if 0
u64 gf_webvtt_parser_last_duration(GF_WebVTTParser *parser)
{
	return parser ? parser->last_duration : 0;
}
#endif


static GF_Err gf_webvtt_add_cue_to_samples(GF_WebVTTParser *parser, GF_List *samples, GF_WebVTTCue *cue)
{
	s32 i;
	u64 cue_start;
	u64 cue_end;
	u64 sample_end;

	sample_end = 0;
	cue_start = gf_webvtt_timestamp_get(&cue->start);
	cue_end   = gf_webvtt_timestamp_get(&cue->end);
	/* samples in the samples list are contiguous: sample(n)->start == sample(n-1)->end */
	for (i = 0; i < (s32)gf_list_count(samples); i++) {
		GF_WebVTTSample *sample;
		sample = (GF_WebVTTSample *)gf_list_get(samples, i);
		/* save the sample end in case there are no more samples to test */
		sample_end = sample->end;
		if (cue_start < sample->start)
		{
			/* cues must be ordered according to their start time, so drop the cue */
			/* TODO delete the cue */
			return GF_BAD_PARAM;
		}
		else if (cue_start == sample->start && cue_end == sample->end)
		{
			/* if the timing of the new cue matches the sample, no need to split, add the cue to the sample */
			gf_list_add(sample->cues, cue);
			/* the cue does not need to processed further */
			return GF_OK;
		}
		else if (cue_start >= sample->end)
		{
			/* flush the current sample */
			gf_list_del_item(samples, sample);
			parser->on_sample_parsed(parser->user, sample);
			sample = NULL;
			i--;
			/* process the cue with next sample (if any) or create a new sample */
			continue;
		}
		else if (cue_start >= sample->start)
		{
			u32 j;
			if (cue_start > sample->start) {
				/* create a new sample, insert it after the current one */
				GF_WebVTTSample *new_sample = gf_webvtt_sample_new();
				new_sample->start = cue_start;
				new_sample->end = sample->end;
				gf_list_insert(samples, new_sample, i+1);
				/* split the cues from the old sample into the new one */
				for (j = 0; j < gf_list_count(sample->cues); j++) {
					GF_WebVTTCue *old_cue = (GF_WebVTTCue *)gf_list_get(sample->cues, j);
					GF_WebVTTCue *new_cue = gf_webvtt_cue_split_at(old_cue, &cue->start);
					gf_list_add(new_sample->cues, new_cue);
				}
				/* adjust the end of the old sample and flush it */
				sample->end = cue_start;
				gf_list_del_item(samples, sample);
				parser->on_sample_parsed(parser->user, sample);
				sample = NULL;
				i--;
				/* process the cue again with this new sample */
				continue;
			}
			if (cue_end > sample->end) {
				/* the cue is longer than the sample, we split the cue, add one part to the current sample
				and reevaluate with the last part of the cue */
				GF_WebVTTCue *old_cue = (GF_WebVTTCue *)gf_list_get(sample->cues, 0);
				GF_WebVTTCue *new_cue = gf_webvtt_cue_split_at(cue, &old_cue->end);
				gf_list_add(sample->cues, cue);
				cue = new_cue;
				cue_start = sample->end;
				/* cue_end unchanged */
				/* process the remaining part of the cue (i.e. the new cue) with the other samples */
				continue;
			} else { /* cue_end < sample->end */
				GF_WebVTTSample *new_sample = gf_webvtt_sample_new();
				new_sample->start = cue_end;
				new_sample->end   = sample->end;
				gf_list_insert(samples, new_sample, i+1);
				for (j = 0; j < gf_list_count(sample->cues); j++) {
					GF_WebVTTCue *old_cue = (GF_WebVTTCue *)gf_list_get(sample->cues, j);
					GF_WebVTTCue *new_cue = gf_webvtt_cue_split_at(old_cue, &cue->end);
					gf_list_add(new_sample->cues, new_cue);
				}
				gf_list_add(sample->cues, cue);
				sample->end = new_sample->start;
				/* done with this cue */
				return GF_OK;
			}
		}
	}
	/* (a part of) the cue remains (was not overlapping) */
	if (cue_start > sample_end) {
		/* if the new cue start is greater than the last sample end,
		    create an empty sample to fill the gap, flush it */
		GF_WebVTTSample *esample = gf_webvtt_sample_new();
		esample->start = sample_end;
		esample->end   = cue_start;
		parser->on_sample_parsed(parser->user, esample);
	}
	/* if the cue has not been added to a sample, create a new sample for it */
	{
		GF_WebVTTSample *sample;
		sample = gf_webvtt_sample_new();
		gf_list_add(samples, sample);
		sample->start = cue_start;
		sample->end = cue_end;
		gf_list_add(sample->cues, cue);
	}
	return GF_OK;
}

#define REM_TRAIL_MARKS(__str, __sep) while (1) {	\
		u32 _len = (u32) strlen(__str);		\
		if (!_len) break;	\
		_len--;				\
		if (strchr(__sep, __str[_len])) { \
			had_marks = GF_TRUE; \
			__str[_len] = 0;	\
		} else break;	\
	}

extern char *gf_text_get_utf8_line(char *szLine, u32 lineSize, FILE *txt_in, s32 unicode_type);

GF_Err gf_webvtt_parse_timestamp(GF_WebVTTParser *parser, GF_WebVTTTimestamp *ts, const char *line)
{
	u32 len;
	u32 pos;
	u32 pos2;
	u32 value1;
	u32 value2;
	u32 value3;
	u32 value4;
	Bool is_hour = GF_FALSE;
	if (!ts || !line) return GF_BAD_PARAM;
	len = (u32) strlen(line);
	if (!len) return GF_BAD_PARAM;
	pos = 0;
	if (!(line[pos] >= '0' && line[pos] <= '9')) return GF_BAD_PARAM;
	value1 = 0;
	while (pos < len && line[pos] >= '0' && line[pos] <= '9') {
		value1 = value1*10 + (line[pos]-'0');
		pos++;
	}
	if (pos>2 || value1>59) {
		is_hour = GF_TRUE;
	}
	if (pos == len || line[pos] != ':') {
		return GF_BAD_PARAM;
	} else {
		pos++;
	}
	value2 = 0;
	pos2 = 0;
	while (pos < len && line[pos] >= '0' && line[pos] <= '9') {
		value2 = value2*10 + (line[pos]-'0');
		pos++;
		pos2++;
		if (pos2 > 2) return GF_BAD_PARAM;
	}
	if (is_hour || (pos < len && line[pos] == ':')) {
		if (pos == len || line[pos] != ':') {
			return GF_BAD_PARAM;
		} else {
			pos++;
			pos2 = 0;
			value3 = 0;
			while (pos < len && line[pos] >= '0' && line[pos] <= '9') {
				value3 = value3*10 + (line[pos]-'0');
				pos++;
				pos2++;
				if (pos2 > 2) return GF_BAD_PARAM;
			}
		}
	} else {
		value3 = value2;
		value2 = value1;
		value1 = 0;
	}
	/* checking SRT syntax for timestamp with , */
	if (pos == len || (!parser->is_srt && line[pos] != '.') || (parser->is_srt && line[pos] != ',')) {
		return GF_BAD_PARAM;
	} else {
		pos++;
	}
	pos2 = 0;
	value4 = 0;
	while (pos < len && line[pos] >= '0' && line[pos] <= '9') {
		value4 = value4*10 + (line[pos]-'0');
		pos++;
		pos2++;
		if (pos2 > 4) return GF_BAD_PARAM;
	}
	if (value2>59 || value3 > 59) return GF_BAD_PARAM;
	ts->hour = value1;
	ts->min = value2;
	ts->sec = value3;
	ts->ms = value4;
	return GF_OK;
}

#define SKIP_WHITESPACE \
    while (pos < len && (line[pos] == ' ' || line[pos] == '\t' || \
           line[pos] == '\r' || line[pos] == '\f' || line[pos] == '\n')) pos++;

GF_Err gf_webvtt_parser_parse_timings_settings(GF_WebVTTParser *parser, GF_WebVTTCue *cue, char *line, u32 len)
{
	GF_Err e;
	char *timestamp_string;
	u32 pos;

	pos = 0;
	if (!cue || !line || !len) return GF_BAD_PARAM;
	SKIP_WHITESPACE
	timestamp_string = line + pos;
	while (pos < len && line[pos] != ' ' && line[pos] != '\t') pos++;
	if (pos == len) {
		e = GF_CORRUPTED_DATA;
		parser->report_message(parser->user, e, "Error scanning WebVTT cue timing in %s", line);
		return e;
	}
	line[pos] = 0;
	e = gf_webvtt_parse_timestamp(parser, &cue->start, timestamp_string);
	if (e) {
		parser->report_message(parser->user, e, "Bad VTT timestamp formatting %s", timestamp_string);
		return e;
	}
	line[pos] = ' ';
	SKIP_WHITESPACE
	if (pos == len) {
		e = GF_CORRUPTED_DATA;
		parser->report_message(parser->user, e, "Error scanning WebVTT cue timing in %s", line);
		return e;
	}
	if ( (pos+2)>= len || line[pos] != '-' || line[pos+1] != '-' || line[pos+2] != '>') {
		e = GF_CORRUPTED_DATA;
		parser->report_message(parser->user, e, "Error scanning WebVTT cue timing in %s", line);
		return e;
	} else {
		pos += 3;
		SKIP_WHITESPACE
		if (pos == len) {
			e = GF_CORRUPTED_DATA;
			parser->report_message(parser->user, e, "Error scanning WebVTT cue timing in %s", line);
			return e;
		}
		timestamp_string = line + pos;
		while (pos < len && line[pos] != ' ' && line[pos] != '\t') pos++;
		if (pos < len) {
			line[pos] = 0;
		}
		e = gf_webvtt_parse_timestamp(parser, &cue->end, timestamp_string);
		if (e) {
			parser->report_message(parser->user, e, "Bad VTT timestamp formatting %s", timestamp_string);
			return e;
		}
		if (pos < len) {
			line[pos] = ' ';
		}
		SKIP_WHITESPACE
		if (pos < len) {
			char *settings = line + pos;
			e = gf_webvtt_cue_add_property(cue, WEBVTT_SETTINGS, settings, (u32) strlen(settings));
		}

		if (!gf_webvtt_timestamp_greater(&cue->end, &cue->start)) {
			parser->report_message(parser->user, e, "Bad VTT timestamps, end smaller than start", timestamp_string);
			cue->end = cue->start;
			cue->end.ms += 1;
			return GF_NON_COMPLIANT_BITSTREAM;

		}
	}
	return e;
}

GF_Err gf_webvtt_parser_parse(GF_WebVTTParser *parser)
{
	char szLine[2048];
	char *sOK;
	u32 len;
	GF_Err e;
	GF_WebVTTCue *cue = NULL;
	char *prevLine = NULL;
	char *header = NULL;
	u32 header_len = 0;
	Bool had_marks = GF_FALSE;

	if (!parser) return GF_BAD_PARAM;
	parser->suspend = GF_FALSE;

	if (parser->is_srt) {
		parser->on_header_parsed(parser->user, "WEBVTT\n");
	}

	while (!parser->is_eof) {
		if (!cue && parser->suspend)
			break;
		sOK = gf_text_get_utf8_line(szLine, 2048, parser->vtt_in, parser->unicode_type);
		REM_TRAIL_MARKS(szLine, "\r\n")
		len = (u32) strlen(szLine);
		switch (parser->state) {
		case WEBVTT_PARSER_STATE_WAITING_SIGNATURE:
			if (!sOK || len < 6 || strnicmp(szLine, "WEBVTT", 6) || (len > 6 && szLine[6] != ' ' && szLine[6] != '\t')) {
				e = GF_CORRUPTED_DATA;
				parser->report_message(parser->user, e, "Bad WEBVTT file signature %s", szLine);
				goto exit;
			} else {
				if (had_marks) {
					szLine[len] = '\n';
					len++;
				}
				header = gf_strdup(szLine);
				header_len = len;
				parser->state = WEBVTT_PARSER_STATE_WAITING_HEADER;
			}
			break; /* proceed to next line */
		case WEBVTT_PARSER_STATE_WAITING_HEADER:
			if (prevLine) {
				u32 prev_len = (u32) strlen(prevLine);
				header = (char *)gf_realloc(header, header_len + prev_len + 1);
				strcpy(header+header_len,prevLine);
				header_len += prev_len;
				gf_free(prevLine);
				prevLine = NULL;
			}
			if (sOK && len) {
				if (strstr(szLine, "-->")) {
					parser->on_header_parsed(parser->user, header);
					/* continue to the next state without breaking */
					parser->state = WEBVTT_PARSER_STATE_WAITING_CUE_TIMESTAMP;
					/* no break, continue to the next state*/
				} else {
					if (had_marks) {
						szLine[len] = '\n';
						len++;
					}
					prevLine = gf_strdup(szLine);
					break; /* proceed to next line */
				}
			} else {
				parser->on_header_parsed(parser->user, header);
				if (header) gf_free(header);
				header = NULL;
				if (!sOK) {
					/* end of file, parsing is done */
					parser->is_eof = GF_TRUE;
					break;
				} else {
					/* empty line means end of header */
					parser->state = WEBVTT_PARSER_STATE_WAITING_CUE;
					/* no break, continue to the next state*/
				}
			}
		case WEBVTT_PARSER_STATE_WAITING_CUE:
			if (sOK && len) {
				if (strstr(szLine, "-->")) {
					parser->state = WEBVTT_PARSER_STATE_WAITING_CUE_TIMESTAMP;
					/* continue to the next state without breaking */
				} else {
					/* discard the previous line */
					/* should we do something with it ? callback ?*/
					if (prevLine) {
						gf_free(prevLine);
						prevLine = NULL;
					}
					/* save this new line */
					if (had_marks) {
						szLine[len] = '\n';
						len++;
					}
					prevLine = gf_strdup(szLine);
					/* stay in the same state */
					break;
				}
			} else {
				/* discard the previous line */
				/* should we do something with it ? callback ?*/
				if (prevLine) {
					gf_free(prevLine);
					prevLine = NULL;
				}
				if (!sOK) {
					parser->is_eof = GF_TRUE;
					break;
				} else {
					/* remove empty lines and stay in the same state */
					break;
				}
			}
		case WEBVTT_PARSER_STATE_WAITING_CUE_TIMESTAMP:
			if (sOK && len) {
				if (cue == NULL) {
					cue   = gf_webvtt_cue_new();
				}
				if (prevLine) {
					gf_webvtt_cue_add_property(cue, WEBVTT_ID, prevLine, (u32) strlen(prevLine));
					gf_free(prevLine);
					prevLine = NULL;
				}
				e = gf_webvtt_parser_parse_timings_settings(parser, cue, szLine, len);
				if (e) {
					if (cue) gf_webvtt_cue_del(cue);
					cue = NULL;
					parser->state = WEBVTT_PARSER_STATE_WAITING_CUE;
				} else {
//					start = (u32)gf_webvtt_timestamp_get(&cue->start);
//					end   = (u32)gf_webvtt_timestamp_get(&cue->end);
					parser->state = WEBVTT_PARSER_STATE_WAITING_CUE_PAYLOAD;
				}
			} else {
				/* not possible */
				assert(0);
			}
			break;
		case WEBVTT_PARSER_STATE_WAITING_CUE_PAYLOAD:
			if (sOK && len) {
				if (had_marks) {
					szLine[len] = '\n';
					len++;
				}
				gf_webvtt_cue_add_property(cue, WEBVTT_PAYLOAD, szLine, len);
				/* remain in the same state as a cue payload can have multiple lines */
				break;
			} else {
				/* end of the current cue */
				gf_webvtt_add_cue_to_samples(parser, parser->samples, cue);
				cue = NULL;

				if (!sOK) {
					parser->is_eof = GF_TRUE;
					break;
				} else {
					/* empty line, move to next cue */
					parser->state = WEBVTT_PARSER_STATE_WAITING_CUE;
					break;
				}
			}
		}
	}
	if (header) gf_free(header);
	header = NULL;

	if (parser->suspend)
		return GF_OK;

	/* no more cues to come, flush everything */
	if (cue) {
		gf_webvtt_add_cue_to_samples(parser, parser->samples, cue);
		cue = NULL;
	}
	while (gf_list_count(parser->samples) > 0) {
		GF_WebVTTSample *sample = (GF_WebVTTSample *)gf_list_get(parser->samples, 0);
		parser->last_duration = (sample->end > sample->start) ? sample->end - sample->start : 0;
		gf_list_rem(parser->samples, 0);
		parser->on_sample_parsed(parser->user, sample);
	}
	e = GF_EOS;
exit:
	if (cue) gf_webvtt_cue_del(cue);
	if (prevLine) gf_free(prevLine);
	if (header) gf_free(header);
	return e;
}

GF_EXPORT
GF_List *gf_webvtt_parse_iso_cues(GF_ISOSample *iso_sample, u64 start)
{
	return gf_webvtt_parse_cues_from_data(iso_sample->data, iso_sample->dataLength, start);
}

GF_EXPORT
GF_List *gf_webvtt_parse_cues_from_data(const u8 *data, u32 dataLength, u64 start)
{
	GF_List *cues;
	GF_WebVTTCue *cue;
	GF_VTTCueBox *cuebox;
	GF_BitStream *bs;
	char *pre_text;
	cue = NULL;
	pre_text = NULL;
	cues = gf_list_new();
	bs = gf_bs_new((u8 *)data, dataLength, GF_BITSTREAM_READ);
	while(gf_bs_available(bs))
	{
		GF_Err  e;
		GF_Box  *box;
		e = gf_isom_box_parse(&box, bs);
		if (e) return NULL;
		if (box->type == GF_ISOM_BOX_TYPE_VTCC_CUE) {
			cuebox = (GF_VTTCueBox *)box;
			cue   = gf_webvtt_cue_new();
			if (pre_text) {
				gf_webvtt_cue_add_property(cue, WEBVTT_PRECUE_TEXT, pre_text, (u32) strlen(pre_text));
				gf_free(pre_text);
				pre_text = NULL;
			}
			gf_list_add(cues, cue);
			gf_webvtt_timestamp_set(&cue->start, start);
			if (cuebox->id) {
				gf_webvtt_cue_add_property(cue, WEBVTT_ID, cuebox->id->string, (u32) strlen(cuebox->id->string));
			}
			if (cuebox->settings) {
				gf_webvtt_cue_add_property(cue, WEBVTT_SETTINGS, cuebox->settings->string, (u32) strlen(cuebox->settings->string));
			}
			if (cuebox->payload) {
				gf_webvtt_cue_add_property(cue, WEBVTT_PAYLOAD, cuebox->payload->string, (u32) strlen(cuebox->payload->string));
			}
		} else if (box->type == GF_ISOM_BOX_TYPE_VTTA) {
			GF_StringBox *sbox = (GF_StringBox *)box;
			if (cue) {
				gf_webvtt_cue_add_property(cue, WEBVTT_POSTCUE_TEXT, sbox->string, (u32) strlen(sbox->string));
			} else {
				pre_text = gf_strdup(sbox->string);
			}
		}
		gf_isom_box_del(box);
	}
	gf_bs_del(bs);
	return cues;
}

GF_Err gf_webvtt_merge_cues(GF_WebVTTParser *parser, u64 start, GF_List *cues)
{
	GF_WebVTTSample *wsample;
	GF_WebVTTSample *prev_wsample;
	Bool            has_continuation_cue = GF_FALSE;

	assert(gf_list_count(parser->samples) <= 1);

	wsample = gf_webvtt_sample_new();
	wsample->start = start;

	prev_wsample = (GF_WebVTTSample *)gf_list_last(parser->samples);
	while (gf_list_count(cues)) {
		GF_WebVTTCue *cue = (GF_WebVTTCue *)gf_list_get(cues, 0);
		gf_list_rem(cues, 0);
		/* add the cue to the current sample */
		gf_list_add(wsample->cues, cue);
		/* update with the previous sample */
		if (prev_wsample) {
			Bool do_del = GF_TRUE;
			Bool  found = GF_FALSE;
			while (!found && gf_list_count(prev_wsample->cues)) {
				GF_WebVTTCue *old_cue = (GF_WebVTTCue *)gf_list_get(prev_wsample->cues, 0);
				gf_list_rem(prev_wsample->cues, 0);
				if (
				    ((!cue->id && !old_cue->id) || (old_cue->id && cue->id && !strcmp(old_cue->id, cue->id))) &&
				    ((!cue->settings && !old_cue->settings) || (old_cue->settings && cue->settings && !strcmp(old_cue->settings, cue->settings))) &&
				    ((!cue->text && !old_cue->text) || (old_cue->text && cue->text && !strcmp(old_cue->text, cue->text)))
				) {
					/* if it is the same cue, update its start with the initial start */
					cue->start = old_cue->start;
					has_continuation_cue = GF_TRUE;
					found = GF_TRUE;
					if (old_cue->pre_text) {
						cue->pre_text = old_cue->pre_text;
						old_cue->pre_text = NULL;
					}
					if (old_cue->post_text) {
						cue->post_text = old_cue->post_text;
						old_cue->post_text = NULL;
					}
				} else {
					/* finalize the end cue time */
					if (gf_webvtt_timestamp_is_zero(&old_cue->end)) {
						gf_webvtt_timestamp_set(&old_cue->end, wsample->start);
					}
					/* transfer the cue */
					if (!has_continuation_cue) {
						/* the cue can be safely serialized while keeping the order */
						parser->on_cue_read(parser->user, old_cue);
					} else {
						/* keep the cue in the current sample to respect cue start ordering */
						gf_list_add(wsample->cues, old_cue);
						do_del = GF_FALSE;
					}
				}
				/* delete the old cue */
				if (do_del)
					gf_webvtt_cue_del(old_cue);
			}
		}
	}
	/* No cue in the current sample */
	if (prev_wsample) {
		while (gf_list_count(prev_wsample->cues)) {
			Bool do_del = GF_TRUE;
			GF_WebVTTCue *cue = (GF_WebVTTCue *)gf_list_get(prev_wsample->cues, 0);
			gf_list_rem(prev_wsample->cues, 0);
			/* finalize the end cue time */
			if (gf_webvtt_timestamp_is_zero(&cue->end)) {
				gf_webvtt_timestamp_set(&cue->end, wsample->start);
			}
			/* transfer the cue */
			if (!has_continuation_cue) {
				/* the cue can be safely serialized while keeping the order */
				parser->on_cue_read(parser->user, cue);
			} else {
				/* keep the cue in the current sample to respect cue start ordering */
				gf_list_add(wsample->cues, cue);
				do_del = GF_FALSE;
			}
			if (do_del)
				gf_webvtt_cue_del(cue);
		}
		gf_webvtt_sample_del(prev_wsample);
		gf_list_rem_last(parser->samples);
		prev_wsample = NULL;
	} else {
		/* nothing to do */
	}
	if (gf_list_count(wsample->cues)) {
		gf_list_add(parser->samples, wsample);
	} else {
		gf_webvtt_sample_del(wsample);
	}
	return GF_OK;
}

GF_Err gf_webvtt_parse_iso_sample(GF_WebVTTParser *parser, u32 timescale, GF_ISOSample *iso_sample, Bool merge, Bool box_mode)
{
	if (merge) {
		u64             start;
		GF_List         *cues;
		start = (iso_sample->DTS * 1000) / timescale;
		cues = gf_webvtt_parse_iso_cues(iso_sample, start);
		gf_webvtt_merge_cues(parser, start, cues);
		gf_list_del(cues);
	} else {
		GF_Err gf_webvtt_dump_iso_sample(FILE *dump, u32 timescale, GF_ISOSample *iso_sample, Bool box_mode);

		gf_webvtt_dump_iso_sample((FILE *)parser->user, timescale, iso_sample, box_mode);
	}

	return GF_OK;
}
#endif //GPAC_DISABLE_MEDIA_IMPORT


void gf_webvtt_timestamp_set(GF_WebVTTTimestamp *ts, u64 value)
{
	u64 tmp;
	if (!ts) return;
	tmp = value;
	ts->hour = (u32)(tmp/(3600*1000));
	tmp -= ts->hour*3600*1000;
	ts->min  = (u32)(tmp/(60*1000));
	tmp -= ts->min*60*1000;
	ts->sec  = (u32)(tmp/1000);
	tmp -= ts->sec*1000;
	ts->ms   = (u32)tmp;
}

u64 gf_webvtt_timestamp_get(GF_WebVTTTimestamp *ts)
{
	if (!ts) return 0;
	return (3600*ts->hour + 60*ts->min + ts->sec)*1000 + ts->ms;
}

void gf_webvtt_timestamp_dump(GF_WebVTTTimestamp *ts, FILE *dump, Bool dump_hour)
{
	if (dump_hour || ts->hour != 0) {
		fprintf(dump, "%02u:", ts->hour);
	}

	fprintf(dump, "%02u:%02u.%03u", ts->min, ts->sec, ts->ms);
}
GF_Err gf_webvtt_dump_header_boxed(FILE *dump, const u8 *data, u32 dataLength, u32 *dumpedLength)
{
#ifdef GPAC_DISABLE_ISOM
	return GF_NOT_SUPPORTED;
#else
	GF_Err e;
	GF_Box *box;
	GF_StringBox *config;
	GF_BitStream *bs;
	*dumpedLength = 0;
	bs = gf_bs_new((u8 *)data, dataLength, GF_BITSTREAM_READ);
	e = gf_isom_box_parse(&box, bs);
	if (!box || (box->type != GF_ISOM_BOX_TYPE_VTTC_CONFIG)) {
		gf_bs_del(bs);
		if (box) gf_isom_box_del(box);
		return GF_BAD_PARAM;
	}
	config = (GF_StringBox *)box;
	if (config->string) {
		fprintf(dump, "%s", config->string);
		*dumpedLength = (u32)strlen(config->string)+1;
	}
	gf_bs_del(bs);
	gf_isom_box_del(box);
	return e;
#endif
}

#ifndef GPAC_DISABLE_ISOM
GF_Err gf_webvtt_dump_header(FILE *dump, GF_ISOFile *file, u32 track, Bool box_mode, u32 index)
{
	GF_WebVTTSampleEntryBox *wvtt;
	wvtt = gf_webvtt_isom_get_description(file, track, index);
	if (!wvtt) return GF_BAD_PARAM;
	if (box_mode) {
		gf_isom_box_dump(wvtt, dump);
	} else {
		fprintf(dump, "%s\n\n", wvtt->config->string);
	}
	return GF_OK;
}

GF_Err gf_webvtt_dump_iso_sample(FILE *dump, u32 timescale, GF_ISOSample *iso_sample, Bool box_mode)
{
	GF_Err e;
	GF_BitStream *bs;

	if (box_mode) {
		fprintf(dump, "<WebVTTSample decodingTimeStamp=\""LLU"\" compositionTimeStamp=\""LLD"\" RAP=\"%d\" dataLength=\"%d\" >\n", iso_sample->DTS, (s64)iso_sample->DTS + iso_sample->CTS_Offset, iso_sample->IsRAP, iso_sample->dataLength);
	}
	bs = gf_bs_new(iso_sample->data, iso_sample->dataLength, GF_BITSTREAM_READ);
	while(gf_bs_available(bs))
	{
		GF_Box *box;
		GF_WebVTTTimestamp ts;
		e = gf_isom_box_parse(&box, bs);
		if (e) return e;

		if (box_mode) {
			gf_isom_box_dump(box, dump);
		} else if (box->type == GF_ISOM_BOX_TYPE_VTCC_CUE) {
			GF_VTTCueBox *cuebox = (GF_VTTCueBox *)box;
			if (cuebox->id) fprintf(dump, "%s", cuebox->id->string);
			gf_webvtt_timestamp_set(&ts, (iso_sample->DTS * 1000) / timescale);
			gf_webvtt_timestamp_dump(&ts, dump, GF_FALSE);
			fprintf(dump, " --> NEXT");
			if (cuebox->settings) fprintf(dump, " %s", cuebox->settings->string);
			fprintf(dump, "\n");
			if (cuebox->payload) fprintf(dump, "%s", cuebox->payload->string);
			fprintf(dump, "\n");
		} else if (box->type == GF_ISOM_BOX_TYPE_VTTE) {
			gf_webvtt_timestamp_set(&ts, (iso_sample->DTS * 1000) / timescale);
			gf_webvtt_timestamp_dump(&ts, dump, GF_FALSE);
			fprintf(dump, " --> NEXT\n\n");
		} else if (box->type == GF_ISOM_BOX_TYPE_VTTA) {
			fprintf(dump, "%s\n\n", ((GF_StringBox *)box)->string);
		}
		gf_isom_box_del(box);
	}
	gf_bs_del(bs);
	if (box_mode) {
		fprintf(dump, "</WebVTTSample>\n");
	}
	return GF_OK;
}
#endif

#ifndef GPAC_DISABLE_ISOM_DUMP
GF_Err gf_webvtt_parser_finalize(GF_WebVTTParser *parser, u64 duration)
{
	GF_WebVTTSample *sample;
	assert(gf_list_count(parser->samples) <= 1);
	sample = (GF_WebVTTSample *)gf_list_get(parser->samples, 0);
	if (sample) {
		while (gf_list_count(sample->cues)) {
			GF_WebVTTCue *cue = (GF_WebVTTCue *)gf_list_get(sample->cues, 0);
			gf_list_rem(sample->cues, 0);
			if (gf_webvtt_timestamp_is_zero(&cue->end)) {
				gf_webvtt_timestamp_set(&cue->end, duration);
			}
			parser->on_cue_read(parser->user, cue);
			gf_webvtt_cue_del(cue);
		}
		gf_webvtt_sample_del(sample);
		gf_list_rem(parser->samples, 0);
	}
	return GF_OK;
}

static void gf_webvtt_dump_cue(void *user, GF_WebVTTCue *cue)
{
	FILE *dump = (FILE *)user;
	if (!cue || !dump) return;
	if (cue->pre_text) {
		fprintf(dump, "%s", cue->pre_text);
		fprintf(dump, "\n");
		fprintf(dump, "\n");
	}
	if (cue->id) fprintf(dump, "%s\n", cue->id);
	if (cue->start.hour || cue->end.hour) {
		gf_webvtt_timestamp_dump(&cue->start, dump, GF_TRUE);
		fprintf(dump, " --> ");
		gf_webvtt_timestamp_dump(&cue->end, dump, GF_TRUE);
	} else {
		gf_webvtt_timestamp_dump(&cue->start, dump, GF_FALSE);
		fprintf(dump, " --> ");
		gf_webvtt_timestamp_dump(&cue->end, dump, GF_FALSE);
	}
	if (cue->settings) {
		fprintf(dump, " %s", cue->settings);
	}
	fprintf(dump, "\n");
	if (cue->text) fprintf(dump, "%s", cue->text);
	fprintf(dump, "\n");
	fprintf(dump, "\n");
	if (cue->post_text) {
		fprintf(dump, "%s", cue->post_text);
		fprintf(dump, "\n");
		fprintf(dump, "\n");
	}
}

//unused
#if 0
static GF_Err gf_webvtt_dump_cues(FILE *dump, GF_List *cues)
{
	u32 i;
	for (i = 0; i < gf_list_count(cues); i++) {
		GF_WebVTTCue *cue = (GF_WebVTTCue *)gf_list_get(cues, i);
		gf_webvtt_dump_cue(dump, cue);
	}
	return GF_OK;
}

GF_Err gf_webvtt_dump_sample(FILE *dump, GF_WebVTTSample *samp)
{
	fprintf(stdout, "NOTE New WebVTT Sample ("LLD"-"LLD")\n\n", samp->start, samp->end);
	return gf_webvtt_dump_cues(dump, samp->cues);
}
#endif


void gf_webvtt_parser_cue_callback(GF_WebVTTParser *parser, void (*on_cue_read)(void *, GF_WebVTTCue *), void *udta)
{
	parser->on_cue_read = on_cue_read;
	parser->user = udta;
}

#ifndef GPAC_DISABLE_MEDIA_EXPORT

GF_EXPORT
GF_Err gf_webvtt_dump_iso_track(GF_MediaExporter *dumper, char *szName, u32 track, Bool merge, Bool box_dump)
{
#ifdef GPAC_DISABLE_MEDIA_IMPORT
	return GF_NOT_SUPPORTED;
#else
	GF_Err  e;
	u32     i;
	u32     count;
	u32     timescale;
	FILE    *out;
	u32     di;
	u64     duration;
	GF_WebVTTParser *parser;

	out = szName ? gf_fopen(szName, "wt") : (dumper->dump_file ? dumper->dump_file : stdout);
	if (!out) return GF_IO_ERR;// gf_export_message(dumper, GF_IO_ERR, "Error opening %s for writing - check disk access & permissions", szName);

	parser = gf_webvtt_parser_new();
	parser->user = out;
	parser->on_cue_read = gf_webvtt_dump_cue;

	if (box_dump)
		fprintf(out, "<WebVTTTrack trackID=\"%d\">\n", gf_isom_get_track_id(dumper->file, track) );

	e = gf_webvtt_dump_header(out, dumper->file, track, box_dump, 1);
	if (e) goto exit;

	timescale = gf_isom_get_media_timescale(dumper->file, track);

	count = gf_isom_get_sample_count(dumper->file, track);
	for (i=0; i<count; i++) {
		GF_ISOSample *samp = gf_isom_get_sample(dumper->file, track, i+1, &di);
		if (!samp) {
			e = gf_isom_last_error(dumper->file);
			goto exit;
		}
		e = gf_webvtt_parse_iso_sample(parser, timescale, samp, merge, box_dump);
		if (e) {
			goto exit;
		}
		gf_isom_sample_del(&samp);
	}
	duration = gf_isom_get_media_duration(dumper->file, track);
	gf_webvtt_parser_finalize(parser, duration);

	if (box_dump)
		fprintf(out, "</WebVTTTrack>\n");

exit:
	gf_webvtt_parser_del(parser);
	if (szName) gf_fclose(out);
	return e;
#endif
}

#endif /*GPAC_DISABLE_MEDIA_EXPORT*/

#endif /*GPAC_DISABLE_ISOM_DUMP*/

#endif /*GPAC_DISABLE_VTT*/

/**
*			GPAC - Multimedia Framework C SDK
*
*			Authors: Pierre Souchay, Jean Le Feuvre, Romain Bouqueau
*			Copyright (c) Telecom ParisTech 2010-2012, Romain Bouqueau
*					All rights reserved
*
*  This file is part of GPAC
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

#define _GNU_SOURCE

#include <gpac/internal/m3u8.h>
#include <gpac/network.h>


GF_Err playlist_element_del(PlaylistElement * e);

static GF_Err cleanup_list_of_elements(GF_List *list) {
	GF_Err result = GF_OK;
	if (list == NULL)
		return result;
	while (gf_list_count(list)) {
		PlaylistElement *pl = (PlaylistElement *) gf_list_get(list, 0);
		if (pl)
			result |= playlist_element_del(pl);
		gf_list_rem(list, 0);
	}
	gf_list_del(list);
	return result;
}

/**
 * Deletes an Playlist element
 */
GF_Err playlist_element_del(PlaylistElement * e) {
	GF_Err result = GF_OK;
	if (e == NULL)
		return result;
	if (e->title) {
		gf_free(e->title);
		e->title = NULL;
	}
	if (e->codecs) {
		gf_free(e->codecs);
		e->codecs = NULL;
	}
	assert(e->url);
	gf_free(e->url);
	e->url = NULL;

	switch (e->element_type) {
	case TYPE_UNKNOWN:
	case TYPE_MEDIA:
		break;
	case TYPE_PLAYLIST:
		assert(e->element.playlist.elements);
		result |= cleanup_list_of_elements(e->element.playlist.elements);
		e->element.playlist.elements = NULL;
	default:
		break;
	}
	gf_free(e);
	return result;
}

/**
 * Creates a new program properly initialized
 */
static Stream* program_new(int stream_id) {
	Stream *program = (Stream *) gf_malloc(sizeof(Stream));
	if (program == NULL) {
		return NULL;
	}
	program->stream_id = stream_id;
	program->variants = gf_list_new();
	if (program->variants == NULL) {
		gf_free(program);
		return NULL;
	}
	return program;
}

/**
 * Deletes the specified program
 */
static GF_Err program_del(Stream *program) {
	GF_Err e = GF_OK;
	if (program == NULL)
		return e;
	if (program->variants) {
		while (gf_list_count(program->variants)) {
			GF_List *l = gf_list_get(program->variants, 0);
			cleanup_list_of_elements(l);
			gf_list_rem(program->variants, 0);
		}
		gf_list_del(program->variants);
	}
	program->variants = NULL;
	gf_free(program);
	return e;
}

/**
 * Creates an Playlist element.
 * This element can be either a playlist of a stream according to first parameter.
 * \return NULL if element could not be created. Elements will be deleted recursively
 */
static PlaylistElement* playlist_element_new(PlaylistElementType element_type, const char *url, const char *title, const char *codecs, double duration_info, u64 byte_range_start, u64 byte_range_end) {
	PlaylistElement *e = gf_malloc(sizeof(PlaylistElement));
	memset(e, 0, sizeof(PlaylistElement));
	assert(url);
	if (e == NULL)
		return NULL;
	e->duration_info = duration_info;
	e->byte_range_start = byte_range_start;
	e->byte_range_end = byte_range_end;

	e->title = (title ? gf_strdup(title) : NULL);
	e->codecs = (codecs ? gf_strdup(codecs) : NULL);
	assert(url);
	e->url = gf_strdup(url);
	e->bandwidth = 0;
	e->element_type = element_type;
	if (element_type == TYPE_PLAYLIST) {
		e->element.playlist.is_ended = GF_FALSE;
		e->element.playlist.target_duration = duration_info;
		e->element.playlist.current_media_seq = 0;
		e->element.playlist.media_seq_min = 0;
		e->element.playlist.media_seq_max = 0;
		e->element.playlist.elements = gf_list_new();
		if (NULL == (e->element.playlist.elements)) {
			if (e->title)
				gf_free(e->title);
			if (e->codecs)
				gf_free(e->codecs);
			if (e->url)
				gf_free(e->url);
			e->url = NULL;
			e->title = NULL;
			e->codecs = NULL;
			gf_free(e);
			return NULL;
		}
	} else {
		/* Nothing to do, media is an empty structure */
	}
	assert(e->bandwidth == 0);
	assert(e->url);
	return e;
}

/**
 * Creates a new MasterPlaylist
 * \return NULL if MasterPlaylist element could not be allocated
 */
MasterPlaylist* master_playlist_new()
{
	MasterPlaylist *pl = (MasterPlaylist*)gf_malloc(sizeof(MasterPlaylist));
	if (pl == NULL)
		return NULL;
	pl->streams = gf_list_new();
	if (!pl->streams) {
		gf_free(pl);
		return NULL;
	}
	pl->current_stream = -1;
	pl->playlist_needs_refresh = GF_TRUE;
	return pl;
}

GF_Err gf_m3u8_master_playlist_del(MasterPlaylist *playlist) {
	if (playlist == NULL)
		return GF_OK;
	assert(playlist->streams);
	while (gf_list_count(playlist->streams)) {
		Stream *p = gf_list_get(playlist->streams, 0);
		assert(p);
		while (gf_list_count(p->variants)) {
			PlaylistElement *pl = gf_list_get(p->variants, 0);
			assert(pl);
			playlist_element_del(pl);
			gf_list_rem(p->variants, 0);
		}
		gf_list_del(p->variants);
		p->variants = NULL;
		program_del(p);
		gf_list_rem(playlist->streams, 0);
	}
	gf_list_del(playlist->streams);
	playlist->streams = NULL;
	gf_free(playlist);

	return GF_OK;
}

static GF_Err playlist_element_dump(const PlaylistElement *e, int indent) {
	int i;
	GF_Err r = GF_OK;
	for (i=0; i<indent; i++)
		GF_LOG(GF_LOG_DEBUG, GF_LOG_DASH, (" "));
	if (e == NULL) {
		GF_LOG(GF_LOG_DEBUG, GF_LOG_DASH, ("[M3U8] NULL PlaylistElement\n"));
		return r;
	}
	GF_LOG(GF_LOG_DEBUG, GF_LOG_DASH, ("[M3U8] PlayListElement[%p, title=%s, codecs=%s, duration=%d, bandwidth=%d, url=%s, type=%s]\n",
	                                   (void*)e, e->title, e->codecs, e->duration_info, e->bandwidth, e->url,
	                                   e->element_type == TYPE_MEDIA ? "media" : "playlist"));
	if (TYPE_PLAYLIST == e->element_type) {
		int sz;
		assert(e->element.playlist.elements);
		sz = gf_list_count(e->element.playlist.elements);
		indent += 2;
		for (i=0 ; i<sz; i++) {
			PlaylistElement *elt = gf_list_get(e->element.playlist.elements, i);
			assert(elt);
			r |= playlist_element_dump(elt, indent);
		}
	}
	return r;
}

static Stream* master_playlist_find_matching_stream(const MasterPlaylist *pl, const u32 stream_id) {
	u32 count, i;
	assert(pl);
	assert(pl->streams);
	assert(stream_id >= 0);
	count = gf_list_count(pl->streams);
	for (i=0; i<count; i++) {
		Stream *cur = gf_list_get(pl->streams, i);
		assert(cur);
		if (stream_id == cur->stream_id) {
			/* We found the program */
			return cur;
		}
	}
	return NULL;
}


typedef struct _s_accumulated_attributes {
	//TODO: store as a structure with: { attribute, version, mandatory }
	char *title;
	double duration_in_seconds;
	int bandwidth;
	int width, height;
	int stream_id;
	char *codecs;
	int target_duration_in_seconds;
	int min_media_sequence;
	int current_media_seq;
	int version;
	int compatibility_version; /*compute version required by the M3U8 content*/
	Bool is_master_playlist;
	Bool is_playlist_ended;
	u64 byte_range_start, byte_range_end;
} s_accumulated_attributes;

static Bool safe_start_equals(const char *attribute, const char *line) {
	size_t len, atlen;
	if (line == NULL)
		return GF_FALSE;
	len = strlen(line);
	atlen = strlen(attribute);
	if (len < atlen)
		return GF_FALSE;
	return (0 == strncmp(attribute, line, atlen));
}

static char** extractAttributes(const char *name, const char *line, const int num_attributes) {
	int sz, i, curr_attribute, start;
	char **ret;
	u8 quote = 0;
	int len = (u32) strlen(line);
	start = (u32) strlen(name);
	if (len <= start)
		return NULL;
	if (!safe_start_equals(name, line))
		return NULL;
	ret = gf_calloc((num_attributes + 1), sizeof(char*));
	curr_attribute = 0;
	for (i=start; i<=len; i++) {
		if (line[i] == '\0' || (!quote && line[i] == ',')  || (line[i] == quote)) {
			u32 spaces = 0;
			sz = 1 + i - start;
			while (line[start+spaces] == ' ')
				spaces++;
			ret[curr_attribute] = gf_calloc( (1+sz-spaces), sizeof(char));
			strncpy(ret[curr_attribute], &(line[start+spaces]), sz-spaces);
			curr_attribute++;
			start = i+1;
			if (start == len) {
				return ret;
			}
		}
		if ((line[i] == '\'') || (line[i] == '"'))  {
			if (quote)
				quote = 0;
			else
				quote = line[i];
		}
	}
	if (curr_attribute == 0) {
		gf_free(ret);
		return NULL;
	}
	return ret;
}

#define M3U8_COMPATIBILITY_VERSION(v) \
	if (v > attributes->compatibility_version) \
		attributes->compatibility_version = v;

/**
 * Parses the attributes and accumulate into the attributes structure
 */
static char** parseAttributes(const char *line, s_accumulated_attributes *attributes) {
	int int_value, i;
	double double_value;
	char **ret;
	char *end_ptr;
	char *utility;
	if (line == NULL)
		return NULL;
	if (!safe_start_equals("#EXT", line))
		return NULL;
	if (safe_start_equals("#EXT-X-ENDLIST", line)) {
		attributes->is_playlist_ended = GF_TRUE;
		M3U8_COMPATIBILITY_VERSION(1);
		return NULL;
	}
	ret = extractAttributes("#EXT-X-TARGETDURATION:", line, 1);
	if (ret) {
		/* #EXT-X-TARGETDURATION:<seconds> */
		if (ret[0]) {
			int_value = (s32) strtol(ret[0], &end_ptr, 10);
			if (end_ptr != ret[0]) {
				attributes->target_duration_in_seconds = int_value;
			}
		}
		M3U8_COMPATIBILITY_VERSION(1);
		return ret;
	}
	ret = extractAttributes("#EXT-X-MEDIA-SEQUENCE:", line, 1);
	if (ret) {
		/* #EXT-X-MEDIA-SEQUENCE:<number> */
		if (ret[0]) {
			int_value = (s32)strtol(ret[0], &end_ptr, 10);
			if (end_ptr != ret[0]) {
				attributes->min_media_sequence = int_value;
				attributes->current_media_seq = int_value;
			}
		}
		M3U8_COMPATIBILITY_VERSION(1);
		return ret;
	}
	ret = extractAttributes("#EXT-X-VERSION:", line, 1);
	if (ret) {
		/* #EXT-X-VERSION:<number> */
		if (ret[0]) {
			int_value = (s32)strtol(ret[0], &end_ptr, 10);
			if (end_ptr != ret[0]) {
				attributes->version = int_value;
			}
			M3U8_COMPATIBILITY_VERSION(2);
		}
		return ret;
	}
	ret = extractAttributes("#EXTINF:", line, 2);
	if (ret) {
		M3U8_COMPATIBILITY_VERSION(1);
		/* #EXTINF:<duration>,<title> */
		if (ret[0]) {
			double_value = strtod(ret[0], &end_ptr);
			if (end_ptr != ret[0]) {
				attributes->duration_in_seconds = double_value;
			}
			if (strstr(ret[0], ".") || (double_value > (int)double_value)) {
				M3U8_COMPATIBILITY_VERSION(3);
			}
		}
		if (ret[1]) {
			attributes->title = gf_strdup(ret[1]);
		}
		return ret;
	}
	ret = extractAttributes("#EXT-X-KEY:", line, 4);
	if (ret) {
		/* #EXT-X-KEY:METHOD=<method>[,URI="<URI>"] */
		const char *method = "METHOD=";
		const size_t method_len = strlen(method);
		if (safe_start_equals(method, ret[0])) {
			if (strncmp(ret[0]+method_len, "NONE", 4)) {
				GF_LOG(GF_LOG_WARNING, GF_LOG_DASH,("[M3U8] EXT-X-KEY not supported.\n", line));
			}
		}
		M3U8_COMPATIBILITY_VERSION(1);
		return ret;
	}
	ret = extractAttributes("#EXT-X-PROGRAM-DATE-TIME:", line, 1);
	if (ret) {
		/* #EXT-X-PROGRAM-DATE-TIME:<YYYY-MM-DDThh:mm:ssZ> */
		GF_LOG(GF_LOG_WARNING, GF_LOG_DASH,("[M3U8] EXT-X-PROGRAM-DATE-TIME not supported.\n", line));
		M3U8_COMPATIBILITY_VERSION(1);
		return ret;
	}
	ret = extractAttributes("#EXT-X-ALLOW-CACHE:", line, 1);
	if (ret) {
		/* #EXT-X-ALLOW-CACHE:<YES|NO> */
		GF_LOG(GF_LOG_WARNING, GF_LOG_DASH,("[M3U8] EXT-X-ALLOW-CACHE not supported.\n", line));
		M3U8_COMPATIBILITY_VERSION(1);
		return ret;
	}
	ret = extractAttributes("#EXT-X-PLAYLIST-TYPE", line, 1);
	if (ret) {
		/* #EXT-X-PLAYLIST-TYPE:<EVENT|VOD> */
		GF_LOG(GF_LOG_WARNING, GF_LOG_DASH,("[M3U8] EXT-X-PLAYLIST-TYPE not supported.\n", line));
		M3U8_COMPATIBILITY_VERSION(3);
		return ret;
	}
	ret = extractAttributes("#EXT-X-STREAM-INF:", line, 10);
	if (ret) {
		/* #EXT-X-STREAM-INF:[attribute=value][,attribute=value]* */
		i = 0;
		attributes->is_master_playlist = GF_TRUE;
		M3U8_COMPATIBILITY_VERSION(1);
		while (ret[i] != NULL) {
			if (safe_start_equals("BANDWIDTH=", ret[i])) {
				utility = &(ret[i][10]);
				int_value = (s32) strtol(utility, &end_ptr, 10);
				if (end_ptr != utility)
					attributes->bandwidth = int_value;
			} else if (safe_start_equals("PROGRAM-ID=", ret[i])) {
				utility = &(ret[i][11]);
				int_value = (s32) strtol(utility, &end_ptr, 10);
				if (end_ptr != utility)
					attributes->stream_id = int_value;
			} else if (safe_start_equals("CODECS=\"", ret[i])) {
				int_value = (u32) strlen(ret[i]);
				if (ret[i][int_value-1] == '"') {
					attributes->codecs = gf_strdup(&(ret[i][7]));
				}
			} else if (safe_start_equals("RESOLUTION=", ret[i])) {
				u32 w, h;
				utility = &(ret[i][11]);
				if ((sscanf(utility, "%dx%d", &w, &h)==2) || (sscanf(utility, "%dx%d,", &w, &h)==2)) {
					attributes->width = w;
					attributes->height = h;
				}
				M3U8_COMPATIBILITY_VERSION(2);
			}
			i++;
		}
		return ret;
	}
	ret = extractAttributes("#EXT-X-DISCONTINUITY", line, 0);
	if (ret) {
		/* #EXT-X-DISCONTINUITY */
		GF_LOG(GF_LOG_WARNING, GF_LOG_DASH,("[M3U8] EXT-X-DISCONTINUITY not supported.\n", line));
		M3U8_COMPATIBILITY_VERSION(1);
		return ret;
	}
	ret = extractAttributes("#EXT-X-BYTERANGE:", line, 1);
	if (ret) {
		/* #EXT-X-BYTERANGE:<begin@end> */
		if (ret[0]) {
			u64 begin, size;
			if (sscanf(ret[0], LLU"@"LLU, &size, &begin) == 2) {
				if (size) {
					attributes->byte_range_start = begin;
					attributes->byte_range_end = begin + size - 1;
				} else {
					GF_LOG(GF_LOG_ERROR, GF_LOG_DASH,("[M3U8] Invalid byte range %s\n", ret[0]));
				}
			}
		}
		M3U8_COMPATIBILITY_VERSION(4);
		return ret;
	}
	GF_LOG(GF_LOG_WARNING, GF_LOG_DASH,("[M3U8] Unknown line in M3U8 file %s\n", line));
	return NULL;
}

#define M3U8_BUF_SIZE 2048

GF_EXPORT
GF_Err gf_m3u8_parse_master_playlist(const char *file, MasterPlaylist **playlist, const char *baseURL)
{
	return gf_m3u8_parse_sub_playlist(file, playlist, baseURL, NULL, NULL);
}

GF_Err gf_m3u8_parse_sub_playlist(const char *file, MasterPlaylist **playlist, const char *baseURL, Stream *in_stream, PlaylistElement *sub_playlist)
{
	int len, i, currentLineNumber;
	FILE *f = NULL;
	char *m3u8_payload;
	u32 m3u8_size, m3u8pos;
	MasterPlaylist *pl;
	char currentLine[M3U8_BUF_SIZE];
	char **attributes = NULL;
	s_accumulated_attributes attribs;

	if (!strncmp(file, "gmem://", 7)) {
		if (sscanf(file, "gmem://%d@%p", &m3u8_size, &m3u8_payload) != 2) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_DASH,("[M3U8] Cannot Open m3u8 source %s for reading\n", file));
			return GF_SERVICE_ERROR;
		}
	} else {
		f = gf_f64_open(file, "rt");
		if (!f) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_DASH,("[M3U8] Cannot Open m3u8 file %s for reading\n", file));
			return GF_SERVICE_ERROR;
		}
	}
	if (*playlist == NULL) {
		*playlist = master_playlist_new();
		if (!(*playlist)) {
			if (f) fclose(f);
			return GF_OUT_OF_MEM;
		}
	}
	pl = *playlist;
	currentLineNumber = 0;
	memset(&attribs, 0, sizeof(s_accumulated_attributes));
	attribs.bandwidth = 0;
	attribs.duration_in_seconds = 0;
	attribs.target_duration_in_seconds = 0;
	attribs.is_master_playlist = GF_FALSE;
	attribs.is_playlist_ended = GF_FALSE;
	attribs.min_media_sequence = 1;
	attribs.current_media_seq = 0;
	attribs.version = 0;
	attribs.compatibility_version = 1;
	m3u8pos = 0;
	while (1) {
		char *eof;
		if (f) {
			if (!fgets(currentLine, sizeof(currentLine), f))
				break;
		} else {
			u32 __idx = 0;
			if (m3u8pos >= m3u8_size)
				break;
			while (1) {
				currentLine[__idx] = m3u8_payload[m3u8pos];
				__idx++;
				m3u8pos++;
				if ((currentLine[__idx-1]=='\n') || (currentLine[__idx-1]=='\r')) {
					currentLine[__idx]=0;
					break;
				}
			}
		}
		currentLineNumber++;
		eof = strchr(currentLine, '\r');
		if (eof)
			eof[0] = '\0';
		eof = strchr(currentLine, '\n');
		if (eof)
			eof[0] = '\0';
		len = (u32) strlen(currentLine);
		if (len < 1)
			continue;
		if (currentLineNumber == 1) {
			/* Playlist MUST start with #EXTM3U */
			if (len < 7 || (strncmp("#EXTM3U", currentLine, 7) != 0)) {
				fclose(f);
				gf_m3u8_master_playlist_del(pl);
				GF_LOG(GF_LOG_ERROR, GF_LOG_DASH, ("Failed to parse M3U8 File, it should start with #EXTM3U, but was : %s\n", currentLine));
				return GF_STREAM_NOT_FOUND;
			}
			continue;
		}
		if (currentLine[0] == '#') {
			/* A comment or a directive */
			if (strncmp("#EXT", currentLine, 4) == 0) {
				attributes = parseAttributes(currentLine, &attribs);
				if (attributes == NULL) {
					GF_LOG(GF_LOG_DEBUG, GF_LOG_DASH, ("Comment at line %d : %s\n", currentLineNumber, currentLine));
				} else {
					GF_LOG(GF_LOG_DEBUG, GF_LOG_DASH, ("Directive at line %d: \"%s\", attributes=", currentLineNumber, currentLine));
					i = 0;
					while (attributes[i] != NULL) {
						GF_LOG(GF_LOG_DEBUG, GF_LOG_DASH, (" [%d]='%s'", i, attributes[i]));
						gf_free(attributes[i]);
						attributes[i] = NULL;
						i++;
					}
					GF_LOG(GF_LOG_DEBUG, GF_LOG_DASH, ("\n"));
					gf_free(attributes);
					attributes = NULL;
				}
				if (attribs.is_playlist_ended) {
					pl->playlist_needs_refresh = GF_FALSE;
				}
			}
		} else {
			char *fullURL = currentLine;

			if (gf_url_is_local(currentLine)) {
				/*
				if (gf_url_is_local(baseURL)){
				int num_chars = -1;
				if (baseURL[strlen(baseURL)-1] == '/'){
				num_chars = asprintf(&fullURL, "%s%s", baseURL, currentLine);
				} else {
				num_chars = asprintf(&fullURL, "%s/%s", baseURL, currentLine);
				}
				if (num_chars < 0 || fullURL == NULL){
				gf_m3u8_master_playlist_del(*playlist);
				playlist = NULL;
				return GF_OUT_OF_MEM;
				}
				} else */ {
					fullURL = gf_url_concatenate(baseURL, currentLine);
				}
				assert(fullURL);
			}
			{
				u32 count;
				PlaylistElement *curr_playlist = sub_playlist;
				/* First, we have to find the matching program */
				Stream *stream = in_stream;
				if (!in_stream)
					stream = master_playlist_find_matching_stream(pl, attribs.stream_id);
				/* We did not found the program, we create it */
				if (stream == NULL) {
					stream = program_new(attribs.stream_id);
					if (stream == NULL) {
						/* OUT of memory */
						gf_m3u8_master_playlist_del(*playlist);
						if (f) fclose(f);
						playlist = NULL;
						return GF_OUT_OF_MEM;
					}
					gf_list_add(pl->streams, stream);
					if (pl->current_stream < 0)
						pl->current_stream = stream->stream_id;
				}

				/* OK, we have a program, we have to choose the elements with the same bandwidth */
				assert(stream);
				assert(stream->variants);
				count = gf_list_count(stream->variants);

				if (!curr_playlist) {
					for (i=0; i<(s32)count; i++) {
						PlaylistElement *i_playlist_element = gf_list_get(stream->variants, i);
						assert(i_playlist_element);
						if (i_playlist_element->bandwidth == attribs.bandwidth) {
							curr_playlist = i_playlist_element;
							break;
						}
					}
				}

				if (attribs.is_master_playlist) {
					/* We are the Master Playlist */
					if (curr_playlist != NULL) {
						/* should not happen, it means we redefine something previously added */
						//assert(0);
					}
					curr_playlist = playlist_element_new(
					                      TYPE_UNKNOWN,
					                      fullURL,
					                      attribs.title,
					                      attribs.codecs,
					                      attribs.duration_in_seconds,
					                      attribs.byte_range_start, attribs.byte_range_end);
					if (curr_playlist == NULL) {
						/* OUT of memory */
						gf_m3u8_master_playlist_del(*playlist);
						playlist = NULL;
						if (f) fclose(f);
						return GF_OUT_OF_MEM;
					}
					assert(fullURL);
					curr_playlist->url = gf_strdup(fullURL);
					curr_playlist->title = attribs.title ? gf_strdup(attribs.title):NULL;
					curr_playlist->codecs = attribs.codecs ? gf_strdup(attribs.codecs):NULL;
					gf_list_add(stream->variants, curr_playlist);
					curr_playlist->width = attribs.width;
					curr_playlist->height = attribs.height;
				} else {
					/* Normal Playlist */
					assert(pl->streams);
					if (curr_playlist == NULL) {
						/* This is a "normal" playlist without any element in it */
						PlaylistElement *subElement;
						assert(baseURL);
						curr_playlist = playlist_element_new(
						                      TYPE_PLAYLIST,
						                      baseURL,
						                      attribs.title,
						                      attribs.codecs,
						                      attribs.duration_in_seconds,
						                      attribs.byte_range_start, attribs.byte_range_end);
						if (curr_playlist == NULL) {
							/* OUT of memory */
							gf_m3u8_master_playlist_del(*playlist);
							playlist = NULL;
							if (f) fclose(f);
							return GF_OUT_OF_MEM;
						}
						assert(curr_playlist->element.playlist.elements);
						assert(fullURL);
						assert(curr_playlist->url);
						curr_playlist->title = NULL;
						curr_playlist->codecs = NULL;
						subElement = playlist_element_new(
						                 TYPE_UNKNOWN,
						                 fullURL,
						                 attribs.title,
						                 attribs.codecs,
						                 attribs.duration_in_seconds,
						                 attribs.byte_range_start, attribs.byte_range_end);
						if (subElement == NULL) {
							gf_m3u8_master_playlist_del(*playlist);
							playlist_element_del(curr_playlist);
							playlist = NULL;
							if (f) fclose(f);
							return GF_OUT_OF_MEM;
						}
						gf_list_add(curr_playlist->element.playlist.elements, subElement);
						gf_list_add(stream->variants, curr_playlist);
						curr_playlist->element.playlist.computed_duration += subElement->duration_info;
						assert(stream);
						assert(stream->variants);
						assert(curr_playlist);

					} else {
						PlaylistElement *subElement = playlist_element_new(
						                                   TYPE_UNKNOWN,
						                                   fullURL,
						                                   attribs.title,
						                                   attribs.codecs,
						                                   attribs.duration_in_seconds,
						                                   attribs.byte_range_start, attribs.byte_range_end);
						if (curr_playlist->element_type != TYPE_PLAYLIST) {
							curr_playlist->element_type = TYPE_PLAYLIST;
							if (!curr_playlist->element.playlist.elements)
								curr_playlist->element.playlist.elements = gf_list_new();
						}
						if (subElement == NULL) {
							gf_m3u8_master_playlist_del(*playlist);
							playlist_element_del(curr_playlist);
							playlist = NULL;
							if (f) fclose(f);
							return GF_OUT_OF_MEM;
						}
						gf_list_add(curr_playlist->element.playlist.elements, subElement);
						curr_playlist->element.playlist.computed_duration += subElement->duration_info;
					}
				}

				curr_playlist->element.playlist.current_media_seq = attribs.current_media_seq;
				/* We first set the default duration for element, aka targetDuration */
				if (attribs.target_duration_in_seconds > 0) {
					curr_playlist->element.playlist.target_duration = attribs.target_duration_in_seconds;
					curr_playlist->duration_info = attribs.target_duration_in_seconds;
				}
				if (attribs.duration_in_seconds) {
					if (curr_playlist->duration_info == 0) {
						/* we set the playlist duration info as the duration of a segment, only if it's not set
						   There are cases of playlist with the last segment with a duration different from the others
						   (example: Apple bipbop test)*/
						curr_playlist->duration_info = attribs.duration_in_seconds;
					}
				}
				curr_playlist->element.playlist.media_seq_min = attribs.min_media_sequence;
				curr_playlist->element.playlist.media_seq_max = attribs.current_media_seq++;
				if (attribs.bandwidth > 1)
					curr_playlist->bandwidth = attribs.bandwidth;
				if (attribs.is_playlist_ended)
					curr_playlist->element.playlist.is_ended = GF_TRUE;
			}
			/* Cleanup all line-specific fields */
			if (attribs.title) {
				gf_free(attribs.title);
				attribs.title = NULL;
			}
			attribs.duration_in_seconds = 0;
			attribs.bandwidth = 0;
			attribs.stream_id = 0;
			if (attribs.codecs != NULL) {
				gf_free(attribs.codecs);
				attribs.codecs = NULL;
			}
			if (fullURL != currentLine) {
				gf_free(fullURL);
			}
		}
	}
	if (f) fclose(f);

	for (i=0; i<(int)gf_list_count(pl->streams); i++) {
		u32 j;
		Stream *prog = gf_list_get(pl->streams, i);
		prog->computed_duration = 0;
		for (j=0; j<gf_list_count(prog->variants); j++) {
			PlaylistElement *ple = gf_list_get(prog->variants, j);
			if (ple->element_type == TYPE_PLAYLIST) {
				if (ple->element.playlist.computed_duration > prog->computed_duration)
					prog->computed_duration = ple->element.playlist.computed_duration;
			}
		}
	}

	if (attribs.version < attribs.compatibility_version) {
		GF_LOG(GF_LOG_WARNING, GF_LOG_DASH, ("[M3U8] Version %d specified but tags from version %d detected\n", attribs.version, attribs.compatibility_version));
	}
	return GF_OK;
}

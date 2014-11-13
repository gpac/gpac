/**
*			GPAC - Multimedia Framework C SDK
*
*			Authors: Pierre Souchay, Jean Le Feuvre
*			Copyright (c) Telecom ParisTech 2010-2012
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
#include <string.h>
#include <stdio.h>
#include <gpac/network.h>

#define MYLOG(msg) GF_LOG(GF_LOG_DEBUG, GF_LOG_DASH, msg)


#if defined(WIN32) || defined(_WIN32_WCE)
#define bzero(a, b) memset(a, 0x0, b)
#endif

GF_Err cleanup_list_of_elements(GF_List * list) {
	GF_Err result = GF_OK;
	if (list == NULL)
		return result;
	while (gf_list_count(list)) {
		PlaylistElement * pl = (PlaylistElement *) gf_list_get(list, 0);
		if (pl)
			result |= playlist_element_del(pl);
		gf_list_rem(list, 0);
	}
	gf_list_del(list);
	return result;
}

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
	assert( e->url);
	gf_free(e->url);
	e->url = NULL;

	switch (e->elementType) {
	case TYPE_UNKNOWN:
	case TYPE_STREAM:
		break;
	case TYPE_PLAYLIST:
		assert( e->element.playlist.elements);
		result |= cleanup_list_of_elements(e->element.playlist.elements);
		e->element.playlist.elements = NULL;
	default:
		break;
	}
	gf_free(e);
	return result;
}

Program * program_new(int programId) {
	Program * program = (Program*)gf_malloc(sizeof(Program));
	if (program == NULL) {
		return NULL;
	}
	program->programId = programId;
	program->bitrates = gf_list_new();
	if (program->bitrates == NULL) {
		gf_free(program);
		return NULL;
	}
	return program;
}

GF_Err program_del(Program * program) {
	GF_Err e = GF_OK;
	if (program == NULL)
		return e;
	if ( program->bitrates) {
		while (gf_list_count(program->bitrates)) {
			GF_List * l = gf_list_get(program->bitrates, 0);
			cleanup_list_of_elements(l);
			gf_list_rem(program->bitrates, 0);
		}
		gf_list_del(program->bitrates);
	}
	program->bitrates = NULL;
	gf_free(program);
	return e;
}

/*
static GF_Err playlist_del(Playlist * pl){
GF_Err result = GF_OK;
if (pl == NULL)
return result;
if (pl->elements){
result|= cleanup_list_of_elements(pl->elements);
pl->elements = NULL;
}
gf_free(pl);
return result;
}*/

PlaylistElement * playlist_element_new(PlaylistElementType elementType, const char * url, const char * title, const char *codecs, int durationInfo, u64 byteRangeStart, u64 byteRangeEnd) {
	PlaylistElement * e = gf_malloc(sizeof(PlaylistElement));
	bzero(e, sizeof(PlaylistElement));
	assert( url );
	if (e == NULL)
		return NULL;
	e->durationInfo = durationInfo;
	e->byteRangeStart = byteRangeStart;
	e->byteRangeEnd = byteRangeEnd;

	e->title = (title ? gf_strdup(title) : NULL);
	e->codecs = (codecs ? gf_strdup(codecs) : NULL);
	assert( url);
	e->url = gf_strdup(url);
	e->bandwidth = 0;
	e->elementType = elementType;
	if (elementType == TYPE_PLAYLIST) {
		e->element.playlist.is_ended = 0;
		e->element.playlist.target_duration = durationInfo;
		e->element.playlist.currentMediaSequence = 0;
		e->element.playlist.mediaSequenceMin = 0;
		e->element.playlist.mediaSequenceMax = 0;
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
		/* Nothing to do, stream is an empty structure */
	}
	assert(e->bandwidth == 0);
	assert(e->url);
	return e;
}
/*
Playlist * playlist_new(){
Playlist * pl = gf_malloc(sizeof(Playlist));
if (pl == NULL)
return NULL;
pl->currentMediaSequence = 1;
pl->target_duration = 0;
pl->mediaSequenceMin = 0;
pl->mediaSequenceMax = 0;
pl->is_ended = 0;
pl->elements = gf_list_new();
if (pl->elements == NULL){
gf_free(pl);
return NULL;
}
return pl;
}
*/

VariantPlaylist * variant_playlist_new ()
{
	VariantPlaylist * pl = (VariantPlaylist*)gf_malloc( sizeof(VariantPlaylist) );
	if (pl == NULL)
		return NULL;
	pl->programs = gf_list_new();
	if (! pl->programs) {
		gf_free( pl );
		return NULL;
	}
	pl->currentProgram = -1;
	pl->playlistNeedsRefresh = 1;
	return pl;
}

GF_Err variant_playlist_del (VariantPlaylist * playlist) {
	if (playlist == NULL)
		return GF_OK;
	assert( playlist->programs);
	while (gf_list_count(playlist->programs)) {
		Program * p = gf_list_get(playlist->programs, 0);
		assert(p);
		while (gf_list_count( p->bitrates )) {
			PlaylistElement * pl = gf_list_get(p->bitrates, 0);
			assert( pl );
			playlist_element_del(pl);
			gf_list_rem(p->bitrates, 0);
		}
		gf_list_del(p->bitrates);
		p->bitrates = NULL;
		program_del(p);
		gf_list_rem(playlist->programs, 0);
	}
	gf_list_del(playlist->programs);
	playlist->programs = NULL;
	gf_free(playlist);
	return GF_OK;
}

GF_Err playlist_element_dump(const PlaylistElement * e, int indent) {
	int i;
	GF_Err r = GF_OK;
	for (i = 0 ; i < indent; i++)
		GF_LOG(GF_LOG_DEBUG, GF_LOG_DASH, (" ") );
	if (e == NULL) {
		GF_LOG(GF_LOG_DEBUG, GF_LOG_DASH, ("[M3U8] NULL PlaylistElement\n"));
		return r;
	}
	GF_LOG(GF_LOG_DEBUG, GF_LOG_DASH, ("[M3U8] PlayListElement[%p, title=%s, codecs=%s, duration=%d, bandwidth=%d, url=%s, type=%s]\n",
	                                   (void*)e,
	                                   e->title,
	                                   e->codecs,
	                                   e->durationInfo,
	                                   e->bandwidth,
	                                   e->url,
	                                   e->elementType == TYPE_STREAM ? "stream" : "playlist"));
	if (TYPE_PLAYLIST == e->elementType) {
		int sz;
		assert( e->element.playlist.elements);
		sz = gf_list_count(e->element.playlist.elements);
		indent+=2;
		for (i = 0 ; i < sz ; i++) {
			PlaylistElement * el = gf_list_get(e->element.playlist.elements, i);
			assert( el);
			r|= playlist_element_dump( el, indent);
		}
	}
	return r;
}

GF_Err variant_playlist_dump(const VariantPlaylist * pl) {
	int i, count;
	GF_Err e = GF_OK;
	if (pl == NULL) {
		GF_LOG(GF_LOG_DEBUG, GF_LOG_DASH, ("[M3U8] VariantPlaylist = NULL\n"));
		return e;
	}
	GF_LOG(GF_LOG_DEBUG, GF_LOG_DASH, ("[M3U8] VariantPlaylist = {\n"));
	assert( pl->programs);
	count = gf_list_count( pl->programs);
	for (i = 0 ; i < count ; i++) {
		int j, countj;
		Program * p = gf_list_get(pl->programs, i);
		assert( p );
		GF_LOG(GF_LOG_DEBUG, GF_LOG_DASH, ("[M3U8] program[programId=%d]{\n",  p->programId));
		assert( p->bitrates );
		countj = gf_list_count(p->bitrates);
		for (j = 0; j < countj; j++) {
			PlaylistElement * el = gf_list_get(p->bitrates, j);
			assert(el);
			e |= playlist_element_dump( el, 4);
		}
		GF_LOG(GF_LOG_DEBUG, GF_LOG_DASH, ("[M3U8] }\n"));
	}
	GF_LOG(GF_LOG_DEBUG, GF_LOG_DASH, ("[M3U8]}\n"));
	return e;
}

Program * variant_playlist_find_matching_program(const VariantPlaylist * pl, const u32 programId) {
	u32 count, i;
	assert( pl);
	assert( pl->programs);
	assert(programId >= 0);
	count = gf_list_count(pl->programs);
	for (i = 0 ; i < count ; i++) {
		Program * cur = gf_list_get(pl->programs, i);
		assert( cur );
		if (programId == cur->programId) {
			/* We found the program */
			return cur;
		}
	}
	return NULL;
}

Program * variant_playlist_get_current_program(const VariantPlaylist * pl) {
	assert( pl );
	return variant_playlist_find_matching_program(pl, pl->currentProgram);
}



typedef struct _s_accumulated_attributes {
	char * title;
	int durationInSeconds;
	int bandwidth;
	int width, height;
	int programId;
	char * codecs;
	int targetDurationInSeconds;
	int minMediaSequence;
	int currentMediaSequence;
	Bool isVariantPlaylist;
	Bool isPlaylistEnded;
	u64 byteRangeStart, byteRangeEnd;
} s_accumulated_attributes;

static Bool safe_start_equals(const char * attribute, const char * line) {
	size_t len, atlen;
	if (line == NULL)
		return 0;
	len = strlen(line);
	atlen = strlen(attribute);
	if (len < atlen)
		return 0;
	return 0 == strncmp(attribute, line, atlen);
}

static char ** extractAttributes(const char * name, const char * line, const int numberOfAttributes) {
	int sz, i, currentAttribute, start;
	char ** ret;
	u8 quote=0;
	int len = (u32) strlen(line);
	start = (u32) strlen(name);
	if (len <= start)
		return NULL;
	if (!safe_start_equals(name, line))
		return NULL;
	ret = gf_calloc((numberOfAttributes + 1 ), sizeof(char*));
	currentAttribute = 0;
	for (i = start ; i <= len ; i++) {
		if (line[i] == '\0' || (!quote && line[i] == ',')  || (line[i] == quote) ) {
			u32 spaces = 0;
			sz = 1 + i - start;
			while (line[start+spaces] == ' ') spaces++;
			ret[currentAttribute] = gf_calloc( (1+sz-spaces), sizeof(char));
			strncpy(ret[currentAttribute], &(line[start+spaces]), sz-spaces);
			currentAttribute++;
			start = i+1;
			if (start == len) {
				return ret;
			}
		}
		if ((line[i] == '\'') || (line[i] == '"'))  {
			if (quote) quote = 0;
			else quote = line[i];
		}
	}
	if (currentAttribute == 0) {
		gf_free(ret);
		return NULL;
	}
	return ret;
}

/**
* Parses the attributes and accumulate into the attributes structure
*/
static char ** parseAttributes(const char * line, s_accumulated_attributes * attributes) {
	int intValue, i;
	char ** ret;
	char * endPtr;
	char * utility;
	if (line == NULL)
		return NULL;
	if (!safe_start_equals("#EXT", line))
		return NULL;
	if (safe_start_equals("#EXT-X-ENDLIST", line)) {
		attributes->isPlaylistEnded = 1;
		return NULL;
	}
	ret = extractAttributes("#EXT-X-TARGETDURATION:", line, 1);
	if (ret) {
		/* #EXT-X-TARGETDURATION:<seconds> */
		if (ret[0]) {
			intValue = (s32) strtol(ret[0], &endPtr, 10);
			if (endPtr != ret[0]) {
				attributes->targetDurationInSeconds = intValue;
			}
		}
		return ret;
	}
	ret = extractAttributes("#EXT-X-MEDIA-SEQUENCE:", line, 1);
	if (ret) {
		/* #EXT-X-MEDIA-SEQUENCE:<number> */
		if (ret[0]) {
			intValue = (s32) strtol(ret[0], &endPtr, 10);
			if (endPtr != ret[0]) {
				attributes->minMediaSequence = intValue;
				attributes->currentMediaSequence = intValue;
			}
		}
		return ret;
	}
	ret = extractAttributes("#EXTINF:", line, 2);
	if (ret) {
		/* #EXTINF:<duration>,<title> */
		if (ret[0]) {
			intValue = (s32) strtol(ret[0], &endPtr, 10);
			if (endPtr != ret[0]) {
				attributes->durationInSeconds = intValue;
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
		/* Not Supported for now */
		return ret;
	}
	ret = extractAttributes("#EXT-X-STREAM-INF:", line, 10);
	if (ret) {
		/* #EXT-X-STREAM-INF:[attribute=value][,attribute=value]* */
		i = 0;
		attributes->isVariantPlaylist = 1;
		while (ret[i] != NULL) {
			if (safe_start_equals("BANDWIDTH=", ret[i])) {
				utility = &(ret[i][10]);
				intValue = (s32) strtol(utility, &endPtr, 10);
				if (endPtr != utility)
					attributes->bandwidth = intValue;
			} else if (safe_start_equals("PROGRAM-ID=", ret[i])) {
				utility = &(ret[i][11]);
				intValue = (s32) strtol(utility, &endPtr, 10);
				if (endPtr != utility)
					attributes->programId = intValue;
			} else if (safe_start_equals("CODECS=\"", ret[i])) {
				intValue = (u32) strlen(ret[i]);
				if (ret[i][intValue-1] == '"') {
					attributes->codecs = gf_strdup(&(ret[i][7]));
				}
			} else if (safe_start_equals("RESOLUTION=", ret[i])) {
				u32 w, h;
				utility = &(ret[i][11]);
				if ((sscanf(utility, "%dx%d", &w, &h)==2) || (sscanf(utility, "%dx%d,", &w, &h)==2)) {
					attributes->width = w;
					attributes->height = h;
				}
			}
			i++;
		}
		return ret;
	}
	ret = extractAttributes("#EXT-X-BYTERANGE:", line, 1);
	if (ret) {
		/* #EXT-X-BYTERANGE:<begin@end> */
		if (ret[0]) {
			u64 begin, size;
			if (sscanf(ret[0], LLU"@"LLU, &size, &begin) == 2) {
				if (size) {
					attributes->byteRangeStart = begin;
					attributes->byteRangeEnd = begin + size - 1;
				} else {
					GF_LOG(GF_LOG_ERROR, GF_LOG_DASH,("[M3U8] Invalid byte range %s\n", ret[0]));
				}
			}
		}
		return ret;
	}
	return NULL;
}

#define M3U8_BUF_SIZE 2048

GF_Err parse_root_playlist(const char * file, VariantPlaylist ** playlist, const char * baseURL)
{
	return parse_sub_playlist(file, playlist, baseURL, NULL, NULL);
}

GF_Err parse_sub_playlist(const char * file, VariantPlaylist ** playlist, const char * baseURL, Program * in_program, PlaylistElement *sub_playlist)
{
	int len, i, currentLineNumber;
	FILE * f=NULL;
	char *m3u8_payload;
	u32 m3u8_size, m3u8pos;
	VariantPlaylist * pl;
	char currentLine[M3U8_BUF_SIZE];
	char ** attributes = NULL;
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
		*playlist = variant_playlist_new();
		if (!(*playlist)) {
			if (f) fclose(f);
			return GF_OUT_OF_MEM;
		}
	}
	pl = *playlist;
	currentLineNumber = 0;
	bzero(&attribs, sizeof(s_accumulated_attributes));
	attribs.bandwidth = 0;
	attribs.durationInSeconds = 0;
	attribs.targetDurationInSeconds = 0;
	attribs.isVariantPlaylist = 0;
	attribs.isPlaylistEnded = 0;
	attribs.minMediaSequence = 0;
	attribs.currentMediaSequence = 0;
	m3u8pos=0;
	while (1) {
		char * eof;
		if (f) {
			if (!fgets(currentLine, sizeof(currentLine), f))
				break;
		} else {
			u32 __idx=0;
			if (m3u8pos>=m3u8_size)
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
		len = (u32) strlen( currentLine);
		if (len < 1)
			continue;
		if (currentLineNumber == 1) {
			/* Playlist MUST start with #EXTM3U */
			/*			if (len < 7 || strncmp("#EXTM3U", currentLine, 7)!=0) {
							fclose(f);
							variant_playlist_del(pl);
							GF_LOG(GF_LOG_ERROR, GF_LOG_DASH, ("Failed to parse M3U8 File, it should start with #EXTM3U, but was : %s\n", currentLine));
							return GF_STREAM_NOT_FOUND;
						}
						continue;
			*/
		}
		if (currentLine[0] == '#') {
			/* A comment or a directive */
			if (strncmp("#EXT", currentLine, 4)==0) {
				attributes = parseAttributes(currentLine, &attribs);
				if (attributes == NULL) {
					MYLOG(("Comment at line %d : %s\n", currentLineNumber, currentLine));
				} else {
					MYLOG(("Directive at line %d: \"%s\", attributes=", currentLineNumber, currentLine));
					i = 0;
					while (attributes[i] != NULL) {
						MYLOG((" [%d]='%s'", i, attributes[i]));
						gf_free(attributes[i]);
						attributes[i] = NULL;
						i++;
					}
					MYLOG(("\n"));
					gf_free(attributes);
					attributes = NULL;
				}
				if (attribs.isPlaylistEnded) {
					pl->playlistNeedsRefresh = 0;
				}
			}
		} else {
			char * fullURL = currentLine;

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
				variant_playlist_del(*playlist);
				playlist = NULL;
				return GF_OUT_OF_MEM;
				}
				} else */ {
					fullURL = gf_url_concatenate(baseURL, currentLine);
				}
				assert( fullURL );
			}
			{
				u32 count;
				PlaylistElement * currentPlayList = sub_playlist;
				/* First, we have to find the matching program */
				Program * program = in_program;
				if (!in_program) program = variant_playlist_find_matching_program(pl, attribs.programId);
				/* We did not found the program, we create it */
				if (program == NULL) {
					program = program_new(attribs.programId);
					if (program == NULL) {
						/* OUT of memory */
						variant_playlist_del(*playlist);
						if (f) fclose(f);
						playlist = NULL;
						return GF_OUT_OF_MEM;
					}
					gf_list_add(pl->programs, program);
					if (pl->currentProgram < 0)
						pl->currentProgram = program->programId;
				}

				/* OK, we have a program, we have to choose the elements with same bandwidth */
				assert( program );
				assert( program->bitrates);
				count = gf_list_count( program->bitrates);

				if (!currentPlayList) {
					for (i = 0; i < (s32) count; i++) {
						PlaylistElement * itPlayListElement = gf_list_get(program->bitrates, i);
						assert( itPlayListElement );
						if (itPlayListElement->bandwidth == attribs.bandwidth) {
							currentPlayList = itPlayListElement;
							break;
						}
					}
				}

				if (attribs.isVariantPlaylist) {
					/* We are the Variant Playlist */
					if (currentPlayList != NULL) {
						/* should not happen, it means we redefine something previsouly added */
						//assert( 0 );
					}
					currentPlayList = playlist_element_new(
					                      TYPE_UNKNOWN,
					                      fullURL,
					                      attribs.title,
					                      attribs.codecs,
					                      attribs.durationInSeconds,
					                      attribs.byteRangeStart, attribs.byteRangeEnd);
					if (currentPlayList == NULL) {
						/* OUT of memory */
						variant_playlist_del(*playlist);
						playlist = NULL;
						if (f) fclose(f);
						return GF_OUT_OF_MEM;
					}
					assert( fullURL);
					currentPlayList->url = gf_strdup(fullURL);
					currentPlayList->title = attribs.title ? gf_strdup(attribs.title):NULL;
					currentPlayList->codecs = attribs.codecs ? gf_strdup(attribs.codecs):NULL;
					gf_list_add(program->bitrates, currentPlayList);
					currentPlayList->width = attribs.width;
					currentPlayList->height = attribs.height;
				} else {
					/* Normal Playlist */
					assert( pl->programs);
					if (currentPlayList == NULL) {
						/* This is in facts a "normal" playlist without any element in it */
						PlaylistElement * subElement;
						assert(baseURL);
						currentPlayList = playlist_element_new(
						                      TYPE_PLAYLIST,
						                      baseURL,
						                      attribs.title,
						                      attribs.codecs,
						                      attribs.durationInSeconds,
						                      attribs.byteRangeStart, attribs.byteRangeEnd);
						if (currentPlayList == NULL) {
							/* OUT of memory */
							variant_playlist_del(*playlist);
							playlist = NULL;
							if (f) fclose(f);
							return GF_OUT_OF_MEM;
						}
						assert(currentPlayList->element.playlist.elements);
						assert( fullURL);
						assert( currentPlayList->url);
						currentPlayList->title = NULL;
						currentPlayList->codecs = NULL;
						subElement = playlist_element_new(
						                 TYPE_UNKNOWN,
						                 fullURL,
						                 attribs.title,
						                 attribs.codecs,
						                 attribs.durationInSeconds,
						                 attribs.byteRangeStart, attribs.byteRangeEnd);
						if (subElement == NULL) {
							variant_playlist_del(*playlist);
							playlist_element_del(currentPlayList);
							playlist = NULL;
							if (f) fclose(f);
							return GF_OUT_OF_MEM;
						}
						gf_list_add(currentPlayList->element.playlist.elements, subElement);
						gf_list_add(program->bitrates, currentPlayList);
						currentPlayList->element.playlist.computed_duration += subElement->durationInfo;
						assert( program );
						assert( program->bitrates);
						assert( currentPlayList);

					} else {
						PlaylistElement * subElement = playlist_element_new(
						                                   TYPE_UNKNOWN,
						                                   fullURL,
						                                   attribs.title,
						                                   attribs.codecs,
						                                   attribs.durationInSeconds,
						                                   attribs.byteRangeStart, attribs.byteRangeEnd);
						if (currentPlayList->elementType != TYPE_PLAYLIST) {
							currentPlayList->elementType = TYPE_PLAYLIST;
							if (!currentPlayList->element.playlist.elements)
								currentPlayList->element.playlist.elements = gf_list_new();
						}
						if (subElement == NULL) {
							variant_playlist_del(*playlist);
							playlist_element_del(currentPlayList);
							playlist = NULL;
							if (f) fclose(f);
							return GF_OUT_OF_MEM;
						}
						gf_list_add(currentPlayList->element.playlist.elements, subElement);
						currentPlayList->element.playlist.computed_duration += subElement->durationInfo;
					}
				}

				currentPlayList->element.playlist.currentMediaSequence = attribs.currentMediaSequence ;
				/* We first set the default duration for element, aka targetDuration */
				if (attribs.targetDurationInSeconds > 0) {
					currentPlayList->element.playlist.target_duration = attribs.targetDurationInSeconds;
					currentPlayList->durationInfo = attribs.targetDurationInSeconds;
				}
				if (attribs.durationInSeconds) {
					if (currentPlayList->durationInfo == 0) {
						/* we set the playlist duration info as the duration of a segment, only if it's not set
						   There are cases of playlist with the last segment with a duration different from the others
						   (example: Apple bipbop test)*/
						currentPlayList->durationInfo = attribs.durationInSeconds;
					}
				}
				currentPlayList->element.playlist.mediaSequenceMin = attribs.minMediaSequence;
				currentPlayList->element.playlist.mediaSequenceMax = attribs.currentMediaSequence++;
				if (attribs.bandwidth > 1)
					currentPlayList->bandwidth = attribs.bandwidth;
				if (attribs.isPlaylistEnded)
					currentPlayList->element.playlist.is_ended = 1;
			}
			/* Cleanup all line-specific fields */
			if (attribs.title) {
				gf_free(attribs.title);
				attribs.title = NULL;
			}
			attribs.durationInSeconds = 0;
			attribs.bandwidth = 0;
			attribs.programId = 0;
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

	for (i=0; i < (int) gf_list_count(pl->programs); i++) {
		u32 j;
		Program *prog = gf_list_get(pl->programs, i);
		prog->computed_duration = 0;
		for (j=0; j<gf_list_count(prog->bitrates); j++) {
			PlaylistElement *ple = gf_list_get(prog->bitrates, j);
			if (ple->elementType==TYPE_PLAYLIST) {
				if (ple->element.playlist.computed_duration > prog->computed_duration)
					prog->computed_duration = ple->element.playlist.computed_duration;
			}
		}
	}
	return GF_OK;
}

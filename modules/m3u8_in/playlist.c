/**
 *			GPAC - Multimedia Framework C SDK
 *
 *			Copyright (c) Jean Le Feuvre 2000-2005
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
 *  Written by Pierre Souchay for VizionR SAS
 *
 */
#include "playlist.h"
#include <string.h>

GF_Err cleanup_list_of_elements(GF_List * list){
	int i, count;
	GF_Err result = GF_OK;
	if (list == NULL)
		return result;
	count = gf_list_count(list);
	for (i = 0; i < count ; i++){
		PlaylistElement * pl = (PlaylistElement *) gf_list_get(list, i);
		if (pl)
			result |= playlist_element_del(pl);
	}
	gf_list_del(list);
	return result;
}

GF_Err playlist_element_del(PlaylistElement * e){
	GF_Err result = GF_OK;
	if (e == NULL)
		return result;
	if (e->title){
		gf_free(e->title);
		e->title = NULL;
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
	return result;
}

Program * program_new(int programId){
	Program * program = malloc(sizeof(Program));
	if (program == NULL){
		return NULL;
	}
	program->programId = programId;
	program->bitrates = gf_list_new();
	if (program->bitrates == NULL){
		gf_free(program);
		return NULL;
	}
	return program;
}

GF_Err program_del(Program * program){
	int count, i;
	GF_Err e = GF_OK;
	if (program == NULL)
		return e;
	assert( program->bitrates);
	count = gf_list_count(program->bitrates);
	for (i = 0 ; i < count; i++){
		e |= cleanup_list_of_elements(gf_list_get(program->bitrates, i));
	}
	gf_list_del(program->bitrates);
	program->bitrates = NULL;
	return e;
}

/*
GF_Err playlist_del(Playlist * pl){
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

PlaylistElement * playlist_element_new(PlaylistElementType elementType, const char * url, const char * title, int durationInfo){
	PlaylistElement * e = malloc(sizeof(PlaylistElement));
	bzero(e, sizeof(PlaylistElement));
	assert( url );
	if (e == NULL)
		return NULL;
	e->durationInfo = durationInfo;
	e->title = (title ? strdup(title) : NULL);
	assert( url);
	e->url = strdup(url);
	e->bandwidth = 0;
	e->elementType = elementType;
	if (elementType == TYPE_PLAYLIST){
		e->element.playlist.is_ended = 0;
		e->element.playlist.target_duration = durationInfo;
		e->element.playlist.currentMediaSequence = 0;
		e->element.playlist.mediaSequenceMin = 0;
		e->element.playlist.mediaSequenceMax = 0;
		e->element.playlist.elements = gf_list_new();
		if (NULL == (e->element.playlist.elements)){
			if (e->title)
				gf_free(e->title);
			if (e->url)
				gf_free(e->url);
			e->url = NULL;
			e->title = NULL;
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
	Playlist * pl = malloc(sizeof(Playlist));
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
	VariantPlaylist * pl = malloc( sizeof(VariantPlaylist) );
	if (pl == NULL)
		return NULL;
	pl->programs = gf_list_new();
	if (! pl->programs){
		gf_free( pl );
		return NULL;
	}
	pl->currentProgram = -1;
	return pl;
}

GF_Err variant_playlist_del (VariantPlaylist * playlist){
	int count, i, count2, j;
	Program * p;
	if (playlist == NULL)
		return GF_OK;
	assert( playlist->programs);
	count = gf_list_count(playlist->programs);
	for (i = 0; i < count ; i++){
		p = gf_list_get(playlist->programs, i);
		assert(p);
		count2 = gf_list_count( p->bitrates );
		for (j = 0; j < count2; j++){
			PlaylistElement * pl = gf_list_get(p->bitrates, j);
			playlist_element_del(pl);
		}
		gf_list_del(p->bitrates);
		p->bitrates = NULL;
	}
	gf_list_del(playlist->programs);
	playlist->programs = NULL;
	gf_free(playlist);
	return GF_OK;
}

GF_Err playlist_element_dump(const PlaylistElement * e, int indent){
	int i;
	GF_Err r = GF_OK;
	for (i = 0 ; i < indent; i++)
		printf(" ");
	if (e == NULL){
		printf("NULL PlaylistElement\n");
		return r;
	}
	printf("PlayListElement[%p, title=%s, duration=%d, bandwidth=%d, url=%s, type=%s]\n",
		   (void*)e,
		   e->title,
		   e->durationInfo,
		   e->bandwidth,
		   e->url,
		   e->elementType == TYPE_STREAM ? "stream" : "playlist");
	if (TYPE_PLAYLIST == e->elementType){
		int sz;
		assert( e->element.playlist.elements);
		sz = gf_list_count(e->element.playlist.elements);
		indent+=2;
		for (i = 0 ; i < sz ; i++){
			PlaylistElement * el = gf_list_get(e->element.playlist.elements, i);
			assert( el);
			r|= playlist_element_dump( el, indent);
		}
	}
	return r;
}

GF_Err variant_playlist_dump(const VariantPlaylist * pl){
	int i, count;
	GF_Err e = GF_OK;
	if (pl == NULL){
		printf("VariantPlaylist = NULL\n");
		return e;
	}
	printf("VariantPlaylist = {\n");
	assert( pl->programs);
	count = gf_list_count( pl->programs);
	for (i = 0 ; i < count ; i++){
		int j, countj;
		Program * p = gf_list_get(pl->programs, i);
		assert( p );
		printf("  program[programId=%d]{\n",  p->programId);
		assert( p->bitrates );
		countj = gf_list_count(p->bitrates);
		for (j = 0; j < countj; j++){
			PlaylistElement * el = gf_list_get(p->bitrates, j);
			assert(el);
			e |= playlist_element_dump( el, 4);
		}
		printf("  }\n");
	}
	printf("}\n");
	return e;
}

Program * variant_playlist_find_matching_program(const VariantPlaylist * pl, const u32 programId){
	u32 count, i;
	assert( pl);
	assert( pl->programs);
	assert(programId >= 0);
	count = gf_list_count(pl->programs);
	for (i = 0 ; i < count ; i++){
		Program * cur = gf_list_get(pl->programs, i);
		assert( cur );
		if (programId == cur->programId){
			/* We found the program */
			return cur;
		} 
	}
	return NULL;
}

Program * variant_playlist_get_current_program(const VariantPlaylist * pl){
	assert( pl );
	return variant_playlist_find_matching_program(pl, pl->currentProgram);
}

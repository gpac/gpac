/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jerome Gorin
 *			Copyright (c) Telecom ParisTech 2000-2013
 *					All rights reserved
 *
 *  This file is part of GPAC / dashcast-Debug@dashcast_build
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
#include <gpac/map.h>
#include <gpac/list.h>

struct _tag_map
{
	u32 hash_capacity;
	GF_List** pairs;
};

GF_EXPORT
GF_Err gf_map_iter_set(GF_Map* map, GF_It_Map* it){
	/* Iterator must be associated to a map */
	if (!map || !it) return GF_BAD_PARAM;

	/* Associate iterator to the beginning of the map */
	it->map = map;
	it->ilist = 0;
	it->hash = 0;
	return GF_OK;
}

GF_EXPORT
void* gf_map_iter_has_next(GF_It_Map* it){
	GF_Pair* next_pair = NULL;

	/* No iterator or iterator out of map */
	if (!it || !(it->hash < it->map->hash_capacity)) return NULL;

	next_pair = (GF_Pair*)gf_list_get(it->map->pairs[it->hash], it->ilist);
	
	if (next_pair){
		/* Next element founds in the same hash */
		it->ilist++;
		return next_pair->value;
	}

	/* Take the next hash */
	it->hash++;
	it->ilist = 0;
	return gf_map_iter_has_next(it);
}

GF_EXPORT
GF_Err gf_map_iter_reset(GF_It_Map* it){
	if (!it) return GF_BAD_PARAM;
	it->hash = 0;
	it->ilist = 0;
	return GF_OK;
}


GF_Pair* gf_pair_new(const char* key, u32 key_len, void* item){
	GF_Pair* new_pair;

	/* Allocate space for a new pair */
	GF_SAFEALLOC(new_pair, GF_Pair);
	if (!new_pair)	return NULL;

	/* Allocate space for a new key */
	GF_SAFE_ALLOC_N(new_pair->key, key_len + 1, char);
	if (!new_pair->key) {
		gf_free(new_pair);
		return NULL;
	}

	/* Set pair value*/
	strcpy(new_pair->key, key);
	new_pair->value = item;

	return new_pair;
}

void gf_pair_del(GF_Pair* pair){
	gf_free(pair->key);
	gf_free(pair);
}

/* Match a key to a pair */
static GF_Pair* gf_pair_get(GF_List* pairs, const char *key)
{
	GF_Pair *pair;
	u32 index = 0;

	while ((pair = (GF_Pair*)gf_list_get(pairs, index))) {
		if (pair->key && !strcmp(pair->key, key)) return pair;
		index++;
	}

	/* Key not found */
	return NULL;
}

/* Remove a key from pairs */
static Bool gf_pair_rem(GF_List* pairs, const char *key)
{
	GF_Pair* pair;
	u32 index = 0;

	while ((pair = (GF_Pair*)gf_list_get(pairs, index))) {
		if (pair->key && !strcmp(pair->key, key)){
			gf_list_rem(pairs, index);
			gf_pair_del(pair);
			return GF_TRUE;
		}
		index++;
	}

	/* Key not found */
	return GF_FALSE;
}


/* Generate a hash code for the provided string */
static unsigned long hash(const char *str)
{
	u32 hash = 5381;
	int c;

	while ((c = *str++)) {
		hash = ((hash << 5) + hash) + c;
	}
	return hash;
}



GF_EXPORT
GF_Map * gf_map_new(u32 hash_capacity)
{
	GF_Map *p_map;
	GF_SAFEALLOC(p_map, GF_Map);
	if (! p_map) return NULL;

	p_map->hash_capacity = hash_capacity;
	GF_SAFE_ALLOC_N(p_map->pairs, p_map->hash_capacity, GF_List*);
	if (!p_map->pairs) {
		gf_free(p_map);
		return NULL;
	}

	return p_map;
}

GF_EXPORT
void gf_map_del(GF_Map *ptr)
{
	u32 i, j;
	GF_Pair *pair;
	GF_List *bucket;

	if (!ptr) return;

	/* Iterate through the map and delete content */
	for (i = 0; i < ptr->hash_capacity; i++){
		bucket = ptr->pairs[i];

		/* Bucket has not be allocated */
		if (bucket == NULL) continue;

		/* Iterate through the bucket and delete pairs */
		j = 0;
		while ((pair = (GF_Pair*)gf_list_get(bucket, j))) {
			gf_pair_del(pair);
			j++;
		}

		gf_list_del(bucket);
	}

	/* Free map */
	gf_free(ptr->pairs);
	gf_free(ptr);
}

GF_EXPORT
void gf_map_reset(GF_Map *ptr){
	u32 i, j;
	GF_Pair *pair;
	GF_List *bucket;

	/* Iterate through the map and delete content */
	for (i = 0; i < ptr->hash_capacity; i++){
		bucket = ptr->pairs[i];

		/* Bucket has not be allocated */
		if (bucket == NULL)
			continue;

		/* Iterate through the bucket and delete pairs */
		j = 0;
		while ((pair = (GF_Pair*)gf_list_get(bucket, j))) {
			gf_pair_del(pair);
			j++;
		}

		/* reset bucket count */
		gf_list_del(bucket);
		ptr->pairs[i] = NULL;
	}
}

GF_EXPORT
void* gf_map_find(GF_Map *ptr, const char* key){
	u32 index;
	GF_List *bucket;
	GF_Pair *pair;

	/* Find requires a map and a key to work properly */
	if (!ptr || !key) return NULL;

	index = hash(key) % ptr->hash_capacity;
	bucket = ptr->pairs[index];
	pair = gf_pair_get(bucket, key);

	/* bucket is empty */
	if (!pair) return NULL;

	return pair->value;
}


GF_EXPORT
GF_Err gf_map_insert(GF_Map *ptr, const char* key, void* item){
	u32 key_len, index;
	GF_List *bucket;
	GF_Pair *pair;
	GF_Err err;

	/* Insert requiered a map, a key and a value */
	if (!ptr || !key || !item) return GF_BAD_PARAM;

	key_len = (u32) strlen(key);

	/* Get a pointer to the bucket the key string hashes to */
	index = hash(key) % ptr->hash_capacity;
	bucket = ptr->pairs[index];

	if (bucket && (pair = gf_pair_get(bucket, key))) {
		/* The bucket already contains the provided key */
		return GF_NOT_SUPPORTED;
	}

	if (!bucket){
		/* No bucket has been assigned yet */
		bucket = gf_list_new();
		if (!bucket) return GF_OUT_OF_MEM;

		ptr->pairs[index] = bucket;
	}

	pair = gf_pair_new(key, key_len, item);

	if (!pair){
		return GF_OUT_OF_MEM;
	}

	/* Store the new pair in the bucket */
	err = gf_list_add(bucket, pair);
	if(err != GF_OK){
		gf_free(pair);
		return GF_OUT_OF_MEM;
	}

	return GF_OK;
}

GF_EXPORT
Bool gf_map_rem(GF_Map *ptr, const char* key)
{
	u32 index;
	GF_List *bucket;

	/* Find requires a map and a key to work properly */
	if (!ptr || !key) return GF_FALSE;

	index = hash(key) % ptr->hash_capacity;
	bucket = ptr->pairs[index];

	return gf_pair_rem(bucket, key);
}

GF_EXPORT
u32 gf_map_count(const GF_Map *ptr){
	u32 i = 0;
	u32 count = 0;

	/* No map provided */
	if (!ptr) return GF_BAD_PARAM;

	/* Iterate through all bucket of the map and count pair */
	for (; i < ptr->hash_capacity; i++ ){
		count += gf_list_count(ptr->pairs[i]);
	}

	return count;
}

GF_EXPORT
Bool gf_map_has_key(GF_Map *ptr, const char* key){
	unsigned int index;
	GF_List *bucket;

	/* Need a map  and key */
	if (!ptr || !key) return GF_FALSE;

	index = hash(key) % ptr->hash_capacity;
	bucket = ptr->pairs[index];

	/* Check if pair exists */
	if (gf_pair_get(bucket, key)) return GF_TRUE;

	return GF_FALSE;
}

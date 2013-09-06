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

#ifndef _GF_MAP_H_
#define _GF_MAP_H_
#ifdef __cplusplus
extern "C" {
#endif

/*!
 *	\file <map.h>
 *	\brief map functions.
 */

/*!
 *	\addtogroup  map_grp map
 *	\ingroup utils_grp
 *	\brief 
 *
 *	This section documents the map object of the GPAC framework
 *	\note The map use a random function for hashing. Collisions are resolved by using a GF_List on each slot.
 *	@{
 */

#include <gpac/tools.h>


/**
 * \struct GF_Map
 * \brief Map Object.
 *
 * A GF_Map associate key to values.
 */
typedef struct _tag_map GF_Map;


/* Pair structure */

/**
 * \struct GF_Pair
 * \brief Pair structure.
 *
 * Define the association Key/value.
 */
typedef struct {
	char *key;
	void *value;
} GF_Pair;


/**
 * \struct GF_Iterator
 * \brief Map iterator Object.
 *
 * Allows to go through each pair key/value in the map.
 */
typedef struct _it_map{
	/* The current map*/
	GF_Map* map;

	/* The current pair */
	GF_Pair* pair;

	/* The current list */
	u32 ilist;

	/* The current index in the hasmap*/
	u32 hash;
	
}GF_It_Map;

/*!
 *	\brief map constructor
 *
 *	Constructs a new map object
 *
 *	\param hash_capacity the number of slot in the hash table
 *	\note a zero hash_capacity is not allowed, a hash_capacity of 1 is
 *	equivalent to a gf_list_find from GF_List with a complexity search in o(n)
 *	\return new map object
 */
GF_Map *gf_map_new(u32 hash_capacity);

/*!
 *	\brief map destructor
 *
 *	Destructs a map object
 *	\param ptr map object to destruct
 *	\note The caller is only responsible to destroy the content of the map, not the key
 */
void gf_map_del(GF_Map *ptr);

/*!
 *	\brief Set a new map iterator
 *
 *	Associate a map iterator to a map
 *
 *	\param map the map associated to the iterator
 *	\param it the resulting iterator
 *	\return GF_OK if iterator has been set properly, otherwise a GF_Err
 */
GF_Err gf_map_iter_set(GF_Map* map, GF_It_Map* it);

/*!
 *	\brief return the next value in the map
 *
 *	Return the next value of a GF_Pair in the map
 *	\param it  the map iterator object
 *  \return the next value of the map if exists, otherwise NULL
 */
void* gf_map_iter_has_next(GF_It_Map* it);

/*!
 *	\brief Reset the iterator in the map
 *
 *	Reinitalize the iterator to the beginning of the map
 *	\param it  the map iterator object
 *  \return GF_OK if the iterator has been correctly reinitialize, otherwise a GF_Err
 */
GF_Err gf_map_iter_reset(GF_It_Map* it);

/*!
 *	\brief get count
 *
 *	Returns number of items in the map
 *	\param ptr target map object
 *	\return number of items in the map
 */
u32 gf_map_count(const GF_Map *ptr);

/*!
 *	\brief add item
 *
 *	Adds an item in the map with an associated key
 *
 *	\param ptr target map object
 *	\param key the identified key
 *	\param item item to add
 *	\return GF_OF if insertion occurs properly, GF_NOT_SUPPORTED if the key already exists, a GF_Err in other cases
 *	\note the caller is responsible for the deallocation of the key as it is copyied in the map
 */
GF_Err gf_map_insert(GF_Map *ptr, const char* key, void* item);

/*!
 *	\brief removes the couple key/item from the map
 *
 *	Removes an item from the map given to its key if exists
 *	\param ptr target map object
 *	\param key the key of the item.
 *	\return GF_TRUE if the key has been delete, otherwise GF_FALSE
 *	\note It is the caller responsability to destroy the content of the list if needed
 */
Bool gf_map_rem(GF_Map *ptr, const char* key);

/*!
 *	\brief finds item
 *
 *	Finds a key in the map
 *
 *	\param ptr target map object.
 *	\param key the key to find.
 *	\return a pointer to the corresponding value if found, otherwise NULL.
 */
void* gf_map_find(GF_Map *ptr, const char* key);

/*!
 *	\brief Check if map contains key
 *
 *	\param ptr target map object.
 *	\param key the key to check.
 *	\return GF_TRUE if map contains keys, otherwise GF_FALSE
 */
Bool gf_map_has_key(GF_Map *ptr, const char* key);

/*!
 *	\brief resets map
 *
 *	Resets the content of the map
 *	\param ptr target map object.
 *	\note It is the caller responsability to destroy the content of the list if needed
 */
void gf_map_reset(GF_Map *ptr);


/*! @} */

#ifdef __cplusplus
}
#endif

#endif /* _GF_MAP_H_ */

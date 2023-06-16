/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2000-2023
 *					All rights reserved
 *
 *  This file is part of GPAC / common tools sub-project
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

#ifndef _GF_LIST_H_
#define _GF_LIST_H_

#ifdef __cplusplus
extern "C" {
#endif

/*!
\file <gpac/list.h>
\brief Generic list of objects.
 */

/*!
\addtogroup list_grp
\brief List object

This section documents the list object of the GPAC framework.

@{
 */

#include <gpac/tools.h>

/*! list object*/
typedef struct _tag_array GF_List;

/*!
\brief list constructor

Constructs a new list object
\return new list object
 */
GF_List *gf_list_new();

/*!
\brief list destructor

Destructs a list object
\param ptr list object to destruct
\note It is the caller responsibility to destroy the content of the list if needed
 */
void gf_list_del(GF_List *ptr);

/*!
\brief get count

Returns number of items in the list
\param ptr target list object
\return number of items in the list
 */
u32 gf_list_count(const GF_List *ptr);

/*!
\brief add item

Adds an item at the end of the list
\param ptr target list object
\param item item to add
\return error if any
 */
GF_Err gf_list_add(GF_List *ptr, void* item);

/*!
\brief inserts item

Insert an item in the list
\param ptr target list object
\param item item to add
\param position insertion position. It is expressed between 0 and gf_list_count-1, and any bigger value is equivalent to gf_list_add
\return error if any
 */
GF_Err gf_list_insert(GF_List *ptr, void *item, u32 position);

/*!
\brief removes item

Removes an item from the list given its position
\param ptr target list object
\param position position of the item to remove. It is expressed between 0 and gf_list_count-1.
\return error if any
\note It is the caller responsibility to destroy the content of the list if needed
 */
GF_Err gf_list_rem(GF_List *ptr, u32 position);

/*!
\brief gets item

Gets an item from the list given its position
\param ptr target list object
\param position position of the item to get. It is expressed between 0 and gf_list_count-1.
\return the item or NULL if not found
 */
void *gf_list_get(GF_List *ptr, u32 position);

/*!
\brief finds item

Finds an item in the list
\param ptr target list object.
\param item the item to find.
\return 0-based item position in the list, or -1 if the item could not be found.
 */
s32 gf_list_find(GF_List *ptr, void *item);

/*!
\brief deletes item

Deletes an item from the list
\param ptr target list object.
\param item the item to find.
\return 0-based item position in the list before removal, or -1 if the item could not be found.
 */
s32 gf_list_del_item(GF_List *ptr, void *item);

/*!
\brief resets list

Resets the content of the list
\param ptr target list object.
\note It is the caller responsibility to destroy the content of the list if needed
 */
void gf_list_reset(GF_List *ptr);

/*!
\brief gets last item

Gets last item o fthe list
\param ptr target list object
\return the last item
 */
void *gf_list_last(GF_List *ptr);

/*!
\brief removes last item

Removes the last item of the list
\param ptr target list object
\return error if any
\note It is the caller responsibility to destroy the content of the list if needed
 */
GF_Err gf_list_rem_last(GF_List *ptr);


/*!
\brief list enumerator

Retrieves given list item and increment current position
\param ptr target list object
\param pos target item position. The position is automatically incremented regardless of the return value
\note A typical enumeration will start with a value of 0 until NULL is returned.
\return the current item for the given index
*/
void *gf_list_enum(GF_List *ptr, u32 *pos);

/*!
\brief list swap

Swaps content of two lists
\param l1 first list object
\param l2 second list object
\return error if any
 */
GF_Err gf_list_swap(GF_List *l1, GF_List *l2);

/*!
\brief list transfer

Transfer content between lists
\param dst destination list object
\param src source list object
\return error if any
 */
GF_Err gf_list_transfer(GF_List *dst, GF_List *src);

/*!
\brief clone list

Returns a new list as an exact copy of the given list
\param ptr the list to clone
\return the cloned list
 */
GF_List* gf_list_clone(GF_List *ptr);

/*!
\brief Pop the first element in the list

Removes the first element in the list container, effectively reducing its size by one and returns the popped element.
\param ptr the list to pop
\return the popped element
 */
void* gf_list_pop_front(GF_List *ptr);

/*!
\brief Pop the last element in the list

Removes the last element in the list container, effectively reducing the container size by one and return the popped element.
\param ptr the list to pop
\return the popped element
 */
void* gf_list_pop_back(GF_List *ptr);

/*! @} */

#ifdef __cplusplus
}
#endif


#endif		/*_GF_LIST_H_*/


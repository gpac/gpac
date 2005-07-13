/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Copyright (c) Jean Le Feuvre 2000-2005 
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

#include <gpac/tools.h>

/**********************************************************************
					CHAIN: list tool
**********************************************************************/
typedef struct _tag_array GF_List;

/*array constructor*/
GF_List *gf_list_new();

/*array destructor - you must delete your objects*/
void gf_list_del(GF_List *ptr);

/* Get the number of items */
u32 gf_list_count(GF_List *ptr);

/* Add an item at the end of the array */
GF_Err gf_list_add(GF_List *ptr, void* item);

/* insert an item at the specified position (between 0 and gf_list_count() - 1) */
GF_Err gf_list_insert(GF_List *ptr, void *item, u32 position);

/* Delete an entry from the array (between 0 and gf_list_count() - 1)*/
GF_Err gf_list_rem(GF_List *ptr, u32 item_number);

/* get the specified entry (between 0 and gf_list_count() - 1) */
void *gf_list_get(GF_List *ptr, u32 item_number);

/*returns entry index if item has been found, -1 otherwise*/
s32 gf_list_find(GF_List *ptr, void *item);

/*removes item by pointer. returns entry index if found, -1 otherwise*/
s32 gf_list_del_item(GF_List *ptr, void *item);

/*empty list content*/
void gf_list_reset(GF_List *ptr);


#ifdef __cplusplus
}
#endif


#endif		/*_GF_LIST_H_*/


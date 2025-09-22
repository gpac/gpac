/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2000-2020
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

#include <gpac/list.h>

/* GF_List modes, ONLY ONE CAN BE DEFINED

	linked-list
	#define GF_LIST_LINKED

	double navigation linked-list
	#define GF_LIST_DOUBLE_LINKED

	single step memory array
	#define GF_LIST_ARRAY

	multi-step memory array without gf_realloc on remove, using the GF_LIST_REALLOC macro
	GF_LIST_ARRAY_GROW
*/

/*after some tuning, this seems to be the fastest mode on WINCE*/
#ifdef _WIN32_WCE
#define GF_LIST_LINKED
#else
#define GF_LIST_ARRAY_GROW
#endif

#define GF_LIST_REALLOC(a) (a = a ? (3*a/2) : 10)
//#define GF_LIST_REALLOC(a) (a++)


#if defined(GF_LIST_LINKED)

typedef struct tagIS
{
	struct tagIS *next;
	void *data;
} ItemSlot;

struct _tag_array
{
	struct tagIS *head;
	struct tagIS *tail;
	u32 entryCount;
	s32 foundEntryNumber;
	struct tagIS *foundEntry;
};


GF_EXPORT
GF_List * gf_list_new()
{
	GF_List *nlist = (GF_List *) gf_malloc(sizeof(GF_List));
	if (! nlist) return NULL;
	nlist->head = nlist->foundEntry = NULL;
	nlist->tail = NULL;
	nlist->foundEntryNumber = -1;
	nlist->entryCount = 0;
	return nlist;
}

GF_EXPORT
void gf_list_del(GF_List *ptr)
{
	if (!ptr) return;
	while (ptr->entryCount) gf_list_rem(ptr, 0);
	gf_free(ptr);
}

GF_EXPORT
void gf_list_reset(GF_List *ptr)
{
	while (ptr && ptr->entryCount) gf_list_rem(ptr, 0);
}

GF_EXPORT
GF_Err gf_list_add(GF_List *ptr, void* item)
{
	ItemSlot *entry;
	if (! ptr) return GF_BAD_PARAM;
	entry = (ItemSlot *) gf_malloc(sizeof(ItemSlot));
	if (!entry) return GF_OUT_OF_MEM;
	entry->data = item;
	entry->next = NULL;
	if (! ptr->head) {
		ptr->head = entry;
		ptr->entryCount = 1;
	} else {
		ptr->entryCount += 1;
		ptr->tail->next = entry;
	}
	ptr->tail = entry;
	ptr->foundEntryNumber = ptr->entryCount - 1;
	ptr->foundEntry = entry;
	return GF_OK;
}

GF_EXPORT
u32 gf_list_count(const GF_List *ptr)
{
	if (!ptr) return 0;
	return ptr->entryCount;
}

GF_EXPORT
void *gf_list_get(GF_List *ptr, u32 itemNumber)
{
	ItemSlot *entry;
	u32 i;

	if (!ptr || (itemNumber >= ptr->entryCount) ) return NULL;

	if (!ptr->foundEntry || (itemNumber < (u32) ptr->foundEntryNumber) ) {
		ptr->foundEntryNumber = 0;
		ptr->foundEntry = ptr->head;
	}
	entry = ptr->foundEntry;
	for (i = ptr->foundEntryNumber; i < itemNumber; i++ ) {
		entry = entry->next;
	}
	ptr->foundEntryNumber = itemNumber;
	ptr->foundEntry = entry;
	return (void *) entry->data;
}

GF_EXPORT
void *gf_list_last(GF_List *ptr)
{
	ItemSlot *entry;
	if (!ptr || !ptr->entryCount) return NULL;
	entry = ptr->head;
	while (entry->next) entry = entry->next;
	return entry->data;
}

GF_EXPORT
GF_Err gf_list_rem(GF_List *ptr, u32 itemNumber)
{
	ItemSlot *tmp, *tmp2;
	u32 i;

	/* !! if head is null (empty list)*/
	if ( (! ptr) || (! ptr->head) || (ptr->head && !ptr->entryCount) || (itemNumber >= ptr->entryCount) )
		return GF_BAD_PARAM;

	/*we delete the head*/
	if (! itemNumber) {
		tmp = ptr->head;
		ptr->head = ptr->head->next;
		ptr->entryCount --;
		ptr->foundEntry = ptr->head;
		ptr->foundEntryNumber = 0;
		gf_free(tmp);
		/*that was the last entry, reset the tail*/
		if (!ptr->entryCount) {
			ptr->tail = ptr->head = ptr->foundEntry = NULL;
			ptr->foundEntryNumber = -1;
		}
		return GF_OK;
	}

	tmp = ptr->head;
	i = 0;
	while (i < itemNumber - 1) {
		tmp = tmp->next;
		i++;
	}
	tmp2 = tmp->next;
	tmp->next = tmp2->next;
	/*if we deleted the last entry, update the tail !!!*/
	if (! tmp->next || (ptr->tail == tmp2) ) {
		ptr->tail = tmp;
		tmp->next = NULL;
	}

	gf_free(tmp2);
	ptr->entryCount --;
	ptr->foundEntry = ptr->head;
	ptr->foundEntryNumber = 0;

	return GF_OK;
}

GF_EXPORT
GF_Err gf_list_rem_last(GF_List *ptr)
{
	return gf_list_rem(ptr, ptr->entryCount-1);
}

GF_EXPORT
GF_Err gf_list_insert(GF_List *ptr, void *item, u32 position)
{
	u32 i;
	ItemSlot *tmp, *tmp2;

	if (!ptr || !item) return GF_BAD_PARAM;
	/*if last entry or first of an empty array...*/
	if (position >= ptr->entryCount) return gf_list_add(ptr, item);

	tmp2 = (ItemSlot *) gf_malloc(sizeof(ItemSlot));
	tmp2->data = item;
	tmp2->next = NULL;
	/*special case for the head*/
	if (position == 0) {
		tmp2->next = ptr->head;
		ptr->head = tmp2;
		ptr->entryCount ++;
		ptr->foundEntry = tmp2;
		ptr->foundEntryNumber = 0;
		return GF_OK;
	}
	tmp = ptr->head;
	for (i = 1; i < position; i++) {
		tmp = tmp->next;
		if (!tmp)
			break;
	}
	tmp2->next = tmp->next;
	tmp->next = tmp2;
	ptr->entryCount ++;
	ptr->foundEntry = tmp2;
	ptr->foundEntryNumber = i;
	return GF_OK;
}

#elif defined(GF_LIST_DOUBLE_LINKED)


typedef struct tagIS
{
	struct tagIS *next;
	struct tagIS *prev;
	void *data;
} ItemSlot;

struct _tag_array
{
	struct tagIS *head;
	struct tagIS *tail;
	u32 entryCount;
	s32 foundEntryNumber;
	struct tagIS *foundEntry;
};


GF_EXPORT
GF_List * gf_list_new()
{
	GF_List *nlist = (GF_List *) gf_malloc(sizeof(GF_List));
	if (! nlist) return NULL;
	nlist->head = nlist->foundEntry = NULL;
	nlist->tail = NULL;
	nlist->foundEntryNumber = -1;
	nlist->entryCount = 0;
	return nlist;
}

GF_List * gf_list_new_prealloc(u32 nb_prealloc)
{
	if (!nb_prealloc) return NULL;
	return gf_list_new();
}

GF_EXPORT
void gf_list_del(GF_List *ptr)
{
	if (!ptr) return;
	while (ptr->entryCount) {
		gf_list_rem(ptr, 0);
	}
	gf_free(ptr);
}

GF_EXPORT
void gf_list_reset(GF_List *ptr)
{
	while (ptr && ptr->entryCount) gf_list_rem(ptr, 0);
}

GF_EXPORT
GF_Err gf_list_add(GF_List *ptr, void* item)
{
	ItemSlot *entry;
	if (! ptr) return GF_BAD_PARAM;
	entry = (ItemSlot *) gf_malloc(sizeof(ItemSlot));
	if (!entry) return GF_OUT_OF_MEM;
	entry->data = item;
	entry->next = entry->prev = NULL;

	if (! ptr->head) {
		ptr->head = entry;
		ptr->entryCount = 1;
	} else {
		ptr->entryCount += 1;
		entry->prev = ptr->tail;
		ptr->tail->next = entry;
	}
	ptr->tail = entry;
	ptr->foundEntryNumber = ptr->entryCount - 1;
	ptr->foundEntry = entry;
	return GF_OK;
}


GF_EXPORT
u32 gf_list_count(GF_List *ptr)
{
	if (! ptr) return 0;
	return ptr->entryCount;
}

GF_EXPORT
void *gf_list_get(GF_List *ptr, u32 itemNumber)
{
	ItemSlot *entry;
	u32 i;

	if (!ptr || !ptr->head || (itemNumber >= ptr->entryCount) ) return NULL;

	if (!itemNumber) {
		ptr->foundEntry = ptr->head;
		ptr->foundEntryNumber = 0;
		return ptr->head->data;
	}

	entry = ptr->foundEntry;
	if ( itemNumber < (u32) ptr->foundEntryNumber ) {
		for (i = ptr->foundEntryNumber; i > itemNumber; i-- ) {
			entry = entry->prev;
		}
	} else {
		for (i = ptr->foundEntryNumber; i < itemNumber; i++ ) {
			entry = entry->next;
		}
	}
	ptr->foundEntryNumber = itemNumber;
	ptr->foundEntry = entry;
	return (void *) entry->data;
}

GF_EXPORT
void *gf_list_last(GF_List *ptr)
{
	if(!ptr || !ptr->tail) return NULL;
	return ptr->tail->data;
}

GF_EXPORT
GF_Err gf_list_rem(GF_List *ptr, u32 itemNumber)
{
	ItemSlot *tmp;
	u32 i;

	/* !! if head is null (empty list)*/
	if ( (! ptr) || (! ptr->head) || (ptr->head && !ptr->entryCount) || (itemNumber >= ptr->entryCount) )
		return GF_BAD_PARAM;

	/*we delete the head*/
	if (! itemNumber) {
		tmp = ptr->head;
		ptr->head = ptr->head->next;

		ptr->entryCount --;
		ptr->foundEntry = ptr->head;
		ptr->foundEntryNumber = 0;
		gf_free(tmp);

		/*that was the last entry, reset the tail*/
		if (!ptr->entryCount) {
			ptr->tail = ptr->head = ptr->foundEntry = NULL;
			ptr->foundEntryNumber = -1;
		} else {
			ptr->head->prev = NULL;
		}
		return GF_OK;
	}
	else if (itemNumber==ptr->entryCount-1) {
		tmp = ptr->tail;
		ptr->tail = tmp->prev;
		ptr->tail->next = NULL;
		ptr->entryCount--;
		if (ptr->foundEntry==tmp) {
			ptr->foundEntry = ptr->tail;
			ptr->foundEntryNumber = ptr->entryCount-1;
		}
		gf_free(tmp);
		return GF_OK;
	}

	tmp = ptr->foundEntry;
	if ( itemNumber < (u32) ptr->foundEntryNumber ) {
		for (i = ptr->foundEntryNumber; i > itemNumber; i-- ) {
			tmp = tmp->prev;
		}
	} else {
		for (i = ptr->foundEntryNumber; i < itemNumber; i++ ) {
			tmp = tmp->next;
		}
	}
	tmp->prev->next = tmp->next;
	tmp->next->prev = tmp->prev;
	if (tmp==ptr->foundEntry) ptr->foundEntry = tmp->next;
	gf_free(tmp);
	ptr->entryCount--;
	return GF_OK;
}

GF_EXPORT
GF_Err gf_list_rem_last(GF_List *ptr)
{
	return gf_list_rem(ptr, ptr->entryCount-1);
}

GF_EXPORT
GF_Err gf_list_insert(GF_List *ptr, void *item, u32 position)
{
	u32 i;
	ItemSlot *tmp, *tmp2;

	if (!ptr || !item) return GF_BAD_PARAM;
	/*if last entry or first of an empty array...*/
	if (position >= ptr->entryCount) return gf_list_add(ptr, item);
	tmp2 = (ItemSlot *) gf_malloc(sizeof(ItemSlot));
	tmp2->data = item;
	tmp2->next = tmp2->prev = NULL;
	/*special case for the head*/
	if (position == 0) {
		ptr->head->prev = tmp2;
		tmp2->next = ptr->head;
		ptr->head = tmp2;
		ptr->entryCount ++;
		ptr->foundEntry = tmp2;
		ptr->foundEntryNumber = 0;
		return GF_OK;
	}

	tmp = ptr->foundEntry;
	if ( position < (u32) ptr->foundEntryNumber ) {
		for (i = ptr->foundEntryNumber; i >= position; i-- ) {
			tmp = tmp->prev;
		}
		tmp = tmp->prev;
	} else {
		for (i = ptr->foundEntryNumber; i < position; i++ ) {
			tmp = tmp->next;
		}
	}
	tmp2->next = tmp->next;
	tmp2->next->prev = tmp2;
	tmp2->prev = tmp;
	tmp2->prev->next = tmp2;
	ptr->entryCount ++;
	ptr->foundEntry = tmp2;
	ptr->foundEntryNumber = position;
	return GF_OK;
}

#elif defined(GF_LIST_ARRAY)

struct _tag_array
{
	void **slots;
	u32 entryCount;
};


GF_EXPORT
GF_List * gf_list_new()
{
	GF_List *nlist = (GF_List *) gf_malloc(sizeof(GF_List));
	if (! nlist) return NULL;
	nlist->slots = NULL;
	nlist->entryCount = 0;
	return nlist;
}

GF_List * gf_list_new_prealloc(u32 nb_prealloc)
{
	if (!nb_prealloc) return NULL;
	return gf_list_new();
}
GF_EXPORT
void gf_list_del(GF_List *ptr)
{
	if (!ptr) return;
	gf_free(ptr->slots);
	gf_free(ptr);
}

GF_EXPORT
GF_Err gf_list_add(GF_List *ptr, void* item)
{
	if (! ptr) return GF_BAD_PARAM;

	ptr->slots = (void **) gf_realloc(ptr->slots, (ptr->entryCount+1)*sizeof(void*));
	if (!ptr->slots) {
		return GF_OUT_OF_MEM;
	}
	ptr->slots[ptr->entryCount] = item;
	ptr->entryCount ++;
	return GF_OK;
}

GF_EXPORT
u32 gf_list_count(GF_List *ptr)
{
	return ptr ? ptr->entryCount : 0;
}

GF_EXPORT
void *gf_list_get(GF_List *ptr, u32 itemNumber)
{
	if(!ptr || (itemNumber >= ptr->entryCount)) return NULL;
	return ptr->slots[itemNumber];
}

GF_EXPORT
void *gf_list_last(GF_List *ptr)
{
	if(!ptr || !ptr->entryCount) return NULL;
	return ptr->slots[ptr->entryCount-1];
}


/*WARNING: itemNumber is from 0 to entryCount - 1*/
GF_EXPORT
GF_Err gf_list_rem(GF_List *ptr, u32 itemNumber)
{
	u32 i;
	if ( !ptr || !ptr->slots || !ptr->entryCount) return GF_BAD_PARAM;
	i = ptr->entryCount - itemNumber - 1;
	if (i) memmove(&ptr->slots[itemNumber], & ptr->slots[itemNumber +1], sizeof(void *)*i);
	ptr->slots[ptr->entryCount-1] = NULL;
	ptr->entryCount -= 1;
	ptr->slots = (void **) gf_realloc(ptr->slots, sizeof(void*)*ptr->entryCount);
	return GF_OK;
}

GF_EXPORT
GF_Err gf_list_rem_last(GF_List *ptr)
{
	if ( !ptr || !ptr->slots || !ptr->entryCount) return GF_BAD_PARAM;
	ptr->entryCount -= 1;
	ptr->slots = (void **) gf_realloc(ptr->slots, sizeof(void*)*ptr->entryCount);
	return GF_OK;
}


/*WARNING: position is from 0 to entryCount - 1*/
GF_EXPORT
GF_Err gf_list_insert(GF_List *ptr, void *item, u32 position)
{
	u32 i;
	if (!ptr || !item) return GF_BAD_PARAM;
	/*if last entry or first of an empty array...*/
	if (position >= ptr->entryCount) return gf_list_add(ptr, item);
	ptr->slots = (void **) gf_realloc(ptr->slots, (ptr->entryCount+1)*sizeof(void*));
	i = ptr->entryCount - position;
	memmove(&ptr->slots[position + 1], &ptr->slots[position], sizeof(void *)*i);
	ptr->entryCount++;
	ptr->slots[position] = item;
	return GF_OK;
}

GF_EXPORT
void gf_list_reset(GF_List *ptr)
{
	if (ptr) {
		ptr->entryCount = 0;
		gf_free(ptr->slots);
		ptr->slots = NULL;
	}
}

#else	/*GF_LIST_ARRAY_GROW*/


struct _tag_array
{
	void **slots;
	u32 entryCount;
	u32 allocSize;
};

GF_EXPORT
GF_List * gf_list_new()
{
	GF_List *nlist;

	nlist = (GF_List *) gf_malloc(sizeof(GF_List));
	if (! nlist) return NULL;

	nlist->slots = NULL;
	nlist->entryCount = 0;
	nlist->allocSize = 0;
	return nlist;
}

GF_List * gf_list_new_prealloc(u32 nb_prealloc)
{
	if (!nb_prealloc) return NULL;
	GF_List *nlist = gf_list_new();
	if (nlist) {
		nlist->allocSize = nb_prealloc;
		nlist->slots = (void**)gf_malloc(nlist->allocSize*sizeof(void*));
	}
	return nlist;
}

GF_EXPORT
void gf_list_del(GF_List *ptr)
{
	if (!ptr) return;
	gf_free(ptr->slots);
	gf_free(ptr);
}

static void realloc_chain(GF_List *ptr)
{
	GF_LIST_REALLOC(ptr->allocSize);
	ptr->slots = (void**)gf_realloc(ptr->slots, ptr->allocSize*sizeof(void*));
}

GF_EXPORT
GF_Err gf_list_add(GF_List *ptr, void* item)
{
	if (! ptr) return GF_BAD_PARAM;
	if (! item)
		return GF_BAD_PARAM;
	if (ptr->allocSize==ptr->entryCount) realloc_chain(ptr);
	if (!ptr->slots) return GF_OUT_OF_MEM;

	ptr->slots[ptr->entryCount] = item;
	ptr->entryCount ++;
	return GF_OK;
}

GF_EXPORT
u32 gf_list_count(const GF_List *ptr)
{
	if (!ptr) return 0;
	return ptr->entryCount;
}

GF_EXPORT
void *gf_list_get(GF_List *ptr, u32 itemNumber)
{
	if(!ptr || (itemNumber >= ptr->entryCount))
		return NULL;
	return ptr->slots[itemNumber];
}

GF_EXPORT
void *gf_list_last(GF_List *ptr)
{
	if(!ptr || !ptr->entryCount) return NULL;
	return ptr->slots[ptr->entryCount-1];
}


/*WARNING: itemNumber is from 0 to entryCount - 1*/
GF_EXPORT
GF_Err gf_list_rem(GF_List *ptr, u32 itemNumber)
{
	u32 i;
	if ( !ptr || !ptr->slots || !ptr->entryCount) return GF_BAD_PARAM;
	if (ptr->entryCount < itemNumber) return GF_BAD_PARAM;
	
	i = ptr->entryCount - itemNumber - 1;
	if (i) memmove(&ptr->slots[itemNumber], & ptr->slots[itemNumber +1], sizeof(void *)*i);
	ptr->slots[ptr->entryCount-1] = NULL;
	ptr->entryCount -= 1;
	return GF_OK;
}

GF_EXPORT
GF_Err gf_list_rem_last(GF_List *ptr)
{
	if ( !ptr || !ptr->slots || !ptr->entryCount) return GF_BAD_PARAM;
	ptr->slots[ptr->entryCount-1] = NULL;
	ptr->entryCount -= 1;
	return GF_OK;
}

/*WARNING: position is from 0 to entryCount - 1*/
GF_EXPORT
GF_Err gf_list_insert(GF_List *ptr, void *item, u32 position)
{
	u32 i;
	if (!ptr || !item) return GF_BAD_PARAM;
	/*if last entry or first of an empty array...*/
	if (position >= ptr->entryCount) return gf_list_add(ptr, item);
	if (ptr->allocSize==ptr->entryCount) realloc_chain(ptr);

	i = ptr->entryCount - position;
	memmove(&ptr->slots[position + 1], &ptr->slots[position], sizeof(void *)*i);
	ptr->entryCount++;
	ptr->slots[position] = item;
	return GF_OK;
}

GF_EXPORT
void gf_list_reset(GF_List *ptr)
{
	if (ptr) ptr->entryCount = 0;
}

#endif

GF_EXPORT
s32 gf_list_find(GF_List *ptr, void *item)
{
	u32 i, count;
	count = gf_list_count(ptr);
	for (i=0; i<count; i++) {
		if (gf_list_get(ptr, i) == item) return (s32) i;
	}
	return -1;
}

GF_EXPORT
s32 gf_list_del_item(GF_List *ptr, void *item)
{
	s32 i = gf_list_find(ptr, item);
	if (i>=0) gf_list_rem(ptr, (u32) i);
	return i;
}

GF_EXPORT
void *gf_list_enum(GF_List *ptr, u32 *pos)
{
	void *res;
	if (!ptr || !pos) return NULL;
	res = gf_list_get(ptr, *pos);
	(*pos)++;
	return res;
}

//unused
#if 0
GF_EXPORT
void *gf_list_rev_enum(GF_List *ptr, u32 *pos) {
	void *res;
	if (!ptr || !pos) return NULL;
	res = gf_list_get(ptr, gf_list_count (ptr) - *pos - 1 );
	(*pos)++;
	return res;
}
#endif

GF_EXPORT
GF_Err gf_list_swap(GF_List *l1, GF_List *l2)
{
	GF_Err e;
	u32 count = gf_list_count(l1);
	if (!l1 || !l2) return GF_BAD_PARAM;
	if (l1 == l2) return GF_OK;

	while (gf_list_count(l2)) {
		void *ptr = gf_list_get(l2, 0);
		e = gf_list_rem(l2, 0);
		if (e) return e;
		e = gf_list_add(l1, ptr);
		if (e) return e;
	}
	while (count) {
		void *ptr = gf_list_get(l1, 0);
		e = gf_list_rem(l1, 0);
		if (e) return e;
		count--;
		e = gf_list_add(l2, ptr);
		if (e) return e;
	}
	return GF_OK;
}

GF_EXPORT
GF_Err gf_list_transfer(GF_List *dst, GF_List *src)
{
	GF_Err e;
	if (!dst || !src) return GF_BAD_PARAM;
	if (dst == src) return GF_OK;

	while (gf_list_count(src)) {
		void *ptr = gf_list_pop_front(src);
		if (!ptr) return GF_BAD_PARAM;
		e = gf_list_add(dst, ptr);
		if (e) return e;
	}
	return GF_OK;
}

GF_EXPORT
GF_List* gf_list_clone(GF_List *ptr) {
	GF_List* new_list;
	u32 i = 0;
	void* item;
	if (!ptr) return NULL;
	new_list = gf_list_new();
	while ((item = gf_list_enum(ptr, &i)))
		gf_list_add(new_list, item);

	return new_list;
}

//unused
#if 0
void gf_list_reverse(GF_List *ptr) {
	GF_List* saved_order;
	void* item;
	u32 i = 0;
	if (!ptr) return;
	saved_order = gf_list_clone(ptr);
	gf_list_reset(ptr);

	while ((item = gf_list_enum(saved_order, &i))) {
		gf_list_insert(ptr, item, 0);
	}

	gf_list_del(saved_order);
}
#endif

GF_EXPORT
void* gf_list_pop_front(GF_List *ptr) {
	void * item;
	if (!ptr) return NULL;

	item = gf_list_get(ptr, 0);
	gf_list_rem(ptr, 0);

	return item;
}

GF_EXPORT
void* gf_list_pop_back(GF_List *ptr) {
	void * item;
	if (!ptr) return NULL;

	item = gf_list_last(ptr);
	gf_list_rem_last(ptr);

	return item;
}

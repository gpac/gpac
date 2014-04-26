/*****************************************************************************
 *
 *  XVID MPEG-4 VIDEO CODEC
 *  - Aligned Memory Allocator -
 *
 *  Copyright(C) 2002-2003 Edouard Gomez <ed.gomez@free.fr>
 *
 *  This program is free software ; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation ; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY ; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program ; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
 *
 * $Id: mem_align.cpp,v 1.3 2010-02-23 16:23:32 jeanlf Exp $
 *
 ****************************************************************************/

#include "mem_align.h"
#include "global.h"

#ifdef __SYMBIAN32__
#include <e32std.h>
#endif

/*****************************************************************************
 * xvid_malloc
 *
 * This function allocates 'size' bytes (usable by the user) on the heap and
 * takes care of the requested 'alignment'.
 * In order to align the allocated memory block, the xvid_malloc allocates
 * 'size' bytes + 'alignment' bytes. So try to keep alignment very small
 * when allocating small pieces of memory.
 *
 * NB : a block allocated by xvid_malloc _must_ be freed with xvid_free
 *      (the libc free will return an error)
 *
 * Returned value : - NULL on error
 *                  - Pointer to the allocated aligned block
 *
 ****************************************************************************/

#include "Rules.h"

void *xvid_malloc(long size, dword alignment) {

	byte *mem_ptr;


	if (!alignment) {

		/* We have not to satisfy any alignment */
		//mem_ptr = (byte*)gf_malloc(size + 1);
		//mem_ptr = new(ELeave) byte[size+1];
		mem_ptr = new byte[size+1];
		if(mem_ptr) {

			/* Store (mem_ptr - "real allocated memory") in *(mem_ptr-1) */
			*mem_ptr = (byte)1;

			/* Return the mem_ptr pointer */
			return ((void *)(mem_ptr+1));
		}
	} else {
		byte *tmp;

		/* Allocate the required size memory + alignment so we
		 * can realign the data if necessary */
		//tmp = (byte *) gf_malloc(size + alignment);
		//tmp = new(ELeave) byte[size + alignment];
		tmp = new byte[size + alignment];
		if(tmp) {

			/* Align the tmp pointer */
			mem_ptr =
			    (byte *) ((dword)(tmp + alignment - 1) & (~(dword)(alignment - 1)));

			/* Special case where gf_malloc have already satisfied the alignment
			 * We must add alignment to mem_ptr because we must store
			 * (mem_ptr - tmp) in *(mem_ptr-1)
			 * If we do not add alignment to mem_ptr then *(mem_ptr-1) points
			 * to a forbidden memory space */
			if (mem_ptr == tmp)
				mem_ptr += alignment;

			/* (mem_ptr - tmp) is stored in *(mem_ptr-1) so we are able to retrieve
			 * the real gf_malloc block allocated and free it in xvid_free */
			*(mem_ptr - 1) = (byte) (mem_ptr - tmp);

			/* Return the aligned pointer */
			return ((void *)mem_ptr);
		}
	}
	return 0;
}

/*****************************************************************************
 * xvid_free
 *
 * Free a previously 'xvid_malloc' allocated block. Does not free NULL
 * references.
 *
 * Returned value : None.
 *
 ****************************************************************************/

void xvid_free(void *mem_ptr) {

	if(!mem_ptr)
		return;

	/* Aligned pointer */
	byte *ptr = (byte*)mem_ptr;

	/* *(ptr - 1) holds the offset to the real allocated block
	 * we sub that offset os we free the real pointer */
	ptr -= *(ptr - 1);

	/* Free the memory */
	//free(ptr);
	delete[] ptr;
}

//----------------------------

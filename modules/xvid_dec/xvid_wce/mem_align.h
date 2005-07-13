/*****************************************************************************
 *
 *  XVID MPEG-4 VIDEO CODEC
 *  - Aligned Memory Allocator header  -
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
 * $Id: mem_align.h,v 1.1.1.1 2005-07-13 14:36:15 jeanlf Exp $
 *
 ****************************************************************************/

#ifndef _MEM_ALIGN_H_
#define _MEM_ALIGN_H_

#include "portab.h"
#include "Rules.h"

void *xvid_malloc(long size, dword alignment);
void xvid_free(void *mem_ptr);

#endif							/* _MEM_ALIGN_H_ */

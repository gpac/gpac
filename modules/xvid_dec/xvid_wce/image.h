/*****************************************************************************
 *
 *  XVID MPEG-4 VIDEO CODEC
 *  - Image related header  -
 *
 *  Copyright(C) 2001-2003 Peter Ross <pross@xvid.org>
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
 * $Id: image.h,v 1.1.1.1 2005-07-13 14:36:14 jeanlf Exp $
 *
 ****************************************************************************/

#ifndef _IMAGE_H_
#define _IMAGE_H_

#include "portab.h"
#include "global.h"
#include "xvid.h"

int image_create(IMAGE * image, dword edged_width, dword edged_height);
void image_destroy(IMAGE * image, dword edged_width, dword edged_height);


#endif							/* _IMAGE_H_ */

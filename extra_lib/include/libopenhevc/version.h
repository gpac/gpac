/*
 * Version macros.
 *
 * This file is part of FFmpeg.
 *
 * FFmpeg is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * FFmpeg is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with FFmpeg; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#ifndef OPENHEVC_VERSION_H
#define OPENHEVC_VERSION_H

/**
 * @file
 * @ingroup libopenhevc
 * Libopenhevc version macros
 */

#include "libavutil/version.h"

#define LIBOPENHEVC_VERSION_MAJOR   1
#define LIBOPENHEVC_VERSION_MINOR  00
#define LIBOPENHEVC_VERSION_MICRO 000

#define LIBOPENHEVC_VERSION_INT AV_VERSION_INT(LIBOPENHEVC_VERSION_MAJOR, \
                                               LIBOPENHEVC_VERSION_MINOR, \
                                               LIBOPENHEVC_VERSION_MICRO)
#define LIBOPENHEVC_VERSION     AV_VERSION(LIBOPENHEVC_VERSION_MAJOR,   \
                                           LIBOPENHEVC_VERSION_MINOR,   \
                                           LIBOPENHEVC_VERSION_MICRO)
#define LIBOPENHEVC_BUILD       LIBOPENHEVC_VERSION_INT

#define LIBOPENHEVC_IDENT       "Loh" AV_STRINGIFY(LIBOPENHEVC_VERSION)

#endif /* OPENHEVC_VERSION_H */

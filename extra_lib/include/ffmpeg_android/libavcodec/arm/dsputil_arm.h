/*
 * Copyright (c) 2009 Mans Rullgard <mans@mansr.com>
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

#ifndef AVCODEC_ARM_DSPUTIL_H
#define AVCODEC_ARM_DSPUTIL_H

#include "libavcodec/avcodec.h"
#include "libavcodec/dsputil.h"

void ff_dsputil_init_armv5te(DSPContext* c, AVCodecContext *avctx);
void ff_dsputil_init_armv6(DSPContext* c, AVCodecContext *avctx);
void ff_dsputil_init_vfp(DSPContext* c, AVCodecContext *avctx);
void ff_dsputil_init_neon(DSPContext *c, AVCodecContext *avctx);
void ff_dsputil_init_iwmmxt(DSPContext* c, AVCodecContext *avctx);

#endif

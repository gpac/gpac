/*
 * arbitrary precision integers
 * Copyright (c) 2004 Michael Niedermayer <michaelni@gmx.at>
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
 *
 */

/**
 * @file integer.h
 * arbitrary precision integers
 * @author Michael Niedermayer <michaelni@gmx.at>
 */

#ifndef INTEGER_H
#define INTEGER_H

#define AV_INTEGER_SIZE 8

typedef struct AVInteger{
    uint16_t v[AV_INTEGER_SIZE];
} AVInteger;

AVInteger av_add_i(AVInteger a, AVInteger b);
AVInteger av_sub_i(AVInteger a, AVInteger b);
int av_log2_i(AVInteger a);
AVInteger av_mul_i(AVInteger a, AVInteger b);
int av_cmp_i(AVInteger a, AVInteger b);
AVInteger av_shr_i(AVInteger a, int s);
AVInteger av_mod_i(AVInteger *quot, AVInteger a, AVInteger b);
AVInteger av_div_i(AVInteger a, AVInteger b);
AVInteger av_int2i(int64_t a);
int64_t av_i2int(AVInteger a);

#endif // INTEGER_H

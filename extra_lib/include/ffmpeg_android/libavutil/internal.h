/*
 * copyright (c) 2006 Michael Niedermayer <michaelni@gmx.at>
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

/**
 * @file libavutil/internal.h
 * common internal API header
 */

#ifndef AVUTIL_INTERNAL_H
#define AVUTIL_INTERNAL_H

#if !defined(DEBUG) && !defined(NDEBUG)
#    define NDEBUG
#endif

#include <limits.h>
#include <stdint.h>
#include <stddef.h>
#include <assert.h>
#include "config.h"
#include "attributes.h"
#include "timer.h"

#ifndef attribute_align_arg
#if (!defined(__ICC) || __ICC > 1110) && AV_GCC_VERSION_AT_LEAST(4,2)
#    define attribute_align_arg __attribute__((force_align_arg_pointer))
#else
#    define attribute_align_arg
#endif
#endif

#ifndef attribute_used
#if AV_GCC_VERSION_AT_LEAST(3,1)
#    define attribute_used __attribute__((used))
#else
#    define attribute_used
#endif
#endif

#ifndef av_alias
#if HAVE_ATTRIBUTE_MAY_ALIAS && (!defined(__ICC) || __ICC > 1110) && AV_GCC_VERSION_AT_LEAST(3,3)
#   define av_alias __attribute__((may_alias))
#else
#   define av_alias
#endif
#endif

#ifndef INT16_MIN
#define INT16_MIN       (-0x7fff - 1)
#endif

#ifndef INT16_MAX
#define INT16_MAX       0x7fff
#endif

#ifndef INT32_MIN
#define INT32_MIN       (-0x7fffffff - 1)
#endif

#ifndef INT32_MAX
#define INT32_MAX       0x7fffffff
#endif

#ifndef UINT32_MAX
#define UINT32_MAX      0xffffffff
#endif

#ifndef INT64_MIN
#define INT64_MIN       (-0x7fffffffffffffffLL - 1)
#endif

#ifndef INT64_MAX
#define INT64_MAX INT64_C(9223372036854775807)
#endif

#ifndef UINT64_MAX
#define UINT64_MAX UINT64_C(0xFFFFFFFFFFFFFFFF)
#endif

#ifndef INT_BIT
#    define INT_BIT (CHAR_BIT * sizeof(int))
#endif

#ifndef offsetof
#    define offsetof(T, F) ((unsigned int)((char *)&((T *)0)->F))
#endif

/* Use to export labels from asm. */
#define LABEL_MANGLE(a) EXTERN_PREFIX #a

// Use rip-relative addressing if compiling PIC code on x86-64.
#if ARCH_X86_64 && defined(PIC)
#    define LOCAL_MANGLE(a) #a "(%%rip)"
#else
#    define LOCAL_MANGLE(a) #a
#endif

#define MANGLE(a) EXTERN_PREFIX LOCAL_MANGLE(a)

/* debug stuff */

/* dprintf macros */
#ifdef DEBUG
#    define dprintf(pctx, ...) av_log(pctx, AV_LOG_DEBUG, __VA_ARGS__)
#else
#    define dprintf(pctx, ...)
#endif

#define av_abort()      do { av_log(NULL, AV_LOG_ERROR, "Abort at %s:%d\n", __FILE__, __LINE__); abort(); } while (0)

/* math */

#if ARCH_X86
#define MASK_ABS(mask, level)\
            __asm__ volatile(\
                "cltd                   \n\t"\
                "xorl %1, %0            \n\t"\
                "subl %1, %0            \n\t"\
                : "+a" (level), "=&d" (mask)\
            );
#else
#define MASK_ABS(mask, level)\
            mask  = level >> 31;\
            level = (level ^ mask) - mask;
#endif

/* avoid usage of dangerous/inappropriate system functions */
#undef  malloc
#define malloc please_use_av_malloc
#undef  free
#define free please_use_av_free
#undef  realloc
#define realloc please_use_av_realloc
#undef  time
#define time time_is_forbidden_due_to_security_issues
#undef  rand
#define rand rand_is_forbidden_due_to_state_trashing_use_av_lfg_get
#undef  srand
#define srand srand_is_forbidden_due_to_state_trashing_use_av_lfg_init
#undef  random
#define random random_is_forbidden_due_to_state_trashing_use_av_lfg_get
#undef  sprintf
#define sprintf sprintf_is_forbidden_due_to_security_issues_use_snprintf
#undef  strcat
#define strcat strcat_is_forbidden_due_to_security_issues_use_av_strlcat
#undef  exit
#define exit exit_is_forbidden
#ifndef LIBAVFORMAT_BUILD
#undef  printf
#define printf please_use_av_log_instead_of_printf
#undef  fprintf
#define fprintf please_use_av_log_instead_of_fprintf
#undef  puts
#define puts please_use_av_log_instead_of_puts
#undef  perror
#define perror please_use_av_log_instead_of_perror
#endif

#define FF_ALLOC_OR_GOTO(ctx, p, size, label)\
{\
    p = av_malloc(size);\
    if (p == NULL && (size) != 0) {\
        av_log(ctx, AV_LOG_ERROR, "Cannot allocate memory.\n");\
        goto label;\
    }\
}

#define FF_ALLOCZ_OR_GOTO(ctx, p, size, label)\
{\
    p = av_mallocz(size);\
    if (p == NULL && (size) != 0) {\
        av_log(ctx, AV_LOG_ERROR, "Cannot allocate memory.\n");\
        goto label;\
    }\
}

#include "libm.h"

/**
 * Returns NULL if CONFIG_SMALL is true, otherwise the argument
 * without modification. Used to disable the definition of strings
 * (for example AVCodec long_names).
 */
#if CONFIG_SMALL
#   define NULL_IF_CONFIG_SMALL(x) NULL
#else
#   define NULL_IF_CONFIG_SMALL(x) x
#endif

#endif /* AVUTIL_INTERNAL_H */

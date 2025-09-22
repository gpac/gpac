#ifndef _INTTYPES_H_
#define _INTTYPES_H_

#if defined(_WIN32) && !defined(PRId64)
#define PRId64 "I64d"
#define PRIu64 "I64u"
#define PRIx64 "I64x"
#endif

typedef signed char int8_t;
typedef signed short int16_t;
typedef signed int int32_t;
typedef unsigned char uint8_t;
typedef unsigned short uint16_t;
typedef unsigned int uint32_t;
typedef signed __int64 int64_t;
typedef unsigned __int64 uint64_t;

typedef int64_t ssize_t;

#endif	/*_INTTYPES_H_*/

#ifndef __RULES_H
#define __RULES_H

/*----------------------------
   Copyright (c) Lonely Cat Games  All rights reserved.
   General types required for compilation of Insanity group of libraries.
----------------------------*/
//warning settings for MS compiler
#ifdef _MSC_VER

#pragma warning(disable: 4786)//trunc symbols to 255 chars
#pragma warning(disable: 4530)//exception handling
#pragma warning(disable: 4800)//exception handling
#pragma warning(disable: 4096)//exception handling
#pragma warning(disable: 4100)//unreferenced formal parameter
#pragma warning(disable: 4505)//unreferenced local function has been removed
#pragma warning(disable: 4514)//unreferenced inline function has been removed
#pragma warning(disable: 4663)//C++ error in std headers
#pragma warning(disable: 4710)//function not inlined
#pragma warning(disable: 4201)//nonstandard extension with unnamed structs
#pragma warning(disable: 4244)//conversion from ??? to ???, possible loss of data
#pragma warning(disable: 4146)//unary minus operator applied to unsigned type, result still unsigned
#pragma warning(disable: 4284)//C++...
#pragma warning(disable: 4290)//C++ Exception Specification ignored
#pragma warning(disable: 4127)//conditional expression is constant
#pragma warning(disable: 4663)//C++ language change...
#pragma warning(disable: 4702)//unreachable code
#pragma warning(disable: 4511)//copy constructor could not be generated
#pragma warning(disable: 4512)//assignment operator could not be generated
#pragma warning(disable: 4711)//function selected for automatic inline expansion



//level4 warnings:
#pragma warning(3: 4189)      //local variable is initialized but not referenced
#pragma warning(3: 4305)      //truncation from const double to float
//#pragma warning(3: 4244)      //conversion from ??? to ???, possible loss of data
#pragma warning(3: 4245)      //signed/unsigned mismatch
#pragma warning(3: 4018)      //signed/unsigned mismatch
#pragma warning(3: 4706)      //assignment within conditional expression
#pragma warning(3: 4701)      //local variable may be used without having been initialized
#pragma warning(3: 4211)      //nonstandard extension used : redefined extern to static
#pragma warning(3: 4310)      //cast truncates constant value

#endif

//----------------------------

typedef unsigned char byte;
typedef signed char schar;
typedef unsigned short word;
typedef unsigned long dword;
typedef unsigned long ulong;

#ifndef _SIZE_T_DEFINED_
#define _SIZE_T_DEFINED_
typedef unsigned size_t;
#endif

#ifndef NULL
#define NULL 0
#endif

//----------------------------

#endif

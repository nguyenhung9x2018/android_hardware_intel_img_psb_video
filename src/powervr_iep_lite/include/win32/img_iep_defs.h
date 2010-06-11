/*!
******************************************************************************
 @file   : img_iep_defs.h

 @brief

 @Author Various

         <b>Copyright 2008 by Imagination Technologies Limited.</b>\n
         All rights reserved.  No part of this software, either
         material or conceptual may be copied or distributed,
         transmitted, transcribed, stored in a retrieval system
         or translated into any human or computer language in any
         form by any means, electronic, mechanical, manual or
         other-wise, or disclosed to third parties without the
         express written permission of Imagination Technologies
         Limited, Unit 8, HomePark Industrial Estate,
         King's Langley, Hertfordshire, WD4 8LZ, U.K.

 <b>Description:</b>\n
         This header file contains the Imagination Technologies platform
		 specific defintions.

 <b>Platform:</b>\n
	     Win32.

******************************************************************************/

/********************************************************************
	DISCLAIMER:
	This code is provided for demonstration purposes only. It has
	been ported from other projects and has not been tested with
	real hardware. It is not provided in a state that can be run
	with real hardware - this is not intended as the basis for a
	production driver. This code should only be used as an example 
	of the algorithms to be used for setting up the IEP lite 
	hardware.
 ********************************************************************/

#if !defined (__IMG_IEP_DEFS_H__)
#define __IMG_IEP_DEFS_H__

#include <assert.h>

#include <stdio.h>
#include <stdlib.h>
#include "img_defs.h"

#if (__cplusplus)
extern "C" {
#endif

typedef IMG_INT32		img_int32,	* img_pint32;
typedef int				img_result,	* img_presult;
typedef unsigned int	img_uint32, * img_puint32;
typedef void			img_void,	* img_pvoid;
typedef void*		    img_handle,	* img_phandle;
typedef void**			img_hvoid,	* img_phvoid;
typedef int				img_bool,	* img_pbool;
typedef unsigned char	img_uint8,	* img_puint8;
typedef float			img_float,	* img_pfloat;
typedef char			img_int8,	* img_pint8;
typedef char			img_char,	* img_pchar;
typedef unsigned char	img_byte,	* img_pbyte;
typedef unsigned short	img_uint16, * img_puint16;
typedef short			img_int16,  * img_pint16;
#if 0
typedef unsigned __int64 img_uint64, * img_puint64;
typedef __int64          img_int64,  * img_pint64;
#else
typedef unsigned long long img_uint64, * img_puint64;
typedef long long          img_int64,  * img_pint64;
#endif
typedef unsigned long    img_uintptr_t;


//#define	IMG_SUCCESS					(0)
#define IMG_TIMEOUT					(1)
#define IMG_MALLOC_FAILED			(2)
#define IMG_FATAL_ERROR				(3)

#if 0
typedef		enum	img_tag_TriStateSwitch
{
	IMG_ON		=	0x00,
	IMG_OFF,
	IMG_IGNORE

} img_TriStateSwitch, * img_pTriStateSwitch;
#endif

#define	IMG_NULL 0
#define IMG_TRUE 1
#define IMG_FALSE 0

/* Maths functions - platform dependant						*/
#define		IMG_COS					cos
#define		IMG_SIN					sin
#define		IMG_POW(A, B)			pow		(A,B)
#define		IMG_SQRT(A)				sqrt	(A)
#define		IMG_FABS(A)				fabs	(A)
#define		IMG_EXP(A)				exp		(A)

/* General functions										*/
#define	IMG_MEMCPY(A,B,C)	memcpy	(A,B,C)
#define	IMG_MEMSET(A,B,C)	memset	(A,B,C)
#define IMG_MEMCMP(A,B,C)	memcmp	(A,B,C)
#define IMG_MEMMOVE(A,B,C)	memmove	(A,B,C)

#ifndef EXIT_ON_ASSERT
/*FIXME: not use _flushall()
#define IMG_ASSERT(exp) (void)( (exp) || (fprintf(stderr, "ERROR: Assert %s; File %s; Line %d\n", #exp, __FILE__, __LINE__), _flushall(), assert(exp), 0) )
*/
#define IMG_ASSERT(exp) (void)( (exp) || (fprintf(stderr, "ERROR: Assert %s; File %s; Line %d\n", #exp, __FILE__, __LINE__), assert(exp), 0) )
//#define IMG_ASSERT(A)		assert	(A)
#else
//FIXME: not use  _flushall()
//#define IMG_ASSERT(exp) (void)( (exp) || (fprintf(stderr, "ERROR: Assert %s; File %s; Line %d\n", #exp, __FILE__, __LINE__), _flushall(), exit(-1), 0) )
#define IMG_ASSERT(exp) (void)( (exp) || (fprintf(stderr, "ERROR: Assert %s; File %s; Line %d\n", #exp, __FILE__, __LINE__),  exit(-1), 0) )
#endif

#define IMG_ASSERT_LIVE  // Defined when IMG_ASSERT() does something.

/* Takes any two signed values - return 'IMG_TRUE' if they have the same polarity */
#define	IMG_SAMESIGN(A,B)	(((((img_int32)A)^((img_int32)B))&0x80000000)==0)

#if defined (NO_INLINE_FUNCS)
	#define	INLINE
	#define	FORCE_INLINE
#else
#if defined(_UITRON_)
	#define	INLINE
	#define	FORCE_INLINE
	#define INLINE_IS_PRAGMA
#else
#if defined (__cplusplus)
	#define INLINE					inline
	#define	FORCE_INLINE			inline
#else
	#define	INLINE					__inline
#if defined(UNDER_CE) || defined(UNDER_XP) || defined(UNDER_VISTA)
	#define	FORCE_INLINE			__forceinline
#else
	#define	FORCE_INLINE			static __inline
#endif
#endif
#endif
#endif

/* Use this in any file, or use attributes under GCC - see below */
#ifndef PVR_UNREFERENCED_PARAMETER
#define	PVR_UNREFERENCED_PARAMETER(param) (param) = (param)
#endif

/* The best way to supress unused parameter warnings using GCC is to use a
 * variable attribute.  Place the unref__ between the type and name of an
 * unused parameter in a function parameter list, eg `int unref__ var'. This
 * should only be used in GCC build environments, for example, in files that
 * compile only on Linux. Other files should use UNREFERENCED_PARAMETER */
#ifdef __GNUC__
#define unref__ __attribute__ ((unused))
#else
#define unref__
#endif

#if 0
/*
	Wide character definitions
*/
#if defined(UNICODE)
typedef unsigned short		TCHAR, *PTCHAR, *PTSTR;
#else	/* #if defined(UNICODE) */
typedef char				TCHAR, *PTCHAR, *PTSTR;
#endif	/* #if defined(UNICODE) */
#endif

//#define IMG_CALLCONV __stdcall
//#define IMG_INTERNAL
//#define	IMG_EXPORT	__declspec(dllexport)

/* IMG_IMPORT is defined as IMG_EXPORT so that headers and implementations match.
	* Some compilers require the header to be declared IMPORT, while the implementation is declared EXPORT
	*/
//#define	IMG_IMPORT	IMG_EXPORT

#define IMG_MALLOC(A)		malloc	(A)
#define IMG_FREE(A)			free	(A)

#define IMG_ALIGN(byte)
#define IMG_NORETURN

#if defined (__cplusplus)
	#define INLINE					inline
#else
#if !defined(INLINE)
	#define	INLINE					__inline
#endif
#endif

#define __TBILogF	consolePrintf

#define IMG_NORETURN

#if (__cplusplus)
}
#endif

#if !defined (__IMG_IEP_TYPES_H__)
#include "img_types.h"
#endif

#endif // IMG_IEP_DEFS_H



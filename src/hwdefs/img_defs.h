/*
 * INTEL CONFIDENTIAL
 * Copyright 2007 Intel Corporation. All Rights Reserved.
 * Copyright 2005-2007 Imagination Technologies Limited. All Rights Reserved.
 *
 * The source code contained or described herein and all documents related to
 * the source code ("Material") are owned by Intel Corporation or its suppliers
 * or licensors. Title to the Material remains with Intel Corporation or its
 * suppliers and licensors. The Material may contain trade secrets and
 * proprietary and confidential information of Intel Corporation and its
 * suppliers and licensors, and is protected by worldwide copyright and trade
 * secret laws and treaty provisions. No part of the Material may be used,
 * copied, reproduced, modified, published, uploaded, posted, transmitted,
 * distributed, or disclosed in any way without Intel's prior express written
 * permission.
 *
 * No license under any patent, copyright, trade secret or other intellectual
 * property right is granted to or conferred upon you by disclosure or delivery
 * of the Materials, either expressly, by implication, inducement, estoppel or
 * otherwise. Any license under such intellectual property rights must be
 * express and approved by Intel in writing.
 */

/*!****************************************************************************
@File                   img_defs.h

@Title                  Common header containing type definitions for portability

@Author                 Imagination Technologies

@date                   August 2001

@Platform               cross platform / environment

@Description    Contains variable and structure definitions. Any platform
                                specific types should be defined in this file.

@DoxygenVer

******************************************************************************/

/******************************************************************************
Modifications :-

$Log: img_defs.h $
*****************************************************************************/
#if !defined (__IMG_DEFS_H__)
#define __IMG_DEFS_H__

#include "img_types.h"

/*!
 *****************************************************************************
 *      These types should be changed on a per-platform basis to retain
 *      the indicated number of bits per type. e.g.: A DP_INT_16 should be
 *      always reflect a signed 16 bit value.
 *****************************************************************************/

typedef         enum    img_tag_TriStateSwitch {
    IMG_ON              =       0x00,
    IMG_OFF,
    IMG_IGNORE

} img_TriStateSwitch, * img_pTriStateSwitch;

#define         IMG_SUCCESS                             0
#define     IMG_FAILED             -1

#define         IMG_NULL                                0
#define         IMG_NO_REG                              1

#if defined (NO_INLINE_FUNCS)
#define INLINE
#define FORCE_INLINE
#else
#if defined(_UITRON_)
#define INLINE
#define FORCE_INLINE                    static
#define INLINE_IS_PRAGMA
#else
#if defined (__cplusplus)
#define INLINE                                  inline
#define FORCE_INLINE                    inline
#else
#define INLINE                                  __inline
#if defined(UNDER_CE) || defined(UNDER_XP) || defined(UNDER_VISTA)
#define FORCE_INLINE                    __forceinline
#else
#define FORCE_INLINE                    static __inline
#endif
#endif
#endif
#endif

/* Use this in any file, or use attributes under GCC - see below */
#ifndef PVR_UNREFERENCED_PARAMETER
#define PVR_UNREFERENCED_PARAMETER(param) (param) = (param)
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

#if defined(UNDER_CE)

/* This may need to be _stdcall */
#define IMG_CALLCONV
#define IMG_INTERNAL
#define IMG_RESTRICT

#define IMG_EXPORT
#define IMG_IMPORT      IMG_EXPORT

#ifdef DEBUG
#define IMG_ABORT()     TerminateProcess(GetCurrentProcess(), 0)
#else
#define IMG_ABORT()     for(;;);
#endif
#else
#if defined(_WIN32)

#define IMG_CALLCONV __stdcall
#define IMG_INTERNAL
#define IMG_EXPORT      __declspec(dllexport)
#define IMG_RESTRICT __restrict


/* IMG_IMPORT is defined as IMG_EXPORT so that headers and implementations match.
 * Some compilers require the header to be declared IMPORT, while the implementation is declared EXPORT
 */
#define IMG_IMPORT      IMG_EXPORT
#if defined( UNDER_VISTA ) && !defined(USE_CODE)
#ifndef _INC_STDLIB
void __cdecl abort(void);
#endif
__forceinline void __declspec(noreturn) img_abort(void)
{
    for (;;) abort();
}
#define IMG_ABORT()     img_abort()
#endif
#else
#if defined (__SYMBIAN32__)

#if defined (__GNUC__)
#define IMG_IMPORT
#define IMG_EXPORT      __declspec(dllexport)
#define IMG_RESTRICT __restrict__
#define NONSHARABLE_CLASS(x) class x
#else
#if defined (__CC_ARM)
#define IMG_IMPORT __declspec(dllimport)
#define IMG_EXPORT __declspec(dllexport)
#pragma warning("Please make sure that you've enabled the --restrict mode t")
/* The ARMCC compiler currently requires that you've enabled the --restrict mode
   to permit the use of this keyword */
# define IMG_RESTRICT /*restrict*/
#endif
#endif

#define IMG_CALLCONV
#define IMG_INTERNAL
#define IMG_EXTERNAL

#else
#if defined(__linux__)

#define IMG_CALLCONV
#define IMG_INTERNAL    __attribute__ ((visibility ("hidden")))
#define IMG_EXPORT
#define IMG_IMPORT
#define IMG_RESTRICT    __restrict__

#else
#if defined(_UITRON_)
#define IMG_CALLCONV
#define IMG_INTERNAL
#define IMG_EXPORT
#define IMG_RESTRICT

#define __cdecl

/* IMG_IMPORT is defined as IMG_EXPORT so that headers and implementations match.
* Some compilers require the header to be declared IMPORT, while the implementation is declared EXPORT
*/
#define IMG_IMPORT      IMG_EXPORT
#ifndef USE_CODE
void SysAbort(char const *pMessage);
#define IMG_ABORT()     SysAbort("ImgAbort")
#endif
#else
#error("define an OS")
#endif
#endif
#endif
#endif
#endif

// Use default definition if not overridden
#ifndef IMG_ABORT
#define IMG_ABORT()     abort()
#endif


#endif /* #if !defined (__IMG_DEFS_H__) */
/*****************************************************************************
 End of file (IMG_DEFS.H)
*****************************************************************************/


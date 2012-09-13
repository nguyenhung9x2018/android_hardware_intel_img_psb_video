/*!
******************************************************************************
 @file   : coreflags.h

 @brief  Core specific flags to turn features on/off on different builds

 @Author PowerVR

 @date   1/07/2010

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

 \n<b>Description:</b>\n
         Core specific flags to turn features on/off on different builds
     
 \n<b>Platform:</b>\n
	     N/A



******************************************************************************/

#ifndef _COREFLAGS_H_
#define _COREFLAGS_H_


#ifdef __cplusplus
extern "C" {
#endif

#if defined(ENCFMT_H264MVC) || defined(FAKE_MTX)
#	define ENCFMT_H264
#	define _ENABLE_MVC_        /* Defined for enabling MVC specific code */
#	define _MVC_RC_  1         /* Defined for enabling MVC rate control code, later on it will be replaced by _ENABLE_MVC_ */
#endif

#if ( defined(FAKE_MTX) || defined(ENCFMT_H264MVC) )
#define TOPAZHP_MAX_NUM_STREAMS 2
#else
#define TOPAZHP_MAX_NUM_STREAMS 2
#endif

/*
	Hierarhical B picture support is disabled:
		* for H264 VCM mode, as VCM mode is not expected to use B pictures at all
*/
#if  defined(FAKE_MTX) || (\
		defined(ENCFMT_H264) && !(defined(RCMODE_VCM)) \
	)
#	define INCLUDE_HIER_SUPPORT	1
#else
#	define INCLUDE_HIER_SUPPORT	0
#endif

// Add this back in for SEI inclusion
#if !defined(RCMODE_NONE)
#	if  (defined(FAKE_MTX) || ((defined(RCMODE_VBR) || defined(RCMODE_CBR) || defined(RCMODE_VCM) || defined(RCMODE_LLRC)) && defined(ENCFMT_H264)))
#		define SEI_INSERTION 1  
#	endif
#endif

#if	(defined(FAKE_MTX) || defined(ENCFMT_H264))
#	define WEIGHTED_PREDICTION 1
#	define MULTI_REF_P
#	define LONG_TERM_REFS
#	define CUSTOM_QUANTIZATION
#endif

#define FIRMWARE_BIAS


#define MAX_BFRAMES	7

#define MAX_REF_B_LEVELS_CTXT	3

#if INCLUDE_HIER_SUPPORT
#if ( defined(FAKE_MTX) || !defined(ENCFMT_H264MVC) )
#	define MAX_REF_B_LEVELS	(MAX_REF_B_LEVELS_CTXT)
#else
#	define MAX_REF_B_LEVELS	2
#endif
#else
#	define MAX_REF_B_LEVELS		0
#endif

#ifdef MULTI_REF_P
#	define MAX_REF_I_OR_P_LEVELS	(MAX_REF_I_OR_P_LEVELS_CTXT)
#else
#	define MAX_REF_I_OR_P_LEVELS	2
#endif

#if !defined(FAKE_MTX)
#	if defined(ENABLE_FORCED_INLINE)
/*
	__attribute__((always_inline)) should be only used when all C code is compiled by
	GCC to a blob object with `-combine` swithch.
*/
#		define MTX_INLINE  inline __attribute__((always_inline))
#else
#		define MTX_INLINE  inline
#endif
#else
#	define MTX_INLINE
#endif

/* 
	Check that only one `RCMODE_` symbol is defined.

	If `RCMODE_NONE` is set, block any possebility for RC sources to be included.
*/
#if defined(RCMODE_NONE)
#	define _RCMODE_PRESENT_
#	define __COREFLAGS_RC_ALLOWED__	0
#else
#	define __COREFLAGS_RC_ALLOWED__ 1
#endif
#if defined(RCMODE_CBR)
#	if defined(_RCMODE_PRESENT_)
#		error "Two `RCMODE_` symbols set."
#	else
#		define _RCMODE_PRESENT_
#	endif
#endif
#if defined(RCMODE_VCM)
#	if defined(_RCMODE_PRESENT_)
#		error "Two `RCMODE_` symbols set."
#	else
#		define _RCMODE_PRESENT_
#	endif
#endif
#if defined(RCMODE_VBR)
#	if defined(_RCMODE_PRESENT_)
#		error "Two `RCMODE_` symbols set."
#	else
#		define _RCMODE_PRESENT_
#	endif
#endif
#if defined(RCMODE_ERC)
#	if defined(_RCMODE_PRESENT_)
#		error "Two `RCMODE_` symbols set."
#	endif
#endif

#undef _RCMODE_PRESENT_

/* 
	Determine possible rate control modes.
*/
#if defined(FAKE_MTX)
#	define	CBR_RC_MODE_POSSIBLE	__COREFLAGS_RC_ALLOWED__
#	define  VCM_RC_MODE_POSSIBLE	__COREFLAGS_RC_ALLOWED__
#	define  VBR_RC_MODE_POSSIBLE	__COREFLAGS_RC_ALLOWED__
#	define  ERC_RC_MODE_POSSIBLE	__COREFLAGS_RC_ALLOWED__
#else
#	if defined(RCMODE_CBR)
#		define	CBR_RC_MODE_POSSIBLE	__COREFLAGS_RC_ALLOWED__
#	else
#		define	CBR_RC_MODE_POSSIBLE	FALSE
#	endif
#	if defined(ENCFMT_H264) && (defined(RCMODE_VCM) || defined(RCMODE_LLRC))
		/* VCM is possible only for H264 */
#		define  VCM_RC_MODE_POSSIBLE	__COREFLAGS_RC_ALLOWED__
#	else
#		define	VCM_RC_MODE_POSSIBLE	FALSE
#	endif
#	if defined(RCMODE_VBR)
#		define  VBR_RC_MODE_POSSIBLE	__COREFLAGS_RC_ALLOWED__
#	else
#		define	VBR_RC_MODE_POSSIBLE	FALSE
#	endif
#	if defined(RCMODE_ERC)
#		define  ERC_RC_MODE_POSSIBLE	__COREFLAGS_RC_ALLOWED__
#	else
#		define	ERC_RC_MODE_POSSIBLE	FALSE
#	endif
#endif


#define RATE_CONTROL_AVAILABLE (\
		CBR_RC_MODE_POSSIBLE || VCM_RC_MODE_POSSIBLE || \
		VBR_RC_MODE_POSSIBLE || ERC_RC_MODE_POSSIBLE\
	)
#define RC_MODE_POSSIBLE(MODE) (MODE ## _RC_MODE_POSSIBLE)

#define RC_MODES_POSSIBLE2(M1, M2) \
	(RC_MODE_POSSIBLE(M1) || RC_MODE_POSSIBLE(M2))

#define RC_MODES_POSSIBLE3(M1, M2, M3) \
	(RC_MODES_POSSIBLE2(M1, M2) || RC_MODE_POSSIBLE(M3))

#define USE_VCM_HW_SUPPORT 1

#endif  // #ifndef _COREFLAGS_H_

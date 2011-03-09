/*!
******************************************************************************
 @file   : csc2_data.h

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

#if !defined (__CSC_DATA_H__)
#define __CSC_DATA_H__
#if (__cplusplus)
extern "C"
{
#endif

    /* Macros and definitions */
#define CSC_FP(floating_num)							((img_int32)FIXEDPT_CONVERT_FLOAT_TO_FIXED(floating_num,CSC_FRACTIONAL_BITS))
#define CSC_FP_HSC(floating_num)						((img_int32)FIXEDPT_CONVERT_FLOAT_TO_FIXED(floating_num,CSC_HSC_FRACTIONAL_BITS))

#define	CSC_MAX_HUE										CSC_FP_HSC(30)
#define	CSC_MIN_HUE										CSC_FP_HSC(-30)
#define	CSC_MAX_SATURATION								CSC_FP_HSC(2)
#define CSC_MIN_SATURATION								CSC_FP_HSC(0)
#define CSC_MAX_CONTRAST								CSC_FP_HSC(2)
#define CSC_MIN_CONTRAST								CSC_FP_HSC(0)
#define CSC_MAX_BRIGHTNESS								((img_int32)FIXEDPT_CONVERT_FLOAT_TO_FIXED(50,CSC_OUTPUT_OFFSET_FRACTIONAL_BITS))
#define CSC_MIN_BRIGHTNESS								((img_int32)FIXEDPT_CONVERT_FLOAT_TO_FIXED(-50,CSC_OUTPUT_OFFSET_FRACTIONAL_BITS))

    /* Typedefs */
    typedef struct {
        CSC_eColourSpace	eColourspace;

        img_bool			bIsYUV;			/* If TRUE, is YUV - if FALSE, is RGB */

    } CSC_sAdditionalColourSpaceInformation;

    typedef struct {
        CSC_eColourSpace	eInputColourspace;
        CSC_eColourSpace	eOutputColourspace;

        img_bool			bIsIdentityMatrix;
        img_bool			bIsSupported;

        CSC_s3x3Matrix		sMatrix;

    } CSC_sColourSpaceConversionMatrix;

    typedef struct {
        CSC_eColourPrimary	eInputColourPrimary;
        CSC_eColourPrimary	eOutputColourPrimary;

        img_bool			bIsIdentityMatrix;
        img_bool			bIsSupported;

        CSC_s3x3Matrix		sMatrix;

    } CSC_sColourPrimaryConversionMatrix;

    typedef struct {
        CSC_eRange			eRange;

        img_int32			i32RGBScale;
        img_int32			i32YScale;
        img_int32			i32UVScale;

        img_uint32			ui32RGBOffset;
        img_uint32			ui32YOffset;
        img_uint32			ui32UVOffset;

    } CSC_sAdditionalRangeInformation;

    /* Data */
    extern CSC_s3x3Matrix							sCSC_IdentityMatrix;
    extern CSC_sAdditionalColourSpaceInformation	asColourspaceInfo						[CSC_NO_OF_COLOURSPACES];
    extern CSC_sColourSpaceConversionMatrix			asCSC_ColourSpaceConversionMatrices		[CSC_NO_OF_COLOURSPACES][CSC_NO_OF_COLOURSPACES];
    extern CSC_sAdditionalRangeInformation			asCSC_RangeInfo							[CSC_NO_OF_RANGES];
    extern CSC_sColourPrimaryConversionMatrix		asCSCColourPrimaryConversionMatrices	[CSC_NO_OF_COLOUR_PRIMARIES][CSC_NO_OF_COLOUR_PRIMARIES];

    /* Functions */
    img_result	csc_StaticDataSafetyCheck(img_void);

#if (__cplusplus)
}
#endif
#endif	/* __CSC_DATA_H__ */
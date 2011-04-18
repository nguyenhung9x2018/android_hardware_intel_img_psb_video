/*!
******************************************************************************
 @file   : csc2.h

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


#if !defined (__CSC2_H__)
#define __CSC2_H__
#if (__cplusplus)
extern "C" {
#endif

#define CSC_FRACTIONAL_BITS                                                             (32-5)  /* Final outputs must fit into 4.10, plus we need one bit for sign */
#define CSC_HSC_FRACTIONAL_BITS                                                 25              /* Bits of fractional data in user specified values for Hue, Saturation and Contrast */
#define CSC_OUTPUT_OFFSET_FRACTIONAL_BITS                               10              /* Bits of fractional data in user specified value for Brightness */

    /* This enumeration lists all colour spaces supported by the CSC API */
    typedef enum {
        CSC_COLOURSPACE_YCC_BT601 = 0x00,
        CSC_COLOURSPACE_YCC_BT709,
        CSC_COLOURSPACE_YCC_SMPTE_240,
        CSC_COLOURSPACE_RGB,

        /* Placeholder - do not remove */
        /* Note: This is not a valid colourspace */
        CSC_NO_OF_COLOURSPACES

    }
    CSC_eColourSpace;

    /* This enumeration lists all supported input and output ranges */
    typedef enum {
        CSC_RANGE_0_255 = 0x00,
        CSC_RANGE_16_235,
        CSC_RANGE_48_208,

        /* Placeholder - do not remove */
        /* Note: This is not a valid range */
        CSC_NO_OF_RANGES

    } CSC_eRange;

    /* This enumeration lists all supported colour primaries */
    typedef enum {
        CSC_COLOUR_PRIMARY_BT709 = 0x00,
        CSC_COLOUR_PRIMARY_BT601,
        CSC_COLOUR_PRIMARY_BT470_2_SYSBG,
        CSC_COLOUR_PRIMARY_BT470_2_SYSM,

        /* Placeholder - do not remove */
        /* Note: This is not a valid colour primary */
        CSC_NO_OF_COLOUR_PRIMARIES

    } CSC_eColourPrimary;

    /* This structure contains settings for Hue, Saturation, Brightness and Contrast (HSBC) */
    typedef struct {
        img_int32       i32Hue;                         /* Between -30 and +30 degrees (specified in x.25 signed fixed point format)    */
        img_int32       i32Saturation;          /* Between 0 and 2 (specified in x.25 signed fixed point format)                                */
        img_int32       i32Brightness;          /* Between -50 and 50 (specified in x.10 signed fixed point format)                             */
        img_int32       i32Contrast;            /* Between 0 and 2 (specified in x.25 signed fixed point format)                                */

    } CSC_sHSBCSettings, * CSC_psHSBCSettings;

    typedef struct {
        img_int32       ai32Coefficients        [3][3]; /* Indices are [row][column] */

    } CSC_s3x3Matrix, * CSC_ps3x3Matrix;

    typedef struct {
        CSC_s3x3Matrix  sCoefficients;

        img_int32               ai32InputOffsets        [3];
        img_int32               ai32OutputOffsets       [3];

    } CSC_sConfiguration, * CSC_psConfiguration;

    img_result  CSC_GenerateMatrix(CSC_eColourSpace             eInputColourSpace,
                                   CSC_eColourSpace             eOutputColourSpace,
                                   CSC_eRange                           eInputRange,
                                   CSC_eRange                           eOutputRange,
                                   CSC_eColourPrimary           eInputPrimary,
                                   CSC_eColourPrimary           eOutputPrimary,
                                   CSC_psHSBCSettings           psHSBCSettings,
                                   CSC_psConfiguration          psResultantConfigurationData);

#if (__cplusplus)
}
#endif
#endif  /* __CSC2_H__ */

/******************************************************************************
 @file   : csc2.c

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
         This module contains functions for calculating a 3x3 colour
                 space conversion matrix.

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

#include <memory.h>

#include "img_iep_defs.h"
#include "csc2.h"
#include "csc2_data.h"
#include "fixedpointmaths.h"

/*************************************************************************************************************

        The combined equations for a general purpose transfer (CSC combined with Procamp) are shown below:
        (matrices in CAPITALS, scaler multiplies in lower case).

        As the colour primary conversions needs to be performed on RGB data, and the procamp (H/S/C) adjustment
        needs to be performed on YUV data,  there are 4 cases to consider. All cases have the same input and
        output offsets shown in YUV to RGB.

        If no HSBC adjustment is required, leave out the conSatHue matrix, though see the RGB-RGB case
        for the exception. If no primary adjustment is required, leave out the inoutPrimaries matrix,
        though see YUV-YUV case for the exception.

        Where a matrix name begins with 'INOUT_', the matrix has been pre-generated to convert from one
        thing to another (e.g.: from an input colour space to an output colour space, or from an input primary
        to an output primary) - the 'INOUT_' matrix is just chosen from a table of pregenerated matrices
        based on the user's input and output selections.

        Input and output offsets
        In all cases, the input offsets are negative and account for differences in range. As the internal
        range for these calculations is always 0-255 (the pre-calculated matrices are based on this internal
        range), the input offsets are always either 0 (for input range 0->255), -16 (for input range 16->235),
        or -48 (for input range 48->208). The same principle applies to the output ranges, but the output
        offsets based on the output range are positive rather than negative (0, +16 or +48). In addition, the
        user can specify a brightness value in the range -50 -> +50, which is added to the output offsets
        (only the Y offset in the case of YUV output).

        -------------------------------------------------------------------------------------------------------
        CASE 1) Input YUV and output YUV:

        combinedMatrix = (outputrangeScale*OUT_TRANSFER_RGB_TO_YUV)*INOUT_PRIMARIES*
                                                (inputrangeScale*IN_TRANSFER_YUV_TO_RGB)*CON_SAT_HUE

        An arbitrary intermediate RGB format is used to perform the primary conversion
        (colour space=RGB, range=0-255). Note: The pregenerated primary conversion matrices are created
        based on this arbitrary intermediate format.

        So IN_TRANSFER_YUV_TO_RGB is the same as  INOUT_TRANSFER[(user defined input colour space) to RGB]
        And OUT_TRANSFER_RGB_TO_YUV is the same as  INOUT_TRANSFER[RGB to (user defined output colour space)]

        OPTIMISATION FOR CASE 1)
                If no primaries adjustment is required (i.e.:input primary = output primary), then the equation
                simplifies to:

                combinedMatrix = CON_SAT_HUE*(outputrangeScale*inputrangeScale*INOUT_TRANSFER_YUV_TO_YUV)
        -------------------------------------------------------------------------------------------------------

        -------------------------------------------------------------------------------------------------------
        CASE 2) Input YUV and output RGB:

        combinedMatrix = INOUT_PRIMARIES*(outputrangeScale*inputrangeScale*INOUT_TRANSFER_YUV_TO_RGB)
                                                *CON_SAT_HUE
        -------------------------------------------------------------------------------------------------------

        -------------------------------------------------------------------------------------------------------
        CASE 3) Input RGB and output YUV

        combinedMatrix = CON_SAT_HUE*(outputrangeScale*inputrangeScale*INOUT_TRANSFER_RGB_TO_YUV)
                                                *INOUT_PRIMARIES
        -------------------------------------------------------------------------------------------------------

        -------------------------------------------------------------------------------------------------------
        CASE 4) Input RGB and output RGB:

        combinedMatrix = OUT_PRIMARY_BT709_TO_USER*(outputrangeScale*OUT_TRANSFER_YUV_TO_RGB)*
                                                CON_SAT_HUE*(inputrangeScale*IN_TRANSFER_RGB_TO_YUV)
                                                        *IN_PRIMARY_USER_TO_BT709

        An arbitrary intermediate YUV format is used to perform the H/S/C operation
        (colour space=BT709, range=0-255, primary=BT709).

        So      OUT_TRANSFER_YUV_TO_RGB is the same as INOUT_TRANSFER[BT709 to RGB]
        And IN_TRANSFER_RGB_TO_YUV is the same as INOUT_TRANSFER[RGB to BT709]

        In addition, the primary conversion is split into two separate matrices. This is because it was
        found that when doing an HSC adjustment, combining the primaries conversion in to one matrix at
        either end of the equation gives slightly different results, the correct way is to convert to
        the  primaries used for the intermediate YUV format (BT709).

        OPTIMISATION FOR CASE 4)
                If no HSBC adjustment is required the conversion to the intermediate YUV format is not
                required, the two primaries matrices can be combined into one, and the equation simplifies to:

                combinedMatrix = outputrangeScale*inputrangeScale*INOUT_PRIMARIES
        -------------------------------------------------------------------------------------------------------

*************************************************************************************************************/

static  img_bool                CSC_bAPIInitialised = IMG_FALSE;

#define CSC_PRINT_DEBUG_INFO            0

#define CSC_MULTIPLY_MATRICES(pIn_a,pIn_b,pResult)              FIXEDPT_MatrixMultiply(3,3,3,3,3,3,(img_int32*)&((pIn_a)->ai32Coefficients),(img_int32*)&((pIn_b)->ai32Coefficients),(img_int32*)&((pResult)->ai32Coefficients),CSC_FRACTIONAL_BITS)

img_result      CSC_GenerateHSCMatrix(CSC_psHSBCSettings        psHSBCSettings,
                                      CSC_ps3x3Matrix               psHSCMatrix);

#if CSC_PRINT_DEBUG_INFO
img_void CSC_PRINT_3x3_MATRIX(CSC_ps3x3Matrix           psMatrix)
{
    img_uint32 ui32Row;
    img_uint32 ui32Column;

    IMG_ASSERT(psMatrix != IMG_NULL);

    for (ui32Row = 0; ui32Row < 3; ui32Row++) {
        printf("(");
        for (ui32Column = 0; ui32Column < 3; ui32Column++) {
            printf("%f", (((img_float) psMatrix->ai32Coefficients [ui32Row][ui32Column]) / (1 << CSC_FRACTIONAL_BITS)));

            if (ui32Column != 2) {
                printf(",   ");
            } else {
                printf(")\n");
            }
        }
    }
}
#endif

img_result      CSC_GenerateMatrix(CSC_eColourSpace             eInputColourSpace,
                                   CSC_eColourSpace         eOutputColourSpace,
                                   CSC_eRange                               eInputRange,
                                   CSC_eRange                               eOutputRange,
                                   CSC_eColourPrimary               eInputPrimary,
                                   CSC_eColourPrimary               eOutputPrimary,
                                   CSC_psHSBCSettings               psHSBCSettings,
                                   CSC_psConfiguration              psResultantConfigurationData)
{
    CSC_s3x3Matrix      sCombinedRangeScaleMatrix;
    CSC_s3x3Matrix      sInputRangeScaleMatrix;
    CSC_s3x3Matrix      sOutputRangeScaleMatrix;
    CSC_s3x3Matrix      sScaledMatrix;
    CSC_s3x3Matrix      sHSCMatrix;
    img_int32           i32InvertedScaleFactor;
    img_int32           i32FirstColumnValue_In;
    img_int32           i32SecondAndThirdColumnValue_In;
    img_int32           i32FirstColumnValue_Out;
    img_int32           i32SecondAndThirdColumnValue_Out;
    img_int32           i32InputYOffset;
    img_int32           i32InputUVOffset;
    img_int32           i32OutputYOffset;
    img_int32           i32OutputUVOffset;

    CSC_sColourSpaceConversionMatrix *          psThisColourSpaceConversion;
    CSC_sColourPrimaryConversionMatrix *        psThisColourPrimaryConversion;
    CSC_s3x3Matrix *                                            psCombinedRangeScaleMatrix;
    CSC_s3x3Matrix *                                            psInputRangeScaleMatrix;
    CSC_s3x3Matrix *                                            psOutputRangeScaleMatrix;
    CSC_s3x3Matrix *                                            psScaledMatrix;

    /* We need to keep two result matrices, as the matrix multiply function will not allow us to use a given    */
    /* matrix as both an input and an output.                                                                                                                                           */
    CSC_s3x3Matrix      sResultMatrices [2];

    CSC_ps3x3Matrix     psLatestResult          = IMG_NULL;
    img_uint8           ui8FreeResultMatrix = 0;        /* Index to currently unused member of 'sResultMatrices' */

    IMG_ASSERT(psResultantConfigurationData != IMG_NULL);
    IMG_ASSERT(eInputColourSpace < CSC_NO_OF_COLOURSPACES);
    IMG_ASSERT(eOutputColourSpace < CSC_NO_OF_COLOURSPACES);

    if (CSC_bAPIInitialised == IMG_FALSE) {
        /* Safety check static data */
        csc_StaticDataSafetyCheck();

        CSC_bAPIInitialised = IMG_TRUE;
    }

    /* Calculate scaling matrices */

    /* For both input and output ranges, if we are dealing with RGB */
    /* then all columns of the scalar matrix are set to identical       */
    /* values. If we are dealing with YUV, then the left hand           */
    /* column is the Y scaling value, while the middle and right        */
    /* hand columns are the UV scaling values.                                          */

    /* Input range scaling */
    if (asColourspaceInfo [eInputColourSpace].bIsYUV == IMG_TRUE) {
        i32FirstColumnValue_In = asCSC_RangeInfo [eInputRange].i32YScale;
        i32SecondAndThirdColumnValue_In = asCSC_RangeInfo [eInputRange].i32UVScale;
    } else {
        i32FirstColumnValue_In = asCSC_RangeInfo [eInputRange].i32RGBScale;
        i32SecondAndThirdColumnValue_In = i32FirstColumnValue_In;
    }

    if ((i32FirstColumnValue_In == CSC_FP(1.0)) &&
        (i32SecondAndThirdColumnValue_In == CSC_FP(1.0))) {
        psInputRangeScaleMatrix = IMG_NULL;
    } else {
        /* Complete input scaling matrix */
        sInputRangeScaleMatrix.ai32Coefficients[0][0] = i32FirstColumnValue_In;
        sInputRangeScaleMatrix.ai32Coefficients[1][0] = i32FirstColumnValue_In;
        sInputRangeScaleMatrix.ai32Coefficients[2][0] = i32FirstColumnValue_In;
        sInputRangeScaleMatrix.ai32Coefficients[0][1] = i32SecondAndThirdColumnValue_In;
        sInputRangeScaleMatrix.ai32Coefficients[1][1] = i32SecondAndThirdColumnValue_In;
        sInputRangeScaleMatrix.ai32Coefficients[2][1] = i32SecondAndThirdColumnValue_In;
        sInputRangeScaleMatrix.ai32Coefficients[0][2] = i32SecondAndThirdColumnValue_In;
        sInputRangeScaleMatrix.ai32Coefficients[1][2] = i32SecondAndThirdColumnValue_In;
        sInputRangeScaleMatrix.ai32Coefficients[2][2] = i32SecondAndThirdColumnValue_In;

        psInputRangeScaleMatrix = &sInputRangeScaleMatrix;
    }

    /* Output range scaling - output scale values are inverted (e.g.:   */
    /* if a given range requires that RGB values be scaled by a factor  */
    /* of 255/219 on the inputs, then the scaling range for the same    */
    /* range selected as an output will be 219/255).                                    */
    if (asColourspaceInfo [eOutputColourSpace].bIsYUV == IMG_TRUE) {
        if (asCSC_RangeInfo [eOutputRange].i32YScale == CSC_FP(1.0)) {
            i32InvertedScaleFactor = asCSC_RangeInfo [eOutputRange].i32YScale;
        } else {
            i32InvertedScaleFactor = FIXEDPT_64BitDivide_Signed((1 << CSC_FRACTIONAL_BITS),
                                     asCSC_RangeInfo [eOutputRange].i32YScale,
                                     (32 - CSC_FRACTIONAL_BITS));
        }

        i32FirstColumnValue_Out = i32InvertedScaleFactor;

        if (asCSC_RangeInfo [eOutputRange].i32UVScale == CSC_FP(1.0)) {
            i32InvertedScaleFactor = asCSC_RangeInfo [eOutputRange].i32UVScale;
        } else {
            i32InvertedScaleFactor = FIXEDPT_64BitDivide_Signed((1 << CSC_FRACTIONAL_BITS),
                                     asCSC_RangeInfo [eOutputRange].i32UVScale,
                                     (32 - CSC_FRACTIONAL_BITS));
        }

        i32SecondAndThirdColumnValue_Out = i32InvertedScaleFactor;
    } else {
        if (asCSC_RangeInfo [eOutputRange].i32RGBScale == CSC_FP(1.0)) {
            i32InvertedScaleFactor = asCSC_RangeInfo [eOutputRange].i32RGBScale;
        } else {
            i32InvertedScaleFactor = FIXEDPT_64BitDivide_Signed((1 << CSC_FRACTIONAL_BITS),
                                     asCSC_RangeInfo [eOutputRange].i32RGBScale,
                                     (32 - CSC_FRACTIONAL_BITS));
        }

        i32FirstColumnValue_Out = i32InvertedScaleFactor;
        i32SecondAndThirdColumnValue_Out = i32FirstColumnValue_Out;
    }

    if ((i32FirstColumnValue_Out == CSC_FP(1.0)) &&
        (i32SecondAndThirdColumnValue_Out == CSC_FP(1.0))) {
        psOutputRangeScaleMatrix = IMG_NULL;
    } else {
        /* Complete output scaling matrix */
        sOutputRangeScaleMatrix.ai32Coefficients[0][0] = i32FirstColumnValue_Out;
        sOutputRangeScaleMatrix.ai32Coefficients[1][0] = i32FirstColumnValue_Out;
        sOutputRangeScaleMatrix.ai32Coefficients[2][0] = i32FirstColumnValue_Out;
        sOutputRangeScaleMatrix.ai32Coefficients[0][1] = i32SecondAndThirdColumnValue_Out;
        sOutputRangeScaleMatrix.ai32Coefficients[1][1] = i32SecondAndThirdColumnValue_Out;
        sOutputRangeScaleMatrix.ai32Coefficients[2][1] = i32SecondAndThirdColumnValue_Out;
        sOutputRangeScaleMatrix.ai32Coefficients[0][2] = i32SecondAndThirdColumnValue_Out;
        sOutputRangeScaleMatrix.ai32Coefficients[1][2] = i32SecondAndThirdColumnValue_Out;
        sOutputRangeScaleMatrix.ai32Coefficients[2][2] = i32SecondAndThirdColumnValue_Out;

        psOutputRangeScaleMatrix = &sOutputRangeScaleMatrix;
    }

    if ((psInputRangeScaleMatrix == IMG_NULL) && (psOutputRangeScaleMatrix == IMG_NULL)) {
        psCombinedRangeScaleMatrix = IMG_NULL;
    } else if (psInputRangeScaleMatrix == IMG_NULL) {
        /* No input, just use output */
        psCombinedRangeScaleMatrix = psOutputRangeScaleMatrix;
    } else if (psOutputRangeScaleMatrix == IMG_NULL) {
        /* No output, just use input */
        psCombinedRangeScaleMatrix = psInputRangeScaleMatrix;
    } else if ((eInputRange != eOutputRange)
               ||
               (asColourspaceInfo [eInputColourSpace].bIsYUV != asColourspaceInfo [eOutputColourSpace].bIsYUV)) {       /* Scale factors for YUV and RGB can be different */
        /* Now combine the input and output scaling values */
        sCombinedRangeScaleMatrix.ai32Coefficients [0][0] =
            FIXEDPT_64BitMultiply_Signed(i32FirstColumnValue_In,
                                         i32FirstColumnValue_Out,
                                         CSC_FRACTIONAL_BITS);

        sCombinedRangeScaleMatrix.ai32Coefficients [0][1] =
            FIXEDPT_64BitMultiply_Signed(i32SecondAndThirdColumnValue_In,
                                         i32SecondAndThirdColumnValue_Out,
                                         CSC_FRACTIONAL_BITS);

        /* Duplicate values */
        sCombinedRangeScaleMatrix.ai32Coefficients [1][0] = sCombinedRangeScaleMatrix.ai32Coefficients [0][0];
        sCombinedRangeScaleMatrix.ai32Coefficients [2][0] = sCombinedRangeScaleMatrix.ai32Coefficients [0][0];
        sCombinedRangeScaleMatrix.ai32Coefficients [1][1] = sCombinedRangeScaleMatrix.ai32Coefficients [0][1];
        sCombinedRangeScaleMatrix.ai32Coefficients [2][1] = sCombinedRangeScaleMatrix.ai32Coefficients [0][1];
        sCombinedRangeScaleMatrix.ai32Coefficients [0][2] = sCombinedRangeScaleMatrix.ai32Coefficients [0][1];
        sCombinedRangeScaleMatrix.ai32Coefficients [1][2] = sCombinedRangeScaleMatrix.ai32Coefficients [0][1];
        sCombinedRangeScaleMatrix.ai32Coefficients [2][2] = sCombinedRangeScaleMatrix.ai32Coefficients [0][1];

        psCombinedRangeScaleMatrix = &sCombinedRangeScaleMatrix;
    } else {
        /* No combined range scaling required */
        psCombinedRangeScaleMatrix = IMG_NULL;
    }

    /* If HSBC information has been provided, then generate an HSBC matrix */
    if (psHSBCSettings != IMG_NULL) {
        CSC_GenerateHSCMatrix(psHSBCSettings, &sHSCMatrix);

#if CSC_PRINT_DEBUG_INFO
        printf("\nCSC - HSBC settings will be applied\n");
#endif
    }

    /* Check this primary->primary conversion is supported */
    psThisColourPrimaryConversion = &(asCSCColourPrimaryConversionMatrices[eInputPrimary][eOutputPrimary]);
    IMG_ASSERT(psThisColourPrimaryConversion->bIsSupported == IMG_TRUE);

    if (asColourspaceInfo [eInputColourSpace].bIsYUV == IMG_TRUE) {
        if (asColourspaceInfo [eOutputColourSpace].bIsYUV == IMG_TRUE) {
            /****************************/
            /* CASE 1                                   */
            /* Input YUV, Output YUV    */
            /****************************/
            if (psThisColourPrimaryConversion->bIsIdentityMatrix == IMG_TRUE) {
#if CSC_PRINT_DEBUG_INFO
                printf("\nCSC - Optimised form of case 1, Input YUV/Output YUV\n");
#endif

                /* Case 1 optimisation */
                /* Can simplify equation, as we don't need to convert to RGB to do primary calculations */
                psThisColourSpaceConversion = &(asCSC_ColourSpaceConversionMatrices[eInputColourSpace][eOutputColourSpace]);
                IMG_ASSERT(psThisColourSpaceConversion->bIsSupported == IMG_TRUE);
                if (psThisColourSpaceConversion->bIsIdentityMatrix == IMG_FALSE) {
                    psLatestResult = &(psThisColourSpaceConversion->sMatrix);
                }

                /* Apply scaling */
                if (psCombinedRangeScaleMatrix != IMG_NULL) {
                    if (psLatestResult == IMG_NULL) {
                        /* We need a matrix to scale - in the absence of anything else, use the identity matrix */
                        psLatestResult = &sCSC_IdentityMatrix;
                    }

                    FIXEDPT_ScalarMatrixMultiply(3, 3,
                                                 (img_int32 *) &(psCombinedRangeScaleMatrix->ai32Coefficients),
                                                 (img_int32 *) &(psLatestResult->ai32Coefficients),
                                                 (img_int32 *)(&(sResultMatrices[ui8FreeResultMatrix].ai32Coefficients)),
                                                 CSC_FRACTIONAL_BITS);
                    psLatestResult = &(sResultMatrices[ui8FreeResultMatrix]);
                    ui8FreeResultMatrix = (1 - ui8FreeResultMatrix); /* Swap 'free result matrix' index */
                }

                /* Perform con/sat/hue calculations */
                if (psHSBCSettings != IMG_NULL) {
                    if (psLatestResult != IMG_NULL) {
                        CSC_MULTIPLY_MATRICES((&sHSCMatrix), psLatestResult, (&(sResultMatrices[ui8FreeResultMatrix])));
                        psLatestResult = &(sResultMatrices[ui8FreeResultMatrix]);
                        ui8FreeResultMatrix = (1 - ui8FreeResultMatrix); /* Swap 'free result matrix' index */
                    } else {
                        psLatestResult = &sHSCMatrix;
                    }
                }
            } else {
                /* Non optimised form of case 1 */
#if CSC_PRINT_DEBUG_INFO
                printf("\nCSC - Case 1, Input YUV/Output YUV\n");
#endif

                /* Perform con/sat/hue calculations */
                if (psHSBCSettings != IMG_NULL) {
                    psLatestResult = &sHSCMatrix;
                }

                /* We must convert to RGB prior to performing primary calculations */
                psThisColourSpaceConversion = &(asCSC_ColourSpaceConversionMatrices[eInputColourSpace][CSC_COLOURSPACE_RGB]);
                IMG_ASSERT(psThisColourSpaceConversion->bIsSupported == IMG_TRUE);
                if ((psThisColourSpaceConversion->bIsIdentityMatrix == IMG_FALSE) ||
                    (psInputRangeScaleMatrix != IMG_NULL)) {
                    if (psInputRangeScaleMatrix != IMG_NULL) {
                        /* Produce scaled version of IN_TRANSFER_YUV_TO_RGB matrix */
                        FIXEDPT_ScalarMatrixMultiply(3, 3,
                                                     (img_int32 *) &(psThisColourSpaceConversion->sMatrix.ai32Coefficients),
                                                     (img_int32 *) &(psInputRangeScaleMatrix->ai32Coefficients),
                                                     (img_int32 *) &(sScaledMatrix.ai32Coefficients),
                                                     CSC_FRACTIONAL_BITS);
                        psScaledMatrix = &sScaledMatrix;
                    } else {
                        psScaledMatrix = &(psThisColourSpaceConversion->sMatrix);
                    }

                    /* Now merge latest result with scaled matrix */
                    if (psLatestResult != IMG_NULL) {
                        CSC_MULTIPLY_MATRICES(psScaledMatrix, psLatestResult, (&(sResultMatrices[ui8FreeResultMatrix])));
                        psLatestResult = &(sResultMatrices[ui8FreeResultMatrix]);
                        ui8FreeResultMatrix = (1 - ui8FreeResultMatrix); /* Swap 'free result matrix' index */
                    } else {
                        psLatestResult = psScaledMatrix;
                    }
                }

                /* Now multiply by primaries (we have already covered the identity matrix case, above) */
                if (psLatestResult != IMG_NULL) {
                    CSC_MULTIPLY_MATRICES((&(psThisColourPrimaryConversion->sMatrix)), psLatestResult, (&(sResultMatrices[ui8FreeResultMatrix])));
                    psLatestResult = &(sResultMatrices[ui8FreeResultMatrix]);
                    ui8FreeResultMatrix = (1 - ui8FreeResultMatrix); /* Swap 'free result matrix' index */
                } else {
                    psLatestResult = (&(psThisColourPrimaryConversion->sMatrix));
                }

                /* Should definitely have an intermediate matrix by now */
                IMG_ASSERT(psLatestResult != IMG_NULL);

                /* Now convert back to YUV */
                psThisColourSpaceConversion = &(asCSC_ColourSpaceConversionMatrices[CSC_COLOURSPACE_RGB][eOutputColourSpace]);
                IMG_ASSERT(psThisColourSpaceConversion->bIsSupported == IMG_TRUE);
                if ((psThisColourSpaceConversion->bIsIdentityMatrix == IMG_FALSE) ||
                    (psOutputRangeScaleMatrix != IMG_NULL)) {
                    if (psOutputRangeScaleMatrix != IMG_NULL) {
                        /* Produce scaled version of OUT_TRANSFER_RGB_TO_YUV matrix */
                        FIXEDPT_ScalarMatrixMultiply(3, 3,
                                                     (img_int32 *) &(psThisColourSpaceConversion->sMatrix.ai32Coefficients),
                                                     (img_int32 *) &(psOutputRangeScaleMatrix->ai32Coefficients),
                                                     (img_int32 *) &(sScaledMatrix.ai32Coefficients),
                                                     CSC_FRACTIONAL_BITS);
                        psScaledMatrix = &sScaledMatrix;
                    } else {
                        psScaledMatrix = &(psThisColourSpaceConversion->sMatrix);
                    }

                    /* Now merge latest result with scaled matrix */
                    CSC_MULTIPLY_MATRICES(psScaledMatrix, psLatestResult, (&(sResultMatrices[ui8FreeResultMatrix])));
                    psLatestResult = &(sResultMatrices[ui8FreeResultMatrix]);
                    ui8FreeResultMatrix = (1 - ui8FreeResultMatrix); /* Swap 'free result matrix' index */
                }
            }
        } else {
            /****************************/
            /* CASE 2                                   */
            /* Input YUV, Output RGB    */
            /****************************/
#if CSC_PRINT_DEBUG_INFO
            printf("\nCSC - Case 2, Input YUV/Output RGB\n");
#endif

            /* Perform con/sat/hue calculations */
            if (psHSBCSettings != IMG_NULL) {
                psLatestResult = &sHSCMatrix;
            }

            /* Perform colour space conversion */
            psThisColourSpaceConversion = &(asCSC_ColourSpaceConversionMatrices[eInputColourSpace][eOutputColourSpace]);
            IMG_ASSERT(psThisColourSpaceConversion->bIsSupported == IMG_TRUE);
            if ((psThisColourSpaceConversion->bIsIdentityMatrix == IMG_FALSE) ||
                (psCombinedRangeScaleMatrix != IMG_NULL)) {
                if (psCombinedRangeScaleMatrix != IMG_NULL) {
                    /* Produce scaled version of IN_TRANSFER_YUV_TO_RGB matrix */
                    FIXEDPT_ScalarMatrixMultiply(3, 3,
                                                 (img_int32 *) &(psThisColourSpaceConversion->sMatrix.ai32Coefficients),
                                                 (img_int32 *) &(psCombinedRangeScaleMatrix->ai32Coefficients),
                                                 (img_int32 *) &(sScaledMatrix.ai32Coefficients),
                                                 CSC_FRACTIONAL_BITS);
                    psScaledMatrix = &sScaledMatrix;
                } else {
                    psScaledMatrix = &(psThisColourSpaceConversion->sMatrix);
                }

                /* Now merge latest result with scaled matrix */
                if (psLatestResult != IMG_NULL) {
                    CSC_MULTIPLY_MATRICES(psScaledMatrix, psLatestResult, (&(sResultMatrices[ui8FreeResultMatrix])));
                    psLatestResult = &(sResultMatrices[ui8FreeResultMatrix]);
                    ui8FreeResultMatrix = (1 - ui8FreeResultMatrix); /* Swap 'free result matrix' index */
                } else {
                    psLatestResult = psScaledMatrix;
                }
            }

            /* Now perform colour primary conversion */
            if (psThisColourPrimaryConversion->bIsIdentityMatrix == IMG_FALSE) {
                if (psLatestResult != IMG_NULL) {
                    CSC_MULTIPLY_MATRICES((&(psThisColourPrimaryConversion->sMatrix)), psLatestResult, (&(sResultMatrices[ui8FreeResultMatrix])));
                    psLatestResult = &(sResultMatrices[ui8FreeResultMatrix]);
                    ui8FreeResultMatrix = (1 - ui8FreeResultMatrix); /* Swap 'free result matrix' index */
                } else {
                    psLatestResult = (&(psThisColourPrimaryConversion->sMatrix));
                }
            }
        }
    } else {
        if (asColourspaceInfo [eOutputColourSpace].bIsYUV == IMG_TRUE) {
            /****************************/
            /* CASE 3                                   */
            /* Input RGB, Output YUV    */
            /****************************/
#if CSC_PRINT_DEBUG_INFO
            printf("\nCSC - Case 3, Input RGB/Output YUV\n");
#endif

            /* Perform colour primary conversion */
            if (psThisColourPrimaryConversion->bIsIdentityMatrix == IMG_FALSE) {
                psLatestResult = (&(psThisColourPrimaryConversion->sMatrix));
            }

            /* Now perform colour space conversion */
            psThisColourSpaceConversion = &(asCSC_ColourSpaceConversionMatrices[eInputColourSpace][eOutputColourSpace]);
            IMG_ASSERT(psThisColourSpaceConversion->bIsSupported == IMG_TRUE);
            if ((psThisColourSpaceConversion->bIsIdentityMatrix == IMG_FALSE) ||
                (psCombinedRangeScaleMatrix != IMG_NULL)) {
                if (psCombinedRangeScaleMatrix != IMG_NULL) {
                    /* Produce scaled version of IN_TRANSFER_YUV_TO_RGB matrix */
                    FIXEDPT_ScalarMatrixMultiply(3, 3,
                                                 (img_int32 *) &(psThisColourSpaceConversion->sMatrix.ai32Coefficients),
                                                 (img_int32 *) &(psCombinedRangeScaleMatrix->ai32Coefficients),
                                                 (img_int32 *) &(sScaledMatrix.ai32Coefficients),
                                                 CSC_FRACTIONAL_BITS);
                    psScaledMatrix = &sScaledMatrix;
                } else {
                    psScaledMatrix = &(psThisColourSpaceConversion->sMatrix);
                }

                /* Now merge latest result with scaled matrix */
                if (psLatestResult != IMG_NULL) {
                    CSC_MULTIPLY_MATRICES(psScaledMatrix, psLatestResult, (&(sResultMatrices[ui8FreeResultMatrix])));
                    psLatestResult = &(sResultMatrices[ui8FreeResultMatrix]);
                    ui8FreeResultMatrix = (1 - ui8FreeResultMatrix); /* Swap 'free result matrix' index */
                } else {
                    psLatestResult = psScaledMatrix;
                }
            }

            /* Perform con/sat/hue calculations */
            if (psHSBCSettings != IMG_NULL) {
                if (psLatestResult != IMG_NULL) {
                    CSC_MULTIPLY_MATRICES((&sHSCMatrix), psLatestResult, (&(sResultMatrices[ui8FreeResultMatrix])));
                    psLatestResult = &(sResultMatrices[ui8FreeResultMatrix]);
                    ui8FreeResultMatrix = (1 - ui8FreeResultMatrix); /* Swap 'free result matrix' index */
                } else {
                    psLatestResult = &sHSCMatrix;
                }
            }
        } else {
            /****************************/
            /* CASE 4                                   */
            /* Input RGB, Output RGB    */
            /****************************/

            /* If no HSBC modification, then the equation can be simplified */
            if (psHSBCSettings == IMG_NULL) {
                /* Optimised case 4 */
                /* As no HSBC information has been provided, the equation can be simplified */
#if CSC_PRINT_DEBUG_INFO
                printf("\nCSC - Optimised form of case 4, Input RGB/Output RGB\n");
#endif

                /* Perform colour primary conversion */
                if (psThisColourPrimaryConversion->bIsIdentityMatrix == IMG_FALSE) {
                    psLatestResult = (&(psThisColourPrimaryConversion->sMatrix));
                }

                /* Scale result */
                if (psCombinedRangeScaleMatrix != IMG_NULL) {
                    /* If there is no result to work with (to scale) then just use the identity matrix */
                    if (psLatestResult == IMG_NULL) {
                        psLatestResult = &sCSC_IdentityMatrix;
                    }

                    FIXEDPT_ScalarMatrixMultiply(3, 3,
                                                 (img_int32 *)(&(psLatestResult->ai32Coefficients)),
                                                 (img_int32 *) &(psCombinedRangeScaleMatrix->ai32Coefficients),
                                                 (img_int32 *) &(sScaledMatrix.ai32Coefficients),
                                                 CSC_FRACTIONAL_BITS);

                    psLatestResult = &sScaledMatrix;
                }
            } else {
                /* Non optimised form of case 4 */
#if CSC_PRINT_DEBUG_INFO
                printf("\nCSC - Case 4, Input RGB/Output RGB\n");
#endif

                /* Perform colour primary conversion - when HSBC modification is also being performed,  */
                /* this must be performed in two steps, so that the HSBC modification is performed in   */
                /* a fixed format (BT709 in the case of this algorithm).                                                                */
                psThisColourPrimaryConversion = &(asCSCColourPrimaryConversionMatrices[eInputPrimary][CSC_COLOUR_PRIMARY_BT709]);
                IMG_ASSERT(psThisColourPrimaryConversion->bIsSupported == IMG_TRUE);

                if (psThisColourPrimaryConversion->bIsIdentityMatrix == IMG_FALSE) {
                    psLatestResult = &(psThisColourPrimaryConversion->sMatrix);
                }

                /* We must convert to YUV prior to performing con/sat/hue calculations */
                psThisColourSpaceConversion = &(asCSC_ColourSpaceConversionMatrices[eInputColourSpace][CSC_COLOURSPACE_YCC_BT709]);
                IMG_ASSERT(psThisColourSpaceConversion->bIsSupported == IMG_TRUE);
                if ((psThisColourSpaceConversion->bIsIdentityMatrix == IMG_FALSE) ||
                    (psInputRangeScaleMatrix != IMG_NULL)) {
                    if (psInputRangeScaleMatrix != IMG_NULL) {
                        /* Produce scaled version of IN_TRANSFER_YUV_TO_RGB matrix */
                        FIXEDPT_ScalarMatrixMultiply(3, 3,
                                                     (img_int32 *) &(psThisColourSpaceConversion->sMatrix.ai32Coefficients),
                                                     (img_int32 *) &(psInputRangeScaleMatrix->ai32Coefficients),
                                                     (img_int32 *) &(sScaledMatrix.ai32Coefficients),
                                                     CSC_FRACTIONAL_BITS);
                        psScaledMatrix = &sScaledMatrix;
                    } else {
                        psScaledMatrix = &(psThisColourSpaceConversion->sMatrix);
                    }

                    /* Now merge latest result with scaled matrix */
                    if (psLatestResult != IMG_NULL) {
                        CSC_MULTIPLY_MATRICES(psScaledMatrix, psLatestResult, (&(sResultMatrices[ui8FreeResultMatrix])));
                        psLatestResult = &(sResultMatrices[ui8FreeResultMatrix]);
                        ui8FreeResultMatrix = (1 - ui8FreeResultMatrix); /* Swap 'free result matrix' index */
                    } else {
                        psLatestResult = psScaledMatrix;
                    }
                }

                /* Perform con/sat/hue calculations */
                IMG_ASSERT(psHSBCSettings != IMG_NULL);    /* There is an optimised case for dealing with no HSBC input - see above */

                if (psLatestResult != IMG_NULL) {
                    CSC_MULTIPLY_MATRICES((&sHSCMatrix), psLatestResult, (&(sResultMatrices[ui8FreeResultMatrix])));
                    psLatestResult = &(sResultMatrices[ui8FreeResultMatrix]);
                    ui8FreeResultMatrix = (1 - ui8FreeResultMatrix); /* Swap 'free result matrix' index */
                } else {
                    psLatestResult = &sHSCMatrix;
                }

                /* Should definitely have an intermediate matrix by now */
                IMG_ASSERT(psLatestResult != IMG_NULL);

                /* Now convert back to RGB */
                psThisColourSpaceConversion = &(asCSC_ColourSpaceConversionMatrices[CSC_COLOURSPACE_YCC_BT709][eOutputColourSpace]);
                IMG_ASSERT(psThisColourSpaceConversion->bIsSupported == IMG_TRUE);
                if ((psThisColourSpaceConversion->bIsIdentityMatrix == IMG_FALSE) ||
                    (psOutputRangeScaleMatrix != IMG_NULL)) {
                    if (psOutputRangeScaleMatrix != IMG_NULL) {
                        /* Produce scaled version of OUT_TRANSFER_RGB_TO_YUV matrix */
                        FIXEDPT_ScalarMatrixMultiply(3, 3,
                                                     (img_int32 *) &(psThisColourSpaceConversion->sMatrix.ai32Coefficients),
                                                     (img_int32 *) &(psOutputRangeScaleMatrix->ai32Coefficients),
                                                     (img_int32 *) &(sScaledMatrix.ai32Coefficients),
                                                     CSC_FRACTIONAL_BITS);
                        psScaledMatrix = &sScaledMatrix;
                    } else {
                        psScaledMatrix = &(psThisColourSpaceConversion->sMatrix);
                    }

                    /* Now merge latest result with scaled matrix */
                    CSC_MULTIPLY_MATRICES(psScaledMatrix, psLatestResult, (&(sResultMatrices[ui8FreeResultMatrix])));
                    psLatestResult = &(sResultMatrices[ui8FreeResultMatrix]);
                    ui8FreeResultMatrix = (1 - ui8FreeResultMatrix); /* Swap 'free result matrix' index */
                }

                /* Now convert to the requested output primary */
                psThisColourPrimaryConversion = &(asCSCColourPrimaryConversionMatrices[CSC_COLOUR_PRIMARY_BT709][eOutputPrimary]);
                IMG_ASSERT(psThisColourPrimaryConversion->bIsSupported == IMG_TRUE);

                if (psThisColourPrimaryConversion->bIsIdentityMatrix == IMG_FALSE) {
                    CSC_MULTIPLY_MATRICES((&(psThisColourPrimaryConversion->sMatrix)), psLatestResult, (&(sResultMatrices[ui8FreeResultMatrix])));
                    psLatestResult = &(sResultMatrices[ui8FreeResultMatrix]);
                    ui8FreeResultMatrix = (1 - ui8FreeResultMatrix); /* Swap 'free result matrix' index */
                }
            }
        }
    }

    /* Entire calculation resulted in no result - use identity matrix */
    if (psLatestResult == IMG_NULL) {
#if CSC_PRINT_DEBUG_INFO
        printf("\nCSC final output defaulting to identity matrix\n");
#endif

        psLatestResult = &sCSC_IdentityMatrix;
    }

#if CSC_PRINT_DEBUG_INFO
    printf("\nFinal CSC matrix :\n");
    CSC_PRINT_3x3_MATRIX(psLatestResult);
#endif

    /* Now copy the final results into the user supplied matrix */
    IMG_MEMCPY(&(psResultantConfigurationData->sCoefficients),
               psLatestResult,
               sizeof(CSC_s3x3Matrix));

    /* Finally, calculate input and output offsets */
    if (asColourspaceInfo [eInputColourSpace].bIsYUV == IMG_TRUE) {
        i32InputYOffset = -1 * (img_int32) asCSC_RangeInfo [eInputRange].ui32YOffset;
        i32InputUVOffset = -1 * (img_int32) asCSC_RangeInfo [eInputRange].ui32UVOffset;
    } else {
        i32InputYOffset = -1 * (img_int32) asCSC_RangeInfo [eInputRange].ui32RGBOffset;
        i32InputUVOffset = i32InputYOffset;
    }

    if (asColourspaceInfo [eOutputColourSpace].bIsYUV == IMG_TRUE) {
        i32OutputYOffset = (img_int32) asCSC_RangeInfo [eOutputRange].ui32YOffset;
        i32OutputUVOffset = (img_int32) asCSC_RangeInfo [eOutputRange].ui32UVOffset;
    } else {
        i32OutputYOffset = (img_int32) asCSC_RangeInfo [eOutputRange].ui32RGBOffset;
        i32OutputUVOffset = i32OutputYOffset;
    }

    /* Output offsets are specified with a fractional part */
    i32OutputYOffset = FIXEDPT_CONVERT_FRACTIONAL_BITS_NO_ROUNDING(i32OutputYOffset, 0, CSC_OUTPUT_OFFSET_FRACTIONAL_BITS);
    i32OutputUVOffset = FIXEDPT_CONVERT_FRACTIONAL_BITS_NO_ROUNDING(i32OutputUVOffset, 0, CSC_OUTPUT_OFFSET_FRACTIONAL_BITS);

    if (psHSBCSettings != IMG_NULL) {
        i32OutputYOffset += psHSBCSettings->i32Brightness;

        /* For YUV outputs, brightness is only applied to the Y output */
        if (asColourspaceInfo [eOutputColourSpace].bIsYUV == IMG_FALSE) {
            i32OutputUVOffset += psHSBCSettings->i32Brightness;
        }
    }

    /* Copy offsets into user provided structure */
    psResultantConfigurationData->ai32InputOffsets[0] = i32InputYOffset;
    psResultantConfigurationData->ai32InputOffsets[1] = i32InputUVOffset;
    psResultantConfigurationData->ai32InputOffsets[2] = i32InputUVOffset;

    psResultantConfigurationData->ai32OutputOffsets[0] = i32OutputYOffset;
    psResultantConfigurationData->ai32OutputOffsets[1] = i32OutputUVOffset;
    psResultantConfigurationData->ai32OutputOffsets[2] = i32OutputUVOffset;

#if CSC_PRINT_DEBUG_INFO
    printf("\nInput offsets:\n");
    printf("  [0] %03i\n", psResultantConfigurationData->ai32InputOffsets[0]);
    printf("  [1] %03i\n", psResultantConfigurationData->ai32InputOffsets[1]);
    printf("  [2] %03i\n", psResultantConfigurationData->ai32InputOffsets[2]);

    printf("\nOutput offsets:\n");
    printf("  [0] %f\n", (((img_float) psResultantConfigurationData->ai32OutputOffsets[0]) / (1 << CSC_OUTPUT_OFFSET_FRACTIONAL_BITS)));
    printf("  [1] %f\n", (((img_float) psResultantConfigurationData->ai32OutputOffsets[1]) / (1 << CSC_OUTPUT_OFFSET_FRACTIONAL_BITS)));
    printf("  [2] %f\n", (((img_float) psResultantConfigurationData->ai32OutputOffsets[2]) / (1 << CSC_OUTPUT_OFFSET_FRACTIONAL_BITS)));
#endif

    return IMG_SUCCESS;
};

img_result      CSC_GenerateHSCMatrix(CSC_psHSBCSettings        psHSBCSettings,
                                      CSC_ps3x3Matrix               psHSCMatrix)
{
    img_int32   i32CosHue;
    img_int32   i32SinHue;
    img_int32   i32HueInRadians;
    img_int32   i32ContrastTimesSaturation;

    IMG_ASSERT(psHSBCSettings != IMG_NULL);
    IMG_ASSERT(psHSCMatrix != IMG_NULL);

    /* Check all supplied values are in range */
    IMG_ASSERT(psHSBCSettings->i32Hue <= CSC_MAX_HUE);
    IMG_ASSERT(psHSBCSettings->i32Hue >= CSC_MIN_HUE);
    IMG_ASSERT(psHSBCSettings->i32Saturation <= CSC_MAX_SATURATION);
    IMG_ASSERT(psHSBCSettings->i32Saturation >= CSC_MIN_SATURATION);
    IMG_ASSERT(psHSBCSettings->i32Brightness <= CSC_MAX_BRIGHTNESS);
    IMG_ASSERT(psHSBCSettings->i32Brightness >= CSC_MIN_BRIGHTNESS);
    IMG_ASSERT(psHSBCSettings->i32Contrast <= CSC_MAX_CONTRAST);
    IMG_ASSERT(psHSBCSettings->i32Contrast >= CSC_MIN_CONTRAST);

    /* conSatHue matrix is constructed as follows :                                             */
    /*                                                                                                                                  */
    /* [ con,           0,                                                      0                                       ]       */
    /* [ 0,                     (con*sat*cos(hue)),                     (con*sat*sin(hue))      ]       */
    /* [ 0,                     -(con*sat*cos(hue)),            (con*sat*cos(hue))      ]       */

    /* Generate some useful values */
    i32HueInRadians = psHSBCSettings->i32Hue;
    if (i32HueInRadians < 0) {
        i32HueInRadians = i32HueInRadians * -1;
    }
    i32HueInRadians = FIXEDPT_64BitMultiply_Signed((img_int32) FIXEDPT_ONE_DEGREE_IN_RADIANS,           /* x.FIXEDPT_TRIG_INPUT_FRACTIONAL_BITS */
                      i32HueInRadians,                  /* x.CSC_HSC_FRACTIONAL_BITS                    */
                      CSC_HSC_FRACTIONAL_BITS);  /* Result is in x.FIXEDPT_TRIG_INPUT_FRACTIONAL_BITS format */

    i32CosHue = FIXEDPT_Cosine((img_uint32) i32HueInRadians);   /* Cosine is symmetrical about 0 degrees, hence cos(theta)=cos(-theta) */
    i32CosHue = FIXEDPT_CONVERT_FRACTIONAL_BITS(i32CosHue, FIXEDPT_TRIG_OUTPUT_FRACTIONAL_BITS, CSC_FRACTIONAL_BITS);

    i32SinHue = FIXEDPT_Sine((img_uint32) i32HueInRadians);   /* Sine is not symmetrical, however sin(theta)=-1 * sin(-theta) */
    if (psHSBCSettings->i32Hue < 0) {
        i32SinHue = -1 * i32SinHue;
    }
    i32SinHue = FIXEDPT_CONVERT_FRACTIONAL_BITS(i32SinHue, FIXEDPT_TRIG_OUTPUT_FRACTIONAL_BITS, CSC_FRACTIONAL_BITS);

    i32ContrastTimesSaturation = FIXEDPT_64BitMultiply_Signed(psHSBCSettings->i32Contrast,
                                 psHSBCSettings->i32Saturation,
                                 ((2 * CSC_HSC_FRACTIONAL_BITS) - CSC_FRACTIONAL_BITS));        /* Result is in x.CSC_FRACTIONAL_BITS format */

    /* Now complete matrix */
    /* Row 0 */
    psHSCMatrix->ai32Coefficients [0][0] = FIXEDPT_CONVERT_FRACTIONAL_BITS(psHSBCSettings->i32Contrast, CSC_HSC_FRACTIONAL_BITS, CSC_FRACTIONAL_BITS);
    psHSCMatrix->ai32Coefficients [0][1] = 0;
    psHSCMatrix->ai32Coefficients [0][2] = 0;

    /* Row 1 */
    psHSCMatrix->ai32Coefficients [1][0] = 0;
    psHSCMatrix->ai32Coefficients [1][1] = FIXEDPT_64BitMultiply_Signed(i32ContrastTimesSaturation,
                                           i32CosHue,
                                           CSC_FRACTIONAL_BITS);                /* Result is in x.CSC_FRACTIONAL_BITS format */
    psHSCMatrix->ai32Coefficients [1][2] = FIXEDPT_64BitMultiply_Signed(i32ContrastTimesSaturation,
                                           i32SinHue,
                                           CSC_FRACTIONAL_BITS);                /* Result is in x.CSC_FRACTIONAL_BITS format */

    /* Row 2 */
    psHSCMatrix->ai32Coefficients [2][0] = 0;
    psHSCMatrix->ai32Coefficients [2][1] = -1 * psHSCMatrix->ai32Coefficients [1][2];
    psHSCMatrix->ai32Coefficients [2][2] = psHSCMatrix->ai32Coefficients [1][1];

#if CSC_PRINT_DEBUG_INFO
    printf("\nHSC matrix :\n");
    CSC_PRINT_3x3_MATRIX(psHSCMatrix);
#endif

    return IMG_SUCCESS;
}

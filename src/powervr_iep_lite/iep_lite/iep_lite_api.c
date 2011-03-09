/*********************************************************************************
 *********************************************************************************
 **
 ** Name        : IEP_LITE_api.c
 ** Author      : Imagination Technologies
 **
 ** Copyright   : 2001 by Imagination Technologies Limited. All rights reserved
 **             : No part of this software, either material or conceptual
 **             : may be copied or distributed, transmitted, transcribed,
 **             : stored in a retrieval system or translated into any
 **             : human or computer language in any form by any means,
 **             : electronic, mechanical, manual or other-wise, or
 **             : disclosed to third parties without the express written
 **             : permission of Imagination Technologies Limited, Unit 8,
 **             : HomePark Industrial Estate, King's Langley, Hertfordshire,
 **             : WD4 8LZ, U.K.
 **
 ** Description : Contains the main IEP Lite control functions.
 **
 *********************************************************************************
 *********************************************************************************/

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

/*
	Includes
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "img_iep_defs.h"
#include "csc2.h"
#include "iep_lite_reg_defs.h"
#include "iep_lite_api.h"

#define	EXTERNAL
#include "iep_lite_utils.h"
#undef EXTERNAL

FILE * pfDebugOutput = IMG_NULL;

/*-------------------------------------------------------------------------------*/

/*
	Functions
*/

/*!
******************************************************************************

 @Function				IEP_LITE_Initialise

******************************************************************************/

img_result
IEP_LITE_Initialise(void * p_iep_lite_context, img_uint32 ui32RegBaseAddr)
{
    IEP_LITE_sContext * sIEP_LITE_Context = p_iep_lite_context;
    DEBUG_PRINT("Entering IEP_LITE_Initialise\n");

    if (NULL == sIEP_LITE_Context) {
        return IMG_FAILED;
    }
    /* If the API is already initialised, just return */
    if (sIEP_LITE_Context->bInitialised == IMG_TRUE) {
        return IMG_SUCCESS;
    }

    /* Check enumerated arrays are consistent with size & order of enumerations */
    iep_lite_StaticDataSafetyCheck();

    /* Register 'render complete' callback - this is a function which is called */
    /* automatically after the h/w has finished rendering.						*/
    REGISTER_CALLBACK(RENDER_COMPLETE, iep_lite_RenderCompleteCallback);

    IMG_MEMSET(sIEP_LITE_Context,
               0x00,
               sizeof(IEP_LITE_sContext));

    sIEP_LITE_Context->bInitialised = IMG_TRUE;
    sIEP_LITE_Context->ui32RegBaseAddr = ui32RegBaseAddr;

    DEBUG_PRINT("Leaving IEP_LITE_Initialise\n");

    return IMG_SUCCESS;
}

/*!
******************************************************************************

 @Function				IEP_LITE_UploadCSCMatrix

******************************************************************************/
img_result	IEP_LITE_UploadCSCMatrix(void * p_iep_lite_context,
                                    img_int32	ai32Coefficients[3][3])
{
    IEP_LITE_sContext * sIEP_LITE_Context = p_iep_lite_context;
    img_uint32 ui32MyReg = 0;

    DEBUG_PRINT("Entering IEP_LITE_UploadCSCMatrix\n");
    if (NULL == sIEP_LITE_Context) {
        return IMG_FAILED;
    }

    /* Ensure API is initialised */
    IMG_ASSERT(sIEP_LITE_Context->bInitialised == IMG_TRUE);

    ui32MyReg = 0;

    WRITE_BITFIELD(IEP_LITE_HS_C11C12_HS_C11,
                   (((img_uint32) ai32Coefficients [0][0]) >> (CSC_FRACTIONAL_BITS - IEP_LITE_CSC_FRACTIONAL_BITS_IN_COEFFICIENTS)),
                   ui32MyReg);

    WRITE_BITFIELD(IEP_LITE_HS_C11C12_HS_C12,
                   (((img_uint32) ai32Coefficients [0][1]) >> (CSC_FRACTIONAL_BITS - IEP_LITE_CSC_FRACTIONAL_BITS_IN_COEFFICIENTS)),
                   ui32MyReg);

    WRITE_REGISTER(IEP_LITE_HS_C11C12_OFFSET,
                   ui32MyReg);

    ui32MyReg = 0;

    WRITE_BITFIELD(IEP_LITE_HS_C13C21_HS_C13,
                   (((img_uint32) ai32Coefficients [0][2]) >> (CSC_FRACTIONAL_BITS - IEP_LITE_CSC_FRACTIONAL_BITS_IN_COEFFICIENTS)),
                   ui32MyReg);

    WRITE_BITFIELD(IEP_LITE_HS_C13C21_HS_C21,
                   (((img_uint32) ai32Coefficients [1][0]) >> (CSC_FRACTIONAL_BITS - IEP_LITE_CSC_FRACTIONAL_BITS_IN_COEFFICIENTS)),
                   ui32MyReg);

    WRITE_REGISTER(IEP_LITE_HS_C13C21_OFFSET,
                   ui32MyReg);

    ui32MyReg = 0;

    WRITE_BITFIELD(IEP_LITE_HS_C22C23_HS_C22,
                   (((img_uint32) ai32Coefficients [1][1]) >> (CSC_FRACTIONAL_BITS - IEP_LITE_CSC_FRACTIONAL_BITS_IN_COEFFICIENTS)),
                   ui32MyReg);

    WRITE_BITFIELD(IEP_LITE_HS_C22C23_HS_C23,
                   (((img_uint32) ai32Coefficients [1][2]) >> (CSC_FRACTIONAL_BITS - IEP_LITE_CSC_FRACTIONAL_BITS_IN_COEFFICIENTS)),
                   ui32MyReg);

    WRITE_REGISTER(IEP_LITE_HS_C22C23_OFFSET,
                   ui32MyReg);

    ui32MyReg = 0;

    WRITE_BITFIELD(IEP_LITE_HS_C31C32_HS_C31,
                   (((img_uint32) ai32Coefficients [2][0]) >> (CSC_FRACTIONAL_BITS - IEP_LITE_CSC_FRACTIONAL_BITS_IN_COEFFICIENTS)),
                   ui32MyReg);

    WRITE_BITFIELD(IEP_LITE_HS_C31C32_HS_C32,
                   (((img_uint32) ai32Coefficients [2][1]) >> (CSC_FRACTIONAL_BITS - IEP_LITE_CSC_FRACTIONAL_BITS_IN_COEFFICIENTS)),
                   ui32MyReg);

    WRITE_REGISTER(IEP_LITE_HS_C31C32_OFFSET,
                   ui32MyReg);

    ui32MyReg = 0;

    WRITE_BITFIELD(IEP_LITE_HS_C33_HS_C33,
                   (((img_uint32) ai32Coefficients [2][2]) >> (CSC_FRACTIONAL_BITS - IEP_LITE_CSC_FRACTIONAL_BITS_IN_COEFFICIENTS)),
                   ui32MyReg);

    WRITE_REGISTER(IEP_LITE_HS_C33_OFFSET,
                   ui32MyReg);

    DEBUG_PRINT("Leaving IEP_LITE_UploadCSCMatrix\n");
    return IMG_SUCCESS;
}

/*!
******************************************************************************

 @Function				IEP_LITE_IEPCSCConfigure

******************************************************************************/
img_result	IEP_LITE_CSCConfigure(
    void * p_iep_lite_context,
    CSC_eColourSpace	eInputColourSpace,
    CSC_eColourSpace	eOutputColourSpace,
    CSC_psHSBCSettings	psHSBCSettings
)
{
    IEP_LITE_sContext * sIEP_LITE_Context = p_iep_lite_context;
    CSC_sConfiguration				sInternalConfig;
    img_uint32						ui32MyReg;

    DEBUG_PRINT("Entering IEP_LITE_IEPCSCConfigure\n");
    if (NULL == sIEP_LITE_Context) {
        return IMG_FAILED;
    }
    /* Ensure API is initialised */
    IMG_ASSERT(sIEP_LITE_Context->bInitialised == IMG_TRUE);

    /* Generate the CSC matrix coefficients */
    CSC_GenerateMatrix(eInputColourSpace,
                       eOutputColourSpace,
                       CSC_RANGE_0_255,
                       CSC_RANGE_0_255,
                       CSC_COLOUR_PRIMARY_BT709,
                       CSC_COLOUR_PRIMARY_BT709,
                       psHSBCSettings,
                       &sInternalConfig);

    READ_REGISTER(sIEP_LITE_Context, IEP_LITE_HS_CONF_STATUS_OFFSET, &ui32MyReg);
    /* Colour configuration matrix */
    ui32MyReg = 0;

    WRITE_BITFIELD(IEP_LITE_HS_C11C12_HS_C11,
                   (((img_uint32) sInternalConfig.sCoefficients.ai32Coefficients [0][0]) >> (CSC_FRACTIONAL_BITS - IEP_LITE_CSC_FRACTIONAL_BITS_IN_COEFFICIENTS)),
                   ui32MyReg);

    WRITE_BITFIELD(IEP_LITE_HS_C11C12_HS_C12,
                   (((img_uint32) sInternalConfig.sCoefficients.ai32Coefficients [0][1]) >> (CSC_FRACTIONAL_BITS - IEP_LITE_CSC_FRACTIONAL_BITS_IN_COEFFICIENTS)),
                   ui32MyReg);

    WRITE_REGISTER(IEP_LITE_HS_C11C12_OFFSET,
                   ui32MyReg);

    ui32MyReg = 0;

    WRITE_BITFIELD(IEP_LITE_HS_C13C21_HS_C13,
                   (((img_uint32) sInternalConfig.sCoefficients.ai32Coefficients [0][2]) >> (CSC_FRACTIONAL_BITS - IEP_LITE_CSC_FRACTIONAL_BITS_IN_COEFFICIENTS)),
                   ui32MyReg);

    WRITE_BITFIELD(IEP_LITE_HS_C13C21_HS_C21,
                   (((img_uint32) sInternalConfig.sCoefficients.ai32Coefficients [1][0]) >> (CSC_FRACTIONAL_BITS - IEP_LITE_CSC_FRACTIONAL_BITS_IN_COEFFICIENTS)),
                   ui32MyReg);

    WRITE_REGISTER(IEP_LITE_HS_C13C21_OFFSET,
                   ui32MyReg);

    ui32MyReg = 0;

    WRITE_BITFIELD(IEP_LITE_HS_C22C23_HS_C22,
                   (((img_uint32) sInternalConfig.sCoefficients.ai32Coefficients [1][1]) >> (CSC_FRACTIONAL_BITS - IEP_LITE_CSC_FRACTIONAL_BITS_IN_COEFFICIENTS)),
                   ui32MyReg);

    WRITE_BITFIELD(IEP_LITE_HS_C22C23_HS_C23,
                   (((img_uint32) sInternalConfig.sCoefficients.ai32Coefficients [1][2]) >> (CSC_FRACTIONAL_BITS - IEP_LITE_CSC_FRACTIONAL_BITS_IN_COEFFICIENTS)),
                   ui32MyReg);

    WRITE_REGISTER(IEP_LITE_HS_C22C23_OFFSET,
                   ui32MyReg);

    ui32MyReg = 0;

    WRITE_BITFIELD(IEP_LITE_HS_C31C32_HS_C31,
                   (((img_uint32) sInternalConfig.sCoefficients.ai32Coefficients [2][0]) >> (CSC_FRACTIONAL_BITS - IEP_LITE_CSC_FRACTIONAL_BITS_IN_COEFFICIENTS)),
                   ui32MyReg);

    WRITE_BITFIELD(IEP_LITE_HS_C31C32_HS_C32,
                   (((img_uint32) sInternalConfig.sCoefficients.ai32Coefficients [2][1]) >> (CSC_FRACTIONAL_BITS - IEP_LITE_CSC_FRACTIONAL_BITS_IN_COEFFICIENTS)),
                   ui32MyReg);

    WRITE_REGISTER(IEP_LITE_HS_C31C32_OFFSET,
                   ui32MyReg);

    ui32MyReg = 0;

    WRITE_BITFIELD(IEP_LITE_HS_C33_HS_C33,
                   (((img_uint32) sInternalConfig.sCoefficients.ai32Coefficients [2][2]) >> (CSC_FRACTIONAL_BITS - IEP_LITE_CSC_FRACTIONAL_BITS_IN_COEFFICIENTS)),
                   ui32MyReg);

    WRITE_REGISTER(IEP_LITE_HS_C33_OFFSET,
                   ui32MyReg);

    /* Input Offset Cb0, Cr0, Y0 */
    ui32MyReg = 0;

    WRITE_BITFIELD(IEP_LITE_HS_CBOCRO_HS_CBO,
                   sInternalConfig.ai32InputOffsets [1],
                   ui32MyReg);

    WRITE_BITFIELD(IEP_LITE_HS_CBOCRO_HS_CRO,
                   sInternalConfig.ai32InputOffsets [2],
                   ui32MyReg);

    WRITE_REGISTER(IEP_LITE_HS_CBOCRO_OFFSET,
                   ui32MyReg);

    ui32MyReg = 0;

    WRITE_BITFIELD(IEP_LITE_HS_YO_HS_YO,
                   sInternalConfig.ai32InputOffsets [0],
                   ui32MyReg);

    WRITE_REGISTER(IEP_LITE_HS_YO_OFFSET,
                   ui32MyReg);

    /* Output Offset R0, G0, B0 */
    ui32MyReg = 0;

    WRITE_BITFIELD(IEP_LITE_HS_ROGO_HS_RO,
                   (sInternalConfig.ai32OutputOffsets [0] >> (CSC_OUTPUT_OFFSET_FRACTIONAL_BITS - IEP_LITE_CSC_FRACTIONAL_BITS_IN_OUTPUT_OFFSETS)),
                   ui32MyReg);

    WRITE_BITFIELD(IEP_LITE_HS_ROGO_HS_GO,
                   (sInternalConfig.ai32OutputOffsets [1] >> (CSC_OUTPUT_OFFSET_FRACTIONAL_BITS - IEP_LITE_CSC_FRACTIONAL_BITS_IN_OUTPUT_OFFSETS)),
                   ui32MyReg);

    WRITE_REGISTER(IEP_LITE_HS_ROGO_OFFSET,
                   ui32MyReg);

    ui32MyReg = 0;

    WRITE_BITFIELD(IEP_LITE_HS_BO_HS_BO,
                   (sInternalConfig.ai32OutputOffsets [2] >> (CSC_OUTPUT_OFFSET_FRACTIONAL_BITS - IEP_LITE_CSC_FRACTIONAL_BITS_IN_OUTPUT_OFFSETS)),
                   ui32MyReg);

    WRITE_REGISTER(IEP_LITE_HS_BO_OFFSET,
                   ui32MyReg);

    /* Enable the CSC */
    WRITE_REGISTER(IEP_LITE_HS_CONF_STATUS_OFFSET,
                   IEP_LITE_HS_CONF_STATUS_HS_EN_MASK);

    READ_REGISTER(sIEP_LITE_Context, IEP_LITE_HS_CONF_STATUS_OFFSET, &ui32MyReg);
    DEBUG_PRINT("Leaving IEP_LITE_IEPCSCConfigure\n");

    return IMG_SUCCESS;
}

/*!
******************************************************************************

 @Function				IEP_LITE_BlackLevelExpanderConfigure

******************************************************************************/
img_result	IEP_LITE_BlackLevelExpanderConfigure(
    void * p_iep_lite_context,
    IEP_LITE_eBLEMode					eBLEBlackMode,
    IEP_LITE_eBLEMode					eBLEWhiteMode
)
{
    IEP_LITE_sContext * sIEP_LITE_Context = p_iep_lite_context;
    IEP_LITE_eBLEMode		eCurrentBlackMode;
    IEP_LITE_eBLEMode		eCurrentWhiteMode;

    DEBUG_PRINT("Entering IEP_LITE_BlackLevelExpanderConfigure\n");
    if (NULL == sIEP_LITE_Context) {
        return IMG_FAILED;
    }
    /* Ensure API is initialised */
    IMG_ASSERT(sIEP_LITE_Context->bInitialised == IMG_TRUE);

    /* Store current mode */
    eCurrentBlackMode = sIEP_LITE_Context->eBLEBlackMode;
    eCurrentWhiteMode = sIEP_LITE_Context->eBLEWhiteMode;

    sIEP_LITE_Context->eBLEBlackMode = eBLEBlackMode;
    sIEP_LITE_Context->eBLEWhiteMode = eBLEWhiteMode;

    /* Are we switching the hardware off ? */
    if (
        ((eBLEBlackMode == IEP_LITE_BLE_OFF) &&
         (eBLEWhiteMode == IEP_LITE_BLE_OFF))
        &&
        ((eCurrentBlackMode != IEP_LITE_BLE_OFF) ||
         (eCurrentWhiteMode != IEP_LITE_BLE_OFF))
    ) {
        WRITE_REGISTER(IEP_LITE_BLE_CONF_STATUS_OFFSET,
                       0);

        sIEP_LITE_Context->bFirstLUTValuesWritten 		= IMG_FALSE;
    }

    DEBUG_PRINT("Leaving IEP_LITE_BlackLevelExpanderConfigure\n");

    return IMG_SUCCESS;
}


/*!
******************************************************************************

 @Function				IEP_LITE_BlueStretchConfigure

******************************************************************************/
img_result	IEP_LITE_BlueStretchConfigure(
    void * p_iep_lite_context,
    img_uint8							ui8Gain
)
{
    IEP_LITE_sContext * sIEP_LITE_Context = p_iep_lite_context;
    img_uint32	ui32Offset = 0;
    img_uint32	ui32MyReg = 0;
    img_uint32	ui32EnableReg = 0;

    DEBUG_PRINT("Entering IEP_LITE_BlueStretchConfigure\n");
    if (NULL == sIEP_LITE_Context) {
        return IMG_FAILED;
    }
    /* Ensure API is initialised */
    IMG_ASSERT(sIEP_LITE_Context->bInitialised == IMG_TRUE);

    /* Standard setup using gain slider */
    READ_REGISTER(sIEP_LITE_Context,
                  IEP_LITE_BSSCC_CTRL_OFFSET,
                  &ui32EnableReg);

    if (ui8Gain == 0) {
        /* Blue stretch off */
        WRITE_BITFIELD(IEP_LITE_BSSCC_CTRL_BS_EN,
                       0,
                       ui32EnableReg);

        WRITE_REGISTER(IEP_LITE_BSSCC_CTRL_OFFSET,
                       ui32EnableReg);
    } else {
        /* Blue stretch radius chroma parameters */
        ui32MyReg = 0;

        WRITE_BITFIELD(IEP_LITE_BS_CHROMA_RADIUS_R3,
                       IEP_LITE_BS_R3_DEFAULT_VALUE,
                       ui32MyReg);

        WRITE_BITFIELD(IEP_LITE_BS_CHROMA_RADIUS_R4,
                       IEP_LITE_BS_R4_DEFAULT_VALUE,
                       ui32MyReg);

        WRITE_REGISTER(IEP_LITE_BS_CHROMA_RADIUS_OFFSET,
                       ui32MyReg);

        /* Blue stretch angle chroma preferences */
        ui32MyReg = 0;

        WRITE_BITFIELD(IEP_LITE_BS_CHROMA_ANGLE_THETA3,
                       ((IEP_LITE_BS_THETA3_DEFAULT_VALUE * (1 << 10)) / 360),
                       ui32MyReg);

        WRITE_BITFIELD(IEP_LITE_BS_CHROMA_ANGLE_THETA4,
                       ((IEP_LITE_BS_THETA4_DEFAULT_VALUE * (1 << 10)) / 360),
                       ui32MyReg);

        WRITE_REGISTER(IEP_LITE_BS_CHROMA_ANGLE_OFFSET,
                       ui32MyReg);

        /* Blue stretch area luma parameters */
        WRITE_REGISTER(IEP_LITE_BS_LUMA_OFFSET,
                       IEP_LITE_BS_YMIN_DEFAULT_VALUE);

        /* Blue stretch correction parameters - controlled by gain slider */

        /* Scale between 0 and IEP_LITE_BS_OFFSET_MAX_VALUE, using ui8Gain */
        ui32Offset = IEP_LITE_BS_OFFSET_MAX_VALUE * ui8Gain; /* 0.23 * 8.0 = 8.23 */

        /* Rather than dividing by (1 << 8) and losing precision, just promote the decimal	*/
        /* point i.e.: Consider it a 0.31 value instead of an 8.23.							*/

        ui32Offset += (1 << (31 - 7)) >> 1; /* Prevent error when rounding down */
        ui32Offset >>= (31 - 7); /* Adjust for register */

        WRITE_REGISTER(IEP_LITE_BS_CORR_OFFSET,
                       ui32Offset);

        /* Turn blue stretch module on */
        WRITE_BITFIELD(IEP_LITE_BSSCC_CTRL_BS_EN,
                       1,
                       ui32EnableReg);

        WRITE_REGISTER(IEP_LITE_BSSCC_CTRL_OFFSET,
                       ui32EnableReg);
    }

    /* Store current setting in context structure */
    sIEP_LITE_Context->ui8BSGain = ui8Gain;

    DEBUG_PRINT("Leaving IEP_LITE_BlueStretchConfigure\n");

    return IMG_SUCCESS;
}

/*!
******************************************************************************

 @Function				IEP_LITE_SkinColourCorrectionConfigure

******************************************************************************/
img_result	IEP_LITE_SkinColourCorrectionConfigure(
    void * p_iep_lite_context,
    img_uint8							ui8Gain
)
{
    IEP_LITE_sContext * sIEP_LITE_Context = p_iep_lite_context;
    img_uint32	ui32MyReg = 0;
    img_uint32	ui32Offset = 0;
    img_uint32	ui32EnableReg = 0;

    DEBUG_PRINT("Entering IEP_LITE_SkinColourCorrectionConfigure\n");
    if (NULL == sIEP_LITE_Context) {
        return IMG_FAILED;
    }
    /* Ensure API is initialised */
    IMG_ASSERT(sIEP_LITE_Context->bInitialised == IMG_TRUE);

    READ_REGISTER(sIEP_LITE_Context,
                  IEP_LITE_BSSCC_CTRL_OFFSET,
                  &ui32EnableReg);

    /* Standard setup using gain slider */
    if (ui8Gain == 0) {
        /* Skin colour correction off */
        WRITE_BITFIELD(IEP_LITE_BSSCC_CTRL_SCC_EN,
                       0,
                       ui32EnableReg);

        /* FIXME: forget to disable control bit in register ? by Daniel Miao */
        WRITE_REGISTER(IEP_LITE_BSSCC_CTRL_OFFSET, ui32EnableReg);
    } else {
        /* Skin colour correction on */
        WRITE_BITFIELD(IEP_LITE_BSSCC_CTRL_SCC_EN,
                       1,
                       ui32EnableReg);

        /* FIXME: forget to enable control bit in register ? by Daniel Miao */
        WRITE_REGISTER(IEP_LITE_BSSCC_CTRL_OFFSET, ui32EnableReg);

        /* Skin colour correction colour radius parameters */
        ui32MyReg = 0;

        WRITE_BITFIELD(IEP_LITE_SCC_RADIUS_R2,
                       IEP_LITE_SCC_R2_DEFAULT_VALUE,
                       ui32MyReg);

        WRITE_BITFIELD(IEP_LITE_SCC_RADIUS_R1,
                       IEP_LITE_SCC_R1_DEFAULT_VALUE,
                       ui32MyReg);

        WRITE_REGISTER(IEP_LITE_SCC_RADIUS_OFFSET,
                       ui32MyReg);

        /* Skin colour correction colour angle parameters */
        ui32MyReg = 0;

        WRITE_BITFIELD(IEP_LITE_SCC_ANGLE_THETA2,
                       ((IEP_LITE_SCC_THETA2_DEFAULT_VALUE * (1 << 10)) / 360),
                       ui32MyReg);

        WRITE_BITFIELD(IEP_LITE_SCC_ANGLE_THETA1,
                       ((IEP_LITE_SCC_THETA1_DEFAULT_VALUE * (1 << 10)) / 360),
                       ui32MyReg);

        WRITE_REGISTER(IEP_LITE_SCC_ANGLE_OFFSET,
                       ui32MyReg);

        /* Skin colour correction parameters - controlled by gain slider */

        /* Scale between 0 and IEP_LITE_SCC_OFFSET_MAX_VALUE, using ui8Gain 	 */
        ui32Offset = IEP_LITE_SCC_OFFSET_MAX_VALUE * ui8Gain; /* 0.23 * 8.0 = 8.23 */

        /* Rather than dividing by (1 << 8) and losing precision, just promote the decimal	*/
        /* point i.e.: Consider it a 0.31 value instead of an 8.23.			*/

        /* Note: gain register in hardware 'works backwards' 						*/
        /* (i.e.: 1023 is min gain, 0 is max gain).											*/
        ui32Offset = (1 << 31) - ui32Offset;

        ui32Offset += (1 << (31 - 10)) >> 1; /* Prevent error when rounding down 			*/
        ui32Offset >>= (31 - 10); /* Adjust for register */

        WRITE_REGISTER(IEP_LITE_SCC_CORR_OFFSET,
                       ui32Offset);
    }

    /* Store current setting in context structure */
    sIEP_LITE_Context->ui8SCCGain = ui8Gain;

    DEBUG_PRINT("Leaving IEP_LITE_SkinColourCorrectionConfigure\n");

    return IMG_SUCCESS;
}

/*--------------------------- End of File --------------------------------*/





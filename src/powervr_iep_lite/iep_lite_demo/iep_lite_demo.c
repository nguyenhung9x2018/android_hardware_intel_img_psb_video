#include <stdio.h>

#include "img_defs.h"
#include "csc2.h"
#include "iep_lite_api.h"

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

img_void iep_lite_RenderCompleteCallback(img_void);

int main(void)
{
    CSC_sHSBCSettings   sHSBCSettings;

    DEBUG_PRINT("IEP Lite demo\n");

    /* Initialise API */
    IEP_LITE_Initialise();

    /* Configure BLE module */
    IEP_LITE_BlackLevelExpanderConfigure(IEP_LITE_BLE_LOW, IEP_LITE_BLE_HIGH);

    /* Configure blue stretch module */
    IEP_LITE_BlueStretchConfigure(200);

    /* Configure skin colour correction module */
    IEP_LITE_SkinColourCorrectionConfigure(100);

    /* Configure colour space converter */
    sHSBCSettings.i32Hue                = (img_int32)(5.25f * (1 << 25));
    sHSBCSettings.i32Saturation = (img_int32)(1.07f * (1 << 25));
    sHSBCSettings.i32Brightness = (img_int32)(-10.1f * (1 << 10));
    sHSBCSettings.i32Contrast   = (img_int32)(0.99f * (1 << 25));

    IEP_LITE_CSCConfigure(CSC_COLOURSPACE_YCC_BT601,
                          CSC_COLOURSPACE_RGB,
                          &sHSBCSettings);

    /* The h/w would now be kicked (not shown here) */

    /* Once the render has completed, the render callback will be invoked */
    /* Simulate this */
    iep_lite_RenderCompleteCallback();

    DEBUG_PRINT("End of demo\n");

    fclose(pfDebugOutput);

    return 0;
}
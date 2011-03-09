/******************************************************************************
 @file   : csc2_data.c

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
         This module contains data used by the CSC library.

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

#include "img_iep_defs.h"
#include "fixedpointmaths.h"
#include "csc2.h"
#include "csc2_data.h"

img_result	csc_StaticDataSafetyCheck(img_void)
{
    img_uint32	ui32i, ui32j;

    for (ui32i = 0; ui32i < CSC_NO_OF_COLOURSPACES; ui32i++) {
        IMG_ASSERT(asColourspaceInfo [ui32i].eColourspace == (CSC_eColourSpace) ui32i);
    }

    for (ui32i = 0; ui32i < CSC_NO_OF_COLOURSPACES; ui32i++) {
        for (ui32j = 0; ui32j < CSC_NO_OF_COLOURSPACES; ui32j++) {
            IMG_ASSERT(asCSC_ColourSpaceConversionMatrices [ui32i][ui32j].eInputColourspace == (CSC_eColourSpace) ui32i);
            IMG_ASSERT(asCSC_ColourSpaceConversionMatrices [ui32i][ui32j].eOutputColourspace == (CSC_eColourSpace) ui32j);
        }
    }

    for (ui32i = 0; ui32i < CSC_NO_OF_RANGES; ui32i++) {
        IMG_ASSERT(asCSC_RangeInfo [ui32i].eRange == (CSC_eRange) ui32i);
    }

    for (ui32i = 0; ui32i < CSC_NO_OF_COLOUR_PRIMARIES; ui32i++) {
        for (ui32j = 0; ui32j < CSC_NO_OF_COLOUR_PRIMARIES; ui32j++) {
            IMG_ASSERT(asCSCColourPrimaryConversionMatrices [ui32i][ui32j].eInputColourPrimary == (CSC_eColourPrimary) ui32i);
            IMG_ASSERT(asCSCColourPrimaryConversionMatrices [ui32i][ui32j].eOutputColourPrimary == (CSC_eColourPrimary) ui32j);
        }
    }

    return IMG_SUCCESS;
}

CSC_s3x3Matrix sCSC_IdentityMatrix = {
    {
        { CSC_FP(1), CSC_FP(0), CSC_FP(0) },
        { CSC_FP(0), CSC_FP(1), CSC_FP(0) },
        { CSC_FP(0), CSC_FP(0), CSC_FP(1) }
    }
};

/* Stores additional information about supported colour spaces */
CSC_sAdditionalColourSpaceInformation asColourspaceInfo [CSC_NO_OF_COLOURSPACES] = {
    /*	Safety check					Is YUV?		*/
    {	CSC_COLOURSPACE_YCC_BT601,		IMG_TRUE	},
    {	CSC_COLOURSPACE_YCC_BT709,		IMG_TRUE	},
    {	CSC_COLOURSPACE_YCC_SMPTE_240,	IMG_TRUE	},
    {	CSC_COLOURSPACE_RGB,			IMG_FALSE	}
};

/* Stores conversion matrices for all combinations of input and output colour spaces */
CSC_sColourSpaceConversionMatrix	asCSC_ColourSpaceConversionMatrices [CSC_NO_OF_COLOURSPACES][CSC_NO_OF_COLOURSPACES] = {
    /**************/
    /* BT601 -> ? */
    /**************/
    {
        /* BT601 -> BT601 */
        {
            CSC_COLOURSPACE_YCC_BT601,			/* Input colour space	*/
            CSC_COLOURSPACE_YCC_BT601,			/* Output colour space	*/

            IMG_TRUE,							/* Is identity matrix?	*/
            IMG_TRUE,							/* Is supported?		*/

            /* Coefficients */
            {
                {
                    { CSC_FP(1.00000),	CSC_FP(0.00000),	CSC_FP(0.00000)	},
                    { CSC_FP(0.00000),	CSC_FP(1.00000),	CSC_FP(0.00000)	},
                    { CSC_FP(0.00000),	CSC_FP(0.00000),	CSC_FP(1.00000)	},
                }
            }
        },

        /* BT601 -> BT709 */
        {
            CSC_COLOURSPACE_YCC_BT601,			/* Input colour space	*/
            CSC_COLOURSPACE_YCC_BT709,			/* Output colour space	*/

            IMG_FALSE,							/* Is identity matrix?	*/
            IMG_TRUE,							/* Is supported?		*/

            /* Coefficients */
            {
                {
                    { CSC_FP(1.00000),	CSC_FP(-0.11819),	CSC_FP(-0.21268)	},
                    { CSC_FP(0.00000),	CSC_FP(1.01864),	CSC_FP(0.11462)		},
                    { CSC_FP(0.00000),	CSC_FP(0.07505),	CSC_FP(1.02532)		},
                }
            }
        },

        /* BT601 -> SMPTE240 */
        {
            CSC_COLOURSPACE_YCC_BT601,			/* Input colour space	*/
            CSC_COLOURSPACE_YCC_SMPTE_240,		/* Output colour space	*/

            IMG_FALSE,							/* Is identity matrix?	*/
            IMG_TRUE,							/* Is supported?		*/

            /* Coefficients */
            {
                {
                    { CSC_FP(1.00000),	CSC_FP(-0.08708),	CSC_FP(-0.20345)	},
                    { CSC_FP(0.00000),	CSC_FP(1.01813),	CSC_FP(0.11121)		},
                    { CSC_FP(0.00000),	CSC_FP(0.05568),	CSC_FP(1.01883)		},
                }
            }
        },

        /* BT601 -> RGB */
        {
            CSC_COLOURSPACE_YCC_BT601,			/* Input colour space	*/
            CSC_COLOURSPACE_RGB,				/* Output colour space	*/

            IMG_FALSE,							/* Is identity matrix?	*/
            IMG_TRUE,							/* Is supported?		*/

            /* Coefficients */
            {
                {
                    { CSC_FP(1.00000),	CSC_FP(0.00000),	CSC_FP(1.40200)	},
                    { CSC_FP(1.00000),	CSC_FP(-0.34414),	CSC_FP(-0.71414)},
                    { CSC_FP(1.00000),	CSC_FP(1.77200),	CSC_FP(0.00000)	},
                }
            }
        }
    },

    /**************/
    /* BT709 -> ? */
    /**************/
    {
        /* BT709 -> BT601 */
        {
            CSC_COLOURSPACE_YCC_BT709,			/* Input colour space	*/
            CSC_COLOURSPACE_YCC_BT601,			/* Output colour space	*/

            IMG_FALSE,							/* Is identity matrix?	*/
            IMG_TRUE,							/* Is supported?		*/

            /* Coefficients */
            {
                {
                    { CSC_FP(1.00000),	CSC_FP(0.10158),	CSC_FP(0.19607)		},
                    { CSC_FP(0.00000),	CSC_FP(0.98985),	CSC_FP(-0.11065)	},
                    { CSC_FP(0.00000),	CSC_FP(-0.07245),	CSC_FP(0.98340)		},
                }
            }
        },

        /* BT709 -> BT709 */
        {
            CSC_COLOURSPACE_YCC_BT709,			/* Input colour space	*/
            CSC_COLOURSPACE_YCC_BT709,			/* Output colour space	*/

            IMG_TRUE,							/* Is identity matrix?	*/
            IMG_TRUE,							/* Is supported?		*/

            /* Coefficients */
            {
                {
                    { CSC_FP(1.00000),	CSC_FP(0.00000),	CSC_FP(0.00000)	},
                    { CSC_FP(0.00000),	CSC_FP(1.00000),	CSC_FP(0.00000)	},
                    { CSC_FP(0.00000),	CSC_FP(0.00000),	CSC_FP(1.00000)	},
                }
            }
        },

        /* BT709 -> SMPTE240 */
        {
            CSC_COLOURSPACE_YCC_BT709,			/* Input colour space	*/
            CSC_COLOURSPACE_YCC_SMPTE_240,		/* Output colour space	*/

            IMG_FALSE,							/* Is identity matrix?	*/
            IMG_TRUE,							/* Is supported?		*/

            /* Coefficients */
            {
                {
                    { CSC_FP(1.00000),	CSC_FP(0.03012),	CSC_FP(0.00563)		},
                    { CSC_FP(0.00000),	CSC_FP(0.99974),	CSC_FP(-0.00329)	},
                    { CSC_FP(0.00000),	CSC_FP(-0.01870),	CSC_FP(0.99576)		},
                }
            }
        },

        /* BT709 -> RGB */
        {
            CSC_COLOURSPACE_YCC_BT709,			/* Input colour space	*/
            CSC_COLOURSPACE_RGB,				/* Output colour space	*/

            IMG_FALSE,							/* Is identity matrix?	*/
            IMG_TRUE,							/* Is supported?		*/

            /* Coefficients */
            {
                {
                    { CSC_FP(1.00000),	CSC_FP(0.00000),	CSC_FP(1.57480)	},
                    { CSC_FP(1.00000),	CSC_FP(-0.18732),	CSC_FP(-0.46813)},
                    { CSC_FP(1.00000),	CSC_FP(1.85560),	CSC_FP(0.00000)	},
                }
            }
        }
    },

    /*****************/
    /* SMPTE240 -> ? */
    /*****************/
    {
        /* SMPTE240 -> BT601 */
        {
            CSC_COLOURSPACE_YCC_SMPTE_240,		/* Input colour space	*/
            CSC_COLOURSPACE_YCC_BT601,			/* Output colour space	*/

            IMG_FALSE,							/* Is identity matrix?	*/
            IMG_TRUE,							/* Is supported?		*/

            /* Coefficients */
            {
                {
                    { CSC_FP(1.00000),	CSC_FP(0.07506),	CSC_FP(0.19150)		},
                    { CSC_FP(0.00000),	CSC_FP(0.98809),	CSC_FP(-0.10786)	},
                    { CSC_FP(0.00000),	CSC_FP(-0.05400),	CSC_FP(0.98741)		},
                }
            }
        },

        /* SMPTE240 -> BT709 */
        {
            CSC_COLOURSPACE_YCC_SMPTE_240,		/* Input colour space	*/
            CSC_COLOURSPACE_YCC_BT709,			/* Output colour space	*/

            IMG_FALSE,							/* Is identity matrix?	*/
            IMG_TRUE,							/* Is supported?		*/

            /* Coefficients */
            {
                {
                    { CSC_FP(1.00000),	CSC_FP(-0.03024),	CSC_FP(-0.00576)	},
                    { CSC_FP(0.00000),	CSC_FP(1.00032),	CSC_FP(0.00331)		},
                    { CSC_FP(0.00000),	CSC_FP(0.01879),	CSC_FP(1.00432)		},
                }
            }
        },

        /* SMPTE240 -> SMPTE240 */
        {
            CSC_COLOURSPACE_YCC_SMPTE_240,		/* Input colour space	*/
            CSC_COLOURSPACE_YCC_SMPTE_240,		/* Output colour space	*/

            IMG_TRUE,							/* Is identity matrix?	*/
            IMG_TRUE,							/* Is supported?		*/

            /* Coefficients */
            {
                {
                    { CSC_FP(1.00000),	CSC_FP(0.00000),	CSC_FP(0.00000)	},
                    { CSC_FP(0.00000),	CSC_FP(1.00000),	CSC_FP(0.00000)	},
                    { CSC_FP(0.00000),	CSC_FP(0.00000),	CSC_FP(1.00000)	},
                }
            }
        },

        /* SMPTE240 -> RGB */
        {
            CSC_COLOURSPACE_YCC_SMPTE_240,		/* Input colour space	*/
            CSC_COLOURSPACE_RGB,				/* Output colour space	*/

            IMG_FALSE,							/* Is identity matrix?	*/
            IMG_TRUE,							/* Is supported?		*/

            /* Coefficients */
            {
                {
                    { CSC_FP(1.00000),	CSC_FP(-0.00066),	CSC_FP(1.57585)		},
                    { CSC_FP(1.00000),	CSC_FP(-0.22642),	CSC_FP(-0.47653)	},
                    { CSC_FP(1.00000),	CSC_FP(1.82596),	CSC_FP(0.00038)		},
                }
            }
        }
    },

    /************/
    /* RGB -> ? */
    /************/
    {
        /* RGB -> BT601 */
        {
            CSC_COLOURSPACE_RGB,				/* Input colour space	*/
            CSC_COLOURSPACE_YCC_BT601,			/* Output colour space	*/

            IMG_FALSE,							/* Is identity matrix?	*/
            IMG_TRUE,							/* Is supported?		*/

            /* Coefficients */
            {
                {
                    { CSC_FP(0.29900),	CSC_FP(0.58700),	CSC_FP(0.11400)		},
                    { CSC_FP(-0.16874),	CSC_FP(-0.33126),	CSC_FP(0.50000)		},
                    { CSC_FP(0.50000),	CSC_FP(-0.41869),	CSC_FP(-0.08131)	},
                }
            }
        },

        /* RGB -> BT709 */
        {
            CSC_COLOURSPACE_RGB,				/* Input colour space	*/
            CSC_COLOURSPACE_YCC_BT709,			/* Output colour space	*/

            IMG_FALSE,							/* Is identity matrix?	*/
            IMG_TRUE,							/* Is supported?		*/

            /* Coefficients */
            {
                {
                    { CSC_FP(0.21260),	CSC_FP(0.71520),	CSC_FP(0.07220)		},
                    { CSC_FP(-0.11457),	CSC_FP(-0.38543),	CSC_FP(0.50000)		},
                    { CSC_FP(0.50000),	CSC_FP(-0.45415),	CSC_FP(-0.04585)	},
                }
            }
        },

        /* RGB -> SMPTE240 */
        {
            CSC_COLOURSPACE_RGB,				/* Input colour space	*/
            CSC_COLOURSPACE_YCC_SMPTE_240,		/* Output colour space	*/

            IMG_FALSE,							/* Is identity matrix?	*/
            IMG_TRUE,							/* Is supported?		*/

            /* Coefficients */
            {
                {
                    { CSC_FP(0.21197),	CSC_FP(0.70103),	CSC_FP(0.08700)		},
                    { CSC_FP(-0.11619),	CSC_FP(-0.38383),	CSC_FP(0.50002)		},
                    { CSC_FP(0.50002),	CSC_FP(-0.44502),	CSC_FP(-0.05500)	},
                }
            }
        },

        /* RGB -> RGB */
        {
            CSC_COLOURSPACE_RGB,				/* Input colour space	*/
            CSC_COLOURSPACE_RGB,				/* Output colour space	*/

            IMG_TRUE,							/* Is identity matrix?	*/
            IMG_TRUE,							/* Is supported?		*/

            /* Coefficients */
            {
                {
                    { CSC_FP(1.00000),	CSC_FP(0.00000),	CSC_FP(0.00000)	},
                    { CSC_FP(0.00000),	CSC_FP(1.00000),	CSC_FP(0.00000)	},
                    { CSC_FP(0.00000),	CSC_FP(0.00000),	CSC_FP(1.00000)	},
                }
            }
        }
    }
};

/* Stores additional information about input and output ranges */
CSC_sAdditionalRangeInformation asCSC_RangeInfo [CSC_NO_OF_RANGES] = {
    /* 0 -> 255 */
    {
        CSC_RANGE_0_255,

        CSC_FP(1),						/* RGB scale	*/
        CSC_FP(1),						/* Y scale		*/
        CSC_FP(1),						/* UV scale		*/

        0,								/* RGB offset	*/
        0,								/* Y offset		*/
        0								/* UV offset	*/
    },

    /* 16 -> 235 */
    {
        CSC_RANGE_16_235,

        CSC_FP((255.0f / 219.0f)),		/* RGB scale	*/
        CSC_FP((255.0f / 219.0f)),		/* Y scale		*/
        CSC_FP((255.0f / 224.0f)),		/* UV scale		*/

        16,								/* RGB offset	*/
        16,								/* Y offset		*/
        0								/* UV offset	*/
    },

    /* 48 -> 208 */
    {
        CSC_RANGE_48_208,

        CSC_FP((255.0f / 160.0f)),		/* RGB scale	*/
        CSC_FP((255.0f / 160.0f)),		/* Y scale		*/
        CSC_FP((255.0f / 160.0f)),		/* UV scale		*/

        48,								/* RGB offset	*/
        48,								/* Y offset		*/
        0								/* UV offset	*/
    }
};

/* Stores conversion matrices for all combinations of input and output colour primaries */
CSC_sColourPrimaryConversionMatrix	asCSCColourPrimaryConversionMatrices [CSC_NO_OF_COLOUR_PRIMARIES][CSC_NO_OF_COLOUR_PRIMARIES] = {
    /**************/
    /* BT709 -> ? */
    /**************/
    {
        /* BT709 -> BT709 */
        {
            CSC_COLOUR_PRIMARY_BT709,			/* Input colour primary		*/
            CSC_COLOUR_PRIMARY_BT709,			/* Output colour primary	*/

            IMG_TRUE,							/* Is identity matrix?	*/
            IMG_TRUE,							/* Is supported?		*/

            /* Coefficients */
            {
                {
                    { CSC_FP(1.00000),	CSC_FP(0.00000),	CSC_FP(0.00000)	},
                    { CSC_FP(0.00000),	CSC_FP(1.00000),	CSC_FP(0.00000)	},
                    { CSC_FP(0.00000),	CSC_FP(0.00000),	CSC_FP(1.00000)	},
                }
            }
        },

        /* BT709 -> BT601 */
        {
            CSC_COLOUR_PRIMARY_BT709,			/* Input colour primary		*/
            CSC_COLOUR_PRIMARY_BT601,			/* Output colour primary	*/

            IMG_FALSE,							/* Is identity matrix?	*/
            IMG_TRUE,							/* Is supported?		*/

            /* Coefficients */
            {
                {
                    { CSC_FP(1.0654),	CSC_FP(-0.0554),	CSC_FP(-0.0100)	},
                    { CSC_FP(-0.0196),	CSC_FP(1.0364),		CSC_FP(-0.0167)	},
                    { CSC_FP(0.0016),	CSC_FP(0.0044),		CSC_FP(0.9940)	},
                }
            }
        },

        /* BT709 -> BT470_2_SYSBG */
        {
            CSC_COLOUR_PRIMARY_BT709,			/* Input colour primary		*/
            CSC_COLOUR_PRIMARY_BT470_2_SYSBG,	/* Output colour primary	*/

            IMG_FALSE,							/* Is identity matrix?	*/
            IMG_TRUE,							/* Is supported?		*/

            /* Coefficients */
            {
                {
                    { CSC_FP(0.9578),	CSC_FP(0.0422),		CSC_FP(0.00000)	},
                    { CSC_FP(0.00000),	CSC_FP(1.00000),	CSC_FP(0.00000)	},
                    { CSC_FP(0.00000),	CSC_FP(-0.0119),	CSC_FP(1.0119)	},
                }
            }
        },

        /* BT709 -> BT470_2_SYSM */
        {
            CSC_COLOUR_PRIMARY_BT709,			/* Input colour primary		*/
            CSC_COLOUR_PRIMARY_BT470_2_SYSM,	/* Output colour primary	*/

            IMG_FALSE,							/* Is identity matrix?	*/
            IMG_TRUE,							/* Is supported?		*/

            /* Coefficients */
            {
                {
                    { CSC_FP(0.6688),	CSC_FP(0.2678),		CSC_FP(0.0323)	},
                    { CSC_FP(0.0185),	CSC_FP(1.0746),		CSC_FP(-0.0603)	},
                    { CSC_FP(0.0162),	CSC_FP(0.0431),		CSC_FP(0.8542)	},
                }
            }
        }
    },

    /**************/
    /* BT601 -> ? */
    /**************/
    {
        /* BT601 -> BT709 */
        {
            CSC_COLOUR_PRIMARY_BT601,			/* Input colour primary		*/
            CSC_COLOUR_PRIMARY_BT709,			/* Output colour primary	*/

            IMG_FALSE,							/* Is identity matrix?	*/
            IMG_TRUE,							/* Is supported?		*/

            /* Coefficients */
            {
                {
                    { CSC_FP(0.9395),	CSC_FP(0.0502),		CSC_FP(0.0103)	},
                    { CSC_FP(0.0178),	CSC_FP(0.9658),		CSC_FP(0.0164)	},
                    { CSC_FP(-0.0016),	CSC_FP(-0.0044),	CSC_FP(1.0060)	},
                }
            }
        },

        /* BT601 -> BT601 */
        {
            CSC_COLOUR_PRIMARY_BT601,			/* Input colour primary		*/
            CSC_COLOUR_PRIMARY_BT601,			/* Output colour primary	*/

            IMG_TRUE,							/* Is identity matrix?	*/
            IMG_TRUE,							/* Is supported?		*/

            /* Coefficients */
            {
                {
                    { CSC_FP(1.00000),	CSC_FP(0.00000),	CSC_FP(0.00000)	},
                    { CSC_FP(0.00000),	CSC_FP(1.00000),	CSC_FP(0.00000)	},
                    { CSC_FP(0.00000),	CSC_FP(0.00000),	CSC_FP(1.00000)	},
                }
            }
        },

        /* BT601 -> BT470_2_SYSBG */
        {
            CSC_COLOUR_PRIMARY_BT601,			/* Input colour primary		*/
            CSC_COLOUR_PRIMARY_BT470_2_SYSBG,	/* Output colour primary	*/

            IMG_FALSE,							/* Is identity matrix?	*/
            IMG_TRUE,							/* Is supported?		*/

            /* Coefficients */
            {
                {
                    { CSC_FP(0.9007),	CSC_FP(0.0888),		CSC_FP(0.0105)	},
                    { CSC_FP(0.0178),	CSC_FP(0.9658),		CSC_FP(0.0164)	},
                    { CSC_FP(-0.0019),	CSC_FP(-0.0159),	CSC_FP(1.0178)	},
                }
            }
        },

        /* BT601 -> BT470_2_SYSM */
        {
            CSC_COLOUR_PRIMARY_BT601,			/* Input colour primary		*/
            CSC_COLOUR_PRIMARY_BT470_2_SYSM,	/* Output colour primary	*/

            IMG_FALSE,							/* Is identity matrix?	*/
            IMG_TRUE,							/* Is supported?		*/

            /* Coefficients */
            {
                {
                    { CSC_FP(0.6330),	CSC_FP(0.2920),	CSC_FP(0.0438)	},
                    { CSC_FP(0.0366),	CSC_FP(1.0390),	CSC_FP(-0.0428)	},
                    { CSC_FP(0.0146),	CSC_FP(0.0387),	CSC_FP(0.8602)	},
                }
            }
        }
    },

    /**********************/
    /* BT470_2_SYSBG -> ? */
    /**********************/
    {
        /* BT470_2_SYSBG -> BT709 */
        {
            CSC_COLOUR_PRIMARY_BT470_2_SYSBG,	/* Input colour primary		*/
            CSC_COLOUR_PRIMARY_BT709,			/* Output colour primary	*/

            IMG_FALSE,							/* Is identity matrix?	*/
            IMG_TRUE,							/* Is supported?		*/

            /* Coefficients */
            {
                {
                    { CSC_FP(1.0440),	CSC_FP(-0.0440),	CSC_FP(0.00000)	},
                    { CSC_FP(0.00000),	CSC_FP(1.00000),	CSC_FP(0.00000)	},
                    { CSC_FP(0.00000),	CSC_FP(0.0118),		CSC_FP(0.9882)	},
                }
            }
        },

        /* BT470_2_SYSBG -> BT601 */
        {
            CSC_COLOUR_PRIMARY_BT470_2_SYSBG,	/* Input colour primary		*/
            CSC_COLOUR_PRIMARY_BT601,			/* Output colour primary	*/

            IMG_FALSE,							/* Is identity matrix?	*/
            IMG_TRUE,							/* Is supported?		*/

            /* Coefficients */
            {
                {
                    { CSC_FP(1.1123),	CSC_FP(-0.1024),	CSC_FP(-0.0099)	},
                    { CSC_FP(-0.0205),	CSC_FP(1.0370),		CSC_FP(-0.0165)	},
                    { CSC_FP(0.0017),	CSC_FP(0.0161),		CSC_FP(0.9822)	},
                }
            }
        },

        /* BT470_2_SYSBG -> BT470_2_SYSBG */
        {
            CSC_COLOUR_PRIMARY_BT470_2_SYSBG,	/* Input colour primary		*/
            CSC_COLOUR_PRIMARY_BT470_2_SYSBG,	/* Output colour primary	*/

            IMG_TRUE,							/* Is identity matrix?	*/
            IMG_TRUE,							/* Is supported?		*/

            /* Coefficients */
            {
                {
                    { CSC_FP(1.00000),	CSC_FP(0.00000),	CSC_FP(0.00000)	},
                    { CSC_FP(0.00000),	CSC_FP(1.00000),	CSC_FP(0.00000)	},
                    { CSC_FP(0.00000),	CSC_FP(0.00000),	CSC_FP(1.00000)	},
                }
            }
        },

        /* BT470_2_SYSBG -> BT470_2_SYSM */
        {
            CSC_COLOUR_PRIMARY_BT470_2_SYSBG,	/* Input colour primary		*/
            CSC_COLOUR_PRIMARY_BT470_2_SYSM,	/* Output colour primary	*/

            IMG_FALSE,							/* Is identity matrix?	*/
            IMG_TRUE,							/* Is supported?		*/

            /* Coefficients */
            {
                {
                    { CSC_FP(0.6982),	CSC_FP(0.2387),		CSC_FP(0.0319)	},
                    { CSC_FP(0.0193),	CSC_FP(1.0731),		CSC_FP(-0.0596)	},
                    { CSC_FP(0.0169),	CSC_FP(0.0525),		CSC_FP(0.8441)	},
                }
            }
        }
    },

    /*********************/
    /* BT470_2_SYSM -> ? */
    /*********************/
    {
        /* BT470_2_SYSM -> BT709 */
        {
            CSC_COLOUR_PRIMARY_BT470_2_SYSM,	/* Input colour primary		*/
            CSC_COLOUR_PRIMARY_BT709,			/* Output colour primary	*/

            IMG_FALSE,							/* Is identity matrix?	*/
            IMG_TRUE,							/* Is supported?		*/

            /* Coefficients */
            {
                {
                    { CSC_FP(1.5076),	CSC_FP(-0.3724),	CSC_FP(-0.0833)	},
                    { CSC_FP(-0.0275),	CSC_FP(0.9347),		CSC_FP(0.0670)	},
                    { CSC_FP(-0.0272),	CSC_FP(-0.0401),	CSC_FP(1.1689)	},
                }
            }
        },

        /* BT470_2_SYSM -> BT601 */
        {
            CSC_COLOUR_PRIMARY_BT470_2_SYSM,	/* Input colour primary		*/
            CSC_COLOUR_PRIMARY_BT601,			/* Output colour primary	*/

            IMG_FALSE,							/* Is identity matrix?	*/
            IMG_TRUE,							/* Is supported?		*/

            /* Coefficients */
            {
                {
                    { CSC_FP(1.6080),	CSC_FP(-0.4481),	CSC_FP(-0.1042)	},
                    { CSC_FP(-0.0576),	CSC_FP(0.9767),		CSC_FP(0.0516)	},
                    { CSC_FP(-0.0247),	CSC_FP(-0.0364),	CSC_FP(1.1620)	},
                }
            }
        },

        /* BT470_2_SYSM -> BT470_2_SYSBG */
        {
            CSC_COLOUR_PRIMARY_BT470_2_SYSM,	/* Input colour primary		*/
            CSC_COLOUR_PRIMARY_BT470_2_SYSBG,	/* Output colour primary	*/

            IMG_FALSE,							/* Is identity matrix?	*/
            IMG_TRUE,							/* Is supported?		*/

            /* Coefficients */
            {
                {
                    { CSC_FP(1.4429),	CSC_FP(-0.3172),	CSC_FP(-0.0770)	},
                    { CSC_FP(-0.0275),	CSC_FP(0.9347),		CSC_FP(0.0670)	},
                    { CSC_FP(-0.0272),	CSC_FP(-0.0518),	CSC_FP(1.1821)	},
                }
            }
        },

        /* BT470_2_SYSM -> BT470_2_SYSM */
        {
            CSC_COLOUR_PRIMARY_BT470_2_SYSM,	/* Input colour primary		*/
            CSC_COLOUR_PRIMARY_BT470_2_SYSM,	/* Output colour primary	*/

            IMG_TRUE,							/* Is identity matrix?	*/
            IMG_TRUE,							/* Is supported?		*/

            /* Coefficients */
            {
                {
                    { CSC_FP(1.00000),	CSC_FP(0.00000),	CSC_FP(0.00000)	},
                    { CSC_FP(0.00000),	CSC_FP(1.00000),	CSC_FP(0.00000)	},
                    { CSC_FP(0.00000),	CSC_FP(0.00000),	CSC_FP(1.00000)	},
                }
            }
        }
    }
};

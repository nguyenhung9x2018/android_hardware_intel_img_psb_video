/*
* Copyright (c) 2011 Intel Corporation. All Rights Reserved.
* Copyright (c) Imagination Technologies Limited, UK
*
* Permission is hereby granted, free of charge, to any person obtaining a
* copy of this software and associated documentation files (the
* "Software"), to deal in the Software without restriction, including
* without limitation the rights to use, copy, modify, merge, publish,
* distribute, sub license, and/or sell copies of the Software, and to
* permit persons to whom the Software is furnished to do so, subject to
* the following conditions:
*
* The above copyright notice and this permission notice (including the
* next paragraph) shall be included in all copies or substantial portions
* of the Software.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
* OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT.
* IN NO EVENT SHALL PRECISION INSIGHT AND/OR ITS SUPPLIERS BE LIABLE FOR
* ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
* TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
* SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*
*/
#include <stdio.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdarg.h>

#ifdef PDUMP_TEST
#include "topazscfwif.h"
#else
#include "tng_hostcode.h"
#include "tng_hostheader.h"
#include "tng_picmgmt.h"
#include "tng_jpegES.h"
#endif

#include "tng_trace.h"

#define PRINT_ARRAY_NEW( FEILD, NUM)            \
    for(i=0;i< NUM;i++) {                       \
        if(i%6==0)                              \
            printf("\n\t\t");                   \
        printf("\t0x%x", data->FEILD[i]); } \
    printf("\n\t\t}\n");

#define PRINT_ARRAY_INT( FEILD, NUM)            \
do {                                            \
    int tmp;                                    \
                                                \
    for(tmp=0;tmp< NUM;tmp++) {                 \
        if(tmp%6==0)                            \
            printf("\n\t\t");                   \
        printf("\t0x%08x", FEILD[tmp]);         \
    }                                           \
    printf("\n\t\t}\n");                        \
} while (0)


#define PRINT_ARRAY_BYTE( FEILD, NUM)           \
do {                                            \
    int tmp;                                    \
                                                \
    for(tmp=0;tmp< NUM;tmp++) {                 \
        if(tmp%8==0)                           \
            printf("\n\t\t");                   \
        printf("\t0x%02x", FEILD[tmp]);         \
    }                                           \
    printf("\n\t\t}\n");                        \
} while (0)


/*
#define PRINT_ARRAY( FEILD, NUM)                \
    for(i=0;i< NUM;i++)                         \
        printf("\t0x%x", data->FEILD[i]);       \
    printf("\n\t\t}\n");
*/

#define PRINT_ARRAY( FEILD, NUM)  PRINT_ARRAY_NEW(FEILD, NUM)                

#define PRINT_ARRAY_ADDR(STR, FEILD, NUM)                       \
do {                                                            \
    int i = 0;                                                  \
    printf("\n");                                               \
    for (i=0;i< NUM;i++)  {                                     \
        printf("\t\t%s[%02d]=x%08x = {\t", STR, i, data->FEILD[i]); \
        if (dump_address_content && data->FEILD[i]) {           \
            unsigned char *virt = phy2virt(data->FEILD[i]);     \
            PRINT_ARRAY_BYTE( virt, 64);                        \
        } else {                                                \
            printf("}\n");                                      \
        }                                                       \
    }                                                           \
} while (0)


unsigned int duplicate_setvideo_dump = 0;
unsigned int dump_address_content = 0;
static unsigned int last_setvideo_dump = 0;
static unsigned int hide_setvideo_dump = 0;

static unsigned int linear_fb = 0;
static unsigned int linear_mmio_topaz = 0;
static unsigned int phy_fb, phy_mmio;
static IMG_MTX_VIDEO_CONTEXT *mtx_ctx = NULL; /* MTX context */

static int setup_device()
{
    unsigned int linear_mmio;
    int fd;
    
    /* Allow read/write to ALL io ports */
    ioperm(0, 1024, 1);
    iopl(3);
    
    phy_mmio = pci_get_long(1,0<<3, PCI_BASE_ADDRESS_1);
    printf("MMIO:  PCI base1 for MMIO is 0x%08x\n", phy_mmio);
    phy_fb = pci_get_long(1,0<<3, PCI_BASE_ADDRESS_2);
    printf("DDRM:  PCI base2 for FB   is 0x%08x\n", phy_fb);
        
    phy_mmio &= 0xfff80000;
    phy_fb &= 0xfffff000;

    fd = open("/dev/mem", O_RDWR);
    if (fd == -1) {
        perror("open");
        exit(-1);
    }
    
    /* map frame buffer to user space(map 128M) */
    linear_fb = (unsigned int)mmap(NULL,128<<20,PROT_READ | PROT_WRITE, 
                             MAP_SHARED,fd, phy_fb);

    /* map mmio to user space(1M) */
    linear_mmio = (unsigned int)mmap(NULL,0x100000,PROT_READ | PROT_WRITE, 
                              MAP_SHARED,fd, phy_mmio);
    linear_mmio_topaz = linear_mmio;

    close(fd);

    return 0;
}

/* convert physicall address to virtual by search MMU */
static void *phy2virt_mmu(unsigned int phyaddr)
{
#ifdef _TOPAZHP_VIRTUAL_
    return phyaddr;
#endif
    unsigned int fb_start, fb_end;
    int pd_index, pt_index, pg_offset;
    unsigned int pd_phyaddr, pt_phyaddr; /* phyaddrss of page directory/table */
    unsigned int mem_start; /* memory start physicall address */
    unsigned int *pd, *pt;/* virtual of page directory/table */
    void *mem_virt;

    if (phy_mmio == 0 || phy_fb == 0 || linear_fb == 0 || linear_mmio_topaz == 0) {
        setup_device();
    }
    if (phy_mmio == 0 || phy_fb == 0 || linear_fb == 0 || linear_mmio_topaz == 0) {
        printf("ERROR:setup_device failed!\n");
        exit(-1);
    }
    
    /* first map page directory */
    MULTICORE_READ32(REGNUM_TOPAZ_CR_MMU_DIR_LIST_BASE_ADDR, &pd_phyaddr);
    printf("INFO: page directory 0x%08x, phy addr is 0x%08x\n", pd_phyaddr, phyaddr);

    pd_phyaddr &= 0xfffff000;
    fb_start = phy_fb;
    fb_end = fb_start + (128<<20);
    printf("ERROR: page directory 0x%08x is not fb range [0x%08x, 0x%08x]\n",
               pd_phyaddr, fb_start, fb_end);


    
    pd_index = phyaddr >> 22; /* the top 10bits are pd index */
    pt_index = (phyaddr >> 12) & 0x3ff; /* the middle 10 bits are pt index */
    pg_offset = phyaddr & 0xfff;

    
    if ((pd_phyaddr < fb_start) || (pd_phyaddr > fb_end)) {
        printf("ERROR: page directory 0x%08x is not fb range [0x%08x, 0x%08x]\n",
               pd_phyaddr, fb_start, fb_end);
        exit(-1);
    }
        
    /* find page directory entry */
    pd = (unsigned int *)(linear_fb + (pd_phyaddr - phy_fb));

    if ((pd[pd_index] & 1) == 0) {/* not valid */
        printf("Error: the page directory index is invalid, not mapped\n");
        exit(-1);
    }
    pt_phyaddr = pd[pd_index] & 0xfffff000;

    /* process page table entry */
    if ((pt_phyaddr < fb_start) || (pt_phyaddr > fb_end)) {
        printf("ERROR: page table 0x%08x is not fb range [0x%08x, 0x%08x]\n",
               pt_phyaddr, fb_start, fb_end);
        exit(-1);
    }
    pt = (unsigned int *)(linear_fb + (pt_phyaddr - phy_fb));
            
    if ((pt[pt_index] & 1) == 0) {
        printf("Error: the page table index is invalid, not mapped\n");
        exit(-1);
    }
    mem_start = pt[pt_index] & 0xfffff000;
    
#if 0
    printf("Phy=0x%08x(PD index=%d, PT index=%d, Page offset=%d)\n",
           phyaddr, pd_index, pt_index, pg_offset);
    printf("MMU PD Phy=0x%08x, PT Phy=PD[%d]=0x%08x, PT[%d]=0x%08x, means mem real phy(start)=0x%08x\n",
           pd_phyaddr, pd_index, pd[pd_index], pt_index, pt[pt_index], mem_start);
#endif    
    mem_virt = (void *)(linear_fb + (mem_start - phy_fb));

    return mem_virt;
}

static void *phy2virt(unsigned int phyaddr)
{
#ifdef PDUMP_TEST
    void* phy2virt_pdump(unsigned int phy);

    (void)phy2virt_mmu; /* silence the warning */
    return phy2virt_pdump(phyaddr);
#else
    return phy2virt_mmu(phyaddr);
#endif
}


static void JPEG_MTX_DMA_dump(JPEG_MTX_DMA_SETUP *data)
{
    int i;
    printf("\t\t	ComponentPlane{\n");
    for(i=0;i<MTX_MAX_COMPONENTS ;i++)
    {
        printf("\t\t\t   ui32PhysAddr=%d\n",data->ComponentPlane[i].ui32PhysAddr);
        printf("\t   ui32Stride=%d",data->ComponentPlane[i].ui32Stride);
        printf("\t   ui32Height=%d\n",data->ComponentPlane[i].ui32Height);
    }
    printf("\t\t	}\n");
    printf("\t\t	MCUComponent{\n");
    for(i=0;i<MTX_MAX_COMPONENTS ;i++)
    {
        printf("\t\t\t   ui32WidthBlocks=%d",data->MCUComponent[i].ui32WidthBlocks);
        printf("\t   ui32HeightBlocks=%d",data->MCUComponent[i].ui32HeightBlocks);
        printf("\t   ui32XLimit=%d\n",data->MCUComponent[i].ui32XLimit);
        printf("\t   ui32YLimit=%d\n",data->MCUComponent[i].ui32YLimit);
    }
    printf("\t\t	}\n");
    printf("\t\t	ui32ComponentsInScan =%d\n", data->ui32ComponentsInScan);
    printf("\t\t	ui32TableA =%d\n", data->ui32TableA);
    printf("\t\t	ui16DataInterleaveStatus =%d\n", data->ui16DataInterleaveStatus);
    printf("\t\t	ui16MaxPipes =%d\n", data->ui16MaxPipes);
    printf("\t\t	apWritebackRegions  {");
    PRINT_ARRAY(	apWritebackRegions, WB_FIFO_SIZE);
}

static void ISSUE_BUFFER_dump(MTX_ISSUE_BUFFERS *data)
{
    printf("\t\t	ui32MCUPositionOfScanAndPipeNo =%d\n", data->ui32MCUPositionOfScanAndPipeNo);
    printf("\t\t	ui32MCUCntAndResetFlag =%d\n", data->ui32MCUCntAndResetFlag);
}

static void JPEG_TABLE_dump(JPEG_MTX_QUANT_TABLE *data)
{
    int i;
    printf("\t\t	aui8LumaQuantParams  {");
    PRINT_ARRAY(	aui8LumaQuantParams, QUANT_TABLE_SIZE_BYTES);
    printf("\t\t	aui8ChromaQuantParams  {");
    PRINT_ARRAY(	aui8ChromaQuantParams, QUANT_TABLE_SIZE_BYTES);
}


static int SETVIDEO_ui32MVSettingsBTable_dump(unsigned int phyaddr)
{
    IMG_UINT32 ui32DistanceB, ui32Position;
    IMG_MV_SETTINGS * pHostMVSettingsBTable;
        
    pHostMVSettingsBTable = (IMG_MV_SETTINGS *) phy2virt(phyaddr);
    printf("\t\t(====ui32MVSettingsBTable====)\n");
    
    for (ui32DistanceB = 0; ui32DistanceB < MAX_BFRAMES; ui32DistanceB++)
    {
        for (ui32Position = 1; ui32Position <= ui32DistanceB + 1; ui32Position++)
        {
            IMG_MV_SETTINGS * pMvElement = (IMG_MV_SETTINGS * ) ((IMG_UINT8 *) pHostMVSettingsBTable + MV_OFFSET_IN_TABLE(ui32DistanceB, ui32Position - 1));
            printf("\t\t[ui32DistanceB=%d][ui32Position=%d].ui32MVCalc_Config=0x%08x\n",
                   ui32DistanceB, ui32Position, pMvElement->ui32MVCalc_Config);
            printf("\t\t[ui32DistanceB=%d][ui32Position=%d].ui32MVCalc_Colocated=0x%08x\n",
                   ui32DistanceB, ui32Position, pMvElement->ui32MVCalc_Colocated);
            printf("\t\t[ui32DistanceB=%d][ui32Position=%d].ui32MVCalc_Below=0x%08x\n",
                   ui32DistanceB, ui32Position, pMvElement->ui32MVCalc_Below);
        }
    }
    printf("\t\t(====ui32MVSettingsBTable====)\n");

    return 0;
}

static int SETVIDEO_ui32MVSettingsHierarchical_dump(unsigned int phyaddr)
{
    IMG_UINT32 ui32DistanceB;
    IMG_MV_SETTINGS * pHostMVSettingsHierarchical;
        
    pHostMVSettingsHierarchical = (IMG_MV_SETTINGS *) phy2virt(phyaddr);

    printf("\t\t(====ui32MVSettingsHierarchical====)\n");
    
    for (ui32DistanceB = 0; ui32DistanceB < MAX_BFRAMES; ui32DistanceB++) {
        IMG_MV_SETTINGS *pMvElement = pHostMVSettingsHierarchical + ui32DistanceB;

        printf("\t\t[ui32DistanceB=%d].ui32MVCalc_Config=0x%08x\n",
               ui32DistanceB, pMvElement->ui32MVCalc_Config);
        printf("\t\t[ui32DistanceB=%d].ui32MVCalc_Colocated=0x%08x\n",
               ui32DistanceB, pMvElement->ui32MVCalc_Colocated);
        printf("\t\t[ui32DistanceB=%d].ui32MVCalc_Below=0x%08x\n",
               ui32DistanceB, pMvElement->ui32MVCalc_Below);
        
    }
    printf("\t\t(====ui32MVSettingsHierarchical====)\n");

    return 0;
}


static int SETVIDEO_ui32FlatGopStruct_dump(unsigned int phyaddr)
{
    IMG_UINT16 * psGopStructure = (IMG_UINT16 * )phy2virt(phyaddr);
    int ui8EncodeOrderPos;
    
    printf("\t\t(====ui32FlatGopStruct====)\n");

    /* refer to DDK:MiniGop_GenerateFlat */
    for (ui8EncodeOrderPos = 0; ui8EncodeOrderPos < MAX_GOP_SIZE; ui8EncodeOrderPos++){
        printf("\t\tui32FlatGopStruct[%d]=0x%04x\n",ui8EncodeOrderPos, psGopStructure[ui8EncodeOrderPos]);
    }
    printf("\t\t(====ui32FlatGopStruct====)\n");

    return 0;
}


static int SETVIDEO_ui32HierarGopStruct_dump(unsigned int phyaddr)
{
    IMG_UINT16 * psGopStructure = (IMG_UINT16 * )phy2virt(phyaddr);
    int ui8EncodeOrderPos;
    
    printf("\t\t(====ui32HierarGopStruct====)\n");

    /* refer to DDK:MiniGop_GenerateFlat */
    for (ui8EncodeOrderPos = 0; ui8EncodeOrderPos < MAX_GOP_SIZE; ui8EncodeOrderPos++){
        printf("\t\tui32HierarGopStruct[%d]=0x%04x\n",ui8EncodeOrderPos, psGopStructure[ui8EncodeOrderPos]);
    }
    printf("\t\t(====ui32HierarGopStruct====)\n");

    return 0;
}


static char *IMG_FRAME_TEMPLATE_TYPE2Str(IMG_FRAME_TEMPLATE_TYPE tmp)
{
    switch (tmp){
    case IMG_FRAME_IDR:return "IMG_FRAME_IDR";
    case IMG_FRAME_INTRA:return "IMG_FRAME_INTRA";
    case IMG_FRAME_INTER_P:return "IMG_FRAME_INTER_P";
    case IMG_FRAME_INTER_B:return "IMG_FRAME_INTER_B";
    case IMG_FRAME_INTER_P_IDR:return "IMG_FRAME_INTER_P_IDR";
    case IMG_FRAME_UNDEFINED:return "IMG_FRAME_UNDEFINED";
    }

    return "Undefined";
}

static int MTX_HEADER_PARAMS_dump(MTX_HEADER_PARAMS *p);
static int apSliceParamsTemplates_dump(SLICE_PARAMS *p)
{
    unsigned char *ptmp = (unsigned char*)&p->sSliceHdrTmpl;
    printf("\t\tui32Flags=0x%08x\n", p->ui32Flags);
    printf("\t\tui32SliceConfig=0x%08x\n", p->ui32SliceConfig);
    printf("\t\tui32IPEControl=0x%08x\n", p->ui32IPEControl);
    printf("\t\tui32SeqConfig=0x%08x\n", p->ui32SeqConfig);
    printf("\t\teTemplateType=%s\n", IMG_FRAME_TEMPLATE_TYPE2Str(p->eTemplateType));

    PRINT_ARRAY_BYTE(ptmp, 64);

    MTX_HEADER_PARAMS_dump(&p->sSliceHdrTmpl);
    
    return 0;
}

static void SETVIDEO_dump(IMG_MTX_VIDEO_CONTEXT *data)
{
    unsigned int i;
    mtx_ctx = data;

    if(hide_setvideo_dump == 1)
        return ;
    printf("\t\t==========IMG_MTX_VIDEO_CONTEXT=============\n");
    printf("\t	ui64ClockDivBitrate=%lld\n", data->ui64ClockDivBitrate);
    printf("\t	ui32WidthInMbs=%d\n", data->ui32WidthInMbs);
    printf("\t	ui32PictureHeightInMbs=%d\n", data->ui32PictureHeightInMbs);
#ifdef FORCED_REFERENCE
    printf("\t	apTmpReconstructured  {");
    PRINT_ARRAY_ADDR("apTmpReconstructured",	apTmpReconstructured, MAX_PIC_NODES_CTXT);
#endif
    printf("\t	apReconstructured  {");
    PRINT_ARRAY_ADDR("apReconstructured",	apReconstructured, MAX_PIC_NODES_CTXT);
    printf("\t	apColocated  {");
    PRINT_ARRAY_ADDR("apColocated",	apColocated, MAX_PIC_NODES_CTXT);
    printf("\t	apMV  {");
    PRINT_ARRAY_ADDR("apMV",	apMV, MAX_MV_CTXT);	
    printf("\t	apInterViewMV  {");
//    PRINT_ARRAY(	apInterViewMV, 2 );
    PRINT_ARRAY_ADDR("apInterViewMV", apInterViewMV, 2);	

    printf("\t	ui32DebugCRCs=0x%x\n", data->ui32DebugCRCs);
    printf("\t	apWritebackRegions  {");
    PRINT_ARRAY_ADDR("apWritebackRegions",	apWritebackRegions, WB_FIFO_SIZE);
    printf("\t	ui32InitialCPBremovaldelayoffset=0x%x\n", data->ui32InitialCPBremovaldelayoffset);
    printf("\t	ui32MaxBufferMultClockDivBitrate=0x%x\n", data->ui32MaxBufferMultClockDivBitrate);
    printf("\t	pSEIBufferingPeriodTemplate=0x%x\n", data->pSEIBufferingPeriodTemplate);
    printf("\t	pSEIPictureTimingTemplate=0x%x\n", data->pSEIPictureTimingTemplate);
    printf("\t	b16EnableMvc=%d\n", data->b16EnableMvc);
    //printf("\t	b16EnableInterViewReference=%d\n", data->b16EnableInterViewReference);
    printf("\t	ui16MvcViewIdx=0x%x\n", data->ui16MvcViewIdx);

    printf("\t	apSliceParamsTemplates  {\n");
    //PRINT_ARRAY_ADDR( apSliceParamsTemplates, 5);
    for (i=0; i<5; i++) {
        printf("\t\tapSliceParamsTemplates[%d]=0x%08x  {\n", i, data->apSliceParamsTemplates[i]);
        apSliceParamsTemplates_dump(phy2virt(data->apSliceParamsTemplates[i]));
        printf("\t\t}\n");
    }

    printf("\t	apPicHdrTemplates  {");
    PRINT_ARRAY_ADDR("apPicHdrTemplates", apPicHdrTemplates, 4);
    MTX_HEADER_PARAMS_dump(phy2virt(data->apPicHdrTemplates[0]));
    printf("\t	aui32SliceMap  {");
    PRINT_ARRAY_ADDR("aui32SliceMap", 	aui32SliceMap, MAX_SOURCE_SLOTS_SL);

    printf("\t	ui32FlatGopStruct=0x%x\n", data->ui32FlatGopStruct);
    SETVIDEO_ui32FlatGopStruct_dump(data->ui32FlatGopStruct);

    printf("\t	apSeqHeader        =0x%x\n", data->apSeqHeader);
    if (data->apSeqHeader != 0)
	DO_HEADER_dump((MTX_HEADER_PARAMS *)(phy2virt(data->apSeqHeader)));
    printf("\t	apSubSetSeqHeader  =0x%x\n", data->apSubSetSeqHeader);
    if(data->apSubSetSeqHeader != 0)
        DO_HEADER_dump((MTX_HEADER_PARAMS *)(phy2virt(data->apSubSetSeqHeader)));
    printf("\t	bNoSequenceHeaders =0x%x\n", data->bNoSequenceHeaders);
    
    printf("\t	b8WeightedPredictionEnabled=%d\n", data->b8WeightedPredictionEnabled);
    printf("\t	ui8MTXWeightedImplicitBiPred=0x%x\n", data->ui8MTXWeightedImplicitBiPred);
    printf("\t	aui32WeightedPredictionVirtAddr  {");
    PRINT_ARRAY(aui32WeightedPredictionVirtAddr, MAX_SOURCE_SLOTS_SL);
    printf("\t	ui32HierarGopStruct=0x%x\n", data->ui32HierarGopStruct);
    if(data->ui32HierarGopStruct != 0)
        SETVIDEO_ui32HierarGopStruct_dump(data->ui32HierarGopStruct);

    printf("\t	pFirstPassOutParamAddr  {");
    PRINT_ARRAY_ADDR("pFirstPassOutParamAddr",pFirstPassOutParamAddr, MAX_SOURCE_SLOTS_SL);
#ifndef EXCLUDE_BEST_MP_DECISION_DATA
    printf("\t	pFirstPassOutBestMultipassParamAddr  {");
    PRINT_ARRAY_ADDR("pFirstPassOutBestMultipassParamAddr", pFirstPassOutBestMultipassParamAddr, MAX_SOURCE_SLOTS_SL);
#endif
    printf("\t	pMBCtrlInParamsAddr  {");
    PRINT_ARRAY_ADDR("pMBCtrlInParamsAddr", pMBCtrlInParamsAddr, MAX_SOURCE_SLOTS_SL);
    printf("\t	ui32InterIntraScale{");
    PRINT_ARRAY( ui32InterIntraScale, SCALE_TBL_SZ);
    printf("\t	ui32SkippedCodedScale  {");
    PRINT_ARRAY( ui32SkippedCodedScale, SCALE_TBL_SZ);


    printf("\t	ui32PicRowStride=0x%x\n", data->ui32PicRowStride);

    printf("\t	aui32BytesCodedAddr  {");
    PRINT_ARRAY_ADDR("aui32BytesCodedAddr", aui32BytesCodedAddr, MAX_CODED_BUFFERS);
    printf("\t	apAboveParams  {");
    PRINT_ARRAY_ADDR("apAboveParams", apAboveParams, TOPAZHP_NUM_PIPES);

    printf("\t	ui32IdrPeriod =0x%x\n ", data->ui32IdrPeriod);
    printf("\t	ui32IntraLoopCnt =0x%x\n", data->ui32IntraLoopCnt);
    printf("\t	ui32BFrameCount =0x%x\n", data->ui32BFrameCount);
    printf("\t	b8Hierarchical=%d\n", data->b8Hierarchical);
    printf("\t	ui8MPEG2IntraDCPrecision =0x%x\n", data->ui8MPEG2IntraDCPrecision);

    printf("\t	aui8PicOnLevel  {");
    PRINT_ARRAY(aui8PicOnLevel, MAX_REF_LEVELS_CTXT);
    printf("\t	ui32VopTimeResolution=0x%x\n", data->ui32VopTimeResolution);
    printf("\t	ui32InitialQp=0x%x\n", data->ui32InitialQp);
    printf("\t	ui32BUSize=0x%x\n", data->ui32BUSize);
    printf("\t	sMVSettingsIdr { \n\t\t\tui32MVCalc_Below=0x%x\n \t\t\tui32MVCalc_Colocated=0x%x\n \t\t\tui32MVCalc_Config=0x%x\n \t\t}\n", data->sMVSettingsIdr.ui32MVCalc_Below,data->sMVSettingsIdr.ui32MVCalc_Colocated, data->sMVSettingsIdr.ui32MVCalc_Config);

    printf("\t	sMVSettingsNonB { \n");
    for(i=0;i<MAX_BFRAMES +1;i++)
        printf("\t\t\tui32MVCalc_Below=0x%x   ui32MVCalc_Colocated=0x%x   ui32MVCalc_Config=0x%x }\n", data->sMVSettingsNonB[i].ui32MVCalc_Below,data->sMVSettingsNonB[i].ui32MVCalc_Colocated, data->sMVSettingsNonB[i].ui32MVCalc_Config);
    printf("\t\t}\n");

    printf(" \t	ui32MVSettingsBTable=0x%x\n", data->ui32MVSettingsBTable);
    SETVIDEO_ui32MVSettingsBTable_dump(data->ui32MVSettingsBTable);
    
    printf(" \t	ui32MVSettingsHierarchical=0x%x\n", data->ui32MVSettingsHierarchical);
    if(data->ui32MVSettingsHierarchical != 0)
        SETVIDEO_ui32MVSettingsHierarchical_dump(data->ui32MVSettingsHierarchical);
    
#ifdef FIRMWARE_BIAS
    printf("\t	aui32DirectBias_P  {");
    PRINT_ARRAY_NEW(aui32DirectBias_P,27 );
    printf("\t	aui32InterBias_P  {");
    PRINT_ARRAY_NEW(aui32InterBias_P,27 );
    printf("\t	aui32DirectBias_B  {");
    PRINT_ARRAY_NEW(aui32DirectBias_B,27 );
    printf("\t	aui32InterBias_B  {");
    PRINT_ARRAY_NEW(aui32InterBias_B,27 );
#endif
    printf("\t	eFormat=%d\n", data->eFormat);
    printf("\t	eStandard=%d\n", data->eStandard);
    printf("\t	eRCMode=%d\n", data->eRCMode);
    printf("\t	b8FirstPic=%d\n", data->b8FirstPic);
    printf("\t	b8IsInterlaced=%d\n", data->b8IsInterlaced);
    printf("\t	b8TopFieldFirst=%d\n", data->b8TopFieldFirst);
    printf("\t	b8ArbitrarySO=%d\n", data->b8ArbitrarySO);
    printf("\t	bOutputReconstructed=%d\n", data->bOutputReconstructed);
    printf("\t	b8DisableBitStuffing=%d\n", data->b8DisableBitStuffing);
    printf("\t	b8InsertHRDparams=%d\n", data->b8InsertHRDparams);
    printf("\t	ui8MaxSlicesPerPicture=%d\n", data->ui8MaxSlicesPerPicture);
    printf("\t	ui8NumPipes=%d\n", data->ui8NumPipes);
    printf("\t	bCARC=%d\n", data->bCARC);
    printf("\t	iCARCBaseline=%d\n", data->iCARCBaseline);
    printf("\t	uCARCThreshold=%d\n", data->uCARCThreshold);
    printf("\t	uCARCCutoff=%d\n", data->uCARCCutoff);
    printf("\t	uCARCNegRange=%d\n", data->uCARCNegRange);
    printf("\t	uCARCNegScale=%d\n", data->uCARCNegScale);
    printf("\t	uCARCPosRange=%d\n", data->uCARCPosRange);
    printf("\t	uCARCPosScale=%d\n", data->uCARCPosScale);
    printf("\t	uCARCShift=%d\n", data->uCARCShift);
    printf("\t	ui32MVClip_Config=%d\n", data->ui32MVClip_Config);
    printf("\t	ui32PredCombControl=%d\n", data->ui32PredCombControl);
    printf("\t	ui32LRITC_Tile_Use_Config=%d\n", data->ui32LRITC_Tile_Use_Config);
    printf("\t	ui32LRITC_Cache_Chunk_Config=%d\n", data->ui32LRITC_Cache_Chunk_Config);
    printf("\t	ui32IPEVectorClipping=%d\n", data->ui32IPEVectorClipping);
    printf("\t	ui32H264CompControl=%d\n", data->ui32H264CompControl);
    printf("\t	ui32H264CompIntraPredModes=%d\n", data->ui32H264CompIntraPredModes);
    printf("\t	ui32IPCM_0_Config=%d\n", data->ui32IPCM_0_Config);
    printf("\t	ui32IPCM_1_Config=%d\n", data->ui32IPCM_1_Config);
    printf("\t	ui32SPEMvdClipRange=%d\n", data->ui32SPEMvdClipRange);
    printf("\t	ui32JMCompControl=%d\n", data->ui32JMCompControl);
    printf("\t	ui32MBHostCtrl=%d\n", data->ui32MBHostCtrl);
    printf("\t	ui32DeblockCtrl=%d\n", data->ui32DeblockCtrl);
    
    printf("\t	ui32SkipCodedInterIntra=%d\n", data->ui32SkipCodedInterIntra);
    printf("\t	ui32VLCControl=%d\n", data->ui32VLCControl);
    printf("\t	ui32VLCSliceControl=%d\n", data->ui32VLCSliceControl);
    printf("\t	ui32VLCSliceMBControl=%d\n", data->ui32VLCSliceMBControl);
    printf("\t	ui16CQPOffset=%d\n", data->ui16CQPOffset);
    printf("\t	b8CodedHeaderPerSlice=%d\n", data->b8CodedHeaderPerSlice);
    printf("\t	ui32FirstPicFlags=%d\n", data->ui32FirstPicFlags);
    printf("\t	ui32NonFirstPicFlags=%d\n", data->ui32NonFirstPicFlags);

#ifndef EXCLUDE_ADAPTIVE_ROUNDING
    printf("\t	bMCAdaptiveRoundingDisable=%d\n",data->bMCAdaptiveRoundingDisable);
    int j;
    printf("\t	ui16MCAdaptiveRoundingOffsets[18][4]");
    printf("\n");
    for(i=0;i<18;i++){
        for(j=0;j<4;j++)
            printf("\t\t\t0x%x", data-> ui16MCAdaptiveRoundingOffsets[i][j]);
        printf("\n");
    }
#endif

#ifdef FORCED_REFERENCE
    printf("\t	ui32PatchedReconAddress=0x%x\n", data->ui32PatchedReconAddress);
    printf("\t	ui32PatchedRef0Address=0x%x\n", data->ui32PatchedRef0Address);
    printf("\t	ui32PatchedRef1Address=0x%x\n", data->ui32PatchedRef1Address);
#endif
#ifdef LTREFHEADER
    printf("\t	aui32LTRefHeader  {");
    PRINT_ARRAY_ADDR("aui32LTRefHeader",aui32LTRefHeader, MAX_SOURCE_SLOTS_SL);
    printf("\t	i8SliceHeaderSlotNum=%d\n",data->i8SliceHeaderSlotNum);
#endif
    printf("\t	b8ReconIsLongTerm=%d\n", data->b8ReconIsLongTerm);
    printf("\t	b8Ref0IsLongTerm=%d\n", data->b8Ref0IsLongTerm);
    printf("\t	b8Ref1IsLongTerm=%d\n", data->b8Ref1IsLongTerm);
    printf("\t	ui8RefSpacing=0x%x\n", data->ui8RefSpacing);

    printf("\t	ui8FirstPipe=0x%x\n", data->ui8FirstPipe);
    printf("\t	ui8LastPipe=0x%x\n", data->ui8LastPipe);
    printf("\t	ui8PipesToUseFlags=0x%x\n", data->ui8PipesToUseFlags);
    
    printf("\t	sInParams {\n");
    printf("\t\t	ui16MBPerFrm=%d\n",data->sInParams.ui16MBPerFrm);
    printf("\t\t	ui16MBPerBU=%d\n", data->sInParams.ui16MBPerBU);
    printf("\t\t	ui16BUPerFrm=%d\n",data->sInParams.ui16BUPerFrm);
    printf("\t\t	ui16IntraPerio=%d\n",data->sInParams.ui16IntraPeriod);
    printf("\t\t	ui16BFrames=%d\n", data->sInParams.ui16BFrames);
    printf("\t\t	bHierarchicalMode=%d\n",data->sInParams.bHierarchicalMode);
    printf("\t\t	i32BitsPerFrm=%d\n",   data->sInParams.i32BitsPerFrm);
    printf("\t\t	i32BitsPerBU=%d\n",    data->sInParams.i32BitsPerBU);
    printf("\t\t	i32BitsPerMB=%d\n",    data->sInParams.i32BitsPerMB);
    printf("\t\t	i32BitRate=%d\n",data->sInParams.i32BitRate);
    printf("\t\t	i32BufferSiz=%d\n",data->sInParams.i32BufferSize );
    printf("\t\t	i32InitialLevel=%d\n", data->sInParams.i32InitialLevel);
    printf("\t\t	i32InitialDelay=%d\n", data->sInParams.i32InitialDelay);
    printf("\t\t	i32BitsPerGOP=%d\n",   data->sInParams.i32BitsPerGOP);
    printf("\t\t	ui16AvQPVal=%d\n", data->sInParams.ui16AvQPVal);
    printf("\t\t	ui16MyInitQP=%d\n",data->sInParams.ui16MyInitQP);
    printf("\t\t	ui32RCScaleFactor=%d\n",data->sInParams.ui32RCScaleFactor);
    printf("\t\t	bScDetectDis;=%d\n", data->sInParams.bScDetectDisable);
    printf("\t\t	bFrmSkipDisable=%d\n",data->sInParams.bFrmSkipDisable);
    printf("\t\t	bBUSkipDisable=%d\n",data->sInParams.bBUSkipDisable);
    printf("\t\t	bRCIsH264VBR=%d\n",data->sInParams.bRCIsH264VBR);
    printf("\t\t	ui8SeInitQP=%d\n",    data->sInParams.ui8SeInitQP	);
    printf("\t\t	ui8MinQPVal=%d\n",    data->sInParams.ui8MinQPVal	);
    printf("\t\t	ui8MaxQPVal=%d\n",    data->sInParams.ui8MaxQPVal	);
    printf("\t\t	ui8MBPerRow=%d\n",    data->sInParams.ui8MBPerRow	);
    printf("\t\t	ui8ScaleFactor=%d\n",  data->sInParams.ui8ScaleFactor);
    printf("\t\t	ui8HalfFrame=%d\n",    data->sInParams.ui8HalfFrameRate);
    printf("\t\t	ui8FCode=%d\n",        data->sInParams.ui8FCode);
    printf("\t\t	ui8VCMBitrateMargin=%d\n", data->sInParams.ui8VCMBitrateMargin);
    printf("\t\t	i32ForceSkipin=%d\n",data->sInParams.i32ForceSkipMargin);
    printf("\t\t	i32TransferRate=%d\n",data->sInParams.i32TransferRate);
    
    printf("\t\t}\n");
}


struct header_token {
    int token;
    char *str;
} header_tokens[] = {
    {ELEMENT_STARTCODE_RAWDATA,"ELEMENT_STARTCODE_RAWDATA=0"},
    {ELEMENT_STARTCODE_MIDHDR,"ELEMENT_STARTCODE_MIDHDR"},
    {ELEMENT_RAWDATA,"ELEMENT_RAWDATA"},
    {ELEMENT_QP,"ELEMENT_QP"},
    {ELEMENT_SQP,"ELEMENT_SQP"},
    {ELEMENT_FRAMEQSCALE,"ELEMENT_FRAMEQSCALE"},
    {ELEMENT_SLICEQSCALE,"ELEMENT_SLICEQSCALE"},
    {ELEMENT_INSERTBYTEALIGN_H264,"ELEMENT_INSERTBYTEALIGN_H264"},
    {ELEMENT_INSERTBYTEALIGN_MPG4,"ELEMENT_INSERTBYTEALIGN_MPG4"},
    {ELEMENT_INSERTBYTEALIGN_MPG2,"ELEMENT_INSERTBYTEALIGN_MPG2"},
    {ELEMENT_VBV_MPG2,"ELEMENT_VBV_MPG2"},
    {ELEMENT_TEMPORAL_REF_MPG2,"ELEMENT_TEMPORAL_REF_MPG2"},
    {ELEMENT_CURRMBNR,"ELEMENT_CURRMBNR"},
    {ELEMENT_FRAME_NUM,"ELEMENT_FRAME_NUM"},
    {ELEMENT_TEMPORAL_REFERENCE,"ELEMENT_TEMPORAL_REFERENCE"},
    {ELEMENT_EXTENDED_TR,"ELEMENT_EXTENDED_TR"},
    {ELEMENT_IDR_PIC_ID,"ELEMENT_IDR_PIC_ID"},
    {ELEMENT_PIC_ORDER_CNT,"ELEMENT_PIC_ORDER_CNT"},
    {ELEMENT_GOB_FRAME_ID,"ELEMENT_GOB_FRAME_ID"},
    {ELEMENT_VOP_TIME_INCREMENT,"ELEMENT_VOP_TIME_INCREMENT"},
    {ELEMENT_MODULO_TIME_BASE,"ELEMENT_MODULO_TIME_BASE"},
    {ELEMENT_BOTTOM_FIELD,"ELEMENT_BOTTOM_FIELD"},
    {ELEMENT_SLICE_NUM,"ELEMENT_SLICE_NUM"},
    {ELEMENT_MPEG2_SLICE_VERTICAL_POS,"ELEMENT_MPEG2_SLICE_VERTICAL_POS"},
    {ELEMENT_MPEG2_IS_INTRA_SLICE,"ELEMENT_MPEG2_IS_INTRA_SLICE"},
    {ELEMENT_MPEG2_PICTURE_STRUCTURE,"ELEMENT_MPEG2_PICTURE_STRUCTURE"},
    {ELEMENT_REFERENCE,"ELEMENT_REFERENCE"},
    {ELEMENT_ADAPTIVE,"ELEMENT_ADAPTIVE"},
    {ELEMENT_DIRECT_SPATIAL_MV_FLAG,"ELEMENT_DIRECT_SPATIAL_MV_FLAG"},
    {ELEMENT_NUM_REF_IDX_ACTIVE,"ELEMENT_NUM_REF_IDX_ACTIVE"},
    {ELEMENT_REORDER_L0,"ELEMENT_REORDER_L0"},
    {ELEMENT_REORDER_L1,"ELEMENT_REORDER_L1"},
    {ELEMENT_TEMPORAL_ID,"ELEMENT_TEMPORAL_ID"},
    {ELEMENT_ANCHOR_PIC_FLAG,"ELEMENT_ANCHOR_PIC_FLAG"},
    {BPH_SEI_NAL_INITIAL_CPB_REMOVAL_DELAY,"BPH_SEI_NAL_INITIAL_CPB_REMOVAL_DELAY"},
    {BPH_SEI_NAL_INITIAL_CPB_REMOVAL_DELAY_OFFSET,"BPH_SEI_NAL_INITIAL_CPB_REMOVAL_DELAY_OFFSET"},
    {PTH_SEI_NAL_CPB_REMOVAL_DELAY,"PTH_SEI_NAL_CPB_REMOVAL_DELAY"},
    {PTH_SEI_NAL_DPB_OUTPUT_DELAY,"PTH_SEI_NAL_DPB_OUTPUT_DELAY"},
    {ELEMENT_WEIGHTED_PRED_AND_BIPRED_IDC_FLAGS,"ELEMENT_WEIGHTED_PRED_AND_BIPRED_IDC_FLAGS"},
    {ELEMENT_SLICEWEIGHTEDPREDICTIONSTRUCT,"ELEMENT_SLICEWEIGHTEDPREDICTIONSTRUCT"},
    {ELEMENT_CUSTOM_QUANT,"ELEMENT_CUSTOM_QUANT"}
};

static char *header_to_str(int token)
{
    int i;
    struct header_token *p;
    
    for (i=0; i<sizeof(header_tokens)/sizeof(struct header_token); i++) {
        p = &header_tokens[i];
        if (p->token == token)
            return p->str;
    }

    return "Invalid header token";
}

static int MTX_HEADER_PARAMS_dump(MTX_HEADER_PARAMS *p)
{
    MTX_HEADER_ELEMENT *last_element=NULL;
    int i;
    
    printf("\t\tui32Elements=%d\n", p->ui32Elements);
    for (i=0; i<p->ui32Elements; i++) {
        MTX_HEADER_ELEMENT *q = &(p->asElementStream[0]);

        if (last_element) {
            int ui8Offset = 0;
            IMG_UINT8 *ui8P;
            
            if (last_element->Element_Type==ELEMENT_STARTCODE_RAWDATA ||
                last_element->Element_Type==ELEMENT_RAWDATA ||
                last_element->Element_Type==ELEMENT_STARTCODE_MIDHDR)
            {
                //Add a new element aligned to word boundary
                //Find RAWBit size in bytes (rounded to word boundary))
                ui8Offset=last_element->ui8Size+8+31; // NumberofRawbits (excluding size of bit count field)+ size of the bitcount field
                ui8Offset/=32; //Now contains rawbits size in words
                ui8Offset+=1; //Now contains rawbits+element_type size in words
                ui8Offset*=4; //Convert to number of bytes (total size of structure in bytes, aligned to word boundary).
            }
            else
            {
                ui8Offset=4;
            }
            ui8P=(IMG_UINT8 *) last_element;
            ui8P+=ui8Offset;
            q=(MTX_HEADER_ELEMENT *) ui8P;
        }
        
        printf("\t\t----Head %d----\n",i);
        printf("\t\t\tElement_Type=%d(0x%x:%s)\n",
               q->Element_Type, q->Element_Type, header_to_str(q->Element_Type));

        if (q->Element_Type==ELEMENT_STARTCODE_RAWDATA ||
            q->Element_Type==ELEMENT_RAWDATA ||
            q->Element_Type==ELEMENT_STARTCODE_MIDHDR) {
            int i, ui8Offset = 0;
            IMG_UINT8 *ui8P;
            
            printf("\t\t\tui8Size=%d(0x%x)\n", q->ui8Size, q->ui8Size);
            printf("\t\t\t(====aui8Bits===)");
            
            //Find RAWBit size in bytes (rounded to word boundary))
            ui8Offset=q->ui8Size+8+31; // NumberofRawbits (excluding size of bit count field)+ size of the bitcount field
            ui8Offset/=32; //Now contains rawbits size in words
            //ui8Offset+=1; //Now contains rawbits+element_type size in words
            ui8Offset*=4; //Convert to number of bytes (total size of structure in bytes, aligned to word boundar
            
            ui8P = &q->aui8Bits;
            for (i=0; i<ui8Offset; i++) {
                if ((i%8) == 0)
                    printf("\n\t\t\t");
                printf("0x%02x\t", *ui8P);
                ui8P++;
            }
            printf("\n");
        } else {
            printf("\t\t\t(no ui8Size/aui8Bits for this type header)\n");
        }
        
        last_element = q;
    }

    return 0;
}

int DO_HEADER_dump(MTX_HEADER_PARAMS *data)
{
    MTX_HEADER_PARAMS *p = data;
    unsigned char *q=(unsigned char *)data;

    printf("\t\t(===RawBits===)");
    PRINT_ARRAY_BYTE(q, 128);

    MTX_HEADER_PARAMS_dump(p);
        
    printf("\n\n");
    
    return 0;
}

static char *eBufferType2str(IMG_BUFFER_TYPE tmp)
{
    switch (tmp) {
    case IMG_BUFFER_SOURCE:
        return "IMG_BUFFER_SOURCE";
    case IMG_BUFFER_REF0:
        return "IMG_BUFFER_REF0";
    case IMG_BUFFER_REF1:
        return "IMG_BUFFER_REF1";
    case IMG_BUFFER_RECON:
        return "IMG_BUFFER_RECON";
    case IMG_BUFFER_CODED:
        return "IMG_BUFFER_CODED";
    default:
        return "Unknown Buffer Type";
    }
}

int PROVIDEBUFFER_dump(IMG_BUFFER_PARAMS *data)
{
    IMG_BUFFER_PARAMS *p = data;
    //IMG_BUFFER_DATA *bufdata = p->sData;
    printf("\t\teBufferType=%d(%s)\n", p->eBufferType, eBufferType2str(p->eBufferType));
    printf("\t\tui32PhysAddr=0x%08x\n", p->ui32PhysAddr);
    printf("\t\t(======sData=======)\n");
    if (p->eBufferType == IMG_BUFFER_SOURCE) { /* source */
        IMG_BUFFER_SOURCE_DATA *source = &p->sData.sSource;

        printf("\t\tui32PhysAddrYPlane_Field0=0x%x\n",source->ui32PhysAddrYPlane_Field0);
        printf("\t\tui32PhysAddrUPlane_Field0=0x%x\n",source->ui32PhysAddrUPlane_Field0);
        printf("\t\tui32PhysAddrVPlane_Field0=0x%x\n",source->ui32PhysAddrVPlane_Field0);
        printf("\t\tui32PhysAddrYPlane_Field1=0x%x\n",source->ui32PhysAddrYPlane_Field1);
        printf("\t\tui32PhysAddrUPlane_Field1=0x%x\n",source->ui32PhysAddrUPlane_Field1);
        printf("\t\tui32PhysAddrVPlane_Field1=0x%x\n",source->ui32PhysAddrVPlane_Field1);
        printf("\t\tui32HostContext=%d\n",source->ui32HostContext);
        printf("\t\tui8DisplayOrderNum=%d\n",source->ui8DisplayOrderNum);
        printf("\t\tui8SlotNum=%d\n",source->ui8SlotNum);
    } else if (p->eBufferType == IMG_BUFFER_REF0 || p->eBufferType == IMG_BUFFER_REF1) {
        //IMG_BUFFER_REF_DATA *sRef = bufdata->sRef;
    } else if (p->eBufferType == IMG_BUFFER_RECON) {
    } else if (p->eBufferType == IMG_BUFFER_CODED) {
        IMG_BUFFER_CODED_DATA *sCoded = &p->sData.sCoded;
        printf("\t\tui32Size=%d\n", sCoded->ui32Size);
        printf("\t\tui8SlotNum=%d\n", sCoded->ui8SlotNum);
    }

    printf("\n\n");
    
    return 0;
}

static char *eSubtype2str(IMG_PICMGMT_TYPE eSubtype)
{
    switch (eSubtype) {
    case IMG_PICMGMT_REF_TYPE:return "IMG_PICMGMT_REF_TYPE";
    case IMG_PICMGMT_GOP_STRUCT:return "IMG_PICMGMT_GOP_STRUCT";
    case IMG_PICMGMT_SKIP_FRAME:return "IMG_PICMGMT_SKIP_FRAME";
    case IMG_PICMGMT_EOS:return "IMG_PICMGMT_EOS";
    case IMG_PICMGMT_RC_UPDATE:return "IMG_PICMGMT_RC_UPDATE";
    case IMG_PICMGMT_FLUSH:return "IMG_PICMGMT_FLUSH";
    case IMG_PICMGMT_QUANT:return "IMG_PICMGMT_QUANT";
        
    default: return "Unknow";
    }
}

int PICMGMT_dump(IMG_PICMGMT_PARAMS *data)
{
    IMG_PICMGMT_PARAMS *p = data;

    printf("\t\teSubtype=%d(%s)\n", p->eSubtype, eSubtype2str(p->eSubtype));
    printf("\t\t(=====(additional data)=====\n");
    if (p->eSubtype == IMG_PICMGMT_REF_TYPE) {
        IMG_PICMGMT_REF_DATA *q = &p->sRefType;
        switch (q->eFrameType) {
        case IMG_INTRA_IDR:
            printf("\t\teFrameType=IMG_INTRA_IDR\n");
            break;
        case IMG_INTRA_FRAME:
            printf("\t\teFrameType=IMG_INTRA_FRAME\n");
            break;
        case IMG_INTER_P:
            printf("\t\teFrameType=IMG_INTER_P\n");
            break;
        case IMG_INTER_B:
            printf("\t\teFrameType=IMG_INTER_B\n");
            break;
        }
    } else if (p->eSubtype == IMG_PICMGMT_GOP_STRUCT) {
    } else if (p->eSubtype ==  IMG_PICMGMT_SKIP_FRAME) {
    } else if (p->eSubtype == IMG_PICMGMT_EOS) {
        IMG_PICMGMT_EOS_DATA *q = &p->sEosParams;
        
        printf("\t\tui32FrameCount=%d\n", q->ui32FrameCount);
    } else if (p->eSubtype == IMG_PICMGMT_RC_UPDATE) {
    } else if (p->eSubtype == IMG_PICMGMT_FLUSH) {
    } else if (p->eSubtype == IMG_PICMGMT_QUANT) {
    }
    
    printf("\n\n");

    return 0;
}


static char * cmd2str(int cmdid)
{
    switch (cmdid) {
    case MTX_CMDID_NULL:return "MTX_CMDID_NULL";
    case MTX_CMDID_SHUTDOWN:return "MTX_CMDID_SHUTDOWN"; 
	// Video Commands
    case MTX_CMDID_DO_HEADER:return "MTX_CMDID_DO_HEADER";
    case MTX_CMDID_ENCODE_FRAME:return "MTX_CMDID_ENCODE_FRAME";
    case MTX_CMDID_SETVIDEO:return "MTX_CMDID_SETVIDEO";        
    case MTX_CMDID_GETVIDEO:return "MTX_CMDID_GETVIDEO";        
    case MTX_CMDID_PICMGMT:return "MTX_CMDID_PICMGMT";          
    case MTX_CMDID_PROVIDE_BUFFER:return "MTX_CMDID_PROVIDE_BUFFER";
    case MTX_CMDID_ABORT:return "MTX_CMDID_ABORT";
	// JPEG commands
    case MTX_CMDID_SETQUANT:return "MTX_CMDID_SETQUANT";         
    case MTX_CMDID_SETUP_INTERFACE:return "MTX_CMDID_SETUP_INTERFACE"; 
    case MTX_CMDID_ISSUEBUFF:return "MTX_CMDID_ISSUEBUFF";       
    case MTX_CMDID_SETUP:return "MTX_CMDID_SETUP";           
    case MTX_CMDID_ENDMARKER:
    default:
        return "Invalid Command (%d)";
    }
}

int command_parameter_dump(int cmdid, void *virt_addr)
{
    MTX_HEADER_PARAMS *header_para;
    IMG_MTX_VIDEO_CONTEXT *context;
    IMG_PICMGMT_PARAMS *picmgmt_para;
    IMG_BUFFER_PARAMS *buffer_para ;
    JPEG_MTX_QUANT_TABLE *jpeg_table;
    MTX_ISSUE_BUFFERS *issue_buffer;
    JPEG_MTX_DMA_SETUP *jpeg_mtx_dma_setup;

    switch (cmdid) {
        case MTX_CMDID_NULL:
        case MTX_CMDID_SHUTDOWN:
        case MTX_CMDID_ENDMARKER :         //!< end marker for enum
        case MTX_CMDID_GETVIDEO:
            break;
        case MTX_CMDID_DO_HEADER:
            header_para = (MTX_HEADER_PARAMS *)virt_addr;
            DO_HEADER_dump(header_para);
            if (duplicate_setvideo_dump)
                SETVIDEO_dump(mtx_ctx);
            break;
        case MTX_CMDID_ENCODE_FRAME:
            if (duplicate_setvideo_dump)
                SETVIDEO_dump(mtx_ctx);
            if (last_setvideo_dump == 1)
                SETVIDEO_dump(mtx_ctx);
            break;
        case MTX_CMDID_SETVIDEO:
            context = (IMG_MTX_VIDEO_CONTEXT *)virt_addr;
            if (last_setvideo_dump == 1)
                mtx_ctx = virt_addr;
            else
                SETVIDEO_dump(context);
            break;
        case MTX_CMDID_PICMGMT :            
            picmgmt_para = (IMG_PICMGMT_PARAMS *)virt_addr;
            PICMGMT_dump(picmgmt_para);

            if (duplicate_setvideo_dump)
                SETVIDEO_dump(mtx_ctx);
            break;
        case  MTX_CMDID_PROVIDE_BUFFER:	 //!< (data: #IMG_BUFFER_PARAMS)\n Transfer buffer b\w Host and Topaz\n
            buffer_para = (IMG_BUFFER_PARAMS *)virt_addr;
            PROVIDEBUFFER_dump(buffer_para);

            if (duplicate_setvideo_dump)
                SETVIDEO_dump(mtx_ctx);
            break;

            // JPEG commands
        case      MTX_CMDID_SETQUANT:          //!< (data: #JPEG_MTX_QUANT_TABLE)\n
            jpeg_table = (JPEG_MTX_QUANT_TABLE *)virt_addr;
            JPEG_TABLE_dump(jpeg_table);
            break;
        case      MTX_CMDID_ISSUEBUFF:         //!< (data: #MTX_ISSUE_BUFFERS)\n
            issue_buffer = (MTX_ISSUE_BUFFERS *)virt_addr;
            ISSUE_BUFFER_dump(issue_buffer);
            break;
        case      MTX_CMDID_SETUP:             //!< (data: #JPEG_MTX_DMA_SETUP)\n\n
            jpeg_mtx_dma_setup = (JPEG_MTX_DMA_SETUP *)virt_addr;
            JPEG_MTX_DMA_dump(jpeg_mtx_dma_setup);
            break;
        default:
            break;
    }
    return 0;
}


int topazhp_dump_command(unsigned int *comm_dword)
{
    int cmdid;

    if (comm_dword == NULL)
        return 1;
    
    cmdid = (comm_dword[0] & MASK_MTX_CMDWORD_ID) & ~MTX_CMDID_PRIORITY;

    (void)multicore_regs;
    (void)core_regs;
    (void)mtx_regs;
    (void)dmac_regs;
    
    printf("\tSend command to MTX\n");
    printf("\t\tCMDWORD_ID=%s(High priority:%s)\n", cmd2str(cmdid),
           (comm_dword[0] & MTX_CMDID_PRIORITY)?"Yes":"No");
    printf("\t\tCMDWORD_CORE=%d\n", (comm_dword[0] & MASK_MTX_CMDWORD_CORE) >> SHIFT_MTX_CMDWORD_CORE);
    printf("\t\tCMDWORD_COUNT=%d\n", (comm_dword[0] & MASK_MTX_CMDWORD_COUNT) >> SHIFT_MTX_CMDWORD_COUNT);
    printf("\t\tCMDWORD_PARAM=0x%08x\n", comm_dword[1]);
    printf("\t\tCMDWORD_WBADDR=0x%08x\n", comm_dword[2]);
    printf("\t\tCMDWORD_WBVALUE=0x%08x\n", comm_dword[3]);
    if (comm_dword[1] != NULL)
	command_parameter_dump(cmdid, phy2virt(comm_dword[1]));
    else
	command_parameter_dump(cmdid, NULL);

    return 0;
}

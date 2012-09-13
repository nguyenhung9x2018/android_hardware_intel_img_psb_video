#ifndef _topazhp_cmdump_h
#define _topazhp_cmdump_h

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h> /* for libc5 */

#ifdef ANDROID
#define  outl(...)
#define  outw(...)
#define  inl(...)   0
#define  inw(...)   0
#else
#include <sys/io.h> /* for glibc */
#endif

#define MIN(a,b)  ((a)>(b)?(b):(a))

struct RegisterInfomation {
    char *name;
    int offset;
};

#define ui32TopazMulticoreRegId  1

static struct RegisterInfomation multicore_regs[] = {
    {"MULTICORE_SRST",0x0000},
    {"MULTICORE_INT_STAT",0x0004},
    {"MULTICORE_MTX_INT_ENAB",0x0008},
    {"MULTICORE_HOST_INT_ENAB",0x000C},
    {"MULTICORE_INT_CLEAR",0x0010},
    {"MULTICORE_MAN_CLK_GATE",0x0014},
    {"TOPAZ_MTX_C_RATIO",0x0018},
    {"MMU_STATUS",0x001C},
    {"MMU_MEM_REQ",0x0020},
    {"MMU_CONTROL0",0x0024},
    {"MMU_CONTROL1",0x0028},
    {"MMU_CONTROL2",0x002C},
    {"MMU_DIR_LIST_BASE",0x0030},
    {"MMU_TILE",0x0038},
    {"MTX_DEBUG_MSTR",0x0044},
    {"MTX_DEBUG_SLV",0x0048},
    {"MULTICORE_CORE_SEL_0",0x0050},
    {"MULTICORE_CORE_SEL_1",0x0054},
    {"MULTICORE_HW_CFG",0x0058},
    {"MULTICORE_CMD_FIFO_WRITE",0x0060},
    {"MULTICORE_CMD_FIFO_WRITE_SPACE",0x0064},
    {"TOPAZ_CMD_FIFO_READ",0x0070},
    {"TOPAZ_CMD_FIFO_READ_AVAILABLE",0x0074},
    {"TOPAZ_CMD_FIFO_FLUSH",0x0078},
    {"MMU_TILE_EXT",0x0080},
    {"FIRMWARE_REG_1",0x0100},
    {"FIRMWARE_REG_2",0x0104},
    {"FIRMWARE_REG_3",0x0108},
    {"CYCLE_COUNTER",0x0110},
    {"CYCLE_COUNTER_CTRL",0x0114},
    {"MULTICORE_IDLE_PWR_MAN",0x0118},
    {"DIRECT_BIAS_TABLE",0x0124},
    {"INTRA_BIAS_TABLE",0x0128},
    {"INTER_BIAS_TABLE",0x012C},
    {"INTRA_SCALE_TABLE",0x0130},
    {"QPCB_QPCR_OFFSET",0x0134},
    {"INTER_INTRA_SCALE_TABLE",0x0140},
    {"SKIPPED_CODED_SCALE_TABLE",0x0144},
    {"POLYNOM_ALPHA_COEFF_CORE0",0x0148},
    {"POLYNOM_GAMMA_COEFF_CORE0",0x014C},
    {"POLYNOM_CUTOFF_CORE0",0x0150},
    {"POLYNOM_ALPHA_COEFF_CORE1",0x0154},
    {"POLYNOM_GAMMA_COEFF_CORE1",0x0158},
    {"POLYNOM_CUTOFF_CORE1",0x015C},
    {"POLYNOM_ALPHA_COEFF_CORE2",0x0160},
    {"POLYNOM_GAMMA_COEFF_CORE2",0x0164},
    {"POLYNOM_CUTOFF_CORE2",0x0168},
    {"FIRMWARE_REG_4",0x0300},
    {"FIRMWARE_REG_5",0x0304},
    {"FIRMWARE_REG_6",0x0308},
    {"FIRMWARE_REG_7",0x030C},
    {"MULTICORE_RSVD0",0x03B0},
    {"TOPAZHP_CORE_ID",0x03C0},
    {"TOPAZHP_CORE_REV",0x03D0},
    {"TOPAZHP_CORE_DES1",0x03E0},
    {"TOPAZHP_CORE_DES2",0x03F0},
};


static struct RegisterInfomation core_regs[] = {
    {"TOPAZHP_SRST",0x0000},
    {"TOPAZHP_INTSTAT",0x0004},
    {"TOPAZHP_MTX_INTENAB",0x0008},
    {"TOPAZHP_HOST_INTENAB",0x000C},
    {"TOPAZHP_INTCLEAR",0x0010},
    {"TOPAZHP_INT_COMB_SEL",0x0014},
    {"TOPAZHP_BUSY",0x0018},
    {"TOPAZHP_AUTO_CLOCK_GATING",0x0024},
    {"TOPAZHP_MAN_CLOCK_GATING",0x0028},
    {"TOPAZHP_RTM",0x0030},
    {"TOPAZHP_RTM_VALUE",0x0034},
    {"TOPAZHP_MB_PERFORMANCE_RESULT",0x0038},
    {"TOPAZHP_MB_PERFORMANCE_MB_NUMBER",0x003C},
    {"FIELD_PARITY",0x0188},
    {"WEIGHTED_PRED_CONTROL",0x03D0},
    {"WEIGHTED_PRED_COEFFS",0x03D4},
    {"WEIGHTED_PRED_INV_WEIGHT",0x03E0},
    {"TOPAZHP_RSVD0",0x03F0},
    {"TOPAZHP_CRC_CLEAR",0x03F4},
    {"SPE_ZERO_THRESH",0x0344},
    {"SPE0_BEST_SAD_SIGNATURE",0x0348},
    {"SPE1_BEST_SAD_SIGNATURE",0x034C},
    {"SPE0_BEST_INDEX_SIGNATURE",0x0350},
    {"SPE1_BEST_INDEX_SIGNATURE",0x0354},
    {"SPE_INTRA_COST_SIGNATURE",0x0358},
    {"SPE_MVD_CLIP_RANGE",0x0360},
    {"SPE_SUBPEL_RESOLUTION",0x0364},
    {"SPE0_MV_SIZE_SIGNATURE",0x0368},
    {"SPE1_MV_SIZE_SIGNATURE",0x036C},
    {"SPE_MB_PERFORMANCE_RESULT",0x0370},
    {"SPE_MB_PERFORMANCE_MB_NUMBER",0x0374},
    {"SPE_MB_PERFORMANCE_CLEAR",0x0378},
    {"MEM_SIGNATURE_CONTROL",0x0060},
    {"MEM_SIGNATURE_ENC_WDATA",0x0064},
    {"MEM_SIGNATURE_ENC_RDATA",0x0068},
    {"MEM_SIGNATURE_ENC_ADDR",0x006C},
    {"PREFETCH_LRITC_SIGNATURE",0x0070},
    {"PROC_DMA_CONTROL",0x00E0},
    {"PROC_DMA_STATUS",0x00E4},
    {"PROC_ESB_ACCESS_CONTROL",0x00EC},
    {"PROC_ESB_ACCESS_WORD0",0x00F0},
    {"PROC_ESB_ACCESS_WORD1",0x00F4},
    {"PROC_ESB_ACCESS_WORD2",0x00F8},
    {"PROC_ESB_ACCESS_WORD3",0x00FC},

    {"LRITC_TILE_USE_CONFIG",0x0040},
    {"LRITC_TILE_USE_STATUS",0x0048},
    {"LRITC_TILE_FREE_STATUS",0x004C},
    {"LRITC_CACHE_CHUNK_CONFIG",0x0050},
    {"LRITC_CACHE_CHUNK_STATUS",0x0054},
    {"LRITC_SIGNATURE_ADDR",0x0058},
    {"LRITC_SIGNATURE_RDATA",0x005C},
    
    {"SEQ_CUR_PIC_LUMA_BASE_ADDR",0x0100},
    {"SEQ_CUR_PIC_CB_BASE_ADDR",0x0104},
    {"SEQ_CUR_PIC_CR_BASE_ADDR",0x0108},
    {"SEQ_CUR_PIC_ROW_STRIDE",0x010C},
    {"SEQ_REF_PIC0_LUMA_BASE_ADDR",0x0110},
    {"SEQ_REF_PIC0_CHROMA_BASE_ADDR",0x0114},
    {"SEQ_REF_PIC1_LUMA_BASE_ADDR",0x0118},
    {"SEQ_REF_PIC1_CHROMA_BASE_ADDR",0x011C},
    {"SEQ_CUR_PIC_CONFIG",0x0120},
    {"SEQ_CUR_PIC_SIZE",0x0124},
    {"SEQ_RECON_LUMA_BASE_ADDR",0x0128},
    {"SEQ_RECON_CHROMA_BASE_ADDR",0x012C},
    {"SEQ_ABOVE_PARAM_BASE_ADDR",0x0130},
    {"SEQ_TEMPORAL_COLOCATED_IN_ADDR",0x0134},
    {"SEQ_TEMPORAL_PIC0_MV_IN_ADDR",0x0138},
    {"SEQ_TEMPORAL_PIC1_MV_IN_ADDR",0x013C},
    {"SEQ_TEMPORAL_COLOCATED_OUT_ADDR",0x0140},
    {"SEQ_TEMPORAL_PIC0_MV_OUT_ADDR",0x0144},
    {"SEQ_TEMPORAL_PIC1_MV_OUT_ADDR",0x0148},
    {"SEQ_MB_FIRST_STAGE_OUT_ADDR",0x014C},
    {"SEQ_MB_CONTROL_IN_ADDR",0x0150},
    {"SEQUENCER_CONFIG",0x0154},
    {"SLICE_CONFIG",0x0158},
    {"SLICE_QP_CONFIG",0x015C},
    {"SEQUENCER_KICK",0x0160},
    {"H264COMP_REJECT_THRESHOLD",0x0184},
    {"H264COMP_CUSTOM_QUANT_SP",0x01A0},
    {"H264COMP_CUSTOM_QUANT_Q",0x01A4},
    {"H264COMP_CONTROL",0x01A8},
    {"H264COMP_INTRA_PRED_MODES",0x01AC},
    {"H264COMP_MAX_CYCLE_COUNT",0x01B0},
    {"H264COMP_MAX_CYCLE_MB",0x01B4},
    {"H264COMP_MAX_CYCLE_RESET",0x01B8},
    {"H264COMP4X4_PRED_CRC",0x01BC},
    {"H264COMP4X4_COEFFS_CRC",0x01C0},
    {"H264COMP4X4_RECON_CRC",0x01C4},
    {"H264COMP8X8_PRED_CRC",0x01C8},
    {"H264COMP8X8_COEFFS_CRC",0x01CC},
    {"H264COMP8X8_RECON_CRC",0x01D0},
    {"H264COMP16X16_PRED_CRC",0x01D4},
    {"H264COMP16X16_COEFFS_CRC",0x01D8},
    {"H264COMP16X16_RECON_CRC",0x01DC},
    {"H264COMP_ROUND_0",0x01E0},
    {"H264COMP_ROUND_1",0x01E4},
    {"H264COMP_ROUND_2",0x01E8},
    {"H264COMP_ROUND_INIT",0x01EC},
    {"H264COMP_VIDEO_CONF_CONTROL_0",0x01F0},
    {"H264COMP_VIDEO_CONF_CONTROL_1",0x01F4},
    {"H264COMP_VIDEO_CONF_STATUS_0",0x01F8},
    {"H264COMP_VIDEO_CONF_STATUS_1",0x01FC},
};

static struct RegisterInfomation mtx_regs[] = {
    {"MTX_ENABLE",0x0000},
    {"MTX_STATUS",0x0008},
    {"MTX_KICK",0x0080},
    {"MTX_KICKI",0x0088},
    {"MTX_FAULT0",0x0090},
    {"MTX_REGISTER_READ_WRITE_DATA",0x00F8},
    {"MTX_REGISTER_READ_WRITE_REQUEST",0x00FC},
    {"MTX_RAM_ACCESS_DATA_EXCHANGE",0x0100},
    {"MTX_RAM_ACCESS_DATA_TRANSFER",0x0104},
    {"MTX_RAM_ACCESS_CONTROL",0x0108},
    {"MTX_RAM_ACCESS_STATUS",0x010C},
    {"MTX_SOFT_RESET",0x0200},
    {"MTX_SYSC_CDMAC",0x0340},
    {"MTX_SYSC_CDMAA",0x0344},
    {"MTX_SYSC_CDMAS0",0x0348},
    {"MTX_SYSC_CDMAS1",0x034C},
    {"MTX_SYSC_CDMAT",0x0350}
};


static struct RegisterInfomation dmac_regs[] = {
    {"DMA_Setup",0x0000},
    {"DMA_Count",0x0004},
    {"DMA_Peripheral_param",0x0008},
    {"DMA_IRQ_Stat",0x000c},
    {"DMA_2D_Mode",0x0010},
    {"DMA_Peripheral_addr",0x0014},
    {"DMA_Per_hold",0x0018},
    {"DMA_SoftReset",0x0020},
};

#define PCI_DEVICE_ID_CFG	0x02	/* 16 bits */
#define PCI_BASE_ADDRESS_0	0x10	/* 32 bits */
#define PCI_BASE_ADDRESS_1	0x14	/* 32 bits [htype 0,1 only] */
#define PCI_BASE_ADDRESS_2	0x18	/* 32 bits [htype 0 only] */
#define PCI_BASE_ADDRESS_3	0x1c	/* 32 bits */
#define CONFIG_CMD(bus,device_fn,where)   \
   (0x80000000|((bus&0xff) << 16)|((device_fn&0xff) << 8)|((where&0xff) & ~3))

static inline unsigned long pci_get_long(int bus,int device_fn, int where)
{
    outl(CONFIG_CMD(bus,device_fn,where), 0xCF8);
    return inl(0xCFC);
}

static inline int pci_set_long(int bus,int device_fn, int where,unsigned long value)
{
    outl(CONFIG_CMD(bus,device_fn,where), 0xCF8);
    outl(value,0xCFC);
    return 0;
}

static inline int pci_get_short(int bus,int device_fn, int where)
{
    outl(CONFIG_CMD(bus,device_fn,where), 0xCF8);
    return inw(0xCFC + (where&2));
}


static inline int pci_set_short(int bus,int device_fn, int where,unsigned short value)
{
    outl(CONFIG_CMD(bus,device_fn,where), 0xCF8);
    outw(value,0xCFC + (where&2));
    return 0;
}

#define REG_OFFSET_TOPAZ_MULTICORE                      0x00000000
#define REG_OFFSET_TOPAZ_DMAC                           0x00000400
#define REG_OFFSET_TOPAZ_MTX                            0x00000800

#define REGNUM_TOPAZ_CR_MMU_DIR_LIST_BASE_ADDR          0x0030

#ifndef MV_OFFSET_IN_TABLE
#define MV_OFFSET_IN_TABLE(BDistance, Position) ((BDistance) * MV_ROW_STRIDE + (Position) * sizeof(IMG_MV_SETTINGS))
#endif

#define MULTICORE_READ32(offset, pointer)                               \
    do {                                                                \
	*(pointer) = *((unsigned long *)((unsigned char *)(linear_mmio_topaz) \
                                         + REG_OFFSET_TOPAZ_MULTICORE + offset)); \
    } while (0)


int topazhp_dump_command(unsigned int comm_dword[]);
int ptg_command_parameter_dump(int cmdid, void *virt_addr);
    
#endif

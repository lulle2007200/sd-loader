/*
 * Copyright (c) 2019-2021 CTCaer
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef _MEMORY_MAP_H_
#define _MEMORY_MAP_H_

#if 0

#define IRAM_START  0x40000000

#define IPL_LOAD_ADDR             0x40000000 //60K

#define USB_EP_BULK_IN_BUF_ADDR   (IPL_LOAD_ADDR + MAX_PAYLOAD_SIZE) //64K
#define USB_EP_BULK_IN_MAX_XFER   SZ_64K  
#define USB_EP_BULK_OUT_BUF_ADDR  (USB_EP_BULK_IN_BUF_ADDR + SZ_64K) //64K
#define USB_EP_BULK_OUT_MAX_XFER  SZ_64K

#define IPL_SMALL_FB_SZ           (SZ_32K + SZ_16K + SZ_8K + SZ_4K)
#define IPL_SMALL_FB_ADDR         (USB_EP_BULK_OUT_BUF_ADDR + SZ_64K) //60K

#define XUSB_RING_ADDR            (IPL_SMALL_FB_ADDR + IPL_SMALL_FB_SZ) //1.5K

#define USB_EP_CONTROL_BUF_ADDR   (XUSB_RING_ADDR + SZ_1K + (SZ_1K / 2)) //1K

#define IPL_HEAP_START            (USB_EP_CONTROL_BUF_ADDR + SZ_1K)

#define IPL_STACK_TOP             0x40040000

#define SDMMC_UPPER_BUFFER        USB_EP_BULK_OUT_BUF_ADDR
#define SDMMC_UP_BUF_SZ           USB_EP_BULK_OUT_MAX_XFER

#define PAYLOAD_BUF_ADDR          0x40010000 // load payload directly to 0x40010000 -> max. payload size reduced by 4Kb, but don't need a relocator
#define PAYLOAD_MAX_SZ            (3 * SZ_64K - (4 * SZ_1K)) // leaves 4kb for stack

#if (IPL_HEAP_START + SZ_1K) > IPL_STACK_TOP
// #error payload too large
#endif 

#else

#define IRAM_START  0x40000000

#ifndef IPL_LOAD_ADDR
#define IPL_LOAD_ADDR             0x40003000 //64K max
#endif

#define IPL_SIZE_MAX              0x10000

#define USB_EP_BULK_IN_BUF_ADDR   (IPL_LOAD_ADDR + IPL_SIZE_MAX) //32K
#define USB_EP_BULK_IN_MAX_XFER   (SZ_64K / 2)
#define USB_EP_BULK_OUT_BUF_ADDR  (USB_EP_BULK_IN_BUF_ADDR + USB_EP_BULK_IN_MAX_XFER) //32K
#define USB_EP_BULK_OUT_MAX_XFER  (SZ_64K / 2)

#define XUSB_RING_ADDR            (USB_EP_BULK_OUT_BUF_ADDR + USB_EP_BULK_OUT_MAX_XFER) //1.5K
#define USB_EP_CONTROL_BUF_ADDR   (XUSB_RING_ADDR + SZ_1K + (SZ_1K / 2)) //1K


#define IPL_SMALL_FB_SZ           (SZ_32K + SZ_16K + SZ_8K + SZ_4K)

#define IPL_HEAP_SIZE_MAX         (SZ_1K)
#define IPL_STACK_SIZE_MAX        (SZ_1K * 8)
#define IPL_STACK_TOP             0x40040000

#define IPL_HEAP_START            (IPL_STACK_TOP - IPL_STACK_SIZE_MAX - IPL_HEAP_SIZE_MAX)

#define IPL_SMALL_FB_ADDR         (IPL_HEAP_START - IPL_SMALL_FB_SZ)

// load payload to buffer after ipl, will be relocated to 0x40010000 before jumping to it
#define PAYLOAD_BUF_ADDR          USB_EP_BULK_IN_BUF_ADDR
#define PAYLOAD_SIZE_MAX          (IPL_HEAP_START - PAYLOAD_BUF_ADDR)

#define PAYLOAD_SIZE_SAFE         (IPL_SMALL_FB_ADDR - PAYLOAD_BUF_ADDR)

#define PAYLOAD_LOAD_ADDR         0x40010000

#define SDMMC_UPPER_BUFFER        PAYLOAD_BUF_ADDR
#define SDMMC_UP_BUF_SZ           PAYLOAD_SIZE_SAFE


#if (PAYLOAD_SIZE_MAX) < 0x20000
#error Payload buffer too small
#endif

#if (PAYLOAD_SIZE_SAFE) < 0x10000
#error Payload buffer too small
#endif

#endif


#define DRAM_START                0x80000000

/* --- XUSB EP context and TRB ring buffers --- */

// #define SECMON_MIN_START  0x4002B000

// #define SDRAM_PARAMS_ADDR 0x40030000 // SDRAM extraction buffer during sdram init.
// #define CBFS_DRAM_EN_ADDR 0x4003e000 // u32.

// /* --- DRAM START --- */
// #define DRAM_START     0x80000000
// #define  HOS_RSVD          SZ_16M // Do not write anything in this area.

// #define NYX_LOAD_ADDR  0x81000000
// #define  NYX_SZ_MAX        SZ_16M
// /* --- Gap: 0x82000000 - 0x82FFFFFF --- */

// /* Stack theoretical max: 33MB */
// // #define IPL_STACK_TOP  0x83100000 
// // #define IPL_HEAP_START 0x84000000
// // #define  IPL_HEAP_SZ      SZ_512M
// /* --- Gap: 1040MB 0xA4000000 - 0xE4FFFFFF --- */

// // Virtual disk / Chainloader buffers.
// #define RAM_DISK_ADDR 0xA4000000
// #define  RAM_DISK_SZ  0x41000000 // 1040MB.
// #define  RAM_DISK2_SZ 0x21000000 //  528MB.

// // NX BIS driver sector cache.
// #define NX_BIS_CACHE_ADDR  0xC5000000
// #define  NX_BIS_CACHE_SZ   0x10020000 // 256MB.
// #define NX_BIS_LOOKUP_ADDR 0xD6000000
// #define  NX_BIS_LOOKUP_SZ   0xF000000 // 240MB.

// // L4T Kernel Panic Storage (PSTORE).
// #define PSTORE_ADDR   0xB0000000
// #define  PSTORE_SZ         SZ_2M

// //#define DRAM_LIB_ADDR    0xE0000000
// /* --- Chnldr: 252MB 0xC03C0000 - 0xCFFFFFFF --- */ //! Only used when chainloading.

// // SDMMC DMA buffers 1
// // #define SDMMC_UPPER_BUFFER 0xE5000000
// // #define  SDMMC_UP_BUF_SZ      SZ_128M

// // Nyx buffers.
// #define NYX_STORAGE_ADDR 0xED000000
// #define NYX_RES_ADDR     0xEE000000
// #define  NYX_RES_SZ          SZ_16M

// // SDMMC DMA buffers 2
// // #define SDXC_BUF_ALIGNED   0xEF000000
// // #define MIXD_BUF_ALIGNED   0xF0000000
// // #define EMMC_BUF_ALIGNED   MIXD_BUF_ALIGNED
// // #define  SDMMC_DMA_BUF_SZ      SZ_16M // 4MB currently used.

// // Nyx LvGL buffers.
// #define NYX_LV_VDB_ADR   0xF1000000
// #define  NYX_FB_SZ         0x384000 // 1280 x 720 x 4.
// #define NYX_LV_MEM_ADR   0xF1400000
// #define  NYX_LV_MEM_SZ    0x6600000 // 70MB.

// // Framebuffer addresses.
#define IPL_FB_ADDRESS   0xF5A00000
#define IPL_FB_SZ         0x384000 // 720 x 1280 x 4.
#define LOG_FB_ADDRESS   0xF5E00000
#define LOG_FB_SZ         0x334000 // 1280 x 656 x 4.
#define NYX_FB_ADDRESS   0xF6200000
#define NYX_FB2_ADDRESS  0xF6600000
#define NYX_FB_SZ         0x384000 // 1280 x 720 x 4.

// #define DRAM_MEM_HOLE_ADR 0xF6A00000
// #define DRAM_MEM_HOLE_SZ   0x8140000
// /* ---   Hole: 129MB 0xF6A00000 - 0xFEB3FFFF --- */
// #define DRAM_START2       0xFEB40000

// USB buffers.
// #define USBD_ADDR                 0xFEF00000
// #define USB_DESCRIPTOR_ADDR       0xFEF40000
// #define USB_EP_CONTROL_BUF_ADDR   0xFEF80000
// #define USB_EP_BULK_IN_BUF_ADDR   0xFF000000
// #define USB_EP_BULK_OUT_BUF_ADDR  0xFF800000
// #define  USB_EP_BULK_OUT_MAX_XFER      SZ_8M

// #define EXT_PAYLOAD_ADDR    0xC0000000
// #define RCM_PAYLOAD_ADDR    (EXT_PAYLOAD_ADDR + ALIGN(PATCHED_RELOC_SZ, 0x10))
// #define COREBOOT_ADDR       (0xD0000000 - rom_size)

#endif

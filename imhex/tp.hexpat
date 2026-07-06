// This is AI generated Code
// It's looks worse.....
// I think I need to rewrite this.....


// TP-Link Firmware Image Pattern for ImHex
// Based on reverse-engineered firmware format from tp-firmware-parse
//
// Image Layout:
//   [0x00] HeaderData          - Main image header (variable size, up to header_size)
//   [variable] FacBoot         - Factory boot partition (optional, 0x100000 bytes)
//   [variable] NormalBoot      - Normal boot partition (optional, 0xA0000 or 0x40000 bytes)
//   [variable] TpHeaderData    - Partition table header
//   [variable] Kernel + Rootfs - Remaining partitions (described by TpHeaderData)

#pragma endian big

// =============================================================================
// Constants
// =============================================================================

// Image Header Magic (20 bytes)
//   55 AA 4C 5E 83 1F 53 4B A1 F8 F7 C9 18 DF 8F BF 7D A1 AA 55
#define HEADER_MAGIC_BYTE2 0x4C
#define HEADER_MAGIC_BYTE3 0x5E

// TP Header Magic (20 bytes)
//   55 AA 9D D1 A8 C8 83 31 C9 69 FB BF BC F0 D4 32 70 C7 AA 55
#define TPHEADER_MAGIC_BYTE2 0x9D
#define TPHEADER_MAGIC_BYTE3 0xD1

// Partition size constants
#define PART_SIZE_FACBOOT      0x00100000
#define NORMAL_BOOT_SIZE_A     0x000A0000
#define NORMAL_BOOT_SIZE_B     0x00040000

// ContentType bit positions
#define CONTENT_HAS_NORMAL_BOOT  0
#define CONTENT_HAS_ROOTFS       1
#define CONTENT_HAS_FACBOOT      4

// =============================================================================
// Type Definitions
// =============================================================================

import std.io;
import std.mem;

/// Hardware/Firmware ID (16 bytes)
struct ID {
    u8 id[16];
};

/// Partition entry: absolute offset and size
struct PartInfoData {
    u32 offset;
    u32 size;
};

// =============================================================================
// Main Image Header
// =============================================================================

struct HeaderData {
    u16 resv[2];
    u8 magic_header[20];
    u16 header_size;
    u16 vender_id;
    u16 unknown0;
    u16 content_type;
    u8 image_rsa_sign[128];
    u16 hwid_count;
    u16 fwid_count;
    u16 fwid_map;
    u8 unknown1[10];

    // HWID entries (each 16 bytes, only if count > 0)
    if (hwid_count > 0)
        ID hwid[hwid_count] [[color("376AD0")]];

    // FWID entries (each 16 bytes, only if count > 0)
    if (fwid_count > 0)
        ID fwid[fwid_count] [[color("17BECF")]];

    // Padding to fill up to header_size (relative to start of resv)
    u8 pad[header_size - ($ - addressof(resv))];
};

// =============================================================================
// TP Header (Partition Table)
// =============================================================================

struct TpHeaderData {
    u16 resv[2];
    u8 magic_header[20];
    u8 unknown0[14];
    u16 part_count;

    // 9 partition entries describing the firmware layout
    PartInfoData facboot;
    PartInfoData factory_info;
    PartInfoData art;
    PartInfoData config;
    PartInfoData NormalBoot;
    PartInfoData tp_header;
    PartInfoData BootingKernel;
    PartInfoData Rootfs;
    PartInfoData RootfsData;
    u8 zero[32];
    u8 unknown1[48];

    char product_name[14];
    u16 support_fwid_count;

    // Supported FWID entries (each 16 bytes, only if count > 0)
    if (support_fwid_count > 0)
        ID fwid[support_fwid_count] [[color("17BECF")]];
};

// =============================================================================
// Main Parsing Logic
// =============================================================================

// ---- 1. Parse Image Header at offset 0x00 --------------------------------
HeaderData hd @ 0x0;

// ---- 2. Calculate content offset after header ----------------------------
u64 off = hd.header_size;

// ---- 3. FacBoot Partition (optional) ------------------------------------
// Present when content_type bit 4 (0x10) is set
if (hd.content_type & (1 << CONTENT_HAS_FACBOOT))
    off += PART_SIZE_FACBOOT;

// ---- 4. NormalBoot & TP Header Detection --------------------------------
// Normal boot size is dynamic (0xA0000 or 0x40000). We detect the correct
// size by checking the TP Header magic third byte (0x9D) at each candidate.
// The TP header is located at: off + normal_boot_size

u64 tp_offset = off;    // default: assume no normal boot

if (hd.content_type & (1 << CONTENT_HAS_NORMAL_BOOT)) {
    // Read the third magic byte at each candidate to identify TP header
    // TP header magic[2] = 0x9D, image header magic[2] = 0x4C
    u8 magic_byte_at_a @ (off + NORMAL_BOOT_SIZE_A + 6);

std::print("magic_byte_at_a 0x{:08X}", magic_byte_at_a);

    if (magic_byte_at_a == TPHEADER_MAGIC_BYTE2)
        tp_offset = off + NORMAL_BOOT_SIZE_A;
    else
        tp_offset = off + NORMAL_BOOT_SIZE_B;
}

// ---- 5. TP Header (Partition Table) --------------------------------------
TpHeaderData tp @ tp_offset;

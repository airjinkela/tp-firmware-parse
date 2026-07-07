#pragma author StarField Xu
#pragma endian big
#pragma magic [0x55 0xAA 0x4C 0x5E 0x83 0x1F 0x53 0x4B 0xA1 0xF8 0xF7 0xC9 0x18 0xDF 0x8F 0xBF 0x7D 0xA1 0xAA 0x55] @ 0x04

// Image Header Magic (20 bytes)
#define HEADER_MAGIC    "\x55\xAA\x4C\x5E\x83\x1F\x53\x4B\xA1\xF8\xF7\xC9\x18\xDF\x8F\xBF\x7D\xA1\xAA\x55"
// TP Header Magic (20 bytes)
#define TPHEADER_MAGIC  "\x55\xAA\x9D\xD1\xA8\xC8\x83\x31\xC9\x69\xFB\xBF\xBC\xF0\xD4\x32\x70\xC7\xAA\x55"

// Partition size constants
#define PART_SIZE_FACBOOT      0x00100000
#define NORMAL_BOOT_SIZE_A     0x000A0000
#define NORMAL_BOOT_SIZE_B     0x00040000

#define CONTENT_HAS_NORMAL_BOOT  0
#define CONTENT_HAS_ROOTFS       1
#define CONTENT_HAS_FACBOOT      4

import std.io;
import std.mem;
import std.sys;

struct ID {
    u8 id[16];
};

struct PartInfoData {
    u32 offset;
    u32 size;
};

bitfield ContentType {
    unknown1 : 11;
    have_facboot : 1;
    unknown : 2;
    have_kernel_romfs : 1;
    have_normal_boot : 1;
};

struct HeaderData {
    u16 resv[2];
    u8 magic_header[20];
    u16 header_size;
    u16 vender_id;
    u16 unknown0;
    ContentType content_type;
    u8 image_rsa_sign[128];
    u16 hwid_count;
    u16 fwid_count;
    u16 fwid_map;
    u8 unknown1[10];

    if (hwid_count > 0)
        ID hwid[hwid_count] [[color("376AD0")]];

    if (fwid_count > 0)
        ID fwid[fwid_count] [[color("17BECF")]];

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

    if (support_fwid_count > 0)
        ID fwid[support_fwid_count] [[color("17BECF")]];

    u8 pad[tp_header.size - ($ - addressof(resv))];
};

// ImageHeader
HeaderData image_header @ 0x0;

// calculate TpHeader offset
u64 off = image_header.header_size;

if (image_header.content_type.have_facboot)
    off += PART_SIZE_FACBOOT;

u64 tp_offset = off;    // default: assume no normal boot
char tpmagic_byte_a[20] @ (off + NORMAL_BOOT_SIZE_A + 4) [[hidden]];
char tpmagic_byte_b[20] @ (off + NORMAL_BOOT_SIZE_B + 4) [[hidden]];

if (image_header.content_type.have_normal_boot) {

    if (tpmagic_byte_a == TPHEADER_MAGIC)
        tp_offset = off + NORMAL_BOOT_SIZE_A;
    else if (tpmagic_byte_b == TPHEADER_MAGIC)
        tp_offset = off + NORMAL_BOOT_SIZE_B;
    else
        std::assert(TPHEADER_MAGIC, "Invalid TPHeader value!");
}

u64 offset = image_header.header_size;
struct Parts{
    TpHeaderData tp_header_part @ tp_offset;

    if (image_header.content_type.have_facboot)
    {
        u8 facboot_part[tp_header_part.facboot.size] @ offset;
        offset += tp_header_part.facboot.size;
    }

    if (image_header.content_type.have_normal_boot)
    {
        u8 normal_boot_part[tp_header_part.NormalBoot.size] @ offset;
        offset += tp_header_part.NormalBoot.size;
    }

    offset += tp_header_part.tp_header.size;

    if (image_header.content_type.have_kernel_romfs)
    {
        u8 BootingKernel_part[tp_header_part.BootingKernel.size] @ offset;
        offset += tp_header_part.BootingKernel.size;
        u8 BootingKernel_part[tp_header_part.Rootfs.size] @ offset;
        offset += tp_header_part.Rootfs.size;
    }
};

Parts parts @ 0x0;

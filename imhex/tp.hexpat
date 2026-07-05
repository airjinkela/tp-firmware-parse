// TODO

#pragma endian big

struct ID {
    u8 id[16];
};

struct PartInfoData{
    u32 offset;
    u32 size;
};

struct TpHeaderData
{
    u16 resv[2];
    u8 magic_header[20];
    u8 unknown[14];
    u16 part_count;

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
    ID fwid[support_fwid_count] [[color("17BECF")]];
    u8 pad[tp_header.size - ($ - addressof(resv))];
};

struct HeaderData
{
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
    ID hwid[hwid_count] [[color("376AD0")]];
    ID fwid[fwid_count] [[color("17BECF")]];
    u8 pad[header_size-$];
};

HeaderData hd @ 0x0;
TpHeaderData tp @ 0xa0200;
#include <stdio.h>
#include <stdint.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <malloc.h>
#include <errno.h>
#include <arpa/inet.h>


#define	perr(format, args...) fprintf(stderr, "[%s:%d]" format ": %s\n", __func__ , __LINE__ , ##args, strerror(errno))

#define	prinfo(format, args...) fprintf(stdout, format, ##args)
#define prhex(x, s) \
	do { \
		for(int __i=0;__i<s;__i++) \
			fprintf(stdout, "%02X", *(uint8_t*) (((void*)x)+__i) ); \
		fprintf(stdout, "\n"); \
	} while(0)

#define container_of(ptr, type, member) \
	((type *) ((void*)ptr - offsetof(type, member)))

#define BIT(nr) (1UL << (nr))

#define PART_SIZE_FACBOOT 0x00100000
#define PART_SIZE_NORMAL_BOOT 0x000A0000

const uint8_t default_header_magic[] = 
{
	0x55, 0xAA, 0x4C, 0x5E,
	0x83, 0x1F, 0x53, 0x4B,
	0xA1, 0xF8, 0xF7, 0xC9,
	0x18, 0xDF, 0x8F, 0xBF,
	0x7D, 0xA1, 0xAA, 0x55
};

const uint8_t default_tpheader_magic[] = 
{
	0x55, 0xAA, 0x9D, 0xD1,
	0xA8, 0xC8, 0x83, 0x31,
	0xC9, 0x69, 0xFB, 0xBF,
	0xBC, 0xF0, 0xD4, 0x32,
	0x70, 0xC7, 0xAA, 0x55
};


enum ContentType
{
	CONTENT_HAS_NORMAL_BOOT = 0,
	CONTENT_HAS_ROOTFS = 1,
	CONTENT_HAS_FACBOOT = 4,
};

enum ContentTypeBIT
{
	CONTENT_HAS_NORMAL_BOOT_BIT = BIT(CONTENT_HAS_NORMAL_BOOT),
	CONTENT_HAS_ROOTFS_BIT = BIT(CONTENT_HAS_ROOTFS),
	CONTENT_HAS_FACBOOT_BIT = BIT(CONTENT_HAS_FACBOOT),
};

struct HeaderData
{
	uint16_t resv[2];
	uint8_t magic_header[20];
	uint16_t header_size;
	uint16_t vender_id;
	uint16_t unknown0;
	uint16_t content_type;
	uint8_t image_rsa_sign[128];
	uint16_t hwid_count;
	uint16_t fwid_count;
	uint8_t fwid_map;
	uint8_t unknown1[11];
};

struct ImageHeader
{
	struct HeaderData *data;

	uint8_t *magic_header;
	uint16_t header_size;
	uint16_t vender_id;
	uint16_t content_type;
	uint8_t *image_rsa_sign;

	uint16_t hwid_count;
	uint8_t *hwid;
	uint16_t fwid_count;
	uint8_t *fwid;
	
	uint8_t fwid_map;

	bool have_facboot;
	bool have_normal_boot;
	bool have_rootfs;
};

struct TpHeaderData
{
	uint16_t resv[2];
	uint8_t magic_header[20];
	uint8_t unknown[14];
	uint16_t part_count;

	uint32_t facboot_part_offset;
	uint32_t facboot_part_size;
	uint32_t factory_info_offset;
	uint32_t factory_info_size;
	uint32_t art_offset;
	uint32_t art_size;
	uint32_t config_offset;
	uint32_t config_size;
	uint32_t NormalBoot_offset;
	uint32_t NormalBoot_size;
	uint32_t tp_header_offset;
	uint32_t tp_header_size;
	uint32_t BootingKernel_offset;
	uint32_t BootingKernel_size;
	uint32_t Rootfs_offset;
	uint32_t Rootfs_size;
	uint32_t RootfsData_offset;
	uint32_t RootfsData_size;
};

struct ImageTpHeader
{
	struct TpHeaderData *data;

	uint8_t *magic_header;

	uint16_t part_count;

	uint32_t facboot_part_offset;
	uint32_t facboot_part_size;
	uint32_t factory_info_offset;
	uint32_t factory_info_size;
	uint32_t art_offset;
	uint32_t art_size;
	uint32_t config_offset;
	uint32_t config_size;
	uint32_t NormalBoot_offset;
	uint32_t NormalBoot_size;
	uint32_t tp_header_offset;
	uint32_t tp_header_size;
	uint32_t BootingKernel_offset;
	uint32_t BootingKernel_size;
	uint32_t Rootfs_offset;
	uint32_t Rootfs_size;
	uint32_t RootfsData_offset;
	uint32_t RootfsData_size;
};


struct ImageInfo
{
	struct ImageHeader header;
	struct ImageTpHeader tpheader;
};


struct map_file_info
{
	int fd;
	ssize_t size;
	uint8_t *mmaped;
};

uint8_t **mmap_file(char *path, size_t size, ssize_t *size_maped)
{
	int fd;
	int ret;
	uint8_t *mmaped;
	size_t map_size;
	struct map_file_info *fi;

	fi = malloc(sizeof(*fi)); 
	if (fi == NULL)
	{
		perr("malloc");
		ret = -1;
		goto err_malloc;
	}

	fd = open(path, O_RDONLY);
	if (fd < 0)
	{
		perr("open file %s", path);
		ret = fd;
		goto err_open;
	}
	
	struct stat st;
	ret = fstat(fd, &st);
	if (ret == -1)
	{
		perr("fstat %s", path);
		goto err_fstat;
	}

	if (st.st_size <= 0)
	{
		fprintf(stderr, "File is empty\n");
		ret = -1;
		goto err_fstat;
	}

	if (size > 0)
	{
		if (st.st_size < size)
			map_size = st.st_size;
		else 
			map_size = size;
	}
	else
		map_size = st.st_size;

	if (size_maped)
		*size_maped = map_size;

	mmaped = mmap(NULL, map_size, PROT_READ, MAP_SHARED, fd, 0);
	if ((void*)mmaped == (void*)-1)
	{
		perr("mmap");
		ret = -1;
		goto err_fstat;
	}

	fi->fd = fd;
	fi->size = map_size;
	fi->mmaped = mmaped;
	
	return &fi->mmaped;

err_fstat:
	close(fd);
err_open:
	free(fi);
err_malloc:
	return NULL;
}

int unmmap_file(uint8_t **map_p)
{
	struct map_file_info *fi;
	fi = container_of(map_p, struct map_file_info, mmaped);

	munmap(fi->mmaped, fi->size);
	close(fi->fd);
	free(fi);
	return 0;
}

int parse_tp_image_header(uint8_t *map, struct ImageHeader *o_info)
{
	struct HeaderData *imd = (void*)map;
	o_info->data = imd;

	o_info->header_size = htons(imd->header_size);
	o_info->vender_id = htonl(imd->vender_id);
	o_info->content_type = htons(imd->content_type);
	o_info->hwid_count = htons(imd->hwid_count);
	o_info->fwid_count = htons(imd->fwid_count);
	o_info->fwid_map = imd->fwid_map;

	o_info->magic_header = imd->magic_header;
	o_info->image_rsa_sign = imd->image_rsa_sign;

	if (memcmp(o_info->magic_header, default_header_magic, sizeof(default_header_magic)))
	{
		prinfo("image header magic is mot a valid header magic\n");
		return -1;
	}

	if (imd->hwid_count)
	{
		uint8_t *hwid = (void*)map + sizeof(struct HeaderData);	
		o_info->hwid = hwid;
	}
	if (imd->fwid_count)
	{	
		uint8_t *fwid = (void*)map + sizeof(struct HeaderData) + htons(imd->hwid_count)*16;
		o_info->fwid = fwid;
	}

	for (int i=0; i<16; i++)
	{
		switch (i)
		{
		case CONTENT_HAS_NORMAL_BOOT:
			o_info->have_normal_boot = !!(o_info->content_type & CONTENT_HAS_NORMAL_BOOT_BIT);
			break;
		case CONTENT_HAS_ROOTFS:
			o_info->have_rootfs = !!(o_info->content_type & CONTENT_HAS_ROOTFS_BIT);
			break;
		case CONTENT_HAS_FACBOOT:
			o_info->have_facboot = !!(o_info->content_type & CONTENT_HAS_FACBOOT_BIT);
			break;
		default:
			break;
		}
	}

	/*
	for (int i=0; i<o_info->hwid_count; i++)
	{
		for(int j=0;j<16;j++)
			printf("%02X", *(uint8_t*) (((void*)o_info->hwid)+j+i*16) );
		printf("\n"); 
	}

	for (int i=0; i<o_info->fwid_count; i++)
	{	
		for(int j=0;j<16;j++)
			printf("%02X", *(uint8_t*)  (((void*)o_info->fwid)+j+i*16) );
		printf("\n"); 
	}
	*/

	return 0;
}

int prase_tp_image_tpheader(uint8_t *map, struct ImageInfo *imgif)
{
	size_t tp_header_offset = 0;
	tp_header_offset += imgif->header.header_size;
	if (imgif->header.have_normal_boot)
		tp_header_offset += PART_SIZE_NORMAL_BOOT;
	if (imgif->header.have_facboot)
		tp_header_offset += PART_SIZE_FACBOOT;

	struct TpHeaderData *tp_header = (void*)map + tp_header_offset;
	imgif->tpheader.data = tp_header;


	imgif->tpheader.magic_header = tp_header->magic_header;
	if (memcmp(imgif->tpheader.magic_header, default_tpheader_magic, sizeof(default_tpheader_magic)))
	{
		prinfo("image tpheader magic is mot a valid tpheader magic\n");
		return -1;
	}

	imgif->tpheader.part_count = htons(tp_header->part_count);
	imgif->tpheader.facboot_part_offset = htonl(tp_header->facboot_part_offset);
	imgif->tpheader.facboot_part_size = htonl(tp_header->facboot_part_size);
	imgif->tpheader.factory_info_offset = htonl(tp_header->factory_info_offset);
	imgif->tpheader.factory_info_size = htonl(tp_header->factory_info_size);
	imgif->tpheader.art_offset = htonl(tp_header->art_offset);
	imgif->tpheader.art_size = htonl(tp_header->art_size);
	imgif->tpheader.config_offset = htonl(tp_header->config_offset);
	imgif->tpheader.config_size = htonl(tp_header->config_size);
	imgif->tpheader.NormalBoot_offset = htonl(tp_header->NormalBoot_offset);
	imgif->tpheader.NormalBoot_size = htonl(tp_header->NormalBoot_size);
	imgif->tpheader.tp_header_offset = htonl(tp_header->tp_header_offset);
	imgif->tpheader.tp_header_size = htonl(tp_header->tp_header_size);
	imgif->tpheader.BootingKernel_offset = htonl(tp_header->BootingKernel_offset);
	imgif->tpheader.BootingKernel_size = htonl(tp_header->BootingKernel_size);
	imgif->tpheader.Rootfs_offset = htonl(tp_header->Rootfs_offset);
	imgif->tpheader.Rootfs_size = htonl(tp_header->Rootfs_size);
	imgif->tpheader.RootfsData_offset = htonl(tp_header->RootfsData_offset);
	imgif->tpheader.RootfsData_size = htonl(tp_header->RootfsData_size);
/*
	for (int i=0; i<20; i++)
	{
		printf("%X", tp_header->magic_header[i]);
	}
	printf("\n");
	printf("facboot_part_offset: %d\n", imgif->tpheader.facboot_part_offset);
	printf("facboot_part_size: %d\n", imgif->tpheader.facboot_part_size);
	printf("factory_info_offset: %d\n", imgif->tpheader.factory_info_offset);
	printf("factory_info_size: %d\n", imgif->tpheader.factory_info_size);
	printf("art_offset: %d\n", imgif->tpheader.art_offset);
	printf("art_size: %d\n", imgif->tpheader.art_size);
	printf("config_offset: %d\n", imgif->tpheader.config_offset);
	printf("config_size: %d\n", imgif->tpheader.config_size);
	printf("NormalBoot_offset: %d\n", imgif->tpheader.NormalBoot_offset);
	printf("NormalBoot_size: %d\n", imgif->tpheader.NormalBoot_size);
	printf("tp_header_offset: %d\n", imgif->tpheader.tp_header_offset);
	printf("tp_header_size: %d\n", imgif->tpheader.tp_header_size);
	printf("BootingKernel_offset: %d\n", imgif->tpheader.BootingKernel_offset);
	printf("BootingKernel_size: %d\n", imgif->tpheader.BootingKernel_size);
	printf("Rootfs_offset: %d\n", imgif->tpheader.Rootfs_offset);
	printf("Rootfs_size: %d\n", imgif->tpheader.Rootfs_size);
	printf("RootfsData_offset: %d\n", imgif->tpheader.RootfsData_offset);
	printf("RootfsData_size: %d\n", imgif->tpheader.RootfsData_size);
*/

	return 0;
}

int prase_image(uint8_t *map, struct ImageInfo *imgif)
{
	int ret;

	ret = parse_tp_image_header(map, &(imgif->header));
	if (ret) return ret;
	ret = prase_tp_image_tpheader(map, imgif);
	if (ret) return ret;

	return 0;
}

void print_image_info(struct ImageInfo *imgif)
{
	prinfo("==========================Image Header==========================\n");
	prinfo("Image Header Magic Code: ");
	prhex(imgif->header.magic_header, 20);
	prinfo("Image Header Size: %d\n", imgif->header.header_size);
	prinfo("Image VendorId: %d\n", imgif->header.vender_id);
	prinfo("Image ContentType: %04X\n", imgif->header.content_type);
	prinfo("-------------------------Image RSA Signalture-------------------\n");
	prhex(imgif->header.image_rsa_sign, 32);
	prhex(imgif->header.image_rsa_sign+32, 32);
	prhex(imgif->header.image_rsa_sign+64, 32);
	prhex(imgif->header.image_rsa_sign+96, 32);
	prinfo("----------------------------------------------------------------\n");


	if (imgif->header.hwid)
	{
		prinfo("Image HWID count: %d\n", imgif->header.hwid_count);
		for (int i=0; i<imgif->header.hwid_count; i++)
		{
			prinfo("HWID %02d: ", i);
			prhex(imgif->header.hwid+(i*16), 16);
		}
	}

	if (imgif->header.fwid)
	{
		prinfo("Image FWID count: %d\n", imgif->header.fwid_count);
		prinfo("Image FWID BitMap: %08b\n", imgif->header.fwid_map);
		for (int i=0; i<imgif->header.fwid_count; i++)
		{
			prinfo("FWID %02d: ", i);
			prhex((imgif->header.fwid+(i*16)), 16);
		}
	}
	prinfo("Image have_facboot: %d\n", imgif->header.have_facboot);
	prinfo("Image have_normal_boot: %d\n", imgif->header.have_normal_boot);
	prinfo("Image have_rootfs: %d\n", imgif->header.have_rootfs);

	prinfo("================================================================\n\n\n");

	prinfo("=============================TP Header==========================\n");
	prinfo("Tp Header Magic Code: ");
	prhex(imgif->tpheader.magic_header, 20);
	prinfo("Tp Part Count: %d\n", imgif->tpheader.part_count);
	prinfo("PartName       PartOffset       PartSize\n");
	prinfo("FacBoot:       0x%08X       0x%08X\n",
		imgif->tpheader.facboot_part_offset, imgif->tpheader.facboot_part_size);
	prinfo("FactoryInfo:   0x%08X       0x%08X\n",
		imgif->tpheader.factory_info_offset, imgif->tpheader.factory_info_size);
	prinfo("Art:           0x%08X       0x%08X\n",
		imgif->tpheader.art_offset, imgif->tpheader.art_size);
	prinfo("Config:        0x%08X       0x%08X\n",
		imgif->tpheader.config_offset, imgif->tpheader.config_size);
	prinfo("NormalBoot:    0x%08X       0x%08X\n",
		imgif->tpheader.NormalBoot_offset, imgif->tpheader.NormalBoot_size);
	prinfo("TpHeader:      0x%08X       0x%08X\n",
		imgif->tpheader.tp_header_offset, imgif->tpheader.tp_header_size);
	prinfo("BootingKernel: 0x%08X       0x%08X\n",
		imgif->tpheader.BootingKernel_offset, imgif->tpheader.BootingKernel_size);
	prinfo("Rootfs:        0x%08X       0x%08X\n",
		imgif->tpheader.Rootfs_offset, imgif->tpheader.Rootfs_size);
	prinfo("RootfsData:    0x%08X       0x%08X\n",
		imgif->tpheader.RootfsData_offset, imgif->tpheader.RootfsData_size);
	prinfo("================================================================\n");
}

int main(int argc, char *argv[])
{
	if (argc < 2)
	{
		fprintf(stderr, "missing argument 'file path'\n");
		return 1;
	}	

	size_t maped_size;

	uint8_t **map_p = mmap_file(argv[1], -1, &maped_size);
	uint8_t *map = *map_p;

	if (maped_size < sizeof(struct HeaderData))
	{
		fprintf(stderr, "image is too small\n");
		return 1;
	}

	struct ImageInfo image;

	prase_image(map, &image);

	print_image_info(&image);

	unmmap_file(map_p);
	return 0;
}

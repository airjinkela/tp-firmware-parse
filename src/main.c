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


#define	perr(format, args...) \
	  fprintf(stderr, "[%s:%d]" format ": %s\n", \
		  __func__ , __LINE__ , ##args, strerror(errno))

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

// just don't know what this macro definition should be called
#define UNKNOWN(name) \
	struct { \
		uint32_t offset; \
		uint32_t size; \
	} name;

struct TpHeaderData
{
	uint16_t resv[2];
	uint8_t magic_header[20];
	uint8_t unknown[14];
	uint16_t part_count;

	UNKNOWN(facboot);
	UNKNOWN(factory_info);
	UNKNOWN(art);
	UNKNOWN(config);
	UNKNOWN(NormalBoot);
	UNKNOWN(tp_header);
	UNKNOWN(BootingKernel);
	UNKNOWN(Rootfs);
	UNKNOWN(RootfsData);
};

struct ImageTpHeader
{
	struct TpHeaderData *data;

	uint8_t *magic_header;

	uint16_t part_count;

	UNKNOWN(facboot);
	UNKNOWN(factory_info);
	UNKNOWN(art);
	UNKNOWN(config);
	UNKNOWN(NormalBoot);
	UNKNOWN(tp_header);
	UNKNOWN(BootingKernel);
	UNKNOWN(Rootfs);
	UNKNOWN(RootfsData);
};
#undef UNKNOWN

struct ImageParts
{
	struct
	{
		uint8_t *data;
		int size;
	} facboot;

	struct
	{
		uint8_t *data;
		int size;
	} normal_boot;

	struct
	{
		uint8_t *data;
		int size;
	} tp_header;

	struct
	{
		uint8_t *data;
		int size;
	} kernel;

	struct
	{
		uint8_t *data;
		int size;
	} rootfs;
};

struct ImageInfo
{
	struct ImageHeader header;
	struct ImageTpHeader tpheader;
	struct ImageParts parts;
};

struct map_file_info
{
	int fd;
	ssize_t size;
	uint8_t *mmaped;
};

int save_data_to_file(char *file_name, char *parent_dir, void *data, size_t size)
{
	int ret;
	char buf[256];

	ret = mkdir(parent_dir, 0755);
	if (ret == -1 && errno != EEXIST)
	{
			fprintf(stderr, "failed to create dir: %s: %s\n", parent_dir, strerror(errno));
			return -1;
	}

	snprintf(buf, 256, "%s/%s", parent_dir, file_name);

	int fd = open(buf, O_CREAT | O_WRONLY, 0777);
	if (fd < 0)
	{
		perr("open");
		return fd;
	}

	write(fd, data, size);

	fsync(fd);

	close(fd);

	return 0;
}


uint8_t **mmap_file(char *path, size_t size, size_t *size_maped)
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

	return 0;
}

int parse_tp_image_tpheader(uint8_t *map, struct ImageInfo *imgif)
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

	imgif->tpheader.facboot.offset = htonl(tp_header->facboot.offset);
	imgif->tpheader.facboot.size = htonl(tp_header->facboot.size);
	imgif->tpheader.factory_info.offset = htonl(tp_header->factory_info.offset);
	imgif->tpheader.factory_info.size = htonl(tp_header->factory_info.size);
	imgif->tpheader.art.offset = htonl(tp_header->art.offset);
	imgif->tpheader.art.size = htonl(tp_header->art.offset);
	imgif->tpheader.config.offset = htonl(tp_header->config.offset);
	imgif->tpheader.config.size = htonl(tp_header->config.size);
	imgif->tpheader.NormalBoot.offset = htonl(tp_header->NormalBoot.offset);
	imgif->tpheader.NormalBoot.size = htonl(tp_header->NormalBoot.size);
	imgif->tpheader.tp_header.offset = htonl(tp_header->tp_header.offset);
	imgif->tpheader.tp_header.size = htonl(tp_header->tp_header.size);
	imgif->tpheader.BootingKernel.offset = htonl(tp_header->BootingKernel.offset);
	imgif->tpheader.BootingKernel.size = htonl(tp_header->BootingKernel.size);
	imgif->tpheader.Rootfs.offset = htonl(tp_header->Rootfs.offset);
	imgif->tpheader.Rootfs.size = htonl(tp_header->Rootfs.size);
	imgif->tpheader.RootfsData.offset = htonl(tp_header->RootfsData.offset);
	imgif->tpheader.RootfsData.size = htonl(tp_header->RootfsData.size);

	return 0;
}

int parse_image_parts(uint8_t *map, struct ImageInfo *imgif)
{
	uint8_t *fw_start = (void*)map + imgif->header.header_size;
	uint8_t *normal_boot_start = fw_start;

	if (imgif->header.have_facboot)
	{
		normal_boot_start += imgif->tpheader.facboot.size;
		imgif->parts.facboot.data = fw_start;
		imgif->parts.facboot.size = imgif->tpheader.facboot.size;
	}
	else
	{
		imgif->parts.facboot.data = NULL;
		imgif->parts.facboot.size = 0;
	}

	if (imgif->header.have_normal_boot)
	{
		imgif->parts.normal_boot.data = normal_boot_start;
		imgif->parts.normal_boot.size = imgif->tpheader.NormalBoot.size;
	}
	else
	{
		imgif->parts.normal_boot.data = NULL;
		imgif->parts.normal_boot.size = 0;
	}

	uint8_t *tp_header_start = normal_boot_start + imgif->parts.normal_boot.size;
	imgif->parts.tp_header.data = tp_header_start;
	imgif->parts.tp_header.size = imgif->tpheader.tp_header.size;

	uint8_t *kernel_start = tp_header_start + imgif->parts.tp_header.size;
	if (imgif->header.have_rootfs)
	{
		imgif->parts.kernel.data = kernel_start;
		imgif->parts.kernel.size = imgif->tpheader.BootingKernel.size;

		uint8_t *rootfs_start = kernel_start + imgif->parts.kernel.size;
		imgif->parts.rootfs.data = rootfs_start;
		imgif->parts.rootfs.size = imgif->tpheader.Rootfs.size;
	}
	else
	{
		imgif->parts.kernel.data = NULL;
		imgif->parts.kernel.size = 0;
		imgif->parts.rootfs.data = NULL;
		imgif->parts.rootfs.size = 0;
	}
	return 0;
}

int parse_image_headers(uint8_t *map, struct ImageInfo *imgif)
{
	int ret;

	ret = parse_tp_image_header(map, &(imgif->header));
	if (ret) return ret;
	ret = parse_tp_image_tpheader(map, imgif);
	if (ret) return ret;

	return 0;
}

int parse_image(uint8_t *map, struct ImageInfo *imgif)
{
	int ret;
	ret = parse_image_headers(map, imgif);
	if (ret) return ret;

	ret = parse_image_parts(map, imgif);
	if (ret) return ret;

	return 0;
}

int dump_image(struct ImageInfo *imgif)
{
	char *save_dir = "out";
	save_data_to_file("facboot.bin", save_dir,
		imgif->parts.facboot.data,  imgif->parts.facboot.size);
	save_data_to_file("normal_boot.bin", save_dir,
		imgif->parts.normal_boot.data,  imgif->parts.normal_boot.size);
	save_data_to_file("tp_header.bin", save_dir,
		imgif->parts.tp_header.data,  imgif->parts.tp_header.size);
	save_data_to_file("kernel.bin", save_dir,
		imgif->parts.kernel.data,  imgif->parts.kernel.size);
	save_data_to_file("rootfs.bin", save_dir,
		imgif->parts.rootfs.data,  imgif->parts.rootfs.size);
	return 0;
}

void print_image_header_info(struct ImageInfo *imgif)
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
		imgif->tpheader.facboot.size, imgif->tpheader.facboot.offset);
	prinfo("FactoryInfo:   0x%08X       0x%08X\n",
		imgif->tpheader.factory_info.size, imgif->tpheader.factory_info.offset);
	prinfo("Art:           0x%08X       0x%08X\n",
		imgif->tpheader.art.size, imgif->tpheader.art.offset);
	prinfo("Config:        0x%08X       0x%08X\n",
		imgif->tpheader.config.size, imgif->tpheader.config.offset);
	prinfo("NormalBoot:    0x%08X       0x%08X\n",
		imgif->tpheader.NormalBoot.size, imgif->tpheader.NormalBoot.offset);
	prinfo("TpHeader:      0x%08X       0x%08X\n",
		imgif->tpheader.tp_header.size, imgif->tpheader.tp_header.offset);
	prinfo("BootingKernel: 0x%08X       0x%08X\n",
		imgif->tpheader.BootingKernel.size, imgif->tpheader.BootingKernel.offset);
	prinfo("Rootfs:        0x%08X       0x%08X\n",
		imgif->tpheader.Rootfs.size, imgif->tpheader.Rootfs.offset);
	prinfo("RootfsData:    0x%08X       0x%08X\n",
		imgif->tpheader.RootfsData.size, imgif->tpheader.RootfsData.offset);
	prinfo("================================================================\n");
}

void print_image_part_info(struct ImageInfo *imgif)
{
	prinfo("==========================Image Parts===========================\n");
	prinfo("PartName       PartAddress         PartSize\n");
	if (imgif->parts.facboot.size)
		prinfo("FacBoot:       %16p    0x%08X\n",
			imgif->parts.facboot.data, imgif->parts.facboot.size);
	if (imgif->parts.normal_boot.size)
		prinfo("NormalBoot:    %16p    0x%08X\n",
			imgif->parts.normal_boot.data, imgif->parts.normal_boot.size);
	if (imgif->parts.tp_header.size)
		prinfo("TpHeader:      %16p    0x%08X\n",
			imgif->parts.tp_header.data, imgif->parts.tp_header.size);
	if (imgif->parts.kernel.size)
		prinfo("BootingKernel: %16p    0x%08X\n",
			imgif->parts.kernel.data, imgif->parts.kernel.size);
	if (imgif->parts.rootfs.size)
		prinfo("Rootfs:        %16p    0x%08X\n",
			imgif->parts.rootfs.data, imgif->parts.rootfs.size);
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
	if (map_p == NULL)
		return 1;

	uint8_t *map = *map_p;

	if (maped_size < sizeof(struct HeaderData))
	{
		fprintf(stderr, "image is too small\n");
		return 1;
	}

	struct ImageInfo image;

	parse_image(map, &image);

	print_image_header_info(&image);
	print_image_part_info(&image);
	dump_image(&image);
	unmmap_file(map_p);
	return 0;
}

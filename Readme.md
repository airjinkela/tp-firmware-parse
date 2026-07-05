# TP-Link Firmware Parser

A command-line tool for parsing TP-Link router firmware (`.bin`) binary files, written in C for POSIX systems (Linux).

## Features

- Parses the **Image Header**:
  - Magic validation (`55 AA 4C 5E ...`)
  - Header size, vendor ID
  - Content type flags (NormalBoot / Rootfs / FacBoot presence)
  - 128-byte RSA signature
  - Hardware ID (HWID) list
  - Firmware ID (FWID) list and bitmap
- Parses the **TP Header** (partition table):
  - Magic validation (`55 AA 9D D1 ...`)
  - Partition count and offset/size for each partition:

| Partition | Description |
|---|---|
| FacBoot | Factory boot partition |
| FactoryInfo | Factory information partition |
| Art | Atheros Radio Test partition |
| Config | Configuration partition |
| NormalBoot | Normal boot partition |
| TpHeader | TP partition table itself |
| BootingKernel | Booting kernel partition |
| Rootfs | Root filesystem partition |
| RootfsData | Root filesystem data partition |

- Pretty-prints all parsed results in hex and decimal

## Compile

```bash
make
```

The output is the `main` executable in the project root directory.

## Usage

```bash
./main <firmware-file-path>
```

### Example

```bash
./main "7dr7299mv1_cn_1_0_7_up_boot(260313)_2026-03-13_22.08.58.bin"
```

The output will display the complete parsed information of the Image Header followed by the TP Header.

## Supported Devices

- **TP-Link 7DR7299** (or similar series routers)

## TODO
- ~~Split upgrade files into independent partitions~~

## Links
- [Tplink 7dr7299 Upgrade Firmware](https://resource.tp-link.com.cn/pc/docCenter/showDoc?id=1779808801125266)

## License

This project is for educational and research purposes only. Do not use it for illegal activities.
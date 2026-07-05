# TP-Link Firmware Parser

A command-line tool for parsing TP-Link router firmware (`.bin`) binary files, written in C for POSIX systems (Linux).

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
- [Tplink TL-7DR7299 Upgrade Firmware](https://resource.tp-link.com.cn/pc/docCenter/showDoc?id=1779808801125266)
- [Tplink TL-7DR7290 Upgrade Firmware](https://resource.tp-link.com.cn/pc/docCenter/showDoc?id=1775814620215237)
- [Tplink TL-XDR6088 Upgrade Firmware](https://resource.tp-link.com.cn/pc/docCenter/showDoc?id=1721727939475413)


## License

This project is for educational and research purposes only. Do not use it for illegal activities.
# BEKEN Chip Vendor Support for openvela

[ English | [简体中文](README_zh-cn.md) ]

## Introduction

This repository contains chip support, board configurations, and hardware
drivers from BEKEN Corporation for the openvela operating system. BEKEN is a
wireless connectivity chip vendor focused on Wi-Fi, Bluetooth and other solutions for
AIoT applications.

The current port targets the **BK7236N** SoC and integrates with the BEKEN
Armino SDK through prebuilt libraries and headers under
`chips/bk7236n/bk_idk/armino_as_lib/`.

## Repository Structure

```
vendor/beken/
├── boards/                  # Board-specific configurations
│   └── bk7236n/
│       └── bk7236n-evb/     # BK7236N evaluation board
├── chips/                   # Chip support and hardware abstraction layer
│   └── bk7236n/             # BK7236N chip port
│       ├── bk_idk/
│       │   └── armino_as_lib/  # Prebuilt Armino SDK libs and headers
│       └── bootloader.bin     # Prebuilt bootloader binary
├── tools/
│   └── cmake_encrypt_crc      # Prebuilt host utility for post-build CRC
├── LICENSE                    # Repository license
└── LICENSE-NOTES.md           # Prebuilt binary license notes
```

## Supported Hardware

### BK7236N

**BK7236N** is a Wi-Fi and Bluetooth capable SoC for AIoT devices, featuring:

- **Processor**: Arm®v8-M STAR-MC1 MCU
- **Wireless Connectivity**: Integrated Wi-Fi 6 and Bluetooth Low Energy (LE) 5.4
- **Applications**: Smart home, wireless modules, and embedded connectivity devices

### Development Board

The official BK7236N evaluation board configuration is provided under
`boards/bk7236n/bk7236n-evb/`.

## Integration with openvela

openvela is an embedded OS tailored for AIoT applications. This chip vendor
repository integrates with openvela through:

1. **Kconfig Integration**: Chip and board options via `menuconfig`
2. **Build System**: NuttX/openvela CMake and Makefile build infrastructure
3. **Device Drivers**: Standard NuttX lower-half driver interfaces
4. **Armino SDK**: Prebuilt static libraries linked during the NuttX build

## Configuration and Build

To configure the BK7236N evaluation board with NSH:

```bash
# From the openvela workspace root
./build.sh vendor/beken/boards/bk7236n/bk7236n-evb/nsh

# CMake build (optional)
./build.sh vendor/beken/boards/bk7236n/bk7236n-evb/nsh --cmake
```

To customize features, use `menuconfig` after configuration:

```bash
make -C nuttx menuconfig
```

## License

Repository metadata is provided under the [Apache License 2.0](LICENSE).

Prebuilt binaries in this repository (static libraries, bootloader, and host
tools) are **BEKEN Proprietary (Restricted)**. Some libraries may embed
third-party open-source components; see [LICENSE-NOTES.md](LICENSE-NOTES.md) for
details.

```
Copyright (C) 2026 BEKEN Corporation

Prebuilt binary artifacts in this repository are subject to BEKEN proprietary
terms. See LICENSE-NOTES.md for the complete list and license classification.
```

## Contributing

For chip vendor-specific changes, please follow BEKEN's internal development
process. For openvela integration issues, please refer to the main openvela
contribution guidelines.

## Support

- **BEKEN Corporation**: Contact your BEKEN representative for chip-specific
  support and SDK access
- **openvela Integration**: See openvela documentation or community resources

## Related Resources

- [BEKEN Armino bk_idk SDK](https://github.com/bekencorp/bk_idk/tree/release/v2.2.1)
- [BEKEN official docs](https://docs.bekencorp.com/arminodoc/bk_idk/bk7236n/en/v2.2.1/)
- [LICENSE-NOTES.md](LICENSE-NOTES.md)

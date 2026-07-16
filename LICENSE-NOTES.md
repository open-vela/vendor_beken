# License Notes — Beken BK7236N Prebuilt Binaries

Copyright (C) 2026 BEKEN Corporation

## Overview

This document describes prebuilt binary artifacts for the BK7236N platform in
this repository, including:

- Static libraries and headers under `chips/bk7236n/bk_idk/armino_as_lib/`
- Bootloader binary at `chips/bk7236n/bootloader.bin`
- Host build utility at `tools/cmake_encrypt_crc`

All prebuilt static libraries in this repository are distributed as binary
artifacts without source code. The **License** column below indicates the
license terms of the underlying component. Libraries marked **BEKEN
Proprietary (Restricted)** are closed-source BEKEN components. See also
"Third-Party Components" for embedded open-source attribution.

## Source

The static libraries are prebuilt from the BEKEN Armino bk_idk SDK and
packaged by `tools/build_tools/armino_as_lib.sh`. The bootloader and host
utility are compiled and provided by BEKEN Corporation.

## Restricted Use

Prebuilt binaries marked **BEKEN Proprietary (Restricted)** may only be used
as part of authorized BK7236N platform development and may not be
redistributed, reverse-engineered, or used outside the scope granted by BEKEN
Corporation.

Libraries under Apache-2.0 or other open-source licenses are subject to their
respective license terms in addition to any BEKEN distribution restrictions.

## License Summary

| Category | License | bk_idk source available |
|----------|---------|-------------------------|
| BEKEN proprietary (properties) libraries | BEKEN Proprietary (Restricted) | No |
| BEKEN SDK libraries | Apache-2.0 | Yes |
| Third-party libraries | See "Third-Party Components" | Yes |

> **Note:** `libbk_wifi.a` and `libbk_bluetooth.a` are Apache-2.0 host/API
> layers but link against proprietary `libwifi.a` and
> `libbluetooth_controller_controller_only.a`. WiFi/BT MAC/PHY functionality
> resides in the proprietary libraries.

## Other Prebuilt Binaries

### bootloader.bin

- **Path:** `chips/bk7236n/bootloader.bin`
- **Description:** Prebuilt bootloader binary for the BK7236N platform.
- **License:** BEKEN Proprietary (Restricted)

### cmake_encrypt_crc

- **Path:** `tools/cmake_encrypt_crc`
- **Description:** Prebuilt host utility used during the post-build process
  to generate and append CRCs to firmware binaries.
- **Version:** 5.0.0.7
- **License:** BEKEN Proprietary (Restricted)

## Libraries

| Library | bk_idk source | License |
|---------|---------------|---------|
| libbase64.a | `components/base64/` | Apache-2.0 |
| libbk7236n.a | `middleware/soc/bk7236n/` | Apache-2.0 |
| libbk_ate.a | `components/bk_ate/` | Apache-2.0 |
| libbk_bluetooth.a | `components/bk_bluetooth/` | Apache-2.0 |
| libbk_cli.a | `components/bk_cli/` | Apache-2.0 |
| libbk_coex.a | `properties/modules/bk_coex` | BEKEN Proprietary (Restricted) |
| libbk_common.a | `components/bk_common/` | Apache-2.0 |
| libbk_event.a | `components/bk_event/` | Apache-2.0 |
| libbk_init.a | `components/bk_init/` | Apache-2.0 |
| libbk_netif.a | `components/bk_netif/` | Apache-2.0 |
| libbk_persist_config.a | `components/bk_persist_config/` | Apache-2.0 |
| libbk_phy.a | `components/bk_phy/src/` | Apache-2.0 |
| libbk_phy_info.a | `properties/modules/bk_phy` | BEKEN Proprietary (Restricted) |
| libbk_pm.a | `components/bk_pm/` | Apache-2.0 |
| libbk_rtos.a | `components/bk_rtos/` | Apache-2.0 |
| libbk_startup.a | `components/bk_startup/` | Apache-2.0 |
| libbk_system.a | `components/bk_system/` | Apache-2.0 |
| libbk_wifi.a | `components/bk_wifi/` | Apache-2.0 |
| libbluetooth_controller_controller_only.a | `properties/modules/bluetooth` | BEKEN Proprietary (Restricted) |
| libcm33.a | `middleware/arch/cm33/` | Apache-2.0 |
| libcmsis.a | `components/cmsis/` | Apache-2.0 |
| libcommon.a | `middleware/soc/common/` | Apache-2.0 |
| libcompal.a | `middleware/compal/` | Apache-2.0 |
| libcom_phy.a | `properties/modules/bk_phy` | BEKEN Proprietary (Restricted) |
| libdriver.a | `middleware/driver/` | Apache-2.0 |
| libeasy_flash.a | `components/easy_flash/` | MIT |
| liblwip_intf_v2_1.a | `components/lwip_intf_v2_1/` | BSD-style |
| libmain.a | `projects/app/main/` | Apache-2.0 |
| libproduct.a | `properties/modules/bk_phy` | BEKEN Proprietary (Restricted) |
| libpsa_mbedtls.a | `components/psa_mbedtls/` | Apache-2.0 OR GPL-2.0-or-later |
| libtemp_detect.a | `components/temp_detect/` | Apache-2.0 |
| libvnd_cal.a | `middleware/boards/bk7236n/vnd_cal/` | Apache-2.0 |
| libwifi.a | `properties/modules/wifi` | BEKEN Proprietary (Restricted) |
| libwpa_supplicant-2.10.a | `components/wpa_supplicant-2.10/` | BSD-3-Clause (preferred) / GPL-2.0 |

## Third-Party Components

The following open-source components are statically linked into the prebuilt
libraries above. Their original licenses apply in addition to any BEKEN
distribution terms.

### ARM CMSIS

- **Contained in:** `libcmsis.a`
- **Source in bk_idk:** https://github.com/bekencorp/bk_idk/tree/release/v2.2.1/components/cmsis
- **Embedded license:** Apache-2.0
- **Original project:** https://github.com/ARM-software/CMSIS_5

### EasyFlash

- **Contained in:** `libeasy_flash.a`
- **Source in bk_idk:** https://github.com/bekencorp/bk_idk/tree/release/v2.2.1/components/easy_flash
- **Embedded license:** MIT
- **Original project:** https://github.com/armink/EasyFlash

### lwIP 2.1.2

- **Contained in:** `liblwip_intf_v2_1.a`
- **Source in bk_idk:** https://github.com/bekencorp/bk_idk/tree/release/v2.2.1/components/lwip_intf_v2_1
- **Embedded license:** BSD-style
- **Original project:** https://savannah.nongnu.org/projects/lwip/

### Mbed TLS (PSA)

- **Contained in:** `libpsa_mbedtls.a`
- **Source in bk_idk:** https://github.com/bekencorp/bk_idk/tree/release/v2.2.1/components/psa_mbedtls
- **Embedded license:** Apache-2.0 OR GPL-2.0-or-later
- **Original project:** https://github.com/Mbed-TLS/mbedtls

### wpa_supplicant 2.10

- **Contained in:** `libwpa_supplicant-2.10.a`
- **Source in bk_idk:** https://github.com/bekencorp/bk_idk/tree/release/v2.2.1/components/wpa_supplicant-2.10
- **Embedded license:** BSD-3-Clause (preferred) / GPL-2.0
- **Original project:** https://w1.fi/wpa_supplicant/

## Headers

Public API headers under `chips/bk7236n/bk_idk/armino_as_lib/include/` are
copied from bk_idk `include/` during packaging. BEKEN-owned headers are
generally under Apache-2.0; proprietary components follow the same
restrictions as their corresponding libraries. Third-party header copyrights
are preserved in the individual header files.

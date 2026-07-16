# BEKEN 芯片 openvela 支持

[ [English](README.md) | 简体中文 ]

## 简介

本仓库包含 BEKEN（博通集成）为 openvela 操作系统提供的芯片支持、板级配置
及硬件驱动。BEKEN 是一家专注于 AIoT 领域 Wi-Fi 与蓝牙等无线连接芯片的厂商。

当前移植目标为 **BK7236N** SoC，并通过
`chips/bk7236n/bk_idk/armino_as_lib/` 下的预编译库与头文件集成 BEKEN Armino
SDK。

## 仓库结构

```
vendor/beken/
├── boards/                  # 板级配置
│   └── bk7236n/
│       └── bk7236n-evb/     # BK7236N 评估板
├── chips/                   # 芯片支持与硬件抽象层
│   └── bk7236n/             # BK7236N 芯片移植
│       ├── bk_idk/
│       │   └── armino_as_lib/  # 预编译 Armino SDK 库与头文件
│       └── bootloader.bin     # 预编译 Bootloader 二进制
├── tools/
│   └── cmake_encrypt_crc      # 预编译构建后 CRC 工具
├── LICENSE                    # 仓库许可证
└── LICENSE-NOTES.md           # 预编译二进制许可说明
```

## 支持的硬件

### BK7236N

**BK7236N** 是一款面向 AIoT 设备的 Wi-Fi / 蓝牙 SoC，主要特性包括：

- **处理器**：Arm®v8-M STAR-MC1 MCU
- **无线连接**：集成 Wi-Fi 6 与 BLE 5.4
- **应用场景**：智能家居、无线模组及嵌入式连接设备等

### 开发板

BK7236N 官方评估板配置位于 `boards/bk7236n/bk7236n-evb/`。

## 与 openvela 的集成

openvela 是面向 AIoT 应用的嵌入式操作系统。本芯片 vendor 仓库通过以下方式
与 openvela 集成：

1. **Kconfig 集成**：通过 `menuconfig` 配置芯片与板级选项
2. **构建系统**：NuttX/openvela CMake 与 Makefile 构建基础设施
3. **设备驱动**：标准 NuttX lower-half 驱动接口
4. **Armino SDK**：NuttX 构建过程中链接预编译静态库

## 配置与构建

配置 BK7236N 评估板 NSH 方案：

```bash
# 在 openvela 工作区根目录执行
./build.sh vendor/beken/boards/bk7236n/bk7236n-evb/nsh

# CMake 构建（可选）
./build.sh vendor/beken/boards/bk7236n/bk7236n-evb/nsh --cmake
```

完成配置后，可通过 `menuconfig` 自定义功能：

```bash
make -C nuttx menuconfig
```

## 许可证

仓库元数据遵循 [Apache License 2.0](LICENSE)。

本仓库中的预编译二进制（静态库、Bootloader 及工具）均为 **BEKEN
Proprietary (Restricted)**。部分库内嵌第三方开源组件，详见
[LICENSE-NOTES.md](LICENSE-NOTES.md)。

```
Copyright (C) 2026 BEKEN Corporation

本仓库中的预编译二进制产物受 BEKEN 专有条款约束。
完整清单及许可分类见 LICENSE-NOTES.md。
```

## 贡献

芯片 vendor 相关变更请遵循 BEKEN 内部开发流程。openvela 集成相关问题请
参考 openvela 主仓库的贡献指南。

## 支持

- **BEKEN Corporation**：芯片与 SDK 相关问题请联系 BEKEN 对接人员
- **openvela 集成**：参见 openvela 文档或社区资源

## 相关资源

- [BEKEN Armino bk_idk SDK](https://github.com/bekencorp/bk_idk/tree/release/v2.2.1)
- [BEKEN official docs](https://docs.bekencorp.com/arminodoc/bk_idk/bk7236n/zh_CN/v2.2.1/)
- [LICENSE-NOTES.md](LICENSE-NOTES.md)

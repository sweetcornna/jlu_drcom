# drcom

面向 OpenWrt 的通用 DrCOM 客户端认证插件服务源码仓库，基于 `dogcom` C 实现，并集成 LuCI 控制面板。

这个仓库的目标不是只存一个现成 `ipk`，而是提供一套可以持续维护、自动构建、直接发布的 OpenWrt 包源码：

- `drcom_openwrt` 兼容二进制
- LuCI 控制面板
- 默认配置模板
- `procd` 服务脚本
- GitHub Actions 多架构自动构建与 Release 发布

## 功能

- 基于 C 实现，适合路由器长期运行
- LuCI 控制面板，支持配置编辑、状态查看、日志查看、错误诊断
- `Save & Restart` 正常工作
- 启动前自动检测并清理 UDP `61440` 端口占用
- 支持旧版 dogcom 风格配置语法
- 增加服务端强制下线冷却识别，避免把 `0x15` 误判成单纯客户端版本错误

## 目录结构

- `drcom/`：OpenWrt 包源码目录。为了兼容当前仓库结构，源码目录仍保留 `drcom/` 这个名字；GitHub Release 产物与路由器安装后的包名为 `drcom_openwrt`
- `drcom/src/`：内置 `dogcom` C 源码
- `drcom/files/`：安装到路由器上的配置、服务脚本、LuCI 页面
- `scripts/generate-openwrt-sdk-matrix.py`：从 OpenWrt 官方 release 自动发现 SDK，并按 `pkgarch` 去重
- `scripts/build-openwrt-sdk-ipk.sh`：使用官方 SDK toolchain 交叉编译，并按已验证的 legacy `tar.gz` 结构打包单个 `ipk`
- `scripts/build-legacy-ipk.py`：构造 legacy OpenWrt `ipk` 外层 `./debian-binary` / `./data.tar.gz` / `./control.tar.gz`
- `.github/workflows/build-ipk.yml`：GitHub Actions 自动构建与发布流程

## 安装

### 直接安装 Release 产物

在 GitHub Release 中下载对应架构的 `ipk`，上传到 OpenWrt / iStoreOS：

文件名里的最后一段就是要选择的架构后缀，例如：

`drcom_openwrt_<version>-<release>_aarch64_generic.ipk`

其中 `aarch64_generic` 就是这台机器要用的版本。最可靠的确认方式是在路由器 SSH 里执行：

```sh
opkg print-architecture | awk '$1=="arch" && $2!="all" && $2!="noarch" {print $2}' | tail -n 1
ubus call system board | jsonfilter -e '@.release.target'
```

第一行输出的是应该下载的 `pkgarch` 后缀；第二行输出的是 OpenWrt 的 `target/subtarget`，可以和下面表格互相核对。常见机器快速对照：

| 机器 / 平台 | 常见 OpenWrt target | 下载文件后缀 |
| --- | --- | --- |
| NanoPi R2S / R4S / R4SE / R5S / R5C / R6S，Orange Pi R1 Plus / R1 Plus LTS 等 Rockchip 64 位板 | `rockchip/armv8` | `aarch64_generic` |
| x86_64 软路由、小主机、虚拟机、Intel N100 / J4125 / 赛扬 / 酷睿 / AMD64 | `x86/64` | `x86_64` |
| 旧 32 位 x86 机器 | `x86/generic` | `i386_pentium4` |
| AMD Geode / very old x86 thin client | `x86/geode` | `i386_pentium-mmx` |
| Raspberry Pi 1 / Zero | `bcm27xx/bcm2708` | `arm_arm1176jzf-s_vfp` |
| Raspberry Pi 2 | `bcm27xx/bcm2709` | `arm_cortex-a7_neon-vfpv4` |
| Raspberry Pi 3 / Zero 2 W / CM3 | `bcm27xx/bcm2710` | `aarch64_cortex-a53` |
| Raspberry Pi 4 / 400 / CM4 | `bcm27xx/bcm2711` | `aarch64_cortex-a72` |
| Raspberry Pi 5 / CM5 | `bcm27xx/bcm2712` | `aarch64_cortex-a76` |
| MT7621 路由器，例如 K2P、新路由 3 / Newifi D2、小米路由器 3G、Redmi AC2100 等 | `ramips/mt7621` | `mipsel_24kc` |
| MT7620 / MT7628 / MT7688 路由器 | `ramips/mt7620` / `ramips/mt76x8` | `mipsel_24kc` |
| MT7622 / MT7981 / MT7986 / Filogic 设备 | `mediatek/mt7622` / `mediatek/filogic` | `aarch64_cortex-a53` |
| MT7623 设备 | `mediatek/mt7623` | `arm_cortex-a7_neon-vfpv4` |
| MT7629 设备 | `mediatek/mt7629` | `arm_cortex-a7` |
| AR71xx / AR9xxx / QCA95xx 迁移到 ath79 的老路由，例如 Archer C7 / WDR 系列 | `ath79/generic` / `ath79/nand` / `ath79/tiny` | `mips_24kc` |
| Qualcomm IPQ40xx，例如 IPQ4018 / IPQ4019 路由 | `ipq40xx/generic` | `arm_cortex-a7_neon-vfpv4` |
| Qualcomm IPQ806x，例如 Netgear R7800 / R7500v2、Linksys EA8500 | `ipq806x/generic` | `arm_cortex-a15_neon-vfpv4` |
| Qualcomm IPQ807x / qualcommax Wi-Fi 6 路由 | `qualcommax/ipq807x` | `aarch64_cortex-a53` |
| Broadcom ARM 路由，例如 Asus RT-AC68U、Netgear R7000 等 bcm53xx 设备 | `bcm53xx/generic` | `arm_cortex-a9` |
| Broadcom bcm47xx 老 MIPS 路由 | `bcm47xx/generic` | `mipsel_mips32` |
| Broadcom bcm47xx mips74k 老路由 | `bcm47xx/mips74k` | `mipsel_74kc` |
| Linksys WRT1900 / WRT3200 等 Marvell mvebu Cortex-A9 设备 | `mvebu/cortexa9` | `arm_cortex-a9_vfpv3-d16` |
| Marvell mvebu Cortex-A53 设备 | `mvebu/cortexa53` | `aarch64_cortex-a53` |
| Marvell mvebu Cortex-A72 设备 | `mvebu/cortexa72` | `aarch64_cortex-a72` |
| Realtek RTL838x 交换机 | `realtek/rtl838x` | `mips_4kec` |
| Realtek RTL839x / RTL930x / RTL931x 交换机 | `realtek/rtl839x` / `realtek/rtl930x` / `realtek/rtl931x` | `mips_24kc` |
| Kirkwood NAS / 老 ARM NAS | `kirkwood/generic` | `arm_xscale` |
| Octeon 设备，例如 EdgeRouter Lite 一类平台 | `octeon/generic` | `mips64_octeonplus` |
| OpenWrt armsr 64 位通用 ARM 系统 | `armsr/armv8` | `aarch64_generic` |
| OpenWrt armsr 32 位通用 ARM 系统 | `armsr/armv7` | `arm_cortex-a15_neon-vfpv4` |
| LoongArch64 设备 | `loongarch64/generic` | `loongarch64_generic` |
| RISC-V D1 设备 | `d1/generic` | `riscv64_riscv64` |

如果你的机器不在表里，以 `opkg print-architecture` 输出为准：下载文件名末尾同名的 `.ipk` 即可。

```sh
opkg install /tmp/drcom_openwrt_*.ipk --force-reinstall
chmod 600 /etc/drcom.conf
/etc/init.d/drcom_openwrt enable
/etc/init.d/drcom_openwrt restart
```

当前 Release 构建目标是 OpenWrt `24.10.7` 这一条 `opkg/ipk` 稳定线。OpenWrt `25.12` 及更新版本已经切换到 `apk` 包管理器，不能直接套用这里的 `opkg install` 流程。

安装后进入：

`LuCI -> 服务 -> DrCOM`

如果路由器上之前安装过旧包名 `jludrcom`，建议先执行：

```sh
opkg remove jludrcom
```

如果之前安装过旧的 `drcom` 包，也建议先移除再装新的 `drcom_openwrt` 包，避免同一配置文件和服务路径的归属混在一起。

### 按校园网要求预先配置 WAN

部分校园网接入场景要求先在 OpenWrt 接口页面里把 `WAN` 配置为**静态地址**，再启动 `drcom` 认证。

LuCI 路径：

`网络 -> 接口 -> WAN -> 编辑 -> 协议: 静态地址`

至少要保证以下几项填写正确：

- `IPv4 address`：填写网络侧分配给你的固定地址，并与 `/etc/drcom.conf` 里的 `host_ip` 保持一致
- `IPv4 netmask`：填写网络侧对应掩码
- `IPv4 gateway`：填写网络侧网关
- `Use custom DNS servers`：填写网络侧要求的 DNS，例如 `10.10.10.10`
- `MAC`：如果网络侧做了绑定，WAN 口 MAC 也要改成绑定后的地址

推荐顺序：

1. 先把 `WAN` 切换成 `静态地址`
2. 保存并应用接口配置
3. 确认能够路由到认证服务器，例如：

```sh
ip route get 10.100.61.3
```

4. 再启动 `drcom_openwrt`

如果这里仍然提示 `Network unreachable`、`到认证服务器没有路由` 或 `Challenge` 一直重试，优先排查的不是账号密码，而是 `WAN` 的静态地址、网关、DNS 和 MAC 是否与网络侧登记信息一致。

### 作为 OpenWrt feed 使用

这里有一个命名差异需要说明：

- GitHub Release 直接安装到路由器上的包名是 `drcom_openwrt`
- 仓库里的 OpenWrt 源码目录和 feed 目标目前仍然叫 `drcom`

因此，下面这组 feed / SDK 源码树命令仍然使用 `drcom`，这是源码侧名字，不是路由器上最终安装后的包名。

```sh
echo 'src-git ymylive_drcom https://github.com/ymylive/drcom.git' >> feeds.conf.default
./scripts/feeds update ymylive_drcom
./scripts/feeds install -p ymylive_drcom drcom
make menuconfig
```

然后在 `Network` 分类里选择源码包 `drcom` 进行编译；最终 Release 产物名仍会是 `drcom_openwrt`。

### 复制包目录到 OpenWrt / SDK

如果你在本地 OpenWrt 源码树里直接测试，也可以把源码目录 `drcom/` 复制到 `package/` 下：

```sh
make package/drcom/compile V=s
```

## 配置

配置文件路径：

`/etc/drcom.conf`

最小必填项：

- `server`
- `username`
- `password`
- `host_ip`
- `mac`
- `profile`，吉林大学推荐 `jlu-modern`

如果不使用 `profile`，则需要手动填写：

- `AUTH_VERSION`
- `KEEP_ALIVE_VERSION`

推荐格式：

```ini
username='your_username'
password='your_password'
server='10.100.61.3'
PRIMARY_DNS='10.10.10.10'
SECONDARY_DNS='8.8.8.8'
host_name='OpenWrt'
host_os='Windows 10'
mac=0xB025AA851014
host_ip='172.18.0.100'
dhcp_server='0.0.0.0'
CONTROLCHECKSTATUS='\x20'
ADAPTERNUM='\x05'
IPDOG='\x01'
profile='jlu-modern'
keepalive1_mod=True
startup_delay_seconds=60
```

如果 `jlu-modern` 被老网关拒绝，可以改成 `profile='jlu-legacy'`；如果你要走普通 dogcom 兼容模式，可以使用 `profile='generic'` 或手动填写 `AUTH_VERSION` / `KEEP_ALIVE_VERSION`。当 `profile` 和手动兼容参数同时出现时，手动写出的 `AUTH_VERSION`、`KEEP_ALIVE_VERSION`、`ror_version`、`jlu_mode`、`startup_delay_seconds` 优先。

`startup_delay_seconds` 用来处理路由器刚重启时认证端残留在线状态的问题。JLU profile 默认设置为 60 秒；`generic` 默认不等待。如果确认优雅下线已经稳定，可以设为 `0` 关闭启动等待。

### 大流量和 UDP 丢包

吉林大学相关实现里明确提到校园网 UDP 丢包会导致心跳包丢失，进而被认证服务器判定离线。本项目现在对 DHCP challenge、logout、keepalive1、keepalive2 的关键 UDP 交换都做了同包超时重发：一次发送后如果 3 秒内没有收到回复，会最多重发 3 次，再把这一轮视为失败。登录正文包不做盲目重发，避免服务器已经登录成功但成功回包丢失时，被重复登录包扰乱状态。

如果日志里出现 `UDP response timeout. Resending packet`，通常说明认证 UDP 在当时有丢包或拥塞；如果出现 `Ignoring unexpected UDP response while waiting for current packet`，说明客户端在等待当前 challenge/keepalive 回复时先收到了陈旧或无关的 UDP 包，已跳过继续等正确回复。大流量下载时可以同时检查：

- OpenWrt 的 CPU/软中断是否打满
- WAN 口是否有错包/丢包
- 交换机或上级网口是否半双工/协商异常
- 校园网侧是否在高峰期丢认证 UDP

### MAC 写法

如果你的物理 MAC 是：

`B0:25:AA:85:10:14`

推荐在配置里写：

`0xB025AA851014`

当前解析器也接受：

`B0:25:AA:85:10:14`

但不要写带空格、带连字符或普通整数格式。

### 校园网接入注意事项

对于需要 DrCOM 认证的校园网场景，优先按下面方式处理：

- 在 OpenWrt 接口页面把 `WAN` 配置成 `静态地址`
- `host_ip` 必须与 `WAN` 的静态地址一致
- 网关 / DNS / MAC 必须与网络侧实际要求一致
- 然后再进行 DrCOM 认证

如果 `Challenge` 阶段一直重试但没有回应，优先检查 WAN 接入方式。

## 前台调试

```sh
/etc/init.d/drcom_openwrt stop
killall drcom_openwrt 2>/dev/null
ss -lunp | grep ':61440'
/usr/bin/drcom_openwrt -m dhcp -c /etc/drcom.conf -e -l /tmp/drcom.log
```

查看日志：

```sh
tail -f /tmp/drcom.log
```

查看运行状态：

```sh
ps w | grep [d]rcom_openwrt
logread | grep -E 'drcom|dogcom|EAP'
```

## GitHub Actions 工作流

当前工作流不再使用 `gh-action-sdk + make package/.../compile` 路线，而是改为和本地已验证结果一致的手工打包路线：

1. `verify`：校验 Lua、内联 JS、配置解析器测试
2. `plan-matrix`：从 OpenWrt 官方 release 页面自动发现 target / subtarget，并从 `packages/Packages.gz` 提取 `Architecture`，按 `pkgarch` 去重
3. `build`：对每个唯一 `pkgarch` 使用官方 SDK toolchain 交叉编译，并按已验证的 legacy `tar.gz` 兼容格式打包
4. `release`：在标签构建时直接发布多个 `.ipk` 和 `.sha256`

### 发布产物规则

- 每个架构单独生成一个 `ipk`
- 每个 `ipk` 对应一个 `.sha256`
- Release **直接上传这些文件**
- **不会**再额外打一个 zip / tar.gz 包

产物命名格式：

- `drcom_openwrt_<version>-<release>_<pkgarch>.ipk`
- `drcom_openwrt_<version>-<release>_<pkgarch>.ipk.sha256`

## 支持架构

本地已经重点验证：

- `aarch64_generic`（NanoPi R2S / `rockchip/armv8`）

GitHub Actions 会从 OpenWrt 官方 `24.10.7` release 自动发现并去重所有可用 `pkgarch`，因此最终 Release 会直接包含该版本官方 SDK 能构建出的多架构 `ipk`。Release 文件名遵循：

`drcom_openwrt_<version>-<release>_<pkgarch>.ipk`

完整后缀对照如下：

| 下载文件后缀 / `pkgarch` | 对应示例 target/subtarget | 常见平台说明 |
| --- | --- | --- |
| `aarch64_cortex-a53` | `bcm27xx/bcm2710`、`mediatek/filogic`、`mediatek/mt7622`、`qualcommax/ipq807x`、`mvebu/cortexa53`、`sunxi/cortexa53` | Cortex-A53 64 位 ARM 平台，树莓派 3、Filogic、部分 IPQ807x / mvebu / sunxi |
| `aarch64_cortex-a72` | `bcm27xx/bcm2711`、`mvebu/cortexa72` | Cortex-A72 64 位 ARM 平台，树莓派 4 / CM4 等 |
| `aarch64_cortex-a76` | `bcm27xx/bcm2712` | Cortex-A76 64 位 ARM 平台，树莓派 5 / CM5 等 |
| `aarch64_generic` | `rockchip/armv8`、`armsr/armv8`、`layerscape/armv8_64b` | 通用 ARMv8 64 位平台，R2S / R4S / R5S / R6S 等 Rockchip 设备 |
| `arm_arm1176jzf-s_vfp` | `bcm27xx/bcm2708` | ARM1176 平台，树莓派 1 / Zero |
| `arm_arm926ej-s` | `at91/sam9x`、`mxs/generic` | ARM926EJ-S 老平台 |
| `arm_cortex-a15_neon-vfpv4` | `armsr/armv7`、`ipq806x/generic` | Cortex-A15 / Krait 级 32 位 ARM，IPQ806x 等 |
| `arm_cortex-a5_vfpv4` | `at91/sama5` | Cortex-A5 平台 |
| `arm_cortex-a7` | `mediatek/mt7629` | MT7629 等 Cortex-A7 平台 |
| `arm_cortex-a7_neon-vfpv4` | `bcm27xx/bcm2709`、`ipq40xx/generic`、`mediatek/mt7623`、`sunxi/cortexa7`、`layerscape/armv7` | Cortex-A7 32 位 ARM，树莓派 2、IPQ40xx、MT7623 等 |
| `arm_cortex-a7_vfpv4` | `at91/sama7` | SAMA7 等 Cortex-A7 平台 |
| `arm_cortex-a8_vfpv3` | `omap/generic`、`sunxi/cortexa8` | Cortex-A8 平台 |
| `arm_cortex-a9` | `bcm53xx/generic` | Broadcom bcm53xx ARM 路由 |
| `arm_cortex-a9_neon` | `imx/cortexa9` | i.MX Cortex-A9 平台 |
| `arm_cortex-a9_vfpv3-d16` | `mvebu/cortexa9` | Marvell mvebu Cortex-A9，Linksys WRT 系列等 |
| `arm_fa526` | `gemini/generic` | Gemini / FA526 老 ARM 平台 |
| `arm_xscale` | `kirkwood/generic` | Kirkwood NAS / 老 ARM NAS |
| `armeb_xscale` | `ixp4xx/generic` | 大端 XScale 老平台 |
| `i386_pentium-mmx` | `x86/geode` | AMD Geode / very old x86 |
| `i386_pentium4` | `x86/generic` | 32 位 x86 |
| `loongarch64_generic` | `loongarch64/generic` | LoongArch64 平台 |
| `mips64_mips64r2` | `malta/be64` | 64 位大端 MIPS Malta |
| `mips64_octeonplus` | `octeon/generic` | Cavium Octeon 平台 |
| `mips64el_mips64r2` | `malta/le64` | 64 位小端 MIPS Malta |
| `mips_24kc` | `ath79/generic`、`ath79/nand`、`ath79/tiny`、`realtek/rtl839x`、`realtek/rtl930x`、`realtek/rtl931x` | 大端 MIPS 24Kc，ath79 路由和部分 Realtek 交换机 |
| `mips_4kec` | `realtek/rtl838x` | RTL838x 交换机 |
| `mips_mips32` | `bmips/bcm6318` | 大端 MIPS32 老平台 |
| `mipsel_24kc` | `ramips/mt7620`、`ramips/mt7621`、`ramips/mt76x8`、`ramips/rt288x`、`ramips/rt305x` | 小端 MIPS 24Kc，MT7620 / MT7621 / MT7628 / MT7688 常见路由 |
| `mipsel_24kc_24kf` | `pistachio/generic` | 小端 MIPS 24Kc/24Kf 平台 |
| `mipsel_74kc` | `bcm47xx/mips74k`、`ramips/rt3883` | 小端 MIPS 74Kc 平台 |
| `mipsel_mips32` | `bcm47xx/generic` | bcm47xx 老 MIPS 路由 |
| `powerpc64_e5500` | `qoriq/generic` | PowerPC64 e5500 平台 |
| `powerpc_464fp` | `apm821xx/nand` | APM821xx / PowerPC 464FP |
| `powerpc_8548` | `mpc85xx/p1010`、`mpc85xx/p1020`、`mpc85xx/p2020` | MPC85xx / PowerPC 8548 |
| `riscv64_riscv64` | `d1/generic` | RISC-V 64 位平台 |
| `x86_64` | `x86/64` | 64 位 x86 软路由、小主机、虚拟机 |

如果后续要切换到另一个 `opkg/ipk` 版本，只需要改 `.github/workflows/build-ipk.yml` 里的 `OPENWRT_RELEASE`。如果要支持 OpenWrt `25.12+`，需要新增 `apk` 打包路径，而不是只改版本号。

## 本地复现单架构打包

如果你已经有一个官方 SDK，也可以直接复用仓库脚本本地打包：

```sh
bash scripts/build-openwrt-sdk-ipk.sh \
  --package-root ./drcom \
  --release 24.10.7 \
  --target rockchip \
  --subtarget armv8 \
  --sdk-root /path/to/openwrt-sdk-24.10.7-rockchip-armv8_gcc-13.3.0_musl.Linux-x86_64 \
  --output-dir ./dist/aarch64_generic
```

这个脚本当前输出的文件名为：

- `drcom_openwrt_<version>-<release>_<pkgarch>.ipk`
- `drcom_openwrt_<version>-<release>_<pkgarch>.ipk.sha256`

## 许可证

- C 客户端源码基于上游 `dogcom`
- 仓库保留上游 AGPL 许可证文本
- OpenWrt / LuCI 适配和控制面板改造在本仓库继续维护

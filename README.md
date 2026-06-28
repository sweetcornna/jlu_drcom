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

本地已经验证：

- `aarch64_generic`（R2S / `rockchip/armv8`）

GitHub Actions 会从 OpenWrt 官方 `24.10.7` release 自动发现并去重所有可用 `pkgarch`，因此最终 Release 会直接包含该版本官方 SDK 能构建出的多架构 `ipk`。

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

# 安装 xdb

[English](INSTALL.md) | 简体中文

## 桌面运行包

从 [`packages` 工作流产物](https://github.com/YJBeetle/XDeviceBridge/actions/workflows/packages.yml)
或[带版本标签的 GitHub Release](https://github.com/YJBeetle/XDeviceBridge/releases)
下载适合操作系统和 CPU 架构的运行包。独立的 `*-sources` 包包含对应源码和构建信息，
运行 xdb 不需要下载它。使用 `*-SHA256SUMS.txt` 校验归档文件。

运行包包含 `bin/xdb`（Windows 为 `xdb.exe`）、客户端和服务端库、精简的 FFmpeg
动态库，以及依赖许可证。请保留完整目录结构，不要只复制可执行文件。

### macOS 和 Linux

解压运行包，进入解压后的目录，然后执行：

```sh
./install.sh
export PATH="$HOME/.local/bin:$PATH"  # 如果该目录尚未加入 PATH
xdb devices -l
```

安装脚本会先验证并暂存新运行包，再安装到 `~/.local/opt/xdb`，并创建 `~/.local/bin/xdb` 符号链接。
不需要管理员权限，也不会修改 shell 配置文件。如有需要，请自行把上述 PATH 设置
加入 shell 配置，以便新终端继续使用。

也可以指定自定义安装目录和命令目录，两者应为绝对路径：

```sh
./install.sh /absolute/path/to/xdb /absolute/path/to/bin
```

macOS 包面向 macOS 13.3 或更新系统，CPU 架构需与运行包一致。
普通 `packages` CI 产物使用 ad hoc 签名；单独标注 `notarized` 的运行 ZIP 使用
Developer ID 签名并通过 Apple 公证。裸 CLI ZIP 的公证票据通过联网查询，不支持 staple。

Linux 包在 Ubuntu 24.04 上构建，需要兼容的 glibc/libstdc++ 运行库和系统 zlib，
并非适用于所有 Linux 发行版的通用二进制。Linux USB 访问还需要 usbmuxd。

### Windows

将整个运行包解压到固定位置，例如 `C:\Tools\xdb`。
在系统设置中，把 `C:\Tools\xdb\bin` 加入**用户 PATH**，再打开新终端：

```powershell
xdb devices -l
```

初始 Windows 包面向 x86-64，已包含 MinGW 运行库 DLL。
USB 需要 Apple 移动设备驱动和服务；Wi-Fi 配对不依赖该 USB 驱动。

## scrctl 和 MaaFramework

两者的 SDK 客户端会从 PATH 查找已安装的 `xdb`，并在需要时自动启动 Server。
如果应用使用不同的 PATH（例如从 Finder 启动的 GUI），可在应用环境中用
`XDB_EXECUTABLE` 指定完整路径：

```sh
export XDB_EXECUTABLE="$HOME/.local/opt/xdb/bin/xdb"
```

下游应用也可以随客户端库一起分发完整运行包，保留同样的 `bin/` 和 `lib/` 布局，
并保留依赖许可证。Android 使用 [Android 集成说明](ANDROID.md)中的内嵌 Server
方式，桌面安装包不是 Android 应用安装包。

更新由 install.sh 管理的安装时，直接在新包中执行同样的安装命令即可：脚本先在同一
文件系统暂存新目录并运行 `--help`，再调用旧安装的 `kill-server`，整体替换目录，
清除旧包遗留文件；切换失败会恢复旧目录。停止服务失败时安装不变，并提示使用原版
xdb 停止。脚本拒绝覆盖其他命令或非自身管理的目录，也不会更改 shell 配置。
更新会断开正在使用该服务的客户端，请先结束任务。`XDB_PORT` 可指定需停止的服务端口；
其他端口的服务需分别停止。新版本在下次客户端连接时启动，失败回滚后也需重新连接。
安装不会删除用户数据目录中的配对记录或 DDI 缓存。

手动安装或 Windows 更新：先用原安装的 `xdb kill-server` 停止服务，保留完整旧目录，
将新包解压到新的空目录，运行 `bin/xdb --help` 后更新 PATH / `XDB_EXECUTABLE`。
启动失败时恢复旧路径。不在已加载的 DLL 上原地覆盖。协议不兼容时客户端会拒绝连接，
不会自动杀死旧服务；具体兼容约定见 [PROTOCOL.md](PROTOCOL.md)。

卸载用户目录中的运行包时，先停止 Server，再删除 `~/.local/bin/xdb` 符号链接
和 `~/.local/opt/xdb` 安装目录。

## 从源码构建安装包

原生发行构建需要：C++20 编译器、CMake >= 3.21、Ninja、Make、Perl、Python >= 3.10、
pkg-config、curl、xz 和 zlib 开发头文件。x86-64 构建还需要 NASM，Linux 打包需要
patchelf，Windows 使用 MSYS2 UCRT64 GCC。各平台软件包清单见工作流。

```sh
tools/packaging/dependencies.sh "$PWD/build-release-deps"
export PKG_CONFIG_PATH="$PWD/build-release-deps/lib/pkgconfig"
cmake -C build-release-deps/xdb.cmake -S . -B build-release -G Ninja \
  -DCMAKE_BUILD_TYPE=Release -DXDB_BUILD_TESTS=OFF
cmake --build build-release --parallel 4
# 打包前提交修改，以保证归档源码与二进制对应。
python3 tools/packaging/package.py --build build-release \
  --dependencies build-release-deps --output build-packages
```

依赖构建脚本下载固定版本的 OpenSSL/curl/FFmpeg 源码，并校验 SHA256。
OpenSSL 和 curl 静态链接，FFmpeg 使用动态库，不开启 GPL 或 nonfree 选项，
只启用 xdb 所需的解码器和封装器。macOS 使用系统 curl。
除了平台系统库和 Windows 编译器/zlib 运行库，打包时会拒绝引入指定依赖目录之外
的意外依赖。

运行包与源码包独立生成。源码材料包含确切的仓库提交、获取的依赖源码、上游源码归档、
配置和构建信息，以及文件哈希。构建缓存和含绝对路径的来源信息仅保存在源码包中。

## macOS 签名公证运行包

维护者可在 main 手动运行 `macOS signed and notarized package` 工作流。
使用 `Build` Environment，Secrets：`MACOS_CERTIFICATE_P12_BASE64`、
`MACOS_CERTIFICATE_PASSWORD`、`NOTARY_APPLE_ID`、`NOTARY_APP_PASSWORD`；
Variables：`SIGNING_IDENTITY`、`SIGNING_TEAM_ID`。
P12 包含 Developer ID Application 私钥；NOTARY_APP_PASSWORD 为 App 专用密码。

任务在安装名改写完成后给程序及动态库签名，并在签名后记录运行包与源码材料哈希。
主程序启用 Hardened Runtime，使用安全时间戳。临时签名钥匙串自动删除。
仅在 Apple 返回 Accepted 后上传 `xdb-macos-notarized` 运行 ZIP、对应源码及校验和；
公证响应另作为 `macos-notarization-result` 附件保留。任务不自动发布 Release。
普通 packages 工作流仍为 ad hoc 开发包。ZIP 和裸 CLI 不能 staple，
Gatekeeper 需联网查询票据；若需离线票据，应另行分发可 staple 的 DMG/PKG。

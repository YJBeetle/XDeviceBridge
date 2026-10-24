# Android 宿主接入

[English](ANDROID.md) | 简体中文

## 推荐的初期模型

```text
Android app (UI / lifecycle / permissions)
  ├─ loads libxdb_server.so
  │    └─ xdb_server_start(0, app-private-files-dir)
  └─ MaaFramework / scrctl client
       └─ libxdb_client.so → same loopback protocol
```

如应用生命周期需要，可由 Android Service 承载原生 Server。
XDeviceBridge 本身不安装系统守护进程、不启动 Java 服务，也不自行申请权限。
每个应用使用独立 Server 和私有凭据存储；MaaFramework 不接收设备配对密钥。
应用只需提供绝对、私有、持久的数据目录，例如 `Context.getFilesDir()` 下的子目录。

通过 `xdb_server_port()` 为客户端配置端口。本机连接不需要访问令牌，
iPhone 配对记录仍由 Server 持有。结束时，先中断并等待客户端请求，
销毁客户端，再停止 Server。停止服务会等待工作线程；
不要让 Android UI 线程等待设备服务的截止时间。
在全部服务端工作线程停止前，服务库必须保持加载状态。

Android 禁用桌面式自动进程启动。若客户端请求关闭服务，
`xdb_server_is_running()` 会变为 false；App 仍持有句柄，
须调用 `xdb_server_stop()` 等待工作线程并释放它。

UI 通过调用与 `xdb pair --wifi` 相同的 `pair` 协议配对，并显示 PIN 事件。
Server 负责加密、重连验证和存储。
从 adb shell 运行独立 CLI 可用于诊断，但不能据此证明普通 Android 应用
也能以相同方式启动原生可执行文件。

## 为什么不直接执行应用目录中的 xdb？

对目标 API 29+ 的应用，Android 10+ 限制执行应用可写主目录中的文件。
当前 MaaFramework 的 `ChildPipeIOStream` 回退在 Android 上也会报告
`Boost.Process is not available in this build`。
内嵌 Server C ABI 不依赖这条进程启动路径。

- [Android 10 执行限制](https://developer.android.com/about/versions/10/behavior-changes-10#execute-permission)
- [Android 本地网络权限](https://developer.android.com/privacy-and-security/local-network-permission)

宿主应用须按目标 SDK 提供 Internet／局域网权限，在需要时申请运行时本地网络权限，
处理后台执行，并为设备和网络安排 mDNS／多播访问。
`ACCESS_LOCAL_NETWORK` 适用于目标 Android 17/API 37+ 的应用；
原生套接字代码不能绕过 Android 权限策略。
初期传输方式是 Wi-Fi；Android USB Host 访问需要显式实现 Android 适配层。

## NDK 构建

仅客户端，无设备后端依赖：

```sh
cmake -S . -B build-android-client -G Ninja \
  -DCMAKE_TOOLCHAIN_FILE="$ANDROID_NDK_ROOT/build/cmake/android.toolchain.cmake" \
  -DANDROID_ABI=arm64-v8a -DANDROID_PLATFORM=android-24 \
  -DXDB_BUILD_SERVER=OFF -DXDB_BUILD_CLI=OFF
cmake --build build-android-client
```

arm64 依赖配方为 `tools/android-deps.sh <prefix>`，构建固定版本的
OpenSSL/curl 和 HEVC/PNG FFmpeg 库，并输出 Server 配置参数。
这份配方禁用音频和容器支持。
通过 NDK 搜索根和 CMake 包变量提供目标平台的 OpenSSL/curl、pugixml 及可选 FFmpeg。
交叉编译不使用宿主 pkg-config 元数据。
HEVC 需要 avcodec/avutil/swscale，音频解码还需 swresample。
不能使用其他架构的目标库。初期已验证的编译目标为 Android API 24
（设备发现需要 `getifaddrs`）。

## 性能与验证范围

解码后传输 BGRA 便于 MaaFramework 使用，但一张 1080×2400 帧约为 10.4 MB；
30 帧／秒会让约 311 MB/s 的像素负载经过回环，还不包括额外复制。
需要在实际设备上测量。最新帧轮询限制队列长度；
未来若增加编码 HEVC 或共享内存帧，应显式协商，不能改变已有 BGRA 响应的含义。

本地交叉构建只验证目标编译和链接。
目前尚无在线 Android 设备完成应用沙箱、后台生命周期、权限和 iPhone 端到端验收。
MaaFramework 已有的 Android Native 接入支持加载应用提供的原生库，
可作为生命周期组织的参考；这里不复用或修改其 `GetLockedPixels`／`DispatchInputMessage` ABI。

# XDeviceBridge (`xdb`)

A cross-platform library and command-line bridge for iPhone/iPad device discovery,
pairing, developer-image preparation, capture and input. No jailbreak, WebDriverAgent
or on-device automation application is required for the implemented services.

**Early development:** the protocol and API are experimental. This first import
has local macOS tests and Android cross-build checks; it is not a stable release.
scrctl and MaaFramework are intended consumers and have not yet switched to this backend.

## Architecture

```text
xdb CLI / scrctl / MaaFramework
              |
     libxdb_client (C API)
              |
    authenticated loopback protocol
              |
           xdb Server
              |
     USB / paired Wi-Fi device
```

The client links only sockets, JSON and platform runtime libraries. OpenSSL,
DDI, media decoding and device protocol implementations stay inside the server.
The server runs as a standalone process on desktop or as `libxdb_server` hosted by
an application. Android applications can start the same server on a native thread;
the client still uses the same protocol.

The server owns device pairing records. Its API returns a UDID, never the private
pairing material. Each Android application supplies its own private data directory;
there is no shared system daemon or cross-application credential store.

## Build

C++20 (including `std::jthread`/`std::stop_token`), CMake >= 3.20, OpenSSL and curl
are required for the server. nlohmann-json, pugixml, lwIP and mDNS use pinned source
versions when not supplied locally. FFmpeg video decoding is optional; Apple also
has VideoToolbox. FFmpeg audio decoding additionally needs libswresample.
There is no SDL, MaaUtils or OpenCV dependency.

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j 4
ctest --test-dir build --output-on-failure
```

A downstream application can build only the lightweight client:

```sh
cmake -S . -B build-client -DXDB_BUILD_SERVER=OFF -DXDB_BUILD_CLI=OFF
cmake --build build-client
```

See [Android integration](docs/ANDROID.md) for the NDK and embedded-server model.
The C ABI is in [xdb.h](include/xdb/xdb.h); the wire protocol is documented in
[PROTOCOL.md](docs/PROTOCOL.md).

## Command-line use

The initial version uses an explicit foreground server. Automatic `start-server`
and background process supervision are not implemented yet. Default port: 17423.
Server output contains a private endpoint/token: do not publish that line.

```sh
# Terminal 1. --data-dir is optional; defaults to the platform user data directory.
xdb server --port 17423

# Terminal 2. Use the token printed by the server.
export XDB_TOKEN='<server token>'
xdb devices -l
xdb pair --wifi
# Select the advertised computer in the phone's Developer Mode pairing page.
# Enter the one-time code printed by xdb on the phone.
xdb pair --usb
xdb -s '<UDID>' ddi prepare --wifi
xdb -s '<UDID>' screenshot screen.png --wifi
```

`--wifi` without an address discovers paired devices; `--wifi ADDRESS` selects a
RemotePairing address explicitly. Without `--wifi`, device operations use USB.
USB requires a working system usbmux/Apple device service and device trust.
Android initially supports Wi-Fi; Android USB Host integration is not implemented.

The CLI has an explicit migration command for existing credentials:

```sh
xdb pairing import /path/to/remote-device.pair
```

The file is copied into the server's data directory; consumers do not subsequently
need the original file or any pairing keys. No credentials are imported automatically.

For protocol development, `xdb rpc METHOD JSON_PARAMS` and `xdb rpc-shell` expose
raw requests. `rpc-shell` maintains a client session; it is not an iOS command shell.

## Initial scope and limitations

- Discovery, USB/Wi-Fi pairing, explicit pairing import and DDI download/preparation.
- One device session per client connection; a known UDID has one owning connection.
- PNG screenshots; decoded latest-frame BGRA video with display geometry metadata.
- Single-contact panel-normalized touch, keyboard usages, ASCII text, consumer keys and app control.
- Binary media is framed separately from JSON, never Base64-encoded. Only one outstanding
  request per connection; frame polling returns the latest/newer frame rather than queuing all frames.
- Some operations can already have changed the phone when a transport failure is reported.
  Do not automatically retry input, pairing or image replacement after an unknown outcome.
- Cancellation uses client disconnection or server shutdown. Existing device service calls
  may finish at their own deadlines; immediate cancellation of every service is not promised.
- Audio and recording core code are retained from the source project, but are not exposed
  in the initial public protocol. Encoded video, automatic server startup and consumer
  migrations remain follow-up work.
- Cross-build success does not prove Android application packaging, lifecycle, permission
  handling or physical-device behavior. See [validation](docs/VALIDATION.md).

## License

Project-owned code is licensed under **LGPL-3.0-only**, as authorized by its copyright
owner. See [LICENSE](LICENSE) and [COPYING.GPLv3](COPYING.GPLv3).
Third-party components retain their own licenses; the license of a distributed binary
also depends on its exact dependency build, especially FFmpeg.
See [third-party components](THIRD_PARTY_NOTICES.md) and [source origin](docs/ORIGIN.md).

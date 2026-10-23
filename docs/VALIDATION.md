# Validation

The initial implementation was checked locally on macOS:

- Full native CLI, server and client build.
- Existing derived JSON/plist/HID/DDI-selection offline tests.
- A real local TCP server exercised by the C client, CLI and fragmented raw messages.
- Protocol version rejection, oversized message rejection, invalid requests,
  no persistent state after invalid imports, and shutdown with an idle client.
- A separate client-only build.
- Android arm64 client, server and CLI compilation/linkage with API 24. The server
  includes target-built HEVC libraries; the client ELF has no device/media dependencies.

Android arm64 client/server builds are cross-compilation checks, not application
or physical-device acceptance. They use Android API 24 and target-built dependencies.
No new successful physical-device behavior is implied by the scrctl source origin.

Before a stable release: complete physical-device acceptance for pairing, DDI
preparation, input, audio and recording through the new protocol, and validate
Android application packaging and lifecycle. Desktop distribution checks and
consumer capture evidence are recorded below.

## Consumer migration checks

- 38 native backend/IPC tests pass, covering transport cancellation, HTTP/2/XPC,
  pairing identity, DDI selection/download/TSS/mount fixtures, RTP/frame geometry,
  audio decoding/regulation and recording finalization.
- MaaFramework's real C ABI passes deterministic server tests for discovery,
  BGRA/PNG capture and fallback, crop/rotation, touch/key/text and application RPCs.
  Python's optional-symbol and management boundaries also pass offline tests.
- scrctl's actual executable passes deterministic server tests for background
  capture, explicit clipboard, ASCII, attached PCM delivery and encoded recording.
- MaaFramework's Android arm64 control unit and Toolkit cross-build without device
  cryptography/media dependencies. This is not Android runtime acceptance.
- An existing authorized scrctl credential was imported into a disposable server
  directory. Wi-Fi discovery found the iPhone, but pair-verify rejected the record
  because the device Identifier did not match. PNG/video/input acceptance through
  the new server remains unverified; no device pairing or DDI was changed.

An existing PiCli record was converted in a temporary private copy and successfully
verified against iPhone14,4 / iOS 27.0.1 over Wi-Fi. The server returned a PNG and
decoded a 1136×2464 HEVC frame. The real scrctl executable then consumed two frames
headlessly, and the real MaaFramework C ABI captured through both Stream and
ScreenshotService (cropped native display 1125×2436). This did not test input,
audio listening, fresh pairing, DDI installation or Android device runtime.

Pairing label changes additionally pass the host metadata/serial, pairing workflow
and IPC tests; Android arm64 server recompilation succeeds. These do not assert
that an already paired iPhone updates its cached display labels.

Desktop lifecycle tests exercise real background processes on macOS: CLI and SDK
automatic startup, persistence after client exit, idempotent start/kill, concurrent
first starts, immediate restart, and refusal to replace a non-xdb listener. Initial
Windows launcher checks were cross-compilation; native desktop package checks
are recorded below. The Android client is rebuilt with process startup disabled;
this remains a cross-build check, not Android runtime acceptance.


## Desktop distribution checks

The [2026-10-24 package workflow](https://github.com/YJBeetle/XDeviceBridge/actions/runs/39997769147)
passed at commit `06019f9` on Ubuntu 24.04 x86-64, macOS 15 arm64 and
Windows 2025 x86-64. Each native job:

- Builds pinned server dependencies and the CLI/client/server libraries.
- Produces separate runtime and corresponding-source archives, with license texts
  in the runtime and build caches/provenance confined to the source archive.
- Relocates the runtime to a directory containing spaces and exercises real SDK
  startup through both sibling-bin lookup and a separately shipped client using PATH.
- Exchanges `server.info` through the C API and stops the background server;
  Windows additionally waits for process exit before temporary-directory cleanup.
- Checks that bilingual installation instructions are present and that the runtime
  contains no static archives or CMake build cache.

Local macOS checks also covered user-directory installation, command lookup and
runtime archive SHA256 verification. macOS signatures are ad hoc; Apple notarization
and execution on the minimum deployment OS were not tested. These packaging checks
use isolated data directories and do not exercise physical-device services.

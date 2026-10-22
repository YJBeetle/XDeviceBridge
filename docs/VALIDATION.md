# Validation

The initial implementation was checked locally on macOS:

- Full native CLI, server and client build.
- Existing derived JSON/plist/HID/DDI-selection offline tests.
- A real local TCP server exercised by the C client, CLI and fragmented raw messages.
- Authentication/version rejection, oversized message rejection, invalid requests,
  no persistent state after invalid imports, and shutdown with an idle client.
- A separate client-only build.
- Android arm64 client, server and CLI compilation/linkage with API 24. The server
  includes target-built HEVC libraries; the client ELF has no device/media dependencies.

Android arm64 client/server builds are cross-compilation checks, not application
or physical-device acceptance. They use Android API 24 and target-built dependencies.
No new successful physical-device behavior is implied by the scrctl source origin.

Before a stable release: validate actual iPhone pairing, DDI preparation, PNG/video
geometry and input through the new protocol; test desktop/Android packaging and
lifetimes; add CI for supported targets; migrate consumers after protocol review.

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

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

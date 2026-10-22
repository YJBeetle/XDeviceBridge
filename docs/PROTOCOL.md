# xdb local protocol (experimental version 1)

This is the **client-to-host-server protocol**, not Apple's device protocol.
The first implementation listens only on IPv4 loopback. Network-accessible host
servers, service installation, plugins and remote deployment are outside its scope.

## Framing

Each message is a 16-byte header followed by UTF-8 JSON and optional raw binary:

| Offset | Size | Meaning |
| --- | --- | --- |
| 0 | 4 | ASCII `XDB1` |
| 4 | 4 | Version `1`, unsigned big endian |
| 8 | 4 | JSON bytes, unsigned big endian, 1..1 MiB |
| 12 | 4 | Binary bytes, unsigned big endian, 0..64 MiB |

Reads must tolerate TCP fragmentation and coalescing. JSON nesting is limited to
64 levels. Bad framing closes the connection before allocating oversized payloads.
Client requests currently carry no binary body. Never search for newlines to delimit messages.

## Handshake and calls

The first request must be:

```json
{"id":1,"method":"hello","params":{"protocol":1,"token":"<private server token>"}}
```

Authentication/version failure closes the connection. The library performs this handshake.
Then send one request at a time, with a unique unsigned integer `id` and object `params`:

```json
{"id":2,"method":"server.info","params":{}}
```

Responses use `{"id":2,"ok":true,"result":{...}}` or
`{"id":2,"ok":false,"error":{"code":"...","message":"..."}}`.
During a request, zero or more messages of the form
`{"id":2,"event":"progress","data":{...}}` may precede its response.
Pairing PIN events use `event: "pin"`, `data: {"pin":"123456"}` and must not be logged.
The server does not send credentials in responses, events or discovery results.

Disconnecting cancels pairing/DDI where supported and destroys the connection's
session. Cancelled or failed operations may have already changed device state.
The client can interrupt its socket from another thread; wait for request completion
before destroying that client. Event callbacks cannot reenter or destroy their client.

## Methods

| Method | Parameters | Result / binary |
| --- | --- | --- |
| `server.info` | none | Protocol and server version |
| `devices` | `transport`: auto/usb/wifi; `timeout_ms`: 0..60000 | Devices, candidates, warnings |
| `pair` | `transport`: wifi/usb; optional `udid`; `timeout_ms`: 1..300000 | UDID and reused status; progress/PIN events |
| `pairing.import` | `record`: bounded xdb or scrctl pairing text | Validated/imported UDID; keys remain in server |
| `ddi.download` | `os_version`, optional `product_type`, `cache_directory`, `timeout_seconds` | Build/cache metadata; progress events |
| `ddi.prepare` | `udid`, optional `wifi`, `port`, `cache_directory`, `replace`, download/install timeout seconds | Prepared status; progress events |
| `connection.open` | `udid`, optional `wifi`: auto/address, `port`: device port | Acquires a connection-owned session |
| `connection.close` | none | Releases services, inputs and device lease |
| `device.info` | none | Device UDID, OS version, model |
| `screenshot` | none | `format: png`; raw PNG body |
| `video.start` | optional `hardware_decode` | Starts background latest-frame capture |
| `video.frame` | `since`: frame serial, `timeout_ms`: 0..1000 | Format BGRA, serial, width, height, row pitch, optional display geometry; raw pixels |
| `video.stop` | none | Stops the video pump |
| `input.tap` | `x`, `y`: 0..1; `hold_ms`: 0..2000 | Report delivery status |
| `input.touch` | `x`, `y`: 0..1; `down`: bool | Contact/release report delivery status |
| `input.keyboard` | `usages`: USB HID keyboard usages, each <240 | Replaces the held keyboard usage set; `[]` releases |
| `input.button` | `page` (default 12), `usage`, `down` | Hardware consumer-key down/up |
| `input.text` | `text`: US-keyboard ASCII/tab/newline, <=4096 bytes | Unsupported text is rejected in full |
| `app.list` | none | Bundle identifiers and names |
| `app.launch` | `bundle_id`, optional `terminate_existing` | Request status |
| `app.stop` | `bundle_id` | Request status |

Except management methods, operations require a device session. A disconnected
client cannot attach to an old session. For now, use one long-lived library client
or `rpc-shell` for a multi-operation session.

## Geometry and media ownership

Touch coordinates refer to the device's panel axes, not a rotated/cropped client
window. BGRA frame dimensions can include codec padding. `display` metadata comes
from the device display subscription and contains width, height, orientation and
geometry serial. Clients must account for crop/rotation when mapping visible pixels
to panel coordinates; absent metadata must not be invented.

A response owns its pixels until `xdb_response_destroy`. There is no FFmpeg,
OpenCV, C++ object or GPU handle in the C ABI. Future encoded media negotiation
must be explicit; the current implementation does not send HEVC/AAC payloads.

## Consumer sessions and additional methods

`connection.open` accepts `transport` (`auto`, `usb`, `wifi`) and returns a random,
connection-private `attachment` capability. An authenticated client may call
`connection.attach` with it on another socket, allowing audio/video polling and
control to proceed independently. Keep this capability private. The session and
device lease live until its last attached connection closes. `connection.close`
only detaches the requesting client; it does not invalidate another attachment.

- `display.info`: subscribes to display updates and returns panel geometry.
- `display.rotate`: queries the current cardinal orientation, requests the opposite
  portrait/landscape orientation once, and verifies the returned state. It does not
  unlock the device's orientation policy or retry an uncertain mutation.
- `clipboard.set` / `clipboard.get`: explicit clipboard access (`text`, <=256 KiB
  on write). This is not automatic text-input fallback.
- `recording.configure`: before media starts, provide an absolute server-host `path`,
  `format` (`mp4`/`mkv`), `video`, `audio`, `orientation`, and optionally
  `orientation_from_device`. The file is written by the server. Container media
  remains encoded; it is not reconstructed from the IPC image/audio bodies.
- `video.start`: also accepts `decode` (default true), `record_path` (raw HEVC,
  absolute server path) and `restart_on_silence` (default true). Encoded-only capture
  must become ready with a checked recording IDR before this call succeeds.
- `video.frame`: `allow_cached` permits reuse of a healthy latest frame after its
  wait expires. Geometry is captured with the published frame, not read later.
- `screenshot`: `format: "bgra"` requests server-side PNG decoding; the default
  remains PNG. Both include geometry; a changing display fails the capture.
- `audio.start`: `decode`, `duplicate`, `buffer_ms` (0..1000); returns sample rate,
  channels and preroll frames. `audio.read` requests 1..48000 interleaved frames,
  returning raw **signed 16-bit little-endian** PCM and its actual frame count.
  Poll on a dedicated attached socket; never make a network call in a sound callback.
- `audio.stop`: stops audio only. `media.finish`: joins producers, finalizes
  recordings, then stops device audio. Errors propagate to the caller.
- `media.status`: terminal `error` and `video_unusable` (for screenshot fallback).
- `network.debug`: `enabled` controls server-side tunnel diagnostics.

`devices` includes `capabilities.video_decode`; clients should derive Stream
availability from the server, not their local libraries. In the C API, port 0
uses `XDB_PORT` or 17423 and an empty token uses `XDB_TOKEN`. No process is started
implicitly. `include/xdb/client.hpp` is an optional header-only C++ adapter; the
binary boundary remains the C API. GUI clients can use that API directly for
`pair` / `ddi.prepare`; device credentials never leave the server.

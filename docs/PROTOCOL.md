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

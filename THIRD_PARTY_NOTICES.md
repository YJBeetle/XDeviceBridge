# Third-party components

Project-owned XDeviceBridge code is LGPL-3.0-only. The following dependencies retain
upstream licenses. A source checkout fetches pinned sources or uses installed
packages; it does not relicense those packages.

| Component | Use | License / upstream |
| --- | --- | --- |
| nlohmann-json | JSON | MIT — https://github.com/nlohmann/json |
| pugixml | XML plist | MIT — https://github.com/zeux/pugixml |
| lwIP | Userspace IPv6/TCP stack | BSD-style — https://github.com/lwip-tcpip/lwip |
| mDNS | Discovery/advertisement | Unlicense — https://github.com/mjansson/mdns |
| OpenSSL | TLS and pairing crypto | Apache-2.0 — https://www.openssl.org/source/license.html |
| curl | DDI HTTPS transfer | curl license — https://curl.se/docs/copyright.html |
| zlib | Compression | zlib license — https://zlib.net/zlib_license.html |
| FFmpeg (optional) | HEVC/PNG/AAC decoding, recording | Depends on build configuration — https://ffmpeg.org/legal.html |

Preserve each exact dependency's license texts and source/provenance material when
redistributing binaries. A local Homebrew/MSYS2 FFmpeg package can enable GPL
components; the project's LGPL label does not make such a combined binary LGPL-only.
Desktop release bundles use a pinned minimal FFmpeg build without GPL/nonfree
options. License texts accompany the runtime; exact sources, build recipes and
metadata are distributed in a separate matching source archive.

The initial implementation was derived from the copyright owner's scrctl source;
its owned code was relicensed with authorization. Third-party notices already
present in imported source files remain applicable. See docs/ORIGIN.md.

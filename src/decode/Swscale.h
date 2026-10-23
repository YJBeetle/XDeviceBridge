#pragma once

extern "C" {
#include <libavutil/frame.h>
#include <libavutil/pixdesc.h>
#include <libswscale/swscale.h>
}

namespace xdb::detail {
// YUVJ has the same plane layout as YUV; its implicit full range must survive
// normalization. Passing YUVJ directly also defeats sws_getCachedContext reuse.
inline bool prepare_bgra_scaler(SwsContext *&context, const AVFrame &frame) {
    auto format = static_cast<AVPixelFormat>(frame.format);
    bool legacy_full = true;
    switch (format) {
    case AV_PIX_FMT_YUVJ420P: format = AV_PIX_FMT_YUV420P; break;
    case AV_PIX_FMT_YUVJ422P: format = AV_PIX_FMT_YUV422P; break;
    case AV_PIX_FMT_YUVJ444P: format = AV_PIX_FMT_YUV444P; break;
    case AV_PIX_FMT_YUVJ440P: format = AV_PIX_FMT_YUV440P; break;
    case AV_PIX_FMT_YUVJ411P: format = AV_PIX_FMT_YUV411P; break;
    default: legacy_full = false; break;
    }
    context = sws_getCachedContext(context, frame.width, frame.height, format,
        frame.width, frame.height, AV_PIX_FMT_BGRA, SWS_POINT, nullptr, nullptr, nullptr);
    if (!context) return false;
    const auto *description = av_pix_fmt_desc_get(format);
    if (!description || (description->flags & AV_PIX_FMT_FLAG_RGB) || description->nb_components < 3)
        return true;
    int matrix = SWS_CS_DEFAULT;
    switch (frame.colorspace) {
    case AVCOL_SPC_BT709: matrix = SWS_CS_ITU709; break;
    case AVCOL_SPC_FCC: matrix = SWS_CS_FCC; break;
    case AVCOL_SPC_BT470BG:
    case AVCOL_SPC_SMPTE170M: matrix = SWS_CS_ITU601; break;
    case AVCOL_SPC_SMPTE240M: matrix = SWS_CS_SMPTE240M; break;
    case AVCOL_SPC_BT2020_NCL: matrix = SWS_CS_BT2020; break;
    default: break;
    }
    const int *coefficients = sws_getCoefficients(matrix);
    const bool full = legacy_full || frame.color_range == AVCOL_RANGE_JPEG;
    return sws_setColorspaceDetails(context, coefficients, full, coefficients, 1,
        0, 1 << 16, 1 << 16) >= 0;
}
} // namespace xdb::detail

// 非 Apple 平台按本构建的 libav 能力选择音频后端。
#include "decode/AudioDecoder.h"
#if defined(XDB_HAVE_LIBAV_AUDIO)
#include "decode/FFmpegEldDecoder.h"
#endif

namespace xdb {

std::unique_ptr<AudioDecoder> create_audio_decoder(int sample_rate, int channels,
                                                  int frame_length, std::string &err) {
#if defined(XDB_HAVE_LIBAV_AUDIO)
    return create_ffmpeg_eld_decoder(sample_rate, channels, frame_length, err);
#else
    (void)sample_rate;
    (void)channels;
    (void)frame_length;
    err = XDB_TR(kNoAudioDecoderMessage);
    return nullptr;
#endif
}

}  // namespace xdb

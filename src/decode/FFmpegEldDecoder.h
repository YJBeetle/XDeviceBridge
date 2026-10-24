#pragma once

#include "decode/AudioDecoder.h"

#include <cstddef>
#include <memory>
#include <string>

namespace xdb {

/// 独立的 FFmpeg AAC-ELD 工厂，暂不替代平台工厂，供离线及后端对照使用。
/// 当前只接受 48000 Hz、双声道、每声道 480 或 512 采样的无 LD-SBR 配置。
/// ASC 根据这组明确的配置生成；不是从收到的裸帧猜测编码或 SBR 开关。
///
/// 输入为一帧裸 AAC-ELD，格式转换由 libswresample 完成。失败返回 nullptr 并
/// 填写 err；成功清空 err。解码成功不等于所测设备的有声音频已验证，应同时比较
/// 输出采样数、波形及实际播放，不能只看静音包或返回值。
std::unique_ptr<AudioDecoder> create_ffmpeg_eld_decoder(int sample_rate, int channels,
                                                      int frame_length, std::string &err);

} // namespace xdb

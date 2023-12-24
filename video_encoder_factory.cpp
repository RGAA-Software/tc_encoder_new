//
// Created by hy on 2023/12/19.
//

#include "video_encoder_factory.h"
#include "ffmpeg_video_encoder.h"
#include "tc_common/log.h"

#include <memory>

namespace tc
{

    std::shared_ptr<VideoEncoder> VideoEncoderFactory::CreateEncoder(
            const VideoEncoderParams& params,
            CreateEncoderPolicy policy,
            CreateEncoderName name) {

        if (policy == CreateEncoderPolicy::kAuto) {
            // 1. nvenc 12.0
            // 2. nvenc 8.1
            // 3. amf
            // 4. ffmpeg
            return nullptr;
        }
        else {
            if (name == CreateEncoderName::kNVENC) {
                // 1. 12.0
                // 2. 8.1
                return nullptr;
            }
            else if (name == CreateEncoderName::kAMF) {
                return nullptr;
            }
            else if (name == CreateEncoderName::kFFmpeg) {
                LOGI("Finally, select FFmpeg as encoder.");
                return std::make_shared<FFmpegVideoEncoder>(params);
            }
        }
        return nullptr;
    }

}
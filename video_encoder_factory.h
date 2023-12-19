//
// Created by hy on 2023/12/19.
//

#ifndef TC_APPLICATION_VIDEO_ENCODER_FACTORY_H
#define TC_APPLICATION_VIDEO_ENCODER_FACTORY_H

#include "video_encoder.h"

namespace tc
{

    enum CreateEncoderPolicy {
        // 自动选择编码器
        kAuto,
        // 手动指定，可以从配置文件修改来切换编码器
        kSpecify,
    };

    enum CreateEncoderName {
        kUnknownEncoder,
        kNVENC,
        kAMF,
        kFFmpeg,
    };

    class VideoEncoderFactory {
    public:

        static std::shared_ptr<VideoEncoder> CreateEncoder(
                const VideoEncoderParams& params,
                CreateEncoderPolicy policy,
                CreateEncoderName name = CreateEncoderName::kUnknownEncoder);

    };

}

#endif //TC_APPLICATION_VIDEO_ENCODER_FACTORY_H

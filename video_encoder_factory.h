//
// Created by hy on 2023/12/19.
//

#ifndef TC_APPLICATION_VIDEO_ENCODER_FACTORY_H
#define TC_APPLICATION_VIDEO_ENCODER_FACTORY_H

#include "video_encoder.h"
#include "encoder_config.h"

namespace tc
{



    enum class ECreateEncoderPolicy {
        // 自动选择编码器
        kAuto,
        // 手动指定，可以从配置文件修改来切换编码器
        kSpecify,
    };

    enum class ECreateEncoderName {
        kUnknownEncoder,
        kNVENC,
        kAMF,
        kFFmpeg,
    };

    class VideoEncoderFactory {
    public:

#if 0
        static std::shared_ptr<VideoEncoder> CreateEncoder(
                const VideoEncoderParams& params,
                ECreateEncoderPolicy policy,
                ECreateEncoderName name = ECreateEncoderName::kUnknownEncoder);
#endif

        // adapter_uid_ 显卡适配器ID, 后期，如果同一个显卡，编码多个桌面画面的话，还要区分下桌面索引
        static std::shared_ptr<VideoEncoder> CreateEncoder(
                EncoderFeature feature, ECreateEncoderPolicy policy, const EncoderConfig& config, ECreateEncoderName name = ECreateEncoderName::kUnknownEncoder
        );
    };

}

#endif //TC_APPLICATION_VIDEO_ENCODER_FACTORY_H

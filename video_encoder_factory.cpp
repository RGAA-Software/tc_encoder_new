//
// Created by hy on 2023/12/19.
//

#include "video_encoder_factory.h"
#include "ffmpeg_video_encoder.h"
#include "nvenc_video_encoder.h"
#include "tc_common/log.h"
#include "video_encoder.h"

#include <memory>

namespace tc
{
#if 0
	std::shared_ptr<VideoEncoder> VideoEncoderFactory::CreateEncoder( const VideoEncoderParams& params, ECreateEncoderPolicy policy, ECreateEncoderName name) {
	    if (policy == ECreateEncoderPolicy::kAuto) {
	        // 1. nvenc 12.0
	        // 2. nvenc 8.1
	        // 3. amf
	        // 4. ffmpeg
	        return nullptr;
	    }
	    else {
	        if (name == ECreateEncoderName::kNVENC) {
	            // 1. 12.0
	            // 2. 8.1
	            // return nullptr;
	            return std::make_shared<NVENCVideoEncoder>(params);
	        }
	        else if (name == ECreateEncoderName::kAMF) {
	            return nullptr;
	        }
	        else if (name == ECreateEncoderName::kFFmpeg) {
	            LOGI("Finally, select FFmpeg as encoder.");
	            return std::make_shared<FFmpegVideoEncoder>(params);
	        }
	    }
	    return nullptr;
	}
#endif

	std::shared_ptr<VideoEncoder> VideoEncoderFactory::CreateEncoder(const std::shared_ptr<MessageNotifier>& msg_notifier,
                                                                     EncoderFeature feature,
                                                                     ECreateEncoderPolicy policy,
                                                                     const EncoderConfig& config,
                                                                     ECreateEncoderName name)
	{
	    std::shared_ptr<VideoEncoder> video_encoder;
	    if (policy == ECreateEncoderPolicy::kAuto) {
            // 1. 12.0
            // 2. 8.1
            // return nullptr;
            video_encoder = std::make_shared<NVENCVideoEncoder>(msg_notifier, feature);
            if(!video_encoder->Initialize(config)) {
                printf("NVENCVideoEncoder Initialize error\n");
                return nullptr;
            }
	        return nullptr;
	    }
	    else {
	        if (name == ECreateEncoderName::kNVENC) {
                video_encoder = std::make_shared<NVENCVideoEncoder>(msg_notifier, feature);
                if(!video_encoder->Initialize(config)) {
                    printf("NVENCVideoEncoder Initialize error\n");
                    return nullptr;
                }
	        }
	        else if (name == ECreateEncoderName::kAMF) {
	            return nullptr;
	        }
	        else if (name == ECreateEncoderName::kFFmpeg) {
	            LOGI("Finally, select FFmpeg as encoder.");
	            video_encoder = std::make_shared<FFmpegVideoEncoder>(msg_notifier, feature);
	            if(!video_encoder->Initialize(config)) {
	                printf("FFmpegVideoEncoder Initialize error\n");
	                return nullptr;
	            }
	        }
	    }
	    return video_encoder;
	}

}
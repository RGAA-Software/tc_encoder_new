//
// Created by hy on 2023/12/19.
//

#ifndef TC_APPLICATION_FFMPEG_VIDEO_ENCODER_H
#define TC_APPLICATION_FFMPEG_VIDEO_ENCODER_H

#include "video_encoder.h"

extern "C" {
    #include "libavcodec/avcodec.h"
}


namespace tc
{
    class FFmpegVideoEncoder : public VideoEncoder {
    public:
        FFmpegVideoEncoder(const EncoderFeature& encoder_feature);
        ~FFmpegVideoEncoder() override;

        bool Initialize(const tc::EncoderConfig& config) override;
        //bool Init() override;
        void Encode(const std::shared_ptr<Image>& i420_image, uint64_t frame_index) override;
        void Exit() override;

    private:
        AVCodecContext* context_ = nullptr;
        AVFrame* frame_ = nullptr;
        AVPacket* packet_ = nullptr;
    };

}

#endif //TC_APPLICATION_FFMPEG_VIDEO_ENCODER_H

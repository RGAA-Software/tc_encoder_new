//
// Created by RGAA on 2023-12-16.
//

#ifndef TC_ENCODER_TC_ENCODER_H
#define TC_ENCODER_TC_ENCODER_H

#include <cstdint>
#include <functional>
#include <memory>

namespace tc
{
    class Image;

    enum VideoEncoderFormat {
        kH264,
        kHEVC,
    };

    // params
    struct VideoEncoderParams {
        int width_;
        int height_;
        VideoEncoderFormat format_;
    };

    // callback
    using EncoderCallback = std::function<void(const std::shared_ptr<Image>& frame, uint64_t frame_index, bool key)>;

    class VideoEncoder {
    public:
        explicit VideoEncoder(const VideoEncoderParams& params);
        virtual ~VideoEncoder();
        void RegisterEncodeCallback(EncoderCallback&& cbk);
        void InsertIDR();

        virtual bool Init();
        // 编码D3DTexture类型，用于NVECC/AMF
        virtual void Encode(uint64_t handle, uint64_t frame_index);
        // 编码RGBA格式的图片，用于FFmpeg/VP9
        virtual void Encode(const std::shared_ptr<Image>& i420_data, uint64_t frame_index);

    protected:
        VideoEncoderParams encoder_params_{};
        EncoderCallback encoder_callback_;
        bool insert_idr_ = false;
    };

}

#endif //TC_ENCODER_TC_ENCODER_H

//
// Created by RGAA on 2023-12-16.
//

#include "video_encoder.h"

namespace tc
{

    VideoEncoder::VideoEncoder(const VideoEncoderParams& params) {
        this->encoder_params_ = params;
    }

    VideoEncoder::~VideoEncoder() {

    }

    bool VideoEncoder::Init() {
        return false;
    }

    void VideoEncoder::InsertIDR() {
        insert_idr_ = true;
    }

    void VideoEncoder::RegisterEncodeCallback(EncoderCallback&& cbk) {
        this->encoder_callback_ = std::move(cbk);
    }

    void VideoEncoder::Encode(uint64_t handle, uint64_t frame_index) {

    }

    void VideoEncoder::Encode(const std::shared_ptr<Image>& i420_data, uint64_t frame_index) {

    }

    void VideoEncoder::Exit() {

    }
}
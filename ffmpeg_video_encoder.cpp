//
// Created by hy on 2023/12/19.
//

#include "ffmpeg_video_encoder.h"

#include <libyuv.h>

#include "tc_common/log.h"
#include "tc_common/data.h"
#include "tc_common/image.h"

namespace tc
{

    FFmpegVideoEncoder::FFmpegVideoEncoder(const VideoEncoderParams& params) : VideoEncoder(params) {

    }

    FFmpegVideoEncoder::~FFmpegVideoEncoder() {}

    bool FFmpegVideoEncoder::Init() {
        //设置编码器
        auto encoder_id = encoder_params_.format_ == VideoEncoderFormat::kHEVC ? AV_CODEC_ID_HEVC : AV_CODEC_ID_H264;
        const AVCodec* encoder = avcodec_find_encoder(encoder_id);

//        encoder = avcodec_find_encoder_by_name("libx265");

        //初始化并设置编码器上下文
        context_ = avcodec_alloc_context3(encoder);
        if (!context_) {
            LOGE("avcodec_alloc_context3 error!");
            return false;
        }
        context_->width = this->encoder_params_.width_;
        context_->height = this->encoder_params_.height_;
        context_->time_base = { 1, 60 };
        context_->pix_fmt = AV_PIX_FMT_YUV420P;
        context_->thread_count = (int)std::thread::hardware_concurrency();
        context_->gop_size = 60;
        context_->bit_rate = 10000000;
        context_->max_b_frames = 0;

        //编码器初始化
        auto ret = avcodec_open2(context_, encoder, NULL);
        if (ret != 0) {
            LOGE("avcodec_open2 error : {}", ret);
            return false;
        }

        //初始化并设置 AV FRAME
        frame_ = av_frame_alloc();
        frame_->width = context_->width;
        frame_->height = context_->height;
        frame_->format = context_->pix_fmt;

        //为AV FRAME分配缓冲区
        av_frame_get_buffer(frame_, 0);

        LOGI("Line 1: {} 2: {} 3: {}", frame_->linesize[0], frame_->linesize[1], frame_->linesize[2]);

        //初始化AV PACKET
        packet_ = av_packet_alloc();

        return true;
    }

    void FFmpegVideoEncoder::Encode(const std::shared_ptr<Image>& i420_image, uint64_t frame_index) {
        //
        auto img_width = i420_image->width;
        auto img_height = i420_image->height;
        auto i420_data = i420_image->data;

        // 如果宽高，格式变了，重新初始化编码器
        // todo

        frame_->pts = (int64_t)frame_index;

        if (insert_idr_) {
            insert_idr_ = false;
            // todo
        }

        // 填充数据
        int y_size =  img_width * img_height;
        int uv_size = img_width * img_height / 4;
        memcpy(frame_->data[0], i420_data->CStr(), y_size);
        memcpy(frame_->data[1], i420_data->CStr() + y_size, uv_size);
        memcpy(frame_->data[2], i420_data->CStr() + y_size + uv_size, uv_size);

        int sendResult = avcodec_send_frame(context_, frame_);

        while (sendResult >= 0)
        {
            //获取转为H264的packet数据
            int receiveResult = avcodec_receive_packet(context_, packet_);

            //如果暂无处理完成的packet数据，则继续发送其他YUV帧
            if (receiveResult == AVERROR(EAGAIN) || receiveResult == AVERROR_EOF) break;

            auto encoded_data = Data::Make((char*)packet_->data, packet_->size);
            if (encoder_callback_) {
                auto image = Image::Make(encoded_data, img_width, img_height, 3);
                encoder_callback_(image, frame_index, false);
            }

            av_packet_unref(packet_);
        }
    }

    void FFmpegVideoEncoder::Exit() {
        VideoEncoder::Exit();


    }

}
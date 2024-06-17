//
// Created by RGAA on 2023/12/19.
//

#include "ffmpeg_video_encoder.h"

#include <libyuv.h>

#include "tc_common_new/log.h"
#include "tc_common_new/data.h"
#include "tc_common_new/image.h"

namespace tc
{

    FFmpegVideoEncoder::FFmpegVideoEncoder(const std::shared_ptr<MessageNotifier>& msg_notifier, const EncoderFeature& encoder_feature)
        : VideoEncoder(msg_notifier, encoder_feature) {

    }

    FFmpegVideoEncoder::~FFmpegVideoEncoder() {

    }

    bool FFmpegVideoEncoder::Initialize(const tc::EncoderConfig& encoder_config) {
        VideoEncoder::Initialize(encoder_config);
        ListCodecs();

        auto encoder_id = encoder_config.codec_type == EVideoCodecType::kHEVC ? AV_CODEC_ID_HEVC : AV_CODEC_ID_H264;
        const AVCodec* encoder = avcodec_find_encoder(encoder_id);

        context_ = avcodec_alloc_context3(encoder);
        if (!context_) {
            LOGE("avcodec_alloc_context3 error!");
            return false;
        }
        context_->width = this->out_width_;
        context_->height = this->out_height_;
        context_->time_base = { 1, this->refresh_rate_ };
        context_->pix_fmt = AV_PIX_FMT_YUV420P;
        //context_->thread_count = std::min((int)std::thread::hardware_concurrency()/2, X265_MAX_FRAME_THREADS);
        context_->thread_count = std::min(16, (int)std::thread::hardware_concurrency());
        context_->thread_type = FF_THREAD_SLICE;
        context_->gop_size = encoder_config.gop_size;
        context_->max_b_frames = 0;
        context_->bit_rate = 10000000; // 10Mbps

        if(-1 == encoder_config.gop_size) {
            context_->gop_size = 60;
        }
        context_->bit_rate = encoder_config.bitrate;
        context_->max_b_frames = 0;

        auto ret = avcodec_open2(context_, encoder, NULL);
        if (ret != 0) {
            LOGE("avcodec_open2 error : {}", ret);
            return false;
        }

        frame_ = av_frame_alloc();
        frame_->width = context_->width;
        frame_->height = context_->height;
        frame_->format = context_->pix_fmt;

        av_frame_get_buffer(frame_, 0);
        packet_ = av_packet_alloc();

        LOGI("Line 1: {} 2: {} 3: {}", frame_->linesize[0], frame_->linesize[1], frame_->linesize[2]);
        return true;
    }

    void FFmpegVideoEncoder::Encode(const std::shared_ptr<Image>& i420_image, uint64_t frame_index) {
        //
        auto img_width = i420_image->width;
        auto img_height = i420_image->height;
        auto i420_data = i420_image->data;

        // re-create when width/height changed
        // todo

        frame_->pts = (int64_t)frame_index;

        if (insert_idr_) {
            insert_idr_ = false;
            // insert idr
            // todo
        }

        int y_size =  img_width * img_height;
        int uv_size = img_width * img_height / 4;
        memcpy(frame_->data[0], i420_data->CStr(), y_size);
        memcpy(frame_->data[1], i420_data->CStr() + y_size, uv_size);
        memcpy(frame_->data[2], i420_data->CStr() + y_size + uv_size, uv_size);

        int send_result = avcodec_send_frame(context_, frame_);
        while (send_result >= 0) {
            int receiveResult = avcodec_receive_packet(context_, packet_);
            if (receiveResult == AVERROR(EAGAIN) || receiveResult == AVERROR_EOF) {
                break;
            }

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

    void FFmpegVideoEncoder::ListCodecs() {
        const AVCodec *codec = nullptr;
        void *opaque = nullptr;

        LOGI("Available codecs:");
        while ((codec = av_codec_iterate(&opaque)) != nullptr) {
            if (codec->type == AVMEDIA_TYPE_VIDEO || codec->type == AVMEDIA_TYPE_AUDIO) {
                if (av_codec_is_encoder(codec)) {
                    LOGI("Encoder: {}", codec->name);
                }
                if (av_codec_is_decoder(codec)) {
                    //LOGI("Decoder: {}", codec->name);
                }
            }
        }
    }

}
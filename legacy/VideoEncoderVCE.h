#pragma once

#include "VideoEncoder.h"
#include "amf/common/AMFFactory.h"
#include "amf/include/components/VideoEncoderVCE.h"
#include "amf/include/components/VideoEncoderHEVC.h"
#include "amf/include/components/VideoConverter.h"
#include "amf/common/Thread.h"
#include "tc_encoder_new/encoder_config.h"
#include "tc_encoder_new/video_encoder.h"
#include <thread>
#include <fstream>
#include <functional>

typedef std::function<void (amf::AMFData *)> AMFTextureReceiver;

namespace tc
{
    class AMFTextureEncoder {
    public:
        AMFTextureEncoder(const amf::AMFContextPtr &amfContext, EncoderConfig config,
                          amf::AMF_SURFACE_FORMAT inputFormat, AMFTextureReceiver receiver);
        ~AMFTextureEncoder();
        void Start();
        void Shutdown();
        void Submit(amf::AMFData *data);
        void Run();

    private:
        amf::AMFComponentPtr amf_encoder_ = nullptr;
        std::thread* thread_ = nullptr;
        AMFTextureReceiver receiver_;
        EVideoCodecType codec_;
    };

    class AMFTextureConverter {
    public:
        AMFTextureConverter(const amf::AMFContextPtr &amfContext, int width, int height,
                            amf::AMF_SURFACE_FORMAT inputFormat, amf::AMF_SURFACE_FORMAT outputFormat,
                            AMFTextureReceiver receiver);
        ~AMFTextureConverter();
        void Start();
        void Shutdown();
        void Submit(amf::AMFData *data);
        void Run();

    private:
        amf::AMFComponentPtr amf_converter_ = nullptr;
        std::thread* thread_ = nullptr;
        AMFTextureReceiver receiver_;
    };

    // Video encoder for AMD VCE.
    class VideoEncoderVCE : public VideoEncoder {
    public:
        VideoEncoderVCE(const std::shared_ptr<MessageNotifier> &msg_notifier, const EncoderFeature &encoder_feature);
        ~VideoEncoderVCE() override;

        bool Initialize(const tc::EncoderConfig &config) override;
        //void Encode(uint64_t handle, uint64_t frame_index) override;
        void Encode(ID3D11Texture2D *tex2d, uint64_t frame_index) override;
        void Encode(const std::shared_ptr<Image> &i420_data, uint64_t frame_index) override;
        void Exit() override;
        void Shutdown();
        void Receive(amf::AMFData *data);

    private:
        void EncodeTextureHandle(uint64_t handle, uint64_t frame_index);
        void ApplyFrameProperties(const amf::AMFSurfacePtr &surface, bool insertIDR);
        void SkipAUD(char** buffer, int* length);
        void EncodeTexture(ID3D11Texture2D* texture, int width, int height, int64_t frame_idx);
        void CopyRawTexture(ID3D11Texture2D* texture, DXGI_FORMAT format, int height);

    private:
        amf::AMF_SURFACE_FORMAT convert_input_format_ = amf::AMF_SURFACE_BGRA;// AMF_SURFACE_RGBA;
        amf::AMF_SURFACE_FORMAT encoder_input_format_ = amf::AMF_SURFACE_BGRA;// amf::AMF_SURFACE_NV12;
        amf::AMFContextPtr amf_context_ = nullptr;
        std::shared_ptr<AMFTextureEncoder> encoder_ = nullptr;
        std::shared_ptr<AMFTextureConverter> converter_ = nullptr;
        EVideoCodecType codec_type_{};
        bool insert_idr_ = false;
        int gop_ = 180;
        std::ofstream dbg_file_;
    };

}
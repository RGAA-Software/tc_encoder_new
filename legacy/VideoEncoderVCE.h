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
        AMFTextureEncoder(const amf::AMFContextPtr &amfContext, int width, int height,
                          amf::AMF_SURFACE_FORMAT inputFormat, AMFTextureReceiver receiver, EVideoCodecType codec);
        ~AMFTextureEncoder();
        void Start();
        void Shutdown();
        void Submit(amf::AMFData *data);

    private:
        amf::AMFComponentPtr m_amfEncoder;
        std::thread* m_thread = nullptr;
        AMFTextureReceiver m_receiver;
        EVideoCodecType m_codec;

        void Run();
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

    private:
        amf::AMFComponentPtr m_amfConverter;
        std::thread* m_thread = nullptr;
        AMFTextureReceiver m_receiver;

        void Run();
    };

    // Video encoder for AMD VCE.
    class VideoEncoderVCE : public VideoEncoder {
    public:
        VideoEncoderVCE(const std::shared_ptr<MessageNotifier> &msg_notifier, const EncoderFeature &encoder_feature);
        ~VideoEncoderVCE() override;

        bool Initialize(const tc::EncoderConfig &config) override;
        void Encode(uint64_t handle, uint64_t frame_index) override;
        void Encode(ID3D11Texture2D *tex2d) override;
        void Encode(const std::shared_ptr<Image> &i420_data, uint64_t frame_index) override;
        void Exit() override;
        void Shutdown();
        void Receive(amf::AMFData *data);

    private:
        void EncodeTextureHandle(uint64_t handle, uint64_t frame_index);
        void ApplyFrameProperties(const amf::AMFSurfacePtr &surface, bool insertIDR);
        void SkipAUD(char **buffer, int *length);

    private:
        amf::AMF_SURFACE_FORMAT convert_input_format = amf::AMF_SURFACE_BGRA;// AMF_SURFACE_RGBA;
        amf::AMF_SURFACE_FORMAT encoder_input_format = amf::AMF_SURFACE_BGRA;// amf::AMF_SURFACE_NV12;

        static const wchar_t *START_TIME_PROPERTY;
        static const wchar_t *FRAME_INDEX_PROPERTY;
        static const wchar_t *IS_KEY_FRAME;

        amf::AMFContextPtr m_amfContext = nullptr;
        std::shared_ptr<AMFTextureEncoder> m_encoder = nullptr;
        std::shared_ptr<AMFTextureConverter> m_converter = nullptr;
        std::ofstream fpOut;
        EVideoCodecType codec_type_;
        bool insert_idr = false;
        int gop = 60;
    };

}
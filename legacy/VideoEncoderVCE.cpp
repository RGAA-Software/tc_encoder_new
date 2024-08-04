#include "VideoEncoderVCE.h"

#include "tc_common_new/log.h"
#include "tc_common_new/time_ext.h"
#include "tc_common_new/data.h"
#include "tc_encoder_new/encoder_config.h"
#include "tc_common_new/image.h"
#include "D3DTextureDebug.h"
#include <combaseapi.h>
#include <atlbase.h>
#include <fstream>
#include <iostream>
#include <utility>

#define DEBUG_FILE 0

#define AMF_THROW_IF(expr) {AMF_RESULT res = expr;\
    if(res != AMF_OK){LOGE("ERROR: {}", res); assert(0);}}

const wchar_t *VideoEncoderVCE::START_TIME_PROPERTY = L"StartTimeProperty";
const wchar_t *VideoEncoderVCE::FRAME_INDEX_PROPERTY = L"FrameIndexProperty";
const wchar_t* VideoEncoderVCE::IS_KEY_FRAME = L"IsKeyFrame";
static uint64_t last_time = tc::TimeExt::GetCurrentTimestamp();
static int fps = 0;

namespace tc
{
    AMFTextureEncoder::AMFTextureEncoder(const amf::AMFContextPtr &amfContext,
                                         int width, int height,
                                         amf::AMF_SURFACE_FORMAT inputFormat, AMFTextureReceiver receiver,
                                         EVideoCodecType codec) : m_receiver(std::move(receiver)) {
        m_codec = codec;
        const wchar_t *pCodec;

        amf_int32 frameRateIn = 60;
        amf_int64 bitRateIn = 20000000L; //5Mbits// Settings::Instance().m_encodeBitrateInMBits * 1000000L; // in bits

        switch (m_codec) {
            case EVideoCodecType::kH264:
                pCodec = AMFVideoEncoderVCE_AVC;
                break;
            case EVideoCodecType::kHEVC:
                pCodec = AMFVideoEncoder_HEVC;
                break;
            default:
                LOGE("Error codec type: {}", (int)m_codec);
        }

        // Create encoder component.
        auto ret = g_AMFFactory.GetFactory()->CreateComponent(amfContext, pCodec, &m_amfEncoder);
        if (ret != AMF_OK) {
            LOGE("CreateComponent failed: {}", ret);
            return;
        }

        if (m_codec == EVideoCodecType::kH264) {
            //m_amfEncoder->SetProperty(AMF_VIDEO_ENCODER_USAGE, AMF_VIDEO_ENCODER_USAGE_ULTRA_LOW_LATENCY);
            //m_amfEncoder->SetProperty(AMF_VIDEO_ENCODER_USAGE, AMF_VIDEO_ENCODER_USAGE_TRANSCODING);
            m_amfEncoder->SetProperty(AMF_VIDEO_ENCODER_USAGE, AMF_VIDEO_ENCODER_USAGE_LOW_LATENCY);
            m_amfEncoder->SetProperty(AMF_VIDEO_ENCODER_B_PIC_PATTERN, 0);
            m_amfEncoder->SetProperty(AMF_VIDEO_ENCODER_QUALITY_PRESET, AMF_VIDEO_ENCODER_QUALITY_PRESET_QUALITY);
            m_amfEncoder->SetProperty(AMF_VIDEO_ENCODER_TARGET_BITRATE, bitRateIn);
            m_amfEncoder->SetProperty(AMF_VIDEO_ENCODER_FRAMESIZE, ::AMFConstructSize(width, height));
            m_amfEncoder->SetProperty(AMF_VIDEO_ENCODER_FRAMERATE, ::AMFConstructRate(frameRateIn, 1));

            //m_amfEncoder->SetProperty(AMF_VIDEO_ENCODER_PROFILE, AMF_VIDEO_ENCODER_PROFILE_HIGH);
            //m_amfEncoder->SetProperty(AMF_VIDEO_ENCODER_PROFILE_LEVEL, 51);
        } else {
            m_amfEncoder->SetProperty(AMF_VIDEO_ENCODER_HEVC_USAGE, AMF_VIDEO_ENCODER_HEVC_USAGE_ULTRA_LOW_LATENCY);
            m_amfEncoder->SetProperty(AMF_VIDEO_ENCODER_HEVC_QUALITY_PRESET, AMF_VIDEO_ENCODER_HEVC_QUALITY_PRESET_QUALITY);
            m_amfEncoder->SetProperty(AMF_VIDEO_ENCODER_HEVC_TARGET_BITRATE, bitRateIn);
            m_amfEncoder->SetProperty(AMF_VIDEO_ENCODER_HEVC_FRAMESIZE, ::AMFConstructSize(width, height));
            m_amfEncoder->SetProperty(AMF_VIDEO_ENCODER_HEVC_FRAMERATE, ::AMFConstructRate(frameRateIn, 1));

            //m_amfEncoder->SetProperty(AMF_VIDEO_ENCODER_HEVC_TIER, AMF_VIDEO_ENCODER_HEVC_TIER_HIGH);
            //m_amfEncoder->SetProperty(AMF_VIDEO_ENCODER_HEVC_PROFILE_LEVEL, AMF_LEVEL_5);
        }

        ret = m_amfEncoder->Init(inputFormat, width, height);
        if (ret != AMF_OK) {
            LOGE("!!! AMF encoder init failed: {}, {}x{}, format: {}", ret, width, height, inputFormat);
            return;
        }

        LOGI("Initialized AMFTextureEncoder.");
    }

    AMFTextureEncoder::~AMFTextureEncoder() {
        if (m_amfEncoder) {
            m_amfEncoder->Terminate();
        }
    }

    void AMFTextureEncoder::Start() {
        m_thread = new std::thread(&AMFTextureEncoder::Run, this);
    }

    void AMFTextureEncoder::Shutdown() {
        LOGI("AMFTextureEncoder::Shutdown() m_amfEncoder->Drain");
        m_amfEncoder->Drain();
        LOGI("AMFTextureEncoder::Shutdown() m_thread->join");
        m_thread->join();
        LOGI("AMFTextureEncoder::Shutdown() joined.");
        delete m_thread;
        m_thread = nullptr;
    }

    void AMFTextureEncoder::Submit(amf::AMFData *data) {
        while (true) {
            auto res = m_amfEncoder->SubmitInput(data);
            if (res == AMF_INPUT_FULL) {
                return;
            } else {
                break;
            }
        }
    }

    void AMFTextureEncoder::Run() {
        LOGI("Start AMFTextureEncoder thread. Thread Id={}", GetCurrentThreadId());
        amf::AMFDataPtr data;
        while (true) {
            auto res = m_amfEncoder->QueryOutput(&data);
            if (res == AMF_EOF) {
                LOGI("m_amfEncoder->QueryOutput returns AMF_EOF.");
                return;
            }

            if (data != nullptr) {
                m_receiver(data);
            } else {
                Sleep(1);
            }
        }
    }

    AMFTextureConverter::AMFTextureConverter(const amf::AMFContextPtr &amfContext, int width, int height,
                                             amf::AMF_SURFACE_FORMAT inputFormat, amf::AMF_SURFACE_FORMAT outputFormat,
                                             AMFTextureReceiver receiver) : m_receiver(std::move(receiver)) {
        AMF_THROW_IF(g_AMFFactory.GetFactory()->CreateComponent(amfContext, AMFVideoConverter, &m_amfConverter));
        AMF_THROW_IF(m_amfConverter->SetProperty(AMF_VIDEO_CONVERTER_MEMORY_TYPE, amf::AMF_MEMORY_DX11));
        AMF_THROW_IF(m_amfConverter->SetProperty(AMF_VIDEO_CONVERTER_OUTPUT_FORMAT, outputFormat));
        AMF_THROW_IF(m_amfConverter->SetProperty(AMF_VIDEO_CONVERTER_OUTPUT_SIZE, ::AMFConstructSize(width, height)));
        AMF_THROW_IF(m_amfConverter->Init(inputFormat, width, height));
        LOGI("Initialized AMFTextureConverter.");
    }

    AMFTextureConverter::~AMFTextureConverter() {
        if (m_amfConverter) {
            m_amfConverter->Terminate();
        }
    }

    void AMFTextureConverter::Start() {
        m_thread = new std::thread(&AMFTextureConverter::Run, this);
    }

    void AMFTextureConverter::Shutdown() {
        m_amfConverter->Drain();
        m_thread->join();
        delete m_thread;
        m_thread = nullptr;
    }

    void AMFTextureConverter::Submit(amf::AMFData *data) {
        while (true) {
            auto res = m_amfConverter->SubmitInput(data);
            if (res == AMF_INPUT_FULL) {
                return;
            } else {
                break;
            }
        }
    }

    void AMFTextureConverter::Run() {
        amf::AMFDataPtr data;
        while (true) {
            auto res = m_amfConverter->QueryOutput(&data);
            if (res == AMF_EOF) {
                LOGE("m_amfConverter->QueryOutput returns AMF_EOF.");
                return;
            }

            if (data != nullptr) {
                m_receiver(data);
            } else {
                Sleep(1);
            }
        }
    }

    VideoEncoderVCE::VideoEncoderVCE(const std::shared_ptr<MessageNotifier> &msg_notifier, const EncoderFeature &encoder_feature)
        : VideoEncoder(msg_notifier, encoder_feature) {
    }

    VideoEncoderVCE::~VideoEncoderVCE() {
        if (m_amfContext) {
            m_amfContext->Terminate();
            m_amfContext->Release();
        }
    }

    amf::AMF_SURFACE_FORMAT DXGI_FORMAT_to_AMF_FORMAT(DXGI_FORMAT format) {
        if (DXGI_FORMAT_R8G8B8A8_UNORM == format || DXGI_FORMAT_R8G8B8A8_UNORM_SRGB == format) {
            return amf::AMF_SURFACE_RGBA;
        } else if (DXGI_FORMAT_B8G8R8A8_UNORM == format || DXGI_FORMAT_B8G8R8A8_UNORM_SRGB == format) {
            return amf::AMF_SURFACE_BGRA;
        } else {
            LOGE("DONT known the dxgi format !");
            return amf::AMF_SURFACE_UNKNOWN;
        }
    }

    bool VideoEncoderVCE::Initialize(const tc::EncoderConfig &config) {
        VideoEncoder::Initialize(config);
        codec_type_ = config.codec_type;
        LOGI("Initializing VideoEncoderVCE.");
        AMF_THROW_IF(g_AMFFactory.Init());

        ::amf_increase_timer_precision();

        AMF_THROW_IF(g_AMFFactory.GetFactory()->CreateContext(&m_amfContext));
        if (m_amfContext->InitDX11(d3d11_device_.Get()) != AMF_OK) {
            LOGE("Amf context init dx11 failed.");
            return false;
        }

        auto format = static_cast<DXGI_FORMAT>(config.texture_format);
        convert_input_format = DXGI_FORMAT_to_AMF_FORMAT(format);
        encoder_input_format = DXGI_FORMAT_to_AMF_FORMAT(format);
        m_encoder = std::make_shared<AMFTextureEncoder>(m_amfContext, config.width, config.height, encoder_input_format,
                                                        std::bind(&VideoEncoderVCE::Receive, this, std::placeholders::_1),
                                                        config.codec_type);
        m_converter = std::make_shared<AMFTextureConverter>(m_amfContext, config.width, config.height,
                                                            convert_input_format, encoder_input_format,
                                                            std::bind(&AMFTextureEncoder::Submit, m_encoder.get(), std::placeholders::_1));

        m_encoder->Start();
        m_converter->Start();

#if DEBUG_FILE
        fpOut = std::ofstream("cccc.h264", std::ios::binary | std::ios::beg);
        if (!fpOut) {
            LOGI("Unable to open output file");
        }
#endif
        LOGI("Successfully initialized VideoEncoderVCE.");
        return true;
    }

    void VideoEncoderVCE::Encode(uint64_t handle, uint64_t frame_index) {
        EncodeTextureHandle(handle, frame_index);
    }

    void VideoEncoderVCE::Encode(ID3D11Texture2D *tex2d) {

    }

    void VideoEncoderVCE::Encode(const std::shared_ptr<Image> &i420_data, uint64_t frame_index) {

    }

    void VideoEncoderVCE::Exit() {

    }

    void VideoEncoderVCE::Shutdown() {
        LOGI("Shutting down VideoEncoderVCE.");
        m_encoder->Shutdown();
        m_converter->Shutdown();
        amf_restore_timer_precision();
#if DEBUG_FILE
        if (fpOut) {
            fpOut.close();
        }
#endif
        LOGI("Successfully shutdown VideoEncoderVCE.");
    }

    void VideoEncoderVCE::EncodeTextureHandle(uint64_t handle, uint64_t frame_index) {
        ComPtr<ID3D11Texture2D> shared_texture;
        shared_texture = OpenSharedTexture(reinterpret_cast<HANDLE>(handle));
        if (!shared_texture) {
            LOGE("OpenSharedTexture failed.");
            return;
        }

        D3D11_TEXTURE2D_DESC desc1;
        shared_texture->GetDesc(&desc1);

        if (!CopyID3D11Texture2D(shared_texture)) {
            LOGE("CopyID3D11Texture2D failed.");
            return;
        }

        amf::AMFSurfacePtr surface = nullptr;
        // Surface is cached by AMF.
        AMF_THROW_IF(m_amfContext->AllocSurface(amf::AMF_MEMORY_DX11, convert_input_format, desc1.Width, desc1.Height, &surface));
        if (!surface) {
            LOGE("AllocSurface failed!");
            return;
        }
        auto textureDX11 = (ID3D11Texture2D *) surface->GetPlaneAt(0)->GetNative(); // no reference counting - do not Release()
        d3d11_device_context_->CopyResource(textureDX11, texture2d_.Get());

        // print to check the format equal or not
        //D3DTextureDebug::PrintTextureDesc(textureDX11);
        //D3DTextureDebug::PrintTextureDesc(texture);

        // save to dds
        //D3DTextureDebug::SaveAsDDS(d3d11_device_context_.Get(), textureDX11, "aaaa.dds");
        //D3DTextureDebug::SaveAsDDS(d3d11_device_context_.Get(), shared_texture.Get(), "bbbb.dds");

        amf_pts start_time = amf_high_precision_clock();
        surface->SetProperty(START_TIME_PROPERTY, start_time);
        surface->SetProperty(FRAME_INDEX_PROPERTY, frame_index);

        if (!insert_idr) {
            if (frame_index % gop == 0) {
                insert_idr = true;
            }
        }
        surface->SetProperty(IS_KEY_FRAME, insert_idr);
        ApplyFrameProperties(surface, insert_idr);
        insert_idr = false;

        // perhaps to convert
        //m_converter->Submit(surface);
        m_encoder->Submit(surface);
    }

    void VideoEncoderVCE::Receive(amf::AMFData *data) {
        amf_pts current_time = amf_high_precision_clock();
        amf_pts start_time = 0;
        uint64_t frameIndex;
        bool is_key_frame;
        data->GetProperty(START_TIME_PROPERTY, &start_time);
        data->GetProperty(FRAME_INDEX_PROPERTY, &frameIndex);
        data->GetProperty(IS_KEY_FRAME, &is_key_frame);

        amf::AMFBufferPtr buffer(data); // query for buffer interface
        char *p = reinterpret_cast<char *>(buffer->GetNative());
        int length = buffer->GetSize();

        SkipAUD(&p, &length);

        //LOGI("VCE encode latency: {} ms. Size={} bytes frameIndex={}, length : {}", double(current_time - start_time) / MILLISEC_TIME, (int)buffer->GetSize(), frameIndex, length);
#if DEBUG_FILE
        if (fpOut.is_open()) {
            fpOut.write(p, length);
        }
#endif
        auto encoded_data = Data::Make((char *) p, length);
        if (encoder_callback_) {
            auto image = Image::Make(encoded_data, out_width_, out_height_, 3);
            encoder_callback_(image, ++encoded_frame_index_, insert_idr_);
        }

        fps++;
        uint64_t ct = tc::TimeExt::GetCurrentTimestamp();
        if (ct - last_time > 1000) {
            LOGI("Recv FPS : {}", fps);
            last_time = ct;
            fps = 0;
        }

    }

    void VideoEncoderVCE::ApplyFrameProperties(const amf::AMFSurfacePtr &surface, bool insertIDR) {
        switch (codec_type_) {
            case EVideoCodecType::kH264:
                // Disable AUD (NAL Type 9) to produce the same stream format as VideoEncoderNVENC.
                surface->SetProperty(AMF_VIDEO_ENCODER_INSERT_AUD, false);
                if (insertIDR) {
                    //LOGI("Inserting IDR frame for H.264.");
                    surface->SetProperty(AMF_VIDEO_ENCODER_INSERT_SPS, true);
                    surface->SetProperty(AMF_VIDEO_ENCODER_INSERT_PPS, true);
                    surface->SetProperty(AMF_VIDEO_ENCODER_FORCE_PICTURE_TYPE, AMF_VIDEO_ENCODER_PICTURE_TYPE_IDR);
                }
                break;
            case EVideoCodecType::kHEVC:
                // This option is ignored. Maybe a bug on AMD driver.
                surface->SetProperty(AMF_VIDEO_ENCODER_HEVC_INSERT_AUD, false);
                if (insertIDR) {
                    //LOGI("Inserting IDR frame for H.265.");
                    // Insert VPS,SPS,PPS
                    // These options don't work properly on older AMD driver (Radeon Software 17.7, AMF Runtime 1.4.4)
                    // Fixed in 18.9.2 & 1.4.9
                    surface->SetProperty(AMF_VIDEO_ENCODER_HEVC_INSERT_HEADER, true);
                    surface->SetProperty(AMF_VIDEO_ENCODER_HEVC_FORCE_PICTURE_TYPE, AMF_VIDEO_ENCODER_HEVC_PICTURE_TYPE_IDR);
                }
                break;
        }
    }

    void VideoEncoderVCE::SkipAUD(char **buffer, int *length) {
        // H.265 encoder always produces AUD NAL even if AMF_VIDEO_ENCODER_HEVC_INSERT_AUD is set. But it is not needed.
        static const int AUD_NAL_SIZE = 7;

        if (codec_type_ != EVideoCodecType::kHEVC) {
            return;
        }

        if (*length < AUD_NAL_SIZE + 4) {
            return;
        }

        // Check if start with AUD NAL.
        if (memcmp(*buffer, "\x00\x00\x00\x01\x46", 5) != 0) {
            return;
        }
        // Check if AUD NAL size is AUD_NAL_SIZE bytes.
        if (memcmp(*buffer + AUD_NAL_SIZE, "\x00\x00\x00\x01", 4) != 0) {
            return;
        }
        *buffer += AUD_NAL_SIZE;
        *length -= AUD_NAL_SIZE;
    }

}
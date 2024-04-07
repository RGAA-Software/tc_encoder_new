#include "nvenc_video_encoder.h"
#include "tc_common_new/data.h"
#include "tc_common_new/image.h"
#include "tc_common_new/log.h"
#include "frame_render/FrameRender.h"
#include "tc_common_new/win32/d3d_debug_helper.h"
#include "tc_common_new/time_ext.h"

namespace tc
{

    NVENCVideoEncoder::NVENCVideoEncoder(const std::shared_ptr<MessageNotifier> &msg_notifier,
                                         const EncoderFeature &encoder_feature)
            : VideoEncoder(msg_notifier, encoder_feature) {

    }

    NVENCVideoEncoder::~NVENCVideoEncoder() = default;

    void NVENCVideoEncoder::Encode(ID3D11Texture2D *tex) {
        static int intdex = 0;
        Transmit(tex, intdex % 60 == 0);
        ++intdex;
    }

    void NVENCVideoEncoder::Encode(uint64_t shared_handle, uint64_t frame_index) {
        // 此外还有使用遍历查找的方式 找到合适的d3d设备
        ComPtr<ID3D11Texture2D> shared_texture;
        shared_texture = OpenSharedTexture(reinterpret_cast<HANDLE>(shared_handle));
        if (!shared_texture) {
            printf("sharedTex nullptr\n");
            return;
        }

        //DebugOutDDS(shared_texture.Get(), "1.dds");

        //
        D3D11_TEXTURE2D_DESC desc;
        shared_texture->GetDesc(&desc);
        //LOGI("format: {}", desc.Format);
        if (encoder_config_.frame_resize) {
            auto beg = TimeExt::GetCurrentTimestamp();
            if (!frame_render_ && d3d11_device_ && d3d11_device_context_) {
                frame_render_ = FrameRender::Make(d3d11_device_.Get(), d3d11_device_context_.Get());
                SIZE target_size = {encoder_config_.encode_width, encoder_config_.encode_height};
                frame_render_->Prepare(target_size, {(long) desc.Width, (long) desc.Height}, desc.Format);
            }

            auto resizerDevice = frame_render_->GetD3D11Device();
            auto resizerDeviceContext = frame_render_->GetD3D11DeviceContext();

            {
                if (!D3D11Texture2DLockMutex(shared_texture)) {
                    printf("frame resize, D3D11Texture2DLockMutex error\n");
                    return;
                }
                std::shared_ptr<void> auto_release_texture2D_mutex((void *) nullptr, [=, this](void *temp) {
                    D3D11Texture2DReleaseMutex(shared_texture);
                });
                auto pre_texture = frame_render_->GetSrcTexture();
                ID3D11Device* dev;
                shared_texture->GetDevice(&dev);
                //LOGI("dev: {}, render dev: {}", (void*)dev, (void*)resizerDevice);
                resizerDeviceContext->CopyResource(pre_texture, shared_texture.Get());
                //DebugOutDDS(pre_texture, "2.dds");
            }
            frame_render_->Draw();
            auto final_texture = frame_render_->GetFinalTexture();
            //DebugOutDDS(final_texture, "3.dds");
            if (!final_texture) {
                LOGE("Draw failed...");
                return;
            }

//            if (!CopyID3D11Texture2D(final_texture)) {
//                printf("CopyID3D11Texture2D error");
//                return;
//            }
            LOGI("shared-copy-resize: {}ms", (TimeExt::GetCurrentTimestamp() - beg));
            Encode(final_texture);
            LOGI("shared-copy-resize-encode: {}ms", (TimeExt::GetCurrentTimestamp() - beg));

        } else {
            auto beg = TimeExt::GetCurrentTimestamp();
            if (!CopyID3D11Texture2D(shared_texture)) {
                printf("CopyID3D11Texture2D error");
                return;
            }
            Encode(texture2d_.Get());
            //LOGI("shared-copy-encode: {}ms", (TimeExt::GetCurrentTimestamp() - beg));
        }

        //DebugOutDDS(texture2d_.Get(), "1.dds");
    }

    bool NVENCVideoEncoder::Initialize(const tc::EncoderConfig &config) {
        VideoEncoder::Initialize(config);
        // ffmpeg 编码的时候，有奇偶数的问题，英伟达有没有，等确定下
        auto format = DxgiFormatToNvEncFormat(static_cast<DXGI_FORMAT>(config.texture_format));
        //MessageBoxA(0, "NvEncoderD3D11", "NvEncoderD3D11", 0);
        LOGI("input_frame_width_ = {}, input_frame_height_ = {}, format = {:x} , m_pD3DRender->GetDevice() = {}",
             input_frame_width_, input_frame_height_, (int) format, (void *) d3d11_device_.Get());
        try {
            nv_necoder_ = std::make_shared<NvEncoderD3D11>(d3d11_device_.Get(), input_frame_width_, input_frame_height_,
                                                           format, 0);// 0 代表延迟
        }
        catch (NVENCException e) {
            printf("NvEnc NvEncoderD3D11 failed. Code=%d %s\n", e.getErrorCode(), e.what());
            return false;
        }
        NV_ENC_INITIALIZE_PARAMS initializeParams = {NV_ENC_INITIALIZE_PARAMS_VER};
        NV_ENC_CONFIG encodeConfig = {NV_ENC_CONFIG_VER};

        encodeConfig.rcParams.rateControlMode = NV_ENC_PARAMS_RC_CBR; // 设置为常量比特率控制模式
        encodeConfig.rcParams.averageBitRate = 10 * 1024 * 1024; // 设置平均码率为4Mbps
        encodeConfig.rcParams.maxBitRate = 30 * 1024 * 1024; // 设置最大码率为5Mbps（适用于VBR模式）
        encodeConfig.rcParams.vbvBufferSize = 1 * 1024 * 1024; // 设置VBV缓冲区大小为1MB

        initializeParams.encodeConfig = &encodeConfig;
        // 设置这里可以改变编码后的输出大小
        FillEncodeConfig(initializeParams, refresh_rate_, out_width_, out_height_, config.bitrate);
        try {
            nv_necoder_->CreateEncoder(&initializeParams);
        }
        catch (NVENCException e) {
            if (e.getErrorCode() == NV_ENC_ERR_INVALID_PARAM) {
                printf("This GPU does not support H.265 encoding. (NvEncoderCuda NV_ENC_ERR_INVALID_PARAM)");
            }
            printf("NvEnc CreateEncoder failed.Code = % d % hs \n", e.getErrorCode(), e.what());
            return false;
        }
        printf("CNvEncoder is successfully initialized.\n");
        // test
        if (encoder_config_.codec_type == tc::EVideoCodecType::kH264) {
            fpOut = std::ofstream("encoded_video_frame.h264", std::ios::binary);
        } else {
            fpOut = std::ofstream("encoded_video_frame.h265", std::ios::binary);
        }
        return true;
    }

    void NVENCVideoEncoder::Exit() {
        Shutdown();
    }

    void NVENCVideoEncoder::Shutdown() {
        std::vector<std::vector<uint8_t>> vPacket;
        if (nv_necoder_)
            nv_necoder_->EndEncode(vPacket);

        for (std::vector<uint8_t> &packet: vPacket) {
            if (fpOut) {
                fpOut.write(reinterpret_cast<char *>(packet.data()), packet.size());
            }
        }
        if (nv_necoder_) {
            nv_necoder_->DestroyEncoder();
            nv_necoder_.reset();
        }
        printf("CNvEncoder::Shutdown\n");
        if (fpOut) {
            fpOut.close();
        }
    }

    void NVENCVideoEncoder::Transmit(ID3D11Texture2D *pTexture, bool insertIDR) {
        std::vector<std::vector<uint8_t>> vPacket;
        const NvEncInputFrame *encoderInputFrame = nv_necoder_->GetNextInputFrame();
        auto pInputTexture = reinterpret_cast<ID3D11Texture2D *>(encoderInputFrame->inputPtr);
        d3d11_device_context_->CopyResource(pInputTexture, pTexture);

        NV_ENC_PIC_PARAMS picParams = {};
        if (/*insertIDR ||*/ insert_idr_) {
            printf("Inserting IDR frame.\n");
            picParams.encodePicFlags = NV_ENC_PIC_FLAG_FORCEIDR;
            insert_idr_ = false;
        }
        nv_necoder_->EncodeFrame(vPacket, &picParams);

        for (std::vector<uint8_t> &packet: vPacket) {
            auto encoded_data = Data::Make((char *) packet.data(), packet.size());
            if (encoder_callback_) {
                auto image = Image::Make(encoded_data, out_width_, out_height_, 3);
                encoder_callback_(image, ++encoded_frame_index_, insertIDR);
            }

            if (fpOut) {
                //fpOut.write(reinterpret_cast<char *>(packet.data()), packet.size());
            }
            //ParseFrameNals(m_codec, packet.data(), (int)packet.size(), targetTimestampNs, insertIDR);
        }
    }

    void
    NVENCVideoEncoder::FillEncodeConfig(NV_ENC_INITIALIZE_PARAMS &initializeParams, int refreshRate, int renderWidth,
                                        int renderHeight, uint64_t bitrate_bps) {
        auto &encodeConfig = *initializeParams.encodeConfig;

        GUID encoderGUID = encoder_config_.codec_type == tc::EVideoCodecType::kH264 ? NV_ENC_CODEC_H264_GUID
                                                                                    : NV_ENC_CODEC_HEVC_GUID;

        GUID qualityPreset;

        // 随着从 P1 到 P7 的转变，性能下降而质量提高
        // See recommended NVENC settings for low-latency encoding.
        // https://docs.nvidia.com/video-technologies/video-codec-sdk/nvenc-video-encoder-api-prog-guide/#recommended-nvenc-settings
        switch (encoder_config_.quality_preset) {
            case 7:
                qualityPreset = NV_ENC_PRESET_P7_GUID;
                break;
            case 6:
                qualityPreset = NV_ENC_PRESET_P6_GUID;
                break;
            case 5:
                qualityPreset = NV_ENC_PRESET_P5_GUID;
                break;
            case 4:
                qualityPreset = NV_ENC_PRESET_P4_GUID;
                break;
            case 3:
                qualityPreset = NV_ENC_PRESET_P3_GUID;
                break;
            case 2:
                qualityPreset = NV_ENC_PRESET_P2_GUID;
                break;
            case 1:
            default:
                qualityPreset = NV_ENC_PRESET_P1_GUID;
                break;
        }

        //Tuning information of NVENC encoding(TuningInfo is not applicable to H264 and HEVC MEOnly mode).
        //MEOnly（Motion Estimation Only）模式是指在视频编码中仅使用运动估计（Motion Estimation）的一种模式。在传统的视频编码过程中，运动估计是编码器中的一个重要步骤，用于分析图像帧之间的运动信息，并生成运动矢量（Motion Vector）。这些运动矢量用于描述帧间的运动差异，从而实现视频帧的压缩。
        //在MEOnly模式中，编码器仅执行运动估计步骤，而不进行实际的编码和压缩操作。它通常用于一些特殊的应用场景，例如视频编辑和后期处理中的运动补偿、运动分析或运动跟踪等。通过仅执行运动估计，可以提取图像帧之间的运动信息，而无需实际进行编码和压缩操作。
        //需要注意的是，MEOnly模式在H.264和HEVC编码中并不适用。H.264和HEVC是一种基于帧间预测的视频编码标准，需要进行更多的编码步骤（如变换、量化、熵编码等）来实现高效的压缩。因此，MEOnly模式通常用于其他类型的编码器或特定的视频处理任务中。


        //低延迟流媒体
        //NV_ENC_TUNING_INFO_LOW_LATENCY       = 2,                                     /**< Tune presets for low latency streaming.
        //超低延迟流媒体
        //NV_ENC_TUNING_INFO_ULTRA_LOW_LATENCY = 3,                                     /**< Tune presets for ultra low latency streaming.

        NV_ENC_TUNING_INFO tuningPreset = NV_ENC_TUNING_INFO_LOW_LATENCY;

        nv_necoder_->CreateDefaultEncoderParams(&initializeParams, encoderGUID, qualityPreset, tuningPreset);

        initializeParams.encodeWidth = initializeParams.darWidth = renderWidth;
        initializeParams.encodeHeight = initializeParams.darHeight = renderHeight;
        initializeParams.frameRateNum = refreshRate;
        initializeParams.frameRateDen = 1;

        // enableWeightedPrediction 是否启用加权预测

        //enableWeightedPrediction 是一个编码参数，用于启用或禁用加权预测（Weighted Prediction）。
        //加权预测是一种视频编码技术，用于更准确地预测帧间的像素值。在帧间预测中，编码器使用先前的参考帧来预测当前帧的像素值。
        //加权预测通过为不同像素位置分配不同的权重，以更好地适应图像中的变化和运动情况。
        //当 enableWeightedPrediction 参数被设置为启用时，编码器将使用加权预测来提高预测的准确性。
        //这可以在某些情况下改善编码的效果，特别是对于具有大幅度运动或复杂纹理的视频内容。
        //然而，需要注意的是，加权预测可能会增加编码的计算复杂性，并略微增加比特率。
        //因此，在某些情况下，禁用加权预测可能更适合，例如在对编码速度有更高要求的实时应用中。

        initializeParams.enableWeightedPrediction = 0;

        // 16 is recommended when using reference frame invalidation. But it has caused bad visual quality.
        // Now, use 0 (use default).

        //在使用参考帧无效化时，推荐使用16作为DPB大小。然而，这可能导致视觉质量下降
        //指定用于编码的 DPB（Decoded Picture Buffer）大小。将其设置为 0 将让驱动程序使用默认的 DPB 大小。
        //对于希望将无效的参考帧作为错误容忍工具的低延迟应用程序，建议使用较大的 DPB 大小，这样编码器可以保留旧的参考帧，以便在最近的帧被无效化时使用。

        uint32_t maxNumRefFrames = 0;
        uint32_t gopLength = NVENC_INFINITE_GOPLENGTH;

        if (encoder_config_.gop_size != -1) {
            gopLength = encoder_config_.gop_size;
        }


        if (encoder_config_.codec_type == tc::EVideoCodecType::kH264) {
            auto &config = encodeConfig.encodeCodecConfig.h264Config;
            //将其设置为1以启用在每个IDR帧中写入序列参数（Sequence Parameter）和图像参数（Picture Parameter）。
            //等再研究下这两种参数
            config.repeatSPSPPS = 1;

            //enableIntraRefresh
            //将其设置为1以启用逐渐解码器刷新（gradual decoder refresh）或帧内刷新（intra refresh）。如果 GOP 结构使用了 B 帧，则此设置将被忽略。
            //逐渐解码器刷新是一种技术，可以在视频序列中定期插入额外的帧内编码帧，以减少编码引起的累积错误和失真

            if (encoder_config_.supports_intra_refresh) {
                config.enableIntraRefresh = 1;
                config.intraRefreshPeriod = refreshRate * 10;
                config.intraRefreshCnt = refreshRate;
            }

            //CABAC 提供了更高的编码效率，但需要更多的计算资源和硬件支持。而 CAVLC 具有较低的计算复杂度和硬件要求，适用于资源受限或对编码效率要求不高的场景。
            //在选择熵编码模式时，可以根据具体应用的需求和硬件平台的支持来进行选择。

            config.entropyCodingMode = NV_ENC_H264_ENTROPY_CODING_MODE_CAVLC;
            config.maxNumRefFrames = maxNumRefFrames;
            config.idrPeriod = gopLength;

            // api version = 12

            //设置为1以在比特流中插入填充数据。
            //该标志仅在使用CBR（恒定比特率）的其中一种速率控制模式（
            //NV_ENC_PARAMS_RC_CBR、NV_ENC_PARAMS_RC_CBR_HQ、NV_ENC_PARAMS_RC_CBR_LOWDELAY_HQ）
            //并且NV_ENC_INITIALIZE_PARAMS::frameRateNum和NV_ENC_INITIALIZE_PARAMS::frameRateDen都设置为非零值时才生效。
            //在NV_ENC_INITIALIZE_PARAMS::enableOutputInVidmem也设置的情况下设置此字段目前不受支持，会导致::NvEncInitializeEncoder()返回错误。
            // 这里先不启用了，不知道会不会 影响客户端解码。如果启用了，在比特流中 插入了填充数据，那么客户端解码时候，应该如何使用呢？

            // config.enableFillerDataInsertion;
        } else {
            auto &config = encodeConfig.encodeCodecConfig.hevcConfig;
            config.repeatSPSPPS = 1;
            if (encoder_config_.supports_intra_refresh) {
                config.enableIntraRefresh = 1;
                // Do intra refresh every 10sec.
                config.intraRefreshPeriod = refreshRate * 10;
                config.intraRefreshCnt = refreshRate;
            }
            config.maxNumRefFramesInDPB = maxNumRefFrames;
            config.idrPeriod = gopLength;
        }
        encodeConfig.gopLength = gopLength;
        encodeConfig.frameIntervalP = 1; // forbidden B frame
        // 恒定码率 还是 可变码率
        //return; // no error
        switch (encoder_config_.rate_control_mode) {
            case tc::ERateControlMode::kRateControlModeCbr:
                encodeConfig.rcParams.rateControlMode = NV_ENC_PARAMS_RC_CBR;
                break;
            case tc::ERateControlMode::kRateControlModeVbr:
                encodeConfig.rcParams.rateControlMode = NV_ENC_PARAMS_RC_VBR;
                // 在 NVIDIA Video Codec (NVENC) SDK 中，targetQuality 是用于可变比特率（VBR）模式的参数设置之一。
                // Target CQ (Constant Quality) level for VBR mode (range 0-51 with 0-automatic)
                // targetQuality 值越大编码质量越好
                // encodeConfig.rcParams.targetQuality = 28;
                if (encoder_config_.target_quality != -1) {
                    encodeConfig.rcParams.targetQuality = encoder_config_.target_quality;
                }

                break;
            case tc::ERateControlMode::kRateControlModeConstQp:
                encodeConfig.rcParams.rateControlMode = NV_ENC_PARAMS_RC_CONSTQP;
                break;
            default:
                break;
        }
//	_NV_ENC_MULTI_PASS 多次编码设置   8.x版本的api没有这个参数
//	NV_ENC_MULTI_PASS_DISABLED：表示禁用多次编码过程。在单次编码过程中，每个视频帧只编码一次，没有额外的预处理或后处理步骤。
//	使用此标志时，编码器不会保存任何状态信息，并且不需要执行多次编码。
//
//	NV_ENC_TWO_PASS_QUARTER_RESOLUTION：表示使用两次编码过程，其中第一次编码过程使用四分之一的分辨率进行编码。
//	在第一次编码过程中，视频帧以较低的分辨率进行编码，以获得初步的编码统计信息。然后，编码器使用这些统计信息来优化第二次编码过程，
//	该过程使用完整的分辨率进行编码。这种方法可以在保持较高质量的同时减少编码的计算成本。
//
//	NV_ENC_TWO_PASS_FULL_RESOLUTION：表示使用两次编码过程，其中两次编码过程均使用完整的分辨率进行编码。
//	在第一次编码过程中，视频帧以完整的分辨率进行编码，并生成编码统计信息。然后，编码器使用这些统计信息来优化第二次编码过程，
//	该过程仍使用完整的分辨率进行编码。这种方法可以进一步提高编码质量，但需要更多的计算资源。
        switch (encoder_config_.multi_pass) {
            case tc::ENvdiaEncMultiPass::kMultiPassDisabled:
                encodeConfig.rcParams.multiPass = NV_ENC_MULTI_PASS_DISABLED;
                break;
            case tc::ENvdiaEncMultiPass::kTwoPassQuarterResolution:
                encodeConfig.rcParams.multiPass = NV_ENC_TWO_PASS_QUARTER_RESOLUTION;
                break;
            case tc::ENvdiaEncMultiPass::kTwoPassFullResolution:
                encodeConfig.rcParams.multiPass = NV_ENC_TWO_PASS_FULL_RESOLUTION;
                break;
            default:
                break;
        }

//	在单帧 VBV（Video Buffering Verifier）和 CBR（Constant Bit Rate）速率控制模式下，指定 I 帧比 P 帧的比特率。对于低延迟调优信息，默认设置为 2；
//	对于超低延迟调优信息，默认设置为 1。
        encodeConfig.rcParams.lowDelayKeyFrameScale = 1;
        uint32_t maxFrameSize = static_cast<uint32_t>(bitrate_bps / refreshRate);
        encodeConfig.rcParams.vbvBufferSize = maxFrameSize * 1.1;
        encodeConfig.rcParams.vbvInitialDelay = maxFrameSize * 1.1;
        encodeConfig.rcParams.maxBitRate = static_cast<uint32_t>(bitrate_bps);
        encodeConfig.rcParams.averageBitRate = static_cast<uint32_t>(bitrate_bps);

//	在 NVIDIA Video Codec (NVENC) SDK 中，_NV_ENC_RC_PARAMS 结构体中的 enableAQ 字段用于启用自适应量化（Adaptive Quantization）。
//	自适应量化是一种编码技术，用于根据图像内容的复杂性动态调整量化参数。量化参数控制编码过程中的压缩比和图像质量之间的权衡。通过自适应量化，
//	编码器可以根据图像的空间特征和运动信息，针对不同区域和帧间预测类型采用不同的量化参数，从而提高编码效率和图像质量。
//	enableAQ 字段的含义是是否启用自适应量化。如果将其设置为非零值（通常为 1），则表示启用自适应量化；如果将其设置为零，则表示禁用自适应量化。
//	通过启用自适应量化，编码器可以根据图像内容进行动态调整，以获得更好的压缩效率和图像质量。但请注意，自适应量化可能会增加编码的计算复杂性，并可能对编码性能产生一定的影响。因此，在使用 enableAQ 字段时需要权衡编码质量、性能和计算资源的需求。
//
        encodeConfig.rcParams.enableAQ = 0;
        if (encoder_config_.enable_adaptive_quantization) {
            encodeConfig.rcParams.enableAQ = 1;
        }


//	在 NVIDIA Video Codec (NVENC) SDK 中，_NV_ENC_RC_PARAMS 结构体中的 enableTemporalAQ 字段用于启用时域自适应量化（Temporal Adaptive Quantization）。
//	时域自适应量化是一种编码技术，基于视频序列中帧间相关性的变化，动态调整量化参数。与空间自适应量化（Adaptive Quantization）不同，
//	时域自适应量化考虑了帧间的相关性，以提高编码效率和图像质量。
//	enableTemporalAQ 字段的含义是是否启用时域自适应量化。如果将其设置为非零值（通常为 1），则表示启用时域自适应量化；如果将其设置为零，则表示禁用时域自适应量化。
//	启用时域自适应量化可以在编码过程中根据帧间相关性进行动态调整，以优化压缩效率和图像质量。
//	它可以根据帧间运动信息和相关性来调整量化参数，以便更好地保留运动细节和细微差异，从而提高编码效果。
//	需要注意的是，启用时域自适应量化可能会增加编码器的计算复杂性，并可能对编码性能产生一定的影响。
//	因此，在使用 enableTemporalAQ 字段时需要根据具体情况权衡编码质量、性能和计算资源的需求。

        // 这里先不启用
        // encodeConfig.rcParams.enableTemporalAQ = 1;

//  qp 通常情况下，QP（量化参数）值越大，编码质量越差
//	QP参数调节，指的是量化参数调节。它主要是来调节图像的细节，最终达到调节画面质量的作用。
//	QP值和比特率成反比，QP值越小画面质量越高；反之QP值越大，画面质量越低。
//	而且随着视频源复杂度，这种反比的关系会更加明显。QP调节是改变画面质量最常用的手段之一
//
//	在 NVIDIA Video Codec (NVENC) SDK 中，constQP、maxQP 和 minQP 是用于控制量化参数（QP）的配置参数。
//	constQP：指定恒定量化参数（Constant QP），即所有帧使用相同的固定量化参数值。这是一种简单的编码模式，
//	可以提供一致的编码质量，但无法根据图像内容进行自适应调整。
//	maxQP 和 minQP：用于指定动态量化参数的范围，即允许的最大和最小量化参数值。编码器可以在该范围内根据图像内容进行自适应调整，以平衡压缩率和图像质量。
//这三个参数之间的关系是：
//	maxQP 必须大于或等于 minQP，以确保范围的有效性。
//	constQP 可以设置为介于 maxQP 和 minQP 之间的任意值，即在动态量化参数范围内选择一个恒定的值。
//	如果同时设置了 constQP 和动态量化参数范围（maxQP 和 minQP），编码器将优先使用 constQP 的值，并忽略动态范围的调整。
//	总结起来，constQP 提供了一个固定的量化参数值，而 maxQP 和 minQP 定义了动态范围，允许编码器根据图像内容进行自适应调整。
//	这些参数的使用取决于应用的需求和目标，需要权衡编码质量、压缩率和资源消耗。

        if (encoder_config_.const_qp != -1) {
            uint32_t constQP = encoder_config_.const_qp;
            encodeConfig.rcParams.constQP = {constQP, constQP, constQP};
        } else if (encoder_config_.max_qp != -1 && encoder_config_.min_qp != -1) {
            uint32_t minQP = encoder_config_.min_qp;
            uint32_t maxQP = encoder_config_.max_qp;
            encodeConfig.rcParams.minQP = {minQP, minQP, minQP};
            encodeConfig.rcParams.maxQP = {maxQP, maxQP, maxQP};
            encodeConfig.rcParams.enableMinQP = 1;
            encodeConfig.rcParams.enableMaxQP = 1;
        }
    }


    NV_ENC_BUFFER_FORMAT NVENCVideoEncoder::DxgiFormatToNvEncFormat(DXGI_FORMAT dxgiFormat) {
        switch (dxgiFormat) {
            case DXGI_FORMAT_NV12:
                return NV_ENC_BUFFER_FORMAT_NV12;
            case DXGI_FORMAT_B8G8R8A8_UNORM:
                return NV_ENC_BUFFER_FORMAT_ARGB;
            case DXGI_FORMAT_R8G8B8A8_UNORM:
            case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB:
                return NV_ENC_BUFFER_FORMAT_ABGR;
            case DXGI_FORMAT_R10G10B10A2_UNORM:
                return NV_ENC_BUFFER_FORMAT_ABGR10;
            default:
                return NV_ENC_BUFFER_FORMAT_UNDEFINED;
        }
    }


} // namespace tc
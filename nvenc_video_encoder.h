//
// Created by Administrator on 2024/1/6.
//

#ifndef TC_APPLICATION_NVENC_VIDEO_ENCODER_H
#define TC_APPLICATION_NVENC_VIDEO_ENCODER_H
#include "video_encoder.h"
#include "NvEncoderD3D11.h"
#include <fstream>

namespace tc {

class NVENCVideoEncoder : public VideoEncoder {
public:
    NVENCVideoEncoder(const EncoderFeature& encoder_feature);

    ~NVENCVideoEncoder();

    bool Initialize(const tc::EncoderConfig& config) override;

    void Encode(uint64_t handle, uint64_t frame_index) override;
    void Encode(ID3D11Texture2D* tex2d) override;

    void Transmit(ID3D11Texture2D* pTexture,  bool insertIDR);
    void Exit() override;
private:
    void Shutdown();
    void FillEncodeConfig(NV_ENC_INITIALIZE_PARAMS& initializeParams, int refreshRate, int renderWidth, int renderHeight, uint64_t bitrate_bps);

    //格式转换非常重要, 如果格式不匹配，那么编码的时候 copy纹理这一步 CopyResource(pInputTexture, pTexture);就异常，保存出来的h264文件播放黑屏
    NV_ENC_BUFFER_FORMAT DxgiFormatToNvEncFormat(DXGI_FORMAT dxgiFormat);

    std::ofstream fpOut;
    std::shared_ptr<NvEncoder> nv_necoder_;
};

}
#endif //TC_APPLICATION_NVENC_VIDEO_ENCODER_H

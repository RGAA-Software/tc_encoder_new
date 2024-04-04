#pragma once
#include "VideoEncoder.h"

#include "amf/common/AMFFactory.h"
#include "amf/include/components/VideoEncoderVCE.h"
#include "amf/include/components/VideoEncoderHEVC.h"
#include "amf/include/components/VideoConverter.h"
//#include "amf/common/AMFSTL.h"
#include "amf/common/Thread.h"

#include <thread>
#include <fstream>
#include <functional>

typedef std::function<void (amf::AMFData *)> AMFTextureReceiver;

class AMFTextureEncoder {
public:
	AMFTextureEncoder(const amf::AMFContextPtr &amfContext
		, int width, int height
		, amf::AMF_SURFACE_FORMAT inputFormat
		, AMFTextureReceiver receiver
		, Running::EncoderType codec);
	~AMFTextureEncoder();

	void Start();
	void Shutdown();
	void Submit(amf::AMFData *data);
private:
	amf::AMFComponentPtr m_amfEncoder;
	std::thread *m_thread = NULL;
	AMFTextureReceiver m_receiver;
	Running::EncoderType m_codec;

	void Run();
};

class AMFTextureConverter {
public:
	AMFTextureConverter(const amf::AMFContextPtr &amfContext
		, int width, int height
		, amf::AMF_SURFACE_FORMAT inputFormat, amf::AMF_SURFACE_FORMAT outputFormat
		, AMFTextureReceiver receiver);
	~AMFTextureConverter();

	void Start();
	void Shutdown();
	void Submit(amf::AMFData *data);
private:
	amf::AMFComponentPtr m_amfConverter;
	std::thread *m_thread = NULL;
	AMFTextureReceiver m_receiver;

	void Run();
};

// Video encoder for AMD VCE.
class VideoEncoderVCE : public VideoEncoder
{
public:
	VideoEncoderVCE(std::shared_ptr<CD3DRender> pD3DRender, EncodeDataCallback cbk, int width, int height, Running::EncoderType codec);
	~VideoEncoderVCE();

	bool Initialize();
	void Shutdown();

	void Transmit(ID3D11Device* device, ID3D11DeviceContext* context, ID3D11Texture2D *pTexture, uint64_t frameIndex, bool insertIDR);
	void Transmit(uint64_t handle, uint64_t frame_index);
	bool InitEncoder(ID3D11Texture2D* texture) override;
	void Receive(amf::AMFData *data);

private:
	void EncodeTextureHandle(uint64_t handle, uint64_t frame_index);

private:
	amf::AMF_SURFACE_FORMAT convert_input_format = amf::AMF_SURFACE_BGRA;// AMF_SURFACE_RGBA;
	amf::AMF_SURFACE_FORMAT encoder_input_format =  amf::AMF_SURFACE_BGRA;// amf::AMF_SURFACE_NV12;
	
	static const wchar_t *START_TIME_PROPERTY;
	static const wchar_t *FRAME_INDEX_PROPERTY;
	static const wchar_t* IS_KEY_FRAME;

	const double MILLISEC_TIME = 10000;

	amf::AMFContextPtr m_amfContext = nullptr;
	std::shared_ptr<AMFTextureEncoder> m_encoder = nullptr;
	std::shared_ptr<AMFTextureConverter> m_converter = nullptr;

	std::ofstream fpOut;

	std::shared_ptr<CD3DRender> m_d3dRender = nullptr;

	void ApplyFrameProperties(const amf::AMFSurfacePtr &surface, bool insertIDR);
	void SkipAUD(char **buffer, int *length);
};


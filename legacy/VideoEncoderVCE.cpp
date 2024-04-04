#include "VideoEncoderVCE.h"

#include "common/rlog.h"
#include "common/rtime.h"
#include "common/RData.h"
#include "D3DTextureDebug.h"

#include <combaseapi.h>
#include <atlbase.h>
#include <fstream>
#include <iostream>

#define DEBUG_FILE 0

#define AMF_THROW_IF(expr) {AMF_RESULT res = expr;\
if(res != AMF_OK){LOG_INFO("ERROR++++++++++++++++++++++++++++++++++++++ %d", res); assert(0);}}

const wchar_t *VideoEncoderVCE::START_TIME_PROPERTY = L"StartTimeProperty";
const wchar_t *VideoEncoderVCE::FRAME_INDEX_PROPERTY = L"FrameIndexProperty";
const wchar_t* VideoEncoderVCE::IS_KEY_FRAME = L"IsKeyFrame";

using namespace tc;

//
// AMFTextureEncoder
//

AMFTextureEncoder::AMFTextureEncoder(const amf::AMFContextPtr &amfContext
	, int width, int height
	, amf::AMF_SURFACE_FORMAT inputFormat
	, AMFTextureReceiver receiver
	, Running::EncoderType codec) : m_receiver(receiver)
{
	m_codec = codec;
	const wchar_t *pCodec;

	amf_int32 frameRateIn = 60;// Settings::Instance().m_encodeFPS;
	amf_int64 bitRateIn = 10000000L; //5Mbits// Settings::Instance().m_encodeBitrateInMBits * 1000000L; // in bits

	LOG_INFO("the codec is : %d", m_codec);
	switch (m_codec) {
	case Running::EncoderType::kEncodeH264:
		pCodec = AMFVideoEncoderVCE_AVC;
		break;
	case Running::EncoderType::kEncodeH265:
		pCodec = AMFVideoEncoder_HEVC;
		break;
	default:
		LOG_INFO("Error codec type ...");
		//throw MakeException(L"Unsupported video encoding %d", Settings::Instance().m_codec);
	}

	// Create encoder component.
	AMF_THROW_IF(g_AMFFactory.GetFactory()->CreateComponent(amfContext, pCodec, &m_amfEncoder));

	if (m_codec == Running::kEncodeH264)
	{
		m_amfEncoder->SetProperty(AMF_VIDEO_ENCODER_USAGE, AMF_VIDEO_ENCODER_USAGE_ULTRA_LOW_LATENCY);

		m_amfEncoder->SetProperty(AMF_VIDEO_ENCODER_B_PIC_PATTERN, 0);
		m_amfEncoder->SetProperty(AMF_VIDEO_ENCODER_QUALITY_PRESET, AMF_VIDEO_ENCODER_QUALITY_PRESET_QUALITY);

		m_amfEncoder->SetProperty(AMF_VIDEO_ENCODER_TARGET_BITRATE, bitRateIn);
		m_amfEncoder->SetProperty(AMF_VIDEO_ENCODER_FRAMESIZE, ::AMFConstructSize(width, height));
		m_amfEncoder->SetProperty(AMF_VIDEO_ENCODER_FRAMERATE, ::AMFConstructRate(frameRateIn, 1));

		//m_amfEncoder->SetProperty(AMF_VIDEO_ENCODER_PROFILE, AMF_VIDEO_ENCODER_PROFILE_HIGH);
		//m_amfEncoder->SetProperty(AMF_VIDEO_ENCODER_PROFILE_LEVEL, 51);
	}
	else
	{
		m_amfEncoder->SetProperty(AMF_VIDEO_ENCODER_HEVC_USAGE, AMF_VIDEO_ENCODER_HEVC_USAGE_ULTRA_LOW_LATENCY);

		m_amfEncoder->SetProperty(AMF_VIDEO_ENCODER_HEVC_QUALITY_PRESET, AMF_VIDEO_ENCODER_HEVC_QUALITY_PRESET_QUALITY);

		m_amfEncoder->SetProperty(AMF_VIDEO_ENCODER_HEVC_TARGET_BITRATE, bitRateIn);
		m_amfEncoder->SetProperty(AMF_VIDEO_ENCODER_HEVC_FRAMESIZE, ::AMFConstructSize(width, height));
		m_amfEncoder->SetProperty(AMF_VIDEO_ENCODER_HEVC_FRAMERATE, ::AMFConstructRate(frameRateIn, 1));

		//m_amfEncoder->SetProperty(AMF_VIDEO_ENCODER_HEVC_TIER, AMF_VIDEO_ENCODER_HEVC_TIER_HIGH);
		//m_amfEncoder->SetProperty(AMF_VIDEO_ENCODER_HEVC_PROFILE_LEVEL, AMF_LEVEL_5);
	}
	AMF_THROW_IF(m_amfEncoder->Init(inputFormat, width, height));

	LOG_INFO("Initialized AMFTextureEncoder.");
}

AMFTextureEncoder::~AMFTextureEncoder()
{
	if (m_amfEncoder) {
		m_amfEncoder->Terminate();
	}
	
}

void AMFTextureEncoder::Start()
{
	m_thread = new std::thread(&AMFTextureEncoder::Run, this);
}

void AMFTextureEncoder::Shutdown()
{
	LOG_INFO("AMFTextureEncoder::Shutdown() m_amfEncoder->Drain");
	m_amfEncoder->Drain();
	LOG_INFO("AMFTextureEncoder::Shutdown() m_thread->join");
	m_thread->join();
	LOG_INFO("AMFTextureEncoder::Shutdown() joined.");
	delete m_thread;
	m_thread = NULL;
}

void AMFTextureEncoder::Submit(amf::AMFData *data)
{
	while (true)
	{
		auto res = m_amfEncoder->SubmitInput(data);
		if (res == AMF_INPUT_FULL)
		{
			return;
		}
		else
		{
			break;
		}
	}
}

void AMFTextureEncoder::Run()
{
	LOG_INFO("Start AMFTextureEncoder thread. Thread Id=%d", GetCurrentThreadId());
	amf::AMFDataPtr data;
	while (true)
	{
		auto res = m_amfEncoder->QueryOutput(&data);
		if (res == AMF_EOF)
		{
			LOG_INFO("m_amfEncoder->QueryOutput returns AMF_EOF.");
			return;
		}

		if (data != NULL)
		{
			m_receiver(data);
		}
		else
		{
			Sleep(1);
		}
	}
}

//
// AMFTextureConverter
//

AMFTextureConverter::AMFTextureConverter(const amf::AMFContextPtr &amfContext
	, int width, int height
	, amf::AMF_SURFACE_FORMAT inputFormat, amf::AMF_SURFACE_FORMAT outputFormat
	, AMFTextureReceiver receiver) : m_receiver(receiver)
{
	AMF_THROW_IF(g_AMFFactory.GetFactory()->CreateComponent(amfContext, AMFVideoConverter, &m_amfConverter));

	AMF_THROW_IF(m_amfConverter->SetProperty(AMF_VIDEO_CONVERTER_MEMORY_TYPE, amf::AMF_MEMORY_DX11));
	AMF_THROW_IF(m_amfConverter->SetProperty(AMF_VIDEO_CONVERTER_OUTPUT_FORMAT, outputFormat));
	AMF_THROW_IF(m_amfConverter->SetProperty(AMF_VIDEO_CONVERTER_OUTPUT_SIZE, ::AMFConstructSize(width, height)));

	AMF_THROW_IF(m_amfConverter->Init(inputFormat, width, height));

	LOG_INFO("Initialized AMFTextureConverter.");
}

AMFTextureConverter::~AMFTextureConverter()
{
	if (m_amfConverter) {
		m_amfConverter->Terminate();
	}
}

void AMFTextureConverter::Start()
{
	m_thread = new std::thread(&AMFTextureConverter::Run, this);
}

void AMFTextureConverter::Shutdown()
{
	LOG_INFO("AMFTextureConverter::Shutdown() m_amfConverter->Drain");
	m_amfConverter->Drain();
	LOG_INFO("AMFTextureConverter::Shutdown() m_thread->join");
	m_thread->join();
	LOG_INFO("AMFTextureConverter::Shutdown() joined.");
	delete m_thread;
	m_thread = NULL;
}

void AMFTextureConverter::Submit(amf::AMFData *data)
{
	while (true)
	{
		auto res = m_amfConverter->SubmitInput(data);
		if (res == AMF_INPUT_FULL)
		{
			return;
		}
		else
		{
			break;
		}
	}
}

void AMFTextureConverter::Run()
{
	LOG_INFO("Start AMFTextureConverter thread. Thread Id=%d", GetCurrentThreadId());
	amf::AMFDataPtr data;
	while (true)
	{
		auto res = m_amfConverter->QueryOutput(&data);
		if (res == AMF_EOF)
		{
			LOG_INFO("m_amfConverter->QueryOutput returns AMF_EOF.");
			return;
		}

		if (data != NULL)
		{
			m_receiver(data);
		}
		else
		{
			Sleep(1);
		}
	}
}

//
// VideoEncoderVCE
//

VideoEncoderVCE::VideoEncoderVCE(std::shared_ptr<CD3DRender> d3dRender, EncodeDataCallback cbk, int width, int height, Running::EncoderType codec)
	: VideoEncoder(codec), m_d3dRender(d3dRender)
{
	m_width = width;
	m_height = height;
	m_Listener = cbk;
}

VideoEncoderVCE::~VideoEncoderVCE()
{
	if (m_amfContext) {
		m_amfContext->Terminate();
		m_amfContext->Release();
	}
}

amf::AMF_SURFACE_FORMAT DXGI_FORMAT_to_AMF_FORMAT(DXGI_FORMAT format) {
	if (DXGI_FORMAT_R8G8B8A8_UNORM == format || DXGI_FORMAT_R8G8B8A8_UNORM_SRGB == format) {
		return amf::AMF_SURFACE_RGBA;
	}
	else if (DXGI_FORMAT_B8G8R8A8_UNORM == format || DXGI_FORMAT_B8G8R8A8_UNORM_SRGB == format) {
		return amf::AMF_SURFACE_BGRA;
	}
	else {
		LOG_ERROR("DONT known the dxgi format !");
		return amf::AMF_SURFACE_UNKNOWN;
	}
}

bool VideoEncoderVCE::Initialize()
{
	return true;
}

bool VideoEncoderVCE::InitEncoder(ID3D11Texture2D* texture) {

	LOG_INFO("Initializing VideoEncoderVCE.");
	AMF_THROW_IF(g_AMFFactory.Init());

	::amf_increase_timer_precision();

	AMF_THROW_IF(g_AMFFactory.GetFactory()->CreateContext(&m_amfContext));
	AMF_THROW_IF(m_amfContext->InitDX11(m_d3dRender->GetDevice()));

	D3D11_TEXTURE2D_DESC desc;
	texture->GetDesc(&desc);
	convert_input_format = DXGI_FORMAT_to_AMF_FORMAT(desc.Format);
	encoder_input_format = DXGI_FORMAT_to_AMF_FORMAT(desc.Format);

	LOG_INFO("create encoder format : %d", encoder_input_format);

	m_encoder = std::make_shared<AMFTextureEncoder>(m_amfContext
		, m_width, m_height
		, encoder_input_format, std::bind(&VideoEncoderVCE::Receive, this, std::placeholders::_1), m_codec);
	m_converter = std::make_shared<AMFTextureConverter>(m_amfContext
		, m_width, m_height
		, convert_input_format, encoder_input_format
		, std::bind(&AMFTextureEncoder::Submit, m_encoder.get(), std::placeholders::_1));

	m_encoder->Start();
	m_converter->Start();

#if DEBUG_FILE
	fpOut = std::ofstream("tttt.h264", std::ios::binary | std::ios::beg);
	if (!fpOut)
	{
		LOG_INFO("Unable to open output file");
	}
#endif

	LOG_INFO("Successfully initialized VideoEncoderVCE.");
	OnInitSuccess();
	return true;
}

void VideoEncoderVCE::Shutdown()
{
	LOG_INFO("Shutting down VideoEncoderVCE.");

	m_encoder->Shutdown();
	m_converter->Shutdown();

	amf_restore_timer_precision();
#if DEBUG_FILE
	if (fpOut) {
		fpOut.close();
	}
#endif
	LOG_INFO("Successfully shutdown VideoEncoderVCE.");
}

void VideoEncoderVCE::Transmit(uint64_t handle, uint64_t frame_index) {
	if (!handle) { 
		return; 
	}
	EncodeTextureHandle(handle, frame_index);
}

void VideoEncoderVCE::Transmit(ID3D11Device*, ID3D11DeviceContext*, ID3D11Texture2D* texture, uint64_t frame_index, bool)
{
	HANDLE tex_handle = NULL;
	CComPtr<IDXGIResource> resource = nullptr;
	auto hr = texture->QueryInterface(__uuidof(IDXGIResource), reinterpret_cast<void**>(&resource));
	if (!FAILED(hr)) {
		hr = resource->GetSharedHandle(&tex_handle);
		if (FAILED(hr)) {
			LOG_INFO("GetSharedHandle Failed !!!");
			return;
		}
	}
	if (!tex_handle) {
		return;
	}

	EncodeTextureHandle((uint64_t)tex_handle, frame_index);
}

void VideoEncoderVCE::EncodeTextureHandle(uint64_t handle, uint64_t frame_index) {

	CComPtr<ID3D11Texture2D> target_texture = NULL;
	if (FAILED(m_d3dRender->GetDevice()->OpenSharedResource((HANDLE)handle, __uuidof(ID3D11Texture2D), (void**)&target_texture))) {
		LOG_INFO("OpenSharedResource failed !");
		return;
	}

	// init encoder if needed
	if (m_encoder == nullptr || m_converter == nullptr) {
		bool result = InitEncoder(target_texture);
		if (!result) {
			return;
		}
	}

	amf::AMFSurfacePtr surface = NULL;
	// Surface is cached by AMF.
	AMF_THROW_IF(m_amfContext->AllocSurface(amf::AMF_MEMORY_DX11, convert_input_format, m_width, m_height, &surface));
	ID3D11Texture2D* textureDX11 = (ID3D11Texture2D*)surface->GetPlaneAt(0)->GetNative(); // no reference counting - do not Release()

	CComPtr<IDXGIKeyedMutex> pKeyedMutex;
	if (SUCCEEDED(target_texture->QueryInterface(__uuidof(IDXGIKeyedMutex), (void**)&pKeyedMutex))) {
		HRESULT hr = pKeyedMutex->AcquireSync(0, 1);
		if (hr != S_OK) {
			LOG_INFO("ACQUIRESYNC FAILED!!! hr=%p ", hr);
			return;
		}
	}

	D3D11_TEXTURE2D_DESC desc;
	textureDX11->GetDesc(&desc);
	//printf("the format of encoder : %d, width : %d, height : %d \n", desc.Format, desc.Width, desc.Height);

	m_d3dRender->GetContext()->CopyResource(textureDX11, target_texture);

	// print to check the format equal or not
	//D3DTextureDebug::PrintTextureDesc(textureDX11);
	//D3DTextureDebug::PrintTextureDesc(texture);

	// save to dds 
	//D3DTextureDebug::SaveAsDDS(m_d3dRender->GetContext(), textureDX11, L"aaaa.dds");
	//D3DTextureDebug::SaveAsDDS(m_d3dRender->GetContext(), target_texture, L"bbbb.dds");

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

	if (pKeyedMutex) {
		pKeyedMutex->ReleaseSync(0);
	}
}

static uint64_t last_time = tc::GetCurrentTimestamp();
static int fps = 0;

void VideoEncoderVCE::Receive(amf::AMFData *data)
{
	amf_pts current_time = amf_high_precision_clock();
	amf_pts start_time = 0;
	uint64_t frameIndex;
	bool is_key_frame;
	data->GetProperty(START_TIME_PROPERTY, &start_time);
	data->GetProperty(FRAME_INDEX_PROPERTY, &frameIndex);
	data->GetProperty(IS_KEY_FRAME, &is_key_frame);

	amf::AMFBufferPtr buffer(data); // query for buffer interface

	char *p = reinterpret_cast<char*>(buffer->GetNative());
	int length = buffer->GetSize();

	SkipAUD(&p, &length);

	//LOG_INFO("VCE encode latency: %.4f ms. Size=%d bytes frameIndex=%llu, length : %d", double(current_time - start_time) / MILLISEC_TIME, (int)buffer->GetSize(), frameIndex, length);
#if DEBUG_FILE
	if (fpOut.is_open()) {
		fpOut.write(p, length);
	}
#endif
	if (m_Listener) {
		auto data = tc::Data::Make(p, length);
		//LOG_INFO("data len : %d, real len : %d", data->Size(), length);
		m_Listener(data, frameIndex, is_key_frame);
	}

	fps++;
	uint64_t ct = tc::GetCurrentTimestamp();
	if (ct - last_time > 1000) {
		printf("Recv FPS : %d \n", fps);
		last_time = ct;
		fps = 0;
	}

}

void VideoEncoderVCE::ApplyFrameProperties(const amf::AMFSurfacePtr &surface, bool insertIDR) {
	switch (m_codec) {
	case Running::kEncodeH264:
		// Disable AUD (NAL Type 9) to produce the same stream format as VideoEncoderNVENC.
		surface->SetProperty(AMF_VIDEO_ENCODER_INSERT_AUD, false);
		if (insertIDR) {
			//LOG_INFO("Inserting IDR frame for H.264.");
			surface->SetProperty(AMF_VIDEO_ENCODER_INSERT_SPS, true);
			surface->SetProperty(AMF_VIDEO_ENCODER_INSERT_PPS, true);
			surface->SetProperty(AMF_VIDEO_ENCODER_FORCE_PICTURE_TYPE, AMF_VIDEO_ENCODER_PICTURE_TYPE_IDR);
		}
		break;
	case Running::kEncodeH265:
		// This option is ignored. Maybe a bug on AMD driver.
		surface->SetProperty(AMF_VIDEO_ENCODER_HEVC_INSERT_AUD, false);
		if (insertIDR) {
			//LOG_INFO("Inserting IDR frame for H.265.");
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

	if (m_codec != Running::kEncodeH265) {
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

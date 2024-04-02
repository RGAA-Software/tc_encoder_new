#pragma once

#include "CommonTypes.h"
#include <string>
#include <memory>

namespace tc
{

    class FrameRender {
    public:
        static std::shared_ptr<FrameRender> Make(ID3D11Device *device, ID3D11DeviceContext *context);

        FrameRender(ID3D11Device *device, ID3D11DeviceContext *context);
        ~FrameRender();

        HRESULT InitDx();
        HRESULT Prepare(SIZE targetSize, SIZE originSize, int format);
        HRESULT Draw();

        ID3D11Device *GetD3D11Device() {
            return m_Device;
        }

        ID3D11DeviceContext *GetD3D11DeviceContext() {
            return m_DeviceContext;
        }

        ID3D11Texture2D *GetFinalTexture() {
            return m_FinalTexture;
        }

        ID3D11Texture2D *GetSrcTexture() {
            return m_SrcTexture;
        }

        //HRESULT GenerateResourceView(ID3D11Texture2D* texture, D3D11_SHADER_RESOURCE_VIEW_DESC srcDesc) {
        //	m_SrcTexture = texture;
        //	return m_Device->CreateShaderResourceView(texture, &srcDesc, &m_SrcSrv);
        //}

        //void ReleaseResourceView() {
        //	if (m_SrcSrv) {
        //		m_SrcSrv->Release();
        //		m_SrcSrv = nullptr;
        //	}
        //          m_SrcTexture = nullptr;
        //}

    private:
        HRESULT InitializeDesc(_In_ SIZE size, _Out_ D3D11_TEXTURE2D_DESC *pTargetDesc, int format);

        HRESULT MakeRTV();

        void SetViewPort(SIZE size);

        HRESULT InitShaders();

        void CleanRefs();

        ID3D11Device *m_Device;
        ID3D11DeviceContext *m_DeviceContext;
        ID3D11SamplerState *m_SamplerLinear;
        ID3D11BlendState *m_BlendState;
        ID3D11VertexShader *m_VertexShader;
        ID3D11PixelShader *m_PixelShader;
        ID3D11InputLayout *m_InputLayout;
        ID3D11Texture2D *m_TargetTexture;
        ID3D11RenderTargetView *m_RTV;
        ID3D11Texture2D *m_SrcTexture;
        ID3D11ShaderResourceView *m_SrcSrv;
        ID3D11Texture2D *m_FinalTexture{nullptr};

        ID3D11Buffer *VertexBuffer = nullptr;
    };

}
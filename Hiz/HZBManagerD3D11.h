#pragma once

class HZBManagerD3D11
{
public:
    HZBManagerD3D11(ID3D11Device* pDevice);
    virtual ~HZBManagerD3D11(void);

    void Initialize(UINT width = 512, UINT height = 256, UINT initialBoundsBufferSize = 1000);

    void AddOccluder();
    
    void Render(ID3D11DeviceContext* pImmediateContext, const D3DXMATRIX& view, const D3DXMATRIX& projection);
    
    void BeginCullingQuery(
        ID3D11DeviceContext* pImmediateContext,
        UINT boundsSize,
        const D3DXVECTOR4* bounds,
        const D3DXMATRIX& view,
        const D3DXMATRIX& projection);

    void EndCullingQuery(ID3D11DeviceContext* pImmediateContext, UINT boundsSize, float* pResults);

    void SetRenderOccluders(bool bRenderOcculders);

protected:
    void Reset();

    void InitializeDownsamplingCL(const D3D11_TEXTURE2D_DESC* hizDesc);
    
    void LoadDepthEffect();
    void LoadMipmapEffect();
    void LoadCullEffect();

    void RenderOccluders(ID3D11DeviceContext* pImmediateContext, const D3DXMATRIX& view, const D3DXMATRIX& projection);
    void RenderMipmap(ID3D11DeviceContext* pImmediateContext);

    void EnsureCullingBuffers(UINT desiredSize);

    ID3D11Device* m_pDevice;

    UINT m_width;
    UINT m_height;
    UINT m_boundsSize;

    UINT m_mipLevels;
    ID3D11RenderTargetView**   m_pMipLevelRTV;
    ID3D11ShaderResourceView** m_pMipLevelSRV;

    ID3D11CommandList* m_pMipmapCL;

    ID3D11Texture2D*        m_pHizBuffer;
    ID3D11Texture2D*        m_pHizDepthBuffer;
    ID3D11DepthStencilView* m_pHizDepthDSV;

    ID3DX11Effect*     m_pDepthEffect;
    ID3D11InputLayout* m_pDepthLayout;
    ID3D11Buffer*      m_pOccluderVertexBuffer;

    ID3DX11Effect*     m_pHizMipmapEffect;
    ID3D11InputLayout* m_pHizMipmapLayout;
    ID3D11Buffer*      m_pHizMipmapVertexBuffer;

    ID3D11ComputeShader* m_pHizCullCS;
    ID3D11Buffer*        m_pCS_CB_HizCull;
    ID3D11SamplerState*  m_pHizCullSampler;

    ID3D11Buffer* m_pCullingBoundsBuffer;
    ID3D11Buffer* m_pCullingResultsBuffer;
    ID3D11Buffer* m_pCullingReadableResultsBuffer;

    ID3D11ShaderResourceView* m_pCullingBoundsSRV;
    ID3D11UnorderedAccessView* m_pCullingResultsUAV;
    ID3D11ShaderResourceView* m_pHizSRV;

    bool m_bRenderOccluders;
};

#pragma once

class HZBManagerD3D9
{
public:
    HZBManagerD3D9(IDirect3DDevice9* pDevice);
    virtual ~HZBManagerD3D9(void);

    void AddOccluder();

    void Render(const D3DXMATRIX& view, const D3DXMATRIX& projection);

    void BeginCullingQuery(
        UINT boundsSize,
        const D3DXVECTOR4* bounds,
        const D3DXMATRIX& view,
        const D3DXMATRIX& projection);

    void EndCullingQuery(UINT boundsSize, float* pResults);

    void Initialize(UINT width = 512, UINT height = 256, UINT initialBoundsBufferSize = 1000);

protected:
    void Reset();

    void LoadDepthEffect();
    void LoadMipmapEffect();
    void LoadCullEffect();

    void RenderOccluders(const D3DXMATRIX& view, const D3DXMATRIX& projection);
    void RenderMipmap();

    void EnsureCullingBuffers(UINT desiredSize);

    IDirect3DDevice9* m_pDevice;

    UINT m_width;
    UINT m_height;
    UINT m_boundsSize;

    UINT m_mipLevels;

    IDirect3DTexture9* m_pHizBufferEven;
    IDirect3DTexture9* m_pHizBufferOdd;
    IDirect3DSurface9** m_pMipLevelEvenSurfaces;
    IDirect3DSurface9** m_pMipLevelOddSurfaces;

    LPD3DXEFFECT m_pDepthEffect;
    IDirect3DVertexBuffer9* m_pOccluderVertexBuffer;
    IDirect3DVertexDeclaration9* m_pDepthVertexDecl;
    D3DXHANDLE m_hDepthEffectTech;
    D3DXHANDLE m_hDepthEffectParamWVP;

    LPD3DXEFFECT m_pHizMipmapEffect;
    IDirect3DVertexBuffer9* m_pHizMipmapVertexBuffer;
    IDirect3DVertexDeclaration9* m_pHizMipmapVertexDecl;
    D3DXHANDLE m_hMipmapTech;
    D3DXHANDLE m_hMipmapParamLastMipInfo;
    D3DXHANDLE m_hMipmapParamLastMip;

    LPD3DXEFFECT m_pHizCullEffect;
    IDirect3DVertexDeclaration9* m_pCullingBoundsVertexDecl;
    D3DXHANDLE m_hCullingTech;
    D3DXHANDLE m_hCullingParamView;
    D3DXHANDLE m_hCullingParamProjection;
    D3DXHANDLE m_hCullingParamViewProjection;
    D3DXHANDLE m_hCullingParamFrustumPlanes;
    D3DXHANDLE m_hCullingParamViewportSize;
    D3DXHANDLE m_hCullingParamResultsSize;
    D3DXHANDLE m_hCullingParamHiZMapEven;
    D3DXHANDLE m_hCullingParamHiZMapOdd;

    IDirect3DVertexBuffer9* m_pCullingBoundsBuffer;
    IDirect3DTexture9* m_pCullingResultsBuffer;
    IDirect3DSurface9* m_pCullingResultsSurface;
    IDirect3DTexture9* m_pCullingResultsBufferSys;
    IDirect3DSurface9* m_pCullingResultsSurfaceSys;
    UINT m_cullingResultsWidth;
    UINT m_cullingResultsHeight;
};

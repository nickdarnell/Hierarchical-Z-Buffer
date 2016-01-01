#include "DXUT.h"
#include "Common.h"

#include "HZBManagerD3D9.h"

struct VBoundPixel
{
	D3DXVECTOR4 Bound;
    D3DXVECTOR2 Pixel;
};

HZBManagerD3D9::HZBManagerD3D9(IDirect3DDevice9* pDevice)
    : m_pDevice(pDevice)
    , m_pDepthEffect(NULL)
    , m_pDepthVertexDecl(NULL)
    , m_pOccluderVertexBuffer(NULL)
    , m_pHizBufferEven(NULL)
    , m_pMipLevelEvenSurfaces(NULL)
    , m_pHizBufferOdd(NULL)
    , m_pMipLevelOddSurfaces(NULL)
    , m_pCullingBoundsBuffer(NULL)
    , m_pCullingResultsBuffer(NULL)
    , m_pCullingResultsSurface(NULL)
    , m_pCullingResultsBufferSys(NULL)
    , m_pCullingResultsSurfaceSys(NULL)
    , m_cullingResultsWidth(0)
    , m_cullingResultsHeight(0)
    , m_pHizMipmapEffect(NULL)
    , m_pHizMipmapVertexDecl(NULL)
    , m_pHizMipmapVertexBuffer(NULL)
    , m_pHizCullEffect(NULL)
    , m_pCullingBoundsVertexDecl(NULL)
    , m_boundsSize(0)
{
    LoadDepthEffect();
    LoadMipmapEffect();
    LoadCullEffect();
}

HZBManagerD3D9::~HZBManagerD3D9(void)
{
    Reset();

    SAFE_RELEASE(m_pDepthEffect);
    SAFE_RELEASE(m_pDepthVertexDecl);
    SAFE_RELEASE(m_pOccluderVertexBuffer);
    
    SAFE_RELEASE(m_pHizMipmapEffect);
    SAFE_RELEASE(m_pHizMipmapVertexDecl);
    SAFE_RELEASE(m_pHizMipmapVertexBuffer);

    SAFE_RELEASE(m_pHizCullEffect);
    SAFE_RELEASE(m_pCullingBoundsVertexDecl);

    SAFE_RELEASE(m_pCullingBoundsBuffer);
    SAFE_RELEASE(m_pCullingResultsSurface);
    SAFE_RELEASE(m_pCullingResultsBuffer);
    SAFE_RELEASE(m_pCullingResultsSurfaceSys);
    SAFE_RELEASE(m_pCullingResultsBufferSys);
}

void HZBManagerD3D9::AddOccluder()
{
}

void HZBManagerD3D9::Render(const D3DXMATRIX& view, const D3DXMATRIX& projection)
{
    RenderOccluders(view, projection);
    RenderMipmap();
}

void HZBManagerD3D9::RenderOccluders(const D3DXMATRIX& view, const D3DXMATRIX& projection)
{
    D3DVIEWPORT9 vp;
    vp.Width = m_width;
    vp.Height = m_height;
    vp.MinZ = 0;
    vp.MaxZ = 1;
    vp.X = 0;
    vp.Y = 0;
    HRESULT hr = m_pDevice->SetViewport(&vp);
    assert(SUCCEEDED(hr));

    // 
    hr = m_pDevice->SetRenderTarget(0, m_pMipLevelEvenSurfaces[0]);
    assert(SUCCEEDED(hr));

    hr = m_pDevice->SetDepthStencilSurface(NULL);
    assert(SUCCEEDED(hr));

    // Clear the render target and the zbuffer 
    hr = m_pDevice->Clear( 0, NULL, D3DCLEAR_TARGET /* | D3DCLEAR_ZBUFFER */, D3DCOLOR_ARGB( 255, 255, 255, 255 ), 1.0f, 0 );
    assert(SUCCEEDED(hr));

    hr = m_pDevice->SetStreamSource(0, m_pOccluderVertexBuffer, 0, sizeof(VPos));
    assert(SUCCEEDED(hr));

    hr = m_pDevice->SetVertexDeclaration(m_pDepthVertexDecl);
    assert(SUCCEEDED(hr));

    hr = m_pDepthEffect->SetTechnique(m_hDepthEffectTech);
    assert(SUCCEEDED(hr));

    D3DXMATRIX occluderWorld;
    D3DXMatrixIdentity(&occluderWorld);
    D3DXMATRIX WVP = occluderWorld * view * projection;

    hr = m_pDepthEffect->SetMatrix(m_hDepthEffectParamWVP, &WVP);
    assert(SUCCEEDED(hr));

    UINT numPasses = 0;
    hr = m_pDepthEffect->Begin(&numPasses, 0);
    assert(SUCCEEDED(hr));

    for (UINT i = 0; i < numPasses; ++i)
    {
        hr = m_pDepthEffect->BeginPass(i);
        assert(SUCCEEDED(hr));

        hr = m_pDevice->DrawPrimitive(D3DPT_TRIANGLESTRIP, 0, 2);
        assert(SUCCEEDED(hr));

        hr = m_pDepthEffect->EndPass();
        assert(SUCCEEDED(hr));
    }

    hr = m_pDepthEffect->End();
    assert(SUCCEEDED(hr));
}

void HZBManagerD3D9::RenderMipmap()
{
    D3DVIEWPORT9 vp;
    vp.Width = m_width;
    vp.Height = m_height;
    vp.MinZ = 0;
    vp.MaxZ = 1;
    vp.X = 0;
    vp.Y = 0;
    HRESULT hr = m_pDevice->SetViewport(&vp);
    assert(SUCCEEDED(hr));

    //
    //hr = m_pDevice->StretchRect(m_pMipLevelRTSurfaces[0], NULL, m_pMipLevelSurfaces[0], NULL, D3DTEXF_NONE);
    //assert(SUCCEEDED(hr));

    for (UINT miplevel = 1; miplevel < m_mipLevels; miplevel++)
    {
        IDirect3DSurface9* pTargetSurface = 
            ((miplevel % 2) == 0) ? m_pMipLevelEvenSurfaces[miplevel] : m_pMipLevelOddSurfaces[miplevel];

        hr = m_pDevice->SetRenderTarget(0, pTargetSurface);
        assert(SUCCEEDED(hr));

        UINT width = m_width >> miplevel;
        UINT height = m_height >> miplevel;
        if (width == 0) width = 1;
        if (height == 0) height = 1;

        D3DVIEWPORT9 vp;
        vp.Width = width;
        vp.Height = height;
        vp.MinZ = 0;
        vp.MaxZ = 1;
        vp.X = 0;
        vp.Y = 0;
        HRESULT hr = m_pDevice->SetViewport(&vp);
        assert(SUCCEEDED(hr));

        hr = m_pDevice->SetStreamSource(0, m_pHizMipmapVertexBuffer, 0, sizeof(VPos));
        assert(SUCCEEDED(hr));

        hr = m_pDevice->SetVertexDeclaration(m_pHizMipmapVertexDecl);
        assert(SUCCEEDED(hr));

        hr = m_pHizMipmapEffect->SetTechnique(m_hMipmapTech);
        assert(SUCCEEDED(hr));

        UINT lastWidth = m_width >> (miplevel - 1);
        UINT lastHeight = m_height >> (miplevel - 1);
        if (lastWidth == 0) lastWidth = 1;
        if (lastHeight == 0) lastHeight = 1;

        IDirect3DTexture9* pLastMipTexture = 
            ((miplevel - 1) % 2 == 0) ? m_pHizBufferEven : m_pHizBufferOdd;

        D3DXVECTOR4 lastMipInfo(lastWidth, lastHeight, miplevel - 1, 0);
        hr = m_pHizMipmapEffect->SetVector(m_hMipmapParamLastMipInfo, &lastMipInfo);
        assert(SUCCEEDED(hr));

        hr = m_pHizMipmapEffect->SetTexture(m_hMipmapParamLastMip, pLastMipTexture);
        assert(SUCCEEDED(hr));

        UINT numPasses = 0;
        hr = m_pHizMipmapEffect->Begin(&numPasses, 0);
        assert(SUCCEEDED(hr));

        for (UINT i = 0; i < numPasses; ++i)
        {
	        hr = m_pHizMipmapEffect->BeginPass(i);
            assert(SUCCEEDED(hr));

            hr = m_pDevice->DrawPrimitive(D3DPT_TRIANGLESTRIP, 0, 2);
            assert(SUCCEEDED(hr));

	        hr = m_pHizMipmapEffect->EndPass();
            assert(SUCCEEDED(hr));
        }

        hr = m_pHizMipmapEffect->End();
        assert(SUCCEEDED(hr));

        hr = m_pHizMipmapEffect->SetTexture(m_hMipmapParamLastMip, NULL);
        assert(SUCCEEDED(hr));

        //
        //hr = m_pDevice->StretchRect(m_pMipLevelRTSurfaces[miplevel], NULL, m_pMipLevelSurfaces[miplevel], NULL, D3DTEXF_NONE);
        //assert(SUCCEEDED(hr));
    }
}

void HZBManagerD3D9::BeginCullingQuery(
    UINT boundsSize,
    const D3DXVECTOR4* bounds,
    const D3DXMATRIX& view,
    const D3DXMATRIX& projection)
{
    EnsureCullingBuffers(boundsSize);

    //
    VBoundPixel* v = NULL;
    HRESULT hr = m_pCullingBoundsBuffer->Lock(0, 0, (void**)&v, D3DLOCK_DISCARD);
    assert(SUCCEEDED(hr));

    for(int i = 0; i < boundsSize; i++)
    {
        v[i].Bound = bounds[i];
        v[i].Pixel = D3DXVECTOR2(i % m_cullingResultsWidth, i / m_cullingResultsWidth);
    }

    hr = m_pCullingBoundsBuffer->Unlock();
    assert(SUCCEEDED(hr));

    hr = m_pDevice->SetRenderTarget(0, m_pCullingResultsSurface);
    assert(SUCCEEDED(hr));

    hr = m_pDevice->SetDepthStencilSurface(NULL);
    assert(SUCCEEDED(hr));

    //
    D3DVIEWPORT9 vp;
    vp.Width = m_cullingResultsWidth;
    vp.Height = m_cullingResultsHeight;
    vp.MinZ = 0;
    vp.MaxZ = 1;
    vp.X = 0;
    vp.Y = 0;
    hr = m_pDevice->SetViewport(&vp);
    assert(SUCCEEDED(hr));

    hr = m_pDevice->SetStreamSource(0, m_pCullingBoundsBuffer, 0, sizeof( VBoundPixel ));
    assert(SUCCEEDED(hr));

    hr = m_pDevice->SetVertexDeclaration(m_pCullingBoundsVertexDecl);
    assert(SUCCEEDED(hr));

    hr = m_pHizCullEffect->SetTechnique(m_hCullingTech);
    assert(SUCCEEDED(hr));

    hr = m_pHizCullEffect->SetMatrix(m_hCullingParamView, &view);
    assert(SUCCEEDED(hr));
    
    hr = m_pHizCullEffect->SetMatrix(m_hCullingParamProjection, &projection);
    assert(SUCCEEDED(hr));

    D3DXMATRIX VP = view * projection;
    hr = m_pHizCullEffect->SetMatrix(m_hCullingParamViewProjection, &VP);
    assert(SUCCEEDED(hr));

    Frustum frustum;
    ExtractFrustum(frustum, VP);
    hr = m_pHizCullEffect->SetVectorArray(m_hCullingParamFrustumPlanes, (D3DXVECTOR4*)&frustum, 6);
    assert(SUCCEEDED(hr));

    D3DXVECTOR4 viewport(m_width, m_height, 0, 0);
    hr = m_pHizCullEffect->SetVector(m_hCullingParamViewportSize, &viewport);
    assert(SUCCEEDED(hr));

    D3DXVECTOR4 resultsSize(m_cullingResultsWidth, m_cullingResultsHeight, 0, 0);
    hr = m_pHizCullEffect->SetVector(m_hCullingParamResultsSize, &resultsSize);
    assert(SUCCEEDED(hr));

    hr = m_pHizCullEffect->SetTexture(m_hCullingParamHiZMapEven, m_pHizBufferEven);
    assert(SUCCEEDED(hr));

    hr = m_pHizCullEffect->SetTexture(m_hCullingParamHiZMapOdd, m_pHizBufferOdd);
    assert(SUCCEEDED(hr));

    UINT numPasses = 0;
    hr = m_pHizCullEffect->Begin(&numPasses, 0);
    assert(SUCCEEDED(hr));

    for (UINT i = 0; i < numPasses; ++i)
    {
        hr = m_pHizCullEffect->BeginPass(i);
        assert(SUCCEEDED(hr));

        hr = m_pDevice->DrawPrimitive(D3DPT_POINTLIST, 0, boundsSize);
        assert(SUCCEEDED(hr));

        hr = m_pHizCullEffect->EndPass();
        assert(SUCCEEDED(hr));
    }

    hr = m_pHizCullEffect->End();
    assert(SUCCEEDED(hr));

    hr = m_pHizCullEffect->SetTexture(m_hCullingParamHiZMapEven, NULL);
    assert(SUCCEEDED(hr));

    hr = m_pHizCullEffect->SetTexture(m_hCullingParamHiZMapOdd, NULL);
    assert(SUCCEEDED(hr));
}

void HZBManagerD3D9::EndCullingQuery(UINT boundsSize, float* pResults)
{
    HRESULT hr = m_pDevice->GetRenderTargetData(m_pCullingResultsSurface, m_pCullingResultsSurfaceSys);
    assert(SUCCEEDED(hr));

    D3DLOCKED_RECT pLockedRect;
    hr = m_pCullingResultsSurfaceSys->LockRect(&pLockedRect, NULL, D3DLOCK_READONLY);
    assert(SUCCEEDED(hr));

    BYTE* pBuffer = (BYTE*)pLockedRect.pBits;

    bool done = false;
    for (UINT y = 0; y < m_cullingResultsHeight; y++)
    {
        BYTE* pBufferRow = (BYTE*)pBuffer;
        for (UINT x = 0; x < m_cullingResultsWidth; x++)
        {
            UINT index = (y * m_cullingResultsWidth) + x;
            if (index > boundsSize - 1)
                goto done;

            pResults[index] = (pBufferRow[x] == 255) ? 1 : 0;
        }

        pBuffer += pLockedRect.Pitch;
    }

done:

    hr = m_pCullingResultsSurfaceSys->UnlockRect();
    assert(SUCCEEDED(hr));
}

void HZBManagerD3D9::Reset()
{
    if (m_pMipLevelEvenSurfaces)
    {
        for (int i = 0; i < m_mipLevels; i++)
            SAFE_RELEASE(m_pMipLevelEvenSurfaces[i]);

        delete[] m_pMipLevelEvenSurfaces;
        m_pMipLevelEvenSurfaces = NULL;
    }

    if (m_pMipLevelOddSurfaces)
    {
        for (int i = 0; i < m_mipLevels; i++)
            SAFE_RELEASE(m_pMipLevelOddSurfaces[i]);

        delete[] m_pMipLevelOddSurfaces;
        m_pMipLevelOddSurfaces = NULL;
    }

    SAFE_RELEASE(m_pHizBufferEven);
    SAFE_RELEASE(m_pHizBufferOdd);
}

void HZBManagerD3D9::Initialize(UINT width, UINT height, UINT initialBoundsBufferSize)
{
    Reset();

    m_width = width;
    m_height = height;

    // HiZ Buffer
    //---------------------------------------------------------------------------------------
    HRESULT hr = m_pDevice->CreateTexture(m_width, m_height, 0, D3DUSAGE_RENDERTARGET, 
        D3DFMT_R32F, D3DPOOL_DEFAULT, &m_pHizBufferEven, NULL);
    assert(SUCCEEDED(hr));

    hr = m_pDevice->CreateTexture(m_width, m_height, 0, D3DUSAGE_RENDERTARGET, 
        D3DFMT_R32F, D3DPOOL_DEFAULT, &m_pHizBufferOdd, NULL);
    assert(SUCCEEDED(hr));

    // HiZ Depth Buffer
    //---------------------------------------------------------------------------------------


    // Generate the Render target and shader resource views that we'll need for rendering
    // the mip levels in the downsampling step.
    //---------------------------------------------------------------------------------------
    m_mipLevels = m_pHizBufferEven->GetLevelCount();
    m_pMipLevelEvenSurfaces = new IDirect3DSurface9*[m_mipLevels];
    m_pMipLevelOddSurfaces = new IDirect3DSurface9*[m_mipLevels];

    for (UINT miplevel = 0; miplevel < m_mipLevels; miplevel++)
    {
        IDirect3DSurface9* pMipSurfaceEven;
        hr = m_pHizBufferEven->GetSurfaceLevel(miplevel, &pMipSurfaceEven);
        assert(SUCCEEDED(hr));

        IDirect3DSurface9* pMipSurfaceOdd;
        hr = m_pHizBufferOdd->GetSurfaceLevel(miplevel, &pMipSurfaceOdd);
        assert(SUCCEEDED(hr));

        m_pMipLevelEvenSurfaces[miplevel] = pMipSurfaceEven;
        m_pMipLevelOddSurfaces[miplevel] = pMipSurfaceOdd;
    }

    // Initialize the culling read back and submission buffers to the initial size.
    EnsureCullingBuffers(initialBoundsBufferSize);
}

void HZBManagerD3D9::EnsureCullingBuffers(UINT desiredSize)
{
    if (desiredSize > m_boundsSize)
    {
        SAFE_RELEASE(m_pCullingBoundsBuffer);
        SAFE_RELEASE(m_pCullingResultsSurface);
        SAFE_RELEASE(m_pCullingResultsBuffer);
        SAFE_RELEASE(m_pCullingResultsSurfaceSys);
        SAFE_RELEASE(m_pCullingResultsBufferSys);

        m_cullingResultsWidth = 256;
        m_cullingResultsHeight = floor((desiredSize / 256.0f) + 1.0f);

        m_boundsSize = m_cullingResultsWidth * m_cullingResultsHeight;

        HRESULT hr = m_pDevice->CreateTexture(m_cullingResultsWidth, m_cullingResultsHeight, 
            1, D3DUSAGE_RENDERTARGET, D3DFMT_L8, D3DPOOL_DEFAULT, &m_pCullingResultsBuffer, NULL);
        assert(SUCCEEDED(hr));
        // Probably need to use D3DFMT_A8R8G8B8 if D3DFMT_L8 isn't supported, possibly try
        // D3DFMT_P8 also?

        hr = m_pCullingResultsBuffer->GetSurfaceLevel(0, &m_pCullingResultsSurface);
        assert(SUCCEEDED(hr));

        hr = m_pDevice->CreateTexture(m_cullingResultsWidth, m_cullingResultsHeight, 
            1, D3DUSAGE_DYNAMIC, D3DFMT_L8, D3DPOOL_SYSTEMMEM, &m_pCullingResultsBufferSys, NULL);
        assert(SUCCEEDED(hr));

        hr = m_pCullingResultsBufferSys->GetSurfaceLevel(0, &m_pCullingResultsSurfaceSys);
        assert(SUCCEEDED(hr));

        hr = m_pDevice->CreateVertexBuffer(m_boundsSize * sizeof( VBoundPixel ), 
            D3DUSAGE_WRITEONLY | D3DUSAGE_DYNAMIC,
	        0, D3DPOOL_DEFAULT, &m_pCullingBoundsBuffer, 0);
        assert(SUCCEEDED(hr));
    }
}

void HZBManagerD3D9::LoadDepthEffect()
{
    HRESULT hr = LoadEffectFromFile9(m_pDevice, L"Content\\Depth9.fx", &m_pDepthEffect);
    assert(SUCCEEDED(hr));

    m_hDepthEffectTech = m_pDepthEffect->GetTechniqueByName("Render");
    m_hDepthEffectParamWVP = m_pDepthEffect->GetParameterByName(0, "WorldViewProjection");

    // 
    D3DVERTEXELEMENT9 pElements[] = {
        {0, 0, D3DDECLTYPE_FLOAT4, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0},
        D3DDECL_END()
    };

    hr = m_pDevice->CreateVertexDeclaration((D3DVERTEXELEMENT9*)&pElements, &m_pDepthVertexDecl);
    assert(SUCCEEDED(hr));

    // 
    hr = m_pDevice->CreateVertexBuffer(6 * sizeof( VPos ), D3DUSAGE_WRITEONLY,
	    0, D3DPOOL_MANAGED, &m_pOccluderVertexBuffer, 0);
    assert(SUCCEEDED(hr));

    VPos vertices[] =
    {
        { D3DXVECTOR4(0.0f, 15.0f, 15.0f, 1.0f) },
        { D3DXVECTOR4(15.0f, -15.0f, 15.0f, 1.0f) },
        { D3DXVECTOR4(-15.0f, -15.0f, 15.0f, 1.0f) },

        { D3DXVECTOR4(0.0f, 10.0f, 10.0f, 1.0f) },
        { D3DXVECTOR4(10.0f, -10.0f, 10.0f, 1.0f) },
        { D3DXVECTOR4(-10.0f, -10.0f, 10.0f, 1.0f) },
    };

    VPos* v = NULL;
    hr = m_pOccluderVertexBuffer->Lock(0, 0, (void**)&v, 0);
    assert(SUCCEEDED(hr));

    memcpy((void*)v, &vertices, sizeof(vertices));

    hr = m_pOccluderVertexBuffer->Unlock();
    assert(SUCCEEDED(hr));
}

void HZBManagerD3D9::LoadMipmapEffect()
{
    HRESULT hr = LoadEffectFromFile9(m_pDevice, L"Content\\HizMipmap9.fx", &m_pHizMipmapEffect);
    assert(SUCCEEDED(hr));

    m_hMipmapTech = m_pHizMipmapEffect->GetTechniqueByName("Render");
    m_hMipmapParamLastMipInfo = m_pHizMipmapEffect->GetParameterByName(0, "LastMipInfo");
    m_hMipmapParamLastMip =     m_pHizMipmapEffect->GetParameterByName(0, "LastMip");

    // 
    D3DVERTEXELEMENT9 pElements[] = {
        {0, 0, D3DDECLTYPE_FLOAT4, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0},
        D3DDECL_END()
    };

    hr = m_pDevice->CreateVertexDeclaration((D3DVERTEXELEMENT9*)&pElements, &m_pHizMipmapVertexDecl);
    assert(SUCCEEDED(hr));

    // 
    hr = m_pDevice->CreateVertexBuffer(4 * sizeof( VPos ), D3DUSAGE_WRITEONLY,
	    0, D3DPOOL_MANAGED, &m_pHizMipmapVertexBuffer, 0);
    assert(SUCCEEDED(hr));

    VPos vertices[] =
    {
        { D3DXVECTOR4(-1.0f,  1.0f, 0.0f, 1.0f) },
        { D3DXVECTOR4( 1.0f,  1.0f, 0.0f, 1.0f) },
        { D3DXVECTOR4(-1.0f, -1.0f, 0.0f, 1.0f) },
        { D3DXVECTOR4( 1.0f, -1.0f, 0.0f, 1.0f) },
    };

    VPos* v = NULL;
    hr = m_pHizMipmapVertexBuffer->Lock(0, 0, (void**)&v, 0);
    assert(SUCCEEDED(hr));

    memcpy((void*)v, &vertices, sizeof(vertices));

    hr = m_pHizMipmapVertexBuffer->Unlock();
    assert(SUCCEEDED(hr));
}

void HZBManagerD3D9::LoadCullEffect()
{
    HRESULT hr = LoadEffectFromFile9(m_pDevice, L"Content\\HizCull9.fx", &m_pHizCullEffect);
    assert(SUCCEEDED(hr));

    m_hCullingTech = m_pHizCullEffect->GetTechniqueByName("Render");
    m_hCullingParamView = m_pHizCullEffect->GetParameterByName(0, "View");
    m_hCullingParamProjection = m_pHizCullEffect->GetParameterByName(0, "Projection");
    m_hCullingParamViewProjection = m_pHizCullEffect->GetParameterByName(0, "ViewProjection");
    m_hCullingParamFrustumPlanes = m_pHizCullEffect->GetParameterByName(0, "FrustumPlanes");
    m_hCullingParamViewportSize = m_pHizCullEffect->GetParameterByName(0, "ViewportSize");
    m_hCullingParamResultsSize = m_pHizCullEffect->GetParameterByName(0, "ResultsSize");
    m_hCullingParamHiZMapEven = m_pHizCullEffect->GetParameterByName(0, "HiZMapEven");
    m_hCullingParamHiZMapOdd = m_pHizCullEffect->GetParameterByName(0, "HiZMapOdd");

    // 
    D3DVERTEXELEMENT9 pElements[] = {
        {0, 0, D3DDECLTYPE_FLOAT4, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0},
        {0, 16, D3DDECLTYPE_FLOAT2, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 0},
        D3DDECL_END()
    };

    hr = m_pDevice->CreateVertexDeclaration((D3DVERTEXELEMENT9*)&pElements, &m_pCullingBoundsVertexDecl);
    assert(SUCCEEDED(hr));
}
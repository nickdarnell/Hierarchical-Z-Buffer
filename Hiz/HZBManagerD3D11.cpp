#include "DXUT.h"
#include "Common.h"
#include "D3DUtil.h"

#include "HZBManagerD3D11.h"

struct CB_HizCull
{
    D3DXMATRIX View;
    D3DXMATRIX Projection;
    D3DXMATRIX ViewProjection;

    Frustum Frustum;                // view-frustum planes in world space (normals face out)

    D3DXVECTOR2 ViewportSize;       // Viewport Width and Height in pixels

private:
    float PADDING[2];
};

HZBManagerD3D11::HZBManagerD3D11(ID3D11Device* pDevice)
    : m_pDevice(pDevice)
    , m_pMipLevelRTV(NULL)
    , m_pMipLevelSRV(NULL)
    , m_pMipmapCL(NULL)
    , m_pHizBuffer(NULL)
    , m_pHizDepthBuffer(NULL)
    , m_pHizDepthDSV(NULL)
    , m_pHizMipmapEffect(NULL)
    , m_pHizMipmapLayout(NULL)
    , m_pHizMipmapVertexBuffer(NULL)
    , m_pDepthEffect(NULL)
    , m_pDepthLayout(NULL)
    , m_pOccluderVertexBuffer(NULL)
    , m_pHizCullCS(NULL)
    , m_pCS_CB_HizCull(NULL)
    , m_pHizCullSampler(NULL)
    , m_pCullingBoundsBuffer(NULL)
    , m_pCullingResultsBuffer(NULL)
    , m_pCullingReadableResultsBuffer(NULL)
    , m_pCullingBoundsSRV(NULL)
    , m_pCullingResultsUAV(NULL)
    , m_pHizSRV(NULL)
    , m_boundsSize(0)
    , m_bRenderOccluders(true)
{
    LoadDepthEffect();
    LoadMipmapEffect();
    LoadCullEffect();
}

HZBManagerD3D11::~HZBManagerD3D11(void)
{
    Reset();

    SAFE_RELEASE(m_pHizMipmapEffect);
    SAFE_RELEASE(m_pHizMipmapLayout);
    SAFE_RELEASE(m_pHizMipmapVertexBuffer);

    SAFE_RELEASE(m_pDepthEffect);
    SAFE_RELEASE(m_pDepthLayout);
    SAFE_RELEASE(m_pOccluderVertexBuffer);

    SAFE_RELEASE(m_pHizCullCS);
}

void HZBManagerD3D11::SetRenderOccluders(bool bRenderOcculders)
{
    m_bRenderOccluders = bRenderOcculders;
}

void HZBManagerD3D11::AddOccluder()
{
}

void HZBManagerD3D11::Render(
    ID3D11DeviceContext* pImmediateContext,
    const D3DXMATRIX& view,
    const D3DXMATRIX& projection)
{
    RenderOccluders(pImmediateContext, view, projection);
    RenderMipmap(pImmediateContext);
}

void HZBManagerD3D11::RenderOccluders(
    ID3D11DeviceContext* pImmediateContext,
    const D3DXMATRIX& view,
    const D3DXMATRIX& projection)
{
    PROFILE_BLOCK;

    // Setup the viewport to match
    D3D11_VIEWPORT vp;
    vp.Width = (FLOAT)m_width;
    vp.Height = (FLOAT)m_height;
    vp.MinDepth = 0;
    vp.MaxDepth = 1;
    vp.TopLeftX = 0;
    vp.TopLeftY = 0;
    pImmediateContext->RSSetViewports( 1, &vp );

    if (m_bRenderOccluders)
    {
        pImmediateContext->OMSetRenderTargets(1, &m_pMipLevelRTV[0], m_pHizDepthDSV);

        pImmediateContext->ClearRenderTargetView( m_pMipLevelRTV[0], WhiteColor );
        pImmediateContext->ClearDepthStencilView( m_pHizDepthDSV, D3D11_CLEAR_DEPTH, 1.0, 0 );

        pImmediateContext->IASetInputLayout(m_pDepthLayout);
        pImmediateContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

        UINT stride = sizeof(VPos);
        UINT offset = 0;

        pImmediateContext->IASetVertexBuffers(0, 1, &m_pOccluderVertexBuffer, &stride, &offset);

        ID3DX11EffectMatrixVariable* pWVPVar = 
            m_pDepthEffect->GetVariableByName("WorldViewProjection")->AsMatrix();

        D3DXMATRIX occluderWorld;
        D3DXMatrixIdentity(&occluderWorld);

        D3DXMATRIX WVP = occluderWorld * view * projection;

        pWVPVar->SetMatrix((float*)&WVP);

        ID3DX11EffectTechnique* pTechnique = m_pDepthEffect->GetTechniqueByIndex(0);
        ID3DX11EffectPass* pPass = pTechnique->GetPassByIndex(0);
        
        HRESULT hr = pPass->Apply(0, pImmediateContext);
        assert(SUCCEEDED(hr));
        
        pImmediateContext->Draw(6, 0);
    }
}

void HZBManagerD3D11::RenderMipmap(ID3D11DeviceContext* pImmediateContext)
{
    PROFILE_BLOCK;

    pImmediateContext->ExecuteCommandList(m_pMipmapCL, FALSE);
}

void HZBManagerD3D11::BeginCullingQuery(
    ID3D11DeviceContext* pImmediateContext,
    UINT boundsSize,
    const D3DXVECTOR4* bounds,
    const D3DXMATRIX& view,
    const D3DXMATRIX& projection)
{
    PROFILE_BLOCK;

    EnsureCullingBuffers(boundsSize);

    ID3D11ShaderResourceView* srviews[] = 
    {
        m_pCullingBoundsSRV,
        m_pHizSRV
    };

    ID3D11ShaderResourceView* srviewsnull[] = 
    {
        NULL,
        NULL
    };

    // Setup Bounds Input Structured Buffers
    //-----------------------------------------------------------------
    D3D11_MAPPED_SUBRESOURCE pCullingBoundsBufferMapped;
    HRESULT hr = pImmediateContext->Map(m_pCullingBoundsBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &pCullingBoundsBufferMapped);
    assert(SUCCEEDED(hr));

    memcpy((D3DXVECTOR4*)pCullingBoundsBufferMapped.pData, bounds, sizeof(D3DXVECTOR4) * boundsSize);

    pImmediateContext->Unmap( m_pCullingBoundsBuffer, 0 );

    // Setup Culling Constant Buffer
    //-----------------------------------------------------------------
    D3D11_MAPPED_SUBRESOURCE pCS_CB_HizCullMapped;
    hr = pImmediateContext->Map(m_pCS_CB_HizCull, 0, D3D11_MAP_WRITE_DISCARD, 0, &pCS_CB_HizCullMapped);
    assert(SUCCEEDED(hr));

    CB_HizCull* pCB_HizCull = ( CB_HizCull* )pCS_CB_HizCullMapped.pData;  
    
    pCB_HizCull->View = view;
    pCB_HizCull->Projection = projection;
    pCB_HizCull->ViewProjection = view * projection;

    ExtractFrustum(pCB_HizCull->Frustum, pCB_HizCull->ViewProjection);

    pCB_HizCull->ViewportSize = D3DXVECTOR2((FLOAT)m_width, (FLOAT)m_height);

    pImmediateContext->Unmap( m_pCS_CB_HizCull, 0 );
    pImmediateContext->CSSetConstantBuffers( 0, 1, &m_pCS_CB_HizCull );

    // Run Culling Compute Shader
    //-----------------------------------------------------------------

    pImmediateContext->CSSetShader( m_pHizCullCS, NULL, 0 );
    pImmediateContext->CSSetShaderResources( 0, 2, (ID3D11ShaderResourceView**)&srviews );
    pImmediateContext->CSSetSamplers( 0, 1, &m_pHizCullSampler);
    pImmediateContext->CSSetUnorderedAccessViews( 0, 1, &m_pCullingResultsUAV, NULL );

    pImmediateContext->Dispatch(boundsSize, 1, 1 );

    pImmediateContext->CSSetShader( NULL, NULL, 0 );
    //pImmediateContext->CSSetUnorderedAccessViews( 0, 0, NULL, NULL );
    pImmediateContext->CSSetShaderResources( 0, 2, (ID3D11ShaderResourceView**)&srviewsnull );
    //pImmediateContext->CSSetConstantBuffers( 0, 0, NULL );

    //-----------------------------------------------------------------
}

void HZBManagerD3D11::EndCullingQuery(ID3D11DeviceContext* pImmediateContext, UINT boundsSize, float* pResults)
{
    PROFILE_BLOCK;

    // Download the data
    pImmediateContext->CopyResource( m_pCullingReadableResultsBuffer, m_pCullingResultsBuffer );

    D3D11_MAPPED_SUBRESOURCE MappedResults = {0};
    HRESULT hr = pImmediateContext->Map( m_pCullingReadableResultsBuffer, 0, D3D11_MAP_READ, 0, &MappedResults );
    assert(SUCCEEDED(hr));

    memcpy(pResults, (float*)MappedResults.pData, sizeof(float) * boundsSize);

    pImmediateContext->Unmap( m_pCullingReadableResultsBuffer, 0 );
}

void HZBManagerD3D11::Reset()
{
    if (m_pMipLevelRTV)
    {
        for (int i = 0; i < m_mipLevels; i++)
            SAFE_RELEASE(m_pMipLevelRTV[i]);

        delete[] m_pMipLevelRTV;
        m_pMipLevelRTV = NULL;
    }

    if (m_pMipLevelSRV)
    {
        for (int i = 0; i < m_mipLevels; i++)
            SAFE_RELEASE(m_pMipLevelSRV[i]);

        delete[] m_pMipLevelSRV;
        m_pMipLevelSRV = NULL;
    }

    SAFE_RELEASE(m_pHizBuffer);
    SAFE_RELEASE(m_pHizDepthBuffer);
    SAFE_RELEASE(m_pHizDepthDSV);

    SAFE_RELEASE(m_pMipmapCL);

    SAFE_RELEASE(m_pCS_CB_HizCull);
    SAFE_RELEASE(m_pHizCullSampler);
    SAFE_RELEASE(m_pHizSRV);

    SAFE_RELEASE(m_pCullingBoundsBuffer);
    SAFE_RELEASE(m_pCullingResultsBuffer);
    SAFE_RELEASE(m_pCullingReadableResultsBuffer);
    SAFE_RELEASE(m_pCullingBoundsSRV);
    SAFE_RELEASE(m_pCullingResultsUAV);
}

void HZBManagerD3D11::Initialize(UINT width, UINT height, UINT initialBoundsBufferSize)
{
    Reset();

    m_width = width;
    m_height = height;

    // HiZ Buffer
    //---------------------------------------------------------------------------------------
    D3D11_TEXTURE2D_DESC hizDesc;
    hizDesc.Width = width;
    hizDesc.Height = height;
    hizDesc.MipLevels = 0;
    hizDesc.ArraySize = 1;
    hizDesc.Format = DXGI_FORMAT_R32_FLOAT;
    hizDesc.SampleDesc.Count = 1;
    hizDesc.SampleDesc.Quality = 0;
    hizDesc.Usage = D3D11_USAGE_DEFAULT;
    hizDesc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
    hizDesc.CPUAccessFlags = 0;
    hizDesc.MiscFlags = 0;

    HRESULT hr = m_pDevice->CreateTexture2D(&hizDesc, NULL, &m_pHizBuffer);
    assert(SUCCEEDED(hr));

    // Get the new description now that the mip levels have been generated and the field
    // has been populated with the actual number of mip levels.
    m_pHizBuffer->GetDesc(&hizDesc);

    // HiZ Depth Buffer
    //---------------------------------------------------------------------------------------
    D3D11_TEXTURE2D_DESC hizDepthDesc;
    hizDepthDesc.Width = width;
    hizDepthDesc.Height = height;
    hizDepthDesc.MipLevels = 1;
    hizDepthDesc.ArraySize = 1;
    hizDepthDesc.Format = DXGI_FORMAT_D32_FLOAT;
    hizDepthDesc.SampleDesc.Count = 1;
    hizDepthDesc.SampleDesc.Quality = 0;
    hizDepthDesc.Usage = D3D11_USAGE_DEFAULT;
    hizDepthDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL;
    hizDepthDesc.CPUAccessFlags = 0;
    hizDepthDesc.MiscFlags = 0;

    hr = m_pDevice->CreateTexture2D(&hizDepthDesc, NULL, &m_pHizDepthBuffer);
    assert(SUCCEEDED(hr));
    //---------------------------------------------------------------------------------------

    // HiZ Depth Buffer SRV
    //---------------------------------------------------------------------------------------
    D3D11_DEPTH_STENCIL_VIEW_DESC hizDSVDesc;
    hizDSVDesc.Format = hizDepthDesc.Format;
    hizDSVDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
    hizDSVDesc.Texture2D.MipSlice = 0;
    hizDSVDesc.Flags = 0;

    hr = m_pDevice->CreateDepthStencilView(m_pHizDepthBuffer, &hizDSVDesc, &m_pHizDepthDSV);
    assert(SUCCEEDED(hr));
    //---------------------------------------------------------------------------------------

    // Generate the Render target and shader resource views that we'll need for rendering
    // the mip levels in the downsampling step.
    //---------------------------------------------------------------------------------------
    m_mipLevels = hizDesc.MipLevels;
    m_pMipLevelRTV = new ID3D11RenderTargetView*[m_mipLevels];
    m_pMipLevelSRV = new ID3D11ShaderResourceView*[m_mipLevels];

    for (UINT miplevel = 0; miplevel < m_mipLevels; miplevel++)
    {
        D3D11_RENDER_TARGET_VIEW_DESC downSampleHizRTVD;
        downSampleHizRTVD.Format = hizDesc.Format;
        downSampleHizRTVD.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
        downSampleHizRTVD.Texture2D.MipSlice = miplevel;

        ID3D11RenderTargetView* pHizRenderView;
        hr = m_pDevice->CreateRenderTargetView(m_pHizBuffer, &downSampleHizRTVD, &pHizRenderView);
        assert(SUCCEEDED(hr));

        // I cheat, I report 1 mip level, and say that the most detailed one is the mip I want to read from.
        D3D11_SHADER_RESOURCE_VIEW_DESC lastMipSRVD;
        lastMipSRVD.Format = hizDesc.Format;
        lastMipSRVD.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
        lastMipSRVD.Texture2D.MipLevels = 1;
        lastMipSRVD.Texture2D.MostDetailedMip = miplevel;

        ID3D11ShaderResourceView* pLastMipSRV;
        hr = m_pDevice->CreateShaderResourceView(m_pHizBuffer, &lastMipSRVD, &pLastMipSRV);
        assert(SUCCEEDED(hr));

        m_pMipLevelRTV[miplevel] = pHizRenderView;
        m_pMipLevelSRV[miplevel] = pLastMipSRV;
    }

    // Lets go ahead and build a commandlist of the downsampling code, because it's not like
    // we're going to be changing this process at all.
    //---------------------------------------------------------------------------------------
    InitializeDownsamplingCL(&hizDesc);

    // Create a shader resource view that can see the entire mip chain.
    D3D11_SHADER_RESOURCE_VIEW_DESC hizSRVD;
    hizSRVD.Format = hizDesc.Format;
    hizSRVD.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
    hizSRVD.Texture2D.MipLevels = hizDesc.MipLevels;
    hizSRVD.Texture2D.MostDetailedMip = 0;

    hr = m_pDevice->CreateShaderResourceView(m_pHizBuffer, &hizSRVD, &m_pHizSRV);
    assert(SUCCEEDED(hr));

    // Create constant buffers for the culling CS
    D3D11_BUFFER_DESC Desc;
    Desc.Usage = D3D11_USAGE_DYNAMIC;
    Desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    Desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    Desc.MiscFlags = 0;    
    Desc.ByteWidth = sizeof( CB_HizCull );
    hr = m_pDevice->CreateBuffer( &Desc, NULL, &m_pCS_CB_HizCull );
    assert(SUCCEEDED(hr));

    // Create the sampler to use in the culling CS on the HZB texture.
    D3D11_SAMPLER_DESC hizSD;
    hizSD.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT;
    hizSD.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
    hizSD.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
    hizSD.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
    hizSD.MipLODBias = 0.0f;
    hizSD.MaxAnisotropy = 1;
    hizSD.ComparisonFunc = D3D11_COMPARISON_NEVER;
    hizSD.BorderColor[0] = hizSD.BorderColor[1] = hizSD.BorderColor[2] = hizSD.BorderColor[3] = 0;//-FLT_MAX;
    hizSD.MinLOD = 0;
    hizSD.MaxLOD = D3D11_FLOAT32_MAX;
    hr = m_pDevice->CreateSamplerState( &hizSD, &m_pHizCullSampler );
    assert(SUCCEEDED(hr));

    // Initialize the culling read back and submission buffers to the initial size.
    EnsureCullingBuffers(initialBoundsBufferSize);
}

void HZBManagerD3D11::InitializeDownsamplingCL(const D3D11_TEXTURE2D_DESC* hizDesc)
{
    ID3D11DeviceContext* pMipmapDC;
    HRESULT hr = m_pDevice->CreateDeferredContext(0, &pMipmapDC);
    assert(SUCCEEDED(hr));

    // Setup the viewport to match
    D3D11_VIEWPORT vp;
    vp.Width = (FLOAT)m_width;
    vp.Height = (FLOAT)m_height;
    vp.MinDepth = 0;
    vp.MaxDepth = 1;
    vp.TopLeftX = 0;
    vp.TopLeftY = 0;
    pMipmapDC->RSSetViewports( 1, &vp );

    pMipmapDC->IASetInputLayout(m_pHizMipmapLayout);
    pMipmapDC->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

    UINT stride = sizeof(VPos);
    UINT offset = 0;

    pMipmapDC->IASetVertexBuffers(0, 1, &m_pHizMipmapVertexBuffer, &stride, &offset);

    ID3DX11EffectTechnique* pTechnique = m_pHizMipmapEffect->GetTechniqueByIndex(0);
    ID3DX11EffectPass* pPass = pTechnique->GetPassByIndex(0);

    ID3DX11EffectShaderResourceVariable* pLastMipVar = 
        m_pHizMipmapEffect->GetVariableByName("LastMip")->AsShaderResource();

    for (UINT miplevel = 1; miplevel < hizDesc->MipLevels; miplevel++)
    {
        pMipmapDC->OMSetRenderTargets(1, &m_pMipLevelRTV[miplevel], NULL);

        pLastMipVar->SetResource(m_pMipLevelSRV[miplevel - 1]);

        hr = pPass->Apply(0, pMipmapDC);
        assert(SUCCEEDED(hr));

        pMipmapDC->Draw(4, 0);
    }

    hr = pMipmapDC->FinishCommandList(FALSE, &m_pMipmapCL);
    assert(SUCCEEDED(hr));

    pMipmapDC->Release();
}

void HZBManagerD3D11::EnsureCullingBuffers(UINT desiredSize)
{
    if (desiredSize > m_boundsSize)
    {
        SAFE_RELEASE(m_pCullingBoundsBuffer);
        SAFE_RELEASE(m_pCullingResultsBuffer);
        SAFE_RELEASE(m_pCullingReadableResultsBuffer);
        SAFE_RELEASE(m_pCullingBoundsSRV);
        SAFE_RELEASE(m_pCullingResultsUAV);

        m_boundsSize = desiredSize;

        // Create Compute Shader Culling Buffers and Views
        //---------------------------------------------------------------------------------------
        HRESULT hr = CreateStructuredBuffer11(m_pDevice, sizeof(D3DXVECTOR4), m_boundsSize, NULL, D3D11_USAGE_DYNAMIC, &m_pCullingBoundsBuffer);
        assert(SUCCEEDED(hr));

        hr = CreateStructuredBuffer11(m_pDevice, sizeof(float), m_boundsSize, NULL, D3D11_USAGE_DEFAULT, &m_pCullingResultsBuffer);
        assert(SUCCEEDED(hr));

        hr = CreateBufferSRV11(m_pDevice, m_pCullingBoundsBuffer, &m_pCullingBoundsSRV);
        assert(SUCCEEDED(hr));

        hr = CreateBufferUAV11(m_pDevice, m_pCullingResultsBuffer, &m_pCullingResultsUAV);
        assert(SUCCEEDED(hr));

        // Create the Scratch Buffer
        // This is used to read the results back to the CPU
        D3D11_BUFFER_DESC readback_buffer_desc;
        ZeroMemory( &readback_buffer_desc, sizeof(readback_buffer_desc) );
        readback_buffer_desc.ByteWidth = m_boundsSize * sizeof(float);
        readback_buffer_desc.Usage = D3D11_USAGE_STAGING;
        readback_buffer_desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
        readback_buffer_desc.StructureByteStride = sizeof(float);
        hr = m_pDevice->CreateBuffer( &readback_buffer_desc, NULL, &m_pCullingReadableResultsBuffer );
        assert(SUCCEEDED(hr));
    }
}

void HZBManagerD3D11::LoadDepthEffect()
{
    HRESULT hr = LoadEffectFromFile11(m_pDevice, L"Content\\Depth.fx", &m_pDepthEffect);
    assert(SUCCEEDED(hr));

    ID3DX11EffectTechnique* pTechnique = m_pDepthEffect->GetTechniqueByIndex(0);
    ID3DX11EffectPass* pPass = pTechnique->GetPassByIndex(0);

    D3D11_INPUT_ELEMENT_DESC pElements[] = {
	    { "POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT,    0,  0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
    };

    D3DX11_PASS_SHADER_DESC vs_desc;
    hr = pPass->GetVertexShaderDesc(&vs_desc);
    assert(SUCCEEDED(hr));

    D3DX11_EFFECT_SHADER_DESC s_desc;
    hr = vs_desc.pShaderVariable->GetShaderDesc(0, &s_desc);
    assert(SUCCEEDED(hr));

    hr = m_pDevice->CreateInputLayout((D3D11_INPUT_ELEMENT_DESC*)&pElements, 1, 
        s_desc.pBytecode,
        s_desc.BytecodeLength,
        &m_pDepthLayout);
    assert(SUCCEEDED(hr));

    int Stride = 16;
    int VerticiesCount = 6;

    VPos vertices[] =
    {
        { D3DXVECTOR4(0.0f, 15.0f, 15.0f, 1.0f) },
        { D3DXVECTOR4(15.0f, -15.0f, 15.0f, 1.0f) },
        { D3DXVECTOR4(-15.0f, -15.0f, 15.0f, 1.0f) },

        { D3DXVECTOR4(0.0f, 10.0f, 10.0f, 1.0f) },
        { D3DXVECTOR4(10.0f, -10.0f, 10.0f, 1.0f) },
        { D3DXVECTOR4(-10.0f, -10.0f, 10.0f, 1.0f) },
    };

    D3D11_BUFFER_DESC bufferDesc;
    bufferDesc.Usage          = D3D11_USAGE_DEFAULT;
    bufferDesc.ByteWidth      = sizeof( VPos ) * VerticiesCount;
    bufferDesc.BindFlags      = D3D11_BIND_VERTEX_BUFFER;
    bufferDesc.CPUAccessFlags = 0;
    bufferDesc.MiscFlags      = 0;	

    D3D11_SUBRESOURCE_DATA InitData;
    InitData.pSysMem = &vertices;
    InitData.SysMemPitch = sizeof(vertices);
    InitData.SysMemSlicePitch = sizeof(vertices);

    hr = m_pDevice->CreateBuffer(&bufferDesc, &InitData, &m_pOccluderVertexBuffer);
    assert(SUCCEEDED(hr));
}

void HZBManagerD3D11::LoadMipmapEffect()
{
    HRESULT hr = LoadEffectFromFile11(m_pDevice, L"Content\\HizMipmap.fx", &m_pHizMipmapEffect);
    assert(SUCCEEDED(hr));



    ID3DX11EffectTechnique* pTechnique = m_pHizMipmapEffect->GetTechniqueByIndex(0);
    ID3DX11EffectPass* pPass = pTechnique->GetPassByIndex(0);

    D3D11_INPUT_ELEMENT_DESC pElements[] = {
	    { "POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT,    0,  0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
    };

    D3DX11_PASS_SHADER_DESC vs_desc;
    hr = pPass->GetVertexShaderDesc(&vs_desc);
    assert(SUCCEEDED(hr));

    D3DX11_EFFECT_SHADER_DESC s_desc;
    hr = vs_desc.pShaderVariable->GetShaderDesc(0, &s_desc);
    assert(SUCCEEDED(hr));

    hr = m_pDevice->CreateInputLayout((D3D11_INPUT_ELEMENT_DESC*)&pElements, 1, 
        s_desc.pBytecode,
        s_desc.BytecodeLength,
        &m_pHizMipmapLayout);
    assert(SUCCEEDED(hr));

    int Stride = 16;
    int VerticiesCount = 4;

    VPos vertices[] =
    {
        { D3DXVECTOR4(-1.0f,  1.0f, 0.0f, 1.0f) },
        { D3DXVECTOR4( 1.0f,  1.0f, 0.0f, 1.0f) },
        { D3DXVECTOR4(-1.0f, -1.0f, 0.0f, 1.0f) },
        { D3DXVECTOR4( 1.0f, -1.0f, 0.0f, 1.0f) },
    };


    D3D11_BUFFER_DESC bufferDesc;
    bufferDesc.Usage          = D3D11_USAGE_DEFAULT;
    bufferDesc.ByteWidth      = sizeof( VPos ) * VerticiesCount;
    bufferDesc.BindFlags      = D3D11_BIND_VERTEX_BUFFER;
    bufferDesc.CPUAccessFlags = 0;
    bufferDesc.MiscFlags      = 0;	

    D3D11_SUBRESOURCE_DATA InitData;
    InitData.pSysMem = &vertices;
    InitData.SysMemPitch = sizeof(vertices);
    InitData.SysMemSlicePitch = sizeof(vertices);

    hr = m_pDevice->CreateBuffer(&bufferDesc, &InitData, &m_pHizMipmapVertexBuffer);
    assert(SUCCEEDED(hr));
}

void HZBManagerD3D11::LoadCullEffect()
{
    HRESULT hr = CreateComputeShader11( L"Content\\HizCull.fx", "CSMain", m_pDevice, &m_pHizCullCS );
    assert(SUCCEEDED(hr));
}




////------------------------------------------------------------------------------------
//// Draw visible occluder
////------------------------------------------------------------------------------------
//if (g_bShowOccluders)
//{
//    pImmediateContext->IASetInputLayout(g_pDepthLayout);
//    pImmediateContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

//    UINT stride = sizeof(VPos);
//    UINT offset = 0;

//    pImmediateContext->IASetVertexBuffers(0, 1, &g_pOccluderVertexBuffer, &stride, &offset);

//    ID3DX11EffectMatrixVariable* pWorldVar = g_pPosColorEffect->GetVariableByName("World")->AsMatrix();
//    ID3DX11EffectMatrixVariable* pViewVar = g_pPosColorEffect->GetVariableByName("View")->AsMatrix();
//    ID3DX11EffectMatrixVariable* pProjectionVar = g_pPosColorEffect->GetVariableByName("Projection")->AsMatrix();

//    ID3DX11EffectVectorVariable* pColorVar = g_pPosColorEffect->GetVariableByName("Color")->AsVector();

//    pWorldVar->SetMatrix((float*)&g_worldMatrix);
//    pViewVar->SetMatrix((float*)&g_viewMatrix);
//    pProjectionVar->SetMatrix((float*)&g_projectionMatrix);

//    pColorVar->SetFloatVector((float*)&BlueColor);

//    ID3DX11EffectTechnique* pTechnique = g_pPosColorEffect->GetTechniqueByIndex(0);
//    ID3DX11EffectPass* pPass = pTechnique->GetPassByIndex(0);

//    Vx(SUCCEEDED(pPass->Apply(0, pImmediateContext)));
//    
//    if (g_bDrawOccluders)
//    {
//        pImmediateContext->Draw(6, 0);
//    }
//}
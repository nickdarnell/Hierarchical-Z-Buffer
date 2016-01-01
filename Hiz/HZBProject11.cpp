#include "DXUT.h"

#include "Common.h"
#include "D3DUtil.h"

#include "HZBManagerD3D11.h"

//----------------------------------------------------------------------------------
// External Global Data
//----------------------------------------------------------------------------------

extern bool g_bRenderResults;
extern bool g_bObserve;
extern bool g_bShowVisible;
extern bool g_bShowOccluders;

extern int g_selectedBoxIndex;

extern D3DXVECTOR3 g_eye;
extern D3DXVECTOR3 g_lookAt;
extern D3DXVECTOR3 g_up;

extern int BoxColumns;
extern int BoxRows;

extern D3DXMATRIX g_worldMatrix;
extern D3DXMATRIX g_viewMatrix;
extern D3DXMATRIX g_projectionMatrix;

//----------------------------------------------------------------------------------
// Global Data
//----------------------------------------------------------------------------------

HZBManagerD3D11* g_pHZBManager = NULL;

ID3DX11Effect* g_pPosColorEffect = NULL;
ID3D11Buffer* g_pPosColorVertexBuffer = NULL;
ID3D11Buffer* g_pPosColorIndexBuffer = NULL;
ID3D11InputLayout* g_pPosColorLayout = NULL;

//--------------------------------------------------------------------------------------
// Reject any D3D11 devices that aren't acceptable by returning false
//--------------------------------------------------------------------------------------
bool CALLBACK IsD3D11DeviceAcceptable( const CD3D11EnumAdapterInfo *AdapterInfo, UINT Output, const CD3D11EnumDeviceInfo *DeviceInfo,
                                       DXGI_FORMAT BackBufferFormat, bool bWindowed, void* pUserContext )
{
    return true;
}

//--------------------------------------------------------------------------------------
// Create any D3D11 resources that aren't dependant on the back buffer
//--------------------------------------------------------------------------------------
HRESULT CALLBACK OnD3D11CreateDevice( ID3D11Device* pd3dDevice, const DXGI_SURFACE_DESC* pBackBufferSurfaceDesc,
                                      void* pUserContext )
{
    g_pHZBManager = new HZBManagerD3D11(pd3dDevice);
    g_pHZBManager->Initialize();




    HRESULT hr = LoadEffectFromFile11(pd3dDevice, L"Content\\PosColor.fx", &g_pPosColorEffect);
    assert(SUCCEEDED(hr));

    {
        ID3DX11EffectTechnique* pTechnique = g_pPosColorEffect->GetTechniqueByIndex(0);
        ID3DX11EffectPass* pPass = pTechnique->GetPassByIndex(0);

        D3D11_INPUT_ELEMENT_DESC pElements[] = {
		    { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
            { "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
	    };
        UINT NumElements = 2;

        D3DX11_PASS_SHADER_DESC vs_desc;
        hr = pPass->GetVertexShaderDesc(&vs_desc);
        assert(SUCCEEDED(hr));

        D3DX11_EFFECT_SHADER_DESC s_desc;
        hr = vs_desc.pShaderVariable->GetShaderDesc(0, &s_desc);
        assert(SUCCEEDED(hr));

        hr = pd3dDevice->CreateInputLayout((D3D11_INPUT_ELEMENT_DESC*)&pElements, NumElements, 
            s_desc.pBytecode,
            s_desc.BytecodeLength,
            &g_pPosColorLayout);
        assert(SUCCEEDED(hr));

        float scale = 0.3535533905932738f;

        // Create vertex buffer
        VPosColor vertices[] =
        {
            { D3DXVECTOR3( -1.0f, 1.0f, -1.0f ) * scale, D3DXVECTOR4( 0.0f, 0.0f, 1.0f, 1.0f ) },
            { D3DXVECTOR3( 1.0f, 1.0f, -1.0f ) * scale, D3DXVECTOR4( 0.0f, 1.0f, 0.0f, 1.0f ) },
            { D3DXVECTOR3( 1.0f, 1.0f, 1.0f ) * scale, D3DXVECTOR4( 0.0f, 1.0f, 1.0f, 1.0f ) },
            { D3DXVECTOR3( -1.0f, 1.0f, 1.0f ) * scale, D3DXVECTOR4( 1.0f, 0.0f, 0.0f, 1.0f ) },
            { D3DXVECTOR3( -1.0f, -1.0f, -1.0f ) * scale, D3DXVECTOR4( 1.0f, 0.0f, 1.0f, 1.0f ) },
            { D3DXVECTOR3( 1.0f, -1.0f, -1.0f ) * scale, D3DXVECTOR4( 1.0f, 1.0f, 0.0f, 1.0f ) },
            { D3DXVECTOR3( 1.0f, -1.0f, 1.0f ) * scale, D3DXVECTOR4( 1.0f, 1.0f, 1.0f, 1.0f ) },
            { D3DXVECTOR3( -1.0f, -1.0f, 1.0f ) * scale, D3DXVECTOR4( 0.0f, 0.0f, 0.0f, 1.0f ) },
        };
        D3D11_BUFFER_DESC bd;
        bd.Usage = D3D11_USAGE_DEFAULT;
        bd.ByteWidth = sizeof( VPosColor ) * 8;
        bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
        bd.CPUAccessFlags = 0;
        bd.MiscFlags = 0;

        D3D11_SUBRESOURCE_DATA InitData;
        InitData.pSysMem = vertices;

        hr = pd3dDevice->CreateBuffer( &bd, &InitData, &g_pPosColorVertexBuffer );
        assert(SUCCEEDED(hr));

        // Create index buffer
        DWORD indices[] =
        {
            3,1,0,
            2,1,3,

            0,5,4,
            1,5,0,

            3,4,7,
            0,4,3,

            1,6,5,
            2,6,1,

            2,7,6,
            3,7,2,

            6,4,5,
            7,4,6,
        };
        bd.Usage = D3D11_USAGE_DEFAULT;
        bd.ByteWidth = sizeof( DWORD ) * 36;        // 36 vertices needed for 12 triangles in a triangle list
        bd.BindFlags = D3D11_BIND_INDEX_BUFFER;
        bd.CPUAccessFlags = 0;
        bd.MiscFlags = 0;
        InitData.pSysMem = indices;

        hr = pd3dDevice->CreateBuffer( &bd, &InitData, &g_pPosColorIndexBuffer );
        assert(SUCCEEDED(hr));
    }

    return S_OK;
}



//--------------------------------------------------------------------------------------
// Create any D3D11 resources that depend on the back buffer
//--------------------------------------------------------------------------------------
HRESULT CALLBACK OnD3D11ResizedSwapChain( ID3D11Device* pd3dDevice, IDXGISwapChain* pSwapChain,
                                          const DXGI_SURFACE_DESC* pBackBufferSurfaceDesc, void* pUserContext )
{
    return S_OK;
}



//--------------------------------------------------------------------------------------
// Render the scene using the D3D11 device
//--------------------------------------------------------------------------------------
void CALLBACK OnD3D11FrameRender( ID3D11Device* pd3dDevice, ID3D11DeviceContext* pImmediateContext,
                                  double fTime, float fElapsedTime, void* pUserContext )
{
    // Clear render target and the depth stencil 
    ID3D11RenderTargetView* pRTV = DXUTGetD3D11RenderTargetView();
    ID3D11DepthStencilView* pDSV = DXUTGetD3D11DepthStencilView();

    //------------------------------------------------------------------------------------
    // World / View / Projection
    //------------------------------------------------------------------------------------

    D3DXMatrixIdentity(&g_worldMatrix);

    // Set up the view matrix
    //--------------------------------------------------------------

    D3DXMatrixLookAtLH( &g_viewMatrix, &g_eye, &g_lookAt, &g_up );

    int width = 1024;
    int height = 768;

    float nearPlane = 0.1f;
    float farPlane = 100.0f;

    //Set up projection matrix
    //--------------------------------------------------------------
    D3DXMatrixPerspectiveFovLH(&g_projectionMatrix, (float)D3DX_PI * 0.5f, width / (float)height, nearPlane, farPlane);

    // Renders the occluders and generates the HZB
    g_pHZBManager->Render(pImmediateContext, g_viewMatrix, g_projectionMatrix);

    // Setup bounds
    D3DXVECTOR4* bounds = new D3DXVECTOR4[BoxColumns * BoxRows];
    float*       results = new float[BoxColumns * BoxRows];

    for ( int x = 0; x < BoxRows; ++x ) 
    {
        for ( int y = 0; y < BoxColumns; ++y ) 
        {
            bounds[(y * BoxColumns) + x] = D3DXVECTOR4((y - BoxColumns / 2), 0, x, 0.50);
        }
    }


    // Sends out a request to determine what would be culled given the list of bounds.  This
    // operation can take several milliseconds GPU time, because we're still waiting for the 
    // HZB mipmap generation to occur.
    g_pHZBManager->BeginCullingQuery(pImmediateContext, BoxColumns * BoxRows, bounds, g_viewMatrix, g_projectionMatrix);
    // Attempts to read the results of the query, given the latency of the rendering overhead it's
    // best to schedule some CPU work inbetween the Begin and End Culling Query calls.
    g_pHZBManager->EndCullingQuery(pImmediateContext, BoxColumns * BoxRows, results);


    //------------------------------------------------------------------------------------
    // Render Data
    //------------------------------------------------------------------------------------
    if (g_bRenderResults)
    {
        PROFILE_BLOCK("Render Results");

        pImmediateContext->OMSetRenderTargets(1, &pRTV, pDSV);

        pImmediateContext->ClearRenderTargetView( pRTV, ClearColor );
        pImmediateContext->ClearDepthStencilView( pDSV, D3D11_CLEAR_DEPTH, 1.0, 0 );

        D3DXVECTOR3 observerEye;
        D3DXVECTOR3 observerLookAt;
        D3DXVECTOR3 observerUp;

        if (g_bObserve)
        {
            observerEye = D3DXVECTOR3(5.0f, 15.0f, -10.0f);
            observerLookAt = D3DXVECTOR3(0.0f, 0.0f, 20.0f);
            observerUp = D3DXVECTOR3(0.0f, 1.0f, 0.0f);
        }
        else
        {
            observerEye = g_eye;
            observerLookAt = g_lookAt;
            observerUp = g_up;
        }

        D3DXMatrixLookAtLH( &g_viewMatrix, &observerEye, &observerLookAt, &observerUp );

        // Setup the viewport to match
        D3D11_VIEWPORT vp;
        vp.Width = (FLOAT)width;
        vp.Height = (FLOAT)height;
        vp.MinDepth = 0;
        vp.MaxDepth = 1;
        vp.TopLeftX = 0;
        vp.TopLeftY = 0;
        pImmediateContext->RSSetViewports( 1, &vp );

        for ( int i = 0; i < BoxColumns * BoxRows; ++i )
        {
            if (!g_bShowVisible && results[i] != 0)
                continue;

            ID3DX11EffectMatrixVariable* pWorldVar = g_pPosColorEffect->GetVariableByName("World")->AsMatrix();
            ID3DX11EffectMatrixVariable* pViewVar = g_pPosColorEffect->GetVariableByName("View")->AsMatrix();
            ID3DX11EffectMatrixVariable* pProjectionVar = g_pPosColorEffect->GetVariableByName("Projection")->AsMatrix();

            ID3DX11EffectVectorVariable* pColorVar = g_pPosColorEffect->GetVariableByName("Color")->AsVector();

            D3DXMATRIX localMatrix;
            D3DXMatrixIdentity(&localMatrix);

            D3DXMatrixTranslation(&localMatrix, bounds[i].x, bounds[i].y, bounds[i].z);

            pWorldVar->SetMatrix((float*)&localMatrix);
            pViewVar->SetMatrix((float*)&g_viewMatrix);
            pProjectionVar->SetMatrix((float*)&g_projectionMatrix);

            if (results[i] == 0) // Culled
            {
                pColorVar->SetFloatVector((float*)&RedColor);
            }
            else
            {
                pColorVar->SetFloatVector((float*)&WhiteColor);
            }

            //if (i == g_selectedBoxIndex)
            //{
            //    pColorVar->SetFloatVector((float*)&YellowColor);
            //}

            // Set the input layout
            pImmediateContext->IASetInputLayout( g_pPosColorLayout );

            // Set vertex buffer
            UINT stride = sizeof( VPosColor );
            UINT offset = 0;
            pImmediateContext->IASetVertexBuffers( 0, 1, &g_pPosColorVertexBuffer, &stride, &offset );

            // Set index buffer
            pImmediateContext->IASetIndexBuffer( g_pPosColorIndexBuffer, DXGI_FORMAT_R32_UINT, 0 );

            // Set primitive topology
            pImmediateContext->IASetPrimitiveTopology( D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST );

            ID3DX11EffectTechnique* pTechnique = g_pPosColorEffect->GetTechniqueByIndex(0);
            ID3DX11EffectPass* pPass = pTechnique->GetPassByIndex(0);

            HRESULT hr = pPass->Apply(0, pImmediateContext);
            assert(SUCCEEDED(hr));

            pImmediateContext->DrawIndexed( 36, 0, 0 );
        }
    }

    delete[] bounds;
    delete[] results;
}


//--------------------------------------------------------------------------------------
// Release D3D11 resources created in OnD3D11ResizedSwapChain 
//--------------------------------------------------------------------------------------
void CALLBACK OnD3D11ReleasingSwapChain( void* pUserContext )
{
}


//--------------------------------------------------------------------------------------
// Release D3D11 resources created in OnD3D11CreateDevice 
//--------------------------------------------------------------------------------------
void CALLBACK OnD3D11DestroyDevice( void* pUserContext )
{
    SAFE_RELEASE(g_pPosColorEffect);
    SAFE_RELEASE(g_pPosColorVertexBuffer);
    SAFE_RELEASE(g_pPosColorIndexBuffer);
    SAFE_RELEASE(g_pPosColorLayout);

    delete g_pHZBManager;
}

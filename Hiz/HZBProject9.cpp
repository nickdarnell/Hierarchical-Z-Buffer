//--------------------------------------------------------------------------------------
// File: EmptyProject9.cpp
//
// Empty starting point for new Direct3D 9 and/or Direct3D 11 applications
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------
#include "DXUT.h"
#include "D3DUtil.h"

#include "HZBManagerD3D9.h"

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

HZBManagerD3D9* g_pHZBManager = NULL;

//--------------------------------------------------------------------------------------
// Rejects any D3D9 devices that aren't acceptable to the app by returning false
//--------------------------------------------------------------------------------------
bool CALLBACK IsD3D9DeviceAcceptable( D3DCAPS9* pCaps, D3DFORMAT AdapterFormat, D3DFORMAT BackBufferFormat,
                                      bool bWindowed, void* pUserContext )
{
    // Typically want to skip back buffer formats that don't support alpha blending
    IDirect3D9* pD3D = DXUTGetD3D9Object();
    if( FAILED( pD3D->CheckDeviceFormat( pCaps->AdapterOrdinal, pCaps->DeviceType,
                                         AdapterFormat, D3DUSAGE_QUERY_POSTPIXELSHADER_BLENDING,
                                         D3DRTYPE_TEXTURE, BackBufferFormat ) ) )
        return false;

    return true;
}


//--------------------------------------------------------------------------------------
// Create any D3D9 resources that will live through a device reset (D3DPOOL_MANAGED)
// and aren't tied to the back buffer size
//--------------------------------------------------------------------------------------
HRESULT CALLBACK OnD3D9CreateDevice( IDirect3DDevice9* pDevice, const D3DSURFACE_DESC* pBackBufferSurfaceDesc,
                                     void* pUserContext )
{
    g_pHZBManager = new HZBManagerD3D9(pDevice);
    g_pHZBManager->Initialize();

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Create any D3D9 resources that won't live through a device reset (D3DPOOL_DEFAULT) 
// or that are tied to the back buffer size 
//--------------------------------------------------------------------------------------
HRESULT CALLBACK OnD3D9ResetDevice( IDirect3DDevice9* pd3dDevice, const D3DSURFACE_DESC* pBackBufferSurfaceDesc,
                                    void* pUserContext )
{
    return S_OK;
}


//--------------------------------------------------------------------------------------
// Render the scene using the D3D9 device
//--------------------------------------------------------------------------------------
void CALLBACK OnD3D9FrameRender( IDirect3DDevice9* pDevice, double fTime, float fElapsedTime, void* pUserContext )
{
    HRESULT hr;

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


    // Render the scene
    if( SUCCEEDED( pDevice->BeginScene() ) )
    {
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

        {
            PROFILE_BLOCK;

            // Renders the occluders and generates the HZB
            g_pHZBManager->Render(g_viewMatrix, g_projectionMatrix);

            // Sends out a request to determine what would be culled given the list of bounds.  This
            // operation can take several milliseconds GPU time, because we're still waiting for the 
            // HZB mipmap generation to occur.
            g_pHZBManager->BeginCullingQuery(BoxColumns * BoxRows, bounds, g_viewMatrix, g_projectionMatrix);
            // Attempts to read the results of the query, given the latency of the rendering overhead it's
            // best to schedule some CPU work inbetween the Begin and End Culling Query calls.
            g_pHZBManager->EndCullingQuery(BoxColumns * BoxRows, results);
        }


        //------------------------------------------------------------------------------------
        // Render Data
        //------------------------------------------------------------------------------------
        if (g_bRenderResults)
        {
            D3DVIEWPORT9 vp;
            vp.Width = width;
            vp.Height = height;
            vp.MinZ = 0;
            vp.MaxZ = 1;
            vp.X = 0;
            vp.Y = 0;
            HRESULT hr = pDevice->SetViewport(&vp);
            assert(SUCCEEDED(hr));

            IDirect3DSurface9* pBackBuffer = NULL;
            hr = pDevice->GetBackBuffer( 0, 0, D3DBACKBUFFER_TYPE_MONO, &pBackBuffer );

            // 
            hr = pDevice->SetRenderTarget(0, pBackBuffer);
            assert(SUCCEEDED(hr));

            hr = pDevice->SetDepthStencilSurface(NULL);
            assert(SUCCEEDED(hr));
            
            // Clear the render target and the zbuffer 
            V( pDevice->Clear( 0, NULL, D3DCLEAR_TARGET/* | D3DCLEAR_ZBUFFER */, D3DCOLOR_ARGB( 0, 45, 50, 170 ), 1.0f, 0 ) );

            SAFE_RELEASE(pBackBuffer);
        }

        V( pDevice->EndScene() );

        delete[] bounds;
        delete[] results;
    }
}


//--------------------------------------------------------------------------------------
// Release D3D9 resources created in the OnD3D9ResetDevice callback 
//--------------------------------------------------------------------------------------
void CALLBACK OnD3D9LostDevice( void* pUserContext )
{
}


//--------------------------------------------------------------------------------------
// Release D3D9 resources created in the OnD3D9CreateDevice callback 
//--------------------------------------------------------------------------------------
void CALLBACK OnD3D9DestroyDevice( void* pUserContext )
{
    if (g_pHZBManager)
        delete g_pHZBManager;
}

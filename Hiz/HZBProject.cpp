#include "DXUT.h"

//--------------------------------------------------------------------------------------
// Extern declarations 
//--------------------------------------------------------------------------------------
bool CALLBACK IsD3D11DeviceAcceptable( const CD3D11EnumAdapterInfo *AdapterInfo, UINT Output, const CD3D11EnumDeviceInfo *DeviceInfo,
                                       DXGI_FORMAT BackBufferFormat, bool bWindowed, void* pUserContext );
HRESULT CALLBACK OnD3D11CreateDevice( ID3D11Device* pd3dDevice, const DXGI_SURFACE_DESC* pBackBufferSurfaceDesc,
                                      void* pUserContext );
HRESULT CALLBACK OnD3D11ResizedSwapChain( ID3D11Device* pd3dDevice, IDXGISwapChain* pSwapChain,
                                          const DXGI_SURFACE_DESC* pBackBufferSurfaceDesc, void* pUserContext );
void CALLBACK OnD3D11FrameRender( ID3D11Device* pd3dDevice, ID3D11DeviceContext* pImmediateContext,
                                  double fTime, float fElapsedTime, void* pUserContext );
void CALLBACK OnD3D11ReleasingSwapChain( void* pUserContext );
void CALLBACK OnD3D11DestroyDevice( void* pUserContext );



extern bool CALLBACK IsD3D9DeviceAcceptable( D3DCAPS9* pCaps, D3DFORMAT AdapterFormat, D3DFORMAT BackBufferFormat,
                                             bool bWindowed, void* pUserContext );
extern HRESULT CALLBACK OnD3D9CreateDevice( IDirect3DDevice9* pd3dDevice,
                                            const D3DSURFACE_DESC* pBackBufferSurfaceDesc, void* pUserContext );
extern HRESULT CALLBACK OnD3D9ResetDevice( IDirect3DDevice9* pd3dDevice, const D3DSURFACE_DESC* pBackBufferSurfaceDesc,
                                           void* pUserContext );
extern void CALLBACK OnD3D9FrameRender( IDirect3DDevice9* pd3dDevice, double fTime, float fElapsedTime,
                                        void* pUserContext );
extern void CALLBACK OnD3D9LostDevice( void* pUserContext );
extern void CALLBACK OnD3D9DestroyDevice( void* pUserContext );

//--------------------------------------------------------------------------------------
// Global Data
//--------------------------------------------------------------------------------------

bool g_bRenderResults = true;
bool g_bObserve = true;
bool g_bShowVisible = true;
bool g_bShowOccluders = true;

int g_selectedBoxIndex = 0;

D3DXVECTOR3 g_eye(0.0f, 0.0f, 6.0f);
D3DXVECTOR3 g_lookAt(0.0f, 0.0f, 7.0f);
D3DXVECTOR3 g_up(0.0f, 1.0f, 0.0f);

int BoxColumns = 30;
int BoxRows = 30;

D3DXMATRIX g_worldMatrix;
D3DXMATRIX g_viewMatrix;
D3DXMATRIX g_projectionMatrix;

//--------------------------------------------------------------------------------------
// Handle key presses
//--------------------------------------------------------------------------------------
void CALLBACK OnKeyboard( UINT nChar, bool bKeyDown, bool bAltDown, void* pUserContext )
{
    //if (bKeyDown && nChar == '1')
    //{
    //    g_bDrawOccluders = !g_bDrawOccluders;
    //}

    if (bKeyDown && nChar == '2')
    {
        g_bObserve = !g_bObserve;
    }

    if (bKeyDown && nChar == '3')
    {
        g_bShowVisible = !g_bShowVisible;
    }

    if (bKeyDown && nChar == '4')
    {
        g_bShowOccluders = !g_bShowOccluders;
    }

    if (bKeyDown && nChar == '5')
    {
        g_bRenderResults = !g_bRenderResults;
    }

    if (bKeyDown && nChar == VK_LEFT)
    {
        g_eye.x--;
        g_lookAt.x--;
    }
    if (bKeyDown && nChar == VK_RIGHT)
    {
        g_eye.x++;
        g_lookAt.x++;
    }
    if (bKeyDown && nChar == VK_UP)
    {
        g_eye.z++;
        g_lookAt.z++;
    }
    if (bKeyDown && nChar == VK_DOWN)
    {
        g_eye.z--;
        g_lookAt.z--;
    }
    if (bKeyDown && nChar == VK_PRIOR) // Page Up
    {
        g_eye.y++;
        g_lookAt.y++;
    }
    if (bKeyDown && nChar == VK_NEXT) // Page Down
    {
        g_eye.y--;
        g_lookAt.y--;
    }
    
    if (bKeyDown && nChar == 'W')
        g_selectedBoxIndex += BoxColumns;
    if (bKeyDown && nChar == 'A')
        g_selectedBoxIndex -= 1;
    if (bKeyDown && nChar == 'S')
        g_selectedBoxIndex -= BoxColumns;
    if (bKeyDown && nChar == 'D')
        g_selectedBoxIndex += 1;
}

//--------------------------------------------------------------------------------------
// Handle mouse button presses
//--------------------------------------------------------------------------------------
void CALLBACK OnMouse( bool bLeftButtonDown, bool bRightButtonDown, bool bMiddleButtonDown,
                       bool bSideButton1Down, bool bSideButton2Down, int nMouseWheelDelta,
                       int xPos, int yPos, void* pUserContext )
{
}

//--------------------------------------------------------------------------------------
// Handle updates to the scene.  This is called regardless of which D3D API is used
//--------------------------------------------------------------------------------------
void CALLBACK OnFrameMove( double fTime, float fElapsedTime, void* pUserContext )
{
}

//--------------------------------------------------------------------------------------
// Handle messages to the application
//--------------------------------------------------------------------------------------
LRESULT CALLBACK MsgProc( HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam,
                          bool* pbNoFurtherProcessing, void* pUserContext )
{
    return 0;
}

//--------------------------------------------------------------------------------------
// Called right before creating a D3D9 or D3D11 device, allowing the app to modify the device settings as needed
//--------------------------------------------------------------------------------------
bool CALLBACK ModifyDeviceSettings( DXUTDeviceSettings* pDeviceSettings, void* pUserContext )
{
    return true;
}

//--------------------------------------------------------------------------------------
// Call if device was removed.  Return true to find a new device, false to quit
//--------------------------------------------------------------------------------------
bool CALLBACK OnDeviceRemoved( void* pUserContext )
{
    return true;
}


//--------------------------------------------------------------------------------------
// Initialize everything and go into a render loop
//--------------------------------------------------------------------------------------
int WINAPI wWinMain( HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR lpCmdLine, int nCmdShow )
{
    // Enable run-time memory check for debug builds.
#if defined(DEBUG) | defined(_DEBUG)
    _CrtSetDbgFlag( _CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF );
#endif

    // DXUT will create and use the best device (either D3D9 or D3D11) 
    // that is available on the system depending on which D3D callbacks are set below

    // Set general DXUT callbacks
    DXUTSetCallbackFrameMove( OnFrameMove );
    DXUTSetCallbackKeyboard( OnKeyboard );
    DXUTSetCallbackMouse( OnMouse );
    DXUTSetCallbackMsgProc( MsgProc );
    DXUTSetCallbackDeviceChanging( ModifyDeviceSettings );
    DXUTSetCallbackDeviceRemoved( OnDeviceRemoved );

    // Set the D3D9 DXUT callbacks. Remove these sets if the app doesn't need to support D3D9
    DXUTSetCallbackD3D9DeviceAcceptable( IsD3D9DeviceAcceptable );
    DXUTSetCallbackD3D9DeviceCreated( OnD3D9CreateDevice );
    DXUTSetCallbackD3D9DeviceReset( OnD3D9ResetDevice );
    DXUTSetCallbackD3D9FrameRender( OnD3D9FrameRender );
    DXUTSetCallbackD3D9DeviceLost( OnD3D9LostDevice );
    DXUTSetCallbackD3D9DeviceDestroyed( OnD3D9DestroyDevice );

    // Set the D3D11 DXUT callbacks. Remove these sets if the app doesn't need to support D3D11
    DXUTSetCallbackD3D11DeviceAcceptable( IsD3D11DeviceAcceptable );
    DXUTSetCallbackD3D11DeviceCreated( OnD3D11CreateDevice );
    DXUTSetCallbackD3D11SwapChainResized( OnD3D11ResizedSwapChain );
    DXUTSetCallbackD3D11FrameRender( OnD3D11FrameRender );
    DXUTSetCallbackD3D11SwapChainReleasing( OnD3D11ReleasingSwapChain );
    DXUTSetCallbackD3D11DeviceDestroyed( OnD3D11DestroyDevice );

    // Perform any application-level initialization here

    DXUTInit( true, true, NULL ); // Parse the command line, show msgboxes on error, no extra command line params
    DXUTSetCursorSettings( true, true ); // Show the cursor and clip it when in full screen
    DXUTCreateWindow( L"Hiz" );

    // Only require 10-level hardware
    DXUTCreateDevice( D3D_FEATURE_LEVEL_10_0, true, 1024, 768 );
    DXUTMainLoop(); // Enter into the DXUT ren  der loop

    // Perform any application-level cleanup here

    return DXUTGetExitCode();
}
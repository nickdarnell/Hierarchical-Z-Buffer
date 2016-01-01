#include "DXUT.h"
#include "d3dx11effect.h"
#include "Common.h"

float ClearColor[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
float WhiteColor[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
float RedColor[4] = { 1.0f, 0.0f, 0.0f, 1.0f };
float BlueColor[4] = { 0.0f, 0.0f, 1.0f, 1.0f };
float YellowColor[4] = { 1.0f, 1.0f, 0.0f, 1.0f };

void cdecl HZBDebugPrintf(LPCSTR szFormat, ...)
{
    static UINT uDebugLevel = (UINT) -1;

    char strA[4096];
    char strB[4096];

    va_list ap;
    va_start(ap, szFormat);
    StringCchVPrintfA(strA, sizeof(strA), szFormat, ap);
    strA[4095] = '\0';
    va_end(ap);

    StringCchPrintfA(strB, sizeof(strB), "Hiz: %s\r\n", strA);

    strB[4095] = '\0';

    OutputDebugStringA(strB);
}

HRESULT LoadEffectFromFile11(ID3D11Device *pDevice, LPCWSTR pSrcFile, ID3DX11Effect** ppEffect)
{
    DWORD dwShaderFlags = D3D10_SHADER_ENABLE_STRICTNESS;
#if defined( DEBUG ) || defined( _DEBUG )
    // Set the D3D10_SHADER_DEBUG flag to embed debug information in the shaders.
    // Setting this flag improves the shader debugging experience, but still allows 
    // the shaders to be optimized and to run exactly the way they will run in 
    // the release configuration of this program.
    dwShaderFlags |= D3D10_SHADER_DEBUG;
#endif

    ID3D10Blob *pEffectBlob = NULL;
	ID3D10Blob *pErrorBlob = NULL;
	HRESULT hr = D3DX11CompileFromFile(pSrcFile, NULL, NULL, NULL, 
        "fx_5_0", dwShaderFlags, NULL, NULL, &pEffectBlob, &pErrorBlob, NULL);

	if (FAILED(hr))
	{
		if (hr == D3D11_ERROR_FILE_NOT_FOUND)
		{
			HZBDebugPrintf("Failed to compile effect. File not found.\n");
		}
		else
		{
			if (pErrorBlob)
			{
				LPVOID error = pErrorBlob->GetBufferPointer();
				HZBDebugPrintf("Failed to compile effect. %s\n", (char*)error);
			}
			else
			{
				HZBDebugPrintf("Failed to compile effect. Code: %d\n", HRESULT_CODE(hr));
			}
		}

        SAFE_RELEASE( pErrorBlob );
        SAFE_RELEASE( pEffectBlob );

		return E_FAIL;
	}

    hr = D3DX11CreateEffectFromMemory(pEffectBlob->GetBufferPointer(), pEffectBlob->GetBufferSize(), 
        0, pDevice, ppEffect);

    SAFE_RELEASE( pErrorBlob );
    SAFE_RELEASE( pEffectBlob );

	if (FAILED(hr))
	{
		HZBDebugPrintf("Failed to create effect. Error code: %d\n", HRESULT_CODE(hr));
		return E_FAIL;
	}

    return S_OK;
}

HRESULT CreateComputeShader11(LPCWSTR pSrcFile, LPCSTR pFunctionName, 
                              ID3D11Device* pDevice, ID3D11ComputeShader** ppShaderOut)
{
    HRESULT hr;
   
    DWORD dwShaderFlags = D3D10_SHADER_ENABLE_STRICTNESS;
#if defined( DEBUG ) || defined( _DEBUG )
    // Set the D3D10_SHADER_DEBUG flag to embed debug information in the shaders.
    // Setting this flag improves the shader debugging experience, but still allows 
    // the shaders to be optimized and to run exactly the way they will run in 
    // the release configuration of this program.
    dwShaderFlags |= D3D10_SHADER_DEBUG;
#endif

    // We generally prefer to use the higher CS shader profile when possible as CS 5.0 is better performance 
    // on 11-class hardware
    LPCSTR pProfile = ( pDevice->GetFeatureLevel() >= D3D_FEATURE_LEVEL_11_0 ) ? "cs_5_0" : "cs_4_0";

    ID3DBlob* pErrorBlob = NULL;
    ID3DBlob* pBlob = NULL;
    hr = D3DX11CompileFromFile( pSrcFile, NULL, NULL, pFunctionName, pProfile, 
        dwShaderFlags, NULL, NULL, &pBlob, &pErrorBlob, NULL );

    if (FAILED(hr))
	{
		if (hr == D3D11_ERROR_FILE_NOT_FOUND)
		{
			HZBDebugPrintf("Failed to compile effect. File not found.\n");
		}
		else
		{
			if (pErrorBlob)
			{
				LPVOID error = pErrorBlob->GetBufferPointer();
				HZBDebugPrintf("Failed to compile effect. %s\n", (char*)error);
			}
			else
			{
				HZBDebugPrintf("Failed to compile effect. Code: %d\n", HRESULT_CODE(hr));
			}
		}

        SAFE_RELEASE( pErrorBlob );
        SAFE_RELEASE( pBlob );

		return E_FAIL;
	}

    hr = pDevice->CreateComputeShader( pBlob->GetBufferPointer(), pBlob->GetBufferSize(), NULL, ppShaderOut );

    SAFE_RELEASE( pErrorBlob );
    SAFE_RELEASE( pBlob );

    return hr;
}

HRESULT CreateStructuredBuffer11(
    ID3D11Device* pDevice,
    UINT uElementSize,
    UINT uCount,
    VOID* pInitData,
    D3D11_USAGE usage,
    ID3D11Buffer** ppBufOut )
{
    *ppBufOut = NULL;

    D3D11_BUFFER_DESC desc;
    ZeroMemory( &desc, sizeof(desc) );
    desc.Usage = usage;
    desc.BindFlags = D3D11_BIND_UNORDERED_ACCESS | D3D11_BIND_SHADER_RESOURCE;
    desc.ByteWidth = uElementSize * uCount;
    desc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
    desc.StructureByteStride = uElementSize;

    if (usage == D3D11_USAGE_DYNAMIC)
    {
        desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
        desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    }
    else if (usage == D3D11_USAGE_STAGING)
    {
        desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ | D3D11_CPU_ACCESS_WRITE;
    }

    if ( pInitData )
    {
        D3D11_SUBRESOURCE_DATA InitData;
        InitData.pSysMem = pInitData;
        return pDevice->CreateBuffer( &desc, &InitData, ppBufOut );
    }
    
    return pDevice->CreateBuffer( &desc, NULL, ppBufOut );
}

HRESULT CreateBufferSRV11(ID3D11Device* pDevice, ID3D11Buffer* pBuffer, ID3D11ShaderResourceView** ppSRVOut)
{
    D3D11_BUFFER_DESC descBuf;
    ZeroMemory( &descBuf, sizeof(descBuf) );
    pBuffer->GetDesc( &descBuf );

    D3D11_SHADER_RESOURCE_VIEW_DESC desc;
    ZeroMemory( &desc, sizeof(desc) );
    desc.ViewDimension = D3D11_SRV_DIMENSION_BUFFEREX;
    desc.BufferEx.FirstElement = 0;

    if ( descBuf.MiscFlags & D3D11_RESOURCE_MISC_BUFFER_ALLOW_RAW_VIEWS )
    {
        // This is a Raw Buffer

        desc.Format = DXGI_FORMAT_R32_TYPELESS;
        desc.BufferEx.Flags = D3D11_BUFFEREX_SRV_FLAG_RAW;
        desc.BufferEx.NumElements = descBuf.ByteWidth / 4;
    }
    else if ( descBuf.MiscFlags & D3D11_RESOURCE_MISC_BUFFER_STRUCTURED )
    {
        // This is a Structured Buffer

        desc.Format = DXGI_FORMAT_UNKNOWN;
        desc.BufferEx.NumElements = descBuf.ByteWidth / descBuf.StructureByteStride;
    }
    else
    {
        return E_INVALIDARG;
    }

    return pDevice->CreateShaderResourceView( pBuffer, &desc, ppSRVOut );
}

//--------------------------------------------------------------------------------------
// Create Unordered Access View for Structured or Raw Buffers
//-------------------------------------------------------------------------------------- 
HRESULT CreateBufferUAV11(ID3D11Device* pDevice, ID3D11Buffer* pBuffer, ID3D11UnorderedAccessView** ppUAVOut)
{
    D3D11_BUFFER_DESC descBuf;
    ZeroMemory( &descBuf, sizeof(descBuf) );
    pBuffer->GetDesc( &descBuf );
    
    D3D11_UNORDERED_ACCESS_VIEW_DESC desc;
    ZeroMemory( &desc, sizeof(desc) );
    desc.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
    desc.Buffer.FirstElement = 0;

    if ( descBuf.MiscFlags & D3D11_RESOURCE_MISC_BUFFER_ALLOW_RAW_VIEWS )
    {
        // This is a Raw Buffer

        desc.Format = DXGI_FORMAT_R32_TYPELESS; // Format must be DXGI_FORMAT_R32_TYPELESS, when creating Raw Unordered Access View
        desc.Buffer.Flags = D3D11_BUFFER_UAV_FLAG_RAW;
        desc.Buffer.NumElements = descBuf.ByteWidth / 4; 
    }
    else if ( descBuf.MiscFlags & D3D11_RESOURCE_MISC_BUFFER_STRUCTURED )
    {
        // This is a Structured Buffer

        desc.Format = DXGI_FORMAT_UNKNOWN;      // Format must be must be DXGI_FORMAT_UNKNOWN, when creating a View of a Structured Buffer
        desc.Buffer.NumElements = descBuf.ByteWidth / descBuf.StructureByteStride; 
    }
    else
    {
        return E_INVALIDARG;
    }
    
    return pDevice->CreateUnorderedAccessView( pBuffer, &desc, ppUAVOut );
}



HRESULT LoadEffectFromFile9(LPDIRECT3DDEVICE9 pDevice, LPCWSTR pSrcFile, LPD3DXEFFECT* ppEffect)
{
    DWORD dwShaderFlags = 0;
#if defined( DEBUG ) || defined( _DEBUG )
    dwShaderFlags |= D3DXSHADER_DEBUG;
#endif

    LPD3DXBUFFER pCompilationErrors;
    HRESULT hr = D3DXCreateEffectFromFileExW(pDevice, pSrcFile, 
        NULL, NULL, NULL, dwShaderFlags, NULL, ppEffect, &pCompilationErrors);

    if (FAILED(hr))
	{
		if (pCompilationErrors)
		{
			LPVOID error = pCompilationErrors->GetBufferPointer();
			HZBDebugPrintf("Failed to compile effect. %s\n", (char*)error);
		}
		else
		{
			HZBDebugPrintf("Failed to compile effect. Code: %d\n", HRESULT_CODE(hr));
		}

        SAFE_RELEASE( pCompilationErrors );

		return E_FAIL;
	}

    return S_OK;
}

void NormalizePlane(Plane& plane)
{
    float mag;
    mag = sqrt(plane.a * plane.a + plane.b * plane.b + plane.c * plane.c);

    plane.a = plane.a / mag;
    plane.b = plane.b / mag;
    plane.c = plane.c / mag;
    plane.d = plane.d / mag;
}

void ExtractFrustum(Frustum& frustum, const D3DXMATRIX& viewProjMatrix)
{
    frustum.Left.a = viewProjMatrix._14 + viewProjMatrix._11;
    frustum.Left.b = viewProjMatrix._24 + viewProjMatrix._21;
    frustum.Left.c = viewProjMatrix._34 + viewProjMatrix._31;
    frustum.Left.d = viewProjMatrix._44 + viewProjMatrix._41;

    frustum.Right.a = viewProjMatrix._14 - viewProjMatrix._11;
    frustum.Right.b = viewProjMatrix._24 - viewProjMatrix._21;
    frustum.Right.c = viewProjMatrix._34 - viewProjMatrix._31;
    frustum.Right.d = viewProjMatrix._44 - viewProjMatrix._41;

    frustum.Top.a = viewProjMatrix._14 - viewProjMatrix._12;
    frustum.Top.b = viewProjMatrix._24 - viewProjMatrix._22;
    frustum.Top.c = viewProjMatrix._34 - viewProjMatrix._32;
    frustum.Top.d = viewProjMatrix._44 - viewProjMatrix._42;

    frustum.Bottom.a = viewProjMatrix._14 + viewProjMatrix._12;
    frustum.Bottom.b = viewProjMatrix._24 + viewProjMatrix._22;
    frustum.Bottom.c = viewProjMatrix._34 + viewProjMatrix._32;
    frustum.Bottom.d = viewProjMatrix._44 + viewProjMatrix._42;

    frustum.Near.a = viewProjMatrix._13;
    frustum.Near.b = viewProjMatrix._23;
    frustum.Near.c = viewProjMatrix._33;
    frustum.Near.d = viewProjMatrix._43;

    frustum.Far.a = viewProjMatrix._14 - viewProjMatrix._13;
    frustum.Far.b = viewProjMatrix._24 - viewProjMatrix._23;
    frustum.Far.c = viewProjMatrix._34 - viewProjMatrix._33;
    frustum.Far.d = viewProjMatrix._44 - viewProjMatrix._43;

    NormalizePlane(frustum.Left);
    NormalizePlane(frustum.Right);
    NormalizePlane(frustum.Top);
    NormalizePlane(frustum.Bottom);
    NormalizePlane(frustum.Near);
    NormalizePlane(frustum.Far);
}

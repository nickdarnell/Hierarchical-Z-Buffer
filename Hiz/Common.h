#pragma once
#ifndef COMMON_H
#define COMMON_H

#include "DXUT.h"
#include "d3dx11effect.h"
#include <strsafe.h>

extern float ClearColor[4];
extern float WhiteColor[4];
extern float RedColor[4];
extern float BlueColor[4];
extern float YellowColor[4];

void cdecl HZBDebugPrintf(LPCSTR szFormat, ...);

HRESULT LoadEffectFromFile11(
    ID3D11Device *pDevice,
    LPCWSTR pSrcFile,
    ID3DX11Effect** ppEffect);

HRESULT CreateComputeShader11(
    LPCWSTR pSrcFile,
    LPCSTR pFunctionName, 
    ID3D11Device* pDevice,
    ID3D11ComputeShader** ppShaderOut);

HRESULT CreateStructuredBuffer11(
    ID3D11Device* pDevice,
    UINT uElementSize,
    UINT uCount,
    VOID* pInitData,
    D3D11_USAGE usage,
    ID3D11Buffer** ppBufOut);

HRESULT CreateBufferSRV11(
    ID3D11Device* pDevice,
    ID3D11Buffer* pBuffer,
    ID3D11ShaderResourceView** ppSRVOut);

//--------------------------------------------------------------------------------------
// Create Unordered Access View for Structured or Raw Buffers
//-------------------------------------------------------------------------------------- 
HRESULT CreateBufferUAV11(
    ID3D11Device* pDevice,
    ID3D11Buffer* pBuffer,
    ID3D11UnorderedAccessView** ppUAVOut);





HRESULT LoadEffectFromFile9(LPDIRECT3DDEVICE9 pDevice, LPCWSTR pSrcFile, LPD3DXEFFECT* ppEffect);




struct VPos
{
	D3DXVECTOR4 Pos;  
};

struct VPosColor
{
    D3DXVECTOR3 Pos;
    D3DXVECTOR4 Color;
};

struct VPosColorTex
{
    D3DXVECTOR3 Pos;
    D3DXVECTOR4 Color;
    D3DXVECTOR2 Tex;
};







struct Plane
{
    float a, b, c, d;
};

union Frustum
{
    struct
    {
        Plane Left;
        Plane Right;
        Plane Top;
        Plane Bottom;
        Plane Near;
        Plane Far;
    };
    Plane Planes[6];
};

void NormalizePlane(Plane& plane);

void ExtractFrustum(Frustum& frustum, const D3DXMATRIX& viewProjMatrix);



#endif
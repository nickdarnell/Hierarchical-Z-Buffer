#define SM4
//#define SupportNonPow2

Texture2D<float> LastMip;

#ifdef SM5
SamplerState s;
#endif

void VS( in  float4 Position   : POSITION,
         out float4 PositionSS : SV_POSITION )
{
	PositionSS = Position;
}

float4 PS( float4 PositionSS : SV_POSITION ) : SV_TARGET
{
    uint lastMipWidth, lastMipHeight;
    LastMip.GetDimensions(lastMipWidth, lastMipHeight);
    
    // get integer pixel coordinates
    uint3 nCoords = uint3( PositionSS.xy, 0 );
    
    // fetch a 2x2 neighborhood and compute the max
    nCoords.xy *= 2;
    
#ifdef SM4
    float4 vTexels;
    vTexels.x = LastMip.Load( nCoords );
    vTexels.y = LastMip.Load( nCoords, uint2(1,0) );
    vTexels.z = LastMip.Load( nCoords, uint2(0,1) );
    vTexels.w = LastMip.Load( nCoords, uint2(1,1) );
#else
    // SM5
    float4 vTexels;
    vTexels = LastMip.Gather( nCoords );
#endif
    
    // Determine the largest depth value and use it as the new down sampled
    // color.
    float fMaxDepth = max( max( vTexels.x, vTexels.y ), max( vTexels.z, vTexels.w ) );
    
#ifdef SupportNonPow2
    // TODO NDarnell We could throw away the rest of this code for other textures.
    // Maybe break this into another shader and use seperate shaders for oddly sized textures.
    
    // if we are reducing an odd-sized texture,
    // then the edge pixels need to fetch additional texels
    float2 vExtra;
    if( (lastMipWidth & 1) && nCoords.x == lastMipWidth-3 )
    {
        vExtra.x = LastMip.Load( nCoords, uint2(2,0) );
        vExtra.y = LastMip.Load( nCoords, uint2(2,1) );
        fMaxDepth = max( fMaxDepth, max( vExtra.x, vExtra.y ) );
    }
    
    if( (lastMipHeight & 1) && nCoords.y == lastMipHeight-3 )
    {
        vExtra.x = LastMip.Load( nCoords, uint2(0,2) );
        vExtra.y = LastMip.Load( nCoords, uint2(1,2) );
        fMaxDepth = max( fMaxDepth, max( vExtra.x, vExtra.y ) );
    }
    
    // extreme case: If both edges are odd, fetch the bottom-right corner texel
    if( ( ( lastMipWidth & 1 ) && ( lastMipHeight & 1 ) ) &&
        nCoords.x == lastMipWidth-3 && nCoords.y == lastMipHeight-3 )
    {
        fMaxDepth = max( fMaxDepth, LastMip.Load( nCoords, uint2(2,2) ) );
    }
#endif
    
    return fMaxDepth;
}

technique10 Render
{
	pass P0
	{
		SetVertexShader( CompileShader( vs_4_0, VS() ) );
		SetGeometryShader( 0 );
		SetPixelShader( CompileShader( ps_4_0, PS() ) );
	}
}
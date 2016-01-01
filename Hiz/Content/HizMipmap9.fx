// x = width, y = height, z = mip level
float3 LastMipInfo;

texture2D LastMip;
sampler2D LastMipSampler = sampler_state
{
    Texture = <LastMip>;
    MinFilter = Point;
    MagFilter = Point;
    MipFilter = Point;
};

void VS( in  float4 Position   : POSITION0,
         out float4 PositionSS : POSITION0 )
{
	PositionSS = Position;
}

float4 PS( in float4 Position   : POSITION0,
           in float4 PositionSS : VPOS ) : Color0
{
    float width = LastMipInfo.x;
    float height = LastMipInfo.y;
    float mip = LastMipInfo.z;
    
    // get the upper left pixel coordinates of the 2x2 block we're going to downsample.
    // we need to muliply the screenspace positions components by 2 because we're sampling
    // the previous mip, so the maximum x/y values will be half as much of the mip we're
    // rendering to.
    float2 nCoords0 = float2((PositionSS.x * 2) / width, (PositionSS.y * 2) / height);
    
    float2 nCoords1 = float2(nCoords0.x + (1 / width), nCoords0.y);
    float2 nCoords2 = float2(nCoords0.x, nCoords0.y + (1 / height));
    float2 nCoords3 = float2(nCoords1.x, nCoords2.y);
    
    // fetch a 2x2 neighborhood and compute the max
    float4 vTexels;
    vTexels.x = tex2Dlod( LastMipSampler, float4(nCoords0, 0, mip) ).x;
    vTexels.y = tex2Dlod( LastMipSampler, float4(nCoords1, 0, mip) ).x;
    vTexels.z = tex2Dlod( LastMipSampler, float4(nCoords2, 0, mip) ).x;
    vTexels.w = tex2Dlod( LastMipSampler, float4(nCoords3, 0, mip) ).x;
    float fMaxDepth = max( max( vTexels.x, vTexels.y ), max( vTexels.z, vTexels.w ) );
        
    return fMaxDepth;
}

technique Render
{
    pass Pass1
    {
        Sampler[0] = (LastMipSampler);
        VertexShader = compile vs_3_0 VS();
        pixelShader = compile ps_3_0 PS();
    }
}
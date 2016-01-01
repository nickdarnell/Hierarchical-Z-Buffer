matrix WorldViewProjection;

struct VS_IN
{
	float4 Position : POSITION0;
};

struct PS_IN
{
	float4 Position : SV_POSITION;
	float Depth     : ZDEPTH;
};

PS_IN VS( VS_IN input )
{
	PS_IN output;
	
    // Transform the position into screen space.
    output.Position = mul(input.Position, WorldViewProjection);

    // Record the depth value to pass to the pixel shader
    output.Depth = output.Position.z / output.Position.w;
	
	return output;
}

float4 PS( PS_IN input ) : SV_Target
{
	return float4(input.Depth, input.Depth, input.Depth, 1);
}

technique11 Render
{
	pass P0
	{
		SetGeometryShader( 0 );
		SetVertexShader( CompileShader( vs_4_0, VS() ) );
		SetPixelShader( CompileShader( ps_4_0, PS() ) );
	}
}
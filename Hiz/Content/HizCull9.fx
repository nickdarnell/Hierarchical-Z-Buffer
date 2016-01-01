float4x4 View;
float4x4 Projection;
float4x4 ViewProjection;

float4 FrustumPlanes[6];    // view-frustum planes in world space (normals face out)

float2 ViewportSize;        // Viewport Width and Height in pixels
float2 ResultsSize;         // The width and height in pixels of the render target we're rendering the points to.

texture2D HiZMapEven;
sampler2D HiZMapEvenSampler = sampler_state
{
    Texture = <HiZMapEven>;
    MinFilter = Point;
    MagFilter = Point;
    MipFilter = Point;
};

texture2D HiZMapOdd;
sampler2D HiZMapOddSampler = sampler_state
{
    Texture = <HiZMapOdd>;
    MinFilter = Point;
    MagFilter = Point;
    MipFilter = Point;
};

struct VS_IN
{
    // Bounding sphere center (XYZ) and radius (W), world space
    float4 Position : POSITION0;
    // xy  = Screen space location of the final result visibility pixel
	float2 Pixel    : TEXCOORD0;
};

struct PS_IN
{
    float4 Position : POSITION0;
    // Bounding sphere center (XYZ) and radius (W), world space
	float4 Bound    : TEXCOORD0;
};

// Computes signed distance between a point and a plane
// vPlane: Contains plane coefficients (a,b,c,d) where: ax + by + cz = d
// vPoint: Point to be tested against the plane.
float DistanceToPlane( float4 vPlane, float3 vPoint )
{
    return dot(float4(vPoint, 1), vPlane);
}

// Frustum cullling on a sphere. Returns > 0 if visible, <= 0 otherwise
float CullSphere( float4 vPlanes[6], float3 vCenter, float fRadius )
{
   float dist01 = min(DistanceToPlane(vPlanes[0], vCenter), DistanceToPlane(vPlanes[1], vCenter));
   float dist23 = min(DistanceToPlane(vPlanes[2], vCenter), DistanceToPlane(vPlanes[3], vCenter));
   float dist45 = min(DistanceToPlane(vPlanes[4], vCenter), DistanceToPlane(vPlanes[5], vCenter));
   
   return min(min(dist01, dist23), dist45) + fRadius;
}

PS_IN VS( VS_IN input )
{
    PS_IN output;
    // Calculate the pixel location of this bounds information so that we can encode it into
    // the render target at an 'index' we can read back later.
    output.Position = float4((input.Pixel.x / ResultsSize.x * 2) - 1, (input.Pixel.y / ResultsSize.y * -2) + 1, 0, 1);
    
    // Bounding sphere center (XYZ) and radius (W), world space
    output.Bound = input.Position;
    
    return output;
}

float4 PS( PS_IN input ) : COLOR0
{
    float3 BoundCenterWS = input.Bound.xyz;
    float BoundRadius = input.Bound.w;
    
    // Perform view-frustum test
    float fVisible = CullSphere(FrustumPlanes, BoundCenterWS, BoundRadius);
    
    if (fVisible > 0)
    {
        float3 viewEye = -View._m03_m13_m23;
        float CameraSphereDistance = distance( viewEye, BoundCenterWS );
        
        float3 viewEyeSphereDirection = viewEye - BoundCenterWS;
        
        float3 viewUp = View._m01_m11_m21;
        float3 viewDirection = View._m02_m12_m22;
        float3 viewRight = normalize(cross(viewEyeSphereDirection, viewUp));
        
        // Help deal with the perspective distortion.
        // http://article.gmane.org/gmane.games.devel.algorithms/21697/
        float fRadius = CameraSphereDistance * tan(asin(BoundRadius / CameraSphereDistance));
        
        // Compute the offsets for the points around the sphere
        float3 vUpRadius = viewUp * fRadius;
        float3 vRightRadius = viewRight * fRadius;
        
        // Generate the 4 corners of the sphere in world space.
        float4 vCorner0WS = float4( BoundCenterWS + vUpRadius - vRightRadius, 1 ); // Top-Left
        float4 vCorner1WS = float4( BoundCenterWS + vUpRadius + vRightRadius, 1 ); // Top-Right
        float4 vCorner2WS = float4( BoundCenterWS - vUpRadius - vRightRadius, 1 ); // Bottom-Left
        float4 vCorner3WS = float4( BoundCenterWS - vUpRadius + vRightRadius, 1 ); // Bottom-Right
        
        // Project the 4 corners of the sphere into clip space
        float4 vCorner0CS = mul(ViewProjection, vCorner0WS);
        float4 vCorner1CS = mul(ViewProjection, vCorner1WS);
        float4 vCorner2CS = mul(ViewProjection, vCorner2WS);
        float4 vCorner3CS = mul(ViewProjection, vCorner3WS);
        
        // Convert the corner points from clip space to normalized device coordinates
        float2 vCorner0NDC = vCorner0CS.xy / vCorner0CS.w;
        float2 vCorner1NDC = vCorner1CS.xy / vCorner1CS.w;
        float2 vCorner2NDC = vCorner2CS.xy / vCorner2CS.w;
        float2 vCorner3NDC = vCorner3CS.xy / vCorner3CS.w;
        vCorner0NDC = float2( 0.5, -0.5 ) * vCorner0NDC + float2( 0.5, 0.5 );
        vCorner1NDC = float2( 0.5, -0.5 ) * vCorner1NDC + float2( 0.5, 0.5 );
        vCorner2NDC = float2( 0.5, -0.5 ) * vCorner2NDC + float2( 0.5, 0.5 );
        vCorner3NDC = float2( 0.5, -0.5 ) * vCorner3NDC + float2( 0.5, 0.5 );
    
        // In order to have the sphere covering at most 4 textels, we need to use
        // the entire width of the rectangle, instead of only the radius of the rectangle,
        // which was the orignal implementation in the ATI paper, it had some edge case
        // failures I observed from being overly conservative.
        float fSphereWidthNDC = distance( vCorner0NDC, vCorner1NDC );
        
        // Compute the center of the bounding sphere in screen space
        float3 Cv = mul( View, float4( BoundCenterWS, 1 ) ).xyz;
        
        // compute nearest point to camera on sphere, and project it
        float3 Pv = Cv - normalize( Cv ) * BoundRadius;
        float4 ClosestSpherePoint = mul( Projection, float4( Pv, 1 ) );
    
        // Choose a MIP level in the HiZ map.
        // The orginal assumed viewport width > height, however I've changed it
        // to determine the greater of the two.
        //
        // This will result in a mip level where the object takes up at most
        // 2x2 textels such that the 4 sampled points have depths to compare
        // against.
        float W = fSphereWidthNDC * max(ViewportSize.x, ViewportSize.y);
        
        // We clamp the fLOD between 0 and 9 because if we don't we might calculate a LOD level
        // that is odd, but negative so we want to sample from mip 0, not mip 1 (but would actually
        // sample from mip 1 when the hardware sampled the closest mip to -1, or whatever the odd
        // negative number was.
        // 
        // WARNING: If the HiZ buffer ever changes in size and thus in number of mip
        // levels, the clamp range here must be updated to match.
        int fLOD = clamp(ceil(log2( W )), 0, 9);
        
        // Fetch depth samples at the corners of the square to compare against
        float4 vSamples;
        if (fLOD % 2 == 0)
        {
            vSamples.x = tex2Dlod(HiZMapEvenSampler, float4(vCorner0NDC.x, vCorner0NDC.y, 0, fLOD));
            vSamples.y = tex2Dlod(HiZMapEvenSampler, float4(vCorner1NDC.x, vCorner1NDC.y, 0, fLOD));
            vSamples.z = tex2Dlod(HiZMapEvenSampler, float4(vCorner2NDC.x, vCorner2NDC.y, 0, fLOD));
            vSamples.w = tex2Dlod(HiZMapEvenSampler, float4(vCorner3NDC.x, vCorner3NDC.y, 0, fLOD));
        }
        else
        {
            vSamples.x = tex2Dlod(HiZMapOddSampler, float4(vCorner0NDC.x, vCorner0NDC.y, 0, fLOD));
            vSamples.y = tex2Dlod(HiZMapOddSampler, float4(vCorner1NDC.x, vCorner1NDC.y, 0, fLOD));
            vSamples.z = tex2Dlod(HiZMapOddSampler, float4(vCorner2NDC.x, vCorner2NDC.y, 0, fLOD));
            vSamples.w = tex2Dlod(HiZMapOddSampler, float4(vCorner3NDC.x, vCorner3NDC.y, 0, fLOD));
        }
        
        float fMaxSampledDepth = max( max( vSamples.x, vSamples.y ), max( vSamples.z, vSamples.w ) );
        float fSphereDepth = (ClosestSpherePoint.z / ClosestSpherePoint.w);
        
        // cull sphere if the depth is greater than the largest of our ZMap values
        // or if the sphere's depth is less than 0, indicating that the object is behind us.
        return ((fSphereDepth > fMaxSampledDepth) || (ClosestSpherePoint.z < 0)) ? 0 : 1;
    }
    
    return 0;
}

technique Render
{
    pass Pass0
    {
        Sampler[0] = (HiZMapEvenSampler);
        Sampler[1] = (HiZMapOddSampler);
        
        VertexShader = compile vs_3_0 VS();
        PixelShader = compile ps_3_0 PS();
    }
}
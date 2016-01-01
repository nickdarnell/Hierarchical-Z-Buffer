#define SM4

cbuffer CB
{
    matrix View;
    matrix Projection;
    matrix ViewProjection;
    
    float4 FrustumPlanes[6];    // view-frustum planes in world space (normals face out)
    
    float2 ViewportSize;        // Viewport Width and Height in pixels
    
    float2 PADDING;
};

// Bounding sphere center (XYZ) and radius (W), world space
StructuredBuffer<float4> Buffer0    : register(t0);
// Is Visible 1 (Visible) 0 (Culled)
RWStructuredBuffer<float> BufferOut : register(u0);

Texture2D<float> HizMap    : register(t1);
SamplerState HizMapSampler : register(s0);

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

//#ifdef SM4
//#define NUM_THREADS_X 768
//#else
//#define NUM_THREADS_X 1024
//#endif
//[numthreads(NUM_THREADS_X, 1, 1)]
[numthreads(1, 1, 1)]
void CSMain( uint3 GroupId          : SV_GroupID,
             uint3 DispatchThreadId : SV_DispatchThreadID,
             uint GroupIndex        : SV_GroupIndex)
{
    // Calculate the actual index this thread in this group will be reading from.
    //int index = GroupIndex + (GroupId.x * NUM_THREADS_X);
    int index = DispatchThreadId.x;
    
    // Bounding sphere center (XYZ) and radius (W), world space
    float4 Bounds = Buffer0[index];
    
    // Perform view-frustum test
    float fVisible = CullSphere(FrustumPlanes, Bounds.xyz, Bounds.w);
    
    if (fVisible > 0)
    {
        float3 viewEye = -View._m03_m13_m23;
        float CameraSphereDistance = distance( viewEye, Bounds.xyz );
        
        float3 viewEyeSphereDirection = viewEye - Bounds.xyz;
        
        float3 viewUp = View._m01_m11_m21;
        float3 viewDirection = View._m02_m12_m22;
        float3 viewRight = normalize(cross(viewEyeSphereDirection, viewUp));
        
        // Help deal with the perspective distortion.
        // http://article.gmane.org/gmane.games.devel.algorithms/21697/
        float fRadius = CameraSphereDistance * tan(asin(Bounds.w / CameraSphereDistance));
        
        // Compute the offsets for the points around the sphere
        float3 vUpRadius = viewUp * fRadius;
        float3 vRightRadius = viewRight * fRadius;
        
        // Generate the 4 corners of the sphere in world space.
        float4 vCorner0WS = float4( Bounds.xyz + vUpRadius - vRightRadius, 1 ); // Top-Left
        float4 vCorner1WS = float4( Bounds.xyz + vUpRadius + vRightRadius, 1 ); // Top-Right
        float4 vCorner2WS = float4( Bounds.xyz - vUpRadius - vRightRadius, 1 ); // Bottom-Left
        float4 vCorner3WS = float4( Bounds.xyz - vUpRadius + vRightRadius, 1 ); // Bottom-Right
        
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
        float3 Cv = mul( View, float4( Bounds.xyz, 1 ) ).xyz;
        
        // compute nearest point to camera on sphere, and project it
        float3 Pv = Cv - normalize( Cv ) * Bounds.w;
        float4 ClosestSpherePoint = mul( Projection, float4( Pv, 1 ) );
        
        // Choose a MIP level in the HiZ map.
        // The orginal assumed viewport width > height, however I've changed it
        // to determine the greater of the two.
        //
        // This will result in a mip level where the object takes up at most
        // 2x2 textels such that the 4 sampled points have depths to compare
        // against.
        float W = fSphereWidthNDC * max(ViewportSize.x, ViewportSize.y);
        float fLOD = ceil(log2( W ));
        
        // fetch depth samples at the corners of the square to compare against
        float4 vSamples;
        vSamples.x = HizMap.SampleLevel( HizMapSampler, vCorner0NDC, fLOD );
        vSamples.y = HizMap.SampleLevel( HizMapSampler, vCorner1NDC, fLOD );
        vSamples.z = HizMap.SampleLevel( HizMapSampler, vCorner2NDC, fLOD );
        vSamples.w = HizMap.SampleLevel( HizMapSampler, vCorner3NDC, fLOD );
        
        float fMaxSampledDepth = max( max( vSamples.x, vSamples.y ), max( vSamples.z, vSamples.w ) );
        float fSphereDepth = (ClosestSpherePoint.z / ClosestSpherePoint.w);
        
        // cull sphere if the depth is greater than the largest of our ZMap values
        // or if the sphere's depth is less than 0, indicating that the object is behind us.
        BufferOut[index] = ((fSphereDepth > fMaxSampledDepth) || (ClosestSpherePoint.z < 0)) ? 0 : 1;
    }
    else
    {
        // The sphere is outside of the view frustum
        BufferOut[index] = 0;
    }
}
/* Usage:
slangc shader.slang -target spirv -capability spvBindlessTextureNV -emit-spirv-directly -fvk-use-entrypoint-name -source-embed-style u32 -source-embed-name shader_spv -o shaders.h
*/
[[vk::push_constant]] vector<uint64_t, 2> pc; /* 0: TLAS, 1: RWTexture2D<float4> */

[vk::constant_id(0)] const int WGS_X = 1;
[vk::constant_id(1)] const int WGS_Y = 1;
[shader("compute"), numthreads(WGS_X, WGS_Y, 1)]

[require(spvRayTracingKHR, spvRayQueryKHR, spvBindlessTextureNV)]
void main(
    uint3 id: SV_DispatchThreadID
) {
    let target = RWTexture2D<float4>.Handle(pc.y);
    uint2 imageDimensions;
    target.GetDimensions(imageDimensions.x, imageDimensions.y);
    if (any(id.xy >= imageDimensions)) return;
    const float WIDTH = float(imageDimensions.x);
    const float HEIGHT = float(imageDimensions.y);

    const float RATIO = WIDTH / HEIGHT;
    const float4 S = float4(-1.0, 1.0, 1.0, -1.0); // Screen: left, top, right, bottom
    const float2 PIXELSIZE = float2((S[2] - S[0]) / WIDTH, (S[3] - S[1]) / HEIGHT);
    const float2 PIXELSIZE_HALF = PIXELSIZE / 2.0;

    const float3 EYE = float3(0.0, 0.0, -1.0);
    const float SENSOR_OFFSET_Z = 0.0;
    const float3 PIXEL = float3(S[0] + PIXELSIZE.x * float(id.x) + PIXELSIZE_HALF.x, S[1] + PIXELSIZE.y * float(id.y) + PIXELSIZE_HALF.y, SENSOR_OFFSET_Z);
    const float3 DIR = normalize(PIXEL - EYE);

    RayQuery<0> rayQuery;
    RayDesc ray = { EYE, 0.001, DIR, 1000.0 };
    rayQuery.TraceRayInline(RaytracingAccelerationStructure(pc.x), RAY_FLAG_NONE, 0xFF, ray);

    while (rayQuery.Proceed()) {
        if (rayQuery.CandidateType() == CANDIDATE_NON_OPAQUE_TRIANGLE) {
            rayQuery.CommitNonOpaqueTriangleHit();
        }
    }

    target.Store(id.xy, float4(rayQuery.CommittedStatus(), 0, 0, 0));
}

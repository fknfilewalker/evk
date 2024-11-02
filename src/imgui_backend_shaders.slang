/* Usage:
slangc imgui_backend_shaders.slang -target spirv -emit-spirv-directly -fvk-use-entrypoint-name -source-embed-style u32 -source-embed-name imgui_backend_shaders_spv -o imgui_backend_shaders.h
*/
struct Vertex {
    float2 position;
    float2 uv;
    uint8_t4 color;
};

struct PushConstants
{
    Vertex* vertex;
    float2 scale;
    float2 translate;
};
[[vk::push_constant]] PushConstants pc;

struct InOut {
    float4 Position : SV_Position;
    float4 Color : COLOR;
    float2 UV : TEXCOORD;
};

[shader("vertex")]
InOut vertexMain(uint vid: SV_VertexID)
{
    InOut output;
    output.Color = pc.vertex[vid].color / 255.0;
    output.UV = pc.vertex[vid].uv;
    output.Position = float4(pc.vertex[vid].position * pc.scale + pc.translate, 0.0, 1.0);
    return output;
}

[vk::binding(0, 0)] Sampler2D sTexture;

[shader("fragment")]
float4 fragmentMain(InOut input) : SV_Target
{
    return input.Color * sTexture.Sample(input.UV);
}
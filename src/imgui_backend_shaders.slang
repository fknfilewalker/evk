/* Usage:
slangc imgui_backend_shaders.slang -target spirv -emit-spirv-directly -fvk-use-entrypoint-name -source-embed-style u32 -source-embed-name imgui_backend_shaders_spv -o imgui_backend_shaders.h
*/

struct In
{
    float2 Position : ATTRIB0;
    float2 UV : ATTRIB1;
    float4 Color : ATTRIB2;
};

struct PushConstants
{
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
InOut vertex(In input)
{
    InOut output;
    output.Color = input.Color;
    output.UV = input.UV;
    output.Position = float4(input.Position * pc.scale + pc.translate, 0.0, 1.0);
    return output;
}

[vk::binding(0, 0)] Sampler2D sTexture;

[shader("fragment")]
float4 fragment(InOut input) : SV_Target
{
    return input.Color * sTexture.Sample(input.UV);
}
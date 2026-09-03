struct VSInput
{
    float3 pos   : POSITION;
    float3 color : COLOR;
};

struct VSOutput
{
    // SV_Position is the clip-space position the rasteriser consumes.
    float4 position : SV_Position;
    float3 color    : COLOR;
};

struct PushData
{
    float4x4 transform;
    float4 tint;
};

[[vk::push_constant]]
PushData pushData;

[shader("vertex")]
VSOutput vertexMain(VSInput input)
{
    VSOutput output;
    output.position = mul(pushData.transform, float4(input.pos, 1.0));
    // Tint multiplies here, not in the fragment stage: the push constant
    // range is VERTEX_BIT only (see CreateGraphicsPipeline), so the fragment
    // stage never sees pushData at all.
    output.color = input.color * pushData.tint.rgb;
    return output;
}

[shader("fragment")]
float4 fragmentMain(VSOutput input) : SV_Target
{
    // Swapchain format is *_SRGB, so the hardware does linear -> sRGB on
    // write. Output linear values here; do not gamma-correct by hand.
    return float4(input.color, 1.0);
}

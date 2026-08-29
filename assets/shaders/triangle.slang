struct VSOutput
{
    // SV_Position is the clip-space position the rasteriser consumes.
    float4 position : SV_Position;
    float3 color    : COLOR;
};

// counter clockwise is face
static const float2 kPositions[3] = {
    float2( 0.0, -0.5),
    float2(-0.5,  0.5),
    float2( 0.5,  0.5),
};

static const float3 kColors[3] = {
    float3(1.0, 0.0, 0.0),
    float3(0.0, 1.0, 0.0),
    float3(0.0, 0.0, 1.0),
};

[shader("vertex")]
VSOutput vertexMain(uint vertexID : SV_VertexID)
{
    VSOutput output;
    output.position = float4(kPositions[vertexID], 0.0, 1.0);
    output.color = kColors[vertexID];
    return output;
}

[shader("fragment")]
float4 fragmentMain(VSOutput input) : SV_Target
{
    // Swapchain format is *_SRGB, so the hardware does linear -> sRGB on
    // write. Output linear values here; do not gamma-correct by hand.
    return float4(input.color, 1.0);
}
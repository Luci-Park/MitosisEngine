struct VSInput
{
    float3 pos    : POSITION;
    float3 color  : COLOR;
    float3 normal : NORMAL;
};

struct VSOutput
{
    float4 position : SV_Position;
    float3 color    : COLOR;
};

struct PushData
{
    float4x4 transform;
    float4 normalCol0;
    float4 normalCol1;
    float4 normalCol2;
    float4 tint;
}

[[vk::push_constant]]
PushData pushData;

[shader("vertex")]
VSOutput vertexMain(VSInput input)
{
    VSOutput output;
    output.position = mul(pushData.transform, float4(input.pos, 1.0));
    output.color = input.color * pushData.tint.rgb;
    return output;
}

[shader("fragment")]
float4 fragmentMain(VSOutput input) : SV_Target
{
    return float4(input.color, 1.0);
}

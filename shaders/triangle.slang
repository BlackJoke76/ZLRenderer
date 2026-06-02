struct VertexOutput {
    float4 position : SV_Position;
    float3 color : COLOR0;
};

[shader("vertex")]
VertexOutput vertexMain(uint vertexId : SV_VertexID)
{
    float2 positions[3] = {
        float2(0.0, -0.55),
        float2(0.55, 0.55),
        float2(-0.55, 0.55),
    };

    float3 colors[3] = {
        float3(0.95, 0.25, 0.18),
        float3(0.18, 0.72, 0.34),
        float3(0.20, 0.45, 0.95),
    };

    VertexOutput output;
    output.position = float4(positions[vertexId], 0.0, 1.0);
    output.color = colors[vertexId];
    return output;
}

[shader("fragment")]
float4 fragmentMain(VertexOutput input) : SV_Target
{
    return float4(input.color, 1.0);
}

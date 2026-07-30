struct VertexOutput {
    float4 position : SV_Position;
    float3 color : COLOR0;
};

struct VertexInput {
    float2 position : POSITION0;
    float3 color : COLOR0;
};

struct TriangleUniformData {
    float4 tint;
};

[[vk::binding(0, 0)]]
ConstantBuffer<TriangleUniformData> triangleUniforms;

[shader("vertex")]
VertexOutput vertexMain(VertexInput input)
{
    VertexOutput output;
    output.position = float4(input.position, 0.0, 1.0);
    output.color = input.color;
    return output;
}

[shader("fragment")]
float4 fragmentMain(VertexOutput input) : SV_Target
{
    return float4(input.color * triangleUniforms.tint.rgb, triangleUniforms.tint.a);
}

//--------------------------------------------------------------------------------------
// Constant Buffer Variables
//--------------------------------------------------------------------------------------
//cbuffer ConstantBuffer : register( b0 )
//{
//	  float4x4 World;
//    float4x4 View;
//    float4x4 Projection;
//    float Scalar;
//}

cbuffer ConstantBuffer : register( b0 )
{
    row_major float4x4 World;
    row_major float4x4 View;
    row_major float4x4 Projection;
    float Scalar;
}

struct VS_IN
{
    float4 Position : POSITION;
    float4 Normal : NORMAL;
    float4 Color : COLOR;
};

//--------------------------------------------------------------------------------------
struct VS_OUTPUT
{
    float4 Pos : SV_POSITION;
    float4 Color : COLOR0;
};

//--------------------------------------------------------------------------------------
// Vertex Shader
//--------------------------------------------------------------------------------------
VS_OUTPUT VS( VS_IN input )
{
    VS_OUTPUT output = (VS_OUTPUT)0;
    
    float4x4 WVP = mul(World, mul(View, Projection));
   
    // also works 
    //output.Pos = mul(input.Position, World);
    //output.Pos = mul(output.Pos, View);
    //output.Pos = mul(output.Pos, Projection);

    output.Pos = mul(input.Position, WVP);
    output.Color = input.Color * cos(Scalar);
    
    return output;
}


//--------------------------------------------------------------------------------------
// Pixel Shader
//--------------------------------------------------------------------------------------
float4 PS( VS_OUTPUT input ) : SV_Target
{
    return input.Color;
}

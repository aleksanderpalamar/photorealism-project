#ifndef PHOTOREALISM_DEPTH_VIEW_SPACE_HLSLI
#define PHOTOREALISM_DEPTH_VIEW_SPACE_HLSLI

// Fonte unica da matematica depth -> view-space -> normal.
//
// Antes da 0.12.0 estas funcoes estavam duplicadas em ssao.hlsl,
// temporal.hlsl e depth-preview.hlsl, com tres corpos que precisavam ser
// mantidos em sincronia a mao. O SSRTGI precisa exatamente das mesmas
// funcoes, e uma quarta copia seria insustentavel.
//
// Tudo aqui recebe por parametro o que antes vinha de cbuffer (NearPlane,
// ProjectionScale, TexelSize) e recebe as amostras de depth ja lidas. O
// header nao declara textura, sampler nem cbuffer: quem faz I/O e o shader,
// que sabe em quais registradores seus recursos estao. E o que permite os
// quatro shaders compartilharem o mesmo corpo sem compartilhar layout.

float3 srgb_to_linear(float3 color)
{
    color = saturate(color);
    float3 low = color / 12.92;
    float3 high = pow((color + 0.055) / 1.055, 2.4);
    float3 use_low = step(color, 0.04045.xxx);
    return lerp(high, low, use_low);
}

float3 linear_to_srgb(float3 color)
{
    color = max(color, 0.0);
    float3 low = color * 12.92;
    float3 high = 1.055 * pow(color, 1.0 / 2.4) - 0.055;
    float3 use_low = step(color, 0.0031308.xxx);
    return lerp(high, low, use_low);
}

// Modelo reversed-Z de plano distante infinito: distancia = near / depth.
float linearize_reversed_depth(float raw_depth, float near_plane)
{
    return max(near_plane, 0.000001) / max(raw_depth, 0.0000001);
}

float3 reconstruct_view_position(
    float2 uv, float linear_distance, float2 projection_scale)
{
    float2 ndc = uv * 2.0 - 1.0;
    ndc.y = -ndc.y;
    return float3(
        ndc.x * linear_distance / max(projection_scale.x, 0.000001),
        ndc.y * linear_distance / max(projection_scale.y, 0.000001),
        linear_distance);
}

// Inverso exato de reconstruct_view_position, e por isso mora ao lado dela:
// separar as duas metades da mesma transformacao em arquivos diferentes e como
// a duplicacao que esta versao veio desfazer comecou.
//
// De reconstruct_view_position temos view.x = ndc.x * z / proj.x, entao
// ndc.x = view.x * proj.x / z. O eixo vertical carrega a mesma inversao de
// sinal usada la.
float2 project_view_position(float3 view_position, float2 projection_scale)
{
    float depth = max(view_position.z, 0.000001);
    float2 ndc = float2(
        view_position.x * max(projection_scale.x, 0.000001) / depth,
        view_position.y * max(projection_scale.y, 0.000001) / depth);
    return float2((ndc.x + 1.0) * 0.5, (1.0 - ndc.y) * 0.5);
}

// As cinco amostras de depth chegam prontas porque cada shader liga a textura
// de depth em um registrador diferente. Entre o vizinho da frente e o de tras
// vence o de menor salto em Z: e o que impede a normal de atravessar uma
// silhueta e apontar para o lugar errado na borda da geometria.
//
// normal_valid sai zerado quando o produto vetorial degenera, o que acontece
// em regiao plana o suficiente para os dois vetores ficarem colineares. Sem
// essa saida o normalize produziria NaN, e o consumidor nao teria como saber.
float3 reconstruct_view_normal(
    float2 uv,
    float2 texel,
    float near_plane,
    float2 projection_scale,
    float raw_center,
    float raw_left,
    float raw_right,
    float raw_up,
    float raw_down,
    out float normal_valid)
{
    float center_distance = linearize_reversed_depth(raw_center, near_plane);
    float left_distance = linearize_reversed_depth(raw_left, near_plane);
    float right_distance = linearize_reversed_depth(raw_right, near_plane);
    float up_distance = linearize_reversed_depth(raw_up, near_plane);
    float down_distance = linearize_reversed_depth(raw_down, near_plane);

    float3 center = reconstruct_view_position(
        uv, center_distance, projection_scale);
    float3 left = reconstruct_view_position(
        uv - float2(texel.x, 0.0), left_distance, projection_scale);
    float3 right = reconstruct_view_position(
        uv + float2(texel.x, 0.0), right_distance, projection_scale);
    float3 up = reconstruct_view_position(
        uv - float2(0.0, texel.y), up_distance, projection_scale);
    float3 down = reconstruct_view_position(
        uv + float2(0.0, texel.y), down_distance, projection_scale);

    float3 horizontal_forward = right - center;
    float3 horizontal_backward = center - left;
    float3 vertical_forward = down - center;
    float3 vertical_backward = center - up;

    float3 horizontal =
        abs(horizontal_forward.z) < abs(horizontal_backward.z)
            ? horizontal_forward
            : horizontal_backward;
    float3 vertical =
        abs(vertical_forward.z) < abs(vertical_backward.z)
            ? vertical_forward
            : vertical_backward;

    float3 normal_cross = cross(horizontal, vertical);
    float normal_length_squared = dot(normal_cross, normal_cross);
    normal_valid = normal_length_squared > 0.0000000001 ? 1.0 : 0.0;
    float3 normal =
        normal_cross * rsqrt(max(normal_length_squared, 0.0000000001));
    if (normal.z > 0.0)
    {
        normal = -normal;
    }
    return normal;
}

#endif  // PHOTOREALISM_DEPTH_VIEW_SPACE_HLSLI

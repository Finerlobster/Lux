
#version 450

layout(location = 0) in vec3 fragNormal;
layout(location = 1) in vec2 fragUV;
layout(location = 2) in vec3 fragWorldPos;

layout(set = 0, binding = 1) uniform sampler2D texSampler;

layout(set = 0, binding = 2) uniform LightUBO
{
    vec4 direction;
    vec4 color;
    vec4 ambient;
} light;

layout(set = 0, binding = 0) uniform GlobalUBO
{
    mat4 model;
    mat4 view;
    mat4 projection;
    vec4 cameraPos;
} ubo;

layout(location = 0) out vec4 outColor;

void main()
{
    
    vec4 texColor = texture(texSampler, fragUV);
    
    if(texColor.a < 0.1)
        discard;
    
    vec3 normal = normalize(fragNormal);
    vec3 lightDir = normalize(-light.direction.xyz);
    vec3 viewDir = normalize(ubo.cameraPos.xyz - fragWorldPos);
    vec3 halfway = normalize(lightDir + viewDir);

    

    vec3 ambient = light.ambient.xyz * texColor.rgb;

    float diff = max(dot(normal, lightDir), 0.0);
    vec3 diffuse = diff * light.color.xyz * texColor.rgb;

    float spec = pow(max(dot(normal, halfway), 0.0), 32.0);
    vec3 specular = spec * light.color.xyz * 0.3;

    vec3 result = ambient + diffuse + specular;
    outColor = vec4(result, texColor);
}
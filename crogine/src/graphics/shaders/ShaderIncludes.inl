/*-----------------------------------------------------------------------

Matt Marchant 2017 - 2023
http://trederia.blogspot.com

crogine - Zlib license.

This software is provided 'as-is', without any express or
implied warranty.In no event will the authors be held
liable for any damages arising from the use of this software.

Permission is granted to anyone to use this software for any purpose,
including commercial applications, and to alter it and redistribute
it freely, subject to the following restrictions :

1. The origin of this software must not be misrepresented;
you must not claim that you wrote the original software.
If you use this software in a product, an acknowledgment
in the product documentation would be appreciated but
is not required.

2. Altered source versions must be plainly marked as such,
and must not be misrepresented as being the original software.

3. This notice may not be removed or altered from any
source distribution.

-----------------------------------------------------------------------*/

#pragma once

#include <string>

/*
These shader strings are partial fragments of shader code which are shared between multiple
default shaders. They are automatically registered with any ShaderResource manager, so not
only are they available to all built-in shaders, they can be used with external/custom
shaders via #include directives, as long as the shaders are loaded via a ShaderResource instance.
*/
//TODO probably need to be extern to prevent multiple copies with each ShaderResource instance

//#include WVP_UNIFORMS
static const std::string WVPMatrices =
R"(
#if defined(INSTANCING)
    uniform mat4 u_viewMatrix;
#else
    uniform mat4 u_worldViewMatrix;
    uniform mat3 u_normalMatrix;
#endif
    uniform mat4 u_worldMatrix;
    uniform mat4 u_projectionMatrix;
)";



//#include INSTANCE_ATTRIBS
static const std::string InstanceAttribs =
R"(
    ATTRIBUTE mat4 a_instanceWorldMatrix;
    ATTRIBUTE mat3 a_instanceNormalMatrix;
)";

//#include INSTANCE_MATRICES
static const std::string InstanceMatrices =
R"(
    mat4 worldMatrix = u_worldMatrix * a_instanceWorldMatrix;
    mat4 worldViewMatrix = u_viewMatrix * worldMatrix;
    mat3 normalMatrix = mat3(u_worldMatrix) * a_instanceNormalMatrix;
)";



//#include SKIN_UNIFORMS
static const std::string SkinUniforms =
R"(
    ATTRIBUTE vec4 a_boneIndices;
    ATTRIBUTE vec4 a_boneWeights;
    uniform mat4 u_boneMatrices[MAX_BONES];
)";

//#include SKIN_MATRIX
static const std::string SkinMatrix =
R"(
    mat4 skinMatrix = a_boneWeights.x * u_boneMatrices[int(a_boneIndices.x)];
    skinMatrix += a_boneWeights.y * u_boneMatrices[int(a_boneIndices.y)];
    skinMatrix += a_boneWeights.z * u_boneMatrices[int(a_boneIndices.z)];
    skinMatrix += a_boneWeights.w * u_boneMatrices[int(a_boneIndices.w)];
)";


//#include SHADOWMAP_UNIFORMS_VERT
static const std::string ShadowmapUniformsVert =
R"(
    #if !defined(MAX_CASCADES)
    #define MAX_CASCADES 3
    #endif
        uniform mat4 u_lightViewProjectionMatrix[MAX_CASCADES];
    #if defined (MOBILE)
        const int u_cascadeCount = 1;
    #else
        uniform int u_cascadeCount = 1;
    #endif
)";

//#include SHADOWMAP_OUTPUTS
static const std::string ShadowmapOutputs =
R"(
    VARYING_OUT LOW vec4 v_lightWorldPosition[MAX_CASCADES];
    VARYING_OUT float v_viewDepth;
)";

//#include SHADOWMAP_VERTEX_PROC
static const std::string ShadowmapVertProc =
R"(
    for(int i = 0; i < u_cascadeCount; i++)
    {
        v_lightWorldPosition[i] = u_lightViewProjectionMatrix[i] * worldMatrix * position;
    }
    v_viewDepth = (worldViewMatrix * position).z;
)";

//#include SHADOWMAP_UNIFORMS_FRAG
static const std::string ShadowmapUniformsFrag =
R"(
    #if !defined(MAX_CASCADES)
    #define MAX_CASCADES 3
    #endif
    uniform sampler2DArray u_shadowMap;
    uniform int u_cascadeCount = 1;
    uniform float u_frustumSplits[MAX_CASCADES];
)";

//#include SHADOWMAP_INPUTS
static const std::string ShadowmapInputs =
R"(
    VARYING_IN LOW vec4 v_lightWorldPosition[MAX_CASCADES];
    VARYING_IN float v_viewDepth;
)";

//#include PCF_SHADOWS
static const std::string PCFShadows = 
R"(
    int getCascadeIndex()
    {
        for(int i = 0; i < u_cascadeCount; ++i)
        {
            if (v_viewDepth >= u_frustumSplits[i])
            {
                return min(u_cascadeCount - 1, i);
            }
        }
        return u_cascadeCount - 1;
    }

    const vec2 kernel[16] = vec2[](
        vec2(-0.94201624, -0.39906216),
        vec2(0.94558609, -0.76890725),
        vec2(-0.094184101, -0.92938870),
        vec2(0.34495938, 0.29387760),
        vec2(-0.91588581, 0.45771432),
        vec2(-0.81544232, -0.87912464),
        vec2(-0.38277543, 0.27676845),
        vec2(0.97484398, 0.75648379),
        vec2(0.44323325, -0.97511554),
        vec2(0.53742981, -0.47373420),
        vec2(-0.26496911, -0.41893023),
        vec2(0.79197514, 0.19090188),
        vec2(-0.24188840, 0.99706507),
        vec2(-0.81409955, 0.91437590),
        vec2(0.19984126, 0.78641367),
        vec2(0.14383161, -0.14100790)
    );
    const int filterSize = 3;

#if defined(PBR)
    float shadowAmount(int cascadeIndex, SurfaceProperties surfProp)
    {
        vec4 lightWorldPos = v_lightWorldPosition[cascadeIndex];

        vec3 projectionCoords = lightWorldPos.xyz / lightWorldPos.w;
        projectionCoords = projectionCoords * 0.5 + 0.5;

        if(projectionCoords.z > 1.0) return 1.0;

        float slope = dot(surfProp.normalDir, surfProp.lightDir);

        float bias = max(0.008 * (1.0 - slope), 0.001);
        //float bias = 0.004;

        float shadow = 0.0;
        vec2 texelSize = 1.0 / textureSize(u_shadowMap, 0).xy;
        for(int x = 0; x < filterSize; ++x)
        {
            for(int y = 0; y < filterSize; ++y)
            {
                float pcfDepth = TEXTURE(u_shadowMap, vec3(projectionCoords.xy + kernel[y * filterSize + x] * texelSize, cascadeIndex)).r;
                shadow += (projectionCoords.z - bias) > pcfDepth ? 0.4 : 0.0;
            }
        }
        return 1.0 - ((shadow / 9.0) * clamp(slope, 0.0, 1.0));
    }
#else
        float shadowAmount(int cascadeIndex)
        {
            vec4 lightWorldPos = v_lightWorldPosition[cascadeIndex];

            vec3 projectionCoords = lightWorldPos.xyz / lightWorldPos.w;
            projectionCoords = projectionCoords * 0.5 + 0.5;

            if(projectionCoords.z > 1.0) return 1.0;

            float shadow = 0.0;
            vec2 texelSize = 1.0 / textureSize(u_shadowMap, 0).xy;
            for(int x = 0; x < filterSize; ++x)
            {
                for(int y = 0; y < filterSize; ++y)
                {
                    float pcfDepth = TEXTURE(u_shadowMap, vec3(projectionCoords.xy + kernel[y * filterSize + x] * texelSize, cascadeIndex)).r;
                    shadow += (projectionCoords.z - 0.001) > pcfDepth ? 0.4 : 0.0;
                }
            }
            return 1.0 - (shadow / 9.0);
        }
#endif
)";
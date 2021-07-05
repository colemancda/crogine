/*-----------------------------------------------------------------------

Matt Marchant 2017 - 2021
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

namespace cro::Shaders::Deferred
{
    static const std::string Vertex = R"(
        ATTRIBUTE vec4 a_position;
    #if defined (VERTEX_COLOUR)
        ATTRIBUTE LOW vec4 a_colour;
    #endif
        ATTRIBUTE vec3 a_normal;
    #if defined(BUMP)
        ATTRIBUTE vec3 a_tangent;
        ATTRIBUTE vec3 a_bitangent;
    #endif
    #if defined(TEXTURED)
        ATTRIBUTE vec2 a_texCoord0;
    #endif

    #if defined(SKINNED)
        ATTRIBUTE vec4 a_boneIndices;
        ATTRIBUTE vec4 a_boneWeights;
        uniform mat4 u_boneMatrices[MAX_BONES];
    #endif

        uniform mat4 u_worldMatrix;
        uniform mat4 u_worldViewMatrix;
        uniform mat3 u_normalMatrix;
        uniform mat4 u_projectionMatrix;

        uniform vec4 u_clipPlane;
        
    #if defined(BUMP)
        VARYING_OUT vec3 v_tbn[3];
    #else
        VARYING_OUT vec3 v_normal;
    #endif

    #if defined(TEXTURED)
        VARYING_OUT vec2 v_texCoord;
    #endif
    #if defined (VERTEX_COLOUR)
        VARYING_OUT vec4 v_colour;
    #endif
        VARYING_OUT vec4 v_fragPosition;

        void main()
        {
            vec4 position = a_position;

        #if defined(SKINNED)
            mat4 skinMatrix = a_boneWeights.x * u_boneMatrices[int(a_boneIndices.x)];
            skinMatrix += a_boneWeights.y * u_boneMatrices[int(a_boneIndices.y)];
            skinMatrix += a_boneWeights.z * u_boneMatrices[int(a_boneIndices.z)];
            skinMatrix += a_boneWeights.w * u_boneMatrices[int(a_boneIndices.w)];
            position = skinMatrix * position;
        #endif

            v_fragPosition = u_worldViewMatrix * position;
            gl_Position = u_projectionMatrix * v_fragPosition; //we need this to render the depth buffer

            vec3 normal = a_normal;

        #if defined(SKINNED)
            normal = (skinMatrix * vec4(normal, 0.0)).xyz;
        #endif

        #if defined (BUMP)
            vec4 tangent = vec4(a_tangent, 0.0);
            vec4 bitangent = vec4(a_bitangent, 0.0);
        #if defined (SKINNED)
            tangent = skinMatrix * tangent;
            bitangent = skinMatrix * bitangent;
        #endif
            v_tbn[0] = normalize(u_normalMatrix * tangent.xyz);
            v_tbn[1] = normalize(u_normalMatrix * bitangent.xyz);
            v_tbn[2] = normalize(u_normalMatrix * normal);
        #else
            v_normal = u_normalMatrix * normal;
        #endif

        #if defined(TEXTURED)
            v_texCoord = a_texCoord0;
        #endif

        #if defined (MOBILE)

        #else
            gl_ClipDistance[0] = dot(u_worldMatrix * position, u_clipPlane);
        #endif
        #if defined(VERTEX_COLOUR)
            v_colour = a_colour;
        #endif
        })";

    static const std::string GBufferFragment = R"(
    #if defined(DIFFUSE_MAP)
        uniform sampler2D u_diffuseMap;
    #endif
        uniform vec4 u_colour;

    #if defined(ALPHA_CLIP)
        uniform float u_alphaClip;
    #endif

    #if defined(MASK_MAP)
        uniform sampler2D u_maskMap;
    #else
        uniform vec4 u_maskColour;
    #endif

        #if defined(BUMP)
        uniform sampler2D u_normalMap;
 
        VARYING_IN vec3 v_tbn[3];
    #else
        VARYING_IN vec3 v_normal;
    #endif

    #if defined(TEXTURED)
        VARYING_IN vec2 v_texCoord;
    #endif
    #if defined (VERTEX_COLOUR)
        VARYING_IN vec4 v_colour;
    #endif
        VARYING_IN vec4 v_fragPosition;

        out vec4[4] output;

        void main()
        {
        #if defined(ALPHA_CLIP)
            if(TEXTURE(u_diffuseMap, v_texCoord).a < u_alphaClip) discard;
        #endif

        #if defined (BUMP)
            vec3 texNormal = TEXTURE(u_normalMap, v_texCoord).rgb * 2.0 - 1.0;
            vec3 normal = normalize(v_tbn[0] * texNormal.r + v_tbn[1] * texNormal.g + v_tbn[2] * texNormal.b);
        #else
            vec3 normal = normalize(v_normal);
        #endif

            vec4 colour = v_colour;
        #if defined(DIFFUSE_MAP)
            colour *= TEXTURE(u_diffuseMap, v_texCoord);
        #endif

            output[0] = vec4(normal, 1.0);
            output[1] = v_fragPosition;
            output[2] = colour;

        #if defined(MASK_MAP)
            output[3] = TEXTURE(u_maskMap, v_texCoord);
        #else
            output[3] = u_maskColour;
        #endif

        })";

    static const std::string LightingFragment = R"()";
}
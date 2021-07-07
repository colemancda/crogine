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

#include "../../detail/GLCheck.hpp"

#include <crogine/ecs/systems/DeferredRenderSystem.hpp>

#include <crogine/ecs/components/Model.hpp>
#include <crogine/ecs/components/Transform.hpp>
#include <crogine/ecs/components/GBuffer.hpp>
#include <crogine/ecs/components/Camera.hpp>

#include <crogine/ecs/systems/ModelRenderer.hpp>

#include <crogine/ecs/Scene.hpp>

#include <crogine/util/Matrix.hpp>
#include <crogine/graphics/EnvironmentMap.hpp>

#include <crogine/detail/glm/gtc/type_ptr.hpp>
#include <crogine/detail/glm/gtc/matrix_transform.hpp>
#include <crogine/detail/glm/gtc/matrix_inverse.hpp>

#include "../../graphics/shaders/Deferred.hpp"

using namespace cro;

namespace
{

    /*
    GBuffer layout:
    Channel 0 - View space normal, RGB (A) potentially revealage
    Channel 1 - View space position, RGB (A) potentially reflectivity? (SSR)
    Channel 2 - Albedo, RGBA
    Channel 3 - PBR mask, RGB(A) potentially emission
    Channel 4 - Accumulated colour for transparency, RGBA
    */

    struct TextureIndex final
    {
        enum
        {
            Normal,
            Position,
            Diffuse,
            Mask
        };
    };

    std::vector<Colour> ClearColours; /*=
    {
        cro::Colour::White,
        cro::Colour::Black,
        cro::Colour::Transparent,
        cro::Colour::Black,
        cro::Colour::Transparent
    };*/
}

DeferredRenderSystem::DeferredRenderSystem(MessageBus& mb)
    : System        (mb, typeid(DeferredRenderSystem)),
    m_cameraCount   (0),
    m_vao           (0),
    m_vbo           (0),
    m_envMap        (nullptr)
{
    requireComponent<Model>();
    requireComponent<Transform>();

    //for some reason we can't const initialise this
    //the colours all end up zeroed..?
    ClearColours =
    {
        cro::Colour::White,
        cro::Colour::Black,
        cro::Colour::Transparent,
        cro::Colour::Black,
        cro::Colour::Transparent
    };

    setupRenderQuad();
}

DeferredRenderSystem::~DeferredRenderSystem()
{
#ifdef PLATFORM_DESKTOP
    if (m_vbo)
    {
        glCheck(glDeleteBuffers(1, &m_vbo));
    }

    if (m_vao)
    {
        glCheck(glDeleteVertexArrays(1, &m_vao));
    }
#endif
}

//public
void DeferredRenderSystem::updateDrawList(Entity camera)
{
    auto& cam = camera.getComponent<Camera>();
    if (!cam.active)
    {
        return;
    }
    CRO_ASSERT(camera.hasComponent<GBuffer>(), "Needs a GBuffer component to render");

    auto cameraPos = camera.getComponent<Transform>().getWorldPosition();
    auto& entities = getEntities();

    if (m_visibleLists.size() == m_cameraCount)
    {
        m_visibleLists.emplace_back();
    }
    auto& [deferred, forward] = m_visibleLists[m_cameraCount];
    deferred.clear(); //TODO if we had a dirty flag on the cam transform we could only update this if the camera moved...
    forward.clear();

    for (auto& entity : entities)
    {
        auto& model = entity.getComponent<Model>();
        if (model.isHidden())
        {
            continue;
        }

        //cull entities from frustum - TODO using a dynamic tree could improve this
        //on bigger scenes
        auto sphere = model.m_meshData.boundingSphere;
        const auto& tx = entity.getComponent<Transform>();

        sphere.centre = glm::vec3(tx.getWorldTransform() * glm::vec4(sphere.centre, 1.f));
        auto scale = tx.getScale();
        sphere.radius *= ((scale.x + scale.y + scale.z) / 3.f);

        //TODO we *could* do every camera pass - but for now we're
        //expecting screen space reflections rather than using camera reflection mapping

        //for(pass in cam.passes)
        //{
            const auto& frustum = cam.getPass(Camera::Pass::Final).getFrustum();
            auto forwardVector = cro::Util::Matrix::getForwardVector(cam.getPass(Camera::Pass::Final).viewMatrix);

            model.m_visible = true;
            std::size_t i = 0;
            while (model.m_visible && i < frustum.size())
            {
                model.m_visible = (Spatial::intersects(frustum[i++], sphere) != Planar::Back);
            }

            if (model.m_visible)
            {
                auto direction = (sphere.centre - cameraPos);
                float distance = glm::dot(forwardVector, direction);

                SortData d;
                d.entity = entity;
                d.distanceFromCamera = distance;

                SortData f;
                f.entity = entity;
                d.distanceFromCamera = distance;

                //TODO a large model with a centre behind the camera
                //might still intersect the view but register as being
                //further away than smaller objects in front

                //foreach material
                //add ent/index pair to deferred or forward list
                for (i = 0u; i < model.m_meshData.submeshCount; ++i)
                {
                    if (!model.m_materials[Mesh::IndexData::Final][i].deferred)
                    {
                        f.materialIDs.push_back(static_cast<std::int32_t>(i));
                    }
                    else
                    {
                        d.materialIDs.push_back(static_cast<std::int32_t>(i));
                    }
                }

                //store the entity in the appropriate list
                //if it has any materials associated with it
                if (!d.materialIDs.empty())
                {
                    deferred.push_back(d);
                }

                if (!f.materialIDs.empty())
                {
                    forward.push_back(f);
                }
            }
        //}
    }
    
    //sort deferred list by distance to Camera.
    std::sort(deferred.begin(), deferred.end(),
        [](const SortData& a, const SortData& b)
        {
            return a.distanceFromCamera < b.distanceFromCamera;
        });

    DPRINT("Deferred ents: ", std::to_string(deferred.size()));
    DPRINT("Forward ents: ", std::to_string(forward.size()));

    cam.getDrawList(Camera::Pass::Final)[getType()] = std::make_any<std::uint32_t>(m_cameraCount);
    m_cameraCount++;
}

void DeferredRenderSystem::render(Entity camera, const RenderTarget& rt)
{
#ifdef PLATFORM_DESKTOP
    const auto& cam = camera.getComponent<Camera>();
    const auto& pass = cam.getPass(Camera::Pass::Final);

    auto listID = std::any_cast<std::uint32_t>(pass.drawList.at(getType()));

    const auto& [deferred, forward] = m_visibleLists[listID];

    glm::vec4 clipPlane = glm::vec4(0.f, 1.f, 0.f, -getScene()->getWaterLevel() + (0.08f * pass.getClipPlaneMultiplier())) * pass.getClipPlaneMultiplier();

    const auto& camTx = camera.getComponent<Transform>();
    auto cameraPosition = camTx.getWorldPosition();
    auto screenSize = glm::vec2(rt.getSize());

    glCheck(glCullFace(pass.getCullFace()));
    glCheck(glEnable(GL_CULL_FACE));
    glCheck(glEnable(GL_DEPTH_TEST));
    glCheck(glDisable(GL_BLEND));

    //render deferred to GBuffer
    auto& buffer = camera.getComponent<GBuffer>().buffer;
    buffer.clear(ClearColours);

    for (auto [entity, matIDs, depth] : deferred)
    {
        //foreach submesh / material:
        const auto& model = entity.getComponent<Model>();

        if ((model.m_renderFlags & cam.renderFlags) == 0)
        {
            continue;
        }

        //calc entity transform
        const auto& tx = entity.getComponent<Transform>();
        glm::mat4 worldMat = tx.getWorldTransform();
        glm::mat4 worldView = pass.viewMatrix * worldMat;

        for (auto i : matIDs)
        {
            //bind shader
            glCheck(glUseProgram(model.m_materials[Mesh::IndexData::Final][i].shader));

            //apply shader uniforms from material
            glCheck(glUniformMatrix4fv(model.m_materials[Mesh::IndexData::Final][i].uniforms[Material::WorldView], 1, GL_FALSE, glm::value_ptr(worldView)));
            ModelRenderer::applyProperties(model.m_materials[Mesh::IndexData::Final][i], model, *getScene(), cam);

            //apply standard uniforms - TODO check all these are necessary
            //glCheck(glUniform3f(model.m_materials[Mesh::IndexData::Final][i].uniforms[Material::Camera], cameraPosition.x, cameraPosition.y, cameraPosition.z));
            //glCheck(glUniform2f(model.m_materials[Mesh::IndexData::Final][i].uniforms[Material::ScreenSize], screenSize.x, screenSize.y));
            glCheck(glUniform4f(model.m_materials[Mesh::IndexData::Final][i].uniforms[Material::ClipPlane], clipPlane[0], clipPlane[1], clipPlane[2], clipPlane[3]));
            glCheck(glUniformMatrix4fv(model.m_materials[Mesh::IndexData::Final][i].uniforms[Material::View], 1, GL_FALSE, glm::value_ptr(pass.viewMatrix)));
            glCheck(glUniformMatrix4fv(model.m_materials[Mesh::IndexData::Final][i].uniforms[Material::ViewProjection], 1, GL_FALSE, glm::value_ptr(pass.viewProjectionMatrix)));
            glCheck(glUniformMatrix4fv(model.m_materials[Mesh::IndexData::Final][i].uniforms[Material::Projection], 1, GL_FALSE, glm::value_ptr(cam.getProjectionMatrix())));
            glCheck(glUniformMatrix4fv(model.m_materials[Mesh::IndexData::Final][i].uniforms[Material::World], 1, GL_FALSE, glm::value_ptr(worldMat)));
            glCheck(glUniformMatrix3fv(model.m_materials[Mesh::IndexData::Final][i].uniforms[Material::Normal], 1, GL_FALSE, glm::value_ptr(glm::inverseTranspose(glm::mat3(worldView)))));

            //check for depth test override
            if (!model.m_materials[Mesh::IndexData::Final][i].enableDepthTest)
            {
                glCheck(glDisable(GL_DEPTH_TEST));
            }

            const auto& indexData = model.m_meshData.indexData[i];
            glCheck(glBindVertexArray(indexData.vao[Mesh::IndexData::Final]));
            glCheck(glDrawElements(static_cast<GLenum>(indexData.primitiveType), indexData.indexCount, static_cast<GLenum>(indexData.format), 0));
        }
    }


    //TODO render forward/transparent items to GBuffer (inc normals/position for screen space effects)
    glCheck(glDisable(GL_CULL_FACE));
    glCheck(glEnable(GL_BLEND)); //TODO set correct blend mode for individual buffers

    buffer.display();


    //TODO optionally calc screen space effects (ao, reflections etc)



    //PBR lighting pass of deferred items to render target
    //blend alpha so background shows through
    glCheck(glDepthMask(GL_FALSE));
    glCheck(glEnable(GL_CULL_FACE));
    glCheck(glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA));
    glCheck(glBlendEquation(GL_FUNC_ADD));

    CRO_ASSERT(m_envMap, "Env map not set!");

    //TODO can we cache these?
    glm::vec2 size(rt.getSize());
    size.x *= cam.viewport.width;
    size.y *= cam.viewport.height;
    glm::mat4 transform = glm::scale(glm::mat4(1.f), { size.x, size.y, 0.f });
    //transform = glm::translate(transform, { left, bottom, 0.f });
    glm::mat4 projection = glm::ortho(0.f, size.x, 0.f, size.y, -0.1f, 1.f);

    glCheck(glUseProgram(m_pbrShader.getGLHandle()));
    glCheck(glUniformMatrix4fv(m_pbrUniforms[PBRUniformIDs::WorldMat], 1, GL_FALSE, glm::value_ptr(transform)));
    glCheck(glUniformMatrix4fv(m_pbrUniforms[PBRUniformIDs::ProjMat], 1, GL_FALSE, glm::value_ptr(projection)));

    glCheck(glActiveTexture(GL_TEXTURE0));
    glCheck(glBindTexture(GL_TEXTURE_2D, buffer.getTexture(TextureIndex::Diffuse).textureID));

    glCheck(glActiveTexture(GL_TEXTURE1));
    glCheck(glBindTexture(GL_TEXTURE_2D, buffer.getTexture(TextureIndex::Mask).textureID));

    glCheck(glActiveTexture(GL_TEXTURE2));
    glCheck(glBindTexture(GL_TEXTURE_2D, buffer.getTexture(TextureIndex::Normal).textureID));

    glCheck(glActiveTexture(GL_TEXTURE3));
    glCheck(glBindTexture(GL_TEXTURE_2D, buffer.getTexture(TextureIndex::Position).textureID));

    glCheck(glUniform1i(m_pbrUniforms[PBRUniformIDs::Diffuse], 0));
    glCheck(glUniform1i(m_pbrUniforms[PBRUniformIDs::Mask], 1));
    glCheck(glUniform1i(m_pbrUniforms[PBRUniformIDs::Normal], 2));
    glCheck(glUniform1i(m_pbrUniforms[PBRUniformIDs::Position], 3));



    glCheck(glBindVertexArray(m_vao));
    glCheck(glDrawArrays(GL_TRIANGLE_STRIP, 0, 4));


    //TODO Transparent pass to render target


    //just reset the camera count - it's only
    //used to track how many visible lists we have
    m_cameraCount = 0;

    glCheck(glBindVertexArray(0));
    glCheck(glUseProgram(0));

    glCheck(glDisable(GL_DEPTH_TEST));
    glCheck(glDepthMask(GL_TRUE));
#endif
}

void DeferredRenderSystem::setEnvironmentMap(const EnvironmentMap& map)
{
    m_envMap = &map;
}

//private
bool DeferredRenderSystem::loadPBRShader()
{
    std::fill(m_pbrUniforms.begin(), m_pbrUniforms.end(), -1);

    auto result = m_pbrShader.loadFromString(Shaders::Deferred::LightingVertex, Shaders::Deferred::LightingFragment);
    if (result)
    {
        //load uniform mappings
        const auto& uniforms = m_pbrShader.getUniformMap();
        m_pbrUniforms[PBRUniformIDs::WorldMat] = uniforms.at("u_worldMatrix");
        m_pbrUniforms[PBRUniformIDs::ProjMat] = uniforms.at("u_projectionMatrix");

        if (uniforms.count("u_diffuseMap"))
        {
            m_pbrUniforms[PBRUniformIDs::Diffuse] = uniforms.at("u_diffuseMap");
        }
        if (uniforms.count("u_maskMap"))
        {
            m_pbrUniforms[PBRUniformIDs::Mask] = uniforms.at("u_maskMap");
        }
        if (uniforms.count("u_normalMap"))
        {
            m_pbrUniforms[PBRUniformIDs::Normal] = uniforms.at("u_normalMap");
        }
        if (uniforms.count("u_positionMap"))
        {
            m_pbrUniforms[PBRUniformIDs::Position] = uniforms.at("u_positionMap");
        }
    }

    return result;
}

bool DeferredRenderSystem::loadOITShader()
{
    return true;
}

void DeferredRenderSystem::setupRenderQuad()
{
#ifdef PLATFORM_DESKTOP
    if (loadPBRShader() && loadOITShader())
    {
        glCheck(glGenBuffers(1, &m_vbo));
        glCheck(glGenVertexArrays(1, &m_vao));

        //0------2
        //|      |
        //|      |
        //1------3

        std::vector<float> verts =
        {
            0.f, 1.f, //0.f, 1.f,
            0.f, 0.f,// 0.f, 0.f,
            1.f, 1.f, //1.f, 1.f,
            1.f, 0.f, //1.f, 0.f
        };
        static constexpr std::uint32_t VertexSize = 2 * sizeof(float);

        glCheck(glBindVertexArray(m_vao));
        glCheck(glBindBuffer(GL_ARRAY_BUFFER, m_vbo));
        glCheck(glBufferData(GL_ARRAY_BUFFER, VertexSize * verts.size(), verts.data(), GL_STATIC_DRAW));

        const auto& attribs = m_pbrShader.getAttribMap();
        glCheck(glEnableVertexAttribArray(attribs[Mesh::Attribute::Position]));
        glCheck(glVertexAttribPointer(attribs[Mesh::Attribute::Position], 2, GL_FLOAT, GL_FALSE, static_cast<GLsizei>(VertexSize), reinterpret_cast<void*>(static_cast<intptr_t>(0))));
        glCheck(glDisableVertexAttribArray(attribs[Mesh::Attribute::Position]));
        glCheck(glEnableVertexAttribArray(0));

        glCheck(glBindVertexArray(0));
        glCheck(glBindBuffer(GL_ARRAY_BUFFER, 0));
    }
#endif
}
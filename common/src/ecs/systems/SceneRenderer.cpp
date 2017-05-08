/*-----------------------------------------------------------------------

Matt Marchant 2017
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

#include <crogine/ecs/systems/SceneRenderer.hpp>
#include <crogine/detail/Assert.hpp>
#include <crogine/ecs/components/Camera.hpp>
#include <crogine/ecs/components/Transform.hpp>
#include <crogine/ecs/components/Model.hpp>
#include <crogine/core/Clock.hpp>

#include "../../detail/GLCheck.hpp"

#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/matrix_inverse.hpp>

using namespace cro;

SceneRenderer::SceneRenderer(MessageBus& mb, Entity camera)
    : System            (mb, typeid(SceneRenderer)),
    m_activeCamera      (camera),
    m_currentTextureUnit(0)
{
    CRO_ASSERT(camera.hasComponent<Camera>() && camera.hasComponent<Transform>(), "Invalid camera Entity");
    requireComponent<Transform>();
    //requireComponent<Model>();
}

//public
Entity SceneRenderer::setActiveCamera(Entity camera)
{
    CRO_ASSERT(camera.hasComponent<Camera>() && camera.hasComponent<Transform>(), "Invalid camera Entity");
    auto oldCam = m_activeCamera;
    m_activeCamera = camera;
    return oldCam;
}

Entity SceneRenderer::getActiveCamera() const
{
    return m_activeCamera;
}

void SceneRenderer::setDrawableList(MaterialList& entities)
{
    m_visibleEntities.swap(entities);
    entities.clear();
}

void SceneRenderer::render()
{
    //glCheck(glEnable(GL_DEPTH_TEST));
    //glCheck(glEnable(GL_CULL_FACE));
    //glCheck(glEnable(GL_BLEND));

    auto cameraPosition = glm::vec3(m_activeCamera.getComponent<Transform>().getWorldTransform()[3]);
    auto viewMat = glm::inverse(m_activeCamera.getComponent<Transform>().getWorldTransform());
    auto projMat = m_activeCamera.getComponent<Camera>().projection;

    for (auto& e : m_visibleEntities)
    {
        //calc entity transform
        const auto& tx = e.first.getComponent<Transform>();
        glm::mat4 worldMat = tx.getWorldTransform();
        glm::mat4 worldView = viewMat * worldMat;

        //foreach submesh / material:
        const auto& model = e.first.getComponent<Model>();
        glCheck(glBindBuffer(GL_ARRAY_BUFFER, model.m_meshData.vbo));
        //for (auto i = 0u; i < model.m_meshData.submeshCount; ++i)
        for(auto i : e.second.matIDs)
        {
            //bind shader
            glCheck(glUseProgram(model.m_materials[i].shader));

            //apply shader uniforms from material
            glCheck(glUniformMatrix4fv(model.m_materials[i].uniforms[Material::WorldView], 1, GL_FALSE, glm::value_ptr(worldView)));
            applyProperties(model.m_materials[i].properties);

            //apply standard uniforms
            glCheck(glUniform3f(model.m_materials[i].uniforms[Material::Camera], cameraPosition.x, cameraPosition.y, cameraPosition.z));
            glCheck(glUniformMatrix4fv(model.m_materials[i].uniforms[Material::Projection], 1, GL_FALSE, glm::value_ptr(projMat)));
            glCheck(glUniformMatrix4fv(model.m_materials[i].uniforms[Material::World], 1, GL_FALSE, glm::value_ptr(worldMat)));
            glCheck(glUniformMatrix3fv(model.m_materials[i].uniforms[Material::Normal], 1, GL_FALSE, glm::value_ptr(glm::inverseTranspose(glm::mat3(worldMat)))));

            //TODO set material winding/cullface/depthfunc

            applyBlendMode(model.m_materials[i].blendMode);

            //bind attribs
            const auto& attribs = model.m_materials[i].attribs;
            for (auto j = 0u; j < model.m_materials[i].attribCount; ++j)
            {
                glCheck(glEnableVertexAttribArray(attribs[j][Material::Data::Index]));
                glCheck(glVertexAttribPointer(attribs[j][Material::Data::Index], attribs[j][Material::Data::Size],
                    GL_FLOAT, GL_FALSE, static_cast<GLsizei>(model.m_meshData.vertexSize),
                    reinterpret_cast<void*>(static_cast<intptr_t>(attribs[j][Material::Data::Offset]))));               
            }

            //bind element/index buffer
            const auto& indexData = model.m_meshData.indexData[i];
            glCheck(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, indexData.ibo));

            //draw elements
            glCheck(glDrawElements(static_cast<GLenum>(indexData.primitiveType), indexData.indexCount, static_cast<GLenum>(indexData.format), 0));

            glCheck(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0));

            //unbind attribs
            for (auto j = 0u; j < model.m_materials[i].attribCount; ++j)
            {
                glCheck(glDisableVertexAttribArray(attribs[j][Material::Data::Index]));
            }
        }
        //glCheck(glBindBuffer(GL_ARRAY_BUFFER, 0));
        glCheck(glUseProgram(0));
        /*for (auto i = 0; i < m_currentTextureUnit; ++i)
        {
            glCheck(glActiveTexture(GL_TEXTURE0 + i));
            glCheck(glBindTexture(GL_TEXTURE_2D, 0));
        }*/
    }

    glCheck(glBindBuffer(GL_ARRAY_BUFFER, 0));

    glCheck(glDisable(GL_BLEND));
    glCheck(glDisable(GL_CULL_FACE));
    glCheck(glDisable(GL_DEPTH_TEST));
    glCheck(glDepthMask(GL_TRUE)); //restore this else clearing the depth buffer fails
}

//private
void SceneRenderer::applyProperties(const Material::PropertyList& properties)
{
    m_currentTextureUnit = 0;
    for (const auto& prop : properties)
    {
        switch (prop.second.second.type)
        {
        default: break;
        case Material::Property::Texture:
            //TODO track the current tex ID bound to this unit and only bind if different
            glCheck(glActiveTexture(GL_TEXTURE0 + m_currentTextureUnit));
            glCheck(glBindTexture(GL_TEXTURE_2D, prop.second.second.textureID));
            glCheck(glUniform1i(prop.second.first, m_currentTextureUnit++));
            break;
        case Material::Property::Number:
            glCheck(glUniform1f(prop.second.first,
                prop.second.second.numberValue));
            break;
        case Material::Property::Vec2:
            glCheck(glUniform2f(prop.second.first, 
                prop.second.second.vecValue[0],
                prop.second.second.vecValue[1]));
            break;
        case Material::Property::Vec3:
            glCheck(glUniform3f(prop.second.first, prop.second.second.vecValue[0],
                prop.second.second.vecValue[1], prop.second.second.vecValue[2]));
            break;
        case Material::Property::Vec4:
            glCheck(glUniform4f(prop.second.first, prop.second.second.vecValue[0],
                prop.second.second.vecValue[1], prop.second.second.vecValue[2], prop.second.second.vecValue[3]));
            break;
        }
    }
}

void SceneRenderer::applyBlendMode(Material::BlendMode mode)
{
    switch (mode)
    {
    default: break;
    case Material::BlendMode::Additive:
        glCheck(glEnable(GL_BLEND));
        glCheck(glEnable(GL_DEPTH_TEST));
        glCheck(glDepthMask(GL_TRUE));
        glCheck(glEnable(GL_CULL_FACE));
        glCheck(glBlendFunc(GL_ONE, GL_ONE));
        glCheck(glBlendEquation(GL_FUNC_ADD));
        break;
    case Material::BlendMode::Alpha:
        glCheck(glDisable(GL_CULL_FACE));
        //glCheck(glDisable(GL_DEPTH_TEST));
        glCheck(glDepthMask(GL_FALSE));
        glCheck(glEnable(GL_BLEND));
        glCheck(glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA));
        glCheck(glBlendEquation(GL_FUNC_ADD));
        break;
    case Material::BlendMode::Multiply:
        glCheck(glEnable(GL_BLEND));
        glCheck(glEnable(GL_DEPTH_TEST));
        glCheck(glDepthMask(GL_TRUE));
        glCheck(glEnable(GL_CULL_FACE));
        glCheck(glBlendFunc(GL_DST_COLOR, GL_ZERO));
        glCheck(glBlendEquation(GL_FUNC_ADD));
        break;
    case Material::BlendMode::None:
        glCheck(glEnable(GL_DEPTH_TEST));
        glCheck(glDepthMask(GL_TRUE));
        glCheck(glEnable(GL_CULL_FACE));
        glCheck(glDisable(GL_BLEND));
        break;
    }
}

/*-----------------------------------------------------------------------

Matt Marchant 2017 - 2020
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

#include <crogine/ecs/systems/ParticleSystem.hpp>
#include <crogine/ecs/components/ParticleEmitter.hpp>
#include <crogine/ecs/components/Transform.hpp>
#include <crogine/ecs/components/Camera.hpp>
#include <crogine/ecs/Scene.hpp>
#include <crogine/graphics/MeshData.hpp>
#include <crogine/core/Clock.hpp>
#include <crogine/core/App.hpp>
#include <crogine/core/Console.hpp>
#include <crogine/util/Random.hpp>
#include <crogine/util/Constants.hpp>

#include "../../detail/GLCheck.hpp"

#include <crogine/detail/glm/gtc/type_ptr.hpp>

//why do I have to hack this? there has to be a catch...
#ifdef PLATFORM_DESKTOP
#ifndef GL_PROGRAM_POINT_SIZE
#define GL_PROGRAM_POINT_SIZE 34370
#endif //GL_PROGRAM_POINT_SIZE
#define GL_POINT_SPRITE 34913
#define ENABLE_POINT_SPRITES  glCheck(glEnable(GL_PROGRAM_POINT_SIZE)); glCheck(glEnable(GL_POINT_SPRITE))
#define DISABLE_POINT_SPRITES glCheck(glDisable(GL_PROGRAM_POINT_SIZE)); glCheck(glDisable(GL_POINT_SPRITE))
#else
#define ENABLE_POINT_SPRITES
#define DISABLE_POINT_SPRITES
#endif // PLATFORM_DESKTOP

using namespace cro;

namespace
{
    const std::string vertex = R"(
        ATTRIBUTE vec4 a_position;
        ATTRIBUTE LOW vec4 a_colour;
        ATTRIBUTE MED vec3 a_normal; //this actually stores rotation and scale

        uniform mat4 u_projection;
        uniform mat4 u_viewProjection;
        uniform LOW float u_viewportHeight;
        uniform LOW float u_particleSize;

#if defined(MOBILE)

#else
        uniform vec4 u_clipPlane;
#endif

        VARYING_OUT LOW vec4 v_colour;
        VARYING_OUT MED mat2 v_rotation;

        void main()
        {
            v_colour = a_colour;

            vec2 rot = vec2(sin(a_normal.x), cos(a_normal.x));
            v_rotation[0] = vec2(rot.y, -rot.x);
            v_rotation[1]= rot;

            gl_Position = u_viewProjection * a_position;
            gl_PointSize = u_viewportHeight * u_projection[1][1] / gl_Position.w * u_particleSize * a_normal.y;

#if defined (MOBILE)

#else
            gl_ClipDistance[0] = dot(a_position, u_clipPlane);
#endif

        }
    )";

    const std::string fragment = R"(
        uniform sampler2D u_texture;

        VARYING_IN LOW vec4 v_colour;
        VARYING_IN MED mat2 v_rotation;
        OUTPUT

        void main()
        {
            vec2 texCoord = v_rotation * (gl_PointCoord - vec2(0.5));
            FRAG_OUT = v_colour * TEXTURE(u_texture, texCoord + vec2(0.5)) * v_colour.a;
        }
    )";

    constexpr std::size_t MaxVertData = ParticleEmitter::MaxParticles * (3 + 4 + 3); //pos, colour, rotation/scale vert attribs
    const std::size_t MaxParticleSystems = 128; //max number of VBOs - must be divisible by min count
    const std::size_t MinParticleSystems = 4; //min amount before resizing - this many added on resize (so don't make too large!!)
    const std::size_t VertexSize = 10 * sizeof(float); //pos, colour, rotation/scale vert attribs
}

ParticleSystem::ParticleSystem(MessageBus& mb)
    : System            (mb, typeid(ParticleSystem)),
    m_dataBuffer        (MaxVertData),
    m_vboIDs            (MaxParticleSystems),
    m_vaoIDs            (MaxParticleSystems),
    m_nextBuffer        (0),
    m_bufferCount       (0),
    m_clipPlaneUniform  (-1),
    m_projectionUniform (-1),
    m_textureUniform    (-1),
    m_viewProjUniform   (-1),
    m_viewportUniform   (-1),
    m_sizeUniform       (-1)
{
    for (auto& vbo : m_vboIDs)
    {
        vbo = 0;
    }

    //required for core profile on desktop
    for (auto& vao : m_vaoIDs)
    {
        vao = 0;
    }

    requireComponent<Transform>();
    requireComponent<ParticleEmitter>();

    if (!m_shader.loadFromString(vertex, fragment))
    {
        Logger::log("Failed to compile Particle shader", Logger::Type::Error);
    }
    else
    {
        //fetch uniforms.
        const auto& uniforms = m_shader.getUniformMap();
#ifdef PLATFORM_DESKTOP
        m_clipPlaneUniform = uniforms.find("u_clipPlane")->second;
#endif
        m_projectionUniform = uniforms.find("u_projection")->second;
        m_textureUniform = uniforms.find("u_texture")->second;
        m_viewProjUniform = uniforms.find("u_viewProjection")->second;
        m_viewportUniform = uniforms.find("u_viewportHeight")->second;
        m_sizeUniform = uniforms.find("u_particleSize")->second;

        //map attributes
        const auto& attribMap = m_shader.getAttribMap();
        m_attribData[0].index = attribMap[Mesh::Position];
        m_attribData[0].attribSize = 3;
        m_attribData[0].offset = 0;

        m_attribData[1].index = attribMap[Mesh::Colour];
        m_attribData[1].attribSize = 4;
        m_attribData[1].offset = 3 * sizeof(float);

        m_attribData[2].index = attribMap[Mesh::Normal]; //actually rotation/scale just using the existing naming convention
        m_attribData[2].attribSize = 3;
        m_attribData[2].offset = (3 + 4) * sizeof(float);
    }
}

ParticleSystem::~ParticleSystem()
{
    //delete VBOs
    for (auto vbo : m_vboIDs)
    {
        if (vbo)
        {
            glCheck(glDeleteBuffers(1, &vbo));
        }
    }
#ifdef PLATFORM_DESKTOP
    for (auto vao : m_vaoIDs)
    {
        glCheck(glDeleteVertexArrays(1, &vao));
    }

#endif
}

//public
void ParticleSystem::updateDrawList(Entity cameraEnt)
{
    auto& cam = cameraEnt.getComponent<Camera>();

    for (auto& visible : m_visibleEntities)
    {
        visible.clear();
    }

    const auto& entities = getEntities();
    for (auto entity : entities)
    {
        const auto& emitter = entity.getComponent<ParticleEmitter>();
        auto inView = [&emitter](const Frustum& frustum)->bool
        {
            bool visible = true;
            std::size_t i = 0;
            while (visible && i < frustum.size())
            {
                visible = (Spatial::intersects(frustum[i++], emitter.m_bounds) != Planar::Back);
            }
            return visible;
        };

        for (auto i = 0u; i < m_visibleEntities.size(); ++i)
        {
            const auto& frustum = cam.getPass(i).getFrustum();
            if (emitter.m_nextFreeParticle > 0 && inView(frustum))
            {
                m_visibleEntities[i].push_back(entity);
            }
        }
    }

    DPRINT("Visible particle Systems", std::to_string(m_visibleEntities[0].size()));

    for (auto i = 0u; i < m_visibleEntities.size(); ++i)
    {
        //TODO find a way to swap this buffer rather than copy each time.
        cam.getDrawList(i)[getType()] = std::make_any<std::vector<Entity>>(m_visibleEntities[i]);
    }
}

void ParticleSystem::process(float dt)
{
    auto& entities = getEntities();
    for (auto& e : entities)
    {
        //TODO we're not anticipating large worlds, but it might be worth
        //skipping emitter updates that are a long way from the camera.
        //this if course also means per camera updates, and therefore suddenly
        //gets complicated....

        //check each emitter to see if it should spawn a new particle
        auto& emitter = e.getComponent<ParticleEmitter>();
        if (emitter.m_running &&
            emitter.m_emissionClock.elapsed().asSeconds() > (1.f / emitter.settings.emitRate))
        {
            emitter.m_emissionClock.restart();
            static const float epsilon = 0.0001f;
            if (emitter.m_nextFreeParticle < emitter.m_particles.size() - 1)
            {
                auto& tx = e.getComponent<Transform>();
                glm::quat rotation = glm::quat_cast(tx.getLocalTransform());

                const auto& settings = emitter.settings;
                CRO_ASSERT(settings.emitRate > 0, "Emit rate must be grater than 0");
                CRO_ASSERT(settings.lifetime > 0, "Lifetime must be greater than 0");
                auto& p = emitter.m_particles[emitter.m_nextFreeParticle];
                p.colour = settings.colour;
                p.gravity = settings.gravity;
                p.lifetime = settings.lifetime + cro::Util::Random::value(-settings.lifetimeVariance, settings.lifetimeVariance + epsilon);
                p.maxLifeTime = p.lifetime;
                p.velocity = rotation * settings.initialVelocity;
                p.rotation = Util::Random::value(-Util::Const::TAU, Util::Const::TAU);
                p.scale = 1.f;
                p.acceleration = settings.acceleration;

                //spawn particle in world position
                p.position = tx.getWorldPosition();
                
                //add random radius placement - TODO how to do with a position table? CAN'T HAVE +- 0!!
                p.position.x += Util::Random::value(-settings.spawnRadius, settings.spawnRadius + epsilon);
                p.position.y += Util::Random::value(-settings.spawnRadius, settings.spawnRadius + epsilon);
                p.position.z += Util::Random::value(-settings.spawnRadius, settings.spawnRadius + epsilon);

                auto offset = settings.spawnOffset;
                offset *= tx.getScale();
                p.position += offset;

                emitter.m_nextFreeParticle++;
                if (emitter.m_releaseCount > 0)
                {
                    emitter.m_releaseCount--;
                }
            }
        }

        if (emitter.m_releaseCount == 0)
        {
            emitter.stop();
        }

        //update each particle
        glm::vec3 minBounds(std::numeric_limits<float>::max());
        glm::vec3 maxBounds(0.f);
        for (auto i = 0u; i < emitter.m_nextFreeParticle; ++i)
        {
            auto& p = emitter.m_particles[i];

            p.velocity += p.gravity * dt;
            for (auto f : emitter.settings.forces)
            {
                p.velocity += f * dt;
            }
            p.position += p.velocity * dt;            
           
            p.lifetime -= dt;
            p.colour.setAlpha(std::max(p.lifetime / p.maxLifeTime, 0.f));

            p.rotation += emitter.settings.rotationSpeed * dt;
            p.scale += ((p.scale * emitter.settings.scaleModifier) * dt);
            //LOG(std::to_string(emitter.settings.scaleModifier), Logger::Type::Info);

            //update bounds for culling
            if (p.position.x < minBounds.x) minBounds.x = p.position.x;
            if (p.position.y < minBounds.y) minBounds.y = p.position.y;
            if (p.position.z < minBounds.z) minBounds.z = p.position.z;

            if (p.position.x > maxBounds.x) maxBounds.x = p.position.x;
            if (p.position.y > maxBounds.y) maxBounds.y = p.position.y;
            if (p.position.z > maxBounds.z) maxBounds.z = p.position.z;
        }
        auto dist = (maxBounds - minBounds) / 2.f;
        emitter.m_bounds.centre = dist + minBounds;
        emitter.m_bounds.radius = glm::length(dist);

        //go over again and remove dead particles with pop/swap
        for (auto i = 0u; i < emitter.m_nextFreeParticle; ++i)
        {
            if (emitter.m_particles[i].lifetime < 0)
            {
                emitter.m_nextFreeParticle--;
                std::swap(emitter.m_particles[i], emitter.m_particles[emitter.m_nextFreeParticle]);                
            }
        }
        //DPRINT("Next free Particle", std::to_string(emitter.m_nextFreeParticle));

        //TODO sort verts by depth? should be drawing back to front for transparency really.

        //update VBO
        std::size_t idx = 0;
        for (auto i = 0u; i < emitter.m_nextFreeParticle; ++i)
        {
            const auto& p = emitter.m_particles[i];

            //position
            m_dataBuffer[idx++] = p.position.x;
            m_dataBuffer[idx++] = p.position.y;
            m_dataBuffer[idx++] = p.position.z;

            //colour
            m_dataBuffer[idx++] = p.colour.getRed();
            m_dataBuffer[idx++] = p.colour.getGreen();
            m_dataBuffer[idx++] = p.colour.getBlue();
            m_dataBuffer[idx++] = p.colour.getAlpha();

            //rotation/size
            m_dataBuffer[idx++] = p.rotation;
            m_dataBuffer[idx++] = p.scale;
            m_dataBuffer[idx++] = 0.f;
        }
        glCheck(glBindBuffer(GL_ARRAY_BUFFER, emitter.m_vbo));
        glCheck(glBufferSubData(GL_ARRAY_BUFFER, 0, idx * sizeof(float), m_dataBuffer.data()));
    }

    glCheck(glBindBuffer(GL_ARRAY_BUFFER, 0));
}

void ParticleSystem::render(Entity camera, const RenderTarget& rt)
{   
    //particles are already in world space so just need viewProj
    const auto& cam = camera.getComponent<Camera>();
    const auto& pass = cam.getActivePass();

    if (pass.drawList.count(getType()) == 0)
    {
        return;
    }

    glm::vec4 clipPlane = glm::vec4(0.f, 1.f, 0.f, -getScene()->getWaterLevel() + (0.05f * pass.getClipPlaneMultiplier())) * pass.getClipPlaneMultiplier();

    glCheck(glEnable(GL_CULL_FACE));
    glCheck(glCullFace(pass.getCullFace()));
    glCheck(glEnable(GL_BLEND));
    glCheck(glEnable(GL_DEPTH_TEST));
    glCheck(glDepthMask(GL_FALSE));
    ENABLE_POINT_SPRITES;

    auto vp = applyViewport(cam.viewport, rt);

    //bind shader
    glCheck(glUseProgram(m_shader.getGLHandle()));

    //set shader uniforms (texture/projection)
    glCheck(glUniform4f(m_clipPlaneUniform, clipPlane.r, clipPlane.g, clipPlane.b, clipPlane.a));
    glCheck(glUniformMatrix4fv(m_projectionUniform, 1, GL_FALSE, glm::value_ptr(cam.projectionMatrix)));
    glCheck(glUniformMatrix4fv(m_viewProjUniform, 1, GL_FALSE, glm::value_ptr(pass.viewProjectionMatrix)));
    glCheck(glUniform1f(m_viewportUniform, static_cast<float>(vp.height)));
    glCheck(glUniform1i(m_textureUniform, 0));
    glCheck(glActiveTexture(GL_TEXTURE0));
    
    const auto& entities = std::any_cast<const std::vector<Entity>&>(pass.drawList.at(getType()));
    for(auto entity : entities)
    {
        const auto& emitter = entity.getComponent<ParticleEmitter>();
        //bind emitter texture
        glCheck(glBindTexture(GL_TEXTURE_2D, emitter.settings.textureID));
        glCheck(glUniform1f(m_sizeUniform, emitter.settings.size));
        
        //apply blend mode
        switch (emitter.settings.blendmode)
        {
        default: break;
        case EmitterSettings::Alpha:
            glCheck(glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA));
            break;
        case EmitterSettings::Multiply:
            glCheck(glBlendFunc(GL_DST_COLOR, GL_ZERO));
            break;
        case EmitterSettings::Add:
            glCheck(glBlendFunc(GL_ONE, GL_ONE));
            break;
        }

#ifdef PLATFORM_DESKTOP
        glCheck(glBindVertexArray(emitter.m_vao));
        glCheck(glDrawArrays(GL_POINTS, 0, static_cast<GLsizei>(emitter.m_nextFreeParticle)));
#else
        //bind emitter vbo
        glCheck(glBindBuffer(GL_ARRAY_BUFFER, emitter.m_vbo));

        //bind vertex attribs
        for (auto [index, attribSize, offset] : m_attribData)
        {
            glCheck(glEnableVertexAttribArray(index));
            glCheck(glVertexAttribPointer(index, attribSize,
                GL_FLOAT, GL_FALSE, VertexSize,
                reinterpret_cast<void*>(static_cast<intptr_t>(offset))));
        }

        //draw
        glCheck(glDrawArrays(GL_POINTS, 0, static_cast<GLsizei>(emitter.m_nextFreeParticle)));

        //unbind attribs
        for (auto j = 0u; j < m_attribData.size(); ++j)
        {
            glCheck(glDisableVertexAttribArray(m_attribData[j].index));
        }
#endif //PLATFORM
    }

#ifdef PLATFORM_DESKTOP
    glCheck(glBindVertexArray(0));
#else
    glCheck(glBindBuffer(GL_ARRAY_BUFFER, 0));
#endif //PLATFORM

    glCheck(glUseProgram(0));
    glCheck(glBindTexture(GL_TEXTURE_2D, 0));

    restorePreviousViewport();
    glCheck(glDisable(GL_CULL_FACE));
    glCheck(glDisable(GL_BLEND));
    glCheck(glDisable(GL_DEPTH_TEST));
    glCheck(glDepthMask(GL_TRUE));
    DISABLE_POINT_SPRITES;
}

//private
void ParticleSystem::onEntityAdded(Entity entity)
{    
    //check VBO count and increase if needed
    if (m_nextBuffer == m_bufferCount)
    {
        for (auto i = 0u; i < MinParticleSystems; ++i)
        {
            allocateBuffer();
        }
    }

    entity.getComponent<ParticleEmitter>().m_vbo = m_vboIDs[m_nextBuffer];
    entity.getComponent<ParticleEmitter>().m_vao = m_vaoIDs[m_nextBuffer];
    m_nextBuffer++;
}

void ParticleSystem::onEntityRemoved(Entity entity)
{
    auto vboID = entity.getComponent<ParticleEmitter>().m_vbo;
    auto vaoID = entity.getComponent<ParticleEmitter>().m_vao;
    
    //update available VBOs
    std::size_t idx = 0;
    while (m_vboIDs[idx] != vboID) { idx++; }

    std::swap(m_vboIDs[idx], m_vboIDs[m_nextBuffer]);

    //and vaos
    idx = 0;
    while (m_vaoIDs[idx] != vaoID) { idx++; }

    std::swap(m_vaoIDs[idx], m_vaoIDs[m_nextBuffer]);

    m_nextBuffer--;
}

void ParticleSystem::allocateBuffer()
{
    CRO_ASSERT(m_bufferCount < m_vboIDs.size(), "Max Buffers Reached!");
    glCheck(glGenBuffers(1, &m_vboIDs[m_bufferCount]));

#ifdef PLATFORM_DESKTOP
    glCheck(glGenVertexArrays(1, &m_vaoIDs[m_bufferCount]));
    glCheck(glBindVertexArray(m_vaoIDs[m_bufferCount]));
#endif //PLATFORM

    glCheck(glBindBuffer(GL_ARRAY_BUFFER, m_vboIDs[m_bufferCount]));
    glCheck(glBufferData(GL_ARRAY_BUFFER, MaxVertData * sizeof(float), nullptr, GL_DYNAMIC_DRAW));

#ifdef PLATFORM_DESKTOP
    for(auto [index, attribSize, offset] : m_attribData)
    {
        glCheck(glEnableVertexAttribArray(index));
        glCheck(glVertexAttribPointer(index, attribSize,
            GL_FLOAT, GL_FALSE, VertexSize,
            reinterpret_cast<void*>(static_cast<intptr_t>(offset))));
    }

    glCheck(glBindVertexArray(0));
#endif //PLATFORM

    glCheck(glBindBuffer(GL_ARRAY_BUFFER, 0));

    m_bufferCount++;
}

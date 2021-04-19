/*-----------------------------------------------------------------------

Matt Marchant 2021
http://trederia.blogspot.com

crogine application - Zlib license.

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

#include "CrateSystem.hpp"
#include "Collision.hpp"
#include "GameConsts.hpp"
#include "CommonConsts.hpp"
#include "Messages.hpp"

#include <crogine/ecs/Scene.hpp>

#include <crogine/ecs/components/Transform.hpp>
#include <crogine/ecs/components/DynamicTreeComponent.hpp>

#include <crogine/ecs/systems/DynamicTreeSystem.hpp>

#include <crogine/detail/glm/gtx/norm.hpp>

namespace
{
    constexpr std::int32_t StepCount = 2;
    constexpr auto Timestep = ConstVal::FixedGameUpdate / static_cast<float>(StepCount);
}

CrateSystem::CrateSystem(cro::MessageBus& mb)
    : cro::System(mb, typeid(CrateSystem))
{
    requireComponent<Crate>();
    requireComponent<cro::Transform>();
    requireComponent<cro::DynamicTreeComponent>();
}

//public
void CrateSystem::process(float)
{
    auto& entities = getEntities();
    for (auto& entity : entities)
    {
        const auto& crate = entity.getComponent<Crate>();
        auto oldState = crate.state;
        switch (crate.state)
        {
        default: break;
        case Crate::Ballistic:
            processBallistic(entity);
            break;
        case Crate::Carried:
            //processCarried(entity);
            break;
        case Crate::Falling:
            processFalling(entity);
            break;
        case Crate::Idle:
            processIdle(entity);
            break;
        }

        if (crate.state != oldState)
        {
            auto* msg = postMessage<CrateEvent>(MessageID::CrateMessge);
            msg->crate = entity;
            msg->type = CrateEvent::StateChanged;
        }
    }
}

//private
void CrateSystem::processIdle(cro::Entity entity)
{
    auto& crate = entity.getComponent<Crate>();
    crate.owner = -1; //set this here to just make sure it's not missed by any state switch

    //test if foot is free and switch to falling
    //ie if the crate underneath was punted out
    auto collisions = doBroadPhase(entity);
    auto position = entity.getComponent<cro::Transform>().getPosition();
    const auto& collisionComponent = entity.getComponent<CollisionComponent>();

    auto footRect = collisionComponent.rects[1].bounds;
    footRect.left += position.x;
    footRect.bottom += position.y;

    for (auto e : collisions)
    {
        auto otherPos = e.getComponent<cro::Transform>().getPosition();
        const auto& otherCollision = e.getComponent<CollisionComponent>();
        for (auto i = 0; i < otherCollision.rectCount; ++i)
        {
            auto otherRect = otherCollision.rects[i].bounds;
            otherRect.left += otherPos.x;
            otherRect.bottom += otherPos.y;

            //foot collision
            if (footRect.intersects(otherRect))
            {
                crate.collisionFlags |= (1 << CollisionMaterial::Foot);
            }
        }
    }

    if (crate.collisionFlags == 0)
    {
        crate.state = Crate::State::Falling;
    }
}

void CrateSystem::processFalling(cro::Entity entity)
{
    auto& crate = entity.getComponent<Crate>();
    
    //smaller steps help reduce tunneling
    for (auto i = 0; i < StepCount; ++i)
    {
        //apply gravity
        crate.velocity.y = std::max(crate.velocity.y + (Gravity * Timestep), MaxGravity);

        //move
        entity.getComponent<cro::Transform>().move(crate.velocity * Timestep);

        //collision update
        auto collisions = doBroadPhase(entity);

        auto position = entity.getComponent<cro::Transform>().getPosition();
        const auto& collisionComponent = entity.getComponent<CollisionComponent>();

        auto footRect = collisionComponent.rects[1].bounds;
        footRect.left += position.x;
        footRect.bottom += position.y;

        auto bodyRect = collisionComponent.rects[0].bounds;
        bodyRect.left += position.x;
        bodyRect.bottom += position.y;

        for (auto e : collisions)
        {
            auto otherPos = e.getComponent<cro::Transform>().getPosition();
            const auto& otherCollision = e.getComponent<CollisionComponent>();
            for (auto i = 0; i < otherCollision.rectCount; ++i)
            {
                auto otherRect = otherCollision.rects[i].bounds;
                otherRect.left += otherPos.x;
                otherRect.bottom += otherPos.y;

                cro::FloatRect overlap;

                //crate collision
                if (bodyRect.intersects(otherRect, overlap))
                {
                    //set the flag to what we're touching as long as it's not a foot
                    crate.collisionFlags |= ((1 << otherCollision.rects[i].material) & ~(1 << CollisionMaterial::Foot));

                    auto manifold = calcManifold(bodyRect, otherRect, overlap);

                    //foot collision
                    if (footRect.intersects(otherRect, overlap))
                    {
                        crate.collisionFlags |= (1 << CollisionMaterial::Foot);
                    }


                    switch (otherCollision.rects[i].material)
                    {
                    default: break;
                    case CollisionMaterial::Crate:
                        if (e.getComponent<Crate>().state != Crate::State::Idle)
                        {
                            //ignore non-idle crates
                            break;
                        }
                        //otherwise treat as solid
                        [[fallthrough]];
                    case CollisionMaterial::Solid:
                        //correct for position
                        entity.getComponent<cro::Transform>().move(manifold.normal * manifold.penetration);

                        if (crate.collisionFlags & (1 << CollisionMaterial::Foot)
                            && manifold.normal.y > 0
                            && crate.velocity.y < 0) //landed from above
                        {
                            crate.state = Crate::State::Idle;
                            crate.velocity.y = 0.f;
                        }
                        else
                        {
                            crate.velocity = glm::reflect(crate.velocity, glm::vec3(manifold.normal, 0.f));
                            crate.velocity.x *= 0.3f; //0.1 ? ought to make these magic numbers const, compare with player collision for example
                        }
                        break;
                    case CollisionMaterial::Teleport:
                        //nothing?
                        break;
                    case CollisionMaterial::Body:
                        //hit a player - we'll check the flags in the result at the end
                        //else this may be raised multiple times due to multiple iterations
                        //so we'll just track which player we hit then deal with it in the flag check below
                        break;
                    }
                }
            }
        }
    }

    if (crate.collisionFlags & (1 << CollisionMaterial::Body))
    {
        //hitting a player - TODO track who this is
    }
}

void CrateSystem::processBallistic(cro::Entity entity)
{
    auto& crate = entity.getComponent<Crate>();

    //apply friction
    crate.velocity *= 0.9f;

    for (auto i = 0; i < StepCount; ++i)
    {
        //apply sideways movement
        entity.getComponent<cro::Transform>().move(crate.velocity * Timestep);

        //do collisions
        auto collisions = doBroadPhase(entity);
        auto position = entity.getComponent<cro::Transform>().getPosition();
        const auto& collisionComponent = entity.getComponent<CollisionComponent>();

        auto footRect = collisionComponent.rects[1].bounds;
        footRect.left += position.x;
        footRect.bottom += position.y;

        auto bodyRect = collisionComponent.rects[0].bounds;
        bodyRect.left += position.x;
        bodyRect.bottom += position.y;

        for (auto e : collisions)
        {
            //check for solid/player collision
            if (!e.isValid())
            {
                continue;
            }

            auto otherPos = e.getComponent<cro::Transform>().getPosition();
            const auto& otherCollision = e.getComponent<CollisionComponent>();
            for (auto i = 0; i < otherCollision.rectCount; ++i)
            {
                auto otherRect = otherCollision.rects[i].bounds;
                otherRect.left += otherPos.x;
                otherRect.bottom += otherPos.y;

                cro::FloatRect overlap;

                //foot collision
                if (footRect.intersects(otherRect, overlap))
                {
                    crate.collisionFlags |= (1 << CollisionMaterial::Foot);
                }

                //body collision
                if (bodyRect.intersects(otherRect, overlap))
                {
                    //track which objects we're touching
                    crate.collisionFlags |= ((1 << otherCollision.rects[i].material) & ~(1 << CollisionMaterial::Foot));

                    auto manifold = calcManifold(bodyRect, otherRect, overlap);
                    switch (otherCollision.rects[i].material)
                    {
                    default: break;
                    case CollisionMaterial::Crate:
                        if (e.getComponent<Crate>().state != Crate::State::Idle)
                        {
                            //ignore crates which aren't idle
                            break;
                        }
                        //otherwise treat as solid
                        [[fallthrough]];
                    case CollisionMaterial::Solid:
                        entity.getComponent<cro::Transform>().move(manifold.normal * manifold.penetration);

                        if (manifold.normal.x != 0)
                        {
                            crate.velocity = glm::reflect(crate.velocity, glm::vec3(manifold.normal, 0.f)) * 0.1f;
                        }
                        break;
                    case CollisionMaterial::Body:
                        //hit a player! TODO track which and deal with them in flag check below
                        break;
                    }
                }
            }
        }
    }


    //check if we fell off an edge
    if (crate.collisionFlags == 0)
    {
        crate.state = Crate::State::Falling;
    }

    else if (crate.collisionFlags & (1 << CollisionMaterial::Body))
    {
        //TODO deal with any player we hit
    }

    //switch state if we stopped moving
    else if (glm::length2(crate.velocity) < 0.1f)
    {
        crate.velocity = glm::vec3(0.f);
        crate.state = Crate::State::Idle;
    }
}

std::vector<cro::Entity> CrateSystem::doBroadPhase(cro::Entity entity)
{
    entity.getComponent<Crate>().collisionFlags = 0;
    auto position = entity.getComponent<cro::Transform>().getPosition();

    auto bb = CrateBounds;
    bb += position;

    auto& collisionComponent = entity.getComponent<CollisionComponent>();
    auto bounds2D = collisionComponent.sumRect;
    bounds2D.left += position.x;
    bounds2D.bottom += position.y;

    std::vector<cro::Entity> collisions;

    //broadphase
    auto entities = getScene()->getSystem<cro::DynamicTreeSystem>().query(bb, entity.getComponent<Crate>().collisionLayer + 1);
    for (auto e : entities)
    {
        //make sure we skip our own ent
        if (e != entity)
        {
            auto otherPos = e.getComponent<cro::Transform>().getPosition();
            auto otherBounds = e.getComponent<CollisionComponent>().sumRect;
            otherBounds.left += otherPos.x;
            otherBounds.bottom += otherPos.y;

            if (otherBounds.intersects(bounds2D))
            {
                collisions.push_back(e);
            }
        }
    }

    return collisions;
}
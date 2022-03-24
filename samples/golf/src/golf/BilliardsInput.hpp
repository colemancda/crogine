/*-----------------------------------------------------------------------

Matt Marchant 2022
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

#pragma once

#include "InputBinding.hpp"

#include <crogine/core/Window.hpp>
#include <crogine/ecs/Entity.hpp>
#include <crogine/detail/glm/gtc/quaternion.hpp>

struct ControllerRotation final
{
    float rotation = 0.f; //rads
    bool* activeCamera = nullptr;
};

struct ControlEntities final
{
    cro::Entity camera;
    cro::Entity cue;
    cro::Entity previewBall;
};

class BilliardsInput final
{
public:
    BilliardsInput(const InputBinding&, cro::MessageBus&);

    void handleEvent(const cro::Event&);
    void update(float);

    void setActive(bool active, bool placeCueball);
    void setControlEntities(ControlEntities);

    std::pair<glm::vec3, glm::vec3> getImpulse() const;

    static constexpr float MaxPower = 1.f;
    float getPower() const { return m_power; }

    const glm::quat& getSpinOffset() const { return m_spinOffset; }

private:
    const InputBinding& m_inputBinding;
    cro::MessageBus& m_messageBus;

    enum
    {
        Play, PlaceBall
    }m_state = PlaceBall;

    std::uint16_t m_inputFlags;
    std::uint16_t m_prevFlags;
    std::uint16_t m_prevStick;

    glm::vec2 m_mouseMove;
    glm::vec2 m_prevMouseMove;
    float m_analogueAmountLeft;
    float m_analogueAmountRight;

    bool m_active;

    float m_power;
    float m_topSpin;
    float m_sideSpin;
    glm::quat m_spinOffset;

    ControlEntities m_controlEntities;
    glm::vec3 m_basePosition;

    bool hasMouseMotion() const;

    void checkController(float);
    void updatePlay(float);
    void updatePlaceBall(float);
};

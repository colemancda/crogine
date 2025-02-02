/*-----------------------------------------------------------------------

Matt Marchant 2017
http://trederia.blogspot.com

crogine test application - Zlib license.

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

#ifndef DH_PLAYER_DIRECTOR_HPP_
#define DH_PLAYER_DIRECTOR_HPP_

#include <crogine/ecs/Director.hpp>
#include <crogine/gui/GuiClient.hpp>

#include <crogine/detail/glm/vec3.hpp>

struct Player final
{
    enum class State
    {
        Idle, Running
    }state = State::Idle;
    std::int16_t lives = 3;
    float health = 100.f;
};

class PlayerDirector final : public cro::Director, public cro::GuiClient
{
public:
    PlayerDirector();

private:

    std::uint16_t m_flags;
    std::uint16_t m_previousFlags;
    float m_accumulator;
    float m_playerRotation;

    void handleEvent(const cro::Event&) override;
    void handleMessage(const cro::Message&) override;
    void process(float)override;
};

#endif //DH_PLAYER_DIRECTOR_HPP_
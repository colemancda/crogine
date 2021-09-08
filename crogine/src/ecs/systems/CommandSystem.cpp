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

#include <crogine/ecs/components/CommandTarget.hpp>
#include <crogine/ecs/systems/CommandSystem.hpp>
#include <crogine/core/Clock.hpp>

using namespace cro;

namespace
{
    //this can be made larger if necessary, is only used to prevent continual reallocation of heap memory
    //TODO could be make this a circular buffer to prevent crashes when we exceed max size? We might lose
    //some commands however.
    const std::size_t MaxCommands = 4096;
}

CommandSystem::CommandSystem(MessageBus& mb)
    : System        (mb, typeid(CommandSystem)),
    m_commands      (MaxCommands),
    m_commandBuffer (MaxCommands),
    m_count         (0)
{
    requireComponent<CommandTarget>();
}

//public
void CommandSystem::sendCommand(const Command& cmd)
{
    if (m_count < MaxCommands)
    {
        m_commandBuffer[m_count++] = cmd;
    }
}

void CommandSystem::process(float dt)
{
    auto count = m_count;
    m_count = 0;
    m_commands.swap(m_commandBuffer);

    auto& entities = getEntities();

    for (auto i = 0u; i < count; ++i)
    {
        for (auto& e : entities)
        {
            if (e.getComponent<CommandTarget>().ID & m_commands[i].targetFlags)
            {
                m_commands[i].action(e, dt);
            }
        }
    }
}
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

#include "CPUGolfer.hpp"
#include "InputParser.hpp"
#include "MessageIDs.hpp"
#include "Clubs.hpp"
#include "server/ServerPacketData.hpp"

#include <crogine/gui/Gui.hpp>
#include <crogine/util/Random.hpp>
#include <crogine/util/Maths.hpp>

namespace
{
    const std::array StateStrings =
    {
        std::string("Inactive"),
        std::string("Picking Club"),
        std::string("Aiming"),
        std::string("Stroke"),
        std::string("Watching")
    };

    struct Debug final
    {
        float diff = 0.f;
        float windDot = 0.f;
        float windComp = 0.f;
        float targetAngle = 0.f;
    }debug;

    constexpr float MinSearchDistance = 10.f;
    constexpr float SearchIncrease = 5.f;
}

CPUGolfer::CPUGolfer(const InputParser& ip, const ActivePlayer& ap)
    : m_inputParser     (ip),
    m_activePlayer      (ap),
    m_target            (0.f),
    m_clubID            (ClubID::Driver),
    m_prevClubID        (ClubID::Driver),
    m_searchDirection   (0),
    m_searchDistance    (MinSearchDistance),
    m_aimDistance       (0.f),
    m_aimAngle          (0.f),
    m_targetPower       (1.f),
    m_targetAccuracy    (0.f),
    m_thinking          (false),
    m_thinkTime         (0.f)
{
#ifdef CRO_DEBUG_
    registerWindow([&]()
        {
            if (ImGui::Begin("CPU"))
            {
                ImGui::Text("State: %s", StateStrings[static_cast<std::int32_t>(m_state)].c_str());
                ImGui::Text("Wind Dot: %3.2f", debug.windDot);
                ImGui::Text("Target Diff: %3.2f", debug.diff);
                ImGui::Text("Search Distance: %3.2f", m_searchDistance);
                ImGui::Text("Target Distance: %3.3f", m_aimDistance);
                ImGui::Text("Current Club: %s", Clubs[m_clubID].name.c_str());
                ImGui::Separator();
                ImGui::Text("Wind Comp: %3.3f", debug.windComp);
                ImGui::Text("Start Angle: %3.3f", m_aimAngle);
                ImGui::Text("Target Angle: %3.3f", debug.targetAngle);
                ImGui::Text("Current Angle: %3.3f", m_inputParser.getYaw());
                ImGui::Text("Target Power: %3.3f", m_targetPower);
                ImGui::Text("Target Accuracy: %3.3f", m_targetAccuracy);
            }
            ImGui::End();
        });
#endif
}

//public
void CPUGolfer::handleMessage(const cro::Message& msg)
{
    switch (msg.id)
    {
    default: break;
    case MessageID::GolfMessage:
    {
        const auto& data = msg.getData<GolfEvent>();
        if (data.type == GolfEvent::BallLanded)
        {
            m_state = State::Inactive;
            LOG("CPU is now inactive", cro::Logger::Type::Info);
        }
    }
        break;
    }
}

void CPUGolfer::activate(glm::vec3 target)
{
    if (m_state == State::Inactive)
    {
        m_target = target;
        m_state = State::PickingClub;
        m_clubID = m_inputParser.getClub();
        m_prevClubID = m_clubID;
        m_searchDistance = MinSearchDistance;

        startThinking(2.f);
        LOG("CPU is now active", cro::Logger::Type::Info);
    }
}

void CPUGolfer::update(float dt, glm::vec3 windVector)
{
    for (auto& evt : m_popEvents)
    {
        SDL_PushEvent(&evt);
    }
    m_popEvents.clear();

    switch (m_state)
    {
    default:
    case State::Inactive:
        
        break;
    case State::PickingClub:
        pickClub(dt, windVector);
        break;
    case State::Aiming:
        aim(dt, windVector);
        break;
    case State::Stroke:
        stroke();
        break;
    case State::Watching:

        break;
    }
}

//private
void CPUGolfer::startThinking(float duration)
{
    if (!m_thinking)
    {
        m_thinking = true;
        m_thinkTime = duration + cro::Util::Random::value(0.1f, 0.8f);
    }
}

void CPUGolfer::think(float dt)
{
    m_thinkTime -= dt;
    if (m_thinkTime < 0)
    {
        m_thinking = false;
    }
}

void CPUGolfer::pickClub(float dt, glm::vec3 windVector)
{
    if (m_thinking)
    {
        think(dt);
    }
    else
    {
        //get distance to target
        auto targetDir = m_target - m_activePlayer.position;
        float targetDistance = glm::length(targetDir);

        //if we're on the green putter should be auto selected
        if (m_activePlayer.terrain == TerrainID::Green)
        {
            startThinking(2.f);
            m_state = State::Aiming;
            m_clubID = ClubID::Putter;
            m_prevClubID = m_clubID;

            m_aimDistance = targetDistance;
            m_aimAngle = m_inputParser.getYaw();

            LOG("CPU Entered Aiming Mode", cro::Logger::Type::Info);
            return;
        }

        //adjust the target distance depending on how the wind carries us
        float windDot = -(glm::dot(glm::vec2(windVector.x, windVector.z), glm::vec2(targetDir.x, targetDir.z)) / targetDistance);
        windDot *= windVector.y;
        windDot *= 0.3f; //magic number. This extends a distance of 77m to 100 for example
        windDot = std::max(0.f, windDot); //skew this positively, negative amounts will be compensated for by using less power
        targetDistance += (targetDistance * windDot);

        //and hit further if we're off the fairway
        if (m_activePlayer.terrain == TerrainID::Rough
            || m_activePlayer.terrain == TerrainID::Bunker)
        {
            //we want to aim a bit further if we went
            //off the fairway, so we hit a bit harder.
            targetDistance += 20.f;
        }



        auto club = m_inputParser.getClub();
        float clubDistance = Clubs[club].target;

        const auto acceptClub = [&]()
        {
            startThinking(1.f);
            m_state = State::Aiming;
            m_aimDistance = targetDistance;
            m_aimAngle = m_inputParser.getYaw();
            LOG("CPU Entered Aiming Mode", cro::Logger::Type::Info);
        };
        
        auto diff = targetDistance - clubDistance;
#ifdef CRO_DEBUG_
        debug.diff = diff;
        debug.windDot = windDot;
#endif
        if (std::abs(diff) < m_searchDistance)
        {
            m_prevClubID = club;
            m_clubID = club;

            acceptClub();
            return;
        }

        //if the new club has looped switch back and accept it
        if (m_searchDirection == 1 && clubDistance < Clubs[m_prevClubID].target)
        {
            sendKeystroke(m_inputParser.getInputBinding().keys[InputBinding::PrevClub]);
            m_clubID = m_prevClubID;

            acceptClub();
            return;
        }

        if (m_searchDirection == -1 && clubDistance > Clubs[m_prevClubID].target)
        {
            sendKeystroke(m_inputParser.getInputBinding().keys[InputBinding::NextClub]);
            m_clubID = m_prevClubID;

            acceptClub();
            return;
        }

        if (diff > 0)
        {
            //if we previously searched down increase the search distance
            if (m_searchDirection == -1)
            {
                m_searchDistance += SearchIncrease;
            }

            //increase club if needed
            sendKeystroke(m_inputParser.getInputBinding().keys[InputBinding::NextClub]);
            m_searchDirection = 1;
            startThinking(0.25f);
        }
        else
        {
            //if we previously searched up increase the search distance
            if (m_searchDirection == 1)
            {
                m_searchDistance += SearchIncrease;
            }

            //else decrease
            sendKeystroke(m_inputParser.getInputBinding().keys[InputBinding::PrevClub]);
            m_searchDirection = -1;
            startThinking(0.25f);
        }
        m_prevClubID = club;

        //else think for some time
        startThinking(1.f);
    }
}

void CPUGolfer::aim(float dt, glm::vec3 windVector)
{
    if (m_thinking)
    {
        think(dt);
    }
    else
    {
        //check someone hasn't pressed the keyboard already
        //and started the power bar. In which case break from this
        //TODO could play avatar disappointed sound? :)
        if (m_inputParser.inProgress())
        {
            m_state = State::Watching;
            return;
        }

        //we don't have slope info here unfortunately
        //so we'll have to settle on hitting it as straight as possible
        float greenCompensation = 1.f;
        if (m_activePlayer.terrain == TerrainID::Green)
        {
            greenCompensation = 0.01f;
        }


        //wind is x, strength (0 - 1), z

        //create target angle based on wind strength / direction
        auto targetDir = m_target - m_activePlayer.position;

        auto w = glm::normalize(glm::vec2(windVector.x, -windVector.z));
        auto t = glm::normalize(glm::vec2(targetDir.x, -targetDir.z));

        float dot = glm::dot(w, t);
        float windComp = 1.f - std::abs(dot);
        windComp *= cro::Util::Maths::sgn(std::atan2(w.y, w.x) - m_aimAngle);
        windComp *= windVector.y; //reduce with wind strength
        windComp *= greenCompensation;

        float targetAngle = m_aimAngle + (m_inputParser.getMaxRotation() * windComp);

#ifdef CRO_DEBUG_
        debug.windComp = windComp;
        debug.targetAngle = targetAngle;
#endif        

        //hold rotate button if not within angle tolerance
        if (targetAngle > m_inputParser.getYaw())
        {
            sendKeystroke(m_inputParser.getInputBinding().keys[InputBinding::Right], false);
        }
        else
        {
            sendKeystroke(m_inputParser.getInputBinding().keys[InputBinding::Left], false);
        }

        //if angle correct then calc a target power
        if (std::abs(m_inputParser.getYaw() - targetAngle) < 0.01f)
        {
            //stop holding rotate
            sendKeystroke(m_inputParser.getInputBinding().keys[InputBinding::Right], true);
            sendKeystroke(m_inputParser.getInputBinding().keys[InputBinding::Left], true);

            //based on dot prod of aim angle and wind dir
            //multiplied by percent of selected club distance to target distance
            m_targetPower = m_aimDistance / Clubs[m_clubID].target;
            m_targetPower += (0.2f * (-dot * windVector.y)) * m_targetPower;

            //add some random factor to target power and set to stroke mode
            m_targetPower = std::min(1.f, m_targetPower);
            m_targetPower += static_cast<float>(cro::Util::Random::value(-5, 0)) / 100.f;
            m_targetAccuracy = static_cast<float>(cro::Util::Random::value(-10, 10)) / 100.f;


            m_state = State::Stroke;
            startThinking(2.f);
            LOG("Started stroke", cro::Logger::Type::Info);
        }
    }
}

void CPUGolfer::stroke()
{

}

void CPUGolfer::sendKeystroke(std::int32_t key, bool autoRelease)
{
    SDL_Event evt;
    evt.type = SDL_KEYDOWN;
    evt.key.keysym.sym = key;
    evt.key.keysym.scancode = SDL_GetScancodeFromKey(key);
    evt.key.timestamp = 0;
    evt.key.repeat = 0;
    evt.key.windowID = 0;
    evt.key.state = SDL_PRESSED;

    SDL_PushEvent(&evt);

    if (autoRelease)
    {
        //stash the counter-event
        evt.type = SDL_KEYUP;
        evt.key.state = SDL_RELEASED;
        m_popEvents.push_back(evt);
    }
};
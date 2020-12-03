/*-----------------------------------------------------------------------

Matt Marchant 2020
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

#include <crogine/core/State.hpp>
#include <crogine/ecs/Scene.hpp>
#include <crogine/graphics/ModelDefinition.hpp>
#include <crogine/graphics/Texture.hpp>
#include <crogine/graphics/EnvironmentMap.hpp>
#include <crogine/graphics/RenderTexture.hpp>

#include <crogine/gui/GuiClient.hpp>

#include "StateIDs.hpp"
#include "ResourceIDs.hpp"

#include <array>

class GameState final : public cro::State, public cro::GuiClient
{
public:
    GameState(cro::StateStack&, cro::State::Context);
    ~GameState() = default;

    cro::StateID getStateID() const override { return States::Game; }

    bool handleEvent(const cro::Event&) override;
    void handleMessage(const cro::Message&) override;
    bool simulate(float) override;
    void render() override;

private:

    cro::Scene m_gameScene;
    cro::Scene m_uiScene;

    cro::ResourceCollection m_resources;
    std::array<std::int32_t, MaterialID::Count> m_materialIDs = {};

    cro::EnvironmentMap m_environmentMap;
    cro::RenderTexture m_reflectionBuffer;
    cro::Entity m_reflectionCamera;

    void addSystems();
    void loadAssets();
    void createScene();
    void createUI();

    void updateView(cro::Camera&);
};
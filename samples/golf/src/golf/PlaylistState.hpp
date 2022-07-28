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

#include "../StateIDs.hpp"
#include "GameConsts.hpp"

#include <crogine/core/State.hpp>
#include <crogine/audio/AudioScape.hpp>
#include <crogine/ecs/Scene.hpp>
#include <crogine/graphics/ModelDefinition.hpp>
#include <crogine/graphics/RenderTexture.hpp>
#include <crogine/graphics/UniformBuffer.hpp>

struct SharedStateData;

class PlaylistState final : public cro::State
{
public:
    PlaylistState(cro::StateStack&, cro::State::Context, SharedStateData&);

    bool handleEvent(const cro::Event&) override;

    void handleMessage(const cro::Message&) override;

    bool simulate(float) override;

    void render() override;

    cro::StateID getStateID() const override { return StateID::Playlist; }

private:

    cro::Scene m_skyboxScene;
    cro::Scene m_gameScene;
    cro::Scene m_uiScene;
    SharedStateData& m_sharedData;

    cro::ResourceCollection m_resources;
    cro::RenderTexture m_gameSceneTexture;

    cro::UniformBuffer<float> m_scaleBuffer;
    cro::UniformBuffer<ResolutionData> m_resolutionBuffer;
    cro::UniformBuffer<WindData> m_windBuffer;
    struct ResolutionUpdate final
    {
        ResolutionData resolutionData;
        float targetFade = 0.1f;
    }m_resolutionUpdate;

    cro::AudioScape m_menuSounds;
    struct AudioID final
    {
        enum
        {
            Accept, Back,

            Count
        };
    };
    std::array<cro::Entity, AudioID::Count> m_audioEnts = {};

    struct MenuID final
    {
        enum
        {
            Skybox, Shrubbery,
            Holes, FileSystem,

            Count
        };
    };
    std::array<cro::Entity, MenuID::Count> m_menuEntities = {};

    struct MaterialID final
    {
        enum
        {
            Horizon, Water,

            Count
        };
    };
    std::array<std::size_t, MaterialID::Count> m_materialIDs = {};

    glm::vec2 m_viewScale;
    
    struct CourseData final
    {
        std::string skyboxPath;
    }m_courseData;
    std::vector<std::string> m_skyboxes;
    std::size_t m_skyboxIndex;

    void addSystems();
    void loadAssets();
    void buildScene();
    void buildUI();

    void createSkyboxMenu(cro::Entity, std::uint32_t selected, std::uint32_t unselected);
    void createShrubberyMenu(cro::Entity);
    void createHoleMenu(cro::Entity);
    void createFileSystemMenu(cro::Entity);

    void updateNinePatch(cro::Entity);

    void quitState();
};
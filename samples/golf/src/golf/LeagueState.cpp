/*-----------------------------------------------------------------------

Matt Marchant 2021 - 2023
http://trederia.blogspot.com

Super Video Golf - zlib licence.

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

#include "LeagueState.hpp"
#include "SharedStateData.hpp"
#include "CommonConsts.hpp"
#include "CommandIDs.hpp"
#include "MenuConsts.hpp"
#include "GameConsts.hpp"
#include "TextAnimCallback.hpp"
#include "MessageIDs.hpp"
#include "Clubs.hpp"
#include "../GolfGame.hpp"
#include "../Colordome-32.hpp"

#include <Social.hpp>
#include <AchievementIDs.hpp>
#include <AchievementStrings.hpp>
#include <Achievements.hpp>

#include <crogine/core/Window.hpp>
#include <crogine/core/GameController.hpp>
#include <crogine/graphics/Image.hpp>
#include <crogine/graphics/SpriteSheet.hpp>
#include <crogine/gui/Gui.hpp>
#include <crogine/core/SysTime.hpp>

#include <crogine/ecs/components/Transform.hpp>
#include <crogine/ecs/components/UIInput.hpp>
#include <crogine/ecs/components/CommandTarget.hpp>
#include <crogine/ecs/components/Callback.hpp>
#include <crogine/ecs/components/Sprite.hpp>
#include <crogine/ecs/components/SpriteAnimation.hpp>
#include <crogine/ecs/components/Text.hpp>
#include <crogine/ecs/components/Camera.hpp>
#include <crogine/ecs/components/Drawable2D.hpp>
#include <crogine/ecs/components/AudioEmitter.hpp>

#include <crogine/ecs/systems/UISystem.hpp>
#include <crogine/ecs/systems/CommandSystem.hpp>
#include <crogine/ecs/systems/CallbackSystem.hpp>
#include <crogine/ecs/systems/SpriteSystem2D.hpp>
#include <crogine/ecs/systems/SpriteAnimator.hpp>
#include <crogine/ecs/systems/TextSystem.hpp>
#include <crogine/ecs/systems/CameraSystem.hpp>
#include <crogine/ecs/systems/RenderSystem2D.hpp>
#include <crogine/ecs/systems/AudioPlayerSystem.hpp>

#include <crogine/util/Easings.hpp>

#include <crogine/detail/OpenGL.hpp>
#include <crogine/detail/glm/gtc/matrix_transform.hpp>

using namespace cl;

namespace
{

}

LeagueState::LeagueState(cro::StateStack& ss, cro::State::Context ctx, SharedStateData& sd)
    : cro::State            (ss, ctx),
    m_scene                 (ctx.appInstance.getMessageBus(), 480),
    m_sharedData            (sd),
    m_viewScale             (2.f),
    m_currentTab            (0)
{
    ctx.mainWindow.setMouseCaptured(false);

    buildScene();
}

//public
bool LeagueState::handleEvent(const cro::Event& evt)
{
    if (ImGui::GetIO().WantCaptureKeyboard
        || ImGui::GetIO().WantCaptureMouse
        || m_rootNode.getComponent<cro::Callback>().active)
    {
        return false;
    }

    if (evt.type == SDL_KEYUP)
    {
        if (evt.key.keysym.sym == SDLK_BACKSPACE
            || evt.key.keysym.sym == SDLK_ESCAPE
            || evt.key.keysym.sym == SDLK_p)
        {
            quitState();
            return false;
        }
    }
    else if (evt.type == SDL_KEYDOWN)
    {
        switch (evt.key.keysym.sym)
        {
        default: break;
        case SDLK_UP:
        case SDLK_DOWN:
        case SDLK_LEFT:
        case SDLK_RIGHT:
            cro::App::getWindow().setMouseCaptured(true);
            break;
        }
    }
    else if (evt.type == SDL_CONTROLLERBUTTONUP)
    {
        cro::App::getWindow().setMouseCaptured(true);

        switch (evt.cbutton.button)
        {
        default: break;
        case cro::GameController::ButtonB:
        case cro::GameController::ButtonStart:
            quitState();
            return false;
        case cro::GameController::ButtonRightShoulder:
            activateTab((m_currentTab + 1) % TabID::Max);
            break;
        case cro::GameController::ButtonLeftShoulder:
            activateTab((m_currentTab + (TabID::Max - 1)) % TabID::Max);
            break;
        }
    }
    else if (evt.type == SDL_MOUSEBUTTONUP)
    {
        if (evt.button.button == SDL_BUTTON_RIGHT)
        {
            quitState();
            return false;
        }
    }
    else if (evt.type == SDL_CONTROLLERAXISMOTION)
    {
        if (evt.caxis.value > LeftThumbDeadZone)
        {
            cro::App::getWindow().setMouseCaptured(true);
        }
    }
    else if (evt.type == SDL_MOUSEMOTION)
    {
        cro::App::getWindow().setMouseCaptured(false);
    }

    m_scene.getSystem<cro::UISystem>()->handleEvent(evt);
    m_scene.forwardEvent(evt);
    return false;
}

void LeagueState::handleMessage(const cro::Message& msg)
{
    m_scene.forwardMessage(msg);
}

bool LeagueState::simulate(float dt)
{
    m_scene.simulate(dt);
    return true;
}

void LeagueState::render()
{
    m_scene.render();
}

//private
void LeagueState::buildScene()
{
    auto& mb = getContext().appInstance.getMessageBus();
    m_scene.addSystem<cro::UISystem>(mb);
    m_scene.addSystem<cro::CommandSystem>(mb);
    m_scene.addSystem<cro::CallbackSystem>(mb);
    m_scene.addSystem<cro::SpriteSystem2D>(mb);
    m_scene.addSystem<cro::SpriteAnimator>(mb);
    m_scene.addSystem<cro::TextSystem>(mb);
    m_scene.addSystem<cro::CameraSystem>(mb);
    m_scene.addSystem<cro::RenderSystem2D>(mb);
    m_scene.addSystem<cro::AudioPlayerSystem>(mb);

    m_scene.setSystemActive<cro::AudioPlayerSystem>(false);

    m_menuSounds.loadFromFile("assets/golf/sound/menu.xas", m_sharedData.sharedResources->audio);
    m_audioEnts[AudioID::Accept] = m_scene.createEntity();
    m_audioEnts[AudioID::Accept].addComponent<cro::AudioEmitter>() = m_menuSounds.getEmitter("accept");
    m_audioEnts[AudioID::Back] = m_scene.createEntity();
    m_audioEnts[AudioID::Back].addComponent<cro::AudioEmitter>() = m_menuSounds.getEmitter("back");


    struct RootCallbackData final
    {
        enum
        {
            FadeIn, FadeOut
        }state = FadeIn;
        float currTime = 0.f;
    };

    auto rootNode = m_scene.createEntity();
    rootNode.addComponent<cro::Transform>();
    rootNode.addComponent<cro::Callback>().active = true;
    rootNode.getComponent<cro::Callback>().setUserData<RootCallbackData>();
    rootNode.getComponent<cro::Callback>().function =
        [&](cro::Entity e, float dt)
    {
        auto& [state, currTime] = e.getComponent<cro::Callback>().getUserData<RootCallbackData>();

        switch (state)
        {
        default: break;
        case RootCallbackData::FadeIn:
            currTime = std::min(1.f, currTime + (dt * 2.f));
            e.getComponent<cro::Transform>().setScale(m_viewScale * cro::Util::Easing::easeOutQuint(currTime));

            if (currTime == 1)
            {
                state = RootCallbackData::FadeOut;
                e.getComponent<cro::Callback>().active = false;

                m_scene.setSystemActive<cro::AudioPlayerSystem>(true);
                m_audioEnts[AudioID::Accept].getComponent<cro::AudioEmitter>().play();

                Social::setStatus(Social::InfoID::Menu, { "Browsing League Table" });
            }
            break;
        case RootCallbackData::FadeOut:
            currTime = std::max(0.f, currTime - (dt * 2.f));
            e.getComponent<cro::Transform>().setScale(m_viewScale * cro::Util::Easing::easeOutQuint(currTime));
            if (currTime == 0)
            {
                requestStackPop();

                state = RootCallbackData::FadeIn;

                Social::setStatus(Social::InfoID::Menu, { "Clubhouse" });
            }
            break;
        }

    };

    m_rootNode = rootNode;


    //quad to darken the screen
    auto entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 0.f, 0.f, -0.4f });
    entity.addComponent<cro::Drawable2D>().getVertexData() =
    {
        cro::Vertex2D(glm::vec2(-0.5f, 0.5f), cro::Colour::Black),
        cro::Vertex2D(glm::vec2(-0.5f), cro::Colour::Black),
        cro::Vertex2D(glm::vec2(0.5f), cro::Colour::Black),
        cro::Vertex2D(glm::vec2(0.5f, -0.5f), cro::Colour::Black)
    };
    entity.getComponent<cro::Drawable2D>().updateLocalBounds();
    entity.addComponent<cro::Callback>().active = true;
    entity.getComponent<cro::Callback>().function =
        [&, rootNode](cro::Entity e, float)
    {
        auto size = glm::vec2(GolfGame::getActiveTarget()->getSize());
        e.getComponent<cro::Transform>().setScale(size);
        e.getComponent<cro::Transform>().setPosition(size / 2.f);

        auto scale = rootNode.getComponent<cro::Transform>().getScale().x;
        scale = std::min(1.f, scale / m_viewScale.x);

        auto& verts = e.getComponent<cro::Drawable2D>().getVertexData();
        for (auto& v : verts)
        {
            v.colour.setAlpha(BackgroundAlpha * scale);
        }
    };

   
    //background
    cro::SpriteSheet spriteSheet;
    spriteSheet.loadFromFile("assets/golf/sprites/league_table.spt", m_sharedData.sharedResources->textures);

    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 0.f, 0.f, -0.2f });
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>() = spriteSheet.getSprite("background");
    auto bounds = entity.getComponent<cro::Sprite>().getTextureBounds();
    entity.getComponent<cro::Transform>().setOrigin({ std::floor(bounds.width / 2.f), std::floor(bounds.height / 2.f) });
    rootNode.getComponent<cro::Transform >().addChild(entity.getComponent<cro::Transform>());

    auto bgNode = entity;

    //tab buttons
    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 0.f, 0.f, 0.1f });
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>() = spriteSheet.getSprite("club_stats_button");
    bgNode.getComponent<cro::Transform >().addChild(entity.getComponent<cro::Transform>());

    m_tabEntity = entity;

    auto selectedID = m_scene.getSystem<cro::UISystem>()->addCallback(
        [&](cro::Entity e)
        {
            e.getComponent<cro::Callback>().active = true;
            e.getComponent<cro::Sprite>().setColour(cro::Colour::White);
            e.getComponent<cro::AudioEmitter>().play();
        });
    auto unselectedID = m_scene.getSystem<cro::UISystem>()->addCallback(
        [](cro::Entity e)
        {
            e.getComponent<cro::Sprite>().setColour(cro::Colour::Transparent);
        });


    const float stride = 102.f;
    auto sprite = spriteSheet.getSprite("button_highlight");
    bounds = sprite.getTextureBounds();
    const float offset = 51.f;

    for (auto i = 0; i < TabID::Max; ++i)
    {
        entity = m_scene.createEntity();
        entity.addComponent<cro::Transform>().setPosition({ (stride * i) + offset, 13.f, 0.5f });
        entity.getComponent<cro::Transform>().setOrigin({ std::floor(bounds.width / 2.f), std::floor(bounds.height / 2.f) });
        entity.getComponent<cro::Transform>().move(entity.getComponent<cro::Transform>().getOrigin());
        entity.addComponent<cro::AudioEmitter>() = m_menuSounds.getEmitter("switch");
        entity.addComponent<cro::Drawable2D>();
        entity.addComponent<cro::Sprite>() = sprite;
        entity.getComponent<cro::Sprite>().setColour(cro::Colour::Transparent);

        entity.addComponent<cro::UIInput>().area = bounds;
        entity.getComponent<cro::UIInput>().enabled = i != m_currentTab;
        entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Selected] = selectedID;
        entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Unselected] = unselectedID;
        entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonDown] = 
            m_scene.getSystem<cro::UISystem>()->addCallback(
                [&, i](cro::Entity e, const cro::ButtonEvent& evt) 
                {
                    if (activated(evt))
                    {
                        activateTab(i);
                        e.getComponent<cro::Sprite>().setColour(cro::Colour::Transparent);
                    }                
                });

        entity.addComponent<cro::Callback>().function = MenuTextCallback();

        bgNode.getComponent<cro::Transform >().addChild(entity.getComponent<cro::Transform>());

        m_tabButtons[i] = entity;
    }

    createLeagueTab(bgNode, spriteSheet);
    createInfoTab(bgNode, spriteSheet);

    auto updateView = [&, rootNode](cro::Camera& cam) mutable
    {
        glm::vec2 size(GolfGame::getActiveTarget()->getSize());

        cam.setOrthographic(0.f, size.x, 0.f, size.y, -2.f, 10.f);
        cam.viewport = { 0.f, 0.f, 1.f, 1.f };

        m_viewScale = glm::vec2(getViewScale());
        rootNode.getComponent<cro::Transform>().setScale(m_viewScale);
        rootNode.getComponent<cro::Transform>().setPosition(size / 2.f);

        glLineWidth(m_viewScale.x);

        //updates any text objects / buttons with a relative position
        cro::Command cmd;
        cmd.targetFlags = CommandID::Menu::UIElement;
        cmd.action =
            [&, size](cro::Entity e, float)
        {
            const auto& element = e.getComponent<UIElement>();
            auto pos = element.absolutePosition;
            pos += element.relativePosition * size / m_viewScale;

            pos.x = std::floor(pos.x);
            pos.y = std::floor(pos.y);

            e.getComponent<cro::Transform>().setPosition(glm::vec3(pos, element.depth));
        };
        m_scene.getSystem<cro::CommandSystem>()->sendCommand(cmd);
    };

    entity = m_scene.getActiveCamera();
    entity.getComponent<cro::Camera>().resizeCallback = updateView;
    updateView(entity.getComponent<cro::Camera>());

    m_scene.simulate(0.f); //updates all the group IDs

    //makes sure to set correct selection indices
    activateTab(m_currentTab);
}

void LeagueState::createLeagueTab(cro::Entity parent, const cro::SpriteSheet& spriteSheet)
{
    m_tabNodes[TabID::League] = m_scene.createEntity();
    m_tabNodes[TabID::League].addComponent<cro::Transform>().setPosition({ 0.f, 0.f, 0.2f });
    parent.getComponent<cro::Transform>().addChild(m_tabNodes[TabID::League].getComponent<cro::Transform>());
}

void LeagueState::createInfoTab(cro::Entity parent, const cro::SpriteSheet& spriteSheet)
{
    m_tabNodes[TabID::Info] = m_scene.createEntity();
    m_tabNodes[TabID::Info].addComponent<cro::Transform>().setPosition({ 0.f, 0.f, 0.2f });
    m_tabNodes[TabID::Info].getComponent<cro::Transform>().setScale(glm::vec2(0.f));
    parent.getComponent<cro::Transform>().addChild(m_tabNodes[TabID::Info].getComponent<cro::Transform>());
}

void LeagueState::activateTab(std::int32_t tabID)
{
    if (tabID != m_currentTab)
    {
        //update old
        m_tabButtons[m_currentTab].getComponent<cro::UIInput>().enabled = true;
        m_tabNodes[m_currentTab].getComponent<cro::Transform>().setScale(glm::vec2(0.f));

        //update index
        m_currentTab = tabID;

        //update new
        m_tabButtons[m_currentTab].getComponent<cro::UIInput>().enabled = false;
        m_tabNodes[m_currentTab].getComponent<cro::Transform>().setScale(glm::vec2(1.f));

        //update the button selection graphic
        auto bounds = m_tabEntity.getComponent<cro::Sprite>().getTextureRect();
        bounds.bottom = bounds.height * m_currentTab;
        m_tabEntity.getComponent<cro::Sprite>().setTextureRect(bounds);

        m_audioEnts[AudioID::Accept].getComponent<cro::AudioEmitter>().play();
    }

    //for reasons I've given up trying to understand, we need to
    //delay the selection by one frame.
    const auto selectNext = [&](std::int32_t idx)
    {
        auto e = m_scene.createEntity();
        e.addComponent<cro::Callback>().active = true;
        e.getComponent<cro::Callback>().function =
            [&, idx](cro::Entity f, float)
        {
            m_scene.getSystem<cro::UISystem>()->selectByIndex(idx);
            f.getComponent<cro::Callback>().active = false;
            m_scene.destroyEntity(f);
        };
    };

    //update the selection order depending on which page we're on
    /*switch (tabID)
    {
    default: break;
    case TabID::ClubStats:
        m_tabButtons[TabID::Performance].getComponent<cro::UIInput>().setNextIndex(ButtonHistory, ButtonHistory);
        m_tabButtons[TabID::Performance].getComponent<cro::UIInput>().setPrevIndex(ButtonAwards, ButtonAwards);

        m_tabButtons[TabID::History].getComponent<cro::UIInput>().setNextIndex(ButtonAwards, ButtonAwards);
        m_tabButtons[TabID::History].getComponent<cro::UIInput>().setPrevIndex(ButtonPerformance, ButtonPerformance);

        m_tabButtons[TabID::Awards].getComponent<cro::UIInput>().setNextIndex(ButtonPerformance, ButtonPerformance);
        m_tabButtons[TabID::Awards].getComponent<cro::UIInput>().setPrevIndex(ButtonHistory, ButtonHistory);

        selectNext(ButtonPerformance);
        break;
    case TabID::Performance:
        m_tabButtons[TabID::ClubStats].getComponent<cro::UIInput>().setNextIndex(ButtonHistory, Perf01);
        m_tabButtons[TabID::ClubStats].getComponent<cro::UIInput>().setPrevIndex(ButtonAwards, PerfCPU);

        m_tabButtons[TabID::History].getComponent<cro::UIInput>().setNextIndex(ButtonAwards, PerfNextCourse);
        m_tabButtons[TabID::History].getComponent<cro::UIInput>().setPrevIndex(ButtonClubset, PerfNextProf);

        m_tabButtons[TabID::Awards].getComponent<cro::UIInput>().setNextIndex(ButtonClubset, PerfDate);
        m_tabButtons[TabID::Awards].getComponent<cro::UIInput>().setPrevIndex(ButtonHistory, PerfDate);
        
        selectNext(ButtonHistory);
        break;
    case TabID::History:
        m_tabButtons[TabID::ClubStats].getComponent<cro::UIInput>().setNextIndex(ButtonPerformance, ButtonPerformance);
        m_tabButtons[TabID::ClubStats].getComponent<cro::UIInput>().setPrevIndex(ButtonAwards, ButtonAwards);

        m_tabButtons[TabID::Performance].getComponent<cro::UIInput>().setNextIndex(ButtonAwards, ButtonAwards);
        m_tabButtons[TabID::Performance].getComponent<cro::UIInput>().setPrevIndex(ButtonClubset, ButtonClubset);

        m_tabButtons[TabID::Awards].getComponent<cro::UIInput>().setNextIndex(ButtonClubset, ButtonClubset);
        m_tabButtons[TabID::Awards].getComponent<cro::UIInput>().setPrevIndex(ButtonPerformance, ButtonPerformance);

        selectNext(ButtonAwards);
        break;
    case TabID::Awards:
        m_tabButtons[TabID::ClubStats].getComponent<cro::UIInput>().setNextIndex(ButtonPerformance, ButtonPerformance);
        m_tabButtons[TabID::ClubStats].getComponent<cro::UIInput>().setPrevIndex(ButtonHistory, ButtonHistory);
        
        m_tabButtons[TabID::Performance].getComponent<cro::UIInput>().setNextIndex(ButtonHistory, AwardPrevious);
        m_tabButtons[TabID::Performance].getComponent<cro::UIInput>().setPrevIndex(ButtonClubset, AwardPrevious);

        m_tabButtons[TabID::History].getComponent<cro::UIInput>().setNextIndex(ButtonClubset, AwardNext);
        m_tabButtons[TabID::History].getComponent<cro::UIInput>().setPrevIndex(ButtonPerformance, AwardNext);

        selectNext(ButtonClubset);
        break;
    }*/
}

void LeagueState::quitState()
{
    //m_scene.setSystemActive<cro::AudioPlayerSystem>(false);
    m_rootNode.getComponent<cro::Callback>().active = true;
    m_audioEnts[AudioID::Back].getComponent<cro::AudioEmitter>().play();
}
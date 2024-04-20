/*-----------------------------------------------------------------------

Matt Marchant 2021 - 2024
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

#include "ProfileState.hpp"
#include "SharedStateData.hpp"
#include "CommandIDs.hpp"
#include "GameConsts.hpp"
#include "TextAnimCallback.hpp"
#include "MessageIDs.hpp"
#include "BallSystem.hpp"
#include "SharedProfileData.hpp"
#include "CallbackData.hpp"
#include "PlayerColours.hpp"
#include "ProfileEnum.inl"
#include "../GolfGame.hpp"
#include "../Colordome-32.hpp"

#include <crogine/core/Window.hpp>
#include <crogine/core/GameController.hpp>
#include <crogine/core/Mouse.hpp>
#include <crogine/graphics/Image.hpp>
#include <crogine/gui/Gui.hpp>

#include <crogine/ecs/components/Transform.hpp>
#include <crogine/ecs/components/UIInput.hpp>
#include <crogine/ecs/components/CommandTarget.hpp>
#include <crogine/ecs/components/Callback.hpp>
#include <crogine/ecs/components/Sprite.hpp>
#include <crogine/ecs/components/SpriteAnimation.hpp>
#include <crogine/ecs/components/Text.hpp>
#include <crogine/ecs/components/Model.hpp>
#include <crogine/ecs/components/Camera.hpp>
#include <crogine/ecs/components/Drawable2D.hpp>
#include <crogine/ecs/components/AudioEmitter.hpp>
#include <crogine/ecs/components/ParticleEmitter.hpp>

#include <crogine/ecs/systems/UISystem.hpp>
#include <crogine/ecs/systems/CommandSystem.hpp>
#include <crogine/ecs/systems/CallbackSystem.hpp>
#include <crogine/ecs/systems/SpriteSystem2D.hpp>
#include <crogine/ecs/systems/SpriteAnimator.hpp>
#include <crogine/ecs/systems/TextSystem.hpp>
#include <crogine/ecs/systems/ParticleSystem.hpp>
#include <crogine/ecs/systems/CameraSystem.hpp>
#include <crogine/ecs/systems/RenderSystem2D.hpp>
#include <crogine/ecs/systems/SkeletalAnimator.hpp>
#include <crogine/ecs/systems/ModelRenderer.hpp>
#include <crogine/ecs/systems/AudioPlayerSystem.hpp>

#include <crogine/util/Easings.hpp>
#include <crogine/util/Random.hpp>
#include <crogine/gui/Gui.hpp>

#include <crogine/detail/glm/gtc/matrix_transform.hpp>

#include "../ErrorCheck.hpp"

#include <sstream>

namespace
{
    struct MenuID final
    {
        enum
        {
            Dummy, Main,

            //these are the colour palettes
            Hair, Skin, TopL, TopD,
            BottomL, BottomD,

            //BallThumb not used now...
            BallThumb, BallColour,

            //browser windows
            BallSelect, HairSelect
        };
    };
    
    constexpr std::uint32_t ThumbTextureScale = 4;

    //UI selection indices
    constexpr std::size_t PrevArrow = 2000;
    constexpr std::size_t NextArrow = 2001;
    constexpr std::size_t CloseButton = 2002;

    constexpr glm::uvec2 BallTexSize(96u, 110u);
    constexpr glm::uvec2 AvatarTexSize(130u, 202u);
    constexpr glm::uvec2 MugshotTexSize(192u, 96u);

    constexpr std::size_t ThumbColCount = 8;
    constexpr std::size_t ThumbRowCount = 4;
    constexpr glm::uvec2 BallThumbSize = BallTexSize / 2u;

    constexpr glm::vec3 CameraBasePosition({ -0.867f, 1.325f, -1.68f });
    constexpr glm::vec3 CameraZoomPosition({ -0.867f, 1.625f, -0.58f });
    const glm::vec3 CameraZoomVector = glm::normalize(CameraZoomPosition - CameraBasePosition);
    constexpr glm::vec3 MugCameraPosition({ -0.843f, 1.61f, -0.35f });

    const cro::String XboxString("LB/LT - RB/RT Rotate/Zoom");
    const cro::String PSString("L1/L2 - R1/R2 Rotate/Zoom");

    static constexpr std::size_t MaxBioChars = 512;
    constexpr cro::FloatRect BioCrop =
    {
        0.f, -104.f, 90.f, 104.f
    };

    constexpr std::size_t PaletteColumnCount = 8;
    constexpr std::array SwatchPositions =
    {
        glm::vec2(37.f, 171.f),
        glm::vec2(37.f, 139.f),
        glm::vec2(21.f, 107.f),
        glm::vec2(53.f, 107.f),
        glm::vec2(21.f, 74.f),
        glm::vec2(53.f, 74.f)
    };

    namespace Flyout
    {
        //8x5
        static constexpr glm::vec2 IconSize(8.f);
        static constexpr float IconPadding = 4.f;

        static constexpr float BgWidth = (PaletteColumnCount * (IconSize.x + IconPadding)) + IconPadding;
    }
}

ProfileState::ProfileState(cro::StateStack& ss, cro::State::Context ctx, SharedStateData& sd, SharedProfileData& sp)
    : cro::State        (ss, ctx),
    m_uiScene           (ctx.appInstance.getMessageBus(), 384u),
    m_modelScene        (ctx.appInstance.getMessageBus(), 1024), //just because someone might be daft enough to install ALL the workshop items
    m_sharedData        (sd),
    m_profileData       (sp),
    m_viewScale         (2.f),
    m_ballIndex         (0),
    m_ballHairIndex     (0),
    m_avatarIndex       (0),
    m_lockedAvatarCount (0),
    m_lastSelected      (0),
    m_mugshotUpdated    (false)
{
    ctx.mainWindow.setMouseCaptured(false);

    m_activeProfile = sp.playerProfiles[sp.activeProfileIndex];

    addSystems();
    loadResources();
    buildPreviewScene();
    buildScene();

    m_modelScene.simulate(0.f);
    m_uiScene.simulate(0.f);

    //registerWindow([&]()
    //    {
    //        if (ImGui::Begin("Flaps"))
    //        {
    //            if (m_ballThumbs.available())
    //            {
    //                glm::vec2 size(m_ballThumbs.getSize());
    //                ImGui::Image(m_ballThumbs.getTexture(), { size.x, size.y }, { 0.f, 1.f }, { 1.f, 0.f });
    //            }
    //            
    //            /*if (m_mugshotTexture.available())
    //            {
    //                ImGui::Image(m_mugshotTexture.getTexture(), { 192.f, 96.f }, { 0.f, 1.f }, { 1.f, 0.f });
    //            }*/

    //            /*auto pos = m_cameras[CameraID::Mugshot].getComponent<cro::Transform>().getPosition();
    //            if (ImGui::SliderFloat("Height", &pos.y, 0.f, 2.f))
    //            {
    //                m_cameras[CameraID::Mugshot].getComponent<cro::Transform>().setPosition(pos);
    //            }
    //            if (ImGui::SliderFloat("Depth", &pos.z, -2.f, 4.f))
    //            {
    //                m_cameras[CameraID::Mugshot].getComponent<cro::Transform>().setPosition(pos);
    //            }*/
    //        }
    //        ImGui::End();
    //    });
}

//public
bool ProfileState::handleEvent(const cro::Event& evt)
{
    if (ImGui::GetIO().WantCaptureKeyboard
        || ImGui::GetIO().WantCaptureMouse
        || m_menuEntities[EntityID::Root].getComponent<cro::Callback>().active)
    {
        return false;
    }

    const auto updateHelpString = [&](std::int32_t controllerID)
    {
        if (controllerID > -1)
        {
            if (cro::GameController::hasPSLayout(controllerID))
            {
                m_menuEntities[EntityID::HelpText].getComponent<cro::Text>().setString(PSString);
            }
            else
            {
                m_menuEntities[EntityID::HelpText].getComponent<cro::Text>().setString(XboxString);
            }
        }
        else
        {
            cro::String str(cro::Keyboard::keyString(m_sharedData.inputBinding.keys[InputBinding::Left]));
            str += ", ";
            str += cro::Keyboard::keyString(m_sharedData.inputBinding.keys[InputBinding::Right]);
            str += ", ";
            str += cro::Keyboard::keyString(m_sharedData.inputBinding.keys[InputBinding::Up]);
            str += ", ";
            str += cro::Keyboard::keyString(m_sharedData.inputBinding.keys[InputBinding::Down]);
            str += " Rotate/Zoom";
            m_menuEntities[EntityID::HelpText].getComponent<cro::Text>().setString(str);
        }
        centreText(m_menuEntities[EntityID::HelpText]);
    };

    const auto quitMenu = [&]()
        {
            const auto groupID = m_uiScene.getSystem<cro::UISystem>()->getActiveGroup();

            if (groupID == MenuID::Main)
            {
                quitState();
            }
            else if (groupID == MenuID::BallSelect)
            {
                m_menuEntities[EntityID::BallBrowser].getComponent<cro::Callback>().active = true;
                m_audioEnts[AudioID::Back].getComponent<cro::AudioEmitter>().play();
            }
            else if (groupID == MenuID::HairSelect)
            {
                m_menuEntities[EntityID::HairBrowser].getComponent<cro::Callback>().active = true;
                m_audioEnts[AudioID::Back].getComponent<cro::AudioEmitter>().play();
            }
        };

    if (evt.type == SDL_KEYUP)
    {
        handleTextEdit(evt);
        switch (evt.key.keysym.sym)
        {
        default:
            updateHelpString(-1);
            break;
        case SDLK_ESCAPE:
        case SDLK_BACKSPACE:
        if (m_textEdit.string == nullptr)
        {
            quitMenu();
            return false;
        }
            break;
        case SDLK_RETURN:
        case SDLK_RETURN2:
        case SDLK_KP_ENTER:
            if (m_textEdit.string)
            {
                applyTextEdit();
            }
            break;
        //case SDLK_k:
        //    m_menuEntities[EntityID::BioText].getComponent<cro::Callback>().getUserData<std::int32_t>()++;
        //    m_menuEntities[EntityID::BioText].getComponent<cro::Callback>().active = true;
        //    break;
        //case SDLK_l:
        //    m_menuEntities[EntityID::BioText].getComponent<cro::Callback>().getUserData<std::int32_t>()--;
        //    m_menuEntities[EntityID::BioText].getComponent<cro::Callback>().active = true;
        //    break;
        }
    }
    else if (evt.type == SDL_KEYDOWN)
    {
        handleTextEdit(evt);
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
    else if (evt.type == SDL_TEXTINPUT)
    {
        handleTextEdit(evt);
    }
    else if (evt.type == SDL_CONTROLLERBUTTONUP)
    {
        cro::App::getWindow().setMouseCaptured(true);

        if (m_textEdit.string == nullptr)
        {
            switch (evt.cbutton.button)
            {
            default: break;
            case cro::GameController::ButtonRightShoulder:
            {
                const auto group = m_uiScene.getSystem<cro::UISystem>()->getActiveGroup();
                if (group == MenuID::BallSelect)
                {
                    nextPage(PaginationID::Balls);
                }
                else if (group == MenuID::HairSelect)
                {
                    nextPage(PaginationID::Hair);
                }
            }
                break;
            case cro::GameController::ButtonLeftShoulder:
            {
                const auto group = m_uiScene.getSystem<cro::UISystem>()->getActiveGroup();
                if (group == MenuID::BallSelect)
                {
                    prevPage(PaginationID::Balls);
                }
                else if (group == MenuID::HairSelect)
                {
                    prevPage(PaginationID::Hair);
                }
            }
                break;
            case cro::GameController::ButtonB:
                quitMenu();
                return false;
            }
        }
    }
    else if (evt.type == SDL_MOUSEBUTTONUP)
    {
        auto currentMenu = m_uiScene.getSystem<cro::UISystem>()->getActiveGroup();

        switch (currentMenu)
        {
        default:
        {
            updateHelpString(-1);
            if (evt.button.button == SDL_BUTTON_RIGHT)
            {
                quitState();
                return false;
            }
            else if (evt.button.button == SDL_BUTTON_LEFT)
            {
                if (applyTextEdit())
                {
                    //we applied a text edit so don't update the
                    //UISystem
                    return false;
                }
            }
        }
            break;
        case MenuID::BallSelect:
            if (evt.button.button == SDL_BUTTON_RIGHT)
            {
                m_menuEntities[EntityID::BallBrowser].getComponent<cro::Callback>().active = true;
                m_audioEnts[AudioID::Back].getComponent<cro::AudioEmitter>().play();
            }
            break;
        case MenuID::HairSelect:
            if (evt.button.button == SDL_BUTTON_RIGHT)
            {
                m_menuEntities[EntityID::HairBrowser].getComponent<cro::Callback>().active = true;
                m_audioEnts[AudioID::Back].getComponent<cro::AudioEmitter>().play();
            }
            break;
        case MenuID::BallColour:
        {
            auto bounds = m_ballColourFlyout.background.getComponent<cro::Drawable2D>().getLocalBounds();
            bounds = m_ballColourFlyout.background.getComponent<cro::Transform>().getWorldTransform() * bounds;

            if (!bounds.contains(m_uiScene.getActiveCamera().getComponent<cro::Camera>().pixelToCoords(cro::Mouse::getPosition())))
            {
                m_ballColourFlyout.background.getComponent<cro::Transform>().setScale(glm::vec2(0.f));
                m_uiScene.getSystem<cro::UISystem>()->setActiveGroup(MenuID::Main);
                m_uiScene.getSystem<cro::UISystem>()->setColumnCount(1);
                m_uiScene.getSystem<cro::UISystem>()->selectAt(m_lastSelected);

                m_ballModels[m_ballIndex].ball.getComponent<cro::Model>().setMaterialProperty(0, "u_ballColour", m_activeProfile.ballColour);
                return false;
            }
        }
            break;
        case MenuID::BallThumb:
        {
            auto bounds = m_flyouts[PaletteID::BallThumb].background.getComponent<cro::Drawable2D>().getLocalBounds();
            bounds = m_flyouts[PaletteID::BallThumb].background.getComponent<cro::Transform>().getWorldTransform() * bounds;

            if (!bounds.contains(m_uiScene.getActiveCamera().getComponent<cro::Camera>().pixelToCoords(cro::Mouse::getPosition())))
            {
                m_flyouts[PaletteID::BallThumb].background.getComponent<cro::Transform>().setScale(glm::vec2(0.f));
                m_uiScene.getSystem<cro::UISystem>()->setActiveGroup(MenuID::Main);
                m_uiScene.getSystem<cro::UISystem>()->setColumnCount(1);
                m_uiScene.getSystem<cro::UISystem>()->selectAt(m_lastSelected);
                return false;
            }
        }
        break;
        case MenuID::Hair:
        case MenuID::BottomD:
        case MenuID::BottomL:
        case MenuID::Skin:
        case MenuID::TopL:
        case MenuID::TopD:
        {
            auto flyoutID = currentMenu - MenuID::Hair;
            auto bounds = m_flyouts[flyoutID].background.getComponent<cro::Drawable2D>().getLocalBounds();
            bounds = m_flyouts[flyoutID].background.getComponent<cro::Transform>().getWorldTransform() * bounds;

            if (!bounds.contains(m_uiScene.getActiveCamera().getComponent<cro::Camera>().pixelToCoords(cro::Mouse::getPosition())))
            {
                m_flyouts[flyoutID].background.getComponent<cro::Transform>().setScale(glm::vec2(0.f));
                m_uiScene.getSystem<cro::UISystem>()->setActiveGroup(MenuID::Main);
                m_uiScene.getSystem<cro::UISystem>()->setColumnCount(1);
                m_uiScene.getSystem<cro::UISystem>()->selectAt(m_lastSelected);

                //restore model preview colours
                m_profileTextures[m_avatarIndex].setColour(pc::ColourKey::Index(flyoutID), m_activeProfile.avatarFlags[flyoutID]);
                m_profileTextures[m_avatarIndex].apply();

                //refresh hair colour
                if (m_avatarHairModels[m_avatarModels[m_avatarIndex].hairIndex].isValid())
                {
                    m_avatarHairModels[m_avatarModels[m_avatarIndex].hairIndex].getComponent<cro::Model>().setMaterialProperty(0, "u_hairColour", pc::Palette[m_activeProfile.avatarFlags[0]]);
                }

                //don't forward this to the menu system
                return false;
            }
        }
            break;
        }        
    }
    else if (evt.type == SDL_CONTROLLERAXISMOTION)
    {
        if (evt.caxis.value > LeftThumbDeadZone)
        {
            cro::App::getWindow().setMouseCaptured(true);

            updateHelpString(cro::GameController::controllerID(evt.caxis.which));
        }
    }
    else if (evt.type == SDL_MOUSEMOTION)
    {
        cro::App::getWindow().setMouseCaptured(false);
        updateHelpString(-1);

        auto mousePos = m_uiScene.getActiveCamera().getComponent<cro::Camera>().pixelToCoords({ evt.motion.x, evt.motion.y });
        auto bounds = m_menuEntities[EntityID::AvatarPreview].getComponent<cro::Sprite>().getTextureBounds();
        bounds = m_menuEntities[EntityID::AvatarPreview].getComponent<cro::Transform>().getWorldTransform() * bounds;

        if (bounds.contains(mousePos)
            && cro::Mouse::isButtonPressed(cro::Mouse::Button::Left))
        {
            m_avatarModels[m_avatarIndex].previewModel.getComponent<cro::Transform>().rotate(cro::Transform::Y_AXIS, static_cast<float>(evt.motion.xrel) * (1.f/60.f));
        }
    }
    else if (evt.type == SDL_MOUSEWHEEL)
    {
        const auto group = m_uiScene.getSystem<cro::UISystem>()->getActiveGroup();
        if (group == MenuID::BallSelect)
        {
            if (evt.wheel.y > 0)
            {
                prevPage(PaginationID::Balls);
            }
            else if(evt.wheel.y < 0)
            {
                nextPage(PaginationID::Balls);
            }
        }
        else if (group == MenuID::HairSelect)
        {
            if (evt.wheel.y > 0)
            {
                prevPage(PaginationID::Hair);
            }
            else if (evt.wheel.y < 0)
            {
                nextPage(PaginationID::Hair);
            }
        }
        else
        {
            auto mousePos = m_uiScene.getActiveCamera().getComponent<cro::Camera>().pixelToCoords(cro::Mouse::getPosition());
            auto bounds = m_menuEntities[EntityID::AvatarPreview].getComponent<cro::Sprite>().getTextureBounds();
            bounds = m_menuEntities[EntityID::AvatarPreview].getComponent<cro::Transform>().getWorldTransform() * bounds;

            if (bounds.contains(mousePos))
            {
                float zoom = evt.wheel.preciseY * (1.f / 20.f);
                auto pos = m_cameras[CameraID::Avatar].getComponent<cro::Transform>().getPosition();
                if (glm::dot(CameraZoomPosition - pos, CameraZoomVector) > zoom
                    && glm::dot(CameraBasePosition - pos, CameraZoomVector) < zoom)
                {
                    pos += CameraZoomVector * zoom;
                    m_cameras[CameraID::Avatar].getComponent<cro::Transform>().setPosition(pos);
                }
            }
            else
            {
                //check bio for scroll
                bounds = cro::Text::getLocalBounds(m_menuEntities[EntityID::BioText]);
                bounds = m_menuEntities[EntityID::BioText].getComponent<cro::Transform>().getWorldTransform() * bounds;

                if (bounds.contains(mousePos))
                {
                    m_menuEntities[EntityID::BioText].getComponent<cro::Callback>().getUserData<std::int32_t>() -= evt.wheel.y;
                    m_menuEntities[EntityID::BioText].getComponent<cro::Callback>().active = true;
                }
            }
        }
    }

    m_uiScene.getSystem<cro::UISystem>()->handleEvent(evt);
    m_uiScene.forwardEvent(evt);
    m_modelScene.forwardEvent(evt);
    return false;
}

void ProfileState::handleMessage(const cro::Message& msg)
{
    if (msg.id == cro::Message::StateMessage)
    {
        const auto& data = msg.getData<cro::Message::StateEvent>();
        if (data.action == cro::Message::StateEvent::Popped)
        {
            switch (data.id)
            {
            default: break;
            case StateID::Keyboard:
                applyTextEdit();
                break;
            }
        }
    }

    m_uiScene.forwardMessage(msg);
    m_modelScene.forwardMessage(msg);
}

bool ProfileState::simulate(float dt)
{
    //rotate/zoom avatar
    if (!m_textEdit.entity.isValid())
    {
        float rotation = 0.f;

        if (cro::GameController::isButtonPressed(0, cro::GameController::ButtonLeftShoulder)
            || cro::Keyboard::isKeyPressed(m_sharedData.inputBinding.keys[InputBinding::Left]))
        {
            rotation -= dt;
        }
        if (cro::GameController::isButtonPressed(0, cro::GameController::ButtonRightShoulder)
            || cro::Keyboard::isKeyPressed(m_sharedData.inputBinding.keys[InputBinding::Right]))
        {
            rotation += dt;
        }
        m_avatarModels[m_avatarIndex].previewModel.getComponent<cro::Transform>().rotate(cro::Transform::Y_AXIS, rotation);


        float zoom = 0.f;
        if (cro::GameController::getAxisPosition(0, cro::GameController::TriggerLeft) > TriggerDeadZone
            || cro::Keyboard::isKeyPressed(m_sharedData.inputBinding.keys[InputBinding::Down]))
        {
            zoom -= dt;
        }
        if (cro::GameController::getAxisPosition(0, cro::GameController::TriggerRight) > TriggerDeadZone
            || cro::Keyboard::isKeyPressed(m_sharedData.inputBinding.keys[InputBinding::Up]))
        {
            zoom += dt;
        }
        if (zoom != 0)
        {
            auto pos = m_cameras[CameraID::Avatar].getComponent<cro::Transform>().getPosition();
            if (glm::dot(CameraZoomPosition - pos, CameraZoomVector) > zoom
                && glm::dot(CameraBasePosition - pos, CameraZoomVector) < zoom)
            {
                pos += CameraZoomVector * zoom;
                m_cameras[CameraID::Avatar].getComponent<cro::Transform>().setPosition(pos);
            }
        }
    }

    m_modelScene.simulate(dt);
    m_uiScene.simulate(dt);
    return true;
}

void ProfileState::render()
{
    m_ballTexture.clear(cro::Colour::Transparent);
    m_modelScene.setActiveCamera(m_cameras[CameraID::Ball]);
    m_modelScene.render();
    m_ballTexture.display();

    m_avatarTexture.clear(cro::Colour::Transparent);
    m_modelScene.setActiveCamera(m_cameras[CameraID::Avatar]);
    m_modelScene.render();
    m_avatarTexture.display();

    m_uiScene.render();

}

//private
void ProfileState::addSystems()
{
    auto& mb = getContext().appInstance.getMessageBus();
    m_uiScene.addSystem<cro::UISystem>(mb)->setMouseScrollNavigationEnabled(false);
    m_uiScene.addSystem<cro::CommandSystem>(mb);
    m_uiScene.addSystem<cro::CallbackSystem>(mb);
    m_uiScene.addSystem<cro::SpriteSystem2D>(mb);
    m_uiScene.addSystem<cro::SpriteAnimator>(mb);
    m_uiScene.addSystem<cro::TextSystem>(mb);
    m_uiScene.addSystem<cro::CameraSystem>(mb);
    m_uiScene.addSystem<cro::RenderSystem2D>(mb);
    m_uiScene.addSystem<cro::AudioPlayerSystem>(mb);

    m_uiScene.setSystemActive<cro::AudioPlayerSystem>(false);

    m_modelScene.addSystem<cro::CallbackSystem>(mb);
    m_modelScene.addSystem<cro::SkeletalAnimator>(mb);
    m_modelScene.addSystem<cro::CameraSystem>(mb);
    m_modelScene.addSystem<cro::ModelRenderer>(mb);
    m_modelScene.addSystem<cro::ParticleSystem>(mb);

    //m_uiScene.getSystem<cro::UISystem>()->initDebug("Profile UI");
}

void ProfileState::loadResources()
{
    //button audio
    m_menuSounds.loadFromFile("assets/golf/sound/menu.xas", m_resources.audio);
    m_audioEnts[AudioID::Accept] = m_uiScene.createEntity();
    m_audioEnts[AudioID::Accept].addComponent<cro::AudioEmitter>() = m_menuSounds.getEmitter("accept");
    m_audioEnts[AudioID::Back] = m_uiScene.createEntity();
    m_audioEnts[AudioID::Back].addComponent<cro::AudioEmitter>() = m_menuSounds.getEmitter("back");
    m_audioEnts[AudioID::Snap] = m_uiScene.createEntity();
    m_audioEnts[AudioID::Snap].addComponent<cro::AudioEmitter>() = m_menuSounds.getEmitter("snapshot");

    m_audioEnts[AudioID::Select] = m_uiScene.createEntity();
    m_audioEnts[AudioID::Select].addComponent<cro::AudioEmitter>() = m_menuSounds.getEmitter("switch");

    m_audioEnts[AudioID::Accept].getComponent<cro::AudioEmitter>().play();


    m_mugshotTexture.create(MugshotTexSize.x, MugshotTexSize.y);
}

void ProfileState::buildScene()
{
    struct RootCallbackData final
    {
        enum
        {
            FadeIn, FadeOut
        }state = FadeIn;
        float currTime = 0.f;
    };

    auto rootNode = m_uiScene.createEntity();
    rootNode.addComponent<cro::Transform>();
    rootNode.addComponent<cro::CommandTarget>().ID = CommandID::Menu::RootNode;
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

                m_uiScene.setSystemActive<cro::AudioPlayerSystem>(true);
                m_uiScene.getSystem<cro::UISystem>()->setActiveGroup(MenuID::Main);

                //assume we launched from a cached state and update
                //local profile data if necessary
                if (m_activeProfile.profileID != m_profileData.playerProfiles[m_profileData.activeProfileIndex].profileID)
                {
                    m_activeProfile = m_profileData.playerProfiles[m_profileData.activeProfileIndex];

                    //refresh the avatar settings
                    setAvatarIndex(indexFromAvatarID(m_activeProfile.skinID));
                    setHairIndex(indexFromHairID(m_activeProfile.hairID));
                    setBallIndex(indexFromBallID(m_activeProfile.ballID) % m_ballModels.size());
                    refreshMugshot();
                    refreshNameString();
                    refreshSwatch();
                }
                m_ballModels[m_ballIndex].ball.getComponent<cro::Model>().setMaterialProperty(0, "u_ballColour", m_activeProfile.ballColour);
                refreshBio();
            }
            break;
        case RootCallbackData::FadeOut:
            currTime = std::max(0.f, currTime - (dt * 2.f));
            e.getComponent<cro::Transform>().setScale(m_viewScale * cro::Util::Easing::easeOutQuint(currTime));
            if (currTime == 0)
            {
                requestStackPop();

                state = RootCallbackData::FadeIn;
                m_uiScene.getSystem<cro::UISystem>()->setActiveGroup(MenuID::Dummy);
                m_uiScene.setSystemActive<cro::AudioPlayerSystem>(false);
            }
            break;
        }

    };

    m_menuEntities[EntityID::Root] = rootNode;


    //quad to darken the screen
    auto entity = m_uiScene.createEntity();
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

    //prevents input when text input is active.
    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>();
    entity.addComponent<cro::UIInput>().setGroup(MenuID::Dummy);


    //background
    cro::SpriteSheet spriteSheet;
    spriteSheet.loadFromFile("assets/golf/sprites/avatar_edit.spt", m_resources.textures);

    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 0.f, 10.f, -0.2f });
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>() = spriteSheet.getSprite("background");
    auto bounds = entity.getComponent<cro::Sprite>().getTextureBounds();
    entity.getComponent<cro::Transform>().setOrigin({ bounds.width / 2.f, bounds.height / 2.f });
    rootNode.getComponent<cro::Transform >().addChild(entity.getComponent<cro::Transform>());

    auto bgEnt = entity;

    auto& largeFont = m_sharedData.sharedResources->fonts.get(FontID::UI);

    //active profile name
    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 382.f, 226.f, 0.1f });
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Text>(largeFont).setString(m_activeProfile.name);
    entity.getComponent<cro::Text>().setFillColour(TextNormalColour);
    entity.getComponent<cro::Text>().setCharacterSize(UITextSize);
    entity.addComponent<cro::Callback>().function =
        [&](cro::Entity e, float)
    {
        if (m_textEdit.string != nullptr)
        {
            auto str = *m_textEdit.string;
            if (str.size() == 0)
            {
                str += "_";
            }
            e.getComponent<cro::Text>().setString(str);

            centreText(e);
            /*bounds = cro::Text::getLocalBounds(e);
            bounds.left = (bounds.width - NameWidth) / 2.f;
            bounds.width = NameWidth;
            e.getComponent<cro::Drawable2D>().setCroppingArea(bounds);*/
        }
    };
    centreText(entity);
    bgEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
    m_menuEntities[EntityID::NameText] = entity;

    auto& uiSystem = *m_uiScene.getSystem<cro::UISystem>();
    auto selected = uiSystem.addCallback([&](cro::Entity e)
        {
            e.getComponent<cro::Sprite>().setColour(cro::Colour::White);
            e.getComponent<cro::Callback>().active = true;
            m_audioEnts[AudioID::Accept].getComponent<cro::AudioEmitter>().play();

            const auto& str = e.getLabel();
            if (str.empty())
            {
                m_menuEntities[EntityID::TipText].getComponent<cro::Text>().setString(" ");
            }
            else
            {
                m_menuEntities[EntityID::TipText].getComponent<cro::Text>().setString(str);
            }
        });
    auto unselected = uiSystem.addCallback([&](cro::Entity e)
        {
            e.getComponent<cro::Sprite>().setColour(cro::Colour::Transparent); 
            m_menuEntities[EntityID::TipText].getComponent<cro::Text>().setString(" ");
        });

    const auto createButton = [&](const std::string& spriteID, glm::vec2 position, std::int32_t selectionIndex)
    {
        auto bounds = spriteSheet.getSprite(spriteID).getTextureBounds();

        auto entity = m_uiScene.createEntity();
        entity.addComponent<cro::Transform>().setPosition(glm::vec3(position, 0.4f));
        entity.getComponent<cro::Transform>().setOrigin({ std::floor(bounds.width / 2.f), std::floor(bounds.height / 2.f) });
        entity.getComponent<cro::Transform>().move(entity.getComponent<cro::Transform>().getOrigin());
        entity.addComponent<cro::Drawable2D>();
        entity.addComponent<cro::Sprite>() = spriteSheet.getSprite(spriteID);
        entity.getComponent<cro::Sprite>().setColour(cro::Colour::Transparent);
        entity.addComponent<cro::Callback>().function = MenuTextCallback();
        entity.addComponent<cro::UIInput>().area = bounds;
        entity.getComponent<cro::UIInput>().setGroup(MenuID::Main);
        entity.getComponent<cro::UIInput>().setSelectionIndex(selectionIndex);
        entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Selected] = selected;
        entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Unselected] = unselected;
        bgEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());

        return entity;
    };

#ifdef USE_GNS
    //add workshop button
    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 17.f, 202.f, 0.1f });
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>() = spriteSheet.getSprite("workshop_button");
    bgEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());

    entity = createButton("workshop_highlight", { 15.f, 200.f }, ButtonWorkshop);
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonUp] =
        uiSystem.addCallback([&](cro::Entity e, const cro::ButtonEvent& evt) 
            {
                if (activated(evt))
                {
                    Social::showWorkshop();
                }
            });

    entity.getComponent<cro::UIInput>().setNextIndex(ButtonHairBrowse, ButtonHairColour);
    entity.getComponent<cro::UIInput>().setPrevIndex(ButtonName, ButtonRandomise);
    entity.setLabel("Open Steam Workshop In Overlay");
#endif

    //colour swatch - this has its own update function
    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>();
    entity.addComponent<cro::Drawable2D>().setPrimitiveType(GL_TRIANGLES);
    bgEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
    m_menuEntities[EntityID::Swatch] = entity;
    refreshSwatch();

    auto fetchUIIndexFromColour =
        [&](std::uint8_t colourIndex, std::int32_t paletteIndex) 
    {
        const auto rowCount = m_uiScene.getSystem<cro::UISystem>()->getGroupSize() / PaletteColumnCount;
        const auto x = colourIndex % PaletteColumnCount;
        auto y = colourIndex / PaletteColumnCount;

        y = (rowCount - 1) - y;
        return (y * PaletteColumnCount + x) % pc::PairCounts[paletteIndex];
    };

    //colour buttons
    auto hairColour = createButton("colour_highlight", glm::vec2(33.f, 167.f), ButtonHairColour);
    hairColour.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonUp] =
        uiSystem.addCallback([&, fetchUIIndexFromColour](cro::Entity e, const cro::ButtonEvent& evt)
            {
                if (activated(evt))
                {
                    applyTextEdit();

                    //show palette menu
                    m_flyouts[PaletteID::Hair].background.getComponent<cro::Transform>().setScale(glm::vec2(1.f));

                    //set column/row count for menu
                    m_uiScene.getSystem<cro::UISystem>()->setActiveGroup(MenuID::Hair);
                    m_uiScene.getSystem<cro::UISystem>()->setColumnCount(PaletteColumnCount);
                    m_uiScene.getSystem<cro::UISystem>()->selectAt(fetchUIIndexFromColour(m_activeProfile.avatarFlags[0], pc::ColourKey::Hair));

                    m_audioEnts[AudioID::Accept].getComponent<cro::AudioEmitter>().play();

                    m_lastSelected = e.getComponent<cro::UIInput>().getSelectionIndex();
                }
            });
    hairColour.getComponent<cro::UIInput>().setNextIndex(ButtonPrevHair, ButtonSkinTone);
#ifdef USE_GNS
    hairColour.getComponent<cro::UIInput>().setPrevIndex(ButtonDescUp, ButtonWorkshop);
#else
    hairColour.getComponent<cro::UIInput>().setPrevIndex(ButtonDescUp, ButtonRandomise);
#endif

    auto skinColour = createButton("colour_highlight", glm::vec2(33.f, 135.f), ButtonSkinTone);
    skinColour.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonUp] =
        uiSystem.addCallback([&, fetchUIIndexFromColour](cro::Entity e, const cro::ButtonEvent& evt)
            {
                if (activated(evt))
                {
                    applyTextEdit();

                    m_flyouts[PaletteID::Skin].background.getComponent<cro::Transform>().setScale(glm::vec2(1.f));

                    m_uiScene.getSystem<cro::UISystem>()->setActiveGroup(MenuID::Skin);
                    m_uiScene.getSystem<cro::UISystem>()->setColumnCount(PaletteColumnCount);
                    m_uiScene.getSystem<cro::UISystem>()->selectAt(fetchUIIndexFromColour(m_activeProfile.avatarFlags[1], pc::ColourKey::Skin));

                    m_audioEnts[AudioID::Accept].getComponent<cro::AudioEmitter>().play();

                    m_lastSelected = e.getComponent<cro::UIInput>().getSelectionIndex();
                }
            });
    skinColour.getComponent<cro::UIInput>().setNextIndex(ButtonPrevHair, ButtonTopLight);
    skinColour.getComponent<cro::UIInput>().setPrevIndex(ButtonNextBall, ButtonHairColour);


    auto topLightColour = createButton("colour_highlight", glm::vec2(17.f, 103.f), ButtonTopLight);
    topLightColour.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonUp] =
        uiSystem.addCallback([&, fetchUIIndexFromColour](cro::Entity e, const cro::ButtonEvent& evt)
            {
                if (activated(evt))
                {
                    applyTextEdit();

                    m_flyouts[PaletteID::TopL].background.getComponent<cro::Transform>().setScale(glm::vec2(1.f));

                    m_uiScene.getSystem<cro::UISystem>()->setActiveGroup(MenuID::TopL);
                    m_uiScene.getSystem<cro::UISystem>()->setColumnCount(PaletteColumnCount);
                    m_uiScene.getSystem<cro::UISystem>()->selectAt(fetchUIIndexFromColour(m_activeProfile.avatarFlags[2], pc::ColourKey::TopLight));

                    m_audioEnts[AudioID::Accept].getComponent<cro::AudioEmitter>().play();

                    m_lastSelected = e.getComponent<cro::UIInput>().getSelectionIndex();
                }
            });
    topLightColour.getComponent<cro::UIInput>().setNextIndex(ButtonTopDark, ButtonBottomLight);
    topLightColour.getComponent<cro::UIInput>().setPrevIndex(ButtonNextBall, ButtonSkinTone);


    auto topDarkColour = createButton("colour_highlight", glm::vec2(49.f, 103.f), ButtonTopDark);
    topDarkColour.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonUp] =
        uiSystem.addCallback([&, fetchUIIndexFromColour](cro::Entity e, const cro::ButtonEvent& evt)
            {
                if (activated(evt))
                {
                    applyTextEdit();

                    m_flyouts[PaletteID::TopD].background.getComponent<cro::Transform>().setScale(glm::vec2(1.f));

                    m_uiScene.getSystem<cro::UISystem>()->setActiveGroup(MenuID::TopD);
                    m_uiScene.getSystem<cro::UISystem>()->setColumnCount(PaletteColumnCount);
                    m_uiScene.getSystem<cro::UISystem>()->selectAt(fetchUIIndexFromColour(m_activeProfile.avatarFlags[3], pc::ColourKey::TopDark));

                    m_audioEnts[AudioID::Accept].getComponent<cro::AudioEmitter>().play();

                    m_lastSelected = e.getComponent<cro::UIInput>().getSelectionIndex();
                }
            });
    topDarkColour.getComponent<cro::UIInput>().setNextIndex(ButtonPrevBody, ButtonBottomDark);
    topDarkColour.getComponent<cro::UIInput>().setPrevIndex(ButtonTopLight, ButtonSkinTone);


    auto bottomLightColour = createButton("colour_highlight", glm::vec2(17.f, 69.f), ButtonBottomLight);
    bottomLightColour.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonUp] =
        uiSystem.addCallback([&, fetchUIIndexFromColour](cro::Entity e, const cro::ButtonEvent& evt)
            {
                if (activated(evt))
                {
                    applyTextEdit();

                    m_flyouts[PaletteID::BotL].background.getComponent<cro::Transform>().setScale(glm::vec2(1.f));

                    m_uiScene.getSystem<cro::UISystem>()->setActiveGroup(MenuID::BottomL);
                    m_uiScene.getSystem<cro::UISystem>()->setColumnCount(PaletteColumnCount);
                    m_uiScene.getSystem<cro::UISystem>()->selectAt(fetchUIIndexFromColour(m_activeProfile.avatarFlags[4], pc::ColourKey::BottomLight));

                    m_audioEnts[AudioID::Accept].getComponent<cro::AudioEmitter>().play();

                    m_lastSelected = e.getComponent<cro::UIInput>().getSelectionIndex();
                }
            });
    bottomLightColour.getComponent<cro::UIInput>().setNextIndex(ButtonBottomDark, ButtonSouthPaw);
    bottomLightColour.getComponent<cro::UIInput>().setPrevIndex(ButtonDescDown, ButtonTopLight);

    auto bottomDarkColour = createButton("colour_highlight", glm::vec2(49.f, 69.f), ButtonBottomDark);
    bottomDarkColour.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonUp] =
        uiSystem.addCallback([&, fetchUIIndexFromColour](cro::Entity e, const cro::ButtonEvent& evt)
            {
                if (activated(evt))
                {
                    applyTextEdit();

                    m_flyouts[PaletteID::BotD].background.getComponent<cro::Transform>().setScale(glm::vec2(1.f));

                    m_uiScene.getSystem<cro::UISystem>()->setActiveGroup(MenuID::BottomD);
                    m_uiScene.getSystem<cro::UISystem>()->setColumnCount(PaletteColumnCount);
                    m_uiScene.getSystem<cro::UISystem>()->selectAt(fetchUIIndexFromColour(m_activeProfile.avatarFlags[5], pc::ColourKey::BottomDark));

                    m_audioEnts[AudioID::Accept].getComponent<cro::AudioEmitter>().play();

                    m_lastSelected = e.getComponent<cro::UIInput>().getSelectionIndex();
                }
            });
    bottomDarkColour.getComponent<cro::UIInput>().setNextIndex(ButtonDescDown, ButtonRandomise);
    bottomDarkColour.getComponent<cro::UIInput>().setPrevIndex(ButtonBottomLight, ButtonTopDark);


    //hat select button
    entity = m_uiScene.createEntity();
    entity.setLabel("Open Headwear Browser");
    entity.addComponent<cro::Transform>().setPosition({163.f, 214.f, 0.5f});
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>() = spriteSheet.getSprite("hat_select");
    bounds = entity.getComponent<cro::Sprite>().getTextureBounds();
    auto rect = entity.getComponent<cro::Sprite>().getTextureRect();
    entity.addComponent<cro::UIInput>().area = bounds;
    entity.getComponent<cro::UIInput>().setGroup(MenuID::Main);
    entity.getComponent<cro::UIInput>().setSelectionIndex(ButtonHairBrowse);
    entity.getComponent<cro::UIInput>().setNextIndex(ButtonNextHair, ButtonNextBody);
    entity.getComponent<cro::UIInput>().setPrevIndex(ButtonPrevHair, ButtonPrevBody);
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Unselected] = 
        uiSystem.addCallback([rect](cro::Entity e) {e.getComponent<cro::Sprite>().setTextureRect(rect); });
    rect.bottom += rect.height;
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Selected] =
        uiSystem.addCallback([&,rect](cro::Entity e)
            {
                e.getComponent<cro::Sprite>().setTextureRect(rect);
                m_audioEnts[AudioID::Select].getComponent<cro::AudioEmitter>().play();

                m_menuEntities[EntityID::TipText].getComponent<cro::Text>().setString(e.getLabel());
            });
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonUp] =
        uiSystem.addCallback([&](cro::Entity, const cro::ButtonEvent& evt) 
            {
                if (activated(evt))
                {
                    m_menuEntities[EntityID::HairBrowser].getComponent<cro::Callback>().active = true;
                }
            });
    entity.getComponent<cro::Transform>().setOrigin({ bounds.width / 2.f, bounds.height / 2.f });
    bgEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());


    const auto expandHitbox = 
        [](cro::Entity e)
        {
            auto bounds = e.getComponent<cro::UIInput>().area;
            bounds.bottom -= 16.f;
            bounds.height += 32.f;
            e.getComponent<cro::UIInput>().area = bounds;
        };

    //avatar arrow buttons
    auto hairLeft = createButton("arrow_left", glm::vec2(87.f, 156.f), ButtonPrevHair);
    hairLeft.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonUp] =
        uiSystem.addCallback([&](cro::Entity, const cro::ButtonEvent& evt)
            {
                if (activated(evt))
                {
                    auto hairIndex = (m_avatarModels[m_avatarIndex].hairIndex + (m_avatarHairModels.size() - 1)) % m_avatarHairModels.size();
                    setHairIndex(hairIndex);

                    m_audioEnts[AudioID::Back].getComponent<cro::AudioEmitter>().play();
                }
            });
    hairLeft.getComponent<cro::UIInput>().setNextIndex(ButtonHairBrowse, ButtonPrevBody);
    hairLeft.getComponent<cro::UIInput>().setPrevIndex(ButtonHairColour, ButtonPrevBody);
    hairLeft.setLabel("Previous Headwear");
    expandHitbox(hairLeft);

    auto hairRight = createButton("arrow_right", glm::vec2(234.f, 156.f), ButtonNextHair);
    hairRight.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonUp] =
        uiSystem.addCallback([&](cro::Entity, const cro::ButtonEvent& evt)
            {
                if (activated(evt))
                {
                    auto hairIndex = (m_avatarModels[m_avatarIndex].hairIndex + 1) % m_avatarHairModels.size();
                    setHairIndex(hairIndex);
                    m_audioEnts[AudioID::Back].getComponent<cro::AudioEmitter>().play();
                }
            });
    hairRight.getComponent<cro::UIInput>().setNextIndex(ButtonPrevBall, ButtonNextBody);
    hairRight.getComponent<cro::UIInput>().setPrevIndex(ButtonHairBrowse, ButtonNextBody);
    hairRight.setLabel("Next Headwear");
    expandHitbox(hairRight);

    auto avatarLeft = createButton("arrow_left", glm::vec2(87.f, 110.f), ButtonPrevBody);
    avatarLeft.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonUp] =
        uiSystem.addCallback([&](cro::Entity, const cro::ButtonEvent& evt)
            {
                if (activated(evt))
                {
                    setAvatarIndex((m_avatarIndex + (m_avatarModels.size() - 1)) % m_avatarModels.size());
                    m_audioEnts[AudioID::Back].getComponent<cro::AudioEmitter>().play();
                }
            });
    avatarLeft.getComponent<cro::UIInput>().setNextIndex(ButtonNextBody, ButtonPrevHair);
    avatarLeft.getComponent<cro::UIInput>().setPrevIndex(ButtonTopDark, ButtonPrevHair);
    avatarLeft.setLabel("Previous Avatar");
    expandHitbox(avatarLeft);

    auto avatarRight = createButton("arrow_right", glm::vec2(234.f, 110.f), ButtonNextBody);
    avatarRight.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonUp] =
        uiSystem.addCallback([&](cro::Entity, const cro::ButtonEvent& evt)
            {
                if (activated(evt))
                {
                    setAvatarIndex((m_avatarIndex + 1) % m_avatarModels.size());
                    m_audioEnts[AudioID::Back].getComponent<cro::AudioEmitter>().play();
                }
            });
    avatarRight.getComponent<cro::UIInput>().setNextIndex(ButtonPrevBall, ButtonNextHair);
    avatarRight.getComponent<cro::UIInput>().setPrevIndex(ButtonPrevBody, ButtonNextHair);
    avatarRight.setLabel("Next Avatar");
    expandHitbox(avatarRight);

    //southpaw checkbox
    auto southPaw = createButton("check_highlight", glm::vec2(17.f, 50.f), ButtonSouthPaw);
    southPaw.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonUp] =
        uiSystem.addCallback([&](cro::Entity, const cro::ButtonEvent& evt)
            {
                if (activated(evt))
                {
                    m_activeProfile.flipped = !m_activeProfile.flipped;
                    m_audioEnts[AudioID::Back].getComponent<cro::AudioEmitter>().play();
                }
            });
    southPaw.getComponent<cro::UIInput>().setNextIndex(ButtonSaveClose, ButtonRandomise);
    southPaw.getComponent<cro::UIInput>().setPrevIndex(ButtonSaveClose, ButtonBottomLight);
    southPaw.getComponent<cro::UIInput>().area.width *= 7.f;
    southPaw.setLabel("Use Left Handed Avatar");
    
    auto innerEnt = m_uiScene.createEntity();
    innerEnt.addComponent<cro::Transform>().setPosition(glm::vec3(19.f, 52.f, 0.1f));
    innerEnt.addComponent<cro::Drawable2D>().setVertexData(
        {
            cro::Vertex2D(glm::vec2(0.f, 5.f), cro::Colour::Transparent),
            cro::Vertex2D(glm::vec2(0.f), cro::Colour::Transparent),
            cro::Vertex2D(glm::vec2(5.f), cro::Colour::Transparent),
            cro::Vertex2D(glm::vec2(5.f, 0.f), cro::Colour::Transparent)
        }
    );
    innerEnt.getComponent<cro::Drawable2D>().updateLocalBounds();
    innerEnt.addComponent<cro::Callback>().active = true;
    innerEnt.getComponent<cro::Callback>().function =
        [&](cro::Entity e, float)
    {
        auto c = m_activeProfile.flipped ? TextGoldColour : cro::Colour::Transparent;
        for (auto& v : e.getComponent<cro::Drawable2D>().getVertexData())
        {
            v.colour = c;
        }
    };
    bgEnt.getComponent<cro::Transform>().addChild(innerEnt.getComponent<cro::Transform>());

    //randomise button
    auto randomButton = createButton("random_highlight", { 9.f, 24.f }, ButtonRandomise);
    randomButton.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonUp] =
        uiSystem.addCallback([&](cro::Entity e, const cro::ButtonEvent& evt)
            {
                if (activated(evt))
                {
                    //randomise hair
                    setHairIndex(cro::Util::Random::value(0u, m_avatarHairModels.size() - 1));

                    //randomise avatar
                    setAvatarIndex(cro::Util::Random::value(0u, m_sharedData.avatarInfo.size() - 1));

                    //randomise colours
                    for (auto i = 0; i < PaletteID::BallThumb; ++i)
                    {
                        m_activeProfile.avatarFlags[i] = static_cast<std::uint8_t>(cro::Util::Random::value(0u, pc::PairCounts[i] - 1));
                        m_profileTextures[m_avatarIndex].setColour(pc::ColourKey::Index(i), m_activeProfile.avatarFlags[i]);
                    }

                    //update texture
                    m_profileTextures[m_avatarIndex].apply();

                    //update hair
                    if (m_avatarHairModels[m_avatarModels[m_avatarIndex].hairIndex].isValid())
                    {
                        m_avatarHairModels[m_avatarModels[m_avatarIndex].hairIndex].getComponent<cro::Model>().setMaterialProperty(0, "u_hairColour", pc::Palette[m_activeProfile.avatarFlags[0]]);
                    }

                    //update swatch
                    refreshSwatch();
                }
            });
#ifdef USE_GNS
    randomButton.getComponent<cro::UIInput>().setNextIndex(ButtonCancel, ButtonWorkshop);
#else
    randomButton.getComponent<cro::UIInput>().setNextIndex(ButtonCancel, ButtonHairColour);
#endif
    if (!Social::isSteamdeck())
    {
        randomButton.getComponent<cro::UIInput>().setPrevIndex(ButtonMugshot, ButtonSouthPaw);
    }
    else
    {
        randomButton.getComponent<cro::UIInput>().setPrevIndex(ButtonCancel, ButtonSouthPaw);
    }

    //name button
    auto nameButton = createButton("name_highlight", glm::vec2(264.f, 213.f), ButtonName);
    nameButton.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonUp] =
        uiSystem.addCallback([&](cro::Entity, const cro::ButtonEvent& evt) mutable
            {
                if (!m_activeProfile.isSteamID &&
                    activated(evt))
                {
                    auto& callback = m_menuEntities[EntityID::NameText].getComponent<cro::Callback>();
                    callback.active = !callback.active;
                    if (callback.active)
                    {
                        beginTextEdit(m_menuEntities[EntityID::NameText], &m_activeProfile.name, ConstVal::MaxStringChars);
                        m_audioEnts[AudioID::Accept].getComponent<cro::AudioEmitter>().play();

                        if (evt.type == SDL_CONTROLLERBUTTONUP)
                        {
                            auto* msg = postMessage<SystemEvent>(cl::MessageID::SystemMessage);
                            msg->type = SystemEvent::RequestOSK;
                            msg->data = 0;
                        }
                    }
                    else
                    {
                        applyTextEdit();
                        m_audioEnts[AudioID::Back].getComponent<cro::AudioEmitter>().play();                
                    }
                }
            });
#ifdef USE_GNS
    nameButton.getComponent<cro::UIInput>().setNextIndex(ButtonWorkshop, ButtonBallSelect);
    nameButton.getComponent<cro::UIInput>().setPrevIndex(ButtonHairBrowse, ButtonCancel);
#else
    nameButton.getComponent<cro::UIInput>().setNextIndex(ButtonDescUp, ButtonBallSelect);
    nameButton.getComponent<cro::UIInput>().setPrevIndex(ButtonHairBrowse, ButtonBallSelect);
#endif

    //ball arrow buttons
    auto ballLeft = createButton("arrow_left", glm::vec2(267.f, 144.f), ButtonPrevBall);
    ballLeft.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonUp] =
        uiSystem.addCallback([&](cro::Entity, const cro::ButtonEvent& evt)
            {
                if (activated(evt))
                {
                    applyTextEdit();
                    setBallIndex((m_ballIndex + (m_ballModels.size() - 1)) % m_ballModels.size());
                    m_audioEnts[AudioID::Back].getComponent<cro::AudioEmitter>().play();
                }
            });
    ballLeft.getComponent<cro::UIInput>().setNextIndex(ButtonBallSelect, ButtonBallColour);
    ballLeft.getComponent<cro::UIInput>().setPrevIndex(ButtonNextHair, ButtonName);
    ballLeft.setLabel("Previous Ball");
    expandHitbox(ballLeft);

    //toggles the ball browser
    auto ballThumb = createButton("ballthumb_highlight", { 279.f , 93.f }, ButtonBallSelect);
    ballThumb.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonUp] =
        uiSystem.addCallback([&](cro::Entity e, const cro::ButtonEvent& evt)
            {
                if (activated(evt))
                {
                    applyTextEdit();
                    m_uiScene.getSystem<cro::UISystem>()->setActiveGroup(MenuID::Dummy);
                    m_menuEntities[EntityID::BallBrowser].getComponent<cro::Callback>().active = true;

                    m_audioEnts[AudioID::Accept].getComponent<cro::AudioEmitter>().play();

                    m_lastSelected = e.getComponent<cro::UIInput>().getSelectionIndex();
                }
            });
    ballThumb.getComponent<cro::UIInput>().setNextIndex(ButtonNextBall, ButtonBallColour);
    ballThumb.getComponent<cro::UIInput>().setPrevIndex(ButtonPrevBall, ButtonName);
    ballThumb.setLabel("Open Ball Browser");

    auto ballRight = createButton("arrow_right", glm::vec2(382.f, 144.f), ButtonNextBall);
    ballRight.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonUp] =
        uiSystem.addCallback([&](cro::Entity, const cro::ButtonEvent& evt)
            {
                if (activated(evt))
                {
                    applyTextEdit();
                    setBallIndex((m_ballIndex + 1) % m_ballModels.size());
                    m_audioEnts[AudioID::Back].getComponent<cro::AudioEmitter>().play();
                }
            });
    ballRight.getComponent<cro::UIInput>().setNextIndex(ButtonDescUp, ButtonBallColourReset);
    ballRight.getComponent<cro::UIInput>().setPrevIndex(ButtonBallSelect, ButtonName);
    ballRight.setLabel("Next Ball");
    expandHitbox(ballRight);



    //ball colour button
    auto ballColour = createButton("ball_colour_highlight", glm::vec2(313.f, 75.f), ButtonBallColour);
    ballColour.getComponent<cro::UIInput>().setNextIndex(ButtonBallColourReset, ButtonUpdateIcon);
    ballColour.getComponent<cro::UIInput>().setPrevIndex(ButtonNextBody, ButtonBallSelect);
    ballColour.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonUp] =
        uiSystem.addCallback([&, fetchUIIndexFromColour](cro::Entity e, const cro::ButtonEvent& evt)
            {
                if (activated(evt))
                {
                    applyTextEdit();

                    m_ballColourFlyout.background.getComponent<cro::Transform>().setScale(glm::vec2(1.f));

                    m_uiScene.getSystem<cro::UISystem>()->setActiveGroup(MenuID::BallColour);
                    m_uiScene.getSystem<cro::UISystem>()->setColumnCount(PaletteColumnCount);

                    if (m_activeProfile.ballColourIndex < pc::Palette.size())
                    {
                        m_uiScene.getSystem<cro::UISystem>()->selectAt(fetchUIIndexFromColour(m_activeProfile.ballColourIndex, pc::ColourKey::Hair));
                    }
                    else
                    {
                        m_uiScene.getSystem<cro::UISystem>()->selectAt(0);
                    }

                    m_audioEnts[AudioID::Accept].getComponent<cro::AudioEmitter>().play();
                    m_lastSelected = e.getComponent<cro::UIInput>().getSelectionIndex();
                }
            });
    ballColour.setLabel("Choose Ball Tint");

    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 316.f, 78.f, 0.1f });
    entity.addComponent<cro::Drawable2D>().setVertexData(
        {
            cro::Vertex2D(glm::vec2(0.f, 10.f), cro::Colour::White),
            cro::Vertex2D(glm::vec2(0.f), cro::Colour::White),
            cro::Vertex2D(glm::vec2(10.f), cro::Colour::White),
            cro::Vertex2D(glm::vec2(10.f, 0.f), cro::Colour::White)
        });
    entity.addComponent<cro::Callback>().active = true;
    entity.getComponent<cro::Callback>().function =
        [&](cro::Entity e, float)
        {
            auto& verts = e.getComponent<cro::Drawable2D>().getVertexData();
            for (auto& v : verts)
            {
                v.colour = m_activeProfile.ballColour;
            }
        };
    bgEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());


    //ball colour reset button
    auto ballColourReset = createButton("ball_reset_highlight", glm::vec2(340.f, 75.f), ButtonBallColourReset);
    ballColourReset.getComponent<cro::UIInput>().setNextIndex(ButtonDescDown, ButtonUpdateIcon);
    ballColourReset.getComponent<cro::UIInput>().setPrevIndex(ButtonBallColour, ButtonNextBall);
    ballColourReset.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonUp] =
        uiSystem.addCallback([&](cro::Entity, const cro::ButtonEvent& evt)
            {
                if (activated(evt))
                {
                    applyTextEdit();

                    m_activeProfile.ballColour = cro::Colour::White;
                    m_activeProfile.ballColourIndex = 255;

                    m_ballModels[m_ballIndex].ball.getComponent<cro::Model>().setMaterialProperty(0, "u_ballColour", m_activeProfile.ballColour);

                    m_audioEnts[AudioID::Back].getComponent<cro::AudioEmitter>().play();
                }
            });
    ballColourReset.setLabel("Reset Ball Tint");


    //updates the profile icon
    auto profileIcon = createButton("button_highlight", glm::vec2(269.f, 55.f), ButtonUpdateIcon);
    bounds = profileIcon.getComponent<cro::Sprite>().getTextureBounds();
    bounds.left += 2.f;
    bounds.bottom += 2.f;
    bounds.width -= 4.f;
    bounds.height -= 4.f;
    profileIcon.getComponent<cro::UIInput>().area = bounds;
    profileIcon.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonUp] =
        uiSystem.addCallback([&](cro::Entity, const cro::ButtonEvent& evt)
            {
                if (activated(evt))
                {
                    generateMugshot();
                    m_audioEnts[AudioID::Snap].getComponent<cro::AudioEmitter>().play();
                }
            });
    profileIcon.getComponent<cro::UIInput>().setNextIndex(ButtonDescDown, ButtonSaveClose);
    profileIcon.getComponent<cro::UIInput>().setPrevIndex(ButtonSouthPaw, ButtonBallColour);

    //save/quit buttons
    auto saveQuit = createButton("button_highlight", glm::vec2(269.f, 38.f), ButtonSaveClose);
    bounds = saveQuit.getComponent<cro::Sprite>().getTextureBounds();
    bounds.left += 2.f;
    bounds.bottom += 2.f;
    bounds.width -= 4.f;
    bounds.height -= 4.f;
    saveQuit.getComponent<cro::UIInput>().area = bounds;
    saveQuit.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonUp] =
        uiSystem.addCallback([&](cro::Entity, const cro::ButtonEvent& evt)
            {
                if (activated(evt))
                {
                    m_profileData.playerProfiles[m_profileData.activeProfileIndex] = m_activeProfile;
                    m_profileData.playerProfiles[m_profileData.activeProfileIndex].saveProfile();

                    if (m_mugshotUpdated)
                    {
                        auto path = Social::getUserContentPath(Social::UserContent::Profile) + m_activeProfile.profileID + "/mug.png";
                        m_mugshotTexture.getTexture().saveToFile(path);

                        m_activeProfile.mugshot = path;
                        m_profileData.playerProfiles[m_profileData.activeProfileIndex].mugshot = path;
                        m_profileData.playerProfiles[m_profileData.activeProfileIndex].saveProfile();

                        m_mugshotUpdated = false;
                    }

                    quitState();
                }
            });
    saveQuit.getComponent<cro::UIInput>().setNextIndex(ButtonSouthPaw, ButtonCancel);
    saveQuit.getComponent<cro::UIInput>().setPrevIndex(ButtonSouthPaw, ButtonUpdateIcon);


    auto quit = createButton("button_highlight", glm::vec2(269.f, 21.f), ButtonCancel);
    bounds = quit.getComponent<cro::Sprite>().getTextureBounds();
    bounds.left += 2.f;
    bounds.bottom += 2.f;
    bounds.width -= 4.f;
    bounds.height -= 4.f;
    quit.getComponent<cro::UIInput>().area = bounds;
    quit.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonUp] =
        uiSystem.addCallback([&](cro::Entity, const cro::ButtonEvent& evt)
            {
                if (activated(evt))
                {
                    quitState();
                }
            });
    if (!Social::isSteamdeck())
    {
        quit.getComponent<cro::UIInput>().setNextIndex(ButtonMugshot, ButtonName);
    }
    else
    {
        quit.getComponent<cro::UIInput>().setNextIndex(ButtonRandomise, ButtonName);
    }
    quit.getComponent<cro::UIInput>().setPrevIndex(ButtonRandomise, ButtonSaveClose);



    auto addCorners = [&](cro::Entity p, cro::Entity q)
    {
        auto bounds = q.getComponent<cro::Sprite>().getTextureBounds() * q.getComponent<cro::Transform>().getScale().x;
        auto offset = q.getComponent<cro::Transform>().getPosition();

        auto cornerEnt = m_uiScene.createEntity();
        cornerEnt.addComponent<cro::Transform>().setPosition({ 0.f, 0.f, 0.3f });
        cornerEnt.getComponent<cro::Transform>().move(glm::vec2(offset));
        cornerEnt.addComponent<cro::Drawable2D>();
        cornerEnt.addComponent<cro::Sprite>() = spriteSheet.getSprite("corner_bl");
        p.getComponent<cro::Transform>().addChild(cornerEnt.getComponent<cro::Transform>());

        auto cornerBounds = cornerEnt.getComponent<cro::Sprite>().getTextureBounds();

        cornerEnt = m_uiScene.createEntity();
        cornerEnt.addComponent<cro::Transform>().setPosition({ 0.f, bounds.height - cornerBounds.height, 0.3f });
        cornerEnt.getComponent<cro::Transform>().move(glm::vec2(offset));
        cornerEnt.addComponent<cro::Drawable2D>();
        cornerEnt.addComponent<cro::Sprite>() = spriteSheet.getSprite("corner_tl");
        p.getComponent<cro::Transform>().addChild(cornerEnt.getComponent<cro::Transform>());

        cornerEnt = m_uiScene.createEntity();
        cornerEnt.addComponent<cro::Transform>().setPosition({ bounds.width - cornerBounds.width, bounds.height - cornerBounds.height, 0.3f });
        cornerEnt.getComponent<cro::Transform>().move(glm::vec2(offset));
        cornerEnt.addComponent<cro::Drawable2D>();
        cornerEnt.addComponent<cro::Sprite>() = spriteSheet.getSprite("corner_tr");
        p.getComponent<cro::Transform>().addChild(cornerEnt.getComponent<cro::Transform>());

        cornerEnt = m_uiScene.createEntity();
        cornerEnt.addComponent<cro::Transform>().setPosition({ bounds.width - cornerBounds.width, 0.f, 0.3f });
        cornerEnt.getComponent<cro::Transform>().move(glm::vec2(offset));
        cornerEnt.addComponent<cro::Drawable2D>();
        cornerEnt.addComponent<cro::Sprite>() = spriteSheet.getSprite("corner_br");
        p.getComponent<cro::Transform>().addChild(cornerEnt.getComponent<cro::Transform>());
    };


    //avatar preview
    glm::vec3 previewPos({ 98.f, 27.f, 0.1f });
    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition(previewPos);
    if (!m_sharedData.pixelScale)
    {
        entity.getComponent<cro::Transform>().setScale(glm::vec2(1.f / getViewScale()));
    }
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>(m_avatarTexture.getTexture());
    bgEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
    m_menuEntities[EntityID::AvatarPreview] = entity;
    addCorners(bgEnt, entity);

    //displays an icon based on whether the model is custom or built-in etc
    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition(previewPos + glm::vec3({ 108.f, 180.f, 0.1f }));
    entity.getComponent<cro::Transform>().setOrigin({ 8.f, 8.f, 0.f });
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>() = spriteSheet.getSprite("asset_type");
    entity.addComponent<cro::SpriteAnimation>();

    struct AnimData final
    {
        std::size_t m_lastIndex = 0;
        float progress = 0.f;
    };
    entity.addComponent<cro::Callback>().active = true;
    entity.getComponent<cro::Callback>().setUserData<AnimData>();
    entity.getComponent<cro::Callback>().function =
        [&](cro::Entity e, float dt)
        {
            auto& [lastIndex, progress] = e.getComponent<cro::Callback>().getUserData<AnimData>();
            
            if (m_avatarIndex != lastIndex)
            {
                //shrink
                progress = std::max(0.f, progress - (dt * 6.f));
                if(progress == 0)
                {
                    lastIndex = m_avatarIndex;
                    e.getComponent<cro::SpriteAnimation>().play(m_avatarModels[m_avatarIndex].type);
                }
            }
            else
            {
                if (progress != 1)
                {
                    //grow
                    progress = std::min(1.f, progress + (dt * 6.f));
                }
            }
            const float scale = cro::Util::Easing::easeOutSine(progress);
            e.getComponent<cro::Transform>().setScale(glm::vec2(scale, 1.f));
        };

    bgEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());


    //displays the index of the current avatar model
    auto& smallFont = m_sharedData.sharedResources->fonts.get(FontID::Info);
    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition(previewPos + glm::vec3(65.f, 9.f, 0.1f));
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Text>(smallFont).setCharacterSize(InfoTextSize);
    entity.getComponent<cro::Text>().setFillColour(TextNormalColour);
    entity.getComponent<cro::Text>().setShadowColour(LeaderboardTextDark);
    entity.getComponent<cro::Text>().setShadowOffset({ 1.f, -1.f });
    entity.getComponent<cro::Text>().setAlignment(cro::Text::Alignment::Centre);
    entity.getComponent<cro::Text>().setString("1/" + std::to_string(m_avatarModels.size() - m_lockedAvatarCount));
    entity.addComponent<cro::Callback>().active = true;
    entity.getComponent<cro::Callback>().setUserData<std::size_t>(0);
    entity.getComponent<cro::Callback>().function =
        [&](cro::Entity e, float)
        {
            auto& lastIdx = e.getComponent<cro::Callback>().getUserData<std::size_t>();
            if (m_avatarIndex != lastIdx)
            {
                lastIdx = m_avatarIndex;
                auto str = std::to_string(m_avatarModels[m_avatarIndex].previewIndex) + "/" + std::to_string(m_avatarModels.size() - m_lockedAvatarCount);
                e.getComponent<cro::Text>().setString(str);
            }
        };
    bgEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());


    //ball preview
    previewPos = { 279.f, 93.f, 0.1f };
    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition(previewPos);
    if (!m_sharedData.pixelScale)
    {
        entity.getComponent<cro::Transform>().setScale(glm::vec2(1.f / getViewScale()));
    }
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>(m_ballTexture.getTexture());
    bgEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());

    addCorners(bgEnt, entity);

    //displays an icon based on whether the model is custom or built-in etc
    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition(previewPos + glm::vec3({ 74.f, 88.f, 0.1f }));
    entity.getComponent<cro::Transform>().setOrigin({ 8.f, 8.f, 0.f });
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>() = spriteSheet.getSprite("asset_type");
    entity.addComponent<cro::SpriteAnimation>();
    entity.addComponent<cro::Callback>().active = true;
    entity.getComponent<cro::Callback>().setUserData<AnimData>();
    entity.getComponent<cro::Callback>().function =
        [&](cro::Entity e, float dt)
        {
            auto& [lastIndex, progress] = e.getComponent<cro::Callback>().getUserData<AnimData>();

            if (m_ballIndex != lastIndex)
            {
                //shrink
                progress = std::max(0.f, progress - (dt * 6.f));
                if (progress == 0)
                {
                    lastIndex = m_ballIndex;
                    e.getComponent<cro::SpriteAnimation>().play(m_ballModels[m_ballIndex].type);
                }
            }
            else
            {
                if (progress != 1)
                {
                    //grow
                    progress = std::min(1.f, progress + (dt * 6.f));
                }
            }
            const float scale = cro::Util::Easing::easeOutSine(progress);
            e.getComponent<cro::Transform>().setScale(glm::vec2(scale, 1.f));
        };
    bgEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());

    //displays selected index
    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition(previewPos + glm::vec3(48.f, 9.f, 0.1f));
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Text>(smallFont).setCharacterSize(InfoTextSize);
    entity.getComponent<cro::Text>().setFillColour(TextNormalColour);
    entity.getComponent<cro::Text>().setShadowColour(LeaderboardTextDark);
    entity.getComponent<cro::Text>().setShadowOffset({ 1.f, -1.f });
    entity.getComponent<cro::Text>().setAlignment(cro::Text::Alignment::Centre);
    entity.getComponent<cro::Text>().setString("1/" + std::to_string(m_ballModels.size()));
    entity.addComponent<cro::Callback>().active = true;
    entity.getComponent<cro::Callback>().setUserData<std::size_t>(0);
    entity.getComponent<cro::Callback>().function =
        [&](cro::Entity e, float)
        {
            auto& lastIdx = e.getComponent<cro::Callback>().getUserData<std::size_t>();
            if (m_ballIndex != lastIdx)
            {
                lastIdx = m_ballIndex;
                auto str = std::to_string(m_ballIndex + 1) + "/" + std::to_string(m_ballModels.size());
                e.getComponent<cro::Text>().setString(str);
            }
        };
    bgEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());

    //mugshot
    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 396.f, 24.f, 0.1f });
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>();
    bgEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
    m_menuEntities[EntityID::Mugshot] = entity;

    refreshMugshot();

    //opens the profile folder for easier adding of mugshot
    //but only in windowed mode and not on steamdeck
    if (!Social::isSteamdeck())
    {
        //open the profile folder to copy mugshot image
        entity = createButton("profile_highlight", {393.f, 21.f}, ButtonMugshot);
        entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonUp] =
            uiSystem.addCallback([&](cro::Entity, const cro::ButtonEvent& evt)
                {
                    if (activated(evt))
                    {
                        auto path = Social::getUserContentPath(Social::UserContent::Profile) + m_activeProfile.profileID;
                        if (cro::FileSystem::directoryExists(path)
                            && !cro::App::getWindow().isFullscreen())
                        {
                            cro::Util::String::parseURL(path);
                        }
                    }
                });
        entity.getComponent<cro::UIInput>().setNextIndex(ButtonRandomise, ButtonName);
        entity.getComponent<cro::UIInput>().setPrevIndex(ButtonCancel, ButtonDescDown);


        //*sigh* entity already has a callback for animation (which by this point should probably be its own system)
        //so hack around this with another entity that enables the button when !full screen
        auto b = entity;
        entity = m_uiScene.createEntity();
        entity.addComponent<cro::Callback>().active = true;
        entity.getComponent<cro::Callback>().function =
            [b](cro::Entity, float) mutable
        {
            b.getComponent<cro::UIInput>().enabled = !cro::App::getWindow().isFullscreen();
        };
    }

    entity = createButton("scroll_up", { 441.f, 202.f }, ButtonDescUp);
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonUp] =
        uiSystem.addCallback([&](cro::Entity, const cro::ButtonEvent& evt)
            {
                if (activated(evt))
                {
                    m_menuEntities[EntityID::BioText].getComponent<cro::Callback>().getUserData<std::int32_t>()--;
                    m_menuEntities[EntityID::BioText].getComponent<cro::Callback>().active = true;
                }
            });
    entity.getComponent<cro::UIInput>().setNextIndex(ButtonHairColour, ButtonDescDown);
    entity.getComponent<cro::UIInput>().setPrevIndex(ButtonNextBall, ButtonName);
    expandHitbox(entity);

    entity = createButton("scroll_down", { 441.f, 91.f }, ButtonDescDown);
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonUp] =
        uiSystem.addCallback([&](cro::Entity, const cro::ButtonEvent& evt)
            {
                if (activated(evt))
                {
                    m_menuEntities[EntityID::BioText].getComponent<cro::Callback>().getUserData<std::int32_t>()++;
                    m_menuEntities[EntityID::BioText].getComponent<cro::Callback>().active = true;
                }
            });
    if (!Social::isSteamdeck())
    {
        entity.getComponent<cro::UIInput>().setNextIndex(ButtonBottomLight, ButtonMugshot);
    }
    else
    {
        entity.getComponent<cro::UIInput>().setNextIndex(ButtonBottomLight, ButtonDescUp);
    }
    entity.getComponent<cro::UIInput>().setPrevIndex(ButtonNextBall, ButtonDescUp);
    expandHitbox(entity);


    //bio string
    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 399.f, 200.f, 0.1f });
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Text>(smallFont);// .setString("Cleftwhistle");
    entity.getComponent<cro::Text>().setFillColour(TextNormalColour);
    entity.getComponent<cro::Text>().setShadowColour(LeaderboardTextDark);
    entity.getComponent<cro::Text>().setShadowOffset({ 1.f, -1.f });
    entity.getComponent<cro::Text>().setCharacterSize(InfoTextSize);
    
    entity.addComponent<cro::Callback>().setUserData<std::int32_t>(0);
    entity.getComponent<cro::Callback>().function =
        [](cro::Entity e, float dt)
    {
        static constexpr float RowHeight = 8.f;
        const auto bounds = cro::Text::getLocalBounds(e);
        const auto maxHeight = -bounds.height - BioCrop.bottom;

        auto& idx = e.getComponent<cro::Callback>().getUserData<std::int32_t>();
        idx = std::clamp(idx, 0, std::abs(static_cast<std::int32_t>(maxHeight / RowHeight)) + 1);

        const float target = idx * -RowHeight;
        auto org = e.getComponent<cro::Transform>().getOrigin();
        org.y = target;// std::min(target, org.y - (target - org.y) * dt);

        auto rect = BioCrop;
        rect.bottom += org.y;
        e.getComponent<cro::Drawable2D>().setCroppingArea(rect);
        e.getComponent<cro::Transform>().setOrigin(org);

        if (org.y == target)
        {
            e.getComponent<cro::Callback>().active = false;
        }
    };

    bgEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
    m_menuEntities[EntityID::BioText] = entity;
    setBioString(generateRandomBio());

    //help string
    bounds = bgEnt.getComponent<cro::Sprite>().getTextureBounds();
    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({/*bounds.width / 2.f*/164.f, 14.f, 0.1f});
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Text>(smallFont);
    entity.getComponent<cro::Text>().setFillColour(TextNormalColour);
    entity.getComponent<cro::Text>().setShadowColour(LeaderboardTextDark);
    entity.getComponent<cro::Text>().setShadowOffset({ 1.f, -1.f });
    entity.getComponent<cro::Text>().setCharacterSize(InfoTextSize);
    bgEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
    m_menuEntities[EntityID::HelpText] = entity;


    //button help string
    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({bounds.width - 126.f, 14.f, 0.1f });
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Text>(smallFont);
    entity.getComponent<cro::Text>().setFillColour(TextNormalColour);
    entity.getComponent<cro::Text>().setShadowColour(LeaderboardTextDark);
    entity.getComponent<cro::Text>().setShadowOffset({ 1.f, -1.f });
    entity.getComponent<cro::Text>().setCharacterSize(InfoTextSize);
    entity.getComponent<cro::Text>().setAlignment(cro::Text::Alignment::Centre);
    //entity.getComponent<cro::Text>().setString("This is some test text. Hello!");
    bgEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
    m_menuEntities[EntityID::TipText] = entity;


    CallbackContext ctx;
    ctx.spriteSheet.loadFromFile("assets/golf/sprites/avatar_browser.spt", m_resources.textures);
    ctx.createArrow = 
        [&](std::int32_t left)
        {
            auto ent = m_uiScene.createEntity();
            ent.addComponent<cro::Transform>().setPosition({ 0.f, 0.f, 0.2f });
            ent.addComponent<cro::AudioEmitter>() = m_menuSounds.getEmitter("switch");
            ent.addComponent<cro::Drawable2D>();
            ent.addComponent<cro::Sprite>() = ctx.spriteSheet.getSprite(left ? "arrow_left" : "arrow_right");
            ent.addComponent<cro::Callback>().setUserData<float>(ent.getComponent<cro::Sprite>().getTextureRect().left);
            ent.addComponent<cro::UIInput>().area = ent.getComponent<cro::Sprite>().getTextureBounds();
            ent.getComponent<cro::UIInput>().callbacks[cro::UIInput::Selected] = ctx.arrowSelected;
            ent.getComponent<cro::UIInput>().callbacks[cro::UIInput::Unselected] = ctx.arrowUnselected;
            return ent;
        };
    ctx.arrowSelected = uiSystem.addCallback([&](cro::Entity e) mutable
        {
            e.getComponent<cro::AudioEmitter>().play();
            //we hacked in the base pos here
            const float left = e.getComponent<cro::Callback>().getUserData<float>();
            auto bounds = e.getComponent<cro::Sprite>().getTextureRect();
            bounds.left = bounds.width + left;
            e.getComponent<cro::Sprite>().setTextureRect(bounds);

            //TODO we need to know the type of item we're paging
            for (auto& page : m_pageContexts[PaginationID::Balls].pageList)
            {
                page.highlight.getComponent<cro::Drawable2D>().setFacing(cro::Drawable2D::Facing::Back);
            }
            //instead of hacking this here
            for (auto& page : m_pageContexts[PaginationID::Hair].pageList)
            {
                page.highlight.getComponent<cro::Drawable2D>().setFacing(cro::Drawable2D::Facing::Back);
            }
        });
    ctx.arrowUnselected = uiSystem.addCallback([](cro::Entity e)
        {
            const float left = e.getComponent<cro::Callback>().getUserData<float>();
            auto bounds = e.getComponent<cro::Sprite>().getTextureRect();
            bounds.left = left;
            e.getComponent<cro::Sprite>().setTextureRect(bounds);
        });
    ctx.closeSelected = uiSystem.addCallback([&](cro::Entity e) mutable
        {
            e.getComponent<cro::AudioEmitter>().play();
            e.getComponent<cro::Sprite>().setColour(cro::Colour::White);
            e.getComponent<cro::Callback>().active = true;

            //TODO get the item ID of what we're paging
            for (auto& page : m_pageContexts[PaginationID::Balls].pageList)
            {
                page.highlight.getComponent<cro::Drawable2D>().setFacing(cro::Drawable2D::Facing::Back);
            }
            //instead of hacking this here
            for (auto& page : m_pageContexts[PaginationID::Hair].pageList)
            {
                page.highlight.getComponent<cro::Drawable2D>().setFacing(cro::Drawable2D::Facing::Back);
            }
        });
    ctx.closeUnselected = uiSystem.addCallback([](cro::Entity e)
        {
            e.getComponent<cro::Sprite>().setColour(cro::Colour::Transparent);
        });


    createPalettes(bgEnt);
    createBallBrowser(rootNode, ctx);
    createHairBrowser(rootNode, ctx);

    auto updateView = [&, rootNode](cro::Camera& cam) mutable
    {
        glm::vec2 size(GolfGame::getActiveTarget()->getSize());

        cam.setOrthographic(0.f, size.x, 0.f, size.y, -2.f, 10.f);
        cam.viewport = { 0.f, 0.f, 1.f, 1.f };

        m_viewScale = glm::vec2(getViewScale());
        rootNode.getComponent<cro::Transform>().setScale(m_viewScale);
        rootNode.getComponent<cro::Transform>().setPosition(size / 2.f);

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
        m_uiScene.getSystem<cro::CommandSystem>()->sendCommand(cmd);
    };

    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>();
    entity.addComponent<cro::Camera>().resizeCallback = updateView;
    m_uiScene.setActiveCamera(entity);
    updateView(entity.getComponent<cro::Camera>());
}

void ProfileState::buildPreviewScene()
{
    CRO_ASSERT(!m_profileData.ballDefs.empty(), "Must load this state on top of menu");

    cro::EmitterSettings emitterSettings;
    emitterSettings.loadFromFile("assets/golf/particles/puff_small.cps", m_resources.textures);

    //this has all been parsed by the menu state - so we're assuming
    //all the models etc are fine and load without chicken
    std::int32_t c = 0;
    static constexpr glm::vec3 BallPos({ 10.f, 0.f, 0.f });
    for (auto& ballDef : m_profileData.ballDefs)
    {
        auto entity = m_modelScene.createEntity();
        entity.addComponent<cro::Transform>();
        ballDef.createModel(entity);
        entity.getComponent<cro::Model>().setHidden(true);
        entity.getComponent<cro::Model>().setMaterial(0, m_profileData.profileMaterials.ball);
        entity.getComponent<cro::Model>().setMaterial(1, m_profileData.profileMaterials.ballReflection);
        entity.addComponent<cro::Callback>().active = true;

        //if (m_sharedData.ballInfo[c].rollAnimation)
        //{
        //    entity.getComponent<cro::Callback>().function =
        //        [](cro::Entity e, float dt)
        //    {
        //        e.getComponent<cro::Transform>().rotate(cro::Transform::X_AXIS, -dt * 6.f);
        //    };
        //    entity.getComponent<cro::Transform>().setRotation(cro::Transform::Y_AXIS, /*cro::Util::Const::PI + */m_sharedData.ballInfo[c].previewRotation);
        //    entity.getComponent<cro::Transform>().move({ 0.f, Ball::Radius, 0.f });
        //    entity.getComponent<cro::Transform>().setOrigin({ 0.f, Ball::Radius, 0.f });
        //}
        //else
        {
            entity.getComponent<cro::Callback>().function =
                [](cro::Entity e, float dt)
            {
                e.getComponent<cro::Transform>().rotate(cro::Transform::Y_AXIS, dt);
            };
        }

        auto& preview = m_ballModels.emplace_back();
        preview.ball = entity;
        preview.type = m_sharedData.ballInfo[c].type;
        preview.root = m_modelScene.createEntity();
        preview.root.addComponent<cro::Transform>().setPosition(BallPos);
        preview.root.getComponent<cro::Transform>().setRotation(cro::Transform::Y_AXIS, m_sharedData.ballInfo[c].previewRotation);
        preview.root.getComponent<cro::Transform>().addChild(preview.ball.getComponent<cro::Transform>());
        preview.root.addComponent<cro::ParticleEmitter>().settings = emitterSettings;
        
        ++c;
    }
    
    auto entity = m_modelScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition(BallPos + glm::vec3(0.f, -0.01f, 0.f));
    m_profileData.shadowDef->createModel(entity);

    entity = m_modelScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition(BallPos + glm::vec3(0.f, -0.15f, 0.f));
    entity.getComponent<cro::Transform>().setScale(glm::vec3(5.f));
    m_profileData.grassDef->createModel(entity);

    static constexpr glm::vec3 AvatarPos({ CameraBasePosition.x, 0.f, 0.f });
    std::size_t previewIndex = 1;
    for (auto i = 0u; i < m_profileData.avatarDefs.size(); ++i)
    {
        //need to pad this out regardless for correct indexing
        auto& avt = m_avatarModels.emplace_back();
        if (!m_sharedData.avatarInfo[i].locked)
        {
            auto& avatar = m_profileData.avatarDefs[i];

            auto entity = m_modelScene.createEntity();
            entity.addComponent<cro::Transform>().setOrigin(AvatarPos);
            entity.getComponent<cro::Transform>().setPosition(entity.getComponent<cro::Transform>().getOrigin());
            avatar.createModel(entity);
            entity.getComponent<cro::Model>().setHidden(true);

            auto material = m_profileData.profileMaterials.avatar;
            applyMaterialData(avatar, material);
            entity.getComponent<cro::Model>().setMaterial(0, material);

            entity.addComponent<cro::Callback>().setUserData<AvatarAnimCallbackData>();
            entity.getComponent<cro::Callback>().function =
                [&](cro::Entity e, float dt)
                {
                    auto& [direction, progress] = e.getComponent<cro::Callback>().getUserData<AvatarAnimCallbackData>();
                    const float Speed = dt * 4.f;
                    float rotation = 0.f; //hmm would be nice to rotate in the direction of the index change...

                    if (direction == 0)
                    {
                        //grow
                        progress = std::min(1.f, progress + Speed);
                        rotation = -cro::Util::Const::TAU + (cro::Util::Const::TAU * progress);

                        if (progress == 1)
                        {
                            e.getComponent<cro::Callback>().active = false;
                        }
                    }
                    else
                    {
                        //shrink
                        progress = std::max(0.f, progress - Speed);
                        rotation = cro::Util::Const::TAU * (1.f - progress);

                        if (progress == 0)
                        {
                            e.getComponent<cro::Callback>().active = false;
                            e.getComponent<cro::Model>().setHidden(true);
                        }
                    }

                    glm::vec3 scale(progress, 1.f, progress);
                    e.getComponent<cro::Transform>().setScale(scale);

                    //TODO we want to add initial rotation here ideally...
                    //however it's not easily extractable from the orientation.
                    e.getComponent<cro::Transform>().setRotation(cro::Transform::Y_AXIS, rotation);
                };

            avt.previewModel = entity;
            avt.type = m_sharedData.avatarInfo[i].type;
            avt.previewIndex = previewIndex++;

            //these are unique models from the menu so we'll 
            //need to capture their attachment points once again...
            if (entity.hasComponent<cro::Skeleton>())
            {
                //this should never not be true as the models were validated
                //in the menu state - but 
                auto id = entity.getComponent<cro::Skeleton>().getAttachmentIndex("head");
                if (id > -1)
                {
                    //hair is optional so OK if this doesn't exist
                    avt.hairAttachment = &entity.getComponent<cro::Skeleton>().getAttachments()[id];
                }

                entity.getComponent<cro::Skeleton>().play(entity.getComponent<cro::Skeleton>().getAnimationIndex("idle_standing"));
            }
        }
        else
        {
            m_lockedAvatarCount++;
        }
    }

    entity = m_modelScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition(AvatarPos + glm::vec3(0.f, -0.1f, -0.08f));
    entity.getComponent<cro::Transform>().setScale(glm::vec3(16.f));
    m_profileData.shadowDef->createModel(entity);

    entity = m_modelScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition(AvatarPos + glm::vec3(0.f, -0.15f, 0.f));
    entity.getComponent<cro::Transform>().setScale(glm::vec3(60.f));
    m_profileData.grassDef->createModel(entity);
    entity.getComponent<cro::Model>().setRenderFlags((1 << 1));
    
    //update the model textures with the current colour settings
    //and any available audio
    const std::array<std::string, 3u> emitterNames =
    {
        "bunker", "fairway", "green"
    };
    for (auto i = 0u; i < m_sharedData.avatarInfo.size(); ++i)
    {
        //need to pad out the vector anyway for correct indexing
        auto& t = m_profileTextures.emplace_back(m_sharedData.avatarInfo[i].texturePath);
        
        if (!m_sharedData.avatarInfo[i].locked)
        {
            for (auto j = 0; j < pc::ColourKey::Count; ++j)
            {
                t.setColour(pc::ColourKey::Index(j), m_activeProfile.avatarFlags[j]);
            }
            t.apply();

            m_avatarModels[i].previewModel.getComponent<cro::Model>().setMaterialProperty(0, "u_diffuseMap", t.getTextureID());

            cro::AudioScape as;
            if (!m_sharedData.avatarInfo[i].audioscape.empty() &&
                as.loadFromFile(m_sharedData.avatarInfo[i].audioscape, m_resources.audio))
            {
                for (const auto& name : emitterNames)
                {
                    if (as.hasEmitter(name))
                    {
                        auto e = m_uiScene.createEntity();
                        e.addComponent<cro::Transform>();
                        e.addComponent<cro::AudioEmitter>() = as.getEmitter(name);
                        e.getComponent<cro::AudioEmitter>().setLooped(false);
                        m_avatarModels[i].previewAudio.push_back(e);
                    }
                }
            }
        }
    }

    //empty at front for 'bald'
    m_avatarHairModels.push_back({});
    m_ballHairModels.push_back({});
    for (auto& hair : m_profileData.hairDefs)
    {
        entity = m_modelScene.createEntity();
        entity.addComponent<cro::Transform>();
        hair.createModel(entity);
        entity.getComponent<cro::Model>().setHidden(true);
        entity.getComponent<cro::Model>().setMaterial(0, m_profileData.profileMaterials.hair);
        m_avatarHairModels.push_back(entity);
        
        entity = m_modelScene.createEntity();
        entity.addComponent<cro::Transform>().setScale(BallHairScale);
        entity.getComponent<cro::Transform>().setOrigin(BallHairOffset);
        entity.getComponent<cro::Transform>().rotate(cro::Transform::Y_AXIS, 0.3f);
        hair.createModel(entity);
        entity.getComponent<cro::Model>().setMaterial(0, m_profileData.profileMaterials.hair);
        entity.getComponent<cro::Model>().setMaterialProperty(0, "u_hairColour", CD32::Colours[CD32::Orange]);
        entity.getComponent<cro::Model>().setHidden(true);
        m_ballHairModels.push_back(entity);
    }

    m_avatarIndex = indexFromAvatarID(m_activeProfile.skinID);
    m_avatarModels[m_avatarIndex].previewModel.getComponent<cro::Model>().setHidden(false);
    m_avatarModels[m_avatarIndex].previewModel.getComponent<cro::Callback>().active = true;

    m_ballIndex = indexFromBallID(m_activeProfile.ballID);
    m_ballModels[m_ballIndex].ball.getComponent<cro::Model>().setHidden(false);

    setHairIndex(indexFromHairID(m_activeProfile.hairID));

    auto ballTexCallback = [&](cro::Camera& cam)
    {
        //std::uint32_t samples = m_sharedData.pixelScale ? 0 :
        //    m_sharedData.antialias ? m_sharedData.multisamples : 0;

        //auto windowSize = static_cast<float>(cro::App::getWindow().getSize().x);

        float windowScale = getViewScale();
        float scale = m_sharedData.pixelScale ? windowScale : 1.f;

        auto invScale = static_cast<std::uint32_t>((windowScale + 1.f) - scale);
        auto size = BallTexSize * invScale;
        m_ballTexture.create(size.x, size.y, true, false, /*samples*/0);

        size = AvatarTexSize * invScale;
        m_avatarTexture.create(size.x, size.y, true, false, /*samples*/0);


        cam.setPerspective(1.1f, static_cast<float>(BallTexSize.x) / BallTexSize.y, 0.001f, 2.f);
        cam.viewport = { 0.f, 0.f, 1.f, 1.f };
    };
    m_cameras[CameraID::Ball] = m_modelScene.createEntity();
    m_cameras[CameraID::Ball].addComponent<cro::Transform>().setPosition({ 10.f, 0.045f, 0.099f });
    m_cameras[CameraID::Ball].addComponent<cro::Camera>().resizeCallback = ballTexCallback;
    ballTexCallback(m_cameras[CameraID::Ball].getComponent<cro::Camera>());

    createItemThumbs();

    m_cameras[CameraID::Avatar] = m_modelScene.createEntity();
    m_cameras[CameraID::Avatar].addComponent<cro::Transform>().setPosition(CameraBasePosition);
    m_cameras[CameraID::Avatar].getComponent<cro::Transform>().setRotation(cro::Transform::Y_AXIS, cro::Util::Const::PI);
    m_cameras[CameraID::Avatar].getComponent<cro::Transform>().rotate(cro::Transform::X_AXIS, -0.157f);
    auto& cam = m_cameras[CameraID::Avatar].addComponent<cro::Camera>();
    cam.setPerspective(70.f * cro::Util::Const::degToRad, static_cast<float>(AvatarTexSize.x) / AvatarTexSize.y, 0.1f, 6.f);
    cam.viewport = { 0.f, 0.f, 1.f, 1.f };

    m_cameras[CameraID::Mugshot] = m_modelScene.createEntity();
    m_cameras[CameraID::Mugshot].addComponent<cro::Transform>().setPosition(MugCameraPosition);
    m_cameras[CameraID::Mugshot].getComponent<cro::Transform>().setRotation(cro::Transform::Y_AXIS, cro::Util::Const::PI);
    m_cameras[CameraID::Mugshot].getComponent<cro::Transform>().rotate(cro::Transform::X_AXIS, -0.057f);
    auto& cam2 = m_cameras[CameraID::Mugshot].addComponent<cro::Camera>();
    cam2.setPerspective(60.f * cro::Util::Const::degToRad, 1.f, 0.1f, 6.f);
    cam2.viewport = { 0.f, 0.f, 0.5f, 1.f };
    cam2.setRenderFlags(cro::Camera::Pass::Final, ~(1 << 1));


    m_modelScene.getSunlight().getComponent<cro::Transform>().rotate(cro::Transform::Y_AXIS, 96.f * cro::Util::Const::degToRad);
    m_modelScene.getSunlight().getComponent<cro::Transform>().rotate(cro::Transform::X_AXIS, -39.f * cro::Util::Const::degToRad);
}

void ProfileState::createPalettes(cro::Entity parent)
{
    for (auto i = 0u; i < PaletteID::BallThumb; ++i)
    {
        auto entity = createPaletteBackground(parent, m_flyouts[i], i);

        //buttons/menu items
        m_flyouts[i].activateCallback = m_uiScene.getSystem<cro::UISystem>()->addCallback(
            [&, i](cro::Entity e, const cro::ButtonEvent& evt) mutable
            {
                auto quitMenu = [&]()
                {
                    m_flyouts[i].background.getComponent<cro::Transform>().setScale(glm::vec2(0.f));
                    m_uiScene.getSystem<cro::UISystem>()->setActiveGroup(MenuID::Main);
                    m_uiScene.getSystem<cro::UISystem>()->setColumnCount(1);
                    m_uiScene.getSystem<cro::UISystem>()->selectAt(m_lastSelected);

                    //update texture
                    m_profileTextures[m_avatarIndex].setColour(pc::ColourKey::Index(i), m_activeProfile.avatarFlags[i]);
                    m_profileTextures[m_avatarIndex].apply();

                    //refresh hair colour
                    if (m_avatarHairModels[m_avatarModels[m_avatarIndex].hairIndex].isValid())
                    {
                        m_avatarHairModels[m_avatarModels[m_avatarIndex].hairIndex].getComponent<cro::Model>().setMaterialProperty(0, "u_hairColour", pc::Palette[m_activeProfile.avatarFlags[0]]);
                    }
                };

                if (activated(evt))
                {
                    m_activeProfile.avatarFlags[i] = e.getComponent<cro::Callback>().getUserData<std::uint8_t>();
                    quitMenu();

                    refreshSwatch();
                }
                else if (deactivated(evt))
                {
                    quitMenu();
                }
            });
        m_flyouts[i].selectCallback = m_uiScene.getSystem<cro::UISystem>()->addCallback(
            [&, entity, i](cro::Entity e) mutable
            {
                if (i == PaletteID::Hair)
                {
                    if (m_avatarHairModels[m_avatarModels[m_avatarIndex].hairIndex].isValid())
                    {
                        m_avatarHairModels[m_avatarModels[m_avatarIndex].hairIndex].getComponent<cro::Model>().setMaterialProperty(0, "u_hairColour", pc::Palette[e.getComponent<cro::Callback>().getUserData<std::uint8_t>()]);
                    }
                }

                m_profileTextures[m_avatarIndex].setColour(pc::ColourKey::Index(i), e.getComponent<cro::Callback>().getUserData<std::uint8_t>());
                m_profileTextures[m_avatarIndex].apply();

                entity.getComponent<cro::Transform>().setPosition(e.getComponent<cro::Transform>().getPosition());
                m_audioEnts[AudioID::Select].getComponent<cro::AudioEmitter>().play();
                m_audioEnts[AudioID::Select].getComponent<cro::AudioEmitter>().setPlayingOffset(cro::Time());
            });

        const auto menuID = i + MenuID::Hair;
        const std::size_t IndexOffset = (i * 100) + 200;
        createPaletteButtons(m_flyouts[i], i, menuID, IndexOffset);
    }

    auto entity = createPaletteBackground(parent, m_ballColourFlyout, PaletteID::Hair);
    m_ballColourFlyout.background.getComponent<cro::Transform>().setPosition(glm::vec2(320.f, 12.f));

    //activate and select callbacks
    m_ballColourFlyout.activateCallback = m_uiScene.getSystem<cro::UISystem>()->addCallback(
        [&](cro::Entity e, const cro::ButtonEvent& evt) mutable
        {
            auto quitMenu = [&]()
                {
                    m_ballColourFlyout.background.getComponent<cro::Transform>().setScale(glm::vec2(0.f));
                    m_uiScene.getSystem<cro::UISystem>()->setActiveGroup(MenuID::Main);
                    m_uiScene.getSystem<cro::UISystem>()->setColumnCount(1);
                    m_uiScene.getSystem<cro::UISystem>()->selectAt(m_lastSelected);

                    m_ballModels[m_ballIndex].ball.getComponent<cro::Model>().setMaterialProperty(0, "u_ballColour", m_activeProfile.ballColour);
                };

            if (activated(evt))
            {
                m_activeProfile.ballColourIndex = e.getComponent<cro::Callback>().getUserData<std::uint8_t>();
                m_activeProfile.ballColour = pc::Palette[m_activeProfile.ballColourIndex];
                quitMenu();
            }
            else if (deactivated(evt))
            {
                quitMenu();
            }
        });

    m_ballColourFlyout.selectCallback = m_uiScene.getSystem<cro::UISystem>()->addCallback(
        [&, entity](cro::Entity e) mutable
        {
            m_ballModels[m_ballIndex].ball.getComponent<cro::Model>().setMaterialProperty(0, "u_ballColour", pc::Palette[e.getComponent<cro::Callback>().getUserData<std::uint8_t>()]);


            entity.getComponent<cro::Transform>().setPosition(e.getComponent<cro::Transform>().getPosition());
            m_audioEnts[AudioID::Select].getComponent<cro::AudioEmitter>().play();
            m_audioEnts[AudioID::Select].getComponent<cro::AudioEmitter>().setPlayingOffset(cro::Time());
        });

    createPaletteButtons(m_ballColourFlyout, PaletteID::Hair, MenuID::BallColour, (PaletteID::Count * 100) + 200);
}

void ProfileState::createItemThumbs()
{
    glm::uvec2 texSize(std::min(ThumbColCount, m_ballModels.size()) * BallThumbSize.x, ((m_ballModels.size() / ThumbColCount) + 1) * BallThumbSize.y);
    texSize *= ThumbTextureScale;

    cro::FloatRect vp = { 0.f, 0.f, static_cast<float>(BallThumbSize.x * ThumbTextureScale) / texSize.x, static_cast<float>(BallThumbSize.y * ThumbTextureScale) / texSize.y };
    auto& cam = m_cameras[CameraID::Ball].getComponent<cro::Camera>();

    auto oldIndex = m_ballIndex;

    glm::quat oldRot = cro::Transform::QUAT_IDENTITY;
    const auto showBall = [&](std::size_t idx)
        {
            m_ballModels[m_ballIndex].ball.getComponent<cro::Model>().setHidden(true);
            m_ballIndex = idx;
            m_ballModels[m_ballIndex].ball.getComponent<cro::Model>().setHidden(false);

            oldRot = m_ballModels[m_ballIndex].ball.getComponent<cro::Transform>().getRotation();
            m_ballModels[m_ballIndex].ball.getComponent<cro::Transform>().setRotation(cro::Transform::QUAT_IDENTITY);
        };

    m_modelScene.setActiveCamera(m_cameras[CameraID::Ball]);
    m_modelScene.simulate(0.f);

    const auto& ident = m_resources.textures.get("assets/golf/images/ident.png");
    cro::SimpleQuad keyIcon(ident);
    cro::SimpleQuad spannerIcon(ident);

    const auto identSize = glm::vec2(ident.getSize());
    const cro::FloatRect key(0.f, 0.f, identSize.x / 2.f, identSize.y);
    const cro::FloatRect spanner(identSize.x / 2.f, 0.f, identSize.x / 2.f, identSize.y);
    const glm::vec2 iconOffset = (glm::vec2(BallThumbSize) - glm::vec2(17.f)) * ThumbTextureScale;
    keyIcon.setTextureRect(key);
    keyIcon.setScale(glm::vec2(ThumbTextureScale));
    spannerIcon.setTextureRect(spanner);
    spannerIcon.setScale(glm::vec2(ThumbTextureScale));


    //ball thumbs
    m_pageContexts[PaginationID::Balls].thumbnailTexture.create(texSize.x, texSize.y);
    m_pageContexts[PaginationID::Balls].thumbnailTexture.clear(CD32::Colours[CD32::BlueLight]);

    for (auto i = 0u; i < m_ballModels.size(); ++i)
    {
        const auto x = (i % ThumbColCount);
        const auto y = (i / ThumbColCount);

        vp.left = x * vp.width;
        vp.bottom = y * vp.height;
        cam.viewport = vp;

        showBall(i);

        m_modelScene.simulate(0.f);
        m_modelScene.render();
        m_ballModels[m_ballIndex].ball.getComponent<cro::Transform>().setRotation(oldRot);

        if (m_ballModels[i].type == 1)
        {
            //unlocked ball
            keyIcon.setPosition(glm::vec2(x * BallThumbSize.x * ThumbTextureScale, y * BallThumbSize.y * ThumbTextureScale) + iconOffset);
            keyIcon.draw();
        }
        else if (m_ballModels[i].type == 2)
        {
            //custom model
            spannerIcon.setPosition(glm::vec2(x * BallThumbSize.x * ThumbTextureScale, y * BallThumbSize.y * ThumbTextureScale) + iconOffset);
            spannerIcon.draw();
        }
    }

    m_pageContexts[PaginationID::Balls].thumbnailTexture.display();
    showBall(oldIndex);


    cro::ModelDefinition md(m_resources);
    md.loadFromFile("assets/models/bust.cmt");
    auto ent = m_modelScene.createEntity();
    ent.addComponent<cro::Transform>();
    md.createModel(ent);
    ent.getComponent<cro::Model>().setMaterial(0, m_profileData.profileMaterials.hair);
    ent.getComponent<cro::Model>().setMaterialProperty(0, "u_hairColour", CD32::Colours[CD32::BeigeLight]);
    m_modelScene.simulate(0.f);

    const auto showHair = [&](std::size_t idx)
        {
            //first item is bald so there's no model ent
            if (m_ballHairModels[idx].isValid())
            {
                m_ballModels[m_ballIndex].root.getComponent<cro::Transform>().addChild(m_ballHairModels[idx].getComponent<cro::Transform>());
                m_ballModels[m_ballIndex].root.getComponent<cro::Transform>().setRotation(cro::Transform::QUAT_IDENTITY); //this will have been set when rendering balls
                m_ballHairModels[idx].getComponent<cro::Model>().setHidden(false);
                m_ballHairModels[idx].getComponent<cro::Transform>().addChild(ent.getComponent<cro::Transform>());
            }
        };

    const auto hideHair = [&](std::size_t idx)
        {
            if (m_ballHairModels[idx].isValid())
            {
                m_ballModels[m_ballIndex].root.getComponent<cro::Transform>().removeChild(m_ballHairModels[idx].getComponent<cro::Transform>());
                m_ballHairModels[idx].getComponent<cro::Model>().setHidden(true);
                m_ballHairModels[idx].getComponent<cro::Transform>().removeChild(ent.getComponent<cro::Transform>());
                m_modelScene.simulate(0.f);
            }
        };

    //hair thumbs
    texSize = glm::uvec2(std::min(ThumbColCount, m_ballHairModels.size()) * BallThumbSize.x, ((m_ballHairModels.size() / ThumbColCount) + 1) * BallThumbSize.y);
    texSize *= ThumbTextureScale;
    vp = { 0.f, 0.f, static_cast<float>(BallThumbSize.x * ThumbTextureScale) / texSize.x, static_cast<float>(BallThumbSize.y * ThumbTextureScale) / texSize.y };
    cam.viewport = vp;

    m_ballModels[m_ballIndex].ball.getComponent<cro::Model>().setHidden(true);

    m_pageContexts[PaginationID::Hair].thumbnailTexture.create(texSize.x, texSize.y);
    m_pageContexts[PaginationID::Hair].thumbnailTexture.clear(CD32::Colours[CD32::BlueLight]);
    
    //fudge to render bald bust
    ent.getComponent<cro::Transform>().setPosition(m_ballModels[m_ballIndex].root.getComponent<cro::Transform>().getWorldPosition() - (BallHairOffset * BallHairScale));
    ent.getComponent<cro::Transform>().setScale(BallHairScale);
    ent.getComponent<cro::Transform>().setRotation(cro::Transform::Y_AXIS, 0.3f);
    m_modelScene.simulate(0.f);
    m_modelScene.render();
    ent.getComponent<cro::Transform>().setPosition(glm::vec3(0.f));
    ent.getComponent<cro::Transform>().setScale(glm::vec3(1.f));
    ent.getComponent<cro::Transform>().setRotation(cro::Transform::QUAT_IDENTITY);

    //don't include bald at the front
    for (auto i = 1u; i < m_ballHairModels.size(); ++i)
    {
        const auto x = (i % ThumbColCount);
        const auto y = (i / ThumbColCount);

        vp.left = x * vp.width;
        vp.bottom = y * vp.height;
        cam.viewport = vp;

        showHair(i);

        m_modelScene.simulate(0.f);
        m_modelScene.render();

        hideHair(i);

        if (m_sharedData.hairInfo[i].type == 1)
        {
            //unlocked hair
            keyIcon.setPosition(glm::vec2(x * BallThumbSize.x * ThumbTextureScale, y * BallThumbSize.y * ThumbTextureScale) + iconOffset);
            keyIcon.draw();
        }
        else if (m_sharedData.hairInfo[i].type == 2)
        {
            //custom model
            spannerIcon.setPosition(glm::vec2(x * BallThumbSize.x * ThumbTextureScale, y * BallThumbSize.y * ThumbTextureScale) + iconOffset);
            spannerIcon.draw();
        }
    }

    m_pageContexts[PaginationID::Hair].thumbnailTexture.display();
    
    m_modelScene.destroyEntity(ent);
    m_ballModels[m_ballIndex].ball.getComponent<cro::Model>().setHidden(false);

    cam.viewport = { 0.f, 0.f , 1.f, 1.f };
}

void ProfileState::createItemPage(cro::Entity parent, std::int32_t page, std::int32_t itemID)
{
    CRO_ASSERT(m_pageContexts[itemID].pageList.size() == page, "");
    static constexpr glm::vec2 IconSize(BallThumbSize);
    static constexpr float IconPadding = 6.f;

    static constexpr float ThumbWidth = (ThumbColCount * (IconSize.x + IconPadding)) + IconPadding;
    const float BgWidth = parent.getComponent<cro::Sprite>().getTextureBounds().width;

    const std::size_t RangeStart = page * ThumbRowCount * ThumbColCount;
    const std::size_t RangeEnd = RangeStart + std::min(ThumbRowCount * ThumbColCount, m_pageContexts[itemID].itemCount - RangeStart);
    const std::size_t ItemCount = RangeEnd - RangeStart;

    const auto RowCount = std::min(ThumbRowCount, (ItemCount / ThumbColCount) + std::min(std::size_t(1), ItemCount % ThumbColCount));
    const float ThumbHeight = (RowCount * IconSize.y) + (RowCount * IconPadding) + IconPadding;

    auto& browserPage = m_pageContexts[itemID].pageList.emplace_back();
    browserPage.background = m_uiScene.createEntity();
    browserPage.background.addComponent<cro::Transform>().setPosition({ (BgWidth - ThumbWidth) / 2.f, 290.f - ThumbHeight, 0.2f});
    parent.getComponent<cro::Transform>().addChild(browserPage.background.getComponent<cro::Transform>());

    //thumbnails
    std::vector<cro::Vertex2D> verts;
    auto entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 0.f, 0.f, 0.1f });
    entity.addComponent<cro::Drawable2D>().setPrimitiveType(GL_TRIANGLES);
    entity.getComponent<cro::Drawable2D>().setTexture(&m_pageContexts[itemID].thumbnailTexture.getTexture());

    const glm::vec2 TexSize(m_pageContexts[itemID].thumbnailTexture.getSize());
    cro::FloatRect textureBounds = { 0.f, 0.f, static_cast<float>(BallThumbSize.x * ThumbTextureScale) / TexSize.x, static_cast<float>(BallThumbSize.y * ThumbTextureScale) / TexSize.y };

    for (auto j = RangeStart; j < RangeEnd; ++j)
    {
        std::size_t x = (j - RangeStart) % ThumbColCount;
        std::size_t y = (j - RangeStart) / ThumbColCount;
        textureBounds.left = (j%ThumbColCount) * textureBounds.width;
        textureBounds.bottom = (j/ThumbColCount) * textureBounds.height;

        glm::vec2 pos = { x * (IconSize.x + IconPadding), ((RowCount - y) - 1) * (IconSize.y + IconPadding) };
        pos += IconPadding;

        verts.emplace_back(glm::vec2(pos.x, pos.y + IconSize.y), glm::vec2(textureBounds.left, textureBounds.bottom + textureBounds.height));
        verts.emplace_back(pos, glm::vec2(textureBounds.left, textureBounds.bottom));
        verts.emplace_back(pos + IconSize, glm::vec2(textureBounds.left + textureBounds.width, textureBounds.bottom + textureBounds.height));

        verts.emplace_back(pos + IconSize, glm::vec2(textureBounds.left + textureBounds.width, textureBounds.bottom + textureBounds.height));
        verts.emplace_back(pos, glm::vec2(textureBounds.left, textureBounds.bottom));
        verts.emplace_back(glm::vec2(pos.x + IconSize.x, pos.y), glm::vec2(textureBounds.left + textureBounds.width, textureBounds.bottom));
    }
    entity.getComponent<cro::Drawable2D>().setVertexData(verts);
    browserPage.background.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());


    //highlight
    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ IconPadding, IconPadding + ((RowCount - 1) * (IconPadding + IconSize.y)), 0.1f});
    entity.addComponent<cro::Drawable2D>().setVertexData(createMenuHighlight(IconSize, TextGoldColour));
    entity.getComponent<cro::Drawable2D>().setPrimitiveType(GL_TRIANGLES);

    browserPage.highlight = entity;
    browserPage.background.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());


    //buttons
    
    //TODO rather than capture this ent use a single callback which indexes into the page items array
    //browserPage.selectCallback = m_uiScene.getSystem<cro::UISystem>()->addCallback(
    //    [&, itemID, page](cro::Entity e) mutable
    //    {
    //        m_pageContexts[itemID].pageList[page].highlight.getComponent<cro::Transform>().setPosition(e.getComponent<cro::Transform>().getPosition());
    //        m_pageContexts[itemID].pageList[page].highlight.getComponent<cro::Drawable2D>().setFacing(cro::Drawable2D::Facing::Front);

    //        m_audioEnts[AudioID::Select].getComponent<cro::AudioEmitter>().play();
    //        m_audioEnts[AudioID::Select].getComponent<cro::AudioEmitter>().setPlayingOffset(cro::Time());
    //    });

    constexpr std::size_t IndexOffset = 100;
    for (auto j = RangeStart; j < RangeEnd; ++j)
    {
        //the Y order is reversed so that the navigation
        //direction of keys/controller is correct in the grid
        std::size_t x = (j - RangeStart) % ThumbColCount;
        std::size_t y = (RowCount - 1) - ((j - RangeStart) / ThumbColCount);

        glm::vec2 pos = { x * (IconSize.x + IconPadding), y * (IconSize.y + IconPadding) };
        pos += IconPadding;

        auto inputIndex = ((((RowCount - y) - 1) * ThumbColCount) + x) + RangeStart;

        entity = m_uiScene.createEntity();
        entity.addComponent<cro::Transform>().setPosition(glm::vec3(pos, 0.1f));
        entity.addComponent<cro::UIInput>().setGroup(m_pageContexts[itemID].menuID);
        entity.getComponent<cro::UIInput>().area = { glm::vec2(0.f), IconSize };
        entity.getComponent<cro::UIInput>().setSelectionIndex(IndexOffset + inputIndex);

        std::size_t leftIndex = inputIndex - 1;
        std::size_t rightIndex = inputIndex + 1;
        std::size_t upIndex = inputIndex - ThumbColCount;
        std::size_t downIndex = inputIndex + ThumbColCount;

        //if moving down takes us off the end of the array
        //ie the next row isn't complete, skip to the nav arrow
        if (downIndex > (RangeEnd - 1))
        {
            if (m_pageContexts[itemID].pageHandles.pageTotal > 1)
            {
                downIndex = x < 4 ? PrevArrow : NextArrow;
            }
            else
            {
                //wrap back to top (assumes we never only have one rown on the first page)
                downIndex -= ((RowCount - 1) * ThumbColCount);
            }
        }
        else
        {
            downIndex += IndexOffset;
        }

        const auto itemsThisRow = std::min(ThumbColCount, (RangeEnd - RangeStart) - (((RowCount - y) - 1) * ThumbColCount));

        //these might be the same col (eg if there's only 1 entry this page)
        if (x == 0)
        {
            //left hand column
            leftIndex += itemsThisRow;
        }
        if (x == (itemsThisRow - 1))
        {
            //right hand column
            rightIndex -= itemsThisRow;
        }
        leftIndex += IndexOffset;
        rightIndex += IndexOffset;
        upIndex += IndexOffset;
        
        //these might be the same row
        if (y == 0
            || RowCount == 1)
        {
            //bottom row
            if (m_pageContexts[itemID].pageHandles.pageTotal > 1)
            {
                downIndex = x < 4 ? PrevArrow : NextArrow;
            }
            else
            {
                //wrap back to top (assumes we never only have one row on the first page)
                downIndex = (inputIndex - ((RowCount - 1) * ThumbColCount)) + IndexOffset;
            }
        }
        if (y == (RowCount - 1)
            || RowCount == 1)
        {
            upIndex = CloseButton;
        }

        entity.getComponent<cro::UIInput>().setPrevIndex(leftIndex, upIndex);
        entity.getComponent<cro::UIInput>().setNextIndex(rightIndex, downIndex);

        //this needs to be set per-pagination type so we know which model array to index for description labels
        //entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Selected] = browserPage.selectCallback;
        entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonDown] = m_pageContexts[itemID].activateCallback;
        entity.addComponent<cro::Callback>().setUserData<std::uint8_t>(static_cast<std::uint8_t>(inputIndex));

        browserPage.background.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
        browserPage.items.push_back(entity);
    }


    //hide this page by default
    browserPage.background.getComponent<cro::Transform>().setScale(glm::vec2(0.f));
    browserPage.highlight.getComponent<cro::Transform>().setScale(glm::vec2(0.f));
    for (auto item : browserPage.items)
    {
        item.getComponent<cro::UIInput>().enabled = false;
    }
}

void ProfileState::createBallBrowser(cro::Entity parent, const CallbackContext& ctx)
{
    auto bgEnt = createBrowserBackground(MenuID::BallSelect, ctx);
    m_menuEntities[EntityID::BallBrowser] = bgEnt;
    parent.getComponent<cro::Transform>().addChild(bgEnt.getComponent<cro::Transform>());

    const auto& font = m_sharedData.sharedResources->fonts.get(FontID::UI);
    const auto bgSize = bgEnt.getComponent<cro::Sprite>().getTextureBounds();

    auto entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition(glm::vec3(std::floor(bgSize.width / 2.f), bgSize.height - 38.f, 0.1f));
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Text>(font).setString("Choose Your Ball");
    entity.getComponent<cro::Text>().setCharacterSize(UITextSize);
    entity.getComponent<cro::Text>().setFillColour(TextNormalColour);
    entity.getComponent<cro::Text>().setShadowColour(LeaderboardTextDark);
    entity.getComponent<cro::Text>().setShadowOffset(glm::vec2(1.f, -1.f));
    entity.getComponent<cro::Text>().setAlignment(cro::Text::Alignment::Centre);
    bgEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());

    //calc balls per page
    static constexpr auto BallsPerPage = ThumbColCount * ThumbRowCount;

    //calc number of pages
    const auto PageCount = (m_ballModels.size() / BallsPerPage) + std::min(std::size_t(1), m_ballModels.size() % BallsPerPage);

    //add arrows
    if (PageCount > 1)
    {
        //this func sets up the input callback, and stores
        //UV data in cro::Callback::setUserData() - don't modify this!
        auto& ballPageHandles = m_pageContexts[PaginationID::Balls].pageHandles;

        ballPageHandles.prevButton = ctx.createArrow(1);
        ballPageHandles.prevButton.getComponent<cro::UIInput>().setGroup(MenuID::BallSelect);
        ballPageHandles.prevButton.getComponent<cro::UIInput>().setSelectionIndex(PrevArrow);
        ballPageHandles.prevButton.getComponent<cro::UIInput>().setNextIndex(NextArrow);
        ballPageHandles.prevButton.getComponent<cro::UIInput>().setPrevIndex(NextArrow);
        ballPageHandles.prevButton.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonDown] =
            m_uiScene.getSystem<cro::UISystem>()->addCallback(
                [&](cro::Entity, const cro::ButtonEvent& evt) 
                {
                    if (activated(evt))
                    {
                        prevPage(PaginationID::Balls);
                    }
                });
        ballPageHandles.prevButton.getComponent<cro::Transform>().setPosition(glm::vec2(192.f, 25.f));
        bgEnt.getComponent<cro::Transform>().addChild(ballPageHandles.prevButton.getComponent<cro::Transform>());

        ballPageHandles.nextButton = ctx.createArrow(0);
        ballPageHandles.nextButton.getComponent<cro::UIInput>().setGroup(MenuID::BallSelect);
        ballPageHandles.nextButton.getComponent<cro::UIInput>().setSelectionIndex(NextArrow);
        ballPageHandles.nextButton.getComponent<cro::UIInput>().setNextIndex(PrevArrow, CloseButton);
        ballPageHandles.nextButton.getComponent<cro::UIInput>().setPrevIndex(PrevArrow);
        ballPageHandles.nextButton.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonDown] =
            m_uiScene.getSystem<cro::UISystem>()->addCallback(
                [&](cro::Entity, const cro::ButtonEvent& evt)
                {
                    if (activated(evt))
                    {
                        nextPage(PaginationID::Balls);
                    }
                });
        ballPageHandles.nextButton.getComponent<cro::Transform>().setPosition(glm::vec2(bgSize.width - 192.f - 16.f, 25.f));
        bgEnt.getComponent<cro::Transform>().addChild(ballPageHandles.nextButton.getComponent<cro::Transform>());

        ballPageHandles.pageTotal = PageCount;
        ballPageHandles.pageCount = m_uiScene.createEntity();
        ballPageHandles.pageCount.addComponent<cro::Transform>().setPosition(glm::vec3(std::floor(bgSize.width / 2.f), 37.f, 0.1f));
        ballPageHandles.pageCount.addComponent<cro::Drawable2D>();
        ballPageHandles.pageCount.addComponent<cro::Text>(font).setCharacterSize(UITextSize);
        ballPageHandles.pageCount.getComponent<cro::Text>().setFillColour(TextNormalColour);
        ballPageHandles.pageCount.getComponent<cro::Text>().setAlignment(cro::Text::Alignment::Centre);
        ballPageHandles.pageCount.getComponent<cro::Text>().setString("1/" + std::to_string(PageCount));
        bgEnt.getComponent<cro::Transform>().addChild(ballPageHandles.pageCount.getComponent<cro::Transform>());
    }


    //item label
    const auto& smallFont = m_sharedData.sharedResources->fonts.get(FontID::Info);

    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ bgSize.width / 2.f, 13.f, 0.1f });
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Text>(smallFont).setString(" ");
    entity.getComponent<cro::Text>().setCharacterSize(InfoTextSize);
    entity.getComponent<cro::Text>().setFillColour(TextNormalColour);
    entity.getComponent<cro::Text>().setAlignment(cro::Text::Alignment::Centre);
    bgEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
    m_pageContexts[PaginationID::Balls].pageHandles.itemLabel = entity;

    m_pageContexts[PaginationID::Balls].itemCount = m_ballModels.size();
    m_pageContexts[PaginationID::Balls].menuID = MenuID::BallSelect;
    m_pageContexts[PaginationID::Balls].activateCallback = 
        m_uiScene.getSystem<cro::UISystem>()->addCallback(
        [&](cro::Entity e, const cro::ButtonEvent& evt)
        {
            auto quitMenu = [&]()
                {
                    m_menuEntities[EntityID::BallBrowser].getComponent<cro::Callback>().active = true;
                    m_audioEnts[AudioID::Back].getComponent<cro::AudioEmitter>().play();
                };

            if (activated(evt))
            {
                //apply selection
                setBallIndex(e.getComponent<cro::Callback>().getUserData<std::uint8_t>());
                quitMenu();
            }
            else if (deactivated(evt))
            {
                quitMenu();
            }
        });

    //for each page - this tests if arrows were created (above)
    for (auto i = 0u; i < PageCount; ++i)
    {
        createItemPage(bgEnt, i, PaginationID::Balls);

        //shame model arrays are unique, else we could
        //recycle this for all paging types...
        auto selectCallback = m_uiScene.getSystem<cro::UISystem>()->addCallback(
            [&, i](cro::Entity e) mutable
            {
                m_pageContexts[PaginationID::Balls].pageList[i].highlight.getComponent<cro::Transform>().setPosition(e.getComponent<cro::Transform>().getPosition());
                m_pageContexts[PaginationID::Balls].pageList[i].highlight.getComponent<cro::Drawable2D>().setFacing(cro::Drawable2D::Facing::Front);

                const auto itemIndex = e.getComponent<cro::Callback>().getUserData<std::uint8_t>();
                if (!m_sharedData.ballInfo[itemIndex].label.empty())
                {
                    m_pageContexts[PaginationID::Balls].pageHandles.itemLabel.getComponent<cro::Text>().setString(m_sharedData.ballInfo[itemIndex].label);
                }
                else
                {
                    m_pageContexts[PaginationID::Balls].pageHandles.itemLabel.getComponent<cro::Text>().setString(" ");
                }

                m_audioEnts[AudioID::Select].getComponent<cro::AudioEmitter>().play();
                m_audioEnts[AudioID::Select].getComponent<cro::AudioEmitter>().setPlayingOffset(cro::Time());
            });
        for (auto& item : m_pageContexts[PaginationID::Balls].pageList[i].items)
        {
            item.getComponent<cro::UIInput>().callbacks[cro::UIInput::Selected] = selectCallback;
        }
    }
    activatePage(PaginationID::Balls, m_pageContexts[PaginationID::Balls].pageIndex, true);
}

void ProfileState::createHairBrowser(cro::Entity parent, const CallbackContext& ctx)
{
    auto bgEnt = createBrowserBackground(MenuID::HairSelect, ctx);
    m_menuEntities[EntityID::HairBrowser] = bgEnt;
    parent.getComponent<cro::Transform>().addChild(bgEnt.getComponent<cro::Transform>());

    const auto& font = m_sharedData.sharedResources->fonts.get(FontID::UI);
    const auto bgSize = bgEnt.getComponent<cro::Sprite>().getTextureBounds();

    auto entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition(glm::vec3(std::floor(bgSize.width / 2.f), bgSize.height - 38.f, 0.1f));
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Text>(font).setString("Choose Your Headwear");
    entity.getComponent<cro::Text>().setCharacterSize(UITextSize);
    entity.getComponent<cro::Text>().setFillColour(TextNormalColour);
    entity.getComponent<cro::Text>().setShadowColour(LeaderboardTextDark);
    entity.getComponent<cro::Text>().setShadowOffset(glm::vec2(1.f, -1.f));
    entity.getComponent<cro::Text>().setAlignment(cro::Text::Alignment::Centre);
    bgEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());


    //calc hair per page
    static constexpr auto HairPerPage = ThumbColCount * ThumbRowCount;

    //calc number of pages
    const auto PageCount = (m_avatarHairModels.size() / HairPerPage) + std::min(std::size_t(1), m_avatarHairModels.size() % HairPerPage);

    //add arrows
    if (PageCount > 1)
    {
        //this func sets up the input callback, and stores
        //UV data in cro::Callback::setUserData() - don't modify this!
        auto& pageHandles = m_pageContexts[PaginationID::Hair].pageHandles;

        pageHandles.prevButton = ctx.createArrow(1);
        pageHandles.prevButton.getComponent<cro::UIInput>().setGroup(MenuID::HairSelect);
        pageHandles.prevButton.getComponent<cro::UIInput>().setSelectionIndex(PrevArrow);
        pageHandles.prevButton.getComponent<cro::UIInput>().setNextIndex(NextArrow);
        pageHandles.prevButton.getComponent<cro::UIInput>().setPrevIndex(NextArrow);
        pageHandles.prevButton.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonDown] =
            m_uiScene.getSystem<cro::UISystem>()->addCallback(
                [&](cro::Entity, const cro::ButtonEvent& evt)
                {
                    if (activated(evt))
                    {
                        prevPage(PaginationID::Hair);
                    }
                });
        pageHandles.prevButton.getComponent<cro::Transform>().setPosition(glm::vec2(192.f, 25.f));
        bgEnt.getComponent<cro::Transform>().addChild(pageHandles.prevButton.getComponent<cro::Transform>());

        pageHandles.nextButton = ctx.createArrow(0);
        pageHandles.nextButton.getComponent<cro::UIInput>().setGroup(MenuID::HairSelect);
        pageHandles.nextButton.getComponent<cro::UIInput>().setSelectionIndex(NextArrow);
        pageHandles.nextButton.getComponent<cro::UIInput>().setNextIndex(PrevArrow, CloseButton);
        pageHandles.nextButton.getComponent<cro::UIInput>().setPrevIndex(PrevArrow);
        pageHandles.nextButton.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonDown] =
            m_uiScene.getSystem<cro::UISystem>()->addCallback(
                [&](cro::Entity, const cro::ButtonEvent& evt)
                {
                    if (activated(evt))
                    {
                        nextPage(PaginationID::Hair);
                    }
                });
        pageHandles.nextButton.getComponent<cro::Transform>().setPosition(glm::vec2(bgSize.width - 192.f - 16.f, 25.f));
        bgEnt.getComponent<cro::Transform>().addChild(pageHandles.nextButton.getComponent<cro::Transform>());

        pageHandles.pageTotal = PageCount;
        pageHandles.pageCount = m_uiScene.createEntity();
        pageHandles.pageCount.addComponent<cro::Transform>().setPosition(glm::vec3(std::floor(bgSize.width / 2.f), 37.f, 0.1f));
        pageHandles.pageCount.addComponent<cro::Drawable2D>();
        pageHandles.pageCount.addComponent<cro::Text>(font).setCharacterSize(UITextSize);
        pageHandles.pageCount.getComponent<cro::Text>().setFillColour(TextNormalColour);
        pageHandles.pageCount.getComponent<cro::Text>().setAlignment(cro::Text::Alignment::Centre);
        pageHandles.pageCount.getComponent<cro::Text>().setString("1/" + std::to_string(PageCount));
        bgEnt.getComponent<cro::Transform>().addChild(pageHandles.pageCount.getComponent<cro::Transform>());
    }

    //item label - TODO we should probably be creating this as part of the background
    const auto& smallFont = m_sharedData.sharedResources->fonts.get(FontID::Info);

    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({bgSize.width / 2.f, 13.f, 0.1f});
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Text>(smallFont).setString(" ");
    entity.getComponent<cro::Text>().setCharacterSize(InfoTextSize);
    entity.getComponent<cro::Text>().setFillColour(TextNormalColour);
    entity.getComponent<cro::Text>().setAlignment(cro::Text::Alignment::Centre);
    bgEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
    m_pageContexts[PaginationID::Hair].pageHandles.itemLabel = entity;

    m_pageContexts[PaginationID::Hair].itemCount = m_avatarHairModels.size();
    m_pageContexts[PaginationID::Hair].menuID = MenuID::HairSelect;
    m_pageContexts[PaginationID::Hair].activateCallback =
        m_uiScene.getSystem<cro::UISystem>()->addCallback(
            [&](cro::Entity e, const cro::ButtonEvent& evt)
            {
                auto quitMenu = [&]()
                    {
                        m_menuEntities[EntityID::HairBrowser].getComponent<cro::Callback>().active = true;
                        m_audioEnts[AudioID::Back].getComponent<cro::AudioEmitter>().play();
                    };

                if (activated(evt))
                {
                    //apply selection
                    setHairIndex(e.getComponent<cro::Callback>().getUserData<std::uint8_t>());
                    quitMenu();
                }
                else if (deactivated(evt))
                {
                    quitMenu();
                }
            });

    //for each page - this tests if arrows were created (above)
    for (auto i = 0u; i < PageCount; ++i)
    {
        createItemPage(bgEnt, i, PaginationID::Hair);
        //TODO if we only had one highlight we could do this for
        //all items in the browser, rather than per-page
        auto selectCallback = m_uiScene.getSystem<cro::UISystem>()->addCallback(
            [&, i](cro::Entity e) mutable
            {
                m_pageContexts[PaginationID::Hair].pageList[i].highlight.getComponent<cro::Transform>().setPosition(e.getComponent<cro::Transform>().getPosition());
                m_pageContexts[PaginationID::Hair].pageList[i].highlight.getComponent<cro::Drawable2D>().setFacing(cro::Drawable2D::Facing::Front);

                const auto itemIndex = e.getComponent<cro::Callback>().getUserData<std::uint8_t>();
                if (!m_sharedData.hairInfo[itemIndex].label.empty())
                {
                    m_pageContexts[PaginationID::Hair].pageHandles.itemLabel.getComponent<cro::Text>().setString(m_sharedData.hairInfo[itemIndex].label);
                }
                else
                {
                    m_pageContexts[PaginationID::Hair].pageHandles.itemLabel.getComponent<cro::Text>().setString(" ");
                }
                m_audioEnts[AudioID::Select].getComponent<cro::AudioEmitter>().play();
                m_audioEnts[AudioID::Select].getComponent<cro::AudioEmitter>().setPlayingOffset(cro::Time());
            });

        for (auto& item : m_pageContexts[PaginationID::Hair].pageList[i].items)
        {
            item.getComponent<cro::UIInput>().callbacks[cro::UIInput::Selected] = selectCallback;
        }
    }
    activatePage(PaginationID::Hair, m_pageContexts[PaginationID::Hair].pageIndex, true);
}

cro::Entity ProfileState::createBrowserBackground(std::int32_t menuID, const CallbackContext& ctx)
{
    auto entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>();
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>() = ctx.spriteSheet.getSprite("background");
    auto bounds = entity.getComponent<cro::Sprite>().getTextureBounds();
    entity.getComponent<cro::Transform>().setPosition({ -std::floor(bounds.width / 2.f), -std::floor(bounds.height / 2.f), 1.f });
    entity.getComponent<cro::Transform>().setScale(glm::vec2(0.f));

    struct BGCallbackData final
    {
        float progress = 0.f;
        std::int32_t direction = 0; //0 grow 1 shrink
    };
    entity.addComponent<cro::Callback>().setUserData<BGCallbackData>();
    entity.getComponent<cro::Callback>().function =
        [&, menuID](cro::Entity e, float dt)
        {
            glm::vec2 scale(1.f);

            auto& [currTime, dir] = e.getComponent<cro::Callback>().getUserData<BGCallbackData>();
            if (dir == 0)
            {
                //grow
                currTime = std::min(currTime + (dt * 2.f), 1.f);
                scale.x = cro::Util::Easing::easeOutQuint(currTime);

                if (currTime == 1)
                {
                    m_uiScene.getSystem<cro::UISystem>()->setActiveGroup(menuID);
                    dir = 1;
                    e.getComponent<cro::Callback>().active = false;
                }
            }
            else
            {
                //shrink
                currTime = std::max(0.f, currTime - (dt * 2.f));
                scale.y = cro::Util::Easing::easeInQuint(currTime);

                if (currTime == 0)
                {
                    m_uiScene.getSystem<cro::UISystem>()->setActiveGroup(MenuID::Main);
                    m_uiScene.getSystem<cro::UISystem>()->selectAt(m_lastSelected);
                    dir = 0;
                    e.getComponent<cro::Callback>().active = false;
                }
            }
            e.getComponent<cro::Transform>().setScale(scale);
        };


    auto buttonEnt = m_uiScene.createEntity();
    buttonEnt.addComponent<cro::Transform>().setPosition({ 468.f, 331.f, 0.1f });
    buttonEnt.addComponent<cro::AudioEmitter>() = m_menuSounds.getEmitter("switch");
    buttonEnt.addComponent<cro::Drawable2D>();
    buttonEnt.addComponent<cro::Sprite>() = ctx.spriteSheet.getSprite("close_button");
    buttonEnt.getComponent<cro::Sprite>().setColour(cro::Colour::Transparent);
    buttonEnt.addComponent<cro::UIInput>().setGroup(menuID);
    buttonEnt.getComponent<cro::UIInput>().setSelectionIndex(CloseButton);
    bounds = buttonEnt.getComponent<cro::Sprite>().getTextureBounds();
    buttonEnt.getComponent<cro::UIInput>().area = bounds;
    buttonEnt.getComponent<cro::UIInput>().callbacks[cro::UIInput::Selected] = ctx.closeSelected;
    buttonEnt.getComponent<cro::UIInput>().callbacks[cro::UIInput::Unselected] = ctx.closeUnselected;
    buttonEnt.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonUp] =
        m_uiScene.getSystem<cro::UISystem>()->addCallback(
            [&, entity](cro::Entity e, const cro::ButtonEvent& evt) mutable
            {
                if (activated(evt))
                {
                    m_uiScene.getSystem<cro::UISystem>()->setActiveGroup(MenuID::Dummy);
                    entity.getComponent<cro::Callback>().active = true;
                    m_audioEnts[AudioID::Accept].getComponent<cro::AudioEmitter>().play();
                }
            });

    buttonEnt.addComponent<cro::Callback>().function = MenuTextCallback();
    buttonEnt.getComponent<cro::Transform>().setOrigin({ bounds.width / 2.f, bounds.height / 2.f });
    entity.getComponent<cro::Transform>().addChild(buttonEnt.getComponent<cro::Transform>());

    return entity;
}

void ProfileState::quitState()
{
    auto currentMenu = m_uiScene.getSystem<cro::UISystem>()->getActiveGroup();
    if (currentMenu == MenuID::Main)
    {
        applyTextEdit();

        m_menuEntities[EntityID::Root].getComponent<cro::Callback>().active = true;
        m_audioEnts[AudioID::Back].getComponent<cro::AudioEmitter>().play();
    }
    //m_uiScene.setSystemActive<cro::AudioPlayerSystem>(false);
}

cro::Entity ProfileState::createPaletteBackground(cro::Entity parent, FlyoutMenu& target, std::uint32_t i)
{
    const auto RowCount = (pc::PairCounts[i] / PaletteColumnCount);
    const float BgHeight = (RowCount * Flyout::IconSize.y) + (RowCount * Flyout::IconPadding) + Flyout::IconPadding;

    //background
    auto entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition(glm::vec3(SwatchPositions[i], 0.5f));
    entity.getComponent<cro::Transform>().setScale(glm::vec2(0.f));
    entity.addComponent<cro::Drawable2D>().setPrimitiveType(GL_TRIANGLES);

    auto verts = createMenuBackground({ Flyout::BgWidth, BgHeight });

    for (auto j = 0u; j < pc::PairCounts[i]; ++j)
    {
        std::size_t x = j % PaletteColumnCount;
        std::size_t y = j / PaletteColumnCount;
        auto colour = pc::Palette[j];

        glm::vec2 pos = { x * (Flyout::IconSize.x + Flyout::IconPadding), y * (Flyout::IconSize.y + Flyout::IconPadding) };
        pos += Flyout::IconPadding;

        verts.emplace_back(glm::vec2(pos.x, pos.y + Flyout::IconSize.y), colour);
        verts.emplace_back(pos, colour);
        verts.emplace_back(pos + Flyout::IconSize, colour);

        verts.emplace_back(pos + Flyout::IconSize, colour);
        verts.emplace_back(pos, colour);
        verts.emplace_back(glm::vec2(pos.x + Flyout::IconSize.x, pos.y), colour);
    }
    entity.getComponent<cro::Drawable2D>().setVertexData(verts);

    target.background = entity;
    parent.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());


    //highlight
    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ Flyout::IconPadding, Flyout::IconPadding, 0.1f });
    entity.addComponent<cro::Drawable2D>().setVertexData(createMenuHighlight(Flyout::IconSize, TextGoldColour));
    entity.getComponent<cro::Drawable2D>().setPrimitiveType(GL_TRIANGLES);

    target.highlight = entity;
    target.background.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());

    return entity;
}

void ProfileState::createPaletteButtons(FlyoutMenu& target, std::uint32_t i, std::uint32_t menuID, std::size_t indexOffset)
{
    const auto RowCount = (pc::PairCounts[i] / PaletteColumnCount);

    for (auto j = 0u; j < pc::PairCounts[i]; ++j)
    {
        //the Y order is reversed so that the navigation
        //direction of keys/controller is correct in the grid
        std::size_t x = j % PaletteColumnCount;
        std::size_t y = (RowCount - 1) - (j / PaletteColumnCount);

        glm::vec2 pos = { x * (Flyout::IconSize.x + Flyout::IconPadding), y * (Flyout::IconSize.y + Flyout::IconPadding) };
        pos += Flyout::IconPadding;

        auto inputIndex = (((RowCount - y) - 1) * PaletteColumnCount) + x;

        auto entity = m_uiScene.createEntity();
        entity.addComponent<cro::Transform>().setPosition(glm::vec3(pos, 0.1f));
        entity.addComponent<cro::UIInput>().setGroup(menuID);
        entity.getComponent<cro::UIInput>().area = { glm::vec2(0.f), Flyout::IconSize };
        entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Selected] = target.selectCallback;
        entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonUp] = target.activateCallback;


        entity.getComponent<cro::UIInput>().setSelectionIndex(indexOffset + inputIndex);
        if (x == 0)
        {
            auto prevIndex = std::min(inputIndex + (PaletteColumnCount - 1), pc::PairCounts[i] - 1);
            entity.getComponent<cro::UIInput>().setPrevIndex(indexOffset + prevIndex);
        }
        else if (x == (PaletteColumnCount - 1))
        {
            entity.getComponent<cro::UIInput>().setNextIndex(indexOffset + (inputIndex - (PaletteColumnCount - 1)));
        }
        else if (y == 0
            && x == (pc::PairCounts[i] % PaletteColumnCount) - 1)
        {
            auto nextIndex = inputIndex - ((pc::PairCounts[i] % PaletteColumnCount) - 1);
            entity.getComponent<cro::UIInput>().setNextIndex(indexOffset + nextIndex);
        }


        entity.addComponent<cro::Callback>().setUserData<std::uint8_t>(static_cast<std::uint8_t>((y * PaletteColumnCount) + x));

        target.background.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
    }
}

std::size_t ProfileState::indexFromAvatarID(std::uint32_t skinID) const
{
    const auto& avatarInfo = m_sharedData.avatarInfo;

    if (auto result = std::find_if(avatarInfo.cbegin(), avatarInfo.cend(), 
        [skinID](const SharedStateData::AvatarInfo& a){return a.uid == skinID;}); result != avatarInfo.cend())
    {
        return std::distance(avatarInfo.cbegin(), result);
    }

    return 0;
}

std::size_t ProfileState::indexFromBallID(std::uint32_t ballID) const
{
    const auto& ballInfo = m_sharedData.ballInfo;
    if (auto result = std::find_if(ballInfo.cbegin(), ballInfo.cend(),
        [ballID](const SharedStateData::BallInfo& b)
        {
            return b.uid == ballID;
        }); result != ballInfo.cend())
    {
        return std::distance(ballInfo.cbegin(), result);
    }

    return 0;
}

std::size_t ProfileState::indexFromHairID(std::uint32_t hairID) const
{
    const auto& hairInfo = m_sharedData.hairInfo;
    if (auto result = std::find_if(hairInfo.cbegin(), hairInfo.cend(), 
        [hairID](const SharedStateData::HairInfo& hi) {return hi.uid == hairID;}); result != hairInfo.end())
    {
        return std::distance(hairInfo.begin(), result);
    }
    return 0;
}

void ProfileState::setAvatarIndex(std::size_t idx)
{
    auto hairIdx = m_avatarModels[m_avatarIndex].hairIndex;
    if (m_avatarModels[m_avatarIndex].hairAttachment)
    {
        m_avatarModels[m_avatarIndex].hairAttachment->setModel({});
    }

    m_avatarModels[m_avatarIndex].previewModel.getComponent<cro::Callback>().active = true;
    m_avatarModels[m_avatarIndex].previewModel.getComponent<cro::Callback>().getUserData<AvatarAnimCallbackData>().direction = 1;

    //we might have blank spots so skip these
    if (m_avatarIndex == 0 || idx < m_avatarIndex)
    {
        while (!m_avatarModels[idx].previewModel.isValid()
            && idx != m_avatarIndex)
        {
            idx = (idx + (m_avatarModels.size() -1)) % m_avatarModels.size();
        }
    }
    else
    {
        while (!m_avatarModels[idx].previewModel.isValid()
            && idx != m_avatarIndex)
        {
            idx = (idx + 1) % m_avatarModels.size();
        }
    }

    m_avatarIndex = idx;

    if (m_avatarModels[m_avatarIndex].hairAttachment)
    {
        m_avatarModels[m_avatarIndex].hairAttachment->setModel(m_avatarHairModels[hairIdx]);
        m_avatarModels[m_avatarIndex].hairIndex = hairIdx;
    }
    m_avatarModels[m_avatarIndex].previewModel.getComponent<cro::Model>().setHidden(false);
    m_avatarModels[m_avatarIndex].previewModel.getComponent<cro::Callback>().active = true;
    m_avatarModels[m_avatarIndex].previewModel.getComponent<cro::Callback>().getUserData<AvatarAnimCallbackData>().direction = 0;

    m_activeProfile.skinID = m_sharedData.avatarInfo[m_avatarIndex].uid;

    for (auto i = 0u; i < m_activeProfile.avatarFlags.size(); ++i)
    {
        m_profileTextures[idx].setColour(pc::ColourKey::Index(i), m_activeProfile.avatarFlags[i]);
    }
    m_profileTextures[idx].apply();


    if (!m_avatarModels[m_avatarIndex].previewAudio.empty())
    {
        m_avatarModels[m_avatarIndex].previewAudio[cro::Util::Random::value(0u, m_avatarModels[m_avatarIndex].previewAudio.size() - 1)].getComponent<cro::AudioEmitter>().play();
    }
}

void ProfileState::setHairIndex(std::size_t idx)
{
    auto hairIndex = m_avatarModels[m_avatarIndex].hairIndex;

    if (m_avatarHairModels[hairIndex].isValid())
    {
        m_avatarHairModels[hairIndex].getComponent<cro::Model>().setHidden(true);
    }
    hairIndex = idx;
    if (m_avatarHairModels[hairIndex].isValid())
    {
        m_avatarHairModels[hairIndex].getComponent<cro::Model>().setHidden(false);
    }

    if (m_avatarModels[m_avatarIndex].hairAttachment
        && m_avatarHairModels[hairIndex].isValid())
    {
        m_avatarModels[m_avatarIndex].hairAttachment->setModel(m_avatarHairModels[hairIndex]);
        m_avatarHairModels[hairIndex].getComponent<cro::Model>().setMaterialProperty(0, "u_hairColour", pc::Palette[m_activeProfile.avatarFlags[0]]);
    }
    m_avatarModels[m_avatarIndex].hairIndex = hairIndex;

    m_activeProfile.hairID = m_sharedData.hairInfo[hairIndex].uid;
}

void ProfileState::setBallIndex(std::size_t idx)
{
    CRO_ASSERT(idx < m_ballModels.size(), "");

    if (m_ballHairModels[m_ballHairIndex].isValid())
    {
        m_ballModels[m_ballIndex].root.getComponent<cro::Transform>().removeChild(m_ballHairModels[m_ballHairIndex].getComponent<cro::Transform>());
    }
    m_ballModels[m_ballIndex].ball.getComponent<cro::Model>().setHidden(true);

    m_ballIndex = idx;

    if (m_ballHairModels[m_ballHairIndex].isValid())
    {
        m_ballModels[m_ballIndex].root.getComponent<cro::Transform>().addChild(m_ballHairModels[m_ballHairIndex].getComponent<cro::Transform>());
    }
    m_ballModels[m_ballIndex].ball.getComponent<cro::Model>().setHidden(false);
    m_ballModels[m_ballIndex].ball.getComponent<cro::Model>().setMaterialProperty(0, "u_ballColour", m_activeProfile.ballColour);

    m_ballModels[m_ballIndex].root.getComponent<cro::ParticleEmitter>().start();

    m_activeProfile.ballID = m_sharedData.ballInfo[m_ballIndex].uid;
}

void ProfileState::nextPage(std::int32_t itemID)
{
    const auto page = (m_pageContexts[itemID].pageIndex + 1) % m_pageContexts[itemID].pageList.size();
    
    activatePage(itemID, page, false);
    m_uiScene.getSystem<cro::UISystem>()->selectAt(NextArrow);
}

void ProfileState::prevPage(std::int32_t itemID)
{
    auto page = (m_pageContexts[itemID].pageIndex + (m_pageContexts[itemID].pageList.size() - 1))
        % m_pageContexts[itemID].pageList.size();
    activatePage(itemID, page, false);
    m_uiScene.getSystem<cro::UISystem>()->selectAt(PrevArrow);
}

void ProfileState::activatePage(std::int32_t itemID, std::size_t page, bool forceRefresh)
{
    if (m_uiScene.getSystem<cro::UISystem>()->getActiveGroup() == m_pageContexts[itemID].menuID
        || forceRefresh)
    {
        auto& pages = m_pageContexts[itemID].pageList;
        auto& pageIndex = m_pageContexts[itemID].pageIndex;

        pages[pageIndex].background.getComponent<cro::Transform>().setScale(glm::vec2(0.f));
        pages[pageIndex].highlight.getComponent<cro::Transform>().setScale(glm::vec2(0.f));
        for (auto item : pages[pageIndex].items)
        {
            item.getComponent<cro::UIInput>().enabled = false;
        }

        pageIndex = page;

        pages[pageIndex].background.getComponent<cro::Transform>().setScale(glm::vec2(1.f));
        pages[pageIndex].highlight.getComponent<cro::Transform>().setScale(glm::vec2(1.f));
        for (auto item : pages[pageIndex].items)
        {
            item.getComponent<cro::UIInput>().enabled = true;
        }


        auto& pageHandles = m_pageContexts[itemID].pageHandles;
        if (pageHandles.pageCount.isValid())
        {
            pageHandles.pageCount.getComponent<cro::Text>().setString(std::to_string(page + 1) + "/" + std::to_string(pageHandles.pageTotal));
        }

        if (pageHandles.prevButton.isValid())
        {
            auto itemIndex = std::min(std::size_t(3), pages[pageIndex].items.size() - 1);
            auto downIndex = pages[pageIndex].items[itemIndex].getComponent<cro::UIInput>().getSelectionIndex();
            pageHandles.prevButton.getComponent<cro::UIInput>().setNextIndex(NextArrow, downIndex);

            itemIndex += ThumbColCount * 3;
            while (itemIndex >= pages[pageIndex].items.size())
            {
                if (itemIndex > ThumbColCount)
                {
                    itemIndex -= ThumbColCount;
                }
                else
                {
                    itemIndex = std::min(std::size_t(3), pages[pageIndex].items.size() - 1);
                }
            }

            auto upIndex = pages[pageIndex].items[itemIndex].getComponent<cro::UIInput>().getSelectionIndex();
            pageHandles.prevButton.getComponent<cro::UIInput>().setPrevIndex(NextArrow, upIndex);
        }

        if (pageHandles.nextButton.isValid())
        {
            auto itemIndex = std::min(std::size_t(4), pages[pageIndex].items.size() - 1);
            itemIndex += ThumbColCount * 3;
            while (itemIndex >= pages[pageIndex].items.size())
            {
                if (itemIndex > ThumbColCount)
                {
                    itemIndex -= ThumbColCount;
                }
                else
                {
                    itemIndex = std::min(std::size_t(4), pages[pageIndex].items.size() - 1);
                }
            }

            auto upIndex = pages[pageIndex].items[itemIndex].getComponent<cro::UIInput>().getSelectionIndex();
            pageHandles.nextButton.getComponent<cro::UIInput>().setPrevIndex(PrevArrow, upIndex);
        }

        if (!forceRefresh)
        {
            m_uiScene.getSystem<cro::UISystem>()->selectAt(pages[pageIndex].items[0].getComponent<cro::UIInput>().getSelectionIndex());

            m_audioEnts[AudioID::Accept].getComponent<cro::AudioEmitter>().play();
            m_audioEnts[AudioID::Accept].getComponent<cro::AudioEmitter>().setPlayingOffset(cro::seconds(0.f));
        }
    }
}

void ProfileState::refreshMugshot()
{
    if (!m_activeProfile.mugshot.empty())
    {
        auto& tex = m_sharedData.sharedResources->textures.get(m_activeProfile.mugshot);

        glm::vec2 texSize(tex.getSize());
        glm::vec2 scale = glm::vec2(96.f, 48.f) / texSize;
        m_menuEntities[EntityID::Mugshot].getComponent<cro::Transform>().setScale(scale);
        m_menuEntities[EntityID::Mugshot].getComponent<cro::Sprite>().setTexture(tex);
    }
    else
    {
        m_menuEntities[EntityID::Mugshot].getComponent<cro::Transform>().setScale(glm::vec2(0.f));
    }
}

void ProfileState::refreshNameString()
{
    m_menuEntities[EntityID::NameText].getComponent<cro::Text>().setString(m_activeProfile.name);
    centreText(m_menuEntities[EntityID::NameText]);
}

void ProfileState::refreshSwatch()
{
    static constexpr glm::vec2 SwatchSize = glm::vec2(20.f);

    std::vector<cro::Vertex2D> verts;
    for (auto i = 0u; i < SwatchPositions.size(); ++i)
    {
        cro::Colour c = pc::Palette[m_activeProfile.avatarFlags[i]];
        auto pos = SwatchPositions[i];

        verts.emplace_back(glm::vec2(pos.x, pos.y + SwatchSize.y), c);
        verts.emplace_back(pos, c);
        verts.emplace_back(pos + SwatchSize, c);
        
        verts.emplace_back(pos + SwatchSize, c);
        verts.emplace_back(pos, c);
        verts.emplace_back(glm::vec2(pos.x + SwatchSize.x, pos.y), c);
    }
    m_menuEntities[EntityID::Swatch].getComponent<cro::Drawable2D>().setVertexData(verts);
}

void ProfileState::refreshBio()
{
    //look for bio file and load it if it exists
    auto path = Social::getUserContentPath(Social::UserContent::Profile);
    if (cro::FileSystem::directoryExists(path))
    {
        path += m_activeProfile.profileID + "/";

        if (cro::FileSystem::directoryExists(path))
        {
            path += "bio.txt";

            if (cro::FileSystem::fileExists(path))
            {
                std::vector<char> buffer(MaxBioChars + 1);

                cro::RaiiRWops inFile;
                inFile.file = SDL_RWFromFile(path.c_str(), "r");
                if (inFile.file)
                {
                    auto readCount = inFile.file->read(inFile.file, buffer.data(), 1, MaxBioChars);
                    buffer[readCount] = 0; //nullterm
                    setBioString(buffer.data());
                }
            }
            else
            {
                //else set bio to random and write file
                std::string bio = generateRandomBio();

                cro::RaiiRWops outfile;
                outfile.file = SDL_RWFromFile(path.c_str(), "w");
                if (outfile.file)
                {
                    outfile.file->write(outfile.file, bio.data(), bio.size(), 1);
                }
                setBioString(bio);
            }
        }
        else
        {
            setBioString(generateRandomBio());
        }
    }
}

void ProfileState::beginTextEdit(cro::Entity stringEnt, cro::String* dst, std::size_t maxChars)
{
    *dst = dst->substr(0, maxChars);
    m_previousString = *dst;

    stringEnt.getComponent<cro::Text>().setFillColour(TextEditColour);
    m_textEdit.string = dst;
    m_textEdit.entity = stringEnt;
    m_textEdit.maxLen = maxChars;

    //block input to menu
    m_uiScene.getSystem<cro::UISystem>()->setActiveGroup(MenuID::Dummy);

    SDL_StartTextInput();
}

void ProfileState::handleTextEdit(const cro::Event& evt)
{
    if (!m_textEdit.string)
    {
        return;
    }

    if (evt.type == SDL_KEYDOWN)
    {
        switch (evt.key.keysym.sym)
        {
        default: break;
        case SDLK_BACKSPACE:
            if (!m_textEdit.string->empty())
            {
                m_textEdit.string->erase(m_textEdit.string->size() - 1);
            }
            break;
        case SDLK_v:
            if (evt.key.keysym.mod & KMOD_CTRL)
            {
                if (SDL_HasClipboardText())
                {
                    char* text = SDL_GetClipboardText();
                    auto codePoints = cro::Util::String::getCodepoints(text);
                    SDL_free(text);

                    cro::String str = cro::String::fromUtf32(codePoints.begin(), codePoints.end());
                    auto len = std::min(str.size(), ConstVal::MaxStringChars - m_textEdit.string->size());

                    *m_textEdit.string += str.substr(0, len);
                }
            }
            break;
        }
    }
    else if (evt.type == SDL_KEYUP)
    {
        switch (evt.key.keysym.sym)
        {
        default: break;
        case SDLK_ESCAPE:
            cancelTextEdit();
            break;
        }
    }
    else if (evt.type == SDL_TEXTINPUT)
    {
        if (m_textEdit.string->size() < ConstVal::MaxStringChars
            && m_textEdit.string->size() < m_textEdit.maxLen)
        {
            auto codePoints = cro::Util::String::getCodepoints(evt.text.text);
            *m_textEdit.string += cro::String::fromUtf32(codePoints.begin(), codePoints.end());
        }
    }
}

void ProfileState::cancelTextEdit()
{
    *m_textEdit.string = m_previousString;
    applyTextEdit();

    //strictly speaking this should be whichever entity
    //that just cancelled editing - but m_textEdit is reset
    //after having applied the edit...
    centreText(m_menuEntities[EntityID::NameText]);

    m_previousString.clear();
}

bool ProfileState::applyTextEdit()
{
    if (m_textEdit.string && m_textEdit.entity.isValid())
    {
        if (m_textEdit.string->empty())
        {
            *m_textEdit.string = "INVALID";
        }

        m_textEdit.entity.getComponent<cro::Text>().setFillColour(TextNormalColour);
        m_textEdit.entity.getComponent<cro::Text>().setString(*m_textEdit.string);
        m_textEdit.entity.getComponent<cro::Callback>().active = false;


        //send this as a command to delay it by a frame - doesn't matter who receives it :)
        cro::Command cmd;
        cmd.targetFlags = CommandID::Menu::RootNode;
        cmd.action = [&](cro::Entity, float)
        {
            //commandception
            cro::Command cmd2;
            cmd2.targetFlags = CommandID::Menu::RootNode;
            cmd2.action = [&](cro::Entity, float)
            {
                m_uiScene.getSystem<cro::UISystem>()->setActiveGroup(MenuID::Main);
            };
            m_uiScene.getSystem<cro::CommandSystem>()->sendCommand(cmd2);
        };
        m_uiScene.getSystem<cro::CommandSystem>()->sendCommand(cmd);


        SDL_StopTextInput();
        m_textEdit = {};
        return true;
    }
    m_textEdit = {};
    return false;
}

std::string ProfileState::generateRandomBio() const
{
    switch (cro::Util::Random::value(0, 6))
    {
    default:
    case 0:
        return
            u8"This former house-builder knows all about the benefits of elevation. If they play a good shot, you'll certainly be able to see their big beam and they'll hit the roof if they manage a hole in one! As a low-handicapper, tends to play off the builders tee, and enjoys ladder tournaments.";
    case 1:
        return
            u8"A retired gardener this player knows a thing or two about lying in the rough. Don't underestimate them though - they could be considered the rake in the grass!";
    case 2:
        return
            u8"Small feet means nothing - not when you can handle your wood like this.";
    case 3:
        return
            u8"Formerly a countryside resident this player moved to the city to experience the thrills of urban golf. Just don't sneak up on them when they're strumming the banjo.";
    case 4:
        return
            u8"\"Good things come in small packages\" is this player's motto. Apparently they diet exclusively on fortune cookies.";
    case 5:
        return
            u8"The clever use of turn signals got this player to where they are today.";
    case 6:
        return
            u8"Nick-named \"The Midwife\" because they always deliver (and \"Postman\" was already taken) here's a player who knows a comfy lie is much better than a water-berth. Takes a cautious approach as they know it's much better to crawl before you can walk. Becoming a golf pro was their crowning achievement";
    }
}

void ProfileState::setBioString(const std::string& s)
{
    if (s.empty())
    {
        return;
    }

    static constexpr std::size_t MaxWidth = 17;

    auto output = cro::Util::String::wordWrap(s, MaxWidth, MaxBioChars);

    m_menuEntities[EntityID::BioText].getComponent<cro::Transform>().setOrigin({ 0.f, -0.f });
    m_menuEntities[EntityID::BioText].getComponent<cro::Text>().setString(output);
    m_menuEntities[EntityID::BioText].getComponent<cro::Drawable2D>().setCroppingArea(BioCrop);
}

void ProfileState::generateMugshot()
{
    m_mugshotTexture.clear({0xa9c0afff});
    auto& cam = m_cameras[CameraID::Mugshot].getComponent<cro::Camera>();
    cam.viewport = { 0.f, 0.f, 0.5f, 1.f };
    m_cameras[CameraID::Mugshot].getComponent<cro::Transform>().setPosition(MugCameraPosition);
    m_cameras[CameraID::Mugshot].getComponent<cro::Transform>().setRotation(cro::Transform::Y_AXIS, cro::Util::Const::PI);
    m_cameras[CameraID::Mugshot].getComponent<cro::Camera>().updateMatrices(m_cameras[CameraID::Mugshot].getComponent<cro::Transform>());
    m_modelScene.setActiveCamera(m_cameras[CameraID::Mugshot]);
    m_modelScene.render();

    cam.viewport = { 0.5f, 0.f, 0.5f, 1.f };
    m_cameras[CameraID::Mugshot].getComponent<cro::Transform>().setPosition(MugCameraPosition + glm::vec3(-MugCameraPosition.z /*+ 0.05f*/, 0.f, -MugCameraPosition.z));
    m_cameras[CameraID::Mugshot].getComponent<cro::Transform>().setRotation(cro::Transform::Y_AXIS, cro::Util::Const::PI / 2.f);
    m_cameras[CameraID::Mugshot].getComponent<cro::Camera>().updateMatrices(m_cameras[CameraID::Mugshot].getComponent<cro::Transform>());
    m_modelScene.render();

    m_mugshotTexture.display();

    m_mugshotUpdated = true;
    m_menuEntities[EntityID::Mugshot].getComponent<cro::Sprite>().setTexture(m_mugshotTexture.getTexture());
    m_menuEntities[EntityID::Mugshot].getComponent<cro::Transform>().setScale(glm::vec2(0.5f));
}

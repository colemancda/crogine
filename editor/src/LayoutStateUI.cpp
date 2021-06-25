/*-----------------------------------------------------------------------

Matt Marchant 2020 - 2021
http://trederia.blogspot.com

crogine editor - Zlib license.

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

#include "LayoutState.hpp"
#include "UIConsts.hpp"
#include "SharedStateData.hpp"

#include <crogine/ecs/components/Sprite.hpp>
#include <crogine/ecs/components/Camera.hpp>

namespace
{
    enum WindowID
    {
        Inspector,
        Browser,
        Info,

        Count
    };

    std::array<std::pair<glm::vec2, glm::vec2>, WindowID::Count> WindowLayouts =
    {
        std::make_pair(glm::vec2(), glm::vec2())
    };
}

void LayoutState::initUI()
{
    registerWindow([&]()
        {
            drawMenuBar();
            drawInspector();
            drawBrowser();
            drawInfo();
        });

    getContext().appInstance.resetFrameTime();
    getContext().mainWindow.setTitle("Crogine Layout Editor");

    auto size = getContext().mainWindow.getSize();
    updateLayout(size.x, size.y);

    //TODO load the last used workspace (keep a ref to this in the prefs file?)
}

void LayoutState::drawMenuBar()
{
    if (ImGui::BeginMainMenuBar())
    {
        //file menu
        if (ImGui::BeginMenu("File##Particle"))
        {
            if (ImGui::MenuItem("Open##Layout", nullptr, nullptr))
            {

            }
            if (ImGui::MenuItem("Save##Layout", nullptr, nullptr))
            {

            }

            if (ImGui::MenuItem("Save As...##Layout", nullptr, nullptr))
            {

            }

            if (getStateCount() > 1)
            {
                if (ImGui::MenuItem("Return To World Editor"))
                {
                    getContext().mainWindow.setTitle("Crogine Editor");
                    requestStackPop();
                }
            }

            if (ImGui::MenuItem("Quit##Layout", nullptr, nullptr))
            {
                cro::App::quit();
            }
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Edit##layout"))
        {
            if (ImGui::BeginMenu("Add"))
            {
                if (ImGui::MenuItem("Sprite"))
                {

                }
                
                if (ImGui::MenuItem("Text"))
                {

                }
                ImGui::EndMenu();
            }

            if (ImGui::MenuItem("Remove"))
            {
                if (cro::FileSystem::showMessageBox("", "Delete the selected node?", cro::FileSystem::YesNo, cro::FileSystem::IconType::Question))
                {
                    //TODO delete the selected node
                }
            }

            ImGui::EndMenu();
        }

        //view menu
        if (ImGui::BeginMenu("View##Layout"))
        {
            if (ImGui::MenuItem("Options", nullptr, nullptr))
            {
               // m_showPreferences = !m_showPreferences;
            }



            ImGui::EndMenu();
        }

        //workspace menu
        if (ImGui::BeginMenu("Workspace"))
        {
            if (ImGui::MenuItem("Open##workspace"))
            {

            }

            if (ImGui::MenuItem("Save##workspace"))
            {
                //TODO save background info,
                //the layout size
                //and any loaded spritesheets/fonts
                //the layout tree is saved as its own file
            }

            if (ImGui::MenuItem("Set Background"))
            {
                auto path = cro::FileSystem::openFileDialogue(m_sharedData.workingDirectory + "untitled.png", "png,jpg,bmp");
                if (!path.empty())
                {
                    if (m_backgroundTexture.loadFromFile(path))
                    {
                        //technically it's already set, just a lazy way of telling the sprite to resize
                        m_backgroundEntity.getComponent<cro::Sprite>().setTexture(m_backgroundTexture);
                        m_layoutSize = m_backgroundTexture.getSize();
                        updateView2D(m_uiScene.getActiveCamera().getComponent<cro::Camera>());
                    }
                }
            }
            uiConst::showTipMessage("This will resize the the workspace to match the background image if possible");


            /*if (ImGui::MenuItem("Set Size"))
            {

            }*/

            ImGui::EndMenu();
        }

        ImGui::EndMainMenuBar();
    }
}

void LayoutState::drawInspector()
{
    auto [pos, size] = WindowLayouts[WindowID::Inspector];
    ImGui::SetNextWindowPos({ pos.x, pos.y });
    ImGui::SetNextWindowSize({ size.x, size.y });
    if (ImGui::Begin("Inspector##Layout", nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse))
    {
        ImGui::BeginTabBar("Scene");
        if (ImGui::BeginTabItem("Tree"))
        {


            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }
    ImGui::End();
}

void LayoutState::drawBrowser()
{
    auto [pos, size] = WindowLayouts[WindowID::Browser];

    ImGui::SetNextWindowPos({ pos.x, pos.y });
    ImGui::SetNextWindowSize({ size.x, size.y });
    if (ImGui::Begin("Browser##Layout", nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse))
    {

    }

    ImGui::End();
}

void LayoutState::drawInfo()
{
    auto [pos, size] = WindowLayouts[WindowID::Info];
    ImGui::SetNextWindowPos({ pos.x, pos.y });
    ImGui::SetNextWindowSize({ size.x, size.y });
    if (ImGui::Begin("InfoBar##Layout", nullptr, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoScrollbar))
    {
        ImGui::PushStyleColor(ImGuiCol_Text, m_messageColour);
        ImGui::Text("%s", cro::Console::getLastOutput().c_str());
        ImGui::PopStyleColor();

        ImGui::SameLine();
        auto cursor = ImGui::GetCursorPos();
        cursor.y -= 4.f;
        ImGui::SetCursorPos(cursor);

        if (ImGui::Button("More...", ImVec2(56.f, 20.f)))
        {
            cro::Console::show();
        }
    }
    ImGui::End();
}

void LayoutState::updateLayout(std::int32_t w, std::int32_t h)
{
    float width = static_cast<float>(w);
    float height = static_cast<float>(h);
    WindowLayouts[WindowID::Inspector] =
        std::make_pair(glm::vec2(0.f, uiConst::TitleHeight),
            glm::vec2(width * uiConst::InspectorWidth, height - (uiConst::TitleHeight + uiConst::InfoBarHeight)));

    WindowLayouts[WindowID::Browser] =
        std::make_pair(glm::vec2(width * uiConst::InspectorWidth, height - (height * uiConst::BrowserHeight) - uiConst::InfoBarHeight),
            glm::vec2(width - (width * uiConst::InspectorWidth), height * uiConst::BrowserHeight));

    WindowLayouts[WindowID::Info] =
        std::make_pair(glm::vec2(0.f, height - uiConst::InfoBarHeight), glm::vec2(width, uiConst::InfoBarHeight));
}
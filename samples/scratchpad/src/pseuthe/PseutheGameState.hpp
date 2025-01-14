//Auto-generated header file for Scratchpad Stub 20/08/2024, 12:39:17

#pragma once

#include "../StateIDs.hpp"

#include <crogine/core/State.hpp>
#include <crogine/ecs/Scene.hpp>
#include <crogine/ecs/components/Sprite.hpp>
#include <crogine/gui/GuiClient.hpp>
#include <crogine/graphics/ModelDefinition.hpp>

class PseutheGameState final : public cro::State, public cro::GuiClient
{
public:
    PseutheGameState(cro::StateStack&, cro::State::Context);

    cro::StateID getStateID() const override { return States::ScratchPad::PseutheGame; }

    bool handleEvent(const cro::Event&) override;
    void handleMessage(const cro::Message&) override;
    bool simulate(float) override;
    void render() override;

private:

    cro::Scene m_gameScene;
    cro::Scene m_uiScene;
    cro::ResourceCollection m_resources;

    cro::Entity m_player;
    std::int16_t m_axisX; //game controller axes
    std::int16_t m_axisY;


    struct SpriteID final
    {
        enum
        {
            Body, Amoeba, Jelly,

            Count
        };
    };
    std::array<cro::Sprite, SpriteID::Count> m_sprites = {};

    void addSystems();
    void loadAssets();
    void createScene();
    void createUI();

    void createPlayer();
    void addBodyPart();

    void playerInput(const cro::Event&);
};
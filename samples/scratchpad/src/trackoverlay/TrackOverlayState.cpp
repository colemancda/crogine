//Auto-generated source file for Scratchpad Stub 01/07/2024, 11:14:05

#include "TrackoverlayState.hpp"

#include <crogine/gui/Gui.hpp>
#include <crogine/core/ConfigFile.hpp>

#include <crogine/ecs/components/Camera.hpp>
#include <crogine/ecs/components/Transform.hpp>
#include <crogine/ecs/components/Callback.hpp>
#include <crogine/ecs/components/Drawable2D.hpp>
#include <crogine/ecs/components/Text.hpp>
#include <crogine/ecs/components/Sprite.hpp>

#include <crogine/ecs/systems/CameraSystem.hpp>
#include <crogine/ecs/systems/CallbackSystem.hpp>
#include <crogine/ecs/systems/ModelRenderer.hpp>
#include <crogine/ecs/systems/TextSystem.hpp>
#include <crogine/ecs/systems/SpriteSystem2D.hpp>
#include <crogine/ecs/systems/RenderSystem2D.hpp>

#include <crogine/graphics/Font.hpp>

#include <crogine/util/Constants.hpp>
#include <crogine/util/Easings.hpp>

namespace
{
    constexpr glm::vec2 ViewSize(1920.f, 220.f);
    constexpr glm::vec2 BannerSize(ViewSize.x, 180.f);
    constexpr glm::vec2 ThumbSize(ViewSize.y, ViewSize.y);

    constexpr glm::uvec2 TexSize(ViewSize.y, ViewSize.y);

    struct FontID final
    {
        enum
        {
            Default,

            Count
        };
    };

    constexpr float BannerDepth = -0.5f;
    constexpr float TextDepth = 0.f;
    constexpr float ImageDepth = 0.5f;

    constexpr std::uint32_t LargeTextSize = 80;
    constexpr std::uint32_t SmallTextSize = 60;

    constexpr glm::vec3 TextPosition(ThumbSize.x + 80.f, ThumbSize.y - 40.f, TextDepth);
    constexpr float TransitionSpeed = 1.f; //seconds

    const cro::Colour BannerColour = cro::Colour(0.f, 0.f, 0.f, 0.3f);

    const std::string ThumbFrag = 
R"(
OUTPUT

uniform sampler2DArray u_texture;
uniform float u_index = 0.0;

VARYING_IN vec2 v_texCoord;
VARYING_IN vec4 v_colour;


void main()
{
    FRAG_OUT = texture(u_texture, vec3(v_texCoord, u_index)) * v_colour;
})";
}

TrackOverlayState::TrackOverlayState(cro::StateStack& stack, cro::State::Context context)
    : cro::State    (stack, context),
    m_gameScene     (context.appInstance.getMessageBus()),
    m_uiScene       (context.appInstance.getMessageBus()),
    m_currentIndex  (0)
{
    context.mainWindow.loadResources([this]() {
        addSystems();
        loadAssets();
        createScene();
        createUI();
    });

    cro::App::getInstance().setClearColour(cro::Colour(0.f, 1.f, 0.f, 0.f));
}

//public
bool TrackOverlayState::handleEvent(const cro::Event& evt)
{
    if (cro::ui::wantsMouse() || cro::ui::wantsKeyboard())
    {
        return true;
    }

    if (evt.type == SDL_KEYDOWN)
    {
        switch (evt.key.keysym.sym)
        {
        default: break;
        case SDLK_BACKSPACE:
        case SDLK_ESCAPE:
            requestStackClear();
            requestStackPush(0);
            break;
        case SDLK_SPACE:
            nextTrack();
            break;
        }
    }
    else if(evt.type == SDL_CONTROLLERBUTTONDOWN)
    {
        switch (evt.cbutton.button)
        {
        default: break;
        case cro::GameController::ButtonRightShoulder:
            nextTrack();
            break;
        }
    }

    m_gameScene.forwardEvent(evt);
    m_uiScene.forwardEvent(evt);
    return true;
}

void TrackOverlayState::handleMessage(const cro::Message& msg)
{
    m_gameScene.forwardMessage(msg);
    m_uiScene.forwardMessage(msg);
}

bool TrackOverlayState::simulate(float dt)
{
    m_gameScene.simulate(dt);
    m_uiScene.simulate(dt);
    return true;
}

void TrackOverlayState::render()
{
    m_gameScene.render();
    m_uiScene.render();
}

//private
void TrackOverlayState::addSystems()
{
    auto& mb = getContext().appInstance.getMessageBus();
    m_gameScene.addSystem<cro::CallbackSystem>(mb);
    m_gameScene.addSystem<cro::CameraSystem>(mb);
    m_gameScene.addSystem<cro::ModelRenderer>(mb);

    m_uiScene.addSystem<cro::CallbackSystem>(mb);
    m_uiScene.addSystem<cro::TextSystem>(mb);
    m_uiScene.addSystem<cro::SpriteSystem2D>(mb);
    m_uiScene.addSystem<cro::CameraSystem>(mb);
    m_uiScene.addSystem<cro::RenderSystem2D>(mb);
}

void TrackOverlayState::loadAssets()
{
    m_resources.fonts.load(FontID::Default, "assets/fonts/VeraMono.ttf");
   
    cro::Image fallback;
    fallback.create(TexSize.x, TexSize.y, cro::Colour::Black);
    
    const float halfSize = static_cast<float>(TexSize.x / 2);
    const glm::vec2 centre(halfSize);
    for (auto i = 0u; i < (TexSize.x * TexSize.y); ++i)
    {
        auto x = i % TexSize.x;
        auto y = i / TexSize.y;

        glm::vec2 position(x, y);
        const float dist = glm::length(position - centre);
        const float alpha = (1.f - glm::smoothstep(halfSize - 3.5f, halfSize - 1.5f, dist)) * 255.f;
        const auto colour = std::uint8_t((1.f - glm::smoothstep(halfSize - 73.f, halfSize - 71.f, dist)) * 255.f);

        cro::Colour c(colour, colour, colour, std::uint8_t(alpha));
        fallback.setPixel(x, y, c);
    }

    m_textures.create(TexSize.x, TexSize.y);
    m_textures.insertLayer(fallback, 0);

    if (m_thumbShader.loadFromString(cro::RenderSystem2D::getDefaultVertexShader(), ThumbFrag, "#define TEXTURED\n"))
    {
        m_shaderHandle.id = m_thumbShader.getGLHandle();
        m_shaderHandle.indexUniform = m_thumbShader.getUniformID("u_index");
    }

    std::uint32_t index = 0;
    cro::Image thumbImage;

    cro::ConfigFile cfg;
    if (cfg.loadFromFile("assets/tracklist.cfg"))
    {
        const auto& obs = cfg.getObjects();
        for (const auto& ob : obs)
        {
            cro::String title;
            cro::String artist;
            std::string image;

            const auto& props = ob.getProperties();
            for (const auto& prop : props)
            {
                if (prop.getName() == "title")
                {
                    title = prop.getValue<cro::String>();
                }
                else if (prop.getName() == "artist")
                {
                    artist = prop.getValue<cro::String>();
                }
                else if (prop.getName() == "thumb")
                {
                    image = prop.getValue<std::string>();
                }
            }

            if (!title.empty() && !artist.empty())
            {
                m_textStrings.emplace_back(std::make_pair(title, artist));

                //load image and insert at index
                if (!image.empty())
                {
                    if (thumbImage.loadFromFile("assets/" + image))
                    {
                        if (thumbImage.getSize().x != TexSize.x
                            || thumbImage.getSize().y != TexSize.y)
                        {
                            thumbImage.resize(TexSize);
                        }
                        
                        if (!m_textures.insertLayer(thumbImage, index))
                        {
                            m_textures.insertLayer(fallback, index);
                        }
                    }
                    else
                    {
                        m_textures.insertLayer(fallback, index);
                    }
                }

                index++;

                if (index == MaxTracks)
                {
                    break;
                }
            }
        }
    }
}

void TrackOverlayState::createScene()
{
    auto resize = [](cro::Camera& cam)
    {
        glm::vec2 size(cro::App::getWindow().getSize());
        cam.viewport = { 0.f, 0.f, 1.f, 1.f };
        cam.setPerspective(70.f * cro::Util::Const::degToRad, size.x / size.y, 0.1f, 10.f);
    };

    auto& cam = m_gameScene.getActiveCamera().getComponent<cro::Camera>();
    cam.resizeCallback = resize;
    resize(cam);

    m_gameScene.getActiveCamera().getComponent<cro::Transform>().setPosition({ 0.f, 0.8f, 2.f });
}

void TrackOverlayState::createUI()
{
    //banner
    constexpr float Padding = (ViewSize.y - BannerSize.y) / 2.f;
    auto entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 0.f, 0.f, BannerDepth });
    entity.addComponent<cro::Drawable2D>().setVertexData(
        {
            cro::Vertex2D(glm::vec2(0.f, BannerSize.y + Padding), BannerColour),
            cro::Vertex2D(glm::vec2(0.f, Padding), BannerColour),
            cro::Vertex2D(glm::vec2(BannerSize.x, BannerSize.y + Padding), BannerColour),
            cro::Vertex2D(glm::vec2(BannerSize.x, Padding), BannerColour)
        }    
    );

    //thumbnail
    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 40.f + (ThumbSize.x / 2.f), 0.f, ImageDepth});
    entity.getComponent<cro::Transform>().setOrigin({ ThumbSize.x / 2.f, 0.f, 0.f });
    entity.addComponent<cro::Drawable2D>().setVertexData(
        {
            cro::Vertex2D(glm::vec2(0.f, ThumbSize.y), glm::vec2(0.f, 1.f)),
            cro::Vertex2D(glm::vec2(0.f), glm::vec2(0.f)),
            cro::Vertex2D(ThumbSize, glm::vec2(1.f)),
            cro::Vertex2D(glm::vec2(ThumbSize.x, 0.f), glm::vec2(1.f, 0.f))
        }
    );
    entity.getComponent<cro::Drawable2D>().setShader(&m_thumbShader);
    entity.getComponent<cro::Drawable2D>().setTexture(cro::TextureID(m_textures), TexSize);
    m_displayEnts.thumb = entity;

    cro::String title("No Tracks Loaded");
    cro::String artist("Buns.");

    if (!m_textStrings.empty())
    {
        title = m_textStrings[0].first;
        artist = m_textStrings[0].second;
    }

    //title text
    const auto& font = m_resources.fonts.get(FontID::Default);
    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition(TextPosition);
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Text>(font).setString(title);
    entity.getComponent<cro::Text>().setFillColour(cro::Colour::White);
    entity.getComponent<cro::Text>().setCharacterSize(LargeTextSize);

    struct CallbackData final
    {
        float width = 0.f;
        std::int32_t state = 0; //0 out 1 in
        CallbackData(float w, std::int32_t s)
            : width(w), state(s) {}
    };

    entity.addComponent<cro::Callback>().setUserData<CallbackData>(cro::Text::getLocalBounds(entity).width, 0);
    entity.getComponent<cro::Callback>().function =
        [&](cro::Entity e, float dt)
        {
            auto& [width, state] = e.getComponent<cro::Callback>().getUserData<CallbackData>();
            const float Speed = (dt * (ViewSize.x * TransitionSpeed) * 2.f);

            e.getComponent<cro::Transform>().move({ -Speed, 0.f });
            const auto xPos = e.getComponent<cro::Transform>().getPosition().x;
            if (state == 0)
            {
                const float imageScale = 1.f - ((xPos - TextPosition.x) / -width);
                m_displayEnts.thumb.getComponent<cro::Transform>().setScale({ cro::Util::Easing::easeOutSine(imageScale), 1.f });

                if (xPos < -width)
                {
                    e.getComponent<cro::Transform>().move({ width + ViewSize.x, 0.f });

                    const auto& [title, artist] = m_textStrings[m_currentIndex];
                    m_displayEnts.artistText.getComponent<cro::Text>().setString(artist);
                    m_displayEnts.titleText.getComponent<cro::Text>().setString(title);
                    //TODO check size and scale to fit if needed

                    //update the image index
                    glUseProgram(m_shaderHandle.id);
                    glUniform1f(m_shaderHandle.indexUniform, static_cast<float>(m_currentIndex));

                    width = cro::Text::getLocalBounds(e).width;
                    state = 1;
                }
            }
            else
            {
                const float imageScale = 1.f - ((xPos - TextPosition.x) / (ViewSize.x - TextPosition.x));
                m_displayEnts.thumb.getComponent<cro::Transform>().setScale({ cro::Util::Easing::easeInSine(imageScale), 1.f });

                if (xPos < TextPosition.x)
                {
                    m_displayEnts.thumb.getComponent<cro::Transform>().setScale(glm::vec2(1.f));

                    e.getComponent<cro::Transform>().setPosition(TextPosition);
                    state = 0;
                    e.getComponent<cro::Callback>().active = 0;
                }
            }
        };

    auto artistText = entity;
    m_displayEnts.artistText = entity;

    //track text
    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 0.f, -80.f, 0.f });
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Text>(font).setString(artist);
    entity.getComponent<cro::Text>().setFillColour(cro::Colour::White);
    entity.getComponent<cro::Text>().setCharacterSize(SmallTextSize);
    artistText.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
    m_displayEnts.titleText = entity;


    auto resize = [](cro::Camera& cam)
    {
        glm::vec2 size(cro::App::getWindow().getSize());

        cam.viewport = {0.f, 0.f, 1.f, (size.x / ViewSize.x) * (ViewSize.y / size.y)};
        //cam.setOrthographic(0.f, size.x, 0.f, size.y, -0.1f, 10.f);
        cam.setOrthographic(0.f, ViewSize.x, 0.f, ViewSize.y, -1.f, 10.f);
    };

    auto& cam = m_uiScene.getActiveCamera().getComponent<cro::Camera>();
    cam.resizeCallback = resize;
    resize(cam);
}

void TrackOverlayState::nextTrack()
{
    if (!m_textStrings.empty() &&
        !m_displayEnts.artistText.getComponent<cro::Callback>().active)
    {
        m_displayEnts.artistText.getComponent<cro::Callback>().active = true;


        m_currentIndex = (m_currentIndex + 1) % m_textStrings.size();
    }
}
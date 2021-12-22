/*-----------------------------------------------------------------------

Matt Marchant 2017 - 2021
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

#pragma once

#include <crogine/Config.hpp>
#include <crogine/graphics/Transformable2D.hpp>
#include <crogine/graphics/SimpleDrawable.hpp>
#include <crogine/graphics/Font.hpp>

namespace cro
{
    class String;
    class RenderTarget;

    /*!
    \brief SimpleText class.
    This class can be used to render text string to the active target
    without the need for a Scene or ECS. However, unlike the ECS equivalent,
    the SimpleText class is less flexible. SimpleText is rendered at the
    current target resolution with one pixel per-unit.
    */
    class CRO_EXPORT_API SimpleText final : public cro::Transformable2D, public cro::SimpleDrawable
    {
    public:
        /*!
        \brief Default constructor.
        A font must be set before this text will render.
        */
        SimpleText();

        /*!
        \brief Construxt a SimpleText with the given Font.
        */
        SimpleText(const Font& font);

        /*!
        \brief Set the font to be used with this Text
        \param font Font to use when rendering
        */
        void setFont(const Font& font);

        /*!
        \brief Set the character size of the Text
        \param size The size of the character in points
        */
        void setCharacterSize(std::uint32_t size);

        /*!
        \brief Sets the vertical spacing between lines of text
        \param spacing By default the spacingis 0. Negative values
        can cause lines to overlap or event appear in reverse order.
        */
        void setVerticalSpacing(float spacing);

        /*!
        \brief Set the string to render with this Text
        \param str A cro::String containing the string to use
        */
        void setString(const String& str);

        /*!
        \brief Set the Text fill colour
        \param colour The colour with which the inner part of the
        text will be rendered. Defaults to cro::Colour::White
        */
        void setFillColour(Colour colour);

        /*!
        \brief Set the colour used for the text outline
        If the outline id not visible make sure that this
        colour is not transparent, and that the outline
        thickness is greater than 0. Defaults to cro::Colour::Black
        \param colour Colour with which to render the outline
        \see setOutlineThickness
        */
        void setOutlineColour(Colour colour);

        /*!
        \brief Set the thickness of the text outline
        Negative values will not have the expected result.
        The outline colour must not be transparent and a value
        greater than zero must be set in orderfor outlines to appear
        \param outlineThickness The thickness, in pixels, of
        the outline to render
        */
        void setOutlineThickness(float outlineThickness);

        /*!
        \brief Return a pointer to the active font
        May be nullptr
        */
        const Font* getFont() const;

        /*!
        \brief Returns the current character size
        */
        std::uint32_t getCharacterSize() const;

        /*!
        \brief Returns the current line spacing
        */
        float getVerticalSpacing() const;

        /*!
        \brief Returns a reference to the current string
        */
        const String& getString() const;

        /*!
        \brief Returns the current fill colour
        */
        Colour getFillColour() const;

        /*!
        \brief Gets the colour used to draw the text outline
        */
        Colour getOutlineColour() const;

        /*!
        \brief Gets the thickness of the text outline
        */
        float getOutlineThickness() const;

        /*!
        \brief Get the enclosing AABB
        */
        FloatRect getLocalBounds();

        /*!
        \brief Get the enclosing AABB taking into account any transform
        */
        FloatRect getGlobalBounds();

        /*!
        \brief Draw the text to the active buffer
        */
        void draw() override;

    private:
        TextContext m_context;
        FloatRect m_localBounds;
        glm::uvec2 m_lastTextureSize;
        const Texture* m_fontTexture;

        struct DirtyFlags final
        {
            enum
            {
                ColourInner = 0x1,
                ColourOuter = 0x2,
                Font        = 0x4,
                String      = 0x8,

                All = ColourInner | ColourOuter | Font | String
            };
        };
        std::uint16_t m_dirtyFlags;

        void updateVertices();
    };
}

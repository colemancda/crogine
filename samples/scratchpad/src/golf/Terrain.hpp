/*-----------------------------------------------------------------------

Matt Marchant 2021
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

#include <array>
#include <string>

struct RenderFlags final
{
    enum
    {
        MiniMap = 0x1
    };
};

struct TerrainID final
{
    //these values are multiplied by 10 in the map's red channel
    //just cos it makes its a tiiiny bit easier to see in the image
    enum
    {
        Rough, Fairway,
        Green, Bunker,
        Water, Scrub,

        Hole,

        Count
    };
};

static const std::array<std::string, TerrainID::Count> TerrainStrings =
{
    "Rough", "Fairway", "Green", "Bunker", "Water", "Scrub", "Hole"
};

//how much the stroke is affected by the current terrain
static constexpr std::array<float, TerrainID::Count> Dampening =
{
    0.9f, 1.f, 1.f, 0.85f, 1.f, 1.f, 0.f
};
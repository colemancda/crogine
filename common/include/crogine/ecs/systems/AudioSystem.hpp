/*-----------------------------------------------------------------------

Matt Marchant 2017
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

#ifndef CRO_AUDIO_SYSTEM_HPP_
#define CRO_AUDIO_SYSTEM_HPP_

#include <crogine/ecs/System.hpp>

namespace cro
{
    /*!
    \brief Processes the Scene's AudioSource components
    based on the Scene's active AudioListener.
    Because the audio subsystem at a hardware level only has a 
    single listener point when using multiple scenes (for
    example a second 2D scene to render a UI) onnly one of
    these scenes should have an AudioSystem active within it.
    Multiple systems will work, however spatialisation will
    not be calculated correctly.
    */
    class CRO_EXPORT_API AudioSystem final : public System
    {
    public:
        AudioSystem(MessageBus&);

        void process(cro::Time) override;

    private:

        void onEntityAdded(Entity) override;
    };
}

#endif //CRO_AUDIO_SYSTEM_HPP_
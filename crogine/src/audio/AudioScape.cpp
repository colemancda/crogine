/*-----------------------------------------------------------------------

Matt Marchant 2021
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

#include <crogine/audio/AudioResource.hpp>
#include <crogine/audio/AudioScape.hpp>

using namespace cro;

AudioScape::AudioScape()
    : m_audioResource(nullptr)
{

}

//public
bool AudioScape::loadFromFile(const std::string& path, AudioResource& audioResource)
{
    cro::ConfigFile cfg;
    if (cfg.loadFromFile(path))
    {
        m_audioResource = &audioResource;
        m_configs.clear();

        const auto& objs = cfg.getObjects();
        for (const auto& obj : objs)
        {
            if (obj.getId().empty())
            {
                LogW << "Parsed AudioScape with missing emitter ID in " << path << std::endl;
                continue;
            }

            bool streaming = true; //fall back to this if missing so we don't accidentally try to load huge files
            std::string mediaPath;
            AudioConfig ac;

            const auto& props = obj.getProperties();
            for (const auto& prop : props)
            {
                auto propName = prop.getName();
                if (propName == "path")
                {
                    mediaPath = prop.getValue<std::string>();
                }
                else if (propName == "streaming")
                {
                    streaming = prop.getValue<bool>();
                }
                else if (propName == "looped")
                {
                    ac.looped = prop.getValue<bool>();
                }
                else if (propName == "pitch")
                {
                    ac.pitch = prop.getValue<float>();
                }
                else if (propName == "volume")
                {
                    ac.volume = prop.getValue<float>();
                }
                else if (propName == "mixer_channel")
                {
                    ac.channel = static_cast<std::uint8_t>(std::min(15, std::max(0, prop.getValue<std::int32_t>())));
                }
                else if (propName == "attenuation")
                {
                    ac.rolloff = prop.getValue<float>();
                }
                else if (propName == "relative_listener")
                {
                    //oh...
                }
            }

            if (!mediaPath.empty() && cro::FileSystem::fileExists(mediaPath))
            {
                ac.audioBuffer = audioResource.load(mediaPath, streaming);
                if (ac.audioBuffer != -1)
                {
                    m_configs.insert(std::make_pair(obj.getId(), ac));
                }
                else
                {
                    LogW << "Failed opening file " << mediaPath << std::endl;
                }
            }
            else
            {
                LogW << obj.getId() << " found no valid media file" << std::endl;
            }
        }

        if (m_configs.empty())
        {
            LogW << "No valid AudioScape definitions were loaded from " << path << std::endl;
            return false;
        }
        return true;
    }

    return false;
}

AudioEmitter AudioScape::getEmitter(const std::string& name) const
{
    AudioEmitter emitter;
    if (m_audioResource
        && m_configs.count(name))
    {
        const auto& cfg = m_configs.at(name);
        emitter.setSource(m_audioResource->get(cfg.audioBuffer));
        emitter.setMixerChannel(cfg.channel);
        emitter.setLooped(cfg.looped);
        emitter.setPitch(cfg.pitch);
        emitter.setRolloff(cfg.rolloff);
        emitter.setVolume(cfg.volume);               
    }
    else
    {
        LogW << name << " not found in AudioScape" << std::endl;
    }
    return emitter;
}
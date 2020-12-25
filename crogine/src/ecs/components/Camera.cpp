/*-----------------------------------------------------------------------

Matt Marchant 2017 - 2020
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

#include "../../detail/GLCheck.hpp"

#include <crogine/ecs/components/Camera.hpp>

using namespace cro;

Camera::Camera() 
    : viewport      (0.f, 0.f, 1.f, 1.f),
    m_verticalFOV   (0.6f),
    m_aspectRatio   (1.f),
    m_nearPlane     (0.1f),
    m_farPlane      (150.f)
{
    glm::vec2 windowSize(App::getWindow().getSize());
    m_aspectRatio = windowSize.x / windowSize.y;
    m_projectionMatrix = glm::perspective(m_verticalFOV, m_aspectRatio, m_nearPlane, m_farPlane);

    m_passes[Pass::Final].m_cullFace = GL_BACK;
    m_passes[Pass::Final].m_planeMultiplier = -1.f;

    m_passes[Pass::Reflection].m_cullFace = GL_FRONT;
}

void Camera::setActivePass(std::uint32_t pass)
{
    CRO_ASSERT(pass <= Pass::Refraction, "Must be a valid Pass enum value");

    switch (pass)
    {
    default:
    case Pass::Final:
        m_passIndex = Pass::Final;
#ifdef PLATFORM_DESKTOP
        glCheck(glDisable(GL_CLIP_DISTANCE0));
#endif
        break;
    case Pass::Reflection:
        m_passIndex = Pass::Reflection;
#ifdef PLATFORM_DESKTOP
        glCheck(glEnable(GL_CLIP_DISTANCE0));
#endif
        break;
    case Pass::Refraction:
        //we'll be sharing the same matrices
        //and draw lists
        m_passIndex = Pass::Final;
#ifdef PLATFORM_DESKTOP
        glCheck(glEnable(GL_CLIP_DISTANCE0));
#endif
        break;
    }
}

const Camera::Pass& Camera::getPass(std::uint32_t pass) const
{
    CRO_ASSERT(pass <= Camera::Pass::Refraction, "Must be a value for Camera::Pass enum");
    switch (pass)
    {
    default:
    case Camera::Pass::Final:
    case Camera::Pass::Refraction:
        return m_passes[Pass::Final];
    case Camera::Pass::Reflection:
        return m_passes[Pass::Reflection];
    }
}

Camera::DrawList& Camera::getDrawList(std::uint32_t pass)
{
    CRO_ASSERT(pass <= Camera::Pass::Refraction, "Must be a value for Camera::Pass enum");
    switch (pass)
    {
    default:
    case Camera::Pass::Final:
    case Camera::Pass::Refraction:
        return m_passes[Pass::Final].drawList;
    case Camera::Pass::Reflection:
        return m_passes[Pass::Reflection].drawList;
    }
}

void Camera::setPerspective(float fov, float aspect, float nearPlane, float farPlane)
{
    m_projectionMatrix = glm::perspective(fov, aspect, nearPlane, farPlane);
    m_verticalFOV = fov;
    m_aspectRatio = aspect;
    m_nearPlane = nearPlane;
    m_farPlane = farPlane;
}

void Camera::setOrthographic(float left, float right, float bottom, float top, float nearPlane, float farPlane)
{
    m_projectionMatrix = glm::ortho(left, right, bottom, top, nearPlane, farPlane);
    m_verticalFOV = -1.f;
    m_aspectRatio = (right - left) / (bottom - top);
    m_nearPlane = nearPlane;
    m_farPlane = farPlane;
}
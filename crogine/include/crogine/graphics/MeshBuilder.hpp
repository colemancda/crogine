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

#pragma once

#include <crogine/Config.hpp>
#include <crogine/detail/SDLResource.hpp>
#include <crogine/graphics/MeshData.hpp>

#include <vector>
#include <array>

namespace cro
{
    /*!
    \brief Base class for mesh creation.
    This class should be used as a base for mesh file format parsers
    or programatically creating meshes. See the Quad, Cube or Sphere
    builders for examples.

    Mesh builder types should be used to pre-load mesh data with the
    MeshResource class. Mesh data created with a MeshBuilder instance
    will apparently work with the Model component, but will not be 
    properly memory managed unless it is assigned a valid ID via the
    MeshResource manager, and lead to memory leaks.

    All meshes are drawn with at least one index array so this should
    be considered if creating a builder which dynamically builds meshes.
    */
    class CRO_EXPORT_API MeshBuilder : public Detail::SDLResource
    {
    public:
        MeshBuilder() = default;
        virtual ~MeshBuilder() = default;
        MeshBuilder(const MeshBuilder&) = default;
        MeshBuilder(MeshBuilder&&) = default;
        MeshBuilder& operator = (const MeshBuilder&) = default;
        MeshBuilder& operator = (MeshBuilder&&) = default;

        /*!
        \brief If creating a builder which loads a mesh from disk, for example,
        create a Unique ID based on the hash of the file path to prevent the
        same mesh being loaded more than once if using the ResourceAutomation
        */
        virtual int32 getUID() const { return 0; }

    protected:
        friend class MeshResource;
        /*!
        \brief Implement this to create the appropriate VBO / IBO
        and return a Mesh::Data struct.
        */
        virtual Mesh::Data build() const = 0;

        static std::size_t getVertexSize(const std::array<std::size_t, Mesh::Attribute::Total>& attrib);
        static void createVBO(Mesh::Data& meshData, const std::vector<float>& vertexData);
        static void createIBO(Mesh::Data& meshData, void* idxData, std::size_t idx, int32 dataSize);
    };
}
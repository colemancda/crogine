/*-----------------------------------------------------------------------

Matt Marchant 2020
http://trederia.blogspot.com

crogine model viewer/importer - Zlib license.

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

#include "gltf/tiny_gltf.h"
#include "ResourceIDs.hpp"

#include <crogine/core/State.hpp>
#include <crogine/core/ConfigFile.hpp>
#include <crogine/ecs/Scene.hpp>
#include <crogine/graphics/ModelDefinition.hpp>
#include <crogine/graphics/EnvironmentMap.hpp>
#include <crogine/gui/GuiClient.hpp>
#include <crogine/gui/detail/imgui.h>

#include "StateIDs.hpp"
#include "MaterialDefinition.hpp"

#include <map>
#include <memory>
#include <optional>

namespace tf = tinygltf;

struct CMFHeader final
{
    //note when writing to static meshes
    //only the first 8 bits of the flags are written
    //this is only expanded to 16 bit for animated
    //meshes/models which require skeleton flags
    std::uint16_t flags = 0;
    std::uint8_t arrayCount = 0;
    std::int32_t arrayOffset = 0;
    std::vector<std::int32_t> arraySizes;
    bool animated = false;
};

struct SharedStateData;
class ModelState final : public cro::State, public cro::GuiClient
{
public:
    ModelState(cro::StateStack&, cro::State::Context, SharedStateData&);

    cro::StateID getStateID() const override { return States::ModelViewer; }

    bool handleEvent(const cro::Event&) override;
    void handleMessage(const cro::Message&) override;
    bool simulate(float) override;
    void render() override;

private:

    SharedStateData& m_sharedData;
    cro::EnvironmentMap m_environmentMap;
    cro::Scene m_scene;
    cro::Scene m_previewScene;
    cro::ResourceCollection m_resources;

    float m_fov;
    float m_viewportRatio;

    struct Preferences final
    {
        cro::Colour skyBottom = cro::Colour(0.82f, 0.98f, 0.99f);
        cro::Colour skyTop = cro::Colour(0.21f, 0.5f, 0.96f);

        std::string lastImportDirectory;
        std::string lastExportDirectory;
        std::string lastModelDirectory;
    }m_preferences;
    bool m_showPreferences;
    bool m_showGroundPlane;
    bool m_showSkybox;
    bool m_showMaterialWindow;
    bool m_showBakingWindow;

    cro::ConfigFile m_currentModelConfig;
    std::string m_currentFilePath;

    std::array<cro::Entity, EntityID::Count> m_entities = {};

    void addSystems();
    void loadAssets();
    void createScene();


    //------------ModelStateModels.cpp----------//
    struct ModelProperties final
    {
        std::string name = "Untitled";

        enum Type
        {
            Static, Skinned, Cube,
            Quad, Sphere, Billboard
        }type = Static;

        bool lockRotation = false;
        bool lockScale = false;
        bool castShadows = false;

        float radius = 0.5f;
        glm::vec3 size = glm::vec3(1.f);
        glm::vec4 uv = glm::vec4(0.f, 0.f, 1.f, 1.f);

        std::vector<float> vertexData;
        std::vector<std::vector<std::uint32_t>> indexData;

    }m_modelProperties;

    void openModel();
    void openModelAtPath(const std::string&);
    void saveModel(const std::string&);
    void closeModel();

    CMFHeader m_importedHeader;
    std::vector<float> m_importedVBO;
    std::vector<std::vector<std::uint32_t>> m_importedIndexArrays;
    std::unordered_map<std::uint16_t, std::size_t> m_importedMeshes; //< maps created VBOs to vert flags - this recycles matching VBOs if they exist and only creates new when necessary
    struct ImportTransform final
    {
        glm::vec3 rotation = glm::vec3(0.f);
        float scale = 1.f;
    }m_importedTransform;
    std::size_t m_skeletonMeshID;

    void importModel();
    void updateImportNode(CMFHeader, std::vector<float>& verts, std::vector<std::vector<std::uint32_t>>& indices);
    void buildSkeleton();
    void exportStaticModel(bool = false, bool = true);
    void applyImportTransform();
    //-------------------------------------------//


    //--------gltf/gltf.cpp--------//
    bool m_browseGLTF;
    std::unique_ptr<tf::TinyGLTF> m_GLTFLoader;
    tf::Model m_GLTFScene;
    void showGLTFBrowser();
    void parseGLTFNode(std::int32_t, bool importAnims);
    void parseGLTFAnimations(std::int32_t);
    void parseGLTFSkin(std::int32_t, cro::Skeleton&);
    bool importGLTF(std::int32_t, bool);
    //----------------------------//

    void loadPrefs();
    void savePrefs();

    bool m_showAABB;
    bool m_showSphere;
    void updateNormalVis();
    void updateGridMesh(cro::Mesh::Data&, std::optional<cro::Sphere>, std::optional<cro::Box>);

    void updateMouseInput(const cro::Event&);


    //--------ModelStateMaterials.cpp------//
    std::array<std::int32_t, MaterialID::Count> m_materialIDs = {};

    struct MaterialTexture final
    {
        std::unique_ptr<cro::Texture> texture;
        std::string name;
        std::string relPath; //inc trailing '/'
    };
    std::map<std::uint32_t, MaterialTexture> m_materialTextures;
    std::uint32_t m_selectedTexture;
    std::uint32_t addTextureToBrowser(const std::string&);
    void addMaterialToBrowser(MaterialDefinition&&);

    cro::Texture m_blackTexture;
    cro::Texture m_magentaTexture;
    std::vector<MaterialDefinition> m_materialDefs;
    std::size_t m_selectedMaterial;
    cro::Entity m_previewEntity;
    void applyPreviewSettings(MaterialDefinition&);
    void refreshMaterialThumbnail(MaterialDefinition&);

    std::vector<std::int32_t> m_activeMaterials; //indices into the materials array of materials used on the currently open model. -1 means default material is applied
    void exportMaterial() const;
    void importMaterial(const std::string&);
    void readMaterialDefinition(MaterialDefinition&, const cro::ConfigObject&);
    //-------------------------------------//



    //---------ModelStateUI.cpp--------//
    ImVec4 m_messageColour;
    void buildUI();
    void showSaveMessage();
    void drawInspector();
    void drawBrowser();
    void drawInfo();
    void drawGizmo();
    void updateLayout(std::int32_t, std::int32_t);
    //---------------------------------//

    std::vector<std::unique_ptr<cro::Texture>> m_lightmapTextures;
    std::vector<std::vector<float>> m_lightmapBuffers;
    void bakeLightmap();
};
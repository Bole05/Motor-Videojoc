#pragma once
#include <assimp/cimport.h>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <glad/glad.h>
#include <glm/glm.hpp>
#include <memory>
#include <string>
#include <vector>
#include "Texture.h"

struct Mesh
{
	unsigned int id_index = 0;
	unsigned int num_index = 0;
	unsigned int* index = nullptr;

	unsigned int id_vertex = 0;
	unsigned int num_vertex = 0;
	float* vertex = nullptr;

	unsigned int id_normal = 0;   // VBO de normales (para iluminación)
	float* normal = nullptr;

	unsigned int id_texcoord = 0; // VBO de coordenadas UV (para texturas)
	float* texcoord = nullptr;

	unsigned int materialIndex = 0; // Índice del material de Assimp que usa esta submalla
};

// Nodo de la jerarquía del modelo (copia de los aiNode del FBX).
// Ej. BakerHouse.fbx -> RootNode -> { Baker_house, Chimney }
struct ModelNode
{
	std::string name;
	glm::mat4 transform = glm::mat4(1.0f);   // Transform local respecto al padre (del FBX)
	std::vector<unsigned int> meshes;        // Índices de las submallas que tiene este nodo
	std::vector<ModelNode> children;         // Nodos hijos
};

// Información de la textura difusa de un material del modelo
struct MaterialTexture
{
	std::string requestedPath;          // Ruta tal y como viene dentro del FBX
	std::string resolvedPath;           // Ruta real donde se encontró (vacía si no se encontró)
	bool embedded = false;              // true si venía embebida dentro del FBX
	std::shared_ptr<Texture> texture;   // nullptr si no tiene textura o no se pudo cargar
};

class Geometria {
public:
	Geometria();
	~Geometria();

	// Deshabilitar copia directa para evitar doble liberación de VBOs
	Geometria(const Geometria&) = delete;
	Geometria& operator=(const Geometria&) = delete;

	static void DebugInit();

	bool Load(const std::string& file_path);

	// Dibuja las submallas indicadas (las de un nodo de la jerarquía).
	// La matriz del nodo ya debe estar cargada en OpenGL antes de llamar.
	//  useTextures     : si es false se dibuja sólo con el color del material
	//  overrideTexture : si no es nullptr, se usa esta textura en todas las submallas
	//                    en lugar de las del FBX
	void Draw(const std::vector<unsigned int>& meshIndices, bool useTextures = true,
	          const Texture* overrideTexture = nullptr) const;

	// Matriz que centra el modelo completo y lo escala a tamaño unitario
	glm::mat4 GetNormalizeMatrix() const;

	void CleanUp();

	bool HasMesh() const { return hasLoaded; }
	unsigned int GetTotalVertices() const { return totalVertices; }
	unsigned int GetTotalTriangles() const { return totalTriangles; }
	const std::string& GetFilePath() const { return filePath; }
	float GetRadius() const { return meshRadius; }
	glm::vec3 GetCenter() const { return meshCenter; }

	bool HasUVs() const { return hasUVs; }
	bool HasAnyTexture() const;
	const std::vector<MaterialTexture>& GetMaterials() const { return materials; }

	// Jerarquía de nodos del FBX
	const ModelNode& GetRootNode() const { return rootNode; }

	// Información por submalla (para el inspector)
	unsigned int GetMeshCount() const { return (unsigned int)meshes.size(); }
	unsigned int GetMeshVertices(unsigned int i) const { return i < meshes.size() ? meshes[i].num_vertex : 0; }
	unsigned int GetMeshTriangles(unsigned int i) const { return i < meshes.size() ? meshes[i].num_index / 3 : 0; }
	unsigned int GetMeshMaterial(unsigned int i) const { return i < meshes.size() ? meshes[i].materialIndex : 0; }

private:
	void LoadMaterials(const aiScene* scene, const std::string& modelPath);
	// Copia recursivamente un aiNode (y sus hijos) a la jerarquía ModelNode
	void BuildNode(const aiNode* node, const glm::mat4& pendingLocal, const glm::mat4& parentGlobal,
	               const std::vector<int>& meshRemap, ModelNode& out, glm::vec3& minV, glm::vec3& maxV);

	std::vector<Mesh> meshes;
	std::vector<MaterialTexture> materials;
	ModelNode rootNode;
	bool hasLoaded = false;
	bool hasUVs = false;
	std::string filePath;
	unsigned int totalVertices = 0;
	unsigned int totalTriangles = 0;

	// Centro y radio del bounding box (calculados en Load)
	glm::vec3 meshCenter = glm::vec3(0.0f);
	float meshRadius = 1.0f;
};

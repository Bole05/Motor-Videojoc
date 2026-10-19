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

	// Dibuja la malla centrada y escalada a tamaño unitario.
	//  useTextures     : si es false se dibuja sólo con el color del material
	//  overrideTexture : si no es nullptr, se usa esta textura en todas las submallas
	//                    en lugar de las del FBX
	void Draw(bool useTextures = true, const Texture* overrideTexture = nullptr) const;

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

private:
	void LoadMaterials(const aiScene* scene, const std::string& modelPath);

	std::vector<Mesh> meshes;
	std::vector<MaterialTexture> materials;
	bool hasLoaded = false;
	bool hasUVs = false;
	std::string filePath;
	unsigned int totalVertices = 0;
	unsigned int totalTriangles = 0;

	// Centro y radio del bounding box (calculados en Load)
	glm::vec3 meshCenter = glm::vec3(0.0f);
	float meshRadius = 1.0f;
};

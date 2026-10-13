#pragma once
#include <assimp/cimport.h>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <glad/glad.h>
#include <glm/glm.hpp>
#include <string>
#include <vector>

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
	void Draw() const;

	void CleanUp();

	bool HasMesh() const { return hasLoaded; }
	unsigned int GetTotalVertices() const { return totalVertices; }
	unsigned int GetTotalTriangles() const { return totalTriangles; }
	const std::string& GetFilePath() const { return filePath; }
	float GetRadius() const { return meshRadius; }
	glm::vec3 GetCenter() const { return meshCenter; }

private:
	std::vector<Mesh> meshes;
	bool hasLoaded = false;
	std::string filePath;
	unsigned int totalVertices = 0;
	unsigned int totalTriangles = 0;

	// Centro y radio del bounding box (calculados en Load)
	glm::vec3 meshCenter = glm::vec3(0.0f);
	float meshRadius = 1.0f;
};

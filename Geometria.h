#pragma once
#include <assimp/cimport.h>
#include <Assimp/scene.h>
#include <Assimp/postprocess.h>
#include <glad/glad.h>
#include <glm/glm.hpp>
#include <string>

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

	static void DebugInit();

	void Load(const std::string& file_path);

	// Dibuja la malla centrada y escalada a tamaño unitario.
	void Draw() const;

	void CleanUp();

	bool HasMesh() const { return hasLoaded; }

private:
	Mesh ourMesh;
	bool hasLoaded = false;

	// Centro y radio del bounding box (calculados en Load)
	glm::vec3 meshCenter = glm::vec3(0.0f);
	float meshRadius = 1.0f;
};

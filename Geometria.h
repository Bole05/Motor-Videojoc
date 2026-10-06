#pragma once
#include <assimp/cimport.h>
#include <Assimp/scene.h>
#include <Assimp/postprocess.h>
#include <string>
struct Mesh
{
	unsigned int id_index = 0;
	unsigned int num_index = 0;
	unsigned int* index = nullptr;

	unsigned int id_vertex = 0;
	unsigned int num_vertex = 0;
	float* vertex = nullptr;
};
class Geometria {
public:
	Geometria();
	~Geometria();

	static void DebugInit();

	void Load(std::string file_path);

	void CleanUp();
private:
	Mesh ourMesh;
};
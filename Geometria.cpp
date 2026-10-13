#include "Geometria.h"
#include "Log.h"
#include <cfloat>
#include <algorithm>

Geometria::Geometria() {}

Geometria::~Geometria() {
	CleanUp();
}

void Geometria::DebugInit() {
	aiLogStream stream;
	stream = aiGetPredefinedLogStream(aiDefaultLogStream_DEBUGGER, nullptr);
	aiAttachLogStream(&stream);
}

bool Geometria::Load(const std::string& file_path) {
	// Limpiar mallas anteriores antes de cargar una nueva
	CleanUp();

	const aiScene* scene = aiImportFile(file_path.c_str(), aiProcessPreset_TargetRealtime_MaxQuality);
	if (!scene || !scene->HasMeshes()) {
		LOG("Error loading scene %s: %s", file_path.c_str(), aiGetErrorString());
		return false;
	}

	filePath = file_path;
	glm::vec3 minV(FLT_MAX);
	glm::vec3 maxV(-FLT_MAX);
	totalVertices = 0;
	totalTriangles = 0;

	meshes.reserve(scene->mNumMeshes);

	for (unsigned int m = 0; m < scene->mNumMeshes; ++m) {
		const aiMesh* ai_mesh = scene->mMeshes[m];
		if (!ai_mesh || ai_mesh->mNumVertices == 0) continue;

		Mesh mesh;
		mesh.num_vertex = ai_mesh->mNumVertices;
		totalVertices += mesh.num_vertex;

		// Vértices
		mesh.vertex = new float[mesh.num_vertex * 3];
		memcpy(mesh.vertex, ai_mesh->mVertices, sizeof(float) * mesh.num_vertex * 3);

		// Actualizar Bounding Box global
		for (unsigned int i = 0; i < mesh.num_vertex; ++i) {
			float x = mesh.vertex[i * 3 + 0];
			float y = mesh.vertex[i * 3 + 1];
			float z = mesh.vertex[i * 3 + 2];
			minV.x = std::min(minV.x, x);  minV.y = std::min(minV.y, y);  minV.z = std::min(minV.z, z);
			maxV.x = std::max(maxV.x, x);  maxV.y = std::max(maxV.y, y);  maxV.z = std::max(maxV.z, z);
		}

		// Subir vértices a GPU
		glGenBuffers(1, &mesh.id_vertex);
		glBindBuffer(GL_ARRAY_BUFFER, mesh.id_vertex);
		glBufferData(GL_ARRAY_BUFFER, sizeof(float) * mesh.num_vertex * 3, mesh.vertex, GL_STATIC_DRAW);

		// Normales
		if (ai_mesh->HasNormals()) {
			mesh.normal = new float[mesh.num_vertex * 3];
			memcpy(mesh.normal, ai_mesh->mNormals, sizeof(float) * mesh.num_vertex * 3);
			glGenBuffers(1, &mesh.id_normal);
			glBindBuffer(GL_ARRAY_BUFFER, mesh.id_normal);
			glBufferData(GL_ARRAY_BUFFER, sizeof(float) * mesh.num_vertex * 3, mesh.normal, GL_STATIC_DRAW);
		}

		// Caras / Índices
		if (ai_mesh->HasFaces()) {
			mesh.num_index = ai_mesh->mNumFaces * 3;
			totalTriangles += ai_mesh->mNumFaces;
			mesh.index = new unsigned int[mesh.num_index];
			for (unsigned int f = 0; f < ai_mesh->mNumFaces; ++f) {
				if (ai_mesh->mFaces[f].mNumIndices == 3) {
					memcpy(&mesh.index[f * 3], ai_mesh->mFaces[f].mIndices, 3 * sizeof(unsigned int));
				} else {
					mesh.index[f * 3 + 0] = 0;
					mesh.index[f * 3 + 1] = 0;
					mesh.index[f * 3 + 2] = 0;
				}
			}

			glGenBuffers(1, &mesh.id_index);
			glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mesh.id_index);
			glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(unsigned int) * mesh.num_index, mesh.index, GL_STATIC_DRAW);
		}

		meshes.push_back(mesh);
	}

	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

	if (!meshes.empty() && totalVertices > 0) {
		meshCenter = (minV + maxV) * 0.5f;
		meshRadius = glm::length(maxV - meshCenter);
		if (meshRadius < 0.0001f) meshRadius = 1.0f;
		hasLoaded = true;

		LOG("Modelo cargado: %s (%zu submallas, %u vertices, %u triangulos)",
			file_path.c_str(), meshes.size(), totalVertices, totalTriangles);
		LOG("AABB center=(%.2f, %.2f, %.2f) radius=%.2f", meshCenter.x, meshCenter.y, meshCenter.z, meshRadius);
	}

	aiReleaseImport(scene);
	return hasLoaded;
}

void Geometria::Draw() const {
	if (!hasLoaded || meshes.empty()) return;

	// Auto-centrar y normalizar a tamaño estándar
	float s = 1.0f / meshRadius;
	glScalef(s, s, s);
	glTranslatef(-meshCenter.x, -meshCenter.y, -meshCenter.z);

	glEnableClientState(GL_VERTEX_ARRAY);

	for (const auto& mesh : meshes) {
		if (mesh.id_vertex == 0 || mesh.id_index == 0) continue;

		glBindBuffer(GL_ARRAY_BUFFER, mesh.id_vertex);
		glVertexPointer(3, GL_FLOAT, 0, nullptr);

		if (mesh.id_normal != 0) {
			glEnableClientState(GL_NORMAL_ARRAY);
			glBindBuffer(GL_ARRAY_BUFFER, mesh.id_normal);
			glNormalPointer(GL_FLOAT, 0, nullptr);
		} else {
			glDisableClientState(GL_NORMAL_ARRAY);
		}

		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mesh.id_index);
		glDrawElements(GL_TRIANGLES, mesh.num_index, GL_UNSIGNED_INT, nullptr);
	}

	glDisableClientState(GL_VERTEX_ARRAY);
	glDisableClientState(GL_NORMAL_ARRAY);

	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
}

void Geometria::CleanUp() {
	for (auto& mesh : meshes) {
		if (mesh.id_vertex != 0) {
			glDeleteBuffers(1, &mesh.id_vertex);
			mesh.id_vertex = 0;
		}
		if (mesh.id_normal != 0) {
			glDeleteBuffers(1, &mesh.id_normal);
			mesh.id_normal = 0;
		}
		if (mesh.id_index != 0) {
			glDeleteBuffers(1, &mesh.id_index);
			mesh.id_index = 0;
		}

		delete[] mesh.vertex; mesh.vertex = nullptr;
		delete[] mesh.normal; mesh.normal = nullptr;
		delete[] mesh.index;  mesh.index  = nullptr;
		mesh.num_vertex = 0;
		mesh.num_index = 0;
	}
	meshes.clear();

	hasLoaded = false;
	filePath.clear();
	totalVertices = 0;
	totalTriangles = 0;
	meshCenter = glm::vec3(0.0f);
	meshRadius = 1.0f;
	aiDetachAllLogStreams();
}

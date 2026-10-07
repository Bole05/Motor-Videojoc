#include "Geometria.h"
#include "Log.h"
#include <cfloat>
#include <algorithm>

Geometria::Geometria() {}
Geometria::~Geometria() {}

void Geometria::DebugInit() {
	aiLogStream stream;
	stream = aiGetPredefinedLogStream(aiDefaultLogStream_DEBUGGER, nullptr);
	aiAttachLogStream(&stream);
}

void Geometria::Load(const std::string& file_path) {
	// Limpiar malla anterior antes de cargar una nueva
	CleanUp();

	const aiScene* scene = aiImportFile(file_path.c_str(), aiProcessPreset_TargetRealtime_MaxQuality);
	if (scene != nullptr && scene->HasMeshes()) {
		aiMesh* aiMesh = scene->mMeshes[0];

		// Copiar vértices a RAM
		ourMesh.num_vertex = aiMesh->mNumVertices;
		ourMesh.vertex = new float[ourMesh.num_vertex * 3];
		memcpy(ourMesh.vertex, aiMesh->mVertices, sizeof(float) * ourMesh.num_vertex * 3);

		// Calcular Bounding Box (AABB) para centrar y normalizar el modelo
		glm::vec3 minV(FLT_MAX), maxV(-FLT_MAX);
		for (unsigned int i = 0; i < ourMesh.num_vertex; ++i) {
			float x = ourMesh.vertex[i * 3 + 0];
			float y = ourMesh.vertex[i * 3 + 1];
			float z = ourMesh.vertex[i * 3 + 2];
			minV.x = std::min(minV.x, x);  minV.y = std::min(minV.y, y);  minV.z = std::min(minV.z, z);
			maxV.x = std::max(maxV.x, x);  maxV.y = std::max(maxV.y, y);  maxV.z = std::max(maxV.z, z);
		}
		meshCenter = (minV + maxV) * 0.5f;
		meshRadius = glm::length(maxV - meshCenter);
		if (meshRadius < 0.0001f) meshRadius = 1.0f;

		LOG("New mesh with %d vertices", ourMesh.num_vertex);
		LOG("AABB center=(%.2f, %.2f, %.2f)  radius=%.2f", meshCenter.x, meshCenter.y, meshCenter.z, meshRadius);

		// Copiar índices a RAM
		if (aiMesh->HasFaces()) {
			ourMesh.num_index = aiMesh->mNumFaces * 3;
			ourMesh.index = new unsigned int[ourMesh.num_index];
			for (unsigned int i = 0; i < aiMesh->mNumFaces; ++i) {
				if (aiMesh->mFaces[i].mNumIndices != 3)
					LOG("WARNING, geometry face with != 3 indices!");
				else
					memcpy(&ourMesh.index[i * 3], aiMesh->mFaces[i].mIndices, 3 * sizeof(unsigned int));
			}
		}

		// Subir vértices a la GPU
		glGenBuffers(1, &ourMesh.id_vertex);
		glBindBuffer(GL_ARRAY_BUFFER, ourMesh.id_vertex);
		glBufferData(GL_ARRAY_BUFFER, sizeof(float) * ourMesh.num_vertex * 3, ourMesh.vertex, GL_STATIC_DRAW);

		// Subir normales a la GPU (necesarias para iluminación 3D)
		if (aiMesh->HasNormals()) {
			ourMesh.normal = new float[ourMesh.num_vertex * 3];
			memcpy(ourMesh.normal, aiMesh->mNormals, sizeof(float) * ourMesh.num_vertex * 3);
			glGenBuffers(1, &ourMesh.id_normal);
			glBindBuffer(GL_ARRAY_BUFFER, ourMesh.id_normal);
			glBufferData(GL_ARRAY_BUFFER, sizeof(float) * ourMesh.num_vertex * 3, ourMesh.normal, GL_STATIC_DRAW);
			LOG("Normales cargadas correctamente");
		}
		else {
			LOG("WARNING: el modelo no tiene normales, la iluminacion no funcionara");
		}

		// Subir índices a la GPU
		glGenBuffers(1, &ourMesh.id_index);
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ourMesh.id_index);
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(unsigned int) * ourMesh.num_index, ourMesh.index, GL_STATIC_DRAW);

		aiReleaseImport(scene);
		hasLoaded = true;
	}
	else {
		LOG("Error loading scene %s", file_path.c_str());
	}
}

void Geometria::Draw() const {
	if (!hasLoaded) return;

	// Auto-centrar y normalizar (glScalef primero, glTranslatef después = orden inverso OpenGL)
	float s = 1.0f / meshRadius;
	glScalef(s, s, s);
	glTranslatef(-meshCenter.x, -meshCenter.y, -meshCenter.z);

	glEnableClientState(GL_VERTEX_ARRAY);
	glBindBuffer(GL_ARRAY_BUFFER, ourMesh.id_vertex);
	glVertexPointer(3, GL_FLOAT, 0, nullptr);

	// Normales → permiten que GL_LIGHTING calcule sombreado 3D
	if (ourMesh.id_normal != 0) {
		glEnableClientState(GL_NORMAL_ARRAY);
		glBindBuffer(GL_ARRAY_BUFFER, ourMesh.id_normal);
		glNormalPointer(GL_FLOAT, 0, nullptr);
	}

	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ourMesh.id_index);
	glDrawElements(GL_TRIANGLES, ourMesh.num_index, GL_UNSIGNED_INT, nullptr);

	glDisableClientState(GL_VERTEX_ARRAY);
	if (ourMesh.id_normal != 0)
		glDisableClientState(GL_NORMAL_ARRAY);

	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
}

void Geometria::CleanUp() {
	// Liberar buffers de la GPU si existen
	if (ourMesh.id_vertex != 0) {
		glDeleteBuffers(1, &ourMesh.id_vertex);
		ourMesh.id_vertex = 0;
	}
	if (ourMesh.id_normal != 0) {
		glDeleteBuffers(1, &ourMesh.id_normal);
		ourMesh.id_normal = 0;
	}
	if (ourMesh.id_index != 0) {
		glDeleteBuffers(1, &ourMesh.id_index);
		ourMesh.id_index = 0;
	}

	// Liberar memoria de la CPU
	delete[] ourMesh.vertex;  ourMesh.vertex = nullptr;
	delete[] ourMesh.normal;  ourMesh.normal = nullptr;
	delete[] ourMesh.index;   ourMesh.index  = nullptr;
	ourMesh.num_vertex = 0;
	ourMesh.num_index  = 0;

	hasLoaded = false;
	aiDetachAllLogStreams();
}


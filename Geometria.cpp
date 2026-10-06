#include "Geometria.h"
#include "Log.h"
#include "Engine.h"
Geometria:: Geometria() {

}
Geometria::~Geometria(){}

void Geometria::DebugInit() {
	aiLogStream stream;
	stream = aiGetPredefinedLogStream(aiDefaultLogStream_DEBUGGER, nullptr);
	aiAttachLogStream(&stream);
}

void Geometria::Load(std::string file_path){

	const aiScene* scene = aiImportFile(file_path.c_str(), aiProcessPreset_TargetRealtime_MaxQuality);
	if (scene != nullptr && scene -> HasMeshes()) {
		aiMesh* aiMesh = scene->mMeshes[0];

		ourMesh.num_vertex = aiMesh->mNumVertices;
		ourMesh.vertex = new float[ourMesh.num_vertex * 3];
		memcpy(ourMesh.vertex, aiMesh->mVertices, sizeof(float) * ourMesh.num_vertex * 3);
		
		//index
		if (aiMesh->HasFaces())
		{
			ourMesh.num_index = aiMesh->mNumFaces * 3;
			ourMesh.index = new unsigned int [ourMesh.num_index]; // assume each face is a triangle
			for (unsigned int i = 0; i < aiMesh->mNumFaces; ++i)
			{
				if (aiMesh->mFaces[i].mNumIndices != 3)
					LOG("WARNING, geometry face with != 3 indices!");
				else
					memcpy(&ourMesh.index[i * 3], aiMesh->mFaces[i].mIndices, 3 * sizeof(unsigned int));
			}
		}

		//enviar datos a OpenGl
		// 3. Subir vértices a la GPU  ← aquí
		glGenBuffers(1, &ourMesh.id_vertex);
		glBindBuffer(GL_ARRAY_BUFFER, ourMesh.id_vertex);
		glBufferData(GL_ARRAY_BUFFER, sizeof(float) * ourMesh.num_vertex * 3, ourMesh.vertex, GL_STATIC_DRAW);
		// 4. Subir índices a la GPU  ← aquí
		glGenBuffers(1, &ourMesh.id_index);
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ourMesh.id_index);
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(unsigned int) * ourMesh.num_index, ourMesh.index, GL_STATIC_DRAW);

		LOG("New mesh with %d vertices", ourMesh.num_vertex);
		aiReleaseImport(scene);
	

	}
	else {
		LOG("Error loading scena %s", file_path.c_str());
	}


}
void Geometria::CleanUp() {
	delete[] ourMesh.vertex;
	delete[] ourMesh.index;
	aiDetachAllLogStreams();
}

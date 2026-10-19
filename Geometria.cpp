#include "Geometria.h"
#include "Log.h"
#include <cfloat>
#include <cstring>
#include <algorithm>
#include <filesystem>
#include <map>

namespace fs = std::filesystem;

Geometria::Geometria() {}

Geometria::~Geometria() {
	CleanUp();
}

void Geometria::DebugInit() {
	aiLogStream stream;
	stream = aiGetPredefinedLogStream(aiDefaultLogStream_DEBUGGER, nullptr);
	aiAttachLogStream(&stream);
}

// Busca el fichero de textura en varias ubicaciones razonables.
// Los FBX suelen guardar rutas absolutas del ordenador del artista
// (p.ej. "C:\Users\artista\Desktop\tex\body.png"), que no existen en nuestro PC.
static std::string ResolveTexturePath(const std::string& texPath, const std::string& modelPath)
{
	std::string normalized = texPath;
	std::replace(normalized.begin(), normalized.end(), '\\', '/');

	fs::path modelDir = fs::path(modelPath).parent_path();
	fs::path original(normalized);
	fs::path fileName = original.filename();

	std::vector<fs::path> candidates = {
		original,                                // ruta tal cual (absoluta o relativa al ejecutable)
		modelDir / original,                     // relativa a la carpeta del modelo
		modelDir / fileName,                     // mismo nombre, junto al modelo
		modelDir / "textures" / fileName,
		modelDir / "Textures" / fileName,
		modelDir / "texture" / fileName,
		modelDir.parent_path() / "textures" / fileName,
		modelDir.parent_path() / "Textures" / fileName,
		fs::path("assets") / fileName,
	};

	for (const auto& c : candidates) {
		std::error_code ec;
		if (!c.empty() && fs::is_regular_file(c, ec)) {
			return c.string();
		}
	}
	return "";
}

void Geometria::LoadMaterials(const aiScene* scene, const std::string& modelPath)
{
	materials.clear();
	materials.resize(scene->mNumMaterials);

	// Caché para no subir la misma imagen varias veces a la GPU
	std::map<std::string, std::shared_ptr<Texture>> cache;

	for (unsigned int i = 0; i < scene->mNumMaterials; ++i) {
		const aiMaterial* mat = scene->mMaterials[i];
		MaterialTexture& out = materials[i];

		aiString texPath;
		bool found = false;
		// Algunos exportadores usan BASE_COLOR (PBR) en vez de DIFFUSE
		if (aiGetMaterialTexture(mat, aiTextureType_DIFFUSE, 0, &texPath) == AI_SUCCESS) found = true;
		else if (aiGetMaterialTexture(mat, aiTextureType_BASE_COLOR, 0, &texPath) == AI_SUCCESS) found = true;
		if (!found || texPath.length == 0) continue;

		out.requestedPath = texPath.C_Str();

		auto cached = cache.find(out.requestedPath);
		if (cached != cache.end()) {
			out.texture = cached->second;
			out.resolvedPath = cached->second ? cached->second->GetPath() : "";
			continue;
		}

		auto tex = std::make_shared<Texture>();
		bool ok = false;

		// 1) Textura embebida dentro del FBX ("*0", "*1"... o el nombre del fichero)
		const aiTexture* embedded = scene->GetEmbeddedTexture(texPath.C_Str());
		if (embedded) {
			out.embedded = true;
			if (embedded->mHeight == 0) {
				// Imagen comprimida (png/jpg...) de mWidth bytes
				ok = tex->LoadFromMemory(embedded->pcData, embedded->mWidth, out.requestedPath + " (embebida)");
			} else {
				// Píxeles crudos BGRA
				ok = tex->LoadFromBGRA(embedded->pcData, embedded->mWidth, embedded->mHeight, out.requestedPath + " (embebida)");
			}
		}

		// 2) Textura externa en disco
		if (!ok) {
			std::string resolved = ResolveTexturePath(out.requestedPath, modelPath);
			if (!resolved.empty()) {
				ok = tex->Load(resolved);
				if (ok) out.resolvedPath = resolved;
			}
		} else {
			out.resolvedPath = tex->GetPath();
		}

		if (ok) {
			out.texture = tex;
			LOG("Material %u: textura '%s' cargada (%dx%d)", i, out.resolvedPath.c_str(), tex->GetWidth(), tex->GetHeight());
		} else {
			LOG("Material %u: NO se encontro la textura '%s'. Copiala junto al FBX o arrastra la imagen a la ventana.",
				i, out.requestedPath.c_str());
		}
		cache[out.requestedPath] = out.texture;
	}
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
		mesh.materialIndex = ai_mesh->mMaterialIndex;
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

		// Coordenadas UV (canal 0). Assimp las guarda como vec3, nos quedamos con (u, v).
		if (ai_mesh->HasTextureCoords(0)) {
			mesh.texcoord = new float[mesh.num_vertex * 2];
			for (unsigned int i = 0; i < mesh.num_vertex; ++i) {
				mesh.texcoord[i * 2 + 0] = ai_mesh->mTextureCoords[0][i].x;
				mesh.texcoord[i * 2 + 1] = ai_mesh->mTextureCoords[0][i].y;
			}
			glGenBuffers(1, &mesh.id_texcoord);
			glBindBuffer(GL_ARRAY_BUFFER, mesh.id_texcoord);
			glBufferData(GL_ARRAY_BUFFER, sizeof(float) * mesh.num_vertex * 2, mesh.texcoord, GL_STATIC_DRAW);
			hasUVs = true;
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

	// Materiales y texturas difusas
	LoadMaterials(scene, file_path);

	if (!meshes.empty() && totalVertices > 0) {
		meshCenter = (minV + maxV) * 0.5f;
		meshRadius = glm::length(maxV - meshCenter);
		if (meshRadius < 0.0001f) meshRadius = 1.0f;
		hasLoaded = true;

		LOG("Modelo cargado: %s (%zu submallas, %u vertices, %u triangulos, UVs: %s)",
			file_path.c_str(), meshes.size(), totalVertices, totalTriangles, hasUVs ? "si" : "no");
		LOG("AABB center=(%.2f, %.2f, %.2f) radius=%.2f", meshCenter.x, meshCenter.y, meshCenter.z, meshRadius);
	}

	aiReleaseImport(scene);
	return hasLoaded;
}

bool Geometria::HasAnyTexture() const {
	for (const auto& m : materials) {
		if (m.texture && m.texture->IsLoaded()) return true;
	}
	return false;
}

void Geometria::Draw(bool useTextures, const Texture* overrideTexture) const {
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

		// Elegir textura de esta submalla
		const Texture* tex = nullptr;
		if (useTextures && mesh.id_texcoord != 0) {
			if (overrideTexture && overrideTexture->IsLoaded()) {
				tex = overrideTexture;
			} else if (mesh.materialIndex < materials.size()) {
				const auto& mt = materials[mesh.materialIndex].texture;
				if (mt && mt->IsLoaded()) tex = mt.get();
			}
		}

		if (tex) {
			glEnable(GL_TEXTURE_2D);
			tex->Bind();
			// MODULATE: textura * color iluminado (así se mantiene la iluminación)
			glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);

			glEnableClientState(GL_TEXTURE_COORD_ARRAY);
			glBindBuffer(GL_ARRAY_BUFFER, mesh.id_texcoord);
			glTexCoordPointer(2, GL_FLOAT, 0, nullptr);
		} else {
			glDisableClientState(GL_TEXTURE_COORD_ARRAY);
			glBindTexture(GL_TEXTURE_2D, 0);
			glDisable(GL_TEXTURE_2D);
		}

		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mesh.id_index);
		glDrawElements(GL_TRIANGLES, mesh.num_index, GL_UNSIGNED_INT, nullptr);
	}

	glDisableClientState(GL_TEXTURE_COORD_ARRAY);
	glDisableClientState(GL_VERTEX_ARRAY);
	glDisableClientState(GL_NORMAL_ARRAY);
	glBindTexture(GL_TEXTURE_2D, 0);
	glDisable(GL_TEXTURE_2D);

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
		if (mesh.id_texcoord != 0) {
			glDeleteBuffers(1, &mesh.id_texcoord);
			mesh.id_texcoord = 0;
		}
		if (mesh.id_index != 0) {
			glDeleteBuffers(1, &mesh.id_index);
			mesh.id_index = 0;
		}

		delete[] mesh.vertex;   mesh.vertex = nullptr;
		delete[] mesh.normal;   mesh.normal = nullptr;
		delete[] mesh.texcoord; mesh.texcoord = nullptr;
		delete[] mesh.index;    mesh.index  = nullptr;
		mesh.num_vertex = 0;
		mesh.num_index = 0;
	}
	meshes.clear();
	materials.clear(); // libera las texturas (shared_ptr) de este modelo

	hasLoaded = false;
	hasUVs = false;
	filePath.clear();
	totalVertices = 0;
	totalTriangles = 0;
	meshCenter = glm::vec3(0.0f);
	meshRadius = 1.0f;
	aiDetachAllLogStreams();
}

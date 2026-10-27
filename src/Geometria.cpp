#include "Geometria.h"
#include "Log.h"
#include <cfloat>
#include <cmath>
#include <cstring>
#include <algorithm>
#include <filesystem>
#include <map>
#include <assimp/config.h>
#include <assimp/version.h>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/quaternion.hpp>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/matrix_decompose.hpp>

namespace fs = std::filesystem;

Geometria::Geometria() {}

Geometria::~Geometria() {
	CleanUp();
}

// Los mensajes de Assimp se reenvían a LOG para que salgan en el panel "Consola"
static void AssimpLogCallback(const char* message, char* /*user*/) {
	std::string msg(message ? message : "");
	while (!msg.empty() && (msg.back() == '\n' || msg.back() == '\r')) msg.pop_back();
	if (!msg.empty()) LOG("[Assimp] %s", msg.c_str());
}

static bool assimpLogAttached = false;

void Geometria::DebugShutdown() {
	if (!assimpLogAttached) return;
	aiDetachAllLogStreams();
	assimpLogAttached = false;
}

void Geometria::DebugInit() {
	if (assimpLogAttached) return;
	assimpLogAttached = true;

	// IMPORTANTE: Assimp NO copia el aiLogStream, guarda una referencia a él.
	// Por eso tienen que ser 'static' (vivir todo el programa). Si fueran variables locales,
	// al salir de esta función Assimp se quedaría apuntando a memoria basura y el programa
	// fallaría (access violation en LogToCallbackRedirector::write) al cargar un modelo.
	static aiLogStream debugger;
	static aiLogStream toConsole;

	// Salida del depurador de Visual Studio (como antes)
	debugger = aiGetPredefinedLogStream(aiDefaultLogStream_DEBUGGER, nullptr);
	aiAttachLogStream(&debugger);

	// Y además a nuestro LOG / Consola
	toConsole.callback = AssimpLogCallback;
	toConsole.user = nullptr;
	aiAttachLogStream(&toConsole);
	LOG("Assimp %u.%u.%u inicializado (mensajes redirigidos a la Consola).",
	    aiGetVersionMajor(), aiGetVersionMinor(), aiGetVersionPatch());
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

// Assimp guarda las matrices por filas y glm por columnas -> hay que trasponer
static glm::mat4 ToGlm(const aiMatrix4x4& m)
{
	return glm::transpose(glm::make_mat4(&m.a1));
}

// Los FBX exportados desde Blender (como BakerHouse.fbx) traen en cada nodo una
// rotación de -90º en X y escala 100 para pasar de Z-arriba a Y-arriba, pero
// los vértices YA vienen convertidos a Y-arriba. Si aplicamos esa rotación el
// modelo sale tumbado, así que la quitamos y nos quedamos sólo con la posición.
static glm::mat4 RemoveBlenderAxisCorrection(const glm::mat4& local, const char* nodeName)
{
	glm::vec3 scale, translation, skew;
	glm::vec4 perspective;
	glm::quat rotation;
	if (!glm::decompose(local, scale, rotation, translation, skew, perspective)) return local;

	const glm::quat blenderRot = glm::angleAxis(glm::radians(-90.0f), glm::vec3(1.0f, 0.0f, 0.0f));
	const bool isBlenderRot = std::abs(glm::dot(rotation, blenderRot)) > 0.9999f;
	const bool isScale100 = std::abs(scale.x - 100.0f) < 0.5f &&
	                        std::abs(scale.y - 100.0f) < 0.5f &&
	                        std::abs(scale.z - 100.0f) < 0.5f;
	if (!isBlenderRot || !isScale100) return local;

	LOG("Nodo '%s': corregida rotacion -90 X / escala 100 de exportacion de Blender", nodeName);
	return glm::translate(glm::mat4(1.0f), translation);
}

void Geometria::BuildNode(const aiNode* node, const glm::mat4& pendingLocal, const glm::mat4& parentGlobal,
                          const std::vector<int>& meshRemap, ModelNode& out, glm::vec3& minV, glm::vec3& maxV)
{
	const glm::mat4 local = RemoveBlenderAxisCorrection(pendingLocal * ToGlm(node->mTransformation),
	                                                    node->mName.C_Str());
	const glm::mat4 global = parentGlobal * local;

	out.name = node->mName.length > 0 ? node->mName.C_Str() : "Nodo";
	out.transform = local;

	// Submallas de este nodo + bounding box en espacio del modelo
	for (unsigned int i = 0; i < node->mNumMeshes; ++i) {
		const unsigned int aiIdx = node->mMeshes[i];
		if (aiIdx >= meshRemap.size() || meshRemap[aiIdx] < 0) continue;
		const unsigned int idx = (unsigned int)meshRemap[aiIdx];
		out.meshes.push_back(idx);

		const Mesh& mesh = meshes[idx];
		for (unsigned int v = 0; v < mesh.num_vertex; ++v) {
			glm::vec4 p = global * glm::vec4(mesh.vertex[v * 3 + 0], mesh.vertex[v * 3 + 1], mesh.vertex[v * 3 + 2], 1.0f);
			minV = glm::min(minV, glm::vec3(p));
			maxV = glm::max(maxV, glm::vec3(p));
		}
	}

	for (unsigned int c = 0; c < node->mNumChildren; ++c) {
		const aiNode* child = node->mChildren[c];
		std::string childName = child->mName.C_Str();

		// Nodos auxiliares que crea Assimp para los pivotes del FBX
		// ("Chimney$AssimpFbx$_Rotation", ...): no se muestran, su transform
		// se acumula en el primer nodo real que cuelga de ellos.
		if (childName.find("$AssimpFbx$") != std::string::npos && child->mNumMeshes == 0) {
			glm::mat4 helper = ToGlm(child->mTransformation);
			const aiNode* cur = child;
			while (cur->mNumChildren == 1 && cur->mNumMeshes == 0 &&
			       std::string(cur->mChildren[0]->mName.C_Str()).find("$AssimpFbx$") != std::string::npos) {
				cur = cur->mChildren[0];
				helper = helper * ToGlm(cur->mTransformation);
			}
			for (unsigned int g = 0; g < cur->mNumChildren; ++g) {
				out.children.emplace_back();
				BuildNode(cur->mChildren[g], helper, global, meshRemap, out.children.back(), minV, maxV);
			}
			continue;
		}

		out.children.emplace_back();
		BuildNode(child, glm::mat4(1.0f), global, meshRemap, out.children.back(), minV, maxV);
	}
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

	// Desactivamos los nodos de pivote extra del FBX para que la jerarquía
	// salga limpia (Baker_house, Chimney... en vez de nodos $AssimpFbx$)
	aiPropertyStore* props = aiCreatePropertyStore();
	aiSetImportPropertyInteger(props, AI_CONFIG_IMPORT_FBX_PRESERVE_PIVOTS, 0);
	// Se importa SIN generar normales, para poder saber cuáles vienen en el fichero.
	const unsigned int importFlags = aiProcessPreset_TargetRealtime_MaxQuality &
		~(aiProcess_GenSmoothNormals | aiProcess_GenNormals);
	const aiScene* scene = aiImportFileExWithProperties(file_path.c_str(), importFlags, nullptr, props);
	aiReleasePropertyStore(props);
	if (!scene || !scene->HasMeshes()) {
		LOG("Error loading scene %s: %s", file_path.c_str(), aiGetErrorString());
		return false;
	}

	// Normales: anotar qué mallas las traen en el fichero y, si a alguna le faltan,
	// pedir a Assimp que las calcule (normales suaves). Las que ya tienen no se tocan.
	std::vector<bool> normalsInFile(scene->mNumMeshes, false);
	unsigned int meshesWithoutNormals = 0;
	for (unsigned int m = 0; m < scene->mNumMeshes; ++m) {
		normalsInFile[m] = scene->mMeshes[m] && scene->mMeshes[m]->HasNormals();
		if (!normalsInFile[m]) ++meshesWithoutNormals;
	}
	if (meshesWithoutNormals > 0) {
		LOG("%u submallas sin normales en el fichero: se generan normales suaves.", meshesWithoutNormals);
		scene = aiApplyPostProcessing(scene, aiProcess_GenSmoothNormals);
		if (!scene) {
			LOG("Error al generar normales para %s", file_path.c_str());
			return false;
		}
	}
	unsigned int importedNormals = 0, generatedNormals = 0;

	filePath = file_path;
	glm::vec3 minV(FLT_MAX);
	glm::vec3 maxV(-FLT_MAX);
	totalVertices = 0;
	totalTriangles = 0;

	meshes.reserve(scene->mNumMeshes);
	// Índice de aiMesh -> índice en 'meshes' (-1 si la malla se descarta)
	std::vector<int> meshRemap(scene->mNumMeshes, -1);

	for (unsigned int m = 0; m < scene->mNumMeshes; ++m) {
		const aiMesh* ai_mesh = scene->mMeshes[m];
		if (!ai_mesh || ai_mesh->mNumVertices == 0) continue;
		meshRemap[m] = (int)meshes.size();

		Mesh mesh;
		mesh.num_vertex = ai_mesh->mNumVertices;
		mesh.materialIndex = ai_mesh->mMaterialIndex;
		totalVertices += mesh.num_vertex;

		// Vértices
		mesh.vertex = new float[mesh.num_vertex * 3];
		memcpy(mesh.vertex, ai_mesh->mVertices, sizeof(float) * mesh.num_vertex * 3);

		// AABB local de la submalla (para el modo debug)
		mesh.aabbMin = glm::vec3(FLT_MAX);
		mesh.aabbMax = glm::vec3(-FLT_MAX);
		for (unsigned int v = 0; v < mesh.num_vertex; ++v) {
			const glm::vec3 p(mesh.vertex[v * 3 + 0], mesh.vertex[v * 3 + 1], mesh.vertex[v * 3 + 2]);
			mesh.aabbMin = glm::min(mesh.aabbMin, p);
			mesh.aabbMax = glm::max(mesh.aabbMax, p);
		}

		// Subir vértices a GPU
		glGenBuffers(1, &mesh.id_vertex);
		glBindBuffer(GL_ARRAY_BUFFER, mesh.id_vertex);
		glBufferData(GL_ARRAY_BUFFER, sizeof(float) * mesh.num_vertex * 3, mesh.vertex, GL_STATIC_DRAW);

		// Normales
		if (ai_mesh->HasNormals()) {
			mesh.normalsFromFile = normalsInFile[m];
			if (mesh.normalsFromFile) ++importedNormals; else ++generatedNormals;
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

	// Jerarquía de nodos (y bounding box con las transformaciones de cada nodo)
	rootNode = ModelNode();
	if (scene->mRootNode) {
		BuildNode(scene->mRootNode, glm::mat4(1.0f), glm::mat4(1.0f), meshRemap, rootNode, minV, maxV);
	}

	if (!meshes.empty() && totalVertices > 0 && minV.x <= maxV.x) {
		meshCenter = (minV + maxV) * 0.5f;
		meshRadius = glm::length(maxV - meshCenter);
		if (meshRadius < 0.0001f) meshRadius = 1.0f;
		hasLoaded = true;

		LOG("Modelo cargado: %s (%zu submallas, %u vertices, %u triangulos, UVs: %s)",
			file_path.c_str(), meshes.size(), totalVertices, totalTriangles, hasUVs ? "si" : "no");
		LOG("AABB center=(%.2f, %.2f, %.2f) radius=%.2f", meshCenter.x, meshCenter.y, meshCenter.z, meshRadius);
		LOG("Normales: %u submallas importadas del fichero, %u generadas", importedNormals, generatedNormals);
		LOG("Jerarquia: nodo raiz '%s' con %zu hijos", rootNode.name.c_str(), rootNode.children.size());
	}

	aiReleaseImport(scene);
	return hasLoaded;
}

bool Geometria::ReplaceMaterialTexture(unsigned int materialIndex, const std::shared_ptr<Texture>& newTexture) {
	if (materialIndex >= materials.size() || !newTexture || !newTexture->IsLoaded()) return false;

	MaterialTexture& mt = materials[materialIndex];
	// Guardar la original sólo la primera vez (si se sustituye varias veces, se conserva la del FBX)
	if (!mt.replaced) {
		mt.originalTexture  = mt.texture;
		mt.originalPath     = mt.resolvedPath;
		mt.originalEmbedded = mt.embedded;
		mt.replaced = true;
	}
	mt.texture = newTexture;
	mt.resolvedPath = newTexture->GetPath();
	mt.embedded = false;

	LOG("Material %u: textura sustituida por '%s'", materialIndex, mt.resolvedPath.c_str());
	return true;
}

void Geometria::RestoreMaterialTexture(unsigned int materialIndex) {
	if (materialIndex >= materials.size()) return;

	MaterialTexture& mt = materials[materialIndex];
	if (!mt.replaced) return;
	mt.texture      = mt.originalTexture;
	mt.resolvedPath = mt.originalPath;
	mt.embedded     = mt.originalEmbedded;
	mt.originalTexture.reset();
	mt.originalPath.clear();
	mt.replaced = false;

	LOG("Material %u: textura original restaurada", materialIndex);
}

bool Geometria::HasAnyTexture() const {
	for (const auto& m : materials) {
		if (m.texture && m.texture->IsLoaded()) return true;
	}
	return false;
}

bool Geometria::GetLocalAABB(const std::vector<unsigned int>& meshIndices, glm::vec3& outMin, glm::vec3& outMax) const {
	bool any = false;
	outMin = glm::vec3(FLT_MAX);
	outMax = glm::vec3(-FLT_MAX);
	for (unsigned int idx : meshIndices) {
		if (idx >= meshes.size() || meshes[idx].num_vertex == 0) continue;
		outMin = glm::min(outMin, meshes[idx].aabbMin);
		outMax = glm::max(outMax, meshes[idx].aabbMax);
		any = true;
	}
	return any;
}

bool Geometria::RaycastLocal(const std::vector<unsigned int>& meshIndices, const glm::vec3& origin,
                             const glm::vec3& dir, float& tHit) const {
	bool hit = false;
	float best = FLT_MAX;
	for (unsigned int idx : meshIndices) {
		if (idx >= meshes.size()) continue;
		const Mesh& m = meshes[idx];
		if (!m.vertex || !m.index) continue;

		for (unsigned int i = 0; i + 2 < m.num_index; i += 3) {
			const unsigned int i0 = m.index[i], i1 = m.index[i + 1], i2 = m.index[i + 2];
			if (i0 >= m.num_vertex || i1 >= m.num_vertex || i2 >= m.num_vertex) continue;
			const glm::vec3 v0(m.vertex[i0 * 3], m.vertex[i0 * 3 + 1], m.vertex[i0 * 3 + 2]);
			const glm::vec3 v1(m.vertex[i1 * 3], m.vertex[i1 * 3 + 1], m.vertex[i1 * 3 + 2]);
			const glm::vec3 v2(m.vertex[i2 * 3], m.vertex[i2 * 3 + 1], m.vertex[i2 * 3 + 2]);

			// Möller–Trumbore (sin descartar caras traseras)
			const glm::vec3 e1 = v1 - v0, e2 = v2 - v0;
			const glm::vec3 p = glm::cross(dir, e2);
			const float det = glm::dot(e1, p);
			if (std::fabs(det) < 1e-12f) continue;
			const float invDet = 1.0f / det;
			const glm::vec3 s = origin - v0;
			const float u = glm::dot(s, p) * invDet;
			if (u < 0.0f || u > 1.0f) continue;
			const glm::vec3 q = glm::cross(s, e1);
			const float v = glm::dot(dir, q) * invDet;
			if (v < 0.0f || u + v > 1.0f) continue;
			const float t = glm::dot(e2, q) * invDet;
			if (t > 0.0f && t < best) { best = t; hit = true; }
		}
	}
	if (hit) tHit = best;
	return hit;
}

void Geometria::BuildNormalLines(const std::vector<unsigned int>& meshIndices, const glm::mat4& world, float length,
                                 std::vector<glm::vec3>* vertexLines, std::vector<glm::vec3>* faceLines) const {
	// Las normales no se transforman con la matriz mundo, sino con su inversa traspuesta
	// (si no, se deformarían con escalas no uniformes)
	const glm::mat3 normalMatrix = glm::transpose(glm::inverse(glm::mat3(world)));

	for (unsigned int idx : meshIndices) {
		if (idx >= meshes.size()) continue;
		const Mesh& m = meshes[idx];
		if (!m.vertex) continue;

		// Normales de vértice (las importadas / generadas)
		if (vertexLines && m.normal) {
			for (unsigned int v = 0; v < m.num_vertex; ++v) {
				const glm::vec3 p(world * glm::vec4(m.vertex[v * 3], m.vertex[v * 3 + 1], m.vertex[v * 3 + 2], 1.0f));
				glm::vec3 n = normalMatrix * glm::vec3(m.normal[v * 3], m.normal[v * 3 + 1], m.normal[v * 3 + 2]);
				const float len = glm::length(n);
				if (len < 1e-8f) continue;
				vertexLines->push_back(p);
				vertexLines->push_back(p + n / len * length);
			}
		}

		// Normales de cara: se calculan con el producto vectorial de dos aristas del triángulo
		if (faceLines && m.index) {
			for (unsigned int i = 0; i + 2 < m.num_index; i += 3) {
				const unsigned int i0 = m.index[i], i1 = m.index[i + 1], i2 = m.index[i + 2];
				if (i0 >= m.num_vertex || i1 >= m.num_vertex || i2 >= m.num_vertex) continue;
				const glm::vec3 a(world * glm::vec4(m.vertex[i0 * 3], m.vertex[i0 * 3 + 1], m.vertex[i0 * 3 + 2], 1.0f));
				const glm::vec3 b(world * glm::vec4(m.vertex[i1 * 3], m.vertex[i1 * 3 + 1], m.vertex[i1 * 3 + 2], 1.0f));
				const glm::vec3 c(world * glm::vec4(m.vertex[i2 * 3], m.vertex[i2 * 3 + 1], m.vertex[i2 * 3 + 2], 1.0f));
				glm::vec3 n = glm::cross(b - a, c - a);
				const float len = glm::length(n);
				if (len < 1e-12f) continue;   // triángulo degenerado
				const glm::vec3 center = (a + b + c) / 3.0f;
				faceLines->push_back(center);
				faceLines->push_back(center + n / len * length);
			}
		}
	}
}

glm::mat4 Geometria::GetNormalizeMatrix() const {
	// Auto-centrar y normalizar a tamaño estándar
	const float s = 1.0f / meshRadius;
	glm::mat4 m = glm::scale(glm::mat4(1.0f), glm::vec3(s));
	return glm::translate(m, -meshCenter);
}

void Geometria::Draw(const std::vector<unsigned int>& meshIndices, bool useTextures, const Texture* overrideTexture) const {
	if (!hasLoaded || meshes.empty() || meshIndices.empty()) return;

	glEnableClientState(GL_VERTEX_ARRAY);

	for (unsigned int idx : meshIndices) {
		if (idx >= meshes.size()) continue;
		const Mesh& mesh = meshes[idx];
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

void Geometria::DrawShader(const std::vector<unsigned int>& meshIndices, bool useTextures,
                           const Texture* overrideTexture, GLint locHasTexture) const {
	if (!hasLoaded || meshes.empty() || meshIndices.empty()) return;

	glActiveTexture(GL_TEXTURE0);

	for (unsigned int idx : meshIndices) {
		if (idx >= meshes.size()) continue;
		const Mesh& mesh = meshes[idx];
		if (mesh.id_vertex == 0 || mesh.id_index == 0) continue;

		// Atributo 0: posición
		glBindBuffer(GL_ARRAY_BUFFER, mesh.id_vertex);
		glEnableVertexAttribArray(0);
		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, nullptr);

		// Atributo 1: normal (si la malla no tiene, se envía (0,0,0) y el shader calcula una plana)
		if (mesh.id_normal != 0) {
			glBindBuffer(GL_ARRAY_BUFFER, mesh.id_normal);
			glEnableVertexAttribArray(1);
			glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 0, nullptr);
		} else {
			glDisableVertexAttribArray(1);
			glVertexAttrib3f(1, 0.0f, 0.0f, 0.0f);
		}

		// Atributo 2: coordenadas UV
		if (mesh.id_texcoord != 0) {
			glBindBuffer(GL_ARRAY_BUFFER, mesh.id_texcoord);
			glEnableVertexAttribArray(2);
			glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 0, nullptr);
		} else {
			glDisableVertexAttribArray(2);
			glVertexAttrib2f(2, 0.0f, 0.0f);
		}

		// Textura de esta submalla (misma lógica que Draw)
		const Texture* tex = nullptr;
		if (useTextures && mesh.id_texcoord != 0) {
			if (overrideTexture && overrideTexture->IsLoaded()) {
				tex = overrideTexture;
			} else if (mesh.materialIndex < materials.size()) {
				const auto& mt = materials[mesh.materialIndex].texture;
				if (mt && mt->IsLoaded()) tex = mt.get();
			}
		}
		glBindTexture(GL_TEXTURE_2D, tex ? tex->GetID() : 0);
		if (locHasTexture >= 0) glUniform1i(locHasTexture, tex ? 1 : 0);

		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mesh.id_index);
		glDrawElements(GL_TRIANGLES, mesh.num_index, GL_UNSIGNED_INT, nullptr);
	}

	glDisableVertexAttribArray(0);
	glDisableVertexAttribArray(1);
	glDisableVertexAttribArray(2);
	glBindTexture(GL_TEXTURE_2D, 0);
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
}

// ─────────────────────────────────────────────
//  Geometrías básicas (primitivas)
// ─────────────────────────────────────────────

const char* Geometria::PrimitiveName(PrimitiveType type) {
	switch (type) {
	case PrimitiveType::Cube:     return "Cubo";
	case PrimitiveType::Sphere:   return "Esfera";
	case PrimitiveType::Cylinder: return "Cilindro";
	case PrimitiveType::Cone:     return "Cono";
	case PrimitiveType::Plane:    return "Plano";
	case PrimitiveType::Pyramid:  return "Piramide";
	}
	return "Primitiva";
}

namespace {
	// Datos de una malla mientras se construye
	struct MeshBuilder {
		std::vector<float> pos, nrm, uv;
		std::vector<unsigned int> idx;

		unsigned int Vertex(const glm::vec3& p, const glm::vec3& n, const glm::vec2& t) {
			pos.insert(pos.end(), { p.x, p.y, p.z });
			nrm.insert(nrm.end(), { n.x, n.y, n.z });
			uv.insert(uv.end(), { t.x, t.y });
			return (unsigned int)(pos.size() / 3 - 1);
		}
		void Tri(unsigned int a, unsigned int b, unsigned int c) { idx.insert(idx.end(), { a, b, c }); }
		void Quad(unsigned int a, unsigned int b, unsigned int c, unsigned int d) { Tri(a, b, c); Tri(a, c, d); }

		// Cara plana de 4 esquinas (en orden antihorario visto desde fuera)
		void Face(const glm::vec3& a, const glm::vec3& b, const glm::vec3& c, const glm::vec3& d) {
			const glm::vec3 n = glm::normalize(glm::cross(b - a, c - a));
			const unsigned int i0 = Vertex(a, n, { 0, 0 }), i1 = Vertex(b, n, { 1, 0 });
			const unsigned int i2 = Vertex(c, n, { 1, 1 }), i3 = Vertex(d, n, { 0, 1 });
			Quad(i0, i1, i2, i3);
		}
	};

	const float PI = 3.14159265358979f;

	void BuildCube(MeshBuilder& m) {
		const float h = 0.5f;
		const glm::vec3 p[8] = { {-h,-h,-h}, { h,-h,-h}, { h, h,-h}, {-h, h,-h},
		                         {-h,-h, h}, { h,-h, h}, { h, h, h}, {-h, h, h} };
		m.Face(p[4], p[5], p[6], p[7]);   // +Z
		m.Face(p[1], p[0], p[3], p[2]);   // -Z
		m.Face(p[5], p[1], p[2], p[6]);   // +X
		m.Face(p[0], p[4], p[7], p[3]);   // -X
		m.Face(p[7], p[6], p[2], p[3]);   // +Y
		m.Face(p[0], p[1], p[5], p[4]);   // -Y
	}

	void BuildPlane(MeshBuilder& m) {
		const float h = 0.5f;
		m.Face({ -h, 0, h }, { h, 0, h }, { h, 0, -h }, { -h, 0, -h });   // mirando hacia +Y
	}

	void BuildSphere(MeshBuilder& m, int slices = 32, int stacks = 16) {
		const float r = 0.5f;
		for (int i = 0; i <= stacks; ++i) {
			const float v = (float)i / stacks;
			const float phi = v * PI;   // 0 = polo norte, PI = polo sur
			for (int j = 0; j <= slices; ++j) {
				const float u = (float)j / slices;
				const float theta = u * 2.0f * PI;
				const float sp = (i == 0 || i == stacks) ? 0.0f : std::sin(phi);   // polos exactos
				const float cp = (i == 0) ? 1.0f : (i == stacks ? -1.0f : std::cos(phi));
				const glm::vec3 n(sp * std::sin(theta), cp, sp * std::cos(theta));
				m.Vertex(n * r, n, { u, 1.0f - v });
			}
		}
		for (int i = 0; i < stacks; ++i) {
			for (int j = 0; j < slices; ++j) {
				const unsigned int a = i * (slices + 1) + j, b = a + slices + 1;
				// En los polos el cuadrilátero se reduce a un triángulo (dos esquinas coinciden)
				if (i != 0)          m.Tri(a, b, a + 1);
				if (i != stacks - 1) m.Tri(a + 1, b, b + 1);
			}
		}
	}

	// Cilindro (topRadius = bottomRadius) o cono (topRadius = 0), altura 1, centrado en el origen
	void BuildCylinder(MeshBuilder& m, float bottomRadius, float topRadius, int slices = 32) {
		const float h = 0.5f;
		// Pendiente del lateral, para que las normales del cono sean correctas
		const float slope = (bottomRadius - topRadius) / 1.0f;
		for (int j = 0; j <= slices; ++j) {
			const float u = (float)j / slices;
			const float t = u * 2.0f * PI;
			const glm::vec3 dir(std::sin(t), 0.0f, std::cos(t));
			const glm::vec3 n = glm::normalize(glm::vec3(dir.x, slope, dir.z));
			m.Vertex(dir * bottomRadius + glm::vec3(0, -h, 0), n, { u, 0.0f });
			m.Vertex(dir * topRadius    + glm::vec3(0,  h, 0), n, { u, 1.0f });
		}
		for (int j = 0; j < slices; ++j) {
			const unsigned int b0 = j * 2, t0 = b0 + 1, b1 = b0 + 2, t1 = b0 + 3;
			m.Quad(b0, b1, t1, t0);
		}
		// Tapas (abajo siempre; arriba sólo si no es un cono)
		auto cap = [&](float y, float radius, bool up) {
			const glm::vec3 n(0, up ? 1.0f : -1.0f, 0);
			const unsigned int center = m.Vertex({ 0, y, 0 }, n, { 0.5f, 0.5f });
			for (int j = 0; j <= slices; ++j) {
				const float t = (float)j / slices * 2.0f * PI;
				m.Vertex({ std::sin(t) * radius, y, std::cos(t) * radius }, n,
				         { 0.5f + 0.5f * std::sin(t), 0.5f + 0.5f * std::cos(t) });
			}
			for (int j = 0; j < slices; ++j) {
				const unsigned int a = center + 1 + j, b = a + 1;
				if (up) m.Tri(center, a, b); else m.Tri(center, b, a);
			}
		};
		cap(-h, bottomRadius, false);
		if (topRadius > 0.0f) cap(h, topRadius, true);
	}

	void BuildPyramid(MeshBuilder& m) {
		const float h = 0.5f;
		const glm::vec3 a(-h, -h, h), b(h, -h, h), c(h, -h, -h), d(-h, -h, -h), top(0, h, 0);
		m.Face(d, c, b, a);   // base (mirando hacia -Y)
		auto side = [&](const glm::vec3& p0, const glm::vec3& p1) {
			const glm::vec3 n = glm::normalize(glm::cross(p1 - p0, top - p0));
			const unsigned int i0 = m.Vertex(p0, n, { 0, 0 }), i1 = m.Vertex(p1, n, { 1, 0 });
			const unsigned int i2 = m.Vertex(top, n, { 0.5f, 1 });
			m.Tri(i0, i1, i2);
		};
		side(a, b); side(b, c); side(c, d); side(d, a);
	}
}

bool Geometria::CreatePrimitive(PrimitiveType type) {
	CleanUp();

	MeshBuilder b;
	switch (type) {
	case PrimitiveType::Cube:     BuildCube(b); break;
	case PrimitiveType::Sphere:   BuildSphere(b); break;
	case PrimitiveType::Cylinder: BuildCylinder(b, 0.5f, 0.5f); break;
	case PrimitiveType::Cone:     BuildCylinder(b, 0.5f, 0.0f); break;
	case PrimitiveType::Plane:    BuildPlane(b); break;
	case PrimitiveType::Pyramid:  BuildPyramid(b); break;
	}

	Mesh mesh;
	mesh.num_vertex = (unsigned int)(b.pos.size() / 3);
	mesh.num_index = (unsigned int)b.idx.size();
	mesh.vertex = new float[b.pos.size()];
	mesh.normal = new float[b.nrm.size()];
	mesh.texcoord = new float[b.uv.size()];
	mesh.index = new unsigned int[b.idx.size()];
	std::copy(b.pos.begin(), b.pos.end(), mesh.vertex);
	std::copy(b.nrm.begin(), b.nrm.end(), mesh.normal);
	std::copy(b.uv.begin(), b.uv.end(), mesh.texcoord);
	std::copy(b.idx.begin(), b.idx.end(), mesh.index);
	mesh.normalsFromFile = false;

	mesh.aabbMin = glm::vec3(FLT_MAX);
	mesh.aabbMax = glm::vec3(-FLT_MAX);
	for (unsigned int v = 0; v < mesh.num_vertex; ++v) {
		const glm::vec3 p(mesh.vertex[v * 3], mesh.vertex[v * 3 + 1], mesh.vertex[v * 3 + 2]);
		mesh.aabbMin = glm::min(mesh.aabbMin, p);
		mesh.aabbMax = glm::max(mesh.aabbMax, p);
	}

	// Subir a la GPU (igual que en Load)
	glGenBuffers(1, &mesh.id_vertex);
	glBindBuffer(GL_ARRAY_BUFFER, mesh.id_vertex);
	glBufferData(GL_ARRAY_BUFFER, sizeof(float) * b.pos.size(), mesh.vertex, GL_STATIC_DRAW);
	glGenBuffers(1, &mesh.id_normal);
	glBindBuffer(GL_ARRAY_BUFFER, mesh.id_normal);
	glBufferData(GL_ARRAY_BUFFER, sizeof(float) * b.nrm.size(), mesh.normal, GL_STATIC_DRAW);
	glGenBuffers(1, &mesh.id_texcoord);
	glBindBuffer(GL_ARRAY_BUFFER, mesh.id_texcoord);
	glBufferData(GL_ARRAY_BUFFER, sizeof(float) * b.uv.size(), mesh.texcoord, GL_STATIC_DRAW);
	glGenBuffers(1, &mesh.id_index);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mesh.id_index);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(unsigned int) * b.idx.size(), mesh.index, GL_STATIC_DRAW);
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

	meshes.push_back(mesh);
	totalVertices = mesh.num_vertex;
	totalTriangles = mesh.num_index / 3;
	hasUVs = true;
	isPrimitive = true;
	filePath = std::string("(primitiva) ") + PrimitiveName(type);

	rootNode = ModelNode();
	rootNode.name = PrimitiveName(type);
	rootNode.meshes.push_back(0);

	meshCenter = (mesh.aabbMin + mesh.aabbMax) * 0.5f;
	meshRadius = glm::length(mesh.aabbMax - meshCenter);
	hasLoaded = true;

	LOG("Primitiva creada: %s (%u vertices, %u triangulos)", PrimitiveName(type), totalVertices, totalTriangles);
	return true;
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
	rootNode = ModelNode();
	materials.clear(); // libera las texturas (shared_ptr) de este modelo

	hasLoaded = false;
	hasUVs = false;
	filePath.clear();
	totalVertices = 0;
	totalTriangles = 0;
	meshCenter = glm::vec3(0.0f);
	meshRadius = 1.0f;
	isPrimitive = false;
	// (Antes aquí se llamaba a aiDetachAllLogStreams(), pero eso desconectaba el log de Assimp
	//  cada vez que se cargaba un modelo. Ahora se hace una sola vez en DebugShutdown.)
}

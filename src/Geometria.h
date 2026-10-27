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

	// Origen de las normales: true = venían en el fichero (FBX/OBJ),
	// false = el fichero no traía y las ha calculado Assimp (normales suaves)
	bool normalsFromFile = false;

	// AABB de la submalla en su espacio local (se calcula al cargar)
	glm::vec3 aabbMin = glm::vec3(0.0f);
	glm::vec3 aabbMax = glm::vec3(0.0f);
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

	// Si el usuario ha sustituido la textura arrastrando una imagen, se guarda
	// la original para poder restaurarla
	bool replaced = false;
	std::shared_ptr<Texture> originalTexture;
	std::string originalPath;
	bool originalEmbedded = false;
};

// Geometrías básicas que el motor puede crear sin cargar ningún fichero
enum class PrimitiveType { Cube, Sphere, Cylinder, Cone, Plane, Pyramid };

class Geometria {
public:
	Geometria();
	~Geometria();

	// Deshabilitar copia directa para evitar doble liberación de VBOs
	Geometria(const Geometria&) = delete;
	Geometria& operator=(const Geometria&) = delete;

	static void DebugInit();       // conecta los mensajes de Assimp al LOG / Consola
	static void DebugShutdown();   // los desconecta (al cerrar el motor)

	bool Load(const std::string& file_path);

	// Crea una geometría básica (cubo, esfera...) de tamaño 1 centrada en el origen,
	// con normales y coordenadas UV (para poder ponerle textura)
	bool CreatePrimitive(PrimitiveType type);
	static const char* PrimitiveName(PrimitiveType type);
	bool IsPrimitive() const { return isPrimitive; }

	// Dibuja las submallas indicadas (las de un nodo de la jerarquía).
	// La matriz del nodo ya debe estar cargada en OpenGL antes de llamar.
	//  useTextures     : si es false se dibuja sólo con el color del material
	//  overrideTexture : si no es nullptr, se usa esta textura en todas las submallas
	//                    en lugar de las del FBX
	void Draw(const std::vector<unsigned int>& meshIndices, bool useTextures = true,
	          const Texture* overrideTexture = nullptr) const;

	// Igual que Draw, pero para dibujar con un shader GLSL (el programa ya debe estar activo).
	// Envía los VBOs como atributos de vértice: 0 = posición, 1 = normal, 2 = UV.
	// locHasTexture es la posición del uniform bool que indica si la submalla tiene textura.
	void DrawShader(const std::vector<unsigned int>& meshIndices, bool useTextures,
	                const Texture* overrideTexture, GLint locHasTexture) const;

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

	// Sustituye la textura de un material por otra imagen (afecta a todas las mallas que lo usan)
	bool ReplaceMaterialTexture(unsigned int materialIndex, const std::shared_ptr<Texture>& newTexture);
	// Vuelve a poner la textura original del FBX
	void RestoreMaterialTexture(unsigned int materialIndex);

	// Jerarquía de nodos del FBX
	const ModelNode& GetRootNode() const { return rootNode; }

	// Información por submalla (para el inspector)
	unsigned int GetMeshCount() const { return (unsigned int)meshes.size(); }
	unsigned int GetMeshVertices(unsigned int i) const { return i < meshes.size() ? meshes[i].num_vertex : 0; }
	unsigned int GetMeshTriangles(unsigned int i) const { return i < meshes.size() ? meshes[i].num_index / 3 : 0; }
	unsigned int GetMeshMaterial(unsigned int i) const { return i < meshes.size() ? meshes[i].materialIndex : 0; }

	// AABB local que envuelve las submallas indicadas (las de un nodo).
	// Devuelve false si ninguna de ellas tiene vértices.
	bool GetLocalAABB(const std::vector<unsigned int>& meshIndices, glm::vec3& outMin, glm::vec3& outMax) const;

	// Lanza un rayo (en el espacio local del nodo) contra los triángulos de las submallas
	// indicadas. Devuelve true si choca y en tHit la distancia paramétrica (origen + t * dir)
	// del impacto más cercano.
	bool RaycastLocal(const std::vector<unsigned int>& meshIndices, const glm::vec3& origin,
	                  const glm::vec3& dir, float& tHit) const;

	// --- Normales ---
	bool MeshHasNormals(unsigned int i) const { return i < meshes.size() && meshes[i].normal != nullptr; }
	bool MeshNormalsFromFile(unsigned int i) const { return i < meshes.size() && meshes[i].normalsFromFile; }

	// Genera los segmentos (pares de puntos en coordenadas de MUNDO) para visualizar las normales
	// de las submallas indicadas. 'world' es la matriz mundo del nodo y 'length' la longitud de
	// cada segmento en unidades de mundo.
	//  vertexLines : una línea por vértice, en la dirección de su normal importada
	//  faceLines   : una línea por triángulo, desde su centro, en la dirección de la normal de la cara
	void BuildNormalLines(const std::vector<unsigned int>& meshIndices, const glm::mat4& world, float length,
	                      std::vector<glm::vec3>* vertexLines, std::vector<glm::vec3>* faceLines) const;

private:
	void LoadMaterials(const aiScene* scene, const std::string& modelPath);
	// Copia recursivamente un aiNode (y sus hijos) a la jerarquía ModelNode
	void BuildNode(const aiNode* node, const glm::mat4& pendingLocal, const glm::mat4& parentGlobal,
	               const std::vector<int>& meshRemap, ModelNode& out, glm::vec3& minV, glm::vec3& maxV);

	std::vector<Mesh> meshes;
	std::vector<MaterialTexture> materials;
	ModelNode rootNode;
	bool hasLoaded = false;
	bool isPrimitive = false;
	bool hasUVs = false;
	std::string filePath;
	unsigned int totalVertices = 0;
	unsigned int totalTriangles = 0;

	// Centro y radio del bounding box (calculados en Load)
	glm::vec3 meshCenter = glm::vec3(0.0f);
	float meshRadius = 1.0f;
};

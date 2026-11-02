#include "ComponentMesh.h"
#include "Geometria.h"

bool ComponentMesh::HasGeometry() const {
    return geometry && geometry->HasMesh() && !meshIndices.empty();
}

unsigned int ComponentMesh::GetVertexCount() const {
    unsigned int count = 0;
    if (geometry) for (unsigned int idx : meshIndices) count += geometry->GetMeshVertices(idx);
    return count;
}

unsigned int ComponentMesh::GetTriangleCount() const {
    unsigned int count = 0;
    if (geometry) for (unsigned int idx : meshIndices) count += geometry->GetMeshTriangles(idx);
    return count;
}

void ComponentMesh::CopyFrom(const ComponentMesh& other) {
    SetActive(other.IsActive());
    geometry = other.geometry;          // la geometría de GPU se comparte, no se duplica
    meshIndices = other.meshIndices;
    color = other.color;
    wireframe = other.wireframe;
}

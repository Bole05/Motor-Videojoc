#include "Renderer.h"
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include "Log.h"

// ─────────────────────────────────────────────
//  Constructor / Destructor
// ─────────────────────────────────────────────

Renderer::Renderer() : angle(0.0f) {
    buffs[0] = buffs[1] = buffs[2] = 0;
}

Renderer::~Renderer() {}

// ─────────────────────────────────────────────
//  Init: luz, textura y cubo
// ─────────────────────────────────────────────

bool Renderer::Init() {
    // Configurar GL_LIGHT0 (luz blanca direccional)
    GLfloat lightAmbient[]  = { 0.25f, 0.25f, 0.25f, 1.0f };
    GLfloat lightDiffuse[]  = { 0.9f,  0.9f,  0.9f,  1.0f };
    GLfloat lightSpecular[] = { 0.5f,  0.5f,  0.5f,  1.0f };
    glLightfv(GL_LIGHT0, GL_AMBIENT,  lightAmbient);
    glLightfv(GL_LIGHT0, GL_DIFFUSE,  lightDiffuse);
    glLightfv(GL_LIGHT0, GL_SPECULAR, lightSpecular);
    glEnable(GL_LIGHT0);
    glLightModeli(GL_LIGHT_MODEL_TWO_SIDE, GL_TRUE);

    // Cargar textura del cubo
    if (!texture.Load("assets/Lenna.dds")) {
        texture.Load("assets/Lenna.png");
    }

    // Crear geometría del cubo (VBOs)
    InitCube();

    return true;
}

// ─────────────────────────────────────────────
//  Update: lógica de animación
// ─────────────────────────────────────────────

void Renderer::Update() {
    // Avanzar la rotación del cubo cada frame
    angle += 0.01f;
}

// ─────────────────────────────────────────────
//  LoadModel: delega en Geometria
// ─────────────────────────────────────────────

void Renderer::LoadModel(const std::string& path) {
    geometria.Load(path);
}

// ─────────────────────────────────────────────
//  Render: punto de entrada de cada frame
// ─────────────────────────────────────────────

void Renderer::Render(Window& window) {
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // Proyección perspectiva (far amplio para soportar cualquier escala de FBX)
    float aspect = (float)window.GetWidth() / (float)window.GetHeight();
    glm::mat4 projection = glm::perspective(glm::radians(45.0f), aspect, 0.1f, 5000.0f);
    glMatrixMode(GL_PROJECTION);
    glLoadMatrixf(glm::value_ptr(projection));

    glMatrixMode(GL_MODELVIEW);

    if (!geometria.HasMesh()) {
        RenderCube(window);  // Sin modelo cargado → cubo giratorio
    }
    else {
        RenderModel();       // Modelo FBX/OBJ con iluminación 3D
    }

    window.SwapBuffers();
}

// ─────────────────────────────────────────────
//  RenderCube: cubo texturizado giratorio
// ─────────────────────────────────────────────

void Renderer::RenderCube(Window& window) {
    glPushMatrix();
    {
        glm::mat4 cubeMatrix = glm::mat4(1.0f);
        cubeMatrix = glm::translate(cubeMatrix, glm::vec3(0.0f, 0.0f, -3.5f));
        cubeMatrix = glm::rotate(cubeMatrix, angle, glm::vec3(0.5f, 1.0f, 0.2f));
        glLoadMatrixf(glm::value_ptr(cubeMatrix));

        glEnableClientState(GL_VERTEX_ARRAY);
        glEnableClientState(GL_TEXTURE_COORD_ARRAY);

        glBindBuffer(GL_ARRAY_BUFFER, buffs[0]);
        glVertexPointer(3, GL_FLOAT, 0, NULL);

        glBindBuffer(GL_ARRAY_BUFFER, buffs[2]);
        glTexCoordPointer(2, GL_FLOAT, 0, NULL);
        glColor3f(1.0f, 1.0f, 1.0f);

        glEnable(GL_TEXTURE_2D);
        texture.Bind();
        glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);

        glDrawArrays(GL_TRIANGLES, 0, 36);

        glDisableClientState(GL_TEXTURE_COORD_ARRAY);
        glDisableClientState(GL_VERTEX_ARRAY);
        glDisable(GL_TEXTURE_2D);
    }
    glPopMatrix();
}

// ─────────────────────────────────────────────
//  RenderModel: modelo Assimp con iluminación
// ─────────────────────────────────────────────

void Renderer::RenderModel() {
    glPushMatrix();
    {
        // Posición de luz con modelview = identidad → fija relativa a la cámara
        glLoadIdentity();
        GLfloat lightPos[] = { 1.0f, 1.5f, 2.0f, 0.0f };
        glLightfv(GL_LIGHT0, GL_POSITION, lightPos);

        // Cámara: el modelo se auto-centra y normaliza dentro de Draw()
        glm::mat4 geomMatrix = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.0f, -2.0f));
        glLoadMatrixf(glm::value_ptr(geomMatrix));

        glEnable(GL_LIGHTING);
        glEnable(GL_NORMALIZE);  // Compensar el glScalef de Draw()

        // Material gris neutro (estilo visor 3D)
        GLfloat matDiffuse[]  = { 0.75f, 0.75f, 0.75f, 1.0f };
        GLfloat matAmbient[]  = { 0.3f,  0.3f,  0.3f,  1.0f };
        GLfloat matSpecular[] = { 0.4f,  0.4f,  0.4f,  1.0f };
        glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE,   matDiffuse);
        glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT,   matAmbient);
        glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR,  matSpecular);
        glMaterialf (GL_FRONT_AND_BACK, GL_SHININESS, 32.0f);

        geometria.Draw();

        glDisable(GL_NORMALIZE);
        glDisable(GL_LIGHTING);
    }
    glPopMatrix();
}

// ─────────────────────────────────────────────
//  CleanUp: libera GPU
// ─────────────────────────────────────────────

void Renderer::CleanUp() {
    if (buffs[0] != 0) {
        glDeleteBuffers(3, buffs);
        buffs[0] = buffs[1] = buffs[2] = 0;
    }
    texture.CleanUp();
    geometria.CleanUp();
}

// ─────────────────────────────────────────────
//  InitCube: VBOs del cubo (posiciones, UVs)
// ─────────────────────────────────────────────

void Renderer::InitCube() {
    // Vértices de las 6 caras del cubo (2 triángulos por cara)
    float v[] = {
        // Cara frontal
        -0.5f, -0.5f,  0.5f,   0.5f, -0.5f,  0.5f,   0.5f,  0.5f,  0.5f,
        -0.5f, -0.5f,  0.5f,   0.5f,  0.5f,  0.5f,  -0.5f,  0.5f,  0.5f,
        // Cara trasera
        -0.5f, -0.5f, -0.5f,  -0.5f,  0.5f, -0.5f,   0.5f,  0.5f, -0.5f,
        -0.5f, -0.5f, -0.5f,   0.5f,  0.5f, -0.5f,   0.5f, -0.5f, -0.5f,
        // Cara izquierda
        -0.5f,  0.5f,  0.5f,  -0.5f,  0.5f, -0.5f,  -0.5f, -0.5f, -0.5f,
        -0.5f,  0.5f,  0.5f,  -0.5f, -0.5f, -0.5f,  -0.5f, -0.5f,  0.5f,
        // Cara derecha
         0.5f,  0.5f,  0.5f,   0.5f, -0.5f, -0.5f,   0.5f,  0.5f, -0.5f,
         0.5f, -0.5f, -0.5f,   0.5f,  0.5f,  0.5f,   0.5f, -0.5f,  0.5f,
        // Cara superior
        -0.5f,  0.5f, -0.5f,  -0.5f,  0.5f,  0.5f,   0.5f,  0.5f,  0.5f,
        -0.5f,  0.5f, -0.5f,   0.5f,  0.5f,  0.5f,   0.5f,  0.5f, -0.5f,
        // Cara inferior
        -0.5f, -0.5f, -0.5f,   0.5f, -0.5f, -0.5f,   0.5f, -0.5f,  0.5f,
        -0.5f, -0.5f, -0.5f,   0.5f, -0.5f,  0.5f,  -0.5f, -0.5f,  0.5f
    };

    // Colores por cara
    float c[36 * 3];
    float colorsPerFace[6][3] = {
        {1.0f, 0.0f, 0.0f}, // Rojo
        {0.0f, 1.0f, 0.0f}, // Verde
        {0.0f, 0.0f, 1.0f}, // Azul
        {1.0f, 1.0f, 0.0f}, // Amarillo
        {1.0f, 0.0f, 1.0f}, // Magenta
        {0.0f, 1.0f, 1.0f}  // Cian
    };
    for (int face = 0; face < 6; face++) {
        for (int vert = 0; vert < 6; vert++) {
            int idx = (face * 6 + vert) * 3;
            c[idx + 0] = colorsPerFace[face][0];
            c[idx + 1] = colorsPerFace[face][1];
            c[idx + 2] = colorsPerFace[face][2];
        }
    }

    // Coordenadas UV por cara
    float uvs[] = {
        0.0f, 0.0f,  1.0f, 0.0f,  1.0f, 1.0f,
        0.0f, 0.0f,  1.0f, 1.0f,  0.0f, 1.0f,
        1.0f, 0.0f,  1.0f, 1.0f,  0.0f, 1.0f,
        1.0f, 0.0f,  0.0f, 1.0f,  0.0f, 0.0f,
        1.0f, 1.0f,  1.0f, 0.0f,  0.0f, 0.0f,
        1.0f, 1.0f,  0.0f, 0.0f,  0.0f, 1.0f,
        0.0f, 1.0f,  1.0f, 0.0f,  0.0f, 0.0f,
        1.0f, 0.0f,  0.0f, 1.0f,  1.0f, 1.0f,
        0.0f, 1.0f,  0.0f, 0.0f,  1.0f, 0.0f,
        0.0f, 1.0f,  1.0f, 0.0f,  1.0f, 1.0f,
        0.0f, 1.0f,  1.0f, 1.0f,  1.0f, 0.0f,
        0.0f, 1.0f,  1.0f, 0.0f,  0.0f, 0.0f
    };

    glGenBuffers(3, buffs);

    glBindBuffer(GL_ARRAY_BUFFER, buffs[0]);
    glBufferData(GL_ARRAY_BUFFER, sizeof(v),   v,   GL_STATIC_DRAW);

    glBindBuffer(GL_ARRAY_BUFFER, buffs[1]);
    glBufferData(GL_ARRAY_BUFFER, sizeof(c),   c,   GL_STATIC_DRAW);

    glBindBuffer(GL_ARRAY_BUFFER, buffs[2]);
    glBufferData(GL_ARRAY_BUFFER, sizeof(uvs), uvs, GL_STATIC_DRAW);
}

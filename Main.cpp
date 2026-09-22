#include<glad/glad.h>
#include <SDL3/SDL.h>
#include <fmt/core.h>
#include<SDL3/SDL_main.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>



int main(int argc, char* argv[]) {    //int argc y argv, argc carga el trabajo de contador sobre cantidad de operacion que has introducido, argv para guardar operaciones(su nombre, el valor etc), su posicion 0 siempre sera ocupado por el nombre de la programa en esta caso main.exe
    // 1. Inicializar el subsistema de video de SDL3
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {//SDL int su funcion es como un mechero, osea carga de encender la librerian sdl3, y como que en este situacion solo necesitamos que la parte del video que se enciende, por lo tanto escribirle dentro del parectesis de que solo queremos activar la parte de video 
        //pq <0, significa que existe algu error de que el sistema que no se puede encenderlo, pues avisa por la pantalla de que existe error de que esta rompiendo la ejecutación de la programa
        fmt::print("Error al inicializar SDL3: {}\n", SDL_GetError());
        return -1;//retornamos -1 para que el sistema sabe que algo que esta mal
    }
    fmt::print("SDL3 inicializado correctamente.\n"); //si todo bien, saltamos el if y mostra directamente el mensaje de exito por la pantalla

    //Inicialización OpenGL  antes de crearse la ventana, le dicen a la tarjeta del grafica que herramienta interna de hardware que vamos a necesitar para tranajar en  3d
    //Configuración de las reglas del juego/Configurar la nuestra tarjeta de grafica antes de que empieza trabajar
    //Version OpenGL, evitar versiones antiguos, oclrtar lo
    //SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE); // su funcion es decir a la trajeta de grafica que se usa directamente la version mas moderna y ocultando las versiones viejas, pq OGL tiene muchas vesiones antiguos de hace muchos años que actualmente ya no esta usando, pero por la motivo de que no rompre los juegos que han creado con la version vieja, los tecnicos no han limpiado estas version
    //por lo tanto pñara que no cargamops todas las versiuon de Open GL, usamos esta funcion para solamnete accedir la que mas moderna
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_COMPATIBILITY);
    //Activa Double Buffer 
    //Su funcion es preparar el siguiente frame antes de que termina el frame actual, para evitar la separacion entre frames
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);//1 siginifica activa o enciende

    //Reservar un bufer de profundidad Z.buffer de 24 bits de memoria
    //Su funcion es calcular y decir a la camara que objeto o imagen esta delanmte de todo, cual esta atras, lo de derante va tapar lo de detras, osea profundidad

    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);

    //Reservar buffer de plantilla de 8 bits
    //Su funcion es decir a la tarjeta el borde de cada imagen o dibujo que no aparece parte que no es necesario
    SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);
    //Vesion de OpenGL exactanebte que utilizamos
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 4);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 6);

    Uint32 flags = SDL_WINDOW_OPENGL;

    // FULLSCREEN   
#define WINDOW_REZIZEABLE
#ifdef  WINDOW_FULLSCREEN
    flags |= SDL_WINDOW_FULLSCREEN;
#endif // 
#ifdef  WINDOW_REZIZEABLE
    flags |= SDL_WINDOW_RESIZABLE;
#endif // 
    unsigned int wWidth = 1280;
    unsigned int wHeight = 720;

    SDL_Window* window = SDL_CreateWindow("Engine", wWidth, wHeight, flags);

    if (window == nullptr) {
        return -1;
    }



    // 2. Crear el módulo Window (Ventana)
    // Parámetros: Título, Ancho, Alto, Opciones adicionales
   /* SDL_Window* window = SDL_CreateWindow("Motor - Primera Ventana", 800, 600, SDL_WINDOW_OPENGL);*/
    //SDL_Window, su funcion es decir a la memoria o la computadera de que reservale un espacio para crear una ventana, se anomena con un nombre, * resevar en la memoria en lugar de que crear una variable
    //SDL_CreateWindow es la funcion de que se carga crear la ventana, primero darle un nombre, x, y, y si es una ventana normal =0, o si queremos que esa ventana contiene otra propiedad, en lugar de 0 podemos poder otra funcion del sdl, por ejemplo pantalla completa, tranparente etc
    //if (!window) {
    //    //para caso de que no pudiera crear correctamente la ventana
    //    fmt::print("Error al crear la ventana: {}\n", SDL_GetError());
    //    //nos avisa con el mensaje
    //    SDL_Quit();//su funcion es muy importate, se carga de apagar todo lo que hemos encentido y limpiar la memoria antes de ir se, osea antes de apargar la ejecutacion de la programa
    //    return -1;
    //    //se carga de que cerrar la programa, osea cerrar la ejecutación de c++ y la funcion main
    //}
    //fmt::print("Ventana creada con exito.\n");

    //OpenGL
    // Crear el contexto de OpenGL
    SDL_GLContext gl_context = SDL_GL_CreateContext(window);
    if (!gl_context) {
        return -1;
    }

    //// Inicializar Glad
    // forma 1
    if (gladLoadGLLoader(reinterpret_cast<GLADloadproc>(SDL_GL_GetProcAddress)) == 0) return -1;

    //Forma 2
    // 
    //int version = gladLoadGLLoader((GLADloadproc)SDL_GL_GetProcAddress);
    //if (version == 0) {
    //    fmt::print("Error al cargar la libreria glad\n");
    //    return -1;
    //}

    // Tarea de los deberes: Imprimir la info del driver
    fmt::print("Vendor: {}\n", (const char*)glGetString(GL_VENDOR));
    fmt::print("Renderer: {}\n", (const char*)glGetString(GL_RENDERER));
    fmt::print("OpenGL version supported: {}\n", (const char*)glGetString(GL_VERSION));
    fmt::print("GLSL: {}\n", (const char*)glGetString(GL_SHADING_LANGUAGE_VERSION));

    //

    glViewport(0, 0, wWidth, wHeight);
    glClearColor(0.1f, 0.25f, 0.5f, 1.0f);

    //-------------------------------Triangulo-----------------------------------//
    //float v[] = { -0.5f,-0.5f,0.0f,0.5f,-0.5f,0.0f,0.0f,0.5f,0.0f };
    //float c[] = { 1.0f,0.0f,0.0f,0.0f,1.0f,0.0f,0.0f,0.0f,1.0f };
    //-----------------------------------------------------------------------------//

    //-------------------------------Rectangulo-----------------------------------//
    float v[] = { -0.5f,-0.5f,0.0f,
                   0.5f,-0.5f,0.0f,
                   0.5f,0.5f,0.0f,

                  -0.5f,-0.5f,0.0f,
                  0.5f,0.5f,0.0f,
                  -0.5f,0.5f,0.0f
    
    
    
    };
    float c[] = {
        1.0f, 0.0f, 0.0f, // Rojo (V0)
        0.0f, 1.0f, 0.0f, // Verde (V1)
        0.0f, 0.0f, 1.0f, // Azul (V2)
        1.0f, 0.0f, 0.0f, // Rojo (V3)
        0.0f, 0.0f, 1.0f, // Azul (V4)
        1.0f, 1.0f, 0.0f  // Amarillo (V5)
    };
    //-----------------------------------------------------------------------------//

    GLuint buffs[2];
    glGenBuffers(2, buffs);
    glBindBuffer(GL_ARRAY_BUFFER, buffs[0]);
    glBufferData(GL_ARRAY_BUFFER, sizeof(v), v, GL_STATIC_DRAW);
    glBindBuffer(GL_ARRAY_BUFFER, buffs[1]);
    glBufferData(GL_ARRAY_BUFFER, sizeof(c), c, GL_STATIC_DRAW);

    //triangulo
    float vertices[] = {
        -0.5f, -0.5f,0.0f, 1.0f, 0.0f,0.0f,0.5f,-0.5f,0.0f,0.0f,1.0f,0.0f,0.0f,0.5f,0.0f,0.0f,0.0f,1.0f
    };

    GLuint vBuff;
    glGenBuffers(1, &vBuff);
    glBindBuffer(GL_ARRAY_BUFFER, vBuff);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    //Rot
    float angle = 0.0f;


    //
    
    // 3. Application y Main Loop (Bucle Principal)
    bool isRunning = true;
    //repetiendo o podemos decir seguir encendido la pantalla hasta de que retornale un false

    //Funcion estrella para guardar las operaciones o tareas que ha enviado por el usuario, como un buzon de correo, cada vez de que el usuario que hacer una accion, inclementa una carta en buzon
    // 
    // Este bucle mantiene el motor corriendo frame a frame
    while (isRunning) {
        SDL_Event event;
        //cada roda = 1 frame
        // Procesar todos los eventos de la ventana (teclado, ratón, cerrar)
        while (SDL_PollEvent(&event)) {
            //un segundo bucle para asegurar de que no pasar al siguiente frame antes de que completar las tareas, pq muchas veces en una frame puede pasar muchas cosas
            //SDL,pollevent, es aquel que se coge las carta para enviarlas, osea completarlas, preguntado al sistema si queda algunas cartas en pediente de hacerla

            // Si el usuario presiona la 'X' de la ventana
            if (event.type == SDL_EVENT_QUIT) {
                //event quit es cerrar el juego
                isRunning = false;
                fmt::print("Cerrando el motor...\n");
            }
            else if (event.type == SDL_EVENT_WINDOW_RESIZED) {
                wWidth = event.window.data1;
                wHeight = event.window.data2;

                glViewport(0, 0, wWidth, wHeight);


            }
        }
        //Rotación//
        angle += 0.01f;
        //matrix de 4x4 dimenciones
        //Al poner 1.0f significa La Matrix Indentidad(situación sin rotación, sin movimiento y sin cambios de tamaño)
        glm::mat4 transform = glm::mat4(1.0f);

        transform = glm::rotate(transform,angle,glm::vec3(0.0f,0.0f,1.0f));

        glMatrixMode(GL_MODELVIEW);
        glLoadMatrixf(glm::value_ptr(transform));



        // 
        //La parte para redenrizar, en nuestro caso usamos openGL
        // Aquí irá en el futuro el código para renderizar gráficos
        // (Por ahora, la ventana se quedará en negro automáticamente)
        // --- NUEVO CÓDIGO DE OPENGL ---

        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        
        //triangulo
        //glBegin(GL_TRIANGLES);
        //glVertex3f(-0.5f, -0.5f, 0.0f);
        //glVertex3fv(v);
        //glColor3f(1.0f, 0.0f, 0.0f);
        //glColor3fv(c);
        //glVertex3f(0.5f, -0.5f, 0.0f);
        //glVertex3f(0.0f, 1.0f, 0.0f);
        //glColor3f(0.0f, 1.0f, 0.0f);
        //glColor3fv(c[3]);
        //glVertex3f(6.0f, 0.0f, 0.0f);
        //glVertex3fv(v[6]);
        //glColor3f(0.0f, 0.0f, 1.0f);
        //glColor3df(ac[0]);
        //glEnd();
        // 

        glEnableClientState(GL_VERTEX_ARRAY);
        glEnableClientState(GL_COLOR_ARRAY);
       
        //triangulo
        /*glVertexPointer(3, GL_FLOAT, 0, v);*/
        /*glColorPointer(3, GL_FLOAT, 0, c);*/
        //
        
        glBindBuffer(GL_ARRAY_BUFFER, buffs[0]);
        glVertexPointer(3, GL_FLOAT, 0, NULL);
        glBindBuffer(GL_ARRAY_BUFFER, buffs[1]);
        glColorPointer(3, GL_FLOAT, 0, NULL);

        //triangulo
        //glBindBuffer(GL_ARRAY_BUFFER, vBuff);
        //glVertexPointer(3, GL_FLOAT, sizeof(float) * 6, NULL);
        //glColorPointer(3, GL_FLOAT, sizeof(float) * 6, reinterpret_cast<void*>(sizeof(float) * 3));
        //glDrawArrays(GL_TRIANGLES, 0, 3);
        //

        glDrawArrays(GL_TRIANGLES, 0, 6);

        SDL_GL_SwapWindow(window);




        //// 1. Elegir el color de fondo (Rojo, Verde, Azul, Transparencia)
        //   glClearColor(0.2f, 0.3f, 0.3f, 1.0f); // Esto dará un tono gris azulado oscuro

        //   // 2. Limpiar la pantalla (Color y Profundidad) usando el color elegido arriba
        //   glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        //   // 3. Actualizar la ventana al final de cada frame para mostrar los cambios
        //   SDL_GL_SwapWindow(window);
    }
    return 0;

}
    //muy importante de que limpiamos el variable puntero del window
    // 4. Limpieza de memoria al salir
//    SDL_GL_DestroyContext(gl_context);
//    SDL_DestroyWindow(window);
//    SDL_Quit();//limpiamos las memorias y apagar los que hemos encendido durante la ejecutacion
//    fmt::print("Motor apagado correctamente.\n");
//
//    return 0;
//}
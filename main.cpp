#include "pcd.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <stdint.h>
#include "SDLStage.h"
#include <SDL.h>
#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>
#include <math.h>

#ifdef EMSCRIPTEN
#include <emscripten.h>
#endif

#include <thread>

static float *rotate_mat = NULL;
volatile pcd_t *pcd = NULL;
volatile bool pcl_changed = false;
std::mutex pcd_mutex;

uint64_t get_tick_count() {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec * 1000 + tv.tv_usec / 1000;
}


GLuint programObject;
SDLStage* stage;

float scale_factor = 0.02;
float xshift = 0;
float yshift = 0;

bool mousebuttondown = false;
bool rightbuttondown = false;
bool ctrl_pressed = false;

static inline void Multiply2X2(float& fOut_11, float& fOut_12, float& fOut_21, float& fOut_22,
                               float f1_11, float f1_12, float f1_21, float f1_22,
                               float f2_11, float f2_12, float f2_21, float f2_22)
{
    const float x1((f1_11 + f1_22) * (f2_11 + f2_22));
    const float x2((f1_21 + f1_22) * f2_11);
    const float x3(f1_11 * (f2_12 - f2_22));
    const float x4(f1_22 * (f2_21 - f2_11));
    const float x5((f1_11 + f1_12) * f2_22);
    const float x6((f1_21 - f1_11) * (f2_11 + f2_12));
    const float x7((f1_12 - f1_22) * (f2_21 + f2_22));

    fOut_11 = x1 + x4 - x5 + x7;
    fOut_12 = x3 + x5;
    fOut_21 = x2 + x4;
    fOut_22 = x1 - x2 + x3 + x6;
}

static void Multiply(float *mOut, const float *m1, const float* m2)
{
    float fTmp[7][4];

    Multiply2X2(fTmp[0][0], fTmp[0][1], fTmp[0][2], fTmp[0][3],
                m1[0] + m1[10], m1[1] + m1[11], m1[4] + m1[14], m1[5] + m1[15],
                m2[0] + m2[10], m2[1] + m2[11], m2[4] + m2[14], m2[5] + m2[15]);

    Multiply2X2(fTmp[1][0], fTmp[1][1], fTmp[1][2], fTmp[1][3],
                m1[8] + m1[10], m1[9] + m1[11], m1[12] + m1[14], m1[13] + m1[15],
                m2[0], m2[1], m2[4], m2[5]);

    Multiply2X2(fTmp[2][0], fTmp[2][1], fTmp[2][2], fTmp[2][3],
                m1[0], m1[1], m1[4], m1[5],
                m2[2] - m2[10], m2[3] - m2[11], m2[6] - m2[14], m2[7] - m2[15]);

    Multiply2X2(fTmp[3][0], fTmp[3][1], fTmp[3][2], fTmp[3][3],
                m1[10], m1[11], m1[14], m1[15],
                m2[8] - m2[0], m2[9] - m2[1], m2[12] - m2[4], m2[13] - m2[5]);

    Multiply2X2(fTmp[4][0], fTmp[4][1], fTmp[4][2], fTmp[4][3],
                m1[0] + m1[2], m1[1] + m1[3], m1[4] + m1[6], m1[5] + m1[7],
                m2[10], m2[11], m2[14], m2[15]);

    Multiply2X2(fTmp[5][0], fTmp[5][1], fTmp[5][2], fTmp[5][3],
                m1[8] - m1[0], m1[9] - m1[1], m1[12] - m1[4], m1[13] - m1[5],
                m2[0] + m2[2], m2[1] + m2[3], m2[4] + m2[6], m2[5] + m2[7]);

    Multiply2X2(fTmp[6][0], fTmp[6][1], fTmp[6][2], fTmp[6][3],
                m1[2] - m1[10], m1[3] - m1[11], m1[6] - m1[14], m1[7] - m1[15],
                m2[8] + m2[10], m2[9] + m2[11], m2[12] + m2[14], m2[13] + m2[15]);

    mOut[0] = fTmp[0][0] + fTmp[3][0] - fTmp[4][0] + fTmp[6][0];
    mOut[1] = fTmp[0][1] + fTmp[3][1] - fTmp[4][1] + fTmp[6][1];
    mOut[4] = fTmp[0][2] + fTmp[3][2] - fTmp[4][2] + fTmp[6][2];
    mOut[5] = fTmp[0][3] + fTmp[3][3] - fTmp[4][3] + fTmp[6][3];

    mOut[2] = fTmp[2][0] + fTmp[4][0];
    mOut[3] = fTmp[2][1] + fTmp[4][1];
    mOut[6] = fTmp[2][2] + fTmp[4][2];
    mOut[7] = fTmp[2][3] + fTmp[4][3];

    mOut[8] = fTmp[1][0] + fTmp[3][0];
    mOut[9] = fTmp[1][1] + fTmp[3][1];
    mOut[12] = fTmp[1][2] + fTmp[3][2];
    mOut[13] = fTmp[1][3] + fTmp[3][3];

    mOut[10] = fTmp[0][0] - fTmp[1][0] + fTmp[2][0] + fTmp[5][0];
    mOut[11] = fTmp[0][1] - fTmp[1][1] + fTmp[2][1] + fTmp[5][1];
    mOut[14] = fTmp[0][2] - fTmp[1][2] + fTmp[2][2] + fTmp[5][2];
    mOut[15] = fTmp[0][3] - fTmp[1][3] + fTmp[2][3] + fTmp[5][3];
}

static void rotateX(float angle) {
    float rotx[4 * 4] = { 1.0, 0.0, 0.0, 0.0,
                          0.0, cos(angle), -sin(angle), 0.0,
                          0.0, sin(angle), cos(angle), 0.0,
                          0.0, 0.0, 0.0, 1.0
                        };
    Multiply(rotate_mat, rotate_mat, rotx);
}

static void rotateY(float angle) {
    float roty[4 * 4] = { cos(angle), 0.0, -sin(angle), 0.0,
                          0.0, 1.0, 0.0, 0.0,
                          sin(angle), 0.0, cos(angle), 0.0,
                          0.0, 0.0, 0.0, 1.0
                        };
    Multiply(rotate_mat, rotate_mat, roty);
}

static void rotateZ(float angle) {
    float rotz[4 * 4] = { cos(angle), -sin(angle), 0.0, 0.0,
                          sin(angle), cos(angle), 0.0, 0.0,
                          0.0, 0.0, 1.0, 0.0,
                          0.0, 0.0, 0.0, 1.0
                        };
    Multiply(rotate_mat, rotate_mat, rotz);
}

static void Zrotate(float angle) {
    float rotz[4 * 4] = { cos(angle), -sin(angle), 0.0, 0.0,
                          sin(angle), cos(angle), 0.0, 0.0,
                          0.0, 0.0, 1.0, 0.0,
                          0.0, 0.0, 0.0, 1.0
                        };
    Multiply(rotate_mat, rotz, rotate_mat);
}

void handleEvent (SDL_Event &event) {
    switch (event.type) {
    case SDL_MOUSEWHEEL:
        if (event.wheel.y < -5) {
            scale_factor += 0.003;
            if (scale_factor > 0.25) scale_factor = 0.25;
        } else if (event.wheel.y > 5) {
            scale_factor -= 0.003;
            if (scale_factor < 0) scale_factor = 0;
        }
        break;

    case SDL_MOUSEMOTION:
        if (mousebuttondown) {
            if (!ctrl_pressed) {
                Zrotate(-event.motion.xrel / 200.0);
                rotateX(event.motion.yrel / 200.0);
            } else {
                xshift += event.motion.xrel / 1000.0;
                yshift -= event.motion.yrel / 1000.0;
            }
        } else if (rightbuttondown) {
            xshift += event.motion.xrel / 1000.0;
            yshift -= event.motion.yrel / 1000.0;
        }
        break;

    case SDL_MOUSEBUTTONDOWN:
        if (event.button.button == 1) {
            mousebuttondown = true;
        } else if (event.button.button == 3) {
            rightbuttondown = true;
        }
        break;

    case SDL_MOUSEBUTTONUP:
        if (event.button.button == 1) {
            mousebuttondown = false;
        } else if (event.button.button == 3) {
            rightbuttondown = false;
        }
        break;

    case SDL_KEYDOWN:
        if (event.key.keysym.sym == SDLK_LCTRL) {
            ctrl_pressed = true;
        }
        break;

    case SDL_KEYUP:
        if (event.key.keysym.sym == SDLK_LCTRL) {
            ctrl_pressed = false;
        }
        break;
    }
}


GLuint loadShader (GLenum type, const char *source) {

    GLuint shader;
    GLint compiled;

    shader = glCreateShader (type);

    if (shader == 0) {

        fprintf(stderr, "Error creating shader\n");
        return 0;

    }

    glShaderSource (shader, 1, &source, NULL);
    glCompileShader (shader);

    glGetShaderiv (shader, GL_COMPILE_STATUS, &compiled);

    if (!compiled) {

        GLint infoLen = 0;
        glGetShaderiv (shader, GL_INFO_LOG_LENGTH, &infoLen);

        if (infoLen > 1) {

            char* infoLog = (char*) malloc (sizeof (char) * infoLen);
            glGetShaderInfoLog (shader, infoLen, NULL, infoLog);
            fprintf(stderr, "Error compiling shader: %s\n", infoLog);
            free (infoLog);

        }

        glDeleteShader (shader);
        return 0;

    }

    return shader;

}

int initOpenGL () {

    const char vertexShaderString[] =
        "attribute vec4 vPosition;"
        "uniform float scale_factor;"
        "uniform mat4 rotate;"
        "uniform float xshift;"
        "uniform float yshift;"
        "varying lowp vec4 f_color;"
        "void main()"
        "{"
        "  gl_PointSize = 1.0; "
        "  mat4 scale, rotx, roty, rotz; "
        "  scale = mat4(scale_factor, 0.0, 0.0, 0.0, "
        "     0.0, scale_factor, 0.0, 0.0, "
        "     0.0, 0.0, -scale_factor, 0.0, "
        "     0.0, 0.0, 0.0, 1.0);"
        "  gl_Position = rotate * (scale * vPosition);"
        "  gl_Position[0] += xshift; gl_Position[1] += yshift; gl_Position[2] = 0.0;"
        "  if (vPosition[2] < -1.2) f_color = vec4 ( 1.0, 0.0, 0.0, 1.0 );"
        "  else if (vPosition[2] < -0.8) f_color = vec4 ( 1.0, 1.0, 0.0, 1.0 );"
        "  else f_color = vec4 ( 0.0, 1.0, 0.0, 1.0 );"
        "}";

    const char fragmentShaderString[] =
        "precision mediump float;"
        "varying lowp vec4 f_color;"
        "void main()"
        "{"
        "  gl_FragColor = f_color;"
        "}";

    GLuint vertexShader;
    GLuint fragmentShader;
    GLint linked;

    vertexShader = loadShader (GL_VERTEX_SHADER, vertexShaderString);
    fragmentShader = loadShader (GL_FRAGMENT_SHADER, fragmentShaderString);

    programObject = glCreateProgram ();

    if (programObject == 0) {
        fprintf(stderr, "Could not create OpenGL program\n");
        return 0;
    }

    glAttachShader (programObject, vertexShader);
    glAttachShader (programObject, fragmentShader);
    glBindAttribLocation (programObject, 0, "vPosition");
    glLinkProgram (programObject);

    glGetProgramiv (programObject, GL_LINK_STATUS, &linked);

    if (!linked) {
        GLint infoLen = 0;

        glGetProgramiv (programObject, GL_INFO_LOG_LENGTH, &infoLen);

        if (infoLen > 1) {
            char* infoLog = (char*) malloc (sizeof (char) * infoLen);
            glGetProgramInfoLog (programObject, infoLen, NULL, infoLog);
            fprintf(stderr, "Error linking program: %s", infoLog);
            free (infoLog);
        }

        glDeleteProgram (programObject);
        return 0;
    }

    glClearColor (0.0f, 0.0f, 0.0f, 1.0f);
    return 1;
}


void render (SDL_Surface *screen) {
    std::lock_guard<std::mutex> _lock(pcd_mutex);
    if (pcd == NULL) {
        return;
    }

    static GLuint vboid = 0;
    if (vboid == 0) {
        glGenBuffers(1, &vboid);
    }

    if (pcl_changed) {
        glBindBuffer(GL_ARRAY_BUFFER, vboid);
        glBufferData(GL_ARRAY_BUFFER, sizeof(float) * 3 * pcd->width *pcd->height, (void *)pcd->points, GL_STATIC_DRAW);
	pcl_changed = false;
    }

    glViewport (0, 0, screen -> w, screen -> h);

    glColorMask (GL_TRUE, GL_TRUE, GL_TRUE, GL_FALSE);
    glClear (GL_COLOR_BUFFER_BIT);
    glUseProgram (programObject);
    glUniform1f(glGetUniformLocation(programObject, "scale_factor"), scale_factor);
    glUniformMatrix4fv(glGetUniformLocation(programObject, "rotate"), 1, GL_FALSE, (GLfloat *)rotate_mat);
    glUniform1f(glGetUniformLocation(programObject, "xshift"), xshift);
    glUniform1f(glGetUniformLocation(programObject, "yshift"), yshift);

    glEnableVertexAttribArray (0);
    glVertexAttribPointer (0, 3, GL_FLOAT, GL_FALSE, sizeof(float) * 3, (const void *)0);

    glDrawArrays (GL_POINTS, 0, pcd->height * pcd->width);

    SDL_GL_SwapBuffers ();
}


void update (int deltaTime) {
}


extern "C" void step () {
    stage -> step ();
}

bool initialized = false;

int initialize() {
    stage = new SDLStage (1000, 1000, 10, SDL_OPENGL);

    if (!initOpenGL ()) {
        fprintf(stderr, "Error initializing OpenGL\n");
        return -1;
    }

    stage->setEventListener (&handleEvent);
    stage->setRenderCallback (&render);
    stage->setUpdateCallback (&update);

    rotate_mat = (float *)malloc(sizeof(float) * 16);
    float id_matrix[4 * 4] = { 1, 0, 0, 0,
                               0, 1, 0, 0,
                               0, 0, 1, 0,
                               0, 0, 0, 1
                             };
    memcpy(rotate_mat, id_matrix, sizeof(id_matrix));
    return 0;
}

extern "C" int draw_pcd (const char *pcd_path) {
    if (!initialized) {
        if (initialize() != 0) {
            fprintf(stderr, "initialize failed\n");
            return -1;
        }
        initialized = true;
    }
    FILE *fp = fopen(pcd_path, "rb");
    if (fp == NULL) {
        fprintf(stderr, "CANNOT OPEN FILE %s\n", pcd_path);
        return -1;
    }

    fseek(fp, 0, SEEK_END);
    int size = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    char *buf = (char *)malloc(size);
    fread(buf, 1, size, fp);
    fclose(fp);

    std::lock_guard<std::mutex> _lock(pcd_mutex);

    if (pcd != NULL) {
        free((void *)pcd);
    }

    pcd = load_pcd_from_buffer(buf);
    free(buf);

    pcl_changed = true;

    return 0;
}

extern "C" int draw_laz (const char *laz_path) {
    if (!initialized) {
        if (initialize() != 0) {
            fprintf(stderr, "initialize failed\n");
            return -1;
        }
        initialized = true;
    }
    std::lock_guard<std::mutex> _lock(pcd_mutex);

    if (pcd != NULL) {
        free((void *)pcd);
    }

    pcd = load_laz_from_file(laz_path);
    pcl_changed = true;

    return 0;
}

extern "C" void set_rotate(float x, float y, float z) {
    float id_matrix[4 * 4] = { 1, 0, 0, 0,
                               0, 1, 0, 0,
                               0, 0, 1, 0,
                               0, 0, 0, 1
                             };
    memcpy(rotate_mat, id_matrix, sizeof(id_matrix));
    rotateX(x);
    rotateY(y);
    rotateZ(z);
}

extern "C" void set_scale(float scale) {
    scale_factor = scale;
}


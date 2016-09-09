#include "pcd.h"
#include <stdio.h>
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

pcd_t *pcd;

uint64_t get_tick_count() {
  struct timeval tv;
  gettimeofday(&tv, NULL);
  return tv.tv_sec * 1000 + tv.tv_usec / 1000;
}


GLuint programObject;
SDLStage* stage;

float scale_factor = 0.05;
float xshift = 0;
float yshift = 0;
float xrotate = 0;
float yrotate = 0;
float zrotate = 0;

bool mousebuttondown = false;
bool rightbuttondown = false;
bool ctrl_pressed = false;

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
	xrotate += event.motion.yrel / 200.0;
	yrotate -= event.motion.xrel / 200.0;
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
		
    cerr << "Error creating shader" << endl;
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
      cerr << "Error compiling shader: " << infoLog << endl;
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
    "uniform float xrotate;"
    "uniform float yrotate;"
    "uniform float zrotate;"
    "uniform float xshift;"
    "uniform float yshift;"
    "void main()"
    "{"
    "  gl_PointSize = 1.0; "
    "  mat4 scale, rotx, roty, rotz; "
    "  scale = mat4(scale_factor, 0.0, 0.0, 0.0, "
    "     0.0, scale_factor, 0.0, 0.0, "
    "     0.0, 0.0, 1.0, 0.0, "
    "     0.0, 0.0, 0.0, 1.0);"
    "  rotx = mat4(1.0, 0.0, 0.0, 0.0, "
    "     0.0, cos(xrotate), -sin(xrotate), 0.0, "
    "     0.0, sin(xrotate), cos(xrotate), 0.0, "
    "     0.0, 0.0, 0.0, 1.0);"
    "  roty = mat4(cos(yrotate), 0.0, -sin(yrotate), 0.0, "
    "     0.0, 1.0, 0.0, 0.0, "
    "     sin(yrotate), 0.0, cos(xrotate), 0.0, "
    "     0.0, 0.0, 0.0, 1.0);"
    "  rotz = mat4(cos(zrotate), -sin(zrotate), 0.0, 0.0, "
    "     sin(zrotate), cos(zrotate), 0.0, 0.0, "
    "     0.0, 0.0, 1.0, 0.0, "
    "     0.0, 0.0, 0.0, 1.0);"
    "  gl_Position = scale * (rotz * (roty * (rotx * vPosition)));"
    "  gl_Position[0] += xshift; gl_Position[1] += yshift; gl_Position[2] = 0.0;"
    "}";

  const char fragmentShaderString[] =  
    "precision mediump float;"
    "void main()"
    "{"
    "  gl_FragColor = vec4 ( 0.0, 1.0, 0.0, 1.0 );"
    "}";

  GLuint vertexShader;
  GLuint fragmentShader;
  GLint linked;
	
  vertexShader = loadShader (GL_VERTEX_SHADER, vertexShaderString);
  fragmentShader = loadShader (GL_FRAGMENT_SHADER, fragmentShaderString);
	
  programObject = glCreateProgram ();
	
  if (programObject == 0) {
    cerr << "Could not create OpenGL program" << endl;
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
      cerr << "Error linking program: " << infoLog << endl;
      free (infoLog);
    }
	
    glDeleteProgram (programObject);
    return 0;
  }
	
  glClearColor (0.0f, 0.0f, 0.0f, 1.0f);
  return 1;
}


void render (SDL_Surface *screen) {
  static GLuint vboid = 0;
  if (vboid == 0) {
    glGenBuffers(1, &vboid);
  }

  glBindBuffer(GL_ARRAY_BUFFER, vboid);
  glBufferData(GL_ARRAY_BUFFER, sizeof(float) * 3 * pcd->width *pcd->height, pcd->points, GL_STATIC_DRAW);
  glViewport (0, 0, screen -> w, screen -> h);

  glColorMask (GL_TRUE, GL_TRUE, GL_TRUE, GL_FALSE);
  glClear (GL_COLOR_BUFFER_BIT);

  glUseProgram (programObject);
  glUniform1f(glGetUniformLocation(programObject, "scale_factor"), scale_factor);
  glUniform1f(glGetUniformLocation(programObject, "xrotate"), xrotate);
  glUniform1f(glGetUniformLocation(programObject, "yrotate"), yrotate);
  glUniform1f(glGetUniformLocation(programObject, "zrotate"), zrotate);
  glUniform1f(glGetUniformLocation(programObject, "xshift"), xshift);
  glUniform1f(glGetUniformLocation(programObject, "yshift"), yshift);

  glEnableVertexAttribArray (0);
  glVertexAttribPointer (0, 3, GL_FLOAT, GL_FALSE, sizeof(float) * 3, (const void *)0);

  glDrawArrays (GL_POINTS, 0, pcd->height * pcd->width);
	
  SDL_GL_SwapBuffers ();
}


void update (int deltaTime) {
}


void step () {
  stage -> step ();
}


int main (int argc, char* argv[]) {
  char *buf;

  FILE *fp=fopen("1.pcd", "rb");
  fseek(fp, 0, SEEK_END);
  int size = ftell(fp);
  fseek(fp, 0, SEEK_SET);
  buf = (char *)malloc(size);
  fread(buf, 1, size, fp);
  fclose(fp);

  // uint64_t start=get_tick_count();
  pcd = load_pcd_from_buffer(buf);
  // uint64_t end=get_tick_count();
  // printf("%lld\n", end - start);

  // printf("%d x %d\n", pcd->width, pcd->height);

   for (int i = 0; i < 100; i++) {
     printf("%f, %f, %f\n", pcd->points[i].x, pcd->points[i].y, pcd->points[i].z);
   }

  stage = new SDLStage (1000, 1000, 10, SDL_OPENGL);

  if (!initOpenGL ()) {
		
    cerr << "Error initializing OpenGL" << endl;
    return 1;
		
  }
	
  stage -> setCaption ("SimpleGL");
  stage -> setEventListener (&handleEvent);
  stage -> setRenderCallback (&render);
  stage -> setUpdateCallback (&update);

#ifdef EMSCRIPTEN

  emscripten_set_main_loop (step, 10, true);

#else
	
  while (stage -> active) {
		
    step ();
		
  }
	
#endif
	
  delete stage;
  return 0;
	
}

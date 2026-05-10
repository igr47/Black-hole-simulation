#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <glm/detail/qualifier.hpp>
#include <glm/ext/vector_float3.hpp>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <iostream>
#include <vector>
#include <cmath> 

// Window settings 
const int SRC_WIDTH = 1280;
const int SCR_HEIGHT = 720;

// Camera 
glm::vec3 cameraPos = glm::vec3(0.0f, 2.0f, 8.0f);
glm::vec3 cameraFront = glm::vec3(0.0f, -0.2f, -0.1f);
glm::vec3 cameraUp = glm::vec3(0.0f, 1.0f, 0.0f);
float 

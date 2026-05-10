#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <iostream>
#include <vector>
#include <cmath>

// Window settings
const int SCR_WIDTH = 1280;
const int SCR_HEIGHT = 720;

// Camera
glm::vec3 cameraPos = glm::vec3(0.0f, 2.0f, 8.0f);
glm::vec3 cameraFront = glm::vec3(0.0f, -0.2f, -1.0f);
glm::vec3 cameraUp = glm::vec3(0.0f, 1.0f, 0.0f);
float yaw = -90.0f, pitch = -10.0f;
float lastX = SCR_WIDTH / 2.0f, lastY = SCR_HEIGHT / 2.0f;
bool firstMouse = true;

// Black hole parameters
float blackHoleMass = 1.0f;
float schwarzschildRadius = 0.5f;
float diskInnerRadius = 1.2f;
float diskOuterRadius = 3.5f;
float timeScale = 1.0f;
float rotationSpeed = 0.5f;

// Grid parameters
const int GRID_SIZE = 100;
const float GRID_EXTENT = 10.0f;

// Shader sources
const char* vertexShaderSource = R"(
#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec2 aTexCoord;
layout (location = 2) in vec3 aNormal;

out vec3 FragPos;
out vec2 TexCoord;
out vec3 Normal;
out float Height;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;
uniform float time;
uniform float blackHoleMass;
uniform vec3 blackHolePos;

void main() {
    vec3 pos = aPos;
    
    // Calculate distance from black hole center (in xz plane)
    vec2 toCenter = pos.xz - blackHolePos.xz;
    float dist = length(toCenter);
    
    // Gravitational warping of spacetime grid (rubber sheet analogy)
    float warp = 0.0f;
    if (dist > 0.3f) {
        warp = -blackHoleMass / (dist + 0.1f);
    } else {
        warp = -blackHoleMass / 0.4f; // Clamp near singularity
    }
    
    // Apply warping to y coordinate
    pos.y += warp;
    Height = warp;
    
    FragPos = vec3(model * vec4(pos, 1.0));
    TexCoord = aTexCoord;
    Normal = mat3(transpose(inverse(model))) * aNormal;
    gl_Position = projection * view * vec4(FragPos, 1.0);
}
)";

const char* gridFragmentShaderSource = R"(
#version 330 core
in vec3 FragPos;
in vec2 TexCoord;
in vec3 Normal;
in float Height;

out vec4 FragColor;

uniform vec3 viewPos;
uniform float time;
uniform vec3 blackHolePos;
uniform float schwarzschildRadius;

// Accretion disk color based on temperature
vec3 diskColor(float dist) {
    float t = clamp((dist - 1.0f) / 3.0f, 0.0f, 1.0f);
    // Hot inner disk (white/blue) to cooler outer (orange/red)
    vec3 hot = vec3(1.0f, 0.9f, 0.7f);
    vec3 mid = vec3(1.0f, 0.5f, 0.1f);
    vec3 cool = vec3(0.8f, 0.1f, 0.05f);
    
    if (t < 0.3f) return mix(hot, mid, t / 0.3f);
    else return mix(mid, cool, (t - 0.3f) / 0.7f);
}

void main() {
    vec3 viewDir = normalize(viewPos - FragPos);
    vec3 normal = normalize(Normal);
    
    // Grid line pattern
    float gridScale = 2.0f;
    vec2 gridUV = TexCoord * gridScale;
    vec2 gridFract = fract(gridUV);
    
    float lineWidth = 0.03f;
    float line = 0.0f;
    if (gridFract.x < lineWidth || gridFract.y < lineWidth || 
        gridFract.x > 1.0f - lineWidth || gridFract.y > 1.0f - lineWidth) {
        line = 1.0f;
    }
    
    // Distance from black hole center
    float dist2D = length(FragPos.xz - blackHolePos.xz);
    
    // Event horizon (black circle)
    if (dist2D < schwarzschildRadius * 1.05f) {
        FragColor = vec4(0.0f, 0.0f, 0.0f, 1.0f);
        return;
    }
    
    // Base grid color (deep space blue/purple)
    vec3 baseColor = vec3(0.05f, 0.05f, 0.15f);
    vec3 gridColor = vec3(0.3f, 0.3f, 0.6f);
    
    // Gravitational lensing glow near black hole
    float lensing = exp(-dist2D * 0.5f) * 0.5f;
    vec3 lensColor = vec3(0.4f, 0.2f, 0.8f) * lensing;
    
    // Accretion disk visualization on grid
    float diskGlow = 0.0f;
    vec3 diskCol = vec3(0.0f);
    if (dist2D > diskInnerRadius && dist2D < diskOuterRadius) {
        float diskIntensity = 1.0f - abs(dist2D - (diskInnerRadius + diskOuterRadius) * 0.5f) 
                              / ((diskOuterRadius - diskInnerRadius) * 0.5f);
        diskIntensity = pow(diskIntensity, 2.0f);
        diskCol = diskColor(dist2D) * diskIntensity * 0.8f;
        diskGlow = diskIntensity * 0.3f;
    }
    
    // Combine colors
    vec3 finalColor = baseColor;
    if (line > 0.5f) {
        finalColor = mix(baseColor, gridColor, 0.7f);
    }
    
    finalColor += lensColor + diskCol;
    
    // Height-based shading for 3D effect
    float heightShade = 1.0f + Height * 0.3f;
    finalColor *= heightShade;
    
    // Fresnel effect on grid warping
    float fresnel = pow(1.0f - abs(dot(viewDir, normal)), 2.0f);
    finalColor += vec3(0.2f, 0.1f, 0.4f) * fresnel * 0.3f;
    
    // Photon ring (bright ring just outside event horizon)
    float photonRing = exp(-pow(dist2D - schwarzschildRadius * 1.5f, 2.0f) * 20.0f);
    finalColor += vec3(1.0f, 0.8f, 0.4f) * photonRing * 0.5f;
    
    FragColor = vec4(finalColor, 1.0f);
}
)";

const char* diskVertexShaderSource = R"(
#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec2 aTexCoord;

out vec2 TexCoord;
out vec3 WorldPos;
out float Angle;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;
uniform float time;
uniform float rotationSpeed;

void main() {
    vec3 pos = aPos;
    
    // Rotate disk vertices
    float angle = time * rotationSpeed;
    float c = cos(angle);
    float s = sin(angle);
    float x = pos.x * c - pos.z * s;
    float z = pos.x * s + pos.z * c;
    pos.x = x;
    pos.z = z;
    
    Angle = atan(pos.z, pos.x);
    
    WorldPos = vec3(model * vec4(pos, 1.0));
    TexCoord = aTexCoord;
    gl_Position = projection * view * vec4(WorldPos, 1.0);
}
)";

const char* diskFragmentShaderSource = R"(
#version 330 core
in vec2 TexCoord;
in vec3 WorldPos;
in float Angle;

out vec4 FragColor;

uniform vec3 viewPos;
uniform float time;
uniform vec3 blackHolePos;
uniform float schwarzschildRadius;
uniform float diskInnerRadius;
uniform float diskOuterRadius;

// Turbulence function for disk texture
float hash(vec2 p) {
    return fract(sin(dot(p, vec2(127.1f, 311.7f))) * 43758.5453f);
}

float noise(vec2 p) {
    vec2 i = floor(p);
    vec2 f = fract(p);
    f = f * f * (3.0f - 2.0f * f);
    float a = hash(i);
    float b = hash(i + vec2(1.0f, 0.0f));
    float c = hash(i + vec2(0.0f, 1.0f));
    float d = hash(i + vec2(1.0f, 1.0f));
    return mix(mix(a, b, f.x), mix(c, d, f.x), f.y);
}

float fbm(vec2 p) {
    float value = 0.0f;
    float amplitude = 0.5f;
    for (int i = 0; i < 5; i++) {
        value += amplitude * noise(p);
        p *= 2.0f;
        amplitude *= 0.5f;
    }
    return value;
}

vec3 temperatureColor(float temp) {
    // Blackbody-like color
    if (temp > 0.9f) return vec3(1.0f, 1.0f, 1.0f); // White hot
    if (temp > 0.7f) return mix(vec3(1.0f, 0.8f, 0.4f), vec3(1.0f, 1.0f, 1.0f), (temp - 0.7f) / 0.2f);
    if (temp > 0.4f) return mix(vec3(1.0f, 0.4f, 0.05f), vec3(1.0f, 0.8f, 0.4f), (temp - 0.4f) / 0.3f);
    if (temp > 0.2f) return mix(vec3(0.8f, 0.1f, 0.02f), vec3(1.0f, 0.4f, 0.05f), (temp - 0.2f) / 0.2f);
    return mix(vec3(0.3f, 0.0f, 0.0f), vec3(0.8f, 0.1f, 0.02f), temp / 0.2f);
}

void main() {
    vec3 viewDir = normalize(viewPos - WorldPos);
    
    // Distance from black hole center
    float dist = length(WorldPos.xz - blackHolePos.xz);
    
    // Radial coordinate normalized
    float r = (dist - diskInnerRadius) / (diskOuterRadius - diskInnerRadius);
    
    // Discard if outside disk
    if (dist < diskInnerRadius || dist > diskOuterRadius) {
        discard;
    }
    
    // Disk thickness falloff
    float height = abs(WorldPos.y);
    float thickness = 0.15f * (1.0f - r * 0.5f); // Thicker in middle
    float heightFade = exp(-height * height / (thickness * thickness));
    
    // Temperature profile (hotter inside)
    float temp = (1.0f - r);
    temp += fbm(vec2(dist * 3.0f - time * 0.5f, Angle * 2.0f)) * 0.2f; // Turbulence
    temp = clamp(temp, 0.0f, 1.0f);
    
    // Doppler beaming effect (brighter when moving toward camera)
    float doppler = 1.0f + sin(Angle + time * rotationSpeed) * 0.3f * r;
    
    vec3 color = temperatureColor(temp);
    float alpha = heightFade * (0.3f + temp * 0.7f) * doppler;
    
    // Gravitational redshift near horizon
    float redshift = 1.0f - exp(-(dist - schwarzschildRadius) * 2.0f);
    color *= redshift;
    
    // Bloom/glow
    float glow = exp(-dist * 0.3f) * 0.2f;
    color += vec3(1.0f, 0.6f, 0.2f) * glow;
    
    FragColor = vec4(color * alpha * 2.0f, alpha);
}
)";

const char* particleVertexShaderSource = R"(
#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in float aLife;

out float Life;

uniform mat4 view;
uniform mat4 projection;
uniform float time;

void main() {
    Life = aLife;
    gl_Position = projection * view * vec4(aPos, 1.0);
    gl_PointSize = 3.0f * aLife;
}
)";

const char* particleFragmentShaderSource = R"(
#version 330 core
in float Life;

out vec4 FragColor;

void main() {
    // Circular particle
    vec2 coord = gl_PointCoord - vec2(0.5f);
    float dist = length(coord);
    if (dist > 0.5f) discard;
    
    // Glow falloff
    float glow = 1.0f - dist * 2.0f;
    glow = pow(glow, 1.5f);
    
    vec3 color = mix(vec3(1.0f, 0.8f, 0.3f), vec3(1.0f, 0.2f, 0.05f), 1.0f - Life);
    FragColor = vec4(color * glow * Life, glow * Life);
}
)";

// Shader compilation helper
unsigned int compileShader(unsigned int type, const char* source) {
    unsigned int id = glCreateShader(type);
    glShaderSource(id, 1, &source, nullptr);
    glCompileShader(id);
    
    int success;
    glGetShaderiv(id, GL_COMPILE_STATUS, &success);
    if (!success) {
        char infoLog[512];
        glGetShaderInfoLog(id, 512, nullptr, infoLog);
        std::cerr << "Shader compilation error:\n" << infoLog << std::endl;
    }
    return id;
}

unsigned int createShaderProgram(const char* vertexSource, const char* fragmentSource) {
    unsigned int program = glCreateProgram();
    unsigned int vs = compileShader(GL_VERTEX_SHADER, vertexSource);
    unsigned int fs = compileShader(GL_FRAGMENT_SHADER, fragmentSource);
    
    glAttachShader(program, vs);
    glAttachShader(program, fs);
    glLinkProgram(program);
    
    int success;
    glGetProgramiv(program, GL_LINK_STATUS, &success);
    if (!success) {
        char infoLog[512];
        glGetProgramInfoLog(program, 512, nullptr, infoLog);
        std::cerr << "Program linking error:\n" << infoLog << std::endl;
    }
    
    glDeleteShader(vs);
    glDeleteShader(fs);
    return program;
}

// Generate spacetime grid
void generateGrid(std::vector<float>& vertices, std::vector<unsigned int>& indices) {
    vertices.clear();
    indices.clear();
    
    for (int i = 0; i <= GRID_SIZE; i++) {
        for (int j = 0; j <= GRID_SIZE; j++) {
            float x = (i / (float)GRID_SIZE - 0.5f) * GRID_EXTENT * 2.0f;
            float z = (j / (float)GRID_SIZE - 0.5f) * GRID_EXTENT * 2.0f;
            float y = 0.0f;
            
            // Position
            vertices.push_back(x);
            vertices.push_back(y);
            vertices.push_back(z);
            
            // TexCoord
            vertices.push_back(i / (float)GRID_SIZE);
            vertices.push_back(j / (float)GRID_SIZE);
            
            // Normal (will be recalculated, but start with up)
            vertices.push_back(0.0f);
            vertices.push_back(1.0f);
            vertices.push_back(0.0f);
        }
    }
    
    for (int i = 0; i < GRID_SIZE; i++) {
        for (int j = 0; j < GRID_SIZE; j++) {
            unsigned int topLeft = i * (GRID_SIZE + 1) + j;
            unsigned int topRight = topLeft + 1;
            unsigned int bottomLeft = (i + 1) * (GRID_SIZE + 1) + j;
            unsigned int bottomRight = bottomLeft + 1;
            
            indices.push_back(topLeft);
            indices.push_back(bottomLeft);
            indices.push_back(topRight);
            
            indices.push_back(topRight);
            indices.push_back(bottomLeft);
            indices.push_back(bottomRight);
        }
    }
}

// Generate accretion disk
void generateDisk(std::vector<float>& vertices, std::vector<unsigned int>& indices, 
                  int segments = 128, int rings = 32) {
    vertices.clear();
    indices.clear();
    
    for (int r = 0; r <= rings; r++) {
        float radius = diskInnerRadius + (r / (float)rings) * (diskOuterRadius - diskInnerRadius);
        for (int s = 0; s <= segments; s++) {
            float angle = (s / (float)segments) * 2.0f * M_PI;
            float x = cos(angle) * radius;
            float z = sin(angle) * radius;
            float y = 0.0f;
            
            // Add some height variation for 3D effect
            float height = 0.05f * sin(angle * 3.0f) * (1.0f - r / (float)rings);
            y += height;
            
            vertices.push_back(x);
            vertices.push_back(y);
            vertices.push_back(z);
            
            vertices.push_back(r / (float)rings);
            vertices.push_back(s / (float)segments);
        }
    }
    
    for (int r = 0; r < rings; r++) {
        for (int s = 0; s < segments; s++) {
            unsigned int topLeft = r * (segments + 1) + s;
            unsigned int topRight = topLeft + 1;
            unsigned int bottomLeft = (r + 1) * (segments + 1) + s;
            unsigned int bottomRight = bottomLeft + 1;
            
            indices.push_back(topLeft);
            indices.push_back(bottomLeft);
            indices.push_back(topRight);
            
            indices.push_back(topRight);
            indices.push_back(bottomLeft);
            indices.push_back(bottomRight);
        }
    }
}

// Particle system for accretion disk matter
struct Particle {
    glm::vec3 pos;
    glm::vec3 vel;
    float life;
    float maxLife;
};

std::vector<Particle> particles;
const int MAX_PARTICLES = 2000;

void initParticles() {
    particles.reserve(MAX_PARTICLES);
    for (int i = 0; i < MAX_PARTICLES; i++) {
        Particle p;
        float angle = ((float)rand() / RAND_MAX) * 2.0f * M_PI;
        float radius = diskInnerRadius + ((float)rand() / RAND_MAX) * (diskOuterRadius - diskInnerRadius);
        p.pos = glm::vec3(cos(angle) * radius, 
                         ((float)rand() / RAND_MAX - 0.5f) * 0.1f, 
                         sin(angle) * radius);
        float speed = 1.0f / sqrt(radius); // Keplerian velocity
        p.vel = glm::vec3(-sin(angle) * speed, 0.0f, cos(angle) * speed);
        p.life = (float)rand() / RAND_MAX;
        p.maxLife = 2.0f + (float)rand() / RAND_MAX * 3.0f;
        particles.push_back(p);
    }
}

void updateParticles(float dt) {
    for (auto& p : particles) {
        p.life -= dt;
        if (p.life <= 0) {
            // Respawn
            float angle = ((float)rand() / RAND_MAX) * 2.0f * M_PI;
            float radius = diskOuterRadius - 0.2f;
            p.pos = glm::vec3(cos(angle) * radius, 
                             ((float)rand() / RAND_MAX - 0.5f) * 0.05f, 
                             sin(angle) * radius);
            float speed = 1.0f / sqrt(radius);
            p.vel = glm::vec3(-sin(angle) * speed, 0.0f, cos(angle) * speed);
            p.life = p.maxLife;
        }
        
        // Spiral inward
        float dist = sqrt(p.pos.x * p.pos.x + p.pos.z * p.pos.z);
        float angle = atan2(p.pos.z, p.pos.x);
        float speed = 1.0f / sqrt(dist);
        p.vel.x = -sin(angle) * speed;
        p.vel.z = cos(angle) * speed;
        
        // Apply velocity
        p.pos += p.vel * dt * 0.5f;
        
        // Spiral in slowly
        float spiralFactor = 1.0f - dt * 0.1f;
        p.pos.x *= spiralFactor;
        p.pos.z *= spiralFactor;
        
        // Accretion disk height damping
        p.pos.y *= 0.95f;
    }
}

// Input callbacks
void framebuffer_size_callback(GLFWwindow* window, int width, int height) {
    glViewport(0, 0, width, height);
}

void mouse_callback(GLFWwindow* window, double xpos, double ypos) {
    if (firstMouse) {
        lastX = xpos;
        lastY = ypos;
        firstMouse = false;
    }
    
    float xoffset = xpos - lastX;
    float yoffset = lastY - ypos;
    lastX = xpos;
    lastY = ypos;
    
    float sensitivity = 0.1f;
    xoffset *= sensitivity;
    yoffset *= sensitivity;
    
    yaw += xoffset;
    pitch += yoffset;
    
    if (pitch > 89.0f) pitch = 89.0f;
    if (pitch < -89.0f) pitch = -89.0f;
    
    glm::vec3 front;
    front.x = cos(glm::radians(yaw)) * cos(glm::radians(pitch));
    front.y = sin(glm::radians(pitch));
    front.z = sin(glm::radians(yaw)) * cos(glm::radians(pitch));
    cameraFront = glm::normalize(front);
}

void processInput(GLFWwindow* window, float deltaTime) {
    float speed = 2.5f * deltaTime;
    
    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
        cameraPos += speed * cameraFront;
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
        cameraPos -= speed * cameraFront;
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
        cameraPos -= glm::normalize(glm::cross(cameraFront, cameraUp)) * speed;
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
        cameraPos += glm::normalize(glm::cross(cameraFront, cameraUp)) * speed;
    if (glfwGetKey(window, GLFW_KEY_Q) == GLFW_PRESS)
        cameraPos.y += speed;
    if (glfwGetKey(window, GLFW_KEY_E) == GLFW_PRESS)
        cameraPos.y -= speed;
    
    // Adjust black hole mass
    if (glfwGetKey(window, GLFW_KEY_UP) == GLFW_PRESS)
        blackHoleMass += deltaTime * 0.5f;
    if (glfwGetKey(window, GLFW_KEY_DOWN) == GLFW_PRESS)
        blackHoleMass = std::max(0.1f, blackHoleMass - deltaTime * 0.5f);
    
    // Adjust rotation speed
    if (glfwGetKey(window, GLFW_KEY_RIGHT) == GLFW_PRESS)
        rotationSpeed += deltaTime;
    if (glfwGetKey(window, GLFW_KEY_LEFT) == GLFW_PRESS)
        rotationSpeed = std::max(0.0f, rotationSpeed - deltaTime);
    
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        glfwSetWindowShouldClose(window, true);
}

int main() {
    // Initialize GLFW
    if (!glfwInit()) {
        std::cerr << "Failed to initialize GLFW" << std::endl;
        return -1;
    }
    
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    
    GLFWwindow* window = glfwCreateWindow(SCR_WIDTH, SCR_HEIGHT, 
                                          "Black Hole Simulation - C++ OpenGL", 
                                          nullptr, nullptr);
    if (!window) {
        std::cerr << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return -1;
    }
    
    glfwMakeContextCurrent(window);
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
    glfwSetCursorPosCallback(window, mouse_callback);
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
    
    // Initialize GLEW
    if (glewInit() != GLEW_OK) {
        std::cerr << "Failed to initialize GLEW" << std::endl;
        return -1;
    }
    
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glPointSize(2.0f);
    
    // Create shader programs
    unsigned int gridShader = createShaderProgram(vertexShaderSource, gridFragmentShaderSource);
    unsigned int diskShader = createShaderProgram(diskVertexShaderSource, diskFragmentShaderSource);
    unsigned int particleShader = createShaderProgram(particleVertexShaderSource, particleFragmentShaderSource);
    
    // Generate grid
    std::vector<float> gridVertices;
    std::vector<unsigned int> gridIndices;
    generateGrid(gridVertices, gridIndices);
    
    unsigned int gridVAO, gridVBO, gridEBO;
    glGenVertexArrays(1, &gridVAO);
    glGenBuffers(1, &gridVBO);
    glGenBuffers(1, &gridEBO);
    
    glBindVertexArray(gridVAO);
    glBindBuffer(GL_ARRAY_BUFFER, gridVBO);
    glBufferData(GL_ARRAY_BUFFER, gridVertices.size() * sizeof(float), 
                 gridVertices.data(), GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, gridEBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, gridIndices.size() * sizeof(unsigned int), 
                 gridIndices.data(), GL_STATIC_DRAW);
    
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(5 * sizeof(float)));
    glEnableVertexAttribArray(2);
    
    // Generate disk
    std::vector<float> diskVertices;
    std::vector<unsigned int> diskIndices;
    generateDisk(diskVertices, diskIndices);
    
    unsigned int diskVAO, diskVBO, diskEBO;
    glGenVertexArrays(1, &diskVAO);
    glGenBuffers(1, &diskVBO);
    glGenBuffers(1, &diskEBO);
    
    glBindVertexArray(diskVAO);
    glBindBuffer(GL_ARRAY_BUFFER, diskVBO);
    glBufferData(GL_ARRAY_BUFFER, diskVertices.size() * sizeof(float), 
                 diskVertices.data(), GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, diskEBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, diskIndices.size() * sizeof(unsigned int), 
                 diskIndices.data(), GL_STATIC_DRAW);
    
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);
    
    // Initialize particles
    initParticles();
    
    unsigned int particleVAO, particleVBO;
    glGenVertexArrays(1, &particleVAO);
    glGenBuffers(1, &particleVBO);
    
    // Main loop
    float deltaTime = 0.0f;
    float lastFrame = 0.0f;
    
    while (!glfwWindowShouldClose(window)) {
        float currentFrame = glfwGetTime();
        deltaTime = currentFrame - lastFrame;
        lastFrame = currentFrame;
        
        processInput(window, deltaTime);
        updateParticles(deltaTime);
        
        // Clear
        glClearColor(0.0f, 0.0f, 0.02f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        
        // Matrices
        glm::mat4 projection = glm::perspective(glm::radians(45.0f), 
                                                (float)SCR_WIDTH / SCR_HEIGHT, 
                                                0.1f, 100.0f);
        glm::mat4 view = glm::lookAt(cameraPos, cameraPos + cameraFront, cameraUp);
        glm::mat4 model = glm::mat4(1.0f);
        
        // Update schwarzschild radius based on mass
        schwarzschildRadius = blackHoleMass * 0.5f;
        
        // --- Render Grid (Spacetime) ---
        glUseProgram(gridShader);
        glUniformMatrix4fv(glGetUniformLocation(gridShader, "model"), 1, GL_FALSE, 
                          glm::value_ptr(model));
        glUniformMatrix4fv(glGetUniformLocation(gridShader, "view"), 1, GL_FALSE, 
                          glm::value_ptr(view));
        glUniformMatrix4fv(glGetUniformLocation(gridShader, "projection"), 1, GL_FALSE, 
                          glm::value_ptr(projection));
        glUniform1f(glGetUniformLocation(gridShader, "time"), currentFrame);
        glUniform1f(glGetUniformLocation(gridShader, "blackHoleMass"), blackHoleMass);
        glUniform3f(glGetUniformLocation(gridShader, "blackHolePos"), 0.0f, 0.0f, 0.0f);
        glUniform3f(glGetUniformLocation(gridShader, "viewPos"), 
                   cameraPos.x, cameraPos.y, cameraPos.z);
        glUniform1f(glGetUniformLocation(gridShader, "schwarzschildRadius"), schwarzschildRadius);
        
        glBindVertexArray(gridVAO);
        glDrawElements(GL_TRIANGLES, gridIndices.size(), GL_UNSIGNED_INT, 0);
        
        // --- Render Accretion Disk ---
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE); // Additive blending for glow
        
        glUseProgram(diskShader);
        glUniformMatrix4fv(glGetUniformLocation(diskShader, "model"), 1, GL_FALSE, 
                          glm::value_ptr(model));
        glUniformMatrix4fv(glGetUniformLocation(diskShader, "view"), 1, GL_FALSE, 
                          glm::value_ptr(view));
        glUniformMatrix4fv(glGetUniformLocation(diskShader, "projection"), 1, GL_FALSE, 
                          glm::value_ptr(projection));
        glUniform1f(glGetUniformLocation(diskShader, "time"), currentFrame);
        glUniform1f(glGetUniformLocation(diskShader, "rotationSpeed"), rotationSpeed);
        glUniform3f(glGetUniformLocation(diskShader, "viewPos"), 
                   cameraPos.x, cameraPos.y, cameraPos.z);
        glUniform3f(glGetUniformLocation(diskShader, "blackHolePos"), 0.0f, 0.0f, 0.0f);
        glUniform1f(glGetUniformLocation(diskShader, "schwarzschildRadius"), schwarzschildRadius);
        glUniform1f(glGetUniformLocation(diskShader, "diskInnerRadius"), diskInnerRadius);
        glUniform1f(glGetUniformLocation(diskShader, "diskOuterRadius"), diskOuterRadius);
        
        glBindVertexArray(diskVAO);
        glDrawElements(GL_TRIANGLES, diskIndices.size(), GL_UNSIGNED_INT, 0);
        
        // --- Render Particles ---
        std::vector<float> particleData;
        for (const auto& p : particles) {
            particleData.push_back(p.pos.x);
            particleData.push_back(p.pos.y);
            particleData.push_back(p.pos.z);
            particleData.push_back(p.life / p.maxLife);
        }
        
        glBindVertexArray(particleVAO);
        glBindBuffer(GL_ARRAY_BUFFER, particleVBO);
        glBufferData(GL_ARRAY_BUFFER, particleData.size() * sizeof(float), 
                    particleData.data(), GL_STREAM_DRAW);
        
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(1, 1, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(3 * sizeof(float)));
        glEnableVertexAttribArray(1);
        
        glUseProgram(particleShader);
        glUniformMatrix4fv(glGetUniformLocation(particleShader, "view"), 1, GL_FALSE, 
                          glm::value_ptr(view));
        glUniformMatrix4fv(glGetUniformLocation(particleShader, "projection"), 1, GL_FALSE, 
                          glm::value_ptr(projection));
        glUniform1f(glGetUniformLocation(particleShader, "time"), currentFrame);
        
        glDrawArrays(GL_POINTS, 0, particles.size());
        
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA); // Reset blend mode
        
        // Swap buffers
        glfwSwapBuffers(window);
        glfwPollEvents();
    }
    
    // Cleanup
    glDeleteVertexArrays(1, &gridVAO);
    glDeleteBuffers(1, &gridVBO);
    glDeleteBuffers(1, &gridEBO);
    glDeleteVertexArrays(1, &diskVAO);
    glDeleteBuffers(1, &diskVBO);
    glDeleteBuffers(1, &diskEBO);
    glDeleteVertexArrays(1, &particleVAO);
    glDeleteBuffers(1, &particleVBO);
    glDeleteProgram(gridShader);
    glDeleteProgram(diskShader);
    glDeleteProgram(particleShader);
    
    glfwTerminate();
    return 0;
}

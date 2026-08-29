// Gargantua-like Black Hole Simulation - Fixed Version
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <iostream>
#include <vector>
#include <cmath>
#include <random>

// Window settings
const int SCR_WIDTH = 1600;
const int SCR_HEIGHT = 900;

// Camera
glm::vec3 cameraPos = glm::vec3(0.0f, 3.0f, 10.0f);
glm::vec3 cameraFront = glm::vec3(0.0f, -0.2f, -1.0f);
glm::vec3 cameraUp = glm::vec3(0.0f, 1.0f, 0.0f);
float yaw = -90.0f, pitch = -10.0f;
float lastX = SCR_WIDTH / 2.0f, lastY = SCR_HEIGHT / 2.0f;
bool firstMouse = true;

// Black hole parameters
float blackHoleMass = 2.0f;
float schwarzschildRadius = 1.0f;
float diskInnerRadius = 1.5f;
float diskOuterRadius = 5.0f;
float diskTilt = 0.5f;
float rotationSpeed = 0.5f;

// Fullscreen quad shader for ray marching
const char* screenQuadVertexShader = R"(
#version 330 core
layout (location = 0) in vec2 aPos;
layout (location = 1) in vec2 aTexCoords;

out vec2 TexCoords;

void main() {
    TexCoords = aTexCoords;
    gl_Position = vec4(aPos.x, aPos.y, 0.0, 1.0);
}
)";

const char* blackholeShader = R"(
#version 330 core
out vec4 FragColor;
in vec2 TexCoords;

uniform vec3 viewPos;
uniform vec3 viewDir;
uniform vec3 cameraRight;
uniform vec3 cameraUp;
uniform float time;
uniform float schwarzschildRadius;
uniform float diskInnerRadius;
uniform float diskOuterRadius;
uniform float rotationSpeed;
uniform float blackHoleMass;

const int MAX_STEPS = 150;
const float MAX_DIST = 150.0;
const float STEP_SIZE = 0.15;

// Hash function for noise
float hash(vec2 p) {
    return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453);
}

// Value noise
float noise(vec2 p) {
    vec2 i = floor(p);
    vec2 f = fract(p);
    f = f * f * (3.0 - 2.0 * f);
    
    float a = hash(i);
    float b = hash(i + vec2(1.0, 0.0));
    float c = hash(i + vec2(0.0, 1.0));
    float d = hash(i + vec2(1.0, 1.0));
    
    return mix(mix(a, b, f.x), mix(c, d, f.x), f.y);
}

// Fractal Brownian Motion for more detail
float fbm(vec2 p) {
    float value = 0.0;
    float amplitude = 0.5;
    float frequency = 1.0;
    
    for (int i = 0; i < 4; i++) {
        value += amplitude * noise(p * frequency);
        frequency *= 2.0;
        amplitude *= 0.5;
    }
    return value;
}

// Temperature to color mapping
vec3 temperatureColor(float t) {
    t = clamp(t, 0.0, 1.0);
    
    vec3 color;
    if (t < 0.3) {
        color = mix(vec3(0.8, 0.1, 0.02), vec3(1.0, 0.4, 0.05), t / 0.3);
    } else if (t < 0.6) {
        color = mix(vec3(1.0, 0.4, 0.05), vec3(1.0, 0.8, 0.3), (t - 0.3) / 0.3);
    } else if (t < 0.85) {
        color = mix(vec3(1.0, 0.8, 0.3), vec3(1.0, 0.95, 0.7), (t - 0.6) / 0.25);
    } else {
        color = mix(vec3(1.0, 0.95, 0.7), vec3(1.0, 1.0, 0.95), (t - 0.85) / 0.15);
    }
    return color;
}

// Simple gravitational deflection
vec3 deflectRay(vec3 rayPos, vec3 rayDir) {
    vec3 toBH = -rayPos; // Black hole at origin
    float dist = length(toBH);
    
    if (dist < schwarzschildRadius * 1.1) {
        return rayDir; // Too close, just continue
    }
    
    vec3 dirToBH = normalize(toBH);
    float deflectionStrength = blackHoleMass * 0.5 / (dist * dist);
    deflectionStrength = min(deflectionStrength, 5.0);
    
    return normalize(rayDir + dirToBH * deflectionStrength * STEP_SIZE);
}

void main() {
    // Calculate ray direction for this pixel (like a camera)
    vec2 uv = TexCoords * 2.0 - 1.0;
    float aspectRatio = 16.0 / 9.0; // Match your window aspect ratio
    
    vec3 rayOrigin = viewPos;
    vec3 rayDir = normalize(viewDir + uv.x * cameraRight * aspectRatio + uv.y * cameraUp);
    
    vec3 pos = rayOrigin;
    vec3 dir = rayDir;
    
    vec4 accumulatedColor = vec4(0.0, 0.0, 0.0, 0.0);
    float transmittance = 1.0;
    bool hitEventHorizon = false;
    
    for (int i = 0; i < MAX_STEPS; i++) {
        float distToBH = length(pos);
        
        // Check if we hit the event horizon
        if (distToBH < schwarzschildRadius * 0.95) {
            hitEventHorizon = true;
            break;
        }
        
        // Check if we're near the photon ring
        float photonDist = abs(distToBH - schwarzschildRadius * 1.5);
        if (photonDist < 0.1) {
            float ringIntensity = exp(-photonDist * 30.0) * 0.5;
            accumulatedColor.rgb += transmittance * vec3(1.0, 0.9, 0.6) * ringIntensity;
        }
        
        // Check if we're in the disk
        float distXZ = length(pos.xz);
        if (distXZ > diskInnerRadius - 0.2 && distXZ < diskOuterRadius + 0.2) {
            // Check vertical position (disk thickness)
            float thickness = 0.1 + (distXZ - diskInnerRadius) / (diskOuterRadius - diskInnerRadius) * 0.2;
            float verticalFade = exp(-abs(pos.y) / thickness);
            
            if (verticalFade > 0.01 && distToBH > schwarzschildRadius * 1.2) {
                // Calculate disk properties
                float normalizedRadius = (distXZ - diskInnerRadius) / (diskOuterRadius - diskInnerRadius);
                float temperature = 1.0 - normalizedRadius * 0.7;
                
                // Add turbulence
                float azimuth = atan(pos.z, pos.x);
                float turb = fbm(vec2(distXZ * 2.0 + time * 0.1, azimuth * 3.0)) * 0.2;
                temperature += turb;
                temperature = clamp(temperature, 0.0, 1.0);
                
                // Doppler effect
                float dopplerFactor = 1.0 + sin(azimuth + time * rotationSpeed * 0.5) * 0.3;
                
                // Emission and absorption
                vec3 emission = temperatureColor(temperature) * verticalFade * dopplerFactor * 3.0;
                float absorption = verticalFade * 1.5;
                
                // Accumulate
                accumulatedColor.rgb += transmittance * emission * STEP_SIZE;
                accumulatedColor.a += transmittance * absorption * STEP_SIZE;
                transmittance *= exp(-absorption * STEP_SIZE);
                
                if (transmittance < 0.01) break;
            }
        }
        
        // Apply gravitational lensing
        dir = deflectRay(pos, dir);
        
        // March forward
        pos += dir * STEP_SIZE;
        
        if (length(pos) > MAX_DIST) break;
    }
    
    // Dark center for event horizon
    if (hitEventHorizon) {
        accumulatedColor = mix(accumulatedColor, vec4(0.0, 0.0, 0.0, 1.0), 0.9);
    }
    
    // Add background stars (very subtle)
    float starNoise = fbm(pos.xz * 0.01 + pos.y * 0.05) * 0.05;
    accumulatedColor.rgb += starNoise * transmittance * vec3(0.5, 0.7, 1.0);
    
    // HDR-like bloom
    float brightness = dot(accumulatedColor.rgb, vec3(0.299, 0.587, 0.114));
    if (brightness > 0.5) {
        accumulatedColor.rgb += (brightness - 0.5) * vec3(1.0, 0.7, 0.3) * 0.5;
    }
    
    // Ensure output alpha is 1.0 for proper display
    FragColor = vec4(accumulatedColor.rgb, 1.0);
}
)";

// Shader compilation helpers
unsigned int compileShader(unsigned int type, const char* source) {
    unsigned int id = glCreateShader(type);
    glShaderSource(id, 1, &source, nullptr);
    glCompileShader(id);
    
    int success;
    glGetShaderiv(id, GL_COMPILE_STATUS, &success);
    if (!success) {
        char infoLog[1024];
        glGetShaderInfoLog(id, 1024, nullptr, infoLog);
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
        char infoLog[1024];
        glGetProgramInfoLog(program, 1024, nullptr, infoLog);
        std::cerr << "Program linking error:\n" << infoLog << std::endl;
    }
    
    glDeleteShader(vs);
    glDeleteShader(fs);
    return program;
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
    float speed = 5.0f * deltaTime;
    
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
    
    if (glfwGetKey(window, GLFW_KEY_UP) == GLFW_PRESS)
        blackHoleMass = std::min(5.0f, blackHoleMass + deltaTime * 0.5f);
    if (glfwGetKey(window, GLFW_KEY_DOWN) == GLFW_PRESS)
        blackHoleMass = std::max(0.5f, blackHoleMass - deltaTime * 0.5f);
    if (glfwGetKey(window, GLFW_KEY_RIGHT) == GLFW_PRESS)
        rotationSpeed = std::min(2.0f, rotationSpeed + deltaTime * 0.2f);
    if (glfwGetKey(window, GLFW_KEY_LEFT) == GLFW_PRESS)
        rotationSpeed = std::max(0.1f, rotationSpeed - deltaTime * 0.2f);
    
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        glfwSetWindowShouldClose(window, true);
    
    // Update derived parameters
    schwarzschildRadius = blackHoleMass * 0.5f;
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
                                          "Gargantua Black Hole", 
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
    
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        std::cerr << "Failed to initialize GLAD" << std::endl;
        return -1;
    }
    
    std::cout << "OpenGL Version: " << glGetString(GL_VERSION) << std::endl;
    std::cout << "\nGargantua Black Hole Simulation" << std::endl;
    std::cout << "Controls: WASD/QE to move, Mouse to look, Arrows to adjust" << std::endl;
    
    // Create shader program
    unsigned int blackholeProgram = createShaderProgram(screenQuadVertexShader, blackholeShader);
    
    // Create fullscreen quad
    float quadVertices[] = {
        // positions   // texCoords
        -1.0f,  1.0f,  0.0f, 1.0f,
        -1.0f, -1.0f,  0.0f, 0.0f,
         1.0f, -1.0f,  1.0f, 0.0f,
        -1.0f,  1.0f,  0.0f, 1.0f,
         1.0f, -1.0f,  1.0f, 0.0f,
         1.0f,  1.0f,  1.0f, 1.0f
    };
    
    unsigned int quadVAO, quadVBO;
    glGenVertexArrays(1, &quadVAO);
    glGenBuffers(1, &quadVBO);
    glBindVertexArray(quadVAO);
    glBindBuffer(GL_ARRAY_BUFFER, quadVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), quadVertices, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
    
    // Main loop
    float deltaTime = 0.0f;
    float lastFrame = 0.0f;
    
    while (!glfwWindowShouldClose(window)) {
        float currentFrame = glfwGetTime();
        deltaTime = currentFrame - lastFrame;
        lastFrame = currentFrame;
        
        processInput(window, deltaTime);
        
        // Clear screen
        glClearColor(0.0f, 0.0f, 0.01f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        
        // Calculate camera vectors for ray direction
        glm::vec3 cameraRight = glm::normalize(glm::cross(cameraFront, cameraUp));
        glm::vec3 actualCameraUp = glm::normalize(glm::cross(cameraRight, cameraFront));
        
        // Render black hole using fullscreen quad
        glUseProgram(blackholeProgram);
        glUniform3f(glGetUniformLocation(blackholeProgram, "viewPos"), 
                   cameraPos.x, cameraPos.y, cameraPos.z);
        glUniform3f(glGetUniformLocation(blackholeProgram, "viewDir"), 
                   cameraFront.x, cameraFront.y, cameraFront.z);
        glUniform3f(glGetUniformLocation(blackholeProgram, "cameraRight"), 
                   cameraRight.x, cameraRight.y, cameraRight.z);
        glUniform3f(glGetUniformLocation(blackholeProgram, "cameraUp"), 
                   actualCameraUp.x, actualCameraUp.y, actualCameraUp.z);
        glUniform1f(glGetUniformLocation(blackholeProgram, "time"), currentFrame);
        glUniform1f(glGetUniformLocation(blackholeProgram, "schwarzschildRadius"), schwarzschildRadius);
        glUniform1f(glGetUniformLocation(blackholeProgram, "diskInnerRadius"), diskInnerRadius);
        glUniform1f(glGetUniformLocation(blackholeProgram, "diskOuterRadius"), diskOuterRadius);
        glUniform1f(glGetUniformLocation(blackholeProgram, "rotationSpeed"), rotationSpeed);
        glUniform1f(glGetUniformLocation(blackholeProgram, "blackHoleMass"), blackHoleMass);
        
        glBindVertexArray(quadVAO);
        glDrawArrays(GL_TRIANGLES, 0, 6);
        
        // Swap buffers
        glfwSwapBuffers(window);
        glfwPollEvents();
    }
    
    // Cleanup
    glDeleteVertexArrays(1, &quadVAO);
    glDeleteBuffers(1, &quadVBO);
    glDeleteProgram(blackholeProgram);
    
    glfwTerminate();
    return 0;
}

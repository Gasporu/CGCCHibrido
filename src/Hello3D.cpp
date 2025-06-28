#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <string>
#include <map>
#include <filesystem>

#include "json.hpp"
#include <glad/glad.h>
#include <GLFW/glfw3.h>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

using namespace std;
using namespace glm;
using json = nlohmann::json;

struct Model {
    GLuint vaoId;
    int vertexCount;
};

struct SceneObject {
    string modelPath;
    Model model;
    GLuint textureId;
    vec3 position;
    vec3 scale;
    vec3 rotation;
    vector<vec3> trajectoryPoints;
    size_t currentTargetIndex;
    float moveSpeed;
};

const GLuint WIDTH = 800, HEIGHT = 600;
GLFWwindow* window;

class Camera {
public:
    vec3 position;
    float yaw;
    float pitch;
    Camera(vec3 startPosition = vec3(0.0f, 0.0f, 3.0f), float startYaw = -90.0f, float startPitch = 0.0f)
        : position(startPosition), yaw(startYaw), pitch(startPitch) {}
    mat4 getViewMatrix() {
        vec3 front;
        front.x = cos(radians(yaw)) * cos(radians(pitch));
        front.y = sin(radians(pitch));
        front.z = sin(radians(yaw)) * cos(radians(pitch));
        front = normalize(front);
        vec3 right = normalize(cross(front, vec3(0.0f, 1.0f, 0.0f)));
        vec3 up = normalize(cross(right, front));
        return lookAt(position, position + front, up);
    }
    void moveForward(float delta) { position += delta * getFrontVector(); }
    void moveRight(float delta) { position += delta * getRightVector(); }
    void moveUp(float delta) { position.y += delta; }
    void rotate(float deltaYaw, float deltaPitch) {
        yaw += deltaYaw;
        pitch += deltaPitch;
        if (pitch > 89.0f) pitch = 89.0f;
        if (pitch < -89.0f) pitch = -89.0f;
    }
private:
    vec3 getFrontVector() {
        vec3 front;
        front.x = cos(radians(yaw)) * cos(radians(pitch));
        front.y = sin(radians(pitch));
        front.z = sin(radians(yaw)) * cos(radians(pitch));
        return normalize(front);
    }
    vec3 getRightVector() {
        vec3 front = getFrontVector();
        return normalize(cross(front, vec3(0.0f, 1.0f, 0.0f)));
    }
};

void key_callback(GLFWwindow* window, int key, int scancode, int action, int mode);
Model loadModel(const string& objPath);
GLuint loadTexture(const string& filePath);
void loadTrajectoryPoints(vector<vec3>& points, const string& filename);

const char* vertexShaderSource = R"(#version 400 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec2 aTexCoord;
layout (location = 2) in vec3 aNormal;
out vec2 TexCoord;
out vec3 FragPos;
out vec3 Normal;
uniform mat4 projection;
uniform mat4 view;
uniform mat4 model;
void main(){
    gl_Position = projection * view * model * vec4(aPos, 1.0);
    FragPos = vec3(model * vec4(aPos, 1.0));
    Normal = mat3(transpose(inverse(model))) * aNormal;
    TexCoord = aTexCoord;
})";

const char* fragmentShaderSource = R"(#version 400 core
in vec2 TexCoord;
in vec3 FragPos;
in vec3 Normal;
out vec4 FragColor;
uniform sampler2D texture1;
uniform vec3 lightPos;
uniform vec3 viewPos;
uniform vec3 lightColor;
void main(){
    vec3 ambientColor = 0.2 * lightColor;
    vec3 norm = normalize(Normal);
    vec3 lightDir = normalize(lightPos - FragPos);
    float diff = max(dot(norm, lightDir), 0.0);
    vec3 diffuseColor = diff * lightColor;
    float shininess = 32.0;
    vec3 viewDir = normalize(viewPos - FragPos);
    vec3 reflectDir = reflect(-lightDir, norm);
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), shininess);
    vec3 specularColor = spec * lightColor;
    vec3 phong = (ambientColor + diffuseColor + specularColor);
    vec4 texColor = texture(texture1, TexCoord);
    FragColor = vec4(phong, 1.0) * texColor;
})";

Camera camera;
vector<SceneObject> sceneObjects;
map<string, Model> loadedModels;
map<string, GLuint> loadedTextures;

int main() {
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    window = glfwCreateWindow(WIDTH, HEIGHT, "Visualizador de Modelos Multiplos", nullptr, nullptr);
    glfwMakeContextCurrent(window);
    glfwSetKeyCallback(window, key_callback);

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        cout << "Failed to initialize GLAD" << endl;
        return -1;
    }

    glEnable(GL_DEPTH_TEST);

    GLuint shaderProgram = 0;
    {
        GLuint vertexShader = glCreateShader(GL_VERTEX_SHADER);
        glShaderSource(vertexShader, 1, &vertexShaderSource, nullptr);
        glCompileShader(vertexShader);
        GLuint fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
        glShaderSource(fragmentShader, 1, &fragmentShaderSource, nullptr);
        glCompileShader(fragmentShader);
        shaderProgram = glCreateProgram();
        glAttachShader(shaderProgram, vertexShader);
        glAttachShader(shaderProgram, fragmentShader);
        glLinkProgram(shaderProgram);
        glDeleteShader(vertexShader);
        glDeleteShader(fragmentShader);
    }
    
    try {
        ifstream f("config.json");
        json config = json::parse(f);

        for (const auto& item : config["scene_objects"]) {
            SceneObject obj;
            obj.modelPath = item["model_path"];
            string texturePath = item["texture_path"];
            string trajectoryPath = item["trajectory_path"];

            if (loadedModels.find(obj.modelPath) == loadedModels.end()) {
                loadedModels[obj.modelPath] = loadModel(obj.modelPath);
            }
            obj.model = loadedModels[obj.modelPath];

            if (loadedTextures.find(texturePath) == loadedTextures.end()) {
                loadedTextures[texturePath] = loadTexture(texturePath);
            }
            obj.textureId = loadedTextures[texturePath];
            
            if (!trajectoryPath.empty()) {
                loadTrajectoryPoints(obj.trajectoryPoints, trajectoryPath);
            }

            if (item.contains("initial_position")) {
                obj.position = vec3(item["initial_position"][0], item["initial_position"][1], item["initial_position"][2]);
            } else if (!obj.trajectoryPoints.empty()) {
                obj.position = obj.trajectoryPoints[0];
            } else {
                obj.position = vec3(0.0f);
            }

            obj.scale = vec3(item["initial_scale"][0], item["initial_scale"][1], item["initial_scale"][2]);
            obj.rotation = vec3(0.0f);
            obj.currentTargetIndex = 0;
            obj.moveSpeed = 1.0f;

            sceneObjects.push_back(obj);
        }
    } catch (const exception& e) {
        cout << "ERRO ao carregar config.json ou seus assets: " << e.what() << endl;
        return -1;
    }

    mat4 projection = perspective(radians(45.0f), (float)WIDTH / (float)HEIGHT, 0.1f, 100.0f);
    vec3 lightPos = vec3(0.0f, 10.0f, 10.0f);
    vec3 lightColor = vec3(1.0f);
    float lastFrameTime = (float)glfwGetTime();

    while (!glfwWindowShouldClose(window)) {
        float currentFrameTime = (float)glfwGetTime();
        float deltaTime = currentFrameTime - lastFrameTime;
        lastFrameTime = currentFrameTime;

        glfwPollEvents();
        glClearColor(0.05f, 0.05f, 0.05f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glUseProgram(shaderProgram);
        glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "projection"), 1, GL_FALSE, value_ptr(projection));
        mat4 view = camera.getViewMatrix();
        glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "view"), 1, GL_FALSE, value_ptr(view));
        glUniform3fv(glGetUniformLocation(shaderProgram, "lightPos"), 1, value_ptr(lightPos));
        glUniform3fv(glGetUniformLocation(shaderProgram, "viewPos"), 1, value_ptr(camera.position));
        glUniform3fv(glGetUniformLocation(shaderProgram, "lightColor"), 1, value_ptr(lightColor));

        for (auto& obj : sceneObjects) {
            if (obj.trajectoryPoints.size() > 1) {
                vec3 targetPos = obj.trajectoryPoints[obj.currentTargetIndex];
                vec3 direction = normalize(targetPos - obj.position);
                float distance = length(targetPos - obj.position);
                if (distance < 0.1f) {
                    obj.currentTargetIndex = (obj.currentTargetIndex + 1) % obj.trajectoryPoints.size();
                } else {
                    obj.position += direction * obj.moveSpeed * deltaTime;
                }
            }

            mat4 modelMatrix = mat4(1.0f);
            modelMatrix = translate(modelMatrix, obj.position);
            modelMatrix = scale(modelMatrix, obj.scale);
            glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "model"), 1, GL_FALSE, value_ptr(modelMatrix));
            
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, obj.textureId);
            glUniform1i(glGetUniformLocation(shaderProgram, "texture1"), 0);

            glBindVertexArray(obj.model.vaoId);
            glDrawArrays(GL_TRIANGLES, 0, obj.model.vertexCount);
        }

        glfwSwapBuffers(window);
    }

    glfwTerminate();
    return 0;
}

Model loadModel(const string& objPath) {
    vector<vec3> tempPositions;
    vector<vec2> tempTexCoords;
    vector<vec3> tempNormals;
    vector<float> vertexData;
    ifstream file(objPath);
    if (!file.is_open()) {
        throw runtime_error("Nao foi possivel abrir OBJ: " + objPath);
    }
    string line;
    while (getline(file, line)) {
        stringstream ss(line);
        string prefix;
        ss >> prefix;
        if (prefix == "v") {
            vec3 pos;
            ss >> pos.x >> pos.y >> pos.z;
            tempPositions.push_back(pos);
        } else if (prefix == "vt") {
            vec2 tex;
            ss >> tex.x >> tex.y;
            tempTexCoords.push_back(tex);
        } else if (prefix == "vn") {
            vec3 norm;
            ss >> norm.x >> norm.y >> norm.z;
            tempNormals.push_back(norm);
        } else if (prefix == "f") {
            for (int i = 0; i < 3; i++) {
                string face_data;
                ss >> face_data;
                stringstream face_ss(face_data);
                string v_str, vt_str, vn_str;
                getline(face_ss, v_str, '/');
                getline(face_ss, vt_str, '/');
                getline(face_ss, vn_str, '/');
                int v_idx = stoi(v_str) - 1;
                int vt_idx = stoi(vt_str) - 1;
                int vn_idx = stoi(vn_str) - 1;
                vertexData.push_back(tempPositions[v_idx].x);
                vertexData.push_back(tempPositions[v_idx].y);
                vertexData.push_back(tempPositions[v_idx].z);
                vertexData.push_back(tempTexCoords[vt_idx].x);
                vertexData.push_back(1.0f - tempTexCoords[vt_idx].y);
                vertexData.push_back(tempNormals[vn_idx].x);
                vertexData.push_back(tempNormals[vn_idx].y);
                vertexData.push_back(tempNormals[vn_idx].z);
            }
        }
    }
    GLuint VAO, VBO;
    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);
    glBindVertexArray(VAO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, vertexData.size() * sizeof(float), vertexData.data(), GL_STATIC_DRAW);
    int stride = 8 * sizeof(float);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, stride, (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, stride, (void*)(5 * sizeof(float)));
    glEnableVertexAttribArray(2);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
    Model model;
    model.vaoId = VAO;
    model.vertexCount = vertexData.size() / 8;
    return model;
}

GLuint loadTexture(const string& filePath) {
    int width, height, nrChannels;
    stbi_set_flip_vertically_on_load(false);
    unsigned char* data = stbi_load(filePath.c_str(), &width, &height, &nrChannels, 0);
    if (!data) {
        throw runtime_error("Falha ao carregar imagem: " + filePath);
    }
    GLuint texID;
    glGenTextures(1, &texID);
    glBindTexture(GL_TEXTURE_2D, texID);
    GLenum format = (nrChannels == 4) ? GL_RGBA : GL_RGB;
    glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, data);
    glGenerateMipmap(GL_TEXTURE_2D);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    stbi_image_free(data);
    return texID;
}

void loadTrajectoryPoints(vector<vec3>& points, const string& filename) {
    points.clear();
    ifstream inFile(filename);

    // Linha de debug para sabermos que a função foi chamada
    cout << "Tentando carregar trajetoria do arquivo: " << filename << endl;

    if (!inFile.is_open()) {
        // Linha de debug se o arquivo não for encontrado
        cout << "  -> ERRO: Nao foi possivel abrir o arquivo de trajetoria." << endl;
        return;
    }
    
    vec3 p;
    while (inFile >> p.x >> p.y >> p.z) {
        points.push_back(p);
    }
    inFile.close();

    // Linha de debug para sabermos quantos pontos foram carregados
    cout << "  -> SUCESSO: Carregado " << points.size() << " pontos." << endl;
}

void key_callback(GLFWwindow* window, int key, int scancode, int action, int mode) {
    const float camMoveSpeed = 2.5f;
    const float camRotateSpeed = 2.0f;
    if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
        glfwSetWindowShouldClose(window, true);
    float deltaTime = 0.1f;
    if (key == GLFW_KEY_W) camera.moveForward(camMoveSpeed * deltaTime);
    if (key == GLFW_KEY_S) camera.moveForward(-camMoveSpeed * deltaTime);
    if (key == GLFW_KEY_A) camera.moveRight(-camMoveSpeed * deltaTime);
    if (key == GLFW_KEY_D) camera.moveRight(camMoveSpeed * deltaTime);
    if (key == GLFW_KEY_Q) camera.moveUp(camMoveSpeed * deltaTime);
    if (key == GLFW_KEY_E) camera.moveUp(-camMoveSpeed * deltaTime);
    if (key == GLFW_KEY_LEFT) camera.rotate(-camRotateSpeed, 0.0f);
    if (key == GLFW_KEY_RIGHT) camera.rotate(camRotateSpeed, 0.0f);
    if (key == GLFW_KEY_UP) camera.rotate(0.0f, camRotateSpeed);
    if (key == GLFW_KEY_DOWN) camera.rotate(0.0f, -camRotateSpeed);
}
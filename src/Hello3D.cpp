/* Hello Textured OBJ Viewer com modelo Phong
 * Versão com instâncias, interação por teclado e iluminação Phong
 * Adaptado por ChatGPT (junho/2025)
 */

#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <string>

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

using namespace std;
using namespace glm;

// --- Variáveis globais ---
const GLuint WIDTH = 800, HEIGHT = 600;
GLFWwindow *window;

vector<vec3> positions;
vector<vec2> texCoords;

GLuint textureID;

// Variáveis para interação
vec3 cubePosition = vec3(0.0f, 0.0f, -5.0f);
vec3 cubeScale = vec3(1.0f);
float cubeRotationX = 0.0f;
float cubeRotationY = 0.0f;
float cubeRotationZ = 0.0f;

// --- Vertex Shader (com Normais e iluminação Phong) ---
const char *vertexShaderSource = R"(
#version 400 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec2 aTexCoord;
layout (location = 2) in vec3 aNormal;

out vec2 TexCoord;
out vec3 FragPos;
out vec3 Normal;

uniform mat4 projection;
uniform mat4 model;

void main()
{
    gl_Position = projection * model * vec4(aPos, 1.0);
    FragPos = vec3(model * vec4(aPos, 1.0));
    Normal = mat3(transpose(inverse(model))) * aNormal;
    TexCoord = aTexCoord;
}
)";

// --- Fragment Shader com iluminação Phong + textura ---
const char *fragmentShaderSource = R"(
#version 400 core
in vec2 TexCoord;
in vec3 FragPos;
in vec3 Normal;

out vec4 FragColor;

uniform sampler2D texture1;
uniform vec3 lightPos;
uniform vec3 viewPos;
uniform vec3 lightColor;
uniform vec3 objectColor;

void main()
{
    // propriedades do material
    vec3 ambientColor = 0.2 * lightColor;
    
    // Difuso
    vec3 norm = normalize(Normal);
    vec3 lightDir = normalize(lightPos - FragPos);
    float diff = max(dot(norm, lightDir), 0.0);
    vec3 diffuseColor = diff * lightColor;

    // Especular
    float shininess = 32.0;
    vec3 viewDir = normalize(viewPos - FragPos);
    vec3 reflectDir = reflect(-lightDir, norm);
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), shininess);
    vec3 specularColor = spec * lightColor;

    // Combinar componentes
    vec3 phong = (ambientColor + diffuseColor + specularColor);

    vec4 texColor = texture(texture1, TexCoord);
    FragColor = vec4(phong, 1.0) * texColor;
}
)";

// --- Funções ---
void key_callback(GLFWwindow *window, int key, int scancode, int action, int mode);
GLuint setupShader();
GLuint setupGeometry();
bool loadOBJ(const string &objPath, const string &mtlPath, string &textureFileOut);
GLuint loadTexture(const string &filePath);
void drawCube(GLuint shaderProgram, GLuint VAO, vec3 position, vec3 scale, vec3 rotation);

int main()
{
    std::string objPath = "C:/Users/Pedro/Desktop/atividade_cg/CGCCHibrido/assets/Modelos3D/Cube.obj";
    std::string mtlPath = "C:/Users/Pedro/Desktop/atividade_cg/CGCCHibrido/assets/Modelos3D/Cube.mtl";
    std::string texturePath = "C:/Users/Pedro/Desktop/atividade_cg/CGCCHibrido/assets/tex/pixelWall.png";


    std::string textureFile;

    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    window = glfwCreateWindow(WIDTH, HEIGHT, "Cubo Texturizado - Pedro de Gasperi", nullptr, nullptr);
    glfwMakeContextCurrent(window);
    glfwSetKeyCallback(window, key_callback);

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
    {
        cout << "Failed to initialize GLAD" << endl;
        return -1;
    }

    glViewport(0, 0, WIDTH, HEIGHT);
    glEnable(GL_DEPTH_TEST);

    GLuint shaderProgram = setupShader();

    if (!loadOBJ(objPath, mtlPath, textureFile))
    {
        cout << "Erro ao carregar Cube.obj" << endl;
        return -1;
    }

    textureID = loadTexture(texturePath);
    if (textureID == 0)
    {
        cout << "Erro ao carregar textura: " << texturePath << endl;
        return -1;
    }

    GLuint VAO = setupGeometry();

    mat4 projection = perspective(radians(45.0f), (float)WIDTH / (float)HEIGHT, 0.1f, 100.0f);

    vec3 lightPos = vec3(3.0f, 3.0f, 3.0f);
    vec3 lightColor = vec3(1.0f);
    vec3 objectColor = vec3(1.0f);

    while (!glfwWindowShouldClose(window))
    {
        glfwPollEvents();

        glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glUseProgram(shaderProgram);
        glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "projection"), 1, GL_FALSE, value_ptr(projection));
        glUniform3fv(glGetUniformLocation(shaderProgram, "lightPos"), 1, value_ptr(lightPos));
        glUniform3fv(glGetUniformLocation(shaderProgram, "viewPos"), 1, value_ptr(vec3(0.0f, 0.0f, 0.0f)));
        glUniform3fv(glGetUniformLocation(shaderProgram, "lightColor"), 1, value_ptr(lightColor));
        glUniform3fv(glGetUniformLocation(shaderProgram, "objectColor"), 1, value_ptr(objectColor));

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, textureID);
        glUniform1i(glGetUniformLocation(shaderProgram, "texture1"), 0);

        vec3 baseRotation = vec3(cubeRotationX, cubeRotationY, cubeRotationZ);
        vec3 baseScale = cubeScale;
        vec3 basePosition = cubePosition;

        drawCube(shaderProgram, VAO, basePosition + vec3(0.0f, 0.0f, 0.0f), baseScale, baseRotation);
        drawCube(shaderProgram, VAO, basePosition + vec3(4.0f, 0.0f, 0.0f), baseScale, baseRotation);
        drawCube(shaderProgram, VAO, basePosition + vec3(-4.0f, 0.0f, 0.0f), baseScale, baseRotation);

        glfwSwapBuffers(window);
    }

    glfwTerminate();
    return 0;
}

GLuint setupShader()
{
    GLuint vertexShader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertexShader, 1, &vertexShaderSource, nullptr);
    glCompileShader(vertexShader);

    GLint success;
    GLchar infoLog[512];
    glGetShaderiv(vertexShader, GL_COMPILE_STATUS, &success);
    if (!success)
    {
        glGetShaderInfoLog(vertexShader, 512, nullptr, infoLog);
        cout << "Vertex Shader Error:\n" << infoLog << endl;
    }

    GLuint fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragmentShader, 1, &fragmentShaderSource, nullptr);
    glCompileShader(fragmentShader);
    glGetShaderiv(fragmentShader, GL_COMPILE_STATUS, &success);
    if (!success)
    {
        glGetShaderInfoLog(fragmentShader, 512, nullptr, infoLog);
        cout << "Fragment Shader Error:\n" << infoLog << endl;
    }

    GLuint shaderProgram = glCreateProgram();
    glAttachShader(shaderProgram, vertexShader);
    glAttachShader(shaderProgram, fragmentShader);
    glLinkProgram(shaderProgram);
    glGetProgramiv(shaderProgram, GL_LINK_STATUS, &success);
    if (!success)
    {
        glGetProgramInfoLog(shaderProgram, 512, nullptr, infoLog);
        cout << "Shader Linking Error:\n" << infoLog << endl;
    }

    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);

    return shaderProgram;
}

GLuint setupGeometry()
{
    GLuint VAO, VBO;
    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);

    struct Vertex
    {
        vec3 pos;
        vec2 tex;
        vec3 normal;
    };

    vector<Vertex> vertices;
    for (size_t i = 0; i < positions.size(); i++)
    {
        Vertex v;
        v.pos = positions[i];
        v.tex = texCoords[i];
        v.normal = vec3(0.0f, 0.0f, 1.0f); // normal default — (opcional: calcular normal real!)
        vertices.push_back(v);
    }

    glBindVertexArray(VAO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(Vertex), vertices.data(), GL_STATIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (GLvoid *)0);
    glEnableVertexAttribArray(0);

    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (GLvoid *)offsetof(Vertex, tex));
    glEnableVertexAttribArray(1);

    glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (GLvoid *)offsetof(Vertex, normal));
    glEnableVertexAttribArray(2);

    glBindVertexArray(0);

    return VAO;
}

bool loadOBJ(const string &objPath, const string &mtlPath, string &textureFileOut)
{
    ifstream objFile(objPath);
    if (!objFile.is_open())
    {
        cout << "Não foi possível abrir OBJ: " << objPath << endl;
        return false;
    }

    vector<vec3> tempPositions;
    vector<vec2> tempTexCoords;

    string line;
    while (getline(objFile, line))
    {
        istringstream iss(line);
        string prefix;
        iss >> prefix;

        if (prefix == "v")
        {
            vec3 pos;
            iss >> pos.x >> pos.y >> pos.z;
            tempPositions.push_back(pos);
        }
        else if (prefix == "vt")
        {
            vec2 tex;
            iss >> tex.x >> tex.y;
            tex.y = 1.0f - tex.y;
            tempTexCoords.push_back(tex);
        }
        else if (prefix == "f")
        {
            for (int i = 0; i < 3; i++)
            {
                string v;
                iss >> v;

                size_t pos1 = v.find('/');
                size_t pos2 = v.find('/', pos1 + 1);

                int vi = stoi(v.substr(0, pos1)) - 1;
                int ti = stoi(v.substr(pos1 + 1, pos2 - pos1 - 1)) - 1;

                positions.push_back(tempPositions[vi]);
                texCoords.push_back(tempTexCoords[ti]);
            }
        }
    }

    ifstream mtlFile(mtlPath);
    if (!mtlFile.is_open())
    {
        cout << "Não foi possível abrir MTL: " << mtlPath << endl;
        return false;
    }

    while (getline(mtlFile, line))
    {
        istringstream iss(line);
        string prefix;
        iss >> prefix;

        if (prefix == "map_Kd")
        {
            string texFile;
            iss >> texFile;

            string mtlDir = mtlPath.substr(0, mtlPath.find_last_of("/\\"));
            textureFileOut = mtlDir + "/" + texFile;
            break;
        }
    }

    cout << "OBJ carregado com " << positions.size() << " vértices.\n";
    cout << "Textura: " << textureFileOut << endl;
    return true;
}

GLuint loadTexture(const string &filePath)
{
    int width, height, nrChannels;
    stbi_set_flip_vertically_on_load(true);
    unsigned char *data = stbi_load(filePath.c_str(), &width, &height, &nrChannels, 0);
    if (!data)
    {
        cout << "Falha ao carregar imagem: " << filePath << endl;
        return 0;
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

void drawCube(GLuint shaderProgram, GLuint VAO, vec3 position, vec3 scaleVec, vec3 rotation)
{
    mat4 model = translate(mat4(1.0f), position);
    model = rotate(model, radians(rotation.x), vec3(1.0f, 0.0f, 0.0f));
    model = rotate(model, radians(rotation.y), vec3(0.0f, 1.0f, 0.0f));
    model = rotate(model, radians(rotation.z), vec3(0.0f, 0.0f, 1.0f));
    model = glm::scale(model, scaleVec);

    glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "model"), 1, GL_FALSE, value_ptr(model));

    glBindVertexArray(VAO);
    glDrawArrays(GL_TRIANGLES, 0, positions.size());
    glBindVertexArray(0);
}

void key_callback(GLFWwindow *window, int key, int scancode, int action, int mode)
{
    const float moveSpeed = 0.1f;
    const float scaleSpeed = 0.05f;
    const float rotationSpeed = 5.0f;

    if (action == GLFW_PRESS || action == GLFW_REPEAT)
    {
        if (key == GLFW_KEY_ESCAPE)
            glfwSetWindowShouldClose(window, true);

        if (key == GLFW_KEY_W)
            cubePosition.y += moveSpeed;
        if (key == GLFW_KEY_S)
            cubePosition.y -= moveSpeed;
        if (key == GLFW_KEY_A)
            cubePosition.x -= moveSpeed;
        if (key == GLFW_KEY_D)
            cubePosition.x += moveSpeed;

        if (key == GLFW_KEY_I)
            cubePosition.z += moveSpeed;
        if (key == GLFW_KEY_J)
            cubePosition.z -= moveSpeed;

        if (key == GLFW_KEY_LEFT_BRACKET)
            cubeScale *= (1.0f - scaleSpeed);
        if (key == GLFW_KEY_RIGHT_BRACKET)
            cubeScale *= (1.0f + scaleSpeed);

        if (key == GLFW_KEY_X)
            cubeRotationX += rotationSpeed;
        if (key == GLFW_KEY_Y)
            cubeRotationY += rotationSpeed;
        if (key == GLFW_KEY_Z)
            cubeRotationZ += rotationSpeed;
    }
}
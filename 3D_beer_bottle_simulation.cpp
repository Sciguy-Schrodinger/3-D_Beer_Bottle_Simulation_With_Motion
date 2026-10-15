#include <stdio.h>
#include <stdlib.h>
#include <iostream>
#include <cmath>
#include <vector>
#include <cstdlib>
#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#define STB_IMAGE_IMPLEMENTATION
#include <stb/stb_image.h>

using namespace std;
using namespace glm;

float random_number(float min, float max){
return min + static_cast <float> (rand()) /( static_cast <float> (RAND_MAX/(max-min)));
}

int width = 2000;
int height = 2000;
float aspect = (float)width/(float)height;

unsigned int bottleIndexCount = 0;
unsigned int liquidIndexCount = 0;

float x = -1.81f;
float y = -1.0f;
float w = 3.6f;
float h = 1.1f;
float z = 0.0f;
float u = 0.0f;
float du = 1.0f;
float v = 0.0f;
float dv = 1.0f;

float dx = 0.0f;
float dy = 0.75f;
float dz = 0.0f;

float bottomY = -0.75f;
float topY = -0.3f;
float radius = 0.2f;
float topTaperStart = 0.69f;
float waveAmplitude = 0.01f;
float waveSpeed = 4.0f;

float liquidRadius = 0.2f;
float bubble_y_init_low = -0.8f;
float bubble_y_init_high = -0.5f;
float top_of_bottle = -0.325f;

bool move_bottle = false;
bool throw_bottle = false;
bool can_throw = false;
bool broken = false;
bool shards_spawned = false;

float hitx = 0.0f;
float sensitivity = 0.005;

float delta_x;
float delta_y;
float vel_x = 0.0f;
float vel_y = 0.0f;
float throw_scale = 0.005f;
float g = 0.01f;
float Floor = -2.0f;

float scale_factor = 0.7f;
float rotate_factor = (float)glfwGetTime();

unsigned int shaderProgram, colourShaderProgram, pointShaderProgram, VAO_bottle, VBO_bottle, EBO_bottle, VAO_liquid, VBO_liquid, EBO_liquid, VAO_bubbles, VBO_bubbles, VAO_shards, VBO_shards;

struct Bubble{
  float x;
  float y;
  float z;
  float speed;
  float size;
  float phase;
};

vector<Bubble> bubbles;

struct Bottle_shard{
  float local_cx;
  float local_cy;
  float x;
  float y;
  float z;
  float vx;
  float vy;
  float vz;
  float theta;
  float omega;
};

vector<Bottle_shard> Bottle_shards;

vector<float> shard_vertices;

int cols = 10;
int rows = 10;

float BX_MIN = -1.81f;
float BX_MAX = 1.79f;
float BY_MIN = -3.0f;
float BY_MAX = -1.9f;

// Add this function to create a point shader for bubbles
unsigned int createPointShader(){
    const char* vertexShaderSource = R"(
        #version 330 core
        layout (location = 0) in vec3 pos;
        layout (location = 1) in float size;

        uniform mat4 model;
        uniform mat4 view;
        uniform mat4 projection;

        void main(){
            gl_Position = projection * view * model * vec4(pos, 1.0);
            gl_PointSize = 200 * size / gl_Position.w; // Scale factor for visibility
        } 
    )";

    const char* fragmentShaderSource = R"(
        #version 330 core
        out vec4 FragColor;
        void main() {
            // Create a circular point
            vec2 center = gl_PointCoord - vec2(0.5);
            float dist = length(center);
            if(dist > 0.5){
             discard;
            }

            // Soft edge
            float alpha = 1.0 - smoothstep(0.3, 0.5, dist);
            float red = 1;
            float green = 0.5;
            float blue = 0.0; 
            FragColor = vec4(red, green, blue, alpha * 1);
        }
    )";

    unsigned int vertexShader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertexShader, 1, &vertexShaderSource, NULL);
    glCompileShader(vertexShader);
    
    int success;
    char infoLog[512];
    glGetShaderiv(vertexShader, GL_COMPILE_STATUS, &success);
    if(!success){
        glGetShaderInfoLog(vertexShader, 512, NULL, infoLog);
        cout << "ERROR::SHADER::VERTEX::COMPILATION_FAILED\n" << infoLog << endl;
    }

    unsigned int fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragmentShader, 1, &fragmentShaderSource, NULL);
    glCompileShader(fragmentShader);
    
    glGetShaderiv(fragmentShader, GL_COMPILE_STATUS, &success);
    if(!success){
        glGetShaderInfoLog(fragmentShader, 512, NULL, infoLog);
        cout << "ERROR::SHADER::FRAGMENT::COMPILATION_FAILED\n" << infoLog << endl;
    }

    unsigned int program = glCreateProgram();
    glAttachShader(program, vertexShader);
    glAttachShader(program, fragmentShader);
    glLinkProgram(program);
    
    glGetProgramiv(program, GL_LINK_STATUS, &success);
    if(!success){
        glGetProgramInfoLog(program, 512, NULL, infoLog);
        cout << "ERROR::SHADER::PROGRAM::LINKING_FAILED\n" << infoLog << endl;
    }

    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);
    
    return program;
}

unsigned int createColourShader(){
  const char* vertexShaderSource = R"(
      #version 330 core
      layout (location = 0) in vec3 pos;
      layout (location = 1) in vec3 normal;

      out vec3 Normal;
      out vec3 FragPos;

      uniform mat4 model;
      uniform mat4 view;
      uniform mat4 projection;

      void main(){
       vec4 worldPos = model * vec4(pos, 1.0);
       FragPos = vec3(worldPos);
       Normal = mat3(transpose(inverse(model))) * normal;
       gl_Position = projection * view * worldPos;
      }
)";

  const char* fragmentShaderSource = R"(
        #version 330 core
        in vec3 Normal;
        in vec3 FragPos;

        out vec4 FragColor;

        uniform vec3 lightDir;   // direction the light points, e.g. (-0.5, -1.0, -0.3)
        uniform vec4 liquidColor;

        void main() {
         vec3 N = normalize(Normal);
         vec3 L = normalize(-lightDir);
         float diff = max(dot(N, L), 0.0);
         vec3 baseColor = liquidColor.rgb;
         vec3 lit = baseColor * (0.3 + 0.7 * diff);
         FragColor = vec4(lit, liquidColor.a);
}
    )";

  // Compile vertex shader
    unsigned int vertexShader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertexShader, 1, &vertexShaderSource, NULL);
    glCompileShader(vertexShader);
    
    int success;
    char infoLog[512];
    glGetShaderiv(vertexShader, GL_COMPILE_STATUS, &success);
    if(!success){
        glGetShaderInfoLog(vertexShader, 512, NULL, infoLog);
        cout << "ERROR::SHADER::VERTEX::COMPILATION_FAILED\n" << infoLog << endl;
    }

    // Compile fragment shader
    unsigned int fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragmentShader, 1, &fragmentShaderSource, NULL);
    glCompileShader(fragmentShader);
    
    glGetShaderiv(fragmentShader, GL_COMPILE_STATUS, &success);
    if(!success){
        glGetShaderInfoLog(fragmentShader, 512, NULL, infoLog);
        cout << "ERROR::SHADER::FRAGMENT::COMPILATION_FAILED\n" << infoLog << endl;
    }

    // Link program
    unsigned int program = glCreateProgram();
    glAttachShader(program, vertexShader);
    glAttachShader(program, fragmentShader);
    glLinkProgram(program);
    
    glGetProgramiv(program, GL_LINK_STATUS, &success);
    if(!success){
        glGetProgramInfoLog(program, 512, NULL, infoLog);
        cout << "ERROR::SHADER::PROGRAM::LINKING_FAILED\n" << infoLog << endl;
    }

    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);
    
    return program;
}

unsigned int shaders(){
const char* vertexShaderSource = R"(
    #version 330 core
    layout (location = 0) in vec3 aPos;
    layout (location = 1) in vec2 aTexCoord;

    out vec2 TexCoord;

    uniform mat4 model;
    uniform mat4 view;
    uniform mat4 projection;

    void main(){
        gl_Position = projection * view * model * vec4(aPos, 1.0);
        TexCoord = aTexCoord;
    }
)";

const char* fragmentShaderSource = R"(
    #version 330 core
    out vec4 FragColor;
    in vec2 TexCoord;
    uniform sampler2D ourTexture;

     void main(){
        vec4 texColor = texture(ourTexture, TexCoord);
        
        // Use the texture's alpha channel (transparent background = invisible)
        float alpha = texColor.a;  // 0 = transparent, 1 = opaque
        
        // If the texture has no alpha, use 1.0
        if(alpha < 0.01){
            discard;  // Don't draw transparent pixels
        }
        
        FragColor = vec4(texColor.rgb, alpha);
    }
)";

     // Compile vertex shader
    unsigned int vertexShader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertexShader, 1, &vertexShaderSource, NULL);
    glCompileShader(vertexShader);
    
    int success;
    char infoLog[512];
    glGetShaderiv(vertexShader, GL_COMPILE_STATUS, &success);
    if(!success){
        glGetShaderInfoLog(vertexShader, 512, NULL, infoLog);
        cout << "ERROR::SHADER::VERTEX::COMPILATION_FAILED\n" << infoLog << endl;
    }

    // Compile fragment shader
    unsigned int fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragmentShader, 1, &fragmentShaderSource, NULL);
    glCompileShader(fragmentShader);
    
    glGetShaderiv(fragmentShader, GL_COMPILE_STATUS, &success);
    if(!success){
        glGetShaderInfoLog(fragmentShader, 512, NULL, infoLog);
        cout << "ERROR::SHADER::FRAGMENT::COMPILATION_FAILED\n" << infoLog << endl;
    }

    // Link program
    unsigned int program = glCreateProgram();
    glAttachShader(program, vertexShader);
    glAttachShader(program, fragmentShader);
    glLinkProgram(program);
    
    glGetProgramiv(program, GL_LINK_STATUS, &success);
    if(!success){
        glGetProgramInfoLog(program, 512, NULL, infoLog);
        cout << "ERROR::SHADER::PROGRAM::LINKING_FAILED\n" << infoLog << endl;
    }

    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);
    
    return program;
}

unsigned int loadTexture(const char* path){
    unsigned int textureID = 0;
    int width, height, channels;
    
    // Load the image
    unsigned char* data = stbi_load(path, &width, &height, &channels, 0);
    
    if(!data){
        cout << "Failed to load texture: " << path;
        return 0;
    }
    
    // Determine the format
    GLenum format;
    if(channels == 1){
      format = GL_RED;
    }
    
    else if(channels == 3){
      format = GL_RGB;
    }
    
    else if(channels == 4){
      	format = GL_RGBA;
      }
      
    // Generate and bind the texture
    glGenTextures(1, &textureID);
    glBindTexture(GL_TEXTURE_2D, textureID);
    
    // Set texture parameters
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    
    // Upload to GPU
    glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, data);
    glGenerateMipmap(GL_TEXTURE_2D);
    
    // Free CPU memory
    stbi_image_free(data);
    
    return textureID;
}

void initBottle(){
// --- VERTEX DATA ---
  x = -1.81f;
  y = -3.0f;
  w = 3.6f;
  h = 1.1f;
  z = 0.0f;
  u = 0.0f;
  du = 1.0f;
  v = 0.0f;
  dv = 1.0f;
  
  float bottle_vertices[] = {// x,y,z,u,v
    // positions          // texture coordinates
    x, y, z, u, v,   // bottom-left
    x + w, y, z,  u + du, v,   // bottom-right
    x + w, y + h, z, u + du, v + dv,   // top-right
    x,  y + h, z, u, v + dv    // top-left
};

unsigned int bottle_indices[] = {
    0, 1, 2,
    0, 2, 3
};

  glGenVertexArrays(1, &VAO_bottle);
    glGenBuffers(1, &VBO_bottle);
    glGenBuffers(1, &EBO_bottle);

    glBindVertexArray(VAO_bottle);

    glBindBuffer(GL_ARRAY_BUFFER, VBO_bottle);
    glBufferData(GL_ARRAY_BUFFER, sizeof(bottle_vertices), bottle_vertices, GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO_bottle);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(bottle_indices), bottle_indices, GL_STATIC_DRAW);

    // location 0: position
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5*sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    // location 1: texcoord
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5*sizeof(float), (void*)(3*sizeof(float)));
    glEnableVertexAttribArray(1);

    glBindVertexArray(0);

    bottleIndexCount = 6;
}

void getLiquidGeometry(vector<float>& vertices, vector<unsigned int>& indices, float time){
  vertices.clear();
  indices.clear();

  int sectors = 32;
  int stacks = 15;
  bottomY = -2.75f;
  topY = -2.315f;
  radius = 0.2f;
  topTaperStart = 0.69f;
  waveAmplitude = 0.01f;
  waveSpeed = 4.0f;

  // bottom-center vertex: position + normal
  vertices.push_back(0.0f);
  vertices.push_back(bottomY);
  vertices.push_back(0.0f);
  vertices.push_back(0.0f);
  vertices.push_back(-1.0f);
  vertices.push_back(0.0f);			
 
  int surfaceCenterIdx = (stacks + 1) * (sectors + 1) + 1;

  for(int i = 0; i <= stacks; i++){
    float factor = (float)i/stacks;
    float y = bottomY + factor * (topY - bottomY);

    float r = radius;

    if(factor > topTaperStart){
      r = radius * (1.0f - (factor - topTaperStart) * 1.5f);
    }

    if(r < 0.05){
      r = 0.05;
    }
    
    for(int j = 0; j <= sectors; j++){
      float angle = (float)j/sectors * 2.0f * M_PI;
      float currY = y;

      if(i == stacks){
	currY += waveAmplitude * sin(time * waveSpeed + 2.0f * angle);
      }

      float x = r * cos(angle);
      float z = r * sin(angle);
      vertices.push_back(x);
      vertices.push_back(currY);
      vertices.push_back(z);

      float nx = cos(angle);
      float ny = 0.0f;
      float nz = sin(angle);
      vertices.push_back(nx);
      vertices.push_back(ny);
      vertices.push_back(nz);
      }
    }

  for(int i = 0; i < stacks; i++){
    for(int j = 0; j < sectors; j++){
      unsigned int first = 1 + (i * (sectors + 1)) + j;
      unsigned int second = first + sectors + 1;
      indices.push_back(first);
      indices.push_back(second);
      indices.push_back(first + 1);
      indices.push_back(second);
      indices.push_back(second + 1);
      indices.push_back(first + 1);
  }
  }

  // top-center vertex: position + normal
  vertices.push_back(0.0f);
  vertices.push_back(topY + waveAmplitude * sin(time * waveSpeed));
  vertices.push_back(0.0f);
  vertices.push_back(0.0f);
  vertices.push_back(1.0f);
  vertices.push_back(0.0f);
 
  unsigned int lastStackOffset = 1 + stacks * (sectors + 1);
  for(int j = 0; j < sectors; j++){
    indices.push_back(surfaceCenterIdx);
    indices.push_back(lastStackOffset + j + 1);
    indices.push_back(lastStackOffset + j);
  }
  
  liquidIndexCount = indices.size();
}

void getSpilledLiquidGeometry(vector<float>& vertices, vector<unsigned int>& indices, float time){
  vertices.clear();
  indices.clear();

  int sectors = 750;
  float puddleY = -0.6f;
  float puddleRadius = 0.2f;
  float puddleIrregularity = 0.5f;

  // bottom-center vertex: position + normal
  vertices.push_back(0.0f);
  vertices.push_back(puddleY);
  vertices.push_back(0.0f);
  vertices.push_back(0.0f);
  vertices.push_back(1.0f);
  vertices.push_back(0.0f);

  for(int j = 0; j <= sectors; j++){
    float angle = (float)j / sectors * 2.0f * M_PI;
    float r = puddleRadius * (1.0f + puddleIrregularity * sin(3.0f * angle) * cos(5.0f * angle));
    float x = r*cos(angle);
    float z = r*sin(angle);

    vertices.push_back(x);
    vertices.push_back(puddleY);
    vertices.push_back(z);
    vertices.push_back(0.0f);   // nx
    vertices.push_back(1.0f);   // ny
    vertices.push_back(0.0f); 
  }

for(int j = 0; j < sectors; j++){
      unsigned int center = 0;
      unsigned int first = 1 + j;
      unsigned int second = 2 + j;
      indices.push_back(center);
      indices.push_back(second);
      indices.push_back(first);
  }
  
  liquidIndexCount = indices.size();
}

void initLiquid(){
  vector<float> vertices;
  vector<unsigned int> indices;
  getLiquidGeometry(vertices, indices, 0.0f);

  glGenVertexArrays(1, &VAO_liquid);
  glGenBuffers(1, &VBO_liquid);
  glGenBuffers(1, &EBO_liquid);
  
  glBindVertexArray(VAO_liquid);
  
  glBindBuffer(GL_ARRAY_BUFFER, VBO_liquid);
  glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(float) * 2, NULL, GL_DYNAMIC_DRAW);
  glBufferSubData(GL_ARRAY_BUFFER, 0, vertices.size() * sizeof(float), vertices.data());
 
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO_liquid);
  glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int), indices.data(), GL_STATIC_DRAW);
  
  glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0);
  
  glEnableVertexAttribArray(0);

  glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(3 * sizeof(float)));

  glEnableVertexAttribArray(1);

  glBindVertexArray(0);
}

void updateLiquid(){
  vector<float> vertices;
  vector<unsigned int> indices;

  if(!broken){
  getLiquidGeometry(vertices,indices,glfwGetTime());
  }

  else{
    getSpilledLiquidGeometry(vertices,indices,glfwGetTime());
  }
  
  glBindBuffer(GL_ARRAY_BUFFER,VBO_liquid);
  glBufferSubData(GL_ARRAY_BUFFER,0,vertices.size() * sizeof(float),vertices.data());
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO_liquid);
glBufferSubData(GL_ELEMENT_ARRAY_BUFFER, 0, indices.size() * sizeof(unsigned int), indices.data());
  glBindBuffer(GL_ARRAY_BUFFER,0);
}

void initBubbles(int num_bubbles){
  bubbles.clear();
  vector<float> bubbleData;
  float liquidRadius = 0.2f;
  float bubble_y_init_low = -2.85f;
  float bubble_y_init_high = -2.315f;
  
  for(int i = 0; i < num_bubbles; i++){
    Bubble b;
    float angle = random_number(0.0f,2*M_PI);
    float r = random_number(0.25f, liquidRadius);

    b.x = r * cos(angle);
    b.y = random_number(bubble_y_init_low,bubble_y_init_high);
    b.z = r * sin(angle);
    b.speed = random_number(0.15f, 0.45f);
    b.size = random_number(0.04f, 0.12f);
    b.phase = random_number(0.0f, 6.28f);
    bubbles.push_back(b);

    bubbleData.push_back(b.x);
    bubbleData.push_back(b.y);
    bubbleData.push_back(b.z);
    bubbleData.push_back(b.size);
    }
    
    // Create VBO with positions and sizes
    glGenBuffers(1, &VBO_bubbles);
    glBindBuffer(GL_ARRAY_BUFFER, VBO_bubbles);
    glBufferData(GL_ARRAY_BUFFER, bubbleData.size() * sizeof(float), bubbleData.data(), GL_DYNAMIC_DRAW);
    
    // Setup VAO for bubbles
    glGenVertexArrays(1, &VAO_bubbles);
    glBindVertexArray(VAO_bubbles);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    glVertexAttribPointer(1, 1, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);
}

void updateBubbles(float deltaTime){
  vector<float> bubbleData;
  liquidRadius = 0.2f;
  float bubble_y_init_low = -2.85f;
  float bubble_y_init_high = -2.315f;
  top_of_bottle = -2.325f;
    
  for(auto &b : bubbles){
    b.x += 0.02*sin(b.phase + 5.0f*b.y) * deltaTime;
    b.y += b.speed * deltaTime;
    b.z += 0.02*cos(b.phase + 5.0f*b.y) * deltaTime;

    float r2 = b.x*b.x + b.z*b.z;
    float maxR = liquidRadius * 0.9f;   // stay a bit inside the wall
    if(r2 > maxR*maxR){
      float r = sqrtf(r2);
      b.x = b.x / r * maxR;
      b.z = b.z / r * maxR;
    }
 
    if(b.y >= top_of_bottle){
      b.y = random_number(bubble_y_init_low,bubble_y_init_high);
      float angle = random_number(0.0f,2*M_PI);
      float r = random_number(0.0f, 0.9f*liquidRadius);
      b.x = r*cos(angle);
      b.z = r*sin(angle);
    }
    
   // Store position and size
        bubbleData.push_back(b.x);
        bubbleData.push_back(b.y);
	bubbleData.push_back(b.z);
        bubbleData.push_back(b.size);
    }
    
    // Update the VBO with new data
    glBindBuffer(GL_ARRAY_BUFFER, VBO_bubbles);
    glBufferSubData(GL_ARRAY_BUFFER, 0, bubbleData.size() * sizeof(float), bubbleData.data());
    glBindBuffer(GL_ARRAY_BUFFER, 0);
}

void add_shard(pair<float,float> p1, pair<float,float> p2, pair<float,float> p3){

  float shard_scale = 0.75f;
  
  float x1 = p1.first;
  float y1 = p1.second;
  float x2 = p2.first;
  float y2 = p2.second;
  float x3 = p3.first;
  float y3 = p3.second;

  float cx = (x1 + x2 + x3)/3.0f;
  float cy = (y1 + y2 + y3)/3.0f;

  auto uv = [](float px, float py, float& u, float& v){
    u = (px - BX_MIN)/(BX_MAX - BX_MIN);
    v = (py - BY_MIN)/(BY_MAX - BY_MIN);
  };

  float u1, v1, u2, v2, u3, v3;

  uv(x1,y1,u1,v1);
  uv(x2,y2,u2,v2);
  uv(x3,y3,u3,v3);

  shard_vertices.insert(shard_vertices.end(),{
      (x1 - cx) * shard_scale, (y1 - cy) * shard_scale, 0.0f, u1, v1,
      (x2 - cx) * shard_scale, (y2 - cy) * shard_scale, 0.0f, u2, v2,
      (x3 - cx) * shard_scale, (y3 - cy) * shard_scale, 0.0f, u3, v3
});

  Bottle_shard s = {};

  s.local_cx = cx;
  s.local_cy = cy;
  s.x = 0.0f;
  s.y = 0.0f;
  s.z = 0.0f;
  s.vx = 0.0f;
  s.vy = 0.0f;
  s.vz = 0.0f;
  s.theta = 0.0f;
  s.omega = 0.0f;
  Bottle_shards.push_back(s);
}

void initShards(){
    vector<pair<float,float>> grid((cols + 1) * (rows + 1));

    for(int i = 0; i <= rows; ++i){
      for(int j = 0; j <= cols; ++j){
            float fx = (float)j / cols;
            float fy = (float)i / rows;

            float px = BX_MIN + fx * (BX_MAX - BX_MIN);
            float py = BY_MIN + fy * (BY_MAX - BY_MIN);

            if(j > 0 && j < cols && i > 0 && i < rows){
                px += random_number(-0.05f, 0.05f);
                py += random_number(-0.05f, 0.05f);
            }
            grid[i * (cols + 1) + j] = {px, py};
        }
    }

    shard_vertices.clear();
    Bottle_shards.clear();

    for(int i = 0; i < rows; ++i){
        for(int j = 0; j < cols; ++j){
            auto A = grid[i * (cols + 1) + j];
            auto B = grid[i * (cols + 1) + (j + 1)];
            auto C = grid[(i + 1) * (cols + 1) + j];
            auto D = grid[(i + 1) * (cols + 1) + (j + 1)];

            add_shard(A, B, C);
            add_shard(B, D, C);
        }
    }
}

int main(){

  glfwInitHint(GLFW_PLATFORM, GLFW_PLATFORM_X11);
  
  if(!glfwInit()){
    fprintf(stderr, "Failed to initialize GLFW\n");
    return -1;
  }

  glfwWindowHint(GLFW_SAMPLES, 4);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR,3);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR,3);
  glfwWindowHint(GLFW_OPENGL_PROFILE,GLFW_OPENGL_CORE_PROFILE);

  GLFWwindow* window;
  window = glfwCreateWindow(width,height,"3D Beer Bottle Simulation!",NULL,NULL);
  
  if(window == NULL){
    fprintf(stderr,"Failed to open GLFW window\n");
    glfwTerminate();
    return -1;
  }
  
  glfwMakeContextCurrent(window);
  glewExperimental = true;

  if(glewInit() != GLEW_OK){
    fprintf(stderr,"Failed to initalize GLEW\n");
    return -1;
  }

  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

  glEnable(GL_DEPTH_TEST);
  glDisable(GL_CULL_FACE);
  
  shaderProgram = shaders();
  colourShaderProgram = createColourShader();
  pointShaderProgram = createPointShader();

  dx = 0;
  dy = 0.75;
  dz = 0;
  
  float camera_dist = 5.0f;
  float half_h = tan(radians(45.0f)*0.5f) * camera_dist;
  float half_w = half_h * aspect;

  mat4 bottleModel = mat4(1.0f);
  mat4 view = translate(mat4(1.0f),vec3(0.0f, 0.0f, -camera_dist));
  mat4 projection = perspective(radians(45.0f), (float)width / (float)height, 0.1f, 100.0f);
  mat4 puddleModel = translate(mat4(1.0f),vec3(dx, -1.19, dz));

  initBottle();
  initShards();

  glGenVertexArrays(1, &VAO_shards);
  glGenBuffers(1, &VBO_shards);
  glBindVertexArray(VAO_shards);
  
  glBindBuffer(GL_ARRAY_BUFFER, VBO_shards);
  glBufferData(GL_ARRAY_BUFFER, shard_vertices.size() * sizeof(float), shard_vertices.data(), GL_STATIC_DRAW);

  glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5*sizeof(float), (void*)0);
  glEnableVertexAttribArray(0);
  glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5*sizeof(float), (void*)(3*sizeof(float)));
  glEnableVertexAttribArray(1);

  glBindVertexArray(0);
 
  initLiquid();
  initBubbles(150);

  stbi_set_flip_vertically_on_load(true);  
  unsigned int bottleTexture = loadTexture("beer_bottle.png");
    
  float lastFrame = 0.0f;
  double previous_mouse_x = 0.0;
  double previous_mouse_y = 0.0;
  bool first_drag_frame = false;
  bool was_pressed = false;

  vec3 pivot = vec3(-0.01f, -2.45f, 0.0f);
  
  while(!glfwWindowShouldClose(window)){
    // ... input handling ...

    glClearColor(0.0f, 0.0f, 0.2f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    float currentFrame = glfwGetTime();
    float deltaTime = currentFrame - lastFrame;
    lastFrame = currentFrame;
    double current_mouse_x;
    double current_mouse_y;
    
    glfwGetCursorPos(window, &current_mouse_x, &current_mouse_y);

    const float half_w_bottle = 1.80f;
    const float half_h_bottle = 0.55f;
    float max_dx =  half_w - (half_w_bottle - 1.35f) * scale_factor;
    float min_dx = -half_w + (half_w_bottle - 1.55f) * scale_factor;
    float max_dy =  half_h - (half_h_bottle - 0.7f) * scale_factor;
    float min_dy = -half_h + (half_h_bottle + 3.5f) * scale_factor;
  
    bool is_pressed = (glfwGetMouseButton(window,GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS);

    if(glfwGetKey(window,GLFW_KEY_R) == GLFW_PRESS){
      scale_factor = 0.7f;
      rotate_factor = 0.0f;
      dx = 0.0f;
      dy = 0.75f;
      dz = 0.0f;
      throw_bottle = false;
      move_bottle = false;
      can_throw = false;
      broken = false;
      shards_spawned = false;
    }

    if(is_pressed && !was_pressed){
      first_drag_frame = true;
      can_throw = true;
      move_bottle = true;
    }

  if(move_bottle && is_pressed && !broken){
    scale_factor = 0.7f;

    if(first_drag_frame){
        previous_mouse_x = current_mouse_x;
        previous_mouse_y = current_mouse_y;
        first_drag_frame = false;
    }
    
    else{
        delta_x = current_mouse_x - previous_mouse_x;
        delta_y = current_mouse_y - previous_mouse_y;

        dx += delta_x * sensitivity;
        dy -= delta_y * sensitivity;

        // --- CLAMP TO VIEWPORT ---
        if(dx > max_dx){
	  dx = max_dx;
	}
	
	if(dx < min_dx){
	    dx = min_dx;
        }

	if(dy > max_dy){
	  dy = max_dy;
	}
	
        if(dy < min_dy){
	  dy = min_dy;
	}
        // --- END CLAMP ---

        if(deltaTime > 0.001f){
            vel_x = delta_x * throw_scale/deltaTime;
	    vel_y = -delta_y * throw_scale/deltaTime;
        }

        previous_mouse_x = current_mouse_x;
        previous_mouse_y = current_mouse_y;
    }
}

    if(!is_pressed && was_pressed){
      move_bottle = false;
      throw_bottle = true;
}

    was_pressed = is_pressed;

    bottleModel= mat4(1.0f);
    
    if(throw_bottle && !move_bottle){
    vel_x += delta_x * deltaTime;
    dx += vel_x * deltaTime;

    vel_y += delta_y * deltaTime;
    dy += vel_y * deltaTime;

    rotate_factor = (float)glfwGetTime();

    if(dx > max_dx){
      vel_x *= -0.99;
    }
    
    if(dx < min_dx){
      vel_x *= -0.99;
    }
    
    if(dy > max_dy){
      vel_y *= -0.99;
    }
    
    if(dy < min_dy && !shards_spawned){
        hitx = dx;
        // Build world matrix for spawn
        mat4 spawnModel = mat4(1.0f);
        spawnModel = translate(spawnModel, vec3(dx, dy, dz));
        spawnModel = translate(spawnModel, pivot);
        spawnModel = rotate(spawnModel, rotate_factor, vec3(0.0f, 0.0f, 1.0f));
        spawnModel = scale(spawnModel, vec3(scale_factor));
        spawnModel = translate(spawnModel, -pivot);

        vec3 pivot_world = vec3(spawnModel * vec4(pivot, 1.0f));

        for(auto& s : Bottle_shards){
            vec4 local = vec4(s.local_cx, s.local_cy, 0.0f, 1.0f);
            vec3 world = vec3(spawnModel * local);

            s.x = world.x;
            s.y = world.y;
            s.z = world.z;

            vec3 dir = world - pivot_world;
            float len = length(dir);
            if(len > 1e-4f){
	      dir /= len;
	    }
	    
            else{
	      dir = vec3(1.0f, 0.0f, 0.0f);
	    }
	    
            float speed = random_number(2.0f, 4.0f);
            s.vx = dir.x * speed;
            s.vy = dir.y * speed;
            s.vz = 0.0f;

            s.theta = random_number(0.0f, 6.28f);
            s.omega = random_number(-8.0f, 8.0f);
        }

        broken = true;
        shards_spawned = true;
    }
}
    
    updateBubbles(deltaTime);
    updateLiquid();

    if(broken){
    for(auto& s : Bottle_shards){
        s.vy -= g * deltaTime * 500.0f;   
        s.x  += s.vx * deltaTime;
        s.y  += s.vy * deltaTime;
        s.theta += s.omega * deltaTime;
        s.omega *= 0.99f;

	if(s.x < BX_MIN || s.x > BX_MAX){
	  s.vx *= -1;
	}

	if(s.y > BX_MAX){
	  s.vy *= -0.75;
	}

	if(s.y < Floor + 0.1f && fabsf(s.vy) < 0.3f){
	  s.y = Floor + 0.1f;
	  s.vy = 0.0f;
	  s.vx *= 0.85f;            // friction, but keep sliding
	  s.omega *= 0.85f;
	}
	
	else if(s.y < Floor + 0.1f){
	  s.y = Floor + 0.1f;
	  s.vy *= -0.6f;
	  s.vx *= 0.75f;
	  s.omega *= 0.75f;
	}
    }
    }

    // bottleModel: main
    bottleModel = mat4(1.0f);
    bottleModel = translate(bottleModel, vec3(dx, dy, dz));
    bottleModel = translate(bottleModel, pivot);
    bottleModel = rotate(bottleModel, rotate_factor, vec3(0.0f, 0.0f, 1.0f));
    bottleModel = scale(bottleModel, vec3(scale_factor));
    bottleModel = translate(bottleModel, -pivot);

    // puddleModel: static
    puddleModel = mat4(1.0f);
    puddleModel = translate(puddleModel, vec3(hitx, Floor + 0.6f, dz));
    
    // 1. Draw bottle
    if(!broken){
    glUseProgram(shaderProgram);
    glUniform1i(glGetUniformLocation(shaderProgram, "ourTexture"), 0);
    glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "model"), 1, GL_FALSE, value_ptr(bottleModel));  
    glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "view"), 1, GL_FALSE, value_ptr(view));
    glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "projection"), 1, GL_FALSE, value_ptr(projection));
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, bottleTexture);
    glBindVertexArray(VAO_bottle);
    glDrawElements(GL_TRIANGLES, bottleIndexCount, GL_UNSIGNED_INT, 0);

    // 2. Draw bubbles (Shares the exact same 'model' matrix)
    glUseProgram(pointShaderProgram);
    glEnable(GL_PROGRAM_POINT_SIZE);
    glUniformMatrix4fv(glGetUniformLocation(pointShaderProgram, "model"), 1, GL_FALSE, value_ptr(bottleModel));
    glUniformMatrix4fv(glGetUniformLocation(pointShaderProgram, "view"), 1, GL_FALSE, value_ptr(view));
    glUniformMatrix4fv(glGetUniformLocation(pointShaderProgram, "projection"), 1, GL_FALSE, value_ptr(projection));
    glBindVertexArray(VAO_bubbles);
    glDrawArrays(GL_POINTS, 0, bubbles.size());
    glDisable(GL_PROGRAM_POINT_SIZE);

        // 3. Draw liquid (Shares the exact same 'model' matrix)
    glUseProgram(colourShaderProgram);
    glUniformMatrix4fv(glGetUniformLocation(colourShaderProgram, "model"), 1, GL_FALSE, value_ptr(bottleModel));
    glUniformMatrix4fv(glGetUniformLocation(colourShaderProgram, "view"), 1, GL_FALSE, value_ptr(view));
    glUniformMatrix4fv(glGetUniformLocation(colourShaderProgram, "projection"), 1, GL_FALSE, value_ptr(projection));
    glUniform4f(glGetUniformLocation(colourShaderProgram, "liquidColor"), 0.9f, 0.5f, 0.0f, 0.8f);
    glUniform3f(glGetUniformLocation(colourShaderProgram, "lightDir"), -0.5f, -1.0f, -0.3f);
    glDepthMask(GL_FALSE);
    glBindVertexArray(VAO_liquid);
    glDrawElements(GL_TRIANGLES, liquidIndexCount, GL_UNSIGNED_INT, 0);
    glDepthMask(GL_TRUE);
    }
    
    if(broken){ 
      // 1. Draw broken shards
      glUseProgram(shaderProgram);
      glUniform1i(glGetUniformLocation(shaderProgram, "ourTexture"), 0);
      glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "view"), 1, GL_FALSE, value_ptr(view));
      glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "projection"), 1, GL_FALSE, value_ptr(projection));
      glActiveTexture(GL_TEXTURE0);
      
      glBindVertexArray(VAO_shards);
      for(int i = 0; i < (int)Bottle_shards.size(); i++){
	const auto& s = Bottle_shards[i];
	mat4 m = mat4(1.0f);
	m = translate(m, vec3(s.x, s.y, s.z));
	m = rotate(m, s.theta, vec3(0.0f, 0.0f, 1.0f));
	glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "model"), 1, GL_FALSE, value_ptr(m));
	glDrawArrays(GL_TRIANGLES, i * 3, 3);
      }
        // 2. Draw liquid (Shares the exact same ' broken model' matrix)
    glUseProgram(colourShaderProgram);
    glUniformMatrix4fv(glGetUniformLocation(colourShaderProgram, "model"), 1, GL_FALSE, value_ptr(puddleModel));
    glUniformMatrix4fv(glGetUniformLocation(colourShaderProgram, "view"), 1, GL_FALSE, value_ptr(view));
    glUniformMatrix4fv(glGetUniformLocation(colourShaderProgram, "projection"), 1, GL_FALSE, value_ptr(projection));
    glUniform4f(glGetUniformLocation(colourShaderProgram, "liquidColor"), 0.95f, 0.75f, 0.3f, 0.6f);
    glUniform3f(glGetUniformLocation(colourShaderProgram, "lightDir"), -0.5f, -1.0f, -0.3f);
    glDepthMask(GL_FALSE);
    glBindVertexArray(VAO_liquid);
    glDrawElements(GL_TRIANGLES, liquidIndexCount, GL_UNSIGNED_INT, 0);
    glDepthMask(GL_TRUE);
}

    glfwSwapBuffers(window);
    glfwPollEvents();
}
  
    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}

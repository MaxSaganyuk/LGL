#include <fstream>
#include <sstream>
#include <algorithm>

#include "stb_image.h"
#include "LGL.h"

std::function<void(double, double)> LGL::cursorPositionFunc = nullptr;
std::function<void(double, double)> LGL::scrollCallbackFunc = nullptr;

LGL::LGL(int major, int minor)
{
	glfwInit();
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, major);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, minor);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

	background = { 0, 0, 0, 1 };
	currentVAOToRender = {};

	stbi_set_flip_vertically_on_load(true);

	std::cout << "Created LambdaGL instance\n";
}

LGL::~LGL()
{
	for (auto& shader : shaderInfoCollection)
	{
		glDeleteShader(shader.second.shaderId);
	}

	glfwTerminate();

	std::cout << "LambdaGL instance destroyed\n";
}

bool LGL::CreateWindow(const int height, const int width, const std::string& title)
{
	if (window)
	{
		std::cout << "Window already created\n";
		return false;
	}

	window = glfwCreateWindow(height, width, title.c_str(), nullptr, nullptr);

	if (!window)
	{
		std::cout << "Failed to crate GLFW window\n";
		return false;
	}

	glfwMakeContextCurrent(window);

	std::cout << "Created a GLFW window\n";

	return true;
}

bool LGL::InitGLAD()
{
	if (!gladLoadGLLoader(reinterpret_cast<GLADloadproc>(glfwGetProcAddress)))
	{
		std::cout << "Failed to init GLAD\n";
		return false;
	}

	glEnable(GL_DEPTH_TEST);
	//glDepthFunc(GL_ALWAYS);

	std::cout << "GLAD initialized\n";

	return true;
}

void LGL::InitCallbacks()
{
	glfwSetFramebufferSizeCallback(window, FramebufferSizeCallback);
}

void LGL::CaptureMouse()
{
	glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

	std::cout << "Mouse has been captured\n";
}

void LGL::SetCursorPositionCallback(std::function<void(double, double)> callbackFunc)
{
	glfwSetCursorPosCallback(window, CursorPositionCallback);
	cursorPositionFunc = callbackFunc;

	std::cout << "Cursor callback set\n";
}

void LGL::SetScrollCallback(std::function<void(double, double)> callbackFunc)
{
	glfwSetScrollCallback(window, ScrollCallback);
	scrollCallbackFunc = callbackFunc;

	std::cout << "Scroll callback set\n";
}

int LGL::GetMaxAmountOfVertexAttr()
{
	int attr;
	glGetIntegerv(GL_MAX_VERTEX_ATTRIBS, &attr);
	
	std::cout << "Max amount of vertex attributes: " << attr << '\n';
	
	return attr;
}

void LGL::ProcessInput()
{
	for (auto& interact : interactCollection)
	{
		if (glfwGetKey(window, interact.first) == GLFW_PRESS)
		{
			interact.second();
		}
	}
}

bool LGL::HandleGLError(const unsigned int& glId, int statusToCheck)
{
	constexpr int logSize = 512;

	int success;
	char log[logSize];

	glGetShaderiv(glId, GL_COMPILE_STATUS, &success);
	if (!success)
	{
		glGetShaderInfoLog(glId, logSize, nullptr, log);
		std::cout << log << '\n';

		return false;
	}
	
	return true;
}

void LGL::FramebufferSizeCallback(GLFWwindow* window, int width, int height)
{
	glViewport(0, 0, width, height);
}

void LGL::Render()
{
	if (currentVAOToRender.vboId != 0)
	{
		if (!currentVAOToRender.useIndices)
		{
			glDrawArrays(GL_TRIANGLES, 0, currentVAOToRender.pointAmount);
		}
		else
		{
			glDrawElements(GL_TRIANGLES, currentVAOToRender.pointAmount, GL_UNSIGNED_INT, 0);
		}
	}
}

void LGL::RunRenderingCycle(std::function<void()> additionalSteps)
{
	while (!glfwWindowShouldClose(window))
	{
		ProcessInput();

		glClearColor(background.r, background.g, background.b, background.a);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		if (additionalSteps)
		{
			additionalSteps();
		}

		for (auto& currentVAO : VAOCollection)
		{
			currentVAOToRender = currentVAO;

			lastProgram = currentVAO.shader;
			glUseProgram(shaderProgramCollection[lastProgram]);
			glBindVertexArray(currentVAO.vboId);

			for (int i = 0; i < currentVAO.textures.size(); ++i)
			{
				if (textureCollection.find(currentVAO.textures[i]) != textureCollection.end())
				{
					TextureInfo& textureInfo = textureCollection.at(currentVAO.textures[i]);
					glActiveTexture(GL_TEXTURE0 + i);
					glBindTexture(GL_TEXTURE_2D, textureInfo.textureId);
				}
			}

			if (currentVAO.behaviour)
			{
				currentVAO.behaviour();
			}

			Render();
		}
		currentVAOToRender = {};

		glfwSwapBuffers(window);
		glfwPollEvents();
	}
}

void LGL::SetStaticBackgroundColor(const glm::vec4& rgba)
{
	background = rgba;
}

void LGL::CreateMesh(const MeshInfo& meshInfo)
{
	VAOCollection.push_back({0, false});
	VAO* newVAO = &VAOCollection.back().vboId;
	glGenVertexArrays(1, newVAO);
	glBindVertexArray(*newVAO);

	VBOCollection.push_back(VBO());
	VBO* newVBO = &VBOCollection.back();

	glGenBuffers(1, newVBO);
	glBindBuffer(GL_ARRAY_BUFFER, *newVBO);
	glBufferData(
		GL_ARRAY_BUFFER, 
		meshInfo.vert.size() * sizeof(Vertex),
		&meshInfo.vert[0],
		meshInfo.isDynamic ? GL_DYNAMIC_DRAW : GL_STATIC_DRAW
	);
	size_t stride = 0;
	for (int i = 0; i < steps.size(); ++i)
	{
		stride += steps[i];
	}

	if (!meshInfo.indices.empty())
	{
		EBOCollection.push_back(EBO());
		EBO* newEBO = &EBOCollection.back();

		glGenBuffers(1, newEBO);
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, *newEBO);
		glBufferData(
			GL_ELEMENT_ARRAY_BUFFER, 
			meshInfo.indices.size() * sizeof(unsigned int),
			&meshInfo.indices[0],
			meshInfo.isDynamic ? GL_DYNAMIC_DRAW : GL_STATIC_DRAW
		);
		VAOCollection.back().useIndices = true;
		VAOCollection.back().pointAmount = meshInfo.indices.size();
	}
	else
	{
		VAOCollection.back().pointAmount = meshInfo.vert.size();
	}

	stride *= sizeof(float);

	size_t step = 0;
	for (int i = 0; i < steps.size(); ++i)
	{
		step = !i ? 0 : (step + steps[i - 1]);
		glEnableVertexAttribArray(i);
		glVertexAttribPointer(i, steps[i], GL_FLOAT, false, stride, (void*)(step * sizeof(float)));
	}

	VAOCollection.back().shader = meshInfo.shaderProgram;
	VAOCollection.back().textures = meshInfo.textures;

	if (meshInfo.behaviour)
	{
		VAOCollection.back().behaviour = meshInfo.behaviour;
	}
	//glBindBuffer(GL_ARRAY_BUFFER, 0);
	//glBindVertexArray(0);

	int polygons = VAOCollection.back().pointAmount / 3;
	std::cout << "Mesh with " << VAOCollection.back().pointAmount << " point(s) / " << polygons << " polygons created\n";
}

void LGL::GetMeshFromFile(const std::string& file, std::vector<Vertex>& vertexes, std::vector<unsigned int>& indeces)
{
	std::ifstream reader(file);

	if (!reader)
	{
		std::cout << "Could not open mesh file " << file << '\n';
	}
	
	std::map<std::string, int> typeNames
	{
		{"v", 0},
		{"vt", 1},
		{"vn", 2}
	};

	int fileSection = 0;

	std::string line;
	std::string geoType;
	std::string value;
	std::string indexValue;

	std::vector<Vertex> allVertexes;
	std::unordered_map<std::string, int> objIndeces;
	int indeceCount = 0;
	std::vector<Vertex> resVertexes;
	std::vector<int> indexes;


	std::vector<int> meshIndexer { 0, 0, 0 };
	std::vector<unsigned int> resIndeces;

	auto parseOneObjIndece = [](const std::string& value)
	{
		std::vector<int> indexes;
		std::string collected;

		for (auto& i : value)
		{
			if (i == '/')
			{
				indexes.push_back(std::stoi(collected) - 1);
				collected = "";
			}
			else
			{
				collected += i;
			}
		}

		indexes.push_back(std::stoi(collected) - 1);

		return indexes;
	};

	while (std::getline(reader, line))
	{
		switch (fileSection)
		{
		case 0:
		{
			if (line[0] == 'v')
			{
				++fileSection;
			}
			else 
			{
				break;
			}
		}
		case 1:
		{
			if (line[0] == 'f')
			{
				++fileSection;
			}
			else if(line[0] == 'v')
			{
				geoType = "";
				value = "";
				glm::vec3 convertedValues;
				int indexOfValueToCollect = 0;

				for (int i = 0; i < line.size() + 1 && indexOfValueToCollect < 4; ++i)
				{
					if (line[i] == ' ' || line[i] == '\0')
					{
						if (indexOfValueToCollect == 0)
						{
							geoType = value;
						}
						else
						{
							convertedValues[indexOfValueToCollect - 1] = std::stof(value);
						}

						++indexOfValueToCollect;
						value = "";
					}
					else
					{
						value += line[i];
					}
				}

				if (meshIndexer[typeNames[geoType]] >= allVertexes.size())
				{
					allVertexes.push_back(Vertex{});
				}

				Vertex& vertexToSet = allVertexes[meshIndexer[typeNames[geoType]]];

				switch (typeNames[geoType])
				{
				case 0:
					vertexToSet.Position = convertedValues;
					break;
				case 1:
					vertexToSet.TexCoords = convertedValues;
					break;
				case 2:
					vertexToSet.Normal = convertedValues;
					break;
				}

				++meshIndexer[typeNames[geoType]];
				break;
			}
		}
		case 2:
		{
			int currentCoordType = 0;

			if (line[0] == 'f')
			{
				for (int i = 2; i < line.size() + 1; ++i)
				{
					if (line[i] == ' ' || line[i] == '\0')
					{
						if (objIndeces.find(value) == objIndeces.end())
						{
							resIndeces.push_back(indeceCount);
							objIndeces[value] = indeceCount++;

							indexes = parseOneObjIndece(value);
							resVertexes.push_back(
								Vertex{
									allVertexes[indexes[0]].Position,
									allVertexes[indexes[1]].TexCoords,
									allVertexes[indexes[2]].Normal
								}
							);
						}
						else 
						{
							resIndeces.push_back(objIndeces[value]);
						}

						value = "";
					}
					else
					{
						value += line[i];
					}
				}
			}
			break;
		}
		}
	}

	vertexes = resVertexes;
	indeces = resIndeces;
}

bool LGL::CompileShader(const std::string& name)
{
	using AcceptableShaderCode = const char* const;

	std::string currentName = name;

	if (currentName == "")
	{
		currentName = lastShader;
	}

	if (shaderInfoCollection.find(currentName) == shaderInfoCollection.end())
	{
		std::cout << "Shader by this name is not found\n";
		return false;
	}

	AcceptableShaderCode shaderToC = shaderInfoCollection[currentName].shaderCode.c_str();

	Shader* newShader = &shaderInfoCollection[currentName].shaderId;

	glShaderSource(*newShader, 1, &shaderToC, nullptr);
	glCompileShader(*newShader);

	shaderInfoCollection[currentName].compiled = true;

	return HandleGLError(*newShader, 0); // imporve
}

bool LGL::LoadShader(const std::string& code, const std::string& type, const std::string& name)
{
	shaderInfoCollection.emplace(
		name,
		ShaderInfo{
			glCreateShader(shaderTypeChoice[type]),
			code
		}
	);

	std::cout << "Shader " << lastShader << " loaded\n";

	return true;
}

bool LGL::LoadShaderFromFile(const std::string& file, const std::string& shaderName)
{
	std::string shader; // change to stringstream
	std::string line;

	std::ifstream reader(file);

	if (!reader)
	{
		std::cout << "Could not load file\n";
		return false;
	}

	while (std::getline(reader, line))
	{
		shader += (line + '\n');
	}

	size_t typePos = file.find('.') + 1;
	std::string shaderType = file.substr(typePos);

	if (shaderTypeChoice.find(shaderType) == shaderTypeChoice.end())
	{
		std::cout << "Unknown shader type\n";
		return false;
	}

	lastShader = shaderName == "" ? file : shaderName;

	return LoadShader(shader, shaderType, lastShader);
}

bool LGL::LoadTextureFromFile(const std::string& file, const std::string& textureName)
{
	int textureWidth;
	int textureHeight;
	int nrChannels;
	GLenum format;

	TextureData textureData = stbi_load(file.c_str(), &textureWidth, &textureHeight, &nrChannels, 0);
	if (!textureData)
	{
		std::cout << "Failed to load texture\n";
		return false;
	}
	else
	{
		switch (nrChannels)
		{
		case 1:
			format = GL_RED;
			break;
		case 3:
			format = GL_RGB;
			break;
		case 4:
			format = GL_RGBA;
			break;
		default:
			std::cout << "Unknown format\n";
			return false;
		}
	}
	
	lastTexture = textureName == "" ? file : textureName;

	textureCollection.emplace(
		lastTexture,
		TextureInfo{
			Texture(),
			textureData,
			format,
			textureWidth,
			textureHeight,
			nrChannels,
		}
	);

	std::cout << "Loaded texture " << lastTexture << " from " << file << '\n';

	return true;
}

bool LGL::ConfigureTexture(const std::string& textureName, const TextureParams& textureParams)
{
	if (textureCollection.find(textureName) == textureCollection.end())
	{
		std::cout << "Can't find the texture\n";
		return false;
	}

	TextureInfo* newTextureInfo = &textureCollection.at(textureName);
	Texture* newTexture = &newTextureInfo->textureId;
	glGenTextures(1, newTexture);
	glBindTexture(GL_TEXTURE_2D, *newTexture);

	float color[] {
		textureParams.color.r,
		textureParams.color.g,
		textureParams.color.b,
		textureParams.color.a,
	};

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, static_cast<int>(textureParams.overlay));
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, static_cast<int>(textureParams.overlay));

	glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, color);
	if (textureParams.createMipmaps) 
	{
		int glMipParams[2][2]
		{
			{ GL_LINEAR_MIPMAP_LINEAR, GL_LINEAR_MIPMAP_NEAREST },
			{ GL_NEAREST_MIPMAP_LINEAR, GL_NEAREST_MIPMAP_NEAREST}
		};

		glGenerateMipmap(GL_TEXTURE_2D);

		glTexParameteri(
			GL_TEXTURE_2D, 
			GL_TEXTURE_MIN_FILTER, 
			glMipParams[textureParams.mipmapBFConfig.minFilter][textureParams.BFConfig.minFilter]
		);
		glTexParameteri(
			GL_TEXTURE_2D,
			GL_TEXTURE_MAG_FILTER,
			glMipParams[textureParams.mipmapBFConfig.maxFilter][textureParams.BFConfig.maxFilter]
		);
	}
	else 
	{
		int glParams[]{ GL_LINEAR, GL_NEAREST };

		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, glParams[textureParams.BFConfig.minFilter]);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, glParams[textureParams.BFConfig.maxFilter]);
	}

	glTexImage2D(
		GL_TEXTURE_2D, 
		0, 
		newTextureInfo->textureFormat,
		newTextureInfo->width, 
		newTextureInfo->height, 
		0, 
		newTextureInfo->textureFormat, 
		GL_UNSIGNED_BYTE, 
		newTextureInfo->textureData
	);

	//stbi_image_free(newTextureInfo.textureData);

	std::cout << "Texture " << textureName << " configured\n";

	return true;
}

bool LGL::ConfigureTexture(const TextureParams& textureParams)
{
	return ConfigureTexture(lastTexture, textureParams);
}

bool LGL::CreateShaderProgram(const std::string& name, const std::vector<std::string>& shaderNames)
{	
	shaderProgramCollection.emplace(name, glCreateProgram());
	ShaderProgram* newShaderProgram = &shaderProgramCollection[name];

	for (auto& shaderInfo : shaderInfoCollection)
	{
		if (shaderInfo.second.compiled && (shaderNames.empty() || std::find(shaderNames.begin(), shaderNames.end(), shaderInfo.first) != std::end(shaderNames)))
		{
			glAttachShader(*newShaderProgram, shaderInfo.second.shaderId);
			shaderInfo.second.compiled = false;
		}
	}
	glLinkProgram(*newShaderProgram);

	// rewrite
	int success;
	char log[512];

	glGetProgramiv(*newShaderProgram, GL_LINK_STATUS, &success);
	if (!success) {
		glGetProgramInfoLog(*newShaderProgram, 512, NULL, log);
		std::cout << "ERROR::SHADER::PROGRAM::LINKING_FAILED\n" << log << std::endl;

		return false;
	}
	
	lastProgram = name;

	std::cout << "Shader program: " << lastProgram << " created\n";

	//glUseProgram(*newShaderProgram);
	return true;
}

void LGL::SetInteractable(int key, std::function<void()> func)
{
	interactCollection[key] = func;
	
	std::cout << "Interactable for keyId " << key << " set\n";
}

void LGL::CursorPositionCallback(GLFWwindow* window, double xpos, double ypos)
{
	if (cursorPositionFunc)
	{
		cursorPositionFunc(xpos, ypos);
	}
}

void LGL::ScrollCallback(GLFWwindow* window, double xoffset, double yoffset)
{
	if (scrollCallbackFunc)
	{
		scrollCallbackFunc(xoffset, yoffset);
	}
}
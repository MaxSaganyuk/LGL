#include <iostream>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cctype>

#include "stb_image.h"

#define LGL_EXPORT
#include "LGL.h"
#include "stdEx/mapEx.h"

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include "GLExecutor.h"

#include "ContextManager.h"
#define ContextLock ContextManager<GLFWwindow> mux(window, [this](GLFWwindow* context){ glfwMakeContextCurrent(context); });
std::recursive_mutex ContextManager<GLFWwindow>::rMutex;
size_t ContextManager<GLFWwindow>::counter = 0;

using namespace LGLStructs;
using namespace LGLEnums;

std::function<void(double, double)> LGL::cursorPositionFunc = nullptr;
std::function<void(double, double)> LGL::scrollCallbackFunc = nullptr;

std::map<std::string, LGL::ShaderType> LGL::shaderTypeChoice =
{
	{"vert", GL_VERTEX_SHADER},
	{"frag", GL_FRAGMENT_SHADER}
};

const std::vector<int> LGL::LGLEnumInterpreter::DepthTestModeInter =
{
	{0, GL_ALWAYS, GL_NEVER, GL_LESS, GL_GREATER, GL_EQUAL, GL_NOTEQUAL, GL_LEQUAL, GL_GEQUAL}
};

const std::vector<int> LGL::LGLEnumInterpreter::TextureOverlayTypeInter =
{
	{GL_REPEAT, GL_MIRRORED_REPEAT, GL_CLAMP_TO_EDGE, GL_CLAMP_TO_BORDER}
};


LGL::LGL()
{
	background = { 0, 0, 0, 1 };
	currentVAOToRender = {};
	window = nullptr;

	stbi_set_flip_vertically_on_load(true);

	std::cout << "Created LambdaGL instance\n";
}

LGL::~LGL()
{
	for (auto& shader : shaderInfoCollection)
	{
		GLSafeExecute(glDeleteShader, shader.second.shaderId);
	}

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

	if (!glfwGetCurrentContext())
	{
		std::cerr << "Context does not set correctly\n";
	}

	std::cout << "Created a GLFW window by address " << window << '\n';

	return true;
}

void LGL::InitOpenGL(int major, int minor)
{
	glfwInit();
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, major);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, minor);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

	std::cout << "OpenGL initialized\n";
}

bool LGL::InitGLAD()
{
	ContextLock

	if (!gladLoadGLLoader(reinterpret_cast<GLADloadproc>(glfwGetProcAddress)))
	{
		std::cout << "Failed to init GLAD\n";
		return false;
	}

	SetDepthTest(DepthTestMode::Less);

	std::cout << "GLAD initialized\n";

	return true;
}

void LGL::InitCallbacks()
{
	ContextLock

	glfwSetFramebufferSizeCallback(window, FramebufferSizeCallback);
	glfwSetErrorCallback(GLFWErrorCallback);
}

void LGL::TerminateOpenGL()
{
	glfwTerminate();

	std::cout << "Terminate OpenGL\n";
}

void LGL::SetDepthTest(DepthTestMode depthTestMode)
{
	ContextLock

	if (depthTestMode == DepthTestMode::Disable)
	{
		GLSafeExecute(glDisable, GL_DEPTH_TEST);
	}
	else
	{
		GLSafeExecute(glEnable, GL_DEPTH_TEST);
		GLSafeExecute(glDepthFunc, static_cast<GLenum>(LGLEnumInterpreter::DepthTestModeInter[static_cast<GLenum>(depthTestMode)]));
	}
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
	ContextLock

	int attr;
	GLSafeExecute(glGetIntegerv, GL_MAX_VERTEX_ATTRIBS, &attr);
	
	std::cout << "Max amount of vertex attributes: " << attr << '\n';
	
	return attr;
}

void LGL::ProcessInput()
{
	for (auto& interact : interactCollection)
	{
		int retCode = glfwGetKey(window, interact.first);

		if (retCode == GLFW_PRESS && interact.second.first)
		{
			interact.second.first();
		}
		else if (retCode == GLFW_RELEASE && interact.second.second)
		{
			interact.second.second();
		}
	}
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
			GLSafeExecute(glDrawArrays, GL_TRIANGLES, 0, currentVAOToRender.pointAmount);
		}
		else
		{
			GLSafeExecute(glDrawElements, GL_TRIANGLES, currentVAOToRender.pointAmount, GL_UNSIGNED_INT, nullptr);
		}
	}
}

void LGL::RunRenderingCycle(std::function<void()> additionalSteps)
{
	while (!glfwWindowShouldClose(window))
	{
		ContextLock

		ProcessInput();

		GLSafeExecute(glClearColor, background.r, background.g, background.b, background.a);
		GLSafeExecute(glClear, GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		if (additionalSteps)
		{
			additionalSteps();
		}

		for (auto& currentVAO : VAOCollection)
		{
			currentVAOToRender = currentVAO;

			std::string shaderProgramToCheck = currentVAO.meshInfo->shaderProgram;
			if (lastProgram != shaderProgramToCheck)
			{
				lastProgram = shaderProgramToCheck;
				GLSafeExecute(glUseProgram, shaderProgramCollection[lastProgram]);
			}

			GLSafeExecute(glBindVertexArray, currentVAO.vboId);

			if (currentVAO.meshInfo->mesh.textures.size())
			{
				for (int i = 0; i < currentVAO.meshInfo->mesh.textures.size(); ++i)
				{
					if (textureCollection.find(currentVAO.meshInfo->mesh.textures[i].name) != textureCollection.end())
					{
						TextureInfo& textureInfo = textureCollection.at(currentVAO.meshInfo->mesh.textures[i].name);
						GLSafeExecute(glActiveTexture, GL_TEXTURE0 + i);
						GLSafeExecute(glBindTexture, GL_TEXTURE_2D, textureInfo.textureId);
					}
				}
			}
			else
			{
				GLSafeExecute(glActiveTexture, GL_TEXTURE0 + 0);
				GLSafeExecute(glBindTexture, GL_TEXTURE_2D, 0);
			}

			std::function<void()> behaviourToCheck = currentVAO.meshInfo->behaviour;
			if (behaviourToCheck)
			{
				behaviourToCheck();
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

void LGL::CreateMesh(MeshInfo& meshInfo)
{
	ContextLock

	std::vector<int> steps{ 3, 3, 3, 3, 3 };

	VAOCollection.push_back({0, false});
	VAO* newVAO = &VAOCollection.back().vboId;
	GLSafeExecute(glGenVertexArrays, 1, newVAO);
	GLSafeExecute(glBindVertexArray, *newVAO);

	VBOCollection.push_back(VBO());
	VBO* newVBO = &VBOCollection.back();

	GLSafeExecute(glGenBuffers, 1, newVBO);
	GLSafeExecute(glBindBuffer, GL_ARRAY_BUFFER, *newVBO);
	GLSafeExecute(
		glBufferData,
		GL_ARRAY_BUFFER, 
		meshInfo.mesh.vert.size() * sizeof(Vertex),
		&meshInfo.mesh.vert[0],
		meshInfo.isDynamic ? GL_DYNAMIC_DRAW : GL_STATIC_DRAW
	);
	size_t stride = 0;
	for (int i = 0; i < steps.size(); ++i)
	{
		stride += steps[i];
	}

	if (!meshInfo.mesh.indices.empty())
	{
		EBOCollection.push_back(EBO());
		EBO* newEBO = &EBOCollection.back();

		GLSafeExecute(glGenBuffers, 1, newEBO);
		GLSafeExecute(glBindBuffer, GL_ELEMENT_ARRAY_BUFFER, *newEBO);
		GLSafeExecute(
			glBufferData,
			GL_ELEMENT_ARRAY_BUFFER, 
			meshInfo.mesh.indices.size() * sizeof(unsigned int),
			&meshInfo.mesh.indices[0],
			meshInfo.isDynamic ? GL_DYNAMIC_DRAW : GL_STATIC_DRAW
		);
		VAOCollection.back().useIndices = true;
		VAOCollection.back().pointAmount = meshInfo.mesh.indices.size();
	}
	else
	{
		VAOCollection.back().pointAmount = meshInfo.mesh.vert.size();
	}

	VAOCollection.back().meshInfo = &meshInfo;

	stride *= sizeof(float);

	size_t step = 0;
	for (int i = 0; i < steps.size(); ++i)
	{
		step = !i ? 0 : (step + steps[i - 1]);
		GLSafeExecute(glEnableVertexAttribArray, i);
		GLSafeExecute(glVertexAttribPointer, i, steps[i], GL_FLOAT, false, stride, (void*)(step * sizeof(float)));
	}
	//glBindBuffer(GL_ARRAY_BUFFER, 0);
	//glBindVertexArray(0);

	int polygons = VAOCollection.back().pointAmount / 3;
	std::cout << "Mesh with " << VAOCollection.back().pointAmount << " point(s) / " << polygons << " polygons created\n";
}

void LGL::CreateModel(LGLStructs::ModelInfo& model)
{
	for (auto& mesh : model.meshes)
	{
		CreateMesh(mesh);
	}
}

#ifdef ENABLE_OLD_MODEL_IMPORT

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
	std::string indexValue;

	std::vector<Vertex> allVertexes;
	std::unordered_map<std::string, int> objIndeces;
	int indeceCount = 0;
	std::vector<Vertex> resVertexes;
	std::map<int, int> indexes;

	std::vector<int> meshIndexer { 0, 0, 0 };
	std::vector<unsigned int> resIndeces;

	auto parseOneObjIndece = [](const std::string& value)
	{
		std::map<int, int> indexes;
		std::string collected;
		int count = 0;
			
		for (auto& i : value)
		{
			if (i == '/')
			{
				indexes[count++] = collected == "" ? 0 : std::stoi(collected) - 1;
				collected = "";
			}
			else
			{
				collected += i;
			}
		}

		indexes[count++] = collected == "" ? 0 : std::stoi(collected) - 1;

		return indexes;
	};

	auto CheckForFileSectionChange = [&line, &fileSection]()
	{
		stdEx::map<char, int> charToFilesection;
		charToFilesection['v'] = 1;
		charToFilesection['f'] = 2;
		charToFilesection.SetDefaultValue(0);

		fileSection = charToFilesection.at(line[0]);
	};

	auto CheckVertexVectors = [&line, &fileSection, &meshIndexer, &typeNames, &allVertexes]()
	{
		if (line[0] == 'v')
		{
			std::string geoType = "";
			std::string value = "";
			glm::vec3 convertedValues;
			int indexOfValueToCollect = 0;

			for (int i = 0; i < line.size() + 1 && indexOfValueToCollect < 4; ++i)
			{
				if (!value.empty() && (line[i] == ' ' || line[i] == '\0'))
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
		}
	};

	auto CheckIndecesAndFormResult = 
		[&line, &objIndeces, &resIndeces, &indeceCount, &indexes, &parseOneObjIndece, &resVertexes, &allVertexes]()
	{
		int currentCoordType = 0;
		std::string value = "";

		if (line[0] == 'f')
		{
			for (int i = 2; i < line.size() + 1; ++i)
			{
				if (!value.empty() && (line[i] == ' ' || line[i] == '\0'))
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
	};

	stdEx::map<int, std::function<void()>> processFileFuncs;
	processFileFuncs[1] = CheckVertexVectors;
	processFileFuncs[2] = CheckIndecesAndFormResult;
	processFileFuncs.SetDefaultValue([](){});

	while (std::getline(reader, line))
	{
		CheckForFileSectionChange();
		processFileFuncs.at(fileSection)();
	}

	vertexes = resVertexes;
	indeces = resIndeces;
}
#endif

bool LGL::CompileShader(const std::string& name)
{
	using AcceptableShaderCode = const char* const;

	ContextLock

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

	GLSafeExecute(glShaderSource, *newShader, 1, &shaderToC, nullptr);
	bool shaderCompiled = GLSafeExecute(glCompileShader, *newShader);

	shaderInfoCollection[currentName].compiled = true;

	return shaderCompiled; // imporve
}

bool LGL::LoadShader(const std::string& code, const std::string& type, const std::string& name)
{
	ContextLock

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
	ContextLock

	if (textureCollection.find(textureName) == textureCollection.end())
	{
		std::cout << "Can't find the texture\n";
		return false;
	}

	TextureInfo* newTextureInfo = &textureCollection.at(textureName);
	Texture* newTexture = &newTextureInfo->textureId;
	GLSafeExecute(glGenTextures, 1, newTexture);
	GLSafeExecute(glBindTexture, GL_TEXTURE_2D, *newTexture);

	float color[] {
		textureParams.color.r,
		textureParams.color.g,
		textureParams.color.b,
		textureParams.color.a,
	};

	GLSafeExecute(
		glTexParameteri,
		GL_TEXTURE_2D, 
		GL_TEXTURE_WRAP_S, 
		LGLEnumInterpreter::TextureOverlayTypeInter[static_cast<int>(textureParams.overlay)]
	);
	GLSafeExecute(
		glTexParameteri,
		GL_TEXTURE_2D, 
		GL_TEXTURE_WRAP_T, 
		LGLEnumInterpreter::TextureOverlayTypeInter[static_cast<int>(textureParams.overlay)]
	);

	GLSafeExecute(glTexParameterfv, GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, color);
	if (textureParams.createMipmaps) 
	{
		int glMipParams[2][2]
		{
			{ GL_LINEAR_MIPMAP_LINEAR, GL_LINEAR_MIPMAP_NEAREST },
			{ GL_NEAREST_MIPMAP_LINEAR, GL_NEAREST_MIPMAP_NEAREST}
		};

		GLSafeExecute(glGenerateMipmap, GL_TEXTURE_2D);

		GLSafeExecute(
			glTexParameteri,
			GL_TEXTURE_2D, 
			GL_TEXTURE_MIN_FILTER, 
			glMipParams[textureParams.mipmapBFConfig.minFilter][textureParams.BFConfig.minFilter]
		);
		GLSafeExecute(
			glTexParameteri,
			GL_TEXTURE_2D,
			GL_TEXTURE_MAG_FILTER,
		   glMipParams[textureParams.mipmapBFConfig.maxFilter][textureParams.BFConfig.maxFilter]
		);
	}
	else 
	{
		int glParams[]{ GL_LINEAR, GL_NEAREST };

		GLSafeExecute(glTexParameteri, GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, glParams[textureParams.BFConfig.minFilter]);
		GLSafeExecute(glTexParameteri, GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, glParams[textureParams.BFConfig.maxFilter]);
	}

	GLSafeExecute(
		glTexImage2D,
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

	//stbi_image_free(newTextureInfo->textureData);

	std::cout << "Texture " << textureName << " configured\n";

	return true;
}

bool LGL::ConfigureTexture(const TextureParams& textureParams)
{
	return ConfigureTexture(lastTexture, textureParams);
}

bool LGL::CreateShaderProgram(const std::string& name, const std::vector<std::string>& shaderNames)
{	
	ContextLock

	shaderProgramCollection.emplace(name, glCreateProgram());
	ShaderProgram* newShaderProgram = &shaderProgramCollection[name];

	for (auto& shaderInfo : shaderInfoCollection)
	{
		if (shaderInfo.second.compiled && (shaderNames.empty() || std::find(shaderNames.begin(), shaderNames.end(), shaderInfo.first) != std::end(shaderNames)))
		{
			GLSafeExecute(glAttachShader, *newShaderProgram, shaderInfo.second.shaderId);
			shaderInfo.second.compiled = false;
		}
	}
	GLSafeExecute(glLinkProgram, *newShaderProgram);

	// rewrite
	int success;
	char log[512];

	GLSafeExecute(glGetProgramiv, *newShaderProgram, GL_LINK_STATUS, &success);
	
	lastProgram = name;

	std::cout << "Shader program: " << lastProgram << " created\n";

	//glUseProgram(*newShaderProgram);
	return true;
}

bool LGL::LoadAndCompileShaders(const std::string& dir, const std::vector<std::string>& names)
{
	bool res = true;

	for (const auto& name : names)
	{
		for (const auto& shaderFileType : shaderTypeChoice)
		{
			res &= LoadShaderFromFile(dir + name + '.' + shaderFileType.first);
			res &= CompileShader();
		}

		res &= CreateShaderProgram(name);
	}

	return res;
}

bool LGL::LoadAndConfigureTextures(const std::string& dir, const std::vector<std::string>& files, const std::vector<TextureParams>& texParamVect)
{
	bool res = true;

	if ((files.size() == texParamVect.size()) || texParamVect.empty())
	{
		for (size_t i = 0; i < files.size(); ++i)
		{
			res &= LoadTextureFromFile(dir + files[i], files[i]);
			res &= ConfigureTexture(!texParamVect.empty() ? texParamVect[i] : TextureParams());
		}

		return res;
	}
	
	return false;
}

void LGL::SetInteractable(unsigned char key, const OnPressFunction& preFunc, const OnReleaseFunction& relFunc)
{
	interactCollection[std::toupper(key)] = {preFunc, relFunc};
	
	std::cout << "Interactable for keyId " << key << " set\n";
}

void LGL::GLFWErrorCallback(int errorCode, const char* description)
{
	int x = errorCode;
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

void LGL::SetAssetOnOpenGLFailure(bool value)
{
	GLExecutor::SetAssertOnFailure(value);
	std::cout << "AssertOnFailure has been set to " << value << '\n';
}

const std::unordered_map<std::type_index, std::function<void(int, void*)>> uniformValueLocators
{
	{ typeid(int), [](int uniformValueLocation, void* value) { glUniform1i(uniformValueLocation, *reinterpret_cast<int*>(value)); } },
	{ typeid(float), [](int uniformValueLocation, void* value) { glUniform1f(uniformValueLocation, *reinterpret_cast<float*>(value)); } },

	{ typeid(glm::vec3), [](int uniformValueLocation, void* value)
	{
		glm::vec3* coords = reinterpret_cast<glm::vec3*>(value);
		glUniform3f(uniformValueLocation, coords->x, coords->y, coords->z);
	} },

	{ typeid(glm::vec4), [](int uniformValueLocation, void* value)
	{
		glm::vec4* coords = reinterpret_cast<glm::vec4*>(value);
		glUniform4f(uniformValueLocation, coords->x, coords->y, coords->z, coords->w);
	} },

	{ typeid(glm::mat4), [](int uniformValueLocation, void* value)
	{
		glUniformMatrix4fv(uniformValueLocation, 1, GL_FALSE, glm::value_ptr(*reinterpret_cast<glm::mat4*>(value)));
	} }

};


int LGL::CheckUnifromValueLocation(const std::string& valueName, const std::string& shaderProgramName)
{
	ShaderProgram shaderProgramToUse = shaderProgramCollection[shaderProgramName == "" ? lastProgram : shaderProgramName];

	int uniformValueLocation = glGetUniformLocation(shaderProgramToUse, valueName.c_str());

	if (uniformValueLocation == -1)
	{
		if (std::find(uniformErrorAntispam.begin(), uniformErrorAntispam.end(), valueName) == std::end(uniformErrorAntispam))
		{
			std::cout << "[ERROR] Shader value " + valueName << " could not be located\n";
			uniformErrorAntispam.push_back(valueName);
		}
	}

	return uniformValueLocation;
}

template<typename Type>
bool LGL::SetShaderUniformValue(const std::string& valueName, Type&& value, bool render, const std::string& shaderProgramName)
{
	int uniformValueLocation = CheckUnifromValueLocation(valueName, shaderProgramName);
	if (uniformValueLocation == -1)
	{
		return false;
	}

	uniformValueLocators.at(typeid(Type))(uniformValueLocation, &value);

	if (render)
	{
		Render();
	}

	return true;
}

#define ShaderUniformValueExplicit(Type) \
template LGL_API bool LGL::SetShaderUniformValue<Type>(const std::string& valueName, Type&& value, bool render, const std::string& shaderProgramName); \
template LGL_API bool LGL::SetShaderUniformValue<Type&>(const std::string& valueName, Type& value, bool render, const std::string& shaderProgramName);

ShaderUniformValueExplicit(int)
ShaderUniformValueExplicit(float)
ShaderUniformValueExplicit(glm::vec3)
ShaderUniformValueExplicit(glm::vec4)
ShaderUniformValueExplicit(glm::mat4)

#undef ShaderUniformValueExplicit
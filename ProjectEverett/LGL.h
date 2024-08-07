#pragma once

#include <string>
#include <vector>
#include <map>
#include <unordered_map>
#include <functional>
#include <thread>
#include <mutex>
#include <typeindex>
#include <glad/include/glad/glad.h>
#include <GLFW/glfw3.h>

#include "LGLStructs.h"

#define CALLBACK static void

/*
	Lambda (Open) GL

	Todo:
	Add frame limiter (fix frame dependent camera movement speed)
	Maybe improve SetShaderUniformValue for arrays
*/
class LGL
{
// Alias section
private:
	using VBO = unsigned int; // Vertex Buffer Object
	using VAO = unsigned int; // Vertex Array Object
	using EBO = unsigned int; // Element Buffer Object

	using Shader = unsigned int;
	using ShaderCode = std::string;
	using ShaderType = int;
	using ShaderProgram = unsigned int;

	using Texture = unsigned int;
	using TextureData = unsigned char*;

	using OnPressFunction = std::function<void()>;
	using OnReleaseFunction = std::function<void()>;

// Structs for internal use
	struct VAOInfo
	{
		VAO vboId = 0;
		size_t pointAmount;
		bool useIndices;
		std::string modelParent;
		LGLStructs::MeshInfo* meshInfo = nullptr;
	};

	struct ShaderInfo
	{
		Shader shaderId;
		ShaderCode shaderCode;
		bool compiled = true;
	};

	struct TextureInfo
	{
		Texture textureId;
		TextureData textureData;
		GLenum textureFormat;
		int width;
		int height;
		int nrChannels;
	};

public:

	LGL();
	~LGL();

	bool CreateWindow(const int height, const int width, const std::string& title);
	
	// Additional steps to rendering can be passed as a function pointer or a lambda
	// It is expected to get a lambda with a script for camera behaviour
	void RunRenderingCycle(std::function<void()> additionalSteps = nullptr);
	void SetStaticBackgroundColor(const glm::vec4& rgba);

	// Creates a VAO, VBO and (if indices are given) EBO
	// Must accept amount of steps for
	// You can pass a lambda to describe general behaviour for your shape
	// Behaviour function will be called inside the rendering cycle
	// exaclty at it's VAO binding.
	// If you need several shapes with similar behaviour, it's possible 
	// to render additional ones with SetShaderUniformValue function inside
	// the lambda script without creating additional VAOs
#ifdef LGL_DISABLEASSIMP
	void GetMeshFromFile(const std::string& file, std::vector<LGLStructs::Vertex>& vertexes, std::vector<unsigned int>& indeces);
#else	
	void CreateMesh(LGLStructs::MeshInfo& meshInfo);
	void CreateModel(LGLStructs::ModelInfo& model);
	void GetModelFromFile(const std::string& file, LGLStructs::ModelInfo& model);
#endif
	// If no name is given will compile last loaded shader
	bool CompileShader(const std::string& name = "");
	bool LoadShader(const std::string& code, const std::string& type, const std::string& name);
	bool LoadShaderFromFile(const std::string& file, const std::string& shaderName = "");

	// If no list of shaders is provided, will create a program with all compiled shaders
	bool CreateShaderProgram(const std::string& name, const std::vector<std::string>& shaderVector = {});

	// If shader file names can be identical to shader program name, general load and compile can be used
	bool LoadAndCompileShaders(const std::string& dir, const std::vector<std::string>& names);

	bool LoadTextureFromFile(const std::string& file, const std::string& textureName = "");
	bool ConfigureTexture(const std::string& textureName, const LGLStructs::TextureParams& textureParams = {});
	bool ConfigureTexture(const LGLStructs::TextureParams& textureParams = {});
	bool LoadAndConfigureTextures(const std::string& dir, const std::vector<std::string>& files, const std::vector<LGLStructs::TextureParams>& texParamVect = {});

	static void InitOpenGL(int major, int minor);

	bool InitGLAD();
	void InitCallbacks();
	static void TerminateOpenGL();

	enum class DepthTestMode
	{
		Disable,
		Always         = GL_ALWAYS,
		Never          = GL_NEVER,
		Less           = GL_LESS,
		Greater        = GL_GREATER,
		Equal          = GL_EQUAL,
		NotEqual       = GL_NOTEQUAL,
		LessOrEqual    = GL_LEQUAL,
		GreaterOrEqual = GL_GEQUAL
	};

	void SetDepthTest(DepthTestMode depthTestMode);

	int GetMaxAmountOfVertexAttr();

	void CaptureMouse();

	void SetInteractable(int key, const OnPressFunction& preFunc, const OnReleaseFunction& relFunc = nullptr);

	void SetCursorPositionCallback(std::function<void(double, double)> callbackFunc);
	void SetScrollCallback(std::function<void(double, double)> callbackFunc);

	template<class Type>
	bool SetShaderUniformValue(const std::string& valueName, Type&& value, bool render = false, const std::string& shaderProgramName = "");

	template<typename Type>
	bool SetShaderUniformStruct(const std::string& structName, const std::vector<std::string>& valueNames, Type value);

	template<typename Type, typename... Types>
	bool SetShaderUniformStruct(const std::string& structName, const std::vector<std::string>& valueNames, Type value, Types... values);

private:

	int CheckUnifromValueLocation(const std::string& valueName, const std::string& shaderProgramName);

	template<typename Type>
	bool SetShaderUniformStructImpl(const std::string& structName, const std::vector<std::string>& valueNames, size_t i, Type value);

	template<typename Type, typename... Types>
	bool SetShaderUniformStructImpl(const std::string& structName, const std::vector<std::string>& valueNames, size_t i, Type value, Types... values);

	// Callbacks
	CALLBACK FramebufferSizeCallback(GLFWwindow* window, int width, int height);
	
	CALLBACK CursorPositionCallback(GLFWwindow* window, double xpos, double ypos);
	static std::function<void(double, double)> cursorPositionFunc;
	
	CALLBACK ScrollCallback(GLFWwindow* window, double xoffset, double yoffset);
	static std::function<void(double, double)> scrollCallbackFunc;

	void ProcessInput();
	bool HandleGLError(const unsigned int& glId, int statusToCheck);
	void Render();

	GLFWwindow* window;

	glm::vec4 background;

	VAOInfo currentVAOToRender;
	std::vector<VBO> VBOCollection;
	std::vector<VAOInfo> VAOCollection;
	std::vector<EBO> EBOCollection;

	// Shader
	static std::map<std::string, ShaderType> shaderTypeChoice;

	std::string lastShader;
	std::map<std::string, ShaderInfo> shaderInfoCollection;
	
	// Shader Program
	std::string lastProgram;
	std::map<std::string, ShaderProgram> shaderProgramCollection;

	// Texture
	std::string lastTexture;
	std::map<std::string, TextureInfo> textureCollection;

	std::vector<std::string> uniformErrorAntispam;

	std::map<size_t, std::pair<OnPressFunction, OnReleaseFunction>> interactCollection;

	static const std::unordered_map<std::type_index, std::function<void(int, void*)>> uniformValueLocators;
};

/*
*  it's possible to use something like std::pair<std::string, Type>, but it makes it to similar to using regular SetShaderUnifromValue repeatedly
*/

template<typename Type>
bool LGL::SetShaderUniformStruct(const std::string& structName, const std::vector<std::string>& valueNames, Type value)
{
	return SetShaderUniformValueImpl(structName, valueNames, 0, value);
}

template<typename Type>
bool LGL::SetShaderUniformStructImpl(const std::string& structName, const std::vector<std::string>& valueNames, size_t i, Type value)
{
	return SetShaderUniformValue(structName + '.' + valueNames[i], value);
}

template<typename Type, typename... Types>
bool LGL::SetShaderUniformStruct(const std::string& structName, const std::vector<std::string>& valueNames, Type value, Types... values)
{
	// In C++20 and after could probably be made as a compile time check
	if (valueNames.size() - 1 != sizeof...(Types))
	{
		assert(false && "Mismatch between amount of valueNames and values");
		return false;
	}

	return SetShaderUniformStructImpl(structName, valueNames, 0, value, values...);
}

template<typename Type, typename... Types>
bool LGL::SetShaderUniformStructImpl(const std::string& structName, const std::vector<std::string>& valueNames, size_t i, Type value, Types... values)
{
	bool res = SetShaderUniformValue(structName + '.' + valueNames[i], value);

	// Can't decide if I should stop processing the whole structure if one of the elements failed.
	// return SetShaderUniformStruct(structName, std::move(valueNames), values...) && res;
	return res && SetShaderUniformStructImpl(structName, valueNames, ++i, values...);
}
#include <time.h>
#include <cmath>
#include <cstdlib>

#include "LGL.h"
#include "Verts.h"

#include "MaterialSim.h"
#include "LightSim.h"
#include "SolidSim.h"
#include "CameraSim.h"

#include "MazeGen.h"

constexpr int windowHeight = 800;
constexpr int windowWidth = 600;

std::vector<glm::vec3> GenerateRandomCubes(int amount)
{
	auto gen = []() -> float
	{
		return (float)(rand() % 200 - 100) / 10;
	};

	std::vector<glm::vec3> vec;

	for (int i = 0; i < amount; ++i)
	{
		vec.push_back(glm::vec3(gen(), gen(), gen()));
	
	}

	return vec;
}

std::vector<glm::vec3> PlaceCubesInAMaze(MazeGen::MazeInfo& maze)
{
	std::vector<glm::vec3> cubesPos;

	for (size_t i = 0; i < maze.matrix.size(); ++i)
	{
		for (size_t j = 0; j < maze.matrix[i].size(); ++j)
		{
			if (!maze.matrix[i][j])
			{
				cubesPos.push_back({ i, 0, j });
			}
		}
	}

	return cubesPos;
}

std::vector<LGL::Vertex> ConvertAVerySpecificFloatPointerToVertexVector(float* ptr, size_t size)
{
	std::vector<LGL::Vertex> vert;

	for (int i = 0; i < size / sizeof(float) / 8; ++i)
	{
		vert.push_back(
			LGL::Vertex{
			{ ptr[i * 8], ptr[i * 8 + 1], ptr[i * 8 + 2] },
			{ ptr[i * 8 + 6], ptr[i * 8 + 7] },
			{ ptr[i * 8 + 3], ptr[i * 8 + 4], ptr[i * 8 + 5]}
			}
		);
	}

	return vert;
}

std::vector<SolidSim> ConvertPosesToSolids(const std::vector<glm::vec3>& posVect)
{
	std::vector<SolidSim> solidVect;

	for (const auto& pos : posVect)
	{
		solidVect.push_back(SolidSim(pos));
	}

	return solidVect;
}

glm::vec3 SelectRandomPlaceInAMaze(const MazeGen::MazeInfo& maze)
{
	bool avalible = false;
	size_t sizeN = maze.matrix.size();
	size_t sizeM = maze.matrix[0].size();
	size_t pickedN = 0;
	size_t pickedM = 0;
	
	while (!avalible)
	{ 
		pickedN = rand() % sizeN;
		pickedM = rand() % sizeM;
		avalible = maze.matrix[pickedN][pickedN];
	}

	return { static_cast<float>(pickedN), 0, static_cast<float>(pickedM) };
}

int main()
{
	srand(static_cast<unsigned int>(time(nullptr)));

	LGL::InitOpenGL(3, 3);

	LGL lgl;

	lgl.CreateWindow(windowHeight, windowWidth, "ProjectEverett");
	lgl.InitGLAD();
	lgl.InitCallbacks();

	lgl.LoadShaderFromFile("lightComb.vert");
	lgl.CompileShader();

	lgl.LoadShaderFromFile("lightComb.frag");
	lgl.CompileShader();

	lgl.CreateShaderProgram("colorsTex");

	lgl.LoadShaderFromFile("lamp.vert");
	lgl.CompileShader();

	lgl.LoadShaderFromFile("lamp.frag");
	lgl.CompileShader();

	lgl.CreateShaderProgram("lamp");

	lgl.LoadTextureFromFile("box.png");
	lgl.ConfigureTexture();

	lgl.LoadTextureFromFile("boxEdge.png");
	lgl.ConfigureTexture();

	lgl.LoadTextureFromFile("extraStuff/coilTex.png");
	lgl.ConfigureTexture();

	lgl.LoadTextureFromFile("extraStuff/coilTex2.png");
	lgl.ConfigureTexture();

	MaterialSim::Material mat = MaterialSim::GetMaterial(MaterialSim::MaterialID::GOLD);
	LightSim::Attenuation atte = LightSim::GetAttenuation(60);

	MazeGen::MazeInfo maze = MazeGen::GenerateMaze(20, 20);
	MazeGen::PrintExitPath(maze);

	CameraSim camera(windowHeight, windowWidth, SelectRandomPlaceInAMaze(maze));
	camera.SetMode(CameraSim::Mode::Fly);

	std::vector<glm::vec3> cubesPos = PlaceCubesInAMaze(maze);
	std::vector<glm::vec3> lightsPos = { {0.0f, 0.0f, 0.0f} };

	std::vector<SolidSim> cubes = ConvertPosesToSolids(cubesPos);
	std::vector<SolidSim> lights = { SolidSim({ 0.0f, 0.0f, 0.0f }, { 0.35f, 0.35f, 0.35f })};
	SolidSim mug = SolidSim(SelectRandomPlaceInAMaze(maze), {0.35f, 0.45f, 0.35f});
	mug.GetPositionVectorAddr().y -= 0.45f;


	auto rotateG = [&lgl, &cubesPos](float ia, float ib)
	{
		static float a = 0;
		static float b = 0;

		a += ia;
		b += ib;

		for (int i = 0; i < cubesPos.size(); ++i)
		{
			glm::mat4 trans = glm::mat4(1.0);
			trans = glm::translate(trans, cubesPos[i]);
			if (!i)
			{
				trans = glm::rotate(trans, 0.0f, glm::vec3(0.0f, 1.0f, 0.0f));
			}
			else
			{
				trans = glm::rotate(trans, (float)sin(glfwGetTime()), glm::vec3(0.0f, 1.0f, 0.0f));
			}
			trans = glm::rotate(trans, glm::radians(a / 25), glm::vec3(-1.0, 0.0f, 0.0f));

			lgl.SetShaderUniformValue("model", trans);
		}
	};

	auto rotateUp = [&rotateG]()
	{
		rotateG(1, 0);
	};

	auto rotateLeft = [&rotateG]()
	{
		rotateG(0, 1);
	};

	auto rotateNo = [&rotateG]()
	{
		rotateG(0, 0);
	};

	glm::vec3 lightColor(1.0f, 1.0f, 1.0f);

	std::vector<std::pair<std::string, std::vector<std::string>>> lightShaderValueNames
	{
		{"material", { "diffuse", "specular", "shininess" }},
		{"pointLight",
			{
				"position", "ambient", "diffuse",
				"specular", "constant", "linear",
				"quadratic"
			}
		},
		{"spotLight",
			{
				"position", "direction", "ambient",
				"diffuse", "specular", "constant",
				"linear", "quadratic", "cutOff",
				"outerCutOff"
			}
		}
	};

	auto lightBeh = [&lgl, &cubes, &lights, &mat, &atte, &camera, &lightShaderValueNames]()
	{
		lgl.SetShaderUniformValue("proj", camera.GetProjectionMatrixAddr());
		lgl.SetShaderUniformValue("view", camera.GetViewMatrixAddr());

		lgl.SetShaderUniformStruct(lightShaderValueNames[0].first, lightShaderValueNames[0].second, 0, 1, mat.shininess);

		//lgl.SetShaderUniformValue("dirLight.direction", lightPos);
		//lgl.SetShaderUniformValue("dirLight.ambient", glm::vec3(0.05f, 0.05f, 0.05f));
		//lgl.SetShaderUniformValue("dirLight.diffuse", glm::vec3(0.4f, 0.4f, 0.4f));
		//lgl.SetShaderUniformValue("dirLight.specular", glm::vec3(0.5f, 0.5f, 0.5f));

		lgl.SetShaderUniformValue("lightAmount", static_cast<int>(lights.size()));
		for (int i = 0; i < lights.size(); ++i)
		{
			std::string accessIndex = lightShaderValueNames[1].first + "[" + std::to_string(i) + "]";

			lgl.SetShaderUniformStruct(
				accessIndex,
				lightShaderValueNames[1].second,
				lights[i].GetPositionVectorAddr(), glm::vec3(0.05f, 0.05f, 0.05f), glm::vec3(0.4f, 0.4f, 0.4f),
				glm::vec3(1.0f, 1.0f, 1.0f), 1.0f, atte.linear,
				atte.quadratic
			);
		}

		lgl.SetShaderUniformStruct(
			lightShaderValueNames[2].first,
			lightShaderValueNames[2].second,
			camera.GetPositionVectorAddr(), camera.GetFrontVectorAddr(), glm::vec3(0.2f, 0.2f, 0.2f),
			glm::vec3(0.5f, 0.5f, 0.5f), glm::vec3(1.0f, 1.0f, 1.0f), 1.0f,
			atte.linear, atte.quadratic, glm::cos(glm::radians(12.5f)),
			glm::cos(glm::radians(17.5f))
		);

		lgl.SetShaderUniformValue("viewPos", camera.GetPositionVectorAddr());

		for (auto& cube : cubes)
		{
			glm::mat4 model = glm::mat4(1.0f);
			glm::vec3 scale = cube.GetScaleVectorAddr();
			model = glm::translate(model, cube.GetPositionVectorAddr());
			model = glm::scale(model, scale);
			//model = glm::rotate(model, (float)sin(glfwGetTime()), glm::vec3(0.0f, 1.0f, 0.0f));
			lgl.SetShaderUniformValue("model", model);
			lgl.SetShaderUniformValue("inv", glm::inverse(model), true);

			if (SolidSim::CheckForCollision(camera, cube))
			{
				camera.SetLastPosition();
			}
			if (SolidSim::CheckForCollision(lights[0], cube))
			{
				lights[0].SetLastPosition();
			}
		}
	};

	auto lampBeh = [&lgl, &camera, &lights, &lightColor]()
	{
		lgl.SetShaderUniformValue("lightColor", lightColor);

		lgl.SetShaderUniformValue("view", camera.GetViewMatrixAddr());
		lgl.SetShaderUniformValue("proj", camera.GetProjectionMatrixAddr());
		for (auto& light : lights)
		{
			glm::mat4 model = glm::mat4(1.0f);
			model = glm::translate(model, light.GetPositionVectorAddr());
			model = glm::scale(model, glm::vec3(0.2f));
			//model = glm::rotate(model, (float)sin(glfwGetTime()), glm::vec3(0.0f, 1.0f, 0.0f));
			light.SetPosition(SolidSim::Direction::Nowhere);
			lgl.SetShaderUniformValue("model", model, true);
		}
	};

	auto mugBeh = [&lgl, &camera, &mug]()
	{
		lgl.SetShaderUniformValue("view", camera.GetViewMatrixAddr());
		lgl.SetShaderUniformValue("proj", camera.GetProjectionMatrixAddr());
		
		glm::mat4 model = glm::mat4(1.0f);
		glm::vec3 scale = mug.GetScaleVectorAddr();
		model = glm::translate(model, mug.GetPositionVectorAddr());
		model = glm::scale(model, scale);
		//model = glm::rotate(model, (float)sin(glfwGetTime()), glm::vec3(0.0f, 1.0f, 0.0f));
		lgl.SetShaderUniformValue("model", model);
		//lgl.SetShaderUniformValue("inv", glm::inverse(model), true);
	};
	/*
	//glfw.CreatePolygon(vert, sizeof(vert), false);

	lgl.CreateMesh(
		{
			cube,
			sizeof(cube),
			cubeSteps,
			sizeof(cubeSteps) / sizeof(int),
			nullptr,
			0,
			false,
			"lamp",
			lampBeh
		}
	);
	*/
	std::vector<LGL::Vertex> cubeV;
	std::vector<unsigned int> cubeInd;

	std::vector<LGL::Vertex> a = ConvertAVerySpecificFloatPointerToVertexVector(vertNT, sizeof(vertNT));


	lgl.CreateMesh(
		{
			a,
			{},
			false,
			"colorsTex",
			{"box.png", "boxEdge.png"},
			lightBeh
		}
	);
	/*
	lgl.GetMeshFromFile("sphere.obj", cubeV, cubeInd);

	lgl.CreateMesh(
		{
			cubeV,
			cubeInd,
			false,
			"lamp",
		    {},
			lampBeh
		}
	);
	*/

	std::vector<LGL::Vertex> mugV;
	std::vector<unsigned int> mugInd;


	lgl.GetMeshFromFile("extraStuff/coilHead.obj", mugV, mugInd);
	lgl.CreateMesh(
		{
			mugV,
			mugInd,
			false,
			"colorsTex",
			{"extraStuff/coilTex2.png"},
			mugBeh
		}
	);

	lgl.SetStaticBackgroundColor({ 0.0f, 0.0f, 0.0f, 0.0f });

	lgl.SetCursorPositionCallback(
		[&camera](double xpos, double ypos) { camera.Rotate(static_cast<float>(xpos), static_cast<float>(ypos));}
	);
	lgl.SetScrollCallback(
		[&camera](double xpos, double ypos) { camera.Zoom(static_cast<float>(xpos), static_cast<float>(ypos)); }
	);

	lgl.SetInteractable(GLFW_KEY_W, [&camera]() { camera.SetPosition(CameraSim::Direction::Forward); });
	lgl.SetInteractable(GLFW_KEY_S, [&camera]() { camera.SetPosition(CameraSim::Direction::Backward); });
	lgl.SetInteractable(GLFW_KEY_A, [&camera]() { camera.SetPosition(CameraSim::Direction::Left); });
	lgl.SetInteractable(GLFW_KEY_D, [&camera]() { camera.SetPosition(CameraSim::Direction::Right); });

	bool controlLight = true;
	if (controlLight)
	{
		lgl.SetInteractable(GLFW_KEY_O, [&lights]() { lights[0].SetPosition(SolidSim::Direction::Up); });
		lgl.SetInteractable(GLFW_KEY_L, [&lights]() { lights[0].SetPosition(SolidSim::Direction::Down); });
		lgl.SetInteractable(GLFW_KEY_U, [&lights]() { lights[0].SetPosition(SolidSim::Direction::Forward); });
		lgl.SetInteractable(GLFW_KEY_J, [&lights]() { lights[0].SetPosition(SolidSim::Direction::Backward); });
		lgl.SetInteractable(GLFW_KEY_H, [&lights]() { lights[0].SetPosition(SolidSim::Direction::Left); });
		lgl.SetInteractable(GLFW_KEY_K, [&lights]() { lights[0].SetPosition(SolidSim::Direction::Right); });
	}

	lgl.GetMaxAmountOfVertexAttr();
	lgl.CaptureMouse();


	/*
	LGL other;
	other.CreateWindow(windowHeight, windowWidth, "Other");
	other.InitCallbacks();
	other.SetStaticBackgroundColor({ 1.0f, 0.0f, 0.0f, 0.0f });

	std::thread otherThread([&other]() { other.RunRenderingCycle(); });

	*/

	lgl.RunRenderingCycle(
		[&lgl, &camera]() 
		{ 
			camera.SetPosition(CameraSim::Direction::Nowhere);
			lgl.SetShaderUniformValue("view", camera.GetViewMatrixAddr());
			lgl.SetShaderUniformValue("proj", camera.GetProjectionMatrixAddr());
		}
	);

	LGL::TerminateOpenGL();

	//otherThread.join();
}
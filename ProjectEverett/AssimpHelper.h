#pragma once

#include <assimp/Importer.hpp>
#include "assimp/scene.h"
#include "assimp/postprocess.h"

#include "LGLStructs.h"

class AssimpHelper
{
	const aiScene* modelHandle;
	LGLStructs::ModelInfo model;

	void ProcessNode(const aiNode* nodeHandle);
	LGLStructs::Mesh ProcessMesh(const aiMesh* meshHandle);
	AssimpHelper(const std::string& file);
public:
	static void GetModel(const std::string& file, LGLStructs::ModelInfo& model);
};
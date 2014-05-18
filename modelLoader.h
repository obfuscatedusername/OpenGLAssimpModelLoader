///		***
///
///		modelLoader.h - declaration of the modelLoader class, variables and methods - Tom
///
///		***

#ifndef MODELLOADER_H
#define MODELLOADER_H

#include <Windows.h>
#include "assimp\cimport.h"
#include "assimp\scene.h"
#include "assimp\postprocess.h"
#include "GL\glew.h"
#include "GLFW\glfw3.h"
#include "SOIL\SOIL.h"
#include "matrix4x4.h"
#define GLM_FORCE_RADIANS
#include "include\glm\gtc\matrix_transform.hpp"

#include <assert.h>
#include <math.h>
#include <vector>
#include <string>
#include <map>
#include <stdio.h>
#include <iostream>

using namespace std;

#define BONES_PER_VERTEX 4

//enum to be used for setting up the pointers with the set numbers for VBO use!
enum attrib{
	vertAt, //vertex Attribute = 0
	normAt, //Normal Attribute = 1
	texCAt, //Texture Coordinate Attribute = 2
	boneAt, //bone Attribute = 3
	boneWLoc, //Bone Weight Location = 4
	indAt, //index buffer attribute = 5
	tanAt, //tangent Attribute = 6
	biTanAt //bi-Tangent Attribute = 7
};

//This struct holds all the variables pertaining to materials
struct mat{
	float diff[4];
	float amb[4];
	float spec[4];
	float emis[4];
	float shininess;
	GLuint matTex; //the diffuse texture
	GLuint matNorm; // the Normal texture (if it has any)
};

//this struct holds all the variables pertaining to bone info
struct boneInfo{
	Matrix_4f boneOffset;
	Matrix_4f finalTrans;
	boneInfo()
	{
		//iterate through and set all values to 0.0
		for(size_t i = 0; i<4;i++)
		{
			for(size_t j = 0; j<4;j++)
			{
				boneOffset.m[i][j] = 0.0;
				finalTrans.m[i][j] = 0.0;
			}
		}
	}
};

//This struct holds all of the variables pertaining to each mesh of the model
struct sMesh{
	GLuint vao, numFaces, numInd, numVert, 
			matInd, ibo, vbo, nbo, tbo, bbo, *indexes;
	GLfloat *verts, *texCoords, *normals;
	bool indexed, hasNorm, hasTexCoords, hasBones;
	size_t baseVert, baseInd;
};

//this struct holds all of the variables pertaining to the entire model, including
//instances of other structs.
struct model{
	char* cPath;
	char* cDir;
	string sName, sDir;
	GLuint numMesh,	numMat, tex;
	vector<sMesh> vMesh;
	vector<mat> vMat;
	float max_x, max_y, max_z, min_x, min_y, min_z;
	glm::mat4 MVP, ModelView;
	GLuint boneTransforms[100]; //100 is max bones, this will be the indexes of the bone transformations
};

struct vBoneData{
	size_t IDs[BONES_PER_VERTEX];
	float weights[BONES_PER_VERTEX];
	vBoneData(){reset();}
	void reset()
	{
		//iterate through and set all values to 0
		for(size_t i = 0; i< BONES_PER_VERTEX;i++)
		{
			IDs[i] = 0;
			weights[i]=0;
		}
	}
	void addBoneData(size_t bID, float w);
};

class modelLoader{
public:
	model* loadModel(char* file);
	void loadMat(model* m, const aiScene* s);
	void loadVert(model* m, const aiScene* s);
	void makeVAO(model* m);
	void freeModel(model* m);
	void renderModel(model* m);
	glm::vec3 getCentre(model* m);
	vector<glm::vec3> getMinMaxTing(model* m);
	void boneTransform(float secs, vector<Matrix_4f>& transforms, int anim, float& anTime);
	void setBoneLocations();
	void regularGrid(model* m);

private:
	
	void calcInterpScaling(aiVector3D& out, float animTime, const aiNodeAnim* pNodeAnim);
	void calcInterpRotation(aiQuaternion& out, float animTime, const aiNodeAnim* pNodeAnim);
	void calcInterpPosition(aiVector3D& out, float animTime, const aiNodeAnim* pNodeAnim);
	size_t findScaling(float animTime, const aiNodeAnim* pNodeAnim);
	size_t findRotation(float animTime, const aiNodeAnim* pNodeAnim);
	size_t findPosition(float animTime, const aiNodeAnim* pNodeAnim);
	const aiNodeAnim* findNodeAnim(const aiAnimation* pAnim, const string nodeName);
	void readNodeHierarchy(float animTime, const aiNode* pNode, const Matrix_4f& parentTrans);
	void loadBones(size_t meshInd, const aiMesh* m, vector<vBoneData>& bones, sMesh smash);
	size_t getNumBones();
	

	map<string, size_t> m_Bonemapping;
	size_t numBones;
	vector<boneInfo> m_BoneInfo;
	vector<vBoneData> theBones;
	Matrix_4f m_GlobalInverseTransform;
	const aiScene* theScene;
};
#endif
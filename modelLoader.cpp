///		***
///
///		modelLoader.cpp - modelLoader class implementation - Tom
///		This class serves as an interface between the application and the ASSIMP library.
///		3D models and any relevant animation, bone, mesh, and material data is read by this class and loaded
///		into data structures for access in the game.
///
///		***



#include "modelLoader.h"

// the current aiProcess Preset implements all of these processes as default
//
//  aiProcess_CalcTangentSpace				//Calculates the tangents and bitangents 
//  aiProcess_GenSmoothNormals				//Generates smooth normals for all vertices 
//  aiProcess_JoinIdenticalVertices			//Identifies and joins identical vertex data sets
//  aiProcess_ImproveCacheLocality			//Reorders triangles for better vertex cache locality
//  aiProcess_LimitBoneWeights				//Limits no. of bones affecting a single vertex to a max value (4)
//  aiProcess_RemoveRedundantMaterials		//Searches for unused materials and removes them
//  aiProcess_SplitLargeMeshes				//Splits large meshes into small sub-meshes
//  aiProcess_Triangulate					//Triangulates all faces of all meshes
//  aiProcess_GenUVCoords					//Converts non-UV mappings to proper texture coordinate channels
//  aiProcess_SortByPType					//Splits meshes with more than one primitive type into homogeneous sub-meshes
//  aiProcess_FindDegenerates				//Finds any degenerate primitives and converts them to proper lines or points
//  aiProcess_FindInvalidData				//removes or fixes any invalid normal vectors or UV coords

//struct method definition
void vBoneData::addBoneData(size_t bID, float w)
{
	for(size_t i = 0; i < sizeof(IDs); i++)
	{
		if (weights[i]==0.0)
		{
			IDs[i]		= bID;
			weights[i]	= w;
			return;
		}
	}
	//we should never see this place as it infers we have more bones than we allocated space for =)
	assert(0);
}

model* modelLoader::loadModel(char* file){
	theScene = aiImportFile(file, aiProcessPreset_TargetRealtime_Quality);
	if(!theScene){
		//print->error("reading mesh ", file, 4);
		printf("ERROR reading mesh - %s", file);
		exit(1);
	}
	//check that the scene has at least one mesh, though it may contain more!
	assert(theScene->mNumMeshes>0);

	model* theModel = new model;
	theModel->cPath = _strdup(file);
	theModel->sName = (string)file;
	string::size_type slashInd = theModel->sName.find_last_of("/");
	if(slashInd == string::npos){
		theModel->sDir = ".";
	} else if(slashInd == 0){
		theModel->sDir="/";
	}else{
		theModel->sDir = theModel->sName.substr(0, slashInd);
	}
	printf("Model has %i animations\n", theScene->mNumAnimations);
	printf("Model has %i cameras\n", theScene->mNumCameras);
	printf("Model has %i lights\n", theScene->mNumLights);
	printf("Model has %i materials\n", theScene->mNumMaterials);
	printf("Model has %i meshes\n", theScene->mNumMeshes);
	printf("Model has %i textures\n", theScene->mNumTextures);
	printf("Model has %i nodes below root node\n", theScene->mRootNode->mNumChildren);
	
	//assign the number of materials and meshes from the scene to the model
	theModel->numMat = theScene->mNumMaterials;
	theModel->numMesh = theScene->mNumMeshes;
	//load the vertices, normals and textures for the model
	loadVert(theModel, theScene);
	//create the VAOs and VBOs associated with the model
	makeVAO(theModel);
	//if there are materials, use SOIL to load them
	if(theScene->HasMaterials()){
		loadMat(theModel, theScene);
	}
	//if the scene has bones do these things
	m_GlobalInverseTransform = theScene->mRootNode->mTransformation;
	m_GlobalInverseTransform.Inverse();
	printf("Loaded "); printf(file); printf("\n");
	return theModel;
}

void modelLoader::loadMat(model* m, const aiScene* s){
	for(size_t i = 0; i < s->mNumMaterials; i++){
		struct aiMaterial *tm = s->mMaterials[i];
		//create a mat struct object
		mat theMat;
		//create 4 aiColor4D objects to represent(R,G,B,A) values of each component
		aiColor4D theDiff, theAmb, theSpec, theEmis;
		//Check for diffuse and assign to the mat struct, if not load default values
		if(AI_SUCCESS == aiGetMaterialColor(tm, AI_MATKEY_COLOR_DIFFUSE, &theDiff)){	
			memcpy(theMat.diff, &theDiff, sizeof(GLfloat)*4);	
		}else{theMat.diff[0]=0.5f;theMat.diff[1]=0.5f;theMat.diff[2]=0.5f;theMat.diff[3] = 1.0f;
		}

		//Check for ambient and assign to the mat struct, if not load default values
		if(AI_SUCCESS == aiGetMaterialColor(tm, AI_MATKEY_COLOR_AMBIENT, &theAmb)){
			memcpy(theMat.amb, &theAmb, sizeof(GLfloat)*4);	
		}else{
			theMat.amb[0]=0.2f;theMat.amb[1]=0.2f;theMat.amb[2]=0.2f;theMat.amb[3] = 1.0f;
		}

		//Check for specular and assign to the mat struct, if not load default values
		if(AI_SUCCESS == aiGetMaterialColor(tm, AI_MATKEY_COLOR_SPECULAR, &theSpec)){	
			memcpy(theMat.spec, &theSpec, sizeof(GLfloat)*4);	
		}else{
			theMat.spec[0]=0.0f;theMat.spec[1]=0.0f;theMat.spec[2]=0.0f;theMat.spec[3] = 1.0f;
		}

		//Check for emisive and assign to the mat struct, if not load default values
		if(AI_SUCCESS == aiGetMaterialColor(tm, AI_MATKEY_COLOR_EMISSIVE, &theEmis)){	
			memcpy(theMat.emis, &theEmis, sizeof(GLfloat)*4);	
		}else{
			theMat.emis[0]=0.0f;theMat.emis[1]=0.0f;theMat.emis[2]=0.0f;theMat.emis[3] = 1.0f;
		}
		
		float shininess = 0.0;
		unsigned int max;
		//finally get the shininess array
		aiGetMaterialFloatArray(tm, AI_MATKEY_SHININESS, &shininess, &max);
		theMat.shininess = shininess;
		//if the texture count is greater than 0, then....
		if(tm->GetTextureCount(aiTextureType_DIFFUSE) > 0){
			aiString path;
			//if we can get get a material...
			if(tm->GetTexture(aiTextureType_DIFFUSE, 0, &path, NULL, NULL, NULL, NULL, NULL)==AI_SUCCESS){
				//create a string with the correct path
				string fp = m->sDir +"/"+path.data;
				//print->loading("Texture Diffuse ", fp);
				printf("Loading Texture, diffuse - %s", fp);
				//use the SOIL library to load the texture into memory
				theMat.matTex = SOIL_load_OGL_texture(fp.c_str(), SOIL_LOAD_AUTO, 
											SOIL_CREATE_NEW_ID, SOIL_FLAG_INVERT_Y);
				//if success then continue, else print error
				if(theMat.matTex == 0){
					//print->loadingFailed();
					printf("ERROR, failed to load texture: %s", fp);
				}else 
					//print->loadingComp();
					printf("Succesfully loaded %s", fp);
			}
		}
		
		// If the model has normal maps grab em and load em yo
		if(tm->GetTextureCount(aiTextureType_HEIGHT) > 0){
			aiString path;
			if(tm->GetTexture(aiTextureType_HEIGHT, 0, &path, NULL, NULL, NULL, NULL, NULL) == AI_SUCCESS){
				string fp = m->sDir+"/"+path.data;
				//print->loading("Texture Normal ", fp);
				printf("Loading Texture, normal - %s", fp);
				theMat.matNorm = SOIL_load_OGL_texture(fp.c_str(), SOIL_LOAD_AUTO, SOIL_CREATE_NEW_ID, SOIL_FLAG_INVERT_Y);
				if(theMat.matNorm == 0){
					//print->loadingFailed();
					printf("ERROR, failed to load texture: %s", fp);
				}else 
					//print->loadingComp();
					printf("Succesfully loaded %s", fp);
			}

		}
		//push back the material vector of the model with the currently used material object.
		m->vMat.push_back(theMat);
	}
}

void modelLoader::loadVert(model* m, const aiScene*s){
	const aiMesh *mesh;
	const aiFace *face;
	aiNode* theNode;
	sMesh theMesh;
	
	size_t bv = 0;
	size_t bi = 0;

	for(size_t mCount = 0; mCount<s->mNumMeshes;mCount++){
		mesh = s->mMeshes[mCount];
		//set all of the values relating to the sMesh object
		theMesh.vao = theMesh.ibo = theMesh.nbo = theMesh.vbo = theMesh.tbo = theMesh.bbo = 0;
		theMesh.indexes=NULL;theMesh.verts=NULL;theMesh.normals=NULL;theMesh.texCoords=NULL;
		theMesh.numFaces = s->mMeshes[mCount]->mNumFaces;
		theMesh.numInd = s->mMeshes[mCount]->mNumFaces*3;
		theMesh.numVert = s->mMeshes[mCount]->mNumVertices;
		theMesh.matInd = s->mMeshes[mCount]->mMaterialIndex;
		theMesh.baseInd = bi;
		theMesh.baseVert = bv;

		theBones.resize(theBones.size() + theMesh.numVert);
		bi += theMesh.numInd;
		bv += theMesh.numVert;
		//theMesh.numFaces = mesh->mNumFaces;
		theMesh.indexes = (unsigned int*)malloc(sizeof(unsigned int) * mesh->mNumFaces * 3);
		unsigned int fIndex = 0;
		
		for(size_t i = 0; i<mesh->mNumFaces;i++){
			face=&mesh->mFaces[i];
			memcpy(&theMesh.indexes[fIndex], face->mIndices, 3 * sizeof(unsigned int));
			fIndex+=3;
		}

		//create a buffer of the correct size to hold the vertex positions
		if(mesh->HasPositions()){
			theMesh.verts = (GLfloat *) malloc(sizeof(GLfloat) * 3 * mesh->mNumVertices);
			memcpy(theMesh.verts, mesh->mVertices, sizeof(GLfloat)*3*mesh->mNumVertices);
		}
		
		//create a buffer of the correct size to hold the vertex normals
		if(mesh->HasNormals()){
			theMesh.hasNorm = true;
			theMesh.normals = (GLfloat *) malloc(sizeof(GLfloat) * 3 * mesh->mNumVertices);
			memcpy(theMesh.normals, mesh->mNormals, sizeof (GLfloat) * 3 * mesh->mNumVertices);
		} else {theMesh.hasNorm = false;}

		//create a buffer of the correct size to hold the vertex texture positions
		if(mesh->HasTextureCoords(0)){
			theMesh.hasTexCoords = true;
			theMesh.texCoords = (GLfloat *) malloc(sizeof(GLfloat) * 2 * mesh->mNumVertices);
			for (size_t j = 0;j<mesh->mNumVertices;j++)
			{
				theMesh.texCoords[j*2]		= mesh->mTextureCoords[0][j].x;
				theMesh.texCoords[j*2+1]	= mesh->mTextureCoords[0][j].y;
			}
		} else theMesh.hasTexCoords = false;

		//bone stuff go here
		if(mesh->HasBones()){
			theMesh.hasBones = true;
			printf("mesh %i has %i bones\n", mCount,  mesh->mNumBones);
			//print->mlPrint("mesh ", " has bones totalling: ", mCount, mesh->mNumBones, 7);
			loadBones(mCount, mesh, theBones, theMesh);
		}
		glBindVertexArray(0);
		glBindBuffer(GL_ARRAY_BUFFER, 0);
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
		//push_back the model vector with the current mesh
		m->vMesh.push_back(theMesh);
	}
}

void modelLoader::makeVAO(model* m)
{
	sMesh* theMesh;
	for (size_t i = 0; i < m->numMesh; i++)
	{
		theMesh = &m->vMesh[i];

		//generate vertex array object for each mesh
		glGenVertexArrays(1, &m->vMesh[i].vao);
		glBindVertexArray(m->vMesh[i].vao);

		//generate a buffer for the faces
		glGenBuffers(1, &theMesh->ibo);
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, theMesh->ibo);
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, 
			sizeof(unsigned int) * theMesh->numInd, m->vMesh[i].indexes, GL_STATIC_DRAW);

		//generate a buffer for the vertex positions
		if(theMesh->numVert > 0)
		{
			glGenBuffers(1, &theMesh->vbo);
			glBindBuffer(GL_ARRAY_BUFFER, theMesh->vbo);
			glBufferData(GL_ARRAY_BUFFER, 
				sizeof(GLfloat)*3*theMesh->numVert, theMesh->verts, GL_STATIC_DRAW);	
			glEnableVertexAttribArray(vertAt);
			glVertexAttribPointer(vertAt, 3, GL_FLOAT, 0, 0, 0);
		}

		//generate a buffer for the normals
		if(theMesh->hasNorm)
		{
			glGenBuffers(1, &theMesh->nbo);
			glBindBuffer(GL_ARRAY_BUFFER, theMesh->nbo);
			glBufferData(GL_ARRAY_BUFFER, 
				sizeof(GLfloat)*3*theMesh->numVert, theMesh->normals, GL_STATIC_DRAW);	
			glEnableVertexAttribArray(normAt);
			glVertexAttribPointer(normAt, 3, GL_FLOAT, 0, 0, 0);
		}

		//generate a buffer for the texture coords
		if(theMesh->hasTexCoords)
		{
			glGenBuffers(1, &theMesh->tbo);
			glBindBuffer(GL_ARRAY_BUFFER, theMesh->tbo);
			glBufferData(GL_ARRAY_BUFFER, 
				sizeof(float)*2*theMesh->numVert, theMesh->texCoords, GL_STATIC_DRAW);	
			glEnableVertexAttribArray(texCAt);
			glVertexAttribPointer(texCAt, 2, GL_FLOAT, 0, 0, 0);
		}

		//generate a buffer for dem bones
		if(theMesh->hasBones)
		{
			glGenBuffers(1, &theMesh->bbo);
			glBindBuffer(GL_ARRAY_BUFFER, theMesh->bbo);
			glBufferData(GL_ARRAY_BUFFER, sizeof(theBones[0]) * theBones.size(), &theBones[0], GL_STATIC_DRAW);
			glEnableVertexAttribArray(boneAt);
			glVertexAttribIPointer(boneAt, 4, GL_INT, sizeof(vBoneData), (const GLvoid*)0);
			glEnableVertexAttribArray(boneWLoc);
			glVertexAttribPointer(boneWLoc, 4, GL_FLOAT, GL_FALSE, sizeof(vBoneData), (const GLvoid*)16);
		}

		//finally unbind the buffers
		glBindVertexArray(0);
		glBindBuffer(GL_ARRAY_BUFFER, 0);
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
	}
}

void modelLoader::renderModel(model* m)
{
	//bind the VAO so the model and its associated vbos display
	for (size_t i = 0; i< m->numMesh; i++)
	{
		glBindVertexArray(m->vMesh[i].vao);

		glActiveTexture(GL_TEXTURE1);
		glBindTexture(GL_TEXTURE_2D, m->vMat[m->vMesh[i].matInd].matNorm);
		glTexParameteri(GL_TEXTURE_2D, GL_GENERATE_MIPMAP, GL_TRUE);		
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR_MIPMAP_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
		glHint(GL_PERSPECTIVE_CORRECTION_HINT, GL_NICEST);
		glGenerateMipmap(GL_TEXTURE_2D);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, m->vMat[m->vMesh[i].matInd].matTex);
		glTexParameteri(GL_TEXTURE_2D, GL_GENERATE_MIPMAP, GL_TRUE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR_MIPMAP_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
		glHint(GL_PERSPECTIVE_CORRECTION_HINT, GL_NICEST);
		glGenerateMipmap(GL_TEXTURE_2D);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

		glDrawElements(GL_TRIANGLES, m->vMesh[i].numInd, GL_UNSIGNED_INT, 0);
		glBindVertexArray(0);
	}
}

void modelLoader::freeModel(model* m)
{
	glBindVertexArray(0);
	printf("delete model and free memory\n");
	for(size_t i = 0; i < m->numMesh;i++)
	{
		if(m->vMesh[i].ibo != 0)
		{
			glDeleteVertexArrays(1, &m->vMesh[i].ibo);
			free(m->vMesh[i].indexes);
			printf("Your vao has been deleted, ");
		}
		if(m->vMesh[i].vbo != 0)
		{
			glDeleteVertexArrays(1, &m->vMesh[i].vbo);
			free(m->vMesh[i].verts);
			printf("Your vbo has been deleted, ");
		}
		if(m->vMesh[i].nbo != 0)
		{
			glDeleteVertexArrays(1, &m->vMesh[i].nbo);
			free(m->vMesh[i].normals);
			printf("Your nbo has been deleted, ");
		}
		if(m->vMesh[i].ibo != 0)
		{
			glDeleteVertexArrays(1, &m->vMesh[i].tbo);
			free(m->vMesh[i].texCoords);
			printf("Your tbo has been deleted, ");
		}
		if(m->vMesh[i].vao != 0)
		{
			glDeleteVertexArrays(1, &m->vMesh[i].vao);
			printf("Your vao has been deleted.");
		}
	}
	free(m);
}

void modelLoader::loadBones(size_t meshInd, const aiMesh* m, vector<vBoneData>& bones, sMesh smash)
{
	int bonerCount = 0;
	for(size_t i = 0; i < m->mNumBones; i++)
	{
		size_t bIndex = 0;
		string bName = m->mBones[i]->mName.data;
		if(m_Bonemapping.find(bName) == m_Bonemapping.end() )
		{
			//allocate an index for a new bone(r)
			bIndex = numBones;
			numBones++;
			boneInfo bi;
			m_BoneInfo.push_back(bi);
			m_BoneInfo[bIndex].boneOffset = m->mBones[i]->mOffsetMatrix;
			m_Bonemapping[bName] = bIndex;
		}else {bIndex = m_Bonemapping[bName];}
		
		for(size_t j = 0; j < m->mBones[i]->mNumWeights; j++)
		{
			size_t vertID = smash.baseVert + m->mBones[i]->mWeights[j].mVertexId; //possible point of contention if tings don't work
			float weight = m->mBones[i]->mWeights[j].mWeight;
			bones[vertID].addBoneData(bIndex, weight);
		}
		bonerCount++;
	}
	//print->loaded("bone(s) successfully", bonerCount, 2);
	printf("Succesfully loaded bones: %i", bonerCount);
}

size_t modelLoader::findPosition(float animTime, const aiNodeAnim* pNodeAnim)
{
	for(size_t i = 0; i < pNodeAnim->mNumPositionKeys - 1; i++)
	{
		if(animTime < (float)pNodeAnim->mPositionKeys[i + 1].mTime)
		{
			return i; 
		}
	}
	assert(0);
	return 0;
}

size_t modelLoader::findRotation(float animTime, const aiNodeAnim* pNodeAnim)
{
	assert(pNodeAnim->mNumRotationKeys > 0);
	for( size_t i = 0; i < pNodeAnim->mNumRotationKeys-1; i++)
	{
		if (animTime < (float)pNodeAnim->mRotationKeys[i + 1].mTime)
		{
			return i;
		}
	}
	assert(0);
	return 0;
}

size_t modelLoader::findScaling(float animTime, const aiNodeAnim* pNodeAnim)
{
	assert(pNodeAnim->mNumScalingKeys > 0); 
	for (size_t i = 0; i < pNodeAnim->mNumScalingKeys - 1; i++)
	{
		if(animTime < pNodeAnim->mScalingKeys[i+1].mTime)
		{
			return i; 
		}
	}
	assert(0);
	return 0;
}

void modelLoader::calcInterpPosition(aiVector3D& out, float animTime, const aiNodeAnim* pNodeAnim)
{
	if(pNodeAnim->mNumPositionKeys == 1)
	{
		out = pNodeAnim->mPositionKeys[0].mValue;
		return;
	}
	size_t posIndex = findPosition(animTime, pNodeAnim);
	size_t nextPosIndex = (posIndex + 1);
	assert(nextPosIndex < pNodeAnim->mNumPositionKeys);
	float deltaTime = (float)(pNodeAnim->mPositionKeys[nextPosIndex].mTime - pNodeAnim->mPositionKeys[posIndex].mTime);
	float factor = (animTime - (float)pNodeAnim->mPositionKeys[posIndex].mTime) / deltaTime;
	assert(factor >=0.0f && factor <= 1.0f);
	const aiVector3D& start = pNodeAnim->mPositionKeys[posIndex].mValue;
	const aiVector3D& end = pNodeAnim->mPositionKeys[nextPosIndex].mValue;
	aiVector3D delta = end - start;
	out = start + factor * delta;
}

void modelLoader::calcInterpRotation(aiQuaternion& out, float animTime, const aiNodeAnim* pNodeAnim)
{
	//we need at least 2 values to interpolate!!!!
	if(pNodeAnim->mNumRotationKeys == 1)
	{
		out = pNodeAnim->mRotationKeys[0].mValue;
		return;
	}

	size_t rotInd = findRotation(animTime, pNodeAnim);
	size_t nextRotInd = (rotInd + 1);
	assert(nextRotInd < pNodeAnim->mNumRotationKeys);
	float deltaTime = (float)(pNodeAnim->mRotationKeys[nextRotInd].mTime - pNodeAnim->mRotationKeys[rotInd].mTime);
	float factor = (animTime - (float) pNodeAnim->mRotationKeys[rotInd].mTime) / deltaTime;
	assert(factor >= 0.0f && factor <= 1.0f);
	const aiQuaternion& sRotQ = pNodeAnim->mRotationKeys[rotInd].mValue;
	const aiQuaternion& eRotQ = pNodeAnim->mRotationKeys[nextRotInd].mValue;
	aiQuaternion::Interpolate(out, sRotQ, eRotQ, factor);
	out = out.Normalize();
}

void modelLoader::calcInterpScaling(aiVector3D& out, float animTime, const aiNodeAnim* pNodeAnim)
{
	if(pNodeAnim->mNumScalingKeys == 1)
	{
		out = pNodeAnim->mScalingKeys[0].mValue;
		return;
	}
	size_t sIndex = findScaling(animTime, pNodeAnim);
	size_t nextSIndex = (sIndex+1);
	assert(nextSIndex < pNodeAnim->mNumScalingKeys);
	float deltaTime = (float)(pNodeAnim->mScalingKeys[nextSIndex].mTime - pNodeAnim->mScalingKeys[sIndex].mTime);
	float factor = (animTime - (float) pNodeAnim->mScalingKeys[sIndex].mTime) / deltaTime;
	assert(factor >= 0.0f && factor <= 1.0f); 
	const aiVector3D& start =  pNodeAnim->mScalingKeys[sIndex].mValue;
	const aiVector3D& end = pNodeAnim->mScalingKeys[nextSIndex].mValue;
	aiVector3D delta = end - start;
	out = start + factor * delta; 
}

void modelLoader::readNodeHierarchy(float animTime, const aiNode* pNode, const Matrix_4f& parentTrans)
{
	string nodeName = pNode->mName.data;
	const aiAnimation* pAnim = theScene->mAnimations[0];
	Matrix_4f nodeTransformation(pNode->mTransformation);
	const aiNodeAnim* pNodeAnim = findNodeAnim(pAnim, nodeName);
	if(pNodeAnim)
	{
		//interpolate scaling and gen the scaling transform matrix
		aiVector3D scaling;
		calcInterpScaling(scaling, animTime, pNodeAnim);
		Matrix_4f sMat; //scaling matrix
		sMat.InitScaleTransform(scaling.x, scaling.y, scaling.z);

		//interpolate the rotation and gen rotation transform matrix
		aiQuaternion rotQ;
		calcInterpRotation(rotQ, animTime, pNodeAnim);
		Matrix_4f rotM = Matrix_4f(rotQ.GetMatrix());

		//interpolate the translation and gen translation transform matrix
		aiVector3D trans;
		calcInterpPosition(trans, animTime, pNodeAnim);
		Matrix_4f transM; 
		transM.InitTranslationTransform(trans.x, trans.y, trans.z); 

		//finally, combine all of the above transformations
		nodeTransformation = transM * rotM * sMat;
	}

	Matrix_4f globalTrans = parentTrans * nodeTransformation;
	if(m_Bonemapping.find(nodeName) != m_Bonemapping.end() )
	{
		size_t boneInd = m_Bonemapping[nodeName];
		m_BoneInfo[boneInd].finalTrans = m_GlobalInverseTransform * globalTrans * m_BoneInfo[boneInd].boneOffset;
	}

	for(size_t x = 0; x < pNode->mNumChildren; x ++)
	{
		readNodeHierarchy(animTime, pNode->mChildren[x], globalTrans);
	}
}

void modelLoader::boneTransform(float secs, vector<Matrix_4f>& transforms, int anim, float& antime)
{
	Matrix_4f ident;
	ident.InitIdentity();
	float tps = (float) (theScene->mAnimations[0]->mTicksPerSecond != 0 ? theScene->mAnimations[0]->mTicksPerSecond : 25.0f);//ticks per second
	float tit = secs * tps; //time in ticks (haha, tit)

	//fmod gets the remainer from a/b e.g. fmod(5, 2.2) = 0.6
	float animTime;
	if(anim==1)
		animTime = fmod(tit, 2.66666666667); //duration of first anim	
	else if (anim==2) 
		animTime = fmod(tit, 7.03333333333) + 3.2; //duration of 2nd anim + start time of 2nd anim	
	else 
		animTime = fmod(tit, 6.0) + 10.9333333333; //duration of 3rd anim + start time of 3rd anim

	antime = animTime;
	//float test = fmod(tit, 0.23333333334) + 0.76666666666;
	//float test2 = fmod(tit, 0.23333333334) + 2.1;

	//0.76, 0.98
	//2.10, 2.32

	readNodeHierarchy(animTime, theScene->mRootNode, ident);
	transforms.resize(numBones);
	for(size_t i = 0; i< numBones; i++)
	{
		transforms[i] = m_BoneInfo[i].finalTrans;
	}
}

const aiNodeAnim* modelLoader::findNodeAnim(const aiAnimation* pAnim, const string nodeName)
{
	for(size_t i = 0; i < pAnim->mNumChannels; i++)
	{
		const aiNodeAnim* pNodeAnim = pAnim->mChannels[i];
		if(string(pNodeAnim->mNodeName.data)==nodeName)
		{
			return pNodeAnim;
		}
	}
	return NULL;
}

glm::vec3 modelLoader::getCentre(model* m){

	float l_x, l_y, l_z;
	l_x = l_y = l_z = -10000;
	float s_x, s_y, s_z;
	s_x = s_y = s_z = 10000;

	for(size_t j = 0; j < m->numMesh; j++){
		for(size_t i=0; i < m->vMesh[j].numVert; i+=3){
			float tempx = m->vMesh[j].verts[i];
			float tempy = m->vMesh[j].verts[i+1];
			float tempz = m->vMesh[j].verts[i+2];
			if(tempx > l_x){
				l_x = tempx;
			}
			if(tempy > l_y){
				l_x = tempy;
			}
			if(tempz > l_z){
				l_z = tempz;
			}
			if(tempx < s_x){
				s_x = tempx;
			}
			if(tempy < s_y){
				s_x = tempy;
			}
			if(tempz < s_z){
				s_z = tempz;
			}
		}
	}

	m->max_x = l_x; m->max_y = l_y; m->max_z = l_z; m->min_x = s_x; m->min_y = s_x; m->min_z = s_x;
	float middlex, middley, middlez;
	middlex = (s_x + l_x)/2; middley = (s_y + l_y)/2; middlez = (s_z + l_z)/2;
	return glm::vec3(middlex,middley,middlez); //this is correct for the model though for the rest z and y should be swapped
}

vector<glm::vec3> modelLoader::getMinMaxTing(model* m)
{
	float l_x, l_y, l_z;
	l_x = l_y = l_z = -10000;
	float s_x, s_y, s_z;
	s_x = s_y = s_z = 10000;

	for(size_t j = 0; j < m->numMesh; j++){
		for(size_t i=0; i < m->vMesh[j].numVert; i+=3){
			float tempx = m->vMesh[j].verts[i];
			float tempy = m->vMesh[j].verts[i+1];
			float tempz = m->vMesh[j].verts[i+2];
			if(tempx > l_x){
				l_x = tempx;
			}
			if(tempy > l_y){
				l_y = tempy;
			}
			if(tempz > l_z){
				l_z = tempz;
			}
			if(tempx < s_x){
				s_x = tempx;
			}
			if(tempy < s_y){
				s_y = tempy;
			}
			if(tempz < s_z){
				s_z = tempz;
			}
		}
	}
	vector<glm::vec3> temp; 
	glm::vec3 min, max;
	min = glm::vec3(s_x, s_y, s_z); 
	temp.push_back(min); 
	max = glm::vec3(l_x, l_y, l_z);
	temp.push_back(max);
	return temp;

}

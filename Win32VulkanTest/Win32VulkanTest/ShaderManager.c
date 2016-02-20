#include "stdafx.h"
#include "ShaderManager.h"

#include "MemoryUtils.h"
#include "VulkanRenderer.h"

struct shader_manager_t {
	VulkanRenderer* pVulkanRenderer;

	char* szShaderDirectory;
	char* szVertexExtension;
	char* szFragmentExtension;
};

FILE* ShaderManager_OpenShader(
	ShaderManager* pThis,
	const char* szFileName,
	const char* szExtension);
void ShaderManager_CloseShader(ShaderManager* pThis, FILE* pShaderFile);

/*!
 * \brief	creates a manager to load and save shaders
 *
 * A simple manager to create track shader objects
 *
 * \param	pVulkanRenderer the vulkan renderer to use
 * \param	szShaderDirectory where to look for shaders (copied)
 * \param	szVertexExtension the extension on vertex shaders (copied)
 * \param	szFragmentExtension the extension on fragment shaders (copied)
 * \param	vertexShaderCount the number of vertex shaders we will support
 * \param	fragmentShaderCount the number of fragment shaders we will support
 */
ShaderManager* ShaderManager_Create(
	const char* szShaderDirectory,
	const char* szVertexExtension,
	const char* szFragmentExtension) {

	ShaderManager* pShaderManager = (ShaderManager*)malloc(sizeof(ShaderManager));
	memset(pShaderManager, 0, sizeof(ShaderManager));

	pShaderManager->szShaderDirectory = strdup(szShaderDirectory);
	pShaderManager->szVertexExtension = strdup(szVertexExtension);
	pShaderManager->szFragmentExtension = strdup(szFragmentExtension);

	return pShaderManager;
}

void ShaderManager_Destroy(ShaderManager* pThis) {
	assert(pThis);

	SAFE_FREE(pThis->szShaderDirectory);
	SAFE_FREE(pThis->szVertexExtension);
	SAFE_FREE(pThis->szFragmentExtension);

	free(pThis);
}

ShaderCode ShaderManager_GetVertexShader(
	ShaderManager* pThis,
	const char* szShaderName) {

	FILE* pShaderFile = ShaderManager_OpenShader(
		pThis,
		szShaderName,
		pThis->szVertexExtension);

	fseek(pShaderFile, 0, SEEK_END);
	fpos_t fileLength;
	fgetpos(pShaderFile, &fileLength);
	fseek(pShaderFile, 0, SEEK_SET);
	char* szFileContents = SAFE_ALLOCATE_ARRAY(char, fileLength);

	fgets(szFileContents, fileLength, pShaderFile);

	ShaderManager_CloseShader(pThis, pShaderFile);

	return (ShaderCode) {
		szFileContents,
		fileLength
	};
}

ShaderCode ShaderManager_GetFragmentShader(
	ShaderManager* pThis,
	const char* szShaderName) {
	assert(FALSE);
}

void ShaderManager_CleanupShaderCode(ShaderCode shaderCode) {
	assert(shaderCode.pCode);
	assert(shaderCode.codeSize > 0);

	SAFE_FREE(shaderCode.pCode);
	shaderCode.codeSize = 0;
}

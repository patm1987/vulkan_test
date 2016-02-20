
#ifndef __SHADER_MANAGER_H
#define __SHADER_MANAGER_H

#ifdef __cplusplus
extern "C" {
#endif//__cplusplus

typedef struct shader_manager_t ShaderManager;

typedef struct shader_code_t {
	uint32_t* pCode;
	size_t codeSize;
} ShaderCode;

ShaderManager* ShaderManager_Create(
	const char* szShaderDirectory,
	const char* szVertexExtension,
	const char* szFragmentExtension);
void ShaderManager_Destroy(ShaderManager* pThis);

ShaderCode ShaderManager_GetVertexShader(
	ShaderManager* pThis,
	const char* szShaderName);
ShaderCode ShaderManager_GetFragmentShader(
	ShaderManager* pThis,
	const char* szShaderName);

void ShaderManager_CleanupShaderCode(ShaderCode shaderCode);

#ifdef __cplusplus
}
#endif//__cplusplus

#endif//__SHADER_MANAGER_H

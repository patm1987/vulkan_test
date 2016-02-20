
#ifndef __VULKAN_RENDERER_H
#define __VULKAN_RENDERER_H

#ifdef __cplusplus
extern "C" {
#endif//__cplusplus

typedef struct vulkan_renderer_t VulkanRenderer;

VulkanRenderer* VulkanRenderer_Create(float width, float height);
void VulkanRenderer_Render(VulkanRenderer* pThis);
void VulkanRenderer_Destroy(VulkanRenderer* pThis);

#ifdef __cplusplus
}
#endif//__cplusplus

#endif//__VULKAN_RENDERER_H

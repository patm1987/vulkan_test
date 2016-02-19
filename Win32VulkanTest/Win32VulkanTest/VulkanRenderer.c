#include "stdafx.h"

#include "VulkanRenderer.h"

struct vulkan_renderer_t {
	int placeholder;
};

VulkanRenderer* VulkanRenderer_Create() {
	VulkanRenderer* pVulkanRenderer = (VulkanRenderer*)malloc(sizeof(VulkanRenderer*));
}

void VulkanRenderer_Render(VulkanRenderer* pThis) {

}

void VulkanRenderer_Destroy(VulkanRenderer* pThis) {
	assert(pThis);
	free(pThis);
}

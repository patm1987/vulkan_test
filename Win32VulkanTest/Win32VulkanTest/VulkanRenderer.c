#include "stdafx.h"

#include "VulkanRenderer.h"

#include "ShaderManager.h"
#include "Utils.h"

typedef struct vertex_t {
	float position [3];
	float color [3];
} Vertex;

struct vulkan_renderer_t {
	uint32_t width;
	uint32_t height;

	VkInstance instance;
	VkDevice device;

	uint32_t queueFamilyIndex;

	// TODO: do I need this?
	VkQueue mainQueue;
	VkCommandPool commandPool;

	VkCommandBuffer setupCommandBuffer;

	ShaderManager* pShaderManager;

	VkFormat depthBufferFormat;
};

void VulkanRenderer_CreateCommandPool(VulkanRenderer* pThis);
void VulkanRenderer_CreateSetupCommandBuffer(VulkanRenderer* pThis);
void VulkanRenderer_CreatePipelines(VulkanRenderer* pThis);
void VulkanRenderer_FreeSetupCommandBuffer(VulkanRenderer* pThis);

VkFormat SelectDepthFormat(VkPhysicalDevice physicalDevice);

BOOL DeviceTypeIsSuperior(VkPhysicalDeviceType newType, VkPhysicalDeviceType oldType);

// order the devices by the kind we'd prefer to run on
VkPhysicalDeviceType aDevicePrecidents[] = {
	VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU,
	VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU,
	VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU,
	VK_PHYSICAL_DEVICE_TYPE_CPU,
	VK_PHYSICAL_DEVICE_TYPE_OTHER,
};

VulkanRenderer* VulkanRenderer_Create(uint32_t width, uint32_t height) {
	VulkanRenderer* pVulkanRenderer = (VulkanRenderer*)malloc(sizeof(VulkanRenderer));
	memset(pVulkanRenderer, 0, sizeof(VulkanRenderer));

	pVulkanRenderer->width = width;
	pVulkanRenderer->height = height;

	pVulkanRenderer->pShaderManager = ShaderManager_Create(
		"Resources/Shaders",
		".vert.spv",
		".frag.spv");

	// initialize vulkan
	VkInstanceCreateInfo createInfo = { 0 };
	createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
	createInfo.pNext = NULL;
	createInfo.flags = 0;
	createInfo.pApplicationInfo = NULL;
	createInfo.enabledLayerCount = 0;
	createInfo.ppEnabledLayerNames = NULL;
	createInfo.enabledExtensionCount = 0;
	createInfo.ppEnabledExtensionNames = NULL;
	VK_EXECUTE_REQUIRE_SUCCESS(
		vkCreateInstance(&createInfo, NULL, &pVulkanRenderer->instance)
	);

	// get all the physical devices
	// first find the numer of devices
	uint32_t physicalDeviceCount;
	VK_EXECUTE_REQUIRE_SUCCESS(
		vkEnumeratePhysicalDevices(
			pVulkanRenderer->instance,
			&physicalDeviceCount,
			NULL)
	);

	// then get the devices
	VkPhysicalDevice* paPhysicalDevices = (VkPhysicalDevice*)malloc(
		sizeof(VkPhysicalDevice) * physicalDeviceCount);
	VK_EXECUTE_REQUIRE_SUCCESS(
		vkEnumeratePhysicalDevices(
			pVulkanRenderer->instance,
			&physicalDeviceCount,
			paPhysicalDevices)
	);

	// find one we like
	VkPhysicalDevice chosenDevice = NULL;
	VkPhysicalDeviceProperties chosenDeviceProperties;
	uint32_t chosenQueueIndex = 0;
	for (uint32_t deviceIndex = 0; deviceIndex < physicalDeviceCount; deviceIndex++) {
		VkPhysicalDeviceProperties deviceProperties;
		vkGetPhysicalDeviceProperties(paPhysicalDevices[deviceIndex], &deviceProperties);

		uint32_t queueFamilyPropertyCount;
		vkGetPhysicalDeviceQueueFamilyProperties(
			paPhysicalDevices[deviceIndex],
			&queueFamilyPropertyCount,
			NULL);
		VkQueueFamilyProperties* paQueueFamilyProperties
			= (VkQueueFamilyProperties*)malloc(
				sizeof(VkQueueFamilyProperties) * queueFamilyPropertyCount);

		BOOL queueValid = FALSE;
		uint32_t queueIndex = 0;
		for (; queueIndex < queueFamilyPropertyCount; queueIndex++) {
			if (paQueueFamilyProperties[queueIndex].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
				queueValid = TRUE;
				break;
			}
		}

		if (!chosenDevice) {
			chosenDevice = paPhysicalDevices[deviceIndex];
			chosenDeviceProperties = deviceProperties;
			chosenQueueIndex = queueIndex;
		}
		else {
			if (DeviceTypeIsSuperior(
				deviceProperties.deviceType,
				chosenDeviceProperties.deviceType)) {

				chosenDevice = paPhysicalDevices[deviceIndex];
				chosenDeviceProperties = deviceProperties;
				chosenQueueIndex = queueIndex;
			}
		}

		// if we found a discreet GPU - that's probably ideal
		if (chosenDeviceProperties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
			// we've struck gold!
			break;
		}

		free(paQueueFamilyProperties);
	}

	pVulkanRenderer->depthBufferFormat = SelectDepthFormat(chosenDevice);

	// make a logical device
	float queuePriorities[1] = { 0.5f };
	VkDeviceQueueCreateInfo deviceQueues[1] = {0};
	deviceQueues[0].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
	deviceQueues[0].pNext = NULL;
	deviceQueues[0].flags = 0;
	deviceQueues[0].queueFamilyIndex = chosenQueueIndex;
	deviceQueues[0].queueCount = 1;
	deviceQueues[0].pQueuePriorities = queuePriorities;

	VkDeviceCreateInfo deviceCreateInfo = { 0 };
	deviceCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
	deviceCreateInfo.pNext = NULL;
	deviceCreateInfo.flags = 0;
	deviceCreateInfo.queueCreateInfoCount = 1;
	deviceCreateInfo.pQueueCreateInfos = deviceQueues;
	deviceCreateInfo.enabledLayerCount = 0; // don't turn any layers on for now
	deviceCreateInfo.ppEnabledLayerNames = NULL;
	deviceCreateInfo.enabledExtensionCount = 0; // no extensions
	deviceCreateInfo.ppEnabledExtensionNames = NULL;
	deviceCreateInfo.pEnabledFeatures = NULL; // no special features for now
	VK_EXECUTE_REQUIRE_SUCCESS(
		vkCreateDevice(
			chosenDevice,
			&deviceCreateInfo,
			NULL,
			&pVulkanRenderer->device)
	);

	// grab the queue!
	vkGetDeviceQueue(
		pVulkanRenderer->device,
		chosenQueueIndex,
		0,
		&pVulkanRenderer->mainQueue);
	pVulkanRenderer->queueFamilyIndex = chosenQueueIndex;

	free(paPhysicalDevices);

	VulkanRenderer_CreateCommandPool(pVulkanRenderer);
	VulkanRenderer_CreateSetupCommandBuffer(pVulkanRenderer);
	//VulkanRenderer_CreatePipelines(pVulkanRenderer);

	return pVulkanRenderer;
}

void VulkanRenderer_Render(VulkanRenderer* pThis) {
	assert(pThis);
}

void VulkanRenderer_Destroy(VulkanRenderer* pThis) {
	assert(pThis);

	ShaderManager_Destroy(pThis->pShaderManager);
	pThis->pShaderManager = NULL;

	VulkanRenderer_FreeSetupCommandBuffer(pThis);
	vkDestroyCommandPool(pThis->device, pThis->commandPool, NULL);
	vkDeviceWaitIdle(pThis->device);
	vkDestroyDevice(pThis->device, NULL);
	vkDestroyInstance(pThis->instance, NULL);
	free(pThis);
}

// Private Interface!

void VulkanRenderer_CreateCommandPool(VulkanRenderer* pThis) {
	VkCommandPoolCreateInfo createInfo = { 0 };
	createInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	createInfo.pNext = NULL;
	createInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
	createInfo.queueFamilyIndex = pThis->queueFamilyIndex;

	VK_EXECUTE_REQUIRE_SUCCESS(
		vkCreateCommandPool(
			pThis->device,
			&createInfo,
			NULL,
			&pThis->commandPool)
	);
}

void VulkanRenderer_CreateSetupCommandBuffer(VulkanRenderer* pThis) {
	VkCommandBufferAllocateInfo commandBufferAllocateInfo = { 0 };
	commandBufferAllocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	commandBufferAllocateInfo.pNext = NULL;
	commandBufferAllocateInfo.commandPool = pThis->commandPool;
	commandBufferAllocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	commandBufferAllocateInfo.commandBufferCount = 1;

	VK_EXECUTE_REQUIRE_SUCCESS(
		vkAllocateCommandBuffers(
			pThis->device,
			&commandBufferAllocateInfo,
			&pThis->setupCommandBuffer)
		);

	// put in setup commands
	VkCommandBufferBeginInfo beginInfo = { 0 };
	beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	beginInfo.pNext = NULL;
	beginInfo.flags = 0;
	VK_EXECUTE_REQUIRE_SUCCESS(
		vkBeginCommandBuffer(pThis->setupCommandBuffer, &beginInfo));
	VK_EXECUTE_REQUIRE_SUCCESS(vkEndCommandBuffer(pThis->setupCommandBuffer));
}

void VulkanRenderer_CreatePipelines(VulkanRenderer* pThis) {
	// shader stuff
	ShaderCode vertexShaderCode = ShaderManager_GetVertexShader(
		pThis->pShaderManager,
		"main");
	VkShaderModuleCreateInfo vertexShaderCreateInfo = { 0 };
	vertexShaderCreateInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	vertexShaderCreateInfo.pNext = NULL;
	vertexShaderCreateInfo.flags = 0;
	vertexShaderCreateInfo.codeSize = vertexShaderCode.codeSize;
	vertexShaderCreateInfo.pCode = vertexShaderCode.pCode;
	VkShaderModule vertexShaderModule;
	VK_EXECUTE_REQUIRE_SUCCESS(
		vkCreateShaderModule(
			pThis->device,
			&vertexShaderCreateInfo,
			NULL,
			&vertexShaderModule)
		);

	ShaderCode fragmentShaderCode = ShaderManager_GetFragmentShader(
		pThis->pShaderManager,
		"main");
	VkShaderModuleCreateInfo fragmentShaderCreateInfo = { 0 };
	fragmentShaderCreateInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	fragmentShaderCreateInfo.pNext = NULL;
	fragmentShaderCreateInfo.flags = 0;
	fragmentShaderCreateInfo.codeSize = fragmentShaderCode.codeSize;
	fragmentShaderCreateInfo.pCode = fragmentShaderCode.pCode;
	VkShaderModule fragmentShaderModule;
	VK_EXECUTE_REQUIRE_SUCCESS(
		vkCreateShaderModule(
			pThis->device,
			&fragmentShaderCreateInfo,
			NULL,
			&fragmentShaderModule)
		);

	VkPipelineShaderStageCreateInfo aShaderStageCreateInfo[2] = { 0 };
	aShaderStageCreateInfo[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	aShaderStageCreateInfo[0].pNext = NULL;
	aShaderStageCreateInfo[0].flags = 0;
	aShaderStageCreateInfo[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
	aShaderStageCreateInfo[0].module = vertexShaderModule;
	aShaderStageCreateInfo[0].pName = "main";
	aShaderStageCreateInfo[0].pSpecializationInfo = NULL;
	aShaderStageCreateInfo[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	aShaderStageCreateInfo[1].pNext = NULL;
	aShaderStageCreateInfo[1].flags = 0;
	aShaderStageCreateInfo[1].stage = VK_SHADER_STAGE_VERTEX_BIT;
	aShaderStageCreateInfo[1].module = fragmentShaderModule;
	aShaderStageCreateInfo[1].pName = "main";
	aShaderStageCreateInfo[1].pSpecializationInfo = NULL;

	// create the input binding
	VkVertexInputBindingDescription inputBindingDescriptions[1] = { 0 };
	inputBindingDescriptions[0].binding = 0;
	inputBindingDescriptions[0].stride = sizeof(Vertex);
	inputBindingDescriptions[0].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

	// create vertex attribute descriptions
	VkVertexInputAttributeDescription inputAttributeDescriptions[2] = { 0 };

	// position attribute
	inputAttributeDescriptions[0].location = 0;
	inputAttributeDescriptions[0].binding = 0;
	inputAttributeDescriptions[0].format = VK_FORMAT_R32G32B32A32_SFLOAT;
	inputAttributeDescriptions[0].offset = offsetof(Vertex, position);

	// color attribute
	inputAttributeDescriptions[1].location = 1;
	inputAttributeDescriptions[1].binding = 0;
	inputAttributeDescriptions[1].format = VK_FORMAT_R32G32B32A32_SFLOAT;
	inputAttributeDescriptions[1].offset = offsetof(Vertex, color);

	VkPipelineVertexInputStateCreateInfo vertexInputStateInfo = { 0 };
	vertexInputStateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	vertexInputStateInfo.pNext = NULL;
	vertexInputStateInfo.flags = 0;
	vertexInputStateInfo.vertexBindingDescriptionCount = 1;
	vertexInputStateInfo.pVertexBindingDescriptions = inputBindingDescriptions;
	vertexInputStateInfo.vertexAttributeDescriptionCount = 2;
	vertexInputStateInfo.pVertexAttributeDescriptions = inputAttributeDescriptions;

	VkPipelineInputAssemblyStateCreateInfo inputAssemblyState = { 0 };
	inputAssemblyState.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	inputAssemblyState.pNext = NULL;
	inputAssemblyState.flags = 0;
	inputAssemblyState.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
	inputAssemblyState.primitiveRestartEnable = VK_FALSE;

	VkViewport aViewports[1] = { 0 };
	aViewports[0].x = 0;
	aViewports[0].y = 0;
	aViewports[0].width = (float)pThis->width;
	aViewports[0].height = (float)pThis->height;
	aViewports[0].minDepth = 0.f;
	aViewports[0].maxDepth = 1.f;

	VkRect2D aScissors[1] = { 0 };
	aScissors[0].extent.width = pThis->width;
	aScissors[0].extent.height = pThis->height;
	aScissors[0].offset.x = 0;
	aScissors[0].offset.y = 0;

	VkPipelineViewportStateCreateInfo viewportCreateInfo = { 0 };
	viewportCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
	viewportCreateInfo.pNext = NULL;
	viewportCreateInfo.flags = 0;
	viewportCreateInfo.viewportCount = 1;
	viewportCreateInfo.pViewports = aViewports;
	viewportCreateInfo.scissorCount = 1;
	viewportCreateInfo.pScissors = aScissors;

	VkPipelineRasterizationStateCreateInfo rasterizationState = { 0 };
	rasterizationState.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
	rasterizationState.pNext = NULL;
	rasterizationState.flags = 0;
	rasterizationState.depthClampEnable = VK_TRUE;
	rasterizationState.rasterizerDiscardEnable = VK_TRUE;
	rasterizationState.polygonMode = VK_POLYGON_MODE_POINT;
	rasterizationState.cullMode = VK_CULL_MODE_NONE; // TODO: actually do front-facing polys
	rasterizationState.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
	rasterizationState.depthBiasEnable = VK_FALSE;
	rasterizationState.depthBiasConstantFactor = 0;
	rasterizationState.depthBiasClamp = 0;
	rasterizationState.depthBiasSlopeFactor = 0;
	rasterizationState.lineWidth = 1;

	VkPipelineMultisampleStateCreateInfo multisampleState;
	multisampleState.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	multisampleState.pNext = NULL;
	multisampleState.flags = 0;
	multisampleState.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
	multisampleState.sampleShadingEnable = VK_FALSE;
	multisampleState.minSampleShading = 0;
	multisampleState.pSampleMask = NULL;
	multisampleState.alphaToCoverageEnable = VK_FALSE;
	multisampleState.alphaToOneEnable = VK_FALSE;

	VkPipelineDepthStencilStateCreateInfo depthStencilState = { 0 };
	depthStencilState.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
	depthStencilState.pNext = NULL;
	depthStencilState.flags = 0;
	depthStencilState.depthTestEnable = VK_TRUE;
	depthStencilState.depthWriteEnable = VK_TRUE;
	depthStencilState.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
	depthStencilState.depthBoundsTestEnable = VK_FALSE;
	depthStencilState.stencilTestEnable = VK_FALSE;
	depthStencilState.front.failOp = VK_STENCIL_OP_KEEP;
	depthStencilState.front.passOp = VK_STENCIL_OP_KEEP;
	depthStencilState.front.depthFailOp = VK_STENCIL_OP_KEEP;
	depthStencilState.front.compareOp = VK_COMPARE_OP_ALWAYS;
	depthStencilState.back.failOp = VK_STENCIL_OP_KEEP;
	depthStencilState.back.passOp = VK_STENCIL_OP_KEEP;
	depthStencilState.back.depthFailOp = VK_STENCIL_OP_KEEP;
	depthStencilState.back.compareOp = VK_COMPARE_OP_ALWAYS;
	depthStencilState.minDepthBounds = 0;
	depthStencilState.maxDepthBounds = 0;

	VkPipelineColorBlendAttachmentState aColorBlendAttachments[1] = { 0 };
	aColorBlendAttachments[0].blendEnable = VK_FALSE;

	VkPipelineColorBlendStateCreateInfo colorBlendState = { 0 };
	colorBlendState.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	colorBlendState.pNext = NULL;
	colorBlendState.flags = 0;
	colorBlendState.logicOpEnable = VK_FALSE;
	colorBlendState.attachmentCount = 1;
	colorBlendState.pAttachments = aColorBlendAttachments;

	// Descriptor Sets/Bindings: uniforms can be in sets and bindings:
	// layout(set=0, binding=0) uniform blah{};
	VkDescriptorSetLayoutBinding aLayoutBindings0[1] = { 0 };
	aLayoutBindings0[0].binding = 0;
	aLayoutBindings0[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	aLayoutBindings0[0].descriptorCount = 1;
	aLayoutBindings0[0].stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
	aLayoutBindings0[0].pImmutableSamplers = NULL;

	VkDescriptorSetLayoutCreateInfo aDescriptorSetCreateInfos[1] = { 0 };
	aDescriptorSetCreateInfos[0].sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	aDescriptorSetCreateInfos[0].pNext = NULL;
	aDescriptorSetCreateInfos[0].flags = 0;
	aDescriptorSetCreateInfos[0].bindingCount = 1;
	aDescriptorSetCreateInfos[0].pBindings = aLayoutBindings0;

	VkDescriptorSetLayout descriptorSetLayout;
	VkDescriptorSetLayout aSetLayouts[1] = { 0 };
	VK_EXECUTE_REQUIRE_SUCCESS(
		vkCreateDescriptorSetLayout(
			pThis->device,
			aDescriptorSetCreateInfos,
			NULL,
			&descriptorSetLayout)
		);

	VkPipelineLayoutCreateInfo pipelineLayoutCreate = { 0 };
	pipelineLayoutCreate.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	pipelineLayoutCreate.pNext = NULL;
	pipelineLayoutCreate.flags = 0;
	pipelineLayoutCreate.setLayoutCount = 1;
	pipelineLayoutCreate.pSetLayouts = &descriptorSetLayout;
	pipelineLayoutCreate.pushConstantRangeCount = 0;
	pipelineLayoutCreate.pPushConstantRanges = NULL;

	VkPipelineLayout pipelineLayout;
	VK_EXECUTE_REQUIRE_SUCCESS(
		vkCreatePipelineLayout(
			pThis->device,
			&pipelineLayoutCreate,
			NULL,
			&pipelineLayout)
		);

	VkAttachmentDescription aRenderPassAttachments[2] = { 0 };

	// cbuffer
	aRenderPassAttachments[0].flags = 0;
	aRenderPassAttachments[0].format = VK_FORMAT_B8G8R8A8_UNORM;
	aRenderPassAttachments[0].samples = VK_SAMPLE_COUNT_1_BIT;
	aRenderPassAttachments[0].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	aRenderPassAttachments[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	aRenderPassAttachments[0].stencilLoadOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	aRenderPassAttachments[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	aRenderPassAttachments[0].initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
	aRenderPassAttachments[0].finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	// zbuffer
	aRenderPassAttachments[1].flags = 0;
	aRenderPassAttachments[1].format = pThis->depthBufferFormat;
	aRenderPassAttachments[1].samples = VK_SAMPLE_COUNT_1_BIT;
	aRenderPassAttachments[1].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	aRenderPassAttachments[1].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	aRenderPassAttachments[1].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	aRenderPassAttachments[1].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	aRenderPassAttachments[1].initialLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
	aRenderPassAttachments[1].finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

	VkAttachmentReference aColorAttachmentReferences[1] = { 0 };
	aColorAttachmentReferences[0].attachment = 0;
	aColorAttachmentReferences[0].layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	VkAttachmentReference aDepthAttachmentReferences[1] = { 0 };
	aDepthAttachmentReferences[0].attachment = 1;
	aDepthAttachmentReferences[0].layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

	VkSubpassDescription aSubpasses[1] = { 0 };
	aSubpasses[0].flags = 0;
	aSubpasses[0].pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	aSubpasses[0].inputAttachmentCount = 0;
	aSubpasses[0].pInputAttachments = NULL;
	aSubpasses[0].colorAttachmentCount = 1;
	aSubpasses[0].pColorAttachments = aColorAttachmentReferences;
	aSubpasses[0].pResolveAttachments = NULL;
	aSubpasses[0].pDepthStencilAttachment = aDepthAttachmentReferences;
	aSubpasses[0].preserveAttachmentCount = 0;
	aSubpasses[0].pPreserveAttachments = NULL;

	VkRenderPassCreateInfo renderPassCreate = { 0 };
	renderPassCreate.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	renderPassCreate.pNext = NULL;
	renderPassCreate.flags = 0;
	renderPassCreate.attachmentCount = 2; // cbuffer and zbuffer
	renderPassCreate.pAttachments = aRenderPassAttachments;
	renderPassCreate.subpassCount = 1;
	renderPassCreate.pSubpasses = aSubpasses;
	renderPassCreate.dependencyCount = 0;
	renderPassCreate.pDependencies = NULL;

	// TODO: this can be entirely seperate from the pipeline create!
	VkRenderPass renderPass;
	VK_EXECUTE_REQUIRE_SUCCESS(
		vkCreateRenderPass(
			pThis->device,
			&renderPassCreate,
			NULL,
			&renderPass)
		);

	VkGraphicsPipelineCreateInfo pipelineCreateInfo = { 0 };
	pipelineCreateInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	pipelineCreateInfo.pNext = NULL;
	pipelineCreateInfo.flags = 0;
	pipelineCreateInfo.stageCount = 2;
	pipelineCreateInfo.pStages = aShaderStageCreateInfo;
	pipelineCreateInfo.pVertexInputState = &vertexInputStateInfo;
	pipelineCreateInfo.pInputAssemblyState = &inputAssemblyState;
	pipelineCreateInfo.pTessellationState = NULL;
	pipelineCreateInfo.pViewportState = &viewportCreateInfo;
	pipelineCreateInfo.pRasterizationState = &rasterizationState;
	pipelineCreateInfo.pMultisampleState = &multisampleState;
	pipelineCreateInfo.pDepthStencilState = &depthStencilState;
	pipelineCreateInfo.pColorBlendState = &colorBlendState;
	pipelineCreateInfo.pDynamicState = NULL;
	pipelineCreateInfo.layout = pipelineLayout;
	pipelineCreateInfo.renderPass = renderPass;
	pipelineCreateInfo.subpass = 0;
	pipelineCreateInfo.basePipelineHandle = 0;
	pipelineCreateInfo.basePipelineIndex = 0;

	VkPipeline graphicsPipeline;
	VK_EXECUTE_REQUIRE_SUCCESS(
		vkCreateGraphicsPipelines(
			pThis->device,
			VK_NULL_HANDLE,
			1,
			&pipelineCreateInfo,
			VK_NULL_HANDLE,
			&graphicsPipeline)
		);

	ShaderManager_CleanupShaderCode(vertexShaderCode);
	ShaderManager_CleanupShaderCode(fragmentShaderCode);

	// TODO: destroy graphics pipeline
	// TODO: destroy vertex shader
	// TODO: destroy fragment shader
	// TODO: destroy pipeline layout
	// TODO: destroy render pass
}

void VulkanRenderer_FreeSetupCommandBuffer(VulkanRenderer* pThis) {
	if (pThis->setupCommandBuffer) {
		vkFreeCommandBuffers(
			pThis->device,
			pThis->commandPool,
			1,
			&pThis->setupCommandBuffer);
		pThis->setupCommandBuffer = NULL;
	}
}

const VkFormat kaDepthFormats[] = {
	VK_FORMAT_D32_SFLOAT_S8_UINT,
	VK_FORMAT_D32_SFLOAT,
	VK_FORMAT_D24_UNORM_S8_UINT,
	VK_FORMAT_D16_UNORM_S8_UINT,
	VK_FORMAT_D16_UNORM
};

VkFormat SelectDepthFormat(VkPhysicalDevice physicalDevice) {
	for (int i = 0; i < sizeof(kaDepthFormats) / sizeof(VkFormat); i++) {
		VkFormatProperties formatProperties;
		vkGetPhysicalDeviceFormatProperties(
			physicalDevice,
			kaDepthFormats[i],
			&formatProperties);
		if (formatProperties.optimalTilingFeatures
			& VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT) {

			return kaDepthFormats[i];
		}
	}

	assert(FALSE);
	return VK_FORMAT_D16_UNORM;
}

/*!
* \brief	Determine if the \a newType of device is better than the \a oldType
* \param	newType the new type of device
* \param	oldType the old type of device
* \return	TRUE if we think that \a newType is better than or equal to \a oldType
*/
BOOL DeviceTypeIsSuperior(VkPhysicalDeviceType newType, VkPhysicalDeviceType oldType) {
	for (int i = 0; i < sizeof(aDevicePrecidents) / sizeof(VkPhysicalDevice); i++) {
		VkPhysicalDeviceType currentPrecident = aDevicePrecidents[i];
		if (currentPrecident == newType) {
			return TRUE;
		}
		if (currentPrecident == oldType) {
			return FALSE;
		}
	}
	return TRUE;
}

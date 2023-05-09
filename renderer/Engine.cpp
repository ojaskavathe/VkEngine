#include <Engine.hpp>

#include <iostream>
#include <vector>
#include <stdexcept>
#include <cstring>		//strcmp
#include <map>			//std::multimap
#include <set>			//std::set doesn't allow duplicates

#include <limits>		//std::numeric_limits
#include <algorithm>	//std::clamp

#include <fstream>		//for reading the shaders

#include <utils.hpp>
#include <debugUtils.hpp>

void Engine::run()
{
	initWindow();
	initVulkan();
	mainLoop();
	cleanUp();
}

void Engine::initWindow()
{
	glfwInit();
	glfwSetErrorCallback(glfwErrorCallback);

	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
	glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

	window = glfwCreateWindow(WIDTH, HEIGHT, "HAHAHA", nullptr, nullptr);
}

void Engine::initVulkan()
{
	createInstance();
	setupDebugMessenger();
	createSurface();
	pickPhysicalDevice();
	createLogicalDevice();
	createSwapchain();
	createRenderPass();
	createGraphicsPipeline();
	createFramebuffers();
	createCommandPool();
	createCommandBuffer();
	createSyncObjects();
}

void Engine::mainLoop()
{
	while (!glfwWindowShouldClose(window))
	{
		glfwPollEvents();
		drawFrame();
	}

	vkDeviceWaitIdle(device);
}

void Engine::drawFrame()
{
	vkWaitForFences(device, 1, &inFlightFence, VK_TRUE, UINT32_MAX);
	vkResetFences(device, 1, &inFlightFence);

	uint32_t imageIndex = 0;
	vkAcquireNextImageKHR(device, swapchain, UINT32_MAX, imageAvailableSemaphore, VK_NULL_HANDLE, &imageIndex);

	vkResetCommandBuffer(commandBuffer, 0);
	recordCommandBuffer(commandBuffer, imageIndex);

	VkSemaphore waitSemaphores[] = { imageAvailableSemaphore };
	VkSemaphore signalSemaphores[] = { renderFinishedSemaphore };
	VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
	VkSubmitInfo submitInfo{
		VK_STRUCTURE_TYPE_SUBMIT_INFO,	//sType
		nullptr,						//pNext
		1,								//waitSemaphoreCount
		waitSemaphores,					//pWaitSemaphores
		waitStages,						//pWaitDstStageMask
		1,								//commandBufferCount
		&commandBuffer,					//pCommandBuffers
		1,								//signalSemaphoreCount
		signalSemaphores 				//pSignalSemaphores
	};
	if (vkQueueSubmit(graphicsQueue, 1, &submitInfo, inFlightFence) != VK_SUCCESS) {
		throw std::runtime_error("[VK_Queue]: Could not submit command buffer to the graphics queue!");
	}

	VkSwapchainKHR swapchains[] = { swapchain };
	VkPresentInfoKHR presentInfo{
		VK_STRUCTURE_TYPE_PRESENT_INFO_KHR, //sType
		nullptr,							//pNext
		1,									//waitSemaphoreCount
		signalSemaphores,					//pWaitSemaphores
		1,									//swapchainCount
		swapchains,							//pSwapchains
		&imageIndex,						//pImageIndices
		nullptr 							//pResults
	};
	vkQueuePresentKHR(graphicsQueue, &presentInfo);

}

void Engine::cleanUp()
{
	vkDestroySemaphore(device, imageAvailableSemaphore, nullptr);
	vkDestroySemaphore(device, renderFinishedSemaphore, nullptr);
	vkDestroyFence(device, inFlightFence, nullptr);

	vkDestroyCommandPool(device, commandPool, nullptr);
	for (VkFramebuffer& framebuffer : swapchainFramebuffers)
	{
		vkDestroyFramebuffer(device, framebuffer, nullptr);
	}

	vkDestroyPipeline(device, graphicsPipeline, nullptr);
	vkDestroyPipelineLayout(device, pipelineLayout, nullptr);
	vkDestroyRenderPass(device, renderPass, nullptr);

	for (VkImageView& imageView : swapchainImageViews)
	{
		vkDestroyImageView(device, imageView, nullptr);
	}

	vkDestroySwapchainKHR(device, swapchain, nullptr);
	vkDestroySurfaceKHR(instance, surface, nullptr);
	
	vkDestroyDevice(device, nullptr);

	if (enableValidationLayers)
	{
		DestoryDebugUtilsMessengerEXT(instance, debugMessenger, nullptr);
	}

	vkDestroyInstance(instance, nullptr);

	glfwDestroyWindow(window);
	glfwTerminate();
}

void Engine::createInstance()
{
	VkApplicationInfo appInfo{
		VK_STRUCTURE_TYPE_APPLICATION_INFO,	//sType
		nullptr,							//pNext
		"TriangleTest",						//pApplicationName
		VK_MAKE_API_VERSION(0, 1, 0, 0),	//applicationVersion
		"NoEngine",							//pEngineName
		VK_MAKE_API_VERSION(0, 1, 0, 0),	//engineVersion
		VK_API_VERSION_1_0					//apiVersion
	};

	std::vector<const char*> extensions = getRequiredInstanceExtensions();
	VkDebugUtilsMessengerCreateInfoEXT debugInstanceCreateInfo{};
	if (enableValidationLayers)
	{
		populateDebugMessengerCreateInfo(debugInstanceCreateInfo);
	}

	VkInstanceCreateInfo createInfo {
		VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,						//sType;
		enableValidationLayers ? &debugInstanceCreateInfo : nullptr,//pNext;
		0,															//flags;
		&appInfo,													//pApplicationInfo;
		static_cast<uint32_t>(enableValidationLayers ? ValidationLayers.size() : 0),
																	//enabledLayerCount;
		enableValidationLayers ? ValidationLayers.data() : nullptr,	//ppEnabledLayerNames;
		static_cast<uint32_t>(extensions.size()),					//enabledExtensionCount;
		extensions.data()											//ppEnabledExtensionNames;
	};

	//CreateInstance
	if (vkCreateInstance(&createInfo, nullptr, &instance) != VK_SUCCESS) {
		throw std::runtime_error("[VK_Instance]: Failed To Create Instance!");
	}

	//Check if all Validation Layer are supported
	if (enableValidationLayers && !checkValidationLayerSupport())
	{
		throw std::runtime_error("[VK_Instance]: Validation Layers requested but not available!");
	}
}

void Engine::pickPhysicalDevice()
{
	uint32_t deviceCount = 0;
	vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr);

	if (deviceCount == 0)
	{
		throw std::runtime_error("[VK_Instance]: No Graphics Devices with Vulkan support!");
	}

	std::vector<VkPhysicalDevice> devices(deviceCount);
	vkEnumeratePhysicalDevices(instance, &deviceCount, devices.data());

	std::multimap<int, VkPhysicalDevice> candidates;
	for (const auto& device : devices)
	{
		int score = rateDeviceSuitability(device);
		candidates.insert(std::make_pair(score, device));
	}

	//the candidate with the highest score is selected
	if (candidates.rbegin()->first > 0)
	{
		physicalDevice = candidates.rbegin()->second;
	} 
	else if (candidates.rbegin()->first == -1)
	{
		throw std::runtime_error("[VK_Instance]: No Supported Graphics Devices with required Queue Families!");
	}
	else if (candidates.rbegin()->first == -2)
	{
		throw std::runtime_error("[VK_Instance]: No Supported Graphics Devices with required Device Extensions!");
	}
	else if (candidates.rbegin()->first == -3)
	{
		throw std::runtime_error("[VK_Instance]: No Supported Graphics Devices with required Swap Chain Support!");
	}

	if (physicalDevice == VK_NULL_HANDLE)
	{
		throw std::runtime_error("[VK_Instance]: No Supported Graphics Devices found!");
	}
}

void Engine::createLogicalDevice()
{
	QueueFamilyIndices indices = queryQueueFamilyIndices(physicalDevice);

	std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;

	//sets don't allow duplicates
	std::set<uint32_t> uniqueQueueFamilies = {
		indices.graphicsFamily.value(),
		indices.presentFamily.value()
	};

	float queuePriority = 1.0f;
	for (uint32_t queueFamily : uniqueQueueFamilies)
	{
		VkDeviceQueueCreateInfo queueCreateInfo {
			VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,	//sType;
			nullptr,									//pNext;
			0,											//flags;
			queueFamily,								//queueFamilyIndex;
			1,											//queueCount;
			&queuePriority								//pQueuePriorities;
		};
		queueCreateInfos.push_back(queueCreateInfo);
	}

	VkPhysicalDeviceFeatures deviceFeatures{};

	VkDeviceCreateInfo deviceCreateInfo {
		VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,						//sType;
		nullptr,													//pNext;
		0,															//flags;
		static_cast<uint32_t>(queueCreateInfos.size()),				//queueCreateInfoCount;
		queueCreateInfos.data(),									//pQueueCreateInfos;
		static_cast<uint32_t>(enableValidationLayers ? ValidationLayers.size() : 0),
																	//enabledLayerCount;
		enableValidationLayers ? ValidationLayers.data() : nullptr,	//ppEnabledLayerNames;
		static_cast<uint32_t>(DeviceExtensions.size()),				//enabledExtensionCount;
		DeviceExtensions.data(),									//ppEnabledExtensionNames;
		&deviceFeatures												//pEnabledFeatures;
	};

	if (vkCreateDevice(physicalDevice, &deviceCreateInfo, nullptr, &device) != VK_SUCCESS)
	{
		throw std::runtime_error("[VK_Device]: Failed to create Logical Device.");
	}

	vkGetDeviceQueue(device, indices.graphicsFamily.value(), 0, &graphicsQueue);
	vkGetDeviceQueue(device, indices.presentFamily.value(), 0, &presentQueue);
}

void Engine::createSurface()
{
	if (glfwCreateWindowSurface(instance, window, nullptr, &surface) != VK_SUCCESS)
	{
		throw std::runtime_error("[VK_Instance]: Failed to create a Surface.");
	}
}

void Engine::createSwapchain()
{
	SwapchainSupportDetails swapchainSupport = querySwapchainSupport(physicalDevice);

	VkSurfaceFormatKHR surfaceFormat = chooseSwapSurfaceFormat(swapchainSupport.formats);
	VkPresentModeKHR presentMode = choostSwapPresentMode(swapchainSupport.presentModes);
	VkExtent2D extent = chooseSwapExtent(swapchainSupport.capabilities);

	//minImageCount may mean waiting on the drivers thus +1
	uint32_t minImageCount = swapchainSupport.capabilities.minImageCount + 1;
	//If minImageCount + 1 is more than maxImageCount
	if (minImageCount > swapchainSupport.capabilities.maxImageCount && swapchainSupport.capabilities.maxImageCount > 0)
	{
		minImageCount = swapchainSupport.capabilities.maxImageCount;
	}

	QueueFamilyIndices indices = queryQueueFamilyIndices(physicalDevice);
	uint32_t queueFamilyIndices[2] = {
		indices.graphicsFamily.value(),
		indices.presentFamily.value()
	};
	bool exclusive = indices.graphicsFamily == indices.presentFamily;

	VkSwapchainCreateInfoKHR createInfo {
		VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,	//sType
		nullptr,										//pNext
		0,												//flags
		surface,										//surface
		minImageCount,									//minImageCount
		surfaceFormat.format,							//imageFormat
		surfaceFormat.colorSpace,						//imageColorSpace
		extent,											//imageExtent
		1,												//imageArrayLayers
		VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,			//imageUsage
		exclusive? VK_SHARING_MODE_EXCLUSIVE : VK_SHARING_MODE_CONCURRENT,
														//imageSharingMode
		static_cast<uint32_t>(exclusive ? 0 : 2),		//queueFamilyIndexCount
		exclusive? nullptr : queueFamilyIndices,		//pQueueFamilyIndices
		swapchainSupport.capabilities.currentTransform,	//preTransform
		VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,				//compositeAlpha
		presentMode,									//presentMode
		VK_TRUE,										//clipped
		VK_NULL_HANDLE									//oldSwapchain
	};

	if (vkCreateSwapchainKHR(device, &createInfo, nullptr, &swapchain) != VK_SUCCESS)
	{
		throw std::runtime_error("[Logical Device]: Swapchain could not be created!");
	}

	uint32_t imageCount = 0;
	vkGetSwapchainImagesKHR(device, swapchain, &imageCount, nullptr);
	swapchainImages.resize(imageCount);
	vkGetSwapchainImagesKHR(device, swapchain, &imageCount, swapchainImages.data());

	swapchainImageFormat = surfaceFormat.format;
	swapchainImageExtent = extent;

	//Create Image Views for Swapchain Images
	swapchainImageViews.resize(swapchainImages.size());
	for (uint32_t i = 0; i < swapchainImages.size(); ++i)
	{
		VkImageViewCreateInfo createInfo {
			VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,	//sType;
			nullptr,									//pNext;
			0,											//flags;
			swapchainImages[i],							//image;
			VK_IMAGE_VIEW_TYPE_2D,						//viewType;
			swapchainImageFormat,						//format;
			VkComponentMapping {						//components;
				VK_COMPONENT_SWIZZLE_IDENTITY,	//r
				VK_COMPONENT_SWIZZLE_IDENTITY,	//g
				VK_COMPONENT_SWIZZLE_IDENTITY,	//b
				VK_COMPONENT_SWIZZLE_IDENTITY	//a
			},
			VkImageSubresourceRange {					//subresourceRange
				VK_IMAGE_ASPECT_COLOR_BIT,		//aspectMask;
				0,								//baseMipLevel;
				1,								//levelCount;
				0,								//baseArrayLayer;
				1,								//layerCount;
			}
		};
		if (vkCreateImageView(device, &createInfo, nullptr, &swapchainImageViews[i]) != VK_SUCCESS) {
			throw std::runtime_error("[Swapchain]: Failed to create Swapchain Image Views!");
		}
	}

}

void Engine::createRenderPass()
{
	VkAttachmentDescription colorAttachment{
		0,									//flags
		swapchainImageFormat,				//format
		VK_SAMPLE_COUNT_1_BIT,				//samples
		VK_ATTACHMENT_LOAD_OP_CLEAR,		//loadOp
		VK_ATTACHMENT_STORE_OP_STORE,		//storeOp
		VK_ATTACHMENT_LOAD_OP_DONT_CARE,	//stencilLoadOp
		VK_ATTACHMENT_STORE_OP_DONT_CARE,	//stencilStoreOp
		VK_IMAGE_LAYOUT_UNDEFINED,			//initialLayout
		VK_IMAGE_LAYOUT_PRESENT_SRC_KHR		//finalLayout
	};

	VkAttachmentReference colorAttachmentReference{
		0,											//attachment
		VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL	//layout
	};

	VkSubpassDescription subpass{
		0,									//flags
		VK_PIPELINE_BIND_POINT_GRAPHICS,	//pipelineBindPoint
		0,									//inputAttachmentCount
		0,									//pInputAttachments
		1,									//colorAttachmentCount
		&colorAttachmentReference,			//pColorAttachments
		nullptr,							//pResolveAttachments
		nullptr,							//pDepthStencilAttachment
		0,									//preserveAttachmentCount
		nullptr,							//pPreserveAttachments
	};

	VkSubpassDependency subpassDependency{
		VK_SUBPASS_EXTERNAL,							//srcSubpass
		0,												//dstSubpass
		VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,	//srcStageMask
		VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,	//dstStageMask
		0,												//srcAccessMask
		VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,			//dstAccessMask
		0												//dependencyFlags
	};

	VkRenderPassCreateInfo renderPassInfo{
		VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,	//sType
		0,											//pNext
		0,											//flags
		1,											//attachmentCount
		&colorAttachment,							//pAttachments
		1,											//subpassCount
		&subpass,									//pSubpasses
		1,											//dependencyCount
		&subpassDependency							//pDependencies
	};

	if (vkCreateRenderPass(device, &renderPassInfo, nullptr, &renderPass) != VK_SUCCESS) {
		throw std::runtime_error("[VK_Device]: Failed to create render pass!");
	}
}

void Engine::createGraphicsPipeline()
{
	std::vector<char> vertexShaderCode = readFile(utils::getExecutableDir() / "res\\shaders\\vert.spv");
	std::vector<char> fragmentShaderCode = readFile(utils::getExecutableDir() / "res\\shaders\\frag.spv");

	VkShaderModule vertShaderModule = createShaderModule(vertexShaderCode);
	VkShaderModule fragShaderModule = createShaderModule(fragmentShaderCode);

	VkPipelineShaderStageCreateInfo vertShaderStageInfo{
		VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,	//sType
		nullptr,												//pNext
		0,														//flags
		VK_SHADER_STAGE_VERTEX_BIT,								//stage
		vertShaderModule,										//module
		"main",													//pName -> Entrypoint
		nullptr													//pSpecializationInfo
	};

	VkPipelineShaderStageCreateInfo fragShaderStageInfo {
		VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,	//sType
		nullptr,												//pNext
		0,														//flags
		VK_SHADER_STAGE_FRAGMENT_BIT,							//stage
		fragShaderModule,										//module
		"main",													//pName
		nullptr													//pSpecializationInfo
	};

	VkPipelineShaderStageCreateInfo shaderStages[] = {
		vertShaderStageInfo,
		fragShaderStageInfo
	};

	VkPipelineVertexInputStateCreateInfo vertexInputInfo {
		VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,	//sType
		nullptr,										//pNext;
		0,												//flags;
		0,												//vertexBindingDescriptionCount;
		nullptr,										//pVertexBindingDescriptions;
		0,												//vertexAttributeDescriptionCount;
		nullptr											//pVertexAttributeDescriptions;
	};

	VkPipelineInputAssemblyStateCreateInfo inputAssemblyInfo {
		VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,	//sType
		nullptr,														//pNext
		0,																//flags
		VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,							//topology
		VK_FALSE											//primitiveRestartEnable
	};

	std::vector<VkDynamicState> dynamicStates = {
		VK_DYNAMIC_STATE_VIEWPORT,
		VK_DYNAMIC_STATE_SCISSOR
	};

	VkPipelineDynamicStateCreateInfo dynamicStateCreateInfo {
		VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,	//sType
		nullptr,												//pNext
		0,														//flags
		static_cast<uint32_t>(dynamicStates.size()),			//dynamicStateCount
		dynamicStates.data()									//pDynamicStates
	};

	VkPipelineViewportStateCreateInfo viewportStateInfo {
		VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,	//sType
		nullptr,												//pNext
		0,														//flags
		1,														//viewportCount
		nullptr,												//pViewports
		1,														//scissorCount
		nullptr													//pScissors
	};
	// We'll define the viewports and scissors at draw time

	VkPipelineRasterizationStateCreateInfo rasterizationStateInfo {
		VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,	//sType
		nullptr,													//pNext
		0,															//flags
		VK_FALSE,													//depthClampEnable
		VK_FALSE,													//rasterizerDiscardEnable
		VK_POLYGON_MODE_FILL,										//polygonMode
		VK_CULL_MODE_BACK_BIT,										//cullMode
		VK_FRONT_FACE_CLOCKWISE,									//frontFace
		VK_FALSE,													//depthBiasEnable
		0.0f,														//depthBiasConstantFactor
		0.0f,														//depthBiasClamp
		0.0f,														//depthBiasSlopeFactor
		1.0f,														//lineWidth
	};

	VkPipelineMultisampleStateCreateInfo multisampleInfo{
		VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,	//sType
		nullptr,													//pNext
		0,															//flags
		VK_SAMPLE_COUNT_1_BIT,										//rasterizationSamples
		VK_FALSE,													//sampleShadingEnable
		1.0f,														//minSampleShading
		nullptr,													//pSampleMask
		VK_FALSE,													//alphaToCoverageEnable
		VK_FALSE,													//alphaToOneEnable
	};

	VkPipelineColorBlendAttachmentState colorBlendAttachment{
		VK_FALSE,								//blendEnable
		VK_BLEND_FACTOR_ONE,					//srcColorBlendFactor
		VK_BLEND_FACTOR_ZERO,					//dstColorBlendFactor
		VK_BLEND_OP_ADD,						//colorBlendOp
		VK_BLEND_FACTOR_ONE,					//srcAlphaBlendFactor
		VK_BLEND_FACTOR_ZERO,					//dstAlphaBlendFactor
		VK_BLEND_OP_ADD,						//alphaBlendOp
		VK_COLOR_COMPONENT_R_BIT |				//colorWriteMask
		VK_COLOR_COMPONENT_G_BIT |
		VK_COLOR_COMPONENT_B_BIT |
		VK_COLOR_COMPONENT_A_BIT
	};
	// I'm not sure what the colorWriteMask should be so check everything once

	VkPipelineColorBlendStateCreateInfo colorBlendInfo{
		VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,	//sType
		nullptr,													//pNext
		0,															//flags
		VK_FALSE,				                                    //logicOpEnable
		VK_LOGIC_OP_COPY,		                                    //logicOp
		1,					                                        //attachmentCount
		&colorBlendAttachment,										//pAttachments
		{															//blendConstants[4]
			0.0f,
			0.0f,
			0.0f,
			0.0f
		}
	};

	VkPipelineLayoutCreateInfo pipelineLayoutInfo{
		VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,	//sType
		nullptr,										//pNext
		0,												//flags
		0,												//setLayoutCount
		nullptr,										//pSetLayouts
		0,												//pushConstantRangeCount
		nullptr,										//pPushConstantRanges
	};

	if (vkCreatePipelineLayout(device, &pipelineLayoutInfo, nullptr, &pipelineLayout))
	{
		throw std::runtime_error("[VK_Device]: Failed to Create Pipeline Layout.");
	}

	VkGraphicsPipelineCreateInfo pipelineInfo{
		VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,	//sType
		nullptr,											//pNext
		0,													//flags
		2,													//stageCount
		shaderStages,										//pStages
		&vertexInputInfo,									//pVertexInputState
		&inputAssemblyInfo,									//pInputAssemblyState
		nullptr,											//pTessellationState
		&viewportStateInfo,									//pViewportState
		&rasterizationStateInfo,							//pRasterizationState
		&multisampleInfo,									//pMultisampleState
		nullptr,											//pDepthStencilState
		&colorBlendInfo,									//pColorBlendState
		&dynamicStateCreateInfo,							//pDynamicState
		pipelineLayout,										//layout
		renderPass,											//renderPass
		0,													//subpass
		VK_NULL_HANDLE,                                     //basePipelineHandle
		-1													//basePipelineIndex
	};

	if (vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &graphicsPipeline) != VK_SUCCESS) 
	{
		throw std::runtime_error("[VK_Device]: Failed to create graphics pipeline!");
	}

	vkDestroyShaderModule(device, vertShaderModule, nullptr);
	vkDestroyShaderModule(device, fragShaderModule, nullptr);
}

void Engine::createFramebuffers()
{
	swapchainFramebuffers.resize(swapchainImages.size());

	for (size_t i = 0; i < swapchainImageViews.size(); ++i)
	{
		VkImageView attachments[] = { swapchainImageViews[i] };

		VkFramebufferCreateInfo framebufferInfo{
			VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,	//sType
			nullptr,									//pNext
			0,											//flags
			renderPass,									//renderPass
			1,											//attachmentCount
			attachments,								//pAttachments
			swapchainImageExtent.width,					//width
			swapchainImageExtent.height,				//height
			1,											//layers
		};

		if (vkCreateFramebuffer(device, &framebufferInfo, nullptr, &swapchainFramebuffers[i]) != VK_SUCCESS)
		{
			throw std::runtime_error("[VK_Device]: Failed to create Framebuffer!");
		}
	}
}

void Engine::createCommandPool()
{
	QueueFamilyIndices queueFamilyIndices = queryQueueFamilyIndices(physicalDevice);

	VkCommandPoolCreateInfo commandPoolCreateInfo{
		VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,			//sType
		nullptr,											//pNext
		VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,	//flags
		queueFamilyIndices.graphicsFamily.value()			//queueFamilyIndex
	};

	if (vkCreateCommandPool(device, &commandPoolCreateInfo, nullptr, &commandPool) != VK_SUCCESS) {
		throw std::runtime_error("[VK_Device]: Unable to create Command Pool!");
	}
}

void Engine::createCommandBuffer()
{
	VkCommandBufferAllocateInfo commandBufferAllocateInfo{
		VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO, //sType
		nullptr,										//pNext
		commandPool,									//commandPool
		VK_COMMAND_BUFFER_LEVEL_PRIMARY,				//level
		1												//commandBufferCount
	};

	if (vkAllocateCommandBuffers(device, &commandBufferAllocateInfo, &commandBuffer) != VK_SUCCESS) {
		throw std::runtime_error("[VK_Device]: Couldn't allocate Command Buffer!");
	}
}

void Engine::createSyncObjects()
{
	VkSemaphoreCreateInfo semaphoreCreateInfo{
		VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,	//sType
		nullptr,									//pNext
		0											//flags
	};

	VkFenceCreateInfo fenceCreateInfo{
		VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,		//sType
		nullptr,									//pNext
		VK_FENCE_CREATE_SIGNALED_BIT				//flags	-> first frame waits for a signaled fence
	};

	if (vkCreateSemaphore(device, &semaphoreCreateInfo, nullptr, &imageAvailableSemaphore) != VK_SUCCESS ||
		vkCreateSemaphore(device, &semaphoreCreateInfo, nullptr, &renderFinishedSemaphore) != VK_SUCCESS ||
		vkCreateFence(device, &fenceCreateInfo, nullptr, &inFlightFence) != VK_SUCCESS) 
	{
		throw std::runtime_error("[VK_Device]: Couldn't create necessary Synchronization Objects!");
	}

}

void Engine::recordCommandBuffer(VkCommandBuffer _commandBuffer, uint32_t _imageIndex)
{
	VkCommandBufferBeginInfo commandBufferBeginInfo{
		VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,	//sType
		nullptr,										//pNext
		0,												//flags
		nullptr											//pInheritanceInfo
	};
	if (vkBeginCommandBuffer(commandBuffer, &commandBufferBeginInfo) != VK_SUCCESS) {
		throw std::runtime_error("[VK_CommandBuffer]: Couldn't begin recording Command Buffer!");
	}
	
	VkClearValue clearColorValue = { {{0.0f, 0.0f, 0.0f, 1.0f}} };
	VkRenderPassBeginInfo renderPassBeginInfo{
		VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,	//sType
		nullptr,									//pNext
		renderPass,									//renderPass
		swapchainFramebuffers[_imageIndex],			//framebuffer
		VkRect2D {									//renderArea
			VkOffset2D { 0, 0 },
			swapchainImageExtent 
		},
		1,											//clearValueCount
		&clearColorValue							//pClearValues
	};
	vkCmdBeginRenderPass(commandBuffer, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

	vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipeline);

	VkViewport viewport{
		0.0f,											//x
		0.0f,											//y
		static_cast<float>(swapchainImageExtent.width), //width
		static_cast<float>(swapchainImageExtent.height),//height
		0.0f,											//minDepth
		1.0f											//maxDepth
	};
	vkCmdSetViewport(commandBuffer, 0, 1, &viewport);

	VkRect2D scissor{
		VkOffset2D {0, 0},		//offset
		swapchainImageExtent	//extent
	};
	vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

	vkCmdDraw(commandBuffer, 3, 1, 0, 0);

	vkCmdEndRenderPass(commandBuffer);

	if (vkEndCommandBuffer(commandBuffer) != VK_SUCCESS) {
		throw std::runtime_error("[VK_CommandBuffer]: Couldn't end recording Command Buffer!");
	}
}

int Engine::rateDeviceSuitability(VkPhysicalDevice _device)
{
	//if all required queue families aren't found, don't use the device
	if (!queryQueueFamilyIndices(_device).isComplete())
	{
		return -1;
	}
	if (!checkDeviceExtensionSupport(_device))
	{
		return -2;
	}

	SwapchainSupportDetails SwapchainDetails = querySwapchainSupport(_device);
	bool swapchainAdequate = 
		!SwapchainDetails.formats.empty() || !SwapchainDetails.presentModes.empty();
	if (!swapchainAdequate)
	{
		return -3;
	}

	int score = 1;

	VkPhysicalDeviceProperties properties;
	VkPhysicalDeviceFeatures features;
	vkGetPhysicalDeviceProperties(_device, &properties);
	vkGetPhysicalDeviceFeatures(_device, &features);

	if (properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU)
	{
		score += 1000;
	}
	if (queryQueueFamilyIndices(_device).graphicsFamily == queryQueueFamilyIndices(_device).presentFamily)
	{
		//better performance if graphics and presentation is done on the same queue
		score += 50;
		//Note: It may actually be faster with multiple queues running concurrently but that would require manual tranfer of ownership. Look into it later.
	}

	return score;
}

//Returns the indices of all required Queue Families supported by the device.
Engine::QueueFamilyIndices Engine::queryQueueFamilyIndices(VkPhysicalDevice _device)
{
	QueueFamilyIndices indices;

	uint32_t queueFamilyCount = 0;
	vkGetPhysicalDeviceQueueFamilyProperties(_device, &queueFamilyCount, nullptr);

	std::vector<VkQueueFamilyProperties> queueFamilyProperties(queueFamilyCount);
	vkGetPhysicalDeviceQueueFamilyProperties(_device, &queueFamilyCount, queueFamilyProperties.data());

	for (uint32_t i = 0; i < queueFamilyCount; ++i)
	{
		if (queueFamilyProperties.at(i).queueFlags & VK_QUEUE_GRAPHICS_BIT)
		{
			indices.graphicsFamily = i;
		}

		VkBool32 presentSupport = false;
		vkGetPhysicalDeviceSurfaceSupportKHR(_device, i, surface, &presentSupport);
		if (presentSupport)
		{
			indices.presentFamily = i;
		}

		if (indices.isComplete())
		{
			break;
		}
	}

	return indices;
}

//Returns the details of the Swapchain provided by the device.
Engine::SwapchainSupportDetails Engine::querySwapchainSupport(VkPhysicalDevice _device)
{
	SwapchainSupportDetails details;

	vkGetPhysicalDeviceSurfaceCapabilitiesKHR(_device, surface, &(details.capabilities));

	uint32_t formatCount, modeCount = 0;
	vkGetPhysicalDeviceSurfaceFormatsKHR(_device, surface, &formatCount, nullptr);
	vkGetPhysicalDeviceSurfacePresentModesKHR(_device, surface, &modeCount, nullptr);

	if (formatCount != 0)
	{
		details.formats.resize(formatCount);
		vkGetPhysicalDeviceSurfaceFormatsKHR(_device, surface, &formatCount, details.formats.data());
	}

	if (modeCount != 0)
	{
		details.presentModes.resize(modeCount);
		vkGetPhysicalDeviceSurfacePresentModesKHR(_device, surface, &modeCount, details.presentModes.data());
	}

	return details;
}

//Check if Device Extensions required by the application are supported by the device.
bool Engine::checkDeviceExtensionSupport(VkPhysicalDevice _device)
{
	//Get all extensions supported by the device
	uint32_t extensionCount = 0;
	vkEnumerateDeviceExtensionProperties(_device, nullptr, &extensionCount, nullptr);

	std::vector<VkExtensionProperties> availableExtensions(extensionCount);
	vkEnumerateDeviceExtensionProperties(_device, nullptr, &extensionCount, availableExtensions.data());

	//Make a set of all required extensions and remove them from the set if they're found. If all required extensions are found, the set is empty.
	std::set<std::string> requiredExtensions(DeviceExtensions.begin(), DeviceExtensions.end());
	for (const auto& extension : availableExtensions)
	{
		requiredExtensions.erase(extension.extensionName);
	}

	return requiredExtensions.empty();
}

//Returns the Surface Format to be used by the Swapchain.
VkSurfaceFormatKHR Engine::chooseSwapSurfaceFormat(std::vector<VkSurfaceFormatKHR> _surfaceFormats)
{
	for (const VkSurfaceFormatKHR& surfaceFormat : _surfaceFormats)
	{
		if (surfaceFormat.format == VK_FORMAT_B8G8R8A8_SRGB && surfaceFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
		{
			return surfaceFormat;
		}
	}

	return _surfaceFormats[0];
}

//Returns the way in which the Swapchain present images to the screen (Vsync, Triple Buffering etc.)
VkPresentModeKHR Engine::choostSwapPresentMode(std::vector<VkPresentModeKHR> _presentModes)
{
	for (const VkPresentModeKHR& presentMode : _presentModes)
	{
		if (presentMode == VK_PRESENT_MODE_MAILBOX_KHR)
		{
			//if Triple Buffering is supported, use it.
			return presentMode;
		}
	}

	//VSync (is required to be supported by Vulkan)
	return VK_PRESENT_MODE_FIFO_KHR;
}

VkExtent2D Engine::chooseSwapExtent(const VkSurfaceCapabilitiesKHR _capabilities)
{
	if (_capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max())
	{
		return _capabilities.currentExtent;
	}
	else {
		int width, height;
		glfwGetFramebufferSize(window, &width, &height);

		VkExtent2D out = {
			static_cast<uint32_t>(width),
			static_cast<uint32_t>(height)
		};

		out.width = std::clamp(out.width, _capabilities.maxImageExtent.width, _capabilities.minImageExtent.width);
		out.height = std::clamp(out.height, _capabilities.maxImageExtent.height, _capabilities.minImageExtent.height);

		return out;
	}
}

//Check if Instance Extensions required by the application are supported by the Vulkan Implementation and return them.
std::vector<const char*> Engine::getRequiredInstanceExtensions()
{
	//Gathering extensions required by GLFW
	uint32_t glfwExtensionCount = 0;
	const char** glfwExtensions;
	glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

	//Lambda to check if the required instance extension is supported by the Vulkan implementation.
	auto checkInstanceExtensionSupport = [](const char* pExtension)
	{
		uint32_t extensionCount = 0;
		vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, nullptr);

		std::vector<VkExtensionProperties> availableExtensions(extensionCount);
		vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, availableExtensions.data());

		for (const auto& avail : availableExtensions) {
			if (strcmp(pExtension, avail.extensionName) == 0) {
				return true;
			}
		}

		return false;
	};

	for (uint32_t i = 0; i < glfwExtensionCount; ++i)
	{
		if (!checkInstanceExtensionSupport(glfwExtensions[i]))
		{
			throw std::runtime_error("[VK_Instance]: Extensions required by GLFW not available!");
		}
	}

	std::vector<const char*> extensions(glfwExtensions, glfwExtensions + glfwExtensionCount);

	if (enableValidationLayers) {
		if (!checkInstanceExtensionSupport(VK_EXT_DEBUG_UTILS_EXTENSION_NAME))
		{
			throw std::runtime_error("[VK_Instance]: VK_EXT_debug_utils not available!");
		}
		extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);

		if (checkInstanceExtensionSupport(VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME))
		{
			extensions.push_back(VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME);
		}
		else {
			std::cout << "[VK_Instance]: VK_KHR_get_physical_device_properties2 couldn't be loaded. Find stack trace to this function lol.";
		}
		//need to include this extension to prevent this warning(possibly wrong):
		//[Validation Layer]: vkGetPhysicalDeviceProperties2KHR: Emulation found unrecognized structure type in pProperties->pNext - this struct will be ignored
		//Genuinely think this is a bug with the vulkanSDK cuz it throws on the vkCreateDevice call which then calls vkGetPhysicalDeviceProperties
	}

	return extensions;
}

bool Engine::checkValidationLayerSupport()
{
	uint32_t layerCount = 0;
	vkEnumerateInstanceLayerProperties(&layerCount, nullptr);

	std::vector<VkLayerProperties> availableLayers(layerCount);
	vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());

	for (const auto& layer : ValidationLayers)
	{
		bool found = false;
		for (const auto& avail : availableLayers) {
			if (strcmp(layer, avail.layerName) == 0) {
				found = true;
				break;
			}
		}

		if (!found) return false;
	}

	return true;
}

void Engine::setupDebugMessenger()
{
	if (!enableValidationLayers) 
		return;

	VkDebugUtilsMessengerCreateInfoEXT createInfo{};
	populateDebugMessengerCreateInfo(createInfo);

	if (CreateDebugUtilsMessengerEXT(instance, &createInfo, nullptr, &debugMessenger) != VK_SUCCESS)
	{
		throw std::runtime_error("[VK_Instance]: Failed to Setup Debug Messenger!");
	}
}

std::vector<char> Engine::readFile(const std::filesystem::path& filename) {
	std::ifstream file(filename, std::ios::ate | std::ios::binary);

	if (!file.is_open()) {
		throw std::runtime_error(std::string("[CPU]: Failed to open file at: ") + filename.string());
	}

	std::cout << "[CPU]: Reading: " + filename.string() << "\n";
	size_t fileSize = static_cast<size_t>(file.tellg());
	std::vector<char> buffer(fileSize);

	file.seekg(0);
	file.read(buffer.data(), fileSize);

	file.close();

	return buffer;
}

VkShaderModule Engine::createShaderModule(std::vector<char>& code)
{
	VkShaderModuleCreateInfo createInfo {
		VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,			//sType
		nullptr,												//pNext
		0,														//flags
		code.size(),											//codeSize
		reinterpret_cast<const uint32_t*>(code.data())			//pCode
	};

	VkShaderModule shaderModule;

	if (vkCreateShaderModule(device, &createInfo, nullptr, &shaderModule) != VK_SUCCESS)
	{
		throw std::runtime_error("[VK_Device]: Failed to Create Shader Module.");
	}

	return shaderModule;
}

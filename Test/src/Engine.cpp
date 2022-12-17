#include "Engine.hpp"
#include <iostream>
#include <vector>
#include <stdexcept>
#include <cstring>
#include <map>
#include <set>

#include "utils/debugUtils.hpp"

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
}

void Engine::mainLoop()
{
	while (!glfwWindowShouldClose(window))
	{
		glfwPollEvents();
	}
}

void Engine::cleanUp()
{
	vkDestroyDevice(device, nullptr);

	vkDestroySurfaceKHR(instance, surface, nullptr);

	if (enableValidationLayers)
	{
		DestoryDebugUtilsMessenger(instance, debugMessenger, nullptr);
	}

	vkDestroyInstance(instance, nullptr);

	glfwDestroyWindow(window);

	glfwTerminate();
}

void Engine::createInstance()
{
	VkApplicationInfo appInfo{};
	appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
	appInfo.pNext = nullptr;
	appInfo.pApplicationName = "TriangleTest";
	appInfo.applicationVersion = VK_MAKE_API_VERSION(0, 1, 0, 0);
	appInfo.pEngineName = "NoEngine";
	appInfo.engineVersion = VK_MAKE_API_VERSION(0, 1, 0, 0);
	appInfo.apiVersion = VK_API_VERSION_1_0;

	VkInstanceCreateInfo createInfo{}; //value initialization
	createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
	createInfo.pApplicationInfo = &appInfo;

	std::vector<const char*> extensions = getRequiredExtensions();
	createInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
	createInfo.ppEnabledExtensionNames = extensions.data();

	VkDebugUtilsMessengerCreateInfoEXT debugInstanceCreateInfo{};
	if (enableValidationLayers)
	{
		createInfo.enabledLayerCount = static_cast<uint32_t>(ValidationLayers.size());
		createInfo.ppEnabledLayerNames = ValidationLayers.data();

		populateDebugMessengerCreateInfo(debugInstanceCreateInfo);
		createInfo.pNext = &debugInstanceCreateInfo;
	}
	else {
		createInfo.enabledLayerCount = 0;
		createInfo.pNext = nullptr;
	}

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

	if (physicalDevice == VK_NULL_HANDLE)
	{
		throw std::runtime_error("[VK_Instance]: No Supported Graphics Devices found!");
	}
}

void Engine::createLogicalDevice()
{
	QueueFamilyIndices indices = getQueueFamilies(physicalDevice);

	std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;

	//sets don't allow duplicates
	std::set<uint32_t> uniqueQueueFamilies = {
		indices.graphicsFamily.value(),
		indices.presentFamily.value()
	};

	float queuePriority = 1.0f;
	for (uint32_t queueFamily : uniqueQueueFamilies)
	{
		VkDeviceQueueCreateInfo queueCreateInfo{};
		queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
		queueCreateInfo.queueFamilyIndex = queueFamily;
		queueCreateInfo.queueCount = 1;
		queueCreateInfo.pQueuePriorities = &queuePriority;
		queueCreateInfos.push_back(queueCreateInfo);
	}

	VkPhysicalDeviceFeatures deviceFeatures{};

	VkDeviceCreateInfo deviceCreateInfo{};
	deviceCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
	deviceCreateInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());
	deviceCreateInfo.pQueueCreateInfos = queueCreateInfos.data();
	deviceCreateInfo.pEnabledFeatures = &deviceFeatures;

	if (enableValidationLayers)
	{
		deviceCreateInfo.enabledLayerCount = static_cast<uint32_t>(ValidationLayers.size());
		deviceCreateInfo.ppEnabledLayerNames = ValidationLayers.data();
	}
	else {
		deviceCreateInfo.enabledLayerCount = 0;
	}

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

int Engine::rateDeviceSuitability(VkPhysicalDevice device)
{
	//if all required queue families aren't found, don't use the device
	if (!getQueueFamilies(device).isComplete())
	{
		return -1;
	}

	int score = 1;

	VkPhysicalDeviceProperties properties;
	VkPhysicalDeviceFeatures features;
	vkGetPhysicalDeviceProperties(device, &properties);
	vkGetPhysicalDeviceFeatures(device, &features);

	if (properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU)
	{
		score += 1000;
	}
	if (getQueueFamilies(device).graphicsFamily == getQueueFamilies(device).presentFamily)
	{
		//better performance if graphics and presentation is done on the same queue
		score += 50;
		//Note: It may actually be faster with multiple queues running concurrently but that would require manual tranfer of ownership. Look into it later.
	}

	return score;
}

QueueFamilyIndices Engine::getQueueFamilies(VkPhysicalDevice device)
{
	QueueFamilyIndices indices;

	uint32_t queueFamilyCount = 0;
	vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);

	std::vector<VkQueueFamilyProperties> queueFamilyProperties(queueFamilyCount);
	vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilyProperties.data());

	for (uint32_t i = 0; i < queueFamilyCount; ++i)
	{
		if (queueFamilyProperties.at(i).queueFlags & VK_QUEUE_GRAPHICS_BIT)
		{
			indices.graphicsFamily = i;
		}

		VkBool32 presentSupport = false;
		vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface, &presentSupport);
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

void Engine::setupDebugMessenger()
{
	if (!enableValidationLayers) 
		return;

	VkDebugUtilsMessengerCreateInfoEXT createInfo{};
	populateDebugMessengerCreateInfo(createInfo);

	if (CreateDebugUtilsMessenger(instance, &createInfo, nullptr, &debugMessenger) != VK_SUCCESS)
	{
		throw std::runtime_error("[VK_Instance]: Failed to Setup Debug Messenger!");
	}
}

std::vector<const char*> Engine::getRequiredExtensions()
{
	//Gathering extensions required by GLFW
	uint32_t glfwExtensionCount = 0;
	const char** glfwExtensions;
	glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

	for (uint32_t i = 0; i < glfwExtensionCount; ++i)
	{
		if (!checkExtensionSupport(glfwExtensions[i]))
		{
			throw std::runtime_error("[VK_Instance]: Extensions required by GLFW not available!");
		}
	}

	std::vector<const char*> extensions(glfwExtensions, glfwExtensions + glfwExtensionCount);

	if (enableValidationLayers) {
		if (!checkExtensionSupport(VK_EXT_DEBUG_UTILS_EXTENSION_NAME))
		{
			throw std::runtime_error("[VK_Instance]: VK_EXT_debug_utils not available!");
		}
		extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);

		if (checkExtensionSupport(VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME))
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

bool Engine::checkExtensionSupport(const char* pExtension)
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
}

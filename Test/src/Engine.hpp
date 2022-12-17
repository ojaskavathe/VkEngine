#pragma once
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <vector>
#include <optional>

const uint32_t WIDTH = 800;
const uint32_t HEIGHT = 600;

const std::vector<const char*> ValidationLayers = {
	"VK_LAYER_KHRONOS_validation"
};

#ifdef _DEBUG
const bool enableValidationLayers = true;
#else
const bool enableValidationLayers = false;
#endif // If in release mode, no validation layers are to be used.

struct QueueFamilyIndices {
	std::optional<uint32_t> graphicsFamily;
	std::optional<uint32_t> presentFamily;

	bool isComplete()
	{
		return graphicsFamily.has_value() && presentFamily.has_value();
	}
};

class Engine
{
private:
	GLFWwindow* window;
	VkInstance instance;
	VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
	VkDevice device;

	VkSurfaceKHR surface;

	VkQueue graphicsQueue;
	VkQueue presentQueue;

	VkDebugUtilsMessengerEXT debugMessenger;

public:
	void run();

private:
	void initWindow();
	void initVulkan();
	void mainLoop();
	void cleanUp();

	void createInstance();
	void pickPhysicalDevice();
	void createLogicalDevice();

	void createSurface();

	int rateDeviceSuitability(VkPhysicalDevice device);
	QueueFamilyIndices getQueueFamilies(VkPhysicalDevice device);

	void setupDebugMessenger();	

	std::vector<const char*> getRequiredExtensions();

	bool checkValidationLayerSupport();
	bool checkExtensionSupport(const char* _extension);
};
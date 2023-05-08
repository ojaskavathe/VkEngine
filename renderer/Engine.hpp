#pragma once
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <vector>
#include <optional>
#include <string>

const uint32_t WIDTH = 800;
const uint32_t HEIGHT = 600;

#ifdef _DEBUG
const bool enableValidationLayers = true;
#else
const bool enableValidationLayers = false;
#endif // If in release mode, no validation layers are to be used.

class Engine
{
private:
	GLFWwindow* window;
	VkInstance instance;
	VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
	VkDevice device;

	VkSurfaceKHR surface;

	VkSwapchainKHR swapchain;
	std::vector<VkImage> swapchainImages;
	std::vector<VkImageView> swapchainImageViews;
	VkFormat swapchainImageFormat;
	VkExtent2D swapchainImageExtent;

	std::vector<VkFramebuffer> swapchainFramebuffers;

	VkQueue graphicsQueue;
	VkQueue presentQueue;

	VkRenderPass renderPass;
	VkPipelineLayout pipelineLayout;

	VkPipeline graphicsPipeline;

	VkCommandPool commandPool;
	VkCommandBuffer commandBuffer;

	VkSemaphore imageAvailableSemaphore;
	VkSemaphore renderFinishedSemaphore;
	VkFence inFlightFence;

	VkDebugUtilsMessengerEXT debugMessenger;

	const std::vector<const char*> ValidationLayers = {
		"VK_LAYER_KHRONOS_validation"
	};

	const std::vector<const char*> DeviceExtensions = {
		VK_KHR_SWAPCHAIN_EXTENSION_NAME
	};

	struct QueueFamilyIndices {
		//The Physical Device may not support all Queue Families.
		std::optional<uint32_t> graphicsFamily;
		std::optional<uint32_t> presentFamily;

		bool isComplete()
		{
			return graphicsFamily.has_value() && presentFamily.has_value();
		}
	};

	struct SwapchainSupportDetails {
		VkSurfaceCapabilitiesKHR capabilities;
		std::vector<VkSurfaceFormatKHR> formats;
		std::vector<VkPresentModeKHR> presentModes;
	};

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
	void createSwapchain();
	void createRenderPass();
	void createGraphicsPipeline();
	void createFramebuffers();
	void createCommandPool();
	void createCommandBuffer();
	void createSyncObjects();

	void drawFrame();

	void recordCommandBuffer(VkCommandBuffer _commandBuffer, uint32_t _imageIndex);

	std::vector<const char*> getRequiredInstanceExtensions();

	int rateDeviceSuitability(VkPhysicalDevice _device);
	bool checkDeviceExtensionSupport(VkPhysicalDevice _device);
	QueueFamilyIndices queryQueueFamilyIndices(VkPhysicalDevice _device);
	SwapchainSupportDetails querySwapchainSupport(VkPhysicalDevice _device);

	VkSurfaceFormatKHR chooseSwapSurfaceFormat(std::vector<VkSurfaceFormatKHR> _surfaceFormats);
	VkPresentModeKHR choostSwapPresentMode(std::vector<VkPresentModeKHR> _presentModes);
	VkExtent2D chooseSwapExtent(const VkSurfaceCapabilitiesKHR _capabilities);

	bool checkValidationLayerSupport();
	void setupDebugMessenger();

	static std::vector<char> readFile(const std::string& filename);
	VkShaderModule createShaderModule(std::vector<char>& code);
};
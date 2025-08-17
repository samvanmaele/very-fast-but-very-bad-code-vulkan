#pragma once
#include <SDL3/SDL_video.h>
#include <volk.h>
#include <vector>

#include "common_structs.hpp"

class DeviceManager
{
    public:
        VkInstance instance;
        VkSurfaceKHR surface;
        VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
        VkDevice device;
        VkQueue graphicsQueue;
        VkQueue presentQueue;
        QueueFamilyIndices indices;

        void createInstance(SDL_Window* window, bool enableValidationLayers, std::vector<const char*> validationLayers, VkDebugUtilsMessengerCreateInfoEXT &debugCreateInfo);
        bool checkPhysicalDevice();
        int rateDeviceSuitability(VkPhysicalDevice physicalDevice);
        SwapChainSupportDetails querySwapChainSupport(VkPhysicalDevice physicalDevice);
        bool checkDeviceExtensionSupport(VkPhysicalDevice physicalDevice);
        QueueFamilyIndices findQueueFamilies(VkPhysicalDevice physicalDevice);
        void createLogicalDevice(QueueFamilyIndices &indices, bool enableValidationLayers, std::vector<const char*> validationLayers);

    private:
        const std::vector<const char*> deviceExtensions = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};

        std::vector<const char*> getRequiredExtensions(bool enableValidationLayers, SDL_Window* window);
};
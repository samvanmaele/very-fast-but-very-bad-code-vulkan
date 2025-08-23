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

        void createInstance(SDL_Window* window);
        bool checkPhysicalDevice(int USE_IGPU);
        int rateDeviceSuitability(VkPhysicalDevice physicalDevice, int USE_IGPU);
        SwapChainSupportDetails querySwapChainSupport(VkPhysicalDevice physicalDevice);
        bool checkDeviceExtensionSupport(VkPhysicalDevice physicalDevice);
        QueueFamilyIndices findQueueFamilies(VkPhysicalDevice physicalDevice);
        void createLogicalDevice(QueueFamilyIndices &indices);

    private:
        const std::vector<const char*> deviceExtensions = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};

        std::vector<const char*> getRequiredExtensions(SDL_Window* window);
};
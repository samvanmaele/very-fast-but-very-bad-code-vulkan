#pragma once
#include <volk.h>
#include <optional>
#include <vector>
#include <glm/glm.hpp>

const int MAX_FRAMES_IN_FLIGHT = 7;
const int WIDTH = 200;
const int HEIGHT = 200;

struct QueueFamilyIndices
{
    std::optional<uint32_t> graphicsFamily;
    std::optional<uint32_t> presentFamily;

    bool isComplete()
    {
        return graphicsFamily.has_value() && presentFamily.has_value();
    }
};

struct SwapChainSupportDetails
{
    VkSurfaceCapabilitiesKHR capabilities;
    std::vector<VkSurfaceFormatKHR> formats;
    std::vector<VkPresentModeKHR> presentModes;
};
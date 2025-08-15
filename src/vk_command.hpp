#pragma once
#include <vulkan/vulkan.h>
#include <vector>

#include "common_structs.hpp"

class CommandManager
{
    public:
        VkCommandPool commandPool;
        std::vector<VkCommandBuffer> commandBuffers;

        void init(VkDevice &device, QueueFamilyIndices &indices, size_t swapChainSize, std::vector<VkFramebuffer> &swapChainFramebuffers, VkExtent2D &swapChainExtent, VkPipeline &graphicsPipeline, VkRenderPass &renderPass);
        void createCommandPool(VkDevice &device, QueueFamilyIndices &indices);
        void createCommandBuffers(VkDevice &device, size_t swapchainSize, std::vector<VkFramebuffer> &swapChainFramebuffers, VkExtent2D &swapChainExtent, VkPipeline &graphicsPipeline, VkRenderPass &renderPass);
        void recordCommandBuffer(VkCommandBuffer &commandBuffer, VkFramebuffer &swapChainFramebuffer, VkExtent2D &swapChainExtent, VkPipeline &graphicsPipeline, VkRenderPass &renderPass);
};
#include <vulkan/vulkan.h>
#include <SDL2/SDL_vulkan.h>
#include <SDL2/SDL_video.h>
#include <stdexcept>
#include <SDL2/SDL.h>

#include <cstdlib>
#include <thread>
#include <vector>
#include <cstdint>
#include <atomic>

#include "common_structs.hpp"
#include "vk_debug.hpp"
#include "vk_device.hpp"
#include "vk_frames.hpp"
#include "vk_command.hpp"
#include "vk_sync.hpp"

const int MAX_FRAMES_IN_FLIGHT = 4;
const std::vector<const char*> validationLayers = {"VK_LAYER_KHRONOS_validation"};
#ifdef NDEBUG
    const bool enableValidationLayers = false;
#else
    const bool enableValidationLayers = true;
#endif

int WIDTH = 1920;
int HEIGHT = 1080;

const bool forceOpenGL = false;

class Triangle
{
    public:
        Triangle()
        {
            initWindow();

            #ifdef __EMSCRIPTEN__
                initWebGL();
                mainLoop();
                cleanAllWebGPU();
            #else
                if (!initVulkan())
                {
                    printf("Failed to create vulkan instance\n");
                    cleanInstance();
                    //init openGL
                }
                else
                {
                    mainLoop();
                    cleanAll();
                }
            #endif
        }

    private:
        SDL_Window* window;
        uint32_t currentFrame = 0;

        DebugManager debugManager;
        DeviceManager deviceManager;
        FrameManager frameManager;
        CommandManager commandManager;
        SyncManager syncManager;

        void initWindow()
        {
            SDL_Init(SDL_INIT_VIDEO);

            #ifdef __EMSCRIPTEN__
                window = SDL_CreateWindow("...", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, WIDTH, HEIGHT, SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE);
            #else
                window = SDL_CreateWindow("...", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, WIDTH, HEIGHT, SDL_WINDOW_VULKAN | SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE);
            #endif
        }

        bool initVulkan()
        {
            if (enableValidationLayers && !debugManager.checkValidationLayerSupport(validationLayers)) throw std::runtime_error("validation layers requested, but not available!");

            VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo{};
            if (enableValidationLayers) debugManager.populateDebugMessengerCreateInfo(debugCreateInfo);
            deviceManager.createInstance(window, enableValidationLayers, validationLayers, debugCreateInfo);
            if (enableValidationLayers) debugManager.setupDebugMessenger(deviceManager.instance);

            if (!deviceManager.checkPhysicalDevice() || forceOpenGL)
            {
                return false;
            }

            deviceManager.createLogicalDevice(deviceManager.indices, enableValidationLayers, validationLayers);

            SwapChainSupportDetails swapChainSupport = deviceManager.querySwapChainSupport(deviceManager.physicalDevice);
            frameManager.init(deviceManager.device, window, deviceManager.surface, deviceManager.indices, swapChainSupport);

            //createVertexBuffer();

            commandManager.init(deviceManager.device, deviceManager.indices, frameManager.swapChainImages.size(), frameManager.swapChainFramebuffers, frameManager.swapChainExtent, frameManager.graphicsPipeline, frameManager.renderPass);
            syncManager.createSyncObjects(deviceManager.device, MAX_FRAMES_IN_FLIGHT);

            return true;
        }
        void recreateSwapChain()
        {
            vkDeviceWaitIdle(deviceManager.device);

            SwapChainSupportDetails swapChainSupport = deviceManager.querySwapChainSupport(deviceManager.physicalDevice);
            frameManager.reinit(deviceManager.device, window, deviceManager.surface, deviceManager.indices, swapChainSupport);

            vkFreeCommandBuffers(deviceManager.device, commandManager.commandPool, static_cast<uint32_t>(commandManager.commandBuffers.size()), commandManager.commandBuffers.data());
            commandManager.createCommandBuffers(deviceManager.device, frameManager.swapChainImages.size(), frameManager.swapChainFramebuffers, frameManager.swapChainExtent, frameManager.graphicsPipeline, frameManager.renderPass);
        }


        const std::vector<Vertex> vertices =
        {
            {{0.0f, -0.5f}, {1.0f, 0.0f, 0.0f}},
            {{0.5f,  0.5f}, {0.0f, 1.0f, 0.0f}},
            {{-0.5f, 0.5f}, {0.0f, 0.0f, 1.0f}}
        };
        VkBuffer vertexBuffer;

        void createVertexBuffer()
        {
            VkBufferCreateInfo bufferInfo{};
            bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
            bufferInfo.size = sizeof(vertices[0]) * vertices.size();
            bufferInfo.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
            bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

            if (vkCreateBuffer(deviceManager.device, &bufferInfo, nullptr, &vertexBuffer) != VK_SUCCESS) throw std::runtime_error("failed to create vertex buffer!");
        }


        std::atomic<bool> running = true;
        std::atomic<bool> resized = false;
        std::atomic<int> frameCount = 0;
        std::thread renderThread;

        void mainLoop()
        {
            createRenderthread();
            Uint32 lastTime = SDL_GetTicks();
            char titleBuffer[64];

            const int targetFPS = 20;
            const int frameDelay = 1000 / targetFPS;

            while (running)
            {
                SDL_Event event;
                while (SDL_PollEvent(&event))
                {
                    switch (event.type)
                    {
                        case SDL_QUIT:
                            running = false;
                            break;
                        case SDL_WINDOWEVENT:
                            if (event.window.event == SDL_WINDOWEVENT_SIZE_CHANGED)
                            {
                                resized = true;
                            }
                            break;
                    }
                }

                Uint32 currentTime = SDL_GetTicks();
                Uint32 frametime = currentTime - lastTime;

                if (frametime >= 1000)
                {
                    int frames = frameCount.exchange(0);
                    float fps = 1000.0f * (float)frames / (float)frametime;

                    std::snprintf(titleBuffer, 64, "FPS: %f", fps);
                    SDL_SetWindowTitle(window, titleBuffer);
                    lastTime = currentTime;
                }
                SDL_Delay(frameDelay);
            }

            renderThread.join();
        }
        void createRenderthread()
        {
            renderThread = std::thread([this]()
            {
                while (running)
                {
                    VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
                    VkSwapchainKHR swapChains[] = {frameManager.swapChain};

                    VkFence &fence = syncManager.inFlightFences[currentFrame];
                    vkWaitForFences(deviceManager.device, 1, &fence, VK_TRUE, UINT64_MAX);

                    uint32_t imageIndex;
                    VkResult result = vkAcquireNextImageKHR(deviceManager.device, frameManager.swapChain, UINT64_MAX, syncManager.imageAvailableSemaphores[currentFrame], VK_NULL_HANDLE, &imageIndex);
                    if (result == VK_ERROR_OUT_OF_DATE_KHR)
                    {
                        recreateSwapChain();
                        return;
                    }
                    else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR)
                    {
                        throw std::runtime_error("failed to acquire swap chain image!");
                    }

                    vkResetFences(deviceManager.device, 1, &fence);

                    VkSemaphore waitSemaphores[] = {syncManager.imageAvailableSemaphores[currentFrame]};
                    VkSemaphore signalSemaphores[] = {syncManager.renderFinishedSemaphores[currentFrame]};

                    const VkSubmitInfo submitInfo =
                    {
                        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
                        .pNext = nullptr,
                        .waitSemaphoreCount = 1,
                        .pWaitSemaphores = waitSemaphores,
                        .pWaitDstStageMask = waitStages,
                        .commandBufferCount = 1,
                        .pCommandBuffers = &commandManager.commandBuffers[imageIndex],
                        .signalSemaphoreCount = 1,
                        .pSignalSemaphores = signalSemaphores,
                    };
                    vkQueueSubmit(deviceManager.graphicsQueue, 1, &submitInfo, fence);
                    const VkPresentInfoKHR presentInfo =
                    {
                        .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
                        .waitSemaphoreCount = 1,
                        .pWaitSemaphores = signalSemaphores,
                        .swapchainCount = 1,
                        .pSwapchains = swapChains,
                        .pImageIndices = &imageIndex,
                        .pResults = nullptr
                    };

                    result = vkQueuePresentKHR(deviceManager.presentQueue, &presentInfo);
                    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || resized.exchange(false))
                    {
                        recreateSwapChain();
                    }
                    else if (result != VK_SUCCESS)
                    {
                        throw std::runtime_error("failed to present swap chain image!");
                    }

                    currentFrame = (currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
                    frameCount++;
                }
            });
        }

        void cleanAll()
        {
            vkDeviceWaitIdle(deviceManager.device);
            frameManager.cleanupSwapChain(deviceManager.device);
            frameManager.cleanupPipeline(deviceManager.device);

            for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
            {
                vkDestroySemaphore(deviceManager.device, syncManager.renderFinishedSemaphores[i], nullptr);
                vkDestroySemaphore(deviceManager.device, syncManager.imageAvailableSemaphores[i], nullptr);
                vkDestroyFence(deviceManager.device, syncManager.inFlightFences[i], nullptr);
            }

            vkDestroyCommandPool(deviceManager.device, commandManager.commandPool, nullptr);
            vkDestroyDevice(deviceManager.device, nullptr);

            cleanInstance();
        }
        void cleanInstance()
        {
            vkDestroySurfaceKHR(deviceManager.instance, deviceManager.surface, nullptr);

            if (enableValidationLayers) debugManager.DestroyDebugUtilsMessengerEXT(deviceManager.instance, nullptr);

            vkDestroyInstance(deviceManager.instance, nullptr);

            SDL_DestroyWindow(window);
            SDL_Quit();
        }
};

int main(int argc, char* argv[])
{
    Triangle app;
    return EXIT_SUCCESS;
}
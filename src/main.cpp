bool USE_IGPU = false;

#ifdef _WIN32
    #define WIN32_LEAN_AND_MEAN
    #include <windows.h>
#elif __linux__
    #include <sched.h>
#endif

#include <thread>

#include <volk.h>
#include <SDL3/SDL_vulkan.h>
#include <SDL3/SDL_video.h>
#include <SDL3/SDL.h>

#include <cstdlib>
#include <vector>
#include <cstdint>
#include <atomic>

#include "common_structs.hpp"
#include "vk_device.hpp"
#include "vk_frames.hpp"
#include "vk_command.hpp"
#include "vk_sync.hpp"

class Triangle
{
    public:
        Triangle()
        {
            initWindow();
            initVulkan();
            mainLoop();
            cleanAll();
        }

    private:
        SDL_Window* window;

        DeviceManager deviceManager;
        FrameManager frameManager;
        CommandManager commandManager;
        SyncManager syncManager;

        void initWindow()
        {
            SDL_Init(SDL_INIT_VIDEO);
            window = SDL_CreateWindow("...", WIDTH, HEIGHT, SDL_WINDOW_VULKAN);
        }

        bool initVulkan()
        {
            volkInitialize();

            deviceManager.createInstance(window);

            volkLoadInstance(deviceManager.instance);

            deviceManager.checkPhysicalDevice(USE_IGPU);
            deviceManager.createLogicalDevice(deviceManager.indices);

            SwapChainSupportDetails swapChainSupport = deviceManager.querySwapChainSupport(deviceManager.physicalDevice);
            frameManager.init(deviceManager.device, window, deviceManager.surface, deviceManager.indices, swapChainSupport);
            commandManager.init(deviceManager.device, deviceManager.indices, frameManager.swapChainImages.size(), frameManager.swapChainFramebuffers, frameManager.swapChainExtent, frameManager.graphicsPipeline, frameManager.renderPass);
            syncManager.createSyncObjects(deviceManager.device, MAX_FRAMES_IN_FLIGHT);

            return true;
        }

        std::atomic<bool> running = true;
        uint64_t frameCount = 0;
        uint64_t prevframeCount = 0;
        std::thread renderThread;

        void mainLoop()
        {
            createRenderthread();

            #ifdef _WIN32
                HANDLE hThread = GetCurrentThread();
                SetThreadAffinityMask(hThread, 1 << 1);
                SetThreadPriority(hThread, THREAD_PRIORITY_HIGHEST);
            #elif __linux__
                cpu_set_t cpuset;
                CPU_ZERO(&cpuset);
                const int core_id = 2;
                CPU_SET(core_id, &cpuset);

                const pthread_t thread = pthread_self();
                pthread_setaffinity_np(thread, sizeof(cpu_set_t), &cpuset);

                sched_param sch_params;
                sch_params.sched_priority = sched_get_priority_max(SCHED_RR);
                pthread_setschedparam(thread, SCHED_RR, &sch_params);
            #endif

            char titleBuffer[64];

            while (running)
            {
                SDL_PumpEvents();
                if (SDL_HasEvent(SDL_EVENT_QUIT)) running = false;

                uint_fast64_t fps = frameCount - prevframeCount;

                std::snprintf(titleBuffer, 64, "FPS: %lu", fps);
                SDL_SetWindowTitle(window, titleBuffer);
                prevframeCount = frameCount;

                SDL_Delay(1000u);
            }

            renderThread.join();
        }
        void createRenderthread()
        {
            renderThread = std::thread([this]()
            {
                #ifdef _WIN32
                    HANDLE hThread = GetCurrentThread();
                    SetThreadAffinityMask(hThread, 1 << 1);
                    SetThreadPriority(hThread, THREAD_PRIORITY_HIGHEST);
                #elif __linux__
                    cpu_set_t cpuset;
                    CPU_ZERO(&cpuset);
                    const int core_id = 1;
                    CPU_SET(core_id, &cpuset);

                    const pthread_t thread = pthread_self();
                    pthread_setaffinity_np(thread, sizeof(cpu_set_t), &cpuset);

                    sched_param sch_params;
                    sch_params.sched_priority = sched_get_priority_max(SCHED_RR);
                    pthread_setschedparam(thread, SCHED_RR, &sch_params);
                #endif

                const VkSwapchainKHR swapChains[] = {frameManager.swapChain};
                VkSemaphore signalSemaphores[1];

                VkCommandBufferSubmitInfo cmdBufInfo{};
                cmdBufInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO;
                VkSemaphoreSubmitInfo waitInfo{};
                waitInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
                waitInfo.stageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
                VkSemaphoreSubmitInfo signalInfo{};
                signalInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
                signalInfo.stageMask = VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT;

                uint32_t imageIndex;
                uint32_t currentFrame = 0;

                VkPresentInfoKHR presentInfo =
                {
                    .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
                    .waitSemaphoreCount = 1,
                    .swapchainCount = 1,
                    .pSwapchains = swapChains,
                    .pResults = nullptr
                };

                const VkSubmitInfo2 submitInfo
                {
                    .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2,
                    .waitSemaphoreInfoCount = 1,
                    .pWaitSemaphoreInfos = &waitInfo,
                    .commandBufferInfoCount = 1,
                    .pCommandBufferInfos = &cmdBufInfo,
                    .signalSemaphoreInfoCount = 1,
                    .pSignalSemaphoreInfos = &signalInfo,
                };

                while (running)
                {
                    //VkFence &fence = syncManager.inFlightFences[currentFrame];
                    //vkWaitForFences(deviceManager.device, 1, &fence, VK_TRUE, UINT64_MAX);

                    vkAcquireNextImageKHR(deviceManager.device, frameManager.swapChain, UINT64_MAX, syncManager.imageAvailableSemaphores[currentFrame], VK_NULL_HANDLE, &imageIndex);

                    //vkResetFences(deviceManager.device, 1, &fence);

                    cmdBufInfo.commandBuffer = commandManager.commandBuffers[imageIndex];
                    waitInfo.semaphore = syncManager.imageAvailableSemaphores[currentFrame];
                    signalInfo.semaphore = syncManager.renderFinishedSemaphores[currentFrame];
                    vkQueueSubmit2(deviceManager.graphicsQueue, 1, &submitInfo, syncManager.inFlightFences[currentFrame]);

                    signalSemaphores[0] = {syncManager.renderFinishedSemaphores[currentFrame]};
                    presentInfo.pWaitSemaphores = signalSemaphores;
                    presentInfo.pImageIndices = &imageIndex;

                    vkQueuePresentKHR(deviceManager.presentQueue, &presentInfo);

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

            vkDestroySurfaceKHR(deviceManager.instance, deviceManager.surface, nullptr);
            vkDestroyInstance(deviceManager.instance, nullptr);

            SDL_DestroyWindow(window);
            SDL_Quit();
        }
};

int main(int argc, char* argv[])
{
    for (int i = 1; i < argc; i++)
    {
        std::string arg = argv[i];
        if (arg == "--use-igpu")
        {
            USE_IGPU = true;
        }
        else
        {
            USE_IGPU = false;
        }
    }

    Triangle app;
    return EXIT_SUCCESS;
}

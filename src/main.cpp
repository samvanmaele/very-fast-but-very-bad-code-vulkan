#include <cstddef>
#include <iostream>
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
                const int core_id = 1;
                CPU_SET(core_id, &cpuset);

                const pthread_t thread = pthread_self();
                pthread_setaffinity_np(thread, sizeof(cpu_set_t), &cpuset);

                sched_param sch_params;
                sch_params.sched_priority = sched_get_priority_max(SCHED_RR);
                pthread_setschedparam(thread, SCHED_RR, &sch_params);
            #endif

            while (running)
            {
                SDL_PumpEvents();
                if (SDL_HasEvent(SDL_EVENT_QUIT)) running = false;

                uint_fast64_t fps = frameCount - prevframeCount;
                std::cout << fps << "\n";
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
                    const int core_id = 19;
                    CPU_SET(core_id, &cpuset);

                    const pthread_t thread = pthread_self();
                    pthread_setaffinity_np(thread, sizeof(cpu_set_t), &cpuset);

                    sched_param sch_params;
                    sch_params.sched_priority = sched_get_priority_max(SCHED_RR);
                    pthread_setschedparam(thread, SCHED_RR, &sch_params);
                #endif

                const VkSwapchainKHR swapChains[] = {frameManager.swapChain};
                const int swapchainSize = 65;

                std::vector<VkCommandBufferSubmitInfo> cmdBufInfo(swapchainSize);
                std::vector<VkSubmitInfo2> submitInfo(swapchainSize);
                std::vector<VkPresentInfoKHR> presentInfo(swapchainSize);
                std::vector<uint32_t> imageIndices(swapchainSize);

                for (size_t i = 0; i < swapchainSize; i++)
                {
                    cmdBufInfo[i].sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO;
                    cmdBufInfo[i].commandBuffer = commandManager.commandBuffers[i];

                    submitInfo[i].sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2;
                    submitInfo[i].waitSemaphoreInfoCount = 0;
                    submitInfo[i].pWaitSemaphoreInfos = nullptr;
                    submitInfo[i].commandBufferInfoCount = 1;
                    submitInfo[i].pCommandBufferInfos = &cmdBufInfo[i];
                    submitInfo[i].signalSemaphoreInfoCount = 0;
                    submitInfo[i].pSignalSemaphoreInfos = nullptr;

                    presentInfo[i].sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
                    presentInfo[i].waitSemaphoreCount = 0;
                    presentInfo[i].pWaitSemaphores = nullptr;
                    presentInfo[i].swapchainCount = 1;
                    presentInfo[i].pSwapchains = swapChains;
                    imageIndices[i] = i;
                    presentInfo[i].pImageIndices = &imageIndices[i];
                    presentInfo[i].pResults = nullptr;
                }

                uint32_t imageIndex;
                uint32_t currentFrame = 0;

                while (running)
                {
                    vkQueueSubmit2(deviceManager.graphicsQueue, 1, &submitInfo[currentFrame], VK_NULL_HANDLE);
                    vkQueuePresentKHR(deviceManager.presentQueue, &presentInfo[currentFrame]);

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

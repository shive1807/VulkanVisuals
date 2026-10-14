
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <optional>
#include <set>
#include <stdexcept>
#include <vector>

class VulkanApp
{
public:
    void Run()
    {
        try
        {
            InitWindow();
            InitVulkan();
            MainLoop();
        }
        catch (const std::exception& exception)
        {
            std::cerr << "Error: " << exception.what() << '\n';
            Cleanup();
            throw;
        }

        Cleanup();
    }

private:
    GLFWwindow* window = nullptr;

    VkInstance instance = VK_NULL_HANDLE;
    VkSurfaceKHR surface = VK_NULL_HANDLE;
    VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
    VkDevice device = VK_NULL_HANDLE;

    VkQueue graphicsQueue = VK_NULL_HANDLE;
    VkQueue presentQueue = VK_NULL_HANDLE;

    VkSwapchainKHR swapchain = VK_NULL_HANDLE;
    VkFormat swapchainFormat = VK_FORMAT_UNDEFINED;
    VkExtent2D swapchainExtent{};

    std::vector<VkImage> swapchainImages;
    std::vector<VkImageView> imageViews;
    std::vector<VkFramebuffer> framebuffers;

    VkRenderPass renderPass = VK_NULL_HANDLE;
    VkCommandPool commandPool = VK_NULL_HANDLE;
    std::vector<VkCommandBuffer> commandBuffers;

    VkSemaphore imageAvailable = VK_NULL_HANDLE;
    VkSemaphore renderFinished = VK_NULL_HANDLE;
    VkFence inFlightFence = VK_NULL_HANDLE;

    struct QueueFamilies
    {
        std::optional<uint32_t> graphics;
        std::optional<uint32_t> present;

        bool Complete() const
        {
            return graphics.has_value() && present.has_value();
        }
    };

    void InitWindow()
    {
        if (glfwInit() != GLFW_TRUE)
        {
            throw std::runtime_error("Failed to initialize GLFW.");
        }

        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

        window = glfwCreateWindow(
            1280,
            720,
            "Vulkan Visuals | First Render",
            nullptr,
            nullptr
        );

        if (window == nullptr)
        {
            throw std::runtime_error("Failed to create GLFW window.");
        }
    }

    void InitVulkan()
    {
        CreateInstance();
        CreateSurface();
        PickPhysicalDevice();
        CreateLogicalDevice();
        CreateSwapchain();
        CreateImageViews();
        CreateRenderPass();
        CreateFramebuffers();
        CreateCommandPool();
        CreateCommandBuffer();
        CreateSyncObjects();

        std::cout << "Vulkan renderer initialized.\n";
    }

    void CreateInstance()
    {
        uint32_t glfwExtensionCount = 0;
        const char** glfwExtensions =
            glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

        if (glfwExtensions == nullptr || glfwExtensionCount == 0)
        {
            throw std::runtime_error(
                "Failed to get GLFW Vulkan extensions."
            );
        }

        std::vector<const char*> extensions(
            glfwExtensions,
            glfwExtensions + glfwExtensionCount
        );

        extensions.push_back(
            VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME
        );

        VkApplicationInfo appInfo{};
        appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
        appInfo.pApplicationName = "Vulkan Visuals";
        appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
        appInfo.pEngineName = "VulkanVisuals";
        appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
        appInfo.apiVersion = VK_API_VERSION_1_1;

        VkInstanceCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
        createInfo.flags =
            VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR;
        createInfo.pApplicationInfo = &appInfo;
        createInfo.enabledExtensionCount =
            static_cast<uint32_t>(extensions.size());
        createInfo.ppEnabledExtensionNames = extensions.data();

        VkResult result = vkCreateInstance(
            &createInfo,
            nullptr,
            &instance
        );

        if (result != VK_SUCCESS)
        {
            throw std::runtime_error("Failed to create Vulkan instance.");
        }
    }

    void CreateSurface()
    {
        if (glfwCreateWindowSurface(
                instance, window, nullptr, &surface) != VK_SUCCESS)
        {
            throw std::runtime_error("Failed to create window surface.");
        }
    }

    QueueFamilies FindQueueFamilies(VkPhysicalDevice gpu)
    {
        QueueFamilies families;

        uint32_t count = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(gpu, &count, nullptr);

        std::vector<VkQueueFamilyProperties> queues(count);
        vkGetPhysicalDeviceQueueFamilyProperties(
            gpu, &count, queues.data()
        );

        for (uint32_t i = 0; i < count; ++i)
        {
            if ((queues[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) != 0)
            {
                families.graphics = i;
            }

            VkBool32 canPresent = VK_FALSE;
            vkGetPhysicalDeviceSurfaceSupportKHR(
                gpu, i, surface, &canPresent
            );

            if (canPresent == VK_TRUE)
            {
                families.present = i;
            }

            if (families.Complete())
            {
                break;
            }
        }

        return families;
    }

    bool HasSwapchainExtension(VkPhysicalDevice gpu)
    {
        uint32_t count = 0;

        vkEnumerateDeviceExtensionProperties(
            gpu, nullptr, &count, nullptr
        );

        std::vector<VkExtensionProperties> extensions(count);

        vkEnumerateDeviceExtensionProperties(
            gpu, nullptr, &count, extensions.data()
        );

        bool hasSwapchain = false;
        bool hasPortabilitySubset = false;

        for (const auto& extension : extensions)
        {
            if (std::string(extension.extensionName) ==
                VK_KHR_SWAPCHAIN_EXTENSION_NAME)
            {
                hasSwapchain = true;
            }

            if (std::string(extension.extensionName) ==
                "VK_KHR_portability_subset")
            {
                hasPortabilitySubset = true;
            }
        }

        // MoltenVK may expose portability_subset. If advertised,
        // it must be enabled when creating the logical device.
        portabilitySubsetAvailable = hasPortabilitySubset;

        return hasSwapchain;
    }

    bool portabilitySubsetAvailable = false;

    void PickPhysicalDevice()
    {
        uint32_t count = 0;
        vkEnumeratePhysicalDevices(instance, &count, nullptr);

        if (count == 0)
        {
            throw std::runtime_error("No Vulkan GPUs found.");
        }

        std::vector<VkPhysicalDevice> devices(count);
        vkEnumeratePhysicalDevices(
            instance, &count, devices.data()
        );

        for (VkPhysicalDevice gpu : devices)
        {
            QueueFamilies families = FindQueueFamilies(gpu);

            if (!families.Complete() || !HasSwapchainExtension(gpu))
            {
                continue;
            }

            uint32_t formatCount = 0;
            uint32_t modeCount = 0;

            vkGetPhysicalDeviceSurfaceFormatsKHR(
                gpu, surface, &formatCount, nullptr
            );

            vkGetPhysicalDeviceSurfacePresentModesKHR(
                gpu, surface, &modeCount, nullptr
            );

            if (formatCount == 0 || modeCount == 0)
            {
                continue;
            }

            physicalDevice = gpu;
            queueFamilies = families;
            break;
        }

        if (physicalDevice == VK_NULL_HANDLE)
        {
            throw std::runtime_error(
                "No GPU supports graphics and window presentation."
            );
        }

        VkPhysicalDeviceProperties properties{};
        vkGetPhysicalDeviceProperties(physicalDevice, &properties);

        std::cout << "Selected GPU: "
                  << properties.deviceName << '\n';
    }

    QueueFamilies queueFamilies;

    void CreateLogicalDevice()
    {
        std::set<uint32_t> uniqueFamilies = {
            queueFamilies.graphics.value(),
            queueFamilies.present.value()
        };

        float priority = 1.0f;
        std::vector<VkDeviceQueueCreateInfo> queueInfos;

        for (uint32_t family : uniqueFamilies)
        {
            VkDeviceQueueCreateInfo queueInfo{};
            queueInfo.sType =
                VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
            queueInfo.queueFamilyIndex = family;
            queueInfo.queueCount = 1;
            queueInfo.pQueuePriorities = &priority;

            queueInfos.push_back(queueInfo);
        }

        std::vector<const char*> extensions = {
            VK_KHR_SWAPCHAIN_EXTENSION_NAME
        };

        if (portabilitySubsetAvailable)
        {
            extensions.push_back("VK_KHR_portability_subset");
        }

        VkPhysicalDeviceFeatures features{};

        VkDeviceCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
        createInfo.queueCreateInfoCount =
            static_cast<uint32_t>(queueInfos.size());
        createInfo.pQueueCreateInfos = queueInfos.data();
        createInfo.pEnabledFeatures = &features;
        createInfo.enabledExtensionCount =
            static_cast<uint32_t>(extensions.size());
        createInfo.ppEnabledExtensionNames = extensions.data();

        if (vkCreateDevice(
                physicalDevice, &createInfo, nullptr, &device) != VK_SUCCESS)
        {
            throw std::runtime_error("Failed to create logical device.");
        }

        vkGetDeviceQueue(
            device, queueFamilies.graphics.value(), 0, &graphicsQueue
        );

        vkGetDeviceQueue(
            device, queueFamilies.present.value(), 0, &presentQueue
        );
    }

    void CreateSwapchain()
    {
        VkSurfaceCapabilitiesKHR capabilities{};
        vkGetPhysicalDeviceSurfaceCapabilitiesKHR(
            physicalDevice, surface, &capabilities
        );

        uint32_t formatCount = 0;
        vkGetPhysicalDeviceSurfaceFormatsKHR(
            physicalDevice, surface, &formatCount, nullptr
        );

        std::vector<VkSurfaceFormatKHR> formats(formatCount);
        vkGetPhysicalDeviceSurfaceFormatsKHR(
            physicalDevice, surface, &formatCount, formats.data()
        );

        VkSurfaceFormatKHR chosenFormat = formats[0];

        for (const auto& format : formats)
        {
            if (format.format == VK_FORMAT_B8G8R8A8_SRGB &&
                format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
            {
                chosenFormat = format;
                break;
            }
        }

        swapchainFormat = chosenFormat.format;

        if (capabilities.currentExtent.width !=
            std::numeric_limits<uint32_t>::max())
        {
            swapchainExtent = capabilities.currentExtent;
        }
        else
        {
            int width = 0;
            int height = 0;

            glfwGetFramebufferSize(window, &width, &height);

            swapchainExtent.width = std::clamp(
                static_cast<uint32_t>(width),
                capabilities.minImageExtent.width,
                capabilities.maxImageExtent.width
            );

            swapchainExtent.height = std::clamp(
                static_cast<uint32_t>(height),
                capabilities.minImageExtent.height,
                capabilities.maxImageExtent.height
            );
        }

        uint32_t imageCount = capabilities.minImageCount + 1;

        if (capabilities.maxImageCount > 0 &&
            imageCount > capabilities.maxImageCount)
        {
            imageCount = capabilities.maxImageCount;
        }

        uint32_t familyIndices[] = {
            queueFamilies.graphics.value(),
            queueFamilies.present.value()
        };

        VkSwapchainCreateInfoKHR createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
        createInfo.surface = surface;
        createInfo.minImageCount = imageCount;
        createInfo.imageFormat = chosenFormat.format;
        createInfo.imageColorSpace = chosenFormat.colorSpace;
        createInfo.imageExtent = swapchainExtent;
        createInfo.imageArrayLayers = 1;
        createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

        if (familyIndices[0] != familyIndices[1])
        {
            createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
            createInfo.queueFamilyIndexCount = 2;
            createInfo.pQueueFamilyIndices = familyIndices;
        }
        else
        {
            createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
        }

        createInfo.preTransform = capabilities.currentTransform;
        createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
        createInfo.presentMode = VK_PRESENT_MODE_FIFO_KHR;
        createInfo.clipped = VK_TRUE;

        if (vkCreateSwapchainKHR(
                device, &createInfo, nullptr, &swapchain) != VK_SUCCESS)
        {
            throw std::runtime_error("Failed to create swapchain.");
        }

        vkGetSwapchainImagesKHR(
            device, swapchain, &imageCount, nullptr
        );

        swapchainImages.resize(imageCount);

        vkGetSwapchainImagesKHR(
            device, swapchain, &imageCount, swapchainImages.data()
        );
    }

    void CreateImageViews()
    {
        imageViews.resize(swapchainImages.size());

        for (size_t i = 0; i < swapchainImages.size(); ++i)
        {
            VkImageViewCreateInfo info{};
            info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
            info.image = swapchainImages[i];
            info.viewType = VK_IMAGE_VIEW_TYPE_2D;
            info.format = swapchainFormat;
            info.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
            info.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
            info.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
            info.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
            info.subresourceRange.aspectMask =
                VK_IMAGE_ASPECT_COLOR_BIT;
            info.subresourceRange.baseMipLevel = 0;
            info.subresourceRange.levelCount = 1;
            info.subresourceRange.baseArrayLayer = 0;
            info.subresourceRange.layerCount = 1;

            if (vkCreateImageView(
                    device, &info, nullptr, &imageViews[i]) != VK_SUCCESS)
            {
                throw std::runtime_error("Failed to create image view.");
            }
        }
    }

    void CreateRenderPass()
    {
        VkAttachmentDescription colorAttachment{};
        colorAttachment.format = swapchainFormat;
        colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
        colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        colorAttachment.finalLayout =
            VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

        VkAttachmentReference colorReference{};
        colorReference.attachment = 0;
        colorReference.layout =
            VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

        VkSubpassDescription subpass{};
        subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpass.colorAttachmentCount = 1;
        subpass.pColorAttachments = &colorReference;

        VkSubpassDependency dependency{};
        dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
        dependency.dstSubpass = 0;
        dependency.srcStageMask =
            VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        dependency.dstStageMask =
            VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        dependency.dstAccessMask =
            VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

        VkRenderPassCreateInfo info{};
        info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        info.attachmentCount = 1;
        info.pAttachments = &colorAttachment;
        info.subpassCount = 1;
        info.pSubpasses = &subpass;
        info.dependencyCount = 1;
        info.pDependencies = &dependency;

        if (vkCreateRenderPass(
                device, &info, nullptr, &renderPass) != VK_SUCCESS)
        {
            throw std::runtime_error("Failed to create render pass.");
        }
    }

    void CreateFramebuffers()
    {
        framebuffers.resize(imageViews.size());

        for (size_t i = 0; i < imageViews.size(); ++i)
        {
            VkFramebufferCreateInfo info{};
            info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
            info.renderPass = renderPass;
            info.attachmentCount = 1;
            info.pAttachments = &imageViews[i];
            info.width = swapchainExtent.width;
            info.height = swapchainExtent.height;
            info.layers = 1;

            if (vkCreateFramebuffer(
                    device, &info, nullptr, &framebuffers[i]) != VK_SUCCESS)
            {
                throw std::runtime_error("Failed to create framebuffer.");
            }
        }
    }

    void CreateCommandPool()
    {
        VkCommandPoolCreateInfo info{};
        info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        info.queueFamilyIndex = queueFamilies.graphics.value();

        if (vkCreateCommandPool(
                device, &info, nullptr, &commandPool) != VK_SUCCESS)
        {
            throw std::runtime_error("Failed to create command pool.");
        }
    }

    void CreateCommandBuffer()
    {
        commandBuffers.resize(framebuffers.size());

        VkCommandBufferAllocateInfo info{};
        info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        info.commandPool = commandPool;
        info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        info.commandBufferCount =
            static_cast<uint32_t>(commandBuffers.size());

        if (vkAllocateCommandBuffers(
                device, &info, commandBuffers.data()) != VK_SUCCESS)
        {
            throw std::runtime_error("Failed to allocate command buffers.");
        }

        for (size_t i = 0; i < commandBuffers.size(); ++i)
        {
            VkCommandBufferBeginInfo begin{};
            begin.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

            vkBeginCommandBuffer(commandBuffers[i], &begin);

            // Deep navy background. We'll render particles over this.
            VkClearValue clearColor{};
            clearColor.color = {{0.008f, 0.012f, 0.035f, 1.0f}};

            VkRenderPassBeginInfo renderBegin{};
            renderBegin.sType =
                VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
            renderBegin.renderPass = renderPass;
            renderBegin.framebuffer = framebuffers[i];
            renderBegin.renderArea.offset = {0, 0};
            renderBegin.renderArea.extent = swapchainExtent;
            renderBegin.clearValueCount = 1;
            renderBegin.pClearValues = &clearColor;

            vkCmdBeginRenderPass(
                commandBuffers[i],
                &renderBegin,
                VK_SUBPASS_CONTENTS_INLINE
            );

            vkCmdEndRenderPass(commandBuffers[i]);

            if (vkEndCommandBuffer(commandBuffers[i]) != VK_SUCCESS)
            {
                throw std::runtime_error(
                    "Failed to record command buffer."
                );
            }
        }
    }

    void CreateSyncObjects()
    {
        VkSemaphoreCreateInfo semaphoreInfo{};
        semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

        VkFenceCreateInfo fenceInfo{};
        fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

        if (vkCreateSemaphore(
                device, &semaphoreInfo, nullptr,
                &imageAvailable) != VK_SUCCESS ||
            vkCreateSemaphore(
                device, &semaphoreInfo, nullptr,
                &renderFinished) != VK_SUCCESS ||
            vkCreateFence(
                device, &fenceInfo, nullptr,
                &inFlightFence) != VK_SUCCESS)
        {
            throw std::runtime_error(
                "Failed to create frame synchronization objects."
            );
        }
    }

    void DrawFrame()
    {
        vkWaitForFences(
            device, 1, &inFlightFence, VK_TRUE, UINT64_MAX
        );

        uint32_t imageIndex = 0;

        VkResult result = vkAcquireNextImageKHR(
            device,
            swapchain,
            UINT64_MAX,
            imageAvailable,
            VK_NULL_HANDLE,
            &imageIndex
        );

        if (result == VK_ERROR_OUT_OF_DATE_KHR)
        {
            return;
        }

        if (result != VK_SUCCESS &&
            result != VK_SUBOPTIMAL_KHR)
        {
            throw std::runtime_error(
                "Failed to acquire swapchain image."
            );
        }

        vkResetFences(device, 1, &inFlightFence);

        VkPipelineStageFlags waitStage =
            VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

        VkSubmitInfo submit{};
        submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submit.waitSemaphoreCount = 1;
        submit.pWaitSemaphores = &imageAvailable;
        submit.pWaitDstStageMask = &waitStage;
        submit.commandBufferCount = 1;
        submit.pCommandBuffers = &commandBuffers[imageIndex];
        submit.signalSemaphoreCount = 1;
        submit.pSignalSemaphores = &renderFinished;

        if (vkQueueSubmit(
                graphicsQueue, 1, &submit, inFlightFence) != VK_SUCCESS)
        {
            throw std::runtime_error("Failed to submit render commands.");
        }

        VkPresentInfoKHR present{};
        present.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
        present.waitSemaphoreCount = 1;
        present.pWaitSemaphores = &renderFinished;
        present.swapchainCount = 1;
        present.pSwapchains = &swapchain;
        present.pImageIndices = &imageIndex;

        result = vkQueuePresentKHR(presentQueue, &present);

        if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR &&
            result != VK_ERROR_OUT_OF_DATE_KHR)
        {
            throw std::runtime_error("Failed to present frame.");
        }
    }

    void MainLoop()
    {
        while (glfwWindowShouldClose(window) == GLFW_FALSE)
        {
            glfwPollEvents();
            DrawFrame();
        }

        vkDeviceWaitIdle(device);
    }

    void Cleanup()
    {
        if (device != VK_NULL_HANDLE)
        {
            vkDeviceWaitIdle(device);

            if (inFlightFence != VK_NULL_HANDLE)
            {
                vkDestroyFence(device, inFlightFence, nullptr);
            }

            if (renderFinished != VK_NULL_HANDLE)
            {
                vkDestroySemaphore(device, renderFinished, nullptr);
            }

            if (imageAvailable != VK_NULL_HANDLE)
            {
                vkDestroySemaphore(device, imageAvailable, nullptr);
            }

            if (commandPool != VK_NULL_HANDLE)
            {
                vkDestroyCommandPool(device, commandPool, nullptr);
            }

            for (VkFramebuffer framebuffer : framebuffers)
            {
                vkDestroyFramebuffer(device, framebuffer, nullptr);
            }

            if (renderPass != VK_NULL_HANDLE)
            {
                vkDestroyRenderPass(device, renderPass, nullptr);
            }

            for (VkImageView view : imageViews)
            {
                vkDestroyImageView(device, view, nullptr);
            }

            if (swapchain != VK_NULL_HANDLE)
            {
                vkDestroySwapchainKHR(device, swapchain, nullptr);
            }

            vkDestroyDevice(device, nullptr);
        }

        if (surface != VK_NULL_HANDLE && instance != VK_NULL_HANDLE)
        {
            vkDestroySurfaceKHR(instance, surface, nullptr);
        }

        if (instance != VK_NULL_HANDLE)
        {
            vkDestroyInstance(instance, nullptr);
        }

        if (window != nullptr)
        {
            glfwDestroyWindow(window);
        }

        glfwTerminate();
    }
};

int main()
{
    VulkanApp app;

    try
    {
        app.Run();
    }
    catch (const std::exception&)
    {
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}

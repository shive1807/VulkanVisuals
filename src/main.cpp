
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <cstdlib>
#include <iostream>
#include <vector>

struct VulkanApp
{
    GLFWwindow* window = nullptr;
    VkInstance instance = VK_NULL_HANDLE;
    VkSurfaceKHR surface = VK_NULL_HANDLE;
    VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;

    bool Initialize()
    {
        if (glfwInit() != GLFW_TRUE)
        {
            std::cerr << "Failed to initialize GLFW.\n";
            return false;
        }

        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);

        window = glfwCreateWindow(
            1280,
            720,
            "Vulkan Visuals",
            nullptr,
            nullptr
        );

        if (window == nullptr)
        {
            std::cerr << "Failed to create GLFW window.\n";
            return false;
        }

        if (glfwVulkanSupported() != GLFW_TRUE)
        {
            std::cerr << "GLFW could not find a Vulkan loader.\n";
            return false;
        }

        if (!CreateInstance())
        {
            return false;
        }

        if (glfwCreateWindowSurface(
                instance, window, nullptr, &surface) != VK_SUCCESS)
        {
            std::cerr << "Failed to create Vulkan surface.\n";
            return false;
        }

        if (!SelectPhysicalDevice())
        {
            return false;
        }

        const VkPhysicalDeviceProperties properties = [&]()
        {
            VkPhysicalDeviceProperties result{};
            vkGetPhysicalDeviceProperties(physicalDevice, &result);
            return result;
        }();

        std::cout << "Vulkan initialized successfully.\n";
        std::cout << "GPU: " << properties.deviceName << '\n';

        return true;
    }

    bool CreateInstance()
    {
        uint32_t extensionCount = 0;
        const char** extensions =
            glfwGetRequiredInstanceExtensions(&extensionCount);

        if (extensions == nullptr || extensionCount == 0)
        {
            std::cerr << "Failed to get Vulkan surface extensions.\n";
            return false;
        }

        VkApplicationInfo applicationInfo{};
        applicationInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
        applicationInfo.pApplicationName = "Vulkan Visuals";
        applicationInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
        applicationInfo.pEngineName = "VulkanVisuals";
        applicationInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
        applicationInfo.apiVersion = VK_API_VERSION_1_1;

        VkInstanceCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
        createInfo.pApplicationInfo = &applicationInfo;
        createInfo.enabledExtensionCount = extensionCount;
        createInfo.ppEnabledExtensionNames = extensions;

        const VkResult result =
            vkCreateInstance(&createInfo, nullptr, &instance);

        if (result != VK_SUCCESS)
        {
            std::cerr << "Failed to create Vulkan instance: "
                      << result << '\n';
            return false;
        }

        return true;
    }

    bool SelectPhysicalDevice()
    {
        uint32_t deviceCount = 0;

        if (vkEnumeratePhysicalDevices(
                instance, &deviceCount, nullptr) != VK_SUCCESS ||
            deviceCount == 0)
        {
            std::cerr << "No Vulkan physical devices found.\n";
            return false;
        }

        std::vector<VkPhysicalDevice> devices(deviceCount);

        if (vkEnumeratePhysicalDevices(
                instance, &deviceCount, devices.data()) != VK_SUCCESS)
        {
            std::cerr << "Failed to enumerate physical devices.\n";
            return false;
        }

        for (VkPhysicalDevice device : devices)
        {
            VkPhysicalDeviceProperties properties{};
            vkGetPhysicalDeviceProperties(device, &properties);

            if (properties.deviceType ==
                VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU)
            {
                physicalDevice = device;
                break;
            }
        }

        if (physicalDevice == VK_NULL_HANDLE)
        {
            std::cerr << "No suitable GPU found.\n";
            return false;
        }

        return true;
    }

    void Run()
    {
        while (glfwWindowShouldClose(window) == GLFW_FALSE)
        {
            glfwPollEvents();
        }
    }

    void Cleanup()
    {
        if (surface != VK_NULL_HANDLE &&
            instance != VK_NULL_HANDLE)
        {
            vkDestroySurfaceKHR(instance, surface, nullptr);
            surface = VK_NULL_HANDLE;
        }

        if (instance != VK_NULL_HANDLE)
        {
            vkDestroyInstance(instance, nullptr);
            instance = VK_NULL_HANDLE;
        }

        if (window != nullptr)
        {
            glfwDestroyWindow(window);
            window = nullptr;
        }

        glfwTerminate();
    }
};

int main()
{
    VulkanApp app;

    if (!app.Initialize())
    {
        app.Cleanup();
        return EXIT_FAILURE;
    }

    app.Run();
    app.Cleanup();

    return EXIT_SUCCESS;
}


#include "VulkanBackend.h"
#include "Core/Vertex.h"
#include "Core/Assert.h"
#include "Core/Mesh.h"

#include <stb_image.h>
#include <cstdio>
#include <cstring>

namespace LX {
    #if defined(LX_DEBUG)
        static VKAPI_ATTR VkBool32 VKAPI_CALL DebugCallback(
            VkDebugUtilsMessageSeverityFlagBitsEXT          severity,
            VkDebugUtilsMessageTypeFlagsEXT                 type,
            const VkDebugUtilsMessengerCallbackDataEXT*     pCallbackData,
            void*                                           pUserData
        )
        {
            (void)type;
            (void)pUserData;
        
            if(severity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT){
                ::fprintf(stderr, "[Vulkan Validation] %s\n", pCallbackData->pMessage);
            }
            return VK_FALSE;
        }

    #endif

    static bool LoadShaderModule(VkDevice device, const char* path, VkShaderModule* outModule)
    {
        FILE* file = nullptr;
        
        #if defined(_MSC_VER)
            fopen_s(&file, path, "rb");
        #else
            file = ::fopen(path, "rb");
        #endif 

        if(!file)
        {
            ::fprintf(stderr, "[Lux] Failed to open shader: %s\n", path);
            return false;
        }

        ::fseek(file, 0, SEEK_END);
        usize fileSize = (usize)::ftell(file);
        ::fseek(file, 0, SEEK_SET);

        // SPIR-V must be read into a uint32_t aligned buffer
        // We allocate fileSize bytes but treat it as an array of u32
        u8* buffer = new u8[fileSize];
        ::fread(buffer, 1, fileSize, file);
        ::fclose(file);

        VkShaderModuleCreateInfo createInfo{};
        createInfo.sType    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        createInfo.codeSize = fileSize;
        createInfo.pCode    = reinterpret_cast<const u32*>(buffer);

        VkResult result = vkCreateShaderModule(device, &createInfo, nullptr, outModule);

        delete[] buffer;

        if (result != VK_SUCCESS)
        {
            ::fprintf(stderr, "[Lux] Failed to create shader module: %s\n", path);
            return false;
        }

        return true;
    }

    bool VulkanBackend::Init(const WindowHandle& window)
    {
        VkResult result = volkInitialize();
        LX_ASSERT(result == VK_SUCCESS, "Failed to initialize Volk");

        if(!InitInstance()) return false;
        if(!InitSurface(window)) return false;
        if(!SelectPhysicalDevice()) return false;
        if(!InitDevice()) return false;
        if(!InitAllocator()) return false;
        if(!InitSwapchain()) return false;
        if(!InitDepthBuffer()) return false;
        if(!InitRenderPass()) return false;
        if(!InitDescriptors()) return false;
        if(!InitFramebuffers()) return false;
        if(!InitCommands()) return false;
        if(!InitSyncObjects()) return false;
        if(!InitUniformBuffers()) return false;
        if(!InitPipeline()) return false;
        if(!InitSampler()) return false;

        ::printf("Vulkan initialized successfully\n");
        return true;
    }

    bool VulkanBackend::InitInstance()
    {
        VkApplicationInfo appInfo{};
        appInfo.sType                   = VK_STRUCTURE_TYPE_APPLICATION_INFO;
        appInfo.pApplicationName        = "Lux Sandbox";
        appInfo.applicationVersion      = VK_MAKE_VERSION(0, 1, 0);
        appInfo.pEngineName             = "Lux";
        appInfo.engineVersion           = VK_MAKE_VERSION(0, 1, 0);
        appInfo.apiVersion              = VK_API_VERSION_1_3;

        const char* extensions[] = {
            VK_KHR_SURFACE_EXTENSION_NAME,
            VK_KHR_WIN32_SURFACE_EXTENSION_NAME,
            #if defined(LX_DEBUG)
                VK_EXT_DEBUG_UTILS_EXTENSION_NAME
            #endif
        };

        u32 extensionCount = sizeof(extensions) / sizeof(extensions[0]);

        VkInstanceCreateInfo createInfo{};
        createInfo.sType                    = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
        createInfo.pApplicationInfo         = &appInfo;
        createInfo.enabledExtensionCount    = extensionCount;
        createInfo.ppEnabledExtensionNames  = extensions;
        createInfo.enabledLayerCount        = 0;
        createInfo.ppEnabledLayerNames      = nullptr;

        #if defined(LX_DEBUG)
            const char* validationLayer = "VK_LAYER_KHRONOS_validation";

            u32 layerCount = 0;
            vkEnumerateInstanceLayerProperties(&layerCount, nullptr);
        
            VkLayerProperties availableLayers[64];
            vkEnumerateInstanceLayerProperties(&layerCount, availableLayers);
        
            bool validationAvailable = false;
            for (u32 i = 0; i < layerCount; i++)
            {
                if(::strcmp(availableLayers[i].layerName, validationLayer) == 0)
                {
                    validationAvailable = true;
                    break;
                }
            }
            
            if(validationAvailable){
                createInfo.enabledLayerCount = 1;
                createInfo.ppEnabledLayerNames = &validationLayer;
                ::printf("[Lux] Validation layers enabled\n");
            }
            else
            {
                ::printf("[Lux] Warning: Validation layers not available\n");
            }
        #endif

        VkResult result = vkCreateInstance(&createInfo, nullptr, &m_Instance);
        LX_ASSERT(result == VK_SUCCESS, "Failed to create VkInstance");
        
        volkLoadInstance(m_Instance);

        #if defined(LX_DEBUG)
            if(validationAvailable)
            {
                VkDebugUtilsMessengerCreateInfoEXT messengerInfo{};
                messengerInfo.sType             = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
                messengerInfo.messageSeverity   =
                    VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                    VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
                messengerInfo.messageType       =
                    VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                    VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                    VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
                messengerInfo.pfnUserCallback   = DebugCallback;

                result = vkCreateDebugUtilsMessengerEXT(
                    m_Instance, &messengerInfo, nullptr, &m_DebugMessenger
                );

                LX_ASSERT(result == VK_SUCCESS, "Failed to create debug messenger");

            }
        #endif
            
        ::printf("[Lux] Vulkan instance created\n");
        return true;
    }

    void VulkanBackend::Shutdown()
    {
        if(m_Device != VK_NULL_HANDLE)
        {
            // Wait for the GPU to finish all work before destroying anything
            vkDeviceWaitIdle(m_Device);

            //Destroy Synchronization objects
            for (u32 i = 0; i < m_SwapchainImageCount; i++)
            {
                if (m_ImageAvailableSemaphores[i] != VK_NULL_HANDLE)
                {
                    vkDestroySemaphore(
                        m_Device, m_ImageAvailableSemaphores[i], nullptr);
                    m_ImageAvailableSemaphores[i] = VK_NULL_HANDLE;
                }

                if (m_RenderFinishedSemaphores[i] != VK_NULL_HANDLE)
                {
                    vkDestroySemaphore(
                        m_Device, m_RenderFinishedSemaphores[i], nullptr);
                    m_RenderFinishedSemaphores[i] = VK_NULL_HANDLE;
                }

                if (m_InFlightFences[i] != VK_NULL_HANDLE)
                {
                    vkDestroyFence(m_Device, m_InFlightFences[i], nullptr);
                    m_InFlightFences[i] = VK_NULL_HANDLE;
                }
            }
            ::printf("[Lux] Sync objects destroyed\n");

            //Destroy Command pool
            if (m_CommandPool != VK_NULL_HANDLE)
            {
                vkDestroyCommandPool(m_Device, m_CommandPool, nullptr);
                m_CommandPool = VK_NULL_HANDLE;
                ::printf("[Lux] Command pool destroyed\n");
            }

            //Destroy Framebuffers
            for(u32 i = 0; i < m_SwapchainImageCount; i++)
            {
                if(m_Framebuffers[i] != VK_NULL_HANDLE)
                {
                    vkDestroyFramebuffer(m_Device, m_Framebuffers[i], nullptr);
                    m_Framebuffers[i] = VK_NULL_HANDLE;
                }
            }
            ::printf("[Lux] Framebuffers destroyed\n");

            //Destroy Pipeline
            if (m_Pipeline != VK_NULL_HANDLE)
            {
                vkDestroyPipeline(m_Device, m_Pipeline, nullptr);
                m_Pipeline = VK_NULL_HANDLE;
                ::printf("[Lux] Pipeline destroyed\n");
            }

            //Destroy Pipeline layout
            if (m_PipelineLayout != VK_NULL_HANDLE)
            {
                vkDestroyPipelineLayout(m_Device, m_PipelineLayout, nullptr);
                m_PipelineLayout = VK_NULL_HANDLE;
                ::printf("[Lux] Pipeline layout destroyed\n");
            }

            //Destroy RenderPass
            if(m_RenderPass != VK_NULL_HANDLE)
            {
                vkDestroyRenderPass(m_Device, m_RenderPass, nullptr);
                m_RenderPass = VK_NULL_HANDLE;
                ::printf("[Lux] RenderPass destroyed\n");
            }

            //Destroy depth buffer
            if (m_DepthImageView != VK_NULL_HANDLE)
            {
                vkDestroyImageView(m_Device, m_DepthImageView, nullptr);
                m_DepthImageView = VK_NULL_HANDLE;
            }
            if (m_DepthImage != VK_NULL_HANDLE)
            {
                vmaDestroyImage(m_Allocator, m_DepthImage, m_DepthAllocation);
                m_DepthImage      = VK_NULL_HANDLE;
                m_DepthAllocation = VK_NULL_HANDLE;
                ::printf("[Lux] Depth buffer destroyed\n");
            }

            //Destroy ImageViews
            for(u32 i = 0; i < m_SwapchainImageCount; i++)
            {
                if(m_SwapchainImageViews[i] != VK_NULL_HANDLE)
                {
                    vkDestroyImageView(m_Device, m_SwapchainImageViews[i], nullptr);
                    m_SwapchainImageViews[i] = VK_NULL_HANDLE;
                }
            }

            //Destroy Swapchain
            if(m_Swapchain != VK_NULL_HANDLE)
            {
                vkDestroySwapchainKHR(m_Device, m_Swapchain, nullptr);
                m_Swapchain = VK_NULL_HANDLE;
                ::printf("[Lux] Swapchain destroyed\n");
            }

            //Destroy uniform buffers
            for (u32 i = 0; i < m_SwapchainImageCount; i++)
            {
                if (m_UniformBuffers[i] != VK_NULL_HANDLE)
                {
                    vmaDestroyBuffer(m_Allocator, m_UniformBuffers[i], m_UniformAllocations[i]);
                    m_UniformBuffers[i]     = VK_NULL_HANDLE;
                    m_UniformAllocations[i] = VK_NULL_HANDLE;
                }
                if(m_LightBuffers[i] != VK_NULL_HANDLE)
                {
                    vmaDestroyBuffer(m_Allocator, m_LightBuffers[i], m_LightAllocations[i]);
                    m_LightBuffers[i] = VK_NULL_HANDLE;
                    m_LightAllocations[i] = VK_NULL_HANDLE;
                }
                if(m_SkinningBuffers[i] != VK_NULL_HANDLE)
                {
                    vmaDestroyBuffer(m_Allocator, m_SkinningBuffers[i], m_SkinningAllocations[i]);
                    m_SkinningBuffers[i] = VK_NULL_HANDLE;
                    m_SkinningAllocations[i] = VK_NULL_HANDLE;
                }
            }
            ::printf("[Lux] Uniform buffers destroyed\n");

            //Destroy descriptor pool
            //Destroying the pool implicitly frees all descriptor sets
            if (m_DescriptorPool != VK_NULL_HANDLE)
            {
                vkDestroyDescriptorPool(m_Device, m_DescriptorPool, nullptr);
                m_DescriptorPool = VK_NULL_HANDLE;
                ::printf("[Lux] Descriptor pool destroyed\n");
            }

            // Destroy descriptor set layout
            if (m_DescriptorSetLayout != VK_NULL_HANDLE)
            {
                vkDestroyDescriptorSetLayout(m_Device, m_DescriptorSetLayout, nullptr);
                m_DescriptorSetLayout = VK_NULL_HANDLE;
                ::printf("[Lux] Descriptor set layout destroyed\n");
            }

            // Destroy all active textures
            for (u32 i = 0; i < MAX_TEXTURES; i++)
            {
                if (m_Textures[i].inUse)
                {
                    vkDestroyImageView(m_Device, m_Textures[i].view, nullptr);
                    vmaDestroyImage(
                        m_Allocator, m_Textures[i].image, m_Textures[i].allocation);
                    m_Textures[i] = Texture{};
                }
            }
            ::printf("[Lux] Texture pool cleared\n");

            //Destroy all active buffers
            for (u32 i = 0; i < MAX_BUFFERS; i++)
            {
                if (m_Buffers[i].inUse)
                {
                    vmaDestroyBuffer(
                        m_Allocator, m_Buffers[i].buffer, m_Buffers[i].allocation);
                    m_Buffers[i] = Buffer{};
                }
            }
            ::printf("[Lux] Buffer pool cleared\n");

            // Destroy allocator
            if (m_Allocator != VK_NULL_HANDLE)
            {
                vmaDestroyAllocator(m_Allocator);
                m_Allocator = VK_NULL_HANDLE;
                ::printf("[Lux] VMA allocator destroyed\n");
            }

            // Destroy Sampler
            if(m_Sampler != VK_NULL_HANDLE)
            {
                vkDestroySampler(m_Device, m_Sampler, nullptr);
                m_Sampler = VK_NULL_HANDLE;
                ::printf("[Lux] Sampler destroyed\n");
            }

            // Destroy Device
            vkDestroyDevice(m_Device, nullptr);
            m_Device = VK_NULL_HANDLE;
            ::printf("[Lux] Logical device destroyed\n");
        }

        if(m_Surface != VK_NULL_HANDLE){
            vkDestroySurfaceKHR(m_Instance, m_Surface, nullptr);
            m_Surface = VK_NULL_HANDLE;
            ::printf("[Lux] Surface destroyed\n");
        }

        if(m_Instance != VK_NULL_HANDLE)
        {
        #if defined(LX_DEBUG)
            if(m_DebugMessenger != VK_NULL_HANDLE){
                vkDestroyDebugUtilsMessengerEXT(m_Instance, m_DebugMessenger, nullptr);
                m_DebugMessenger = VK_NULL_HANDLE;
            }
        #endif
            vkDestroyInstance(m_Instance, nullptr);
            m_Instance = VK_NULL_HANDLE;
            ::printf("[Lux] Vulkan instance destroyed\n");
        }
    }

    bool VulkanBackend::InitSurface(const WindowHandle& window)
    {
        #if defined(_WIN32)
            VkWin32SurfaceCreateInfoKHR createInfo{};
            createInfo.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
            createInfo.hwnd = static_cast<HWND>(window.hwnd);
            createInfo.hinstance = static_cast<HINSTANCE>(window.hinstance);

            VkResult result = vkCreateWin32SurfaceKHR(m_Instance, &createInfo, nullptr, &m_Surface);

            LX_ASSERT(result == VK_SUCCESS, "Failed to create Win32 surface");
            if(result != VK_SUCCESS)
                return false;

            ::printf("Vulkan surface created successfully\n");
            return true;
        #endif

        #if defined(__linux__)
            return false;
        #endif
    }
    
    bool VulkanBackend::SelectPhysicalDevice()
    {
        u32 deviceCount = 0;
        vkEnumeratePhysicalDevices(m_Instance, &deviceCount, nullptr);
        LX_ASSERT(deviceCount > 0, "No Vulkan capable GPU found");
        if(deviceCount == 0)
            return false;

        VkPhysicalDevice devices[8];
        vkEnumeratePhysicalDevices(m_Instance, &deviceCount, devices);

        VkPhysicalDevice bestDevice = VK_NULL_HANDLE;
        u32 bestScore = 0;
        u32 bestFamily = UINT32_MAX;
        
        for(u32 i = 0; i < deviceCount; i++)
        {
            VkPhysicalDevice device = devices[i];
            VkPhysicalDeviceProperties properties{};
            vkGetPhysicalDeviceProperties(device, &properties);

            //Disqualify GPUs that don't support Vulkan 1.3
            if(VK_API_VERSION_MAJOR(properties.apiVersion) < 1 || VK_API_VERSION_MINOR(properties.apiVersion) < 3)
            {
                ::printf("[Lux] Skipping %s - Does not support Vulkan 1.3\n", properties.deviceName);
                continue;
            }

            u32 familyCount = 0;
            vkGetPhysicalDeviceQueueFamilyProperties(device, &familyCount, nullptr);

            VkQueueFamilyProperties families[32];
            vkGetPhysicalDeviceQueueFamilyProperties(device, &familyCount, families);

            u32 graphicsFamily = UINT32_MAX;
            for(u32 f = 0; f < familyCount; f++)
            {
                if (!(families[f].queueFlags & VK_QUEUE_GRAPHICS_BIT))
                    continue;

                VkBool32 presentSupport = VK_FALSE;
                vkGetPhysicalDeviceSurfaceSupportKHR(
                    device, f, m_Surface, &presentSupport);

                if (presentSupport == VK_TRUE)
                {
                    graphicsFamily = f;
                    break;
                }
            } 
            if (graphicsFamily == UINT32_MAX)
            {
                ::printf("[Lux] Skipping %s — no graphics queue family\n", properties.deviceName);
                continue;
            }

            u32 score = 0;
            if(properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU)
                score += 1000; //prefer dedicated GPUs
            else if(properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU)
                score += 100;

            if(score > bestScore)
            {
                bestDevice = device;
                bestFamily = graphicsFamily;
                bestScore = score;
            }
        }

        if(bestDevice == VK_NULL_HANDLE)
        {
            ::printf("[Lux] No suitable GPU found\n");
            return false;
        }

        m_PhysicalDevice = bestDevice;
        m_GraphicsQueueFamily = bestFamily;

        // Print picked GPU
        VkPhysicalDeviceProperties chosen{};
        vkGetPhysicalDeviceProperties(m_PhysicalDevice, &chosen);
        ::printf("[Lux] Selected GPU: %s\n", chosen.deviceName);
        ::printf("[Lux] Graphics queue family: %u\n", m_GraphicsQueueFamily);

        return true;
    }

    bool VulkanBackend::InitDevice()
    {
        //One graphics queue at full priority
        f32 queuePriority = 1.0f;

        VkDeviceQueueCreateInfo queueInfo{};
        queueInfo.sType                 = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queueInfo.queueFamilyIndex      = m_GraphicsQueueFamily;
        queueInfo.queueCount            = 1;
        queueInfo.pQueuePriorities      = &queuePriority;

        const char* deviceExtensions[] =
        {
            VK_KHR_SWAPCHAIN_EXTENSION_NAME
        };

        VkPhysicalDeviceFeatures features{};
        features.samplerAnisotropy = VK_TRUE;

        VkDeviceCreateInfo createInfo{};
        createInfo.sType                   = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
        createInfo.queueCreateInfoCount    = 1;
        createInfo.pQueueCreateInfos       = &queueInfo;
        createInfo.enabledExtensionCount   = 1;
        createInfo.ppEnabledExtensionNames = deviceExtensions;
        createInfo.pEnabledFeatures        = &features;

        VkResult result = vkCreateDevice(m_PhysicalDevice, &createInfo, nullptr, &m_Device);
        LX_ASSERT(result == VK_SUCCESS, "Failed to create logical device");

        volkLoadDevice(m_Device);

        vkGetDeviceQueue(m_Device, m_GraphicsQueueFamily, 0, &m_GraphicsQueue);

        ::printf("[Lux] Logical device created\n");
        ::printf("[Lux] Graphics queue retrieved\n");
        return true;
    }

    bool VulkanBackend::InitSwapchain()
    {
        VkSurfaceCapabilitiesKHR capabilities{};
        vkGetPhysicalDeviceSurfaceCapabilitiesKHR(m_PhysicalDevice, m_Surface, &capabilities);

        u32 formatCount = 0;
        vkGetPhysicalDeviceSurfaceFormatsKHR(m_PhysicalDevice, m_Surface, &formatCount, nullptr);

        VkSurfaceFormatKHR formats[32];
        vkGetPhysicalDeviceSurfaceFormatsKHR(m_PhysicalDevice, m_Surface, &formatCount, formats);

        VkSurfaceFormatKHR chosenFormat = formats[0];
        for (u32 i = 0; i < formatCount; i++)
        {
            if (formats[i].format     == VK_FORMAT_B8G8R8A8_SRGB &&
                formats[i].colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
            {
                chosenFormat = formats[i];
                break;
            }
        }

        u32 presentModeCount = 0;
        vkGetPhysicalDeviceSurfacePresentModesKHR(m_PhysicalDevice, m_Surface, &presentModeCount, nullptr);

        VkPresentModeKHR presentModes[8];
        vkGetPhysicalDeviceSurfacePresentModesKHR(m_PhysicalDevice, m_Surface, &presentModeCount, presentModes);
    
        VkPresentModeKHR chosenPresentMode = VK_PRESENT_MODE_FIFO_KHR;
        for (u32 i = 0; i < presentModeCount; i++)
        {
            if (presentModes[i] == VK_PRESENT_MODE_MAILBOX_KHR)
            {
                chosenPresentMode = VK_PRESENT_MODE_MAILBOX_KHR;
                break;
            }
        }

        VkExtent2D extent;

        if (capabilities.currentExtent.width != UINT32_MAX)
        {
            extent = capabilities.currentExtent;
        }
        else
        {
            extent.width  = 1280;
            extent.height = 720;

            if (extent.width < capabilities.minImageExtent.width)
                extent.width = capabilities.minImageExtent.width;
            if (extent.width > capabilities.maxImageExtent.width)
                extent.width = capabilities.maxImageExtent.width;
            if (extent.height < capabilities.minImageExtent.height)
                extent.height = capabilities.minImageExtent.height;
            if (extent.height > capabilities.maxImageExtent.height)
                extent.height = capabilities.maxImageExtent.height;
        }

        //Triple buffering
        u32 imageCount = 3;

        if (imageCount < capabilities.minImageCount)
            imageCount = capabilities.minImageCount;

        if (capabilities.maxImageCount > 0 &&
            imageCount > capabilities.maxImageCount)
            imageCount = capabilities.maxImageCount;

        VkSwapchainCreateInfoKHR createInfo{};
        createInfo.sType            = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
        createInfo.surface          = m_Surface;
        createInfo.minImageCount    = imageCount;
        createInfo.imageFormat      = chosenFormat.format;
        createInfo.imageColorSpace  = chosenFormat.colorSpace;
        createInfo.imageExtent      = extent;
        createInfo.imageArrayLayers = 1;  // always 1 unless stereoscopic 3D
        createInfo.imageUsage       = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
        createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
        createInfo.preTransform     = capabilities.currentTransform;
        createInfo.compositeAlpha   = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
        createInfo.presentMode      = chosenPresentMode;
        createInfo.clipped          = VK_TRUE;
        createInfo.oldSwapchain     = VK_NULL_HANDLE;

        VkResult result = vkCreateSwapchainKHR(
            m_Device, &createInfo, nullptr, &m_Swapchain);
        LX_ASSERT(result == VK_SUCCESS, "Failed to create swapchain");
        if (result != VK_SUCCESS)
            return false;

        
        vkGetSwapchainImagesKHR(m_Device, m_Swapchain, &m_SwapchainImageCount, nullptr);
        vkGetSwapchainImagesKHR(m_Device, m_Swapchain, &m_SwapchainImageCount, m_SwapchainImages);

        m_SwapchainFormat = chosenFormat.format;
        m_SwapchainExtent = extent;

        for (u32 i = 0; i < m_SwapchainImageCount; i++)
        {
            VkImageViewCreateInfo viewInfo{};
            viewInfo.sType    = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
            viewInfo.image    = m_SwapchainImages[i];
            viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
            viewInfo.format   = m_SwapchainFormat;

            viewInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
            viewInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
            viewInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
            viewInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;

            viewInfo.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
            viewInfo.subresourceRange.baseMipLevel   = 0;
            viewInfo.subresourceRange.levelCount     = 1;
            viewInfo.subresourceRange.baseArrayLayer = 0;
            viewInfo.subresourceRange.layerCount     = 1;

            result = vkCreateImageView(
                m_Device, &viewInfo, nullptr, &m_SwapchainImageViews[i]);
            LX_ASSERT(result == VK_SUCCESS, "Failed to create swapchain image view");
            if (result != VK_SUCCESS)
                return false;
        }

        ::printf("[Lux] Swapchain created (%ux%u, %u images)\n",
            extent.width, extent.height, m_SwapchainImageCount);

        return true;
    }

    bool VulkanBackend::InitRenderPass()
    {
        // ── Color attachment ──────────────────────────────────────────────────
        VkAttachmentDescription colorAttachment{};
        colorAttachment.format         = m_SwapchainFormat;
        colorAttachment.samples        = VK_SAMPLE_COUNT_1_BIT;
        colorAttachment.loadOp         = VK_ATTACHMENT_LOAD_OP_CLEAR;
        colorAttachment.storeOp        = VK_ATTACHMENT_STORE_OP_STORE;
        colorAttachment.stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        colorAttachment.initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED;
        colorAttachment.finalLayout    = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

        // ── Depth attachment ──────────────────────────────────────────────────
        VkAttachmentDescription depthAttachment{};
        depthAttachment.format         = m_DepthFormat;
        depthAttachment.samples        = VK_SAMPLE_COUNT_1_BIT;
        depthAttachment.loadOp         = VK_ATTACHMENT_LOAD_OP_CLEAR;
        depthAttachment.storeOp        = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        depthAttachment.stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        depthAttachment.initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED;
        depthAttachment.finalLayout    =
            VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

        // ── Attachment references ─────────────────────────────────────────────
        VkAttachmentReference colorRef{};
        colorRef.attachment = 0;
        colorRef.layout     = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

        VkAttachmentReference depthRef{};
        depthRef.attachment = 1;
        depthRef.layout     = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

        // ── Subpass ───────────────────────────────────────────────────────────
        VkSubpassDescription subpass{};
        subpass.pipelineBindPoint       = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpass.colorAttachmentCount    = 1;
        subpass.pColorAttachments       = &colorRef;
        subpass.pDepthStencilAttachment = &depthRef;

        // ── Dependency ────────────────────────────────────────────────────────
        VkSubpassDependency dependency{};
        dependency.srcSubpass    = VK_SUBPASS_EXTERNAL;
        dependency.dstSubpass    = 0;
        dependency.srcStageMask  =
            VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
            VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
        dependency.srcAccessMask = 0;
        dependency.dstStageMask  =
            VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
            VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
        dependency.dstAccessMask =
            VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT |
            VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

        // ── Create render pass ────────────────────────────────────────────────
        VkAttachmentDescription attachments[] = { colorAttachment, depthAttachment };

        VkRenderPassCreateInfo createInfo{};
        createInfo.sType           = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        createInfo.attachmentCount = 2;
        createInfo.pAttachments    = attachments;
        createInfo.subpassCount    = 1;
        createInfo.pSubpasses      = &subpass;
        createInfo.dependencyCount = 1;
        createInfo.pDependencies   = &dependency;

        VkResult result = vkCreateRenderPass(
            m_Device, &createInfo, nullptr, &m_RenderPass);

        LX_ASSERT(result == VK_SUCCESS, "Failed to create render pass");
        if (result != VK_SUCCESS)
            return false;

        ::printf("[Lux] Render pass created\n");
        return true;
    }

    bool VulkanBackend::InitFramebuffers()
    {
        for(u32 i = 0; i < m_SwapchainImageCount; i++)
        {
            VkImageView attachments[] = { m_SwapchainImageViews[i], m_DepthImageView} ;

            VkFramebufferCreateInfo createInfo{};
            createInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
            createInfo.renderPass = m_RenderPass;
            createInfo.attachmentCount = 2;
            createInfo.pAttachments = attachments;
            createInfo.width = m_SwapchainExtent.width;
            createInfo.height = m_SwapchainExtent.height;
            createInfo.layers = 1;

            VkResult result = vkCreateFramebuffer(m_Device, &createInfo, nullptr, &m_Framebuffers[i]);

            LX_ASSERT(result == VK_SUCCESS, "Failed to create framebuffer");
            if(result != VK_SUCCESS) return false;    
        }
        
        ::printf("[Lux] Framebuffers created (%u)\n", m_SwapchainImageCount);
        return true;
    }

    bool VulkanBackend::InitCommands()
    {
        VkCommandPoolCreateInfo poolInfo{};
        poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        poolInfo.queueFamilyIndex = m_GraphicsQueueFamily;
        poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

        VkResult result = vkCreateCommandPool(
        m_Device, &poolInfo, nullptr, &m_CommandPool);

        LX_ASSERT(result == VK_SUCCESS, "Failed to create command pool");
        if (result != VK_SUCCESS)
            return false;

        VkCommandBufferAllocateInfo allocInfo{};
        allocInfo.sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocInfo.commandPool        = m_CommandPool;
        allocInfo.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocInfo.commandBufferCount = m_SwapchainImageCount;

        result = vkAllocateCommandBuffers(
            m_Device, &allocInfo, m_CommandBuffers);

        LX_ASSERT(result == VK_SUCCESS, "Failed to allocate command buffers");
        if (result != VK_SUCCESS)
            return false;

        ::printf("[Lux] Command pool and buffers created\n");
        return true;
    }

    bool VulkanBackend::InitSyncObjects()
    {
        VkSemaphoreCreateInfo semaphoreInfo{};
        semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

        VkFenceCreateInfo fenceInfo{};
        fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

        for (u32 i = 0; i < m_SwapchainImageCount; i++)
        {
            VkResult result;

            result = vkCreateSemaphore(
                m_Device, &semaphoreInfo, nullptr, &m_ImageAvailableSemaphores[i]);
            LX_ASSERT(result == VK_SUCCESS, "Failed to create image available semaphore");
            if (result != VK_SUCCESS) return false;

            result = vkCreateSemaphore(
                m_Device, &semaphoreInfo, nullptr, &m_RenderFinishedSemaphores[i]);
            LX_ASSERT(result == VK_SUCCESS, "Failed to create render finished semaphore");
            if (result != VK_SUCCESS) return false;

            result = vkCreateFence(
                m_Device, &fenceInfo, nullptr, &m_InFlightFences[i]);
            LX_ASSERT(result == VK_SUCCESS, "Failed to create in flight fence");
            if (result != VK_SUCCESS) return false;
        }

        ::printf("[Lux] Sync objects created\n");
        return true;
    }

    bool VulkanBackend::InitPipeline()
    {
        VkShaderModule vertModule = VK_NULL_HANDLE;
        VkShaderModule fragModule = VK_NULL_HANDLE;

        if (!LoadShaderModule(m_Device, "Shaders/triangle.vert.spv", &vertModule))
            return false;

        if (!LoadShaderModule(m_Device, "Shaders/triangle.frag.spv", &fragModule))
            return false;

        VkPipelineShaderStageCreateInfo vertStage{};
        vertStage.sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        vertStage.stage  = VK_SHADER_STAGE_VERTEX_BIT;
        vertStage.module = vertModule;
        vertStage.pName  = "main"; // entry point function name in the shader

        VkPipelineShaderStageCreateInfo fragStage{};
        fragStage.sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        fragStage.stage  = VK_SHADER_STAGE_FRAGMENT_BIT;
        fragStage.module = fragModule;
        fragStage.pName  = "main";

        VkPipelineShaderStageCreateInfo shaderStages[] = { vertStage, fragStage };

        VkVertexInputBindingDescription bindingDescription = Vertex::GetBindingDescription();
        VkVertexInputAttributeDescription attributeDescriptions[5];
        u32 attributeCount = 0;
        Vertex::GetAttributeDescriptions(attributeDescriptions, &attributeCount);

        VkPipelineVertexInputStateCreateInfo vertexInput{};
        vertexInput.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
        vertexInput.vertexBindingDescriptionCount   = 1;
        vertexInput.pVertexBindingDescriptions = &bindingDescription;
        vertexInput.vertexAttributeDescriptionCount = attributeCount;
        vertexInput.pVertexAttributeDescriptions = attributeDescriptions;

        VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
        inputAssembly.sType    = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
        inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        inputAssembly.primitiveRestartEnable = VK_FALSE;

        VkViewport viewport{};
        viewport.x        = 0.0f;
        viewport.y        = 0.0f;
        viewport.width    = (f32)m_SwapchainExtent.width;
        viewport.height   = (f32)m_SwapchainExtent.height;
        viewport.minDepth = 0.0f;
        viewport.maxDepth = 1.0f;

        VkRect2D scissor{};
        scissor.offset = {0, 0};
        scissor.extent = m_SwapchainExtent;

        VkPipelineViewportStateCreateInfo viewportState{};
        viewportState.sType         = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
        viewportState.viewportCount = 1;
        viewportState.pViewports    = &viewport;
        viewportState.scissorCount  = 1;
        viewportState.pScissors     = &scissor;

        //Rasterizer
        VkPipelineRasterizationStateCreateInfo rasterizer{};
        rasterizer.sType            = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
        rasterizer.depthClampEnable = VK_FALSE;
        rasterizer.polygonMode      = VK_POLYGON_MODE_FILL;
        rasterizer.cullMode         = VK_CULL_MODE_BACK_BIT;
        rasterizer.frontFace        = VK_FRONT_FACE_COUNTER_CLOCKWISE;
        rasterizer.lineWidth        = 1.0f;

        VkPipelineMultisampleStateCreateInfo multisampling{};
        multisampling.sType                = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
        multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
        multisampling.sampleShadingEnable  = VK_FALSE;

        VkPipelineDepthStencilStateCreateInfo depthStencil{};
        depthStencil.sType                 = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
        depthStencil.depthTestEnable       = VK_TRUE;
        depthStencil.depthWriteEnable      = VK_TRUE;
        depthStencil.depthCompareOp        = VK_COMPARE_OP_LESS;
        depthStencil.depthBoundsTestEnable = VK_FALSE;
        depthStencil.stencilTestEnable     = VK_FALSE;

        //Color Blending
        VkPipelineColorBlendAttachmentState colorBlendAttachment{};
        colorBlendAttachment.colorWriteMask =
            VK_COLOR_COMPONENT_R_BIT |
            VK_COLOR_COMPONENT_G_BIT |
            VK_COLOR_COMPONENT_B_BIT |
            VK_COLOR_COMPONENT_A_BIT;
        colorBlendAttachment.blendEnable = VK_FALSE;

        VkPipelineColorBlendStateCreateInfo colorBlending{};
        colorBlending.sType           = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
        colorBlending.logicOpEnable   = VK_FALSE;
        colorBlending.attachmentCount = 1;
        colorBlending.pAttachments    = &colorBlendAttachment;

        VkPipelineLayoutCreateInfo layoutInfo{};
        layoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        layoutInfo.setLayoutCount         = 1;
        layoutInfo.pSetLayouts            = &m_DescriptorSetLayout;
        layoutInfo.pushConstantRangeCount = 0;

        VkResult result = vkCreatePipelineLayout(m_Device, &layoutInfo, nullptr, &m_PipelineLayout);
        LX_ASSERT(result == VK_SUCCESS, "Failed to create pipeline layout");
        if (result != VK_SUCCESS)
            return false;

        VkGraphicsPipelineCreateInfo pipelineInfo{};
        pipelineInfo.sType               = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
        pipelineInfo.stageCount          = 2;
        pipelineInfo.pStages             = shaderStages;
        pipelineInfo.pVertexInputState   = &vertexInput;
        pipelineInfo.pInputAssemblyState = &inputAssembly;
        pipelineInfo.pViewportState      = &viewportState;
        pipelineInfo.pRasterizationState = &rasterizer;
        pipelineInfo.pMultisampleState   = &multisampling;
        pipelineInfo.pDepthStencilState  = &depthStencil;
        pipelineInfo.pColorBlendState    = &colorBlending;
        pipelineInfo.pDynamicState       = nullptr;
        pipelineInfo.layout              = m_PipelineLayout;
        pipelineInfo.renderPass          = m_RenderPass;
        pipelineInfo.subpass             = 0;

        result = vkCreateGraphicsPipelines(m_Device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &m_Pipeline);
        LX_ASSERT(result == VK_SUCCESS, "Failed to create graphics pipeline");
        if (result != VK_SUCCESS)
            return false;

        vkDestroyShaderModule(m_Device, vertModule, nullptr);
        vkDestroyShaderModule(m_Device, fragModule, nullptr);

        ::printf("[Lux] Pipeline created\n");
        return true;
    }

    bool VulkanBackend::InitAllocator()
    {
        VmaVulkanFunctions vulkanFunctions{};
        vulkanFunctions.vkGetInstanceProcAddr = vkGetInstanceProcAddr;
        vulkanFunctions.vkGetDeviceProcAddr = vkGetDeviceProcAddr;

        VmaAllocatorCreateInfo allocatorInfo{};
        allocatorInfo.instance         = m_Instance;
        allocatorInfo.physicalDevice   = m_PhysicalDevice;
        allocatorInfo.device           = m_Device;
        allocatorInfo.vulkanApiVersion = VK_API_VERSION_1_3;
        allocatorInfo.pVulkanFunctions = &vulkanFunctions;

        VkResult result = vmaCreateAllocator(&allocatorInfo, &m_Allocator);
        LX_ASSERT(result == VK_SUCCESS, "Failed to create VMA allocator");
        if (result != VK_SUCCESS)
            return false;

        ::printf("[Lux] VMA allocator created\n");
        return true;
    }

    bool VulkanBackend::InitUniformBuffers()
    {
        VkDeviceSize transformSize = sizeof(GlobalUBO);
        VkDeviceSize lightSize     = sizeof(LightUBO);

        for (u32 i = 0; i < m_SwapchainImageCount; i++)
        {
            // Transform buffer
            {
                VkBufferCreateInfo bufferInfo{};
                bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
                bufferInfo.size  = transformSize;
                bufferInfo.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;

                VmaAllocationCreateInfo allocInfo{};
                allocInfo.usage = VMA_MEMORY_USAGE_CPU_TO_GPU;
                allocInfo.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT;

                VmaAllocationInfo allocationInfo{};
                VkResult result = vmaCreateBuffer(
                    m_Allocator,
                    &bufferInfo, &allocInfo,
                    &m_UniformBuffers[i], &m_UniformAllocations[i],
                    &allocationInfo);

                LX_ASSERT(result == VK_SUCCESS, "Failed to create transform buffer");
                if (result != VK_SUCCESS) return false;

                m_UniformMapped[i] = allocationInfo.pMappedData;
            }

            // Light buffer
            {
                VkBufferCreateInfo bufferInfo{};
                bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
                bufferInfo.size  = lightSize;
                bufferInfo.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;

                VmaAllocationCreateInfo allocInfo{};
                allocInfo.usage = VMA_MEMORY_USAGE_CPU_TO_GPU;
                allocInfo.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT;

                VmaAllocationInfo allocationInfo{};
                VkResult result = vmaCreateBuffer(
                    m_Allocator,
                    &bufferInfo, &allocInfo,
                    &m_LightBuffers[i], &m_LightAllocations[i],
                    &allocationInfo);

                LX_ASSERT(result == VK_SUCCESS, "Failed to create light buffer");
                if (result != VK_SUCCESS) return false;

                m_LightMapped[i] = allocationInfo.pMappedData;
            }

            //Skinning buffer - bone matrices
            {
                VkBufferCreateInfo bufferInfo{};
                bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
                bufferInfo.size = sizeof(SkinningUBO);
                bufferInfo.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;

                VmaAllocationCreateInfo allocInfo{};
                allocInfo.usage = VMA_MEMORY_USAGE_CPU_TO_GPU;
                allocInfo.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT;

                VmaAllocationInfo allocationInfo{};
                VkResult result = vmaCreateBuffer(
                    m_Allocator,
                    &bufferInfo, &allocInfo,
                    &m_SkinningBuffers[i], &m_SkinningAllocations[i],
                    &allocationInfo
                );

                LX_ASSERT(result == VK_SUCCESS, "Failed to create skinning buffer");
                if(result != VK_SUCCESS) return false;

                m_SkinningMapped[i] = allocationInfo.pMappedData;

                // Initialize all bones to identity right after creation
                // so unanimated meshes render correctly
                SkinningUBO defaultSkinning{};
                for (u32 b = 0; b < MAX_BONES; b++)
                    defaultSkinning.boneMatrices[b] = glm::mat4(1.0f);

                ::memcpy(m_SkinningMapped[i], &defaultSkinning, sizeof(SkinningUBO));
            }
        }

        ::printf("[Lux] Uniform buffers created\n");
        return true;
    }

    bool VulkanBackend::InitDescriptors()
    {
        // ── Descriptor Set Layout ─────────────────────────────────────────────
        VkDescriptorSetLayoutBinding bindings[4] = {};

        // Binding 0 — transform uniform buffer (vertex + fragment)
        bindings[0].binding            = 0;
        bindings[0].descriptorType     = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        bindings[0].descriptorCount    = 1;
        bindings[0].stageFlags         = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
        bindings[0].pImmutableSamplers = nullptr;

        // Binding 1 — texture sampler (fragment only)
        bindings[1].binding            = 1;
        bindings[1].descriptorType     = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        bindings[1].descriptorCount    = 1;
        bindings[1].stageFlags         = VK_SHADER_STAGE_FRAGMENT_BIT;
        bindings[1].pImmutableSamplers = nullptr;

        // Binding 2 — light uniform buffer (fragment only)
        bindings[2].binding            = 2;
        bindings[2].descriptorType     = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        bindings[2].descriptorCount    = 1;
        bindings[2].stageFlags         = VK_SHADER_STAGE_FRAGMENT_BIT;
        bindings[2].pImmutableSamplers = nullptr;

        // Binding 3 — skinning bone matrices (vertex only)
        // The vertex shader reads these to deform the mesh
        bindings[3].binding            = 3;
        bindings[3].descriptorType     = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        bindings[3].descriptorCount    = 1;
        bindings[3].stageFlags         = VK_SHADER_STAGE_VERTEX_BIT;

        VkDescriptorSetLayoutCreateInfo layoutInfo{};
        layoutInfo.sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        layoutInfo.flags        = 0;
        layoutInfo.bindingCount = 4;
        layoutInfo.pBindings    = bindings;
        layoutInfo.pNext        = nullptr;

        VkResult result = vkCreateDescriptorSetLayout(
            m_Device, &layoutInfo, nullptr, &m_DescriptorSetLayout);
        LX_ASSERT(result == VK_SUCCESS, "Failed to create descriptor set layout");
        if (result != VK_SUCCESS)
            return false;

        // ── Descriptor Pool ───────────────────────────────────────────────────
        // Large enough for all primitives across all frames in flight
        // MAX_MESHES(256) * MAX_PRIMITIVES(16) * MAX_SWAPCHAIN_IMAGES(3) = 12288
        // We use 1024 as a practical limit — more than enough for any scene
        u32 maxSets = 1024;

        VkDescriptorPoolSize poolSizes[2] = {};

        // Uniform buffers — 2 per set (transform + light)
        poolSizes[0].type            = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        poolSizes[0].descriptorCount = maxSets * 3;

        // Combined image samplers — 1 per set
        poolSizes[1].type            = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        poolSizes[1].descriptorCount = maxSets;

        VkDescriptorPoolCreateInfo poolInfo{};
        poolInfo.sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        poolInfo.flags         = 0;
        poolInfo.maxSets       = maxSets;
        poolInfo.poolSizeCount = 2;
        poolInfo.pPoolSizes    = poolSizes;

        result = vkCreateDescriptorPool(
            m_Device, &poolInfo, nullptr, &m_DescriptorPool);
        LX_ASSERT(result == VK_SUCCESS, "Failed to create descriptor pool");
        if (result != VK_SUCCESS)
            return false;

        // ── Allocate simple descriptor sets for DrawIndexed ───────────────────
        VkDescriptorSetLayout layouts[MAX_SWAPCHAIN_IMAGES];
        for (u32 i = 0; i < m_SwapchainImageCount; i++)
            layouts[i] = m_DescriptorSetLayout;

        VkDescriptorSetAllocateInfo allocInfo{};
        allocInfo.sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        allocInfo.descriptorPool     = m_DescriptorPool;
        allocInfo.descriptorSetCount = m_SwapchainImageCount;
        allocInfo.pSetLayouts        = layouts;

        result = vkAllocateDescriptorSets(m_Device, &allocInfo, m_SimpleDescriptorSets);
        LX_ASSERT(result == VK_SUCCESS, "Failed to allocate simple descriptor sets");
        if (result != VK_SUCCESS)
            return false;

        ::printf("[Lux] Descriptors created\n");
        return true;
    }

    bool VulkanBackend::InitDepthBuffer()
    {
        m_DepthFormat = FindDepthFormat();

        // ── Create depth image ────────────────────────────────────────────────
        VkImageCreateInfo imageInfo{};
        imageInfo.sType         = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        imageInfo.imageType     = VK_IMAGE_TYPE_2D;
        imageInfo.format        = m_DepthFormat;
        imageInfo.extent.width  = m_SwapchainExtent.width;
        imageInfo.extent.height = m_SwapchainExtent.height;
        imageInfo.extent.depth  = 1;
        imageInfo.mipLevels     = 1;
        imageInfo.arrayLayers   = 1;
        imageInfo.samples       = VK_SAMPLE_COUNT_1_BIT;
        imageInfo.tiling        = VK_IMAGE_TILING_OPTIMAL;
        imageInfo.usage         = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
        imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

        VmaAllocationCreateInfo allocInfo{};
        allocInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;

        VkResult result = vmaCreateImage(
            m_Allocator,
            &imageInfo, &allocInfo,
            &m_DepthImage, &m_DepthAllocation,
            nullptr);

        LX_ASSERT(result == VK_SUCCESS, "Failed to create depth image");
        if (result != VK_SUCCESS)
            return false;

        // ── Create depth image view ───────────────────────────────────────────
        VkImageViewCreateInfo viewInfo{};
        viewInfo.sType    = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewInfo.image    = m_DepthImage;
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.format   = m_DepthFormat;

        viewInfo.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_DEPTH_BIT;
        viewInfo.subresourceRange.baseMipLevel   = 0;
        viewInfo.subresourceRange.levelCount     = 1;
        viewInfo.subresourceRange.baseArrayLayer = 0;
        viewInfo.subresourceRange.layerCount     = 1;

        result = vkCreateImageView(
            m_Device, &viewInfo, nullptr, &m_DepthImageView);

        LX_ASSERT(result == VK_SUCCESS, "Failed to create depth image view");
        if (result != VK_SUCCESS)
            return false;

        ::printf("[Lux] Depth buffer created (%ux%u)\n",
            m_SwapchainExtent.width, m_SwapchainExtent.height);

        return true;
    }

    bool VulkanBackend::InitSampler()
    {
        VkSamplerCreateInfo samplerInfo{};
        samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;

        samplerInfo.magFilter = VK_FILTER_LINEAR;
        samplerInfo.minFilter = VK_FILTER_LINEAR;

        samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;

        samplerInfo.anisotropyEnable = VK_TRUE;
        samplerInfo.maxAnisotropy = 16.0f;

        samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
        samplerInfo.mipLodBias = 0.0f;
        samplerInfo.minLod = 0.0f;
        samplerInfo.maxLod = 0.0f;

        VkResult result = vkCreateSampler(m_Device, &samplerInfo, nullptr, &m_Sampler);
        LX_ASSERT(result == VK_SUCCESS, "Failed to create sampler");
        if(result != VK_SUCCESS)
            return false;

        ::printf("[Lux] Sampler created\n");
        return true;
    }

    void VulkanBackend::UpdateUniformBuffer(u32 frameIndex)
    {
        GlobalUBO ubo{};
        ubo.model = m_Model;
        ubo.view = m_View;
        ubo.projection = m_Projection;

        glm::mat4 invView = glm::inverse(m_View);
        ubo.cameraPos = glm::vec4(invView[3]);

        ::memcpy(m_UniformMapped[frameIndex], &ubo, sizeof(ubo));

        // Light stays hardcoded for now — engine will drive this later
        LightUBO light{};
        light.direction = glm::vec4(
            glm::normalize(glm::vec3(-0.5f, -1.0f, -0.5f)), 0.0f);
        light.color   = glm::vec4(1.0f, 0.95f, 0.8f, 1.0f);
        light.ambient = glm::vec4(0.15f, 0.15f, 0.2f, 1.0f);

        ::memcpy(m_LightMapped[frameIndex], &light, sizeof(light));
    }

    VkFormat VulkanBackend::FindDepthFormat()
    {
        // Formats we're willing to use in order of preference
        VkFormat candidates[] =
        {
            VK_FORMAT_D32_SFLOAT,
            VK_FORMAT_D32_SFLOAT_S8_UINT,
            VK_FORMAT_D24_UNORM_S8_UINT
        };

        for (u32 i = 0; i < 3; i++)
        {
            VkFormatProperties props;
            vkGetPhysicalDeviceFormatProperties(
                m_PhysicalDevice, candidates[i], &props);

            // Check if this format supports depth stencil attachment
            if (props.optimalTilingFeatures &
                VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT)
            {
                return candidates[i];
            }
        }

        LX_ASSERT(false, "Failed to find supported depth format");
        return VK_FORMAT_UNDEFINED;
    }

    BufferHandle VulkanBackend::CreateBuffer(const void* data, usize size, VkBufferUsageFlags usage)
    {
        VkBufferCreateInfo stagingInfo{};
        stagingInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        stagingInfo.size = size;
        stagingInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;

        VmaAllocationCreateInfo stagingAllocInfo{};
        stagingAllocInfo.usage = VMA_MEMORY_USAGE_CPU_ONLY;

        VkBuffer stagingBuffer;
        VmaAllocation stagingAllocation;

        VkResult result = vmaCreateBuffer(
            m_Allocator,
            &stagingInfo, &stagingAllocInfo, &stagingBuffer, &stagingAllocation,
            nullptr
        );
        LX_ASSERT(result == VK_SUCCESS, "Failed to create staging buffer");

        //Copy data into staging buffer
        void* mapped;
        vmaMapMemory(m_Allocator, stagingAllocation, &mapped);
        ::memcpy(mapped, data, size);
        vmaUnmapMemory(m_Allocator, stagingAllocation);

        //Device local buffer
        VkBufferCreateInfo bufferInfo{};
        bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bufferInfo.size = size;
        bufferInfo.usage = usage | VK_BUFFER_USAGE_TRANSFER_DST_BIT;

        VmaAllocationCreateInfo bufferAllocInfo{};
        bufferAllocInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;

        //Find a free slot in the buffer pool
        u32 slotIndex = UINT32_MAX;
        for(u32 i = 0; i < MAX_BUFFERS; i++)
        {
            if(!m_Buffers[i].inUse)
            {
                slotIndex = i;
                break;
            }
        }
        LX_ASSERT(slotIndex != UINT32_MAX, "Buffer pool exhausted");

        Buffer& slot = m_Buffers[slotIndex];

        result = vmaCreateBuffer(
            m_Allocator,
            &bufferInfo, &bufferAllocInfo, &slot.buffer, &slot.allocation,
            nullptr
        );
        LX_ASSERT(result == VK_SUCCESS, "Failed to create device buffer");

        slot.size = size;
        slot.inUse = true;

        VkCommandBufferAllocateInfo allocInfo{};
        allocInfo.sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocInfo.commandPool        = m_CommandPool;
        allocInfo.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocInfo.commandBufferCount = 1;

        VkCommandBuffer transferCmd;
        vkAllocateCommandBuffers(m_Device, &allocInfo, &transferCmd);

        VkCommandBufferBeginInfo beginInfo{};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

        vkBeginCommandBuffer(transferCmd, &beginInfo);

        VkBufferCopy copyRegion{};
        copyRegion.size = size;
        vkCmdCopyBuffer(transferCmd, stagingBuffer, slot.buffer, 1, &copyRegion);

        vkEndCommandBuffer(transferCmd);

        VkSubmitInfo submitInfo{};
        submitInfo.sType              = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers    = &transferCmd;

        vkQueueSubmit(m_GraphicsQueue, 1, &submitInfo, VK_NULL_HANDLE);
        vkQueueWaitIdle(m_GraphicsQueue);

        vkFreeCommandBuffers(m_Device, m_CommandPool, 1, &transferCmd);
        vmaDestroyBuffer(m_Allocator, stagingBuffer, stagingAllocation);

        BufferHandle handle{};
        handle.index = slotIndex;
        return handle;
    }

    BufferHandle VulkanBackend::CreateVertexBuffer(const void* data, usize size)
    {
        return CreateBuffer(data, size, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);
    }

    BufferHandle VulkanBackend::CreateIndexBuffer(const void* data, usize size)
    {
        return CreateBuffer(data, size, VK_BUFFER_USAGE_INDEX_BUFFER_BIT);
    }

    TextureHandle VulkanBackend::CreateTexture(const char* path)
    {
        //Load image from disk
        i32 width, height, channels;
        ::printf("[Lux] Attempting to load texture: %s\n", path);
        stbi_uc* pixels = stbi_load(path, &width, &height, &channels, STBI_rgb_alpha);

        if(!pixels){
            ::printf("[Lux] stb error: %s\n", stbi_failure_reason());
            LX_ASSERT(false, "Failed to load texture");
            return TextureHandle{};
        }

        VkDeviceSize imageSize = (VkDeviceSize)(width * height * 4);

        //Upload to staging buffer
        VkBufferCreateInfo stagingInfo{};
        stagingInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        stagingInfo.size = imageSize;
        stagingInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;

        VmaAllocationCreateInfo stagingAllocInfo{};
        stagingAllocInfo.usage = VMA_MEMORY_USAGE_CPU_ONLY;

        VkBuffer stagingBuffer;
        VmaAllocation stagingAllocation;

        vmaCreateBuffer(
            m_Allocator,
            &stagingInfo, &stagingAllocInfo,
            &stagingBuffer, &stagingAllocation,
            nullptr
        );

        void* mapped;
        vmaMapMemory(m_Allocator, stagingAllocation, &mapped);
        ::memcpy(mapped, pixels, (usize)imageSize);
        vmaUnmapMemory(m_Allocator, stagingAllocation);

        stbi_image_free(pixels);

        // ── Create GPU image ──────────────────────────────────────────────────
        VkImageCreateInfo imageInfo{};
        imageInfo.sType         = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        imageInfo.imageType     = VK_IMAGE_TYPE_2D;
        imageInfo.format        = VK_FORMAT_R8G8B8A8_SRGB;
        imageInfo.extent.width  = (u32)width;
        imageInfo.extent.height = (u32)height;
        imageInfo.extent.depth  = 1;
        imageInfo.mipLevels     = 1;
        imageInfo.arrayLayers   = 1;
        imageInfo.samples       = VK_SAMPLE_COUNT_1_BIT;
        imageInfo.tiling        = VK_IMAGE_TILING_OPTIMAL;
        imageInfo.usage         =
            VK_IMAGE_USAGE_TRANSFER_DST_BIT |
            VK_IMAGE_USAGE_SAMPLED_BIT;
        imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

        VmaAllocationCreateInfo imageAllocInfo{};
        imageAllocInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;

        // Find free texture slot
        u32 slotIndex = UINT32_MAX;
        for (u32 i = 0; i < MAX_TEXTURES; i++)
        {
            if (!m_Textures[i].inUse)
            {
                slotIndex = i;
                break;
            }
        }

        LX_ASSERT(slotIndex != UINT32_MAX, "Texture pool exhausted");

        Texture& slot = m_Textures[slotIndex];

        vmaCreateImage(
            m_Allocator,
            &imageInfo, &imageAllocInfo,
            &slot.image, &slot.allocation,
            nullptr);

        // ── Transition and copy ───────────────────────────────────────────────
        VkCommandBufferAllocateInfo allocInfo{};
        allocInfo.sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocInfo.commandPool        = m_CommandPool;
        allocInfo.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocInfo.commandBufferCount = 1;

        VkCommandBuffer cmd;
        vkAllocateCommandBuffers(m_Device, &allocInfo, &cmd);

        VkCommandBufferBeginInfo beginInfo{};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        vkBeginCommandBuffer(cmd, &beginInfo);

        // Transition image to transfer destination layout
        VkImageMemoryBarrier barrier{};
        barrier.sType                           = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier.oldLayout                       = VK_IMAGE_LAYOUT_UNDEFINED;
        barrier.newLayout                       = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        barrier.srcQueueFamilyIndex             = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex             = VK_QUEUE_FAMILY_IGNORED;
        barrier.image                           = slot.image;
        barrier.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
        barrier.subresourceRange.baseMipLevel   = 0;
        barrier.subresourceRange.levelCount     = 1;
        barrier.subresourceRange.baseArrayLayer = 0;
        barrier.subresourceRange.layerCount     = 1;
        barrier.srcAccessMask                   = 0;
        barrier.dstAccessMask                   = VK_ACCESS_TRANSFER_WRITE_BIT;

        vkCmdPipelineBarrier(
            cmd,
            VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
            VK_PIPELINE_STAGE_TRANSFER_BIT,
            0, 0, nullptr, 0, nullptr,
            1, &barrier);

        // Copy staging buffer to image
        VkBufferImageCopy region{};
        region.bufferOffset      = 0;
        region.bufferRowLength   = 0;
        region.bufferImageHeight = 0;
        region.imageSubresource.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
        region.imageSubresource.mipLevel       = 0;
        region.imageSubresource.baseArrayLayer = 0;
        region.imageSubresource.layerCount     = 1;
        region.imageOffset = {0, 0, 0};
        region.imageExtent = {(u32)width, (u32)height, 1};

        vkCmdCopyBufferToImage(
            cmd,
            stagingBuffer,
            slot.image,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            1, &region);

        // Transition image to shader read layout
        barrier.oldLayout     = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        barrier.newLayout     = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

        vkCmdPipelineBarrier(
            cmd,
            VK_PIPELINE_STAGE_TRANSFER_BIT,
            VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
            0, 0, nullptr, 0, nullptr,
            1, &barrier);

        vkEndCommandBuffer(cmd);

        VkSubmitInfo submitInfo{};
        submitInfo.sType              = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers    = &cmd;

        vkQueueSubmit(m_GraphicsQueue, 1, &submitInfo, VK_NULL_HANDLE);
        vkQueueWaitIdle(m_GraphicsQueue);

        vkFreeCommandBuffers(m_Device, m_CommandPool, 1, &cmd);
        vmaDestroyBuffer(m_Allocator, stagingBuffer, stagingAllocation);

        // ── Create image view ─────────────────────────────────────────────────
        VkImageViewCreateInfo viewInfo{};
        viewInfo.sType    = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewInfo.image    = slot.image;
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.format   = VK_FORMAT_R8G8B8A8_SRGB;
        viewInfo.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
        viewInfo.subresourceRange.baseMipLevel   = 0;
        viewInfo.subresourceRange.levelCount     = 1;
        viewInfo.subresourceRange.baseArrayLayer = 0;
        viewInfo.subresourceRange.layerCount     = 1;

        vkCreateImageView(m_Device, &viewInfo, nullptr, &slot.view);

        slot.width  = (u32)width;
        slot.height = (u32)height;
        slot.inUse  = true;

        ::printf("[Lux] Texture loaded (%ux%u): %s\n", slot.width, slot.height, path);

        TextureHandle handle{};
        handle.index = slotIndex;
        return handle;
    }

    void VulkanBackend::DestroyBuffer(BufferHandle handle)
    {
        LX_ASSERT(handle.IsValid(), "Destroying invalid buffer handle");

        Buffer& slot = m_Buffers[handle.index];
        LX_ASSERT(slot.inUse, "Destroying buffer that is not in use");

        vmaDestroyBuffer(m_Allocator, slot.buffer, slot.allocation);
        slot = Buffer{}; // reset to default - marks inUse = false
    }

    void VulkanBackend::DestroyTexture(TextureHandle handle)
    {
        LX_ASSERT(handle.IsValid(), "Destroying invalid texture handle");

        Texture& slot = m_Textures[handle.index];
        LX_ASSERT(slot.inUse, "Destroying texture not in use");

        vkDestroyImageView(m_Device, slot.view, nullptr);
        vmaDestroyImage(m_Allocator, slot.image, slot.allocation);
        slot = Texture{};
    }

    void VulkanBackend::DrawIndexed(BufferHandle vertexBuffer, BufferHandle indexBuffer, u32 indexCount, TextureHandle texture)
    {
        // Simple single-primitive draw
        // Creates a temporary MeshPrimitive and calls DrawPrimitive
        // Used for simple test cases — for real meshes use DrawPrimitive directly

        LX_ASSERT(vertexBuffer.IsValid(), "Invalid vertex buffer");
        LX_ASSERT(indexBuffer.IsValid(),  "Invalid index buffer");
        LX_ASSERT(texture.IsValid(),      "Invalid texture handle");

        VkCommandBuffer cmd = m_CommandBuffers[m_CurrentFrame];
        Buffer&  vb  = m_Buffers[vertexBuffer.index];
        Buffer&  ib  = m_Buffers[indexBuffer.index];
        Texture& tex = m_Textures[texture.index];

        // Update and bind the shared descriptor set
        VkDescriptorBufferInfo transformInfo{};
        transformInfo.buffer = m_UniformBuffers[m_CurrentFrame];
        transformInfo.offset = 0;
        transformInfo.range  = sizeof(GlobalUBO);

        VkDescriptorImageInfo imageInfo{};
        imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        imageInfo.imageView   = tex.view;
        imageInfo.sampler     = m_Sampler;

        VkDescriptorBufferInfo lightInfo{};
        lightInfo.buffer = m_LightBuffers[m_CurrentFrame];
        lightInfo.offset = 0;
        lightInfo.range  = sizeof(LightUBO);

        VkDescriptorBufferInfo skinningInfo{};
        skinningInfo.buffer = m_SkinningBuffers[m_CurrentFrame];
        skinningInfo.offset = 0;
        skinningInfo.range  = sizeof(SkinningUBO);

        VkWriteDescriptorSet writes[4] = {};

        writes[0].sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writes[0].dstSet          = m_SimpleDescriptorSets[m_CurrentFrame];
        writes[0].dstBinding      = 0;
        writes[0].descriptorType  = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        writes[0].descriptorCount = 1;
        writes[0].pBufferInfo     = &transformInfo;

        writes[1].sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writes[1].dstSet          = m_SimpleDescriptorSets[m_CurrentFrame];
        writes[1].dstBinding      = 1;
        writes[1].descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        writes[1].descriptorCount = 1;
        writes[1].pImageInfo      = &imageInfo;

        writes[2].sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writes[2].dstSet          = m_SimpleDescriptorSets[m_CurrentFrame];
        writes[2].dstBinding      = 2;
        writes[2].descriptorType  = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        writes[2].descriptorCount = 1;
        writes[2].pBufferInfo     = &lightInfo;

        writes[3].sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writes[3].dstSet          = m_SimpleDescriptorSets[m_CurrentFrame];
        writes[3].dstBinding      = 3;
        writes[3].descriptorType  = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        writes[3].descriptorCount = 1;
        writes[3].pBufferInfo     = &skinningInfo;


        vkUpdateDescriptorSets(m_Device, 4, writes, 0, nullptr);

        vkCmdBindDescriptorSets(
            cmd,
            VK_PIPELINE_BIND_POINT_GRAPHICS,
            m_PipelineLayout,
            0, 1,
            &m_SimpleDescriptorSets[m_CurrentFrame],
            0, nullptr);

        VkDeviceSize offsets[] = { 0 };
        vkCmdBindVertexBuffers(cmd, 0, 1, &vb.buffer, offsets);
        vkCmdBindIndexBuffer(cmd, ib.buffer, 0, VK_INDEX_TYPE_UINT32);
        vkCmdDrawIndexed(cmd, indexCount, 1, 0, 0, 0);
    }

    void VulkanBackend::DrawPrimitive(const MeshPrimitive& primitive)
    {
        LX_ASSERT(primitive.vertexBuffer.IsValid(), "Invalid vertex buffer");
        LX_ASSERT(primitive.indexBuffer.IsValid(),  "Invalid index buffer");

        VkCommandBuffer cmd = m_CommandBuffers[m_CurrentFrame];
        Buffer& vb = m_Buffers[primitive.vertexBuffer.index];
        Buffer& ib = m_Buffers[primitive.indexBuffer.index];

        // Bind this primitive's own descriptor set — already written, no update needed
        vkCmdBindDescriptorSets(
            cmd,
            VK_PIPELINE_BIND_POINT_GRAPHICS,
            m_PipelineLayout,
            0, 1,
            &primitive.descriptorSets[m_CurrentFrame],
            0, nullptr);

        VkDeviceSize offsets[] = { 0 };
        vkCmdBindVertexBuffers(cmd, 0, 1, &vb.buffer, offsets);
        vkCmdBindIndexBuffer(cmd, ib.buffer, 0, VK_INDEX_TYPE_UINT32);
        vkCmdDrawIndexed(cmd, primitive.indexCount, 1, 0, 0, 0);
    }

    void VulkanBackend::AllocatePrimitiveDescriptors(MeshPrimitive& primitive)
    {
        LX_ASSERT(primitive.texture.IsValid(), "Primitive has no texture");

        Texture& tex = m_Textures[primitive.texture.index];

        // Allocate one descriptor set per frame in flight
        VkDescriptorSetLayout layouts[MAX_SWAPCHAIN_IMAGES];
        for (u32 i = 0; i < m_SwapchainImageCount; i++)
            layouts[i] = m_DescriptorSetLayout;

        VkDescriptorSetAllocateInfo allocInfo{};
        allocInfo.sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        allocInfo.descriptorPool     = m_DescriptorPool;
        allocInfo.descriptorSetCount = m_SwapchainImageCount;
        allocInfo.pSetLayouts        = layouts;

        VkResult result = vkAllocateDescriptorSets(
            m_Device, &allocInfo, primitive.descriptorSets);
        LX_ASSERT(result == VK_SUCCESS, "Failed to allocate primitive descriptor sets");

        // Write descriptor sets once — they never change for this primitive
        for (u32 i = 0; i < m_SwapchainImageCount; i++)
        {
            VkDescriptorBufferInfo transformInfo{};
            transformInfo.buffer = m_UniformBuffers[i];
            transformInfo.offset = 0;
            transformInfo.range  = sizeof(GlobalUBO);

            VkDescriptorImageInfo imageInfo{};
            imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            imageInfo.imageView   = tex.view;
            imageInfo.sampler     = m_Sampler;

            VkDescriptorBufferInfo lightInfo{};
            lightInfo.buffer = m_LightBuffers[i];
            lightInfo.offset = 0;
            lightInfo.range  = sizeof(LightUBO);

            VkDescriptorBufferInfo skinningInfo{};
            skinningInfo.buffer = m_SkinningBuffers[i];
            skinningInfo.offset = 0;
            skinningInfo.range  = sizeof(SkinningUBO);

            VkWriteDescriptorSet writes[4] = {};

            writes[0].sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            writes[0].dstSet          = primitive.descriptorSets[i];
            writes[0].dstBinding      = 0;
            writes[0].descriptorType  = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            writes[0].descriptorCount = 1;
            writes[0].pBufferInfo     = &transformInfo;

            writes[1].sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            writes[1].dstSet          = primitive.descriptorSets[i];
            writes[1].dstBinding      = 1;
            writes[1].descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            writes[1].descriptorCount = 1;
            writes[1].pImageInfo      = &imageInfo;

            writes[2].sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            writes[2].dstSet          = primitive.descriptorSets[i];
            writes[2].dstBinding      = 2;
            writes[2].descriptorType  = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            writes[2].descriptorCount = 1;
            writes[2].pBufferInfo     = &lightInfo;

            writes[3].sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            writes[3].dstSet          = primitive.descriptorSets[i];
            writes[3].dstBinding      = 3;
            writes[3].descriptorType  = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            writes[3].descriptorCount = 1;
            writes[3].pBufferInfo     = &skinningInfo;


            vkUpdateDescriptorSets(m_Device, 4, writes, 0, nullptr);
        }
    }

    void VulkanBackend::SetCamera(const glm::mat4& view, const glm::mat4& projection)
    {
        m_View = view;
        m_Projection = projection;
    }

    void VulkanBackend::SetModelTransform(const glm::mat4& model)
    {
        m_Model = model;
    }

    void VulkanBackend::UploadBoneMatrices(const glm::mat4* matrices, u32 count)
    {
        LX_ASSERT(matrices != nullptr, "Bone matrices is null");
        LX_ASSERT(count <= MAX_BONES, "Too many bones");

        usize copySize = sizeof(glm::mat4) * count;
        ::memcpy(m_SkinningMapped[m_CurrentFrame], matrices, copySize);
    }

    void VulkanBackend::BeginFrame()
    {
        vkWaitForFences(m_Device, 1, &m_InFlightFences[m_CurrentFrame], VK_TRUE, UINT32_MAX);
        VkResult result = vkAcquireNextImageKHR(
            m_Device,
            m_Swapchain,
            UINT64_MAX,
            m_ImageAvailableSemaphores[m_CurrentFrame],
            VK_NULL_HANDLE,
            &m_CurrentImageIndex);

        LX_ASSERT(result == VK_SUCCESS, "Failed to acquire swapchain image");

        vkResetFences(m_Device, 1, &m_InFlightFences[m_CurrentFrame]);

        VkCommandBuffer cmd = m_CommandBuffers[m_CurrentFrame];

        vkResetCommandBuffer(cmd, 0);

        VkCommandBufferBeginInfo beginInfo{};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

        result = vkBeginCommandBuffer(cmd, &beginInfo);
        LX_ASSERT(result == VK_SUCCESS, "Failed to begin command buffer");

        VkClearValue clearValues[2];
        clearValues[0].color = {{ 0.1f, 0.1f, 0.1f, 1.0f }}; // dark grey
        clearValues[1].depthStencil = {1.0f, 0};

        VkRenderPassBeginInfo renderPassInfo{};
        renderPassInfo.sType             = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        renderPassInfo.renderPass        = m_RenderPass;
        renderPassInfo.framebuffer       = m_Framebuffers[m_CurrentImageIndex];
        renderPassInfo.renderArea.offset = {0, 0};
        renderPassInfo.renderArea.extent = m_SwapchainExtent;
        renderPassInfo.clearValueCount   = 2;
        renderPassInfo.pClearValues      = clearValues;

        vkCmdBeginRenderPass(cmd, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

        //Bind the pipeline
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_Pipeline);

        UpdateUniformBuffer(m_CurrentFrame);

    }

    void VulkanBackend::EndFrame()
    {
        VkCommandBuffer cmd = m_CommandBuffers[m_CurrentFrame];

        vkCmdEndRenderPass(cmd);

        VkResult result = vkEndCommandBuffer(cmd);
        LX_ASSERT(result == VK_SUCCESS, "Failed to end command buffer");

        VkPipelineStageFlags waitStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

        VkSubmitInfo submitInfo{};
        submitInfo.sType                = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submitInfo.waitSemaphoreCount   = 1;
        submitInfo.pWaitSemaphores      = &m_ImageAvailableSemaphores[m_CurrentFrame];
        submitInfo.pWaitDstStageMask    = &waitStage;
        submitInfo.commandBufferCount   = 1;
        submitInfo.pCommandBuffers      = &cmd;
        submitInfo.signalSemaphoreCount = 1;
        submitInfo.pSignalSemaphores    = &m_RenderFinishedSemaphores[m_CurrentFrame];

        result = vkQueueSubmit(
            m_GraphicsQueue, 1, &submitInfo, m_InFlightFences[m_CurrentFrame]);
        LX_ASSERT(result == VK_SUCCESS, "Failed to submit command buffer");

        VkPresentInfoKHR presentInfo{};
        presentInfo.sType              = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
        presentInfo.waitSemaphoreCount = 1;
        presentInfo.pWaitSemaphores    = &m_RenderFinishedSemaphores[m_CurrentFrame];
        presentInfo.swapchainCount     = 1;
        presentInfo.pSwapchains        = &m_Swapchain;
        presentInfo.pImageIndices      = &m_CurrentImageIndex;

        result = vkQueuePresentKHR(m_GraphicsQueue, &presentInfo);
        LX_ASSERT(result == VK_SUCCESS, "Failed to present swapchain image");

        m_CurrentFrame = (m_CurrentFrame + 1) % m_SwapchainImageCount;
    }
}
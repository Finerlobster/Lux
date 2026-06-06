
#pragma once
#include "Core/IRendererBackend.h"
#include "Core/Buffer.h"
#include "Core/UniformBuffer.h"
#include <volk.h>
#include <vk_mem_alloc.h>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

namespace LX {

    static constexpr u32 MAX_SWAPCHAIN_IMAGES = 3;

    class VulkanBackend : public IRendererBackend {
    public:
        bool Init(const WindowHandle& window)         override;
        void Shutdown()     override;
        void BeginFrame()   override;
        void EndFrame()     override;
        BufferHandle CreateVertexBuffer(const void* data, usize size) override;
        void DestroyBuffer(BufferHandle handle) override;
        void DrawVertexBuffer(BufferHandle handle, u32 vertexCount) override;

    private:
        bool InitInstance();
        bool SelectPhysicalDevice();
        bool InitDevice();
        bool InitSurface(const WindowHandle& window);
        bool InitAllocator();
        bool InitSwapchain();
        bool InitRenderPass();
        bool InitFramebuffers();
        bool InitCommands();
        bool InitSyncObjects();
        bool InitPipeline();
        bool InitUniformBuffers();
        bool InitDescriptors();
        void UpdateUniformBuffer(u32 frameIndex);

        //Internal helper - does the actual staging and copy work
        BufferHandle CreateBuffer(const void* data, usize size, VkBufferUsageFlags usage);

        //Instance
        VkInstance m_Instance               = VK_NULL_HANDLE;
        VkPhysicalDevice m_PhysicalDevice   = VK_NULL_HANDLE;
        VkDevice m_Device                   = VK_NULL_HANDLE;
        VkQueue m_GraphicsQueue             = VK_NULL_HANDLE;
        VkSurfaceKHR m_Surface              = VK_NULL_HANDLE;
        u32 m_GraphicsQueueFamily           = UINT32_MAX;

        //Swapchain
        VkSwapchainKHR m_Swapchain                                  = VK_NULL_HANDLE;
        VkFormat m_SwapchainFormat                                  = VK_FORMAT_UNDEFINED;
        VkExtent2D      m_SwapchainExtent                           = {0, 0};
        VkImage         m_SwapchainImages[MAX_SWAPCHAIN_IMAGES]     = {};
        VkImageView     m_SwapchainImageViews[MAX_SWAPCHAIN_IMAGES] = {};
        u32             m_SwapchainImageCount                       = 0;

        //RenderPass
        VkRenderPass m_RenderPass = VK_NULL_HANDLE;

        //Framebuffers
        VkFramebuffer m_Framebuffers[MAX_SWAPCHAIN_IMAGES] = {};

        //Commands
        VkCommandPool m_CommandPool = VK_NULL_HANDLE;
        VkCommandBuffer m_CommandBuffers[MAX_SWAPCHAIN_IMAGES] = {};

        //Synchronization
        VkSemaphore m_ImageAvailableSemaphores[MAX_SWAPCHAIN_IMAGES] = {};
        VkSemaphore m_RenderFinishedSemaphores[MAX_SWAPCHAIN_IMAGES] = {};
        VkFence     m_InFlightFences[MAX_SWAPCHAIN_IMAGES]           = {};
        u32         m_CurrentFrame                                   = 0;
        u32 m_CurrentImageIndex = 0;

        //Pipeline
        VkPipelineLayout m_PipelineLayout = VK_NULL_HANDLE;
        VkPipeline m_Pipeline = VK_NULL_HANDLE;

        //VMA
        VmaAllocator m_Allocator = VK_NULL_HANDLE;

        //Buffer pool
        Buffer m_Buffers[MAX_BUFFERS] = {};

        //Uniform Buffers
        VkBuffer m_UniformBuffers[MAX_SWAPCHAIN_IMAGES] = {};
        VmaAllocation m_UniformAllocations[MAX_SWAPCHAIN_IMAGES] = {};
        void* m_UniformMapped[MAX_SWAPCHAIN_IMAGES] = {};

        //Descriptors
        VkDescriptorSetLayout m_DescriptorSetLayout = VK_NULL_HANDLE;
        VkDescriptorPool m_DescriptorPool = VK_NULL_HANDLE;
        VkDescriptorSet m_DescriptorSets[MAX_SWAPCHAIN_IMAGES] = {};


        #if defined(LX_DEBUG)
            VkDebugUtilsMessengerEXT m_DebugMessenger = VK_NULL_HANDLE;
        #endif
    };
}
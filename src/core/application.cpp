// Copyright (c) 2024, Evangelion Manuhutu

#include "application.hpp"

#include <imgui/imgui.h>
#include <imgui/backends/imgui_impl_sdl3.h>
#include <imgui/backends/imgui_impl_vulkan.h>
#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>

#include "vulkan/buffers.hpp"
#include "vulkan/command_buffer.hpp"
#include "vulkan/graphics_pipeline.hpp"
#include "vulkan/shader.hpp"
#include "vulkan/texture.hpp"

#include "vulkan/vulkan_context.hpp"
#include "vulkan/vulkan_wrapper.hpp"

#include <vector>
#include <cstddef>
#include <thread>

Application::Application(i32 argc, char **argv)
{
    m_Window = CreateScope<Window>(1024, 720, "Vulkan Engine");

    m_Window->set_window_resize_callback([this](uint32_t width, uint32_t height) {
        on_window_resize(width, height);
    });
    m_Window->set_framebuffer_resize_callback([this](uint32_t width, uint32_t height) {
        on_framebuffer_resize(width, height);
    });

    m_Vk = m_Window->get_vk_context();

    m_CommandBuffer = CommandBuffer::create();

    const glm::vec2 size = { static_cast<float>(m_Window->get_window_width()), static_cast<float>(m_Window->get_window_height())};
    m_Camera = Camera(45.0f, size.x, size.y);
    m_Camera.set_position(glm::vec3(0.0f, 0.0f, 5.0f)).update_view_matrix();

    m_Texture = Texture2D::create("res/textures/brick.jpg");

    create_graphics_pipeline();
}

Application::~Application()
{
    const VkDevice device = m_Vk->get_device();
    m_Vk->get_queue()->wait_idle();

    if (m_Texture)
    {
        m_Texture->destroy();
    }

    if (m_Pipeline)
    {
        m_Pipeline->destroy();
    }

    if (m_VertexBuffer)
    {
        m_VertexBuffer->destroy();
    }

    if (m_IndexBuffer)
    {
        m_IndexBuffer->destroy();
    }

    if (m_UniformBuffer)
    {
        m_UniformBuffer->destroy();
    }

    if (m_CommandBuffer)
    {
        m_CommandBuffer->destroy();
    }

    imgui_shutdown();
}

void Application::run()
{
    imgui_init();

    std::thread render_thread = std::thread([&]()
    {
        while (m_Window->is_looping())
        {            
            if (auto frame_index = m_Vk->begin_frame())
            {
                imgui_begin();
                ImGui::ShowDemoWindow();
                ImGui::Begin("Settings");
                ImGui::ColorEdit4("clear color", &m_ClearColor[0]);
                ImGui::End();
                imgui_end();

                VkFramebuffer framebuffer = m_Vk->get_framebuffer(*frame_index);
                record_frame(framebuffer, *frame_index);

                m_Vk->present();
            }
        }
    });

    Uint64 prev_counter = SDL_GetPerformanceCounter();
    float freq = static_cast<float>(SDL_GetPerformanceFrequency());
    float title_update_interval = 0.0f;

    SDL_Event event;
    while (m_Window->is_looping())
    {
        // Poll all SDL events - this includes events from secondary ImGui viewport windows
        while (SDL_PollEvent(&event))
        {
            // Always pass ALL events to ImGui first (important for multi-viewport support)
            ImGui_ImplSDL3_ProcessEvent(&event);
            m_Window->poll_events(&event);            
        }

        const Uint64 current_count = SDL_GetPerformanceCounter();
        const float delta_time = static_cast<float>(current_count - prev_counter) / freq;
        prev_counter = current_count;

        title_update_interval -= delta_time;
        if (title_update_interval <= 0.0f && delta_time > 0.0f)
        {
            const int fps = static_cast<int>(1.0f / delta_time);
            // m_Window->set_title();
            // SDL_Log("%d FPS - %.2f ms", fps, delta_time * 1000.0f);// std::format("Vulkan Engine - {} FPS - {} ms", fps, delta_time).c_str();

            title_update_interval = 0.3f;
        }
        
        on_update(delta_time);
    }

    render_thread.join();
}

void Application::on_update(float delta_time)
{
    m_Camera.update_view_matrix();

    static float y_rot = 0.0f;
    y_rot += delta_time;

    m_UboData.transform = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f))
        * glm::rotate(glm::mat4(1.0f), glm::radians(y_rot * 40.0f), glm::vec3(1.0f, 0.0f, 0.0f))
        * glm::rotate(glm::mat4(1.0f), glm::radians(y_rot * 25.0f), glm::vec3(0.0f, 1.0f, 0.0f))
        * glm::rotate(glm::mat4(1.0f), glm::radians(y_rot * 14.0f), glm::vec3(0.0f, 0.0f, 1.0f))
        * glm::scale(glm::mat4(1.0f), glm::vec3((glm::sin(y_rot) + 1.0f) * 0.1f) + glm::vec3(1.0f));

    m_UboData.viewProjection = m_Camera.get_view_projection_matrix();
}

void Application::on_window_resize(uint32_t width, uint32_t height)
{
}

void Application::on_framebuffer_resize(uint32_t width, uint32_t height)
{
    const auto w = static_cast<float>(width);
    const auto h = static_cast<float>(height);
    m_Camera.resize({w, h}).update_projection_matrix();
}

void Application::create_graphics_pipeline()
{
    auto const device = m_Vk->get_device();

    const Ref<Shader> vertex_shader = CreateRef<Shader>("res/shaders/default.vert", VK_SHADER_STAGE_VERTEX_BIT);
    const Ref<Shader> fragment_shader = CreateRef<Shader>("res/shaders/default.frag", VK_SHADER_STAGE_FRAGMENT_BIT);

    std::vector<Vertex> vertices =
    {
        // Position, Color (clockwise winding)
        {{ -0.5f, -0.5f, 0.0f }, {1.0f, 1.0f, 1.0f}, { 0.0f, 0.0f }},
        {{ -0.5f,  0.5f, 0.0f }, {1.0f, 1.0f, 1.0f}, { 0.0f, 1.0f }},
        {{  0.5f,  0.5f, 0.0f }, {1.0f, 1.0f, 1.0f}, { 1.0f, 1.0f }},
        {{  0.5f, -0.5f, 0.0f }, {1.0f, 1.0f, 1.0f}, { 1.0f, 0.0f }},
    };

    std::vector<uint32_t> indices = 
    {
        0, 1, 2,
        0, 2, 3
    };

    VkDeviceSize buffer_size = sizeof(vertices[0]) * vertices.size();
    m_VertexBuffer = VertexBuffer::create(vertices.data(), buffer_size);
    m_IndexBuffer = IndexBuffer::create(indices);

    // Build vertex input state from reflection if available; fallback to hardcoded Vertex layout
    VkVertexInputBindingDescription binding_desc = {};
    std::vector<VkVertexInputAttributeDescription> attr_desc;
    const auto& reflected_attrs = vertex_shader->get_vertex_attributes();
    ASSERT(reflected_attrs.size() <= 255, "Too many vertex attributes");
    ASSERT(reflected_attrs.empty() == false, "Attributes are empty");
    binding_desc.binding = 0;
    binding_desc.stride = vertex_shader->get_vertex_stride();
    binding_desc.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
    attr_desc = reflected_attrs; // copy

    GraphicsPipelineInfo pipeline_info = {};
    pipeline_info.binding_description = binding_desc;
    pipeline_info.attribute_descriptions = attr_desc;  // This copies the vector
    pipeline_info.extent = VulkanContext::get()->get_swap_chain()->get_extent();
    pipeline_info.render_pass = m_Vk->get_render_pass();

    m_Pipeline = CreateRef<GraphicsPipeline>();
    m_Pipeline->set_shaders({vertex_shader, fragment_shader})
        .build(pipeline_info);

    m_UniformBuffer = UniformBuffer::create(sizeof(UniformBufferData));
    m_UniformBuffer->bind_memory();

    auto descriptor_layouts = m_Pipeline->get_descriptor_set_layouts();

    VkDescriptorPool descriptor_pool = VulkanContext::get()->get_descriptor_pool();
    VkDescriptorSetAllocateInfo descriptor_alloc_info = {};
    descriptor_alloc_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    descriptor_alloc_info.descriptorPool = descriptor_pool;
    descriptor_alloc_info.descriptorSetCount = static_cast<uint32_t>(descriptor_layouts.size());
    descriptor_alloc_info.pSetLayouts = descriptor_layouts.data();

    VkResult result = vkAllocateDescriptorSets(device, &descriptor_alloc_info, &m_DescriptorSet);
    VK_ERROR_CHECK(result, "[Vulkan] Failed to allocate descriptor set");

    // Uniform buffer info
    VkDescriptorBufferInfo ubo_info = {};
    ubo_info.buffer = m_UniformBuffer->get_buffer();
    ubo_info.offset = 0;
    ubo_info.range = sizeof(UniformBufferData);

    VkWriteDescriptorSet ubo_write = {};
    ubo_write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    ubo_write.pNext = VK_NULL_HANDLE;
    ubo_write.dstSet = m_DescriptorSet;
    ubo_write.dstBinding = 0;
    ubo_write.dstArrayElement = 0;
    ubo_write.descriptorCount = 1;
    ubo_write.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    ubo_write.pBufferInfo = &ubo_info;

    // Image Info
    VkDescriptorImageInfo image_info = {};
    image_info.sampler = m_Texture->get_sampler(),
    image_info.imageView = m_Texture->get_image_view(),
    image_info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    VkWriteDescriptorSet image_write = {};
    image_write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    image_write.pNext = VK_NULL_HANDLE;
    image_write.dstSet = m_DescriptorSet;
    image_write.dstBinding = 1;
    image_write.dstArrayElement = 0;
    image_write.descriptorCount = 1;
    image_write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    image_write.pImageInfo = &image_info;

    std::vector writes = {ubo_write, image_write};
    vkUpdateDescriptorSets(device, static_cast<uint32_t>(writes.size()),
        writes.data(), 0, nullptr);
}

void Application::record_frame(VkFramebuffer framebuffer, uint32_t frame_index)
{
    auto const device = m_Vk->get_device();
    const VkExtent2D extent = m_Vk->get_swap_chain()->get_extent();

    VkViewport viewport{};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = static_cast<float>(extent.width);
    viewport.height = static_cast<float>(extent.height);
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;

    VkRect2D scissor{};
    scissor.offset = {0, 0};
    scissor.extent = extent;

    VkClearValue clear_value{};
    clear_value.color.float32[0] = m_ClearColor.r;
    clear_value.color.float32[1] = m_ClearColor.g;
    clear_value.color.float32[2] = m_ClearColor.b;
    clear_value.color.float32[3] = m_ClearColor.a;

    m_CommandBuffer->begin(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
    VkCommandBuffer command_buffer = m_CommandBuffer->get_active_handle();

    // const glm::mat4 &view_projection = m_Camera.get_view_projection_matrix();
    // m_CommandBuffer->set_push_constants(VK_SHADER_STAGE_VERTEX_BIT, m_Pipeline->get_layout(), &view_projection, sizeof(glm::mat4));

    m_UniformBuffer->set_data(&m_UboData, sizeof(m_UboData));
    
    GraphicsState state;
    state.pipeline = m_Pipeline->get_handle();
    state.pipeline_layout = m_Pipeline->get_layout();
    state.framebuffer = framebuffer;
    state.render_pass = m_Vk->get_render_pass();
    state.scissor = scissor;
    state.viewport = viewport;
    state.clear_value = clear_value;
    state.descriptor_sets = { m_DescriptorSet };
    state.index_buffer = { m_IndexBuffer->get_buffer(), 0, VK_INDEX_TYPE_UINT32 };
    state.vertex_buffers = { m_VertexBuffer->get_buffer() };
    
    m_CommandBuffer->set_graphics_state(state);

    DrawArguments args;
    args.vertex_count = m_IndexBuffer->get_count();
    args.instance_count = 1;
    m_CommandBuffer->draw_indexed(args);

    if (ImDrawData* draw_data = ImGui::GetDrawData())
    {
        ImGui_ImplVulkan_RenderDrawData(draw_data, command_buffer);
    }

    m_CommandBuffer->end();
    m_Vk->submit({command_buffer});
}

void Application::imgui_init()
{
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;         // Enable Docking
    // io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;       // Enable Multi-Viewport / Platform Windows
    //io.ConfigFlags |= ImGuiConfigFlags_ViewportsNoTaskBarIcons;
    //io.ConfigFlags |= ImGuiConfigFlags_ViewportsNoMerge;

    ImGui::StyleColorsDark();

    // When viewports are enabled, tweak WindowRounding/WindowBg for platform windows
    ImGuiStyle& style = ImGui::GetStyle();
    if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
    {
        style.WindowRounding = 0.0f;
        style.Colors[ImGuiCol_WindowBg].w = 1.0f;
    }

    // setup renderer backends
    ImGui_ImplSDL3_InitForVulkan(m_Window->get_native_window());
    ImGui_ImplVulkan_InitInfo init_info = {};
    init_info.Instance = m_Vk->get_instance();
    init_info.PhysicalDevice = m_Vk->get_physical_device();
    init_info.Device = m_Vk->get_device();
    init_info.QueueFamily = m_Vk->get_queue_family();
    init_info.Queue = m_Vk->get_queue()->get_handle();
    init_info.PipelineCache = VK_NULL_HANDLE;
    init_info.DescriptorPool = m_Vk->get_descriptor_pool();
    init_info.MinImageCount = m_Vk->get_swap_chain()->get_min_image_count();
    init_info.ImageCount = m_Vk->get_swap_chain()->get_image_count();
    init_info.PipelineInfoMain.RenderPass = m_Vk->get_render_pass();
    init_info.PipelineInfoMain.Subpass = 0;
    init_info.PipelineInfoMain.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
    init_info.Allocator = VK_NULL_HANDLE;
    init_info.CheckVkResultFn = VK_NULL_HANDLE;
    ImGui_ImplVulkan_Init(&init_info);
}

void Application::imgui_begin()
{
    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplSDL3_NewFrame();
    ImGui::NewFrame();

    static ImGuiDockNodeFlags doc_space_flags = ImGuiDockNodeFlags_None | ImGuiDockNodeFlags_PassthruCentralNode;
    ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove
        | ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;

    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->WorkPos);
    ImGui::SetNextWindowSize(viewport->WorkSize);
    ImGui::SetNextWindowViewport(viewport->ID);

    if (doc_space_flags & ImGuiDockNodeFlags_PassthruCentralNode)
    {
        window_flags |= ImGuiWindowFlags_NoBackground;
    }

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(1, 2));
    ImGui::Begin("Vulkan", nullptr, window_flags);

    ImGuiStyle& style = ImGui::GetStyle();
    const float min_window_size_x = style.WindowMinSize.x;
    const float min_window_size_y = style.WindowMinSize.y;
    style.WindowMinSize.x = 220.0f;
    style.WindowMinSize.y = 38.0f;
    if (ImGui::GetIO().ConfigFlags & ImGuiConfigFlags_DockingEnable)
    {
        const ImGuiID dock_space_id = ImGui::GetID("MyDockSpace");
        ImGui::DockSpace(dock_space_id, ImVec2(0.0f, 0.0f), doc_space_flags);
    }
    style.WindowMinSize.x = min_window_size_x;
    style.WindowMinSize.y = min_window_size_y;
}

void Application::imgui_end()
{
    // docking window
    ImGui::End();
    ImGui::PopStyleVar();

    ImGui::Render();

    if (ImGui::GetIO().ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
    {
        ImGui::UpdatePlatformWindows();
        ImGui::RenderPlatformWindowsDefault();
    }
}

void Application::imgui_shutdown() const
{
    ImGui_ImplVulkan_Shutdown();
    ImGui_ImplSDL3_Shutdown();
    ImGui::DestroyContext();
}


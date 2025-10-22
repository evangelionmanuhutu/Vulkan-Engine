// Copyright (c) 2025, Evangelion Manuhutu

#ifndef APPLICATION_HPP
#define APPLICATION_HPP

#include "window.hpp"
#include "camera.hpp"

#include <memory>

class VulkanContext;
class CommandBuffer;
class GraphicsPipeline;
class VertexBuffer;
class IndexBuffer;
class UniformBuffer;
class Shader;
class Texture2D;

struct UniformBufferData
{
    glm::mat4 viewProjection;
    glm::mat4 transform;
};

class Application {
public:
    Application(i32 argc, char **argv);
    ~Application();

    void run();

private:
    void on_update(float delta_time);

    void on_window_resize(uint32_t width, uint32_t height);
    void on_framebuffer_resize(uint32_t width, uint32_t height);

    void create_graphics_pipeline();
    void record_frame(VkFramebuffer framebuffer, uint32_t frame_index);

    void imgui_init();
    void imgui_begin();
    void imgui_end();
    void imgui_shutdown() const;

    Ref<GraphicsPipeline> m_Pipeline;
    Ref<VertexBuffer> m_VertexBuffer;
    Ref<IndexBuffer> m_IndexBuffer;
    Ref<UniformBuffer> m_UniformBuffer;
    Ref<Texture2D> m_Texture;
    Ref<CommandBuffer> m_CommandBuffer;
    Scope<Window> m_Window;

    UniformBufferData m_UboData;
    VkDescriptorSet m_DescriptorSet;
    Camera m_Camera;
    VulkanContext *m_Vk;
    glm::vec4 m_ClearColor = glm::vec4(0.1f, 0.1f, 0.1f, 1.0f);
};

#endif //APPLICATION_HPP

#include "core/window.hpp"
#include "core/event.hpp"
#include "core/log.hpp"
#include "core/mesh.hpp"
#include "core/material.hpp"
#include "core/model.hpp"

#include "gfx/vulkan/context.hpp"
#include "gfx/vulkan/pipeline.hpp"
#include "gfx/vulkan/descriptor.hpp"
#include "gfx/vulkan/buffer.hpp"
#include "gfx/vulkan/image.hpp"
#include "gfx/vulkan/renderpass.hpp"
#include "gfx/vulkan/framebuffer.hpp"

#include "renderer.hpp"
#include "editor_camera.hpp"

#include <glm/glm.hpp>
#include "glm/gtx/string_cast.hpp"
#include <entt/entt.hpp>

#include <memory>
#include <iostream>
#include <chrono>

int main(int argc, char **argv) {
    auto window = std::make_shared<core::Window>("VIZON-vulkan", 800, 600);
    auto context = std::make_shared<gfx::vulkan::context_t>(window, 2, true);
    auto dispatcher = std::make_shared<event::Dispatcher>();

    core::Material::init(context);

    Renderer renderer{window, context, dispatcher};

    std::shared_ptr<entt::registry> scene = std::make_shared<entt::registry>();

    {
        auto ent = scene->create();
        auto& model = scene->emplace<core::Model>(ent, context);
        model.loadFromPath("../../assets/models/Sponza/glTF/Sponza.gltf");
    }

    auto test = gfx::vulkan::descriptor_set_layout_builder_t{}
        .build(context);

    // double lastTime = glfwGetTime();
    float targetFPS = 5000.f;

    EditorCamera editorCamera{window};

    auto lastTime = std::chrono::system_clock::now();

    while (!window->shouldClose()) {
        window->pollEvents();

        auto currentTime = std::chrono::system_clock::now();
        auto dt = currentTime - lastTime;
        if (dt.count() / 1e6 < 1000.f / targetFPS) {
            continue;
        }
        lastTime = currentTime;

        auto ms = dt.count() / float(1e6);
        
        INFO("Took {}ms, FPS is {}", dt.count() / 1e6, 1000.f / ms);
        INFO("-------------------------------------------------");

        editorCamera.onUpdate(dt.count() / 1e6);

        renderer.render(scene, editorCamera);
    }

    vkDeviceWaitIdle(context->device());

    core::Material::destroy();

    return 0;
}
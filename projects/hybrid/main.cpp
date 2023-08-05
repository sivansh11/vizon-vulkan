#include <iostream>

#include "renderer.hpp"

int main() {
    ref<window_t> window = make_ref<window_t>("hybrid", 600, 400);
    ref<context_t> context = make_ref<context_t>(window, 2, true);
    ImGui_init(window, context);
    renderer_t renderer{ window, context };

    float target_fps = 60.f;
    auto last_time = std::chrono::system_clock::now();
    while (!window->should_close()) {
        window->poll_events();

        auto current_time = std::chrono::system_clock::now();
        auto time_difference = current_time - last_time;
        if (time_difference.count() / 1e6 < 1000.f / target_fps) {
            continue;
        }
        last_time = current_time;
        float dt = time_difference.count() / float(1e6);

        if (auto start_frame = context->start_frame()) {
            auto [commandbuffer, current_index] = *start_frame;
            
            VkClearValue clear_color{};
            clear_color.color = {0, 0, 0, 0};    
            
            renderer.render(commandbuffer, current_index);

            context->begin_swapchain_renderpass(commandbuffer, clear_color);

            ImGui_newframe();
        
            ImGui::Begin("viewport");
            ImGui::Image(renderer.get_color(), ImGui::GetContentRegionAvail());
            ImGui::End();

            ImGui_endframe(commandbuffer);

            context->end_swapchain_renderpass(commandbuffer);

            context->end_frame(commandbuffer);
        }

        clear_frame_function_times();
    }
    context->wait_idle();
    ImGui_shutdown();
    return 0;
}
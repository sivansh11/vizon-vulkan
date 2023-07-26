#include "core/window.hpp"
#include "core/core.hpp"
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
#include "gfx/vulkan/timer.hpp"

#include <glm/glm.hpp>
#include "glm/gtx/string_cast.hpp"
#include <entt/entt.hpp>

#include "core/imgui_utils.hpp"

#include "editor_camera.hpp"

#include <memory>
#include <iostream>
#include <algorithm>
#include <chrono>

using namespace core;
using namespace gfx::vulkan;

struct acceleration_structure_t {
    VkAccelerationStructureKHR acceleration_structure;
    VkDeviceAddress device_address;
    ref<buffer_t> buffer;
};

int main(int argc, char **argv) {
    auto window = core::make_ref<core::Window>("test3", 600, 400);
    auto ctx = core::make_ref<gfx::vulkan::context_t>(window, 1, true, true);

    core::ImGui_init(window, ctx);

    auto [width, height] = window->getSize();

    auto image = image_builder_t{}
        .build2D(ctx,
                 width, height,
                 VK_FORMAT_R8G8B8A8_SNORM,
                 VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                 VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    image->transition_layout(VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);

    acceleration_structure_t blas;
    acceleration_structure_t tlas;
    ref<buffer_t> vertex_buffer;
    ref<buffer_t> index_buffer;
    ref<buffer_t> transform_matrix_buffer;
    ref<buffer_t> instance_buffer;
    ref<buffer_t> scratch_buffer;

    struct vertex_t {
        glm::vec3 position;
    };
    std::vector<vertex_t> vertices = {
        {{ 1.0f,  1.0f, 0.0f}},
        {{-1.0f,  1.0f, 0.0f}},
        {{ 0.0f, -1.0f, 0.0f}}
    };
    std::vector<uint32_t> indices = {0, 1, 2};

    uint32_t vertex_buffer_size = sizeof(vertex_t) * vertices.size();
    uint32_t index_buffer_size = sizeof(uint32_t) * indices.size();

    vertex_buffer = buffer_builder_t{}
        .build(ctx, 
               vertex_buffer_size,
               VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
               VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    
    index_buffer = buffer_builder_t{}
        .build(ctx,
               index_buffer_size,
               VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
               VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    
    std::memcpy(vertex_buffer->map(), vertices.data(), vertex_buffer_size);
    std::memcpy(index_buffer->map(), indices.data(), index_buffer_size);

    vertex_buffer->unmap();
    index_buffer->unmap();

    VkTransformMatrixKHR transform_matrix = {
        1.0f, 0.0f, 0.0f, 0.0f,
        0.0f, 1.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 1.0f, 0.0f,
    };

    transform_matrix_buffer = buffer_builder_t{}
        .build(ctx,
               sizeof(transform_matrix),
               VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
               VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    
    std::memcpy(transform_matrix_buffer->map(), &transform_matrix, sizeof(transform_matrix));

    transform_matrix_buffer->unmap();

    std::vector<VkAccelerationStructureGeometryKHR> blas_geometries{};

    VkAccelerationStructureGeometryKHR blas_geometry{};
    blas_geometry.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
    blas_geometry.geometryType = VK_GEOMETRY_TYPE_TRIANGLES_KHR;
    blas_geometry.flags = VK_GEOMETRY_OPAQUE_BIT_KHR;
    blas_geometry.geometry.triangles.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR;
    blas_geometry.geometry.triangles.vertexFormat = VK_FORMAT_R32G32B32_SFLOAT;
    blas_geometry.geometry.triangles.vertexData.deviceAddress = vertex_buffer->device_address();
    blas_geometry.geometry.triangles.maxVertex = vertices.size();
    blas_geometry.geometry.triangles.indexType = VK_INDEX_TYPE_UINT32;
    blas_geometry.geometry.triangles.indexData.deviceAddress = index_buffer->device_address();
    blas_geometry.geometry.triangles.transformData.deviceAddress = transform_matrix_buffer->device_address();

    blas_geometries.push_back(blas_geometry);

    VkAccelerationStructureBuildGeometryInfoKHR blas_build_geometry_info{};
    blas_build_geometry_info.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
    blas_build_geometry_info.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
    blas_build_geometry_info.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;
    blas_build_geometry_info.geometryCount = blas_geometries.size();
    blas_build_geometry_info.pGeometries = blas_geometries.data();

    const uint32_t blas_primitive_count = indices.size() / 3;

    VkAccelerationStructureBuildSizesInfoKHR blas_build_size_info{};
    blas_build_size_info.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR;
    vkGetAccelerationStructureBuildSizesKHR(ctx->device(),
                                            VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
                                            &blas_build_geometry_info,
                                            &blas_primitive_count,
                                            &blas_build_size_info);
    
    blas.buffer = buffer_builder_t{}
        .build(ctx,
               blas_build_size_info.accelerationStructureSize,
               VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR,
               VK_MEMORY_HEAP_DEVICE_LOCAL_BIT);
    
    VkAccelerationStructureCreateInfoKHR blas_create_info{};
    blas_create_info.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR;
    blas_create_info.buffer = blas.buffer->buffer();
    blas_create_info.size = blas_build_size_info.accelerationStructureSize;
    blas_create_info.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;

    vkCreateAccelerationStructureKHR(ctx->device(),
                                     &blas_create_info,
                                     nullptr,
                                     &blas.acceleration_structure);
    
    scratch_buffer = buffer_builder_t{}
        .build(ctx,
               blas_build_size_info.buildScratchSize,
               VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
               VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    
    blas_build_geometry_info.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
    blas_build_geometry_info.dstAccelerationStructure = blas.acceleration_structure;
    blas_build_geometry_info.scratchData.deviceAddress = scratch_buffer->device_address();

    VkAccelerationStructureBuildRangeInfoKHR blas_build_range_info{};
    blas_build_range_info.primitiveCount = blas_primitive_count;
    blas_build_range_info.primitiveOffset = 0;
    blas_build_range_info.firstVertex = 0;
    blas_build_range_info.transformOffset = 0;

    std::vector<VkAccelerationStructureBuildRangeInfoKHR *> blas_build_range_infos = { &blas_build_range_info };

    ctx->single_use_command_buffer([&](VkCommandBuffer command_buffer) {
        vkCmdBuildAccelerationStructuresKHR(command_buffer,
                                            blas_build_range_infos.size(),
                                            &blas_build_geometry_info,
                                            blas_build_range_infos.data());
    });

    VkAccelerationStructureDeviceAddressInfoKHR blas_device_address_info{};
    blas_device_address_info.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR;
    blas_device_address_info.accelerationStructure = blas.acceleration_structure;
    blas.device_address = vkGetAccelerationStructureDeviceAddressKHR(ctx->device(),
                                                                     &blas_device_address_info);

    transform_matrix = {
        1.0f, 0.0f, 0.0f, 0.0f,
        0.0f, 1.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 1.0f, 0.0f,
    };

    VkAccelerationStructureInstanceKHR tlas_instance{};
    tlas_instance.transform = transform_matrix;
    tlas_instance.instanceCustomIndex = 0;
    tlas_instance.mask = 0xFF;
    tlas_instance.instanceShaderBindingTableRecordOffset = 0;
    tlas_instance.flags = VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR;
    tlas_instance.accelerationStructureReference = blas.device_address;

    instance_buffer = buffer_builder_t{}
        .build(ctx,
               sizeof(VkAccelerationStructureInstanceKHR),
               VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
               VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    
    std::memcpy(instance_buffer->map(), &tlas_instance, sizeof(tlas_instance));
    instance_buffer->unmap();

    VkAccelerationStructureGeometryKHR tlas_geometry{};
    tlas_geometry.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
    tlas_geometry.geometryType = VK_GEOMETRY_TYPE_INSTANCES_KHR;
    tlas_geometry.flags = VK_GEOMETRY_OPAQUE_BIT_KHR;
    tlas_geometry.geometry.instances.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR;
    tlas_geometry.geometry.instances.arrayOfPointers = VK_FALSE;
    tlas_geometry.geometry.instances.data.deviceAddress = instance_buffer->device_address();

    std::vector<VkAccelerationStructureGeometryKHR> tlas_geometries = { tlas_geometry };

    VkAccelerationStructureBuildGeometryInfoKHR tlas_build_geometry_info{};
    tlas_build_geometry_info.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
    tlas_build_geometry_info.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
    tlas_build_geometry_info.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;
    tlas_build_geometry_info.geometryCount = tlas_geometries.size();
    tlas_build_geometry_info.pGeometries = tlas_geometries.data();

    const uint32_t tlas_primitive_count = 1;  // I donno what/why is this number here ?

    VkAccelerationStructureBuildSizesInfoKHR tlas_build_size_info{};
    tlas_build_size_info.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR;
    
    vkGetAccelerationStructureBuildSizesKHR(ctx->device(),
                                            VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
                                            &tlas_build_geometry_info,
                                            &tlas_primitive_count,
                                            &tlas_build_size_info);

    tlas.buffer = buffer_builder_t{}   
        .build(ctx,
               tlas_build_size_info.accelerationStructureSize,
               VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR,
               VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    
    VkAccelerationStructureCreateInfoKHR tlas_create_info{};
    tlas_create_info.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR;
    tlas_create_info.buffer = tlas.buffer->buffer();
    tlas_create_info.size = tlas_build_size_info.accelerationStructureSize;
    tlas_create_info.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
    
    vkCreateAccelerationStructureKHR(ctx->device(),
                                     &tlas_create_info,
                                     nullptr,
                                     &tlas.acceleration_structure);
                    
    scratch_buffer = buffer_builder_t{}
        .build(ctx,
               tlas_build_size_info.buildScratchSize,
               VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
               VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    
    tlas_build_geometry_info.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
    tlas_build_geometry_info.dstAccelerationStructure = tlas.acceleration_structure;
    tlas_build_geometry_info.scratchData.deviceAddress = scratch_buffer->device_address();

    VkAccelerationStructureBuildRangeInfoKHR tlas_build_range_info{};
    tlas_build_range_info.primitiveCount = tlas_primitive_count;   
    tlas_build_range_info.primitiveOffset = 0;
    tlas_build_range_info.firstVertex = 0;
    tlas_build_range_info.transformOffset = 0;

    std::vector<VkAccelerationStructureBuildRangeInfoKHR *> tlas_build_range_infos = { &tlas_build_range_info };

    ctx->single_use_command_buffer([&](VkCommandBuffer command_buffer) {
        vkCmdBuildAccelerationStructuresKHR(command_buffer, 
                                            1, 
                                            &tlas_build_geometry_info, 
                                            tlas_build_range_infos.data());
    });

    VkAccelerationStructureDeviceAddressInfoKHR tlas_device_address_info{};
    tlas_device_address_info.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR;
    tlas_device_address_info.accelerationStructure = tlas.acceleration_structure;
    tlas.device_address = vkGetAccelerationStructureDeviceAddressKHR(ctx->device(),
                                                                     &tlas_device_address_info);

    struct uniform_data_t {
        glm::mat4 view_inv;
        glm::mat4 projection_inv;
    } uniform_data;

    auto ubo = buffer_builder_t{}
        .build(ctx,
               sizeof(uniform_data),
               VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
               VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    auto rt_descriptor_set_layout = descriptor_set_layout_builder_t{}
        .addLayoutBinding(0, VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, 1, VK_SHADER_STAGE_RAYGEN_BIT_KHR)
        .addLayoutBinding(1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_RAYGEN_BIT_KHR)
        .addLayoutBinding(2, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_RAYGEN_BIT_KHR)
        .build(ctx);
    
    auto rt_descriptor_set = rt_descriptor_set_layout->new_descriptor_set();

    VkPipelineLayoutCreateInfo rt_pipeline_layout_create_info{};
    rt_pipeline_layout_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    rt_pipeline_layout_create_info.setLayoutCount = 1;
    rt_pipeline_layout_create_info.pSetLayouts = &rt_descriptor_set_layout->descriptor_set_layout();

    VkPipelineLayout rt_pipeline_layout;

    auto result = vkCreatePipelineLayout(ctx->device(),
                                         &rt_pipeline_layout_create_info,
                                         nullptr, &rt_pipeline_layout);
    
    if (result != VK_SUCCESS) {
        ERROR("Failed to create rt pipeline layout");
        std::terminate();
    }

    std::vector<VkPipelineShaderStageCreateInfo> pipeline_shader_stage_create_infos;
    std::vector<VkShaderModule> shader_modules;
    std::vector<VkRayTracingShaderGroupCreateInfoKHR> rt_shader_group_create_infos;

    // ray gen
    {
        VkPipelineShaderStageCreateInfo pipeline_shader_stage_create_info{};
        pipeline_shader_stage_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        pipeline_shader_stage_create_info.module = load_shader_module(ctx, "../../assets/shaders/test3/rt/shader.rgen.spv");
        pipeline_shader_stage_create_info.stage = VK_SHADER_STAGE_RAYGEN_BIT_KHR;
        pipeline_shader_stage_create_info.pName = "main";
        pipeline_shader_stage_create_infos.push_back(pipeline_shader_stage_create_info);
        shader_modules.push_back(pipeline_shader_stage_create_info.module);
        VkRayTracingShaderGroupCreateInfoKHR rgen_group{};
        rgen_group.sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR;
        rgen_group.type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR;
        rgen_group.generalShader = pipeline_shader_stage_create_infos.size() - 1;
        rgen_group.closestHitShader = VK_SHADER_UNUSED_KHR;
        rgen_group.anyHitShader = VK_SHADER_UNUSED_KHR;
        rgen_group.intersectionShader = VK_SHADER_UNUSED_KHR;
        rt_shader_group_create_infos.push_back(rgen_group);
    }
    
    // ray miss
    {
        VkPipelineShaderStageCreateInfo pipeline_shader_stage_create_info{};
        pipeline_shader_stage_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        pipeline_shader_stage_create_info.module = load_shader_module(ctx, "../../assets/shaders/test3/rt/shader.rmiss.spv");
        pipeline_shader_stage_create_info.stage = VK_SHADER_STAGE_MISS_BIT_KHR;
        pipeline_shader_stage_create_info.pName = "main";
        pipeline_shader_stage_create_infos.push_back(pipeline_shader_stage_create_info);
        shader_modules.push_back(pipeline_shader_stage_create_info.module);
        VkRayTracingShaderGroupCreateInfoKHR rmiss_group{};
        rmiss_group.sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR;
        rmiss_group.type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR;
        rmiss_group.generalShader = pipeline_shader_stage_create_infos.size() - 1;
        rmiss_group.closestHitShader = VK_SHADER_UNUSED_KHR;
        rmiss_group.anyHitShader = VK_SHADER_UNUSED_KHR;
        rmiss_group.intersectionShader = VK_SHADER_UNUSED_KHR;
        rt_shader_group_create_infos.push_back(rmiss_group);
    }

    // ray chit
    {
        VkPipelineShaderStageCreateInfo pipeline_shader_stage_create_info{};
        pipeline_shader_stage_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        pipeline_shader_stage_create_info.module = load_shader_module(ctx, "../../assets/shaders/test3/rt/shader.rchit.spv");
        pipeline_shader_stage_create_info.stage = VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;
        pipeline_shader_stage_create_info.pName = "main";
        pipeline_shader_stage_create_infos.push_back(pipeline_shader_stage_create_info);
        shader_modules.push_back(pipeline_shader_stage_create_info.module);
        VkRayTracingShaderGroupCreateInfoKHR rchit_group{};
        rchit_group.sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR;
        rchit_group.type = VK_RAY_TRACING_SHADER_GROUP_TYPE_TRIANGLES_HIT_GROUP_KHR;
        rchit_group.generalShader = VK_SHADER_UNUSED_KHR;
        rchit_group.closestHitShader = pipeline_shader_stage_create_infos.size() - 1;
        rchit_group.anyHitShader = VK_SHADER_UNUSED_KHR;
        rchit_group.intersectionShader = VK_SHADER_UNUSED_KHR;
        rt_shader_group_create_infos.push_back(rchit_group);
    }

    VkPipeline rt_pipeline;

    VkRayTracingPipelineCreateInfoKHR rt_pipeline_create_info{};
    rt_pipeline_create_info.sType = VK_STRUCTURE_TYPE_RAY_TRACING_PIPELINE_CREATE_INFO_KHR;
    rt_pipeline_create_info.stageCount = pipeline_shader_stage_create_infos.size();
    rt_pipeline_create_info.pStages = pipeline_shader_stage_create_infos.data();
    rt_pipeline_create_info.groupCount = rt_shader_group_create_infos.size();
    rt_pipeline_create_info.pGroups = rt_shader_group_create_infos.data();
    rt_pipeline_create_info.maxPipelineRayRecursionDepth = 1;
    rt_pipeline_create_info.layout = rt_pipeline_layout;
    result = vkCreateRayTracingPipelinesKHR(ctx->device(),
                                                 VK_NULL_HANDLE,
                                                 VK_NULL_HANDLE,
                                                 1,
                                                 &rt_pipeline_create_info, 
                                                 nullptr,
                                                 &rt_pipeline);

    if (result != VK_SUCCESS) {
        ERROR("Failed to create rt pipeline");
        std::terminate();
    }

    VkPhysicalDeviceRayTracingPipelinePropertiesKHR physical_device_raytracing_pipeline_properties{};
    physical_device_raytracing_pipeline_properties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_PROPERTIES_KHR;
    VkPhysicalDeviceProperties2 physical_device_properties_2{};
    physical_device_properties_2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
    physical_device_properties_2.pNext = &physical_device_raytracing_pipeline_properties;
    vkGetPhysicalDeviceProperties2(ctx->physical_device(), &physical_device_properties_2);

    uint32_t handle_size = physical_device_raytracing_pipeline_properties.shaderGroupHandleSize;
    uint32_t handle_size_aligned = aligned_size(physical_device_raytracing_pipeline_properties.shaderGroupHandleSize, physical_device_raytracing_pipeline_properties.shaderGroupHandleAlignment);
    
    auto rgen_shader_binding_table = buffer_builder_t{}
        .build(ctx,
               handle_size,
               VK_BUFFER_USAGE_SHADER_BINDING_TABLE_BIT_KHR | VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
               VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    auto rmiss_shader_binding_table = buffer_builder_t{}
        .build(ctx,
               handle_size,
               VK_BUFFER_USAGE_SHADER_BINDING_TABLE_BIT_KHR | VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
               VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    auto rchit_shader_binding_table = buffer_builder_t{}
        .build(ctx,
               handle_size,
               VK_BUFFER_USAGE_SHADER_BINDING_TABLE_BIT_KHR | VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
               VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    
    std::vector<uint8_t> shader_handle_storage(rt_shader_group_create_infos.size() * handle_size_aligned);
    vkGetRayTracingShaderGroupHandlesKHR(ctx->device(),
                                         rt_pipeline,
                                         0,
                                         rt_shader_group_create_infos.size(),
                                         rt_shader_group_create_infos.size() * handle_size_aligned,
                                         shader_handle_storage.data());

    std::memcpy(rgen_shader_binding_table->map(), shader_handle_storage.data(), handle_size);
    std::memcpy(rmiss_shader_binding_table->map(), shader_handle_storage.data() + handle_size_aligned, handle_size);
    std::memcpy(rchit_shader_binding_table->map(), shader_handle_storage.data() + handle_size_aligned * 2, handle_size);
    rgen_shader_binding_table->unmap();
    rmiss_shader_binding_table->unmap();
    rchit_shader_binding_table->unmap();

    rt_descriptor_set->write()
        .pushAccelerationStructureInfo(0, 1, VkWriteDescriptorSetAccelerationStructureKHR{
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR,
            .accelerationStructureCount = 1,
            .pAccelerationStructures = &tlas.acceleration_structure,
        })
        .pushImageInfo(1, 1, image->descriptor_info(VK_IMAGE_LAYOUT_GENERAL), VK_DESCRIPTOR_TYPE_STORAGE_IMAGE)
        .pushBufferInfo(2, 1, ubo->descriptor_info())
        .update();
    
    auto swapChain_descriptor_set_layout = gfx::vulkan::descriptor_set_layout_builder_t{}
        .addLayoutBinding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_ALL_GRAPHICS)
        .build(ctx);

    auto swapChain_descriptor_set = swapChain_descriptor_set_layout->new_descriptor_set();

    auto swapChain_pipeline = gfx::vulkan::pipeline_builder_t{}
        .add_default_color_blend_attachment_state()
        .add_dynamic_state(VK_DYNAMIC_STATE_VIEWPORT)
        .add_dynamic_state(VK_DYNAMIC_STATE_SCISSOR)
        .add_descriptor_set_layout(swapChain_descriptor_set_layout)
        .add_shader("../../assets/shaders/swapchain/base.vert.spv")
        .add_shader("../../assets/shaders/swapchain/base.frag.spv")
        .build(ctx, ctx->swapchain_renderpass());

    swapChain_descriptor_set->write()
        .pushImageInfo(0, 1, image->descriptor_info(VK_IMAGE_LAYOUT_GENERAL))
        .update();

    auto temp_test_renderpass = renderpass_builder_t{}
        .add_color_attachment(VkAttachmentDescription{
            .format = VK_FORMAT_R8G8B8A8_SNORM,
            .samples = VK_SAMPLE_COUNT_1_BIT,
            .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
            .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
            .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
            .finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,            
        })
        .build(ctx);

    auto temp_test_image = image_builder_t{}
        .build2D(ctx, width, height, VK_FORMAT_R8G8B8A8_SNORM, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    
    auto temp_test_framebuffer = framebuffer_builder_t{}
        .add_attachment_view(temp_test_image->image_view())
        .build(ctx, temp_test_renderpass->renderpass(), width, height);

    auto temp_test_descriptor_set_layout = descriptor_set_layout_builder_t{}
        .addLayoutBinding(0, VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, 1, VK_SHADER_STAGE_FRAGMENT_BIT)
        .addLayoutBinding(1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_FRAGMENT_BIT)
        .build(ctx);

    auto temp_test_descriptor_set = temp_test_descriptor_set_layout->new_descriptor_set();

    struct properties_t {
        int width = width;
        int height = height;
    } properties;

    auto properties_ubo = buffer_builder_t{}
        .build(ctx, sizeof(properties), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);
    
    std::memcpy(properties_ubo->map(), &properties, sizeof(properties));
    properties_ubo->unmap();

    temp_test_descriptor_set->write()
        .pushAccelerationStructureInfo(0, 1, VkWriteDescriptorSetAccelerationStructureKHR{
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR,
            .accelerationStructureCount = 1,
            .pAccelerationStructures = &tlas.acceleration_structure
        })
        .pushBufferInfo(1, 1, properties_ubo->descriptor_info())
        .update();
    
    auto temp_test_pipeline = pipeline_builder_t{}
        .add_default_color_blend_attachment_state()
        .add_dynamic_state(VK_DYNAMIC_STATE_VIEWPORT)
        .add_dynamic_state(VK_DYNAMIC_STATE_SCISSOR)
        .add_descriptor_set_layout(temp_test_descriptor_set_layout)
        .add_shader("../../assets/shaders/test3/test_new_rt/shader.vert.spv")
        .add_shader("../../assets/shaders/test3/test_new_rt/shader.frag.spv")
        .build(ctx, temp_test_renderpass->renderpass());

    swapChain_descriptor_set->write()
        .pushImageInfo(0, 1, temp_test_image->descriptor_info(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL))
        .update();

    EditorCamera editor_camera{window};

    float target_FPS = 60.f;
    auto last_time = std::chrono::system_clock::now();
    while (!window->shouldClose()) {
        window->pollEvents();
        
        auto current_time = std::chrono::system_clock::now();
        auto time_difference = current_time - last_time;
        if (time_difference.count() / 1e6 < 1000.f / target_FPS) {
            continue;
        }
        last_time = current_time;
        float dt = time_difference.count() / float(1e6);

        editor_camera.onUpdate(dt);

        uniform_data.view_inv = glm::inverse(editor_camera.getView());
        uniform_data.projection_inv = glm::inverse(editor_camera.getProjection());
        // INFO("{}\n\n{}", glm::to_string(uniform_data.view_inv), glm::to_string(uniform_data.projection_inv));
        std::memcpy(ubo->map(), &uniform_data, sizeof(uniform_data));

        if (auto start_frame = ctx->start_frame()) {
            auto [command_buffer, current_index] = *start_frame;

            VkClearValue clear_color{};
            clear_color.color = {0, 0, 0, 0};    
            VkClearValue clear_depth{};
            clear_depth.depthStencil.depth = 1;

            VkViewport swapchain_viewport{};
            swapchain_viewport.x = 0;
            swapchain_viewport.y = 0;
            swapchain_viewport.width = static_cast<float>(ctx->swapchain_extent().width);
            swapchain_viewport.height = static_cast<float>(ctx->swapchain_extent().height);
            swapchain_viewport.minDepth = 0;
            swapchain_viewport.maxDepth = 1;
            VkRect2D swapchain_scissor{};
            swapchain_scissor.offset = {0, 0};
            swapchain_scissor.extent = ctx->swapchain_extent();

            // VkStridedDeviceAddressRegionKHR rgen_shader_sbt_entry{};
            // rgen_shader_sbt_entry.deviceAddress = rgen_shader_binding_table->device_address();
            // rgen_shader_sbt_entry.stride = handle_size_aligned;
            // rgen_shader_sbt_entry.size = handle_size_aligned;
            
            // VkStridedDeviceAddressRegionKHR rmiss_shader_sbt_entry{};
            // rmiss_shader_sbt_entry.deviceAddress = rmiss_shader_binding_table->device_address();
            // rmiss_shader_sbt_entry.stride = handle_size_aligned;
            // rmiss_shader_sbt_entry.size = handle_size_aligned;
            
            // VkStridedDeviceAddressRegionKHR rchit_shader_sbt_entry{};
            // rchit_shader_sbt_entry.deviceAddress = rchit_shader_binding_table->device_address();
            // rchit_shader_sbt_entry.stride = handle_size_aligned;
            // rchit_shader_sbt_entry.size = handle_size_aligned;

            // VkStridedDeviceAddressRegionKHR callable_shader_sbt_entry{};

            // vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, rt_pipeline);
            // vkCmdBindDescriptorSets(command_buffer, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, rt_pipeline_layout, 0, 1, &rt_descriptor_set->descriptor_set(), 0, 0);

            // vkCmdTraceRaysKHR(command_buffer,
            //                   &rgen_shader_sbt_entry,
            //                   &rmiss_shader_sbt_entry,
            //                   &rchit_shader_sbt_entry,
            //                   &callable_shader_sbt_entry,
            //                   width,
            //                   height,
            //                   1);
            
            
            VkViewport viewport{};
            viewport.x = 0.0f;
            viewport.y = 0.0f;
            viewport.width = static_cast<float>(ctx->swapchain_extent().width);
            viewport.height = static_cast<float>(ctx->swapchain_extent().height);
            viewport.minDepth = 0.0f;
            viewport.maxDepth = 1.0f;
            VkRect2D scissor{};
            scissor.offset = {0, 0};
            scissor.extent = ctx->swapchain_extent();   

            temp_test_renderpass->begin(command_buffer, temp_test_framebuffer->framebuffer(), VkRect2D{
                .offset = {0, 0},
                .extent = ctx->swapchain_extent(),
            }, {
                clear_color,
            }); 
            vkCmdSetViewport(command_buffer, 0, 1, &viewport);
    		vkCmdSetScissor(command_buffer, 0, 1, &scissor);
            temp_test_pipeline->bind(command_buffer);
            vkCmdBindDescriptorSets(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, temp_test_pipeline->pipeline_layout(), 0, 1, &temp_test_descriptor_set->descriptor_set(), 0, nullptr);
            vkCmdDraw(command_buffer, 6, 1, 0, 0);

            temp_test_renderpass->end(command_buffer);

            ctx->begin_swapchain_renderpass(command_buffer, clear_color);

            vkCmdSetViewport(command_buffer, 0, 1, &viewport);
    		vkCmdSetScissor(command_buffer, 0, 1, &scissor);
            swapChain_pipeline->bind(command_buffer);
            vkCmdBindDescriptorSets(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, swapChain_pipeline->pipeline_layout(), 0, 1, &swapChain_descriptor_set->descriptor_set(), 0, nullptr);
            vkCmdDraw(command_buffer, 6, 1, 0, 0);

            // core::ImGui_newframe();
            // ImGui::Begin("debug");
            // ImGui::End();
            // core::ImGui_endframe(command_buffer);

            ctx->end_swapchain_renderpass(command_buffer);

            ctx->end_frame(command_buffer);
        }
        core::clear_frame_function_times();
    }

    ctx->wait_idle();

    core::ImGui_shutdown();

    return 0;
}
//
// Created by Mike on 2022/1/7.
//

#include <scene/pipeline.h>
#include <scene/integrator.h>

namespace luisa::render {

class NormalVisualizer;

class NormalVisualizerInstance : public Integrator::Instance {

private:
    void _render_one_camera(
        Stream &stream, Pipeline &pipeline,
        const Camera::Instance *camera, const Filter::Instance *filter,
        Film::Instance *film) noexcept;

public:
    explicit NormalVisualizerInstance(const NormalVisualizer *integrator) noexcept;
    void render(Stream &stream, Pipeline &pipeline) noexcept override;
};

class NormalVisualizer final : public Integrator {

public:
    NormalVisualizer(Scene *scene, const SceneNodeDesc *desc) noexcept : Integrator{scene, desc} {}
    [[nodiscard]] luisa::string_view impl_type() const noexcept override { return "normal"; }
    [[nodiscard]] luisa::unique_ptr<Instance> build(Pipeline &pipeline, CommandBuffer &command_buffer) const noexcept override {
        return luisa::make_unique<NormalVisualizerInstance>(this);
    }
};

NormalVisualizerInstance::NormalVisualizerInstance(const NormalVisualizer *integrator) noexcept
    : Integrator::Instance{integrator} {}

void NormalVisualizerInstance::render(Stream &stream, Pipeline &pipeline) noexcept {
    for (auto i = 0u; i < pipeline.camera_count(); i++) {
        auto [camera, film, filter] = pipeline.camera(i);
        _render_one_camera(stream, pipeline, camera, filter, film);
        film->save(stream);
    }
}

void NormalVisualizerInstance::_render_one_camera(
    Stream &stream, Pipeline &pipeline,
    const Camera::Instance *camera,
    const Filter::Instance *filter,
    Film::Instance *film) noexcept {

    auto spp = film->film()->spp();
    auto resolution = film->film()->resolution();
    LUISA_INFO(
        "Rendering to '{}' of resolution {}x{} at {}spp.",
        film->film()->file().string(),
        resolution.x, resolution.y, spp);

    auto sampler = pipeline.sampler();
    auto command_buffer = stream.command_buffer();
    film->clear(command_buffer);
    sampler->reset(command_buffer, resolution, spp);
    command_buffer.commit();

    using namespace luisa::compute;
    Kernel2D render_kernel = [&](UInt frame_index, Float4x4 camera_to_world, Float3x3 camera_to_world_normal, Float time) noexcept {
        auto pixel_id = dispatch_id().xy();
        sampler->start(pixel_id, frame_index);
        auto pixel = make_float2(pixel_id);
        if (filter == nullptr) {// not specified, using default box sampler
            pixel += sampler->generate_2d();
        } else {
            // TODO: support filter sampling
            LUISA_ERROR_WITH_LOCATION(
                "Filter sampling is not implemented.");
        }
        auto [ray, weight] = camera->generate_ray(*sampler, pixel, time);
        sampler->save_state();

        if (camera->camera()->transform() != nullptr) {
            ray.origin = make_float3(camera_to_world * make_float4(def<float3>(ray.origin), 1.0f));
            ray.direction = normalize(camera_to_world_normal * def<float3>(ray.direction));
        }
        auto hit = pipeline.accel().trace_closest(ray);
        $if(hit->miss()) {
            film->accumulate(pixel_id, make_float3());
        }
        $else {
            auto [instance, instance_transform] = pipeline.instance(hit);
            auto triangle = pipeline.triangle(instance, hit);
            auto [normal, tangent, uv] = pipeline.vertex_attribute(instance, triangle, hit);
            auto m = transpose(inverse(make_float3x3(instance_transform)));
            film->accumulate(pixel_id, make_float3(m * normal * 0.5f + 0.5f));
        };
    };
    auto render = pipeline.device().compile(render_kernel);
    Clock clock;
    auto time_start = camera->camera()->time_span().x;
    auto time_end = camera->camera()->time_span().x;
    for (auto i = 0u; i < spp; i++) {
        auto t = static_cast<float>((static_cast<double>(i) + 0.5f) / static_cast<double>(spp));
        auto time = lerp(time_start, time_end, t);
        pipeline.update_geometry(command_buffer, time);
        auto camera_transform = camera->camera()->transform();
        auto camera_to_world = camera_transform == nullptr ? make_float4x4() : camera_transform->matrix(t);
        auto camera_to_world_normal = transpose(inverse(make_float3x3(camera_to_world)));
        command_buffer << render(i, camera_to_world, camera_to_world_normal, time).dispatch(resolution)
                       << commit();
    }
}

}// namespace luisa::render

LUISA_RENDER_MAKE_SCENE_NODE_PLUGIN(luisa::render::NormalVisualizer)
//
// Created by Mike on 2022/1/7.
//

#include <util/imageio.h>
#include <util/rng.h>
#include <base/pipeline.h>
#include <base/integrator.h>

namespace luisa::render {

class NormalVisualizer;

class NormalVisualizerInstance final : public Integrator::Instance {

private:
    void _render_one_camera(
        CommandBuffer &command_buffer,
        Camera::Instance *camera) noexcept;

public:
    explicit NormalVisualizerInstance(
        const NormalVisualizer *integrator,
        Pipeline &pipeline, CommandBuffer &cb) noexcept;
    void render(Stream &stream) noexcept override;
};

class NormalVisualizer final : public Integrator {

public:
    NormalVisualizer(Scene *scene, const SceneNodeDesc *desc) noexcept : Integrator{scene, desc} {}
    [[nodiscard]] bool is_differentiable() const noexcept override { return false; }
    [[nodiscard]] luisa::string_view impl_type() const noexcept override { return LUISA_RENDER_PLUGIN_NAME; }
    [[nodiscard]] luisa::unique_ptr<Integrator::Instance> build(Pipeline &pipeline, CommandBuffer &cb) const noexcept override {
        return luisa::make_unique<NormalVisualizerInstance>(this, pipeline, cb);
    }
};

NormalVisualizerInstance::NormalVisualizerInstance(
    const NormalVisualizer *integrator, Pipeline &pipeline, CommandBuffer &cb) noexcept
    : Integrator::Instance{pipeline, cb, integrator} {}

void NormalVisualizerInstance::render(Stream &stream) noexcept {
    luisa::vector<float4> pixels;
    auto command_buffer = stream.command_buffer();
    for (auto i = 0u; i < pipeline().camera_count(); i++) {
        auto camera = pipeline().camera(i);
        auto resolution = camera->film()->node()->resolution();
        auto pixel_count = resolution.x * resolution.y;
        camera->film()->prepare(command_buffer);
        _render_one_camera(command_buffer, camera);
        pixels.resize(next_pow2(pixel_count));
        camera->film()->download(command_buffer, reinterpret_cast<float4 *>(pixels.data()));
        command_buffer << compute::synchronize();
        camera->film()->release();
        auto film_path = camera->node()->file();
        save_image(film_path, (const float *)pixels.data(), resolution);
    }
}

void NormalVisualizerInstance::_render_one_camera(
    CommandBuffer &command_buffer, Camera::Instance *camera) noexcept {

    auto spp = camera->node()->spp();
    auto resolution = camera->film()->node()->resolution();
    auto image_file = camera->node()->file();
    LUISA_INFO(
        "Rendering to '{}' of resolution {}x{} at {}spp.",
        image_file.string(),
        resolution.x, resolution.y, spp);

    auto pixel_count = resolution.x * resolution.y;
    sampler()->reset(command_buffer, resolution, pixel_count, spp);
    command_buffer.commit();

    using namespace luisa::compute;
    auto render = pipeline().device().compile<2>([&](UInt frame_index, Float time, Float shutter_weight) noexcept {
        auto pixel_id = dispatch_id().xy();
        sampler()->start(pixel_id, frame_index);
        auto [ray, _, camera_weight] = camera->generate_ray(*sampler(), pixel_id, time);
        auto swl = pipeline().spectrum()->sample(sampler()->generate_1d());
        auto path_weight = camera_weight;
        auto it = pipeline().geometry()->intersect(ray);
        auto color = def(make_float3(0.f));
        auto wo = -ray->direction();
        $if(it->valid()) {
            pipeline().surfaces().dispatch(it->shape()->surface_tag(), [&](auto surface) noexcept {
                auto closure = surface->closure(*it, swl, 1.f, time);
                color = closure->it().shading().n() * .5f + .5f;
            });
        };
        camera->film()->accumulate(pixel_id, shutter_weight * path_weight * color);
    });
    auto shutter_samples = camera->node()->shutter_samples();
    command_buffer << synchronize();
    LUISA_INFO("Rendering started.");
    Clock clock;
    auto sample_id = 0u;
    auto dispatch_count = 0u;
    auto dispatches_per_commit = 64u;
    for (auto s : shutter_samples) {
        pipeline().update(command_buffer, s.point.time);
        for (auto i = 0u; i < s.spp; i++) {
            command_buffer << render(sample_id++, s.point.time, s.point.weight)
                                  .dispatch(resolution);
            if (++dispatch_count % dispatches_per_commit == 0u) [[unlikely]] {
                command_buffer << commit();
                dispatch_count = 0u;
            }
        }
    }
    command_buffer << synchronize();
    LUISA_INFO("Rendering finished in {} ms.", clock.toc());
}

}// namespace luisa::render

LUISA_RENDER_MAKE_SCENE_NODE_PLUGIN(luisa::render::NormalVisualizer)

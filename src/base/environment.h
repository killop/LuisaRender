//
// Created by Mike Smith on 2022/1/14.
//

#pragma once

#include <base/scene_node.h>
#include <base/light.h>

namespace luisa::render {

class Transform;
using compute::Float3x3;

class Environment : public SceneNode {

public:
    class Instance {

    private:
        const Pipeline &_pipeline;
        const Environment *_env;

    public:
        Instance(Pipeline &pipeline, const Environment *env) noexcept;
        virtual ~Instance() noexcept = default;
        template<typename T = Environment>
            requires std::is_base_of_v<Environment, T>
        [[nodiscard]] auto node() const noexcept { return static_cast<const T *>(_env); }
        [[nodiscard]] auto &pipeline() const noexcept { return _pipeline; }
        [[nodiscard]] virtual Light::Evaluation evaluate(
            Expr<float3> wi, const SampledWavelengths &swl, Expr<float> time) const noexcept = 0;
        [[nodiscard]] virtual Light::Sample sample(
            Expr<float3> p_from, const SampledWavelengths &swl,
            Expr<float> time, Expr<float2> u) const noexcept = 0;
        [[nodiscard]] Float3x3 transform_to_world() const noexcept;
    };

private:
    const Transform *_transform;

public:
    Environment(Scene *scene, const SceneNodeDesc *desc) noexcept;
    [[nodiscard]] auto transform() const noexcept { return _transform; }
    [[nodiscard]] virtual bool is_black() const noexcept = 0;
    [[nodiscard]] virtual luisa::unique_ptr<Instance> build(
        Pipeline &pipeline, CommandBuffer &command_buffer) const noexcept = 0;
};

}// namespace luisa::render

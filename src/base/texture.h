//
// Created by Mike Smith on 2022/1/25.
//

#pragma once

#include <dsl/syntax.h>
#include <util/spec.h>
#include <util/imageio.h>
#include <util/half.h>
#include <base/scene_node.h>
#include <base/differentiation.h>
#include <base/spectrum.h>

namespace luisa::render {

#define LUISA_RENDER_CHECK_ALBEDO_TEXTURE(class_name, name)                      \
    [&] {                                                                        \
        if (_##name == nullptr) { return; }                                      \
        LUISA_ASSERT(_##name->semantic() == Texture::Semantic::ALBEDO ||         \
                         _##name->semantic() == Texture::Semantic::GENERIC,      \
                     "Expected albedo texture for " #class_name "::" #name "."); \
        if (_##name->semantic() == Texture::Semantic::GENERIC) {                 \
            LUISA_WARNING_WITH_LOCATION(                                         \
                #class_name "::" #name " requires an albedo texture. "           \
                            "Using a generic texture might be inefficient "      \
                            "due to spectrum conversion overhead.");             \
        }                                                                        \
    }()

#define LUISA_RENDER_CHECK_ILLUMINANT_TEXTURE(class_name, name)                      \
    [&] {                                                                            \
        if (_##name == nullptr) { return; }                                          \
        LUISA_ASSERT(_##name->semantic() == Texture::Semantic::ILLUMINANT ||         \
                         _##name->semantic() == Texture::Semantic::GENERIC,          \
                     "Expected illuminant texture for " #class_name "::" #name "."); \
        if (_##name->semantic() == Texture::Semantic::GENERIC) {                     \
            LUISA_WARNING_WITH_LOCATION(                                             \
                #class_name "::" #name " requires an illuminant texture. "           \
                            "Using a generic texture might be inefficient "          \
                            "due to spectrum conversion overhead.");                 \
        }                                                                            \
    }()

#define LUISA_RENDER_CHECK_GENERIC_TEXTURE(class_name, name, channel_num)  \
    LUISA_ASSERT(_##name == nullptr ||                                     \
                     (_##name->semantic() == Texture::Semantic::GENERIC && \
                      _##name->channels() >= (channel_num)),               \
                 "Expected generic texture with channels >= " #channel_num \
                 " for " #class_name "::" #name ".")

class Pipeline;
class Interaction;
class SampledWavelengths;

using compute::Buffer;
using compute::BufferView;
using compute::CommandBuffer;
using compute::Float4;
using compute::Image;
using compute::PixelStorage;
using TextureSampler = compute::Sampler;

class Texture : public SceneNode {

public:
    enum struct Semantic {
        ALBEDO,
        ILLUMINANT,
        GENERIC
    };

public:
    class Instance {

    private:
        const Pipeline &_pipeline;
        const Texture *_texture;

    public:
        Instance(const Pipeline &pipeline, const Texture *texture) noexcept
            : _pipeline{pipeline}, _texture{texture} {}
        virtual ~Instance() noexcept = default;
        // clang-format off
        template<typename T = Texture>
            requires std::is_base_of_v<Texture, T>
        [[nodiscard]] auto node() const noexcept { return static_cast<const T *>(_texture); }
        // clang-format on
        [[nodiscard]] auto &pipeline() const noexcept { return _pipeline; }
        [[nodiscard]] virtual Float4 evaluate(
            const Interaction &it, const SampledWavelengths &swl, Expr<float> time) const noexcept = 0;
        virtual void backward(
            const Interaction &it, const SampledWavelengths &swl,
            Expr<float> time, Expr<float4> grad) const noexcept = 0;
        [[nodiscard]] virtual Spectrum::Decode evaluate_albedo_spectrum(
            const Interaction &it, const SampledWavelengths &swl, Expr<float> time) const noexcept;
        [[nodiscard]] virtual Spectrum::Decode evaluate_illuminant_spectrum(
            const Interaction &it, const SampledWavelengths &swl, Expr<float> time) const noexcept;
        void backward_albedo_spectrum(
            const Interaction &it, const SampledWavelengths &swl,
            Expr<float> time, const SampledSpectrum &dSpec) const noexcept;
        void backward_illuminant_spectrum(
            const Interaction &it, const SampledWavelengths &swl,
            Expr<float> time, const SampledSpectrum &dSpec) const noexcept;
    };

private:
    float2 _range;
    Semantic _semantic;
    bool _requires_grad;

public:
    Texture(Scene *scene, const SceneNodeDesc *desc) noexcept;
    [[nodiscard]] auto range() const noexcept { return _range; }
    [[nodiscard]] auto semantic() const noexcept { return _semantic; }
    [[nodiscard]] virtual bool requires_gradients() const noexcept;
    virtual void disable_gradients() noexcept;
    [[nodiscard]] virtual bool is_black() const noexcept = 0;
    [[nodiscard]] virtual bool is_constant() const noexcept = 0;
    [[nodiscard]] virtual bool is_spectral_encoding() const noexcept { return false; }
    [[nodiscard]] virtual uint channels() const noexcept { return 4u; }
    [[nodiscard]] virtual luisa::unique_ptr<Instance> build(
        Pipeline &pipeline, CommandBuffer &command_buffer) const noexcept = 0;
};

}// namespace luisa::render

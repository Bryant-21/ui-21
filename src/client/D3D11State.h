#pragma once
#include <d3d11_1.h>
#include <array>

namespace b21ui::client {
    template <class Shader>
    struct ShaderBinding {
        Shader* shader{};
        std::array<ID3D11ClassInstance*, 256> classes{};
        UINT count = static_cast<UINT>(classes.size());
        ~ShaderBinding() {
            if (shader) shader->Release();
            for (UINT i = 0; i < count; ++i) if (classes[i]) classes[i]->Release();
        }
    };

    class D3D11State {
    public:
        explicit D3D11State(ID3D11DeviceContext* context) : context_(context) {
            ID3D11Device* device{};
            context_->GetDevice(&device);
            unorderedCount_ = device->GetFeatureLevel() >= D3D_FEATURE_LEVEL_11_1 ?
                D3D11_1_UAV_SLOT_COUNT : D3D11_PS_CS_UAV_REGISTER_COUNT;
            device->Release();
            context_->OMGetRenderTargetsAndUnorderedAccessViews(static_cast<UINT>(targets_.size()), targets_.data(),
                &depth_, 0, unorderedCount_, unordered_.data());
            for (UINT i = 0; i < targets_.size(); ++i) if (targets_[i]) targetCount_ = i + 1;
            context_->HSGetShader(&hull_.shader, hull_.classes.data(), &hull_.count);
            context_->DSGetShader(&domain_.shader, domain_.classes.data(), &domain_.count);
            context_->CSGetShader(&compute_.shader, compute_.classes.data(), &compute_.count);
        }

        D3D11State(const D3D11State&) = delete;
        D3D11State& operator=(const D3D11State&) = delete;

        ~D3D11State() {
            std::array<UINT, D3D11_1_UAV_SLOT_COUNT> counters;
            counters.fill(UINT(-1));
            // Null trailing RTVs still occupy UAV slots, so restore only through the last bound RTV.
            context_->OMSetRenderTargetsAndUnorderedAccessViews(targetCount_, targets_.data(), depth_,
                targetCount_, unorderedCount_ - targetCount_, unordered_.data() + targetCount_, counters.data());
            // ImGui clears these stages without restoring them; graphics mods use them during Present.
            context_->HSSetShader(hull_.shader, hull_.classes.data(), hull_.count);
            context_->DSSetShader(domain_.shader, domain_.classes.data(), domain_.count);
            context_->CSSetShader(compute_.shader, compute_.classes.data(), compute_.count);
            for (auto* target : targets_) if (target) target->Release();
            for (auto* unordered : unordered_) if (unordered) unordered->Release();
            if (depth_) depth_->Release();
        }

    private:
        ID3D11DeviceContext* context_;
        std::array<ID3D11RenderTargetView*, D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT> targets_{};
        std::array<ID3D11UnorderedAccessView*, D3D11_1_UAV_SLOT_COUNT> unordered_{};
        ID3D11DepthStencilView* depth_{};
        UINT targetCount_{}, unorderedCount_{};
        ShaderBinding<ID3D11HullShader> hull_;
        ShaderBinding<ID3D11DomainShader> domain_;
        ShaderBinding<ID3D11ComputeShader> compute_;
    };
}

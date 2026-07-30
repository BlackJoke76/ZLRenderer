#pragma once

#include "zl/rhi/RHI.hpp"

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace zl::rhi {

struct CompiledShaderDesc {
    ShaderStage stage = ShaderStage::Vertex;
    std::string fileName;
    std::string entryPoint = "main";
};

struct CompiledShader {
    ShaderStage stage = ShaderStage::Vertex;
    std::string entryPoint;
    std::vector<std::uint32_t> spirv;
};

class CompiledShaderLibrary {
public:
    explicit CompiledShaderLibrary(std::filesystem::path rootDirectory);

    CompiledShader load(const CompiledShaderDesc& desc) const;

private:
    std::filesystem::path rootDirectory_;
};

} // namespace zl::rhi

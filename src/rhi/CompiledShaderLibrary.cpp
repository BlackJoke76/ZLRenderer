#include "zl/rhi/CompiledShaderLibrary.hpp"

#include <fstream>
#include <stdexcept>
#include <utility>

namespace zl::rhi {

CompiledShaderLibrary::CompiledShaderLibrary(std::filesystem::path rootDirectory)
    : rootDirectory_(std::move(rootDirectory))
{
}

CompiledShader CompiledShaderLibrary::load(const CompiledShaderDesc& desc) const
{
    if (desc.fileName.empty()) {
        throw std::invalid_argument("Compiled shader file name must not be empty.");
    }
    if (desc.entryPoint.empty()) {
        throw std::invalid_argument("Compiled shader entry point must not be empty.");
    }

    const auto path = rootDirectory_ / desc.fileName;
    std::ifstream file(path, std::ios::ate | std::ios::binary);
    if (!file.is_open()) {
        throw std::runtime_error("Failed to open SPIR-V file: " + path.string());
    }

    const auto fileSize = static_cast<std::streamoff>(file.tellg());
    if (fileSize <= 0 || fileSize % static_cast<std::streamoff>(sizeof(std::uint32_t)) != 0) {
        throw std::runtime_error("SPIR-V file size is invalid: " + path.string());
    }

    std::vector<std::uint32_t> spirv(
        static_cast<std::size_t>(fileSize) / sizeof(std::uint32_t));
    file.seekg(0);
    file.read(reinterpret_cast<char*>(spirv.data()), static_cast<std::streamsize>(fileSize));
    if (!file) {
        throw std::runtime_error("Failed to read SPIR-V file: " + path.string());
    }

    return CompiledShader{
        .stage = desc.stage,
        .entryPoint = desc.entryPoint,
        .spirv = std::move(spirv),
    };
}

} // namespace zl::rhi

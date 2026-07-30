#include "zl/rhi/CompiledShaderLibrary.hpp"

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace {

class TemporaryDirectory {
public:
    TemporaryDirectory()
    {
        const auto suffix = std::chrono::steady_clock::now().time_since_epoch().count();
        path_ = std::filesystem::temp_directory_path() /
            ("zl_compiled_shader_tests_" + std::to_string(suffix));
        std::filesystem::create_directories(path_);
    }

    ~TemporaryDirectory()
    {
        std::error_code error;
        std::filesystem::remove_all(path_, error);
    }

    TemporaryDirectory(const TemporaryDirectory&) = delete;
    TemporaryDirectory& operator=(const TemporaryDirectory&) = delete;

    const std::filesystem::path& path() const
    {
        return path_;
    }

private:
    std::filesystem::path path_;
};

bool expect(bool condition, std::string_view message)
{
    if (!condition) {
        std::cerr << "FAILED: " << message << '\n';
    }
    return condition;
}

void writeWords(const std::filesystem::path& path, const std::vector<std::uint32_t>& words)
{
    std::ofstream file(path, std::ios::binary);
    if (!file.is_open()) {
        throw std::runtime_error("Failed to create test shader file.");
    }
    file.write(
        reinterpret_cast<const char*>(words.data()),
        static_cast<std::streamsize>(words.size() * sizeof(std::uint32_t)));
}

bool loadsCompiledShaderMetadataAndWords()
{
    const TemporaryDirectory directory;
    const std::vector<std::uint32_t> expectedSpirv = {
        0x07230203u,
        0x00010500u,
    };
    writeWords(directory.path() / "triangle.vert.spv", expectedSpirv);

    const zl::rhi::CompiledShaderLibrary library(directory.path());
    const auto shader = library.load(zl::rhi::CompiledShaderDesc{
        .stage = zl::rhi::ShaderStage::Vertex,
        .fileName = "triangle.vert.spv",
        .entryPoint = "vertexMain",
    });

    bool passed = true;
    passed &= expect(shader.stage == zl::rhi::ShaderStage::Vertex, "stage is preserved");
    passed &= expect(shader.entryPoint == "vertexMain", "entry point is preserved");
    passed &= expect(shader.spirv == expectedSpirv, "SPIR-V words are loaded exactly");
    return passed;
}

bool rejectsMissingShaderFile()
{
    const TemporaryDirectory directory;
    const zl::rhi::CompiledShaderLibrary library(directory.path());

    try {
        static_cast<void>(library.load(zl::rhi::CompiledShaderDesc{
            .stage = zl::rhi::ShaderStage::Fragment,
            .fileName = "missing.frag.spv",
            .entryPoint = "fragmentMain",
        }));
    } catch (const std::runtime_error& error) {
        return expect(
            std::string_view{error.what()}.find("Failed to open SPIR-V file") != std::string_view::npos,
            "missing file error includes the failure reason");
    }

    return expect(false, "missing shader file throws");
}

bool rejectsMisalignedShaderFile()
{
    const TemporaryDirectory directory;
    const auto path = directory.path() / "invalid.spv";
    std::ofstream file(path, std::ios::binary);
    const char bytes[] = {0x03, 0x02, 0x23};
    file.write(bytes, sizeof(bytes));
    file.close();

    const zl::rhi::CompiledShaderLibrary library(directory.path());
    try {
        static_cast<void>(library.load(zl::rhi::CompiledShaderDesc{
            .stage = zl::rhi::ShaderStage::Vertex,
            .fileName = "invalid.spv",
            .entryPoint = "main",
        }));
    } catch (const std::runtime_error& error) {
        return expect(
            std::string_view{error.what()}.find("file size is invalid") != std::string_view::npos,
            "misaligned file error includes the failure reason");
    }

    return expect(false, "misaligned shader file throws");
}

} // namespace

int main()
{
    bool passed = true;
    passed &= loadsCompiledShaderMetadataAndWords();
    passed &= rejectsMissingShaderFile();
    passed &= rejectsMisalignedShaderFile();

    if (passed) {
        std::cout << "CompiledShaderLibrary tests passed.\n";
        return 0;
    }

    return 1;
}

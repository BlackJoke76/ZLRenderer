#include "zl/render_graph/RenderGraph.hpp"

#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>

namespace zl::render_graph {
namespace {

const char* passTypeName(PassType type)
{
    switch (type) {
    case PassType::Graphics:
        return "Graphics";
    case PassType::Compute:
        return "Compute";
    case PassType::Transfer:
        return "Transfer";
    }

    return "Unknown";
}

const char* resourceStateName(rhi::ResourceState state)
{
    switch (state) {
    case rhi::ResourceState::Undefined:
        return "Undefined";
    case rhi::ResourceState::RenderTarget:
        return "RenderTarget";
    case rhi::ResourceState::DepthWrite:
        return "DepthWrite";
    case rhi::ResourceState::ShaderRead:
        return "ShaderRead";
    case rhi::ResourceState::ShaderWrite:
        return "ShaderWrite";
    case rhi::ResourceState::TransferSrc:
        return "TransferSrc";
    case rhi::ResourceState::TransferDst:
        return "TransferDst";
    case rhi::ResourceState::Present:
        return "Present";
    }

    return "Unknown";
}

std::string passLabel(std::size_t index, const std::string& name)
{
    std::ostringstream stream;
    stream << "pass[" << index << "] " << name;
    return stream.str();
}

std::string textureLabel(std::size_t index, const std::string& name)
{
    std::ostringstream stream;
    stream << "texture[" << index << "] " << name;
    return stream.str();
}

std::string compileErrorMessage(const RenderGraph::CompileResult& result)
{
    std::ostringstream stream;
    stream << "RenderGraph compile failed.";
    for (const auto& error : result.errors) {
        stream << '\n' << "- " << error;
    }
    return stream.str();
}

} // namespace

bool RenderGraph::CompileResult::succeeded() const
{
    return errors.empty();
}

RenderGraph::PassBuilder::PassBuilder(RenderGraph& graph, PassHandle pass)
    : graph_(graph)
    , pass_(pass)
{
}

void RenderGraph::PassBuilder::readTexture(TextureHandle texture, rhi::ResourceState state)
{
    graph_.addTextureAccess(pass_, texture, state, false);
}

void RenderGraph::PassBuilder::writeTexture(TextureHandle texture, rhi::ResourceState state)
{
    graph_.addTextureAccess(pass_, texture, state, true);
}

TextureHandle RenderGraph::importSwapchainTexture(
    std::string name,
    rhi::ResourceState initialState,
    rhi::ResourceState finalState)
{
    const TextureHandle handle{static_cast<std::uint32_t>(textures_.size())};
    textures_.push_back(TextureResource{
        .name = std::move(name),
        .kind = TextureKind::Swapchain,
        .imported = true,
        .initialState = initialState,
        .finalState = finalState,
    });
    return handle;
}

PassHandle RenderGraph::addPass(
    std::string name,
    PassType type,
    const std::function<void(PassBuilder&)>& build,
    ExecuteCallback execute)
{
    if (!execute) {
        throw std::runtime_error("RenderGraph pass requires an execute callback.");
    }

    const PassHandle handle{static_cast<std::uint32_t>(passes_.size())};
    passes_.push_back(Pass{
        .name = std::move(name),
        .type = type,
        .textureAccesses = {},
        .execute = std::move(execute),
    });

    PassBuilder builder(*this, handle);
    if (build) {
        build(builder);
    }

    return handle;
}

RenderGraph::CompileResult RenderGraph::compile() const
{
    CompileResult result;

    std::vector<rhi::ResourceState> currentStates;
    std::vector<bool> hasContents;
    currentStates.reserve(textures_.size());
    hasContents.reserve(textures_.size());

    for (const auto& texture : textures_) {
        currentStates.push_back(texture.initialState);
        hasContents.push_back(texture.imported && texture.initialState != rhi::ResourceState::Undefined);
    }

    for (std::size_t passIndex = 0; passIndex < passes_.size(); ++passIndex) {
        const auto& pass = passes_[passIndex];
        if (!pass.execute) {
            result.errors.push_back(passLabel(passIndex, pass.name) + " has no execute callback.");
        }
        if (pass.textureAccesses.empty()) {
            result.errors.push_back(passLabel(passIndex, pass.name) + " declares no texture accesses.");
        }

        for (std::size_t accessIndex = 0; accessIndex < pass.textureAccesses.size(); ++accessIndex) {
            const auto& access = pass.textureAccesses[accessIndex];
            if (!valid(access.texture)) {
                result.errors.push_back(passLabel(passIndex, pass.name) + " references an invalid texture handle.");
                continue;
            }

            for (std::size_t otherIndex = accessIndex + 1; otherIndex < pass.textureAccesses.size(); ++otherIndex) {
                const auto& other = pass.textureAccesses[otherIndex];
                if (access.texture.index != other.texture.index) {
                    continue;
                }
                if (access.write || other.write || access.state != other.state) {
                    result.errors.push_back(
                        passLabel(passIndex, pass.name) + " declares conflicting accesses to " +
                        textureLabel(access.texture.index, textures_[access.texture.index].name) + ".");
                }
            }
        }

        for (const auto& access : pass.textureAccesses) {
            if (!valid(access.texture)) {
                continue;
            }

            const auto textureIndex = access.texture.index;
            if (access.state == rhi::ResourceState::Undefined) {
                result.errors.push_back(
                    passLabel(passIndex, pass.name) + " uses Undefined state for " +
                    textureLabel(textureIndex, textures_[textureIndex].name) + ".");
                continue;
            }

            if (!access.write && !hasContents[textureIndex]) {
                result.errors.push_back(
                    passLabel(passIndex, pass.name) + " reads " +
                    textureLabel(textureIndex, textures_[textureIndex].name) +
                    " before any pass writes it or imports defined contents.");
            }

            if (currentStates[textureIndex] != access.state) {
                result.transitions.push_back(CompileTransition{
                    .texture = access.texture,
                    .pass = PassHandle{static_cast<std::uint32_t>(passIndex)},
                    .before = currentStates[textureIndex],
                    .after = access.state,
                    .finalTransition = false,
                });
                currentStates[textureIndex] = access.state;
            }

            if (access.write) {
                hasContents[textureIndex] = true;
            }
        }
    }

    for (std::size_t textureIndex = 0; textureIndex < textures_.size(); ++textureIndex) {
        const auto& texture = textures_[textureIndex];
        if (texture.finalState == rhi::ResourceState::Undefined) {
            continue;
        }
        if (currentStates[textureIndex] == texture.finalState) {
            continue;
        }

        result.transitions.push_back(CompileTransition{
            .texture = TextureHandle{static_cast<std::uint32_t>(textureIndex)},
            .pass = PassHandle{},
            .before = currentStates[textureIndex],
            .after = texture.finalState,
            .finalTransition = true,
        });
    }

    return result;
}

void RenderGraph::execute(rhi::CommandList& commandList, const rhi::FrameContext& frameContext) const
{
    const auto compiled = compile();
    if (!compiled.succeeded()) {
        throw std::runtime_error(compileErrorMessage(compiled));
    }

    for (std::size_t passIndex = 0; passIndex < passes_.size(); ++passIndex) {
        for (const auto& transition : compiled.transitions) {
            if (!transition.finalTransition && transition.pass.index == passIndex) {
                executeTransition(commandList, transition);
            }
        }

        passes_[passIndex].execute(commandList, frameContext);
    }

    for (const auto& transition : compiled.transitions) {
        if (transition.finalTransition) {
            executeTransition(commandList, transition);
        }
    }
}

std::string RenderGraph::dump() const
{
    std::ostringstream stream;
    stream << "RenderGraph\n";
    stream << "  Resources\n";
    for (std::size_t i = 0; i < textures_.size(); ++i) {
        const auto& texture = textures_[i];
        stream << "    [" << i << "] texture " << texture.name
               << " imported=" << (texture.imported ? "true" : "false")
               << " initial=" << resourceStateName(texture.initialState)
               << " final=" << resourceStateName(texture.finalState)
               << '\n';
    }

    stream << "  Passes\n";
    for (std::size_t i = 0; i < passes_.size(); ++i) {
        const auto& pass = passes_[i];
        stream << "    [" << i << "] " << pass.name
                << " type=" << passTypeName(pass.type)
                << '\n';

        for (const auto& access : pass.textureAccesses) {
            const auto& texture = textures_[access.texture.index];
            stream << "      " << (access.write ? "write" : "read")
                   << " texture[" << access.texture.index << "] " << texture.name
                   << " state=" << resourceStateName(access.state)
                   << '\n';
        }
    }

    const auto compiled = compile();
    stream << "  Compile\n";
    stream << "    validation=" << (compiled.succeeded() ? "ok" : "failed") << '\n';
    for (const auto& error : compiled.errors) {
        stream << "    error: " << error << '\n';
    }
    if (compiled.transitions.empty()) {
        stream << "    transitions=none\n";
    } else {
        for (const auto& transition : compiled.transitions) {
            const auto textureIndex = transition.texture.index;
            const auto& texture = textures_[textureIndex];
            stream << "    ";
            if (transition.finalTransition) {
                stream << "final ";
            }
            stream << "transition texture[" << textureIndex << "] " << texture.name
                   << ": " << resourceStateName(transition.before)
                   << " -> " << resourceStateName(transition.after);
            if (!transition.finalTransition) {
                const auto passIndex = transition.pass.index;
                stream << " before pass[" << passIndex << "] " << passes_[passIndex].name;
            }
            stream << '\n';
        }
    }

    return stream.str();
}

void RenderGraph::clear()
{
    passes_.clear();
    textures_.clear();
}

void RenderGraph::addTextureAccess(
    PassHandle pass,
    TextureHandle texture,
    rhi::ResourceState state,
    bool write)
{
    if (!valid(pass)) {
        throw std::runtime_error("RenderGraph pass handle is invalid.");
    }
    if (!valid(texture)) {
        throw std::runtime_error("RenderGraph texture handle is invalid.");
    }

    passes_[pass.index].textureAccesses.push_back(TextureAccess{
        .texture = texture,
        .state = state,
        .write = write,
    });
}

void RenderGraph::executeTransition(rhi::CommandList& commandList, const CompileTransition& transition) const
{
    if (!valid(transition.texture)) {
        throw std::runtime_error("RenderGraph transition references an invalid texture handle.");
    }

    const auto& texture = textures_[transition.texture.index];
    switch (texture.kind) {
    case TextureKind::Swapchain:
        commandList.transitionSwapchainImage(transition.after);
        return;
    }

    throw std::runtime_error("RenderGraph transition references an unsupported texture kind.");
}

bool RenderGraph::valid(TextureHandle texture) const
{
    return texture.index < textures_.size();
}

bool RenderGraph::valid(PassHandle pass) const
{
    return pass.index < passes_.size();
}

} // namespace zl::render_graph

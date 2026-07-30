// Binary semaphore submit example.
//
// This file is intentionally not wired into CMake. Open it in VSCode and read
// it as a minimal submit-order example:
//
//     producer queue submit: command buffer + signal binary semaphore
//     consumer queue submit: wait binary semaphore + command buffer
//
// Assumptions:
// - producerCmd has already been recorded. It writes some resource.
// - consumerCmd has already been recorded. It reads that resource.
// - Any required image layout transitions or queue-family ownership transfers
//   are already recorded in those command buffers.
// - The device supports Vulkan 1.3 or VK_KHR_synchronization2.

#include <cstdint>
#include <stdexcept>
#include <vulkan/vulkan.h>

namespace zl::notes
{
namespace
{
void Check(VkResult result, const char* what)
{
    if (result != VK_SUCCESS)
    {
        throw std::runtime_error(what);
    }
}

VkCommandBufferSubmitInfo CommandBufferInfo(VkCommandBuffer commandBuffer)
{
    VkCommandBufferSubmitInfo info{};
    info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO;
    info.commandBuffer = commandBuffer;
    return info;
}

VkSemaphoreSubmitInfo BinarySemaphoreInfo(
    VkSemaphore semaphore,
    VkPipelineStageFlags2 stageMask)
{
    VkSemaphoreSubmitInfo info{};
    info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
    info.semaphore = semaphore;
    info.value = 0; // Ignored for binary semaphores. Timeline semaphores use this.
    info.stageMask = stageMask;
    info.deviceIndex = 0;
    return info;
}
}

void SubmitProducerThenConsumerWithBinarySemaphore(
    VkDevice device,
    VkQueue producerQueue,
    VkQueue consumerQueue,
    VkCommandBuffer producerCmd,
    VkCommandBuffer consumerCmd)
{
    VkSemaphore binarySemaphore = VK_NULL_HANDLE;

    VkSemaphoreCreateInfo semaphoreInfo{};
    semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    Check(
        vkCreateSemaphore(device, &semaphoreInfo, nullptr, &binarySemaphore),
        "vkCreateSemaphore failed");

    VkFence consumerFinished = VK_NULL_HANDLE;

    VkFenceCreateInfo fenceInfo{};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    Check(
        vkCreateFence(device, &fenceInfo, nullptr, &consumerFinished),
        "vkCreateFence failed");

    // 1. Submit producer work first.
    //
    // The signal operation waits for the producer submit's scoped compute
    // shader work to complete. In this example, producerCmd writes the resource
    // from a compute shader.
    VkCommandBufferSubmitInfo producerCommand = CommandBufferInfo(producerCmd);
    VkSemaphoreSubmitInfo signalBinary = BinarySemaphoreInfo(
        binarySemaphore,
        VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT);

    VkSubmitInfo2 producerSubmit{};
    producerSubmit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2;
    producerSubmit.commandBufferInfoCount = 1;
    producerSubmit.pCommandBufferInfos = &producerCommand;
    producerSubmit.signalSemaphoreInfoCount = 1;
    producerSubmit.pSignalSemaphoreInfos = &signalBinary;

    Check(
        vkQueueSubmit2(producerQueue, 1, &producerSubmit, VK_NULL_HANDLE),
        "producer vkQueueSubmit2 failed");

    // 2. Submit consumer work after the producer signal has been submitted.
    //
    // Binary semaphores have no numeric value. The wait must match one known
    // signal operation, so submit the signal side before submitting the wait
    // side. The wait blocks consumerCmd before it reaches the fragment shader
    // stage specified below.
    VkCommandBufferSubmitInfo consumerCommand = CommandBufferInfo(consumerCmd);
    VkSemaphoreSubmitInfo waitBinary = BinarySemaphoreInfo(
        binarySemaphore,
        VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT);

    VkSubmitInfo2 consumerSubmit{};
    consumerSubmit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2;
    consumerSubmit.waitSemaphoreInfoCount = 1;
    consumerSubmit.pWaitSemaphoreInfos = &waitBinary;
    consumerSubmit.commandBufferInfoCount = 1;
    consumerSubmit.pCommandBufferInfos = &consumerCommand;

    Check(
        vkQueueSubmit2(consumerQueue, 1, &consumerSubmit, consumerFinished),
        "consumer vkQueueSubmit2 failed");

    // This wait is only here to make the sample self-contained. A renderer
    // would usually keep the semaphore alive through its frame resource system.
    Check(
        vkWaitForFences(device, 1, &consumerFinished, VK_TRUE, UINT64_MAX),
        "vkWaitForFences failed");

    vkDestroyFence(device, consumerFinished, nullptr);
    vkDestroySemaphore(device, binarySemaphore, nullptr);
}

/*
Invalid binary-semaphore submit order:

    vkQueueSubmit2(consumerQueue, wait binarySemaphore);
    vkQueueSubmit2(producerQueue, signal binarySemaphore);

That wait-before-signal pattern is what timeline semaphores are for:

    wait timeline value N;
    signal timeline value N later;

Binary semaphores do not have a value N, so the wait must be paired with an
already-submitted signal operation.
*/
}

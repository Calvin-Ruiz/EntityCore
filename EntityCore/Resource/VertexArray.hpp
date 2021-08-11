#ifndef VERTEX_ARRAY_HPP
#define VERTEX_ARRAY_HPP

#include <memory>
#include <vector>
#include <array>
#include <vulkan/vulkan.h>
#include <cstring>

class VulkanMgr;
class VertexBuffer;
class BufferMgr;

/*
* A VertexBuffer layout to facilitate binding in pipeline
* The stride must be a multiple of the alignment
* If two or more VertexArray have the same alignment and use the same dedicated BufferMgr,
* then a binding from the start of the BufferMgr allow using any VertexBuffer
* with the VertexBuffer getOffset used as vertexOffset of the draw
* This allow reusing Vertex binding and dynamically change the VertexBuffer used by an indirect draw
*/
class VertexArray
{
public:
    VertexArray(VulkanMgr &master, int alignment);
    ~VertexArray() = default;

    // Create a binding entry which match a VertexBuffer binding
    bool createBindingEntry(uint32_t stride, VkVertexInputRate type = VK_VERTEX_INPUT_RATE_VERTEX);
    // Add input for the last binding entry created
    bool addInput(VkFormat format, bool used = true);
    VertexBuffer *createBuffer(int binding, int vertexCount, BufferMgr *vertexMgr = nullptr, BufferMgr *instanceMgr = nullptr);
    //! Internally used
    const std::vector<VkVertexInputBindingDescription> &getVertexBindingDesc() {return bindingDesc;}
    //! Internally used
    const std::vector<VkVertexInputAttributeDescription> &getVertexAttributeDesc() {return attributeDesc;}
private:
    VulkanMgr &master;
    std::vector<VkVertexInputBindingDescription> bindingDesc;
    std::vector<VkVertexInputAttributeDescription> attributeDesc;
    uint32_t binding = UINT32_MAX;
    int size = 0;
    uint32_t location = 0;
    uint32_t offset = 0;
    const int alignment;
};

#endif /* end of include guard: VERTEX_ARRAY_HPP */

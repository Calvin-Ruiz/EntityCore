#ifndef VERTEX_ARRAY_HPP
#define VERTEX_ARRAY_HPP

#include <memory>
#include <vector>
#include <array>
#include <vulkan/vulkan.h>
#include <cstring>

class VulkanMgr;
class VertexBuffer;

/*
* A VertexBuffer layout to facilitate binding in pipeline
*/
class VertexArray
{
public:
    VertexArray(VulkanMgr &master);
    ~VertexArray() = default;

    
    //! Internally used
    const std::vector<VkVertexInputBindingDescription> &getVertexBindingDesc() {return bindingDesc;}
    //! Internally used
    const std::vector<VkVertexInputAttributeDescription> &getVertexAttributeDesc() {return attributeDesc;}
private:
    VulkanMgr &master;
    std::vector<VkVertexInputBindingDescription> bindingDesc;
    std::vector<VkVertexInputAttributeDescription> attributeDesc;
};

#endif /* end of include guard: VERTEX_ARRAY_HPP */

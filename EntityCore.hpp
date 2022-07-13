// Regroup include of every highly-used resources of EntityCore
// Note that including this file may increase compilation time

#include "EntityCore/Core/VulkanMgr.hpp"
#include "EntityCore/Core/BufferMgr.hpp"
#include "EntityCore/Core/FrameMgr.hpp"

#include "EntityCore/Resource/ComputePipeline.hpp"
#include "EntityCore/Resource/Pipeline.hpp"
#include "EntityCore/Resource/PipelineLayout.hpp"
#include "EntityCore/Resource/Set.hpp"
#include "EntityCore/Resource/SyncEvent.hpp"
#include "EntityCore/Resource/Texture.hpp"
#include "EntityCore/Resource/VertexArray.hpp"
#include "EntityCore/Resource/VertexBuffer.hpp"
#include "EntityCore/Resource/TransferMgr.hpp"
#include "EntityCore/Resource/SharedBuffer.hpp"
#include "EntityCore/Resource/Collector.hpp"

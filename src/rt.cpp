module;
#include <cstdint>
#include <memory>
#include <optional>
#include <stdexcept>
#include <vector>
#include <cstring>
module evk;
import :rt;
using namespace evk::rt;

SBT::SBT(
	const evk::SharedPtr<Device>& device,
	const std::vector<GeneralGroup>& rgen,
	const std::vector<GeneralGroup>& miss,
	const std::vector<HitGroup>& hit,
	const std::vector<GeneralGroup>& callable
) : missEntries{ 0 }, hitEntries{ 0 }, callableEntries{ 0 }, sizeInBytes{ 0 }
{
	shaderGroupCreateInfos.reserve(rgen.size() + miss.size() + hit.size() + callable.size());
	// each entry
	const uint32_t shaderGroupHandleSize = device->rayTracingPipelineProperties.shaderGroupHandleSize;
	const uint32_t shaderGroupHandleAlignment = device->rayTracingPipelineProperties.shaderGroupHandleAlignment;
	const uint32_t shaderGroupHandleSizeAligned = utils::roundUpToMultipleOfPowerOf2(shaderGroupHandleSize, shaderGroupHandleAlignment);
	// each group
	const uint32_t shaderGroupBaseAlignment = device->rayTracingPipelineProperties.shaderGroupBaseAlignment;
	const uint32_t shaderGroupBaseAligned = utils::roundUpToMultipleOfPowerOf2(shaderGroupHandleSizeAligned, shaderGroupBaseAlignment);

	// rgen
	// each entry aligned with shaderGroupBaseAligned
	{
		uint32_t offset = 0;
		rgenRegions.reserve(rgen.size());
		for (const auto& g : rgen) {
			rgenRegions.emplace_back(offset, shaderGroupBaseAligned, shaderGroupBaseAligned);
			shaderGroupCreateInfos.emplace_back(vk::RayTracingShaderGroupTypeKHR::eGeneral);
			shaderGroupCreateInfos.back().setGeneralShader(g.raygen_miss_callable);
			offset += shaderGroupBaseAligned;
		}
		sizeInBytes += offset;
	}
	// miss
	// each entry aligned with shaderGroupHandleSizeAligned
	{
		missEntries = miss.size();
		const uint32_t size = missEntries ? utils::roundUpToMultipleOfPowerOf2(missEntries * shaderGroupHandleSizeAligned, shaderGroupBaseAlignment) : 0u;
		for (const auto& g : miss) {
			shaderGroupCreateInfos.emplace_back(vk::RayTracingShaderGroupTypeKHR::eGeneral);
			shaderGroupCreateInfos.back().setGeneralShader(g.raygen_miss_callable);
		}
		missRegion = vk::StridedDeviceAddressRegionKHR{ sizeInBytes, shaderGroupHandleSizeAligned, size };
		sizeInBytes += size;
	}
	// hit
	// each entry aligned with shaderGroupHandleSizeAligned
	{
		hitEntries = hit.size();
		const uint32_t size = hitEntries ? utils::roundUpToMultipleOfPowerOf2(hitEntries * shaderGroupHandleSizeAligned, shaderGroupBaseAlignment) : 0u;
		for (const auto& g : hit) {
			shaderGroupCreateInfos.emplace_back(static_cast<vk::RayTracingShaderGroupTypeKHR>(g.type));
			shaderGroupCreateInfos.back().setClosestHitShader(g.closestHit).setAnyHitShader(g.anyHit).setIntersectionShader(g.intersection);
		}
		hitRegion = vk::StridedDeviceAddressRegionKHR{ sizeInBytes, shaderGroupHandleSizeAligned, size };
		sizeInBytes += size;
	}
	// callable
	// each entry aligned with shaderGroupHandleSizeAligned
	{
		callableEntries = callable.size();
		const uint32_t size = callableEntries ? utils::roundUpToMultipleOfPowerOf2(callableEntries * shaderGroupHandleSizeAligned, shaderGroupBaseAlignment) : 0u;
		for (const auto& g : callable) {
			shaderGroupCreateInfos.emplace_back(vk::RayTracingShaderGroupTypeKHR::eGeneral);
			shaderGroupCreateInfos.back().setGeneralShader(g.raygen_miss_callable);
		}
		callableRegion = vk::StridedDeviceAddressRegionKHR{ sizeInBytes, shaderGroupHandleSizeAligned, size };
		sizeInBytes += size;
	}
}

TriangleGeometry::TriangleGeometry() : data{ vk::AccelerationStructureGeometryTrianglesDataKHR{}.setIndexType(vk::IndexType::eNoneKHR) },
hasIndices{ false }, triangleCount{ 0 }, indexBufferMemoryByteOffset{ 0 }, vertexBufferMemoryByteOffset{ 0 }, transformBufferMemoryByteOffset{ 0 } {}

// memoryOffset describes the start of the data in bytes from the beginning of the corresponding buffer
TriangleGeometry& TriangleGeometry::setVertices(
	const vk::DeviceOrHostAddressConstKHR address, 
	const vk::Format vertexFormat, 
	const uint32_t vertexCount, 
	const uint32_t memoryByteOffset, uint32_t stride
) {
	if (stride == 0) {
		if (vertexFormat == vk::Format::eR16G16B16Sfloat) stride = 6;
		else if (vertexFormat == vk::Format::eR16G16B16A16Sfloat) stride = 8;
		else if (vertexFormat == vk::Format::eR32G32B32Sfloat) stride = 12;
		else if (vertexFormat == vk::Format::eR32G32B32A32Sfloat) stride = 16;
		else if (vertexFormat == vk::Format::eR64G64B64Sfloat) stride = 24;
		else if (vertexFormat == vk::Format::eR64G64B64A64Sfloat) stride = 32;
		else throw std::runtime_error{ "Stride must be specified for this vertex format" };
	}

	data.vertexData = address;
	data.vertexFormat = vertexFormat;
	data.maxVertex = vertexCount;
	data.vertexStride = stride;
	// only set triangle count when no indices are set YET
	if (data.indexType == vk::IndexType::eNoneKHR) {
		if (vertexCount % 3 == 0) triangleCount = vertexCount / 3;
	}
	vertexBufferMemoryByteOffset = memoryByteOffset;
	return *this;
}

TriangleGeometry& TriangleGeometry::setIndices(
	const vk::DeviceOrHostAddressConstKHR address,
	const vk::IndexType indexType,
	const uint32_t indexCount,
	const uint32_t memoryByteOffset
) {
	data.indexData = address;
	data.indexType = indexType;
	if (indexCount % 3 != 0) throw std::runtime_error{ "Only geometries with triangle topology are supported for acceleration structure creation" };
	triangleCount = indexCount / 3;
	indexBufferMemoryByteOffset = memoryByteOffset;
	hasIndices = true;
	return *this;
}

TriangleGeometry& TriangleGeometry::setTransform(
	const vk::DeviceOrHostAddressConstKHR address, 
	const uint32_t memoryByteOffset
) {
	data.transformData = address;
	transformBufferMemoryByteOffset = memoryByteOffset;
	return *this;
}

AabbGeometry& AabbGeometry::setAABBsFromDevice(
	const vk::DeviceOrHostAddressConstKHR address,
	const uint32_t aabbCount,
	const uint32_t memoryByteOffset,
	const uint32_t stride
) {
	data.data = address;
	data.stride = stride;
	_aabbCount = aabbCount;
	_memoryByteOffset = memoryByteOffset;
	return *this;
}

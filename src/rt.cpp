module;
#include <cstdint>
#include <optional>
#include <stdexcept>
#include <vector>
module evk;
import :rt;
using namespace evk::rt;

SBT::SBT(
	const std::shared_ptr<Device>& device,
	const std::vector<GeneralGroup>& rgen,
	const std::vector<GeneralGroup>& miss,
	const std::vector<HitGroup>& hit,
	const std::vector<GeneralGroup>& callable
)
{
	shaderGroupCreateInfos.reserve(rgen.size() + miss.size() + hit.size() + callable.size());
	// each entry
	const uint32_t shaderGroupHandleSize = device->rayTracingPipelineProperties.shaderGroupHandleSize;
	const uint32_t shaderGroupHandleAlignment = device->rayTracingPipelineProperties.shaderGroupHandleAlignment;
	const uint32_t shaderGroupHandleSizeAligned = utils::roundUpToMultipleOfPowerOf2(shaderGroupHandleSize, shaderGroupHandleAlignment);
	// each group
	const uint32_t shaderGroupBaseAlignment = device->rayTracingPipelineProperties.shaderGroupBaseAlignment;
	const uint32_t shaderGroupBaseAligned = utils::roundUpToMultipleOfPowerOf2(shaderGroupHandleSizeAligned, shaderGroupBaseAlignment);

	sizeInBytes = 0;
	// rgen
	// each entry aligned with shaderGroupBaseAlignment
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
		const uint32_t entries = miss.size();
		const uint32_t size = entries ? utils::roundUpToMultipleOfPowerOf2(entries * shaderGroupHandleSizeAligned, shaderGroupBaseAlignment) : 0u;
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
		const uint32_t entries = hit.size();
		const uint32_t size = entries ? utils::roundUpToMultipleOfPowerOf2(entries * shaderGroupHandleSizeAligned, shaderGroupBaseAlignment) : 0u;
		for (const auto& g : hit) {
			shaderGroupCreateInfos.emplace_back(vk::RayTracingShaderGroupTypeKHR::eGeneral);
			shaderGroupCreateInfos.back().setClosestHitShader(g.closestHit).setAnyHitShader(g.anyHit).setIntersectionShader(g.intersection);
		}
		hitRegion = vk::StridedDeviceAddressRegionKHR{ sizeInBytes, shaderGroupHandleSizeAligned, size };
		sizeInBytes += size;
	}
	// callable
	// each entry aligned with shaderGroupHandleSizeAligned
	{
		const uint32_t entries = callable.size();
		const uint32_t size = entries ? utils::roundUpToMultipleOfPowerOf2(entries * shaderGroupHandleSizeAligned, shaderGroupBaseAlignment) : 0u;
		for (const auto& g : callable) {
			shaderGroupCreateInfos.emplace_back(vk::RayTracingShaderGroupTypeKHR::eGeneral);
			shaderGroupCreateInfos.back().setGeneralShader(g.raygen_miss_callable);
		}
		callableRegion = vk::StridedDeviceAddressRegionKHR{ sizeInBytes, shaderGroupHandleSizeAligned, size };
		sizeInBytes += size;
	}
}

RayTracingPipeline::RayTracingPipeline(
	const std::shared_ptr<Device>& device,
	const ShaderModules& stages,
	const SBT& sbt,
	const std::vector<vk::PushConstantRange>& pcRanges,
	const ShaderSpecialization& specialization
) : Resource{ device }, _layout{ *dev, vk::PipelineLayoutCreateInfo{}.setPushConstantRanges(pcRanges) }, _pipeline{ nullptr },
_sbtBuffer{ device,  sbt.sizeInBytes, vk::BufferUsageFlagBits::eShaderBindingTableKHR, vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eDeviceLocal } {

	std::vector<vk::PipelineShaderStageCreateInfo> shaderStages{ stages.size() };
	for (auto i = 0; i < stages.size(); i++) {
		shaderStages[i].setStage(stages[i].first.get().stage).setModule(stages[i].first.get()).setPName(stages[i].second.data()).setPSpecializationInfo(&specialization.constInfo);
	}
	auto createInfo = vk::RayTracingPipelineCreateInfoKHR{}
		.setStages(shaderStages)
		.setLayout(_layout)
		.setMaxPipelineRayRecursionDepth(device->rayTracingPipelineProperties.maxRayRecursionDepth)
		.setGroups(sbt.shaderGroupCreateInfos);
	_pipeline = vk::raii::Pipeline{ *device, nullptr, nullptr, createInfo };

	{
		auto shaderHandleStorage = _pipeline.getRayTracingShaderGroupHandlesKHR<uint8_t>(0, sbt.shaderGroupCreateInfos.size(), sbt.sizeInBytes);
		std::memcpy(_sbtBuffer.memory.mapMemory(0, vk::WholeSize), shaderHandleStorage.data(), shaderHandleStorage.size());
		_sbtBuffer.memory.unmapMemory();

		rgenRegions = sbt.rgenRegions;
		missRegion = sbt.missRegion;
		hitRegion = sbt.hitRegion;
		callableRegion = sbt.callableRegion;

		for (auto& r : rgenRegions) r.deviceAddress += _sbtBuffer.deviceAddress;
		missRegion.deviceAddress += _sbtBuffer.deviceAddress;
		hitRegion.deviceAddress += _sbtBuffer.deviceAddress;
		callableRegion.deviceAddress += _sbtBuffer.deviceAddress;
	}
}

TriangleGeometry::TriangleGeometry() : triangleData{ vk::AccelerationStructureGeometryTrianglesDataKHR{}.setIndexType(vk::IndexType::eNoneKHR) },
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

	triangleData.vertexData = address;
	triangleData.vertexFormat = vertexFormat;
	triangleData.maxVertex = vertexCount;
	triangleData.vertexStride = stride;
	// only set triangle count when no indices are set YET
	if (triangleData.indexType == vk::IndexType::eNoneKHR) {
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
	triangleData.indexData = address;
	triangleData.indexType = indexType;
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
	triangleData.transformData = address;
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

module;
#include <memory>
#include <vector>
#include <optional>
#include <stdexcept>
#include <cstring>
export module evk:rt;
import :core;
import :utils;
import vulkan_hpp;

export namespace evk::rt {
	struct SBT : Resource {
		struct GeneralGroup // raygen | miss | callable
		{
			GeneralGroup(const uint32_t raygen_miss_callable) : raygen_miss_callable{ raygen_miss_callable } {}
			uint32_t raygen_miss_callable;
		};
		struct HitGroup
		{
            enum Type
            {
                Triangles = vk::RayTracingShaderGroupTypeKHR::eTrianglesHitGroup,
                Procedural = vk::RayTracingShaderGroupTypeKHR::eProceduralHitGroup
            };
			HitGroup(const Type type, const std::optional<uint32_t> closestHit, const std::optional<uint32_t> anyHit = {}, const std::optional<uint32_t> intersection = {}) : type{ type },
				closestHit{ closestHit.value_or(vk::ShaderUnusedKHR) }, anyHit{ anyHit.value_or(vk::ShaderUnusedKHR) }, intersection{ intersection.value_or(vk::ShaderUnusedKHR) } {}
            Type type;
		    uint32_t closestHit, anyHit, intersection;
		};

		struct GroupInfo { uint32_t byteOffset, byteSize, entries, entriesOffset; };

		EVK_API SBT(
			const evk::SharedPtr<Device>& device,
			const std::vector<GeneralGroup>& rgen,
			const std::vector<GeneralGroup>& miss,
			const std::vector<HitGroup>& hit,
			const std::vector<GeneralGroup>& callable = {}
		);

		std::vector<vk::StridedDeviceAddressRegionKHR> rgenRegions;
		vk::StridedDeviceAddressRegionKHR missRegion;
		vk::StridedDeviceAddressRegionKHR hitRegion;
		vk::StridedDeviceAddressRegionKHR callableRegion;
		size_t sizeInBytes;
		std::vector<vk::RayTracingShaderGroupCreateInfoKHR> shaderGroupCreateInfos;
	};

	struct RayTracingPipeline : Resource
	{
		EVK_API RayTracingPipeline(
			const evk::SharedPtr<Device>& device,
			const ShaderModules& stages,
			const SBT& sbt,
			const std::vector<vk::PushConstantRange>& pcRanges = {},
			const ShaderSpecialization& specialization = {}
		);

		vk::raii::PipelineLayout _layout;
		vk::raii::Pipeline _pipeline;
		evk::Buffer _sbtBuffer;
		// regions in _sbtBuffer
		std::vector<vk::StridedDeviceAddressRegionKHR> rgenRegions;
		vk::StridedDeviceAddressRegionKHR missRegion;
		vk::StridedDeviceAddressRegionKHR hitRegion;
		vk::StridedDeviceAddressRegionKHR callableRegion;
	};

	using Transform = vk::TransformMatrixKHR;
	using AABB = vk::AabbPositionsKHR;
	EVK_API const Transform identityMatrix{
		std::array<std::array<float, 4>, 3>{
			std::array<float, 4>{1.0f, 0.0f, 0.0f, 0.0f},
			std::array<float, 4>{0.0f, 1.0f, 0.0f, 0.0f},
			std::array<float, 4>{0.0f, 0.0f, 1.0f, 0.0f}
		}
	};

	struct TriangleGeometry {
		EVK_API TriangleGeometry();
		EVK_API TriangleGeometry& setVertices(
			vk::DeviceOrHostAddressConstKHR address,
			vk::Format vertexFormat, // only eR16G16B16A16Sfloat, eR32G32B32Sfloat, eR32G32B32A32Sfloat, eR64G64B64Sfloat, eR64G64B64A64Sfloat are supported
			uint32_t vertexCount,
			uint32_t memoryByteOffset = 0, // describes the start of the data in bytes from the beginning of the corresponding buffer
			uint32_t stride = 0
		);

		EVK_API TriangleGeometry& setIndices(
			vk::DeviceOrHostAddressConstKHR address,
			vk::IndexType indexType,
			uint32_t indexCount,
			uint32_t memoryByteOffset = 0
		);

		EVK_API TriangleGeometry& setTransform( // of VkTransformMatrixKHR { float matrix[3][4]; }
			vk::DeviceOrHostAddressConstKHR address,
			uint32_t memoryByteOffset
		);

		vk::AccelerationStructureGeometryTrianglesDataKHR data;
		bool hasIndices;
		uint32_t triangleCount, indexBufferMemoryByteOffset, vertexBufferMemoryByteOffset, transformBufferMemoryByteOffset;
	};

	struct AabbGeometry {
		EVK_API AabbGeometry& setAABBsFromDevice(
			vk::DeviceOrHostAddressConstKHR address,
			uint32_t aabbCount = 1,
			uint32_t memoryByteOffset = 0,
			uint32_t stride = 24
		);

	    vk::AccelerationStructureGeometryAabbsDataKHR data;
	    uint32_t _aabbCount, _memoryByteOffset;
	};

	using AsInstanceGeometry = vk::AccelerationStructureInstanceKHR;

	struct InternalOrExternalBuffer
	{
		SharedPtr<evk::Buffer> buffer;
		vk::DeviceSize offset = 0;
		operator bool() const { return buffer; }
	};

	struct BottomLevelAccelerationStructure
	{
		EVK_API BottomLevelAccelerationStructure() : accelerationStructure { nullptr }, deviceAddress{ 0 } {}
		EVK_API BottomLevelAccelerationStructure(
			const evk::SharedPtr<evk::Device>& device,
			const std::vector<std::pair<TriangleGeometry, vk::GeometryFlagsKHR>>& geometries,
			const vk::BuildAccelerationStructureFlagsKHR buildFlags = vk::BuildAccelerationStructureFlagBitsKHR::ePreferFastTrace
		) : device{ device }, _primitives( geometries.size() ), _geos { geometries.size() }, _ranges{ geometries.size() }, accelerationStructure{ nullptr }, deviceAddress{ 0 }
		{
			for (size_t i = 0; i < geometries.size(); i++)
			{
				const auto& g = geometries[i].first;
				_geos[i].setGeometryType(vk::GeometryTypeKHR::eTriangles)
					.setGeometry(g.data)
					.setFlags(geometries[i].second);

				_primitives[i] = g.triangleCount;

				_ranges[i].setTransformOffset(g.transformBufferMemoryByteOffset).setPrimitiveCount(g.triangleCount);
				if (g.hasIndices) {
					_ranges[i].setPrimitiveOffset(g.indexBufferMemoryByteOffset).setFirstVertex(g.vertexBufferMemoryByteOffset / g.data.vertexStride);
				}
				else {
					_ranges[i].setPrimitiveOffset(g.vertexBufferMemoryByteOffset);
				}
			}

			_asBuildGeoInfo = vk::AccelerationStructureBuildGeometryInfoKHR{}.setType(vk::AccelerationStructureTypeKHR::eBottomLevel)
				.setFlags(buildFlags).setGeometries(_geos);
		}

		EVK_API BottomLevelAccelerationStructure(
			const evk::SharedPtr<evk::Device>& device,
			const std::vector<std::pair<AabbGeometry, vk::GeometryFlagsKHR>>& geometries,
			const vk::BuildAccelerationStructureFlagsKHR buildFlags = vk::BuildAccelerationStructureFlagBitsKHR::ePreferFastTrace
		) : device{ device }, _primitives(geometries.size()), _geos{ geometries.size() }, _ranges{ geometries.size() }, accelerationStructure{ nullptr }, deviceAddress{ 0 }
		{
			for (size_t i = 0; i < geometries.size(); i++)
			{
				const auto& g = geometries[i].first;
				_geos[i].setGeometryType(vk::GeometryTypeKHR::eAabbs)
					.setGeometry(g.data)
					.setFlags(geometries[i].second);

				_primitives[i] = g._aabbCount;

				_ranges[i].setPrimitiveCount(g._aabbCount).setPrimitiveOffset(g._memoryByteOffset);
				
			}

			_asBuildGeoInfo = vk::AccelerationStructureBuildGeometryInfoKHR{}.setType(vk::AccelerationStructureTypeKHR::eBottomLevel)
				.setFlags(buildFlags).setGeometries(_geos).setMode(vk::BuildAccelerationStructureModeKHR::eBuild);
		}

        const vk::AccelerationStructureBuildSizesInfoKHR& updateSizeInfo()
		{
			buildSizesInfo = device->getAccelerationStructureBuildSizesKHR(vk::AccelerationStructureBuildTypeKHR::eDevice, _asBuildGeoInfo, _primitives);
			return buildSizesInfo;
		}

		void cmdBuild(const vk::raii::CommandBuffer& cb)
	    {
			if (!buildSizesInfo.accelerationStructureSize) updateSizeInfo();
			if (!scratchBuffer)
			{
				scratchBuffer = { evk::make_shared<evk::Buffer>(device, buildSizesInfo.buildScratchSize, vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eShaderDeviceAddress, vk::MemoryPropertyFlagBits::eDeviceLocal), 0 };
			}
            if (!accelerationStructureBuffer)
            {
				accelerationStructureBuffer = { evk::make_shared<evk::Buffer>(device, buildSizesInfo.accelerationStructureSize, vk::BufferUsageFlagBits::eAccelerationStructureStorageKHR | vk::BufferUsageFlagBits::eShaderDeviceAddress, vk::MemoryPropertyFlagBits::eDeviceLocal), 0 };
			}
            if (accelerationStructureBuffer.offset % 256 != 0) throw std::runtime_error("Acceleration structure buffer offset must be a multiple of 256");

	        if (!*accelerationStructure){
	            const auto accelerationStructureInfo = vk::AccelerationStructureCreateInfoKHR{}
	                .setCreateFlags(vk::AccelerationStructureCreateFlagsKHR{})
	                .setBuffer(*accelerationStructureBuffer.buffer->buffer)
	                .setOffset(accelerationStructureBuffer.offset)
	                .setSize(buildSizesInfo.accelerationStructureSize)
	                .setType(vk::AccelerationStructureTypeKHR::eBottomLevel);
	            accelerationStructure = device->createAccelerationStructureKHR(accelerationStructureInfo);
	            deviceAddress = device->getAccelerationStructureAddressKHR(vk::AccelerationStructureDeviceAddressInfoKHR{}.setAccelerationStructure(*accelerationStructure));
	        }

	        _asBuildGeoInfo
		        .setDstAccelerationStructure(*accelerationStructure)
		        .setScratchData(scratchBuffer.buffer->deviceAddress + scratchBuffer.offset);
	        cb.buildAccelerationStructuresKHR(_asBuildGeoInfo, { _ranges.data() });
			_asBuildGeoInfo.setSrcAccelerationStructure(*accelerationStructure);
			_asBuildGeoInfo.setMode(vk::BuildAccelerationStructureModeKHR::eUpdate);
	    }

		evk::SharedPtr<evk::Device> device;
		std::vector<uint32_t> _primitives;
		std::vector<vk::AccelerationStructureGeometryKHR> _geos;
		std::vector<vk::AccelerationStructureBuildRangeInfoKHR> _ranges;
		vk::AccelerationStructureBuildGeometryInfoKHR _asBuildGeoInfo;
		vk::AccelerationStructureBuildSizesInfoKHR buildSizesInfo;

		InternalOrExternalBuffer scratchBuffer;
		InternalOrExternalBuffer accelerationStructureBuffer;

	    vk::raii::AccelerationStructureKHR accelerationStructure;
	    vk::DeviceAddress deviceAddress;

	};
	
	struct TopLevelAccelerationStructure
	{
		EVK_API TopLevelAccelerationStructure() : accelerationStructure{ nullptr }, deviceAddress{ 0 } {}
		EVK_API TopLevelAccelerationStructure(
	        const evk::SharedPtr<evk::Device>& device,
	        const vk::raii::CommandBuffer& cb,
	        const std::vector<AsInstanceGeometry>& instances,
	        const vk::GeometryFlagsKHR flags = {},
	        const vk::BuildAccelerationStructureFlagsKHR buildFlags = vk::BuildAccelerationStructureFlagBitsKHR::ePreferFastTrace)
	        : accelerationStructure{ nullptr }
	    {
	        { // prepare instance buffer
	            size_t instanceBufferSize = instances.size() * sizeof(vk::AccelerationStructureInstanceKHR);
	            instanceBuffer = std::make_unique<evk::Buffer>(device, instanceBufferSize, vk::BufferUsageFlagBits::eAccelerationStructureBuildInputReadOnlyKHR | vk::BufferUsageFlagBits::eShaderDeviceAddress,
	                vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eDeviceLocal);
	            void* ptr = instanceBuffer->memory.mapMemory(0, instanceBufferSize);
	            std::memcpy(ptr, instances.data(), instanceBufferSize);
	            instanceBuffer->memory.unmapMemory();
	        }

	        auto asInstanceData = vk::AccelerationStructureGeometryInstancesDataKHR{}.setData(instanceBuffer->deviceAddress);
	        auto asInstanceGeometry = vk::AccelerationStructureGeometryKHR{}
	            .setGeometryType(vk::GeometryTypeKHR::eInstances)
	            .setGeometry(asInstanceData)
	            .setFlags(flags);


	        auto asBuildGeoInfo = vk::AccelerationStructureBuildGeometryInfoKHR{}
	            .setType(vk::AccelerationStructureTypeKHR::eTopLevel)
	            .setFlags(buildFlags)
	            .setMode(vk::BuildAccelerationStructureModeKHR::eBuild)
	            .setGeometries(asInstanceGeometry); // must be 1 for top

	        // prepare aux buffers
	        auto asBuildSizeInfo = device->getAccelerationStructureBuildSizesKHR(
	            vk::AccelerationStructureBuildTypeKHR::eDevice, asBuildGeoInfo, static_cast<uint32_t>(instances.size()));
	        scratchBuffer = std::make_unique<evk::Buffer>(device, asBuildSizeInfo.buildScratchSize, vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eShaderDeviceAddress, vk::MemoryPropertyFlagBits::eDeviceLocal);
	        accelerationStructureBuffer = std::make_unique<evk::Buffer>(device, asBuildSizeInfo.accelerationStructureSize, vk::BufferUsageFlagBits::eAccelerationStructureStorageKHR | vk::BufferUsageFlagBits::eShaderDeviceAddress, vk::MemoryPropertyFlagBits::eDeviceLocal);

	        {
	            const auto accelerationStructureInfo = vk::AccelerationStructureCreateInfoKHR{}
	                .setCreateFlags(vk::AccelerationStructureCreateFlagsKHR{})
	                .setBuffer(*accelerationStructureBuffer->buffer)
	                .setOffset(0)
	                .setSize(asBuildSizeInfo.accelerationStructureSize)
	                .setType(vk::AccelerationStructureTypeKHR::eTopLevel);
	            accelerationStructure = device->createAccelerationStructureKHR(accelerationStructureInfo);
	            deviceAddress = device->getAccelerationStructureAddressKHR(vk::AccelerationStructureDeviceAddressInfoKHR{}.setAccelerationStructure(*accelerationStructure));
	        }

	        asBuildGeoInfo.setDstAccelerationStructure(*accelerationStructure).setScratchData(scratchBuffer->deviceAddress);
	        auto range = vk::AccelerationStructureBuildRangeInfoKHR{}.setPrimitiveCount(static_cast<uint32_t>(instances.size()));
	        cb.buildAccelerationStructuresKHR(asBuildGeoInfo, { &range });
	    }

		EVK_API void cleanup() { scratchBuffer = nullptr; }

	    std::unique_ptr<evk::Buffer> instanceBuffer;
	    std::unique_ptr<evk::Buffer> scratchBuffer;
	    std::unique_ptr<evk::Buffer> accelerationStructureBuffer;
	    vk::raii::AccelerationStructureKHR accelerationStructure;
	    vk::DeviceAddress deviceAddress;
	};
}

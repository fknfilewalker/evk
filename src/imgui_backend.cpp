module;
#include <imgui.h>
#include <stdexcept>
#include <memory>
#include <deque>
#include <string_view>
#include <vector>
module evk.imgui;
import evk;

using namespace evk;

namespace
{
	const std::vector<uint32_t> imgui_backend_shaders_spv = {
		0x07230203, 0x00010500, 0x00000028, 0x0000006b, 0x00000000, 0x00020011, 0x0000115a, 0x00020011, 0x000014e3, 
		0x00020011, 0x00000027, 0x00020011, 0x00000001, 0x0008000a, 0x5f565053, 0x5f52484b, 0x69726176, 0x656c6261, 
		0x696f705f, 0x7265746e, 0x00000073, 0x0009000a, 0x5f565053, 0x5f52484b, 0x73796870, 0x6c616369, 0x6f74735f, 
		0x65676172, 0x6675625f, 0x00726566, 0x0003000e, 0x000014e4, 0x00000001, 0x000b000f, 0x00000000, 0x00000002, 
		0x74726576, 0x614d7865, 0x00006e69, 0x00000018, 0x00000044, 0x00000047, 0x0000004b, 0x0000000e, 0x000b000f, 
		0x00000004, 0x0000004e, 0x67617266, 0x746e656d, 0x6e69614d, 0x00000000, 0x00000054, 0x0000005e, 0x0000005c, 
		0x00000057, 0x00030010, 0x0000004e, 0x00000007, 0x00030003, 0x0000000b, 0x00000001, 0x00040005, 0x00000005, 
		0x754f6e49, 0x00000074, 0x00060006, 0x00000005, 0x00000000, 0x69736f50, 0x6e6f6974, 0x00000000, 0x00050006, 
		0x00000005, 0x00000001, 0x6f6c6f43, 0x00000072, 0x00040006, 0x00000005, 0x00000002, 0x00005655, 0x00080005, 
		0x00000014, 0x68737550, 0x736e6f43, 0x746e6174, 0x74735f73, 0x30343164, 0x00000000, 0x00050006, 0x00000014, 
		0x00000000, 0x74726576, 0x00007865, 0x00050006, 0x00000014, 0x00000001, 0x6c616373, 0x00000065, 0x00060006, 
		0x00000014, 0x00000002, 0x6e617274, 0x74616c73, 0x00000065, 0x00030005, 0x00000018, 0x00006370, 0x000b0005, 
		0x00000047, 0x72746e65, 0x696f5079, 0x6150746e, 0x5f6d6172, 0x74726576, 0x614d7865, 0x432e6e69, 0x726f6c6f, 
		0x00000000, 0x000a0005, 0x0000004b, 0x72746e65, 0x696f5079, 0x6150746e, 0x5f6d6172, 0x74726576, 0x614d7865, 
		0x552e6e69, 0x00000056, 0x00050005, 0x00000002, 0x74726576, 0x614d7865, 0x00006e69, 0x00050005, 0x00000054, 
		0x78655473, 0x65727574, 0x00000000, 0x00050005, 0x00000057, 0x75706e69, 0x56552e74, 0x00000000, 0x00040005, 
		0x00000058, 0x706d6173, 0x0064656c, 0x00050005, 0x0000005c, 0x75706e69, 0x6f432e74, 0x00726f6c, 0x000a0005, 
		0x0000005e, 0x72746e65, 0x696f5079, 0x6150746e, 0x5f6d6172, 0x67617266, 0x746e656d, 0x6e69614d, 0x00000000, 
		0x00060005, 0x0000004e, 0x67617266, 0x746e656d, 0x6e69614d, 0x00000000, 0x00060005, 0x00000015, 0x74726556, 
		0x6e5f7865, 0x72757461, 0x00006c61, 0x00060006, 0x00000015, 0x00000000, 0x69736f70, 0x6e6f6974, 0x00000000, 
		0x00040006, 0x00000015, 0x00000001, 0x00007675, 0x00050006, 0x00000015, 0x00000002, 0x6f6c6f63, 0x00000072, 
		0x00050048, 0x00000005, 0x00000000, 0x00000023, 0x00000000, 0x00050048, 0x00000005, 0x00000001, 0x00000023, 
		0x00000010, 0x00050048, 0x00000005, 0x00000002, 0x00000023, 0x00000020, 0x00040047, 0x0000000e, 0x0000000b, 
		0x0000002a, 0x00040047, 0x00000016, 0x00000006, 0x00000014, 0x00030047, 0x00000014, 0x00000002, 0x00050048, 
		0x00000014, 0x00000000, 0x00000023, 0x00000000, 0x00050048, 0x00000014, 0x00000001, 0x00000023, 0x00000008, 
		0x00050048, 0x00000014, 0x00000002, 0x00000023, 0x00000010, 0x00040047, 0x00000021, 0x00000006, 0x00000004, 
		0x00040047, 0x0000002d, 0x00000006, 0x00000008, 0x00040047, 0x00000044, 0x0000000b, 0x00000000, 0x00040047, 
		0x00000047, 0x0000001e, 0x00000000, 0x00040047, 0x0000004b, 0x0000001e, 0x00000001, 0x00040047, 0x00000054, 
		0x00000021, 0x00000000, 0x00040047, 0x00000054, 0x00000022, 0x00000000, 0x00040047, 0x00000057, 0x0000001e, 
		0x00000001, 0x00040047, 0x0000005c, 0x0000001e, 0x00000000, 0x00040047, 0x0000005e, 0x0000001e, 0x00000000, 
		0x00050048, 0x00000015, 0x00000000, 0x00000023, 0x00000000, 0x00050048, 0x00000015, 0x00000001, 0x00000023, 
		0x00000008, 0x00050048, 0x00000015, 0x00000002, 0x00000023, 0x00000010, 0x00020013, 0x00000001, 0x00030021, 
		0x00000003, 0x00000001, 0x00030016, 0x00000006, 0x00000020, 0x00040017, 0x00000007, 0x00000006, 0x00000004, 
		0x00040017, 0x00000008, 0x00000006, 0x00000002, 0x0005001e, 0x00000005, 0x00000007, 0x00000007, 0x00000008, 
		0x00040020, 0x00000009, 0x00000007, 0x00000005, 0x00040015, 0x0000000b, 0x00000020, 0x00000001, 0x00040020, 
		0x0000000d, 0x00000001, 0x0000000b, 0x00040015, 0x0000000f, 0x00000020, 0x00000000, 0x0004002b, 0x0000000b, 
		0x00000011, 0x00000001, 0x00040020, 0x00000012, 0x00000007, 0x00000007, 0x00030027, 0x00000016, 0x000014e5, 
		0x0005001e, 0x00000014, 0x00000016, 0x00000008, 0x00000008, 0x00040020, 0x00000017, 0x00000009, 0x00000014, 
		0x0004002b, 0x0000000b, 0x00000019, 0x00000000, 0x00040020, 0x0000001a, 0x00000009, 0x00000016, 0x0004002b, 
		0x0000000b, 0x0000001e, 0x00000002, 0x00040015, 0x0000001f, 0x00000008, 0x00000000, 0x00040017, 0x00000020, 
		0x0000001f, 0x00000004, 0x00040020, 0x00000021, 0x000014e5, 0x00000020, 0x0004002b, 0x00000006, 0x00000026, 
		0x437f0000, 0x00040020, 0x00000029, 0x00000007, 0x00000008, 0x00040020, 0x0000002d, 0x000014e5, 0x00000008, 
		0x00040020, 0x00000036, 0x00000009, 0x00000008, 0x0004002b, 0x00000006, 0x0000003e, 0x00000000, 0x0004002b, 
		0x00000006, 0x0000003f, 0x3f800000, 0x00040020, 0x00000043, 0x00000003, 0x00000007, 0x00040020, 0x0000004a, 
		0x00000003, 0x00000008, 0x00090019, 0x00000050, 0x00000006, 0x00000001, 0x00000002, 0x00000000, 0x00000000, 
		0x00000001, 0x00000000, 0x0003001b, 0x00000051, 0x00000050, 0x00040020, 0x00000053, 0x00000000, 0x00000051, 
		0x00040020, 0x00000056, 0x00000001, 0x00000008, 0x00040020, 0x0000005b, 0x00000001, 0x00000007, 0x0005001e, 
		0x00000015, 0x00000008, 0x00000008, 0x00000020, 0x00040020, 0x00000016, 0x000014e5, 0x00000015, 0x0004003b, 
		0x0000000d, 0x0000000e, 0x00000001, 0x0004003b, 0x00000017, 0x00000018, 0x00000009, 0x0004003b, 0x00000043, 
		0x00000044, 0x00000003, 0x0004003b, 0x00000043, 0x00000047, 0x00000003, 0x0004003b, 0x0000004a, 0x0000004b, 
		0x00000003, 0x0004003b, 0x00000053, 0x00000054, 0x00000000, 0x0004003b, 0x00000056, 0x00000057, 0x00000001, 
		0x0004003b, 0x0000005b, 0x0000005c, 0x00000001, 0x0004003b, 0x00000043, 0x0000005e, 0x00000003, 0x0007002c, 
		0x00000007, 0x00000068, 0x00000026, 0x00000026, 0x00000026, 0x00000026, 0x0004002b, 0x00000006, 0x00000069, 
		0x3b808081, 0x0007002c, 0x00000007, 0x0000006a, 0x00000069, 0x00000069, 0x00000069, 0x00000069, 0x00050036, 
		0x00000001, 0x00000002, 0x00000000, 0x00000003, 0x000200f8, 0x00000004, 0x0004003b, 0x00000029, 0x00000063, 
		0x00000007, 0x0004003b, 0x00000012, 0x00000062, 0x00000007, 0x0004003b, 0x00000012, 0x00000061, 0x00000007, 
		0x0004003d, 0x0000000b, 0x0000000c, 0x0000000e, 0x0004007c, 0x0000000f, 0x00000010, 0x0000000c, 0x00050041, 
		0x0000001a, 0x0000001b, 0x00000018, 0x00000019, 0x0004003d, 0x00000016, 0x0000001c, 0x0000001b, 0x00050043, 
		0x00000016, 0x0000001d, 0x0000001c, 0x00000010, 0x00050041, 0x00000021, 0x00000022, 0x0000001d, 0x0000001e, 
		0x0006003d, 0x00000020, 0x00000023, 0x00000022, 0x00000002, 0x00000001, 0x00040070, 0x00000007, 0x00000024, 
		0x00000023, 0x00050085, 0x00000007, 0x00000027, 0x00000024, 0x0000006a, 0x0003003e, 0x00000062, 0x00000027, 
		0x0004003d, 0x00000016, 0x0000002b, 0x0000001b, 0x00050043, 0x00000016, 0x0000002c, 0x0000002b, 0x00000010, 
		0x00050041, 0x0000002d, 0x0000002e, 0x0000002c, 0x00000011, 0x0006003d, 0x00000008, 0x0000002f, 0x0000002e, 
		0x00000002, 0x00000004, 0x0003003e, 0x00000063, 0x0000002f, 0x0004003d, 0x00000016, 0x00000032, 0x0000001b, 
		0x00050043, 0x00000016, 0x00000033, 0x00000032, 0x00000010, 0x00050041, 0x0000002d, 0x00000034, 0x00000033, 
		0x00000019, 0x0006003d, 0x00000008, 0x00000035, 0x00000034, 0x00000002, 0x00000004, 0x00050041, 0x00000036, 
		0x00000037, 0x00000018, 0x00000011, 0x0004003d, 0x00000008, 0x00000038, 0x00000037, 0x00050085, 0x00000008, 
		0x00000039, 0x00000035, 0x00000038, 0x00050041, 0x00000036, 0x0000003a, 0x00000018, 0x0000001e, 0x0004003d, 
		0x00000008, 0x0000003b, 0x0000003a, 0x00050081, 0x00000008, 0x0000003c, 0x00000039, 0x0000003b, 0x00060050, 
		0x00000007, 0x0000003d, 0x0000003c, 0x0000003e, 0x0000003f, 0x0003003e, 0x00000061, 0x0000003d, 0x00060050, 
		0x00000005, 0x00000067, 0x0000003d, 0x00000027, 0x0000002f, 0x0003003e, 0x00000044, 0x0000003d, 0x0003003e, 
		0x00000047, 0x00000027, 0x0003003e, 0x0000004b, 0x0000002f, 0x000100fd, 0x00010038, 0x00050036, 0x00000001, 
		0x0000004e, 0x00000000, 0x00000003, 0x000200f8, 0x0000004f, 0x0004003d, 0x00000051, 0x00000052, 0x00000054, 
		0x0004003d, 0x00000008, 0x00000055, 0x00000057, 0x00060057, 0x00000007, 0x00000058, 0x00000052, 0x00000055, 
		0x00000000, 0x0004003d, 0x00000007, 0x0000005a, 0x0000005c, 0x00050085, 0x00000007, 0x0000005d, 0x0000005a, 
		0x00000058, 0x0003003e, 0x0000005e, 0x0000005d, 0x000100fd, 0x00010038, 
	};
}

ImGuiBackend::ImGuiBackend(
	const evk::SharedPtr<Device>& device,
	uint32_t imageCount
) : Resource{ device }, descriptorSetLayout{ device, {
        { { 0, vk::DescriptorType::eCombinedImageSampler, 1, vk::ShaderStageFlagBits::eFragment } }
    } }, descriptorSet{ device, descriptorSetLayout }, sampler{ nullptr }
{
	vertexBuffers.resize(imageCount);
	vertexBuffersPtr.resize(imageCount);
	indexBuffers.resize(imageCount);
	indexBuffersPtr.resize(imageCount);
	auto tmp = std::deque<std::unique_ptr<evk::Buffer>>();
	vertexBuffersToBeDeleted.resize(imageCount);
	indexBuffersToBeDeleted.resize(imageCount);
	{
		vk::SamplerCreateInfo samplerInfo{ {}, vk::Filter::eLinear, vk::Filter::eLinear, vk::SamplerMipmapMode::eLinear,
		vk::SamplerAddressMode::eRepeat, vk::SamplerAddressMode::eRepeat, vk::SamplerAddressMode::eRepeat };
		samplerInfo.setMinLod(-1000.0f).setMaxLod(1000.0f);
		sampler = vk::raii::Sampler{ *dev, samplerInfo };
	}

	constexpr vk::PushConstantRange pcRange{ vk::ShaderStageFlagBits::eVertex, 0, sizeof(vk::DeviceAddress) + sizeof(float) * 4u };
	shader = evk::ShaderObject{ device, {
		{ vk::ShaderStageFlagBits::eVertex, imgui_backend_shaders_spv, "vertexMain" },
		{ vk::ShaderStageFlagBits::eFragment, imgui_backend_shaders_spv, "fragmentMain" }
	}, { pcRange }, {}, { descriptorSetLayout.layout } };
}

void ImGuiBackend::setFont(const std::string_view filepath, const float scaleFactor)
{
	const auto& io = ImGui::GetIO();
	if (!filepath.empty()) {
		if (!io.Fonts->AddFontFromFileTTF(filepath.data(), scaleFactor)) {
			throw std::runtime_error("failed to load ImGui font");
		}
	}

	uint8_t* pixels;
	int32_t width, height;
	io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);

	fontImage = evk::Image{ dev, { static_cast<uint32_t>(width), static_cast<uint32_t>(height) }, vk::Format::eR8G8B8A8Unorm, vk::ImageTiling::eLinear,
		vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eHostTransferEXT, vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eDeviceLocal };
	fontImage.transitionLayout(vk::ImageLayout::eTransferDstOptimal);
	fontImage.copyMemoryToImage(pixels);
	fontImage.transitionLayout(vk::ImageLayout::eShaderReadOnlyOptimal);

	io.Fonts->SetTexID((ImTextureID)((vk::DescriptorSet::NativeType)descriptorSet.set)); // Store our identifier
	descriptorSet.setDescriptor(0, vk::DescriptorImageInfo{ *sampler, fontImage.imageView, vk::ImageLayout::eShaderReadOnlyOptimal });
	descriptorSet.update();
}

void ImGuiBackend::setContext(ImGuiContext* ctx) { ImGui::SetCurrentContext(ctx); }

void ImGuiBackend::render(const vk::raii::CommandBuffer& cb, const uint32_t imageIdx)
{
	ImGui::Render();

	const ImDrawData* draw_data = ImGui::GetDrawData();
	const int fb_width = static_cast<int>(draw_data->DisplaySize.x * draw_data->FramebufferScale.x);
	const int fb_height = static_cast<int>(draw_data->DisplaySize.y * draw_data->FramebufferScale.y);
	if (fb_width <= 0 || fb_height <= 0) return;

	size_t delete_history = 4;
    while (vertexBuffersToBeDeleted[imageIdx].size() > delete_history) vertexBuffersToBeDeleted[imageIdx].pop_front();
	while (indexBuffersToBeDeleted[imageIdx].size() > delete_history) indexBuffersToBeDeleted[imageIdx].pop_front();
	if (draw_data->TotalVtxCount > 0)
	{
		const size_t vertex_size = draw_data->TotalVtxCount * sizeof(ImDrawVert);
		const size_t index_size = draw_data->TotalIdxCount * sizeof(ImDrawIdx);
		if (!vertexBuffers[imageIdx] || vertexBuffers[imageIdx]->size < vertex_size) {
			vertexBuffersToBeDeleted[imageIdx].emplace_back(std::move(vertexBuffers[imageIdx]));
			vertexBuffers[imageIdx] = evk::Buffer::shared(dev, vertex_size, vk::BufferUsageFlagBits::eVertexBuffer | vk::BufferUsageFlagBits::eShaderDeviceAddress, vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eDeviceLocal);
			vertexBuffersPtr[imageIdx] = vertexBuffers[imageIdx]->memory.mapMemory(0, vk::WholeSize);
		}
		if (!indexBuffers[imageIdx] || indexBuffers[imageIdx]->size < index_size) {
			indexBuffersToBeDeleted[imageIdx].emplace_back(std::move(indexBuffers[imageIdx]));
			indexBuffers[imageIdx] = evk::Buffer::shared(dev, index_size, vk::BufferUsageFlagBits::eIndexBuffer, vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eDeviceLocal);
			indexBuffersPtr[imageIdx] = indexBuffers[imageIdx]->memory.mapMemory(0, vk::WholeSize);
		}

		// Upload vertex/index data into a single contiguous GPU buffer
		auto* vtx_dst = static_cast<ImDrawVert*>(vertexBuffersPtr[imageIdx]);
		auto* idx_dst = static_cast<ImDrawIdx*>(indexBuffersPtr[imageIdx]);
		for (int n = 0; n < draw_data->CmdListsCount; n++)
		{
			const ImDrawList* cmd_list = draw_data->CmdLists[n];
			memcpy(vtx_dst, cmd_list->VtxBuffer.Data, cmd_list->VtxBuffer.Size * sizeof(ImDrawVert));
			memcpy(idx_dst, cmd_list->IdxBuffer.Data, cmd_list->IdxBuffer.Size * sizeof(ImDrawIdx));
			vtx_dst += cmd_list->VtxBuffer.Size;
			idx_dst += cmd_list->IdxBuffer.Size;
		}
		/*const vk::MappedMemoryRange memoryRanges[2] = {
			{ vertexBuffers[imageIdx].memory, 0, vk::WholeSize},
			{indexBuffers[imageIdx].memory, 0, vk::WholeSize}
		};
		dev->flushMappedMemoryRanges(memoryRanges);*/

		cb.setPrimitiveTopologyEXT(vk::PrimitiveTopology::eTriangleList);
		cb.setPolygonModeEXT(vk::PolygonMode::eFill);
		cb.setCullModeEXT(vk::CullModeFlagBits::eNone);
		cb.setFrontFaceEXT(vk::FrontFace::eCounterClockwise);
		cb.setColorBlendEnableEXT(0, vk::True);
		cb.setColorBlendEquationEXT(0, vk::ColorBlendEquationEXT{
			vk::BlendFactor::eSrcAlpha, vk::BlendFactor::eOneMinusSrcAlpha, vk::BlendOp::eAdd,
			vk::BlendFactor::eOne, vk::BlendFactor::eOneMinusSrcAlpha, vk::BlendOp::eAdd,
		});
		cb.setColorWriteMaskEXT(0, vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG | vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA);
		cb.setDepthTestEnableEXT(vk::False);
		cb.setDepthWriteEnableEXT(vk::False);
		cb.setDepthBiasEnableEXT(vk::False);
		cb.setStencilTestEnableEXT(vk::False);
		cb.setSampleMaskEXT(vk::SampleCountFlagBits::e1, { 0xffffffff });
		cb.setRasterizationSamplesEXT(vk::SampleCountFlagBits::e1);
		cb.setRasterizerDiscardEnableEXT(vk::False);
		cb.setAlphaToCoverageEnableEXT(vk::False);
		cb.setPrimitiveRestartEnableEXT(vk::False);
		cb.setVertexInputEXT({}, {});

		// cb.setVertexInputEXT({ { 0, sizeof(ImDrawVert), vk::VertexInputRate::eVertex, 1 } }, {
		// 	{ 0, 0, vk::Format::eR32G32Sfloat, IM_OFFSETOF(ImDrawVert, pos) },
		// 	{ 1, 0, vk::Format::eR32G32Sfloat, IM_OFFSETOF(ImDrawVert, uv) },
		// 	{ 2, 0, vk::Format::eR8G8B8A8Unorm, IM_OFFSETOF(ImDrawVert, col) }
		// });

		cb.setViewportWithCountEXT({ { 0, 0, static_cast<float>(fb_width), static_cast<float>(fb_height) } });
		cb.bindShadersEXT(shader.stages, shader.shaders);
		// cb.bindVertexBuffers(0, { vertexBuffers[imageIdx].buffer }, { 0 });
		cb.bindIndexBuffer(indexBuffers[imageIdx]->buffer, 0, sizeof(ImDrawIdx) == 2 ? vk::IndexType::eUint16 : vk::IndexType::eUint32);

		float scale[2];
		scale[0] = 2.0f / draw_data->DisplaySize.x;
		scale[1] = 2.0f / draw_data->DisplaySize.y;
		float translate[2];
		translate[0] = -1.0f - draw_data->DisplayPos.x * scale[0];
		translate[1] = -1.0f - draw_data->DisplayPos.y * scale[1];

		cb.pushConstants<vk::DeviceAddress>(*shader.layout, vk::ShaderStageFlagBits::eVertex, 0, vertexBuffers[imageIdx]->deviceAddress);
		cb.pushConstants<float[2]>(*shader.layout, vk::ShaderStageFlagBits::eVertex, sizeof(float) * 2, scale);
		cb.pushConstants<float[2]>(*shader.layout, vk::ShaderStageFlagBits::eVertex, sizeof(float) * 4, translate);

		// Render command lists
		// (Because we merged all buffers into a single one, we maintain our own offset into them)
		// Will project scissor/clipping rectangles into framebuffer space
		const ImVec2 clip_off = draw_data->DisplayPos;         // (0,0) unless using multi-viewports
		const ImVec2 clip_scale = draw_data->FramebufferScale; // (1,1) unless using retina display which are often (2,2)
		uint32_t global_vtx_offset = 0;
		uint32_t global_idx_offset = 0;
		vk::DescriptorSet::NativeType prevDesc = nullptr;
		for (int n = 0; n < draw_data->CmdListsCount; n++)
		{
			const ImDrawList* cmd_list = draw_data->CmdLists[n];
			for (int cmd_i = 0; cmd_i < cmd_list->CmdBuffer.Size; cmd_i++)
			{
				const ImDrawCmd* pcmd = &cmd_list->CmdBuffer[cmd_i];
				// Project scissor/clipping rectangles into framebuffer space
				ImVec2 clip_min((pcmd->ClipRect.x - clip_off.x) * clip_scale.x, (pcmd->ClipRect.y - clip_off.y) * clip_scale.y);
				ImVec2 clip_max((pcmd->ClipRect.z - clip_off.x) * clip_scale.x, (pcmd->ClipRect.w - clip_off.y) * clip_scale.y);

				// Clamp to viewport as vkCmdSetScissor() won't accept values that are off bounds
				if (clip_min.x < 0.0f) { clip_min.x = 0.0f; }
				if (clip_min.y < 0.0f) { clip_min.y = 0.0f; }
				if (clip_max.x > static_cast<float>(fb_width)) { clip_max.x = static_cast<float>(fb_width); }
				if (clip_max.y > static_cast<float>(fb_height)) { clip_max.y = static_cast<float>(fb_height); }
				if (clip_max.x <= clip_min.x || clip_max.y <= clip_min.y)
					continue;

				// Apply scissor/clipping rectangle
				cb.setScissorWithCountEXT(vk::Rect2D{
					vk::Offset2D { static_cast<int32_t>(clip_min.x), static_cast<int32_t>(clip_min.y) },
					vk::Extent2D { static_cast<uint32_t>(clip_max.x - clip_min.x), static_cast<uint32_t>(clip_max.y - clip_min.y) }
				});

				// Bind DescriptorSet with font or user texture if different
				const vk::DescriptorSet::NativeType descSet = (vk::DescriptorSet::NativeType)(pcmd->TextureId);
				if (!prevDesc || prevDesc != descSet) {
					cb.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, shader.layout, 0, { descSet }, {});
					prevDesc = descSet;
				}
				cb.drawIndexed(pcmd->ElemCount, 1, pcmd->IdxOffset + global_idx_offset,
					static_cast<int32_t>(pcmd->VtxOffset + global_vtx_offset), 0);
			}
			global_idx_offset += cmd_list->IdxBuffer.Size;
			global_vtx_offset += cmd_list->VtxBuffer.Size;
		}
	}
}

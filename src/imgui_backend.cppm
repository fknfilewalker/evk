module;
#include <imgui.h>
#include <vector>
#include <memory>
#include <string_view>
export module evk.imgui;
import evk;

export namespace evk
{
	struct ImGuiBackend : Resource
	{
		EVK_API ImGuiBackend() : sampler{nullptr} {};
		EVK_API ImGuiBackend(
			const evk::SharedPtr<Device>& device,
			uint32_t imageCount
		);

		EVK_API void setFont(
			std::string_view filepath = "",
			float scaleFactor = 1.0f
		);

		EVK_API static void setContext(ImGuiContext* ctx);

		EVK_API void render(
			const vk::raii::CommandBuffer& cb, 
			uint32_t imageIdx
		);

        evk::DescriptorSetLayout descriptorSetLayout;
		evk::DescriptorSet descriptorSet;
		evk::ShaderObject shader;
		std::vector<std::unique_ptr<evk::Buffer>> vertexBuffers;
		std::vector<void*> vertexBuffersPtr;
		std::vector<std::unique_ptr<evk::Buffer>> indexBuffers;
		std::vector<void*> indexBuffersPtr;
		std::vector<std::unique_ptr<evk::Buffer>> vertexBuffersToBeDeleted;
		std::vector<std::unique_ptr<evk::Buffer>> indexBuffersToBeDeleted;

		evk::Image fontImage;
		vk::raii::Sampler sampler;
	};

}
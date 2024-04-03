module;
#include <vector>
#include <memory>
#include <string_view>
export module evk.imgui;
import evk;

export namespace evk
{
	struct ImGuiBackend : Resource
	{
		EVK_API ImGuiBackend(
			const std::shared_ptr<Device>& device,
			const vk::raii::SurfaceKHR& surface,
			uint32_t imageCount
		);

		EVK_API void setFont(
			std::string_view filepath = "",
			float scaleFactor = 1.0f
		);

		EVK_API void render(
			const vk::raii::CommandBuffer& cb, 
			uint32_t imageIdx
		);

		evk::DescriptorSet descriptorSet;
		evk::ShaderObject shader;
		std::vector<evk::Buffer> vertexBuffers;
		std::vector<void*> vertexBuffersPtr;
		std::vector<evk::Buffer> indexBuffers;
		std::vector<void*> indexBuffersPtr;

		evk::Image fontImage;
		vk::raii::Sampler sampler;
	};

}
#ifndef OCTOON_RENDERER_H_
#define OCTOON_RENDERER_H_

#include <octoon/runtime/singleton.h>

#include <octoon/hal/graphics.h>

#include <octoon/camera/camera.h>
#include <octoon/light/light.h>
#include <octoon/geometry/geometry.h>

#include <octoon/video/render_object.h>
#include <octoon/video/render_scene.h>
#include <octoon/video/forward_buffer.h>
#include <octoon/video/forward_material.h>
#include <octoon/video/forward_scene.h>

#include <octoon/lightmap/lightmap.h>

#include <unordered_map>

namespace octoon::video
{
	enum class ShadowMode : std::uint8_t
	{
		ShadowModeNone,
		ShadowModeHard,
		ShadowModeSoft,
		ShadowModeBeginRange = ShadowModeNone,
		ShadowModeEndRange = ShadowModeSoft,
		ShadowModeRangeSize = (ShadowModeEndRange - ShadowModeBeginRange + 1),
	};

	enum class ShadowQuality : std::uint8_t
	{
		ShadowQualityNone,
		ShadowQualityLow,
		ShadowQualityMedium,
		ShadowQualityHigh,
		ShadowQualityVeryHigh,
		ShadowQualityBeginRange = ShadowQualityNone,
		ShadowQualityEndRange = ShadowQualityVeryHigh,
		ShadowQualityRangeSize = (ShadowQualityEndRange - ShadowQualityBeginRange + 1),
	};

	class OCTOON_EXPORT Renderer final
	{
		OctoonDeclareSingleton(Renderer)
	public:
		Renderer() noexcept;
		~Renderer() noexcept;

		void setup(const hal::GraphicsContextPtr& context, std::uint32_t w, std::uint32_t h) except;
		void close() noexcept;

		void setFramebufferSize(std::uint32_t w, std::uint32_t h) noexcept;
		void getFramebufferSize(std::uint32_t& w, std::uint32_t& h) const noexcept;

		void setSortObjects(bool sortObject) noexcept;
		bool getSortObject() const noexcept;

		void setOverrideMaterial(const std::shared_ptr<material::Material>& material) noexcept;
		const std::shared_ptr<material::Material>& getOverrideMaterial() const noexcept;

		void readColorBuffer(math::float3 data[]);
		void readAlbedoBuffer(math::float3 data[]);
		void readNormalBuffer(math::float3 data[]);

		const hal::GraphicsFramebufferPtr& getFramebuffer() const noexcept;

		hal::GraphicsInputLayoutPtr createInputLayout(const hal::GraphicsInputLayoutDesc& desc) noexcept;
		hal::GraphicsDataPtr createGraphicsData(const hal::GraphicsDataDesc& desc) noexcept;
		hal::GraphicsTexturePtr createTexture(const hal::GraphicsTextureDesc& desc) noexcept;
		hal::GraphicsSamplerPtr createSampler(const hal::GraphicsSamplerDesc& desc) noexcept;
		hal::GraphicsFramebufferPtr createFramebuffer(const hal::GraphicsFramebufferDesc& desc) noexcept;
		hal::GraphicsFramebufferLayoutPtr createFramebufferLayout(const hal::GraphicsFramebufferLayoutDesc& desc) noexcept;
		hal::GraphicsShaderPtr createShader(const hal::GraphicsShaderDesc& desc) noexcept;
		hal::GraphicsProgramPtr createProgram(const hal::GraphicsProgramDesc& desc) noexcept;
		hal::GraphicsStatePtr createRenderState(const hal::GraphicsStateDesc& desc) noexcept;
		hal::GraphicsPipelinePtr createRenderPipeline(const hal::GraphicsPipelineDesc& desc) noexcept;
		hal::GraphicsDescriptorSetPtr createDescriptorSet(const hal::GraphicsDescriptorSetDesc& desc) noexcept;
		hal::GraphicsDescriptorSetLayoutPtr createDescriptorSetLayout(const hal::GraphicsDescriptorSetLayoutDesc& desc) noexcept;
		hal::GraphicsDescriptorPoolPtr createDescriptorPool(const hal::GraphicsDescriptorPoolDesc& desc) noexcept;

		void generateMipmap(const hal::GraphicsTexturePtr& texture) noexcept;
		void render(const std::shared_ptr<RenderScene>& scene) noexcept(false);

	private:
		Renderer(const Renderer&) = delete;
		Renderer& operator=(const Renderer&) = delete;

	private:
		bool sortObjects_;
		bool enableGlobalIllumination_;

		std::uint32_t width_, height_;

		hal::GraphicsContextPtr context_;

		std::unique_ptr<class RtxManager> rtxManager_;
		std::unique_ptr<class ForwardRenderer> forwardRenderer_;

		std::shared_ptr<material::Material> overrideMaterial_;
	};
}

#endif
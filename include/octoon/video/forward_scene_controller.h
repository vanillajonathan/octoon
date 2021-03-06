#ifndef OCTOON_VIDEO_FORWARD_SCENE_CONTROLLER_H_
#define OCTOON_VIDEO_FORWARD_SCENE_CONTROLLER_H_

#include <octoon/hal/graphics_context.h>
#include <octoon/video/collector.h>
#include <unordered_map>

#include "forward_scene.h"
#include "scene_controller.h"

namespace octoon::video
{
	class OCTOON_EXPORT ForwardSceneController final : public SceneController
	{
	public:
		ForwardSceneController(const hal::GraphicsContextPtr& context);

		void cleanCache() noexcept override;
		void compileScene(const std::shared_ptr<RenderScene>& scene) noexcept override;
		CompiledScene& getCachedScene(const std::shared_ptr<RenderScene>& scene) const noexcept(false);

	private:
		void updateCamera(const std::shared_ptr<RenderScene>& scene, ForwardScene& out, bool force = false) const;
		void updateLights(const std::shared_ptr<RenderScene>& scene, ForwardScene& out, bool force = false) const;
		void updateMaterials(const std::shared_ptr<RenderScene>& scene, ForwardScene& out, bool force = false) const;
		void updateShapes(const std::shared_ptr<RenderScene>& scene, ForwardScene& out, bool force = false) const;

	private:
		ForwardSceneController(const ForwardSceneController&) = delete;
		ForwardSceneController& operator=(const ForwardSceneController&) = delete;

	private:
		Collector materialCollector;

		hal::GraphicsContextPtr context_;
		std::unordered_map<std::shared_ptr<RenderScene>, std::unique_ptr<ForwardScene>> sceneCache_;
	};
}

#endif
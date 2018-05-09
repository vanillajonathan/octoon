#include "path_meshing_component.h"

#include <octoon/model/mesh.h>
#include <octoon/model/contour_group.h>
#include <octoon/mesh_filter_component.h>
#include <octoon/mesh_renderer_component.h>
#include <octoon/transform_component.h>

#include <octoon/runtime/json.h>
#include <octoon/runtime/except.h>
#include <octoon/runtime/uuid.h>
#include <octoon/io/fcntl.h>

#include <octoon/video/text_material.h>
#include <octoon/video/line_material.h>
#include <octoon/video/phong_material.h>
#include <octoon/video/render_system.h>
#include <octoon/camera_component.h>

#include <cstring>
#include <chrono>
#include <sstream>
#include <fstream>

#define POD_TT_PRIM_NONE 0
#define POD_TT_PRIM_LINE 1   	// line to, 一个点［x,y]
#define POD_TT_PRIM_QSPLINE 2	// qudratic bezier to, 三个点［[controlX,controlY]，[endX,endY]］
#define POD_TT_PRIM_CSPLINE 3	// cubic bezier to, 三个点［[control1X,control1Y]，[control2X,control2Y]，[endX,endY]］
#define POD_TT_PRIM_ARCLINE 4	// arc to, 三个点［[startX,startY]，[endX,endY]，[startDegree,sweepAngle]］
#define POD_TT_PRIM_MOVE 8   	// move to, 一个点［x,y]
#define POD_TT_PRIM_CLOSE 9  	// close path, equal to fisrt point of path

#define MATERIAL_TYPE_LINE 0
#define MATERIAL_TYPE_BASIC 1
#define MATERIAL_TYPE_PHONGSHADING 2

OctoonImplementSubClass(PathMeshingComponent, octoon::GameComponent, "PathMeshingComponent")

using namespace octoon;
using namespace octoon::runtime;
using namespace octoon::graphics;
using namespace octoon::video;

math::float3& operator << (math::float3& v, json::reference& json)
{
	auto size = std::min<std::size_t>(3, json.size());
	for (std::uint8_t n = 0; n < size; n++)
		v[n] = json[n].get<json::number_float_t>();
	return v;
}

math::float4& operator << (math::float4& v, json::reference& json)
{
	auto size = std::min<std::size_t>(4, json.size());
	for (std::uint8_t n = 0; n < size; n++)
		v[n] = json[n].get<json::number_float_t>();
	return v;
}

PathMeshingComponent::PathMeshingComponent() noexcept
{
}

PathMeshingComponent::PathMeshingComponent(std::string&& json) noexcept
{
	this->setBezierPath(std::move(json));
}

PathMeshingComponent::PathMeshingComponent(const std::string& json) noexcept
{
	this->setBezierPath(json);
}

PathMeshingComponent::~PathMeshingComponent() noexcept
{
}

void
PathMeshingComponent::setBezierPath(std::string&& json) except
{
	json_ = std::move(json);

	if (this->getActive())
	{
		auto object = this->getGameObject();
		if (object && object->getActive())
		{
			this->updateContour(json_);
			this->updateMesh();
		}
	}
}

void
PathMeshingComponent::setBezierPath(const std::string& json) except
{
	json_ = json;

	if (this->getActive())
	{
		auto object = this->getGameObject();
		if (object && object->getActive())
		{
			this->updateContour(json_);
			this->updateMesh();
		}
	}
}

const std::string&
PathMeshingComponent::getBezierPath() const noexcept
{
	return json_;
}

void
PathMeshingComponent::onActivate() noexcept(false)
{
	this->addComponentDispatch(octoon::GameDispatchType::FrameEnd);

	if (!json_.empty())
	{
		this->updateContour(json_);
		this->updateMesh();
	}
}

void
PathMeshingComponent::onDeactivate() noexcept
{
	this->removeComponentDispatch(octoon::GameDispatchType::FrameEnd);
}

GameComponentPtr
PathMeshingComponent::clone() const noexcept
{
	auto instance = std::make_shared<PathMeshingComponent>();
	instance->setName(this->getName());
	instance->setBezierPath(this->getBezierPath());

	return instance;
}

void
PathMeshingComponent::updateContour(const std::string& data) noexcept(false)
{
	auto reader = json::parse(data);

	auto transform = reader["transform"];
	if (!transform.is_null())
	{
		params_.transform.translate << transform["translate"];
		params_.transform.scale << transform["scale"];
		params_.transform.rotation << transform["rotation"];
		params_.transform.rotation *= math::float3(-1.0, 1.0, -1.0);
		params_.transform.rotation = math::radians(params_.transform.rotation);
	}

	auto& bound = reader["boundingBox"];
	if (!bound.is_null())
	{
		params_.bound.center << bound["center"];
		params_.bound.aabb.min << bound["min"];
		params_.bound.aabb.max << bound["max"];
	}

	auto& json_material = reader["material"];
	if (!json_material.is_null())
	{
		params_.material.color << reader["color"];

		auto& type = json_material["type"];
		auto& hollow = json_material["hollow"];
		auto& thickness = json_material["thickness"];
		auto& bezierSteps = json_material["bezierSteps"];

		if (!type.is_null())
			params_.material.type = (PathMeshing::Material::Type)type.get<json::number_integer_t>();
		else
			throw runtime::runtime_error::create(R"(The "type" must be interger, but is null)");

		if (!hollow.is_null())
			params_.material.hollow = hollow.get<json::boolean_t>();
		else
			throw runtime::runtime_error::create(R"(The "hollow" must be boolean, but is null)");

		if (!thickness.is_null())
			params_.material.thickness = thickness.get<json::number_float_t>();
		else
			throw runtime::runtime_error::create(R"(The "thickness" must be float, but is null)");

		if (!bezierSteps.is_null())
			params_.material.bezierSteps = bezierSteps.get<json::number_unsigned_t>();
		else
			throw runtime::runtime_error::create(R"(The "bezierSteps" must be unsigned int, but is null)");

		switch (params_.material.type)
		{
		case PathMeshing::Material::Line:
		{
			auto& lineSize = json_material["lineWidth"];
			if (!lineSize.is_null())
				params_.material.line.lineWidth = lineSize.get<json::number_float_t>();
		}
		break;
		case PathMeshing::Material::Basic:
		case PathMeshing::Material::PhongShading:
		{
			auto& lights = json_material["lights"];
			if (lights.size() > 0)
			{
				auto& light = lights[0];

				auto& intensity = light["intensity"];
				auto& ambient = light["ambient"];
				auto& highlight = light["highlight"];
				auto& highlightSize = light["highlightSize"];

				params_.material.phong.direction << light["direction"];
				params_.material.phong.darkcolor << light["darkcolor"];

				if (!intensity.is_null())
					params_.material.phong.intensity = intensity.get<json::number_float_t>();
				else
					throw runtime::runtime_error::create(R"(The "intensity" must be float, but is null)");

				if (!ambient.is_null())
					params_.material.phong.ambient = ambient.get<json::number_float_t>();
				else
					throw runtime::runtime_error::create(R"(The "ambient" must be float, but is null)");

				if (!highlight.is_null())
					params_.material.phong.highlight = highlight.get<json::number_float_t>();
				else
					throw runtime::runtime_error::create(R"(The "highlight" must be float, but is null)");

				if (!highlightSize.is_null())
					params_.material.phong.highlightSize = highlightSize.get<json::number_float_t>();
				else
					throw runtime::runtime_error::create(R"(The "highlightSize" must be float, but is null)");
			}
		}
		break;
		default:
			throw runtime::runtime_error::create("invalid type of material.");
		}
	}

	auto& text = reader["text"];
	if (!text.is_null())
	{
		params_.contours.clear();

		for (auto& group : text["chars"])
		{
			for (auto& path : group["paths"])
			{
				json::pointer prev = nullptr;
				json::pointer cur = nullptr;
				json::reference points = path["points"];

				auto contour = std::make_unique<model::Contour>();

				for (std::size_t index = 0; index < points.size(); index++)
				{
					cur = &points[index];

					switch (cur->at(2).get<json::number_unsigned_t>())
					{
					case POD_TT_PRIM_LINE:
					case POD_TT_PRIM_MOVE:
					case POD_TT_PRIM_CLOSE:
					{
						contour->addPoints(math::float3(cur->at(0).get<json::number_float_t>(), cur->at(1).get<json::number_float_t>(), 0));
						prev = cur;
					}
					break;
					case POD_TT_PRIM_QSPLINE:
					{
						auto& p1 = *prev;
						auto& p2 = *cur;
						auto& p3 = points[index + 1];

						math::float3 A(p1[0].get<json::number_float_t>(), p1[1].get<json::number_float_t>(), 0.0);
						math::float3 B(p2[0].get<json::number_float_t>(), p2[1].get<json::number_float_t>(), 0.0);
						math::float3 C(p3[0].get<json::number_float_t>(), p3[1].get<json::number_float_t>(), 0.0);

						contour->addPoints(A, B, C, params_.material.bezierSteps);

						prev = &p3;

						index++;
					}
					break;
					case POD_TT_PRIM_CSPLINE:
					{
						auto& p1 = *prev;
						auto& p2 = *cur;
						auto& p3 = points[index + 1];
						auto& p4 = points[index + 2];

						math::float3 A(p1[0].get<json::number_float_t>(), p1[1].get<json::number_float_t>(), 0.0);
						math::float3 B(p2[0].get<json::number_float_t>(), p2[1].get<json::number_float_t>(), 0.0);
						math::float3 C(p3[0].get<json::number_float_t>(), p3[1].get<json::number_float_t>(), 0.0);
						math::float3 D(p4[0].get<json::number_float_t>(), p4[1].get<json::number_float_t>(), 0.0);

						contour->addPoints(A, B, C, D, params_.material.bezierSteps);

						prev = &p4;

						index += 2;
					}
					break;
					default:
						throw runtime::runtime_error::create("invalid type of bezier path.");
					}
				}

				contour->isClockwise(!params_.material.hollow);
				params_.contours.push_back(std::move(contour));
			}
		}
	}
}

void
PathMeshingComponent::updateMesh() noexcept
{
	if (params_.contours.empty())
		return;

	math::float3 offset = math::lerp(params_.bound.aabb, params_.bound.center);

	params_.bound.aabb -= offset;
	params_.bound.aabb.min.z -= params_.material.thickness;
	params_.bound.aabb.max.z += params_.material.thickness;
	params_.bound.aabb = math::transform(params_.bound.aabb, math::makeRotation(math::Quaternion(params_.transform.rotation)));
	params_.transform.translate += offset;

	for (auto& contour : params_.contours)
		(*contour) -= offset;

	camera_ = std::make_shared<octoon::GameObject>();

	auto cameraComponent = camera_->addComponent<octoon::CameraComponent>();
	cameraComponent->setCameraOrder(octoon::video::CameraOrder::Custom);
	cameraComponent->setCameraType(octoon::video::CameraType::Ortho);
	cameraComponent->setOrtho(octoon::math::float4(0.0, 1.0, 0.0, 1.0));
	cameraComponent->setClearColor(octoon::math::float4(1.0, 1.0, 1.0, 0.0));
	cameraComponent->setupFramebuffers(params_.bound.aabb.size().x, params_.bound.aabb.size().y, 4);
	cameraComponent->setupSwapFramebuffers(params_.bound.aabb.size().x, params_.bound.aabb.size().y);

	object_ = std::make_shared<octoon::GameObject>();
	object_->getComponent<TransformComponent>()->setTranslate(-params_.bound.aabb.min);
	object_->getComponent<TransformComponent>()->setQuaternion(math::Quaternion(params_.transform.rotation));
	object_->getComponent<TransformComponent>()->setScale(params_.transform.scale);

	switch (params_.material.type)
	{
	case PathMeshing::Material::Line:
	{
		auto material = std::make_shared<octoon::video::LineMaterial>(params_.material.line.lineWidth);
		material->setColor(params_.material.color);

		object_->addComponent<octoon::MeshFilterComponent>(model::makeMeshWireframe(params_.contours, params_.material.thickness));
		object_->addComponent<octoon::MeshRendererComponent>(std::move(material));
	}
	break;
	case PathMeshing::Material::Basic:
	{
		auto material = std::make_shared<octoon::video::TextMaterial>();
		material->setTextColor(octoon::video::TextColor::FrontColor, params_.material.color);
		material->setTextColor(octoon::video::TextColor::SideColor, params_.material.color);

		object_->addComponent<octoon::MeshFilterComponent>(model::makeMesh(params_.contours, params_.material.thickness));
		object_->addComponent<octoon::MeshRendererComponent>(std::move(material));
	}
	break;
	case PathMeshing::Material::PhongShading:
	{
		auto material = std::make_shared<octoon::video::PhongMaterial>();
		material->setBaseColor(params_.material.color * params_.material.phong.intensity / 255.0f);
		material->setAmbientColor(params_.material.color * params_.material.phong.ambient / 255.0f);
		material->setSpecularColor(math::float3::One * params_.material.phong.highlight);
		material->setShininess(params_.material.phong.highlightSize);
		material->setLightDir(math::normalize(params_.material.phong.direction));
		material->setDarkColor(params_.material.phong.darkcolor / 255.0f);

		object_->addComponent<octoon::MeshFilterComponent>(model::makeMesh(params_.contours, params_.material.thickness));
		object_->addComponent<octoon::MeshRendererComponent>(std::move(material));
	}
	break;
	}
}

void
PathMeshingComponent::onFrameEnd() except
{
	auto framebuffer = camera_->getComponent<octoon::CameraComponent>()->getSwapFramebuffer();
	if (!framebuffer)
		return;

	std::uint32_t w = framebuffer->getGraphicsFramebufferDesc().getWidth();
	std::uint32_t h = framebuffer->getGraphicsFramebufferDesc().getHeight();

	image::Image image(image::Format::R8G8B8A8UNorm, w, h);

	void* data = nullptr;
	auto texture = framebuffer->getGraphicsFramebufferDesc().getColorAttachments().at(0).getBindingTexture();
	if (texture->map(0, 0, w, h, 0, &data))
	{
		std::memcpy((void*)image.data(), data, image.size());
		texture->unmap();
	}

	auto center = params_.transform.translate + params_.bound.aabb.min;
	this->onSaveImage(image, center.x, center.y);
}

void
PathMeshingComponent::onSaveImage(octoon::image::Image& image, float x, float y) except
{
	std::ostringstream stream;
#if __WINDOWS__
	stream << "D:/media/images/";
#else
	stream << "/media/images/";
#endif

	auto time = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
	stream << std::put_time(std::localtime(&time), "%Y/%m/%d/");

	io::fcntl::mkdir(stream.str().c_str());

	stream << make_guid();

	json j;
	j["x"] = x;
	j["y"] = y;
	j["w"] = image.width();
	j["h"] = image.height();
	j["path"] = stream.str() + ".png";

	std::ostringstream sstream;
	sstream << j;

	std::ofstream file(stream.str() + ".json", std::ios_base::out | std::ios_base::binary);
	if (file)
		file.write(sstream.str().c_str(), sstream.str().size());

	image.save(stream.str() + ".png", "png");

	std::cout << sstream.str();
}
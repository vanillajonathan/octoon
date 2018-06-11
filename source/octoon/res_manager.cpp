#include <octoon/res_manager.h>
#include <octoon/graphics/graphics.h>
#include <octoon/image/image.h>
#include <octoon/io/vstream.h>
#include <octoon/video/render_system.h>
#include <octoon/video/ggx_material.h>
#include <octoon/model/model.h>
#include <octoon/model/property.h>
#include <octoon/mesh_filter_component.h>
#include <octoon/mesh_renderer_component.h>
#include <octoon/runtime/except.h>
#include <octoon/runtime/string.h>

using namespace octoon::io;
using namespace octoon::image;
using namespace octoon::graphics;
using namespace octoon::video;
using namespace octoon::model;

namespace octoon
{
	OctoonImplementSingleton(ResManager)

	ResManager::ResManager() noexcept
	{
	}

	ResManager::~ResManager() noexcept
	{
	}

	GameObjectPtr 
	ResManager::createModel(const std::string& path, bool cache) except
	{
		auto it = _prefabs.find(path);
		if (it != _prefabs.end())
			return (*it).second->clone();

		Model model(path);

		auto actor = GameObject::create(path);
		auto rootPath = runtime::string::directory(path);

		for (std::size_t i = 0; i < model.get<Model::material>().size(); i++)
		{
			auto mesh = model.get<Model::mesh>(i);
			auto materialProp = model.get<Model::material>(i);

			std::string name;
			materialProp->get(MATKEY_NAME, name);

			std::string textureName;
			materialProp->get(MATKEY_TEXTURE_DIFFUSE(0), textureName);

			math::float3 base;
			materialProp->get(MATKEY_COLOR_DIFFUSE, base);

			math::float3 ambient;
			materialProp->get(MATKEY_COLOR_AMBIENT, ambient);

			auto material = std::make_shared<video::GGXMaterial>();
			material->setAmbientColor(base);
			material->setBaseColor(math::float3::Zero);
			material->setTexture(ResManager::instance()->createTexture(rootPath + textureName));

			auto object = GameObject::create(std::move(name));
			object->addComponent<MeshFilterComponent>(mesh);
			object->addComponent<MeshRendererComponent>(std::move(material));
			object->setParent(actor);
		}

		_prefabs[path] = actor->clone();

		return actor;
	}

	graphics::GraphicsTexturePtr
	ResManager::createTexture(const std::string& path, bool cache) except
	{
		assert(!path.empty());

		auto it = _textureCaches.find(path);
		if (it != _textureCaches.end())
			return (*it).second;

		fstream stream;
		if (!stream.open(path))
			throw runtime::runtime_error::create("Failed to open file :" + path);

		Image image;
		if (!image.load(stream))
			throw runtime::runtime_error::create("Unknown stream:" + path);

		GraphicsFormat format = GraphicsFormat::Undefined;
		switch (image.format())
		{
		case Format::BC1RGBUNormBlock: format = GraphicsFormat::BC1RGBUNormBlock; break;
		case Format::BC1RGBAUNormBlock: format = GraphicsFormat::BC1RGBAUNormBlock; break;
		case Format::BC1RGBSRGBBlock: format = GraphicsFormat::BC1RGBSRGBBlock; break;
		case Format::BC1RGBASRGBBlock: format = GraphicsFormat::BC1RGBASRGBBlock; break;
		case Format::BC3UNormBlock: format = GraphicsFormat::BC3UNormBlock; break;
		case Format::BC3SRGBBlock: format = GraphicsFormat::BC3SRGBBlock; break;
		case Format::BC4UNormBlock: format = GraphicsFormat::BC4UNormBlock; break;
		case Format::BC4SNormBlock: format = GraphicsFormat::BC4SNormBlock; break;
		case Format::BC5UNormBlock: format = GraphicsFormat::BC5UNormBlock; break;
		case Format::BC5SNormBlock: format = GraphicsFormat::BC5SNormBlock; break;
		case Format::BC6HUFloatBlock: format = GraphicsFormat::BC6HUFloatBlock; break;
		case Format::BC6HSFloatBlock: format = GraphicsFormat::BC6HSFloatBlock; break;
		case Format::BC7UNormBlock: format = GraphicsFormat::BC7UNormBlock; break;
		case Format::BC7SRGBBlock: format = GraphicsFormat::BC7SRGBBlock; break;
		case Format::R8G8B8UNorm: format = GraphicsFormat::R8G8B8UNorm; break;
		case Format::R8G8B8SRGB: format = GraphicsFormat::R8G8B8UNorm; break;
		case Format::R8G8B8A8UNorm: format = GraphicsFormat::R8G8B8A8UNorm; break;
		case Format::R8G8B8A8SRGB: format = GraphicsFormat::R8G8B8A8UNorm; break;
		case Format::B8G8R8UNorm: format = GraphicsFormat::B8G8R8UNorm; break;
		case Format::B8G8R8SRGB: format = GraphicsFormat::B8G8R8UNorm; break;
		case Format::B8G8R8A8UNorm: format = GraphicsFormat::B8G8R8A8UNorm; break;
		case Format::B8G8R8A8SRGB: format = GraphicsFormat::B8G8R8A8UNorm; break;
		case Format::R8UNorm: format = GraphicsFormat::R8UNorm; break;
		case Format::R8SRGB: format = GraphicsFormat::R8UNorm; break;
		case Format::R8G8UNorm: format = GraphicsFormat::R8G8UNorm; break;
		case Format::R8G8SRGB: format = GraphicsFormat::R8G8UNorm; break;
		case Format::R16SFloat: format = GraphicsFormat::R16SFloat; break;
		case Format::R16G16SFloat: format = GraphicsFormat::R16G16SFloat; break;
		case Format::R16G16B16SFloat: format = GraphicsFormat::R16G16B16SFloat; break;
		case Format::R16G16B16A16SFloat: format = GraphicsFormat::R16G16B16A16SFloat; break;
		case Format::R32SFloat: format = GraphicsFormat::R32SFloat; break;
		case Format::R32G32SFloat: format = GraphicsFormat::R32G32SFloat; break;
		case Format::R32G32B32SFloat: format = GraphicsFormat::R32G32B32SFloat; break;
		case Format::R32G32B32A32SFloat: format = GraphicsFormat::R32G32B32A32SFloat; break;
		default:
			throw runtime::runtime_error::create("This image type is not supported by this function:" + path);
		}

		GraphicsTextureDesc textureDesc;
		textureDesc.setSize(image.width(), image.height(), image.depth());
		textureDesc.setTexDim(GraphicsTextureDim::Texture2D);
		textureDesc.setTexFormat(format);
		textureDesc.setStream(image.data());
		textureDesc.setStreamSize(image.size());
		textureDesc.setMipBase(image.mipBase());
		textureDesc.setMipNums(image.mipLevel());
		textureDesc.setLayerBase(image.layerBase());
		textureDesc.setLayerNums(image.layerLevel());

		auto texture = RenderSystem::instance()->createTexture(textureDesc);
		if (!texture)
			return nullptr;

		if (cache)
			_textureCaches[path] = texture;

		return texture;
	}
}
#include <octoon/video/forward_buffer.h>
#include <octoon/video/renderer.h>

namespace octoon::video
{
	ForwardBuffer::ForwardBuffer() noexcept
	{
	}

	ForwardBuffer::ForwardBuffer(const std::shared_ptr<mesh::Mesh>& mesh) noexcept(false)
	{
		this->setMesh(mesh);
	}

	ForwardBuffer::~ForwardBuffer() noexcept
	{
	}

	void
	ForwardBuffer::setMesh(const std::shared_ptr<mesh::Mesh>& mesh) noexcept(false)
	{
		if (this->mesh_ != mesh)
		{
			this->updateData(mesh);
			this->mesh_ = mesh;
		}
	}

	const std::shared_ptr<mesh::Mesh>&
	ForwardBuffer::getMesh() const noexcept
	{
		return this->mesh_;
	}

	const hal::GraphicsDataPtr&
	ForwardBuffer::getVertexBuffer() const noexcept
	{
		return vertices_;
	}

	const hal::GraphicsDataPtr&
	ForwardBuffer::getIndexBuffer() const noexcept
	{
		return indices_;
	}

	std::size_t
	ForwardBuffer::getNumVertices() const noexcept
	{
		return mesh_->getVertexArray().size();
	}

	std::size_t
	ForwardBuffer::getNumIndices(std::size_t n) const noexcept
	{
		return mesh_->getIndicesArray(n).size();
	}

	std::size_t
	ForwardBuffer::getStartIndices(std::size_t n) const noexcept
	{
		return this->startIndice_[n];
	}

	void
	ForwardBuffer::updateData(const std::shared_ptr<mesh::Mesh>& mesh) noexcept(false)
	{
		if (mesh)
		{
			auto& vertices = mesh->getVertexArray();
			auto& texcoord = mesh->getTexcoordArray();
			auto& texcoord1 = mesh->getTexcoordArray(1);
			auto& normals = mesh->getNormalArray();

			hal::GraphicsInputLayoutDesc inputLayout;
			inputLayout.addVertexLayout(hal::GraphicsVertexLayout(0, "POSITION", 0, hal::GraphicsFormat::R32G32B32SFloat));
			inputLayout.addVertexLayout(hal::GraphicsVertexLayout(0, "NORMAL", 0, hal::GraphicsFormat::R32G32B32SFloat));
			inputLayout.addVertexLayout(hal::GraphicsVertexLayout(0, "TEXCOORD", 0, hal::GraphicsFormat::R32G32SFloat));
			inputLayout.addVertexLayout(hal::GraphicsVertexLayout(0, "TEXCOORD", 1, hal::GraphicsFormat::R32G32SFloat));

			inputLayout.addVertexBinding(hal::GraphicsVertexBinding(0, inputLayout.getVertexSize()));

			auto vertexSize = inputLayout.getVertexSize() / sizeof(float);

			auto vertexCount = vertices.size() * vertexSize;
			if (vertexCount > 0)
			{
				auto vertexBuffer = std::make_unique<float[]>(vertices.size() * vertexSize);

				auto dst = vertexBuffer.get();
				auto numVertices = vertices.size();

				for (std::size_t i = 0; i < numVertices; i++, dst += vertexSize)
				{
					if (!vertices.empty())
					{
						auto& v = vertices[i];
						dst[0] = v.x;
						dst[1] = v.y;
						dst[2] = v.z;
					}

					if (!normals.empty())
					{
						auto& n = normals[i];
						dst[3] = n.x;
						dst[4] = n.y;
						dst[5] = n.z;
					}

					if (!texcoord.empty())
					{
						auto& uv = texcoord[i];
						dst[6] = uv.x;
						dst[7] = uv.y;
					}

					if (!texcoord1.empty())
					{
						auto& uv = texcoord1[i];
						dst[8] = uv.x;
						dst[9] = uv.y;
					}
				}

				hal::GraphicsDataDesc dataDesc;
				dataDesc.setType(hal::GraphicsDataType::StorageVertexBuffer);
				dataDesc.setStream((std::uint8_t*)vertexBuffer.get());
				dataDesc.setStreamSize(vertexCount * sizeof(float));
				dataDesc.setUsage(hal::GraphicsUsageFlagBits::ReadBit);

				this->vertices_ = video::Renderer::instance()->createGraphicsData(dataDesc);
			}

			if (mesh->getNumSubsets() == 1)
			{
				auto& indices = mesh->getIndicesArray(0);

				hal::GraphicsDataDesc indiceDesc;
				indiceDesc.setType(hal::GraphicsDataType::StorageIndexBuffer);
				indiceDesc.setStream((std::uint8_t*)indices.data());
				indiceDesc.setStreamSize(indices.size() * sizeof(std::uint32_t));
				indiceDesc.setUsage(hal::GraphicsUsageFlagBits::ReadBit);

				this->startIndice_.push_back(0);
				this->indices_ = video::Renderer::instance()->createGraphicsData(indiceDesc);
			}
			else
			{
				std::size_t streamsize = 0;
				for (std::size_t i = 0; i < mesh->getNumSubsets(); i++)
				{
					auto& indices = mesh->getIndicesArray(i);
					if (!indices.empty())
						streamsize += indices.size();
				}

				if (streamsize > 0)
				{
					auto indicesBuffer = std::make_unique<std::uint32_t[]>(streamsize);

					for (std::size_t i = 0, streamOffset = 0; i < mesh->getNumSubsets(); i++)
					{
						auto& indices = mesh->getIndicesArray(i);
						if (!indices.empty())
						{
							std::memcpy(indicesBuffer.get() + streamOffset, indices.data(), indices.size() * sizeof(std::uint32_t));
							this->startIndice_.push_back(streamOffset);
							streamOffset += indices.size();
						}
						else
						{
							this->startIndice_.push_back(streamOffset);
						}
					}

					hal::GraphicsDataDesc indiceDesc;
					indiceDesc.setType(hal::GraphicsDataType::StorageIndexBuffer);
					indiceDesc.setStream((std::uint8_t*)indicesBuffer.get());
					indiceDesc.setStreamSize(streamsize * sizeof(std::uint32_t));
					indiceDesc.setUsage(hal::GraphicsUsageFlagBits::ReadBit);

					this->indices_ = video::Renderer::instance()->createGraphicsData(indiceDesc);
				}
			}
		}
		else
		{
			this->vertices_.reset();
			this->indices_.reset();
		}
	}
}
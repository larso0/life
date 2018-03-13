#include "CellRenderer.h"
#include <bp/Util.h>

using namespace bp;

void CellDrawable::init(Device& device, RenderPass& renderPass)
{
	CellDrawable::device = &device;

	createShaders();
	createPipelineLayout();
	createPipeline(renderPass);
	createBuffer(1024);
}

void CellDrawable::updateCells(const SparseGrid& grid)
{
	VkDeviceSize requiredSize = grid.size() * sizeof(glm::ivec2);
	if (positionBuffer->getSize() < requiredSize)
	{
		VkDeviceSize newSize = positionBuffer->getSize();
		while (newSize < requiredSize) newSize *= 2;
		delete positionBuffer;
		createBuffer(newSize);
	}

	glm::ivec2* mapped = static_cast<glm::ivec2*>(positionBuffer->map());

	for (auto i = 0; i < grid.size(); i++)
		mapped[i] = glm::ivec2(grid[i].x, grid[i].y);

	positionBuffer->flushStagingBuffer();

	cellCount = static_cast<uint32_t>(grid.size());
}

void CellDrawable::draw(VkCommandBuffer cmdBuffer)
{
	vkCmdPushConstants(cmdBuffer, pipelineLayout, VK_SHADER_STAGE_GEOMETRY_BIT, 0,
			   sizeof(glm::mat4), &mvp);

	VkDeviceSize offset = 0;
	VkBuffer vertexBuffer = positionBuffer->getHandle();
	vkCmdBindVertexBuffers(cmdBuffer, 0, 1, &vertexBuffer, &offset);

	vkCmdDraw(cmdBuffer, cellCount, 1, 0, 0);
}

void CellDrawable::createShaders()
{
	auto vertexShaderCode = readBinaryFile("spv/cell.vert.spv");
	auto geometryShaderCode = readBinaryFile("spv/cell.geom.spv");
	auto fragmentShaderCode = readBinaryFile("spv/cell.frag.spv");

	vertexShader.init(*device, VK_SHADER_STAGE_VERTEX_BIT,
			  (uint32_t) vertexShaderCode.size(),
			  (const uint32_t*) vertexShaderCode.data());

	geometryShader.init(*device, VK_SHADER_STAGE_GEOMETRY_BIT,
			    (uint32_t) geometryShaderCode.size(),
			    (const uint32_t*) geometryShaderCode.data());

	fragmentShader.init(*device, VK_SHADER_STAGE_FRAGMENT_BIT,
			    (uint32_t) fragmentShaderCode.size(),
			    (const uint32_t*) fragmentShaderCode.data());
}

void CellDrawable::createPipelineLayout()
{
	pipelineLayout.addPushConstantRange({VK_SHADER_STAGE_GEOMETRY_BIT, 0, sizeof(glm::mat4)});
	pipelineLayout.init(*device);
}

void CellDrawable::createPipeline(RenderPass& renderPass)
{
	pipeline.addShaderStageInfo(vertexShader.getPipelineShaderStageInfo());
	pipeline.addShaderStageInfo(geometryShader.getPipelineShaderStageInfo());
	pipeline.addShaderStageInfo(fragmentShader.getPipelineShaderStageInfo());
	pipeline.addVertexBindingDescription({0, sizeof(glm::ivec2), VK_VERTEX_INPUT_RATE_VERTEX});
	pipeline.addVertexAttributeDescription({0, 0, VK_FORMAT_R32G32_SINT, 0});
	pipeline.setPrimitiveTopology(VK_PRIMITIVE_TOPOLOGY_POINT_LIST);
	pipeline.init(*device, renderPass, pipelineLayout);
}

void CellDrawable::createBuffer(VkDeviceSize size)
{
	positionBuffer = new Buffer(*device, size, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
				    VMA_MEMORY_USAGE_GPU_ONLY);
}

void CellRenderer::resize(uint32_t width, uint32_t height)
{
	Renderer::resize(width, height);

	float aspectRatio = static_cast<float>(width) / static_cast<float>(height);
	camera.setPerspectiveProjection(glm::radians(45.f), aspectRatio, 0.1f, 8500.f);
}

void CellRenderer::update(float delta)
{
	if (controls == nullptr) return;

	if (controls->in && !controls->out)
	{
		zoomOut -= delta;
	} else if (controls->out)
	{
		zoomOut += delta;
	}
	if (zoomOut > 13.f) zoomOut = 13.f;

	cameraPos.z = static_cast<float>(pow(2.f, zoomOut));

	if (controls->up && !controls->down) cameraPos.y -= delta * cameraPos.z;
	else if (controls->down) cameraPos.y += delta * cameraPos.z;

	if (controls->left && !controls->right) cameraPos.x -= delta * cameraPos.z;
	else if (controls->right) cameraPos.x += delta * cameraPos.z;

	cameraNode.setTranslation(cameraPos);
	cameraNode.update();
	camera.update();

	cellDrawable.setMvp(camera.getProjectionMatrix() * camera.getViewMatrix());
}

void CellRenderer::setupSubpasses()
{
	subpass.addColorAttachment(getColorAttachmentSlot());
	addSubpassGraph(subpass);
}

void CellRenderer::initResources(uint32_t width, uint32_t height)
{
	cellDrawable.init(getDevice(), getRenderPass());
	subpass.addDrawable(cellDrawable);

	zoomOut = 8.f;
	cameraPos.z = static_cast<float>(pow(2.f, zoomOut));

	float aspectRatio = static_cast<float>(width) / static_cast<float>(height);

	camera.setNode(&cameraNode);
	camera.setPerspectiveProjection(glm::radians(45.f), aspectRatio, 0.1f, 8500.f);
	cameraNode.translate(cameraPos);
	cameraNode.update();
	camera.update();

	cellDrawable.setMvp(camera.getProjectionMatrix() * camera.getViewMatrix());
}

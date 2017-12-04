#include "CellRenderer.h"
#include <stdexcept>
#include <bp/Util.h>

using namespace bp;
using namespace std;

CellRenderer::~CellRenderer()
{
	if (!isReady()) return;
	if (positionBuffer != nullptr) delete positionBuffer;
}

void CellRenderer::setCamera(const glm::vec2& center, float zoomOut)
{
	if (zoomOut > 13.f) zoomOut = 13.f;
	this->zoomOut = zoomOut;
	cameraPos.z = pow(2.f, zoomOut);
}

void CellRenderer::init(bp::NotNull<bp::RenderTarget> target, const VkRect2D& area)
{
	if (isReady()) throw runtime_error("Cell renderer already initialized.");
	this->target = target;
	this->device = target->getDevice();
	renderPass.setClearEnabled(true);
	renderPass.setClearValue({0.2f, 0.2f, 0.2f, 1.f});
	renderPass.init(target, area);

	createBuffer(1024);
	createShaders();
	createPipelineLayout();
	createPipeline();

	float aspectRatio = static_cast<float>(area.extent.width) /
			    static_cast<float>(area.extent.height);

	camera.setNode(&cameraNode);
	camera.setPerspectiveProjection(glm::radians(45.f), aspectRatio, 0.1f, 8500.f);
	cameraNode.translate(cameraPos);
	cameraNode.update();
	camera.update();
}

void CellRenderer::render(VkCommandBuffer cmdBuffer)
{
	renderPass.begin(cmdBuffer);
	draw(cmdBuffer);
	renderPass.end(cmdBuffer);
}

void CellRenderer::resize(uint32_t w, uint32_t h)
{
	renderPass.setRenderArea({{0, 0}, {w, h}});
	renderPass.recreateFramebuffers();
	float aspectRatio = static_cast<float>(w) / static_cast<float>(h);
	camera.setPerspectiveProjection(glm::radians(45.f), aspectRatio, 0.1f, 8500.f);
}

void CellRenderer::updateCells(const SparseGrid& grid)
{
	VkDeviceSize requiredSize = grid.size() * sizeof(glm::ivec2);
	if (positionBuffer->getSize() < requiredSize)
	{
		VkDeviceSize newSize = positionBuffer->getSize();
		while (newSize < requiredSize) newSize *= 2;
		delete positionBuffer;
		createBuffer(newSize);
	}

	glm::ivec2* mapped = static_cast<glm::ivec2*>(positionBuffer->map(0, VK_WHOLE_SIZE));

	for (auto i = 0; i < grid.size(); i++)
		mapped[i] = glm::ivec2(grid[i].x, grid[i].y);

	positionBuffer->unmap();

	elementCount = grid.size();
}

void CellRenderer::update(float delta)
{
	if (controls->in && !controls->out)
	{
		zoomOut -= delta;
	} else if (controls->out)
	{
		zoomOut += delta;
	}
	if (zoomOut > 13.f) zoomOut = 13.f;

	cameraPos.z = pow(2.f, zoomOut);

	if (controls->up && !controls->down) cameraPos.y -= delta * cameraPos.z;
	else if (controls->down) cameraPos.y += delta * cameraPos.z;

	if (controls->left && !controls->right) cameraPos.x -= delta * cameraPos.z;
	else if (controls->right) cameraPos.x += delta * cameraPos.z;

	cameraNode.setTranslation(cameraPos);
	cameraNode.update();
	camera.update();
}

void CellRenderer::draw(VkCommandBuffer cmdBuffer)
{
	if (elementCount == 0) return;

	vkCmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);

	const VkRect2D& area = renderPass.getRenderArea();
	VkViewport viewport = {(float) area.offset.x, (float) area.offset.y,
			       (float) area.extent.width, (float) area.extent.height, 0.f, 1.f};
	vkCmdSetViewport(cmdBuffer, 0, 1, &viewport);
	vkCmdSetScissor(cmdBuffer, 0, 1, &area);

	glm::mat4 mvp = camera.getProjectionMatrix() * camera.getViewMatrix();
	vkCmdPushConstants(cmdBuffer, pipelineLayout, VK_SHADER_STAGE_GEOMETRY_BIT, 0,
			   sizeof(glm::mat4), &mvp);

	VkDeviceSize offset = 0;
	VkBuffer vertexBuffer = positionBuffer->getHandle();
	vkCmdBindVertexBuffers(cmdBuffer, 0, 1, &vertexBuffer, &offset);

	vkCmdDraw(cmdBuffer, elementCount, 1, 0, 0);
}

void CellRenderer::createBuffer(VkDeviceSize size)
{
	positionBuffer = new Buffer(renderPass.getRenderTarget().getDevice(), size,
				    VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
				    VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
				    VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT |
				    VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);
}

void CellRenderer::createShaders()
{
	auto vertexShaderCode = readBinaryFile("spv/cell.vert.spv");
	auto geometryShaderCode = readBinaryFile("spv/cell.geom.spv");
	auto fragmentShaderCode = readBinaryFile("spv/cell.frag.spv");

	vertexShader.init(device, VK_SHADER_STAGE_VERTEX_BIT,
			  (uint32_t) vertexShaderCode.size(),
			  (const uint32_t*) vertexShaderCode.data());

	geometryShader.init(device, VK_SHADER_STAGE_GEOMETRY_BIT,
			    (uint32_t) geometryShaderCode.size(),
			    (const uint32_t*) geometryShaderCode.data());

	fragmentShader.init(device, VK_SHADER_STAGE_FRAGMENT_BIT,
			    (uint32_t) fragmentShaderCode.size(),
			    (const uint32_t*) fragmentShaderCode.data());
}

void CellRenderer::createPipelineLayout()
{
	pipelineLayout.addPushConstantRange({VK_SHADER_STAGE_GEOMETRY_BIT, 0, 128});
	pipelineLayout.init(device);
}

void CellRenderer::createPipeline()
{
	pipeline.addShaderStageInfo(vertexShader.getPipelineShaderStageInfo());
	pipeline.addShaderStageInfo(geometryShader.getPipelineShaderStageInfo());
	pipeline.addShaderStageInfo(fragmentShader.getPipelineShaderStageInfo());
	pipeline.addVertexBindingDescription({0, sizeof(glm::ivec2), VK_VERTEX_INPUT_RATE_VERTEX});
	pipeline.addVertexAttributeDescription({0, 0, VK_FORMAT_R32G32_SINT, 0});
	pipeline.setPrimitiveTopology(VK_PRIMITIVE_TOPOLOGY_POINT_LIST);
	pipeline.init(device, &renderPass, pipelineLayout);
}
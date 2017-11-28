#include "CellRenderer.h"
#include <stdexcept>
#include <bp/Util.h>

using namespace bp;
using namespace std;

CellRenderer::~CellRenderer()
{
	if (!isReady()) return;
	vkDestroyPipeline(*device, pipeline, nullptr);
	vkDestroyPipelineLayout(*device, pipelineLayout, nullptr);
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
	//Vulkan promises at leas 128 bytes for push constant. Enough for mvp and normal
	//pushConstants.
	VkPushConstantRange pushConstantRange = {VK_SHADER_STAGE_GEOMETRY_BIT, 0, 128};

	//Currently not using any descriptors. Enough with push constants.
	VkPipelineLayoutCreateInfo layoutInfo = {};
	layoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	layoutInfo.pushConstantRangeCount = 1;
	layoutInfo.pPushConstantRanges = &pushConstantRange;

	VkResult result = vkCreatePipelineLayout(*device, &layoutInfo, nullptr, &pipelineLayout);
	if (result != VK_SUCCESS)
		throw runtime_error("Failed to create pipeline layout.");
}

void CellRenderer::createPipeline()
{
	vector<VkPipelineShaderStageCreateInfo> shaderStages;
	shaderStages.push_back(vertexShader.getPipelineShaderStageInfo());
	shaderStages.push_back(geometryShader.getPipelineShaderStageInfo());
	shaderStages.push_back(fragmentShader.getPipelineShaderStageInfo());

	VkVertexInputBindingDescription vertexBindingDescription;

	vertexBindingDescription = {};
	vertexBindingDescription.binding = 0;
	vertexBindingDescription.stride = sizeof(glm::uvec2);
	vertexBindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

	VkVertexInputAttributeDescription vertexAttribute = {};
	vertexAttribute.location = 0;
	vertexAttribute.binding = 0;
	vertexAttribute.format = VK_FORMAT_R32G32_SINT;
	vertexAttribute.offset = 0;

	VkPipelineVertexInputStateCreateInfo vertexInputStateCreateInfo = {};
	vertexInputStateCreateInfo.sType =
		VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	vertexInputStateCreateInfo.vertexBindingDescriptionCount = 1;
	vertexInputStateCreateInfo.pVertexBindingDescriptions = &vertexBindingDescription;
	vertexInputStateCreateInfo.vertexAttributeDescriptionCount = 1;
	vertexInputStateCreateInfo.pVertexAttributeDescriptions = &vertexAttribute;

	VkPipelineInputAssemblyStateCreateInfo inputAssemblyStateCreateInfo = {};
	inputAssemblyStateCreateInfo.sType =
		VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	inputAssemblyStateCreateInfo.topology = VK_PRIMITIVE_TOPOLOGY_POINT_LIST;
	inputAssemblyStateCreateInfo.primitiveRestartEnable = VK_FALSE;

	VkViewport vp = {};
	VkRect2D sc = {};

	VkPipelineViewportStateCreateInfo viewportState = {};
	viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
	viewportState.viewportCount = 1;
	viewportState.pViewports = &vp;
	viewportState.scissorCount = 1;
	viewportState.pScissors = &sc;

	VkPipelineRasterizationStateCreateInfo rasterizationState = {};
	rasterizationState.sType =
		VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
	rasterizationState.depthClampEnable = VK_FALSE;
	rasterizationState.rasterizerDiscardEnable = VK_FALSE;
	rasterizationState.polygonMode = VK_POLYGON_MODE_FILL;
	rasterizationState.cullMode = VK_CULL_MODE_BACK_BIT;
	rasterizationState.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
	rasterizationState.depthBiasEnable = VK_FALSE;
	rasterizationState.depthBiasConstantFactor = 0;
	rasterizationState.depthBiasClamp = 0;
	rasterizationState.depthBiasSlopeFactor = 0;
	rasterizationState.lineWidth = 1;

	VkPipelineMultisampleStateCreateInfo multisampleState = {};
	multisampleState.sType =
		VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	multisampleState.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
	multisampleState.sampleShadingEnable = VK_FALSE;
	multisampleState.minSampleShading = 0;
	multisampleState.pSampleMask = nullptr;
	multisampleState.alphaToCoverageEnable = VK_FALSE;
	multisampleState.alphaToOneEnable = VK_FALSE;

	VkStencilOpState noOPStencilState = {};
	noOPStencilState.failOp = VK_STENCIL_OP_KEEP;
	noOPStencilState.passOp = VK_STENCIL_OP_KEEP;
	noOPStencilState.depthFailOp = VK_STENCIL_OP_KEEP;
	noOPStencilState.compareOp = VK_COMPARE_OP_ALWAYS;
	noOPStencilState.compareMask = 0;
	noOPStencilState.writeMask = 0;
	noOPStencilState.reference = 0;

	VkPipelineDepthStencilStateCreateInfo depthState = {};
	depthState.sType =
		VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
	depthState.depthTestEnable = VK_FALSE;
	depthState.depthWriteEnable = VK_FALSE;
	depthState.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
	depthState.depthBoundsTestEnable = VK_FALSE;
	depthState.stencilTestEnable = VK_FALSE;
	depthState.front = noOPStencilState;
	depthState.back = noOPStencilState;
	depthState.minDepthBounds = 0;
	depthState.maxDepthBounds = 0;

	VkPipelineColorBlendAttachmentState colorBlendAttachmentState = {};
	colorBlendAttachmentState.blendEnable = VK_FALSE;
	colorBlendAttachmentState.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_COLOR;
	colorBlendAttachmentState.dstColorBlendFactor =
		VK_BLEND_FACTOR_ONE_MINUS_DST_COLOR;
	colorBlendAttachmentState.colorBlendOp = VK_BLEND_OP_ADD;
	colorBlendAttachmentState.srcAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
	colorBlendAttachmentState.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
	colorBlendAttachmentState.alphaBlendOp = VK_BLEND_OP_ADD;
	colorBlendAttachmentState.colorWriteMask = 0xf;

	VkPipelineColorBlendStateCreateInfo colorBlendState = {};
	colorBlendState.sType =
		VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	colorBlendState.logicOpEnable = VK_FALSE;
	colorBlendState.logicOp = VK_LOGIC_OP_CLEAR;
	colorBlendState.attachmentCount = 1;
	colorBlendState.pAttachments = &colorBlendAttachmentState;
	colorBlendState.blendConstants[0] = 0.0;
	colorBlendState.blendConstants[1] = 0.0;
	colorBlendState.blendConstants[2] = 0.0;
	colorBlendState.blendConstants[3] = 0.0;

	VkDynamicState dynamicState[2] = {VK_DYNAMIC_STATE_VIEWPORT,
					  VK_DYNAMIC_STATE_SCISSOR};
	VkPipelineDynamicStateCreateInfo dynamicStateCreateInfo = {};
	dynamicStateCreateInfo.sType =
		VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
	dynamicStateCreateInfo.dynamicStateCount = 2;
	dynamicStateCreateInfo.pDynamicStates = dynamicState;

	VkGraphicsPipelineCreateInfo pipelineCreateInfo = {};
	pipelineCreateInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	pipelineCreateInfo.stageCount = (uint32_t) shaderStages.size();
	pipelineCreateInfo.pStages = shaderStages.data();
	pipelineCreateInfo.pVertexInputState = &vertexInputStateCreateInfo;
	pipelineCreateInfo.pInputAssemblyState = &inputAssemblyStateCreateInfo;
	pipelineCreateInfo.pTessellationState = nullptr;
	pipelineCreateInfo.pViewportState = &viewportState;
	pipelineCreateInfo.pRasterizationState = &rasterizationState;
	pipelineCreateInfo.pMultisampleState = &multisampleState;
	pipelineCreateInfo.pDepthStencilState = &depthState;
	pipelineCreateInfo.pColorBlendState = &colorBlendState;
	pipelineCreateInfo.pDynamicState = &dynamicStateCreateInfo;
	pipelineCreateInfo.layout = pipelineLayout;
	pipelineCreateInfo.renderPass = renderPass.getHandle();
	pipelineCreateInfo.subpass = 0;
	pipelineCreateInfo.basePipelineHandle = nullptr;
	pipelineCreateInfo.basePipelineIndex = 0;

	VkResult result = vkCreateGraphicsPipelines(*device, VK_NULL_HANDLE, 1, &pipelineCreateInfo,
					   nullptr, &pipeline);
	if (result != VK_SUCCESS)
		throw runtime_error("Failed to create pipeline.");
}
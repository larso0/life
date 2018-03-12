#ifndef LIFE_CELLRENDERER_H
#define LIFE_CELLRENDERER_H

#include <bp/Device.h>
#include <bp/RenderPass.h>
#include <bp/Subpass.h>
#include <bp/Buffer.h>
#include <bp/Shader.h>
#include <bp/PipelineLayout.h>
#include <bp/GraphicsPipeline.h>
#include <bpScene/Camera.h>
#include "Life.h"

class CellRenderer : public bp::Subpass
{
public:
	struct Controls
	{
		Controls() :
			in{false}, out{false}, left{false}, right{false}, up{false}, down{false} {}
		bool in, out, left, right, up, down;
	};

	CellRenderer() :
		device{nullptr},
		controls{nullptr},
		positionBuffer{nullptr},
		elementCount{0} {}
	CellRenderer(bp::Device& device, Controls& controls, const glm::vec2& center, float zoomOut,
		     bp::RenderPass& renderPass) :
		CellRenderer{}
	{
		init(device, controls, center, zoomOut, renderPass);
	}
	~CellRenderer() final;

	void setControls(Controls& controls)
	{
		CellRenderer::controls = &controls;
	}
	void setCamera(const glm::vec2& center, float zoomOut);
	void init(bp::Device& device, Controls& controls, const glm::vec2& center, float zoomOut,
		  bp::RenderPass& renderPass);
	void render(const VkRect2D& area, VkCommandBuffer cmdBuffer) override;
	void resize(uint32_t w, uint32_t h);
	void updateCells(const SparseGrid& grid);
	void update(float delta);

	bool isReady() const { return pipeline.isReady(); }

private:
	bp::Device* device;

	Controls* controls;
	bp::Buffer* positionBuffer;
	uint32_t elementCount;
	bp::Shader vertexShader;
	bp::Shader geometryShader;
	bp::Shader fragmentShader;
	bp::PipelineLayout pipelineLayout;
	bp::GraphicsPipeline pipeline;

	bpScene::Node cameraNode;
	bpScene::Camera camera;
	float zoomOut;
	glm::vec3 cameraPos;

	void createBuffer(VkDeviceSize size);
	void createShaders();
	void createPipelineLayout();
	void createPipeline(bp::RenderPass& renderPass);
};


#endif

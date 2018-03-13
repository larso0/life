#ifndef LIFE_CELLRENDERER_H
#define LIFE_CELLRENDERER_H

#include <bp/Renderer.h>
#include <bp/Shader.h>
#include <bp/PipelineLayout.h>
#include <bp/GraphicsPipeline.h>
#include <bp/Buffer.h>
#include <bpScene/Node.h>
#include <bpScene/Camera.h>
#include <bpScene/DrawableSubpass.h>
#include "Life.h"

class CellDrawable : public bpScene::Drawable
{
public:
	CellDrawable() :
		bpScene::Drawable{},
		device{nullptr},
		positionBuffer{nullptr},
		cellCount{0} {}
	~CellDrawable() { delete positionBuffer; }

	void init(bp::Device& device, bp::RenderPass& renderPass);
	void updateCells(const SparseGrid& grid);
	void setMvp(const glm::mat4& mvp) { CellDrawable::mvp = mvp; }
	void draw(VkCommandBuffer cmdBuffer) override;

	bp::GraphicsPipeline* getPipeline() override { return &pipeline; }

private:
	bp::Device* device;

	bp::Shader vertexShader;
	bp::Shader geometryShader;
	bp::Shader fragmentShader;
	bp::PipelineLayout pipelineLayout;
	bp::GraphicsPipeline pipeline;

	bp::Buffer* positionBuffer;
	uint32_t cellCount;
	glm::mat4 mvp;

	void createShaders();
	void createPipelineLayout();
	void createPipeline(bp::RenderPass& renderPass);
	void createBuffer(VkDeviceSize size);
};

class CellRenderer : public bp::Renderer
{
public:
	struct Controls
	{
		Controls() :
			in{false}, out{false}, left{false}, right{false}, up{false}, down{false} {}
		bool in, out, left, right, up, down;
	};

	CellRenderer() :
		Renderer{},
		controls{nullptr} {}

	void setControls(Controls& controls) { CellRenderer::controls = &controls; }
	void resize(uint32_t width, uint32_t height) override;

	void updateCells(const SparseGrid& grid) { cellDrawable.updateCells(grid); }
	void update(float delta);

private:
	Controls* controls;
	bpScene::Node cameraNode;
	bpScene::Camera camera;
	float zoomOut;
	glm::vec3 cameraPos;

	bpScene::DrawableSubpass subpass;
	CellDrawable cellDrawable;

	void setupSubpasses() override;
	void initResources(uint32_t width, uint32_t height) override;
};


#endif

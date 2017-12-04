#ifndef LIFE_CELLRENDERER_H
#define LIFE_CELLRENDERER_H

#include <bp/Renderer.h>
#include <bp/RenderPass.h>
#include <bp/Buffer.h>
#include <bp/Shader.h>
#include <bp/PipelineLayout.h>
#include <bp/GraphicsPipeline.h>
#include <bpScene/Camera.h>
#include "Life.h"

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
		target{nullptr},
		device{nullptr},
		controls{nullptr},
		positionBuffer{nullptr},
		elementCount{0} {}
	CellRenderer(bp::NotNull<bp::RenderTarget> target, bp::NotNull<Controls> controls,
		     const glm::vec2& center, float zoomOut) :
		CellRenderer{}
	{
		setControls(controls);
		setCamera(center, zoomOut);
		init(target, {{0, 0}, {target->getWidth(), target->getHeight()}});
	}
	~CellRenderer() final;

	void setControls(bp::NotNull<Controls> controls)
	{
		this->controls = controls;
	}
	void setCamera(const glm::vec2& center, float zoomOut);
	void init(bp::NotNull<bp::RenderTarget> target, const VkRect2D& area) override;
	void render(VkCommandBuffer cmdBuffer) override;
	void resize(uint32_t w, uint32_t h);
	void updateCells(const SparseGrid& grid);
	void update(float delta);
	void draw(VkCommandBuffer cmdBuffer);

	bool isReady() const { return pipeline.isReady(); }

private:
	bp::RenderTarget* target;
	bp::Device* device;
	bp::RenderPass renderPass;

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
	void createPipeline();
};


#endif

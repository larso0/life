#ifndef LIFE_CELLRENDERER_H
#define LIFE_CELLRENDERER_H

#include <bp/Renderer.h>
#include <bp/Buffer.h>
#include <bp/Shader.h>
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

	CellRenderer(bp::RenderPass& renderPass, const Controls& controls, const glm::vec2& center);
	~CellRenderer() final;

	void updateCells(const SparseGrid& grid);
	void update(float delta) override;
	void draw(VkCommandBuffer cmdBuffer) override;

private:
	const Controls& controls;
	bp::Buffer* positionBuffer;
	uint32_t elementCount;
	bp::Shader* vertexShader;
	bp::Shader* geometryShader;
	bp::Shader* fragmentShader;
	VkPipelineLayout pipelineLayout;
	VkPipeline pipeline;

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

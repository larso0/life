#include <bpView/bpView.h>
#include <bpView/Window.h>
#include <bp/Instance.h>
#include <bp/Device.h>
#include <bp/Swapchain.h>
#include <bp/RenderPass.h>
#include <iostream>
#include <iomanip>
#include <future>
#include <sstream>
#include "CellRenderer.h"
#include "Life.h"

using namespace bp;
using namespace std;

int main(int argc, char** argv)
{
	bpView::init();

#ifdef NDEBUG
	bool debugEnabled = false;
#else
	bool debugEnabled = true;
#endif

	Instance instance;

	instance.enableExtensions(bpView::requiredInstanceExtensions.begin(),
				  bpView::requiredInstanceExtensions.end());
	instance.init(debugEnabled);

	auto print = [](const string& s) { cout << s << endl; };
	auto printErr = [](const string& s) { cerr << s << endl; };

	connect(instance.infoEvent, print);
	connect(instance.warningEvent, printErr);
	connect(instance.errorEvent, printErr);
	connect(bpView::errorEvent, instance.errorEvent);

	static const uint32_t WIDTH = 1024;
	static const uint32_t HEIGHT = 768;
	static const char* SWAPCHAIN_EXTENSION = "VK_KHR_swapchain";

	Window window{instance, WIDTH, HEIGHT, "Game of Life", nullptr};

	DeviceRequirements requirements;
	requirements.queues = VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_TRANSFER_BIT;
	requirements.features.samplerAnisotropy = VK_TRUE;
	requirements.features.geometryShader = VK_TRUE;
	requirements.surface = window.getSurface();
	requirements.extensions.push_back(SWAPCHAIN_EXTENSION);

	VkPhysicalDevice physical = queryDevices(instance, requirements)[0];
	Device device{physical, requirements};

	Swapchain target{&device, window, WIDTH, HEIGHT,
			 FlagSet<Swapchain::Flags>() << Swapchain::Flags::VERTICAL_SYNC};
	connect(window.resizeEvent, target, &Swapchain::resize);

	CellRenderer::Controls controls;

	connect(window.keyPressEvent, [&](int key, int){
		switch (key)
		{
		case GLFW_KEY_PAGE_UP: controls.in = true; break;
		case GLFW_KEY_PAGE_DOWN: controls.out = true; break;
		case GLFW_KEY_LEFT: controls.left = true; break;
		case GLFW_KEY_RIGHT: controls.right = true; break;
		case GLFW_KEY_UP: controls.up = true; break;
		case GLFW_KEY_DOWN: controls.down = true; break;
		}
	});
	connect(window.keyReleaseEvent, [&](int key, int){
		switch (key)
		{
		case GLFW_KEY_PAGE_UP: controls.in = false; break;
		case GLFW_KEY_PAGE_DOWN: controls.out = false; break;
		case GLFW_KEY_LEFT: controls.left = false; break;
		case GLFW_KEY_RIGHT: controls.right = false; break;
		case GLFW_KEY_UP: controls.up = false; break;
		case GLFW_KEY_DOWN: controls.down = false; break;
		}
	});

	SparseGrid grids[2];

	if (argc < 2)
	{
		grids[0].emplace_back(5, 4);
		grids[0].emplace_back(6, 5);
		grids[0].emplace_back(4, 6);
		grids[0].emplace_back(5, 6);
		grids[0].emplace_back(6, 6);
	} else
	{
		loadGrid(argv[1], grids[0]);
		stringstream ss;
		ss << "Game of Life - " << argv[1];
		window.setTitle(ss.str());
	}

	CellRenderer renderer{&target, &controls, {0.f, 0.f}, 8.f};
	connect(window.resizeEvent, renderer, &CellRenderer::resize);

	VkCommandBuffer cmdBuffer;
	VkCommandBufferAllocateInfo cmdBufferInfo = {};
	cmdBufferInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	cmdBufferInfo.commandPool = target.getCmdPool();
	cmdBufferInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	cmdBufferInfo.commandBufferCount = 1;
	VkResult result = vkAllocateCommandBuffers(device, &cmdBufferInfo, &cmdBuffer);
	if (result != VK_SUCCESS)
		throw runtime_error("Failed to allocate command buffer.");

	VkCommandBufferBeginInfo cmdBufferBeginInfo = {};
	cmdBufferBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	cmdBufferBeginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

	VkSemaphore renderCompleteSem;
	VkSemaphoreCreateInfo semInfo = {};
	semInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
	result = vkCreateSemaphore(device, &semInfo, nullptr, &renderCompleteSem);
	if (result != VK_SUCCESS)
		throw runtime_error("Failed to create render complete semaphore.");

	VkSemaphore presentSem = target.getPresentSemaphore();
	VkPipelineStageFlags waitStages = {VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT};
	VkSubmitInfo submitInfo = {};
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &cmdBuffer;
	submitInfo.signalSemaphoreCount = 1;
	submitInfo.pSignalSemaphores = &renderCompleteSem;
	submitInfo.waitSemaphoreCount = 1;
	submitInfo.pWaitSemaphores = &presentSem;
	submitInfo.pWaitDstStageMask = &waitStages;

	Queue& graphicsQueue = device.getGraphicsQueue();

	double seconds = glfwGetTime();
	double frametimeAccumulator = seconds;
	unsigned frameCounter = 0;
	while (!glfwWindowShouldClose(window.getHandle()))
	{
		unsigned i0 = frameCounter % 2, i1 = (i0 + 1) % 2;
		future<void> advanceFut = async(launch::async, [&]{
			grids[i1] = advance(grids[i0]);
		});
		renderer.updateCells(grids[i0]);

		vkBeginCommandBuffer(cmdBuffer, &cmdBufferBeginInfo);
		target.beginFrame(cmdBuffer);
		renderer.render(cmdBuffer);
		target.endFrame(cmdBuffer);
		vkEndCommandBuffer(cmdBuffer);
		vkQueueSubmit(graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE);
		vkQueueWaitIdle(graphicsQueue);
		target.present(renderCompleteSem);

		bpView::pollEvents();
		window.handleEvents();

		double time = glfwGetTime();
		float delta = time - seconds;
		seconds = time;
		if (++frameCounter % 50 == 0)
		{
			double diff = time - frametimeAccumulator;
			frametimeAccumulator = time;
			double fps = 50.0 / diff;
			cout << '\r' << setprecision(4) << fps << "FPS";
		}
		renderer.update(delta);
		advanceFut.wait();
	}

	return 0;
}
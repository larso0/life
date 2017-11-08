#include <bpView/bpView.h>
#include <bpView/Window.h>
#include <bp/Instance.h>
#include <bp/Device.h>
#include <bp/Swapchain.h>
#include <bp/RenderPass.h>
#include <iostream>
#include <future>
#include "CellRenderer.h"
#include "Life.h"

using namespace bp;
using namespace std;

int main(int argc, char** argv)
{
	bpView::init();

	Instance instance{true, bpView::requiredInstanceExtensions};
	auto print = [](const string& s) { cout << s << endl; };
	auto printErr = [](const string& s) { cerr << s << endl; };

	connect(instance.infoEvent, print);
	connect(instance.warningEvent, printErr);
	connect(instance.errorEvent, printErr);
	connect(bpView::errorEvent, instance.errorEvent);

	static const uint32_t WIDTH = 1024;
	static const uint32_t HEIGHT = 768;
	static const char* SWAPCHAIN_EXTENSION = "VK_KHR_swapchain";

	FlagSet<Window::Flags> windowFlags;
	windowFlags << Window::Flags::VISIBLE
		    << Window::Flags::DECORATED
		    << Window::Flags::AUTO_ICONIFY;

	Window window{instance, WIDTH, HEIGHT, "", nullptr, windowFlags};

	DeviceRequirements requirements;
	requirements.queues = VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_TRANSFER_BIT;
	requirements.features.samplerAnisotropy = VK_TRUE;
	requirements.features.geometryShader = VK_TRUE;
	requirements.surface = window.getSurface();
	requirements.extensions.push_back(SWAPCHAIN_EXTENSION);

	VkPhysicalDevice physical = queryDevices(instance, requirements)[0];
	Device device{physical, requirements};

	Swapchain target{device, window, WIDTH, HEIGHT,
			 FlagSet<Swapchain::Flags>() << Swapchain::Flags::VERTICAL_SYNC};
	RenderPass pass{target, {{0, 0}, {WIDTH, HEIGHT}}, {0.2f, 0.2f, 0.2f, 1.f}};

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

	SparseGrid grid;

	if (argc < 2)
	{
		grid.emplace_back(5, 4);
		grid.emplace_back(6, 5);
		grid.emplace_back(4, 6);
		grid.emplace_back(5, 6);
		grid.emplace_back(6, 6);
	} else
	{
		loadGrid(argv[1], grid);
	}

	CellRenderer renderer{pass, controls, {0.f, 0.f}};

	double seconds = glfwGetTime();
	while (!glfwWindowShouldClose(window.getHandle()))
	{
		renderer.updateCells(grid);
		renderer.render(target.getPresentSemaphore());
		grid = advance(grid);
		target.present(renderer.getRenderCompleteSemaphore());

		bpView::pollEvents();
		window.handleEvents();

		double time = glfwGetTime();
		float delta = time - seconds;
		seconds = time;

		renderer.update(delta);
	}

	return 0;
}
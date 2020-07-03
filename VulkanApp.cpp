// May 3, 2020

// #define GLFW_INCLUDE_VULKAN
// #include <GLFW/glfw3.h>

// #define GLM_FORCE_RADIANS
// #define GLM_FORCE_DEPTH_ZERO_TO_ONE
// #include <glm/vec4.hpp>
// #include <glm/mat4x4.hpp>

#include <iostream>
#include <fstream>
#include <stdexcept>

#include "VulkanApp.hpp"

// ***** Public methods *****

void VulkanApp::run() {
    initWindow();
    initVulkan();
    mainLoop();
    cleanup();
}

// ***** Private methods *****

// Static

std::vector<char> VulkanApp::readFile(const std::string& filename) {
    // ate: start at the end of file to see file size
    // binary: don't get text conversion
    std::ifstream shaderFile(filename, std::ios::ate | std::ios::binary);

    if(!shaderFile.is_open()) {
        throw std::runtime_error(std::string("ERROR: Failed to open ") + filename);
    }

    // Get the position in the file, which is at the end of file, to get size
    size_t fileSize = shaderFile.tellg();

    std::vector<char> buffer(fileSize);

    shaderFile.seekg(0);
    shaderFile.read(buffer.data(), fileSize);

    shaderFile.close();

    return buffer;
}

void VulkanApp::framebufferResizeCallback(GLFWwindow* window, int width, int height) {
    // Get the app reference that was given to the window
    VulkanApp* app = reinterpret_cast<VulkanApp*>(glfwGetWindowUserPointer(window));

    app->framebufferResized = true;
}


// Non-static

void VulkanApp::initWindow() {
    glfwInit();

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);

    window = glfwCreateWindow(WIDTH, HEIGHT, "Vulkan", nullptr, nullptr);
    // Give the window a pointer to this app so that it can set the resize flag
    glfwSetWindowUserPointer(window, this);
    glfwSetFramebufferSizeCallback(window, framebufferResizeCallback);
}

void VulkanApp::initVulkan() {
    createInstance();
    if (enableValidationLayers) {
        // debugMessenger.initializeFromInstance(instance);
    }
    createSurface();
    pickPhysicalDevice();
    createLogicalDevice();
    createSwapchain();
    createImageViews();
    createRenderPass();
    createGraphicsPipeline();
    createFramebuffers();
    createCommandPool();
    createCommandBuffers();
    createSyncObjects();
}

void VulkanApp::mainLoop() {
    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();
        drawFrame();
    }

    // Let all of the asynchronous processes finish, so they are done for 
    // the cleanup function
    device.waitIdle();
}

void VulkanApp::cleanup() {
    cleanupSwapchain();

    for (int i = 0; i < MAX_CONCURRENT_FRAMES; i++) { 
        device.destroySemaphore(imageAvailableSemaphores[i]);
        device.destroySemaphore(renderFinishedSemaphores[i]);
        device.destroyFence(inFlightFences[i]);
    }

    device.destroyCommandPool(commandPool);

    // Queues are destroyed with the logical device
    device.destroy();

    // Physical device is implicitly destroyed with the instance

    instance.destroySurfaceKHR(surface);

    if (enableValidationLayers) {
        // Make sure to call this before destroying the instance
        // debugMessenger.destroy();
    }

    instance.destroy();

    glfwDestroyWindow(window);
    glfwTerminate();
}

// Helper methods for initVulkan()

void VulkanApp::createInstance() {
    // Check that requested validation layers are available
    std::string missingLayer = checkValidationLayerSupport();
    if (enableValidationLayers && missingLayer != "") {
        throw std::runtime_error(std::string("ERROR: Validation layer unavailable. ") + std::string(missingLayer));
    }

    printSupportedExtensions();

    // This struct is optional, but can allow for various optimizations
    // through providing useful info to the driver
    vk::ApplicationInfo appInfo{};
    appInfo.pApplicationName = "Vulkan";
    appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.pEngineName = "No Engine";
    appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.apiVersion = VK_API_VERSION_1_0;
    appInfo.pNext = nullptr; // Can point to extension information

    // This struct tells the Vulkan driver which extensions and validation
    // layers to use
    vk::InstanceCreateInfo createInfo{};
    createInfo.pApplicationInfo = &appInfo;

    std::vector<const char*> requiredExtensions = getRequiredExtensions();

    createInfo.enabledExtensionCount = static_cast<uint32_t>(requiredExtensions.size());
    createInfo.ppEnabledExtensionNames = requiredExtensions.data();

    // The enabled validation layers
    if (enableValidationLayers) {
        createInfo.enabledLayerCount = (uint32_t)validationLayers.size();
        createInfo.ppEnabledLayerNames = validationLayers.data();

        // debugMessenger.updateInstanceCreateInfo(&createInfo);
    } else {
        createInfo.enabledLayerCount = 0;
    }

    instance = vk::createInstance(createInfo);
}

void VulkanApp::createSurface() {
    VkSurfaceKHR cSurface; // GLFW only uses the c version
    VkResult result = glfwCreateWindowSurface(instance, window, nullptr, &cSurface);
    if (result != VK_SUCCESS) {
        throw std::runtime_error(std::string("ERROR: Failed to create window surface. ") + std::to_string(result));
    }

    // Convert C surface to C++ surface
    surface = cSurface;
}

void VulkanApp::pickPhysicalDevice() {
    std::vector<vk::PhysicalDevice> devices = instance.enumeratePhysicalDevices();
    bool found = false;

    for (vk::PhysicalDevice device : devices) {
        if (isDeviceSuitable(device)) {
            physicalDevice = device;
            found = true;
            break;
        }
    }

    if (!found) {
        throw std::runtime_error("ERROR: Failed to find suitable graphics card.");
    }
}

void VulkanApp::createLogicalDevice() {
    QueueFamilyIndices indices = findQueueFamilies(physicalDevice);

    // The queues need the priority that helps select which one to use during
    // threading, even when there is just one
    float queuePriority = 1.f;
    auto uniqueIndices = indices.getUniqueIndices();

    // We add the queue families to this vector, but because there are few to
    // add, the loss in efficiency will be negligible
    std::vector<vk::DeviceQueueCreateInfo> queueCreateInfos;

    // For each different queue family we want to use, set it up by adding the
    // queueCreateInfo
    for (uint32_t uniqueQueueFamilyIndex : uniqueIndices) {
        vk::DeviceQueueCreateInfo queueCreateInfo{};

        // Add the next queue family and specify the number of queues from it
        queueCreateInfo.queueFamilyIndex = uniqueQueueFamilyIndex;
        queueCreateInfo.queueCount = 1;
        queueCreateInfo.pQueuePriorities = &queuePriority;
        queueCreateInfos.push_back(queueCreateInfo);
    }

    // Specify device features to be using (none currently)
    vk::PhysicalDeviceFeatures deviceFeatures{};

    vk::DeviceCreateInfo deviceCreateInfo{};
    deviceCreateInfo.pQueueCreateInfos = queueCreateInfos.data();
    deviceCreateInfo.queueCreateInfoCount = (uint32_t)queueCreateInfos.size();

    deviceCreateInfo.pEnabledFeatures = &deviceFeatures;

    // Enable the device specific extensions
    deviceCreateInfo.enabledExtensionCount = (uint32_t)deviceExtensions.size();
    deviceCreateInfo.ppEnabledExtensionNames = deviceExtensions.data();

    // Modern versions of Vulkan don't use device specific validation layers,
    // but this is done in case there is an old version
    if (enableValidationLayers) {
        deviceCreateInfo.enabledLayerCount = (uint32_t)validationLayers.size();
        deviceCreateInfo.ppEnabledLayerNames = validationLayers.data();
    } else {
        deviceCreateInfo.enabledLayerCount = 0;
    }

    // The logical device needs the physical device
    device = physicalDevice.createDevice(deviceCreateInfo);

    // Finally, fill in the queue handles with the first queue from the
    // respective queue families in the device
    device.getQueue(indices[QUEUE_FAMILY_GRAPHICS].value(), 0, &graphicsQueue);
    device.getQueue(indices[QUEUE_FAMILY_PRESENT].value(), 0, &presentQueue);
}

void VulkanApp::createSwapchain() {
    SwapchainProperties properties = getSwapchainProperties(physicalDevice);

    vk::SurfaceFormatKHR surfaceFormat = chooseSwapchainSurfaceFormat(properties);
    vk::PresentModeKHR presentMode = chooseSwapchainPresentMode(properties);
    vk::Extent2D extent = chooseSwapchainExtent(properties);

    // Save the extent and format data
    swapchainImageFormat = surfaceFormat.format;
    swapchainExtent = extent;

    // The number of images we want to be in the swapchain. The minimum would
    // mean that there might not always be an available target, so we add one
    uint32_t imageCount = properties.surfaceCapabilities.minImageCount + 1;

    // Check to make sure not to exceed maxImageCount (0 means no max)
    if (properties.surfaceCapabilities.maxImageCount > 0 &&
        imageCount > properties.surfaceCapabilities.maxImageCount) {

        imageCount = properties.surfaceCapabilities.maxImageCount;
    }

    // Now we need a createInfo struct
    vk::SwapchainCreateInfoKHR createInfo{};
    createInfo.surface = surface;
    createInfo.minImageCount = imageCount;

    createInfo.imageFormat = surfaceFormat.format;
    createInfo.imageColorSpace = surfaceFormat.colorSpace;
    createInfo.presentMode = presentMode;
    createInfo.imageExtent = extent;

    // Each image is composed of just one layer
    createInfo.imageArrayLayers = 1;

    // vk::ImageUsage::eColorAttachmentBit specifies that the swapchain is
    // writing to an image to render.
    // Use eTransferDstBit or other values if writing somewhere else first for
    // postprocessing and then copying to an image
    createInfo.imageUsage = vk::ImageUsageFlagBits::eColorAttachment;

    // Specify the queue families for the swapchain
    QueueFamilyIndices indices = findQueueFamilies(physicalDevice);
    uint32_t indicesArray[] = {
        indices[QUEUE_FAMILY_GRAPHICS].value(),
        indices[QUEUE_FAMILY_PRESENT].value()
    };

    // Specify what to do based on if the queues are the same
    if (indices[QUEUE_FAMILY_GRAPHICS] != indices[QUEUE_FAMILY_PRESENT]) {
        // This means images are shared between queues
        createInfo.imageSharingMode = vk::SharingMode::eConcurrent;

        createInfo.queueFamilyIndexCount = 2;
        createInfo.pQueueFamilyIndices = indicesArray;
    } else {
        // This means a queue has exclusive access to a queue, unless it is
        // explicitly transferred. Best for performance
        createInfo.imageSharingMode = vk::SharingMode::eExclusive;

        createInfo.queueFamilyIndexCount = 0; // Technically optional
        createInfo.pQueueFamilyIndices = nullptr; // Also optional
    }

    // We don't need any additional transformations (from supportedTransforms)
    createInfo.preTransform = properties.surfaceCapabilities.currentTransform;

    // We don't want alpha channel to be used for blending with other windows
    createInfo.compositeAlpha = vk::CompositeAlphaFlagBitsKHR::eOpaque;

    // Get the performance boost from not caring about the colors of obscured
    // pixels (such as those behind other windows)
    createInfo.clipped = VK_TRUE;

    // When the swapchain is recreated (like with a window resizing) the old
    // swapchain is needed. Assume it is never recreated... for now!
    // createInfo.oldSwapchain = VK_NULL_HANDLE;
    createInfo.oldSwapchain = nullptr;

    swapchain = device.createSwapchainKHR(createInfo);

    // Get the handles for the images from the swapchain
    // device.getSwapchainImagesKHR(swapchain, &imageCount, nullptr);
    // swapchainImages.resize(imageCount);
    // device.getSwapchainImagesKHR(swapchain, &imageCount, swapchainImages.data());
    swapchainImages = device.getSwapchainImagesKHR(swapchain);
}

void VulkanApp::createImageViews() {
    swapchainImageViews.resize(swapchainImages.size());

    for (int i = 0; i < swapchainImages.size(); i++) {
        vk::ImageViewCreateInfo createInfo{};
        createInfo.image = swapchainImages[i];
        
        // Specify how to interpret the image data (1D, 2D, 3D, cubemap)
        createInfo.viewType = vk::ImageViewType::e2D;

        createInfo.format = swapchainImageFormat;

        // If we wanted to change the values assigned to each color channel (to
        // another color or clamp it to 1 or 0).
        createInfo.components.r = vk::ComponentSwizzle::eIdentity;
        createInfo.components.g = vk::ComponentSwizzle::eIdentity;
        createInfo.components.b = vk::ComponentSwizzle::eIdentity;
        createInfo.components.a = vk::ComponentSwizzle::eIdentity;

        // Writing a color value to the image (as opposed to depth, etc...)
        createInfo.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
        
        // Not using mipmaps
        createInfo.subresourceRange.baseMipLevel = 0;
        createInfo.subresourceRange.levelCount = 1;

        // Only 1 layer (multiple layers might be for different views for right
        // and left eyes in 3D applications)
        createInfo.subresourceRange.baseArrayLayer = 0;
        createInfo.subresourceRange.layerCount = 1;

        swapchainImageViews[i] = device.createImageView(createInfo);

        // result = vkCreateImageView(device, &createInfo, nullptr, &swapchainImageViews[i]);
        // if (result != VK_SUCCESS) {
        //     throw std::runtime_error(std::string("ERROR: Failed to create image view. ") + std::to_string(result));
        // }
    }
}

void VulkanApp::createGraphicsPipeline() {
    // Pipeline:
    // - Input assembler
    // - Vertex shader (programmable)
    // - Tesselation (programmable)
    // - Geometry shader (programmable)
    // - Rasterization
    // - Fragment shader (programmable)
    // - Color blending

    // ***** Set up the programmed aspects of the pipeline *****

    // Set up vertex shader

    // Local variables because they are not needed after making the pipeline
    vk::ShaderModule vertShader = createShaderModule(readFile(VERT_PATH));

    vk::PipelineShaderStageCreateInfo vertShaderInfo{};
    vertShaderInfo.stage = vk::ShaderStageFlagBits::eVertex;
    vertShaderInfo.module = vertShader;
    // Theoretically can combine multiple fragment shaders into one module and
    // use different entry point
    vertShaderInfo.pName = SHADER_MAIN.c_str();
    // For specifying values for shader constants (to optimize before runtime)
    // Optional: by default is nullptr
    vertShaderInfo.pSpecializationInfo = nullptr; // Optional

    // Set up fragment shader

    vk::ShaderModule fragShader = createShaderModule(readFile(FRAG_PATH));

    vk::PipelineShaderStageCreateInfo fragShaderInfo{};
    fragShaderInfo.stage = vk::ShaderStageFlagBits::eFragment;
    fragShaderInfo.module = fragShader;
    fragShaderInfo.pName = SHADER_MAIN.c_str();
    fragShaderInfo.pSpecializationInfo = nullptr; // Optional

    // An array of the shader stage create infos to use
    vk::PipelineShaderStageCreateInfo shaderStageCreateInfos[] = {
        vertShaderInfo,
        fragShaderInfo,
    };

    // ***** Set up the fixed aspects of the pipeline *****

    // Set up Vertex Input State

    // For now, specify no vertex inputs (points defined in shader)
    vk::PipelineVertexInputStateCreateInfo vertexInputInfo{};
    // Specifies spacing between data (and whether per vertex or per instance)
    vertexInputInfo.vertexBindingDescriptionCount = 0;
    vertexInputInfo.pVertexBindingDescriptions = nullptr; // Optional
    // Types of data, which binding, and offset for each attribute
    vertexInputInfo.vertexAttributeDescriptionCount = 0;
    vertexInputInfo.pVertexAttributeDescriptions = nullptr; // Optional

    // Set up Input Assembler State

    // Describes the kind of geometry and whether to use primitive restart
    vk::PipelineInputAssemblyStateCreateInfo inputAssemblyInfo{};
    // ePointList draws points from vertices
    // eLineList draws lines from every 2 vertices (without reusing them)
    // eLineStrip draws linesbetween each pair of vertices (with reusing them)
    // eTriangleList draws triangles without reuse
    // eTriangleStrip draws triangles with 2nd, 3rd verts as 1st, 2nd of next
    inputAssemblyInfo.topology = vk::PrimitiveTopology::eTriangleList;
    // Primitive restart is for strip modes, allows to restart the strip
    inputAssemblyInfo.primitiveRestartEnable = VK_FALSE;

    // Set up Viewport and Scissor

    // Viewport defines the region that is drawn to
    vk::Viewport viewport{};
    viewport.x = 0.f;
    viewport.y = 0.f;
    viewport.width = (float)swapchainExtent.width;
    viewport.height = (float)swapchainExtent.height;
    // Always must be within [0, 1]. Usually stick to defaults
    viewport.minDepth = 0.f;
    viewport.maxDepth = 1.f;

    // Scissor defines the region of the image that is used
    vk::Rect2D scissor{};
    scissor.offset.setX(0);
    scissor.offset.setY(0);
    scissor.extent = swapchainExtent;

    vk::PipelineViewportStateCreateInfo viewportStateInfo{};
    viewportStateInfo.viewportCount = 1;
    viewportStateInfo.pViewports = &viewport;
    viewportStateInfo.scissorCount = 1;
    viewportStateInfo.pScissors = &scissor;

    // Set up Rasterizer 

    // Rasterizer turns vertices into fragments
    vk::PipelineRasterizationStateCreateInfo rasterizationStateInfo{};
    // Whether to crop fragments that are beyond near/far frames or clamp to
    // edges. Useful for shadowmapping, but requires enabling a GPU feature
    rasterizationStateInfo.depthClampEnable = VK_FALSE;
    // If true, rasterizer stage discards geometry that would go to framebuffer
    rasterizationStateInfo.rasterizerDiscardEnable = VK_FALSE;
    // How polygons are drawn, fill (eFill), lines (eLine), or points (ePoint)
    // Modes other than fill require enabling a GPU feature
    rasterizationStateInfo.polygonMode = vk::PolygonMode::eFill;
    // How thick to draw lines. Wider than 1.f requires enabling GPU feature
    // "wideLines", and the max width depends on hardware
    rasterizationStateInfo.lineWidth = 1.f;
    // Which faces to cull (eNone, eFront, eBack, eFrontAndBack)
    rasterizationStateInfo.cullMode = vk::CullModeFlagBits::eBack;
    // Specifies the order for front facing sides
    rasterizationStateInfo.frontFace = vk::FrontFace::eClockwise;
    // For altering the depth with constant or based on slope. Not wanted here
    rasterizationStateInfo.depthBiasEnable = VK_FALSE;
    rasterizationStateInfo.depthBiasConstantFactor = 0.f; // Optional
    rasterizationStateInfo.depthBiasClamp = 0.f; // Optional
    rasterizationStateInfo.depthBiasSlopeFactor = 0.f; // Optional

    // Set up Multisampling

    // Multisampling combines the results of multiple fragments that render to
    // the same pixel, and allows for anti-aliasing. Requires enabling GPU a
    // GPU feature
    // For now, disabled
    vk::PipelineMultisampleStateCreateInfo multisamplingInfo{};
    multisamplingInfo.sampleShadingEnable = VK_FALSE;
    multisamplingInfo.rasterizationSamples = vk::SampleCountFlagBits::e1;
    multisamplingInfo.minSampleShading = 1.f; // Optional
    multisamplingInfo.pSampleMask = nullptr; // Optional
    multisamplingInfo.alphaToCoverageEnable = VK_FALSE; // Optional
    multisamplingInfo.alphaToOneEnable = VK_FALSE; // Optional

    // Depth and stencil testing set up would also be needed here, if used

    // Set up Color Blending

    // Defines color blending for this framebuffer specifically
    vk::PipelineColorBlendAttachmentState colorBlendAttachment{};
    colorBlendAttachment.colorWriteMask =
        vk::ColorComponentFlagBits::eR | 
        vk::ColorComponentFlagBits::eG | 
        vk::ColorComponentFlagBits::eB | 
        vk::ColorComponentFlagBits::eA;
    // No need for blending specific to this framebuffer, so pass in fragment
    // unmodified
    colorBlendAttachment.blendEnable = VK_FALSE;
    colorBlendAttachment.srcColorBlendFactor = vk::BlendFactor::eOne; // Optional
    colorBlendAttachment.dstColorBlendFactor = vk::BlendFactor::eZero; // Optional
    colorBlendAttachment.colorBlendOp = vk::BlendOp::eAdd; // Optional
    colorBlendAttachment.srcAlphaBlendFactor = vk::BlendFactor::eOne; // Optional
    colorBlendAttachment.dstAlphaBlendFactor = vk::BlendFactor::eZero; // Optional
    colorBlendAttachment.alphaBlendOp = vk::BlendOp::eAdd; // Optional

    // For alpha blending:
    // colorBlendAttachment.blendEnable = VK_TRUE;
    // colorBlendAttachment.srcColorBlendFactor = vk::BlendFactor::eSrcAlpha; // Optional
    // colorBlendAttachment.dstColorBlendFactor = vk::BlendFactor::eOneMinusSrcAlpha; // Optional
    // colorBlendAttachment.colorBlendOp = vk::BlendOp::eAdd; // Optional
    // colorBlendAttachment.stcAlphaBlendFactor = vk::BlendFactor::eOne; // Optional
    // colorBlendAttachment.dstAlphaBlendFactor = vk::BlendFactor::eZero; // Optional
    // colorBlendAttachment.alphaBlendOp = vk::BlendOp::eAdd; // Optional

    // Sets up color blending for all framebuffers
    vk::PipelineColorBlendStateCreateInfo colorBlendInfo{};
    // Setting this to true disables attachment specific blending (as if each
    // was VK_FALSE)
    colorBlendInfo.logicOpEnable = VK_FALSE;
    colorBlendInfo.logicOp = vk::LogicOp::eCopy; // Optional
    colorBlendInfo.attachmentCount = 1;
    colorBlendInfo.pAttachments = &colorBlendAttachment;
    colorBlendInfo.blendConstants[0] = 0.f; // Optional
    colorBlendInfo.blendConstants[1] = 0.f; // Optional
    colorBlendInfo.blendConstants[2] = 0.f; // Optional
    colorBlendInfo.blendConstants[3] = 0.f; // Optional

    // Set up Dynamic State

    // Dynamic state allows for changing a handful of aspects of the pipeline
    // dynamically (ex: viewport size, line width, blend constants). This tells
    // Vulkan to ignore the configured state, and it must be specified
    // dynamically instead. Can be substituted with a nullptr to ignore
    vk::DynamicState dynamicStates[] = {
        vk::DynamicState::eViewport,
        // vk::DynamicState::eLineWidth,
    };
    vk::PipelineDynamicStateCreateInfo dynamicStateInfo{};
    dynamicStateInfo.dynamicStateCount = 1; // 2;
    dynamicStateInfo.pDynamicStates = dynamicStates;

    // ***** Set up Pipeline Layout ***

    // Set up Uniform Layout
    vk::PipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.setLayoutCount = 0; // Optional
    pipelineLayoutInfo.pSetLayouts = nullptr; // Optional
    pipelineLayoutInfo.pushConstantRangeCount = 0; // Optional
    pipelineLayoutInfo.pPushConstantRanges = nullptr; // Optional
    
    pipelineLayout = device.createPipelineLayout(pipelineLayoutInfo);

    // ***** Initialize the pipeline *****

    vk::GraphicsPipelineCreateInfo pipelineInfo{};
    // Set the shader stage
    pipelineInfo.stageCount = 2;
    pipelineInfo.pStages = shaderStageCreateInfos;
    // Set the fixed function stages
    pipelineInfo.pVertexInputState = &vertexInputInfo;
    pipelineInfo.pInputAssemblyState = &inputAssemblyInfo;
    pipelineInfo.pViewportState = &viewportStateInfo;
    pipelineInfo.pRasterizationState = &rasterizationStateInfo;
    pipelineInfo.pMultisampleState = &multisamplingInfo;
    pipelineInfo.pDepthStencilState = nullptr; // Optional
    pipelineInfo.pColorBlendState = &colorBlendInfo;
    pipelineInfo.pDynamicState = nullptr; // Optional
    // Set the pipeline layout (Vulkan handle, not a pointer to a struct)
    pipelineInfo.layout = pipelineLayout;
    // Set the render pass
    pipelineInfo.renderPass = renderPass;
    // Set the index of the subpass where this graphics pipeline will be used
    pipelineInfo.subpass = 0;
    // When creating a pipeline, it can be derived from an existing one with
    // which it may share features, and switching between them will be faster.
    // This is only done if vk::PipelineCreateFlagBits::eDerivative is set in
    // the flag bits of this create info is set. We only have one pipeline, so
    // we do not need to do any pipeline deriving
    pipelineInfo.basePipelineHandle = nullptr; // Optional
    pipelineInfo.basePipelineIndex = -1; // Optional

    // The pipeline cache allows for storing and reusing data about pipelines
    // between separate calls for pipeline creation. But it can be set to
    // VK_NULL_HANDLE (or nullptr in c++) if not desired. Additionally, often
    // when creating pipelines, multiple pipelines can be made together. We 
    // must extract the first pipeline from the vector, because although we
    // only make one, it can be used to make multiple
    auto vector = device.createGraphicsPipelines(nullptr, pipelineInfo);
    if (vector.size() == 0) {
        throw std::runtime_error("ERROR: VulkanApp::createGraphicsPipeline() created 0 pipelines");
    }
    graphicsPipeline = vector[0];

    // Destroy the shaders that are no longer needed
    device.destroyShaderModule(vertShader);
    device.destroyShaderModule(fragShader);
}

void VulkanApp::createFramebuffers() {
    // Resize so all the framebuffers can be held
    swapchainFramebuffers.resize(swapchainImageViews.size());

    for (int i = 0; i < swapchainFramebuffers.size(); i++) {
        vk::ImageView attachments = {
            swapchainImageViews[i]
        };

        vk::FramebufferCreateInfo framebufferInfo{};
        // Use the framebuffer only with a render pass it is compatible with.
        // This often includes having the same number and type of attachments
        framebufferInfo.renderPass = renderPass;
        // Specify the image view object to be bound to the correct attachment
        // for the render pass
        framebufferInfo.attachmentCount = 1;
        framebufferInfo.pAttachments = &attachments;
        // Set the correct size for the framebuffer based on the swapchain
        framebufferInfo.width = swapchainExtent.width;
        framebufferInfo.height = swapchainExtent.height;
        // 1 because the swapchain images are single images
        framebufferInfo.layers = 1;

        swapchainFramebuffers[i] = device.createFramebuffer(framebufferInfo);
    }
}

void VulkanApp::createCommandPool() {
    QueueFamilyIndices queueFamilyIndices = findQueueFamilies(physicalDevice);

    vk::CommandPoolCreateInfo commandPoolInfo{};
    commandPoolInfo.queueFamilyIndex = queueFamilyIndices[QUEUE_FAMILY_GRAPHICS].value();
    // vk::CommandPoolCreateFlagBits::eTransient allows for optimizing if
    // command buffers are rerecorded very often, or with eResetCommandBuffer
    // for buffers to be rerecorded without resetting them all. For now, the 
    // command buffers are only going to be recorded once
    commandPoolInfo.flags = {}; // Optional

    commandPool = device.createCommandPool(commandPoolInfo);
}

void VulkanApp::createCommandBuffers() {
    commandBuffers.resize(swapchainFramebuffers.size());
    vk::CommandBufferAllocateInfo bufferAllocateInfo{};
    bufferAllocateInfo.commandPool = commandPool;
    // Primary level buffers can be submitted to a queue for execution, but
    // cannot be called by other buffers. Secondary level buffers are cannot
    // be submitted to queues, but can be called by other buffers
    bufferAllocateInfo.level = vk::CommandBufferLevel::ePrimary;
    bufferAllocateInfo.commandBufferCount = (uint32_t)commandBuffers.size();

    commandBuffers = device.allocateCommandBuffers(bufferAllocateInfo);

    // For each command buffer, set the correct commands to be run
    for (int i = 0; i < commandBuffers.size(); i++) {
        // Specify the usage of each command buffer
        vk::CommandBufferBeginInfo bufferBeginInfo{};
        // Flags indicate how the buffer wil be used. If it is rerecorded after
        // being used once, then vk::CommandBufferUsage::eOneTimeSubmit. If it
        // is a secondary command buffer to only be used within a single render
        // pass, then eRenderPassContinue. If it can be rerecorded while
        // waiting to be executed, then eSimultaneousUsage
        bufferBeginInfo.flags = {}; // Optional
        // For secondary buffers to specify which state to inherit from the
        // calling primary buffer
        bufferBeginInfo.pInheritanceInfo = nullptr; // Optional

        // This also rerecords the command buffer if it's been recorded already
        commandBuffers[i].begin(bufferBeginInfo);

        // Begin the render pass, by configuring with render pass info

        vk::RenderPassBeginInfo renderPassBeginInfo{};
        // Set the render pass
        renderPassBeginInfo.renderPass = renderPass;
        // Set the framebuffer
        renderPassBeginInfo.framebuffer = swapchainFramebuffers[i];
        // Set the size of the render area. Outside the render area, values are
        // undefined, so this should be the window size
        renderPassBeginInfo.renderArea.offset.setX(0);
        renderPassBeginInfo.renderArea.offset.setY(0);
        renderPassBeginInfo.renderArea.extent = swapchainExtent;
        // Set the clear parameters for vk::AttachmentLoadOp::eClear. Clear
        // color is black
        std::array<float, 4> color = { 0.f, 0.f, 0.f, 1.f };
        vk::ClearValue clearColor = vk::ClearColorValue(color);
        renderPassBeginInfo.clearValueCount = 1;
        renderPassBeginInfo.pClearValues = &clearColor;

        // Begin the render pass

        // Specify how the commmand will be used. For just using a primary
        // buffer, use vk::SubpassContents::eInline to say that the commands
        // should be rolled in with the command buffer. This means no secondary
        // ones will be run. eSecondaryCommandBuffers means the commands will
        // be used with secondary command buffers
        commandBuffers[i].beginRenderPass(&renderPassBeginInfo, vk::SubpassContents::eInline);

        // Bind the graphics pipeline

        // Specifythat this is a graphics pipeline, not a compute pipeline
        commandBuffers[i].bindPipeline(vk::PipelineBindPoint::eGraphics, graphicsPipeline);

        // Draw

        /* draw(vertexCount, instanceCount, firstVertex, firstInstance)
         *
         * vertexCount: The number of vertices to draw
         * instanceCount: The number of instances to use, or 1 if not using
         * firstVertex: Offset for the vertex index (gl_VertexIndex starting
         *              value)
         * firstInstance: Offset for the instanced rendering (gl_InstanceIndex
         *                starting value)
         */
        commandBuffers[i].draw(3, 1, 0, 0);

        commandBuffers[i].endRenderPass();

        // Once we finish recording the command buffer. Will throw an error if
        // this fails to record
        commandBuffers[i].end();
    }
}

void VulkanApp::createSyncObjects() {
    imageAvailableSemaphores.resize(MAX_CONCURRENT_FRAMES);
    renderFinishedSemaphores.resize(MAX_CONCURRENT_FRAMES);
    inFlightFences.resize(MAX_CONCURRENT_FRAMES);
    imagesInFlight.resize(swapchainImages.size(), nullptr);

    vk::SemaphoreCreateInfo semaphoreInfo{};
    // Currently, no need to set any values (like flags or pNext)

    vk::FenceCreateInfo fenceInfo{};
    // Ensure that when the fence is created, it is signaled, so the program is
    // not waiting for ever for an unsignaled fence
    fenceInfo.flags = vk::FenceCreateFlagBits::eSignaled;

    for (int i = 0; i < MAX_CONCURRENT_FRAMES; i++) {
        imageAvailableSemaphores[i] = device.createSemaphore(semaphoreInfo);
        renderFinishedSemaphores[i] = device.createSemaphore(semaphoreInfo);
        inFlightFences[i] = device.createFence(fenceInfo);
    }
}

// Helper methods for mainLoop()

void VulkanApp::drawFrame() {
    // Wait for the previous frame to have been completed before starting the
    // next with the same index
    device.waitForFences(inFlightFences[currentFrame], VK_TRUE, UINT64_MAX);

    // We need to:
    // Get an image from the swapchain
    // Run the command buffer with that image as attachment in framebuffer
    // Return the image to the swapchain so it can be presented

    // These are executed asynchronously, so we need to ensure they are run in
    // the required order. This can be done with fences or semaphores. Fences
    // are generally for synchonizing this application with Vulkan functions, 
    // because the application can wait for them. Semaphores are generally used
    // instead to synchronize between command queues

    // Get an image from the swapchain

    /* acquireNextImageKHR(swapchain, timeout, semaphore, fence)
    * 
    * swapchain: The swapchain to get the image from
    * timeout: How long to wait for an image to become available. UINT64_MAX
    *          means no timeout end
    * semaphore: The semaphore to use, if any
    * fence: The fence to use, if any
    * pImageIndex: The index of the image in swapchain images array to set
    * 
    * returns: The result of the operation
    */
    uint32_t imageIndex;
    vk::Result result = device.acquireNextImageKHR(swapchain, UINT64_MAX, imageAvailableSemaphores[currentFrame], nullptr, &imageIndex);

    if (result == vk::Result::eErrorOutOfDateKHR) {
        recreateSwapchain();
        return;
    } else if (result != vk::Result::eSuccess && result != vk::Result::eSuboptimalKHR) {
        // eSuboptimalKHR still counts as a success, so we won't recreate here
        // because we already got the image

        vk::throwResultException(result, "vk::Device::acquireNextImageKHR");
    }

    // If the image is being used used currently, wait for it to become free
    if (imagesInFlight[imageIndex]) {
        device.waitForFences(imagesInFlight[imageIndex], VK_TRUE, UINT64_MAX);
    }
    imagesInFlight[imageIndex] = inFlightFences[currentFrame];

    // Submit the command buffer to the queue to be executed, with the provided
    // semaphores

    vk::SubmitInfo submitInfo{};
    vk::Semaphore waitSemaphores[] = { imageAvailableSemaphores[currentFrame] };
    // This could also be eTopOfPipe
    vk::PipelineStageFlags waitStages[] = { vk::PipelineStageFlagBits::eColorAttachmentOutput };
    // Which semaphore(s) to wait on, and where in the pipeline to wait
    submitInfo.waitSemaphoreCount = 1;
    submitInfo.pWaitSemaphores = waitSemaphores;
    submitInfo.pWaitDstStageMask = waitStages;
    // Which command buffers to be executed. Use the one with the corresponding
    // ready image
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffers[imageIndex];
    // Which semaphore to signal on completion
    vk::Semaphore signalSemaphores[] = { renderFinishedSemaphores[currentFrame] };
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores = signalSemaphores;

    // Reset the fence. Unlike semaphores, this does not occur automatically
    device.resetFences(inFlightFences[currentFrame]);

    // Takes (an array of) submit info(s), and a fence for synchronizing
    graphicsQueue.submit(submitInfo, inFlightFences[currentFrame]);

    // Resubmit the result back to the swapchain so it can be rendered

    vk::PresentInfoKHR presentInfo{};
    // Which semaphore(s) to wait on before presentation
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores = signalSemaphores; // Wait on 2nd semaphore
    // The swapchain(s) to present on and the index of the image being used.
    // This is usually just one swapchain
    vk::SwapchainKHR swapchains[] = { swapchain };
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = swapchains;
    presentInfo.pImageIndices = &imageIndex;

    result = presentQueue.presentKHR(presentInfo);
    
    // Doing this here so we don't miss out on waiting on a signaled semaphore
    if (result == vk::Result::eErrorOutOfDateKHR || result == vk::Result::eSuboptimalKHR || framebufferResized) {
        framebufferResized = false;
        // Can recreate with eSuboptimalKHR here for best results
        recreateSwapchain();
        return;
    } else if (result != vk::Result::eSuccess) {
        vk::throwResultException(result, "vk::Queue::presentKHR");
    }

    // Move to the next frame, so multiple frames can be worked on at once
    currentFrame = (currentFrame + 1) % MAX_CONCURRENT_FRAMES;
}


// Swapchain helper methods

void VulkanApp::recreateSwapchain() {
    // Handle minimization by pausing until the window has a non-zero size
    int width = 0, height = 0;
    glfwGetFramebufferSize(window, &width, &height);
    while (width == 0 || height == 0) {
        glfwGetFramebufferSize(window, &width, &height);
        glfwWaitEvents();
    }

    // So we don't touch resources as they are being used
    device.waitIdle();

    // Destroy the old swapchain
    cleanupSwapchain();

    // Needs to be recreated... of course
    createSwapchain();
    // Recreated because directly based on swapchain images
    createImageViews();
    // Recreated because depends on swapchain image format (even though that
    // usually won't change in these scenarios)
    createRenderPass();
    // Recreated because viewport and scissor rectangle are defined
    createGraphicsPipeline();
    // Recreated because also depends on swapchain image
    createFramebuffers();
    // Recreated because once again depends on swapchain image
    createCommandBuffers();
}

void VulkanApp::cleanupSwapchain() {
    for (auto framebuffer : swapchainFramebuffers) {
        device.destroyFramebuffer(framebuffer);
    }

    // Remove all of the command buffers from the command pool, because the 
    // command pool doesn't need to be recreated then
    device.freeCommandBuffers(commandPool, (uint32_t)commandBuffers.size(), commandBuffers.data());

    device.destroyPipeline(graphicsPipeline);
    device.destroyPipelineLayout(pipelineLayout);
    device.destroyRenderPass(renderPass);

    for (auto imageView : swapchainImageViews) {
        device.destroyImageView(imageView);
    }

    device.destroySwapchainKHR(swapchain);
}

SwapchainProperties VulkanApp::getSwapchainProperties(vk::PhysicalDevice physicalDevice) {
    SwapchainProperties properties;
    properties.surfaceCapabilities = physicalDevice.getSurfaceCapabilitiesKHR(surface);
    properties.surfaceFormats = physicalDevice.getSurfaceFormatsKHR(surface);
    properties.presentModes = physicalDevice.getSurfacePresentModesKHR(surface);

    return properties;
}

vk::SurfaceFormatKHR VulkanApp::chooseSwapchainSurfaceFormat(const SwapchainProperties& properties) {
    // Surface formats are the colors available for rendering, and how they are
    // stored

    // Look for sRGB format
    for (const auto& surfaceFormat : properties.surfaceFormats) {
        if (surfaceFormat.format == vk::Format::eB8G8R8A8Srgb &&
            surfaceFormat.colorSpace == vk::ColorSpaceKHR::eSrgbNonlinear) {

            return surfaceFormat;
        }
    }

    // If no sRGB formats, return any
    return properties.surfaceFormats[0];
}

vk::PresentModeKHR VulkanApp::chooseSwapchainPresentMode(const SwapchainProperties& properties) {
    // Present modes are the way that new frames are rendered, typically by 
    // being added to a queue of frames to be rendered

    // Four options, but VK_PRESENT_MODE_FIFO_KHR is guaranteed to be available.
    // We want VK_PRESENT_MODE_MAILBOX_KHR ideally, which is similar but doesn't
    // block if the render queue is full
    for (const auto& presentMode : properties.presentModes) {
        if (presentMode == vk::PresentModeKHR::eMailbox) {
            return presentMode;
        }
    }

    // If VK_PRESENT_MODE_MAILBOX_KHR can't be found, simply use
    // VK_PRESENT_MODE_FIFO_KHR
    return vk::PresentModeKHR::eFifo;
}

vk::Extent2D VulkanApp::chooseSwapchainExtent(const SwapchainProperties& properties) {
    // The extent is the resolution of swapchain images, and is almost always
    // the window size. The surface capabilities struct tells us the possible 
    // range of values

    // Typically the actual extent should be the same as the current extent, but
    // some window managers set the current one to the max int, so we pick a
    // different value based on the range
    if (properties.surfaceCapabilities.currentExtent.width != UINT32_MAX) {
        return properties.surfaceCapabilities.currentExtent;
    } else {
        int width, height;
        glfwGetFramebufferSize(window, &width, &height);

        // Use the current width and height, clamped to the available border
        vk::Extent2D actualExtent = {
            std::clamp(
                (uint32_t)width,
                properties.surfaceCapabilities.minImageExtent.width,
                properties.surfaceCapabilities.maxImageExtent.width
            ),
            std::clamp(
                (uint32_t)height,
                properties.surfaceCapabilities.minImageExtent.height,
                properties.surfaceCapabilities.maxImageExtent.height
            )
        };
        return actualExtent;
    }
}


// Graphics and Shaders helper methods

vk::ShaderModule VulkanApp::createShaderModule(const std::vector<char>& bytes) {
    vk::ShaderModuleCreateInfo createInfo{};
    createInfo.codeSize = bytes.size();

    // Convert the array of chars so that every 4 bytes are a uint32
    createInfo.pCode = reinterpret_cast<const uint32_t *>(bytes.data());

    return device.createShaderModule(createInfo);
}

void VulkanApp::createRenderPass() {
    // Set up the attachments for the render pass

    // We just want one color buffer attachment
    vk::AttachmentDescription colorAttachment{};
    colorAttachment.format = swapchainImageFormat;
    // No multisampling
    colorAttachment.samples = vk::SampleCountFlagBits::e1;
    // What to do with the contents of the buffer before rendering. It can be
    // cleared (eClear), saved (eLoad), or left undefined (eDontCare)
    colorAttachment.loadOp = vk::AttachmentLoadOp::eClear;
    // After rendering, the framebuffer can either be saved (eStore) or ignored
    // and left undefined (eDontCare). Save in order to render it.
    colorAttachment.storeOp = vk::AttachmentStoreOp::eStore;
    // While color and depth use the above operations, stenciling uses its own.
    // We don't use stenciling, so they can be left undefined
    colorAttachment.stencilLoadOp = vk::AttachmentLoadOp::eDontCare;
    colorAttachment.stencilStoreOp = vk::AttachmentStoreOp::eDontCare;
    // The layout of an image may depend on what we want to use it for. Options
    // include using the image for color attachments (eColorAttachmentOptimal),
    // for presenting in the swapchain (ePresentSrcKHR), or for a memory copy
    // operation (eTransferDstOptimal). The initial layout is the layout of the
    // previous image, which we don't care about
    colorAttachment.initialLayout = vk::ImageLayout::eUndefined;
    colorAttachment.finalLayout = vk::ImageLayout::ePresentSrcKHR;

    // During a rendering operation, there may be multiple subpasses in series,
    // like various post processing effects after each other. Each subpass uses
    // one or more attachments, which are referenced as vk::AttachmentReference
    vk::AttachmentReference colorAttachmentRef{};
    // Specifies the index in the attachment array. We have just 1, so index 0 
    colorAttachmentRef.attachment = 0;
    // We can specify the ideal layout for the attachment at the start of the
    // subpass. This attachment we want as a color buffer
    colorAttachmentRef.layout = vk::ImageLayout::eColorAttachmentOptimal;

    // Set up subpasses for the render pass

    // To describe the subpass
    vk::SubpassDescription subpassDescription{};
    // Specify that subpass is for graphics, as opposed to compute
    subpassDescription.pipelineBindPoint = vk::PipelineBindPoint::eGraphics;
    subpassDescription.colorAttachmentCount = 1;
    // The index of the attachment is the index referenced in the fragment
    // shader "layout(location = 0) out vec4 outColor"
    // Other attachment types are pInputAttachments for reading from a shader,
    // pResolveAttachments for multisampling color attachments, 
    // pDepthStencilAttachments for depth and stencil data, and
    // pPreserveAttachments for attachments not used by this subpass, but must
    // be preserved
    subpassDescription.pColorAttachments = &colorAttachmentRef;

    // In the render pass, each subpass automatically converts the image layout
    // but the desired layouts must be specified, including before and after
    // the subpass(es), because the operations before and after are implicit
    // subpasses. Normally, these happen at certain times, but the image set up
    // timing needs to be changed, because we when drawing, the image is not be
    // ready until the color attachment output stage
    vk::SubpassDependency dependency{};
    // Specify the subpass indices. VK_SUBPASS_EXTERAL refers to implicit ones
    dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    dependency.dstSubpass = 0;
    // Specify when we need the transition to occur. It can't occur before here
    // because we won't have the image yet, since the semaphore from
    // drawFrame() waits in that stage
    dependency.srcStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput;
    dependency.srcAccessMask = {}; // ******************************************* OK?
    // The stage that must wait on this dependency is the part of the color
    // attachment output stage in which the image is written to
    dependency.dstStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput;
    dependency.dstAccessMask = vk::AccessFlagBits::eColorAttachmentWrite;

    // Create the render pass itself
    
    vk::RenderPassCreateInfo createInfo{};
    createInfo.attachmentCount = 1;
    createInfo.pAttachments = &colorAttachment;
    createInfo.subpassCount = 1;
    createInfo.pSubpasses = &subpassDescription;
    createInfo.dependencyCount = 1;
    createInfo.pDependencies = &dependency;

    renderPass = device.createRenderPass(createInfo);
}


// Misc helper methods

bool VulkanApp::isDeviceSuitable(vk::PhysicalDevice physicalDevice) {
    // // Includes name, type, supported Vulkan version, and more
    // VkPhysicalDeviceProperties deviceProperties;
    // vkGetPhysicalDeviceProperties(physicalDevice, &deviceProperties);
    
    // // Includes optional features like texture compression and 64 bit floats
    // VkPhysicalDeviceFeatures deviceFeatures;
    // vkGetPhysicalDeviceFeatures(physicalDevice, &deviceFeatures);

    // For now any kind of graphics card works regardless of properties/features

    // Require a graphics queue to be present
    QueueFamilyIndices indices = findQueueFamilies(physicalDevice);

    bool supportsExtensions = deviceSupportsExtensions(physicalDevice);

    bool swapchainSuitable = false;
    if (supportsExtensions) {
        SwapchainProperties properties = getSwapchainProperties(physicalDevice);
        swapchainSuitable = !properties.surfaceFormats.empty() && !properties.presentModes.empty();
    }

    return supportsExtensions && swapchainSuitable && indices.isComplete();
}

bool VulkanApp::deviceSupportsExtensions(vk::PhysicalDevice physicalDevice) {
    std::vector<vk::ExtensionProperties> supportedExtensions = physicalDevice.enumerateDeviceExtensionProperties();

    // // Similar to printing supported instance devices, but gets the extensions
    // // from the device
    // uint32_t extensionCount = 0;
    // vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &extensionCount, nullptr);
    // std::vector<VkExtensionProperties> supportedExtensions(extensionCount);
    // vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &extensionCount, supportedExtensions.data());

    // Filter out the required extensions, and if there are none left, then all
    // are present
    std::unordered_set<std::string> requiredExtensions(deviceExtensions.begin(), deviceExtensions.end());
    for (const auto& supportedExtension : supportedExtensions) {
        requiredExtensions.erase(supportedExtension.extensionName);
        if (requiredExtensions.empty()) {
            return true;
        }
    }
    return false;
}

QueueFamilyIndices VulkanApp::findQueueFamilies(vk::PhysicalDevice physicalDevice) {
    QueueFamilyIndices indices;

    std::vector<vk::QueueFamilyProperties> queueFamilies = physicalDevice.getQueueFamilyProperties();

    for (int i = 0; i < queueFamilies.size(); i++) {
        if (queueFamilies[i].queueFlags & vk::QueueFlagBits::eGraphics) {
            // Save the index of a found graphics queue family
            indices[QUEUE_FAMILY_GRAPHICS] = i;
        }

        // We need to find a queue family that has present support
        if (physicalDevice.getSurfaceSupportKHR(i, surface)) {
            indices[QUEUE_FAMILY_PRESENT] = i;
        }

        // No need to keep looking through queues
        if (indices.isComplete()) {
            break;
        }
    }

    return indices;
}

std::vector<const char*> VulkanApp::getRequiredExtensions() {
    // Pass in the extensions required by GLFW
    uint32_t glfwExtensionCount = 0;
    const char** glfwExtensions;
    
    glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

    // If validation layers are enabled, make space for the additional extension
    uint32_t numberExtensions = glfwExtensionCount;
    if (enableValidationLayers) {
        numberExtensions++;
    }

    std::vector<const char*> extensions(numberExtensions);
    for (int i = 0; i < glfwExtensionCount; i++) {
        extensions[i] = glfwExtensions[i];
    }

    // If validation layers are enabled, add the debug extension
    if (enableValidationLayers) {
        extensions[extensions.size() - 1] = VK_EXT_DEBUG_UTILS_EXTENSION_NAME;
    }

    return extensions;
}

void VulkanApp::printSupportedExtensions() {
    // Check for supported extensions
    std::vector<vk::ExtensionProperties> supportedExtensions = vk::enumerateInstanceExtensionProperties();

    std::cout << "Supported extensions:\n";
    for (const auto& extension: supportedExtensions) {
        std::cout << "\t" << extension.extensionName << std::endl;
    }
}

std::string VulkanApp::checkValidationLayerSupport() {
    std::vector<vk::LayerProperties> availableLayers = vk::enumerateInstanceLayerProperties();

    // Check if the requested layers from the layers vector are available
    for (const char* requestedLayerName : validationLayers) {
        bool layerFound = false;
        for (const auto& layer : availableLayers) {
            if (strcmp(requestedLayerName, layer.layerName) == 0) {
                layerFound = true;
                break;
            }
        }
        if (!layerFound) {
            return std::string(requestedLayerName);
        }
    }
    return "";
}
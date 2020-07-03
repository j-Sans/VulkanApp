 // May 3, 2020

#pragma once

// #define GLFW_INCLUDE_VULKAN
// #include <GLFW/glfw3.h>

// #define GLM_FORCE_RADIANS
// #define GLM_FORCE_DEPTH_ZERO_TO_ONE
// #include <glm/vec4.hpp>
// #include <glm/mat4x4.hpp>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <vulkan/vulkan.hpp>

#include <vector>
#include <unordered_set>
#include <optional>

#include "DebugMessenger.hpp"

enum QueueFamilyTypes {
    QUEUE_FAMILY_GRAPHICS = 0,  // For graphics computation
    QUEUE_FAMILY_PRESENT,       // For rendering
    NUM_QUEUE_FAMILY_TYPES
};

/**
 * A simple struct that holds optionals of the desired queue families. Each
 * queue family is stored as an int which is the index of that queue family
 */
struct QueueFamilyIndices {
    std::vector<std::optional<uint32_t> > indices;

    // Index operator can directly return the index from the vector
    std::optional<uint32_t>& operator[](std::size_t i) { return indices[i]; }
    
    QueueFamilyIndices() {
        indices = std::vector<std::optional<uint32_t> >(NUM_QUEUE_FAMILY_TYPES);
    }

    // Presenting queues may be different from the graphics itself, so we need
    // to check for that as well

    bool isComplete() {
        for(auto optionalIndex : indices) {
            if(!optionalIndex) return false;
        }
        return true;
    }

    std::unordered_set<uint32_t> getUniqueIndices() {
        std::unordered_set<uint32_t> set;
        for(auto optionalIndex : indices) {
            if(optionalIndex) set.insert(optionalIndex.value());
        }
        return set;
    }
};

/** A struct holding information about the swapchain */
struct SwapchainProperties {
    vk::SurfaceCapabilitiesKHR surfaceCapabilities;
    std::vector<vk::SurfaceFormatKHR> surfaceFormats;
    std::vector<vk::PresentModeKHR> presentModes;
};

class VulkanApp { 
public:
    /**
     * Initializes and runs the window and the Vulkan program until the window
     * is closed, at which point everything is disposed of
     */
    void run();

private:

    // Static fields and methods

    // Inline so they can be initialized here
    /** The path to the vertex shader byte code */
    inline static const std::string VERT_PATH = "shaders/vert.spv";
    /** The path to the fragment shader byte code */
    inline static const std::string FRAG_PATH = "shaders/frag.spv";
    /** The name of the main function within the shaders */
    inline static const std::string SHADER_MAIN = "main";

    /** The number of frames that can be computed at the same time */
    static const int MAX_CONCURRENT_FRAMES = 2;

    /**
     * Reads a file into a vector of chars. Note that if the file is too large,
     * memory may overflow!
     * 
     * @param filename The name (and path) of the file to read
     * 
     * @return A vector of chars that is the contents of the file
     */
    static std::vector<char> readFile(const std::string& filename);

    /**
     * A callback function for GLFW to call on resizing, that indicates to the
     * app that a resizing has occurred, so the swapchain can be remade (at a 
     * proper time, not in this callback)
     */
    static void framebufferResizeCallback(GLFWwindow* window, int width, int height);


    // Non static fields and methods

    const uint32_t WIDTH = 800;
    const uint32_t HEIGHT = 600;

    const std::vector<const char*> validationLayers = {
        "VK_LAYER_KHRONOS_validation" // Default Khronos validation layer
    };

    const std::vector<const char*> deviceExtensions = {
        // The extension for swapchains, for rendering to a window
        VK_KHR_SWAPCHAIN_EXTENSION_NAME
    };

#ifdef NDEBUG
    const bool enableValidationLayers = false;
#else
    const bool enableValidationLayers = true;
#endif

    // Primary objects
    /** The window to render to */
    GLFWwindow* window;
    /** The Vulkan instance */
    vk::Instance instance;
    /** Debug messenger for custom debugging from validation layer messages */
    DebugMessenger debugMessenger;
    /** Surface for interfacing between Vulkan and a window */
    vk::SurfaceKHR surface;
    /** Handle to a graphics card */
    vk::PhysicalDevice physicalDevice;
    /** Handle to the logical device that interfaces with the physical device */
    vk::Device device;
    /** Handle to the queue used for graphics commands */
    vk::Queue graphicsQueue;
    /** Handle to the queue used for presenting. Often will be graphics queue */
    vk::Queue presentQueue;

    // Swapchain objects
    /** The swapchain object for rendering */
    vk::SwapchainKHR swapchain;
    /** Vector of handles to the images from the swapchain for drawing to */
    std::vector<vk::Image> swapchainImages;
    /** Vector of image views which dictate the part of the image to use */
    std::vector<vk::ImageView> swapchainImageViews;
    // std::vector<vk::ImageView> vec;
    /** The image format of swapchain images */
    vk::Format swapchainImageFormat;
    /** The extent (size) of the window */
    vk::Extent2D swapchainExtent;

    // Graphics objects
    /** Store details about each render pass */
    vk::RenderPass renderPass;
    /** The layout which interfaces with shader uniforms */
    vk::PipelineLayout pipelineLayout;
    /** The graphics pipeline itself */
    vk::Pipeline graphicsPipeline;
    /** A list of the framebuffers */
    std::vector<vk::Framebuffer> swapchainFramebuffers;
    /** Store the pool of commands used for drawing */
    vk::CommandPool commandPool;
    /** The command buffers used for drawing */
    std::vector<vk::CommandBuffer> commandBuffers;

    // Drawing objects
    // One semaphore for each possible concurrent frame
    /** The semaphore indicating when an image is available */
    std::vector<vk::Semaphore> imageAvailableSemaphores;
    /** The semaphore indicating when the image is finished being drawn */
    std::vector<vk::Semaphore> renderFinishedSemaphores;
    /** Fences to synchonize GPU drawing with CPU, so frame isn't drawn over
     *  while in use */
    std::vector<vk::Fence> inFlightFences;
    /** Fences to keep track of whether a particular swapchain image is being
     *  used, separate from whether a frame is being used */
    std::vector<vk::Fence> imagesInFlight;
    /** The current frame, for drawing multiple frames */
    int currentFrame = 0;

    /** If a resize has occurred, this flag indicates that the swapchain must
     * be reset, for cases in which an exception is not thrown */
    bool framebufferResized = false;

    // Primary functions

    /**
     * Initializes the GLFW window
     */
    void initWindow();

    /**
     * Initializes Vulkan, including the instance, all Vulkan devices, and
     * everything else used for rendering
     */
    void initVulkan();

    /**
     * The main run loop of the program that renders and updates
     */
    void mainLoop();

    /**
     * A function to dispose of all allocated Vulkan objects
     */
    void cleanup();


    // Helper methods for initVulkan()

    /**
     * Creates the Vulkan instance
     */
    void createInstance();

    /**
     * Creates the surface for interfacing between Vulkan and the window
     */
    void createSurface();

    /**
     * Chooses a graphics card to use
     */ 
    void pickPhysicalDevice();

    /**
     * Initialize the logical interface to the physical device
     * 
     * Requires: The physical device has already been created
     */
    void createLogicalDevice();

    /**
     * Creates the swap chain
     * 
     * Requires: The physical device has already been created
     */
    void createSwapchain();

    /**
     * Creates the interface dictating which part of the image to use
     */
    void createImageViews();

    /**
     * Creates the graphics pipeline
     */
    void createGraphicsPipeline();

    /**
     * Create framebuffer objects that store the images to be rendered
     */
    void createFramebuffers();

    /**
     * Create the pool of commands to run
     */
    void createCommandPool();

    /**
     * Create the commands which can be executed for rendering
     */
    void createCommandBuffers();

    /**
     * Initializes the semaphores and fences needed for synchronizing drawing
     */
    void createSyncObjects();

    // Helper methods for mainLoop()

    /**
     * This function first gets an image from the swapchain, then runs the
     * command buffer using that image as the attachment in the framebuffer.
     * Finally, the image is returned to the swapchain so it can be presented
     */
    void drawFrame();


    // Swapchain helper methods

    /**
     * Recreate the swapchain and all objects that depend on it, in the cases
     * where the swapchain must be created, typically because of a change in
     * window size
     */
    void recreateSwapchain();

    /**
     * Destroy everything that is recreated in recreateSwapchain(). Should only
     * be called before recreateSwapchain()
     */
    void cleanupSwapchain();

    /**
     * Gets the supported swapchain properties from the physical device
     * 
     * Requires: The swapchain extension is supported
     * 
     * @param physicalDevice The physical device
     * 
     * @return A SwapchainProperties struct of the supported properties
     */
    SwapchainProperties getSwapchainProperties(vk::PhysicalDevice physicalDevice);

    /**
     * Gets the window extent from the swapchain
     * 
     * @param properties A SwapchainProperties struct containing data from the
     *                   swapchain
     * 
     * @return The surface format, which stores the format that color is stored
     *         as and the available color space being used
     */
    vk::SurfaceFormatKHR chooseSwapchainSurfaceFormat(const SwapchainProperties& properties);

    /**
     * Gets the present mode setting from the swapchain
     * 
     * @param properties A SwapchainProperties struct containing data from the
     *                   swapchain
     * 
     * @return The mode by which frames are rendered to the screen (typically
     *         by popping new frames onto a queue in various ways)
     */
    vk::PresentModeKHR chooseSwapchainPresentMode(const SwapchainProperties& properties);

    /**
     * Gets the window extent from the swapchain
     * 
     * @param properties A SwapchainProperties struct containing data from the
     *                   swapchain
     * 
     * @return The extent of the window from the swapchain, or based on the
     *         width and height with the available max if the swapchain didn't
     *         specify
     */
    vk::Extent2D chooseSwapchainExtent(const SwapchainProperties& properties);


    // Graphics and Shaders helper methods

    /**
     * Creates a Vulkan shader module from provided compiled shader code
     * 
     * @param bytes Code from a compiled shader file
     * 
     * @return The vulkan shader module
     */
    vk::ShaderModule createShaderModule(const std::vector<char>& bytes);

    /**
     * Create a render pass object which stores data about how many color and
     * depth buffers to use, or how to proces samples during rendering 
     * operations
     */
    void createRenderPass();


    // Misc helper functions
    
    /**
     * @return Whether the given device is suitable
     */
    bool isDeviceSuitable(vk::PhysicalDevice physicalDevice);
    
    /**
     * @return Whether the given device supports all the required extensions
     */
    bool deviceSupportsExtensions(vk::PhysicalDevice physicalDevice);

    /**
     * Gets the indices of the required queue families that are available
     * 
     * @return A QueueFamilyIndices struct of the available queue families
     */
    QueueFamilyIndices findQueueFamilies(vk::PhysicalDevice physicalDevice);

    /**
     * This function gets the required validation layers, which will be the
     * ones required by GLFW, and potentially the debug utils extension if
     * validation layers are being used.
     *  
     * @return A vector of c strings containing the extension names
     */
    std::vector<const char*> getRequiredExtensions();

    /**
     * Prints the extensions that are supported
     */
    void printSupportedExtensions();

    /**
     * This function checks to ensure that all of the layers that are to be
     * used by this class are here.
     * 
     * It checks the required layers by looking in the validationLayers field
     * 
     * @return The name of the first missing validation layer, or the empty
     *         string if all are available
     */
    std::string checkValidationLayerSupport();
};
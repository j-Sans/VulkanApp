#include "DebugMessenger.hpp"

// ***** Static methods *****

/**
 * A callback function that handles debug messages from validation layers
 * 
 * @param messageSeverity How severe the message is. Can be one of:
 *  VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT (diagnostic message)
 *  VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT (informational message
 *  about resource creation)
 *  VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT (not an error, but a
 *  likely bug)
 *  VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT (invalid error that may
 *  cause crashing)
 * 
 * @param messageType The type of message. Can be one of:
 *  VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT (General message)
 *  VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT (Specification violated,
 *  a possible mistake)
 *  VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT (Potential use of Vulkan
 *  that is not optimal performance wise)
 * 
 * @param pCallbackData A struct with the data. Fields include:
 *  pMessage: the message itself, as a c string.
 *  pObjects: An array of Vulkan object handles relevent to the message
 *  objectCount: The number of objects in pObjects
 * 
 * @param pUserData A pointer to data specified by the user when callbacks
 *  were set up, allowing for additional information to be passed in
 * 
 * @return This should always be VK_FALSE unless the message should be
 *  cancelled. That is primarily useful just for testing validation layers
 */
VKAPI_ATTR VkBool32 VKAPI_CALL DebugMessenger::debugCallback(
    VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
    VkDebugUtilsMessageTypeFlagsEXT messageType,
    const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
    void* pUserData) {

    std::cerr << "validation layer: " << pCallbackData->pMessage << std::endl << std::endl;

    return VK_FALSE;
}

// ***** Public methods *****

void DebugMessenger::updateInstanceCreateInfo(vk::InstanceCreateInfo* pCreateInfo) {
    // To debug vkCreateInstance (and vkDestroyInstance), we can provide the 
    // functions with another debug messenger by giving it a createInfo for
    // the debug messenger in the pNext field
    fillMessengerCreateInfo(&debugCreateInfo);
    pCreateInfo->pNext = &debugCreateInfo;
}

void DebugMessenger::initializeFromInstance(vk::Instance instance) {
    // Save the instance for when destroying this object
    this->instance = instance;

    // Create the struct for making the debug messenger callback
    VkDebugUtilsMessengerCreateInfoEXT createInfo{};
    fillMessengerCreateInfo(&createInfo);

    VkResult result = createDebugUtilsMessengerEXT(instance, &createInfo, nullptr, &debugMessenger);
    if (result != VK_SUCCESS) {
        throw std::runtime_error(std::string("ERROR: Could not set up debug messenger. ") + std::to_string(result));
    }

    // The object is ready to be used
    set = true;
}

void DebugMessenger::destroy() {
    // Object can no longer be used
    set = false;

    destroyDebugUtilsMessengerEXT(instance, debugMessenger, nullptr);
}

// ***** Private methods *****

void DebugMessenger::fillMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT* createInfo) {
    // Fill in the struct for making the debug messenger callback
    createInfo->sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;

    // Not including general info messages
    createInfo->messageSeverity =
        VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
        VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
        VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;

    // Including all possible message types
    createInfo->messageType =
        VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
        VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
        VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;

    // The pointer to the callback function
    createInfo->pfnUserCallback = debugCallback;

    // No user data to pass in
    createInfo->pUserData = nullptr;
}

VkResult DebugMessenger::createDebugUtilsMessengerEXT(
    VkInstance instance,
    const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo,
    const VkAllocationCallbacks* pAllocator,
    VkDebugUtilsMessengerEXT* pDebugMessenger
) {
    // Load the creation function address. It is not loaded by default because
    // it is from an extension
    auto createDebugMessengerFunc = (PFN_vkCreateDebugUtilsMessengerEXT) vkGetInstanceProcAddr(
        instance,
        "vkCreateDebugUtilsMessengerEXT"
    );

    // If the function was loaded, use it to create the debug messenger
    if (createDebugMessengerFunc != nullptr) {
        return createDebugMessengerFunc(instance, pCreateInfo, pAllocator, pDebugMessenger);
    } else {
        std::cout << "ERROR: Could not load debug messenger create function." << std::endl;
        return VK_ERROR_EXTENSION_NOT_PRESENT;
    }
}

void DebugMessenger::destroyDebugUtilsMessengerEXT(
    VkInstance instance,
    VkDebugUtilsMessengerEXT pDebugMessenger,
    const VkAllocationCallbacks* pAllocator
) {
    // Load the creation function address. It is not loaded by default because
    // it is from an extension
    auto destroyDebugMessengerFunc = (PFN_vkDestroyDebugUtilsMessengerEXT) vkGetInstanceProcAddr(
        instance,
        "vkDestroyDebugUtilsMessengerEXT"
    );

    // If the function was loaded, use it to destroy the debug messenger
    if (destroyDebugMessengerFunc != nullptr) {
        destroyDebugMessengerFunc(instance, pDebugMessenger, pAllocator);
    }
}
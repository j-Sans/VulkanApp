
// May 15, 2020

#pragma once

#include <GLFW/glfw3.h>

#include <vulkan/vulkan.hpp>

#include <iostream>

class DebugMessenger {
public:
    /**
     * A callback that handles debug messages from the validation layers. For
     * more details on the parameters, see DebugMessenger.cpp
     */
    static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
        VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
        VkDebugUtilsMessageTypeFlagsEXT messageType,
        const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
        void* pUserData
    );

    /**
     * Should be called on the create info struct for creating and destroying
     * the instance, so that instance can be debugged as well
     * 
     * Note that this function should be called before the debug messenger is
     * initialized, because to initialize it from an instance, the instance 
     * must have already been created. This is for creating the instance itself
     * 
     * @param pCreateInfo A pointer to the create info struct to be used for 
     *                    creating the instance. This should be called on it
     *                    before it is used to create the instance
     */
    void updateInstanceCreateInfo(vk::InstanceCreateInfo* pCreateInfo);

    /**
     * Creates the debug messenger from the given instance
     * 
     * @param instance The Vulkan instance
     * 
     * @throw std::runtime_error if the messenger could not be created (or if
     *        the creation function could not be loaded)
     */
    void initializeFromInstance(vk::Instance instance);

    /**
     * Destroys the debug messenger. Should be called before this object leaves
     * scope.
     * 
     * Requires: The Vulkan instance used to create this object has not been
     *           destroyed
     */
    void destroy();

private:

    /** Whether the debug messenger has been created */
    bool set = false;

    /** A debug messenger create info for debugging vkCreateInstance. Can't be
        created in the update function or else it may go out of scope */
    VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo{};

    /** The debug callback handle */
    VkDebugUtilsMessengerEXT debugMessenger;

    /** Save the instance just for when it is destroyed */
    vk::Instance instance;

    /**
     * Initializes the createInfo struct for debug messenger creation
     * 
     * @param createInfo The struct to populate
     */
    static void fillMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT* createInfo);

    /**
     * Creates the debug messenger by loading in the create function from the 
     * correct extension
     * 
     * @param instance The current Vulkan instance
     * @param pCreateInfo The struct of information required for set up
     * @param pAllocator A callback for allocating memory. Can simply be null
     * @param pDebugMessenger A pointer to initialize as the debug messenger
     * 
     * @return VK_SUCCESS if the creation worked, an error otherwise.
     *  VK_ERROR_EXTENSION_NOT_PRESENT is returned if the function could not be
     *  loaded
     */
    static VkResult createDebugUtilsMessengerEXT(
        VkInstance instance,
        const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo,
        const VkAllocationCallbacks* pAllocator,
        VkDebugUtilsMessengerEXT* pDebugMessenger
    );

    /**
     * Destroys the debug messenger by loading in the destroy function from the 
     * correct extension
     * 
     * @param instance The current Vulkan instance
     * @param pDebugMessenger The debug messenger to delete
     * @param pAllocator A callback for deallocating memory. Can simply be null
     */
    static void destroyDebugUtilsMessengerEXT(
        VkInstance instance,
        VkDebugUtilsMessengerEXT pDebugMessenger,
        const VkAllocationCallbacks* pAllocator
    );
};
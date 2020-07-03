# May 2, 2020

CXX = g++
VULKAN_SDK_PATH = /Users/Jake/vulkansdk-macos-1.2.135.0/macOS

CXXFLAGS = -Wall -std=c++17 -I$(VULKAN_SDK_PATH)/include/
LDFLAGS = -L$(VULKAN_SDK_PATH)/lib `pkg-config --static --libs glfw3` -lvulkan

SRC_DIR = .
OBJ_DIR = .

TARGET = VulkanApp

OBJECTS = main.o VulkanApp.o DebugMessenger.o
# OBJECTS = example.o

$(TARGET): $(OBJ_DIR)/$(OBJECTS)
	$(CXX) $(CXXFLAGS) -o $(TARGET) $(OBJ_DIR)/$(OBJECTS) $(LDFLAGS)

# Example.o: Example.cpp
# 	$(CXX) $(CXXFLAGS) -c DebugMessenger.cpp

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.cpp $(SRC_DIR)/%.hpp

# DebugMessenger.o: DebugMessenger.cpp DebugMessenger.hpp
# 	$(CXX) $(CXXFLAGS) -c DebugMessenger.cpp

# VulkanApp.o: VulkanApp.cpp VulkanAppSwapchain.cpp VulkanApp.hpp
# 	$(CXX) $(CXXFLAGS) -c VulkanApp.cpp VulkanAppSwapchain.

# main.o: main.cpp VulkanApp.hpp
# 	$(CXX) $(CXXFLAGS) -c main.cpp

.PHONY: test clean

test: VulkanApp
	LD_LIBRARY_PATH=$(VULKAN_SDK_PATH)/lib \
	VK_LAYER_PATH=$(VULKAN_SDK_PATH)/share/vulkan/explicit_layer.d \
	VK_ICD_FILENAMES=$(VULKAN_SDK_PATH)/share/vulkan/icd.d/MoltenVK_icd.json \
	./$(TARGET)

clean:
	rm -f $(TARGET)
	rm -f *.o

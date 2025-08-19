CXX = clang++
OUTPUT = output

CXXFLAGS_COMMON = -O3 -std=c++23 -Wall -DNDEBUG -I./src -DSDL_MAIN_HANDLED -march=native -flto -fomit-frame-pointer -fno-rtti -fno-exceptions -ffast-math

ifeq ($(OS),Windows_NT)
	# windows

	TARGET = $(OUTPUT)/vulkan.exe

	VULKAN_LOCATION = -IC:\VulkanSDK\1.4.321.1\Include
	SDL3_LOCATION = -IC:\SDL3-3.2.20\include
	CXXFLAGS = $(CXXFLAGS_COMMON) $(VULKAN_LOCATION) $(SDL3_LOCATION)

	LDFLAGS = -fuse-ld=lld -LC:\VulkanSDK\1.4.321.1\Lib -LC:\SDL3-3.2.20\lib\x64 -lvulkan-1 -lSDL3 -lkernel32 -luser32 -lgdi32 -lwinmm -limm32 -lole32 -loleaut32 -lversion -luuid -ladvapi32 -lsetupapi -lshell32 -ldinput8
else
    # linux
    TARGET = $(OUTPUT)/vulkan
    CXXFLAGS = $(CXXFLAGS_COMMON)
    LDFLAGS  = -fuse-ld=lld -static-libstdc++ -static-libgcc -lvulkan -ldl -lpthread -lm -lSDL3
endif

OBJS_COMMON = main.o vk_frames.o vk_command.o vk_device.o vk_sync.o volk.o

.PHONY: all clean run
all: $(TARGET)

$(TARGET): $(addprefix $(OUTPUT)/,$(OBJS_COMMON))
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS)

$(OUTPUT)/%.o: src/%.cpp | $(OUTPUT)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(OUTPUT)/volk.o: ./src/volk.c
	$(CXX) -x c -O3 -Wall -march=native -flto -fomit-frame-pointer -c $< -o $@
	$(CXX) -x c -O3 -Wall -IC:\VulkanSDK\1.4.321.1\Include -IC:\SDL3-3.2.20\include -march=native -flto -fomit-frame-pointer -c $< -o $@

$(OUTPUT) $(OUTPUT_DEBUG):
	mkdir -p $@

run: $(TARGET)
	./$(TARGET) $(ARGS)

clean:
	rm -rf output
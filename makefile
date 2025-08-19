CXX = clang++
OUTPUT = output
TARGET = $(OUTPUT)/vulkan

CXXFLAGS = -O3 -std=c++23 -Wall -DNDEBUG -I./src -DSDL_MAIN_HANDLED -march=native -flto -fomit-frame-pointer -fno-rtti -fno-exceptions -ffast-math
LDFLAGS = -fuse-ld=lld -static-libstdc++ -static-libgcc -lvulkan -ldl -lpthread -lm -lSDL3

OBJS_COMMON = main.o vk_frames.o vk_command.o vk_device.o vk_sync.o volk.o

.PHONY: all clean run
all: $(TARGET)

$(TARGET): $(addprefix $(OUTPUT)/,$(OBJS_COMMON)) $(VOLK_OBJ)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS)

$(OUTPUT)/%.o: src/%.cpp | $(OUTPUT) $(VOLK_OBJ)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(OUTPUT)/volk.o: ./src/volk.c
	$(CXX) -x c -O3 -Wall -march=native -flto -fomit-frame-pointer -c $< -o $@

$(OUTPUT) $(OUTPUT_DEBUG):
	mkdir -p $@

run: $(TARGET)
	./$(TARGET) $(ARGS)

clean:
	rm -rf output
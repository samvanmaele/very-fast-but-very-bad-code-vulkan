CXX = clang++
OUTPUT_RELEASE = output/release
OUTPUT_DEBUG   = output/debug
OUTPUT_WEB     = output/web
TARGET_RELEASE = $(OUTPUT_RELEASE)/vulkan
TARGET_DEBUG   = $(OUTPUT_DEBUG)/vulkan

# Assume Vulkan SDK and SDL3 are installed via pacman
CXXFLAGS_RELEASE = -O3 -Wall -DNDEBUG -I./src -DSDL_MAIN_HANDLED -std=c++23 -march=native -flto -fomit-frame-pointer -fno-rtti -fno-exceptions -ffast-math
CXXFLAGS_DEBUG   = -O0 -g3 -Wall -I./src -DSDL_MAIN_HANDLED -std=c++23

SDL2_STATIC = /usr/local/lib/libSDL3.a

VOLK_OBJ = $(OUTPUT_RELEASE)/volk.o
$(OUTPUT_RELEASE)/volk.o: ./src/volk.c
	$(CXX) -x c -O3 -Wall -march=native -flto -fomit-frame-pointer -c $< -o $@

LDFLAGS = -fuse-ld=lld -static-libstdc++ -static-libgcc -lvulkan -ldl -lpthread -lm $(SDL2_STATIC)

OBJS_COMMON = main.o vk_frames.o vk_command.o vk_device.o vk_sync.o
OBJS_DEBUG  = vk_debug.o

SRC_DIR = src

.PHONY: all debug clean run run-debug
all: $(TARGET_RELEASE)
debug: $(TARGET_DEBUG)
emcc: $(OUTPUT_WEB)/main.html

$(TARGET_RELEASE): $(addprefix $(OUTPUT_RELEASE)/,$(OBJS_COMMON)) $(VOLK_OBJ)
	$(CXX) $(CXXFLAGS_RELEASE) -o $@ $^ $(LDFLAGS)

$(TARGET_DEBUG): $(addprefix $(OUTPUT_DEBUG)/,$(OBJS_COMMON) $(OBJS_DEBUG)) $(VOLK_OBJ)
	$(CXX) $(CXXFLAGS_DEBUG) -o $@ $^ $(LDFLAGS)

$(OUTPUT_RELEASE)/%.o: $(SRC_DIR)/%.cpp | $(OUTPUT_RELEASE) $(VOLK_OBJ)
	$(CXX) $(CXXFLAGS_RELEASE) -c $< -o $@

$(OUTPUT_DEBUG)/%.o: $(SRC_DIR)/%.cpp | $(OUTPUT_DEBUG) $(VOLK_OBJ)
	$(CXX) $(CXXFLAGS_DEBUG) -c $< -o $@

$(OUTPUT_RELEASE) $(OUTPUT_DEBUG):
	mkdir -p $@

# Optional: requires Emscripten setup
$(OUTPUT_WEB)/main.html: webgpu.cpp | $(OUTPUT_WEB)
	emcc -O3 -Wall \
	-I. \
	-I/path/to/emscripten/ports/emdawnwebgpu/emdawnwebgpu_pkg/webgpu_cpp/include \
	-DSDL_MAIN_HANDLED -std=c++23 \
	webgpu.cpp \
	-s FORCE_FILESYSTEM=1 \
	-s ALLOW_MEMORY_GROWTH=1 \
	-s USE_SDL=2 \
	-s ASSERTIONS=1 \
	--use-preload-plugins \
	--use-port=emdawnwebgpu \
	-o $@

$(OUTPUT_WEB):
	mkdir -p $@

run: $(TARGET_RELEASE)
	./$(TARGET_RELEASE) $(ARGS)

run-debug: $(TARGET_DEBUG)
	./$(TARGET_DEBUG)

server:
	cd $(OUTPUT_WEB)
	npx live-server . -o -p 9999

clean:
	rm -rf output
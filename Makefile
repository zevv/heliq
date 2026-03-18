
BIN = heliq

IMGUI_DIR = /home/ico/external/imgui

SRC += main.cpp
SRC += ui/app.cpp
SRC += ui/view.cpp
SRC += ui/panel.cpp
SRC += ui/widget.cpp
SRC += ui/widgetregistry.cpp
SRC += ui/misc.cpp
SRC += ui/config.cpp
SRC += ui/style.cpp
SRC += ui/glview.cpp
SRC += model/loader.cpp
SRC += model/experiment.cpp
SRC += model/simulation.cpp
SRC += model/solver.cpp
SRC += model/solver_cpu.cpp
SRC += model/solver_gpu.cpp
SRC += model/simcontext.cpp
SRC += widget/widget-dummy.cpp
SRC += widget/widget-info.cpp
SRC += widget/widget-grid.cpp
SRC += widget/widget-helix2.cpp
SRC += widget/widget-trace.cpp
SRC += $(IMGUI_DIR)/imgui.cpp 
SRC += $(IMGUI_DIR)/imgui_demo.cpp
SRC += $(IMGUI_DIR)/imgui_draw.cpp 
SRC += $(IMGUI_DIR)/imgui_tables.cpp
SRC += $(IMGUI_DIR)/imgui_widgets.cpp
SRC += $(IMGUI_DIR)/backends/imgui_impl_sdl3.cpp
SRC += $(IMGUI_DIR)/backends/imgui_impl_sdlrenderer3.cpp

PKG += fftw3 sdl3 lua5.4


PKG_CFLAGS := $(shell pkg-config $(PKG) --cflags)
PKG_LIBS := $(shell pkg-config $(PKG) --libs)

OBJS = $(SRC:.cpp=.o)
DEPS = $(OBJS:.o=.d)

CXXFLAGS += -std=c++23
CXXFLAGS += -g 
CXXFLAGS += -Wall -Wformat -Werror
CXXFLAGS += -Wno-unused-but-set-variable -Wno-unused-variable -Wno-format-truncation -Wno-c99-designator
CXXFLAGS += -O3 -ffast-math
CXXFLAGS += -march=native
CXXFLAGS += -MMD
CXXFLAGS += -Iui -Imodel -Iwidget
CXXFLAGS += -I$(IMGUI_DIR) -I$(IMGUI_DIR)/backends
CXXFLAGS += -DVKFFT_BACKEND=3
CXXFLAGS += $(PKG_CFLAGS)

LIBS += -ldl -latomic -lfftw3f_threads -lfftw3f -lpthread -lOpenCL -lEGL -lGLESv2 $(PKG_LIBS)

ifdef clang
CXX=clang++
LD=clang++
endif

ifdef asan
CXXFLAGS += -fsanitize=address 
LDFLAGS += -fsanitize=address 
endif

ifdef smem
CXXFLAGS += -fsanitize=memory -fsanitize-memory-track-origins=2
LDFLAGS += -fsanitize=memory -fsanitize-memory-track-origins=2
endif

ifdef lto
CXXFLAGS += -flto
LDFLAGS += -flto
endif

CCACHE := $(shell which ccache)
ifneq ($(CCACHE),)
CXX := $(CCACHE) $(CXX)
endif

CFLAGS = $(CXXFLAGS)

all: $(BIN)

%.o:%.cpp
	$(CXX) $(CXXFLAGS) -c -o $@ $<

%.o:$(IMGUI_DIR)/%.cpp
	$(CXX) $(CXXFLAGS) -c -o $@ $<

%.o:$(IMGUI_DIR)/backends/%.cpp
	$(CXX) $(CXXFLAGS) -c -o $@ $<

TEST_SRC += test/test_main.cpp
TEST_SRC += test/test_loader.cpp
TEST_SRC += test/test_grid.cpp
TEST_SRC += test/test_simulation.cpp
TEST_SRC += model/loader.cpp
TEST_SRC += model/simulation.cpp
TEST_SRC += model/solver.cpp
TEST_SRC += model/solver_cpu.cpp
TEST_SRC += model/solver_gpu.cpp

TEST_OBJS = $(TEST_SRC:.cpp=.o)
TEST_DEPS = $(TEST_OBJS:.o=.d)
TEST_BIN = test_quantum
TEST_PKG = lua5.4 fftw3
TEST_LIBS = $(shell pkg-config $(TEST_PKG) --libs) -lfftw3f_threads -lfftw3f -lpthread -lOpenCL


$(BIN): $(OBJS)
	$(CXX) -o $@ $^ $(CXXFLAGS) $(LIBS)

$(TEST_BIN): $(TEST_OBJS)
	$(CXX) -o $@ $^ $(CXXFLAGS) $(TEST_LIBS)

test: $(TEST_BIN)
	./$(TEST_BIN)

clean:
	rm -f $(BIN) $(TEST_BIN) $(OBJS) $(DEPS) $(TEST_OBJS) $(TEST_DEPS)

-include $(DEPS) $(TEST_DEPS)

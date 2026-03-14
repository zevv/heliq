
BIN = quantum

IMGUI_DIR = /home/ico/external/imgui

SRC += main.cpp
SRC += app.cpp
SRC += view.cpp
SRC += panel.cpp
SRC += widget.cpp
SRC += widgetregistry.cpp
SRC += widget-dummy.cpp
SRC += misc.cpp
SRC += config.cpp
SRC += style.cpp
SRC += $(IMGUI_DIR)/imgui.cpp 
SRC += $(IMGUI_DIR)/imgui_demo.cpp
SRC += $(IMGUI_DIR)/imgui_draw.cpp 
SRC += $(IMGUI_DIR)/imgui_tables.cpp
SRC += $(IMGUI_DIR)/imgui_widgets.cpp
SRC += $(IMGUI_DIR)/backends/imgui_impl_sdl3.cpp
SRC += $(IMGUI_DIR)/backends/imgui_impl_sdlrenderer3.cpp

PKG += fftw3 sdl3


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
CXXFLAGS += -I$(IMGUI_DIR) -I$(IMGUI_DIR)/backends
CXXFLAGS += $(PKG_CFLAGS)

LIBS += -ldl -latomic $(PKG_LIBS)

ifdef bitline
CXXFLAGS += -DBITLINE
endif

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

%.o:%.cpp
	$(CXX) $(CXXFLAGS) -c -o $@ $<

%.o:$(IMGUI_DIR)/%.cpp
	$(CXX) $(CXXFLAGS) -c -o $@ $<

%.o:$(IMGUI_DIR)/backends/%.cpp
	$(CXX) $(CXXFLAGS) -c -o $@ $<

all: $(BIN)

$(BIN): $(OBJS)
	$(CXX) -o $@ $^ $(CXXFLAGS) $(LIBS)

clean:
	rm -f $(BIN) $(OBJS) $(DEPS)

-include $(DEPS)

CXX ?= g++
NVCC ?= nvcc
CXXFLAGS ?= -O2 -std=c++17 -Wall -Wextra
NVCCFLAGS ?= -O2 -std=c++17
CPPFLAGS += -isystem third_party/zxing-cpp/core/src -isystem third_party
LDFLAGS ?=

ifeq ($(USE_LIBJPEG),1)
CXXFLAGS += -DUSE_LIBJPEG
LDFLAGS += -ljpeg
endif

ifeq ($(USE_SSE),1)
CXXFLAGS += -DUSE_SSE -msse2
endif

ifeq ($(USE_CUDA),1)
CXXFLAGS += -DUSE_CUDA
NVCCFLAGS += -DUSE_CUDA
CUDA_SRCS := $(wildcard src/*.cu)
CUDA_OBJS := $(CUDA_SRCS:.cu=.o)
LINKER := $(NVCC)
else
CUDA_SRCS :=
CUDA_OBJS :=
LINKER := $(CXX)
endif

SRCS := $(wildcard src/*.cpp)
OBJS := $(SRCS:.cpp=.o)
HEADERS := $(wildcard src/*.h)

ZXING_BUILD := build/zxing
ZXING_LIB := $(ZXING_BUILD)/core/libZXing.a
ZXING_SOURCES := $(shell find third_party/zxing-cpp -type f)

all: qrb

$(ZXING_LIB): $(ZXING_SOURCES)
	cmake -S third_party/zxing-cpp -B $(ZXING_BUILD) -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_STANDARD=17 -DBUILD_SHARED_LIBS=OFF -DZXING_WRITERS=OLD -DZXING_EXAMPLES=OFF -DZXING_DEPENDENCIES=LOCAL
	+$(MAKE) -C $(ZXING_BUILD)

qrb: $(OBJS) $(CUDA_OBJS) $(ZXING_LIB)
	$(LINKER) -o $@ $^ $(LDFLAGS)

%.o: %.cpp $(HEADERS)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c $< -o $@

%.o: %.cu $(HEADERS)
	$(NVCC) $(NVCCFLAGS) -c $< -o $@

clean:
	rm -f qrb $(OBJS) $(CUDA_OBJS)

.PHONY: all clean
src/input_image.o: third_party/stb_image.h

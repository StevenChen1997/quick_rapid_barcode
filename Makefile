CXX ?= g++
NVCC ?= nvcc
CXXFLAGS ?= -O2 -std=c++17 -Wall -Wextra
NVCCFLAGS ?= -O2 -std=c++17
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

all: qrb

qrb: $(OBJS) $(CUDA_OBJS)
	$(LINKER) -o $@ $^ $(LDFLAGS)

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

%.o: %.cu
	$(NVCC) $(NVCCFLAGS) -c $< -o $@

clean:
	rm -f qrb $(OBJS) $(CUDA_OBJS)

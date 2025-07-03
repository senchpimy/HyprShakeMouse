CXX := g++
CXXFLAGS := -Wall -std=c++17

SRCS := main.cpp tabletmode.cpp

ifdef DETECT_SHAKE
    CXXFLAGS += -DDETECT_SHAKE
    SRCS += shakedetector.cpp
endif

OBJS := $(SRCS:.cpp=.o)
TARGET := a.out

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $^

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(TARGET)

.PHONY: all clean

CXX      = g++
CXXFLAGS = -std=c++17 -O2 -Wall -Wextra $(shell sdl2-config --cflags)
LDFLAGS  = $(shell sdl2-config --libs) -lSDL2_ttf -lcurl

TARGET   = bin/stockchart
SRC      = src/main.cpp src/viewmode_stats.cpp src/viewmode_bollinger.cpp
OBJ      = $(SRC:.cpp=.o)

all: $(TARGET)

$(TARGET): $(OBJ) | bin
	$(CXX) $(CXXFLAGS) -o $@ $(OBJ) $(LDFLAGS)

bin:
	mkdir -p bin

src/%.o: src/%.cpp $(wildcard src/*.h)
	$(CXX) $(CXXFLAGS) -c -o $@ $<

clean:
	rm -f $(TARGET) $(OBJ)

.PHONY: all clean

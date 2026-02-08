CXX      = g++
CXXFLAGS = -std=c++17 -O2 -Wall -Wextra $(shell sdl2-config --cflags)
LDFLAGS  = $(shell sdl2-config --libs) -lSDL2_ttf -lcurl

TARGET   = stockchart
SRC      = main.cpp

all: $(TARGET)

$(TARGET): $(SRC)
	$(CXX) $(CXXFLAGS) -o $@ $< $(LDFLAGS)

clean:
	rm -f $(TARGET)

.PHONY: all clean

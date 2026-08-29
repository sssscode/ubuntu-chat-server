CXX ?= g++
CXXFLAGS ?= -std=c++17 -O2 -Wall -Wextra -pthread

TARGET = chat_server
SOURCES = main.cpp chat_server.cpp tcp_connection.cpp
OBJECTS = $(SOURCES:.cpp=.o)

all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CXX) $(CXXFLAGS) -o $@ $^

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -f $(TARGET) $(OBJECTS)

.PHONY: all clean

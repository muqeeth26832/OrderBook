# Compiler and flags
CXX = g++
CXXFLAGS = -std=c++20 -Wall -Wextra -pedantic -g

# Executable name
TARGET = OrderBook

# Source files
SRCS = main.cpp Orderbook.cpp

# Header files (optional, for tracking)
HEADERS = Orderbook.h Order.h OrderType.h Side.h Trade.h TradeInfo.h OrderModify.h Usings.h \
          LevelInfo.h OrderbookLevelInfos.h

# Object files
OBJS = $(SRCS:.cpp=.o)

# Default target
all: $(TARGET)

# Link object files into executable
$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $^

# Compile source files into object files
%.o: %.cpp $(HEADERS)
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Run the program
run: $(TARGET)
	./$(TARGET)

# Clean up
clean:
	rm -f $(OBJS) $(TARGET)

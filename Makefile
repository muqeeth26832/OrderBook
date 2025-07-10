# Compiler and flags
CXX = g++
CXXFLAGS = -std=c++20 -Wall -Wextra -pedantic -g

# Executable names
TARGET = OrderBook
TEST_TARGET = orderbook_test_bin

# Source files
SRCS = main.cpp Orderbook.cpp
TEST_SRCS = ./OrderbookTest/test.cpp Orderbook.cpp

# Header files (optional)
HEADERS = Orderbook.h Order.h OrderType.h Side.h Trade.h TradeInfo.h OrderModify.h Usings.h \
          LevelInfo.h OrderbookLevelInfos.h

# Object files
OBJS = $(SRCS:.cpp=.o)
TEST_OBJS = $(TEST_SRCS:.cpp=.o) Orderbook.o

# Default target
all: $(TARGET)

# Build main app
$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $^

# Build test binary with Google Test
$(TEST_TARGET): $(TEST_SRCS)
	$(CXX) $(CXXFLAGS) -o $@ $^ -lgtest -lgtest_main -lpthread -I/usr/include -L/usr/lib -Wl,--no-as-needed

# Compile .cpp into .o
%.o: %.cpp $(HEADERS)
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Run the main app
run: $(TARGET)
	./$(TARGET)

# Run unit tests
test: $(TEST_TARGET)
	./$(TEST_TARGET)

# Clean up all builds
clean:
	rm -f $(OBJS) $(TEST_OBJS) $(TARGET) $(TEST_TARGET)

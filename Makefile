# Makefile for GB/T 43229-2023 Traffic Protocol Implementation

CC = gcc
CFLAGS = -Wall -Wextra -std=c99 -D_GNU_SOURCE -g -O2
LDFLAGS = -pthread

# 目录定义
SRCDIR = src
COMMONDIR = $(SRCDIR)/common
SERVERDIR = $(SRCDIR)/server
CLIENTDIR = $(SRCDIR)/client
UTILSDIR = $(SRCDIR)/utils
EXAMPLESDIR = examples
BUILDDIR = build
BINDIR = bin

# 源文件
COMMON_SOURCES = $(COMMONDIR)/protocol.c $(COMMONDIR)/crc16.c
UTILS_SOURCES = $(UTILSDIR)/logger.c $(UTILSDIR)/socket_utils.c
SERVER_SOURCES = $(SERVERDIR)/signal_controller.c
CLIENT_SOURCES = $(CLIENTDIR)/vehicle_detector.c

# 对象文件
COMMON_OBJECTS = $(BUILDDIR)/common/protocol.o $(BUILDDIR)/common/crc16.o
UTILS_OBJECTS = $(BUILDDIR)/utils/logger.o $(BUILDDIR)/utils/socket_utils.o
SERVER_OBJECTS = $(BUILDDIR)/server/signal_controller.o
CLIENT_OBJECTS = $(BUILDDIR)/client/vehicle_detector.o

# 可执行文件
SERVER_DEMO = $(BINDIR)/server_demo
CLIENT_DEMO = $(BINDIR)/client_demo

# 库文件
COMMON_LIB = $(BUILDDIR)/libtraffic_common.a
UTILS_LIB = $(BUILDDIR)/libtraffic_utils.a
SERVER_LIB = $(BUILDDIR)/libtraffic_server.a
CLIENT_LIB = $(BUILDDIR)/libtraffic_client.a

# 默认目标
.PHONY: all clean install uninstall help test

all: directories $(SERVER_DEMO) $(CLIENT_DEMO)

# 创建目录
directories:
	@mkdir -p $(BUILDDIR)/common $(BUILDDIR)/server $(BUILDDIR)/client $(BUILDDIR)/utils $(BINDIR)

# 编译规则 - 通用模式
$(BUILDDIR)/%.o: $(SRCDIR)/%.c
	@echo "Compiling: $<"
	@$(CC) $(CFLAGS) -I$(SRCDIR) -c $< -o $@

# 编译静态库
$(COMMON_LIB): $(COMMON_OBJECTS)
	@echo "Creating common library: $@"
	@ar rcs $@ $^

$(UTILS_LIB): $(UTILS_OBJECTS)
	@echo "Creating utils library: $@"
	@ar rcs $@ $^

$(SERVER_LIB): $(SERVER_OBJECTS)
	@echo "Creating server library: $@"
	@ar rcs $@ $^

$(CLIENT_LIB): $(CLIENT_OBJECTS)
	@echo "Creating client library: $@"
	@ar rcs $@ $^

# 编译示例程序
$(SERVER_DEMO): $(EXAMPLESDIR)/server_demo.c $(COMMON_LIB) $(UTILS_LIB) $(SERVER_LIB)
	@echo "Building server demo: $@"
	@$(CC) $(CFLAGS) -I$(SRCDIR) -o $@ $< $(SERVER_LIB) $(UTILS_LIB) $(COMMON_LIB) $(LDFLAGS)

$(CLIENT_DEMO): $(EXAMPLESDIR)/client_demo.c $(COMMON_LIB) $(UTILS_LIB) $(CLIENT_LIB)
	@echo "Building client demo: $@"
	@$(CC) $(CFLAGS) -I$(SRCDIR) -o $@ $< $(CLIENT_LIB) $(UTILS_LIB) $(COMMON_LIB) $(LDFLAGS)

# 清理目标
clean:
	@echo "Cleaning build files..."
	@rm -rf $(BUILDDIR)/*.o $(BUILDDIR)/*.a $(BUILDDIR)/*/*.o
	@rm -f $(SERVER_DEMO) $(CLIENT_DEMO)
	@echo "Clean completed"

# 深度清理
distclean: clean
	@echo "Deep cleaning..."
	@rm -rf $(BUILDDIR) $(BINDIR)
	@echo "Deep clean completed"

# 安装目标
install: all
	@echo "Installing to /usr/local/bin..."
	@sudo cp $(SERVER_DEMO) /usr/local/bin/
	@sudo cp $(CLIENT_DEMO) /usr/local/bin/
	@echo "Installation completed"

# 卸载目标
uninstall:
	@echo "Uninstalling from /usr/local/bin..."
	@sudo rm -f /usr/local/bin/server_demo
	@sudo rm -f /usr/local/bin/client_demo
	@echo "Uninstallation completed"

# 运行测试
test: all
	@echo "Running basic functionality test..."
	@echo "Starting server in background..."
	@./$(SERVER_DEMO) -p 45000 -l 3 &
	@sleep 2
	@echo "Starting client for 10 seconds..."
	@timeout 10 ./$(CLIENT_DEMO) -s 127.0.0.1 -p 45000 -l 3 || true
	@echo "Stopping server..."
	@pkill -f server_demo || true
	@sleep 1
	@echo "Test completed"

# 调试版本
debug: CFLAGS += -DDEBUG -g3 -O0
debug: clean all

# 发布版本
release: CFLAGS += -DNDEBUG -O3 -s
release: clean all

# 代码检查
check:
	@echo "Running static code analysis..."
	@which cppcheck > /dev/null && cppcheck --enable=all --inconclusive --std=c99 $(SRCDIR) || echo "cppcheck not found, skipping..."
	@echo "Code check completed"

# 显示帮助
help:
	@echo "Available targets:"
	@echo "  all       - Build all components (default)"
	@echo "  clean     - Remove object files and executables"
	@echo "  distclean - Remove all build artifacts"
	@echo "  debug     - Build debug version"
	@echo "  release   - Build optimized release version"
	@echo "  install   - Install binaries to /usr/local/bin"
	@echo "  uninstall - Remove installed binaries"
	@echo "  test      - Run basic functionality test"
	@echo "  check     - Run static code analysis"
	@echo "  help      - Show this help message"
	@echo ""
	@echo "Example usage:"
	@echo "  make all          # Build everything"
	@echo "  make debug        # Build debug version"
	@echo "  make test         # Run tests"
	@echo "  make clean        # Clean build files"

# 显示编译信息
info:
	@echo "Build Configuration:"
	@echo "  CC:       $(CC)"
	@echo "  CFLAGS:   $(CFLAGS)"
	@echo "  LDFLAGS:  $(LDFLAGS)"
	@echo "  SRCDIR:   $(SRCDIR)"
	@echo "  BUILDDIR: $(BUILDDIR)"
	@echo "  BINDIR:   $(BINDIR)"
	@echo ""
	@echo "Source Files:"
	@echo "  Common:   $(COMMON_SOURCES)"
	@echo "  Utils:    $(UTILS_SOURCES)"
	@echo "  Server:   $(SERVER_SOURCES)"
	@echo "  Client:   $(CLIENT_SOURCES)"
	@echo ""
	@echo "Object Files:"
	@echo "  Common:   $(COMMON_OBJECTS)"
	@echo "  Utils:    $(UTILS_OBJECTS)"
	@echo "  Server:   $(SERVER_OBJECTS)"
	@echo "  Client:   $(CLIENT_OBJECTS)"

# 依赖关系
$(BUILDDIR)/common/protocol.o: $(COMMONDIR)/protocol.c $(COMMONDIR)/protocol.h $(COMMONDIR)/crc16.h
$(BUILDDIR)/common/crc16.o: $(COMMONDIR)/crc16.c $(COMMONDIR)/crc16.h
$(BUILDDIR)/utils/logger.o: $(UTILSDIR)/logger.c $(UTILSDIR)/logger.h
$(BUILDDIR)/utils/socket_utils.o: $(UTILSDIR)/socket_utils.c $(UTILSDIR)/socket_utils.h
$(BUILDDIR)/server/signal_controller.o: $(SERVERDIR)/signal_controller.c $(SERVERDIR)/signal_controller.h $(COMMONDIR)/protocol.h $(UTILSDIR)/socket_utils.h $(UTILSDIR)/logger.h
$(BUILDDIR)/client/vehicle_detector.o: $(CLIENTDIR)/vehicle_detector.c $(CLIENTDIR)/vehicle_detector.h $(COMMONDIR)/protocol.h $(UTILSDIR)/socket_utils.h $(UTILSDIR)/logger.h
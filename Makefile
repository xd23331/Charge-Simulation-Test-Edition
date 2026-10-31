CC = gcc
CFLAGS = -std=c11 -Wall -Wextra -pedantic -Isrc

BACKEND_SRC = \
	src/module1_graph/graph.c \
	src/module2_scheduler/scheduler.c \
	src/module3_entity/entity.c \
	src/module4_charger/charger.c \
	src/module5_recommend/recommend.c \
	src/module6_simulation/simulation.c \
	src/module7_fileio/fileio.c

# 前端源码：模块8 GTK 图形界面与模块9 定时控制
FRONTEND_SRC = \
	src/module8_ui/gtk_ui.c \
	src/module9_timer/timer.c

# GTK 4 的编译和链接参数（在 msys2 ucrt64 环境中使用）
GTK_CFLAGS = $(shell pkg-config --cflags gtk4)
GTK_LIBS = $(shell pkg-config --libs gtk4)

.PHONY: all gui gui_test test clean

all: build/backend_demo.exe

# 正式前端：模块8+模块9 组成的 GTK 图形界面程序
gui: build/charging_system_gui.exe

# 仅用于检查后端的 Win32 测试页面，不属于正式模块8
gui_test: build/backend_test_gui.exe

build/charging_system_gui.exe: $(FRONTEND_SRC) $(BACKEND_SRC)
	$(CC) $(CFLAGS) $(GTK_CFLAGS) $^ $(GTK_LIBS) -o $@

build/backend_demo.exe: src/app/main.c $(BACKEND_SRC)
	$(CC) $(CFLAGS) $^ -o $@

build/backend_test_gui.exe: src/test_ui/win32_test.c $(BACKEND_SRC)
	$(CC) $(CFLAGS) -fexec-charset=GBK -mwindows $^ -lgdi32 -o $@

build/test_graph.exe: tests/module1_graph/test_graph.c src/module1_graph/graph.c
	$(CC) $(CFLAGS) $^ -o $@

build/test_scheduler.exe: tests/module2_scheduler/test_scheduler.c src/module2_scheduler/scheduler.c
	$(CC) $(CFLAGS) $^ -o $@

build/test_entity.exe: tests/module3_entity/test_entity.c src/module1_graph/graph.c src/module3_entity/entity.c
	$(CC) $(CFLAGS) $^ -o $@

build/test_charger.exe: tests/module4_charger/test_charger.c src/module1_graph/graph.c src/module3_entity/entity.c src/module4_charger/charger.c
	$(CC) $(CFLAGS) $^ -o $@

build/test_recommend.exe: tests/module5_recommend/test_recommend.c src/module1_graph/graph.c src/module2_scheduler/scheduler.c src/module3_entity/entity.c src/module4_charger/charger.c src/module5_recommend/recommend.c
	$(CC) $(CFLAGS) $^ -o $@

build/test_simulation.exe: tests/module6_simulation/test_simulation.c src/module1_graph/graph.c src/module2_scheduler/scheduler.c src/module3_entity/entity.c src/module4_charger/charger.c src/module5_recommend/recommend.c src/module6_simulation/simulation.c
	$(CC) $(CFLAGS) $^ -o $@

build/test_fileio.exe: tests/module7_fileio/test_fileio.c src/module1_graph/graph.c src/module3_entity/entity.c src/module4_charger/charger.c src/module7_fileio/fileio.c
	$(CC) $(CFLAGS) $^ -o $@

build/test_backend.exe: tests/integration/test_backend.c $(BACKEND_SRC)
	$(CC) $(CFLAGS) $^ -o $@

test: build/test_graph.exe build/test_scheduler.exe build/test_entity.exe build/test_charger.exe build/test_recommend.exe build/test_simulation.exe build/test_fileio.exe build/test_backend.exe
	./build/test_graph.exe
	./build/test_scheduler.exe
	./build/test_entity.exe
	./build/test_charger.exe
	./build/test_recommend.exe
	./build/test_simulation.exe
	./build/test_fileio.exe
	./build/test_backend.exe

clean:
	$(RM) build/charging_system_gui.exe build/backend_demo.exe build/backend_test_gui.exe build/test_graph.exe build/test_scheduler.exe build/test_entity.exe build/test_charger.exe build/test_recommend.exe build/test_simulation.exe build/test_fileio.exe build/test_backend.exe


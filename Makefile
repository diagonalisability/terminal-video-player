CFLAGS= -Wno-parentheses
VIDEO_FILE= "data/video-file.mkv"
main.elf: main.cpp generic.cpp
	clang++ main.cpp -ggdb -o main.elf ${CFLAGS} `pkg-config --cflags --libs libavcodec libavutil libavformat libswscale` -fdiagnostics-color -std=c++2a
run: main.elf
	./main.elf ${VIDEO_FILE}
memcheck: main.elf
	valgrind --track-origins=yes --show-leak-kinds=all --leak-check=full ./main.elf ${VIDEO_FILE}
.PHONY: run

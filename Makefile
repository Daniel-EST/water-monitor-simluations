CC = g++

CFLAGS = -Wall --std=c++20
RAYLIB = `pkg-config  --cflags --libs raylib`

main:
	mkdir -p ./builds
	$(CC) ./src/main.cpp $(CFLAGS) $(RAYLIB) -o ./builds/main
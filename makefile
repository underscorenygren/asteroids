compile: clean game

run: compile
	./game

clean:
	rm -f game

game:
	cc main.c `pkg-config --libs --cflags raylib` -o game

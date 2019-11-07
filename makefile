compile: clean game

run: compile
	./game $(SESSION)

clean:
	rm -f game

game:
	gcc main.c -L./ -lraylib -lparsec -o game

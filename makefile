.PHONY: test

compile: clean game

run: compile
	./game $(SESSION)

clean:
	rm -f game
	rm -f test

game:
	gcc main.c -L./ -lraylib -lparsec -o game

test: clean
	gcc test.c -L./ -lraylib -lparsec -o test
	./test

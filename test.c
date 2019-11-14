/* Some tests to make to make sure objects and such are
 * rendered correctly.
 */
#include <assert.h>
#include <stdlib.h>

#include "types.h"
#include "game.c"

void test_run(GameState *state) {
	Vector2 direction = vector_random_direction();
	state->localPlayer->ship->direction = direction;

	game_draw(state);

	game_handle_objects(state);

	game_handle_frame_end(state);

	game_handle_players(state);

	game_handle_destructions(state);
}

void test_init(GameState *state) {

	state->localPlayer = game_add_player(state, NULL);
	assert(state->localPlayer);
	state->localPlayer->p_space = true;
	state->localPlayer->ship->x = 100;
	state->localPlayer->ship->y = 100;
	state->localPlayer->ship->destroyed = true;
}

int main(int argc, char *argv[])
{
	GameState state = { 0 };

	game_init(&state);

	test_init(&state);

	while (!WindowShouldClose())    // Detect window close button or ESC key
	{
		test_run(&state);
	}

	game_deinit(&state);
}

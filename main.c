#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <time.h>
#include <string.h>

#include "types.h"
#include "raylib.h"
#include "parsec.h"
//Since this is a local project, we don't bother with
//header files.
#include "parsecify.c"
#include "game.c"

/* Game functions stored in main to avoid circilar import hassles. */

/* Handles one frame of game logic. */
void game_loop(GameState *state) {
	game_handle_reset(state);

	game_draw(state);

	parsecify_submit_frame(state->parsec);

	game_handle_objects(state);

	game_handle_destructions(state);

	game_handle_asteroid_spawn(state);

	if(parsecify_check_events(state->parsec, state)) {
		game_trigger_welcome(state);
	}

	parsecify_check_input(state->parsec, state);

	game_handle_local_keypress(state);

	game_handle_players(state);

	game_handle_frame_end(state);
}

/* initilaizes game, raylib and parsec. returns true iff there was an init error. */
bool game_init(GameState *state, char *parsecSession) {
	bool failed = parsecify_init(state->parsec, parsecSession);
	if (failed) { return failed; }

	SetTraceLogLevel(LOG_WARNING);
	random_seed();
	InitWindow(SCREEN_W, SCREEN_H, GAME_NAME);
	SetTargetFPS(FPS);

	return false;
}

/* deinitalizes game */
void game_deinit(GameState *state) {
	CloseWindow();        // Close window and OpenGL context
	parsecify_deinit(state->parsec, state);
}

/* main loop */
int main(int argc, char *argv[])
{
    // Initialization
    //--------------------------------------------------------------------------------------
		char *session;
		GameState state = { 0 };

		if (argc < 2) {
			printf("Usage: ./ [session-id]\n");
			return 1;
		}

		session = argv[1];

		if (game_init(&state, session)) {
			return 1;
		}
    //--------------------------------------------------------------------------------------

    // Main game loop
    while (!WindowShouldClose())    // Detect window close button or ESC key
    {
			game_loop(&state);
    }

		game_deinit(&state);

    return 0;
}

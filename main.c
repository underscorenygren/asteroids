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
void loop(GameState *state) {
	DLOG("handling reset");
	game_handle_reset(state);

	DLOG("drawing");
	game_draw(state);

	DLOG("submitting frame");
	parsecify_submit_frame(state->parsec);

	DLOG("handling objectsl");
	game_handle_objects(state);

	DLOG("destructions");
	game_handle_destructions(state);

	DLOG("spawning asteroids");
	game_handle_asteroid_spawn(state);

	DLOG("parsec events");
	if(parsecify_check_events(state->parsec, state)) {
		game_trigger_welcome(state);
	}

	DLOG("parsec inputs");
	parsecify_check_input(state->parsec, state);

	DLOG("local inputs");
	game_handle_local_keypress(state);

	DLOG("handling players");
	game_handle_players(state);

	DLOG("frame end");
	game_handle_frame_end(state);
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
		game_init(&state);

		if (strcmp(session, DISABLE_PARSEC) == 0) {
			ILOG("skipping parsec init");
		} else {
			if(parsecify_init(&state.parsec, session)) {
				return 1;
			}
		}
    //--------------------------------------------------------------------------------------

    // Main game loop
    while (!WindowShouldClose())    // Detect window close button or ESC key
    {
			loop(&state);
    }

		game_deinit(&state);
		parsecify_deinit(state.parsec, &state);

    return 0;
}

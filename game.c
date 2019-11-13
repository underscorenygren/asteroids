/*
 * Functions for running the game.
 * All functions here will typically take GameState
 * as their first argument and operate on it,
 * either returning data or editing it by side effect.
 */
#ifndef GAME_C
#define GAME_C

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

#include "types.h"
#include "object.c"
#include "player.c"

/*
 * true iff cooldown frames have passed since obj framecounter was updated.
 */
bool game_is_object_in_cooldown(GameState *state, Object *obj, uint64_t cooldown) {
	uint64_t now = state->framecounter;
	int diff = now - obj->framecounter;
	//NB first check handles overflow of uint in global frame counter
	return !(now < obj->framecounter || diff > cooldown);
}

/* sets welcome text counter to trigger welcome drawing. */
void game_trigger_welcome(GameState *state) {
	state->welcomeTextCooldown = WELCOME_TEXT_COOLDOWN;
}


/* returns number of active players */
uint32_t game_get_n_players(GameState *state) {
	uint32_t res = 0;
	for (uint32_t i = 0; i < MAX_PLAYERS; i++) {
		if (state->players[i].active) { res++; }
	}
	return res;
}

/* returns active objects of type type */
uint32_t game_get_n_objects(GameState *state, uint32_t type) {
	uint32_t res = 0;
	for (uint32_t i = 0; i < MAX_OBJS; i++) {
		if (state->objs[i].active && state->objs[i].type == type) {
			res += 1;
		}
	}
	return res;
}

/* returns player from an object.
 * NULL if object belongs to no player.
 */
Player* game_get_player_from_object(GameState *state, Object *obj) {
	if (obj == NULL) {
		return NULL;
	}
	if (obj->type == SHIP) {
		for (uint32_t i = 0; i < MAX_PLAYERS; i++) {
			Player *player = &state->players[i];
			if (player->ship == obj) {
				return player;
			}
		}
	}
	//Missiles belong to users if they are same color.
	//Hacky, so should have a better system.
	if (obj->type == MISSILE) {
		for (uint32_t i = 0; i < MAX_PLAYERS; i++) {
			Player *player = &state->players[i];
			if (is_color_equal(player->col, obj->col)) {
				return player;
			}
		}
	}
	return NULL;
}

/* Object functions */

/* Iterates over active objects and finds first collider with o.
 * NULL if no collision.
 */
Object *game_get_first_collider(GameState *state, Object *o) {
	for (uint32_t j = 0; j < MAX_OBJS; j++) {
		Object *oj = &state->objs[j];
		if (o != oj) {
			if (object_is_colliding(o, oj)) {
				return oj;
			}
		}
	}
	return NULL;
}

/*
 * returns a 0 initialized game object from the state.
 * NULL if no objects available.
 */
Object* game_get_free_object(GameState *state) {
	Object *obj, *ref = NULL;

	for (uint32_t i = 0; i < MAX_OBJS; i++) {
		ref = &state->objs[i];
		if (!ref->active) {
			obj = ref;
			*obj = EMPTY_OBJECT; //nulls object
			break;
		}
	}
	return obj;
}

/*
 * Places an object in the game. Ensures doesn't collide with existing objects.
 */
Object* game_place_object(GameState *state, Object *obj, uint32_t type, Color col) {
	const uint32_t maxTries = 100;

	for (uint32_t i = 0; i < maxTries; i++) {
		obj = object_activate(obj, type, col);
		if (obj == NULL) {
			break;
		}
		if (game_get_first_collider(state, obj) == NULL) {
			break;
		} else {
			ILOG("failed collision test, retrying placement");
		}
	}

	return obj;
}

/*
 * Adds a new object to the game of type type.
 */
Object* game_add_object(GameState *state, uint32_t type, Color col) {
	return game_place_object(state, game_get_free_object(state), type, col);
}

/* Adds a missile from a ship.
 * Does maths to make sure it goes in right direction and doesn't
 * hit source ship.
 */
Object *game_add_missile(GameState *state, Object *ship) {
	Object *obj = NULL;
	Object *objs = state->objs;
	obj = game_get_free_object(state);
	Player *p = game_get_player_from_object(state, ship);

	if (obj == NULL) {
		return NULL;
	}
	if (p == NULL) {
		ILOG("no player for ship launching missile");
		return NULL;
	}

	//middle of ship, vector radiating out in direction of length
	Vector2 mid = {
		ship->x + SHIP_SIZE.x/2,
		ship->y + SHIP_SIZE.y/2};

	float scaling = sqrt(pow(SHIP_SIZE.x, 2) + pow(SHIP_SIZE.y, 2));

	//adds vector of ship length to mid point origin.
	//this takes rotation of ship already into account
	Vector2 startDirection = ship->direction;

	float angleOffset = MISSILE_ANGLE_OFFSET;
	//Rotate missiles by a fixed number to align with ship "front",
	//which is not at 0
	Vector2 missileDirection = vector_rotate(startDirection, angleOffset);
	//Vector2 missileDirection = randomDirection();

	Vector2 dir = {
		scaling * missileDirection.x,
		scaling * missileDirection.y,
	};

	Vector2 pos = {
		mid.x + dir.x,
		mid.y + dir.y,
	};

	object_init(obj, MISSILE, ship->speed + MISSILE_SPEED, missileDirection, MISSILE_SIZE, pos, 0, p->col);

	return obj;
}

/* Adds a player */
Player* game_add_player(GameState *state, ParsecGuest *guest) {
	for (uint32_t i = 0; i < MAX_PLAYERS; i++) {
		Player *p = &state->players[i];
		if (!p->active) {
			if (guest != NULL) {
				p->guest = *guest;
			}
			p->col = COLORS[random_uint32_t(N_COLORS)];
			Object *ship = game_add_object(state, SHIP, p->col);
			p->ship = ship;
			p->active = true;
			if (ship == NULL) {
				return NULL;
			}

			return &(state->players[i]); }
	}
	return NULL;
}

/* gets player from a parsec guest. NULL if no match. */
Player* game_get_player_from_guest(GameState *state, ParsecGuest *guest) {
	for (uint32_t i = 0; i < MAX_PLAYERS; i++) {
		Player *player = &state->players[i];
		if (state->players[i].active &&
				state->players[i].guest.id == guest->id) {
			return &state->players[i];
		}
	}
	return NULL;
}

/* returns local player. */
Player* game_get_local_player(GameState *state) {
	return &state->localPlayer;
}

/* true iff there is a local player who is active. */
bool game_is_local_player_active(GameState *state) {
	Player *p = game_get_local_player(state);
	return p != NULL && p->active;
}

/* removes a player from game. */
bool game_remove_player(GameState *state, Player *p) {
	if (p == NULL) {
		return false;
	}
	Object *obj = p->ship;
	p->active = false;
	p->ship = NULL;
	if (obj != NULL) {
		obj->active = false;
	}
	return true;
}


/*
 * destroys object obj.
 * Adjusts score based on collider.
 */
void game_destroy_object(GameState *state, Object *obj, Object *collider) {
	uint32_t amount = 0;
	Player *p = game_get_player_from_object(state, obj);
	object_destroy(obj);

	if (p == NULL) {
		return;
	}

	if (obj->type == SHIP) {
		player_adjust_score(p, -1);
	}

	if (obj->type == MISSILE && collider->type == SHIP) {
		Player *other = game_get_player_from_object(state, collider);
		if (other != p) {
			player_adjust_score(p, 1);
		}
	}
}

/* handling functions */

/* advances objects and checks collisions.
 * We do a bunch of double loops here, but
 * game is small it won't matter. */
void game_handle_objects(GameState *state) {
	Player *p;
	Object *objs = state->objs;
	for (uint32_t i = 0; i < MAX_OBJS; i++) {
		Object *oi = &objs[i];
		Object *oj = game_get_first_collider(state, oi);

		//destroy colliders
		if (oj != NULL) {
			game_destroy_object(state, oi, oj);
			game_destroy_object(state, oj, oi);
		}

		//advance all objects
		object_advance(oi);

		//respawn ships
		if (oi->active &&
				oi->type == SHIP &&
				object_is_destroyed(oi)) {
			game_place_object(state, oi, SHIP, oi->col);
		}
	}
}

/* adjusts ship state based on action. */
void game_handle_ship_action(GameState *state, Object *obj, ShipAction action) {
	if (obj == NULL) {
		ILOG("null ship");
		return;
	}
	DLOG("taking action %d", action);

	switch (action) {
		case TURN_LEFT:
			DLOG("turning left");
			object_adjust_direction(obj, -SHIP_ANGLE_ADJUSTMENT);
			break;
		case TURN_RIGHT:
			DLOG("turning right");
			object_adjust_direction(obj, SHIP_ANGLE_ADJUSTMENT);
			break;
		case SPEED_UP:
			DLOG("speeding up");
			object_adjust_speed(obj, SHIP_SPEED_ADJUSTMENT);
			break;
		case SPEED_DOWN:
			DLOG("speeding down");
			object_adjust_speed(obj, -SHIP_SPEED_ADJUSTMENT);
			break;
		case SHOOT:
			DLOG("shooting");
			if (!game_is_object_in_cooldown(state, obj, SHIP_MISSILE_COOLDOWN)) {
				game_add_missile(state, obj);
				obj->framecounter = state->framecounter;
			}
			break;
		case NO_ACTION:
			DLOG("no action");
			break;
		default:
			ILOG("Unkown action %d", action);
			break;
	}
}

/* translates player key states into ship actions. */
void game_handle_player(GameState *state, Player *p) {
	if (p == NULL || !p->active) {
		return;
	}
	DLOG("handling player");

	ShipAction action = NO_ACTION;
	if (p->p_w || p->p_up || p->p_g_up || p->p_g_a) {
		action = SPEED_UP;
	}
	if (p->p_s || p->p_down || p->p_g_down || p->p_g_b) {
		action = SPEED_DOWN;
	}
	if (p->p_a || p->p_left || p->p_g_left) {
		action = TURN_LEFT;
	}
	if (p->p_w || p->p_right || p->p_g_right) {
		action = TURN_RIGHT;
	}
	if (p->p_space || p->p_g_x) {
		action = SHOOT;
	}
	game_handle_ship_action(state, p->ship, action);
}

/* handles all players */
void game_handle_players(GameState *state) {
	for (uint32_t i = 0; i < MAX_PLAYERS; i++) {
		game_handle_player(state, &state->players[i]);
	}
}

/* handles local input as parsed by raylib. */
void game_handle_local_keypress(GameState *state) {
	ShipAction action = NO_ACTION;

	Player *localPlayer = NULL;

	//Adds a local player. Should maybe be done in add_player fn.
	if (IsKeyDown(KEY_O) && !game_is_local_player_active(state)) {
		ILOG("adding local player");
		localPlayer = game_add_player(state, NULL);
		if (localPlayer != NULL) {
			state->localPlayer = *localPlayer;
		}
	}
	if (IsKeyDown(KEY_U) && game_is_local_player_active(state)) {
		ILOG("removing local player");
		game_remove_player(state, game_get_local_player(state));
	}

	if (localPlayer == NULL) {
		return;
	}

	DLOG("handling local key player presses");

	localPlayer->p_w = IsKeyDown(KEY_W);
	localPlayer->p_up = IsKeyDown(KEY_UP);
	localPlayer->p_s = IsKeyDown(KEY_S);
	localPlayer->p_down = IsKeyDown(KEY_DOWN);
	localPlayer->p_a = IsKeyDown(KEY_A);
	localPlayer->p_left = IsKeyDown(KEY_LEFT);
	localPlayer->p_d = IsKeyDown(KEY_D);
	localPlayer->p_right = IsKeyDown(KEY_RIGHT);
	localPlayer->p_space = IsKeyDown(KEY_SPACE);
	localPlayer->p_q = IsKeyDown(KEY_Q);
}

/* increments destruction counters for objects, and deactivates
 * objects when they exceed threshold.
 */
void game_handle_destructions(GameState *state) {
	Object *objs = state->objs;
	for (uint32_t i = 0; i < MAX_OBJS; i++) {
		Object *obj = &objs[i];
		if (obj->active &&
				object_is_destroyed(obj)) {
			//NB risk for segfault here if indexing is wrong
			uint32_t destructThreshold = DESTRUCTION_THRESHOLDS[obj->type];
			if (objs[i].destroyed++ > destructThreshold) {
				obj->active = 0;
			}
		}
	}
}

/* increments frame counter */
void game_handle_frame_end(GameState *state) {
	//will overflow, but we don't care, loops back around.
	state->framecounter++;
}

/* resets game state if we're on 0 frame (first game run),
 * or all active players request reset.
 */
void game_handle_reset(GameState *state) {

	bool allWantReset = game_get_n_players(state) > 0; //don't reset on 0 players

	for (uint32_t i = 0; i < MAX_PLAYERS; i++) {
		Player *p = &state->players[i];
		if (p->active) {
			bool thisReset = player_is_reset_requested(p);
			ILOG("player %i wants reset: %i", i, thisReset);
			allWantReset = allWantReset && thisReset;
		}
	}

	if (state->framecounter == 0 || (allWantReset && state->framecounter > RESET_COOLDOWN)) {
		ILOG("resetting game");
		for (uint32_t i = 0; i < MAX_OBJS; i++) {
			state->objs[i].active = 0;
		}

		for (uint32_t i = 0; i < N_START_ASTEROIDS; i++) {
			game_add_object(state, ASTEROID, WHITE);
		}

		for (uint32_t i = 0; i < MAX_PLAYERS; i++) {
			Player *p = &state->players[i];
			if (p->active && p->ship) {
				p->ship->destroyed = true;
				p->ship->active = true;
			}
			p->score = 0;
		}

		state->framecounter = 1;
		game_trigger_welcome(state);
	}
}

/* handles asteroids spawning based on a probability that is
 * set in settings, and is proprtional to how many asteroids there are.
 * Logic is simply to inc/dev probability by fixed % based on how
 * many asteroids we are off from middle.
 */
void game_handle_asteroid_spawn(GameState *state) {
	float nAsteroidsMidpoint, baseSpawnP, asteroidSpawnP;
	int nAsteroids = game_get_n_objects(state, ASTEROID);

	if (nAsteroids >= MAX_ASTEROIDS) {
		return;
	}

	nAsteroidsMidpoint = (MAX_ASTEROIDS - N_START_ASTEROIDS) / 2;
	baseSpawnP = EXPECTED_ASTEROIDS_PER_SEC / FPS;
	asteroidSpawnP = baseSpawnP * (
		1 + ASTEROID_SPAWN_DRIVER *
			(nAsteroidsMidpoint - nAsteroids) / nAsteroidsMidpoint);

	if (random_prob(asteroidSpawnP)) {
		DLOG("spawn asteroid triggered");
		game_add_object(state, ASTEROID, WHITE);
	}
}

/** DRAWING **/

/* draws welcome if cooldown in effect */
void game_draw_welcome(GameState *state) {
	if (state->welcomeTextCooldown > 0) {
		DrawText(WELCOME_TEXT, 0, 0, SCOREBOARD_FONT_SIZE, WHITE);
		state->welcomeTextCooldown--;
	}
}

/* draws score for all players. */
void game_draw_scoreboard(GameState *state) {
	uint32_t nPlayers = game_get_n_players(state);
	if (nPlayers == 0) {
		return;
	}
	float chunk = SCREEN_W / nPlayers;
	char text[32];
	for (uint32_t i = 0; i < MAX_PLAYERS; i++) {
		Player *p = &state->players[i];
		if (p->active) {
			float x = i * chunk + chunk/2;
			const char *fmtString = player_is_reset_requested(p) ? RESET_TEXT : "%d";
			snprintf(&text[0], 32, fmtString, p->score);
			DrawText(text, x, SCOREBOARD_Y_OFFSET, SCOREBOARD_FONT_SIZE, p->col);
		}
	}
}

/* draws all active objects. */
void game_draw_objects(GameState *state) {

	for (uint32_t i = 0; i < MAX_OBJS; i++) {
		Object *obj = &state->objs[i];
		if (obj->active) {
			object_draw(obj);
		}
	}
}

/* draws game */
void game_draw(GameState *state) {
	BeginDrawing();
	ClearBackground(BLACK);

	game_draw_welcome(state);
	game_draw_scoreboard(state);
	game_draw_objects(state);

	EndDrawing();
}

#endif /* GAME_C */

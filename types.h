#ifndef TYPES_H
#define TYPES_H
#include <stdbool.h>
#include <stdint.h>

#include "raylib.h"
#include "parsec.h"

#define DEBUG true
#define INFO true

#define DLOG(f_, ...) (DEBUG ? printf((f_), ##__VA_ARGS__), printf("\n") : 0)
#define ILOG(f_, ...) (INFO ? printf((f_), ##__VA_ARGS__), printf("\n") : 0)

//These are really enums, but I got a bit lazy
//so they are just stored as types
const int ASTEROID = 1;
const int SHIP = 2;
const int MISSILE = 3;

//Game Settings
const int FPS = 60; //Can be set lower, useful for testing
const int MAX_PLAYERS = 8;
const int SCREEN_W = 1600;
const int SCREEN_H = (2 * SCREEN_W / 3);
const int SCOREBOARD_Y_OFFSET = 30;
const int SCOREBOARD_FONT_SIZE = 24;
const int RESET_COOLDOWN = FPS;
const int MAX_OBJS = 200;
const char* GAME_NAME = "Asteroids BATTLE!";
const char* WELCOME_TEXT = "Welcome to Asteroids Battle! Move: Keyboard(WASD,Arrows+Space) or DPAD+A/B/X. Reset game by holding Q or L and R Triggers";
const char* RESET_TEXT = "**wants[%d]reset**";
const int WELCOME_TEXT_COOLDOWN = 5 * FPS;
const char* DISABLE_PARSEC = "noparsec";

//Ship Settings
const float SHIP_SPEED_ADJUSTMENT = 0.4;
const float SHIP_ANGLE_ADJUSTMENT = 5.0f;
const int SHIP_MISSILE_COOLDOWN = 10; //NB: Measured in frames

//ASTEROID Settings
const int ASTEROID_MAX_SPEED = 8.0;
const int N_START_ASTEROIDS = 0;
const int MAX_ASTEROIDS = 0;
const float EXPECTED_ASTEROIDS_PER_SEC = 3;
const float ASTEROID_SPAWN_DRIVER = 0.05;

//MISSILE Settings
const int MISSILE_SPEED = 20.0;
const float MISSILE_RADIUS = 1.0;
const float MISSILE_ANGLE_OFFSET = 0.0;

//Sizes
const Vector2 ASTEROID_SIZE = {35.0, 35.0};
const Vector2 SHIP_SIZE = {20.0, 20.0};
const Vector2 MISSILE_SIZE = {MISSILE_RADIUS, MISSILE_RADIUS};

//Color Settings
const int N_COLORS = 8;
const Color COLORS[N_COLORS] = {
	GOLD,
	ORANGE,
	PINK,
	LIME,
	GREEN,
	SKYBLUE,
	VIOLET,
	DARKBROWN,
};

//We use types to index into, beware the segfault
int DESTRUCTION_THRESHOLDS[] = {
	0, //ignored
	5, //ASTEROID
	3, //SHIP
	0, //MISSILE
};

/* All Objects share the same struct, and store
 * all relevant game state information about themselves.
 * They have shared functions in object.c.
 * They are distinguished by their type field.
 */
typedef struct Object {
	float x, y, w, h, speed, angle;
	Vector2 direction;
	int destroyed;
	int type;
	bool active;
	uint64_t framecounter;
	Color col;
} Object;

//Shorthand for an all 0 object, used for re-initialization.
const Object EMPTY_OBJECT = { 0 };

/*
 * Game state for a player. Can be connected via Parsec
 * or be local.
 * Stores ship state, score, etc, and which keys are
 * pressed at the given time, if any. We use this to
 * normalize behaviour between Parsec Input handling
 * and raylib input handling.
 */
typedef struct Player {
	ParsecGuest guest;
	Object *ship;
	Color col;
	int score;
	bool active;
	bool p_w;
	bool p_up;
	bool p_s;
	bool p_down;
	bool p_a;
	bool p_left;
	bool p_d;
	bool p_right;
	bool p_space;
	bool p_q;
	bool p_g_up;
	bool p_g_down;
	bool p_g_left;
	bool p_g_right;
	bool p_g_a;
	bool p_g_b;
	bool p_g_x;
	bool p_g_lt;
	bool p_g_rt;
} Player;

/*
 * Stores state of the game.
 * We don't allocate anything dynamically, so all objects'
 * are stored in this state on the stack, then passed by
 * reference.  As such, all objects have an active/inactive
 * bool bit set.
 */
typedef struct GameState {
	Player players[MAX_PLAYERS];
	Object objs[MAX_OBJS];
	uint64_t framecounter;
	uint32_t welcomeTextCooldown;
  Parsec *parsec;
  Player *localPlayer;
} GameState;

/*
 * Enumeration for which action to take on
 * the ship, if any. Triggered by input checking
 * against player state.
 */
typedef enum ShipAction {
	NO_ACTION = 0,
	TURN_LEFT = 1,
	TURN_RIGHT = 2,
	SPEED_UP = 3,
	SPEED_DOWN = 4,
	SHOOT = 5,
} ShipAction;

/* utility functions */
bool is_color_equal(Color c1, Color c2) {
	return c1.r == c2.r &&
		c1.g == c2.g &&
		c1.b == c2.b;
}

//Pre-assign some functions that are needed by parsec and game.
Player* game_get_player_from_guest(GameState *state, ParsecGuest *guest);
Player* game_add_player(GameState *state, ParsecGuest *guest);
bool game_remove_player(GameState *state, Player *p);

#endif /* TYPES_H */

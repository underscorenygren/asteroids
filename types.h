#ifndef TYPES_H
#define TYPES_H
#include <stdbool.h>
#include <stdint.h>

#include "raylib.h"
#include "parsec.h"

#define DEBUG false
#define INFO true

#define DLOG(f_, ...) (DEBUG ? printf((f_), ##__VA_ARGS__), printf("\n") : 0)
#define ILOG(f_, ...) (INFO ? printf((f_), ##__VA_ARGS__), printf("\n") : 0)

/* Constants */
//Constants are stored as const [type] as opposed to Enums. I anticipated making them settings,
//so figured typing would be good here. I ended up not doing that, and not converting it because
//it didn't seem like it would matter much either way.

//Game Settings
const uint32_t FPS = 60; //Can be set lower, useful for testing
const uint32_t MAX_PLAYERS = 8; //this is quite arbitrary, just has implications on memory.
const uint32_t SCREEN_W = 1600;
const uint32_t SCREEN_H = (2 * SCREEN_W / 3); //arbitrary ratio
const uint32_t SCOREBOARD_Y_OFFSET = 30; //how far down to render scores
const uint32_t GAME_FONT_SIZE = 24;  //size of scoreboard is also font for
const uint32_t RESET_COOLDOWN = FPS;
const uint32_t MAX_OBJS = 200;
const uint32_t WELCOME_TEXT_COOLDOWN = 5 * FPS;
const char* GAME_NAME = "Asteroids BATTLE!";
const char* WELCOME_TEXT = "Welcome to Asteroids Battle! Move: WASD/Arrows/Space | DPAD/A/B/X. Reset Game: Q | L+R Trigger. (Un)Spawn Local Player: O+U";
const char* RESET_TEXT = "**wants[%d]reset**";
const char* DISABLE_PARSEC = "noparsec";

//Ship Settings
const float SHIP_SPEED_ADJUSTMENT = 0.4;
const float SHIP_ANGLE_ADJUSTMENT = 5.0f;
const uint32_t SHIP_MISSILE_COOLDOWN = 10; //NB: Measured in frames

//ASTEROID Settings
const uint32_t ASTEROID_MAX_SPEED = 8.0;
const uint32_t N_START_ASTEROIDS = 5;
const uint32_t MAX_ASTEROIDS = 30;
const float EXPECTED_ASTEROIDS_PER_SEC = 3;
const float ASTEROID_SPAWN_DRIVER = 0.05;

//MISSILE Settings
const int MISSILE_SPEED = 20.0;
const float MISSILE_RADIUS = 1.0;
const float MISSILE_ANGLE_OFFSET = 0.0;

//Types & Sizes
//These are really enums, but I got a bit lazy so they are just stored ints
typedef enum ObjectType {
  NONE = 0,
  ASTEROID = 1, //started at 1 so 0 set object has no type.
  SHIP = 2,
  MISSILE = 3,
} ObjectType;
const uint32_t N_TYPES = 3; //used for destruction threshold.

const Vector2 ASTEROID_SIZE = {35.0, 35.0};
const Vector2 SHIP_SIZE = {20.0, 20.0};
const Vector2 MISSILE_SIZE = {MISSILE_RADIUS, MISSILE_RADIUS};

//We use types to index into, beware the segfault
int DESTRUCTION_THRESHOLDS[N_TYPES+1] = {
	0, //ignored
	5, //ASTEROID
	3, //SHIP
	0, //MISSILE
};

//Colors for Players
const int N_COLORS = 8; //NB: used for indexing into below array, so must match.
const Color COLORS[N_COLORS] = {
	GOLD,
	ORANGE,
	PINK,
	LIME,
	GREEN,
	SKYBLUE,
	VIOLET,
	BEIGE,
};

/* All Objects share the same struct, and store
 * all relevant game state information about themselves.
 * They have shared functions in object.c.
 * They are distinguished by their type field.
 */
typedef struct Object {
	float x, y, //position in space
        w, h, //size
        speed, //speed
        angle; //obj rotation
	Vector2 direction; //direction object is moving.
	int destroyed; //destruction is counter to animate destroyed.
	int type;  //what kind of object it is
	bool active;  //objects are on stack, so need active counter for simple gc.
	uint64_t framecounter;  //an overloaded field. Stores frame when relevant events happened.
  //for ships, when last shot was made (for throttling)
  //for missiles, when it was launched (to account for not hitting source).
	Color col; //objects color
} Object;


/*
 * Game state for a player. Can be connected via Parsec
 * or be local.
 * Stores ship state, score, etc, and which keys are
 * pressed at the given time, if any. We use this to
 * normalize behaviour between Parsec Input handling
 * and raylib input handling.
 */
typedef struct Player {
	ParsecGuest guest; //parsec guest data. Stored on object b/c received data goes away.
	Object *ship;  //pointer to ship object in object array
	Color col;  //color
	int score;  //for scoreboard
	bool active; //active for garbage collection on stack.
	bool p_w;  //keyboard presses
	bool p_up;
	bool p_s;
	bool p_down;
	bool p_a;
	bool p_left;
	bool p_d;
	bool p_right;
	bool p_space;
	bool p_q;
	bool p_g_up; //gamepad presses
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

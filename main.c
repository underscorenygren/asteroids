#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <time.h>
#include <string.h>

#include "raylib.h"
#include "parsec.h"

#define MAX_PLAYERS 8

#define ANGULAR_MOVEMENT 6
#define SPEED_MOVEMENT 0.5f

#define DEBUG false
#define INFO true

#define DLOG(f_, ...) (DEBUG ? printf((f_), ##__VA_ARGS__), printf("\n") : 0)
#define ILOG(f_, ...) (INFO ? printf((f_), ##__VA_ARGS__), printf("\n") : 0)

const int ASTEROID = 1;
const int SHIP = 2;
const int MISSILE = 3;

const int SCREEN_W = 1600;
const int SCREEN_H = (2 * SCREEN_W / 3);

const int ASTEROID_MAX_SPEED = 10.0;
const int N_START_ASTEROIDS = 5;
const int MAX_ASTEROIDS = 30;
const float EXPECTED_ASTEROIDS_PER_SEC = 3;
const float ASTEROID_SPAWN_DRIVER = 0.05;
const int MISSILE_COOLDOWN = 10; //NB: Measured in frames
const float MAX_ASTEROID_ANGULAR_MOMENTUM = 15.0;

const int MAX_OBJS = 100;
const int MISSILE_SPEED = 20.0;
const float MISSILE_RADIUS = 1.0;
const float MISSILE_ANGLE_OFFSET = 0.0;

const Vector2 ASTEROID_SIZE = {45.0, 45.0};
const Vector2 SHIP_SIZE = {20.0, 20.0};
const Vector2 MISSILE_SIZE = {MISSILE_RADIUS, MISSILE_RADIUS};

//We use types to index into, beware the segfault
int DESTRUCTION_THRESHOLDS[] = {
	0, //ignored
	5, //ASTEROID
	3, //SHIP
	0, //MISSILE
};

typedef struct Object {
	float x, y, w, h, speed, angle;
	Vector2 direction;
	int destroyed;
	int type;
	bool active;
	uint64_t framecounter;
	float rotation;
} Object;

typedef struct Player {
	ParsecGuest guest;
	Object *ship;
	bool active;
} Player;

typedef struct GameState {
	Player players[MAX_PLAYERS];
	Object objs[MAX_OBJS];
	uint64_t framecounter;
} GameState;

typedef enum ShipAction {
	NO_ACTION = 0,
	TURN_LEFT = 1,
	TURN_RIGHT = 2,
	SPEED_UP = 3,
	SPEED_DOWN = 4,
	SHOOT = 5,
} ShipAction;

char *typeToString(Object *obj) {
	if (obj->type == ASTEROID) {
		return "ASTEROID";
	} else if(obj->type == SHIP) {
		return "SHIP";
	} else if (obj->type == MISSILE) {
		return "MISSILE";
	}
	return "UNKNOWN";
}

float randfloat(float a) {
	float res = (float)rand()/((float)(RAND_MAX/a));
	assert(res > 0.0);
	assert(res <= a);
	return  res;
}

bool randprob(float a) {
	int res = rand();
	return (float)res < RAND_MAX * a;
}

float randangle() {
	return 360.0 * randfloat(1.0);
}

Vector2 rotate(Vector2 v, int angle) {
	float newX = (v.x * cos(angle*DEG2RAD)) - (v.y * sin(angle*DEG2RAD));
	float newY = (v.x * sin(angle*DEG2RAD)) + (v.y * cos(angle*DEG2RAD));

	Vector2 res = {newX, newY};
	return res;
}

Vector2 translate(Vector2 v, Vector2 translation) {
	v.x = v.x - translation.x;
	v.y = v.y - translation.y;
	return v;
}

/*
Vector2 randomPosition() {
	Vector2 v = { 100, 100};
	return v;
}

*/

Vector2 fixedDirection() {
	float angle = 45;
	Vector2 v = {1.0, 0.0};

	return rotate(v, angle);
}

Vector2 randomPosition() {
	Vector2 v = {SCREEN_W * randfloat(1.0), SCREEN_H * randfloat(1.0)};
	return v;
}

Vector2 randomDirection() {
	float angle = randangle();
	Vector2 v = {1.0, 0.0};

	return rotate(v, angle);
}

bool isVectorEqual(Vector2 v1, Vector2 v2) {
	return v1.x == v2.x && v1.y == v2.y;
}

Vector2 objMid(Object *obj) {
	Vector2 v = {obj->x + obj->w/2, obj->y + obj->h/2};
	return v;
}

Vector2 objPos(Object *obj) {
	Vector2 v = {obj->x, obj->y};
	return v;
}


void rotateAroundCenter(Object *obj, Vector2 *pts, int n) {
	Vector2 mid = objMid(obj);
	Vector2 inv = {-mid.x, -mid.y};
	for (int i = 0; i < n; i++) {
		Vector2 res =
			translate(
				rotate(
					translate(pts[i], mid),
				obj->angle),
			inv);
		pts[i].x = res.x;
		pts[i].y = res.y;
	}
}

void points(Object *obj, Vector2 *pts, int *n) {
	int m = 0;
	int *ms = (n == NULL) ? &m : n;

	if (obj->type == ASTEROID) {
		*ms = 4;
		pts[0].x = obj->x;
		pts[0].y = obj->y;
		pts[1].x = obj->x + obj->w;
		pts[1].y = obj->y;
		pts[2].x = obj->x + obj->w;
		pts[2].y = obj->y + obj->h;
		pts[3].x = obj->x;
		pts[3].y = obj->y + obj->h;
	} else if (obj->type == SHIP) {
		*ms = 3;
		//const float pyth = sqrt(pow(obj->w/2, 2) + pow(obj->h/2, 2));
		pts[0].x = obj->x + obj->w/2;
		pts[0].y = obj->y;
		pts[1].x = obj->x;
		pts[1].y = obj->y + obj->h/2;
		pts[2].x = obj->x + obj->w;
		pts[2].y = obj->y + obj->h;
	} else if (obj->type == MISSILE) {
		*ms = 1;
		pts[0].x = obj->x;
		pts[0].y = obj->y;
	} else {
		ILOG("cannot points from unknown type %d", obj->type);
		return;
	}

	rotateAroundCenter(obj, pts, *ms);
}

bool isObjDestroyed(Object *obj) {
	return obj->destroyed > 0;
}

bool isNotInCooldown(GameState *state, Object *obj, uint64_t cooldown) {
	uint64_t now = state->framecounter;
	int diff = now - obj->framecounter;
	//NB first check handles overflow of uint in global frame counter
	return now < obj->framecounter || diff > cooldown;
}

void objDestroy(Object *obj) {
	if (!isObjDestroyed(obj)) {
		obj->destroyed = 1;
	}
}


void debugObj(Object *obj) {
	DLOG("[%s] (%f, %f)->(%f, %f)@(%f, %f)", typeToString(obj), obj->x, obj->y, obj->direction.x, obj->direction.y, obj->angle, obj->rotation);
}

void infoObj(Object *obj) {
	ILOG("[%s] (%f, %f)->(%f, %f)@(%f, %f)", typeToString(obj), obj->x, obj->y, obj->direction.x, obj->direction.y, obj->angle, obj->rotation);
}

bool checkCollide(Object *o1, Object *o2) {
	Vector2 verts[4];
	int n;

	if (!o1->active || !o2->active) {
		return false;
	}

	points(o1, verts, &n);

	for (int i = 0; i < n; i++) {
		Vector2 point = verts[i];
		if (o2->type == ASTEROID) {
			Rectangle rec = {o2->x, o2->y, o2->w, o2->h};
			if (CheckCollisionPointRec(point, rec)) {
				return true;
			}
		} else if (o2->type == SHIP) {
			Vector2 tv[3];
			points(o2, tv, NULL);
			if (CheckCollisionPointTriangle(point, tv[0], tv[1], tv[2])) {
				return true;
			}
		} else if (o2->type == MISSILE) {
			Vector2 missile;
			missile.x = o2->x;
			missile.y = o2->y;
			if (isVectorEqual(point, missile)) {
				return true;
			}
		} else {
			ILOG("cannot check collisions for unknown type %d", o2->type);
		}
	}
	return false;
}


Object *firstCollider(Object *objs, Object *o) {
	for (int j = 0; j < MAX_OBJS; j++) {
		Object *oj = &objs[j];
		if (o != oj) {
			if (checkCollide(o, oj)) {
				return oj;
			}
		}
	}
	return NULL;
}

void initObject(Object *obj, int type, float speed, Vector2 direction, Vector2 size, Vector2 pos, float angle, float rotation) {
	obj->w = size.x;
	obj->h = size.y;
	obj->angle = angle;
	obj->speed = speed;
	obj->destroyed = 0;
	obj->type = type;
	obj->active = true;
	obj->direction.x = direction.x;
	obj->direction.y = direction.y;
	obj->x = pos.x;
	obj->y = pos.y;
	obj->framecounter = 0;
	obj->rotation = rotation;
	ILOG("[%s] (%f, %f)->(%f, %f)$(%f, %f)", typeToString(obj), obj->x, obj->y, obj->direction.x, obj->direction.y, angle, rotation);
}


Object* activateObject(Object *objs, Object *obj, int type) {

	int speed = 0;
	float angle = 0;
	Vector2 size, pos;
	Vector2	direction = randomDirection();
	float rotation = 0;

	if (obj == NULL) {
		ILOG("cannot activate null object");
		return NULL;
	}

	if (type == ASTEROID) {
		size = ASTEROID_SIZE;
		speed = ASTEROID_MAX_SPEED * randfloat(1.0);
		//rotation = MAX_ASTEROID_ANGULAR_MOMENTUM * randfloat(1.0);
		//rotation = randprob(0.5) ? rotation : -rotation; //spin in both directions
	} else if (type == SHIP) {
		size = SHIP_SIZE;
		direction = fixedDirection();
		angle = randangle();
		direction = rotate(direction, angle);
	} else if (type == MISSILE) {
		size.x = MISSILE_RADIUS;
		size.y = MISSILE_RADIUS;
	} else {
		ILOG("Cannot activate unknown object type %d", type);
	}

	//pos inited to 0 here, will get randomly placed
	initObject(obj, type, speed, direction, size, pos, angle, rotation);

	while (true) {
		pos = randomPosition();
		obj->x = pos.x;
		obj->y = pos.y;
		if (firstCollider(objs, obj) == NULL) {
			break;
		} else {
			ILOG("failed collision test, retrying placement");
		}
	}
	return obj;
}

Object *getFreeObject(Object *objs) {
	Object *obj, *ref = NULL;

	for (int i = 0; i < MAX_OBJS; i++) {
		ref = &objs[i];
		if (!ref->active) {
			obj = ref;
			break;
		}
	}
	return obj;
}

Object* addObject(Object *objs, int type) {
	Object *obj = getFreeObject(objs);
	if (obj == NULL) {
		return NULL;
	}

	return activateObject(objs, obj, type);
}

Object *addMissile(Object *objs, Object *ship) {
	Object *obj = NULL;
	obj = getFreeObject(objs);

	if (obj == NULL) {
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
	Vector2 missileDirection = rotate(startDirection, angleOffset);
	//Vector2 missileDirection = randomDirection();

	Vector2 dir = {
		scaling * missileDirection.x,
		scaling * missileDirection.y,
	};

	Vector2 pos = {
		mid.x + dir.x,
		mid.y + dir.y,
	};

	initObject(obj, MISSILE, ship->speed + MISSILE_SPEED, missileDirection, MISSILE_SIZE, pos, 0, 0.0f);

	return obj;
}


int countPlayers(GameState *state) {
	int res = 0;
	for (int i = 0; i < MAX_PLAYERS; i++) {
		if (state->players[i].active) { res++; }
	}
	return res;
}

Player* addPlayer(GameState *state, ParsecGuest *guest) {
	for (int i = 0; i < MAX_PLAYERS; i++) {
		if (!state->players[i].active) {
			if (guest != NULL) {
				memcpy(&(state->players[i].guest), guest, sizeof(ParsecGuest));
			}
			Object *ship = addObject(state->objs, SHIP);
			state->players[i].ship = ship;
			state->players[i].active = true;
			if (ship == NULL) {
				return NULL;
			}

			return &(state->players[i]); }
	}
	return NULL;
}

Object* guestToShip(GameState *state, ParsecGuest *guest) {
	for (int i = 0; i < MAX_PLAYERS; i++) {
		Player *player = &state->players[i];
		if (state->players[i].active &&
				state->players[i].guest.id == guest->id) {
			return state->players[i].ship;
		}
	}
	return NULL;
}

Player* shipToPlayer(GameState *state, Object *obj) {
	if (obj == NULL) {
		return NULL;
	}
	for (int i = 0; i < MAX_PLAYERS; i++) {
		Player *player = &state->players[i];
		if (player->ship == obj) {
			return player;
		}
	}
	return NULL;
}

bool removePlayer(GameState *state, Object *obj) {
	Player *player = shipToPlayer(state, obj);
	if (player == NULL) {
		return false;
	}
	player->active = false;
	player->ship = NULL;
	if (obj != NULL) {
		obj->active = false;
	}
	return true;
}

void drawObj(Object *obj) {
	Color col = WHITE;
	if (isObjDestroyed(obj)) {
		col = RED;
	}
	if (obj->type == ASTEROID) {
		DrawRectangleLines(obj->x, obj->y, obj->w, obj->h, col);  // NOTE: Uses QUADS internally, not lines
		//DrawPoly(objPos(obj), 4, obj->w, obj->angle, col);
	} else if (obj->type == SHIP) {
		Vector2 verts[3];
		points(obj, verts, NULL);
		//DrawPoly(verts[0], 3, obj->w, obj->angle, col);
		DrawTriangleLines(
				verts[0],
				verts[1],
				verts[2],
			col);
	} else if (obj->type == MISSILE) {
		DrawCircle(obj->x, obj->y, obj->w, col);
	} else {
		ILOG("cannot draw unrecognized type %d", obj->type);
	}
	debugObj(obj);
}


void warpX(Object *obj, float newX) {
	obj->x = newX;
	//obj->direction.x = -obj->direction.x;
}

void warpY(Object *obj, float newY) {
	obj->y = newY;
	//obj->direction.y = -obj->direction.y;
}

void adjustAngle(Object *obj, int amount) {
	obj->angle += amount;
	if (obj->angle > 360) {
		obj->angle -= 360;
	}
	if (obj->angle < 0) {
		obj->angle += 360;
	}
}

void adjustDirection(Object *obj, int amount) {
	adjustAngle(obj, amount);
	obj->direction = rotate(obj->direction, amount);
}

void adjustSpeed(Object *obj, float amount) {
	obj->speed += amount;
}

void advance(Object *obj) {
	Vector2 av = {obj->direction.x * obj->speed, obj->direction.y * obj->speed};
	obj->x = av.x + obj->x;
	obj->y = av.y + obj->y;
	adjustAngle(obj, obj->rotation);
}

void handleOffscreen(Object *obj) {
	//We only warp one direction at a time,
	//because it's easier, and render loop will fix the other on
	//next iteration
	//
	Vector2 verts[4];
	int n;
	points(obj, verts, &n);
	int buf = 5;

	for (int i = 0; i < n; i++) {
		Vector2 point = verts[i];
		if (point.x < 0) {
			warpX(obj, SCREEN_W - obj->w - buf);
			return;
		} else if (point.x > SCREEN_W) {
			warpX(obj, buf);
			return;
		} else if (point.y < 0) {
			warpY(obj, SCREEN_H - obj->h - buf);
			return;
		} else if (point.y > SCREEN_H) {
			warpY(obj, buf);
			return;
		}
	}
}

void handleCollisions(Object *objs) {
	for (int i = 0; i < MAX_OBJS; i++) {
		Object *oi = &objs[i];
		Object *oj = firstCollider(objs, oi);
		if (oj != NULL) {
			objDestroy(oi);
			objDestroy(oj);
		}
	}
}

void takeShipAction(GameState *state, Object *obj, ShipAction action, bool local) {

	switch (action) {
		case TURN_LEFT:
			ILOG("turning left");
			adjustDirection(obj, -ANGULAR_MOVEMENT);
			break;
		case TURN_RIGHT:
			ILOG("turning right");
			adjustDirection(obj, ANGULAR_MOVEMENT);
			break;
		case SPEED_UP:
			ILOG("speeding up");
			adjustSpeed(obj, SPEED_MOVEMENT);
			break;
		case SPEED_DOWN:
			ILOG("speeding down");
			adjustSpeed(obj, -SPEED_MOVEMENT);
			break;
		case SHOOT:
			ILOG("shooting");
			if (isNotInCooldown(state, obj, MISSILE_COOLDOWN)) {
				addMissile(state->objs, obj);
				obj->framecounter = state->framecounter;
			}
			break;
		case NO_ACTION:
			break;
		default:
			ILOG("Unkown action %d", action);
			break;
	}
}


void handleKeyPress(GameState *state, Object *obj) {
	ShipAction action = NO_ACTION;

	if (obj->type != SHIP) {
		return;
	}

	if (IsKeyDown(KEY_RIGHT)) {
		action = TURN_RIGHT;
	}
	if (IsKeyDown(KEY_LEFT)) {
		action = TURN_LEFT;
	}
	if (IsKeyDown(KEY_UP)) {
		action = SPEED_UP;
	}
	if (IsKeyDown(KEY_DOWN)) {
		action = SPEED_DOWN;
	}
	if (IsKeyDown(KEY_SPACE)) {
		action = SHOOT;
	}

	if (action != NO_ACTION) {
		DLOG("local action");
		takeShipAction(state, obj, action, true);
	}
}

int countObjects(Object *objs, int type) {
	int res = 0;
	for (int i = 0; i < MAX_OBJS; i++) {
		if (objs[i].active && objs[i].type == type) {
			res += 1;
		}
	}
	return res;
}

void handleDestruction(Object *objs) {
	for (int i = 0; i < MAX_OBJS; i++) {
		if (objs[i].active &&
				isObjDestroyed(&objs[i])) {
			//NB risk for segfault here if indexing is wrong
			int destructThreshold = DESTRUCTION_THRESHOLDS[objs[i].type];
			if (objs[i].destroyed++ > destructThreshold) {
				objs[i].active = 0;
			}
		}
	}
}

void guest_state_change(GameState *state, ParsecGuest *guest) {
	ILOG("guest state change %d %d %d", guest->state, GUEST_CONNECTED, GUEST_DISCONNECTED);
	if (guest->state == GUEST_CONNECTED) {
		if (addPlayer(state, guest) != NULL) {
			ILOG("added player id: %d", guest->id);
		} else {
			ILOG("failed to add player");
		}
	} else if (guest->state == GUEST_DISCONNECTED) {
		if (removePlayer(state, guestToShip(state, guest))) {
			ILOG("removed player id: %d", guest->id);
		} else {
			ILOG("failed to remove player");
		}
	}
}


void submitHostFrameBuffer(Parsec *parsec) {
  if (!parsec)
    return;
  ParsecGuest* guests;
  ParsecHostGetGuests(parsec, GUEST_CONNECTED, &guests);
  if (&guests[0] != NULL) {
    Image image = GetScreenData();
    ImageFlipVertical(&image);
    Texture2D tex = LoadTextureFromImage(image);
    ParsecHostGLSubmitFrame(parsec, tex.id);
  }
  ParsecFree(guests);
}

void respawnShips(GameState *state) {
	for (int i = 0; i < MAX_OBJS; i++) {
		Object *obj = &state->objs[i];
		if (obj->active &&
				obj->type == SHIP &&
				isObjDestroyed(obj)) {
			activateObject(state->objs, obj, SHIP);
		}
	}
}

void handleInput(GameState *state, ParsecGuest *guest, ParsecMessage *msg) {
	ShipAction action = NO_ACTION;
	Object *ship = guestToShip(state, guest);
	ILOG("handling input for [%d] %d", guest->id, msg->type);
	if (ship == NULL) {
		ILOG("no ship for guest");
		return;
	}

	if (msg->type == MESSAGE_KEYBOARD) {
		ILOG("keyboard event");
		switch (msg->keyboard.code) {
			case PARSEC_KEY_W:
			case PARSEC_KEY_UP:
				action = SPEED_UP;
				break;
			case PARSEC_KEY_S:
			case PARSEC_KEY_DOWN:
				action = SPEED_DOWN;
				break;
			case PARSEC_KEY_A:
			case PARSEC_KEY_LEFT:
				action = TURN_LEFT;
				break;
			case PARSEC_KEY_D:
			case PARSEC_KEY_RIGHT:
				action = TURN_RIGHT;
				break;
			case PARSEC_KEY_SPACE:
				action = SHOOT;
				break;
			default:
				break;
		}
	} else if (msg->type == MESSAGE_GAMEPAD_BUTTON) {
		ILOG("gamepad button event");
		switch (msg->gamepadButton.button) {
			case GAMEPAD_BUTTON_A:
				action = SHOOT;
				break;
			case GAMEPAD_BUTTON_DPAD_UP:
				action = SPEED_UP;
				break;
			case GAMEPAD_BUTTON_DPAD_DOWN:
			case GAMEPAD_BUTTON_B:
				action = SPEED_DOWN;
				break;
			case GAMEPAD_BUTTON_DPAD_LEFT:
				action = TURN_LEFT;
				break;
			case GAMEPAD_BUTTON_DPAD_RIGHT:
				action = TURN_RIGHT;
				break;
			default:
				break;
		}
	} else if (msg->type == MESSAGE_GAMEPAD_AXIS) {
		ParsecGamepadAxisMessage *pm = &msg->gamepadAxis;
	}

	takeShipAction(state, ship, action, false);
}

void kickGuests(GameState *state, Parsec *parsec) {
	for (int i = 0; i < MAX_PLAYERS; i++) {
		Player *player = &state->players[i];
		if (player->active) {
			ILOG("kicking player id: %d", player->guest.id);
			ParsecHostKickGuest(parsec, player->guest.id);
		}
	}
}


int main(int argc, char *argv[])
{
    // Initialization
    //--------------------------------------------------------------------------------------
		const int fps = 30;
		float nAsteroidsMidpoint = (MAX_ASTEROIDS - N_START_ASTEROIDS) / 2;
		Parsec *parsec;
		char *session;

		Player *localPlayer;

		SetTraceLogLevel(LOG_WARNING);
		GameState state;
		Object *objs = &state.objs[0];

		if (argc < 2) {
			printf("Usage: ./ [session-id]\n");
			return 1;
		}

		session = argv[1];

		if (PARSEC_OK != ParsecInit(PARSEC_VER, NULL, NULL, &parsec)) {
			ILOG("Couldn't init parsec");
			return 1;
		}

		if (PARSEC_OK != ParsecHostStart(parsec, HOST_GAME, NULL, session)) {
			ILOG("Couldn't start hosting");
			return 1;
		}

		for (int i = 0; i < MAX_OBJS; i++) {
			objs[i].active = 0;
		}

		for (int i = 0; i < N_START_ASTEROIDS; i++) {
			addObject(objs, ASTEROID);
		}

		Object objPtr = *objs;

    InitWindow(SCREEN_W, SCREEN_H, "Asteroid BATTLE!");

    SetTargetFPS(fps);               // Set our game to run at 60 frames-per-second
    //--------------------------------------------------------------------------------------

    // Main game loop
    while (!WindowShouldClose())    // Detect window close button or ESC key
    {
			// Update
			//----------------------------------------------------------------------------------
			// TODO: Update your variables here
			//----------------------------------------------------------------------------------

			// Draw
			//----------------------------------------------------------------------------------
			BeginDrawing();

			float baseSpawnP = EXPECTED_ASTEROIDS_PER_SEC / fps;
			float asteroidSpawnP = baseSpawnP * (
						1 + ASTEROID_SPAWN_DRIVER *
							(nAsteroidsMidpoint - countObjects(state.objs, ASTEROID)) / nAsteroidsMidpoint);

			ClearBackground(BLACK);

				DLOG("--BEGIN--");

				if (IsKeyDown(KEY_O) && localPlayer == NULL) {
					ILOG("adding local player");
					localPlayer = addPlayer(&state, NULL);
				}
				if (IsKeyDown(KEY_U) && localPlayer != NULL) {
					ILOG("removing local player");
					removePlayer(&state, localPlayer->ship);
					localPlayer = NULL;
				}

				for (int i = 0; i < MAX_OBJS; i++) {

					if (objs[i].active) {
						DLOG("drawing");

						drawObj(&objs[i]);

						advance(&objs[i]); //NB must pass by reference

						handleOffscreen(&objs[i]);

					}
				}

				if (localPlayer != NULL) {
					handleKeyPress(&state, localPlayer->ship);
				}

				handleCollisions(objs);

				if (randprob(asteroidSpawnP)) {
					DLOG("spawn asteroid triggered");

					if (countObjects(objs, ASTEROID) < MAX_ASTEROIDS) {
						addObject(objs, ASTEROID);
					} else {
						DLOG("not spawning b/c max asteroids");
					}
				}

				handleDestruction(objs);
				respawnShips(&state);

			EndDrawing();
			//----------------------------------------------------------------------------------
			//
			submitHostFrameBuffer(parsec);

			for (ParsecHostEvent event; ParsecHostPollEvents(parsec, 0, &event);) {
				if (event.type == HOST_EVENT_GUEST_STATE_CHANGE)
					guest_state_change(&state, &event.guestStateChange.guest);
			}

			ParsecGuest guest;
			for (ParsecMessage msg; ParsecHostPollInput(parsec, 0, &guest, &msg);) {
				handleInput(&state, &guest, &msg);
			}

			//will overflow, but we don't care, loops back around.
			state.framecounter++;
    }

    // De-Initialization
    //--------------------------------------------------------------------------------------
    CloseWindow();        // Close window and OpenGL context
    //--------------------------------------------------------------------------------------
		kickGuests(&state, parsec);
		ParsecHostStop(parsec);

    return 0;
}

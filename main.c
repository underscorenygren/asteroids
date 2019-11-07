#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <time.h>
#include <string.h>

#include "raylib.h"
#include "parsec.h"

#define MAX_PLAYERS 8

#define SHIP_SPAWN_GUARD 1

const int ASTEROID = 1;
const int SHIP = 2;
const bool DEBUG = false;

const int SCREEN_W = 800;
const int SCREEN_H = 450;
const int ASTEROID_MAX_SPEED = 10;

const Vector2 ASTEROID_SIZE = {45, 45};
const Vector2 SHIP_SIZE = {10, 20};

const int MAX_OBJS = 100;

typedef struct Object {
	float x, y, w, h, speed, angle;
	Vector2 direction;
	int destroyed;
	int type;
	int spawnTime;
	bool active;
} Object;

typedef struct Player {
	ParsecGuest guest;
	Object *ship;
	bool active;
} Player;

typedef struct GameState {
	Player players[MAX_PLAYERS];
	Object objs[MAX_OBJS];
} GameState;

char *typeToString(Object *obj) {
	if (obj->type == ASTEROID) {
		return "ASTEROID";
	} else if(obj->type == SHIP) {
		return "SHIP";
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

Vector2 randomPosition() {
	Vector2 v = {SCREEN_W * randfloat(1.0), SCREEN_H * randfloat(1.0)};
	return v;
}

Vector2 rotate(Vector2 v, int angle) {
	float newX = (v.x * cos(angle*DEG2RAD)) - (v.y * sin(angle*DEG2RAD));
	float newY = (v.x * sin(angle*DEG2RAD)) + (v.y * cos(angle*DEG2RAD));

	Vector2 res = {newX, newY};
	return res;
}

Vector2 randomDirection() {
	float angle = 360.0 * randfloat(1.0);
	Vector2 v = {1.0, 0.0};

	return rotate(v, angle);
}

void points(Object *obj, Vector2 *pts, int *n) {
	if (obj->type == ASTEROID) {
		if (n != NULL) {
			*n = 4;
		}
		pts[0].x = obj->x;
		pts[0].y = obj->y;
		pts[1].x = obj->x + obj->w;
		pts[1].y = obj->y;
		pts[2].x = obj->x + obj->w;
		pts[2].y = obj->y + obj->h;
		pts[3].x = obj->x;
		pts[3].y = obj->y + obj->h;
	} else {
		if (n != NULL) {
			*n = 3;
		}
		const float pyth = sqrt(pow(obj->w/2, 2) + pow(obj->h/2, 2));
		pts[0].x = obj->x;
		pts[0].y = obj->y;
		pts[1].x = obj->x + obj->w;
		pts[1].y = obj->y;
		pts[2].x = obj->x + pyth;
		pts[2].y = obj->y + pyth;
	}
}


bool isObjDestroyed(Object *obj) {
	return obj->destroyed > 0;
}

void objDestroy(Object *obj) {
	if (!isObjDestroyed(obj)) {
		obj->destroyed = 1;
	}
}


void printObj(Object *obj) {
	if (DEBUG) {
		printf("%s pos: (%f, %f), vel: (%f, %f, %f)\n", typeToString(obj), obj->x, obj->y, obj->speed, obj->direction.x, obj->direction.y);
	}
}

void debug(char *msg) {
	if (DEBUG) {
		printf("%s\n", msg);
	}
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
		} else {
			Vector2 tv[3];
			points(o2, tv, NULL);
			if (CheckCollisionPointTriangle(point, tv[0], tv[1], tv[2])) {
				return true;
			}
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

Object* activateObject(Object *objs, Object *obj) {

	int speed = 0;
	int type = obj->type;
	Vector2 size, pos;
	Vector2	direction = randomDirection();

	if (type == ASTEROID) {
		size = ASTEROID_SIZE;
		speed = ASTEROID_MAX_SPEED * randfloat(1.0);
	} else {
		size = SHIP_SIZE;
	}

	if (obj == NULL) {
		debug("no free object slots");
		return NULL;
	} else {
		obj->w = size.x;
		obj->h = size.y;
		obj->angle = 0;
		obj->speed = speed;
		obj->destroyed = 0;
		obj->type = type;
		obj->active = true;
		obj->direction = direction;
		obj->spawnTime = time(NULL);
	}

	while (true) {
		pos = randomPosition();
		obj->x = pos.x;
		obj->y = pos.y;
		if (firstCollider(objs, obj) == NULL) {
			debug("adding object");
			printObj(obj);
			break;
		} else {
			debug("failed collision test, retrying placement");
		}
	}
	return obj;
}

Object* addObject(Object *objs, int type) {
	Object *obj, *ref = NULL;

	for (int i = 0; i < MAX_OBJS; i++) {
		ref = &objs[i];
		if (!ref->active) {
			obj = ref;
			break;
		}
	}

	obj->type = type;

	return activateObject(objs, obj);
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
			memcpy(&(state->players[i].guest), guest, sizeof(ParsecGuest));
			Object *ship = addObject(state->objs, SHIP);
			state->players[i].ship = ship;
			if (ship == NULL) {
				return NULL;
			}

			return &(state->players[i]); }
	}
	return NULL;
}

bool removePlayer(GameState *state, ParsecGuest *guest) {
	for (int i = 0; i < MAX_PLAYERS; i++) {
		if (state->players[i].active &&
				state->players[i].guest.id == guest->id) {
			state->players[i].active = false;
			if (state->players[i].ship != NULL) {
				state->players[i].ship->active = false;
				state->players[i].ship = NULL;
			}
			return true;
		}
	}
	return false;
}

void drawObj(Object *obj) {
	Color col = WHITE;
	if (isObjDestroyed(obj)) {
		col = RED;
	}
	if (obj->type == ASTEROID) {
		DrawRectangleLines(obj->x, obj->y, obj->w, obj->h, col);  // NOTE: Uses QUADS internally, not lines
	} else if (obj->type == SHIP) {
		Vector2 verts[3];
		points(obj, verts, NULL);
		DrawPoly(verts[0], 3, obj->w, obj->angle, col);
		/*DrawTriangleLines(
				verts[0],
				verts[1],
				verts[2],
			col);
			*/
	}
	printObj(obj);
}


void warpX(Object *obj, float newX) {
	obj->x = newX;
	//obj->direction.x = -obj->direction.x;
}

void warpY(Object *obj, float newY) {
	obj->y = newY;
	//obj->direction.y = -obj->direction.y;
}

void advance(Object *obj) {
	debug("advancing");
	Vector2 av = {obj->direction.x * obj->speed, obj->direction.y * obj->speed};
	obj->x = av.x + obj->x;
	obj->y = av.y + obj->y;
	printObj(obj);
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

void adjustAngle(Object *obj, int amount) {
	obj->angle += amount;
	if (obj->angle > 360) {
		obj->angle -= 360;
	}
	if (obj->angle < 0) {
		obj->angle += 360;
	}
	obj->direction = rotate(obj->direction, amount);
}

void handleKeyPress(Object *obj) {
	const int angMom = 3;
	float speedMom = 0.5;

	if (obj->type != SHIP) {
		return;
	}

	if (IsKeyDown(KEY_RIGHT)) {
		adjustAngle(obj, angMom);
	}
	if (IsKeyDown(KEY_LEFT)) {
		adjustAngle(obj, -angMom);
	}
	if (IsKeyDown(KEY_UP)) {
		obj->speed = obj->speed + speedMom;
	}
	if (IsKeyDown(KEY_DOWN)) {
		obj->speed = obj->speed - speedMom;
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

bool isOutsideSpawnGuard(Object *obj, int type) {
	if (type == SHIP) {
		return time(NULL) - obj->spawnTime > SHIP_SPAWN_GUARD;
	}
	return true;
}

void handleDestruction(Object *objs) {
	int destructThreshold = 5;
	for (int i = 0; i < MAX_OBJS; i++) {
		if (objs[i].active &&
				isObjDestroyed(&objs[i])) {
			if (objs[i].destroyed++ > destructThreshold) {
				objs[i].active = 0;
				objs[i].spawnTime = time(NULL);
			}
		}
	}
}

void guest_state_change(GameState *state, ParsecGuest *guest) {
	printf("guest state change %d %d %d\n", guest->state, GUEST_CONNECTED, GUEST_DISCONNECTED);
	if (guest->state == GUEST_CONNECTED) {
		if (addPlayer(state, guest) != NULL) {
			printf("added player\n");
		} else {
			printf("failed to add player\n");
		}
	} else if (guest->state == GUEST_DISCONNECTED) {
		if (removePlayer(state, guest)) {
			printf("removed player\n");
		} else {
			printf("failed to remove player\n");
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
				isObjDestroyed(obj) &&
				isOutsideSpawnGuard(obj, SHIP)) {
			activateObject(state->objs, obj);
		}
	}
}

int main(int argc, char *argv[])
{
    // Initialization
    //--------------------------------------------------------------------------------------
		const int fps = 30;
		int nAsteroids = 2, maxAsteroids = 10;
		float expAsteroidSpawnPerSec = 1;
		float asteroidSpawnP = expAsteroidSpawnPerSec / fps;
		Parsec *parsec;
		char *session;

		SetTraceLogLevel(LOG_WARNING);
		GameState state;
		Object *objs = &state.objs[0];

		if (argc < 2) {
			printf("Usage: ./ [session-id]\n");
			return 1;
		}

		session = argv[1];

		if (PARSEC_OK != ParsecInit(PARSEC_VER, NULL, NULL, &parsec)) {
			printf("Couldn't init parsec");
			return 1;
		}

		if (PARSEC_OK != ParsecHostStart(parsec, HOST_GAME, NULL, session)) {
			printf("Couldn't start hosting");
		}

		for (int i = 0; i < MAX_OBJS; i++) {
			objs[i].active = 0;
		}

		for (int i = 0; i < nAsteroids; i++) {
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

			ClearBackground(BLACK);

				debug("--BEGIN--");

				for (int i = 0; i < MAX_OBJS; i++) {

					if (objs[i].active) {
						debug("drawing");

						drawObj(&objs[i]);

						advance(&objs[i]); //NB must pass by reference

						handleOffscreen(&objs[i]);

						handleKeyPress(&objs[i]);
					}
				}

				handleCollisions(objs);

				if (randprob(asteroidSpawnP)) {
					debug("spawn asteroid triggered");

					if (countObjects(objs, ASTEROID) < maxAsteroids) {
						addObject(objs, ASTEROID);
					} else {
						debug("not spawning b/c max asteroids");
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
    }

    // De-Initialization
    //--------------------------------------------------------------------------------------
    CloseWindow();        // Close window and OpenGL context
    //--------------------------------------------------------------------------------------
		ParsecHostStop(parsec);

    return 0;
}

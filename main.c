#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <time.h>
#include <string.h>

#include "raylib.h"
#include "parsec.h"

#define MAX_PLAYERS 8

#define ANGULAR_MOVEMENT 5
#define SPEED_MOVEMENT 0.4f

#define DEBUG false
#define INFO true

#define DLOG(f_, ...) (DEBUG ? printf((f_), ##__VA_ARGS__), printf("\n") : 0)
#define ILOG(f_, ...) (INFO ? printf((f_), ##__VA_ARGS__), printf("\n") : 0)

const int FPS = 60;

const int ASTEROID = 1;
const int SHIP = 2;
const int MISSILE = 3;

const int SCREEN_W = 1600;
const int SCREEN_H = (2 * SCREEN_W / 3);
const int SCOREBOARD_Y_OFFSET = 30;
const int SCOREBOARD_FONT_SIZE = 24;
const int RESET_COOLDOWN = FPS;

const int ASTEROID_MAX_SPEED = 8.0;
const int N_START_ASTEROIDS = 5;
const int MAX_ASTEROIDS = 30;
const float EXPECTED_ASTEROIDS_PER_SEC = 3;
const float ASTEROID_SPAWN_DRIVER = 0.05;
const int MISSILE_COOLDOWN = 10; //NB: Measured in frames
const float MAX_ASTEROID_ANGULAR_MOMENTUM = 15.0;

const int MAX_OBJS = 200;
const int MISSILE_SPEED = 20.0;
const float MISSILE_RADIUS = 1.0;
const float MISSILE_ANGLE_OFFSET = 0.0;

const Vector2 ASTEROID_SIZE = {35.0, 35.0};
const Vector2 SHIP_SIZE = {20.0, 20.0};
const Vector2 MISSILE_SIZE = {MISSILE_RADIUS, MISSILE_RADIUS};

const char* WELCOME_TEXT = "Welcome to Asteroids Battle! Move: Keyboard(WASD,Arrows+Space) or DPAD+A/B/X. Reset game by holding Q or L and R Triggers";
const char* RESET_TEXT = "**wants[%d]reset**";
const int WELCOME_TEXT_COOLDOWN = 5 * FPS;

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

typedef struct Object {
	float x, y, w, h, speed, angle;
	Vector2 direction;
	int destroyed;
	int type;
	bool active;
	uint64_t framecounter;
	float rotation;
	Color col;
} Object;

const Object EMPTY_OBJECT = { 0 };

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

typedef struct GameState {
	Player players[MAX_PLAYERS];
	Object objs[MAX_OBJS];
	uint64_t framecounter;
	uint32_t welcomeTextCooldown;
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

int randint(int max) {
	int res = rand();
	int chunk = RAND_MAX / max;
	int i = 0;
	//iterate until random falls out of interval
	for (; i < max; i++) {
		if (res < chunk * (i+1)) {
			break;
		}
	}

	return i;
}

float randangle() {
	return 360.0 * randfloat(1.0);
}

//https://stackoverflow.com/questions/563198/how-do-you-detect-where-two-line-segments-intersect
bool get_line_intersection(float p0_x, float p0_y, float p1_x, float p1_y,
    float p2_x, float p2_y, float p3_x, float p3_y, float *i_x, float *i_y)
{
    float s1_x, s1_y, s2_x, s2_y;
    s1_x = p1_x - p0_x;     s1_y = p1_y - p0_y;
    s2_x = p3_x - p2_x;     s2_y = p3_y - p2_y;

    float s, t;
    s = (-s1_y * (p0_x - p2_x) + s1_x * (p0_y - p2_y)) / (-s2_x * s1_y + s1_x * s2_y);
    t = ( s2_x * (p0_y - p2_y) - s2_y * (p0_x - p2_x)) / (-s2_x * s1_y + s1_x * s2_y);

    if (s >= 0 && s <= 1 && t >= 0 && t <= 1)
    {
        // Collision detected
        if (i_x != NULL)
            *i_x = p0_x + (t * s1_x);
        if (i_y != NULL)
            *i_y = p0_y + (t * s1_y);
        return true;
    }

    return false; // No collision
}

bool checkLineCollision(Vector2 p0, Vector2 p1, Vector2 p2, Vector2 p3) {
	return get_line_intersection(p0.x, p0.y, p1.x, p1.y,
			p2.x, p2.y, p3.x, p3.y, NULL, NULL);
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

Vector2 nextCoord(Object *obj, bool inverted) {
	Vector2 av = {obj->direction.x * obj->speed, obj->direction.y * obj->speed};
	if (inverted) {
		av.x = -av.x;
		av.y = -av.y;
	}
	Vector2 res = {av.x + obj->x, av.y + obj->y};
	return res;
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
	Vector2 prev;
	Vector2 point;
	int n = 0;

	if (!o1->active || !o2->active) {
		return false;
	}

	if (o1 == o2) {
		return false;
	}

	points(o1, verts, &n);
	if (n == 0) {
		return false;
	}

	prev = verts[n-1];

	for (int i = 0; i < n; i++) {
		point = verts[i];
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
			Vector2 prevPos = nextCoord(o2, true);
			if (checkLineCollision(prev, point, missile, prevPos)) {
				return true;
			}
		} else {
			ILOG("cannot check collisions for unknown type %d", o2->type);
		}
		prev = point;
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

void initObject(Object *obj, int type, float speed, Vector2 direction, Vector2 size, Vector2 pos, float angle, float rotation, Color col) {
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
	obj->col = col;
	ILOG("[%s] (%f, %f)->(%f, %f)$(%f, %f)", typeToString(obj), obj->x, obj->y, obj->direction.x, obj->direction.y, angle, rotation);
}


Object* activateObject(Object *objs, Object *obj, int type, Color col) {

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
	initObject(obj, type, speed, direction, size, pos, angle, rotation, col);

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
			*obj = EMPTY_OBJECT;
			break;
		}
	}
	return obj;
}

Object* addObject(Object *objs, int type, Color col) {
	Object *obj = getFreeObject(objs);
	if (obj == NULL) {
		return NULL;
	}

	return activateObject(objs, obj, type, col);
}

bool isColorEqual(Color c1, Color c2) {
	return c1.r == c2.r &&
		c1.g == c2.g &&
		c1.b == c2.b;
}

Player* objToPlayer(GameState *state, Object *obj) {
	if (obj == NULL) {
		return NULL;
	}
	if (obj->type == SHIP) {
		for (int i = 0; i < MAX_PLAYERS; i++) {
			Player *player = &state->players[i];
			if (player->ship == obj) {
				return player;
			}
		}
	}
	if (obj->type == MISSILE) {
		for (int i = 0; i < MAX_PLAYERS; i++) {
			Player *player = &state->players[i];
			if (isColorEqual(player->col, obj->col)) {
				return player;
			}
		}
	}
	return NULL;
}

Object *addMissile(GameState *state, Object *ship) {
	Object *obj = NULL;
	Object *objs = state->objs;
	obj = getFreeObject(objs);
	Player *p = objToPlayer(state, ship);

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

	initObject(obj, MISSILE, ship->speed + MISSILE_SPEED, missileDirection, MISSILE_SIZE, pos, 0, 0.0f, p->col);

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
		Player *p = &state->players[i];
		if (!p->active) {
			if (guest != NULL) {
				p->guest = *guest;
			}
			p->col = COLORS[randint(N_COLORS)];
			Object *ship = addObject(state->objs, SHIP, p->col);
			p->ship = ship;
			p->active = true;
			if (ship == NULL) {
				return NULL;
			}

			return &(state->players[i]); }
	}
	return NULL;
}

Player* guestToPlayer(GameState *state, ParsecGuest *guest) {
	for (int i = 0; i < MAX_PLAYERS; i++) {
		Player *player = &state->players[i];
		if (state->players[i].active &&
				state->players[i].guest.id == guest->id) {
			return &state->players[i];
		}
	}
	return NULL;
}

void adjustScore(Player *p, int amount) {
	if (p != NULL) {
		p->score += amount;
	}
}

bool removePlayer(GameState *state, Player *p) {
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

void drawObj(Object *obj) {
	Color col = obj->col;
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
	if (obj->speed < 0) {
		obj->speed = 0;
	}
}

void advance(Object *obj) {
	Vector2 res = nextCoord(obj, false);
	obj->x = res.x;
	obj->y = res.y;
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

void destroy(GameState *state, Object *obj, Object *collider) {
	int amount = 0;
	Player *p = objToPlayer(state, obj);
	objDestroy(obj);

	if (p == NULL) {
		return;
	}

	if (obj->type == SHIP) {
		adjustScore(p, -1);
	}

	if (obj->type == MISSILE && collider->type == SHIP) {
		Player *other = objToPlayer(state, collider);
		if (other != p) {
			adjustScore(p, 1);
		}
	}
}

void handleCollisions(GameState *state) {
	Player *p;
	Object *objs = state->objs;
	for (int i = 0; i < MAX_OBJS; i++) {
		Object *oi = &objs[i];
		Object *oj = firstCollider(objs, oi);
		if (oj != NULL) {
			destroy(state, oi, oj);
			destroy(state, oj, oi);
		}
	}
}

void takeShipAction(GameState *state, Object *obj, ShipAction action) {
	if (obj == NULL) {
		ILOG("null ship");
		return;
	}
	DLOG("taking action %d", action);

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
				addMissile(state, obj);
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


void handlePlayer(GameState *state, Player *p) {
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
	takeShipAction(state, p->ship, action);
}

bool playerWantsReset(Player *p) {
	return p->p_q || (p->p_g_lt && p->p_g_rt);
}

void handleKeyPress(GameState *state, Player *player) {
	ShipAction action = NO_ACTION;

	if (player == NULL) {
		return;
	}

	DLOG("handling key presses");

	player->p_w = IsKeyDown(KEY_W);
	player->p_up = IsKeyDown(KEY_UP);
	player->p_s = IsKeyDown(KEY_S);
	player->p_down = IsKeyDown(KEY_DOWN);
	player->p_a = IsKeyDown(KEY_A);
	player->p_left = IsKeyDown(KEY_LEFT);
	player->p_d = IsKeyDown(KEY_D);
	player->p_right = IsKeyDown(KEY_RIGHT);
	player->p_space = IsKeyDown(KEY_SPACE);
	player->p_q = IsKeyDown(KEY_Q);
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

void handleDestruction(GameState *state) {
	Object *objs = state->objs;
	for (int i = 0; i < MAX_OBJS; i++) {
		Object *obj = &objs[i];
		if (obj->active &&
				isObjDestroyed(obj)) {
			//NB risk for segfault here if indexing is wrong
			int destructThreshold = DESTRUCTION_THRESHOLDS[obj->type];
			if (objs[i].destroyed++ > destructThreshold) {
				obj->active = 0;
			}
		}
	}
}

void stateTriggerWelcome(GameState *state) {
	state->welcomeTextCooldown = WELCOME_TEXT_COOLDOWN;
}

void parsecStateChange(GameState *state, ParsecGuest *guest) {
	ILOG("guest state change %d %d %d", guest->state, GUEST_CONNECTED, GUEST_DISCONNECTED);
	if (guest->state == GUEST_CONNECTED) {
		if (addPlayer(state, guest) != NULL) {
			ILOG("added player id: %d", guest->id);
			stateTriggerWelcome(state);
		} else {
			ILOG("failed to add player");
		}
	} else if (guest->state == GUEST_DISCONNECTED) {
		if (removePlayer(state, guestToPlayer(state, guest))) {
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
			activateObject(state->objs, obj, SHIP, obj->col);
		}
	}
}

void handleInput(GameState *state, ParsecGuest *guest, ParsecMessage *msg) {
	//ILOG("handling input for [%d] %d", guest->id, msg->type);
	Player *p = guestToPlayer(state, guest);
	bool pressed = false;

	if (msg->type == MESSAGE_KEYBOARD) {
		pressed = msg->keyboard.pressed;
		DLOG("[%d] keyboard event: %i", guest->id, pressed);
		switch (msg->keyboard.code) {
			case PARSEC_KEY_W:
				p->p_w = pressed;
				break;
			case PARSEC_KEY_UP:
				p->p_up = pressed;
				break;
			case PARSEC_KEY_S:
				p->p_s = pressed;
				break;
			case PARSEC_KEY_DOWN:
				p->p_up = pressed;
				break;
			case PARSEC_KEY_A:
				p->p_a = pressed;
				break;
			case PARSEC_KEY_LEFT:
				p->p_left = pressed;
				break;
			case PARSEC_KEY_D:
				p->p_d = pressed;
				break;
			case PARSEC_KEY_RIGHT:
				p->p_right = pressed;
				break;
			case PARSEC_KEY_SPACE:
				p->p_space = pressed;
				break;
			case PARSEC_KEY_Q:
				p->p_q = pressed;
				break;
			default:
				DLOG("unrecognized keyboard");
				break;
		}
	} else if (msg->type == MESSAGE_GAMEPAD_BUTTON) {
		pressed = msg->gamepadButton.pressed;
		DLOG("[%i] gamepad button event: %i", guest->id, pressed);
		switch (msg->gamepadButton.button) {
			case GAMEPAD_BUTTON_DPAD_UP:
				p->p_g_up = pressed;
				break;
			case GAMEPAD_BUTTON_DPAD_DOWN:
				p->p_g_down = pressed;
				break;
			case GAMEPAD_BUTTON_DPAD_LEFT:
				p->p_g_left = pressed;
				break;
			case GAMEPAD_BUTTON_DPAD_RIGHT:
				p->p_g_right = pressed;
				break;
			case GAMEPAD_BUTTON_A:
				p->p_g_a = pressed;
				break;
			case GAMEPAD_BUTTON_B:
				p->p_g_b = pressed;
				break;
			case GAMEPAD_BUTTON_X:
				p->p_g_x = pressed;
				break;
			case GAMEPAD_BUTTON_LSHOULDER:
				p->p_g_lt = pressed;
				break;
			case GAMEPAD_BUTTON_RSHOULDER:
				p->p_g_rt = pressed;
				break;
			default:
				DLOG("unrecognized gamepad");
				break;
		}
	} else {
		DLOG("unmapped parsec message %i", msg->type);
	//} else if (msg->type == MESSAGE_GAMEPAD_AXIS) {
	//	ParsecGamepadAxisMessage *pm = &msg->gamepadAxis;
	}
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

void drawScoreboard(GameState *state) {
	int nPlayers = countPlayers(state);
	if (nPlayers == 0) {
		return;
	}
	float chunk = SCREEN_W / nPlayers;
	char text[32];
	for (int i = 0; i < MAX_PLAYERS; i++) {
		Player *p = &state->players[i];
		if (p->active) {
			float x = i * chunk + chunk/2;
			const char *fmtString = playerWantsReset(p) ? RESET_TEXT : "%d";
			snprintf(&text[0], 32, fmtString, p->score);
			DrawText(text, x, SCOREBOARD_Y_OFFSET, SCOREBOARD_FONT_SIZE, p->col);
		}
	}
}

void checkClearGame(GameState *state) {

	bool allWantReset = countPlayers(state) > 0; //don't reset on 0 players

	for (int i = 0; i < MAX_PLAYERS; i++) {
		Player *p = &state->players[i];
		if (p->active) {
			bool thisReset = playerWantsReset(p);
			ILOG("player %i wants reset: %i", i, thisReset);
			allWantReset = allWantReset && thisReset;
		}
	}

	if (state->framecounter == 0 || (allWantReset && state->framecounter > RESET_COOLDOWN)) {
		ILOG("resetting game");
		for (int i = 0; i < MAX_OBJS; i++) {
			state->objs[i].active = 0;
		}

		for (int i = 0; i < N_START_ASTEROIDS; i++) {
			addObject(state->objs, ASTEROID, WHITE);
		}

		for (int i = 0; i < MAX_PLAYERS; i++) {
			Player *p = &state->players[i];
			if (p->active && p->ship) {
				p->ship->destroyed = true;
				p->ship->active = true;
			}
			p->score = 0;
		}

		state->framecounter = 1;
		stateTriggerWelcome(state);
	}
}

int main(int argc, char *argv[])
{
    // Initialization
    //--------------------------------------------------------------------------------------
		float nAsteroidsMidpoint = (MAX_ASTEROIDS - N_START_ASTEROIDS) / 2;
		Parsec *parsec;
		char *session;

		Player *localPlayer;

		SetTraceLogLevel(LOG_WARNING);
		GameState state = { 0 };

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

    InitWindow(SCREEN_W, SCREEN_H, "Asteroid BATTLE!");

    SetTargetFPS(FPS);               // Set our game to run at 60 frames-per-second
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
			checkClearGame(&state);

			float baseSpawnP = EXPECTED_ASTEROIDS_PER_SEC / FPS;
			float asteroidSpawnP = baseSpawnP * (
						1 + ASTEROID_SPAWN_DRIVER *
							(nAsteroidsMidpoint - countObjects(state.objs, ASTEROID)) / nAsteroidsMidpoint);

			ClearBackground(BLACK);

				DLOG("--BEGIN--");

				if (state.welcomeTextCooldown > 0) {
					DrawText(WELCOME_TEXT, 0, 0, SCOREBOARD_FONT_SIZE, WHITE);
					state.welcomeTextCooldown--;
				}

				if (IsKeyDown(KEY_O) && localPlayer == NULL) {
					ILOG("adding local player");
					localPlayer = addPlayer(&state, NULL);
				}
				if (IsKeyDown(KEY_U) && localPlayer != NULL) {
					ILOG("removing local player");
					removePlayer(&state, localPlayer);
					localPlayer = NULL;
				}

				for (int i = 0; i < MAX_OBJS; i++) {

					if (state.objs[i].active) {
						DLOG("drawing");

						drawObj(&state.objs[i]);

						advance(&state.objs[i]); //NB must pass by reference

						handleOffscreen(&state.objs[i]);

					}
				}

				handleCollisions(&state);

				if (randprob(asteroidSpawnP)) {
					DLOG("spawn asteroid triggered");

					if (countObjects(state.objs, ASTEROID) < MAX_ASTEROIDS) {
						addObject(state.objs, ASTEROID, WHITE);
					} else {
						DLOG("not spawning b/c max asteroids");
					}
				}

				handleDestruction(&state);
				respawnShips(&state);
				drawScoreboard(&state);

			EndDrawing();
			//----------------------------------------------------------------------------------
			//
			submitHostFrameBuffer(parsec);

			for (ParsecHostEvent event; ParsecHostPollEvents(parsec, 0, &event);) {
				if (event.type == HOST_EVENT_GUEST_STATE_CHANGE)
					parsecStateChange(&state, &event.guestStateChange.guest);
			}

			ParsecGuest guest;
			for (ParsecMessage msg; ParsecHostPollInput(parsec, 0, &guest, &msg);) {
				handleInput(&state, &guest, &msg);
			}
			handleKeyPress(&state, localPlayer);

			for (int i = 0; i < MAX_PLAYERS; i++) {
				handlePlayer(&state, &state.players[i]);
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

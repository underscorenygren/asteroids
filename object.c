#ifndef OBJECT_C
#define OBJECT_C

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include "types.h"
#include "raylib.h"
#include "parsec.h"

#include "random.c"
#include "vector.c"

/*
 * returns string repr of type.
 * used for debugging
 */
char* object_type_string(Object *obj) {
	if (obj->type == ASTEROID) {
		return "ASTEROID";
	} else if(obj->type == SHIP) {
		return "SHIP";
	} else if (obj->type == MISSILE) {
		return "MISSILE";
	}
	return "UNKNOWN";
}

/* initializes object by setting values in struct */
void object_init(Object *obj,
			uint32_t type,
			float speed,
			Vector2 direction,
			Vector2 size,
			Vector2 pos,
			float angle,
			Color col) {

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
	obj->col = col;
	ILOG("[%s] (%f, %f)->(%f, %f)@(%f)",
			object_type_string(obj),
			obj->x, obj->y,
			obj->direction.x, obj->direction.y,
			angle);
}

/* prints object using debug logger */
void object_debug(Object *obj) {
	DLOG("[%s] (%f, %f)->(%f, %f)@(%f)", object_type_string(obj), obj->x, obj->y, obj->direction.x, obj->direction.y, obj->angle);
}

/* prints object using info logger */
void object_info(Object *obj) {
	ILOG("[%s] (%f, %f)->(%f, %f)@(%f)", object_type_string(obj), obj->x, obj->y, obj->direction.x, obj->direction.y, obj->angle);
}

/*
 * Sets initial state on an object based on it's type.
 * Will place on random position, and will not guarantee
 * that it doesn't collide with existing objects.
 * Returns NULL if object is null
 */
Object* object_activate(Object *obj, uint32_t type, Color col) {

	uint32_t speed = 0;
	float angle = 0;
	Vector2 size, pos;
	Vector2	direction = vector_random_direction();

	if (obj == NULL) {
		ILOG("cannot activate null object");
		return NULL;
	}

	if (type == ASTEROID) {
		size = ASTEROID_SIZE;
		speed = ASTEROID_MAX_SPEED * random_float(1.0);
	} else if (type == SHIP) {
		size = SHIP_SIZE;
		direction = vector_fixed_direction();
		angle = random_angle();
		direction = vector_rotate(direction, angle);
	} else if (type == MISSILE) {
		size.x = MISSILE_RADIUS;
		size.y = MISSILE_RADIUS;
	} else {
		ILOG("Cannot activate unknown object type %d", type);
	}

	//pos inited to 0 here, will get randomly placed
	object_init(obj, type, speed, direction, size, pos, angle, col);
	return obj;
}

/* true iff object is destroyed */
bool object_is_destroyed(Object *obj) {
	return obj->destroyed > 0;
}

/* gets (x, y) of midpoint of object */
Vector2 object_midpoint(Object *obj) {
	Vector2 v = {obj->x + obj->w/2, obj->y + obj->h/2};
	return v;
}

/* gets object (x, y) */
Vector2 object_position(Object *obj) {
	Vector2 v = {obj->x, obj->y};
	return v;
}

/* returns the next position of the object based on it's
 * movement settings. inverted=true gets the previous
 * position. */
Vector2 object_get_movement(Object *obj, bool inverted) {
	Vector2 av = {obj->direction.x * obj->speed, obj->direction.y * obj->speed};
	if (inverted) {
		av.x = -av.x;
		av.y = -av.y;
	}
	Vector2 res = {av.x + obj->x, av.y + obj->y};
	return res;
}

/* Returns points for the object rotated around it's
 * center. pts is the destination array, and must already
 * be filled with object coordinates. n is the
 * size of the points array, e.g. how many sides
 * the object has.
 * NB: does not check that n is right for object type,
 * beware segfaults.
 */
void object_rotate(Object *obj, Vector2 *pts, uint32_t n) {
	Vector2 mid = object_midpoint(obj);
	Vector2 inv = {-mid.x, -mid.y};
	for (uint32_t i = 0; i < n; i++) {
		Vector2 res =
			vector_translate(
				vector_rotate(
					vector_translate(pts[i], mid),
				obj->angle),
			inv);
		pts[i].x = res.x;
		pts[i].y = res.y;
	}
}

/*
 * Returns coordinates of point definining edges of object.
 * Pouint32_t returned in pts.
 * Will set number of points in n if set, otherwise ignored.
 * Takes rotation uint32_to account.
 */
void object_get_points(Object *obj, Vector2 *pts, uint32_t *n) {
	uint32_t m = 0;
	uint32_t *ms = (n == NULL) ? &m : n;

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

	object_rotate(obj, pts, *ms);
}

/* sets object X coord */
void object_set_x(Object *obj, float newX) {
	obj->x = newX;
}

/* sets object Y coord */
void object_set_y(Object *obj, float newY) {
	obj->y = newY;
	//obj->direction.y = -obj->direction.y;
}

/*
 * increases objects current angle by amount.
 * Handles wraparound.
 */
void object_adjust_angle(Object *obj, uint32_t amount) {
	obj->angle += amount;
	if (obj->angle > 360) {
		obj->angle -= 360;
	}
	if (obj->angle < 0) {
		obj->angle += 360;
	}
}

/*
 * changes objects direction angle by amount.
 */
void object_adjust_direction(Object *obj, uint32_t amount) {
	object_adjust_angle(obj, amount);
	obj->direction = vector_rotate(obj->direction, amount);
}

/*
 * changes object speed by amount. Will not
 * decrease speed below 0.
 */
void object_adjust_speed(Object *obj, float amount) {
	obj->speed += amount;
	if (obj->speed < 0) {
		obj->speed = 0;
	}
}

/* sets destruction state on object */
void object_destroy(Object *obj) {
	if (!object_is_destroyed(obj)) {
		obj->destroyed = 1;
	}
}

/* true iff o1 collides with o2 */
bool object_is_colliding(Object *o1, Object *o2) {
	Vector2 verts[4];
	Vector2 prev;
	Vector2 point;
	uint32_t n = 0;

	if (!o1->active || !o2->active) {
		return false;
	}

	if (o1 == o2) {
		return false;
	}

	object_get_points(o1, verts, &n);
	if (n == 0) {
		return false;
	}

	prev = verts[n-1];

	for (uint32_t i = 0; i < n; i++) {
		point = verts[i];
		if (o2->type == ASTEROID) {
			Rectangle rec = {o2->x, o2->y, o2->w, o2->h};
			if (CheckCollisionPointRec(point, rec)) {
				return true;
			}
		} else if (o2->type == SHIP) {
			Vector2 tv[3];
			object_get_points(o2, tv, NULL);
			if (CheckCollisionPointTriangle(point, tv[0], tv[1], tv[2])) {
				return true;
			}
		} else if (o2->type == MISSILE) {
			Vector2 missile;
			missile.x = o2->x;
			missile.y = o2->y;
			if (vector_is_equal(point, missile)) {
				return true;
			}
			Vector2 prevPos = object_get_movement(o2, true);
			if (vector_is_line_colliding(prev, point, missile, prevPos)) {
				return true;
			}
		} else {
			ILOG("cannot check collisions for unknown type %d", o2->type);
		}
		prev = point;
	}
	return false;
}

/* moves an object to it's next coordinate */
void object_advance(Object *obj) {
	Vector2 verts[4];
	uint32_t n;
	Vector2 mvmt = object_get_movement(obj, false);

	obj->x = mvmt.x;
	obj->y = mvmt.y;

	//handles obj falling off screen
	object_get_points(obj, verts, &n);
	uint32_t buf = 5;

	for (uint32_t i = 0; i < n; i++) {
		Vector2 point = verts[i];
		if (point.x < 0) {
			object_set_x(obj, SCREEN_W - obj->w - buf);
			return;
		} else if (point.x > SCREEN_W) {
			object_set_x(obj, buf);
			return;
		} else if (point.y < 0) {
			object_set_y(obj, SCREEN_H - obj->h - buf);
			return;
		} else if (point.y > SCREEN_H) {
			object_set_y(obj, buf);
			return;
		}
	}
}

/* draws object based on type */
void object_draw(Object *obj) {
	Color col = obj->col;
	if (object_is_destroyed(obj)) {
		col = RED;
	}
	if (obj->type == ASTEROID) {
		DrawRectangleLines(obj->x, obj->y, obj->w, obj->h, col);  // NOTE: Uses QUADS uint32_ternally, not lines
		//DrawPoly(objPos(obj), 4, obj->w, obj->angle, col);
	} else if (obj->type == SHIP) {
		Vector2 verts[3];
		object_get_points(obj, verts, NULL);
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
	object_debug(obj);
}
#endif /* OBJECT_C */

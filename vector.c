#ifndef VECTOR_C
#define VECTOR_C

#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

#include "types.h"
#include "raylib.h"

#include "random.c"

/* true iff two lines intersect, from:
 * https://stackoverflow.com/questions/563198/how-do-you-detect-where-two-line-segments-intersect
 */
bool vector_get_line_intersection(float p0_x, float p0_y, float p1_x, float p1_y,
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

/** CHECKS **/

/* true iff lines defined by vectors (p0, p1) and (p2, p3) intersect. */
bool vector_is_line_colliding(Vector2 p0, Vector2 p1, Vector2 p2, Vector2 p3) {
	return vector_get_line_intersection(p0.x, p0.y, p1.x, p1.y,
			p2.x, p2.y, p3.x, p3.y, NULL, NULL);
}

/* true iff the two vectors have the same coordinates */
bool vector_is_equal(Vector2 v1, Vector2 v2) {
	return v1.x == v2.x && v1.y == v2.y;
}

/** VECTOR OPS **/
//All return new vector

/* rotates a vector angle degrees, returns new vector.
 * NB: does radian conversion */
Vector2 vector_rotate(Vector2 v, int angle) {
	float newX = (v.x * cos(angle*DEG2RAD)) - (v.y * sin(angle*DEG2RAD));
	float newY = (v.x * sin(angle*DEG2RAD)) + (v.y * cos(angle*DEG2RAD));

	Vector2 res = {newX, newY};
	return res;
}

/* translates a vector v by subtracting vector translation.
 * returns new vector. */
Vector2 vector_translate(Vector2 v, Vector2 translation) {
	v.x = v.x - translation.x;
	v.y = v.y - translation.y;
	return v;
}

/* scales vector by scaling in x and y */
Vector2 vector_scale(Vector2 v, float scaling) {
	v.x = scaling * v.x;
	v.y = scaling * v.y;
	return v;
}

/* Adds two vectors together. The opposite of translate */
Vector2 vector_add(Vector2 v, Vector2 add) {
	v.x = v.x + add.x;
	v.y = v.y + add.y;
	return v;
}


/** VECTOR RANDOMIZATION **/

/* returns a fixed direction. Used for ships that
 * we rotate randomly using direction after the fact. */
Vector2 vector_fixed_direction() {
	float angle = 45;
	Vector2 v = {1.0, 0.0};

	return vector_rotate(v, angle);
}

/* returns a random direction */
Vector2 vector_random_direction() {
	float angle = random_angle();
	Vector2 v = {1.0, 0.0};

	return vector_rotate(v, angle);
}

/* returns a random position */
Vector2 vector_random_position() {
	Vector2 v = {SCREEN_W * random_float(1.0), SCREEN_H * random_float(1.0)};
	return v;
}

#endif /* VECTOR_C */

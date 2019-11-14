#ifndef PLAYER_C
#define PLAYER_C

#include <stdbool.h>
#include <stdlib.h>

#include "types.h"

/* changes player score by amount */
void player_adjust_score(Player *p, uint32_t amount) {
	if (p != NULL) {
		p->score += amount;
	}
}

/* true iff player wants to reset game */
bool player_is_reset_requested(Player *p) {
	if (p == NULL) { return false; }
	return p->p_q || (p->p_g_lt && p->p_g_rt);
}

/* sets player fields to 0 if pointer is not null */
void player_clear(Player *p) {
	Player empty = { 0 };
	if (p != NULL) {
		*p = empty;
	}
}

/* true iff player is not null and has active bit set */
bool player_is_active(Player *p) {
	return p != NULL && p->active;
}

#endif /* PLAYER_C */

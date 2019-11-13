#ifndef PLAYER_C
#define PLAYER_C

#include <stdbool.h>
#include <stdlib.h>

#include "types.h"

void player_adjust_score(Player *p, uint32_t amount) {
	if (p != NULL) {
		p->score += amount;
	}
}

bool player_is_reset_requested(Player *p) {
	return p->p_q || (p->p_g_lt && p->p_g_rt);
}

#endif /* PLAYER_C */

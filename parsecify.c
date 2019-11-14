/* Parsec intergration functions */
#ifndef PARSECIFY_C
#define PARSECIFY_C

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

#include "parsec.h"
#include "raylib.h"

#include "types.h"

/* sends frame to parsec for distribution if parsec is initialized */
void parsecify_submit_frame(Parsec *parsec) {
  if (parsec == NULL) {
    return;
	}
	DLOG("submit_frame");

  ParsecGuest* guests = NULL; //@ronald: make this null to avoid corruption at free.
  uint32_t n_guests = ParsecHostGetGuests(parsec, GUEST_CONNECTED, &guests);
  if (n_guests > 0) {
		DLOG("one guest connected");
		assert(guests);
    Image image = GetScreenData();
    ImageFlipVertical(&image);
    Texture2D tex = LoadTextureFromImage(image);
    ParsecHostGLSubmitFrame(parsec, tex.id);
		UnloadImage(image);
		UnloadTexture(tex);
		ParsecFree(guests);
  } else {
		DLOG("No guests");
	}
}

/* adds/removes guests based on Parsec events.
 * Returns true iff player was added. */
bool parsecify_state_change(GameState *state, ParsecGuest *guest) {
	assert(state);
	assert(guest);
	bool playerAdded = false;
	ILOG("guest state change %d %d %d", guest->state, GUEST_CONNECTED, GUEST_DISCONNECTED);
	if (guest->state == GUEST_CONNECTED) {
		if (game_add_player(state, guest) != NULL) {
			ILOG("added player id: %d", guest->id);
			playerAdded = true;
		} else {
			ILOG("failed to add player");
		}
	} else if (guest->state == GUEST_DISCONNECTED) {
		if (game_remove_player(state, game_get_player_from_guest(state, guest))) {
			ILOG("removed player id: %d", guest->id);
		} else {
			ILOG("failed to remove player");
		}
	}
	return playerAdded;
}

/* parsec event check loop. */
bool parsecify_check_events(Parsec *parsec, GameState *state) {
	bool playerAdded = false;
	assert(parsec);
	assert(state);
	if (parsec != NULL) {
		for (ParsecHostEvent event; ParsecHostPollEvents(parsec, 0, &event);) {
			if (event.type == HOST_EVENT_GUEST_STATE_CHANGE)
				if (parsecify_state_change(state, &event.guestStateChange.guest)) {
					playerAdded = true;
				}
		}
	}

	return playerAdded;
}

/* translates parsec input into player state */
void parsecify_handle_input_message(GameState *state, ParsecGuest *guest, ParsecMessage *msg) {
	assert(state);
	assert(guest);
	assert(msg);
	Player *p = game_get_player_from_guest(state, guest);
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

/* Checks Parsec Inputs */
void parsecify_check_input(Parsec *parsec, GameState *state) {
	if (parsec == NULL) {
		return;
	}
	assert(state);
	ParsecGuest guest;
	for (ParsecMessage msg; ParsecHostPollInput(parsec, 0, &guest, &msg);) {
		parsecify_handle_input_message(state, &guest, &msg);
	}
}

/* kicks a player */
void parsecify_kick_guest(Parsec *parsec, Player *player) {
	assert(parsec);
	assert(player);
	if (player != NULL) {
		ILOG("kicking player id: %d", player->guest.id);
		ParsecHostKickGuest(parsec, player->guest.id);
	} else {
		ILOG("Cannot kick null player");
	}
}

/* Initializes parsec. returns true on failure, false othwerwise. */
bool parsecify_init(Parsec **parsec, char *session) {
	if (PARSEC_OK != ParsecInit(PARSEC_VER, NULL, NULL, parsec)) {
		ILOG("Couldn't init parsec");
		return true;
	}
	assert(*parsec);

	if (PARSEC_OK != ParsecHostStart(*parsec, HOST_GAME, NULL, session)) {
		ILOG("Couldn't start hosting");
		return true;
	}

	return false;
}

/* Kicks connected gets and stops Parsec on game end */
void parsecify_deinit(Parsec *parsec, GameState *state) {
	if (parsec == NULL) {
		return;
	}
	assert(state);

	for (uint32_t i = 0; i < MAX_PLAYERS; i++) {
		Player *player = &state->players[i];
		if (player->active) {
			parsecify_kick_guest(state->parsec, player);
		}
	}

	ParsecHostStop(parsec);
}

#endif /* PARSECIFY_C */

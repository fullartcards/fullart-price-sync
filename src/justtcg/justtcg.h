#ifndef FULLART_PRICE_SYNC_JUSTTCG_H
#define FULLART_PRICE_SYNC_JUSTTCG_H

#include "config.h"
#include "observation.h"

typedef struct {
	/* Raw /v1/cards response body allocated by justtcg_get_card. */
	char *body;
	/* HTTP status code returned by JustTCG. */
	long status;
} justtcg_card_response;

/* Frees memory owned by a justtcg_card_response and resets its fields. */
void justtcg_card_response_free(justtcg_card_response *response);

/* Fetches one card using the JustTCG cardId query parameter. Returns 1 on success. */
int justtcg_get_card(const app_config *cfg, const char *card_id, justtcg_card_response *response,
	char *err, unsigned long err_len);

/*
 * Extracts the first returned variant price into the minimal Full Art price observation.
 * Returns 0 when the card response has no priced variants or cannot be parsed.
 */
int justtcg_first_price_observation(const char *product_id, const char *card_body,
	price_observation *observation, char *err, unsigned long err_len);

#endif

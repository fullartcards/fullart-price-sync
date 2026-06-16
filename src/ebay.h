#ifndef FULLART_PRICE_SYNC_EBAY_H
#define FULLART_PRICE_SYNC_EBAY_H

#include "config.h"

typedef struct {
	/* OAuth access token allocated by ebay_mint_token. */
	char *access_token;
	/* Token lifetime in seconds, as returned by eBay. */
	long expires_in;
} ebay_token;

typedef struct {
	/* Raw Browse API response body allocated by ebay_search. */
	char *body;
	/* HTTP status code returned by the Browse API. */
	long status;
} ebay_search_response;

typedef struct {
	/* Stable daily id for a source/product/price observation. */
	char event_id[128];
	/* Full Art product id or SKU supplied by the caller. */
	const char *product_id;
	/* Normalized price in cents. */
	long price_cents;
} price_observation;

/* Frees memory owned by an ebay_token and resets its fields. */
void ebay_token_free(ebay_token *token);

/* Frees memory owned by an ebay_search_response and resets its fields. */
void ebay_search_response_free(ebay_search_response *response);

/* Mints an eBay OAuth client-credentials access token. Returns 1 on success. */
int ebay_mint_token(const app_config *cfg, ebay_token *token, char *err, unsigned long err_len);

/* Runs an eBay Browse API item summary search for query and stores the raw JSON response. */
int ebay_search(const app_config *cfg, const ebay_token *token, const char *query, int limit,
	ebay_search_response *response, char *err, unsigned long err_len);

/*
 * Extracts the first returned eBay item price into the minimal Full Art price observation.
 * Returns 0 when the response has no priced items or cannot be parsed.
 */
int ebay_first_price_observation(const char *product_id, const char *search_body,
	price_observation *observation, char *err, unsigned long err_len);

#endif

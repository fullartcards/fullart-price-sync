#ifndef FULLART_PRICE_SYNC_EBAY_H
#define FULLART_PRICE_SYNC_EBAY_H

#include "config.h"
#include "observation.h"

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
	price_observation *items;
	unsigned long len;
} ebay_price_observation_list;

/* Frees memory owned by an ebay_token and resets its fields. */
void ebay_token_free(ebay_token *token);

/* Frees memory owned by an ebay_search_response and resets its fields. */
void ebay_search_response_free(ebay_search_response *response);

/* Frees memory owned by an eBay price observation list. */
void ebay_price_observation_list_free(ebay_price_observation_list *list);

/* Mints an eBay OAuth client-credentials access token. Returns 1 on success. */
int ebay_mint_token(const app_config *cfg, ebay_token *token, char *err, unsigned long err_len);

/* Runs an eBay Browse API item summary search for query and stores the raw JSON response. */
int ebay_search(const app_config *cfg, const ebay_token *token, const char *query, int limit,
	ebay_search_response *response, char *err, unsigned long err_len);

/*
 * Extracts priced eBay item summaries into minimal price observations.
 * Each emitted product_id is the eBay itemId from that summary.
 */
int ebay_price_observations(const char *search_body, ebay_price_observation_list *list,
	char *err, unsigned long err_len);

#endif

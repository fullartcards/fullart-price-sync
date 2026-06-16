#ifndef FULLART_PRICE_SYNC_DB_H
#define FULLART_PRICE_SYNC_DB_H

#include "config.h"

typedef struct {
	/* Full Art product id or SKU used in emitted observations. */
	char *product_id;
	/* JustTCG cardId used with /v1/cards?cardId=. */
	char *justtcg_card_id;
} justtcg_card_mapping;

typedef struct {
	justtcg_card_mapping *items;
	unsigned long len;
} justtcg_card_mapping_list;

/* Frees all memory owned by a mapping list. */
void justtcg_card_mapping_list_free(justtcg_card_mapping_list *list);

/*
 * Loads JustTCG card mappings from Postgres.
 * The query must return two text-compatible columns:
 *   1. product_id
 *   2. justtcg_card_id
 */
int db_load_justtcg_card_mappings(const app_config *cfg, justtcg_card_mapping_list *list,
	char *err, unsigned long err_len);

#endif

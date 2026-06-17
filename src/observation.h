#ifndef FULLART_PRICE_SYNC_OBSERVATION_H
#define FULLART_PRICE_SYNC_OBSERVATION_H

typedef struct {
	/* Stable daily id for a source/product/price observation. */
	char event_id[128];
	/* Product identity for this source; eBay uses itemId, app-backed sources use SKU/id. */
	char product_id[256];
	/* Normalized price in cents. */
	long price_cents;
} price_observation;

#endif

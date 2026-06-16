#ifndef FULLART_PRICE_SYNC_OBSERVATION_H
#define FULLART_PRICE_SYNC_OBSERVATION_H

typedef struct {
	/* Stable daily id for a source/product/price observation. */
	char event_id[128];
	/* Full Art product id or SKU supplied by the caller. */
	const char *product_id;
	/* Normalized price in cents. */
	long price_cents;
} price_observation;

#endif

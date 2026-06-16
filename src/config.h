#ifndef FULLART_PRICE_SYNC_CONFIG_H
#define FULLART_PRICE_SYNC_CONFIG_H

typedef struct {
	/* "sandbox" or "production"; sandbox is the default when EBAY_ENV is unset. */
	const char *env;
	/* eBay application client id from EBAY_CLIENT_ID. */
	const char *client_id;
	/* eBay application client secret from EBAY_CLIENT_SECRET. */
	const char *client_secret;
	/* Marketplace used by Browse API requests, such as EBAY_US. */
	const char *marketplace_id;
	/* OAuth scope used for the client credentials token request. */
	const char *scope;
} app_config;

/* Loads app_config from environment variables. Returns 1 on success, 0 with err set on failure. */
int load_config(app_config *cfg, char *err, unsigned long err_len);

/* Returns the eBay REST API base URL for the configured environment. */
const char *ebay_api_base_url(const app_config *cfg);

/* Returns the eBay OAuth token URL for the configured environment. */
const char *ebay_token_url(const app_config *cfg);

#endif

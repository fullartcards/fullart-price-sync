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
	/* JustTCG API key from JUSTTCG_API_KEY. */
	const char *justtcg_api_key;
	/* PostgreSQL connection string from PRICE_SYNC_DATABASE_URL. */
	const char *database_url;
	/* SQL that returns product_id and justtcg_card_id columns for JustTCG sync. */
	const char *justtcg_card_query;
} app_config;

/* Loads app_config from environment variables without validating source-specific secrets. */
int load_config(app_config *cfg, char *err, unsigned long err_len);

/* Validates the eBay credentials needed for eBay commands. */
int require_ebay_config(const app_config *cfg, char *err, unsigned long err_len);

/* Validates the JustTCG credentials needed for JustTCG commands. */
int require_justtcg_config(const app_config *cfg, char *err, unsigned long err_len);

/* Validates the database settings needed for Postgres-backed sync commands. */
int require_database_config(const app_config *cfg, char *err, unsigned long err_len);

/* Returns the eBay REST API base URL for the configured environment. */
const char *ebay_api_base_url(const app_config *cfg);

/* Returns the eBay OAuth token URL for the configured environment. */
const char *ebay_token_url(const app_config *cfg);

/* Returns the JustTCG v1 REST API base URL. */
const char *justtcg_api_base_url(const app_config *cfg);

#endif

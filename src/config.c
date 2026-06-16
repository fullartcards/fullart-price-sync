#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *env_or_default(const char *name, const char *fallback)
{
	const char *value = getenv(name);
	return (value == NULL || value[0] == '\0') ? fallback : value;
}

int load_config(app_config *cfg, char *err, unsigned long err_len)
{
	(void)err;
	(void)err_len;

	if (cfg == NULL) {
		return 0;
	}
	cfg->env = env_or_default("EBAY_ENV", "sandbox");
	cfg->client_id = env_or_default("EBAY_CLIENT_ID", "");
	cfg->client_secret = env_or_default("EBAY_CLIENT_SECRET", "");
	cfg->marketplace_id = env_or_default("EBAY_MARKETPLACE_ID", "EBAY_US");
	cfg->scope = env_or_default("EBAY_SCOPE", "https://api.ebay.com/oauth/api_scope");
	cfg->justtcg_api_key = env_or_default("JUSTTCG_API_KEY", "");

	return 1;
}

int require_ebay_config(const app_config *cfg, char *err, unsigned long err_len)
{
	if (cfg == NULL) {
		snprintf(err, err_len, "config is required");
		return 0;
	}
	if (cfg->client_id[0] == '\0') {
		snprintf(err, err_len, "EBAY_CLIENT_ID is required");
		return 0;
	}
	if (cfg->client_secret[0] == '\0') {
		snprintf(err, err_len, "EBAY_CLIENT_SECRET is required");
		return 0;
	}

	return 1;
}

int require_justtcg_config(const app_config *cfg, char *err, unsigned long err_len)
{
	if (cfg == NULL) {
		snprintf(err, err_len, "config is required");
		return 0;
	}
	if (cfg->justtcg_api_key[0] == '\0') {
		snprintf(err, err_len, "JUSTTCG_API_KEY is required");
		return 0;
	}
	return 1;
}

const char *ebay_api_base_url(const app_config *cfg)
{
	if (cfg != NULL && strcmp(cfg->env, "production") == 0) {
		return "https://api.ebay.com";
	}
	return "https://api.sandbox.ebay.com";
}

const char *ebay_token_url(const app_config *cfg)
{
	if (cfg != NULL && strcmp(cfg->env, "production") == 0) {
		return "https://api.ebay.com/identity/v1/oauth2/token";
	}
	return "https://api.sandbox.ebay.com/identity/v1/oauth2/token";
}

const char *justtcg_api_base_url(const app_config *cfg)
{
	(void)cfg;
	return "https://api.justtcg.com/v1";
}

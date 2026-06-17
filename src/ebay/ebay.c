#include "ebay.h"

#include "../http.h"

#include <curl/curl.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static char *json_string_value(const char *json, const char *key)
{
	char needle[128];
	const char *p;
	const char *start;
	const char *end;
	char *out;
	unsigned long len;

	snprintf(needle, sizeof(needle), "\"%s\"", key);
	p = strstr(json, needle);
	if (p == NULL) {
		return NULL;
	}
	p = strchr(p + strlen(needle), ':');
	if (p == NULL) {
		return NULL;
	}
	p++;
	while (*p != '\0' && isspace((unsigned char)*p)) {
		p++;
	}
	if (*p != '"') {
		return NULL;
	}
	start = p + 1;
	end = start;
	while (*end != '\0' && *end != '"') {
		end++;
	}
	if (*end != '"') {
		return NULL;
	}

	len = (unsigned long)(end - start);
	out = malloc(len + 1);
	if (out == NULL) {
		return NULL;
	}
	memcpy(out, start, len);
	out[len] = '\0';
	return out;
}

static long json_long_value(const char *json, const char *key)
{
	char needle[128];
	const char *p;

	snprintf(needle, sizeof(needle), "\"%s\"", key);
	p = strstr(json, needle);
	if (p == NULL) {
		return 0;
	}
	p = strchr(p + strlen(needle), ':');
	if (p == NULL) {
		return 0;
	}
	return strtol(p + 1, NULL, 10);
}

static long money_to_cents(const char *value)
{
	long dollars = 0;
	long cents = 0;
	const char *p = value;

	while (*p != '\0' && isdigit((unsigned char)*p)) {
		dollars = (dollars * 10) + (*p - '0');
		p++;
	}
	if (*p == '.') {
		p++;
		if (isdigit((unsigned char)*p)) {
			cents += (*p - '0') * 10;
			p++;
		}
		if (isdigit((unsigned char)*p)) {
			cents += (*p - '0');
		}
	}

	return (dollars * 100) + cents;
}

static unsigned long long fnv1a(const char *s)
{
	unsigned long long hash = 1469598103934665603ULL;
	while (*s != '\0') {
		hash ^= (unsigned char)*s;
		hash *= 1099511628211ULL;
		s++;
	}
	return hash;
}

void ebay_token_free(ebay_token *token)
{
	if (token == NULL) {
		return;
	}
	free(token->access_token);
	token->access_token = NULL;
	token->expires_in = 0;
}

void ebay_search_response_free(ebay_search_response *response)
{
	if (response == NULL) {
		return;
	}
	free(response->body);
	response->body = NULL;
	response->status = 0;
}

int ebay_mint_token(const app_config *cfg, ebay_token *token, char *err, unsigned long err_len)
{
	CURL *curl = curl_easy_init();
	char *escaped_scope = NULL;
	char *form = NULL;
	http_result result = {0};
	int ok = 0;

	if (curl == NULL) {
		snprintf(err, err_len, "unable to initialize curl");
		return 0;
	}

	escaped_scope = curl_easy_escape(curl, cfg->scope, 0);
	if (escaped_scope == NULL) {
		snprintf(err, err_len, "unable to encode eBay OAuth scope");
		goto done;
	}

	form = malloc(strlen("grant_type=client_credentials&scope=") + strlen(escaped_scope) + 1);
	if (form == NULL) {
		snprintf(err, err_len, "unable to allocate OAuth form body");
		goto done;
	}
	sprintf(form, "grant_type=client_credentials&scope=%s", escaped_scope);

	if (!http_post_form_basic(ebay_token_url(cfg), cfg->client_id, cfg->client_secret, form,
		    &result, err, err_len)) {
		goto done;
	}
	if (result.status < 200 || result.status > 299) {
		snprintf(err, err_len, "eBay token request returned HTTP %ld: %.200s", result.status,
			result.body);
		goto done;
	}

	token->access_token = json_string_value(result.body, "access_token");
	token->expires_in = json_long_value(result.body, "expires_in");
	if (token->access_token == NULL) {
		snprintf(err, err_len, "eBay token response did not include access_token");
		goto done;
	}

	ok = 1;

done:
	http_result_free(&result);
	curl_free(escaped_scope);
	curl_easy_cleanup(curl);
	free(form);
	return ok;
}

int ebay_search(const app_config *cfg, const ebay_token *token, const char *query, int limit,
	ebay_search_response *response, char *err, unsigned long err_len)
{
	CURL *curl = curl_easy_init();
	char *escaped_query = NULL;
	char url[2048];
	http_result result = {0};
	int ok = 0;

	if (curl == NULL) {
		snprintf(err, err_len, "unable to initialize curl");
		return 0;
	}
	if (limit < 1 || limit > 200) {
		snprintf(err, err_len, "limit must be between 1 and 200");
		goto done;
	}

	escaped_query = curl_easy_escape(curl, query, 0);
	if (escaped_query == NULL) {
		snprintf(err, err_len, "unable to encode search query");
		goto done;
	}

	snprintf(url, sizeof(url), "%s/buy/browse/v1/item_summary/search?q=%s&limit=%d&sort=price",
		ebay_api_base_url(cfg), escaped_query, limit);

	if (!http_get_bearer(url, token->access_token, cfg->marketplace_id, &result, err, err_len)) {
		goto done;
	}
	if (result.status < 200 || result.status > 299) {
		snprintf(err, err_len, "eBay search returned HTTP %ld: %.200s", result.status,
			result.body);
		goto done;
	}

	response->status = result.status;
	response->body = result.body;
	result.body = NULL;
	ok = 1;

done:
	http_result_free(&result);
	curl_free(escaped_query);
	curl_easy_cleanup(curl);
	return ok;
}

int ebay_first_price_observation(const char *product_id, const char *search_body,
	price_observation *observation, char *err, unsigned long err_len)
{
	char *price_value = json_string_value(search_body, "value");
	char hash_input[512];
	time_t now;
	struct tm observed_day;
	char observed_date[9];
	long price_cents;

	if (price_value == NULL) {
		if (json_long_value(search_body, "total") == 0 || strstr(search_body, "\"itemSummaries\"") == NULL) {
			snprintf(err, err_len,
				"eBay search returned no priced items; use production data or broaden the query");
		} else {
			snprintf(err, err_len,
				"eBay response included item summaries but no price.value field; rerun with --raw to inspect it");
		}
		return 0;
	}

	price_cents = money_to_cents(price_value);
	if (price_cents <= 0) {
		snprintf(err, err_len, "eBay response price.value was not a positive amount");
		free(price_value);
		return 0;
	}

	now = time(NULL);
	if (gmtime_r(&now, &observed_day) == NULL ||
		strftime(observed_date, sizeof(observed_date), "%Y%m%d", &observed_day) == 0) {
		snprintf(err, err_len, "unable to build observation date");
		free(price_value);
		return 0;
	}
	snprintf(hash_input, sizeof(hash_input), "ebay|%s|%ld|%s", product_id, price_cents,
		observed_date);
	snprintf(observation->event_id, sizeof(observation->event_id), "ebay-%016llx",
		fnv1a(hash_input));
	observation->product_id = product_id;
	observation->price_cents = price_cents;

	free(price_value);
	return 1;
}

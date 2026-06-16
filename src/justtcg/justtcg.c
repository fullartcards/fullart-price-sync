#include "justtcg.h"

#include "http.h"

#include <curl/curl.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static const char *json_key_after(const char *json, const char *key)
{
	char needle[128];
	const char *p;

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
	return p;
}

static int json_has_nonempty_data(const char *json)
{
	const char *p = json_key_after(json, "data");
	if (p == NULL || *p != '[') {
		return 0;
	}
	p++;
	while (*p != '\0' && isspace((unsigned char)*p)) {
		p++;
	}
	return *p != ']';
}

static int json_number_cents(const char *json, const char *key, long *cents)
{
	const char *p = json_key_after(json, key);
	long whole = 0;
	long fraction = 0;
	int fraction_digits = 0;

	if (p == NULL) {
		return 0;
	}
	if (*p == '"') {
		p++;
	}
	while (*p != '\0' && isspace((unsigned char)*p)) {
		p++;
	}
	if (!isdigit((unsigned char)*p)) {
		return 0;
	}
	while (*p != '\0' && isdigit((unsigned char)*p)) {
		whole = (whole * 10) + (*p - '0');
		p++;
	}
	if (*p == '.') {
		p++;
		while (*p != '\0' && isdigit((unsigned char)*p) && fraction_digits < 3) {
			if (fraction_digits < 2) {
				fraction = (fraction * 10) + (*p - '0');
			}
			fraction_digits++;
			p++;
		}
	}
	if (fraction_digits == 0) {
		fraction *= 100;
	} else if (fraction_digits == 1) {
		fraction *= 10;
	}

	*cents = (whole * 100) + fraction;
	return 1;
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

void justtcg_card_response_free(justtcg_card_response *response)
{
	if (response == NULL) {
		return;
	}
	free(response->body);
	response->body = NULL;
	response->status = 0;
}

int justtcg_get_card(const app_config *cfg, const char *card_id, justtcg_card_response *response,
	char *err, unsigned long err_len)
{
	CURL *curl = curl_easy_init();
	char *escaped_card_id = NULL;
	char url[2048];
	http_result result = {0};
	int ok = 0;

	if (curl == NULL) {
		snprintf(err, err_len, "unable to initialize curl");
		return 0;
	}

	escaped_card_id = curl_easy_escape(curl, card_id, 0);
	if (escaped_card_id == NULL) {
		snprintf(err, err_len, "unable to encode JustTCG cardId");
		goto done;
	}

	snprintf(url, sizeof(url), "%s/cards?cardId=%s", justtcg_api_base_url(cfg), escaped_card_id);
	if (!http_get_api_key(url, "x-api-key", cfg->justtcg_api_key, &result, err, err_len)) {
		goto done;
	}
	if (result.status < 200 || result.status > 299) {
		snprintf(err, err_len, "JustTCG card request returned HTTP %ld: %.200s",
			result.status, result.body);
		goto done;
	}

	response->status = result.status;
	response->body = result.body;
	result.body = NULL;
	ok = 1;

done:
	http_result_free(&result);
	curl_free(escaped_card_id);
	curl_easy_cleanup(curl);
	return ok;
}

int justtcg_first_price_observation(const char *product_id, const char *card_body,
	price_observation *observation, char *err, unsigned long err_len)
{
	char hash_input[512];
	time_t now;
	struct tm observed_day;
	char observed_date[9];
	long price_cents = 0;

	if (!json_has_nonempty_data(card_body)) {
		snprintf(err, err_len, "JustTCG response did not include card data for that cardId");
		return 0;
	}
	if (!json_number_cents(card_body, "price", &price_cents) || price_cents <= 0) {
		snprintf(err, err_len,
			"JustTCG response did not include a positive variants[].price field; rerun with --raw to inspect it");
		return 0;
	}

	now = time(NULL);
	if (gmtime_r(&now, &observed_day) == NULL ||
		strftime(observed_date, sizeof(observed_date), "%Y%m%d", &observed_day) == 0) {
		snprintf(err, err_len, "unable to build observation date");
		return 0;
	}
	snprintf(hash_input, sizeof(hash_input), "justtcg|%s|%ld|%s", product_id, price_cents,
		observed_date);
	snprintf(observation->event_id, sizeof(observation->event_id), "justtcg-%016llx",
		fnv1a(hash_input));
	observation->product_id = product_id;
	observation->price_cents = price_cents;
	return 1;
}

#include "http.h"

#include <curl/curl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
	char *data;
	unsigned long len;
} response_buffer;

static size_t write_body(char *ptr, size_t size, size_t nmemb, void *userdata)
{
	size_t incoming = size * nmemb;
	response_buffer *buf = userdata;
	char *next = realloc(buf->data, buf->len + incoming + 1);
	if (next == NULL) {
		return 0;
	}

	buf->data = next;
	memcpy(buf->data + buf->len, ptr, incoming);
	buf->len += incoming;
	buf->data[buf->len] = '\0';

	return incoming;
}

void http_result_free(http_result *result)
{
	if (result == NULL) {
		return;
	}
	free(result->body);
	result->body = NULL;
	result->body_len = 0;
	result->status = 0;
}

static int perform(CURL *curl, http_result *result, char *err, unsigned long err_len)
{
	response_buffer buf = {0};
	CURLcode code;

	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_body);
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, &buf);
	curl_easy_setopt(curl, CURLOPT_USERAGENT, "fullart-price-sync/0.1");
	curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30L);

	code = curl_easy_perform(curl);
	if (code != CURLE_OK) {
		snprintf(err, err_len, "curl request failed: %s", curl_easy_strerror(code));
		free(buf.data);
		return 0;
	}

	curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &result->status);
	result->body = buf.data;
	result->body_len = buf.len;

	if (result->body == NULL) {
		result->body = calloc(1, 1);
		if (result->body == NULL) {
			snprintf(err, err_len, "unable to allocate empty response body");
			return 0;
		}
	}

	return 1;
}

int http_post_form_basic(const char *url, const char *username, const char *password,
	const char *form_body, http_result *result, char *err, unsigned long err_len)
{
	CURL *curl = curl_easy_init();
	struct curl_slist *headers = NULL;
	int ok;

	if (curl == NULL) {
		snprintf(err, err_len, "unable to initialize curl");
		return 0;
	}

	headers = curl_slist_append(headers, "Content-Type: application/x-www-form-urlencoded");
	curl_easy_setopt(curl, CURLOPT_URL, url);
	curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
	curl_easy_setopt(curl, CURLOPT_POST, 1L);
	curl_easy_setopt(curl, CURLOPT_POSTFIELDS, form_body);
	curl_easy_setopt(curl, CURLOPT_HTTPAUTH, CURLAUTH_BASIC);
	curl_easy_setopt(curl, CURLOPT_USERNAME, username);
	curl_easy_setopt(curl, CURLOPT_PASSWORD, password);

	ok = perform(curl, result, err, err_len);
	curl_slist_free_all(headers);
	curl_easy_cleanup(curl);
	return ok;
}

int http_get_bearer(const char *url, const char *bearer_token, const char *marketplace_id,
	http_result *result, char *err, unsigned long err_len)
{
	CURL *curl = curl_easy_init();
	struct curl_slist *headers = NULL;
	char auth[4096];
	char marketplace[128];
	int ok;

	if (curl == NULL) {
		snprintf(err, err_len, "unable to initialize curl");
		return 0;
	}

	snprintf(auth, sizeof(auth), "Authorization: Bearer %s", bearer_token);
	snprintf(marketplace, sizeof(marketplace), "X-EBAY-C-MARKETPLACE-ID: %s", marketplace_id);
	headers = curl_slist_append(headers, auth);
	headers = curl_slist_append(headers, marketplace);

	curl_easy_setopt(curl, CURLOPT_URL, url);
	curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

	ok = perform(curl, result, err, err_len);
	curl_slist_free_all(headers);
	curl_easy_cleanup(curl);
	return ok;
}

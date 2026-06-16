#ifndef FULLART_PRICE_SYNC_HTTP_H
#define FULLART_PRICE_SYNC_HTTP_H

typedef struct {
	/* HTTP status code returned by the server. */
	long status;
	/* Null-terminated response body allocated by the HTTP helper. */
	char *body;
	/* Response body length in bytes, excluding the null terminator. */
	unsigned long body_len;
} http_result;

/* Frees memory owned by an http_result and resets its fields. */
void http_result_free(http_result *result);

/*
 * Sends an application/x-www-form-urlencoded POST using HTTP Basic auth.
 * The caller owns result and must call http_result_free after use.
 */
int http_post_form_basic(const char *url, const char *username, const char *password,
	const char *form_body, http_result *result, char *err, unsigned long err_len);

/*
 * Sends a GET request with Authorization: Bearer and eBay marketplace headers.
 * The caller owns result and must call http_result_free after use.
 */
int http_get_bearer(const char *url, const char *bearer_token, const char *marketplace_id,
	http_result *result, char *err, unsigned long err_len);

/*
 * Sends a GET request with a single API-key style header, for APIs like JustTCG.
 * The caller owns result and must call http_result_free after use.
 */
int http_get_api_key(const char *url, const char *header_name, const char *api_key,
	http_result *result, char *err, unsigned long err_len);

#endif

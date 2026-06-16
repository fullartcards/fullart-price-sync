#include "config.h"
#include "ebay/ebay.h"
#include "justtcg/justtcg.h"

#include <curl/curl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void usage(const char *program)
{
	fprintf(stderr,
		"Usage:\n"
		"  %s ebay-search --product-id <id-or-sku> --query <query> [--limit <1-200>] [--raw]\n"
		"  %s justtcg-card --product-id <id-or-sku> --card-id <justtcg-card-id> [--raw]\n",
		program,
		program);
}

static const char *arg_value(int argc, char **argv, const char *name)
{
	for (int i = 2; i + 1 < argc; i++) {
		if (strcmp(argv[i], name) == 0) {
			return argv[i + 1];
		}
	}
	return NULL;
}

static int has_flag(int argc, char **argv, const char *name)
{
	for (int i = 2; i < argc; i++) {
		if (strcmp(argv[i], name) == 0) {
			return 1;
		}
	}
	return 0;
}

static int run_ebay_search(int argc, char **argv)
{
	const char *product_id = arg_value(argc, argv, "--product-id");
	const char *query = arg_value(argc, argv, "--query");
	const char *limit_raw = arg_value(argc, argv, "--limit");
	int limit = limit_raw == NULL ? 10 : atoi(limit_raw);
	int print_raw = has_flag(argc, argv, "--raw");
	char err[512] = {0};
	app_config cfg = {0};
	ebay_token token = {0};
	ebay_search_response response = {0};
	price_observation observation = {0};
	int ok = 1;

	if (product_id == NULL || query == NULL) {
		usage(argv[0]);
		return 2;
	}

	if (!load_config(&cfg, err, sizeof(err))) {
		fprintf(stderr, "config error: %s\n", err);
		return 1;
	}
	if (!require_ebay_config(&cfg, err, sizeof(err))) {
		fprintf(stderr, "config error: %s\n", err);
		return 1;
	}

	if (!ebay_mint_token(&cfg, &token, err, sizeof(err))) {
		fprintf(stderr, "token error: %s\n", err);
		ok = 0;
		goto done;
	}

	if (!ebay_search(&cfg, &token, query, limit, &response, err, sizeof(err))) {
		fprintf(stderr, "search error: %s\n", err);
		ok = 0;
		goto done;
	}

	if (print_raw) {
		printf("%s\n", response.body);
		goto done;
	}

	if (!ebay_first_price_observation(product_id, response.body, &observation, err,
		    sizeof(err))) {
		fprintf(stderr, "parse error: %s\n", err);
		ok = 0;
		goto done;
	}

	printf("{\"event_id\":\"%s\",\"product_id\":\"%s\",\"price\":%ld}\n",
		observation.event_id, observation.product_id, observation.price_cents);

done:
	ebay_search_response_free(&response);
	ebay_token_free(&token);
	return ok ? 0 : 1;
}

static int run_justtcg_card(int argc, char **argv)
{
	const char *product_id = arg_value(argc, argv, "--product-id");
	const char *card_id = arg_value(argc, argv, "--card-id");
	int print_raw = has_flag(argc, argv, "--raw");
	char err[512] = {0};
	app_config cfg = {0};
	justtcg_card_response response = {0};
	price_observation observation = {0};
	int ok = 1;

	if (product_id == NULL || card_id == NULL) {
		usage(argv[0]);
		return 2;
	}

	if (!load_config(&cfg, err, sizeof(err))) {
		fprintf(stderr, "config error: %s\n", err);
		return 1;
	}
	if (!require_justtcg_config(&cfg, err, sizeof(err))) {
		fprintf(stderr, "config error: %s\n", err);
		return 1;
	}

	if (!justtcg_get_card(&cfg, card_id, &response, err, sizeof(err))) {
		fprintf(stderr, "card error: %s\n", err);
		ok = 0;
		goto done;
	}

	if (print_raw) {
		printf("%s\n", response.body);
		goto done;
	}

	if (!justtcg_first_price_observation(product_id, response.body, &observation, err,
		    sizeof(err))) {
		fprintf(stderr, "parse error: %s\n", err);
		ok = 0;
		goto done;
	}

	printf("{\"event_id\":\"%s\",\"product_id\":\"%s\",\"price\":%ld}\n",
		observation.event_id, observation.product_id, observation.price_cents);

done:
	justtcg_card_response_free(&response);
	return ok ? 0 : 1;
}

int main(int argc, char **argv)
{
	int code;

	if (argc < 2) {
		usage(argv[0]);
		return 2;
	}

	curl_global_init(CURL_GLOBAL_DEFAULT);

	if (strcmp(argv[1], "ebay-search") == 0) {
		code = run_ebay_search(argc, argv);
	} else if (strcmp(argv[1], "justtcg-card") == 0) {
		code = run_justtcg_card(argc, argv);
	} else {
		usage(argv[0]);
		code = 2;
	}

	curl_global_cleanup();
	return code;
}

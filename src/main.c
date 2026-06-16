#include "config.h"
#include "ebay/ebay.h"
#include "justtcg/justtcg.h"

#include <curl/curl.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
	const app_config *cfg;
	const char *product_id;
	const char *query;
	int limit;
	char output[256];
	char err[512];
	int ok;
} ebay_worker_args;

typedef struct {
	const app_config *cfg;
	const char *product_id;
	const char *card_id;
	char output[256];
	char err[512];
	int ok;
} justtcg_worker_args;

static void usage(const char *program)
{
	fprintf(stderr,
		"Usage:\n"
		"  %s sync-product --product-id <id-or-sku> --ebay-query <query> --justtcg-card-id <card-id> [--limit <1-200>]\n"
		"  %s ebay-search --product-id <id-or-sku> --query <query> [--limit <1-200>] [--raw]\n"
		"  %s justtcg-card --product-id <id-or-sku> --card-id <justtcg-card-id> [--raw]\n",
		program,
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

static void format_observation(char *dst, unsigned long dst_len, const price_observation *observation)
{
	snprintf(dst, dst_len, "{\"event_id\":\"%s\",\"product_id\":\"%s\",\"price\":%ld}",
		observation->event_id, observation->product_id, observation->price_cents);
}

static void *run_ebay_worker(void *arg)
{
	ebay_worker_args *worker = arg;
	ebay_token token = {0};
	ebay_search_response response = {0};
	price_observation observation = {0};
	char err[512] = {0};

	worker->ok = 0;
	if (!ebay_mint_token(worker->cfg, &token, err, sizeof(err))) {
		snprintf(worker->err, sizeof(worker->err), "ebay token error: %s", err);
		goto done;
	}
	if (!ebay_search(worker->cfg, &token, worker->query, worker->limit, &response, err,
		    sizeof(err))) {
		snprintf(worker->err, sizeof(worker->err), "ebay search error: %s", err);
		goto done;
	}
	if (!ebay_first_price_observation(worker->product_id, response.body, &observation, err,
		    sizeof(err))) {
		snprintf(worker->err, sizeof(worker->err), "ebay parse error: %s", err);
		goto done;
	}

	format_observation(worker->output, sizeof(worker->output), &observation);
	worker->ok = 1;

done:
	ebay_search_response_free(&response);
	ebay_token_free(&token);
	return NULL;
}

static void *run_justtcg_worker(void *arg)
{
	justtcg_worker_args *worker = arg;
	justtcg_card_response response = {0};
	price_observation observation = {0};
	char err[512] = {0};

	worker->ok = 0;
	if (!justtcg_get_card(worker->cfg, worker->card_id, &response, err, sizeof(err))) {
		snprintf(worker->err, sizeof(worker->err), "justtcg card error: %s", err);
		goto done;
	}
	if (!justtcg_first_price_observation(worker->product_id, response.body, &observation, err,
		    sizeof(err))) {
		snprintf(worker->err, sizeof(worker->err), "justtcg parse error: %s", err);
		goto done;
	}

	format_observation(worker->output, sizeof(worker->output), &observation);
	worker->ok = 1;

done:
	justtcg_card_response_free(&response);
	return NULL;
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
	char output[256];
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

	format_observation(output, sizeof(output), &observation);
	printf("%s\n", output);

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
	char output[256];
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

	format_observation(output, sizeof(output), &observation);
	printf("%s\n", output);

done:
	justtcg_card_response_free(&response);
	return ok ? 0 : 1;
}

static int run_sync_product(int argc, char **argv)
{
	const char *product_id = arg_value(argc, argv, "--product-id");
	const char *ebay_query = arg_value(argc, argv, "--ebay-query");
	const char *justtcg_card_id = arg_value(argc, argv, "--justtcg-card-id");
	const char *limit_raw = arg_value(argc, argv, "--limit");
	int limit = limit_raw == NULL ? 10 : atoi(limit_raw);
	char err[512] = {0};
	app_config cfg = {0};
	pthread_t ebay_thread;
	pthread_t justtcg_thread;
	ebay_worker_args ebay_args = {0};
	justtcg_worker_args justtcg_args = {0};
	int ebay_started = 0;
	int justtcg_started = 0;
	int ok = 1;

	if (product_id == NULL || ebay_query == NULL || justtcg_card_id == NULL) {
		usage(argv[0]);
		return 2;
	}
	if (limit < 1 || limit > 200) {
		fprintf(stderr, "config error: limit must be between 1 and 200\n");
		return 1;
	}
	if (!load_config(&cfg, err, sizeof(err))) {
		fprintf(stderr, "config error: %s\n", err);
		return 1;
	}
	if (!require_ebay_config(&cfg, err, sizeof(err))) {
		fprintf(stderr, "config error: %s\n", err);
		return 1;
	}
	if (!require_justtcg_config(&cfg, err, sizeof(err))) {
		fprintf(stderr, "config error: %s\n", err);
		return 1;
	}

	ebay_args.cfg = &cfg;
	ebay_args.product_id = product_id;
	ebay_args.query = ebay_query;
	ebay_args.limit = limit;
	justtcg_args.cfg = &cfg;
	justtcg_args.product_id = product_id;
	justtcg_args.card_id = justtcg_card_id;

	if (pthread_create(&ebay_thread, NULL, run_ebay_worker, &ebay_args) != 0) {
		fprintf(stderr, "thread error: unable to start eBay worker\n");
		ok = 0;
		goto done;
	}
	ebay_started = 1;
	if (pthread_create(&justtcg_thread, NULL, run_justtcg_worker, &justtcg_args) != 0) {
		fprintf(stderr, "thread error: unable to start JustTCG worker\n");
		ok = 0;
		goto done;
	}
	justtcg_started = 1;

done:
	if (ebay_started) {
		pthread_join(ebay_thread, NULL);
	}
	if (justtcg_started) {
		pthread_join(justtcg_thread, NULL);
	}

	if (ebay_args.ok) {
		printf("%s\n", ebay_args.output);
	} else if (ebay_started) {
		fprintf(stderr, "%s\n", ebay_args.err);
		ok = 0;
	}
	if (justtcg_args.ok) {
		printf("%s\n", justtcg_args.output);
	} else if (justtcg_started) {
		fprintf(stderr, "%s\n", justtcg_args.err);
		ok = 0;
	}

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

	if (strcmp(argv[1], "sync-product") == 0) {
		code = run_sync_product(argc, argv);
	} else if (strcmp(argv[1], "ebay-search") == 0) {
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

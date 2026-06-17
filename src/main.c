#include "config.h"
#include "db/db.h"
#include "ebay/ebay.h"
#include "justtcg/justtcg.h"

#include <curl/curl.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
	const app_config *cfg;
	const char *query;
	int limit;
	char *output;
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

typedef struct {
	const app_config *cfg;
	const justtcg_card_mapping_list *mappings;
	pthread_mutex_t lock;
	unsigned long next_index;
	unsigned long success_count;
	unsigned long failure_count;
} justtcg_batch_state;

static void usage(const char *program)
{
	fprintf(stderr,
		"Usage:\n"
		"  %s sync-product --product-id <id-or-sku> --ebay-query <query> --justtcg-card-id <card-id> [--limit <1-200>]\n"
		"  %s ebay-search --query <query> [--limit <1-200>] [--raw]\n"
		"  %s justtcg-card --product-id <id-or-sku> --card-id <justtcg-card-id> [--raw]\n"
		"  %s justtcg-sync [--workers <1-32>]\n",
		program,
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

static int append_observation_line(char **dst, unsigned long *dst_len,
	const price_observation *observation, char *err, unsigned long err_len)
{
	char line[256];
	char *next;
	unsigned long line_len;

	format_observation(line, sizeof(line), observation);
	line_len = strlen(line);
	next = realloc(*dst, *dst_len + line_len + 2);
	if (next == NULL) {
		snprintf(err, err_len, "unable to allocate observation output");
		return 0;
	}
	*dst = next;
	memcpy(*dst + *dst_len, line, line_len);
	*dst_len += line_len;
	(*dst)[(*dst_len)++] = '\n';
	(*dst)[*dst_len] = '\0';
	return 1;
}

static int format_ebay_observation_lines(const char *search_body, char **output,
	char *err, unsigned long err_len)
{
	ebay_price_observation_list observations = {0};
	unsigned long output_len = 0;

	*output = NULL;
	if (!ebay_price_observations(search_body, &observations, err, err_len)) {
		return 0;
	}
	for (unsigned long i = 0; i < observations.len; i++) {
		if (!append_observation_line(output, &output_len, &observations.items[i], err, err_len)) {
			free(*output);
			*output = NULL;
			ebay_price_observation_list_free(&observations);
			return 0;
		}
	}
	ebay_price_observation_list_free(&observations);
	return 1;
}

static void *run_ebay_worker(void *arg)
{
	ebay_worker_args *worker = arg;
	ebay_token token = {0};
	ebay_search_response response = {0};
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
	if (!format_ebay_observation_lines(response.body, &worker->output, err, sizeof(err))) {
		snprintf(worker->err, sizeof(worker->err), "ebay parse error: %s", err);
		goto done;
	}

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

static int parse_worker_count(const char *value)
{
	int workers;

	if (value == NULL) {
		return 4;
	}
	workers = atoi(value);
	if (workers < 1 || workers > 32) {
		return 0;
	}
	return workers;
}

static int fetch_justtcg_observation(const app_config *cfg, const char *product_id,
	const char *card_id, char *output, unsigned long output_len, char *err, unsigned long err_len)
{
	justtcg_card_response response = {0};
	price_observation observation = {0};
	char detail[512] = {0};
	int ok = 0;

	if (!justtcg_get_card(cfg, card_id, &response, detail, sizeof(detail))) {
		snprintf(err, err_len, "justtcg card error for %s: %s", product_id, detail);
		goto done;
	}
	if (!justtcg_first_price_observation(product_id, response.body, &observation, detail,
		    sizeof(detail))) {
		snprintf(err, err_len, "justtcg parse error for %s: %s", product_id, detail);
		goto done;
	}

	format_observation(output, output_len, &observation);
	ok = 1;

done:
	justtcg_card_response_free(&response);
	return ok;
}

static void *run_justtcg_batch_worker(void *arg)
{
	justtcg_batch_state *state = arg;

	for (;;) {
		unsigned long index;
		const justtcg_card_mapping *mapping;
		char output[256];
		char err[512];
		int ok;

		pthread_mutex_lock(&state->lock);
		index = state->next_index;
		if (index >= state->mappings->len) {
			pthread_mutex_unlock(&state->lock);
			break;
		}
		state->next_index++;
		pthread_mutex_unlock(&state->lock);

		mapping = &state->mappings->items[index];
		ok = fetch_justtcg_observation(state->cfg, mapping->product_id,
			mapping->justtcg_card_id, output, sizeof(output), err, sizeof(err));

		pthread_mutex_lock(&state->lock);
		if (ok) {
			puts(output);
			state->success_count++;
		} else {
			fprintf(stderr, "%s\n", err);
			state->failure_count++;
		}
		pthread_mutex_unlock(&state->lock);
	}

	return NULL;
}

static int run_ebay_search(int argc, char **argv)
{
	const char *query = arg_value(argc, argv, "--query");
	const char *limit_raw = arg_value(argc, argv, "--limit");
	int limit = limit_raw == NULL ? 10 : atoi(limit_raw);
	int print_raw = has_flag(argc, argv, "--raw");
	char err[512] = {0};
	app_config cfg = {0};
	ebay_token token = {0};
	ebay_search_response response = {0};
	char *output = NULL;
	int ok = 1;

	if (query == NULL) {
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

	if (!format_ebay_observation_lines(response.body, &output, err, sizeof(err))) {
		fprintf(stderr, "parse error: %s\n", err);
		ok = 0;
		goto done;
	}

	printf("%s", output);

done:
	free(output);
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

static int run_justtcg_sync(int argc, char **argv)
{
	const char *workers_raw = arg_value(argc, argv, "--workers");
	int worker_count = parse_worker_count(workers_raw);
	char err[512] = {0};
	app_config cfg = {0};
	justtcg_card_mapping_list mappings = {0};
	justtcg_batch_state state = {0};
	pthread_t *threads = NULL;
	int started = 0;
	int ok = 1;

	if (worker_count == 0) {
		fprintf(stderr, "config error: workers must be between 1 and 32\n");
		return 1;
	}
	if (!load_config(&cfg, err, sizeof(err))) {
		fprintf(stderr, "config error: %s\n", err);
		return 1;
	}
	if (!require_justtcg_config(&cfg, err, sizeof(err))) {
		fprintf(stderr, "config error: %s\n", err);
		return 1;
	}
	if (!require_database_config(&cfg, err, sizeof(err))) {
		fprintf(stderr, "config error: %s\n", err);
		return 1;
	}
	if (!db_load_justtcg_card_mappings(&cfg, &mappings, err, sizeof(err))) {
		fprintf(stderr, "database error: %s\n", err);
		return 1;
	}
	if (mappings.len == 0) {
		fprintf(stderr, "JustTCG sync found no mapped card products\n");
		goto done;
	}
	if ((unsigned long)worker_count > mappings.len) {
		worker_count = (int)mappings.len;
	}

	threads = calloc((unsigned long)worker_count, sizeof(*threads));
	if (threads == NULL) {
		fprintf(stderr, "runtime error: unable to allocate worker threads\n");
		ok = 0;
		goto done;
	}

	state.cfg = &cfg;
	state.mappings = &mappings;
	if (pthread_mutex_init(&state.lock, NULL) != 0) {
		fprintf(stderr, "runtime error: unable to initialize worker lock\n");
		ok = 0;
		goto done;
	}

	for (int i = 0; i < worker_count; i++) {
		if (pthread_create(&threads[i], NULL, run_justtcg_batch_worker, &state) != 0) {
			fprintf(stderr, "thread error: unable to start JustTCG sync worker\n");
			ok = 0;
			break;
		}
		started++;
	}
	for (int i = 0; i < started; i++) {
		pthread_join(threads[i], NULL);
	}
	pthread_mutex_destroy(&state.lock);

	if (state.failure_count > 0) {
		ok = 0;
	}
	fprintf(stderr, "JustTCG sync complete: %lu succeeded, %lu failed\n",
		state.success_count, state.failure_count);

done:
	free(threads);
	justtcg_card_mapping_list_free(&mappings);
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
		printf("%s", ebay_args.output);
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

	free(ebay_args.output);
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
	} else if (strcmp(argv[1], "justtcg-sync") == 0) {
		code = run_justtcg_sync(argc, argv);
	} else {
		usage(argv[0]);
		code = 2;
	}

	curl_global_cleanup();
	return code;
}

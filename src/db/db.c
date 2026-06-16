#include "db/db.h"

#include <libpq-fe.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char *dup_pg_value(PGresult *result, int row, int col)
{
	const char *value = PQgetvalue(result, row, col);
	char *copy;
	unsigned long len;

	if (value == NULL) {
		return NULL;
	}
	len = strlen(value);
	copy = malloc(len + 1);
	if (copy == NULL) {
		return NULL;
	}
	memcpy(copy, value, len + 1);
	return copy;
}

void justtcg_card_mapping_list_free(justtcg_card_mapping_list *list)
{
	if (list == NULL) {
		return;
	}
	for (unsigned long i = 0; i < list->len; i++) {
		free(list->items[i].product_id);
		free(list->items[i].justtcg_card_id);
	}
	free(list->items);
	list->items = NULL;
	list->len = 0;
}

int db_load_justtcg_card_mappings(const app_config *cfg, justtcg_card_mapping_list *list,
	char *err, unsigned long err_len)
{
	PGconn *conn = NULL;
	PGresult *result = NULL;
	justtcg_card_mapping *items = NULL;
	int rows = 0;
	int ok = 0;

	if (list == NULL) {
		snprintf(err, err_len, "mapping list is required");
		return 0;
	}
	list->items = NULL;
	list->len = 0;

	conn = PQconnectdb(cfg->database_url);
	if (conn == NULL) {
		snprintf(err, err_len, "Postgres connection allocation failed");
		goto done;
	}
	if (PQstatus(conn) != CONNECTION_OK) {
		snprintf(err, err_len, "Postgres connection failed: %.240s", PQerrorMessage(conn));
		goto done;
	}

	result = PQexec(conn, cfg->justtcg_card_query);
	if (PQresultStatus(result) != PGRES_TUPLES_OK) {
		snprintf(err, err_len, "JustTCG mapping query failed: %.240s", PQerrorMessage(conn));
		goto done;
	}
	if (PQnfields(result) < 2) {
		snprintf(err, err_len, "JustTCG mapping query must return product_id and justtcg_card_id");
		goto done;
	}

	rows = PQntuples(result);
	if (rows == 0) {
		ok = 1;
		goto done;
	}

	items = calloc((unsigned long)rows, sizeof(*items));
	if (items == NULL) {
		snprintf(err, err_len, "unable to allocate JustTCG mapping list");
		goto done;
	}

	for (int i = 0; i < rows; i++) {
		if (PQgetisnull(result, i, 0) || PQgetisnull(result, i, 1)) {
			snprintf(err, err_len, "JustTCG mapping query returned NULL in row %d", i + 1);
			goto done;
		}
		items[i].product_id = dup_pg_value(result, i, 0);
		items[i].justtcg_card_id = dup_pg_value(result, i, 1);
		if (items[i].product_id == NULL || items[i].justtcg_card_id == NULL) {
			snprintf(err, err_len, "unable to copy JustTCG mapping row %d", i + 1);
			goto done;
		}
	}

	list->items = items;
	list->len = (unsigned long)rows;
	items = NULL;
	ok = 1;

done:
	if (items != NULL) {
		justtcg_card_mapping_list partial = {
			.items = items,
			.len = rows > 0 ? (unsigned long)rows : 0,
		};
		justtcg_card_mapping_list_free(&partial);
	}
	PQclear(result);
	if (conn != NULL) {
		PQfinish(conn);
	}
	return ok;
}

#include "db/db.h"

#include <stdio.h>

void justtcg_card_mapping_list_free(justtcg_card_mapping_list *list)
{
	if (list == NULL) {
		return;
	}
	list->items = NULL;
	list->len = 0;
}

int db_load_justtcg_card_mappings(const app_config *cfg, justtcg_card_mapping_list *list,
	char *err, unsigned long err_len)
{
	(void)cfg;

	if (list != NULL) {
		list->items = NULL;
		list->len = 0;
	}
	snprintf(err, err_len,
		"Postgres support was not built; install libpq/pg_config and rebuild");
	return 0;
}

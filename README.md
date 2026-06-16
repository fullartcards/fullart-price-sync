# fullart-price-sync

Price syncing engine for Full Art.

The first targets are eBay Browse API and JustTCG card price ingestion. The
current v1 contract keeps the output intentionally small:

```json
{"event_id":"ebay-...","product_id":"1001-or-sku","price":800}
```

`price` is stored as integer cents.

## Initial design

```text
daily scheduler
  -> source adapter
  -> minimal price observation
  -> later: gRPC ingestion into fullart-api
```

## Build

```sh
make
```

The C code uses libcurl for HTTP calls and pthreads to run source adapters
concurrently for a product sync.

## Source layout

```text
src/ebay/      eBay OAuth and Browse API logic
src/justtcg/   JustTCG /v1/cards logic
src/http.*     shared libcurl helpers
src/config.*   shared environment config
```

## Configuration

Copy `.env.example` into your shell configuration or export the values directly:

```sh
export EBAY_ENV=sandbox
export EBAY_CLIENT_ID="..."
export EBAY_CLIENT_SECRET="..."
export EBAY_MARKETPLACE_ID=EBAY_US
export EBAY_SCOPE=https://api.ebay.com/oauth/api_scope
export JUSTTCG_API_KEY="..."
```

Use `EBAY_ENV=production` when you are ready to call production eBay APIs.
JustTCG uses `JUSTTCG_API_KEY` in the `x-api-key` request header.

## Product sync

Run both eBay and JustTCG for the same Full Art product id or SKU:

```sh
set -a
source .env
set +a

make

bin/fullart-price-sync sync-product \
  --product-id "GD01-001-NM" \
  --ebay-query "Gundam GD01-001 near mint" \
  --justtcg-card-id "pokemon-battle-academy-fire-energy-22-charizard-stamped-promo" \
  --limit 10
```

The command starts both source requests at the same time and prints one
observation per successful source:

```json
{"event_id":"ebay-...","product_id":"GD01-001-NM","price":800}
{"event_id":"justtcg-...","product_id":"GD01-001-NM","price":14}
```

## eBay search

```sh
set -a
source .env
set +a

make

bin/fullart-price-sync ebay-search \
  --product-id "GD01-001-NM" \
  --query "Gundam GD01-001 near mint" \
  --limit 10
```

Print the raw eBay response instead:

```sh
set -a
source .env
set +a

make

bin/fullart-price-sync ebay-search \
  --product-id 1001 \
  --query "Gundam GD01-001 near mint" \
  --limit 10 \
  --raw
```

## JustTCG card lookup

JustTCG card lookup calls:

```text
GET https://api.justtcg.com/v1/cards?cardId=<card-id>
```

and sends:

```text
x-api-key: <JUSTTCG_API_KEY>
```

Run it with:

```sh
set -a
source .env
set +a

make

bin/fullart-price-sync justtcg-card \
  --product-id "GD01-001-NM" \
  --card-id "pokemon-battle-academy-fire-energy-22-charizard-stamped-promo"
```

Print the raw JustTCG response instead:

```sh
bin/fullart-price-sync justtcg-card \
  --product-id "GD01-001-NM" \
  --card-id "pokemon-battle-academy-fire-energy-22-charizard-stamped-promo" \
  --raw
```

## Next code changes

1. Add a local product-source mapping file or endpoint so the sync job can map
   Full Art product ids/SKUs to eBay queries and JustTCG card ids.
2. Parse more than the first returned price and filter bad matches before
   emitting observations.
3. Add a gRPC or HTTP ingestion client that sends observations to `fullart-api`.
4. Add a daily runner that iterates through all active product mappings.

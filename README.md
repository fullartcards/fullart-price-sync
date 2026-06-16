# fullart-price-sync

Price syncing engine for Full Art.

The first target is eBay Browse API ingestion. The current v1 contract keeps the
output intentionally small:

```json
{"event_id":"ebay-...","product_id":"1001-or-sku","price":800}
```

`price` is stored as integer cents.

## Initial design

```text
daily scheduler
  -> source adapter, starting with eBay
  -> minimal price observation
  -> later: gRPC ingestion into fullart-api
```

## Build

```sh
make
```

The C code uses libcurl for OAuth and Browse API HTTP calls.

## Configuration

Copy `.env.example` into your shell configuration or export the values directly:

```sh
export EBAY_ENV=sandbox
export EBAY_CLIENT_ID="..."
export EBAY_CLIENT_SECRET="..."
export EBAY_MARKETPLACE_ID=EBAY_US
export EBAY_SCOPE=https://api.ebay.com/oauth/api_scope
```

Use `EBAY_ENV=production` when you are ready to call production eBay APIs.

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

## Next code changes

1. Add a local product-source mapping file or endpoint so the sync job can map
   Full Art product ids/SKUs to eBay queries.
2. Parse more than the first returned price and filter bad matches before
   emitting observations.
3. Add a gRPC or HTTP ingestion client that sends observations to `fullart-api`.
4. Add a daily runner that iterates through all active product mappings.

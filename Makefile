.PHONY: test

test:
	mkdir -p .competitive-verifier/cache
	competitive-verifier oj-resolve --config .competitive-verifier/config.toml --include tests --no-bundle > .competitive-verifier/cache/verify_files.json
	competitive-verifier verify --verify-json .competitive-verifier/cache/verify_files.json --check-error --no-download --output .competitive-verifier/cache/result.json

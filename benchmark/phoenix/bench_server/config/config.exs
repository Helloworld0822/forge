import Config

config :bench_server, BenchServerWeb.Endpoint,
  url: [host: "127.0.0.1", port: 19_082],
  http: [
    ip: {127, 0, 0, 1},
    port: 19_082,
    thousand_island_options: [num_acceptors: max(System.schedulers_online() * 2, 8)]
  ],
  adapter: Bandit.PhoenixAdapter,
  server: true,
  secret_key_base: "bench_secret_key_base_for_forge_http_benchmark_must_be_at_least_sixty_four_bytes_long_ok"

config :phoenix, :json_library, Jason

config :logger, level: :error

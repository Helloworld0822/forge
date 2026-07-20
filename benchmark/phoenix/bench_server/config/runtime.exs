import Config

if config_env() == :prod do
  port = String.to_integer(System.get_env("PORT", "19082"))

  config :bench_server, BenchServerWeb.Endpoint,
    url: [host: "127.0.0.1", port: port],
    http: [
      ip: {127, 0, 0, 1},
      port: port,
      thousand_island_options: [num_acceptors: max(System.schedulers_online() * 2, 8)]
    ],
    adapter: Bandit.PhoenixAdapter
end

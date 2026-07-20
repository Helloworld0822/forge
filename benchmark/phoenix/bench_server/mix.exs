defmodule BenchServer.MixProject do
  use Mix.Project

  def project do
    [
      app: :bench_server,
      version: "0.1.0",
      elixir: "~> 1.14",
      start_permanent: Mix.env() == :prod,
      deps: deps(),
      releases: releases()
    ]
  end

  def application do
    [
      extra_applications: [:logger],
      mod: {BenchServer.Application, []}
    ]
  end

  defp deps do
    [
      {:phoenix, "~> 1.7.14"},
      {:bandit, "~> 1.5"},
      {:jason, "~> 1.4"}
    ]
  end

  defp releases do
    [
      bench_server: [
        include_executables_for: [:unix],
        applications: [bench_server: :permanent]
      ]
    ]
  end
end

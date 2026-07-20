defmodule BenchServer.Application do
  @moduledoc false
  use Application

  @impl true
  def start(_type, _args) do
    children = [
      BenchServerWeb.Endpoint
    ]

    Supervisor.start_link(children, strategy: :one_for_one, name: BenchServer.Supervisor)
  end
end

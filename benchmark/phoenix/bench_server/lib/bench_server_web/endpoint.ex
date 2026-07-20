defmodule BenchServerWeb.Endpoint do
  @moduledoc false
  use Phoenix.Endpoint, otp_app: :bench_server

  plug BenchServerWeb.Router
end

defmodule BenchServerWeb.HelloController do
  @moduledoc false
  use Phoenix.Controller, formats: [:html]

  @body "Hello, World"

  def index(conn, _params) do
    conn
    |> put_resp_header("connection", "close")
    |> put_resp_content_type("text/plain")
    |> send_resp(200, @body)
  end
end

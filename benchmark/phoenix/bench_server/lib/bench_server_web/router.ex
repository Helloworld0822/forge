defmodule BenchServerWeb.Router do
  @moduledoc false
  use Phoenix.Router

  get "/", BenchServerWeb.HelloController, :index
end

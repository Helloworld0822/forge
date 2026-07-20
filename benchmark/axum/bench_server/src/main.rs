use axum::{
    http::{header, HeaderValue},
    response::IntoResponse,
    routing::get,
    Router,
};
use std::net::SocketAddr;

#[tokio::main]
async fn main() {
    let port: u16 = std::env::var("PORT")
        .ok()
        .and_then(|p| p.parse().ok())
        .unwrap_or(19083);

    let app = Router::new().route("/", get(hello));
    let addr = SocketAddr::from(([127, 0, 0, 1], port));

    println!("Rust Axum benchmark server on port {port}");
    let listener = tokio::net::TcpListener::bind(addr).await.unwrap();
    axum::serve(listener, app).await.unwrap();
}

async fn hello() -> impl IntoResponse {
    (
        [(header::CONNECTION, HeaderValue::from_static("close"))],
        "Hello, World",
    )
}

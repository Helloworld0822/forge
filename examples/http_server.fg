import io;
import http;

process main {
    let server: int = http_listen(8080);
    println("http server on port 8080");

    let req: int = http_accept(server);
    let path: string = http_req_path(req);
    let method: string = http_req_method(req);
    println(method, path);

    let body: string = "Hello from Forge HTTP server";
    http_respond(req, 200, body);
    http_close(req);
    http_server_close(server);
}

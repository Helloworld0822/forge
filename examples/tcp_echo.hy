import io;
import tcp;

process main {
    let server: int = tcp_listen(9000);
    println("tcp echo server on port 9000");

    let client: int = tcp_accept(server);
    let msg: string = tcp_recv(client);
    println("received:", msg);

    tcp_send(client, msg);
    tcp_close(client);
    tcp_close(server);
}

import io;
import udp;

process main {
    let sock: int = udp_bind(9001);
    println("udp server on port 9001");

    let msg: string = udp_recv(sock);
    let peer: string = udp_peer(sock);
    println("from", peer, ":", msg);

    udp_send(sock, "127.0.0.1", 9001, "pong");
    udp_close(sock);
}

process main {
    coroutine worker(id: int) {
        println("worker start", id);
        yield;
        println("worker done", id);
    }

    spawn worker(1);
    spawn worker(2);
    spawn worker(3);
}

process main {
    coroutine worker(id: int) {
        let step: int = id;
        println("worker", step);
        yield;
        step = step + 10;
        println("done", step);
    }

    spawn worker(1);
    spawn worker(2);
}

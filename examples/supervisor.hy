process worker_proc {
    coroutine task(n: int) {
        println("task", n);
        yield;
        println("task done", n);
    }

    spawn task(1);
    spawn task(2);
}

supervisor app {
    restart: all;
    worker_proc;
}

process main {
    println("supervisor demo");
}

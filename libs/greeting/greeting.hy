library greeting {
    import strings;

    export fn hello(name: string): string {
        return str_concat("Hello, ", name);
    }

    export fn shout(name: string): string {
        return str_concat(str_concat("Hey ", name), "!");
    }
}

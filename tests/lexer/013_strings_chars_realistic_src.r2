// Realistic use of strings, chars and comments.
fn greet() {
    var c: char = 'H';
    print("hello world");
    print("line\nbreak");
    print("tab\there");
    print("say \"hi\"");
    print('x');
    c = '\n';
    c = '\'';
}

fn main() {
    // Greet and show escapes.
    greet();
    var sep: char = ':';
    var q: char = '"';
    log("first\\second", "it's ok");
}

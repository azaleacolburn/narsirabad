use std::collections::HashMap;

#[derive(Debug)]
enum Item<'a> {
    Object,
    Pointer(Pointer<'a>),
}

#[derive(Debug)]
struct Object<'a> {
    size: usize,
    pointers: Vec<Pointer<'a>>,
}

#[derive(Debug)]
struct Pointer<'a> {
    pointee: &'a Item<'a>,
}

#[derive(Debug)]
pub struct Gc<'a> {
    stack_pointers: HashMap<String, Pointer<'a>>,
}

impl<'a> Gc<'a> {
    pub fn new() -> Gc<'a> {
        Self {
            stack_pointers: HashMap::new(),
        }
    }
}

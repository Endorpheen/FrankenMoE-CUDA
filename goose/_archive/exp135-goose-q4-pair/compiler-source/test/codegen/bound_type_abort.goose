// A bound type's values reach the length itself, and no further (§3.11).

import std;

var items: i32[>..] = [];

fn bound(x: i64) -> u32 upto items { x as (u32 upto items) }

fn main() {
    for v in 3 { items.push(v as i32); }
    print(bound(3));
    print(bound(4));
    print("not reached");
}

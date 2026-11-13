// What an index or bound type gives the bounds-check pass (§3.11, §10.5): a
// value of `u32 in nodes` is below nodes.len at any later point, since nodes
// never shrinks; against another array the check reduces to one relation
// between the two lengths, which the analysis states and keeps as any other.

import std;

var nodes: i32[>..] = [];
type NodeId = u32 in nodes;
var links: NodeId[>..] = [];

// Into the array the type names: a load is enough, whatever the analysis
// knows about where it came from.
fn own(k: i64) -> i32 {
    guard k >= 0 && k < links.len else { return 0; }
    let w = links[k];                          // bce:elide
    return nodes[w];                           // bce:elide (w is an index of nodes)
}

// Into another array: one length relation settles every index.
fn parallel(dist: i32[>..]&) -> i64 {
    assert(dist.len >= nodes.len);
    var t = 0;
    for k in links.len {
        let w = links[k];                      // bce:elide
        t += dist[w];                          // bce:elide (w < nodes.len <= dist.len)
    }
    return t;
}

// The relation is about the lengths as they were: once nodes grows, a new
// index may be past the end of dist.
fn grown(dist: i32[>..]&) -> i32 {
    assert(dist.len >= nodes.len);
    nodes.push(0);
    let w = (nodes.len - 1) as NodeId;         // bce:elide
    return dist[w];                            // bce:keep (nodes grew after the assert)
}

// A plain integer read out of storage has no known value; its conversion
// is the check, and what it makes needs none.
fn convert(raw: u32[:]) -> i64 {
    var t = 0;
    for x in raw {
        let w = x as NodeId;                   // bce:keep (x is data)
        t += nodes[w];                         // bce:elide
    }
    return t;
}

// A bound type's values reach the length itself: the end of a range.
fn ranged(lo: u32 upto links, hi: u32 upto links) -> i64 {
    var t = 0;
    for k in lo..hi {
        t += nodes[links[k]];                  // bce:elide
    }
    return t;
}

fn main() {
    for v in 4 { nodes.push(v as i32 * 10); }
    for v in 4 { links.push((3 - v) as NodeId); }       // bce:elide
    var dist: i32[>..] = [];
    for v in nodes.len { dist.push(v as i32); }
    var raw: u32[>..] = [];
    raw.push(1);
    raw.push(2);
    print(own(1), " ", parallel(dist), " ", convert(raw), " ",
          ranged(1 as (u32 upto links), links.len as (u32 upto links)));   // bce:elide
    for v in nodes.len { dist.push(0); }
    print(grown(dist));
}

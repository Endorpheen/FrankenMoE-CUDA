// P6-07: batch admissibility checks, in Goose.
//
// Native reference: src/llama-batch.cpp, the "consistency checks" block and the plain-position section.
// Three rejection rules are transferred; the sequence-set rule is reduced to a 64-bit mask per token, which is
// exact for n_seq_max <= 64 and is asserted.
//
//   rule 1 (continuity): for every sequence that appears in the batch, with a memory present and a known last
//          position, the batch's first position must be last + 1, and the position span must not exceed the number of
//          tokens the sequence carries.
//   rule 2 (decreasing): positions of one sequence must not decrease along the batch.
//   rule 3 (sequence sets): a sequence's running set intersection must stay non-empty; an empty one means the token
//          belongs to incompatible sequence sets.
//
// Returns 0 when admissible, otherwise 1/2/3 naming the rule that refused.

extern "fg291b_fail" fn fail(c:i64);
extern "fg291b_ld_i64" fn ld_i64(base:u64, byte:i64)->i64;
extern "fg291b_ld_u64" fn ld_u64(base:u64, byte:i64)->i64;
extern "fg291b_st_i64" fn st_i64(base:u64, byte:i64, v:i64);

fn ensure(v:bool,c:i64){if !v{fail(c);}}
fn at(a:u64, i:i64)->i64 {return ld_i64(a, i*8 as i64);}
fn atu(a:u64, i:i64)->i64 {return ld_u64(a, i*8 as i64);}

// positions: pos[n_tokens]; per sequence: seq_min[], seq_max[], seq_count[]; memory: has_mem, mem_max[]
fn check_continuity(n_tokens:i64, pos:u64, seq_min:u64, seq_max:u64, seq_count:u64,
                    has_mem:i64, mem_max:u64, n_seq:i64) -> i64 {
    ensure(n_tokens >= 0 as i64 && n_seq >= 0 as i64, 10);
    if has_mem == 0 as i64 {return 0 as i64;}
    var s:i64 = 0 as i64;
    while s < n_seq {
        if at(seq_count, s) > 0 as i64 {
            let p0:i64 = at(mem_max, s);
            if p0 >= 0 as i64 {
                if at(seq_min, s) != p0 + 1 as i64 {return 1 as i64;}
                if at(seq_max, s) - at(seq_min, s) + 1 as i64 > at(seq_count, s) {return 1 as i64;}
            }
        }
        s = s + 1 as i64;
    }
    0 as i64
}

// The native walk checks, for every token and every sequence it carries, both the running set intersection and the
// non-decreasing position rule. mask_of[token] is the token's 64-bit sequence set, pos[token] its position; cur_set and
// cur_pos are scratch arrays of n_seq entries. Returns 3 when an intersection empties, 2 on a decreasing position, else 0.
fn check_walk(n_tokens:i64, pos:u64, mask_of:u64, n_seq:i64, cur_set:u64, cur_pos:u64) -> i64 {
    ensure(n_tokens >= 0 as i64 && n_seq >= 0 as i64 && n_seq <= 64 as i64, 11);
    let all:i64 = 0 as i64 - 1 as i64;
    var s:i64 = 0 as i64;
    while s < n_seq {
        st_i64(cur_set, s*8 as i64, all);
        st_i64(cur_pos, s*8 as i64, 0 as i64 - 1 as i64);
        s = s + 1 as i64;
    }
    var i:i64 = 0 as i64;
    while i < n_tokens {
        let m:i64 = atu(mask_of, i);
        let p:i64 = at(pos, i);
        var k:i64 = 0 as i64;
        while k < n_seq {
            if ((m >> k) & 1 as i64) != 0 as i64 {
                let cur:i64 = at(cur_set, k) & m;
                if cur == 0 as i64 {return 3 as i64;}
                st_i64(cur_set, k*8 as i64, cur);
                if p < at(cur_pos, k) {return 2 as i64;}
                st_i64(cur_pos, k*8 as i64, p);
            }
            k = k + 1 as i64;
        }
        i = i + 1 as i64;
    }
    0 as i64
}

fn main(){
    check_continuity(0 as i64, 0 as u64, 0 as u64, 0 as u64, 0 as u64, 0 as i64, 0 as u64, 0 as i64);
    check_walk(0 as i64, 0 as u64, 0 as u64, 0 as i64, 0 as u64, 0 as u64);
}

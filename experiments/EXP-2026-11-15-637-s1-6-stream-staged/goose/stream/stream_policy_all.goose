// S1.6 composition: the P9-03 and P9-04 stream decisions as one standalone
// module, so a single generated C file carries every export.
//
// The two experiments each ship their own combined unit (stream_policy.goose in
// EXP-621, stream_hooks.goose in EXP-629) because the slice headers declare the
// same sl_i64/sl_u8 typedefs and cannot be included side by side. This file
// joins the two halves for the staged build; the six imported files are byte
// copies of goose/model/ and no policy lives here.
import expert_lru;    // xl_  expert cache: touch/promote, evict, budget
import dense_place;   // dw_  dense byte ranges and the complement subtraction
import stream_plan;   // sp_  slab geometry and the materialize limit
import hook_rules;    // rh_  router hook: capture, isolation, deferral, order
import overlap_rules; // ov_  I/O-compute overlap: waves, gate, stall union
import row_select;    // rs_  row-source verdict and the hook's dispatch

// P9-04: the router hook, the I/O-compute overlap and the row-source selection,
// as one module. The three files it imports are the slices:
//
//   hook_rules.goose     router_hook.cpp   activation, node-name matching,
//                                          isolation, terminal learning and
//                                          deferral, the topk/ask call order
//   overlap_rules.goose  expert_stream_source.cpp (overlap path) +
//                        stall_union.h     which batch to wait for, what to
//                                          copy ahead, when a slice is ready,
//                                          where the staging window ends
//   row_select.goose     router_hook.cpp capture + row_stream.cpp takeover +
//                        session.cpp       which tables become a row source and
//                                          what the hook does with one
//
// The import exists so one generated C file carries every export: the three
// headers declare the same slice typedefs and cannot be included side by side.
// No policy lives in this file.
import hook_rules;
import overlap_rules;
import row_select;

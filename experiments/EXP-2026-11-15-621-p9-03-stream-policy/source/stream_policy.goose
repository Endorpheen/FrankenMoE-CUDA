// P9-03: the policy core of the expert stream and the dense/row source, as one
// module. The three files it imports are the slices:
//
//   expert_lru.goose    expert_stream_source.cpp  cache, LRU, retention,
//                                                 residency, prefetch filter,
//                                                 budget, demand accounting
//   dense_place.goose   dense_weights.cpp         dense byte ranges, oversized
//                                                 hold-back, row takeover,
//                                                 init verdict
//   stream_plan.goose   row_stream.cpp            slab geometry, gather plan,
//                                                 eviction guard, fetch order,
//                                                 materialize limit
//
// The import exists so one generated C file carries every export: the three
// headers declare the same slice/struct typedefs and cannot be included in one
// translation unit. No policy lives in this file.
import expert_lru;
import dense_place;
import stream_plan;

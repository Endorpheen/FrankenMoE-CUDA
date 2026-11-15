// Staged S1.4: the host side of the Goose metrics/trace text encoders.
//
// The Goose module (goose/metrics/metrics-traces.goose) exports byte-level encoders over
// raw slices; the sinks here own std::string and FILE*. This header is the seam between
// the two: every function returns the encoded bytes in a std::string sized by the module,
// so no caller has to guess a buffer length and no output can be truncated.
//
// OFF build: only calls() exists (it returns 0), because the sinks never enter the bridge.
// ON build:  every encoder is defined here and forwards to the generated mt_* export.
//
// All encoders carry hidden visibility: they are internal to the library and must not
// enter its dynamic symbol table, which keeps the OFF and ON ABI byte-identical.
#pragma once

#include <cstdint>
#include <string>

#if defined(__GNUC__)
#define FGMT_INTERNAL __attribute__((visibility("hidden")))
#else
#define FGMT_INTERNAL
#endif

namespace fgmt {

// decode_trace_sink.cpp write_csv_field — the shared CSV field encoder.
FGMT_INTERNAL std::string field_encode(const std::string & v);

// decode_trace_sink.cpp CsvComputeTraceSink::on_rows — one nine-column compute row.
FGMT_INTERNAL std::string compute_row(int turn, int phase, int step, int seq, int layer, const std::string & op,
                                      const std::string & name, uint64_t wall_ns, uint64_t majflt);

// decode_trace_sink.cpp CsvIoTraceSink::on_rows — one twelve-column io row.
FGMT_INTERNAL std::string io_row(int turn, int phase, int step, int layer, int32_t expert, int8_t proj, int8_t lane,
                                 uint8_t spec, uint64_t offset, uint64_t req_bytes, uint64_t read_bytes,
                                 uint64_t latency_ns);

// route_trace_sink.cpp CsvRouteTraceSink::on_rows — one ten-column route row.
FGMT_INTERNAL std::string route_row(int turn, int phase, int step, int layer, int slot, int32_t expert,
                                    double weight, uint8_t residency, uint64_t expert_bytes, uint8_t dropped);

// How many times the selected path entered the Goose module. 0 in the OFF build.
// Exported on purpose: the gate reads it and the value is the same symbol in both builds,
// so the OFF and ON dynamic symbol tables stay identical.
int64_t calls();

} // namespace fgmt

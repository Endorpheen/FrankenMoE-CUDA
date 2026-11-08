#pragma once
/* worker-10: Apex CUDA movement/indexing/type-conversion/cache-write ABI.
 *
 * Goose owns every loop and index computation.  The primitives below are raw
 * device load/store, thread/block index, and float<->half conversions only.
 * The parameter block is a flat int64 table so the same generated Goose code
 * runs on the host differential oracle and on the CUDA device.
 */

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Parameter table layout (must match apex-move-bridge.h and Goose constants). */
#define FG153_AM_P_OP    0
#define FG153_AM_P_SRC   1
#define FG153_AM_P_DST   2
#define FG153_AM_P_IDX   3
#define FG153_AM_P_NE00  4
#define FG153_AM_P_NE01  5
#define FG153_AM_P_NE02  6
#define FG153_AM_P_NE03  7
#define FG153_AM_P_NB00  8
#define FG153_AM_P_NB01  9
#define FG153_AM_P_NB02 10
#define FG153_AM_P_NB03 11
#define FG153_AM_P_NE10 12
#define FG153_AM_P_NE11 13
#define FG153_AM_P_NE12 14
#define FG153_AM_P_NE13 15
#define FG153_AM_P_NB10 16
#define FG153_AM_P_NB11 17
#define FG153_AM_P_NB12 18
#define FG153_AM_P_NB13 19
#define FG153_AM_P_NB1  20
#define FG153_AM_P_NB2  21
#define FG153_AM_P_NB3  22
#define FG153_AM_P_ST   23
#define FG153_AM_P_DT   24
#define FG153_AM_P_IT   25
#define FG153_AM_P_N    26
#define FG153_AM_P_GRID_Y 27
#define FG153_AM_P_GRID_Z 28
#define FG153_AM_P_AUX0 29
#define FG153_AM_P_AUX1 30
#define FG153_AM_P_AUX2 31

typedef struct fg153_am_params {
    int64_t v[32];
} fg153_am_params;

/* Host/device operation codes. */
#define FG153_AM_OP_COPY_SCALAR 1
#define FG153_AM_OP_COPY_CONTIG 2
#define FG153_AM_OP_COPY_BLOCKS 3
#define FG153_AM_OP_TRANSPOSE   4
#define FG153_AM_OP_GETROWS     5
#define FG153_AM_OP_SETROWS     6
#define FG153_AM_OP_Q4_DEQUANT  7

#ifndef FG153_AM_DEVICE_PRIMITIVES
/* Operation markers: generated entry functions call exactly one. They are
 * witnesses for reachability and let the emitter adapter bind generated ids
 * without guessing function order. */
void fg153_am_enter_copy_scalar(void);
void fg153_am_enter_copy_contig(void);
void fg153_am_enter_copy_blocks(void);
void fg153_am_enter_transpose(void);
void fg153_am_enter_getrows(void);
void fg153_am_enter_setrows(void);
void fg153_am_enter_q4_dequant(void);

/* Typed primitives implemented by apex-move-services.c (host) and
 * apex-move-device-primitives.cuh (device). */
int64_t fg153_am_param(int64_t p, int64_t index);
int64_t fg153_am_load_u8(int64_t base, int64_t off);
int64_t fg153_am_load_u16(int64_t base, int64_t off);
int64_t fg153_am_load_i32(int64_t base, int64_t off);
int64_t fg153_am_load_i64(int64_t base, int64_t off);
void    fg153_am_store_u8(int64_t base, int64_t off, int64_t v);
void    fg153_am_store_u16(int64_t base, int64_t off, int64_t v);
void    fg153_am_store_i32(int64_t base, int64_t off, int64_t v);
void    fg153_am_store_f32(int64_t base, int64_t off, float v);
float   fg153_am_load_f32(int64_t base, int64_t off);
int64_t fg153_am_f16_bits(float v);
float   fg153_am_f16_value(int64_t bits);
float   fg153_am_fabs(float v);
int64_t fg153_am_trunc_i64(float v);

int64_t fg153_am_gid_x(void);
int64_t fg153_am_bid_x(void);
int64_t fg153_am_bid_y(void);
int64_t fg153_am_bid_z(void);
int64_t fg153_am_tid_x(void);
int64_t fg153_am_tid_y(void);
int64_t fg153_am_bdim_x(void);
int64_t fg153_am_gdim_y(void);
int64_t fg153_am_gdim_z(void);

#endif /* FG153_AM_DEVICE_PRIMITIVES */

/* Runtime gate: default-on, FG153_APEX_MOVE=0 is the rollback switch. */
int fg153_apex_move_enabled(void);

/* Host-side support/validation shared by the launcher and the fixtures. */
int fg153_am_can_run(int64_t op, const fg153_am_params * p);
int64_t fg153_am_witness_count(int64_t kind);

/* Call one generated entry. serial=1 selects the Goose range loop used by the
 * host differential oracle; serial=0 calls the device-shaped entry. */
int64_t fg153_am_call(int64_t kind, int64_t serial, int64_t params_ptr);

#ifdef __cplusplus
}
#endif

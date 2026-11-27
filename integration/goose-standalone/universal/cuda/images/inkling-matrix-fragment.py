from pathlib import Path
s=Path("inkling-matrix.c").read_text()
body=s.split("/* ---- prototypes ---- */",1)[1].split("/* ---- host ---- */",1)[0]
assert "gs_sp" not in body and "i2_matrix_eight_g" in body
types=s.split("/* ---- types ---- */",1)[1].split("/* ---- data ---- */",1)[0]
types="\n".join(line for line in types.splitlines() if not line.endswith(("gs_rhdr;","gs_rref;","gs_pref;","sl_u8;")))
Path("inkling-matrix-device.h").write_text(types+"\n"+body)

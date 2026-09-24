; An endless loop of 18 T-states. That does not divide the length of a frame,
; so the point at which a CPU instruction ends relative to the raster drifts
; from one frame to the next, as it does in any real program.

SECTION code_user

PUBLIC _spin

_spin:
    inc hl          ;  6 T
    jr _spin        ; 12 T

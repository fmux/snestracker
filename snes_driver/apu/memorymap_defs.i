.enum 0
SPC_DP0_SLOT db
SPC_DP1_SLOT db
SPC_RAM_SLOT db
SPC_CODE_SLOT db
.ende

.equ SPC_RAM_START $200
.equ SPC_RAM_SIZE  $200

.equ SPC_CODE_START (SPC_RAM_START + SPC_RAM_SIZE)
.equ SPC_CODE_SIZE ($10000 - SPC_CODE_START)

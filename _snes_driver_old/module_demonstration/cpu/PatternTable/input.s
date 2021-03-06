.include "cpu/memorymap.i"
.include "apu/enums.i"
;.include "cpu/PatternTable/enums.i"

.BANK 0
.orga $8000
.section "PatternTableInput" SEMIFREE

_AllTimeJoyCheck:
; Checks these no matter what
lda Joy1Press
bit #B_L
beq +
	; Send Stop Command to SPC
	jmp SPCStopSong
	
+	bit #B_R
beq +
	; Send Play Commdand to SPC
	; Reset the Pattern
	lda InputMode
	cmp #PatternListMode_Nav
	bne _save_values
	jml SPCPlaySong
_save_values:
	lda #PatternListMode_Nav
	sta InputMode
	jsr SaveCurrentEntryToSPC
	jml SPCPlaySong
+	rts


PatternTableInput:
	php
	rep #$10
	sep #$20
	
	jsr _AllTimeJoyCheck		; always do these checks
	
	; do other stuff here
	; like mode-based checks
	lda SNESbPlaySong
	bne _mainend
	
	lda InputMode
	cmp #PatternListMode_Nav
	bne _EditInput
	jmp _NavInput
	
_mainend:
	plp
	rts

_EditInput:
.index 16
.accu 8
	;php
	rep #$10
	sep #$20
	;do these checks if we are in MODE_NAV
	lda Joy1Press+1		; Up/Down
	ora Joy1Held3+1
	
	bit #B_UP
	beq _check_down
	sep #$10
	ldx SNESPatternList_CurrentEntry.w
	lda SNESPatternList.w,X
	rep #$10
	cmp #MAX_PATTERNS-1
	beq +
	ina
	sta SNESPatternList.w,X
	
	bra _next
+	stz SNESPatternList.w,X
	bra _next
_check_down:
	bit #B_DOWN
	beq _next
	sep #$10
	ldx SNESPatternList_CurrentEntry.w
	lda SNESPatternList.w,X
	rep #$10
	beq +
	dea
	sta SNESPatternList.w,X
	bra _next
+	lda #MAX_PATTERNS-1
	sta SNESPatternList.w,X
	
_next:
	
	lda Joy1Press+1
	bit #B_B
	beq _next1
	lda #0
	xba
	lda SNESPatternList_CurrentEntry.w
	tax
	lda PatternOldVal
	sta SNESPatternList.w,X
	pha
	rep #$20
	txa
	clc
	adc #SPCPatternList
	tax
	sep #$20
	pla
	jsr SPCWriteRamByte
	lda #PatternListMode_Nav
	sta InputMode
	bra _plprts
	
_next1:
+	lda Joy1Press
	bit #B_A
	beq +
	jsr SaveCurrentEntryToSPC
	lda #PatternListMode_Nav
	sta InputMode
	
_plprts:
	plp
	rts

_NavInput:
.index 16
.accu 8
	;php
	rep #$10
	sep #$20
	;do these checks if we are in MODE_NAV
	lda Joy1Press+1
	bit #B_UP
	beq _check_down2
	
	lda SNESPatternList_CurrentEntry.w
	beq _up_end2
	dec SNESPatternList_CurrentEntry.w
	lda SNESPatternList_CurrentEntry.w
	ldx #SPCPatternList_CurrentEntry.w
	jsr SPCWriteRamByte
	bra _up_end2
_up_end2:
	lda Joy1Press+1
_check_down2:
	bit #B_DOWN
	beq +
	;lda SNESPatternList_CurrentEntry.w
	lda SNESPatternListLength.w
	dea
	cmp SNESPatternList_CurrentEntry.w
	beq +
	inc SNESPatternList_CurrentEntry.w
	lda SNESPatternList_CurrentEntry.w
	ldx #SPCPatternList_CurrentEntry.w
	jsr SPCWriteRamByte

+	lda Joy1Press
	bit #B_A
	beq +
	lda #PatternListMode_Edit
	sta InputMode
	sep #$10
	ldx SNESPatternList_CurrentEntry.w
	lda SNESPatternList.w,X
	rep #$10
	sta PatternOldVal

+	plp
	rts

SaveCurrentEntryToSPC:
.accu 8
.index 16
	php
	rep #$10
	sep #$20
	lda #0
	xba
	lda SNESPatternList_CurrentEntry.w
	tax
	lda SNESPatternList.w,X
	pha
	rep #$20
	txa
	clc
	adc #SPCPatternList
	tax
	sep #$20
	pla
	jsr SPCWriteRamByte
	plp
	rts



.ends

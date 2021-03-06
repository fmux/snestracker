.include "header.i"

.define UnderlineTileNum 2

.bank 0
.section "underline"

ClearUnderlineLocations:
	php
	rep #$30
	phd
	
	lda #String1.w
	tcd
	lda ($00)
	ldy #5
	pha
	and #$ff00
	lsr
	lsr
	lsr
	; that's the equiv. of 5 shifts left from LSB;
	sta temp.w
	pla
	and #$00ff
	clc
	adc temp.w
	; word address to byte address << 1
	asl
	tax
-	stz bg2copy.w,X
	inx
	inx
	dey
	bne -
	
	
	lda #String2.w
	tcd
	lda ($00)
	ldy #9
	
	pha
	and #$ff00
	lsr
	lsr
	lsr
	; that's the equiv. of 5 shifts left from LSB;
	sta temp.w
	pla
	and #$00ff
	clc
	adc temp.w
	; word address to byte address << 1
	asl
	tax
	
-	stz bg2copy.w,X
	inx
	inx
	dey
	bne -
	
	lda #String3.w
	tcd
	lda ($00)
	ldy #9
	pha
	and #$ff00
	lsr
	lsr
	lsr
	; that's the equiv. of 5 shifts left from LSB;
	sta temp.w
	pla
	and #$00ff
	clc
	adc temp.w
	; word address to byte address << 1
	asl
	tax
	
-	stz bg2copy.w,X
	inx
	inx
	dey
	bne -

	
	pld
	
	plp
	rts

; call with X/Y coord in A 16 bit
; A = YX
; Y = quanity
DisplayUnderline:
;expect rep #$30
.accu 16
.index 16
	php
	pha
	and #$ff00
	lsr
	lsr
	lsr
	; that's the equiv. of 5 shifts left from LSB;
	sta temp.w
	pla
	and #$00ff
	clc
	adc temp.w
	; word address to byte address << 1
	asl
	tax
	sep #$20
-	lda #UnderlineTileNum
	sta bg2copy.w,X
	lda #%00000100
	sta bg2copy.w+1,X
	inx
	inx
	dey
	bne -
	plp
	rts	


.ends
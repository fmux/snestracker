;============================================================================
; Includes
;============================================================================

;== Include MemoryMap, Vector Table, and HeaderInfo ==
.INCLUDE "header.inc"

;== Include SNES Initialization routines ==
.INCLUDE "InitSNES.asm"

; Graphics Routines
.include "LoadGraphics.asm"
.INCLUDE "2Input.asm"
.INCLUDE "Strings.asm"
.INCLUDE "QuickSetup.asm"


.ENUM $80	
	xpos		dw
	ypos		dw
	x_tile		dw
	x_remainder dw
	y_tile 		dw
	y_remainder dw
	temp 		dw
	old_vram_pos dw
	vram_plot_position dw
	x_pixel		dw
	x_tilesize	dw
	y_tilesize	dw
	cursor_speed	db
.ENDE

;============================================================================
; Main Code
;============================================================================

.BANK 0 SLOT 0
.ORG 0
.SECTION "MainCode"

Start:
    InitSNES    ; Clear registers, etc.

    ; Load Palette for our tiles
    LoadPalette pal, 0, 4

    ; Load Tile data to VRAM
    LoadBlockToVRAM tiles, $0000, tiles_end-tiles	; 2 tiles, 2bpp, = 32 bytes
	LoadBlockToVRAM map, $1000, map_end-map
	
	

   
    ; Setup Video modes and other stuff, then turn on the screen
    jsr SetupVideo
	
	
	
	
	
	.equ brightness_value $0a	
	.equ loop_value $ffff
	
	stz $2100
	ldy #loop_value
	ldx.w #showoff_logo
	stx $10
	
	;FadeIn 3
	lda #$01		; add value
	sta $01	
	lda #$0f		; value to compare for
	sta $02
	
	ldx #4			; loop for $04ffff
loop_back:
	dey
	bne loop_back
	dey
	dex
	cpx #0
	bne loop_back
	
	; restore initial loop values for loopback
	ldy #loop_value
	ldx #1
	
	; update brightness settings
	; $01,$02 control whether the value is added ($01 = 1, $02 = $0f) or subtracted ($01 = $FF, $02 = 0)
	lda brightness_value
	clc
	adc $01
	sta brightness_value
	sta $2100
	cmp $02
	beq indirect_jump
	bra loop_back
	
indirect_jump:
	jmp ($10)	; jump to address at $10
	
showoff_logo:	
	;Wait 4
	ldy #$ffff	; loop delay = #$04ffff
	ldx #$12
	; we really just wait for awhile that's all
wait_loop:
	dey
	bne wait_loop
	dey
	dex
	cpx #0
	bne wait_loop
; done waiting
	
	;FadeOut 3
	ldx.w #enter_tracker	; our destination after fadeout
	stx $10
	lda #$ff
	sta $01
	lda #$00
	sta $02
	ldy #$ffff
	ldx #1
	bra loop_back
	
enter_tracker:
	lda #$80
	sta $2100
	; Blank screen
	; clear memory we used
	ldx #$1000
	stx $2116
	lda #$80
	sta $2115
	ldy #0
	ldx #$400
-	sty $2118
	dex
	bne -
	
	ldx #$0000
	stx $2116
	lda #$80
	sta $2115
	ldy #0
	ldx #$1080/2
-	sty $2118
	dex
	bne -
	
	 Here we will check for SNESTracker First time Run?? By checking SRAM at Bank 70
	lda $700000 ; = First run? 
	bne NotFirstTime
	
	ldx #$ffff
	ldy #$ff
;-	
	dex
	cpx #0
	bne -
	dey
	cpy #0
	bne -
	
; We are HERE
	; VRAM got cleared, it's time to get our Main Menu loaded up
	
	
;
	
	


;============================================================================
; SetupVideo -- Sets up the video mode and tile-related registers
;----------------------------------------------------------------------------
; In: None
;----------------------------------------------------------------------------
; Out: None
;----------------------------------------------------------------------------
SetupVideo:
    lda #$00
    sta $2105           ; Set Video mode 0, 8x8 tiles, 4 color BG1/BG2/BG3/BG4

    lda #4<<2          ; Set BG1's Tile Map offset to $1000 (Word address)
    sta $2107           ; And the Tile Map size to 32x32

    stz $210B           ; Set BG1's Character VRAM offset to $0000 (word address)

    lda #$01            ; Enable BG1
    sta $212C

    lda #$FF
    sta $210E
    stz $210E

    rts
;============================================================================
; Call with Address stored in $00
PlayIntroSound:
	; loop until spc is ready
	REP #$20	; 16-bit akku
scr_checkready:
	lda #$BBAA
	cmp $2140
	bne scr_checkready
 
	SEP #$20	; 8-bit akku
 
	ldy #0
 
	lda #0
	xba
 
scr_firstblock:
 
	; load spx
	lda #$01
	sta $2141
	lda #$CC
	ldx.w spx_binary, y
	iny
	iny
	stx $2142
	sta $2140
 
scr_check2:
	cmp $2140
	; check for $CC
	bne scr_check2
 
 ; size bytes -> X
	ldx.w spx_binary, y
	iny
	iny
; byte to transfer
REDO:
	lda spx_binary, y
	iny
	
; store it with 0
	sta $2141
 
	lda #0
	sta $2140
	
	; check for 0
scr_check3:
	cmp $2140
	bne scr_check3
 
 ; decrement byte transfer count
	dex
 ; switch to our counter in a
	xba
 
scr_data_loop:
; load a byte
	lda spx_binary, y
	iny
	sta $2141
	xba			; counter
 	ina
	sta $2140
	
	;check port0
scr_check4:
	cmp $2140
	bne scr_check4
 
	xba			; data
 
	dex
	bne scr_data_loop
 
 
	ldx.w spx_binary, y
	iny
	iny
	cpx #0
	beq scr_terminate
 
 
 	; X has starting address
	lda #1	; non-zero
	sta $2141
	
	;Store it
	stx $2142
	
	
	
	; stx $2142	; address
	xba
	ina
scr_nz1:
	ina
	beq scr_nz1
 
	sta $2140
scr_check5:
	cmp $2140
	bne scr_check5
	
	and #0
	xba	; data
	
	ldx.w spx_binary, y
	iny
	iny
	
	bra REDO
 
scr_terminate:
 
	stz $2141
	ldx.w spx_binary, y
 
	stx $2142
	xba
	ina
scr_nz2:
	ina
	beq scr_nz2
 
	sta $2140
scr_check6:
	cmp $2140
	bne scr_check6
 
	; PROTOCOL COMPLETE.
 
	RTS
    
.ENDS

.SECTION "MusicData"

spx_binary: 
    .dw $0200, $c1 ;$41d+$71 ; Start Address, size_bytes(sample + Spc code)
    .INCBIN "spc.obj"
	.dw $1000, $27f0
	.INCBIN "piano.brr"
	.dw $4000, $1506
	.INCBIN "bass.brr"
    .dw $0000, $200	; 00 to finish transfer, then address to jump SPC700 too to begin code execution
.ENDS


;============================================================================
; Character Data
;============================================================================
.BANK 1 SLOT 0
.ORG 0
.SECTION "CharacterData3"

map:
	.incbin "gfx/mainlogo/mainlogo.map"
map_end:
tiles:
    .incbin "gfx/mainlogo/mainlogo.pic"
tiles_end:
pal:
	.incbin "gfx/mainlogo/mainlogo.clr"

.ENDS


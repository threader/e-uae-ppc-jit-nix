; Mandelbrot hardware banging
; Requires more processor power

	machine	68020

	incdir	includes:
	include	cinstr.i
	include	hardware/custom.i
	include hardware/cia.i

;------------------------
; Reset vectors
;------------------------
stackptr:	dc.l	$4000			;Initial value of the stack pointer
resetvector:	dc.l	$00f80000 + start	;Reset vector for starting, jump to ROM directly

;------------------------
; Constants
;------------------------
width:		equ	320
height:		equ	256
depth:		equ	8
width2:		equ	320
height2:	equ	256

realpart:	equ	-$100
imgpart:	equ	-$d00
strtint:	equ	$a00

picmod:		equ	40
zoom:		equ	$9

videomem:	equ	$5000				;chipmem address for screen
coppermem:	equ	videomem + (320*256)		;copper list memory
coplist_len:	equ	copper_list_end-copper_list-1	;length of copper list

;------------------------
; Main code
;------------------------

start:	move.l	#%1001,d0
	movec	d0,cacr				;enable instruction cache

	move.b	#1,$bfe201			;Set off KickROM overlay from Chip Ram
	move.b	#0,$bfe001

	move.l	#$dff000,a6
	move.w	#$7fff,intena(a6) 		;Disable interrupts
	move.w	#$7fff,intreq(a6)		;Remove all interrupt requests
	move.w	#7,fmode(a6)			;Set up magic FMODE register
	move.w	#%1000001110000000,dmacon(a6)	;Turn on Copper, Raster and Master DMA
	move.w	#%0000010000111111,dmacon(a6)	;Turn off audio, blitter, disk and sprite
	move.l	#coppermem,cop1lc(a6)		;Load Copper list

	move.l	#coppermem,a1			;Copper list copy to chip ram
	move.l	#coplist_len,d0
	lea	copper_list(pc),a0

cyc_copy_copper:
	move.b	(a0)+,(a1)+
	dbf	d0,cyc_copy_copper

	bsr.w	render_palette

	move.l	#videomem,d0			;Put plane pointers into the copper list
	move.l	#(screenpoint1-copper_list)+coppermem,a1
	moveq.l	#8,d1
cycpoint:
	move.w	d0,6(a1)
	swap	d0
	move.w	d0,2(a1)
	swap	d0
	add.l	#width*height/8,d0
	add.l	#screenpoint2-screenpoint1,a1
	dbf	d1,cycpoint

	move.l	#videomem,a0
	move.l	#height*width*depth/32-1,d0
clr:	move.l	#0,(a0)+
	dbf	d0,clr

	move.w	d0,copjmp1(a6)			;Enforce reload the Copper list address

	bsr.w	render_mandel

wait_idle:
	bra.b	wait_idle

;------------------------
; Subroutines
;------------------------

; *** Render mandelbrot set to the screen

render_mandel:
	move.l	#height2-1,d7
cyc_man1:
	move.l	#zoom,d1
	mulu	d7,d1
	add.l	#imgpart,d1
	move.l	#width2-1,d6
cyc_man2:
	move.l	#zoom,d0
	mulu	d6,d0
	add.l	#realpart,d0
	move.w	#strtint,d2
	moveq	#0,d3
	moveq	#0,d4
cyc_man3:
	move.w	d4,d5
	muls	d3,d4
	rol.l	#5,d4
	swap	d4
	add.w	d1,d4
	muls	d3,d3
	asl.l	#4,d3
	swap	d3
	muls	d5,d5
	asl.l	#4,d5
	swap	d5
	add.w	d0,d3
	sub.w	d5,d3
	move.w	d4,d5
	muls	d5,d5
	asl.l	#4,d5
	swap	d5
	move.w	d3,a3
	muls	d3,d3
	asl.l	#4,d3
	swap	d3
	add.w	d3,d5
	move.w	a3,d3
	cmp.w	#$4000,d5
	bhi.s	too_much
	dbf	d2,cyc_man3

too_much:
	bsr.s	plot

	dbf	d6,cyc_man2
	dbf	d7,cyc_man1
	rts

; *** Draw a plot to the screen, d2 - plot color, d6/d7 - coordinates

plot:	movem.l	d4-d7,-(sp)

	tst.b	d2
	beq.w	po8

	move.w	d7,d4

	asl.w	#5,d4	;mulu	#40,d7
	asl.w	#3,d7
	add.w	d4,d7

	move.w	d6,d5
	lsr.w	#3,d5
	add.w	d5,d7
	move.l	d7,a0
	add.l	#videomem,a0
	moveq	#7,d7
	and.b	#7,d6
	sub.b	d6,d7

	btst	#0,d2
	bne.s	po1
	bset	d7,(a0)
po1:	add.l	#width*height/8,a0
	btst	#1,d2
	bne.s	po2
	bset	d7,(a0)
po2:	add.l	#width*height/8,a0
	btst	#2,d2
	bne.s	po3
	bset	d7,(a0)
po3:	add.l	#width*height/8,a0
	btst	#3,d2
	bne.s	po4
	bset	d7,(a0)
po4:	add.l	#width*height/8,a0
	btst	#4,d2
	bne.s	po5
	bset	d7,(a0)
po5:	add.l	#width*height/8,a0
	btst	#5,d2
	bne.s	po6
	bset	d7,(a0)
po6:	add.l	#width*height/8,a0
	btst	#6,d2
	bne.s	po7
	bset	d7,(a0)
po7:	add.l	#width*height/8,a0
	btst	#7,d2
	bne.s	po8
	bset	d7,(a0)
po8:	movem.l	(sp)+,d4-d7
	rts

; *** Copper palette render: palette_data - palette data base address, a1 - Copper list target pointer

render_palette:
	lea	palette_data(pc),a0
	moveq	#0,d0
cyc_pal1:
	move.l	d0,d1
	swap	d1
	asr.l	#3,d1
	or.l	#$01060000,d1
	move.l	d1,(a1)+

	move.l	a0,a2
	move.l	d0,d2
	asl.l	#7,d2
	adda.l	d2,a2
	move.l	a2,a3
	move.l	#$01800000,d5

cyc_pal2:
	move.l	(a2)+,d2
	asr.l	#4,d2
	move.l	d2,d3
	and.l	#$f,d3
	asr.l	#4,d2
	move.l	d2,d4
	and.l	#$f0,d4
	or.l	d4,d3
	asr.l	#4,d2
	and.l	#$f00,d2
	or.l	d2,d3
	or.l	d5,d3
	move.l	d3,(a1)+
	add.l	#$00020000,d5
	cmp.l	#$01c00000,d5
	bne.s	cyc_pal2

	move.l	a3,a2
	move.l	#$01800000,d5
	or.l	#$200,d1
	move.l	d1,(a1)+

cyc_pal3:	move.l	(a2)+,d2
	move.l	d2,d3
	and.l	#$f,d3
	asr.l	#4,d2
	move.l	d2,d4
	and.l	#$f0,d4
	or.l	d4,d3
	asr.l	#4,d2
	and.l	#$f00,d2
	or.l	d2,d3
	or.l	d5,d3
	move.l	d3,(a1)+
	add.l	#$00020000,d5
	cmp.l	#$01c00000,d5
	bne.s	cyc_pal3

	addq.l	#1,d0
	cmp.l	#8,d0
	bne.w	cyc_pal1
	move.l	#$fffffffe,(a1)+

	rts	


iamalive:
	lea $dff000,a0	;Custom registers basereg
	moveq.l	#0,d1
cyc:	addq.l	#1,d1
	move.w	d1,color(a0)
	bra.b	cyc

;------------------------
; Copper list
;------------------------

copper_list:	CMove	$2c81,diwstrt
		CMove	$2cc1,diwstop
		CMove	$38,ddfstrt
		CMove	$d0,ddfstop
		CMove	-8,bpl1mod
		CMove	-8,bpl2mod
		CMove	%0000000000010000,bplcon0
screenpoint1:	CMove	0,bplpt
		CMove	0,bplpt+2
screenpoint2:	CMove	0,bplpt+4
		CMove	0,bplpt+6
screenpoint3:	CMove	0,bplpt+8
		CMove	0,bplpt+10
screenpoint4:	CMove	0,bplpt+12
		CMove	0,bplpt+14
screenpoint5:	CMove	0,bplpt+16
		CMove	0,bplpt+18
screenpoint6:	CMove	0,bplpt+20
		CMove	0,bplpt+22
screenpoint7:	CMove	0,bplpt+24
		CMove	0,bplpt+26
screenpoint8:	CMove	0,bplpt+28
		CMove	0,bplpt+30
copper_list_end:	dc.l	0
		
	EVEN

;------------------------
; Palette data for nice colors
;------------------------
palette_data:
	DC.B	$00,$FF,$AA,$F3,$00,$FF,$AA,$E6
	DC.B	$00,$FF,$AA,$D9,$00,$FF,$AA,$CC
	DC.B	$00,$FF,$95,$D4,$00,$FF,$80,$E5
	DC.B	$00,$FF,$6B,$FF,$00,$DD,$55,$FF
	DC.B	$00,$B2,$40,$FF,$00,$7F,$2B,$FF
	DC.B	$00,$44,$16,$FF,$00,$00,$00,$FF
	DC.B	$00,$13,$00,$FF,$00,$26,$00,$FF
	DC.B	$00,$39,$00,$FF,$00,$4C,$00,$FF
	DC.B	$00,$5F,$00,$FF,$00,$72,$00,$FF
	DC.B	$00,$85,$00,$FF,$00,$99,$00,$FF
	DC.B	$00,$AC,$00,$FF,$00,$BF,$00,$FF
	DC.B	$00,$D2,$00,$FF,$00,$E5,$00,$FF
	DC.B	$00,$F8,$00,$FF,$00,$FF,$00,$F3
	DC.B	$00,$FF,$00,$E0,$00,$FF,$00,$CC
	DC.B	$00,$FF,$00,$B9,$00,$FF,$00,$A6
	DC.B	$00,$FF,$00,$93,$00,$FF,$00,$80
	DC.B	$00,$FF,$00,$6D,$00,$FF,$00,$5A
	DC.B	$00,$FF,$00,$47,$00,$FF,$00,$33
	DC.B	$00,$FF,$00,$20,$00,$FF,$00,$0D
	DC.B	$00,$FF,$06,$00,$00,$FF,$19,$00
	DC.B	$00,$FF,$2C,$00,$00,$FF,$3F,$00
	DC.B	$00,$FF,$52,$00,$00,$FF,$66,$01
	DC.B	$00,$FF,$79,$01,$00,$FF,$8C,$01
	DC.B	$00,$FF,$9F,$01,$00,$FF,$B2,$01
	DC.B	$00,$FF,$C5,$01,$00,$FF,$D8,$01
	DC.B	$00,$FF,$EB,$01,$00,$FF,$FF,$02
	DC.B	$00,$F6,$FF,$02,$00,$ED,$FF,$02
	DC.B	$00,$E4,$FF,$02,$00,$DB,$FF,$02
	DC.B	$00,$D2,$FF,$02,$00,$C9,$FF,$02
	DC.B	$00,$C0,$FF,$02,$00,$B7,$FF,$02
	DC.B	$00,$AE,$FF,$02,$00,$A4,$FF,$02
	DC.B	$00,$9B,$FF,$02,$00,$92,$FF,$02
	DC.B	$00,$89,$FF,$02,$00,$80,$FF,$01
	DC.B	$00,$77,$FF,$01,$00,$6E,$FF,$01
	DC.B	$00,$65,$FF,$01,$00,$5C,$FF,$01
	DC.B	$00,$52,$FF,$01,$00,$49,$FF,$01
	DC.B	$00,$40,$FF,$01,$00,$37,$FF,$01
	DC.B	$00,$2E,$FF,$01,$00,$25,$FF,$01
	DC.B	$00,$1C,$FF,$01,$00,$13,$FF,$01
	DC.B	$00,$0A,$FF,$01,$00,$02,$FE,$09
	DC.B	$00,$04,$FC,$1D,$00,$05,$FA,$30
	DC.B	$00,$07,$F8,$42,$00,$08,$F6,$53
	DC.B	$00,$0A,$F4,$64,$00,$0B,$F2,$75
	DC.B	$00,$0D,$F0,$85,$00,$0E,$EE,$94
	DC.B	$00,$0F,$EC,$A3,$00,$10,$EA,$B1
	DC.B	$00,$12,$E8,$BF,$00,$13,$E6,$CD
	DC.B	$00,$14,$E4,$DA,$00,$15,$DD,$E2
	DC.B	$00,$16,$CC,$E0,$00,$17,$BD,$DE
	DC.B	$00,$19,$AE,$DC,$00,$19,$9E,$DA
	DC.B	$00,$1B,$90,$D8,$00,$1B,$82,$D6
	DC.B	$00,$1C,$A0,$D7,$00,$1C,$C0,$D8
	DC.B	$00,$1B,$DA,$D2,$00,$1A,$DB,$B4
	DC.B	$00,$1A,$DC,$94,$00,$1A,$DE,$76
	DC.B	$00,$19,$DF,$56,$00,$18,$E0,$34
	DC.B	$00,$1B,$E2,$18,$00,$3B,$E3,$18
	DC.B	$00,$5D,$E4,$17,$00,$7F,$E6,$16
	DC.B	$00,$A1,$E7,$15,$00,$C4,$E8,$15
	DC.B	$00,$EA,$EA,$15,$00,$EB,$C8,$14
	DC.B	$00,$EC,$A5,$13,$00,$EC,$85,$14
	DC.B	$00,$ED,$65,$14,$00,$EE,$46,$14
	DC.B	$00,$EF,$24,$13,$00,$F0,$13,$20
	DC.B	$00,$F1,$13,$42,$00,$F2,$13,$62
	DC.B	$00,$F3,$13,$83,$00,$F4,$13,$A5
	DC.B	$00,$F5,$13,$C8,$00,$F6,$13,$E9
	DC.B	$00,$E1,$12,$F7,$00,$C0,$12,$F8
	DC.B	$00,$9F,$12,$F9,$00,$7D,$10,$FA
	DC.B	$00,$67,$10,$FA,$00,$4F,$0F,$FA
	DC.B	$00,$39,$0E,$FA,$00,$21,$0D,$FB
	DC.B	$00,$0C,$0C,$FB,$00,$0B,$21,$FB
	DC.B	$00,$0A,$38,$FC,$00,$09,$4E,$FC
	DC.B	$00,$08,$63,$FC,$00,$07,$7A,$FD
	DC.B	$00,$06,$90,$FD,$00,$05,$A7,$FD
	DC.B	$00,$04,$C0,$FE,$00,$03,$D7,$FE
	DC.B	$00,$02,$EF,$FE,$00,$00,$FF,$F6
	DC.B	$00,$00,$E8,$FF,$00,$0B,$D4,$FF
	DC.B	$00,$16,$C3,$FF,$00,$22,$B3,$FF
	DC.B	$00,$2D,$A6,$FF,$00,$38,$99,$FF
	DC.B	$00,$44,$90,$FF,$00,$4F,$87,$FF
	DC.B	$00,$5A,$81,$FF,$00,$66,$7D,$FF
	DC.B	$00,$71,$7B,$FF,$00,$7E,$7C,$FF
	DC.B	$00,$93,$88,$FF,$00,$A6,$93,$FF
	DC.B	$00,$B7,$9E,$FF,$00,$C8,$AA,$FF
	DC.B	$00,$CF,$A3,$FD,$00,$D7,$9B,$FA
	DC.B	$00,$E2,$93,$F8,$00,$ED,$8C,$F5
	DC.B	$00,$F2,$83,$E9,$00,$F0,$7D,$D7
	DC.B	$00,$ED,$75,$C1,$00,$EA,$6E,$AA
	DC.B	$00,$E8,$66,$92,$00,$E5,$60,$7A
	DC.B	$00,$E2,$58,$5F,$00,$E0,$5E,$52
	DC.B	$00,$DD,$6C,$4B,$00,$DA,$7D,$45
	DC.B	$00,$D8,$8D,$3E,$00,$D5,$A0,$38
	DC.B	$00,$D2,$B4,$31,$00,$D2,$B4,$35
	DC.B	$00,$D3,$B6,$39,$00,$D4,$B7,$3C
	DC.B	$00,$D5,$B8,$40,$00,$D6,$BA,$44
	DC.B	$00,$D7,$BB,$47,$00,$D8,$BD,$4C
	DC.B	$00,$D9,$BF,$50,$00,$DA,$C0,$53
	DC.B	$00,$DB,$C1,$57,$00,$DC,$C3,$5B
	DC.B	$00,$DD,$C5,$5F,$00,$DE,$C7,$63
	DC.B	$00,$DF,$C9,$68,$00,$E0,$CA,$6C
	DC.B	$00,$E1,$CC,$70,$00,$E2,$CD,$74
	DC.B	$00,$E3,$CF,$78,$00,$E4,$D0,$7C
	DC.B	$00,$E5,$D1,$80,$00,$E6,$D3,$85
	DC.B	$00,$E7,$D5,$89,$00,$E8,$D7,$8E
	DC.B	$00,$E8,$D7,$91,$00,$E9,$D9,$95
	DC.B	$00,$EA,$DB,$9A,$00,$EB,$DC,$9F
	DC.B	$00,$EC,$DE,$A3,$00,$ED,$E0,$A8
	DC.B	$00,$EE,$E1,$AC,$00,$EF,$E3,$B1
	DC.B	$00,$F0,$E4,$B5,$00,$F1,$E7,$BA
	DC.B	$00,$F2,$E8,$BF,$00,$F3,$EA,$C4
	DC.B	$00,$F4,$EB,$C8,$00,$F5,$ED,$CD
	DC.B	$00,$F6,$EF,$D2,$00,$F7,$F1,$D7
	DC.B	$00,$F8,$F2,$DB,$00,$F9,$F4,$E1
	DC.B	$00,$FA,$F6,$E6,$00,$FB,$F8,$EB
	DC.B	$00,$FC,$FA,$F0,$00,$FD,$FC,$F5
	DC.B	$00,$FE,$FE,$FA,$00,$FF,$FF,$FF
	DC.B	$00,$00,$00,$02,$00,$FF,$AA,$AA
	DC.B	$00,$FF,$BB,$AA,$00,$FF,$C6,$AA
	DC.B	$00,$FF,$D1,$AA,$00,$FF,$DD,$AA
	DC.B	$00,$FF,$E8,$AA,$00,$FF,$F3,$AA
	DC.B	$00,$FF,$FF,$AA,$00,$EE,$FF,$AA
	DC.B	$00,$DD,$FF,$AA,$00,$CC,$FF,$AA
	DC.B	$00,$BB,$FF,$AA,$00,$AA,$FF,$AA
	DC.B	$00,$AA,$FF,$BB,$00,$AA,$FF,$D0
	DC.B	$00,$AA,$FF,$E5,$00,$AA,$FF,$FA
	DC.B	$00,$AA,$EE,$FF,$00,$AA,$D9,$FF
	DC.B	$00,$AA,$C3,$FF,$00,$AA,$AE,$FF
	DC.B	$00,$BB,$AA,$FF,$00,$C8,$AA,$FF
	DC.B	$00,$D5,$AA,$FF,$00,$E2,$AA,$FF
	DC.B	$00,$EF,$AA,$FF,$00,$FD,$AA,$FF

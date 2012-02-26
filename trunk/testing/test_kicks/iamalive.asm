;"I am alive" E-UAE test
;When the machine starts it shows some color bars crawling on the screen

	machine	68020
	incdir	includes:
	include	hardware/custom.i

stackptr:	dc.l	$4000	;Initial value of the stack pointer
reset_vector:	dc.l	start	;Reset vector address

start:	move.l	#%1001,d0
	movec	d0,cacr		;enable instruction cache

	lea	$dff000,a0	;Custom registers basereg
	moveq.l	#0,d1
cyc:	addq.l	#1,d1
	move.w	d1,color(a0)	;Set color register
	bra.b	cyc

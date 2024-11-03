	AREA CLZ_Code, CODE
	EXPORT __main

__main
	MOV R0, #0x000F0000    ; load value to r0 reg
	CLZ R1, R0             ; count leading zeros in r0 reg and store result in r1 reg
	END
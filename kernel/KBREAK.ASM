	.model small, c
	.code
kbreak	proc	near
	int	3
	ret
kbreak	endp

	end


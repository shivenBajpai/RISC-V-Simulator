main:
    lui gp, 0x10
    lui sp, 0x50
    
    addi x10, x0, 7
    jal x1, fib
    beq x0, x0, 0

fib:
    addi t0, x0, 2
    blt x10, t0, Exit2
    
    addi sp, sp, -16
    sd x1, 8(sp)
    sd x10, 0(sp) # Save func args
    
    addi x10, x10, -2 # Call fib(n-2)
    jal x1, fib
    
    ld t0, 0(sp) # Restore function argument into temporary variable
    sd x10, 0(sp) # Reuse that space to store result of fib(n-2)
    
    addi x10, t0, -1
    jal x1, fib # Call fib (n-1)
    
    ld t0, 0(sp) # Restore t0
    add x10, x10, t0 # Sum of the two fib calls
    ld x1, 8(sp)
    addi sp, sp, 16
    jalr x0, 0(x1)
    
Exit2: 
    jalr x0, 0(x1) # Returning whatever the input was
# Virtual Memory Unit (MMU)
This is a C program that translates logical addresses to physical addresses.  It resolves page faults using demand paging and manages a TLB. This program was written for my Operating Systems course.

# Usage
There are two ways of using the program.

1. You can use the test script (./test.sh), and it will run using the pagetable parameter and output the stdout to "out.txt", and also create the output.csv
2. By compiling, and then running the program with the backing store file as the first input, and address file as the second input. So:
	gcc mmu.c
	./a.out BACKING_STORE.bin addresses.txt

The program will output the 3 columns (logical address, corresponding physical address, signed byte value) to CSV file titled "output.csv"

# Parallel Zip (pzip)

## overview


`pzip` is a parallel zip utility that compresses files using run-length encoding and pthreads for parallel processing.


## features
- Compresses multiple files in parallel.
- Uses run-length encoding for compression.
- Handles large files efficiently.


## running
To compile the program:

```bash
gcc -pthread pzip.c -o pzip

./pzip <file1> <file2> ... <fileN>

example with text files test1,2,3
./pzip test1.txt test2.txt test3.txt

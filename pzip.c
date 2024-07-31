#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>

//structure to hold the arguments for each thread
typedef struct {
    char *data;
    size_t start;
    size_t end;
    FILE *output;
} thread_args_t;

//a mutex for thread-safe writing (GPT helped)
pthread_mutex_t write_mutex;

//Function for run-length encoding
void *run_length_encode(void *args) {
    thread_args_t *targs = (thread_args_t *)args;
    char *data = targs->data;
    size_t start = targs->start;
    size_t end = targs->end;

    //Buffer
    size_t buffer_size = (end - start) * (sizeof(size_t) + sizeof(char));
    char *buffer = malloc(buffer_size);
    if (buffer == NULL) {
        fprintf(stderr, "malloc failed\n");
        exit(1);
    }
    char *buf_ptr = buffer;

    //Perform rle
    for (size_t i = start; i < end;) {
        char current = data[i];
        size_t count = 1;
        while (i + count < end && data[i + count] == current) {
            count++;
        }
        memcpy(buf_ptr, &count, sizeof(size_t));
        buf_ptr += sizeof(size_t);
        memcpy(buf_ptr, &current, sizeof(char));
        buf_ptr += sizeof(char);
        i += count;
    }

    //to the output file in a thread-safe manner
    pthread_mutex_lock(&write_mutex);
    size_t bytes_to_write = buf_ptr - buffer;
    fwrite(buffer, 1, bytes_to_write, targs->output);
    pthread_mutex_unlock(&write_mutex);

    free(buffer);
    return NULL;
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "usage: pzip <file1> <file2> ... <fileN>\n");
        exit(1);
    }

    //Initialize mutex
    if (pthread_mutex_init(&write_mutex, NULL) != 0) {
        fprintf(stderr, "mutex init failed\n");
        exit(1);
    }

    //the number of available processors
    int num_processors = sysconf(_SC_NPROCESSORS_ONLN);
    pthread_t threads[num_processors];
    thread_args_t targs[num_processors];

    for (int file_idx = 1; file_idx < argc; file_idx++) {
        //Open the input file
        int fd = open(argv[file_idx], O_RDONLY);
        if (fd == -1) {
            fprintf(stderr, "error: cannot open file '%s'\n", argv[file_idx]);
            exit(1);
        }

        //The size of the file
        struct stat st;
        if (fstat(fd, &st) == -1) {
            fprintf(stderr, "error: cannot stat file '%s'\n", argv[file_idx]);
            exit(1);
        }

        //Memory-mapping the file
        char *data = mmap(NULL, st.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
        if (data == MAP_FAILED) {
            fprintf(stderr, "error: cannot mmap file '%s'\n", argv[file_idx]);
            exit(1);
        }

        close(fd);

        //Open the output file
        char output_filename[256];
        snprintf(output_filename, sizeof(output_filename), "%s.z", argv[file_idx]);
        FILE *output = fopen(output_filename, "w");
        if (output == NULL) {
            fprintf(stderr, "error: cannot open output file '%s'\n", output_filename);
            exit(1);
        }

        //Divide the work among threads
        size_t chunk_size = (st.st_size + num_processors - 1) / num_processors; //Ensure all data is processed
        for (int i = 0; i < num_processors; i++) {
            targs[i].data = data;
            targs[i].start = i * chunk_size;
            targs[i].end = (i == num_processors - 1) ? st.st_size : (i + 1) * chunk_size;
            targs[i].output = output;
            if (targs[i].start >= targs[i].end) {
                //Skip invalid ranges
                continue;
            }
            pthread_create(&threads[i], NULL, run_length_encode, &targs[i]);
        }

        //Wait for all threads to complete
        for (int i = 0; i < num_processors; i++) {
            if (targs[i].start >= targs[i].end) {
                //skipping invalid ranges
                continue;
            }
            pthread_join(threads[i], NULL);
        }

        fclose(output);
        munmap(data, st.st_size);
    }

    //Destroy the mutex
    pthread_mutex_destroy(&write_mutex);

    return 0;
}

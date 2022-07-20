/*
    The sorting program to use for Operating Systems Assignment 1 2020
    written by Robert Sheehan

    Modified by: Martin Tiangco
    UPI: mtia116

    By submitting a program you are claiming that you and only you have made
    adjustments and additions to this code.
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/resource.h>
#include <stdbool.h>
#include <sys/times.h>
#include <pthread.h>

#define SIZE 10

static pthread_t thread;
static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t cond = PTHREAD_COND_INITIALIZER;
static int isBusy = 1;             // condition for the second thread being busy
static int isMainComplete = 0;     // condition of the main thread being complete
static struct block unsorted_side; // shared side the second thread sorts
static int count = 0;              // to see how many times the second thread is called

struct block
{
    int size;
    int *data;
};

void print_data(struct block my_data)
{
    for (int i = 0; i < my_data.size; ++i)
        printf("%d ", my_data.data[i]);
    printf("\n");
}

/* Split the shared array around the pivot, return pivot index. */
int split_on_pivot(struct block my_data)
{
    int right = my_data.size - 1;
    int left = 0;
    int pivot = my_data.data[right];
    while (left < right)
    {
        int value = my_data.data[right - 1];
        if (value > pivot)
        {
            my_data.data[right--] = value;
        }
        else
        {
            my_data.data[right - 1] = my_data.data[left];
            my_data.data[left++] = value;
        }
    }
    my_data.data[right] = pivot;
    return right;
}

void quick_sort(struct block my_data);

/* Quick sort on the second thread */
void *quick_sort_thread(void *args)
{
    // first run through
    struct block *my_data = args;
    if (my_data->size < 2)
    {
        return 0;
    }

    quick_sort(*my_data);
    // end of first run through

    pthread_mutex_lock(&lock);
    isBusy = 0;                 // second thread is not working
    while (isMainComplete == 0) // once main is complete, can exit the loop now
    {
        pthread_cond_wait(&cond, &lock);
        count++;
        quick_sort(unsorted_side);

        isBusy = 0; // thread has now sorted both of its sides, now is no longer busy
    }
    pthread_mutex_unlock(&lock);
}

// 1. Main thread checks to see if teh other thread is busy. If not busy, then the main thread should //SIGNAL// to the
// second thread to continue sorting on the data.
// 2. Second thread must //WAIT// on a condition variable.
void quick_sort_main(struct block my_data)
{
    if (my_data.size < 2)
        return;
    int pivot_pos = split_on_pivot(my_data);

    struct block left_side, right_side;
    left_side.size = pivot_pos;
    left_side.data = my_data.data;

    right_side.size = my_data.size - pivot_pos - 1;
    right_side.data = my_data.data + pivot_pos + 1;

    // if second thread is not busy, we will send a signal for it to sort
    if (!isBusy)
    {
        // shares the unsorted left side with the second thread
        pthread_mutex_lock(&lock);
        isBusy = 1;
        unsorted_side = left_side;
        pthread_mutex_unlock(&lock);
        pthread_cond_signal(&cond);

        // sorts right side on the main thread
        quick_sort_main(right_side);
    }
    else
    {
        // otherwise the main thread just sorts both sides
        quick_sort_main(left_side);
        quick_sort_main(right_side);
    }
}

// Called by the second thread recursively
void quick_sort(struct block my_data)
{
    if (my_data.size < 2)
        return;
    int pivot_pos = split_on_pivot(my_data);

    struct block left_side, right_side;

    left_side.size = pivot_pos;
    left_side.data = my_data.data;
    right_side.size = my_data.size - pivot_pos - 1;
    right_side.data = my_data.data + pivot_pos + 1;

    quick_sort(left_side);
    quick_sort(right_side);
}

/* Check to see if the data is sorted. */
bool is_sorted(struct block my_data)
{
    bool sorted = true;
    for (int i = 0; i < my_data.size - 1; i++)
    {
        if (my_data.data[i] > my_data.data[i + 1])
            sorted = false;
    }
    return sorted;
}

/* Fill the array with random data. */
void produce_random_data(struct block my_data)
{
    srand(1); // the same random data seed every time
    for (int i = 0; i < my_data.size; i++)
    {
        my_data.data[i] = rand() % 1000;
    }
}

int main(int argc, char *argv[])
{
    long size;

    if (argc < 2)
    {
        size = SIZE;
    }
    else
    {
        size = atol(argv[1]);
    }
    struct block start_block;
    start_block.size = size;
    start_block.data = (int *)calloc(size, sizeof(int));
    if (start_block.data == NULL)
    {
        printf("Problem allocating memory.\n");
        exit(EXIT_FAILURE);
    }

    produce_random_data(start_block);

    if (start_block.size < 1001)
        print_data(start_block);

    struct tms start_times, finish_times;
    times(&start_times);
    printf("start time in clock ticks: %ld\n", start_times.tms_utime);

    // ======================
    // First run of quick_sort

    if (start_block.size < 2)
        return 0;
    int pivot_pos = split_on_pivot(start_block);

    struct block left_side, right_side;

    left_side.size = pivot_pos;
    left_side.data = start_block.data;
    right_side.size = start_block.size - pivot_pos - 1;
    right_side.data = start_block.data + pivot_pos + 1;

    pthread_create(&thread, NULL, quick_sort_thread, &left_side); // creates the second thread
    quick_sort_main(right_side);                                  // quick_sorts on the main thread
    isMainComplete = 1;                                           // main is complete at this point

    pthread_join(thread, NULL);
    // =====================

    times(&finish_times);
    printf("finish time in clock ticks: %ld\n", finish_times.tms_utime);

    if (start_block.size < 1001)
        print_data(start_block);

    printf(is_sorted(start_block) ? "sorted\n" : "not sorted\n");
    printf("The second thread is called %d times", count);
    free(start_block.data);

    exit(EXIT_SUCCESS);
}
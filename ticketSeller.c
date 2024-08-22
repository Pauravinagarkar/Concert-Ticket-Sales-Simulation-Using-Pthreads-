#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <string.h>
#include <pthread.h>
#include "utility.h"

// Define constants
#define HIGH_SELLERS 1
#define MEDIUM_SELLERS 3
#define LOW_SELLERS 6
#define TOTAL_SELLERS (HIGH_SELLERS + MEDIUM_SELLERS + LOW_SELLERS)
#define CONCERT_ROWS 10
#define CONCERT_COLS 10
#define TOTAL_SIMULATION_DURATION 60

// Structure for Seller
typedef struct Seller {
    char seller_number;
    char seller_type;
    queue *seller_queue;
} Seller;

// Structure for Customer
typedef struct Customer {
    char customer_number;
    int arrival_time;
    int response_time;
    int turnaround_time;
} Customer;

// Global variables
int current_time_slice;
int total_customers = 0;
char available_seats_matrix[CONCERT_ROWS][CONCERT_COLS][5];
int response_time_for_high;
int response_time_for_medium;
int response_time_for_low;
int turnaround_time_for_high;
int turnaround_time_for_medium;
int turnaround_time_for_low;

// Thread variables
pthread_t seller_threads[TOTAL_SELLERS];
pthread_mutex_t thread_count_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t thread_waiting_for_clock_tick_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t reservation_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t thread_completion_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t condition_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t condition_cond = PTHREAD_COND_INITIALIZER;

// Function declarations
void print_customer_queue(queue *q);
void create_seller_threads(pthread_t *threads, char seller_type, int num_sellers);
void wait_for_threads_to_complete_current_time_slice();
void notify_all_seller_threads();
void *sell(void *);
queue *generate_customer_queue(int);
int find_available_seat(char seller_type);
int thread_count = 0;
int threads_waiting_for_clock_tick = 0;
int active_threads = 0;
int verbose = 0;
int compare_by_arrival_time(void *value1, void *value2);

int main(int argc, char **argv) {
    // Command line argument for the total number of customers
    if (argc == 2) {
        total_customers = atoi(argv[1]);
    }

    // Initialize the matrix for available seats
    for (int r = 0; r < CONCERT_ROWS; r++) {
        for (int c = 0; c < CONCERT_COLS; c++) {
            strncpy(available_seats_matrix[r][c], "-", 1);
        }
    }

    // Create seller threads for high, medium, and low types
    create_seller_threads(seller_threads, 'H', HIGH_SELLERS);
    create_seller_threads(seller_threads + HIGH_SELLERS, 'M', MEDIUM_SELLERS);
    create_seller_threads(seller_threads + HIGH_SELLERS + MEDIUM_SELLERS, 'L', LOW_SELLERS);

    // Wait for threads to finish initialization and synchronize with clock tick
    while (1) {
        pthread_mutex_lock(&thread_count_mutex);
        if (thread_count == 0) {
            pthread_mutex_unlock(&thread_count_mutex);
            break;
        }
        pthread_mutex_unlock(&thread_count_mutex);
    }

    // Simulate each time quanta/slice as one iteration
    printf("\nThread simulation begins:");
    printf("\n*--------------------------------------------------------------------------------------------*\n");
    printf("Time   |S   |Activity                      |Response Time    |Turnaround Time");
    printf("\n*--------------------------------------------------------------------------------------------*\n");
    threads_waiting_for_clock_tick = 0;
    notify_all_seller_threads(); // For the first tick

    do {
        // Wake up all threads
        wait_for_threads_to_complete_current_time_slice();
        current_time_slice += 1;
        notify_all_seller_threads();
        // Wait for thread completion
    } while (current_time_slice < TOTAL_SIMULATION_DURATION);

    // Wake up all threads so that num more threads keep waiting for the clock tick in limbo
    notify_all_seller_threads();

    while (active_threads);

    // Display concert chart
    printf("\n\n");
    printf("Final Concert Seating Chart");
    printf("\n************************************************************************************\n\n");

    int high_customers = 0, medium_customers = 0, low_customers = 0;
    for (int r = 0; r < CONCERT_ROWS; r++) {
        for (int c = 0; c < CONCERT_COLS; c++) {
            if (c != 0)
                printf("\t");
            printf("%5s", available_seats_matrix[r][c]);
            if (available_seats_matrix[r][c][0] == 'H')
                high_customers++;
            if (available_seats_matrix[r][c][0] == 'M')
                medium_customers++;
            if (available_seats_matrix[r][c][0] == 'L')
                low_customers++;
        }
        printf("\n");
    }
    printf("\n************************************************************************************\n\n");
    // Display statistics
    printf("\n\n######################################");
    printf("\nMulti-threaded Ticket Sellers");
    printf("\nTotal Customers = %02d\n", total_customers);
    printf("######################################\n\n");

    printf("*----------------------------------------------------------*\n");
    printf("|%2c| Number of Customers | Got Seat | Returned | Throughput|\n", 'S');
    printf("----------------------------------------------------------\n");
    printf("|%2c| %19d | %8d | %8d | %.2f      |\n", 'H', HIGH_SELLERS * total_customers, high_customers,
           (HIGH_SELLERS * total_customers) - high_customers, (high_customers / 60.0));
    printf("|%2c| %19d | %8d | %8d | %.2f      |\n", 'M', MEDIUM_SELLERS * total_customers, medium_customers,
           (MEDIUM_SELLERS * total_customers) - medium_customers, (medium_customers / 60.0));
    printf("|%2c| %19d | %8d | %8d | %.2f      |\n", 'L', LOW_SELLERS * total_customers, low_customers,
           (LOW_SELLERS * total_customers) - low_customers, (low_customers / 60.0));
    printf("*----------------------------------------------------------*\n");
    printf("*----------------------------------------------------------*\n");
    printf("\n");

    printf("*----------------------------------------------------------*\n");
    printf("|%2c| Average Response Time | Average Turnaround Time|\n", 'S');
    printf("*----------------------------------------------------------*\n");
    printf("|%2c| %22f| %23f|\n", 'H', response_time_for_high / (HIGH_SELLERS * total_customers * 1.0),
           turnaround_time_for_high / (HIGH_SELLERS * total_customers * 1.0));
    printf("|%2c| %22f| %23f|\n", 'M', response_time_for_medium / (MEDIUM_SELLERS * total_customers * 1.0),
           turnaround_time_for_medium / (MEDIUM_SELLERS * total_customers * 1.0));
    printf("|%2c| %22f| %23f%|\n", 'L', response_time_for_low / (LOW_SELLERS * total_customers * 1.0),
           turnaround_time_for_low / (LOW_SELLERS * total_customers * 1.0));
    printf("*----------------------------------------------------------*\n");
    return 0;
}

// Create threads for each seller type
void create_seller_threads(pthread_t *threads, char seller_type, int num_sellers) {
    for (int thread_no = 0; thread_no < num_sellers; thread_no++) {
        Seller *seller_param = (Seller *)malloc(sizeof(Seller));
        seller_param->seller_number = thread_no;
        seller_param->seller_type = seller_type;
        seller_param->seller_queue = generate_customer_queue(total_customers);

        pthread_mutex_lock(&thread_count_mutex);
        thread_count++;
        pthread_mutex_unlock(&thread_count_mutex);

        if (verbose)
            printf("Creating thread %c%02d\n", seller_type, thread_no);

        pthread_create(threads + thread_no, NULL, &sell, seller_param);
    }
}

// Print the customer queue for debugging
void print_customer_queue(queue *q) {
    for (node *ptr = q->head; ptr != NULL; ptr = ptr->next) {
        Customer *cust = (Customer *)ptr->value;
        printf("[%d,%d]", cust->customer_number, cust->arrival_time);
    }
}

// Wait for all threads to complete the current time slice
void wait_for_threads_to_complete_current_time_slice() {
    while (1) {
        pthread_mutex_lock(&thread_waiting_for_clock_tick_mutex);
        if (threads_waiting_for_clock_tick == active_threads) {
            threads_waiting_for_clock_tick = 0;
            pthread_mutex_unlock(&thread_waiting_for_clock_tick_mutex);
            break;
        }
        pthread_mutex_unlock(&thread_waiting_for_clock_tick_mutex);
    }
}

// Notify all seller threads of the clock tick
void notify_all_seller_threads() {
    pthread_mutex_lock(&condition_mutex);
    if (verbose)
        printf("00:%02d Main Thread Broadcasting Clock Tick\n", current_time_slice);

    pthread_cond_broadcast(&condition_cond);
    pthread_mutex_unlock(&condition_mutex);
}

// Function executed by each seller thread
void *sell(void *t_args) {
    Seller *args = (Seller *)t_args;
    queue *customer_queue = args->seller_queue;
    queue *seller_queue = create_queue();
    char seller_type = args->seller_type;
    int seller_number = args->seller_number + 1;

    pthread_mutex_lock(&thread_count_mutex);
    thread_count--;
    active_threads++;
    pthread_mutex_unlock(&thread_count_mutex);

    Customer *cust = NULL;
    int random_wait_time = 0;

    while (current_time_slice < TOTAL_SIMULATION_DURATION) {
        // Waiting for clock tick
        pthread_mutex_lock(&condition_mutex);
        if (verbose)
            printf("00:%02d %c%02d Waiting for the next clock tick\n", current_time_slice, seller_type, seller_number);

        pthread_mutex_lock(&thread_waiting_for_clock_tick_mutex);
        threads_waiting_for_clock_tick++;
        pthread_mutex_unlock(&thread_waiting_for_clock_tick_mutex);

        pthread_cond_wait(&condition_cond, &condition_mutex);
        if (verbose)
            printf("00:%02d %c%02d Received Clock Tick\n", current_time_slice, seller_type, seller_number);

        pthread_mutex_unlock(&condition_mutex);

        // Sell
        if (current_time_slice == TOTAL_SIMULATION_DURATION)
            break;

        // All New Customers Arrived
        while (customer_queue->size > 0 && ((Customer *)customer_queue->head->value)->arrival_time <= current_time_slice) {
            Customer *temp = (Customer *)dequeue(customer_queue);
            enqueue(seller_queue, temp);
            printf("00:%02d   %c%d   Customer Number %c%d%02d arrived\n", current_time_slice, seller_type, seller_number, seller_type, seller_number, temp->customer_number);
        }

        // Serve the next customer
        if (cust == NULL && seller_queue->size > 0) {
            cust = (Customer *)dequeue(seller_queue);
            cust->response_time = current_time_slice - cust->arrival_time;

            printf("00:%02d   %c%d   Serving Customer Number %c%d%02d   %8d\n", current_time_slice, seller_type, seller_number, seller_type, seller_number, cust->customer_number, cust->response_time);

            switch (seller_type) {
            case 'H':
                random_wait_time = (rand() % 2) + 1;
                response_time_for_high += cust->response_time;
                break;
            case 'M':
                random_wait_time = (rand() % 3) + 2;
                response_time_for_medium += cust->response_time;
                break;
            case 'L':
                random_wait_time = (rand() % 4) + 4;
                response_time_for_low += cust->response_time;
                break;
            }
        }

        if (cust != NULL) {
            if (random_wait_time == 0) {
                // Selling Seat
                pthread_mutex_lock(&reservation_mutex);

                // Find seat
                int seatIndex = find_available_seat(seller_type);
                if (seatIndex == -1) {
                    printf("00:%02d   %c%d   Customer Number %c%d%02d has been told the concert is sold out.\n", current_time_slice, seller_type, seller_number, seller_type, seller_number, cust->customer_number);
                } else {
                    int row_no = seatIndex / CONCERT_COLS;
                    int col_no = seatIndex % CONCERT_COLS;
                    cust->turnaround_time += current_time_slice;
                    sprintf(available_seats_matrix[row_no][col_no], "%c%d%02d", seller_type, seller_number, cust->customer_number);
                    printf("00:%02d   %c%d   Customer Number %c%d%02d seat %d,%d   %15d\n", current_time_slice, seller_type, seller_number, seller_type, seller_number, cust->customer_number, row_no, col_no, cust->turnaround_time);

                    switch (seller_type) {
                    case 'H':
                        turnaround_time_for_high += cust->turnaround_time;
                        break;
                    case 'M':
                        turnaround_time_for_medium += cust->turnaround_time;
                        break;
                    case 'L':
                        turnaround_time_for_low += cust->turnaround_time;
                        break;
                    }
                }

                pthread_mutex_unlock(&reservation_mutex);
                cust = NULL;
            } else {
                random_wait_time--;
            }
        }
    }

    while (cust != NULL || seller_queue->size > 0) {
        if (cust == NULL)
            cust = (Customer *)dequeue(seller_queue);

        printf("00:%02d   %c%d   Ticket sales closed. Customer Number %c%d%02d leaves\n", current_time_slice, seller_type, seller_number, seller_type, seller_number, cust->customer_number);
        cust = NULL;
    }

    pthread_mutex_lock(&thread_count_mutex);
    active_threads--;
    pthread_mutex_unlock(&thread_count_mutex);
}

// Find an available seat based on the seller type
int find_available_seat(char seller_type) {
    int seatIndex = -1;

    if (seller_type == 'H') {
        for (int row_no = 0; row_no < CONCERT_ROWS; row_no++) {
            for (int col_no = 0; col_no < CONCERT_COLS; col_no++) {
                if (strcmp(available_seats_matrix[row_no][col_no], "-") == 0) {
                    seatIndex = row_no * CONCERT_COLS + col_no;
                    return seatIndex;
                }
            }
        }
    } else if (seller_type == 'M') {
        int mid = CONCERT_ROWS / 2;
        int row_jump = 0;
        int next_row_no = mid;

        for (row_jump = 0;; row_jump++) {
            int row_no = mid + row_jump;
            if (mid + row_jump < CONCERT_ROWS) {
                for (int col_no = 0; col_no < CONCERT_COLS; col_no++) {
                    if (strcmp(available_seats_matrix[row_no][col_no], "-") == 0) {
                        seatIndex = row_no * CONCERT_COLS + col_no;
                        return seatIndex;
                    }
                }
            }

            row_no = mid - row_jump;
            if (mid - row_jump >= 0) {
                for (int col_no = 0; col_no < CONCERT_COLS; col_no++) {
                    if (strcmp(available_seats_matrix[row_no][col_no], "-") == 0) {
                        seatIndex = row_no * CONCERT_COLS + col_no;
                        return seatIndex;
                    }
                }
            }

            if (mid + row_jump >= CONCERT_ROWS && mid - row_jump < 0) {
                break;
            }
        }
    } else if (seller_type == 'L') {
        for (int row_no = CONCERT_ROWS - 1; row_no >= 0; row_no--) {
            for (int col_no = CONCERT_COLS - 1; col_no >= 0; col_no--) {
                if (strcmp(available_seats_matrix[row_no][col_no], "-") == 0) {
                    seatIndex = row_no * CONCERT_COLS + col_no;
                    return seatIndex;
                }
            }
        }
    }

    return -1;
}

// Generate a customer queue with N customers
queue *generate_customer_queue(int N) {
    queue *customer_queue = create_queue();
    char customer_number = 0;

    while (N--) {
        Customer *cust = (Customer *)malloc(sizeof(Customer));
        cust->customer_number = customer_number;
        cust->arrival_time = rand() % TOTAL_SIMULATION_DURATION;
        enqueue(customer_queue, cust);
        customer_number++;
    }

    // Sort the customer queue based on arrival time
    sort(customer_queue, compare_by_arrival_time);
    node *ptr = customer_queue->head;
    customer_number = 0;

    while (ptr != NULL) {
        customer_number++;
        Customer *cust = (Customer *)ptr->value;
        cust->customer_number = customer_number;
        ptr = ptr->next;
    }

    return customer_queue;
}

// Comparison function for sorting customers by arrival time
int compare_by_arrival_time(void *value1, void *value2) {
    Customer *c1 = (Customer *)value1;
    Customer *c2 = (Customer *)value2;

    if (c1->arrival_time < c2->arrival_time) {
        return -1;
    } else if (c1->arrival_time == c2->arrival_time) {
        return 0;
    } else {
        return 1;
    }
}

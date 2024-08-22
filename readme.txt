This repository contains a multi-threaded ticket selling simulation implemented in C. The simulation involves high, medium, and low priority ticket sellers handling customer requests concurrently. The project uses a linked list and queue implementation provided in the utility.c and utility.h files to manage data structures efficiently.

Compilation and Execution: To compile the project, use the following commands:
	$ gcc -o ticket_selling_simulation ticketSeller.c utility.c -pthread

To run the simulation with a specific number of customers (replace <number_of_customers> with the desired number):
	$ ./ticket_selling_simulation <number_of_customers>
Example:
	$ ./ticket_selling_simulation 5

Code Structure
ticketSeller.c: The main simulation code.
utility.c and utility.h: Implementations of linked list and queue data structures used in the project.
Linked List and Queue
The utility.c and utility.h files contain functions for creating, managing, and sorting linked lists and queues. These data structures are essential for efficient customer and thread management in the simulation.

Customization
Adjust the simulation parameters, such as the number of high, medium, and low priority sellers, total simulation duration, and concert seating details, in the ticketSeller.c file as needed.



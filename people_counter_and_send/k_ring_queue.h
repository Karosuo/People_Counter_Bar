/**
*	@file k_ring_queue.h
*	@author Rafael Karosuo 
*	@date Apr 29 2018
* 	@brief Circular queue implementation header
* 	The idea is a circular queue that does not block the process when the buffer is full
* 	Ignore all the new data that comes when the buffer is full
* 	It is used when transmiting data that don't want to block the process since we can sense some new information
* 	while transmiting. The new data ignored when the buffer is full, is not lost, since saved in a separate non volatile memory and eventually will be sent away
*/

#ifndef K_RING_QUEUE_H_
#define K_RING_QUEUE_H_
#endif


//If the queue length not defined, use a default size
/**
 * The idea is to set this define in the code, before calling the library.
 * But if for some reason that value is missed, this section ensure the size is always set*/
#ifndef MAX_QUEUE_LEN
#define MAX_QUEUE_LEN 128
#endif

#define IS_EMPTY(buffer) (buffer->tail==buffer->head) ///< Macro that checks if queue is empty or not, uses C int bool logic, 0 false, non 0 true
#define IS_FULL(buffer) (((buffer->tail+1)%MAX_QUEUE_LEN) == buffer->head)///< Macro that checks if queue is full or not, uses C int bool logic, 0 false, non 0 true

/**
 * Circular Queue structure to hold bytes as main elements
 * unsigned char elements, means 0 to 255
 * */
typedef struct {
	unsigned char buffer[MAX_QUEUE_LEN]; ///< Buffer element list
	unsigned char tail; ///< Index pointer to the tail, the one that increments when push items into the queue
	unsigned char head; ///< Index pointer to the head, the one that decreases when pops out items from the queue	
} byte_circ_queue_t;

void enqueue_byte(byte_circ_queue_t * ring_buffer, unsigned char new_element); ///< Pushes a byte into the queue, increment tail
unsigned char dequeue_byte(byte_circ_queue_t * ring_buffer); ///< Pops out a byte from the queue, increment head
void clear_queue(byte_circ_queue_t * ring_buffer); ///< Reset the pointers to -1, empty queue
//byte_circ_queue_t * getBufferPointer(); ///< Get the pointer to the instance of the struct as a kind of Singleton styled buffer

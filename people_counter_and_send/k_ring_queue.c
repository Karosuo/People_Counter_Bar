
/**
*	@file k_ring_queue.c
*	@author Rafael Karosuo 
*	@date Apr 29 2018
* 	@brief Circular queue implementation file
* 	The idea is a circular queue that does not block the process when the buffer is full
* 	Ignore all the new data that comes when the buffer is full
* 	It is used when transmiting data that don't want to block the process since we can sense some new information
* 	while transmiting. The new data ignored when the buffer is full, is not lost, since saved in a separate non volatile memory and eventually will be sent away
* 	
*/

#include "k_ring_queue.h"

/**
 * Initialize the buffer as a 0 values
 * tail and head -1
 * */
//static byte_circ_queue_t outgoing_buffer = {{0}, -1, -1}; ///< Instance of the queue type, use static to keep it private to the file, so other header files could use the same variable name for different purposes, and not alter this one

/**
 * General enqueue function, that takes the ring buffer and inserts a char as a new component
 * @param [in] ring_buffer is the pointer to the tag_buffer_queue that will receive the data
 * @param [in] new_element is the byte element which will put into the queue
 * The function ensures that the element will be putted into the buffer if there's space
 * If no space available, just ignore the new data
 * It also follows the logic
 * CHECK IF FULL, IF NOT THEN CONTINUE, RETURN IMMEDIATLY OTHERWISE
 * INCREMENT TAIL FIRST
 * THEN PUSH THE ELEMENT INTO THE QUEUE
 * */
void enqueue_byte(byte_circ_queue_t * ring_buffer, unsigned char new_element)
{///< Pushes a byte into the queue, increment tail
	if(!IS_FULL(ring_buffer))
	{
		ring_buffer->tail = (ring_buffer->tail+1) % MAX_QUEUE_LEN; ///< Increment tail index, do a mod of the queue size to avoid index overflow
		ring_buffer->buffer[ring_buffer->tail] = new_element; ///< Insert the new element in the tail position		
	}
}


/**
 * General denqueue function, that takes the ring buffer and pops out  a byte, it removes that element from the queue
 * @param [in] ring_buffer is the pointer to the tag_buffer_queue that has the element to be extracted 
 * The function ensures that the queue is not EMPTY before try to extract the element
 * RETURN:
 * 	popped_element -> unsigned char returned from the queue, this value was deleted from the queue
 * 
 * Typically the ring buffer will be used to save indexes or ascii values, so its default value is 255
 * In this way we can know at some level if the function is just ignoring the instruction or really getting out a value
 * BUT! AS WE CAN STORE THE VALUE 255 IN THE QUEUE, IT WILL NOT WORK FOR ALL THE CASES
 * 
 * If empty then just ignore the operation 
 * It also follows the logic
 * IF TAIL == HEAD, SET TAIL AND HEAD TO -1 (EMPTY STATE)
 * CHECK IF EMPTY, IF NOT THEN CONTINUE, RETURN IMMEDIATLY OTHERWISE
 * INCREMENT THE HEAD FIRST
 * THEN POP OUT THE ELEMENT FROM THE QUEUE
 * */
unsigned char dequeue_byte(byte_circ_queue_t * ring_buffer)
{///< Pops out a byte from the queue, increment head
	unsigned char popped_element = 255; ///< Default value
	if(ring_buffer->tail == ring_buffer->head)
	{///Means need to change to empty state
		ring_buffer->tail = -1;
		ring_buffer->head = ring_buffer->tail;
	}
	
	if(!IS_EMPTY(ring_buffer))
	{
		ring_buffer->head = (ring_buffer->head+1) % MAX_QUEUE_LEN;
		popped_element = ring_buffer->buffer[ring_buffer->head]; ///< Take out the element, and copy to the returned variable		
	}
}

/*
byte_circ_queue_t * getBufferPointer()
{
	///< Get the pointer to the instance of the struct as a kind of Singleton styled buffer
	 return &outgoing_buffer;
} 
*/

void clear_queue(byte_circ_queue_t * ring_buffer)
{///< Reset the pointers to -1, empty queue	
	ring_buffer->tail = -1;
	ring_buffer->head = ring_buffer->tail;
}

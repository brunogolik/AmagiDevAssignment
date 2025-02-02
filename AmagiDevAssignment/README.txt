Upon calling, the BinaryWriter.write function writes a single payload
into the binary file. Since the binary file contains only the payloads,
the information about the received packet history needs to be stored
elsewhere. In my implementation, it is handled in memory through
BinaryWriter's "intervals" member. Instead of naively storing every
packet ID individually, they are stored as ranges. For example:

Let's say that packets with IDs 1,2,3,5,6,7,9,10 have arrived. The data
is stored in the form [1-3][5-7][9-10] (only the endpoints are relevant).
If we were to now receive a packet with ID = 4, this would collapse
into [1-7][9-10]. This way of storing data keeps the "intervals" relatively
small at all times, as opposed to the naive approach, which always linearly
grows as new packets come. Note that in the case of a completely random shuffle
(where packet arrival order is completely uncorrelated with its sending order), 
in the worst case scenario the size still grows linearly until half of all 
packets have been inserted (after the midpoint, it would linearly decrease).

The most expensive part of the process is the insertion into the binary
file. In order to "squeeze" the new payload into the file, the tailing data
(the data that comes after the payload's rightful position) must be copied
to a temporary file and then returned after tha payload has been inserted
(alternatively, the process can be done without a second file through buffers,
but the logic is slightly more complex. In my implementation, I use a temporary
file). Since the problem requires that the file be updated each time a new packet 
arrives, the cost of this operation could potentially make the process slow. 

Therefore, if speed is crucial, and there are no working memory restrictions, 
my suggestion for improvement would be to write the packets in "batches" instead 
of individually. This could be done by modifying the code of the current "write" 
function in the following way:
1) When a package arrives, it gets processed in the same way (this includes
   updating "intervals"), but it is not yet written into the file. Instead, its
   start pointer is placed in an array (batch), which would be a member of 
   BinaryWriter.
2) Once the batch is filled, the output file's contents are copied into a 
   temporary file. Then, data is written into the output file by stitching 
   together the copied sequence and the sequences in the batch (this stitching 
   would be governed by "intervals", which contains all the necessary data about
   payload positions).
3) The batch is emptied and can receive new packets. Note that there should
   be some way for the program to know when to "prematurely" write from the
   batch. For example, if there are 50 packages altogether, and the batch
   size is 32, the second batch should be written when it fills up to 18.
   (One way would be to include some type of "END" indicator in the header
   of the final packet).
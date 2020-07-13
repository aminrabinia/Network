# Network
/*
 This program can read a file from server side and send it to the client
 The file is first read into an array of chunks (size of 4096 bytes) then
 Server sends the chunk size, number of chunks, and the port number to
 Client. This program uses TCP to control the dataflow and UDP to transfer
 the data. There are 10 threads used here to send the data. Client priodically
 sends SACK to Server. It also sends the NACK, containing a bitmap of received
 chunks, to Server. If there are missing chunks in the bitmap, Server will
 resend those. Client, finally, gathers all the chunks and recreats the file.
*/

This is not finished! - currently working on it!

This is a FTP protocol that adheres to the CCSDS (Consultative Committee for Space Data Systems) spec for sending files into space. This project is being built for a student lead space initiative at the Univerity of Alberta called ABsat. 

I want to eventually make it opensource, but there is a lot of work to do before it is in any condition for use.

if you want to get in contact with me
email me at evangiese77@gmail.com




Compilation Notes:
    for compiling with free rtos, first we need to specifiy the locatifon of the free RTOS header files that we need within
    our makefile. Then we can compile our .a file. To use this file, just staticly link to the file_delivery_app.a.

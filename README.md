# Spell Checker Server
I used netcat for clients to connect to my server.

When running my server program, the commandline arguments mean the following:
argv[0] - filename
argv[1] - port number (optional)
argv[2] - dictionary textfile name (optional)
eg: ./server 9898 words.txt
eg: ./server 7777
eg: ./server

Using netcat to connect to my server can be done by entering the following
in the commandline:

netcat localhost [port number]

If when starting the server, no port number was entered, use 8080, that is the
default port.

If not dictionary name is provided, the server uses a default dictionary textfile called "words.txt".

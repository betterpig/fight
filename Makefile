.PHONY:all
all:echo_client echo_server

echo_client:echo_client.c str_cli.c libunp.a
	gcc echo_client.c str_cli.c libunp.a -o echo_client 
echo_server:echo_server.c sigchldwait.c str_echo.c libunp.a
	gcc -g echo_server.c sigchldwait.c str_echo.c libunp.a -o echo_server 

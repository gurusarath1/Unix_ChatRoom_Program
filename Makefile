
c=gcc
all:
	$(c) UnixChatServer.c -o UnixChatServer
	$(c) UnixChatClient.c -o UnixChatClient
clean:
	rm UnixChatServer
	rm UnixChatClient
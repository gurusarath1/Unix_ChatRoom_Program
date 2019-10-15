#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/select.h>

#define MAX_BUFFER_SIZE 1000
#define MAX_PAYLOAD_SIZE 1000
#define BACKLOG 3
#define NO_SOCK 0

#define CHAT_v1_VERSION 3
#define MAX_USERNAME_LEN 16 //16  bytes
#define MAX_MESSAGE_LEN 512 //512 bytes
#define MAX_REASON_LEN 32 //32 bytes
#define CLIENT_COUNT_LEN 2 //2 bytes

typedef enum YES_NO{
	YES = 1,
	NO = 0
} YES_NO;

typedef enum FOUND_NOT_FOUND{
	FOUND = 1,
	NOT_FOUND = 0
} FOUND_NOT_FOUND;

typedef enum CHAT_v1_Frame_Type{

    CONNECT = 2,
    DIRECT = 3,
    SEND = 4,
    NAK = 5,
    OFFLINE = 6,
    ACK = 7,
    ONLINE = 8,
	IDLE = 9

}CHAT_v1_Frame_Type;



typedef enum CHAT_v1_FrameType_Type {

    Reason = 1,
    Username = 2,
    ClinetCount = 3,
    Message = 10

}CHAT_v1_FrameType_Type;



enum error_Codes
{
	VERSION_ERROR = 1,
	INVALID_LENGTH,
	INVALID_MESSAGE_TYPE,
	INVALID_ATTRIBUTE_TYPE
};

typedef struct CHAT_v1_FrameType {

    unsigned int Type;
    unsigned int len;
    char PayLoad[MAX_PAYLOAD_SIZE];
} CHAT_v1_FrameType;

typedef struct CHAT_v1_Frame {

    unsigned int Version;
    unsigned int Type;
    unsigned int len;
    CHAT_v1_FrameType PayLoad;
} CHAT_v1_Frame;


typedef struct USER USER;
struct USER
{
    char UserName[16];
    int sock;
    int CONNECT_Received;
    USER* NextUser;
};



typedef struct USER_LIST
{
    int number_of_users_currently_connected;
    USER* FirstUser;
}USER_LIST;



/* Function Declarations */

int WriteData(int port, char c[], int len);
int getLine();

int Create_MessageToTransmit(char* Buf, CHAT_v1_Frame MSG);
int Create_CHAT_v1_FrameType(int type, int length, char* Payload, CHAT_v1_FrameType* Attribute);
int Create_CHAT_v1_Frame(int type, int length, CHAT_v1_FrameType Payload, CHAT_v1_Frame* MessageX);
int DecodeReceivedMessage(char* Buf, CHAT_v1_Frame* RCV_MSG);
int ProcessReceivedMessage(int SD);

/* Variable Declarations */
int portNum;
int maxValueOfSD;

char receiveBuffer[MAX_BUFFER_SIZE];
char transmitBuffer[MAX_BUFFER_SIZE];
CHAT_v1_FrameType receiveFrameType_struct;
CHAT_v1_Frame receiveFrame_struct;

char UserNameOfClient[16]; //Username of client
char IP_address[100]; //IP of server
char port[100]; //Port of server
int ACK_received = 0; // Falg to indicate if ACK was received

int main()
{
	
	printf("\n\n#### Simple Chat Client Application ####\n");
	
	printf("\nEnter User Name - ");
	gets(UserNameOfClient);
	printf("\nEnter IP - ");
	gets(IP_address);
	printf("\nEnter the Unique ID to join Chatroom - ");
	gets(port);
	
	
	int sd_Client;
	struct sockaddr_in server;
	char *host;
	char buf[MAX_BUFFER_SIZE];
	int len;
	char server_reply[2000];
	fd_set read_fds, master_copy_read_fds;
	char MessageFromUser[1000];
	
	
	//Create socket
	sd_Client = socket(AF_INET , SOCK_STREAM , 0);
	if (sd_Client == -1)
	{
		printf("Could not create socket");
	}
	
	
	//server.sin_addr.s_addr = inet_addr(host);
	server.sin_addr.s_addr = inet_addr(IP_address);
	server.sin_family = AF_INET;
	server.sin_port = htons(atoi(port));
	//server.sin_port = htons(8889);

	//Connect to remote server
	if ( connect(sd_Client , (struct sockaddr *)&server , sizeof(server)) < 0)
	{
		puts("connect error");
		return 1;
	}
	
	puts("\nConnected");
	
	
	// Send CONNECT Message
	CHAT_v1_Frame CONNECT_MSG;
	CHAT_v1_FrameType CONNECT_MSG_ATTRIB;
	Create_CHAT_v1_FrameType(Username, strlen(UserNameOfClient), UserNameOfClient, &CONNECT_MSG_ATTRIB);
	Create_CHAT_v1_Frame(CONNECT, 1, CONNECT_MSG_ATTRIB, &CONNECT_MSG);
	Create_MessageToTransmit(transmitBuffer, CONNECT_MSG);			
	WriteData(sd_Client, transmitBuffer, strlen(transmitBuffer));
	
	
	
	
	FD_ZERO(&read_fds);
    FD_ZERO(&master_copy_read_fds);
    FD_SET(sd_Client, &master_copy_read_fds);
	FD_SET(0, &master_copy_read_fds);
	
	if(sd_Client > maxValueOfSD) maxValueOfSD = sd_Client;
	
	while(1)
	{
		read_fds = master_copy_read_fds;
		select(maxValueOfSD + 1, &read_fds, NULL, NULL, NULL);

		if (FD_ISSET(sd_Client, &read_fds))
		{
			FD_ZERO(&read_fds);
			memset(receiveBuffer, '\0', MAX_BUFFER_SIZE);
			
			int rcv_len;
			rcv_len = getLine(sd_Client, receiveBuffer, sizeof(receiveBuffer));
			
			if (rcv_len <= 0)
			{
				printf("\nServer Down!!");
				return 1;
			}
				
			else
			{
				ProcessReceivedMessage(sd_Client);
			}
				
		}
		
		if (FD_ISSET(0, &read_fds))
		{
			FD_ZERO(&read_fds);
			memset(MessageFromUser, '\0', MAX_BUFFER_SIZE);
			fgets(MessageFromUser, MAX_PAYLOAD_SIZE, stdin);
			
			CHAT_v1_Frame MSG;
			CHAT_v1_FrameType MSG_ATTRIB;
			Create_CHAT_v1_FrameType(Message, strlen(MessageFromUser), MessageFromUser, &MSG_ATTRIB);
			Create_CHAT_v1_Frame(SEND, 1, MSG_ATTRIB, &MSG);
			Create_MessageToTransmit(transmitBuffer, MSG);			
			WriteData(sd_Client, transmitBuffer, strlen(transmitBuffer));
			
			continue;
		}
		
		
	}
	
	
	
	return 0;
	
}


int WriteData(int sock_d, char Buff[], int len)
{
    int n;
    n = send(sock_d, Buff, len, 0);
    return n;
}

int getLine(int sock_d, char Buff[], int len)
{
	int n;
	n = recv(sock_d, Buff, len-1, 0);
	return n;
}

int Create_CHAT_v1_FrameType(int type, int length, char* Payload, CHAT_v1_FrameType* Attribute)
{

    if(type == Username)
    {
        Attribute->Type = Username;
    } else if(type == Message)
    {
        Attribute->Type = Message;
    } else if(type == Reason)
    {
        Attribute->Type = Reason;
    } else if (type == ClinetCount)
    {
        Attribute->Type = ClinetCount;
    } else
    {
        return -1;
    }

    Attribute->len = length;

    int i;
    for(i=0; i<length; i++)
    {
        Attribute->PayLoad[i] = Payload[i];
    }

    Attribute->PayLoad[i] = '\0';

}

int Create_CHAT_v1_Frame(int type, int length, CHAT_v1_FrameType Payload, CHAT_v1_Frame* MessageX)
{
    MessageX->Version = CHAT_v1_VERSION;

    if(type == CONNECT)
    {
        MessageX->Type = CONNECT;

    } else if (type == SEND)
    {
        MessageX->Type = SEND;

    } else if (type == DIRECT)
    {
        MessageX->Type = DIRECT;
    }else
    {
        return -1;
    }

    MessageX->len = length;
    MessageX->PayLoad = Payload;

    return 0;
}


int DecodeReceivedMessage(char* Buf, CHAT_v1_Frame* RCV_MSG)
{

	
    int version, type1, length1, type2, length2, length1_byte1, length1_byte2, \
	       type2_byte1, type2_byte2, length2_byte1, length2_byte2;
		  
    char c[1000];
	memset(c, '\0', MAX_BUFFER_SIZE);

    version = Buf[0];
    type1 = Buf[1];
	length1 = Buf[2];
	type2 = Buf[3];
    length2 = Buf[4];
	
	//printf("\n%d  %d  %d  %d  %d", version, type1, length1, type2, length2);
	int i = 0;
	while(Buf[i] != ':')
	{
		i++;
	}
	
	i += 1;
	
	int j = 0;
    while(j < length2)
    {
        c[j] = Buf[i];
		i++;
		j++;
    }
	c[j] = '\0';
	

	
	if(version != 3)
	{
		return VERSION_ERROR;
		
	} else if (length1 < 0 || length2 < 0)
	{
		return INVALID_LENGTH;
		
	} else if (type1 != SEND && type1 != CONNECT && \
				type1 != DIRECT && type1 != ACK && type1 != NAK && \
				type1 != OFFLINE && type1 != ONLINE && type1 != IDLE )
	{
		return INVALID_MESSAGE_TYPE;
		
	} else if (type2 != Username && type2 != Message && type2 != Reason && type2 != ClinetCount)
	{
		return INVALID_ATTRIBUTE_TYPE;
		
	}
	
    RCV_MSG->len = length1;

    RCV_MSG->Type = type1;
	
    RCV_MSG->Version = version;
	
    RCV_MSG->PayLoad.len = length2;

    RCV_MSG->PayLoad.Type = type2;
	
    for (i=0; i<RCV_MSG->PayLoad.len; i++)
    {

        RCV_MSG->PayLoad.PayLoad[i] = c[i];

    }
    RCV_MSG->PayLoad.PayLoad[i] = '\0';
	
    return 0;

}


int Create_MessageToTransmit(char* Buf, CHAT_v1_Frame MSG)
{
	memset(Buf, '\0', MAX_BUFFER_SIZE);
	
	
	char c0 = MSG.Version;
	char c1 = MSG.Type;
	char c2 = MSG.len;
	char c3 = MSG.PayLoad.Type;
	char c4 = MSG.PayLoad.len;
	
	strcat(Buf, &c0);
	strcat(Buf, &c1);
	strcat(Buf, &c2);
	strcat(Buf, &c3);
	strcat(Buf, &c4);
	strcat(Buf, ":");
	strcat(Buf, MSG.PayLoad.PayLoad);
	

    return 0;
}

int ProcessReceivedMessage(int SD)
{
	
	CHAT_v1_Frame ReceivedMessage;
	DecodeReceivedMessage(receiveBuffer, &ReceivedMessage);

	
	
	if(ReceivedMessage.Type == DIRECT)
	{
		puts(ReceivedMessage.PayLoad.PayLoad);
		return 1;
	}
	
	if(ReceivedMessage.Type == ACK)
	{
		puts(ReceivedMessage.PayLoad.PayLoad);
		ACK_received = 1;
		return 1;
	}
	
	if(ReceivedMessage.Type == NAK)
	{
		puts(ReceivedMessage.PayLoad.PayLoad);
		close(SD);
		return 1;
	}
	
	if(ReceivedMessage.Type == ONLINE)
	{
		puts(ReceivedMessage.PayLoad.PayLoad);
		return 1;
	}
	
	if(ReceivedMessage.Type == OFFLINE)
	{
		puts(ReceivedMessage.PayLoad.PayLoad);
		return 1;
	}

	return 0;
}


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
#define MAX_DATA_X_SIZE 1000
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
    unsigned int Length;
    char DATA_X[MAX_DATA_X_SIZE];
} CHAT_v1_FrameType;

typedef struct CHAT_v1_Frame {

    unsigned int Version;
    unsigned int Type;
    unsigned int Length;
    CHAT_v1_FrameType DATA_X;
} CHAT_v1_Frame;


// Linked list of users (used to hold the connected users)
typedef struct USER USER;
struct USER
{
    char UserName[16];
    int sock;
    int CONNECT_Received;
    USER* NextUser;
};

// Head of linked list
typedef struct USER_LINKED_LIST
{
    int number_of_users_currently_connected; //Number of users in chat room
    USER* FirstUser;
	
}USER_LINKED_LIST;



/* Function Declarations */

int WriteData(int port, char c[], int len);
int getLine();
int Create_MessageToTransmit(char* Buf, CHAT_v1_Frame MSG);
int Create_CHAT_v1_FrameType(int type, int length, char* DATA_X, CHAT_v1_FrameType* Attribute);
int Create_CHAT_v1_Frame(int type, int length, CHAT_v1_FrameType DATA_X, CHAT_v1_Frame* MessageX);
int DecodeReceivedMessage(char* Buf, CHAT_v1_Frame* RCV_MSG);
int ProcessReceivedMessage(int SD);
int ForwardMessageToAllConnectedClients( int sender_SD, char MessageToSend[], int len);
void AddNewUser(char name[], int sd);
int removeUser(int SD);
void printAllUsers();
int UserJoined(int SD);
int HadCONNECTED(int sd, USER** userPtr);
int GetUserStructFromSock(int sd, USER** userPtr);
int IsValidUsername(char *userName);

/* Variable Declarations */
USER_LINKED_LIST AllConnectedUsers;
int portNum;
int maxValueOfSD;
int maxUsersAllowedToConnect;

char receiveBuffer[MAX_BUFFER_SIZE]; // Buffer to hold received data
char transmitBuffer[MAX_BUFFER_SIZE]; //Buffer used to transmit data
CHAT_v1_FrameType receiveFrameType_struct; //Structure to hold the received attribute
CHAT_v1_Frame receiveFrame_struct; //Structure to hold the received message
fd_set read_fds, master_copy_read_fds; //Multiplex IO 

int main()
{

    printf("\n\n#### Simple Chat Server Application ####\n");

    printf("\nEnter the Unique ID for the Chatroom (Clinet program will use this number to connect)- ");
	scanf("%d", &portNum);
	printf("\nEnter maximum client limit - ");
	scanf("%d", &maxUsersAllowedToConnect);


    /* MAIN CODE STARTS FROM HERE */
    struct sockaddr_in ServerAddr_in, clientX;
    int sd_temp, sd_server, sd_connection;
    struct addrinfo hints, *serverinfo, *servP;
    int rcv_len = 0;
    



	// Set all memory locations to zero
    memset(&ServerAddr_in, 0, sizeof(ServerAddr_in));

    ServerAddr_in.sin_family = AF_INET; //IPv4
    ServerAddr_in.sin_addr.s_addr = INADDR_ANY;
    ServerAddr_in.sin_port = htons( portNum ); // Host to network order conversion


    if( (sd_temp = socket(AF_INET, SOCK_STREAM, 0)) < 0 ) // Create socket
    {
        puts("Could not create socket");
        return 1;
    } else {

        sd_server = sd_temp;

    }

    if(bind(sd_server, (struct sockaddr *)&ServerAddr_in, sizeof(ServerAddr_in)) < 0) // Bind to socket
    {
        puts("\nBind action failed !");
        return 1;
    }

    FD_ZERO(&read_fds);
    FD_ZERO(&master_copy_read_fds);
    FD_SET(sd_server, &master_copy_read_fds);

    if(sd_server > maxValueOfSD) maxValueOfSD = sd_server;

    if(listen(sd_server, BACKLOG) < 0)
    {
        puts("\nListen Error !");
        return 1;
    }

    while(1)
    {
        puts("\n\nListening ... ... ... ");
        read_fds = master_copy_read_fds;
        select(maxValueOfSD + 1, &read_fds, NULL, NULL, NULL);


        /* NEW CONNECTION REQUEST */
		
        if (FD_ISSET(sd_server, &read_fds))
        {
			FD_ZERO(&read_fds);

            int sizeX = sizeof(struct sockaddr_in);
			
			sd_connection = accept(sd_server, (struct sockaddr *)&clientX, (socklen_t*)&sizeX);
			
			// Check if maximum limit reached
			if (AllConnectedUsers.number_of_users_currently_connected < maxUsersAllowedToConnect)
			{

				if(sd_connection < 0)
				{
					puts("\nConnection Failed !");
					return 1;
					
				} else {

					if(sd_connection > maxValueOfSD) maxValueOfSD = sd_connection;
					FD_SET(sd_connection, &master_copy_read_fds);
					printf("\nNew Connection on %d   Max = %d", sd_connection, maxValueOfSD);

				}
				
			}else {
				
				/*
				** Maximum number of users reached. Server closes the connection after sending NAK
				*/
				
				printf("\nMaximum limit of connected clients reached !!!");
				
				// Send NAK
				memset(transmitBuffer, '\0', MAX_BUFFER_SIZE);
				strcat(transmitBuffer, "Maximum Client limit reached !!!");
				//printf("\n");
				//puts(transmitBuffer);
				
				CHAT_v1_Frame MSG_NAK_USER_ENTER;
				CHAT_v1_FrameType MSG_NAK_USER_ENTER_ATTRIB;
				Create_CHAT_v1_FrameType(Reason, strlen(transmitBuffer), transmitBuffer, &MSG_NAK_USER_ENTER_ATTRIB);
				Create_CHAT_v1_Frame(NAK, 1, MSG_NAK_USER_ENTER_ATTRIB, &MSG_NAK_USER_ENTER);
				Create_MessageToTransmit(transmitBuffer, MSG_NAK_USER_ENTER);
				WriteData(sd_connection, transmitBuffer, strlen(transmitBuffer));
				
				close(sd_connection);
				
			}


            continue;

        }






        /* CLIENT SENDING DATA */
		
		/*
		** Iterate through all sockets to check which one has something to read
		*/
		
        for(int sd=0; sd<=maxValueOfSD; sd++)
        {

            if (FD_ISSET(sd, &read_fds) && sd != sd_server)
            {
				
				FD_ZERO(&read_fds);
				
                printf("\nNew Message on %d!", sd);
                memset(receiveBuffer, '\0', MAX_BUFFER_SIZE);

                // Read the received message
                rcv_len = getLine(sd, receiveBuffer, MAX_BUFFER_SIZE);
				//printf("\nlen rcv = %d", rcv_len);

                if(!rcv_len)
                {
					USER *UserExitingClient;
					GetUserStructFromSock(sd, &UserExitingClient);
					
					memset(transmitBuffer, '\0', MAX_BUFFER_SIZE);
					strcat(transmitBuffer, UserExitingClient->UserName);
					strcat(transmitBuffer, " - OFFLINE !!");
					//printf("\n");
					//puts(transmitBuffer);
			
					CHAT_v1_Frame MSG_OFFLINE_USER_EXIT;
					CHAT_v1_FrameType MSG_OFFLINE_USER_EXIT_ATTRIB;
					Create_CHAT_v1_FrameType(Username, strlen(transmitBuffer), transmitBuffer, &MSG_OFFLINE_USER_EXIT_ATTRIB);
					Create_CHAT_v1_Frame(OFFLINE, 1, MSG_OFFLINE_USER_EXIT_ATTRIB, &MSG_OFFLINE_USER_EXIT);
					Create_MessageToTransmit(transmitBuffer, MSG_OFFLINE_USER_EXIT);
					ForwardMessageToAllConnectedClients( sd, transmitBuffer, strlen(transmitBuffer));
					
					//printf("\nClient Closed !!");
					removeUser(sd);
                    close(sd);
                    FD_CLR(sd, &master_copy_read_fds);
					printAllUsers();
					
					
                } else {
					
					// Message received from client
					ProcessReceivedMessage(sd);

                } // Rcv_len > 0


            } // If set


        } // Loop through all SD s

    } //While 1

    close(sd_server);
    return 0;
}

/*
** ProcessReceivedMessage - This function processes the recieved packets
*/

int ProcessReceivedMessage(int SD)
{

	CHAT_v1_Frame ReceivedMessage;
	DecodeReceivedMessage(receiveBuffer, &ReceivedMessage);


	if(ReceivedMessage.Type == CONNECT)
	{
		// Check if username already exists 
		if (IsValidUsername(ReceivedMessage.DATA_X.DATA_X))
		{
			// Add the new user to list
			AddNewUser(ReceivedMessage.DATA_X.DATA_X, SD);
			// Set the CONNECTED flag to indicate that client has explicitly CONNECTED
			UserJoined(SD);
				
			// Notify all users
			memset(transmitBuffer, '\0', MAX_BUFFER_SIZE);
			strcat(transmitBuffer, ReceivedMessage.DATA_X.DATA_X);
			strcat(transmitBuffer, " - ONLINE !!");
				
			CHAT_v1_Frame MSG_ONLINE_USER_ENTER;
			CHAT_v1_FrameType MSG_ONLINE_USER_ENTER_ATTRIB;
			Create_CHAT_v1_FrameType(Username, strlen(transmitBuffer), transmitBuffer, &MSG_ONLINE_USER_ENTER_ATTRIB);
			Create_CHAT_v1_Frame(ONLINE, 1, MSG_ONLINE_USER_ENTER_ATTRIB, &MSG_ONLINE_USER_ENTER);
			Create_MessageToTransmit(transmitBuffer, MSG_ONLINE_USER_ENTER);
			ForwardMessageToAllConnectedClients( SD, transmitBuffer, strlen(transmitBuffer));


			// Send ACK
			 
			memset(transmitBuffer, '\0', MAX_BUFFER_SIZE);
			//strcat(transmitBuffer, "ACK FROM SERVER");
			sprintf(transmitBuffer, "ACK Message\nNumber of clients = %d",AllConnectedUsers.number_of_users_currently_connected);

			
			CHAT_v1_Frame MSG_ACK_USER_ENTER;
			CHAT_v1_FrameType MSG_ACK_USER_ENTER_ATTRIB;
			Create_CHAT_v1_FrameType(Message, strlen(transmitBuffer), transmitBuffer, &MSG_ACK_USER_ENTER_ATTRIB);
			Create_CHAT_v1_Frame(ACK, 1, MSG_ACK_USER_ENTER_ATTRIB, &MSG_ACK_USER_ENTER);
			Create_MessageToTransmit(transmitBuffer, MSG_ACK_USER_ENTER);
			WriteData(SD, transmitBuffer, strlen(transmitBuffer));

			
			printAllUsers();

			return 1;
			
		} else {
			
			/*
			** Username is already in use, send NAK and close the socket
			*/
			
			// Send NAK
			memset(transmitBuffer, '\0', MAX_BUFFER_SIZE);
			strcat(transmitBuffer, "Username already exists\nTry again with another username !!!");
			
			CHAT_v1_Frame MSG_NAK_USER_ENTER;
			CHAT_v1_FrameType MSG_NAK_USER_ENTER_ATTRIB;
			Create_CHAT_v1_FrameType(Reason, strlen(transmitBuffer), transmitBuffer, &MSG_NAK_USER_ENTER_ATTRIB);
			Create_CHAT_v1_Frame(NAK, 1, MSG_NAK_USER_ENTER_ATTRIB, &MSG_NAK_USER_ENTER);
			Create_MessageToTransmit(transmitBuffer, MSG_NAK_USER_ENTER);
			WriteData(SD, transmitBuffer, strlen(transmitBuffer));
            close(SD);
            FD_CLR(SD, &master_copy_read_fds);
			return 1;
			
		}

	}
	
	
	/*
	** When client sends a new message 
	*/
	if(ReceivedMessage.Type == SEND)
	{
		USER* userPtr;
		// Check if the client had CONNECTED the chat room 
		if(HadCONNECTED(SD, &userPtr))
		{
			
			// Forward message to all users
			memset(transmitBuffer, '\0', MAX_BUFFER_SIZE);
			strcat(transmitBuffer, userPtr->UserName);
			strcat(transmitBuffer, "- ");
			strcat(transmitBuffer, ReceivedMessage.DATA_X.DATA_X);
			//puts(transmitBuffer);

			CHAT_v1_Frame MSG_DIRECT;
			CHAT_v1_FrameType MSG_DIRECT_ATTRIB;
			Create_CHAT_v1_FrameType(Message, strlen(transmitBuffer), transmitBuffer, &MSG_DIRECT_ATTRIB);
			Create_CHAT_v1_Frame(DIRECT, 1, MSG_DIRECT_ATTRIB, &MSG_DIRECT);
			Create_MessageToTransmit(transmitBuffer, MSG_DIRECT);			
			ForwardMessageToAllConnectedClients( SD, transmitBuffer, strlen(transmitBuffer));

			return 1;

		}

	}
	

	return 0;
}


/*
** WriteData - Used to send data form socket
*/
int WriteData(int sock_d, char Buff[], int len)
{
    int n;
    n = send(sock_d, Buff, len, 0);
    return n;
}

/*
** getLine - Used to read data from socket
*/
int getLine(int sock_d, char Buff[], int len)
{
	int n;
	n = recv(sock_d, Buff, len-1, 0);
	return n;
}

/*
** Create_CHAT_v1_FrameType - Sets up the CHAT_v1 attribute structure
*/
int Create_CHAT_v1_FrameType(int type, int length, char* DATA_X, CHAT_v1_FrameType* Attribute)
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
        printf("\nInvalid Attrib Type"); return -1;
    }

    Attribute->Length = length;

    int i;
    for(i=0; i<length; i++)
    {
        Attribute->DATA_X[i] = DATA_X[i];
    }

    Attribute->DATA_X[i] = '\0';

}

/*
** Create_CHAT_v1_Frame - Sets up the CHAT_v1 message structure
*/
int Create_CHAT_v1_Frame(int type, int length, CHAT_v1_FrameType DATA_X, CHAT_v1_Frame* MessageX)
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
    }else if (type == ONLINE)
    {
        MessageX->Type = ONLINE;
    } else if (type == OFFLINE)
	{
		MessageX->Type = OFFLINE;
	} else if (type == ACK)
	{
		MessageX->Type = ACK;
	} else if (type == NAK)
	{
		MessageX->Type = NAK;
	} else if (type == NAK)
	{
		MessageX->Type = NAK;
	} else if (type == IDLE)
	{
		MessageX->Type = IDLE;
	} else {printf("\nInvalid Message Type"); return -1;}

    MessageX->Length = length;
    MessageX->DATA_X = DATA_X;

    return 0;
}

/*
** AddNewUser - Adds new user to the linked list (AllConnectedUsers)
** All users currently connected in the chat room
*/
void AddNewUser(char name[], int sd)
{
    AllConnectedUsers.number_of_users_currently_connected += 1;

    USER** UserX;
    UserX = &AllConnectedUsers.FirstUser;


    while(*UserX != NULL)
    {
        UserX = &((*UserX)->NextUser);
    }


    (*UserX) = malloc(sizeof(USER));

    strcpy((*UserX)->UserName, name);
    (*UserX)->sock = sd;
	(*UserX)->CONNECT_Received = 1;
    (*UserX)->NextUser = NULL;

}


/*
** DecodeReceivedMessage - Decode the incoming message and check the validity
*/
int DecodeReceivedMessage(char* Buf, CHAT_v1_Frame* RCV_MSG)
{

	
    int version, type1, length1, type2, length2;
	
    char c[1000];
	memset(c, '\0', MAX_BUFFER_SIZE);

    version = Buf[0];
    type1 = Buf[1];
	length1 = Buf[2];
	type2 = Buf[3];
    length2 = Buf[4];
	
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


	/*
	** This if else block checks the validity of the received message
	*/
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
	
	
    RCV_MSG->Length = length1;

    RCV_MSG->Type = type1;
	
    RCV_MSG->Version = version;
	
    RCV_MSG->DATA_X.Length = length2;

    RCV_MSG->DATA_X.Type = type2;
	
    for (i=0; i<RCV_MSG->DATA_X.Length; i++)
    {

        RCV_MSG->DATA_X.DATA_X[i] = c[i];

    }
    RCV_MSG->DATA_X.DATA_X[i] = '\0';
	
    return 0;

}


/*
** Create_MessageToTransmit - Convert message to transmit format
*/
int Create_MessageToTransmit(char* Buf, CHAT_v1_Frame MSG)
{
	memset(Buf, '\0', MAX_BUFFER_SIZE);
	
	char c0 = MSG.Version;
	char c1 = MSG.Type;
	char c2 = MSG.Length;
	char c3 = MSG.DATA_X.Type;
	char c4 = MSG.DATA_X.Length;
	
	strcat(Buf, &c0);
	strcat(Buf, &c1);
	strcat(Buf, &c2);
	strcat(Buf, &c3);
	strcat(Buf, &c4);
	strcat(Buf, ":");
	strcat(Buf, MSG.DATA_X.DATA_X);
	
    return 0;
}

/*
** ForwardMessageToAllConnectedClients -  Sends message to all users except sender_SD
*/
int ForwardMessageToAllConnectedClients( int sender_SD, char MessageToSend[], int len)
{
	USER* UserX  = AllConnectedUsers.FirstUser;
	
	while(UserX != NULL)
	{
		if(UserX->sock != sender_SD && UserX->CONNECT_Received == 1)
		{
			WriteData(UserX->sock, MessageToSend, len);
		}
		
		UserX = UserX->NextUser;
	}
	
	return 0;
}


/*
** UserJoined - set the joined flag to indicate that the user has CONNECTED
*/
int UserJoined(int SD)
{
	USER* UserX  = AllConnectedUsers.FirstUser;
	
	while(UserX != NULL)
	{
		if(UserX->sock == SD)
		{
			UserX->CONNECT_Received = 1;
		}
		UserX = UserX->NextUser;
	}
	
	return 0;
}


/*
** removeUser - Remove user from the linked list
*/
int removeUser(int SD)
{
	USER* UserX  = AllConnectedUsers.FirstUser;
	USER* PrevUserX = AllConnectedUsers.FirstUser;
	
	int count = 0;
	while(UserX != NULL)
	{
		if(UserX->sock == SD)
		{
			AllConnectedUsers.number_of_users_currently_connected -= 1;
			UserX->CONNECT_Received = 0;
			USER* TempUserX_next = UserX->NextUser;
			UserX->NextUser = NULL;
			
			if (count != 0)
				PrevUserX->NextUser = TempUserX_next;
			else 
				AllConnectedUsers.FirstUser  = TempUserX_next;
			
			free(UserX);
		}
		count += 1;
		PrevUserX = UserX;
		UserX = UserX->NextUser;
	}
	
	return 0;	
}


/*
** printAllUsers - Prints all the users in the chat room (used mailnly for debugging)
*/
void printAllUsers()
{
    USER* UserX = AllConnectedUsers.FirstUser;
	
	printf("\n==============================================");
    printf("\nNumber of User = %d", AllConnectedUsers.number_of_users_currently_connected);
	
	if(UserX != NULL)
		printf("\nUSER == SOCKET == CONNECTED ?");
    
	while(UserX != NULL)
    {
        printf("\n%s %d %d", UserX->UserName, UserX->sock, UserX->CONNECT_Received);
        UserX = UserX->NextUser;
    }
	
	printf("\n==============================================\n");

}

/*
** IsValidUsername - checks if a username is already in use
*/
int IsValidUsername(char *userName)
{
    USER* UserX = AllConnectedUsers.FirstUser;
    //printf("\nNumber of User = %d\n", AllConnectedUsers.number_of_users_currently_connected);

    while(UserX != NULL)
    {
        printf("\n%s", UserX->UserName);
		
		if( !(strcmp(UserX->UserName, userName)) )
		{	
			return NO;
		}
        UserX = UserX->NextUser;
    }	
    return YES;
}


/*
** HadCONNECTED -  Chekcks if an user had CONNECTED
*/
int HadCONNECTED(int sd, USER** userPtr)
{
    USER* UserX = AllConnectedUsers.FirstUser;
	
    while(UserX != NULL)
    {
		if(UserX->sock == sd)
		{
			if(UserX->CONNECT_Received)
			{
				*userPtr = UserX;
				return YES;
			
			} else {
					return NO;
				}
		}
        
        UserX = UserX->NextUser;
    }
	
	return NO;
	
}

/*
** GetUserStructFromSock - Gets the name of an user from socket ID
*/
int GetUserStructFromSock(int sd, USER** userPtr)
{
    USER* UserX = AllConnectedUsers.FirstUser;
	
    while(UserX != NULL)
    {
		if(UserX->sock == sd)
		{

			*userPtr = UserX;
			return FOUND;

		}
        
        UserX = UserX->NextUser;
    }
	
	return NOT_FOUND;
	
}




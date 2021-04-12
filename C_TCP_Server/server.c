#undef UNICODE

#define WIN32_LEAN_AND_MEAN

#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <stdlib.h>
#include <stdio.h>

// Need to link with Ws2_32.lib
#pragma comment (lib, "Ws2_32.lib")
// #pragma comment (lib, "Mswsock.lib")

#define DEFAULT_BUFLEN 512
#define DEFAULT_PORT "27015"

#define TIMEOUT_SEC      10

#define LEN_ADDRESS      30
#define NAME_ADDRESS     "\"addressOfRecord\""
#define LEN_MAX_RECORD   700

int ae_load_file_to_memory(const char* filename, char** result) {
    int size = 0;
    FILE* f;
    errno_t err = fopen_s(&f, filename, "rb");
    if (err != 0)
    {
        *result = NULL;
        return -1; // -1 means file opening fail 
    }
    fseek(f, 0, SEEK_END);
    size = ftell(f);
    fseek(f, 0, SEEK_SET);
    *result = (char*)malloc(size + 1);
    if (size != fread(*result, sizeof(char), size, f))
    {
        free(*result);
        return -2; // -2 means file reading fail 
    }
    fclose(f);
    (*result)[size] = 0;
    return size;
}

int findRecord(char* req, char* mem, char* res) {
    int count = 0;

    char* ptrMemCur = mem;
    char* ptrAorCur;

    char* address = malloc(LEN_ADDRESS);
    int lenNameAddress = strlen(NAME_ADDRESS);

    int lenRecord = 0;

    while (*(ptrMemCur + 1) != 0) {
        ptrAorCur = strstr(ptrMemCur, NAME_ADDRESS);
        if (ptrAorCur == 0) {
            printf("Error reading memory\n");
            return 1;
        }
        memcpy(address, ptrAorCur + lenNameAddress + 2, LEN_ADDRESS);
        *(address + LEN_ADDRESS) = 0;

        ptrMemCur = strstr(ptrAorCur, "\n");

        if (strcmp(address, req) == 0) {
            lenRecord = ptrMemCur - ptrAorCur + 1;
            memcpy(res, ptrAorCur - 1, lenRecord);
            *(res + lenRecord) = 0;
            printf("%s\n", res);
            break;
        }
    }

    return 0;
}

int __cdecl main(void)
{
    char* memRec;
    int size;
    size = ae_load_file_to_memory("../Data/regs", &memRec);
    if (size < 0) {
        puts("Error loading file");
        return 1;
    }

    char resRec[LEN_MAX_RECORD + 1];

    int statusSearch = 0;

    WSADATA wsaData;
    int iResult;

    SOCKET ListenSocket = INVALID_SOCKET;
    SOCKET ClientSocket = INVALID_SOCKET;

    struct addrinfo* result = NULL;
    struct addrinfo hints;

    int iSendResult;
    char recvbuf[DEFAULT_BUFLEN];
    int recvbuflen = DEFAULT_BUFLEN;

    // Initialize Winsock
    iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (iResult != 0) {
        printf("WSAStartup failed with error: %d\n", iResult);
        return 1;
    }

    ZeroMemory(&hints, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;
    hints.ai_flags = AI_PASSIVE;

    // Resolve the server address and port
    iResult = getaddrinfo(NULL, DEFAULT_PORT, &hints, &result);
    if (iResult != 0) {
        printf("getaddrinfo failed with error: %d\n", iResult);
        WSACleanup();
        return 1;
    }

    // Create a SOCKET for connecting to server
    ListenSocket = socket(result->ai_family, result->ai_socktype, result->ai_protocol);
    if (ListenSocket == INVALID_SOCKET) {
        printf("socket failed with error: %ld\n", WSAGetLastError());
        freeaddrinfo(result);
        WSACleanup();
        return 1;
    }

    // Setup the TCP listening socket
    iResult = bind(ListenSocket, result->ai_addr, (int)result->ai_addrlen);
    if (iResult == SOCKET_ERROR) {
        printf("bind failed with error: %d\n", WSAGetLastError());
        freeaddrinfo(result);
        closesocket(ListenSocket);
        WSACleanup();
        return 1;
    }

    freeaddrinfo(result);

    iResult = listen(ListenSocket, SOMAXCONN);
    if (iResult == SOCKET_ERROR) {
        printf("listen failed with error: %d\n", WSAGetLastError());
        closesocket(ListenSocket);
        WSACleanup();
        return 1;
    }

    // Accept a client socket
    ClientSocket = accept(ListenSocket, NULL, NULL);
    if (ClientSocket == INVALID_SOCKET) {
        printf("accept failed with error: %d\n", WSAGetLastError());
        closesocket(ListenSocket);
        WSACleanup();
        return 1;
    }

    DWORD timeout = TIMEOUT_SEC * 1000;
    setsockopt(ClientSocket, SOL_SOCKET, SO_RCVTIMEO, (char*)&timeout, sizeof(timeout));

    // Receive until the peer shuts down the connection
    do {

        iResult = recv(ClientSocket, recvbuf, recvbuflen, 0);
        if (iResult > 0) {
            printf("Bytes received: %d\n", iResult);
            *(recvbuf + LEN_ADDRESS) = 0;
            printf("%s\n", recvbuf);

            resRec[0] = 0;
            statusSearch = findRecord(recvbuf, memRec, resRec);
            if (statusSearch != 0) {
                printf("Error in searching\n");
            }
            printf("%s\n", resRec);

            // Echo the buffer back to the sender
            iSendResult = send(ClientSocket, resRec, LEN_MAX_RECORD, 0);
            if (iSendResult == SOCKET_ERROR) {
                printf("send failed with error: %d\n", WSAGetLastError());
                closesocket(ClientSocket);
                WSACleanup();
                return 1;
            }
            printf("Bytes sent: %d\n", iSendResult);

        }
        else if (iResult == 0) {
            printf("Connection closing...\n");
        }
        else if (iResult == SOCKET_ERROR) {
            if (WSAGetLastError() == WSAETIMEDOUT) {
                printf("Timeout\n");
                printf("Connection closing...\n");
            }
            else {
                printf("recv failed with error: %d\n", WSAGetLastError());
                closesocket(ClientSocket);
                WSACleanup();
                return 1;
            }
        }
        else {
            printf("recv failed with error: %d\n", WSAGetLastError());
            closesocket(ClientSocket);
            WSACleanup();
            return 1;
        }

    } while (iResult > 0);

    // shutdown the connection since we're done
    iResult = shutdown(ClientSocket, SD_SEND);
    if (iResult == SOCKET_ERROR) {
        printf("shutdown failed with error: %d\n", WSAGetLastError());
        closesocket(ClientSocket);
        WSACleanup();
        return 1;
    }

    // cleanup
    closesocket(ClientSocket);
    WSACleanup();

    return 0;
}
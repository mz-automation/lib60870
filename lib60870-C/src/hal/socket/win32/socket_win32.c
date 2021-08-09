/*
 *  Copyright 2016 MZ Automation GmbH
 *
 *  This file is part of lib60870-C
 *
 *  lib60870-C is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  lib60870-C is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with lib60870-C.  If not, see <http://www.gnu.org/licenses/>.
 *
 *  See COPYING file for the complete license text.
 */

#define _WINSOCK_DEPRECATED_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <stdbool.h>
#include <stdio.h>

#pragma comment (lib, "Ws2_32.lib")

#include "hal_socket.h"
#include "lib_memory.h"
#include "lib60870_config.h"
#include <stdio.h>

#define DEBUG_SOCKET 0

#ifndef __MINGW64_VERSION_MAJOR
struct tcp_keepalive {
    u_long  onoff;
    u_long  keepalivetime;
    u_long  keepaliveinterval;
};
#endif

#define SIO_KEEPALIVE_VALS    _WSAIOW(IOC_VENDOR,4)

struct sSocket {
    SOCKET fd;
    uint32_t connectTimeout;
};

struct sServerSocket {
    SOCKET fd;
    int backLog;
};

struct sHandleSet {
    fd_set handles;
    SOCKET maxHandle;
};

HandleSet
Handleset_new(void)
{
    HandleSet result = (HandleSet) GLOBAL_MALLOC(sizeof(struct sHandleSet));

    if (result != NULL) {
        FD_ZERO(&result->handles);
        result->maxHandle = INVALID_SOCKET;
    }

    return result;
}

void
Handleset_reset(HandleSet self)
{
    FD_ZERO(&self->handles);
    self->maxHandle = INVALID_SOCKET;
}

void
Handleset_addSocket(HandleSet self, const Socket sock)
{
    if (self != NULL && sock != NULL && sock->fd != INVALID_SOCKET) {
        FD_SET(sock->fd, &self->handles);

        if ((sock->fd > self->maxHandle) || (self->maxHandle == INVALID_SOCKET))
            self->maxHandle = sock->fd;
    }
}

int
Handleset_waitReady(HandleSet self, unsigned int timeoutMs)
{
    int result;

    if (self != NULL && self->maxHandle != INVALID_SOCKET) {
        struct timeval timeout;

        timeout.tv_sec = timeoutMs / 1000;
        timeout.tv_usec = (timeoutMs % 1000) * 1000;
        result = select((int)self->maxHandle + 1, &self->handles, NULL, NULL, &timeout);
    } else {
        result = -1;
    }

    return result;
}

void
Handleset_destroy(HandleSet self)
{
    GLOBAL_FREEMEM(self);
}

static bool wsaStartupCalled = false;
static int socketCount = 0;

void
Socket_activateTcpKeepAlive(Socket self, int idleTime, int interval, int count)
{
    struct tcp_keepalive keepalive;
    DWORD retVal=0;

    keepalive.onoff = 1;
    keepalive.keepalivetime = CONFIG_TCP_KEEPALIVE_IDLE * 1000;
    keepalive.keepaliveinterval = CONFIG_TCP_KEEPALIVE_INTERVAL * 1000;

     if (WSAIoctl(self->fd, SIO_KEEPALIVE_VALS, &keepalive, sizeof(keepalive),
                NULL, 0, &retVal, NULL, NULL) == SOCKET_ERROR)
     {
         if (DEBUG_SOCKET)
                printf("WIN32_SOCKET: WSAIotcl(SIO_KEEPALIVE_VALS) failed: %d\n",
                    WSAGetLastError());
     }
}

static void
setSocketNonBlocking(Socket self)
{
    unsigned long mode = 1;
    if (ioctlsocket(self->fd, FIONBIO, &mode) != 0) {
        if (DEBUG_SOCKET)
            printf("WIN32_SOCKET: failed to set socket non-blocking!\n");
    }

    /* activate TCP_NODELAY */

    int tcpNoDelay = 1;

    setsockopt(self->fd, IPPROTO_TCP, TCP_NODELAY, (const char*)&tcpNoDelay, sizeof(int));
}

static bool
prepareAddress(const char *address, int port, struct sockaddr_in *sockaddr)
{
    memset((char *)sockaddr, 0, sizeof(struct sockaddr_in));

    if (address != NULL) {
        struct hostent *server;
        server = gethostbyname(address);

        if (server == NULL)
            return false;

        memcpy((char *)&sockaddr->sin_addr.s_addr, (char *)server->h_addr, server->h_length);
    }
    else
        sockaddr->sin_addr.s_addr = htonl(INADDR_ANY);

    sockaddr->sin_family = AF_INET;
    sockaddr->sin_port = htons(port);

    return true;
}

static bool
wsaStartUp()
{
    if (wsaStartupCalled == false) {
        int ec;
        WSADATA wsa;

        if ((ec = WSAStartup(MAKEWORD(2, 0), &wsa)) != 0) {
            if (DEBUG_SOCKET)
                printf("WIN32_SOCKET: winsock error: code %i\n", ec);
            return false;
        }
        else {
            wsaStartupCalled = true;
            return true;
        }
    }
    else
        return true;
}

static void
wsaShutdown()
{
    if (wsaStartupCalled) {
        if (socketCount == 0) {
            WSACleanup();
            wsaStartupCalled = false;
        }
    }
}

ServerSocket
TcpServerSocket_create(const char *address, int port)
{
    ServerSocket serverSocket = NULL;
    int ec;
    SOCKET listen_socket = INVALID_SOCKET;

    if (wsaStartUp() == false)
        return NULL;

    struct sockaddr_in server_addr;

    if (!prepareAddress(address, port, &server_addr))
        return NULL;

    listen_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

#if CONFIG_ACTIVATE_TCP_KEEPALIVE == 1
    Socket_activateTcpKeepAlive(listen_socket, 1, 1, 1);
#endif

    if (listen_socket == INVALID_SOCKET) {
        if (DEBUG_SOCKET)
            printf("WIN32_SOCKET: socket failed with error: %i\n", WSAGetLastError());

        wsaShutdown();

        return NULL;
    }

    int optionReuseAddr = 1;
    setsockopt(listen_socket, SOL_SOCKET, SO_REUSEADDR, (char *)&optionReuseAddr, sizeof(int));

    ec = bind(listen_socket, (struct sockaddr *)&server_addr, sizeof(server_addr));

    if (ec == SOCKET_ERROR) {
        if (DEBUG_SOCKET)
            printf("WIN32_SOCKET: bind failed with error:%i\n", WSAGetLastError());
        closesocket(listen_socket);

        wsaShutdown();

        return NULL;
    }

    serverSocket = (ServerSocket)GLOBAL_MALLOC(sizeof(struct sServerSocket));

    if (serverSocket) {
        serverSocket->fd = listen_socket;
        serverSocket->backLog = 10;

        setSocketNonBlocking((Socket)serverSocket);

        socketCount++;
    }
    else {
        closesocket(listen_socket);
        wsaShutdown();
    }

    return serverSocket;
}

void
ServerSocket_listen(ServerSocket self)
{
    listen(self->fd, self->backLog);
}

Socket
ServerSocket_accept(ServerSocket self)
{
    SOCKET fd = INVALID_SOCKET;

    Socket conSocket = NULL;

    fd = accept(self->fd, NULL, NULL);

    if (fd != INVALID_SOCKET) {
        conSocket = (Socket)GLOBAL_CALLOC(1, sizeof(struct sSocket));
        conSocket->fd = fd;

        socketCount++;

        setSocketNonBlocking(conSocket);
    }

    return conSocket;
}

void
ServerSocket_setBacklog(ServerSocket self, int backlog)
{
    self->backLog = backlog;
}

void
ServerSocket_destroy(ServerSocket self)
{
    closesocket(self->fd);
    socketCount--;
    wsaShutdown();
    GLOBAL_FREEMEM(self);
}

Socket
TcpSocket_create()
{
    Socket self = NULL;

    if (wsaStartUp() == false)
        return NULL;

    SOCKET sock = socket(AF_INET, SOCK_STREAM, 0);

    if (sock != INVALID_SOCKET) {
        self = (Socket) GLOBAL_MALLOC(sizeof(struct sSocket));

        if (self) {
            self->fd = sock;
            self->connectTimeout = 5000;

            socketCount++;
        }
        else {
            if (DEBUG_SOCKET)
                printf("SOCKET: failed to create socket - cannot allocate memory\n");

            closesocket(sock);
            wsaShutdown();
        }
    }
    else {
        if (DEBUG_SOCKET)
            printf("SOCKET: failed to create socket (error code=%i)\n", WSAGetLastError());
    }

	return self;
}

void
Socket_setConnectTimeout(Socket self, uint32_t timeoutInMs)
{
    self->connectTimeout = timeoutInMs;
}

bool
Socket_bind(Socket self, const char* srcAddress, int srcPort)
{
    struct sockaddr_in localAddress;

    if (!prepareAddress(srcAddress, srcPort, &localAddress))
        return false;

    int result = bind(self->fd, (struct sockaddr*)&localAddress, sizeof(localAddress));

    if (result == SOCKET_ERROR) {
        if (DEBUG_SOCKET)
            printf("SOCKET: failed to bind TCP socket (errno=%i)\n", WSAGetLastError());

        closesocket(self->fd);
        self->fd = -1;

        return false;
    }    

    return true;
}

bool
Socket_connect(Socket self, const char* address, int port)
{
    struct sockaddr_in serverAddress;

    if (!prepareAddress(address, port, &serverAddress))
        return false;

    setSocketNonBlocking(self);

    fd_set fdSet;
    FD_ZERO(&fdSet);
    FD_SET(self->fd, &fdSet);

    if (connect(self->fd, (struct sockaddr *) &serverAddress, sizeof(serverAddress)) == SOCKET_ERROR) {
        if (WSAGetLastError() != WSAEWOULDBLOCK)
            return false;
    }

    struct timeval timeout;
    timeout.tv_sec = self->connectTimeout / 1000;
    timeout.tv_usec = (self->connectTimeout % 1000) * 1000;

    if (select((int)self->fd + 1, NULL, &fdSet, NULL, &timeout) <= 0)
        return false;
    else
        return true;
}

static char*
convertAddressToStr(struct sockaddr_storage* addr)
{
    char addrString[INET6_ADDRSTRLEN + 7];
    int addrStringLen = INET6_ADDRSTRLEN + 7;
    int port;

    bool isIPv6;

    if (addr->ss_family == AF_INET)  {
        struct sockaddr_in* ipv4Addr = (struct sockaddr_in*) addr;
        port = ntohs(ipv4Addr->sin_port);
        ipv4Addr->sin_port = 0;
        WSAAddressToString((LPSOCKADDR) ipv4Addr, sizeof(struct sockaddr_storage), NULL,
            (LPSTR) addrString, (LPDWORD) &addrStringLen);
        isIPv6 = false;
    }
    else if (addr->ss_family == AF_INET6){
        struct sockaddr_in6* ipv6Addr = (struct sockaddr_in6*) addr;
        port = ntohs(ipv6Addr->sin6_port);
        ipv6Addr->sin6_port = 0;
        WSAAddressToString((LPSOCKADDR) ipv6Addr, sizeof(struct sockaddr_storage), NULL,
            (LPSTR) addrString, (LPDWORD) &addrStringLen);
        isIPv6 = true;
    }
    else
        return NULL;

    char* clientConnection = (char*) GLOBAL_MALLOC(strlen(addrString) + 9);

    if (isIPv6)
        sprintf(clientConnection, "[%s]:%i", addrString, port);
    else
        sprintf(clientConnection, "%s:%i", addrString, port);

    return clientConnection;
}


char*
Socket_getPeerAddress(Socket self)
{
    struct sockaddr_storage addr;
    socklen_t addrLen = sizeof(addr);

    if (getpeername(self->fd, (struct sockaddr*) &addr, &addrLen) == 0) {
        return convertAddressToStr(&addr);
    }
    else
        return NULL;
}

char*
Socket_getLocalAddress(Socket self)
{
    struct sockaddr_storage addr;
    socklen_t addrLen = sizeof(addr);

    if (getsockname(self->fd, (struct sockaddr*) &addr, &addrLen) == 0) {
        return convertAddressToStr(&addr);
    }
    else
        return NULL;
}

char*
Socket_getPeerAddressStatic(Socket self, char* peerAddressString)
{
    struct sockaddr_storage addr;
    int addrLen = sizeof(addr);

    getpeername(self->fd, (struct sockaddr*) &addr, &addrLen);

    char addrString[INET6_ADDRSTRLEN + 7];
    int addrStringLen = INET6_ADDRSTRLEN + 7;
    int port;

    bool isIPv6;

    if (addr.ss_family == AF_INET) {
        struct sockaddr_in* ipv4Addr = (struct sockaddr_in*) &addr;
        port = ntohs(ipv4Addr->sin_port);
        ipv4Addr->sin_port = 0;
        WSAAddressToString((LPSOCKADDR) ipv4Addr, sizeof(struct sockaddr_storage), NULL,
                (LPSTR) addrString, (LPDWORD) & addrStringLen);
        isIPv6 = false;
    }
    else if (addr.ss_family == AF_INET6) {
        struct sockaddr_in6* ipv6Addr = (struct sockaddr_in6*) &addr;
        port = ntohs(ipv6Addr->sin6_port);
        ipv6Addr->sin6_port = 0;
        WSAAddressToString((LPSOCKADDR) ipv6Addr, sizeof(struct sockaddr_storage), NULL,
                (LPSTR) addrString, (LPDWORD) & addrStringLen);
        isIPv6 = true;
    }
    else
        return NULL;

    if (isIPv6)
        sprintf(peerAddressString, "[%s]:%i", addrString, port);
    else
        sprintf(peerAddressString, "%s:%i", addrString, port);

    return peerAddressString;
}

int
Socket_read(Socket self, uint8_t* buf, int size)
{
    int bytes_read = recv(self->fd, (char*) buf, size, 0);

    if (bytes_read == 0) /* peer has closed socket */
        return -1;

    if (bytes_read == SOCKET_ERROR) {
        if (WSAGetLastError() == WSAEWOULDBLOCK)
            return 0;
        else
            return -1;
    }

    return bytes_read;
}

int
Socket_write(Socket self, uint8_t* buf, int size)
{
    int bytes_sent = send(self->fd, (char*) buf, size, 0);

    if (bytes_sent == SOCKET_ERROR) {
        int errorCode = WSAGetLastError();

        if (errorCode == WSAEWOULDBLOCK)
            bytes_sent = 0;
        else
            bytes_sent = -1;
    }

    return bytes_sent;
}

void
Socket_destroy(Socket self)
{
    if (self->fd != INVALID_SOCKET) {
        closesocket(self->fd);
    }

    socketCount--;
    wsaShutdown();

    GLOBAL_FREEMEM(self);
}

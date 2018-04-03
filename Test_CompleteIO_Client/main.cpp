#include<stdio.h>
#include <winsock2.h>  
#pragma comment(lib,"ws2_32.lib") 
int main(int argc, char*argv[])
{
	WSADATA wsadata;
	int ierr = WSAStartup(MAKEWORD(1, 1), &wsadata);
	if (ierr != 0)
	{
		printf("WSAStartup faile\n");
	}
	SOCKET m_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (m_socket == INVALID_SOCKET)
	{
		printf("socket faile\n");
	}
	sockaddr_in m_addr;
	m_addr.sin_family = AF_INET;
	m_addr.sin_port = ntohs(1234);
	m_addr.sin_addr.S_un.S_addr = inet_addr("10.7.3.142");

	ierr = connect(m_socket,(const sockaddr*) &m_addr, sizeof(m_addr));
	if (ierr == SOCKET_ERROR)
	{
		printf("connect fiale\n");
	}
	printf("connect success\n");
	ierr = send(m_socket, "1234", 4, 0);
	if (ierr != 4)
	{
		printf("send faile\n");
	}
	for (;;)
	{
		Sleep(1000);
		send(m_socket, "1234", 4, 0);
	}

	return 0;
}
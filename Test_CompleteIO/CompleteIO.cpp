#include<stdio.h>
#include "CompleteIO.h"

#define MAX_ACCEPT              10  
DWORD WINAPI CCompleteIO::workThread(LPVOID param)
{
	CCompleteIO *oMyCompleteIo = (CCompleteIO*)param;
	OVERLAPPED           *pOverlapped = NULL;
	SPER_SOCKET_CONTEXT   *pSocketContext = NULL;
	DWORD                dwBytesTransfered = 0;

	while (WaitForSingleObject(oMyCompleteIo->m_hEvent,0) != WAIT_OBJECT_0)
	{
		BOOL bReturn = GetQueuedCompletionStatus(
			oMyCompleteIo->m_hIOCompletionPort,
			&dwBytesTransfered,
			(PULONG_PTR)&pSocketContext,
			&pOverlapped,
			INFINITE);
		if (bReturn)
		{
			SIO_CONTEXT *pIOContext = CONTAINING_RECORD(pOverlapped, SIO_CONTEXT, m_Overlapped);
			if (dwBytesTransfered == 0)//client disconnect
			{

			}
			else 
			{
				switch (pIOContext->m_OpType)
				{
				case ACCEPT_POSTED:
					oMyCompleteIo->DoAccept(pSocketContext,pIOContext);
					break;
				case SEND_POSTED:
					break;
				case RECV_POSTED:
					oMyCompleteIo->DoRecv(pSocketContext, pIOContext);
					break;
				default:
					break;
				}
			}
		}
		else
		{
			printf("GetQueuedCompletionStatus faile\n");
		}
	}
	return 0;
}
CCompleteIO::CCompleteIO()
{
	m_hIOCompletionPort = nullptr;
	m_pListenContext = nullptr;
	InitializeCriticalSection(&m_csLock);

	WSADATA wsadata;
	int ierr = WSAStartup(MAKEWORD(1, 1), &wsadata);
	if (ierr != 0)
	{
		printf("WSAStartup faile\n");
	}
}


CCompleteIO::~CCompleteIO()
{
	DeleteCriticalSection(&m_csLock);

	WSACleanup();
}
void CCompleteIO::DoRecv(SPER_SOCKET_CONTEXT *in_pSocket_Context, SIO_CONTEXT *in_pIO_Context)
{
	SOCKADDR_IN* ClientAddr = &in_pSocket_Context->m_ClientAddr;
	printf("Recv ip==%s,port==%d,Data==%s\n", inet_ntoa(ClientAddr->sin_addr), ntohs(ClientAddr->sin_port), in_pIO_Context->m_wsaBuf.buf);
	// 初始化变量  
	DWORD dwFlags = 0;
	DWORD dwBytes = 0;
	WSABUF *p_wbuf = &in_pIO_Context->m_wsaBuf;
	OVERLAPPED *p_ol = &in_pIO_Context->m_Overlapped;

	in_pIO_Context->ResetBuffer();
	in_pIO_Context->m_OpType = RECV_POSTED;

	// 初始化完成后，，投递WSARecv请求  
	int nBytesRecv = WSARecv(in_pIO_Context->m_sockAccept, p_wbuf, 1, &dwBytes, &dwFlags, p_ol, NULL);
	// 如果返回值错误，并且错误的代码并非是Pending的话，那就说明这个重叠请求失败了  
	if ((SOCKET_ERROR == nBytesRecv) && (WSA_IO_PENDING != WSAGetLastError()))
	{
		printf("DoRecv Post faile\n");
	}
}
void CCompleteIO::DoAccept(SPER_SOCKET_CONTEXT *in_pSocket_Context, SIO_CONTEXT *in_pIO_Context)
{
	SOCKADDR_IN* ClientAddr = NULL;//Remote
	SOCKADDR_IN* LocalAddr = NULL;
	
	int remoteLen = sizeof(SOCKADDR_IN), localLen = sizeof(SOCKADDR_IN);
	
	//获取连接的远端IP和端口
	this->m_lpfnGetAcceptExSockAddrs(in_pIO_Context->m_wsaBuf.buf, in_pIO_Context->m_wsaBuf.len - ((sizeof(SOCKADDR_IN) + 16) * 2),
		sizeof(SOCKADDR_IN) + 16, sizeof(SOCKADDR_IN) + 16, (LPSOCKADDR*)&LocalAddr, &localLen, (LPSOCKADDR*)&ClientAddr, &remoteLen);
	
	printf("RemoteClient ip==%s,port==%d\n", inet_ntoa(ClientAddr->sin_addr), ntohs(ClientAddr->sin_port));
	printf("LocalClient ip==%s,port==%d\n", inet_ntoa(LocalAddr->sin_addr), ntohs(LocalAddr->sin_port));
	
	//将连入的远端socket添加到完成端口用于监听
	SPER_SOCKET_CONTEXT* pNewSocketContext = new SPER_SOCKET_CONTEXT;
	//this socket is remote socket
	pNewSocketContext->m_Socket = in_pIO_Context->m_sockAccept;
	memcpy(&(pNewSocketContext->m_ClientAddr), ClientAddr, sizeof(SOCKADDR_IN));
	
	// remote socket set in completionport
	HANDLE hTemp = CreateIoCompletionPort((HANDLE)pNewSocketContext->m_Socket, m_hIOCompletionPort, (DWORD)pNewSocketContext, 0);
	if (NULL == hTemp)
	{
		printf("CreateIoCompletionPort 远端失败\n");
	}
	
	//create IoContext，post remote socket RECV_POSTED 
	SIO_CONTEXT* pNewIoContext = pNewSocketContext->GetSIOContext();
	
	pNewIoContext->m_OpType = RECV_POSTED;
	pNewIoContext->m_sockAccept = pNewSocketContext->m_Socket;
	
	//投递WSARecv请求  
	DWORD dwFlags = 0;
	DWORD dwBytes = 0;
	WSABUF *p_wbuf = &pNewIoContext->m_wsaBuf;
	OVERLAPPED *p_ol = &pNewIoContext->m_Overlapped;
	//
	int nBytesRecv = WSARecv(pNewIoContext->m_sockAccept, p_wbuf, 1, &dwBytes, &dwFlags, p_ol, NULL);
	// 如果返回值错误，并且错误的代码并非是Pending的话，那就说明这个重叠请求失败了  
	if ((SOCKET_ERROR == nBytesRecv) && (WSA_IO_PENDING != WSAGetLastError()))
	{
		printf("WSARecv faile\n");
	}
	//把这个有效的客户端信息，加入到ContextList中去(需要统一管理，方便释放资源)
	AddToContextList(pNewSocketContext);
	//使用完毕之后，把Listen Socket的那个IoContext重置，然后准备投递新的AcceptEx
	in_pIO_Context->ResetBuffer();
	PostAccept(in_pIO_Context);
}
void CCompleteIO::AddToContextList(SPER_SOCKET_CONTEXT *pSocketContext)
{
	EnterCriticalSection(&m_csLock);
	m_arrayClientContext.Add(pSocketContext);
	LeaveCriticalSection(&m_csLock);
}
bool CCompleteIO::PostAccept(SIO_CONTEXT *in_pIo_Context)
{
	printf("PostAccept\n");
	//创建Accept需要的Socket用于接收连接的客户端
	in_pIo_Context->m_sockAccept = WSASocket(AF_INET,SOCK_STREAM,IPPROTO_TCP,NULL,0, WSA_FLAG_OVERLAPPED);
	if (in_pIo_Context->m_sockAccept == INVALID_SOCKET)
	{
		printf("PostAccept#WSASocket faile\n");
	}
	
	in_pIo_Context->m_OpType = ACCEPT_POSTED;
	
	WSABUF *p_wbuf = &in_pIo_Context->m_wsaBuf;
	OVERLAPPED *p_ol = &in_pIo_Context->m_Overlapped;

	DWORD dwBytes = 0;
	//结构体要初始化，不然会返回6，句柄无效
	//传递OVERLAPPED指针，用于在GetQueueCompletionPort时获取结构体成员
	if (FALSE == m_lpfnAcceptEx(m_pListenContext->m_Socket, in_pIo_Context->m_sockAccept, p_wbuf->buf, p_wbuf->len - ((sizeof(SOCKADDR_IN) + 16) * 2),
		(sizeof(SOCKADDR_IN) + 16), (sizeof(SOCKADDR_IN) + 16), &dwBytes, p_ol))
	{
		int ierr = WSAGetLastError();
		if (WSA_IO_PENDING != WSAGetLastError())
		{
			printf("AcceptEx fiale %d\n",ierr);
		}
		
	}

	return true;
}
bool CCompleteIO::InitSocket(char *in_ip, int in_port)
{
	printf("Ip=%s,Port=%d\n", in_ip, in_port);
	// 生成用于监听的Socket的信息  
	m_pListenContext = new SPER_SOCKET_CONTEXT;
	// 需要使用重叠IO，必须得使用WSASocket来建立Socket，才可以支持重叠IO操作  
	m_pListenContext->m_Socket = WSASocket(AF_INET, SOCK_STREAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED);
	if (m_pListenContext->m_Socket == INVALID_SOCKET)
	{
		printf("WSASocket faile\n");
	}
	// 将Listen Socket绑定至完成端口中  
	if (NULL == CreateIoCompletionPort(HANDLE(m_pListenContext->m_Socket), m_hIOCompletionPort, (DWORD)m_pListenContext, 0))
	{
		printf("CreateIoCompletionPort Listen Faile\n");
	}

	struct sockaddr_in sock_addr;
	ZeroMemory((char *)&sock_addr, sizeof(sockaddr_in));
	sock_addr.sin_family = AF_INET;
	sock_addr.sin_addr.S_un.S_addr = htonl(INADDR_ANY);
	//sock_addr.sin_addr.S_un.S_addr = inet_addr(in_ip);
	sock_addr.sin_port = htons(in_port);

	int ierr = bind(m_pListenContext->m_Socket, (const sockaddr*)&sock_addr, sizeof(sockaddr_in));
	if (ierr == SOCKET_ERROR)
	{
		printf("bind faile\n");
	}
	ierr = listen(m_pListenContext->m_Socket, 5);
	if (ierr == SOCKET_ERROR)
	{
		printf("listen faile\n");
	}
	GUID GuidAcceptEx = WSAID_ACCEPTEX;
	GUID GuidGetAcceptExSockAddrs = WSAID_GETACCEPTEXSOCKADDRS;

	// 获取AcceptEx函数指针  
	DWORD dwBytes = 0;
	if (SOCKET_ERROR == WSAIoctl(
		m_pListenContext->m_Socket,
		SIO_GET_EXTENSION_FUNCTION_POINTER,
		&GuidAcceptEx,
		sizeof(GuidAcceptEx),
		&m_lpfnAcceptEx,
		sizeof(m_lpfnAcceptEx),
		&dwBytes,
		NULL,
		NULL))
	{
		printf("WSAIOCTL acceptex fiale\n");
	}
	if (SOCKET_ERROR == WSAIoctl(
		m_pListenContext->m_Socket,
		SIO_GET_EXTENSION_FUNCTION_POINTER,
		&GuidGetAcceptExSockAddrs,
		sizeof(GuidGetAcceptExSockAddrs),
		&m_lpfnGetAcceptExSockAddrs,
		sizeof(m_lpfnGetAcceptExSockAddrs),
		&dwBytes,
		NULL,
		NULL))
	{
		printf("WSATOCTL GetAccept faile\n");
	}
	//初始化Accepter需要的socket
	for (int i = 0; i < MAX_ACCEPT; i++)
	{
		SIO_CONTEXT *pContext = m_pListenContext->GetSIOContext();
		PostAccept(pContext);
	}
	return true;
}
bool CCompleteIO::InitCompleteIO(char *in_ip, int in_port)
{
	m_hIOCompletionPort = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0);
	if (m_hIOCompletionPort == NULL)
	{
		printf("CreateIoCompletionPort faile\n");
	}
	DWORD nThreadID;

	m_hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
	
	m_hWorkThread = CreateThread(0, 0, workThread, (void*)this, 0, &nThreadID);

	InitSocket(in_ip, in_port);

	return true;
}
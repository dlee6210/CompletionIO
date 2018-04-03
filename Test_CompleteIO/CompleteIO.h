#pragma once
#include<afxtempl.h>
#include <winsock2.h>  
#include <MSWSock.h>  
#pragma comment(lib,"ws2_32.lib")  
#define MAX_BUFFER_LEN 1024*4
#define _WINSOCK_DEPRECATED_NO_WARNINGS
// 在完成端口上投递的I/O操作的类型  
typedef enum _OPERATION_TYPE
{
	ACCEPT_POSTED,                     // 标志投递的Accept操作  
	SEND_POSTED,                       // 标志投递的是发送操作  
	RECV_POSTED,                       // 标志投递的是接收操作  
	NULL_POSTED                        // 用于初始化，无意义  

}OPERATION_TYPE;

typedef struct SIO_CONTEXT
{
	OVERLAPPED     m_Overlapped;
	SOCKET         m_sockAccept;
	WSABUF         m_wsaBuf;
	char           m_szBuffer[MAX_BUFFER_LEN];
	OPERATION_TYPE m_OpType;
	SIO_CONTEXT()
	{
		ZeroMemory(&m_Overlapped, sizeof(m_Overlapped));
		ZeroMemory(m_szBuffer, MAX_BUFFER_LEN);
		m_sockAccept = INVALID_SOCKET;
		m_wsaBuf.buf = m_szBuffer;
		m_wsaBuf.len = MAX_BUFFER_LEN;
		m_OpType = NULL_POSTED;
	}
	~SIO_CONTEXT()
	{
		if (m_sockAccept != INVALID_SOCKET)
		{
			closesocket(m_sockAccept);
			m_sockAccept = INVALID_SOCKET;
		}
	}
	void ResetBuffer()
	{
		ZeroMemory(m_szBuffer, MAX_BUFFER_LEN);
	}
}sio_context;
typedef struct SPER_SOCKET_CONTEXT
{
	SOCKET      m_Socket;                                  // 每一个客户端连接的Socket  
	SOCKADDR_IN m_ClientAddr;                              // 客户端的地址  
	CArray<SIO_CONTEXT*>m_sio_context;
	
	SPER_SOCKET_CONTEXT()
	{
		m_Socket = INVALID_SOCKET;
		memset(&m_ClientAddr, 0, sizeof(m_ClientAddr));
	}
	~SPER_SOCKET_CONTEXT()
	{
		if (m_Socket != INVALID_SOCKET)
		{
			closesocket(m_Socket);
			m_Socket = INVALID_SOCKET;
		}
	}
	SIO_CONTEXT *GetSIOContext()
	{
		SIO_CONTEXT *io_context = nullptr;
		io_context = new SIO_CONTEXT;
		m_sio_context.Add(io_context);
		return io_context;
	}
	void RemovIOContext(SIO_CONTEXT *in_pIoContext)
	{
		for (int i = 0; i < m_sio_context.GetSize(); i++)
		{
			if (in_pIoContext == m_sio_context.GetAt(i))
			{
				delete in_pIoContext;
				in_pIoContext = nullptr;
				m_sio_context.RemoveAt(i);
				break;
			}
		}
	}
}sper_socket_context;
class CCompleteIO
{
public:
	CCompleteIO();
	~CCompleteIO();
public:
	bool InitCompleteIO(char *in_ip, int in_port);
	bool InitSocket(char *in_ip, int in_port);
	bool PostAccept(SIO_CONTEXT *in_pIo_Context);
	void DoAccept(SPER_SOCKET_CONTEXT *in_pSocket_Context, SIO_CONTEXT *in_pIO_Context);
	void DoRecv(SPER_SOCKET_CONTEXT *in_pSocket_Context, SIO_CONTEXT *in_pIO_Context);
	// 将客户端的相关信息存储到数组中  
	void AddToContextList(SPER_SOCKET_CONTEXT *pSocketContext);

	static DWORD WINAPI workThread(LPVOID param);
	HANDLE                       m_hIOCompletionPort;           // 完成端口的句柄  
private:
	CRITICAL_SECTION			 m_csLock;
	CArray<SPER_SOCKET_CONTEXT*>  m_arrayClientContext;          // 客户端Socket的Context信息     
	SPER_SOCKET_CONTEXT*         m_pListenContext;              // 用于监听的Socket的Context信息 
	HANDLE						 m_hWorkThread;
	HANDLE						 m_hEvent;
	SOCKET	m_SocketListen;
	LPFN_ACCEPTEX                m_lpfnAcceptEx;                // AcceptEx 和 GetAcceptExSockaddrs 的函数指针，用于调用这两个扩展函数  
	LPFN_GETACCEPTEXSOCKADDRS    m_lpfnGetAcceptExSockAddrs;
};


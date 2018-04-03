#pragma once
#include<afxtempl.h>
#include <winsock2.h>  
#include <MSWSock.h>  
#pragma comment(lib,"ws2_32.lib")  
#define MAX_BUFFER_LEN 1024*4
#define _WINSOCK_DEPRECATED_NO_WARNINGS
// ����ɶ˿���Ͷ�ݵ�I/O����������  
typedef enum _OPERATION_TYPE
{
	ACCEPT_POSTED,                     // ��־Ͷ�ݵ�Accept����  
	SEND_POSTED,                       // ��־Ͷ�ݵ��Ƿ��Ͳ���  
	RECV_POSTED,                       // ��־Ͷ�ݵ��ǽ��ղ���  
	NULL_POSTED                        // ���ڳ�ʼ����������  

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
	SOCKET      m_Socket;                                  // ÿһ���ͻ������ӵ�Socket  
	SOCKADDR_IN m_ClientAddr;                              // �ͻ��˵ĵ�ַ  
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
	// ���ͻ��˵������Ϣ�洢��������  
	void AddToContextList(SPER_SOCKET_CONTEXT *pSocketContext);

	static DWORD WINAPI workThread(LPVOID param);
	HANDLE                       m_hIOCompletionPort;           // ��ɶ˿ڵľ��  
private:
	CRITICAL_SECTION			 m_csLock;
	CArray<SPER_SOCKET_CONTEXT*>  m_arrayClientContext;          // �ͻ���Socket��Context��Ϣ     
	SPER_SOCKET_CONTEXT*         m_pListenContext;              // ���ڼ�����Socket��Context��Ϣ 
	HANDLE						 m_hWorkThread;
	HANDLE						 m_hEvent;
	SOCKET	m_SocketListen;
	LPFN_ACCEPTEX                m_lpfnAcceptEx;                // AcceptEx �� GetAcceptExSockaddrs �ĺ���ָ�룬���ڵ�����������չ����  
	LPFN_GETACCEPTEXSOCKADDRS    m_lpfnGetAcceptExSockAddrs;
};


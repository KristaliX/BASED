
// ServerDlg.cpp : implementation file
//

#include "stdafx.h"
#include "Server.h"
#include "ServerDlg.h"
#include "afxdialogex.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

#include <winsock2.h>

#define PORT 5150			// Порт по умолчанию
#define DATA_BUFSIZE 8192 	// Размер буфера по умолчанию

int  iPort = PORT; 	 // Порт для прослушивания подключений
bool bPrint = false; // Выводить ли сообщения клиентов

typedef struct _SOCKET_INFORMATION {
	BOOL RecvPosted;
	CHAR Buffer[DATA_BUFSIZE];
	WSABUF DataBuf;
	SOCKET Socket;
	DWORD BytesSEND;
	DWORD BytesRECV;
	_SOCKET_INFORMATION *Next;
} SOCKET_INFORMATION, *LPSOCKET_INFORMATION;

#define WM_SOCKET (WM_USER + 1)  // Сообщение о событии

void CreateSocketInformation(SOCKET s, char *Str,
	CListBox *pLB);
LPSOCKET_INFORMATION GetSocketInformation(SOCKET s,
	char *Str, CListBox *pLB);
void FreeSocketInformation(SOCKET s, char *Str,
	CListBox *pLB);

HWND MakeWorkerWindow(char *Str, CListBox *pLB);
LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg,
	WPARAM wParam, LPARAM lParam);

LPSOCKET_INFORMATION SocketInfoList;

HWND   hWnd_LB;  // Для вывода в других потоках

UINT ListenThread(PVOID lpParam);
int GetNumClients();
bool CheckS();
void SendAll(HWND _hwnd);
SOCKET GetSocket(SOCKET sender, SOCKET& prev);

int GetNumClients() {
	LPSOCKET_INFORMATION temp = NULL;
	temp = SocketInfoList;
	int cnt = 0;
	while (temp) {
		cnt++;
		temp = temp->Next;
	}
	return cnt;
}

bool CheckS() {
	LPSOCKET_INFORMATION temp = NULL;
	temp = SocketInfoList;
	int n = 0;
	while (temp) {
		if (temp->BytesRECV == 0)
			n++;
		temp = temp->Next;
	}
	if (n == 0) return true;
	else return false;
}

SOCKET GetSocket(SOCKET sender, SOCKET& prev) {
	LPSOCKET_INFORMATION temp = NULL;
	temp = SocketInfoList;
	int i = 0;
	while (temp) {
		i++;
		if (temp->Socket != sender && temp->Socket != prev) {
			prev = temp->Socket;
			break;
		}


		else
			temp = temp->Next;
	}
	return temp->Socket;
}

void SendAll(HWND _hwnd) {
	LPSOCKET_INFORMATION temp = NULL;
	temp = SocketInfoList;
	while (temp) {
		PostMessage(_hwnd, WM_SOCKET, temp->Socket, FD_WRITE);
		temp = temp->Next;

	}
}

// CServerDlg dialog

CServerDlg::CServerDlg(CWnd* pParent /*=nullptr*/)
	: CDialogEx(IDD_SERVER_DIALOG, pParent)
{
	m_hIcon = AfxGetApp()->LoadIcon(IDR_MAINFRAME);
}

void CServerDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
	DDX_Control(pDX, IDC_LISTBOX, m_ListBox);
}

BEGIN_MESSAGE_MAP(CServerDlg, CDialogEx)
	ON_WM_PAINT()
	ON_WM_QUERYDRAGICON()
	ON_BN_CLICKED(IDC_START, &CServerDlg::OnBnClickedStart)
	ON_BN_CLICKED(IDC_PRINT, &CServerDlg::OnBnClickedPrint)
END_MESSAGE_MAP()


// CServerDlg message handlers

BOOL CServerDlg::OnInitDialog()
{
	CDialogEx::OnInitDialog();

	// Set the icon for this dialog.  The framework does this automatically
	//  when the application's main window is not a dialog
	SetIcon(m_hIcon, TRUE);			// Set big icon
	SetIcon(m_hIcon, FALSE);		// Set small icon

	// TODO: Add extra initialization here
	char Str[128];

	sprintf_s(Str, sizeof(Str), "%d", iPort);
	GetDlgItem(IDC_PORT)->SetWindowText(Str);

	if (bPrint)
		((CButton *)GetDlgItem(IDC_PRINT))->SetCheck(1);
	else
		((CButton *)GetDlgItem(IDC_PRINT))->SetCheck(0);

	return TRUE;  // return TRUE  unless you set the focus to a control
}

// If you add a minimize button to your dialog, you will need the code below
//  to draw the icon.  For MFC applications using the document/view model,
//  this is automatically done for you by the framework.

void CServerDlg::OnPaint()
{
	if (IsIconic())
	{
		CPaintDC dc(this); // device context for painting

		SendMessage(WM_ICONERASEBKGND, reinterpret_cast<WPARAM>(dc.GetSafeHdc()), 0);

		// Center icon in client rectangle
		int cxIcon = GetSystemMetrics(SM_CXICON);
		int cyIcon = GetSystemMetrics(SM_CYICON);
		CRect rect;
		GetClientRect(&rect);
		int x = (rect.Width() - cxIcon + 1) / 2;
		int y = (rect.Height() - cyIcon + 1) / 2;

		// Draw the icon
		dc.DrawIcon(x, y, m_hIcon);
	}
	else
	{
		CDialogEx::OnPaint();
	}
}

// The system calls this function to obtain the cursor to display while the user drags
//  the minimized window.
HCURSOR CServerDlg::OnQueryDragIcon()
{
	return static_cast<HCURSOR>(m_hIcon);
}



void CServerDlg::OnBnClickedStart()
{
	// TODO: Add your control notification handler code here
	char Str[81];

	hWnd_LB = m_ListBox.m_hWnd;  // Для ListenThread
	GetDlgItem(IDC_PORT)->GetWindowText(Str, sizeof(Str));
	iPort = atoi(Str);
	if (iPort <= 0 || iPort >= 0x10000)
	{
		AfxMessageBox("Incorrect Port number");
		return;
	}

	AfxBeginThread(ListenThread, NULL);

	GetDlgItem(IDC_START)->EnableWindow(false);
}

UINT ListenThread(PVOID lpParam)
{
	MSG msg;
	DWORD Ret;
	SOCKET Listen;
	SOCKADDR_IN InternetAddr;
	HWND Window;
	WSADATA wsaData;

	char  Str[200];
	CListBox  *pLB =
		(CListBox *)(CListBox::FromHandle(hWnd_LB));

	if ((Window = MakeWorkerWindow(Str, pLB)) == NULL)
		return 1;

	if ((Ret = WSAStartup(0x0202, &wsaData)) != 0)
	{
		sprintf_s(Str, sizeof(Str),
			"WSAStartup failed with error %d", Ret);
		pLB->AddString(Str);
		return 1;
	}

	if ((Listen = socket(PF_INET, SOCK_STREAM, 0)) ==
		INVALID_SOCKET)
	{
		sprintf_s(Str, sizeof(Str),
			"socket() failed with error %d",
			WSAGetLastError());
		pLB->AddString(Str);
		return 1;
	}

	WSAAsyncSelect(Listen, Window, WM_SOCKET,
		FD_ACCEPT | FD_CLOSE);

	InternetAddr.sin_family = AF_INET;
	InternetAddr.sin_addr.s_addr = htonl(INADDR_ANY);
	InternetAddr.sin_port = htons(iPort);

	if (bind(Listen, (PSOCKADDR)&InternetAddr,
		sizeof(InternetAddr)) == SOCKET_ERROR)
	{
		sprintf_s(Str, sizeof(Str),
			"bind() failed with error %d",
			WSAGetLastError());
		pLB->AddString(Str);
		return 1;
	}

	if (listen(Listen, 5))
	{
		sprintf_s(Str, sizeof(Str),
			"listen() failed with error %d",
			WSAGetLastError());
		pLB->AddString(Str);
		return 1;
	}

	while (Ret = GetMessage(&msg, NULL, 0, 0))
	{
		if (Ret == -1)
		{
			sprintf_s(Str, sizeof(Str),
				"GetMessage() failed with error %d",
				GetLastError());
			pLB->AddString(Str);
			return 1;
		}
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}
	return 0;
}

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg,
	WPARAM wParam, LPARAM lParam)
{
	SOCKET Accept;
	LPSOCKET_INFORMATION SocketInfo;
	DWORD RecvBytes, SendBytes;
	DWORD Flags;

	char  Str[200];
	CListBox  *pLB =
		(CListBox *)(CListBox::FromHandle(hWnd_LB));

	if (uMsg == WM_SOCKET)
	{
		if (WSAGETSELECTERROR(lParam))
		{
			sprintf_s(Str, sizeof(Str),
				"Socket failed with error %d",
				WSAGETSELECTERROR(lParam));
			pLB->AddString(Str);
			FreeSocketInformation(wParam, Str, pLB);
		}
		else
		{
			switch (WSAGETSELECTEVENT(lParam))
			{
			case FD_ACCEPT:
				if ((Accept = accept(wParam, NULL, NULL)) ==
					INVALID_SOCKET)
				{
					sprintf_s(Str, sizeof(Str),
						"accept() failed with error %d",
						WSAGetLastError());
					pLB->AddString(Str);
					break;
				}
				if (GetNumClients() < 3) {

					// Создание структуры с информацией о сокете
					// и занесение ее в список
					CreateSocketInformation(Accept, Str, pLB);
					sprintf_s(Str, sizeof(Str),
						"Socket number %d connected", Accept);
					pLB->AddString(Str);
					WSAAsyncSelect(Accept, hwnd, WM_SOCKET, FD_READ | FD_WRITE | FD_CLOSE);
					send(Accept, "1", 2, 0);
				}
				else {
					send(Accept, "0", 2, 0);
					closesocket(Accept);
				}
				break;
			case FD_READ:

				SocketInfo =
					GetSocketInformation(wParam, Str, pLB);

				// Читаем только если буфер приема пуст
				if (SocketInfo->BytesRECV != 0)
				{
					SocketInfo->RecvPosted = TRUE;
					return 0;
				}
				else
				{
					SocketInfo->DataBuf.buf =
						SocketInfo->Buffer;
					SocketInfo->DataBuf.len = DATA_BUFSIZE;

					Flags = 0;
					if (WSARecv(SocketInfo->Socket,
						&(SocketInfo->DataBuf), 1,
						&RecvBytes,
						&Flags, NULL, NULL) ==
						SOCKET_ERROR)
					{
						if (WSAGetLastError() !=
							WSAEWOULDBLOCK)
						{
							sprintf_s(Str, sizeof(Str),
								"WSARecv() failed with"
								" error %d",
								WSAGetLastError());
							pLB->AddString(Str);
							FreeSocketInformation(wParam,
								Str, pLB);
							return 0;
						}
					}
					else // OK, изменяем счетчик байтов
					{
						SocketInfo->BytesRECV = RecvBytes;
						// Вывод сообщения
						if (bPrint)
						{
							unsigned l = sizeof(Str) - 1;
							if (l > RecvBytes)
								l = RecvBytes;
							sprintf_s(Str, sizeof(Str),
								SocketInfo->Buffer, l);
							Str[l] = 0;
							pLB->AddString(Str);
						}
					}
				}
				// ЗДЕСЬ НЕТ ОПЕРАТОРА break, поскольку мы 
				// получили данные. Продолжаем работу и
				// начинаем их отправку клиенту.
				if (GetNumClients() == 3 && CheckS()) {
					SendAll(hwnd);
				}
				break;

			case FD_WRITE:

				SocketInfo = GetSocketInformation(
					wParam, Str, pLB);

				if (SocketInfo->BytesRECV >
					SocketInfo->BytesSEND)
				{
					SocketInfo->DataBuf.buf =
						SocketInfo->Buffer +
						SocketInfo->BytesSEND;
					SocketInfo->DataBuf.len =
						SocketInfo->BytesRECV -
						SocketInfo->BytesSEND;
					SOCKET prev = NULL;
					for (int i = 0; i < 2; i++)
						if (WSASend(GetSocket(SocketInfo->Socket, prev),
							&(SocketInfo->DataBuf), 1,
							&SendBytes, 0, NULL, NULL) ==
							SOCKET_ERROR)
					{
						if (WSAGetLastError() !=
							WSAEWOULDBLOCK)
						{
							sprintf_s(Str, sizeof(Str),
								"WSASend() failed"
								" with error %d",
								WSAGetLastError());
							pLB->AddString(Str);
							FreeSocketInformation(wParam,
								Str, pLB);
							return 0;
						}
					}
					else // OK, изменяем счетчик байтов
					{
						SocketInfo->BytesSEND += SendBytes;
					}
				}

				if (SocketInfo->BytesSEND ==
					SocketInfo->BytesRECV)
				{
					SocketInfo->BytesSEND = 0;
					SocketInfo->BytesRECV = 0;

					// Если пришли данные пока мы занимались 
					// отправкой, надо послать на сокет
					// уведомление FD_READ.

					if (SocketInfo->RecvPosted == TRUE)
					{
						SocketInfo->RecvPosted = FALSE;
						PostMessage(hwnd, WM_SOCKET,
							wParam, FD_READ);
					}
				}

				break;

			case FD_CLOSE:

				sprintf_s(Str, sizeof(Str),
					"Closing socket %d", wParam);
				pLB->AddString(Str);
				FreeSocketInformation(wParam, Str, pLB);

				break;
			}
		}
		return 0;
	}

	return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

void CreateSocketInformation(SOCKET s, char *Str,
	CListBox *pLB)
{
	LPSOCKET_INFORMATION SI;

	if ((SI = (LPSOCKET_INFORMATION)GlobalAlloc(GPTR,
		sizeof(SOCKET_INFORMATION))) == NULL)
	{
		sprintf_s(Str, sizeof(Str),
			"GlobalAlloc() failed with error %d",
			GetLastError());
		pLB->AddString(Str);
		return;
	}

	// Подготовка структуры для использования.

	SI->Socket = s;
	SI->RecvPosted = FALSE;
	SI->BytesSEND = 0;
	SI->BytesRECV = 0;

	SI->Next = SocketInfoList;

	SocketInfoList = SI;
}

LPSOCKET_INFORMATION GetSocketInformation(SOCKET s,
	char *Str, CListBox *pLB)
{
	SOCKET_INFORMATION *SI = SocketInfoList;
	while (SI)
	{
		if (SI->Socket == s)
			return SI;
		SI = SI->Next;
	}
	return NULL;
}

void FreeSocketInformation(SOCKET s, char *Str,
	CListBox *pLB)
{
	SOCKET_INFORMATION *SI = SocketInfoList;
	SOCKET_INFORMATION *PrevSI = NULL;

	while (SI)
	{
		if (SI->Socket == s)
		{
			if (PrevSI)
				PrevSI->Next = SI->Next;
			else
				SocketInfoList = SI->Next;

			closesocket(SI->Socket);
			GlobalFree(SI);
			return;
		}
		PrevSI = SI;
		SI = SI->Next;
	}
}

HWND MakeWorkerWindow(char *Str, CListBox *pLB)
{
	WNDCLASS wndclass;
	CHAR *ProviderClass = "AsyncSelect";
	HWND Window;

	wndclass.style = CS_HREDRAW | CS_VREDRAW;
	wndclass.lpfnWndProc = (WNDPROC)WindowProc;
	wndclass.cbClsExtra = 0;
	wndclass.cbWndExtra = 0;
	wndclass.hInstance = NULL;
	wndclass.hIcon = LoadIcon(NULL, IDI_APPLICATION);
	wndclass.hCursor = LoadCursor(NULL, IDC_ARROW);
	wndclass.hbrBackground =
		(HBRUSH)GetStockObject(WHITE_BRUSH);
	wndclass.lpszMenuName = NULL;
	wndclass.lpszClassName = ProviderClass;

	if (RegisterClass(&wndclass) == 0)
	{
		sprintf_s(Str, sizeof(Str),
			"RegisterClass() failed with error %d",
			GetLastError());
		pLB->AddString(Str);
		return NULL;
	}

	// Собственно создание окна.
	if ((Window = CreateWindow(
		ProviderClass,
		"",
		WS_OVERLAPPEDWINDOW,
		CW_USEDEFAULT,
		CW_USEDEFAULT,
		CW_USEDEFAULT,
		CW_USEDEFAULT,
		NULL,
		NULL,
		NULL,
		NULL)) == NULL)
	{
		sprintf_s(Str, sizeof(Str),
			"CreateWindow() failed with error %d",
			GetLastError());
		pLB->AddString(Str);
		return NULL;
	}

	return Window;
}


void CServerDlg::OnBnClickedPrint()
{
	// TODO: Add your control notification handler code here
	if (((CButton *)GetDlgItem(IDC_PRINT))->GetCheck() == 1)
		bPrint = true;
	else
		bPrint = false;
}

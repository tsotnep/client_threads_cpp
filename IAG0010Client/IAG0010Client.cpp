#include "stdafx.h"

// Global
TCHAR CommandBuf[81];
HANDLE hCommandGot;			 // event "the user has typed a command"
HANDLE hStopCommandGot;		 // event "the main thread has recognized that it was the stop command"
HANDLE hCommandProcessed;    // event "the main thread has finished the processing of command"
HANDLE hReadKeyboard;        // keyboard reading thread handle
HANDLE hStdIn;			     // stdin standard input stream handle
HANDLE hReceiveNet;          // TCP/IP info reading thread handle
HANDLE hSendNet;		     // TCP/IP info sending thread handle
HANDLE hSendmsgHelloServer;  // event for shaking hands
HANDLE hSendmsgReady;	     // event to send server Ready message, to let it know client is ready to receive packet
HANDLE hFile;                //handle for file
BOOL handshakeflag;          //Flag for determining weather or not handshake happened
WSADATA WsaData;             // filled during Winsock initialization 
DWORD Error;
SOCKET hClientSocket = INVALID_SOCKET;
sockaddr_in ClientSocketInfo;
BOOL SocketError;
_TCHAR * fileName;

// Prototypes
void processReceivedData(int nReceivedBytes, WSABUF DataBuf);
unsigned int __stdcall ReadKeyboard(void* pArguments);
unsigned int __stdcall ReceiveNet(void* pArguments);
unsigned int __stdcall SendNet(void* pArguments);


//****************************************************************************************************************
//	MAIN THREAD
int _tmain(int argc, _TCHAR* argv[])
{
begin:
	handshakeflag=false;
	
	//
	// Initializations for multithreading
	//
	if (!(hCommandGot = CreateEvent(NULL, TRUE, FALSE, NULL)) ||
		!(hStopCommandGot = CreateEvent(NULL, TRUE, FALSE, NULL)) ||  
		!(hCommandProcessed = CreateEvent(NULL, TRUE, TRUE, NULL))||
		!(hSendmsgHelloServer = CreateEvent(NULL, TRUE, FALSE, NULL))||
		!(hSendmsgReady = CreateEvent(NULL, TRUE, FALSE, NULL)))
	{
		_tprintf(_T("CreateEvent() failed, error %d\n"), GetLastError());
		return 1;
	}
	//
	// Prepare keyboard, start the thread
	//
	hStdIn = GetStdHandle(STD_INPUT_HANDLE);
	if (hStdIn == INVALID_HANDLE_VALUE)
	{
		_tprintf(_T("GetStdHandle() failed, error %d\n"), GetLastError());
		return 1;
	}
	if (!SetConsoleMode(hStdIn, ENABLE_LINE_INPUT | ENABLE_ECHO_INPUT | ENABLE_PROCESSED_INPUT))
	{
		_tprintf(_T("SetConsoleMode() failed, error %d\n"), GetLastError());
		return 1;
	}
	if (!(hReadKeyboard = (HANDLE)_beginthreadex(NULL, 0, &ReadKeyboard, NULL, 0, NULL)))
	{ 
		_tprintf(_T("Unable to create keyboard thread\n"));
		return 1;
	}
		
	// Create file for writing with the provided name, if not provided then with defined name

	if (argv[1]!=NULL)
	{
		hFile = CreateFile(argv[1], GENERIC_READ | GENERIC_WRITE, 0, NULL,
		CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL); 
		fileName=argv[1];
	}
	else
	{
		hFile = CreateFile(_T("TTU_pohikiri_2008_clients.doc"), GENERIC_READ | GENERIC_WRITE, 0, NULL,
		CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL); 
		fileName=_T("TTU_pohikiri_2008_clients.doc");
	}
	if (hFile == INVALID_HANDLE_VALUE)
	{
		_tprintf(_T("Unable to create file, ERROR: %d\n"), GetLastError());
		SetEvent(hStopCommandGot);
	} else {
		_tprintf(_T("File has been created\n")); 
	}

	//
	// Initializations for socket
	//
	if (Error = WSAStartup(MAKEWORD(2, 0), &WsaData)) // Initialize Windows socket support
	{
		_tprintf(_T("WSAStartup() failed, ERROR: %d\n"), Error);
		SocketError = TRUE;
	}
	else if ((hClientSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) == INVALID_SOCKET)
	{
		_tprintf(_T("socket() failed, ERROR: %d\n"), WSAGetLastError());
		SocketError = TRUE;
	}
	//
	// Connect client to server
	//
	if (!SocketError)
	{
		ClientSocketInfo.sin_family = AF_INET;
		ClientSocketInfo.sin_addr.s_addr = inet_addr("127.0.0.1");
		ClientSocketInfo.sin_port = htons(1234);  // port number is selected just for example
		if (connect(hClientSocket, (SOCKADDR*) &ClientSocketInfo, sizeof(ClientSocketInfo)) == SOCKET_ERROR)
		{ 
			_tprintf(_T("Unable to connect to server, ERROR: %d\n"), WSAGetLastError());
			SocketError = TRUE;
		}
	}

	//
	// Start net thread
	//
	if (!SocketError)
	{
		if (!(hReceiveNet= (HANDLE)_beginthreadex(NULL, 0, &ReceiveNet, NULL, 0, NULL)))
		{
			_tprintf(_T("Unable to create socket receiving thread\n"));
			goto out;
		}
	
		if(!(hSendNet = (HANDLE)_beginthreadex(NULL, 0, &SendNet, NULL, 0, NULL)))
		{
			_tprintf(_T("Unable to create socket sending thread\n"));
			goto out;
		}
	}

	// Main processing loop
	while (TRUE)
	{
		if (WaitForSingleObject(hCommandGot, INFINITE) != WAIT_OBJECT_0)
		{ // Wait until the command has arrived (i.e. until CommandGot is signaled)
			_tprintf(_T("WaitForSingleObject() failed, error %d\n"), GetLastError());
			goto out;
		}
		ResetEvent(hCommandGot); // CommandGot back to unsignaled
		if (!_tcsicmp(CommandBuf, _T("exit"))) // Case-insensitive comparation
		{
			SetEvent(hStopCommandGot); // To force the other threads to quit
			break;
		}
		else if (!_tcsicmp(CommandBuf, _T("reset"))) // Case-insensitive comparation
		{
			//_tmain(1,&fileName);
			SetEvent(hStopCommandGot); // 
			_tprintf(_T("Client restarting."));
			if (hReadKeyboard)
			{
				WaitForSingleObject(hReadKeyboard, INFINITE); // Wait until the end of keyboard thread
				CloseHandle(hReadKeyboard);
			}
			if (hReceiveNet)
			{
				WaitForSingleObject(hReceiveNet, INFINITE); // Wait until the end of receive thread
				if (hReceiveNet != "0x00000000")
						{
							_tprintf(_T("/n hReceiveNet were already closed, ERROR: %d\n"), GetLastError());
							//SetEvent(hStopCommandGot);
						}
				else CloseHandle(hReceiveNet);
			}
			if (hSendNet)
			{
				WaitForSingleObject(hSendNet, INFINITE);
				if (hSendNet != "0x00000000")
						{
							_tprintf(_T("/n hSendNet were already closed, ERROR: %d\n"), GetLastError());
							//SetEvent(hStopCommandGot);
						}
				else CloseHandle(hSendNet);
			}
			if (hClientSocket != INVALID_SOCKET)
			{
				if (shutdown(hClientSocket, SD_RECEIVE) == SOCKET_ERROR)
				{
					if ((Error = WSAGetLastError()) != WSAENOTCONN) // WSAENOTCONN means that the connection was not established,
																	// so the shut down was senseless
						_tprintf(_T("restarting() failed, error %d\n"), WSAGetLastError());
				}
				closesocket(hClientSocket);
			}
			WSACleanup(); // clean Windows sockets support
			CloseHandle(hStopCommandGot);
			CloseHandle(hCommandGot);
			CloseHandle(hCommandProcessed);	
			CloseHandle(hFile);
			//CloseHandle(hSendmsgHelloServer);
			//CloseHandle(hSendmsgReady);
			//_tprintf(_T("I was here %s\n"), fileName);
			//_tmain(1,&fileName);
			SocketError=FALSE;
			goto begin;

			
		}
		else
		{
			_tprintf(_T("Command \"%s\" not recognized\n"), CommandBuf);
			SetEvent(hCommandProcessed); // To allow the keyboard reading thread to continue
		}
	}



	//
	// Shut down
	//
out:
	_tprintf(_T("Client shutting down."));
	if (hReadKeyboard)
	{
		WaitForSingleObject(hReadKeyboard, INFINITE); // Wait until the end of keyboard thread
		CloseHandle(hReadKeyboard);
	}
	if (hReceiveNet)
	{
		WaitForSingleObject(hReceiveNet, INFINITE); // Wait until the end of receive thread
		CloseHandle(hReceiveNet);
	}
	if (hSendNet)
	{
		WaitForSingleObject(hSendNet, INFINITE);
		CloseHandle(hSendNet);
	}
	if (hClientSocket != INVALID_SOCKET)
	{
		if (shutdown(hClientSocket, SD_RECEIVE) == SOCKET_ERROR)
		{
			if ((Error = WSAGetLastError()) != WSAENOTCONN) // WSAENOTCONN means that the connection was not established,
															// so the shut down was senseless
				_tprintf(_T("shutdown() failed, error %d\n"), WSAGetLastError());
		}
		closesocket(hClientSocket);
	}
	WSACleanup(); // clean Windows sockets support
	CloseHandle(hStopCommandGot);
	CloseHandle(hCommandGot);
	CloseHandle(hCommandProcessed);	
	//CloseHandle(hSendmsgHelloServer);		6E	//CloseHandle(hSendmsgReady);
	return 0;
}


//**************************************************************************************************************
//	KEYBOARD READING THREAD
unsigned int __stdcall ReadKeyboard(void* pArguments)
{
	DWORD nReadChars;
	HANDLE KeyboardEvents[3];	 
	KeyboardEvents[0] = hStopCommandGot;	
	KeyboardEvents[1] = hCommandProcessed;
	DWORD WaitResult;
	//
	// Reading loop
	//
	while (TRUE)
	{
		WaitResult = WaitForMultipleObjects(2, KeyboardEvents,
			FALSE, // wait until one of the events becomes signaled
			INFINITE);
		   // Waiting until hCommandProcessed or hStopCommandGot becomes signaled. Initially hCommandProcessed
		   // is signaled, so at the beginning WaitForMultipleObjects() returns immediately with WaitResult equal
		   // with WAIT_OBJECT_0 + 1.
		if (WaitResult == WAIT_OBJECT_0)
			return 0;  // Stop command, i.e. hStopCommandGot is signaled
		else if (WaitResult == WAIT_OBJECT_0 + 1)
		{ // If the signaled event is hCommandProcessed, the WaitResult is WAIT_OBJECT_0 + 1
			_tprintf(_T("Insert command\n"));
			if (!ReadConsole(hStdIn, CommandBuf, 80, &nReadChars, NULL)) 
			{ // The problem is that when we already are in this function, the only way to leave it
			  // is to type something and then press ENTER. So we cannot step into this function at any moment.
			  // WaitForMultipleObjects() prevents it.
				_tprintf(_T("ReadConsole() failed, error %d\n"), GetLastError()); 
				return 1; 
			} 
			CommandBuf[nReadChars - 2] = 0; // The command in buf ends with "\r\n", we have to get rid of them
			ResetEvent(hCommandProcessed); 
			// Set hCommandProcessed to non-signaled. Therefore WaitForMultipleObjects() blocks the keyboard thread.
			// When the main thread has ended the analyzing of command, it sets hCommandprocessed or hStopCommandGot
			// to signaled and the keyboard thread can continue.
			SetEvent(hCommandGot); 
			// Set hCommandGot event to signaled. Due to that WaitForSingleObject() in the main thread
			// returns, the waiting stops and the analyzing of inserted command may begin
		}
		else
		{	// waiting failed
			_tprintf(_T("WaitForMultipleObjects()failed, error %d\n"), GetLastError());
			return 1;
		}
	}
	return 0;
}


//********************************************************************************************************************
//	TCP/IP INFO RECEIVING THREAD
unsigned int __stdcall ReceiveNet(void* pArguments)
{
	//
	// Preparations
	//
	WSABUF DataBuf;  // Buffer for received data is a structure
	char ArrayInBuf[2048];
	DataBuf.buf = &ArrayInBuf[0];
	DataBuf.len = 2048;
	DWORD nReceivedBytes = 0, ReceiveFlags = 0;
	HANDLE NetEvents[3];
	NetEvents[0] = hStopCommandGot;
	WSAOVERLAPPED Overlapped;
	memset(&Overlapped, 0, sizeof Overlapped);
	Overlapped.hEvent = NetEvents[1] = WSACreateEvent(); // manual and nonsignaled
	DWORD Result, Error;


	//
	// Receiving loop
	//
	while (TRUE)
	{
		Result = WSARecv(hClientSocket,
						  &DataBuf,
						  1,  // no comments here
						  &nReceivedBytes, 
						  &ReceiveFlags, // no comments here
						  &Overlapped, //for asynchronus multithreading
						  NULL);  // no comments here
		if (Result == SOCKET_ERROR)					 
		{  // Returned with socket error, let us examine why
			if ((Error = WSAGetLastError()) != WSA_IO_PENDING)
			{  // Unable to continue, for example because the server has closed the connection
				_tprintf(_T("WSARecv() failed, error %d\n"), Error);
				goto out;
			}
			DWORD WaitResult = WSAWaitForMultipleEvents(2, NetEvents, FALSE, 5000, FALSE); // wait for data
			switch (WaitResult) // analyse why the waiting ended
			{
			case WAIT_OBJECT_0:
				// Waiting stopped because hStopCommandGot has become signaled, i.e. the user has decided to exit
				goto out; 
			case WAIT_OBJECT_0 + 1:

				// Waiting stopped because Overlapped.hEvent is now signaled, i.e. the receiving operation has ended. 
			    // Now we have to see how many bytes we have got.
				WSAResetEvent(NetEvents[1]); // to be ready for the next data package
				if (WSAGetOverlappedResult(hClientSocket, &Overlapped, &nReceivedBytes, FALSE, &ReceiveFlags))
				{
					//Processing of received data
					processReceivedData(nReceivedBytes, DataBuf);
					break;
			    }
			    else
			    {	// Fatal problems
		  		   _tprintf(_T("WSAGetOverlappedResult() failed, ERROR: %d\n"), GetLastError());
				   goto out;
			    }
			case WSA_WAIT_TIMEOUT:
				_tprintf(_T("Timeout. Transfer complete \n"));
				goto out;
			default: // Fatal problems
				_tprintf(_T("WSAWaitForMultipleEvents() failed, ERROR: %d\n"), WSAGetLastError());
				goto out;
			}
		}
		else
		{  // Returned immediately without socket error
			if (!nReceivedBytes)
			{  // When the receiving function has read nothing and returned immediately, the connection is off  
				_tprintf(_T("Server has closed the connection\n"));
				goto out;
			}
			else
			{
			    //processing of received data
				processReceivedData(nReceivedBytes, DataBuf);
			}
		}
	}
out:
	WSACloseEvent(NetEvents[1]);
	return 0;
}


//********************************************************************************************************************
//	TCP/IP INFO SENDING THREAD
unsigned int __stdcall SendNet(void* pArguments) {

	TCHAR msgHello[] = _T("Hello IAG0010Server");
	int msgHelloSize = sizeof(msgHello);
	
	TCHAR msgReady[] = _T("Ready");
	int msgReadySize = sizeof(msgReady);

	HANDLE NetEvents[4];
	NetEvents[0] = hStopCommandGot;
	NetEvents[1] = hSendmsgHelloServer;
	NetEvents[2] = hSendmsgReady;

	WSABUF DataBuf;
	char ArrayInBuf[2048];
	DataBuf.buf = &ArrayInBuf[0];
	DWORD	SendBytes;
	WSAOVERLAPPED SendOverlapped;
	DWORD Flags;
	int err = 0;
	DWORD Result;
	int i = 0;
	boolean MsgSent = false;

	SendOverlapped.hEvent = WSACreateEvent();
		if (SendOverlapped.hEvent == NULL) {
			printf("WSACreateEvent failed with ERROR: %d\n", WSAGetLastError());
			closesocket(hClientSocket);
			return 0;
		}

	while(TRUE) {
		//Wait for event
		DWORD WaitResult = WSAWaitForMultipleEvents(3, NetEvents, FALSE, WSA_INFINITE, FALSE);

		switch (WaitResult) {
		case WAIT_OBJECT_0: //stop command
			goto out;
		case WAIT_OBJECT_0 + 1: //waiting stopped because of handshake 
			_tprintf(_T("Sending handshake response\n"));
			DataBuf.len = msgHelloSize + 4;
			memcpy(DataBuf.buf, &msgHelloSize, 4); 
			memcpy(DataBuf.buf + 4, msgHello, msgHelloSize);
			Result = WSASend(hClientSocket, 
					&DataBuf, 
					1,
					&SendBytes, 
					0, 
					&SendOverlapped, 
					NULL);

			ResetEvent(hSendmsgHelloServer);
			break;
		case WAIT_OBJECT_0 + 2: //waiting stopped because program is ready to receive next packet
			_tprintf(_T("Ready for next packet\n"));
			DataBuf.len = msgReadySize + 4;
			memcpy(DataBuf.buf, &msgReadySize, 4); 
			memcpy(DataBuf.buf + 4, msgReady, msgReadySize);
			Result = WSASend(hClientSocket, 
					&DataBuf, 
					1,
					&SendBytes, 
					0, 
					&SendOverlapped, 
					NULL);
			ResetEvent(hSendmsgReady);
			break;
		default: 
			break;
		}
		if ((Result == SOCKET_ERROR) &&
			(WSA_IO_PENDING != (err = WSAGetLastError()))) {
			printf("WSASend failed with error: %d\n", err);
			return 0;
		}

		Result = WSAWaitForMultipleEvents(1, &SendOverlapped.hEvent, TRUE, INFINITE,
									TRUE);
		if (Result == WSA_WAIT_FAILED) {
			printf("WSAWaitForMultipleEvents failed with error: %d\n",
					WSAGetLastError());
			return 0;
		}

		Result = WSAGetOverlappedResult(hClientSocket, &SendOverlapped, &SendBytes,
									FALSE, &Flags);
		if (Result == FALSE) {
			printf("WSASend failed with error: %d\n", WSAGetLastError());
			return 0;
		}
		_tprintf(_T("'%s' sent.\n"), DataBuf.buf+4);

		WSAResetEvent(SendOverlapped.hEvent);
	}
	out:
	WSACloseEvent(NetEvents[1]);	
	WSACloseEvent(NetEvents[2]);
	return 0;
}


//********************************************************************************************************************
//	PROCESSING DATA
void processReceivedData(int nReceivedBytes, WSABUF DataBuf){
	///check if  received message is for handshake
	if (!handshakeflag) {
		//If yes print in terminal and say Hello
		_tprintf(_T("%d bytes received\n"), nReceivedBytes);
		int length;
		memcpy(&length, DataBuf.buf, 4);

		TCHAR message[2048];
		memcpy(&message, DataBuf.buf + 4, length);

		_tprintf(_T("Message received: %s\n"), message);


		if (wcscmp(message, _T("Hello IAG0010Client")) == 0) {
			SetEvent(hSendmsgHelloServer);
			handshakeflag = TRUE;
		}

	}
	else {
		// If not write received message in file
		int length;
		memcpy(&length, DataBuf.buf, 4);

		BYTE message[2048];
		memcpy(&message, DataBuf.buf + 4, length);

		DWORD written;
		if (!WriteFile(hFile, message, length, &written, NULL))
		{
			_tprintf(_T("Unable to write into file, error %d\n"), GetLastError());
		}
		else {
			_tprintf(_T("%d bytes written into file.\n"), written);
			SetEvent(hSendmsgReady);
		}
	}
}

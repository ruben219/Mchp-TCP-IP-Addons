#define __HTTPC_C
#include "TCPIPConfig.h"

#if defined(STACK_USE_HTTP_CLIENT)

#include "TCPIP Stack/TCPIP.h"
#include "HTTPC.h"

/** This enumeration defines the states of the HTTP client Finite State Machine */
enum HttpcStatesEnum
{
	E_HTTPC_HOME = 0,
	E_HTTPC_SOCKET_OPEN,
	E_HTTPC_SEND_REQUEST,
	E_HTTPC_SEND_HEADERS,
	E_HTTPC_SEND_USRHEADERS,
	E_HTTPC_FINALIZE_HEADERS,
	E_HTTPC_SEND_BODY,
	E_HTTPC_PROCESS_RESPONSE,
	E_HTTPC_SOCKET_CLOSE,
	E_HTTPC_DONE,
};

/** This structure contains bitfields and flags for HTTP client operation and error logging*/
struct HttpcFlags
{
	unsigned char HttpcRomData: 1;
	unsigned char HttpcHdrsReceived: 1; 
	unsigned char HttpcConnClose: 1;
	unsigned char HttpcInUse: 1;
	unsigned char HttpcErrorCode: 4;
};

/** This structure holds the data for the internal mechanisms of the HTTP Client */
static struct HttpClientData
{
	enum HttpcStatesEnum FsmState;					//!< Actual State of the FSM
	DWORD TickValue;								//!< Last system's tick value, for timeouts
	BYTE * NextByte;								//!< Pointer to the next byte to be transmitted (for TCPPutString())
	WORD DataCount;									//!< Used for data transmission and reception counts
	struct HttpcFlags Flags;						//!< Flags used to signal operational conditions and errors
	struct HttpcRequest * pxRequestData;			//!< Pointer to request data
	TCP_SOCKET xSocket;								//!< TCP Socket Handle
	BOOL (*HttpDataCallback)(BYTE *, WORD);			//!< Pointer to the callback function that process received data
} HttpcData = {E_HTTPC_DONE, 0, NULL, 0, {0,0,0}, NULL, INVALID_SOCKET, NULL};

#if CONFIG_HTTPC_COMPILE_PRINTABLE_ERRORS == 1
ROM HttpcFlag[] = "HTTPC> ";

/**
@brief Contains printable error messages
This array contains human readable error messages associated with the error codes defined in the
HttpcErrorsEnum
*/
ROM char * ROM HttpcErrorMsgs[] =
{
	"None",
	"Can't obtain socket",
	"Invalid request data",
	"HTTP Method error",
	"Connection timeout",
	"Inactivity timeout",
	"TX Overflow",
	"RX Overflow",
};

ROM char * ROM HttpGeneralMsgs[] =
{
	"-----------------------------------------",
	"BEGIN REQUEST",
	"REQUEST FINISHED",
	"Client Error: "
	"Receiving Headers",
	"Receiving Body",
};
#endif

/**
@brief Claims the exclusive use of the HTTP Client
This function acts as a semaphore to obtain the exclusive use of the HTTP Client module.
This function should be called before your program uses any of the other API functions. You can only
safely use the HTTP module if this function returns TRUE.
*/
BOOL HttpcBeginUsage()
{
	if(HttpcData.Flags.HttpcInUse)
		return FALSE;
	HttpcData.Flags.HttpcInUse = TRUE;
	return TRUE;
}

/**
@brief Finalizes the use of the HTTP Client module.
You should always call this function when your program finishes and you will no longer use the HTTPC
module. If this function is not called, other programs willing to use the HTTP module will not be
able to use it or the client program will be in an undetermined state.
*/
void HttpcEndUsage()
{
	if(HttpcData.xSocket != INVALID_SOCKET)	// Clean up and preparation for next usage
	{
		TCPDisconnect(HttpcData.xSocket);		// Make sure the socket is disconnected and reference is clear
		HttpcData.xSocket = INVALID_SOCKET;
	}
	HttpcData.FsmState = E_HTTPC_DONE;		// State machine and flag reset
	HttpcData.Flags.HttpcHdrsReceived = 0;
	HttpcData.Flags.HttpcConnClose = 0;
	HttpcData.Flags.HttpcInUse = 0;
	HttpcData.Flags.HttpcErrorCode = 0;
}

/**
@brief Attach a callback function to the HTTP client to process the requested data.
This function sets a user defined function to process incomming data from the HTTP server.
The provided function should return TRUE if it WILL NO longer need the data placed on the buffer by
the HTTP client, if the user's program is waiting for an async operation to complete and the data on
the buffer is still needed, the function should return FALSE and the HTTP client will not copy new
data to the buffer untill the user callback returns TRUE.

If the passed pointer is NULL, the received data will be discarted.
*/
void HttpAttachCallback(BOOL (*HttpcCallback)(BYTE *, WORD))
{
	HttpcData.HttpDataCallback = HttpcCallback;
}

/**
@brief Begins the HTTP request process
This function begins an HTTP request to the server with the parameters passed on the HttpcRequest
structure. HttpBeginUsage must be called before using this function in order to avoid conflicts with
other programs that are using the HTTP client module. See usage examples for further details.
*/
BOOL HttpcRequest(struct HttpcRequest * HttpReqData)
{
	if( HttpReqData != NULL && HttpReqData->Server != NULL && HttpReqData->pcUrl != NULL && HttpReqData->ServerPort != 0 )
	{
		HttpcData.pxRequestData = HttpReqData;		// Iniciar apuntador a estructura de petición
		HttpcData.FsmState = E_HTTPC_HOME;		// Preparar maquina de estados
		return TRUE;
	}
	return FALSE;
}

#if CONFIG_HTTPC_COMPILE_HELPER_FUNCTIONS == 1
/**
@brief Prepares the given HttpcRequest structure for a GET request
*/
void HttpcPrepareGet(struct HttpcRequest * HttpReqData, BYTE * HttpServer, BYTE * HttpUrl, BYTE * HttpHeaders)
{
	HttpcReqData->eMethod = E_HTTP_GET;
	HttpcReqData->Server = HttpServer;
	HttpcReqData->ServerPort = 80;
	HttpcReqData->pcUrl = HttpUrl;
	HttpcReqData->ReqData = NULL;
	HttpcReqData->ReqDataLength = 0;
	HttpcReqData->AdditionalHeaders = HttpHeaders;
}

/**
@brief Prepares the given HttpcRequest for a POST request
*/
void HttpcPreparePost(struct HttpcRequest * HttpReqData, BYTE * HttpServer, BYTE * HttpUrl, BYTE * HttpHeaders, BYTE * HttpData, WORD HttpDataLen)
{
	HttpcReqData->eMethod = E_HTTP_POST;
	HttpcReqData->Server = HttpServer;
	HttpcReqData->ServerPort = 80;
	HttpcReqData->pcUrl = HttpUrl;
	HttpcReqData->ReqData = HttpData;
	HttpcReqData->ReqDataLength = HttpDataLen;
	HttpcReqData->AdditionalHeaders = HttpHeaders;
}
#endif

/**
@brief If an error occurred during HTTP client operation, returns the HTTPC error code.
This function returns an error code different from 0 if an exceptional condition is detected while 
processing an HTTP request. It returns 0 if no errors were detected on the operation of the client.
Please note that the code returned by this function has no relationship with the HTTP Status Codes,
the code returned by this function only contains information about the HTTP Client execution.

@return Returns an error code defined in HttpcErrorsEnum or E_HTTPC_ERROR_NONE (0) if no error was
detected.
*/
enum HttpcErrorsEnum HttpcGetError()
{
	return HttpcData.Flags.HttpcErrorCode;
}

/**
@brief This function checks if HTTP headers have been received completely.
This function checks if the HTTP client has received an empty line after sending the request line
denoting the end of the protocol headers.
*/
BOOL HttpcHeadersReceived()
{
	if(HttpcData.Flags.HttpcHdrsReceived)
		return TRUE;
	return FALSE;
}

/**
@brief Verifies if the Http Client finished a request.
This function verifies if the Http Client module has finished the HTTP transaction. This function]
checks the current state of the FSM and returns true only if the state

@return Returns TRUE if the client is "Idle" and FALSE if the client module is working on a request
*/
BOOL HttpcRequestDone()
{
	if(HttpcData.FsmState != E_HTTPC_DONE)
		return FALSE;
	return TRUE;
}

/**
@brief Main HTTP Client Loop Function 
This function performs the basic tasks of an HTTP Client. Operations such as opening a transport
layer connection, generating a request, sending the request Entity body and dispatching received
data to the callback or processing function are handled on this state machine.*/
void HttpClientTask()
{
	switch(HttpcData.FsmState)
	{
		case E_HTTPC_HOME:			// Obtain the socket for the HTTP client
			HttpcData.xSocket = TCPOpen((DWORD)HttpcData.pxRequestData->Server, TCP_OPEN_RAM_HOST, HttpcData.pxRequestData->ServerPort, TCP_PURPOSE_HTTP_CLIENT);
			if(HttpcData.xSocket == INVALID_SOCKET)
			{
				HttpcData.Flags.HttpcErrorCode = E_HTTPC_ERROR_SOCKET;
				HttpcData.FsmState = E_HTTPC_SOCKET_CLOSE;
                break;
			}
			TCPWasReset(HttpcData.xSocket);		// Clear disconnection semaphore, so we can use it later
			HttpcData.TickValue = TickGet();	// Store current system tick value
			HttpcData.FsmState++;				// Go to next state, wait for TCP Handshake
		break;	// Return to stack loop, allow remote server to acknowledge tcp connection

		case E_HTTPC_SOCKET_OPEN:	// Check if we're connected to the remote host
			if(!TCPIsConnected(HttpcData.xSocket))
            {
                if(TickGet() - HttpcData.TickValue > 10 * TICK_SECOND)	// Check for connection timeouts
                {
					HttpcData.Flags.HttpcErrorCode = E_HTTPC_ERROR_CON_TIMEOUT;
                    HttpcData.FsmState =  E_HTTPC_SOCKET_CLOSE;
                }
                break;
            }
			TCPAdjustFIFOSize(HttpcData.xSocket, 10, 0, TCP_ADJUST_GIVE_REST_TO_TX);	// Use the FIFO space for TX
			HttpcData.TickValue = TickGet();	// Connection is now stablished, send request line
			HttpcData.FsmState++;
		//break;

		case E_HTTPC_SEND_REQUEST:		// Send request line to server
			if(TCPIsPutReady(HttpcData.xSocket) < CONFIG_HTTPC_TCP_TXBUFFER_MAX)	// Check for buffer space
                break; 
			switch(HttpcData.pxRequestData->eMethod)	// Put HTTP Method ~ 7 bytes
			{
				case E_HTTP_GET:
					TCPPutROMString(HttpcData.xSocket, (ROM BYTE*)"GET ");
				break;
				case E_HTTP_POST:
					TCPPutROMString(HttpcData.xSocket, (ROM BYTE*)"POST ");
				break;
				case E_HTTP_PUT:
					TCPPutROMString(HttpcData.xSocket, (ROM BYTE*)"PUT ");
				break;
				case E_HTTP_DELETE:
					TCPPutROMString(HttpcData.xSocket, (ROM BYTE*)"DELETE ");
				break;
				default:
					HttpcData.Flags.HttpcErrorCode = E_HTTPC_ERROR_METHOD;	// Request method not implemented
					HttpcData.FsmState = E_HTTPC_SOCKET_CLOSE;
					break;
			}
			if(*(TCPPutString(HttpcData.xSocket, HttpcData.pxRequestData->pcUrl)) != '\0'	// Put request URL
			|| *(TCPPutROMString(HttpcData.xSocket, (ROM BYTE*)" HTTP/1.1\r\n")) != '\0')		// Put protocol Version ~ 11 bytes
			{
				HttpcData.Flags.HttpcErrorCode = E_HTTPC_ERROR_TXOVERFLOW;	 
				HttpcData.FsmState = E_HTTPC_SOCKET_CLOSE;	
				break;
			}
			HttpcData.FsmState++;
		//break;
			
		case E_HTTPC_SEND_HEADERS:		// Start sending headers
			if(TCPIsPutReady(HttpcData.xSocket) < CONFIG_HTTPC_TCP_TXBUFFER_MAX)	// Check for buffer space
				break;
			// General Headers (You can hardcode other headers here, but you should respect the buffer size)
			TCPPutROMString(HttpcData.xSocket, (ROM BYTE*)"Connection: close\r\n");	// 19
			// Request Headers
			TCPPutROMString(HttpcData.xSocket, (ROM BYTE*)"User-Agent: MCHP HTTPC\r\n");	// 26
			TCPPutROMString(HttpcData.xSocket, (ROM BYTE*)"Host: ");
			if(*(TCPPutString(HttpcData.xSocket, HttpcData.pxRequestData->Server)) != '\0'
			|| *(TCPPutROMString(HttpcData.xSocket, (ROM BYTE*)"\r\n")) != '\0')
			{
				HttpcData.Flags.HttpcErrorCode = E_HTTPC_ERROR_TXOVERFLOW;
				HttpcData.FsmState = E_HTTPC_SOCKET_CLOSE;
				break;
			}
			HttpcData.NextByte = HttpcData.pxRequestData->AdditionalHeaders;	// Prepare transmision of custom headers
            HttpcData.FsmState++;
		//break;
		
		case E_HTTPC_SEND_USRHEADERS:		// Send user provided headers (Entity Headers, Non-standard headers, others)
			HttpcData.NextByte = TCPPutString(HttpcData.xSocket, HttpcData.NextByte);
			if(* HttpcData.NextByte != '\0')	// String is longer than available buffer space
				break;		// Store pointer to next byte to tx and wait
			HttpcData.FsmState++;		// If we get here all additional headers have been transmitted
		//break;
		case E_HTTPC_FINALIZE_HEADERS:
			if(TCPIsPutReady(HttpcData.xSocket) < 30u)
				break;				
			if(HttpcData.pxRequestData->ReqDataLength > 0)	// If needed send entity length header
			{
				BYTE EntityBodySize[6];
				uitoa(HttpcData.pxRequestData->ReqDataLength, EntityBodySize);	// Convert to string
				TCPPutROMString(HttpcData.xSocket, (ROM BYTE*)"Content-Length: ");
				TCPPutString(HttpcData.xSocket, EntityBodySize);
				TCPPutROMString(HttpcData.xSocket, (ROM BYTE*)"\r\n");
			}
			// Send empty line to finish HTTP Headers
			TCPPutROMString(HttpcData.xSocket, (ROM BYTE*)"\r\n");
			HttpcData.DataCount = 0;		// Clear counter to begin with body transmission
            HttpcData.FsmState++;	
		break;
		case E_HTTPC_SEND_BODY:		// Send request body, no limit on the size of the array, this code fills the TCP TX buffer with data
			HttpcData.DataCount += TCPPutArray(HttpcData.xSocket,	
				&(HttpcData.pxRequestData->ReqData[HttpcData.DataCount]),	// Pass pointer to the current item
				HttpcData.pxRequestData->ReqDataLength - HttpcData.DataCount);		// Data bytes to TX
			if(HttpcData.DataCount < HttpcData.pxRequestData->ReqDataLength)	// check if done
				break;
			HttpcData.DataCount = 0;
			TCPFlush(HttpcData.xSocket);	// Force transmission of pending data
			TCPAdjustFIFOSize(HttpcData.xSocket, 1, 10, TCP_ADJUST_PRESERVE_RX | TCP_ADJUST_GIVE_REST_TO_RX);	// Adjust FIFO to process response
			HttpcData.FsmState++;
		break;
#if CONFIG_HTTPC_PROCESS_HEADERS
		case E_HTTPC_PROCESS_HEADERS:
			DataLength = StrTCPArrayFind(HttpcData.xSocket,(ROM BYTE*)"\r\n", 2, 0, HttpcData.pxRequestData.ResponseBufferLength, FALSE)
			if(DataLength == 0xFFFF)	// End of line not detected
			{
				if(TCPGetRxFIFOFree(HttpcData.xSocket)==0)
					HttpcData.Flags.HttpcErrorCode = E_HTTP_ERROR_RXOVERFLOW;
			}
			if(DataLength == 0x0000)	// Empty line, end of headers from server
				HttpcData.FsmState++;
		break;
#endif
		case E_HTTPC_PROCESS_RESPONSE:		// The response is the data transmitted after the headers and untill the server closes the connection
            if(TCPWasReset(HttpcData.xSocket))			// Check if server closed the connection
				HttpcData.Flags.HttpcConnClose = 1;		// Remote host closed connection, Flag end of connection
			if(HttpcData.DataCount == 0)	// If previous data was processed and no longer needed
			{
				WORD DataToCopy = TCPIsGetReady(HttpcData.xSocket);	// Check for pending data to be received
				if( DataToCopy != 0)	// Check if data is available to copy
				{
					HttpcData.DataCount = (DataToCopy > HttpcData.pxRequestData->ResponseBufferLength) ? HttpcData.pxRequestData->ResponseBufferLength : DataToCopy;
					TCPGetArray(HttpcData.xSocket, HttpcData.pxRequestData->ResponseBuffer, HttpcData.DataCount);
				}
			}
			if( HttpcData.DataCount != 0)	// If we have some data pending
			{
				if(HttpcProcessResponse(HttpcData.pxRequestData->ResponseBuffer, HttpcData.DataCount))	// Pass data to processing
					HttpcData.DataCount = 0;	// Data successfully processed
				else
					break;	// Call this function again
			}	
			if(TCPIsGetReady(HttpcData.xSocket) == 0 && HttpcData.Flags.HttpcConnClose)	// If no data left on the buffer and remote disconnected
				HttpcData.FsmState = E_HTTPC_SOCKET_CLOSE;		// Close connection on this side
		break;
		case E_HTTPC_SOCKET_CLOSE:	// Reset all important variables so we can make another request
			HttpcData.DataCount = 0;
			HttpcData.Flags.HttpcConnClose = 0;
			TCPDisconnect(HttpcData.xSocket);
            HttpcData.xSocket = INVALID_SOCKET;
            HttpcData.FsmState = E_HTTPC_DONE;
		break;
		case E_HTTPC_DONE:
		break;		
	}
}

#endif

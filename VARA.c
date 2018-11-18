/*
Copyright 2001-2015 John Wiseman G8BPQ

This file is part of LinBPQ/BPQ32.

LinBPQ/BPQ32 is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

LinBPQ/BPQ32 is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with LinBPQ/BPQ32.  If not, see http://www.gnu.org/licenses
*/	

//
//	Interface to allow G8BPQ switch to use VARA Virtual TNC in a form 
//	of ax.25


#define _CRT_SECURE_NO_DEPRECATE
#define _USE_32BIT_TIME_T

#include <stdio.h>
#include <time.h>


#include "CHeaders.h"

#ifdef WIN32
#include <Psapi.h>
#endif

int (WINAPI FAR *GetModuleFileNameExPtr)();
int (WINAPI FAR *EnumProcessesPtr)();

#define SD_RECEIVE      0x00
#define SD_SEND         0x01
#define SD_BOTH         0x02

#include "bpq32.h"

#include "tncinfo.h"


#define WSA_ACCEPT WM_USER + 1
#define WSA_DATA WM_USER + 2
#define WSA_CONNECT WM_USER + 3

static int Socket_Data(int sock, int error, int eventcode);

int KillTNC(struct TNCINFO * TNC);
int RestartTNC(struct TNCINFO * TNC);
int KillPopups(struct TNCINFO * TNC);
VOID MoveWindows(struct TNCINFO * TNC);
int SendReporttoWL2K(struct TNCINFO * TNC);
char * CheckAppl(struct TNCINFO * TNC, char * Appl);
int DoScanLine(struct TNCINFO * TNC, char * Buff, int Len);
BOOL KillOldTNC(char * Path);
int VARASendData(struct TNCINFO * TNC, UCHAR * Buff, int Len);
VOID VARASendCommand(struct TNCINFO * TNC, char * Buff, BOOL Queue);
VOID VARAProcessDataPacket(struct TNCINFO * TNC, UCHAR * Data, int Length);

#ifndef LINBPQ
BOOL CALLBACK EnumVARAWindowsProc(HWND hwnd, LPARAM  lParam);
#endif

static char ClassName[]="VARASTATUS";
static char WindowTitle[] = "VARA";
static int RigControlRow = 165;

#define WINMOR
#define NARROWMODE 21
#define WIDEMODE 22

#ifndef LINBPQ
#include <commctrl.h>
#endif

extern char * PortConfig[33];
extern int SemHeldByAPI;

static RECT Rect;

struct TNCINFO * TNCInfo[34];		// Records are Malloc'd

static int ProcessLine(char * buf, int Port);

unsigned long _beginthread( void( *start_address )(), unsigned stack_size, int arglist);

// RIGCONTROL COM60 19200 ICOM IC706 5e 4 14.103/U1w 14.112/u1 18.1/U1n 10.12/l1

static int ProcessLine(char * buf, int Port)
{
	UCHAR * ptr,* p_cmd;
	char * p_ipad = 0;
	char * p_port = 0;
	unsigned short WINMORport = 0;
	int BPQport;
	int len=510;
	struct TNCINFO * TNC;
	char errbuf[256];

	strcpy(errbuf, buf);

	ptr = strtok(buf, " \t\n\r");

	if(ptr == NULL) return (TRUE);

	if(*ptr =='#') return (TRUE);			// comment

	if(*ptr ==';') return (TRUE);			// comment

	if (_stricmp(buf, "ADDR"))
		return FALSE;						// Must start with ADDR

	ptr = strtok(NULL, " \t\n\r");

	BPQport = Port;
	p_ipad = ptr;

	TNC = TNCInfo[BPQport] = malloc(sizeof(struct TNCINFO));
	memset(TNC, 0, sizeof(struct TNCINFO));

		TNC->InitScript = malloc(1000);
		TNC->InitScript[0] = 0;
	
		if (p_ipad == NULL)
			p_ipad = strtok(NULL, " \t\n\r");

		if (p_ipad == NULL) return (FALSE);
	
		p_port = strtok(NULL, " \t\n\r");
			
		if (p_port == NULL) return (FALSE);

		WINMORport = atoi(p_port);

		TNC->destaddr.sin_family = AF_INET;
		TNC->destaddr.sin_port = htons(WINMORport);
		TNC->Datadestaddr.sin_family = AF_INET;
		TNC->Datadestaddr.sin_port = htons(WINMORport+1);

		TNC->HostName = malloc(strlen(p_ipad)+1);

		if (TNC->HostName == NULL) return TRUE;

		strcpy(TNC->HostName,p_ipad);

		ptr = strtok(NULL, " \t\n\r");

		if (ptr)
		{
			if (_stricmp(ptr, "PTT") == 0)
			{
				ptr = strtok(NULL, " \t\n\r");

				if (ptr)
				{
					if (_stricmp(ptr, "CI-V") == 0)
						TNC->PTTMode = PTTCI_V;
					else if (_stricmp(ptr, "CAT") == 0)
						TNC->PTTMode = PTTCI_V;
					else if (_stricmp(ptr, "RTS") == 0)
						TNC->PTTMode = PTTRTS;
					else if (_stricmp(ptr, "DTR") == 0)
						TNC->PTTMode = PTTDTR;
					else if (_stricmp(ptr, "DTRRTS") == 0)
						TNC->PTTMode = PTTDTR | PTTRTS;
					else if (_stricmp(ptr, "CM108") == 0)
						TNC->PTTMode = PTTCM108;

					ptr = strtok(NULL, " \t\n\r");
				}
			}
		}
		
		if (ptr)
		{
			if (_memicmp(ptr, "PATH", 4) == 0)
			{
				p_cmd = strtok(NULL, "\n\r");
				if (p_cmd) TNC->ProgramPath = _strdup(p_cmd);
//				if (p_cmd) TNC->ProgramPath = _strdup(_strupr(p_cmd));
			}
		}

		TNC->MaxConReq = 10;		// Default

		// Read Initialisation lines

		while(TRUE)
		{
			if (GetLine(buf) == 0)
				return TRUE;

			strcpy(errbuf, buf);

			if (memcmp(buf, "****", 4) == 0)
				return TRUE;

			ptr = strchr(buf, ';');
			if (ptr)
			{
				*ptr++ = 13;
				*ptr = 0;
			}
				
			if ((_memicmp(buf, "CAPTURE", 7) == 0) || (_memicmp(buf, "PLAYBACK", 8) == 0))
			{}		// Ignore
			else
/*
			if (_memicmp(buf, "PATH", 4) == 0)
			{
				char * Context;
				p_cmd = strtok_s(&buf[5], "\n\r", &Context);
				if (p_cmd) TNC->ProgramPath = _strdup(p_cmd);
			}
			else
*/
			if (_memicmp(buf, "WL2KREPORT", 10) == 0)
				TNC->WL2K = DecodeWL2KReportLine(buf);
			else
			if (_memicmp(buf, "BUSYHOLD", 8) == 0)		// Hold Time for Busy Detect
				TNC->BusyHold = atoi(&buf[8]);

			else
			if (_memicmp(buf, "BUSYWAIT", 8) == 0)		// Wait time beofre failing connect if busy
				TNC->BusyWait = atoi(&buf[8]);

			else
			if (_memicmp(buf, "MAXCONREQ", 9) == 0)		// Hold Time for Busy Detect
				TNC->MaxConReq = atoi(&buf[9]);

			else
			if (_memicmp(buf, "STARTINROBUST", 13) == 0)
				TNC->StartInRobust = TRUE;
			
			else
			if (_memicmp(buf, "ROBUST", 6) == 0)
			{
				if (_memicmp(&buf[7], "TRUE", 4) == 0)
					TNC->Robust = TRUE;
				
				strcat (TNC->InitScript, buf);
			}
			else

			strcat (TNC->InitScript, buf);
		}


	return (TRUE);	
}



void VARAThread(int port);
VOID VARAProcessDataSocketData(int port);
int ConnecttoVARA();
VOID VARAProcessReceivedData(struct TNCINFO * TNC);
VOID VARAProcessReceivedControl(struct TNCINFO * TNC);
VOID VARAReleaseTNC(struct TNCINFO * TNC);
VOID SuspendOtherPorts(struct TNCINFO * ThisTNC);
VOID ReleaseOtherPorts(struct TNCINFO * ThisTNC);
VOID WritetoTrace(struct TNCINFO * TNC, char * Msg, int Len);


#define MAXBPQPORTS 32

static time_t ltime;


static SOCKADDR_IN sinx; 
static SOCKADDR_IN rxaddr;

static int addrlen=sizeof(sinx);

static int ExtProc(int fn, int port,unsigned char * buff)
{
	int datalen;
	UINT * buffptr;
	char txbuff[500];
	unsigned int bytes,txlen=0;
	int Param;
	HKEY hKey=0;
	struct TNCINFO * TNC = TNCInfo[port];
	struct STREAMINFO * STREAM = &TNC->Streams[0];
	struct ScanEntry * Scan;

	if (TNC == NULL)
		return 0;							// Port not defined

	if (TNC->CONNECTED == 0)
	{
		// clear Q if not connected

		while(TNC->BPQtoWINMOR_Q)
		{
			buffptr = Q_REM(&TNC->BPQtoWINMOR_Q);

			if (buffptr)
				ReleaseBuffer(buffptr);
		}
	}


	switch (fn)
	{
	case 1:				// poll

		while (TNC->PortRecord->UI_Q)
		{
			buffptr = Q_REM(&TNC->PortRecord->UI_Q);
			ReleaseBuffer(buffptr);	
		}

		if (TNC->Busy)							//  Count down to clear
		{
			if ((TNC->BusyFlags & CDBusy) == 0)	// TNC Has reported not busy
			{
				TNC->Busy--;
				if (TNC->Busy == 0)
					SetWindowText(TNC->xIDC_CHANSTATE, "Clear");
					strcpy(TNC->WEB_CHANSTATE, "Clear");
			}
		}

		if (TNC->BusyDelay)
		{
			// Still Busy?

			if (InterlockedCheckBusy(TNC) == FALSE)
			{
				// No, so send

				VARASendCommand(TNC, TNC->ConnectCmd, TRUE);
				TNC->Streams[0].Connecting = TRUE;

				memset(TNC->Streams[0].RemoteCall, 0, 10);
				memcpy(TNC->Streams[0].RemoteCall, &TNC->ConnectCmd[8], strlen(TNC->ConnectCmd)-10);

				sprintf(TNC->WEB_TNCSTATE, "%s Connecting to %s", TNC->Streams[0].MyCall, TNC->Streams[0].RemoteCall);
				SetWindowText(TNC->xIDC_TNCSTATE, TNC->WEB_TNCSTATE);

				free(TNC->ConnectCmd);
				TNC->BusyDelay = 0;
			}
			else
			{
				// Wait Longer

				TNC->BusyDelay--;

				if (TNC->BusyDelay == 0)
				{
					// Timed out - Send Error Response

					UINT * buffptr = GetBuff();

					if (buffptr == 0) return (0);			// No buffers, so ignore

					buffptr[1]=39;
					memcpy(buffptr+2,"Sorry, Can't Connect - Channel is busy\r", 39);

					C_Q_ADD(&TNC->WINMORtoBPQ_Q, buffptr);
					free(TNC->ConnectCmd);

					sprintf(TNC->WEB_TNCSTATE, "In Use by %s", TNC->Streams[0].MyCall);
					SetWindowText(TNC->xIDC_TNCSTATE, TNC->WEB_TNCSTATE);

				}
			}
		}

		if (TNC->HeartBeat++ > 600)			// Every Minute 		{
		{
			TNC->HeartBeat = 0;

			if (TNC->CONNECTED)
			{
				// Probe link

				if (TNC->Streams[0].Connecting || TNC->Streams[0].Connected)
				{}
				else
					VARASendCommand(TNC, "STATE\r", TRUE);
			}
		}

		//	FECPending can be set if not in FEC Mode (eg beacon)
		
		if (TNC->FECPending)	// Check if FEC Send needed
		{
			if (TNC->Streams[0].BytesOutstanding)  //Wait for data to be queued (async data session)
			{
				if (!TNC->Busy)
				{
					TNC->FECPending = 0;
//					VARASendCommand(TNC,"FECSEND TRUE", TRUE);
				}
			}
		}

		if (STREAM->NeedDisc)
		{
			STREAM->NeedDisc--;

			if (STREAM->NeedDisc == 0)
			{
				// Send the DISCONNECT

				VARASendCommand(TNC, "DISCONNECT\r", TRUE);
			}
		}

		if (TNC->DiscPending)
		{
			TNC->DiscPending--;

			if (TNC->DiscPending == 0)
			{
				// Too long in Disc Pending - Kill and Restart TNC

				if (TNC->PID)
					KillTNC(TNC);
					
				RestartTNC(TNC);
			}
		}

		if (TNC->TimeSinceLast++ > 800)			// Allow 10 secs for Keepalive
		{
			// Restart TNC
		
			TNC->TimeSinceLast = 0;
			
			if (TNC->CONNECTED && TNC->ProgramPath)
			{
//				if (strstr(TNC->ProgramPath, "WINMOR TNC"))
				{
					struct tm * tm;
					char Time[80];
				
					TNC->Restarts++;
					TNC->LastRestart = time(NULL);

					tm = gmtime(&TNC->LastRestart);	
				
					sprintf_s(Time, sizeof(Time),"%04d/%02d/%02d %02d:%02dZ",
						tm->tm_year +1900, tm->tm_mon+1, tm->tm_mday, tm->tm_hour, tm->tm_min);

					MySetWindowText(TNC->xIDC_RESTARTTIME, Time);
					strcpy(TNC->WEB_RESTARTTIME, Time);

					sprintf_s(Time, sizeof(Time),"%d", TNC->Restarts);
					MySetWindowText(TNC->xIDC_RESTARTS, Time);
					strcpy(TNC->WEB_RESTARTS, Time);
	
					KillTNC(TNC);
					RestartTNC(TNC);
				}
			}
		}

		if (TNC->PortRecord->ATTACHEDSESSIONS[0] && TNC->Streams[0].Attached == 0)
		{
			// New Attach

			int calllen;
			char Msg[80];

			TNC->Streams[0].Attached = TRUE;

			calllen = ConvFromAX25(TNC->PortRecord->ATTACHEDSESSIONS[0]->L4USER, TNC->Streams[0].MyCall);
			TNC->Streams[0].MyCall[calllen] = 0;

			// Stop Listening, and set MYCALL to user's call

			VARASendCommand(TNC, "LISTEN OFF\r", TRUE);

			// Stop other ports in same group

			SuspendOtherPorts(TNC);

			sprintf(TNC->WEB_TNCSTATE, "In Use by %s", TNC->Streams[0].MyCall);
			MySetWindowText(TNC->xIDC_TNCSTATE, TNC->WEB_TNCSTATE);

			// Stop Scanning

			sprintf(Msg, "%d SCANSTOP", TNC->Port);
	
			Rig_Command(-1, Msg);

		}

		if (TNC->Streams[0].Attached)
			CheckForDetach(TNC, 0, &TNC->Streams[0], TidyClose, ForcedClose, CloseComplete);

		if (TNC->Streams[0].ReportDISC)
		{
			TNC->Streams[0].ReportDISC = FALSE;
			buff[4] = 0;
			return -1;
		}

		if (TNC->CONNECTED == FALSE && TNC->CONNECTING == FALSE)
		{
			//	See if time to reconnect
		
			time(&ltime);
			if (ltime - TNC->lasttime > 9 )
			{
				TNC->lasttime = ltime;
				ConnecttoVARA(port);
			}
		}
		
		// See if any frames for this port

		if (TNC->Streams[0].BPQtoPACTOR_Q)		//Used for CTEXT
		{
			UINT * buffptr = Q_REM(&TNC->Streams[0].BPQtoPACTOR_Q);
			txlen=buffptr[1];
			memcpy(txbuff,buffptr+2,txlen);
			bytes = VARASendData(TNC, &txbuff[0], txlen);
			STREAM->BytesTXed += bytes;
			ReleaseBuffer(buffptr);
		}


		if (TNC->WINMORtoBPQ_Q != 0)
		{
			buffptr=Q_REM(&TNC->WINMORtoBPQ_Q);

			datalen=buffptr[1];

			buff[4] = 0;						// Compatibility with Kam Driver
			buff[7] = 0xf0;
			memcpy(&buff[8],buffptr+2,datalen);	// Data goes to +7, but we have an extra byte
			datalen+=8;
			buff[5]=(datalen & 0xff);
			buff[6]=(datalen >> 8);
		
			ReleaseBuffer(buffptr);

			return (1);
		}

		return (0);

	case 2:				// send

		if (!TNC->CONNECTED)
		{
			// Send Error Response

			UINT * buffptr = GetBuff();

			if (buffptr == 0) return (0);			// No buffers, so ignore

			buffptr[1]=36;
			memcpy(buffptr+2,"No Connection to VARA TNC\r", 36);

			C_Q_ADD(&TNC->WINMORtoBPQ_Q, buffptr);
			
			return 0;		// Don't try if not connected
		}

		if (TNC->Streams[0].BPQtoPACTOR_Q)		//Used for CTEXT
		{
			UINT * buffptr = Q_REM(&TNC->Streams[0].BPQtoPACTOR_Q);
			txlen=buffptr[1];
			memcpy(txbuff,buffptr+2,txlen);
			bytes=send(TNC->TCPDataSock,(const char FAR *)&buff[8],txlen,0);
			STREAM->BytesTXed += bytes;
			WritetoTrace(TNC, txbuff, txlen);
			ReleaseBuffer(buffptr);
		}
		
		if (TNC->SwallowSignon)
		{
			TNC->SwallowSignon = FALSE;		// Discard *** connected
			return 0;
		}


		txlen=(buff[6]<<8) + buff[5]-8;	
		
		if (TNC->Streams[0].Connected)
		{
			STREAM->PacketsSent++;

			if (STREAM->PacketsSent == 3)
			{
//				if (TNC->Robust)
//					ARDOPSendCommand(TNC, "ROBUST TRUE");
//				else
//					ARDOPSendCommand(TNC, "ROBUST FALSE");
			}

			bytes=send(TNC->TCPDataSock,(const char FAR *)&buff[8],txlen,0);
			STREAM->BytesTXed += bytes;
			WritetoTrace(TNC, &buff[8], txlen);

		}
		else
		{
			if (_memicmp(&buff[8], "D\r", 2) == 0 || _memicmp(&buff[8], "BYE\r", 4) == 0)
			{
				TNC->Streams[0].ReportDISC = TRUE;		// Tell Node
				return 0;
			}
	
			// See if Local command (eg RADIO)

			if (_memicmp(&buff[8], "RADIO ", 6) == 0)
			{
				sprintf(&buff[8], "%d %s", TNC->Port, &buff[14]);

				if (Rig_Command(TNC->PortRecord->ATTACHEDSESSIONS[0]->L4CROSSLINK->CIRCUITINDEX, &buff[8]))
				{
				}
				else
				{
					UINT * buffptr = GetBuff();

					if (buffptr == 0) return 1;			// No buffers, so ignore

					buffptr[1] = sprintf((UCHAR *)&buffptr[2], "%s", &buff[8]);
					C_Q_ADD(&TNC->WINMORtoBPQ_Q, buffptr);
				}
				return 1;
			}

			if (_memicmp(&buff[8], "OVERRIDEBUSY", 12) == 0)
			{
				UINT * buffptr = GetBuff();

				TNC->OverrideBusy = TRUE;

				if (buffptr)
				{
					buffptr[1] = sprintf((UCHAR *)&buffptr[2], "VARA} OK\r");
					C_Q_ADD(&TNC->WINMORtoBPQ_Q, buffptr);
				}

				return 0;

			}


			if (_memicmp(&buff[8], "MAXCONREQ", 9) == 0)
			{
				if (buff[17] != 13)
				{
					UINT * buffptr = GetBuff();
				
					// Limit connects

					int tries = atoi(&buff[18]);
					if (tries > 10) tries = 10;

					TNC->MaxConReq = tries;

					if (buffptr)
					{
						buffptr[1] = sprintf((UCHAR *)&buffptr[2], "VARA} OK\r");
						C_Q_ADD(&TNC->WINMORtoBPQ_Q, buffptr);
					}
					return 0;
				}
			}
			if (_memicmp(&buff[8], "ARQBW ", 6) == 0)
				TNC->WinmorCurrentMode = 0;			// So scanner will set next value

			if (_memicmp(&buff[8], "CODEC TRUE", 9) == 0)
				TNC->StartSent = TRUE;

			if (_memicmp(&buff[8], "D\r", 2) == 0)
			{
				TNC->Streams[0].ReportDISC = TRUE;		// Tell Node
				return 0;
			}

			// See if a Connect Command. If so set Connecting

			if (toupper(buff[8]) == 'C' && buff[9] == ' ' && txlen > 2)	// Connect
			{
				char Connect[80];
				char * ptr = strchr(&buff[10], 13);

				if (ptr)
					*ptr = 0;

				_strupr(&buff[10]);

				sprintf(Connect, "CONNECT %s %s\r", TNC->Streams[0].MyCall, &buff[10]);

				// See if Busy
				
				if (InterlockedCheckBusy(TNC))
				{
					// Channel Busy. Unless override set, wait

					if (TNC->OverrideBusy == 0)
					{
						// Save Command, and wait up to 10 secs
						
						sprintf(TNC->WEB_TNCSTATE, "Waiting for clear channel");
						MySetWindowText(TNC->xIDC_TNCSTATE, TNC->WEB_TNCSTATE);

						TNC->ConnectCmd = _strdup(Connect);
						TNC->BusyDelay = TNC->BusyWait * 10;		// BusyWait secs
						return 0;
					}
				}

				TNC->OverrideBusy = FALSE;

				VARASendCommand(TNC, Connect, TRUE);
				TNC->Streams[0].Connecting = TRUE;

				memset(TNC->Streams[0].RemoteCall, 0, 10);
				strcpy(TNC->Streams[0].RemoteCall, &buff[10]);

				sprintf(TNC->WEB_TNCSTATE, "%s Connecting to %s", TNC->Streams[0].MyCall, TNC->Streams[0].RemoteCall);
				MySetWindowText(TNC->xIDC_TNCSTATE, TNC->WEB_TNCSTATE);
			}
			else
			{
				buff[7 + txlen++] = 13;
				buff[7 + txlen] = 0;
				VARASendCommand(TNC, &buff[8], TRUE);
			}
		}
		return (0);

	case 3:	
		
		// CHECK IF OK TO SEND (And check TNC Status)
	
		if (TNC->Streams[0].Attached == 0)
			return TNC->CONNECTED << 8 | 1;

		return (TNC->CONNECTED << 8 | TNC->Streams[0].Disconnecting << 15);		// OK


	case 4:				// reinit

		shutdown(TNC->TCPSock, SD_BOTH);
		Sleep(100);
		closesocket(TNC->TCPSock);

		if (TNC->PID && TNC->WeStartedTNC)
		{
			KillTNC(TNC);
			RestartTNC(TNC);
		}
		return 0;

	case 5:				// Close

		if (TNC->CONNECTED)
		{
//			GetSemaphore(&Semaphore, 52);
//			VARASendCommand(TNC, "CLOSE", FALSE);
//			FreeSemaphore(&Semaphore);
//			Sleep(100);
		}

		shutdown(TNC->TCPSock, SD_BOTH);
		Sleep(100);
		closesocket(TNC->TCPSock);
		if (TNC->WeStartedTNC)
			KillTNC(TNC);
	
		return 0;


	case 6:				// Scan Stop Interface

		Param = (int)buff;
	
		if (Param == 1)		// Request Permission
		{
			if (TNC->ConnectPending)
				TNC->ConnectPending--;		// Time out if set too long

			if (!TNC->ConnectPending)
				return 0;	// OK to Change

			VARASendCommand(TNC, "LISTEN OFF\r", TRUE);

			return TRUE;
		}

		if (Param == 2)		// Check  Permission
		{
			if (TNC->ConnectPending)
			{
				TNC->ConnectPending--;
				return -1;	// Skip Interval
			}
			return 1;		// OK to change
		}

		if (Param == 3)		// Release  Permission
		{
			VARASendCommand(TNC, "LISTEN ON\r", TRUE);
			return 0;
		}

		// Param is Address of a struct ScanEntry

		Scan = (struct ScanEntry *)buff;

		if (strcmp(Scan->ARDOPMode, TNC->ARDOPCurrentMode) != 0)
		{
			// Mode changed

			char CMD[32];
			

			strcpy(TNC->ARDOPCurrentMode, Scan->ARDOPMode); 

			
			if (Scan->ARDOPMode[0] == 'S') // SKIP - Dont Allow Connects
			{
				if (TNC->ARDOPCurrentMode[0] != 0)
				{
					VARASendCommand(TNC, "LISTEN OFF\r", TRUE);
					TNC->ARDOPCurrentMode[0] = 0;
				}

				TNC->WL2KMode = 0;
				return 0;
			}
			else
			{
				if (TNC->ARDOPCurrentMode[0] == 0)
					VARASendCommand(TNC, "LISTEN ON\r", TRUE);
			}

			if (strchr(Scan->ARDOPMode, 'F'))
				sprintf(CMD, "ARQBW %sORCED", Scan->ARDOPMode);
			else
				sprintf(CMD, "ARQBW %sAX", Scan->ARDOPMode);
	
			return 0;
		}
		return 0;
	}
	return 0;
}


static int WebProc(struct TNCINFO * TNC, char * Buff, BOOL LOCAL)
{
	int Len = sprintf(Buff, "<html><meta http-equiv=expires content=0><meta http-equiv=refresh content=15>"
		"<script type=\"text/javascript\">\r\n"
		"function ScrollOutput()\r\n"
		"{var textarea = document.getElementById('textarea');"
		"textarea.scrollTop = textarea.scrollHeight;}</script>"
		"</head><title>VARA Status</title></head><body id=Text onload=\"ScrollOutput()\">"
		"<h2><form method=post target=\"POPUPW\" onsubmit=\"POPUPW = window.open('about:blank','POPUPW',"
		"'width=440,height=150');\" action=ARDOPAbort?%d>VARA Status"
		"<input name=Save value=\"Abort Session\" type=submit style=\"position: absolute; right: 20;\"></form></h2>",
		TNC->Port);


	Len += sprintf(&Buff[Len], "<table style=\"text-align: left; width: 500px; font-family: monospace; align=center \" border=1 cellpadding=2 cellspacing=2>");

	Len += sprintf(&Buff[Len], "<tr><td width=110px>Comms State</td><td>%s</td></tr>", TNC->WEB_COMMSSTATE);
	Len += sprintf(&Buff[Len], "<tr><td>TNC State</td><td>%s</td></tr>", TNC->WEB_TNCSTATE);
	Len += sprintf(&Buff[Len], "<tr><td>Mode</td><td>%s</td></tr>", TNC->WEB_MODE);
	Len += sprintf(&Buff[Len], "<tr><td>Channel State</td><td>%s</td></tr>", TNC->WEB_CHANSTATE);
	Len += sprintf(&Buff[Len], "<tr><td>Proto State</td><td>%s</td></tr>", TNC->WEB_PROTOSTATE);
	Len += sprintf(&Buff[Len], "<tr><td>Traffic</td><td>%s</td></tr>", TNC->WEB_TRAFFIC);
//	Len += sprintf(&Buff[Len], "<tr><td>TNC Restarts</td><td></td></tr>", TNC->WEB_RESTARTS);
	Len += sprintf(&Buff[Len], "</table>");

	Len += sprintf(&Buff[Len], "<textarea rows=10 style=\"width:500px; height:250px;\" id=textarea >%s</textarea>", TNC->WebBuffer);
	Len = DoScanLine(TNC, Buff, Len);

	return Len;
}

VOID VARASuspendPort(struct TNCINFO * TNC)
{
	VARASendCommand(TNC, "LISTEN OFF\r", TRUE);
}

VOID VARAReleasePort(struct TNCINFO * TNC)
{
	VARASendCommand(TNC, "LISTEN ON\r", TRUE);
}


UINT VARAExtInit(EXTPORTDATA * PortEntry)
{
	int i, port;
	char Msg[255];
	char * ptr;
	APPLCALLS * APPL;
	struct TNCINFO * TNC;
	int AuxCount = 0;
	char Appl[11];
	char * TempScript;
	struct PORTCONTROL * PORT = &PortEntry->PORTCONTROL;
	//
	//	Will be called once for each VARA port 
	//
	//	The Socket to connect to is in IOBASE
	//

	port = PortEntry->PORTCONTROL.PORTNUMBER;

	ReadConfigFile(port, ProcessLine);

	TNC = TNCInfo[port];

	if (TNC == NULL)
	{
		// Not defined in Config file

		sprintf(Msg," ** Error - no info in BPQ32.cfg for this port\n");
		WritetoConsole(Msg);

		return (int) ExtProc;
	}
	
	TNC->Port = port;

	TNC->ARDOPBuffer = malloc(8192);
	TNC->ARDOPDataBuffer = malloc(8192);

	if (TNC->ProgramPath)
		TNC->WeStartedTNC = RestartTNC(TNC);

	TNC->Hardware = H_VARA;

	if (TNC->BusyWait == 0)
		TNC->BusyWait = 10;

	if (TNC->BusyHold == 0)
		TNC->BusyHold = 1;

	TNC->PortRecord = PortEntry;

	if (PortEntry->PORTCONTROL.PORTCALL[0] == 0)
		memcpy(TNC->NodeCall, MYNODECALL, 10);
	else
		ConvFromAX25(&PortEntry->PORTCONTROL.PORTCALL[0], TNC->NodeCall);

	TNC->Interlock = PortEntry->PORTCONTROL.PORTINTERLOCK;

	PortEntry->PORTCONTROL.PROTOCOL = 10;
	PortEntry->PORTCONTROL.PORTQUALITY = 0;
	PortEntry->MAXHOSTMODESESSIONS = 1;	
	PortEntry->SCANCAPABILITIES = SIMPLE;			// Scan Control - pending connect only

	PortEntry->PORTCONTROL.UICAPABLE = FALSE;

	if (PortEntry->PORTCONTROL.PORTPACLEN == 0)
		PortEntry->PORTCONTROL.PORTPACLEN = 236;

	TNC->SuspendPortProc = VARASuspendPort;
	TNC->ReleasePortProc = VARAReleasePort;

	TNC->ModemCentre = 1500;				// WINMOR is always 1500 Offset

	ptr=strchr(TNC->NodeCall, ' ');
	if (ptr) *(ptr) = 0;					// Null Terminate

	// Set Essential Params and MYCALL

	// Put overridable ones on front, essential ones on end

	TempScript = zalloc(1000);

//	strcat(TempScript, "ROBUST False\r");

	// Set MYCALL

	sprintf(Msg, "MYCALL %s", TNC->NodeCall);

	for (i = 0; i < 32; i++)
	{
		APPL=&APPLCALLTABLE[i];

		if (APPL->APPLCALL_TEXT[0] > ' ')
		{
			char * ptr;
			memcpy(Appl, APPL->APPLCALL_TEXT, 10);
			ptr=strchr(Appl, ' ');

			if (ptr)
			{
//				*ptr++ = ' ';
				*ptr = 0;
			}
			strcat(Msg, " ");
			strcat(Msg, Appl);
			AuxCount++;
			if (AuxCount == 4)			// Max 5 in MYCALL
				break;
		}
	}

	strcat(Msg, "\r");

	strcat(TempScript, Msg);

	strcat(TempScript, TNC->InitScript);

	free(TNC->InitScript);
	TNC->InitScript = TempScript;


	strcat(TNC->InitScript,"LISTEN ON\r");	

	strcpy(TNC->CurrentMYC, TNC->NodeCall);

	if (TNC->WL2K == NULL)
		if (PortEntry->PORTCONTROL.WL2KInfo.RMSCall[0])			// Alrerady decoded
			TNC->WL2K = &PortEntry->PORTCONTROL.WL2KInfo;

	if (TNC->destaddr.sin_family == 0)
	{
		// not defined in config file, so use localhost and port from IOBASE

		TNC->destaddr.sin_family = AF_INET;
		TNC->destaddr.sin_port = htons(PortEntry->PORTCONTROL.IOBASE);
		TNC->Datadestaddr.sin_family = AF_INET;
		TNC->Datadestaddr.sin_port = htons(PortEntry->PORTCONTROL.IOBASE+1);

		TNC->HostName=malloc(10);

		if (TNC->HostName != NULL) 
			strcpy(TNC->HostName,"127.0.0.1");

	}

	TNC->WebWindowProc = WebProc;
	TNC->WebWinX = 520;
	TNC->WebWinY = 500;
	TNC->WebBuffer = zalloc(5000);

	TNC->WEB_COMMSSTATE = zalloc(100);
	TNC->WEB_TNCSTATE = zalloc(100);
	TNC->WEB_CHANSTATE = zalloc(100);
	TNC->WEB_BUFFERS = zalloc(100);
	TNC->WEB_PROTOSTATE = zalloc(100);
	TNC->WEB_RESTARTTIME = zalloc(100);
	TNC->WEB_RESTARTS = zalloc(100);

	TNC->WEB_MODE = zalloc(20);
	TNC->WEB_TRAFFIC = zalloc(100);


#ifndef LINBPQ

	CreatePactorWindow(TNC, ClassName, WindowTitle, RigControlRow, PacWndProc, 500, 450, ForcedClose);

	CreateWindowEx(0, "STATIC", "Comms State", WS_CHILD | WS_VISIBLE, 10,6,120,20, TNC->hDlg, NULL, hInstance, NULL);
	TNC->xIDC_COMMSSTATE = CreateWindowEx(0, "STATIC", "", WS_CHILD | WS_VISIBLE, 116,6,386,20, TNC->hDlg, NULL, hInstance, NULL);
	
	CreateWindowEx(0, "STATIC", "TNC State", WS_CHILD | WS_VISIBLE, 10,28,106,20, TNC->hDlg, NULL, hInstance, NULL);
	TNC->xIDC_TNCSTATE = CreateWindowEx(0, "STATIC", "", WS_CHILD | WS_VISIBLE, 116,28,520,20, TNC->hDlg, NULL, hInstance, NULL);

	CreateWindowEx(0, "STATIC", "Mode", WS_CHILD | WS_VISIBLE, 10,50,80,20, TNC->hDlg, NULL, hInstance, NULL);
	TNC->xIDC_MODE = CreateWindowEx(0, "STATIC", "", WS_CHILD | WS_VISIBLE, 116,50,200,20, TNC->hDlg, NULL, hInstance, NULL);
 
	CreateWindowEx(0, "STATIC", "Channel State", WS_CHILD | WS_VISIBLE, 10,72,110,20, TNC->hDlg, NULL, hInstance, NULL);
	TNC->xIDC_CHANSTATE = CreateWindowEx(0, "STATIC", "", WS_CHILD | WS_VISIBLE, 116,72,144,20, TNC->hDlg, NULL, hInstance, NULL);
 
 	CreateWindowEx(0, "STATIC", "Proto State", WS_CHILD | WS_VISIBLE,10,94,80,20, TNC->hDlg, NULL, hInstance, NULL);
	TNC->xIDC_PROTOSTATE = CreateWindowEx(0, "STATIC", "", WS_CHILD | WS_VISIBLE,116,94,374,20 , TNC->hDlg, NULL, hInstance, NULL);
 
	CreateWindowEx(0, "STATIC", "Traffic", WS_CHILD | WS_VISIBLE,10,116,80,20, TNC->hDlg, NULL, hInstance, NULL);
	TNC->xIDC_TRAFFIC = CreateWindowEx(0, "STATIC", "0 0 0 0", WS_CHILD | WS_VISIBLE,116,116,374,20 , TNC->hDlg, NULL, hInstance, NULL);

	CreateWindowEx(0, "STATIC", "TNC Restarts", WS_CHILD | WS_VISIBLE,10,138,100,20, TNC->hDlg, NULL, hInstance, NULL);
	TNC->xIDC_RESTARTS = CreateWindowEx(0, "STATIC", "0", WS_CHILD | WS_VISIBLE,116,138,40,20 , TNC->hDlg, NULL, hInstance, NULL);
	CreateWindowEx(0, "STATIC", "Last Restart", WS_CHILD | WS_VISIBLE,140,138,100,20, TNC->hDlg, NULL, hInstance, NULL);
	TNC->xIDC_RESTARTTIME = CreateWindowEx(0, "STATIC", "Never", WS_CHILD | WS_VISIBLE,250,138,200,20, TNC->hDlg, NULL, hInstance, NULL);

	TNC->hMonitor= CreateWindowEx(0, "LISTBOX", "", WS_CHILD |  WS_VISIBLE  | LBS_NOINTEGRALHEIGHT | 
            LBS_DISABLENOSCROLL | WS_HSCROLL | WS_VSCROLL,
			0,170,250,300, TNC->hDlg, NULL, hInstance, NULL);

	TNC->ClientHeight = 450;
	TNC->ClientWidth = 500;

	TNC->hMenu = CreatePopupMenu();

	AppendMenu(TNC->hMenu, MF_STRING, WINMOR_KILL, "Kill VARA TNC");
	AppendMenu(TNC->hMenu, MF_STRING, WINMOR_RESTART, "Kill and Restart VARA TNC");
	AppendMenu(TNC->hMenu, MF_STRING, WINMOR_RESTARTAFTERFAILURE, "Restart TNC after failed Connection");	
	CheckMenuItem(TNC->hMenu, WINMOR_RESTARTAFTERFAILURE, (TNC->RestartAfterFailure) ? MF_CHECKED : MF_UNCHECKED);
	AppendMenu(TNC->hMenu, MF_STRING, ARDOP_ABORT, "Abort Current Session");

	MoveWindows(TNC);
#endif
	Consoleprintf("VARA Host %s %d", TNC->HostName, htons(TNC->destaddr.sin_port));

	ConnecttoVARA(port);

	time(&TNC->lasttime);			// Get initial time value

	return ((int) ExtProc);
}

int ConnecttoVARA(int port)
{
	_beginthread(VARAThread,0,port);

	return 0;
}

VOID VARAThread(int port)
{
	// Opens sockets and looks for data on control and data sockets.
	
	char Msg[255];
	int err, i, ret;
	u_long param=1;
	BOOL bcopt=TRUE;
	struct hostent * HostEnt;
	struct TNCINFO * TNC = TNCInfo[port];
	fd_set readfs;
	fd_set errorfs;
	struct timeval timeout;
	char * ptr1;
	char * ptr2;
	UINT * buffptr;

	if (TNC->HostName == NULL)
		return;

	TNC->BusyFlags = 0;

	TNC->CONNECTING = TRUE;

	Sleep(3000);		// Allow init to complete 

#ifdef WIN32
	if (strcmp(TNC->HostName, "127.0.0.1") == 0)
	{
		// can only check if running on local host
		
		TNC->PID = GetListeningPortsPID(TNC->destaddr.sin_port);
		if (TNC->PID == 0)
		{
			TNC->CONNECTING = FALSE;
			return;						// Not listening so no point trying to connect
		}

		// Get the File Name in case we want to restart it.

		if (TNC->ProgramPath == NULL)
		{
			if (GetModuleFileNameExPtr)
			{
				HANDLE hProc;
				char ExeName[256] = "";

				hProc =  OpenProcess(PROCESS_QUERY_INFORMATION |PROCESS_VM_READ, FALSE, TNC->PID);
	
				if (hProc)
				{
					GetModuleFileNameExPtr(hProc, 0,  ExeName, 255);
					CloseHandle(hProc);

					TNC->ProgramPath = _strdup(ExeName);
				}
			}
		}
	}
#endif

//	// If we started the TNC make sure it is still running.

//	if (!IsProcess(TNC->PID))
//	{
//		RestartTNC(TNC);
//		Sleep(3000);
//	}


	TNC->destaddr.sin_addr.s_addr = inet_addr(TNC->HostName);
	TNC->Datadestaddr.sin_addr.s_addr = inet_addr(TNC->HostName);

	if (TNC->destaddr.sin_addr.s_addr == INADDR_NONE)
	{
		//	Resolve name to address

		HostEnt = gethostbyname (TNC->HostName);
		 
		 if (!HostEnt)
		 {
		 	TNC->CONNECTING = FALSE;
			return;			// Resolve failed
		 }
		 memcpy(&TNC->destaddr.sin_addr.s_addr,HostEnt->h_addr,4);
		 memcpy(&TNC->Datadestaddr.sin_addr.s_addr,HostEnt->h_addr,4);
	}

//	closesocket(TNC->TCPSock);
//	closesocket(TNC->TCPDataSock);

	TNC->TCPSock=socket(AF_INET,SOCK_STREAM,0);

	if (TNC->TCPSock == INVALID_SOCKET)
	{
		i=sprintf(Msg, "Socket Failed for VARA socket - error code = %d\r\n", WSAGetLastError());
		WritetoConsole(Msg);

	 	TNC->CONNECTING = FALSE;
  	 	return; 
	}

	TNC->TCPDataSock=socket(AF_INET,SOCK_STREAM,0);

	if (TNC->TCPDataSock == INVALID_SOCKET)
	{
		i=sprintf(Msg, "Socket Failed for VARA Data socket - error code = %d\r\n", WSAGetLastError());
		WritetoConsole(Msg);

	 	TNC->CONNECTING = FALSE;
		closesocket(TNC->TCPSock);

  	 	return; 
	}

 
	setsockopt(TNC->TCPSock, SOL_SOCKET, SO_REUSEADDR, (const char FAR *)&bcopt, 4);
	setsockopt(TNC->TCPDataSock, SOL_SOCKET, SO_REUSEADDR, (const char FAR *)&bcopt, 4);
//	setsockopt(TNC->TCPDataSock, IPPROTO_TCP, TCP_NODELAY, (const char FAR *)&bcopt, 4); 

	sinx.sin_family = AF_INET;
	sinx.sin_addr.s_addr = INADDR_ANY;
	sinx.sin_port = 0;

	if (connect(TNC->TCPSock,(LPSOCKADDR) &TNC->destaddr,sizeof(TNC->destaddr)) == 0)
	{
		//
		//	Connected successful
		//
	}
	else
	{
		if (TNC->Alerted == FALSE)
		{
			err=WSAGetLastError();
   			i=sprintf(Msg, "Connect Failed for VARA socket - error code = %d\r\n", err);
			WritetoConsole(Msg);
			sprintf(TNC->WEB_COMMSSTATE, "Connection to TNC failed");
			MySetWindowText(TNC->xIDC_COMMSSTATE, TNC->WEB_COMMSSTATE);

			TNC->Alerted = TRUE;
		}
		
		closesocket(TNC->TCPSock);
		TNC->TCPSock = 0;
	 	TNC->CONNECTING = FALSE;
		return;
	}

	// Connect Data Port

	if (connect(TNC->TCPDataSock,(LPSOCKADDR) &TNC->Datadestaddr,sizeof(TNC->Datadestaddr)) == 0)
	{
		//
		//	Connected successful
		//
	}
	else
	{
		if (TNC->Alerted == FALSE)
		{
			err=WSAGetLastError();
   			i=sprintf(Msg, "Connect Failed for VARA Data socket - error code = %d\r\n", err);
			WritetoConsole(Msg);
			sprintf(TNC->WEB_COMMSSTATE, "Connection to TNC failed");
			MySetWindowText(TNC->xIDC_COMMSSTATE, TNC->WEB_COMMSSTATE);

			TNC->Alerted = TRUE;
		}
		
		closesocket(TNC->TCPSock);
		closesocket(TNC->TCPDataSock);
		TNC->TCPSock = 0;
	 	TNC->CONNECTING = FALSE;
		return;
	}


	Sleep(1000);

	TNC->LastFreq = 0;			//	so V4 display will be updated

 	TNC->CONNECTING = FALSE;
	TNC->CONNECTED = TRUE;
	TNC->BusyFlags = 0;
	TNC->InputLen = 0;

	// Send INIT script

	// VARA needs each command in a separate send

	ptr1 = &TNC->InitScript[0];

	// We should wait for first RDY. Cheat by queueing a null command

	GetSemaphore(&Semaphore, 52);

	while(TNC->BPQtoWINMOR_Q)
	{
		buffptr = Q_REM(&TNC->BPQtoWINMOR_Q);

		if (buffptr)
			ReleaseBuffer(buffptr);
	}


	while (ptr1 && ptr1[0])
	{
		unsigned char c;
		
		ptr2 = strchr(ptr1, 13);
		
		if (ptr2)
		{
			c = *(ptr2 + 1);		// Save next char
			*(ptr2 + 1) = 0;		// Terminate string
		}
		VARASendCommand(TNC, ptr1, TRUE);

		if (ptr2)
			*(1 + ptr2++) = c;		// Put char back 

		ptr1 = ptr2;
	}
	
	TNC->Alerted = TRUE;

	sprintf(TNC->WEB_COMMSSTATE, "Connected to VARA TNC");		
	MySetWindowText(TNC->xIDC_COMMSSTATE, TNC->WEB_COMMSSTATE);

	FreeSemaphore(&Semaphore);

	sprintf(Msg, "Connected to VARA TNC Port %d\r\n", TNC->Port);
	WritetoConsole(Msg);


	#ifndef LINBPQ
//	FreeSemaphore(&Semaphore);
	Sleep(1000);		// Give VARA time to update Window title
	EnumWindows(EnumVARAWindowsProc, (LPARAM)TNC);
//	GetSemaphore(&Semaphore, 52);
#endif


	while (TNC->CONNECTED)
	{
		FD_ZERO(&readfs);	
		FD_ZERO(&errorfs);

		FD_SET(TNC->TCPSock,&readfs);
		FD_SET(TNC->TCPSock,&errorfs);

		if (TNC->CONNECTED) FD_SET(TNC->TCPDataSock,&readfs);
			
//		FD_ZERO(&writefs);

//		if (TNC->BPQtoWINMOR_Q) FD_SET(TNC->TCPDataSock,&writefs);	// Need notification of busy clearing
		
		if (TNC->CONNECTING || TNC->CONNECTED) FD_SET(TNC->TCPDataSock,&errorfs);

		timeout.tv_sec = 60;
		timeout.tv_usec = 0;				// We should get messages more frequently that this

		ret = select(TNC->TCPDataSock + 1, &readfs, NULL, &errorfs, &timeout);
		
		if (ret == SOCKET_ERROR)
		{
			Debugprintf("VARA Select failed %d ", WSAGetLastError());
			goto Lost;
		}
		if (ret > 0)
		{
			//	See what happened

			if (FD_ISSET(TNC->TCPSock, &readfs))
			{
				GetSemaphore(&Semaphore, 52);
				VARAProcessReceivedControl(TNC);
				FreeSemaphore(&Semaphore);
			}
								
			if (FD_ISSET(TNC->TCPDataSock, &readfs))
			{
				GetSemaphore(&Semaphore, 52);
				VARAProcessReceivedData(TNC);
				FreeSemaphore(&Semaphore);
			}

			if (FD_ISSET(TNC->TCPSock, &errorfs))
			{
Lost:	
				sprintf(Msg, "VARA Connection lost for Port %d\r\n", TNC->Port);
				WritetoConsole(Msg);

				sprintf(TNC->WEB_COMMSSTATE, "Connection to TNC lost");
				MySetWindowText(TNC->xIDC_COMMSSTATE, TNC->WEB_COMMSSTATE);

				TNC->CONNECTED = FALSE;
				TNC->Alerted = FALSE;

				if (TNC->PTTMode)
					Rig_PTT(TNC->RIG, FALSE);			// Make sure PTT is down

				if (TNC->Streams[0].Attached)
					TNC->Streams[0].ReportDISC = TRUE;

				closesocket(TNC->TCPDataSock);
				TNC->TCPSock = 0;
				return;
			}

			if (FD_ISSET(TNC->TCPDataSock, &errorfs))
			{
				sprintf(Msg, "VARA Connection lost for Port %d\r\n", TNC->Port);
				WritetoConsole(Msg);

				sprintf(TNC->WEB_COMMSSTATE, "Connection to TNC lost");
				MySetWindowText(TNC->xIDC_COMMSSTATE, TNC->WEB_COMMSSTATE);

				TNC->CONNECTED = FALSE;
				TNC->Alerted = FALSE;

				if (TNC->PTTMode)
					Rig_PTT(TNC->RIG, FALSE);			// Make sure PTT is down

				if (TNC->Streams[0].Attached)
					TNC->Streams[0].ReportDISC = TRUE;

				closesocket(TNC->TCPSock);
				closesocket(TNC->TCPDataSock);
				TNC->TCPSock = 0;
				return;
			}
			continue;
		}
		else
		{
			// 60 secs without data. Shouldn't happen

			continue;

			sprintf(Msg, "VARA No Data Timeout Port %d\r\n", TNC->Port);
			WritetoConsole(Msg);

//			sprintf(TNC->WEB_COMMSSTATE, "Connection to TNC lost");
//			GetSemaphore(&Semaphore, 52);
//			MySetWindowText(TNC->xIDC_COMMSSTATE, TNC->WEB_COMMSSTATE);
//			FreeSemaphore(&Semaphore);
	

			TNC->CONNECTED = FALSE;
			TNC->Alerted = FALSE;

			if (TNC->PTTMode)
				Rig_PTT(TNC->RIG, FALSE);			// Make sure PTT is down

			if (TNC->Streams[0].Attached)
				TNC->Streams[0].ReportDISC = TRUE;

//			GetSemaphore(&Semaphore, 52);
//			VARASendCommand(TNC, "CODEC FALSE", FALSE);
//			FreeSemaphore(&Semaphore);

			Sleep(100);
			shutdown(TNC->TCPSock, SD_BOTH);
			Sleep(100);

			closesocket(TNC->TCPDataSock);

			Sleep(100);
			shutdown(TNC->TCPDataSock, SD_BOTH);
			Sleep(100);

			closesocket(TNC->TCPDataSock);

//	if (TNC->PID && TNC->WeStartedTNC)
//	{
//		KillTNC(TNC);
//
			return;
		}
	}
	sprintf(Msg, "VARA Thread Terminated Port %d\r\n", TNC->Port);
	WritetoConsole(Msg);
}


VOID VARAProcessResponse(struct TNCINFO * TNC, UCHAR * Buffer, int MsgLen)
{
	UINT * buffptr;
	struct STREAMINFO * STREAM = &TNC->Streams[0];

	Buffer[MsgLen - 1] = 0;		// Remove CR

	TNC->TimeSinceLast = 0;

	if (TNC->HeartBeat < 2 && strcmp(Buffer, "WRONG") == 0)
		return;			// Link probe

		
	if (_memicmp(Buffer, "PTT ON", 6) == 0)
	{
		TNC->Busy = TNC->BusyHold * 10;				// BusyHold  delay

		if (TNC->PTTMode)
			Rig_PTT(TNC->RIG, TRUE);

		return;
	}

	if (_memicmp(Buffer, "PTT OFF", 6) == 0)
	{
		if (TNC->PTTMode)
			Rig_PTT(TNC->RIG, FALSE);

		return;
	}

	if (_stricmp(Buffer, "BUSY ON") == 0)
	{	
		TNC->BusyFlags |= CDBusy;
		TNC->Busy = TNC->BusyHold * 10;				// BusyHold  delay

		MySetWindowText(TNC->xIDC_CHANSTATE, "Busy");
		strcpy(TNC->WEB_CHANSTATE, "Busy");

		TNC->WinmorRestartCodecTimer = time(NULL);
		return;
	}

	if (_stricmp(Buffer, "BUSY OFF") == 0)
	{
		TNC->BusyFlags &= ~CDBusy;
		if (TNC->BusyHold)
			strcpy(TNC->WEB_CHANSTATE, "BusyHold");
		else
			strcpy(TNC->WEB_CHANSTATE, "Clear");

		MySetWindowText(TNC->xIDC_CHANSTATE, TNC->WEB_CHANSTATE);
		TNC->WinmorRestartCodecTimer = time(NULL);
		return;
	}


	if (_memicmp(&Buffer[0], "PENDING", 7) == 0)	// Save Pending state for scan control
	{
		TNC->ConnectPending = 6;				// Time out after 6 Scanintervals
		Debugprintf(Buffer);
//		WritetoTrace(TNC, Buffer, MsgLen - 1);
		return;
	}

	if (_memicmp(&Buffer[0], "CANCELPENDING", 13) == 0)
	{
		TNC->ConnectPending = FALSE;

		// If a callsign is present it is the calling station - add to MH

		if (TNC->SeenCancelPending == 0)
		{
			WritetoTrace(TNC, Buffer, MsgLen - 1);
			TNC->SeenCancelPending = 1;
		}

		if (Buffer[13] == ' ')
			UpdateMH(TNC, &Buffer[14], '!', 'I');

		return;
	}

	TNC->SeenCancelPending = 0;

	if (strcmp(Buffer, "OK") == 0)
	{
		if (TNC->Streams[0].Connecting == TRUE)
			return;		// Discard response or it will mess up connect scripts

		// Need also to discard response to LISTEN OFF after attach

		if (TNC->DiscardNextOK)
		{
			TNC->DiscardNextOK = 0;
			return;
		}
	}



	if (_memicmp(Buffer, "BUFFER", 6) == 0)
	{
		Debugprintf(Buffer);

		sscanf(&Buffer[7], "%d", &TNC->Streams[0].BytesOutstanding);

		if (TNC->Streams[0].BytesOutstanding == 0)
		{
			// all sent
			
			if (TNC->Streams[0].Disconnecting)						// Disconnect when all sent
			{
				if (STREAM->NeedDisc == 0)
					STREAM->NeedDisc = 60;								// 6 secs
			}
		}
		else
		{
			// Make sure Node Keepalive doesn't kill session.

			TRANSPORTENTRY * SESS = TNC->PortRecord->ATTACHEDSESSIONS[0];

			if (SESS)
			{
				SESS->L4KILLTIMER = 0;
				SESS = SESS->L4CROSSLINK;
				if (SESS)
					SESS->L4KILLTIMER = 0;
			}
		}

		sprintf(TNC->WEB_TRAFFIC, "Sent %d RXed %d Queued %s",
			STREAM->BytesTXed, STREAM->BytesRXed, &Buffer[7]);
		MySetWindowText(TNC->xIDC_TRAFFIC, TNC->WEB_TRAFFIC);
		return;
	}

	if (_memicmp(Buffer, "CONNECTED ", 10) == 0)
	{
		char Call[11];
		char * ptr;
		APPLCALLS * APPL;
		char * ApplPtr = APPLS;
		int App;
		char Appl[10];
		struct WL2KInfo * WL2K = TNC->WL2K;
		int Speed = 0;

		Debugprintf(Buffer);
		WritetoTrace(TNC, Buffer, MsgLen - 1);

		STREAM->ConnectTime = time(NULL); 
		STREAM->BytesRXed = STREAM->BytesTXed = STREAM->PacketsSent = 0;

		memcpy(Call, &Buffer[10], 10);

		ptr = strchr(Call, ' ');	
		if (ptr) *ptr = 0;

		// Get Target Call

		ptr = strchr(&Buffer[10], ' ');	
		if (ptr)
		{
			strcpy(TNC->TargetCall, ++ptr);
		}
	
		TNC->HadConnect = TRUE;

		if (TNC->PortRecord->ATTACHEDSESSIONS[0] == 0)
		{
			TRANSPORTENTRY * SESS;
			
			// Incoming Connect

			// Stop other ports in same group

			SuspendOtherPorts(TNC);

			ProcessIncommingConnectEx(TNC, Call, 0, TRUE, TRUE);
				
			SESS = TNC->PortRecord->ATTACHEDSESSIONS[0];
			
			TNC->ConnectPending = FALSE;

			if (TNC->RIG && TNC->RIG != &TNC->DummyRig && strcmp(TNC->RIG->RigName, "PTT"))
			{
				sprintf(TNC->WEB_TNCSTATE, "%s Connected to %s Inbound Freq %s", TNC->Streams[0].RemoteCall, TNC->TargetCall, TNC->RIG->Valchar);
				SESS->Frequency = (atof(TNC->RIG->Valchar) * 1000000.0) + 1500;		// Convert to Centre Freq
				SESS->Mode = TNC->WL2KMode;
			}
			else
			{
				sprintf(TNC->WEB_TNCSTATE, "%s Connected to %s Inbound", TNC->Streams[0].RemoteCall, TNC->TargetCall);
				if (WL2K)
				{
					SESS->Frequency = WL2K->Freq;
					SESS->Mode = WL2K->mode;
				}
			}
			
			if (WL2K)
				strcpy(SESS->RMSCall, WL2K->RMSCall);

			MySetWindowText(TNC->xIDC_TNCSTATE, TNC->WEB_TNCSTATE);
			
			// Check for ExcludeList

			if (ExcludeList[0])
			{
				if (CheckExcludeList(SESS->L4USER) == FALSE)
				{
					char Status[32];

					TidyClose(TNC, 0);
					sprintf(Status, "%d SCANSTART 15", TNC->Port);
					Rig_Command(-1, Status);
					Debugprintf("VARA Call from %s rejected", Call);
					return;
				}
			}

			// See which application the connect is for

			for (App = 0; App < 32; App++)
			{
				APPL=&APPLCALLTABLE[App];
				memcpy(Appl, APPL->APPLCALL_TEXT, 10);
				ptr=strchr(Appl, ' ');

				if (ptr)
					*ptr = 0;
	
				if (_stricmp(TNC->TargetCall, Appl) == 0)
					break;
			}

			if (App < 32)
			{
				char AppName[13];

				memcpy(AppName, &ApplPtr[App * sizeof(CMDX)], 12);
				AppName[12] = 0;

				// Make sure app is available

				if (CheckAppl(TNC, AppName))
				{
					MsgLen = sprintf(Buffer, "%s\r", AppName);

					buffptr = GetBuff();

					if (buffptr == 0)
					{
						return;			// No buffers, so ignore
					}

					buffptr[1] = MsgLen;
					memcpy(buffptr+2, Buffer, MsgLen);

					C_Q_ADD(&TNC->WINMORtoBPQ_Q, buffptr);
		
					TNC->SwallowSignon = TRUE;

					// Save Appl Call in case needed for 

				}
				else
				{
					char Msg[] = "Application not available\r";
					
					// Send a Message, then a disconenct

					// Send CTEXT First


					if (TNC->Streams[0].BPQtoPACTOR_Q)		//Used for CTEXT
					{
						UINT * buffptr = Q_REM(&TNC->Streams[0].BPQtoPACTOR_Q);
						int txlen=buffptr[1];
						VARASendData(TNC, (UCHAR *)buffptr+2, txlen);
						ReleaseBuffer(buffptr);
					}
				
					VARASendData(TNC, Msg, strlen(Msg));
					STREAM->NeedDisc = 100;	// 10 secs
				}
			}
			return;
		}
		else
		{
			// Connect Complete

			char Reply[80];
			int ReplyLen;
			
			buffptr = GetBuff();

			if (buffptr == 0)
			{
				return;			// No buffers, so ignore
			}
			ReplyLen = sprintf(Reply, "*** Connected to %s\r", TNC->TargetCall);

			buffptr[1] = ReplyLen;
			memcpy(buffptr+2, Reply, ReplyLen);

			C_Q_ADD(&TNC->WINMORtoBPQ_Q, buffptr);

			TNC->Streams[0].Connecting = FALSE;
			TNC->Streams[0].Connected = TRUE;			// Subsequent data to data channel

			if (TNC->RIG && TNC->RIG->Valchar[0])
				sprintf(TNC->WEB_TNCSTATE, "%s Connected to %s Outbound Freq %s",  TNC->Streams[0].MyCall, TNC->Streams[0].RemoteCall, TNC->RIG->Valchar);
			else
				sprintf(TNC->WEB_TNCSTATE, "%s Connected to %s Outbound", TNC->Streams[0].MyCall, TNC->Streams[0].RemoteCall);
			
			MySetWindowText(TNC->xIDC_TNCSTATE, TNC->WEB_TNCSTATE);

			UpdateMH(TNC, TNC->TargetCall, '+', 'O');
			return;
		}
	}


	if (_memicmp(Buffer, "DISCONNECTED", 12) == 0)
	{
		Debugprintf(Buffer);

		TNC->ConnectPending = FALSE;			// Cancel Scan Lock

		if (TNC->StartSent)
		{
			TNC->StartSent = FALSE;		// Disconnect reported following start codec
			return;
		}

		if (TNC->Streams[0].Connecting)
		{
			// Report Connect Failed, and drop back to command mode

			TNC->Streams[0].Connecting = FALSE;

			buffptr = GetBuff();

			if (buffptr == 0)
			{
				return;			// No buffers, so ignore
			}

			buffptr[1] = sprintf((UCHAR *)&buffptr[2], "VARA} Failure with %s\r", TNC->Streams[0].RemoteCall);

			C_Q_ADD(&TNC->WINMORtoBPQ_Q, buffptr);


			if (TNC->RestartAfterFailure)
			{
				if (TNC->PID)
					KillTNC(TNC);

				RestartTNC(TNC);
			}

			return;
		}

		WritetoTrace(TNC, Buffer, MsgLen - 1);

		// Release Session3

		if (TNC->Streams[0].Connected)
		{
			// Create a traffic record
		
			char logmsg[120];	
			time_t Duration;

			Duration = time(NULL) - STREAM->ConnectTime;

			if (Duration == 0)
				Duration = 1;
				
			sprintf(logmsg,"Port %2d %9s Bytes Sent %d  BPS %d Bytes Received %d BPS %d Time %d Seconds",
				TNC->Port, STREAM->RemoteCall,
				STREAM->BytesTXed, (int)(STREAM->BytesTXed/Duration),
				STREAM->BytesRXed, (int)(STREAM->BytesRXed/Duration), (int)Duration);

			Debugprintf(logmsg);
		}


		TNC->Streams[0].Connecting = FALSE;
		TNC->Streams[0].Connected = FALSE;		// Back to Command Mode
		TNC->Streams[0].ReportDISC = TRUE;		// Tell Node

		if (TNC->Streams[0].Disconnecting)		// 
			VARAReleaseTNC(TNC);

		TNC->Streams[0].Disconnecting = FALSE;

		return;
	}

	Debugprintf(Buffer);

	if (_memicmp(Buffer, "FAULT", 5) == 0)
	{
		WritetoTrace(TNC, Buffer, MsgLen - 3);
//		return;
	}

	// Others should be responses to commands

	//	Return others to user (if attached but not connected)

	if (TNC->Streams[0].Attached == 0)
		return;

	if (TNC->Streams[0].Connected)
		return;

	if (MsgLen > 200)
		MsgLen = 200;

	buffptr = GetBuff();

	if (buffptr == 0)
	{
		return;			// No buffers, so ignore
	}
	
	buffptr[1] = sprintf((UCHAR *)&buffptr[2], "VARA} %s\r", Buffer);

	C_Q_ADD(&TNC->WINMORtoBPQ_Q, buffptr);
}

VOID VARAProcessReceivedData(struct TNCINFO * TNC)
{
	int InputLen;

	InputLen = recv(TNC->TCPDataSock, TNC->ARDOPDataBuffer, 8192, 0);

	if (InputLen == 0 || InputLen == SOCKET_ERROR)
	{
		// Does this mean closed?
		
//		closesocket(TNC->TCPSock);
	
		sprintf(TNC->WEB_COMMSSTATE, "Connection to TNC lost");
		MySetWindowText(TNC->xIDC_COMMSSTATE, TNC->WEB_COMMSSTATE);

		TNC->CONNECTED = FALSE;
		TNC->Alerted = FALSE;

		if (TNC->PTTMode)
			Rig_PTT(TNC->RIG, FALSE);			// Make sure PTT is down

		if (TNC->Streams[0].Attached)
			TNC->Streams[0].ReportDISC = TRUE;

		closesocket(TNC->TCPDataSock);
		TNC->TCPSock = 0;


		return;					
	}

	TNC->DataInputLen += InputLen;
		
	VARAProcessDataPacket(TNC, TNC->ARDOPDataBuffer, TNC->DataInputLen);
	TNC->DataInputLen=0;
	return;
}



VOID VARAProcessReceivedControl(struct TNCINFO * TNC)
{
	int InputLen, MsgLen;
	char * ptr, * ptr2;
	char Buffer[4096];

	// shouldn't get several messages per packet, as each should need an ack
	// May get message split over packets

if (TNC->InputLen > 8000)	// Shouldnt have packets longer than this
		TNC->InputLen=0;

	//	I don't think it likely we will get packets this long, but be aware...

	//	We can get pretty big ones in the faster 
				
	InputLen=recv(TNC->TCPSock, &TNC->ARDOPBuffer[TNC->InputLen], 8192 - TNC->InputLen, 0);

	if (InputLen == 0 || InputLen == SOCKET_ERROR)
	{
		// Does this mean closed?
		
		closesocket(TNC->TCPSock);

		TNC->TCPSock = 0;

		TNC->CONNECTED = FALSE;
		TNC->Streams[0].ReportDISC = TRUE;

		sprintf(TNC->WEB_COMMSSTATE, "Connection to TNC lost");
		MySetWindowText(TNC->xIDC_COMMSSTATE, TNC->WEB_COMMSSTATE);

		return;					
	}

	TNC->InputLen += InputLen;

loop:

	ptr = memchr(TNC->ARDOPBuffer, '\r', TNC->InputLen);

	if (ptr == 0)	//  CR in buffer
		return;		// Wait for it

	ptr2 = &TNC->ARDOPBuffer[TNC->InputLen];

	if ((ptr2 - ptr) == 1)	// CR 
	{
		// Usual Case - single meg in buffer

		VARAProcessResponse(TNC, TNC->ARDOPBuffer, TNC->InputLen);
		TNC->InputLen=0;
		return;
	}
	else
	{
		MsgLen = TNC->InputLen - (ptr2-ptr) + 1;	// Include CR 

		memcpy(Buffer, TNC->ARDOPBuffer, MsgLen);

		VARAProcessResponse(TNC, Buffer, MsgLen);

		if (TNC->InputLen < MsgLen)
		{
			TNC->InputLen = 0;
			return;
		}
		memmove(TNC->ARDOPBuffer, ptr + 1,  TNC->InputLen-MsgLen);

		TNC->InputLen -= MsgLen;
		goto loop;
	}	
	return;
}



VOID VARAProcessDataPacket(struct TNCINFO * TNC, UCHAR * Data, int Length)
{
	// Info on Data Socket - just packetize and send on
	
	struct STREAMINFO * STREAM = &TNC->Streams[0];

	int PacLen = 236;
	UINT * buffptr;
		
	TNC->TimeSinceLast = 0;

	STREAM->BytesRXed += Length;

	Data[Length] = 0;	
	Debugprintf("VARA: RXD %d bytes", Length);

	sprintf(TNC->WEB_TRAFFIC, "Sent %d RXed %d Queued %d",
			STREAM->BytesTXed, STREAM->BytesRXed,STREAM->BytesOutstanding);
	MySetWindowText(TNC->xIDC_TRAFFIC, TNC->WEB_TRAFFIC);

	

	//	May need to fragment

	while (Length)
	{
		int Fraglen = Length;

		if (Length > PACLEN)
			Fraglen = PACLEN;

		Length -= Fraglen;

		buffptr = GetBuff();	

		if (buffptr == 0)
			return;			// No buffers, so ignore
				
		memcpy(&buffptr[2], Data, Fraglen);
		
		WritetoTrace(TNC, Data, Fraglen);

		Data += Fraglen;

		buffptr[1] = Fraglen;

		C_Q_ADD(&TNC->WINMORtoBPQ_Q, buffptr);
	}
	return;
}

static VOID TidyClose(struct TNCINFO * TNC, int Stream)
{
	// If all acked, send disc
	
	if (TNC->Streams[0].BytesOutstanding == 0)
		VARASendCommand(TNC, "DISCONNECT\r", TRUE);
}

static VOID ForcedClose(struct TNCINFO * TNC, int Stream)
{
	VARASendCommand(TNC, "ABORT\r", TRUE);
}

static VOID CloseComplete(struct TNCINFO * TNC, int Stream)
{
	VARAReleaseTNC(TNC);

	if (TNC->FECMode)
	{
		TNC->FECMode = FALSE;
//		VARASendCommand(TNC, "SENDID", TRUE);
	}
}


VOID VARASendCommand(struct TNCINFO * TNC, char * Buff, BOOL Queue)
{
	int SentLen;

	if (Buff[0] == 0)		// Terminal Keepalive?
		return;

	if (memcmp(Buff, "LISTEN O", 8) == 0)
		TNC->DiscardNextOK = TRUE;			// Responding to LISTEN  messes up forwarding


	if(TNC->TCPSock)
	{
		SentLen = send(TNC->TCPSock, Buff, strlen(Buff), 0);
		
		if (SentLen != strlen(Buff))
		{			
			int winerr=WSAGetLastError();
			char ErrMsg[80];
				
			sprintf(ErrMsg, "VARA Write Failed for port %d - error code = %d\r\n", TNC->Port, winerr);
			WritetoConsole(ErrMsg);
							
			closesocket(TNC->TCPSock);
			TNC->TCPSock = 0;		
			TNC->CONNECTED = FALSE;
			return;
		}
	}
	return;
}

int VARASendData(struct TNCINFO * TNC, UCHAR * Buff, int Len)
{
	struct STREAMINFO * STREAM = &TNC->Streams[0];

	int bytes=send(TNC->TCPDataSock,(const char FAR *)Buff, Len, 0);
	STREAM->BytesTXed += bytes;
	WritetoTrace(TNC, Buff, Len);
	return bytes;
}

#ifndef LINBPQ

BOOL CALLBACK EnumVARAWindowsProc(HWND hwnd, LPARAM  lParam)
{
	char wtext[128];
	struct TNCINFO * TNC = (struct TNCINFO *)lParam; 
	UINT ProcessId;
	int n;

	n = GetWindowText(hwnd, wtext, 127);

	if (memcmp(wtext,"VARA HF", 7) == 0)
	{
		GetWindowThreadProcessId(hwnd, &ProcessId);

		if (TNC->PID == ProcessId)
		{
			 // Our Process

			wtext[n] = 0;
			sprintf (wtext, "%s- BPQ %s", wtext, TNC->PortRecord->PORTCONTROL.PORTDESCRIPTION);
			SetWindowText(hwnd, wtext);
			return FALSE;
		}
	}
	
	return (TRUE);
}
#endif

VOID VARAReleaseTNC(struct TNCINFO * TNC)
{
	// Set mycall back to Node or Port Call, and Start Scanner

	UCHAR TXMsg[1000];

//	ARDOPChangeMYC(TNC, TNC->NodeCall);

	VARASendCommand(TNC, "LISTEN ON\r", TRUE);

	strcpy(TNC->WEB_TNCSTATE, "Free");
	MySetWindowText(TNC->xIDC_TNCSTATE, TNC->WEB_TNCSTATE);

	//	Start Scanner
				
	sprintf(TXMsg, "%d SCANSTART 15", TNC->Port);

	Rig_Command(-1, TXMsg);

	ReleaseOtherPorts(TNC);

}







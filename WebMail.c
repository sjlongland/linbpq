/*
Copyright 2001-2018 John Wiseman G8BPQ

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

#define _CRT_SECURE_NO_DEPRECATE
#define _USE_32BIT_TIME_T

#include "CHeaders.h"
#include "BPQMail.h"

#define MAIL
#include "HTTPConnectionInfo.h"

#ifdef WIN32
//#include "C:\Program Files (x86)\GnuWin32\include\iconv.h"
#else
#include <iconv.h>
#ifndef MACBPQ
#include <sys/prctl.h>
#endif
#include <dirent.h>
#endif

static struct HTTPConnectionInfo * FindSession(char * Key);
int APIENTRY SessionControl(int stream, int command, int param);
int SetupNodeMenu(char * Buff);
VOID SetMultiStringValue(char ** values, char * Multi);
char * GetTemplateFromFile(int Version, char * FN);
VOID FormatTime(char * Time, time_t cTime);
struct MsgInfo * GetMsgFromNumber(int msgno);
BOOL CheckUserMsg(struct MsgInfo * Msg, char * Call, BOOL SYSOP);
BOOL OkToKillMessage(BOOL SYSOP, char * Call, struct MsgInfo * Msg);
int DisplayWebForm(struct HTTPConnectionInfo * Session, struct MsgInfo * Msg, char * FileName, char * XML, char * Reply, char * RawMessage, int RawLen);
struct HTTPConnectionInfo * AllocateWebMailSession();
VOID SaveNewMessage(struct HTTPConnectionInfo * Session, char * MsgPtr, char * Reply, int * RLen, char * Rest);
void ConvertTitletoUTF8(char * Title, char * UTF8Title);
char *stristr (char *ch1, char *ch2);
char * ReadTemplate(char * FormSet, char * DirName, char * FileName);
VOID DoStandardTemplateSubsitutions(struct HTTPConnectionInfo * Session, char * txtFile);
VOID UndoTransparency(char * ptr);
BOOL CheckifPacket(char * Via);
int GetHTMLFormSet(char * FormSet);
void ProcessFormInput(struct HTTPConnectionInfo * Session, char * input, char * Reply, int * RLen);
char * FindPart(char ** Msg, char * Boundary, int * PartLen);
struct HTTPConnectionInfo * FindWMSession(char * Key);
int SendWebMailHeaderEx(char * Reply, char * Key, struct HTTPConnectionInfo * Session, char * Alert);
char * BuildFormMessage(struct HTTPConnectionInfo * Session, struct MsgInfo * Msg, 	char * Keys[1000], char * Values[1000], int NumKeys);
char * FindXMLVariable(WebMailInfo * WebMail, char * Var);
int ReplyToFormsMessage(struct HTTPConnectionInfo * Session, struct MsgInfo * Msg, char * Reply);
BOOL ParsetxtTemplate(struct HTTPConnectionInfo * Session, struct HtmlFormDir * Dir, char * FN, BOOL isReply);
VOID UpdateFormAction(char * Template, char * Key);
BOOL APIENTRY GetAPRSLatLon(double * PLat,  double * PLon);
BOOL APIENTRY GetAPRSLatLonString(char * PLat,  char * PLon);
void FreeWebMailFields(WebMailInfo * WebMail);
VOID BuildXMLAttachment(struct HTTPConnectionInfo * Session, char * Keys[1000], char * Values[1000], int NumKeys);
VOID SaveTemplateMessage(struct HTTPConnectionInfo * Session, char * MsgPtr, char * Reply, int * RLen, char * Rest);
VOID DownloadAttachments(struct HTTPConnectionInfo * Session, char * Reply, int * RLen, char * Rest);
VOID getAttachmentList(struct HTTPConnectionInfo * Session, char * Reply, int * RLen, char * Rest);

extern char NodeTail[];
extern char BBSName[10];

extern char LTFROMString[2048];
extern char LTTOString[2048];
extern char LTATString[2048];

//static UCHAR BPQDirectory[260];

int LineCount = 35;					// Lines per page on message list

// Forms 

struct HtmlFormDir ** HtmlFormDirs = NULL;
int FormDirCount = 0;

struct HtmlForm
{
	char * FileName;
	BOOL HasInitial;
	BOOL HasViewer;
	BOOL HasReply;
	BOOL HasReplyViewer;
};

struct HtmlFormDir
{
	char * FormSet;
	char * DirName;
	struct HtmlForm ** Forms;
	int FormCount;
	struct HtmlFormDir ** Dirs;		// Nested Directories
	int DirCount;
};


char FormDirList[4][MAX_PATH] = {"Standard_Templates", "Local_Templates"};

static char PassError[] = "<p align=center>Sorry, User or Password is invalid - please try again</p>";
static char BusyError[] = "<p align=center>Sorry, No sessions available - please try later</p>";

extern char MailSignon[];

char WebMailSignon[] = "<html><head><title>BPQ32 Mail Server Access</title></head><body background='/background.jpg'>"
	"<h3 align=center>BPQ32 Mail Server %s Access</h3>"
	"<h3 align=center>Please enter Callsign and Password to access WebMail</h3>"
	"<form method=post action=/WebMail/Signon>"
	"<table align=center  bgcolor=white>"
	"<tr><td>User</td><td><input type=text name=user tabindex=1 size=20 maxlength=50 /></td></tr>" 
	"<tr><td>Password</td><td><input type=password name=password tabindex=2 size=20 maxlength=50 /></td></tr></table>"  
	"<p align=center><input type=submit value=Submit /><input type=submit value=Cancel name=Cancel /></form>";

static char MsgInputPage[] = "<html><head><meta content=\"text/html; charset=ISO-8859-1\" http-equiv=\"content-type\">"
	"<title></title><script src='/WebMail/webscript.js'></script></head>"
	"<body background=/background.jpg onload='initialize(185)' onresize='initialize(185)'>"
	"<h3 align=center>Webmail Interface - Message Input Form</h3>"
	"<form align=center id=myform style=\"font-family: monospace; \" method=post action=/WebMail/EMSave\?%s>"
	"<div style='text-align: center;'><div style='display: inline-block;'><span style='display:block; text-align: left;'>"
	"To &nbsp;&nbsp;&nbsp;&nbsp;&nbsp;<input size=60 id='To' name='To' value='%s'>%s<br>"
	"Subject <input size=60 id='Subj' name='Subj' value='%s'>"
//"<input type='file' name='myFile' multiple>"
	"<br>Type &nbsp;&nbsp;&nbsp;"
	"<select tabindex=1 size=1 name=Type><option value=P>P</option>"
	"<option value=B>B</option><option value=T>T</option></select>"
	" BID <input name=BID><br><br>"
	"</span></div>"
	"<textarea id='main' name=Msg style='overflow:auto;'>%s</textarea><br>"
	"<input name=Send value=Send type=submit><input name=Cancel value=Cancel type=submit></div></form>";

static char CheckFormMsgPage[] = "<html><head><meta content=\"text/html; charset=ISO-8859-1\" http-equiv=\"content-type\">"
	"<title></title><script src='/WebMail/webscript.js'></script></head>"
	"<body background=/background.jpg onload='initialize(210)' onresize='initialize(210)'>"
	"<h3 align=center>Webmail Forms Interface - Check Message</h3>"
	"<form align=center id=myform style=\"font-family: monospace; \"method=post action=/WebMail/FormMsgSave\?%s>"

	"<div style='text-align: center;'><div style='display: inline-block;'><span style='display:block; text-align: left;'>"
	"To &nbsp;&nbsp;&nbsp;&nbsp;&nbsp;<input size=60 id='To' name='To' value='%s'><br>"
	"CC &nbsp;&nbsp;&nbsp;&nbsp;&nbsp;<input size=60 id='CC' name='CC' value='%s'><br>"
	"Subject <input size=60 id='Subj' name='Subj' value='%s'>"
//"<input type='file' name='myFile' multiple>"
	"<br>Type &nbsp;&nbsp;&nbsp;"
	"<select tabindex=1 size=1 name=Type><option value=P>P</option>"
	"<option value=B>B</option><option value=T>T</option></select>"
	" BID <input name=BID><br><br>"
	"</span></div>"

	"<textarea id='main' name=Msg style='overflow:auto;'>%s</textarea><br>"
	"<input name=Send value=Send type=submit><input name=Cancel value=Cancel type=submit></div></form>";


extern char * WebMailTemplate;
extern char * WebMailMsgTemplate;
extern char * jsTemplate;

static char *dat[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
char *longday[] = {"Sunday", "Monday", "Tusday", "Wednesday", "Thusday", "Friday", "Saturday"};

static struct HTTPConnectionInfo * WebSessionList;	// active WebMail sessions

#ifdef LINBPQ
UCHAR * GetBPQDirectory();
#endif

void ReleaseWebMailStruct(WebMailInfo * WebMail)
{
	// release any malloc'ed resources

	if (WebMail == NULL)
		return;

	FreeWebMailFields(WebMail);

	free(WebMail);
	return;
}

VOID FreeWebMailMallocs()
{
	// called when closing. Not really needed, but simplifies tracking down real memory leaks

	struct HTTPConnectionInfo * Session, * SaveNext;
	int i;
	Session = WebSessionList;

	while (Session)
	{
		SaveNext = Session->Next;
			
		// Release amy malloc'ed resouces
			
		ReleaseWebMailStruct(Session->WebMail);
		free(Session);
		Session = SaveNext;
	}

	for (i = 0; i < FormDirCount; i++)
	{
		struct HtmlFormDir * Dir = HtmlFormDirs[i];

		int j;

		for (j = 0; j < Dir->FormCount; j++)
		{
			free(Dir->Forms[j]->FileName);
			free(Dir->Forms[j]);
		}

		if (Dir->DirCount)
		{
			struct HtmlFormDir * SubDir;

			int k, l;

			for (l = 0; l < Dir->DirCount; l++)
			{
				SubDir = Dir->Dirs[l];
		
				for (k = 0; k < Dir->Dirs[l]->FormCount; k++)
				{
					free(SubDir->Forms[k]->FileName);
					free(SubDir->Forms[k]);
				}
				free(SubDir->DirName);
				free(SubDir->Forms);
				free(SubDir->FormSet);

				free(Dir->Dirs[l]);
			}
		}

		free(Dir->DirName);
		free(Dir->Forms);
		free(Dir->FormSet);
		free(Dir);
	}

	free(HtmlFormDirs);

	return;

}


struct HTTPConnectionInfo * AllocateWebMailSession()
{
	int KeyVal;
	struct HTTPConnectionInfo * Session, * SaveNext;
	time_t NOW = time(NULL);

	//	First see if any session records havent been used for a while

	Session = WebSessionList;

	while (Session)
	{
		if (NOW - Session->WebMailLastUsed > 1200)	// 20 Mins
		{
			SaveNext = Session->Next;
			
			// Release amy malloc'ed resouces
			
			ReleaseWebMailStruct(Session->WebMail);

			memset(Session, 0, sizeof(struct HTTPConnectionInfo));
	
			Session->Next = SaveNext;
			goto UseThis;
		}
		Session = Session->Next;
	}
	
	Session = zalloc(sizeof(struct HTTPConnectionInfo));
	
	if (Session == NULL)
		return NULL;

	if (WebSessionList)
		Session->Next = WebSessionList;

	WebSessionList = Session;

UseThis:

	Session->WebMail = zalloc(sizeof(WebMailInfo));

	KeyVal = ((rand() % 100) + 1);

	KeyVal *= time(NULL);

	sprintf(Session->Key, "%c%08X", 'W', KeyVal);

	return Session;
}

struct HTTPConnectionInfo * FindWMSession(char * Key)
{
	struct HTTPConnectionInfo * Session = WebSessionList;

	while (Session)
	{
		if (strcmp(Session->Key, Key) == 0)
		{
			Session->WebMailLastUsed = time(NULL);
			return Session;
		}
		Session = Session->Next;
	}

	return NULL;
}


// Build list of available forms

VOID ProcessFormDir(char * FormSet, char * DirName, struct HtmlFormDir *** xxx, int * DirCount)
{
	struct HtmlFormDir * FormDir;
	struct HtmlFormDir ** FormDirs = *xxx;
	struct HtmlForm * Form;
	char Search[MAX_PATH];
	int count = *DirCount;

#ifdef WIN32
	HANDLE hFind = INVALID_HANDLE_VALUE;
	WIN32_FIND_DATA ffd;
#else
	DIR *dir;
	struct dirent *entry;
	char name[256];
#endif

	FormDir = zalloc(sizeof (struct HtmlFormDir));

	FormDir->DirName = _strdup(DirName);
	FormDir->FormSet = _strdup(FormSet);
	FormDirs=realloc(FormDirs, (count + 1)*4);
	FormDirs[count++] = FormDir;
	
	*DirCount = count;
	*xxx = FormDirs;


	// Scan Directory for .txt files

	sprintf(Search, "%s/%s/%s/*", GetBPQDirectory(), FormSet, DirName);

	// Find the first file in the directory.

#ifdef WIN32

	hFind = FindFirstFile(Search, &ffd);

	if (INVALID_HANDLE_VALUE == hFind) 
		return;

	do
	{
		if (ffd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
		{
			char Dir[MAX_PATH];

			if (strcmp(ffd.cFileName, ".") == 0 || strcmp(ffd.cFileName, "..") == 0)
                continue;

			// Recurse in subdir

			wsprintf(Dir, "%s/%s", DirName, ffd.cFileName);

			ProcessFormDir(FormSet, Dir, &FormDir->Dirs, &FormDir->DirCount);

			continue;

		}
	
		// Add to list

		Form = zalloc(sizeof (struct HtmlForm));

		Form->FileName = _strdup(ffd.cFileName);

		FormDir->Forms=realloc(FormDir->Forms, (FormDir->FormCount + 1)*4);
		FormDir->Forms[FormDir->FormCount++] = Form;       
	}

	while (FindNextFile(hFind, &ffd) != 0);
 
	FindClose(hFind);

#else

	sprintf(Search, "%s/%s/%s", GetBPQDirectory(), FormSet, DirName);

	if (!(dir = opendir(Search)))
	{
		Debugprintf("%s %d %d", "cant open forms dir", errno, dir);
        return ;
	}
    while ((entry = readdir(dir)) != NULL)
	{
        if (entry->d_type == DT_DIR)
		{
            if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
                continue;

			Debugprintf("Recurse %s/%s/%s", FormSet, DirName, entry->d_name);
			continue;

		}
		// see if initial html

//		if (stristr(entry->d_name, "initial.html"))
		{
			// Add to list

			Form = zalloc(sizeof (struct HtmlForm));

			Form->FileName = _strdup(entry->d_name);

			FormDir->Forms=realloc(FormDir->Forms, (FormDir->FormCount + 1)*4);
			FormDir->Forms[FormDir->FormCount++] = Form;
		}
    }
    closedir(dir);
#endif
	return;
}

int GetHTMLForms()
{
	int n = 0;

	while (FormDirList[n][0])
		GetHTMLFormSet(FormDirList[n++]);

	return 0;
}

int GetHTMLFormSet(char * FormSet)
{
	int i;

#ifdef WIN32

	WIN32_FIND_DATA ffd;
	char szDir[MAX_PATH];
	HANDLE hFind = INVALID_HANDLE_VALUE;
	DWORD dwError=0;

   	sprintf(szDir, "%s/%s/*", BPQDirectory, FormSet);

	// Find the first file in the directory.

	hFind = FindFirstFile(szDir, &ffd);

	if (INVALID_HANDLE_VALUE == hFind) 
	{
		return 0;
	}

	// Scan all directories looking for file
	
	do
	{
		if (ffd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
		{
			if (strcmp(ffd.cFileName, ".") == 0 || strcmp(ffd.cFileName, "..") == 0)
                continue;

			// Add to Directory List

			ProcessFormDir(FormSet, ffd.cFileName, &HtmlFormDirs, &FormDirCount);
		}
	}
	
	while (FindNextFile(hFind, &ffd) != 0);

	FindClose(hFind);

#else

	DIR *dir;
	struct dirent *entry;
	char name[256];
	
   	sprintf(name, "%s/%s", BPQDirectory, FormSet);

	if (!(dir = opendir(name)))
	{
		Debugprintf("cant open forms dir %s %d %d", name, errno, dir);
        return 0;
	}

    while ((entry = readdir(dir)) != NULL)
	{
        if (entry->d_type == DT_DIR)
		{
            if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
                continue;

			// Add to Directory List

			ProcessFormDir(FormSet, entry->d_name, &HtmlFormDirs, &FormDirCount);
        }
    }
    closedir(dir);
#endif

	// List for testing

	return 0;

	Debugprintf("%d form dirs", FormDirCount);

	for (i = 0; i < FormDirCount; i++)
	{
		struct HtmlFormDir * Dir = HtmlFormDirs[i];

		int j;
		Debugprintf("%3d %s", Dir->FormCount, Dir->DirName);

		for (j = 0; j < Dir->FormCount; j++)
			Debugprintf("   %s", Dir->Forms[j]->FileName);

		if (Dir->DirCount)
		{
			int k, l;

			for (l = 0; l < Dir->DirCount; l++)
			{
				Debugprintf("Subdir %3d %s", Dir->Dirs[l]->DirCount, Dir->Dirs[l]->DirName);
				for (k = 0; k < Dir->Dirs[l]->FormCount; k++)
					Debugprintf("       %s", Dir->Dirs[l]->Forms[k]->FileName);
			}
		}
	}


	return 0;
}


static int compare(const void *arg1, const void *arg2)
{
   // Compare Calls. Fortunately call is at start of stuct

   return _stricmp(*(char**)arg1 , *(char**)arg2);
}


int SendWebMailHeader(char * Reply, char * Key, struct HTTPConnectionInfo * Session)
{
	return SendWebMailHeaderEx(Reply, Key, Session, NULL);
}


int SendWebMailHeaderEx(char * Reply, char * Key, struct HTTPConnectionInfo * Session, char * Alert)
{
 	// Ex includes an alert string to be sent before message
	
	struct UserInfo * User = Session->User;
	char Messages[8192];
	int m;
	struct MsgInfo * Msg;
	char * ptr = Messages;
	int n = LineCount;
	char Via[64];
	int Count = 0;

	Messages[0] = 0;

	if (Alert && Alert[0])
		ptr += sprintf(Messages, "<script>alert(\"%s\");window.location.href = '/Webmail/WebMail?%s';</script>", Alert, Key); 

	ptr += sprintf(ptr, "%s", "     #  Date  XX   Len To      @       From    Subject\r\n\r\n");

	for (m = LatestMsg; m >= 1; m--)
	{
		Msg = GetMsgFromNumber(m);
		
		if (Msg && CheckUserMsg(Msg, User->Call, User->flags & F_SYSOP))
		{
			char UTF8Title[128];
			
			// List if it is the right type and in the page range we want

			if (Session->WebMailTypes[0] && strchr(Session->WebMailTypes, Msg->type) == 0) 
				continue;

			// All Types or right Type. Check Mine Flag

			if (Session->WebMailMine)
			{
				// Only list if to or from me

				if (strcmp(User->Call, Msg->to) != 0 && strcmp(User->Call, Msg->from) != 0)
					continue;
			}

			if (Count++ < Session->WebMailSkip)
				continue;

			strcpy(Via, Msg->via);
			strlop(Via, '.');

			// make sure title is UTF 8 encoded

			ConvertTitletoUTF8(Msg->title, UTF8Title);
			
			ptr += sprintf(ptr, "<a href=/WebMail/WM?%s&%d>%6d</a> %s %c%c %5d %-8s%-8s%-8s%s\r\n",
				Key, Msg->number, Msg->number,
				FormatDateAndTime(Msg->datecreated, TRUE), Msg->type,
				Msg->status, Msg->length, Msg->to, Via,
				Msg->from, UTF8Title);

			n--;

			if (n == 0)
				break;
		}
	}

	if (WebMailTemplate == NULL)
		WebMailTemplate = GetTemplateFromFile(5, "WebMailPage.txt");

	return sprintf(Reply, WebMailTemplate, BBSName, User->Call, Key, Key, Key, Key, Key, Key, Key, Key, Key, Key, Messages);
}

int ViewWebMailMessage(struct HTTPConnectionInfo * Session, char * Reply, int Number, BOOL DisplayHTML)
{
	char * Key = Session->Key;
	struct UserInfo * User = Session->User;
	WebMailInfo * WebMail = Session->WebMail;
	char * DisplayStyle;

	char Message[100000] = "";
	struct MsgInfo * Msg;
	char * ptr = Message;
	char * MsgBytes, * Save;
	char FullTo[100];
	char UTF8Title[128];
	int Index;
	char * crcrptr;
	char DownLoad[256] = "";

	DisplayStyle = "textarea";			// Prevents interpretation of html and xml

	Msg = GetMsgFromNumber(Number);

	if (Msg == NULL)
	{
		ptr += sprintf(ptr, "Message %d not found\r\n", Number);
		return sprintf(Reply, WebMailTemplate, BBSName, User->Call, Key, Key, Key, Key, Key, Key, Key, Key, Key, Key, Message);
	}

	// New Display so free any old values

	FreeWebMailFields(WebMail);	

	if (!CheckUserMsg(Msg, User->Call, User->flags & F_SYSOP))
	{
		ptr += sprintf(ptr, "Message %d not for you\r", Number);
		return sprintf(Reply, WebMailTemplate, BBSName, User->Call, Key, Key, Key, Key, Key, Key, Key, Key, Key, Key, Message);
	}

	if (_stricmp(Msg->to, "RMS") == 0)
		 sprintf(FullTo, "RMS:%s", Msg->via);
	else
	if (Msg->to[0] == 0)
		sprintf(FullTo, "smtp:%s", Msg->via);
	else
		strcpy(FullTo, Msg->to);

	// make sure title is UTF 8 encoded

	ConvertTitletoUTF8(Msg->title, UTF8Title);

	// if a B2 message diplay B2 Header instead of a locally generated one

	if (Msg->B2Flags == 0)
	{
		ptr += sprintf(ptr, "From: %s%s\nTo: %s\nType/Status: %c%c\nDate/Time: %s\nBid: %s\nTitle: %s\n\n",
			Msg->from, Msg->emailfrom, FullTo, Msg->type, Msg->status, FormatDateAndTime(Msg->datecreated, FALSE), Msg->bid, UTF8Title);
	}

	MsgBytes = Save = ReadMessageFile(Number);

	if (Msg->type == 'P')
		Index = PMSG;
	else if (Msg->type == 'B')
		Index = BMSG;
	else if (Msg->type == 'T')
		Index = TMSG;

	if (MsgBytes)
	{
		if (Msg->B2Flags)
		{
			char * ptr1;
	
			// if message has attachments, display them if plain text

			if (Msg->B2Flags & Attachments)
			{
				int BodyLen, NewLen;
				int i;
				char *ptr2, *attptr;	

				sprintf(DownLoad, "<td><a href=/WebMail/DL?%s&%d>Save Attachments</a></td>", Key, Msg->number);

				WebMail->Files = 0;
			
				ptr1 = MsgBytes;
	
				ptr += sprintf(ptr, "Message has Attachments\r\n\r\n");

				while(*ptr1 != 13)
				{
					ptr2 = strchr(ptr1, 10);	// Find CR

					if (memcmp(ptr1, "Body: ", 6) == 0)
					{
						BodyLen = atoi(&ptr1[6]);
					}

					if (memcmp(ptr1, "File: ", 6) == 0)
					{
						char * ptr3 = strchr(&ptr1[6], ' ');	// Find Space
						*(ptr2 - 1) = 0;

						WebMail->FileLen[WebMail->Files] = atoi(&ptr1[6]);
						WebMail->FileName[WebMail->Files++] = _strdup(&ptr3[1]);
					
					}
				
					ptr1 = ptr2;
					ptr1++;
				}

				ptr1 += 2;			// Over Blank Line and Separator

				// ptr1 is pointing to body. Save for possible reply

				WebMail->Body = malloc(BodyLen + 2);
				memcpy(WebMail->Body, ptr1, BodyLen);
				WebMail->Body[BodyLen] = 0;

				ptr += sprintf(ptr, "%s", ptr1);

				ptr1 += BodyLen + 2;		// to first file

				// Save pointers to file

				attptr = ptr1; 

				for (i = 0; i < WebMail->Files; i++)
				{
					WebMail->FileBody[i] = malloc(WebMail->FileLen[i]);
					memcpy(WebMail->FileBody[i], attptr, WebMail->FileLen[i]);
					attptr += (WebMail->FileLen[i] + 2);
				}

				// if first (only??) attachment is XML and  filename
				// starts "RMS_Express_Form" process as HTML Form

				if (DisplayHTML && _memicmp(ptr1, "<?xml", 4) == 0 && 
					_memicmp(WebMail->FileName[0], "RMS_Express_Form_", 16) == 0)
				{
					int Len = DisplayWebForm(Session, Msg, WebMail->FileName[0], ptr1, Reply, MsgBytes, BodyLen + 32); // 32 for added "has attachments"
					free(MsgBytes);
					return Len;
				}

				for (i = 0; i < WebMail->Files; i++)
				{
					int n;
					char * p = ptr1;
					char c;

					// Check if message is probably binary

					int BinCount = 0;

					NewLen = RemoveLF(ptr1, WebMail->FileLen[i]);		// Removes LF agter CR but not on its own

					for (n = 0; n < NewLen; n++)
					{
						c = *p;
						
						if (c == 10)
							*p = 13;

						if (c==0 || (c & 128))
							BinCount++;

						p++;

					}

					if (BinCount > NewLen/10)
					{
						// File is probably Binary

						ptr += sprintf(ptr, "\rAttachment %s is a binary file\r", WebMail->FileName[i]);
					}
					else
					{
						ptr += sprintf(ptr, "\rAttachment %s\r\r", WebMail->FileName[i]);

						User->Total.MsgsSent[Index] ++;
						User->Total.BytesForwardedOut[Index] += NewLen;
					}
				
					ptr1 += WebMail->FileLen[i];
					ptr1 +=2;				// Over separator
				}

				free(Save);
				return sprintf(Reply, WebMailMsgTemplate, BBSName, User->Call, Msg->number, Msg->number, Key, Msg->number, Key, DownLoad, Key, DisplayStyle, Message, DisplayStyle);
			}
			
			// Remove B2 Headers (up to the File: Line)
			
	//		ptr1 = strstr(MsgBytes, "Body:");

	//		if (ptr1)
	//			MsgBytes = ptr1;
		}

		// Remove lf chars

//		Length = RemoveLF(MsgBytes, strlen(MsgBytes));

		User->Total.MsgsSent[Index] ++;
//		User->Total.BytesForwardedOut[Index] += Length;

		// Body  may have cr cr lf which causes double space

		crcrptr = strstr(MsgBytes, "\r\r");

		while (crcrptr)
		{
			*crcrptr = ' ';
			crcrptr = strstr(crcrptr, "\r\r");
		}


		// if body not UTF-8, convert it

	if (WebIsUTF8(MsgBytes, strlen(MsgBytes)) == FALSE)
	{
		// With Windows it is simple - convert using current codepage
		// I think the only reliable way is to convert to unicode and back

		int origlen = strlen(MsgBytes) + 1;

		UCHAR * BufferB = malloc(2 * origlen);

#ifdef WIN32

		WCHAR * BufferW = malloc(2 * origlen);
		int wlen;
		int len = origlen;

		wlen = MultiByteToWideChar(CP_ACP, 0, MsgBytes, len, BufferW, origlen * 2); 
		len = WideCharToMultiByte(CP_UTF8, 0, BufferW, wlen, BufferB, origlen * 2, NULL, NULL); 
		
		free(Save);
		Save = MsgBytes = BufferB;
		free(BufferW);

#else
		int left = 2 * strlen(MsgBytes);
		int len = strlen(MsgBytes);
		UCHAR * BufferBP = BufferB;
		iconv_t * icu = NULL;

		if (icu == NULL)
			icu = iconv_open("UTF-8", "CP1252");

		iconv(icu, NULL, NULL, NULL, NULL);		// Reset State Machine
		iconv(icu, &MsgBytes, &len, (char ** __restrict__)&BufferBP, &left);
	
		free(Save);
		Save = MsgBytes = BufferB;

#endif

	}


		ptr += sprintf(ptr, "%s", MsgBytes);
		free(Save);

		ptr += sprintf(ptr, "\r\r[End of Message #%d from %s]\r", Number, Msg->from);

		if ((_stricmp(Msg->to, User->Call) == 0) || ((User->flags & F_SYSOP) && (_stricmp(Msg->to, "SYSOP") == 0)))
		{
			if ((Msg->status != 'K') && (Msg->status != 'H') && (Msg->status != 'F') && (Msg->status != 'D'))
			{
				if (Msg->status != 'Y')
				{
					Msg->status = 'Y';
					Msg->datechanged=time(NULL);
				}
			}
		}
	}
	else
	{
		ptr += sprintf(ptr, "File for Message %d not found\r", Number);
	}

	if (DisplayHTML && stristr(Message, "html>"))
		DisplayStyle = "div";				// Use div so HTML and XML are interpreted

	return sprintf(Reply, WebMailMsgTemplate, BBSName, User->Call, Msg->number, Msg->number, Key, Msg->number, Key, DownLoad, Key, DisplayStyle, Message, DisplayStyle);
}

int KillWebMailMessage(char * Reply, char * Key, struct UserInfo * User, int Number)
{
	struct MsgInfo * Msg;
	char Message[100] = "";

	Msg = GetMsgFromNumber(Number);

	if (Msg == NULL)
	{
		sprintf(Message, "Message %d not found", Number);
		goto returnit;
	}

	if (OkToKillMessage(User->flags & F_SYSOP, User->Call, Msg))
	{
		FlagAsKilled(Msg);
		sprintf(Message, "Message #%d Killed\r", Number);
		goto returnit;
	}

	sprintf(Message, "Not your message\r");

returnit:
	return sprintf(Reply, WebMailMsgTemplate, BBSName, User->Call, Msg->number, Msg->number, Key, Msg->number, Key, "", Key, "div", Message, "div");
}

void freeKeys(KeyValues * Keys)
{
	while (Keys->Key)
	{
		free(Keys->Key);
		free(Keys->Value);
		Keys++;
	}
}

void FreeWebMailFields(WebMailInfo * WebMail)
{
	// release any malloc'ed resources

	int i;

	if (WebMail == NULL)
		return;

	if (WebMail->txtFile)
		free(WebMail->txtFile);

	if (WebMail->txtFileName)
		free(WebMail->txtFileName);

	if (WebMail->InputHTMLName)
		free(WebMail->InputHTMLName);
	
	if (WebMail->DisplayHTMLName)
		free(WebMail->DisplayHTMLName);

	if (WebMail->ReplyHTMLName)
		free(WebMail->ReplyHTMLName);

	if (WebMail->To)
		free(WebMail->To);
	if (WebMail->CC)
		free(WebMail->CC);
	if (WebMail->Subject)
		free(WebMail->Subject);
	if (WebMail->Body)
		free(WebMail->Body);
	if (WebMail->XML)
		free(WebMail->XML);
	if (WebMail->XMLName)
		free(WebMail->XMLName);

	if (WebMail->OrigTo)
		free(WebMail->OrigTo);
	if (WebMail->OrigSubject)
		free(WebMail->OrigSubject);
	if (WebMail->OrigBody)
		free(WebMail->OrigBody);

	if (WebMail->BID)
		free(WebMail->BID);

	freeKeys(WebMail->txtKeys);
	freeKeys(WebMail->XMLKeys);

	for (i = 0; i < WebMail->Files; i++)
	{
		free(WebMail->FileBody[i]);
		free(WebMail->FileName[i]);
	}

	memset(WebMail, 0, sizeof(WebMailInfo));
	return;
}


void ProcessWebMailMessage(struct HTTPConnectionInfo * Session, char * Key, BOOL LOCAL, char * Method, char * NodeURL, char * input, char * Reply, int * RLen)
{
	char * URLParams = strlop(Key, '&');
	int ReplyLen;
	char Appl = 'M';

	// Webmail doesn't use the normal Mail Key.

	// webscript.js doesn't need a key

	if (_stricmp(NodeURL, "/WebMail/webscript.js") == 0)
	{
		if (jsTemplate)
			free(jsTemplate);
		
		jsTemplate = GetTemplateFromFile(2, "webscript.js");

		ReplyLen = sprintf(Reply, "HTTP/1.1 200 OK\r\nContent-Length: %d\r\n"
			"Cache-Control: max-age=60\r\nContent-Type: text/javascript\r\n\r\n%s", strlen(jsTemplate), jsTemplate);
		*RLen = ReplyLen;
		return;
	}

	Session = NULL;

	if (Key && Key[0])
		Session = FindWMSession(Key);

	if (Session == NULL)
	{
		//	Lost Session
			
		if (LOCAL)
		{
			Session = AllocateWebMailSession();

			Key = Session->Key;

			Session->User = LookupCall(BBSName);
	
			if (Session->User)
			{
				strcpy(NodeURL, "/WebMail/WebMail");
				Session->WebMailSkip = 0;
				Session->WebMailLastUsed = time(NULL);
			}
		}
		else
		{
			//	Send Login Page

			if (_stricmp(NodeURL, "/WebMail/Signon") != 0)
			{
				ReplyLen = sprintf(Reply, WebMailSignon, BBSName, BBSName);
				*RLen = ReplyLen;
				return;
			}
		}
	}

	if (strcmp(Method, "POST") == 0)
	{	
		if (_stricmp(NodeURL, "/WebMail/Signon") == 0)
		{
			char * msg = strstr(input, "\r\n\r\n");	// End of headers
			char * user, * password, * Key;

			if (msg)
			{
				struct UserInfo * User;

				if (strstr(msg, "Cancel=Cancel"))
				{
//					ReplyLen = SetupNodeMenu(Reply);
//					return ReplyLen;
				}
				// Webmail Gets Here with a dummy Session

				Session = AllocateWebMailSession();

				Key = Session->Key;
		
				user = strtok_s(&msg[9], "&", &Key);
				password = strtok_s(NULL, "=", &Key);
				password = Key;

				Session->User = User = LookupCall(user);

				if (User)
				{
					// Check Password

					if (strcmp(User->pass, password) == 0)
					{
						// send Message Index

						Session->WebMailLastUsed = time(NULL);
						Session->WebMailSkip = 0;
						Session->WebMailMine = FALSE;

						if (WebMailTemplate)
						{
							free(WebMailTemplate);
							WebMailTemplate = NULL;
						}

						*RLen = SendWebMailHeader(Reply, Session->Key, Session);
 						return;
					}
				}

				//	Bad User or Pass

				ReplyLen = sprintf(Reply, WebMailSignon, BBSName, BBSName);
				*RLen = ReplyLen;
				return;
			}
		}

		if (_stricmp(NodeURL, "/WebMail/EMSave") == 0)
		{
			//	Save New Message

			SaveNewMessage(Session, input, Reply, RLen, Key);
			return;
		}

		if (_stricmp(NodeURL, "/WebMail/Submit") == 0)
		{
			// Get the POST data from the page and place in message
			
			char * 	param = strstr(input, "\r\n\r\n");	// End of headers
			WebMailInfo * WebMail = Session->WebMail;

			if (WebMail == NULL)
				return;				// Can't proceed if we have no info on form

			ProcessFormInput(Session, input, Reply, RLen);
			return;
		}

		if (_stricmp(NodeURL, "/WebMail/FormMsgSave") == 0)
		{
			//	Save New Message

			SaveTemplateMessage(Session, input, Reply, RLen, Key);
			return;
		}

		
		// End of POST section
	}

	if (_stricmp(NodeURL, "/WebMail/WMLogout") == 0)
	{
		Session->Key[0] = 0;
		Session->WebMailLastUsed = 0;
		ReplyLen = sprintf(Reply, WebMailSignon, BBSName, BBSName);
		*RLen = ReplyLen;
		return;
	}

	if ((_stricmp(NodeURL, "/WebMail/MailEntry") == 0) ||
		(_stricmp(NodeURL, "/WebMail") == 0) ||
		(_stricmp(NodeURL, "/WebMail/") == 0))
	{
		// Entry from Menu if signed in, continue. If not and Localhost 
		// signin as sysop.

		if (Session->User == NULL)
		{
			// Not yet signed in

			if (LOCAL)
			{
				// Webmail Gets Here with a dummy Session

				Session = AllocateWebMailSession();

				Key = Session->Key;

				Session->User = LookupCall(BBSName);
	
				if (Session->User)
				{
					strcpy(NodeURL, "/WebMail/WebMail");
					Session->WebMailSkip = 0;
					Session->WebMailLastUsed = time(NULL);
				}
			}
			else
			{
				//	Send Login Page

				ReplyLen = sprintf(Reply, WebMailSignon, BBSName, BBSName);
				*RLen = ReplyLen;
				return;
			}
		}
	}

	if (_stricmp(NodeURL, "/WebMail/WebMail") == 0)
	{
		if (WebMailTemplate)
		{
			free(WebMailTemplate);
			WebMailTemplate = NULL;
		}

		Session->WebMailSkip = 0;
		Session->WebMailMine = FALSE;

		*RLen = SendWebMailHeader(Reply, Session->Key, Session);
 		return;
	}

	if (_stricmp(NodeURL, "/WebMail/WMAll") == 0)
	{
		Session->WebMailSkip = 0;
		Session->WebMailTypes[0] = 0;
		Session->WebMailMine = FALSE;

		*RLen = SendWebMailHeader(Reply, Session->Key, Session);
 		return;
	}

	if (_stricmp(NodeURL, "/WebMail/WMB") == 0)
	{
		Session->WebMailSkip = 0;
		strcpy(Session->WebMailTypes, "B");
		Session->WebMailMine = FALSE;

		*RLen = SendWebMailHeader(Reply, Session->Key, Session);
 		return;
	}
		
	if (_stricmp(NodeURL, "/WebMail/WMP") == 0)
	{
		Session->WebMailSkip = 0;
		strcpy(Session->WebMailTypes, "P");
		Session->WebMailMine = FALSE;

		*RLen = SendWebMailHeader(Reply, Session->Key, Session);
 		return;
	}

	if (_stricmp(NodeURL, "/WebMail/WMT") == 0)
	{
		Session->WebMailSkip = 0;
		strcpy(Session->WebMailTypes, "T");
		Session->WebMailMine = FALSE;

		*RLen = SendWebMailHeader(Reply, Session->Key, Session);
 		return;
	}

	if (_stricmp(NodeURL, "/WebMail/WMMine") == 0)
	{
		Session->WebMailSkip = 0;
		Session->WebMailTypes[0] = 0;
		Session->WebMailMine = TRUE;
		
		*RLen = SendWebMailHeader(Reply, Session->Key, Session);
 		return;
	}


	if (_stricmp(NodeURL, "/WebMail/WMSame") == 0)
	{
		*RLen = SendWebMailHeader(Reply, Session->Key, Session);
 		return;
	}
	if (_stricmp(NodeURL, "/WebMail/WMNext") == 0)
	{
		Session->WebMailSkip += LineCount;

		*RLen = SendWebMailHeader(Reply, Session->Key, Session);
 		return;
	}

	if (_stricmp(NodeURL, "/WebMail/WMBack") == 0)
	{
		Session->WebMailSkip -= LineCount;

		if (Session->WebMailSkip < 0)
			Session->WebMailSkip  = 0;

		*RLen = SendWebMailHeader(Reply, Session->Key, Session);
 		return;
	}

	if (memcmp(NodeURL, "/WebMail/Reply/", 15) == 0)
	{
		// Reply to Message

		int n = atoi(&NodeURL[15]);
		struct MsgInfo * Msg;
		char Message[100] = "";
		char Title[100];

		Msg = GetMsgFromNumber(n);

		if (Msg == NULL)
		{
			sprintf(Message, "Message %d not found", n);
			*RLen = sprintf(Reply, "%s", Message);
			return;
		}

		Session->WebMail->Msg = Msg;

		// See if the message was displayed in an HTML form with a reply template

		*RLen = ReplyToFormsMessage(Session, Msg, Reply);

		// If couldn't build reply form use normal text reply

		if (*RLen)
			return;
	
		sprintf(Title, "Re:%s", Msg->title);

		*RLen = sprintf(Reply, MsgInputPage, Key, Msg->from, "", Title , "");
		return;
	}

	if (strcmp(NodeURL, "/WebMail/WM") == 0)
	{
		// Read Message

		int n = 0;
		
		if (URLParams)
			n = atoi(URLParams);

		if (WebMailMsgTemplate)
			free(WebMailMsgTemplate);

		WebMailMsgTemplate = GetTemplateFromFile(4, "WebMailMsg.txt");

		*RLen = ViewWebMailMessage(Session, Reply, n, TRUE);

 		return;
	}

	if (strcmp(NodeURL, "/WebMail/DisplayText") == 0)
	{
		// Read Message

		int n = 0;
		
		if (URLParams)
			n = atoi(URLParams);

		if (WebMailMsgTemplate)
			free(WebMailMsgTemplate);

		WebMailMsgTemplate = GetTemplateFromFile(4, "WebMailMsg.txt");

		*RLen = ViewWebMailMessage(Session, Reply, n, FALSE);

 		return;
	}
	if (memcmp(NodeURL, "/WebMail/WMDel/", 15) == 0)
	{
		// Kill  Message

		int n = atoi(&NodeURL[15]);

		*RLen = KillWebMailMessage(Reply, Session->Key, Session->User, n);

 		return;
	}

	if (_stricmp(NodeURL, "/WebMail/NewMsg") == 0)
	{
		// Add HTML Template Button if we have any HTML Form

		char Button[] = 
			" &nbsp; &nbsp; &nbsp;<script>function myfunc(){"
			" document.getElementById('myform').submit();}</script>"
			"<input type=Button onclick='myfunc()' "
			"value='Use HTML Template'>";
			
		char Temp[1024];

		sprintf(Temp, Button, Key);

		if (FormDirCount == 0)
			*RLen = sprintf(Reply, MsgInputPage, Key, "", "", "", "");
		else
			*RLen = sprintf(Reply, MsgInputPage, Key, "", Temp, "", "");

		return;
	}

	if (_memicmp(NodeURL, "/WebMail/GetPage/", 17) == 0)
	{
		// Read the HTML Template file and do any needed substitutions
	
		WebMailInfo * WebMail = Session->WebMail;
		KeyValues * txtKey = WebMail->txtKeys;

		int DirNo = atoi(&NodeURL[17]);
		char * ptr = strchr(&NodeURL[17], ',');
		int FileNo = 0;
		char * SubDir;
		int SubDirNo;

		struct HtmlFormDir * Dir = HtmlFormDirs[DirNo];
		char * Template;
		char * inptr;
		char FormDir[MAX_PATH] = "";
		char * InputName = NULL;	// HTML to input message
		char * ReplyName = NULL;
		char * To = NULL;
		char * CC = NULL;
		char * Subject = NULL;
		char * MsgBody = NULL;
		char * varptr;
		char * endptr;
		int varlen, vallen = 0;
		char val[256]="";			// replacement text
		char var[100] = "\"";

		char Submit[64];

		SubDir = strlop(&NodeURL[17], ':');
		if (SubDir)
		{
			SubDirNo = atoi(SubDir);
			Dir = Dir->Dirs[SubDirNo];
		}

//		VariableNames[0] = "http://{FormServer}:{FormPort}";

		sprintf(Submit, "/Webmail/Submit?%s", Key);
//		sprintf(Seq, "%d", ++WebMail->SeqNo);
//		VariableValues[0] = Submit;

		if (ptr)
			FileNo = atoi(ptr + 1);

		// First we read the .txt. then get name of input .html from it

		if (WebMail->txtFile)
			free(WebMail->txtFile);

		if (WebMail->txtFileName)
			free(WebMail->txtFileName);

		WebMail->txtFileName = _strdup(Dir->Forms[FileNo]->FileName);

		ParsetxtTemplate(Session, NULL, WebMail->txtFileName, FALSE);

		if (WebMail->InputHTMLName == NULL)
		{
			// This is a plain text template without HTML 

			if (To == NULL)
				To = "";

			if (To[0] == 0 && WebMail->To && WebMail->To[0])
				To = WebMail->To;

			if (CC == NULL)
				CC = "";

			if (CC[0] == 0 && WebMail->CC && WebMail->CC[0])
				CC = WebMail->CC;

			if (Subject == NULL)
				Subject = "";

			if (Subject[0] == 0 && WebMail->Subject && WebMail->Subject[0])
				Subject = WebMail->Subject;
	
			if (MsgBody == NULL)
				MsgBody = "";

			if (MsgBody[0] == 0 && WebMail->Body && WebMail->Body[0])
				MsgBody = WebMail->Body;

			*RLen = sprintf(Reply, CheckFormMsgPage, Key, To, CC, Subject, MsgBody);
			return;
		}

		Template = ReadTemplate(Dir->FormSet, Dir->DirName, WebMail->InputHTMLName);

		if (Template == NULL)
		{
			ReplyLen = sprintf(Reply, "%s", "HTML Template not found");
			*RLen = ReplyLen;
			return;
		}

		// I've going to update the template in situ, as I can't see a better way
		// of making sure all occurances of variables in any order are substituted.
		// The space allocated to Template is twice the size of the file
		// to allow for insertions

		// First find the Form Action string and replace with our URL. It should have
		// action="http://{FormServer}:{FormPort}" but some forms have localhost:8001 instead

		// Also remove the OnSubmit if it contains the standard popup about having to close browser
	
		UpdateFormAction(Template, Key);

		// Search for "{var }" strings in form and replace with
		// corresponding variable

		// we run through the Template once for each variable

		while (txtKey->Key)
		{
			char Key[256] = "{";
		
			strcpy(&Key[1], &txtKey->Key[1]);
		
			ptr = strchr(Key, '>');
			if (ptr)
				*ptr = '}';

			inptr = Template;
			varptr = stristr(inptr, Key);

			while (varptr)
			{
				// Move the remaining message up/down the buffer to make space for substitution

				varlen = strlen(Key);
				if (txtKey->Value)
					vallen = strlen(txtKey->Value);
				else vallen = 0;

				endptr = varptr + varlen;
		
				memmove(varptr + vallen, endptr, strlen(endptr) + 1);		// copy null on end
				memcpy(varptr, txtKey->Value, vallen);

				inptr = endptr + 1;
		
				varptr = stristr(inptr, Key);
			}
			txtKey++;
		}

		// Remove </body></html> from end as we add it on later

		ptr = stristr(Template, "</body></html>");
		
		if (ptr)
			*ptr = 0;
		
		*RLen = sprintf(Reply, "%s", Template);
		free(Template);	
		return;
	}

	if (_memicmp(NodeURL, "/WebMail/GetList/", 17) == 0)
	{
		// Send Select Template Popup

		char * SubDir;
		int DirNo = 0;
		int SubDirNo = 0;
		char popup[10000]; 

		char popuphddr[] = 
			
			"<html><body align=center background='/background.jpg'>"
			"<script>function myFunction() {var x = document.getElementById(\"mySelect\").value;"
			"var Key = \"%s\";"
			"var param = \"toolbar=yes,location=yes,directories=yes,status=yes,menubar,=scrollbars=yes,resizable=yes,titlebar=yes,toobar=yes\";"
			"window.open(\"/WebMail/GetPage/\" + x + \"?\" + Key,\"_self\",param);"
			"}</script>"
			"<p align=center>"
			"Select Required Template from %s<br><br>"
			"<select id=\"mySelect\" onchange=\"myFunction()\">"
			"<option value=-1>No Page Selected";
			
		struct HtmlFormDir * Dir;
		int i;

		SubDir = strlop(&NodeURL[17], ':');
		DirNo = atoi(&NodeURL[17]);

		if (DirNo == -1)
		{
			// User has gone back, then selected "No Folder Selected"

			// For now just stop crash

			DirNo = 0;
		}


		if (SubDir)
			SubDirNo = atoi(SubDir);

		Dir = HtmlFormDirs[DirNo];

		if (SubDir)
			sprintf(popup, popuphddr, Key, Dir->Dirs[SubDirNo]->DirName);
		else
			sprintf(popup, popuphddr, Key, Dir->DirName);

		if (SubDir)
		{
			for (i = 0; i < Dir->Dirs[SubDirNo]->FormCount; i++)
			{		
				char * Name = Dir->Dirs[SubDirNo]->Forms[i]->FileName;

				// We only send if there is a .txt file

				if (_stricmp(&Name[strlen(Name) - 4], ".txt") == 0)
					sprintf(popup, "%s <option value=%d:%d,%d>%s", popup, DirNo, SubDirNo, i, Name);
			}
		}
		else
		{
			for (i = 0; i < Dir->FormCount; i++)
			{
				char * Name = Dir->Forms[i]->FileName;

				// We only send if there is a .txt file

				if (_stricmp(&Name[strlen(Name) - 4], ".txt") == 0)
					sprintf(popup, "%s <option value=%d,%d>%s", popup, DirNo, i, Name);
			}
		}
		sprintf(popup, "%s</select></p>", popup);

		*RLen = sprintf(Reply, "%s", popup);
		return;
	}
	
	if (_stricmp(NodeURL, "/WebMail/DL") == 0)
	{
		getAttachmentList(Session, Reply, RLen, URLParams);
		return;
	}

	if (_stricmp(NodeURL, "/WebMail/GetDownLoad") == 0)
	{
		DownloadAttachments(Session, Reply, RLen, URLParams);
		return;
	}

	ReplyLen = sprintf(Reply, MailSignon, BBSName, BBSName);
	*RLen = ReplyLen;
}

VOID SaveNewMessage(struct HTTPConnectionInfo * Session, char * MsgPtr, char * Reply, int * RLen, char * Rest)
{
	int ReplyLen = 0;
	struct MsgInfo * Msg;
	char * ptr, *input;
	int MsgLen;
	FILE * hFile;
	char Type;
	int Template=0;
	char * via = NULL;
	char BID[32];
	BIDRec * BIDRec;
	char MsgFile[MAX_PATH];
	int WriteLen=0;
	char * Body;
	char * HDest;
	char * Title;
	char * Vptr = NULL;
	char * Context;
	char Prompt[256] = "Message Saved";
	char OrigTo[256];
	WebMailInfo * WebMail = Session->WebMail;
	BOOL SendMsg = FALSE;
	BOOL SendReply = FALSE;

	input = strstr(MsgPtr, "\r\n\r\n");	// End of headers

	if (input == NULL)
		return;

	if (WebMail->txtFileName)
	{
		// Processing Form Input

		SaveTemplateMessage(Session, MsgPtr, Reply, RLen, Rest);
	
		// Prevent re-entry
		
		free(WebMail->txtFileName);
		WebMail->txtFileName = NULL;

		return;
	}

	// We get here both when requesting a template and submitting
	// a message. Check if Send or Cancel is set. If not it is a Template request

	if (strstr(input, "Input=Input"))
		SendMsg = TRUE;
	else if (strstr(input, "Send=Send"))
		SendReply = TRUE;
	else if (strstr(input, "Cancel=Cancel"))
	{
		*RLen = sprintf(Reply, "<html><script>window.location.href = '/Webmail/WebMail?%s';</script>", Session->Key);
		return;
	}

	ptr = strtok_s(input + 4, "&", &Context);

	while (ptr)
	{
		char * val = strlop(ptr, '=');

		if (strcmp(ptr, "To") == 0)
			HDest = val;
		else if (strcmp(ptr, "Subj") == 0)
			Title = val;
		else if (strcmp(ptr, "Type") == 0)
			Type = val[0];
		else if (strcmp(ptr, "BID") == 0)
			strcpy(BID, val);
		else if (strcmp(ptr, "Msg") == 0)
			Body = _strdup(val);
		else if (strcmp(ptr, "Tmpl") == 0)
			Template = 1;

		ptr = strtok_s(NULL, "&", &Context);

	}
	strlop(BID, ' ');
	if (strlen(BID) > 12)
		BID[12] = 0;

	UndoTransparency(BID);
	UndoTransparency(HDest);
	UndoTransparency(Title);
	UndoTransparency(Body);

	if (!SendMsg && !SendReply)		// Must be request for template
	{
		// Save any supplied message fields and Send HTML Template dropdown list

		char popuphddr[] = 
			
			"<html><body align=center background='/background.jpg'>"
//			"<script>if(window.opener != null) {window.opener.close();}</script>"
			"<script>"
			"function myFunction(val) {var x = document.getElementById(val).value;"
			"var Key = \"%s\";"
			"var param = \"toolbar=yes,location=no,directories=no,status=no,menubar=no,scrollbars=yes,resizable=yes,titlebar=yes,toobar=yes\";"
			"window.open(\"/WebMail/GetList/\" + x + \"?\" + Key,\"_self\",param);"
			"}"
			"</script>"
			"<p align=center>"
			" Select Required Template Folder from List<br><br>"
			"<table border=1 cellpadding=2 bgcolor=white>"
			"<tr><th>Standard Templates</th><th>Local Templates</th></tr>"
			"<tr><td width=50%%><select id=\"Sel1\" onchange=\"myFunction('Sel1')\">"
			"<option value=-1>No Folder Selected";

		char NewGroup [] =
			"</select></td><td width=50%% align=center>"
			"<select id=Sel2 onchange=\"myFunction('Sel2')\">"
			"<option value=-1>No Folder Selected";

		char popup[10000];
		struct HtmlFormDir * Dir;
		char * LastGroup;

		int i;

//		if (!HDest || HDest[0] == 0)
//		{
//			*RLen = sprintf(Reply, "%s", "<html><script>"
//				"alert('Enter To Address before selecting form');"
//				"window.history.back();</script></html>");
//			return;
//		}
	
		// Getting new template so free any old values

		FreeWebMailFields(WebMail);	

		// Save values of To Subject and Body from message when Template requested

		WebMail->OrigTo = _strdup(HDest);
		WebMail->OrigSubject = _strdup(Title);
		WebMail->OrigBody = _strdup(Body);

		// Also to active fields in case not changed by form

		WebMail->To = _strdup(HDest);
		WebMail->Subject = _strdup(Title);
		WebMail->Body = _strdup(Body);

		sprintf(popup, popuphddr, Session->Key);

		LastGroup = HtmlFormDirs[0]->FormSet;		// Save so we know when changes

		for (i = 0; i < FormDirCount; i++)
		{
			int n;
			
			Dir = HtmlFormDirs[i];

			if (strcmp(LastGroup, Dir->FormSet) != 0)
			{
				LastGroup = Dir->FormSet;
				sprintf(popup, "%s%s", popup, NewGroup);
			}

			sprintf(popup, "%s <option value=%d>%s", popup, i, Dir->DirName);
			
			// Recurse any Subdirs
			
			n = 0;
			while (n < Dir->DirCount)
			{
				sprintf(popup, "%s <option value=%d:%d>%s", popup, i, n, Dir->Dirs[n]->DirName);
				n++;
			}
		}
		sprintf(popup, "%s</select></td></tr></table></p>", popup);

		*RLen = sprintf(Reply, "%s", popup);
		return;
	}

	MsgLen = strlen(Body);


	/*
		" &nbsp; &nbsp; &nbsp;<script>function myfunc(){"
			" var call = document.getElementById('To').value;"
			"if (call == '') {"
			"call = prompt('Enter To Address', '');"
			"document.getElementById('To').value = call;}"
			" document.getElementById('myform').submit();}</script>"
			"<input type=Button onclick='myfunc()' "
			"value='Use HTML Template'>";
*/
	if (strlen(HDest) > 255)
	{
		*RLen = sprintf(Reply, "%s", "<html><script>alert(\"To: Call too long!\");window.history.back();</script></html>");
		return;
	}

	if (strlen(BID))
	{		
		if (LookupBID(BID))
		{
			// Duplicate bid
			*RLen = sprintf(Reply, "%s", "<html><script>alert(\"Duplicate BID\");window.history.back();</script></html>");
			return;
		}
	}

	if (Type == 'B')
	{
		if (RefuseBulls)
		{
			*RLen = sprintf(Reply, "%s", "<html><script>alert(\"This system doesn't allow sending Bulls\");window.history.back();script></html>");
			return;
		}

		if (Session->User->flags & F_NOBULLS)
		{
			*RLen = sprintf(Reply, "%s", "<html><script>alert(\"You are not allowed to send Bulls\");window.history.back();</script></html>");
			return;
		}
	}
	
	Msg = AllocateMsgRecord();
		
	// Set number here so they remain in sequence
		
	Msg->number = ++LatestMsg;
	MsgnotoMsg[Msg->number] = Msg;

	strcpy(Msg->from, Session->User->Call);

	if (_memicmp(HDest, "rms:", 4) == 0 || _memicmp(HDest, "rms/", 4) == 0)
	{
		Vptr=&HDest[4];
		strcpy(Msg->to, "RMS");
	}
	else if (_memicmp(HDest, "smtp:", 5) == 0)
	{
		if (ISP_Gateway_Enabled)
		{
			Vptr=&HDest[5];
			Msg->to[0] = 0;
		}
	}
	else if (strchr(HDest, '@'))
	{
		strcpy(OrigTo, HDest);

		Vptr = strlop(HDest, '@');

		if (Vptr)
		{
			// If looks like a valid email address, treat as such

			if (strlen(HDest) > 6 || !CheckifPacket(Vptr))
			{
				// Assume Email address

				Vptr = OrigTo;

				if (FindRMS() || strchr(Vptr, '!')) // have RMS or source route
					strcpy(Msg->to, "RMS");
				else if (ISP_Gateway_Enabled)
					Msg->to[0] = 0;
				else
				{		
					*RLen = sprintf(Reply, "%s", "<html><script>alert(\"This system doesn't allow Sending to Internet Email\");window.close();</script></html>");
					return;
		
				}
			}
		}
	}

	else
	{	
		if (strlen(HDest) > 6)
			HDest[6] = 0;
		
		strcpy(Msg->to, _strupr(HDest));
	}

	if (SendBBStoSYSOPCall)
		if (_stricmp(HDest, BBSName) == 0)
			strcpy(Msg->to, SYSOPCall);

	if (Vptr)
	{
		if (strlen(Vptr) > 40)
			Vptr[40] = 0;

		strcpy(Msg->via, _strupr(Vptr));
	}
	else
	{
		// No via. If not local user try to add BBS 
	
		struct UserInfo * ToUser = LookupCall(Msg->to);

		if (ToUser)
		{
			// Local User. If Home BBS is specified, use it

			if (ToUser->HomeBBS[0])
			{
				strcpy(Msg->via, ToUser->HomeBBS);
				sprintf(Prompt, "%s added from HomeBBS. Message Saved", Msg->via);
			}
		}
		else
		{
			// Not local user - Check WP
			
			WPRecP WP = LookupWP(Msg->to);

			if (WP)
			{
				strcpy(Msg->via, WP->first_homebbs);
				sprintf(Prompt, "%s added from WP", Msg->via);
			}
		}
	}

	if (strlen(Title) > 60)
		Title[60] = 0;

	strcpy(Msg->title,Title);
	Msg->type = Type;
	Msg->status = 'N';

	if (strlen(BID) == 0)
		sprintf_s(BID, sizeof(BID), "%d_%s", LatestMsg, BBSName);

	strcpy(Msg->bid, BID);

	Msg->datereceived = Msg->datechanged = Msg->datecreated = time(NULL);

	BIDRec = AllocateBIDRecord();

	strcpy(BIDRec->BID, Msg->bid);
	BIDRec->mode = Msg->type;
	BIDRec->u.msgno = LOWORD(Msg->number);
	BIDRec->u.timestamp = LOWORD(time(NULL)/86400);

	Msg->length = MsgLen;

	sprintf_s(MsgFile, sizeof(MsgFile), "%s/m_%06d.mes", MailDir, Msg->number);
	
	if (WebMail->Body)
		free(WebMail->Body);

	WebMail->Body = Body;

//	BuildFormMessage(Session, Msg);
	hFile = fopen(MsgFile, "wb");
	
	if (hFile)
	{
		WriteLen = fwrite(WebMail->Body, 1, Msg->length, hFile); 
		fclose(hFile);
	}

	MatchMessagetoBBSList(Msg, 0);

	BuildNNTPList(Msg);				// Build NNTP Groups list

	SaveMessageDatabase();
	SaveBIDDatabase();

	*RLen = SendWebMailHeaderEx(Reply, Session->Key, Session, Prompt);

	return;
}








// RMS Express Forms Support

char * GetHTMLViewerTemplate(char * FN)
{
	int i, j, k, l;

	// Seach list of forms for base file (without .html)

	for (i = 0; i < FormDirCount; i++)
	{
		struct HtmlFormDir * Dir = HtmlFormDirs[i];

		for (j = 0; j < Dir->FormCount; j++)
		{
			if (strcmp(FN, Dir->Forms[j]->FileName) == 0)
			{
				return ReadTemplate(Dir->FormSet, Dir->DirName, FN);
			}
		}

		if (Dir->DirCount)
		{
			for (l = 0; l < Dir->DirCount; l++)
			{
				for (k = 0; k < Dir->Dirs[l]->FormCount; k++)
				{
					if (strcmp(FN, Dir->Dirs[l]->Forms[k]->FileName) == 0)
					{
						return ReadTemplate(Dir->FormSet, Dir->DirName, Dir->Dirs[l]->Forms[k]->FileName);
					}
				}
			}
		}
	}

	return NULL;
}

char * ReadTemplate(char * FormSet, char * DirName, char *FileName)
{
	int FileSize;
	char * MsgBytes;
	char MsgFile[265];
	int ReadLen;
	struct stat STAT;
	FILE * hFile;

	sprintf(MsgFile, "%s/%s/%s/%s", GetBPQDirectory(), FormSet, DirName, FileName);

	if (stat(MsgFile, &STAT) != -1)
	{
		hFile = fopen(MsgFile, "rb");
	
		if (hFile == 0)
		{
			MsgBytes = _strdup("File is missing");
			return MsgBytes;
		}

		FileSize = STAT.st_size;
		MsgBytes = malloc(FileSize * 2);		// Allow plenty of room for template substitution
		ReadLen = fread(MsgBytes, 1, FileSize, hFile); 
		MsgBytes[FileSize] = 0;
		fclose(hFile);

		return MsgBytes;
	}
	
	return NULL;
}

int	ReturnRawMessage(struct UserInfo * User, struct MsgInfo * Msg, char * Key, char * Reply, char * RawMessage, int len, char * ErrorString)
{
	char * ErrorMsg = malloc(len + 100);
	char * ptr;
	char DownLoad[256];

	sprintf(DownLoad, "<td><a href=/WebMail/DL?%s&%d>Save Attachments</a></td>", Key, Msg->number);

	RawMessage[strlen(RawMessage)] = '.'; // We null terminated file name 
	RawMessage[strlen(RawMessage)] = ' '; // We null terminated file name 	Len = XML - RawMessage; 

	RawMessage[len] = 0;

	// Body seems to have cr cr lf which causes double space

	ptr = strstr(RawMessage, "\r\r");

	while (ptr)
	{
		*ptr = ' ';
		ptr = strstr(ptr, "\r\r");
	}

	sprintf(ErrorMsg, ErrorString, RawMessage);

	len = sprintf(Reply, WebMailMsgTemplate, BBSName, User->Call, Msg->number, Msg->number, Key, Msg->number, Key, DownLoad, Key, "textarea", ErrorMsg, "textarea");
	free(ErrorMsg);
	return len;
}

char * FindXMLVariable(WebMailInfo * WebMail, char * Var)
{
	KeyValues * XMLKey = &WebMail->XMLKeys[0];

	while (XMLKey->Key)
	{
		if (_stricmp(Var, XMLKey->Key) == 0)
		{
			return XMLKey->Value;
		}

		XMLKey++;
	}
	return NULL;
}


BOOL ParseXML(WebMailInfo * WebMail, char * XMLOrig)
{
	char * XML = _strdup(XMLOrig);		// Get copy so we can mess about with it
	char * ptr1, * ptr2, * ptr3;
	KeyValues * XMLKeys = &WebMail->XMLKeys[0];

	// Extract Fields (stuff between < and >. Ignore Whitespace between fields

	ptr1 = strstr(XML, "<xml_file_version>");

	while (ptr1)
	{
		ptr2 = strchr(++ptr1, '>');

		if (ptr2 == NULL)
			goto quit;

		*ptr2++ = 0;

		ptr3 = strchr(ptr2, '<');	// end of value string
		if (ptr3 == NULL)
			goto quit;

		*ptr3++ = 0;

		XMLKeys->Key = _strdup(ptr1);
		XMLKeys->Value = _strdup(ptr2);

		XMLKeys++;

		ptr1 = strchr(ptr3, '<');
	}




quit:
	free(XML);
	return TRUE;
}
/*
?xml version="1.0"?>
<RMS_Express_Form>
 <form_parameters>
 <xml_file_version>1,0</xml_file_version>
 <rms_express_version>6.0.16.38 Debug Build</rms_express_version>
 <submission_datetime>20181022105202</submission_datetime>
 <senders_callsign>G8BPQ</senders_callsign>
 <grid_square></grid_square>
 <display_form>Alaska_ARES_ICS213_Initial_Viewer.html</display_form>
 <reply_template>Alaska_ARES_ICS213_SendReply.txt</reply_template>
 </form_parameters>
<variables>
 <msgto>g8bpq</msgto>
 <msgcc></msgcc>
 <msgsender>G8BPQ</msgsender>
 <msgsubject></msgsubject>
 <msgbody></msgbody>
 <msgp2p>false</msgp2p>
 <msgisreply>false</msgisreply>
 <msgisforward>false</msgisforward>
 <msgisacknowledgement>false</msgisacknowledgement>
 <SeqNum>G8BPQ-1</SeqNum>
 <Priority>Routine</Priority>
 <HX></HX>
 <OrgStation></OrgStation>
 <Check></Check>
 <OrgLocation>Here</OrgLocation>
 <Time>11:51</Time>
 <Date>2018-10-22</Date>
 <Incident_Name>Test</Incident_Name>
 <To_Name>John</To_Name>
 <From_Name>John</From_Name>
 <Subjectline>Test</Subjectline>
 <DateTime>2018-10-22 11:51</DateTime>
 <Message></Message>
 <Approved_Name>Me</Approved_Name>
 <Approved_PosTitle>Me</Approved_PosTitle>
 <Submit>Submit</Submit>
</variables>
</RMS_Express_Form>
*/


char HTMLNotFoundMsg[] = " *** HTML Template for message not found - displaying raw content ***\r\n\r\n%s";
char VarNotFoundMsg[] = " *** Variable {%s} not found in message - displaying raw content ***\r\n\r\n%s";
char BadXMLMsg[] = " *** XML for Variable {%s} invalid - displaying raw content ***\r\n\r\n%s";
char BadTemplateMsg[] = " *** Template near \"%s\" invalid - displaying raw content ***\r\n\r\n%s";

int DisplayWebForm(struct HTTPConnectionInfo * Session, struct MsgInfo * Msg, char * FileName, char * XML, char * Reply, char * RawMessage, int RawLen)
{
	WebMailInfo * WebMail = Session->WebMail;
	struct UserInfo * User = Session->User;
	char * Key = Session->Key;
	int Len = 0;
	char * Form;
	char * SaveReply = Reply;
	char FN[MAX_PATH] = "";
	char * formptr, * varptr, * xmlptr;
	char * ptr = NULL, * endptr, * xmlend;
	int varlen, xmllen;
	char var[100] = "<";
	KeyValues * KeyValue; 

	if (ParseXML(WebMail, XML))
		ptr = FindXMLVariable(WebMail, "display_form");

	if (ptr == NULL)		// ?? No Display Form Specified??
	{
		// Not found - just display as normal message

		return ReturnRawMessage(User, Msg, Key, Reply, RawMessage, XML - RawMessage, HTMLNotFoundMsg);
	}

	strcpy(FN, ptr);

	Form = GetHTMLViewerTemplate(FN);

	if (Form == NULL)
	{
		// Not found - just display as normal message

		return ReturnRawMessage(User, Msg, Key, Reply, RawMessage, XML - RawMessage, HTMLNotFoundMsg);
	}

	formptr = Form;

	// Search for {var xxx} strings in form and replace with
	// corresponding variable in xml

	// Don't know why, but {MsgOriginalBody} is sent instead of {var MsgOriginalBody}

	varptr = stristr(Form, "{MsgOriginalBody}");
	{
		char * temp, * tempsave;
		char * xvar, * varsave, * ptr;

		if (varptr)
		{
			varptr++;
		
			endptr = strchr(varptr, '}');
			varlen = endptr - varptr;

			if (endptr == NULL || varlen > 99)
			{
				// corrupt template - display raw message
	
				char Err[256];

				varptr[20] = 0;
		
				sprintf(Err, BadTemplateMsg, varptr - 5, "%s");
				return ReturnRawMessage(User, Msg, Key, SaveReply, RawMessage, XML - RawMessage, Err);
			}
	
			memcpy(var + 1, varptr, varlen);
			var[++varlen] = '>';
			var[++varlen] = 0;

			xmlptr = stristr(XML, var);

			if (xmlptr)
			{
				xmlptr += varlen;
			
				xmlend = strstr(xmlptr, "</");
			
				if (xmlend == NULL)
				{
					// Bad XML - return error message

					char Err[256];

					// remove <> from var as it confuses html

					var[strlen(var) -1] = 0;
			
					sprintf(Err, BadXMLMsg, var + 1, "%s");
					return ReturnRawMessage(User, Msg, Key, SaveReply, RawMessage, XML - RawMessage, Err);
				}

				xmllen = xmlend - xmlptr;
			}
			else
			{	
				// Variable not found - return error message

				char Err[256];

				// remove <> from var as it confuses html

				var[strlen(var) -1] = 0;
			
				sprintf(Err, VarNotFoundMsg, var + 1, "%s");
				return ReturnRawMessage(User, Msg, Key, SaveReply, RawMessage, XML - RawMessage, Err);
			}

			// Ok, we have the position of the variable and the substitution text.
			// Copy message up to variable to Result, then copy value

			// We create a copy so we can rescan later.
			// We also need to replace CR or CRLF with <br> 

			xvar = varsave = malloc(xmllen * 2);

			ptr = xmlptr;

			while(ptr < xmlend)
			{
				while (*ptr == '\n')
					ptr++;

				if (*ptr  == '\r')
				{
					*ptr++;
					strcpy(xvar, "<br>");
					xvar += 4;
				}
				else
					*(xvar++) = *(ptr++);
			}
			*xvar = 0;

			temp = tempsave = malloc(strlen(Form) + strlen(XML));

			memcpy(temp, formptr, varptr - formptr - 1);	// omit "{"
			temp += (varptr - formptr - 1);

			strcpy(temp, varsave);
			temp += strlen(varsave);
			free(varsave);

			formptr = endptr + 1;
			strcpy(temp, formptr);	
			strcpy(Form, tempsave);
			free(tempsave);
		}				
	}

	formptr = Form;

	varptr = stristr(Form, "{var ");

	while (varptr)
	{
		varptr+= 5;
		
		endptr = strchr(varptr, '}');

		varlen = endptr - varptr;

		if (endptr == NULL || varlen > 99)
		{
			// corrupt template - display raw message
	
			char Err[256];

			varptr[20] = 0;
		
			sprintf(Err, BadTemplateMsg, varptr - 5, "%s");
			return ReturnRawMessage(User, Msg, Key, SaveReply, RawMessage, XML - RawMessage, Err);
		}
	
		memcpy(var, varptr, varlen);
		var[varlen] = 0;

		KeyValue = &WebMail->XMLKeys[0];

		while (KeyValue->Key)
		{
			if (_stricmp(var, KeyValue->Key) == 0)
			{
				xmllen = strlen(KeyValue->Value);

				// Ok, we have the position of the variable and the substitution text.
				// Copy message up to variable to Result, then copy value

				memcpy(Reply, formptr, varptr - formptr - 5);	// omit "{var "
				Reply += (varptr - formptr - 5);

				strcpy(Reply, KeyValue->Value);
				Reply += xmllen;
				break;
			}

			if (KeyValue->Key == NULL)
			{
				// Not found in XML

				char Err[256];
			
				sprintf(Err, VarNotFoundMsg, var, "%s");
				return ReturnRawMessage(User, Msg, Key, SaveReply, RawMessage, XML - RawMessage, Err);
			}
			KeyValue++;
		}

		formptr = endptr + 1;

		varptr = stristr(endptr, "{var ");
	}	
	
	// copy remaining

	// Remove </body></html> as we add it later

	ptr = strstr(formptr, "</body>");

	if (ptr)
		*ptr = 0;

	strcpy(Reply, formptr);	

	// Add Webmail header between <Body> and form data

	ptr = stristr(SaveReply, "<body");

	if (ptr)
	{
		ptr = strchr(ptr, '>');
		if (ptr)
		{
			char * temp = malloc(strlen(SaveReply) + 1000);
			int len = ++ptr - SaveReply;
			memcpy(temp, SaveReply, len);

			sprintf(&temp[len],
				"<script>function Reply(Num, Key)"
				"{"
				"var param = \"toolbar=yes,location=yes,directories=yes,status=yes,menubar=yes,scrollbars=yes,resizable=yes,titlebar=yes,toobar=yes\";"
				"window.open(\"/WebMail/Reply/\" + Num + \"?\" + Key,\"_self\",param);"
				"}</script>"
				"<h3 align=center> %s Webmail Interface - User %s - Message %d</h3>"
				"<table align=center border=1 cellpadding=2 bgcolor=white><tr>"
				"<td><a href=\"#\" onclick=\"Reply('%d' ,'%s'); return false;\">Reply</a></td>"
				"<td><a href=/WebMail/WMDel/%d?%s>Kill Message</a></td>"
				"<td><a href=/WebMail/DisplayText?%s&%d>Display as Text</a></td>"
				"<td><a href=/WebMail/WMSame?%s>Back to List</a></td>"
				"</tr></table>", BBSName, User->Call, Msg->number, Msg->number, Key, Msg->number, Key,  Key, Msg->number,Key);

			strcat(temp, ptr);

			strcpy(SaveReply, temp);
			free(temp);
		}
	}

	if (Form)
		free(Form);

	return strlen(SaveReply);
}

char * BuildB2Header(WebMailInfo * WebMail, struct MsgInfo * Msg)
{
	// Create B2 Header
	
	char * NewMsg = malloc(100000);
	char * SaveMsg = NewMsg;
	char DateString[80];
	struct tm * tm;

	tm = gmtime(&Msg->datecreated);	
	
	sprintf(DateString, "%04d%02d%02d%02d%02d%02d",
		tm->tm_year+1900, tm->tm_mon+1, tm->tm_mday, tm->tm_hour, tm->tm_min, tm->tm_sec);

	NewMsg += sprintf(NewMsg,
		"MID: %s\r\n"
		"Date: %s\r\n"
		"Type: %s\r\n"
		"From: %s\r\n"
		"To: %s\r\n",
			Msg->bid, DateString, "Private", Msg->from, WebMail->To);

	if (WebMail->CC && WebMail->CC[0])
		NewMsg += sprintf(NewMsg, "CC: %s\r\n", WebMail->CC);

	NewMsg += sprintf(NewMsg,
		"Subject: %s\r\n"
		"Mbo: %s\r\n",
		Msg->title, BBSName);

	NewMsg += sprintf(NewMsg, "Body: %d\r\n", strlen(WebMail->Body));

//	for (n = 0; n < Files; n++)
//	{
//		char * p = FileName[n], * q;
//
		// Remove any path

//		q = strchr(p, '\\');
//					
//		while (q)
//		{
//			if (q)
//				*q++ = 0;
//			p = q;
//			q = strchr(p, '\\');
//		}

	Msg->B2Flags = B2Msg;

	if (WebMail->XML)
	{
		Msg->B2Flags |= Attachments;
		NewMsg += sprintf(NewMsg, "File: %d %s\r\n",
			WebMail->XMLLen, WebMail->XMLName);
	}

	NewMsg += sprintf(NewMsg, "\r\n");		// Blank Line to end header

	return SaveMsg;
}


VOID SaveTemplateMessage(struct HTTPConnectionInfo * Session, char * MsgPtr, char * Reply, int * RLen, char * Rest)
{
	int ReplyLen = 0;
	struct MsgInfo * Msg;
	char * ptr, *input;
	int MsgLen;
	FILE * hFile;
	char Type;
	int Template=0;
	char * via = NULL;
	char BID[32];
	BIDRec * BIDRec;
	char MsgFile[MAX_PATH];
	int WriteLen=0;
	char * Body = NULL;
	char * To = NULL;
	char * CC = NULL;
	char * HDest = NULL;
	char * Title = NULL;
	char * Vptr = NULL;
	char * Context;
	char Prompt[256] = "Message Saved";
	char OrigTo[256];
	WebMailInfo * WebMail = Session->WebMail;
	BOOL SendMsg = FALSE;
	BOOL SendReply = FALSE;
	char * B2Header;

	input = strstr(MsgPtr, "\r\n\r\n");	// End of headers

	if (input == NULL)
		return;
	
	// Prevent re-entry

	free(WebMail->txtFileName);
	WebMail->txtFileName = NULL;

	if (strstr(input, "Cancel=Cancel"))
	{
		*RLen = sprintf(Reply, "<html><script>window.location.href = '/Webmail/WebMail?%s';</script>", Session->Key);
		return;
	}

	ptr = strtok_s(input + 4, "&", &Context);

	while (ptr)
	{
		char * val = strlop(ptr, '=');

		if (strcmp(ptr, "To") == 0)
			HDest = To = val;
		else if (strcmp(ptr, "CC") == 0)
			CC = val;
		else if (strcmp(ptr, "Subj") == 0)
			Title = val;
		else if (strcmp(ptr, "Type") == 0)
			Type = val[0];
		else if (strcmp(ptr, "BID") == 0)
			strcpy(BID, val);
		else if (strcmp(ptr, "Msg") == 0)
			Body = _strdup(val);
		else if (strcmp(ptr, "Tmpl") == 0)
			Template = 1;

		ptr = strtok_s(NULL, "&", &Context);

	}
	strlop(BID, ' ');
	if (strlen(BID) > 12)
		BID[12] = 0;

	UndoTransparency(To);
	UndoTransparency(CC);
	UndoTransparency(BID);
	UndoTransparency(HDest);
	UndoTransparency(Title);
	UndoTransparency(Body);

	MsgLen = strlen(Body);

	// The user could have changed any of the input fields

	free (WebMail->To);
	WebMail->To = _strdup(To);
	if (CC)
	{
		free (WebMail->CC);
		WebMail->CC = _strdup(CC);
	}
	free (WebMail->Subject);
	WebMail->Subject = _strdup(Title);
	free (WebMail->Body);
	WebMail->Body = _strdup(Body);

	// We will put the supplied address in the B2 header
	
	// We may need to change the HDest to make sure message
	// is delivered to Internet or Packet as requested

	if (HDest == NULL || HDest[0] == 0)
	{
		*RLen = sprintf(Reply, "%s", "<html><script>alert(\"To: Call Missing\");window.history.back();</script></html>");
		return;
	}

	if (strlen(HDest) > 255)
	{
//		*RLen = sprintf(Reply, "%s", "<html><script>alert(\"To: Call too long!\");window.history.back();</script></html>");
		return;
	}

	if (strlen(BID))
	{		
		if (LookupBID(BID))
		{
			// Duplicate bid
			*RLen = sprintf(Reply, "%s", "<html><script>alert(\"Duplicate BID\");window.history.back();</script></html>");
			return;
		}
	}

	if (Type == 'B')
	{
		if (RefuseBulls)
		{
			*RLen = sprintf(Reply, "%s", "<html><script>alert(\"This system doesn't allow sending Bulls\");window.history.back();script></html>");
			return;
		}

		if (Session->User->flags & F_NOBULLS)
		{
			*RLen = sprintf(Reply, "%s", "<html><script>alert(\"You are not allowed to send Bulls\");window.history.back();</script></html>");
			return;
		}
	}

	// Treat plain call as @winlink.org unless "dont add winlink.org" flag set on user record, as
	// forms are normally used for Winlink

	if ((Session->User->flags & F_NOWINLINK) == 0)
		if (strchr(HDest, '@') == 0)			// No At
			strcat(HDest, "@winlink.org");

	Msg = AllocateMsgRecord();
		
	// Set number here so they remain in sequence
		
	Msg->number = ++LatestMsg;
	MsgnotoMsg[Msg->number] = Msg;

	strcpy(Msg->from, Session->User->Call);

	if (_memicmp(HDest, "rms:", 4) == 0 || _memicmp(HDest, "rms/", 4) == 0)
	{
		Vptr=&HDest[4];
		strcpy(Msg->to, "RMS");
	}
	else if (_memicmp(HDest, "smtp:", 5) == 0)
	{
		if (ISP_Gateway_Enabled)
		{
			Vptr=&HDest[5];
			Msg->to[0] = 0;
		}
	}
	else if (strchr(HDest, '@'))
	{
		strcpy(OrigTo, HDest);

		Vptr = strlop(HDest, '@');

		if (Vptr)
		{
			// If looks like a valid email address, treat as such

			if (strlen(HDest) > 6 || !CheckifPacket(Vptr))
			{
				// Assume Email address

				Vptr = OrigTo;

				if (FindRMS() || strchr(Vptr, '!')) // have RMS or source route
					strcpy(Msg->to, "RMS");
				else if (ISP_Gateway_Enabled)
					Msg->to[0] = 0;
				else
				{		
					*RLen = sprintf(Reply, "%s", "<html><script>alert(\"This system doesn't allow Sending to Internet Email\");window.close();</script></html>");
					return;
		
				}
			}
		}
	}

	else
	{	
		if (strlen(HDest) > 6)
			HDest[6] = 0;
		
		strcpy(Msg->to, _strupr(HDest));
	}

	if (SendBBStoSYSOPCall)
		if (_stricmp(HDest, BBSName) == 0)
			strcpy(Msg->to, SYSOPCall);

	if (Vptr)
	{
		if (strlen(Vptr) > 40)
			Vptr[40] = 0;

		strcpy(Msg->via, _strupr(Vptr));
	}
	else
	{
		// No via. If not local user try to add BBS 
	
		struct UserInfo * ToUser = LookupCall(Msg->to);

		if (ToUser)
		{
			// Local User. If Home BBS is specified, use it

			if (ToUser->HomeBBS[0])
			{
				strcpy(Msg->via, ToUser->HomeBBS);
				sprintf(Prompt, "%s added from HomeBBS. Message Saved", Msg->via);
			}
		}
		else
		{
			// Not local user - Check WP
			
			WPRecP WP = LookupWP(Msg->to);

			if (WP)
			{
				strcpy(Msg->via, WP->first_homebbs);
				sprintf(Prompt, "%s added from WP", Msg->via);
			}
		}
	}

	if (strlen(Title) > 60)
		Title[60] = 0;

	strcpy(Msg->title,Title);
	Msg->type = Type;
	Msg->status = 'N';

	if (strlen(BID) == 0)
		sprintf_s(BID, sizeof(BID), "%d_%s", LatestMsg, BBSName);

	strcpy(Msg->bid, BID);

	Msg->datereceived = Msg->datechanged = Msg->datecreated = time(NULL);

	BIDRec = AllocateBIDRecord();

	strcpy(BIDRec->BID, Msg->bid);
	BIDRec->mode = Msg->type;
	BIDRec->u.msgno = LOWORD(Msg->number);
	BIDRec->u.timestamp = LOWORD(time(NULL)/86400);

	Msg->length = MsgLen;

	sprintf_s(MsgFile, sizeof(MsgFile), "%s/m_%06d.mes", MailDir, Msg->number);
	
	// We write a B2 Header, Body and XML attachment if present


//	BuildFormMessage(Session, Msg);

	B2Header = BuildB2Header(WebMail, Msg);

	hFile = fopen(MsgFile, "wb");
	
	if (hFile)
	{
		WriteLen = fwrite(B2Header, 1, strlen(B2Header), hFile); 
		WriteLen += fwrite(WebMail->Body, 1, Msg->length, hFile); 
		WriteLen += fwrite("\r\n", 1, 2, hFile); 
		WriteLen += fwrite(WebMail->XML, 1, WebMail->XMLLen, hFile); 
		WriteLen += fwrite("\r\n", 1, 2, hFile); 
		fclose(hFile);
	}

	free(B2Header);

	Msg->length = WriteLen;

	MatchMessagetoBBSList(Msg, 0);

	BuildNNTPList(Msg);				// Build NNTP Groups list

	SaveMessageDatabase();
	SaveBIDDatabase();

	*RLen = SendWebMailHeaderEx(Reply, Session->Key, Session, Prompt);

	return;
}










VOID DoStandardTemplateSubsitutions(struct HTTPConnectionInfo * Session, char * txtFile)
{
	WebMailInfo * WebMail = Session->WebMail;
	struct UserInfo * User = Session->User;
	KeyValues * txtKey = WebMail->txtKeys;

	char * inptr, * varptr, * endptr;
	int varlen, vallen;

	while (txtKey->Key != NULL)
	{
		inptr = WebMail->txtFile;

		varptr = stristr(inptr, txtKey->Key);

		while (varptr)
		{
			// Move the remaining message up/down the buffer to make space for substitution

			varlen = strlen(txtKey->Key);

			if (txtKey->Value)
				vallen = strlen(txtKey->Value);
			else
				vallen = 0;

			endptr = varptr + varlen;
		
			memmove(varptr + vallen, endptr, strlen(endptr) + 1);		// copy null on end
			memcpy(varptr, txtKey->Value, vallen);

			inptr = endptr + 1;
		
			varptr = stristr(inptr, txtKey->Key);
		}
		txtKey++;
	}
}



VOID BuildMessageFromHTMLInput(struct HTTPConnectionInfo * Session, char * Reply, int * RLen, char * Keys[1000], char * Values[1000], int NumKeys)
{
	int ReplyLen = 0;
	struct MsgInfo * Msg;
	int MsgLen;
	FILE * hFile;
	char Type = 'P';
	BIDRec * BIDRec;
	char * MailBuffer;
	char MsgFile[MAX_PATH];
	int WriteLen=0;
	char Prompt[256] = "Message Saved";
	char OrigTo[256];
	WebMailInfo * WebMail = Session->WebMail;
	char * HDest = _strdup(WebMail->To);
	char * Vptr = NULL;
	char BID[16] = "";

///	if (strlen(HDest) > 255)
///	{
//		*RLen = sprintf(Reply, "%s", "<html><script>alert(\"To: Call too long!\");</script></html>");
//		return;
//	}

	MsgLen = strlen(WebMail->Body);	
	Msg = AllocateMsgRecord();
		
	// Set number here so they remain in sequence
		
	Msg->number = ++LatestMsg;
	MsgnotoMsg[Msg->number] = Msg;

	strcpy(Msg->from, Session->User->Call);

	if (_memicmp(HDest, "rms:", 4) == 0 || _memicmp(HDest, "rms/", 4) == 0)
	{
		Vptr=&HDest[4];
		strcpy(Msg->to, "RMS");
	}
	else if (_memicmp(HDest, "smtp:", 5) == 0)
	{
		if (ISP_Gateway_Enabled)
		{
			Vptr=&HDest[5];
			Msg->to[0] = 0;
		}
	}
	else if (strchr(HDest, '@'))
	{
		strcpy(OrigTo, HDest);

		Vptr = strlop(HDest, '@');

		if (Vptr)
		{
			// If looks like a valid email address, treat as such

			if (strlen(HDest) > 6 || !CheckifPacket(Vptr))
			{
				// Assume Email address

				Vptr = OrigTo;

				if (FindRMS() || strchr(Vptr, '!')) // have RMS or source route
					strcpy(Msg->to, "RMS");
				else if (ISP_Gateway_Enabled)
					Msg->to[0] = 0;
				else
				{		
					*RLen = sprintf(Reply, "%s", "<html><script>alert(\"This system doesn't allow Sending to Internet Email\");window.close();</script></html>");
					return;
		
				}
			}
		}
	}

	else
	{	
		if (strlen(HDest) > 6)
			HDest[6] = 0;
		
		strcpy(Msg->to, _strupr(HDest));
	}

	if (SendBBStoSYSOPCall)
		if (_stricmp(HDest, BBSName) == 0)
			strcpy(Msg->to, SYSOPCall);

	if (Vptr)
	{
		if (strlen(Vptr) > 40)
			Vptr[40] = 0;

		strcpy(Msg->via, _strupr(Vptr));
	}
	else
	{
		// No via. If not local user try to add BBS 
	
		struct UserInfo * ToUser = LookupCall(Msg->to);

		if (ToUser)
		{
			// Local User. If Home BBS is specified, use it

			if (ToUser->HomeBBS[0])
			{
				strcpy(Msg->via, ToUser->HomeBBS);
				sprintf(Prompt, "%s added from HomeBBS", Msg->via);
			}
		}
		else
		{
			// Not local user - Check WP
			
			WPRecP WP = LookupWP(Msg->to);

			if (WP)
			{
				strcpy(Msg->via, WP->first_homebbs);
				sprintf(Prompt, "%s added from WP", Msg->via);
			}
		}
	}

	if (strlen(WebMail->Subject) > 60)
		WebMail->Subject[60] = 0;

	strcpy(Msg->title, WebMail->Subject);
	Msg->type = Type;
	Msg->status = 'N';

	if (strlen(BID) == 0)
		sprintf_s(BID, sizeof(BID), "%d_%s", LatestMsg, BBSName);

	strcpy(Msg->bid, BID);

	Msg->datereceived = Msg->datechanged = Msg->datecreated = time(NULL);

	BIDRec = AllocateBIDRecord();

	strcpy(BIDRec->BID, Msg->bid);
	BIDRec->mode = Msg->type;
	BIDRec->u.msgno = LOWORD(Msg->number);
	BIDRec->u.timestamp = LOWORD(time(NULL)/86400);

	MailBuffer = malloc(MsgLen + 2000);		// Allow for a B2 Header if attachments

	Msg->length = MsgLen;

	BuildFormMessage(Session, Msg, Keys, Values, NumKeys);

	sprintf_s(MsgFile, sizeof(MsgFile), "%s/m_%06d.mes", MailDir, Msg->number);
	
	hFile = fopen(MsgFile, "wb");
	
	if (hFile)
	{
		WriteLen = fwrite(WebMail->Body, 1, Msg->length, hFile); 
		fclose(hFile);
	}
	
	free(WebMail->Body);
	free(HDest);

	WebMail->Body = NULL;

	MatchMessagetoBBSList(Msg, 0);

	BuildNNTPList(Msg);				// Build NNTP Groups list

	SaveMessageDatabase();
	SaveBIDDatabase();

	*RLen = sprintf(Reply, "<html><script>alert(\"%s\");window.close();</script></html>", Prompt);

	return;
}









void ProcessFormInput(struct HTTPConnectionInfo * Session, char * input, char * Reply, int * RLen)
{
	// If there is no display html defined place data in a normal
	// input window, else build the Message body and XML attachment and send

	// I now think it is better to put data in a normal input window
	// even if there is a display form so user can view it before submission

	WebMailInfo * WebMail = Session->WebMail;

	char * info = strstr(input, "\r\n\r\n"); // To end of HTML header

	// look through header for Content-Type line, and if multipart
	// find boundary string.

	char * ptr, * saveptr, * ptr1, * ptr2, * inptr;
	char Boundary[1000];
	BOOL Multipart = FALSE;
	int Partlen;
	char ** Body = &info;
	int i;
	char * Keys[1000];
	char * Values[1000];
	char * saveForfree[1000];

	int NumKeys = 0;
	char * varptr;
	char * endptr;
	int varlen, vallen = 0;
	char *crcrptr;

	ptr = input;

	while(*ptr != 13)
	{
		ptr2 = strchr(ptr, 10);	// Find CR

		while(ptr2[1] == ' ' || ptr2[1] == 9)		// Whitespace - continuation line
		{
			ptr2 = strchr(&ptr2[1], 10);	// Find CR
		}

//		Content-Type: multipart/mixed;
//	boundary="----=_NextPart_000_025B_01CAA004.84449180"
//		7.2.2 The Multipart/mixed (primary) subtype
//		7.2.3 The Multipart/alternative subtype


		if (_memicmp(ptr, "Content-Type: ", 14) == 0)
		{
			char Line[1000] = "";
			char lcLine[1000] = "";

			char * ptr3;

			memcpy(Line, &ptr[14], ptr2-ptr-14);
			memcpy(lcLine, &ptr[14], ptr2-ptr-14);
			_strlwr(lcLine);

			if (_memicmp(Line, "Multipart/", 10) == 0)
			{
				Multipart = TRUE;

	
				ptr3 = strstr(Line, "boundary");

				if (ptr3)
				{
					ptr3+=9;

					if ((*ptr3) == '"')
						ptr3++;

					strcpy(Boundary, ptr3);
					ptr3 = strchr(Boundary, '"');
					if (ptr3) *ptr3 = 0;
					ptr3 = strchr(Boundary, 13);			// CR
					if (ptr3) *ptr3 = 0;

				}
				else
					return;						// Can't do anything without a boundary ??
			}

		}

		ptr = ptr2;
		ptr++;

	}

	if (info == NULL)
		return;		// Wot!

	// Extract the Key/Value pairs from input data

	saveptr = ptr = FindPart(Body, Boundary, &Partlen);

	if (ptr == NULL)
		return;			// Couldn't find separator

	// Now extract fields

	while (ptr)
	{
		char * endptr;
		char * val;
//		Debugprintf(ptr);
	
		// Format seems to be

		//Content-Disposition: form-data; name="FieldName"
		// crlf crlf
		// field value
		// crlf crlf

		// No, is actually

		// ------WebKitFormBoundary7XHZ1i7Jc8tOZJbw
		// Content-Disposition: form-data; name="State"
		//
		// UK
		// ------WebKitFormBoundary7XHZ1i7Jc8tOZJbw

		// ie Value is terminated by ------WebKitFormBoundary7XHZ1i7Jc8tOZJbw
		// But FindPart has returned length, so can use that
		// Be aware that Part and PartLen include the CRLF which is actually part of the Boundary string so should be removed.


		ptr = strstr(ptr, "name=");
	
		if (ptr)
		{
			endptr = strstr(ptr, "\"\r\n\r\n");		// "/r/n/r/n
			if (endptr)
			{
				*endptr = 0;
				ptr += 6;			// to start of name string
				val = endptr + 5;

				// val was Null Terminated by FindPart so can just use it. This assumes all fields are text,
				// which I think is safe enough here.

				saveptr[Partlen - 2] = 0;

				// Now have key value pair

				Keys[NumKeys] = ptr;
				Values[NumKeys] = val;
				saveForfree[NumKeys++] = saveptr;		// so we can free() when finisged with it
			}
			else
				free(saveptr);
		}
		else
			free(saveptr);

		saveptr = ptr = FindPart(Body, Boundary, &Partlen);
	}

	if (info == NULL)
		return;		// Wot!

	info += 4;

	// It looks like some standard variables can be used in <var subsitutions as well as fields from form. Put on end, 
	// so fields from form have priority


	saveForfree[NumKeys]  = 0;		// flag end of malloced items

	Keys[NumKeys] = "MsgTo";
	Values[NumKeys++]  = "";
	Keys[NumKeys] = "MsgCc";
	Values[NumKeys++] = "";
	Keys[NumKeys] = "MsgSender";
	Values[NumKeys++] = Session->User->Call;
	Keys[NumKeys] = "MsgSubject";
	Values[NumKeys++] = "";
	Keys[NumKeys] = "MsgBody";
//	if (WebMail->OrigBody)
//		txtKey++->Value = _strdup(WebMail->OrigBody);
//	else
		Values[NumKeys++] = "";

	Keys[NumKeys] = _strdup("MsgP2P");
	Values[NumKeys++] = _strdup("");

	Keys[NumKeys] = _strdup("MsgIsReply");
	if (WebMail->isReply)
		Values[NumKeys++] = "True";
	else
		Values[NumKeys++] = "True";

	Keys[NumKeys] = "MsgIsForward";
	Values[NumKeys++] = "False";
	Keys[NumKeys] = "MsgIsAcknowledgement";
	Values[NumKeys++] = "False";


	// Update Template with variables from the form
	
	// I've going to update the template in situ, as I can't see a better way
	// of making sure all occurances of variables in any order are substituted.
	// The space allocated to Template is twice the size of the file
	// to allow for insertions

	inptr = WebMail->txtFile;

	// Search for "<var>" strings in form and replace with
	// corresponding variable

	// we run through the Template once for each variable

	i = 0;

	while (i < NumKeys)
	{
		char Key[256];
		
		sprintf(Key, "<var %s>", Keys[i]);
		
		inptr = WebMail->txtFile;
		varptr = stristr(inptr, Key);

		while (varptr)
		{
			// Move the remaining message up/down the buffer to make space for substitution

			varlen = strlen(Key);
			vallen = strlen(Values[i]);

			endptr = varptr + varlen;
		
			memmove(varptr + vallen, endptr, strlen(endptr) + 1);		// copy null on end
			memcpy(varptr, Values[i], vallen);

			inptr = endptr + 1;
		
			varptr = stristr(inptr, Key);
		}
		i++;
	}

	// We need to look for To:, CC: and Subject lines, and remove any other
	// Var: lines. Use everything following Msg: as the plain text body
	
	// Find start of Message body

	ptr = WebMail->txtFile;

	ptr1 = strchr(ptr, '\r');
		
	while (ptr1)
	{
		if (_memicmp(ptr, "Msg:", 4) == 0)
		{
			// Rest is message body. <var> substitutions have been done

			if (WebMail->Body)
				free(WebMail->Body);

			WebMail->Body = _strdup(ptr + 4);	// Remove Msg:
			break;
		}

		// Can now terminate lines

		*ptr1++ = 0;

		while (*ptr1 == '\r' || *ptr1 == '\n')
			*ptr1++ = 0;

		if (_memicmp(ptr, "To:", 3) == 0)
		{
			if (strlen(ptr) > 5)
				WebMail->To = _strdup(&ptr[3]);
		}
		else if (_memicmp(ptr, "CC:", 3) == 0)
		{
			if (strlen(ptr) > 5)
				WebMail->CC = _strdup(&ptr[3]);
		}
		else if (_memicmp(ptr, "Subj:", 5) == 0)
		{
			if (ptr[5] == ' ')		// May have space after :
				ptr++;
			if (strlen(ptr) > 6)
				WebMail->Subject = _strdup(&ptr[5]);
		}

		else if (_memicmp(ptr, "Subject:", 8) == 0)
		{
			if (ptr[8] == ' ')
				ptr++;
			if (strlen(ptr) > 9)
				WebMail->Subject = _strdup(&ptr[8]);
		}
		ptr = ptr1;
		ptr1 = strchr(ptr, '\r');
	}

	if (WebMail->Subject == NULL)
		 WebMail->Subject = _strdup("");

	if (WebMail->To == NULL)
		 WebMail->To = _strdup("");

	if (WebMail->CC == NULL)
		 WebMail->CC = _strdup("");

	// Build XML Attachment if Display Form is defined

	if (WebMail->DisplayHTMLName)
		BuildXMLAttachment(Session, Keys, Values, NumKeys);

	// if Reply, attach original message to Body;

	if (WebMail->isReply && WebMail->OrigBody)
	{
		char * NewBody = malloc(strlen(WebMail->Body) + strlen(WebMail->OrigBody) + 100);
		sprintf(NewBody, "%s\r\n%s", WebMail->Body, WebMail->OrigBody);
		free(WebMail->Body);
		WebMail->Body = NewBody;
	}

	// Display Message for user to check and send

	// fix any cr cr lf sequence
	
	crcrptr = strstr(WebMail->Body, "\r\r");

	while (crcrptr)
	{
		*crcrptr = ' ';
		crcrptr = strstr(crcrptr, "\r\r");
	}

	*RLen = sprintf(Reply, CheckFormMsgPage, Session->Key, WebMail->To, WebMail->CC, WebMail->Subject, WebMail->Body);

	// Free the part strings 

	i = 0;

	while (saveForfree[i])
		free(saveForfree[i++]);
}

// XML Template Stuff

char XMLHeader [] = 
	"<?xml version=\"1.0\"?>\r\n"
	"<RMS_Express_Form>\r\n"
	" <form_parameters>\r\n"
	" <xml_file_version>%s</xml_file_version>\r\n"
	" <rms_express_version>%s</rms_express_version>\r\n"
	" <submission_datetime>%s</submission_datetime>\r\n"
	" <senders_callsign>%s</senders_callsign>\r\n"
	" <grid_square>%s</grid_square>\r\n"
	" <display_form>%s</display_form>\r\n"
	" <reply_template>%s</reply_template>\r\n"
	" </form_parameters>\r\n"
	"<variables>\r\n"
	" <msgto>%s</msgto>\r\n"
    " <msgcc>%s</msgcc>\r\n"
    " <msgsender>%s</msgsender>\r\n"
    " <msgsubject>%s</msgsubject>\r\n"
    " <msgbody>%s</msgbody>\r\n"
    " <msgp2p>%s</msgp2p>\r\n"
    " <msgisreply>%s</msgisreply>\r\n"
    " <msgisforward>%s</msgisforward>\r\n"
    " <msgisacknowledgement>%s</msgisacknowledgement>\r\n";


char XMLLine[] = " <%s>%s</%s>\r\n";

char XMLTrailer[] = "</variables>\r\n</RMS_Express_Form>\r\n";

char * doXMLTransparency(char * string)
{
	// Make sure string doesn't contain forbidden XML chars (<>"'&)

	char * newstring = malloc(5 * strlen(string));

	char * in = string;
	char * out = newstring;
	char c;

	c = *(in++);

	while (c)
	{
		switch (c)
		{
		case '<':

			strcpy(out, "&lt;");
			out += 4;
			break;

		case '>':

			strcpy(out, "&gt;");
			out += 4;
			break;

		case '"':

			strcpy(out, "&quot;");
			out += 6;
			break;

		case '\'':

			strcpy(out, "&lt;");
			out += 6;
			break;

		case '&':

			strcpy(out, "&amp;");
			out += 5;
			break;

		default:

			*(out++) = c;
		}
		c = *(in++);
	}

	*(out++) = 0;
	return newstring;
}
 

VOID BuildXMLAttachment(struct HTTPConnectionInfo * Session, char * Keys[1000], char * Values[1000], int NumKeys)
{
	// Create XML Attachment for form

	WebMailInfo * WebMail = Session->WebMail;
	char XMLName[MAX_PATH];
	char * XMLPtr;
	char DateString[80];
	struct tm * tm;
	time_t NOW = time(NULL);

	int n;
	int TotalFileSize = 0;
		
	tm = gmtime(&NOW);	
	
	sprintf(DateString, "%04d%02d%02d%02d%02d%02d",
		tm->tm_year+1900, tm->tm_mon+1, tm->tm_mday, tm->tm_hour, tm->tm_min, tm->tm_sec);

	strcpy(XMLName, WebMail->DisplayHTMLName);
	XMLName[strlen(XMLName) - 5] = 0;	// remove .html

	WebMail->XMLName = malloc(MAX_PATH);
	WebMail->XML = XMLPtr = malloc(100000);
	WebMail->XMLLen = 0;

	sprintf(WebMail->XMLName, "RMS_Express_Form_%s.xml", XMLName);

	XMLPtr += sprintf(XMLPtr, XMLHeader,
		"1,0", VersionString,
		DateString,
		Session->User->Call,
		"", //Grid
		WebMail->DisplayHTMLName,
		WebMail->ReplyHTMLName,
		WebMail->To,
		WebMail->CC,
		Session->User->Call,
		WebMail->OrigSubject,
		"", //	WebMail->OrigBody,
		"False",			// P2P
		WebMail->isReply ? "True": "False",
		"False",			// Forward,
		"False");			// Ack

	// create XML lines for Key/Value Pairs

	for (n = 0; n < NumKeys; n++)
	{
		if (Values[n] == NULL)
			Values[n] = _strdup("");

		XMLPtr += sprintf(XMLPtr, XMLLine, Keys[n], Values[n], Keys[n]);
	}
	if (WebMail->isReply)
	{
		if (WebMail->OrigBody)
		{
			char * goodXML = doXMLTransparency(WebMail->OrigBody);
			XMLPtr += sprintf(XMLPtr, XMLLine, "MsgOriginalBody", goodXML, "MsgOriginalBody");
		}
		else
			XMLPtr += sprintf(XMLPtr, XMLLine, "MsgOriginalBody", "", "MsgOriginalBody");
	}
		
	XMLPtr += sprintf(XMLPtr, "%s", XMLTrailer);
	WebMail->XMLLen = XMLPtr - WebMail->XML;
}

char * BuildFormMessage(struct HTTPConnectionInfo * Session, struct MsgInfo * Msg, char * Keys[1000], char * Values[1000], int NumKeys)
{

	// Create B2 message with template body and xml attachment

	char * NewMsg = malloc(100000);
	char * SaveMsg = NewMsg;
	char * XMLPtr;

	char DateString[80];
	struct tm * tm;

	char * FileName[100];
	int FileLen[100];
	char * FileBody[100];
	int n, Files = 0;
	int TotalFileSize = 0;

	WebMailInfo * WebMail = Session->WebMail;

	// Create a B2 Message

	tm = gmtime(&Msg->datecreated);	
	
	sprintf(DateString, "%04d%02d%02d%02d%02d%02d",
		tm->tm_year+1900, tm->tm_mon+1, tm->tm_mday, tm->tm_hour, tm->tm_min, tm->tm_sec);

	if (WebMail->DisplayHTMLName)
	{
		char XMLName[MAX_PATH];
		
		strcpy(XMLName, WebMail->DisplayHTMLName);

		XMLName[strlen(XMLName) - 5] = 0;	// remove .html

		FileName[0] = malloc(MAX_PATH);
		FileBody[0] = malloc(100000);
		Files = 1;
		FileLen[0] = 0;

		sprintf(FileName[0], "RMS_Express_Form_%s.xml", XMLName);

		XMLPtr = FileBody[0];

		XMLPtr += sprintf(XMLPtr, XMLHeader,
			"1,0", VersionString,
			DateString,
			Session->User->Call,
			"", //Grid
			WebMail->DisplayHTMLName,
			WebMail->ReplyHTMLName,
			WebMail->OrigTo,
			"",		// CC
			Session->User->Call,
			WebMail->OrigSubject,
			WebMail->OrigBody,
			"false",		// P2P,
			"false",		//Reply
			"false",		//Forward,
			"false");			// Ack

		// create XML lines for Key/Value Pairs

		for (n = 0; n < NumKeys; n++)
		{
			if (Values[n] == NULL)
				Values[n] = _strdup("");

			XMLPtr += sprintf(XMLPtr, XMLLine, Keys[n], Values[n], Keys[n]);
		}
		XMLPtr += sprintf(XMLPtr, "%s", XMLTrailer);

		FileLen[0] = XMLPtr - FileBody[0];

	}

	sprintf(DateString, "%04d/%02d/%02d %02d:%02d",
		tm->tm_year+1900, tm->tm_mon+1, tm->tm_mday, tm->tm_hour, tm->tm_min);

	// We put orignal To call in B2 Header

	NewMsg += sprintf(NewMsg,
		"MID: %s\r\nDate: %s\r\nType: %s\r\nFrom: %s\r\nTo: %s\r\nSubject: %s\r\nMbo: %s\r\n",
			Msg->bid, DateString, "Private", Msg->from, WebMail->To, Msg->title, BBSName);
				

	NewMsg += sprintf(NewMsg, "Body: %d\r\n", strlen(WebMail->Body));

	for (n = 0; n < Files; n++)
	{
		char * p = FileName[n], * q;

		// Remove any path

		q = strchr(p, '\\');
					
		while (q)
		{
			if (q)
				*q++ = 0;
			p = q;
			q = strchr(p, '\\');
		}

		NewMsg += sprintf(NewMsg, "File: %d %s\r\n", FileLen[n], p);
	}

	NewMsg += sprintf(NewMsg, "\r\n");
	strcpy(NewMsg, WebMail->Body);
	NewMsg += strlen(WebMail->Body);
	NewMsg += sprintf(NewMsg, "\r\n");

	for (n = 0; n < Files; n++)
	{
		memcpy(NewMsg, FileBody[n], FileLen[n]);
		NewMsg += FileLen[n];
		free(FileName[n]);
		free(FileBody[n]);
		NewMsg += sprintf(NewMsg, "\r\n");
	}

	Msg->length = strlen(SaveMsg);
	Msg->B2Flags = B2Msg;
	
	if (Files)
		Msg->B2Flags |= Attachments;

	if (WebMail->Body)
		free(WebMail->Body);

	WebMail->Body = SaveMsg;

	return NULL;
}

VOID UpdateFormAction(char * Template, char * Key)
{
	char * inptr, * saveptr;
	char * varptr, * endptr;
	int varlen, vallen;
	char Submit[64];

	sprintf(Submit, "/Webmail/Submit?%s", Key);

	// First find the Form Action string and replace with our URL. It should have
	// action="http://{FormServer}:{FormPort}" but some forms have localhost:8001 instead

	// Also remove the OnSubmit if it contains the standard popup about having to close browser

	inptr = Template;
	saveptr = varptr = stristr(inptr, "<Form ");

	if (varptr)			// Just in case!
	{
		varptr = stristr(inptr, " Action=");
		if (varptr)
		{
			char delim = ' ';

			varptr += 8;		// to first char.

			if (*varptr == '\"' || *varptr == '\'')
				delim = *varptr++;

			endptr = strchr(varptr, delim);

			// Move the remaining message up/down the buffer to make space for substitution

			varlen = endptr - varptr;
			vallen = strlen(Submit);

			endptr = varptr + varlen;
		
			memmove(varptr + vallen, endptr, strlen(endptr) + 1);		// copy null on end
			memcpy(varptr, Submit, vallen);
		}
	}
		
	varptr = saveptr;

	if (varptr)			// Just in case!
	{
		saveptr = varptr = stristr(inptr, " onsubmit=");
		if (varptr)
		{
			char delim = ' ';

			varptr += 10;		// to first char.

			if (*varptr == '\"' || *varptr == '\'')
				delim = *varptr++;

			endptr = strchr(varptr, delim);

			// We want to remove onsubmit and delimiter 

			varptr = saveptr;
			endptr++;

			// Move the remaining message up/down the buffer to make space for substitution

			varlen = endptr - varptr;
			vallen = 0;

			endptr = varptr + varlen;
		
			memmove(varptr + vallen, endptr, strlen(endptr) + 1);		// copy null on end
			memcpy(varptr, Submit, vallen);
		}
	}
}


int ReplyToFormsMessage(struct HTTPConnectionInfo * Session, struct MsgInfo * Msg, char * Reply)
{
	WebMailInfo * WebMail = Session->WebMail;
	KeyValues * txtKey = WebMail->XMLKeys;
	int Len;
	char * inptr, * ptr;
	char * varptr, * endptr;
	int varlen, vallen;

	char * Template = FindXMLVariable(WebMail, "reply_template");

	if (Template == NULL)
		return 0;					// No Template

	WebMail->isReply = TRUE;

	if (WebMail->OrigBody)
		free(WebMail->OrigBody);

	WebMail->OrigBody = _strdup(WebMail->Body);
	
	// Add "Re: " to Subject

	ptr = WebMail->Subject;

	WebMail->Subject = malloc(strlen(Msg->title) + 10);
	sprintf(WebMail->Subject, "Re: %s", Msg->title);
	
	if (ptr)
		free(ptr);

	WebMail->txtFileName = _strdup(Template);

	// Read the .txt file

	ParsetxtTemplate(Session, NULL, WebMail->txtFileName, TRUE);

	if (WebMail->InputHTMLName == NULL)
		return 0;

	Template = ReadTemplate(WebMail->Dir->FormSet, WebMail->Dir->DirName, WebMail->InputHTMLName);

	if (Template == NULL)
		return 0;

	// I've going to update the template in situ, as I can't see a better way
	// of making sure all occurances of variables in any order are substituted.
	// The space allocated to Template is twice the size of the file
	// to allow for insertions

	UpdateFormAction(Template, Session->Key);		// Update "Submit" Action

	// Search for "{var }" strings in form and replace with
	// corresponding variable from XML

	while (txtKey->Key)
	{
		char Key[256] = "{var ";
		
		strcpy(&Key[5], txtKey->Key);
		strcat(Key, "}");

		inptr = Template;
		varptr = stristr(inptr, Key);

		while (varptr)
		{
			// Move the remaining message up/down the buffer to make space for substitution

			varlen = strlen(Key);
			if (txtKey->Value)
				vallen = strlen(txtKey->Value);
			else vallen = 0;

			endptr = varptr + varlen;
		
			memmove(varptr + vallen, endptr, strlen(endptr) + 1);		// copy null on end
			memcpy(varptr, txtKey->Value, vallen);

			inptr = endptr + 1;
		
			varptr = stristr(inptr, Key);
		}
		txtKey++;
	}

	// Remove </body></html> from end as we add it on later

	ptr = stristr(Template, "</body></html>");
		
	if (ptr)
		*ptr = 0;
		
	Len = sprintf(Reply, "%s", Template);
	free(Template);	
	return Len;
}


char * CheckFile(struct HtmlFormDir * Dir, char * FN)
{
	struct stat STAT;
	FILE * hFile;
	char MsgFile[MAX_PATH];
	char * MsgBytes;
	int ReadLen;
	int FileSize;

	sprintf(MsgFile, "%s/%s/%s/%s", GetBPQDirectory(), Dir->FormSet, Dir->DirName, FN);

	if (stat(MsgFile, &STAT) != -1)
	{
		hFile = fopen(MsgFile, "rb");
	
		if (hFile == 0)
		{
			MsgBytes = _strdup("File is missing");
			return MsgBytes;
		}

		FileSize = STAT.st_size;
		MsgBytes = malloc(FileSize * 2);		// Allow plenty of room for template substitution
		ReadLen = fread(MsgBytes, 1, FileSize, hFile); 
		MsgBytes[FileSize] = 0;
		fclose(hFile);

		return MsgBytes;
	}
	return NULL;
}


BOOL ParsetxtTemplate(struct HTTPConnectionInfo * Session, struct HtmlFormDir * Dir, char * FN, BOOL isReply)
{
	WebMailInfo * WebMail = Session->WebMail;
	KeyValues * txtKey = WebMail->txtKeys;

	int i;
	char * MsgBytes;

	char * txtFile;
	char * ptr, *ptr1;
	char * InputName = NULL;	// HTML to input message
	char * ReplyName = NULL;
	char * To = NULL;
	char * Subject = NULL;
	char * MsgBody = NULL;

	char Date[16];
	char UDate[16];
	char DateTime[32];
	char UDateTime[32];
	char Day[16];
	char UDay[16];
	char UDTG[32];
	char Seq[16];
	char FormDir[MAX_PATH];
	double Lat;
	double Lon;
	char LatString[32], LonString[32], GPSString[32];
	BOOL GPSOK;
	
	struct tm * tm;
	time_t NOW;

	// if Dir not specified search all for Filename

	if (Dir == NULL)
	{
		for (i = 0; i < FormDirCount; i++)
		{
			int n;
			
			Dir = HtmlFormDirs[i];

			MsgBytes = CheckFile(Dir, FN);
			if (MsgBytes)
				goto gotFile;
			
			// Recurse any Subdirs
			
			n = 0;
			while (n < Dir->DirCount)
			{
				MsgBytes = CheckFile(Dir->Dirs[n], FN);
				if (MsgBytes)
				{
					Dir = Dir->Dirs[n];
					goto gotFile;
				}
				n++;
			}
		}
		return FALSE;
	}
	else
		MsgBytes = CheckFile(Dir, FN);

gotFile:

	WebMail->Dir = Dir;

	if (WebMail->txtFile)
		free(WebMail->txtFile);

	WebMail->txtFile = MsgBytes;
	
	NOW = time(NULL);
	tm = localtime(&NOW);

	sprintf(Date, "%02d-%02d-%02d",
		tm->tm_year - 100,tm->tm_mon + 1, tm->tm_mday);
	
	sprintf(DateTime, "%02d-%02d-%02d %02d:%02d",
		tm->tm_year - 100,tm->tm_mon + 1, tm->tm_mday, tm->tm_hour, tm->tm_min);
	
	strcpy(Day, longday[tm->tm_wday]);

	tm = gmtime(&NOW);				

	sprintf(UDate, "%02d-%02d-%02dZ",
		tm->tm_year - 100,tm->tm_mon + 1, tm->tm_mday);

	sprintf(UDateTime, "%02d-%02d-%02d %02d:%02dZ",
		tm->tm_year - 100,tm->tm_mon + 1, tm->tm_mday, tm->tm_hour, tm->tm_min);

	sprintf(UDTG, "%02d%02d%02dZ %s %04d",
		tm->tm_mday, tm->tm_hour, tm->tm_min, month[tm->tm_mon], tm->tm_year + 1900);

	strcpy(UDay, longday[tm->tm_wday]);

	sprintf(Seq, "%d", ++Session->User->WebSeqNo);
	sprintf(FormDir, "%s/%s/", Dir->FormSet, Dir->DirName);
	
	txtKey->Key = _strdup("<DateTime>");
	txtKey++->Value = _strdup(DateTime);
	txtKey->Key = _strdup("<UDateTime>");
	txtKey++->Value = _strdup(UDateTime);
	txtKey->Key = _strdup("<Date>");
	txtKey++->Value = _strdup(Date);
	txtKey->Key = _strdup("<UDate>");
	txtKey++->Value = _strdup(UDate);
	txtKey->Key = _strdup("<Time>");
	txtKey++->Value = _strdup(&DateTime[9]);
	txtKey->Key = _strdup("<UTime>");
	txtKey++->Value = _strdup(&UDateTime[9]);
	txtKey->Key = _strdup("<Day>");
	txtKey++->Value = _strdup(Day);
	txtKey->Key = _strdup("<UDay>");
	txtKey++->Value = _strdup(UDay);
	txtKey->Key = _strdup("<UDTG>");
	txtKey++->Value = _strdup(UDTG);

	// Try to get position from APRS

	GPSOK = GetAPRSLatLon(&Lat, &Lon);
	GPSOK = GetAPRSLatLonString(&LatString[1], &LonString[1]);
	memmove(LatString, &LatString[1], 2);
	memmove(LonString, &LonString[1], 3);
	LatString[2] = '-';
	LonString[3] = '-';
	sprintf(GPSString,"%s %s", LatString, LonString);

	txtKey->Key = _strdup("<GPS>");
	if (GPSOK)
		txtKey++->Value = _strdup(GPSString);
	else
		txtKey++->Value = _strdup("");
	
	txtKey->Key = _strdup("<Position>");
	txtKey++->Value = _strdup(GPSString);
	txtKey->Key = _strdup("<SeqNum>");
	txtKey++->Value = _strdup(Seq);
	txtKey->Key = _strdup("<ProgramVersion>");
	txtKey++->Value = _strdup(VersionString);
	txtKey->Key = _strdup("<Callsign>");
	txtKey++->Value = _strdup(Session->User->Call);
	txtKey->Key = _strdup("<MsgTo>");
	txtKey++->Value = _strdup("");
	txtKey->Key = _strdup("<MsgCc>");
	txtKey++->Value = _strdup("");
	txtKey->Key = _strdup("<MsgSender>");
	txtKey++->Value = _strdup(Session->User->Call);
	txtKey->Key = _strdup("<MsgSubject>");
	txtKey++->Value = _strdup("");
	txtKey->Key = _strdup("<MsgBody>");
//	if (WebMail->OrigBody)
//		txtKey++->Value = _strdup(WebMail->OrigBody);
//	else
		txtKey++->Value = _strdup("");

	txtKey->Key = _strdup("<MsgP2P>");
	txtKey++->Value = _strdup("");

	if (isReply)
	{
		txtKey->Key = _strdup("<MsgIsReply>");
		txtKey++->Value = _strdup("True");
		txtKey->Key = _strdup("<MsgIsForward>");
		txtKey++->Value = _strdup("False");
		txtKey->Key = _strdup("<MsgIsAcknowledgement>");
		txtKey++->Value = _strdup("False");
		txtKey->Key = _strdup("<MsgOriginalSubject>");
		txtKey++->Value = _strdup("");
		txtKey->Key = _strdup("<MsgOriginalSender>");
		txtKey++->Value = _strdup("");
		txtKey->Key = _strdup("<MsgOriginalBody>");
		txtKey++->Value = _strdup(WebMail->Body);
		txtKey->Key = _strdup("<MsgOriginalID>");
		txtKey++->Value = _strdup(WebMail->Msg->bid);

		// Get Timestamp from Message

		tm = gmtime(&WebMail->Msg->datecreated);				

		sprintf(Date, "%02d-%02d-%02d",
			tm->tm_year - 100,tm->tm_mon + 1, tm->tm_mday);
	
		sprintf(DateTime, "%02d-%02d-%02d %02d:%02d",
			tm->tm_year - 100,tm->tm_mon + 1, tm->tm_mday, tm->tm_hour, tm->tm_min);
	
		strcpy(Day, longday[tm->tm_wday]);
			tm = gmtime(&WebMail->Msg->datecreated);				

		sprintf(UDate, "%02d-%02d-%02dZ",
			tm->tm_year - 100,tm->tm_mon + 1, tm->tm_mday);

		sprintf(UDateTime, "%02d-%02d-%02d %02d:%02dZ",
			tm->tm_year - 100,tm->tm_mon + 1, tm->tm_mday, tm->tm_hour, tm->tm_min);

		sprintf(UDTG, "%02d%02d%02dZ %s %04d",
			tm->tm_mday, tm->tm_hour, tm->tm_min, month[tm->tm_mon], tm->tm_year + 1900);



		txtKey->Key = _strdup("<MsgOriginalDate>");
		txtKey++->Value = _strdup(UDate);
		txtKey->Key = _strdup("<MsgOriginalUtcDate>");
		txtKey++->Value = _strdup(UDate);
		txtKey->Key = _strdup("<MsgOriginalUtcTime>");
		txtKey++->Value = _strdup(&UDateTime[9]);
		txtKey->Key = _strdup("<MsgOriginalLocalDate>");
		txtKey++->Value = _strdup(Date);
		txtKey->Key = _strdup("<MsgOriginalLocalTime>");
		txtKey++->Value = _strdup(&UDateTime[9]);
		txtKey->Key = _strdup("<MsgOriginalDTG>");
		txtKey++->Value = _strdup(UDTG);
		txtKey->Key = _strdup("<MsgOriginalSize>");
		txtKey++->Value = _strdup("");
		txtKey->Key = _strdup("<MsgOriginalAttachmentCount>");
		txtKey++->Value = _strdup("");
		txtKey->Key = _strdup("<MsgOriginalXML>");
		txtKey++->Value = _strdup("");
	}
	else
	{
		txtKey->Key = _strdup("<MsgIsReply>");
		txtKey++->Value = _strdup("False");
		txtKey->Key = _strdup("<MsgIsForward>");
		txtKey++->Value = _strdup("False");
		txtKey->Key = _strdup("<MsgIsAcknowledgement>");
		txtKey++->Value = _strdup("False");
		txtKey->Key = _strdup("<MsgOriginalSubject>");
		txtKey++->Value = _strdup("");
		txtKey->Key = _strdup("<MsgOriginalSender>");
		txtKey++->Value = _strdup("");
		txtKey->Key = _strdup("<MsgOriginalBody>");
		txtKey++->Value = _strdup("");
		txtKey->Key = _strdup("<MsgOriginalID>");
		txtKey++->Value = _strdup("");
		txtKey->Key = _strdup("<MsgOriginalDate>");
		txtKey++->Value = _strdup("");
		txtKey->Key = _strdup("<MsgOriginalUtcDate>");
		txtKey++->Value = _strdup("");
		txtKey->Key = _strdup("<MsgOriginalUtcTime>");
		txtKey++->Value = _strdup("");
		txtKey->Key = _strdup("<MsgOriginalLocalDate>");
		txtKey++->Value = _strdup("");
		txtKey->Key = _strdup("<MsgOriginalLocalTime>");
		txtKey++->Value = _strdup("");
		txtKey->Key = _strdup("<MsgOriginalDTG>");
		txtKey++->Value = _strdup("");
		txtKey->Key = _strdup("<MsgOriginalSize>");
		txtKey++->Value = _strdup("");
		txtKey->Key = _strdup("<MsgOriginalAttachmentCount>");
		txtKey++->Value = _strdup("");
		txtKey->Key = _strdup("<MsgOriginalXML>");
		txtKey++->Value = _strdup("");
	}

	txtKey->Key = _strdup("<FormFolder>");
	txtKey++->Value = _strdup(FormDir);		//Form Folder

	// Do standard Variable substitution on file

	DoStandardTemplateSubsitutions(Session, WebMail->txtFile);

	txtFile = _strdup(WebMail->txtFile);		// We chop up and modify bits of txtFile, so need copy

	// Scan template line by line extracting useful information

	ptr = txtFile;
	ptr1 = strchr(ptr, '\r');
		
	while (ptr1)
	{
		if (_memicmp(ptr, "Msg:", 4) == 0)
		{
			// Rest is message body. May need <var> substitutions

			if (WebMail->Body)
				free(WebMail->Body);

			WebMail->Body = _strdup(ptr + 4);
			break;
		}

		// Can now terminate lines

		*ptr1++ = 0;

		while (*ptr1 == '\r' || *ptr1 == '\n')
			*ptr1++ = 0;

		if (_memicmp(ptr, "Form:", 5) == 0)
		{
			InputName = &ptr[5];
	
			while (*InputName == ' ')		// Remove leading spaces
				InputName++;

			WebMail->InputHTMLName = _strdup(InputName);
			WebMail->DisplayHTMLName = strlop(WebMail->InputHTMLName, ',');

			if (WebMail->DisplayHTMLName)
			{
				while (*WebMail->DisplayHTMLName == ' ')		// Remove leading spaces
				WebMail->DisplayHTMLName++;
				WebMail->DisplayHTMLName = _strdup(WebMail->DisplayHTMLName);
			}
		}
		else if (_memicmp(ptr, "ReplyTemplate:",14) == 0)
		{
			ReplyName = &ptr[14];
	
			while (*ReplyName == ' ')
				ReplyName++;
			
			strlop(ReplyName, '\r');		// Terminate
			strlop(ReplyName, ' ');			// Strip trailing spaces

			WebMail->ReplyHTMLName = _strdup(ReplyName);
		}
		else if (_memicmp(ptr, "To:", 3) == 0)
		{
			if (strlen(ptr) > 5)
				WebMail->To = _strdup(&ptr[3]);
		}
		else if (_memicmp(ptr, "Subj:", 5) == 0)
		{
			if (strlen(ptr) > 6)
				WebMail->Subject = _strdup(&ptr[5]);
		}
		else if (_memicmp(ptr, "Def:", 4) == 0)
		{
			// Def: MsgOriginalBody=<var MsgOriginalBody> 

			char * val = strlop(ptr, '=');

			if (val)
			{
				// Make Room for {} delimiters

				memmove(ptr, ptr + 1, strlen(ptr)); 
				ptr[3] = '<';
				ptr[strlen(ptr)] = '>';
				
				while (val[strlen(val) - 1] == ' ')
					val[strlen(val) - 1]  = 0;

				txtKey->Key = _strdup(&ptr[3]);
				txtKey++->Value = _strdup(val);		//Form Folder
			}
		}
	
		ptr = ptr1;
		ptr1 = strchr(ptr, '\r');
	}

	if (WebMail->ReplyHTMLName == NULL)
		WebMail->ReplyHTMLName = _strdup("");

	free(txtFile);

	return TRUE;
}

VOID FormatTime2(char * Time, time_t cTime)
{
	struct tm * TM;
	TM = gmtime(&cTime);

	sprintf(Time, "%s, %02d %s %3d %02d:%02d:%02d GMT", dat[TM->tm_wday], TM->tm_mday, month[TM->tm_mon],
		TM->tm_year + 1900, TM->tm_hour, TM->tm_min, TM->tm_sec);

}

VOID DownloadAttachments(struct HTTPConnectionInfo * Session, char * Reply, int * RLen, char * Param)
{
	WebMailInfo * WebMail = Session->WebMail;
	char TimeString[64];
	char FileTimeString[64];
	int file = atoi(Param);

	if (file == -1)
	{
		// User has gone back, then selected "No file Selected"

		// For now just stop crash

		file = 0;
	}

	FormatTime2(FileTimeString, time(NULL));
	FormatTime2(TimeString, time(NULL));

	*RLen =	sprintf(Reply, "HTTP/1.1 200 OK\r\nContent-Length: %d\r\n"
		"Content-Type: application/octet-stream\r\n"
		"Content-Disposition: attachment; filename=\"%s\"\r\n"
		"Date: %s\r\n"
		"Last-Modified: %s\r\n" 
		"\r\n", WebMail->FileLen[file], WebMail->FileName[file], TimeString, FileTimeString);

	memcpy(&Reply[*RLen], WebMail->FileBody[file], WebMail->FileLen[file]); 

	*RLen += WebMail->FileLen[file];

	return;
}

VOID getAttachmentList(struct HTTPConnectionInfo * Session, char * Reply, int * RLen, char * Rest)
{
	char popuphddr[] = 
			
		"<html><body align=center background='/background.jpg'>"
		"<script>"
		"function myFunction() {var x = document.getElementById('Sel').value;"
		"var Key = \"%s\";"
		"var param = \"toolbar=yes,location=no,directories=no,status=no,menubar=no,scrollbars=yes,resizable=yes,titlebar=yes,toobar=yes\";"
		"window.open(\"/WebMail/GetDownLoad\" + '?' + Key + '&' + x,'_self', param);"
		"}"
		"</script>"
		"<div align=center>Note files over 100K long can't be downloaded<br><br>"
		"<table border=1 cellpadding=2 bgcolor=white>"
		"<tr><td><select id='Sel' onchange=myFunction()>"
		"<option value=-1>Select Attachment";

	char popup[10000];
	int i;
	WebMailInfo * WebMail = Session->WebMail;

	sprintf(popup, popuphddr, Session->Key);

	for (i = 0; i < WebMail->Files; i++)
	{
		if(WebMail->FileLen[i] < 100000)
			sprintf(popup, "%s <option value=%d>%s (Len %d)", popup, i, WebMail->FileName[i], WebMail->FileLen[i]);
	}

	sprintf(popup, "%s</select></td></tr></table><br><input onclick=window.history.back() value=Back type=button></div>", popup);

	*RLen = sprintf(Reply, "%s", popup);
	return;
}

// Polling.cpp : Defines the entry point for the console application.
//

// Sensor/Aktor Systeme LU
// Demoprogramm: Polling
// Liest via Profibus-DP ein Byte von der CP242-8 aus und schreibt dieses
// via Profibus-DP zurück

#include "stdafx.h"
#include "dp_5613.h"
#include "5613_ret.h"
#include <iostream>
#include <windows.h>
#include <stdio.h>

#define MAX_SEM_COUNT 1

bool master_wants = FALSE;
DPR_STRING *pCP_Name="CP_L2_1:";		// Logischer Name des CP
DPR_STRING *pDatabase= NULL;			// Pfad und Name der Datenbasis (NULL Default)
DP_ERROR_T  ErrStruct;				// Details zur Fehlerursache
DPR_DWORD   RetErrClass;
DPR_STRING *pErrLanguage="German";		// Sprache des auszugebenden Fehlertextes
DPR_STRING  ErrorText[DP_ERR_TXT_SIZE];		// Datenbereich für Fehlertext
DPR_DWORD  DPUserHandle;			// User Handle für Anwenderprogramm
DPR_CP5613_DP_T volatile *pDPRam;		// Zugriff auf Dual Ported RAM
DPR_BYTE old_mode;
DPR_BYTE DP_Data[2];				// Nutzdaten

//Semaphores
HANDLE dp_write_semaphore;
HANDLE gotoxy_semaphore;


void gotoxy(int xpos, int ypos)
{
  COORD scrn;
  HANDLE hOuput = GetStdHandle(STD_OUTPUT_HANDLE);
  scrn.X = xpos; scrn.Y = ypos;
  SetConsoleCursorPosition(hOuput,scrn);
}


int dp_init(void)
{
	 printf("DP_start_cp\n");
	// DP_start_cp: Laden der Firmware und der Datenbasis in den CP 5613
	//    CP_NAME: logischer Name des CP (CP_L2_1:)
	//    Database: Pfad und Name der Datenbasis (NULL Default)
	//    ErrStruct: Details zur Fehlerursache
	RetErrClass = DP_start_cp(pCP_Name, pDatabase, &ErrStruct);
	if ((RetErrClass != DP_OK) && (ErrStruct.error_code != CI_RET_START_ALREADY_DONE))
	{
		(void)DP_get_err_txt(&ErrStruct, pErrLanguage, ErrorText);
		printf("%s\n",ErrorText);
		return -1;
	}
	
	printf("DP_open_cp\n");
	// DP_open:	Anmelden eines DP-Anwenderprogramms
	//    CP_NAME: logischer Name des CP (CP_L2_1:)
	//    DPUser_Handle: Vergabe eines User-Handle
	//    ErrStruct: Details zur Fehlerursache
	RetErrClass = DP_open(pCP_Name, &DPUserHandle, &ErrStruct);
	if (RetErrClass != DP_OK)
	{
		(void)DP_get_err_txt(&ErrStruct, pErrLanguage, ErrorText);
		printf("%s\n",ErrorText);
		return -1;
	}
	
	printf("DP_get_pointer\n");
	// DP_get_pointer: Exklusiver Zugriff auf das Prozessabbild
	//    DPUser_Handle: User-Handle, das beim Aufruf DP_open vergeben wurde
	//    DP_TIMEOUT_FOREVER: Dauer der maximalen Wartezeit ("unendlich")
	//    pDPRam: Zugriffsadresse auf das Prozessabbild des CP 5613
	//    ErrStruct: Details zur Fehlerursache
	RetErrClass = DP_get_pointer(DPUserHandle, 5000, &pDPRam, &ErrStruct);
	if (RetErrClass != DP_OK)
	{
		(void)DP_get_err_txt(&ErrStruct, pErrLanguage, ErrorText);
		printf("%s\n",ErrorText);
		return -1;
	}

	for (int i=1; i < 4; i++) {
		// DP_set_mode: Einstellen des gewünschten DP-Zustand (OFFLINE, STOP, CLEAR, OPERATE)
		//    DPUser_Handle: User-Handle, das beim Aufruf DP_open vergeben wurde
		//    Mode: OFFLINE-, STOP-, CLEAR, OPERATE-Zustand
		//    ErrStruct: Details zur Fehlerursache
		printf("DP_set_mode");
		switch (i) {
		case 0: 
			RetErrClass = DP_set_mode(DPUserHandle, DP_OFFLINE, &ErrStruct);
			break;
		case 1: 
			old_mode=DP_OFFLINE;
			RetErrClass = DP_set_mode(DPUserHandle, DP_STOP, &ErrStruct);
			break;
		case 2:	
			old_mode=DP_STOP;
			RetErrClass = DP_set_mode(DPUserHandle, DP_CLEAR, &ErrStruct);
			break;
		case 3: 
			old_mode=DP_CLEAR;
			RetErrClass = DP_set_mode(DPUserHandle, DP_OPERATE, &ErrStruct);
			break;
		}
		if(RetErrClass != DP_OK)
		{
			(void)DP_get_err_txt(&ErrStruct, pErrLanguage, ErrorText);
			printf("%s\n",ErrorText);
			return -1;
		}
		while (old_mode == pDPRam->info_watch.master_info.USIF_state) ;
		switch (pDPRam->info_watch.master_info.USIF_state) {
		case DP_OFFLINE:	
			printf(" OFFLINE\n");
			break;
		case DP_STOP:
			printf(" STOP\n");
			break;
		case DP_CLEAR:
			printf(" CLEAR\n"); 
			break;
		/*case DP_AUTOCLEAR:
			printf(" AUTOCLEAR\n");
			break;*/
		case DP_OPERATE: 
			printf(" OPERATE\n");
			break;
		}
	}
	return 0;
}

void dp_read(void)
{
	/* Sperren des Datenbereichs gegen Aktualisierung */		
	pDPRam->ctr.D_lock_in_slave_adr = 1;
	/* Daten kopieren */
	memcpy(&DP_Data[0], (void*)(&(pDPRam->pi.slave_in[1].data[0])), 1);
	/* Sperre wieder aufheben */
	pDPRam->ctr.D_lock_in_slave_adr = DPR_DP_UNLOCK;
}
void dp_write(void)
{
	DWORD wait_for_sem;
	bool sem_dp_write_continue;

	sem_dp_write_continue = TRUE;
	while(sem_dp_write_continue)
	{
		wait_for_sem =	WaitForSingleObject( 
						dp_write_semaphore,   // handle to semaphore
						0L);				// zero-second time-out interval
		switch (wait_for_sem) 
		{ 
			// The semaphore object was signaled.
			case WAIT_OBJECT_0: 				
				/* Daten kopieren */
				memcpy((void*)&(pDPRam->pi.slave_out[1].data[0]), &DP_Data[0], 2);
				/* Übertragung anstoßen */		
				pDPRam->ctr.D_out_slave_adr = 1;
				
				sem_dp_write_continue = FALSE;
				if (!ReleaseSemaphore( 
                     dp_write_semaphore,	// handle to semaphore
                       1,					// increase count by one
                       NULL) )				// not interested in previous count
				{
					printf("ReleaseSemaphore error: %d\n", GetLastError());
				}
				break;
			case WAIT_TIMEOUT:
				break;
		}
	}
}



void dp_exit(void)
{
	//Auslesen DP Zustand
	pDPRam->info_watch.master_info.USIF_state;
	// DP_set_mode: Einstellen des gewünschten DP-Zustand (OFFLINE, STOP, CLEAR, OPERATE)
	DP_set_mode(DPUserHandle, DP_STOP, &ErrStruct);
	// DP_release_pointer: Freigabe der Zugriffsadresse auf das Prozessabbild des CP 5613
	DP_release_pointer(DPUserHandle, &ErrStruct);
	// DP_close: Abmelden eines DP-Anwenderprogramms
	DP_close(DPUserHandle, &ErrStruct);
	// DP_reset_cp: Rücksetzen des CP 5613
	DP_reset_cp(pCP_Name, &ErrStruct);
}




DWORD WINAPI life_byte_thread_proc(void)
{
	DWORD dwWaitResult;
	bool semContinue;

	while(1)
	{	
		if(master_wants == FALSE)
		{
			semContinue = TRUE;
			while(semContinue)
			{
				dwWaitResult =	WaitForSingleObject( 
								gotoxy_semaphore,   // handle to semaphore
								0L);				// zero-second time-out interval
				switch (dwWaitResult) 
				{ 
					// The semaphore object was signaled.
					case WAIT_OBJECT_0: 	
						DP_Data[1] = DP_Data[1]++;
						gotoxy(0,7);
						printf("Life Byte: %X", DP_Data[1]);
						gotoxy(0,9);

						semContinue = FALSE;	
						if (!ReleaseSemaphore( 
							 gotoxy_semaphore,	// handle to semaphore
							 1,					// increase count by one
							 NULL) )				// not interested in previous count
						{
							printf("ReleaseSemaphore error: %d\n", GetLastError());
						}
						break;
					case WAIT_TIMEOUT:
						break;
				}
			}
			(void)dp_write();
		}
		else
		{
			while(master_wants == TRUE);
		}
	}
	return 1;
}


DWORD WINAPI target_control_thread_proc(void)
{
	DWORD dwWaitResult;
	bool semContinue;
    char input = ' ';
	bool ende = 0;
	int count = 0;

	while(ende == 0) {

		(void)dp_read();

		semContinue = TRUE;
		while(semContinue)
		{
			dwWaitResult =	WaitForSingleObject( 
							gotoxy_semaphore,   // handle to semaphore
							0L);				// zero-second time-out interval
			switch (dwWaitResult) 
			{ 
				// The semaphore object was signaled.
				case WAIT_OBJECT_0: 	
					gotoxy(0,8);
					printf("Switch Stellung: %X", DP_Data[0]);
					gotoxy(0,9);
					
					semContinue = FALSE;
					if (!ReleaseSemaphore( 
                        gotoxy_semaphore,	// handle to semaphore
                        1,					// increase count by one
                        NULL) )				// not interested in previous count
					{
						printf("ReleaseSemaphore error: %d\n", GetLastError());
					}
					break;

				case WAIT_TIMEOUT:
					break;
			}
		}

		std::cin >> input;
		
		switch(input)
		{
			case 'q':
				(void)dp_exit();
				ende = 1;
				break;
			case '1':
				while(count < 1000) {
					if( (((DP_Data[0]>>3) ^  (DP_Data[0]>>2) ^ (DP_Data[0]>>1) ^ DP_Data[0]) & 0x01) == 0x01 ||
						(((DP_Data[0]>>3) ^  (DP_Data[0]>>2) ^ (DP_Data[0]>>1) ^ DP_Data[0]) & 0x10) == 0x10
					) {						
						DP_Data[0] = 0x01;
					}
					else {
						DP_Data[0] = 0x00;
					}
					master_wants = TRUE;
					(void)dp_write();
					master_wants = FALSE;
					count = count++;
					Sleep(10);
					if(count < 1000) {
						(void)dp_read();
					}
				}
				count = 0;
				break;
			case 'd':
				DP_Data[0] = 0x01;
				master_wants = TRUE;
				(void)dp_write();
				master_wants = FALSE;
				break;
			case 'u':
				DP_Data[0] = 0x00;
				master_wants = TRUE;
				(void)dp_write();
				master_wants = FALSE;
				break;
			default:
				break;
		}
	}
	return 1;
}


int _tmain(int argc, _TCHAR* argv[])
{
	HANDLE target_control_thread;
	HANDLE life_byte_thread;
    DWORD ThreadID;

	if(dp_init() == 0)
	{
		DP_Data[1] = 0x00;

		// Create a semaphore with initial and max counts of MAX_SEM_COUNT
		gotoxy_semaphore =	CreateSemaphore( 
							NULL,           // default security attributes
							MAX_SEM_COUNT,  // initial count
							MAX_SEM_COUNT,  // maximum count
							NULL);          // unnamed semaphore

		if (gotoxy_semaphore == NULL) 
		{
			printf("CreateSemaphore error: %d\n", GetLastError());
			return -1;
		}

		// Create a semaphore with initial and max counts of MAX_SEM_COUNT
		dp_write_semaphore =	CreateSemaphore( 
								NULL,           // default security attributes
								MAX_SEM_COUNT,  // initial count
								MAX_SEM_COUNT,  // maximum count
								NULL);          // unnamed semaphore

		if (dp_write_semaphore == NULL) 
		{
			printf("CreateSemaphore error: %d\n", GetLastError());
			return -1;
		}

		// start life-byte counter thread
		life_byte_thread =	CreateThread( 
							NULL,       // default security attributes
							0,          // default stack size
							(LPTHREAD_START_ROUTINE) life_byte_thread_proc, 
							NULL,       // no thread function arguments
							0,          // default creation flags
							&ThreadID); // receive thread identifier

		 if(life_byte_thread == NULL )
		 {
			 printf("CreateThread error: %d\n", GetLastError());
			 return  -1;
		 }

		target_control_thread = CreateThread(
								NULL,       // default security attributes
								0,          // default stack size
								(LPTHREAD_START_ROUTINE) target_control_thread_proc, 
								NULL,       // no thread function arguments
								0,          // default creation flags
								&ThreadID); // receive thread identifier

		if(target_control_thread == NULL )
		{
			printf("CreateThread error: %d\n", GetLastError());
			return -1;
		}

		WaitForSingleObjectEx(target_control_thread, INFINITE, TRUE);

		//close target-control thread
		CloseHandle(target_control_thread);

		//close life-byte thread
		CloseHandle(life_byte_thread);
	
		return 0;
	}
	else
	{
		return -1;
	}
}



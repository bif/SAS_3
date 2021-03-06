// Polling.cpp : Defines the entry point for the console application.
//

// Sensor/Aktor Systeme LU
// Demoprogramm: Polling
// Liest via Profibus-DP ein Byte von der CP242-8 aus und schreibt dieses
// via Profibus-DP zur�ck

#include "stdafx.h"
#include "dp_5613.h"
#include "5613_ret.h"
#include <iostream>
#include <windows.h>
#include <stdio.h>
#include <conio.h>



HANDLE gotoxy_semaphore;

void gotoxy(int xpos, int ypos)
{
  COORD scrn;
  HANDLE hOuput = GetStdHandle(STD_OUTPUT_HANDLE);
  scrn.X = xpos; scrn.Y = ypos;
  SetConsoleCursorPosition(hOuput,scrn);
}

int _tmain(int argc, _TCHAR* argv[])
{
	DPR_STRING *pCP_Name="CP_L2_1:";		// Logischer Name des CP
	DPR_STRING *pDatabase= NULL;			// Pfad und Name der Datenbasis (NULL Default)
	DP_ERROR_T  ErrStruct;				// Details zur Fehlerursache
	DPR_DWORD   RetErrClass;
	DPR_STRING *pErrLanguage="German";		// Sprache des auszugebenden Fehlertextes
	DPR_STRING  ErrorText[DP_ERR_TXT_SIZE];		// Datenbereich f�r Fehlertext
	DPR_DWORD  DPUserHandle;			// User Handle f�r Anwenderprogramm
	DPR_CP5613_DP_T volatile *pDPRam;		// Zugriff auf Dual Ported RAM
	DPR_BYTE DP_Data[2];				// Nutzdaten
	DPR_BYTE old_mode;
    char input = ' ';
	bool ende = 0;
	int count = 0;

	//dp init
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
		// DP_set_mode: Einstellen des gew�nschten DP-Zustand (OFFLINE, STOP, CLEAR, OPERATE)
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

	DP_Data[1] = 0x00;
//	for (;DP_Data[0] != 0x08;) {
	while(ende == 0) {
		/* Sperren des Datenbereichs gegen Aktualisierung */
		pDPRam->ctr.D_lock_in_slave_adr = 1;
		/* Daten kopieren */
		memcpy(&DP_Data[0], (void*)(&(pDPRam->pi.slave_in[1].data[0])), 1);
		/* Sperre wieder aufheben */
		pDPRam->ctr.D_lock_in_slave_adr = DPR_DP_UNLOCK;
		//_byteswap_ushort()  big endian to little endian or little endian to big endian for integer
		//_byteswap_ushort()  big endian to little endian or little endian to big endian for double

		if(kbhit())
		{
			input = getch();
		}

		gotoxy(0,7);
		printf("Switch Stellung: %X\n", DP_Data[0]);
		printf("Life Byte: %X\n", DP_Data[1]);
		gotoxy(0,7);
		DP_Data[1] = DP_Data[1]++;

		/* Daten kopieren */
		memcpy((void*)&(pDPRam->pi.slave_out[1].data[1]), &DP_Data[1], 1);
		/* �bertragung ansto�en */		
		pDPRam->ctr.D_out_slave_adr = 1;

		/* Sperren des Datenbereichs gegen Aktualisierung */
		pDPRam->ctr.D_lock_in_slave_adr = 1;
		/* Daten kopieren */
		memcpy(&DP_Data[0], (void*)(&(pDPRam->pi.slave_in[1].data[0])), 1);
		/* Sperre wieder aufheben */
		pDPRam->ctr.D_lock_in_slave_adr = DPR_DP_UNLOCK;

		switch(input) {
			case 'q':
				//Auslesen DP Zustand
				pDPRam->info_watch.master_info.USIF_state;
				// DP_set_mode: Einstellen des gew�nschten DP-Zustand (OFFLINE, STOP, CLEAR, OPERATE)
				DP_set_mode(DPUserHandle, DP_STOP, &ErrStruct);
				// DP_release_pointer: Freigabe der Zugriffsadresse auf das Prozessabbild des CP 5613
				DP_release_pointer(DPUserHandle, &ErrStruct);
				// DP_close: Abmelden eines DP-Anwenderprogramms
				DP_close(DPUserHandle, &ErrStruct);
				// DP_reset_cp: R�cksetzen des CP 5613
				DP_reset_cp(pCP_Name, &ErrStruct);
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
					/* Daten kopieren */
					memcpy((void*)&(pDPRam->pi.slave_out[1].data[0]), &DP_Data[0], 2);
					/* �bertragung ansto�en */		
					pDPRam->ctr.D_out_slave_adr = 1;
					count = count++;
					Sleep(10);
					if(count < 1000) {
						/* Sperren des Datenbereichs gegen Aktualisierung */		
						pDPRam->ctr.D_lock_in_slave_adr = 1;
						/* Daten kopieren */
						memcpy(&DP_Data[0], (void*)(&(pDPRam->pi.slave_in[1].data[0])), 1);
						/* Sperre wieder aufheben */
						pDPRam->ctr.D_lock_in_slave_adr = DPR_DP_UNLOCK;

						gotoxy(0,7);
						printf("Switch Stellung: %X\n", DP_Data[0]);
						printf("Life Byte: %X\n", DP_Data[1]);
						gotoxy(0,7);
						DP_Data[1] = DP_Data[1]++;
					}
				}
				count = 0;
				break;
			case 'd':
				DP_Data[0] = 0x01;
				/* Daten kopieren */
				memcpy((void*)&(pDPRam->pi.slave_out[1].data[0]), &DP_Data[0], 2);
				/* �bertragung ansto�en */
				pDPRam->ctr.D_out_slave_adr = 1;
				break;
			case 'u':
				DP_Data[0] = 0x00;
				/* Daten kopieren */
				memcpy((void*)&(pDPRam->pi.slave_out[1].data[0]), &DP_Data[0], 2);
				/* �bertragung ansto�en */
				pDPRam->ctr.D_out_slave_adr = 1;
				break;
			default:
				break;
		}
	
	}

	
	return 0;
}
